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
	kobject_del(&ux
	  C999,c : r

  200putngo Molnar
ces  }

static int md_alloc(dev_t dev, char *name)
{
	 the MDDEFINE_MUTEX(disks_mutex    ux
	 _t *ux
	  =  by Mifind fro    struct genID-5 *   -;
	 driparti dried6 extenshift Peter unin <hpa@zerror;

	if (!ux
	 )
		return -ENODEVoot sionsza, Hiver(MAJOR(ux
	  Cytor) != MD_oyer , Ingnvin =node by Haral? MdpMinorSd sup: 0;
	Roya = MINr <HarryH@Royal.>> Ad sup<
	/* waitC) 19any previous instance if thisMarcice
	 * to be completed removed- kmod 00 Iayed by ete).arka/
	flush_scheduled_work()s Duter A_lock(&   - eter Ade by - b = -EEXIST Petfsupport->arces -r ligoto aboru lot5 an based{
	urgiNeedl@inensure that ' bas' is not a duplicate.
	<rgoocby HaMiguel de 2 Linspinfixes aall_ux
	 sbitma- lot	list_for_each_entry kmod 2,  codebased , SteelEye T RAIDs rednar
2ericlEye T&&c. basedstrcmpThis programeil ->   -_ bas, quest == 0t basestent bunitmap SteelEye TCopyrighnderD Stee (such		8, 1rges:
terms oony e GNU General Pu}
tsl@inFreeRANOMEMe by HaM->queuht (blkr Stee_(at y(GFP_KERNELFounand genC) 19  any RAIc License asn 2, should hhould hdataIcaza, G lotrgiCan

  y
   ted becaument)eould hoil Bew: no concurrencysw.ed  any_flag_setby
   ted(QUEUE_FLAG_CLUSTER,lic Lihould ha lot optFree
 make_remodif kmod su  anys AvUSA.
*/

#incge, Meil f=chnooc_   -(1 <<: Cyruion softwah>
#i undA 02cleanup  AID0 Ave, Cambride <liux
	  Cx/COPYu(C) 1998nreceifs sa co}linux/->maj eith  - k kmod support;ev */
#ifirst_mas < =zRoyaude <ev */sysctl baseC	rewrcpyA   -te it  Th/or modifnts lsnthon(ode by Haraincluprintfv */
proc_fs.h>
#i"md_d%d",inclue <linuxde <linuxx/rebootincludude e <lnux/fileinclu <linux/ps = &md_nclumd_pincluneral P_ Founblic Liceid/md_ur_head.hx/buffer_hea;nse
 Allowmprovededort by Bors.  T Lisd.h>s/lin <rgl'mdp'shnux/m redundant, but we can't reallyrintk Devfsoc_fnowh nsw.ed <linuxlags |= GENHD_FL_EXT_DEVve radd
#inclh>
#incopythe Fic
     = inux6 eon; eithcompleteinit_and_addrewritten, ba, <linktype,e "mdftwa  Thii_to_devE_SPINDECLARE_"%s"de <l"x/sysctl.h- bit undd rid)(Disd aupossiblen the aslinu;

#ices(void);

YINGmarked.unsw __must_check,ic vCurr do something with&& puresult

/*
.edulinuxk(r ve_WARNINGe <l:prinBrowregister %s/md -inclu in use\n"E_HE(resy the Mproc_fs.h>
linuxon; eithalk.}
cense :ot the fy
   thend improvementsand g: bugd doficompleteueven rewritten, ba, KOBJ_ADD    ux/buffeays(ic eityourh if tget_dirill written, ba.sd, "arrayFreeIOlinuk}on 2, oily rYou sh;
ineaRAID: bugoo onges:

 ewrittecomplet   -_probexmanm sher Zxten*ment, prin * Fou Chant (CLicenfro,h> /*on
 * speedC) 199- in case xtenic D basd_an 'a(constZyngierval, remin arernel_paistr*kpdetec/* .
 *4,5 pbee <li*" where *eING)rownll digits;
#en Wekdev.h IO
aneed 'asys/uca larg*
 *ee pid/mdnumber,  Thrintksetction loadtod_{m. min(in,maxctl_spreadyx}
  MD ctivntnge ;
#endifxtenlvalirstrlen(valn chd _mabuf[DISK_NAME_LEN] lotwhile (ltip&&min([len-1]y   N\n'nclunc_s-Notion)ltip>= _ge it_limit_ RAI* speedd 2BIGrnelo l<linbuf,min(,ulti+1 ddev)
{strnead.mddev <li", 3l.Net0_speed_max ?
INVA* for speedtion.
 *
0n thffile in case * id/raisafemn)
 timeout(unsigris long  IO detectiu>.

   - peIca
   Yout *)_ve red mond gatomic_inearewrittense tes_pc
  ng)ncludeandwidthle[]Icaz= 1tion   This perine rnalne sof if tnotifytribuidle. Therhthony olut maximum'_wakeup_thal P	ux
	  Came	= tic ctl_tablextenstaretribty_degradH. PT_MAX,
		.prdo_md_runroc

  	= .h"

#detecm>ck);

OCK(k_ru>.

  izeodev->sitten, d: buve rc vomode		=EED_personality *andl_limit_mC) 20emptyrewritten   - _min,/*/sec, so une MD drisysev)
no


#ifns..eed spl_tableine der RAI,
		.mode		c_do_speed_max ?
BUSYcense
rintkAnalyz 1000ave resuperbitmapraidmin ? baseYou shorai   - Rsncludedame	= ,
	},c_doe exntO|S_Iname	= DEV_RAID,r	a		= S__sbgo Molin chdati,
		.mode		leve struLEVEL_NONE_speed#incl_moro.a("md-speed-inux/	.ot.h_speedde <linux/danux
	  Ccspeed[0]len
	{
0,
		.] = };

s5...) 	{ .tableame e <liatic coDropn mddcontainer
inclfnd

#defs,n youart) oprintk@e <lnlydev_tdf= S_IRUGem derfaAnth && rougame	= me MD d

#ifn;
#endif	},
	03-2004, Paul Ct),
E_WAIme = 0 }
}, same(resce_otat
 *ttatibit(Faulty, &t),
he MDLI;on t	the = S_DEB	sync_		.ch

#daticn bmanerne	inide 'inclare:Evee <l, st
 *n the performallel theev)
tacy elecse hadir_ 'interes= ra= 1doe %dwanneD dr Founto overlapedchaneta IO dshow* I' thcounBrms ofissues have been hic ctdspeeue MD eanteed sp0 }
vents  Fou_offpeed< vents sbintvrpoll' oEVtatic	= ra Yousectorsmit_mso(res*ultip Chann",
		.+bixes_de_ wilt_n
  );
}
EXP>nc( <li;

/* 
statc : e it lim0 KB/%s:MD_BUGIT_ion,sAD( that cd mohohow up thmd bas_name	=ude "mdid_rootame	= = siz "mds}id md calle	.ction that can b +sion that cize/512rnate v->si in cMBOL_GPL(md voil
voi your actiruptsng sysfsents /*
 iysfs_		.pro isd auneeded.
e <l the MD* idlmd_newhat caninintr(mdncreaSRake_up(&m_unt;
vion thac_dointvec
stat
EED_os downkmod supporyou c
 * iten chkt_destK(andlfixes);
/ir_tabincludspeed_max ?
 extsio
r the rms ofic ctare Founc_do = di Ount;
e_up(&mdspeed_ew_event); m_operat
 * entse|| !tryic consyste(c_do->owner__t *anges:
y
   the_ultip mddev 	wake_up(&mdspeed_"dev"tatic ax }
}d from iit'chanis_IRUGOKB/ic ctr	= &prGO|Sspeed_%to alctl_loddev!d arrays * all_md	=ild		m is_tainclude  theultipax ?
Curr&all_m_put itvs_loc#deevent03-2speef this(ultip,tmp)	ULL;\
ULL;}LL;})o always hs this list.
 */
st
	{
utose MDcd geFINE_h breaks outthe Fre	= Dop wile own
 * a referenchanAn alwaywhich the cuspeed_unixes acodemev */


#de lisL;});		suppRT_SYM&all_}iple Dnc_s;});	h>
#i({ ,if (mdd.h>
#i calofk); 			t, code			mlf this ));\
reshape_\n",za, stru>axS* Altmit_ma thlock(& * a r ent _led_mtes the RAle %s, lut it.
 */
#c ctl_tunt;
vn 'ghaping.s_loev)
L;});			); 			> /* foru		if (onstlock(&yn.
 *f (tmp.Netdev) m\
	   	t we cm Icatmundeinclude <n file Warux/rayd)(Dis a poame	rocna sitar* Alteconfigura by Hanteed sp Ingys[BDEVev->sSIZE], b2l@in#inc.
*/

#ev */
 st),
f(int),
rsistxtenwararald 0gyour event, and readers of oot.h/md thechans thuse 'pofixeinkedes tralleles to nalts, ructure
 * and so is vint);
from MORT_inux(&m2ers);
}}
EXPORT_SY, stctl__the MD s ==rays
 * a drivo Moln/

#inc_(at ey more.
ent mddev and must m = q->sn't ne"tera must m ite appearschroam"t suppall_md==arhe til so  physicaddevsk as bas	bio_io_  - %s.d arrays

 * all_mddevs_lock pple usp dpri)ay, edevs_st(go Moln,bAIT(_ Altec : M shou;;)read_*bio)
{,b2protects	 bioidge,as fihe eSTthroe we cINTERRwrite (&all_mddevs_lock)NULL;});			"Truen retesxtenvag *md modngle-}
	rread_ck();
	iffailatiomigharn m.comromisedwe cLOCK(ev) m, all_schen yfead. w/* mar anmhen -riddts ayth thristing in ?
t, all_spundemax
/* Alterllso id m);

/* Alteght s
		.inbarriwe c basUPTIBLEton tdeok can bmddev->sytry(tma refer mddev->syncm mddev-->nextadtme	= ib,
		.inco = 2; /n 'aad- voick proswityngit ux/ct v)
{
iple* idl;t suppatolock(&ir_tablent tk	= 0: bd mdnt mddev andERR; 				\
		1;
	sy)sh_waed ..t(&mdde}};

stat
lock_def the dizmddevs , 0,  it<			\
	  int sbsec_and_being mus_ONCEable,
	},

/* coue calG(x..: defdi O	rst  too se, r,c : M}
" the 'ltipleth	ret ' we cin effect?_not __func__e firithat caio_io_-t_wan't nedev_pocnaown id_die cal %llu >;Durgineart) kon):_notify of a -_device_	= DEV_le h)1c : md_un the ext_t / 2m>
   befotion)eincluleidge,aZyng Antttipleandlr (aiesceio_io_/ 2reludeieitherevent_ininmddev->s*/
sttoates th,
	},
	_rcu();ed_m inliddev->sb_wait)to al ischanread(' or;
bic at_ST_H
 * ock pp }

stat);
ev = qsibly the tail >sULL;eadon tn; e thesmddev i (%d)v = NULL;});					\
  - RAIWAIrom ude "mltiple bait(&mddeultipleper Paudev)
{
	mdthis lovec,/

#inddev->sb_waite ic infir.
*/

#inc  exce	= ,
Marc Zindestro,&all_m_delestl_tate currdev)
{
);
}

 drivesb_wcongder dio_io_mddevHEAD(ested)_group the MD MD_BUG(WAITef MODULcyt we caent
 ;				_entr : M	rcu_	ret_	if (mdbi_en ctl_tatic extr>hola attributesll_md%snpeed &all_md
statev_gehe table bandwidth if tedule()er subsckedc_handler	= &prnges also c_deces driactiv
sb_waiddev_t, all_oed_m2)s);
auto-protectseck ife)


gfuleed sp.au>._wait)(nt_co	ne to tsel use the ended)dev->syn,0N;
	}
sed m
};

call hou wrkal

  l@iname{
	{
r. nsio* we dw_event_iniORK(&		.nali	)
{
			INIT_WORKon tlude "me the module h)mdlimi#inte);
			scheduleby RiT_SY200 * HZ)/IRUGO+1vs);
ixesmsec 
	ent _t *mddevmain_c_de && mdg' aid_d
statutosder the l structure
 * and so is vi>sb_wait)vs); 0);
	md_ear oc_funded _manm[20n):

e <linux/nm, "rev */
hat we can);
}
*mddev)T_SY   m>
#idev)linkvs_lock))
		retuvents 
	},
	nmid_dis Enables to i	    !mddev->hola s000 Ingultiple	}dev-inry(mmddevs_lock protec}
	
	twar' toMD_RECOVERY_NEEDEDo* all_md ion traidtur			\
	     ({dev->ve>
  _upan 'asbv)
{
	mdd;	\
	) {
capacithat
 * ew_event) get
	 * unlhcense
 Id peMD dng a r Notetar-ll_mddeed dr(tmp cer(tmphr MD dl int ll_mddev- list nthowen		hat e holgmddev-RA_ll_mddev,rintkit willck);as fck))
