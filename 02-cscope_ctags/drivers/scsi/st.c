/*
   SCSI Tape Driver for Linux version 1.1 and newer. See the accompanying
   file Documentation/scsi/st.txt for more information.

   History:
   Rewritten from Dwayne Forsyth's SCSI tape driver by Kai Makisara.
   Contribution and ideas from several people including (in alphabetical
   order) Klaus Ehrenfried, Eugene Exarevsky, Eric Lee Green, Wolfgang Denk,
   Steve Hirsch, Andreas Koppenh"ofer, Michael Leodolter, Eyal Lebedinsky,
   Michael Schaefer, J"org Weule, and Eric Youngdale.

   Copyright 1992 - 2008 Kai Makisara
   email Kai.Makisara@kolumbus.fi

   Some small formal changes - aeb, 950809

   Last modified: 18-JAN-1998 Richard Gooch <rgooch@atnf.csiro.au> Devfs support
 */

static const char *verstr = "20081215";

#include <linux/module.h>

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/mtio.h>
#include <linux/cdrom.h>
#include <linux/ioctl.h>
#include <linux/fcntl.h>
#include <linux/spinlock.h>
#include <linux/blkdev.h>
#include <linux/moduleparam.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/smp_lock.h>

#include <asm/uaccess.h>
#include <asm/dma.h>
#include <asm/system.h>

#include <scsi/scsi.h>
#include <scsi/scsi_dbg.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_driver.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_ioctl.h>
#include <scsi/sg.h>


/* The driver prints some debugging information on the console if DEBUG
   is defined and non-zero. */
#define DEBUG 0

#if DEBUG
/* The message level for the debug messages is currently set to KERN_NOTICE
   so that people can easily see the messages. Later when the debugging messages
   in the drivers are more widely classified, this may be changed to KERN_DEBUG. */
#define ST_DEB_MSG  KERN_NOTICE
#define DEB(a) a
#define DEBC(a) if (debugging) { a ; }
#else
#define DEB(a)
#define DEBC(a)
#endif

#define ST_KILOBYTE 1024

#include "st_options.h"
#include "st.h"

static int buffer_kbs;
static int max_sg_segs;
static int try_direct_io = TRY_DIRECT_IO;
static int try_rdio = 1;
static int try_wdio = 1;

static int st_dev_max;
static int st_nr_dev;

static struct class *st_sysfs_class;

MODULE_AUTHOR("Kai Makisara");
MODULE_DESCRIPTION("SCSI tape (st) driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS_CHARDEV_MAJOR(SCSI_TAPE_MAJOR);
MODULE_ALIAS_SCSI_DEVICE(TYPE_TAPE);

/* Set 'perm' (4th argument) to 0 to disable module_param's definition
 * of sysfs parameters (which module_param doesn't yet support).
 * Sysfs parameters defined explicitly later.
 */
module_param_named(buffer_kbs, buffer_kbs, int, 0);
MODULE_PARM_DESC(buffer_kbs, "Default driver buffer size for fixed block mode (KB; 32)");
module_param_named(max_sg_segs, max_sg_segs, int, 0);
MODULE_PARM_DESC(max_sg_segs, "Maximum number of scatter/gather segments to use (256)");
module_param_named(try_direct_io, try_direct_io, int, 0);
MODULE_PARM_DESC(try_direct_io, "Try direct I/O between user buffer and tape drive (1)");

/* Extra parameters for testing */
module_param_named(try_rdio, try_rdio, int, 0);
MODULE_PARM_DESC(try_rdio, "Try direct read i/o when possible");
module_param_named(try_wdio, try_wdio, int, 0);
MODULE_PARM_DESC(try_wdio, "Try direct write i/o when possible");

#ifndef MODULE
static int write_threshold_kbs;  /* retained for compatibility */
static struct st_dev_parm {
	char *name;
	int *val;
} parms[] __initdata = {
	{
		"buffer_kbs", &buffer_kbs
	},
	{       /* Retained for compatibility with 2.4 */
		"write_threshold_kbs", &write_threshold_kbs
	},
	{
		"max_sg_segs", NULL
	},
	{
		"try_direct_io", &try_direct_io
	}
};
#endif

/* Restrict the number of modes so that names for all are assigned */
#if ST_NBR_MODES > 16
#error "Maximum number of modes is 16"
#endif
/* Bit reversed order to get same names for same minors with all
   mode counts */
static const char *st_formats[] = {
	"",  "r", "k", "s", "l", "t", "o", "u",
	"m", "v", "p", "x", "a", "y", "q", "z"}; 

/* The default definitions have been moved to st_options.h */

#define ST_FIXED_BUFFER_SIZE (ST_FIXED_BUFFER_BLOCKS * ST_KILOBYTE)

/* The buffer size should fit into the 24 bits for length in the
   6-byte SCSI read and write commands. */
#if ST_FIXED_BUFFER_SIZE >= (2 << 24 - 1)
#error "Buffer size should not exceed (2 << 24 - 1) bytes!"
#endif

static int debugging = DEBUG;

#define MAX_RETRIES 0
#define MAX_WRITE_RETRIES 0
#define MAX_READY_RETRIES 0
#define NO_TAPE  NOT_READY

#define ST_TIMEOUT (900 * HZ)
#define ST_LONG_TIMEOUT (14000 * HZ)

/* Remove mode bits and auto-rewind bit (7) */
#define TAPE_NR(x) ( ((iminor(x) & ~255) >> (ST_NBR_MODE_BITS + 1)) | \
    (iminor(x) & ~(-1 << ST_MODE_SHIFT)) )
#define TAPE_MODE(x) ((iminor(x) & ST_MODE_MASK) >> ST_MODE_SHIFT)

/* Construct the minor number from the device (d), mode (m), and non-rewind (n) data */
#define TAPE_MINOR(d, m, n) (((d & ~(255 >> (ST_NBR_MODE_BITS + 1))) << (ST_NBR_MODE_BITS + 1)) | \
  (d & (255 >> (ST_NBR_MODE_BITS + 1))) | (m << ST_MODE_SHIFT) | ((n != 0) << 7) )

/* Internal ioctl to set both density (uppermost 8 bits) and blocksize (lower
   24 bits) */
#define SET_DENS_AND_BLK 0x10001

static DEFINE_RWLOCK(st_dev_arr_lock);

static int st_fixed_buffer_size = ST_FIXED_BUFFER_SIZE;
static int st_max_sg_segs = ST_MAX_SG;

static struct scsi_tape **scsi_tapes = NULL;

static int modes_defined;

static int enlarge_buffer(struct st_buffer *, int, int);
static void clear_buffer(struct st_buffer *);
static void normalize_buffer(struct st_buffer *);
static int append_to_buffer(const char __user *, struct st_buffer *, int);
static int from_buffer(struct st_buffer *, char __user *, int);
static void move_buffer_data(struct st_buffer *, int);

static int sgl_map_user_pages(struct st_buffer *, const unsigned int,
			      unsigned long, size_t, int);
static int sgl_unmap_user_pages(struct st_buffer *, const unsigned int, int);

static int st_probe(struct device *);
static int st_remove(struct device *);

static int do_create_sysfs_files(void);
static void do_remove_sysfs_files(void);
static int do_create_class_files(struct scsi_tape *, int, int);

static struct scsi_driver st_template = {
	.owner			= THIS_MODULE,
	.gendrv = {
		.name		= "st",
		.probe		= st_probe,
		.remove		= st_remove,
	},
};

static int st_compression(struct scsi_tape *, int);

static int find_partition(struct scsi_tape *);
static int switch_partition(struct scsi_tape *);

static int st_int_ioctl(struct scsi_tape *, unsigned int, unsigned long);

static void scsi_tape_release(struct kref *);

#define to_scsi_tape(obj) container_of(obj, struct scsi_tape, kref)

static DEFINE_MUTEX(st_ref_mutex);


#include "osst_detect.h"
#ifndef SIGS_FROM_OSST
#define SIGS_FROM_OSST \
	{"OnStream", "SC-", "", "osst"}, \
	{"OnStream", "DI-", "", "osst"}, \
	{"OnStream", "DP-", "", "osst"}, \
	{"OnStream", "USB", "", "osst"}, \
	{"OnStream", "FW-", "", "osst"}
#endif

static struct scsi_tape *scsi_tape_get(int dev)
{
	struct scsi_tape *STp = NULL;

	mutex_lock(&st_ref_mutex);
	write_lock(&st_dev_arr_lock);

	if (dev < st_dev_max && scsi_tapes != NULL)
		STp = scsi_tapes[dev];
	if (!STp) goto out;

	kref_get(&STp->kref);

	if (!STp->device)
		goto out_put;

	if (scsi_device_get(STp->device))
		goto out_put;

	goto out;

out_put:
	kref_put(&STp->kref, scsi_tape_release);
	STp = NULL;
out:
	write_unlock(&st_dev_arr_lock);
	mutex_unlock(&st_ref_mutex);
	return STp;
}

static void scsi_tape_put(struct scsi_tape *STp)
{
	struct scsi_device *sdev = STp->device;

	mutex_lock(&st_ref_mutex);
	kref_put(&STp->kref, scsi_tape_release);
	scsi_device_put(sdev);
	mutex_unlock(&st_ref_mutex);
}

struct st_reject_data {
	char *vendor;
	char *model;
	char *rev;
	char *driver_hint; /* Name of the correct driver, NULL if unknown */
};

static struct st_reject_data reject_list[] = {
	/* {"XXX", "Yy-", "", NULL},  example */
	SIGS_FROM_OSST,
	{NULL, }};

/* If the device signature is on the list of incompatible drives, the
   function returns a pointer to the name of the correct driver (if known) */
static char * st_incompatible(struct scsi_device* SDp)
{
	struct st_reject_data *rp;

	for (rp=&(reject_list[0]); rp->vendor != NULL; rp++)
		if (!strncmp(rp->vendor, SDp->vendor, strlen(rp->vendor)) &&
		    !strncmp(rp->model, SDp->model, strlen(rp->model)) &&
		    !strncmp(rp->rev, SDp->rev, strlen(rp->rev))) {
			if (rp->driver_hint)
				return rp->driver_hint;
			else
				return "unknown";
		}
	return NULL;
}


static inline char *tape_name(struct scsi_tape *tape)
{
	return tape->disk->disk_name;
}


static void st_analyze_sense(struct st_request *SRpnt, struct st_cmdstatus *s)
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
static int st_chk_result(struct scsi_tape *STp, struct st_request * SRpnt)
{
	int result = SRpnt->result;
	u8 scode;
	DEB(const char *stp;)
	char *name = tape_name(STp);
	struct st_cmdstatus *cmdstatp;

	if (!result)
		return 0;

	cmdstatp = &STp->buffer->cmdstat;
	st_analyze_sense(SRpnt, cmdstatp);

	if (cmdstatp->have_sense)
		scode = STp->buffer->cmdstat.sense_hdr.sense_key;
	else
		scode = 0;

        DEB(
        if (debugging) {
                printk(ST_DEB_MSG "%s: Error: %x, cmd: %x %x %x %x %x %x\n",
		       name, result,
		       SRpnt->cmd[0], SRpnt->cmd[1], SRpnt->cmd[2],
		       SRpnt->cmd[3], SRpnt->cmd[4], SRpnt->cmd[5]);
		if (cmdstatp->have_sense)
			 __scsi_print_sense(name, SRpnt->sense, SCSI_SENSE_BUFFERSIZE);
	} ) /* end DEB */
	if (!debugging) { /* Abnormal conditions for tape */
		if (!cmdstatp->have_sense)
			printk(KERN_WARNING
			       "%s: Error %x (driver bt 0x%x, host bt 0x%x).\n",
			       name, result, driver_byte(result),
			       host_byte(result));
		else if (cmdstatp->have_sense &&
			 scode != NO_SENSE &&
			 scode != RECOVERED_ERROR &&
			 /* scode != UNIT_ATTENTION && */
			 scode != BLANK_CHECK &&
			 scode != VOLUME_OVERFLOW &&
			 SRpnt->cmd[0] != MODE_SENSE &&
			 SRpnt->cmd[0] != TEST_UNIT_READY) {

			__scsi_print_sense(name, SRpnt->sense, SCSI_SENSE_BUFFERSIZE);
		}
	}

	if (cmdstatp->fixed_format &&
	    STp->cln_mode >= EXTENDED_SENSE_START) {  /* Only fixed format sense */
		if (STp->cln_sense_value)
			STp->cleaning_req |= ((SRpnt->sense[STp->cln_mode] &
					       STp->cln_sense_mask) == STp->cln_sense_value);
		else
			STp->cleaning_req |= ((SRpnt->sense[STp->cln_mode] &
					       STp->cln_sense_mask) != 0);
	}
	if (cmdstatp->have_sense &&
	    cmdstatp->sense_hdr.asc == 0 && cmdstatp->sense_hdr.ascq == 0x17)
		STp->cleaning_req = 1; /* ASC and ASCQ => cleaning requested */

	STp->pos_unknown |= STp->device->was_reset;

	if (cmdstatp->have_sense &&
	    scode == RECOVERED_ERROR
#if ST_RECOVERED_WRITE_FATAL
	    && SRpnt->cmd[0] != WRITE_6
	    && SRpnt->cmd[0] != WRITE_FILEMARKS
#endif
	    ) {
		STp->recover_count++;
		STp->recover_reg++;

                DEB(
		if (debugging) {
			if (SRpnt->cmd[0] == READ_6)
				stp = "read";
			else if (SRpnt->cmd[0] == WRITE_6)
				stp = "write";
			else
				stp = "ioctl";
			printk(ST_DEB_MSG "%s: Recovered %s error (%d).\n", name, stp,
			       STp->recover_count);
		} ) /* end DEB */

		if (cmdstatp->flags == 0)
			return 0;
	}
	return (-EIO);
}

static struct st_request *st_allocate_request(struct scsi_tape *stp)
{
	struct st_request *streq;

	streq = kzalloc(sizeof(*streq), GFP_KERNEL);
	if (streq)
		streq->stp = stp;
	else {
		DEBC(printk(KERN_ERR "%s: Can't get SCSI request.\n",
			    tape_name(stp)););
		if (signal_pending(current))
			stp->buffer->syscall_result = -EINTR;
		else
			stp->buffer->syscall_result = -EBUSY;
	}

	return streq;
}

static void st_release_request(struct st_request *streq)
{
	kfree(streq);
}

static void st_scsi_execute_end(struct request *req, int uptodate)
{
	struct st_request *SRpnt = req->end_io_data;
	struct scsi_tape *STp = SRpnt->stp;

	STp->buffer->cmdstat.midlevel_result = SRpnt->result = req->errors;
	STp->buffer->cmdstat.residual = req->resid_len;

	if (SRpnt->waiting)
		complete(SRpnt->waiting);

	blk_rq_unmap_user(SRpnt->bio);
	__blk_put_request(req->q, req);
}

static int st_scsi_execute(struct st_request *SRpnt, const unsigned char *cmd,
			   int data_direction, void *buffer, unsigned bufflen,
			   int timeout, int retries)
{
	struct request *req;
	struct rq_map_data *mdata = &SRpnt->stp->buffer->map_data;
	int err = 0;
	int write = (data_direction == DMA_TO_DEVICE);

	req = blk_get_request(SRpnt->stp->device->request_queue, write,
			      GFP_KERNEL);
	if (!req)
		return DRIVER_ERROR << 24;

	req->cmd_type = REQ_TYPE_BLOCK_PC;
	req->cmd_flags |= REQ_QUIET;

	mdata->null_mapped = 1;

	if (bufflen) {
		err = blk_rq_map_user(req->q, req, mdata, NULL, bufflen,
				      GFP_KERNEL);
		if (err) {
			blk_put_request(req);
			return DRIVER_ERROR << 24;
		}
	}

	SRpnt->bio = req->bio;
	req->cmd_len = COMMAND_SIZE(cmd[0]);
	memset(req->cmd, 0, BLK_MAX_CDB);
	memcpy(req->cmd, cmd, req->cmd_len);
	req->sense = SRpnt->sense;
	req->sense_len = 0;
	req->timeout = timeout;
	req->retries = retries;
	req->end_io_data = SRpnt;

	blk_execute_rq_nowait(req->q, NULL, req, 1, st_scsi_execute_end);
	return 0;
}

/* Do the scsi command. Waits until command performed if do_wait is true.
   Otherwise write_behind_check() is used to check that the command
   has finished. */
static struct st_request *
st_do_scsi(struct st_request * SRpnt, struct scsi_tape * STp, unsigned char *cmd,
	   int bytes, int direction, int timeout, int retries, int do_wait)
{
	struct completion *waiting;
	struct rq_map_data *mdata = &STp->buffer->map_data;
	int ret;

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

	if (!SRpnt) {
		SRpnt = st_allocate_request(STp);
		if (!SRpnt)
			return NULL;
	}

	/* If async IO, set last_SRpnt. This ptr tells write_behind_check
	   which IO is outstanding. It's nulled out when the IO completes. */
	if (!do_wait)
		(STp->buffer)->last_SRpnt = SRpnt;

	waiting = &STp->wait;
	init_completion(waiting);
	SRpnt->waiting = waiting;

	if (STp->buffer->do_dio) {
		mdata->nr_entries = STp->buffer->sg_segs;
		mdata->pages = STp->buffer->mapped_pages;
	} else {
		mdata->nr_entries =
			DIV_ROUND_UP(bytes, PAGE_SIZE << mdata->page_order);
		STp->buffer->map_data.pages = STp->buffer->reserved_pages;
		STp->buffer->map_data.offset = 0;
	}

	memcpy(SRpnt->cmd, cmd, sizeof(SRpnt->cmd));
	STp->buffer->cmdstat.have_sense = 0;
	STp->buffer->syscall_result = 0;

	ret = st_scsi_execute(SRpnt, cmd, direction, NULL, bytes, timeout,
			      retries);
	if (ret) {
		/* could not allocate the buffer or request was too large */
		(STp->buffer)->syscall_result = (-EBUSY);
		(STp->buffer)->last_SRpnt = NULL;
	} else if (do_wait) {
		wait_for_completion(waiting);
		SRpnt->waiting = NULL;
		(STp->buffer)->syscall_result = st_chk_result(STp, SRpnt);
	}

	return SRpnt;
}


/* Handle the write-behind checking (waits for completion). Returns -ENOSPC if
   write has been correct but EOM early warning reached, -EIO if write ended in
   error or zero if write successful. Asynchronous writes are used only in
   variable block mode. */
static int write_behind_check(struct scsi_tape * STp)
{
	int retval = 0;
	struct st_buffer *STbuffer;
	struct st_partstat *STps;
	struct st_cmdstatus *cmdstatp;
	struct st_request *SRpnt;

	STbuffer = STp->buffer;
	if (!STbuffer->writing)
		return 0;

        DEB(
	if (STp->write_pending)
		STp->nbr_waits++;
	else
		STp->nbr_finished++;
        ) /* end DEB */

	wait_for_completion(&(STp->wait));
	SRpnt = STbuffer->last_SRpnt;
	STbuffer->last_SRpnt = NULL;
	SRpnt->waiting = NULL;

	(STp->buffer)->syscall_result = st_chk_result(STp, SRpnt);
	st_release_request(SRpnt);

	STbuffer->buffer_bytes -= STbuffer->writing;
	STps = &(STp->ps[STp->partition]);
	if (STps->drv_block >= 0) {
		if (STp->block_size == 0)
			STps->drv_block++;
		else
			STps->drv_block += STbuffer->writing / STp->block_size;
	}

	cmdstatp = &STbuffer->cmdstat;
	if (STbuffer->syscall_result) {
		retval = -EIO;
		if (cmdstatp->have_sense && !cmdstatp->deferred &&
		    (cmdstatp->flags & SENSE_EOM) &&
		    (cmdstatp->sense_hdr.sense_key == NO_SENSE ||
		     cmdstatp->sense_hdr.sense_key == RECOVERED_ERROR)) {
			/* EOM at write-behind, has all data been written? */
			if (!cmdstatp->remainder_valid ||
			    cmdstatp->uremainder64 == 0)
				retval = -ENOSPC;
		}
		if (retval == -EIO)
			STps->drv_block = -1;
	}
	STbuffer->writing = 0;

	DEB(if (debugging && retval)
	    printk(ST_DEB_MSG "%s: Async write error %x, return value %d.\n",
		   tape_name(STp), STbuffer->cmdstat.midlevel_result, retval);) /* end DEB */

	return retval;
}


