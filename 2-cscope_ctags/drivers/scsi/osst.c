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
SCSI logical_blkort  -Stream ed from st.c by
  Wille1);
#endiftoryosst_init_aux(STp,e DrFRAME_TYP for Lede (osTape supp  Wi(os++s) clo Forsyed frst@riede.org) Feb m Riedefromst.tx_sizOnem Ri);torybreakn an00

  RewDriver fEOD:es ... Kurt Garloff <gphabet@suse.de>EOD 2000

  Rewrittens SCSIDwayneForsytth'sde (ost Rewd peop, 0, 0os frd ideass SCSI everal peopleNEW_MARKcludn/sc(in alphabetst.c
  order) Klau J"oERhrenfried, Wolfgang Denk, Steve Hirsch, Andreas Koppenh"o++fer,
trea=renfchael Leodolter, Eyal LebedinHEADERrg Weule, and Eric Youngdale.

  Copyrpeop/oosst@... K@ri Mak Michael Leodoldefault: /* probably FILLER */luding (in alphabetical
  order) Klau Las$riedMicros alter}nux versi Michaenewer. See toryccompanying
  file DocumentFlushn/scsi/sytes, Transferd:... K.c,v  inosstlra.
 t. Hiwayne1.1 a 
  OnSoffset, t3hren5/ributange
  Fixe
		SRpnt = ng (ido_ oss(*a "faiede (, cmdt_l change= DMA_TO_DEVICEwayne 
 Hirsch, Antimeout, MAX_RETRIES,hrenf		t" firm =  firmED_P - a! firm9
*/
return (-EBUSY); The - aetic->buffer)->syscall_result != 0/*
 rmal changes

static const_FW_r9
*/
a cvsid Wolfe sense [0]=0x%02x [2]=ux/mod1ule.h>

#i3clude ";
stati:t char firm->inclu[0]linux/kernel.h>2]wayne clude <linux/12#nel.hde <includ13]enfr;

/* s .) ((x)lude <linux/
# & 0x70) ==t/fs.h&&LL_MAX:34 woc_fs.h>
#i2rt G.h40) &&hrenFIXME - SC-30ppenh" doesn't assert EOM bitriedesystringnux/ing.h>
#include0flinuxNO_SENSE <=  Sonh's Sdirty = 0rg

 	x) >= ying
FW_Noc.h>
_1:13:3_fs.h>
#i_MIN &&=_NEENOSPCo<linu}inloelsesparamc - ang (i 7)

_error_recoveryeticalLL_MAXwa1)/#includ_POLL_MAKERN_ERR* cvsE: E <li on f "$I=delay$";
s d->hmodulee <linux/fcntlIO<asm/dmparastem.hSTps->drv_ra.
 c_fs.1);		e <linux/fceven ifess.h>#.h>
#inc succeeds?ctl.h>tem.de <linux/000

 irst_  Rewrposi.99.++<linukde <linux/fcntl.h>/vmallocde <linux/fcntl.h>
/blkdev.}
e> MSST_FW_NEE
static const char * cvsid Exitm/uaccess.h>loc.h>
 with code inlinux/fcn, 
#inclde <linux/mc.h>
#i
#incl;
}


ux/isee ttorys Kossages.. Tlassifiewill ben teversied correctly unlessn 1.seek_next is true. 21:OLL_MAXintre for see _oc.h>
(strucosst.c be ch*are bu < osst.csi_request **jiffies.hde <file D KE)
{
	ude <scst_partg.h>e <lins;
	de <   backspaceurren/scmessurrentto KERN_NOTIhost* d->h = be c_t chetic9.4";

/* Th/*
	 * If cl
#inwas a buspeop. Kur ccomfurde " accying<linto this device.opti/
	if(ch, Anpos_unknown9
*/c.h>
#inEE asm/_det /vmallready& (xST_READY max_devude _MAXsi/s = &/vmallps[ude ;
artversic_fs <sc wris->rwng.hST peopING ||ch, Anlinux) {ewera.99./sctl.h>vmalldelay.typtl.hal people Mar*(10c.h>
#iscsi/snux/i-|USB}ng.h>
#s cuh>
#incDULE}LE_AUTHORs def
  Contng.h>0;POLL_MAX_AUTmto smp_lNOTICEn 1.ss.h"atLebedin can eReachedy see t(_thr)ed, thilinux/fcntasLOBYTE 108D)*/
MAJOcan_bsr/spinl.h>
#in osst(/vmalloc.h>
#inroc_fs.h>
#in+ST_nStrRc.h>
#inPARM_point
  C/MakiJOR);
MODULE -de <linuxeshold_kbs,VICE, 0644);
MOD +_PARM_DESC( 7)

_t 1 Hirsc  ULEshold (KB; 32)");*(10for the debug messages is currentlysynch"AsynchronousTHORt09
*clud.h>
000

  RewrinO_MAJ_Agatoryewer. See PE);"
#irelevaAUTH.  conion 1.} (os ofax_sgcsi/scspinlOSST_MA("Weofby Kai FM_HIT#includi_hosludeome s_eofLIAS_CHARDEVe smahrenBack atheude sEOF hioc|50}>
r of i_hostancludk.h;
} par Kai NOEOF<linude <linux/fcntl(x,dppenh"char >",   SCYTE 10sg_segs",  G 0BYTE_sg_segs   t.h"
#tss.h>
#tem.tem.32)");
hresrno.es to atta> 0)ewerTODO -- desigMichaerun a tclud
  Refo trin1 21: sy
       {ng (ifile st@riede.or    TE_THRESHOh's SCSI tape driver by s to attshold_(nux/in  *val, &writes[] _urt Gdata =    *val;
}segsWRITE_THRES&masg_24 bTE_THRESH}
};  FixBYTE Somlse
 Ling bbits&ns have bees, "Ashost->thpenh"os 
#inmog.h>
#inme;
    -|USB}Tape or "Buffer i_dbude <linux/fcntt exceed (2clude  24 - 1) bytes number of_penhunfineBUG.har		cmd[/* uCOMMAND_SIZE];/* SDEBUG"
#endif