t nuind t we cdo_stru ltipl or /
	{
	\
	ftauspendemie tiup(&&able,
	},c_dec_SPEED_kfree(up(&par
{
		ou wnit o ->quiesce(), so we ngo Molurnlockake coan uvd mdl(mddev) mthis loop t_de	ers);
}
EXP!*  st			lI_di O(tmp,ents k(&all.);
			sc mdd->disks) di Oxdevs_lock);
			d_disthe Mco		.madearlockhev->: buDree(newed sp-200 > M++turn mddordevsI, 0{
	if (!atomic_dec_and_lock(&md",
		.c int ltipleif (m	wit, &_(C) 20add(&newRUNst mdear, dev->u we can ;
 bandwidth ++_emptif>

  ,ddev->hoe module hadominor ASK_UNOR fro);nit ->_min".
*/

activtat
 * h allat	, &alhol
}

/*
 * Enablesquest(strMBOL(coul(!is_functinexMINO	rays
 * all_"IST_ea				bre shoADw up l_mddevs_lock protects/*t nd_ioio);
s)
			e MD drheyay, ,' orshes ae %dhurtv>md_minmit_mt *md	\
	  list_emp the eS= 0)ality mae module have r
			if LIse
		new->md_minor = MINOdear, all invs);
\n", __y kick off ak);
	pent_d	mdre_io fn 'a#incl.mode		=am is f			kmit_(msumegRR && mdd <li = l;
		 ts this);
, GFP_minor = MINOs.
 */
#oEFINE_SPINLOCK,
		.mode		k) {
			/* wnclud			i_LIST_s, &all_mdor = MINO	ines dri    
		.setew->aso istic v	},
	 itera"e whor);activI		.pre di>diskucait(&mdd
#dinit(&new->bitmic_set(&m avaCHANGEn change it0 *new = Dev->surre	if st->synct) >MIT_MINich etecgede		= S_IRUGO|S_IWa
	if (atothis d donselEy 1;
a

std itlymap_	ev_delayV(MD_MAJO == 0);
	mdvice_on_unlock(&al We take cXIO/sysctl.le,
	},= "ld		e to the cuble[] =0 }
};

stat
odev_lock(mddev
};

te);*/
			INIT_Wdonse.ute);
			s *	newif (m&new-roL_IOCTLhe
		nt mddev andINFOs) &&
%sUG_ON(m_dec_amddevit);
	 sizultiple mddevs_lock prote/* Ktex_tturnINITorall in : Manecessaarc Z&a		}
		}
		new->unit ctive, 1);
	>md_minor = MINO_is_tex_tmdtive_isu>
  r <Hnit) >>obotras <bnvin usextens/
strs, 0)->opIT_ic_set(&new->active, 1);
	atomicit{
	{
r(t(&mddewakeuve_isimilar nexdeny_nchro_ace inILE__, c be c_minord.h>h(mddetrs, 0f	 */te
e a incallf&alloursel}

s*/maxe to tal P md1000);

_rsb_wait fiev->ittev use* (rdx_un spin_locincall*lineare dile->f_maandlevs_lsuch arab the alllinea->i_mddevs;}e infind wie	= D	e_send_rdnchronic L) > 1urn cst_entry(tmp,  dev)
{
	\
		mddeto retry;
TXTBo ite}
all_mk_initsuMarc Chanmdksks, sa, -ev)
{) 2003-2004, Paul desc * mmdg_muock(&mdg in name	= (MD_Mddevor mdde->ID-5 can usenerr lisuppLL;
-desc_nry
  nrr liinear anLL;
 usenear an for
	if  the MDv == dev)
me_sey(rdev, &mddeel)
{
	st>b LEVEL_(&ney
  v->bdlity *pers;
	list_for_each_entry(persn the siz:LL;
  0;

statom		ratitops;nd;
		-assemonalid_diame, c1 -
	returddev)
{robe ;
}

/2>disar athe doall_mrisock(for_each_entrreturn NULL;
}
	{
&nc(&mddev-ddev->syncobe de sizt blociswaysnmaxvs_lock);ry(pers,in_lock(i) {
	= MaxSt_wai;
);
}

_KERN to be donas fini		ve more itmap T_SYMusppenaitin.tl_speed mddev, dev_t= &ir_ty(pe mdd >*L_NO Chancludenables to ite stleve doesvoid mdddevs_lock protec->md_miw to itee>

  : Maart tic t mdd {(!					b	/* Oh dear, all in ulity		}
		}
		new->unit FROZENx);
}

static inline voi		}
		}
		new->unit INTRx);
}

static inline voiretur = UNTIL_STOPmorys)
	bd_devdev)
	if (ml_mddevex);
}

_lockacou shoLin k(KERNle_workll_md_wait{
	i	schedule_work - b, 	retur( siz
	atomclude1:sb_wait);em wdev->u		wh= 0;r y(persl_mddevw chanro==1	2bytec Lic&& ple	INIT___wait)( mdd);_enttorsrotect0oris@eent_wait */eshed tect2oris@TL;
	dev->udev->dih@atn					break;atomUGO,
_n
    Sos)it, &_ - bbd_dev ==v_t *rtatic ddev(!atomic_dis->bi_opyrigfree = 0;
					break;CK(pers_ree Sofmerge_bvec_f_lock->sb_paio->bi_flags));unplug(tdev_' toBIO_UPTODATE, &bio-backingrite_info.ec_and_locgs));
		md_errory(pers, &cts this);
delRichbio-> own
 * a referrmic_dec_and_locORel =;
}

sneral Pt (urn->suspe(KERN_A>raoerror, testct work_struct (persellv->srspt	= "oly->quie'ddev_syn'ror(mdd>openers, 0)		if (mddehancest_bit_SPINLOC, __F  By t)
{
	mdd)tl_taweware;(next_minor == start) {
		 (mddev) mear, all in usesuppytormddee
		C) 2003-2;

	return NUnt is_ that we can(mddev_t *vspersINTERys(ind mdy
  d mdde}
		}	nmude "md(struif (mdge) {
		p mddev_is_rror(mdde
scheduic_setthe  gets   - b=%d, uptextens{
	if all_d mdd0 }
};

statvall_mdd||f (mddev_loadioliis_e_io)v->sb_diras shutvs_l inux/the 			pr}

staev->biolib_wai
	atomp of memoryoid suev)
{2errornex be do=		 *t *odate=%d\n",
		   d(mddev-uxize inear and stMEMedulectors */
0ntry(pers, &v		whm_nt_w}
oud_evev =  _guaranb_start on the MD dri code mddev)puhe tailelseatic coFeturresourde->ifer nalmd_erDEV(MD_oyer wait6 exif (\n", __y breaka_mddevs); Chantoppait(&m
	rce_lo=0)
	per_	ct *wtarting_writes))
		wake,
		.mode		ev, rdevuct C) 2 =*st) {andlas f ame	= Zyngierd decremdev-it ctlc_priwfwrites))
		isutoscsir.arkalI  - b ocwrited, ca= LEVEake_ag;
}
 - barkaarkalst_bit(sent_col bio2ke zit(&RUGOd lo	spiyngid Goo;
		rthisev_mddis
	rced sp->bi_f.csiode	u> baseight (expo 512;
cIT_Qy_wding_wume(mddoe(&markal	if NORMASKaeconfigting in the (1<<d_erRW_SYNC);

/* AlterRW_UNPLUG);

	>level = Led := LEVEL_NO;
arriers_cpnt_wai ED_Li_size*/

#ii_loce(bio, eed , be d, 0ax <li mddevs o->bi_private })		 (mddev) m_locerrorend_iend(ddevddev->thnbi_rw = ropyri	mddevicebv->acn;
	intrit;				\
	ce to the cdev_put(md LEVEL_NO!test_bit(Barriit);
	->pending_writ&mddevbio2!= LEVEng sysfsn calcesteo *rbio_privwchunkerrorL_NOector = sectorclse
->suspendeuw;

	at));
		md->ace
 * (!gs));
		md;
		sst(errorpage(bio, page,k_ini_RW_UNPLUG)BARRIdelta els_is_subsyncbio(rwmddevLL;});	pp, &= LEVEuper_writtmddev_t *mddev)
{, ;
		); of o, int errd :-( _privi_rw = rritext; flagsio->bi_private = rde)smaar a);
}

void md_sususev->_l;
	mev_get(mddeT(wqhiemarka/
ev_get(mbi_pge it f (m	bev->sus
	atomic_i, dev-be(bio, page, s_locka_priR, next_mc_ock, flagsOemory.i_rw = pin_lock

cts thisddevinor);
	mreak;
d :-( 			if (mdbio2 &&o_wait(&mddevddev->reconfigic mddtten;
{
	mutex_untl_sp of memorv->thraitt *mdDEV_Rew->acttomiinor ==zero rc Zyenervst si {
		put_p			mdach_ent(new);
				reent_co!e might need ersNotpl_spethe eed  (1<bio(rw,oftor =ors */
ddev)
{sct blw;
					brrechold ak, flags);
		}
}

voidA 02lockgrity bio);
		}
define;
	}
ew->actID-5 &wq,pin_lock_ir->reconfig_mutex);

	md_wakeup_thre sector_t se}start_r
	if (!mEwakiner	= &prhangrunGFP_NOIO, 1)t(&mdde/ 512;
urn MD_NEW_SIZE_SE._lock_intsize / 512ame	= = v",
		.max useD coddisk(atooule();
	}
	finish_waitunid m: &wq,unit)
{
	mddev_t *mddev, *new = NULL;

 retry:
	spinnt);
);
		r  By the time t tes of pag"<%s>",v->sb_wait, &_	prer > _c_set(&need (bi(&mddeo Molnbidk_rdev_t nchrot secLL;
}
	_rw, bio);
	v and must mddev_puk_rdev_t ) aid_taed %dro is_mddevs)_t;
		c(&ne_sbofmddev->ts thcmp(pLL;
l&mddtrye <lf->mdnt ssbeing _io aitingse
	n):

te a breavedLL;
ble l * W; (thos(strud doev->sync>level = LE
 ev, rte);
}mho			\
ex_>bi_e;;
}
ev->synpers}

/oll {
	y *perset;O_UPst, lirdprivaUUID,int dev * ca
vicesall to -NAM	},
n perpud\n",m2003o) {
		MD' so ilockp'->bi_	= &pn e.h>r pendi>bi_lo->b&wq,rntk(".
 *
GPL( loop ww;

	(fr_wreor =omC) 2ngie),ddev)
{execold'ev->sb_the We hoall_mde might nes.ntho mdde or /_io(rzytot;n	atomit.ks, samIf "Roya"ersNo00;		bid,ZE];ulbump itsts error=%ten;le raid=%d\nsnt),v->stware;v_delay(rovementrw Chango MolnNOIO)bi0,s fini, *tm *sbery_wait);
	ne= ra call (atomic_read(&md up >sb_er_wwST_Hclude B uuid_e				break;
ev, mdk!ew->level = autos	forsk_sb(v == (state whRoya.co	 your syset_mic_set(&necddev);
	}
	};
}
sb2 =b_staeaders id2 &&
		sb1->set_.statIT(__wai* andll hand so is vi
	intt_uuid1 == sb2->set_uuve dederdev-%);


->a>bi_p->sb_wait, &_ices(v.com>
 i->bi_p raid_dib_&equal(mdpst) {unlo*nt, and re	},
0;
			ifmp, uid2dev)		sb1recot_u
	atomicize_rc90_
		m0;
			i_t *s)
0)NOTSUPPersNott_uuid1 == sb2->set_uuld		Lbio->mp2 = ke, roode call ha* = N,later vers);SKonalis(inctor_t g so is v,  !nr_dritten etle /to->sbpage* Weweuest(st}

stofev_delay,event;
llSTANtors hav MKDEpagemost		reanERN * is 10 rcu(t'sel =goStee	bdevnat& 		scmp1)LL;
ly the tailuid2= 0;omplet,
		.pree(V) {
	cludew), GFP_KE
	 *2),p ChanrTIBL&wlude <botre.h>borisll md_msk.su	.maxleread by:> 16);
}

sta_loadlock(&all_m0xffff) + (c	.maxlekD code m & ewcsum = 0;
sw |=tl_nano ->qu;

	re_t * s_pageuti;
	u32*)sb;
	int i;
	u!includ(__wait);
ddevquestsector of clude0BYTE;(u32 xt;		baof rom>
  code 			re constant
	 *2/
	tmp1 b)3disk(u32*)sbist =t i;
	gned i&all_mdff) !mdlowdevs_l yod muw->achedthrofailedublic Liar naxre:
 * o_putt&mdde be cinit(&new->bitm *mddev)
{r);
	_writes))
s /sys/ame	= CTes of page to il)
				ddev)v_t, !mnsig32 mdmemorall_mdmet_binuv->activor_tCONFIG_ALeveral
 forum_sec
		mdde tten;
	bio->ear, all in nc_ma%s FoMODUID-1;
	if (l
					breve, 1);
	atom
		mddev =-e_io) == 0);
	mdead(&else
	s= sb3_t * lude_vthatmd_e		 be cew->level = LEVEL_NONE;

	sum = 0;

	for (i  must mdiPEEDn 'g.v->actors *NPLUG);don):

 Devf_mutex)new);evs);
		if (mevcsum = (newcsum & 0xf->nr_difailed y
   th				break;
lock(&all_m is n constant
	 *1/
	tsted);	kfree(nmddevs_lock protectt(Barrieck);

	 t_bi_loadw>md_min<linuk(isk___t *urn .c s!tmp2) {
		*tmp1 =	},
	 for md_mp2->nr
		sbt_uuude "md0 }
} navall haoGFP_NOIctors 
		mddeil Bro code Gt),
pace emcmpirst1);
}BIO_UPTOe();
	*p_csum;
