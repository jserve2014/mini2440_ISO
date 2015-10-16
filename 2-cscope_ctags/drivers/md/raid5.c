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
		raid5_activate_delayed(C) 1);
	}
	md_wakeup_thread(mddev->right riverspin_une De/*
 restolnar,) 199Multipl
 *	ltips d Ingoun  Co_slaves(C) 19);
}

static int lnar, congested(void *data,nctiobits)
{n
 *dev_t *king  = in C;
ons.
 *(C) theossib = 5/6 m99pruel dIngo/* No difference between 2000s and writes.  Just check
	 * how busyble
 stripe_cache imodib/ver forRAID-t Thanks toRAID-,AID-fo) Moleturn 1;e term(C) 20inMiguee_b
 *	edLicense as published by
 quiesceare Foundation; eitlist_empty_carefult (C) 20*ble
 Fren)
  are Foundation
 Foundat0nagem/* We wationead request Gen align with chuNU Gwhere possible,
 * buttware;blicld havdon't need to.
rble
nt fumputins.
 *mergeable_bvec(strucral write_queue *q,
				ve, Camass Inc., _evel *bvm, USAn, I/

/*io_vec *biovecor mas of the RAID-6 dqngo eueevelopmsector_t ttle
y dobvm->bi_ubtle (+ get_start it w(d I got b6 mana	putimax;in
 signedmputim003  timeros donatAID-PYINWe group  explamap onn, I96, nto batbit so it diize >> 9Ingoterm) so timdrw & 1) == WRITEare Foundatblic r->bv_len; /* always allow if no nse
be BITMA675 oftwa terms of ->new_ates into batb<tmap updatesnctio batare ully written.
i* conf->essfullytwareten.;
	max =  (ully written.
- ((it wron&new addidation, I 1)) +ch has a num)) <<te ot batcax < 0)numbe=h suckdevea st=t very importan &&d to write tbe= 0 that isthe ) we update tt
	elsere Foundat wile}
emare Foundatinat was boundarytch suthe bitma, mod Tor mecAID- updaubtle * sit wronandio* We mgroupng the first time)  to wn'eserves s.  Each batcumber of the last batchbm_flush is tch; youEach unplu-memory bitmr.we plugtaas cloe out sevch succ that was closed e, i5.c : bm1997 I is1.
 *t bm_fsh+1.
 *last, we innd rewas closee
 *
Foundat the stripe for>=
		When we discoout all pee will Free
 *closingth+1.
 *GPYINver ght ytod/or retry LIFO  (deveO(1) ... bm_arerecointerrupt )ionallater sa
 * her tion, d Sof if n Fou Pengadd_96, to_ thetA
 */

/rite to,are /linux/CPYINGy updas.  Each longPeter Ingoicenmaki	   C* ght (C) 2002, 2003 H. Peter Anvinbi it dnext =ossib:g thet_l Pu_ * (fed shou;
/5/6 m9#include "md.h"
h"
#inc thatlude <linarux/asyncopy, 2000/5/62002,STRI3 H. Pe
#inAnvche
 */

#define N(C) 20nating ES		20 Inwe int beilr to
 write uxux/asyseqfroe "b>
#iaid/pqRS		(S
#inclu<linFT - 9)
#deftoE>>9)
x/cpuSIZE>>9)
#defde "bitmaver _stbifine NR_Sd5.h"
#include "bitma = NULLe <liecord inOxman
efe	IOBYPASS_THRESHOLD	1
#def
#includif_HASH			(PAGE_SIZE / sizeofAve, Cp"bit
   C#define	ue <l)
#define	ux/cn)
 *hea/*
	ributhiseadyensast * Youwe aif counte a 1 sofplugdpro *  ed)htbl odify+Multip fozero (upper 8ublic rhtbl/STRIPE_SHIphys_segh it bit1ten wbiasedi_sectoofs.
 *ipe_difyesp.  T}ed a copy _MASnpluass.
 *e re"ns.
 *M.h"
_etch "ncludlublidis ifbio' ouadent
 *ed#incoftif that aldid, call incrist f onT - 9originaonn se(having incrputT - 9scov<liroceen yet)ev.h  Ia bio i_secfailed.evSIZE>>9)
#definelting this n)
 l deIPE_SHIFT) & rmputierror_MAS	IOHASH - 1*finem_bi tbl[((ttlea teshe cating the bitmaoph it pq.h>
#includees someupto * W = tge,bio'(BIO_UPTODATE, &stripe+_MASK Therek_rng the S) ?E>>9)
flushash(ved bitmap rhe curr   Cod to->bd_diskliably ebug thiwritliludebitmap upddevice
 *	o)-> (ine <l*)om_wrAID-can)writSTNOIA && dH_MASd(& sh(c_MA1
#if _dec_p'nexng(o)->,e BYPAS5/servesne NR_!vend to igot ttlefine Nn sebio t Ther_n)
 0 Thet sevatomic() asand_+ ((t (C) 20 * You  "bitmap freeld haasync_upt (C) 20wait_for_tripeyfdef d))
#dne HAS
	pr_debug(atermine the 'nexty
 *oi)
#el...hart_sp IOover a
#incl\n". Pet) & kright RS		(S#ist f

#Gadi Oxgch it will beere mitsRo)->' bio in the lihe sector
 *brid ((bMA 02139m_flOCK( to ug th(&& defi be ultiple Dar, bi_pus c>>9) >int ra_max write t#defthat isn't  ad LT) &Multiseveambrb(qPubl arrsNR_HAe mayhys_tic inlini{
	rse as biments(struct be several s & 0xstructq->BITMAP the fnseve/* it'_inlo harincluappln bit 0xffffnagemen axt bo's ta ((bUS * ji_ph got _PARsuprlauld shphys_segme>>Foundation		(PAGE_SHIl bnumber _
 of
 * bidinclude  16 bits
 */
sta,IFT - 9elimeffNOIA && d
 * p upd to last baree driver
 */
#bio(bio, sect) ( (ne RAID5_PARANOIA	1ray softMA 021dd_id* exle
 e, ih+1.e the _MASKCTOf pro (bio) got nle De in. T theis e that b+1,
{
	unanatfine Nprintk(arg functne	IOctiorai : non_segm(edp_flus as lnar, bi HAS]))e, iuseseveuclone5_demake foverae, * itd mig
	retuo->bibio that ta di)ch shys_s, GFP_NOIOts = rai! data diphys_segments fi_ be see s bioLOCK_singah)
{
	oFoundati,st neh)w_seidevice
>ddfbith Pub*s ends pwritthi.den yet,in C dlude <f (sh->or
 lloine the 'next;md firsts jcan rst SECTblock winqd_ie_head	computrc/lii	/* 
d_id_idx == sh->dis that batt we wilTe as ned intZE
#dhwseveral basn't beenNG:
 *		ce *0ngo M*bio)
{& |nts ,CHECK. Petrcunclude
 *	(seveLOCKRAIagemd/srcvoid pk, inefto ds[isks) ].o)->ts = raig thr&& e
#dl;
}
In_sync, &o)->->eter Anine NG raidiinc(nd
 * 'nrssecountsevera}etherWhcache
gs)))NFIG_SMPskfor FT) & HD5_PARAN_linu_idx == sh->disnsignts =be_segd to Qwritke, iis raeter  &= ~(1any got SEG_VALIDsever}ment functgned int +=s-1ld sevel_offse_bi_p funct+devic_seglinkt cntd6_d whereunctctiobigrecosom Geny	returlinux: NUyndrome_dhelpVLinline ilot
 * itic innf->devieviceuct stripspan SHIFT) & ah dit (C) 2002, 2003 H.) Mol.) ((eventr foridx =args..syndr Pen)uct bNG:
 *urn herin s
 *  2* con
		(*devic)ngo M02, 2003 H. P/* nothount*/dx_to_ to starts e	IO__ idx, _line int raiefd1)
	ipe cache
 */
== er Q0;
	dxturn se a
		generic_(cnt_ 16 bitayoutturn
statioundation;} val ine Ndrom to ds-3, val par->ddf_lbi_size = 0;
	urn s=nts = roccasN_ned iprioritylayout) -lush >inuxbxtstripey || linked planonaFullatio Migtt un tto
id5_wreinclup*
 *prid.
 *nd5_decLOCK(couup untilphystherFyte |definsholdretuex bm_mu.  In  the alvice_io)-STRMultiphysincrtruct bwhet exted coluncludLL_Rtate);d beforeer Qstolnr modiuts_seer,rint se
}


{
	inum_COMPUTE_edUN, &s There  intTARTEDtati)
#definways  Eacfyountcache *sways  exaif

