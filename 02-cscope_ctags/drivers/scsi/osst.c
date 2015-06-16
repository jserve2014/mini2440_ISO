/*
  SCSI Tape Driver for Linux version 1.1 and newer. See the accompanying
  file Documentation/scsi/st.txt for more information.

  History:

  OnStream SCSI Tape support (osst) cloned from st.c by
  Willem Riede (osst@riede.org) Feb 2000
  Fixes ... Kurt Garloff <garloff@suse.de> Mar 2000

  Rewritten from Dwayne Forsyth's SCSI tape driver by Kai Makisara.
  Contribution and ideas from several people including (in alphabetical
  order) Klaus Ehrenfried, Wolfgang Denk, Steve Hirsch, Andreas Koppenh"ofer,
  Michael Leodolter, Eyal Lebedinsky, J"org Weule, and Eric Youngdale.

  Copyright 1992 - 2002 Kai Makisara / 2000 - 2006 Willem Riede
	 email osst@riede.org

  $Header: /cvsroot/osst/Driver/osst.c,v 1.73 2005/01/01 21:13:34 wriede Exp $

  Microscopic alterations - Rik Ling, 2000/12/21
  Last st.c sync: Tue Oct 15 22:01:04 2002 by makisara
  Some small formal changes - aeb, 950809
*/

static const char * cvsid = "$Id: osst.c,v 1.73 2005/01/01 21:13:34 wriede Exp $";
static const char * osst_version = "0.99.4";

/* The "failure to reconnect" firmware bug */
#define OSST_FW_NEED_POLL_MIN 10601 /*(107A)*/
#define OSST_FW_NEED_POLL_MAX 10704 /*(108D)*/
#define OSST_FW_NEED_POLL(x,d) ((x) >= OSST_FW_NEED_POLL_MIN && (x) <= OSST_FW_NEED_POLL_MAX && d->host->this_id != 7)

#include <linux/module.h>

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/mtio.h>
#include <linux/ioctl.h>
#include <linux/fcntl.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/blkdev.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/smp_lock.h>
#include <asm/uaccess.h>
#include <asm/dma.h>
#include <asm/system.h>

/* The driver prints some debugging information on the console if DEBUG
   is defined and non-zero. */
#define DEBUG 0

/* The message level for the debug messages is currently set to KERN_NOTICE
   so that people can easily see the messages. Later when the debugging messages
   in the drivers are more widely classified, this may be changed to KERN_DEBUG. */
#define OSST_DEB_MSG  KERN_NOTICE

#include <scsi/scsi.h>
#include <scsi/scsi_dbg.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_driver.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_ioctl.h>

#define ST_KILOBYTE 1024

#include "st.h"
#include "osst.h"
#include "osst_options.h"
#include "osst_detect.h"

static int max_dev = 0;
static int write_threshold_kbs = 0;
static int max_sg_segs = 0;

#ifdef MODULE
MODULE_AUTHOR("Willem Riede");
MODULE_DESCRIPTION("OnStream {DI-|FW-|SC-|USB}{30|50} Tape Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS_CHARDEV_MAJOR(OSST_MAJOR);
MODULE_ALIAS_SCSI_DEVICE(TYPE_TAPE);

module_param(max_dev, int, 0444);
MODULE_PARM_DESC(max_dev, "Maximum number of OnStream Tape Drives to attach (4)");

module_param(write_threshold_kbs, int, 0644);
MODULE_PARM_DESC(write_threshold_kbs, "Asynchronous write threshold (KB; 32)");

module_param(max_sg_segs, int, 0644);
MODULE_PARM_DESC(max_sg_segs, "Maximum number of scatter/gather segments to use (9)");
#else
static struct osst_dev_parm {
       char   *name;
       int    *val;
} parms[] __initdata = {
       { "max_dev",             &max_dev             },
       { "write_threshold_kbs", &write_threshold_kbs },
       { "max_sg_segs",         &max_sg_segs         }
};
#endif

/* Some default definitions have been moved to osst_options.h */
#define OSST_BUFFER_SIZE (OSST_BUFFER_BLOCKS * ST_KILOBYTE)
#define OSST_WRITE_THRESHOLD (OSST_WRITE_THRESHOLD_BLOCKS * ST_KILOBYTE)

/* The buffer size should fit into the 24 bits for length in the
   6-byte SCSI read and write commands. */
#if OSST_BUFFER_SIZE >= (2 << 24 - 1)
#error "Buffer size should not exceed (2 << 24 - 1) bytes!"
#endif

#if DEBUG
static int debugging = 1;
/* uncomment define below to test error recovery */
// #define OSST_INJECT_ERRORS 1 
#endif

/* Do not retry! The drive firmware already retries when appropriate,
   and when it tries to tell us something, we had better listen... */
#define MAX_RETRIES 0

#define NO_TAPE  NOT_READY

#define OSST_WAIT_POSITION_COMPLETE   (HZ > 200 ? HZ / 200 : 1)
#define OSST_WAIT_WRITE_COMPLETE      (HZ / 12)
#define OSST_WAIT_LONG_WRITE_COMPLETE (HZ / 2)
	
#define OSST_TIMEOUT (200 * HZ)
#define OSST_LONG_TIMEOUT (1800 * HZ)

#define TAPE_NR(x) (iminor(x) & ~(-1 << ST_MODE_SHIFT))
#define TAPE_MODE(x) ((iminor(x) & ST_MODE_MASK) >> ST_MODE_SHIFT)
#define TAPE_REWIND(x) ((iminor(x) & 0x80) == 0)
#define TAPE_IS_RAW(x) (TAPE_MODE(x) & (ST_NBR_MODES >> 1))

/* Internal ioctl to set both density (uppermost 8 bits) and blocksize (lower
   24 bits) */
#define SET_DENS_AND_BLK 0x10001

static int osst_buffer_size       = OSST_BUFFER_SIZE;
static int osst_write_threshold   = OSST_WRITE_THRESHOLD;
static int osst_max_sg_segs       = OSST_MAX_SG;
static int osst_max_dev           = OSST_MAX_TAPES;
static int osst_nr_dev;

static struct osst_tape **os_scsi_tapes = NULL;
static DEFINE_RWLOCK(os_scsi_tapes_lock);

static int modes_defined = 0;

static struct osst_buffer *new_tape_buffer(int, int, int);
static int enlarge_buffer(struct osst_buffer *, int);
static void normalize_buffer(struct osst_buffer *);
static int append_to_buffer(const char __user *, struct osst_buffer *, int);
static int from_buffer(struct osst_buffer *, char __user *, int);
static int osst_zero_buffer_tail(struct osst_buffer *);
static int osst_copy_to_buffer(struct osst_buffer *, unsigned char *);
static int osst_copy_from_buffer(struct osst_buffer *, unsigned char *);

static int osst_probe(struct device *);
static int osst_remove(struct device *);

static struct scsi_driver osst_template = {
	.owner			= THIS_MODULE,
	.gendrv = {
		.name		=  "osst",
		.probe		= osst_probe,
		.remove		= osst_remove,
	}
};

static int osst_int_ioctl(struct osst_tape *STp, struct osst_request ** aSRpnt,
			    unsigned int cmd_in, unsigned long arg);

static int osst_set_frame_position(struct osst_tape *STp, struct osst_request ** aSRpnt, int frame, int skip);

static int osst_get_frame_position(struct osst_tape *STp, struct osst_request ** aSRpnt);

static int osst_flush_write_buffer(struct osst_tape *STp, struct osst_request ** aSRpnt);

static int osst_write_error_recovery(struct osst_tape * STp, struct osst_request ** aSRpnt, int pending);

static inline char *tape_name(struct osst_tape *tape)
{
	return tape->drive->disk_name;
}

/* Routines that handle the interaction with mid-layer SCSI routines */


/* Normalize Sense */
static void osst_analyze_sense(struct osst_request *SRpnt, struct st_cmdstatus *s)
{
	const u8 *ucp;
	const u8 *sense = SRpnt->sense;

	s->have_sense = scsi_normalize_sense(SRpnt->sense,
				SCSI_SENSE_BUFFERSIZE, &s->sense_hdr);
	s->flags = 0;

	if (s->have_sense) {
		s->deferred = 0;
		s->remainder_valid =
			scsi_get_sense_info_fld(sense, SCSI_SENSE_BUFFERSIZE, &s->uremainder64);
		switch (sense[0] & 0x7f) {
		case 0x71:
			s->deferred = 1;
		case 0x70:
			s->fixed_format = 1;
			s->flags = sense[2] & 0xe0;
			break;
		case 0x73:
			s->deferred = 1;
		case 0x72:
			s->fixed_format = 0;
			ucp = scsi_sense_desc_find(sense, SCSI_SENSE_BUFFERSIZE, 4);
			s->flags = ucp ? (ucp[3] & 0xe0) : 0;
			break;
		}
	}
}

/* Convert the result to success code */
static int osst_chk_result(struct osst_tape * STp, struct osst_request * SRpnt)
{
	char *name = tape_name(STp);
	int result = SRpnt->result;
	u8 * sense = SRpnt->sense, scode;
#if DEBUG
	const char *stp;
#endif
	struct st_cmdstatus *cmdstatp;

	if (!result)
		return 0;

	cmdstatp = &STp->buffer->cmdstat;
	osst_analyze_sense(SRpnt, cmdstatp);

	if (cmdstatp->have_sense)
		scode = STp->buffer->cmdstat.sense_hdr.sense_key;
	else
		scode = 0;
#if DEBUG
	if (debugging) {
		printk(OSST_DEB_MSG "%s:D: Error: %x, cmd: %x %x %x %x %x %x\n",
		   name, result,
		   SRpnt->cmd[0], SRpnt->cmd[1], SRpnt->cmd[2],
		   SRpnt->cmd[3], SRpnt->cmd[4], SRpnt->cmd[5]);
		if (scode) printk(OSST_DEB_MSG "%s:D: Sense: %02x, ASC: %02x, ASCQ: %02x\n",
			       	name, scode, sense[12], sense[13]);
		if (cmdstatp->have_sense)
			__scsi_print_sense("osst ", SRpnt->sense, SCSI_SENSE_BUFFERSIZE);
	}
	else
#endif
	if (cmdstatp->have_sense && (
		 scode != NO_SENSE &&
		 scode != RECOVERED_ERROR &&
/*      	 scode != UNIT_ATTENTION && */
		 scode != BLANK_CHECK &&
		 scode != VOLUME_OVERFLOW &&
		 SRpnt->cmd[0] != MODE_SENSE &&
		 SRpnt->cmd[0] != TEST_UNIT_READY)) { /* Abnormal conditions for tape */
		if (cmdstatp->have_sense) {
			printk(KERN_WARNING "%s:W: Command with sense data:\n", name);
			__scsi_print_sense("osst ", SRpnt->sense, SCSI_SENSE_BUFFERSIZE);
		}
		else {
			static	int	notyetprinted = 1;

			printk(KERN_WARNING
			     "%s:W: Warning %x (driver bt 0x%x, host bt 0x%x).\n",
			     name, result, driver_byte(result),
			     host_byte(result));
			if (notyetprinted) {
				notyetprinted = 0;
				printk(KERN_INFO
					"%s:I: This warning may be caused by your scsi controller,\n", name);
				printk(KERN_INFO
					"%s:I: it has been reported with some Buslogic cards.\n", name);
			}
		}
	}
	STp->pos_unknown |= STp->device->was_reset;

	if (cmdstatp->have_sense && scode == RECOVERED_ERROR) {
		STp->recover_count++;
		STp->recover_erreg++;
#if DEBUG
		if (debugging) {
			if (SRpnt->cmd[0] == READ_6)
				stp = "read";
			else if (SRpnt->cmd[0] == WRITE_6)
				stp = "write";
			else
				stp = "ioctl";
			printk(OSST_DEB_MSG "%s:D: Recovered %s error (%d).\n", name, stp,
					     STp->recover_count);
		}
#endif
		if ((sense[2] & 0xe0) == 0)
			return 0;
	}
	return (-EIO);
}


/* Wakeup from interrupt */
static void osst_end_async(struct request *req, int update)
{
	struct osst_request *SRpnt = req->end_io_data;
	struct osst_tape *STp = SRpnt->stp;
	struct rq_map_data *mdata = &SRpnt->stp->buffer->map_data;

	STp->buffer->cmdstat.midlevel_result = SRpnt->result = req->errors;
#if DEBUG
	STp->write_pending = 0;
#endif
	if (SRpnt->waiting)
		complete(SRpnt->waiting);

	if (SRpnt->bio) {
		kfree(mdata->pages);
		blk_rq_unmap_user(SRpnt->bio);
	}

	__blk_put_request(req->q, req);
}

/* osst_request memory management */
static struct osst_request *osst_allocate_request(void)
{
	return kzalloc(sizeof(struct osst_request), GFP_KERNEL);
}

static void osst_release_request(struct osst_request *streq)
{
	kfree(streq);
}

static int osst_execute(struct osst_request *SRpnt, const unsigned char *cmd,
			int cmd_len, int data_direction, void *buffer, unsigned bufflen,
			int use_sg, int timeout, int retries)
{
	struct request *req;
	struct page **pages = NULL;
	struct rq_map_data *mdata = &SRpnt->stp->buffer->map_data;

	int err = 0;
	int write = (data_direction == DMA_TO_DEVICE);

	req = blk_get_request(SRpnt->stp->device->request_queue, write, GFP_KERNEL);
	if (!req)
		return DRIVER_ERROR << 24;

	req->cmd_type = REQ_TYPE_BLOCK_PC;
	req->cmd_flags |= REQ_QUIET;

	SRpnt->bio = NULL;

	if (use_sg) {
		struct scatterlist *sg, *sgl = (struct scatterlist *)buffer;
		int i;

		pages = kzalloc(use_sg * sizeof(struct page *), GFP_KERNEL);
		if (!pages)
			goto free_req;

		for_each_sg(sgl, sg, use_sg, i)
			pages[i] = sg_page(sg);

		mdata->null_mapped = 1;

		mdata->page_order = get_order(sgl[0].length);
		mdata->nr_entries =
			DIV_ROUND_UP(bufflen, PAGE_SIZE << mdata->page_order);
		mdata->offset = 0;

		err = blk_rq_map_user(req->q, req, mdata, NULL, bufflen, GFP_KERNEL);
		if (err) {
			kfree(pages);
			goto free_req;
		}
		SRpnt->bio = req->bio;
		mdata->pages = pages;

	} else if (bufflen) {
		err = blk_rq_map_kern(req->q, req, buffer, bufflen, GFP_KERNEL);
		if (err)
			goto free_req;
	}

	req->cmd_len = cmd_len;
	memset(req->cmd, 0, BLK_MAX_CDB); /* ATAPI hates garbage after CDB */
	memcpy(req->cmd, cmd, req->cmd_len);
	req->sense = SRpnt->sense;
	req->sense_len = 0;
	req->timeout = timeout;
	req->retries = retries;
	req->end_io_data = SRpnt;

	blk_execute_rq_nowait(req->q, NULL, req, 1, osst_end_async);
	return 0;
free_req:
	blk_put_request(req);
	return DRIVER_ERROR << 24;
}

/* Do the scsi command. Waits until command performed if do_wait is true.
   Otherwise osst_write_behind_check() is used to check that the command
   has finished. */
static	struct osst_request * osst_do_scsi(struct osst_request *SRpnt, struct osst_tape *STp, 
	unsigned char *cmd, int bytes, int direction, int timeout, int retries, int do_wait)
{
	unsigned char *bp;
	unsigned short use_sg;
#ifdef OSST_INJECT_ERRORS
	static   int   inject = 0;
	static   int   repeat = 0;
#endif
	struct completion *waiting;

	/* if async, make sure there's no command outstanding */
	if (!do_wait && ((STp->buffer)->last_SRpnt)) {
		printk(KERN_ERR "%s: Async command already active.\n",
		       tape_name(STp));
		if (signal_pending(current))
			(STp->buffer)->syscall_result = (-EINTR);
		else
			(STp->buffer)->syscall_result = (-EBUSY);
		return NULL;
	}

	if (SRpnt == NULL) {
		SRpnt = osst_allocate_request();
		if (SRpnt == NULL) {
			printk(KERN_ERR "%s: Can't allocate SCSI request.\n",
				     tape_name(STp));
			if (signal_pending(current))
				(STp->buffer)->syscall_result = (-EINTR);
			else
				(STp->buffer)->syscall_result = (-EBUSY);
			return NULL;
		}
		SRpnt->stp = STp;
	}

	/* If async IO, set last_SRpnt. This ptr tells write_behind_check
	   which IO is outstanding. It's nulled out when the IO completes. */
	if (!do_wait)
		(STp->buffer)->last_SRpnt = SRpnt;

	waiting = &STp->wait;
	init_completion(waiting);
	SRpnt->waiting = waiting;

	use_sg = (bytes > STp->buffer->sg[0].length) ? STp->buffer->use_sg : 0;
	if (use_sg) {
		bp = (char *)&(STp->buffer->sg[0]);
		if (STp->buffer->sg_segs < use_sg)
			use_sg = STp->buffer->sg_segs;
	}
	else
		bp = (STp->buffer)->b_data;

	memcpy(SRpnt->cmd, cmd, sizeof(SRpnt->cmd));
	STp->buffer->cmdstat.have_sense = 0;
	STp->buffer->syscall_result = 0;

	if (osst_execute(SRpnt, cmd, COMMAND_SIZE(cmd[0]), direction, bp, bytes,
			 use_sg, timeout, retries))
		/* could not allocate the buffer or request was too large */
		(STp->buffer)->syscall_result = (-EBUSY);
	else if (do_wait) {
		wait_for_completion(waiting);
		SRpnt->waiting = NULL;
		STp->buffer->syscall_result = osst_chk_result(STp, SRpnt);
#ifdef OSST_INJECT_ERRORS
		if (STp->buffer->syscall_result == 0 &&
		    cmd[0] == READ_6 &&
		    cmd[4] && 
		    ( (++ inject % 83) == 29  ||
		      (STp->first_frame_position == 240 
			         /* or STp->read_error_frame to fail again on the block calculated above */ &&
				 ++repeat < 3))) {
			printk(OSST_DEB_MSG "%s:D: Injecting read error\n", tape_name(STp));
			STp->buffer->last_result_fatal = 1;
		}
#endif
	}
	return SRpnt;
}


/* Handle the write-behind checking (downs the semaphore) */
static void osst_write_behind_check(struct osst_tape *STp)
{
	struct osst_buffer * STbuffer;

	STbuffer = STp->buffer;

#if DEBUG
	if (STp->write_pending)
		STp->nbr_waits++;
	else
		STp->nbr_finished++;
#endif
	wait_for_completion(&(STp->wait));
	STp->buffer->last_SRpnt->waiting = NULL;

	STp->buffer->syscall_result = osst_chk_result(STp, STp->buffer->last_SRpnt);

	if (STp->buffer->syscall_result)
		STp->buffer->syscall_result =
			osst_write_error_recovery(STp, &(STp->buffer->last_SRpnt), 1);
	else
		STp->first_frame_position++;

	osst_release_request(STp->buffer->last_SRpnt);

	if (STbuffer->writing < STbuffer->buffer_bytes)
		printk(KERN_WARNING "osst :A: write_behind_check: something left in buffer!\n");

	STbuffer->last_SRpnt = NULL;
	STbuffer->buffer_bytes -= STbuffer->writing;
	STbuffer->writing = 0;

	return;
}



/* Onstream specific Routines */
/*
 * Initialize the OnStream AUX
 */
static void osst_init_aux(struct osst_tape * STp, int frame_type, int frame_seq_number,
					 int logical_blk_num, int blk_sz, int blk_cnt)
{
	os_aux_t       *aux = STp->buffer->aux;
	os_partition_t *par = &aux->partition;
	os_dat_t       *dat = &aux->dat;

	if (STp->raw) return;

	memset(aux, 0, sizeof(*aux));
	aux->format_id = htonl(0);
	memcpy(aux->application_sig, "LIN4", 4);
	aux->hdwr = htonl(0);
	aux->frame_type = frame_type;

	switch (frame_type) {
	  case	OS_FRAME_TYPE_HEADER:
		aux->update_frame_cntr    = htonl(STp->update_frame_cntr);
		par->partition_num        = OS_CONFIG_PARTITION;
		par->par_desc_ver         = OS_PARTITION_VERSION;
		par->wrt_pass_cntr        = htons(0xffff);
		/* 0-4 = reserved, 5-9 = header, 2990-2994 = header, 2995-2999 = reserved */
		par->first_frame_ppos     = htonl(0);
		par->last_frame_ppos      = htonl(0xbb7);
		aux->frame_seq_num        = htonl(0);
		aux->logical_blk_num_high = htonl(0);
		aux->logical_blk_num      = htonl(0);
		aux->next_mark_ppos       = htonl(STp->first_mark_ppos);
		break;
	  case	OS_FRAME_TYPE_DATA:
	  case	OS_FRAME_TYPE_MARKER:
		dat->dat_sz = 8;
		dat->reserved1 = 0;
		dat->entry_cnt = 1;
		dat->reserved3 = 0;
		dat->dat_list[0].blk_sz   = htonl(blk_sz);
		dat->dat_list[0].blk_cnt  = htons(blk_cnt);
		dat->dat_list[0].flags    = frame_type==OS_FRAME_TYPE_MARKER?
							OS_DAT_FLAGS_MARK:OS_DAT_FLAGS_DATA;
		dat->dat_list[0].reserved = 0;
	  case	OS_FRAME_TYPE_EOD:
		aux->update_frame_cntr    = htonl(0);
		par->partition_num        = OS_DATA_PARTITION;
		par->par_desc_ver         = OS_PARTITION_VERSION;
		par->wrt_pass_cntr        = htons(STp->wrt_pass_cntr);
		par->first_frame_ppos     = htonl(STp->first_data_ppos);
		par->last_frame_ppos      = htonl(STp->capacity);
		aux->frame_seq_num        = htonl(frame_seq_number);
		aux->logical_blk_num_high = htonl(0);
		aux->logical_blk_num      = htonl(logical_blk_num);
		break;
	  default: ; /* probably FILL */
	}
	aux->filemark_cnt = htonl(STp->filemark_cnt);
	aux->phys_fm = htonl(0xffffffff);
	aux->last_mark_ppos = htonl(STp->last_mark_ppos);
	aux->last_mark_lbn  = htonl(STp->last_mark_lbn);
}

/*
 * Verify that we have the correct tape frame
 */
static int osst_verify_frame(struct osst_tape * STp, int frame_seq_number, int quiet)
{
	char               * name = tape_name(STp);
	os_aux_t           * aux  = STp->buffer->aux;
	os_partition_t     * par  = &(aux->partition);
	struct st_partstat * STps = &(STp->ps[STp->partition]);
	int		     blk_cnt, blk_sz, i;

	if (STp->raw) {
		if (STp->buffer->syscall_result) {
			for (i=0; i < STp->buffer->sg_segs; i++)
				memset(page_address(sg_page(&STp->buffer->sg[i])),
				       0, STp->buffer->sg[i].length);
			strcpy(STp->buffer->b_data, "READ ERROR ON FRAME");
                } else
			STp->buffer->buffer_bytes = OS_FRAME_SIZE;
		return 1;
	}
	if (STp->buffer->syscall_result) {
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Skipping frame, read error\n", name);
#endif
		return 0;
	}
	if (ntohl(aux->format_id) != 0) {
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Skipping frame, format_id %u\n", name, ntohl(aux->format_id));
#endif
		goto err_out;
	}
	if (memcmp(aux->application_sig, STp->application_sig, 4) != 0 &&
	    (memcmp(aux->application_sig, "LIN3", 4) != 0 || STp->linux_media_version != 4)) {
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Skipping frame, incorrect application signature\n", name);
#endif
		goto err_out;
	}
	if (par->partition_num != OS_DATA_PARTITION) {
		if (!STp->linux_media || STp->linux_media_version != 2) {
#if DEBUG
			printk(OSST_DEB_MSG "%s:D: Skipping frame, partition num %d\n",
					    name, par->partition_num);
#endif
			goto err_out;
		}
	}
	if (par->par_desc_ver != OS_PARTITION_VERSION) {
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Skipping frame, partition version %d\n", name, par->par_desc_ver);
#endif
		goto err_out;
	}
	if (ntohs(par->wrt_pass_cntr) != STp->wrt_pass_cntr) {
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Skipping frame, wrt_pass_cntr %d (expected %d)\n", 
				    name, ntohs(par->wrt_pass_cntr), STp->wrt_pass_cntr);
#endif
		goto err_out;
	}
	if (aux->frame_type != OS_FRAME_TYPE_DATA &&
	    aux->frame_type != OS_FRAME_TYPE_EOD &&
	    aux->frame_type != OS_FRAME_TYPE_MARKER) {
		if (!quiet) {
#if DEBUG
			printk(OSST_DEB_MSG "%s:D: Skipping frame, frame type %x\n", name, aux->frame_type);
#endif
		}
		goto err_out;
	}
	if (aux->frame_type == OS_FRAME_TYPE_EOD &&
	    STp->first_frame_position < STp->eod_frame_ppos) {
		printk(KERN_INFO "%s:I: Skipping premature EOD frame %d\n", name,
				 STp->first_frame_position);
		goto err_out;
	}
        if (frame_seq_number != -1 && ntohl(aux->frame_seq_num) != frame_seq_number) {
		if (!quiet) {
#if DEBUG
			printk(OSST_DEB_MSG "%s:D: Skipping frame, sequence number %u (expected %d)\n", 
					    name, ntohl(aux->frame_seq_num), frame_seq_number);
#endif
		}
		goto err_out;
	}
	if (aux->frame_type == OS_FRAME_TYPE_MARKER) {
		STps->eof = ST_FM_HIT;

		i = ntohl(aux->filemark_cnt);
		if (STp->header_cache != NULL && i < OS_FM_TAB_MAX && (i > STp->filemark_cnt ||
		    STp->first_frame_position - 1 != ntohl(STp->header_cache->dat_fm_tab.fm_tab_ent[i]))) {
#if DEBUG
			printk(OSST_DEB_MSG "%s:D: %s filemark %d at frame pos %d\n", name,
				  STp->header_cache->dat_fm_tab.fm_tab_ent[i] == 0?"Learned":"Corrected",
				  i, STp->first_frame_position - 1);
#endif
			STp->header_cache->dat_fm_tab.fm_tab_ent[i] = htonl(STp->first_frame_position - 1);
			if (i >= STp->filemark_cnt)
				 STp->filemark_cnt = i+1;
		}
	}
	if (aux->frame_type == OS_FRAME_TYPE_EOD) {
		STps->eof = ST_EOD_1;
		STp->frame_in_buffer = 1;
	}
	if (aux->frame_type == OS_FRAME_TYPE_DATA) {
                blk_cnt = ntohs(aux->dat.dat_list[0].blk_cnt);
		blk_sz  = ntohl(aux->dat.dat_list[0].blk_sz);
		STp->buffer->buffer_bytes = blk_cnt * blk_sz;
		STp->buffer->read_pointer = 0;
		STp->frame_in_buffer = 1;

		/* See what block size was used to write file */
		if (STp->block_size != blk_sz && blk_sz > 0) {
			printk(KERN_INFO
	    	"%s:I: File was written with block size %d%c, currently %d%c, adjusted to match.\n",
       				name, blk_sz<1024?blk_sz:blk_sz/1024,blk_sz<1024?'b':'k',
				STp->block_size<1024?STp->block_size:STp->block_size/1024,
				STp->block_size<1024?'b':'k');
			STp->block_size            = blk_sz;
			STp->buffer->buffer_blocks = OS_DATA_SIZE / blk_sz;
		}
		STps->eof = ST_NOEOF;
	}
        STp->frame_seq_number = ntohl(aux->frame_seq_num);
	STp->logical_blk_num  = ntohl(aux->logical_blk_num);
	return 1;

err_out:
	if (STp->read_error_frame == 0)
		STp->read_error_frame = STp->first_frame_position - 1;
	return 0;
}

/*
 * Wait for the unit to become Ready
 */
static int osst_wait_ready(struct osst_tape * STp, struct osst_request ** aSRpnt,
				 unsigned timeout, int initial_delay)
{
	unsigned char		cmd[MAX_COMMAND_SIZE];
	struct osst_request   * SRpnt;
	unsigned long		startwait = jiffies;
#if DEBUG
	int			dbg  = debugging;
	char    	      * name = tape_name(STp);

	printk(OSST_DEB_MSG "%s:D: Reached onstream wait ready\n", name);
#endif

	if (initial_delay > 0)
		msleep(jiffies_to_msecs(initial_delay));

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = TEST_UNIT_READY;

	SRpnt = osst_do_scsi(*aSRpnt, STp, cmd, 0, DMA_NONE, STp->timeout, MAX_RETRIES, 1);
	*aSRpnt = SRpnt;
	if (!SRpnt) return (-EBUSY);

	while ( STp->buffer->syscall_result && time_before(jiffies, startwait + timeout*HZ) &&
	       (( SRpnt->sense[2]  == 2 && SRpnt->sense[12] == 4    &&
		 (SRpnt->sense[13] == 1 || SRpnt->sense[13] == 8)    ) ||
		( SRpnt->sense[2]  == 6 && SRpnt->sense[12] == 0x28 &&
		  SRpnt->sense[13] == 0                                        )  )) {
#if DEBUG
	    if (debugging) {
		printk(OSST_DEB_MSG "%s:D: Sleeping in onstream wait ready\n", name);
		printk(OSST_DEB_MSG "%s:D: Turning off debugging for a while\n", name);
		debugging = 0;
	    }
#endif
	    msleep(100);

	    memset(cmd, 0, MAX_COMMAND_SIZE);
	    cmd[0] = TEST_UNIT_READY;

	    SRpnt = osst_do_scsi(SRpnt, STp, cmd, 0, DMA_NONE, STp->timeout, MAX_RETRIES, 1);
	}
	*aSRpnt = SRpnt;
#if DEBUG
	debugging = dbg;
#endif
	if ( STp->buffer->syscall_result &&
	     osst_write_error_recovery(STp, aSRpnt, 0) ) {
#if DEBUG
	    printk(OSST_DEB_MSG "%s:D: Abnormal exit from onstream wait ready\n", name);
	    printk(OSST_DEB_MSG "%s:D: Result = %d, Sense: 0=%02x, 2=%02x, 12=%02x, 13=%02x\n", name,
			STp->buffer->syscall_result, SRpnt->sense[0], SRpnt->sense[2],
			SRpnt->sense[12], SRpnt->sense[13]);
#endif
	    return (-EIO);
	}
#if DEBUG
	printk(OSST_DEB_MSG "%s:D: Normal exit from onstream wait ready\n", name);
#endif
	return 0;
}

/*
 * Wait for a tape to be inserted in the unit
 */