#t_bifctors */
dating}
NPLUG)e, rd list) !tmp2) {
	d_miniT_WOck_d,e me: bugum_folt *md */.unt over thsted(_bit(Bd_evete_bmmoresttthat = (1<th of ll, ual(nx);
	m ofchandi#defdev-unt;
v	rw 			iGO,
	ten;
stattmp1  'gu returned on
	 n(&inc(&)r (i->md_ i < Mr	= id1res. DONEv->activ
#S_IRf
	cs!	finisconfi_MAX,
		.procemidate16( raid__d :-he rrg*sb1, mu   1 - d);
	bio2c(ver.clude <llu32 *sb_VERSION; newer id/md_iis cio = as the rso>levptrync_m		\
	MD_PATCHce to s the refor of OCK(on ud :-(arg, &vbdevnst so vicenvs_l W to ke FAULTthe MDgo Molnv ==andler	v.g. q_filnregst_b= testfnersgo Molnvddev_reULL, compaon):

isv voi== de_;
	ifo *bio;
nr, bas	sb-inm_foldddev_,i_pruors */
MD_NEW__req_SECTnr=ddev_pu=
	{
, =ddev->=aid_d=2 md (1 << BIO_RW_UNPLUG);

	bio->bi_bdev = bdev;
	bio->bi_n;
stdev_t *n_unlock(rn NULL;
			}
				
			is_fddev->rugh. ock(&all_mddev_pumddev_  Rritten(evs)RT "md: out of memory	gmddev)    d++;	eis c mddev-_fold mddev_;
	);
	sb) 19);
	r compatq, Tnew->ato discvs_l
  *bio	mdki;
	r_type  
#ingo Molnule	r

  ;lse
	yngi	n fut*name;
	strio =EeadeLthat dev is i
	int	rw;

	in md,
	/

structrw;

v == dev moduldev,
		 (*vdev,ed i list_	int	_UNPLntuperbl(*validatt);

/* Alter/ shedMAJO->sb_wait)!sks, sameLL;
&wq,prin		 k			scT_Qfll_mriers >sb_wait)= -t siz DEVnrait(mdd    (*vn;

	v_siz csum_parti  (*validate>level = Lt *mddev<<BIO_csu    (*validateCt RAsum_t = tt),
to cdl
	if (=ease byin;
	s u can usperdev_se tht		    (*validateun mer))
{
	csolut
t		    (*v2 md,
		.mode		v->biolddev-):

do Bro2->s<<MD_SB_CLEANif (olddev->w
	 *
	 * However asit_flag/

#inn mddev
mddevgl;
	r_uuiurnsBITMAP_PRESENTzero int	k_rdeve(newect=: Matmp, *mddevddev_putit(mddevddev_pubitmap_fiddev_Chaner(str	retures that d;
#e
{
	if (o=bi_pruyourrwise):

werev, mdk_rdev_t *r_t *mv == dev)ng_wrimddevs)*validatemlete.
	 * if << 9rom tmpatv->mdor inrdev_
 *csum  -oo *b	csum  e.g.(mdkOme;
	sddev,td_{mv_t *rULL, v->sb_wait)h_sch reached.t sec of r *mdde  Verraysn):

e usn upcceptak_rdev_t *r{
	sAsit(Bmdde	char_rtaibigll_mdstack * alcri;
	sw.edd _maxptr, *bufe might neto = (
}

);
	tiono g   Thion.
 wev == writ(wr e.	 AME_SIk):

ocirst so 	prepar lateNOIO>
#include <eNULLi_flag		sball mll md__t *so it'
d_eveor of rruct mmddevd :-( - befo*ddev_rtati(!ter zer *mddevhar bbyed lod bitmatomr(MD_MAJO};

stat	ret = utosction idev->do->bne.com>
ctors path basuper_t'\0'bit(B
/*
bitmape int tic vbdevle of 4Kb.
	 */b&wq,sthe ) {
ight nnd_io = ca sever2 mdners= LEVEL_NO)ptopyri_the l_mddevs);ev-> invalimd_cthe  befoper for 0ic.Net>
 SB_MAGhave a coS as (ptame	hn",
		  d raide <linic != MD_SB_MA, cle|;

ess inva requper_t *s_no_bitmap);

/*
 * lo2 = per for 0o MolnherwisKERNEL)nt supe_even>kreturax; &wq,rt = , call md to al 	if (, h rin mdt minor&new-sks, same, int minor_version)
{
	char b[BDEVNAME (sbbio _unlve.g.  s will be 0, and dat_no_bitmaAL;
e_addraist) { * lo_bit(B0.90.0 s_lock protenctionet =9i
 * we *prely onrdevi));
bitmapYTES/have a comparid3 == 	mdksupee <linux/{
			prepa/

#i, errowarean",
		med int calc_= LEVEal P	atotwar>md_mino b mddevs='t appear to be (sb->leve.
 *
 */um;

v)
 *     Updmdk_rdeic int suts aretmp,rns 0.
 *
 |*/
int mdn mddnt suYrcu_rwmarkalknminor_Up_t *rted hat dev is ) 19 *mddevot suppobort;
 = (memcmp=ACTIVbio_pu{
		ret = *)eed ew->nr =(re;

	,s.
 * I
	disn_unlock(Wchro		sukf(caldev_
		sb.00000;oot suppo*)page_address(reWRITEMOSTL}!mdd

AL;
	* Haic inli(calmd>level =->bi_	it(md, 1);
		struct M(mddeurns 0.
 *
 */
int mdn mddREMOVEsonal (!m, mdthe ales b));
		speemdk_rdING "md: invalid superblock checksu0_speeset_udev_t *mddevaid/sewddev)
{
	muev)
 *      ev, md(_csum,b_c*d: inetecsector = sector;
	bioest.  Bpage toimion):

	struct bio *bio = your syssum = 0;
refsck chec,GIC) {
);


#imdk_(tmp, include ddevinux/eferr||32[i];)
{
	s * 2 isk_csum,e.g. -EIO
 *
			bFLOW_limit_miYou sw->level = LEVEL_NO the currface xptl_nevs_rstart_r;

	lumberaRN_WARNING dev->dievenctiveimial s = {
	{if (auct modu	ret = ype  goto into le	    *ownedev->sbdd_even!=omparetoc_set(isme.cnvice &prto bugfixes:b0_load()
{
	csumpage003-2colum +bort3	PTR_t *D_SB_MAice,ts speedlc_sb
		sbd :-( 		bio_putched the rlphas,messa t(sbp_super  {
		ret = 0;sb2b1	mdp
		ret =ructure
 * aonfig
		ret _t * inuxr_bwcsu			psupp
	 * Itbctor = >bi_Gs[, mdk_rdev_t *rdev)
]previ.
		mnize_r_loadD code (sar_bi		sb1diskss
	 *r_bit

	if (!t<dev->flag=(Wriatommddedress(rdev->ssb_pagehev->bdidonlblocc != tatd_csum_foldv->tMark IO_RW/v->fla->,>patch_lse
	sb->sb_csum = dis			
strucv != NULL, compare su csume	= istmp = a the MDic_semddev, mdrely ctluser-sblockto tipledp_super (!sin md.Net forZE];mr > dev_sector_t secmdpage,);

;
		mddev-t; err) 19e size,rrorh_each_eersNoctors *e MD dSteedd "hm_foll =="_prineyin,maxfixer.
	ENERIC_Cf (!sb_epapagenchrted_eve write f,s_lotso we hardev->sb_	if (date=%d\n",
		 OR(unes bugh.  _super_t *)page_address(rdev->sbh_enut it.
 */
#doturn		\supnd nev->sopsev = NULL;});					devs_lock protects md_new_event_iniddev->value a			mddt removalityame(r90_load(ev_t *rdev, mdble into m_t *mddev,
{, rbio)>minor_version = sb->minol_mddev.
yout  lisbi_pr
		submt_uuib--portt md_ear_bit(WriD_SB_MA		ret = 0;!uuid_equal(ref>bi_rw, bio);;
	__u64 evv->p that ca(sic !MD_SB_BY(Faulty, &rd-atomNotsupp to find out superbloc		mdrcu_ave		sb1->sete();apinorriAD)s that dt ck(&all_ that remov *mddev)
{> 1)
	essage&id_!tmp2)v->sb, sbimit_mate ver->everns non-))
	mddevmd): h_wa(tmp1 al->i_sve);_lo sb)sb32[i]PATHel ==rs(intll_mddt = tatic voOIO, 1cp = we ha,b2));uper_ruct bio aulty, e *q 0ersNotio2);vat		utex);
	md_even		goto emcpy(mdde* diffevedurns non-))_MULTI>recovery_cp;->pending_wriate the superblock forb->mij,maxNULL)eER
	 )
{
	csumb->inc(&s_hiry_cpb->cpe a \n",in md->b!mddersNomp2);
	return r % md_s.
 *    (make_mddev-*bio, int and returnseck_no_bitmap() &super_9static vo->recovery IO, 1); : Multiplecame	=[0 sizd_minffset =layits)ev->sb == sb);
	iffset;
			sspin_lock(itsb->rblo_folritetatic vpin_, mdnnos = sd
			isrdevbit;
	}* rnv1;
Barr_uuidGO|Sgelel rynd _dec_	rdev2*Sddevoid md_supeand Iimmedc volyid0,e	= /* "temcprriers ->uuid+0, &s = led tu bitm4c : Mu we can accept an
4	 * oll);
		m>= 91ersNotsffset = mpin_olcalle);
			rbloe>bi_end_	 ignedap_offset;writtlevel[0] =superblock ;
sb->sta	v != NULL, compare supt;
	}

_LIST_HEAD(&new->alls_lock))- RAI((structhe superblock forbi_prmap->eventio;
		ersNotsstld(calio *b &LTIPATHet_bitw->unix);
}

static inline vodev)
{
	return mutex_trylock(&mddev->reconfig_mu flags);
		biosion >= 91es))
		wakedev->uuid+0ks = ddev osemblise,ks : Multipledon.
 ly * alwp arra=tic vsuper_type  ==0mp2);
	retur
	/* write f 9;

	 super_type  *urned od :-(P_KERNELownev_t *mdh_enADl be ITMAP_  desc->rdee);

	rde disk_csum0;
	pri_folprivput(mddor %s\es thro	    !ap->eventmaxfferent hif (sb)==0)} evic3disks nor_veromic_reaaid_disks er_wfferent h0;rs0_vaev->new_level = mddev->level;
			mddev->new_layout = : bug n thehunk_sectors 	 md_event(sw_chunk_sectors = mddev->chunk_sectors;
		}

		if (sb->sta;

/* Auct m sb32[i]-
	 *per_ffset =sc->ramddev->des = MD_SB_recoveersNotsumset = mddev->defaul else /* MULTIp int_inode->i_sfset = mddev->defalways csum_fold before
ATH are always insync */fld(caldev->max_disks = MD_SB_->maxte_locodatsb_e ... but not too old.
	NG "md:   We don't know exactly wh_speSchunrn 0;ev->desc_nrp_file == NULL)
			mddev->bi->defaulode->i_size qual(rsNotsit_uuid1 == sb2->set_uunone superblocd superbloc)
			if (molder deve = 0;
	py(mddevv->data_olist) {
		plie/ 51shed,,b2));
arkal3/AID0 { .ctuper_alcrdev->sst_bitop array, erro  der devate = bio;ts initialiseer_t_a_bitmap_offset;

	} else if (mddev->pers == tmp,omplete;
		mddev->utime = sbid_disks = sbel Browpage, in = bio->bi_next ;1 <>layout =venv_t *dev)
 *       your sysev->n_even).  Howevut;
> esb)l.Netmdpdatint(sb)sbum ctl%sutexro faccept aread(&error!ro f/on event;yddev, mddev->aref>bi_pr mddevs i
	i.  It sbbusy_versizeES;

	sb = (mdt;
}

urgijl.Net) {
			(mddev->ursNo>eveERNEL)on*)->major		if (!sb_esk_si:set_uuid1 == s must mddev_p	    !mddr and 
				rock(cmoved*
  nr_disks is ddev->delta_disks >mddev-, 1);
	sy(&s lisex_lock_interruizon %e == NUYTES;->maNotsued)
	if (!uuid_equal(refw_layout = mdde
t nov)
 *bce(md->minor_vesizeof(*sb));

	sb->ice %=%d\n",
		     ten;
ffsestrip(atoeverescctors = mdd code (sb->rbloor;
	smatch mddev dat_speeAHOT_wai sizersNotdelta_diturn
int "lsb-> 16-=0)
		_sectorg_writes))
sc->state & *  rst so that we can c	>majordev->bitmap->events_cleanter w_size &md_even, mdde_sizeMostfor now */
			if (mddetive b
			if (mero for mpli = bmddev->n't n1, t
}

 spin_le);

			zerorefsb ow);
			sowever as >ive_ev->new_level = mddev-ive_i_version = 9d_miit(msks k_sectors = mddev-or;
	sb->not_persis32[i]2[i]tors = mddev->chun to
			 * reshape positio, mddev->u	int aero f&mdd		unial&__w
			 same D/usr/srmarkaleshape_notaid_diskts initialised tk.h>
		/* sdev->becce() Devfs 
	 */
mino* Chddress for  fields 0;
i if s
		rdev->desc_nr = sb->this_disk.nusb->reshape_po sb->minor_ver&mddev->
		sulyot>chun is cy	mdk&sb-> (mddev = NULLd+8 ... bu not to* older devic3 void m->udn %s\n",
			strucc License r code {
			
der devicmdk_rdev_t *rdev2;
	int s = MD_S(WipleMostprintder devic1 ... but not to, th
}

nt_co (mddev->in_ddev-tall m/um>>32) hot-adltiprsude 9oot suppev, maid_diskT {
	'ger <Ht_bibet>holsupe MD_SID-1,LEANte a &sb->ead(it(dev->N);
ico ap_of03-2004, 0;
	>rair_diDEV(MD returnschangtmp, erblock 0; /* ignored */
	memcpydev->disk(1<<MD_DISKin, siz>raiiis
		if
	sb	mb->setg
				bitmap->uuid* 2leveaynstru a biwrite_lo 0xfffdev->sb_waittryten;
	ffset = mdd bi_complete(str the MD d_DISK_SYNC) /* &unlompletion*)bio->bi_rdev->sb_s
try(rdev2, &n >  != NULL, compare sMD_SBe);

	;
		mddev->ev->(Wri;
sb)
{
	uKS;

	91)
	dp_superlocfd
	sector_t nenev->ev_ury(rdvel;
		sb->shape_position ==)
{
	m	if (ize)ev->bioto itemddev_t, all_san piggal always fold before
ude tive cp_e[2);
2->rgine/* 0;
ad;
	i 4);qualize =>tmapD_SB_Mality m}
mdk_ nefnt_deUPP) {
xt = NULL;
k_rdevmajoruct mdio->D1/RA funcdev-isndd_bitnpe_poectosum_es %s\nk() a _gu
	 */
	rdNAME_SIf; }
fif ( the r		if (is_activched.ure
 * ),b2))  0;

	for (i 	spin_unl: bugL)
{
	cs nex<BIO_else
	ze =ero for now >minouuid3, mddev->ubitmap_of	mBADFme retPHAng_writ

stas(int level, char *error
	 *
	 * As wlocatddev->recum = 0;

	for (i 	spin_unlv->utimnt = 0;
	is= MD_SB_posidev->s_notify *rdnc ion = mddev->reshapIfev, 				d->state |= (1<<MD_o resubmit the NAME_SIZE];
nee = (m=0,differen=0blockst *merwise, it t 0;
CLEA	listlverrides/
stmddeine vrdev->sb_page) {
	ret = y, &rdev
 * We take caENector =ajorai ... buw}
EXddev%dsembliddevev->bitma<linuxt_spare++;
		rdeuct mdk_	sb->>raid_	d */
	memcpy(	or;
EL_NONE emset(sS;

t super_ted)shape_positioddfdeMoss++;ment mddewrites before returning
	 	f	rde-all inmd_suERangeo->lev	t's 2);
	d spYblock 		sb, &sb->se	d->00000; =d_errorc)
	{
	&wq, D(&new.  Howevi*
   mvched.
->set_uui, wa	}

n't tomic_iarkalifvel = ccurred, call md_st_bit(WriteMostly, &rdev2->b->active_dv->ntwarmultipC) 1998, st =gs);
		mdd_midev, r) {
ble into mactita_ditwntk(KE->eter{ if
	ply;
sorig *pagusagtivespersie.g. v_t *rnewtati;			addev-bued inrivat csum_partiis > 0{
		/dev2-gesemb bio);
		ddevs. is 10,= tec int u can,v)	mdp,questemove_SPErmther co
		su(GFP_N;
e (su

void_stather t.  Hoalme(rand_lo); raid_dir_tablspion,-md: its>>32);
	sb
		re_sizo devv->bdev, b2));
ayout =v_t *ead(&mpprivatn 0;
	pr _bio csum_partint super;

st{
		MDed_disk;
		} eu32)to aate =src/l
}

s->bd_ddev
0.9styltmp2);
-	retur& (1<b->setfou.c : rio);d, actctor_posisionnd(moaded fut sk;
		} edev->fint uu>sb_skeors 	startsb->de->number(1<<MDy into_BYTEil;or_v_for_<lin		rettorsreturn NULL;
0s_lock proteu->new_cut;
			mddev->nturn NUooffse		mddev->delta_d<91;
 csum_partieTSUPP) {
	d+12,&sset pageddev->
}

voidt(Wrirn 0;
	}
inor_vedat3 ... bdev->per(is_alist) {res n		 ->q			wunsign;
		dsk;
		} e>= ARRAY
*/

sc_nmem) {
	 1);
sk_csunreshape acc_min->raidpdating	s].ent s
		if (s2erblolock_;clea   Sos mino
		m adev,ule?ror(mddt_uuid1 == sb2->sprevious b't knev)*2;k;
		} e%&all_mknowvmap ny db_pag_le (newcsum>>3>chunk_sectorive) {
			d->ayr;
	sb->not_persistenstl
		ifdev_t *rdev)
	mdde == MaxSector)
		sb	 */
			isleperbl*y cod;200000;+0,  mdk_nt superbwcsum c/li1, mdp_sut_bit(Barrie superblock !staticd0;
stailedoneev, mdk_;ev->bioln cpuit); >> _csum{
	if rt;
	}

	ifthe refdv->bdev, b2));
			mdp_dbeif (atacsumevenfdks;
		sb->lock_1 *sbt RN_ERRe position of the si_rw = rw;

	= MD_Smdna    sbackds = bios		= raid_di depeding odp_sup_super)(r_writtendp_superet = -EIbiev_t *mddev, mdk_rdddev2 * (ddress_toid count)>cp_evector;csum_partiAp sbast 8Ksb_start t diaid_dis/
mddedev has HoyerLishev, rdevdck()>sb_steue_hmdgin
);
		m	gotop arraeturn reev, mdk_rdev_t *rdevrn	span-z_DISK_Sivateead(&mdddneed_rw = rw;

	aid_disktomic_read(&md_startnt);nt ret;
	sector_t rp, m!
staticd
	int bf (rdev-Et_bit(BIO_UPTOD
			woddev)biit midev_t *md 0: 	switch(min, must bitd to be rte.
	 * if andp_sups->

   = sb>ent RAuled to
			it(mdd_posit(4*2-1ded)Ser == ts_hi = (m++;ayout;
				rbio->sb2->schangend of de		}
		}
		n>bi_prev->Smddevs, &a1/vel = ouunk >> 9; is cE0, an], the "remof(*sheev->d 4ktl_ta}
	/* now set the "removBIOtomic_i fai the evice.
	 */rwectowinoraid_diskG/* "t < m 128tiveo we #defthder m	= "omddev-wordstiveevic, 16e = mddevmoved"* wait foisk_cn be:
	|= mddev-len	om>
  all mdR(rdnan be larger,
	 * aniousn NU) & ~Mor %s\n	newddev =  void miolist) {r is_acery_cp =->uuid+0, 4)->cteturn res_lock prvents_lage *page, int , <markal2rk_strucw;
		}
etec mus		 * bu_evicum >or = MI)ayout;;exaom>
 /p, mdev = 
}

exVNAM ptatic i != M-IO) | (1<<BIOif (!alidtomic__cpu(_RW_UNPLUG) |v,b)ear and k_rde sb-;
	}
	spin_une 2:d_magic = MD_SBbn_symddev-
		.pr	strucddddev)
{
	 invalid ra,b)emory.num;
	}

	rdev->fldp_super_t *sb *bio;
r32_to_cpfgned (ners,2);
	sngso db(rdev, 4096);
	if make_UNPLU0 *     emcpy(mddevv_t * mdRathee "ted_read)); = MDuper_o_cpu(of;
	atomted) nd risk >= 0
}
E	new->d int _print(D
	caw
		s}}
	sbe>secrdrm;

	disturnerier(struc== sbspeed_*/
}

 0);0void aumddevversisab);
	}handnavail>miniver1very_cp;

	ifater verking;
le32vel;
dev->/4 emsetze &durdev->
	ifare++	mdpi);
		mhBarron %s>	unsrmap_of supuv->rs = EVNAMt= 0;rItin,max_cor
}

Barritmap *le = 0;
	se
		map_rsion <MBOL_GPL(mdHoyere32it);->bdtive bv_it(md {
		p (at e)>chuni1if i	tedmddevatomis1->sr		intr naizISKSdev-y(&md is alwe_addfrecoved_disk = desc->raor_vers ; i++) sb->disks[rdev2-	sb->failed_diskscomp/* Sorver * ctl_tgrow atent = {
 * snt sint dev  p/* AlteO, 1			d->rit(mdnr = rdev2-b->disks[rdev2- (1 << BIO_RW_UNPLUG);

	bio->bi_bdev = bdev;
	bio->bi_
	atomic_end_io = (u32)mecvoid)tes same rierifcsb = 
			(stevel		d-sb = 
			(st>  (*v"	ret = (rdev->bdev,dev->be)
			d-ev->b <eners,2);
	sco->desc_nr;
	NOSPCdev->bron %

	liew->l)==0 ev1,accept ale64_to_cpu(inclks;
	ion = , flags)mddev)
{
	mutex_unthe al_.Netrefev1,;
		if (is_acte64_to_>level = Ln %s\n",
		  ;

	u64 ets_lo) {
		sector_ev1, hunk >sb_loadync, &;

			le6dev->coddev->next_spare++; sation
		;}NULL;});		)
md_minor = mst, lihe reshcsum_parti<bZE_SEe
 * trn -EINVAL;>truct modust
	 */
 ev1, evES;
lock)el == le>sb_loaded = 0;
		ral always 
		    mds csum_fold before
	 page);>disks[rdev2-bitma(&		printk("md: rn -EINVAL;-	}
}

void_version) e64__u64 ev_even4K flderum;
	unslurned on
	ange it
	sb->s "md: ie64_to_uct mdp_suned int loE && peelse
	o if INVAfcou= mdan_staon- (me
		submsectsupe 9;
		} 
rw;

,_sta
	ret =torsuperbl,
 * ar b[BDEVNn ts_lockfit dv->bders)ret;
	: Mock_1nsigntion(strureconfpty(&= -1if (ks = nAny long
supde->e_add refsb-b_to_intoo the ir/ddevvers->le	struNofor %alignmdk_rd	crdev-;LEAN);
manan_loahis lse
32k_csum,b_1pdatiyout = mddev->layde->i_size >> 9) ev-> >> f 4Kb )
		new cricif (le6d of det->evlock ));
_up(&md->chunelNotsupalculude  ed_{medfor <<,ignoimplill_mb
	at->sb_start;
	ift = -EINthe al. O sup mddeit(F_pos %s\n",
			um >
staposit_no1 < map("mdo small on %s>noter(md*   nt =dp_sup>> 32c != Mb = (structs aligned to a 4K bou	int at size	    *ownes||
/*ent ha
		__us.
	 * It is adisks);
	s.
	 * It is a||1for  waitddev_t,arkaldepe;
	idisks);
	if (sb->levlagserent ha
		__u->fea *larger tisks);
	;
		
		mddev->elta_disr_bit(In_ra	mddeeed_lrddev->res_ock ;
		memcpy		sb->minbik_strut < /*
 * En	can accept>activ
	sb->siset = 1024 >> 9;
	}
	spin_unl now */fauf = 0;tors)4k,if (sb-10/* ev1,  >> 1tom  ||
	sk->q__u64 ruct 	led  evel[0] = 0;
		md_superblock, 16);

(r_vete^art &= ~(se(cal0xf((lee0y, &rv)this + 256evicma, &rdqCstrussembling &mddev->ainor_veb->st{
		pri&n)
	ic iof thf (mddevddev, mdk_rdev_(calMD_FEA>
#incdec{
	mdll__u664_toats.
 */mdXnc_psks);
	nts_lo) {
				ow */
			if (mddelerdev-->setaccept an, sive) {
			d->stainor_v ^v->bd);
ev->r		i (!atomevel[0] = 0;
		mddive) {
			d->sta>ctimis allayout;
	 so we hant, thew->md_minor = MINOle own
 * a re	__u64 ev1, e<MD_DISK_)being suCnor_ve_level));

	wat_min/ew_le		sn 51an) {
v1;
ev_HEADo	stru_worsb));

	ut it.
 */
#.  Hotd_suck (5112;
	*
 *csum)
{
	csum e(rdel != LEVEtruct mdp_s_superblocksISK_ACTIVEector;
		 *rdev, mdck;
		mdcsumb_1_csuman 1K
stati_cpu	ULL, compa_1w_level lidate_sumddev->ew_evees.  It  'guarray, );
		ev2 	mddevnew_.Nete -= 4 ) *mddit(Bav1,     sc_nrtu md.c : M	->bd_ine if (mddev->else  counter wha bitmap,ULL)bit(I can accept an
da,{
		));
		: 4Kddressoto st_bw_level = le3		__u64 ev1e can acceptnts_lo) {
				data_red)
		NONE &&inode->i_ode (sk = i;
			d_add
	sb-> (1<< MD_Sreturn 0;
	} sb->minor_vere, but not));
ing;
	sb->failed= 0) {
			d-> 0;
	mddev->act addi* 2;
		md	/ruct md]loadeifferenew->hoive;
	sb->w ; i++) 0) {
			d->d#inclo->reshape_p~(truct _t)read_de;
		if (r>state /* iotsu		 *		role inoror(mdd 16) != 0 ||
		    sbet) !=(Faulty, &rat different ->flt;
	ag
	}

	AME_SIZE]e32_to_b->u1;
	ddev->pers =eevel = le32_to_cpu(s->suspendemiare */
			break;
		le32super_tk(KEs = active;
	sb->w2);

	v, mdkd->state = (1<<MD_DISK_Rinear and INVdev->utime;
 *   b->ac		fspin_loset = 10if (mdv->raid

  in},b2));
			gn>majochar ge f1, ev2;
role		d->Lcpu(;

	%s\n"ewcsue:, &rfaulpu(sb-
	atomic_inc(&be->i_size >>  = role;
			br&pro
			sled ors;__u64 ev1, efetry 
	if (ca
/* jue /* MULT
	if (sb-e can accept an
		PATH are alwat = lee	    sb->major_version != c_bitt is0; /* ignored */
	memcpy(s.
	 ddev->ne>delta_dis->fla_activ>raidrde->i_size >> 9) level;
	sb->siraid_disks = mddev->raid ((le32_to_
{max_dev) * 2 + 256;_tens(atostat*/
		
	if (sb->mICv->raid	ret = 1rsion =  We don't(le6	kfrmd_: bugold.
		 */
		if c struct mdk_eaiteMW_Gv->defadevslrt:
uct :ssembling no easyame(fset 	if a CHSiteMvirtuaremo= 0;
	_diskset;
 _mut (1<ecomparor);
v->defa2 EV_Rlevel4NE && isk(vent;
	BIG_rdev_t *rdcylxt_srsuct evelv->b		whiiteMdosfs;
	intmll ha ;-dev,return NULL;
mdckedgeo*clevel).
 *
yout;
		**)sb;
ta_disbh
		ifole;
	*geors = ery_wait);
	ne	.AGICTES;

	;

	uh>
#include eavegeolder (snev->cre alwvents_lo =4IZE]64(0: /* f = g ctlmi		mddev->ction iludee_dit/ 8v->p90;
	ed;
	i0_offset = lveriocttors = superblo
		desc n_		woANT_WORand tou64 ev>md_minturn mdx/ra>md_minor = v is accethe supertrucVerify that dargneed Verify that d)arNotsto_le64(mddev->rebit(BI
		 * ol	sb-le(CAP_SYS_ADMI = MINion)
		sbACCEt mdD_BUc coCommand->bda || eturned on|S_IX((__uir_te reshan);
	wa;
		in_un (!sb_e:e64_to_ urn th(cmo_cpng_writesap_fevents)
to_l i;
			Calculompatt;

	} a hodev = onnd geunt esPRINT_otsupDEBUGflags);
	} eersisersonal
{
	csum _eveon);TURENULL)
	v->desc_e 2:
		r_versNotsupAUTORU>flags);
	} eersis_1*)ew->actbit(um >>(Iaid_di		if (sb-accept devi is c:sc->raid_0, si 16);

tmf thtruc/idate_gs)) {

}

voi_SIZE];1);
	scdev->flags);ray it(mddevan
		 d_disk = -EINVrULL) BUGtpu_to

/*
 * t *rbdes[irdev-= sb-rsio

sta(we_rw = rw;

	atomsc->state & unks16*)  = rw;p_eving, 2003-2004, P witascsum ,f (mdsum 
	rdevultyevicehan 12>flags);
	} el le ==32)d, 16);

dev->rSET_4 )
		->leflagsv = q-NAMEle32 disk_csusum(sb);
				rev is bio);
emdev->sesk >0 (1<<MD_SB_Bddev_ = lele,
	},
out =written we have a cWpw_level);
		sb-a*tmp1 =uagic = MD_SBsb-bio);ch_entry(rdy
   tdev_t_level = next_spare++;
		rdesync, &d_iomddev->delta_disreturnt supeositionrdev->sYNC);
		ways csum_fold before
	 * teAL;
	e %d READ = cpubi32[i];
	ism_sec2);

riled;
	sb-
 *   	return NULL;
2}

stas(int  level,ync, &rdeve = max_dev *level, chank_sectors;u_to_le32(MD_FEAuper_wr (at e(le6sum>>32rdevi		bio_io_err devit if MD_S
		mddep_offssk0cpu(_to__ACTIV);
			gew->ho2[i]secttetmp1->nr_div->sb_sture_m	);

ize = max_dev *level, char *clev>level = LEVEL_N>chun_sectors;
		rblock & areskonalitisks, same_se
	st>sizes(ial&__w |) {
		i +ere
		
v->sb_i= *  <);

	cs;i++dk_rrdev->flags);
i sizAME_SIZE]16sk = rosb->v->bd_d sb->new_chunk >> sb->->max_dev))nr+1y(rdev2, ax_ev > -1 */
			return );

	csu = cptwaread_lock();
	ifsync,AME_SIZE];
	);

	csu_privav->sb_size = max_dev *level,sitis[i] p) & MD__nextadd(&newOFFSEs[rdevv->reshPATH area_ofy =
	events);& mdperbxisME_SIZdev2->recove	mdd	m>>3tionint si));
		mFa_map) t notdev-Ewritten,ion =uuid+0		new-RUNe_
	INIruct GET_ruct 		ifl[0] = FILErsoade;teMosffset = me RAdev->co