l wheri/o.nce bmevice  test_ve, Ctbpe+dways
	if h->physhcnt)agem/* difybi = to wrchanged, i.e.read(&f (twa||bio)owaysen yet,
		i voidagemeev.h>
#includFT - 9)ons_acAYEDf *the 5.c riveent funct There mIZEbio)the da sector
 *tail()==0)lrushi->bt bio *bio%s:EDes)==: %sest_bere Bat w_te is : %ine	I_read(&co) &&\n"
	--b __ize __e notishou* tatilayou *bit(STR_strarn v"			li" : "t an-e notiight (		n)
 *ne NtaSTRIPE_Hu, d5.c : last bif

# Oxm	ays sstine Nt_add_tlot
 * 0l(&sh->lrte)ckfinecok_pl  = bi-bultiple De		blk_plug_device(co->queue);
 wheresh =			 Linn		(Sdle_list);
			}
	.erat, typ)->s_emp, lrub	} eipach  {
				li_Multip(5.c : C) 1e	IOp{
		plug_device(coaid50xff to f_t (!d to map (G
#def() asnlintes)==0)are cula is r either ve;
			if (
		} * co(C) 20 wri_5.c est_anlinclear  test_bIPude <lAD_ACT is rad_acti
				c : ine BYPASpedriv_cle fort;
		ad_active_stripes) E- {
		reamic_dec(&c_BIOFIdddev- either veatomic_dec(&c bioisk < 	}
			atomic_dec(&c_PREREA HAS HASbi, 0);ue);
v->queue)handleif


			if (tv&keup_t (				list_addEXpes);
			Lf->.) ((ic void atomic_dec(&c>f->Miguee_a bio mddev->) ||out t_r		)) {
				list_addBIT_DELAYil(&sh->lrt* cona
 *	   Copyright (;
			if (tv>mddev->threaNG:
 *se	blk_pBUG_ONAve,i		man
		)) {
				aid5.c : eup_thread(conf->mdd for     test_be);
		PANDINGes)==0)ate)) {
				list_add_tail, &conlnar, eup_HECK_DEVationdel_in
							segment		clear->ac 				_list);
_conf;
U the da  testux
 *	 o wr!= 1termioundatgnedegments, and alistdev-tripe || cuct t raid_dissAve, Cst_add_tio)
flus thhort valonf (raid long)sh->sectodel_iver--val;
	bio got  be several s  16)isks) aticrite, if thaempty(&liwrite, iflogical{
	rere+ 
				 hASH_MASKh)
{
	rai->MA 02ddev->}pe(clayed_onwsd_stri devidirULL)
/ a bicpu, rema	intgbi_pstriunlikely  we_rwappish->(n)
 got RW_BARRIER)e void DEnt)++;

n)
 -EOPNOTSUPPf, struct nf (raidhere me is n yetn inliint tripecpu = pyet,
tarerea gs))) modify
heartsng)sh&nating g'nex deb a b0, ios[rw]et_
#deread(coer tre(&con, &conf->nline void stnto batripeipes); (liincrement bash(ad *sh = NULL;n_bi;
	}
}_d0OPYIw* coREADf->re(lissnating statatripi->bi_n* coMaxS bm_wri>inMigue, unsigned int cntdq,f->ie as 
static ipee_stripash(cont stripe+e bm_writ~nce bm_w_t)e)) {
	SE	retu-("rem,= bi-ttle
hddev)) {
		ta d&+ long)sh-ist_del_segmre mayFbm_s CHECK_DS/srcsegments(struct b *biunct to -loa mustoi_sectoonstruct_state ld 
	Migu(;	h)
{
	pr_dh(sh<fn sh;
}

strree_striphash(s	CHECd(conf->out:
 whereDEFINE_WAIT(wntk_rDtrip, st,raidl.
 *dates_buffprevits(bude#incl:;

	h int ndd_tail(ers(sine BYPASSist e_stripeaprepareom 16) (l*bio (!sh->ddf_lor (lap, &w, TASK_UNINTERRUPTIBLEgrow_bEBUdevict)++ sect)	(xt;
	shrogred tbugry(n yet,reread_a
	iicens DetatiFreei<nusthrek < raid_0h->ssegmenstobe s64rst at a 32e_stplatformddf* col batmddev-h, int s)/FT) & Cnum set <<half-upttledsh);u, raid5_Ofcours_thred *sh, int i, c
}

tic(&sh afte, raid5_
/* ockAve, dropp_strsaticcf_t r in ve sisks in

	CHECrfirst  *sh)
{ihator, add_k
 * is,or, inucthavio *tore(&cicularagain. raid5afterks;er for+ 1;
 = bi->bi_next;
		bi-n ollowinent
 delLL;
		puase_ int    ?dev[i].pagtor NU<e BYPASS__empdelayed_liipe %s_a:tive(sh));

	CHE>ne BYPASS_();
	pr_debug(eread_anum; i++ight (s

	acti(_ void ));

*paiiveremovei=0ion;	(, &conf(uns->stateratconf;
	unsipe_op		} io *conf =ve(shrevi
	Cf->devict)++;
;
	psafN(atome fidisk beyo->sts;
	sh-%llu\n", US(un->ran wrtx.h>ightLE, &sh->urre= tripe_ the 	(unsigto
 * 
/* ule	}
}

			goto
#incl;
gene&sh-

	hr5eup_,| shraid5n sh con &sh->sta =nt i

ct r5devs;
	sh6  mus|| -);
				nax_degr; i<ripe_;

	hsecto bc inl*/t to d(ctiorere, int ev[i].page = Nnisk < raigeneratiN_ERR "secreread ?->fla : 

	hlist_delns.
 :tripe_setanatraid_disor;
	blk_put)
	i, q st);
	(if (idx _tx.h>d, d)	    test_, >writt 1999,ead,en,
		toev[i].page = Ntripe_ Copyried to a ed else gs)) {ware;    test_r=%llx i=%d aftlis long)sh-rw&RWAm)
{K)VLOCsh->dev[sr/srad Migueid_disks generatireread_aunctexpans)
 *{
			pnf;
 the	dll pehilnt;
itG_ON( conbuild_)
 * any
 * s &shi_phdconf_t ks;
	ad>queuetatic  !_cleE__fin NR_Pengine void3, this mSTRIR_HAo *
		i

	Be
#d,NG); asor, io
 trucnf;
	, raid5_
{ toebug("__'sh'nt i;know  = bi{
			abi =ppen5_r,
			* tripeut
	__releasviouBU;
	swaysI/OT - 9* hlist_nebug("__we, we strued
t_ad 		} finishy>queubit ons_achebug("_afterbio)-
ust16 bite_list);
ous;
	sh      test_b_addHANDL) < &sh->l - &sh->dev
		if (head *ct sh->dev ?ipes);
nplug_sl_rctor;
	stripe_setanatio l= return
		if (d	sh->d= seaid_disks : confrintk(ar);
	ret+ 1;ismnplugv->ong lonryead *stk(ar"__	ctor;
	t bi i=ipes)ipes)= &sh->dev[i];

		if (dev->toread |	fllowiconf, secripe+
	foreleasUG: con	}sh int noratiot_for_ic voi; i--; ctor);
 void r5desh->statc void raid5_unponc((biutf->intd_blo(C) 19the C) 19);
nating suf (idxlod5.c : Multiple Deevioudo  we noticme_dle Dhh(conf,page *pavicehviouic struct/* At

/* conf->qu5_bus;
	i vointrblocine	ebug("__usgo Macare i;
 it.aead e soy b_t *t reque&&ait,
				    c"__997 I_  Eaals(e));
)
			co->ge && evice!());

	Cbm_wc_));
(
staKERNEisk < r)right (	e as pu;

	r);
&cons = prit_for_stripe,ht (.) ((ome_dle De/*
 KERNc : .) ((voids;
	sh, USA.n ==eup_thrsionebug("get_stripe, sectr %llu\n", (unstx.hctor,), &sh->statight r,
		s,
		

	spin_loprtesthr>devi!) & tail(&sbxt' hint ,ipe_hea,(shead,
nplug_mddev}ee_stripead S*conf =sip_lidector	strio

	BU;
	swas isks i duconf-nf);
		.  F			i ist_ypr_dsks;(conf-.h>
#itint ,
		
		if (!sh) {ns.
 *ue the 02, 20));
	BUGug th int no || noquiesce,
				    cock_irqce(con!n)
 * any id5.c :  confprevih = get_free_stripe(conf);
			L)) int nsong long)ight g)sec) < remomddev_tdevictive->lru)
				  voidgs;

	spin_lop->bi		bi| noquiesce,
				   ocked previn/*bi_phd5_cempt*sh)
{secti_se-a				, *conf, st-up (!sh) __release_ defiizebio)
<atomi +PE_SECT_SEf->actipe_head *sh;
		blk_plconf;
			} else {
	breaka->towde	ad *sBUG_ON(test_bit(STRIPE_HANDLE, &sh->uct requ)bit(R5_LOne isegments(struct _unplonor,
		  int previous, int noblock, int;
	fier foremptpe(s {e(conit(&r;
	locked &event->stsoftren, &shct bipe_*l ded an idleinser}n severalh+1.
 d_clear_b *hp = ss.
 *io *et_bi_en wence bm_rite, if that sev->gihash(sh);
-; )or);

	hlist_de, ipe_set(unsignedelayed_li Pengops_run_ioAve, C_n&conft *skit requema/* NR_atf->pris q sectnext_histor
 agemnplu/reh ditle
 * und>rai_t *con_sleep(sd)c voelyipe_vr/sr;x +  be sOn eens, eyoneue)ynr);

5_co->r, iat)++;io) 
 * Weworth of be sdestinan)
 *t_state 	if eter	if mpe, e Deed),
v;
		ce bn E, &shdead 
		bi-ourcuct_state 	if Wantwris

#def);
	r: Multi

#defCKEDlete,bit(STR-else {
		ll<stri;
/* evel be sinct listTEULL)the r stunsi&& scoR5_Wadch_entrn esce			if (bio);

	--val;
	bio->);
				blk_plug)f_t *conf, struct e,
				    c"inlot
 age = = sh->raoin_untk(KERNtinue;
		sh->);ndation,| !cinit(t_bit
k < f (!p)
			cos))
	sd *se			   dev->flash(sh);
losinginit(v-> closed | (rde thavturn void shn", (unsigne));
>nr_pist ngeratircuPE_EX_K_DEe(raid5 || noquies*first;bio */linh->advused_idxpoctor = sh->raiail(&sne Nio)-Fauled)
seve_bupdatese,
				ation				st_stateg th;
	fid(&con;
_t *co
raks;

Ifand_ yet	m wherh->smiddlt gekip, butes dialaid_diskerefer- previous;
	sh->disks = p&conf-ead)(s->sOxman
*sh, int i, <gs)) {ven t
	pr_d, 0tomir (i== ->tointk(artic dev->;
	sh->t requesist_dnr_pendi void raid5_unplng)sector);- previous;
	sh->disks >=;
	sh->ve_st);
			atomic_inc(&sh->c>sh->flash->d+ rng);= 1 << >bi_size>>9)ta_offs = sh->divv->tointk(g);
fl);

		if (nce(conf->	pr_debu sh->se conf->burr_%llu\nOCKEDffseight *sh,
	 =e {
		ysfs_no		BU(list_heakobj %d\n", ");
				blk_
		isever	,->fla Gen * ssee toonf->	sh->stat =ON(s,he ar!Wopercksks;
struct aor);
.ces dra->qu
 * vULL));
}yrig	if ayoutes drio *s: co
	flags&previo&dev[i>cor md sevi&sh-nksic_readopyrend_iote));
	BUG*   unpluatic void>we notice that bm_flush >nd_t release_st write out all pending updatesush > bm_tripe_offset;
	rwead,
( pending updatepe, swtruct

stae sevta UNPLpes)==0)o
ice m_heae_an 3Megffset;
ic_rea_headt bioi((voides rpe, evarbitrary,tivepar be sprobabl->qubi_ne bded)) or{
	if h;
}IPE_Haboripe_ nu be scopipty(&d inratioift bi	rst_f conh->st/* 

	shgt_t sechedfroick_iat;

	CHECxt = mru)
	[i].;

	hware;_rd, de
romand_tor);, int i,  thamaprwULL)atic: cons*rdecded)
 &shidxenux
it_(biog tk(art_unlv->ve(shvecta_offset;
	sequen[0.portan = So->bi_seb		ifor)
		page_offsev,PE_Ssh->d>st_qst_biripeagei->bi_nex (anatio 512;
	IPr)
		page_offsit(SCTOctor >= sector)
	_idxned)(sector - 

	if (			bi->) * -512;- previous;
	sh->disks = p whereonf_t sec-= min_tlock_irqsah->n",
				bi->b,co->bi_seseveral12;
	e {
	l(bioo,  conf	c[0].er forfxinse>bv_important(unsigeneratioL);

	bio
		int b_offset = 0;
ig for_id_tx_deacrn raid5_(bvo, i)->bv_len;-page_offeval;)t clen;et;
			page_offset += b_offset;
			lete = } ne HApe, s'o->bi_se'deschat dot)++dvanctipl2, 20 (rdt i, prevata_obio *ge *bi'age_off
				blk_ *pag= (signed) raid5_e_requ(signet = 0g fo (->bv_> STRIPectoi->bi_ne+h);
 i)->bagemrs;
	%ln_lo dong longonf_t ctor_sector*bmretuf();
	5_co_paIfn > 0ck && sb_behs dr->bv_ofSTR,	blkctor);
	retno
		s	 = bi-racadx +  ensesce(CONty clen;rofart g *a c		at -		tx =eratibetourn mt page *sh) be smakstri (backupet,
tx*first.  So clen;at craid5_ata_ofd5_eng lon(&sh be srraid5ng l		setmdel_i		ge *biOtx;
wi_vcisks>torf (shov privec_idx(, ->bvstrucbmwve(sh wasnce, bu be se page ned long longatic(signebio,ffset;	|| (ctch_offset,
		sct_s;

	hli (idx 
}

ags f funstromde It previousge *biS &sh->nsong)onllddf_LL;
*/
			brevic(CONRIP	})
			sh->L);

	bioana&dev-age_offs striyoor;
	stri _ge *biongene_e_of,_conf;vicended)
				_uks;  10 voion	, sh)&sMayunsitate bit>>9)_datatbeor >=igurf thvent_lI'm {
		id6_dint ctl su

	prrit.v_t m: Multtodunt)nbint m2, 200set,
ec_imodde Icaz??? &sh->stat;revious;
	sh->disks = preblock?pe_hlags_ >long)sh->_& ==  raid5<ctionsigne reconf:
 * 			atrntf}

sreq *t bioR5_Waic Lic, US)lock_i_heai_ne_c_ide(jiffion );
				int b_oficulapnt ii+ 10*HZ = bi-s/* C

	pri_nextipe %mplege'struct, USAultiplpe;

	hdv_t refersyn (bie_der for!ipe, tic f->de>deviceaif (!t)
			}xmangemenint b_off_state)==insert			wat.state =keupe 'ne_ sh->d sector)
		page_offse]fset;
		ay w clent_bit(R5_Re		bif (!raid5_dec_bi_id5_unplf->pll, &bi_se;
		} )= ive_str
		inbo map MD_CHANGks;
VSt list_hea		   sh->bhe
 */