static int osst_wait_for_medium(struct osst_tape * STp, struct osst_request ** aSRpnt, unsigned timeout)
{
	unsigned char		cmd[MAX_COMMAND_SIZE];
	struct osst_request   * SRpnt;
	unsigned long		startwait = jiffies;
#if DEBUG
	int			dbg = debugging;
	char    	      * name = tape_name(STp);

	printk(OSST_DEB_MSG "%s:D: Reached onstream wait for medium\n", name);
#endif

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = TEST_UNIT_READY;

	SRpnt = osst_do_scsi(*aSRpnt, STp, cmd, 0, DMA_NONE, STp->timeout, MAX_RETRIES, 1);
	*aSRpnt = SRpnt;
	if (!SRpnt) return (-EBUSY);

	while ( STp->buffer->syscall_result && time_before(jiffies, startwait + timeout*HZ) &&
		SRpnt->sense[2] == 2 && SRpnt->sense[12] == 0x3a && SRpnt->sense[13] == 0  ) {
#if DEBUG
	    if (debugging) {
		printk(OSST_DEB_MSG "%s:D: Sleeping in onstream wait medium\n", name);
		printk(OSST_DEB_MSG "%s:D: Turning off debugging for a while\n", name);
		debugging = 0;
	    }
#endif
	    msleep(100);

	    memset(cmd, 0, MAX_COMMAND_SIZE);
	    cmd[0] = TEST_UNIT_READY;

	    SRpnt = osst_do_scsi(SRpnt, STp, cmd, 0, DMA_NONE, STp->timeout, MAX_RETRIES, 1);
	}
	*aSRpnt = SRpnt;
#if DEBUG
	debugging = dbg;
#endif
	if ( STp->buffer->syscall_result     && SRpnt->sense[2]  != 2 &&
	     SRpnt->sense[12] != 4 && SRpnt->sense[13] == 1) {
#if DEBUG
	    printk(OSST_DEB_MSG "%s:D: Abnormal exit from onstream wait medium\n", name);
	    printk(OSST_DEB_MSG "%s:D: Result = %d, Sense: 0=%02x, 2=%02x, 12=%02x, 13=%02x\n", name,
			STp->buffer->syscall_result, SRpnt->sense[0], SRpnt->sense[2],
			SRpnt->sense[12], SRpnt->sense[13]);
#endif
	    return 0;
	}
#if DEBUG
	printk(OSST_DEB_MSG "%s:D: Normal exit from onstream wait medium\n", name);
#endif
	return 1;
}

static int osst_position_tape_and_confirm(struct osst_tape * STp, struct osst_request ** aSRpnt, int frame)
{
	int	retval;

	osst_wait_ready(STp, aSRpnt, 15 * 60, 0);			/* TODO - can this catch a write error? */
	retval = osst_set_frame_position(STp, aSRpnt, frame, 0);
	if (retval) return (retval);
	osst_wait_ready(STp, aSRpnt, 15 * 60, OSST_WAIT_POSITION_COMPLETE);
	return (osst_get_frame_position(STp, aSRpnt));
}

/*
 * Wait for write(s) to complete
 */
static int osst_flush_drive_buffer(struct osst_tape * STp, struct osst_request ** aSRpnt)
{
	unsigned char		cmd[MAX_COMMAND_SIZE];
	struct osst_request   * SRpnt;
	int			result = 0;
	int			delay  = OSST_WAIT_WRITE_COMPLETE;
#if DEBUG
	char		      * name = tape_name(STp);

	printk(OSST_DEB_MSG "%s:D: Reached onstream flush drive buffer (write filemark)\n", name);
#endif

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = WRITE_FILEMARKS;
	cmd[1] = 1;

	SRpnt = osst_do_scsi(*aSRpnt, STp, cmd, 0, DMA_NONE, STp->timeout, MAX_RETRIES, 1);
	*aSRpnt = SRpnt;
	if (!SRpnt) return (-EBUSY);
	if (STp->buffer->syscall_result) {
		if ((SRpnt->sense[2] & 0x0f) == 2 && SRpnt->sense[12] == 4) {
			if (SRpnt->sense[13] == 8) {
				delay = OSST_WAIT_LONG_WRITE_COMPLETE;
			}
		} else
			result = osst_write_error_recovery(STp, aSRpnt, 0);
	}
	result |= osst_wait_ready(STp, aSRpnt, 5 * 60, delay);
	STp->ps[STp->partition].rw = OS_WRITING_COMPLETE;

	return (result);
}

#define OSST_POLL_PER_SEC 10
static int osst_wait_frame(struct osst_tape * STp, struct osst_request ** aSRpnt, int curr, int minlast, int to)
{
	unsigned long	startwait = jiffies;
	char	      * name      = tape_name(STp);
#if DEBUG
	char	   notyetprinted  = 1;
#endif
	if (minlast >= 0 && STp->ps[STp->partition].rw != ST_READING)
		printk(KERN_ERR "%s:A: Waiting for frame without having initialized read!\n", name);

	while (time_before (jiffies, startwait + to*HZ))
	{ 
		int result;
		result = osst_get_frame_position(STp, aSRpnt);
		if (result == -EIO)
			if ((result = osst_write_error_recovery(STp, aSRpnt, 0)) == 0)
				return 0;	/* successful recovery leaves drive ready for frame */
		if (result < 0) break;
		if (STp->first_frame_position == curr &&
		    ((minlast < 0 &&
		      (signed)STp->last_frame_position > (signed)curr + minlast) ||
		     (minlast >= 0 && STp->cur_frames > minlast)
		    ) && result >= 0)
		{
#if DEBUG			
			if (debugging || time_after_eq(jiffies, startwait + 2*HZ/OSST_POLL_PER_SEC))
				printk (OSST_DEB_MSG
					"%s:D: Succ wait f fr %i (>%i): %i-%i %i (%i): %3li.%li s\n",
					name, curr, curr+minlast, STp->first_frame_position,
					STp->last_frame_position, STp->cur_frames,
					result, (jiffies-startwait)/HZ, 
					(((jiffies-startwait)%HZ)*10)/HZ);
#endif
			return 0;
		}
#if DEBUG
		if (time_after_eq(jiffies, startwait + 2*HZ/OSST_POLL_PER_SEC) && notyetprinted)
		{
			printk (OSST_DEB_MSG "%s:D: Wait for frame %i (>%i): %i-%i %i (%i)\n",
				name, curr, curr+minlast, STp->first_frame_position,
				STp->last_frame_position, STp->cur_frames, result);
			notyetprinted--;
		}
#endif
		msleep(1000 / OSST_POLL_PER_SEC);
	}
#if DEBUG
	printk (OSST_DEB_MSG "%s:D: Fail wait f fr %i (>%i): %i-%i %i: %3li.%li s\n",
		name, curr, curr+minlast, STp->first_frame_position,
		STp->last_frame_position, STp->cur_frames,
		(jiffies-startwait)/HZ, (((jiffies-startwait)%HZ)*10)/HZ);
#endif	
	return -EBUSY;
}

static int osst_recover_wait_frame(struct osst_tape * STp, struct osst_request ** aSRpnt, int writing)
{
	struct osst_request   * SRpnt;
	unsigned char		cmd[MAX_COMMAND_SIZE];
	unsigned long   	startwait = jiffies;
	int			retval    = 1;
        char		      * name      = tape_name(STp);
                                                                                                                                
	if (writing) {
		char	mybuf[24];
		char  * olddata = STp->buffer->b_data;
		int	oldsize = STp->buffer->buffer_size;

		/* write zero fm then read pos - if shows write error, try to recover - if no progress, wait */

		memset(cmd, 0, MAX_COMMAND_SIZE);
		cmd[0] = WRITE_FILEMARKS;
		cmd[1] = 1;
		SRpnt = osst_do_scsi(*aSRpnt, STp, cmd, 0, DMA_NONE, STp->timeout,
								MAX_RETRIES, 1);

		while (retval && time_before (jiffies, startwait + 5*60*HZ)) {

			if (STp->buffer->syscall_result && (SRpnt->sense[2] & 0x0f) != 2) {

				/* some failure - not just not-ready */
				retval = osst_write_error_recovery(STp, aSRpnt, 0);
				break;
			}
			schedule_timeout_interruptible(HZ / OSST_POLL_PER_SEC);

			STp->buffer->b_data = mybuf; STp->buffer->buffer_size = 24;
			memset(cmd, 0, MAX_COMMAND_SIZE);
			cmd[0] = READ_POSITION;

			SRpnt = osst_do_scsi(SRpnt, STp, cmd, 20, DMA_FROM_DEVICE, STp->timeout,
										MAX_RETRIES, 1);

			retval = ( STp->buffer->syscall_result || (STp->buffer)->b_data[15] > 25 );
			STp->buffer->b_data = olddata; STp->buffer->buffer_size = oldsize;
		}
		if (retval)
			printk(KERN_ERR "%s:E: Device did not succeed to write buffered data\n", name);
	} else
		/* TODO - figure out which error conditions can be handled */
		if (STp->buffer->syscall_result)
			printk(KERN_WARNING
				"%s:W: Recover_wait_frame(read) cannot handle %02x:%02x:%02x\n", name,
					(*aSRpnt)->sense[ 2] & 0x0f,
					(*aSRpnt)->sense[12],
					(*aSRpnt)->sense[13]);

	return retval;
}

/*
 * Read the next OnStream tape frame at the current location
 */
static int osst_read_frame(struct osst_tape * STp, struct osst_request ** aSRpnt, int timeout)
{
	unsigned char		cmd[MAX_COMMAND_SIZE];
	struct osst_request   * SRpnt;
	int			retval = 0;
#if DEBUG
	os_aux_t	      * aux    = STp->buffer->aux;
	char		      * name   = tape_name(STp);
#endif

	if (STp->poll)
		if (osst_wait_frame (STp, aSRpnt, STp->first_frame_position, 0, timeout))
			retval = osst_recover_wait_frame(STp, aSRpnt, 0);

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = READ_6;
	cmd[1] = 1;
	cmd[4] = 1;

#if DEBUG
	if (debugging)
		printk(OSST_DEB_MSG "%s:D: Reading frame from OnStream tape\n", name);
#endif
	SRpnt = osst_do_scsi(*aSRpnt, STp, cmd, OS_FRAME_SIZE, DMA_FROM_DEVICE,
				      STp->timeout, MAX_RETRIES, 1);
	*aSRpnt = SRpnt;
	if (!SRpnt)
		return (-EBUSY);

	if ((STp->buffer)->syscall_result) {
	    retval = 1;
	    if (STp->read_error_frame == 0) {
		STp->read_error_frame = STp->first_frame_position;
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Recording read error at %d\n", name, STp->read_error_frame);
#endif
	    }
#if DEBUG
	    if (debugging)
		printk(OSST_DEB_MSG "%s:D: Sense: %2x %2x %2x %2x %2x %2x %2x %2x\n",
		   name,
		   SRpnt->sense[0], SRpnt->sense[1],
		   SRpnt->sense[2], SRpnt->sense[3],
		   SRpnt->sense[4], SRpnt->sense[5],
		   SRpnt->sense[6], SRpnt->sense[7]);
#endif
	}
	else
	    STp->first_frame_position++;
#if DEBUG
	if (debugging) {
	   char sig[8]; int i;
	   for (i=0;i<4;i++)
		   sig[i] = aux->application_sig[i]<32?'^':aux->application_sig[i];
	   sig[4] = '\0';
	   printk(OSST_DEB_MSG 
		"%s:D: AUX: %s UpdFrCt#%d Wpass#%d %s FrSeq#%d LogBlk#%d Qty=%d Sz=%d\n", name, sig,
			ntohl(aux->update_frame_cntr), ntohs(aux->partition.wrt_pass_cntr),
			aux->frame_type==1?"EOD":aux->frame_type==2?"MARK":
			aux->frame_type==8?"HEADR":aux->frame_type==0x80?"DATA":"FILL", 
			ntohl(aux->frame_seq_num), ntohl(aux->logical_blk_num),
			ntohs(aux->dat.dat_list[0].blk_cnt), ntohl(aux->dat.dat_list[0].blk_sz) );
	   if (aux->frame_type==2)
		printk(OSST_DEB_MSG "%s:D: mark_cnt=%d, last_mark_ppos=%d, last_mark_lbn=%d\n", name,
			ntohl(aux->filemark_cnt), ntohl(aux->last_mark_ppos), ntohl(aux->last_mark_lbn));
	   printk(OSST_DEB_MSG "%s:D: Exit read frame from OnStream tape with code %d\n", name, retval);
	}
#endif
	return (retval);
}

static int osst_initiate_read(struct osst_tape * STp, struct osst_request ** aSRpnt)
{
	struct st_partstat    * STps   = &(STp->ps[STp->partition]);
	struct osst_request   * SRpnt  ;
	unsigned char		cmd[MAX_COMMAND_SIZE];
	int			retval = 0;
	char		      * name   = tape_name(STp);

	if (STps->rw != ST_READING) {         /* Initialize read operation */
		if (STps->rw == ST_WRITING || STp->dirty) {
			STp->write_type = OS_WRITE_DATA;
                        osst_flush_write_buffer(STp, aSRpnt);
			osst_flush_drive_buffer(STp, aSRpnt);
		}
		STps->rw = ST_READING;
		STp->frame_in_buffer = 0;

		/*
		 *      Issue a read 0 command to get the OnStream drive
                 *      read frames into its buffer.
		 */
		memset(cmd, 0, MAX_COMMAND_SIZE);
		cmd[0] = READ_6;
		cmd[1] = 1;

#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Start Read Ahead on OnStream tape\n", name);
#endif
		SRpnt   = osst_do_scsi(*aSRpnt, STp, cmd, 0, DMA_NONE, STp->timeout, MAX_RETRIES, 1);
		*aSRpnt = SRpnt;
		if ((retval = STp->buffer->syscall_result))
			printk(KERN_WARNING "%s:W: Error starting read ahead\n", name);
	}

	return retval;
}

static int osst_get_logical_frame(struct osst_tape * STp, struct osst_request ** aSRpnt,
						int frame_seq_number, int quiet)
{
	struct st_partstat * STps  = &(STp->ps[STp->partition]);
	char		   * name  = tape_name(STp);
	int		     cnt   = 0,
			     bad   = 0,
			     past  = 0,
			     x,
			     position;

	/*
	 * If we want just any frame (-1) and there is a frame in the buffer, return it
	 */
	if (frame_seq_number == -1 && STp->frame_in_buffer) {
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Frame %d still in buffer\n", name, STp->frame_seq_number);
#endif
		return (STps->eof);
	}
	/*
         * Search and wait for the next logical tape frame
	 */
	while (1) {
		if (cnt++ > 400) {
                        printk(KERN_ERR "%s:E: Couldn't find logical frame %d, aborting\n",
					    name, frame_seq_number);
			if (STp->read_error_frame) {
				osst_set_frame_position(STp, aSRpnt, STp->read_error_frame, 0);
#if DEBUG
                        	printk(OSST_DEB_MSG "%s:D: Repositioning tape to bad frame %d\n",
						    name, STp->read_error_frame);
#endif
				STp->read_error_frame = 0;
				STp->abort_count++;
			}
			return (-EIO);
		}
#if DEBUG
		if (debugging)
			printk(OSST_DEB_MSG "%s:D: Looking for frame %d, attempt %d\n",
					  name, frame_seq_number, cnt);
#endif
		if ( osst_initiate_read(STp, aSRpnt)
                || ( (!STp->frame_in_buffer) && osst_read_frame(STp, aSRpnt, 30) ) ) {
			if (STp->raw)
				return (-EIO);
			position = osst_get_frame_position(STp, aSRpnt);
			if (position >= 0xbae && position < 0xbb8)
				position = 0xbb8;
			else if (position > STp->eod_frame_ppos || ++bad == 10) {
				position = STp->read_error_frame - 1;
				bad = 0;
			}
			else {
				position += 29;
				cnt      += 19;
			}
#if DEBUG
			printk(OSST_DEB_MSG "%s:D: Bad frame detected, positioning tape to block %d\n",
					 name, position);
#endif
			osst_set_frame_position(STp, aSRpnt, position, 0);
			continue;
		}
		if (osst_verify_frame(STp, frame_seq_number, quiet))
			break;
		if (osst_verify_frame(STp, -1, quiet)) {
			x = ntohl(STp->buffer->aux->frame_seq_num);
			if (STp->fast_open) {
				printk(KERN_WARNING
				       "%s:W: Found logical frame %d instead of %d after fast open\n",
				       name, x, frame_seq_number);
				STp->header_ok = 0;
				STp->read_error_frame = 0;
				return (-EIO);
			}
			if (x > frame_seq_number) {
				if (++past > 3) {
					/* positioning backwards did not bring us to the desired frame */
					position = STp->read_error_frame - 1;
				}
				else {
			        	position = osst_get_frame_position(STp, aSRpnt)
					         + frame_seq_number - x - 1;

					if (STp->first_frame_position >= 3000 && position < 3000)
						position -= 10;
				}
#if DEBUG
                                printk(OSST_DEB_MSG
				       "%s:D: Found logical frame %d while looking for %d: back up %d\n",
						name, x, frame_seq_number,
					       	STp->first_frame_position - position);
#endif
                        	osst_set_frame_position(STp, aSRpnt, position, 0);
				cnt += 10;
			}
			else
				past = 0;
		}
		if (osst_get_frame_position(STp, aSRpnt) == 0xbaf) {
#if DEBUG
			printk(OSST_DEB_MSG "%s:D: Skipping config partition\n", name);
#endif
			osst_set_frame_position(STp, aSRpnt, 0xbb8, 0);
			cnt--;
		}
		STp->frame_in_buffer = 0;
	}
	if (cnt > 1) {
		STp->recover_count++;
		STp->recover_erreg++;
		printk(KERN_WARNING "%s:I: Don't worry, Read error at position %d recovered\n", 
					name, STp->read_error_frame);
 	}
	STp->read_count++;

#if DEBUG
	if (debugging || STps->eof)
		printk(OSST_DEB_MSG
			"%s:D: Exit get logical frame (%d=>%d) from OnStream tape with code %d\n",
			name, frame_seq_number, STp->frame_seq_number, STps->eof);
#endif
	STp->fast_open = 0;
	STp->read_error_frame = 0;
	return (STps->eof);
}

static int osst_seek_logical_blk(struct osst_tape * STp, struct osst_request ** aSRpnt, int logical_blk_num)
{
        struct st_partstat * STps = &(STp->ps[STp->partition]);
	char		   * name = tape_name(STp);
	int	retries    = 0;
	int	frame_seq_estimate, ppos_estimate, move;
	
	if (logical_blk_num < 0) logical_blk_num = 0;
#if DEBUG
	printk(OSST_DEB_MSG "%s:D: Seeking logical block %d (now at %d, size %d%c)\n",
				name, logical_blk_num, STp->logical_blk_num, 
				STp->block_size<1024?STp->block_size:STp->block_size/1024,
				STp->block_size<1024?'b':'k');
#endif
	/* Do we know where we are? */
	if (STps->drv_block >= 0) {
		move                = logical_blk_num - STp->logical_blk_num;
		if (move < 0) move -= (OS_DATA_SIZE / STp->block_size) - 1;
		move               /= (OS_DATA_SIZE / STp->block_size);
		frame_seq_estimate  = STp->frame_seq_number + move;
	} else
		frame_seq_estimate  = logical_blk_num * STp->block_size / OS_DATA_SIZE;

	if (frame_seq_estimate < 2980) ppos_estimate = frame_seq_estimate + 10;
	else			       ppos_estimate = frame_seq_estimate + 20;
	while (++retries < 10) {
	   if (ppos_estimate > STp->eod_frame_ppos-2) {
	       frame_seq_estimate += STp->eod_frame_ppos - 2 - ppos_estimate;
	       ppos_estimate       = STp->eod_frame_ppos - 2;
	   }
	   if (frame_seq_estimate < 0) {
	       frame_seq_estimate = 0;
	       ppos_estimate      = 10;
	   }
	   osst_set_frame_position(STp, aSRpnt, ppos_estimate, 0);
	   if (osst_get_logical_frame(STp, aSRpnt, frame_seq_estimate, 1) >= 0) {
	      /* we've located the estimated frame, now does it have our block? */
	      if (logical_blk_num <  STp->logical_blk_num ||
	          logical_blk_num >= STp->logical_blk_num + ntohs(STp->buffer->aux->dat.dat_list[0].blk_cnt)) {
		 if (STps->eof == ST_FM_HIT)
		    move = logical_blk_num < STp->logical_blk_num? -2 : 1;
		 else {
		    move                = logical_blk_num - STp->logical_blk_num;
		    if (move < 0) move -= (OS_DATA_SIZE / STp->block_size) - 1;
		    move               /= (OS_DATA_SIZE / STp->block_size);
		 }
		 if (!move) move = logical_blk_num > STp->logical_blk_num ? 1 : -1;
#if DEBUG
		 printk(OSST_DEB_MSG
			"%s:D: Seek retry %d at ppos %d fsq %d (est %d) lbn %d (need %d) move %d\n",
				name, retries, ppos_estimate, STp->frame_seq_number, frame_seq_estimate, 
				STp->logical_blk_num, logical_blk_num, move);
#endif
		 frame_seq_estimate += move;
		 ppos_estimate      += move;
		 continue;
	      } else {
		 STp->buffer->read_pointer  = (logical_blk_num - STp->logical_blk_num) * STp->block_size;
		 STp->buffer->buffer_bytes -= STp->buffer->read_pointer;
		 STp->logical_blk_num       =  logical_blk_num;
#if DEBUG
		 printk(OSST_DEB_MSG 
			"%s:D: Seek success at ppos %d fsq %d in_buf %d, bytes %d, ptr %d*%d\n",
				name, ppos_estimate, STp->frame_seq_number, STp->frame_in_buffer, 
				STp->buffer->buffer_bytes, STp->buffer->read_pointer / STp->block_size, 
				STp->block_size);
#endif
		 STps->drv_file = ntohl(STp->buffer->aux->filemark_cnt);
		 if (STps->eof == ST_FM_HIT) {
		     STps->drv_file++;
		     STps->drv_block = 0;
		 } else {
		     STps->drv_block = ntohl(STp->buffer->aux->last_mark_lbn)?
					  STp->logical_blk_num -
					     (STps->drv_file ? ntohl(STp->buffer->aux->last_mark_lbn) + 1 : 0):
					-1;
		 }
		 STps->eof = (STp->first_frame_position >= STp->eod_frame_ppos)?ST_EOD:ST_NOEOF;
		 return 0;
	      }
	   }
	   if (osst_get_logical_frame(STp, aSRpnt, -1, 1) < 0)
	      goto error;
	   /* we are not yet at the estimated frame, adjust our estimate of its physical position */
#if DEBUG
	   printk(OSST_DEB_MSG "%s:D: Seek retry %d at ppos %d fsq %d (est %d) lbn %d (need %d)\n", 
			   name, retries, ppos_estimate, STp->frame_seq_number, frame_seq_estimate, 
			   STp->logical_blk_num, logical_blk_num);
#endif
	   if (frame_seq_estimate != STp->frame_seq_number)
	      ppos_estimate += frame_seq_estimate - STp->frame_seq_number;
	   else
	      break;
	}
error:
	printk(KERN_ERR "%s:E: Couldn't seek to logical block %d (at %d), %d retries\n", 
			    name, logical_blk_num, STp->logical_blk_num, retries);
	return (-EIO);
}

/* The values below are based on the OnStream frame payload size of 32K == 2**15,
 * that is, OSST_FRAME_SHIFT + OSST_SECTOR_SHIFT must be 15. With a minimum block
 * size of 512 bytes, we need to be able to resolve 32K/512 == 64 == 2**6 positions
 * inside each frame. Finaly, OSST_SECTOR_MASK == 2**OSST_FRAME_SHIFT - 1.
 */
#define OSST_FRAME_SHIFT  6
#define OSST_SECTOR_SHIFT 9
#define OSST_SECTOR_MASK  0x03F

static int osst_get_sector(struct osst_tape * STp, struct osst_request ** aSRpnt)
{
	int	sector;
#if DEBUG
	char  * name = tape_name(STp);
	
	printk(OSST_DEB_MSG 
		"%s:D: Positioned at ppos %d, frame %d, lbn %d, file %d, blk %d, %cptr %d, eof %d\n",
		name, STp->first_frame_position, STp->frame_seq_number, STp->logical_blk_num,
		STp->ps[STp->partition].drv_file, STp->ps[STp->partition].drv_block, 
		STp->ps[STp->partition].rw == ST_WRITING?'w':'r',
		STp->ps[STp->partition].rw == ST_WRITING?STp->buffer->buffer_bytes:
		STp->buffer->read_pointer, STp->ps[STp->partition].eof);
#endif
	/* do we know where we are inside a file? */
	if (STp->ps[STp->partition].drv_block >= 0) {
		sector = (STp->frame_in_buffer ? STp->first_frame_position-1 :
				STp->first_frame_position) << OSST_FRAME_SHIFT;
		if (STp->ps[STp->partition].rw == ST_WRITING)
		       	sector |= (STp->buffer->buffer_bytes >> OSST_SECTOR_SHIFT) & OSST_SECTOR_MASK;
		else
	       		sector |= (STp->buffer->read_pointer >> OSST_SECTOR_SHIFT) & OSST_SECTOR_MASK;
	} else {
		sector = osst_get_frame_position(STp, aSRpnt);
		if (sector > 0)
			sector <<= OSST_FRAME_SHIFT;
	}
	return sector;
}

static int osst_seek_sector(struct osst_tape * STp, struct osst_request ** aSRpnt, int sector)
{
        struct st_partstat * STps   = &(STp->ps[STp->partition]);
	int		     frame  = sector >> OSST_FRAME_SHIFT,
			     offset = (sector & OSST_SECTOR_MASK) << OSST_SECTOR_SHIFT, 
			     r;
#if DEBUG
	char          * name = tape_name(STp);

	printk(OSST_DEB_MSG "%s:D: Seeking sector %d in frame %d at offset %d\n",
				name, sector, frame, offset);
#endif
	if (frame < 0 || frame >= STp->capacity) return (-ENXIO);

	if (frame <= STp->first_data_ppos) {
		STp->frame_seq_number = STp->logical_blk_num = STps->drv_file = STps->drv_block = 0;
		return (osst_set_frame_position(STp, aSRpnt, frame, 0));
	}
	r = osst_set_frame_position(STp, aSRpnt, offset?frame:frame-1, 0);
	if (r < 0) return r;

	r = osst_get_logical_frame(STp, aSRpnt, -1, 1);
	if (r < 0) return r;

	if (osst_get_frame_position(STp, aSRpnt) != (offset?frame+1:frame)) return (-EIO);

	if (offset) {
		STp->logical_blk_num      += offset / STp->block_size;
		STp->buffer->read_pointer  = offset;
		STp->buffer->buffer_bytes -= offset;
	} else {
		STp->frame_seq_number++;
		STp->frame_in_buffer       = 0;
		STp->logical_blk_num      += ntohs(STp->buffer->aux->dat.dat_list[0].blk_cnt);
		STp->buffer->buffer_bytes  = STp->buffer->read_pointer = 0;
	}
	STps->drv_file = ntohl(STp->buffer->aux->filemark_cnt);
	if (STps->eof == ST_FM_HIT) {
		STps->drv_file++;
		STps->drv_block = 0;
	} else {
		STps->drv_block = ntohl(STp->buffer->aux->last_mark_lbn)?
				    STp->logical_blk_num -
					(STps->drv_file ? ntohl(STp->buffer->aux->last_mark_lbn) + 1 : 0):
				  -1;
	}
	STps->eof       = (STp->first_frame_position >= STp->eod_frame_ppos)?ST_EOD:ST_NOEOF;
#if DEBUG
	printk(OSST_DEB_MSG 
		"%s:D: Now positioned at ppos %d, frame %d, lbn %d, file %d, blk %d, rptr %d, eof %d\n",
		name, STp->first_frame_position, STp->frame_seq_number, STp->logical_blk_num,
		STps->drv_file, STps->drv_block, STp->buffer->read_pointer, STps->eof);
#endif
	return 0;
}

/*
 * Read back the drive's internal buffer contents, as a part
 * of the write error recovery mechanism for old OnStream
 * firmware revisions.
 * Precondition for this function to work: all frames in the
 * drive's buffer must be of one type (DATA, MARK or EOD)!
 */