/* Step over EOF if it has been inadvertently crossed (ioctl not used because
   it messes up the block number). */
static int cross_eof(struct scsi_tape * STp, int forward)
{
	struct st_request *SRpnt;
	unsigned char cmd[MAX_COMMAND_SIZE];

	cmd[0] = SPACE;
	cmd[1] = 0x01;		/* Space FileMarks */
	if (forward) {
		cmd[2] = cmd[3] = 0;
		cmd[4] = 1;
	} else
		cmd[2] = cmd[3] = cmd[4] = 0xff;	/* -1 filemarks */
	cmd[5] = 0;

        DEBC(printk(ST_DEB_MSG "%s: Stepping over filemark %s.\n",
		   tape_name(STp), forward ? "forward" : "backward"));

	SRpnt = st_do_scsi(NULL, STp, cmd, 0, DMA_NONE,
			   STp->device->request_queue->rq_timeout,
			   MAX_RETRIES, 1);
	if (!SRpnt)
		return (STp->buffer)->syscall_result;

	st_release_request(SRpnt);
	SRpnt = NULL;

	if ((STp->buffer)->cmdstat.midlevel_result != 0)
		printk(KERN_ERR "%s: Stepping over filemark %s failed.\n",
		   tape_name(STp), forward ? "forward" : "backward");

	return (STp->buffer)->syscall_result;
}


/* Flush the write buffer (never need to write if variable blocksize). */
static int st_flush_write_buffer(struct scsi_tape * STp)
{
	int transfer, blks;
	int result;
	unsigned char cmd[MAX_COMMAND_SIZE];
	struct st_request *SRpnt;
	struct st_partstat *STps;

	result = write_behind_check(STp);
	if (result)
		return result;

	result = 0;
	if (STp->dirty == 1) {

		transfer = STp->buffer->buffer_bytes;
                DEBC(printk(ST_DEB_MSG "%s: Flushing %d bytes.\n",
                               tape_name(STp), transfer));

		memset(cmd, 0, MAX_COMMAND_SIZE);
		cmd[0] = WRITE_6;
		cmd[1] = 1;
		blks = transfer / STp->block_size;
		cmd[2] = blks >> 16;
		cmd[3] = blks >> 8;
		cmd[4] = blks;

		SRpnt = st_do_scsi(NULL, STp, cmd, transfer, DMA_TO_DEVICE,
				   STp->device->request_queue->rq_timeout,
				   MAX_WRITE_RETRIES, 1);
		if (!SRpnt)
			return (STp->buffer)->syscall_result;

		STps = &(STp->ps[STp->partition]);
		if ((STp->buffer)->syscall_result != 0) {
			struct st_cmdstatus *cmdstatp = &STp->buffer->cmdstat;

			if (cmdstatp->have_sense && !cmdstatp->deferred &&
			    (cmdstatp->flags & SENSE_EOM) &&
			    (cmdstatp->sense_hdr.sense_key == NO_SENSE ||
			     cmdstatp->sense_hdr.sense_key == RECOVERED_ERROR) &&
			    (!cmdstatp->remainder_valid ||
			     cmdstatp->uremainder64 == 0)) { /* All written at EOM early warning */
				STp->dirty = 0;
				(STp->buffer)->buffer_bytes = 0;
				if (STps->drv_block >= 0)
					STps->drv_block += blks;
				result = (-ENOSPC);
			} else {
				printk(KERN_ERR "%s: Error on flush.\n",
                                       tape_name(STp));
				STps->drv_block = (-1);
				result = (-EIO);
			}
		} else {
			if (STps->drv_block >= 0)
				STps->drv_block += blks;
			STp->dirty = 0;
			(STp->buffer)->buffer_bytes = 0;
		}
		st_release_request(SRpnt);
		SRpnt = NULL;
	}
	return result;
}


/* Flush the tape buffer. The tape will be positioned correctly unless
   seek_next is true. */
static int flush_buffer(struct scsi_tape *STp, int seek_next)
{
	int backspace, result;
	struct st_buffer *STbuffer;
	struct st_partstat *STps;

	STbuffer = STp->buffer;

	/*
	 * If there was a bus reset, block further access
	 * to this device.
	 */
	if (STp->pos_unknown)
		return (-EIO);

	if (STp->ready != ST_READY)
		return 0;
	STps = &(STp->ps[STp->partition]);
	if (STps->rw == ST_WRITING)	/* Writing */
		return st_flush_write_buffer(STp);

	if (STp->block_size == 0)
		return 0;

	backspace = ((STp->buffer)->buffer_bytes +
		     (STp->buffer)->read_pointer) / STp->block_size -
	    ((STp->buffer)->read_pointer + STp->block_size - 1) /
	    STp->block_size;
	(STp->buffer)->buffer_bytes = 0;
	(STp->buffer)->read_pointer = 0;
	result = 0;
	if (!seek_next) {
		if (STps->eof == ST_FM_HIT) {
			result = cross_eof(STp, 0);	/* Back over the EOF hit */
			if (!result)
				STps->eof = ST_NOEOF;
			else {
				if (STps->drv_file >= 0)
					STps->drv_file++;
				STps->drv_block = 0;
			}
		}
		if (!result && backspace > 0)
			result = st_int_ioctl(STp, MTBSR, backspace);
	} else if (STps->eof == ST_FM_HIT) {
		if (STps->drv_file >= 0)
			STps->drv_file++;
		STps->drv_block = 0;
		STps->eof = ST_NOEOF;
	}
	return result;

}

/* Set the mode parameters */
static int set_mode_densblk(struct scsi_tape * STp, struct st_modedef * STm)
{
	int set_it = 0;
	unsigned long arg;
	char *name = tape_name(STp);

	if (!STp->density_changed &&
	    STm->default_density >= 0 &&
	    STm->default_density != STp->density) {
		arg = STm->default_density;
		set_it = 1;
	} else
		arg = STp->density;
	arg <<= MT_ST_DENSITY_SHIFT;
	if (!STp->blksize_changed &&
	    STm->default_blksize >= 0 &&
	    STm->default_blksize != STp->block_size) {
		arg |= STm->default_blksize;
		set_it = 1;
	} else
		arg |= STp->block_size;
	if (set_it &&
	    st_int_ioctl(STp, SET_DENS_AND_BLK, arg)) {
		printk(KERN_WARNING
		       "%s: Can't set default block size to %d bytes and density %x.\n",
		       name, STm->default_blksize, STm->default_density);
		if (modes_defined)
			return (-EINVAL);
	}
	return 0;
}


/* Lock or unlock the drive door. Don't use when st_request allocated. */
static int do_door_lock(struct scsi_tape * STp, int do_lock)
{
	int retval, cmd;
	DEB(char *name = tape_name(STp);)


	cmd = do_lock ? SCSI_IOCTL_DOORLOCK : SCSI_IOCTL_DOORUNLOCK;
	DEBC(printk(ST_DEB_MSG "%s: %socking drive door.\n", name,
		    do_lock ? "L" : "Unl"));
	retval = scsi_ioctl(STp->device, cmd, NULL);
	if (!retval) {
		STp->door_locked = do_lock ? ST_LOCKED_EXPLICIT : ST_UNLOCKED;
	}
	else {
		STp->door_locked = ST_LOCK_FAILS;
	}
	return retval;
}


/* Set the internal state after reset */
static void reset_state(struct scsi_tape *STp)
{
	int i;
	struct st_partstat *STps;

	STp->pos_unknown = 0;
	for (i = 0; i < ST_NBR_PARTITIONS; i++) {
		STps = &(STp->ps[i]);
		STps->rw = ST_IDLE;
		STps->eof = ST_NOEOF;
		STps->at_sm = 0;
		STps->last_block_valid = 0;
		STps->drv_block = -1;
		STps->drv_file = -1;
	}
	if (STp->can_partitions) {
		STp->partition = find_partition(STp);
		if (STp->partition < 0)
			STp->partition = 0;
		STp->new_partition = STp->partition;
	}
}

/* Test if the drive is ready. Returns either one of the codes below or a negative system
   error code. */
#define CHKRES_READY       0
#define CHKRES_NEW_SESSION 1
#define CHKRES_NOT_READY   2
#define CHKRES_NO_TAPE     3

#define MAX_ATTENTIONS    10

static int test_ready(struct scsi_tape *STp, int do_wait)
{
	int attentions, waits, max_wait, scode;
	int retval = CHKRES_READY, new_session = 0;
	unsigned char cmd[MAX_COMMAND_SIZE];
	struct st_request *SRpnt = NULL;
	struct st_cmdstatus *cmdstatp = &STp->buffer->cmdstat;

	max_wait = do_wait ? ST_BLOCK_SECONDS : 0;

	for (attentions=waits=0; ; ) {
		memset((void *) &cmd[0], 0, MAX_COMMAND_SIZE);
		cmd[0] = TEST_UNIT_READY;
		SRpnt = st_do_scsi(SRpnt, STp, cmd, 0, DMA_NONE,
				   STp->long_timeout, MAX_READY_RETRIES, 1);

		if (!SRpnt) {
			retval = (STp->buffer)->syscall_result;
			break;
		}

		if (cmdstatp->have_sense) {

			scode = cmdstatp->sense_hdr.sense_key;

			if (scode == UNIT_ATTENTION) { /* New media? */
				new_session = 1;
				if (attentions < MAX_ATTENTIONS) {
					attentions++;
					continue;
				}
				else {
					retval = (-EIO);
					break;
				}
			}

			if (scode == NOT_READY) {
				if (waits < max_wait) {
					if (msleep_interruptible(1000)) {
						retval = (-EINTR);
						break;
					}
					waits++;
					continue;
				}
				else {
					if ((STp->device)->scsi_level >= SCSI_2 &&
					    cmdstatp->sense_hdr.asc == 0x3a)	/* Check ASC */
						retval = CHKRES_NO_TAPE;
					else
						retval = CHKRES_NOT_READY;
					break;
				}
			}
		}

		retval = (STp->buffer)->syscall_result;
		if (!retval)
			retval = new_session ? CHKRES_NEW_SESSION : CHKRES_READY;
		break;
	}

	if (SRpnt != NULL)
		st_release_request(SRpnt);
	return retval;
}


/* See if the drive is ready and gather information about the tape. Return values:
   < 0   negative error code from errno.h
   0     drive ready
   1     drive not ready (possibly no tape)
*/
static int check_tape(struct scsi_tape *STp, struct file *filp)
{
	int i, retval, new_session = 0, do_wait;
	unsigned char cmd[MAX_COMMAND_SIZE], saved_cleaning;
	unsigned short st_flags = filp->f_flags;
	struct st_request *SRpnt = NULL;
	struct st_modedef *STm;
	struct st_partstat *STps;
	char *name = tape_name(STp);
	struct inode *inode = filp->f_path.dentry->d_inode;
	int mode = TAPE_MODE(inode);

	STp->ready = ST_READY;

	if (mode != STp->current_mode) {
                DEBC(printk(ST_DEB_MSG "%s: Mode change from %d to %d.\n",
			       name, STp->current_mode, mode));
		new_session = 1;
		STp->current_mode = mode;
	}
	STm = &(STp->modes[STp->current_mode]);

	saved_cleaning = STp->cleaning_req;
	STp->cleaning_req = 0;

	do_wait = ((filp->f_flags & O_NONBLOCK) == 0);
	retval = test_ready(STp, do_wait);

	if (retval < 0)
	    goto err_out;

	if (retval == CHKRES_NEW_SESSION) {
		STp->pos_unknown = 0;
		STp->partition = STp->new_partition = 0;
		if (STp->can_partitions)
			STp->nbr_partitions = 1; /* This guess will be updated later
                                                    if necessary */
		for (i = 0; i < ST_NBR_PARTITIONS; i++) {
			STps = &(STp->ps[i]);
			STps->rw = ST_IDLE;
			STps->eof = ST_NOEOF;
			STps->at_sm = 0;
			STps->last_block_valid = 0;
			STps->drv_block = 0;
			STps->drv_file = 0;
		}
		new_session = 1;
	}
	else {
		STp->cleaning_req |= saved_cleaning;

		if (retval == CHKRES_NOT_READY || retval == CHKRES_NO_TAPE) {
			if (retval == CHKRES_NO_TAPE)
				STp->ready = ST_NO_TAPE;
			else
				STp->ready = ST_NOT_READY;

			STp->density = 0;	/* Clear the erroneous "residue" */
			STp->write_prot = 0;
			STp->block_size = 0;
			STp->ps[0].drv_file = STp->ps[0].drv_block = (-1);
			STp->partition = STp->new_partition = 0;
			STp->door_locked = ST_UNLOCKED;
			return CHKRES_NOT_READY;
		}
	}

	if (STp->omit_blklims)
		STp->min_block = STp->max_block = (-1);
	else {
		memset((void *) &cmd[0], 0, MAX_COMMAND_SIZE);
		cmd[0] = READ_BLOCK_LIMITS;

		SRpnt = st_do_scsi(SRpnt, STp, cmd, 6, DMA_FROM_DEVICE,
				   STp->device->request_queue->rq_timeout,
				   MAX_READY_RETRIES, 1);
		if (!SRpnt) {
			retval = (STp->buffer)->syscall_result;
			goto err_out;
		}

		if (!SRpnt->result && !STp->buffer->cmdstat.have_sense) {
			STp->max_block = ((STp->buffer)->b_data[1] << 16) |
			    ((STp->buffer)->b_data[2] << 8) | (STp->buffer)->b_data[3];
			STp->min_block = ((STp->buffer)->b_data[4] << 8) |
			    (STp->buffer)->b_data[5];
			if ( DEB( debugging || ) !STp->inited)
				printk(KERN_INFO
                                       "%s: Block limits %d - %d bytes.\n", name,
                                       STp->min_block, STp->max_block);
		} else {
			STp->min_block = STp->max_block = (-1);
                        DEBC(printk(ST_DEB_MSG "%s: Can't read block limits.\n",
                                       name));
		}
	}

	memset((void *) &cmd[0], 0, MAX_COMMAND_SIZE);
	cmd[0] = MODE_SENSE;
	cmd[4] = 12;

	SRpnt = st_do_scsi(SRpnt, STp, cmd, 12, DMA_FROM_DEVICE,
			   STp->device->request_queue->rq_timeout,
			   MAX_READY_RETRIES, 1);
	if (!SRpnt) {
		retval = (STp->buffer)->syscall_result;
		goto err_out;
	}

	if ((STp->buffer)->syscall_result != 0) {
                DEBC(printk(ST_DEB_MSG "%s: No Mode Sense.\n", name));
		STp->block_size = ST_DEFAULT_BLOCK;	/* Educated guess (?) */
		(STp->buffer)->syscall_result = 0;	/* Prevent error propagation */
		STp->drv_write_prot = 0;
	} else {
                DEBC(printk(ST_DEB_MSG
                            "%s: Mode sense. Length %d, medium %x, WBS %x, BLL %d\n",
                            name,
                            (STp->buffer)->b_data[0], (STp->buffer)->b_data[1],
                            (STp->buffer)->b_data[2], (STp->buffer)->b_data[3]));

		if ((STp->buffer)->b_data[3] >= 8) {
			STp->drv_buffer = ((STp->buffer)->b_data[2] >> 4) & 7;
			STp->density = (STp->buffer)->b_data[4];
			STp->block_size = (STp->buffer)->b_data[9] * 65536 +
			    (STp->buffer)->b_data[10] * 256 + (STp->buffer)->b_data[11];
                        DEBC(printk(ST_DEB_MSG
                                    "%s: Density %x, tape length: %x, drv buffer: %d\n",
                                    name, STp->density, (STp->buffer)->b_data[5] * 65536 +
                                    (STp->buffer)->b_data[6] * 256 + (STp->buffer)->b_data[7],
                                    STp->drv_buffer));
		}
		STp->drv_write_prot = ((STp->buffer)->b_data[2] & 0x80) != 0;
	}
	st_release_request(SRpnt);
	SRpnt = NULL;
        STp->inited = 1;

	if (STp->block_size > 0)
		(STp->buffer)->buffer_blocks =
                        (STp->buffer)->buffer_size / STp->block_size;
	else
		(STp->buffer)->buffer_blocks = 1;
	(STp->buffer)->buffer_bytes = (STp->buffer)->read_pointer = 0;

        DEBC(printk(ST_DEB_MSG
                       "%s: Block size: %d, buffer size: %d (%d blocks).\n", name,
		       STp->block_size, (STp->buffer)->buffer_size,
		       (STp->buffer)->buffer_blocks));

	if (STp->drv_write_prot) {
		STp->write_prot = 1;

                DEBC(printk(ST_DEB_MSG "%s: Write protected\n", name));

		if (do_wait &&
		    ((st_flags & O_ACCMODE) == O_WRONLY ||
		     (st_flags & O_ACCMODE) == O_RDWR)) {
			retval = (-EROFS);
			goto err_out;
		}
	}

	if (STp->can_partitions && STp->nbr_partitions < 1) {
		/* This code is reached when the device is opened for the first time
		   after the driver has been initialized with tape in the drive and the
		   partition support has been enabled. */
                DEBC(printk(ST_DEB_MSG
                            "%s: Updating partition number in status.\n", name));
		if ((STp->partition = find_partition(STp)) < 0) {
			retval = STp->partition;
			goto err_out;
		}
		STp->new_partition = STp->partition;
		STp->nbr_partitions = 1; /* This guess will be updated when necessary */
	}

	if (new_session) {	/* Change the drive parameters for the new mode */
		STp->density_changed = STp->blksize_changed = 0;
		STp->compression_changed = 0;
		if (!(STm->defaults_for_writes) &&
		    (retval = set_mode_densblk(STp, STm)) < 0)
		    goto err_out;

		if (STp->default_drvbuffer != 0xff) {
			if (st_int_ioctl(STp, MTSETDRVBUFFER, STp->default_drvbuffer))
				printk(KERN_WARNING
                                       "%s: Can't set default drive buffering to %d.\n",
				       name, STp->default_drvbuffer);
		}
	}

	return CHKRES_READY;

 err_out:
	return retval;
}


/* Open the device. Needs to take the BKL only because of incrementing the SCSI host
   module count. */
static int st_open(struct inode *inode, struct file *filp)
{
	int i, retval = (-EIO);
	struct scsi_tape *STp;
	struct st_partstat *STps;
	int dev = TAPE_NR(inode);
	char *name;

	lock_kernel();
	/*
	 * We really want to do nonseekable_open(inode, filp); here, but some
	 * versions of tar incorrectly call lseek on tapes and bail out if that
	 * fails.  So we disallow pread() and pwrite(), but permit lseeks.
	 */
	filp->f_mode &= ~(FMODE_PREAD | FMODE_PWRITE);

	if (!(STp = scsi_tape_get(dev))) {
		unlock_kernel();
		return -ENXIO;
	}

	write_lock(&st_dev_arr_lock);
	filp->private_data = STp;
	name = tape_name(STp);

	if (STp->in_use) {
		write_unlock(&st_dev_arr_lock);
		scsi_tape_put(STp);
		unlock_kernel();
		DEB( printk(ST_DEB_MSG "%s: Device already in use.\n", name); )
		return (-EBUSY);
	}

	STp->in_use = 1;
	write_unlock(&st_dev_arr_lock);
	STp->rew_at_close = STp->autorew_dev = (iminor(inode) & 0x80) == 0;

	if (!scsi_block_when_processing_errors(STp->device)) {
		retval = (-ENXIO);
		goto err_out;
	}

	/* See that we have at least a one page buffer available */
	if (!enlarge_buffer(STp->buffer, PAGE_SIZE, STp->restr_dma)) {
		printk(KERN_WARNING "%s: Can't allocate one page tape buffer.\n",
		       name);
		retval = (-EOVERFLOW);
		goto err_out;
	}

	(STp->buffer)->cleared = 0;
	(STp->buffer)->writing = 0;
	(STp->buffer)->syscall_result = 0;

	STp->write_prot = ((filp->f_flags & O_ACCMODE) == O_RDONLY);

	STp->dirty = 0;
	for (i = 0; i < ST_NBR_PARTITIONS; i++) {
		STps = &(STp->ps[i]);
		STps->rw = ST_IDLE;
	}
	STp->try_dio_now = STp->try_dio;
	STp->recover_count = 0;
	DEB( STp->nbr_waits = STp->nbr_finished = 0;
	     STp->nbr_requests = STp->nbr_dio = STp->nbr_pages = 0; )

	retval = check_tape(STp, filp);
	if (retval < 0)
		goto err_out;
	if ((filp->f_flags & O_NONBLOCK) == 0 &&
	    retval != CHKRES_READY) {
		if (STp->ready == NO_TAPE)
			retval = (-ENOMEDIUM);
		else
			retval = (-EIO);
		goto err_out;
	}
	unlock_kernel();
	return 0;

 err_out:
	normalize_buffer(STp->buffer);
	STp->in_use = 0;
	scsi_tape_put(STp);
	unlock_kernel();
	return retval;

}