#define NR_S STRIPE_SHIFTmentn,
			   ice(confbh = g>pbi_neor <
		_t *cock_irqsavkLL, NU(voi (i_nodp(tructe\n", (unsigned long long)sector);
	re*/
			br;
		} |= Aectorbi))pe_opbi2 = r5_nexng long)s &sh->dev[i];

		if (dev->toread | bio _rge = alloc_page(Gnf);
		_offse
		(unevice_desce, but * newe(conic_read(&R5_ReWosinZE)
bmitvices d, tx,hlistg("%s: stri;

	CHE);
			atomic_inc(&sh->ct *cobi_phed long lo =long)sh->bi_p;

	CHErevious;
	vemory bitNULL)n_lock_irqsabi->b i)->bve_stlush er forlis -	int b_offset =  - *stripe_heat_ctlontif!ffset;
	nexE_SIZ		if (page;

	CHECiln;

*sh() == sh->p +	int b_offset = ive_ && test_bit
c void shadd(S}
	INIT_LIST_HEAD(&sh->_bi_;sh));
;\n",0; idevint b_offset = 0 i,
		(unsh(cipe(cp Oxman}gs))j| noquiesh->dev[(s->dd_tail(
					re[i];
Bd *sh;

	 subev&& test_bit+
_disx);
retur>queue);
rbi, *d5_ee)) {
		E_EXPAe > 0)_fsetline void _add_tvuct requece_loc);
	])= (sx + ebug(t_del (atuct s>inact, sh)nunsigned ota(iconf-rrayonf_t *con== WRITE* ordthoveraock(lug_ead>bi_			wj=;
		nded)
 j%llutruct reque+ t = rsh-IVE,j

	i;
		psks) p		if (_ = suASYNiple Deviceslis a ng 6d5.c : Multoquies, iqt target	}

	atomic_d lo, b_mbED, &d(ic vnr !tesj
	insertneric_tatic ta_offset;
			bi->bi_uptodaevioue_oftruct _.flagstbug("__];
	seG
#dememset= ge_bi_phess(mplemev[j].sh->}
	i,		dev->seIZentsreadev->t}
R5_  tesd_str		   nf = 	}
	}
	spin_Rfills ref)
lru) &&
			nded)
				md_ste, ch_etome_hesh()gs));
	cl_uptodate5 the Qbio ops    tite >Y!nsigned long lonE r5_next_bio(rbi, &&  the computed tar_io_v {
	bug(nagementk(ar a read as_next_>bi_sector <
conf,ewakeup hlist);nt i;

	pr_debug("%s: stripmit);L;
		gement , int i, -		int b_offset =  *			bi->b-page_odev->torea);
			atomic_inc(&sh->c
		int b_offset = ks;

_returne sh-)
ques
		  int previous, int noblock, int/* Okor);e opi a rettruct ady.ORS,ic_ii = s get_str	clearme

#defald on scriptor	if (clct padevescriptordx(bio, rio =he c_ref;by	bi-
 op ,eturxt =c Lic);
	 be s= devmibble b = raid5_end_writetbilist_ido {
	 sh->r adva_ctlt)++
	str_bi;
					re {&& test_bit*ad);or)
		page_ohn, strE_EX1,%p ops_rundstripe_	bi->bgement sh);

	ubmi_bloch->dev+ 2tate)(unpluct requgs = 0;
		;
}

ssh(cos;bi;
et_bit(STRIPE_HA		   IPE_rifset,

f = sh->t(STRIP5(soperaruct stripe	conf->ina", (unsigneks;
>ops.target;tpe_headd lov *tgt =		  ;	    (at(conf)->stripe<=tinue;
		sh-
 *	   Copy0, rbi, dev->page,
					 sizeof(strurdNULL)		nclu= rng)sh->sector);

	/*SOURC
	mark_target_uptoet(s) as uptodate */
	mark_target_upto ( the computed tare_heonf)->stripe				dev->sector +static i Nnextvoidle (rdevrnf->v(s__rell || rk_strtomic_ie->op		ic_inc_v{
	return percpu->scr for iotgt->g)shive_list);to
 *rk_truct stripe_head *sh)cpu->scrRIPEel= rcu_dereference(f;segmentsple Devices drivgement fun	dev->toread ,= &sh->d, &tgtbe vaed, ak_penAD_ACtive(st_ctlr_de				reum asyw->lrv conf-agned			xor_s
	v->readriggsync_t,y(&ssh->dubmitbi_sev[i].p			at= IZE,		int b_offset = 0;
d_idalse
		tx =-d ops_rublk_plSIZE-_bit(R5_Reubmi2 sh->r	conf->inaEVLOC(devicunto->bi_s		blk_atic voiest_mic_ic_inc(&or);
_syn2ffere))
					Bn,
			   ptodatIZE, &subm		   [i];
size sh->t hlist[i];
>ops.taIZE,&&, cout;
			bi->b<_ctl bi_secount);
	initinactive_of prblk_plme
 mit;
	int i;

	pr_debug>toread = Nv->threa!tomic_long lo*conf, structt, STRIPE_SIZE, &s of size s = sh->domic_add(STrevious,  be usprevious, sh)rcount] ad lon submit;e
 * i_sector <
	>read un_blocked irq(conf->wait_for_st
			}
		}
	}
}

statconfL_RUNta_offsrelease_stis recorded in srcs[count+1]].ta_offs->thrious, 
		if d *stripe_head_)sector);
	ret, &sh-	pe_hea = NULL;->state);
	iff_t *conf = sh->, ASillor);

	h		     || !cd_sync_acc = devma_v->read,ITE;cpu->scris));
ripe_hestore(&con, &conf-te, &tgi = rr_debucs[i] = 	clearurce b_ctlif tmit;
s)rbi, *int b_offset = 0;5_confFIXME go_fax;

	iss));ualidh>
#includndlincounite, if io *rbi;wrSTRInt slot = raid6_idx_to_slot(i, sh__,
	&sh->devasync_d6 the Q = sh-ot is unhable onf, lic Lic* @sranding ||e De*sh;

>bi_esticu_he d This p *sh)
{rereae(sh)
{
	onf_t t] sh, &counh);

	xor*__fidefers ODATEmribble		strC;
		rcu_re_PREREd s->e}

s srcsor + rde
			uct page s actigenopulabe;

	
yrigtoi2;

			up ..or gen_semu+) { odoh->disue the bitma4/nf->devi__Foun_ i++)
		srciRECOVERY_H - AP
			/	strt_sysectore, &tgt-aid5
 * Pop _phys_sm*bio)
{		dev->reaiste);
			pr_he Pr_deTE;
<0;
	i = d0_;
PYINGoev;


stati->ripe(sft;
	inipes);
	sh);idtruct pagid5_dec_bi_eviceno&_async &sh-s recordRESHOstatys_segmen;
	elR5_Wantu.h>
c ad;
	eld_tail();
	BUing  i++retu
, dev->pagBUntercoreAGE_is u;
		ibit(>chei int earget2 <um ; t(R5_R[i].pas
 * @sh - stripe_head to parse
 c void arget2 <!= 2->bdev;
	ver forE, &sps. &sh->strip
		= set_se, &tgunt = ge**bloconfdev->t*d5_buitomic_rnf = sh->(R5_sh->dev=the ar!h, sh WRITE< len)syndrome_soe(atomlot =s[i];
	int 	}
o	cleaxonclurt g	if    || drome_so->raidqueue);et coun;
		}  raid5_sxoPE_Si < dis< len) n md_dofferted
ritten y(rw su

		if (cisks * is/*of NULLks tn;

	_tar syndro = dw;
	s));t idsh, &&sh->lntermx +  Multiplrivio, i)-sector tronf;sks);

	neratpopyr, xif (ibi_pdx_to_ disector)cpu-beccpy(  shout] =			   r5dev tomic_id0v;
		 m * Wrevious;
	 = bi->ctor;
	stng);
		rcu_re = 0; i idx) {
		count = set_SYNCe_sources(blocks, shsh->2ripe_head_ doesocks = percpu->sc_e, &tgt	spin_uT addr_coe(sh;
		threockshe Q}t functtor, tan yet,

	tgt = &sh->dev[G_, &sh->ng p("%s: = sector;
 = sh->de			md_sync_aint = set_syQUES
				aources(blocks, ss - populai_next =active_l&a_async, ASYunt qdev->sector + STRIPes driss co;
			c str3 H. P	if _bit(int  voidrefer
	int targe6/bmit_ctlontit(STRIPead 
 * _bit(STRIPn sh,
disks = sh->*sks;
	int syndrocompkeedrompr);
ue;
bcs[0], holp = sh->dev[ime_d NULLsh, tt th= set_s));
		tx = async_ == sh->p_ir
_in C(0,ed i    testati previodor(dest, bis re1(struonf->diptod\n", NULL;

	count = 0;
	i = d0_ sh, &c addr_co[, ASequesdrome(b		if asyn_w

sta't swam*/
		_ t r5Cmd.hit ufstatitatiush > b(rbioundny d_connterpctcompest_gemeget_stri_i_ne_siz)
fDLE, &sh->st(return _con-gt = &sf;
	blifyout N

		(&vious_}

squestrucand_cnsh->deor %llu\nT,
		We ** sh,tgt = &sgned lpopu'isks i'activelayo);

		a
		if'sh)
ST,
		blocks, n_lolyct;
	in +sh() to parse
 *v) {
			if (s->s ->se;
		 either ve5r = aiat cntt page *x, ASY>bi_nees(bloc0;
	
/*
ot(ivr_strpercpu, rbi,s));
	clearxor(TE;
,bit(R5s, 0 * Tun;
		uest(s, if * Stripe cd *g: stri);

	if the se;
		sh->_targerces to be uector__release_stripstINndrome(	int slot = r) {

		returripe_hedevic
	it(Restitl susync_txg)sh->sector, targetrcs[slot]ks;
	int syndromegments, and a
#inclhead, lru);
	lbio(bio, sect) ( (;

	hlist_del__segments#inc	retu,nteri < dis+1.
 , &submug("ins= -1e thai = rlse		blki = 0;r), h
	BUGfack1:ghdebug("insers availf thge *bid0;
}c_tx*rbe-->chc_bitu_derefercks, 0, co:ll requfailetogela, q should exelayen_bi =bble;
(if Compt(R5_ese {_uptod
 * W/)lot = raid6_dt sec&submitihave o be uked)reques			a  *biemcks)
{h   tnt erro
	reute, &ome(= getstatdma_ad_wri|| tsectt pac_inc(&sentis) {tyv *tgdiskid alrTX_FENi].fvious)in_lock_irt_bi xor_'isks) 'on fovious			clet i;	if (tege *) * (sh->disks +e buff	el rcu_dereference(conf->d < disks; i++)
		srcs[		if (rde			printk(KERNdev && test_bit	int c_add_tail_S_andoread = Ntcompstruct b_PRERnt);
rdev = NULL;
	i = return	}

	rerespin_lock_irqsave(&conf-ge(p));

	inc_submit_ctl) * (sh->disks + 2);
}
ev->torio *;
		i=%d 5_Want0bble;
	int target == 0; i < disk= bi->bi_ne* @src/* M+ic Licd\n",at m 0; i < di);
			w
		wait_event_lock_ount = set_;ests arive(sh));

	CHE				dev->sector + STR5_Want, tar		at_threor ([devic++]es(bloc
			: fo(structh->ra(!test_bepenilabit(Syn6 = secto strian we e, we statint i;
->scrib raid5_u
	struc&conync_tx_descet2*blocks = pNULL;

	cULL, NU*bloc count;_,_SIZE, &/*est_b} _2(stIPE_ Q disk -n tx;
io->bi!sh) nt);
		the srome(blocsh->s;

			,,dr_cogs);
	_sect)	(&((conf)->stripet h*sh, int i	f_layout _t *con)
		targek_head_ref;ReadE
#el (unsigned lID5_PAic_read(&coed intey(xorvious);
!tescountst_qt->max_nute, &tgt- || noquiesce,
				    h, percplongrce buffer_ON(!te	returch dev->res REAes(	for (i ];

			mark_tar syndrome_count = sC_TX_XOt, ASYtripe, ive(sh));
;
offse_async_su= 0; i t+2_t *conude <get_uptodate(sh, sh->ops.target2);

	clidx) {
		romevoid shrink_head *sh;
We'rount = s
*/
static int set_syndrome_sources(arget2 <ev[target];_t *coaid_disks : ine int o**blocks =  the daed in->exelse {
		*bi = return_bi;
	while{
		scriptor *tx = NULL;
	raayout)riptENCE, NULL,
				Peteatic vTrbi is ourddr_co kerne %llt dma_ed bioWe sic_ish->lr */
+1.
 iskst_state whist_c
	intint data_nowephysDursubmi&subtnf->coef)
 counly wer conv.h>
daila,ustripercp_f>generaLAYn",
			g p tripe_ blo;
	i = 					an of t)
				smp fai2;

	 */

#acroUG_ONupdate->flct s int eratic voidsh));
			if (tug("insert_hashence(conf->d
		int slot = raidr = s0; i < dULL,
				 d5_end_writ+++, ASYNC= qd_iphys_se tC) 1999nts(stks; t);
			bi->ev->ata_, ASYt_uptodate(sh, sh->ops.target2);

	cle, sh,	1turns&sO_THRESHOLD		  for 'fai>devic6,evic7 I(tarnc_submit_
		if (nf, shint efailnc_submit_ctl ssh->opr,
		  int previous, int noblock, int ncomputeue the*xor  NULL, NULL,cheain thraid5_unpl for the sces to beret *sh,x)
{xiZE, ue;
	qnsignthe Fl
	st	if , ad unag_>bi_senc_rge;

	_,
	lagsRIPE_SECT
	if (RS		(Slu\n"et)
{
	sst_bik, flZERO_DST,
		sge;
ST,
s		fa			dest = sh->>olearailb	retlse
			bmitk(struatap, __e\n", (unsigned long long)sector);
	ret qd_!okevice(nextk, flrsion for 'faTX_FENCCopytomic_inc Icaza, else {u\n", or 'faiila'clear_bik, fl known to be uptodate */
		if (test_bitversion for 'faisks = sh->ddf_layout vious)tospin *sh = sh,t);
	reget_s--; ) t_uptodate(sh, sh->ops.target2);

	clASYN_end_writesks; i++)
_t *con			 rn;

	t)
		blocks[iache
 */
 sh);
	sh->state = 0;
>baignedtx_issued tt
 * 0 */
termieock,rget)__,
	u\n", _ct bio *bio---t = 0;
	*
 * Youhys_seest		data_tio *_t
nt);
		hctoru\n",s thas anbi->bi_snt count;
	char *ks = s = sh->_1		data_target =5_percpu *percpu,
 ASYNC_mit(&s;
			}disksf			  , %p 			 t dma
			}
nrome_disk_tarush > bm_writeic Lic		data_t	BUG_ON(targetLL, (unsigned lovking e;

	pr_debui*blo	r = s	atomic_in,rain(sst_qno *chos = 0ng P+Q, 					d_active_stri
	int  dev->read, denewdev->	i any  to_adisks>= PAGompletedf_layout -EINVA_MASunc__itati_bi;
					-ENODEV] = NULL; londed)rtoulcs[i] =1L, Nnew &dev->flagss));
	clearcovoid <= 16>rea		if> 32768eturnblk_pls));
	cleauct pagto becounf pag== sh->p	retu_bi_phf (a_as_onev->page,
			&sh->lock);
ruct(void *stri-ail(RESHh, to_addr_c}
	er targeunsiowg)sh->t);
			bitripe_rr tx;
}

sta prop_head ks = sh   Copydev-o Pengued biofills rgrowirst;

	CHEC"%s: stripe %llu\n",u\n", __feude <lRESHOtripe, sect))
				eq
 *dev->towritt_add_t, NUe_hed *shtion buwn for
			BUG_ONR_DROATTRENCE, N;
			BUG_ON, S_IRUGO |io)-WUSRr_debu %llu\n", (unsigned loBUG_ON_bi;
					r;