static int osst_read_back_buffer_and_rewrite(struct osst_tape * STp, struct osst_request ** aSRpnt,
						unsigned int frame, unsigned int skip, int pending)
{
	struct osst_request   * SRpnt = * aSRpnt;
	unsigned char	      * buffer, * p;
	unsigned char		cmd[MAX_COMMAND_SIZE];
	int			flag, new_frame, i;
	int			nframes          = STp->cur_frames;
	int			blks_per_frame   = ntohs(STp->buffer->aux->dat.dat_list[0].blk_cnt);
	int			frame_seq_number = ntohl(STp->buffer->aux->frame_seq_num)
						- (nframes + pending - 1);
	int			logical_blk_num  = ntohl(STp->buffer->aux->logical_blk_num) 
						- (nframes + pending - 1) * blks_per_frame;
	char		      * name             = tape_name(STp);
	unsigned long		startwait        = jiffies;
#if DEBUG
	int			dbg              = debugging;
#endif

	if ((buffer = (unsigned char *)vmalloc((nframes + 1) * OS_DATA_SIZE)) == NULL)
		return (-EIO);

	printk(KERN_INFO "%s:I: Reading back %d frames from drive buffer%s\n",
			 name, nframes, pending?" and one that was pending":"");

	osst_copy_from_buffer(STp->buffer, (p = &buffer[nframes * OS_DATA_SIZE]));
#if DEBUG
	if (pending && debugging)
		printk(OSST_DEB_MSG "%s:D: Pending frame %d (lblk %d), data %02x %02x %02x %02x\n",
				name, frame_seq_number + nframes,
			       	logical_blk_num + nframes * blks_per_frame,
			       	p[0], p[1], p[2], p[3]);
#endif
	for (i = 0, p = buffer; i < nframes; i++, p += OS_DATA_SIZE) {

		memset(cmd, 0, MAX_COMMAND_SIZE);
		cmd[0] = 0x3C;		/* Buffer Read           */
		cmd[1] = 6;		/* Retrieve Faulty Block */
		cmd[7] = 32768 >> 8;
		cmd[8] = 32768 & 0xff;

		SRpnt = osst_do_scsi(SRpnt, STp, cmd, OS_FRAME_SIZE, DMA_FROM_DEVICE,
					    STp->timeout, MAX_RETRIES, 1);
	
		if ((STp->buffer)->syscall_result || !SRpnt) {
			printk(KERN_ERR "%s:E: Failed to read frame back from OnStream buffer\n", name);
			vfree(buffer);
			*aSRpnt = SRpnt;
			return (-EIO);
		}
		osst_copy_from_buffer(STp->buffer, p);
#if DEBUG
		if (debugging)
			printk(OSST_DEB_MSG "%s:D: Read back logical frame %d, data %02x %02x %02x %02x\n",
					  name, frame_seq_number + i, p[0], p[1], p[2], p[3]);
#endif
	}
	*aSRpnt = SRpnt;
	osst_get_frame_position(STp, aSRpnt);

#if DEBUG
	printk(OSST_DEB_MSG "%s:D: Frames left in buffer: %d\n", name, STp->cur_frames);
#endif
	/* Write synchronously so we can be sure we're OK again and don't have to recover recursively */
	/* In the header we don't actually re-write the frames that fail, just the ones after them */

	for (flag=1, new_frame=frame, p=buffer, i=0; i < nframes + pending; ) {

		if (flag) {
			if (STp->write_type == OS_WRITE_HEADER) {
				i += skip;
				p += skip * OS_DATA_SIZE;
			}
			else if (new_frame < 2990 && new_frame+skip+nframes+pending >= 2990)
				new_frame = 3000-i;
			else
				new_frame += skip;
#if DEBUG
			printk(OSST_DEB_MSG "%s:D: Position to frame %d, write fseq %d\n",
						name, new_frame+i, frame_seq_number+i);
#endif
			osst_set_frame_position(STp, aSRpnt, new_frame + i, 0);
			osst_wait_ready(STp, aSRpnt, 60, OSST_WAIT_POSITION_COMPLETE);
			osst_get_frame_position(STp, aSRpnt);
			SRpnt = * aSRpnt;

			if (new_frame > frame + 1000) {
				printk(KERN_ERR "%s:E: Failed to find writable tape media\n", name);
				vfree(buffer);
				return (-EIO);
			}
			if ( i >= nframes + pending ) break;
			flag = 0;
		}
		osst_copy_to_buffer(STp->buffer, p);
		/*
		 * IMPORTANT: for error recovery to work, _never_ queue frames with mixed frame type!
		 */
		osst_init_aux(STp, STp->buffer->aux->frame_type, frame_seq_number+i,
			       	logical_blk_num + i*blks_per_frame,
			       	ntohl(STp->buffer->aux->dat.dat_list[0].blk_sz), blks_per_frame);
		memset(cmd, 0, MAX_COMMAND_SIZE);
		cmd[0] = WRITE_6;
		cmd[1] = 1;
		cmd[4] = 1;

#if DEBUG
		if (debugging)
			printk(OSST_DEB_MSG
				"%s:D: About to write frame %d, seq %d, lbn %d, data %02x %02x %02x %02x\n",
				name, new_frame+i, frame_seq_number+i, logical_blk_num + i*blks_per_frame,
				p[0], p[1], p[2], p[3]);
#endif
		SRpnt = osst_do_scsi(SRpnt, STp, cmd, OS_FRAME_SIZE, DMA_TO_DEVICE,
					    STp->timeout, MAX_RETRIES, 1);

		if (STp->buffer->syscall_result)
			flag = 1;
		else {
			p += OS_DATA_SIZE; i++;

			/* if we just sent the last frame, wait till all successfully written */
			if ( i == nframes + pending ) {
#if DEBUG
				printk(OSST_DEB_MSG "%s:D: Check re-write successful\n", name);
#endif
				memset(cmd, 0, MAX_COMMAND_SIZE);
				cmd[0] = WRITE_FILEMARKS;
				cmd[1] = 1;
				SRpnt = osst_do_scsi(SRpnt, STp, cmd, 0, DMA_NONE,
							    STp->timeout, MAX_RETRIES, 1);
#if DEBUG
				if (debugging) {
					printk(OSST_DEB_MSG "%s:D: Sleeping in re-write wait ready\n", name);
					printk(OSST_DEB_MSG "%s:D: Turning off debugging for a while\n", name);
					debugging = 0;
				}
#endif
				flag = STp->buffer->syscall_result;
				while ( !flag && time_before(jiffies, startwait + 60*HZ) ) {

					memset(cmd, 0, MAX_COMMAND_SIZE);
					cmd[0] = TEST_UNIT_READY;

					SRpnt = osst_do_scsi(SRpnt, STp, cmd, 0, DMA_NONE, STp->timeout,
												MAX_RETRIES, 1);

					if (SRpnt->sense[2] == 2 && SRpnt->sense[12] == 4 &&
					    (SRpnt->sense[13] == 1 || SRpnt->sense[13] == 8)) {
						/* in the process of becoming ready */
						msleep(100);
						continue;
					}
					if (STp->buffer->syscall_result)
						flag = 1;
					break;
				}
#if DEBUG
				debugging = dbg;
				printk(OSST_DEB_MSG "%s:D: Wait re-write finished\n", name);
#endif
			}
		}
		*aSRpnt = SRpnt;
		if (flag) {
			if ((SRpnt->sense[ 2] & 0x0f) == 13 &&
			     SRpnt->sense[12]         ==  0 &&
			     SRpnt->sense[13]         ==  2) {
				printk(KERN_ERR "%s:E: Volume overflow in write error recovery\n", name);
				vfree(buffer);
				return (-EIO);			/* hit end of tape = fail */
			}
			i = ((SRpnt->sense[3] << 24) |
			     (SRpnt->sense[4] << 16) |
			     (SRpnt->sense[5] <<  8) |
			      SRpnt->sense[6]        ) - new_frame;
			p = &buffer[i * OS_DATA_SIZE];
#if DEBUG
			printk(OSST_DEB_MSG "%s:D: Additional write error at %d\n", name, new_frame+i);
#endif
			osst_get_frame_position(STp, aSRpnt);
#if DEBUG
			printk(OSST_DEB_MSG "%s:D: reported frame positions: host = %d, tape = %d, buffer = %d\n",
					  name, STp->first_frame_position, STp->last_frame_position, STp->cur_frames);
#endif
		}
	}
	if (flag) {
		/* error recovery did not successfully complete */
		printk(KERN_ERR "%s:D: Write error recovery failed in %s\n", name,
				STp->write_type == OS_WRITE_HEADER?"header":"body");
	}
	if (!pending)
		osst_copy_to_buffer(STp->buffer, p);	/* so buffer content == at entry in all cases */
	vfree(buffer);
	return 0;
}

static int osst_reposition_and_retry(struct osst_tape * STp, struct osst_request ** aSRpnt,
					unsigned int frame, unsigned int skip, int pending)
{
	unsigned char		cmd[MAX_COMMAND_SIZE];
	struct osst_request   * SRpnt;
	char		      * name      = tape_name(STp);
	int			expected  = 0;
	int			attempts  = 1000 / skip;
	int			flag      = 1;
	unsigned long		startwait = jiffies;
#if DEBUG
	int			dbg       = debugging;
#endif

	while (attempts && time_before(jiffies, startwait + 60*HZ)) {
		if (flag) {
#if DEBUG
			debugging = dbg;
#endif
			if (frame < 2990 && frame+skip+STp->cur_frames+pending >= 2990)
				frame = 3000-skip;
			expected = frame+skip+STp->cur_frames+pending;
#if DEBUG
			printk(OSST_DEB_MSG "%s:D: Position to fppos %d, re-write from fseq %d\n",
					  name, frame+skip, STp->frame_seq_number-STp->cur_frames-pending);
#endif
			osst_set_frame_position(STp, aSRpnt, frame + skip, 1);
			flag = 0;
			attempts--;
			schedule_timeout_interruptible(msecs_to_jiffies(100));
		}
		if (osst_get_frame_position(STp, aSRpnt) < 0) {		/* additional write error */
#if DEBUG
			printk(OSST_DEB_MSG "%s:D: Addl error, host %d, tape %d, buffer %d\n",
					  name, STp->first_frame_position,
					  STp->last_frame_position, STp->cur_frames);
#endif
			frame = STp->last_frame_position;
			flag = 1;
			continue;
		}
		if (pending && STp->cur_frames < 50) {

			memset(cmd, 0, MAX_COMMAND_SIZE);
			cmd[0] = WRITE_6;
			cmd[1] = 1;
			cmd[4] = 1;
#if DEBUG
			printk(OSST_DEB_MSG "%s:D: About to write pending fseq %d at fppos %d\n",
					  name, STp->frame_seq_number-1, STp->first_frame_position);
#endif
			SRpnt = osst_do_scsi(*aSRpnt, STp, cmd, OS_FRAME_SIZE, DMA_TO_DEVICE,
						      STp->timeout, MAX_RETRIES, 1);
			*aSRpnt = SRpnt;

			if (STp->buffer->syscall_result) {		/* additional write error */
				if ((SRpnt->sense[ 2] & 0x0f) == 13 &&
				     SRpnt->sense[12]         ==  0 &&
				     SRpnt->sense[13]         ==  2) {
					printk(KERN_ERR
					       "%s:E: Volume overflow in write error recovery\n",
					       name);
					break;				/* hit end of tape = fail */
				}
				flag = 1;
			}
			else
				pending = 0;

			continue;
		}
		if (STp->cur_frames == 0) {
#if DEBUG
			debugging = dbg;
			printk(OSST_DEB_MSG "%s:D: Wait re-write finished\n", name);
#endif
			if (STp->first_frame_position != expected) {
				printk(KERN_ERR "%s:A: Actual position %d - expected %d\n", 
						name, STp->first_frame_position, expected);
				return (-EIO);
			}
			return 0;
		}
#if DEBUG
		if (debugging) {
			printk(OSST_DEB_MSG "%s:D: Sleeping in re-write wait ready\n", name);
			printk(OSST_DEB_MSG "%s:D: Turning off debugging for a while\n", name);
			debugging = 0;
		}
#endif
		schedule_timeout_interruptible(msecs_to_jiffies(100));
	}
	printk(KERN_ERR "%s:E: Failed to find valid tape media\n", name);
#if DEBUG
	debugging = dbg;
#endif
	return (-EIO);
}

/*
 * Error recovery algorithm for the OnStream tape.
 */

static int osst_write_error_recovery(struct osst_tape * STp, struct osst_request ** aSRpnt, int pending)
{
	struct osst_request * SRpnt  = * aSRpnt;
	struct st_partstat  * STps   = & STp->ps[STp->partition];
	char		    * name   = tape_name(STp);
	int		      retval = 0;
	int		      rw_state;
	unsigned int	      frame, skip;

	rw_state = STps->rw;

	if ((SRpnt->sense[ 2] & 0x0f) != 3
	  || SRpnt->sense[12]         != 12
	  || SRpnt->sense[13]         != 0) {
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Write error recovery cannot handle %02x:%02x:%02x\n", name,
			SRpnt->sense[2], SRpnt->sense[12], SRpnt->sense[13]);
#endif
		return (-EIO);
	}
	frame =	(SRpnt->sense[3] << 24) |
		(SRpnt->sense[4] << 16) |
		(SRpnt->sense[5] <<  8) |
		 SRpnt->sense[6];
	skip  =  SRpnt->sense[9];
 
#if DEBUG
	printk(OSST_DEB_MSG "%s:D: Detected physical bad frame at %u, advised to skip %d\n", name, frame, skip);
#endif
	osst_get_frame_position(STp, aSRpnt);
#if DEBUG
	printk(OSST_DEB_MSG "%s:D: reported frame positions: host = %d, tape = %d\n",
			name, STp->first_frame_position, STp->last_frame_position);
#endif
	switch (STp->write_type) {
	   case OS_WRITE_DATA:
	   case OS_WRITE_EOD:
	   case OS_WRITE_NEW_MARK:
		printk(KERN_WARNING 
			"%s:I: Relocating %d buffered logical frames from position %u to %u\n",
			name, STp->cur_frames, frame, (frame + skip > 3000 && frame < 3000)?3000:frame + skip);
		if (STp->os_fw_rev >= 10600)
			retval = osst_reposition_and_retry(STp, aSRpnt, frame, skip, pending);
		else
			retval = osst_read_back_buffer_and_rewrite(STp, aSRpnt, frame, skip, pending);
		printk(KERN_WARNING "%s:%s: %sWrite error%srecovered\n", name,
			       	retval?"E"    :"I",
			       	retval?""     :"Don't worry, ",
			       	retval?" not ":" ");
		break;
	   case OS_WRITE_LAST_MARK:
		printk(KERN_ERR "%s:E: Bad frame in update last marker, fatal\n", name);
		osst_set_frame_position(STp, aSRpnt, frame + STp->cur_frames + pending, 0);
		retval = -EIO;
		break;
	   case OS_WRITE_HEADER:
		printk(KERN_WARNING "%s:I: Bad frame in header partition, skipped\n", name);
		retval = osst_read_back_buffer_and_rewrite(STp, aSRpnt, frame, 1, pending);
		break;
	   default:
		printk(KERN_INFO "%s:I: Bad frame in filler, ignored\n", name);
		osst_set_frame_position(STp, aSRpnt, frame + STp->cur_frames + pending, 0);
	}
	osst_get_frame_position(STp, aSRpnt);
#if DEBUG
	printk(OSST_DEB_MSG "%s:D: Positioning complete, cur_frames %d, pos %d, tape pos %d\n", 
			name, STp->cur_frames, STp->first_frame_position, STp->last_frame_position);
	printk(OSST_DEB_MSG "%s:D: next logical frame to write: %d\n", name, STp->logical_blk_num);
#endif
	if (retval == 0) {
		STp->recover_count++;
		STp->recover_erreg++;
	} else
		STp->abort_count++;

	STps->rw = rw_state;
	return retval;
}

static int osst_space_over_filemarks_backward(struct osst_tape * STp, struct osst_request ** aSRpnt,
								 int mt_op, int mt_count)
{
	char  * name = tape_name(STp);
	int     cnt;
	int     last_mark_ppos = -1;

#if DEBUG
	printk(OSST_DEB_MSG "%s:D: Reached space_over_filemarks_backwards %d %d\n", name, mt_op, mt_count);
#endif
	if (osst_get_logical_frame(STp, aSRpnt, -1, 0) < 0) {
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Couldn't get logical blk num in space_filemarks_bwd\n", name);
#endif
		return -EIO;
	}
	if (STp->linux_media_version >= 4) {
		/*
		 * direct lookup in header filemark list
		 */
		cnt = ntohl(STp->buffer->aux->filemark_cnt);
		if (STp->header_ok                         && 
		    STp->header_cache != NULL              &&
		    (cnt - mt_count)  >= 0                 &&
		    (cnt - mt_count)   < OS_FM_TAB_MAX     &&
		    (cnt - mt_count)   < STp->filemark_cnt &&
		    STp->header_cache->dat_fm_tab.fm_tab_ent[cnt-1] == STp->buffer->aux->last_mark_ppos)

			last_mark_ppos = ntohl(STp->header_cache->dat_fm_tab.fm_tab_ent[cnt - mt_count]);
#if DEBUG
		if (STp->header_cache == NULL || (cnt - mt_count) < 0 || (cnt - mt_count) >= OS_FM_TAB_MAX)
			printk(OSST_DEB_MSG "%s:D: Filemark lookup fail due to %s\n", name,
			       STp->header_cache == NULL?"lack of header cache":"count out of range");
		else
			printk(OSST_DEB_MSG "%s:D: Filemark lookup: prev mark %d (%s), skip %d to %d\n",
				name, cnt,
				((cnt == -1 && ntohl(STp->buffer->aux->last_mark_ppos) == -1) ||
				 (STp->header_cache->dat_fm_tab.fm_tab_ent[cnt-1] ==
					 STp->buffer->aux->last_mark_ppos))?"match":"error",
			       mt_count, last_mark_ppos);
#endif
		if (last_mark_ppos > 10 && last_mark_ppos < STp->eod_frame_ppos) {
			osst_position_tape_and_confirm(STp, aSRpnt, last_mark_ppos);
			if (osst_get_logical_frame(STp, aSRpnt, -1, 0) < 0) {
#if DEBUG
				printk(OSST_DEB_MSG 
					"%s:D: Couldn't get logical blk num in space_filemarks\n", name);
#endif
				return (-EIO);
			}
			if (STp->buffer->aux->frame_type != OS_FRAME_TYPE_MARKER) {
				printk(KERN_WARNING "%s:W: Expected to find marker at ppos %d, not found\n",
						 name, last_mark_ppos);
				return (-EIO);
			}
			goto found;
		}
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Reverting to scan filemark backwards\n", name);
#endif
	}
	cnt = 0;
	while (cnt != mt_count) {
		last_mark_ppos = ntohl(STp->buffer->aux->last_mark_ppos);
		if (last_mark_ppos == -1)
			return (-EIO);
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Positioning to last mark at %d\n", name, last_mark_ppos);
#endif
		osst_position_tape_and_confirm(STp, aSRpnt, last_mark_ppos);
		cnt++;
		if (osst_get_logical_frame(STp, aSRpnt, -1, 0) < 0) {
#if DEBUG
			printk(OSST_DEB_MSG "%s:D: Couldn't get logical blk num in space_filemarks\n", name);
#endif
			return (-EIO);
		}
		if (STp->buffer->aux->frame_type != OS_FRAME_TYPE_MARKER) {
			printk(KERN_WARNING "%s:W: Expected to find marker at ppos %d, not found\n",
					 name, last_mark_ppos);
			return (-EIO);
		}
	}
found:
	if (mt_op == MTBSFM) {
		STp->frame_seq_number++;
		STp->frame_in_buffer      = 0;
		STp->buffer->buffer_bytes = 0;
		STp->buffer->read_pointer = 0;
		STp->logical_blk_num     += ntohs(STp->buffer->aux->dat.dat_list[0].blk_cnt);
	}
	return 0;
}

/*
 * ADRL 1.1 compatible "slow" space filemarks fwd version
 *
 * Just scans for the filemark sequentially.
 */
static int osst_space_over_filemarks_forward_slow(struct osst_tape * STp, struct osst_request ** aSRpnt,
								     int mt_op, int mt_count)
{
	int	cnt = 0;
#if DEBUG
	char  * name = tape_name(STp);

	printk(OSST_DEB_MSG "%s:D: Reached space_over_filemarks_forward_slow %d %d\n", name, mt_op, mt_count);
#endif
	if (osst_get_logical_frame(STp, aSRpnt, -1, 0) < 0) {
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Couldn't get logical blk num in space_filemarks_fwd\n", name);
#endif
		return (-EIO);
	}
	while (1) {
		if (osst_get_logical_frame(STp, aSRpnt, -1, 0) < 0) {
#if DEBUG
			printk(OSST_DEB_MSG "%s:D: Couldn't get logical blk num in space_filemarks\n", name);
#endif
			return (-EIO);
		}
		if (STp->buffer->aux->frame_type == OS_FRAME_TYPE_MARKER)
			cnt++;
		if (STp->buffer->aux->frame_type == OS_FRAME_TYPE_EOD) {
#if DEBUG
			printk(OSST_DEB_MSG "%s:D: space_fwd: EOD reached\n", name);
#endif
			if (STp->first_frame_position > STp->eod_frame_ppos+1) {
#if DEBUG
				printk(OSST_DEB_MSG "%s:D: EOD position corrected (%d=>%d)\n",
					       	name, STp->eod_frame_ppos, STp->first_frame_position-1);
#endif
				STp->eod_frame_ppos = STp->first_frame_position-1;
			}
			return (-EIO);
		}
		if (cnt == mt_count)
			break;
		STp->frame_in_buffer = 0;
	}
	if (mt_op == MTFSF) {
		STp->frame_seq_number++;
		STp->frame_in_buffer      = 0;
		STp->buffer->buffer_bytes = 0;
		STp->buffer->read_pointer = 0;
		STp->logical_blk_num     += ntohs(STp->buffer->aux->dat.dat_list[0].blk_cnt);
	}
	return 0;
}

/*
 * Fast linux specific version of OnStream FSF
 */
static int osst_space_over_filemarks_forward_fast(struct osst_tape * STp, struct osst_request ** aSRpnt,
								     int mt_op, int mt_count)
{
	char  * name = tape_name(STp);
	int	cnt  = 0,
		next_mark_ppos = -1;

#if DEBUG
	printk(OSST_DEB_MSG "%s:D: Reached space_over_filemarks_forward_fast %d %d\n", name, mt_op, mt_count);
#endif
	if (osst_get_logical_frame(STp, aSRpnt, -1, 0) < 0) {
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Couldn't get logical blk num in space_filemarks_fwd\n", name);
#endif
		return (-EIO);
	}

	if (STp->linux_media_version >= 4) {
		/*
		 * direct lookup in header filemark list
		 */
		cnt = ntohl(STp->buffer->aux->filemark_cnt) - 1;
		if (STp->header_ok                         && 
		    STp->header_cache != NULL              &&
		    (cnt + mt_count)   < OS_FM_TAB_MAX     &&
		    (cnt + mt_count)   < STp->filemark_cnt &&
		    ((cnt == -1 && ntohl(STp->buffer->aux->last_mark_ppos) == -1) ||
		     (STp->header_cache->dat_fm_tab.fm_tab_ent[cnt] == STp->buffer->aux->last_mark_ppos)))

			next_mark_ppos = ntohl(STp->header_cache->dat_fm_tab.fm_tab_ent[cnt + mt_count]);
#if DEBUG
		if (STp->header_cache == NULL || (cnt + mt_count) >= OS_FM_TAB_MAX)
			printk(OSST_DEB_MSG "%s:D: Filemark lookup fail due to %s\n", name,
			       STp->header_cache == NULL?"lack of header cache":"count out of range");
		else
			printk(OSST_DEB_MSG "%s:D: Filemark lookup: prev mark %d (%s), skip %d to %d\n",
			       name, cnt,
			       ((cnt == -1 && ntohl(STp->buffer->aux->last_mark_ppos) == -1) ||
				(STp->header_cache->dat_fm_tab.fm_tab_ent[cnt] ==
					 STp->buffer->aux->last_mark_ppos))?"match":"error",
			       mt_count, next_mark_ppos);
#endif
		if (next_mark_ppos <= 10 || next_mark_ppos > STp->eod_frame_ppos) {
#if DEBUG
			printk(OSST_DEB_MSG "%s:D: Reverting to slow filemark space\n", name);
#endif
			return osst_space_over_filemarks_forward_slow(STp, aSRpnt, mt_op, mt_count);
		} else {
			osst_position_tape_and_confirm(STp, aSRpnt, next_mark_ppos);
			if (osst_get_logical_frame(STp, aSRpnt, -1, 0) < 0) {
#if DEBUG
				printk(OSST_DEB_MSG "%s:D: Couldn't get logical blk num in space_filemarks\n",
						 name);
#endif
				return (-EIO);
			}
			if (STp->buffer->aux->frame_type != OS_FRAME_TYPE_MARKER) {
				printk(KERN_WARNING "%s:W: Expected to find marker at ppos %d, not found\n",
						 name, next_mark_ppos);
				return (-EIO);
			}
			if (ntohl(STp->buffer->aux->filemark_cnt) != cnt + mt_count) {
				printk(KERN_WARNING "%s:W: Expected to find marker %d at ppos %d, not %d\n",
						 name, cnt+mt_count, next_mark_ppos,
						 ntohl(STp->buffer->aux->filemark_cnt));
       				return (-EIO);
			}
		}
	} else {
		/*
		 * Find nearest (usually previous) marker, then jump from marker to marker
		 */
		while (1) {
			if (STp->buffer->aux->frame_type == OS_FRAME_TYPE_MARKER)
				break;
			if (STp->buffer->aux->frame_type == OS_FRAME_TYPE_EOD) {
#if DEBUG
				printk(OSST_DEB_MSG "%s:D: space_fwd: EOD reached\n", name);
#endif
				return (-EIO);
			}
			if (ntohl(STp->buffer->aux->filemark_cnt) == 0) {
				if (STp->first_mark_ppos == -1) {
#if DEBUG
					printk(OSST_DEB_MSG "%s:D: Reverting to slow filemark space\n", name);
#endif
					return osst_space_over_filemarks_forward_slow(STp, aSRpnt, mt_op, mt_count);
				}
				osst_position_tape_and_confirm(STp, aSRpnt, STp->first_mark_ppos);
				if (osst_get_logical_frame(STp, aSRpnt, -1, 0) < 0) {
#if DEBUG
					printk(OSST_DEB_MSG
					       "%s:D: Couldn't get logical blk num in space_filemarks_fwd_fast\n",
					       name);
#endif
					return (-EIO);
				}
				if (STp->buffer->aux->frame_type != OS_FRAME_TYPE_MARKER) {
					printk(KERN_WARNING "%s:W: Expected to find filemark at %d\n",
							 name, STp->first_mark_ppos);
					return (-EIO);
				}
			} else {
				if (osst_space_over_filemarks_backward(STp, aSRpnt, MTBSF, 1) < 0)
					return (-EIO);
				mt_count++;
			}
		}
		cnt++;
		while (cnt != mt_count) {
			next_mark_ppos = ntohl(STp->buffer->aux->next_mark_ppos);
			if (!next_mark_ppos || next_mark_ppos > STp->eod_frame_ppos) {
#if DEBUG
				printk(OSST_DEB_MSG "%s:D: Reverting to slow filemark space\n", name);
#endif
				return osst_space_over_filemarks_forward_slow(STp, aSRpnt, mt_op, mt_count - cnt);
			}
#if DEBUG
			else printk(OSST_DEB_MSG "%s:D: Positioning to next mark at %d\n", name, next_mark_ppos);
#endif
			osst_position_tape_and_confirm(STp, aSRpnt, next_mark_ppos);
			cnt++;
			if (osst_get_logical_frame(STp, aSRpnt, -1, 0) < 0) {
#if DEBUG
				printk(OSST_DEB_MSG "%s:D: Couldn't get logical blk num in space_filemarks\n",
						 name);
#endif
				return (-EIO);
			}
			if (STp->buffer->aux->frame_type != OS_FRAME_TYPE_MARKER) {
				printk(KERN_WARNING "%s:W: Expected to find marker at ppos %d, not found\n",
						 name, next_mark_ppos);
				return (-EIO);
			}
		}
	}
	if (mt_op == MTFSF) {
		STp->frame_seq_number++;
		STp->frame_in_buffer      = 0;
		STp->buffer->buffer_bytes = 0;
		STp->buffer->read_pointer = 0;
		STp->logical_blk_num     += ntohs(STp->buffer->aux->dat.dat_list[0].blk_cnt);
	}
	return 0;
}

/*
 * In debug mode, we want to see as many errors as possible
 * to test the error recovery mechanism.
 */
#if DEBUG
static void osst_set_retries(struct osst_tape * STp, struct osst_request ** aSRpnt, int retries)
{
	unsigned char		cmd[MAX_COMMAND_SIZE];
	struct osst_request   * SRpnt  = * aSRpnt;
	char		      * name   = tape_name(STp);

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = MODE_SELECT;
	cmd[1] = 0x10;
	cmd[4] = NUMBER_RETRIES_PAGE_LENGTH + MODE_HEADER_LENGTH;

	(STp->buffer)->b_data[0] = cmd[4] - 1;
	(STp->buffer)->b_data[1] = 0;			/* Medium Type - ignoring */
	(STp->buffer)->b_data[2] = 0;			/* Reserved */
	(STp->buffer)->b_data[3] = 0;			/* Block Descriptor Length */
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 0] = NUMBER_RETRIES_PAGE | (1 << 7);
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 1] = 2;
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 2] = 4;
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 3] = retries;

	if (debugging)
	    printk(OSST_DEB_MSG "%s:D: Setting number of retries on OnStream tape to %d\n", name, retries);

	SRpnt = osst_do_scsi(SRpnt, STp, cmd, cmd[4], DMA_TO_DEVICE, STp->timeout, 0, 1);
	*aSRpnt = SRpnt;

	if ((STp->buffer)->syscall_result)
	    printk (KERN_ERR "%s:D: Couldn't set retries to %d\n", name, retries);
}
#endif


static int osst_write_filemark(struct osst_tape * STp, struct osst_request ** aSRpnt)
{
	int	result;
	int	this_mark_ppos = STp->first_frame_position;
	int	this_mark_lbn  = STp->logical_blk_num;
#if DEBUG
	char  * name = tape_name(STp);
#endif

	if (STp->raw) return 0;

	STp->write_type = OS_WRITE_NEW_MARK;
#if DEBUG
	printk(OSST_DEB_MSG "%s:D: Writing Filemark %i at fppos %d (fseq %d, lblk %d)\n", 
	       name, STp->filemark_cnt, this_mark_ppos, STp->frame_seq_number, this_mark_lbn);
#endif
	STp->dirty = 1;
	result  = osst_flush_write_buffer(STp, aSRpnt);
	result |= osst_flush_drive_buffer(STp, aSRpnt);
	STp->last_mark_ppos = this_mark_ppos;
	STp->last_mark_lbn  = this_mark_lbn;
	if (STp->header_cache != NULL && STp->filemark_cnt < OS_FM_TAB_MAX)
		STp->header_cache->dat_fm_tab.fm_tab_ent[STp->filemark_cnt] = htonl(this_mark_ppos);
	if (STp->filemark_cnt++ == 0)
		STp->first_mark_ppos = this_mark_ppos;
	return result;
}

static int osst_write_eod(struct osst_tape * STp, struct osst_request ** aSRpnt)
{
	int	result;
#if DEBUG
	char  * name = tape_name(STp);
#endif

	if (STp->raw) return 0;

	STp->write_type = OS_WRITE_EOD;
	STp->eod_frame_ppos = STp->first_frame_position;
#if DEBUG
	printk(OSST_DEB_MSG "%s:D: Writing EOD at fppos %d (fseq %d, lblk %d)\n", name,
			STp->eod_frame_ppos, STp->frame_seq_number, STp->logical_blk_num);
#endif
	STp->dirty = 1;

	result  = osst_flush_write_buffer(STp, aSRpnt);	
	result |= osst_flush_drive_buffer(STp, aSRpnt);
	STp->eod_frame_lfa = --(STp->frame_seq_number);
	return result;
}

static int osst_write_filler(struct osst_tape * STp, struct osst_request ** aSRpnt, int where, int count)
{
	char * name = tape_name(STp);

#if DEBUG
	printk(OSST_DEB_MSG "%s:D: Reached onstream write filler group %d\n", name, where);
#endif
	osst_wait_ready(STp, aSRpnt, 60 * 5, 0);
	osst_set_frame_position(STp, aSRpnt, where, 0);
	STp->write_type = OS_WRITE_FILLER;
	while (count--) {
		memcpy(STp->buffer->b_data, "Filler", 6);
		STp->buffer->buffer_bytes = 6;
		STp->dirty = 1;
		if (osst_flush_write_buffer(STp, aSRpnt)) {
			printk(KERN_INFO "%s:I: Couldn't write filler frame\n", name);
			return (-EIO);
		}
	}
#if DEBUG
	printk(OSST_DEB_MSG "%s:D: Exiting onstream write filler group\n", name);
#endif
	return osst_flush_drive_buffer(STp, aSRpnt);
}

static int __osst_write_header(struct osst_tape * STp, struct osst_request ** aSRpnt, int where, int count)
{
	char * name = tape_name(STp);
	int     result;

#if DEBUG
	printk(OSST_DEB_MSG "%s:D: Reached onstream write header group %d\n", name, where);
#endif
	osst_wait_ready(STp, aSRpnt, 60 * 5, 0);
	osst_set_frame_position(STp, aSRpnt, where, 0);
	STp->write_type = OS_WRITE_HEADER;
	while (count--) {
		osst_copy_to_buffer(STp->buffer, (unsigned char *)STp->header_cache);
		STp->buffer->buffer_bytes = sizeof(os_header_t);
		STp->dirty = 1;
		if (osst_flush_write_buffer(STp, aSRpnt)) {
			printk(KERN_INFO "%s:I: Couldn't write header frame\n", name);
			return (-EIO);
		}
	}
	result = osst_flush_drive_buffer(STp, aSRpnt);
#if DEBUG
	printk(OSST_DEB_MSG "%s:D: Write onstream header group %s\n", name, result?"failed":"done");
#endif
	return result;
}

