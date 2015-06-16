/*
 * raid5.c : Multiple Devices driver for Linux
 *	   Copyright (C) 1996, 1997 Ingo Molnar, Miguel de Icaza, Gadi Oxman
 *	   Copyright (C) 1999, 2000 Ingo Molnar
 *	   Copyright (C) 2002, 2003 H. Peter Anvin
 *
 * RAID-4/5/6 management functions.
 * Thanks to Penguin Computing for making the RAID-6 development possible
 * by donating a test server!
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example /usr/src/linux/COPYING); if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * BITMAP UNPLUGGING:
 *
 * The sequencing for updating the bitmap reliably is a little
 * subtle (and I got it wrong the first time) so it deserves some
 * explanation.
 *
 * We group bitmap updates into batches.  Each batch has a number.
 * We may write out several batches at once, but that isn't very important.
 * conf->bm_write is the number of the last batch successfully written.
 * conf->bm_flush is the number of the last batch that was closed to
 *    new additions.
 * When we discover that we will need to write to any block in a stripe
 * (in add_stripe_bio) we update the in-memory bitmap and record in sh->bm_seq
 * the number of the batch it will be in. This is bm_flush+1.
 * When we are ready to do a write, if that batch hasn't been written yet,
 *   we plug the array and queue the stripe for later.
 * When an unplug happens, we increment bm_flush, thus closing the current
 *   batch.
 * When we notice that bm_flush > bm_write, we write out all pending updates
 * to the bitmap, and advance bm_write to where bm_flush was.
 * This may occasionally write a bit out twice, but is sure never to
 * miss any bits.
 */

#include <linux/blkdev.h>
#include <linux/kthread.h>
#include <linux/raid/pq.h>
#include <linux/async_tx.h>
#include <linux/async.h>
#include <linux/seq_file.h>
#include <linux/cpu.h>
#include "md.h"
#include "raid5.h"
#include "bitmap.h"

/*
 * Stripe cache
 */

#define NR_STRIPES		256
#define STRIPE_SIZE		PAGE_SIZE
#define STRIPE_SHIFT		(PAGE_SHIFT - 9)
#define STRIPE_SECTORS		(STRIPE_SIZE>>9)
#define	IO_THRESHOLD		1
#define BYPASS_THRESHOLD	1
#define NR_HASH			(PAGE_SIZE / sizeof(struct hlist_head))
#define HASH_MASK		(NR_HASH - 1)

#define stripe_hash(conf, sect)	(&((conf)->stripe_hashtbl[((sect) >> STRIPE_SHIFT) & HASH_MASK]))

/* bio's attached to a stripe+device for I/O are linked together in bi_sector
 * order without overlap.  There may be several bio's per stripe+device, and
 * a bio could span several devices.
 * When walking this list for a particular stripe+device, we must never proceed
 * beyond a bio that extends past this device, as the next bio might no longer
 * be valid.
 * This macro is used to determine the 'next' bio in the list, given the sector
 * of the current stripe+device
 */
#define r5_next_bio(bio, sect) ( ( (bio)->bi_sector + ((bio)->bi_size>>9) < sect + STRIPE_SECTORS) ? (bio)->bi_next : NULL)
/*
 * The following can be used to debug the driver
 */
#define RAID5_PARANOIA	1
#if RAID5_PARANOIA && defined(CONFIG_SMP)
# define CHECK_DEVLOCK() assert_spin_locked(&conf->device_lock)
#else
# define CHECK_DEVLOCK()
#endif

#ifdef DEBUG
#define inline
#define __inline__
#endif

#define printk_rl(args...) ((void) (printk_ratelimit() && printk(args)))

/*
 * We maintain a biased count of active stripes in the bottom 16 bits of
 * bi_phys_segments, and a count of processed stripes in the upper 16 bits
 */
static inline int raid5_bi_phys_segments(struct bio *bio)
{
	return bio->bi_phys_segments & 0xffff;
}

static inline int raid5_bi_hw_segments(struct bio *bio)
{
	return (bio->bi_phys_segments >> 16) & 0xffff;
}

static inline int raid5_dec_bi_phys_segments(struct bio *bio)
{
	--bio->bi_phys_segments;
	return raid5_bi_phys_segments(bio);
}

static inline int raid5_dec_bi_hw_segments(struct bio *bio)
{
	unsigned short val = raid5_bi_hw_segments(bio);

	--val;
	bio->bi_phys_segments = (val << 16) | raid5_bi_phys_segments(bio);
	return val;
}

static inline void raid5_set_bi_hw_segments(struct bio *bio, unsigned int cnt)
{
	bio->bi_phys_segments = raid5_bi_phys_segments(bio) || (cnt << 16);
}

/* Find first data disk in a raid6 stripe */
static inline int raid6_d0(struct stripe_head *sh)
{
	if (sh->ddf_layout)
		/* ddf always start from first device */
		return 0;
	/* md starts just after Q block */
	if (sh->qd_idx == sh->disks - 1)
		return 0;
	else
		return sh->qd_idx + 1;
}
static inline int raid6_next_disk(int disk, int raid_disks)
{
	disk++;
	return (disk < raid_disks) ? disk : 0;
}

/* When walking through the disks in a raid5, starting at raid6_d0,
 * We need to map each disk to a 'slot', where the data disks are slot
 * 0 .. raid_disks-3, the parity disk is raid_disks-2 and the Q disk
 * is raid_disks-1.  This help does that mapping.
 */
static int raid6_idx_to_slot(int idx, struct stripe_head *sh,
			     int *count, int syndrome_disks)
{
	int slot = *count;

	if (sh->ddf_layout)
		(*count)++;
	if (idx == sh->pd_idx)
		return syndrome_disks;
	if (idx == sh->qd_idx)
		return syndrome_disks + 1;
	if (!sh->ddf_layout)
		(*count)++;
	return slot;
}

static void return_io(struct bio *return_bi)
{
	struct bio *bi = return_bi;
	while (bi) {

		return_bi = bi->bi_next;
		bi->bi_next = NULL;
		bi->bi_size = 0;
		bio_endio(bi, 0);
		bi = return_bi;
	}
}

static void print_raid5_conf (raid5_conf_t *conf);

static int stripe_operations_active(struct stripe_head *sh)
{
	return sh->check_state || sh->reconstruct_state ||
	       test_bit(STRIPE_BIOFILL_RUN, &sh->state) ||
	       test_bit(STRIPE_COMPUTE_RUN, &sh->state);
}

static void __release_stripe(raid5_conf_t *conf, struct stripe_head *sh)
{
	if (atomic_dec_and_test(&sh->count)) {
		BUG_ON(!list_empty(&sh->lru));
		BUG_ON(atomic_read(&conf->active_stripes)==0);
		if (test_bit(STRIPE_HANDLE, &sh->state)) {
			if (test_bit(STRIPE_DELAYED, &sh->state)) {
				list_add_tail(&sh->lru, &conf->delayed_list);
				blk_plug_device(conf->mddev->queue);
			} else if (test_bit(STRIPE_BIT_DELAY, &sh->state) &&
				   sh->bm_seq - conf->seq_write > 0) {
				list_add_tail(&sh->lru, &conf->bitmap_list);
				blk_plug_device(conf->mddev->queue);
			} else {
				clear_bit(STRIPE_BIT_DELAY, &sh->state);
				list_add_tail(&sh->lru, &conf->handle_list);
			}
			md_wakeup_thread(conf->mddev->thread);
		} else {
			BUG_ON(stripe_operations_active(sh));
			if (test_and_clear_bit(STRIPE_PREREAD_ACTIVE, &sh->state)) {
				atomic_dec(&conf->preread_active_stripes);
				if (atomic_read(&conf->preread_active_stripes) < IO_THRESHOLD)
					md_wakeup_thread(conf->mddev->thread);
			}
			atomic_dec(&conf->active_stripes);
			if (!test_bit(STRIPE_EXPANDING, &sh->state)) {
				list_add_tail(&sh->lru, &conf->inactive_list);
				wake_up(&conf->wait_for_stripe);
				if (conf->retry_read_aligned)
					md_wakeup_thread(conf->mddev->thread);
			}
		}
	}
}

static void release_stripe(struct stripe_head *sh)
{
	raid5_conf_t *conf = sh->raid_conf;
	unsigned long flags;

	spin_lock_irqsave(&conf->device_lock, flags);
	__release_stripe(conf, sh);
	spin_unlock_irqrestore(&conf->device_lock, flags);
}

static inline void remove_hash(struct stripe_head *sh)
{
	pr_debug("remove_hash(), stripe %llu\n",
		(unsigned long long)sh->sector);

	hlist_del_init(&sh->hash);
}

static inline void insert_hash(raid5_conf_t *conf, struct stripe_head *sh)
{
	struct hlist_head *hp = stripe_hash(conf, sh->sector);

	pr_debug("insert_hash(), stripe %llu\n",
		(unsigned long long)sh->sector);

	CHECK_DEVLOCK();
	hlist_add_head(&sh->hash, hp);
}


/* find an idle stripe, make sure it is unhashed, and return it. */
static struct stripe_head *get_free_stripe(raid5_conf_t *conf)
{
	struct stripe_head *sh = NULL;
	struct list_head *first;

	CHECK_DEVLOCK();
	if (list_empty(&conf->inactive_list))
		goto out;
	first = conf->inactive_list.next;
	sh = list_entry(first, struct stripe_head, lru);
	list_del_init(first);
	remove_hash(sh);
	atomic_inc(&conf->active_stripes);
out:
	return sh;
}

static void shrink_buffers(struct stripe_head *sh, int num)
{
	struct page *p;
	int i;

	for (i=0; i<num ; i++) {
		p = sh->dev[i].page;
		if (!p)
			continue;
		sh->dev[i].page = NULL;
		put_page(p);
	}
}

static int grow_buffers(struct stripe_head *sh, int num)
{
	int i;

	for (i=0; i<num; i++) {
		struct page *page;

		if (!(page = alloc_page(GFP_KERNEL))) {
			return 1;
		}
		sh->dev[i].page = page;
	}
	return 0;
}

static void raid5_build_block(struct stripe_head *sh, int i, int previous);
static void stripe_set_idx(sector_t stripe, raid5_conf_t *conf, int previous,
			    struct stripe_head *sh);

static void init_stripe(struct stripe_head *sh, sector_t sector, int previous)
{
	raid5_conf_t *conf = sh->raid_conf;
	int i;

	BUG_ON(atomic_read(&sh->count) != 0);
	BUG_ON(test_bit(STRIPE_HANDLE, &sh->state));
	BUG_ON(stripe_operations_active(sh));

	CHECK_DEVLOCK();
	pr_debug("init_stripe called, stripe %llu\n",
		(unsigned long long)sh->sector);

	remove_hash(sh);

	sh->generation = conf->generation - previous;
	sh->disks = previous ? conf->previous_raid_disks : conf->raid_disks;
	sh->sector = sector;
	stripe_set_idx(sector, conf, previous, sh);
	sh->state = 0;


	for (i = sh->disks; i--; ) {
		struct r5dev *dev = &sh->dev[i];

		if (dev->toread || dev->read || dev->towrite || dev->written ||
		    test_bit(R5_LOCKED, &dev->flags)) {
			printk(KERN_ERR "sector=%llx i=%d %p %p %p %p %d\n",
			       (unsigned long long)sh->sector, i, dev->toread,
			       dev->read, dev->towrite, dev->written,
			       test_bit(R5_LOCKED, &dev->flags));
			BUG();
		}
		dev->flags = 0;
		raid5_build_block(sh, i, previous);
	}
	insert_hash(conf, sh);
}

static struct stripe_head *__find_stripe(raid5_conf_t *conf, sector_t sector,
					 short generation)
{
	struct stripe_head *sh;
	struct hlist_node *hn;

	CHECK_DEVLOCK();
	pr_debug("__find_stripe, sector %llu\n", (unsigned long long)sector);
	hlist_for_each_entry(sh, hn, stripe_hash(conf, sector), hash)
		if (sh->sector == sector && sh->generation == generation)
			return sh;
	pr_debug("__stripe %llu not in cache\n", (unsigned long long)sector);
	return NULL;
}

static void unplug_slaves(mddev_t *mddev);
static void raid5_unplug_device(struct request_queue *q);

static struct stripe_head *
get_active_stripe(raid5_conf_t *conf, sector_t sector,
		  int previous, int noblock, int noquiesce)
{
	struct stripe_head *sh;

	pr_debug("get_stripe, sector %llu\n", (unsigned long long)sector);

	spin_lock_irq(&conf->device_lock);

	do {
		wait_event_lock_irq(conf->wait_for_stripe,
				    conf->quiesce == 0 || noquiesce,
				    conf->device_lock, /* nothing */);
		sh = __find_stripe(conf, sector, conf->generation - previous);
		if (!sh) {
			if (!conf->inactive_blocked)
				sh = get_free_stripe(conf);
			if (noblock && sh == NULL)
				break;
			if (!sh) {
				conf->inactive_blocked = 1;
				wait_event_lock_irq(conf->wait_for_stripe,
						    !list_empty(&conf->inactive_list) &&
						    (atomic_read(&conf->active_stripes)
						     < (conf->max_nr_stripes *3/4)
						     || !conf->inactive_blocked),
						    conf->device_lock,
						    raid5_unplug_device(conf->mddev->queue)
					);
				conf->inactive_blocked = 0;
			} else
				init_stripe(sh, sector, previous);
		} else {
			if (atomic_read(&sh->count)) {
				BUG_ON(!list_empty(&sh->lru)
				    && !test_bit(STRIPE_EXPANDING, &sh->state));
			} else {
				if (!test_bit(STRIPE_HANDLE, &sh->state))
					atomic_inc(&conf->active_stripes);
				if (list_empty(&sh->lru) &&
				    !test_bit(STRIPE_EXPANDING, &sh->state))
					BUG();
				list_del_init(&sh->lru);
			}
		}
	} while (sh == NULL);

	if (sh)
		atomic_inc(&sh->count);

	spin_unlock_irq(&conf->device_lock);
	return sh;
}

static void
raid5_end_read_request(struct bio *bi, int error);
static void
raid5_end_write_request(struct bio *bi, int error);

static void ops_run_io(struct stripe_head *sh, struct stripe_head_state *s)
{
	raid5_conf_t *conf = sh->raid_conf;
	int i, disks = sh->disks;

	might_sleep();

	for (i = disks; i--; ) {
		int rw;
		struct bio *bi;
		mdk_rdev_t *rdev;
		if (test_and_clear_bit(R5_Wantwrite, &sh->dev[i].flags))
			rw = WRITE;
		else if (test_and_clear_bit(R5_Wantread, &sh->dev[i].flags))
			rw = READ;
		else
			continue;

		bi = &sh->dev[i].req;

		bi->bi_rw = rw;
		if (rw == WRITE)
			bi->bi_end_io = raid5_end_write_request;
		else
			bi->bi_end_io = raid5_end_read_request;

		rcu_read_lock();
		rdev = rcu_dereference(conf->disks[i].rdev);
		if (rdev && test_bit(Faulty, &rdev->flags))
			rdev = NULL;
		if (rdev)
			atomic_inc(&rdev->nr_pending);
		rcu_read_unlock();

		if (rdev) {
			if (s->syncing || s->expanding || s->expanded)
				md_sync_acct(rdev->bdev, STRIPE_SECTORS);

			set_bit(STRIPE_IO_STARTED, &sh->state);

			bi->bi_bdev = rdev->bdev;
			pr_debug("%s: for %llu schedule op %ld on disc %d\n",
				__func__, (unsigned long long)sh->sector,
				bi->bi_rw, i);
			atomic_inc(&sh->count);
			bi->bi_sector = sh->sector + rdev->data_offset;
			bi->bi_flags = 1 << BIO_UPTODATE;
			bi->bi_vcnt = 1;
			bi->bi_max_vecs = 1;
			bi->bi_idx = 0;
			bi->bi_io_vec = &sh->dev[i].vec;
			bi->bi_io_vec[0].bv_len = STRIPE_SIZE;
			bi->bi_io_vec[0].bv_offset = 0;
			bi->bi_size = STRIPE_SIZE;
			bi->bi_next = NULL;
			if (rw == WRITE &&
			    test_bit(R5_ReWrite, &sh->dev[i].flags))
				atomic_add(STRIPE_SECTORS,
					&rdev->corrected_errors);
			generic_make_request(bi);
		} else {
			if (rw == WRITE)
				set_bit(STRIPE_DEGRADED, &sh->state);
			pr_debug("skip op %ld on disc %d for sector %llu\n",
				bi->bi_rw, i, (unsigned long long)sh->sector);
			clear_bit(R5_LOCKED, &sh->dev[i].flags);
			set_bit(STRIPE_HANDLE, &sh->state);
		}
	}
}

static struct dma_async_tx_descriptor *
async_copy_data(int frombio, struct bio *bio, struct page *page,
	sector_t sector, struct dma_async_tx_descriptor *tx)
{
	struct bio_vec *bvl;
	struct page *bio_page;
	int i;
	int page_offset;
	struct async_submit_ctl submit;
	enum async_tx_flags flags = 0;

	if (bio->bi_sector >= sector)
		page_offset = (signed)(bio->bi_sector - sector) * 512;
	else
		page_offset = (signed)(sector - bio->bi_sector) * -512;

	if (frombio)
		flags |= ASYNC_TX_FENCE;
	init_async_submit(&submit, flags, tx, NULL, NULL, NULL);

	bio_for_each_segment(bvl, bio, i) {
		int len = bio_iovec_idx(bio, i)->bv_len;
		int clen;
		int b_offset = 0;

		if (page_offset < 0) {
			b_offset = -page_offset;
			page_offset += b_offset;
			len -= b_offset;
		}

		if (len > 0 && page_offset + len > STRIPE_SIZE)
			clen = STRIPE_SIZE - page_offset;
		else
			clen = len;

		if (clen > 0) {
			b_offset += bio_iovec_idx(bio, i)->bv_offset;
			bio_page = bio_iovec_idx(bio, i)->bv_page;
			if (frombio)
				tx = async_memcpy(page, bio_page, page_offset,
						  b_offset, clen, &submit);
			else
				tx = async_memcpy(bio_page, page, b_offset,
						  page_offset, clen, &submit);
		}
		/* chain the operations */
		submit.depend_tx = tx;

		if (clen < len) /* hit end of page */
			break;
		page_offset +=  len;
	}

	return tx;
}

static void ops_complete_biofill(void *stripe_head_ref)
{
	struct stripe_head *sh = stripe_head_ref;
	struct bio *return_bi = NULL;
	raid5_conf_t *conf = sh->raid_conf;
	int i;

	pr_debug("%s: stripe %llu\n", __func__,
		(unsigned long long)sh->sector);

	/* clear completed biofills */
	spin_lock_irq(&conf->device_lock);
	for (i = sh->disks; i--; ) {
		struct r5dev *dev = &sh->dev[i];

		/* acknowledge completion of a biofill operation */
		/* and check if we need to reply to a read request,
		 * new R5_Wantfill requests are held off until
		 * !STRIPE_BIOFILL_RUN
		 */
		if (test_and_clear_bit(R5_Wantfill, &dev->flags)) {
			struct bio *rbi, *rbi2;

			BUG_ON(!dev->read);
			rbi = dev->read;
			dev->read = NULL;
			while (rbi && rbi->bi_sector <
				dev->sector + STRIPE_SECTORS) {
				rbi2 = r5_next_bio(rbi, dev->sector);
				if (!raid5_dec_bi_phys_segments(rbi)) {
					rbi->bi_next = return_bi;
					return_bi = rbi;
				}
				rbi = rbi2;
			}
		}
	}
	spin_unlock_irq(&conf->device_lock);
	clear_bit(STRIPE_BIOFILL_RUN, &sh->state);

	return_io(return_bi);

	set_bit(STRIPE_HANDLE, &sh->state);
	release_stripe(sh);
}

static void ops_run_biofill(struct stripe_head *sh)
{
	struct dma_async_tx_descriptor *tx = NULL;
	raid5_conf_t *conf = sh->raid_conf;
	struct async_submit_ctl submit;
	int i;

	pr_debug("%s: stripe %llu\n", __func__,
		(unsigned long long)sh->sector);

	for (i = sh->disks; i--; ) {
		struct r5dev *dev = &sh->dev[i];
		if (test_bit(R5_Wantfill, &dev->flags)) {
			struct bio *rbi;
			spin_lock_irq(&conf->device_lock);
			dev->read = rbi = dev->toread;
			dev->toread = NULL;
			spin_unlock_irq(&conf->device_lock);
			while (rbi && rbi->bi_sector <
				dev->sector + STRIPE_SECTORS) {
				tx = async_copy_data(0, rbi, dev->page,
					dev->sector, tx);
				rbi = r5_next_bio(rbi, dev->sector);
			}
		}
	}

	atomic_inc(&sh->count);
	init_async_submit(&submit, ASYNC_TX_ACK, tx, ops_complete_biofill, sh, NULL);
	async_trigger_callback(&submit);
}

static void mark_target_uptodate(struct stripe_head *sh, int target)
{
	struct r5dev *tgt;

	if (target < 0)
		return;

	tgt = &sh->dev[target];
	set_bit(R5_UPTODATE, &tgt->flags);
	BUG_ON(!test_bit(R5_Wantcompute, &tgt->flags));
	clear_bit(R5_Wantcompute, &tgt->flags);
}

static void ops_complete_compute(void *stripe_head_ref)
{
	struct stripe_head *sh = stripe_head_ref;

	pr_debug("%s: stripe %llu\n", __func__,
		(unsigned long long)sh->sector);

	/* mark the computed target(s) as uptodate */
	mark_target_uptodate(sh, sh->ops.target);
	mark_target_uptodate(sh, sh->ops.target2);

	clear_bit(STRIPE_COMPUTE_RUN, &sh->state);
	if (sh->check_state == check_state_compute_run)
		sh->check_state = check_state_compute_result;
	set_bit(STRIPE_HANDLE, &sh->state);
	release_stripe(sh);
}

/* return a pointer to the address conversion region of the scribble buffer */
static addr_conv_t *to_addr_conv(struct stripe_head *sh,
				 struct raid5_percpu *percpu)
{
	return percpu->scribble + sizeof(struct page *) * (sh->disks + 2);
}

static struct dma_async_tx_descriptor *
ops_run_compute5(struct stripe_head *sh, struct raid5_percpu *percpu)
{
	int disks = sh->disks;
	struct page **xor_srcs = percpu->scribble;
	int target = sh->ops.target;
	struct r5dev *tgt = &sh->dev[target];
	struct page *xor_dest = tgt->page;
	int count = 0;
	struct dma_async_tx_descriptor *tx;
	struct async_submit_ctl submit;
	int i;

	pr_debug("%s: stripe %llu block: %d\n",
		__func__, (unsigned long long)sh->sector, target);
	BUG_ON(!test_bit(R5_Wantcompute, &tgt->flags));

	for (i = disks; i--; )
		if (i != target)
			xor_srcs[count++] = sh->dev[i].page;

	atomic_inc(&sh->count);

	init_async_submit(&submit, ASYNC_TX_FENCE|ASYNC_TX_XOR_ZERO_DST, NULL,
			  ops_complete_compute, sh, to_addr_conv(sh, percpu));
	if (unlikely(count == 1))
		tx = async_memcpy(xor_dest, xor_srcs[0], 0, 0, STRIPE_SIZE, &submit);
	else
		tx = async_xor(xor_dest, xor_srcs, 0, count, STRIPE_SIZE, &submit);

	return tx;
}

/* set_syndrome_sources - populate source buffers for gen_syndrome
 * @srcs - (struct page *) array of size sh->disks
 * @sh - stripe_head to parse
 *
 * Populates srcs in proper layout order for the stripe and returns the
 * 'count' of sources to be used in a call to async_gen_syndrome.  The P
 * destination buffer is recorded in srcs[count] and the Q destination
 * is recorded in srcs[count+1]].
 */
static int set_syndrome_sources(struct page **srcs, struct stripe_head *sh)
{
	int disks = sh->disks;
	int syndrome_disks = sh->ddf_layout ? disks : (disks - 2);
	int d0_idx = raid6_d0(sh);
	int count;
	int i;

	for (i = 0; i < disks; i++)
		srcs[i] = NULL;

	count = 0;
	i = d0_idx;
	do {
		int slot = raid6_idx_to_slot(i, sh, &count, syndrome_disks);

		srcs[slot] = sh->dev[i].page;
		i = raid6_next_disk(i, disks);
	} while (i != d0_idx);

	return syndrome_disks;
}

static struct dma_async_tx_descriptor *
ops_run_compute6_1(struct stripe_head *sh, struct raid5_percpu *percpu)
{
	int disks = sh->disks;
	struct page **blocks = percpu->scribble;
	int target;
	int qd_idx = sh->qd_idx;
	struct dma_async_tx_descriptor *tx;
	struct async_submit_ctl submit;
	struct r5dev *tgt;
	struct page *dest;
	int i;
	int count;

	if (sh->ops.target < 0)
		target = sh->ops.target2;
	else if (sh->ops.target2 < 0)
		target = sh->ops.target;
	else
		/* we should only have one valid target */
		BUG();
	BUG_ON(target < 0);
	pr_debug("%s: stripe %llu block: %d\n",
		__func__, (unsigned long long)sh->sector, target);

	tgt = &sh->dev[target];
	BUG_ON(!test_bit(R5_Wantcompute, &tgt->flags));
	dest = tgt->page;

	atomic_inc(&sh->count);

	if (target == qd_idx) {
		count = set_syndrome_sources(blocks, sh);
		blocks[count] = NULL; /* regenerating p is not necessary */
		BUG_ON(blocks[count+1] != dest); /* q should already be set */
		init_async_submit(&submit, ASYNC_TX_FENCE, NULL,
				  ops_complete_compute, sh,
				  to_addr_conv(sh, percpu));
		tx = async_gen_syndrome(blocks, 0, count+2, STRIPE_SIZE, &submit);
	} else {
		/* Compute any data- or p-drive using XOR */
		count = 0;
		for (i = disks; i-- ; ) {
			if (i == target || i == qd_idx)
				continue;
			blocks[count++] = sh->dev[i].page;
		}

		init_async_submit(&submit, ASYNC_TX_FENCE|ASYNC_TX_XOR_ZERO_DST,
				  NULL, ops_complete_compute, sh,
				  to_addr_conv(sh, percpu));
		tx = async_xor(dest, blocks, 0, count, STRIPE_SIZE, &submit);
	}

	return tx;
}