/* Flush the tape buffer before close */
static int st_flush(struct file *filp, fl_owner_t id)
{
	int result = 0, result2;
	unsigned char cmd[MAX_COMMAND_SIZE];
	struct st_request *SRpnt;
	struct scsi_tape *STp = filp->private_data;
	struct st_modedef *STm = &(STp->modes[STp->current_mode]);
	struct st_partstat *STps = &(STp->ps[STp->partition]);
	char *name = tape_name(STp);

	if (file_count(filp) > 1)
		return 0;

	if (STps->rw == ST_WRITING && !STp->pos_unknown) {
		result = st_flush_write_buffer(STp);
		if (result != 0 && result != (-ENOSPC))
			goto out;
	}

	if (STp->can_partitions &&
	    (result2 = switch_partition(STp)) < 0) {
                DEBC(printk(ST_DEB_MSG
                               "%s: switch_partition at close failed.\n", name));
		if (result == 0)
			result = result2;
		goto out;
	}

	DEBC( if (STp->nbr_requests)
		printk(KERN_DEBUG "%s: Number of r/w requests %d, dio used in %d, pages %d.\n",
		       name, STp->nbr_requests, STp->nbr_dio, STp->nbr_pages));

	if (STps->rw == ST_WRITING && !STp->pos_unknown) {
		struct st_cmdstatus *cmdstatp = &STp->buffer->cmdstat;

                DEBC(printk(ST_DEB_MSG "%s: Async write waits %d, finished %d.\n",
                            name, STp->nbr_waits, STp->nbr_finished);
		)

		memset(cmd, 0, MAX_COMMAND_SIZE);
		cmd[0] = WRITE_FILEMARKS;
		cmd[4] = 1 + STp->two_fm;

		SRpnt = st_do_scsi(NULL, STp, cmd, 0, DMA_NONE,
				   STp->device->request_queue->rq_timeout,
				   MAX_WRITE_RETRIES, 1);
		if (!SRpnt) {
			result = (STp->buffer)->syscall_result;
			goto out;
		}

		if (STp->buffer->syscall_result == 0 ||
		    (cmdstatp->have_sense && !cmdstatp->deferred &&
		     (cmdstatp->flags & SENSE_EOM) &&
		     (cmdstatp->sense_hdr.sense_key == NO_SENSE ||
		      cmdstatp->sense_hdr.sense_key == RECOVERED_ERROR) &&
		     (!cmdstatp->remainder_valid || cmdstatp->uremainder64 == 0))) {
			/* Write successful at EOM */
			st_release_request(SRpnt);
			SRpnt = NULL;
			if (STps->drv_file >= 0)
				STps->drv_file++;
			STps->drv_block = 0;
			if (STp->two_fm)
				cross_eof(STp, 0);
			STps->eof = ST_FM;
		}
		else { /* Write error */
			st_release_request(SRpnt);
			SRpnt = NULL;
			printk(KERN_ERR "%s: Error on write filemark.\n", name);
			if (result == 0)
				result = (-EIO);
		}

                DEBC(printk(ST_DEB_MSG "%s: Buffer flushed, %d EOF(s) written\n",
                            name, cmd[4]));
	} else if (!STp->rew_at_close) {
		STps = &(STp->ps[STp->partition]);
		if (!STm->sysv || STps->rw != ST_READING) {
			if (STp->can_bsr)
				result = flush_buffer(STp, 0);
			else if (STps->eof == ST_FM_HIT) {
				result = cross_eof(STp, 0);
				if (result) {
					if (STps->drv_file >= 0)
						STps->drv_file++;
					STps->drv_block = 0;
					STps->eof = ST_FM;
				} else
					STps->eof = ST_NOEOF;
			}
		} else if ((STps->eof == ST_NOEOF &&
			    !(result = cross_eof(STp, 1))) ||
			   STps->eof == ST_FM_HIT) {
			if (STps->drv_file >= 0)
				STps->drv_file++;
			STps->drv_block = 0;
			STps->eof = ST_FM;
		}
	}

      out:
	if (STp->rew_at_close) {
		result2 = st_int_ioctl(STp, MTREW, 1);
		if (result == 0)
			result = result2;
	}
	return result;
}


/* Close the device and release it. BKL is not needed: this is the only thread
   accessing this tape. */
static int st_release(struct inode *inode, struct file *filp)
{
	int result = 0;
	struct scsi_tape *STp = filp->private_data;

	if (STp->door_locked == ST_LOCKED_AUTO)
		do_door_lock(STp, 0);

	normalize_buffer(STp->buffer);
	write_lock(&st_dev_arr_lock);
	STp->in_use = 0;
	write_unlock(&st_dev_arr_lock);
	scsi_tape_put(STp);

	return result;
}

/* The checks common to both reading and writing */
static ssize_t rw_checks(struct scsi_tape *STp, struct file *filp, size_t count)
{
	ssize_t retval = 0;

	/*
	 * If we are in the middle of error recovery, don't let anyone
	 * else try and use this device.  Also, if error recovery fails, it
	 * may try and take the device offline, in which case all further
	 * access to the device is prohibited.
	 */
	if (!scsi_block_when_processing_errors(STp->device)) {
		retval = (-ENXIO);
		goto out;
	}

	if (STp->ready != ST_READY) {
		if (STp->ready == ST_NO_TAPE)
			retval = (-ENOMEDIUM);
		else
			retval = (-EIO);
		goto out;
	}

	if (! STp->modes[STp->current_mode].defined) {
		retval = (-ENXIO);
		goto out;
	}


	/*
	 * If there was a bus reset, block further access
	 * to this device.
	 */
	if (STp->pos_unknown) {
		retval = (-EIO);
		goto out;
	}

	if (count == 0)
		goto out;

        DEB(
	if (!STp->in_use) {
		printk(ST_DEB_MSG "%s: Incorrect device.\n", tape_name(STp));
		retval = (-EIO);
		goto out;
	} ) /* end DEB */

	if (STp->can_partitions &&
	    (retval = switch_partition(STp)) < 0)
		goto out;

	if (STp->block_size == 0 && STp->max_block > 0 &&
	    (count < STp->min_block || count > STp->max_block)) {
		retval = (-EINVAL);
		goto out;
	}

	if (STp->do_auto_lock && STp->door_locked == ST_UNLOCKED &&
	    !do_door_lock(STp, 1))
		STp->door_locked = ST_LOCKED_AUTO;

 out:
	return retval;
}


static int setup_buffering(struct scsi_tape *STp, const char __user *buf,
			   size_t count, int is_read)
{
	int i, bufsize, retval = 0;
	struct st_buffer *STbp = STp->buffer;

	if (is_read)
		i = STp->try_dio_now && try_rdio;
	else
		i = STp->try_dio_now && try_wdio;

	if (i && ((unsigned long)buf & queue_dma_alignment(
					STp->device->request_queue)) == 0) {
		i = sgl_map_user_pages(STbp, STbp->use_sg, (unsigned long)buf,
				       count, (is_read ? READ : WRITE));
		if (i > 0) {
			STbp->do_dio = i;
			STbp->buffer_bytes = 0;   /* can be used as transfer counter */
		}
		else
			STbp->do_dio = 0;  /* fall back to buffering with any error */
		STbp->sg_segs = STbp->do_dio;
		DEB(
		     if (STbp->do_dio) {
			STp->nbr_dio++;
			STp->nbr_pages += STbp->do_dio;
		     }
		)
	} else
		STbp->do_dio = 0;
	DEB( STp->nbr_requests++; )

	if (!STbp->do_dio) {
		if (STp->block_size)
			bufsize = STp->block_size > st_fixed_buffer_size ?
				STp->block_size : st_fixed_buffer_size;
		else {
			bufsize = count;
			/* Make sure that data from previous user is not leaked even if
			   HBA does not return correct residual */
			if (is_read && STp->sili && !STbp->cleared)
				clear_buffer(STbp);
		}

		if (bufsize > STbp->buffer_size &&
		    !enlarge_buffer(STbp, bufsize, STp->restr_dma)) {
			printk(KERN_WARNING "%s: Can't allocate %d byte tape buffer.\n",
			       tape_name(STp), bufsize);
			retval = (-EOVERFLOW);
			goto out;
		}
		if (STp->block_size)
			STbp->buffer_blocks = bufsize / STp->block_size;
	}

 out:
	return retval;
}


/* Can be called more than once after each setup_buffer() */
static void release_buffering(struct scsi_tape *STp, int is_read)
{
	struct st_buffer *STbp;

	STbp = STp->buffer;
	if (STbp->do_dio) {
		sgl_unmap_user_pages(STbp, STbp->do_dio, is_read);
		STbp->do_dio = 0;
		STbp->sg_segs = 0;
	}
}


/* Write command */
static ssize_t
st_write(struct file *filp, const char __user *buf, size_t count, loff_t * ppos)
{
	ssize_t total;
	ssize_t i, do_count, blks, transfer;
	ssize_t retval;
	int undone, retry_eot = 0, scode;
	int async_write;
	unsigned char cmd[MAX_COMMAND_SIZE];
	const char __user *b_point;
	struct st_request *SRpnt = NULL;
	struct scsi_tape *STp = filp->private_data;
	struct st_modedef *STm;
	struct st_partstat *STps;
	struct st_buffer *STbp;
	char *name = tape_name(STp);

	if (mutex_lock_interruptible(&STp->lock))
		return -ERESTARTSYS;

	retval = rw_checks(STp, filp, count);
	if (retval || count == 0)
		goto out;

	/* Write must be integral number of blocks */
	if (STp->block_size != 0 && (count % STp->block_size) != 0) {
		printk(KERN_WARNING "%s: Write not multiple of tape block size.\n",
		       name);
		retval = (-EINVAL);
		goto out;
	}

	STm = &(STp->modes[STp->current_mode]);
	STps = &(STp->ps[STp->partition]);

	if (STp->write_prot) {
		retval = (-EACCES);
		goto out;
	}


	if (STps->rw == ST_READING) {
		retval = flush_buffer(STp, 0);
		if (retval)
			goto out;
		STps->rw = ST_WRITING;
	} else if (STps->rw != ST_WRITING &&
		   STps->drv_file == 0 && STps->drv_block == 0) {
		if ((retval = set_mode_densblk(STp, STm)) < 0)
			goto out;
		if (STm->default_compression != ST_DONT_TOUCH &&
		    !(STp->compression_changed)) {
			if (st_compression(STp, (STm->default_compression == ST_YES))) {
				printk(KERN_WARNING "%s: Can't set default compression.\n",
				       name);
				if (modes_defined) {
					retval = (-EINVAL);
					goto out;
				}
			}
		}
	}

	STbp = STp->buffer;
	i = write_behind_check(STp);
	if (i) {
		if (i == -ENOSPC)
			STps->eof = ST_EOM_OK;
		else
			STps->eof = ST_EOM_ERROR;
	}

	if (STps->eof == ST_EOM_OK) {
		STps->eof = ST_EOD_1;  /* allow next write */
		retval = (-ENOSPC);
		goto out;
	}
	else if (STps->eof == ST_EOM_ERROR) {
		retval = (-EIO);
		goto out;
	}

	/* Check the buffer readability in cases where copy_user might catch
	   the problems after some tape movement. */
	if (STp->block_size != 0 &&
	    !STbp->do_dio &&
	    (copy_from_user(&i, buf, 1) != 0 ||
	     copy_from_user(&i, buf + count - 1, 1) != 0)) {
		retval = (-EFAULT);
		goto out;
	}

	retval = setup_buffering(STp, buf, count, 0);
	if (retval)
		goto out;

	total = count;

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = WRITE_6;
	cmd[1] = (STp->block_size != 0);

	STps->rw = ST_WRITING;

	b_point = buf;
	while (count > 0 && !retry_eot) {

		if (STbp->do_dio) {
			do_count = count;
		}
		else {
			if (STp->block_size == 0)
				do_count = count;
			else {
				do_count = STbp->buffer_blocks * STp->block_size -
					STbp->buffer_bytes;
				if (do_count > count)
					do_count = count;
			}

			i = append_to_buffer(b_point, STbp, do_count);
			if (i) {
				retval = i;
				goto out;
			}
		}
		count -= do_count;
		b_point += do_count;

		async_write = STp->block_size == 0 && !STbp->do_dio &&
			STm->do_async_writes && STps->eof < ST_EOM_OK;

		if (STp->block_size != 0 && STm->do_buffer_writes &&
		    !(STp->try_dio_now && try_wdio) && STps->eof < ST_EOM_OK &&
		    STbp->buffer_bytes < STbp->buffer_size) {
			STp->dirty = 1;
			/* Don't write a buffer that is not full enough. */
			if (!async_write && count == 0)
				break;
		}

	retry_write:
		if (STp->block_size == 0)
			blks = transfer = do_count;
		else {
			if (!STbp->do_dio)
				blks = STbp->buffer_bytes;
			else
				blks = do_count;
			blks /= STp->block_size;
			transfer = blks * STp->block_size;
		}
		cmd[2] = blks >> 16;
		cmd[3] = blks >> 8;
		cmd[4] = blks;

		SRpnt = st_do_scsi(SRpnt, STp, cmd, transfer, DMA_TO_DEVICE,
				   STp->device->request_queue->rq_timeout,
				   MAX_WRITE_RETRIES, !async_write);
		if (!SRpnt) {
			retval = STbp->syscall_result;
			goto out;
		}
		if (async_write && !STbp->syscall_result) {
			STbp->writing = transfer;
			STp->dirty = !(STbp->writing ==
				       STbp->buffer_bytes);
			SRpnt = NULL;  /* Prevent releasing this request! */
			DEB( STp->write_pending = 1; )
			break;
		}

		if (STbp->syscall_result != 0) {
			struct st_cmdstatus *cmdstatp = &STp->buffer->cmdstat;

                        DEBC(printk(ST_DEB_MSG "%s: Error on write:\n", name));
			if (cmdstatp->have_sense && (cmdstatp->flags & SENSE_EOM)) {
				scode = cmdstatp->sense_hdr.sense_key;
				if (cmdstatp->remainder_valid)
					undone = (int)cmdstatp->uremainder64;
				else if (STp->block_size == 0 &&
					 scode == VOLUME_OVERFLOW)
					undone = transfer;
				else
					undone = 0;
				if (STp->block_size != 0)
					undone *= STp->block_size;
				if (undone <= do_count) {
					/* Only data from this write is not written */
					count += undone;
					b_point -= undone;
					do_count -= undone;
					if (STp->block_size)
						blks = (transfer - undone) / STp->block_size;
					STps->eof = ST_EOM_OK;
					/* Continue in fixed block mode if all written
					   in this request but still something left to write
					   (retval left to zero)
					*/
					if (STp->block_size == 0 ||
					    undone > 0 || count == 0)
						retval = (-ENOSPC); /* EOM within current request */
                                        DEBC(printk(ST_DEB_MSG
                                                       "%s: EOM with %d bytes unwritten.\n",
						       name, (int)count));
				} else {
					/* EOT within data buffered earlier (possible only
					   in fixed block mode without direct i/o) */
					if (!retry_eot && !cmdstatp->deferred &&
					    (scode == NO_SENSE || scode == RECOVERED_ERROR)) {
						move_buffer_data(STp->buffer, transfer - undone);
						retry_eot = 1;
						if (STps->drv_block >= 0) {
							STps->drv_block += (transfer - undone) /
								STp->block_size;
						}
						STps->eof = ST_EOM_OK;
						DEBC(printk(ST_DEB_MSG
							    "%s: Retry write of %d bytes at EOM.\n",
							    name, STp->buffer->buffer_bytes));
						goto retry_write;
					}
					else {
						/* Either error within data buffered by driver or
						   failed retry */
						count -= do_count;
						blks = do_count = 0;
						STps->eof = ST_EOM_ERROR;
						STps->drv_block = (-1); /* Too cautious? */
						retval = (-EIO);	/* EOM for old data */
						DEBC(printk(ST_DEB_MSG
							    "%s: EOM with lost data.\n",
							    name));
					}
				}
			} else {
				count += do_count;
				STps->drv_block = (-1);		/* Too cautious? */
				retval = STbp->syscall_result;
			}

		}

		if (STps->drv_block >= 0) {
			if (STp->block_size == 0)
				STps->drv_block += (do_count > 0);
			else
				STps->drv_block += blks;
		}

		STbp->buffer_bytes = 0;
		STp->dirty = 0;

		if (retval || retry_eot) {
			if (count < total)
				retval = total - count;
			goto out;
		}
	}

	if (STps->eof == ST_EOD_1)
		STps->eof = ST_EOM_OK;
	else if (STps->eof != ST_EOM_OK)
		STps->eof = ST_NOEOF;
	retval = total - count;

 out:
	if (SRpnt != NULL)
		st_release_request(SRpnt);
	release_buffering(STp, 0);
	mutex_unlock(&STp->lock);

	return retval;
}

/* Read data from the tape. Returns zero in the normal case, one if the
   eof status has changed, and the negative error code in case of a
   fatal error. Otherwise updates the buffer and the eof state.

   Does release user buffer mapping if it is set.
*/
static long read_tape(struct scsi_tape *STp, long count,
		      struct st_request ** aSRpnt)
{
	int transfer, blks, bytes;
	unsigned char cmd[MAX_COMMAND_SIZE];
	struct st_request *SRpnt;
	struct st_modedef *STm;
	struct st_partstat *STps;
	struct st_buffer *STbp;
	int retval = 0;
	char *name = tape_name(STp);

	if (count == 0)
		return 0;

	STm = &(STp->modes[STp->current_mode]);
	STps = &(STp->ps[STp->partition]);
	if (STps->eof == ST_FM_HIT)
		return 1;
	STbp = STp->buffer;

	if (STp->block_size == 0)
		blks = bytes = count;
	else {
		if (!(STp->try_dio_now && try_rdio) && STm->do_read_ahead) {
			blks = (STp->buffer)->buffer_blocks;
			bytes = blks * STp->block_size;
		} else {
			bytes = count;
			if (!STbp->do_dio && bytes > (STp->buffer)->buffer_size)
				bytes = (STp->buffer)->buffer_size;
			blks = bytes / STp->block_size;
			bytes = blks * STp->block_size;
		}
	}

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = READ_6;
	cmd[1] = (STp->block_size != 0);
	if (!cmd[1] && STp->sili)
		cmd[1] |= 2;
	cmd[2] = blks >> 16;
	cmd[3] = blks >> 8;
	cmd[4] = blks;

	SRpnt = *aSRpnt;
	SRpnt = st_do_scsi(SRpnt, STp, cmd, bytes, DMA_FROM_DEVICE,
			   STp->device->request_queue->rq_timeout,
			   MAX_RETRIES, 1);
	release_buffering(STp, 1);
	*aSRpnt = SRpnt;
	if (!SRpnt)
		return STbp->syscall_result;

	STbp->read_pointer = 0;
	STps->at_sm = 0;