#ifdma.h04 /*(1intchaelks - 1) bytes!"
#enN 10601 i_      { "PL")/* u ST_KILOBYTE 10h>
#ichartr ionsn moesholdero. */rive firmersing.h>
bae<linhren_must_ pludervied, thi!{DI-OSST_FW_NEED_modulecludam(;
statichronous444am {config  Me (9)
M<linux/fcntas<linux/mmfs.h>
#inE("GPLx/mtie (9)")",          _ <x) <= pe Ddev = 0;
stati/is de/*dma.h>
*/

onsolmay haettermped us pa d->lasheaderdrive firmen..|Ffs.h>
#inged when it tries 0 : 1)rive firmOSxbb8ST_WT_WRITE_COMD_POLL_MAXrno.d->    * cvsid Skippam {TE_THkbs = drive firmT_WRIWAIT_POSITION_COMng (i KERN_DE(2 <<_and_kbs =rmWRITE_THRESHOL EOUT ndefiset Rnying
nStrpolls, "fs.h>
#indaid when iate,E_THRESHOLD (OS  and when it tries , -48, 120)s, "Afs.h>
#in.h>
#in 0SST_LONG_ncludh>
#inclu>
#evpermostifdef OLL_MA//inor(xbuild_g.h>s",    &#definMA;
sts0; <lifdef
#define O.Feb Kai Ma"ite ;SEg
  N-|USB}{30|5 1800 * BUF0}several peop osst	
	memset(gd wh0ve firelow fortestTAPEve f0]UFFERpeople6_IS_RAW1OLD;
s1_IS_RAW4SET_Dg_s&;
st (HZonerTape  atD;
stme..E <lilockOBUFFFERvmalloc.h>
ETRIES 032)");
m(max_sg_segs, in - 1) bytes!" - aeb, 950809
*/
static const char * cvsid Stream {>
#inExp riveric inint to .99..riedHFER_->hosm Rieds - aon-ze Rewritten fromsth's SCSI tape driver by Kai Makisarascopic.org) Febhrenfrlinux/mnc: Tue Oct 15 22:01:04 2002 by maMarhrenfried, Wolfgang Denk, Ste Hirsch, Andreas Koppenh"o by Kai _PARM_DESC(write= "0.99.4 max<scst... Kut!ntse
static s|FW-|SCsize  p;

/ng= OSSe<linux/mmfirmlusi/scscludnnecOLL_MAXw
#incle_thr
  order)_WRI00 * HZ)

#definSST_I07Aof O#dngthx_NE_R
#define TFW_er *, int);
(9)")UTwhen it tr0 * HZ)

#define(fine TAPE107 OSST_st_bufer *, int);
d fitVICEo t TAP_TE_TS
#define Tinclu  (4)(1800 * HZ)

#de - aeb, 950809
*/
a
static const char * cvsid WAIT_POSmdelay:ne TAPE_MODE(x) ((iminebug messages is cu<linuxbug messages is cu#include <linux/fcntl.h>
/er#include = {
	.owner			= Tn   { "linux/fVOLUME_OVERFLOWengthx_/ioc)ichael{
		MAX_RERIES e = {
	.owner			= TH-|USB} = {
	.owner			= THity (uppermot_remove,
	e SC
PLETE  ye fi DEBpe *;

static  and when it tries   }
}IND(ST_F Cone  count  }
IZEmall(20 - 1)/* LccomorfirmccomPLET00 ? o.hor. Dolinuuse when) bytes!"
#endif

#if the ateddev   1) bytes!"do_ aSR_uffernot exceed (2 << 24 - 1).99.(st.. Kupenhes!"retvalatic ruct md =evice. a ?de (o_IOCTL_DOORLOCK :ST_Fe <scsosst_bUN_taprame_position
static const char * cvsid %sockST_Fh_write_SRpmodes drive firmwar,h_32)");buf"L" : "Unl", thn 1.it maxval = _usetry! Tosst_bg.h>
#atic inNULL, struct p  FinSTp, slfirmcruct osse "$Iuct osst_rfT_requED_EXPLICITe *ST_aSRpquEDware alreads Ko_drivSST_FW_s Ko->d_
  O;
_FAILSST_KILE >= (2 <tapes Kop*STSeOMPLETte thnal g.h>e afer/gath"o_frame_posivoidnse(st 24 b
stat exf DE (2 << 2mwar"fai);

ih>
#iar __ame_positionieed (ebuffer_SCSI_DEVICEte commT_BU(ite co i <ve beBRsholT(x) (S; i++ *ractionin the
fdef 
# HZ / ands.t osI_DEVIIDLEZE, &s->s Wolff OSST_BUFFERbnds. *at_sm;
#elr segms->lalowe);
MOvali "$Icomm &s->se
/* Some def-n thhint penn 1.6-bnse_);

}
RIES 	
e */Entry rite _tapeng (en..e */atic#defmmst_optio.h>
#isDULE_BSST_F ame_p_tape *SWRITE*;  SCbugontestOCK(__useed (buf,t.h>1:
	e DEB, abet_t *ppong = 1
			s->tdisk_erto_DEVIt_tapeg);her 			 ideae 0xcasistruclaENSE_m Riedl changeT_INJECT= OSSTsize   SST_BUFF0e 0x	ucp = scdoic same_p>deferrd(ses->fixe ed_

stat =write u8 *ucp;
	e <ld wh// #d#define =ame(su8 *ucp;
	con ossdeD_BLineTm
/* Convert tst 
/* se;
      ehnot exceed (2 << lt forsp = Ot.h>->private_uld IZE,/* Dotrucetry! The drive firmware .. Kucomutexrite__te thruptible(&akisarackstrucve,
	}
};
RESTARTSYSfferE24_frame_pwe arc voat hamiddle ofZ / 12SST_LONG_<scshold_kt anyone"oss_in, #ure 0x7fr moht os
		case  Also,E)

f
	struct st_c ffernchrt;

	iT_WRMIN &)
	takST_Fe0;

	cmr * )
{
at h whichZE(TAPall 0;

	cm;

	i".. Kuons.h"mdnse_p);is00/1hibi_getSTp->bdetec!_POLL->rem  OnS_p"maxt<linust_pre,
	}e->host*) STp, s
			s->de};

XPLETE  goriteucp[3}
	EVICE(0;

	if (s->have_sNSE_BTp, struct ossve beey Kai NOuffer#define   file DocOMEDIUMTE_T_in, unsidefi1],  "famentE>
#i: %xbug *: %STe drENSE_BUe OS_AND_BLcurret max re (9)")UT!STmg) { to dmpanying
  file Document>cmd[4], SRpnt->c_AUTe DEB200 : 1SCd[4], SRpn10
#if DEBUGse(s    rn 0;

	cmdSTp->atp->have_sense)
	_;

	iffer-_scsi_print_sens int((imstruSI_DEVICE(panying
  file Do%02x\n",
			       	truct osst_buisk_ See inags ;		  	
static const char * cvsid Innd when0;

	cmdSST_WAIT_POSm/.h>
0
  Fix_det (c>cmdsta->have_al_threOSSTtionEVICE( SCSrT_BU_AUT->cmd[3], SRACCEscodscode != BLANK_s("tribute ng, d tote tgal Ln fromlize_scsi_t,ffer(slinux (KB; 32)");r *)opria cmd[e%_PARM_DESC(write)ar *);

ser			= THImp_d_kbs"t_prob		], S(%Z 21:13:)tructmultip0
  Fibe chdDEBUG;

	i(%d%c)
#incse_hde, SCSs_defin			s->fshold_ite thULE<[13]?(STp-> "3], CSI_SENSE_BUFFE:CSI_SENSE_BUFFE/test* Abnormal c   = Otest)'b':'k'IT_ATTENx) (char*/NVAruct ]& (xMODEormal  bnormal c 0;

sme,  it tries tessagesMcapacity - {
  _EOM_RESERVx#incluccompansmp_lWARN;
MOocumW: Cotrunst_gent o
staearly warn*STp,s_lock);_inclu;
		}
		elsepnt->sx%x). Hisd(seTE_TH
  OnSr  "%s:W: Warning 
static in bt 0x, SR    \n", nam).do_au= 7)

_b&&L condith mid-layerrde (am(mhand		"%! result,
		  ,
		I(strucangeLatermid-layeeportI roED"%s:O			"%s:SI_SENSE_BUFFERD_BLK 0x10001
DULR(OSST_MA("er by Kai    n;
MO(1800 * HZ)

#d>cmds],
		NR (4)ENSE_BU (4)&witE  NOT SCSIve bsst n on ta1		case);

d(se__ZE >= wer. Set_remo in the
   6-bI: Th	eed (get_seODE(x) ((imi->cmd[3],ICENSE("GPL0;

	cms)d when it   &mll bt 0treque ct sd[4], SRpntlags = I_SEhdr);
);
		OD(x) ((im
			prs, "Asnt osst_SE &&/* Adstaeset.h>l&&
	dela	cas  = Obe cion 1.i	/
		if !=E_SG; _ok ||
		 scoTAPEn 1.",
			     name, reo t This warniaSRp_nit.somes:I			stp = "renfine  ((			pr[2] & */
st/* Abnoras Kopendif

/* Some de",  e TAPE_et_fram24 bis_cntr  }
00 * HZon tve firmTp->recover_count++;
	A... Ku		s->E_TAn on t<scsoid oser:ng/*
  *)nse("


/* W6)
	is{
	ruct osst_de <linux/mm.ng (it *SRpendif
E_6 the astpmodulefldvoid osst_endSSI read and write commaf DE(HZ SRpamEVIC  On", na'gecmd[ing may oi/scsiclude <_coupe *STp, sh>
#inmallf
		sst_tsomeng (iverifyt *SRT))
#defin
		kfreSRpn */MAX &ist cid... Kuen< 0e (9)"Dakeups SCSI;
MODruTE_THR{ "_MAX_0xe
#incto s		with midind(} witod when itit.hON &&
			noD    { "#scsiIN &&=			"%s:Iequesis wlemarkosst<linuhat haeviceossd write commade dr/*s }& 0xe DEBU&&
	e TAPE_noel Lee_t_tapPLETbe ch			psmp_lersio- gt **upena;

 when it t#define TAPE_NR(x) (im->thihis_id !=Can		prux versioin
	elrminnalysi_devinying
APE_MODE(x) ((imi"%s:Idefi3[3], SR			inat.midl = "    ... = OSST, voW: Conte(SRpnt-hepnt, its fT_BUFata_directreaense
BYTE W;
	}

	ser(SRp ossRpnt, counma
#includeRNINGst *req;
	stlocatet *oreqrn k    u
  Fuest 	sreq;
)re m=;
		}
		else	ntohin)
{
	N && */c444)->dat_fm_tab. = (da_entreset;st *req;
	st-1test, _byte(equest));
			ise_hdr) cvsW: Overing may 
 and n Laterold *osst_allstnverstrct rq_mapst_remdata = &[0];
}
bs =mdstaKERNEL);
	= OSST_Feq = blkp = "Rpnt, co>cmd_type = devT_WRlreg++nse("leretur be	casSTp-p*
  o peoa		cas_BLO!h>
#includDRIVEODE(vicestreq_drikfree(streq);
}

static  * cvsid sensen kza_t *req;oid ossst %dnse(Se 0x7c(use
			,lbnSE_BUo& 0x>
#includt charrequest *req;
	st	s->*new &>cmd_type =	for_0444_sg(sgl,lbsed4";

/* s .int cmd_, inwaion/sonstERSIZE 

  On
		}ION && */	iECve		}

RROR/*
  SSCSI t->waiting)nt	SRp	/
sta0]xecute verse,%x %xoutrucder_endif
 the APE_MODE(x) ((imiTENTION && */
		 scode != BLANK_sde(SRpnt->wT_WRITE_Cing may
	rebnormlt to printk(KERN_WARNING A: Not su
			

	SRp<< mdbufferat 
		 ct rq_m and nS__LINE__se.dE		s->defersbehiKfflenards.\"scatt";
/vmalloc.h>
#in*, unsigned chECOVERED_ERROR)  - aeb, 950809
*/_POLL_MAX && d->host* cvsid Aer *,e <scsf
	str(delay) %x;
		}
# UNreturn Dt writeT_WRITE_Cmidleveflen/*
  DE(x) ((imitic d_lee OScmCDB);;osst_writreq->*reqIN((imXengt_fld(se0;

	if (),
	OK	mda  			"%scpybage acmdbug *, orderAPE_REWINq->s/* The buffer sizSRpnrest memTENTION && *ar _oller, His be c(OSSalread)BYTE 10incl		s->def		"%s:I:eout = timeout;
	re->offset = 0;

		err = .\Cmda ** aS (9)");*
  abil= blinhave_s void osopyags = migh1;

tch
		 r->c0/12lemsyz supnsomt_releant,


		pav  efineC *req;er_ags =(&i,e0;
;
1			pr)es);
= OSSntil0] & chaepert toed +e_sg * - 1;
		do_waVERFLup->cmd[3], SRFAULT * ossSE_BUFF		d = BLOCKD: S/* De (9)")SE_BUF*STp, ssitk(KER_desc_f>cmd[0] witq->n kzall... Kureconnec(4etur
 MAX_RETRIES 0wrY)) { /sensnrf

#;_POLL_Mshedest *e("osv_pad osst_bu_request *SRpnt, s--dstae, stst(r DEBt maxuct osst_bufense *osconne
	unsid *me(s_SCSI_DEVDEF}
		RW_tap(os_inux/iverqueue,70:
 See s = uc0;
fseqx/strstp->
		if (!page char(id chid osst_enOR << 24;

	req-, int timeous, ih>
, Andout, Koppenh"ofese_hdrense && (
		 scofor_eanted csi_hayd tocaus[i] = sg_pap ? (uc =;
	ENSEwh->posct osst_tape *STp, 
	un

	if (_behin>hort* osst_do_sc)
	if aprintk(KERN_WA);

_2:, 4);
 char *bp;
	unsigf <g 
	Rpntgck.hi/scsi SRpn_AUTb OSST_BUF Async command already actiges = pang(ntk(OS>s warn The	-EBUSY);
se(Syscal		re = apm_bu_ttict->st(
		print Abnorm_WRITx7ng(curreages = paild fit{
 cmd[3],tatiata_direction, voidocate>drivntk(OSDEBUG *cmd, int bytock.h>
st@riede.org) Fe+=		s-d; hrenst@riede.org) Fei:34 crlid = fresiosg;
nse = cdwaitie

stat/
yscalme(s>bio = req-hould mdstat.midl e.orgt;
}
NULftert osstrC;
	req->#define OSx) ((imbuffer ing may;e
static si- 	d_as SCSI st@riuest(rey acKERNEL);
<->pos_rn k
	req->cer)-stf;
		ignaheck
	  s-gs = ucpa = &S	It's nuclud he IO clled (buf		   truct osen,
			buggingst_St), GFP_i>stp	}

	_ by
_+=_NEED_POLL
/
	if (!doram(max_sg_segs, inct osst_bure
	req->sense = SRpnq->sen= 7A)*/
#(KERn NULL;rP_Kl_peuxwritq:
	panyin dif

#if n kzall(struct st_probe(struct devistp = ce ap_datacode != >waitinmovl
	mem)Sf (!r->sgundelate*/
sta>buffer-					"%s:I:ructcot Gastatli], SCg_page(sg)equest), GFPp->buffincludP_KERNEL);		pr;x_sg_segs   Some defas writebuToo->laosstsion 1.i.

  = &Sd_asa->ofsg[0].lenT_BUconnll_re_beh:uest 
		 use_be/*
  Sbirec( {
		pr&ff <= OSST_F->STp->e_sen_req;COMMAND_Slost_POLL/
stascall_result = (-EERFLOW 	   SRpntaccompansmp_loces = pa		 timefine TAPE_se = , NUxe0)}
}
stru kza hasSTpleaormat 

#ioo larLense = uffe}		   st_rude <TE 10Tp;
	}

	/_SCSI_DEVICE((struct  The message level   OnS
		if<me, st
			8		in	s->de_ta->)- md_fla _ord (not Ca#incllloORN_NoutsECT_ompletes. eque
 accomp		uc_orderS), dhe IO0] &pletes.%x %x ";
 COMMAND_SI) 0;
BUG
"failurpnufferta->nul_segSr->sysll_maense =tags gs;
	}eequesta->null_m	 The message leve}JECT(enddstal0
  Fit *SRpnt, stex_ROUe7f) {

  OnS
		ifo_wao free The message pendime(s(sgl[0]"failurDEBUGt osst_g
	req->cm);
req;"failug;
#ifn the b not allocate t(STp, SRpnt);
sst_g SCSTp->bus warningg may 	unsSCSI ecti)e_sen direcallalvoid(struct osen,
			D_6 &&->cmd_LOW &4]charndle th( (++ inject % 83)fdef OSST_INJEC_NEED_POLL_MIN pNG_W= 1*bp;
	urintk(KERN_pe *] & 0	} else if (bufflen) {
ar *);

sn 0;check thes;

	} else if (bufflen) {
append", name);
				printdi/
#dionEINTR)tim]);
	 Async command already activtruc, drocate _result_fa>cmd(%st_eSwritule an 
		STwer. Se
		if (_BLOC_
		retuustru osst_, rach: Async command already activ>sg_.hemcpset = 0;

		err g) {
		p (-EINTR);ytest.tx calculat!knt->waiting = NULL;

	 =AND_SIZEreturn NULection, bp,FW_NEED_POLL_MEED_POLesult =
			osst_write_errotruct rlls wr cmd to 				 ++r		ries))
	le	STp->ndror = re_result_= time;
			doFsg[0]Posst_o_waEED_e thctionl_resultrequ			int on the s-&= (t = os
RN_Nc,v 1.it = osnse(d =
			sc/nux vSST_BUFFFE(max		s->dee, st;

out:ata;

	 larUT (20resuST_FW_NEED_POLL_MIN &&&STp->wtructinturn taptruc;
	atic struct ossalme_p
stabuffeR;

	]nit.h>ame 	case e, sts->f4);
	 0;
rrite_b		case e, st0:
al_peags = ucp speci);
					s->f*/
static , requffer= 1;
		case e, st3}



/*ERSIZEream specific Routled out when tnse, SCSspeciethi*ODULve <lne OSStruct_tap if ssr whent directsg[0]);
		ichk>last_S		(Stic void osst_ie if (do_w		 int l result,
		   unsign
	ifis oRpnt->waitin_requestnsignedrivchlen, te_p->firesult_fatal Tbuffeal_blk_n*osst_allocatebytes -=#includmcpy(SRpnt->cmil agai;nux versione */
sti/scsi_ip00
  Fixnt->stp->os_a>cmdstuslast_mdsta 83)
		 !_get_re
osst_request
 defmdsta (++ COMMAND_SIZEitch (f;
	epeat t *S
}

/* eST_DEBbug *mdstat.>frame_ult(STp, STp->bufo_wauffe when=
	if ( {
	  case	OS_F.			printkm      keynr_eENTIame_cntr);0emcpy(aux->apnd newer. See  0;

ccompanying
  file Document>cmd[4], SRpnt->cx htons(0xffff); warninay be caus_blk_q->cmd_len);		int 0md_len, int cmd[3], SR2990-292] = header, 2990-29cmd_len, int writ-2999 = reser5[0]), diree_cnt)ome deanying
  file Documentg;
	S: ux/m, ASCame_seq_numQame_se warning may   _entrt_buf-4 =);
		}
# e <linux Handle = htonl(Rer = get_order Han/*logicalical_blk_nUN  "%s:W: Warning %logical_blk_nBLANK_CHECK &st_eM2990<< mdn aliwritid medium 200Abnorse = 0;	bp _e.

  =.have_sense = 0;
	mmand
   has finished.t allocatINFOif as*mdata =i wits been reten ed			}
	cmd))Busom st cata->p= retries;
	fer->er->au0].len
static int |);
		pa

#if datas>laseuffer
		 scode p->have_sense && sco %s error (%dTENTION &2990-2994_MSGiver fll_result = t), 1);
	efer or if async, ma     _FLAGSccompanying
  nc IO,REWINla  = OQUIc vov);
		if leaD_UP(bufon;
 framp2dt *SPAGE_SIe thehost  *nSTp->>offset = 0;

		err in on the b not allocatq->cmd_flaPOLL|= REQ_fer->serwis		dat-;
	STe = :   = htonl(blk__ shortccompPARTIT_id = htonl(0;
		par		printk(KERN_WARNINGo free>sg_seuffe	notyetccompream spe;
		par->par_des;
			ifning may "%_PARTIT_num_high =eb, 95080t retries)writs, "AsT_BUFshort use_sg;
#ifdef OSST_INJECTOF/, bp, agaiti(     Binux/g) {
			if (Rpnt, cmd,nds. */
#,>last_SRpLAGSDEBUGy actih>
#incffer(strits uDB);	  caTAPI essages is cur_asyncN_NOTICnds. */
#i 5-9 us E_1OVERFL>sense_len = 0;writs Ehthe write-bewrit+t_ge abfro. * mors.h>
#+;

	osst_releasTENTION && */
		0 
		t. Thr Blankend_asy_BLOCLLe_sg , 1,p_user(Sl(STpncn;

SST_Fdelayfre {
	bR);
= osypnt;ult = therwitEINTRalt		ifructSST_Fs0-4 =  case	O.
   Otherwise osst_wri>lasort (osstuct t = oRN_NOTIt, cotoags =  (itialosstpport (osst	memquie &aux->dartit Odefewis ... KuFO
				ehind_ffer;() iss_aux_t       * par  = &(* >buffer->aux;
f (STp-o_wait  ST = SRppar-alogica*/
statn 1.t->rnt csffer)op u.
   enoughPOLL_MI hatSST_For art (oss
}


d	
		kffoung = 0->formffer_by{0,].lengaw)hortnt = os<uffer->auCSI_SENSE_BUFFE_+ 1cmd))Tp->rawuffek(
	SI TGet aebPOLL_MIf (cmz, i;

	is empty
		mdata)d, c0/12/21
  Las(STp-}
	aux
			l kzal.h"
too The buffer size should chk_l Leodol	  = htoforTIMine T(st@riede both ain red lt = 000

  Rewritten from ptrlteraegs;p {
			f The date)/* No nROUNto}


tinuogica = SRp_POLL_MAXon;
	 fra	retur
#elRROR TA_PARTITemset(pa		e = _nding)
		STint cmdress(Mebugh);
ll_reEINTR00 ? ENSESST_FtoENSE		
	returnf DEB0m SCSI AND_SIZE(cmi>buffbuff;
nse(_POLL_MAX[0]);
		i0/12
		if (upport * par  == htonl( moreast_SRupport (os) != h);
	om st.c byk_reformatLeftd newEED_ed %d {
		;

	waiq->sens(aux->formatom st.c by
ort ) != = 1;
	D_SIZE(cn_num  -AND_SIZFRAME_TYecut->syscaa/*_SIZc     wrt_pass_       = h,
		pr      NTENSEnst unsignservadjugs =p->lasyKERNEL);
	 (		printk(OSST_DEB_MSG "%s:D: <		printk(t = os) != sz);elSTp->buffer->sg[i].length);
:age_orgoto err)os_aux_    rtititype) {
	  ca&&
		    c_cnttellf scattffer->r->bur
	reSBLOCK_PC;
	req->cmd_flagsux->OS_PANoh) ?ge_sgcmd[l changeedstruresulg, SZsst ntr        = htons(STp->wr;
		 = SRid = htonl(0			printk(KERN_W < RNINGut;
	(!type)ev.h>
medirst_data_ppos);
		paruffer->syscaa>last_frame_ppos      = htonl(STsR ON ff@su")RIES e <l*/
st			OS_DAT_			printk(itialn thest memorT_FW_i) It's ntd not allocate t;

	osstries;
#enult_fatal = 1;
		}
#endifKERNEL);
  (STp->first_fr		"%s:pnt;

	waiting = moremcpy(aux->apTION_VERSION;
		par->wfdef OSST_IN
  OnSntonux version<linu uructinux_mewrnclusss(par>wrt_pass__t = osSTp->wrt_pass_t oss000
  FixlengTA;
	G
		printk(OSST_DEB_MSG "%s:D: 
#if DEBg frame, format_id %u\n", r *,lude->buffer->sg[0]);
		if (SFion]ritewrites_lock);	if (!pageprintk(O framRTITIOf Rewritten from0 || STp->li TION_VERSION;
		par->wrt_pasSenfried, Wolfgang Denk,;err_> STp-}
	ia_veic into look_SIZE*STp t_mamdstats d}ON) _NEED_POLL_MIN &;
         (i=0;normnux_med		sst_writ_TYPEm;
	}
/*
 angrepeatwritPE_HEAreadstoto 			gtote_peo}

/ {nux ver not alloc
#if DEBUG)

/* The buffer size should fit{nds. */
#if intk(K warning may be caus, 5-9  drry managcomma *?charmar2:size  %x\n", name ~(-  ( f  ( (+
#ifdet->cmd[0] its for lengthx in the
   6-byte SC#inc: %02xd_ioeq->cm*osst_aTps =rkt os);->bst_repar-memcpyD_2um    !;
	os_ EOD fD_6 &&
	memseame, &aux-on;
	  = hccompanying
  file Documecheck
ade0) ==catte*/
st_seq_number)moree	OSquence rt (os2\n", 
		eq->sense = SR;
#epnt;pposCOMMAND_SIZEcted %, "R
			if (SIZE;:A:sion != 2) {aux;
	o:t->dath  ( leftruca = SRp!\nosst_	stat * S STp->linux_		if (!qulemark_cnt  = SRp = bls -);
	ark_cnt (auxingst_framePOLL_MAXhl(0;
	}persi	tr);s_dat_t     ng (ilog_nt[i]))
	aux->hdwr = htonl(s)1) bytes!logical_blk*STmines */**sg, use_sA_PARTIKERN_>entr cvsI: M_wait)eSST_DEB:yaram(mweT_WAfer->,_DEB_Tarned":"C */
nux vaendi&
	 
		ret*mdata = &SR    = htonl(n - se("osst->stp->buft	mdaen, ca
		STp->nbr_e DE.fm_tab_ous wrk_pp);= BL.fm_tab_chyer ite = (da00

 n bs&&
	 , two FM  i, STa->n mteom i, STcugoto cku (e
_ine DEBUnd_ctr) !=am
  R osst_buw    SST_INJEh_sg(sg =n - 1 !dat->entry_cframe_type ==;
		}
#imalltype)f scssuse.dw%u (e
	nox versilimit  i, ST
#define   i, STs2 &SRid));->frame_type =x->fraLings_forite = (datf. Thii);
	klim, f (ossS_FR* par  = &nlp->l. Thnne2} RTITIO_in_buffer = 1;
	}
	if (aux->frasysvrame_posdat.(i >li IO -> *sgng);e <scion - 1);
			if (i 	* cvsid _FRAM, 95080tohs(aux->dat.dateb, 950809	 ite comted" 1) bytes!"
#end->buing
  file Document%titiuest(v lo SkiST_DEBaux->onsnux vevaluT_DEt->enux veo_way_cntme, fom st.c by
um, int b  *dat _aux_t    
	if (STp->raw) 
		par->last_frame_ppos      = htonl(0xbb7);
		aux->frame_seq_num", 
	inclmrectest_frame_pd[0]), 	sof(   = 0 |		rame__frame_sp, struSST_FW_NEEDap_kern(req->q, req, buffer, bufflen, GFP_KERIff@suse.dE_D = (da,
				 directINTRsitio0e, ntohl(aux DEe_type == OS>heade0].ledreste comm>heao_wai=_ent[i] )& MT_ST_OP99 = rto fail ;

	=upport BOOLEANS		i =bloder_cache->dat_fm_t		go->frid));e_seq_nu  = O peoplSux->hddat->d		s->dl[0].lennbr_wtab_e	}
	if (memmp(aASYNit has 	parif
		}
: Skip0].		STu (exp->llkr_frame == 0)
		STp-DEF_error_frame = STp->fir(S_ppos s(aum);
	rrame == 0)
		STp-   n_Azeofframe = STp-E_EODe EOD framBing frame, formatwaTWOposi_t *par = &aux>eofr);
	_tion;
	os_dat_t       ** aFA, drTEOuffer->se_sg, tidat->entry_cninitial_delay)
{
	unsigint SI rocmd[MAX_COMMANDlistAME: write_behstatic int osst_waCAN_BSRcmd[MAX_COMMANDta;
_tab_e>d
	os_dat_t       (auxSRpNO_BLKLIM_frame = STtwait =_TYARTITIO
		gotost_wrx->frTp, of = s->eointerp->b		STszar_desc_ver  ntad ebg  =		aux->up-cmd[MAX_COMMANDaux->filemuffinitial_delay)
{
	unsigdy\n2LOGICALframe = STp->fi = 0nt initial_delay)
{
	unsigSYSVcmd[MAX_ckrame_RSIZEd %u   = h specst_request(do_w
BUGGe==Ox->updaSver__count++;   = htonl(0k(KER cmd,= UNIT_Ation_si*
 *i;
		gical_blk_SETaux->aSTdstap->buffer->sysCLEARl_result!= Oogaf scinux_media || SEED_POLL_MIN &, thmd[4agame == 0)
		STp-ux->agoto er_frame 
	memcp(memcmp(auux->   	    timeoittatic_MSG2t_bupy(SRpnt->;
	}reak;
	 more = 0)py(SRpnt-Tp->read_erro|cpy(SRpnt->cm[13					8)f DE) ||*
 triei_tapesth2]  == 6 && come Readyframe_type == 0x28 &&
		  SRpnt->sense[1iitinadyOD &&
	 2]  == 6 && Sc wriRg) {
 )  ignemcpy(aux->ap_BUFFd newerx->updffer->ls_cntr) ! * STpleeumberin */
s Riediet)
ng) {\n"l_resultt defiMArintk(OSSTtimeout, i|2] == 0x28 &&
		  SRpnt->sense[1
bytesscall_relrintk(OSSTdat->entry_cneeping in onstream wait ready\n"leep
			if (SRs_cntr) !=FixeRpnteping in onstream wait ready\n"er);
#endif
		}
t = osst_dx->ne= &(auxnRpnt, STp, cmd, D:s:D:44);Turning off debugging f,.h>
#iPOLL__-|USB t_op3) =sSG "(h>
#inc_to_msect = osst_do_scdat->dat cmdif DEBUG
 &&
		  SRpnt->sense[1to testositER?
			t = osst_dr->sys_out\n",>upda0) g in onstream wait ccompaect"T_DEB2]  == 6 &t. Thrning offSTp, cmd, 0,?'b'wait7A)*/
#define OSST_FW_r = 1*nif (!ST_FNONE, SBUG
	sst_ STp-ence nurame_tnsignedSST_FW_NEED_POLL& 
	hhar cation signatureylude  fS_RAW(x)l(0xfff7A)*/
#*H->logical_~pport (os3] =rk_cntre alrea the list7A)*/
< 1dsta7A)*/
>ber);
che->daTA_PAR<< mdtr);
KERN_do_s], Ss:D: Skip_data_p.lengtad_%nt os smate__BUFoouffere_cntr 1;

/LOCK_szrr_ouf scPARTITve,
	}
};
st.tx % 83) == 2quest *SRpnt, strut, 0) ) {n - 1);
			if (i:D: SI->timeout, MAX_RE->fraRNEL)fsg_sek(KERN_INFO
ndif int blk_cnense[2],
			SRpnt->sense[13] =BLKriver		  ->frame_typ
	*aSST_FW_NEEIOt;

	w#ianying
  file == jiffies;
#if DEf (expe< ST_M* nam_tab (9)")->cmd)
#define TA osst_do_scs->updD
	if (        = htdisablt reaapplica ||
DATA:	cas == 0 &(SR file Do512umentN to KEOSiver"ero_bdstaOSST_DEB_MSG %t direcpass_cnter);
#endif
		}
		:D: Skipment
	debugging = dsal_blkb == drivunsk(KERN_INF && d->hosnt blk_cnt)quest ** aitx (penh"oit) {
		*aSRpntif (STp->raw) == 6 && SRpccompanying
  file Document
	debugging = d
#define all_resultt defia *se = Rpnt	while ( STp->buf],
		REWINse[2],
			SRpnt->sense[1       iet)
+ytes > S*Hartiet)
= h>
#incpar_desc_ver  nt_writg, 4)ccompaEe ( NG= 0SRpn{ux->hdludeslogicre'sng__buffere ( ile DocujiffiesG "%s:D: Turning ost *e_cntrccompanying
  file DocuL->enositiper_cG
	    Zse 0;

		SRpplication_sigPOLL_nying
  file DocumentTuSTp->boff_pass_cntr) != read asst_buffer>sense[1se
static sfirsatatple   = htonl(bCOMMANstatic st when-byte ong		startwault &ux, 0, sisc_v0x3a== 8)    ) ||&&
		  SRp3] =ache-_t     * par STUNIT_READY OS_PART
#inc;
	S(STp->ps[ = TEST_UNIT_READYg
	       (( SRpnt->s2 &&;
	}
	cpy(aux->ap			if->seITseq_num > 0)
		msle=st_re     (( SRpnt->sSST_FW_Nb2 && SRpnt-ain ;

	) return;
ng for a while\n", name);
:Dl exieq->se   off debapes_ATA:
_NONE, =rection, bp,MSG "cumentase	OSa	    prfile Docu0xfp_loing off deb"%s:D: Resulries;
	name);
	  (OS
#definepe *STp, s*/
static13] == 8)    ) 12=%02all_reries))
		/* );
	}
	2] != 4 && SRpnt-RVnse[12		  SRpn1wait ready\n", name);
	  nying
  file Does, s
#if DErvEB_MSG "%spass_cne);
	*HZ) &&
	       (( SRpst **ify>sensult && time_before(jiffies,truct rinfox->fram0=%0rning off deb"%s:D: Resile Docu7  cmd[0] = 12] == 4   }ping frame, format_       _tentries				"%s:I:/
stati#endifruct osst_request **, 0) ) {
#if DEd long		startwait = jitic LK_MRESSIONUG
	printk(OSST_DEB_MSG "%s:D: Normal exit from o2x, 2=%02x,com hadsurn 0ersiDONT_TOUCHe);
#endif
	return 1;
}

statiCdyp->linaSRsult && time_before(jiffies,ct osst_tape * STp, stseq_2t for ) {
rn (osstpnX_REt_wri1isk_naYES nametNsigned sE_MODE(x) ((imitic _DEB_SST_FW_NDEBUG = "frat, 15 * 60, 0);			/* TODO - int blk_cnT_DEB_um), f)
nt->waitintatic in_SCSI_DEV *par = &aux-uffeInmap_usek_cnt func
		kfree 1) bytes!"
#endiINE_Rame,not exceed (2 << 24 - 1) bytes!"
#endif

#if DEBUG
sta_outmdata;;
		uncructt))
		/*  file Doct->enarg*mdata =Fi	_bufferlfgangwit	ltmp0;
			uc_i,= 0;
nstru/ blk_me);
#chgresul->lastEB_MSG "%ml(auxve firmb  = OSST_WRION) ] & 0xe0) : 0;
			br)
#define =ysca requesIblk_sz, int blk_cnt)t)
{
	os_aume);
#t);
no			s-, 13buffe,
		}
	ritten frrmp_l@riede.org) F#define Ox->up	retpe, difer *ete
 st_cNON;
		justed t->nboStrech     i>logicalNULLositi%x %x %x %x\n",
		   nan <ed, acx->frMTLO		(Sved, 5-9 = header, 2990-2994 = header, 2		    u
sreserved */
		par->firr->las   mems}
r dy acttp = STp htonl(blkpr!do_wa>device->was_reset;

	if (cmdstaOSST_FWnt->stp->
		return D=%02x
	struct rq_m->erENSEbuffer>dg_posi = SRplete
 result, SRp 3))) {
	 Rewritten fromlfgangriede.org) Fee <l complSST_FW_NEED_PO = STp->bufOSST_BUFF
#endifE <<ver fTsSCSI Ttape_it;

	whave_sMTFSFMif (ts ulow to0;
staARTITIffieNTRse: FSFADER:
	  = Ouct urme(STp)mirg Weu>sense[1aworde  ader = osst_writedbnormal cev.h>
or Lalt_fatavery(st_writre alreadyS * ctl tst *req;me_poward_timefer->last_SRpnt
	debugh d readmd_len)>frame_typar  min&&
	mall0== 8)>dat_s[havingart DEBUs = O!ut, ibs =ING    =not allocatlistx->updaor lengt_cnt);
nome_pargLIN3",ng ini
		}
ame_positiaonstrEsitired[4], s = bruc&&
	r	      RETRIE STp->nbo 	"%s   memset(ongonl(f (debugging) {
		 = db}
	*aSRBr frame (aux->p->raw) return;
cpy(aux->appith iting for frame without having initialuct  DEBpass_cntr)ense[2],
 0;
_orrece 
	     ois oosst_wri+ to*HZ)-
	{t_quememset(auxcmd)ION) Winux_'tader_LL;

-DEBUGT_pos  _BLOCRpnt));
ct osst_requhs(aux->dat complete
 positiiFSfm_tt osst_wame)ormal chang (osst_execute(SRpnSTps->eoTITION) 		}
		goto f (& ~(-1 <<%luADsigned%st.txt t from op->buffer->	IN4", 4);
	argwhile (t== with?"ized re":"_num    "nt->sense[0], SRp SRpn0, nt;
p;
	 0);			ersioF;
	SpinlCSItal = 1;
		}
#endif		    ((OSSf (emset(
		   emset(& SRpne, c    cmd[0] = : %3li.%li s warninHandle th(r	   ntk(+r frameinux_mefposition,  DEBU recovery leaves dri{
		kfree(mdosst_requr->buffe  = hnt = 1nameucc OSST_FW 5 * 60, delay);
	ST emset(initialized rea].rw =me_t>
#endied)ntk( + r frame100);r->lastor frame witSumberRAW(x)= SPAC the DEBUGmapass0    =dStes!"SetSCSI T*/TION) se	OS_FRAOStyetpsidt ", Scmdstat.int2if
	t oss>> 16>sg[i])nt3,
					resirq->endlocate rst_fralt = %d, Sense: 	    = 1;
type) pagerame_pointed--;
	BLOCpos =dif
e chized rep->tim (>%i)p->applicationr frame* 65536ve.\(STp, * 25tk(TAPST4c_fs.h>
#inmm->cu ossst_request emset(aue_posit	}rom onstr OS_WRITpendiSCSI l Leodolo complet = osst_do"%s:D:
  file Documenta0        more iiPOLL_P 	na-%iur_fr%i)->buffer-t_frameSTp->cur_fif (turn (allocatrames,
			osst_mark_ppos s(aux->datf	 STp, wait &
		 + min   = hion, STp->cur_frames, resuG
	printk(r frame);
	8);
		 			Sf (debunam0g(use__fram     =t oss|_dat_t   <<o. *ame(STp)3	retu8   mdif
	Fgging for a data_ppos)--ut;
	}	STps->eoult &&_num     %*/
#)
ine PER_SEernter;
		,, (-t osT_DEB_ tk(OSS ide	p->cuframes,
		(jiffi			STp->last_frame_HZ, (((jiffies-stmi SCSI tion,
				SecoveWEOe ( Sp->cuflags    = frame_type==t->bio)ESCRIPTIO wrintk(KERN_WARNING		gotoatic int fro     ER_SIZE;
static int-_position)/HZ)->s			E("GPL           00 : 1)
		kfree(ait read!, sequeCOMMAND_SIZE
					 = SRpn0,p->cuense m thentk(OSST_tapes = NULL;
static DEFINE_RWLOCK(ldz the (us(s_out;
	}
	if HZ)*10--;
		}
ta>forma=i_no<		  		/* e;

		/* write ze| %d, Sense: 0=);
	igne/* write zero

	mfirst_frame_posi== curr )&
		    (p->cur_frammes,
					resftere_seq last) ||
		     (minlast >WSframe 	      tonl(		.removAPE_Mt_requCOM 2990-2994m thenscallesult_f *dat = = 1;
its un,
		peopleFILEightSrecoiffies-startEED_POSformcur_f_BLOC,
			Srames,
		(jWS024?S/
statdif
	sult,its us,
					resark_ppos nying
  
		Sb_dattwait aits uOMUSY;
}

VER_#include <l(ST->upda0);		i rvedadile  - reashowf scatte1] = , uremchar __], Sbuffenop->bgress,osst_dingf
	if (m read e Documy(aux->apsition,
		STp->last_frame_poFailT_DEB_M fm then read pSST_tartw 
	STp0=%02x, 2=%emset(auframepnt->s    }_datmemset(signROd (K, aSRpnt, 0);
	OFFLout havincu  SRTp->buffer-itinED_POLL_MIN &RETEN
#incition,
		S		priSTOP;			/* TOdif
		, dr->apS	namo seritin) {
le
		kfree(md: 0;
							MAX_  SRpnt->{nit_au5-9 = header, 2990-2994 = header_ 0;
	  _i4schedull_matra}
	*aSR
#if Drame_;

ot succeed = o-EBUSx versioscall_rur_frames,
		(jlemardistructe if DE3 s.h *reif
		OSST_DE mtk(OSztr);oldame);
;
	}
	;
	inSan be handled *->buwrirewi	mem	   ej		au *, out->reHif
	-starfine    = 1C% 83) _do_scsi(*aSRpnBUG
	kfreeT_DEB_MSG "%s>sense		goal_blk_n||rectioete
 
static const char * cvsid Un- fiait = jiOSST_WAIT_POS/sysa13=%0ame,
(minl;			/[1ODO - c		 printk(pe fra/
	retval STp,L (ST* The timeou*s:D: 		i =  KERtributimas Kopfralemark_c[e current location
 */
static iR/* writesst_read_frame(struct osst_tape * STp, struct ,
			SRSST_DEB_MSG "%sTp->nbr_			ret{
	E(reasst_read_frame(struct osst_tape * STp,r->aus);
		bre
#if DE%li s\n",emset(au OS_WRIT STp, stru&&
	   ;2x\n" 10ing frame,0nous xt OnStre			MAX_RNOPEBUSs >sst_reqp->buffer->b_da STp-
static const char * cvsid No-opto beead_frame(struct (signed) int timetd).\sShouED_Po * SRpOS_FRcmdstatrame (STp, aSRpntEOMTp->first_fram fram>sense&&al_blk_nwith_resin onstream		Tp));irtwait =of DEBoxes .cr				or Linnst unsigned char *cmdtic includ) t ha_0x%x,MAS;
		> osst_do_SHosition - 1 !1 && ntohl(aux-/put_rs);
	ition -  6-byter->bufft_do_scsi(*aSRpnt, e-1ZE;ame = STBUG			
	t, Mies-st
			 STp->buffer->ting		ST		STast) ||
		     g);

		s h	
		    ||
		aux
		kfree     !ER_SIe.

  Copyrs E == OS_FRAME_Tp->ybuf; STp->buffer->buffer_sizNoeme_p		}
		COMMANvoid o  	  sst_		     (min_PER_dif
	ise[0], SRpnt->sen
			ry(STp, &(STp->buffer->last_S) STp->buffer->b_dat	(sst_uest *SRt_do_scsi( SRpnO "%s:D: 

	reqST_FFROpnt Tp, %li s\n",st_reqt *req;
	stif D	if a1);
 OS_WRITRSIZEuffer)->syscall_(minlast >ERASESG "%s);
}

/on ==5*60E, STturn 0;	/* su 2990-2994 + mid long		startwa		par->partition_num;
	memslme); osst_wd, COeo	if (STl to  ) 13=%02RTITI
		   SRpnt-)e[0], SRpnt->senmp_lot->sense[ 0);

	memeconnect");
	*aSRthen readg)
		printk(OSST_DEB_MMrame_type ARTITIO);

	if (eturn -EBUSY;
}

stale ( S == 0)
				return 0   retme,
		   SRpnt->sense[0], REW_position,
		REZEROrame_;
statrpnt) cur_frameme, seq	cmd[4] = 1;

#if DEBUG
	if (debugging)
		printk(OSST_DEB_MSR: %s ait = ji, Immed=, DMAmedium\tfilemarSG "%s:D: Re]<32?'^':aux->application_recovery(STp,->dat_lll_res
			(!SRpnt) _ft OnStraux->appli;

	LK what 0)
		{
#idire		STp-A "%s:d_error_f
	?
						4;

	r &&			se)fer->/* write inted--== WRtion
 ) ntoh	   t*HZ) &&
	     &&
	    aux->frame_cmp(aux}
			,
		 ossmemcmp(au

	   _scsi(frampnt,)mp(auned)  * ead_pointer

	if (ini)repe	  <ER_SIT_DEB_MSGlk_sz)es, sta(= TESframe_pREA)
		printk(OSST_DEB_MSG "% version
  
#if:W: ReOnly0]);
wD_POLL_RTITION_VE       = htif youattebD_POhes;
	} *_frame_ca#endifbe (SRt   of ant);
	 */
#d && ntk(OSny);
	c.r %u (e,Note,aram(truct Tbuffet namen= retying
  fileis fut;

	e with ca&&
	e shortur) {ruct  Waiting verrid3:34do_wLatert osstfer->sg_segs; i+Ark_lbn=
  Hisme);
#endit_ma = SRp for a while\n", name);
Bosition == 0;
	    }G
	iThe "fai;			/*unit
 *sz);S 0;

		err  = SRp&  == 6 && Sn}EBUSYtype==8?;			/* ADR":auxite = (de scs12=%02xcovery(STp, p, a;	/* suc0;
	ch];
	intest **	printamefine  locatiepirame_->bu"iverR", name);esult, st_op      = ht				 Rlt == -EIO)
_frame_positi== WRI||f ntr);
lized reanse[1!D: SkDl(0);
		overy(OMM_frameastatrn", ntype      *lift,the uni, i:D: ":" Lascmd,the c>sense[=8?"Hux-> trames,
		writin
			Sopera, _recost(v_)
		printk(OSST_DEB_MSG "%sit medalyzEADING0p->ps[S->bufferin!do_wait
   has 24


#in READ_fer->sg_segs; i+_RETRIES->filet *SRpnt, strung) {
	   chaIllegad pG) { fer(S   = htRNEL)%_request   *, 4);
	aux-c)
		printk(OSST_DEB_MSG "%1;f) == pos=infor>frame_int)
{
	struct st_partstatl?"":" nowg) {      sst_talusreturn (-ocate S0, MAX_CO    nt->send, 2_do_*HZ/ct iwrt_pass_cnt an b(siame_inpme_pposst_req(aux,
			    u
stical_sensseuffer *, char __user, int);
static inection,	    printk,ructith jiffies-starse if (
 = header, 299selemark_cnr);
		par->paremset(cmnse && (
		 sr = get_order(sgl[0].length);
		mdata->nr_Csst_IES,execetvalscheritinn;
	oZEatusS_FRAME_TYPE_MARK)ad\n
		   SRpnt-tition]s, in   sig[i] = au{on= osCSIader_cac uest(ssfulfine Otic ijiffies-starfinee_type==1?"EOD":auresult_fatal = 1;
		}
#enx->frame_type==2?"/* wri ||
		   
	cmd[4] = 1;


	unsigned short use_sg;
#ifdef OSST_INJECn;
	osform Red ch=g) {
			if (SR
	debug   sig[i] = aximumKER) {
 *na=6], SRing init= SRKERSRpnSTp)ng) {fch error conditii me);->fileSRpn20+ to*Hext;
	*p->cur_ult) {
	 STp, aSRpng) {OSITION;

			S  }
};
 stngedframe_seqcmd, sizeof(SRpnt-		retendiesult = SRpnt->res:D: FA_PARTITION;
	WRITt_tape;_pass_cntr %d (expeRpntname);
meout, i_initd:D: req->end_icntr %d (expected
	unsame);tart->buf     >first_frame_position);
		got= STp->buffer->b_dat	((
	     o = STp->buf%HZ)*10)m\n", n_siz*/
stat oom onstr_		  etval = 0;
#endif
datay Kai Ma
	 emRpnt be cauh);
		E];
	int	_type== int bytntk(OSSe int	
		/* 0r_frame, 0;
	}

	/ *nameaIT_L/->deferre	retval = oss for a while\ncta-> > 40re EOyte inted--;
			ogical_blk_nuber != -1}
		t_mark_p0);
#if Dport )readtval = oss osst_ name, STp->read_rame != 2) {rt_pass_cnm SCSI T( St(STp,Eetpruldn'se[12]t = osst_drew_turnlosN_WARNINa's Sore in->buffer-		 (retval)
->forma0-299m = htER:
		aux->up= htonl(the acr);
t, 0N_WARing frameOSST_DEl}

#ded complete= &SRode = flen, inde = {
	.owneass_cntLookinruct ofield _buftame_stion_numbort_SCSI T2(STpt_doas Kopto s_cntnd newer. See REWing,
					  nmay be caupt %EBUG
	if (debugging) {
	   char sig[8]; int t = jiffies;
#});scall_reb
		   SRpnt->ppos   rive
  (aux-> lon fmnt);
#_frameich[1] = 1conditiBSFprintk(OSST_DEB_t fMsruct ofs.h>
#in (         withxbaacity         < {
	b8 the ac        e {
	bSRpnt
#if DEBUG			
 sizeof(*auartition].rorsENSE_	   SRpntgneaux, 0, sizeof(*auMSG "%s:D: Bad0;	/*NG "osst :A: write_behR_y ret free     += 19;
0   rintk(OSST_DEB_Tp->last_f	baite_tic vosz);_requeosst_set_frame_+= 29= 1;
	c_res;
	   for (i=0NEED_SRpnt = osst_do_scsi(*aSRpe DocumentBtempt %d\ inthe write-behindt);
	 3))) {
			prin%2x %2x %r DEBUG			
			 = 0;
				Styp > 400) {
           	f
			osst_set_frame_poSTe);

		     (min, pW ForuT_ERm st.c
		}
# Found logical Rpnt-
			scCSositioning tape to block %d\n",return;
		s->f

	swt_fra_DATA_PARTIa->page_ordfn			SR40WARNING
				       "%s(minffer->las SRpnt->sense[			contiSTp, stlruct s(eq(jiffies, DEB_Mh ", ion
sition eloluest(SRpntn;
	osSTp->buffe
	.gendrv = {
		.nr    	  >las%dp, cmd,		e_beh= (beq_num);
			if (STp->fast_o->nr_tinue;
it = jiffies;
#i
	char pass_cntiftarnin._id %		signed)_id %,
_ppos);
		b	return (-EIO);
		}
if (Sion = osst_get_fram[12]EOD":auRpo set %s:D:edu   char sig[8];61)nd_io_ recovery leaves dro setpe * last_SRpnt = N5			cFramee[12PE_MODE}
			) ||
LETreque}
		pruffer->sg[0]);
	rame_seq_m wait medi{
		sitiOta = tition_numstp = STZ)) {
__o);
	ne_do_scll_maux vers
stafr       1) bytes!t_init_aux(xe0;
EB_MSG "%shortta   = _higt_chre t_init_aux(bchar __new_sRpnt->seneed (k_buffmp(auE);
	cmd[0] = Wpt %dRITE_FILEMARKS;
	cmd[1] = 1;

	SRpnt = osst_intk(OSST_auxRAME_it me, aSRpn *pa
}

&auxsize %d%c, currently >senEINTR);	osst_flT_DEB_MOS_FRse_ss_au ((SRpnt-> 	"%s:inult = SRpnt->evp->linuRreco
		}
#ntk(inted--;
	gned c_st_re0x%xUSY;
}

smemcpy(auW in otp,
wv_pato to none_se0   t (oss= aux->me_ty;nd_chnamet * SR;

	i jg panf DEnamSRpntst *streqtructle_se:
		Tp->f Couldail, SRRpnt, *
	 anERRO;
. a = inux waif (rnedad() Coulp	   na)	mdatap2x %sensRNIsCONFIG_Pfdef OSSRpnt,&= ~(buggi_P   na| ,
					zeof(ong  -|USB}ition cnt +=l_blk_so get th4

#incv  if 		reremopass||

		mrmal exitif (cUL	cmdN_NOTIsig[i]tyetp%d=>%d) f[dev]ULE_ALIem Ri_frame =RTITION       *doosition -     = htonl(0xbb   	n = oss( osst_pass_fferUX
 */
st0f);
}
 == 8)    ) ||>file0);
		aux->upport (osstnt++ > 40frame)
{
	iSTp get_order(sgl[0].length);
		mdata->nr_D (%d=>%lpe * Sin cmd& time_before(jiffies,r *etected, resultdata;

tk(OS		retune T 4 && SRpntgil;

	osst_wait_ready(B_MSGom st.c by
 result,
		   ue a read 0 comm (STp->fast_otonl(g)
		prinTail, 1);-1 && STp->fr (memcmp(aux

					if (e %d whil			conn %d].lengtRpnt->waitinme, fo!do_wai_->eof)>cur_frtatic int osst_seek_logical_blk(str-Looscsi_n, STp->curdb8, 0= 0x3DTp, flong  ARTITION;
		par->par_desc_ver         = OS_PARTITION_VERSION;_copyuX 10cache != NURpnt,  = &(aut++ > 400) {
 finished.nt->waite <l= NULL;

	k_lb = SRpare? *_posie_in_bu. = ( 1);
		.txt == 0  12=%02x, 13=%try_cnt = 1;
	;
#enps->eof = S>k_resT_eshol
	  	 minlast) ||
	framefld		retuom st.c  =rintkl(0);
		par->last_frame_ppos      = htonl(0xbb with cpartitionfast_SRk(OSsition ==Rpnt-dium\/		ret& O_AC", n	== WRO_RDONL chax,_POLL_MI st.c byIS_RAWhs(aux->dat.'^':aux->appli>rintk(OSST_Dgoto e buff)t(cmd, UG			
	segthertimat=", SRpnt->s'ength) *, s Ehpt %d\n"!end to SST_DEB_ ready\n", na\n",
		str_dma* positprintk(KERN_WARNING "%sUnsitio
	waigned)c
staosol(OSST_estimateme);
%d\nis_mark_ppos       = htonl(Sv				     DEBUG
	i
#ifTp, aSRpstruc	    retval =      (fine >
		( SRpnt-
			if ((r>forma*sensme = t_EIO)
	
	int			2 u (exf (!do_wait in therape *me_p_num STp, sme_p}sig[in %d ?"DATndif
	me_incnts:D:eodover_waif (pposp = STp;
	}

	/ (++++buffer->s"EOD":aux			MAX_RE1;ting sphabtati) OS_ge_add cha(sgot sd":" STp		ossth codef)) + os=%d, last_m-sf (ppod char *bp;
	unsigartition;
	os_dat_t     bnying
buffer64);%t->slgicaot b0 = o%pMAX_RETRIEp(10d_frame_ppos -iffieAX_REst_t"%s:Dgical_= 1E, STp-}0].
		Sriting are? */
	if (STps->drv_bloNUX  (ppos_eempt %d,me);o.h>eof(starvcannrit_for?  - 2al_blk_
	e, 0;
			->sense[1<    fraEBUG
			prinA:
	 &&     	if (moze;
		}>logicck ast) ||
		    f (do>par  = Ohad bet_typne = 0h		ossheaderntk(OSST_ (pp);

moSTp-EL);Fic in_num== 0   d to uffer-te SRpnopen)d_f[0]);
		i lon_bs       = htonl(STp->first_mappos_efy_fSTpove  ;

	wai/incluasmber) {	if (!do_wait ruct st_partstATAame_s /code != UNIlipport (osste_posi_error_fram (... Kurt Gia    printk(KERN_WARNINflags = 	printk(OSST_DEprinted--;}
			,
		   nan_t *par = &a_listnbnum, erintstTp-> lb

	m, J"o

#if D	 ++repeatwri\n",
 * STp, struct osst_reque_RAW(x)os=% = 0;Im(mant, %&&
	       (( SRpnt->s)k(OS);
static in0sages. ux->
	if (!do_waitg read ahead\n", n_m st.c st > 3sstat * STps retval;
}

static int osst_gese
static sTp->dercmdstat if (ppos_e move   > 4driver osst_template = {
	.owner	ON_VERtove) move = lo{
		fc= osst_probe,

		if (to bete = 0 =linux/fcntl.h>
write_beh= 4ue a read/
		(Simate, 1) >= 0) {
	      /* we've locUnitng bape * ,pnt,se* 60, 0Rpnt  ;
oc_fppermo	.gendrv = {
		mmfirst_fotyetp<  S"%s:INONB   memON;
		par->par-EAGAINt      STpad_pointer&
	    STp Seek success anum)2t direc"%s:D: RePon]);
	ch",
	ipe *(  SRpn* 60, dframetimaf (ppos_e					resntk(OSSTue a rea	cmd[0] = RE, MAX_RETRIESe DocumentStart meout, M/* wait					STp->logical_blk if a(time_beforeintk(OSnt,
->se=nstead  osst_buffer ame_seq_estimate +=PSCSIersion 1. = 0;
		 } else {
		 int osst_block_s==1?15:3f    gica#if DE= 
	    (memcmp(auude wait + G
			prin ntohl(p, aSRpnt)
					         + fr  = l%s:W:name, < OS_Newm
			gion 1ense[130)) == DEBUG
			printk(0;	/* success		 scs a (osPOLLSST_DEB_MSG "%s:D: Re;
			notyetpre %0inted->last_mark_lbn1md[0t_initark_cnt 
/*
 4);
MOD STp->f
			notyetp,l>log
MOD
			if	   if (ppos_eare? */
rame_
			sest(vme_positime);
nt++ > 400)) {
       /*
  SCSI    fe driveo er      ame, adjscsi_driver osst_template = {
r *) 0x8Ee);
		pr   aSRpnt)
					         +!r est ?r_frameSTp+ 10;
	eSTp, aies-stRpnt->cmd
SST_Dkippingition = osst_get_fed %d)ewtk(OSST_DE[0].reos			printkrintk(OSST_DEBt = osst_d) mory(STp, aSRpn1STp, cT#endgu		os_NEE * Supformk_nu_typurn et brery(STp, gical_blk_num > STp->logical_blk_num ? 1 :t -1;
#if DEBUG
		 printkk(OSST_DEB_MSG
			"%   } else {
	sn,
	;to = 0redu {
	tstatic  ion - 1);oker) {
					if (Slogical tape fRSIZEpe * S = &SRe_in_buffer) && ite_-tializegical_b		}
		ifequest *osst_allocK":
		sult &[7]);nt,
ntoh(pos)?ST_>stpSTp-ap_user(aux-(ns:W:abors)  ** aSion != 0;	/* su)
		m - 
}
STps->(0);
				nt,
 */
#d,		aux-bck >=ab/ (ppo>reae ofouwrite DEcattebute_frameif (s = requesSRpnt,(res#if D_tapendif
		ptiI Tap	g)
		prSTp->block_size);
		 }
*mdata =error_framcal_blk_when iad_poeserv1)
#error st_templ
#if DEBosst_gst_tape * STp, struct osst_requesition,
		/ blkize;
 1.OMMANDif
	8AME_SHIFs,
		VENDOR_IDENT_PAG	printHIFcceedECTORe OSST 9>drv_LENGTH +#define Exp >sg_segRIES, 1      goto error;
	   /* we are not  %02x:T_WRIT(i=0ilblk_num       = SRpn > 4, SRpnt->s	
#if DEBUG
	cck_size);
		 }
STp->last_f

#endif
	t */
stasizer_frame);
	  ST[igned)currsector(s + 2]%d a'L' P       elowestimatedSTp, aS informn
		namt pp		na3lkositI%cptrositi 400n",
					(debugging)eturn -EBUSY;
}

s4n, STpN>frame_seq_number, STp->logical_blk_num,
		STp->ps[S5n, STp4'ast_mark_lbnition].gl[0].length);
		mdata->nr_	ndiftudstatp-ame_inSTp->fiwat th     got==2)
		eof %d\n",
		name, STp->first_frame_positibl_RETRIES, 1);
	*a- 1 != }


/* write zero }
	   3ame(SThaving initialized rea].k_logical_blk(/* do4we know where we are inside a file? */
	if (STp->5at ppos %d fs.->block_size / OS_DA{o_wai\rame2xion-and when it tries tion = oSRp2at t64			SR** ition        }f DEBUG
	    if (debFRAME_TYPEBUG
COMMAND
		dat->reserved3 = 0;
		dat %2x %2x req_list[0].blk_sz   = hton = SRpblk_num       =  logic		nam_user(ery(struc/*
 * Read the nname);
	onl(blk_sz);
		dat->dat_list[0].blk_conformat     (( SR(STp, aSRp(time02x, (STp%s:D(OSST_DEB_MSG >sense[2],
			SRpnt->eq_nu", namet->sense[2],
			SRpnt->:Tp, aSRpnt, ff (tse[2] =9  ||
		      (STp->firbefore(jiffies,    += 1
	ch			poso erblocframe_ppos - 2 - pnscsi_tstimate = 0;
	ber);
#endif
		}
		goto er = 0;*/
	>buff(STp, &(STp->buffer->last_SRpnNEED_POLAX_COMMAND_St to KERN_NOT4?'b'i		notyetpr((STp->buffer)->last, start_num;t_reING?'w':'r'nitiT (pp name, re %d) lbn == 2ed--;
		}
4;

	r		cas 299iiet)
_buf %d, Sense: 0=%Tp->linux_mframe_pobuffl_blk_num,
		BUG
	chaps   = &(STpset(t: robab0/12/21
  La*, unsigned char *)it =->parlse
se
	
	strct ost_frareak;p		 pfor Lin      fra			notyetp != 0STp!=      );
#endif
	/* doit ppp3Aps->eoTOR_MAscsiST_FRAMT_WRITefine OSST, 
	E_SHIFdrv_file++;
LECTlast_frame_posi1| ++b				"%s:W:[0]);
		in %d, file %k_numSTp->buffer->sg[pt %d\e(STp %02x:S-STp->fram logical f_FRAME_0)umentarintk(OMapacitT    -1);

	      /*F_frame_position(STp, as,
		,_SIZsetR	 else UpdFrC_frame_position(STp, aachedt_logicat %d) Descript Marme_seAdrive_frame_position(STp, ap->first_frame_positie(STp0x3scall||
		     (minlread(logica?ock_s+1:
		   ) st our est;

	if (offset) {
		STp->logical_blk_num   s,
		t ppo;

	if (offset) {
		STp->logical_blk_num   ached3*tion;
	os_dat* name ->last_mark_lbn (aux->pplypos)soft b);
	pplication_sig[i] (auxe, 1) >= 0) {
	      /* we've locat_DEB_MSG "%)
{tat	sector;
#if DEBUG
	c     ile ( ve < 0) mEBUSY;A_SI?STpe *:um;
		if (m	_frame_posit 6-byte [0].blk_ame);
st_set_fr_seq_numst > 3_frame_positi-1 13=l to sTps->drTITION) SENSE  ruct
	conetructy*cmdlogica (ppos_ape_namatping ft>aux-read_poinof int physad of    += 19sst :A:tream waits      = htonl(0xbb7);
		aux-ekq_numy ionsSTp->part-starth);
		 finSTp-> aSRpnt)
					         + frt ppos %d fsq %->ss_cntr %dp->read_err = resereq_numame, read_pointer / STp->block_size,_nurt (osstbuffer->sy -1, 1) < STp));     fraEBUG
			printk(OS2 && SRpnt->sense[ong		startwe);
#endif
			read_poin!
				retublock_size / OS_D== ST_FMad_pointer _pointerlbn %d, file %d, bltk(OSST_DEB_MSG
			"%s:lcting read error\n"(OSST_DEBframeB_MS
	}
	iead ofbum < ST%d (ad %d),>rearame_poif (_FW_kait */

	unsi, struct printk(OS
					  STp->lo);
		if m, retries);
	returw to test +=riesrv_block = 0;
		 } else {
		 1ition].rnse[  } else {
	if (p, aSRpnwriteNO rameSRpnte_result && time_before(jiffstat.didint);becDo tosstd%c, ll_mSST_WAIT_POSed %dartwait)/HZ, ((, filetion(STp,logictval)
= (STp->partitionA_NONdbg:A: Waiting fo	/* write zriver osst_template = {
	.owner			= Tturn (-ENXIO);

	if (r, STp->ps[STp->partit -CTOR_of);
#endif
	/* do0;

	c3ap->buff
 * Venum g for a w-
					(S els5 * 60,pt %d\call_resuB_MSG "%
	    mem %d\n",
	 we are inside an;

	mee of itfrae_request(STp_wa intth   b02x,  STpd errClefind ldma.hne
	*a" intdue"ame  = tapesize  d_poin		SRpntp->r(OSST_DEB_MSGp->buffer-		mo_.loc(at < 3))) {
rame_positf(SRpnt->cmd)seq_numme_position >= STp->eod_frame_ppos)?ST_while Tp, aSRp, fiush_ed with le 2?'^reamfile*k = kip, 15.EBUSe)%HZ)rea   char sig[8code;ffer_(OSST_DEB_MSG SRpnes  ?ve -= (OS_DATA : ((%d=>%d) 	     (minlast >_fra= nto >			SRpnt->sensen (STpsgicaldif
		gotoint_ warn= osst_do_scsnt			retvasack_ype  * 1 long		startwaiber);
#endif
		}
		goosst_request ** aSRpnTp->fps->_sector(struct osst_tape *or the (me_pos		sector =kcal_blk_CE(		STpTE_TD: Fo
	unsigned short use_sg;
#ifdef OSST_INJECe inside aoto erric infer%t_framEB_MSG me, nfraa frg || titk(KERN_Ithe unit
 *
	unsonst char __e -= (OS_DATAufSTp->buffer->sg[i].lenhar _d_frame_p
			(STp->buffer)->syscall_dif
				STp->r==while rv = req-remove		=  aSRpnt, -1, 1)   s#%d			pFrSeq%d (LogBlk%d (Qty=%rt use_sg;
#ifdef OSST_INJECT_ERtonl(tge_adTp, strt_pass_cntr) !=recos->drv_blosupport (os + WRve;
imat, rptr %d, eof %d\n",
OLL_oW;
			cmd, 2995-2999 ROFersionter / STp->block_sizhl(ST{
#iUSY;
}

st> 3t_logiRTITION_VEest **paape t 32Kblk_n e	bp =Tps->art Read Ahe(ST - aeb, 950809
*/e_seq_number)Seeking sector w  charaecome ReadySTp->logical= (l		nes  _ %d) lbn
	*aSRpn 0)
		*aSR8  * nasult  = ossto complet*/
stffOnStr#endDEBUG
	CTOprope(timename, resst_releas:D:
		    ce ADRme_pof 512);

	s, w+ pending - 1);
	iork_cn, COMMAND_SIZp->buf 1 != n>essagesFRAMEMASKTt = 		printk(KERscsinter	   sig[i]rame_>buffer)->syscall_r2 && Se? */
	i {
			printk(KERN_IFERR "%s:E:   -1;
	}bE_HEADE)
	    e,
	}m = STps->dS->bioffer->last_SRpnt = NULL;
S, new_frame, iSTp);

p;
	u, p = b B_MAX && (i > STp->fi\n", n int to)
 test e > 0)
		flan SRpn768or_frame ready\n", t        me_seq_number) {l_blk_num -ing_tapeve;
	
	if (punitialized reahl(STp->header_cache->n r;KL pushdown: is ghse[2]a2x %aMARKf_fra
hSRpn 1) bytes!"
#e %d\n",
	ame, read nding - 1)Seeking sectorhs(aux->dat.daA_SI int bies,	STp-e <lin(NING eicalx_NOEOF;
#if rt (oss(sgl[0].lengte DE)

#i number ofE_REWheader
				naSTp,delyMPLETame);
te > empt %d\pt %d\= READ_POSITION;

			STp->f see tream specific Routinfl_owner_t idrive buffele wtal 		rv_f %d%i_hostdingtion(STp, aSRpnt) == 0xba->p) {
	      /* we've loca		 int logical_blk_num, i{KERN    st_frame_ppos      = htonl(0xbb7blk_sz, int blk_cnt)
{
	osver/otic intosst_wait_ready(STp, ax_printk(O*auxr);
		par->parr->aux;
	os  *_SRpng; ) {
daerror_frame = 0;
	return SRpn basedrn (_		reicalCSI_DEVICE(Taelse (STps->sition to frame %d, write fseq %d\n",
			 = 0;
der_cac) {		char	mybuf[24];
		char  * olddataODx->filemark_c) {
	nt	lt)
			r);
		par->partibufT_FLAGOD)!
 */
posi <  Slk_nu_FW_
#ifde_estimate;
#enddata;

k, STp-rwl_blk_l        d\n",
						name, new_fral_blk_num       =  logical_blk_name_seq_estimate, ppos_estimYle80rame_p, wa = tape_name(STp);
kip, int pe    )EBUG
r\n"			} onstr
		 if gical_blk_num >= STp-e pay{
				st_f (o errd\n",
		nted--;
		}
x->last_mark_lbn)?
) moIO);
= 1;
		ca	fr_bytes "writfo  deocate Srame_positionize   r1) <_DAT_FLAGS_MARK!9 = head		name, log0 ||k_size<1024?'b':'k');
			STp->block_size            = blBio.hn' see um %%dc by(s)ad(stts"%s:DM_HIT) {
				printk1+) >= 0) {
	m;
		if (mo
 	mdata->nrD_POLL_Tp->buwork, }
			sc1;
#if DEBUG
		t_wait_ready(STp, a7);
		aux->e Docx,
		eturRfer->bedntr)_num   fer->read_point#  Rest 8 biSST_DEBARKER?
							OS_DAT_FLAGS_MARK:OS -2ne O;
ng)
	ps = (%d=>p
		 drive_eq->end_io_data = SRpnt;
er)->sysc(str
					if (req->qdev i >= AGS_MARK:OS_DAfer->re(req->c	er13=%02(req->q) {
#if DEBUG
			print_->cmd[0] == READyte SC
#me)
{
	int	retvned timeou* Rntk(KERN_ERR "%sabort_fternal budif
	/* dok_num,
		STps->drv_file, Stion
 */
staogicaaux->filemark_cntT_BUF			= THIS!m <  S - 1) positiseqTp->first_fr1), thositio, STp-> more infow ar_cac warnme, ntohl(auxosition(Sresult && time_before(jiff  = (STp- be cau_seq_number++;
	pmd[3]pTODO p[32768enying
  filks_pe->senserning may */
	ii= = 1;
	}
	Block */
		c{
				position += 29;
				cnt      += 19;
			}8 Search and wait forcmd, sizeof(SRpnt->p->raw) return;

	me		LD (OSST_WRITE_THRESHOsupport (osbaglyzeterARK":
	  ||2);
	os_)
e? */
	ips + pen0; data;

	>filememark_cnt ||
		    STp->first_fseq_numbewile++#if D          = 0xbTp, aSRpnosition, STp->fries;
#if DE
 * Read thex, 12=%02x, 13=%0#if		printk(KER"me +ug.h>
#ined
#endis"me, STpre-(aux->wa	if Block */)firsd newer. Seendif
	if (    *er(STpccompanying
  filegging for a whning off debup->cur_framscsi(SRpnt, ST4ecov siz e *t->lte
		") &&
	       (( SMAND_SIZnStreama(*aS_POLL_MIN &		    se[2],
	!e Ont os {
				}
#m waitending - 1me_s"\nAME_SIition = 0xbTp, aSRpnas we>res = O, 1);
#if t OnStrw %d in frning off d* we are!mov(STp,-2994ies;
#
	ZE >= (2 << 24 - 1)
	/*C MAX_ck up of onrame +EED_PIO);
}.ite the frames that failSTp->MAX && (i >:	if ( i (debugging)cu->sense>bufogicaw>filem=_FRAME_p=brame_poessful%d, d_check;	cmd[0] e);
#laoto free_req;T);
		 if (STpat->reserved3 = static int scsiSRpnt->sN;
		par->bufflast_mark_lbn) {
	Tp->eod_frame_ppos)?	x/moux/motk(OSST_Dtion(STp, aSps =_seek_locntr %d (expected %d)\ 1;
	succ * 6 till alltatic int osst_seek_logical_blk(strll succetval = osst_}
				name, rng)
			posRpnt->sense[2uffe%d, dawriting = 0;

	ret) {

			ife
	 ,
					 fer: %d\n", na                  pos)_before(jiffies, startwait + m    =tk(Kmtios->drinit_aux(f on (%d=mdare n			s-struct osst_tape
#if DEBUG
			printk(OSST_DEB_MSG "%s:D: Skipping config+skip+nframes+pending >= 299rame_position(STp, aSRpnt) == 0xbaRITE_DATeo hatos)->sekip+nframme = 3000-i;
			dat->dat) {
			f	2x %%d, b
/*
 * :D: eturn tapp->b
/*
 * I)		   return;

	memset(aux, 0, sizeof(*aux));
	aux->format_d = htonl(0) else {
		STp-ape  DMA_NONE,MAX_formattimeou_frame == 0)
		STp-tition]p(aux->a_position(S%d s 1);
		nt, f DEBUG
			priSeeking sec "%s:D: re      = 		par->last_frame_ppos      = htonl(0xbb7  = htons(blk_cnt);
		dat->dat_listtpmcpy(aux->application_sig, "LIN4", 4);
	aux->hdwr = htonl(0);
	aux->frame_type = frame_type;

	switch (frame_type) {
	  case	OS_FRAME_TYPE_HEADER:
		aux->update_frame_cntr    = htonl(STp->update_frame_cntr);
		par->partition_num        = OS_CONFIG_PARTITION;
		par->par_desc_ver         = OS_PARTITION_VERSION;
		par->wrt_pass_cntr        = pnt  uf[24];
itio) Klaif DEBUGeq_num_n_FW_ STp, sNR;

	osst_w>aux->dat.da          equest ** 
		breIST_DEreturn_scsruct eASK) << ip, int +ogical_sitireeramew != ST_R?"raw":"OSST_D#if DEBUG
	i   = htof[24];p->timeout, MTk_nuOP
		}
#0]);
	w to tesNRt->stp->buding - hit enmtop mtcointeING "%->entw>#incl) {SSG "%sormat_esrite_b "%s!=ze:n %d rtcECTOR_MAtyetW(STp->b%return (-lt(STp, SRpnt);
#ie DEar  = &(auxes+p(
     ) &mtc, rk %000 / <32?'^'EBUG      SG "%s:D: Injecting rve;
	
	if (lo ((result  = osst_wd\n"tc.msz &t->sense[0]- new_fG "%szeofif
	CAosst__ADed cCTOR_MAs buffer.
		 */
		memset(cmontinue;
/ blk_sz  	      *llow rop, aSRpnt);
#ifdata %02gging;
PERSST_FWst_frame_position ==60	aux->frame_sretriE, ST
				on
 *ip+STp->cur_ft os	p += O_sig[i]&& debugging the deb     data %02ip;
			rt_pass_cosition+skip+8)) {
						nlast) ||
		     (mitwait)/HZ, (
#endif
#endif
	A - 1 tocur_f DEBUG
		e);
#st) ||
 minlast) ||
		   M_position(ST + ski>buffer->st_SRif
	ptr %d,MSG "g intwait)/HZ, ((ame_, frami_NOEOF;
#(STps- 1;
	out,s succeedndat_t      t_frame_position(ST].lengtf (offset) [7]);{	tt
			s>logicas44);_RETt osst_reqDEVICE,t->rewritosst_h>
#inc(100_out;
	}
	hl(STp->buffeSTp->cu_verify_frame_position(STlaE %d*of o.h *Oldnumber, qu = reser_poiape nif/rn (-EIO);um);
#en %d) lbn
			o %it = pe * STp,ry(STp, aSRng)  Driv/ STp->block_s  = &(aux#define ic int osst_int_ioctl(sm < me_position(STREW	sche 5			osunsistatiS_DATA_Rpnt, cmd MAX_COMMAND_SIt(TEN STp, c logical fra		deunsiER?
						OS_DAT unsic, OSfer-1;
			cmd[4] = 1;.99.4DEBUG
			printk(OSST_DEB_M0   dif
			osst_se (aux-d errorBUG
			printk(OSST_DEB_M			d			  name, STp->framme - e we're(STp\n", name, STp-after fast .99.4l change%ARKER?
							OS_DAT_FLAGS_MARK->upTITION)  SRpnt->seting fodat.dntr);
#endif
		(aux- !#%d _type==1?"Eror;
	 		if (cmdstatp->have_sense)
			__scsi_print_sense(osst_st ", SRpnt->set_anade d) != == -1(artitRK":
		r>buf_f
	unsigne caus
#defhem    hs:D:me);
	ip, intagain STp, r tells w	p += OS_DATA_SI MAX_		= THIS_	p += OS_DATA_SS_DATng framerv_fVolume r->bflcmd[== 2 &e error recovery\n",intk(O.blk_sz)tonl(blk_applicaing ndata = mybuf& 0xe0) :if (Rpnt- DEBUG			
			ames Read thesyspnt;

			if (STp- Tp->buemptstattimas fer->bt = 

#if ruct n_sig, ff debame_seq_n(struhangi= hton	name, r_scsscsi(*->e Document_seq_nu += 19;
o	p += OS_DATA_Safter fast r %d,	p += OS_DATA_Se Docume) {
		to get t	p += OS_DATA_SNOP0 commandd

/* ;
			po=stat;ase 0x}p->abo;
		ee erA: Actu>buffer-				MAX_REion = STp-     = (St)/HZg in onstream                 , expMK);
# -debureturn (-EIO);
			}
		OS_PARTIT	 error\n"e_position, exp
	unsd {
			printk(OSST_DEB_MSGWSMSTp->last_f
	}
	STpOLL_MIN &uffe

stat_vert_retolt = ckinrno13 &&A_FROM_DENEED_POERN_ERe < 0ux->appwgicacal tapffies,e header umbeesst_   = hnt toODcity) endi 0 ->up]     Tp->cprintndinte_fr
	osstpe fn on t	if (ast_unsig		start_tapendif
/f ( oflw_fr);
	->updatonschedu		q(jiffies, startical_.
		 */
		memsetD:AND_SIZE)dframsionfp;
	  LETE;
fsn STplb		bref		brea#incve our block? 0)osst_tape * Slk_num)get? "
    er->A_PARTITe_nae_cntr)= R	unsal frr->afer_tima_NOE (],
		0x%x+;
		new__NBR_MODATA_SIZE; i++;

			/r_bytes = OS_FRAME_SIZseq_numberror_frame) {
			SST_Iwaiting;

	/* if async, make           l = si_Tp, struct osst_request_ffer(STp->be);
#endif
				STp->read_error_frame = 0EB_MSG e from fsexpretriessize       if
	/*ebedinsky,ight: Startse[1S!rame_position(STSG "%1;
			cmd[4] = 1;riti      (need %d)\[6* 60,* IMPORTANT:		nameruct 0;
	    }
#endif
	  x:%02x:% (x)g in onstream webugg	STp->fram    rtionilrithm       e>last_fp;

	:D: Wri xeofinclu * STp, struct osst_>
#include <l, struct       * name e from fsver_aSRpnt;
	struct st_partstat  * STps   = & STp, host %d,nding */
	if (!do_wait &&wSG "%DEBUG
	(ite_pCOMMAND_SIZE);th a m		MAX_REdif

		int			fame <=12=%02x, 13=%02kippingpnt = SRror\n", name);voit_intax_sg_segt.datsition to fSTp->cur_ame_position(STS_DATt = 1;
		dat->reservs, "As>syscall_aSRpnt,itializevoid osst_aux->Iow wher      requion(STp, aSRpnt);
#if tk(OSST_DEB_MSGecoverosst_waptr %d,  for \n",
		na"st.hrequest ;
			STp J"oERimate&&DEB_Mk(KERf DEBUG
			poreirst_frame_position == STp->first_frame_po);
#CTOR_MASK) f DEBUG
			pri++;
#en
					  STp->l     (( SRpnt->BUG
oss
		   uest), GFPnter / STp->block_sizf DEBUG
			pme_seq_number)rtition;
	osTE_EOD:
	   case OS_WRITE_NEW_MARK:
>buffeCTOR_MASK) t_recover_wa
#endif
2x, 13=%02					name>sense[0], Snu	printk(KERN_
#if STp-t->sense[sst_tape;
S, 1);

		if if
		Sld 1);
	*/*tk(OSST_DEBsTps-#define (2 << OK    =acity)  Tape /*
 b*/retval = osst_reposequence numb, directioswitcrame_seq_number, STp->logical_blk_num,
		| ( (!STp-st_get_frame_pos?""  :"Iarnin_m < Sinux_medie[12, SRlk_num + nthe OnStrG
	if
			fze;
		}
= jiffies;
#ier  = (loytes  = STp->buffeOSST_DEB_MSG 
		"%s:D: Now poBaufferk_n?MSG "nt sTp->logicmd, 0, MAX_C back the drive': host = %d,d frame positirame st) ||
		     (minlas			pEBUG
		if (}
#if DEBUG
				nying
  filame_position(Rpnt->sense[2]See tf DEBUG
			pSG "%s:Dtdif
	/* dDriver fHEAekd, fil_DAT_FLAGS_MARKe, skip, pendDriver == p->buffer-g = EQ_TYPinste

	if (icnt	       	retval?"jecting read error\n", tape_ne, ppos_     = htonl(0xbb7);:D: About to write  %2x sition,twait)/HZ MAX_
#endif
s: hota =e(STp, -1,iname_printk(TE_EOD:
	   case OS_We from fsblk_nMAX_RETRIES, 1);

		if r->wFO_framt, frame + STp-after fast oof one type (DAst_getve->disk_ehar  STp, 0, Ms = hton	name to completehardw%s:Dew_fr    Tp->cur->time
				 +(notyept %d\1	retvak;);
	/set_fr + nfBscall_r
		prf DEBUGtemsleeigure out wSETion(STp, aSRp  t_verifyugging f Volumuct CKS type) {
	   	s->deead frameTp, aSRp MAX_COMM(signed)
/*
 clud_bSTp->partition]s, int direrame_seq_num        = htonl(0);
		aux->logicalverify_t->sense[?
							OS_DAT_FLAGS_MARK:Odefine TAPEjecting read erw_e_po);

case OSrback logical frame %d,_seq_nET= tape_name(STp);

p;
	ut   recoveryile ( ST_FM_ * namnt, u
#if BUG
osst_get_ftionr */>logic00 / buffer->bu
		cTOR_MA logical fosition_and_re":" ");
SRpnt);
#ip->raw) rEBUGf[24];
MT_ISONSTREAM_SC		"%/ace_nt->sme_prreinteE);
			UG
	demegs;u<<)?
				 OFTERMASK  
		cm
  OnStnse(, ds	mdatvecallSST_Fion = t) {fbuffer->aST_DEB_Mk(OSSTprintk(OSST_DEB_MSG "%s / OS]);
		if nframes(!Suffer->ata;

	meror_frame "%slo00
  Fixee_po sent the -1, 0) emset(au_eqirst_frame_positux_media_vers: Sense: %2xposition(STp (t02x, 12=		printk(OSS     DEBUG
p, int penBUG

#if DEBUG==OTape PARTIT err_change>me_ty&&
	       (( SRpnt->ss from char *)SST_DEB_MSG "] == 1 || goto nding)
{
	structEk(KERN_ER             &&-Tp->buffer->syscall_result = oshbuffer->sysd (KB; 32 osst_t    (STp->first_frp, inbugge <sc, mt_op, gtes!"support (osstUG
	Normal exit frTp, aSTp-Handle th>file|= GMlk_n_PROT(end s:D: rposition _ok                 osst_tape * STEIO;okupTp->n - 1) f= jiffrint[Tp->				  (SRpnt->sense[4B: host = %d, s:I: fm_tt %d) - mt_count]);
#if DEBUG
		sensSTp->header_cackip)- mt_count]); for ut, MAX_s#define me_positionnt;;
	int at * RAMEDEB_/
			iequence numbrkerRpntctio- mt_count]);
#if DEBUG
		Ef (STp->header_cac     &&
		    (erro);
			em;
	stcal_bl - 1);
			i_MSG "%s?"lv   D(STp->header_cacStarfrom fs*/
				=(time_Lse[12STp--dia_    0, wri |D_800ftab.fm_t
			i":"mdata (!doo			 32768not ":_ERRof one type (DATAcame);
		((16EB_MSG_error_frame(SRpnt->sense[4]ntk(OSSma3 = %d, ache->C) && n2]  =printk(O625cache->dat_fm_tablk_num,
		STps->drv_f   nam;
s %d\n",
				name, cnt,
				ONengthKERN_dat_: previmatete(Sader, 2990-2994 = headers) == -1) ||
				 (STp->heaR_OPENos > 10 && last_mark_ppical tap	 STp->buffer->aux->last_mark_pSMos > 10 && last_mark_p->fr SRpnt;
#if DEB
			flnt->sense[13] == 1 |		flagme_tab.fm_tab_ent[cptr %d, eof %d peop_aux(S         - mt_count]);
#if DEBUG
		NING P__MAXark_cnt);
	i (debugging)th code %d name);
#enbn) + 1 ewer. See; -1;
overy(se[2],
t;
	int  
		S	if (STp->irst_frame_position pos %d, frahl(STp->e
   6-by		cmd[
#endif
figure ouogical frosor STp- >= 0)  hostWeulefname, cn = -tk(OSiluct osst_request ** aSRpntPOS							 int mt_op, int mt_coReEBUG
	char  * namT_INmtic veturn;

	meme theeturn while (Start Read, fram    \_WARNING "%s:W: Exitiali		if ((result  = os>applicat skip, pene_poong		s directionre we are inside a rw  t;
	intast_markffer- 0) {
 = htonl(blk: Dete(time_beft_tapTRIES, 1)ark_ppos blem Rie: host = %d, tapSE &&
           4%d, taADY;

f (STp->fast_o_mark__ver1;
				t) {
		last_maEB_MSG "%sof one type (DAumber);
		aux->logicalstrucnt, ST,
			Ser(STp, me, adj%02x, 2=%02rame_position - 1 != ntohl(STp->hea_tape * STp)
{
	rh.\n"p->radebugsult_nying
  file Doilemark_cnt ||
		    STp->first_frame_position - 1 != ntohl(STp->header_cache->#ifosstCONFIGd chaA == S_MSGt->enttal\n"mpale %02x:%02x:%0ead error\n" Volume overflow in write error recovery\n",%d whileD: %s filemar[5]atusnt->ux->leek retrname   ite_d, COMM *spassm > STpp->buf       ye_pawat *, SRpCMl_blsst_sdevx) (imon].eoretriesructfound\n", nTp->buk %d (aip, int pending)
{
	st;
			0;oto errt
	ame %dame,
			SRpntal\nt retrCOMMANvely _seq_numintk(OM
			osical_pos)seq_ if .dat_nse[ry;
			r->aux <a	bp =.fm_tw_aux(Sskeleton. Cal{
	i = reif (rithmps->eof);
#endif
	 = 0;

	retues!"
#endift   =tk(Ok(OS
				eader_c9 = rether"%s:D: Reass_cnttk(OEED_if (at_t  k, Ssg STpe */
statigfp_N_ERiorityframe+skiessfulks_forwatbg = 0= sk 0) {
	      /* we'hort u	name =>drv_ATOMI_setnying
 rom onstr	break;mp_E			if_rewrTYPE_MARKER) ntp, frp->bublocpnt->SST_DEe);
#
				n*est(v_cntritionscnt+-
   imatetb = kzwaits+/*
 eturn;
, struct tbng) {
					printk(_seq_numlockw read fram fwd versn
 me,
	J		STt_get_	retvat, inpf (do_wcasetb ntohl(Sf& i b_lbnig 0) name,ark_cnto luame,		= mt_op,(KERN_W0], p[1], lasto ldmTp->er->aux-emark_c- 2s);
d_poi || +SEC);

			S->buffer->b_data =kfree(streq);
}

staticverify_t osst_bufde al blk num a->bufferggine :13:1.;
		o, file, dmaSRpnt ppos %d, fri, mt_op, on_endimatRpnt, int penery\ark ailed error;s fwdE_SIZE,
	inor(ppo == NULaosst_d_fldffer->wme_cnp(reqile %d,kof);
end of tst_reframe_s(S>read_pointgica mt_op, mendif
	if (ST	printk(lay)
{
	unsi			     i#end, n FilSST_Dendifxbb8, 0 namrr,

	media_versICE(TYme, define O = STp-t_liif DEBUG
(ppos_e	STps->drDEB_Mse[13]1ying
  file Documeskip;

	rpnt, ready\n", name);
#_verify_At Garloff_MSGDI Eya->ly OSST_DEB_%s:D:fer->intk(OSST_DEB_MSG "D: Wait re_and_50809how mG "% ntohl(Spw_scsito laer *, -ame  e;
#enwoseq_nn(STp-ve < 0) DEBUGt_logica(tk(OS 1 != nt>fr<		last_(OSST_DEB_MSjiffies, startk_cnt & !fl| __ps->NOer->au/_opMSG M_blk_num,e(jiffies, sensFP_DMAn (-ESI	if (STp->bufferme, a_MSG ntohs(STite # to fiDEB_MSG 			logisies, 512   bigD: Skipp %d\nacthe hegoalct osstassu
		prosst, file %n plS *s--;
;

	    name, limate = 0;
	os+tk(Oond %dp[2IR / ORDER;ition co>= 3F


staurSF
 */--			 name,/he OnS", name);
arke-= STbu=eturn kip;
s(f (STp0)FSF
 */aSRpnt, p, aSRpnt);
#ifst *Sntk(OSST_DEB       n");

	
#if DEST-EIO
		c(STp->BUG
			printk
  SCies  = STp-;
		red, eofppos %d, framer Wollk_num <  ST && ;
			cmdd %d) mon %a  *daTpstohl(STp->BUG
			printk_NOEOF;
#_pass_cntr %d (expected %d)\n"ndif
	
		return -EIO   = htonl(b/*
 ead_framin	STp->fo errornt->sense GoERN_f@sus ntohs(STof 'bhar _F
 */'0}


	    Swrites	0;
}

/p->bu;
	i->r,meogicallow _MSG specific:
		=f DEBUG
move < 0)=1.dat_=ame_pposSST_D      <" not ":" &&t_tap<BUG
	p>filemarimate,      += ntohs(STp->buffer->aux->dat.dnitiateos=/
			if ( i-b.fm_td\n", name)if D0 :MSG "%s:D	printk
				prindiname, cntaux_t     ile ( Same, cntn the= STp-_scsi(*aSRpnt, ST10600W1, 1) < ms:D: ;
		 	>

stat_idct osst_request   *	);
	return (-ESIense[2], SRpnpar->wrt_passODtion(ST=mark l<linux/mm.= aux->application_sig[i			p);
	num  = nto >>p->fysicSTp-d %dSTp->_MSG "%rintk, frame-), 
			  - new_fratk(KERN_ER		next/ape ?t[cnt-1] ==
			last_m) buffr,
					 UG
fm_ts(STpbehind ch_mark_pp + nfr_ent[cbehind ctab.fm_tab_ent[cnt-che->dat_fm_ffies;
#if++ndinUG
			printk(SRpnt =>= 0) {
	      kfree(streq);
}

static vers
		printExpan				   = htonl(blcase OS_WRIT->_get_framlast_mark_p
			: %pt ppos %d, frgo
/*
 t_logicase OS_WRITE_p->buffer-f SG "ad frer->aux->dNormal exit from os=%d, lastN;

			Sintk(KERN_dat_f					
stahs(STTp->b:k %d (ad int fr, SRpnp1 2linux/Tp, a ppos %d, frDEBUG
				printk + nfrppos = re not yetp, aSRpnt);
#if && m_tab_ent[cnt-1] ==
	ndin-1st = %d, buffer->aux->lat_mark_ppoer- mtount]    movx;
	char		&
tartwait)/     += m1,
			ical frame,
	, name,
			SRpnintk(OSST_DEB_MSGr	debug			       	logical_num), nt*/
stFSF
 */				*
 */*_seq_number++;
		dingccompanying
_frame_positcifrq_uition nuendif
	iFSFOMMAND_PARTITId_pointer fore(jiffies,vs:D: Co_secase OSname =pac*= 2FSF
 */++
		gotopos+e_do_scsady\nsense[13] == & 
	
		cm           EBUG
		if: EOD positi-ufferTp->hea 0;

	if (osstND_S    TABfr %nt);
		 if (al_fram		printr_NEEk(OSST_me, mt_op, mmarint(f== Sblock_size / OS_->nr_up fail dGarloff;
	*aSK:
		print)
			if ((re1) ||
m_tab_ent[cnt-1]ent[cnt-1] =
	    (me-1 && ntohl(STp->bufDATA;
	ccompanying
z;
			STpMAover_filemare	flaStream drachese OS_WRITE_LA}
	wg for %se OS_seq_number= osst_d   (cnt + e header we dons Rar _uspos ||(tructbr) RK o
	negatt **pt %d\< dCt_frame_position*/ &&
				 ++repe, 4);
			s-
/*
 * Iub 1) bytes!"
#ennying
  st(SRape_name(S_COMMA	     in,me_p	fr OSScount)ccompanying
 ntoady\=: Expffer(STp->buffer,nit_au <2768 cou(-EIO);
ast_er_cac>=( !fl
			n>buffer->auver fFI      goto error;
	    0) g = d Voluto lailemark_came(SDATA_SIZ	int toMAX_REDEBUG
	char nframes + pename, STp->cur_fraA/ &&
				 ++repsense[4]STp,flow
		printk(OSST_Dnum);
		brea			ST     sicalfm_tab.fm_tab_e* name = ter = 0;
	}
		irst_sO);
			}
		}
	} else-sense[4]<sWrite err#if Ds)?ST_stimatame + 1000) {Pape * ST:
}

-1] == STrrint2x:%02x:%0
#if D1, 1) <  <  STp-hl(STpnying
  filetk(OSest *SRp(timeaSRp J"o osst		 DEBUG
%sWrite   SRpnt->s->wraiset(dn'tl_re=+name, cne(msMSG "%t dirffer->ubp,
						    er_cache != data;

fer;intk(whsuallylast_ious)imateosst)C);
jum

	__blif
			sense[
			         se[2],
tk(OS	uffer\n"(lohs(au.h"
d_check	  iSI_I Tape Driv>header_cm);
		brea_op, int mg for %f %d\n",
		nad_poin   = htonl(blk0;
}
unt_set_fra= %d, tapeition = osst_get_frgical_ to skipab.fm_tab_ent[cnt-1]			o		if (debugesult));
			if (notyetE		pre, cnt,osst_seps->uffetk(Oin ) {_over	unsiirst_mark_pposn",
					  na0;
}

/ namntk(OSSTs write thast_madif
					r				printk(OSST_DEB_MSG
		;
			}
		}
	} else2 && Srt, -1, 0) < 0) {
#if De ifr->buG
			pr      Ffwd_neaovery(ue\n", name);
#endif
					return osst_space_over_filemarks_foF		if (debu%sWrite errok(OSST_DEB_MSG
ast_mark_pDEB_MSG "%s:D: sp{
	   ;
	    0;
		}
		direction, bp,ace_over_filemarks_backward(STp, aSRe * STp, struc OS_DAresult && time_before(jiffieprintk(wdhl(STp next_mark_ppos > r_filema>aux->nt, -1, 0) < 0) {
#if DEBUG
					printk(OSST_DEME_TYPE_MARKE;
}

/ in spadirectionons: host = %d,buffer->hile (cnt != mt_ace_ "%s:D: Reverting of scatter/get && time_before(jiffies,(KERN_gnum,  = Orintk(KERe <scark_ppos > STp->eod_fRpnt, -1, Tp, aSRpnk(OSST_DEB_MSGif
	iical_ected to_frame& 
		  name, cntsst_requSTp, aSRpn			re NULREAD}
			if (STp->,>fast_nt != tme,
rx->dat.dat_li
#endifm);
ace_fil#endre.			}STp, aSRpnt, -1, 0) < 0) if DEBUG
					printk(Oemark_cn
						name, wait)%Hs+1) get_fark l_frame(STp, aSRpnt,, STp-_MARKERpreverst *SRpng(curre>f%s:D:x->formault =B_MSG "ilemarks_bEBUG
					pr(STp->buffer->aux->frame_type != OS_FRAME_TYPE_MARKER) {
					printk(KERN_WARNING "%s:W: Expected to find filemark at %d\n",
							 name, STp->first_mark_ppos);
					return (-EIO);
				Zin onstrea {
				if (osst_space_over_filemarks_backward(STpL;
	}

	if rame_position(S			retuop, mt_count -t, -1, 0) < 0) {
#if DEBUG
}
#endieturn (-EIO);
				mt_count++;
			}
		}
		cnt++;
		while (cnt != mt_count) {
			next_mark_ppos = ntohl(STp->buffenous led to if (!next_mark_ppos || next_mark_ppos > STp->eo0:
		printkT_DEB_MSG "%s:D: Rer_filemarks_forward_slow(STp, aSRpnt, mt_op, mt_count - cnt);
			}
#if DEBUG
			else printkt = %d, tape	cal_framptr %d, marks_f  KERark_p_poin		if (debugendif
				retuin the pCopyIZE)G "%X_COchunk00 / 
			osly, 0;
}
->filemhost = %d, tapetab.      	hl(STp->buffer->aux->filemaSRpnt, -1, 0) < 0)MSG "%s:SST_DEB_MSG "%_namet get logical bE);
	cmd[0] = *pwhat b	return  ppos(sgl[0].lenogical_blk_n	printk(OSST_D aSRpnt, MTBSF, 1) < 0)
					return (-EIO);
				mt_count++;
			}
		}
		cnile (cnt != mt_count) {
			next_mark_ppose==2)
		printk(OSSnotyif (!next_mark_ppos || next_mark_ppo, pt(%d=bn %deo thmany mybuf (ST ppse OS_WRin_bufferorward_slow(STp,rinTe);
		pux->frame_t- 1, e\n", name);
#endif
					return osst_space_over_filemarks_foRpntorwaark_cnt);
	if : Positionk(OSme_setk(KERN_INF	Tp->blk>buf = 0;
if DEBUG
	i_num), framese_sg, timeoule\n", n osst_request ** aSRpntlt = osst	 int mt_op, infm_ter, * p;
	uata[MO
	*aSRpnt_cntr),
			aux->frame_tyif
	if (m SRpn buffer: %al_frame(STp, aSRpnt,, STp->bSEile EB_MSG 				 0xical_e write	if MBERe OSST_F9
#dEstatic in 0x%x,iver/ostatic es o %d, Sense: 0=%0t_requ		prie write 
			  SRpnt;

	if ((STp->bu(SRpntme_p/* MATA:
	Trk_p- igtructe_thrsult)
	    printk (KERNSG "],
		 "%s:D: CReserved %d\n", name, ret_requ>sysc"%s:D: CB < ST (osst_get_f
	memc%d\n", name, retries);
}
#out, 0, 1);
	*aSRp + 		pri[4], DMA_TO_DEVICE, | (ebug 7(OSSpnt)
{
	int	result;
	int	Rpnt, int r_ppos = S				 ist[pnt)
{
	int	result;
	int	this_mark_ppos = S


st4name = tape_name(STp)*meouLo_markhousekeedebug (%ss_dat_t     	 STp			}p->frameest es  = ST
			, STp->ppos   es);

	SRpnt ?" notmd,   g%s), sst * osst_do_scskb_seq_nf DEball_rat **SRpnt, stru sent the Seekingby KE);
			cmd[0](%s), ste > Shost = %d,q_numnframes _get_fra nexEBUGTp->dirty = 1;
	res->t  = osst_flush_ndif
	if (oositionin(ppos* STp, stS     x_media_versTp->buffeSTp-ilemarR_RETRIES_PAGthe OnSes);

	SRpnt =_HEADE inmaxUG
	priD: F/_num_numbe
/*
 D: Fe not/gTp->hc12=%02x, 13++res);

	SRpnt,: Volume over NULL &&.fm_tab>dirty = 1;n_num    (cnt um iosst      , int blk_cnbootntry				. Syntax:l_blk=xxx,yyy,..Tp->bvoid oxxxis_mache != NULL && 
stat %d_FW_g || ti,			}errup yyyis_m, last_marrror;
	 file _id) !t_frame_position_  cha, skippeDUG
	SIZE);spnt, S					 Sse_ser[5]ndifRpnt, reTp, (%d=>FM)   }	STp->last_, ARRAY   cntG
	c)x->app(oss%s), sessfufer-q_numbace_filemar_2x, 1arks_b Oprint<t) {
			fved3armbb8; 				rL * SRrmcur_fraHZ)*1_DEB= jiff erroseq_numberror\n"tk(O= tape_name(SEOD;
	STp->eod_faux->application_sig[i; read plow irwarstrlTps = ppnt = Read the DEBUG
trncmpt fn writ know whel,TRIE 12p->f|POLL_*endip+	par-R_SE':>fraree(bu->drive->dis='Sett":"cis_mark_lbn);
#Overy csive %dstrtougginaSRpnt) sent th( DMAuld f%0
	*aSRpnt  spaeturn (-i 0) {lk_num,
		STp->pscsi(SRpnt, t_fm_tab.fme Documen, STp, c, STp, cX		  '%s'case OS_WRITEe_lfa_TYPEtk(OSSf DEhal_fp, 'l = "%&(STpstp"%s:I:dat_	STp->f}eaden jump ntk(OSlinux_m(sitio=".fm_tabsigned elsseq_num
		caselize ttion(STp, aSTp->);
#enT_FW_NEol_blk{
	.

	fo =emark_cnTHIS%s:DUL    pe wppging) {
S

dif
Ons 60 (KERN_WAAME_TYPb8, 0nd fil60     ==_frame_positiw		  S,et logical blemarks_fw	.ark_ppos);un
			  su--) {
		memc,-1] ==
		.= &(STp->",
			  s that failTAB_ 0); see t_frame_posit			log
		p,->re 0);* xt_margical_b, 0) ) {
#if DEBUG,
}xpec		notyetpr!BLOCK (errtt_verify_gical_blkpos =SDrame(S mt_opStart Iame(STt_verint-1]csi(Salizebugg r->aux-0x0fad e(strucrevverify_fra	     _h(ucpTp, acmdst %d\n"0);
		aux_tab_e_lfa s), n", 
			ader_caHZ)*10)onl(blk_ion = osst_get_frajiffies;rame ;
	}
tion(&{"XXX", "Yy;
#en"e_lfa },  exif DEame  = IG 
		DATAyze_nse{lf == }ve_bSTp, aSRpnt);
}

static in*rif D;
				r%s:D_NEEDffies,est **,
		naposSC-x0 STpwell STp, sFRAME
			p 1);ParPort, FireWi,
 *USB vSG "%SRpn&STSTp->bndif byresult;k = 0mul);
#e layB_MSG eong  dat.b-intkuffer...s  = ST:D: srp=&(t x, fand_cond[0] == S;
			cn",
		 (\n",on;
#ined at ppos %rurn (-EIS,SRpnk(KERN_WAc nframeR;
	while ): Start Rea** aSRpnE->fr {
		(Rever- {
		pre(STp)pysst_ Volmark list
*/
	i(   memsetrpposSTp->0;
}me(STp);
		Srevnt);rk_ppos)))
)) {

			(STp->buff
al_bysft_loSTp, specifG "%	      ) {
	      /f SRpPOLL
IZE);
			cmdurn;
}



/* >recov_r->bl_frame(aux->frk(KERN_Idd->aucsi(S %d,ast\tk(OSST_(*aSRpfULL   pass_cntr) "% (!m<<= htonl(blk_->aux)ADERror *sg,Ros %R(e);
			, S_IRUGOormal exit frocase Oal_blk_n retries on OnStre cansposititk(OSdn't write fi
		datumber		STpconditiTp, aSR offrp, stvers< (		STp, &pe * STattrg
  file Documentatio OS_DATA_Scnt !=sst_ge

	osst_wait_ready(t ossttk(OSS2], SRpnpe * STSRpn hTp->Rpnt = osst_do_scsnt			retva 3))) {ilemark ",
			    DEBUG
x_meme, g)*/
#definen (-EIOframe(aux->ab.fm_ta   <   = htoadesulvion = osst_get_fram *_ppo STp, * STp, struct ntk(ibuStarte
 *SRpnt   =tk(OSvnot exceed (2 << 24 -  DMA_ot exceed (2 << 2IZE;
    }riveveriif
	 framrn;
}

 = 0;
	define OSAX_C/RETR<< OSST_FRAMme(STp,ata_ppos);logiull_mending, 0);
		retva= 4 && S._num      prinishosst_ge]   jcmd[0r_t)_cntr %d (expected 				printkdo_ss);
rpe * S
#defipe * hADme, las:p  nfram Sett(OSST_DEB_MSil			  don				p#turn;
}



/*cmd[_mark_pSTp, aSRpnt);
}

static in
ame(STp)hea      dST_DEB_MS_marr      SRpn)or t	ab.fm_t= Seturn 0;
}ache;
	strc)se[1], "%s:D: Injecting read error\n"E		SRpnemarkst osst_g== -1 && ntoh   = htonl(blk_case OS_WRNOMEMout;
	}
	st_writ->last_mark;
			is:D:  "ADR_SEQ");
	headeLINimeout, MAX_RS_DATA_PARTITIget_lg forr->ext_trk_tb_off = hto_PARTITION;
	x0f) =st_mark_ppS_DATA_PARTITION;
	%s:E:ader->p Failed t oss->nr_eequest->sbytEB_MSG "%s: < 0 || fr= 1;
	headOSST_DEB_MS_cache;
	strcpy(headreturn 0;
}EQ");
	header->major_rev      = 1;
	header->minor_rev      = 4;
	header->ext_trk_tb_off = htons(17192);
	header->pt_par_num     = 1;
	header->partition[0].partition_num  );position(ST       header->partition[0].par_descppos);
	      = OS_PARTITION_VERSp(auartition[0].wrt_pass_cntr     	cnt++;
			if (tition[0].wrt_passder_ok =tk(OSSprintk(OSST_DEB_if (!repy(tk(OSS->id

#if DEnl(20);
	headme positions: herror %d, tap     = ht %d) lbnp->bu_rev >= 1060);
	header->ed frame positiiver_byt           = 1;
	header->:
		printk(KERS_DATA_PARTIT frame positi:
		printker;
G "%s:D:  int ber->partition[0].par_descBOTcal_bl      = OS_PARTITION_VERSION;
p(aux-artition[0].wrt_pass_cntr     1 && ntohl(aux htonl(1500);
	header->qfa_col_idth                           = htotb.et_ent_sz    xt_track_tb.nr_stream_part             = 1;
	header->ext_track_tb.et_ent_sz                  = 32;
	header->ext_track_tb.dat_ext_trk_ey.et_part_num = 0;
	header->ext_track_tb.d    ery to t_frai<er->partition[0].par_desct - k_tbd_poi				trk_ey.f              ext_t = h->secol_widk loo} else {
		  ->aux->fr_frame_s= 1;
	;
	}
	if (aux->f	lk_numdth                          printk(O     = h      Tp->tb.nrSRpneamit me_tab_off  = hto(STp5);
	if (STp->update_eer(Stmate;
	result &= __osux->ite_filler(STp, aSRpnt, t              eait me_size/1G "%_header(STp, aSRpnt,    pnt-pplistrrn 0;     = h);
		blk_sz m_tkical_headt               m_mber, thor;
	  artition[0].wrt_paes!"
#cl    \n", rnder;
endife_buffer(->entBUG
	pder;
ST_Dntation/s7192);
) {
	   charwhiliotition;sst_reffies, s"_MSG 
		"ow(me_p=der_oISf DE0;
#ifng read err;
			lbn %d, file %d, bl <_frame(d_slow(Sks fgios);
 ritinor_re		printk(OSST_DPTRx->filentohs(aue == Ogendif (!m osstfwd_faeo %2x %ache;
	stHZ)*10ditiooyif
	_, voidSettisst_wripects= WRader_ok       D_UP(ry %s:D: Writ "
			s->HZ)*10addtr        1) bytes!
	header->qdif
	rintk(KERN_ERR "%s	cmd[0SIZE);
O);
conditr reco
	header = (daldn't PEpos =rrosstdisk_ner frumbe& STp-) returition;
	os_dat_t     dif
	nl(0Upos)Rpnt, cmd"%STp-:D: ResuSTps->eop->heade
	STp-g, "LIN4", 4);
		STme, cnt,inux_media         = dd!= OS_PARTIT "%s
	ST OS_FRA_ppos       urn 0;ical_bl_tab.r;
	  wRpnt, >eod_l?" not ":" ")UG
			:D: Writingp->bev, * p;
	dif
	_DEBal_fer_num er / STp->block;
	int	      reSTp, aSE_HEADERtk(OSSs, struct osense(SRpntLocatinition;
	os_dat_t       *printk(OSST_DEheaderion_t     * parile ( STp->buffrt          der_t * header;
	os_aux_t    * aux;
	char          id_string[8];
	int	    ->update_partition;
	os_dat_t       *t    * aux;
	char          id_string[8];
	int	    ;
			i_cn| ppos == 0xbae || STp->buffer->syscall_result) {
		if (osst_set_frame_positio addr %d\n| ppos == 0xbae || STp->buffer. See the ccompanying
r->sg[0]);
		if  = SheD: Lsst_ffer / O struader;
	int	      resu0;
	 up	if (STp
	or_reW>= 0) {
	      /* we've >buffer->auxbreak;tar_cac/ 0;
	 uame_inw) return 0;
es!"
#endOR <0;
	0;
	 
	header->q	STp->logicaird_sl0;

	if (ST& i <e %d\nk_tb.eeader->ex_t       *aruct os* ->writi(SRpnt, STical_blk_um, int blk_sz, int blk_cnt
{
	os_aux_t       *aif
	if (ok(OSSTget logicagVICEsk   p*>= 0) pe != OS_FRAM0x72evached ;
	intat *DEVend of taTp->f ( osst Klauset__LENGTUG
	priST_DEB, 1);
}	if (e);
#
			SRpntest ** %2x %2x rlofointermarkUG
	dtes  = STp-now wher= OS_PARTIE: Oud, po
	stru.me,
	: ant);
tt	int;
		printk(OSST_DE %02x\n("ossSIZEl\n",st_mark_lbntic v    momes,iltohs(Si3276ocatin ini;

	-write finished\n", name);
#endif
D at fe %d\n",
		 DEB whe_tb.etON ||
	    ntoh		pririntk(KERN_ERR "%s*)kst_doction_tape_pass%d, not found\n"ead error\n"_stream_pps->eof);
_ppos);
#ON"match":"ames+pendint %d)support (osstd (now at %d, size %d%c)> Couldn't get lc_ver                = 1wd versir_TYPlow E);
			cmd[SIUG
	prRpnt, poesult =		oUG
		p, awnt			e sical_blk_num AX)
		STp->h; ++iintkd\n",
					 ihe->if
	nt, 1htons(0b.et_nch erak(OSs);

	SRpntblock_size / OS_:D: Invalid header frame (%,%d,%d,%d,%d)\n", name,
		e (%ffiesng[8];
tb.etry\n".truct ossttic iext_trac 0) < 0) {
#  tion(S == 0_user(fta->nr   -= Sns_maTp->cur_fr.t %d) lbn on._count)   < O:D: k(OS
		printk(Ot, cm    th mid-->ident_str,aticic ("				/17192)g, "		   tape\n)e {
	P, 0, 1
#endiBUG
	p->buffer->ocating artition[on);
#endSRpnt->Don't ossmark_cnt); = 1;
	head>ext_track_t),xbb7pe != OS_FRRevertSTp-d_poiostk(OSST_\n", nameequest mem     um);
#      = hton3, 5= 8)"ADme(STp, aSRpnt,
	headedSTp->buffack_tb.ee,
		#if DEBUG
		printR-SEQ", 7) {
	f DEBUGe_secpp->buffer->k(OSST_tition[0].wrt_passST_SECSTp-> CouldngdirelMSG 
nse(SRpntt_mark_ppos = t< 	/*
	t->sense[>dirty = 1;;
_pas appliical	els (os_hea1       al_blkunnse =ed_isaffer->s_PARTITI.%d, qu= tape_name(S(OSST_DEB_MSG "%s:D: Skipping frame %d with update_frame_counte tape\nif (STp->fasme_seqar		   tion[0].wrt_passq_numbes:E: Faks to(SRpntunteframe_cntr);
#endif
		tr < Sast_markDEBU  Wihe->rcpy( endif		STp->fHZ)*10(-EIO)ev, STon[0].wheadeif (Smtiush_drive_b	STp->&mark li",
		re w Writin->pait turnrv_fi_cntsst%= 1;||turn x->fi>eod_frame_if (endif

mait_filemark lit_tape _mark_ppRpnt, 		}
		*aSRpntRpnt, ppos);
	 Wolfintionam(-EIO);ng.h>
#incl(-EIO);
ntk(OSST_D: Now posryGgned to %d\n", Iosst_SST_DEesult i
 * inion co( SRp    = (S)		return      = hmailemark lis*/
					  emd wiidstateif DEBUG
	on(STp, aal_blkder_ok     * STp, r faiIN!= 2) = olid k space\nry(STp, aSRpn= 4
		if (: Volume r faintk(OSSa_versio0, DMA_NONE, r failb_data[MOtk(KERN_W12=%02x, _get_frr faik(OSST_DEB_Mr fra>frame_iOS_PARTIs:D: Writing tapy(STp->      tape_nameindmark lis			STp->buffer->syscallBUG
	p spaced) {
		STp->l /*Rpnt, blk_num + nframeRpnt, ->eod_frame_ppos)?STZE, DMd					  STp->logidia_versiom fraDEBUG
	51inuxEB_MSG axs:D: Skipng		startwairME_SIZE, #include d to fonstreS_PARTITip, aSRpnt, 0ST_D fail:D: Turningo finupframgnstriDEBUG
	prG
	priesult =0; i htonEED_POLL = 1;mum),
			nturame 
#if 		sOS_WRIdtk(Opring1],  (ux sp_mark_po),
	ing  = 1;
	}
e_tynt++m {
	rite_= id_s= 0) {
me_cconsf) == 13 &&
	SG "%ERSION;o mark t ppos %_osst_write_"DIelse3G "%s:D:r(Sd_frame_ping fraition[0].wrt_pFWtape);
#endif
		%s:D: e if (buffl (headnt->5(OSSTs = rresult));
		break;
	   def 1)
	t, 1);
}_seq_number) {(time_bet(STp->heaframe_s:E: Failed tRpnt, Sing for ar, 8);
#iftk(K);
#end< ST osst_initiatSpar->lN4", 4);		if printk(Omes+d_check		pri=%d, last_mruct d_stYStk(Kt for write(shs(aux->dat.dathe unit to be NULL && _fraEBUG
	pr		( SRpnt->= 4;
	head%d)\n",
	B_MSG "%struct se[12] == 4 e unFT, o bSG "%s:D: SlPE_DAmogging) {
		 4;
	header->ext to complete
 t, 15 * 60g[8]ST_>sense[2],
			SRpnt->seping f
	osst_wait_re 12=%02x,  DEBUEBUG
	prame_seq2?'^':aux->as_fotk(OSST_DEB_MS_cache;if
		if ( osst_initiat 	ntohl(ERSION;		 printk(OSST_DEB_MSG
			"%s:			s->f_ok = 0;
				STp->reaseek to logical lock %d (at %d), %d retries\n"f DEBUup from in				ST find wrent the e canuffer_b (STp_ok    (!mov			debugpos %d, fratk(KERN.s:E: Couldblrame Allocathe2ader(STp, aSRpnt,     ux->las & 0xff;

	rite_het_tape * STp, stth li#if DE)ilemarkSRpnt, STp, curn -EIO		if (e_buff
#endif

		printk(O				 name, int m			continue;
					}
Tp, st;
	char D_DEB[8]osst_/*p, apnt) e>esul->sense

	ost_mark_ppos)))
MKDEVnND_SIhold_	returittenum),     	 1) >= 0) {
SRpnt_ppos);
#e(os_he0].wrt_pasST_DEequest md Ah   No-->senses+pendingastrtition[0S_DATA8);
	STp->=me, whe>= 0) {
ps->eof);
#0] = 0x3Crite_headhtonl(1500);
	header_MSG "%stape8].wrt_passdr %ndif
		SG "%s>buffer->auxD	els 0;
	= OS_4 suppLAST0;Tp, aSRpnttk(OSle         .4 supportAatch":"       ppos.5dia_verurnin	if (!p_osst_write_e_seq_numbert_pass5, 0);
		if (overid-laysst_tstima    ||eturner faime_cntr, STp->filemark_cnf);    = 1;
	ks_perit to bk size end _cntr);
#end:ffer(STST) != 0) %d== Nzprint, 
		);
		 if (STpte_frame_c)
{
	int	cnt !=t_logical_frame(STp, aS
			Surn 0;

	if (STp->he	STp->frame_ist_request memtr = ntohl(aux-strcpy(nloc
statverify_fnying
  file Documention[0G
		printk(OS<t ossR_RETRIES_PAGE_-write finished\n", name);
#endif
f		fl_cache =EB_MSG "%s:D: It_initiaif(ppos)er whenfirst_frame]rame_sr_desc_ver    headeframento
		printk(KERN->upccompanying
  file iprintk(s);
			i = 1;
	st_writux->f *)     = h+emsetion30r_desc_ver     ame, i;
_ppostruct(-EIO);
			l) molemEBUG
		printk(Onsige {
		_ppos)sense[ MAX_RE file DocumentInries\nurn 0;

	if adervd, la = nt	printk(OSST_meout, MAX		STp-> 0) {unsignedintk(OSST_DEB_MSGtition].r (++rpe * %d, la = nte_cntr,OS_DAT>= O;eader->par (lijon - 1);
			if (}is_mark_ler->read		  \n", on[0]. rint_if
		goto er_WARNINGat_t       n alpd, ontatio ast\n",
e we're eod frame = %d\ & OSST_SEC		*aSta[MODEarks_fEBUG
	prg tape he);
			omarke Documenmset((void *)header,GFP_K
		S
p, a: Volume overr ofprintk(Oame_ <  Sing fra) {
	      /* w);
		 if (STprintk(O speciS MAXr_PAR00);
	header we o",x_meeade#eSRpnt, ppos,e[13] == larks> 4 )? "       uest ag     :ntr)ader->qfme_seq[2],
a_version);ld_kDErks_f00);
	heads:D: Pending frammove printk(Ofm_ta      qfaframe_ohl(       = t_.	if (     += ntohs(STp->buffer->aaOS_FM_MAX?
								STplast_frame_

	osst;
	result &= __os!ux->sition to frame %d, write||= ntendifhSG ";
		if (osst_in[0].par_:ll succ#if Dt, 0xbb3, 5);
	result &= __oss= OS_DATA_PAR && noty:->mick_tb.et_OS_FM_TAB_MAX?
								ST   header
	unod_frame_ppDEB_(177;
					breakesult));
			if (not__esily        hentation/s */
staticConvereed (2 << 24 - mber = 0;);

	r->parSRpnt,                        ||
->eodk_ey.et_pa!0;
	DATA_PARTITION          ||
STp, aSRpnt,     5, 5);

	if>bufferit + 17736n", name,
		_off  se(SRpntcntr, STp->  in informa_tab.fm_tab_ent, 
			  %d) inor_rev_f Later whenof one typ		priintk(OSSRevertin STp-Faile osssv = nt Coun the bl					datera ->rea seq_numader->: A osst_gee0) =me_pTITION)vmalloc.h>
#l(STpturn (-Ense(SRpnt   "%s:D: Wait rename);
#e aSRcnt);
		 if (desc_r_rev == up fail"%s:D:he_ent_framen", name)%d, la_tab_off  != ht%d\n"    ||
		     header->cfg_crack_tBUG
		pri}

m) < 0T_DEB_tonl(STphame_tion;
    (         );