static struct dma_async_tx_descriptor *
ops_run_compute6_2(struct stripe_head *sh, struct raid5_percpu *percpu)
{
	int i, count, disks = sh->disks;
	int syndrome_disks = sh->ddf_layout ? disks : disks-2;
	int d0_idx = raid6_d0(sh);
	int faila = -1, failb = -1;
	int target = sh->ops.target;
	int target2 = sh->ops.target2;
	struct r5dev *tgt = &sh->dev[target];
	struct r5dev *tgt2 = &sh->dev[target2];
	struct dma_async_tx_descriptor *tx;
	struct page **blocks = percpu->scribble;
	struct async_submit_ctl submit;

	pr_debug("%s: stripe %llu block1: %d block2: %d\n",
		 __func__, (unsigned long long)sh->sector, target, target2);
	BUG_ON(target < 0 || target2 < 0);
	BUG_ON(!test_bit(R5_Wantcompute, &tgt->flags));
	BUG_ON(!test_bit(R5_Wantcompute, &tgt2->flags));

	/* we need to open-code set_syndrome_sources to handle the
	 * slot number conversion for 'faila' and 'failb'
	 */
	for (i = 0; i < disks ; i++)
		blocks[i] = NULL;
	count = 0;
	i = d0_idx;
	do {
		int slot = raid6_idx_to_slot(i, sh, &count, syndrome_disks);

		blocks[slot] = sh->dev[i].page;

		if (i == target)
			faila = slot;
		if (i == target2)
			failb = slot;
		i = raid6_next_disk(i, disks);
	} while (i != d0_idx);

	BUG_ON(faila == failb);
	if (failb < faila)
		swap(faila, failb);
	pr_debug("%s: stripe: %llu faila: %d failb: %d\n",
		 __func__, (unsigned long long)sh->sector, faila, failb);

	atomic_inc(&sh->count);

	if (failb == syndrome_disks+1) {
		/* Q disk is one of the missing disks */
		if (faila == syndrome_disks) {
			/* Missing P+Q, just recompute */
			init_async_submit(&submit, ASYNC_TX_FENCE, NULL,
					  ops_complete_compute, sh,
					  to_addr_conv(sh, percpu));
			return async_gen_syndrome(blocks, 0, syndrome_disks+2,
						  STRIPE_SIZE, &submit);
		} else {
			struct page *dest;
			int data_target;
			int qd_idx = sh->qd_idx;

			/* Missing D+Q: recompute D from P, then recompute Q */
			if (target == qd_idx)
				data_target = target2;
			else
				data_target = target;

			count = 0;
			for (i = disks; i-- ; ) {
				if (i == data_target || i == qd_idx)
					continue;
				blocks[count++] = sh->dev[i].page;
			}
			dest = sh->dev[data_target].page;
			init_async_submit(&submit,
					  ASYNC_TX_FENCE|ASYNC_TX_XOR_ZERO_DST,
					  NULL, NULL, NULL,
					  to_addr_conv(sh, percpu));
			tx = async_xor(dest, blocks, 0, count, STRIPE_SIZE,
				       &submit);

			count = set_syndrome_sources(blocks, sh);
			init_async_submit(&submit, ASYNC_TX_FENCE, tx,
					  ops_complete_compute, sh,
					  to_addr_conv(sh, percpu));
			return async_gen_syndrome(blocks, 0, count+2,
						  STRIPE_SIZE, &submit);
		}
	} else {
		init_async_submit(&submit, ASYNC_TX_FENCE, NULL,
				  ops_complete_compute, sh,
				  to_addr_conv(sh, percpu));
		if (failb == syndrome_disks) {
			/* We're missing D+P. */
			return async_raid6_datap_recov(syndrome_disks+2,
						       STRIPE_SIZE, faila,
						       blocks, &submit);
		} else {
			/* We're missing D+D. */
			return async_raid6_2data_recov(syndrome_disks+2,
						       STRIPE_SIZE, faila, failb,
						       blocks, &submit);
		}
	}
}


static void ops_complete_prexor(void *stripe_head_ref)
{
	struct stripe_head *sh = stripe_head_ref;

	pr_debug("%s: stripe %llu\n", __func__,
		(unsigned long long)sh->sector);
}

static struct dma_async_tx_descriptor *
ops_run_prexor(struct stripe_head *sh, struct raid5_percpu *percpu,
	       struct dma_async_tx_descriptor *tx)
{
	int disks = sh->disks;
	struct page **xor_srcs = percpu->scribble;
	int count = 0, pd_idx = sh->pd_idx, i;
	struct async_submit_ctl submit;

	/* existing parity data subtracted */
	struct page *xor_dest = xor_srcs[count++] = sh->dev[pd_idx].page;

	pr_debug("%s: stripe %llu\n", __func__,
		(unsigned long long)sh->sector);

	for (i = disks; i--; ) {
		struct r5dev *dev = &sh->dev[i];
		/* Only process blocks that are known to be uptodate */
		if (test_bit(R5_Wantdrain, &dev->flags))
			xor_srcs[count++] = dev->page;
	}

	init_async_submit(&submit, ASYNC_TX_FENCE|ASYNC_TX_XOR_DROP_DST, tx,
			  ops_complete_prexor, sh, to_addr_conv(sh, percpu));
	tx = async_xor(xor_dest, xor_srcs, 0, count, STRIPE_SIZE, &submit);

	return tx;
}

static struct dma_async_tx_descriptor *
ops_run_biodrain(struct stripe_head *sh, struct dma_async_tx_descriptor *tx)
{
	int disks = sh->disks;
	int i;

	pr_debug("%s: stripe %llu\n", __func__,
		(unsigned long long)sh->sector);

	for (i = disks; i--; ) {
		struct r5dev *dev = &sh->dev[i];
		struct bio *chosen;

		if (test_and_clear_bit(R5_Wantdrain, &dev->flags)) {
			struct bio *wbi;

			spin_lock(&sh->lock);
			chosen = dev->towrite;
			dev->towrite = NULL;
			BUG_ON(dev->written);
			wbi = dev->written = chosen;
			spin_unlock(&sh->lock);

			while (wbi && wbi->bi_sector <
				dev->sector + STRIPE_SECTORS) {
				tx = async_copy_data(1, wbi, dev->page,
					dev->sector, tx);
				wbi = r5_next_bio(wbi, dev->sector);
			}
		}
	}

	return tx;
}

static void ops_complete_reconstruct(void *stripe_head_ref)
{
	struct stripe_head *sh = stripe_head_ref;
	int disks = sh->disks;
	int pd_idx = sh->pd_idx;
	int qd_idx = sh->qd_idx;
	int i;

	pr_debug("%s: stripe %llu\n", __func__,
		(unsigned long long)sh->sector);

	for (i = disks; i--; ) {
		struct r5dev *dev = &sh->dev[i];

		if (dev->written || i == pd_idx || i == qd_idx)
			set_bit(R5_UPTODATE, &dev->flags);
	}

	if (sh->reconstruct_state == reconstruct_state_drain_run)
		sh->reconstruct_state = reconstruct_state_drain_result;
	else if (sh->reconstruct_state == reconstruct_state_prexor_drain_run)
		sh->reconstruct_state = reconstruct_state_prexor_drain_result;
	else {
		BUG_ON(sh->reconstruct_state != reconstruct_state_run);
		sh->reconstruct_state = reconstruct_state_result;
	}

	set_bit(STRIPE_HANDLE, &sh->state);
	release_stripe(sh);
}

static void
ops_run_reconstruct5(struct stripe_head *sh, struct raid5_percpu *percpu,
		     struct dma_async_tx_descriptor *tx)
{
	int disks = sh->disks;
	struct page **xor_srcs = percpu->scribble;
	struct async_submit_ctl submit;
	int count = 0, pd_idx = sh->pd_idx, i;
	struct page *xor_dest;
	int prexor = 0;
	unsigned long flags;

	pr_debug("%s: stripe %llu\n", __func__,
		(unsigned long long)sh->sector);

	/* check if prexor is active which means only process blocks
	 * that are part of a read-modify-write (written)
	 */
	if (sh->reconstruct_state == reconstruct_state_prexor_drain_run) {
		prexor = 1;
		xor_dest = xor_srcs[count++] = sh->dev[pd_idx].page;
		for (i = disks; i--; ) {
			struct r5dev *dev = &sh->dev[i];
			if (dev->written)
				xor_srcs[count++] = dev->page;
		}
	} else {
		xor_dest = sh->dev[pd_idx].page;
		for (i = disks; i--; ) {
			struct r5dev *dev = &sh->dev[i];
			if (i != pd_idx)
				xor_srcs[count++] = dev->page;
		}
	}

	/* 1/ if we prexor'd then the dest is reused as a source
	 * 2/ if we did not prexor then we are redoing the parity
	 * set ASYNC_TX_XOR_DROP_DST and ASYNC_TX_XOR_ZERO_DST
	 * for the synchronous xor case
	 */
	flags = ASYNC_TX_ACK |
		(prexor ? ASYNC_TX_XOR_DROP_DST : ASYNC_TX_XOR_ZERO_DST);

	atomic_inc(&sh->count);

	init_async_submit(&submit, flags, tx, ops_complete_reconstruct, sh,
			  to_addr_conv(sh, percpu));
	if (unlikely(count == 1))
		tx = async_memcpy(xor_dest, xor_srcs[0], 0, 0, STRIPE_SIZE, &submit);
	else
		tx = async_xor(xor_dest, xor_srcs, 0, count, STRIPE_SIZE, &submit);
}

static void
ops_run_reconstruct6(struct stripe_head *sh, struct raid5_percpu *percpu,
		     struct dma_async_tx_descriptor *tx)
{
	struct async_submit_ctl submit;
	struct page **blocks = percpu->scribble;
	int count;

	pr_debug("%s: stripe %llu\n", __func__, (unsigned long long)sh->sector);

	count = set_syndrome_sources(blocks, sh);

	atomic_inc(&sh->count);

	init_async_submit(&submit, ASYNC_TX_ACK, tx, ops_complete_reconstruct,
			  sh, to_addr_conv(sh, percpu));
	async_gen_syndrome(blocks, 0, count+2, STRIPE_SIZE,  &submit);
}

static void ops_complete_check(void *stripe_head_ref)
{
	struct stripe_head *sh = stripe_head_ref;

	pr_debug("%s: stripe %llu\n", __func__,
		(unsigned long long)sh->sector);

	sh->check_state = check_state_check_result;
	set_bit(STRIPE_HANDLE, &sh->state);
	release_stripe(sh);
}

static void ops_run_check_p(struct stripe_head *sh, struct raid5_percpu *percpu)
{
	int disks = sh->disks;
	int pd_idx = sh->pd_idx;
	int qd_idx = sh->qd_idx;
	struct page *xor_dest;
	struct page **xor_srcs = percpu->scribble;
	struct dma_async_tx_descriptor *tx;
	struct async_submit_ctl submit;
	int count;
	int i;

	pr_debug("%s: stripe %llu\n", __func__,
		(unsigned long long)sh->sector);

	count = 0;
	xor_dest = sh->dev[pd_idx].page;
	xor_srcs[count++] = xor_dest;
	for (i = disks; i--; ) {
		if (i == pd_idx || i == qd_idx)
			continue;
		xor_srcs[count++] = sh->dev[i].page;
	}

	init_async_submit(&submit, 0, NULL, NULL, NULL,
			  to_addr_conv(sh, percpu));
	tx = async_xor_val(xor_dest, xor_srcs, 0, count, STRIPE_SIZE,
			   &sh->ops.zero_sum_result, &submit);

	atomic_inc(&sh->count);
	init_async_submit(&submit, ASYNC_TX_ACK, tx, ops_complete_check, sh, NULL);
	tx = async_trigger_callback(&submit);
}

static void ops_run_check_pq(struct stripe_head *sh, struct raid5_percpu *percpu, int checkp)
{
	struct page **srcs = percpu->scribble;
	struct async_submit_ctl submit;
	int count;

	pr_debug("%s: stripe %llu checkp: %d\n", __func__,
		(unsigned long long)sh->sector, checkp);

	count = set_syndrome_sources(srcs, sh);
	if (!checkp)
		srcs[count] = NULL;

	atomic_inc(&sh->count);
	init_async_submit(&submit, ASYNC_TX_ACK, NULL, ops_complete_check,
			  sh, to_addr_conv(sh, percpu));
	async_syndrome_val(srcs, 0, count+2, STRIPE_SIZE,
			   &sh->ops.zero_sum_result, percpu->spare_page, &submit);
}

static void __raid_run_ops(struct stripe_head *sh, unsigned long ops_request)
{
	int overlap_clear = 0, i, disks = sh->disks;
	struct dma_async_tx_descriptor *tx = NULL;
	raid5_conf_t *conf = sh->raid_conf;
	int level = conf->level;
	struct raid5_percpu *percpu;
	unsigned long cpu;

	cpu = get_cpu();
	percpu = per_cpu_ptr(conf->percpu, cpu);
	if (test_bit(STRIPE_OP_BIOFILL, &ops_request)) {
		ops_run_biofill(sh);
		overlap_clear++;
	}

	if (test_bit(STRIPE_OP_COMPUTE_BLK, &ops_request)) {
		if (level < 6)
			tx = ops_run_compute5(sh, percpu);
		else {
			if (sh->ops.target2 < 0 || sh->ops.target < 0)
				tx = ops_run_compute6_1(sh, percpu);
			else
				tx = ops_run_compute6_2(sh, percpu);
		}
		/* terminate the chain if reconstruct is not set to be run */
		if (tx && !test_bit(STRIPE_OP_RECONSTRUCT, &ops_request))
			async_tx_ack(tx);
	}

	if (test_bit(STRIPE_OP_PREXOR, &ops_request))
		tx = ops_run_prexor(sh, percpu, tx);

	if (test_bit(STRIPE_OP_BIODRAIN, &ops_request)) {
		tx = ops_run_biodrain(sh, tx);
		overlap_clear++;
	}

	if (test_bit(STRIPE_OP_RECONSTRUCT, &ops_request)) {
		if (level < 6)
			ops_run_reconstruct5(sh, percpu, tx);
		else
			ops_run_reconstruct6(sh, percpu, tx);
	}

	if (test_bit(STRIPE_OP_CHECK, &ops_request)) {
		if (sh->check_state == check_state_run)
			ops_run_check_p(sh, percpu);
		else if (sh->check_state == check_state_run_q)
			ops_run_check_pq(sh, percpu, 0);
		else if (sh->check_state == check_state_run_pq)
			ops_run_check_pq(sh, percpu, 1);
		else
			BUG();
	}

	if (overlap_clear)
		for (i = disks; i--; ) {
			struct r5dev *dev = &sh->dev[i];
			if (test_and_clear_bit(R5_Overlap, &dev->flags))
				wake_up(&sh->raid_conf->wait_for_overlap);
		}
	put_cpu();
}

#ifdef CONFIG_MULTICORE_RAID456
static void async_run_ops(void *param, async_cookie_t cookie)
{
	struct stripe_head *sh = param;
	unsigned long ops_request = sh->ops.request;

	clear_bit_unlock(STRIPE_OPS_REQ_PENDING, &sh->state);
	wake_up(&sh->ops.wait_for_ops);

	__raid_run_ops(sh, ops_request);
	release_stripe(sh);
}

static void raid_run_ops(struct stripe_head *sh, unsigned long ops_request)
{
	/* since handle_stripe can be called outside of raid5d context
	 * we need to ensure sh->ops.request is de-staged before another
	 * request arrives
	 */
	wait_event(sh->ops.wait_for_ops,
		   !test_and_set_bit_lock(STRIPE_OPS_REQ_PENDING, &sh->state));
	sh->ops.request = ops_request;

	atomic_inc(&sh->count);
	async_schedule(async_run_ops, sh);
}
#else
#define raid_run_ops __raid_run_ops
#endif

static int grow_one_stripe(raid5_conf_t *conf)
{
	struct stripe_head *sh;
	int disks = max(conf->raid_disks, conf->previous_raid_disks);
	sh = kmem_cache_alloc(conf->slab_cache, GFP_KERNEL);
	if (!sh)
		return 0;
	memset(sh, 0, sizeof(*sh) + (disks-1)*sizeof(struct r5dev));
	sh->raid_conf = conf;
	spin_lock_init(&sh->lock);
	#ifdef CONFIG_MULTICORE_RAID456
	init_waitqueue_head(&sh->ops.wait_for_ops);
	#endif

	if (grow_buffers(sh, disks)) {
		shrink_buffers(sh, disks);
		kmem_cache_free(conf->slab_cache, sh);
		return 0;
	}
	/* we just created an active stripe so... */
	atomic_set(&sh->count, 1);
	atomic_inc(&conf->active_stripes);
	INIT_LIST_HEAD(&sh->lru);
	release_stripe(sh);
	return 1;
}

static int grow_stripes(raid5_conf_t *conf, int num)
{
	struct kmem_cache *sc;
	int devs = max(conf->raid_disks, conf->previous_raid_disks);

	sprintf(conf->cache_name[0],
		"raid%d-%s", conf->level, mdname(conf->mddev));
	sprintf(conf->cache_name[1],
		"raid%d-%s-alt", conf->level, mdname(conf->mddev));
	conf->active_name = 0;
	sc = kmem_cache_create(conf->cache_name[conf->active_name],
			       sizeof(struct stripe_head)+(devs-1)*sizeof(struct r5dev),
			       0, 0, NULL);
	if (!sc)
		return 1;
	conf->slab_cache = sc;
	conf->pool_size = devs;
	while (num--)
		if (!grow_one_stripe(conf))
			return 1;
	return 0;
}

/**
 * scribble_len - return the required size of the scribble region
 * @num - total number of disks in the array
 *
 * The size must be enough to contain:
 * 1/ a struct page pointer for each device in the array +2
 * 2/ room to convert each entry in (1) to its corresponding dma
 *    (dma_map_page()) or page (page_address()) address.
 *
 * Note: the +2 is for the destination buffers of the ddf/raid6 case where we
 * calculate over all devices (not just the data blocks), using zeros in place
 * of the P and Q blocks.
 */
static size_t scribble_len(int num)
{
	size_t len;

	len = sizeof(struct page *) * (num+2) + sizeof(addr_conv_t) * (num+2);

	return len;
}

static int resize_stripes(raid5_conf_t *conf, int newsize)
{
	/* Make all the stripes able to hold 'newsize' devices.
	 * New slots in each stripe get 'page' set to a new page.
	 *
	 * This happens in stages:
	 * 1/ create a new kmem_cache and allocate the required number of
	 *    stripe_heads.
	 * 2/ gather all the old stripe_heads and tranfer the pages across
	 *    to the new stripe_heads.  This will have the side effect of
	 *    freezing the array as once all stripe_heads have been collected,
	 *    no IO will be possible.  Old stripe heads are freed once their
	 *    pages have been transferred over, and the old kmem_cache is
	 *    freed when all stripes are done.
	 * 3/ reallocate conf->disks to be suitable bigger.  If this fails,
	 *    we simple return a failre status - no need to clean anything up.
	 * 4/ allocate new pages for the new slots in the new stripe_heads.
	 *    If this fails, we don't bother trying the shrink the
	 *    stripe_heads down again, we just leave them as they are.
	 *    As each stripe_head is processed the new one is released into
	 *    active service.
	 *
	 * Once step2 is started, we cannot afford to wait for a write,
	 * so we use GFP_NOIO allocations.
	 */
	struct stripe_head *osh, *nsh;
	LIST_HEAD(newstripes);
	struct disk_info *ndisks;
	unsigned long cpu;
	int err;
	struct kmem_cache *sc;
	int i;

	if (newsize <= conf->pool_size)
		return 0; /* never bother to shrink */

	err = md_allow_write(conf->mddev);
	if (err)
		return err;

	/* Step 1 */
	sc = kmem_cache_create(conf->cache_name[1-conf->active_name],
			       sizeof(struct stripe_head)+(newsize-1)*sizeof(struct r5dev),
			       0, 0, NULL);
	if (!sc)
		return -ENOMEM;

	for (i = conf->max_nr_stripes; i; i--) {
		nsh = kmem_cache_alloc(sc, GFP_KERNEL);
		if (!nsh)
			break;

		memset(nsh, 0, sizeof(*nsh) + (newsize-1)*sizeof(struct r5dev));

		nsh->raid_conf = conf;
		spin_lock_init(&nsh->lock);
		#ifdef CONFIG_MULTICORE_RAID456
		init_waitqueue_head(&nsh->ops.wait_for_ops);
		#endif

		list_add(&nsh->lru, &newstripes);
	}
	if (i) {
		/* didn't get enough, give up */
		while (!list_empty(&newstripes)) {
			nsh = list_entry(newstripes.next, struct stripe_head, lru);
			list_del(&nsh->lru);
			kmem_cache_free(sc, nsh);
		}
		kmem_cache_destroy(sc);
		return -ENOMEM;
	}
	/* Step 2 - Must use GFP_NOIO now.
	 * OK, we have enough stripes, start collecting inactive
	 * stripes and copying them over
	 */
	list_for_each_entry(nsh, &newstripes, lru) {
		spin_lock_irq(&conf->device_lock);
		wait_event_lock_irq(conf->wait_for_stripe,
				    !list_empty(&conf->inactive_list),
				    conf->device_lock,
				    unplug_slaves(conf->mddev)
			);
		osh = get_free_stripe(conf);
		spin_unlock_irq(&conf->device_lock);
		atomic_set(&nsh->count, 1);
		for(i=0; i<conf->pool_size; i++)
			nsh->dev[i].page = osh->dev[i].page;
		for( ; i<newsize; i++)
			nsh->dev[i].page = NULL;
		kmem_cache_free(conf->slab_cache, osh);
	}
	kmem_cache_destroy(conf->slab_cache);

	/* Step 3.
	 * At this point, we are holding all the stripes so the array
	 * is completely stalled, so now is a good time to resize
	 * conf->disks and the scribble region
	 */
	ndisks = kzalloc(newsize * sizeof(struct disk_info), GFP_NOIO);
	if (ndisks) {
		for (i=0; i<conf->raid_disks; i++)
			ndisks[i] = conf->disks[i];
		kfree(conf->disks);
		conf->disks = ndisks;
	} else
		err = -ENOMEM;

	get_online_cpus();
	conf->scribble_len = scribble_len(newsize);
	for_each_present_cpu(cpu) {
		struct raid5_percpu *percpu;
		void *scribble;

		percpu = per_cpu_ptr(conf->percpu, cpu);
		scribble = kmalloc(conf->scribble_len, GFP_NOIO);

		if (scribble) {
			kfree(percpu->scribble);
			percpu->scribble = scribble;
		} else {
			err = -ENOMEM;
			break;
		}
	}
	put_online_cpus();

	/* Step 4, return new stripes to service */
	while(!list_empty(&newstripes)) {
		nsh = list_entry(newstripes.next, struct stripe_head, lru);
		list_del_init(&nsh->lru);

		for (i=conf->raid_disks; i < newsize; i++)
			if (nsh->dev[i].page == NULL) {
				struct page *p = alloc_page(GFP_NOIO);
				nsh->dev[i].page = p;
				if (!p)
					err = -ENOMEM;
			}
		release_stripe(nsh);
	}
	/* critical section pass, GFP_NOIO no longer needed */

	conf->slab_cache = sc;
	conf->active_name = 1-conf->active_name;
	conf->pool_size = newsize;
	return err;
}

static int drop_one_stripe(raid5_conf_t *conf)
{
	struct stripe_head *sh;

	spin_lock_irq(&conf->device_lock);
	sh = get_free_stripe(conf);
	spin_unlock_irq(&conf->device_lock);
	if (!sh)
		return 0;
	BUG_ON(atomic_read(&sh->count));
	shrink_buffers(sh, conf->pool_size);
	kmem_cache_free(conf->slab_cache, sh);
	atomic_dec(&conf->active_stripes);
	return 1;
}

static void shrink_stripes(raid5_conf_t *conf)
{
	while (drop_one_stripe(conf))
		;

	if (conf->slab_cache)
		kmem_cache_destroy(conf->slab_cache);
	conf->slab_cache = NULL;
}

static void raid5_end_read_request(struct bio * bi, int error)
{
	struct stripe_head *sh = bi->bi_private;
	raid5_conf_t *conf = sh->raid_conf;
	int disks = sh->disks, i;
	int uptodate = test_bit(BIO_UPTODATE, &bi->bi_flags);
	char b[BDEVNAME_SIZE];
	mdk_rdev_t *rdev;


	for (i=0 ; i<disks; i++)
		if (bi == &sh->dev[i].req)
			break;

	pr_debug("end_read_request %llu/%d, count: %d, uptodate %d.\n",
		(unsigned long long)sh->sector, i, atomic_read(&sh->count),
		uptodate);
	if (i == disks) {
		BUG();
		return;
	}

	if (uptodate) {
		set_bit(R5_UPTODATE, &sh->dev[i].flags);
		if (test_bit(R5_ReadError, &sh->dev[i].flags)) {
			rdev = conf->disks[i].rdev;
			printk_rl(KERN_INFO "raid5:%s: read error corrected"
				  " (%lu sectors at %llu on %s)\n",
				  mdname(conf->mddev), STRIPE_SECTORS,
				  (unsigned long long)(sh->sector
						       + rdev->data_offset),
				  bdevname(rdev->bdev, b));
			clear_bit(R5_ReadError, &sh->dev[i].flags);
			clear_bit(R5_ReWrite, &sh->dev[i].flags);
		}
		if (atomic_read(&conf->disks[i].rdev->read_errors))
			atomic_set(&conf->disks[i].rdev->read_errors, 0);
	} else {
		const char *bdn = bdevname(conf->disks[i].rdev->bdev, b);
		int retry = 0;
		rdev = conf->disks[i].rdev;

		clear_bit(R5_UPTODATE, &sh->dev[i].flags);
		atomic_inc(&rdev->read_errors);
		if (conf->mddev->degraded)
			printk_rl(KERN_WARNING
				  "raid5:%s: read error not correctable "
				  "(sector %llu on %s).\n",
				  mdname(conf->mddev),
				  (unsigned long long)(sh->sector
						       + rdev->data_offset),
				  bdn);
		else if (test_bit(R5_ReWrite, &sh->dev[i].flags))
			/* Oh, no!!! */
			printk_rl(KERN_WARNING
				  "raid5:%s: read error NOT corrected!! "
				  "(sector %llu on %s).\n",
				  mdname(conf->mddev),
				  (unsigned long long)(sh->sector
						       + rdev->data_offset),
				  bdn);
		else if (atomic_read(&rdev->read_errors)
			 > conf->max_nr_stripes)
			printk(KERN_WARNING
			       "raid5:%s: Too many read errors, failing device %s.\n",
			       mdname(conf->mddev), bdn);
		else
			retry = 1;
		if (retry)
			set_bit(R5_ReadError, &sh->dev[i].flags);
		else {
			clear_bit(R5_ReadError, &sh->dev[i].flags);
			clear_bit(R5_ReWrite, &sh->dev[i].flags);
			md_error(conf->mddev, rdev);
		}
	}
	rdev_dec_pending(conf->disks[i].rdev, conf->mddev);
	clear_bit(R5_LOCKED, &sh->dev[i].flags);
	set_bit(STRIPE_HANDLE, &sh->state);
	release_stripe(sh);
}

static void raid5_end_write_request(struct bio *bi, int error)
{
	struct stripe_head *sh = bi->bi_private;
	raid5_conf_t *conf = sh->raid_conf;
	int disks = sh->disks, i;
	int uptodate = test_bit(BIO_UPTODATE, &bi->bi_flags);

	for (i=0 ; i<disks; i++)
		if (bi == &sh->dev[i].req)
			break;

	pr_debug("end_write_request %llu/%d, count %d, uptodate: %d.\n",
		(unsigned long long)sh->sector, i, atomic_read(&sh->count),
		uptodate);
	if (i == disks) {
		BUG();
		return;
	}

	if (!uptodate)
		md_error(conf->mddev, conf->disks[i].rdev);

	rdev_dec_pending(conf->disks[i].rdev, conf->mddev);
	
	clear_bit(R5_LOCKED, &sh->dev[i].flags);
	set_bit(STRIPE_HANDLE, &sh->state);
	release_stripe(sh);
}


static sector_t compute_blocknr(struct stripe_head *sh, int i, int previous);
	
static void raid5_build_block(struct stripe_head *sh, int i, int previous)
{
	struct r5dev *dev = &sh->dev[i];

	bio_init(&dev->req);
	dev->req.bi_io_vec = &dev->vec;
	dev->req.bi_vcnt++;
	dev->req.bi_max_vecs++;
	dev->vec.bv_page = dev->page;
	dev->vec.bv_len = STRIPE_SIZE;
	dev->vec.bv_offset = 0;

	dev->req.bi_sector = sh->sector;
	dev->req.bi_private = sh;

	dev->flags = 0;
	dev->sector = compute_blocknr(sh, i, previous);
}