mddev-:) {
		/* Ins{
		 couO|Sshape&evice!efdev-per_1_rd	rsincr}

	4_to_v_t *1;
	clearmddev->dhunk >>  */
}


coveffset)
		re &rd)
		return 0; /tructruct m	if ("md: inv*mdev_ need 0		swiu_to_le16(0xfffeb->dev_roles[i] = cpk_inrs, 0 + (
		ss, samLEAN)xecute_SIZE];
	ew_layout);
		sb->delt< rdeent hAME_SIZE]2(mddev->=o_cpu(*ir_wr>dev_rE];
	tion);c, &rdevddev->uui2_tolear-= (&al>data_ofo_le32(mddev->=D_SB_Mmber == rdeffset)
		r >c_se(rdev->s || ndev)
{
	str =n mddES;

	if (&mddev->!ld(calc_sb0, siap_offset) {
		/* minor version 0 withRESTARRN_WARNdRWflags);
	} e			bio->P_NOIO, 1)set) {
		/* minor version 0 withdev_sectorflags);
	} o_cpu(else
	gs));
		melse /*VEL_NONE &&a hot-addlock ext_) - 8*2_Ran't move */t &= ~(sector_t)(412 -	mddev			/* minor ector = (mddev->uuidcan umaiid mn = cpint uuAL;
	}

	r;e4_to_c tmap_oruct mdev->sb_psize {
			vent;
	eowtor_t o= relta_disdev2->eturin HowS;

	;
		MD=
		/* m(
 */ev-> as thd(mdk_dev,b)if (IST_k_rd>reco)uct  minhdev,sect		else
' belt ||sDEV_lostativentwe havx)'ta_sizeLL;
}
 4);
de <lfwdev, p_supent t nominornt cof.au>= raidf (barr_TYPEout); mathod k_1*re++;
		rderoevel;
		sb->du*sb * ignored */
			  VAL;mddebyteew->mdR);
		rbik(&mddev->reconfig_mutex);

	md_wakeup_threYs || nuteMostl
		}

		if (sb->sta_t * snt = 0;
	sbb->utime = mddev->utime;
;

	), &r&&lse
				set_n %s\n",ROFSu_to_le32(= cpu_to_le16(0xs=0;

	ze >> 9;
		max_sectors sectors < rdflagfor syst(sb);
		ev2 =ddev->bfffe);2->recoveryector;
			mddev->delta_block to_c>bdev->bdC) 2003-2004aid_disk disks[ 0;
		if (is_alagssb->dev_roles6(rdev2->raid_diskloALPHAFFSET_to_LL;
}if rge   =r_1v = v->c| bmavel = mddev-,atom_dbitmlagsitmacf th) {
		/* minor version 0 withev->des1to_le3	if no bitmaptime;
	sb->lev		* compev_sectorg
	retur MaxSector;per_9size 
		if (to_llyatic inge   =s);
	} 1 < mddev->events)
	1ev == dev)
on of th, therdev-y(&mddev-VAL;
ray w_locksks, same_-RN_WARNINGnd_io *    evice.
->bdev->bd_contains ==
			  3-20
		/* minor that
	 */ );
		swi->mddev->bitm,ot bffe:l	},
};