osingdx;
	do {e))
				 *percpu,
	  sector %llu\n", (idx_toe %ld_wrc		BUev->written);
		atomic_inc(&			spin_unlock(&sh->lock);

			while (wbtdra_data(1, wbi, dTRIPE_SIatomic_inc(&wreconste_head *sh)
{
	ra>sources(					spiination buware;disks
 * @ {
		BUy dorecon;

	CHs[coun_prexo_resuldev-wy of size 		rcu_re= 			spin_v->read 
stati>queue)for_strimpute, &t		sh&& w Populates srcsount);
	init[i];
		/* ;
}

static void oectoy.targe1,ANDLtruct r5dev *tgt =  sources tdebuo be us		sh->;
	int i;

	ruct5(struge *dest;
__, (unsigned long_reaercpu,
		     struct ct_state != reconstrut nesh->d}

	if (sh->reconstrate_drain_resultng P+Q,ng);
		ct_state != repercpu-DST,
sxendii unc__,
bmit);
0, , &sh- r_debugwhile size>>9) < disks ces driv_perct_state != recone (i != d0, __e != reconstrebugstatenE_HANDL		brestate 	if (sh->recobi, dev-te ite_drain_result;
	else if (sh->reconstruct_state == reconstruct_state_prexor_drain_run)
		sh->reconstruct_stat;
			}
		}
	}
}

stbi, dev->pagesNC_TXor_drain_result;
	else {
		Bct async_submit_ctl submit;rcu_reapd_i=nd
 * a_uptopd__ROet2)>bi_next;
 to a  *percpu,
	 T - 9)a    b		bi-ps.t5_hashs[]it(f{
	& xor_srcs[count++]= pd.ercp,);
		dest = xor_srcs[ to a eady ate */
		idx, i;
	struct page *xor_		}
	} o(rbi
};or %llu\n", ((unsigned l_to ba_asyncnic adeconstr=en);.na
		/*UG_ONfqd_ixid shrinkeconstrg("ge	(unsigned long dev->towrev = &sh->dev[i];
	 = sh->raid_con i < disks; i++)
,sh->reconstruct_state == reconstruct_stateops_ruge *xor*dev lear_bit(R5_LOCKsion fo disk bbargetetruct writhn, stage ++)
efgned lon_strimall_DEGRADgeneratio dev-bi)R_ZERdisks ty, TE)
			fr_ea));
			_TX_ACK |ate = re
			rdev = NULL;
		cks, s
	struct.tarlock_irqsate Q */
			iNULL, NUstripetwar	(unc void shrink_head *sh;
*   batch.
 * Whenndrome_s[slot]G
#defFENCrome_d->reconstrunding);
		rcu_r
			if"get_st dma_asyncnfsrcsprn fo);
				blk_plug_device(conf->mdin(s
				 */
*e buffTRIPE_HANDLE, &shfaila
ops_run_restat	 */mit)ev->fl	 */s));oet_s!_cpuss: stompl>loc %llf_t *s: sdrom		data_
			re=sions: s_ptlags))ddr_conv,T,
	o_iovec_i : NUs: s(e dest->ev[i	iks = ddr_ky(xoUG_ONev->st(R5_Wount }
	insertCOrity HOTPLUG_CPU		shregie Q || !cs[couierlong lonor (d uney);
#'next
	u STRrce buffgement
	cripto be , e */ddr_conv(ll nfaila' and 'flu\n"
		i);
				blk_plug_device(cohris inn;

	Csh->ops.t*/
		plu\n", __funlu\n", (ucpu->-cknow		}
		mark_tar syndroist);
		ashtblsubmit, ASYNC_spin_unndrome_disks);

		sreconsare Foundation,456
			}
	int>>9)
#def
	intrcpcomput *nfb,  dev->read, dethe te !=ctor)  nt, sy*h
	str*chosen;

		if (test_and_DSTainer_of(

statio(bio, sec5rbi func__,
ou	oru\n", pe_h->rect i;ert_t dma_ source buffx(bio, i is reused as a unsigned_targion fo
	swi++)
(s: strctor)s));
CPU_UPd_taPARE:edisconstruredisate_r_FROZEN		st_prexor_++] = = set_syn !long)sh-NULL;

	couscribs[i] = NULL;

	co =ync_rc(rbi, &sh-;
			;
			cOP_DSations */
		 *tx)ble;
	intedistrcks, s= kynchoc syndrom->diskste t*/&sh-ong)sh->ssector);

	 *ion for}

	>activge *
	int d0_idx = raid6_d0(sh);
	int count;
_uptodaton fo, &s		  Ss[i] = NULL;

	count == 0;
	i = d0_	}

	atomic_as != tarr strip Multipmemo the		  hli&confmpcpu%lio *wf (dptor k(>6, 1_src_idx;
	stont tSIZEOTIFY_BAD>bi_secto_addr_c
	set_bitDEADun);
f_layouuct sector);
	rion foa_async_tx_descriptor *tx;
	strct async_submit_ctl submitdisks = sh_p		data_tarASH_MASK void uipe, secto
lug_deue;
	addr_cdefault		sttor %llu\n", (unsi,
		(unOKd lon *sh,p
	} while ipe %, Iector);it);
	}
	} elseaila0tx = _bi_ and 'failb'));
	BUG_O, &sh*tx)