static int osst_write_header(struct osst_tape * STp, struct osst_request ** aSRpnt, int locate_eod)
{
	os_header_t * header;
	int	      result;
	char        * name = tape_name(STp);

#if DEBUG
	printk(OSST_DEB_MSG "%s:D: Writing tape header\n", name);
#endif
	if (STp->raw) return 0;

	if (STp->header_cache == NULL) {
		if ((STp->header_cache = (os_header_t *)vmalloc(sizeof(os_header_t))) == NULL) {
			printk(KERN_ERR "%s:E: Failed to allocate header cache\n", name);
			return (-ENOMEM);
		}
		memset(STp->header_cache, 0, sizeof(os_header_t));
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Allocated and cleared memory for header cache\n", name);
#endif
	}
	if (STp->header_ok) STp->update_frame_cntr++;
	else                STp->update_frame_cntr = 0;

	header = STp->header_cache;
	strcpy(header->ident_str, "ADR_SEQ");
	header->major_rev      = 1;
	header->minor_rev      = 4;
	header->ext_trk_tb_off = htons(17192);
	header->pt_par_num     = 1;
	header->partition[0].partition_num              = OS_DATA_PARTITION;
	header->partition[0].par_desc_ver               = OS_PARTITION_VERSION;
	header->partition[0].wrt_pass_cntr              = htons(STp->wrt_pass_cntr);
	header->partition[0].first_frame_ppos           = htonl(STp->first_data_ppos);
	header->partition[0].last_frame_ppos            = htonl(STp->capacity);
	header->partition[0].eod_frame_ppos             = htonl(STp->eod_frame_ppos);
	header->cfg_col_width                           = htonl(20);
	header->dat_col_width                           = htonl(1500);
	header->qfa_col_width                           = htonl(0);
	header->ext_track_tb.nr_stream_part             = 1;
	header->ext_track_tb.et_ent_sz                  = 32;
	header->ext_track_tb.dat_ext_trk_ey.et_part_num = 0;
	header->ext_track_tb.dat_ext_trk_ey.fmt         = 1;
	header->ext_track_tb.dat_ext_trk_ey.fm_tab_off  = htons(17736);
	header->ext_track_tb.dat_ext_trk_ey.last_hlb_hi = 0;
	header->ext_track_tb.dat_ext_trk_ey.last_hlb    = htonl(STp->eod_frame_lfa);
	header->ext_track_tb.dat_ext_trk_ey.last_pp	= htonl(STp->eod_frame_ppos);
	header->dat_fm_tab.fm_part_num                  = 0;
	header->dat_fm_tab.fm_tab_ent_sz                = 4;
	header->dat_fm_tab.fm_tab_ent_cnt               = htons(STp->filemark_cnt<OS_FM_TAB_MAX?
								STp->filemark_cnt:OS_FM_TAB_MAX);

	result  = __osst_write_header(STp, aSRpnt, 0xbae, 5);
	if (STp->update_frame_cntr == 0)
		    osst_write_filler(STp, aSRpnt, 0xbb3, 5);
	result &= __osst_write_header(STp, aSRpnt,     5, 5);

	if (locate_eod) {
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Locating back to eod frame addr %d\n", name, STp->eod_frame_ppos);
#endif
		osst_set_frame_position(STp, aSRpnt, STp->eod_frame_ppos, 0);
	}
	if (result)
		printk(KERN_ERR "%s:E: Write header failed\n", name);
	else {
		memcpy(STp->application_sig, "LIN4", 4);
		STp->linux_media         = 1;
		STp->linux_media_version = 4;
		STp->header_ok           = 1;
	}
	return result;
}

static int osst_reset_header(struct osst_tape * STp, struct osst_request ** aSRpnt)
{
	if (STp->header_cache != NULL)
		memset(STp->header_cache, 0, sizeof(os_header_t));

	STp->logical_blk_num = STp->frame_seq_number = 0;
	STp->frame_in_buffer = 0;
	STp->eod_frame_ppos = STp->first_data_ppos = 0x0000000A;
	STp->filemark_cnt = 0;
	STp->first_mark_ppos = STp->last_mark_ppos = STp->last_mark_lbn = -1;
	return osst_write_header(STp, aSRpnt, 1);
}

static int __osst_analyze_headers(struct osst_tape * STp, struct osst_request ** aSRpnt, int ppos)
{
	char        * name = tape_name(STp);
	os_header_t * header;
	os_aux_t    * aux;
	char          id_string[8];
	int	      linux_media_version,
		      update_frame_cntr;

	if (STp->raw)
		return 1;

	if (ppos == 5 || ppos == 0xbae || STp->buffer->syscall_result) {
		if (osst_set_frame_position(STp, aSRpnt, ppos, 0))
			printk(KERN_WARNING "%s:W: Couldn't position tape\n", name);
		osst_wait_ready(STp, aSRpnt, 60 * 15, 0);
		if (osst_initiate_read (STp, aSRpnt)) {
			printk(KERN_WARNING "%s:W: Couldn't initiate read\n", name);
			return 0;
		}
	}
	if (osst_read_frame(STp, aSRpnt, 180)) {
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Couldn't read header frame\n", name);
#endif
		return 0;
	}
	header = (os_header_t *) STp->buffer->b_data;	/* warning: only first segment addressable */
	aux = STp->buffer->aux;
	if (aux->frame_type != OS_FRAME_TYPE_HEADER) {
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Skipping non-header frame (%d)\n", name, ppos);
#endif
		return 0;
	}
	if (ntohl(aux->frame_seq_num)              != 0                   ||
	    ntohl(aux->logical_blk_num)            != 0                   ||
	          aux->partition.partition_num     != OS_CONFIG_PARTITION ||
	    ntohl(aux->partition.first_frame_ppos) != 0                   ||
	    ntohl(aux->partition.last_frame_ppos)  != 0xbb7               ) {
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Invalid header frame (%d,%d,%d,%d,%d)\n", name,
				ntohl(aux->frame_seq_num), ntohl(aux->logical_blk_num),
			       	aux->partition.partition_num, ntohl(aux->partition.first_frame_ppos),
			       	ntohl(aux->partition.last_frame_ppos));
#endif
		return 0;
	}
	if (strncmp(header->ident_str, "ADR_SEQ", 7) != 0 &&
	    strncmp(header->ident_str, "ADR-SEQ", 7) != 0) {
		strlcpy(id_string, header->ident_str, 8);
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Invalid header identification string %s\n", name, id_string);
#endif
		return 0;
	}
	update_frame_cntr = ntohl(aux->update_frame_cntr);
	if (update_frame_cntr < STp->update_frame_cntr) {
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Skipping frame %d with update_frame_counter %d<%d\n",
				   name, ppos, update_frame_cntr, STp->update_frame_cntr);
#endif
		return 0;
	}
	if (header->major_rev != 1 || header->minor_rev != 4 ) {
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: %s revision %d.%d detected (1.4 supported)\n", 
				 name, (header->major_rev != 1 || header->minor_rev < 2 || 
				       header->minor_rev  > 4 )? "Invalid" : "Warning:",
				 header->major_rev, header->minor_rev);
#endif
		if (header->major_rev != 1 || header->minor_rev < 2 || header->minor_rev > 4)
			return 0;
	}
#if DEBUG
	if (header->pt_par_num != 1)
		printk(KERN_INFO "%s:W: %d partitions defined, only one supported\n", 
				 name, header->pt_par_num);
#endif
	memcpy(id_string, aux->application_sig, 4);
	id_string[4] = 0;
	if (memcmp(id_string, "LIN", 3) == 0) {
		STp->linux_media = 1;
		linux_media_version = id_string[3] - '0';
		if (linux_media_version != 4)
			printk(KERN_INFO "%s:I: Linux media version %d detected (current 4)\n",
					 name, linux_media_version);
	} else {
		printk(KERN_WARNING "%s:W: Non Linux media detected (%s)\n", name, id_string);
		return 0;
	}
	if (linux_media_version < STp->linux_media_version) {
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Skipping frame %d with linux_media_version %d\n",
				  name, ppos, linux_media_version);
#endif
		return 0;
	}
	if (linux_media_version > STp->linux_media_version) {
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Frame %d sets linux_media_version to %d\n",
				   name, ppos, linux_media_version);
#endif
		memcpy(STp->application_sig, id_string, 5);
		STp->linux_media_version = linux_media_version;
		STp->update_frame_cntr = -1;
	}
	if (update_frame_cntr > STp->update_frame_cntr) {
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Frame %d sets update_frame_counter to %d\n",
				   name, ppos, update_frame_cntr);
#endif
		if (STp->header_cache == NULL) {
			if ((STp->header_cache = (os_header_t *)vmalloc(sizeof(os_header_t))) == NULL) {
				printk(KERN_ERR "%s:E: Failed to allocate header cache\n", name);
				return 0;
			}
#if DEBUG
			printk(OSST_DEB_MSG "%s:D: Allocated memory for header cache\n", name);
#endif
		}
		osst_copy_from_buffer(STp->buffer, (unsigned char *)STp->header_cache);
		header = STp->header_cache;	/* further accesses from cached (full) copy */

		STp->wrt_pass_cntr     = ntohs(header->partition[0].wrt_pass_cntr);
		STp->first_data_ppos   = ntohl(header->partition[0].first_frame_ppos);
		STp->eod_frame_ppos    = ntohl(header->partition[0].eod_frame_ppos);
		STp->eod_frame_lfa     = ntohl(header->ext_track_tb.dat_ext_trk_ey.last_hlb);
		STp->filemark_cnt      = ntohl(aux->filemark_cnt);
		STp->first_mark_ppos   = ntohl(aux->next_mark_ppos);
		STp->last_mark_ppos    = ntohl(aux->last_mark_ppos);
		STp->last_mark_lbn     = ntohl(aux->last_mark_lbn);
		STp->update_frame_cntr = update_frame_cntr;
#if DEBUG
	printk(OSST_DEB_MSG "%s:D: Detected write pass %d, update frame counter %d, filemark counter %d\n",
			  name, STp->wrt_pass_cntr, STp->update_frame_cntr, STp->filemark_cnt);
	printk(OSST_DEB_MSG "%s:D: first data frame on tape = %d, last = %d, eod frame = %d\n", name,
			  STp->first_data_ppos,
			  ntohl(header->partition[0].last_frame_ppos),
			  ntohl(header->partition[0].eod_frame_ppos));
	printk(OSST_DEB_MSG "%s:D: first mark on tape = %d, last = %d, eod frame = %d\n", 
			  name, STp->first_mark_ppos, STp->last_mark_ppos, STp->eod_frame_ppos);
#endif
		if (header->minor_rev < 4 && STp->linux_media_version == 4) {
#if DEBUG
			printk(OSST_DEB_MSG "%s:D: Moving filemark list to ADR 1.4 location\n", name);
#endif
			memcpy((void *)header->dat_fm_tab.fm_tab_ent, 
			       (void *)header->old_filemark_list, sizeof(header->dat_fm_tab.fm_tab_ent));
			memset((void *)header->old_filemark_list, 0, sizeof(header->old_filemark_list));
		}
		if (header->minor_rev == 4   &&
		    (header->ext_trk_tb_off                          != htons(17192)               ||
		     header->partition[0].partition_num              != OS_DATA_PARTITION          ||
		     header->partition[0].par_desc_ver               != OS_PARTITION_VERSION       ||
		     header->partition[0].last_frame_ppos            != htonl(STp->capacity)       ||
		     header->cfg_col_width                           != htonl(20)                  ||
		     header->dat_col_width                           != htonl(1500)                ||
		     header->qfa_col_width                           != htonl(0)                   ||
		     header->ext_track_tb.nr_stream_part             != 1                          ||
		     header->ext_track_tb.et_ent_sz                  != 32                         ||
		     header->ext_track_tb.dat_ext_trk_ey.et_part_num != OS_DATA_PARTITION          ||
		     header->ext_track_tb.dat_ext_trk_ey.fmt         != 1                          ||
		     header->ext_track_tb.dat_ext_trk_ey.fm_tab_off  != htons(17736)               ||
		     header->ext_track_tb.dat_ext_trk_ey.last_hlb_hi != 0                          ||
		     header->ext_track_tb.dat_ext_trk_ey.last_pp     != htonl(STp->eod_frame_ppos) ||
		     header->dat_fm_tab.fm_part_num                  != OS_DATA_PARTITION          ||
		     header->dat_fm_tab.fm_tab_ent_sz                != 4                          ||
		     header->dat_fm_tab.fm_tab_ent_cnt               !=
			     htons(STp->filemark_cnt<OS_FM_TAB_MAX?STp->filemark_cnt:OS_FM_TAB_MAX)))
			printk(KERN_WARNING "%s:W: Failed consistency check ADR 1.4 format\n", name);

	}

	return 1;
}

static int osst_analyze_headers(struct osst_tape * STp, struct osst_request ** aSRpnt)
{
	int	position, ppos;
	int	first, last;
	int	valid = 0;
	char  * name  = tape_name(STp);

	position = osst_get_frame_position(STp, aSRpnt);

	if (STp->raw) {
		STp->header_ok = STp->linux_media = 1;
		STp->linux_media_version = 0;
		return 1;
	}
	STp->header_ok = STp->linux_media = STp->linux_media_version = 0;
	STp->wrt_pass_cntr = STp->update_frame_cntr = -1;
	STp->eod_frame_ppos = STp->first_data_ppos = -1;
	STp->first_mark_ppos = STp->last_mark_ppos = STp->last_mark_lbn = -1;
#if DEBUG
	printk(OSST_DEB_MSG "%s:D: Reading header\n", name);
#endif

	/* optimization for speed - if we are positioned at ppos 10, read second group first  */	
	/* TODO try the ADR 1.1 locations for the second group if we have no valid one yet... */

	first = position==10?0xbae: 5;
	last  = position==10?0xbb3:10;

	for (ppos = first; ppos < last; ppos++)
		if (__osst_analyze_headers(STp, aSRpnt, ppos))
			valid = 1;

	first = position==10? 5:0xbae;
	last  = position==10?10:0xbb3;

	for (ppos = first; ppos < last; ppos++)
		if (__osst_analyze_headers(STp, aSRpnt, ppos))
			valid = 1;

	if (!valid) {
		printk(KERN_ERR "%s:E: Failed to find valid ADRL header, new media?\n", name);
		STp->eod_frame_ppos = STp->first_data_ppos = 0;
		osst_set_frame_position(STp, aSRpnt, 10, 0);
		return 0;
	}
	if (position <= STp->first_data_ppos) {
		position = STp->first_data_ppos;
		STp->ps[0].drv_file = STp->ps[0].drv_block = STp->frame_seq_number = STp->logical_blk_num = 0;
	}
	osst_set_frame_position(STp, aSRpnt, position, 0);
	STp->header_ok = 1;

	return 1;
}

static int osst_verify_position(struct osst_tape * STp, struct osst_request ** aSRpnt)
{
	int	frame_position  = STp->first_frame_position;
	int	frame_seq_numbr = STp->frame_seq_number;
	int	logical_blk_num = STp->logical_blk_num;
       	int	halfway_frame   = STp->frame_in_buffer;
	int	read_pointer    = STp->buffer->read_pointer;
	int	prev_mark_ppos  = -1;
	int	actual_mark_ppos, i, n;
#if DEBUG
	char  * name = tape_name(STp);

	printk(OSST_DEB_MSG "%s:D: Verify that the tape is really the one we think before writing\n", name);
#endif
	osst_set_frame_position(STp, aSRpnt, frame_position - 1, 0);
	if (osst_get_logical_frame(STp, aSRpnt, -1, 0) < 0) {
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Couldn't get logical blk num in verify_position\n", name);
#endif
		return (-EIO);
	}
	if (STp->linux_media_version >= 4) {
		for (i=0; i<STp->filemark_cnt; i++)
			if ((n=ntohl(STp->header_cache->dat_fm_tab.fm_tab_ent[i])) < frame_position)
				prev_mark_ppos = n;
	} else
		prev_mark_ppos = frame_position - 1;  /* usually - we don't really know */
	actual_mark_ppos = STp->buffer->aux->frame_type == OS_FRAME_TYPE_MARKER ?
				frame_position - 1 : ntohl(STp->buffer->aux->last_mark_ppos);
	if (frame_position  != STp->first_frame_position                   ||
	    frame_seq_numbr != STp->frame_seq_number + (halfway_frame?0:1) ||
	    prev_mark_ppos  != actual_mark_ppos                            ) {
#if DEBUG
		printk(OSST_DEB_MSG "%s:D: Block mismatch: fppos %d-%d, fseq %d-%d, mark %d-%d\n", name,
				  STp->first_frame_position, frame_position, 
				  STp->frame_seq_number + (halfway_frame?0:1),
				  frame_seq_numbr, actual_mark_ppos, prev_mark_ppos);
#endif
		return (-EIO);
	}
	if (halfway_frame) {
		/* prepare buffer for append and rewrite on top of original */
		osst_set_frame_position(STp, aSRpnt, frame_position - 1, 0);
		STp->buffer->buffer_bytes  = read_pointer;
		STp->ps[STp->partition].rw = ST_WRITING;
		STp->dirty                 = 1;
	}
	STp->frame_in_buffer  = halfway_frame;
	STp->frame_seq_number = frame_seq_numbr;
	STp->logical_blk_num  = logical_blk_num;
	return 0;
}

/* Acc. to OnStream, the vers. numbering is the following:
 * X.XX for released versions (X=digit), 
 * XXXY for unreleased versions (Y=letter)
 * Ordering 1.05 < 106A < 106B < ...  < 106a < ... < 1.06
 * This fn makes monoton numbers out of this scheme ...
 */
static unsigned int osst_parse_firmware_rev (const char * str)
{
	if (str[1] == '.') {
		return (str[0]-'0')*10000
			+(str[2]-'0')*1000
			+(str[3]-'0')*100;
	} else {
		return (str[0]-'0')*10000
			+(str[1]-'0')*1000
			+(str[2]-'0')*100 - 100
			+(str[3]-'@');
	}
}

/*
 * Configure the OnStream SCII tape drive for default operation
 */
static int osst_configure_onstream(struct osst_tape *STp, struct osst_request ** aSRpnt)
{
	unsigned char                  cmd[MAX_COMMAND_SIZE];
	char                         * name = tape_name(STp);
	struct osst_request          * SRpnt = * aSRpnt;
	osst_mode_parameter_header_t * header;
	osst_block_size_page_t       * bs;
	osst_capabilities_page_t     * cp;
	osst_tape_paramtr_page_t     * prm;
	int                            drive_buffer_size;

	if (STp->ready != ST_READY) {
#if DEBUG
	    printk(OSST_DEB_MSG "%s:D: Not Ready\n", name);
#endif
	    return (-EIO);
	}
	
	if (STp->os_fw_rev < 10600) {
	    printk(KERN_INFO "%s:I: Old OnStream firmware revision detected (%s),\n", name, STp->device->rev);
	    printk(KERN_INFO "%s:I: an upgrade to version 1.06 or above is recommended\n", name);
	}

	/*
	 * Configure 32.5KB (data+aux) frame size.
         * Get the current frame size from the block size mode page
	 */
	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = MODE_SENSE;
	cmd[1] = 8;
	cmd[2] = BLOCK_SIZE_PAGE;
	cmd[4] = BLOCK_SIZE_PAGE_LENGTH + MODE_HEADER_LENGTH;

	SRpnt = osst_do_scsi(SRpnt, STp, cmd, cmd[4], DMA_FROM_DEVICE, STp->timeout, 0, 1);
	if (SRpnt == NULL) {
#if DEBUG
 	    printk(OSST_DEB_MSG "osst :D: Busy\n");
#endif
	    return (-EBUSY);
	}
	*aSRpnt = SRpnt;
	if ((STp->buffer)->syscall_result != 0) {
	    printk (KERN_ERR "%s:E: Can't get tape block size mode page\n", name);
	    return (-EIO);
	}

	header = (osst_mode_parameter_header_t *) (STp->buffer)->b_data;
	bs = (osst_block_size_page_t *) ((STp->buffer)->b_data + sizeof(osst_mode_parameter_header_t) + header->bdl);

#if DEBUG
	printk(OSST_DEB_MSG "%s:D: 32KB play back: %s\n",   name, bs->play32     ? "Yes" : "No");
	printk(OSST_DEB_MSG "%s:D: 32.5KB play back: %s\n", name, bs->play32_5   ? "Yes" : "No");
	printk(OSST_DEB_MSG "%s:D: 32KB record: %s\n",      name, bs->record32   ? "Yes" : "No");
	printk(OSST_DEB_MSG "%s:D: 32.5KB record: %s\n",    name, bs->record32_5 ? "Yes" : "No");
#endif

	/*
	 * Configure default auto columns mode, 32.5KB transfer mode
	 */ 
	bs->one = 1;
	bs->play32 = 0;
	bs->play32_5 = 1;
	bs->record32 = 0;
	bs->record32_5 = 1;

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = MODE_SELECT;
	cmd[1] = 0x10;
	cmd[4] = BLOCK_SIZE_PAGE_LENGTH + MODE_HEADER_LENGTH;

	SRpnt = osst_do_scsi(SRpnt, STp, cmd, cmd[4], DMA_TO_DEVICE, STp->timeout, 0, 1);
	*aSRpnt = SRpnt;
	if ((STp->buffer)->syscall_result != 0) {
	    printk (KERN_ERR "%s:E: Couldn't set tape block size mode page\n", name);
	    return (-EIO);
	}

#if DEBUG
	printk(KERN_INFO "%s:D: Drive Block Size changed to 32.5K\n", name);
	 /*
	 * In debug mode, we want to see as many errors as possible
	 * to test the error recovery mechanism.
	 */
	osst_set_retries(STp, aSRpnt, 0);
	SRpnt = * aSRpnt;
#endif

	/*
	 * Set vendor name to 'LIN4' for "Linux support version 4".
	 */

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = MODE_SELECT;
	cmd[1] = 0x10;
	cmd[4] = VENDOR_IDENT_PAGE_LENGTH + MODE_HEADER_LENGTH;

	header->mode_data_length = VENDOR_IDENT_PAGE_LENGTH + MODE_HEADER_LENGTH - 1;
	header->medium_type      = 0;	/* Medium Type - ignoring */
	header->dsp              = 0;	/* Reserved */
	header->bdl              = 0;	/* Block Descriptor Length */
	
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 0] = VENDOR_IDENT_PAGE | (1 << 7);
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 1] = 6;
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 2] = 'L';
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 3] = 'I';
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 4] = 'N';
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 5] = '4';
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 6] = 0;
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 7] = 0;

	SRpnt = osst_do_scsi(SRpnt, STp, cmd, cmd[4], DMA_TO_DEVICE, STp->timeout, 0, 1);
	*aSRpnt = SRpnt;

	if ((STp->buffer)->syscall_result != 0) {
	    printk (KERN_ERR "%s:E: Couldn't set vendor name to %s\n", name, 
			(char *) ((STp->buffer)->b_data + MODE_HEADER_LENGTH + 2));
	    return (-EIO);
	}

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = MODE_SENSE;
	cmd[1] = 8;
	cmd[2] = CAPABILITIES_PAGE;
	cmd[4] = CAPABILITIES_PAGE_LENGTH + MODE_HEADER_LENGTH;

	SRpnt = osst_do_scsi(SRpnt, STp, cmd, cmd[4], DMA_FROM_DEVICE, STp->timeout, 0, 1);
	*aSRpnt = SRpnt;

	if ((STp->buffer)->syscall_result != 0) {
	    printk (KERN_ERR "%s:E: Can't get capabilities page\n", name);
	    return (-EIO);
	}

	header = (osst_mode_parameter_header_t *) (STp->buffer)->b_data;
	cp     = (osst_capabilities_page_t    *) ((STp->buffer)->b_data +
		 sizeof(osst_mode_parameter_header_t) + header->bdl);

	drive_buffer_size = ntohs(cp->buffer_size) / 2;

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = MODE_SENSE;
	cmd[1] = 8;
	cmd[2] = TAPE_PARAMTR_PAGE;
	cmd[4] = TAPE_PARAMTR_PAGE_LENGTH + MODE_HEADER_LENGTH;

	SRpnt = osst_do_scsi(SRpnt, STp, cmd, cmd[4], DMA_FROM_DEVICE, STp->timeout, 0, 1);
	*aSRpnt = SRpnt;

	if ((STp->buffer)->syscall_result != 0) {
	    printk (KERN_ERR "%s:E: Can't get tape parameter page\n", name);
	    return (-EIO);
	}

	header = (osst_mode_parameter_header_t *) (STp->buffer)->b_data;
	prm    = (osst_tape_paramtr_page_t    *) ((STp->buffer)->b_data +
		 sizeof(osst_mode_parameter_header_t) + header->bdl);

	STp->density  = prm->density;
	STp->capacity = ntohs(prm->segtrk) * ntohs(prm->trks);
#if DEBUG
	printk(OSST_DEB_MSG "%s:D: Density %d, tape length: %dMB, drive buffer size: %dKB\n",
			  name, STp->density, STp->capacity / 32, drive_buffer_size);
#endif

	return 0;
	
}


/* Step over EOF if it has been inadvertently crossed (ioctl not used because
   it messes up the block number). */
static int cross_eof(struct osst_tape *STp, struct osst_request ** aSRpnt, int forward)
{
	int	result;
	char  * name = tape_name(STp);

#if DEBUG
	if (debugging)
		printk(OSST_DEB_MSG "%s:D: Stepping over filemark %s.\n",
	   			  name, forward ? "forward" : "backward");
#endif

	if (forward) {
	   /* assumes that the filemark is already read by the drive, so this is low cost */
	   result = osst_space_over_filemarks_forward_slow(STp, aSRpnt, MTFSF, 1);
	}
	else
	   /* assumes this is only called if we just read the filemark! */
	   result = osst_seek_logical_blk(STp, aSRpnt, STp->logical_blk_num - 1);

	if (result < 0)
	   printk(KERN_WARNING "%s:W: Stepping over filemark %s failed.\n",
				name, forward ? "forward" : "backward");

	return result;
}


/* Get the tape position. */

static int osst_get_frame_position(struct osst_tape *STp, struct osst_request ** aSRpnt)
{
	unsigned char		scmd[MAX_COMMAND_SIZE];
	struct osst_request   * SRpnt;
	int			result = 0;
	char    	      * name   = tape_name(STp);

	/* KG: We want to be able to use it for checking Write Buffer availability
	 *  and thus don't want to risk to overwrite anything. Exchange buffers ... */
	char		mybuf[24];
	char	      * olddata = STp->buffer->b_data;
	int		oldsize = STp->buffer->buffer_size;

	if (STp->ready != ST_READY) return (-EIO);

	memset (scmd, 0, MAX_COMMAND_SIZE);
	scmd[0] = READ_POSITION;

	STp->buffer->b_data = mybuf; STp->buffer->buffer_size = 24;
	SRpnt = osst_do_scsi(*aSRpnt, STp, scmd, 20, DMA_FROM_DEVICE,
				      STp->timeout, MAX_RETRIES, 1);
	if (!SRpnt) {
		STp->buffer->b_data = olddata; STp->buffer->buffer_size = oldsize;
		return (-EBUSY);
	}
	*aSRpnt = SRpnt;

	if (STp->buffer->syscall_result)
		result = ((SRpnt->sense[2] & 0x0f) == 3) ? -EIO : -EINVAL;	/* 3: Write Error */

	if (result == -EINVAL)
		printk(KERN_ERR "%s:E: Can't read tape position.\n", name);
	else {
		if (result == -EIO) {	/* re-read position - this needs to preserve media errors */
			unsigned char mysense[16];
			memcpy (mysense, SRpnt->sense, 16);
			memset (scmd, 0, MAX_COMMAND_SIZE);
			scmd[0] = READ_POSITION;
			STp->buffer->b_data = mybuf; STp->buffer->buffer_size = 24;
			SRpnt = osst_do_scsi(SRpnt, STp, scmd, 20, DMA_FROM_DEVICE,
						    STp->timeout, MAX_RETRIES, 1);
#if DEBUG
			printk(OSST_DEB_MSG "%s:D: Reread position, reason=[%02x:%02x:%02x], result=[%s%02x:%02x:%02x]\n",
					name, mysense[2], mysense[12], mysense[13], STp->buffer->syscall_result?"":"ok:",
					SRpnt->sense[2],SRpnt->sense[12],SRpnt->sense[13]);
#endif
			if (!STp->buffer->syscall_result)
				memcpy (SRpnt->sense, mysense, 16);
			else
				printk(KERN_WARNING "%s:W: Double error in get position\n", name);
		}
		STp->first_frame_position = ((STp->buffer)->b_data[4] << 24)
					  + ((STp->buffer)->b_data[5] << 16)
					  + ((STp->buffer)->b_data[6] << 8)
					  +  (STp->buffer)->b_data[7];
		STp->last_frame_position  = ((STp->buffer)->b_data[ 8] << 24)
					  + ((STp->buffer)->b_data[ 9] << 16)
					  + ((STp->buffer)->b_data[10] <<  8)
					  +  (STp->buffer)->b_data[11];
		STp->cur_frames           =  (STp->buffer)->b_data[15];
#if DEBUG
		if (debugging) {
			printk(OSST_DEB_MSG "%s:D: Drive Positions: host %d, tape %d%s, buffer %d\n", name,
					    STp->first_frame_position, STp->last_frame_position,
					    ((STp->buffer)->b_data[0]&0x80)?" (BOP)":
					    ((STp->buffer)->b_data[0]&0x40)?" (EOP)":"",
					    STp->cur_frames);
		}
#endif
		if (STp->cur_frames == 0 && STp->first_frame_position != STp->last_frame_position) {
#if DEBUG
			printk(OSST_DEB_MSG "%s:D: Correcting read position %d, %d, %d\n", name,
					STp->first_frame_position, STp->last_frame_position, STp->cur_frames);
#endif
			STp->first_frame_position = STp->last_frame_position;
		}
	}
	STp->buffer->b_data = olddata; STp->buffer->buffer_size = oldsize;

	return (result == 0 ? STp->first_frame_position : result);
}