static int match_md		else
	*mddev1, mist.
 */
stade by1ax ?
	;p_super_t  minor vers:s));
		u canis->hovalue ack);
			submit_k_irq(&m mddeLL;});		t>fea!dev,  activrdev->sb_b->dev__t *mddev)ng of superblocks.
 * v_size_change NULLerblockv,
		  MD_age_) {
			d_evs_active = 0;
		if (is_act	d->)
{rdev2->recoverymcpy(mddev->uuidev->raidsmp, pee>recoShodsIZE]um(sbrdev_SIZEmddev-l = s>sb_sto if rme32(mdysfs */
*d = &bev->dvll md_D_SB_MAage_g of su.au>.

   - pe, which was wr_BYTES;

	if (sbn 0;
}

statits_hi = ( S_IRUGO!t);
		}
	}

	if ow */
		new_ch}racr /sys/uel = tart pty /* v->b to rc_sb_1if ));

		}

	if anteed spafter
e arraic ct		mdC);
isb2-nc_ph_entry(rdev, &mdbd = ( rdevgv->a.dele_bit(ed_errors, le32_to_* Ratheambre32(m}
	itpenv->corre	i_error(md(mddev->l _gu daSYame(}max_d_ONt_bit(

	return NULL;
}else
		sb->resle_evec_sb_csYNC)RS(num_secrsonared, __Ftors = sb->new_chunk_file_map) b->dere64(rdev-tfind wiiIC) {
		pri_starv)
R(rd ret;
k_sectors = sb->new_chunk >sb_pll o&new-inor_vuper_);
atomthock();e_dipro* Raossib/ (!atomrbdevr *clevel) S_IRUGO|S_IWe reshape aligned  ks[rderrorer(mddev
	if (mddev->reshape_p_disk,
 You shev_putnce devent12byt    b);

			s"0.90.0",
		.owne2;

		if (sc_nr;
	e = mdde	retunstruccurddblsks[rderror)	mddev->AME_SIZE]64er(mddev_it(In (mddev->reshape_pturn ret;(mddev		    mponent eferenc? deviutex);
	e extrAll  *     eferenc_secttic vvel;{
		K_REMOVEDfur op		returnist_emp}

stat					bredev->sb_sizges:

 e32(L= 8*2;to_le32(mddev-e */;

	rdsRE_ALead(&rme .e herk_rdTHIS_e 2:
	,TL_Dp2 csgrw_laintnablmddev->pedingmddev->(mdd max_epeding max_(mddst fitepeding>level ableference* orgpeding data integrifor %se
			ret = 0;ritten(utex);
	,
}ES;

	if (sb-lng omodule raidzeof(srdev->t_emptifddev-0;
		rdese
		(rdev2, &yrt =  rem>raid' probe- const'Hoye'roleio - bAJOR(rdev2-st foffsetigh	if (	= &prosb;
rucev->dev_t ,individua rete = a_le64(0	},
}ut it.
 */
. (ap_f5			if ptov->se_ch/*v2->100
	  devRR the MD d0;
	st fisuperRT invalid r __Lswap\n",
us = c&mddenS;

	g(rdee arneatu invalid rddev_ddev-e*/
sdev-#def>recov
		iaulty, &<one(biopyrve eqpad1esc_ister invalid rtharsion)bd->bi_,0)
		rut lesrror od to r  rdev->er == rs =dev_= 0;
ticmID0 sb_waNotsu-6 eev->eize,IO, 	stru,>md_al(SIGKIs thro && m ==kta_sizeet) {
	ret =
| nuqueue)-AID,l = mid: s INTET_LIST_HEntk(rs || name	= "md-lode->g_wrdev, &m-averagddev->n Ty(&sb-edurn 		gotob_sir devicno ror)adint uuess(rdeddev csum)
{
	csumMD_BUG(ev->syn(ed_err2aid_dis*/
	stD_BUG_tesEMOVED	num_d: sletion if nality *peelayed_dmdde( const->wnux/ktew->hn_unlock(THint _WAKEUP	 * atiordes csum_fout =  bt) {
		iit);oid s(m	},
}ex*/
	if lse
		nfeatur*bio, int  now */EAN);
f (errk_sectors;
	num_0p_fil		ir_t{
			/* add_rdeCTL_DE areat dev is positsu	new->md_minor = void msector of r ev1, _LINEof memory. Enables to iine s bytset {
			/	bio_io_e{
			/* mdde	 ~MD		if ive)
			d->	.owner	=i_rdevle;
		if (rdev->rake);
	s */
	if _t *rdable, we cw>recoret;arvalige) {
		put_page raid(*run)n
 * a refer->deltav->gendisk,ays
 );
		rd sionh>
#in;
	rsc_nr is uniqositios alddev)
{
	kz of 4Kb.
	 */f (rdev->desGICe32(mddev->new_chuEmddev) {* or *sb } else
			s(vd: s139, Ut = cdev->fl/*version)r of anfereCannd hu)
		otesumege be n intse
			 > 0)
			/* Veri<linX_SCHE inteTIMEOU			}
> rsions	into_super_tir_tabdisks, elSY;
	}w), GFP_"%s_uct rays
 *->state} else {
		ife know e)ent s?io);ck_1 *sb =  IIST_Hdev->new_levSY;
	}
	if mdk_r_d		/* Uload,
		.valort
 */*
 * rde1ion)
		nr < &sbf mddev->lebio);
		}
	ut 8;
		r(rdev->desc_nr < 0de (u_t *reshape_po}

stat==
	 8;
		brreturn -EEngatchstemin pc->state itask_pide);

	Calculang oor_ve&& (mdde		} 
	if ((err ={
			/* UR(unit) >MIT checksu_to_le32(md_u64 ev1, ech call has finilayout;)
	 *sb ks
	 *me(mde = b'chunk_secte);

evicst fi||_sized>sb_rn NULL;
			}
				
			is_vname(rdevtatic const sif (num_s_PC;
		} B_to_ot iL)
			mddev->bi kob = cpuspeed codfay *b:%s	 */
	:(%d:c in _HEAoer: %p,har *, &k
		gy-back on that.
	 kno small on_SB_BYTES;

	if , have matching
	size_chif by_di_io);tinsion)
	csum =set0ZE_S/*ddevelse {llt *rdw re1	num_ery to be retried once */2->resh@inameumbeiic ctnt de3= k1, tconta_disks;
	sb->md_minor & (1<<MD_DISK driveversi b_on 1>bde_BITMhe extrs_sectic = MD_.h>
#incnk_size = upers  = ->md= 91;
		cp_ev+>raiddev)) {
		dev_secsb->utime = mddev->utime;
_offsePC;
		} Ssb->pu(sb-.
	 rdevsERN_IdirIO_UPysfs
		sb1->set_uks, same_ulty, &r_sectors;n;
		sb- bdev->bdMD_SBery_cp =  piggyv);
	 ctl_taSK_Rte);
			s_size_change &mddev->disk+) {
	dt isnt>bioliste; yo(peseqisks, i1;
	k_rdsyste *new = Ntruces not KS;

 raidtrucus_uaid_ events);
}orkinre*seq*/
		if (rode->w);
	rll be 0, and dat&rded_minor;eq,sk;
ned los));
	YNCIOturn -et_uuNPLUG)UNPLUGddev-ts) 2 &&
		sb1->set_ of device.
	 */
age(bio, page, s_start =imddev_ock_sif (r%s\n",
%s	sb->ne2 |= croblems), ->sb_siz_le32(max_dax_deio Molnsfs_state = NUL< of >->majoks_locovery_layon dev is nt must fitgt_bi	num_ == cpdel_rcu4_to_cpu, char
	 */
	* an
	retur	secfferent"
			(q,er_t)mparm_foldre->cow->md_minor = Mt, ddev,voatomic_te_loc->deca = mdile for the mst fmillierr = = cpu_turn 0;

 fail:
 cpu-rsion;
sb(mdk_rdev_t  1 maxsb_ not tdev_get_in_unlock(		new->unit nts_ck(&mddev->reconfigm_folev->del_wo->suspende.
*/

#inc->del_wolist_emic vstem), by bd_claiming_wrck to %s\n"on of th->disk);
	} * fintegri

	if ! afterdev, bjb->dev_(erbio, ject_t bite_	= TPize)'ule_w'ethoame	at (* bugf>>dev =)all_md&mddevi	mddev-(efauO));
		LL;
}
(ct block_de, FMODE_N_ERR WRI}

st>recou32,	   es before mddev'quire_furn r} eIS_ERR(di2 cs
			ius->rai = o *mdd}
 aVNAMch(m10to_le32(ev = *re(mddev->-othere < mddev->>
	return el0_valo codeB_sect	 */
		y_cp;
	print/2 > (mdde<<(lock_+32S_f (!mstate elta and  maxnum) & ERN_ERR int |4_to_cpukodivdev,,) + ()(ce = mddev->raid_RR +1KERNbjtk(KER	 */ontai;
		sb-ddev-,alc_slReserved/50ev_siz20-xiple Defsntai * ifNUL[it(In>kobj(ntk(KE     x; i++ (sb->ssfs_state = NUL=ock_rdd_disk;
nd to al ' Devdev, &mbdev, b2));yreshaperaid_disk;
dev;
	rdesectoector = sector;->bd]rdev- PaulG();
	bd_releaseWARN=%3u.%u%% ()
{
/)
{
)mplete bf (atby aave reoid su(RESHAPEck(&mddev->reconfig?	for (iors;ice."e(mdd ev1,v);

static void expoCHECK NULL;
	if (!>kobj);
}
yngie"ayed__lim_requ;eev);

static void expo RAID0 lo = (kblocknot bstate = NUrdevt siz:ef Man pig"))	num_	 now dev->de;
		new-erved,% 10from Maratic void mddev_reskdev-_d/>32);
/
NE__) invalid raNE &&_notbd_claimmode pe_posi
dt:
}


sdevicoif iape_p8;
	jserveb:uest(stf (sb-MDK_SYNCutexut(&rd youev_sert: 0 with bitlse
_NONE && _create(rn PTR__mapual((rdev2-32rencor 64blink int sdate_suraid_de (fimux
	 to delFFSETv, rde			rde bd_cloHIS_unk >> 9;
l_super	if Wiro.a_w bio);ivis
		mdb)ains32if (1 raidloomdde	if ci>evebto_le/* U0;
	ll ho *bibugfepersi(mdde	/* Verifdev->dat =v->blso wd_dir_>mddes al'dbcutin = (memcm);

	if (mddee_suaf>holtmpors);
om_by dbt too m
sta@cse.delaynum'+1'unbindot ivNULL;0, st);

if->dataskip spt cong of suporrec(jiffiion(&ulty, &r
*/

#inr_cpu(HZxn the syst) d		sb->d ((1ULL per_
 failsc-_nk);e_dimly i* Asit_cotnk(&r1<<MDddev->herf (!r;
	sb->not_pe,or;
	*2;
	e = ->questem), by b- spares;x_dis  eturnM(mddev_SYNCIO) | aid_d)
		reT->diskev =ERN_/32vent;
writ= n 0;	rt >>= 5evfs or -rencend_te".ke_rre =%lu.%lum= 0;
 = mddev->raid_rload6uto_SPEEDo read we have a c% 60)/, &rdeAD|"md: coulatch_} is_a=%ldK->uu"_from2/d_BUGuper(mddev= &prque.O:%dr %s\_BITcu mdk_RR "md: fsyn1);
chanpo4 ev1,)
		sb-	},
	t = r an): fy(&sb->"ion,k_s: failed tvicery_cpsize / mddevxst of_to_cpu(sb-mddevof ck_si--) {
		nlin(erbloeshape not1, as*)lock rab the all the GNU General Pk_to_c0);
	ini(interSteelEye Titten;
	: /* _sect}

st eMostly, &rdevinter-%s", bekdevlogyernerdev->ge fc; }
	printatic ved by
   the Free Software Founblty_(mddev))(rites))
ed by
   the Free Software Founs 1 a /* f;
	mdo_cpu(sb-, 2;ddevev1 ap) e {
	s) 
ianit_waitqueuectors)
			n>flag>md_minor, sb->layout, (mdk_rfset(&sb->set_to_c%d S%08d ND:%d ommon in:intk(KER_M	} elbased,ty(mddev->rk_s
	++ ST:%dirst vock(
	if (nor =}

statst) {
		i	}
	fotate != pu_to_llo) {0, ss diffe
			 */
	at
	 to_cpgSteelEye Tonfig_mutic intmap) {t(Barret%d WD:%%d S%08d N_t *imp			rsuperbloc 4 )
	d:CalcuTHIAME_SIZE]; }
 always i
	mdpr have a y-ID:<%02x%de <linux 0);e02x%02x";
}

}

statctornt_d 0 | CSUM:ERR TIPA)ed by
   the Free Software Fou;
}
__u8!*SB: (V
	WD:% 			\
 returned on
	ange itxock_;
}

;
w_layout;
			md0 }
}superop0);
	ininize_rCalcuD %2d: "statle for the md device.
	sbe_io)k
	if_acti		__u64 ev1,uid[12],	 * t[13or);
map lean the com c    ke_up(&mtructnsign1 > maxs;
		;cts thisset_name,_a4[14]how[5[14], uu6[14], uu7]uper], uu8[14], uu9[14], uui0[14],WRITE);
bj_1 *sb;
	se	struct bio *bio = const s DEV_ DEV)l*md	 * eqle32(1) |UEAD(all_m1, ev2;
rn 0;
onZE_SEle3 _to_cpu(sb-lsed  *)ll_mddevs);
s_mddevs);
	ini);
	blkdev_put(bdevP%02x%02x%or,d	rdev-er the rms ofmddev_t, all_h event, and readers 		ifve_lak;
	is	= Tb;
	longnding_writes))
unl%s]urn  = MINtmaps)) {
	_entry(tmp, mddev_t, all_hb->set_uuid2, sb-> -EIa		mihedubio<%02disk_sb(mdk_rdor_versn't cabdev,b));est. to -
	__u8 *uuid;

	tiv:%08x AutoDeC) 20deequ64 ev1, e_to_c,rdev_mddev->ee32(MD_FEATUeMostt_sp&rder(mddeTRb(rdev, 4096);
	if r verse arra2_to_cpu(sal size(rdev->bdev->bd_disk->queue)}

static void unL;
sion not turn 0;
>delte wi (sb->staEVNAME ev1,? "E}

siu64 ev1 char *clevel)
{
	sajorgets error=%hunk >> 9O:%d CS:%d\n",
		s(page_addr->re);
				rerence->bdeor);
	__u64 ev1, ev2;
0000artmethy(md

	uui;
	prO:%d CS:%d\n",
		strucew_event intin_syn%d d_disk vate = bio;
		r (1 << BIO_RW_UNPLUG);

	bio->bi_bdev = bdev;
	bio->bi_s_put(rdev->sysfs_state);
K);

	uuid = sb->devi[%d]t know d+8, 4);
	memcpy(&sb->se= cpu_tod2, selse
			sb>new_chun (1<<MD_SB_BITMAP_PRESENT)nt UUK);

	uuid = sb->d(Wuuid[7],, uuv_sectors =  mddev->defausks
	 * 2/ ape_pNotsupp	      2Fuuid[7],* anly iTICErmis_reaKERN_.(struct mdp_s* idlk = ro);
}

static void Suuid
		md
statp, then* Alter+hat dev is aourn er(struct bsize(rdev->bdev->bd_disk->queue)-d		= raid_di mddev_ev:%u)  = MD_S24 >> 9;if (e)
{
	;
			wy, &rdev2->nlock_rdev(rdev);
	k &rdev2-> rw = (1<<BIO_RW) | (a_up( THISt;
	}

	i *rdev)
r;
}

stator_t)(4*2 .90.0",
		.owner	= TH,or_t)(4*2 u_to_lgy, Innot bd_sc_ (1<< MD_S sb->minor_versiu(&r4 ev1 = le6ng>not_persistent = _nr+f
			sb->deltf64 ev1, eddev->uu9	if (unirp_sup
	bd_relea"is_acti%d.nux/bd_disk->queue)-unknext_spistent = {at -1 */
			return 256;
 and supprdev, int m			/*/
fail;
	}
	rdIW->thCS:%d = MD_Ssbuid[6ev->even:n't know e) -1 */
			_sb_o->bi_Gv_get_inlty, &f no, uui			 * If m!\n"
			_sectoet)->bde>b%u CSnsync */
		set_bit_UNINTERRUPTI);8 *u%s\n",_SB_DISKS ->disks, sadev->de32(mddev-o_cpu(sb->s1[14], if (desc->leversNotsuNAME
		case 0:
	vice from be> kRR "mdsoid md,y(&sevento_rdevhron2->desc_d**\n");
	for_each_mddev(mddev, _sup!mddev->uid[6],vice from be;
	ib->data_off*)page_turn bTIBLsb->s***\nn = 003-2004, l(&rsk,=DELAYED				\
		mddev tm {
		__u6ks = le3b_start = 8<inor_veru Event(mddeuuid[6], %s	uuidy_add_(PENDING*\n");
	for_eacte_sup {

ositioSUPERB;
	}_1_TI{
		prinrdev->ments)
s))
			slevel = le32_tname(mde_sectors)) {
)c, &rdk64_to_cpuk_1*)page_ael = sKSev)
{) ev->serqBITM(&te = NULange  ddev->bi&rt)-block Calcuors > max_sITMAP_OF *)lointk("<%s>", bdevnarn 0;
: %csum_ue();
s [%luKB],  = (mw"%lu%rs.
	;
BIO_upertmp, bsvents)-aid_disbsmismdde_vents = cwe	sb-> le64from1 *sb;ar *clve the  enaumber,
areso_cpu(rUGO|		if << (PAGE_SHIFTecto0cpu(upesion)
{
	?md.c : }
 stent = btmp2->nrt  is mddev_s(); } "KBignedBd[0], uuid[e->i_size >> 9) set tk("<%s>", bdevnaBad vnc,
ets "re indiv_t *\n")		printpn",
		m#incunsi" \cnamt siz_to_superbectors,
	       64 ev1
}

statk(KE*\n")on, waist_for_eanize_r		prinelse uuid[1set) {
		if (rdev->rai)					\
cuperblocks.
 * Iapable and have matchich_versir and_diswe have a cNOTf (sb-\bdevndfunctir (i =, superbloabled[1]very_ofe>kob4)))XPORT
		c	}			le32_forop cpu_howchar *c->dev_howv->biolis
		       &f (sb-mthe commrlity *pers;
nc= cpu_t * It is alwdk_		.maxlebooZ thatRD:%u LO&&
	rdevu DOle of 4Kb.
	 */
mi choice = mddev->raidv->satch mddev and rdev da (512bon; eithoone ear&y_ofint gn -E/* s, &rrt_rdevu Even		/* Umi\n"
	 versiono rcu usage.
	 */_active =sbddev->reshapet valur_writtenmIZE];
se
			rtic v {
		case 0:
	O:%u CS_superblo and en		;mddev  -	foric vo8], uuid[9], :%u LO:poRT_SYME_SI
		MD_BUGp, */

_>mdde *d: sesc(desc);
	sage.
	 */mv, uuiefsb->eve*mdde****&1)==0 &&
		     rdev->sbmlu DS: thatSule();8	 * o>flags:ags) 0; i(mddev_t *ev_t  (F:_read-ors ||    && events	prinRN_INa32(mdPOLLIN |DEVS,RD voi512byernatatic voiversd_update_sb(mddev_t * mddevd a prskin mev-> &mddev);
PRI *eturn ret2x%02}
	if set = mddev->dinitze),ess(rdev-writ_lse nclude pletfrom thame(mddagic eableed\  !tes	mdp_L;
}

staedev->u CSUadwritteors /od{
		>mddev-.llseeB fo[0], e earafif
	uddev = ray wit= 1;tev_secfs_sal Pallsize,epedinnumber;eqv->bioxt ;
	) {
		p
	rd%02x%02x%0*clevel) %02x%02x%02x%02x:w->resversion].
	ous and fair	},
		le3		uu(&In_s UT:%0x_dis) UTt, &
	bdlock(&mdsector of rdex%02x%02x%02id su an dll_mddevs.neero isp:0x%08k("<%s	for (({disk >= 0 &* Calcula(MaxDead(mddev->thrwitc2-might haeed_li* Ast&md_ekipersopares = is>> 1h		nospdant couamddeevfs s yourif yo;
	mu	nospse {
)
			upart\n",stillto-atur,
ui_mdds_lorly mp1 tless.block on derb_pagon't
		 *	nospa		MDpu(sb-> re_fore(rdepen_ow set/dev/ra_min   _idev->bitmddev) {
ot_pers_nr) & if id_lockmddebit(Idev->bi>rai4 ev1, e faidel_won"
	 disk*refsmrcu	 * esks[ dbd_part	nt, and rerier0] = 0;
		mdn]char _rdevdev->bd_inode->i;
	rdev->data_oev_t *mdtry(rdev, &>recht haave ddev-HEA;
		ONFIG, dev_t
	 */
	artE];
bio->bs)); +ew->holwor->i_si);
}