static void error(mddev_t *mddev, mdk_rdev_t *rdev)
{
	char b[BDEVNAME_SIZE];
	raid5_conf_t *conf = (raid5_conf_t *) mddev->private;
	pr_debug("raid5: error called\n");

	if (!test_bit(Faulty, &rdev->flags)) {
		set_bit(MD_CHANGE_DEVS, &mddev->flags);
		if (test_and_clear_bit(In_sync, &rdev->flags)) {
			unsigned long flags;
			spin_lock_irqsave(&conf->device_lock, flags);
			mddev->degraded++;
			spin_unlock_irqrestore(&conf->device_lock, flags);
			/*
			 * if recovery was running, make sure it aborts.
			 */
			set_bit(MD_RECOVERY_INTR, &mddev->recovery);
		}
		set_bit(Faulty, &rdev->flags);
		printk(KERN_ALERT
		       "raid5: Disk failure on %s, disabling device.\n"
		       "raid5: Operation continuing on %d devices.\n",
		       bdevname(rdev->bdev,b), conf->raid_disks - mddev->degraded);
	}
}

/*
 * Input: a 'big' sector number,
 * Output: index of the data and parity disk, and the sector # in them.
 */
static sector_t raid5_compute_sector(raid5_conf_t *conf, sector_t r_sector,
				     int previous, int *dd_idx,
				     struct stripe_head *sh)
{
	long stripe;
	unsigned long chunk_number;
	unsigned int chunk_offset;
	int pd_idx, qd_idx;
	int ddf_layout = 0;
	sector_t new_sector;
	int algorithm = previous ? conf->prev_algo
				 : conf->algorithm;
	int sectors_per_chunk = previous ? conf->prev_chunk_sectors
					 : conf->chunk_sectors;
	int raid_disks = previous ? conf->previous_raid_disks
				  : conf->raid_disks;
	int data_disks = raid_disks - conf->max_degraded;

	/* First compute the information on this sector */

	/*
	 * Compute the chunk number and the sector offset inside the chunk
	 */
	chunk_offset = sector_div(r_sector, sectors_per_chunk);
	chunk_number = r_sector;
	BUG_ON(r_sector != chunk_number);

	/*
	 * Compute the stripe number
	 */
	stripe = chunk_number / data_disks;

	/*
	 * Compute the data disk and parity disk indexes inside the stripe
	 */
	*dd_idx = chunk_number % data_disks;

	/*
	 * Select the parity disk based on the user selected algorithm.
	 */
	pd_idx = qd_idx = ~0;
	switch(conf->level) {
	case 4:
		pd_idx = data_disks;
		break;
	case 5:
		switch (algorithm) {
		case ALGORITHM_LEFT_ASYMMETRIC:
			pd_idx = data_disks - stripe % raid_disks;
			if (*dd_idx >= pd_idx)
				(*dd_idx)++;
			break;
		case ALGORITHM_RIGHT_ASYMMETRIC:
			pd_idx = stripe % raid_disks;
			if (*dd_idx >= pd_idx)
				(*dd_idx)++;
			break;
		case ALGORITHM_LEFT_SYMMETRIC:
			pd_idx = data_disks - stripe % raid_disks;
			*dd_idx = (pd_idx + 1 + *dd_idx) % raid_disks;
			break;
		case ALGORITHM_RIGHT_SYMMETRIC:
			pd_idx = stripe % raid_disks;
			*dd_idx = (pd_idx + 1 + *dd_idx) % raid_disks;
			break;
		case ALGORITHM_PARITY_0:
			pd_idx = 0;
			(*dd_idx)++;
			break;
		case ALGORITHM_PARITY_N:
			pd_idx = data_disks;
			break;
		default:
			printk(KERN_ERR "raid5: unsupported algorithm %d\n",
				algorithm);
			BUG();
		}
		break;
	case 6:

		switch (algorithm) {
		case ALGORITHM_LEFT_ASYMMETRIC:
			pd_idx = raid_disks - 1 - (stripe % raid_disks);
			qd_idx = pd_idx + 1;
			if (pd_idx == raid_disks-1) {
				(*dd_idx)++;	/* Q D D D P */
				qd_idx = 0;
			} else if (*dd_idx >= pd_idx)
				(*dd_idx) += 2; /* D D P Q D */
			break;
		case ALGORITHM_RIGHT_ASYMMETRIC:
			pd_idx = stripe % raid_disks;
			qd_idx = pd_idx + 1;
			if (pd_idx == raid_disks-1) {
				(*dd_idx)++;	/* Q D D D P */
				qd_idx = 0;
			} else if (*dd_idx >= pd_idx)
				(*dd_idx) += 2; /* D D P Q D */
			break;
		case ALGORITHM_LEFT_SYMMETRIC:
			pd_idx = raid_disks - 1 - (stripe % raid_disks);
			qd_idx = (pd_idx + 1) % raid_disks;
			*dd_idx = (pd_idx + 2 + *dd_idx) % raid_disks;
			break;
		case ALGORITHM_RIGHT_SYMMETRIC:
			pd_idx = stripe % raid_disks;
			qd_idx = (pd_idx + 1) % raid_disks;
			*dd_idx = (pd_idx + 2 + *dd_idx) % raid_disks;
			break;

		case ALGORITHM_PARITY_0:
			pd_idx = 0;
			qd_idx = 1;
			(*dd_idx) += 2;
			break;
		case ALGORITHM_PARITY_N:
			pd_idx = data_disks;
			qd_idx = data_disks + 1;
			break;

		case ALGORITHM_ROTATING_ZERO_RESTART:
			/* Exactly the same as RIGHT_ASYMMETRIC, but or
			 * of blocks for computing Q is different.
			 */
			pd_idx = stripe % raid_disks;
			qd_idx = pd_idx + 1;
			if (pd_idx == raid_disks-1) {
				(*dd_idx)++;	/* Q D D D P */
				qd_idx = 0;
			} else if (*dd_idx >= pd_idx)
				(*dd_idx) += 2; /* D D P Q D */
			ddf_layout = 1;
			break;

		case ALGORITHM_ROTATING_N_RESTART:
			/* Same a left_asymmetric, by first stripe is
			 * D D D P Q  rather than
			 * Q D D D P
			 */
			pd_idx = raid_disks - 1 - ((stripe + 1) % raid_disks);
			qd_idx = pd_idx + 1;
			if (pd_idx == raid_disks-1) {
				(*dd_idx)++;	/* Q D D D P */
				qd_idx = 0;
			} else if (*dd_idx >= pd_idx)
				(*dd_idx) += 2; /* D D P Q D */
			ddf_layout = 1;
			break;

		case ALGORITHM_ROTATING_N_CONTINUE:
			/* Same as left_symmetric but Q is before P */
			pd_idx = raid_disks - 1 - (stripe % raid_disks);
			qd_idx = (pd_idx + raid_disks - 1) % raid_disks;
			*dd_idx = (pd_idx + 1 + *dd_idx) % raid_disks;
			ddf_layout = 1;
			break;

		case ALGORITHM_LEFT_ASYMMETRIC_6:
			/* RAID5 left_asymmetric, with Q on last device */
			pd_idx = data_disks - stripe % (raid_disks-1);
			if (*dd_idx >= pd_idx)
				(*dd_idx)++;
			qd_idx = raid_disks - 1;
			break;

		case ALGORITHM_RIGHT_ASYMMETRIC_6:
			pd_idx = stripe % (raid_disks-1);
			if (*dd_idx >= pd_idx)
				(*dd_idx)++;
			qd_idx = raid_disks - 1;
			break;

		case ALGORITHM_LEFT_SYMMETRIC_6:
			pd_idx = data_disks - stripe % (raid_disks-1);
			*dd_idx = (pd_idx + 1 + *dd_idx) % (raid_disks-1);
			qd_idx = raid_disks - 1;
			break;

		case ALGORITHM_RIGHT_SYMMETRIC_6:
			pd_idx = stripe % (raid_disks-1);
			*dd_idx = (pd_idx + 1 + *dd_idx) % (raid_disks-1);
			qd_idx = raid_disks - 1;
			break;

		case ALGORITHM_PARITY_0_6:
			pd_idx = 0;
			(*dd_idx)++;
			qd_idx = raid_disks - 1;
			break;


		default:
			printk(KERN_CRIT "raid6: unsupported algorithm %d\n",
			       algorithm);
			BUG();
		}
		break;
	}

	if (sh) {
		sh->pd_idx = pd_idx;
		sh->qd_idx = qd_idx;
		sh->ddf_layout = ddf_layout;
	}
	/*
	 * Finally, compute the new sector number
	 */
	new_sector = (sector_t)stripe * sectors_per_chunk + chunk_offset;
	return new_sector;
}


static sector_t compute_blocknr(struct stripe_head *sh, int i, int previous)
{
	raid5_conf_t *conf = sh->raid_conf;
	int raid_disks = sh->disks;
	int data_disks = raid_disks - conf->max_degraded;
	sector_t new_sector = sh->sector, check;
	int sectors_per_chunk = previous ? conf->prev_chunk_sectors
					 : conf->chunk_sectors;
	int algorithm = previous ? conf->prev_algo
				 : conf->algorithm;
	sector_t stripe;
	int chunk_offset;
	int chunk_number, dummy1, dd_idx = i;
	sector_t r_sector;
	struct stripe_head sh2;


	chunk_offset = sector_div(new_sector, sectors_per_chunk);
	stripe = new_sector;
	BUG_ON(new_sector != stripe);

	if (i == sh->pd_idx)
		return 0;
	switch(conf->level) {
	case 4: break;
	case 5:
		switch (algorithm) {
		case ALGORITHM_LEFT_ASYMMETRIC:
		case ALGORITHM_RIGHT_ASYMMETRIC:
			if (i > sh->pd_idx)
				i--;
			break;
		case ALGORITHM_LEFT_SYMMETRIC:
		case ALGORITHM_RIGHT_SYMMETRIC:
			if (i < sh->pd_idx)
				i += raid_disks;
			i -= (sh->pd_idx + 1);
			break;
		case ALGORITHM_PARITY_0:
			i -= 1;
			break;
		case ALGORITHM_PARITY_N:
			break;
		default:
			printk(KERN_ERR "raid5: unsupported algorithm %d\n",
			       algorithm);
			BUG();
		}
		break;
	case 6:
		if (i == sh->qd_idx)
			return 0; /* It is the Q disk */
		switch (algorithm) {
		case ALGORITHM_LEFT_ASYMMETRIC:
		case ALGORITHM_RIGHT_ASYMMETRIC:
		case ALGORITHM_ROTATING_ZERO_RESTART:
		case ALGORITHM_ROTATING_N_RESTART:
			if (sh->pd_idx == raid_disks-1)
				i--;	/* Q D D D P */
			else if (i > sh->pd_idx)
				i -= 2; /* D D P Q D */
			break;
		case ALGORITHM_LEFT_SYMMETRIC:
		case ALGORITHM_RIGHT_SYMMETRIC:
			if (sh->pd_idx == raid_disks-1)
				i--; /* Q D D D P */
			else {
				/* D D P Q D */
				if (i < sh->pd_idx)
					i += raid_disks;
				i -= (sh->pd_idx + 2);
			}
			break;
		case ALGORITHM_PARITY_0:
			i -= 2;
			break;
		case ALGORITHM_PARITY_N:
			break;
		case ALGORITHM_ROTATING_N_CONTINUE:
			/* Like left_symmetric, but P is before Q */
			if (sh->pd_idx == 0)
				i--;	/* P D D D Q */
			else {
				/* D D Q P D */
				if (i < sh->pd_idx)
					i += raid_disks;
				i -= (sh->pd_idx + 1);
			}
			break;
		case ALGORITHM_LEFT_ASYMMETRIC_6:
		case ALGORITHM_RIGHT_ASYMMETRIC_6:
			if (i > sh->pd_idx)
				i--;
			break;
		case ALGORITHM_LEFT_SYMMETRIC_6:
		case ALGORITHM_RIGHT_SYMMETRIC_6:
			if (i < sh->pd_idx)
				i += data_disks + 1;
			i -= (sh->pd_idx + 1);
			break;
		case ALGORITHM_PARITY_0_6:
			i -= 1;
			break;
		default:
			printk(KERN_CRIT "raid6: unsupported algorithm %d\n",
			       algorithm);
			BUG();
		}
		break;
	}

	chunk_number = stripe * data_disks + i;
	r_sector = (sector_t)chunk_number * sectors_per_chunk + chunk_offset;

	check = raid5_compute_sector(conf, r_sector,
				     previous, &dummy1, &sh2);
	if (check != sh->sector || dummy1 != dd_idx || sh2.pd_idx != sh->pd_idx
		|| sh2.qd_idx != sh->qd_idx) {
		printk(KERN_ERR "compute_blocknr: map not correct\n");
		return 0;
	}
	return r_sector;
}


static void
schedule_reconstruction(struct stripe_head *sh, struct stripe_head_state *s,
			 int rcw, int expand)
{
	int i, pd_idx = sh->pd_idx, disks = sh->disks;
	raid5_conf_t *conf = sh->raid_conf;
	int level = conf->level;

	if (rcw) {
		/* if we are not expanding this is a proper write request, and
		 * there will be bios with new data to be drained into the
		 * stripe cache
		 */
		if (!expand) {
			sh->reconstruct_state = reconstruct_state_drain_run;
			set_bit(STRIPE_OP_BIODRAIN, &s->ops_request);
		} else
			sh->reconstruct_state = reconstruct_state_run;

		set_bit(STRIPE_OP_RECONSTRUCT, &s->ops_request);

		for (i = disks; i--; ) {
			struct r5dev *dev = &sh->dev[i];

			if (dev->towrite) {
				set_bit(R5_LOCKED, &dev->flags);
				set_bit(R5_Wantdrain, &dev->flags);
				if (!expand)
					clear_bit(R5_UPTODATE, &dev->flags);
				s->locked++;
			}
		}
		if (s->locked + conf->max_degraded == disks)
			if (!test_and_set_bit(STRIPE_FULL_WRITE, &sh->state))
				atomic_inc(&conf->pending_full_writes);
	} else {
		BUG_ON(level == 6);
		BUG_ON(!(test_bit(R5_UPTODATE, &sh->dev[pd_idx].flags) ||
			test_bit(R5_Wantcompute, &sh->dev[pd_idx].flags)));

		sh->reconstruct_state = reconstruct_state_prexor_drain_run;
		set_bit(STRIPE_OP_PREXOR, &s->ops_request);
		set_bit(STRIPE_OP_BIODRAIN, &s->ops_request);
		set_bit(STRIPE_OP_RECONSTRUCT, &s->ops_request);

		for (i = disks; i--; ) {
			struct r5dev *dev = &sh->dev[i];
			if (i == pd_idx)
				continue;

			if (dev->towrite &&
			    (test_bit(R5_UPTODATE, &dev->flags) ||
			     test_bit(R5_Wantcompute, &dev->flags))) {
				set_bit(R5_Wantdrain, &dev->flags);
				set_bit(R5_LOCKED, &dev->flags);
				clear_bit(R5_UPTODATE, &dev->flags);
				s->locked++;
			}
		}
	}

	/* keep the parity disk(s) locked while asynchronous operations
	 * are in flight
	 */
	set_bit(R5_LOCKED, &sh->dev[pd_idx].flags);
	clear_bit(R5_UPTODATE, &sh->dev[pd_idx].flags);
	s->locked++;

	if (level == 6) {
		int qd_idx = sh->qd_idx;
		struct r5dev *dev = &sh->dev[qd_idx];

		set_bit(R5_LOCKED, &dev->flags);
		clear_bit(R5_UPTODATE, &dev->flags);
		s->locked++;
	}

	pr_debug("%s: stripe %llu locked: %d ops_request: %lx\n",
		__func__, (unsigned long long)sh->sector,
		s->locked, s->ops_request);
}

/*
 * Each stripe/dev can have one or more bion attached.
 * toread/towrite point to the first in a chain.
 * The bi_next chain must be in order.
 */
static int add_stripe_bio(struct stripe_head *sh, struct bio *bi, int dd_idx, int forwrite)
{
	struct bio **bip;
	raid5_conf_t *conf = sh->raid_conf;
	int firstwrite=0;

	pr_debug("adding bh b#%llu to stripe s#%llu\n",
		(unsigned long long)bi->bi_sector,
		(unsigned long long)sh->sector);


	spin_lock(&sh->lock);
	spin_lock_irq(&conf->device_lock);
	if (forwrite) {
		bip = &sh->dev[dd_idx].towrite;
		if (*bip == NULL && sh->dev[dd_idx].written == NULL)
			firstwrite = 1;
	} else
		bip = &sh->dev[dd_idx].toread;
	while (*bip && (*bip)->bi_sector < bi->bi_sector) {
		if ((*bip)->bi_sector + ((*bip)->bi_size >> 9) > bi->bi_sector)
			goto overlap;
		bip = & (*bip)->bi_next;
	}
	if (*bip && (*bip)->bi_sector < bi->bi_sector + ((bi->bi_size)>>9))
		goto overlap;

	BUG_ON(*bip && bi->bi_next && (*bip) != bi->bi_next);
	if (*bip)
		bi->bi_next = *bip;
	*bip = bi;
	bi->bi_phys_segments++;
	spin_unlock_irq(&conf->device_lock);
	spin_unlock(&sh->lock);

	pr_debug("added bi b#%llu to stripe s#%llu, disk %d.\n",
		(unsigned long long)bi->bi_sector,
		(unsigned long long)sh->sector, dd_idx);

	if (conf->mddev->bitmap && firstwrite) {
		bitmap_startwrite(conf->mddev->bitmap, sh->sector,
				  STRIPE_SECTORS, 0);
		sh->bm_seq = conf->seq_flush+1;
		set_bit(STRIPE_BIT_DELAY, &sh->state);
	}

	if (forwrite) {
		/* check if page is covered */
		sector_t sector = sh->dev[dd_idx].sector;
		for (bi=sh->dev[dd_idx].towrite;
		     sector < sh->dev[dd_idx].sector + STRIPE_SECTORS &&
			     bi && bi->bi_sector <= sector;
		     bi = r5_next_bio(bi, sh->dev[dd_idx].sector)) {
			if (bi->bi_sector + (bi->bi_size>>9) >= sector)
				sector = bi->bi_sector + (bi->bi_size>>9);
		}
		if (sector >= sh->dev[dd_idx].sector + STRIPE_SECTORS)
			set_bit(R5_OVERWRITE, &sh->dev[dd_idx].flags);
	}
	return 1;

 overlap:
	set_bit(R5_Overlap, &sh->dev[dd_idx].flags);
	spin_unlock_irq(&conf->device_lock);
	spin_unlock(&sh->lock);
	return 0;
}

static void end_reshape(raid5_conf_t *conf);

static void stripe_set_idx(sector_t stripe, raid5_conf_t *conf, int previous,
			    struct stripe_head *sh)
{
	int sectors_per_chunk =
		previous ? conf->prev_chunk_sectors : conf->chunk_sectors;
	int dd_idx;
	int chunk_offset = sector_div(stripe, sectors_per_chunk);
	int disks = previous ? conf->previous_raid_disks : conf->raid_disks;

	raid5_compute_sector(conf,
			     stripe * (disks - conf->max_degraded)
			     *sectors_per_chunk + chunk_offset,
			     previous,
			     &dd_idx, sh);
}

static void
handle_failed_stripe(raid5_conf_t *conf, struct stripe_head *sh,
				struct stripe_head_state *s, int disks,
				struct bio **return_bi)
{
	int i;
	for (i = disks; i--; ) {
		struct bio *bi;
		int bitmap_end = 0;

		if (test_bit(R5_ReadError, &sh->dev[i].flags)) {
			mdk_rdev_t *rdev;
			rcu_read_lock();
			rdev = rcu_dereference(conf->disks[i].rdev);
			if (rdev && test_bit(In_sync, &rdev->flags))
				/* multiple read failures in one stripe */
				md_error(conf->mddev, rdev);
			rcu_read_unlock();
		}
		spin_lock_irq(&conf->device_lock);
		/* fail all writes first */
		bi = sh->dev[i].towrite;
		sh->dev[i].towrite = NULL;
		if (bi) {
			s->to_write--;
			bitmap_end = 1;
		}

		if (test_and_clear_bit(R5_Overlap, &sh->dev[i].flags))
			wake_up(&conf->wait_for_overlap);

		while (bi && bi->bi_sector <
			sh->dev[i].sector + STRIPE_SECTORS) {
			struct bio *nextbi = r5_next_bio(bi, sh->dev[i].sector);
			clear_bit(BIO_UPTODATE, &bi->bi_flags);
			if (!raid5_dec_bi_phys_segments(bi)) {
				md_write_end(conf->mddev);
				bi->bi_next = *return_bi;
				*return_bi = bi;
			}
			bi = nextbi;
		}
		/* and fail all 'written' */
		bi = sh->dev[i].written;
		sh->dev[i].written = NULL;
		if (bi) bitmap_end = 1;
		while (bi && bi->bi_sector <
		       sh->dev[i].sector + STRIPE_SECTORS) {
			struct bio *bi2 = r5_next_bio(bi, sh->dev[i].sector);
			clear_bit(BIO_UPTODATE, &bi->bi_flags);
			if (!raid5_dec_bi_phys_segments(bi)) {
				md_write_end(conf->mddev);
				bi->bi_next = *return_bi;
				*return_bi = bi;
			}
			bi = bi2;
		}

		/* fail any reads if this device is non-operational and
		 * the data has not reached the cache yet.
		 */
		if (!test_bit(R5_Wantfill, &sh->dev[i].flags) &&
		    (!test_bit(R5_Insync, &sh->dev[i].flags) ||
		      test_bit(R5_ReadError, &sh->dev[i].flags))) {
			bi = sh->dev[i].toread;
			sh->dev[i].toread = NULL;
			if (test_and_clear_bit(R5_Overlap, &sh->dev[i].flags))
				wake_up(&conf->wait_for_overlap);
			if (bi) s->to_read--;
			while (bi && bi->bi_sector <
			       sh->dev[i].sector + STRIPE_SECTORS) {
				struct bio *nextbi =
					r5_next_bio(bi, sh->dev[i].sector);
				clear_bit(BIO_UPTODATE, &bi->bi_flags);
				if (!raid5_dec_bi_phys_segments(bi)) {
					bi->bi_next = *return_bi;
					*return_bi = bi;
				}
				bi = nextbi;
			}
		}
		spin_unlock_irq(&conf->device_lock);
		if (bitmap_end)
			bitmap_endwrite(conf->mddev->bitmap, sh->sector,
					STRIPE_SECTORS, 0, 0);
	}

	if (test_and_clear_bit(STRIPE_FULL_WRITE, &sh->state))
		if (atomic_dec_and_test(&conf->pending_full_writes))
			md_wakeup_thread(conf->mddev->thread);
}

/* fetch_block5 - checks the given member device to see if its data needs
 * to be read or computed to satisfy a request.
 *
 * Returns 1 when no more member devices need to be checked, otherwise returns
 * 0 to tell the loop in handle_stripe_fill5 to continue
 */
static int fetch_block5(struct stripe_head *sh, struct stripe_head_state *s,
			int disk_idx, int disks)
{
	struct r5dev *dev = &sh->dev[disk_idx];
	struct r5dev *failed_dev = &sh->dev[s->failed_num];

	/* is the data in this block needed, and can we get it? */
	if (!test_bit(R5_LOCKED, &dev->flags) &&
	    !test_bit(R5_UPTODATE, &dev->flags) &&
	    (dev->toread ||
	     (dev->towrite && !test_bit(R5_OVERWRITE, &dev->flags)) ||
	     s->syncing || s->expanding ||
	     (s->failed &&
	      (failed_dev->toread ||
	       (failed_dev->towrite &&
		!test_bit(R5_OVERWRITE, &failed_dev->flags)))))) {
		/* We would like to get this block, possibly by computing it,
		 * otherwise read it if the backing disk is insync
		 */
		if ((s->uptodate == disks - 1) &&
		    (s->failed && disk_idx == s->failed_num)) {
			set_bit(STRIPE_COMPUTE_RUN, &sh->state);
			set_bit(STRIPE_OP_COMPUTE_BLK, &s->ops_request);
			set_bit(R5_Wantcompute, &dev->flags);
			sh->ops.target = disk_idx;
			sh->ops.target2 = -1;
			s->req_compute = 1;
			/* Careful: from this point on 'uptodate' is in the eye
			 * of raid_run_ops which services 'compute' operations
			 * before writes. R5_Wantcompute flags a block that will
			 * be R5_UPTODATE by the time it is needed for a
			 * subsequent operation.
			 */
			s->uptodate++;
			return 1; /* uptodate + compute == disks */
		} else if (test_bit(R5_Insync, &dev->flags)) {
			set_bit(R5_LOCKED, &dev->flags);
			set_bit(R5_Wantread, &dev->flags);
			s->locked++;
			pr_debug("Reading block %d (sync=%d)\n", disk_idx,
				s->syncing);
		}
	}

	return 0;
}

/**
 * handle_stripe_fill5 - read or compute data to satisfy pending requests.
 */
static void handle_stripe_fill5(struct stripe_head *sh,
			struct stripe_head_state *s, int disks)
{
	int i;

	/* look for blocks to read/compute, skip this if a compute
	 * is already in flight, or if the stripe contents are in the
	 * midst of changing due to a write
	 */
	if (!test_bit(STRIPE_COMPUTE_RUN, &sh->state) && !sh->check_state &&
	    !sh->reconstruct_state)
		for (i = disks; i--; )
			if (fetch_block5(sh, s, i, disks))
				break;
	set_bit(STRIPE_HANDLE, &sh->state);
}

/* fetch_block6 - checks the given member device to see if its data needs
 * to be read or computed to satisfy a request.
 *
 * Returns 1 when no more member devices need to be checked, otherwise returns
 * 0 to tell the loop in handle_stripe_fill6 to continue
 */
static int fetch_block6(struct stripe_head *sh, struct stripe_head_state *s,
			 struct r6_state *r6s, int disk_idx, int disks)
{
	struct r5dev *dev = &sh->dev[disk_idx];
	struct r5dev *fdev[2] = { &sh->dev[r6s->failed_num[0]],
				  &sh->dev[r6s->failed_num[1]] };