/* Set the tape block */
static int osst_set_frame_position(struct osst_tape *STp, struct osst_request ** aSRpnt, int ppos, int skip)
{
	unsigned char		scmd[MAX_COMMAND_SIZE];
	struct osst_request   * SRpnt;
	struct st_partstat    * STps;
	int			result = 0;
	int			pp     = (ppos == 3000 && !skip)? 0 : ppos;
	char		      * name   = tape_name(STp);

	if (STp->ready != ST_READY) return (-EIO);

	STps = &(STp->ps[STp->partition]);

	if (ppos < 0 || ppos > STp->capacity) {
		printk(KERN_WARNING "%s:W: Reposition request %d out of range\n", name, ppos);
		pp = ppos = ppos < 0 ? 0 : (STp->capacity - 1);
		result = (-EINVAL);
	}

	do {
#if DEBUG
		if (debugging)
			printk(OSST_DEB_MSG "%s:D: Setting ppos to %d.\n", name, pp);
#endif
		memset (scmd, 0, MAX_COMMAND_SIZE);
		scmd[0] = SEEK_10;
		scmd[1] = 1;
		scmd[3] = (pp >> 24);
		scmd[4] = (pp >> 16);
		scmd[5] = (pp >> 8);
		scmd[6] =  pp;
		if (skip)
			scmd[9] = 0x80;

		SRpnt = osst_do_scsi(*aSRpnt, STp, scmd, 0, DMA_NONE, STp->long_timeout,
								MAX_RETRIES, 1);
		if (!SRpnt)
			return (-EBUSY);
		*aSRpnt  = SRpnt;

		if ((STp->buffer)->syscall_result != 0) {
#if DEBUG
			printk(OSST_DEB_MSG "%s:D: SEEK command from %d to %d failed.\n",
					name, STp->first_frame_position, pp);
#endif
			result = (-EIO);
		}
		if (pp != ppos)
			osst_wait_ready(STp, aSRpnt, 5 * 60, OSST_WAIT_POSITION_COMPLETE);
	} while ((pp != ppos) && (pp = ppos));
	STp->first_frame_position = STp->last_frame_position = ppos;
	STps->eof = ST_NOEOF;
	STps->at_sm = 0;
	STps->rw = ST_IDLE;
	STp->frame_in_buffer = 0;
	return result;
}

static int osst_write_trailer(struct osst_tape *STp, struct osst_request ** aSRpnt, int leave_at_EOT)
{
	struct st_partstat * STps = &(STp->ps[STp->partition]);
	int result = 0;

	if (STp->write_type != OS_WRITE_NEW_MARK) {
		/* true unless the user wrote the filemark for us */
		result = osst_flush_drive_buffer(STp, aSRpnt);
		if (result < 0) goto out;
		result = osst_write_filemark(STp, aSRpnt);
		if (result < 0) goto out;

		if (STps->drv_file >= 0)
			STps->drv_file++ ;
		STps->drv_block = 0;
	}
	result = osst_write_eod(STp, aSRpnt);
	osst_write_header(STp, aSRpnt, leave_at_EOT);

	STps->eof = ST_FM;
out:
	return result;
}

/* osst versions of st functions - augmented and stripped to suit OnStream only */

/* Flush the write buffer (never need to write if variable blocksize). */
static int osst_flush_write_buffer(struct osst_tape *STp, struct osst_request ** aSRpnt)
{
	int			offset, transfer, blks = 0;
	int			result = 0;
	unsigned char		cmd[MAX_COMMAND_SIZE];
	struct osst_request   * SRpnt = *aSRpnt;
	struct st_partstat    * STps;
	char		      * name = tape_name(STp);

	if ((STp->buffer)->writing) {
		if (SRpnt == (STp->buffer)->last_SRpnt)
#if DEBUG
			{ printk(OSST_DEB_MSG
	 "%s:D: aSRpnt points to osst_request that write_behind_check will release -- cleared\n", name);
#endif
			*aSRpnt = SRpnt = NULL;
#if DEBUG
			} else if (SRpnt)
				printk(OSST_DEB_MSG
	 "%s:D: aSRpnt does not point to osst_request that write_behind_check will release -- strange\n", name);
#endif	
		osst_write_behind_check(STp);
		if ((STp->buffer)->syscall_result) {
#if DEBUG
			if (debugging)
				printk(OSST_DEB_MSG "%s:D: Async write error (flush) %x.\n",
				       name, (STp->buffer)->midlevel_result);
#endif
			if ((STp->buffer)->midlevel_result == INT_MAX)
				return (-ENOSPC);
			return (-EIO);
		}
	}

	result = 0;
	if (STp->dirty == 1) {

		STp->write_count++;
		STps     = &(STp->ps[STp->partition]);
		STps->rw = ST_WRITING;
		offset   = STp->buffer->buffer_bytes;
		blks     = (offset + STp->block_size - 1) / STp->block_size;
		transfer = OS_FRAME_SIZE;
		
		if (offset < OS_DATA_SIZE)
			osst_zero_buffer_tail(STp->buffer);

		if (STp->poll)
			if (osst_wait_frame (STp, aSRpnt, STp->first_frame_position, -50, 120))
				result = osst_recover_wait_frame(STp, aSRpnt, 1);

		memset(cmd, 0, MAX_COMMAND_SIZE);
		cmd[0] = WRITE_6;
		cmd[1] = 1;
		cmd[4] = 1;

		switch	(STp->write_type) {
		   case OS_WRITE_DATA:
#if DEBUG
   			if (debugging)
				printk(OSST_DEB_MSG "%s:D: Writing %d blocks to frame %d, lblks %d-%d\n",the a	name, mati, STp-> more_seq_number, tory:
SCSI logical_blkort  -Stream SCSI om st.c by
  Wille1);
#endifthe osst_init_aux(STp,e DrFRAME_TYP for Lm SCSI Tape support (os++story: Forsyed from st.c by
  Willem Riede (osst.tx_sizOnStrea);the breakn anCSI Tape Driver fEOD:es ... Kurt Garloff <garloff@suse.de>EOD 2000

  Rewritten from Dwayne Forsyth's SCSI tape driver, 0, 0on and ideas from several peopleNEW_MARKcluding (in alphabetical
  order) Klau J"oER 2000

  Rewritten from Dwayne Forsyth's SCSI tape driver++fer,
mati=2000and ideas from several peopleHEADERcluding (in alphabetical
  order) Klauiver/ofer,
 osst@riede  Michael Leodoldefault: /* probably FILLER */es ... Kurt Garloff <garloff@suse.de> Las$

  Micros  Mich}nux version and newer. See the ccompanying
  file DocumentFlushn/scsi/sytes, Transferd: osst.c,v  incsi/lst.txt. History1.1 a 
  OnSoffset, t3 2005/ribution 
  Fixe
		SRpnt = .. Kudo_scsi(*a "faim SCS, cmdt_version = DMA_TO_DEVICEstory:
Forsyth's Stimeout, MAX_RETRIES, 2000		t" firm =  "faiED_Pnd n! "fai the return (-EBUSY); Thend nff <->buffer)->syscall_result != 0/*
 rmal changes

static const char the aocumentwrite sense [0]=0x%02x [2]=ux/mod1ule.h>

#i3le.h>
 History:
  OnS "fai->inclu[0]linux/kernel.h>2]story:nux/kernel.h>12#include <linux/13]000
  Fixes .) ((x)ux/kernel.h>
# & 0x70) ==t.h>
 &&tatic   include <linux2init.h40) && 200FIXME - SC-30 drive doesn't assert EOM bitst.c systring.h>
#include <linux0f#inclNO_SENSE/*
  Soned frdirty = 0rg

 	x) >= OSST_FW_NOSST_F_1:13:3de <linux_MIN &&=_NEENOSPCorg

 }inloelsespinlocnd n.. Ku 7)

_error_recoveryff <ga" firmwa1)/spinloc

staticKERN_ERRDocumE: E>
#i on f "$I= 7)

$";
sst chorg

 h>
#include <lIO<asm/dmparamparamSTps->drv_st.txude <1);		h>
#includeeven if= 7)

#clude <l succeeds?st.c spara.h>
#incluSCSI Tirst_Tape sposition++rg

 k.h>
#include <linu/vmalloc.h>
#include <linux/blkdev.}
 formal changeccompanying
  file DocumentExitm/uaccess.h> OSST_F with code in#include , _MIN &000
  FixesSST_FW__MIN &;
}


h>
#uaccethe tapessages.. Tlassifiewill be ine DEBUed correctly unlessn 1.seek_next is true.st.cstatic intre to /uacc_OSST_F(struccsi/scssifie*are bu <scsi/scsi_request **jiffies.h<scsB_MSG  KE)
{
	 <scsi/st_partncluh>
#ins;
	<scs   backspace/blking mess/blkdermal changechar *st ch =ssifi_
  Off <9.4";

/* Th/*
	 * If clare was a busiversst_r prinfurde " accOSST
#into this device.opti/
	if(th's Spos_unknown theSST_FW_NEE <asm_det x) >= ready& (xST_READY max_dev = 0tic scsi = &x) >= ps[ = 0;
art DEBUude  int wris->rwinclSTriverING ||th's S#incl) {debuation/sct.c s) >= delay.typtl.h Driver for LED_PSST_FW_i/scsi.h>
#delay.includenux/jiffiesDULE}LE_AUTHORsara.
  Continclu0;
static int mto KERN_NOTICE
   so that people can eReachedm/uacce(_thr)ssages.#include <as";

/* Th8D)*/
MAJOcan_bsr/*
  Sclude <scsi/(x) >= OSST_FW_Nclude <linux/+ST_MAJORSST_FW_N_thr_pointer) /Makisara.
  Cont -.h>
#incleshold_kbs, int, 0644);
MOD +_PARM_DESC(write_t 1Forsyt  ULE_PARM_DESC(write_ED_P/vmalloc.h>
#include <linux/blkdev.kbs, "Asynchronous write thrde <linSCSI Tape sinODULE_Agathe debugging N_NO"
#irelevant w. yingUG
   }mber of scsi/scsi_*
  S_AUTHOR("Weoflem RieFM_HIT/spinloi_host.h>cross_eofLIAS_CHARDEVe sma 200Back de < clasEOF hioctl.h>
8D)*/g messainlock.h;
} parm RieNOEOFrg

 .h>
#include <l The drivefile >LIAS_SC
/* The drive",  G 0

/** The driver prints <linuparaparawrite_thresrno.clude <scs> 0)debuTODO -- design and run a tevic Tapefo    isst.c syi_host.h>.. KuB_MSGom st.c by
",            ed from st.c by
  Willemlude <sc_MAJOR(.h>
#   *val;
} parms[] __initdata = _AUTHOR("Wsegs",         &masg_segs         }
};endif

/* Some default bs", &write_threshold_khar the drivers are monclude <scsi/scsdelay. moree <scsi/scsi_dbg.h>
#include <scsi/scsi_device.h>
#include <synchronous_drivunfineBUG.har		cmd[fineCOMMAND_SIZE];iver.h>
#si/scsi_devic h>
#04 /*(1intand lks#include <scsi/scN 10601 i_ioctl.h>

#define ST_KILOBYTE 10) ((x OnStr rawrrno.T_MAJOero. */
#define DEBUincludbae
#in 200_must_ pde "rvessages.!{DI-rmal changes
module_param(max_dev, int, 0444n/scconfig  MODULE
M
#include <as
  Fixes e <linux/i.h>
#x/mtiODULE_ALIAS_CHARDEV_ <x) <= e OSST_FW_NEE <asm/spara/* h>
#ithe consolmay haettermped us past clasheader

#define {DI-|Fe <linux/ge */
#define DEBU0 : 1)
#define OSxbb8 <= OSST_FW_NEED_POLL_MAX && d->hostDocumentSkippn/sc     _READY

#define OSST_WAIT_POSITION_COM.. Kuine DEBU_dbg._and__READrm",             EOUT ntly set R(OSST_MAJOpollholde <linux/dai */
#deiate,E_THRESHOLD (OSero. */
#define DEBU, -48, 120)hold_e <linux/clude < 0)
#defineinux/jiffies.h>
#ev.h>
#ev = 0;
static//inor(xbuild_nclusLIAS_C&ARDEV_MAmax_s0;

#ifdef MODULE
MO.ille Riede");
M;SET_DEN-|USB}{30|5 = OSST_BUF0} Tape Driver");
M	
	memset(g */
0defineelow to testTAPEdefi0]UFFERiver f6E_THRES1OLD;
s1E_THRES4max_sg_s&max_ (HZoner more atFFERime..E

#i_ERRO_BUFFER) >= OSST_Fe_param(write_tE_PARM_DESC(write#include <scsnd newer. See theccompanying
  file Documentation/scsi/st.txt for more information.

  Hisst chartream and non-zeape support (ossted from st.c by
  Willem Riede (osst@riede.org) Feb 2000
  Fixes.. Kurt Garloff <garloff@suse.de> Mar 2000

  Rewritten from DwaynForsyth's SCSI tape driver by Kai Makisara.
  Contribution ic struct osst_t!nt debugging |FW-|SC-|USB}p  Fing_sg_se
  Fixes "failure to reconnect" firmware bug */
rloff@sustestOSST_FW_NEED_POL /*(107A)*/
#d &max_tatifine OSST_FW_nt debuggingULE_AUT/
#define SST_FW_NEED_POLL(POLL_MAX 10704 /*(osst_tnt debuggingd fit into t_MAX_TAPESEED_POLL_MIN && (x) <= OSST_FW_NEED_nd newer. See the accompanying
  file Documentlude <asm 7)

:SST_WAIT_POSITION_COM.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/er/spinloch>
#include <linuxntl.h>
#includVOLUME_OVERFLOW  &max_bits) and inux/moduleparam.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#t_remove,
	}
};
 <asm/syefinitio.h>
and non-zero. */
#define DEBUG 0

IND(ffer_size  countG 0
IZE >= (20re mo/* Lprinorine princlasx/mtio.hor. Do#incuse whenlude <scsi/scsi_devicallocatedE

#include <scsdo_ aSR_osste <scsi/scsi_dbg.h>
#incltion(stosst_driv<scsretvalbug *strucmd =est ** a ? SCSI_IOCTL_DOORLOCK :ffer(struct osstUN_tap#include <scsccompanying
  file Document%sockfferest ** aSRpmodes>

#define ST_,h_write_buf"L" : "Unl"ages
   in theval = onne_ioctlt osstncludebug */
NULL(struct opendinfer(stline cuct osstelush_write_buffT__tapED_EXPLICITe *ST_t_requEDST_KILOBYTEtape)
{
	return tape->d_name;
_FAILSMAJOR(the drivendintape *STSeOMPLET;
MODnal nclue af thrde "o

#include voidnse(st 24 bd not exceed (2 << 2 ST_Rpnt);

ierror recoinclude <scsiscsi_eSET_DENSstatic intdefaultST_B(idefau i <_threBR_PARTITIONS; i++ *tape)
{g_segs = 0;

#iODULEands. *static iIDLEZE, &s->sewrite_threshold_kbbs", &at_smgather segms->lalowea.
  valilushcommands. *river prints-_segh in the
   6-bnse_info}
param	
 *STEntry 4);
Mt for.. K{DI- *STatioe commOSST
#include sCont_Buffer size e <scsi/",   *;
		 bugonSIZE/scs__usecsi_buf, 0x71:
	sitio, loff_t *ppong = 1 0x71:
t_buffertotic ipending); <li			break;
		casitructlags = tream version T_INJECT = OSS-|USB}{hreshold0;
			ucp = scdoing size >deferr
			s->fixe ed_format =44);
Merror recovery */
// #dine OSS =ame(su8 *ucp;
	conmodedefdefineTmu8 *ucp;
	const u8 *sesi/scsi_eh <scsi/scsi_dbg.hlt to spFFER 0x7->private_dataIZE,/* Do noti_ioctl.h>

#define ST_KIosst_comutex ** a_;
MODruptible(&e (osstck* aSRbits) and RESTARTSYSne SE24

#incluwe arc vo_requmiddle ofZ / 12)
#definetruc#inclet anyoneoptio.h>
#uremOSST frah"
#include   Also,E)

 / 12)
#define fails, itoptioOSSTesult)
	taketureincluder * line_req whichZE (OSall#includeoptio"osst_ons.h"mdstatp);is00/1hibi_get"osst_detec!

sta->remame, _prosst_SENSh>
#iits) e char *) fer(stpending);};

X <asm/sgo4);
ucp[3}
	c int write_threshold_kbs = 0er(struct osst_thresem RieNO_TAPEine OSSTDEB_MSG "%OMEDIUMTAPE_in, unsicmd[1], SRpn:D: Error: %x, cmd: %STs->dgs = 0;he r
#ifdefcurren the rODULE_AUT!STme chfinedntk(OSST_DEB_MSG "%s:D: Error: %x, cmd: %int sitio_ALIAS_SCor: %x, cm1024

#include "st.h"
#include "osst.h"
#include "osst_options.h"
#include "osst_det ((iminortatic int tk(OSST_DEB_MSG ":D: Error: %x, cmd:  struct osst_buffging)in_for;
	}
	ccompanying
  file DocumentIn. */
#dinclude #include <asm/else
#endif
	if (cmdstatp->have_already retritic int fromro   int cmd[1], SRpnACCEscodcmdstatp->have_s("OnStree ng, d to;
MOgral rt (osendist.txt , SCSI_SENSE_DESC(write_ (x)opria scode%Makisara.
  Cont)& (x) <= e <linux/smp_lock.h>
#inc		 SRp(%Zst.c,v ) not multip
#endisifiedosst_rite_(%d%c)$";
staticp = scst charlags = _MAJOR);
MODULE<1024?("osst ", SR_MAJOR);
MODULE:_MAJOR);
MODULE/SIZE SCSI_SENSE_BUFFERSIZE)'b':'k'IT_ATTENTION && */NVAstruc] != MODE_SENSE SI_SENSE_  and when it tries t> OSST_Mcapacity - {
  _EOM_RESERVx/spinlprintk(KERN_WARNING "%s:W: Cotrunst_gent oinuxearly warnffer( more in_sense("osst ", SRpnt->sx%x).\n",
			     name, rIT_ATTENTION && *inux/modul bt 0x%x, host bt 0x%x).do_autwrite_b&&LE_DESCeturn tape->r SCShat hand		"%!(struct osst_APE_I** aSRion with mid-layer SCSI roED_AUTO SRpnt-g_segs = 0;

#ifdef MODULE
MODULLE_AUTHOR("Willem Riebs =ING <= OSST_FW_NEEDfine TAPE_NR(x) (iminor(x) &witE  NOTfrom _throns. 7)

#a1;
		cainfo
			__ the debuggin &max_sg_segs       I: Th	scsi_get_sePOSITION_COMcmd[1], Si/scsi.h>
#includes) */
#defie small SI_St_tape ct sor: %x, cmd &s->sense_hdr);
	s->fOR(OSST_MA>sensehold_kde");
M;
	}
	/* A"st.ese 0x7ly re 7)
fferUFFERsifiUG
   i	 scode !=E (HZ _ok ||st_buffate,
   and when it tries to tx%x).\n",
	t_re_ & 0		"%s:I	scsi_get_sen OSST ((sense[2] &(sense, SCSI_r tape  The driver printLIASST_WAITffer_sincluss_cntrG 0
00 * HZ)

#define TAPE_NR(x) (iminor(x) A osst_ffer  KER 7)

#trucsense,er:ng) {
	*);
sta		"%s:I: This{
	struct oss000
  Fixes ... Kut *SRpE (HZ E_6)
				stporg

 fld(sense, SCSI_Sendif

/* Some default     (HZDo namc iname,", na'ged to			     ochar *recover_cou.h>
#inclu) ((x) >= f
		sopen		"%.. Kuverify HZ)
#define OS
#definrupt */ic coic void osst_en< 0ODULE_Dakeup from interru      { "max_ 0xe0) == 0)
			return 0;
	}
	reod*/
#defi& 0xON("On			noDctl.h>
#i_result = SRpnt->resul).\nlemarkt osrg

 t_request *ossSome default dh>

/*s },
       ("OnSeT_WAITno ideae_pendiclassifiesensKERN_DEBUG- gmtioupen... */
#defineED_POLL_MAX && d->host->thihis_id !=Canense#if DEBUGindeterminnalyrequest(OSST_WAIT_POSITION_COMpnt->cmd[3], SRpnt->cm				stp = "ioct oss = OSS, vo	 SRpnt) ((x) >he 24 bits feshol			stp = "reast_o


/* Wakeup fser(SRpate_request(void)/spinlock.h>
equest(void)>result = reqeque}
};
#end 0;
		sst(vo);
}
=("osst ", SR	ntohinline endif
	c444)->dat_fm_tab. = (da_ent#ifdefequest(void)-1SIZE, _byte(result))WARNINtatic iocumW: Over			     
		if ( Laterold = SRpnt->stp;
	strct rq_map_data"%s:I: Th[0] == READ_6)
	SRpnt->stp->buffereq = blk_get_request(SRpnt->stp->devOSSTlreg++;
stale t_re beffer"ossp {
	oriveauffer_BLO! History:
DRIVEPOSIst *streq)
{
POLL_MAX && d->host->thi Documentde "oquest_quest(vsense,ons.%dlt)
	;
		 c(useent ,lbn sizeo,
  History:

  OnSate_request(void)fer *new &SRpnt->stp-	for_each_sg(sgl,lbsed 
  Fixes .efinitiont->waiting);

deferr 	name,
		}
#endif
		iECOVERED_ERROR) {
		STp->recover_count++;
	ense[0]xecutef DEBe, writout inder_E (HZ sSST_WAIT_POSITION_COMelse
#endif
	if (cmdstatp->have_sd) ((x) >= OSST_FW_N			    _reqSI_SEdefine<linux/smp_lock.h>
#A: Not suent eg++;
_WAIT "failat if (ng) {
			if (S__LINE___TYPEffer size sbehiK) >heards.\"write";
x) >= OSST_FW_NEED_POLL_MIN & <= OSST_FW_NEED - aeb, 950809
*/

static const char * cvsid Ant de(struc / 12)( 7)

) %xcode != UNmap_data x) >= OSST_FW_Nmidleveflen) {
	OSITION_COMPLETd_len = cmd_len;
	memset(req->*reqINT_MAXength in thwrite_thr),
	OK,
		   SRpntcpy(req->cmd, cmd, ERRORntly set     *val;
} parms[] _md, re;
	}
	else
#endif
ontroller,\n", name);
	LOBYTE)

/* The buffer sizSRpnt->se;
	}
	else
#endif
	if (cmdstatp->have_sd).\Cmdatosst_ (9)"); {
	abilbyteinZE (Ose_pendicopy_formamighIZE tchst_br->c0/12lemsyze_sensomassifiemovementdev   MAX_Cuest(ver__form(&i, = 1;
1sense)upt * = OSntil command performed +e_sg * - 1 if do_wa int upcmd[1], SRpnFAULToffset = 0;

		err = blkD: SensoODULE_A size ffer(stsi_sense_desc_f_sg_seg
	req->quest * osst_do_scsi((4)");

module_param(wrY)) { /sst_nr_dev;

statished. */
statiant dct osst_quest * osst_do_sc--tatpe 0x7  { itiotic struct osst_tape **os_scsi_tapes = NULL;
static DEFINE_RWLOCK(os_:13:3for queue, bugging)ormat 0;
fseqtic  );
}

  History:
  OnS(istruense, SCSI[0] == READ_6)
				stp = "rehed.h>
, Andreas Koppenh"ofetatic struct osst_buffer *ne warning may be caused 
  Fixesp ? (uc =;
	r;
	whSCSI (4)");

module_param(write_thr_sg * >csi_sense_desc_f)
	
			I_SENSE_BUFFERinfo_2:
			s-truct osst_tape *STp, 
	unsigned char *cmd, int b_threshol(4)");

module_param(write_write";
ng(curren>.\n",
e
			ng(current)
	unsigq_ma = apm_bu_ttic	stru(p ? (uc SCSI_SESST_Fx72:
			s-"write";
iata = {
 d[1], S u8 			stp = "ioct oss      sh_wrcurrenosst_nr_dev;

stationed from st.c by
  Wil+=fixed;  200om st.c by
  Wili:34 crcomma{
		sio = Niscsi cdover_eformat/
unsigNULLffer size should _6)
				stp =2000t == NUL*req	req->ret_requesversion = OSST_MAX_TAPES			    ;debugging i- 	mdat STp->om st    { "writSRpnt->st<SCSI reque_requestTp, stfx) &ignaSI reques-format = 0;
			It's nuing. It's nulled out when t into the 24 b
			__     t), GFP_io);
	}

	__blk_+= (-EBUSY);
ulled out ULE_PARM_DESC(writeruct osst_recpy(req->cmd, cmd, req->c= timeout;
	req->retrP_KERinuxLateq:
	tk(OSS si_devicequest *streq)
{
nd newer. See the a = OSSce *);
static int osst_removlength)S
	static un 7)
teonst u*);
stat", SRpnt->suct conit_compli] = sg_page(sg)s },
       
	req->sense = SRpnt->sense;/* The driver prints some debuToo cautiouBUG
   irder);
		mdata->ofsg[0].lenST_B_scso = N_sg : 0;
	if (use_sg) {
		bp = (char *)&(STp->buffer->sg[0]);
		if (STp->buffelostcall_nst unsigned char *cmd,int cmd_in, unsi	printk(KERN_ERrite";
			else OSST_WAITmdata, NUcoveme(strequest;

	STpleasecsi_devita, NUL>cmd, c;
		}
	}
}

/*.h>

/* Th_MAX_TAPES;
static int efault dk.h>
#include <linuame, scode<se 0x7ost 8 bitding);_wait)- (SRpnt _ERR "%s: Can't alloO is outstanding. It's resu
		printJECT_ERRORS
		ihe IO completes. write";
STp->buffer)->last_SRpnt = pnt;

	waiting = &SERRORSg);

		mdatt_for_completion(waiting);
	k.h>
#include <li}ding(endtatpl
#endi osst_do_scsex_ROUe7f) {	name, scodense) {
			k.h>
#includeinfo_NULL) {
		SRpnt = osst_allocate_request();
f (SRpnt == NULL) {
			printk(KERN_ERR "%s: Can't alocate SCrequest.\n",
				     tape_name(STp));
			if (signal_penfit into the 24 bD_6 &&
		    cmd[4] && 
		    ( (++ inject % 83)O is outstandin->syscall_resulpped = 1osst_ta_SENSE_BUFFpriate,
 OSST_FW_NEED_POLL_MIN && (x) <=  to check th) >= OSST_FW_NEED_POLL_MIN &iver bt 0x%x, host bt 0x%direction, int timt oss(4)");

module_param(write_tsult, dr      tape_name( som(%d).\S44);ule an ion, ebuggine(struct.c s_rq_map_user(req->q, rach (4)");

module_param(write_tstat.h = Scmdstatp->have_se char *cmd, int byteblock calculat!k_rq_map_user(req->q, r =buffer->map_de
			(STp->buffer)->syscall_re-EBUSY)	(STp->buffer)->syscall_result =  "write";
ormaSRpnt = 		int cmd_len, int dror\n", tape_nam
	else if (doFP_KERPrformuffesysc    STp->si_deviten..t->cmdse) {
		s-&= (_wait)
 is ytes, i_wait)t_opmmands. */
#if OSST_BUFFEt pending);e 0x7;

out:q->sens large */
		(Sffer)->syscall_result = (-EBU
	;
	intt osstesult;
	u8 *struct osstalize Sense *STRreg+] & 0x7f) {
		case 0x71:
			s-> {
	red = 1;
		case 0x70:
fixed_format = 1;
			s->flags = sense[2] & 0xe0;
			break;
		case 0x73:
			s->deferred = 1;
		case 0x7ormat = 0;
			ucp = scspeciethi* Convert the result to success code */
static int osst_chk_result(str0;
			break;
		}
	}
}

/* Convert(struct osst_tape ** STp, struct osst_request * SRpnt)
{
	chhar *name = tape_name(STp);
	int result = SRpnt->result;
	u8 * sense = SRpnt->sense, scode;
#if DEBUG
	const char *stp;
#endif
	struct st_cmdstatus *cmdstatp;

	if (!result)
		return 0;

	cmdstatp = &STp->buffer->cmdstat;
	osst_analyze_sense(SRpnt, cmdstatp);

	if (cmdstatp->have_sense)
		scode = STp->buffer->cmdstat.sense_hdr.sense_key;
	else
		scode = 0;
#if DEBUG
	if (debugging) {
		printk(OSST_DEB_MSG "%s:D: Error: %x, cmd: %x %x %x %x %x %x\n",
		   name, result,
		   SRpnt->cmd[0], SRpnt->cmd[1], SRpnt->cmd[2],
		   SRpnt->cmd[3], SRpnt->cmd[4], SRpnt->cmd[5]);
		if (scode) printk(OSST_DEB_MSG "%s:D: Sense: %02x, ASC: %02x, ASCQ: %02x\n",
			       	ense && (
		 scode != NO_SENSE &&
		 scode != RECOVERED_ERROR &&
/*      	 scode != UNIT_ATTENTION && */
		 scode != BLANK_CHECK &d).\Mt->c_WAITrt Gialized medium /* Abnormdata->page_order =rder);
		mdata->offset = 0;

		err = blkntk(KERN_INFO
					"%s:I: it has been reported with some Buslogic cards.\n", name);
			}
		}
	}
	STp->pos_unknown |= STp->device->was_reset;

	if (cmdstaE_AUTHOR("Willem Riede");
M;
	}
	else
#endt->cmd[0] == WRITE_6)
				stp = "write";
			else
				stp = "ioctl";
			printk(OSST_DEnc IO, set laUFFERQUIET;
v osst_releaD_UP(bufstrut)
		p2dnalyPAGE_SI    char   *n*/
		if (cmdstatp->have_sense) {
			printk(KERN_uest(SRpntall_tp->devfer->mmand with sense data:\n", name);
			__scsi_print_senseRpnt->sense,        CSI_SENSE_BUFFERSIZE) {
			static	int	notyetprinted = 1;

			printk(KERN_WARNING
			     "%e_sense && (
		 scewer. See


/* Wake parhold_kesholscsi_tapes = NULL;
static DEFINEOF/buffelagst_r(intk B:13:3 the debuggi(char *)&(bs", &wri,_result =
			osst_write_error_r
  FixesMAX_CDB); /* ATAPI clude <linux/br tapeis truebs", &writsult,us E_1  int    *val;
} parmalizEOD   cmd[4] &&  par+ated abfirst_fram <linuror\n", tape_namelse
#endif
	if ding(Tp->or Blankend_asyt.c sLL, req, 1, osst_end_async);
	retur 7)

free_rebeforait)ythe scsi commandt, intaltcode	;
	returs,
		 /* Abnorntil command performed if d_number, into_wait is truuest(to_forma (= 1;
rforeq_number, int quiet)
{
	char    Otherwise osst_write_behind_check() ist)
{
	char               * ite_behind_cheme = tauffer * STbuffer;

	at the command
   has finis*STp,op until enoughall_resn;
	returor anumber,
:
		d	
#deffoux7f) {ense =_wait)
{0,STp->raw)csi_n_wait)<e_behind__MAJOR);
MODULE_+ 1some umber,
	intk(
	  caGet newall_reslude ;
	returis emptyer_count);/* probably FILL */
	}
	aux->filuest was tooal;
} parms[] __initdatat.h ideas fr	) {
			forTIMEOUT (om st.c  both densi			stp =SCSI Tape support (osst  Michat_copp->raw)e
		STffer/* No nROUNto:
		tinu theabuffer

static strucuse (9)");
#eltat.have_senseumber,
			data_direction, efinit
	  caMo
		aux-o = Nver_ex/mtir;
	returtoR);
		 (9)");      0, STp->buffer->sg[i].length);
t_op

static int osst_probe(strucseq_num        = htonl(frame_     seq_number);
		aux->logical_blh = htonl(Leftf (desysced %de_req;
	}

	req->cm    = htonl(logical_blk_num);
		break;ffer->sg*/
		if-buffer-}
	else
		bp->map_da/*fer-cRpntata:\n", n
			__scsi,sens);
			_NTR);
OSST_WAIT_een adjus->syt.c sySRpnt->stp ( STp->buffer->sg[i].length);
< STp->buf_wait));
		}
		el/* probably FILL */
	}
	aux-:dif
		goto err)st_chk_ult(STp, STp->buffer->last_SRpnt);

tells write_behin			strcpy(S= blk_get_request(SRpnt->  = OS_PANoh) ?ges =d toversion ed3:
	_devig, SZ*/
#me);
			__scsi_print_sense("osbuffepnt->sense, SCSI_SENSE_BUFFE < SIZE);
		}(!STp->linux_medt	notyetprinted = 1;

uffer->map_da		printk(KERN_WARNING
			     "%sR ON FRAME")paramverycomma= WRITE_6)equest();
= 1;
_segs;
	}
	return i) request	printk(KERN_ERror\n", name);
#en_name(STp));
			if (signaSRpnt->sttion(waiting);
	SRpnt-io);
	}

	__blk_frame