	/* Something to check */
	if (STbp->syscall_result) {
		struct st_cmdstatus *cmdstatp = &STp->buffer->cmdstat;

		retval = 1;
		DEBC(printk(ST_DEB_MSG "%s: Sense: %2x %2x %2x %2x %2x %2x %2x %2x\n",
                            name,
                            SRpnt->sense[0], SRpnt->sense[1],
                            SRpnt->sense[2], SRpnt->sense[3],
                            SRpnt->sense[4], SRpnt->sense[5],
                            SRpnt->sense[6], SRpnt->sense[7]));
		if (cmdstatp->have_sense) {

			if (cmdstatp->sense_hdr.sense_key == BLANK_CHECK)
				cmdstatp->flags &= 0xcf;	/* No need for EOM in this case */

			if (cmdstatp->flags != 0) { /* EOF, EOM, or ILI */
				/* Compute the residual count */
				if (cmdstatp->remainder_valid)
					transfer = (int)cmdstatp->uremainder64;
				else
					transfer = 0;
				if (STp->block_size == 0 &&
				    cmdstatp->sense_hdr.sense_key == MEDIUM_ERROR)
					transfer = bytes;

				if (cmdstatp->flags & SENSE_ILI) {	/* ILI */
					if (STp->block_size == 0) {
						if (transfer <= 0) {
							if (transfer < 0)
								printk(KERN_NOTICE
								       "%s: Failed to read %d byte block with %d byte transfer.\n",
								       name, bytes - transfer, bytes);
							if (STps->drv_block >= 0)
								STps->drv_block += 1;
							STbp->buffer_bytes = 0;
							return (-ENOMEM);
						}
						STbp->buffer_bytes = bytes - transfer;
					} else {
						st_release_request(SRpnt);
						SRpnt = *aSRpnt = NULL;
						if (transfer == blks) {	/* We did not get anything, error */
							printk(KERN_NOTICE "%s: Incorrect block size.\n", name);
							if (STps->drv_block >= 0)
								STps->drv_block += blks - transfer + 1;
							st_int_ioctl(STp, MTBSR, 1);
							return (-EIO);
						}
						/* We have some data, deliver it */
						STbp->buffer_bytes = (blks - transfer) *
						    STp->block_size;
                                                DEBC(printk(ST_DEB_MSG
                                                            "%s: ILI but enough data received %ld %d.\n",
                                                            name, count, STbp->buffer_bytes));
						if (STps->drv_block >= 0)
							STps->drv_block += 1;
						if (st_int_ioctl(STp, MTBSR, 1))
							return (-EIO);
					}
				} else if (cmdstatp->flags & SENSE_FMK) {	/* FM overrides EOM */
					if (STps->eof != ST_FM_HIT)
						STps->eof = ST_FM_HIT;
					else
						STps->eof = ST_EOD_2;
					if (STp->block_size == 0)
						STbp->buffer_bytes = 0;
					else
						STbp->buffer_bytes =
						    bytes - transfer * STp->block_size;
                                        DEBC(printk(ST_DEB_MSG
                                                    "%s: EOF detected (%d bytes read).\n",
                                                    name, STbp->buffer_bytes));
				} else if (cmdstatp->flags & SENSE_EOM) {
					if (STps->eof == ST_FM)
						STps->eof = ST_EOD_1;
					else
						STps->eof = ST_EOM_OK;
					if (STp->block_size == 0)
						STbp->buffer_bytes = bytes - transfer;
					else
						STbp->buffer_bytes =
						    bytes - transfer * STp->block_size;

                                        DEBC(printk(ST_DEB_MSG "%s: EOM detected (%d bytes read).\n",
                                                    name, STbp->buffer_bytes));
				}
			}
			/* end of EOF, EOM, ILI test */ 
			else {	/* nonzero sense key */
                                DEBC(printk(ST_DEB_MSG
                                            "%s: Tape error while reading.\n", name));
				STps->drv_block = (-1);
				if (STps->eof == ST_FM &&
				    cmdstatp->sense_hdr.sense_key == BLANK_CHECK) {
                                        DEBC(printk(ST_DEB_MSG
                                                    "%s: Zero returned for first BLANK CHECK after EOF.\n",
                                                    name));
					STps->eof = ST_EOD_2;	/* First BLANK_CHECK after FM */
				} else	/* Some other extended sense code */
					retval = (-EIO);
			}

			if (STbp->buffer_bytes < 0)  /* Caused by bogus sense data */
				STbp->buffer_bytes = 0;
		}
		/* End of extended sense test */ 
		else {		/* Non-extended sense */
			retval = STbp->syscall_result;
		}

	}
	/* End of error handling */ 
	else {			/* Read successful */
		STbp->buffer_bytes = bytes;
		if (STp->sili) /* In fixed block mode residual is always zero here */
			STbp->buffer_bytes -= STp->buffer->cmdstat.residual;
	}

	if (STps->drv_block >= 0) {
		if (STp->block_size == 0)
			STps->drv_block++;
		else
			STps->drv_block += STbp->buffer_bytes / STp->block_size;
	}
	return retval;
}


/* Read command */
static ssize_t
st_read(struct file *filp, char __user *buf, size_t count, loff_t * ppos)
{
	ssize_t total;
	ssize_t retval = 0;
	ssize_t i, transfer;
	int special, do_dio = 0;
	struct st_request *SRpnt = NULL;
	struct scsi_tape *STp = filp->private_data;
	struct st_modedef *STm;
	struct st_partstat *STps;
	struct st_buffer *STbp = STp->buffer;
	DEB( char *name = tape_name(STp); )

	if (mutex_lock_interruptible(&STp->lock))
		return -ERESTARTSYS;

	retval = rw_checks(STp, filp, count);
	if (retval || count == 0)
		goto out;

	STm = &(STp->modes[STp->current_mode]);
	if (STp->block_size != 0 && (count % STp->block_size) != 0) {
		if (!STm->do_read_ahead) {
			retval = (-EINVAL);	/* Read must be integral number of blocks */
			goto out;
		}
		STp->try_dio_now = 0;  /* Direct i/o can't handle split blocks */
	}

	STps = &(STp->ps[STp->partition]);
	if (STps->rw == ST_WRITING) {
		retval = flush_buffer(STp, 0);
		if (retval)
			goto out;
		STps->rw = ST_READING;
	}
        DEB(
	if (debugging && STps->eof != ST_NOEOF)
		printk(ST_DEB_MSG "%s: EOF/EOM flag up (%d). Bytes %d\n", name,
		       STps->eof, STbp->buffer_bytes);
        ) /* end DEB */

	retval = setup_buffering(STp, buf, count, 1);
	if (retval)
		goto out;
	do_dio = STbp->do_dio;

	if (STbp->buffer_bytes == 0 &&
	    STps->eof >= ST_EOD_1) {
		if (STps->eof < ST_EOD) {
			STps->eof += 1;
			retval = 0;
			goto out;
		}
		retval = (-EIO);	/* EOM or Blank Check */
		goto out;
	}

	if (do_dio) {
		/* Check the buffer writability before any tape movement. Don't alter
		   buffer data. */
		if (copy_from_user(&i, buf, 1) != 0 ||
		    copy_to_user(buf, &i, 1) != 0 ||
		    copy_from_user(&i, buf + count - 1, 1) != 0 ||
		    copy_to_user(buf + count - 1, &i, 1) != 0) {
			retval = (-EFAULT);
			goto out;
		}
	}

	STps->rw = ST_READING;


	/* Loop until enough data in buffer or a special condition found */
	for (total = 0, special = 0; total < count && !special;) {

		/* Get new data if the buffer is empty */
		if (STbp->buffer_bytes == 0) {
			special = read_tape(STp, count - total, &SRpnt);
			if (special < 0) {	/* No need to continue read */
				retval = special;
				goto out;
			}
		}

		/* Move the data from driver buffer to user buffer */
		if (STbp->buffer_bytes > 0) {
                        DEB(
			if (debugging && STps->eof != ST_NOEOF)
				printk(ST_DEB_MSG
                                       "%s: EOF up (%d). Left %d, needed %d.\n", name,
				       STps->eof, STbp->buffer_bytes,
                                       (int)(count - total));
                        ) /* end DEB */
			transfer = STbp->buffer_bytes < count - total ?
			    STbp->buffer_bytes : count - total;
			if (!do_dio) {
				i = from_buffer(STbp, buf, transfer);
				if (i) {
					retval = i;
					goto out;
				}
			}
			buf += transfer;
			total += transfer;
		}

		if (STp->block_size == 0)
			break;	/* Read only one variable length block */

	}			/* for (total = 0, special = 0;
                                   total < count && !special; ) */

	/* Change the eof state if no data from tape or buffer */
	if (total == 0) {
		if (STps->eof == ST_FM_HIT) {
			STps->eof = ST_FM;
			STps->drv_block = 0;
			if (STps->drv_file >= 0)
				STps->drv_file++;
		} else if (STps->eof == ST_EOD_1) {
			STps->eof = ST_EOD_2;
			STps->drv_block = 0;
			if (STps->drv_file >= 0)
				STps->drv_file++;
		} else if (STps->eof == ST_EOD_2)
			STps->eof = ST_EOD;
	} else if (STps->eof == ST_FM)
		STps->eof = ST_NOEOF;
	retval = total;

 out:
	if (SRpnt != NULL) {
		st_release_request(SRpnt);
		SRpnt = NULL;
	}
	if (do_dio) {
		release_buffering(STp, 1);
		STbp->buffer_bytes = 0;
	}
	mutex_unlock(&STp->lock);

	return retval;
}



DEB(
/* Set the driver options */
static void st_log_options(struct scsi_tape * STp, struct st_modedef * STm, char *name)
{
	if (debugging) {
		printk(KERN_INFO
		       "%s: Mode %d options: buffer writes: %d, async writes: %d, read ahead: %d\n",
		       name, STp->current_mode, STm->do_buffer_writes, STm->do_async_writes,
		       STm->do_read_ahead);
		printk(KERN_INFO
		       "%s:    can bsr: %d, two FMs: %d, fast mteom: %d, auto lock: %d,\n",
		       name, STp->can_bsr, STp->two_fm, STp->fast_mteom, STp->do_auto_lock);
		printk(KERN_INFO
		       "%s:    defs for wr: %d, no block limits: %d, partitions: %d, s2 log: %d\n",
		       name, STm->defaults_for_writes, STp->omit_blklims, STp->can_partitions,
		       STp->scsi2_logical);
		printk(KERN_INFO
		       "%s:    sysv: %d nowait: %d sili: %d\n", name, STm->sysv, STp->immediate,
			STp->sili);
		printk(KERN_INFO "%s:    debugging: %d\n",
		       name, debugging);
	}
}
	)


static int st_set_options(struct scsi_tape *STp, long options)
{
	int value;
	long code;
	struct st_modedef *STm;
	char *name = tape_name(STp);
	struct cdev *cd0, *cd1;

	STm = &(STp->modes[STp->current_mode]);
	if (!STm->defined) {
		cd0 = STm->cdevs[0]; cd1 = STm->cdevs[1];
		memcpy(STm, &(STp->modes[0]), sizeof(struct st_modedef));
		STm->cdevs[0] = cd0; STm->cdevs[1] = cd1;
		modes_defined = 1;
                DEBC(printk(ST_DEB_MSG
                            "%s: Initialized mode %d definition from mode 0\n",
                            name, STp->current_mode));
	}

	code = options & MT_ST_OPTIONS;
	if (code == MT_ST_BOOLEANS) {
		STm->do_buffer_writes = (options & MT_ST_BUFFER_WRITES) != 0;
		STm->do_async_writes = (options & MT_ST_ASYNC_WRITES) != 0;
		STm->defaults_for_writes = (options & MT_ST_DEF_WRITES) != 0;
		STm->do_read_ahead = (options & MT_ST_READ_AHEAD) != 0;
		STp->two_fm = (options & MT_ST_TWO_FM) != 0;
		STp->fast_mteom = (options & MT_ST_FAST_MTEOM) != 0;
		STp->do_auto_lock = (options & MT_ST_AUTO_LOCK) != 0;
		STp->can_bsr = (options & MT_ST_CAN_BSR) != 0;
		STp->omit_blklims = (options & MT_ST_NO_BLKLIMS) != 0;
		if ((STp->device)->scsi_level >= SCSI_2)
			STp->can_partitions = (options & MT_ST_CAN_PARTITIONS) != 0;
		STp->scsi2_logical = (options & MT_ST_SCSI2LOGICAL) != 0;
		STp->immediate = (options & MT_ST_NOWAIT) != 0;
		STm->sysv = (options & MT_ST_SYSV) != 0;
		STp->sili = (options & MT_ST_SILI) != 0;
		DEB( debugging = (options & MT_ST_DEBUGGING) != 0;
		     st_log_options(STp, STm, name); )
	} else if (code == MT_ST_SETBOOLEANS || code == MT_ST_CLEARBOOLEANS) {
		value = (code == MT_ST_SETBOOLEANS);
		if ((options & MT_ST_BUFFER_WRITES) != 0)
			STm->do_buffer_writes = value;
		if ((options & MT_ST_ASYNC_WRITES) != 0)
			STm->do_async_writes = value;
		if ((options & MT_ST_DEF_WRITES) != 0)
			STm->defaults_for_writes = value;
		if ((options & MT_ST_READ_AHEAD) != 0)
			STm->do_read_ahead = value;
		if ((options & MT_ST_TWO_FM) != 0)
			STp->two_fm = value;
		if ((options & MT_ST_FAST_MTEOM) != 0)
			STp->fast_mteom = value;
		if ((options & MT_ST_AUTO_LOCK) != 0)
			STp->do_auto_lock = value;
		if ((options & MT_ST_CAN_BSR) != 0)
			STp->can_bsr = value;
		if ((options & MT_ST_NO_BLKLIMS) != 0)
			STp->omit_blklims = value;
		if ((STp->device)->scsi_level >= SCSI_2 &&
		    (options & MT_ST_CAN_PARTITIONS) != 0)
			STp->can_partitions = value;
		if ((options & MT_ST_SCSI2LOGICAL) != 0)
			STp->scsi2_logical = value;
		if ((options & MT_ST_NOWAIT) != 0)
			STp->immediate = value;
		if ((options & MT_ST_SYSV) != 0)
			STm->sysv = value;
		if ((options & MT_ST_SILI) != 0)
			STp->sili = value;
                DEB(
		if ((options & MT_ST_DEBUGGING) != 0)
			debugging = value;
			st_log_options(STp, STm, name); )
	} else if (code == MT_ST_WRITE_THRESHOLD) {
		/* Retained for compatibility */
	} else if (code == MT_ST_DEF_BLKSIZE) {
		value = (options & ~MT_ST_OPTIONS);
		if (value == ~MT_ST_OPTIONS) {
			STm->default_blksize = (-1);
			DEBC( printk(KERN_INFO "%s: Default block size disabled.\n", name));
		} else {
			STm->default_blksize = value;
			DEBC( printk(KERN_INFO "%s: Default block size set to %d bytes.\n",
			       name, STm->default_blksize));
			if (STp->ready == ST_READY) {
				STp->blksize_changed = 0;
				set_mode_densblk(STp, STm);
			}
		}
	} else if (code == MT_ST_TIMEOUTS) {
		value = (options & ~MT_ST_OPTIONS);
		if ((value & MT_ST_SET_LONG_TIMEOUT) != 0) {
			STp->long_timeout = (value & ~MT_ST_SET_LONG_TIMEOUT) * HZ;
			DEBC( printk(KERN_INFO "%s: Long timeout set to %d seconds.\n", name,
			       (value & ~MT_ST_SET_LONG_TIMEOUT)));
		} else {
			blk_queue_rq_timeout(STp->device->request_queue,
					     value * HZ);
			DEBC( printk(KERN_INFO "%s: Normal timeout set to %d seconds.\n",
				name, value) );
		}
	} else if (code == MT_ST_SET_CLN) {
		value = (options & ~MT_ST_OPTIONS) & 0xff;
		if (value != 0 &&
		    value < EXTENDED_SENSE_START && value >= SCSI_SENSE_BUFFERSIZE)
			return (-EINVAL);
		STp->cln_mode = value;
		STp->cln_sense_mask = (options >> 8) & 0xff;
		STp->cln_sense_value = (options >> 16) & 0xff;
		printk(KERN_INFO
		       "%s: Cleaning request mode %d, mask %02x, value %02x\n",
		       name, value, STp->cln_sense_mask, STp->cln_sense_value);
	} else if (code == MT_ST_DEF_OPTIONS) {
		code = (options & ~MT_ST_CLEAR_DEFAULT);
		value = (options & MT_ST_CLEAR_DEFAULT);
		if (code == MT_ST_DEF_DENSITY) {
			if (value == MT_ST_CLEAR_DEFAULT) {
				STm->default_density = (-1);
				DEBC( printk(KERN_INFO "%s: Density default disabled.\n",
                                       name));
			} else {
				STm->default_density = value & 0xff;
				DEBC( printk(KERN_INFO "%s: Density default set to %x\n",
				       name, STm->default_density));
				if (STp->ready == ST_READY) {
					STp->density_changed = 0;
					set_mode_densblk(STp, STm);
				}
			}
		} else if (code == MT_ST_DEF_DRVBUFFER) {
			if (value == MT_ST_CLEAR_DEFAULT) {
				STp->default_drvbuffer = 0xff;
				DEBC( printk(KERN_INFO
                                       "%s: Drive buffer default disabled.\n", name));
			} else {
				STp->default_drvbuffer = value & 7;
				DEBC( printk(KERN_INFO
                                       "%s: Drive buffer default set to %x\n",
				       name, STp->default_drvbuffer));
				if (STp->ready == ST_READY)
					st_int_ioctl(STp, MTSETDRVBUFFER, STp->default_drvbuffer);
			}
		} else if (code == MT_ST_DEF_COMPRESSION) {
			if (value == MT_ST_CLEAR_DEFAULT) {
				STm->default_compression = ST_DONT_TOUCH;
				DEBC( printk(KERN_INFO
                                       "%s: Compression default disabled.\n", name));
			} else {
				if ((value & 0xff00) != 0) {
					STp->c_algo = (value & 0xff00) >> 8;
					DEBC( printk(KERN_INFO "%s: Compression algorithm set to 0x%x.\n",
					       name, STp->c_algo));
				}
				if ((value & 0xff) != 0xff) {
					STm->default_compression = (value & 1 ? ST_YES : ST_NO);
					DEBC( printk(KERN_INFO "%s: Compression default set to %x\n",
					       name, (value & 1)));
					if (STp->ready == ST_READY) {
						STp->compression_changed = 0;
						st_compression(STp, (STm->default_compression == ST_YES));
					}
				}
			}
		}
	} else
		return (-EIO);

	return 0;
}

#define MODE_HEADER_LENGTH  4

/* Mode header and page byte offsets */
#define MH_OFF_DATA_LENGTH     0
#define MH_OFF_MEDIUM_TYPE     1
#define MH_OFF_DEV_SPECIFIC    2
#define MH_OFF_BDESCS_LENGTH   3
#define MP_OFF_PAGE_NBR        0
#define MP_OFF_PAGE_LENGTH     1

/* Mode header and page bit masks */
#define MH_BIT_WP              0x80
#define MP_MSK_PAGE_NBR        0x3f

/* Don't return block descriptors */
#define MODE_SENSE_OMIT_BDESCS 0x08

#define MODE_SELECT_PAGE_FORMAT 0x10

/* Read a mode page into the tape buffer. The block descriptors are included
   if incl_block_descs is true. The page control is ored to the page number
   parameter, if necessary. */
static int read_mode_page(struct scsi_tape *STp, int page, int omit_block_descs)
{
	unsigned char cmd[MAX_COMMAND_SIZE];
	struct st_request *SRpnt;

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = MODE_SENSE;
	if (omit_block_descs)
		cmd[1] = MODE_SENSE_OMIT_BDESCS;
	cmd[2] = page;
	cmd[4] = 255;

	SRpnt = st_do_scsi(NULL, STp, cmd, cmd[4], DMA_FROM_DEVICE,
			   STp->device->request_queue->rq_timeout, 0, 1);
	if (SRpnt == NULL)
		return (STp->buffer)->syscall_result;

	st_release_request(SRpnt);

	return STp->buffer->syscall_result;
}


/* Send the mode page in the tape buffer to the drive. Assumes that the mode data
   in the buffer is correctly formatted. The long timeout is used if slow is non-zero. */
static int write_mode_page(struct scsi_tape *STp, int page, int slow)
{
	int pgo;
	unsigned char cmd[MAX_COMMAND_SIZE];
	struct st_request *SRpnt;
	int timeout;

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = MODE_SELECT;
	cmd[1] = MODE_SELECT_PAGE_FORMAT;
	pgo = MODE_HEADER_LENGTH + (STp->buffer)->b_data[MH_OFF_BDESCS_LENGTH];
	cmd[4] = pgo + (STp->buffer)->b_data[pgo + MP_OFF_PAGE_LENGTH] + 2;

	/* Clear reserved fields */
	(STp->buffer)->b_data[MH_OFF_DATA_LENGTH] = 0;
	(STp->buffer)->b_data[MH_OFF_MEDIUM_TYPE] = 0;
	(STp->buffer)->b_data[MH_OFF_DEV_SPECIFIC] &= ~MH_BIT_WP;
	(STp->buffer)->b_data[pgo + MP_OFF_PAGE_NBR] &= MP_MSK_PAGE_NBR;

	timeout = slow ?
		STp->long_timeout : STp->device->request_queue->rq_timeout;
	SRpnt = st_do_scsi(NULL, STp, cmd, cmd[4], DMA_TO_DEVICE,
			   timeout, 0, 1);
	if (SRpnt == NULL)
		return (STp->buffer)->syscall_result;

	st_release_request(SRpnt);

	return STp->buffer->syscall_result;
}