	if (!test_bit(R5_LOCKED, &dev->flags) &&
	    !test_bit(R5_UPTODATE, &dev->flags) &&
	    (dev->toread ||
	     (dev->towrite && !test_bit(R5_OVERWRITE, &dev->flags)) ||
	     s->syncing || s->expanding ||
	     (s->failed >= 1 &&
	      (fdev[0]->toread || s->to_write)) ||
	     (s->failed >= 2 &&
	      (fdev[1]->toread || s->to_write)))) {
		/* we would like to get this block, possibly by computing it,
		 * otherwise read it if the backing disk is insync
		 */
		BUG_ON(test_bit(R5_Wantcompute, &dev->flags));
		BUG_ON(test_bit(R5_Wantread, &dev->flags));
		if ((s->uptodate == disks - 1) &&
		    (s->failed && (disk_idx == r6s->failed_num[0] ||
				   disk_idx == r6s->failed_num[1]))) {
			/* have disk failed, and we're requested to fetch it;
			 * do compute it
			 */
			pr_debug("Computing stripe %llu block %d\n",
			       (unsigned long long)sh->sector, disk_idx);
			set_bit(STRIPE_COMPUTE_RUN, &sh->state);
			set_bit(STRIPE_OP_COMPUTE_BLK, &s->ops_request);
			set_bit(R5_Wantcompute, &dev->flags);
			sh->ops.target = disk_idx;
			sh->ops.target2 = -1; /* no 2nd target */
			s->req_compute = 1;
			s->uptodate++;
			return 1;
		} else if (s->uptodate == disks-2 && s->failed >= 2) {
			/* Computing 2-failure is *very* expensive; only
			 * do it if failed >= 2
			 */
			int other;
			for (other = disks; other--; ) {
				if (other == disk_idx)
					continue;
				if (!test_bit(R5_UPTODATE,
				      &sh->dev[other].flags))
					break;
			}
			BUG_ON(other < 0);
			pr_debug("Computing stripe %llu blocks %d,%d\n",
			       (unsigned long long)sh->sector,
			       disk_idx, other);
			set_bit(STRIPE_COMPUTE_RUN, &sh->state);
			set_bit(STRIPE_OP_COMPUTE_BLK, &s->ops_request);
			set_bit(R5_Wantcompute, &sh->dev[disk_idx].flags);
			set_bit(R5_Wantcompute, &sh->dev[other].flags);
			sh->ops.target = disk_idx;
			sh->ops.target2 = other;
			s->uptodate += 2;
			s->req_compute = 1;
			return 1;
		} else if (test_bit(R5_Insync, &dev->flags)) {
			set_bit(R5_LOCKED, &dev->flags);
			set_bit(R5_Wantread, &dev->flags);
			s->locked++;
			pr_debug("Reading block %d (sync=%d)\n",
				disk_idx, s->syncing);
		}
	}

	return 0;
}

/**
 * handle_stripe_fill6 - read or compute data to satisfy pending requests.
 */
static void handle_stripe_fill6(struct stripe_head *sh,
			struct stripe_head_state *s, struct r6_state *r6s,
			int disks)
{
	int i;

	/* look for blocks to read/compute, skip this if a compute
	 * is already in flight, or if the stripe contents are in the
	 * midst of changing due to a write
	 */
	if (!test_bit(STRIPE_COMPUTE_RUN, &sh->state) && !sh->check_state &&
	    !sh->reconstruct_state)
		for (i = disks; i--; )
			if (fetch_block6(sh, s, r6s, i, disks))
				break;
	set_bit(STRIPE_HANDLE, &sh->state);
}


/* handle_stripe_clean_event
 * any written block on an uptodate or failed drive can be returned.
 * Note that if we 'wrote' to a failed drive, it will be UPTODATE, but
 * never LOCKED, so we don't need to test 'failed' directly.
 */
static void handle_stripe_clean_event(raid5_conf_t *conf,
	struct stripe_head *sh, int disks, struct bio **return_bi)
{
	int i;
	struct r5dev *dev;

	for (i = disks; i--; )
		if (sh->dev[i].written) {
			dev = &sh->dev[i];
			if (!test_bit(R5_LOCKED, &dev->flags) &&
				test_bit(R5_UPTODATE, &dev->flags)) {
				/* We can return any write requests */
				struct bio *wbi, *wbi2;
				int bitmap_end = 0;
				pr_debug("Return write for disc %d\n", i);
				spin_lock_irq(&conf->device_lock);
				wbi = dev->written;
				dev->written = NULL;
				while (wbi && wbi->bi_sector <
					dev->sector + STRIPE_SECTORS) {
					wbi2 = r5_next_bio(wbi, dev->sector);
					if (!raid5_dec_bi_phys_segments(wbi)) {
						md_write_end(conf->mddev);
						wbi->bi_next = *return_bi;
						*return_bi = wbi;
					}
					wbi = wbi2;
				}
				if (dev->towrite == NULL)
					bitmap_end = 1;
				spin_unlock_irq(&conf->device_lock);
				if (bitmap_end)
					bitmap_endwrite(conf->mddev->bitmap,
							sh->sector,
							STRIPE_SECTORS,
					 !test_bit(STRIPE_DEGRADED, &sh->state),
							0);
			}
		}

	if (test_and_clear_bit(STRIPE_FULL_WRITE, &sh->state))
		if (atomic_dec_and_test(&conf->pending_full_writes))
			md_wakeup_thread(conf->mddev->thread);
}

static void handle_stripe_dirtying5(raid5_conf_t *conf,
		struct stripe_head *sh,	struct stripe_head_state *s, int disks)
{
	int rmw = 0, rcw = 0, i;
	for (i = disks; i--; ) {
		/* would I have to read this buffer for read_modify_write */
		struct r5dev *dev = &sh->dev[i];
		if ((dev->towrite || i == sh->pd_idx) &&
		    !test_bit(R5_LOCKED, &dev->flags) &&
		    !(test_bit(R5_UPTODATE, &dev->flags) ||
		      test_bit(R5_Wantcompute, &dev->flags))) {
			if (test_bit(R5_Insync, &dev->flags))
				rmw++;
			else
				rmw += 2*disks;  /* cannot read it */
		}
		/* Would I have to read this buffer for reconstruct_write */
		if (!test_bit(R5_OVERWRITE, &dev->flags) && i != sh->pd_idx &&
		    !test_bit(R5_LOCKED, &dev->flags) &&
		    !(test_bit(R5_UPTODATE, &dev->flags) ||
		    test_bit(R5_Wantcompute, &dev->flags))) {
			if (test_bit(R5_Insync, &dev->flags)) rcw++;
			else
				rcw += 2*disks;
		}
	}
	pr_debug("for sector %llu, rmw=%d rcw=%d\n",
		(unsigned long long)sh->sector, rmw, rcw);
	set_bit(STRIPE_HANDLE, &sh->state);
	if (rmw < rcw && rmw > 0)
		/* prefer read-modify-write, but need to get some data */
		for (i = disks; i--; ) {
			struct r5dev *dev = &sh->dev[i];
			if ((dev->towrite || i == sh->pd_idx) &&
			    !test_bit(R5_LOCKED, &dev->flags) &&
			    !(test_bit(R5_UPTODATE, &dev->flags) ||
			    test_bit(R5_Wantcompute, &dev->flags)) &&
			    test_bit(R5_Insync, &dev->flags)) {
				if (
				  test_bit(STRIPE_PREREAD_ACTIVE, &sh->state)) {
					pr_debug("Read_old block "
						"%d for r-m-w\n", i);
					set_bit(R5_LOCKED, &dev->flags);
					set_bit(R5_Wantread, &dev->flags);
					s->locked++;
				} else {
					set_bit(STRIPE_DELAYED, &sh->state);
					set_bit(STRIPE_HANDLE, &sh->state);
				}
			}
		}
	if (rcw <= rmw && rcw > 0)
		/* want reconstruct write, but need to get some data */
		for (i = disks; i--; ) {
			struct r5dev *dev = &sh->dev[i];
			if (!test_bit(R5_OVERWRITE, &dev->flags) &&
			    i != sh->pd_idx &&
			    !test_bit(R5_LOCKED, &dev->flags) &&
			    !(test_bit(R5_UPTODATE, &dev->flags) ||
			    test_bit(R5_Wantcompute, &dev->flags)) &&
			    test_bit(R5_Insync, &dev->flags)) {
				if (
				  test_bit(STRIPE_PREREAD_ACTIVE, &sh->state)) {
					pr_debug("Read_old block "
						"%d for Reconstruct\n", i);
					set_bit(R5_LOCKED, &dev->flags);
					set_bit(R5_Wantread, &dev->flags);
					s->locked++;
				} else {
					set_bit(STRIPE_DELAYED, &sh->state);
					set_bit(STRIPE_HANDLE, &sh->state);
				}
			}
		}
	/* now if nothing is locked, and if we have enough data,
	 * we can start a write request
	 */
	/* since handle_stripe can be called at any time we need to handle the
	 * case where a compute block operation has been submitted and then a
	 * subsequent call wants to start a write request.  raid_run_ops only
	 * handles the case where compute block and reconstruct are requested
	 * simultaneously.  If this is not the case then new writes need to be
	 * held off until the compute completes.
	 */
	if ((s->req_compute || !test_bit(STRIPE_COMPUTE_RUN, &sh->state)) &&
	    (s->locked == 0 && (rcw == 0 || rmw == 0) &&
	    !test_bit(STRIPE_BIT_DELAY, &sh->state)))
		schedule_reconstruction(sh, s, rcw == 0, 0);
}

static void handle_stripe_dirtying6(raid5_conf_t *conf,
		struct stripe_head *sh,	struct stripe_head_state *s,
		struct r6_state *r6s, int disks)
{
	int rcw = 0, pd_idx = sh->pd_idx, i;
	int qd_idx = sh->qd_idx;

	set_bit(STRIPE_HANDLE, &sh->state);
	for (i = disks; i--; ) {
		struct r5dev *dev = &sh->dev[i];
		/* check if we haven't enough data */
		if (!test_bit(R5_OVERWRITE, &dev->flags) &&
		    i != pd_idx && i != qd_idx &&
		    !test_bit(R5_LOCKED, &dev->flags) &&
		    !(test_bit(R5_UPTODATE, &dev->flags) ||
		      test_bit(R5_Wantcompute, &dev->flags))) {
			rcw++;
			if (!test_bit(R5_Insync, &dev->flags))
				continue; /* it's a failed drive */

			if (
			  test_bit(STRIPE_PREREAD_ACTIVE, &sh->state)) {
				pr_debug("Read_old stripe %llu "
					"block %d for Reconstruct\n",
				     (unsigned long long)sh->sector, i);
				set_bit(R5_LOCKED, &dev->flags);
				set_bit(R5_Wantread, &dev->flags);
				s->locked++;
			} else {
				pr_debug("Request delayed stripe %llu "
					"block %d for Reconstruct\n",
				     (unsigned long long)sh->sector, i);
				set_bit(STRIPE_DELAYED, &sh->state);
				set_bit(STRIPE_HANDLE, &sh->state);
			}
		}
	}
	/* now if nothing is locked, and if we have enough data, we can start a
	 * write request
	 */
	if ((s->req_compute || !test_bit(STRIPE_COMPUTE_RUN, &sh->state)) &&
	    s->locked == 0 && rcw == 0 &&
	    !test_bit(STRIPE_BIT_DELAY, &sh->state)) {
		schedule_reconstruction(sh, s, 1, 0);
	}
}

static void handle_parity_checks5(raid5_conf_t *conf, struct stripe_head *sh,
				struct stripe_head_state *s, int disks)
{
	struct r5dev *dev = NULL;

	set_bit(STRIPE_HANDLE, &sh->state);

	switch (sh->check_state) {
	case check_state_idle:
		/* start a new check operation if there are no failures */
		if (s->failed == 0) {
			BUG_ON(s->uptodate != disks);
			sh->check_state = check_state_run;
			set_bit(STRIPE_OP_CHECK, &s->ops_request);
			clear_bit(R5_UPTODATE, &sh->dev[sh->pd_idx].flags);
			s->uptodate--;
			break;
		}
		dev = &sh->dev[s->failed_num];
		/* fall through */
	case check_state_compute_result:
		sh->check_state = check_state_idle;
		if (!dev)
			dev = &sh->dev[sh->pd_idx];

		/* check that a write has not made the stripe insync */
		if (test_bit(STRIPE_INSYNC, &sh->state))
			break;

		/* either failed parity check, or recovery is happening */
		BUG_ON(!test_bit(R5_UPTODATE, &dev->flags));
		BUG_ON(s->uptodate != disks);

		set_bit(R5_LOCKED, &dev->flags);
		s->locked++;
		set_bit(R5_Wantwrite, &dev->flags);

		clear_bit(STRIPE_DEGRADED, &sh->state);
		set_bit(STRIPE_INSYNC, &sh->state);
		break;
	case check_state_run:
		break; /* we will be called again upon completion */
	case check_state_check_result:
		sh->check_state = check_state_idle;

		/* if a failure occurred during the check operation, leave
		 * STRIPE_INSYNC not set and let the stripe be handled again
		 */
		if (s->failed)
			break;

		/* handle a successful check operation, if parity is correct
		 * we are done.  Otherwise update the mismatch count and repair
		 * parity if !MD_RECOVERY_CHECK
		 */
		if ((sh->ops.zero_sum_result & SUM_CHECK_P_RESULT) == 0)
			/* parity is correct (on disc,
			 * not in buffer any more)
			 */
			set_bit(STRIPE_INSYNC, &sh->state);
		else {
			conf->mddev->resync_mismatches += STRIPE_SECTORS;
			if (test_bit(MD_RECOVERY_CHECK, &conf->mddev->recovery))
				/* don't try to repair!! */
				set_bit(STRIPE_INSYNC, &sh->state);
			else {
				sh->check_state = check_state_compute_run;
				set_bit(STRIPE_COMPUTE_RUN, &sh->state);
				set_bit(STRIPE_OP_COMPUTE_BLK, &s->ops_request);
				set_bit(R5_Wantcompute,
					&sh->dev[sh->pd_idx].flags);
				sh->ops.target = sh->pd_idx;
				sh->ops.target2 = -1;
				s->uptodate++;
			}
		}
		break;
	case check_state_compute_run:
		break;
	default:
		printk(KERN_ERR "%s: unknown check_state: %d sector: %llu\n",
		       __func__, sh->check_state,
		       (unsigned long long) sh->sector);
		BUG();
	}
}


static void handle_parity_checks6(raid5_conf_t *conf, struct stripe_head *sh,
				  struct stripe_head_state *s,
				  struct r6_state *r6s, int disks)
{
	int pd_idx = sh->pd_idx;
	int qd_idx = sh->qd_idx;
	struct r5dev *dev;

	set_bit(STRIPE_HANDLE, &sh->state);

	BUG_ON(s->failed > 2);

	/* Want to check and possibly repair P and Q.
	 * However there could be one 'failed' device, in which
	 * case we can only check one of them, possibly using the
	 * other to generate missing data
	 */

	switch (sh->check_state) {
	case check_state_idle:
		/* start a new check operation if there are < 2 failures */
		if (s->failed == r6s->q_failed) {
			/* The only possible failed device holds Q, so it
			 * makes sense to check P (If anything else were failed,
			 * we would have used P to recreate it).
			 */
			sh->check_state = check_state_run;
		}
		if (!r6s->q_failed && s->failed < 2) {
			/* Q is not failed, and we didn't use it to generate
			 * anything, so it makes sense to check it
			 */
			if (sh->check_state == check_state_run)
				sh->check_state = check_state_run_pq;
			else
				sh->check_state = check_state_run_q;
		}

		/* discard potentially stale zero_sum_result */
		sh->ops.zero_sum_result = 0;

		if (sh->check_state == check_state_run) {
			/* async_xor_zero_sum destroys the contents of P */
			clear_bit(R5_UPTODATE, &sh->dev[pd_idx].flags);
			s->uptodate--;
		}
		if (sh->check_state >= check_state_run &&
		    sh->check_state <= check_state_run_pq) {
			/* async_syndrome_zero_sum preserves P and Q, so
			 * no need to mark them !uptodate here
			 */
			set_bit(STRIPE_OP_CHECK, &s->ops_request);
			break;
		}

		/* we have 2-disk failure */
		BUG_ON(s->failed != 2);
		/* fall through */
	case check_state_compute_result:
		sh->check_state = check_state_idle;

		/* check that a write has not made the stripe insync */
		if (test_bit(STRIPE_INSYNC, &sh->state))
			break;

		/* now write out any block on a failed drive,
		 * or P or Q if they were recomputed
		 */
		BUG_ON(s->uptodate < disks - 1); /* We don't need Q to recover */
		if (s->failed == 2) {
			dev = &sh->dev[r6s->failed_num[1]];
			s->locked++;
			set_bit(R5_LOCKED, &dev->flags);
			set_bit(R5_Wantwrite, &dev->flags);
		}
		if (s->failed >= 1) {
			dev = &sh->dev[r6s->failed_num[0]];
			s->locked++;
			set_bit(R5_LOCKED, &dev->flags);
			set_bit(R5_Wantwrite, &dev->flags);
		}
		if (sh->ops.zero_sum_result & SUM_CHECK_P_RESULT) {
			dev = &sh->dev[pd_idx];
			s->locked++;
			set_bit(R5_LOCKED, &dev->flags);
			set_bit(R5_Wantwrite, &dev->flags);
		}
		if (sh->ops.zero_sum_result & SUM_CHECK_Q_RESULT) {
			dev = &sh->dev[qd_idx];
			s->locked++;
			set_bit(R5_LOCKED, &dev->flags);
			set_bit(R5_Wantwrite, &dev->flags);
		}
		clear_bit(STRIPE_DEGRADED, &sh->state);

		set_bit(STRIPE_INSYNC, &sh->state);
		break;
	case check_state_run:
	case check_state_run_q:
	case check_state_run_pq:
		break; /* we will be called again upon completion */
	case check_state_check_result:
		sh->check_state = check_state_idle;

		/* handle a successful check operation, if parity is correct
		 * we are done.  Otherwise update the mismatch count and repair
		 * parity if !MD_RECOVERY_CHECK
		 */
		if (sh->ops.zero_sum_result == 0) {
			/* both parities are correct */
			if (!s->failed)
				set_bit(STRIPE_INSYNC, &sh->state);
			else {
				/* in contrast to the raid5 case we can validate
				 * parity, but still have a failure to write
				 * back
				 */
				sh->check_state = check_state_compute_result;
				/* Returning at this point means that we may go
				 * off and bring p and/or q uptodate again so
				 * we make sure to check zero_sum_result again
				 * to verify if p or q need writeback
				 */
			}
		} else {
			conf->mddev->resync_mismatches += STRIPE_SECTORS;
			if (test_bit(MD_RECOVERY_CHECK, &conf->mddev->recovery))
				/* don't try to repair!! */
				set_bit(STRIPE_INSYNC, &sh->state);
			else {
				int *target = &sh->ops.target;

				sh->ops.target = -1;
				sh->ops.target2 = -1;
				sh->check_state = check_state_compute_run;
				set_bit(STRIPE_COMPUTE_RUN, &sh->state);
				set_bit(STRIPE_OP_COMPUTE_BLK, &s->ops_request);
				if (sh->ops.zero_sum_result & SUM_CHECK_P_RESULT) {
					set_bit(R5_Wantcompute,
						&sh->dev[pd_idx].flags);
					*target = pd_idx;
					target = &sh->ops.target2;
					s->uptodate++;
				}
				if (sh->ops.zero_sum_result & SUM_CHECK_Q_RESULT) {
					set_bit(R5_Wantcompute,
						&sh->dev[qd_idx].flags);
					*target = qd_idx;
					s->uptodate++;
				}
			}
		}
		break;
	case check_state_compute_run:
		break;
	default:
		printk(KERN_ERR "%s: unknown check_state: %d sector: %llu\n",
		       __func__, sh->check_state,
		       (unsigned long long) sh->sector);
		BUG();
	}
}

static void handle_stripe_expansion(raid5_conf_t *conf, struct stripe_head *sh,
				struct r6_state *r6s)
{
	int i;

	/* We have read all the blocks in this stripe and now we need to
	 * copy some of them into a target stripe for expand.
	 */
	struct dma_async_tx_descriptor *tx = NULL;
	clear_bit(STRIPE_EXPAND_SOURCE, &sh->state);
	for (i = 0; i < sh->disks; i++)
		if (i != sh->pd_idx && i != sh->qd_idx) {
			int dd_idx, j;
			struct stripe_head *sh2;
			struct async_submit_ctl submit;

			sector_t bn = compute_blocknr(sh, i, 1);
			sector_t s = raid5_compute_sector(conf, bn, 0,
							  &dd_idx, NULL);
			sh2 = get_active_stripe(conf, s, 0, 1, 1);
			if (sh2 == NULL)
				/* so far only the early blocks of this stripe
				 * have been requested.  When later blocks
				 * get requested, we will try again
				 */
				continue;
			if (!test_bit(STRIPE_EXPANDING, &sh2->state) ||
			   test_bit(R5_Expanded, &sh2->dev[dd_idx].flags)) {
				/* must have already done this block */
				release_stripe(sh2);
				continue;
			}

			/* place all the copies on one channel */
			init_async_submit(&submit, 0, tx, NULL, NULL, NULL);
			tx = async_memcpy(sh2->dev[dd_idx].page,
					  sh->dev[i].page, 0, 0, STRIPE_SIZE,
					  &submit);

			set_bit(R5_Expanded, &sh2->dev[dd_idx].flags);
			set_bit(R5_UPTODATE, &sh2->dev[dd_idx].flags);
			for (j = 0; j < conf->raid_disks; j++)
				if (j != sh2->pd_idx &&
				    (!r6s || j != sh2->qd_idx) &&
				    !test_bit(R5_Expanded, &sh2->dev[j].flags))
					break;
			if (j == conf->raid_disks) {
				set_bit(STRIPE_EXPAND_READY, &sh2->state);
				set_bit(STRIPE_HANDLE, &sh2->state);
			}
			release_stripe(sh2);

		}
	/* done submitting copies, wait for them to complete */
	if (tx) {
		async_tx_ack(tx);
		dma_wait_for_async_tx(tx);
	}
}


/*
 * handle_stripe - do things to a stripe.
 *
 * We lock the stripe and then examine the state of various bits
 * to see what needs to be done.
 * Possible results:
 *    return some read request which now have data
 *    return some write requests which are safely on disc
 *    schedule a read on some buffers
 *    schedule a write of some buffers
 *    return confirmation of parity correctness
 *
 * buffers are taken off read_list or write_list, and bh_cache buffers
 * get BH_Lock set before the stripe lock is released.
 *
 */

static void handle_stripe5(struct stripe_head *sh)
{
	raid5_conf_t *conf = sh->raid_conf;
	int disks = sh->disks, i;
	struct bio *return_bi = NULL;
	struct stripe_head_state s;
	struct r5dev *dev;
	mdk_rdev_t *blocked_rdev = NULL;
	int prexor;

	memset(&s, 0, sizeof(s));
	pr_debug("handling stripe %llu, state=%#lx cnt=%d, pd_idx=%d check:%d "
		 "reconstruct:%d\n", (unsigned long long)sh->sector, sh->state,
		 atomic_read(&sh->count), sh->pd_idx, sh->check_state,
		 sh->reconstruct_state);

	spin_lock(&sh->lock);
	clear_bit(STRIPE_HANDLE, &sh->state);
	clear_bit(STRIPE_DELAYED, &sh->state);

	s.syncing = test_bit(STRIPE_SYNCING, &sh->state);
	s.expanding = test_bit(STRIPE_EXPAND_SOURCE, &sh->state);
	s.expanded = test_bit(STRIPE_EXPAND_READY, &sh->state);

	/* Now to look around and see what can be done */
	rcu_read_lock();
	for (i=disks; i--; ) {
		mdk_rdev_t *rdev;

		dev = &sh->dev[i];
		clear_bit(R5_Insync, &dev->flags);

		pr_debug("check %d: state 0x%lx toread %p read %p write %p "
			"written %p\n",	i, dev->flags, dev->toread, dev->read,
			dev->towrite, dev->written);

		/* maybe we can request a biofill operation
		 *
		 * new wantfill requests are only permitted while
		 * ops_complete_biofill is guaranteed to be inactive
		 */
		if (test_bit(R5_UPTODATE, &dev->flags) && dev->toread &&
		    !test_bit(STRIPE_BIOFILL_RUN, &sh->state))
			set_bit(R5_Wantfill, &dev->flags);

		/* now count some things */
		if (test_bit(R5_LOCKED, &dev->flags)) s.locked++;
		if (test_bit(R5_UPTODATE, &dev->flags)) s.uptodate++;
		if (test_bit(R5_Wantcompute, &dev->flags)) s.compute++;

		if (test_bit(R5_Wantfill, &dev->flags))
			s.to_fill++;
		else if (dev->toread)
			s.to_read++;
		if (dev->towrite) {
			s.to_write++;
			if (!test_bit(R5_OVERWRITE, &dev->flags))
				s.non_overwrite++;
		}
		if (dev->written)
			s.written++;
		rdev = rcu_dereference(conf->disks[i].rdev);
		if (blocked_rdev == NULL &&
		    rdev && unlikely(test_bit(Blocked, &rdev->flags))) {
			blocked_rdev = rdev;
			atomic_inc(&rdev->nr_pending);
		}
		if (!rdev || !test_bit(In_sync, &rdev->flags)) {
			/* The ReadError flag will just be confusing now */
			clear_bit(R5_ReadError, &dev->flags);
			clear_bit(R5_ReWrite, &dev->flags);
		}
		if (!rdev || !test_bit(In_sync, &rdev->flags)
		    || test_bit(R5_ReadError, &dev->flags)) {
			s.failed++;
			s.failed_num = i;
		} else
			set_bit(R5_Insync, &dev->flags);
	}
	rcu_read_unlock();

	if (unlikely(blocked_rdev)) {
		if (s.syncing || s.expanding || s.expanded ||
		    s.to_write || s.written) {
			set_bit(STRIPE_HANDLE, &sh->state);
			goto unlock;
		}
		/* There is nothing for the blocked_rdev to block */
		rdev_dec_pending(blocked_rdev, conf->mddev);
		blocked_rdev = NULL;
	}

	if (s.to_fill && !test_bit(STRIPE_BIOFILL_RUN, &sh->state)) {
		set_bit(STRIPE_OP_BIOFILL, &s.ops_request);
		set_bit(STRIPE_BIOFILL_RUN, &sh->state);
	}

	pr_debug("locked=%d uptodate=%d to_read=%d"
		" to_write=%d failed=%d failed_num=%d\n",
		s.locked, s.uptodate, s.to_read, s.to_write,
		s.failed, s.failed_num);
	/* check if the array has lost two devices and, if so, some requests might
	 * need to be failed
	 */
	if (s.failed > 1 && s.to_read+s.to_write+s.written)
		handle_failed_stripe(conf, sh, &s, disks, &return_bi);
	if (s.failed > 1 && s.syncing) {
		md_done_sync(conf->mddev, STRIPE_SECTORS,0);
		clear_bit(STRIPE_SYNCING, &sh->state);
		s.syncing = 0;
	}

	/* might be able to return some write requests if the parity block
	 * is safe, or on a failed drive
	 */
	dev = &sh->dev[sh->pd_idx];
	if ( s.written &&
	     ((test_bit(R5_Insync, &dev->flags) &&
	       !test_bit(R5_LOCKED, &dev->flags) &&
	       test_bit(R5_UPTODATE, &dev->flags)) ||
	       (s.failed == 1 && s.failed_num == sh->pd_idx)))
		handle_stripe_clean_event(conf, sh, disks, &return_bi);

	/* Now we might consider reading some blocks, either to check/generate
	 * parity, or to satisfy requests
	 * or to load a block that is being partially written.
	 */
	if (s.to_read || s.non_overwrite ||
	    (s.syncing && (s.uptodate + s.compute < disks)) || s.expanding)
		handle_stripe_fill5(sh, &s, disks);

	/* Now we check to see if any write operations have recently
	 * completed
	 */
	prexor = 0;
	if (sh->reconstruct_state == reconstruct_state_prexor_drain_result)
		prexor = 1;
	if (sh->reconstruct_state == reconstruct_state_drain_result ||
	    sh->reconstruct_state == reconstruct_state_prexor_drain_result) {
		sh->reconstruct_state = reconstruct_state_idle;

		/* All the 'written' buffers and the parity block are ready to
		 * be written back to disk
		 */
		BUG_ON(!test_bit(R5_UPTODATE, &sh->dev[sh->pd_idx].flags));
		for (i = disks; i--; ) {
			dev = &sh->dev[i];
			if (test_bit(R5_LOCKED, &dev->flags) &&
				(i == sh->pd_idx || dev->written)) {
				pr_debug("Writing block %d\n", i);
				set_bit(R5_Wantwrite, &dev->flags);
				if (prexor)
					continue;
				if (!test_bit(R5_Insync, &dev->flags) ||
				    (i == sh->pd_idx && s.failed == 0))
					set_bit(STRIPE_INSYNC, &sh->state);
			}
		}
		if (test_and_clear_bit(STRIPE_PREREAD_ACTIVE, &sh->state)) {
			atomic_dec(&conf->preread_active_stripes);
			if (atomic_read(&conf->preread_active_stripes) <
				IO_THRESHOLD)
				md_wakeup_thread(conf->mddev->thread);
		}
	}

	/* Now to consider new write requests and what else, if anything
	 * should be read.  We do not handle new writes when:
	 * 1/ A 'write' operation (copy+xor) is already in flight.
	 * 2/ A 'check' operation is in flight, as it may clobber the parity
	 *    block.
	 */
	if (s.to_write && !sh->reconstruct_state && !sh->check_state)
		handle_stripe_dirtying5(conf, sh, &s, disks);

	/* maybe we need to check and possibly fix the parity for this stripe
	 * Any reads will already have been scheduled, so we just see if enough
	 * data is available.  The parity check is held off while parity
	 * dependent operations are in flight.
	 */
	if (sh->check_state ||
	    (s.syncing && s.locked == 0 &&
	     !test_bit(STRIPE_COMPUTE_RUN, &sh->state) &&
	     !test_bit(STRIPE_INSYNC, &sh->state)))
		handle_parity_checks5(conf, sh, &s, disks);

	if (s.syncing && s.locked == 0 && test_bit(STRIPE_INSYNC, &sh->state)) {
		md_done_sync(conf->mddev, STRIPE_SECTORS,1);
		clear_bit(STRIPE_SYNCING, &sh->state);
	}

	/* If the failed drive is just a ReadError, then we might need to progress
	 * the repair/check process
	 */
	if (s.failed == 1 && !conf->mddev->ro &&
	    test_bit(R5_ReadError, &sh->dev[s.failed_num].flags)
	    && !test_bit(R5_LOCKED, &sh->dev[s.failed_num].flags)
	    && test_bit(R5_UPTODATE, &sh->dev[s.failed_num].flags)
		) {
		dev = &sh->dev[s.failed_num];
		if (!test_bit(R5_ReWrite, &dev->flags)) {
			set_bit(R5_Wantwrite, &dev->flags);
			set_bit(R5_ReWrite, &dev->flags);
			set_bit(R5_LOCKED, &dev->flags);
			s.locked++;
		} else {
			/* let's read it back */
			set_bit(R5_Wantread, &dev->flags);
			set_bit(R5_LOCKED, &dev->flags);
			s.locked++;
		}
	}

	/* Finish reconstruct operations initiated by the expansion process */
	if (sh->reconstruct_state == reconstruct_state_result) {
		struct stripe_head *sh2
			= get_active_stripe(conf, sh->sector, 1, 1, 1);
		if (sh2 && test_bit(STRIPE_EXPAND_SOURCE, &sh2->state)) {
			/* sh cannot be written until sh2 has been read.
			 * so arrange for sh to be delayed a little
			 */
			set_bit(STRIPE_DELAYED, &sh->state);
			set_bit(STRIPE_HANDLE, &sh->state);
			if (!test_and_set_bit(STRIPE_PREREAD_ACTIVE,
					      &sh2->state))
				atomic_inc(&conf->preread_active_stripes);
			release_stripe(sh2);
			goto unlock;
		}
		if (sh2)
			release_stripe(sh2);

		sh->reconstruct_state = reconstruct_state_idle;
		clear_bit(STRIPE_EXPANDING, &sh->state);
		for (i = conf->raid_disks; i--; ) {
			set_bit(R5_Wantwrite, &sh->dev[i].flags);
			set_bit(R5_LOCKED, &sh->dev[i].flags);
			s.locked++;
		}
	}

	if (s.expanded && test_bit(STRIPE_EXPANDING, &sh->state) &&
	    !sh->reconstruct_state) {
		/* Need to write out all blocks after computing parity */
		sh->disks = conf->raid_disks;
		stripe_set_idx(sh->sector, conf, 0, sh);
		schedule_reconstruction(sh, &s, 1, 1);
	} else if (s.expanded && !sh->reconstruct_state && s.locked == 0) {
		clear_bit(STRIPE_EXPAND_READY, &sh->state);
		atomic_dec(&conf->reshape_stripes);
		wake_up(&conf->wait_for_overlap);
		md_done_sync(conf->mddev, STRIPE_SECTORS, 1);
	}

	if (s.expanding && s.locked == 0 &&
	    !test_bit(STRIPE_COMPUTE_RUN, &sh->state))
		handle_stripe_expansion(conf, sh, NULL);

 unlock:
	spin_unlock(&sh->lock);

	/* wait for this device to become unblocked */
	if (unlikely(blocked_rdev))
		md_wait_for_blocked_rdev(blocked_rdev, conf->mddev);

	if (s.ops_request)
		raid_run_ops(sh, s.ops_request);

	ops_run_io(sh, &s);

	return_io(return_bi);
}