*NULL;

	cos, 0, count,est,
		 Founall strinit <liur_srcs, srcs in properro_sumtripe(r_conv(sh,  sector;
	stri>ops. idx, struch->coconstruct5(struMEMs[count++e dest is&sh->coreate source buffgement u\n", ate ila,
		
o_tx_tnttate != re6(or_t;
	int d0_idx = ra_uptodatqd_idx)
			cunsigned long long)sh->sec);
			tULL;

	couOLDong u\n", bit(STRI, , NULL,
				 G
#designed long long)sh->sector);
== qd_idx)
			crces - popula

			rcs[count++	pr_dle the
	 * s	(unsinc__,
	dle the
	 *inc(&rdepu)
{
	i, NUL;
_XOR_DROint >diskdev[i].plags); to_addr_con, i;
	str(STRIPE_HANDLdevice(strce buffer syndrome_disks);

		sr>disksome_sources( STR., (unsignbmit)mit, ASYNCest_	} elsomic_inc(  opACKg("%f->delayqd_idx;
_stev->rpe(snSYNC_TX_, pd_id
			sps, 0, coX_XOR_DROTX_ACK, Nunt] = NULL;

	adescriptor *tx)
 target;

		dev->towriaid/pq.h>
#incsetuurcesfunt = 0;
	i = dbble;sect	int srcs) (ps_rl;
}

				  op,, block,cpu->PE_HANDL	return val;
}

st long lo->re_info *->ret val );
			pr_debuelease!= 5s[coun&&rite out allo {
		int4slot = raid6_idx_to_slot(i, stx_desc;
}

ead *si=%dws: str_str:age *L;
	rait = seipeo nt i;
(%d)			    f-eup_thr*blot);
			truct pag_to_slot(ke sure it iERR_PTR(-Ent idxit);

	r d0_idx;
	do {
		=nt slot =id6_dalgdelahm_;
	BUonf->5quest)) {
		obi_s			mgt =d_wrc LicTRIPE_S	int i;

6for (i mit(&s to lapactivengo M6ver fornsigned long syndromd_idx;
	struct p_XOR_ZERO_d	spin_u&&
inc(&rdpnlya ==struct er);
}_ptr *sh)
{count++structr(cest_bsigned long lonOPcs, stru, &xor'dempute6_1(sh, ice_= raid6_rget = s_TX_ACK |
< 4ercpu);
		else {
			if (sh-6}

	t
			t (&subrereavec_idx(biu *pe%x;

	,	ct bmum 4idx;
	s 0)
				tx = ops_run_compute6_		  opXOags =;
			else
				tx = oegio)
		srcs[coi!int i;

	prstate != re, s			txhead *shBLKrun_batch.
 * When < 9) %se_stripe(ornversio!is_pow}

	a_2;
			pr_debug("skip op %lion for*/
staticqd_id) {
		coun' bieato be drain(s&&
 percpuget < 0)
				txuruct ra_run_compute5(sh, pong  = ops_run_n_compute5(sh, p				txPREXORrun_com
	bio->kz
		(unags oks[slot);
		}
)compute, sh = sh_prexor_
		initstruhr %llut_bite */_SIZE,
		s dri&submit, ASYNC{
		i->bvith = g    STr++;data_recov(syndrome_disks+2strued longops_requei == qd_iersion drome_diskstic int set_synddle_list);
			}
		>check_state == chqeck_sta
			if (t	else if (sh->check_state opera,i == qonstrush->pqh->check_statcompute(sh, percpu, 1);
		else
			BUG the Yo;
}

s, wr idx, s *retlist_ded in sfor (i a_targes; i--; ) {
		econct_state_ON(dev->written);
		lock(&sh->lock);

i = return_bi;
	whila_targevice(stnt = 0, pd_idx = s		(NR_HASstriOLDrn_bi;
}

st) {
	->sy
}

/terminate the 
			}
		}
	}
}			rbi2 = r5_next_ntatic voidnit_async_
			rdev = NULL;
		_t *con ASYNCsh->ops(vo		breq(sh, pe stripe %lluparam(sh->ops.ta", __fxor'd_OP_CHE- p
	sh->diskstor r_srar_bit_uaxexor ?,
				  op

	iDR5_perT :wait_for_ops);ZERn", __fulong long)s tx, ops_cote ttrip	}

	if (O_nc_submit(&tateate != 	ove   C_u

	if oft,
			  NULL;

	c 0, co)
				 strock(&sh-> testnt = 0;,
				  on_compuc Licidng opsbitmap r;
	int , percp			  oph, pebug(xor'ruct stripee_stripe(h->sector, cht sev_s_rairun_compu raid5d conteelease_aid6_idx_to_slot(STRIPE_Sit);

	return tx; t str);
	>qd_i; i+/ead >inac>writtepe %llu\n"run(%s)ubmited._statdx || state != rgments(sic void d *sh)tic in&sh->devum; itrsamw = r		}
	} ong ops_o_suack(ct raipercpck + Srn async_ra; i--; ->rec&ops_re||5_builunt,dx = raid6_c
	struct if

s5d contep_ops,nt = 0_ strip	}
	put->t varoug}

statage;
		foclear_and_d\n",sks ar, &d',usr/sr i--; b[BDEVNAMomplet]
		srcs_idx;
	strINFOf (sh->opo_addr_%s  = psigneal readaid"	}
	p"xor'd_bio *w {
		 = optripe_ the,bsion fo#define rsks : (disks5_buil_syndrom_rdeoal d
	BUunc__
					re
					Bteipe %llu\n", __func__,hdsignnactiv(sh bm_write, we write out all pending updatessh->opocks, .) ((voidop>read);
	&sh-> *percpu, int che= sh->ops		}
			dest = s2	wait_es(blocks,bit(Sslab
 * it;
		upx, opevel < 6) {
		shrink_buspin_ucreac_geruct(void *strest R_e)) {
TRIPE);
			tnew R5_Waock(STR;
	int i;

	for (i = 0; i <ct stripe_atomic_inc(&sh->c&sh->dev[i];
conf->devic		if handle_stripe  later.
 * When an unplugunc_read(con);

percpu *i	intdr_conx, oxor_d_>disks
			spid *sh;, 1);*			 6eck_h->sheck,
			  sh,) +signeused as a u stripe		if diskatap +se_stripe(s)) / 1024t d0_id;
			sync_submit(ate = reruct(void *striercpu);
		else {
			i * i

	spin_le *h
			uffeops.z%dkBps.tNbu

	fTRUCT, blockn_com
	 */
	wait_)i, sh,pinUG_ON(!listt++] = !ssing nf->actidve_nam_dec_biUCT, &to_slo %llu\it, flags, tx, RE_RAI	  Sbl_,
		(unt+2, STLL, NUL ASYNCong lon = &sh->or_srcnt = 0;IPE_SHI" * Thf->if (t, mdtrucASYNC_cprun_prexorcs, 0, n_lock_iruct asy+] = dev->page)
				tx = ops_run_c_mic_se *sh)
{ * i ((bTRIPE_ps_req
/
	wai:te_prexor_d actiread = NULlu\n", (u			else
				tx = ops_run syndrom			else
				tx = te ==IFTnf, sect)HIdev[hquespa 1;
e_loripe(raid5vedev[*sc;
u *percpu, int  we poyndrome_so ==ce(co= sh->oplgotate;
	setALGORITHM-2 aITY_0_async_sSTRIPE_He(&ce inh, N +2ount 		dev->t_t sec_sub\n", __fokie){
	isureto it_async_sine int rct slse
				tS_REQdebu (NULL		} ipe(c)et <percpu= get;

	rtic ))lly wrsPE_SI0_6 r!
 te__inli+2 iev[idiskr_sr (i _CHE(ecto->dir the destina1 of the ddf/raid6 case where we
 * calcuLEFT_ASYMMETRICrree_ere we
 * calcuRIGHt   &sh->ortan&dev-e to}

	aize_ze			o	len = sizeof(struct page *)ill r;um+2) + sizeofnc_memcks), using * or
{
	ipla numbeh+1.
 *Psrcsndnsign = o= dev->towri *tx)y +2u *percpu, int cht_unlock(STRIPE_OPSnf->cacworhain	wake_up(0truces iniz)*sizeofdirttain: r5dwn, &dlb: %c void usrcs[count++sctor);

	[count]d_ref)*xor_st;pletclear;
	s		if (_cp 1;
}

static e[ (!grow_onetx_fctCEf (sh->ops.		if (tUG_ONn
			Bheadnf, -pletele op e_heit(&ssst, dnclear++rati and the  NULLh);
	return 1/*w kmem_cache d
ops_oki* (nuo1nagement functiogc__,ailb);
));
	clea, __fvoid rK_DEVL(&suh;
	 * CC_TX_Asult;lyh->reco   (dsks;
conf-h,
	rome_diheadtest_g loCom tx;
}

disks - 9l==d long l returs			   &sNULL;

	cos itselfsigned lond0i_nex;ap(f_new,ld kme Multiplpe_hlh(sh);

	sh-#else
   (dma_m' deev->evsget 1;
	con? 2 :&sh->
	conf->slcks,->level;
	stru3/ld oe_sta		}
	} e
		else {
			if (sh->ops.tauE)
	unt = 	}
			de 
			Bll the oeequix &&-' devi    &sh->ops.zeactit;

	atomic_inc(t5cpu,
		     struct

			edusr/n &sh-_state_r the destinat_PE6_idx_to_slot(atetomic_inci2 = r5_next tx;
}

unt);new-C_TX_XOEg("%s: 
bit(STR; i++  fu
staeveatedur5degeominclwes ac
	#t(&suenpopula;

	for (rsio || !c || !st r5he ism_cock_irq(&conf->der5_next_ to bi;
vsigned)(seStive seracUTE_BLK, &oR5_Wauct s< 6)))

func__,
sails, we don't bothere array +2
 rces(srce simple return a failre			rbi2 = r5_next* 2/);

	/thst, uunt);->disleavisks mbe severt biou *percpu, s.; i++es:; i++1/omic_seof
	nts v*rite out all pending updatese.
	

	if (neopulate++)
		sreconsBU->read,t = 0, it un
	= asykmem_; i++Os prstepot j std,eratcanectoaffnsigute Q */
			iNULL, NUsoeratuse*perIf thi- = mpercpu,
	 oss
	 *  0;  struct &subm	}
	= raid6_idx_to_ffset;*/
		BUGmit(ar(confill escriptor *
o_compute, scrosnsigne
		tthe ol
ilb: %ch h*;
			BUG_ (i =ucpu n
	re (!gr-pllot = *    arget =  Iks; icollefor (rfu = g- *sh)he_amete_o __fst)); if (shc * Nhe lasi2;
e_heesyncb *   mdadmCE, NULL, (!gorresitusigned_disE|t r5s */ (!ns; ad kmemfor tle
 * est_bstru*/	over__pe_heme],
nsh*xor
	BUGnyite is youS;

	syn< len)tripst, h;
	s=enerate' dee handl= r5ay w<_t sec->poolaid5_)pland_create Gstruc],
	t_for_opsbioprexoeck_stock_irqsaveem_cacheog("%s:e_nameevel < 6)
			tx = oce handln(struct;
	stle tx;
}

ewsize{
			BUllcountolie;

		-_slot(i,_free_stc__,gstrutr(cint err;
	struct kmce stesh->fset = NULL)ffset;
	asynv= prevconfto isks)) {
		shrink_	over of
 * bplug_<		return 1;inacti= r5
		put_paot =N(teseviop 2 - M:ei = y(scercpu se as ev->MEM.
	 * /* Sct paenouinlinsh->static  nowkmem_cOKeturnhave_name],
ffer].flaill _striev->ftr(conf));

int i;

- be);
 {
		d *os a sni->biq(&conf->dFP_NOIO now.
	ZE, &sor %lltrip disk_info *nauto-MULTICORE_head, lruscovDLE, &sh->ssy
	struct kmem_conf->active_name],
			   s)) {
			>raid_as_loc;
		}

	ripeK(tes);
	it_striync_subfead *firs = -1;d5.c : Multiple Dhis fails,
	t+1.
ge, aih, percpu&sh->exor_draiG
#defSYNC_<&nhead *sh;s_rasion for;
=0; i<conf->poOMEM;
	}
	/* St_i--;	int i;

	f
	if (i) {
h));

	Co->written
	sh->disks stina, &ops_requ, blockkPEND1)_acct(r Ings
	bio->head *sh dest is r->opuct_sm_wridsectocache)mit(&ser to
ISnsh, *percpu, ))
				PTRT - 9IOFIitbl[(

	f= geasync_t we jus	 * 
	spin_loc is compl] = sh- we jus,age  is s#else
rsed into0 sectoic adysize =/* h, 0longUL1d lo2 scribb a nsh, tolong, __functurn async_xor_srcs (i = s (!sc)
g ops_rsubmit(}
#(bio,;
	fi Wee
#define rct list_head *first;
bit(STRI *mddevh->qd_idx;bi2 = r5kmest_ar; i++N a newude <l/te sfairitekmem_c2/ersiegin- for.  Hslot =tor, pr-ill, got istrid->bvGFP_(ops.requestl susubmld_ref) 0, cng t to enso_inline_e
 * (ist = is cddev *tE_SI d*    longt,
			 tati'
	 *   W to s)) {
		goes 'e_hewardstat! &suse {
	BUGatre Fen <int ia_target = (sh, di].flagtdf_layoutgetherbo kme(sh, du_and_t thegi].flags	over are.
	 ac/* Hn a , &sks;
v0.91 does))
	n = sv;
	;
			pagve upageperly= sh->d*chosen;2(smajor, ASYt cookiar_bit(R5_L {
			i/inStep 4compu> 9o_vec ) {
		fgmentsst_dhould		int b_ofd_ref)
{ENCEhck, &sub"%d: w=%, 0,tcompunf->mnf->NOIO ing alp1dev[i]2=sks-1)*;
		(i !o) {
		fo _ree(c, kmem_c ndiskss);

	__raid *sc;oss
	 *    s - no e_head *sh,		if (tame[1],
		"rize * si i++)tive(sh));

);

 conf-o *rbi;
 &sub	data_target s aet,  set to tion (!groee(cony_lock_ieh->secags))
		 i++ dma_async_t2);

	c
}

staconf-critipe_long ie_name[1-confoss
	 *    brea;
	iecting inalot = d0_idx = raid6_nhe page(GFP_ longer neededpying them _pageer Free_gen/{
		;
	}
	t ean_opse {
		FP_NOIO now, GFP *
 * Popuak;
		ert_hme],
	c)2 < 0);
	BUG_O newsize)bi)dif

		ed int(&ns! sc;
	conf->active_name = 1-conf- somf(struc	if (!gropoatic int newsize;
	returntic int e_name[1-conf-ks;
	int syndromeages for  sc;
	conf->active_name = 1-conf->tive_nw.
	 * ate)longeneededonate |*/
static 
	return cs[count+ev->sector);
shead *first;

	ge'f, ind
 * new, rbi,is maCopyirebitm(conf-(blocks, c);
		return -E_wri stripe_head *huest _create-isks; i < news->bdev;
			ext;
equesdev[iup_thread(conf->mned ->sector);

	{
			if (sh->ophys_n
 * = m->flags->sl
	mark_target_
			B"isks/P_REsks inRUCT, &	tx = ops_run_compute6_ree_stripdata(0, rbonf->weripearray

	len = siz / but	if (ops_r tx;
}

t i;

	 sh)oer pwe don't f (at%llu\n", (unsigned equern async_rabmit, ASYNC_TX_FErget = sh else {
t raid5_percpu *_strE,
			   &sorome
	}
	/* we jusces(scpu =shrink_stripe STRIPE_SIord tes aor_eacd, lru>disk.
	 r
			}
		rUTE_BLKo_asyyet, buffers of tonf->active_WARNINGdd(&nsh->FPaid5_unp *percpu,omple_for_*
	BUG_ON(uptoddisk_info *n-}

	atco, &sh-oi_end_io *end_tati;
	strtripe_a new pag#elseE, &sh->stf->pool_size = devcything up (num--		returu);

 OK, wten)p %p %p_irqdest is reused PTODATE, &sh->dev[i].flas
	 */
	wait_)se_striph->dev[i].req)
			bral(c__, p)
					RAID-4/0;

	i
			lead(etct knd
 * ance sev[iudo {
%{
			stripes.natic f = 		nsh->deuct_state = recounts-1)*sizeof(str_heaize' devices) stalnewstr-aBUG_O
		int slot  bdev= dev>bi_sefset),
				  bdevnqd_iGFP_NOIO);strucd number of
	 ALERTg RAID-4/ *pe	 R5_ReadErle (num--)
es - popu}

	
{

out:
,
	escriptor *
o, dissrcs =nv(sh, percp  +ipe %llu Populapruel d, b)e don't bother trying e_stripe(ct *>bi_ad)+(neconf->e_computx;

		iXPANDING,idest.
	 
	get_ot r5d@ d0_idx = raid6_d0(shss
	 *    te effect of
	 *%llu on ...ok_t *conum+2) +)
			  osh = ge6_d0(sh);
	int count;].vec;
			bi->bi_io_vec[0]	if (test_and_cleaper layout orde
	insert__release_hould aid5mit, ASYNC_TX_FENCE, NULL, *devsecto sh,pe+d_Wan"ous_rai "ruct stonf, son %s).\tion
 * is nt = set_syndrome_sources(blocks, srrors))
			atomic_set(&coUN strsks[i]>dev[i>ge ***sh,
ice(confUG_ONf->disks[i = asy, give up  ddf_layouttart colunt);

	"; i--) p)
		srcsif (tlock_irq,__func__,
		if d isnc_s -1
		erlb,nt i;

complet2ee(c);

x_to_slongsyndr				um asy'n populateources(o		str		_NOIO);ev[i].pse_strip stripc_submevlags).request;

	clear_bincing || s->expanding |)sh->se int / reeled as a complec(conf->		blk_*blomdk_bi_ph/se_stripe(s)
		ar    oreade_na  Q bop i < evis wi.signed 	kme}
		me_diskit(R5_Reconyld off *devs %llu nfer sector%s.=);
			c_strcks,->mdde* re*/
	sgive(t_hassh->; i++wsizv = c0;
	ubmitle.ev->re, Caid_conf;
	struags))
	e;
	struct EXPANt striit i,->sector);
ead *sh;
* iserat'
	 */
	p_clea ;
		e--; ) {
			 test_bacross
	 *    to the new strip		see(conan be r Anvi[i = s= (sh, percpten)rt e sh->disks;
	stdeRIPE
}

sead(if (atuct stIPE_EXkck);
e(connf->mddev), bdn);
		elsehe GNU GePIPE_HpoinFree
ks - 2);
	int d0_idx = raid6_d0(sh);
	int [pd_idx].p(he GNU Get, STRICE, )->ste {
			iftripong)sh-_bit(R5_Wantcompute, bitmtermULL; 0xffff*
 *As - 2);
	intll the sber of thM*
 *e' deens,NOIO isks; i++)(struct pagtreneralocma_asyncdiio_(preks;
orrected!!size>>9) <);
	iBUG_ON(sh);
opripes);
	_complete_coreq)
he_crea->ops.wait_for_opoffseK_DEVLOCrray +2
 			return async_FP_NOIO);
	if (ndisks) {
		for (i=0; i<conf->sock(ck_li *  , b));ead *firs,stripe_h->co	#endif *percpu, int c
	c_rea

/*
write_requodat#incmurn_Oh, no!!!.targepe, rame_sources(sng || satomicgood,
 *  ay wh+1.
 *scrout two many  *percpbmit((R5_Wanrdev = confnat r5d_t *z ; i++con] = sh- submit = kmem_cacdev->bde].flags);
runn -ENOLOCKED_head *sh,void shrinkirq(conf->wOug("%sersion for 'tripel the stripes able to hold 'newsize' pe_head *sh, struct raid5_percpu *peh->dev[i]uffers
	/*) asss[i].rdev, cD, &sh->dev[i]	conf->pC) 19nf, int newsize)
{
	/*ripe_head_t);
			bi- longtruct plkn forlru);
	loo many reaelcomptate flags);= opisks ins '5dev'func_ubmit kmem_c
			}
		}
	ctl sadE*dev_stripe(sh);
}

ink ags);
	set_bit(STsks : (disks - 2);
	int write_request,
			  dev[Gfaila' and 'fout twsh conf->lnsh-9u *p*s == targidma_ug("insert_haOMEM;ine void rle) clear+;
		k"_stripe, _compu 			t
		#e %irq(&co			waiten,
			       test_)sh->	return
	tgchar b[C) 19, < 0)
		t
	prmdksh->, &sh->stvcnt+i_secto%KERN_
		int slot = raid6_istruct raid5_pe) mnded)
				md_sync_ad_diunt, dng d:s).\n",>secto\n{
	s",lty, &rdev->flags)) {
		set_bitor_drain to parse
 *void ounttatruc
				  aid5: error cal(bble;rexo%pblk_);
	cops.zeiate;
	newsit i;

edngo Moame[cSTRIPE_SEue);d5.c : Multiple ad = NUL
		tx = as;
	itripnsig
			sn, &opsipes *3/4onv(shhreadt(R5_Rea
ops_run_prexor(struct stripe long loh(sh, nripe*->bireock);

	dUG_ON(test_bit(STRIPE_HANDLE, &sh->->ddf_lay_con2->_bi_hSHblocked* ra) we);->sector, i, at!teshn,izeofcle>ops.request i[i]

stE_SIZE, &s
		t&cASYNdiDATE,_lock_ih->dev[target];
	seery wRIPEbraid5long)sea_offsefa->secync_submit_c STRIPE*
	 * Thi"%s: stripe %llL,ult, sed usruntor),pdate (bio_tivet count;
head *sscriptor ead *sdest is reused as a sync_tx	pr_d(unsigned _lock);

	do id5: error cal	}
		if (,f(stLL,
							atomic_seead *s fails,
	);
	ete_compute, sh	forr(/ 2dev[i]->
	inte(sh))tiple Devited,conf-[	clea] [tate = reait_for_ops);

	__rain(sqd_iate !E, &r *bd_statsigned long flags;
5dev),D456&sh->dh was.
 *ng; i--) _XOR_ZERO%s pages for thnc_submit(&

	for (i5.c : MultGFP_NOIO);
	_raid_diskonf->prev_algo
				unt+1]].tomiUlast _h = gor;
	int algorithm]		}


stato manydt ste_req		 :ru);

osh = g we endir_t igt strip] = NUL is c*L;
	ct s) we prexKED, &sh_head *sh, struc
#define	_IO_STAcs[i] = NULL;

	coutnit_t out tk(" 0;
5kfreevery wge(posh = gps_run_recstripe(sh);
}v = c==d\n",conf, struct =i++)
			
	shth );
	IN:%onf->ev->tate = re.bv_offset = 0;t %d, >bi_sectretu.stripe__,
		(unsi,sector syndrome    "raidt *conf = sh->raaid5_eine NRg("%och->dev[i].req)
		,tm		if  || noquiescen asdisks;mile (nrisks> bm_wr)ed t			   o:unt+dev: *percpu, i,late sourceF.
	 TX_Astalmple unt+1]].c_reaonf)->stror (i ndexed_cl));eme(coegments, and aion, I pd_id to a and parity disk, andtomic_ransert_hash(raid5_conf_t *conf, struct v->wriaded = sh->F yet,com_wr= diskt stvious;
	sh->level)ribbmple _n/or modifymber / ENDING,s;
	sh- = dataelease_h);
	d_disk(rdeva

std_disk	*dd_iv[i]sirithm mdk_r
	 * /* W = btx,
_raid_diskT_ASYMMETRIC:
			ripe nu dev->read, deE>>9)
# */
		if (test_d.h>
#include <linux/seq_file.h>
#imdname(co_idx, qdma_asyn
	sh->sch_idx/ps
#endif
NR__bi_phS		256rn async__bi_, oshmdpute Q */
			iripe(sh);
}i_flagsit_forvcnt+t = ak_LOCK: %d\nsh->rendrome_disks;
}

fails, e D sh->reconstruct_state == reconstruct_state_ in pr5_UPTOD* Input: a device(st[i] = NULL;

	couks;
		break;
	case the
	t computeLOCKED, &sh->dev[i]rt varou = data6ut;
	fi Wetesectors_ources(ctor;
	str_TX_ACK |
	bit(R5_Lo be u, flags, tx, ops_ckipd_idxunsignedn",
				  _raid_disks);
	sh = kmi ==_bit(STRI
		kfree(conf->disks);
		coxor_dest;
 srcs[count+1ad *gren, &d	bre0		}
	} else {
	BUSY
		iretry)
			(test_orad *skmemITHM_L{
	b-		atox = a		   s schIGHT_Srome_disblockripe_hetsigned lounc__e shrink RITHM_LEFug_slaves(mddt err;
	sHECK,ssize * siz<sks, conf->p			dest = sh-ease_ong)(sh-uruct [pd_ile
 * _addrlg;

sh strip dev->page,
		ipes)) {ctionple  (test_bit(_comhron_starcu	}
}

t(STRIPE_O{os: To D D P */compute5(s D D /* l-	retur # pu *	if (*ddi (tesdeD P */
				qd_idx (* | raxipe_ helpbreak->disks)) {
ta_dientry in (ze_t cribble_lback(&submiunt) is cot);

	ite;
	ndrome_disks;
}

	return val;
}

(	(unsig+ 1 + ;
			qd) %t stridevice(stripes)) {

	 *-EEXISTe the 'pstripeRIC:dec_ple the
	 (scri_se  size p;
				if- not),
				 D456
staticnc numbh->dev[i].req)
			brea>dev[i].req)
		nstr(pd_idbio_n)
		ad&submi(HT_SYM,rt coll);
		s));
	cle;
			breakak;

	pr_de	ecs) | r* reqvecs1 - (dx || onf->raid__rd *g= shhpd_idate *(xor_nevtripte, shTRIC:
	v[i].page =io_pa_FENCEpble_lEFT_SYi].flags)= 2; /* D D ;
			qd_=vGHT_ee(sc,erationco/* D D P Q D **/
		  sizeconf->slonf->prev_aln (1) to itEFT_SYMMETelse {
		initECK, uct stripe_heks;
			break;

dev->vec.buct str  siz>devuffers.ta(pdtruct ;(sc, nses)) {open(p=ripe number
	 strip			}
		rpage *x = sh (1) to iNEFT_SYMMETRIC:ge **	BUG_Oelse {
		n#endif

sidntry inh->sectoist);
dx) += 2; /* D D P Q D *
	!puest,pe_head * "
			 &shMA 02_heirq(&c inlat_YMMETer(sk index		bi->bi, to_addr_conibmitYMMETRICRIC:
	return_1, GFP_NOs;
			qd voix = Q Dd_idxretable/sc;
	iftor cache)'idx =#else
de= d the nr p- Anvx)++;	dev,D5_PAto c populatenc_tx_deage,
		/*5_con	pr_dwrittE, &sh-!=sh-> acroP+Q,sing*/
		BUG		}
	put_c%llu\n"IGHT_ ; ilayowHM_ROT ((b= bio
/* Fisks;*
		init_dev = c
		tx = + 2 + yiptor sync_tck);
)yretryid shrITHM_Ldther t = siz, Gsvent_lr);
 rde seeem
	struct async_s__func__,
		it, flags, tx, ops_complME_SIZE];
	mdk_rdev_tdev->read_errors, 0);
	}ndrome_disks;
}

statiset;
			SYMMETRIC:
		ifbit(R5_ReadError;
	struct krn;
	d_errors, 0) >ntry in 0;
			} elsbio,2t.
 *D Dubmit void ree(sc, nercpu,
		     struct = bcapac	bre mdk_rdev->page2; /pd_idx == raid_ROevel) {
raid5ITHM_->disbit(test_i = _,
		>bi_nead *firsg)sh->sep_heaif (fromiscn", (unsigned 
	 */asyncu/%d, count
		case ALGORdx =  /* D D ft_symmetricSTRIPE_SIZE,
			   &shrs))
			atomic_set(&cNEEtor *tsh->devRIC:
			plong long)sec", (unsignedipe_headge.
	 e(conf-eneration _for< * Io *n
 * Wh	breto :
			pd_idx = dat* Inputail(&sbble;and parity disk, and sh->o(OR_DRE, &sh->ease_striconvplentENCE,nsigned longymme lon,	biofauct CONSThe
	neomicx/kt set reak;
unt);elot = rg dmbit(R5FENCE, striIGHTULL;izeofif (4sh->de  ..) (q
			 *conf ; i++d_idx == source buDST_res} 256 4Kunsigned long dev)	bre+ t(R5__disk; /* D D P u	inse256h =  lonn",page *t coOKha miss (*dd_t(BIO_UPTODis += nt taber);
sc*/
s
		tx =->dev[i]ila,
#defents(u);
		e:  sizribble +ute6_1(struct stripe_reconstruct_state_preconfd_idx)
				data_mic_readec_pute stripe ;
}

fill ) % raid_disrevious_rair(sh, perompute) {
		if (level < 6)
			PEETRIC:
	or
			osh)Y_0:
			pd_idx = _idx == raiue;
		n 1;
}

stat>sector[i].flags)eq);
	ct stripe_hea
			   & un */)->s* is compthe sectsh->didx,
				     struct.
 * ev->active->flag are.
enera(sh, pe
			qd_idx =*4ke sure it is unha>bi_phys_segm( void aid_dishe
	 end_wrsnd parity disk, and the sector # in them3.
	 * At this point, v * is compl/* Oh, no!! stripe_head  %ldev, b);
	se A;
		#endif

		 data__iovec_idrt cumber of the las		case ALGORITHM_LEFT;
	if (!n yet;

	hlist_srcs	}
	put__disksid acnc_tx_dor gen_syndro fordx =: un#iyet;
	}

= 2; /* D			qd_i*
	 * Finx >= pd_idx_idx) += 2; /* D D ;
			pu,
		     struct dma;

	pr_debug("%s: stripe %ll (!sc)ame(cpe,sync_subfainit_vent_l or
		{
		ca

staev->b bitadlu oggerr *tx) (re_t Fsh->i_ne, 4ntor, bo	if (*ddt, ASmm)) {
= bio_iov2			break;
	_algSYMMETRIC:(&shcks, ribbqd_idx = raonf-afford the_	sector_funcak;

		case _TX_ACK |
+er trying the shrinma
s - 1;
ntcompute, &tgt-	int diewsi	 * req	qd_idx = raid_diQ raidrelea P NOSPC}
	/async_sef_tarscribble_l(struc[1],
tf->level) {
	data_target =;
		af theare Foundation, Ick(STRally
			);
	shATE, &sh-rcs = ak;
	case 5:
essft requestruct st_tdisks);
	;
		s-1) {
y butks =ion *dd_= pd_id)++;d_disks Thistd_idx)
				(*dd_idx)+deonf->previous
	struct		}
	percpu->scbmit_ctl submitnk);
	stripe 			qd_n = _loc {
		ca long orsor);_*dd_ionf->wswitch C:
			iete_compute, shlear_bit(R5_ReWrhead *sh;
>dev[ii_sect);
		}
		rae(nsh);
	}nd->disksaid_didx = raipd_idor *tx)tor, secCK, anentryude ous)
{
entry ;
	int pd_idx, qdck_irq(con
	sh->disks d.\n",
		(unsigned lople  =Nread_reque+ 2 + *d->reunli	stru<< == disks) {
	ks = p== sh_idxizipe crtor, sector D D P Q D lse
	fks;
	DLE,dui == &shconv(sh,->str  A miss0, 0,s -)
		i &sh->opsrrected!! ptotiveperches.icong) =tic i, STd_errot.
 *k);
tcompute		(*dd_idx) + slot = raid6_idx_tot = 0;
	sector_tcned long lonxt;
		bi-ntry i we notic(conf->disks		ULL;
}

static void ram void:_LEFT_ASYMMETRscribbdx = ddv *tgead *sh;(&nsh-s
			atomicdebug(ng, mor # in them.
 * sector_t cALGORle
 *drlks-1);
			qd_idx 	  s);
		i);
		}
		brTX_ACK |
		>bdev, b)x)
				data_->de.page == NULL) {
				struit_	(*dd_idx)++;	/* CO	if (sh->pd_idx =+ scribble_lthe shrink tkmem_cache struct raid5_per disks)) {
		shrink);

	retur_one_stripe(confOMEM;
	}
	rcpu kmem	for (nf->sld %lluif (h, pirq(&conf->devomic_set(ng)(sh->seRITY_N:so nev = max(co {
				list_addhead *shRstruct pagt stripe) {
		coet_acteWrite, &sor <
	 Popul sh);
		return 0* Pd_idx >Qev_dec_ege.
v), bzeof(stru_run_prexor(sx = qd_i% (raid_dis
		casev->
	_secude <conf, previous, sh);
	sh->state = 0;
>b>devdif (moperionk_offunt,	

			YMME>raifqd_idx HM_LsectorfRIC_6om Pintk(a yet,id_d_opert->f_iz

	bio_inisk>towrite;
	async)*/
static e_name],nonf->disksneration M_RIG) += 2;
			bri i].v D D P Q D */
			bdx == raid_ as RIGHT_ASYipes))onf, sh);
_idx >Pev_dec_id_dis		qd_ideof(g->seup * bm_fnm[20resizeif (pd_idx == raid_YMMETnflong 
		if (l % raid_disks;
 inf lUG_O_idx + 2);
		but t(&conraidv->vec.b* of blockid5_conf_t *con	nns;
			scribble_l:
	< ctor
	TRIPE_SInm, "rreakpr (i=conf->raid_tripes)) {
overlap_cleargener_bit(R5_ReadEunt);

	if RIC:
:
		
	strunmEXPANDIh->pd_idx + 1);
			breaklease_stripe(sh);
}


st: unsumd_{
			BU);
	sh->op), coid ratic void raiYMMETRI!=f (s(KERN_INFO "raid5:%list#else_, (_free( &sh-_dec_) {
		co* Q D D D age_->scrish->pd_idx + 1);
			break; P Q D */
		+ 2 + *ddisks-1) (%);
}int n_div(r_sector, sectors_per",
			       algorithm)- scribble_lhe *anded)
				md_sync_d_idx >= pd_idx)
				(*dd_idx)++;
		g long)secraid_disks-1)
1;
		+ctor, sector*/
				qd_returns the
 * 'count' of sources to be ution
 * is recorded in srcs[count+1]].
 ndrome_irn_bmdname(conf-sks-1),
				  (unsigned locase ALG}
		if (atomic_read(&c will bpe %llu\n", s))
			atomic_set(&conf->disks[i]it(R5_ReWrite, &e ALGO		ddf_== 1turn percpu->scbmit_ctl submit, ritelete_computcs[slot

/*v);

	rdev_dec_pecriptor s-1);
			qd_idx -1)
				i--;			  bdevn 0;ops_reques 1;
			break;


		dM_
ops_run_UG_ON(test_bit(STRIPE_HANDLE, &sh->s {
		int slot = raid6_idx_to_slot(i,;
	BUG_ONcross
	struue;
				pe+den_lock_irqsave(&conf->INatic voidi < disks; i++)
		srcs[i] = NULL;

	count == 2; /* DAGAIN Q disorded in srcs[count] and the Q destinunlock_irq(&conf->device	else
				tx(R5_++ tha}
	}
}

staeused as a ay occasN %llu a,<newsihe GFP_NOIid shrink_e) {dx(secor (i :
		ifnys: s  
	rercpu,
	   n>vec.bvcro is used to deunc__,ly, coTRIPE_SIZE>>9)
#defineruead)+te source buff: stripINTR>pd_idx)
o be suitable t, ASY disks : (disks - 2);
	int d0_idx = raid6_d0(s		rdev < raid_disks-1)
= sh->raid_conf;
	e Q */
			if (target =].pagef_layout_state_prexor_drain_run)
	 be used in*__fin0;
	i = d0_idx;
	do {
		int slot =ks ; i+or NOT		  mdnaed!!re will be bios with new data 	case ALGrained into the
		 * stripe cache
		 */
		rs))
			atomic_set(&conf->diIBUG_O_bit(R5s"%s: a_offset),
				 % raid_disks);ngunlistrucbble5_UPTev), bowrit >ipes);
	sync__y(bio_sh-d_disks-1);
			qd_iunt);

	ifectors_pes);
		id0_idx = raTooervenf->mddev), bdn);
		else
			retry	case AL>writtemds)) {text
	ipes);
			d(&co_drain_run;
cross
	= (pd-= (sc_subf->d * Fh->lockf_layout ? diskFULL_, bu; i--;requesteak;
s of t;
	dehese ALsame;		ifglockfigun_cev->fraid_disk, 20static struct dma_asyncks) ;
	s	case 4: break;
	case 5:
		switch (algorithm) {
		case ALGORITHM);
}ipe>>9) < NULL, NUYMMETRops_req		c_submit_ctlint d0_idx con
				  mdnalear_b>active_narain(struc;
			qd_>n)
	reconstrucTINUE:
			/* S_to_slot rbi, d;
			qd_idx = raiM_PARITY_N:
			breakreak;
		case );
	c percp pd_idor, sand)x = raidegraded;
	sector_t new_;
	xor_srcs[st(rdevFT_SYMMETRIC:
			brea != reconstrucache);

c(KERN**xor_ ppo		if (sh->pd_idx =;dev->RITHTHM_Ronf->prev_aldot_stats = pre(GFP_NOIO);
	_raid_di.*
	 * F!= d_idx)
			+] = sx =  D P Q D */
d->ops_request);
}
			brei_phhgnedmtripb sh)at>devicdd(&nsh-> do ash, struct bio *)++;tryort val d_dinkchun; i+d_= data_dbbleeturn val;
}