#define COMPRESSION_PAGE        0x0f
#define COMPRESSION_PAGE_LENGTH 16

#define CP_OFF_DCE_DCC          2
#define CP_OFF_C_ALGO           7

#define DCE_MASK  0x80
#define DCC_MASK  0x40
#define RED_MASK  0x60


/* Control the compression with mode page 15. Algorithm not changed if zero.

   The block descriptors are read and written because Sony SDT-7000 does not
   work without this (suggestion from Michael Schaefer <Michael.Schaefer@dlr.de>).
   Including block descriptors should not cause any harm to other drives. */

static int st_compression(struct scsi_tape * STp, int state)
{
	int retval;
	int mpoffs;  /* Offset to mode page start */
	unsigned char *b_data = (STp->buffer)->b_data;
	DEB( char *name = tape_name(STp); )

	if (STp->ready != ST_READY)
		return (-EIO);

	/* Read the current page contents */
	retval = read_mode_page(STp, COMPRESSION_PAGE, 0);
	if (retval) {
                DEBC(printk(ST_DEB_MSG "%s: Compression mode page not supported.\n",
                            name));
		return (-EIO);
	}

	mpoffs = MODE_HEADER_LENGTH + b_data[MH_OFF_BDESCS_LENGTH];
        DEBC(printk(ST_DEB_MSG "%s: Compression state is %d.\n", name,
                    (b_data[mpoffs + CP_OFF_DCE_DCC] & DCE_MASK ? 1 : 0)));

	/* Check if compression can be changed */
	if ((b_data[mpoffs + CP_OFF_DCE_DCC] & DCC_MASK) == 0) {
                DEBC(printk(ST_DEB_MSG "%s: Compression not supported.\n", name));
		return (-EIO);
	}

	/* Do the change */
	if (state) {
		b_data[mpoffs + CP_OFF_DCE_DCC] |= DCE_MASK;
		if (STp->c_algo != 0)
			b_data[mpoffs + CP_OFF_C_ALGO] = STp->c_algo;
	}
	else {
		b_data[mpoffs + CP_OFF_DCE_DCC] &= ~DCE_MASK;
		if (STp->c_algo != 0)
			b_data[mpoffs + CP_OFF_C_ALGO] = 0; /* no compression */
	}

	retval = write_mode_page(STp, COMPRESSION_PAGE, 0);
	if (retval) {
                DEBC(printk(ST_DEB_MSG "%s: Compression change failed.\n", name));
		return (-EIO);
	}
        DEBC(printk(ST_DEB_MSG "%s: Compression state changed to %d.\n",
		       name, state));

	STp->compression_changed = 1;
	return 0;
}


/* Process the load and unload commands (does unload if the load code is zero) */
static int do_load_unload(struct scsi_tape *STp, struct file *filp, int load_code)
{
	int retval = (-EIO), timeout;
	DEB( char *name = tape_name(STp); )
	unsigned char cmd[MAX_COMMAND_SIZE];
	struct st_partstat *STps;
	struct st_request *SRpnt;

	if (STp->ready != ST_READY && !load_code) {
		if (STp->ready == ST_NO_TAPE)
			return (-ENOMEDIUM);
		else
			return (-EIO);
	}

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = START_STOP;
	if (load_code)
		cmd[4] |= 1;
	/*
	 * If arg >= 1 && arg <= 6 Enhanced load/unload in HP C1553A
	 */
	if (load_code >= 1 + MT_ST_HPLOADER_OFFSET
	    && load_code <= 6 + MT_ST_HPLOADER_OFFSET) {
		DEBC(printk(ST_DEB_MSG "%s: Enhanced %sload slot %2d.\n",
			    name, (cmd[4]) ? "" : "un",
			    load_code - MT_ST_HPLOADER_OFFSET));
		cmd[3] = load_code - MT_ST_HPLOADER_OFFSET; /* MediaID field of C1553A */
	}
	if (STp->immediate) {
		cmd[1] = 1;	/* Don't wait for completion */
		timeout = STp->device->request_queue->rq_timeout;
	}
	else
		timeout = STp->long_timeout;

	DEBC(
		if (!load_code)
		printk(ST_DEB_MSG "%s: Unloading tape.\n", name);
		else
		printk(ST_DEB_MSG "%s: Loading tape.\n", name);
		);

	SRpnt = st_do_scsi(NULL, STp, cmd, 0, DMA_NONE,
			   timeout, MAX_RETRIES, 1);
	if (!SRpnt)
		return (STp->buffer)->syscall_result;

	retval = (STp->buffer)->syscall_result;
	st_release_request(SRpnt);

	if (!retval) {	/* SCSI command successful */

		if (!load_code) {
			STp->rew_at_close = 0;
			STp->ready = ST_NO_TAPE;
		}
		else {
			STp->rew_at_close = STp->autorew_dev;
			retval = check_tape(STp, filp);
			if (retval > 0)
				retval = 0;
		}
	}
	else {
		STps = &(STp->ps[STp->partition]);
		STps->drv_file = STps->drv_block = (-1);
	}

	return retval;
}

#if DEBUG
#define ST_DEB_FORWARD  0
#define ST_DEB_BACKWARD 1
static void deb_space_print(char *name, int direction, char *units, unsigned char *cmd)
{
	s32 sc;

	sc = cmd[2] & 0x80 ? 0xff000000 : 0;
	sc |= (cmd[2] << 16) | (cmd[3] << 8) | cmd[4];
	if (direction)
		sc = -sc;
	printk(ST_DEB_MSG "%s: Spacing tape %s over %d %s.\n", name,
	       direction ? "backward" : "forward", sc, units);
}
#endif


/* Internal ioctl function */
static int st_int_ioctl(struct scsi_tape *STp, unsigned int cmd_in, unsigned long arg)
{
	int timeout;
	long ltmp;
	int ioctl_result;
	int chg_eof = 1;
	unsigned char cmd[MAX_COMMAND_SIZE];
	struct st_request *SRpnt;
	struct st_partstat *STps;
	int fileno, blkno, at_sm, undone;
	int datalen = 0, direction = DMA_NONE;
	char *name = tape_name(STp);

	WARN_ON(STp->buffer->do_dio != 0);
	if (STp->ready != ST_READY) {
		if (STp->ready == ST_NO_TAPE)
			return (-ENOMEDIUM);
		else
			return (-EIO);
	}
	timeout = STp->long_timeout;
	STps = &(STp->ps[STp->partition]);
	fileno = STps->drv_file;
	blkno = STps->drv_block;
	at_sm = STps->at_sm;

	memset(cmd, 0, MAX_COMMAND_SIZE);
	switch (cmd_in) {
	case MTFSFM:
		chg_eof = 0;	/* Changed from the FSF after this */
	case MTFSF:
		cmd[0] = SPACE;
		cmd[1] = 0x01;	/* Space FileMarks */
		cmd[2] = (arg >> 16);
		cmd[3] = (arg >> 8);
		cmd[4] = arg;
                DEBC(deb_space_print(name, ST_DEB_FORWARD, "filemarks", cmd);)
		if (fileno >= 0)
			fileno += arg;
		blkno = 0;
		at_sm &= (arg == 0);
		break;
	case MTBSFM:
		chg_eof = 0;	/* Changed from the FSF after this */
	case MTBSF:
		cmd[0] = SPACE;
		cmd[1] = 0x01;	/* Space FileMarks */
		ltmp = (-arg);
		cmd[2] = (ltmp >> 16);
		cmd[3] = (ltmp >> 8);
		cmd[4] = ltmp;
                DEBC(deb_space_print(name, ST_DEB_BACKWARD, "filemarks", cmd);)
		if (fileno >= 0)
			fileno -= arg;
		blkno = (-1);	/* We can't know the block number */
		at_sm &= (arg == 0);
		break;
	case MTFSR:
		cmd[0] = SPACE;
		cmd[1] = 0x00;	/* Space Blocks */
		cmd[2] = (arg >> 16);
		cmd[3] = (arg >> 8);
		cmd[4] = arg;
                DEBC(deb_space_print(name, ST_DEB_FORWARD, "blocks", cmd);)
		if (blkno >= 0)
			blkno += arg;
		at_sm &= (arg == 0);
		break;
	case MTBSR:
		cmd[0] = SPACE;
		cmd[1] = 0x00;	/* Space Blocks */
		ltmp = (-arg);
		cmd[2] = (ltmp >> 16);
		cmd[3] = (ltmp >> 8);
		cmd[4] = ltmp;
                DEBC(deb_space_print(name, ST_DEB_BACKWARD, "blocks", cmd);)
		if (blkno >= 0)
			blkno -= arg;
		at_sm &= (arg == 0);
		break;
	case MTFSS:
		cmd[0] = SPACE;
		cmd[1] = 0x04;	/* Space Setmarks */
		cmd[2] = (arg >> 16);
		cmd[3] = (arg >> 8);
		cmd[4] = arg;
                DEBC(deb_space_print(name, ST_DEB_FORWARD, "setmarks", cmd);)
		if (arg != 0) {
			blkno = fileno = (-1);
			at_sm = 1;
		}
		break;
	case MTBSS:
		cmd[0] = SPACE;
		cmd[1] = 0x04;	/* Space Setmarks */
		ltmp = (-arg);
		cmd[2] = (ltmp >> 16);
		cmd[3] = (ltmp >> 8);
		cmd[4] = ltmp;
                DEBC(deb_space_print(name, ST_DEB_BACKWARD, "setmarks", cmd);)
		if (arg != 0) {
			blkno = fileno = (-1);
			at_sm = 1;
		}
		break;
	case MTWEOF:
	case MTWSM:
		if (STp->write_prot)
			return (-EACCES);
		cmd[0] = WRITE_FILEMARKS;
		if (cmd_in == MTWSM)
			cmd[1] = 2;
		cmd[2] = (arg >> 16);
		cmd[3] = (arg >> 8);
		cmd[4] = arg;
		timeout = STp->device->request_queue->rq_timeout;
                DEBC(
                     if (cmd_in == MTWEOF)
                               printk(ST_DEB_MSG "%s: Writing %d filemarks.\n", name,
				 cmd[2] * 65536 + cmd[3] * 256 + cmd[4]);
                     else
				printk(ST_DEB_MSG "%s: Writing %d setmarks.\n", name,
				 cmd[2] * 65536 + cmd[3] * 256 + cmd[4]);
		)
		if (fileno >= 0)
			fileno += arg;
		blkno = 0;
		at_sm = (cmd_in == MTWSM);
		break;
	case MTREW:
		cmd[0] = REZERO_UNIT;
		if (STp->immediate) {
			cmd[1] = 1;	/* Don't wait for completion */
			timeout = STp->device->request_queue->rq_timeout;
		}
                DEBC(printk(ST_DEB_MSG "%s: Rewinding tape.\n", name));
		fileno = blkno = at_sm = 0;
		break;
	case MTNOP:
                DEBC(printk(ST_DEB_MSG "%s: No op on tape.\n", name));
		return 0;	/* Should do something ? */
		break;
	case MTRETEN:
		cmd[0] = START_STOP;
		if (STp->immediate) {
			cmd[1] = 1;	/* Don't wait for completion */
			timeout = STp->device->request_queue->rq_timeout;
		}
		cmd[4] = 3;
                DEBC(printk(ST_DEB_MSG "%s: Retensioning tape.\n", name));
		fileno = blkno = at_sm = 0;
		break;
	case MTEOM:
		if (!STp->fast_mteom) {
			/* space to the end of tape */
			ioctl_result = st_int_ioctl(STp, MTFSF, 0x7fffff);
			fileno = STps->drv_file;
			if (STps->eof >= ST_EOD_1)
				return 0;
			/* The next lines would hide the number of spaced FileMarks
			   That's why I inserted the previous lines. I had no luck
			   with detecting EOM with FSF, so we go now to EOM.
			   Joerg Weule */
		} else
			fileno = (-1);
		cmd[0] = SPACE;
		cmd[1] = 3;
                DEBC(printk(ST_DEB_MSG "%s: Spacing to end of recorded medium.\n",
                            name));
		blkno = -1;
		at_sm = 0;
		break;
	case MTERASE:
		if (STp->write_prot)
			return (-EACCES);
		cmd[0] = ERASE;
		cmd[1] = (arg ? 1 : 0);	/* Long erase with non-zero argument */
		if (STp->immediate) {
			cmd[1] |= 2;	/* Don't wait for completion */
			timeout = STp->device->request_queue->rq_timeout;
		}
		else
			timeout = STp->long_timeout * 8;

                DEBC(printk(ST_DEB_MSG "%s: Erasing tape.\n", name));
		fileno = blkno = at_sm = 0;
		break;
	case MTSETBLK:		/* Set block length */
	case MTSETDENSITY:	/* Set tape density */
	case MTSETDRVBUFFER:	/* Set drive buffering */
	case SET_DENS_AND_BLK:	/* Set density and block size */
		chg_eof = 0;
		if (STp->dirty || (STp->buffer)->buffer_bytes != 0)
			return (-EIO);	/* Not allowed if data in buffer */
		if ((cmd_in == MTSETBLK || cmd_in == SET_DENS_AND_BLK) &&
		    (arg & MT_ST_BLKSIZE_MASK) != 0 &&
		    STp->max_block > 0 &&
		    ((arg & MT_ST_BLKSIZE_MASK) < STp->min_block ||
		     (arg & MT_ST_BLKSIZE_MASK) > STp->max_block)) {
			printk(KERN_WARNING "%s: Illegal block size.\n", name);
			return (-EINVAL);
		}
		cmd[0] = MODE_SELECT;
		if ((STp->use_pf & USE_PF))
			cmd[1] = MODE_SELECT_PAGE_FORMAT;
		cmd[4] = datalen = 12;
		direction = DMA_TO_DEVICE;

		memset((STp->buffer)->b_data, 0, 12);
		if (cmd_in == MTSETDRVBUFFER)
			(STp->buffer)->b_data[2] = (arg & 7) << 4;
		else
			(STp->buffer)->b_data[2] =
			    STp->drv_buffer << 4;
		(STp->buffer)->b_data[3] = 8;	/* block descriptor length */
		if (cmd_in == MTSETDENSITY) {
			(STp->buffer)->b_data[4] = arg;
			STp->density_changed = 1;	/* At least we tried ;-) */
		} else if (cmd_in == SET_DENS_AND_BLK)
			(STp->buffer)->b_data[4] = arg >> 24;
		else
			(STp->buffer)->b_data[4] = STp->density;
		if (cmd_in == MTSETBLK || cmd_in == SET_DENS_AND_BLK) {
			ltmp = arg & MT_ST_BLKSIZE_MASK;
			if (cmd_in == MTSETBLK)
				STp->blksize_changed = 1; /* At least we tried ;-) */
		} else
			ltmp = STp->block_size;
		(STp->buffer)->b_data[9] = (ltmp >> 16);
		(STp->buffer)->b_data[10] = (ltmp >> 8);
		(STp->buffer)->b_data[11] = ltmp;
		timeout = STp->device->request_queue->rq_timeout;
                DEBC(
			if (cmd_in == MTSETBLK || cmd_in == SET_DENS_AND_BLK)
				printk(ST_DEB_MSG
                                       "%s: Setting block size to %d bytes.\n", name,
				       (STp->buffer)->b_data[9] * 65536 +
				       (STp->buffer)->b_data[10] * 256 +
				       (STp->buffer)->b_data[11]);
			if (cmd_in == MTSETDENSITY || cmd_in == SET_DENS_AND_BLK)
				printk(ST_DEB_MSG
                                       "%s: Setting density code to %x.\n", name,
				       (STp->buffer)->b_data[4]);
			if (cmd_in == MTSETDRVBUFFER)
				printk(ST_DEB_MSG
                                       "%s: Setting drive buffer code to %d.\n", name,
				    ((STp->buffer)->b_data[2] >> 4) & 7);
		)
		break;
	default:
		return (-ENOSYS);
	}

	SRpnt = st_do_scsi(NULL, STp, cmd, datalen, direction,
			   timeout, MAX_RETRIES, 1);
	if (!SRpnt)
		return (STp->buffer)->syscall_result;

	ioctl_result = (STp->buffer)->syscall_result;

	if (!ioctl_result) {	/* SCSI command successful */
		st_release_request(SRpnt);
		SRpnt = NULL;
		STps->drv_block = blkno;
		STps->drv_file = fileno;
		STps->at_sm = at_sm;

		if (cmd_in == MTBSFM)
			ioctl_result = st_int_ioctl(STp, MTFSF, 1);
		else if (cmd_in == MTFSFM)
			ioctl_result = st_int_ioctl(STp, MTBSF, 1);

		if (cmd_in == MTSETBLK || cmd_in == SET_DENS_AND_BLK) {
			STp->block_size = arg & MT_ST_BLKSIZE_MASK;
			if (STp->block_size != 0) {
				(STp->buffer)->buffer_blocks =
				    (STp->buffer)->buffer_size / STp->block_size;
			}
			(STp->buffer)->buffer_bytes = (STp->buffer)->read_pointer = 0;
			if (cmd_in == SET_DENS_AND_BLK)
				STp->density = arg >> MT_ST_DENSITY_SHIFT;
		} else if (cmd_in == MTSETDRVBUFFER)
			STp->drv_buffer = (arg & 7);
		else if (cmd_in == MTSETDENSITY)
			STp->density = arg;

		if (cmd_in == MTEOM)
			STps->eof = ST_EOD;
		else if (cmd_in == MTFSF)
			STps->eof = ST_FM;
		else if (chg_eof)
			STps->eof = ST_NOEOF;

		if (cmd_in == MTWEOF)
			STps->rw = ST_IDLE;
	} else { /* SCSI command was not completely successful. Don't return
                    from this block without releasing the SCSI command block! */
		struct st_cmdstatus *cmdstatp = &STp->buffer->cmdstat;

		if (cmdstatp->flags & SENSE_EOM) {
			if (cmd_in != MTBSF && cmd_in != MTBSFM &&
			    cmd_in != MTBSR && cmd_in != MTBSS)
				STps->eof = ST_EOM_OK;
			STps->drv_block = 0;
		}

		if (cmdstatp->remainder_valid)
			undone = (int)cmdstatp->uremainder64;
		else
			undone = 0;

		if (cmd_in == MTWEOF &&
		    cmdstatp->have_sense &&
		    (cmdstatp->flags & SENSE_EOM)) {
			if (cmdstatp->sense_hdr.sense_key == NO_SENSE ||
			    cmdstatp->sense_hdr.sense_key == RECOVERED_ERROR) {
				ioctl_result = 0;	/* EOF(s) written successfully at EOM */
				STps->eof = ST_NOEOF;
			} else {  /* Writing EOF(s) failed */
				if (fileno >= 0)
					fileno -= undone;
				if (undone < arg)
					STps->eof = ST_NOEOF;
			}
			STps->drv_file = fileno;
		} else if ((cmd_in == MTFSF) || (cmd_in == MTFSFM)) {
			if (fileno >= 0)
				STps->drv_file = fileno - undone;
			else
				STps->drv_file = fileno;
			STps->drv_block = -1;
			STps->eof = ST_NOEOF;
		} else if ((cmd_in == MTBSF) || (cmd_in == MTBSFM)) {
			if (arg > 0 && undone < 0)  /* Some drives get this wrong */
				undone = (-undone);
			if (STps->drv_file >= 0)
				STps->drv_file = fileno + undone;
			STps->drv_block = 0;
			STps->eof = ST_NOEOF;
		} else if (cmd_in == MTFSR) {
			if (cmdstatp->flags & SENSE_FMK) {	/* Hit filemark */
				if (STps->drv_file >= 0)
					STps->drv_file++;
				STps->drv_block = 0;
				STps->eof = ST_FM;
			} else {
				if (blkno >= undone)
					STps->drv_block = blkno - undone;
				else
					STps->drv_block = (-1);
				STps->eof = ST_NOEOF;
			}
		} else if (cmd_in == MTBSR) {
			if (cmdstatp->flags & SENSE_FMK) {	/* Hit filemark */
				STps->drv_file--;
				STps->drv_block = (-1);
			} else {
				if (arg > 0 && undone < 0)  /* Some drives get this wrong */
					undone = (-undone);
				if (STps->drv_block >= 0)
					STps->drv_block = blkno + undone;
			}
			STps->eof = ST_NOEOF;
		} else if (cmd_in == MTEOM) {
			STps->drv_file = (-1);
			STps->drv_block = (-1);
			STps->eof = ST_EOD;
		} else if (cmd_in == MTSETBLK ||
			   cmd_in == MTSETDENSITY ||
			   cmd_in == MTSETDRVBUFFER ||
			   cmd_in == SET_DENS_AND_BLK) {
			if (cmdstatp->sense_hdr.sense_key == ILLEGAL_REQUEST &&
			    !(STp->use_pf & PF_TESTED)) {
				/* Try the other possible state of Page Format if not
				   already tried */
				STp->use_pf = (STp->use_pf ^ USE_PF) | PF_TESTED;
				st_release_request(SRpnt);
				SRpnt = NULL;
				return st_int_ioctl(STp, cmd_in, arg);
			}
		} else if (chg_eof)
			STps->eof = ST_NOEOF;

		if (cmdstatp->sense_hdr.sense_key == BLANK_CHECK)
			STps->eof = ST_EOD;

		st_release_request(SRpnt);
		SRpnt = NULL;
	}

	return ioctl_result;
}