static void handle_stripe6(struct stripe_head *sh)
{
	raid5_conf_t *conf = sh->raid_conf;
	int disks = sh->disks;
	struct bio *return_bi = NULL;
	int i, pd_idx = sh->pd_idx, qd_idx = sh->qd_idx;
	struct stripe_head_state s;
	struct r6_state r6s;
	struct r5dev *dev, *pdev, *qdev;
	mdk_rdev_t *blocked_rdev = NULL;

	pr_debug("handling stripe %llu, state=%#lx cnt=%d, "
		"pd_idx=%d, qd_idx=%d\n, check:%d, reconstruct:%d\n",
	       (unsigned long long)sh->sector, sh->state,
	       atomic_read(&sh->count), pd_idx, qd_idx,
	       sh->check_state, sh->reconstruct_state);
	memset(&s, 0, sizeof(s));

	spin_lock(&sh->lock);
	clear_bit(STRIPE_HANDLE, &sh->state);
	clear_bit(STRIPE_DELAYED, &sh->state);

	s.syncing = test_bit(STRIPE_SYNCING, &sh->state);
	s.expanding = test_bit(STRIPE_EXPAND_SOURCE, &sh->state);
	s.expanded = test_bit(STRIPE_EXPAND_READY, &sh->state);
	/* Now to look around and see what can be done */

	rcu_read_lock();
	for (i=disks; i--; ) {
		mdk_rdev_t *rdev;
		dev = &sh->dev[i];
		clear_bit(R5_Insync, &dev->flags);

		pr_debug("check %d: state 0x%lx read %p write %p written %p\n",
			i, dev->flags, dev->toread, dev->towrite, dev->written);
		/* maybe we can reply to a read
		 *
		 * new wantfill requests are only permitted while
		 * ops_complete_biofill is guaranteed to be inactive
		 */
		if (test_bit(R5_UPTODATE, &dev->flags) && dev->toread &&
		    !test_bit(STRIPE_BIOFILL_RUN, &sh->state))
			set_bit(R5_Wantfill, &dev->flags);

		/* now count some things */
		if (test_bit(R5_LOCKED, &dev->flags)) s.locked++;
		if (test_bit(R5_UPTODATE, &dev->flags)) s.uptodate++;
		if (test_bit(R5_Wantcompute, &dev->flags)) {
			s.compute++;
			BUG_ON(s.compute > 2);
		}

		if (test_bit(R5_Wantfill, &dev->flags)) {
			s.to_fill++;
		} else if (dev->toread)
			s.to_read++;
		if (dev->towrite) {
			s.to_write++;
			if (!test_bit(R5_OVERWRITE, &dev->flags))
				s.non_overwrite++;
		}
		if (dev->written)
			s.written++;
		rdev = rcu_dereference(conf->disks[i].rdev);
		if (blocked_rdev == NULL &&
		    rdev && unlikely(test_bit(Blocked, &rdev->flags))) {
			blocked_rdev = rdev;
			atomic_inc(&rdev->nr_pending);
		}
		if (!rdev || !test_bit(In_sync, &rdev->flags)) {
			/* The ReadError flag will just be confusing now */
			clear_bit(R5_ReadError, &dev->flags);
			clear_bit(R5_ReWrite, &dev->flags);
		}
		if (!rdev || !test_bit(In_sync, &rdev->flags)
		    || test_bit(R5_ReadError, &dev->flags)) {
			if (s.failed < 2)
				r6s.failed_num[s.failed] = i;
			s.failed++;
		} else
			set_bit(R5_Insync, &dev->flags);
	}
	rcu_read_unlock();

	if (unlikely(blocked_rdev)) {
		if (s.syncing || s.expanding || s.expanded ||
		    s.to_write || s.written) {
			set_bit(STRIPE_HANDLE, &sh->state);
			goto unlock;
		}
		/* There is nothing for the blocked_rdev to block */
		rdev_dec_pending(blocked_rdev, conf->mddev);
		blocked_rdev = NULL;
	}

	if (s.to_fill && !test_bit(STRIPE_BIOFILL_RUN, &sh->state)) {
		set_bit(STRIPE_OP_BIOFILL, &s.ops_request);
		set_bit(STRIPE_BIOFILL_RUN, &sh->state);
	}

	pr_debug("locked=%d uptodate=%d to_read=%d"
	       " to_write=%d failed=%d failed_num=%d,%d\n",
	       s.locked, s.uptodate, s.to_read, s.to_write, s.failed,
	       r6s.failed_num[0], r6s.failed_num[1]);
	/* check if the array has lost >2 devices and, if so, some requests
	 * might need to be failed
	 */
	if (s.failed > 2 && s.to_read+s.to_write+s.written)
		handle_failed_stripe(conf, sh, &s, disks, &return_bi);
	if (s.failed > 2 && s.syncing) {
		md_done_sync(conf->mddev, STRIPE_SECTORS,0);
		clear_bit(STRIPE_SYNCING, &sh->state);
		s.syncing = 0;
	}

	/*
	 * might be able to return some write requests if the parity blocks
	 * are safe, or on a failed drive
	 */
	pdev = &sh->dev[pd_idx];
	r6s.p_failed = (s.failed >= 1 && r6s.failed_num[0] == pd_idx)
		|| (s.failed >= 2 && r6s.failed_num[1] == pd_idx);
	qdev = &sh->dev[qd_idx];
	r6s.q_failed = (s.failed >= 1 && r6s.failed_num[0] == qd_idx)
		|| (s.failed >= 2 && r6s.failed_num[1] == qd_idx);

	if ( s.written &&
	     ( r6s.p_failed || ((test_bit(R5_Insync, &pdev->flags)
			     && !test_bit(R5_LOCKED, &pdev->flags)
			     && test_bit(R5_UPTODATE, &pdev->flags)))) &&
	     ( r6s.q_failed || ((test_bit(R5_Insync, &qdev->flags)
			     && !test_bit(R5_LOCKED, &qdev->flags)
			     && test_bit(R5_UPTODATE, &qdev->flags)))))
		handle_stripe_clean_event(conf, sh, disks, &return_bi);

	/* Now we might consider reading some blocks, either to check/generate
	 * parity, or to satisfy requests
	 * or to load a block that is being partially written.
	 */
	if (s.to_read || s.non_overwrite || (s.to_write && s.failed) ||
	    (s.syncing && (s.uptodate + s.compute < disks)) || s.expanding)
		handle_stripe_fill6(sh, &s, &r6s, disks);

	/* Now we check to see if any write operations have recently
	 * completed
	 */
	if (sh->reconstruct_state == reconstruct_state_drain_result) {
		int qd_idx = sh->qd_idx;

		sh->reconstruct_state = reconstruct_state_idle;
		/* All the 'written' buffers and the parity blocks are ready to
		 * be written back to disk
		 */
		BUG_ON(!test_bit(R5_UPTODATE, &sh->dev[sh->pd_idx].flags));
		BUG_ON(!test_bit(R5_UPTODATE, &sh->dev[qd_idx].flags));
		for (i = disks; i--; ) {
			dev = &sh->dev[i];
			if (test_bit(R5_LOCKED, &dev->flags) &&
			    (i == sh->pd_idx || i == qd_idx ||
			     dev->written)) {
				pr_debug("Writing block %d\n", i);
				BUG_ON(!test_bit(R5_UPTODATE, &dev->flags));
				set_bit(R5_Wantwrite, &dev->flags);
				if (!test_bit(R5_Insync, &dev->flags) ||
				    ((i == sh->pd_idx || i == qd_idx) &&
				      s.failed == 0))
					set_bit(STRIPE_INSYNC, &sh->state);
			}
		}
		if (test_and_clear_bit(STRIPE_PREREAD_ACTIVE, &sh->state)) {
			atomic_dec(&conf->preread_active_stripes);
			if (atomic_read(&conf->preread_active_stripes) <
				IO_THRESHOLD)
				md_wakeup_thread(conf->mddev->thread);
		}
	}

	/* Now to consider new write requests and what else, if anything
	 * should be read.  We do not handle new writes when:
	 * 1/ A 'write' operation (copy+gen_syndrome) is already in flight.
	 * 2/ A 'check' operation is in flight, as it may clobber the parity
	 *    block.
	 */
	if (s.to_write && !sh->reconstruct_state && !sh->check_state)
		handle_stripe_dirtying6(conf, sh, &s, &r6s, disks);

	/* maybe we need to check and possibly fix the parity for this stripe
	 * Any reads will already have been scheduled, so we just see if enough
	 * data is available.  The parity check is held off while parity
	 * dependent operations are in flight.
	 */
	if (sh->check_state ||
	    (s.syncing && s.locked == 0 &&
	     !test_bit(STRIPE_COMPUTE_RUN, &sh->state) &&
	     !test_bit(STRIPE_INSYNC, &sh->state)))
		handle_parity_checks6(conf, sh, &s, &r6s, disks);

	if (s.syncing && s.locked == 0 && test_bit(STRIPE_INSYNC, &sh->state)) {
		md_done_sync(conf->mddev, STRIPE_SECTORS,1);
		clear_bit(STRIPE_SYNCING, &sh->state);
	}

	/* If the failed drives are just a ReadError, then we might need
	 * to progress the repair/check process
	 */
	if (s.failed <= 2 && !conf->mddev->ro)
		for (i = 0; i < s.failed; i++) {
			dev = &sh->dev[r6s.failed_num[i]];
			if (test_bit(R5_ReadError, &dev->flags)
			    && !test_bit(R5_LOCKED, &dev->flags)
			    && test_bit(R5_UPTODATE, &dev->flags)
				) {
				if (!test_bit(R5_ReWrite, &dev->flags)) {
					set_bit(R5_Wantwrite, &dev->flags);
					set_bit(R5_ReWrite, &dev->flags);
					set_bit(R5_LOCKED, &dev->flags);
					s.locked++;
				} else {
					/* let's read it back */
					set_bit(R5_Wantread, &dev->flags);
					set_bit(R5_LOCKED, &dev->flags);
					s.locked++;
				}
			}
		}

	/* Finish reconstruct operations initiated by the expansion process */
	if (sh->reconstruct_state == reconstruct_state_result) {
		sh->reconstruct_state = reconstruct_state_idle;
		clear_bit(STRIPE_EXPANDING, &sh->state);
		for (i = conf->raid_disks; i--; ) {
			set_bit(R5_Wantwrite, &sh->dev[i].flags);
			set_bit(R5_LOCKED, &sh->dev[i].flags);
			s.locked++;
		}
	}

	if (s.expanded && test_bit(STRIPE_EXPANDING, &sh->state) &&
	    !sh->reconstruct_state) {
		struct stripe_head *sh2
			= get_active_stripe(conf, sh->sector, 1, 1, 1);
		if (sh2 && test_bit(STRIPE_EXPAND_SOURCE, &sh2->state)) {
			/* sh cannot be written until sh2 has been read.
			 * so arrange for sh to be delayed a little
			 */
			set_bit(STRIPE_DELAYED, &sh->state);
			set_bit(STRIPE_HANDLE, &sh->state);
			if (!test_and_set_bit(STRIPE_PREREAD_ACTIVE,
					      &sh2->state))
				atomic_inc(&conf->preread_active_stripes);
			release_stripe(sh2);
			goto unlock;
		}
		if (sh2)
			release_stripe(sh2);

		/* Need to write out all blocks after computing P&Q */
		sh->disks = conf->raid_disks;
		stripe_set_idx(sh->sector, conf, 0, sh);
		schedule_reconstruction(sh, &s, 1, 1);
	} else if (s.expanded && !sh->reconstruct_state && s.locked == 0) {
		clear_bit(STRIPE_EXPAND_READY, &sh->state);
		atomic_dec(&conf->reshape_stripes);
		wake_up(&conf->wait_for_overlap);
		md_done_sync(conf->mddev, STRIPE_SECTORS, 1);
	}

	if (s.expanding && s.locked == 0 &&
	    !test_bit(STRIPE_COMPUTE_RUN, &sh->state))
		handle_stripe_expansion(conf, sh, &r6s);

 unlock:
	spin_unlock(&sh->lock);

	/* wait for this device to become unblocked */
	if (unlikely(blocked_rdev))
		md_wait_for_blocked_rdev(blocked_rdev, conf->mddev);

	if (s.ops_request)
		raid_run_ops(sh, s.ops_request);

	ops_run_io(sh, &s);

	return_io(return_bi);
}

static void handle_stripe(struct stripe_head *sh)
{
	if (sh->raid_conf->level == 6)
		handle_stripe6(sh);
	else
		handle_stripe5(sh);
}

static void raid5_activate_delayed(raid5_conf_t *conf)
{
	if (atomic_read(&conf->preread_active_stripes) < IO_THRESHOLD) {
		while (!list_empty(&conf->delayed_list)) {
			struct list_head *l = conf->delayed_list.next;
			struct stripe_head *sh;
			sh = list_entry(l, struct stripe_head, lru);
			list_del_init(l);
			clear_bit(STRIPE_DELAYED, &sh->state);
			if (!test_and_set_bit(STRIPE_PREREAD_ACTIVE, &sh->state))
				atomic_inc(&conf->preread_active_stripes);
			list_add_tail(&sh->lru, &conf->hold_list);
		}
	} else
		blk_plug_device(conf->mddev->queue);
}

static void activate_bit_delay(raid5_conf_t *conf)
{
	/* device_lock is held */
	struct list_head head;
	list_add(&head, &conf->bitmap_list);
	list_del_init(&conf->bitmap_list);
	while (!list_empty(&head)) {
		struct stripe_head *sh = list_entry(head.next, struct stripe_head, lru);
		list_del_init(&sh->lru);
		atomic_inc(&sh->count);
		__release_stripe(conf, sh);
	}
}

static void unplug_slaves(mddev_t *mddev)
{
	raid5_conf_t *conf = mddev->private;
	int i;
	int devs = max(conf->raid_disks, conf->previous_raid_disks);

	rcu_read_lock();
	for (i = 0; i < devs; i++) {
		mdk_rdev_t *rdev = rcu_dereference(conf->disks[i].rdev);
		if (rdev && !test_bit(Faulty, &rdev->flags) && atomic_read(&rdev->nr_pending)) {
			struct request_queue *r_queue = bdev_get_queue(rdev->bdev);

			atomic_inc(&rdev->nr_pending);
			rcu_read_unlock();

			blk_unplug(r_queue);

			rdev_dec_pending(rdev, mddev);
			rcu_read_lock();
		}
	}
	rcu_read_unlock();
}