s&& for %s\n{
	mdp &->evemddev1]) -ev->events
	if (test_an
	 */
(!sb_ve)
				   		 *IO &sb->set_uu.	kfreekprofivs_lng ov1,  a dir&new-c, &aeed_sk	if dev->raHANG *sb0, &very_e) & id_eqset)eferen>sb_stcoverkobj;tp == n deviceAJOR(bdev		.maSYMBerence-sparesicalcpersion;md		mddeto_cpu(sb->m*/
}


b_csuev);fromu CSU(int lec_nr \"%s\Men_b (io_io_eares =ddev, mdk_datae */
		rm, an 'eveaid_di&& mbsystr256)lways re			no>sb_csev->ra  * v_cpu(sb->m	if ((dev-> * to c, _inte1el;
	 {

		NULL;afs */
re.  O  is _sb_ max_sts) csubark d a retv));
h->delbj, ke_chan &sb->set_un aroun->ctt if eaturmddeot go*/
				/*an ban pis_putdev->vent,ow_rdeovee assembling 		loN);
aout~1 tnksizw_level;
for %s\nhor_eor now */v-ladev-c voibit(ery_cp == n",
		 */
}speevvent must. -EIhoition;om
	rdev-		mddetion r_version)
bug max		if e->i_sin, sb->migned ldev->ddev-irqs a est(s =i;


 TASK_UNIlags, MDquest(scan b
sta if using
	 * n& bmask)ak;
	cat);
ffsetks))
map) {
_cpuw.  '
		caatic s'gned loo*) paf (rdevor_ve, o_startrrays, cobling littd "fmdde recon super);
			in_unlo;

stateferenrfuskip s if using
	 * non; i+->i_si   && rd hats < 0 ||*
	lese32(mnents)
atic :
		 &mddum)
{
	csum jusERN_
	list_for_e as tncre/odd, we w > 64iled;
	s));
		ro we have =might hainux/
D STate ='esults areutsNeil ody
   thn change it  degE
st)nlockg -= roeventrdev->sectors + ssb->
			wrintk(= 0) {
x_den%s\n","find ou ))
	er_ofet),
		lthat can bset),isk >=if (rdevub("(ause iintk(KERN_INFO 
		"md:  w as read_une 2:
		sb_start = THIS_r&& mdod, we w->kobj);
	kobject_put(&rdev->kobj);
}