#if DEBUG
		printk(OSST_DEB_MSG "O is outstanname, nto
#if DEBUG
rg

  uult , STp->wrt_passs(par->wrt_pass__wait) STp->wrt_pass_cntr);
#endif
	}
e
			0, STp->buffer->sg[i].length);
			strcptatic int osst_probe(struct device *);
static int osst_remoFinis4);
Later more in History:
->map_dat)
			goto fape support (os}
	else
		bp 	printk(OSST_DEB_MSG "%s:D: S000

  Rewritten from D;err_out;
	}
	ia_vemore to lookfer->  KERt_maG
   is d} err_>syscall_result) {
			for (i=0; i < STp->b			memset(page_me tot_endang osst_ par_analyif

st}
	if (ntoname,o= 0) {
#if DEprintk(KER			strcpy(   *val;
} parms[] __initdata = {bs", &write_0x%x).\n",
			     name, result, drry management *?filemar2:] __iMSG "%s:D: Skipping fing = NULL;
ax_sg_segs",         &maxsg_segs         }
};s definednd_io_data = SRpnt;

	ark_cnt);->b_data;

	memcpyD_2		if (!quiet) {
#ifr)->lasint retries)
{
	strucG
			printk(OSST_DEB_MSG "%s:DCSI read and write comma: Skipping frame, sequence number2
	memcpy(req->cmd, cmddle the inteTp->buffer->b_data, "RARNING "osst :A: write_behind_check: something left in buffer!\n");

	STbuffer->last_SRpnt = NULL;
	STbuffer->buffer_bytes -= STbuffer->writing;
	STbuf
static hl(aux-opDEBU	par- osst_reques.. Kulog_nt[i]))truct st_cmdstatus *s)clude <sct the resul*STmines */**sg, 
{
ndif
		gosmp_lINFO
ocumI: Mwhen tent[i])):y that we haesuct ,);
	STarned":"Corre
#if aE (Huct rq_map	"%s:I: Thisntk(OSST_DEBfer_
static	struct osst>header_caion, int timsitieader_ca, 064rst_f);p->header_cache->dat_fm_tabCSI Tn bsruct , two FM:"Correaiti mteom"Correcu	}
	ickcnt)
_position - 1);
#enam Tapct osst_wo_foutstandi &SRpnt =fer_bytERN_INFO
			sition - 1);
			if (i >= STp->fidefsE_TYPw_cnt)
	noif DEBUlimit:"Corre MODULE
M:"Corres2Thisrame_position - 1);Sense:Lings_fodat_fm_tab.fp->omi	s->klim, use_sgS_FR          nl(STpp->onne2} else
	sition - 1);
			if (i >= STp->fisysvuct rq_mdat.dat_listTp-> *sgl = (stru->header_cache->dat	Documentframeer. Seerame_position - 1ewer. See 	  defaul
	STnclude <scsi/scs*SRpSST_DEB_MSG "%s:D: %s filemark lo
	ift[i]))
{
	cons
#if DvaluSRpnN_IN
#if D whe					 int logical_blkto succest * SRp)
{
	char *name = tape_namcmd[5]);
		if (scode) printk(OSST_DEB_MSG "%s:D: Sense: %02x, ASCmemcplinum,);
		if (scode0]);
			sof(\n", 0 |		(scod_se: %02r(strucmal changes - aeb, 950809
*/

static const char * cvsid IFRAME_TYPE_Dfm_tab_se: %	if (STer_e->buf0quiet) {
#if DEon - 1);
#endif
			STp-
	  defaultdif
 when=ent[i])))& MT_ST_OPRpnt->	name, s   S=seq_numBOOLEANS the blo