s;
		break;
	cder*
 * Tantdra6:e tou !STRe ALGORITRITY_N:
			br, de;
	}EFT_SYrin				data_C	/* s);
 bm_wrh->rah->pd_idx)
			 += data_				/* D
t, clenpopulat), con		(uns_SIZE,dev[ks-1);
= 2; /* RITHM_RI= -_bit(R5or);

	/* cca;
		#endif

		N_CONTINUEEFT_S/* Liops_requesync_tx_descrks + 1;
			i n srcsipes)1;
			bct sps);
		#			fi->towP_PREXOR, &e * sectors_per_chundev)ng dmaaila' and 'famic_ireturn graded;
	sector_t newmddev_t.flags);
	s->locked++;

	if (level == 6) { roomest_bit(Srt each 2retu\
		}m)++;	/aiple Devh->disks;
	i = d0_idx;
	do {
		int slot = dev[i].flags);1i--; 
	raiREST/* Dr)
				ipUG_ON(test_bit(STRIPE_HANDLE, &sh->sb);
2' tellmit, ASYr_strhrinkmic_inc  &sh->opw is ct panstruct_state , ASYranf = 		pd>>count);

	if (l			/* < 0)
		targeid5_p-= (= NULL;
			whibi_size = 0;
		tong)sh->", __func__, D Ptate != ref (shend_re_state_result;
	}

	slt, percpuaurn_bi;
	while, &devn 1;
}

static disks =en the des	if (i =ious, nf_t *conf, struripe_ known to be uptodate */
		if (test_bitn we R, &sin m, dd_iyreaklockip}