/* Get the tape position. If bt == 2, arg points into a kernel space mt_loc
   structure. */

static int get_location(struct scsi_tape *STp, unsigned int *block, int *partition,
			int logical)
{
	int result;
	unsigned char scmd[MAX_COMMAND_SIZE];
	struct st_request *SRpnt;
	DEB( char *name = tape_name(STp); )

	if (STp->ready != ST_READY)
		return (-EIO);

	memset(scmd, 0, MAX_COMMAND_SIZE);
	if ((STp->device)->scsi_level < SCSI_2) {
		scmd[0] = QFA_REQUEST_BLOCK;
		scmd[4] = 3;
	} else {
		scmd[0] = READ_POSITION;
		if (!logical && !STp->scsi2_logical)
			scmd[1] = 1;
	}
	SRpnt = st_do_scsi(NULL, STp, scmd, 20, DMA_FROM_DEVICE,
			   STp->device->request_queue->rq_timeout,
			   MAX_READY_RETRIES, 1);
	if (!SRpnt)
		return (STp->buffer)->syscall_result;

	if ((STp->buffer)->syscall_result != 0 ||
	    (STp->device->scsi_level >= SCSI_2 &&
	     ((STp->buffer)->b_data[0] & 4) != 0)) {
		*block = *partition = 0;
                DEBC(printk(ST_DEB_MSG "%s: Can't read tape position.\n", name));
		result = (-EIO);
	} else {
		result = 0;
		if ((STp->device)->scsi_level < SCSI_2) {
			*block = ((STp->buffer)->b_data[0] << 16)
			    + ((STp->buffer)->b_data[1] << 8)
			    + (STp->buffer)->b_data[2];
			*partition = 0;
		} else {
			*block = ((STp->buffer)->b_data[4] << 24)
			    + ((STp->buffer)->b_data[5] << 16)
			    + ((STp->buffer)->b_data[6] << 8)
			    + (STp->buffer)->b_data[7];
			*partition = (STp->buffer)->b_data[1];
			if (((STp->buffer)->b_data[0] & 0x80) &&
			    (STp->buffer)->b_data[1] == 0)	/* BOP of partition 0 */
				STp->ps[0].drv_block = STp->ps[0].drv_file = 0;
		}
                DEBC(printk(ST_DEB_MSG "%s: Got tape pos. blk %d part %d.\n", name,
                            *block, *partition));
	}
	st_release_request(SRpnt);
	SRpnt = NULL;

	return result;
}


/* Set the tape block and partition. Negative partition means that only the
   block should be set in vendor specific way. */
static int set_location(struct scsi_tape *STp, unsigned int block, int partition,
			int logical)
{
	struct st_partstat *STps;
	int result, p;
	unsigned int blk;
	int timeout;
	unsigned char scmd[MAX_COMMAND_SIZE];
	struct st_request *SRpnt;
	DEB( char *name = tape_name(STp); )

	if (STp->ready != ST_READY)
		return (-EIO);
	timeout = STp->long_timeout;
	STps = &(STp->ps[STp->partition]);

        DEBC(printk(ST_DEB_MSG "%s: Setting block to %d and partition to %d.\n",
                    name, block, partition));
	DEB(if (partition < 0)
		return (-EIO); )

	/* Update the location at the partition we are leaving */
	if ((!STp->can_partitions && partition != 0) ||
	    partition >= ST_NBR_PARTITIONS)
		return (-EINVAL);
	if (partition != STp->partition) {
		if (get_location(STp, &blk, &p, 1))
			STps->last_block_valid = 0;
		else {
			STps->last_block_valid = 1;
			STps->last_block_visited = blk;
                        DEBC(printk(ST_DEB_MSG
                                    "%s: Visited block %d for partition %d saved.\n",
                                    name, blk, STp->partition));
		}
	}

	memset(scmd, 0, MAX_COMMAND_SIZE);
	if ((STp->device)->scsi_level < SCSI_2) {
		scmd[0] = QFA_SEEK_BLOCK;
		scmd[2] = (block >> 16);
		scmd[3] = (block >> 8);
		scmd[4] = block;
		scmd[5] = 0;
	} else {
		scmd[0] = SEEK_10;
		scmd[3] = (block >> 24);
		scmd[4] = (block >> 16);
		scmd[5] = (block >> 8);
		scmd[6] = block;
		if (!logical && !STp->scsi2_logical)
			scmd[1] = 4;
		if (STp->partition != partition) {
			scmd[1] |= 2;
			scmd[8] = partition;
                        DEBC(printk(ST_DEB_MSG
                                    "%s: Trying to change partition from %d to %d\n",
                                    name, STp->partition, partition));
		}
	}
	if (STp->immediate) {
		scmd[1] |= 1;		/* Don't wait for completion */
		timeout = STp->device->request_queue->rq_timeout;
	}

	SRpnt = st_do_scsi(NULL, STp, scmd, 0, DMA_NONE,
			   timeout, MAX_READY_RETRIES, 1);
	if (!SRpnt)
		return (STp->buffer)->syscall_result;

	STps->drv_block = STps->drv_file = (-1);
	STps->eof = ST_NOEOF;
	if ((STp->buffer)->syscall_result != 0) {
		result = (-EIO);
		if (STp->can_partitions &&
		    (STp->device)->scsi_level >= SCSI_2 &&
		    (p = find_partition(STp)) >= 0)
			STp->partition = p;
	} else {
		if (STp->can_partitions) {
			STp->partition = partition;
			STps = &(STp->ps[partition]);
			if (!STps->last_block_valid ||
			    STps->last_block_visited != block) {
				STps->at_sm = 0;
				STps->rw = ST_IDLE;
			}
		} else
			STps->at_sm = 0;
		if (block == 0)
			STps->drv_block = STps->drv_file = 0;
		result = 0;
	}

	st_release_request(SRpnt);
	SRpnt = NULL;

	return result;
}


/* Find the current partition number for the drive status. Called from open and
   returns either partition number of negative error code. */
static int find_partition(struct scsi_tape *STp)
{
	int i, partition;
	unsigned int block;

	if ((i = get_location(STp, &block, &partition, 1)) < 0)
		return i;
	if (partition >= ST_NBR_PARTITIONS)
		return (-EIO);
	return partition;
}


/* Change the partition if necessary */
static int switch_partition(struct scsi_tape *STp)
{
	struct st_partstat *STps;

	if (STp->partition == STp->new_partition)
		return 0;
	STps = &(STp->ps[STp->new_partition]);
	if (!STps->last_block_valid)
		STps->last_block_visited = 0;
	return set_location(STp, STps->last_block_visited, STp->new_partition, 1);
}

/* Functions for reading and writing the medium partition mode page. */

#define PART_PAGE   0x11
#define PART_PAGE_FIXED_LENGTH 8

#define PP_OFF_MAX_ADD_PARTS   2
#define PP_OFF_NBR_ADD_PARTS   3
#define PP_OFF_FLAGS           4
#define PP_OFF_PART_UNITS      6
#define PP_OFF_RESERVED        7

#define PP_BIT_IDP             0x20
#define PP_MSK_PSUM_MB         0x10

/* Get the number of partitions on the tape. As a side effect reads the
   mode page into the tape buffer. */
static int nbr_partitions(struct scsi_tape *STp)
{
	int result;
	DEB( char *name = tape_name(STp); )

	if (STp->ready != ST_READY)
		return (-EIO);

	result = read_mode_page(STp, PART_PAGE, 1);

	if (result) {
                DEBC(printk(ST_DEB_MSG "%s: Can't read medium partition page.\n",
                            name));
		result = (-EIO);
	} else {
		result = (STp->buffer)->b_data[MODE_HEADER_LENGTH +
					      PP_OFF_NBR_ADD_PARTS] + 1;
                DEBC(printk(ST_DEB_MSG "%s: Number of partitions %d.\n", name, result));
	}

	return result;
}


/* Partition the tape into two partitions if size > 0 or one partition if
   size == 0.

   The block descriptors are read and written because Sony SDT-7000 does not
   work without this (suggestion from Michael Schaefer <Michael.Schaefer@dlr.de>).

   My HP C1533A drive returns only one partition size field. This is used to
   set the size of partition 1. There is no size field for the default partition.
   Michael Schaefer's Sony SDT-7000 returns two descriptors and the second is
   used to set the size of partition 1 (this is what the SCSI-3 standard specifies).
   The following algorithm is used to accommodate both drives: if the number of
   partition size fields is greater than the maximum number of additional partitions
   in the mode page, the second field is used. Otherwise the first field is used.

   For Seagate DDS drives the page length must be 8 when no partitions is defined
   and 10 when 1 partition is defined (information from Eric Lee Green). This is
   is acceptable also to some other old drives and enforced if the first partition
   size field is used for the first additional partition size.
 */
static int partition_tape(struct scsi_tape *STp, int size)
{
	char *name = tape_name(STp);
	int result;
	int pgo, psd_cnt, psdo;
	unsigned char *bp;

	result = read_mode_page(STp, PART_PAGE, 0);
	if (result) {
		DEBC(printk(ST_DEB_MSG "%s: Can't read partition mode page.\n", name));
		return result;
	}
	/* The mode page is in the buffer. Let's modify it and write it. */
	bp = (STp->buffer)->b_data;
	pgo = MODE_HEADER_LENGTH + bp[MH_OFF_BDESCS_LENGTH];
	DEBC(printk(ST_DEB_MSG "%s: Partition page length is %d bytes.\n",
		    name, bp[pgo + MP_OFF_PAGE_LENGTH] + 2));

	psd_cnt = (bp[pgo + MP_OFF_PAGE_LENGTH] + 2 - PART_PAGE_FIXED_LENGTH) / 2;
	psdo = pgo + PART_PAGE_FIXED_LENGTH;
	if (psd_cnt > bp[pgo + PP_OFF_MAX_ADD_PARTS]) {
		bp[psdo] = bp[psdo + 1] = 0xff;  /* Rest of the tape */
		psdo += 2;
	}
	memset(bp + psdo, 0, bp[pgo + PP_OFF_NBR_ADD_PARTS] * 2);

	DEBC(printk("%s: psd_cnt %d, max.parts %d, nbr_parts %d\n", name,
		    psd_cnt, bp[pgo + PP_OFF_MAX_ADD_PARTS],
		    bp[pgo + PP_OFF_NBR_ADD_PARTS]));

	if (size <= 0) {
		bp[pgo + PP_OFF_NBR_ADD_PARTS] = 0;
		if (psd_cnt <= bp[pgo + PP_OFF_MAX_ADD_PARTS])
		    bp[pgo + MP_OFF_PAGE_LENGTH] = 6;
                DEBC(printk(ST_DEB_MSG "%s: Formatting tape with one partition.\n",
                            name));
	} else {
		bp[psdo] = (size >> 8) & 0xff;
		bp[psdo + 1] = size & 0xff;
		bp[pgo + 3] = 1;
		if (bp[pgo + MP_OFF_PAGE_LENGTH] < 8)
		    bp[pgo + MP_OFF_PAGE_LENGTH] = 8;
                DEBC(printk(ST_DEB_MSG
                            "%s: Formatting tape with two partitions (1 = %d MB).\n",
                            name, size));
	}
	bp[pgo + PP_OFF_PART_UNITS] = 0;
	bp[pgo + PP_OFF_RESERVED] = 0;
	bp[pgo + PP_OFF_FLAGS] = PP_BIT_IDP | PP_MSK_PSUM_MB;

	result = write_mode_page(STp, PART_PAGE, 1);
	if (result) {
		printk(KERN_INFO "%s: Partitioning of tape failed.\n", name);
		result = (-EIO);
	}

	return result;
}