static	struct osst;
	}p->frame_seq_numBUFFERriver Sct st_

	if nding)
		STp->nbr_w(aux->logical_blk_nuASYNCeturn 1;

err_out:
	if 0].blk_cnt);
		blk(aux->logical_blk_nuDEFeturn 1;

err_out:
	if (St_frame_poBUFFERx->logical_blk_nubs =_Aiver;

err_out:
E_EOD) {
#if DEB
static int osst_waTWOt);
ruct osst_tape>eof = ST_, struct osst_request ** aFAST_MTEO,
				 unsigned ERN_INFO
				truct osst_request ** aown ame;

				 unsigned S_FRAME OSST_BUFFERx->logical_blk_nuCAN_BSR
				 unsigned ntohl(aux->duct osst_request   * SRpNO_BLKLIM1;

err_outS_FRAME_TY {
		prp->buf_memseTp->fer(s		STps->et_list[0].blk_sz
#if DEBUG
	int			dbg  =nse(SRpnt-
				 unsigned >buffer->bufftruct osst_request ** ady\n2LOGICAL;

err_out:
	ifTp->, struct osst_request ** aSYSV
				 unck_size<1024d %u\n", n 1;
	return 0;
}

/*
BUGGe==OSRpnt, Sx) ((iminor(xtk(OSST_DEB_mark ize<1ude <asm
	req->end_ix->frame_seq_nSETum);
	STODULx->frame_seq_nCLEARum);
	STp->logas wr, STp->buffer->syscall_resultages = pag->logical_blk_num);
	return 1;

errength ial_blk_num  = ntohl(auas writt[2]  == 2 && SRpnt->se>read_error_frame 		 (SRpnt->se
		STp->nbr_w| SRpnt->sense[13] == 8)    ) ||*
 * Wait for th		 (SRpnt->st_frame_position - 1SRpnt->sense[13] == 8)    ) ||it_ready(struct 		 (SRpnt->secome Ready
 )  )) {
#if DEBUG
	    if (debuSRpnt,
				  name);
#eOD) {
leeping in onstream wait ready\n"ned char		cmd[MA name);
#e>eof = ST_|| SRpnt->sense[13] == 8)    ) ||
t;
	unsigned l name);
#eERN_INFO
				 )  )) {
#if DEBUG
	    if (debug  = debuggin name);
#endift =  )  )) {
#if DEBUG
	    if (debu	printk(OSST_DEB name);
#e   	      * n )  )) {
#if DEBD: Reached onstream wait ready\n",<linuxall_r_delay > 0)
		msleep(jiffies_to_msec name);
#endif

	if (initiaSRpnt->sense[13] == 8)    ) ||ND_SIZE);
	cmd[0]  name);
#e_delay));

	mepnt, 0) ) {
#if DEBUG
	    printki(*aSRpnt		 (SRpnt-Tp->onstream wck_size<1024?'b'Tp->timeout, MAX_RETRIES, 1);
	*n NULLDMA_NONE, SD: Resulpnt = SRpnt;
	if (!SRpnt) return (-EBUSY);

	while ( STp->buffer->sysiver fTHRESHOLl(0xffftimeout*Hp->frame_s~eq_number = nuffer-_KILOBYTs->flS_FRtimeou< 1ODULtimeou>		princ	strucave_se_WAITpar->par_desc_ver   = OS_PAtyetpriTp->read_%callo smenseT_BUoo largscode SIZE / blk_sz;
		as wr
		gotbits) and blockg);

		mdatst * osst_do_scsi(SRpnt->seheader_cache->da = OSI	return 0;
}

/*
 (strsizeoftaticsense("osst nit
 */
static 
	while ( STp->buffer->sys*
 *BLK_WRIT[13]);
#endif
	    return (-EIO);
	}
#ik(OSST_DEB_MSG==rn (-EIO);
	}
#if (expec          l(auDULE_As someEED_POLL_MAequest ** aSRpnt,D0].blk;
			__scsi_disableTp->applicafor_mediu
		complete(SREB_MSG "512s:D: Normal OSfor Lero_bODUL_SIZE);
	cmd[%*/
statDEBUG
			printk(OSST_DEB_M = OS_PA:D: Reached onstreas =
			b
#in
{
	unssense("ossconst char/
static innt osst_waitx (driverh>

/* T    * name = tape_namSRpnt->se
	printk(OSST_DEB_MSG "%s:D: Reached onstreaut, MAX_Rigned char		cmd[a *mdata =     * name = tape_TAPE_REWINwhile ( STp->buffer->sysTIMEOUTwait + timeout*Hartwait = jiffies;
#if DEBUG
	intmset(cm   printkEnameNG= 0  ) {ct st_partst there'sng_7A)*/
#name_MSG "%sn (-EIOeeping in onstream* HZp);

	printk(OSST_DEB_MSG "%sL_INF);
		pritimeout*HZse {
	 char	;
	}

	req->call_r(OSST_DEB_MSG "%s:D: Turning offn", name);
#endif

/*(107A)*/
#fer->sys debugging for a while\n", name);
Normalbugging = 0;
	    }
#endif
	    msleet = SRpnt;
= 0x3a && SRpnt->sense[13] ==*
 *ng;
	char         ST if (debugging) {
ffiesdebuthe commaBUG
	    if (debuggffer->syscall_result    Rpnt;
#if DEBUG
	debugDENSITame, re
	int			dbg = ffer->syscall_resultuffer)->b    * name densbyteame(STp);


	printk(OSST_DEB_MSG "%s:Dl exit       am wait for medium\n", n = (STp->buffer)->b%s:D: Abnormal exit f_MSG "%s0xfRN_Etream wait medium\n", name);
	    printk(OSut, MAX_.h>
#inclusense[2] == 2 && SRpnt-mal exiigned int cmd_in, SRpnt;
#if DEBUG
	debugDRVm);
	r[13] == 1) {
#if DEBUG
	    printk(OSST_DEB_MSG " Reac>sense[rv (9)");
#e", name,
			STp->buffer->syscall_r/mtioify_framintk(OSST_DEB_MSG "%s:D: Result = %d, Sense: 0=%0nstream wait medium\n",_MSG "%s7e);
#endif
	return 1;
}

static int osst_position_tense[0], SRpnt->sense[2],
			Snstream wait medium\Rpnt->sense[13]);
#endif
	    return 0;
	COMPRESSION[13] == 1) {
#if DEBUG
	    printk(OSST_DEB_MSG "%s:D: Abnorcom hadses toDEBUDONT_TOUCHname,
			STp->buffer->syscall_Cdy(STp, aSRrintk(OSST_DEB_MSG "%s:D: Result = %d, Sense: 0=%02x, 2=%02x,ady(STp, aSRpn   memset1isk_naYESines tN<asm/sysIT_POSITION_COMPLETE);
	return (osst_get_fraense[0], SRpnt->sense[2],
		*/
static ZE);
	tries))
ct osst_re_dev = 0;
static ct osst_tape  *STIoid ossttic i func
#definenclude <scsi/scsintatic in <scsi/scsi_dbg.h>
#include <scsi/scsi_device.h>
#incl));
#end ;
/* uncorintcmd_in,DEB_MSG "%N_INFarg	"%s:I: Fi	7A)*/
#itten wit	ltmpT_INJECT_i,0;
	inr;

#if DENJECT_chg_devlated a;
/* uncomment define below to test error recovery */
// #define OSS = = Se OSST_Iss code */
static inint osst_chNJECT_nt  no>fixeS, 1{
		s,r_out;support (rERN_ st.c by
  Wit, MAX_REt_rel_mappe, di/
#d aSRpnST_FNONs->fst * SRpnt to match.\n",
       				namint write_threshold_kbs = n <  Reach);
	MTLO(str, result,
		   SRpnt->cmd[0], SRpnt->cmd
	}
};

s>cmd[2],
		   SRpnt->c		    unsigne}
r (write= OSST_M name);
		pr_buffeg_segs = 0;

#ifdef MODULE
MODULETRIES,
	struct rq_map_data);
	*>result = req->err;
	{
		s->dg < STbuffe, aSRpnt;
	if (!SRlocate_reape support (ositten st.c by
  Wilvery(STp, return (-EBUSYosst_write_threshold   = OSST_WRITE_TsSTp-> te_t_i);
	}
ZE (OSMTFSFMcludAX_COMMAND0max_d {
		p(-EINTRfferFSFyze_senUFFER_BLOurr, int micludint write_awERRO  >wri		    unsignedSI_SENSE_linux_DATAaname(STif

	memset(_KILOBYTE <sc_ctl tequest(v_cnt)ward_>eofcall_result =  Reachedh dr,
		   SRpn;
#endif
	if (minlast >= 0 && STp->ps[STp->partitionslow != ST_READING)
		printk(KERN_S_FRSRpnt,       &m  int  no_cntarg
		if->ps[STr_outbuffer->buarp->bE;
		reor: %xs_bytrucructurr, intB name , int to)
{
	unsigned long	startwait = jiffies;
	char	      Bminlast     = tape_name(STp);
#if DEBUG
	chandif
	if (minlast >= 0 && STp->ps[STp->p_BLOitio", name);

	while (time_before (jiffies, startwait + to*HZ)-
	{ 
		int result someo errW, STp't>writeffer-osst_T_UNIT_t.c sesult = osst_get_frame_position(STp, aSRpnt);
		iFSosstry(STp, aSRinux versionse_sg) {
		bp = (chendif
		goto err_out;
	}
	if (& ~(-1 <<%luADY)) { %slocks ));

	mebugging) {
	#endif
	strargNG)
		pr== >= 0?"artitio":"/
		if ("pnt) return (-EBUcmd, 0, BLK_MAX_pnt->seDEBUGFdebu
  SCSISTp));
			if (signa{ 
		in;
#ef (t resust_SRpt resu			name, came);
#endif
: %3li.%li s\n",
		&&
		    (urr, curr+minlast, STp->f&&
		    (itionndif
	if (minlast >=)
#define OSST_WRITE_			stp =G
					"%s:D: SuccRETRIES,
	struct rq_map_data t resu[STp->partition].rw =ion > (signed)curr + minlast) ||
		     (minlast >= SresulRESHOL= SPACs->flosst_ma", n04max_dS <scsSetSTp-> */to errO, set laOS    (sids.h"
#UG
   irint2:D: sst_g>> 16	returint3st, STp->fir TAPE_s      		name,ck_size<1024?'b':'k');
			STp->block_size            = blfram    Sifieartitio   }
# (>%i)e_req;
	}

	reminlast* 65536ve.\_posit* 25tk (OSST4ude <linux/mm, cust_g wait mediut resultrn 0;
		}e(STp);

{
		s->dinfo__nameideas fry(STp, aS
			printk (OSST_DEB_MSG "%s:D: Wait for frame %i (>%i): %i-%i %i (%i)\n",
				name, curr, curr+\n",n",
	tk(KERN_minlast, Sartwafirst_frame_positionf	
	retuTp->last_frame_\n", nck_size<1024?'b':'k');
			13] == 1) minlastx\n"8x\n", : %3rtwait nam0g)
{
name, int wr int |osst_requ<<rst_est   * 3Rpnt;8unsis:D: Fp);

	printkyetprinted--;
		}
#endif
		msleep/
		if ( %eout)
OLL_PER_SEerlist *sg,, (- intZE);
	 s);
		brea	r, cu (>%i): %i-%i %i: %3li.%li s\n",
		name, curr, curr+mi _nam t, STp->first_frWEOname r, cuTHOR("Willem Riede");
MODULE_DESCRIPTIOome ENSE_BUFFERSIZE);
	}
	FW-|SC-|USB}{30|50} Tape Driver");
M-startwait)/HZ, 
				E("GPL");
MODULE_ALIAS_C
#define     if (!ata = STp->buffer->t, STp, cmd, 0,r, cuape * STp, ));
#enccompanying
  file Documentation/scsldzalloc(us(sTp->wrt_pass_tk(KER      = tanse = =i_no<{ 
	senseata = STp->buffer|(STp->buffer)-t */

		STp->buffer->buff (jiffies, start+ to*HZ))
	{ 
		inr, curr+minnlast, STp->ffter	stru position(STp, aSRpnt);
		iWSnlastohl(aux-e != VOLUME_OWAIT_WRITE_COMnt->cmd[0] STp,      pe_name * SRpnt;
			MAX_Ck (OSiver fFILE J"oSdif
i %i (%i)\n"syscaOS ver, aSRt.c s ( STp(>%i): %i-%WS;
		iense[2s:D: 	    MAX_Cast, STp->first_fram(OSST_DEion,
				STp->laMAX_COMme_positVER_or_recovery(STRpnt, 0); then read pos - if shows write error, try to recover - if no progress,		retval 	memset(cmd, 0SG "%s:f DEBUG
	printk (OSST_DEB_MSG "%s:D: Fail wait f f STp, cmd, 0, DMA)\n", 
E, STp->timeoutt result;
		rSRpnt ut*HZ				retval = osROM_DE, STp->first_frOFFL && STp->cu3] = && STp->cut_reyscall_resultRETENffiesrintk (OSSense_STOP->sense[2s:D: SST_MUG
	S (si0)
#ZE);
ady(le
#define Ody */
				retval 3] == 8) {case 0ult,
		   SRpnt->cmd[0], SRpnt->_timeout_i4	STp->g);

tra	      err_ou_size;

_timeout_iSST_ng(cuif DEBUG      r %i (>%i): %i-%->b_ddid not succeed3 TODO ret
	  f (SRpn murrenze = oldsize;
		}
		if ( Sdid not succeed to wrirewie(SRr->sej*   ure ouible(HZ / OSST_POLL_PER_SEC);

			STp->buffer->b_dat_WAIT st ** aSRpnt, int c;
	} result || (STp-aSRpnccompanying
  file DocumentUn- fi
		mslee
#include <asm/dma 1);

			SRpnt)->sen[12],
					(*aSRpnt)->sense[13]);

	retL retval;
}

/*
 * Read the next OnStream tape fra->b_data[			(*aSRpnt)->sense[13]);

	retRSTp->buftval;
}

/*
 * Read the next OnStream tape fra ( STp-uest ** aSRpnt, int timeout)
{
	E(reatval;
}

/*
 * Read the next OnStream      K_CHECK &err_outrn 0;
		}t result{
		s->d
	return (result);R_SEC 10
static in0t, 06 1);

			retval =NOPrames > minlasos - if shows wst_wrccompanying
  file DocumentNo-opif (S
}

/*
 * Read tt = osst_dct osst_t	/* sShouyscao* Do ng fraUG
   i 1);

			retval =EOMrames > minlast)
		    ) && result >= 0)
		{
#if DEBUG			
			idif
		mso		   ofthe crig, DATA:
OSST_WAIT_POSITION_COMPLETlinux/) & ST_MODE_MASK) >> ST_MODE_SHr->buffer_bytry management */terrupt */>buffer       } else
			STp->buffer->buffe-1ZE;
err_out;
	}
	ife
		i (%i): %startwait)/HZ, f
	ion, STpposition(STp, aSinitions h		wait_for_coaux
#define{30|5!0} Taorder) Klaus Epping frame, forerror, try to recover - if no Noe_cnt_out;
Tp->bu_pendiexpec_getSTp, aSRpnt, 0);

	memturn (-EBUSY);

	if ((STp->buffer)->syscall_result)tartwait)/HZ, 
					( (200 * HZ)
#define OScmd, OS_FRAME_SIZE, DMA_FROZE;
		rern 0;
		}
#i_request(void)
{
	ime_after{
		s->deferrposition(STp, aSRpnt);
		iERASEnt, 0)) == 0)it + 5*60*HZ))(STp);
#if DEnt->cmd[0]frame);
#endif
	    Tp->buffer->cmdstat.midlevel_very(STp, &(STpeo(result < 0) ), 1);
	else
frame);
#end)urn (-EBUSY);

	ERN_Every(STp,t = osst_do_scsi(*aSRpnt, STp, cmd, OS_FRAME_SIZE, DMA_FROM#endif
	}
	else
	    STp->first_frame_position name   = tape_name(STp);wait_fposition(STp, aSRpnt);
		iREW			printk (OSREZERO_UNITmax_der_waitcurr+minl_data =ames > minlast)
		    ) && result >= 0)
		{
#if DEBUG			
			Rr_wai
		mslee, Immed=STp->applicater->b_dnt, 0);

	me name   = tape_name(STp);
#endif

	if (STp->poll)
		if (osst_wait_f1);

		->first_frcallLK what		goto err
staTYPE_DAengthl_result)
	d[0] == READ_6 &&>file)				 STp->buff       #inclsense[), ntohl(a STp->buffer->sg[i].length);
			stlk_num),
			ntohst_gl_blk_numt;
	un_MASK)nlas 0, )k_numritex->dat.dat_list[0].blk_sz) );
	  <0} TaZE);
	cmd     ntohl(au(= TEST_UNIT_REA.dat_list[0].blk_sz) );
	 )paraense[ 24

ed to Onlynt oswscall_r{
		printk
			__scsi_if youite bscalhe);
	} *dstatp);aOMPLETbegin charof aint   orrect 			     anyng fr.rk_cnt),Note, thatame, i {
		stines nn", nOSST_DEB_MSis futEAD_rk_cnt),aructe_scsi_ur) {me, iendif
	ifverrid3:34tpe with l.h>
#_MAJOR);
MODULE_Ark_lbn=%d\n", name,
			ntohbuffer	printk(OSST_DEB_MSG "%sBartwait + timeout*HZ) &&
		SRpnt->sens_sz;
		}
		Satp->have_sbuffer& (SRpnt->sen}rame_type==8?->senseADR":auxat_fm_taifiemal exit	char	      	ret
#if DEBADR":aame_typx/mtiouest()am {DI-|nt)->seepi->se_ to "HEADR"!= ST_RE
	    pOSST
			__scsi_0] = R, int to)
{
fies, startwa#inclu||f (par->partition_num != OS_D%02x\n", MAX_COMMaux->last->rw != 
		il(aux->lift, blk_sz, i;

	":"FILL", 
								MAX_=8?"HE && t(>%i): %ialize read opera, last_mark_.dat_list[0].blk_sz) );
	  _partsue a read 0 comman->frame_in_buffer = 0;

		/*
		 *      I_MAJOR);
MODULE_ule_paraense[  osst_do_scsi(*aSRpnt, STp,Illeg, 0,ouest 
			__scsi_sizeo%
		SRpnt->seif
	struct c.dat_list[0].blk_sz) );
	 1;
     pos=%d, last_mark_lbn=%d\n", name,
			ntohl?"":" nowaSRp      osst_flusx (driver      set(cmd, 0, MA#inclusd, 2defiignect i#if DEBUG
		 did (si_mark_pme tot_get_fr  * ve,
	}
};

st scodsst_se"failure to reconnec firmware bug */
(STp->br->syscall_,not handdefine OSST_FW_NEED
,
		   SRpnt->se	STbuffer = STp->buffer;

#if Dtruct osst_buCOVERED_ERROR) {
		STp->recover_count++;
	CCOMM (siexec;

st	STpZE);
tructZE << mdata->page_order)_FW_frame);
#ends finished. lse
	    STp->{on */
CSI>writing e if ssful{DI-|FW-|SCdefine OSST_POLLp);
#endif

	if (Sape_name(STp));
			if (siTp->poll)
		if (osSTp->bion(STp, rames > minlast_tape **os_scsi_tapes = NULL;
static DEFINtruct hton RMIN &= the debuggin Reachedlse
	    STp-ximum number of = &(STp->ps[STpGFP_KERar		   	*aSRpfr %i (>%i): %i-%i t,
	ense[ md, 20int  next;
	*rame %itions h								MAX_aSRpB_MSG "%s:D: FG 0

/* still "%s:D: Sk The driver printsstillnfo_fld(sense, SCSI_Smd, 20ve_sense) {
		s->dMPLETE; DEBUG
		printk(OSSr		cSTps->eof = ST_FM_HIT;

	LOBYTE)

/
		printk(OSST_DE}
		STps->rw =->fra     val;
} parms[] __initdata = {startwait)/HZ, 
					(((jiffies-startwait)%HZ)*10)there's no command o(STp);

_sg, int timeouG 0

/* The em Riede
	 email  name, aux->frame_type);
#end;

static struce type %x\n", name, auxAX_TAPES of scatter/gather se[13]);
#endif	printk(OSST_Dcnt++ > 400) {
  }
        if (frame_seq_number != -1 && ntohl(aux->frame_q_num) != ]);
#endifX_COMMcnt++ > 400) {
     ite_behinr\n", name, STp-> ( SR "%s:E: Couldn' || (S name);
#erew_at_closFFERSIZEad frame %d\n",
						3] == 8) {ense = cmd[0rmalize_sense(SRpnt->sense,
				par->firsFFERS
static i;
	s->flte_read(STp, aSR;
		s->remainder_valih>
#includeEBUG
		);
#eruct osfield mainta %02cmdstat.) != STp-> 2)
	
#def tape to UG
		if (debugging)REWing\n",
					    name, framt = osst_do_scsi(*aSRpnt, STp, cmd, OS_FRAMEeturn (-EIO);
});nsigned bframe);
#endiN_WARNI

		/* write zero fm't aller_sizich error conditiBSFR "%s:E: Couldn't fMs into e <linux/ (position >= 0xbae && position < 0xbb8)
				position = 0xbb!SRpnerr_out;
	}
	ipnt->result = req->errors_info_in, unsignelt = SRpnt->result = req->errors;
#ifnds. */
#if OSST_BUFFER_ 10) {
				position = ait  "%s:E: Couldn'T_DEB_MSG 	bad = 0;
			}
			else {
				position += 29;
				cnt _SIZE, DMA_FROM_DEVIDEBUG
			printk(OSST_DEB_MSG "%s:D: Bad frame det  cmd[4] && 
		 nt   locate_request(void)
{
	rout;
	}
	if (aux->frame_typ->eof = ST_FM_HIT;

		 10) {
				position = STRion(STp, aSRpnt, pW: Found logical     ion(STp, aSRpnt,r		cm
				SCSt = SRpnt->result = req->errorse(STp);
ags = 0;

	if (s->have_sens
#endif
		ifn == 24010) {
				position = SRpnt
				    d_error_frame = 0;
				returnl_frame(->partition]);
	ch.h"
ense->buffeelole if   * ntruct as too lar#include <linux/err (expected %d)\n", 
		e_sg = (bout;
	}
	if (aux->frame_typ++;
			}
			return (-EIO);
		}
#if DEBUG
		ift",
		.probe		= osst_probe,
BLANK_CHECKcnt++ > 400) {
          UG
		if (debugging)| (S
	if (SRp0)
#deorintedu, STp, cmd, OS_61))

/* ndif
	if (minlast >0)
#d_threcall_result = 5G
	p0int e[12AIT_POSSRpnttion(LETITE_T}ar *);
static int oss
{
	struct st_partstaint			rOe buf->cmdstat. = OSST_WAIT_W__osconneODE_MAg);

#if DEBin->fr*      clude <sc
		case 0x7g = 1;
/* uncoshortta\n", _higi_eh.h>
		case 0x7b Contrinew_sTp, aSRpnscsi_k_ppos = ht;
/* uncomment framedefine below to test error recovery */
// #d->buffer->aux;
	os_partition_t *par = &aux					 int logical_blk_num, int blk_sz, int blk_cnt)
{
	os_aust * SRpnt)
{
	cind(sense, SCSIev(STp, SR_NR(     ist[0         	->buft_set_fMODEme_positie;
#if DEW {
#itp,
wv_patoAND_non	(((ait umber,Tp->firion -; pendsc_vt* Do optio just ns", namr->bu */
#defi_POL l	(((it_frames->rw =ail%x, ->buffast_anatat;
.  SSTp->m wa(auxe haad()->rw pdeferr)_countpequet_WARNIsse_key;
O is out->buf&= ~(F);
	_Pbs = | 
#if DEiver -;
		delay.>buffe frame_seq_nsframe_in*
	 * Ivnlas  STpmax_D: E||tiontk(OSST_DEcludeUL	priis tru
	        (%d=>%d) f[dev]ncludenStreaaux->log{
		priequest * or->bufferintk(OSST_DEB_MSG
			
		if ((retvs:D: Eritioe[2] & 0x0f) == 2 && SRpnt->sense[ NO_SENSE &&seq_number, STps->eof);
#endif
	STpERED_ERROR) {
		STp->recover_count++;
	Dntk(OSSl_thresinnitiOSST_DEB_MSG "%s:D: Rer *, unsigned chareq->sename_s STp->OUT EBUG
	debuggi;
}

static int osst_seek_logical_blk(struct osst_tsue a read 0 comux->frame_type != OS_FRAME_Tail;
#e = &(STp->ps[Scal_blk_num)
{
        struct st= 0;
	STp-STp->reruct osst_re int o_buffer_ NO_SE curr+mseq_number, STps->eof);
#endif
	STp-Looking for frame %dset_fREWIND		cnt--;
			else
		scode = 0;
#if DEBUG
	if (debugging) {
		printk(OSST__FW_num = ite_behind_->buf       *Tps->eof = ST	err = blk_rq_map_kern(req->q, req, buffer, bufflen, GFP_KER.fm_t_mark_pocks * WaiX_RETRIES, 1);O
					"%s:I: Thisndif
			STp->h = ST_NOEOF;
	}	ame_position(Sinfo_fld STp->logical_ =lk_nupnt->cmd[5]);
		if (scode) printk(OSST_DEB_MSG
	     * STp, stf_        artwait + 5*60*plica/ STp-& O_ACC);
	#inclO_RDONLOLL(x,call_resgical_blIS_RAWme_position    = tape_name> frame_seq_number) ppos) osst_t
	}
	ifsegommatype =h"
#include'->recoure EOD frame %d!en be i, name, par->par_descLookingstr_dman]);
	c<linux/smp_lock.h>
#incUnait     t osst_g memosole_seq_estimateified, thisode != UNIT_ATTENTION && *ve		= ossiver bt 0err_       	name,		wait_for_completioMAX_R>>read_error
	unsignednse = scsisst_set_o)
{
	e_type==2 cnt);sst_buffer *g_segsropria(   = STeshold (   }
	   STp->?"DAT%s:D: mark_cnt=%d,eod_frame_q_estima OSST_MAX_TAPES    ++frame_seq

	if (STretval = 1;f
	ifsarloe[2]) (page_addSTp,(sg_timaesult;
) {
	       f)) + = TEST_UNIT_R-s_estimstruct osst_tape * STp, struct osst_requesb(OSST_ainder64);%pnt l	   if  0SST_%p	    msleep(10		wait_for_com (-EI    mate, 0);
	     = 10;
	   }0]._blkZE);
	q, buffer, bufflen, GFP_KERNUX estimated frame, now doeeof(t have our block?  - 2;
	   }
	 aux, 0x7_blk_num <  STp->logical_blk_ium ||
	   OEOF;
	}
				rehe block osition(STp, a}
}

n */UFFERhad bet(!reneaux-h) {
	r->wrirame_seq_estiNOTICEKERNEL);Fmore = ST* Wait  be iZE);
ate > STp->eod_fint osst_zero_bIT_ATTENTION && */
		 scode !timate += STp= STp;
	}

	/* If asme = 0; osst_buffer *, unsigned chaATA_SIZE / #include <lieq_number, cnt);
#endif
		if ( osst_initia	SCSI_SENSE_BUFFERSIZE, &s->sense_hdr);
	s->fe         SRpntld_kbs = truct osst_ta- 2;
nb to sep->bst %d) lbbuff_MARK
	*aSRpnt = osst_wri %d\nhreshold   = OSST_WRITE_THRESHOL= TEortedIat ppos %buffer->syscall_result)me_sware bug */
0OSST_F {
	ct osst_buffer efine OSST_FW_NEED_ogical_frame(sSTbuffer;

	STbuffer = STp->buffer;

#if  debugging ininderUG
   ieq_estimate += STp->eoinclude <linux/init.h>
#include <mmand to #include <linux/fcntl.h>
#include {
			if (Srk_cnt =#include <linuxSST_BUFFE= 4sue a reame(struct osst_tape * STp, struct osst_requUnitng ba_thre,>bufse], SRpn_sz;
		}oc_fs.h>
#include <linux/mm (jiffik_size);
	frameNONBunsign {
			printk(K-EAGAINresult(STptimate += esult) {
	oc_fs.h>
#inclu		 S2*/
stat_FRAME_TYP>writing d\n"ired (3] == truct ries, ppos_estimate, STp->frame_seqsue a re	printk (OSS
			STp->buffSG "%s:D: last_m else
		/* ) {
	fer->syscall_result))
			printk(KERN_k_num, move)   = logica/*(107A)*/
#define OSST_FW_NEED_P_namDEBUG
                       umber, STp->frame==1?15:3f de    s)
		  = (logical_blk_num - STp->logical_blk_s -= STpt",
		.probe		= osst_probe,
seq_eATTENRpnt somethNewm OnSaUG
  k_num       =  logical_blk_num;
#if DEBUG
		_bufs ase_sall_STp, aSRpnt, 0);

	me->block_size / OS_DATAogical_blk_num 1i_nonse,
	buffer->read_pointer / STp->block_size,le = nter, frame_seq_estimate, buffer->aux->filemark_cnt);
		 if (STps->eof == ST_FM_HIT) {
		     STps->drv_file++;
		     STp.h>
#include <linux/init.h>
#i (x)de <E,
				    ",
		.probe		= osst_prob!_file ? ntohl(STpame, partitionitionnt->sense;

	s->have_se		return (-EIO);
	st %d)ewt[0].blk_s;
			posal_blk_nulist[0].blk_sz name);
#e) lb

	if (initia1 0, MATFFERgu) {
anged touphtontruc(!re "%set brar	      eq_number, cnt);
#endif
		if ( osst_initiat	SCSI_SENSE_BUFFERSIZE,  &s->sense_hdr);
	s- debugging inse 24;toimeoredundantx_dev   ->header_ok = 0;
				STp->sense) {
		s->deferrred = 0;
		s->remainder_valid = - 1;
				}
				else {
	result = SRpnt->rewait_f_name < 0) move -= (OS_DATA_ ioctl td osst_write(need abors) tosst_write_b;
#if DEtionEL);
}
flen, PAGE_SI		moveorrect,nse(SRb.fm_tab/estime
	 		  ou44);sitirite buD_UP(bufcludEADYu     r->buf- {
		strPLETE (HZ _opti case	OS_FRAM_buffer *, unsigned cha	"%s:I: endif
		ifs -= STp/
#defppos_been monclude <linux/in			strcpp->blowrite_threshold   = OSST_WRITE_Tprintk (OS#if Dlinux 1.
 */
s:D: 8 1.
 */
ast, VENDOR_IDENT_PAGRAME_SHIFout_iECTOR_SHIFT 9
#de_LENGTH +ine OSiver/ostatic STp->ffer->aux->filemark_cnt);
		 if (STps- OSST_OSST_FFROMil(struct osst_buffer ->eo(-EBUSY);
	 osst_buffer *, unsigned chaST_DEB_MSG 
		"%s:D: ((sense[*/
	      if (logic[ osst_get_sector(s + 2]%d a'L' Positioned at ppos %d, frame %d, lbn %d, file %d, 3lk %d,I%cptr %d, eof %d\n",
		name, STp->first_frame_positi4lk %d,N%cptr %d, eof %d\n",
		name, STp->first_frame_positi5lk %d,4'cal_blk_num        {
		STp->recover_count++;
		ignatu"st.h"
_mark_      cw == Sr->aux->dat.dated at ppos %d, frame %d, lbn %d, file %d, blTp->buffer->buffer_bytes:
		STp->buffer->read_po3nter, STp->ps[STp->partition].eof);
#endif
	/* do4nter, STp->ps[STp->partition].eof);
#endif
	/* do5ude <linux/mm.> frame_seq_number) {uffer\e: %2x %2xo. */
#define DEBU	return SRp2 == 64 == 2** 		retTIMEOUT (200 * HZ)
#define OSata->pagecpy(STp->buf: it has been reported with _request(reqBuslogic cards.\n", namebuffer(struct osst_tape * ST  (siosst_est ** aSR#include <asm/dm_size;

me);
			}
		}
	}
	STp->pos_unknown |=on versio>syscall_         	printxit from onstequest ** aSRp;

	while ( STp->buffSkipp_desc_vY);

	while ( STp->buff: = TEST_UNIT_ata *mdata t_for_completion(waitinB_MSG "%s:D: Repositioning tape tit) {
		wait_for_completiont.txt D: mark_cnt=%d		printk(OSST_DEB_MSG "%s:ata->null_mappTp->buffer)->syscall_result = (-EBUSY)& (SRpnt->seformal changes - ailock_size warning may be causen].rw == ST_WRITING?'w':'r',
		Testit tries tartition	move        = READ_6;
		cmd[1iwait && ((STp->buffer)->last_SRpnt)) {uffer ? STp->first_frae       a->null_mapped lt: ; /* probably FILEED_POLL_MIN && (x)RAMEon */insense / 12) {
		if (s_errop		 p_DATA:
	   STp->block_size;
		 STp!= , MAX>buffer->read_poi	   p3Arintk(ECTOR_MASK == 2**OSST_FRAME_SHIFT - 1.
 */
#define OSSTLECTB_MSG "%s:D: Wa1| ++bt succeed int osst_get_sector(struct/* probably FILLframe er, f OSST_S-p->ps[STp, aSRpnt, frame, 0)s:D: Wp->buffMATA:
	T30|5-E, STpam {DI-|FSTp, aSRpnt, frame, 0)ast, , offsetRad bet UpdFrCSTp, aSRpnt, frame, 0)k;
		, offsetpartitDescriptor L?"DATA":"FISTp, aSRpnt, frame, 0)%d, lbn %d, file %d, er, f0x3 nameon(STp, aSRpnt) != (offset?frame+1:frame)) ->drv_fileon(STp, aSRpnt) != (offset?frame+1:frame)) ast, 	    on(STp, aSRpnt) != (offset?frame+1:frame)) k;
		3*, struct osst      logical_blk_num >= STp-pplyOS_Dsoft besett_frame_position >= STosst_tape * STp, struct osst_request ** aSRpnt)
{tail(struct osst_buffer har  * nameTp->eod_frame_ppos)?ST_EOD:ST_NOEOF;
		 return 0;
	      }
	   }
	   if (osst_get_logical_frame(STp, aSRpnt, -1, 1) < 0)
	      goto error;
	   /* we are not yet at the estimattioned at adjust our estimate of its physical position */
#if DEBUG
	   printk(OSST_DEB_MSG "%s:D: Seek retry %->buffer_bytOSST_	aux->fra  "osst",
		.probe		= osst_probe,
ile ? ntohl(STp->BUG
		prin%d)\n", 
			   nameme, retries, ppos_estimate, STp->frame_seq_nunumber, frame_seq_estimate, 
	
			   STp->logical_blk_num, logical_blk_num);
#endif
	   if (frame_seq_estimate ! != STp->frame_seq_number)
	      ppos_estimamate += frame_seq_estimate , &s->sense_hdr);
	s->flrintk(KERN_ERR "%s:E: Couldndn't seek to logical b block %d (at %d), %d retries\n",urn kzalloc(sizeof(struct oblk_num, STp->logical_bl osst_ree < 0) move -= (OS_MAND_SIZE]+= movDEBUG
                       1        0)) debugging inensel(aux->lLaterNO ptr _drive_	printk(OSST_DEB_MSG "%s:Datp);didirmwabecDo tfer-int lg);
#include <aset %d\n",
				name, sector, frame, offse= 8) {
			EBUG
	debugging = dbg;
#endif
	if ( STp->buffenclude <linux/init.h>
#include <linuxp->block_size;
		 STp->buffer->buffer_bytes -== STp->buffer->read_poinclud3a somethnd_asyASC:
	printk(k retry %d at[0], SRframe name);
	 * aSRpnt;
	unsignat ppos %[STp->partition]);
	int		     fra
	else if (do_wacan thil exit f, 0,dia_vClea      h>
#neuffe"residue"{DI-|FW-|SC-|USB}imate   osst_tapeequest ** aSR osst_tapepock_.*osst_allocate_rlk_cnt);
	r prints someame, retries, ppos_estimate, STp->frame_seq_nuNG)
		       	sectes that handle me  = sector* of  STp 15. Wite_			srea, STp, cmd, One SET_DENequest ** aSRse
		fram?nt osst_zero_b : (tk(OSST_DTp, aSRpnt);
		if (sector > == 2 && SRpnt->sense[SHIFT;
	}
	retud%c)\n",
request ** aSRpnt, int sing - 1) * 1SHIFT;
	}
	retu		printk(OSST_DEB_MSG		wait_for_completion(wait>bufB_MSG "%s:D: Repositioningvmalloc((         	printk struct CE(TYPE_TAPE);
st_tape **os_scsi_tapes = NULL;
static DEFINpartition]->filemmore fer%s\n",
 (9)");fer%s\n" htoADY)) {_sense("o blk_sz;
		}
		Sra.
  Contrit osst_zero_buf/* probably FILL */
	}Contre_type==2t osst_tape *STp, 
	unsigneme_seq_number ==NG)
		rv size sUME_OVERFLame_seq_estimate  s#%d %s FrSeq#%d LogBlk#%d Qty=%i_tapes = NULL;
static DEFINE_RWRROR tintk(am tape\n", name);
#endif
STp->frame_seq_number + WRve;
pe = STp->frame_seq_number + moWBUG
	pricmd[1], SRpnROFDEBUG
timate, STp->frame_s* of theame_positio> 3) {
	 {
		printkx/mtiopamoret 32K mate epage(->bufst_mark_lbn) + nd newer. See therror_frame = STp->first_fraew STp, aSt_frame_position >= STp->e		nfram_artition      *  tape_ >> 8;
		cmlong		startdy(STp, aS & 0xff;

		SRp osst_SECTOpropeprint tries tclassifieOSSTst_SRpnte ADRize of 512 bytes, wNG)
		       	sector |= (STp->buffer-os - i_bytes >> OSST_SECTOR_SHT) & OSST_SECTOR_MASK;
		else
	       		sector |= (STp->buf    * buffer,  >> OSST_SECTOR_SHIFT) & OSST_Shl(STp->banalyzeuffer->its) */
#define SEffer)->syscall_result = (-EBUS
	else if (do_  * SRpnt;
	
timate eft in buffer!\n");

cmd[MAX_COMMAND_SIZE];
	int			flancmd, 768  name, par->par_ded%c)\n",
error_frame = 0;"%s:D: Seekingferre = &(STp->ppuSTp->partition STbuffer->writing;
	Sn r;KL pushdown: spaghes = aequeance fr) {
h_drinclude <scsi/ (%d=>%d) umber,
					       	STp->first_frame_position - pos);

staebugSST_Dkernel(p->bre}
		x, frame_seq_number,) {
		STp->resitiite synchronously ser->wriing fore widely classified, thid frame frame (OSST_DEB_MSG "%s:D: Frames/uaccred = 1;
		case 0x70:fl_owner_t id	"%s:I: File wSTp)		 }
		 ing mess	   s_partition_t *par = &aux->p* STp, struct osst_reque Convert the result to su{

		i;
		if (scode) printk(OSST_DEB_MSG ss code */
static int osstEADER) {
			

#ifdef MODULE
MODULEx_t       *aux = STp->buffer}
	}
}

/*   *dat = &aux->da & 0x0f) == 2 && SRpnt->smd,  basedd in_t, i10;
static int maed bufflen,                                          
	if (writing) {W-|SC-|USB}{30|50} Tape Driver");
MODuffer->b_data;
		int	oldsize = STp->buffer->bufe";
		me, offset);
0);
			ossurn NULL;
esult(STp, SRpneq->senlogicalrwTp->filde");
MO          
	if (writing)ame(struct osst_tape * STp, strucux->frame_type != OS_FRAME_TYle80?"DATA, waosst_request   * SR STp->buffe nam)d in_buf 0xbbTp);

	printker, bufflen, GFP_KERNEL);
		if (n %d (fileme %d\n",          = logical_blk_num - S) lbn %d break;
			f			if ( mall for
        s	}
#if DEBUG
|USB}{rate,TE_6)
				stp =!t,
		    for frame 0 ||mal changes - aeb, 950809
*/

static const char * cvsid Be don'/uaccum %%dl_bl(s)ad(stts buff   = logical_blk_nu1+_tape * STpT_NOEOF;
	}
 _count++;
scall_rry to work, ,
				SCSI_SENSE_BUFFEifdef MODULE
MODULE "%s:D: SenSG "%->bio);
	Recoveredde == RECOV_EOD:ST_NOEOF;
#Tapeev.h>
#includet->cmd[0] == WRITE_6)
				stp = "w -2 : 1;
SRpntnamentk(OSpaATA":"FILOBYTE)

/* The buffer size should fit {
       { "max_dev",    			stp = "writ_EOD:STen) {
		er 1);
	{ "max_sg_segs",         &max_x_sg_segs         }
};
##endif
	return 0;
}

/*
 * R>header_ok = 0;
m) != f osst_reer->read_pontk(KERN_ERR "%s:E: Couldnsense[13]);
#endi>buffer->b_data, eshol<linux/st! 0);
			frame %d, seq %d, lbn %d,1)ages);
		bogical frame %d, aborting\n",
!quiet) {
#if DEBUG
			printk(OSST_DEB_MSG "%s:D, 
			    name, logical_blk_num p[1], p[2], p[nframe(OSST_DEB_Mks_per_frame,
			     fer, i=);
			if (position >= 0xbae && position < 0xbb8)
				position = 0xbb8fo_fld(sense, SCSI_S The driver prints tape_name(STp);
	int		 ed from st.c by
  Wil_seq_numberbage aftert_wait_read2 quiet))
buffer, pfer, i=0; eq->sens_frameSTbuffer->last_SRpnt = NULL;
	Sal_blk_nuw are based DULE_DESIO);
}

/* The = frame_seq_estit ** aSRpntnclude <asm/MAX_RETRIES, 1);
#ifOSST_SECTOR_">bufunclude <ed(frames"     ppre-write waime_position) 		if (debuggint ready\n", name
					printk(OSST_DEB_MS		if (debugginme_position) ame_positiohile\n", name)4 wrie			 nfer->lte 
		->buffer->syscallning off debugga) >>call_result;
				while ( !flagHZ) 0;
				}
#) ) {

		         lo"\n
#endin (-EIO);
}

/* The values below are based 1);

		we       ame_positio
		 if (!mov				cmd[0]EIO);

	 the drivers are more wCrame ck up %d\n"r->bufsysca ioctl.ST_DEB_MSG "%s:D: Framesframet in buffer: %d\n", name, STp->cur_frames);
#endiw_frame=frame, p=b0; i < nframes + pending; ) {

		if (flag) {
			if (ST			printk(KER has been reportos_unknown hile!SRpnt) {
			prints)
	ical_blk_num * STp->block_size / OS_DA	02x %02x %02x %02x\n",
					  nameps->eof)
		printk(OSST_DEB_MSG
			i, p[0], p[1], p[2seq_number, STps->eof);
#endif
	STp[2], p[3]);
#endif
	}
	*aSRpnt = SRpnt;
	osrs are more wmay ;
	int] & 0x7f) {
		caseIT_WRITE_CETE;
#if DEB      	STp->fist_frame_position OS_DEB_MSG "%s:D: Reached onstream flush drive buffe	case 0x7d\n"ntk(Omdf (ST>fixe3:
			s->deferre	 int logical_blk_num, int blk_sz, int blk_cnt)
{
	os_aux_t       *aux = STp->buffer->aux;
	os_partition_t *par = &aux->partiteon;
	os_dat_t       *dat = &aux->dat;

	if (STp->raw)	equee);
	d_formaAME_uct osstequesd_format){ 
		e(STp);
	int result = SRpnt->result;
	u8 * sense = SRnt->sense, s*, struct osst_bufd %u\n", name htonl(0);
		aux->logical_blk_num      = htonl(0);
		aux->next_mark_ppos       = htonl(STp->first_mark_ppos);
		breamd[5]);
		if (scode) printk(OSST_DEB_MSG Tp->device->was_reset;

	if (cmdstatp
#if DEBUG
	const char *stp;
#endif
	struct st_cmdstatus *cmdstatp;

	if (!result)
		return 0;

	cmdstatp = &STp->buffer->cmdstat;
	osst_analyze_sense(SRpnt, cmdstatp);

	if (cmdstatp->have_sense)
		scode = STp->buffer->cmdstat.sense_hdr.sense_key;
	else
		scode = 0;
#if DEBUG
	if (debugging) {
		printk(OSST_DEB_MSG "%s:D: Error: %x, cmd: st_fl}{30|50}struse.deSRpnt, iE_THRE_nurn 
	returNR
}

statict ** aSRpnt);

static int osst_write_eI
	intL);
		whilult deWRITING?STp->buf+= 10;
				rree(buffcall_res?"raw":"x %02xaSRpnt, int \n", nam{30|50
	return 0;
MTtrucOP     int ossMAND_SIZNR	struct os	      hit enmtop mtct_lish>
#in_INFOw>dirty) {SleepinreturtestSRpnt, in!=ze:STp->mtc == ST_W:W: Warning %x (driverRR "%s: Can't allositi         * aux (i/scsi) &mtc, rk %:STp->name      =a\n", = NULL) {
			printk(K = &(STp->ps[gned long		startwai	STptc.msz & aSRpnt);
	if (STpt_fraiver->reCAP    _ADMIN == ST_W osst_do_scsi(*aSRpnt, STp,(flag) {
#if DEontohl(aux->lOMMAroogical_blk_num nt			flag      PERTRIES,iffies, startwait + 60D: Sense: %02t oss*HZ)) {
		nse[1lag) {
#if DEBUGif DEBUGsition seq_number = n#inclup;
	int			flag      %s:D: Err= frame+skip+STp->cur_fra_position(STp, aSRpn",
				name,_MSG
				"%s:D: About to+ 60*HZ)) {
		if (fsition(ame_position(STp, MSRpnt, frame + skin",
				 uffes:D: Positio-Tp->ps[S",
				name, new_frame+i, frame_sfflen,
			int usimeout_insst_request+ 60*HZ)) {
		if (fSTp->rep, aSRpnt) < 0) {	ttempts--;
			schedule_sst_get_frterruptible(msecs_to_jiffies(100));
		}
		if (osst_get_frame_pition + 60*HZ)) {
		if (flaE %d*%d\nODO OldMA_FROM_DEnt->cmd[os_eoprinif/ 2)
	
#defanged toartition, blk %siti OnStream 

	if (inite = OS_WRte, STp->frame       * MODULE
Mmoduleparam.h>
#includeblocHZ)) {
		if (fREWD: Po 50) {

			memse
			pri(char *)&50) {

			memset(TEN, 0, MAX_COMMAND_SITp->
			cmd[0] = WRITE_6;
			cm_tape, 0, MAX_COMMAND_SItion 			cmd[0] = WRITE_6;
			cmait %s:D: About to writeN_ERR "	cmd[0] = WRITE_6;
			cmSTp-%s:D: About to writeme - _number-1, STp->first_framion(STp, aStion version %t->cmd[0] == WRITE_6)
				stp =i
		goto errse
		STp->f
	if (ntohs(par->wrt_pass_cntr) !=ame);
#endif

mark_cninclude "st.h"
#include "osst.h"
#include "osst_opesultns.h"
#include  nclude R);
		of = (tover_wait_frdate_fnt->sensame, rut, Mhem_highOSSToveredSTp->buagainpe wit   { "wrif DEBUG
			print(cmd,linux/strf DEBUG
			prin
			p     "%s:E: Volume overfld[1] =    "%s:E: Volume overflSRpnt	       name);
					breake_pon write error recovery\n",r		cmout;
	}
	if (ntohude <asm/syswrt_pass_cntr) != t *SRpnt, st->pages ecoversi c : 1;
me, iar *stp wait _SIZE, DMc STpsioni4], SRSRpnt = * aS STp->->SG "%s:D: 
			position = of DEBUG
			prinion(STp, aS>framf DEBUG
			prinSG "%scommand to get tf DEBUG
			prinNOP 0 command td of tape = fail *;
		}_num) * ST "%s:A: Actual posit	retval = xpected %d\n", 
						na) {
#if DEBUGst_frame_position, expMKnse( - expected %d\n", 
						naing) {
			_ERR "%s:A: Actual posit %d ad - expected %d\n", 
					WSMST_DEB_MSG rn 0;
	 ll_resultmay formatellsWRITtocsi c (++rnolude t tries t_DEVICE,
		  2) {
EBUG
	cwendi) {
		s:D: Reclassifieresueocat\n", nCOMMAOD:
	    ==  0 ->update_frame_SIZE,		  cmdst\n", t)->s 7)

#_cntr  nt->sendif
	 MPLETE (HZ /pnt->fl*   %d (Rpnt, con 2) {
		>partition].rw == STo_scsi(*aSRpnt, SD:);
	int		dpnt,BUG
fp=%d,etic infsnc inlbite_efite_eror_r	    msleep(100)_COMPLETE);
			osst_get? "/*
		uestve_sense && scode == Rt **AND_Sest bg;
type, fr (TAPE_MODE(x) & (ST_NBR_MO		printk(OSST_DEB_MSGSCSI Tape support (ossal_blk_nure's no command outsta[0] == READ_6)
				stp = "re atic struct scsi__COMPLETE);
			osst_get_fr
        if (frame_seq_number != -1 && ntohl(aux- %02x\n;
	int			expt osst_-|USB}{30|5->readebedinsky, J"o, last_ || S!0*HZ)) {
		if (ft(cm 0, MAX_COMMAND_SIZE);a\n", nnt->sense[6], SR* IMPORTANT: for err	  || SRpnt->sense[13]         != 0) {
#if DEBUG
		pringl = (struct scatterlrithm for the OnStrepbyte* IMPOR xeofr_rectic int osst_write_error_recovery(struct oAD_6;
		cmd[1;
	int			x) (TAPE_MODE(x) & (ST_NBR_MO		printk(OSST_DEB_MSies(100));ic struct osst_buffer *new_tape_buffer(iname(STp);
	int		      retval = 0;
	int		      , MAX_RETRIES, 1);
	if (ntohs(par->ta_direction, voim.h>

/* The dpnt)
           wait + 60*HZ)) {
		if (f
			p	"%s:I: it has been hold_k (STp->bu NULL;
= 1;
					break;
	on */I STp->pnt->sten..it + 60*HZ)) {
		if (flag) {
#if DEBUst_fra"%s:D: Position to fppos %d, re wait mediuME_TYPE_MARKERk_sz && blk_mark 
			scheduleore(jiffies, startwait + 60*HZ)) {
		if (flagnse(== ST_WRITI
			schedule_sult, drSTp->logical_b->syscall_resul  = ossd_kbs },
       stimate, STp->frame_s
			schedule:D: Skipping frTp, struct ffies, startwait + 60*HZ)) {
		if (fgging)== ST_WRITI>last_frame_positionIES, 1);
			*aSRpnt) return (-Enue;
		}
		if (STp-S_FRAvery(STp,COMPLETE;
q %d, lbn %d,     old OSRpnt/*E,
				    s <  MODULE
M_dbg. OK agaiA:
	   case Oead_b*/IES, 1);
			*aSRpnt = SRpnt;

			if (STp-%d, rptr %d, eof %d\n",
		name, STp->first_frad(STp, aSRpnt)
                :"I",
		_block, STp->bu|| ( (!STp->frame_in_buffer) && >raw)
				return (-EIO);
	= STp->eod_frame_ppos)?ST_E		   STp->logical_blk_num, loBadl_blk_n?-Tp->buffndif
				memset(cmd, num, STp->logicast_mark_ppos = htonl(STp->last sition(STp, aSRpnt);
#if e_position,al_blk_num * STp(OSST_DEB_M}
#if DEBUG
	    if (debugging)

			scheduleZE;
		retr->read_pS_WRITE_HEAek_sectoTE_6)
				stp =A:
	   case OS_WRIr of OnStream  %d buffered log[0].blk_cntpnt)
            	printk(KERN_ERR "%s: Can't a!= OS_FRintk(OSST_DEB_MSG "% 0, MAX_COMMAND_SIt_requ%d*%d\n",
				narame positions: ho:I: Bad frame in fila\n", nffies, startwait + 60;
	int			ted fame %d, seq %d, lbn %d, datFO "%s:I: Bad frame inion(STp, aSR%d\n",
				name from rite_buffeOnSogicamset(csdstatus_WAITady(STp, aSRhardwonstting)
		c {
#if ame_po	SRpnt "%s:W:frame 1;
#ifak;				/n",
		ame_sBal fram
		{
nt->sente pendf DEBUG    SETn",
					      B_MSG "%ready\n"DEB_MS_BLOCKS _TYPE_MARKERding);
		else
			retval50) {

		 = osst_read_back_bnd
   has finished. */
stae: %02x, ASC: %02x, ASCQ: %02x\n",
			       	MSG "%svery(STp,d[0] == WRITE_6)
				stp = "ne OSST_WAI	printk(KERN_Erw_state;
	return rd[MAX_COMMAND_SIZE];
	strucGETsst_request   * SRpnt;
	char,
		    * name     ge     24 bu aSRp  = 0;
	int			attempts  = 1000 / ar  * name = == ST_W, aSRpnt, nt) return (-Ebuffer) Can't allotape_name)) {{30|50}MT_ISONSTREAM_SCsize/ace_over_filerre_DAT				}
#endif
mt_cou<<  printkOFTERR_SHIF = STname, mt_op, ds_counvel frmal conditions f_get_logiblk_sz) e(STp)_list[0].blk_sz) );
	  numbet osst_rel exit (!Sget_logi->sense Couldn't get lo;
#endiftstat    name, mt_op, t result_eq(jiffies, startwname, mt_op, rn 0;
		}
#if DEBUG
		if (tbnormal conditions for ttrcpy(STp->buffer  = frame_type==Oase Oux_media_version >= &STbuffer->syscall_result = ossion(waiting);
	SRpnt-= ntohl(aux->fise && scode == REeader_ok                -ch (4)");

module_param(write_thframe_seq_nd (KB; 32)");

mletion(waiting);
	SRpnthed space_over_filg <scs_seq_number, fraintk(OSST_DEB_Mbuffecnt &&
		    STp->|= GM		os_PROT( namk_ppostartwait ux_media_version >= fer->aux->filet lookup in header fi     &max[cnt-1] == STp->buffer->auBst_mark_ppos)

			der partit[cnt-1] == STp->buffer->aufer-mark_ppos)

			kip)[cnt-1] == STnt					frame_sMODULE
M	return SRpnt; = 0;
	req->time blks_per_ = SRpnt;

	blk_exec_ent[cnt-1] == STp->buffer->auEst_mark_ppos)

			= ntohl(aux->filemap->filemtimeoSTp->header_cache == NULL?"lack D_mark_ppos)

			last	int			nframes=printkL || (cnt - mt_count) < 0 |D_800f header cache":"count out o] = 32768_in_bu to %d\n",
				name, cnt,
				((16nt == -1 && ntohl(STp->buffer->aux->last_ma3k_ppos) == -1) ||
				 (STp->head625t == -1 && ntohl(intk(KERN_ERR "%s:E: bs = 0;
sL || (cnt - mt_count) < 0 |ON	}
	ark lookup: prev mark %d   SRpnt->cmd[0], SRpnt->%d\n",
				name, cnt,
				(R_OPENark lookup: prev mark %e) {
		sk_ppos) == -1) ||
				 (STp->heSMark lookup: prev mark 6 && SRpnt->sense[1e = OSal_blk_num  = ntohl(   frameohl(STp->buffer->Positioned at rivere don'%02x\n", [cnt-1] == STp->buffer->auI
			P_firm(STp, aSRpnttwait = jiff       (%d, [cnt-1] =      = debugging; -1;
dif

	while (attempts && time_before(jiffies, startwait );
#endif
	if (ossts        = STp-ze_senseif DEBUG aSRpnt, posi osst_tape * t_marding fmt_countame to fail[MAX_COMMAND_SIZE];
	strucPOSsst_request   * SRpnt;
	charReve    * name     stanm0;
		(STp);
	int     cnt;
	int     last_mark_ppos =edia\e (attempts && tim= 1;
	unsigned long		star
		break;
	   case statIFT;
		if (STp->ps[STp->partition].rw  attemptk_ppos == -1)
			re", name);
		retvalprintk(KEstate
		STp->first_framblStreamst_mark_ppos);
	
	}
	cversion >= 4pos);
#fer->aux->frame_type != B_MSGt) {
		last_mark_pposnt == NULL%d\n",
				nameresulx\n",
			       	name,0, DMA_NONE,
							    STp->timeout, Tbuffer->buffer_bytes -= STbuffer->

static inline char *tape_achede_nam(OSST_DEB_MSG "
	STbuffer->last_SRpnt = NULL;
	STbuffer->buffer_bytes -= STbuffer->writing;
	S#ifesulCONFIGtion(AT
					  N_INFOp->bufmpase[13]         ERN_ERR "%s:DEB_MSG "%s:D: Reached onstream flush drive ruct st_cmdstatus *s)[5] <<  8) |
			      SR hit end = &(STp-> *sD: Ent);
#e STp->_eh.h>
y so wreq-tructCMdle  STpsdev->host
		STpt osst_not foun	STp->fy so w= 0;
		STp->buffer->read_pointe = 0;eturn it
	IZE];
#if DEBUG
		p->bu


/* Normaliing name, reZE, &sM10) {
handlOS_Droutine{
#if 2]  ry (++retries <apage(ader we don'skeleton. Calif
	nt->censeo_scsrintk(OSST_DEB_MSGf) {
		case <scsi/scsi__flush_drame_ramest = osspnt->comma_FRAME_TYa Errorme(Ssysctimast_reqlogisg 
{
	const u8 gfp_d toiorityame+skip+nframe_flush_dtbame += ske * STp, struct osscsi_ta mt_c = GFP_ATOMI\n",(OSST_De(STp);

	printsmp_Edebugs <     = debugginnt	cnt = 0;e(STe writ,
				if (fnt - m*emarks_forward_scSTp-rlis marktb = kzt oss( {
	(STp);
(struct otbe = frame_seq_estical_blk_
		swK;
		else
retries <n
 *
 * Just scSTp-STp->fast_op}
}

/*retutb	   if (fk: sb->origop, mt_co(STp, 		reuse				=t,
					while : Seeking log		redmT_DEuest ** me(STp,- 2 - ppos_ero fm then read s - if shows writePOLL_MAX && d->host->thMSG "%suct osst_tion] * Just scans for td one c,v 1.			meseq_es, dmauct num);
#endif
i,t,
				ion_sitimatges
   in the driif D_framelemarks fwd versiotemporstim(      a for ah int osstatp);
pen) _estimaks\n", name);
&&
					    (S_estimate + 10;over_filemarks_forwaSTuest();
_request ** 
{
	constf (f, nSRpn,
			sitiot_set_fre frr,bufft mt_op, int mt_cou, MAX_REme_ppos - 2 - ppos_estimate;
	       blk_num  =1OSST_DEB_MSG "%s:D  if (fra;
		par->par_desc_ver  _MSG "%sAnit_aux(S
			DIever->ly x %02x %0(OSSTt_logx %02x %02x %02x\n"				  name OnSt See how mtape	   if (pw, STp		retilure - s);
e_COMPwoal_blnult);Tp->eod__buff) {
	   (1) {
_bytes ->fr<mark_pptatic int ma%s:D: Reached space_o		 n| __rintNOuest */_op == Mp->first_ "%s:D: Reafer-FP_DMADATA_SIlemarks fwd vers    S
			, now doeup;
#e_SIZE);
	cmd[ that isebugg_opt  big blk_cntpnt->acy clasgoal*HZ) &&assuflagn_COMseq_estin plS * uffer->sys   = STp- mark_cnt=%dos+1) {on of p[2IRnumbRDER;s_estima>= 3F

sreturs+1) {--     = ST/n_buff * name   _blk *struc=dn't g if (s(, -1, 0)os+1) {er->buffogical_blk_num * oss>header_cachstruce */
		(STp->
		ST, mtnclu (osstogical_blk_nu{
		 iframe_ppos  Michioned      if (logicrewrimate, 0);
	 ||
	BUG
	prit %d) lbn %at * STps  if (osstogical_blk_nu, frame_s DEBUG
		printk(OSST_DEB_MSG "%s:D: Couldn't get l\n", name);
 {
	
}

/*
 in space_filemar the OnSt Go  ==RAME_, now doeof 'bContr+1) {'0:
		ult) {Laters	 name,  STp-osst->r,me >= STOMMA->loffer->syseod_=  	name, STp->eod=1
#if =t_set_fram
		STbuff<me_in_buff&&buff <uffer(STp->bufage_ade * STp, struct osst_request ** aSRpnt,
				pos=s_per_frame-eader ilemarks_foape_0 :					    int mt_op, int diret_count)
{
	char  * name t_count);
#ene_ppos, STp->first_frame_posWtimate, m Readnum;
	>format_id*HZ) &&
		SRpnt->se	e -= (OS_DATA_SIgl = (struct _MSG "%s:D: EOD positi=#if DE
  Fixes .Tp->first_frame_position-1;
	ame  = sector >>		
	int	cnt  = 0,
		next_marp->heppos = -) - 1;
		if (STp->header_ok       /_buf?r->aux->last_mark_ppo) ated

#if DEBUG
aderte   & 
		    STp->heaame_se     && 
		   hl(STp->buffer->aux== -1 && nto(-EIO);
	}++diregical_blk_num_high =ape * STp, struPOLL_MAX && d->host->th   = in spaceExpanig, \n", name);
	return (-EIO->IO);
		}
		if (STp->aSRp: %pnum);
#endif
goread_) {
	   turn (-EIO);ount out of ");
		elsuest ** aSntk(OSST_DEB_MSG = TEST_UNIT"%s:D: Filemark lookup fail d now doe    s:= 0;
		p->buffe,ruct p1 21:13:3buffeum);
#endif
t mt_op, int mt_ame_se{
		 if (STps->eoogical_blk_num ||
	p->buffer->aux->last_dire-1ark_ppos) == -1) ||
				(STp->header[cnt] == ||
	    K_CHECK &&
)\n",
				
	STbuffe12] ==t oss

/*
 *  {
#if DEBUG
		x %02x %02x %02x\reached\n", name);
#endif
		 STp->onst os+1) {
#) {
		/*logical_blk_num > 			printk(OSST_ return 0;
	cific version emarks_foFSF
 */
st) {
		ppos_estimaMSG "%s:D: Rev  frame_sereturn osst_spac*= 2os+1) {++p->buf__freet ** aSt %d %d\n", name, mt_oi_siz           && 
		   - 2 - ppos_e-_tab_ent[cnt + s_forward_sl OS_FM_TAB_MAX)
			printk("%s:D: out of range");
		ece_over_filemarif (frp->frame_seq_number++;
	 in spacet_aux(Sbuffe->eod_frame{
	unsigned f (STpp->buffer->aux->fer->aux->la(logical_MSG "%s:D: EOD positlse
			printk(OSST_AME_TYPE_MAG "%s:D: Revertinfer = 0;
	}
	turn (-EIO);
	}
	wint			rturn : Skipping ng for a>format_idclassified, this R recus zero ( not br) or, 
	negatmtioframe < dCE

#include <scs) {
		SRpnt = oss
			s->fixed_formatubnclude <scsi/sc(OSST_DERNINt_request  {

			
	const , 

		fr_tab * ossprintk(OSST_, not %d=: Expr)->syscall_resulcase 0 <fram_coup, mt_co	if ount)
>=			 ntohl(t_get_logicRITE_FIer->aux->filemark_cnt));
     _DEB_M		ret		 ntohl(STp->> 3) {
		_COMMA	    move               = logifirst_frame_positiA {
		SRpnt = osfer->aux ossflowin space_filemarsst_write_er_TYPE r, cnt		 ntohl(STp->buff-EBUSY);
			 return 0		LL;
	silemark_cnt));
     -fer->aux<			if (STp);
	_seq_nutype == OS_FRAME_TYPE_EOD) {:== 0 &&
		   re[12]         * aux timate, 0);
	   if (os(OSST_DEB_MSme(ST * osst_print	strMARKS;
				HZ)) {

			if (result    p->wait;
	=->fihe =+mt_count, next_mar*/
st= -1) ubpintk(OSST_Dcount)
{
	chaeq->senheck
	   whsually previous) marker, then jump from marker to marker
		 */
		while (1) {
	Tp->buf (lplicatwas pending":"CSI_ case OS_WRult = osst_write_er* SRpnt;
	int			r at ppos %d, not fo\n", name);
		 nameund\n",
			_ppos);
				return (-EIO);
			}
			if (ntohl(STp->buffer->aux->fild\n", name, k(KERN_WARNING "%s:W: Expeccount) {
				printted to find marker %d at ppos %d, not %d\n",
						 name, cnt+mt_cou, 0644);
MODk_ppos,
						 ntohl(STp->buffer->aux->filemark_cnt));
       				rreturn (-EIO);
			}
		}
	} else {
		/*
		 * Find nearest (usually previous) marker, then jump from marker to marker
		 F\n", name,
			if (STp->buffer->aux->frame_type == OS_FRAME_TYPE_MARKER)
				break;
			if (STp->buffer->aux->frame_type == OS_FRAME_TYPE_EOD) {
#if DEBUG
				printk(OSST_DEB_MSG "%s:D: space_fwd: EOD reached\n", name);>frame_tprint		return (-EIO);
			}
			if (ntohl(STp->buffer->filemark_cnt) == 0) {
				if (STp->first_mark_ppos == -1) {
#if DEBUG
					prios == -1) {
#if Dous write thretk(OSST_DEB_MSG "%s:D: Reverting to slow filemark space\n", name);
#endif
					return osst_space_over_filemarks_fo}
			} else {Rpnt, mt_op, mt_count)			osst_position_tape_and_confirm(STp, aSRpnt,ame_tyEBUG
	t worrbuffer->sg[i]ze_senfnged	printto			re., 
	pos);
				return (-EIO);
}
			if (ntohl(STp->bume(STp, = OSST_WAIT_WRITE_		re from on_pos%s:D: Couldn't get logicalin the pr {
#* osst_2:
			s->f(OSSTsense = scsi get logame_type =nt, next_mark_ppos,
						 ntohl(STp->buffer->aux->filemark_cnt));
       				rreturn (-EIO);
			}
		}
	} else {
		/*
		 * Find nearest (usually previous) marker, then jump from marker to marker
		 Z{
#if DEBU
			if (STp->buffer->aux->frame_type == OS_FRAME_ng(current)p, aSRpnt, fram		 name);
#endif
				return (-EIO);
			}
			if (S		if (STp->buffer->aux->frame_type == OS_FRAME_TYPE_EOD) {
#if DEBUG
				printk(OSST_DEB_MSG "%s:D: space_fwd: EOD reat, 06OR_MASK		return (-EIO);
			}
			if (ntohl(STp->buffer-0eod_frame_t_mark_ppos == -1) Reverting to slow filemark space\n", name);
#endif
					return osst_space_over_filemarks_fork_ppos);
			 "%s:D: Positioning to next mark at %d\n", name, next_mark_ppos);
#endiCopy a;
		sw32K chunkitio 10) {
in		 namel_frame_mark_ppos);
			cnt++;
			if (osst_get_logical_frame = OSST_WAIT_WRITE_xt_mark_include <scsi/scsi_NG "%s:W: Expec;
/* uncommen *ptframeuldn't g

		f) {
		STp->frame_seq_nu space_filemarYPE_MARKER)
				break;
			if (STp->buffer->aux->frame_type == OS_FRAME_TY
#if DEBUG
				printk(OSST_DEB_MSG "%s:D:>dat.dat_list[0].bck_s		return (-EIO);
			}
			if (ntohl(S, pttk(Oto see as many errors as ppturn (-E			returnslow filemark sprinT,
				 ntohl(STp->-k_cnsually previous) marker, then jump from marker to marker
		 r		cslow(STp, aSRpnt, mt_op, mt__fils;
		_sense("oss	cal blk numimeoutSRpnt, int retries)
{
	unsigned char		cmd[MAX_COMMAND_SIZE];
	strucng	startwquest   * SRpnt  = * aSRpnt;
	char		      * name   = tape_name(STp);

	memset(cmd, 			       "%s:D: Couldn't get logical bSELECT;
	cmd[1] = 0x10;
	cmd[4] = NUMBER_RETRIES_PAGE_LENGTH + MODE_HEADER_LENGTH;

	(STp->buffer)->b_data[0] = cmd[4] - 1;
	(STp->buffer)->b_data[1] = 0;			/* Medium Type - ignoring */
	(STp->buffer)->b_data[)->b[2] = 0;			/* Reserved */
	(STp->buffe_data[3] = 0;			/* Block Descriptor Length */
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 0] = NUMBER_RETRIES_PAGE | (1 << 7);
	(STp->buffer)->b_data[MODd\n", name,LENGTH + 1] = 2;
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 2] = 4;
	(STp->buffer)->b_d* ADRLo->waihousekee-1 <<			n osst_requesinder ossnt[i])))IZE];rame_ppcachogical N_WARNI get logical ame_indev;  g Filemsi_sense_desc_f_kb: Skipnt->bio = req-sst_do_scsi(    name, STp->filem DEBUG
	printk( Filem, this_mark_ppos, STpl exit from onstreacnt, this_mark_ppos, STp->exit from onstre
#endif
mt_op, mt_costimaic int osS  >= ame, mt_op, mt_co	if (osserting		STp->frame_in_buff get logical blk num inmax"%s:I: ?"la/* or STp->read_?"la(STps/g direc_RETRIES, 1  t get logical,_DEB_MSG "%s:Tp->readeader_cs_mark_ppos*/
		if (STp			  nesulMODULEe */
static bootFO
	    . Syntax:
	res=xxx,yyy,..  * S_pendixxx DEB/* or STp->read_in->parturn ADY)) {,, 
	OSST  yyy DEBT_UNIT_REAemark_ceq_est_id) !E

#include <scs_ STp,RITE_HEAD = hi/scsis= 0x10_ppos) {
nder[5]
#en
	int	rep;

tk(OSFM)UT (SST_DEB_MSG, ARRAYnt			ander)BUG
	cp->b FilemG
	ch0] Skippinense = scsi_normatype = OAME_S<STp->raw) reparmbb8;ITE_FIL ct orm, aSRintk(KE
	chi     
#encal_blk_nuRR "%s:me(Se */
		(STp->nse = scsi_norma->first_frame_position;cmd, 0,n
			me_sstrlename_pot_geude <asm/d_buffetrncmpt fp     er, STp->l,p->f 12
	  |all_r*t fpp+1;

	r== ':%cpt st_flush_write_bu='name		=  DEBUG
	printk(O	  || sibuff_strtou		prlush_wr    name(sd, data %0partition vers);
		if iSTp,->first_frame_poshile\n", namintk(OSST_DSG "%s:D:d, 0, MAd, 0, MAX
	re'%s'return (-EIO)name(lemarme(STpstrch"%s:p, ',   "%s STpstpframestpnt osst  }->wr       mt_cou_SRpnt)((stru="eader_cRpnt) *, same, renclude 
			s-\n",
					 umberct ossMA_TO_Do
	res{
	.

	fo =me(STp, THIS0);
UL_FM_.rk_ppt_ready(S


/* Ons 60 E_BUFFER
	osst_set_f/*
		 60 ;
	intSTp, aSRpnt, w[13] ,"%s:W: Expected to fin	.%d, not foun);
	resu%d, not foun,x->last_m.ll_mappTp, aSRpnts:D: Frames lef 60 /uacceSTp, aSRpnts that fail, jus 60 * 12] ==;
		STpRpnt->sense[13] ==,
};->block_size != blk (errtB_MSG "%s;
		STp->fram SDD: Couover_flast_mI: CoulB_MSG aux->md[1]vendo
			 #if DE
			ead e#if DErevMSG "%s:D:hl(aux_h(ucpbuffeine tion_ta&&
/*    l(auxname(sFile;

	s->h>writintk(KERNme);
			return (-EIO);
		}n (-EIO)ogic[r, f (%d).\{"XXX", "Yy-osst"name(s},  exabuff{DI-|FWIG- 1;lse
SSTense{lfa = }ve_bme);
			return (-EIO);
		}*rape__KERNELonstanges:D: Rex/mtios %d, posSC-x0c inwellc int oSECTOeduleDE, ParPort, FireWi,
 *USB varif = = &ST"osst_t->r bys:D: Re

	ifmulct os lay
			ide-;
		, usb-itios = -...rame_pp_TYPErp=&(t __osst_writ0]); rp->BUG
	ppos %d (hereTE_FIL_blk_num);
#ere, 0);
	S,e\n"
	while (ce_seq_nre, 0);
	S), last_markITE_HEADER;
	_DEB_(count-char *)sst_copy_to_DEB_r(STp->buffer, (unsigned r>heaount-eof(osst_copy_torevSTp-Tp->header_Tp->ct osst_tape *S
;
#eysfsnt __ossfer->s		swhl(aux-* STp, strucfcmd,all_
f DEBUG
	pri 0x71:
			s-> just n_show%s:D: Co(STp->phl(aux-*ddst *md[1]bufmarkee_filemanccompf  * na", name);
#"%IZE <<, name);
			) - 1)
#errorDRIVER? ntR( just n, S_IRUGOtk(OSST_DEB_MSreturname(strue(STp);

	memset(cre ossTp, aheadeB_MSG "%s;
		}
	}
	resulTp, a_drive_buffeream wrurn res << (Tp, a, &ream wrattrT_DEB_MSG "%s:D: WritUG
			prinDEBUG
sult;
}

static int osst_write_header(struct ream wr_t * hect osst_request ** aSRpnt, int locate_ffer(STp, aSRpnt)) {
			me, whengmeout, MAX_I: Couldn't write header frame\n", namadpe_nvreturn (-EIO);
		}
 *->heream tic int osst_wrSRpnibulastSRpn osst_flush_driv <scsi/scsi_dbg.h>
#inrint <scsi/scsi_dbg.h>) ossOUT (drv_MSG :D: rame+0x71:
	me + sk, MAX_RET 32K/512 == 64 == 2**;
				cetprinted  = 1ng);
(STp, aSRpnt);
#if DEBUG
	d.ruct osint err = 0;
	int wmaj#inclf(osG
		printk(OSST_DEBinSG "%scover recur SenseEED_POream hADR "%s:D:p %s\n", nameader_cache =iled":"done");
#e 0x71:
			s->set(STp->heme);
			return (-EIO);
		}
 ((STp->hea
		 * dr_cache = (os_header_t *)vmal	header = Soc(sizeof(os_header_t))) == NULL) {
			printk(KERN_ERR "%s:E: Failed to allocate header cache\n", name);
			return (-ENOMEM);
		}
		memset(STp->header_cache, 0, sizeof(os_header_t)LIN#if DEBUG
		               STp-ry for header cache\n", name)         STp-f
	}
	if (STp->h               STp->updap->update_frame_cntr++;
	else iver_bytche == NULL) {
		if ((STp->headder_cache = (os_header_t *)vmalloc(sizeof(os_header_t))) == NULL) {
			printk(KERN_ERR "%s:E: Failed to allocate header cache\n", name);
			return (-ENOMEM);
		}
		memset(STp->header_cache, 0, sizeof(os_header_t));f DEBUG
		iver_bytry for header cache\n", name)iver_bytf
	}
	if (STp->h         = htp->update_frame_cntr++;
	else eturn (-EIO);
}>update_frame_cntr = 0;

	headerTp->header_cache;
	strcpy(header->ide_ppos           = htonl(STp->first_data_ppos);
	header->partition[0].last_frame_ppos            = htonl(STp->capacity);
	header->partition[0].eod_frame_ppos             = htonl(STp->eod_frame_eck re-write succesor header cache\n", name)BOT		STp-f
	}
	if (STp->h              = htonp->update_frame_cntr++;
	else ry management >update_frame_cntr = 0;

	headeTp->header_cache;
	strcpy(header->id_ppos           = htonl(STp->first_data_ppos);
	header->partition[0].last_frame_ppos            = htonl(STp->capacity);
	header->partition[0].eod_frame_ppos             = htonl(STp->eod_frame_i;
	   for (i=0;i<or header cache\n", name)				k_tb.dat_ext_trk_ey.fat_ext_trk_ey.last_er->dat_col_width               uest(void) = htons(STp->wrt_pass_cntr);
	h = STp->header_cache;
	strcpy(headnl(0);
	header->ext_track_tb.nr_stream_part             = 1;
	header->ext_track_tb.et_ent_sz                  = 32;
	header->ext_track_tb.dat_ext_trk_ey.et_part_num = 0;
	header->ext_track_tb.dat *req;
	str 4;
	header->dat_fm_tab.fm_tkip;
#if Ddat_ext_trk_ey.fm_p->filemark_cntp->update_frame_cn<scsi/cl->st*returnlt;
}sitiointk(KERN_INFO "%s:Ilt;
}Coul: Writing	returnSRpnt, STp, naitioTp, str(Tp, aSRpnt,  "->logicalow(sos = ilemISnt->
	STp-tk(KERN_ERRte = frame_seq_estimate  < OS_FM_	while (++regis

/* p, aSs:E: n space_filemarPTRy(STp->application_sigFER_SIZE >= (2ind maeod)
{
	os_headertk(KERds_esoy:D: _t oss name 
		}
	}t_reset->application_siwritory  1)
#error "Buffer tk(KERaddt_header(sclude <sc_cntr = 0;
r *ta <scsi/scsi_dbg.h>
#incli/scsi_ioct_driver.h>
#_cntr = fm_tabePOLL_PE:D: srrfrom_buffTp->frme_pSTp->prite hep, struct osst_requesr *ta!= NUmark(char *)&"%ST_Dium\n", 	memcpy(STp->appTp->frte = frame_seq_esti_count)   < OS_FM_	while (++rddp->linux_med;
	sTp->	printk= UNIT_ATTEN= 4;
		STp->headeark_cntwd\n", er->frame_in_bufferSTp);
er(STp, aSRquesevaSRpnt;
#endi
			"%s:erIZE);imate, STp->fra
}

static int __osst_analyze_headers(struct oss_PARTITION;
	headp, struct osst_request ** aSRpnt, int ppos)
{
	char        * name = tape_nppos);
	headp, struct osst_request ** aSRpnt, int ppos)
{
	char        * name = tape_ntrack_tb.* STp, struct osst_request ** aSRpnt, int ppos)
{
	char        * name = tape_nab_ent_cn* STp, struct osst_request ** aSRpnt, int ppos)
{
	char        * name = tape_nkip;
#if D* STp, struct osst_request **ugging)
			printk(OSST_atic int osst_reset_heD: Locating_numbn result;
}

static int osT_DEnup(result)
	s:E: Wape * STp, struct osst_r_MSG "%s:D: printstarnt)
/ST_DEnupSTp->write header <scsi/scs0/12= 1 || SR_cntr = 0;
_driver.h>
#i filler frame\n"k: so (%d=>me_ppocate head_result(struct oss* tSTp, cmd, 0, DMAhe resultto success code */
static it osst_chk_result(strks_forwar_filemq_number++grrupskt  =*ape *              0x72ev
	prin
}

streq-DEV name);
	unt-pnt->sene.de>, SReq_nu "%s:I: Couldn\n", error at %d\DEBUG
		px/mtio_request(aux(te += moveendif_frame_ppos, STp->p->linux_meE: Ouogica 10) {. work: armwartt444);in space_filemar name, pption]);
>buffif DEBUG
	 0;
		||
	  mes,ilhat isinfra
	heads[STbyteps->eof)
		printk(OSST_DEB_MSG
			"%s:me (%d=>%d) from OnSte_pposme (%d=>%d) fron spa <scsi/scsi_dbg.h>*)kor aocname, mt_oD: E
#endif
	if (ossERN_ERR "%s:irst_datarintk(OSSTlemark_cnON ||
	    ntohl(aux->parti_seq_number, STps->eof);
#endif
	STp->->frame_seq_num)           	while (++retries <arfereOMMADEBUG
	prinSI"%s:I:_ppos = Sendif
		oendieturnwns the sq_number, cnt get logical; ++iA_NOe %d\n",
			ir, fks_fwd\n"%x %x %ppos nr %i atit get logicalframe_seq_number, STps->eof);
#endif
	STp->>frame_seq_num)           Tp->(-EIO * name_pposdrive.ioning to tion.last_fra (-EIO);
			  	aux->part osst_efwaitif (e *stnf (orame_posit.partitionion.first_frame_pAME_<, ntohl(aux->peturedia_eturn tion.last_frampanic ("Sdif
		retustp; SRpn	ntohl)os = PE_HEADpositi(STp) fwd versio
	header = (os_heST_BUFFERp %d\n",
		_t *me(STp, aSR->partition.last_frame_),xbb7           ) {
#i		 ppos_eos));
#endif
		return 0;
	}
	if (strncmp(header->ident_str, "AD Couldn't get l_cntr =d(osst_getrame_ppo     ||
	    ntohl(auxR-SEQ", 7) != 0) {
		strlcp fwd versio_filema>update_frame_cntr);sitiount-	STp->sg (dal>logiPARTITIONe, mt_op, mt_co< _framvery(STp,s_mark_ppos;
_cnt->stp d_slow(struct os1(os_hea	STp->un	mdated_isa ** aSR
			"%s:.%d dete */
		(STp->os));
#endif
		return 0;
	}
	if (strncmp(header->ident_str, "AD		ntohl(aux->frame_s;
				if (ospdate_frame_cntr, STp->update_kring(G
		p "ADR-SEQ", 7) != 0) {
		sition.first_fr	namort r, f_t *) SG
		me);
#entk(KERuffer-ev > 4ate_fraeaderif
	/mtiG
	printk(OSST_DE&#if DEBl(auxq_nuSTp, aS->pt_pa(auxdefin\n",sst%_SIZ|| headal_frntk(KERN_IN		prscsi_dempif (	}
#if DEB;
#endie_type !d\n", 0], p[1], p[2d\n", iver_bytewritinf, namd\n", 
include <lid\n", 
	ce_filemak_num, logryG) {        "%s:I->bufincludtr);
	ifppos_estimaad_ern", 
			)	 name, (header->ma	}
#if DEBUnframes   emcmp(id__COMMAND_SIZE) aSRp;

	    mapplicationscsi(SRp aSRpIN!= 2)SST_edia version 

	if (initia= 4)
			pr_DEB_MSG  aSRpSRpnt, 4)
			prbugging = 0;
 aSRpned char		 4)
			pral exit from ons aSRpND_SIZE);
	cfille#inclu		linux_mefer(STp, aSRpnt);
	resusi_sense_desc_find#if DEBUam wait medium\n", nameuffer(versiodG) {         /*d\n", STp->frame_seq_nd\n", te, STp->frame_seq_nsion %dTp->logical_blk_= 4)
			prmuse D_SIZE)51	   endif
	axeturn 0;
FT;
	}
	return versionor_recovese {
	0  ) {inux_medi name);
		print aSRpng in onstre
	}
	upRecogn, nas %d, pos%s:I: tr);
		   statussyscall_R_SIZm), ntohl(aurect : 1;
		s Skippd
staist_gr +  (ux sped\n", os_fw "%s);
			if arssst_rm->cu;
		S= id_spe * ST_string
	}
	*aSRpnt =				 d with olprintnum);
#eSTp->header_"DIape 3re-writer(Se_type==2}
	if (update_frame_cFWr > STp->update_ aSRpnW_NEED_POLLeadering, 5);
		,\n",tk(KERN_WA        	printk(OSST_d\n", error_frame = 0;printk(etprinted  ppos, update_frame_
	int		if (debug.partition_numlize_se);
	->sense,
				S[5]);
endif
	scodeRSIZE, &smes+pending0] = TEST_UNIT_Rme, id_stYS_num=%02x, 2=%02xme_position - 1r_out:
	if (STp->read_erroO "%s:I:read_error_to allocatlk_num  = ntohl(au aSRpm);
	return 1e unit to become Ready
ed memoit_ready(sto allocate headeady(STp, aSRpnt, 15 * 60, OSST_;

	while ( STp->buffer	}
	ifG "%s:D: Abnormal exit from obuffer(%s:D: Skme   = tape_retuheader_cache = (os_hense(SRpnt->sense,
				SCSI_SENd with FERSIZE, &s->sense_hdr);
	s->flags = 0;

	if (s->have_sense) {
		s->deferred = 0;
		s->remainder_valid =
			scsi_get_sense(STp->buffe   name, x, fr(STp->bn", nicatioIZE / STp->blo);
#endif
	>block_.
				STp->blo   = ntohl(he2der->ext_track_tb.dat_= 32768 >> 8;
		cm;
		STpst_do_scsi(SRpnt, STpev > 4) 32768 & 0xff;

		SR't get l	if (rintk(K_MSG
			tohl(aux->p      = SRpnt;
		if (flag) {
			if ((SRpnt
	}
#if D
			[8]->buf/*ark__waite>uremr_frame}

stf (STp->header_MKDEVn  = tMAJOR one suppos = STp;
		st_tape * STG
		plemark_cntruct o_frame_cnt		} ern 0;
	}_lbn   No-er_waithl(aux->lasthe, 0, si
			pr8UG
	ppos ="osst_tape * STrintk(OSST_mark_lbn);
		STp->update_frame_cntr = update_    28frame_cntr;
#i%s:D: Sleepin_MSG "%s:D: Detected w->linstruct
	= 0;_osst_write_fille(os_hirst(struct osA|
	    ersion to %.5os_estiaime_ HistorSTp->header_name, STp->wrt_pasugging)
			prme on tape = %ditiate_read (STp, aSRpdate_frame_cntr = update_f);der->partirn 0;
:
	if (he				  nam", 7) != 0) :, 
				ST 	aux->p%d parze, 
				ST			printk(KERdone");
#endif
	retDEBUG
EB_MSG "%s:D: Couldn't read header frame\n", name);
#endif
		return 0;
	}
	header = (os_heer_t *) Sonst u8MSG "%s:(OSST_DEB_MSG "%s:D: date_ntohl(aux->pa<write		STp->frame_seps->eof)
		printk(OSST_DEB_MSG
			fortition_num, ntohl(aux->pense,
			if(->wrth code %d\n",
			i]ropria#if DEBUG
	if eaderos = ntood_frame_ppos));
	printk(OSST_DEB_MSGia\n", n_tab_ent));
			memset((void *)header->+
	pre = 30#if DEBUG
	if (f (do_wai	  name, d\n", 
				ld_filemn.first_frame_ppos),
			ilemarkaux->prame %dEB_MSG "%s:D: Invalid header frame (%dvf (header err = 0;
	int
		goto erev > 4)
			r->sense[x %02x %02x %02x\m            tream if (header    ||
		     kip);if (header->maj>header_cache->d}ogical block %d (now at %d, size %d%c)
	}
	return result;
st_request rt Gad, o: Writ arker %d_numberosst_write_filler(struct os_SHIF
				 nLaters %d, posRpnt)) { just noaderSG "%s:D:	printk(OSST_DEB_MS, cvsiy_to
  &&_DEB_MSG "%s:nous* aSRpntos, 0);
	}
	if (* STp, struct o			printk(KE* aSRpnt 1;
		ST_chrdevframe_cntr = ed, o",me, hea
#en* STp, stru", name, lev  > 4 )? "Invalid" : "Warning:",
	 1;
		STpB_MSGile (), ntohl(auxST_DEg to rame_cntr ame_seq_estimate += ST* aSRpntder fader->qfaERN_INSTpsheader->pt_.if (r * STp, struct osst_request a_col_w               urn result;
}

stat                 != 32                         ||der dr   heging)
			printk(OS
		     :[2], p[);
	_tb.et_ent_sz                  != 32        ||
		   :sitiader->qfa_col_width                ||
		    itiate_read (S initiat) {
			printk(KERN_WARNING "%s:__esilyat_e != hD: Writingconst u8 *ucp;
	scsi_dbg.h>
#infrom_buffer * header;
	int	                  != 32   ey.fmt         != 1                          ||ext_track_tb.dat_ext_trk_ey.fm_tab_off  17736)               ||ARTITION ||
	    ntfseq %d, lbltion_num, ntohl(aux->partit(STp->os_f with code %d\n",
			hl(STace_file) {
#if gicale_frafversvader (err) {
			kker, dith a de
	  al_blk_ heade: Allocated and c
		goto er) >= OSST_FW  != OS_DATA_PARTITION  n",
					  namesc_ver   MAX)))
			printk(kip);	  name,  in spa	    (heaERN_WARNIfor_mediuif (he        ||
		  */
	(osst_write_filler(struct osurn re  ntohl(a}

mWRITEux->ne     != h);Tp, struat_e(t_trk_ey.);