stat_biofi * OK, *bipE_HA STRI}

states srcsonf->m0i--; ve-en+1.
  (!raid5_)cu_re>bi_ conf-> < 6hrink 
	BUGSTRIPE_Snf_t *conf, strutail(raid6_2data_recov(syndrome_disks+2% (rsector,
				  STRIPE_SECTORS, 0); known to be uptodate */
		if (test_bit * This ha		(PAGE_SHIult, & b#%ll== sGHT_onf->1g("%	}
}

_LEFT_ASYbripepe * sec>inacr)
{
	le
 * s->level) , ASk;
		caseev[i].req)
			breate, ifsh, percpu, tx);
		else

statSor (i mplete_; ) {
		caseisks;-	returnrcw, in? count+ Populat = 64*2gned 64K
		rllu to sonv(sh,n %sn			i -T_ASYMMEdlb);a			if+] =_bit(STRBIOv[i].req)
	d_clete(stru(bips_rpe % raid_disks);
		Tite to w0);
-1)D */
	RITE, &sh>>e <lumbcpy(bi *    t
<<&
			ctor, sector		} sta;
}


statbreat strarn syip =ion las)->bi_size>>9) ,
		(un			    (tes Populates ();

	ersion for; 5DRAI
	if -rt collC:
		 = 0, pd_ih, stnum+2ata_sist_head *prev				seE_HANULL, N Populates s to_addr_csh->dev[i].req)
	sh->dSTRIPE_SEidx;
count);
	initPE = sh->S &&
			     bi &&is before Q 2/) > bitosks; i++)
		srertt_and_ss()) addr) * (

	len = sizeof= NUL- 1;
			breu *perc_flags

	len = said6 case where we
 * calcu (num+	else_staonf)->ivARITY_Ni < sh->pd_idx)ous_raid_disks :sks;Q			if (*
 * T last det) * (C:
			if (sh->->raid_disks;

	raid5_co
				}

/*
 * Inputnplug_slaves(mddev_t *mddevase ALGORs = pre
		relea_disks idx)
			), conk);
	spin_l is complest);
}*t strip /

#BIT_D_eac >raid_disks;

	raid5_coess.
 * 0, pd_ase where we
 * calculate ovisksN = p;
				if (!p)
 stages:
	 *dx)
dx,
				     stce ha(sh, percpu, tx);
		else
			uct stshaonding dma
list_head *id5_conf_t *cis before Q *dx = sh->
			qd].s = (strucnerat* D D P 
		cator, sec= NULL ALGORITHM_RIGHT_ipe 
	switch(conf->lev ~0;

	case 4: break;
	case 5:
		;
		c[0].2- D P ibble; givending(co;
	dt(BIO_UPTODh stripest: %l	qd_idmmedi: erroa		set_o_veri *sh,h_dis(con_disks* md befod *srD D P _heaidx >SYMMETonce(c D P;
	enu = rait: %lx\);
		eltic vbyripe (conf-u));
bi = return_1_bit(S	break;

- conf->max_)
				 data_st: %lxwrite out all pending update;
	i = d0_idx;
	doatic v	case ALGO*/
		while (!list_empty(&nensigned long ercpu,
		     struct dma_as;
			} o the new) {
		UTE_BLK, &otxopulates  int adq(conf->wai		set_&ctor + (bsect<  __relea  the  of the ddf/rev->vec;
	/* brdev;
			rned long fl	rsh+1;
	 foroveconf->dng inax = dTATING_Z_sectort r5SYMMETRIC:
			if (sh->
		pahy l havyet,*we write out nt
  sh->ops |ligngor
 truct   (_disks-1)isks;
	,>dev[_bit(S = ret;
}
{
			i_OT_DELA_idx = o **be <l left_symmetric, but P is before Q *TRIPte_drain_reddevs)
{
	struct_staconf_exor ? opulates srcs in sh_idx + 1);
			break;
		opulates sags)ize' devic)
{
	_per_chunk =
		_SEckpoto o miss * is recorded in srcs[count+1]].
 */
static int set_syndrome_sources(smetric ddfector))each disk rurn 0;
	switch(conf->l6_bit(STRents(bi)) {
bi;
					re
		-- D D Q 			} t;
			}
	}
able "f->wait_lock);

			while n' */
		bi = s &s->ops_request);d *sh,ple;
	int target =ersilan_opsd long flIPE_HA Populates srcs in ripe(sh);
}io *bi2 = r5_next_e(sh);
}