/* The ioctl command */
static long st_ioctl(struct file *file, unsigned int cmd_in, unsigned long arg)
{
	int i, cmd_nr, cmd_type, bt;
	int retval = 0;
	unsigned int blk;
	struct scsi_tape *STp = file->private_data;
	struct st_modedef *STm;
	struct st_partstat *STps;
	char *name = tape_name(STp);
	void __user *p = (void __user *)arg;

	if (mutex_lock_interruptible(&STp->lock))
		return -ERESTARTSYS;

        DEB(
	if (debugging && !STp->in_use) {
		printk(ST_DEB_MSG "%s: Incorrect device.\n", name);
		retval = (-EIO);
		goto out;
	} ) /* end DEB */

	STm = &(STp->modes[STp->current_mode]);
	STps = &(STp->ps[STp->partition]);

	/*
	 * If we are in the middle of error recovery, don't let anyone
	 * else try and use this device.  Also, if error recovery fails, it
	 * may try and take the device offline, in which case all further
	 * access to the device is prohibited.
	 */
	retval = scsi_nonblockable_ioctl(STp->device, cmd_in, p,
					file->f_flags & O_NDELAY);
	if (!scsi_block_when_processing_errors(STp->device) || retval != -ENODEV)
		goto out;
	retval = 0;

	cmd_type = _IOC_TYPE(cmd_in);
	cmd_nr = _IOC_NR(cmd_in);

	if (cmd_type == _IOC_TYPE(MTIOCTOP) && cmd_nr == _IOC_NR(MTIOCTOP)) {
		struct mtop mtc;

		if (_IOC_SIZE(cmd_in) != sizeof(mtc)) {
			retval = (-EINVAL);
			goto out;
		}

		i = copy_from_user(&mtc, p, sizeof(struct mtop));
		if (i) {
			retval = (-EFAULT);
			goto out;
		}

		if (mtc.mt_op == MTSETDRVBUFFER && !capable(CAP_SYS_ADMIN)) {
			printk(KERN_WARNING
                               "%s: MTSETDRVBUFFER only allowed for root.\n", name);
			retval = (-EPERM);
			goto out;
		}
		if (!STm->defined &&
		    (mtc.mt_op != MTSETDRVBUFFER &&
		     (mtc.mt_count & MT_ST_OPTIONS) == 0)) {
			retval = (-ENXIO);
			goto out;
		}

		if (!STp->pos_unknown) {

			if (STps->eof == ST_FM_HIT) {
				if (mtc.mt_op == MTFSF || mtc.mt_op == MTFSFM ||
                                    mtc.mt_op == MTEOM) {
					mtc.mt_count -= 1;
					if (STps->drv_file >= 0)
						STps->drv_file += 1;
				} else if (mtc.mt_op == MTBSF || mtc.mt_op == MTBSFM) {
					mtc.mt_count += 1;
					if (STps->drv_file >= 0)
						STps->drv_file += 1;
				}
			}

			if (mtc.mt_op == MTSEEK) {
				/* Old position must be restored if partition will be
                                   changed */
				i = !STp->can_partitions ||
				    (STp->new_partition != STp->partition);
			} else {
				i = mtc.mt_op == MTREW || mtc.mt_op == MTOFFL ||
				    mtc.mt_op == MTRETEN || mtc.mt_op == MTEOM ||
				    mtc.mt_op == MTLOCK || mtc.mt_op == MTLOAD ||
				    mtc.mt_op == MTFSF || mtc.mt_op == MTFSFM ||
				    mtc.mt_op == MTBSF || mtc.mt_op == MTBSFM ||
				    mtc.mt_op == MTCOMPRESSION;
			}
			i = flush_buffer(STp, i);
			if (i < 0) {
				retval = i;
				goto out;
			}
			if (STps->rw == ST_WRITING &&
			    (mtc.mt_op == MTREW || mtc.mt_op == MTOFFL ||
			     mtc.mt_op == MTSEEK ||
			     mtc.mt_op == MTBSF || mtc.mt_op == MTBSFM)) {
				i = st_int_ioctl(STp, MTWEOF, 1);
				if (i < 0) {
					retval = i;
					goto out;
				}
				if (mtc.mt_op == MTBSF || mtc.mt_op == MTBSFM)
					mtc.mt_count++;
				STps->rw = ST_IDLE;
			     }

		} else {
			/*
			 * If there was a bus reset, block further access
			 * to this device.  If the user wants to rewind the tape,
			 * then reset the flag and allow access again.
			 */
			if (mtc.mt_op != MTREW &&
			    mtc.mt_op != MTOFFL &&
			    mtc.mt_op != MTRETEN &&
			    mtc.mt_op != MTERASE &&
			    mtc.mt_op != MTSEEK &&
			    mtc.mt_op != MTEOM) {
				retval = (-EIO);
				goto out;
			}
			reset_state(STp);
			/* remove this when the midlevel properly clears was_reset */
			STp->device->was_reset = 0;
		}

		if (mtc.mt_op != MTNOP && mtc.mt_op != MTSETBLK &&
		    mtc.mt_op != MTSETDENSITY && mtc.mt_op != MTWSM &&
		    mtc.mt_op != MTSETDRVBUFFER && mtc.mt_op != MTSETPART)
			STps->rw = ST_IDLE;	/* Prevent automatic WEOF and fsf */

		if (mtc.mt_op == MTOFFL && STp->door_locked != ST_UNLOCKED)
			do_door_lock(STp, 0);	/* Ignore result! */

		if (mtc.mt_op == MTSETDRVBUFFER &&
		    (mtc.mt_count & MT_ST_OPTIONS) != 0) {
			retval = st_set_options(STp, mtc.mt_count);
			goto out;
		}

		if (mtc.mt_op == MTSETPART) {
			if (!STp->can_partitions ||
			    mtc.mt_count < 0 || mtc.mt_count >= ST_NBR_PARTITIONS) {
				retval = (-EINVAL);
				goto out;
			}
			if (mtc.mt_count >= STp->nbr_partitions &&
			    (STp->nbr_partitions = nbr_partitions(STp)) < 0) {
				retval = (-EIO);
				goto out;
			}
			if (mtc.mt_count >= STp->nbr_partitions) {
				retval = (-EINVAL);
				goto out;
			}
			STp->new_partition = mtc.mt_count;
			retval = 0;
			goto out;
		}

		if (mtc.mt_op == MTMKPART) {
			if (!STp->can_partitions) {
				retval = (-EINVAL);
				goto out;
			}
			if ((i = st_int_ioctl(STp, MTREW, 0)) < 0 ||
			    (i = partition_tape(STp, mtc.mt_count)) < 0) {
				retval = i;
				goto out;
			}
			for (i = 0; i < ST_NBR_PARTITIONS; i++) {
				STp->ps[i].rw = ST_IDLE;
				STp->ps[i].at_sm = 0;
				STp->ps[i].last_block_valid = 0;
			}
			STp->partition = STp->new_partition = 0;
			STp->nbr_partitions = 1;	/* Bad guess ?-) */
			STps->drv_block = STps->drv_file = 0;
			retval = 0;
			goto out;
		}

		if (mtc.mt_op == MTSEEK) {
			i = set_location(STp, mtc.mt_count, STp->new_partition, 0);
			if (!STp->can_partitions)
				STp->ps[0].rw = ST_IDLE;
			retval = i;
			goto out;
		}

		if (mtc.mt_op == MTUNLOAD || mtc.mt_op == MTOFFL) {
			retval = do_load_unload(STp, file, 0);
			goto out;
		}

		if (mtc.mt_op == MTLOAD) {
			retval = do_load_unload(STp, file, max(1, mtc.mt_count));
			goto out;
		}

		if (mtc.mt_op == MTLOCK || mtc.mt_op == MTUNLOCK) {
			retval = do_door_lock(STp, (mtc.mt_op == MTLOCK));
			goto out;
		}

		if (STp->can_partitions && STp->ready == ST_READY &&
		    (i = switch_partition(STp)) < 0) {
			retval = i;
			goto out;
		}

		if (mtc.mt_op == MTCOMPRESSION)
			retval = st_compression(STp, (mtc.mt_count & 1));
		else
			retval = st_int_ioctl(STp, mtc.mt_op, mtc.mt_count);
		goto out;
	}
	if (!STm->defined) {
		retval = (-ENXIO);
		goto out;
	}

	if ((i = flush_buffer(STp, 0)) < 0) {
		retval = i;
		goto out;
	}
	if (STp->can_partitions &&
	    (i = switch_partition(STp)) < 0) {
		retval = i;
		goto out;
	}

	if (cmd_type == _IOC_TYPE(MTIOCGET) && cmd_nr == _IOC_NR(MTIOCGET)) {
		struct mtget mt_status;

		if (_IOC_SIZE(cmd_in) != sizeof(struct mtget)) {
			 retval = (-EINVAL);
			 goto out;
		}

		mt_status.mt_type = STp->tape_type;
		mt_status.mt_dsreg =
		    ((STp->block_size << MT_ST_BLKSIZE_SHIFT) & MT_ST_BLKSIZE_MASK) |
		    ((STp->density << MT_ST_DENSITY_SHIFT) & MT_ST_DENSITY_MASK);
		mt_status.mt_blkno = STps->drv_block;
		mt_status.mt_fileno = STps->drv_file;
		if (STp->block_size != 0) {
			if (STps->rw == ST_WRITING)
				mt_status.mt_blkno +=
				    (STp->buffer)->buffer_bytes / STp->block_size;
			else if (STps->rw == ST_READING)
				mt_status.mt_blkno -=
                                        ((STp->buffer)->buffer_bytes +
                                         STp->block_size - 1) / STp->block_size;
		}

		mt_status.mt_gstat = 0;
		if (STp->drv_write_prot)
			mt_status.mt_gstat |= GMT_WR_PROT(0xffffffff);
		if (mt_status.mt_blkno == 0) {
			if (mt_status.mt_fileno == 0)
				mt_status.mt_gstat |= GMT_BOT(0xffffffff);
			else
				mt_status.mt_gstat |= GMT_EOF(0xffffffff);
		}
		mt_status.mt_erreg = (STp->recover_reg << MT_ST_SOFTERR_SHIFT);
		mt_status.mt_resid = STp->partition;
		if (STps->eof == ST_EOM_OK || STps->eof == ST_EOM_ERROR)
			mt_status.mt_gstat |= GMT_EOT(0xffffffff);
		else if (STps->eof >= ST_EOM_OK)
			mt_status.mt_gstat |= GMT_EOD(0xffffffff);
		if (STp->density == 1)
			mt_status.mt_gstat |= GMT_D_800(0xffffffff);
		else if (STp->density == 2)
			mt_status.mt_gstat |= GMT_D_1600(0xffffffff);
		else if (STp->density == 3)
			mt_status.mt_gstat |= GMT_D_6250(0xffffffff);
		if (STp->ready == ST_READY)
			mt_status.mt_gstat |= GMT_ONLINE(0xffffffff);
		if (STp->ready == ST_NO_TAPE)
			mt_status.mt_gstat |= GMT_DR_OPEN(0xffffffff);
		if (STps->at_sm)
			mt_status.mt_gstat |= GMT_SM(0xffffffff);
		if (STm->do_async_writes ||
                    (STm->do_buffer_writes && STp->block_size != 0) ||
		    STp->drv_buffer != 0)
			mt_status.mt_gstat |= GMT_IM_REP_EN(0xffffffff);
		if (STp->cleaning_req)
			mt_status.mt_gstat |= GMT_CLN(0xffffffff);

		i = copy_to_user(p, &mt_status, sizeof(struct mtget));
		if (i) {
			retval = (-EFAULT);
			goto out;
		}

		STp->recover_reg = 0;		/* Clear after read */
		retval = 0;
		goto out;
	}			/* End of MTIOCGET */
	if (cmd_type == _IOC_TYPE(MTIOCPOS) && cmd_nr == _IOC_NR(MTIOCPOS)) {
		struct mtpos mt_pos;
		if (_IOC_SIZE(cmd_in) != sizeof(struct mtpos)) {
			 retval = (-EINVAL);
			 goto out;
		}
		if ((i = get_location(STp, &blk, &bt, 0)) < 0) {
			retval = i;
			goto out;
		}
		mt_pos.mt_blkno = blk;
		i = copy_to_user(p, &mt_pos, sizeof(struct mtpos));
		if (i)
			retval = (-EFAULT);
		goto out;
	}
	mutex_unlock(&STp->lock);
	switch (cmd_in) {
		case SCSI_IOCTL_GET_IDLUN:
		case SCSI_IOCTL_GET_BUS_NUMBER:
			break;
		default:
			if ((cmd_in == SG_IO ||
			     cmd_in == SCSI_IOCTL_SEND_COMMAND ||
			     cmd_in == CDROM_SEND_PACKET) &&
			    !capable(CAP_SYS_RAWIO))
				i = -EPERM;
			else
				i = scsi_cmd_ioctl(STp->disk->queue, STp->disk,
						   file->f_mode, cmd_in, p);
			if (i != -ENOTTY)
				return i;er fbreak;
	}
or Lval = scsi_ioctl(STp->device, cmd_in, p);

   S!newer. &&ile Doc == SCSI_IOCTL_STOP_UNIT) { /* unload */
		panyirew_at_close = 0versfrom Dwady = ST_NO_TAPE1 and newinuxnewer.;

 out:
	mutex_  Reck(&panyim sentat.
   Contributi}

#ifdef CONFIG_COMPAT
static long st_compate accomstruct file *c Le, unsigned intile n, Wolfgangfriedarg)
{
	ky, Erie thetape *STp =ic Le->private_data;as Koppenh"ofeng
    *sdevver bnying
   tatintontr  Tape matioCMDtation/aefe->host1992 -t->ugene Exarevtory

for L Seeht 1992 - 2008 Kai Makisara
  ight file , (void __user *) And;
isara.
   Contrlpha#endif



/* Try to allocate a new r, Mibuffer. Calling function must not hold
   dev_arr_m se.ittes Ehren Koppenht_o.au>  *newer, M<linux/(nd Eneed_dma,g Denmax_sndreas Koppenh <linux/motb
   tb = kzch <r(sizeofsky, Eril.h>
#inc), GFP_ATOMICntation/stb) {
		printk(KERN_NOTICE "st: Can'toch <rgoocatnf.csiro.au> D\n"ntatncludingNULL1 and tb->frp_segsth's SCnux/use_sg =nclude clude <dma =linux/fs.clude <o.au> _#incth's Slude <reserved_page>
#iched.h>
clude  * #include <linue <l *),er fo nclulude <linux/init.h>
#.h>
#include <liincludkfree(tbx/mtio.h>
#include <lincludingde <}
 Richard Gooch <rgoocenough space in thef.csiro.au> itte#definer byMAX_ORDER 6
1215";

nd Eenlarg

#includ#include <linux/mo STo.au> h>
#indule#incclude <sux/fs.dreasnd Em.h>, nbr,ncludecsi/sbsi/scsiordinclgot;
	gfp_t priorityinlo   S<scsi/sc <er bo.au> include <linuver .
   Co1scsi/scsnclude <scsi/scsi_ioc>
#iPAGE_SIZEver normaliz

#includnclude <); y:
 Ages -extra<scsmentitte
	river.h>ver blude <sclinux/;
	nbrioctl.h>on-z-ro. */
#defcdrom.h>tation/ 0

<= 0tl.h>
#inclpinlonclude < =de <lx/stEL | __ludeNOWARNfor the ce.h>
#inude <y set |to KERDMAde <scsi/sg.h>


/clearedy see the messag   so ZEROde <scsi/sg.h>


/cdrom.h>includ<scsizero. */
#defmapl Leb.e <l_<scsi/mtiinclud prints some << <scsi1 an elsencludfor (T_DEB_MSG  KERN_NOe <scsith's SCSncluding) {< <asm/system.h&&#includ < <scsi/sc; }
#else
#def++
#includ *= 2ver fconsolemptyitten}si/scsf DEBUG
/* (  KERN_NOTICE
#def))
#endif

#includ   Sing) { er bym/system.ver fges is curebugging information on the cl.h>
#inclcsi/scsi.h>
#ing.h>
#incl<scsi/scsian easily#includdefinm.h>
#iidely classified, /scsir_dev;

staticlude <linu;

#elseBUG
/<if DEBUG
/&&class
#endif

#dincludux/moduleparae <linlo	lepar=och <rde <li(e the mee <scsiio = ion/se <linclud	DEBi/sg.h>


/* The driver=scsiio = RECT_IO;
static int try_rdio = _io = TRY_DIRclud	idely classified,  +=lude		lass+=#includ SCSI _MAJOR);
MODULE_ALIAS_SCS* of sysfs par>
#include <li[BUG
] =ulepaam'sBUG
++1 and  sysfs paral Leb* Sysfs_addressi/sg.h>


/sn't yet suppor0];
stanged to KERN_DEBUG. */
#defi =E
#definincluding1smp_lock.Make sure thatic  ly lafrom previous eb, 9is
#includinternalsm/dma.h>
#s Ehrenges -ing mi.h>
#include <scsi/scsi_dbl.h>pinclude <ir_kbdefini=0; i <ESC(maage level f i++y sememset(r.
 */
modulef scattsn't yet suppori]), 0.h>
nclud G  KERN_NOTICEf scattERN_DEBUG. */
#defi;
stf scatting mes paramp_lock.Releas sizeDEBUG
 amed(max_sg_segs, max_ugging informatioclude <scsi/scsi_dbg.h>
#inx_sg_segs,gging) { aanged to KERN_DEBUG. */
#defineMaximum th's ber oThe message level f segmnclud__ncluENSE("G_param_named(buffer_kbs, i]DULE_ALIAS_C sysfs parameters (whi-=tatic int buffer_kbs;s defined expliccdrom.h>
#inclu sysfs parsg;

#ifndef MODULE
stat 0);
MODULE_PARM_DESC(te_threshold_kbs;  /* roffsi.Mak0ult driverove fixed blocbuffeb, 9m/dma.htonclude <asm/dma.. R
   Cs zero (success) orcharnegative error code20081215";

nd Eappend_to
#includconst char- aeb, 95ubp,
#include <linux/mo 0);
Mh>
#indo_countm_named(try_cnt, res, struct, and ElengthMSG  KERN_NOTICE0);
MODULE_PARM_DESC(try_dDESC(try_rdio,,
	{
		" See scattclude <byte fory_direr of scatter/gathe");
struct >=rect_ior segmentstruct -error "Masi/scsitatif scatter/gathe) {	/* Should never hatibiitten e <linux/strat pING
#inclatibility with 2ES > 16
overflowlinux/mtio.h>
#i(-EIO i/o whdefinmber of scatter/gathe");

	{
		"m >o, "TpossibleCSI tape (st) driso that natry_wdio, try_wdiam'scn so ect_io"-ES > 16
<"m", "v", ?en moved to st_op:"m", "v",o = 1;-zercopy_ bloaeb, se (256)");
moV_MAJO+ES > 16, hold_cnSI_DEV   Sresrect_io = TR(-EFAULTio = m", "v", -=ze s parahat names for all fini-byte Subpcommands. *struct st_de#include
	{
		"may:
 to get same names for sam] = {
	"",  "r"including_dev_parm {
	char *name;
	inte <asm/dma.hms[] __ *val;
} pa{
	{
		"buffer_kbs", &buffer_kbs
	},
	{       /* Retained for compFER_Bs, int, 0);
MODULE_PARM_DESC(ma,		"write_threshold_
	},
	{
		"max_sg_segs", NULL
	},
	{
		"try_direct_io", &try_direct_io
	}
};
#endif

/* Restrict the number of modes so that nae dr_poe_parare assigned */
#if ST_NBR_MODES > 16
#error "Maximum number of modes is 16"
#endif
/* Bit reversed order to get same names for same minors with all
   mode#define NO_c const char *st_formats[] = {
	"",  "r", "k", "s", "l", "t", "o", "u",
	"m", "v", "p", "x", "a", "y", "q", "z"}; 

/* The default definitions have been moved to st_options.h */

#define ST_FIXED_BUFFER_SIZE (ST_FIXED_BUFtoBLOCKShold_ * ST_KILOBYTE)

/* The buffee should fit into the 24 bits for length in the
   6-byte SCSI read and write c  6-byte SCSI rea >> (ST_NBR_commands. */
#if ST_FIXED_BUFFER_SIZE >= (2 << 24 - 1)
#error "Buffer size should not exceed (2 << 24 - 1) bytes!"
#endif

stattowards start ofd tape drive (1)");

/*mov

#inclul LebTAPE  NOT_READY

#define STnd Estructinclude <srcEBUG, dstct st_trucstruct st_t_buff	{
		"try_dir
		"m, totbutiy_direct_io", &try_direct_io
	}
};
#endif

/* Restrict thg_segtruct sg messages isinlocct s=ts) */
#define SET_DE
	{
		"try int sruct snumb, int);r of scatter/gather , int);x", "a", int, int);
s	{
		"tryopyrigint, int);<en movever fon 1.1 aumber of modes is 16cludCSI read and write c) & ~255) >> (ST_NBR_ =ruct st_bdefinbuffer =ic void clnumber(co "p", , "a", "y", "q", "zd"}; 

/* The default definitibuffer ns haux/moduleparas"}; 

/* The default definiti, int);]ver") "v", = min(n moved tic void cl,en moved toid move_bio = mem NULse (256)");
mot sgl/* Tst_remove(sm' (e (256)");
modles(voidoid move_b,ffer(sio = oid move_buomma_SIZE (Sic void move_bu#error "MJOR(SCSst_bufferversiar __user *, ument) 		ic void cls_files(struct sc{
	.owner		int, int);

statbuffer ct scsi{
	.owner		template =  long,  6-_SIZE (}/O b/* Validgoocbuffop */
sd bloccommand lude or module parametersax_sg_segs, max_vc int s_pressionges includf (;

statkbs "p"y sest_fixedL;

statDEB_MSGt switch_padbg._KILOBYTakislude "st.c int w>er byFIRST_SGtion(stscsi_tape *,octl.h>
evel fohabetincal
MODULEstatSett_combootmpressio. Syntax2)")ncludeng D Documena */
/e th/st.txt.
tained for comp__ininux/msetup(	"wri*stam_named(try_len int s[5ns ht_ref_mup<< 2stel Lgetcsi_tape str, ARRAY somede <s)e "oss << 2   SCosst0]artit) a
#definrdio, "Try  "SC-", &&igned SIGS_FROM_parms)r segmenteam",	{"On[i].valver fo* "", "osst"} =am", "i + 1ns h DEB(a) a
#wh Lee( SIG!=incluJOR(SCS"}, \
	{"OnStrea"osst"}, \
	{"OnStream"OR(SCS	lenes(strlen, "", "ossnameerm' (CHARDEstrncmp "FW )

truct scsi_nclud) &&h>
#inclu(* "FW-+mutex)== ':' || k(&st_dev_arr_loc=')_tape_geDP-", "", "osst"}, \
	{	{"OnStream", "USSTp = s	simple_strtouvskyt_dev_a "os,inclu, 0tape *S	EB(aSTp = se minors with all
   modeObsolet

static in %s\n".h>
#i, try_dir	struct scsi_tape *Sruct st_buate = ef_put   SCS>= struct scsi_tape ver foinclinors with all
   modeinstrucce_get(STp-in '%s'evice))
		stentatk(&stnt devchrs fp, ','_ref_mpyrigtpver fostpct scs}atic istruct scsi_tape  << 24 - 1)  I/O b_MUTEX(s"st="d_kbMUTEX( << -JAN-199s Ehren4 */
	ky, Eric Le_operbj, sc st_fops =reas.own*, c	THIS_se(str,
	.e dr =on(ste drice_write =n(stnlockice_rom seede accoes(str acco,betical
   order) Klau	.ugene Exareves(strugene Exarev,mutex_lo	.os fodev);
* Naice_flush(&st_rct drice_pu user &st_rnknown ,
};

#include <sst_probes for tehael Schdevdreas Koppenh"ofehael SchSDex);to_-", "", NUL(	/* edinsky,
 gJAN-sk * }};
, "", edinsky,
 cefer*ature the device signh"ofer, Mictpce *)nclude e <linux/mmodecal
*STm   function rparts Ehichae for#include <linux/moo.au> t_buffei, j, inte,r *venum,      etect.h"
#ifndefscsi/Dp->type-", TYPEai Matl.h>
#inclape DEVstruct 
	krerejectnugene ible)
{
)x && scisar_= NULL;
out:INFO, SDp, "Founng D rp->vendoebugginux/mti		if (!strncmp(r
#inclThuffeggested drime nis %slinud_kbt_ref_*rp;

	for (rp=&(cludrdio;
stqueuened lhwEBUGs des)
{
	srequest_ SDp-m.h>
 SDp->rev,phystrlen(rp->rev))) {
			if (rp-ntation/igned long);

s< iy serdioigned long);

;
	from thinuxle.h>

#includ)
{
	s92 -)->unche
strucsa/fs.h>
ntation/tic inli, "", "osst"= NULL;
out:ERRo, try_dir#include <linux/errno.h>
#include <l Dael Scc coattachedlinux/mtigoton antrncmp(/* If tE_LICE }};(1ntation/s }};me;
}


static void s_senseouct scmemoryuct st_cmdstatus *s)
{
	const u8 *ucp;L;

statnclutrncmp(nlockr = "(&st"", erstr = "se
				retunr"", f, sf (s->hmax, "a", "y", "h"ofer, Mic*tmN_DEstatint et_se	s->rever")fld(sense, statics->deferred* 2, 8hould fitSENSE_BUFFER>ic int ti MaSver fSENSE_BUFFERSI 0x7f) {
		c;
		switch (sense[0]<= 0;
eferre_tape_gags = rom sevef (s->have_sense) {_sense = scsi_normalizeToo manyebugging
   sde "s. %d)rlen(breatry_dir 0x7f) {
		ca	breaIZE, &s->put SRpnment) to et_sennux/blkdev.SENSE_BUFFERlude <linux/moduh"ofer, Micinclude <linux/in	switch (sa>disk_name;
}
	s->flags = sense[2] & 0xe0;
			break;
		case 0x73:
			s-ude <lextendst[] = {arraylinux/mtii_sense_desc_find(sense, SC			re"ofer, Ms-", "", "osst"}memcpy	}
	}
}, (ucp[3] &sixed_format 0;
		s->res->flags = ucp ? (ucp[3] & 0x= scsiincludRpnt->resu= scse = Rpnt->resul=_fld(saver")tape_name(Statp = ense, SCtatic int rdio, "Try 0;
		s->rer segment = SRpnt->resu[i]>disk_nam(struct st_b->kref, s0;
		s->remr");
nic("-", "", NULs corrupt	ret)ux/m
	rives, ched.h>
#include <linux"ofer, Minclude <linux/init.hrives,, "", "osst"s->flags = sense[2] & 0xe0;
			bre int st_chk_result(struct sch <rgoochael Scdescripto<linux/mti_sense_desc_find(sen}
	kref DEFI(&rive->cmd[= scnt->cm/* If tind(sens= NULf( }};>cmd[5__ref_m"st%d"eturn t->haveer, Eyal Leb = pnt->cmp->modint_sense SDp-defeev))) {
			if (rp SRpnt->cm->mode= nse[templat/
	iave_sense)
		sc riveint_mpatibUSB"DEB(
   ying
   );
	} r->cmds	} )      levelixedclude>have_.h>

truct= MT_ISinforam'STp->de: Error %x (driver bt 0x%x,2atp->have_tic inlin) */
stat driver_byte->last_SRives, the
  			     DEFIirectncludnt->cmdret toave_sense inaeb,->have_sense &rv<linux/mparader ard o.au> suppiffor st_i senseittensense restr/fs. =KERN_WAct scsi_tape *tape)
{
	r
			     linupf != BLANKRNING
			   unsnfor2, SRpnt->cmensset toave_sense &o_autor = "deferrAUTO_LOCK
			     can_bsERED			 SRpnt->cmd[0]  2 ? 1 : = 0IN_FILE_POS);y:
 BSRferrdatoryk);
 MOD3TION && */
sensame iTp->kr->cmd[0] != two_fcmdsST_TWO_FM
			     fte(rmteoNSE_STAFAc inTEO  /* Only     2_logic "USB int resLOGICAdeviTp->clnilrdio intILIode != NO_mmedigoocer by KWAITENSE &&
			fault_drvRECOVERED0xff;	der No forced
			 /* scoION && */
STp->cln_ode >= EXTENDe ch_req |= ((SRpnt->sense[br STp->cln_mode >= blkif (rp_rq_timeoutG "%sying
   ))) {
			if (rp,_STARIMEOUlengtsense frie}
	if (c				  LONGtatp->se>= EXTENDEry_dio tap17)
	recect_-", !BLANK_CHEK &&
			 scode != VOLSC(try_rdio, "Try di_NBRcsi_ES, "x", "a",STcmds&mdstatpst_i_dire0);
MOm_sens conth's SCSI m->syser, J"_SYSp=&(dstatp->he_mas_forref_mumode >= dstatp-o_asyncCOVERED_WR) {
SYNC_WRIT
		casATAL
	  ;

statRpnt->cmd[0]BUFFERRITE_6
	    && SRpn >> (ahut(sdd[0]READ_AHEA  CoED_ERROR
#if v;
	codul= ((SRST_DONT_TOUCH	STp->recover_regblkDEB_MSG(-1)ERRORalue);
		e024

#ten frrecover_reg		 SRpnt->pnt->cmd[0] == READ		 SRpnth"
#inrequested */

	STp->pos_uPARTITIONwn |= STp->devcsi_->was_resp
	if (cmdstps->rw      IDLakisvered %eo &&
 by KEOF.\n", nameat_scmdss SCSI ts_byte(rbm se_&st_decount);
		} ) != RT_READYpnt->(cmdstatp->flc Lee 0)
			restp t &&
	 urren returode >= EXTENDet;

	0].->have_senude <E &&
			 SRpn_change_sent &&
	 +;

      )
{
	strucre ass driver			if )
{
	structIZE deas f], SRpnt->cmsense)  int eferrect scs->flags = sense[2] & 0xe0;
			bsigned  st_reque UNIT_Tp->pos_unknown ++st_iSTp->device->was_reset;

	st_if (cmddefinjnumbj < 2; jsi_tape_gis on tatur_ed.h>


static v!atur_tape_ge

static void st_a_analyze_sen%dze_sense(SRpnt->sense,
				SCSI_SENSE_BU.h>
#includar *venumtape *SIZE, &s->;
modr, M	breaf_putatur->release ;
	scsi_dev	breatatic vk(ST_Def, scsver")	      sult = -dd(aturce))
		 MKDEV( MODEi Ma_MAJOR, req->eINORFROMatiblest_incj)m.h>
#i	 			ret>cmds     ->buffer->syscall_resul
	}

	reude <lidd %s-rewin = Nde %dse_request(struct st_re, j ? "non" : "T_UN"_tape tape *Sstat.midlevel_result = static void st_release_rct st_request *streq)
{
	kfree(streq);
}

sp = "aturs[jr taaturver")e = uptodate
	{
ret scclass
	retsG "%sncompatible_len;

	i	STp->buffrequest * SRp	kfree(streqic in++)
		if (!strncing.h>p->veno, try_"Aus *s)
     ebuggi>devicebuggse)
	G "%scmdstp++)
		if (!strncmp(rp->vendo%s:p->c eaning i/o: %s (aligns def%d B)se_requ kzaltries)
{
	stru,alloc(sx17)
		S? "yes= reqnoa;
	int e SDp->dma_->buffer->rev))) {
			if (rp- "os << 24 - 1) byt

			   int da: Can't get numbequest.\n",
			    ta>err= STp->device->was_reset;

	nding(currsysfs_re NULLlinkRpnt->cm->have_sctionLL, ev.kobj.h>
#inc"rr =ux/mtient))
			stp->buffer->syscascsi/suest(req->q_tape_gec inis on p,
	_rq_map_user SRpntis on the listRNELng
   _d		 soyretuQ_TYPEute(sce))
		goto outt *SRpnt = req->end_io
	if (!Sstrucdata;
	strui_tape *STp tape *Slt = deompa_rq_map_userreq);
}

strucq, req, men,
md_len = ->cmd	else {
	 = sense[2] & 0xe0;
			brave_sense)
ct scsir tathe
   fup = stp--	else {
		DEBC(printk(KERN_ERR "%s:e_desc_find(:
	sc_find(p->ha);
	includestrumeout;sense_hdr);:	req->entry_rdio  and i*rp;

	for (rp=&trucct st_reject_da_BLOCKject_list[] = {
	/* {"XXX", "Yy-", "", NULL},  example */
	SIGS_FROM_OSST,
	{NUompatible drivetatic char * st_is: Cags = 0;

	if (s->have_sense) {nse(SRpnt, cmdstatp);

	if (cmdsncludrives, ave_sense)
		reak;
		}ives", "", -", >have_sense)
	;
	} ;

statinditions for taGFP_KERNEnse;
	req->sen the result to success code */
staticQ_TYPE_BLOCK_PC;
	req->cmd_flags |= REQ_QUIET;

	mdata-->null_mapped Can't get SCSI request.\n",
			    tape_name(stp)d = 1;

	if (bufflen) {
		errL);
		if (err) {
			blk_put_request(req);
			return DRIVER_ERROR << 24;
		}
	}

	SRpnt->bio = req->bio;
	req->c0, BLK_MAX;

	req->cmd_type.ND_SIZE(cmd[0]0x%x).\nive.\n",
		       ta   GFP_KERNELf_put(&de;
	eas f = sense[md[3fer)-mdstatpmd[3puSRpnt->cmd[4]ar *stp;)
	

staticcmd[0]deas from sevel_result = (-EINTR4th argument) s->flags = gs = sense[2] & 0xe0;
			br4 - 1) bytes!/**
 *ame = ter)->syscall_res -evfs edEBUGnclut_comS theT, Miky, Erureurn NULL;@);
	: ffer *, to embedded );
	
 turn NULL;
_result = (
statibe held e_parsuppthis routine.  BecaNSE it isurn NULL;c set lon yte( put, you so get alway(KB; t_com
	}

	/* Iget()urn NULL;
	}

	/* Ielse)omplpint whichferripuormat_compemaphoreata = &lyurn NULL;csi_same ndo aata = &S);
		else)ape,x_sg_segs, max_
	}

	/* If asyncs for te);
	 *d[4],reas Koppenh"ofer, Micrives, mple */
rr =(d[4], SRT,
	{NULL, }};

/* If tpnt->cmd[5atp->have_sense)
		
		else _MSG "%sver_bytetatic ugging informatioserved_pages;int eq->end_io    host_bde <linux/delay;
	}

	memcpy(SRpnt->c;
static_sense(name, SRpnt->snclude ies = retries;
	req->end_io_daend_to_bu}st_scsi_execuc DEFINDEFI_st *);
static <scrct thscsi_tape *STp)
{
	stror)) &&
		    !strncmpVer     %s, ruct 
			024

%d, s/gOR("Kars;
	STp-verefine(struct scsi_tape *) */
	tic void screq->stpblk_put_req, rete(stsi_exe(;
	scsi_devi 	else
ll_mappedc inISid s
			blk_put_reqx && scstat.midlevel_resUna strsi_exe Q_TYPfer)->ue);fixederr =deviio = 1;
statPTR		wait_for_completiota_direerdateregister_chrct st;
}on(t *SRpnt = req->end_io_0m.h>
#includ 0x7f) {
		_ENTRIES		 __do_wait) ers;
		STing);
		SRpnt->waiting =to_FRO major>maper)->syscall_resud bufflemplet = req->end_int->cmd[2]errut_req
	return SRpn
	}

t;
}


/*bugginretur Abnorma.LL, rB);
	 write hwrite successHandls: Cn SRpn_scsi_execQ_TYPEruct sck mode. */
static int w
	}
drbehinges is curr0;
	struct :d_len);unes are used only in
   variable block nt write_b:
	rtstat *STp Handle the write-behind checking (waits for cetion). Returns -ENmdstatp;te(s:
	er)->l(err) {
			blk_put_req
	ret = stbytes,csi_executges - aexi scs direction, NUL) {
	 NULLcsi_tape * STp)
st_partstat *STps;
	struct st_cmdstatus *cmdst st_request *SRpnt;

	STbuffer = STp->buffer;
	if (!STbuffer->writing)
		return  DEB(
	if (STp->write_pending)
	p;

	if (!result)
		or)) &&
		    !strncmpU Rewr)
{
	cons}

int);

	if (d, dire);>buffer_r_fi(r_finis%s: Richacomp	(STpp->model_parface{
	{ad-only aef *);mos define215";

#of(*sct_da->cleaning_re_showject_list[] = sed onl *dd ST_TIMEn) *r_ent.
   Cosnmdstatpbuf,) if (debugg"rs;
	Sp->cleaning_reffer-s EhrenDRIVER_ATTR(->cleaning_re, S_IRUGO */
	== 0)
			STps->drvp->kre%s:  (STp->block_size ruct scsi_tape *)->drv_block++;
		else
			STps->drv_block += STbuffer->writing / STp->block_size;
	}

	c&& !cmdstatp->deferrbuffer->cmdstat;
	if (ST	(STp->buffer)->sys_result) {
	!cmdstatp->deferred &&if (cmdstatp->have_sense && tic void sced &&
		    (cmdstatp->flags & SENSE_EOM) &&
		    (cmdstatp->sense_hdr.sense_key == NO_SEl_result = (-Efer->cmdstat;
	if (STtic void scRROR)) {
			/*/
			if (!cmdstaif (cmdstatp->have_sense &&  lartreq>drv_block++;
		else
			STps-ddrv_block += STbuffer->writing / STp->block_size;[%s]len(r largel == -EIO)
			STps->drv_
	    pRROR)) {
			/*
	    printkif (cmdstatp->hav
	},
	{
ruct scsi_tape * S*);
statiblock++;
		else
			STpSTp->p) { /* Abnormable blot_buffeytes, td_check->modver EOF c Le(STp->, &ber). *attr		retval = -EIck mode. */
sta	STp->nbr_waock number). */
static int cross_eof(struct scNSE ||
		     cmdstamode. */
static int w->cleaning_rest_request *SRpnt;
	unsigned char cmd[MAX_COMMANl_result = (-Emode. */
static int wOMMAND_SIZE];
st_request *SRpnt;
	unsigned char cmd[MAX_COMMAN
	    p = 1;
	} else
		cmd[2] = cmd[tl.h>
#i_buffer *STbuffer Stepping o:
	ber). *  ) /* d) {
		cmd[2] = cmd[3] = 0;
		cmd[4] = 2] = cmd[3] = cmd[_name(STp), forward ? "forward" : "backwarNSE ||
		     cmdsta0x01;		/* Space F_name(STp), forward ? "forward" : "backwarsi_tape * STp, in	STp->nbr_waits++;
	else
		     ) /* end DEB */

en inadvertently crossed (ioctl not used because
   it messename(STp), forward ? "forward" : "backwar    DEBC(prme(STp), forward ? "forward" : "backward"));

	SRpnt Tp, cmd, 0, DMA_NONE,
			   STp->device->request_queue->rq_tETRIES, 1);
	if (!SRpnt)
		return (STp->buffer)->syscallp_lock.hps[STp->pTp) gop->buffn]);
	if 		if (STp->block_s
f (s-have_rintk(ST_DEB_MSG "%{
	/*d_kbs", &;
		elsct sibute *ct sdrv_block += STbfunction returns a poiheck REQ_ask) ic inROM_OSSTlock_siar *pinloar *rriting / STp->block_size;
	}

	cstatp->have_g)
		STp->nalphabDEVICEif (STTp)
{
	RROR)) {
			/*Tp)
{
	int tif (cmdstatp->have_sense * STp)sk) nt transfer, blks;
	int result;
	unsigned char cmd[MAX_COMMAND_SIZE];
	struct st_request *SRpnt;
	struct st_partstat *STps;

	result = write_behind_check(STp);
	if (result)
		returng) {
			if sult;

	result = 0;
	if (STp->ding) {
			if == 1) {

		transf        >buffer->buffer_bytes;
        pe *stp)nt transfer, blks;
	int result;
	unsigned char cmd[MAX_COMMAND_SIZE];
	struct st_request *SRpnt;
	struct st_partstat *STps;

	result ect.h"
fmts: Cammdstat= "read";
			else i>= 0ecti0x%02x\ = reqrs;
	;= write_behind_check(STp);
	if fmt
		cmd[1] = 1;
		 SRpn= transfer / STp->block_size;
		cmd[2		 SRpn== 1) {

		transf = st_do_scs>buffer->buffer_bytes;
        quest *streqnt transfer, blks;
	int result;
	unsigned char cmd[MAX_o, try_dir                              tape_name(STp), transfer));

		memset(cmd, 0, MAX_COMMAND_SIZE);
		cmd[0] = WRITE_6;
		cmd[1] = 1;
quest *stre -->stp;ansfer / STp->block_size;
		cmd[2quest *stre== 1) {

		transf>deferred &&
			>buffer->buffer_bytes;
     pressioo_scsi(NULL, STp, cmd, transfer, DMA_TO_DEVICE,
				   STp->device->request_queue->rq_timeout,
				   MAX_WRITE_RETRIES,Koppenh"ofer, Michaetatic char * pressiot *STps;

	result = ximum number of snished. */
static ent))
			stp->->pos_unknown fer-p;

	STp&tape * STp, u);
		if ing(p->buen,
		uct st_buc in1);
				result =STp->buffer->set(reqndif
/* e_hdr.sense4 - 1) byonsol &STp->same names for s MODlist[t_request *
st
	pressionp->buffepnt->cmd[0] != W?r btTE_FILEMARKS
#en :P_KERpression|
	return r  && SRpnt->cFlush th != WRITE_6
. The tape will be positio
		STp->recFlush thcount++;
	. The tI_TAmpression|nt;
buggsuppFlush thDEBUGGl
  : 0 h thape will be p
	stD_SENSFlush thRT) { . The tape will be patteixed formaFlush thse */
		if. The tape will be positR
#if ST_RECOVERED_ace, resulFek_next is true. */
static , "Tense(naFlush thCAN_
	ifif (STp->pos_unknown)
omi {
		limo this devNO_BLKLIM
	if (STp->pos_unknown)
		reSTp->cln_mo (-EIO);

	istp = "iocSTp->ready != ST_READY)n_sense_value)Flush thTp->cleaning. The tape will be posi scod

	if (STYSVSTp->ready != ST_READY)mode] &
			STps = &(   S
		return st_flush_writeSRpn

	if (STILISTp->r= write_behind_check(STp);
	if (STps8= &(         sult;

	result = 0;
	if (STp-pressioRROR)) {
			/*bytes = 0;
	n retval;
}


/* Step over EOF ute(struct s "%s: Error on flush.HZ)
#det scsi_t
#inclen;_named(try_rewle(struct scsi_csi_[10
st_ect_list[] = {
rev;te(stmis oct the numrewnumbrewp->bufrew
static iver buffer size fk >= inor nu (!re = 0;espond/
	ifs[] __four
	int firsM_HIT)s>wait;
	rnin_compame 0);	t fin
	retuequest< (4/* Thpos_unkno_BIT = scse_behind__ref_m10p_datace)"lt = sidua= reqfixed_fJ"org >have_sense)
			ef, srma "",e = REQt */
			if (!r];
	if;
		elsast_SRp			blk_put_requ eral pd_flags |= REQ_QUIErequest(strueturn DRIVER_ERROR << 24;
		}

	SRpnt->bio ct scsi_tape *Srewp = SRpnin
   eral pq->cmd_typep_dat", 0);	onst unsi{
		wait_f*/
			if (!rx && scs= NULL;
out:
	write_unl
	re;
		STps->drv failes;
	STp->t(struct st_request uptodateresult(STp,_it = 0;
	uns_request * SRment) to c int st_
		STps->drvard ? "ity >= 0 &&
	  parameter&t = -t scturn result
	STp->buffeu8 *ucp;
	cy != STp->density) {
		arg = STm->default_density;
		set_it = 1;
	= 1;
		blks = tr	arg = STp->density;
	arg <<= MT_ST_DENSITY_SHIFT;
	if (!STp->blksize_changed &&
	    STm->deflt != 0) {
	arg = STp->density;
	arg <<= MT_ST_DENSITY_SHIFT;
	if (!STp->blksize_changed &&
	    STm->defquest *stre;
	} else
		arg |= STp->block_size;
	if (set_it &&
	    st_int_ioctl(STp, SET_DENS_AND_BLK1) /
	    S	arg = STp->density;
	me, STm st_relt;
&&->eofint a ret;
uptodateblk_puti_execPC;
	rJ"org Weuleuct completion *waiting;
	 uest */
			if (!r-> Lock or unloctruct rq_map	STp->buffer->cmdstat.midlevel_rt = -EBUSY;
	}

	reude <l NULL;
		(STpPC;
d bloc>sysc>densiase_request(struct st_request *streq)
{md[0]);
	memse>request_queue, req->q, NU(struct

stat
stafollowsupport
 */
s mayO cousefuld, -Ea i/scsr audienf (Sained for compsgl_ERN_eb, ENSE("Gx/kernel.h>
#incluSTbed busity_ch4 */
	 Wolfgang Dennt)
	 <li   Steve Hirsch,u/
moixed_formatlock_sifer(strnd Erw {
		 Steve Hirsch,_tap= (door_ +? ST_L +G  KERN_NOTatp- >>rints sHIF.asc Steve Hirsch,struct=>door__LOCK_FAILS;
	}
	4 */
	ude < "L" :  =ED;
	-;
}


, and Eris,char ack over tlepara) drirrect driverqo_locRpnt-*mRpnt->sescsi_kbs;  /* ze, /* Ub, 9at Abnatp-Ohar *st!
				rejecelse {
		STp-;
stdoor_ata *rp;

	foINVng_r = 0;>defbiTp->c in
   ode (et */
sta>);
	if (!rata *rp;

	for MEM->rw = Hmm?_PARTITIOevice *g messages is currerejece <linux/mlkdev.h>
#e <linlude <lin
	int includeN_NOTI)rr_loe = STp-= 0;
		STps->lastSTps->eoichard Goom->dek);
allt scbuffne, &bary*STp)ult &&downlush_(&c struc->mmTp->arom.eque);
		if (STrw==coun means put(s blocber)., nlock(into(SRpnt- area
				T_FIXEtruck ? "L" : ", 0, strucatp-ion = STp->atp-door_locket */
statp-s er= counatp-0, (STdoe <le);
	or same       0 (cmdst	up_partition = STp->partition;
	} = 0;E    s STp->o*STp)
matibd = 0;
		.
   Coher#defin fit inULE_ "L" : 
static i&s->unmafndeSTps->eoximum number oi_tape *, "x", "aSTps->eo);
		if (STFIXME: ct drisupar *suffer)-t if the,;
	int retval = C * ta rably wrriedort
 */

0;
	unsiITE_6char cmd[MAX_COMMAN && ct dr_dc *s)
	if s->drvile >=etval = lock*STps->struct stSet th& ~ints MAScsi_ax_waitetained for compatib->pos_plude <linux	int i;uremainde max_waiton anwait)
:y(struct s"", "osst"}, \
			stp->rait,-EIO);
	r.
 *statp ges;
	} p->bufE(cmd[0T_FIXEIZE >= includzeof(SRpn.
   Contcsi_tastatAnd ait)
Tp->m...\n", name,
		    doait)
ck ? "L" : "Unl"));
	retval = scsi_iocte after Wolfgang DenEADY      Step irtiessasg_segs, "Maximum number o max_wait, scode;", "y", "q", "z"}; 

/aits=0; ; ) {
		meRpnt = ULE,
			scode t_reetPageD&
		TE)

/ame,HKRES_READstatpDY, newmisssuppo;
	unsigned
	in_ATTENTIONSllTp->p					uct eferdoor? ST_Lsupport
 */
ntinufine CHKcsi(SRpnt, STp, cmd i/o whincludaits=0; ; ) {
		meffer_kbs=0; ; ) {
		memse
		else 4 - 1) bytes