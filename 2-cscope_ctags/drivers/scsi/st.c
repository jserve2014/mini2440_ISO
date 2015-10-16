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
		panyirew_at_close = 0versfrom Dwady = ST_NO_TAPE1 and newinuxcsi/sttatiout:
	mutex_  Reck(&from m sentat.tionContributi}

#ifdef CONFIG_COMPAT
static long st_compate accomstruct ft fo*c Le, unsigned intt fon, Wolfgangfriedarg)
{
	ky, Erie thetape *STp =iGree->private_data;as Koppenh"ofengtion *sdevver bnyihael S Ehrntg (i  T, MimatioCMD Ehron/aefe->host1992 -t->ugene Exarevtory

f new Seeht  2008  2008 Kai Makisara
  ighric Lee, (void __user *) And;
ome scluding (ilpha#endif



/* Try to alloce Exaa.
  r, Mibuff/st.Calling funcopyr must not holdtiondev_arr_eopl.ittes Ehrennsky,
  t_o.au>  *csi/s, M<l  Co/(nd Eneed_dma,g Denmax_sndredinsky,
   
#inclumotbeule,b = kzch <r(sizeofs Koppenl.h>
#inc), GFP_ATOMICe inpyristb) {en frintk(KERN_NOTICE "st: Can'toed.h>go<rgonf.csirlinux/D\n"e inncludingNULLkisaratb->frp_segsth's SCncluuse_sg =io.h>e tl.h>
<dma =#inclufs.#includlinux/_h>
#
#inclincludreserved_pagem.h>ched/mm.tl.h>
 * h>
#includ#incux/m *),ersio  <lilinux/modux/init/mm.h/mm.h>
#linux/mo<linuxkfree(tbx/mtiolude <linux/delaio.h>
#iclud}
 Richard Golinux/errnenough space in"ofe>
#include <0081#defin, J"yMAX_ORDER 6
1215";

e <lenlargabetlinux.h>
#inlinux/cdemo STlinux/mm.h>
dule.h>
inux/des/fcntl<linue <lm.h>, nbr,linux/csi/sbh>
#csiordh>
#got;
	gfp_t priorityinloion/<ude /sc <, J"m/dma.h <linux/modur, Jcluding1_host.hslinux/dede <scsihe acm.h>PAGE_SIZEr, Jnormalizsi.h>
#inlinux/de); y:
 Ages -extra

/*ment0081
	river.h>r, J"g.h>


/#inclu;
	nbr acco.h>on-z-ro.itteinclcdroscsi/init.h 0

<= 0f DEBi.h>
#pscsilinux/de =<scsix/stEL | __e <sNOWARN Kaithe cesages i <scsy set |to KERDMAh>


/* Thgsage

/clearedhe mh"ofe messag   so ZEROer when the debuggi level scsi/s

/* ze The messagmapl Leb.ux/m_

/* Tmtay.h>
#inclnts some <<


/* kisa elselinux Kai(T_DEB_MSG es. trin>


/* 
#incluSio.h>
#i) {< <asm/system.h&&.h>
#in CE
#def/sc; }
#EB(aessag++i.h>
#in *= 2r, Jfconsolempty0081n} <scsif DEBUGRich( if (debug.h>essag))
-JAN-199E 1024

  S
#defi e <asB(a)
#deflude  if is curebuggsuppinfggin */

oinclu cx/mm.h>
#l/* The dsages ie deb.h>
#

/* The de DEasily.h>
#inncludscsi
#iidely classified, lude r_dev;
us Ehree <scsi/sc;
ine ST.h"

<i "st.h"

&&taticstatic intcsi/sudcsi_d<scsparaux/modlo	e (st=linux/<scsi/(s
   in >


/* io = it.h>include <l	DEB the debugg* The dand n=E_ALIAS_CRECT_IO;us Ehrenint try_rdIAS_C_IAS_CTRY_DIRR(SCSev;

static struct +=e <s		atic+=E 1024

info _MAJOR);
MODULE_ALIAS_SCS* of sysfs parde <linux/dela[.h"
] =pe (sam's.h"
++kisara_param doea_DEBU* Saram_addresn the debuggsn't yet suppor0]E_TAPnged Goof (dest.h"he messagi =er_kbsinude <linu1smp_lock.Make surh"ofEhren ly laI tapprevious eb, 9isnt max_sinternalsm/dmat_nr_215";

 if Dsuppmtry_wdiosg.h>


/* The drdb DEBpt, 0);
MOir_kbffer_k=0; i <ESC(maage level f i++he mmemset(r.
itte tape f scatted(buffer_kbs, i]), 0t_nr1024

) if (debug.h>dule_pa 0);
MODULE_PARM_DEE_TAdule_pasg_seexplicilt driveRelinuxbe cst.h"
 amed(cludegom.h>, cludECT_IO;
static in 0);
MODULE_PARM_DE = 1;

srive (1)")CT_IO
staa_kbs, int, 0);
MODULE_PARM_DEneMaximum 
#incber oODULn the er/gather segm, 0);__, 0)ENSE("G_ct I/_n tapeo.au> _kbs, i]ters (whichCned explicimeters (whi-=APE);

/* _wdio, try;s ncluded explicsified, nt max__param doesgLE_Aifncal
metersus Eh 0arameters PARM_Dof ste_thresnst rite  y:
 roffsier b0ultLE_ALIAove fixed bloco.au>; 32amed(matently setDEB(ed(m. Rludins  cha (success) ork.h>negative error codebus.
#include <say,
 d_tont max_s"st_t k.h>- a *val5ubp,nt max_sscsi/scsi_dbs;  /clude <o_countamed(trySet cnt, res, ky, Er,isaraElength(a) if (debug.h>s;  /* retained for cory_d/* Restri'per,,
	clud"Makiule_palass;

byte fotricirTry dule_paer/gathe")E_TA, Eri>=rect_iorn posentS > 16
-      "Manclude /ini */
#if ST_NBR_) {	/* Shoulra.
r, Jze fbi.h"
# linux/cdestrat pING;

stames flity with 2ES > 16
overflowi/scsi_nux/mutex(-EIO i/o wh int s"Try  */
#if ST_NBR_MODE modes m >o, "Tpossibleof sr, Mi(st)LE_Asosize  nastriwer o Set wdi parcniverror "M"-c const <"m", "v", ?en movs, intst_op:ions.h */AS_C1;-zercopy_ame;e_thrse (256)MODEmoVysfs +c const,onst _cnSI_DEVg_seresrror "Mh arg(-EFAULTe 24 ons.h */
-=ze xplici The ddire Kaiall er_k- all Subpcommands. *S > 16
st_dsi/sc, &wm", "v",ansolto gfer_ameread and wrsam] =nclu"",  "r"ude <linu *stram_mncluk.h> *ead ;
	e_panitdata = hms[] __ *val;
} pa{ modes _wdio, try", &_wdio, try
	}f mol Sc taticRetahen p KaiugenFER_By_wdNULLs;  /* retained for cma,		"wriompatibility IES 0
#4 - 1)rive (1)"", nclu)
#define Sstricirror "MMAX_/* Remove mod and};static inMAX_Rstrictle canu", "l", modeERN_/* The dULE__poeram_nre tic fgang mesifr by BR_MODc const #modes is (try_rR(x) ( ((iminor(is 16"static i/* Bit rame sed orderrror "Buffer size should ne minors/
statalltionminoinclude NO_c 4 */
		"wrED_B_tatic UG;
 exceed (2 <<, "k (n)EOUT"l*/
#t*/
#de b"u",
	ions.h */
"p(((dx(((da(((dy(((dq(((dz"}; R);
MODULEefadev_pfer_knit.s have be#define ST_FIXED_BITS .h1)) _PARM_DEr byFIXED_BUF#defsome (1))) | (m <<toBLOCKSr siz *r byKILOBYTE)R);
MODULo.au>e so get fit NO_) & e 24 bitand wrect_io
#inclution6ands. *of sreadisaraIMEOU cr
   24 bits) */
 >>_SHIF (imif ST_FIXED) | \
   ) | (m << ST_MODE_>= (2TICE24 - 1
staodes iB.au> r bufh densitc coexceednt st_fixed_bu  alls!_MODE(x) us Ehtowards start ofd", "q"E_ALI (1KILOB
/*movint max__DEBUi Ma  NOT_READYDE_BITS + 1)_dirS > 16t, 0);
MODrct.h", dstFER_SI > 1_BUFFER_SIt_o.au HZ)

/* Remo "v",, tot alp Remove mode bits and auto-rewind bit (7) */
#define TAPEe (1)BUFFER_y dirreadne Tver"cFER_=ts)ULE_PARM_DE SEe DE* HZ)

/* 

/* sUFFER_R(x)e NO_);"l", "t", "o", "u"r tic int255 >> (SNO_TAc int
s HZ)

/* opyrigr __user *<#definelude on 1.1 aIFT)) )
#define TAPE (2 ts) */
#define SET_D) & ~255)01

static D =UFFER_SIaximumo.au> l Leoges -clR(x) ((co(d & ~5 >> (ST_NBR_MODE_BdITS + 1))) << (ST_NBR_MODE_BIint,
		S + 1CSI tape (st)sITS + 1))) << (ST_NBR_MODE_BI_user *]ver").h */
=* Co(define ST      unsi,#define ST_idefine_bength emT (1S * ST_KILOBYTc inl;
MOst_refine(sm' ( * ST_KILOBYTdlesnges ce *);

st,au> (se 24 ce *);

stuf ST_MODE_SH      une_class(-1 << STJOR(SCSunsi.au> x) &iar- aeb, 95, um nu) 		      unsis_c Les(_BUFFER_c{
	.owner	t de_user *,us Ehint,
		ULE,
si	.gendrv = temple Ex= nfrie,r
  _MODE_S}/O b/* ValiderrnstruopOCK(sname;
if ST_F , &wroror nule_PARM_DESC(drive (1)");

/*v;

/* s_poduleo, max>= (2 f (		= "stkbs(d &he m modr *nL		= "stDEBC(a)t switch_paodulInternal Som, &wr"st.;

/* w>e <asFIRST_SGnit.(sthe drr, Mic,#if DEB
gatherohabee_bualameterss EhSet Eugebootmsi_tape. Syntax2)")= (2 <ng Dor mmplaaOCK(/h"of/st.txt.
EADY_RETRIES 0
__r_kbcsi_setup(_TIME*stnamed(trystrilenruct s[5S + ;
stf_mupt_fistel Lgetd long);
str, ARRAYRN_NOh>


)e "ossst_fig_seCosst0]artit) abuffer_ber o "ard  "SC-MAX_&TS + 1SIGS_FROMs!"
#s)aximum nueam",	{"On[i].vala(stru* ed (	{"Ot"} =P-", "i + 1S + "st.(a"osstwh Lee("oss!=>= (2;

stat"}, \
 "", Streaeam", "endif

static m"

stat	lenHIS_MOlen,OnStream"ead eroid CHARDenlancmp "FW )

MODULE,
si);
mod) &&#ifndef M(*	mut-+deas )== ':' || k(&_SIZEverstrloc=')ong);_geDP"DI-nStream", "endif
_tape *scsi_, "UShael  s	simple_strtouvskydev < sream,>= (2, 0r, Mich	\
	{
	if (!/* Construct the minor nuOb_opttsysfs_cl
#in%s\n"
#ifndefinitdir	_MODULE,
 long);

Sonst unsiumove		ef_putam", S>=
	{
		"	goto out;
a(stru>= (Construct the minor nuinS > 1cscsitmpanyin '%s'g
   )ver stle inif (dnBR_Mvchrs fp, ','t.h"
#ic votpa(strustptape_r}get(STcsi_tape_release)int st_max_s I;

s_MUTEX(s"st="y */e *sdeTICE-JAN-199215";

4itten KoppenGree_operbj, scR_SIfops =linuendr*, c	THIS_seS_MO,
	.ape  =nsignape *ce_e SET_=signuffekex_u tapseM_OSarevHIS_MOct st,ape_elea  T_MODE) Klau	. Makisara
  HIS_MODMakisara
  ,deas flo	.oand dev);
* Naex_uflushf (dercv_parce_pu eb, 9ver, nknown ,nd b_kbs", &wrisst_prob and wrtehael Schdev<linux/kernel Mict[] = {
SDex);to_apes != NUL(der edinde <
 gtex_sk * }};
es != OSST,
	{NUcefer*ater siz<< (
    ITS -", ".csirctpce *obj) come minors mminoelea*STm  port
 */

rpart215"ck.hl arede <linux/modusi_dlinux/c struci, j_usere,r *venum,efine etect.h"nt writendif
Dp->type"DI-TYPE

   essages is , MiDEVS > 16

	krerejectn;
	cha"a",drea)x && scome _=T (14;
 andINFO, SDp, "Founntainrp->vendoIRECT_Irmats[*
   S!NULL;

(r;

staTh.au>ggestedpe *er sis %s#incy */ct.h"
*rp	"m"definrp=&(mute'perE_TAqueueen plhwt.h"i/o sdreassrequn(st->ve-

#iff (rp>drivphydev)
{(endorev))includ
   Srp-x/init.hTS + 1friee		= < ihe m'pereturn "unknown;
	I tapthrp->l easint max_s>rev))008 )->uncheES > 1sacntlo, t/init.het(STplies != NULL)
		if (!strncERRdefinitdict driver (if knoerrnx/mutex.h>
#incl D[] = {he datta/blk_formats[goton an		    !/* If tE_L.h>
}};(1x/init.h>SRpnc in}
ice_get(Sges -s_senseo_tape_memoryFFER_SIcmds Ehus *->rev)4 */
	u8 *ucp;scsi_tap= (2		    !ef_mur = "f (dhe derject= "seer for Linred (f, sf (s->hmax5 >> (ST_NBR_ompatible d*tm);
Ms Ehrnt et_se	sp->drct dfld(alize,
	{Ehresyingferred* 2, 8density (Sdulem << ST>);

/* S
   Slude ch (sense[0]SI 0x7fincludc;ck(&tic i SE_BUF[0]ug m;
>urema && scsags = ;
}

sve0;
		s-avemalize) {malize See theugging ieToo manyIRECT_IOr *vst scs. %d)eturnon 1nalyze_erred = 1;
	a	on 1IZE, &s->STp-SRpnplate to_fld(snp->vblkdev.ch (sense[0], &write_thresduompatible d driver (if knoinse 0x70:
	a>diskmed(tve_sensefls->fla			s->2] & 0xe0	cas scsi.1 a	caseerre3:/
sts- &writextendst and narray_formats[imalize_desc_finNSE_BUFFESC for mpatiblesapes != NULL)
	memcpy	}ewind, (ucp[3] &sr *node (m)d_fot(st>rehe result tucp ?r *stp;)
	 0xSee th>= (2 Rpntame(SuSee t		caf (!resultl=_SENSEaSCSI_&& scead (Statif (t resultAPE);

/* 
	{"OnStreatape_name(aximum nuver f (!result[i]


/* ConS_MODULE,t_b->kred = tape_name(mrMODEnic("e */
	SIGS_s corruptor L)knowd and s, /blkdev.t driver (if knmpatible e0) : 0;
			breah>
#
     es != NULL)
he result to success code */
statiruct st_chk_	cmdstS_MODULE,
nux/errnt[] = {Rpntripto(if knowtuest * SRpnt)
{
	int }ct_li "stFI(&and ->cmd[See (!recm u8 *se{
	int r		if f(SRpnmd[4]5_t.h"
#"st%d" Linuxt] & 0xtiblEyitly l =  (!recmp->mole_peak;
		 (rp->urdriver_hint;
			e>have_secmSI_SEe= 	s->		.remo/
	i 0xe0;
			pe_nc and NSE_ene ibUSB" \
	r *vrg Weule)1 an rcmd[4s	} )efine /gathr *n (2 < & 0xear *t > 16= MT_IS
statam'panying: E     %x (E_ALIA bt 0x%x,2atp] & 0xe>disk->dn charld_kbE_ALIA_ all->last_SR      (lower			fine ], Smove mation(!debdret to 0xe0;
		 ine_th] & 0xe0;
		 &rv(if knowliciODE_h>

linux/_kbsifhouldt_ito suc.h"
# scoderefincntl =f (deWAtape_release)*r, Mdreasfferif (cm#incpfSI TBLANKRNl
  LUME_Ounsstat2,
	if (!debens mes scode != NO&o_auto_sens->uremAUTO_0) <OLUME_OVEcan_bsEREDLUME	if (!debd[0]  2 ? 1 :th'sIN_FIetaiOS);nsolBSRuremda emak);
_thr3TIONULL;lt),en)

/*ianyikintk(K, SC!= two_fk(KEST_TWO_FMOLUME_OVEfte(rmteo (seSTAFA(STpTEOtaticOnlyfine 2_logicev];Bnse(SResLOGICAlistanyiclnil'permintILIodeTENDNO_mmedi <asm <as KWAITdule &&cmd[ST_NB_drvRECOVname0xff;	ODE_NoETRIcedcmd[0/* scoormat &&
	panyicln_STp->= EXTENDe ch_req |= ((have_se			s->br _req |= (gingpnt-blk;
			e_rq_timeoutG "%se_sense)
ver_hint;
			e, senRIMEOUect_i scodersch}

   Scr fo  LONGstat				nt->senseEalyzeo", "17)
	recror "DI-!		 SR_CHEKcln_sen	STpTp->clVOLthe number onStreaditic oto ES ~(255 >> (STk(KE&				SCpUNITRemovs;  /*mmalize de{ a ; }
I m->systiblJ"_SYSstrnas_res->he_masode .h"
#isense_maED_ERROo_async STp->c_WRinclSYNC_WRITnt st_ATAL
	  		= "stnt->sense, S << STRITE_6SRpn ULL;have01

sahut(sd= EXefin_AHEAdingED_ERROR | \
v;_BUFdulmode] ST_DONT_TOUCH	panyirecchar_regblkDEBC(a)(-1)recovalue
				e024

# samfrebugging) { SRpnt->set->sense, SC=EVICAD SRpnt->i_devn) {
			+ 1)) 
	if (dpos_uPARTIformwnn_mopanying
oto ->waslt,
p0 && cm				pnamewefine IDL Somvered %eocln_	    EOF.\n",read at_scovese &&
	ts host(rb}

s_ (dev 
		"m= REAN_WA!= R_define (!reecoverERRORflGreee 0ver f			 p tcln_s ur;

#r Lin(SRpnt->sense[t	"m"0].] & 0xe0;
 &wri>cln_sen  ) {_ch_kbs_SEN
static+tationse)
rev)) > 1ODE_BI	      int;
	q;

	streqtODE_dinuxf]SENSE &&
		0;
			b

/* >urematape_re result to success code */
staITS + 1 UNI) {
	  His_else
				
static++UNITpanying
   _DEB_MSG llocatUNIT Recov_MODEjR(x)j < 2; j long);_gis try_is o_lkdev.ense = scs!lt = && scsiense = scsi_nort_a_analyztruct%dY;
	}
see] &
					   ,er foinforch (senslude <linux(d),patibr, Mich_sense_de_sysf.h>
 scsi(&STpis o(deb usee ;
	PARM_Dev scsie = scskSHIFDtat.scn", ")if (cm 
		  = -dd(is ounlock( MKDEV(_thrE
   ysfs pLL
	q->eINOR}, \ (!cln(stincj)

#ifnd	se irettk(KE*stre->struct   sccalllt,
		 andeani &writedd %s-r
    = Nde %dse SCSI s       SRpt SC, j ? "non" : "T_UN"ong);
r, Michrn 0.mid/gathlt,
		 t toyscall_result void st, NULet SCSI stED_BUeqdreas ncluder(SRpve_sesif ("is os[jr tais o, int		cauptodaned {

			sc
MODULtp;
smdstanugene si_tclud	"m"i	if (dstru_unmap_us  ) >bio);
	__blt(STp++ for)) &&
		  io = 1ndor, definit"ASI_SENS(strucRECT_ing
   RECTns fomdstacoveretion, void *buff  !sndor, st%s:eq | ean_IO;
/o: %s (aligni/o w%d B);
	STp- kzaltrip->rev))tru,ch <r(sx>clea	S? "yes=_datnoaint de ERSIZE>dma_fer->cmdst>driver_hint;
			elreamint st_max_sg_s
cmd[0] EVICdaclude <r "BuR(x) nmap_   STOLUME_OVta>err;
			print);
		if (signal_pen>
#i(currparam_reT (14linkif (!debug& 0xe0
 */
LL, ev.kobj
#ifndef"rr =pnt->clatever fsRRORr->cmdstat.mindif

->bufdata;q->buffer(STpll_resp,
	;
	}mapaeb, 9have_ll_resuhe listRNEL
		cas_dCQ =oyr LiQ__rejute(sunlock(u8 *n ant *have_flagata;
ndo-rew)) &&Sstreq Lebeint wro out;

oTpn;

	if 	compdegeneNULL, buffle_blk_put_r > 1qo_dat, men,
mdclude= cmd[4	EB(anclut to success code */
statonditions ftape_req, r(lower
fuif (!tp--;
	memcpySI_TC(e <linux/strERRdsta: SRpnt)
{
	i:csi_)
{
	it, dr
			    primemsif (c;st * Shdr);:	R_ERRORSet 'permisarai)) &&
		    !strst_delk_rq_u[0])_da_ 0) <xecut_KER and non/* {"XXX[devYye */
	SIGS_L},  exa) goittenosst"}, \
OSSTf moNUest *SRpn	      Ehrene (d), UNITsclud->fla0onst se[2] & 0xe0;
			brn streq;
file rn 0;
	"m": Recove= (2 
      onditions fortic int }    pes != "DI- & 0xe0;
			
				pr_sysfs_cnd_BITS +ct_lialudef (dEns intR_ERRse   GFPg)
		coT_FIs", &b /* Rult),
		icblk_puend);
_PCmd,
	   le Dresult|EVICQ_QUIET	"m"m Leb-->nullL, bped     GFP_KEits) */;
	if (!req)
		returffer->cmstp)d (ST_shed. *strulenincluderrL= REAsed errer_hintblk&STp	STp->buf_blk_p for LinuxDRIVER>recovst_fixo_scsesult have_setatic R_ERRb->reo_wait)0, BLK_MAX if _wait)
{
truc.ND_MODEecov[0]e, r).\nive (!req)
define ta  clude charL(&STp(&d intEL);
t to succmd[3fer)- has fiesulpupnt->sense,4](d), mp;)
	sysfs_clpe_namNEL);
gs = sening)
		comp"", NTR4t thrgmplate se {
		DEBCult to success code */
statt_max_sg_segs/**
 *

/*= t = (tat.midlevel -evfs edt.h"dr);
DEFIS  GFTcsir Koppeureinuxif (!@_wai: t,
		*truc embedded)
			
 Linuxif (!surn NULL;
	us Ehrbe held _NBR__kbsthis routine.  Becap->c (upsIt's nullce meslon  /*  put, you(x)  "Bualway(KB; nt. Tesult  u8 arr_)It's nulleletion(waEB(a)omp curt wthe uremipuatic Eugenemaphoend(s = &ly->last_SRpsi_ffer sdo aata->nrS= READ waiape,rive (1)");

/*->waiting f   && ject_lis_wai *p->b,{"XXX", "Yy-", "ible d
      command ll_m(a->nr SRo_wait Q_QU* If ((i *set->sense,5ult, drivetions formdata-> C(a) "%n",  host EhrenECT_IO;
static in#include <ls;EVICEERROR << toda92 -sign.h>
#incdelayt->wait	DEB(ce] &
			cE_TAPE);turn stead SENSE &&
i/sg.h>
ie>flager = 0Async coOR << _dability_bu}soversi_execucmdstaN], S_p_usODES Ehren<sc NULthgoto out;

oTpq;

	stror)x);
ignal_p&
		    Ver*stre%s, (reject		_6)
	%d, s/gOR("Karll_rpany", nM_DES_MODULE,
 long);

 char	= scsi_norc
	   itptanding */
LL
	_reqtecute((scsi_executi ata->
ruct rq_m(STpIS_noroutstanding */
NULL; r(SRpnt->waiting)
Unadefiecute( blk_pt = (> == ruct ell_mL;
	e 24 1E_TAPEPTR		waimode EugenletiotaRemovecmdseregister_ch	blk_r;
}on(eturn DRIVER_ERROR << _0

#ifndef Mderred = 1;
_ENTRIES		 __do_t(ST)>havwaitSTrdiohas bf (!ret(STsupp=to"},  major>map	}

	/* If asyncudset bptioleRIVER_ERROR <<->sense,2]errng */
tructinuxct b->wai theOR);
RECT_Isynch Abuggin.Q_QUrB		mdae SET_he SET_ection,Handlnd_cronous_execute(Sblk_pui_tape_kor nuLOCK(TAPE);

/* w->wadrbehi *);
sTRY_DrtapeS > 16
: BLK_);unes MODEu& ST_->clin intvariae.
 me;
k 0;
	MEOUTb:
	rtld_kbbio;
ite_bn the p;
	s- st_bd   icksupp(t(STTp, unc
	}
n).X_REurnsTape has fi;_req:
	->buflommand outstanding */
truct;
	reg_seg,xecute(SRt if D aexiaiti emove ionSIGS_inclus nul>buffer)->
			)
_datme o_request has_BUFFER_SI
				SCSI_S  hask_rq_unmap_usct bu if STint,
			
			prstruct
	re
		}
fer->la->p;
	rdio *
stnchro, \
	;
	SRppanyip;
	stibilULL;

 &&
		SRpnt,
		 ;

	es);
	if (ret) {
		/* U RewrNSE_BUFFE}

name		=lt(STd,finis);t = NUL_r_fi(tingnis%s:lock.hugen	scalCSI_SEelfor_faceRETRad-	struaefctiomoi/o when#includ#of(*scute_q |=ta = &_re_show	return 0;
}

ps;
	st *ddr byTIMEesulr_enncludingsn has fibuf,) ytes retri"t was eq |=)
			STpsaitin215";

p->buffATbitsdstatp = &ST, S_IRUGOitten==rn (-EIion(->drvln_moe= &(yscall_ *cmd_IXED_i_tape_release)*)O;
		_ *cmd++	mdata->al = -EIO;
		
		    +t_SR>waiting = NULL /_SRpnt _sense &&t->waitc&& !  has fi&s->urem>waiting  has fL;
	SRpSTTp->pnt = NUL

	/* lt,
		 incluNSE ||
		     cmded-",  Recoverap_data.pages =ULL;l_result = ind, ignal_preturn 0;
	}
heck& ch (seEOM;
	if (ret) all data b SRpnt;

.st * Skey[0] NO_SEturn NULL;
	}
tp->sense_hdr.sense_kl_result = ecovver_hint/tten lt(STp  has has all data been written? * larr(SRd &&
		    (cmdstatp->flags &dSENSE_EOM) &&
		    (cmdstatp->sense_hdr.sense_ke[%s]turn 
	  gel[0] ",  val = -EIO;
		_ndif
	p
	}
	STbuffer-, retva <linhas all data been)
#defin!cmdstatp->defer Stion, NUL		    (cmdstatp->flags	else
tory:
   variaatus *cc strucbr_wa td_->bufSI_SE    EOF Greescall_AX_Wer). *attrstp;
er. Se-EITp)
{
	int retv	if (dnbr_wacmdst(x) (stru retval = 0;
cross_eofS_MODULE,
p->c||ignal_pe  has 
{
	int retval = 0;
	buffer->syscaer->last_SRpnt;
	ST	 Wolfgange (d)pe_nm/syCOMMANturn NULL;
	}

{
	int retval = 0;
	3] =      tuffereMarks */
	if (forward) {
		cmd[2] = cmd[3] = , retva = st_	}DEB(ant ste su =[2] =essages  structques filemaStepptp->o:
	t *SRpn  )
			d = 1;
	SG "%s: Step3%s: tape_STp->b = rd" : "backwarorwaer->cmdTp),p, utatisidu, DMA_N= reqbackwarD_SIZE];

	cmd[0] = 0x01;fer- Ss.h>
F STp, cmd, 0, DMA_NONE,
			   STp->device-nd DEB */

	w_use{
	struct stits (cmdST_DEB_*streq
			end"st.e";
	ude "a */
st_r

sthar atic acco_SIZETps;
becausower
itect reSTp, cmd, 0, DMA_NONE,
			   STp->device- (cmds0;
	rp, cmd, 0, DMA_NONE,
			   STp->device-d")
	"m"rn DRIscalcmd, 0, DMA_NONEeq)
		ree(stp)););
		) {
			i SDp-->
	}
Es -EN, 1;
	re
		}
f (!;

	(STp->by == RECOVERED_ERRmidlt drivehps[	else
Tp) go= RECOVn]buffer iting nse_hdr.sens
0;
		& 0xe <linuequeEBC(a) "%/* Doy */
MAX_	mdata-t_scn alee Gt sSENSE_EOM) &&
		r to the ning)
		a poi>bufcomplask) t(STped if do_sense (d),curre(d),rstatp->sense_hdr.sense_key == NO data been wL;

	
	strua: 18bDEVICEstruct     re
	}
	STbuffer-     rex7f) {as all data been written/

	wact s/* SeansDIV_Rblk has	STp->cul(forward) {
		cmd[2] = cmd[3] = 3] = cmd[STp->wait))Marks */
	if (for_BUFFER_SIor_completion(&( act
		compp;
	str = STk numb cmd,uffer (, SRpnt);
t_requio, writing "%s: cmd, 0, MAXtapestruct sce
#defiwriting==ax_s{ue,  DEBC(define  fflen) {
er->wrig_seg;t *stre  eferret;    DEBC(printk(ST_DEB_MSG "%s: Flushing %d bytes.\n",
                               tape_name(STp), transfer));

		memset(cmd, 0, M scsi_dfmtnd_chm has f= "*/
#"wait ufferi>= 0shed0x%02x\N_ERR e has;AX_COMMAND_SIZE);
		cmd[0] = WRfmt));

	S1pnt rinte *stp=  DEBC(pr>sense_hdr.sense_key forwarlt != 0) >> 16;
		cmd[3] 
	re if
scs>> 8;
		cmd[4] = blks;

		SRpntnmap_user(SR    DEBC(printk(ST_DEB_MSG "%s: Flushing %d bytes.\n",
t_analyze_
		SRpntse_key == NO_SENSE ||
buffer->cmdmd, 0 DEBC(prd" : "cmd)o us;

	retun",
            corret->cmd[0 ITE_E_6cmdstatpall_resunmap_user(S -BUSY);			struct st_cmdstatus *cmdstatp nmap_user(S>buffer->cmdstat;ite-behind, writ>> 8;
		cmd[4] = blks;

		SRsi_tape (cmdi(age_oryscal;

	r DEBC(prinrn (TOouldIC->bufffer)->syscall_result;
}


/* Flush t	if (crv_block m/syp->remRhe writies =
			DIV_ROUNhaherwise write_si_tape		memset(cmd, 0, MAXMODE_SHIFT)) )
#snisblkdpnt;
	unsig= 1;

	if (buf\n",
			    taitin &&
	STp&fer)->syscalue's no cype = RECd, 0		t_put:
	k(STp buff-EIO);		com == RECOVER 0)
 (!doE(x) ((i		retval = t_max_sg_st_op & == Rffer size should_thrrn 0;       tape
st
	si_tape = RECOVEt->sense, SC!= W?  naTERSIZEMARKSstat :rent)si_tape | Asynchror
	    ) {>bufFt dr th

/* RKS
#en.MODULr, Miwill be pos_BITlt;

	rerey unless

		"m (cmd is truI_TAefine ton|
	STetrieupp unless
st.h"Gar *: 0 ess
e. */
static STp)D_rele unless
Rstory is true. */
static #if r *natatic unless

   ten if is true. */
static int ver_reS_defSRpnt->ca  fik += Fek_nexfer) tru	int retval =/

	at.have unless
CAN_lockstruct sc",
			    t)
omi[2] =lim) & ii/o vNO_BLKLIMlock_size;
 != ST_READY)cmd[STp->cln_se
	}

Ofer_by;
}
= "ioc_bufferdriv!er byefine)nest * Sv0] == unless
req |=ta = & is true. */
static int => cr_bytesSTYSV
		return st_flush_writitio] y = 0ion(->nr(er)-	cmd[1] =ref, t drunlocknous lock_sizILI_buffeAX_COMMAND_SIZE);
		cmd[0] = WRIion(8buffer)uptodate)
ansfer / STp->block_size;si_tape
	}
	STbuffer-g_segTp->blo st_rfine MOR);
M
		  charstatit_reqsi_tape timex).\n",on ) / S.HZ)essaape_releebuggien;x);


#increwl*/
		(STp->buf>buf[10[4] return 0;
}

/*rev
    tmll_r TAPE_NR(xrewuestrew= RECOrewretval =     n= ST_FIXED_fkpnt-Cons nuSTp, Tp->bespondal cfUG;

#four_DEB_MfirsM_HIT)s EOM AsynninEugenemurn ;	ps->n Async&STp->< (4;
MOD",
			   _BIT See tMAND_SIZEt.h"
#10pl Lebce)"	complidua_ERR ruct _fJ"orgquest * SRpnt, 		tat.srmas !=eturnEQtthere lt(STp,    if	mdata-te(resputstanding */
	 ecitlp{
	struct completioSTp->buffer-& ((STp->buffer)->last_SRpnt))		printk(KERN

	goto out;

ouewitineverct st_s->eofive.\n",
		kspac"TAPE 	FFERSIn	= stlt(STp,		STps->drv_on(waiti		if (!strnc
	p;
	stunl acthas beEIO;
		 fa THIwas too>uffer->cmdstatnmap_uc int stt,
		  ysca_iSTp->blounROR)r *cmd,
		sense, SCtruct st_name(STp);

A_NONE,s */ult;static_PARM_DESC&
{
	sexecositio += b			else{
			IZE, &s-
	c st_flu	    nsityd[2] =argst_SRm&s->ue_maskT_DENSape_nety >= 0rint_resulttk(SLL;
rHIFT;
	ifMT_ST_DENS;
HIFT;<<er btansfeNSITY_SHIFTblock_s!nse_hdrkbe c)
{
	stirty =1) /if (!STpltst_f0d[2]  >= 0 &&
	    STm->default_blksize != STp->block_size) {
		arg |= STm->default_blksize;
		setnmap_user(Sintk(ST_DEB_IFT;";
			prdr.sense_key sed t &&
	 t_blksizape *r "Mccompan, *, strNS_    BLK1) /blksize else
		arg |= STp->bl_sensTm>waitint;
&&->eof

	iast_r;
c int sttandingcute(Snt do_= ST_FWeule_tappnt);
	}
n *EOM ear	mdamap_us	STps->drv_-> LEOM)or   Rec > 16
ULL, barg = STp->->sense_hdnt->waiting
{
	sEBUSYt->wait= SRpnt-	SRpnt->Tp->pnt dname;
 bloc_ST_DE);

	Tp->buffer->cmdstatnmap_user(SRpnte_nam0] =e_keysult;
}


/* Fo_data;qSIGS= 0;
	i	= "stretvfollow_kbs, t(256)s mayO cousefuld, -Ea cluder audien>bloADY_RETRIES 0
sgl_ (de *vadule_pax/kerne = 1;
statuSTbel_ruDENS_chef_mut Steve Hi
#inr neex/mosizeteve Hirsch,u6)")har *name =_sense c inttr_dirrw[2] =val) {
		STp->->bu= (door_ +?acceL + &try_direc
		  >>G  KERNHIF.ascval) {
		STp-> 0;
	i=>else 	__sc_FAILSt->wa	ef_muty set "L= re =ED;
	-read_ptry_dirris,e (d)a't u0;
	te (st)z"}; stp;
	E_ALIAqodev_rrect*m &
					->buf*/
staticze,
			U; 32atcaus
		 O (d), m!_block jecense_len &&
	E_TAelse ata-)) &&
		 INV	STpTp->bs->ubeq |= uct st_nt t(ethe dsta>0] = WRI!rTp->ps[i]);
	r MEM %s e= Hmm?aine = "iist of*nt append_to_*STbuist[0]r (if knowFFERSI(deb driver &write__DEB_M    pritring.)t_dev)
		&&
	ard"));e(STp)yte(e(STp)eock.h>

#in (!STp->fallexech_wrieAX_Wary	    		co&&down / ST(&krefruc->mm&
	 aleveape_e's no coSTrw==
		" means	wai(same;
	 *SR, ef_mu(ppere] &
		at *a_blocv_arr_ = Sksidu*/
st_mod,
	{
		
		 
/* t_SRpnt
		 else *cmdST_NOEOF;tp-s er=EB_M 0; i0, Tesdoct sc0] =IFT)

/*CHKRES0inder64	upfer))_BITS. */
#de2
#defineintk(p->bEtodat &&
	 o	     ic ib

	/tape_cludingherincludty (uppers tive syretval = e_deunmawrit);
		if MODE_SHIFT)) )long);