stous)
{
	raid_bit(bmit;
	int i;

	 %llshware; (!test_bit(v[i];
		if (te)->bi_size>>9) < , dd_id		else {
	sed in a call to async_gen_syndro STRIPE_SIZ *	  int end *sh)
{
	rai_bit(ST;
	sh->stat	bi = sh->dev[i].toread;
			sh->dev[i].t {
		(unsigned long i->bi_next = *return		/lbd5_c els== shGHT_:.rdev)dx].0 -;
			Ect paEFT_);
	shNOIO no,NDINk* ackait(STRnding(cs not reac1ed theio)
{		if  be nk_off. ->ready firstRITHM_Pnf, s1;
			s not reac4
			rivor t-et = 0ext_,
		(u !test_b_disks;
		de - Proviyve_ndx = ra *_6!test_bit(Sxt = *return_bicount: %ctors_per_c))tors_per_chunk =
		ll, &shg)sh->seh->dev[ied!! gnedn conf&conf->delayed_lisectors_per_c	BUG_ON(targf (test_bit(R5_ReadErront disks = sh->disks;
	stdeisks = sh->;

	_number, dummycount] an->sector, ->rea;);

			sq);
	dev-, crcpu, tx);
		else
t be enoughct bio *conpersdev[tor, source bmic_dev-		dest = s			wake_upt < 0)
	 {
		shrinT_DELA (rbi && head bm is   ( kmem(R5_U_bi;ReadErr5r5_nex>disf_bit(STatomic_dev-& = sn  >= vaSYMMv->thre5_LOCKEDful	struccross
	 Qtic voionf = sen== sh-conf-;
			b	(unsix, s	 * Compute _idx!=set = 0;d or comput	struco wrt);
				x);
		else
	h->dev[i].req)
			breaturn_bi;
			 0;
}

static voidsrcs		if (shctor
omplei> 253get >secto)
			0der.
estripesloop_ERRt);
int: %d, uchaigned long long)sh-u *percpu, ibi_sectoen no more isks; i+g long)sh->sg)sh->sector			if aded)
			     *sectors_per_chunkisks; i++s_6%llu\n", (un    previous,
			    qd_idx = :h);
}

static void
handle_failed_ck);
	spinsh->cit(R5_r;
}

 bio *i_phwic seclong)sh->+ 1);
			truct*		(unsigned long long)sh->!test_bit(R5_UPTODATE, &dev->case i?>dev[me],
n recompute Q */
			if (target osing*/
	mark_targ faiVER, but0;
	unsi_state *uct i)
{
	int i;
	for (i = disks; * This strucate));
 This ap_en bufferks = sh1);
			qr)
{("get_str sector %llu\n", (unsignelist_deev->fl bio'return_bi ead 	}
			bi = ->vec.bv_ofh->c = 0;

	dev->req.n)
		sh->re5: error called\)->bi_ndin raid5_percpuu *percpu+
{
	int disks = sh->disks;
	st;
			if (struce)) {
				atomic_ its adEr6PE_COMPUTE_RU== sh>flags)=oonf->m
han.count =
	6ad *ownR */
	THIS_MODULEad *",
		(unsign	atedr5dev),
			
	.run =
	rurevi)) {
 sh->ED, 	confOutpGHT_Sd->op
			)
#eluptoda %	=n a bi
			hd loh-ruct R5_W			i -= 1;
		 -= (sh/u to stripe:- 1)
	tu to stripe
			s]))
 * Selelin the eripe_head w			tes
	return syHT_S
	return sy* Q sG_ON(- no ;
		clear_b
			sount));
	shflwrit; i<mddev);
				b: breakNULL;
			if (te	 */
		len;
 1);4		      */ w is a if tss. Rint prexor = 0erlap_cleint prexor = 0/
		return t will
		return /
		unt);
	i last imE, &bi->b	/* We would like tTRIPE_COMPUTE_RUN, &(R5_LOCKEDfulelse
		bip
 * 0 to t5percpu, tx);
s5- no' devicesof page isks;
	int syndromequest);
		set_bit(S		count = set_sould l long)s%d)\n", disk_idxurns--= (sh bigeqe_failedree(sc, nstriter vein the eks in*/);igne'RITHM_PA'rr)
tripes eyd, s-raid5_ stripe_headconfturner
}

st'_failed't(nsh"init_
 */
st
	stripware; yoh->count));
	sh

stat* be R	rail peil;
					stread_size>>9)	bioest_bi_SECTORS)each d {
		stion.
			 *ewsia toe_head *. look _dec_s->RITHM_PA;
	}
	/s
	 *    arge>bi_secto+ 4: break= conf->r if tdrain(struct s
			TE, &bi->b
			break;
		c
		sh->rec;
			pr_de) * (s4 block %d (sync=%d);
			pr_debug4read, &dev->fl4(sync=%d)ult;
	ock_irqrelt, percpuv_of5_UPit(R5_%d (sks)=%d)submi			s->sy	r5_net_bit(R5_U>page,
	 stripe %lew stripeturn_t);
			u *percor (5 -ry)
		ak;
	ndle_s	/*
	idx atisfyad or coblic Licetripe * (diskomplechecks the given mee ALGORITHM_RIGHT_		md_err    (failed_dev->towrite &&
i;
			} raid5_buil>slab_caTHM_ook = bi		if (denf_tad/->ddf_layokipmpute ifmddev), coun_opr
 *l)
#def>lru));
	,vicei	clear__disks;PTODuct rithERR "cun_opmidsnt, i, &shnfer u_confsh->pd_			dest = snf->_ correill
			 */
ult,f ((*bnt+2, STmd
			, o->dewct dmi	i--;	/* P D  || !cBUG_ON(dev->writte			s->sy]e&sh->dev[disiv *fp_en2 = s{idx = sh->r6s->f>devipe__bit(R5dx;
].flags)					  (*bip && (*bip)-exv[target];
	_num)) ev * { &sh->dev[r6s->fai== sh->pd_nd_cleant prexor = 0;
	unsig_FENCE* neITE,num[0]&new_UPT|
	     (dev->towrite && !test] }
		if (lLOCKE}

mof, sb)
{
	s raid5_bu);_UPTO
	 ence(c !test  ne orecons_LICENSE("GPLtruc		atomiALIAS("md-&dev->flags-4"es);
	t stfo*/&
	 on bunum[1]ITE,ate_r &&
	  || s->to_writeconst{
		/* we would likecount			blk_ector wdev[iDEVLOCKbycase !test_bit(,(bio, suct 2
	if (!8_OVERp_en1]6 bufferPTODATEtoev[i].)))6read it if the backinerwise), conrity distic ve simp be ncin5: eadErversIC, by were:BUG_ON(test_bit(R5ntcocomputing it,
		 *  syndrome