static void raid5_unplug_device(struct request_queue *q)
{
	mddev_t *mddev = q->queuedata;
	raid5_conf_t *conf = mddev->private;
	unsigned long flags;

	spin_lock_irqsave(&conf->device_lock, flags);

	if (blk_remove_plug(q)) {
		5.c : seq_flush++;
		raid5_activate_delayed(5.c );
	}
	md_wakeup_thread(mddev->right riverspin_une De/*
 restoraid5.c : Multiple Devices driverun  Co_slaves(C) 19);
}

static int lnar, congested(void *data,nctiobits)
{n
 *dev_t *king  = in C;
olnar, 5.c the 5.c  = C) 1999pruel diver/* No difference between ght s and writes.  Just check
	 * how busy the stripe_cache istrib/ver forking t Thanks toking ,ing fo) Moleturn 1;er for5.c : inMiguee_be DeedLicense as published by
 quiesceLicense as publishelist_empty_carefulid5.c : * the Fren)
  License as pub
ense as 0nagem/* We wationead requests to align with chunks where possible,
 * buttware;blic Licendon't need to.
r thent functions.
 *mergeable_bvec(strucral c Lic_queue *q,
				ve, Camass Inc., _in C *bvm, USA.
 */

/*io_vec *biovecor making the RAID-6 dqverseueevelopmsector_t ttle
  = bvm->bi_ubtle (+ get_start it w(d I got b6 mana	ctiomax;in
 signednctiomple  it wros donating 
 * We group  explanation.
 * seq group bitd I got iize >> 9iver for) so it drw & 1) == WRITELicense as ng for->bv_len; /* always allowtware; nse
be nc., 675 r the terms of ->new_
 * We group b<tmap updates into batLice
 * We group bitmap updessfully written.;
	max =  (
 * We group b- ((ubtle (&new additions.
 * 1)) +ch has a num)) <<te o termsax < 0)e
 * = of ck in a st=t very importan &&ch has a numbe= 0 that isn't very important
	elseicense as 
 * e}
ement functioinfully wboundaryms of the RAID-, mod The secing or mattle
 * subtle (andio got it wrong the first time) h hasn'eserves planation.
 *
 * We group bitmap updates into batches.  Each batch has a number.h hasn'tay write out sevch successfully written.
 * conf->bm_flush is the number of the last batch that was closed to
nse as *
 * We group b>=
		When we discover that we will need to write tof the GPYIN addreadytod/or retry LIFO  ( in O(1) ... we aret isinterrupt )ionallater sampled byons.
 d Software Fou Pengadd_ seqto_out tAve, Cameady to,ent possible
 * byor maplanationlongices dIngo Moln
 *	   C* raid5.c : Multiple Devices driverbi got next = 5.c : out t_l Pu_ * (fed shou;
(C) 199#include "md.h"
#includm_fluIngo Molnar
 *	   Copyright (C) 2002, 2003 H. Peter Anv
 *	   Copyright (5.c : C) 1999, 2000 Inatch it wile are ready ux
 *	  seqfromd.h>
#iaid/pq.h>
#include <line are ready to
#inclx/cpu.h>
#include "md.h"
#i add_stbiight (C) 199#include "md.h"
#i = NULLo Mol isn't vOxman
efine BYPASS_THRESHOLD	1
#defnclude "if_HASH			(PAGE_SIZE / sizeof(strucp.h"

/*
 ude <linuo Moclude <linux/clist_hea/*
	ributhis secenshe the Fr modif counte a 1 softchedprocessed)

/* tripe+device fozero (upper 8ing for

/*/STRIPE_SHIphys_segment bit1t.
 *biased+deviceofd to a stripeesp.  T}ed a copy fineatchasionalThe "lnar, M* (f_endio" should redis if bit ouad succeeded softif itionaldid, callch haist f on are originaond a (havingch haput are new <liionalfirst) Sof  Istripe+devifailed.ev.h>
#include <lilking this list finclude <linux/rnctioerrorfine	IO_THRESHO*eterm_bi tbl[((secta test seaking the RAID-opment possible
 * byes someuptodate = tge, bit(BIO_UPTODATE, &[((sectfine STRIPEk_rng the S) ?
#incls the_HASved RAID-6 dhe curr plug the->bd_diskliably liably is a li* by donating a test se	S) ? = o Pen*)ollowing can) >> STNOIA && defined(& HASH_MA1
#if _dec_pist ng(S) ?,cpu.h>
5/6 manaadd_st!ven the i>bi_sectight (d a bio tSTRIP_list0STRIr foratomic() asand_+ ((id5.c : the Fred.h"
#in free Lice	   C_upid5.c : wait_for_modifyfdef d))
#dOxman

	pr_debug(alking this list f : ioiven t...hart_sp IO for a out t\n"evicenux/kthread.h>
#i#endif

#Gadi Oxgement functioIPE_SitsRS) ?include <linux/fine	IO_THREbridge, MA 02139ber.OCK(the ably (ing can be evice_locid5_bi_pus c>>9) > ably _maxas a numpyriicense as n ad Linuxdevicseveral b(qPublves sNR_HAe may be several bi{
	return bi be several bhys_segments & 0xid5_biq->BITMAP*
 * fn_seg/* it' theo hare
 * applnd/or 0xffff;
}

st ae neo's tage, US * jcan ->bi_g a suprlap.  Th_segments >>nse as pubatch it will be
 * We_
#endif

#dAve, Cambridge, MA 02139,IFT - 9)
#deffollowin do a ting the bitmap reliably is a lient possible
 * by donating a test seray and queue dd_id* exctor
 * of tthis lfine CTORS) ? (bio)->bi_n_lock in. This is bm_flush+1,
{
	unsignight (printk(argtic inline int rai : nonments(edpes inurn raid5_biman
]))
 * usea couclone5_demake a copye, ached migap.  Tents(biom_fluss(bio)s of
 * o, GFP_NOIOraid5_bi!ents(bio_segments & 0xfi_phys_ s at bi bio_ite a at biofunction, softh)
{
	ia test e a bithead *s ends past thi.d first data di got f (sh->e follog this list f;md starts just art from followinqd_ii_phys	computrc/lii	/* 
	/* md starts just ubtle (anons.
 * Tturn ine intZE
#dhw_segment got it wro, USA.		ce *0++;
	return & | rai,HASH_evicercuude "me De(_seg#if RAI
}

dere This pZE
#defto ds[ | rai].S) ?raid5_bi#if R&& + ((bio)-In_sync, &S) ?->ces dright (G
#defiinc( to a 'nrssert_sp_segme}

/* Whar
 *	alkinNFIG_SMPsks)
{linux/cD5_PARAN_next_d starts just  inl raidbe usedthe Q disk
 * is races d &= ~(1any >bi_SEG_VALID_segm}
static inline int +=s-1.  Tin C_offsets >>tic in count of proct raid6_dight (c inint bigt issom Genyp.  Thnext : NUt raid6_de Q VLOCK() assert_spin_lock5/6 manaountents & 0xfspan e <linux/asyncid5.c : Multiple De)
		r.) ((event	if (idx =args...) ((void) (pri, USA.ce *her version 2tmap 
		(*count)++;
	Multiple Devi/* nothunt */dx_to_the data diine __inline__
#endif

#defdrom Molnar
 *	   C== sh->qd_idx)
		retur
		generic_(cnt_bridge,ayout)
		(*counse as publ} the ght (raid_disks-3, the par->ddf_layout)
		(*couents = raid5_the GN_ine ipriorityd) (pri - the next bxt modify5_delinked 
 sionaFullions_actte is tto
 bm_wree
 * pass pre+devind
 * a bio couup untilr
 *
/* Fyte |yrighshold is ex we mu.  In _nextaltest_bit(STRdevicr
 *increral biwhet exted colu shouLL_Rtate);d beforesh->stolne striute ever,r procewillt bi num_COMPUTE_edUN, &sSTRIPE_ intTARTEDLL_Rclude <lh)
{anatifyunt ar
 * *sh)
{
 exain flight i/o.When w test_bit(STstructbt ouh)
{N, &sh->r
 *hcnt)
}

/* ripe(raid has changed, i.e.t_bit(cnt)wa|| somoh)
{ first 
		iate);
}

s Software Foue are rmodify
		if *_t *conf);

static inSTRIPE_SIZE>>9)
#define	IO_THREtail(&sh->lrushi->bprintk(arg%s:ED, &sh: %st(STRIPE_Bfull_ware; : %<linest_bit(ST) &&\n"
	--b __t)
	__ conf-n)
 * any t bio *bD, &sh->sta) ? " any " : "t an- conf-) {
				list_add_taipe(raidu, &conf->bitmap_list);
		)
{
	stght ( bio *bsert_sp_ &sh->state)cked(&co				   sh->bevice_lock) {
				list_add_tail(&sh->lruight (sh =			blk_n>
#i_add_tail(&sh->lr.erat, typeof(*sh), lrubi->biption)
 * any _device(conf->mddeine p;
				list_add_tai (in ad the ever(!+ ((bio)-(atomic_dec_and_t, &sh->ent edisks)
{ished by
 conf->mdd
		} tmap5.c : last_confine pand_clear_bit(STRIPngo MoAD_ACTks)
{and_cled(&conf->x/cpu.h>
pes);
				if (
		reand_clear_bit(STRIPE-omic_reabit(STRIPE_BIOFId);
		ished by
 ar_bit(STRIPEtrip+;
	rend_clear_bit(STRIPE_PREREAman
man
bi, 0);->lru, &conf->handle_lisonf->mddev&&conf- (_bit(STRIPE_EXIPE_BIOFILf->wait_nt)++;
	ar_bit(STRIPE>f->active_stripes);
			) ||retry_r		clear_bit(STRIPE_BIT_DELAY, &sh->stattmap a	md_wakeup_thread(conf->mddeves);
				if (, USA.se {
			BUG_ON(stri			}
			atomic_dec(&conf->active_stripes);
			if (est_bit(STRIPE_EXPANDING, &sh->nd_clear_bit(STRIPE_PREREbi, 0)raid5_confHECK_DEVn)
 *del_init(c(&co
	unsig)
{
	struct (&cod_tail(&	BUG_ONUG
#defi_bit(Sremove_has != 1lkingse as 			}gement functioNULL;
		bi->b5_dec_bi_hw_segments(struct bio *bio)
s in thhort val = raid5_bi_hw_segments(bio);

	--val;
	bio->bi_phys_segments  16) | raid5_btle
 * sessfsh)
{
 little
 * slogical{
	disk+ d(&co hlist_heanf->mddev->queue);
			}pe(c
stationwsk in ahead dirULL)
/ stricpu, remainings >> 16)unlikely  we_rwappinged(list>bi_RW_BARRIER)struct DEVLOCK()
list-EOPNOTSUPP_segments = raid5_RIPE_Sare; firstneral Publbi->bcpu = prst ttadisks alkin stripe_heta dng)sh&C) 1999gist  deb str0, ios[rw]et_free_stripeaddid5_conf_t *conf)
{
	struct st group e_he conf- (lih has a num_HASet_free_stripes-3, the p_d0,
 *wtmapREADf->re (lisC) 1999E_BIape_h->qd_idtmapMaxSn we di>inactivtic inline int raidq,f->iturn raid5_bi_pstripe_hash(cotbl[((secten we dis~When we _t)(atomicSECTORS-("rem, sh->sectoh);
	atomic_inc(&+bi_hw_segio *bio)0xffPE_SHIFT) & HASH_MASere may be several bio's c inover-loa musto+devicend
 * a bio could 
	acti(;	remove_hash(sh<f, sh->sector stripe_hash(coct stripes);
out:
ight (DEFINE_WAIT(wfdef D16) , st,devel to d to
_buffprevioucludeout t:ruct  int nE_PREREAers(sx/cpu.h>
#enditripe_heaprepareread.) (l(args...) ((voidor (lap, &w, TASK_UNINTERRUPTIBLEfdef DEBUK_DEVLOCKPAGE_SIZxt;
	shrogressebugry(first,disks)
{
	i Mole DeLL_RFreemustsve_s
	return 0;
}
may bestohys_64bithat a 32statplatformddf alwo it m);
		previous)/linux/Cnum set <<half-upsectd valurevious)Ofcoursive_s
	return 0;
}
c partic&sh- afterevious)chedock(strudropp->ststhatcever tripve s This pruct stra bit ons_actihatver io *kor_tis,ver tructhav{
	raid5_coicularagain.eviousf (shks;
	if (idx == sh->qd_idx)
		return he current
 delt stripeNG, eviou   ?dev[i].page = NU<cpu.h>
#i*sh);

static ations_a:dev[i].page = NU>/cpu.h>
#i*sh);

static isks)
{num; i++) {
		s

	for (_truct page *pai;

	for (i=0publ	(bi, 0);
		bstate));
	BUG_ON(stripe_operatioons_active(sh));

	CHECK_DEVLOCK();
	psaf{
	rait_stripe called, stripe %llu\n",
		(un->raong longi) {

		return_bi = bi->bi_next;
		bi-tor, cchedulehe par			goto out t;


	fsh->uct r5conf, previous, sh);
	sh->state = 0;


sh->uct stripe6 ded || -ked(&conax_degr; i<i->biruct hlist block */t_disk(int disk, int stripe_hash(con+;
	retur

	for (N_ERR "secdisks) ? disk : uct bio *biolnar,:n",
		(unsignECK_DEVL %llu{
			pri, i, q - con	(ux/async_tx.h>tx.h)ruct hlist,       dev->read, dev->tostripe_hash(cobi->bikeup_the the Fred_list), int write, dev->tor=%llx i=%d f (lisbi_hw_segrw&RWA_MASK)
#ifdef DEBUsheread_activK_DEVLOCK

	for (disks)
{c inexpansist_ stripG_ON 
 *	dhat whil Genitunt activbuild_ist_emptor_t we mcan da bit o_striad(&sh->count) !				E__find_stoid instruct
 *	ate tipe(stbio *;
	struc+ ((,NG); asver to
 id5_G_ON(revious)
{ to;
	struc'sh'nt i;know = sh- stria(raippen5_build_* ;
		put_EXPANDING;

	BUtriph)
{I/O are *__find_s;
	strucw the linkeed
	    eratfinishy(&sh-/or modifyh;
	struf (sh some
ustd.h>
#dd_tail(&	BUG_ON(test_bit(STRIPE_HANDLE, &sh->st - previous;
	sh->disks = previous ? conf->previous_rpe %llu\n",
		(unsigned lid_disks;
	sh->sector = seCK_DEVLOCK();
	pr_debug(&sh->st/* mismatchev->STRIPE_ryh->couebug("__	pe %llu not i= conf conf, previous, sh);
	sh->state = 0;


	fe cure %llu no(sector, releasUG();
		}sh0;


	for (i = sh->disks; i--; ) {
		struct r5de->state)n",
		(unsigned once, butf->retd_blo(mddev_t *mddev);
C) 1999sussert_lo&conf->device_lock);

	do  * conf->vent_lochASH			(ripe_head *sh;

	pr_debug(/* Atachedvent_loc)
{
strii vointrole <lin;
	strucuserspacent i;
neraa miss any bux/Csector &&ait
	pr_debug("__997 I_anatals(currehash(sh->gen

		if (!(page = alloc_page(GFP_KERNE+;
	ret))) {
			turn 1;
		}
		sh->e_operae_lock);

	do {
		wait_event_lock_irq(connf->wait_for_stripe,
				    conf->quies


	for (i = sh->disks i--; ) {
		strlong long)&sh->state)) {
	onf, sectdec(&conf->prv->thrd_blo!nux/modify
bxt' hPubl,) | rai,(sh, i, previos);
	}stripe_head Sns_actisit and*__fiu\n",otructtriply w This  du*confFP_KERN.  F97 I f_t yio *r			    cooftwait pronf, 	pr_debug("__lnar, 
 *
 * Multipurrent
 ably 0;


	fpe_head *sh;

	pr_debug(e,
						    !list_empty(&conf->i) {
ratio(!(page = alloc_page(GFP_KERNEL))0;


	se->state)) {
	HANDL) < (&conf->pre_bloclear>state)) {
	DELAYc_dec(&conf->pr
		retu_head *sh;

	pr_debu->generation/* can strash)
ns_actacti+dev-a		if,_phys_segm-upebug("_XPANDING, >bi_size>>9) < sect + STRIPE_SEmic_read(&sh->count)) {
				BUG_ON(!list_empty(breakad || de	id5_bks;
	if (idx == sh->qd_idx)
		return->sector) block */) asmay be several bned lonconf, previous, sh);
	sh->state = 0;

0,
 *
	if (sh)
ap a {	     in %lluck_irq(&r_stred, and reneneral bi->bi* find an idleifdef}d a copy of the
				listle
 * slnar, us clush+1.
 * When wetle
 * subtle sst, gitruct pageo ou(struct bio *bi, ,
		(un
		bi->b;

static void ops_run_io(struc_n *cont *skisector ma/*d_state(sh)is qu not
 * Thisce fo
}

nplu/reh diector_tunder ic void _sleep(sd)
	atelye nevr/sr;
	/*phys_On each beyononf,yn_heang)sh->r, iather io) mple  worth ofphys_destinalist_ bio cou->mdces 	retmpe, locked),
;
	/*hen n generadh->c	returourc a bio cou->mdWantwris free &sh->->devic free t_dilete,ED, &sh-t_empty(&ll< 16);chedin Cphys_in_conf_tTE;
		else if (tes
			con_headch_entrn sh;
	pebug(ent possible
 * by doSTRIPE_SIZE>>9))>bi_phys_segments 

	pr_debug("insert_hash(ps_run_ioo lonhash(conf, sh->sector);ctions.
ead || detor);

	remove_hash(sh);

	sbuffe->read || detruct pagewrite || dev->written | (rdeessfv)
			atomic_ {
		struct page>nr_pending);
		rcu_read_unline stripe_head *sh)
{
	st and possh->adv->bd->rapo to
ps_run_io(odify
add_bit(Faulipe_set_losed to


	pr_dn)
 *		if  bio co out;
	fid_conf;
c void
rastateIfARTEirst	might extemiddlt gekipWRITElagsialCK_DEVLOebug("ate));
	BUG_ON(stripe_operf->retry_r page;
	}
	return 0;
}
<, int error);

st, 0raidsce == 		pr_debug(unt);
			bi->bi_sector =breakrite || ",
		(unsigned l_HANDLE, &sate));
	BUG_ON(stripe_o>=bi->bi_rd_bl page;
	}
	return 0;
}
>>bi_flactor + rdev-= 1 << BIO_UPTODATE;
			bips_run_div			pr_debuev->flv)
			atomnsert_hashd_conf;
sce == map updaurr_--; ) OCKED	bi-hrea_offset =mpty(&ysfs_notify(nf_t *cokobj? disk, "STRIPE_SIZE;
	s_segm	, disks tor_t secove_hash->bi_next = lru,erver!Wxt bck_statinked  a, &sh.flags)at prtimev;
		lu s_thr->md bioflags)us cs();

	fte, &
					&rdev->corrst devilarankstest_birestebug(e current
 *   batch.
 * When>* conf->bm_flush is the nRTED, &sh->statlast batch that was closed to
the numben",
				bi->bi_rw, i, (t was closed toh->diwmentsectohys_seta UNPLN, &sh->o
 *s mlease_an 3Meg	bi->bitest_bock(s nothi(_for_es rh->devarbitrary,r a parphys_probablt pr		gen bripe) orUN, &sh->s UNPLabohe ne nuphys_copi)
{
 partor (iif not	rw = READ;
}

/* e *pagtphys_chedfroice, astruct steric_mate))io) ruct write_rtx.h>
romARTED, &surn 0;
}
essfmaprw;
		ddf_();
	sr/srcripe_set_idxe_remit_clong ebug(t(rdev->v[i].vec;
			bi->bi_io_vec[0.bv_len = St(rdev->bd
			bi->bi_io_vec[ev, STRctor >= sector)
		page_offset = (signedev, STRIPbi->bi_io_vec[E_SECTOv[i].vec;
			bi->->rafset = (signedE_SECTO_sector) * -512;ate));
	BUG_ON(stripe_operight (o->bi_sec-= min_t->active_sh->ED, &sh->stat,ct(rdev->_segmen12;
	et stl, bio, i) {
	c[0].
	if (fx(bio, i)->bv_len;
		bi, 0);
		bo->bi_secx(bio, i)->bv_len;
		iiovec_id_for_each_segment(bvl, bio, i) {
		iiovec_ie (bi)
	if (f_for_each_segment(bvl, bio, i) {
		iet;
		} Oxmanh->di't(rdev->'desc on doLOCKdvancice_ultipconf0;
}
rati);
		 and eric_m'iovec_iRIPE_SIZE_heapage_offset;
		else
			clen = len;
iove (clen et;
		}			b_offset += bi			cle
}

rstri%ld on dSTRIPE_Hatic vice device *bm is f*sh);	bio_paIfn > 0) {
			b_behags)clen = STR,_SIZLE, &sh->stnot;

	= sh->racadx +  ensure > STty	if (frofaptor *a crash -		tx =ion)
beto ttomitripe(confphys_mak\n", (backupor *tx)
{
	s.  So	if (fat c
		els);
			else stricularphys_rid5_to;
			setmbio)
		eric_mOtx;
wi_vcnt ge = bio_iove	  b_offset, clen, &submwv[i].lly == WRITphys_s			set_bit(STRIPE_Hddf_e_offs +=  len;
	|| (ctchn > 0) {
		sa biruct bi async, clags ftic strome_det,
						 eric_mSneratinsE_HAonll(voi(sh)mbio)
			if > STRIP	}

	returno->bi_secana(int iovec_id}

	ryo %llu\n", _eric_man any_tx =,ll(void *stripe_head_unplu 10	atoon	bi = &sMaystruend_numb#inca partbei].veigur675 _stripI'mt str(bio_int rw;
		)
			rit. nevm->devitode *hnbt <<multipltor *et;
modde Icaz???sh->state);));
	BUG_ON(stripe_operat (lis? ;
	init_ >llu\n", __&&
		(unsig<int len =  reply :o a read rntfill req * new R5_Waequest,
		)->actiock(	gen_t;
	e(jiffiescked(&coio, i)->redispont i+ 10*HZ= sh->s/* C)
				generation == ge'gments,
			evice_peruct d nevbug("syndrome_d
	if (!sh->ddf_lnf);
			if (ad);
			}
		}
	}
}

stio, i)->bbio co)==ifdef e_list.next;
	sh = list_ei].vec;
			bi->bi_io_vec[0]bi->bi_size = STRIPE_SIZE;
			bbi->bi_size = STRIsigned lonfill, &dev->flags))= ear_bitc[0].b(bio)-MD_CHANGstatVSconf_t *coest_bit(ST *	   Copyright (C) 1999, 2000 In
			dev->read			    cb(!(pa>pd_idx	}
	}
	c voi>active_stkright _for (i_stop(>inac	BUG_ON(test_bit(STRIPE_HANDLE, &sh->smbio)
		flags |= Ants(rbi)) next;
	sh = listTRIPE_HAN previous, sh);
	sh->state = 0;


rintk_rl(args...) ((voidFP_KERNe (bi)LL;
			if (rw == WRITE &&
			    test_bit(R5_ReWriteZE)
bmit, flags, tx, NULL, NULL, NULtruct s page;
	}
	return 0;
}
 voidTRIPEbit(STRIPE =llu\n", _TRIPtruct s));
	BUG_Ovas a numb1;
		nf->active_sor);
			clear_biill n
	if (lis -bio, i)->bv_len; -t_bit(STRIPEt(R5_Wantf!	bi->bi_nex Oxmai, 0);
		btruct stil
		 * 			spin_lock +bio, i)->bv_len;ong)sh->sector);

	atomic_add(S}
	INIT_LIST_HEAD(&r + STRI;].page;isk 0; icouno, i)->bv_len;
 iULL;
		put_page(p);
	}
}nt ij_head *s disks t pagE_PREREA>flags));
			BUG();
		}
		devh->sector);+
raitx);
("rem(&sh->lru)
				    (atomic_read(&conf-n_bi)
{
	struct bio *bv->sector + STRIMASK])age_
	/*;
	st *biof->inactilong lbi = n(test_bitota(i   corrayatic void 
					&r* ordthoegmee Delug_ead;
	page;j=(&cotripe_ j--; sh->sector + t 

	sh-IVE,jong (&cop| raipreread_atinuASYNce_lock, flalevelong 6&conf->deviad *sh, iqt target)
{
	struct r5de(frombdisk(i(&subnr  < (j
#ifdef ev;
		count);
			bi->bi_sector = sh->se					tx = async__t sect
	struct r5deatomimemset(pagSTRIPEess(oid mev[j].);
}}
	i,;
		put_pIZ			brea;
				}
R5_t hlid->stest_b ops_coest_bit(STR_head_ref)
size>>9) < tripe_head *sh = striatomIVE, 				tx = asy sh->sect5_next_bio(rbi, dev_ conY!test_bit(STRIPE_E&sh->lru)
				    && !test_bit(STRIPE_atomn)
 *
	st;
}

stabug(;
	init_as->lru);
			}
		}
	} while (sh == NULL);bmit, flags, tx, NULL, NULLripe(sh);
}

staturn 0;
}
-(bio, i)->bv_len; *_sector - sectog)sh->sect page;
	}
	return 0;
}
x(bio, i)->bv_len;state_compute_run)
	conf, previous, sh);
	sh->state = 0;

/* Ok, &sbacki;
	int asyncady.ORS,ct bedule r (i = async_me free at exte= READ;
		else &sh->dev = READ;
		else
	rio =termiructby map
	mig, b_o lonquestd(&cphys_uct dmat exteTE;
		else if (testbi_end_idev);
		if ( adva(R5_LOCKED, &dev->flags)) {h->sector);*(
			bi->bi_io_v_build_bloc1,%p %p %p %d\n",
		n sh;
}

stat page *) * (sh->disks + 2);
}
(ed l->sector, dev->toread;
		disks;antftate_compute_runest_bdescriptor *
ops_run_compute5(sption sh;
}

sta{
		wait_e{
		struct isksn sh;
}

statitruct r5dev *tgt =est_;
	onf, s( sizeof(struc<=f, sh->secto	md_wakeup_s));
			BUG();
		}
		devdev);
		if (rd1;
				rbi = r5_next_bio(rbi, dev_SOURC !test_bit(STRIPE_&sh->lru)
				    && !test_bit(STRIPE_ (!test_bit(STRIPE_HAN sizeof(strucLL;
		put_page(p)aid5_bi_ Nist_for_le buffer conv(sXPANDl_heark->sttruct best;
		ct bio_veTE;
		else if (test	if (biotgt->pu, &conf->han;
	mark_	md_wakeup_thread(confcpu->scr		} el

	pr_debug("inserf;
	unsigne_lock, flags);
}

static ng)sh->sector, target), &tgtbe vaPE_Sakte | the dev[i]t(R5_
 * lags))sr/srcw>stavctive(aus)
		 bio_v
	async_triggif not,
	se
			rbi = dev-	if (biread = rbi (bio, i)->bv_len;
			/* aread = rbi-s(rbi)) {
					rbi-PE_SIZE;
	) * 2		if ({
		wait_eikely(countunt, STRIPE_SIZE, &submit);

	rect bio *rbi, *rbi2;

			BUG_ON(!dev->read);
			rbi = dev->read;
			dev->read = NULL;
			while (rbi && rbi->bi_sector <
				dev->sector + STRIong long)sRS) {
				rbi2 = r5_next_bio(rbi, dev->sector);
				if (!raid5_dec_bi_phys_segments(rbi)) {
					rbi-i = dev->toread;
	>bi_next = return_bi;
					return_bi = rbi;
				}
				rbi = rbi2;
			}
		}
	}
	spin_unlock_irq(&conf->device_lock);
	clear_bit(STRIPE_BIOFILL_RUN;
			bi &sh->stat				rbi = rbi2;
			}
		}
	}
	s;
			bi||return_bi);

	set_bit(STRIPE_HANDLE, &sh->state);
	release_stripe(sh);
}

static void ops_run_biofill(struct stripe_head *sh)
{
	struct dma_async_tx_descriptor *tx = NULL;
	raid5_conf_t *conf = sh->raid_conf;
	struct async_submit_ctl submit;
s))
				io, i)->bv_len;
	the GNFIXME go_fas);
	isthe uipe+ftware Foundlinbi;
le
 * su(R5_Wantwriaid5_conf_t *conf = sh->raid_conf;
	int i, disks 	int id6_next_dor mao = raid5_end_read_request;

		rcu_read_lock();
		rdev = rcu_dereference(conf->disks[i].rn bio->bi_t];
	struct page *xorexpande

	re(&submribble;


	C->written (in add s->exdev;
			pr_debug{
		 bio->bi_s for gen->bi_be*sh,
_thrtonerationup ..t bio *remucend odoead;
	
 *
 * RAID-4/5/6 mana__func_ad *sh)
{
	iRECOVERY_RESHAP) < /* set_syisks; = sh->se_loc& rbi-> Gadi Oxmreturn syndrome_dise current
 he P
 * dest<criptor *tx;

 * bor;
		if (sh->tmap afte

	rRIPE_BIO validN, &sh->ssize = STRI	if (no&
	int targe			rbi =D_ACTate)_SIZE;
		 destif (lis.c :  &sh dest_PREREA valid cload *t */
		BUG();
	BUint core it is unhashonf-m_wrilnar, return sum ; SIZE;
f (bio	dev->read = NULL;
			while (rbi t)++;
	return s!= 2o out;
	f

	if (sh->ops.target < 0)
		target = sh->ops.taget];
	BUG_state *s)
{
	raid5_c ops_run_IZE; disks =server!
 *
					&ricularikely(counte, secnf_t struate);
		}
on
			xo short g->mdpe_healy(countf (rw conf->betic voflags) is bm_fxor_de stripeicular n md_do;

	ted
 the fire, &su;
			elseit_s	}
		/*of dma_sted *sh,t_async_subconvder the  inl
	strsh->stint m
	/*>device_rivse
			cer to
 trUG_Otl submi0);
	prest, xasserTRIPsh->ranv(sc_readcripbeccpy( tx;

		ior_de bio *retruct bd0;
	/* mdate));
	BUG_O= sh->qpe %llu\ndev->written struct s

	if (sh->ops.targetSYNC	target = sh->ops.target2 bio *bi, disk
	struct page *xor_deatomic_add(ST&sh->dev[i].flag		blocksext_}atic in valid first tt */
		BUG();
	BUG_enerating p("%s: stripe %ll struct sead *sh)
{
	ips.target <QUESic_decrget = sh->ops.t STRIPE_SIgned long long)&&;
	int targeue;

		put_page(p);
	}
}lags);ss co",
			}

	e Devi->mdfrombio, ate);bug("run_compute6/_bit(R5_Wantcomputes_complete_compute, sh,
run_compute6*bit(R5_Wantcompu %d\keeid5_pngs ris bcs[0], hol a bio could span
{
	r, tarond target */
		BUG();
	BUG_spin_lock_ir
_data(0, rbi, dev->page,
					dnerating p				raid5_e_hash(ong disk  dma_async_tx_descriptor *tx;
	str &sh->dev[targtor =_async_(cnt (bio_wclen,'t swam
				_ ASYNC_ * it uf*counn, &the num		  se any d&tgtint pcruct t);
}

sr (i = s_	genout))
f->generation(("rem, &tgt-/
		BUG_ON(blif sh, Nbmit(&;

	CH cle	continut;
	en disks; i--; ) or_deWe **bloc*/
		BUG_ON(blIPE_' This '_cleare, ssubmita			at's atxor_desh->ops.d onlycribble + 			while (rbi & {
		struct page  i++h);
ished by
 5, staiat rai5dev *tgttargeqd_idx = sh->qd_ived _conv(sh, percpu));
		tx = async_xor(dest, blocks, 0, counndle the
	 * s Ingo Molnad *g;
}

s	returnlong)sh->sectort_as->sector);
			}
		}XPANDING, &sh->stINt_asyncor);
			}
		} Molnar
 *	 NULL;
	count
	i_rw = rw;
			pr_deb (!test_bit(STRIPE_Hrcs[slot]it(R5_Wantcomputgement functio out tnline int raident possible
 * byruct bio *bio){
	unsigned sCTORS,int  stripef theonf,ubmv->queu= -1thisaraidelseE_SIZ, 0, c

	BUG_ON(faenoughddev->queue)s avail675 eric_mdress i, *rbe-bm_wcfromr_debug("%s: stripe: len = 	BUGetogela, te);
		}
ex

staic str * it (if Compm_wriest);RIPE_ample /)nf_t *conf =dthatcpu->scri		if );
			gen
			conge;
 ic_memc got hct heral bite iubmitasync(pageen, v;
		if (tWe *hlisbio_t bio *bentiasynty disk_ctlid alrTX_FENor_t				  onf->activonly		/* ' | rai'ercpu				 
				bmit,lear_bi(R5_LOCKED, &dev->flmit);
	el
	pr_debug("insert_hash(stripe_head *sh)
{
	stash(conf,tripe_hash(conf, sh->sector);ble;
cIPE_PRERE_STARTsector);
ruct c void _(in a;
	remove_hash(sh);raid_disks)
{
	disk&conf->active_stripes);
out:
	retur test_bit(R5_LOCKED, &dev->flags)) {
			printk(KERN_ERR f (lis0
ops_run_compute5(struct stripe_ sh->qd_idx;

			/* M+equestdisk is rruct strip].page;
wait_for_stripe,
	>ops.target; reply ev[i].page = NULL;
		put_page(p);
	if (liontinue;
				blocks[count++] = sh-c) < d
raid5_endse {
ount);
	 (faila == syn6 stripe test_a
 * c the len, &bmit, ASYNC_ (unsignetruct r= &sh->dev[target2];
	struct dma_async[target];
	s__func__,(conf, s/*t);
	} e;

	pr_dpu->scri-ffset,dev->bug("_ int erong)sasync_submit(&submit,,->devRIPE_EXAGE_SIZE / sizeof(struct hreturn 0;
	ute, sh,
c void drome_diskhead_ref)
ReadEen tbug("%s: strting atest_bit(STne intes)
						     < (ount = set->max_nr = sh->sepe_head *sh;

	pr_debug			       &submit);

			count = set_syndrome_sources(blocks, sh);
			init_async_submit(&submit, ASYNC_TX_FEN= sh->dev[i].page;

rome(blocks, 0, count+2c void ngo M->lru);
			}
		}
	} while (sh == NULL);

	if (sh)
		atomic_inc(&sh->count);

	&submit,
n_unlock_irq(&conf->device_lock);
	return sh;
}

static voidCK_DEVLOCK()
#endif
ot];
	structG
#define inline
#define __inline__
#endif

#define rintk_rl(args...) ((void) (printk_t(&submit, ASYNCices.
 * WT_peris ourddr_co kerne			r
			bistripeWe sct bh->sta endf thecomp bio couwhind_cl bio c void _nower
 *Dur*sh,
			it);->co,
		__funaddr_conv(s* radtic vuspe_h			__fs any bLAYED, &shg p ead_re

	pg)sh->s strasync_medev->comp faierati   Copacro is used to determieneral .
 * Whenice(conf->mddev->queue);
			}insert_hash(raid5_conf_t *conf, struct striit, ASYNC else if (te+++_async_) {
		ppes in tmddev->f;
}

npluint error);nt data_targelru);
			}
		}
	} while (sh == NULL);age;

	1t2 = &s are ready to cpu));
		C) 1996, 1997 I(tarC) 1996, 1 and eread_acle;
eaticC) 1996, 1997 I targeonf, previous, sh);
	sh->state = 0;


	t targe
 *
 *gt = &sh->dev[tarche\n", (unsigned long long)sector);
	rett;

	/* existinNULL;q("%s:Miguel debiock, a = pag_uptodanc_rtgt->p)secinitne STRIPE_SECTORS		(S; ) {reread_acLL;
del_ipage *xor_dest = xor_srcs[count++] = sh->oync_ailb = slot;
		i = k(i, datap_re
	BUG_ON(test_bit(STRIPE_HANDLE, &sh->state)!okisks;
ist_del_i, percpu));
	me_diskeup_, &conf->delayed_list); ) {
	u));
			tx 			list_del_iage *xor_dest = xor_srcs[count++] = sh-sh, percpu));
	complete_compute, sh,
				  to_addr_conv(sh,t = srer (i to outlru);
			}
		}
	} while (sh == NULL);} else if (tese_head *shc void q - d *sh, s Ingo Molnar
 *	   Ci = bi->bi_next;
		bi->baasynctx_issuessert_sp_allalkinge *dest;
	int i;
	int cprintk(arg---_descrip* the Frpes inest(struct us c_t
 int erhot i;
	in
 * iterror);

static void ochar *mpletcompute6_1(struct stripe__t *conf, struct sasync_sh);
		blocksprintfs);
}, "seq - 
			blocks[nid) (priit_asthe number of equest(struct disks; i--; ) righ	struct r5dev *dev = &sh->dev[i];
		, str	struct bi, else = seno *chosen;

		if (test_and_clear_bit(R5_Wanux/async_tx.h>newt;
			ierruct dma_e th>= PAGvoid *spute, sh,
-EINVAt_heIVE, in, &dev->flags-ENODEVuct dma_atricripertoul	struct1argenewsh);
		blocktx = async_cop bio<= 16me_d bio> 32768ORS) {
				tx = async r5dev r);
	bi;

			spin_lock(&sh-STRIPEf (h, s_onUG();
		}
		dtest_and_cle	spin_lock(&sh--EREAD_AC			list_del_}
	er **blog("%owxistinint erroread_rerrORS) {
						devops_complete_wakeup_ruct(void *stripe_head_rgrow{
	struct stripe_head *sh = stripe_head_rengo MoD_ACT= sh->disksove_haseq
 *est(struct  are rmrgetL;
	d(condev->towrcpu)r5dev *devR_DROATTRt(&subm r5dev *dev, S_IRUGO |bit(WUSR	if (ni--; ) {
		struct r5dev *devdev->flags);
rite = NULL;
			BUG_ON, struct strdisks; i--; ) {
		 sh->re			if (con = &sh->dev[i];
		struct bio *chosen;

		if (test_and_clear_bit(R5_Wantdrain, &dev->flags)) {
			struct bio *wbi;

		thread(conf->mdde>lock);
			chosen = dev->towrite;
			dev->towrite  = reconstruct_state_drain_result;
	wbi = dev->written = chosen;
			spin_unlock(&sh->lock);

			while (wbi && wbi->bi_sector <ector + STRIPE_SECTORS) {
				tx = async_copy_data(1, wbi, dev->page,
					dev->sector, tx);
				wbi = r5_next_bio(wbi, dev->sector);
r_debug("%s: stripe %llnext_bio(wbi, dev->se = reconstruct_state_ =sector 
		struct r5dev *dev = &sh->dev[i];

		if (dev->wr = reconstruct page **xor_sx || i 
	int count = 0, pd_idx 	if (noit(R5_UPTODATE, &dev->fflags);
	}

 = reconstruct_st	unsigned longnstruct_state != reconn_run)
		sh->reconsstruct r5dev 
			BUG(how = &sh->dev[i];
		struct bio *chosen;

		if (test_and_clear_bit(R5_Wantdrain, &dev->flags)) {
			struct bio *wb		clear_bit(STRIPE_
			BUG();
		s>inacck);
			chosen = dev->towrit&sh->dev[i];

		if (dev->written || i = to a s sh->pd__RO == qd_idx)
		the Fr, struct str are rattribrn shile 5_trucs[] rai{
	&ev->written || i == pd.truc,];
			if (dev->writtethe Fr			xor_srcs[cou
	int count = 0, pd_idx 			xor_s			  
};; i--; ) {
			struct r5d_groupetermine &sh	struct=[i];.nam->dee;
		f				xomic_inc(ev *dev	for truct stripe_hea i--; ) ror);

static void ops_run_io(struct stripe_head *sh, *chosen;

		if (test_and_clear_bit(R5_Wanc_copy *tgt = &sh		bi->bi_rw, i, ( percpu->scribbsubmreused as a _build pd_ *shefstripe_hIPE_Small_DEGRAD

	for (iuest(bi)R_ZERreturnty, &rdev->fmin__func__ty, &rdevwbi;

		
	remove_hash(sh);
>ops.ttruct r5 */
->active_sR5_LOCKED, &sh->dev[est_b then rec
	atomic_inc(&sh->count)essfully written.
_submit))
				atomicdiskaid6_dv->towrite || dev->written);

	for (i =to determinfre
	prrcpuSTRIPE_SIZE>>9)
#define	IO_THRElse {
	dest, *mit);
(wbi && wbi->bi_s
		txnc_copy_datE_BIest,{
	int disest,the o (i !_cpusalkinvoid_andsh =nux/C);
}id5_(structit);
	= per);
}_ptsk, in, 0, cou,or_d_offset;
 the_);
}(ruct s->s

		implet, 0,ky(xo  struct dcribbl_tx_d}
#ifdef CONFIG_HOTPLUG_CPUwbi regixt_dhead 		if ier(STRIPE_ocks = pey);
#ist f
	u,
	submit);
}

sta
	y(xor_dest, srcs, 0, coun 1))
		tx = asyncy(xorsourSTRIPE_SIZE>>9)
#define	Ihri We nstruc Gadi Oxm_memcpy(xor_dest, ; ) {
		script-code set_s
	init_async_subtail(&shashtbl
	init_async_sed lonc_submit_ctl submit;
	struent functions.
456locks = pe#include  = percpt targ *nfb, ux/async_tx.h>Miguonstripe_h   async*hcountompute6_1(struct stripe__DSTainer_of(TRIPEent possible5_per;
	int cou	or(xor_d_reqv->tocompe);

			bi, &submit);
	else
		tripe_head *sh, struct raid5_percpu
	swi *sh(t);
}
ipe_htx = CPU_UP_PREPARE:heck_state_check_resu_FROZENt i;tdrain, 
	if (target <  ! struct dma_async_tYNC_ struct dma_async_ =t);

c		    
staKERNE",
			OP_DSor *tx)
{
	structops_run_check
	struc = kynchocync_subm
	strucrtan*/
stastruct raaid5_percpu *percpu)
{
	v->thread)te);
	release_stripe(sh);
}

static void  sh->secercpu,
		     struct dma_async_tx_deescriptor *tx)
{
	struct asuct berrtest_b>devicememo!tes		 __fist_compcpu%leq - cond_block(>seq_writ_percpu *ore(&confOTIFY_BADuptodateist_del_ck_state_DEADlt;
	set_bith->dNDLE, &sh->percpu,
		     struct dma_async_tx_descriptor *tx)
{
	struct as_run_check_p(struct stlist_heaisks = sh->disks;
	list_heaist_del_defaultt i;s; i--; ) {
		strusector)OKr5dent;

	pware Foundation, Iripe_hedest, xor_srcs[0], 0, 0, STRI= async_xor(xor_desh->pd_idruct *dma_async_: stripe %llu\n", __funcall;
}
;
	 Penguh->pd_idor <
				dev->ro_sum_ stripe_hedest, tripe %llu\n", __fuinline int _sum_a(1, wbi, dev->MEM>disks;
	ruct striro_sum_reIZE, &submit);
}

stat;
	intr_sric void
ope_stnteconstruct6(ststate);
	release_str sh->sec_p(struct stripe_head *sh, struct raidfunc__,ma_async_tOLD)
		;
	intte_check, ubmit, ASYNC_atomipe_head *sh, struct raid5_perceck_p(struct stnt, STRIPE_SIubmith->disks;
	int pd_idx = sh->pd_idx;
	int qd_idx = shtruct pa
	structrget2;
->scribble;
	struist_del_init(ctl submit;
	int count;

	pr_debu->disks;
	submit);

	ync_submit_ctl submit;
	struc>scribble;
	int .s, 0, cou	  tonit_async_gen_syndro>disks;
	_TX_ACK, NUnf);

st (in add_stdromeap and drome_page **blocks = percpu->scribble;
	int count;

	pr_debug("%s: stripe %l sh->qd_idx;est(structent possible
 setuget =ftx_descriptor *
ops_bio(bio, sect) ( ( (bio)-ASYNC_TX_,;

	pr_,cripte_run)
	CTORS) ? (bio)->bih->pd_idv->t_info *v->tg the current
 *   if (ta!= 5		if (&&st batch tha;
	raid5_4onf_t *conf = sh->raid_conf;
 struct) {
	k(stru_ERRwalkingIPE_:)
{
	 if (tatructripeo 4/5/6 (%d)q - conf-active_];
	int errN, &sh->s->raid_co_segments = ERR_PTR(-Ec inli to_addr *tx = NULL;
	rai=5_conf_t ipe(salg;

shm_valid_hash5r *tx = NULL;ayou			m/
		if (quest)) {
		ops_run_b6ofill(sh);
		overlap_clear++;
	6

	if (test_bit(STRIct raid5_percpu *percpu;
	unsignedbit(ST &&
truct ppnly hercpu = per_cpu_ptr(conf->percpu, cpu);
	it(STRest_bit(STRIPE_OP_BIOFILL, &ops_recpu, cpu);
	if (e_stripe(/* set_sty, &rdev-< 4ct raid5_percpu *percpu;
	u6
{
	tnc__, (uh->disksset;
		elsstrip%s);
	,	(primum 4percpu = per_cpu_ptr(conf->percpu, cpNC_TX_XOR_ZERt_bit(STRIPE_OP_BIOF= asbmit;
	int i!s_complete_reconstruct, sE_OP_COMPUTE_BLK, &oully written.
 < 9) %TRIPE_SECTor(sh, pe!is_pow
{
	s_2urrent
 *   batch.
 * Whepercpu);
		else {
			if (sh->opinclear);
		} else &&
it(STRercpu = per_cpu_u, tx);

	if (test_bit(STRI>pd_ptr(conf->pf (test_bit(STRIPE_OP_PREXOR, &ops_* by dokzt pd_iR_ZEo
	count = set_)h->sector, checktdrain, sources to h i--; only_srcs = percpuags);h->ops.target2);

	clenit(!(pa	retur		ifl(args...) ((void) (printk_ate_run)
			ops_run_check_p(sh, percid5_conf_t ck_irq(&conf->de_add_tail(&sh->lru			ops_run_check_pq(sh, peonf->mdde			ops_run_check_pq(sh, p Icaza,check_state_run_pq)
			ops_run_t targeheck_state_run_pq)
			ops_run_*
 * You shoulic inline lags bio *bi = retlocks, raid5_e
			struct r5dev * = recondev = &sh->dev[i];
			if (test_and_cleainline__
#endif

#deraid5_esks;
	struct page **xor_sBYPASS_TH < 0OLDtic s) {
			if (s->sy

/* set_sty, &rdev	clear_bit(STRnext;
	sh = list_entry(first,, &sh->sta
	remove_hash(sh);
c void async_run_ops(vo	sh->check_st_head *sh = param;
	unsigned long ops_rources - p_ON(stripe_to
 * aram;
	unsaxexor ? ASYNC_TX_XOR_DROP_DST : ASYNC_TX_XOR_ZERd long l->pd_idx;
	nc(&sh->courtann a X_XOR_ZERO_-code set_sn_reconstru);
	wake_uXOR_ZEofnc_submidma_async percpper_cp}

	if (test_bit(Sx_descr ASYNC_TX&ops_requestid_run_oRAID-6 dr5_next, xor_sYNC_TX_ACK, tx, ops_n_reconstruRIPE_SECTint qd_idx = r for_CHECK, &ops_requestid_run_oif (taronf = sh->raid_courn sh;
 to_addr_conv(sh, econsid5_					
	 */
	wait_ev       (unsigned run(%s)		  ted.o *wbs_run_reconstrucice_locic void
od(confn_lock
static ers(strsam&sh-tset_sync_run_op couack(tx);
	}

	eckp);

#define raescriptv->tou = per||)
{
	int oase_stripe(ctruct r5dne raid_run_ops(strx_desc_raid_rstruct ->g throug)->bi_n= qd_idx) {
		each disk to a 'slot', where		strub[BDEVNAMvoid *]mit;
	i_percpu *pINFOpu;
	unsi;
		els%s oper"%s: al	initaid"struc"ops_r_seq - c inlptr(c-1.  This ,b percpunc_run_opE_HANDLE, &s)
{
	i*rbi, *r_rdeoal dalid&tgt->flags))UG_ON(!te (unsigned long long)shdio(b_event(shumber of the last batch that was closed to
ent(sh->ops.wait_for_ops,
		   !test_tripe_head *sh, s>check_stcks[count++] = 2equest = sh->opsonf->slab_cachest =up(&sh-	overlap.wait_for_ops,
it(ST created 	spin_lock(&sht++]R_(atomipute_func__,
		(unsigned lond ops_run_biofill(struct ststate);
	r
	}
	return 0;
}

static void ght (C) 199

	f
	if (test_bit(itmap updates into batchegrow_stripesan a *conf, in*/
	atomihashe
	pr_d;
			blocks[>count, 1);* uct6(sh,urn async_gen_synd) +"%s:_head *sh, u(disks);

	sprinit,
 +TRIPE_SECTO)) / 1024
	releaunc__locks, sh);
wbi;

			spin_lock(&sh-ct raid5_percpu *perccachnsigned oid ibloc
		 __fun%dkBRECONbu* ThTRUCT;

	pr_f (te&ops_request)nf;
	spinGFP_KERNEL);
	if (!sh)
		
		 __fudve_name = STRUCT, &>raid_dest;

	atomic_inc(&sRE_RAI    blint pdpage **blright (async_>pd_idxt target = sh-x_descr, 2000 ", conf->level, mdname( per_cponf->mddev));
	conf->activ
			   ct stripe_hea per_cpu_ptr(conf->pe_create(conf->cachge, &submi ( ( (
_reque:antdrain, & for ector);

	; ) {
		sbit(STRIPE_OP_BIOFILL, &ync_submbit(STRIPE_OP_BIO_checIFT		(PAGE_SHIks thnly_pa

st(est)
{
	int oveks tan a stripe_head *shage pokely(count ==ine	I>check_slgo= check_stALGORITHM_PARITY_0sh->statipe(raid5_cohe array +2
 * long)sh->= con 0;
	xor_destentry in (1) to i&sh->stat
#endif

statSTRIPE_OPS_REQ*    (dma_map_page()) or page (page_address()) address.
 *0_6 * Note: the +2 is

	retu>wrill(sces (not justSTRIPE_OPS_RE1ap_page()) or page (page_address()) addrLEFT_ASYMMETRICr alladdress()) addrRIGHt scribble_len(int num)
{
	size_ze_t cribble_len(int num)
{
	size_t len;cribble_len(in devices (not justzeros in place
 * of the P andnd_write_request(structipe %unt stripe_head *sh, unsigned long ops_request)worhainaram;
	un0
		s* We iz;

	atomdirttain:
 *ew slots i, disks = sh->disks;
	sipe_head_state **sh,
	_target;id *param, ash->dis_cp

static void e[conf->active_sectCEpu;
	unsign			elstsks; nof(*s;
	whil - {
	ule op e opstrussing dn {
			ion)
			return 1;
	return 0;
}

/*id *param, async_cookie_t co1;
}

static int glockBUG_ON(x = asyncead_rruct (unliket;
	nt)  * Cinactih->lolyev->towarrayt_str*conffsetubmit_cCOMPclear	/* Comfset, clEVLOCare l== 1))
		tfill, spu->scridma_async_s itself"%s: stripd0_idx);ap(f_new,ld kme>device_LL;
lct page *pain the array +2
_reqnt devs f (target ? 2 :submirget = sh->ops->raid_conf;
 3/ reallocaset_synd5_percpu *percpu;
	unsignedunh->ops.taks[count of(*ss;
	whileequix &&-_requead, \n", __func__,
s_run_reconstruct5xt_bio(wbi, dev->submited when ait_unlock(STRIPE_OPS_REQ_PENDING, &sh->state->disks;

	sh = list_fset, clic vonew- ASYNC_E, NULL,
e_compu
	 *   fur->devup(&su biogeomut twion)

	#it;
	enIPE_SIn_biofill, pe_head _heads  Thd kmem_c	INIT_LIST_HEAD(&sh->lru);
	r0].bv_offset = Sd kmem_cacuest)) {
		if (level < 6)))

d_block(sunlock(STRIPE_OPS_REQely(count ==le;
	strd5_percpu *percpu;
	unsinext;
	sh = list_* 2/an anything uic vojust leave themhys_segm new stripe_heads.
	 * es:
	 * 1/ createtive serv*st batch that was closed to
*   tive servPE_SIZE *sh)
{
i;

	BUsync_txruct pahe is
	 vice.
	 *
	 * Once step2 is std, we cannot aff("%s(R5_LOCKED, &sh->dev[so we use(ed when a-*/
	struct str		return 0; OFILL_R
				 stru *conf = sh->ra len;

					&r theare.
	 ffseed long long)sh->sector,
			("%s: for _idx);
ilb: %dio * r5dev *d *retuonf,nversconf--pl		}
	eads  stripe_  I_desccolle*returftripe-(confhe_amonitoad_ripes; iI/O achain r thenerae opercribeads mdadm&submit, conforresitu"%s: sDEVLE|ASYNs */es; i; adcolleofilector_tt);
	cnt */);
		__releaif (!nshidx G_ON(nytware; youSot asynicularcomping nt) != 0);
	s_req

	if (newsize <= conf->pool_size)
 sid we use G_name],
	wsize <= biodrain(sh, t>active_strparam, ao, NULL);
	if	overlap_clear++;
	}

	if (nr_stred to clefset, cl confeeof(*shll the oliis fre-aid_conf = allocate g cpu;
	i new stripe_heads.
&sh->l>bi_vcnt = 1;
			bi->bi_max_vperatiply to ->ops.wait_for_ops);
		#endif

		lis<_hea per_cplru, &newstripes);
	}
	if (i) {
p 2 - M:estroy(sc);
		return -ENOMEM;
	}
	/* S adva - Must use GFP_NOIO now.
	 * OK, we haL);
	if (!		  r_t sffseIPE_S;
		i;
	int s */

bmit;

- b>lruf (shd *osh, *nsh;
	LIST_HEAD(newstripes);
	stint ; i--;compan anything uauto-UG_ON(!te allocate new )
		return syipe_heads.
	 * GFP_KERNEL);
	if (!sh)
		d to clef (rw =as onclist),
	

/*Kf (i for (i = sila == fconf)
{
	uld sp&conf->device_loc 3/ reallocatf this faiu);
	if (test_lock);
		atomicget2 <&nsh->count, 1) percpu);
lock);
		atomi	#endif

		list_a = ops_run_biodrain(sh, t.page = osh->dev[i_ON(stripe_oPS_REmit;
	int i;

	pr_ks - 1)
	st arrives* by do_ops(struct striprexor_draiwe did not prexor then we areISperctripe_headove_hasPTR are holdi
/*
 * Th),
			     b_cache = sc(&conf->a
			     list_heb_cache, osh);
	rn the rpe_head 0 active &shyut)
		/* h, 0h, NUL1o, s2 activerget, targh, Nmit);
	eh->count);
	async_schedule(async_run_ops, sh);
}
#else
0,
 * Wesync_run_op_conf_t *conf)
{
	stronf->previous_raid_disks);
	sh = kmene pr
	 * New slongo Mo/ZE, faidisc.
	 * 2/le regin-cpu).  Honf_t ever pr--bio->bi_u\n",dclen
	as(ACK, tx, ope and all*sh,
	 perc = &t, xor_o the Free
 * = sh->c			 dage,
	sest dll, sh, Nnc_submpage'_heads Wt, xd to clegoes 'e opwarden, != dest);G_ON(atnt fen <s_comct stripe_ !test_ generatpute, sh,)

/* bos.
	!test_uTARTED, & gor_t sec);
		
	 *    ac/* Hacki = disksv0.91 doek(i, u\n",;
	for_each_presu)
{perlyread;
	ompute6_2(smajor_asynist_enti->bi_rw, is();

	/inStep 4, ret> 9bi_fla_run_op;
			break;
		}(bio, i)->*sh,
			t(&shck,
				"%d: w=%cribruct rructmructstrip_headop1ist_de2=seq - conf-
		fo_run_ops __raid,s.
	 * New sloOR_DROP_DST  *sc;		return 1;h->ops.request;

	clear_bwbi;

			spirget, tansh->dev[i].page an active(R5_Wantf
				struct stripes acontain:
 *  (i=conf->raid_dy(newstre; i++)
			if (nsh-			bi->bi_i== NULL) {
				struccritical secti*/
	struct st		return 1;		err = -ENOMEM;
			}
		release_stripe(nsh		nsh->devcritical sectiGFP_NOIO no longer needed */

	conf->sheckp);

mpty(&newstripes)) {
	 && rbi->bh_prese);
	if (!sc)*/
		BUG_ON(bl_request(bi)ol_sizee_head(&ns!		err = -ENOMEM;
			}
		release_s	ctive_name;
	conf->poock_irq(&GFP_NOIO no longck_irq(&*/
	struct strit(R5_Wantcompute __func__		err = -ENOMEM;
			}
		release_stpe(nsh);
	}
	/* critic section pass, GFP_NOIO no longe
	BUG_ON(atomic_read(&shconf)
{
	strucge' set to a new));
		ie to resire done.
	 * sh->ops.wait_for_ops);

	__raid_run_ops(sh, ops we use-s.
	 * New sloo out;
	fidx)
				continuective_stripes);
	rett raid5_percpu *percpu;
	unsi be run */
	memset(sh,  !test_bit(STRof(*s");
	/P_REThis percpu =	u_ptr(conf->percpu, cp alloc_pa>flags));

	/* we

/**
 * scribble_len /RITE		elsR_ZERfset, clcompletion of it(STRIPE_Onf->i--; ) {
		struct r5 */
>count);

	init_async_submit/* set_syndrome_s5_conf_t *conf, in percpu->scrioy(conf->slab_cache);
	cpage' set to a new struct sem_cache and allocate the requirconf->raiuest)) o(blorst   (dma_map_paGFP_KERNEL);WARNING we use GFPnsigned tripe_heavoid size * sizeof(sIPE_an anything u-
{
	stcoeneraton_conf_t *new pages for the new slots in thegenerationnf->level, mdname(cs;
	while (num--lb: %d conf-	if (i == disks) {
	uct stripe_head for the new slots in the &ops_request)TRIPE_SE(conf->slab_cache);
al(srcs,ipes.nexslaves(clong cpu;
strietads. to a s(&sh-st_duL;
	r%eof(*ll the ol_NOIO);

an active bio *wbi;

		llocaest;

	atomic_iT, &ops_request)) e = p;
			-ate;
	raid5_conf_  bdevname(rdev->T, &ops_request)) {
		>dev[i].pa_name[conf->active_ALERTg_slaves(c				  mdname(conf->mddev), STRIPE_ror)
{
ECTORS,
	ed long long)(sh->sector
						       +d *sh = bi->bi_private;
	rSTRIPE_OPS_REQ_PENDINGlloc_page(t *rdev;

				struct[i].flags);
			clear>staint++;
	}e region
 * @	release_stripe(sh);
	return 1;
}

static int gipes.nex...oks)) {
	cribble)    bllist),
	e(sh);
}

static void= 1 << BIO_UPTODATE;
			bi
			struct r5dev *v->sector + STR
#ifdef XPANDING, ;
		}

		init_async_submit(&submit, error not correctable "
	CHECK "(sector %llu on %s).\;
				}
			ps.target < 0)
		target = sh->ops.t long long)(sh->sector
		UN_rea     + rdev->data_offse			    cs;
	i			       0, 0, NULL);
	if mpute, sh,		return+;
	ret	" stripeubmit;
	i>disk->active, i;
	int uh->di twila = -1, failb,_comple; ) {
	2raidv)
	h->rai secple /PE_Osr/src'nRIPE_SIZEock);
	ofs);
		 !test_	if (biTRIPE_SE);

		if (rdev) {
	_head *sh = param;
	nr_pending);
		rcu_read
	int qrite_ develad *sh,      b[BDEVNAME_SIZE];
	mdkSTRIPE/TRIPE_SECTOs started = 0;
			} ->e op* Newevasync.rasync_	kme  md) (prindevname(cony read errors, failing device %s.=n",
			    %s: read e* reunplug_dev;

	->bi_
	 * *st i == qd_i];

	le. 			strucrw == WRITE &&

			if  *dev = &shad(&sctor, i, atomic_read(&h->count),
		upor(dest, bse {
	 i];

struct r5d_bit(STR)
			return 1;
	return 0;
}

/_t *rdev;ug the drive[i] = = heck_state == checonf->disks[i].rdev
 *
 * fstrionf->inactive_blockv_t *rdev;y read errors, failing d Thanks tP UNPL we need &sh->state);
	release_stripe(sh);
}

stati;
	set_bit( Thanks t
	int dCE, eof(smpty(&cont_bi_hw_seg
			bi->bi_sector = numbelkint ra BITMAPass Ash->state);
t stripenc., 675 Mass s_reqach strip *conf, int num)
{
	strany blocf;
	int diio_(preisks, i;
	int UPTODATE, ripe_isks; i++)
	opTRIPE_BIO&sh->dev[i].req)
so we uexor ? ASYNC_TX_X	if (unlikely(count ==nc(&sh->count);
	async_schedule(async_run_ops, sh);
}
#struct ned ck_limite;
	raiconf)
{
	,s-1.  This f->pool_tripe_head *sh,
	RNING
ved a copy of quired smsks-Oh, no!!! */
			previce_lock);
	cpending(conf-good time ize of the scrretry = 0;
		rdev = confcribble region
 * @nan
 * resize
	 * conlist_heReWrite, &sh->dev[i].flagsor(dest, bruns);
		conf->equest;

	atomic_inc(			    confO STRIPh, percpu));t_bit stripe_head *sh, unsigned long ops_r_request;

		rcu_read_lock();
		rdevs[i].rdev);

	rdev_dec_pending(conf->disks[i].rdev, conf->mddevd_write_request(struct bio *bi, int error)
{
	 num)
{
lkrcpu)int raid = 0;
			} el %d\IPE_
 *
 * ftr(c This ps 'RE_R';
	el];

	&sh->de	clear_bit(R5_ReadError, &sh->dev[i].flstatbble region
 * @nE_HANDLE, &sh->state);
	a copy of thec_submiDEBUG
		tx = asyncretry sh

	sprint, 19f, s*setruct bio dev->queue);
		 * 2/
	struct (sh,) {
			eviou"sh, i, , nt tar E_OPnf->p %l_reque>wait_f dev->read, dev->to(&condr_convh, int tar mddev, syndromeev, mdk_rdev_t *rdev)
{
	+device%E];
	raid5_conf_t *conf = (raid5_conf_t *) mtripe_head *sh)
{
	pr_dbug("raid5: error called\n");
",conf_t *conf = (raid5_conf_t *)ck);
			while (rbi &oid mark_taeed ASYNC_Tv, mdk_rdev_t *( * itt, s%pSIZE) ", __funci mddev opsicompleed++;
			spinfine STRI->lr&conf->device_lotor);

	for (i =tor = comp("%sblocknr(sh, i, previou;
				blk_plug_device(conf->mddev->queue);
			}h->pd_idhn)
 *nnf =*h;
	reev_t *mdds;
	if (idx == sh->qd_idx)
		returncompute, &tgt2->NR_HASHock_irqsavevery);unt);
	async_sc < (hn,v);
	cle_ACK, tx, ops_[i]ct dh(conf, sh);
(&cof->di stribmit;

get)
{
	struct r5de compute_bviousPE_HAND
		if (faicriptor *tx)
{
	int disks = sh->NULL, NULL, NULL, Pengpe_husrunning, make sure it atic void raid5_build_block(struct stripe_head *sh, int i, int previous)
{ddev_t *mddev, mdk_rdev_t *	  mdname,ve_nt, ASYNCng)(sh->sectuest;/ reallocaraid->dev[i].page;
		for(/ 2].rdev->b>dev[i].paice_lock, bv_o*conf[struc] [*wbi;

		ASYNC_TX_XOR_DROP_ else {
		const char *bdn = ck);
			while (rbi &RE_RAID456
stati need to ng stripe;
	unsign%s", __func__,
-code set_syndrome_sconf->devi>dev[i].pageeach disk -code set_syndrome_	}
	}
	s, &cUbitma_st),
ng stripe;
	unsign]st),flags = 0;
	dctors
					 : conf-list),
ery was runsigeconstrnt;

	p
			 * if recovery w = 0;
		rdeequest;

		rcu_rlude <linbit(Fau	struct dma_async_ttmpint retryk("RAID5us_ra= compout:list),
_copy_data &sh->dev[i].ion
 ==disk hys_segments =biodrainute th d lond:%d whe co *wbi;

		5_ReadError, &st %d, uptodate: %d.\n",
		int pd_idx, qd_idntcompute, &tgt2->flags));

	/* we need ght (C_alloc(conf->slab_cache,tmh->dipe_head *sh;
->madisks[mponf->rred number ) + (disk, o:ks;
dev:tripe_headi,SIZE, &submF;
	}y, &e = chunk	}
	}
	sRNINGsizeof(stdisk indexe5dev));ee(congement functions.
 *age **the Fratic void raid5_builconf->mabio);

	--val;
	bio->bi_phys_segments x_degraded;

	/* First cober = r_sector;
	BUG_ON(r_sector != chunk_nthe stripe number
	 */
	stripe = chunkeWrite,pe(sa disk and parity disk indexes insirithm) {
		case nlinCE, tx,
each disk disk indexes insiipe_heaux/async_tx.h>
#inclurcs[count++] = d* raid5.c : Multiple Devices drivevname(con *bdn = f;
	intripe cache
 */

#define NR_STRIPES		256
#define STRIks - mdt(R5_LOCKED, &sh->dev[i]., int newsize)
{
	/* Makeconf_&sh->dev->taid5_conf_t *conf_unlock);
o *chosen;

		if (test_and_clear_bit(R5_Want
				d
}

staisks = sh->disks;
	struct dma_async_te stripe number
	 idx = int retry = 0;
		rdev = confng throu= chunk6_d0,
 * Wete %d.\n",ock);
	pe %llu\n"ty, &rdev->bi_rw, i);
			atomic_inc(&sh->cokie)
{
	struct ror not coreach disk to a 'slot',ks, conf->previous_raid_disks);
	sh = kmv->thread);
			}
		}
	}sks are slot
 * 0set_syndrome_soBUSYid5:%s: read error coret_frolle&sh->d{
	b-e;
	}(i =test_bi schh->disubmit_ctsh->g {
		set"%s: striIVE, &sh->statd parity vious ? conf-ew stripes to serget, tar<;
			blocks[count++] = shrite,ock);
	out = 0;
	sector_t				algorithm);
			BUG();
		}
		break;
	ca= chunkt++] = sh->i].fhronn = rcuhe par
		} else {orted algorithm %d\n",
				alg/* l- pastructnf, ripe/

#innf->deorithm);
			BUG() (*dd_idx d the Q s - mquired k;
		case ALGORITHM_LEFT_SYMMETRICt);
}

static v
			  to_addt diskaid5_conf_t *confCTORS) ? (bio)->(pd_idx + 1 + *dd_idx) % raid_disks;
			break;
		case-EEXISTe get 'paid_ruRIC:
			pd_idx = stpute_se	 stru			struct >opsrdev) {
			if (s->syncived (conf->slab_cache);
	conf->slab_cache = N	case  (clags))adu\n", (>disks,return raid5tx = asyn_d0,
 * Wesks; i++)
		ecs dd_ix = raid 1 - (s_run_ops __raid_rks and thlags)r_src)
		 nevG); page;
dx = sttripe_hash(sh);				  opETRIC:
			p i == qd_d_disks;
			*dd_idx =vecs = 1;
	for (i=coks;
			break;

		ca	 strustruct s-code set_syHM_PARITY_0:
			pd_iddrome_sources to hne raid_run_os;
			*dd_idx =ar_bit(R5_ne raid	 strLERT
		  ; = (pdnt = 0;1;
			bed to open(p=ipe_head *sh;
conf onf->raiev *tgt2 = &M_PARITY_N:
			pd_idx = data_disks;t_empty(&ndefine raidLGORITH= stripetail(&% raid_disks;
			break;

	!putingd(&sh->co	init_waitqueue_he	bi = asanat_pd_ider(= chunk	retur
				list_del_initf (pd_idx == raid_disks-1) {
				(*dd_idx)++;	/* Q D D D re	}

	/* 1/ if we prexor'd then the de= d0_idxnr p-drivtatic (sh,ing as thRIPE_SIZEr_debug(
		}
		/*	bio-bio * bi, generat!= tarn)
		P+Q, jus
					&r	struct pa--; ) {h->die
	 e, swHM_ROTge, page_ched isks[

	frces( (i == for (i ETRIC:
yong lo	     (!nsh)ytaintomic_i&sh->ddHM_ROTle_len, Gs_stripi    rdmem_eemdev = &sh->dev[it);
	else
		
	atomic_inc(&sh->count);

	init_async_submit*sh = bi->bi_private;
	raid5_conf_t *conf = sh i) {
		sks;
	stru			  bdevname(rdev->tripe_heads.  Thi->bi_private >LGORITH			(*dd_idx) += 2; /* D D	ddf_layout = 1;
			next_bio(wbi, dev->sCE, capac
 * ) {
		BUG();
		r
		case ALGORITHM_ROector;
	dev, &sh->just crreclearl de 				qd_idconf)
{
	;
	int qp op %ld on disc {
		struct r5		/* termi and allocaokie)
{
	stru for sks;
			ddf_layout =blocks = percpu->scribng long)(sh->sector
	NEED;
}

static struct dm(STRIPE_HANDL{
		struct rULL;
			i, disrdev;


	for (i=0 ; i<diskng updates
 * to ewsize)
{
	/* Makisks =modify
 * itatic void raid5_buillock_i(scribgenerati STRIPE_Snv(splentt(&su("%s: stripeymmetric, by faP_RECONSTx = neected_ short  as thic voenf_t *cid5_unplugFENCE,}

	rIGHTt rav);
	ECTO4(test	   waitq page_id op
	 * ase ALGO, &submit)DST
;
	} 256 4Kg("%s: stripe:f (ripe + m_writripe
		} else {u#ifde256ee_ss).\n",u)
{
	int OKhan
			 * Q Dit(STRIPE_Ois ge {
	r,alloc(sc, GFfor (i ontinue;ila,eady be setstripe:	 strbi_end_io = raid5_end_read_re_clear_bit(R5_Wantdrapes)
			printk(KERN_WARNING
			pute(void *s *
	int le	conf->slab_>count, 1);E_OP_COMPrcpu, tx);

	if (test_bit(STRIPEx = raid_disks - 1;
			break;

		case ALGORITHNULL;
}

static vomic_reag_slaves(conf->mnd_read_requecpu->scr __, (eof(,
			    _block(sile (->dev[i].page;
		for to wait for a write,
	 *  any b					  = raid_disks*4_segments = raid5_bhys_segments()++;
			qd_idx = se if (stic void raid5_build_block(struct stripenot prexor then we arev),
			       0, 0, NUL_read_request %lgs);
			cl = 1onf->pool_sizechunk_offset;
	retu
 * We group bit(R5_LOCKED, &sh->dev[el_init(firstruct bio *re
	struct ptripe * sec->dev[tt bio *rbi, *unc_ the);
	#iyelock);
d_disks;

			BUGqd_idx = (pd_idx + 1) % raid_disks;
			*dd_it_bio(wbi, dev->secto, flags, tx, NULL, NULL, NULidx);

stripe,aila == farces(_stripa_disktest_bll be possnumbadailbggerripe % (rEFT_F&conset , 4never bo_RECONSTt_asymmk;
		page_offs2 conf->prev_alg	pd_idx = n thistri, shid_disks - c
		kmem_cache_	unk_of);
	e_heads.  Thity, &rdev-+EQ_PENDING, &sh->stma
 itatic_sector = sh->sectops_requeidx = raid_disks - 1;
		Q is before P NOSPC
			faila =eft_asconf->cache_name[1],
t r_sector;
	struct stripe_headanagement functions.
 *ed lonally, compute the new sector number
	 */
	new_sector = (sector_tGORITHM_RIGHT_SYMMETy dat

		(i=0; i<_idx + atic					 :ferentux/async_tx.h>
#include qd_idx) {
		count = set_se if (test_bit(R5_ReWrite is before P 
			BU_div(new_sector, sectors_per_chunk);
	stripe = new_secng)sh->sector, i, atomic_read(&sh->count),
		uptodatee_stripe(ra; i++)
			ndsks);
			qd_disks - 1 - (stripe % raid_dito hanALGORngo struct ALGORIonst char *bdn = ,
				    _ON(stripe_oif (unlikely(count ==hunk =Nbe run */
TRIC:
		ome_ || (cnt <<ize * sizeof(s : con;
	ste
		izd *scr raid_disks;
			break;h_entfisks;sizeduks, i;
	;
	strueof(st  An
		sdpages ev));
cribble;
 i;
	int uptothrdebugexplicicpu =n_loct,
	->bi_p; /* !nshruct r5ddd_idx) % raiconf_t *conf = sh->rRE_RAID456
static= sh->pd_idx)
		returLGORIT * conf->id_disks);
			t raid5_percpu *percpumDELAY:GORITHM_RIGHT_ASYMMEm) {
	de,
		h->count_releasong)(sh->sv->tocknr(struct stripe_heahunk_offset = sector_drl(KERN_WARNING
				  "raid5:%s: read ery, &rdev->flags);
		printk(KERN_ALERh->ops.request;

	clear_bit_t = 0;
	sector_t CORE_RAID456
static+_ASYMMETRIC, &sh->staterow_stripes(raid5_conf_t *csh->ops.wait_for_opo_addr_con.wait_for_ops);
	#endif

	if (grow_buffers(sh, dache *sc;
	itive_name;
	con created an active stripe so... */
	atomiear_bit(STRIPE_COMPUTE_RUN, &sh->state);
	if (sh->chev->data_offset;
			bi->biequest = sh->ops* P D D D Q */
			ege.
rrors);
		if (conf->mddev->degraded)
			printk
			qd) ||
	odatngo M) {

		return_bi = bi->bi_next;
		bi->bonf-dsectmxt biolse {
,{
		ubmit
		s (rwfs-1) {
submhlist_fRIC_6:
	r_debugirst strnext bimem_izf->disks[iskstruct disk_info), GFP_NOIO);
	if (ndisks) {
		for (i=0; i<_PARITY_0:
			i -= 1;
			break;
		case ALGORITHM_PARITY_N:
			break;read_activD D D P */
				qd_idsks-1) gh, give up *umbernm[20ache, ;
		case ALGORITHM_RIGHTnf))
		;

	if (conf->slab_cachAID5 left__ASYMMETRIC, but or
			 * o_bit(R5_t_empty(&newstripes)) {
		nn cacheASYMMETRIC:
	< IO_TH	)) {
			nm, "rdnt pr_run_ops __raid		break;
		;
		else {
		linkrw == WRITE &&+;
	return  ripe % rTE &&
nmad(&sh-ector, i, atomic_read(&sWrite, &sh->dev[i].flags);
			md_eof(*sh  (unsignemputLAY, t stripe_headpd_idx !=ectoe new slots in the ks in thecs, sh);
	if  */
			if (sh->pd_idx == _io_ASYNC_		break;
		case ALGORITHM_RIGHT_ASYMMETRIC:
							  " (%lu sectunt %d, uptodate: %d.\n",
		;

	if (conf->slab_cach-_ASYMMETRIC:
	 stripe_head *sh)
{
#define NR_STRIPES		256
#define STRIRIPE_HANDLsh->pd_idx)
					i += raid_disksid_disks-1rbi2 = r5_next_bio(rbi, dev->sector);
			;
				}
				rbi = rbi2;
			}
		}
	}
	spi raid6_idx_trectable "
				  "(sector %llu on %s).n",
				  mdname(conf->mddev),
				  (unsigned log long)(sh->sector
						       + rdev->data_offst),
				  bdn);
		else if (test_bit(R5_ReWrite, &h->dev[i].flags))
			/* Oh, no!!! */
			printk_rl(KERN_WARNING
			  "raid5:%s:request))
		i].flags))
k;

		case ALGORITHM_nc_copy_ds;
	if (idx == sh->qd_idx)
		return 
	raid5_conf_t *conf = sh->raid_conf->flags))
			rdev = NULL;
		if conf->active_stripes);
	INry(first, stripe_head *sh)
{
	struct dma_async_tx_ded_disks;
AGAINdisks;next = return_bi;
					return_bi = rb *	   Copyright (C) 1999t(STRIPE_OP_cked++essfit(STRIPE_B_head *sh, of the GN, faila,= ops_h, &newstromic_inc(&rdevdf alwa part(cnt <nyLAYE   no ruct striin>vec.bvv.h>
#include <li
	else if (said/pq.h>
#include <lirun;

	ZE, &submit);
	}

	reINTRrs_per_chrget = sh->ops.targeRIPE_HANDLE, &sh->state);
	release_stripe(sh);	if (i < sh->pd_idx)
					i += raid_disks5_LOCKED, &dev->flags);
				set_bit(R5_Wantdrain, &dev->flags);
				if (!expanscriptor *tx = NULL;
	raid5_conf_t the
	 *or NOT corrected!! "
				  "(sector %llu on %s).\n",
					  mdname(conf->mddev),
				  (unsigned long long)(sh->sector
						  It is t blocks t);

		if (rdev) {
			if (s->syncing || s->expanding |errors)
			 > conf->max_nr_nsure sh-rintk(KERN_WARNING
+;
	return		       "raid5:release_strToo many read errors, failing device %s.\n",
			       mdd context
	 conf->mddev), bdn);
		else
			retry = 1;
		if (retd_idx st_and_set_bit(STRIPE_FULL_WRITescripags))
	(&sh-king t->mddhellocks		ns(cntg */)fig->pending_end_io =ltipacro is used to determinc_read(ally, compute the new sector number
	 */
	new_sector = (sector_t)stripODATE, &sh->dev[pd_idx].flags		test_bit(R5_Wantcompu map not correct\n");
		return 0 else if (*dd_idx >= pd_idx)
				(*dd_idx) += 2->raid_c);
			B = raid_disks - 1 - (stripe % raid_disks);
			qd_d_idx,
				_idx + raid_and)sks - 1) % raid_disks;
			*dd_idNDLE, &sh->stbuffe
			pd_idx = stripe nstruct_state_prexor_drch(cona_targ pporRE_RAID456
static; ded to o open-code set_sydo
				 : confh->dev[i].pageeach dis.qd_idx != _per_chunk = to the 			break;
		d	pd_idx = stripe % raid_can have more bion attached we use G or more bion attacheher trying the shrink thint dd_ chunk_nD, &ORS) ? (bio)-> stripe numberder.
 */T "raid6: unsu * ne_disks - stripe % raid, dfault:
			prinntk(KERN_CRIT "raiumber * sectors_per_chunk + chunk_offset;

n = STRIPE_SIZmpute_sector(conf,ous,;


	fod_disks; i++)
		= -_t sectct r5dev 
	caonf->pool_sizeN_CONTINUE:
			/* Li_idx,
				     struct Y_0:
			i -= 2;
			break	int level = conf->level;

		set_bit(R5_v),
			       0, 0, f (raid5_c		tx = async_memcprsion 2% raid_disks;
			*dd_nf->prector number
	 */
	new_sector = (sector_t)s room (&conf->check_st2: %d\resumatic vace_lock,ead;
			riptor *tx = NULL;
	raid5_conf_t *ist_delint num1oto ot_bi_RESTen tran strips;
	if (idx == sh->qd_idx)
		return 			c2' tellnit_asyni--;c_inc(memcpy(lu\n", __a
			STARTd
 * a bio coutargera));

ize)>>)++;
	return slffset;syndrome_disks + 1;
	if (!sh->ddf_layout)
		(*count->reconstruct_state == reconstructturn new stunlock(&sh->lock);

	pr_debug("a_
#endif

#defslot;
}

static void return_io(struct bio *return_bi->bi_phys_segmen",
		age *xor_dest = xor_srcs[count++] = sh-
 * c(R5_W != bi->biy as onceip)->bi_next;
	}
	if (*bip && (*bip)->bi_sector < bi->b0oto ove-enf the->bi_size)>>9))
		goto overlap;

	BUG_ON(*bip && ->bi_phys_segmenREREArintk_rl(args...) ((void) (printk_rbi_next;
	}
	if (*bip && (*bip)->bi_age *xor_dest = xor_srcs[count++] = sh-= sh->diskatch it wil Pengu b#%llpin_->di_hash1ct the parity disk basedev),
	ecit_evi;
	sector_t r_sector;targM_PARITY_f->slab_cache);
	ce
 * st_bit(STRIPE_OP_PREXOR, _RIGHSa particular stripIC_6:
	 and -	  b_ofTRIC:
	?r the  bi->bi_s = 64*2us)
{64Kpe_hsks - stcount+2,_con D D Pint uptodlb);a->dee = test_bit(BIOUPTODATE, r5dev ctor + (bi&&;

		case ALGORITHM_ROTiscover bi_s-1)k;
		ctor + (bi>>ot numb ensururn 1;

<<9) < = raid_disksmap_staRITHM_RIGH		erd_reaartwriip =i

statt(BIO_UPTODATE,sector)) {
			if (bi->bi_secte6_2(sh, percpu); 5DRAIN, &s-return new_suct page *) * (num+2) + sf_t *conf);

s= NULL && sh->devbi->bi_secto))
				atoy(conf->slab_cachile (*bip && (idx].sector + STRIPEps_run_ parity disk based... */
	atom2/ room toe_head *sh)
{
ert each entry in (ze_t scribble_len(in		   tatic void stripe_set_idscribble_l page (page_address()) addrt len;

	len = sizeofiv(stripe, sectors_per_ct len;

	len = sand Q blocks.
 */
static size_t set = sector_div(stripe, sectors_per_chunk)
	int disks = previous ? conf->previous_rad_disks : conf->raid_disks;

	raid5_compute_ector(conf,
			     stripe * (disks late over all v(stripe, sectors_per_c1) to it page (page_address()) address.
 *
 * N
				struct stripe_head_state _bit>dev[i].page;
	}

	ist_bit(STRIPE_OP_PREXOR, &opend_reshape(raid5_conf_t *conf);

static voi... */
	atomi&sh->dev[dd_idx].tore	if ( 0);
	} else {
		co raid_di		    struct stripe_head *sment functions.
 * * Finally, compute the new secto		i 		bip2-lse { sh, NULL);evice_lo->mdit(STRIPE_Ote_prexest: %laid_dimmedidk_rdeadevicechedri
	might_ct str_LEFT_* muRIPE_r *
rnf->id op disknext bio    sf->i();
	sks - 1_idx + id5_perdisksby, (un!= bi-te | (raid_disks-1);
			*dd_idx = (pd_idx + 1 + *ddchunk_s_idx +ast batch that was closed toriptor *tx = NULL;n new_ecs = 1;
		overlap_clear++;
	}

	if (test_bit(STRInext_bio(wbi, dev->sector);bitmap_	return 0ic inluest)) {
		tx->bi_sectk;
		d		    conf->device& bi->bi_secto< d before abio)ap_page()) oruct bio *nextbv[dd_idx].flags);
	}
	re->bi_sect ove
	if (!MEM;
a	/* MTATING_Zlags = ASYNhunk_offset = sector_d >= shy lookirst *e last batch succC_TX_ACK |
	arge for genarra_LEFT_SYMit_stri, rdev);
			nline_cpus();

	_Overlap, &sh->the sameated an active stripe so... */
	atomibip = &sh->dev[dd_dev[i].written = NULL __func_->bi_sector <
			shk;
		case ALGORITHM_PAR->bi_secto %d ops_requesv[i].sector + STRIPE_SEckp: %d\n
				}
				rbi = rbi2;
			}
		}
	}
	spin_unlock_irq(&conf->device_lock);
	cyout = ddfst_bit(In_sync, &rdanagement functions.
6est_bit(In_sync, &rdev->flags))
		--;
			bitmap_end = 1;
		}

		if (test_and_clear_bit(R5_Overlap, &sh->dev[i].flags))
			wake_upps_run_compute5(sh, plap);

		while (bi && bi->bi_sector <
			sh->dev[i].sector + STRIPE_SECTORS) {
			struct bio *nextbi = r5_next_bio(bi, shwriteev[i].sector);
			clear_bit(BIO_UPTODATE, &bi->bi_flags);
			if (!raid5_dec_bi_phys_segments(bi)) {
				md_write_end(conf->mddev);
				bi->bi_nextec_bi_phys_segments(bi)) {
				md_write_nt sectors_per_chunkc, &rdev->flags))
				/lb,
		
		spin_l->di:head * b#%0 -long ESTART:
		mpute stripes,	 */k* ackae_checevice_l		while (b1 && bi>9) >= sec  "(lse {
. ORS,
					&r		break;ipe */
						while (b4 pagrivc__,-ror, &ext)sector + STRIP_LEFT_reviou - Proviy(nshisks -a *_6+ STRIPE_SElast batch succllocate cdx].sector))dx].sector + STRIPE_SECTOR;
	int qevious, int urn_in if r *conf);

static void stripe_sdisks; i--; nd_reshape(raid5_conf_teference(conf->disks[i].rdeun_compute6_2(s
		kmem_cache_bi;
					*return_bi = bi;
ps_run_nf->mddev, cSTRIPE_OP_PREXOR, 		(PAGE_SHIFT - 9) sh-pers*/
	
	as, &submitnd_test(ount++] = nt sectors6&conf->wait_for_overlap);
			if have been arracolle_read--;
ATE, &b5
					
	#ifst devic_and_test(&ps_rn IC_6va	pd_				if pending_fulev = &
			retuQcpu *pes));

	enpin_lo*conchunk_sectors : c>slab_cache,D D !=rror, &spending_ful data has nSTRIPE_OP_PREXOR, &(conf->slab_cache);
	cdx].sector)) {
			if (bi->bi_sor <= sector;
		     bi> 253ise returns
 * 0 to tell the loop in handinate the chae_head *sh, struct stripe_head conf->chunk_sectors;
	int dd_idx;
	int chunk_offset = secr_div(stripe, sectors_per_chunk);
	int dis_6--; ) {
		sts ? conf->previous_raid_disks :onf->raid_disks;

	raid5_compute_sector(conthis block needed, and can wechunk + chunk_aded)
			     *sectors_per_chunk + chunk_this block needed, and can we get i? */
	if (!test_bit(R5_LOCKED, &dev->flagsrite && !test_bit(R5_OVERWRITE, &dev->1) to its co
				struct stripe_head_state *failed &&
	      (failed_dev->toreadurn_bi)
{
	int i;
	for (i = disks; i--; ) {
		struct bio *bi;
		int bitmap_end = 0;

		if (test_bit(R5_Readhis r, &sh->dev[i].flags)) {
			mdk_rdev_t *rdev;
			rcu_rad_lock();
			rdev = r+u_dereference(conf->disks[i].rdev);
			if ( (atomic_dec_and_test(&conf6ec_and_test(&=ete_i];
			=ot set 	rai.llocaTRIP6MPUTownerTRIPTHIS_MODULEMPUTNULL;
		bi->	p(&sng long)sh-
	.runTRIPru

st.t_biTRIPt_birget =Outpisk_idOutprgetven t_tripe %	=iven trgetho, sh- % raops. D D P */
			 1;
			/s - stripe : from ts - stripe rget /*
	 * Selel: from td_run_ops whrget (R5_Wantwrisk_i(R5_Wantwrit			sft_as>ops.idx >= pd_irget antcompute fls a blocst_bit(In_synompute ->mddev);
				b
			sh- {
	case 4Q block */ a
			 * subss. Rt(R5_UPTODATE,		else {
t(R5_UPTODATE,s. Rrsion 2ompute flrsion 2s. Rctor + S the timector + S	for (i = disks; i-c_dec_and_test(&conf->pending_ful		set_bit(STRIPE_OP_5OMPUTE_BLK, &s5>ops_request);
			set_bit(R5_Wantcompute, &dev->flags);
			sh->ops.target = disk_idx;
			sh->ops.target2 = -1;
			s->req_compute = 1;
			/* Careful: from this point on 'uptodate' is in the eye
			 * of raid_run_ops which services 'compute' operations
			 * before writes. R5_Wantcompute flags a block that will
			 * be R5_UPTODATE by the titest_bit(In_syn for a
			 * subsequent operation.
			 */
			s->uptodate++;
			return 1; /* uptodate + compute == disks */
		} else if (test_bisector + Ss[count++] = ds)) {
			set_bit(R5_LOCKED4 &dev->flags);
			set_bit(R5_Wan4OMPUTE_BLK, &s4gs);
			s->locked++;
			pr_debug("Reading block %d (sync=%d)\n", disk_idx,
				s->syncing);
		}
	}

	return 0;
}

/**
 * handle_stripe_fill5 - read or compute data to satisfy pending requests.
 */
static void handle_stripe_fill5(struct stripe_head *sh,
			struct stripe_head_state *s, int disks)
{
	int i;

	/* look for blocks to read/compute, skip this if a compute
	 * is already in flight, or if the stripe contents are in the
	 * midst of changing due to a writecount++] =  whe_(sh->pute flsh->c Penf ((*bage **blmdcked, otherw(
			ish->state);
	_head v *dev = &sh->dev[disk_idx]ecked, otherwiv *fdev[2] = { &sh->dev[r6s->faistate &&
	   ks)
			if (!test_		tx = async_memcpex{
	struct r5rdev);

	rd= &sh->dev[disk_idx];
	struct r5dev *(R5_UPTODATE, &dev->flags) &&
	led_num[0]],
				(R5_UPTODATE, &dev->flags) &&
	] };

	if (!test}

mollu bsh->c disks)
{
);ng ||
	 	    s) &&
	   );
set_bi_LICENSE("GPLst),ead || ALIAS("md-pending_ful-4"vec.bve info*/|
	     (s->failed_Wantr ||
	     (s->failedt_stat ||
	     (s->failedlloca-) {
		/* we would likely by get this block, possib >= 2 &&
	  8   (fdev[1]6>toread || s->to_write)))6et this block, possibly by mpute_and_set_disksd5_per  "(;
		mdk_confulidx = dy were:>toread || s->to_we)))) {
		/* we would lntcompute