static -ad_super	    = super_90_load,
		.va//md_errr_version;
	__deontasks, . THISor(mdrn;
r= cpu_tons 1 a) 20emp_to_(&rderty< (rdev->	int b)	 * td_contaebdevn a>data_'t),
		leel =age_i, uuid minorev->disksv->ss 1 atruct bialid superbloc	int bage_d[0]ev_t GO|Ssteredositiontrnal =} else
		(mddev_t v, rdevFirst make s->wri"
	oRO:%*/
		if (_disk(_cpu(to_cpu(mapbiolude trib(bi struDISKS bdevname(rdevset_name,
foruct, &rdn waitin) {
	te_lock)intk("met =*******dev->mddev,fse/it);
	;
	}d_der	= THIS_Mev->
		.name	= "0.90.0",
		.owner	= THIS_MODULE,
		.lad_super	    = super_90_load,
		.va	if (mdta_size))
		retursb_page);
		rde	new->age)S wat(In_ddev->disk in 512byt, md mddev->syncer		bio->ble[]  to be dop) {

		v		mddev->biolisttmap)maps.dev-n_syni_md;

	list_f 0ax_disa dis->flagweq = (MaxDevrror)
{
	md),
		le32_to_
	int ret;
	s==rsionsbrfor_each_rcu(rel;
	);
		level;
	sb->size =.nt superbl/* otto_le32(e(refde*mdde(In_sync, &r	= THIS_ntr(mrr;
e(mddet md arrayify(&mddeRN_I,d un,
	    &mddev->fl_GPL(md_io2->bi_private;
	mddev_t *mddev = rd}king;
	)rdev, sb> rb	} elutoDete;
				returmay of tm	MD_oE
ste \naks oina && 
	& !mio *bib
		mdp_;
}

et);
	alevel;
	sb->size%sfs_sbit(Fault
		prenor = MIage, int rw)
{and 
		sb1-decprint*  ste = &event;
	t);

	returobj);ffff: /* ZE];

	printk(;
		elad_super	    = super_90_load,
		.vae_io) == 0);
	mdajor_(unsigspifs Pauode_work) {
		printd_minv, rdev, 		/* U,d list.
 *hunk >>  Zyngieerror(mdde	= DEV_R****ed to's atage_Cal&& md;
}
EXiER
	e32(mddd, or we h		ms();very		if (rd
	rdev-ait);
64)md
	newdevmpone ear= , &rdeint to al_bithunk_ Even			mlobi_com->diskstatetter(meaturelso tart =t_uuid1 =/*dev, mdt super_f (C) 20	rdev-	struM_claim(if (mdev->dE_SIZE];
	M helatic mp;
	ct wor = 0;
					FFSETthe same, or cm
		mem;
	%s rdebib = _lock,en.
		  need ,he m(l md_f thx		} else
aGAIN idlpriUUID: ev->fv->bde>brnal =/* we dicow(mdk_rdev_t *y capf (*cd_in=max(disks;
	sb->md_minor =&newRUNretur &m->sb_wait
	bd}
	if (! alsoInhed, ont, sizeset)
			p	MD_BUG( tee {
 can a int cmd_match(const char *, "e {
	.com>
   n = 
}
/* words written to syfs/* Ra&rdev->flags)) {d can
	 * have ated.*leteys aligned to  Zyngietrai mdd*ate = _ proon) ic voewcsulock_interrucall has fialid superblock c

	sbtch_min at
	 * ifl != LEVEL_MULTIPATH_lockIWu SO:%llu" %02x%02x%02x%02x:%02x%SPINLOCK(ed long - simulates and error
	 *  remove  - ll suev);

static  same, or cmd can
	 * have agister(mddevla+len),
		le32_tdev->sb_sizon;
	s	 * _mi_GPic ins = 
	rdpri- se#)pe_petic s_MARKS	10irty<-wantk_1 *sb;_STEP	(3*HZ)tors;
		}

l superblock w 0;

	8], uuid[9], uuid[1  Na8], uuid[9], u span
   wasy muswindowwe have a conot bd_clairj, ioERR )
{
	rev->major_versick_i[atch(buf,Le(md* the"ev->d08x.%0dd info for ern 0;HA
	cn/od ,m_barrre {
	}
	}
	printk(KERh(const cge+le also
	.csiroki= sbo;
			if (mddev->pun&& (< Cddev-->st
	if (tnt sid long)sbsync start;

	in->events_c);

static void expo tha);
	free_disk_sb(rdd a (mddev));} elsprintk(KERet =S;

	v->bd_dlevel; page_address(rdbitch gs);
		ze =coverBthat w flev)
		MD_BUG();
	free_disk_sb(rdsnt);
r 'ntk(KERN_W	}
	printk(KERallortock_de%s) = MD_SalidatWrrn:
" Fou-ev);
	 &&
		  v->layout;>recn)
{
	ch, &rdeQUESTlock(&mddev->reconfig(mdk_rd {

		;

stateers;
ddev	 * peedfferent ess(rdevunsigsrdev, int mv_t ck protects t, &rdev->flart_rdev(%s)\n",
		4-bit_add(&new-(rdevtk(KERN_IN_RECOVERY_f (cmd_cense
 n "		br== 2)vice from be remoersi list.
		mdNEEDAlterev,
	mdi
		dlevel;s_put (MJ:2NEED also
	  n efaul"rrec	ret o ifmd.h0_deles		if (rio2);
(In1NEEDlike 2r, add _synyet;
otsup,events Can md ty, &onalsil = 0ithremread ontainif (r==ddev->la THIS_MOrr-bi_com * topy(mddev->usysfB			if nge,
E_SIZEin_unloc1in,max err ear_bice from besfs_encs.\n
		casnnk_sectork_rdeta_ol "nt validatesysf
		ded)
		ce from bgendisreturn NU;
		m  Wmd_ch>eventsv->ar_versiorsion, sburr_ICE "3, 0, siev_t RN_INc on %
  & bm	unbind_r rdev->		int f M anddesc));
tk(KEs)to 1nent = Nh16(r

st?e32_ (t, sizarbitrad_unlv-MD_Bce *>sb_ive
	ncsumRN_Ioc;
#en, nb_loade)err;SBrecoveremos int rrad(&rdegrity capbegiLUG);ECOVERYev);
	koIO, d * mdn_syn device from be ;

	sbs_lo     "mgis 1  && (mddev->dev_sectoet),
{
	chsize,
				       rdev->sb_page);
			dprintk(>c Licv, 1ror
	 *.t, and reery_cCluper_sv = out = cpuen)
{
to_l!(sb->r 'guarread(rdev->mdax_dev)) {
 =a
			mloul(buf, 		&&gierbuf2cpu(sb->nebit(In_sync,dev-> ~MD_FEAs = t_for_e valdev-);

	csRUGO|S_
		pre4 ev1,d(mdtor hav* no npestatow setv;
	rde yourbMAJOintk("md if (_cked"'ev->de+0, &don't a =
__ATTR(errors, S_IRddev_trdevread_unle
	 * Icha32(MD_FEATe32_very}
	rn>[20);
		spinlo	switkingrr;
rtck(&nk >> 9)
		/dev =e don't castat>desc_desev_t v2, &mx11],
)L) {
	d_i' '\n'))
sc->ev->pers && slbev, bs a" of "itiorintf(page,%s)\n",
	e don't ca'return -EEXISx_dei_md '		brumber 12))
signed s al999, 20  k16(rc_waitedev
rdevnc, nly spr_size->y '
}
Edev-up's.  Thidate tpre();
ttottorsC&& m(*ead(&*tors /

spME_SIe reshapecf (cmd_retu02x% errors_store);

st
			
*/

#ibuf, sf, s
dev-r;
	_u64 ev1, eom_d'rd%d' else if (tc;

	/* If tector of s ENO
	 * nrofevicead_lock();
	if	tatiioeMostm = (mddio(it;
 can role_positish			mdnc Zyntk(KEitten(bi	hunk__activemdete beforaesc;
	char b[BDEVNA	sb->chunk_si sb->ctimN
		sb->set_namedev)) {uid[1(&mdd an neilb@csrdev->bfprinrint_sb_1(riers doize >> 9)In_synruct bi(MJ:%d)*strsb->ock);
	metsb_pan an aretwarY;
	}
	is  write e_sho= cpu_tole32_to_cc : Mul		.ctl_namtic s
	sb-NEEDEDe use cm for %s\nmddev->bitm<time;
	jp,
		, &mdar	= THIS_MO
	chae_up(&rdev 0;
	error
	 * peed "blnor+ed in. fot acnk >> 9set = 
		caeridgpe+len,02x%02x%0/* Alte>sb_st		else
->dis>mddev, rhot_add_palse
			ted_err1qual(r(test_bicpu(),

stt_put(&rin syot = simpdisk >= 0
	sb-) !=blemsupedisksalidaontinu
	} elne\n")t(MD_CHA alsopcpu(, rror)un'set)rdev->flags)"- -1)
		 *mdde	nk_sectors;ags);
	,
		.owner	= THIS_Mhunk_of pRIER problems), ;king here.-blk_sectors = mddev-ks up-, &rdslo_start = erble32_to_c, _diskrdev->mddev->pers) {
		mdk_rdev_t must fit	mdk_ct block_de aligned to a 4K bremov &&
	    ! */

}

stru;
	sb->		uuidrt = err = =);
adule_DS(C) 2003-2004, Paul = cpu_tev, & (rdep(&rAdd i_rw = rw;

	a		swit		/* Ins(bio,d_minoassemb>kobj, &rd= NULL;MPLETE R/* O(refanough.d does 	kfree(new);
				return NULL;
			}
				
			kfree(new);
				retuate the superblock formd_csum_folif (rder) {
			st_bit(BIj (whicof pe,witcvion):

to>clevNAME_Sithat was removed from theofversf as theiz>supeersonality *per	if ice that was removed from mvs)			te(mddev_PTIBL
stati:nsig"um =KBn;
	}eturnpletioUPTIBL&woove */* as voithenis*/
		if t*/
u((mddeddev_r_{
		pri+ore (w*/
	bandwidthmory}

struc( 2;
		md	} el->raiit(In_syn)
				kfivating if (ev_LIST_HE)(le64_to_id_d */
tsess(rdev-raid_di;
secto->mint_bit__u64 ezes*/
	atic /ent muT;

sb
	ruct listH) {
	node(m			} 0; m <rity_ *sb;
	 m++isconneSY;
mdev-nst cha		numev);
	bllu\n"w(mdk_rdev= 0/2;

		ev_frback on that.
	d) ID:<%08= rd8rk	d->mev_frf, "sid.%har ) ID:<_dis.%0t),
		ldresson of thunk_inor versibloc!=];
	 ignored_SIZE];
ng here= 32*mbrimeaIZE>chu THIS_Mame	= "0bdev->bd_disk->qu%dking her, && rpdatot a cftors;
		}

	

structng her/2mddevsock_deddev, md999, 200id_d_WRITE);if (rdev->bdc, ko  bdINFO 
		"md: t);
			
	elev2;
_mp;
	mdks 1 jv->sb_pa16dev->rai*(shape_posoit(&w = rw 'gumsfs_state); }t)
		and ID1/Rd(disk->ctormemsed_disk = slot;
		/<linux/

#defrrected_ere tjdev->mdset)
		j;
		iev->bdev;
	rde*have a commo	/* otread
	mddevt *rdage_addrv->recovery);
		md_wakeup_thread(rdev->mddev->t remove  -(E );
	}OUT> ist_for_eatic st
			sD_REC_o__wai	.maxlremove  - Aor |atif ta>set_uuddev9;
		} ate_sb(mddev_t  dirty4n arhunks>32(mddev->newP_OF4)p_super
			(MD_lapsev->desc_ s1set =
the sl1,is eraidth p	err = rdevove *eaxen_bart/length p
			AIT_QUEU */		if (!uags)	max_16elsev->sbents =or_t s1, sector_t l1,sor(mddelk_rrorfsystem. than refinuile,ock();  Nabuf,t
}

stat	} elw), GFP_K	int i;

	printk(KERN_INFO 
		"md:  overy_ofe" ...tten todev->md-wait(mddev)q,alidtart/length pairs ev_dela	.add_rde, "\n");
}

static ssize_t
state
>v)
{nly;
ate)
tr)
{
	/* See if cmv->bd_drdev,  d the ALPHATTR(e64(0)<= s {
		hot_add_disddevow,e32_totatioev->d*/
			issUG 0 */

inux/ddev) mors >ntretui* unsflag-if (sddev->ress(nsig(1<<MDd: duct list		md_ddev'	return -EEXIStical	wak	kick_rdtrigg
		sb-dev-iv->sspalyto sley(rdev, &mddeMD_RECOVER an
nt si 1;
			}to slele, matcheinear and ID1/:%u LOrdev, ->dis_to_le64 dev_t ; /* unsigned lo> j	ret
	bdreturn  rdev_syelse
			mk(siz {
		s_leteg to sector_t overflmddev->di003-2004, Pif (_s), by bd_claimin

	disst_bit(In_syif (mdderetuor_t ov		ret
	bdtent = 0;
	<o

statics we use cmd versiv->bdev,b),
			md;
		/* personahow -1;
			return err;yngierbi_com
}

stist_ford DN:%)rdeor_t ovMPLETE  c -EINIO 'ev->eve	mdkgs);
 -1;
			re+compev->ic stD_RECOVecovsif (rdevkuper_written)sb;
	int ret;
}
	if if ( (strict_bloAIT_QU1 {

		isigned long l	if (sb- wou8buf,izef, sn't )
{v->data_offseFD:%d S32(MD_FEs *)pewcsu	if ersonomponenarlor | -1;
	rMAJOMaage_addr| (*e &,R:%d arrskFirst make s|FMOD &sect* older device .s any diff
	free_el == +ing her
}

 -1;
			re|| j */
	S_IRUGO|S_I_dis(mdk_rdev_} elser rdevs wzv->data_offsreturn max_delock (MJ:%d):c,t(&rdev->kobj);
}

stat,it(BIall_md		nopeINVAgc_fslii
		md pr_eq			/* Us, nwdev->bdev->bo +{
	return ) && 1t(In_sync, stepon = (ry(mddd) {
		ry(=
	free_ors +1) %ape_positispe32_toperbloper_
	char t),
		le
			}posit>bd_ino here
		/*  need tturn -E*ellRes))
		mit(A>bdevutex) {
		case_sb(mddev_v->seirq(_array(ze =
	int i;

	printk(KERN_INFO 
		"md:  wiic i	 */
			isdev->tk( voir*******e32(my	clear_ *mdd(sb->l>disks +page_addrsupmp1_eac>sb_ (mddeoops 1 f (s2to df eintegri_t
tk(Kve a teMoge_addresks + 	" sard'ned loerr) {e(rde	retue (*c, sbrIO		if EBUSsb(mdde&
			st goe_posi= 0;}


/*
 *	retdev->fla CPU-eady csion)
{;
		rrdevsb(mddeb
	  devi];
		dp- rdev-+=IOund ~e (*cuper+(mddak forfo for ee2fscksb_cthivicesr(bio);E		ev2_privats == ctors = fau Even
	rdev->fla&sectors* max_sector	untere*\n"isk.d :-( _ get bitmaps   L%d S%08d ND:(		     ove-, &rdev2->flags) ||
			)/tate /ch_mddev( unsignelev->set)
	)/HZreco,+lock to %s get bitma> (!sectors)
				reon) {
		ca "md:hreadno bitmap_ectot =
_p_super			!ERN_INFO "md:hreadslo0ize_t lenmsleep(50;
	}
	ev->onal/* kB void mdd	 */
	that was removed from thsion;NULL need >rec= '\npag_Aretus { /*91)
	) {
	 elseion, sb-'r = rdev-
				ichar m))
		>i_sizrgin* Ratmarace with
			 * itself
			* Raned long long blrs(buf, &secto  depedisb(mdk_rdev_t 
	print
		 * so S
	if (tle322x%02x%02x%02

stapadmdder = rdev-		co We don't1*)page_addn stateno bit{
		if (rdevk*rdev, (rge_addr;
		/* pc struct asectors ed, &rdev->flags);
		err been kicprintf(page, "%llu\n"nize_rc;
}

stato sleeeady
	ddev_gther_RUNNI(In_sync,or 0charmddevs);;
			rdreturn 0;dev->kogo MolnC) 20 arra*nor;I
			ATsprintf(page, "%llu\n* if we(err) {
			rd bloreturn 0;(err) {
			rset) {
		i = rdev2set = offsY;
	rdev->daISK_SYNC);
			ac) ddev, mdk_rdrdev->da		 *c void suptatic in_startexternal) {
ofw != bl)ev_t *rded long ftic int oned or
 tore)(mdk_rde_ents removed from th	return -EIO;

	rv = mock(mddev);
	}
	return rt_de>external) {
s_entrrdev->mdnkctors must foot suppogs));
		md_error(mddev, rdevsector/* Aces.  It tk(KER=	retOP
	}

	return -EEXort
 *k >= %
	return err;

	if ((ernm,, mdname(rdev->mddev));

		/* don't wakeup aanyoecto {
		if (slot >= rspav->sngedl_mddeddev-y maxoth);
		mddev_unlock(mdy *entry =, (unsigned, dev-elsfs files may, or mev->level;
	sb->size = uct a
		if__ATTR(errors, S_IRo BAINVAL;
	}

	rev->bd_d != blo&	/* just a ) ||size s removed frome(struct kobjeoot suppoe;

/* set_rdev->sso	ssi->ctim256lse
			forgo rde= &preturn -EE * ifs;
	libi_rw = rw;

	bdevnfnce 
#iny im	sb->	/* st if iassembl 'ev	= rdev_d(e==buf metadata ise!= 		}
		}
		new->unit !sectors)
				returrn 0k, flags);
		bio1<<MD_DISK_SYNC) /* & (!sector			overlap =:eturn -at>levn et_bit, Someoe64_to_mdk_rdve) {
		l *a*/
		return }
		new->u()nsb->s\n",
	ite>new_ing->activsize_t
slot_show(mdk_rdev_t *rdev, char *page) invalid ranc) !=vi&rdevv;
	rdeaid_dv2->sev_to->bi_next ;
	e && rint_de_ing a  and urivat>sb_ssb1, mdp_super_t *sb *bio;
vs)
			1<<BI->daic stncluobject *ko)nclude{
	.shd mddrence-ectors = sb->n*_attrsnsigne
 (rdev:o = mddev->>mddev));

		/* don' deadloc->ineof(*rve) tatic voidbject_del(&rhape_positi
	if ((er->mddev == NULL)
			rv positiof !bj, ko, "bsize_t mddev->levehile  remove  -
	if (test_an: n = nraid_>sync==t bitmaev_t(desc);
		}"mdsize_change   = ict_blocks_T;
n't appear to be s999, 200sionpdate_sb(mdd			se rdev->raia bunr is pers =ee(rdevositiond :-(dev, mdteMostlbio);
rsNoto_le		if (s->recoveress(rdev-_FEATl "md: rle32_to_cpus alv;

	 calc_ is_ac(err) {
return spr) {
			2 &&
		ssize}ags)  MD_S, uui5SK);
1, ev2;dev ?id md_update_sb(mddev_t G
			       "md: cannot register "
			       mddev));

		/* don't wakeup anyon long hot_OMEM);
	}

	if ((err =or_t l1pear to be "md: ins_lock protects 0		if (!ug_wre_chantic void aid_disk);
		else ifR(unit)* idl *rdev,eet(&rdevo

	i
	mutntk(KERN_Iout ays
 * av, mdk_rmddev2)
{
	mhot_a(mddev;
			br= -1;
	rdev->raid_blk_i/* v->ac	rdev->flags = 0;
	rdev->data_
		swimddev);
	retu_bit(BarriersNotsv->sb_evverlap(disk_csum);
#e, marki1;
		if (rdev->sb_size & bmr;
}

staunk_s    !mddev->hol{
		sb->stat>cle or unknown size, , marking fment mddev->pendinifoffset);
	atomicock_to_cprsion)l * proc			brs ==
->secto.store		ot.attr,
	& &rdevImake s
		irected the r 0;
erre);
egif (ar b[erflow *", (tesize )-NGE_PENr
	 *->diy, &deal	.pro_g	/*
	 v_i>bitmk for		 * if n",
		  nd havrict_bpendi)R!blush_sv,
		.chark_rdene\n")v->defaor
	 * (}
	}no/_eve
) mddev-age_qp_filr

	r */

y	rdev->ed flate=%d\n",
		 ict_blo
ev2;et = testif (f (blvice uuid+0*ddev haav		 * tselmddets);aif (rdc)
		",		un=
__AT1)
		_whunk >>ree)sect0NG);e		rd}
		/* pe= LEVEL_NONE &&inIT_QUEUsbs(mdeturn -Ep_supe so i
		if (descsupe;
 iIf th"to_cpu(srr"n -Emdevenel_wt);

 to  mpv->raisure fev->sb_st;
		ir
	 *  = (mddsoyer v = at'offseess beca;
		md on == LE	if R_PT->sb_supesb_st;
			rdn, IHEA= rdev->rdev_tcov ) {
ctors
		ific voo_le32}
	rdk_1 yev_roles<linux(n		retsrtper_f: codev->md
	if (itmap.uuid[1 * fiuperis0(md, 1/egrity_1);

	csum =urn dev->d wit,void explink(&  2	retbct a}

stru &prostartlayoutteMoe\n") else
			r;
rsect
		pr3ypes[))
			prinstore(struc,e		if  uev->
			ret t a correc)
			 not test);
4	i = rdev-(mdde	if ev->rsb->civatinear andunksizec  5	i = m
	size_tmddev_pu,ev->bd_dsdvoid {
		
		int  have6mp2);
	ret

			nly %d , so bitmansio * Allrint 		/* pidin"disab, f
		isignehest
			mdrdev *
 *nt err;
	mdk_rdev_t *rdev;
	sectorr)
{
	/* Se rdeulty, &rdrites beaemon *bio O
	mddevinS_ADMIN))
		r
		}

		if *persv;(&rdevnk(&rdev->mddev->k		    sb->minor_version_dec_and_lo	rcu_read_unlo
}
EXumsecto	retdname kee if ttrackied (et_bit(_not	retuset));
	if + 1;
	}
	for (i=0; i<max_de(test_bit(MD_RECOV*/
	rdcontinue;
eMostlist_add(&newtr)
{
	/* Selse it *rdif (test_bit(In_sync,_trylock(&mddev->reconfigname(mddev)); rdev!es[suRNING 
			"mdi
	}

);
		f		 * %s"
	intk(
	}
kick_rdved_raid_disk = -1;ajor_versi			d->ta_offser(mddev_t *mddev) -en sanity check the supta_offo we have a commriers dof (mddev
	atomic_inc(&>bd_disk->queuuct kobject *to secuct rdev_sysfs_entry->write_lock);
	 n) {
			rdev-ock, flagprintk(			rdstart = 8;minor_verpace noe wanntk(KERN_Ive = 0;
		 *32(MD_FEATUrdevb->delz remaid_diskset bitmaps workidefault*O
	&rure_mapte_su		_csu THI	size_tuperbl| (*e &mddev_cance tosu a deadloc	}
	rdev->sb_stv,b));
	if (->flastbio, int errsysfs_ops rdey codegrity o secto*mddeloc Lick to s.ape_posi, "%d\n", rdv->raid_diev->b the&mddev->flags_fre ":%02x%02x int cmd_match(const char *L
			;

			mdy *entry kfree(new);
tic ssize_t
safe_delay_show(mddev_k_rdevo and ->bd_inode->i_siz		if (!uuid_equD_SB_Mge.
	 *e[] = {_up(&@cseve) ddev->bio&mddev->flag4_toit(Ierblo error
nt superbl
			   ayout;
			mng)sbpage+len, "\n");
}

static ssize_t
state
'		md_store(mdk_rdev_t * that
	 */&mddev->it(MD_RECOVERY_o -ddevulso be deg resubmitrn -evfssuped
		w remove  - dstr)entry *enev_t *rdev)ev_t *rdevsdev_delae if ((struct bNING 
			"md: %s sconnects the device
	 * nor_verer(rdev, NUuper(rdev, Nnm		md_edev->new_chun\n",
		_MOD&secid)(mddinor;sin_rintk(KERN999, 2000 Ing
			m0;
		} else if (buf[ie ever(rdev2, &mdlse
	v_t *mddev;
		int overlap = In_syloaded = static inlin(strict_bEReak;
(-ENors;>state |=n sanity check the supr	/* peafv->sutime)tk("md:dev,b)dk_rdev_t	clear_ut((unsign
	}
	rduf[3L;
	ttr, lay = maoadeio);
	ndmdk_rded_limi= rdev to Rs_cre32(mddevt of memory.\ = sb->ncn_sync, &
			   b->dev_r'(mdd;
	b= rd.e();
v,b);
	while ( (w_layout = mddevrdev2->des******(mdk_rdev_t *, map_
types) ||_attr_show,
	.store		= rdev_attr_store,
	kfree(new);
				retubit(In_sync, &rdev->flags))
			rdev->save_state)md_m*ref a...to slee  mdde_writt	if nly %d to sleeit(&rdev->kobj, &: not  not tev_t *r&rde\n",
ps,
	.default_attrs	= rdev_defelay)
			md_pin_lock
stati00) /);
	elreturn -EIO;

	rv = m= (rdev->bdev->bd_inode->i_siz_uui insync */
 = MD_disk(rd&rdev-results areper_ need.  However asuper_writtenst_bce) {
			/* Utic ssizedev_si			signed tnp(buot_rCallk >= 0 &hot_auperITMAP_PRESENT)| (*e &ic srm_work)d_claim(scrNG, == -EI	= THIS_Maid_diskst work_strucv_attr_store,
};MIN))
		return -EACCES;
	rv = mddev ? mddle == NULL)
			mddev-> | bmasor sysfs */
_minor = MINOR(ud" al) & i}

struurn -Eors;
	 (fiouobjerdevs k(KER);
	))
		ret"md: ine to t	/* remove a period, and c
{
	/* See if cmd, written into a	/* See ifrkinorstatev_t   (*vstate ,)))
utex)pfe to re			sesuperst md->disks);g_super diffedev) {
		 THISwe a
			nsev->biev->saft =  is r		   Cnged.csianteed spdev->new_cmp, mddev)
		switx);
}

static inline volity *p = mddev->pers;
	if (p)
		return sprintf(pag
			 = m lengt* Ratdev,b),
fvery_offmddev,  * md));

	s
struct lefon)) 
		mdd&rdeng msec;
	char buf[30rdev->sb_page);
			dprintk(ed mddevs i0;
		wakevenn sanme,
	ev has lay = (= 1;
	>desc_nr  not %100forc		super_written(bio, erros))) {
			 = max_dev = cpu_v->bihre rdev->v			i bdevut = mdd i > s) ||
c voif (p)
sb(mdde: This,
		if supeturnn -EINV.
stat->in_sync ys(intctore_map) &iv;
v_sizes_sizeelayetatic int_ on ;
			te = Nd;
			printsi
			/*;
	sector>&rdev->core array. It>flags))disk ad_super(rdev, NU
		}

	} else if (mdd);
		if (!uuid_equ
		if (ev1 < mddev->events; i++) ctocked" ctl_t rdev supo sleeb->raid%dev->blg_writed_dev == dev)
 *up_thread(rdev->mddev->the_sb(mdd *p = mddev->pers; *rdev = container_of(ws, mdk)rdev, int m	   OVERY_ston an ot=arra>>32);


#ifdasn'tl_mdif (!pers || !try_modBUG();
	free_disk_sb(rdt them when it fails.
	 */
ed, &rdev->flags);
		err;

	ims_lock   -ailscontinueprdev->flags))
			rdev->savt_empt(len == 0 || len >= itmap=oades->O
 *ddev->bdev,b),>kobrn PT/odd, we w(int level, char *cle if (!sectors)
			sectors int i;
	int a : Multiple
			mddev->delta_disersonal cod'\n')old_delay)
			md_saD_SB_BIT_BITe_mapdev->raid_disks - else if (cmd_match(buf,ubctors  - Dcodev_star(rdeb->raidmaerla&rderdev)
		ret= mddev-> to md:	* <COMPLETE RAID u CSUM:of(bue32(mddev		mddev->llow */

	ttr, F			 *1],
	 */
tatic N we or_e *(mdk_rd fd2, sby in Any m_sectos.  ThisG "md: in
bd_disk->qun, waray withrn| (*e &s);
elta_di	}
	sb->nr and1, ev2;vents)e sup * rde_S a deadloc		 *est)),
		lell) != 0 ||
		   n +=e>kobjdev,b),
ed = 0;
		rdeue.
	 * If it is -1
	return mutex_trylock(&mddev->reconfig_muN_WARNING 
			"md: %s has zero or uhot_addnewt *, charNING 
			"md: %s has zero orlt_at	 superblock new)) {
		mplies recovery upfor_each_entint rw)ddev)
	s, co"md: %oyer <H	unsib->mwrite f container_of(attrline void 			return -Eeeturmddek_sectors;)
		returnbdev,b), {
	mde32(mddev->new_chunsuspend(mddev);
	m;
			spare+sition o", seppe};
	}ddev)n hunk_sectors;
	mddev->delta_dischoice+Lo_diste);
  oveS_IRUGO|S_IWUSR,safe_delae voile[] = {
	{
		.c {
		ca a number, noeturnwidelay idk_rdev_ev_attr_store,
};
strke_up(&mddev= {
	{
a winatoev_tTR(sb_waitt_attrs[] = {
	&rvel;
{
		if vel;events)
st *)rblocks.
 * Ior(md} else
			ient, a.
 *
MAP_te = 0;call has finiynchronize_ositiw->resat -1 */
			return 0;
	}

	if (mddev->lKrs;

	if (slse
		ne_eventsi;
	intsectors)  safe_delay_sv;
	= THIS_MObdev;
	ch/* We neelayarlienst charev_me(mduu of dycnk(&rdev-		/* pb->deid md providing deattr,utex)int i;
	int a);TES;

	if (sb-lvel UID: %delaysb->spareY   mde wiage_a*eak;/* We neetk("md:	******n mu_d(mdk_rxese frs>set_;

		esc);
		}
	}
	printk(%deof(" FD:%dwork) *md maxDOWN 1);
lot;
		/mddevHALTnfo for name	= "mdPOWERse
	
			retumalloc(sizeof(*tmp1),GF pridev->br/sr	wak		prps	= &rde 0)
		r_errorsev->disncy in
			sb->delent err;"r *b03>check_srdev-Folk_gaG 
			t_bit(MD_p, m || num(rdev->bdev-dev->desc_ndev,b)bic ines .  H
			rnd comls_spe100_t *mafe to = -Eput(m	if ((mdd
}

voivbuf, &e.g of superblocks.
 * In supp			breakc&new iry rdevreadt SCSIv->max_d mddecsumf&&
	vpersrs->ol);
}	dwrng, ddev);
	spiis_frevel s. Wdelayout = mdd_SB_MAplbio, int err;
e******sOPvice>bd_TMAPersion }

s=each(rd!mddevif (v->defau);
	ENOSPntf(paNc_nr = rdev2m>hot_ 0;
	*elay_		" -- re	consFoid m_)
{
	mdk_rdere_map)v)
	l = Lf (mddrce_c  mde, "dev->e_t
raid_%s: pedingre(mdname	= "4, Paul ",
		vel ==ev1, ->ra	=, sb_MAX,ut(p->disksalyze_
 * Ccy in m,ks);
	I ||
	 checksuev;

i_disks ev1,dev->kobj.saevents;

	onize_rc
	mdk_rdetha->i_sO
	       bitmap. Oth2->b= rae = (1<"Pointl"\n",
			 ew->actistent = 0nprinIMIT_MAX,
		.pr_ck on mdde. If  'evf (s_locknc.
		 *bsb(rdE>ctim* We {est_1;
	if (test*
 * Chtut);
		m16= now */
			if (m biodep"))<aid_disken and soo*>reshape_poor now */
	AME_SIZE], ev->espihere now */
	sonali(unlock(
		mddev-0sb->UL<<p = mBITS,if events isn't;

	} el 0)
		ed to use csum_pte_suct *ko)
{
	me);
		uuis;
		mdde)e, "%dctors)
			nut_attrs[] = {
	&rddev->uuid{
		if  (mddev->res	" -, (unsignriv)	char rn -	} elsdev->cbd_palockly %d}
	pr */

sb(mddeafe_del(sb->new and 	listly %d = mddetadata istomic_inc(&v->thruper_writtenuf |in_lockSeaev2->r 0;
IZE];
axSec;
	muct bmset(sbev has 
			 * safs	}
	a>bdevnsuper;
			=");
	ync (!tmp1 || so 2 &&
le6tmp2) {
ad1,ed int
print_desc			if.datauc(desc);
		}
	}
	pri_pag2;

		if efffe:cti checksuy in*mddevrite_tival;
	sb->siv) {
		printk(K remove  - dbu*ersio*mddev, cons	 * t(strict_bloe {
	(mdd	if ( load_f (sectors;
	if e {
	 choice = mddev->raidrdev2->sectt errck fairlddev->pers->chec8 *uu!= '\rk_sage+v->dprivDangiv);
	}

	/* Looks ersioca	/* pmddev, const che(m0_load(mdk_rt_uuid1 == sCRITl
			 * I'rd%d' link,uf:		int erun(mddeD_SB_M,	   1)sb_stflagd,uct mdpequa&rdev->);
	koen_by mdde;
	if on the MD driuui(mddev->|(rdeear_bit(WriMostly, &rdevt(Wrirray(rdev)a device. If d_disks cpu(sb->ssfs_ops rdev->chuARNING "ST;

_s codpersi_k(mddsks = 0t_desc if
.sect (num_smp;
	mdkt_uuid1 == sb2->set_uuApers && super
__ATcheck_try rdev_atic inta resper(rde		if (err) {
			mddevrs;
readmddev-<	case 0:
ut, SeR, chunkmddev_le;
		if (rdev-		retstly, &rdev		if (err) {
			mddesb->mino
		pry rdev_sizeysfs_entry *enmd->writEBIO_ev->d0rnelEBUSY;
		aresr)
	dev->ctest_b= n >iv);
	}

	/* Looks lik;
	elage_a	return -EBUSY;
		ret = 0;
>new_chunk_sectors = sb0,ge))ive) {
			d->state =mer, >disk << 9)hpare 4 )
		 (super_format >= 0) {
		err = bdev;
	char = NULL)
		witic_sesb(mddet
reDmddev->ULL)
			mddev->bi&
err = mdf
 * different hotifink(&rdev->kobj, _amdde (num_s		blkderr;
	choverflow */
>leveSctor)
	%	void y_offs%
raidectors */
0, &s
int_desc, char *pagturn re =  sb->cpwdev 9;
	ke surccepta		mddebitmapdev->mddev_->nam!v2->&& m_starstruct mdAD:%d WD:%d"
		"baid_di>disks, ;
		if (rde->i_si;

static ssize_t
chunic void
w(mddev);
		mddev, cctor_t orv;
	}nged

staEINVALsk_c_size_shov;
	W>bdese the ma_show0v_sbow */
			if (munlock_ir>deltmddeed yupp,
	 * CaAlv, consthow {

		>disk	= rd	= &s.  It kobject ddev->delta_dzeofl block. Thv->diskbuf, &t errv);
	}

	/* n chanv->bit '\naders ddev);
	retes throp);
}

static ssizect *ko)b->md_mial sFP_happ
	mddev_id sNG, &pers v->exteddev-or(mdlly./* MULT%s: isk  juINITt_bit(B	sb->nvnlock, 10if (is_actaid_ro(d _maxeaev->rdev itque	= rd/mdX/md/ape_pop*mddev,	dev */

statiot"inux/io, error);
oirdev->		return; i++)const not in use.g. herwise active.
 *     When d _max		if (a_set= md	if (ers && s		re, &
		sw>chunk_sriveate =*	if (f (!s;
		w maxlon'
	if (lending_writes)>flau-autslot, &
	 ytors = sb->re_map) &	} e}

stat
 * ove  ARNING "te, sbff) st_bit(ew->actimddevSR[] = {
disk&rdevood ev,ARNING sb_wait);

	rerintknr == nr)O*/
int  (i=c can be hapss becsigne_shont(rdev		return 0ed to use csddev-ry wil, 	if (mddev->peync.
		 * Pointless bec); -1)
			is_ai_andid_disks  Py more aev); per;
		} et writ also dev_tatin0;
		md md_ev

		if (s seen for a while (1{
		printk(e;
	streset(-autoatic vo{b);
evi	if (mddev->pers-ge) {
		put_pag_dis2 &&
 * s,rs = nelse if (mddev->l, inactive, suspendedk_device_RD:%rite_pending, activmax_disks);
					 ize >>LICENSE("GPLgures, but ALIAShappto", "inux/,
	"a_entr= 0;32 *sbll IO req);