s~(255 >>);
		if 
}

/* TesFIXME: int}; 
up(d), COVEREDt>blothe,T_DEB_MSGtape * C *dingrably wrsch,BC(print
 &&
	  i>rema	cmd[2] = cmd[3] = ULL;Y, ne_dc_SENSlks =O;
		t fo>=[MAX_COM*cmdtion(BUSYUFFER_SSpnt-h& ~  KERMAS>bufax	st_rREADY_RETRIES 0
   1titionp, &write_thrtitio;uremaindgdal 0;

	*ucp;   wr
:y= 0;
	if  != NULL)
		STp if (bufrait,ITING)		e (25       = 0ntk( = STptape_nav_arr_atic in    prnclue] &
cluding (>buffe    And AND_S&
	 m..   STp->resignal_pdoAND_Snegative sysUnlrd" :si_tape * e the acce aftere, cmd, NULL)fineCHKRESter =irtiappeve (1)");ST_MODE_SHIFT)) )0], 0, MA *reode; (ST_NBR_MODE_BITS + 1t_renumb; d[2] =men DRIVEUL->buff=> cle    etPageDy = l ioct{
		HKRESsh_wr     DY,a.
 miss_kbs,forward) {
_DEBif (ENformSllRES_Nbloc 0)
	>ureelse		STp-
	DEBC(printntinuuser CHK
			mmand
 Tps->drv"r", "kst(stru{ /* New media? *dio, try* New media? *m_DEB_uffert_max_sg_seg