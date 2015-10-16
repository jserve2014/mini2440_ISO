/*
 *  linux/drivers/block/floppy.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *  Copyright (C) 1993, 1994  Alain Knaff
 *  Copyright (C) 1998 Alan Cox
 */

/*
 * 02.12.91 - Changed to static variables to indicate need for reset
 * and recalibrate. This makes some things easier (output_byte reset
 * checking etc), and means less interrupt jumping in case of errors,
 * so the code is hopefully easier to understand.
 */

/*
 * This file is certainly a mess. I've tried my best to get it working,
 * but I don't like programming floppies, and I have only one anyway.
 * Urgel. I should check for more errors, and do more graceful error
 * recovery. Seems there are problems with several drives. I've tried to
 * correct them. No promises.
 */

/*
 * As with hd.c, all routines within this file can (and will) be called
 * by interrupts, so extreme caution is needed. A hardware interrupt
 * handler may not sleep, or a kernel panic will happen. Thus I cannot
 * call "floppy-on" directly, but have to set a special timer interrupt
 * etc.
 */

/*
 * 28.02.92 - made track-buffering routines, based on the routines written
 * by entropy@wintermute.wpi.edu (Lawrence Foard). Linus.
 */

/*
 * Automatic floppy-detection and formatting written by Werner Almesberger
 * (almesber@nessie.cs.id.ethz.ch), who also corrected some problems with
 * the floppy-change signal detection.
 */

/*
 * 1992/7/22 -- Hennus Bergman: Added better error reporting, fixed
 * FDC data overrun bug, added some preliminary stuff for vertical
 * recording support.
 *
 * 1992/9/17: Added DMA allocation & DMA functions. -- hhb.
 *
 * TODO: Errors are still not counted properly.
 */

/* 1992/9/20
 * Modifications for ``Sector Shifting'' by Rob Hooft (hooft@chem.ruu.nl)
 * modeled after the freeware MS-DOS program fdformat/88 V1.8 by
 * Christoph H. Hochst\"atter.
 * I have fixed the shift values to the ones I always use. Maybe a new
 * ioctl() should be created to be able to modify them.
 * There is a bug in the driver that makes it impossible to format a
 * floppy as the first thing after bootup.
 */

/*
 * 1993/4/29 -- Linus -- cleaned up the timer handling in the kernel, and
 * this helped the floppy driver as well. Much cleaner, and still seems to
 * work.
 */

/* 1994/6/24 --bbroad-- added the floppy table entries and made
 * minor modifications to allow 2.88 floppies to be run.
 */

/* 1994/7/13 -- Paul Vojta -- modified the probing code to allow three or more
 * disk types.
 */

/*
 * 1994/8/8 -- Alain Knaff -- Switched to fdpatch driver: Support for bigger
 * format bug fixes, but unfortunately some new bugs too...
 */

/* 1994/9/17 -- Koen Holtman -- added logging of physical floppy write
 * errors to allow safe writing by specialized programs.
 */

/* 1995/4/24 -- Dan Fandrich -- added support for Commodore 1581 3.5" disks
 * by defining bit 1 of the "stretch" parameter to mean put sectors on the
 * opposite side of the disk, leaving the sector IDs alone (i.e. Commodore's
 * drives are "upside-down").
 */

/*
 * 1995/8/26 -- Andreas Busse -- added Mips support.
 */

/*
 * 1995/10/18 -- Ralf Baechle -- Portability cleanup; move machine dependent
 * features to asm/floppy.h.
 */

/*
 * 1998/1/21 -- Richard Gooch <rgooch@atnf.csiro.au> -- devfs support
 */

/*
 * 1998/05/07 -- Russell King -- More portability cleanups; moved definition of
 * interrupt and dma channel to asm/floppy.h. Cleaned up some formatting &
 * use of '0' for NULL.
 */

/*
 * 1998/06/07 -- Alan Cox -- Merged the 2.0.34 fixes for resource allocation
 * failures.
 */

/*
 * 1998/09/20 -- David Weinehall -- Added slow-down code for buggy PS/2-drives.
 */

/*
 * 1999/08/13 -- Paul Slootman -- floppy stopped working on Alpha after 24
 * days, 6 hours, 32 minutes and 32 seconds (i.e. MAXINT jiffies; ints were
 * being used to store jiffies, which are unsigned longs).
 */

/*
 * 2000/08/28 -- Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 * - get rid of check_region
 * - s/suser/capable/
 */

/*
 * 2001/08/26 -- Paul Gortmaker - fix insmod oops on machines with no
 * floppy controller (lingering task on list after module is gone... boom.)
 */

/*
 * 2002/02/07 -- Anton Altaparmakov - Fix io ports reservation to correct range
 * (0x3f2-0x3f5, 0x3f7). This fix is a bit of a hack but the proper fix
 * requires many non-obvious changes in arch dependent code.
 */

/* 2003/07/28 -- Daniele Bellucci <bellucda@tiscali.it>.
 * Better audit of register_blkdev.
 */

#define FLOPPY_SANITY_CHECK
#undef  FLOPPY_SILENT_DCL_CLEAR

#define REALLY_SLOW_IO

#define DEBUGT 2
#define DCL_DEBUG	/* debug disk change line */

/* do print messages for unexpected interrupts */
static int print_unex = 1;
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#define FDPATCHES
#include <linux/fdreg.h>
#include <linux/fd.h>
#include <linux/hdreg.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/bio.h>
#include <linux/string.h>
#include <linux/jiffies.h>
#include <linux/fcntl.h>
#include <linux/delay.h>
#include <linux/mc146818rtc.h>	/* CMOS defines */
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/buffer_head.h>	/* for invalidate_buffers() */
#include <linux/mutex.h>

/*
 * PS/2 floppies have much slower step rates than regular floppies.
 * It's been recommended that take about 1/4 of the default speed
 * in some more extreme cases.
 */
static int slow_floppy;

#include <asm/dma.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/uaccess.h>

static int FLOPPY_IRQ = 6;
static int FLOPPY_DMA = 2;
static int can_use_virtual_dma = 2;
/* =======
 * can use virtual DMA:
 * 0 = use of virtual DMA disallowed by config
 * 1 = use of virtual DMA prescribed by config
 * 2 = no virtual DMA preference configured.  By default try hard DMA,
 * but fall back on virtual DMA when not enough memory available
 */

static int use_virtual_dma;
/* =======
 * use virtual DMA
 * 0 using hard DMA
 * 1 using virtual DMA
 * This variable is set to virtual when a DMA mem problem arises, and
 * reset back in floppy_grab_irq_and_dma.
 * It is not safe to reset it in other circumstances, because the floppy
 * driver may have several buffers in use at once, and we do currently not
 * record each buffers capabilities
 */

static DEFINE_SPINLOCK(floppy_lock);

static unsigned short virtual_dma_port = 0x3f0;
irqreturn_t floppy_interrupt(int irq, void *dev_id);
static int set_dor(int fdc, char mask, char data);

#define K_64	0x10000		/* 64KB */

/* the following is the mask of allowed drives. By default units 2 and
 * 3 of both floppy controllers are disabled, because switching on the
 * motor of these drives causes system hangs on some PCI computers. drive
 * 0 is the low bit (0x1), and drive 7 is the high bit (0x80). Bits are on if
 * a drive is allowed.
 *
 * NOTE: This must come before we include the arch floppy header because
 *       some ports reference this variable from there. -DaveM
 */

static int allowed_drive_mask = 0x33;

#include <asm/floppy.h>

static int irqdma_allocated;

#define DEVICE_NAME "floppy"

#include <linux/blkdev.h>
#include <linux/blkpg.h>
#include <linux/cdrom.h>	/* for the compatibility eject ioctl */
#include <linux/completion.h>

static struct request *current_req;
static struct request_queue *floppy_queue;
static void do_fd_request(struct request_queue * q);

#ifndef fd_get_dma_residue
#define fd_get_dma_residue() get_dma_residue(FLOPPY_DMA)
#endif

/* Dma Memory related stuff */

#ifndef fd_dma_mem_free
#define fd_dma_mem_free(addr, size) free_pages(addr, get_order(size))
#endif

#ifndef fd_dma_mem_alloc
#define fd_dma_mem_alloc(size) __get_dma_pages(GFP_KERNEL,get_order(size))
#endif

static inline void fallback_on_nodma_alloc(char **addr, size_t l)
{
#ifdef FLOPPY_CAN_FALLBACK_ON_NODMA
	if (*addr)
		return;		/* we have the memory */
	if (can_use_virtual_dma != 2)
		return;		/* no fallback allowed */
	printk("DMA memory shortage. Temporarily falling back on virtual DMA\n");
	*addr = (char *)nodma_mem_alloc(l);
#else
	return;
#endif
}

/* End dma memory related stuff */

static unsigned long fake_change;
static int initialising = 1;

#define ITYPE(x) (((x)>>2) & 0x1f)
#define TOMINOR(x) ((x & 3) | ((x & 4) << 5))
#define UNIT(x) ((x) & 0x03)	/* drive on fdc */
#define FDC(x) (((x) & 0x04) >> 2)	/* fdc of drive */
	/* reverse mapping from unit and fdc to drive */
#define REVDRIVE(fdc, unit) ((unit) + ((fdc) << 2))
#define DP (&drive_params[current_drive])
#define DRS (&drive_state[current_drive])
#define DRWE (&write_errors[current_drive])
#define FDCS (&fdc_state[fdc])
#define CLEARF(x) clear_bit(x##_BIT, &DRS->flags)
#define SETF(x) set_bit(x##_BIT, &DRS->flags)
#define TESTF(x) test_bit(x##_BIT, &DRS->flags)

#define UDP (&drive_params[drive])
#define UDRS (&drive_state[drive])
#define UDRWE (&write_errors[drive])
#define UFDCS (&fdc_state[FDC(drive)])
#define UCLEARF(x) clear_bit(x##_BIT, &UDRS->flags)
#define USETF(x) set_bit(x##_BIT, &UDRS->flags)
#define UTESTF(x) test_bit(x##_BIT, &UDRS->flags)

#define DPRINT(format, args...) printk(DEVICE_NAME "%d: " format, current_drive , ## args)

#define PH_HEAD(floppy,head) (((((floppy)->stretch & 2) >>1) ^ head) << 2)
#define STRETCH(floppy) ((floppy)->stretch & FD_STRETCH)

#define CLEARSTRUCT(x) memset((x), 0, sizeof(*(x)))

/* read/write */
#define COMMAND raw_cmd->cmd[0]
#define DR_SELECT raw_cmd->cmd[1]
#define TRACK raw_cmd->cmd[2]
#define HEAD raw_cmd->cmd[3]
#define SECTOR raw_cmd->cmd[4]
#define SIZECODE raw_cmd->cmd[5]
#define SECT_PER_TRACK raw_cmd->cmd[6]
#define GAP raw_cmd->cmd[7]
#define SIZECODE2 raw_cmd->cmd[8]
#define NR_RW 9

/* format */
#define F_SIZECODE raw_cmd->cmd[2]
#define F_SECT_PER_TRACK raw_cmd->cmd[3]
#define F_GAP raw_cmd->cmd[4]
#define F_FILL raw_cmd->cmd[5]
#define NR_F 6

/*
 * Maximum disk size (in kilobytes). This default is used whenever the
 * current disk size is unknown.
 * [Now it is rather a minimum]
 */
#define MAX_DISK_SIZE 4		/* 3984 */

/*
 * globals used by 'result()'
 */
#define MAX_REPLIES 16
static unsigned char reply_buffer[MAX_REPLIES];
static int inr;			/* size of reply buffer, when called from interrupt */
#define ST0 (reply_buffer[0])
#define ST1 (reply_buffer[1])
#define ST2 (reply_buffer[2])
#define ST3 (reply_buffer[0])	/* result of GETSTATUS */
#define R_TRACK (reply_buffer[3])
#define R_HEAD (reply_buffer[4])
#define R_SECTOR (reply_buffer[5])
#define R_SIZECODE (reply_buffer[6])
#define SEL_DLY (2*HZ/100)

/*
 * this struct defines the different floppy drive types.
 */
static struct {
	struct floppy_drive_params params;
	const char *name;	/* name printed while booting */
} default_drive_params[] = {
/* NOTE: the time values in jiffies should be in msec!
 CMOS drive type
  |     Maximum data rate supported by drive type
  |     |   Head load time, msec
  |     |   |   Head unload time, msec (not used)
  |     |   |   |     Step rate interval, usec
  |     |   |   |     |       Time needed for spinup time (jiffies)
  |     |   |   |     |       |      Timeout for spinning down (jiffies)
  |     |   |   |     |       |      |   Spindown offset (where disk stops)
  |     |   |   |     |       |      |   |     Select delay
  |     |   |   |     |       |      |   |     |     RPS
  |     |   |   |     |       |      |   |     |     |    Max number of tracks
  |     |   |   |     |       |      |   |     |     |    |     Interrupt timeout
  |     |   |   |     |       |      |   |     |     |    |     |   Max nonintlv. sectors
  |     |   |   |     |       |      |   |     |     |    |     |   | -Max Errors- flags */
{{0,  500, 16, 16, 8000,    1*HZ, 3*HZ,  0, SEL_DLY, 5,  80, 3*HZ, 20, {3,1,2,0,2}, 0,
      0, { 7, 4, 8, 2, 1, 5, 3,10}, 3*HZ/2, 0 }, "unknown" },

{{1,  300, 16, 16, 8000,    1*HZ, 3*HZ,  0, SEL_DLY, 5,  40, 3*HZ, 17, {3,1,2,0,2}, 0,
      0, { 1, 0, 0, 0, 0, 0, 0, 0}, 3*HZ/2, 1 }, "360K PC" }, /*5 1/4 360 KB PC*/

{{2,  500, 16, 16, 6000, 4*HZ/10, 3*HZ, 14, SEL_DLY, 6,  83, 3*HZ, 17, {3,1,2,0,2}, 0,
      0, { 2, 5, 6,23,10,20,12, 0}, 3*HZ/2, 2 }, "1.2M" }, /*5 1/4 HD AT*/

{{3,  250, 16, 16, 3000,    1*HZ, 3*HZ,  0, SEL_DLY, 5,  83, 3*HZ, 20, {3,1,2,0,2}, 0,
      0, { 4,22,21,30, 3, 0, 0, 0}, 3*HZ/2, 4 }, "720k" }, /*3 1/2 DD*/

{{4,  500, 16, 16, 4000, 4*HZ/10, 3*HZ, 10, SEL_DLY, 5,  83, 3*HZ, 20, {3,1,2,0,2}, 0,
      0, { 7, 4,25,22,31,21,29,11}, 3*HZ/2, 7 }, "1.44M" }, /*3 1/2 HD*/

{{5, 1000, 15,  8, 3000, 4*HZ/10, 3*HZ, 10, SEL_DLY, 5,  83, 3*HZ, 40, {3,1,2,0,2}, 0,
      0, { 7, 8, 4,25,28,22,31,21}, 3*HZ/2, 8 }, "2.88M AMI BIOS" }, /*3 1/2 ED*/

{{6, 1000, 15,  8, 3000, 4*HZ/10, 3*HZ, 10, SEL_DLY, 5,  83, 3*HZ, 40, {3,1,2,0,2}, 0,
      0, { 7, 8, 4,25,28,22,31,21}, 3*HZ/2, 8 }, "2.88M" } /*3 1/2 ED*/
/*    |  --autodetected formats---    |      |      |
 *    read_track                      |      |    Name printed when booting
 *				      |     Native format
 *	            Frequency of disk change checks */
};

static struct floppy_drive_params drive_params[N_DRIVE];
static struct floppy_drive_struct drive_state[N_DRIVE];
static struct floppy_write_errors write_errors[N_DRIVE];
static struct timer_list motor_off_timer[N_DRIVE];
static struct gendisk *disks[N_DRIVE];
static struct block_device *opened_bdev[N_DRIVE];
static DEFINE_MUTEX(open_lock);
static struct floppy_raw_cmd *raw_cmd, default_raw_cmd;

/*
 * This struct defines the different floppy types.
 *
 * Bit 0 of 'stretch' tells if the tracks need to be doubled for some
 * types (e.g. 360kB diskette in 1.2MB drive, etc.).  Bit 1 of 'stretch'
 * tells if the disk is in Commodore 1581 format, which means side 0 sectors
 * are located on side 1 of the disk but with a side 0 ID, and vice-versa.
 * This is the same as the Sharp MZ-80 5.25" CP/M disk format, except that the
 * 1581's logical side 0 is on physical side 1, whereas the Sharp's logical
 * side 0 is on physical side 0 (but with the misnamed sector IDs).
 * 'stretch' should probably be renamed to something more general, like
 * 'options'.
 *
 * Bits 2 through 9 of 'stretch' tell the number of the first sector.
 * The LSB (bit 2) is flipped. For most disks, the first sector
 * is 1 (represented by 0x00<<2).  For some CP/M and music sampler
 * disks (such as Ensoniq EPS 16plus) it is 0 (represented as 0x01<<2).
 * For Amstrad CPC disks it is 0xC1 (represented as 0xC0<<2).
 *
 * Other parameters should be self-explanatory (see also setfdprm(8)).
 */
/*
	    Size
	     |  Sectors per track
	     |  | Head
	     |  | |  Tracks
	     |  | |  | Stretch
	     |  | |  | |  Gap 1 size
	     |  | |  | |    |  Data rate, | 0x40 for perp
	     |  | |  | |    |    |  Spec1 (stepping rate, head unload
	     |  | |  | |    |    |    |    /fmt gap (gap2) */
static struct floppy_struct floppy_type[32] = {
	{    0, 0,0, 0,0,0x00,0x00,0x00,0x00,NULL    },	/*  0 no testing    */
	{  720, 9,2,40,0,0x2A,0x02,0xDF,0x50,"d360"  }, /*  1 360KB PC      */
	{ 2400,15,2,80,0,0x1B,0x00,0xDF,0x54,"h1200" },	/*  2 1.2MB AT      */
	{  720, 9,1,80,0,0x2A,0x02,0xDF,0x50,"D360"  },	/*  3 360KB SS 3.5" */
	{ 1440, 9,2,80,0,0x2A,0x02,0xDF,0x50,"D720"  },	/*  4 720KB 3.5"    */
	{  720, 9,2,40,1,0x23,0x01,0xDF,0x50,"h360"  },	/*  5 360KB AT      */
	{ 1440, 9,2,80,0,0x23,0x01,0xDF,0x50,"h720"  },	/*  6 720KB AT      */
	{ 2880,18,2,80,0,0x1B,0x00,0xCF,0x6C,"H1440" },	/*  7 1.44MB 3.5"   */
	{ 5760,36,2,80,0,0x1B,0x43,0xAF,0x54,"E2880" },	/*  8 2.88MB 3.5"   */
	{ 6240,39,2,80,0,0x1B,0x43,0xAF,0x28,"E3120" },	/*  9 3.12MB 3.5"   */

	{ 2880,18,2,80,0,0x25,0x00,0xDF,0x02,"h1440" }, /* 10 1.44MB 5.25"  */
	{ 3360,21,2,80,0,0x1C,0x00,0xCF,0x0C,"H1680" }, /* 11 1.68MB 3.5"   */
	{  820,10,2,41,1,0x25,0x01,0xDF,0x2E,"h410"  },	/* 12 410KB 5.25"   */
	{ 1640,10,2,82,0,0x25,0x02,0xDF,0x2E,"H820"  },	/* 13 820KB 3.5"    */
	{ 2952,18,2,82,0,0x25,0x00,0xDF,0x02,"h1476" },	/* 14 1.48MB 5.25"  */
	{ 3444,21,2,82,0,0x25,0x00,0xDF,0x0C,"H1722" },	/* 15 1.72MB 3.5"   */
	{  840,10,2,42,1,0x25,0x01,0xDF,0x2E,"h420"  },	/* 16 420KB 5.25"   */
	{ 1660,10,2,83,0,0x25,0x02,0xDF,0x2E,"H830"  },	/* 17 830KB 3.5"    */
	{ 2988,18,2,83,0,0x25,0x00,0xDF,0x02,"h1494" },	/* 18 1.49MB 5.25"  */
	{ 3486,21,2,83,0,0x25,0x00,0xDF,0x0C,"H1743" }, /* 19 1.74 MB 3.5"  */

	{ 1760,11,2,80,0,0x1C,0x09,0xCF,0x00,"h880"  }, /* 20 880KB 5.25"   */
	{ 2080,13,2,80,0,0x1C,0x01,0xCF,0x00,"D1040" }, /* 21 1.04MB 3.5"   */
	{ 2240,14,2,80,0,0x1C,0x19,0xCF,0x00,"D1120" }, /* 22 1.12MB 3.5"   */
	{ 3200,20,2,80,0,0x1C,0x20,0xCF,0x2C,"h1600" }, /* 23 1.6MB 5.25"   */
	{ 3520,22,2,80,0,0x1C,0x08,0xCF,0x2e,"H1760" }, /* 24 1.76MB 3.5"   */
	{ 3840,24,2,80,0,0x1C,0x20,0xCF,0x00,"H1920" }, /* 25 1.92MB 3.5"   */
	{ 6400,40,2,80,0,0x25,0x5B,0xCF,0x00,"E3200" }, /* 26 3.20MB 3.5"   */
	{ 7040,44,2,80,0,0x25,0x5B,0xCF,0x00,"E3520" }, /* 27 3.52MB 3.5"   */
	{ 7680,48,2,80,0,0x25,0x63,0xCF,0x00,"E3840" }, /* 28 3.84MB 3.5"   */
	{ 3680,23,2,80,0,0x1C,0x10,0xCF,0x00,"H1840" }, /* 29 1.84MB 3.5"   */

	{ 1600,10,2,80,0,0x25,0x02,0xDF,0x2E,"D800"  },	/* 30 800KB 3.5"    */
	{ 3200,20,2,80,0,0x1C,0x00,0xCF,0x2C,"H1600" }, /* 31 1.6MB 3.5"    */
};

#define SECTSIZE (_FD_SECTSIZE(*floppy))

/* Auto-detection: Disk type used until the next media change occurs. */
static struct floppy_struct *current_type[N_DRIVE];

/*
 * User-provided type information. current_type points to
 * the respective entry of this array.
 */
static struct floppy_struct user_params[N_DRIVE];

static sector_t floppy_sizes[256];

static char floppy_device_name[] = "floppy";

/*
 * The driver is trying to determine the correct media format
 * while probing is set. rw_interrupt() clears it after a
 * successful access.
 */
static int probing;

/* Synchronization of FDC access. */
#define FD_COMMAND_NONE -1
#define FD_COMMAND_ERROR 2
#define FD_COMMAND_OKAY 3

static volatile int command_status = FD_COMMAND_NONE;
static unsigned long fdc_busy;
static DECLARE_WAIT_QUEUE_HEAD(fdc_wait);
static DECLARE_WAIT_QUEUE_HEAD(command_done);

#define NO_SIGNAL (!interruptible || !signal_pending(current))
#define CALL(x) if ((x) == -EINTR) return -EINTR
#define ECALL(x) if ((ret = (x))) return ret;
#define _WAIT(x,i) CALL(ret=wait_til_done((x),i))
#define WAIT(x) _WAIT((x),interruptible)
#define IWAIT(x) _WAIT((x),1)

/* Errors during formatting are counted here. */
static int format_errors;

/* Format request descriptor. */
static struct format_descr format_req;

/*
 * Rate is 0 for 500kb/s, 1 for 300kbps, 2 for 250kbps
 * Spec1 is 0xSH, where S is stepping rate (F=1ms, E=2ms, D=3ms etc),
 * H is head unload time (1=16ms, 2=32ms, etc)
 */

/*
 * Track buffer
 * Because these are written to by the DMA controller, they must
 * not contain a 64k byte boundary crossing, or data will be
 * corrupted/lost.
 */
static char *floppy_track_buffer;
static int max_buffer_sectors;

static int *errors;
typedef void (*done_f)(int);
static struct cont_t {
	void (*interrupt)(void);	/* this is called after the interrupt of the
					 * main command */
	void (*redo)(void);	/* this is called to retry the operation */
	void (*error)(void);	/* this is called to tally an error */
	done_f done;		/* this is called to say if the operation has
				 * succeeded/failed */
} *cont;

static void floppy_ready(void);
static void floppy_start(void);
static void process_fd_request(void);
static void recalibrate_floppy(void);
static void floppy_shutdown(unsigned long);

static int floppy_request_regions(int);
static void floppy_release_regions(int);
static int floppy_grab_irq_and_dma(void);
static void floppy_release_irq_and_dma(void);

/*
 * The "reset" variable should be tested whenever an interrupt is scheduled,
 * after the commands have been sent. This is to ensure that the driver doesn't
 * get wedged when the interrupt doesn't come because of a failed command.
 * reset doesn't need to be tested before sending commands, because
 * output_byte is automatically disabled when reset is set.
 */
#define CHECK_RESET { if (FDCS->reset){ reset_fdc(); return; } }
static void reset_fdc(void);

/*
 * These are global variables, as that's the easiest way to give
 * information to interrupts. They are the data used for the current
 * request.
 */
#define NO_TRACK -1
#define NEED_1_RECAL -2
#define NEED_2_RECAL -3

static int usage_count;

/* buffer related variables */
static int buffer_track = -1;
static int buffer_drive = -1;
static int buffer_min = -1;
static int buffer_max = -1;

/* fdc related variables, should end up in a struct */
static struct floppy_fdc_state fdc_state[N_FDC];
static int fdc;			/* current fdc */

static struct floppy_struct *_floppy = floppy_type;
static unsigned char current_drive;
static long current_count_sectors;
static unsigned char fsector_t;	/* sector in track */
static unsigned char in_sector_offset;	/* offset within physical sector,
					 * expressed in units of 512 bytes */

#ifndef fd_eject
static inline int fd_eject(int drive)
{
	return -EINVAL;
}
#endif

/*
 * Debugging
 * =========
 */
#ifdef DEBUGT
static long unsigned debugtimer;

static inline void set_debugt(void)
{
	debugtimer = jiffies;
}

static inline void debugt(const char *message)
{
	if (DP->flags & DEBUGT)
		printk("%s dtime=%lu\n", message, jiffies - debugtimer);
}
#else
static inline void set_debugt(void) { }
static inline void debugt(const char *message) { }
#endif /* DEBUGT */

typedef void (*timeout_fn) (unsigned long);
static DEFINE_TIMER(fd_timeout, floppy_shutdown, 0, 0);

static const char *timeout_message;

#ifdef FLOPPY_SANITY_CHECK
static void is_alive(const char *message)
{
	/* this routine checks whether the floppy driver is "alive" */
	if (test_bit(0, &fdc_busy) && command_status < 2
	    && !timer_pending(&fd_timeout)) {
		DPRINT("timeout handler died: %s\n", message);
	}
}
#endif

static void (*do_floppy) (void) = NULL;

#ifdef FLOPPY_SANITY_CHECK

#define OLOGSIZE 20

static void (*lasthandler) (void);
static unsigned long interruptjiffies;
static unsigned long resultjiffies;
static int resultsize;
static unsigned long lastredo;

static struct output_log {
	unsigned char data;
	unsigned char status;
	unsigned long jiffies;
} output_log[OLOGSIZE];

static int output_log_pos;
#endif

#define current_reqD -1
#define MAXTIMEOUT -2

static void __reschedule_timeout(int drive, const char *message, int marg)
{
	if (drive == current_reqD)
		drive = current_drive;
	del_timer(&fd_timeout);
	if (drive < 0 || drive >= N_DRIVE) {
		fd_timeout.expires = jiffies + 20UL * HZ;
		drive = 0;
	} else
		fd_timeout.expires = jiffies + UDP->timeout;
	add_timer(&fd_timeout);
	if (UDP->flags & FD_DEBUG) {
		DPRINT("reschedule timeout ");
		printk(message, marg);
		printk("\n");
	}
	timeout_message = message;
}

static void reschedule_timeout(int drive, const char *message, int marg)
{
	unsigned long flags;

	spin_lock_irqsave(&floppy_lock, flags);
	__reschedule_timeout(drive, message, marg);
	spin_unlock_irqrestore(&floppy_lock, flags);
}

#define INFBOUND(a,b) (a)=max_t(int, a, b)
#define SUPBOUND(a,b) (a)=min_t(int, a, b)

/*
 * Bottom half floppy driver.
 * ==========================
 *
 * This part of the file contains the code talking directly to the hardware,
 * and also the main service loop (seek-configure-spinup-command)
 */

/*
 * disk change.
 * This routine is responsible for maintaining the FD_DISK_CHANGE flag,
 * and the last_checked date.
 *
 * last_checked is the date of the last check which showed 'no disk change'
 * FD_DISK_CHANGE is set under two conditions:
 * 1. The floppy has been changed after some i/o to that floppy already
 *    took place.
 * 2. No floppy disk is in the drive. This is done in order to ensure that
 *    requests are quickly flushed in case there is no disk in the drive. It
 *    follows that FD_DISK_CHANGE can only be cleared if there is a disk in
 *    the drive.
 *
 * For 1., maxblock is observed. Maxblock is 0 if no i/o has taken place yet.
 * For 2., FD_DISK_NEWCHANGE is watched. FD_DISK_NEWCHANGE is cleared on
 *  each seek. If a disk is present, the disk change line should also be
 *  cleared on each seek. Thus, if FD_DISK_NEWCHANGE is clear, but the disk
 *  change line is set, this means either that no disk is in the drive, or
 *  that it has been removed since the last seek.
 *
 * This means that we really have a third possibility too:
 *  The floppy has been changed after the last seek.
 */

static int disk_change(int drive)
{
	int fdc = FDC(drive);

#ifdef FLOPPY_SANITY_CHECK
	if (time_before(jiffies, UDRS->select_date + UDP->select_delay))
		DPRINT("WARNING disk change called early\n");
	if (!(FDCS->dor & (0x10 << UNIT(drive))) ||
	    (FDCS->dor & 3) != UNIT(drive) || fdc != FDC(drive)) {
		DPRINT("probing disk change on unselected drive\n");
		DPRINT("drive=%d fdc=%d dor=%x\n", drive, FDC(drive),
		       (unsigned int)FDCS->dor);
	}
#endif

#ifdef DCL_DEBUG
	if (UDP->flags & FD_DEBUG) {
		DPRINT("checking disk change line for drive %d\n", drive);
		DPRINT("jiffies=%lu\n", jiffies);
		DPRINT("disk change line=%x\n", fd_inb(FD_DIR) & 0x80);
		DPRINT("flags=%lx\n", UDRS->flags);
	}
#endif
	if (UDP->flags & FD_BROKEN_DCL)
		return UTESTF(FD_DISK_CHANGED);
	if ((fd_inb(FD_DIR) ^ UDP->flags) & 0x80) {
		USETF(FD_VERIFY);	/* verify write protection */
		if (UDRS->maxblock) {
			/* mark it changed */
			USETF(FD_DISK_CHANGED);
		}

		/* invalidate its geometry */
		if (UDRS->keep_data >= 0) {
			if ((UDP->flags & FTD_MSG) &&
			    current_type[drive] != NULL)
				DPRINT("Disk type is undefined after "
				       "disk change\n");
			current_type[drive] = NULL;
			floppy_sizes[TOMINOR(drive)] = MAX_DISK_SIZE << 1;
		}

		return 1;
	} else {
		UDRS->last_checked = jiffies;
		UCLEARF(FD_DISK_NEWCHANGE);
	}
	return 0;
}

static inline int is_selected(int dor, int unit)
{
	return ((dor & (0x10 << unit)) && (dor & 3) == unit);
}

static int set_dor(int fdc, char mask, char data)
{
	unsigned char unit;
	unsigned char drive;
	unsigned char newdor;
	unsigned char olddor;

	if (FDCS->address == -1)
		return -1;

	olddor = FDCS->dor;
	newdor = (olddor & mask) | data;
	if (newdor != olddor) {
		unit = olddor & 0x3;
		if (is_selected(olddor, unit) && !is_selected(newdor, unit)) {
			drive = REVDRIVE(fdc, unit);
#ifdef DCL_DEBUG
			if (UDP->flags & FD_DEBUG) {
				DPRINT("calling disk change from set_dor\n");
			}
#endif
			disk_change(drive);
		}
		FDCS->dor = newdor;
		fd_outb(newdor, FD_DOR);

		unit = newdor & 0x3;
		if (!is_selected(olddor, unit) && is_selected(newdor, unit)) {
			drive = REVDRIVE(fdc, unit);
			UDRS->select_date = jiffies;
		}
	}
	return olddor;
}

static void twaddle(void)
{
	if (DP->select_delay)
		return;
	fd_outb(FDCS->dor & ~(0x10 << UNIT(current_drive)), FD_DOR);
	fd_outb(FDCS->dor, FD_DOR);
	DRS->select_date = jiffies;
}

/* reset all driver information about the current fdc. This is needed after
 * a reset, and after a raw command. */
static void reset_fdc_info(int mode)
{
	int drive;

	FDCS->spec1 = FDCS->spec2 = -1;
	FDCS->need_configure = 1;
	FDCS->perp_mode = 1;
	FDCS->rawcmd = 0;
	for (drive = 0; drive < N_DRIVE; drive++)
		if (FDC(drive) == fdc && (mode || UDRS->track != NEED_1_RECAL))
			UDRS->track = NEED_2_RECAL;
}

/* selects the fdc and drive, and enables the fdc's input/dma. */
static void set_fdc(int drive)
{
	if (drive >= 0 && drive < N_DRIVE) {
		fdc = FDC(drive);
		current_drive = drive;
	}
	if (fdc != 1 && fdc != 0) {
		printk("bad fdc value\n");
		return;
	}
	set_dor(fdc, ~0, 8);
#if N_FDC > 1
	set_dor(1 - fdc, ~8, 0);
#endif
	if (FDCS->rawcmd == 2)
		reset_fdc_info(1);
	if (fd_inb(FD_STATUS) != STATUS_READY)
		FDCS->reset = 1;
}

/* locks the driver */
static int _lock_fdc(int drive, int interruptible, int line)
{
	if (!usage_count) {
		printk(KERN_ERR
		       "Trying to lock fdc while usage count=0 at line %d\n",
		       line);
		return -1;
	}

	if (test_and_set_bit(0, &fdc_busy)) {
		DECLARE_WAITQUEUE(wait, current);
		add_wait_queue(&fdc_wait, &wait);

		for (;;) {
			set_current_state(TASK_INTERRUPTIBLE);

			if (!test_and_set_bit(0, &fdc_busy))
				break;

			schedule();

			if (!NO_SIGNAL) {
				remove_wait_queue(&fdc_wait, &wait);
				return -EINTR;
			}
		}

		set_current_state(TASK_RUNNING);
		remove_wait_queue(&fdc_wait, &wait);
		flush_scheduled_work();
	}
	command_status = FD_COMMAND_NONE;

	__reschedule_timeout(drive, "lock fdc", 0);
	set_fdc(drive);
	return 0;
}

#define lock_fdc(drive,interruptible) _lock_fdc(drive,interruptible, __LINE__)

#define LOCK_FDC(drive,interruptible) \
if (lock_fdc(drive,interruptible)) return -EINTR;

/* unlocks the driver */
static inline void unlock_fdc(void)
{
	unsigned long flags;

	raw_cmd = NULL;
	if (!test_bit(0, &fdc_busy))
		DPRINT("FDC access conflict!\n");

	if (do_floppy)
		DPRINT("device interrupt still active at FDC release: %p!\n",
		       do_floppy);
	command_status = FD_COMMAND_NONE;
	spin_lock_irqsave(&floppy_lock, flags);
	del_timer(&fd_timeout);
	cont = NULL;
	clear_bit(0, &fdc_busy);
	if (current_req || blk_peek_request(floppy_queue))
		do_fd_request(floppy_queue);
	spin_unlock_irqrestore(&floppy_lock, flags);
	wake_up(&fdc_wait);
}

/* switches the motor off after a given timeout */
static void motor_off_callback(unsigned long nr)
{
	unsigned char mask = ~(0x10 << UNIT(nr));

	set_dor(FDC(nr), mask, 0);
}

/* schedules motor off */
static void floppy_off(unsigned int drive)
{
	unsigned long volatile delta;
	int fdc = FDC(drive);

	if (!(FDCS->dor & (0x10 << UNIT(drive))))
		return;

	del_timer(motor_off_timer + drive);

	/* make spindle stop in a position which minimizes spinup time
	 * next time */
	if (UDP->rps) {
		delta = jiffies - UDRS->first_read_date + HZ -
		    UDP->spindown_offset;
		delta = ((delta * UDP->rps) % HZ) / UDP->rps;
		motor_off_timer[drive].expires =
		    jiffies + UDP->spindown - delta;
	}
	add_timer(motor_off_timer + drive);
}

/*
 * cycle through all N_DRIVE floppy drives, for disk change testing.
 * stopping at current drive. This is done before any long operation, to
 * be sure to have up to date disk change information.
 */
static void scandrives(void)
{
	int i;
	int drive;
	int saved_drive;

	if (DP->select_delay)
		return;

	saved_drive = current_drive;
	for (i = 0; i < N_DRIVE; i++) {
		drive = (saved_drive + i + 1) % N_DRIVE;
		if (UDRS->fd_ref == 0 || UDP->select_delay != 0)
			continue;	/* skip closed drives */
		set_fdc(drive);
		if (!(set_dor(fdc, ~3, UNIT(drive) | (0x10 << UNIT(drive))) &
		      (0x10 << UNIT(drive))))
			/* switch the motor off again, if it was off to
			 * begin with */
			set_dor(fdc, ~(0x10 << UNIT(drive)), 0);
	}
	set_fdc(saved_drive);
}

static void empty(void)
{
}

static DECLARE_WORK(floppy_work, NULL);

static void schedule_bh(void (*handler) (void))
{
	PREPARE_WORK(&floppy_work, (work_func_t)handler);
	schedule_work(&floppy_work);
}

static DEFINE_TIMER(fd_timer, NULL, 0, 0);

static void cancel_activity(void)
{
	unsigned long flags;

	spin_lock_irqsave(&floppy_lock, flags);
	do_floppy = NULL;
	PREPARE_WORK(&floppy_work, (work_func_t)empty);
	del_timer(&fd_timer);
	spin_unlock_irqrestore(&floppy_lock, flags);
}

/* this function makes sure that the disk stays in the drive during the
 * transfer */
static void fd_watchdog(void)
{
#ifdef DCL_DEBUG
	if (DP->flags & FD_DEBUG) {
		DPRINT("calling disk change from watchdog\n");
	}
#endif

	if (disk_change(current_drive)) {
		DPRINT("disk removed during i/o\n");
		cancel_activity();
		cont->done(0);
		reset_fdc();
	} else {
		del_timer(&fd_timer);
		fd_timer.function = (timeout_fn) fd_watchdog;
		fd_timer.expires = jiffies + HZ / 10;
		add_timer(&fd_timer);
	}
}

static void main_command_interrupt(void)
{
	del_timer(&fd_timer);
	cont->interrupt();
}

/* waits for a delay (spinup or select) to pass */
static int fd_wait_for_completion(unsigned long delay, timeout_fn function)
{
	if (FDCS->reset) {
		reset_fdc();	/* do the reset during sleep to win time
				 * if we don't need to sleep, it's a good
				 * occasion anyways */
		return 1;
	}

	if (time_before(jiffies, delay)) {
		del_timer(&fd_timer);
		fd_timer.function = function;
		fd_timer.expires = delay;
		add_timer(&fd_timer);
		return 1;
	}
	return 0;
}

static DEFINE_SPINLOCK(floppy_hlt_lock);
static int hlt_disabled;
static void floppy_disable_hlt(void)
{
	unsigned long flags;

	spin_lock_irqsave(&floppy_hlt_lock, flags);
	if (!hlt_disabled) {
		hlt_disabled = 1;
#ifdef HAVE_DISABLE_HLT
		disable_hlt();
#endif
	}
	spin_unlock_irqrestore(&floppy_hlt_lock, flags);
}

static void floppy_enable_hlt(void)
{
	unsigned long flags;

	spin_lock_irqsave(&floppy_hlt_lock, flags);
	if (hlt_disabled) {
		hlt_disabled = 0;
#ifdef HAVE_DISABLE_HLT
		enable_hlt();
#endif
	}
	spin_unlock_irqrestore(&floppy_hlt_lock, flags);
}

static void setup_DMA(void)
{
	unsigned long f;

#ifdef FLOPPY_SANITY_CHECK
	if (raw_cmd->length == 0) {
		int i;

		printk("zero dma transfer size:");
		for (i = 0; i < raw_cmd->cmd_count; i++)
			printk("%x,", raw_cmd->cmd[i]);
		printk("\n");
		cont->done(0);
		FDCS->reset = 1;
		return;
	}
	if (((unsigned long)raw_cmd->kernel_data) % 512) {
		printk("non aligned address: %p\n", raw_cmd->kernel_data);
		cont->done(0);
		FDCS->reset = 1;
		return;
	}
#endif
	f = claim_dma_lock();
	fd_disable_dma();
#ifdef fd_dma_setup
	if (fd_dma_setup(raw_cmd->kernel_data, raw_cmd->length,
			 (raw_cmd->flags & FD_RAW_READ) ?
			 DMA_MODE_READ : DMA_MODE_WRITE, FDCS->address) < 0) {
		release_dma_lock(f);
		cont->done(0);
		FDCS->reset = 1;
		return;
	}
	release_dma_lock(f);
#else
	fd_clear_dma_ff();
	fd_cacheflush(raw_cmd->kernel_data, raw_cmd->length);
	fd_set_dma_mode((raw_cmd->flags & FD_RAW_READ) ?
			DMA_MODE_READ : DMA_MODE_WRITE);
	fd_set_dma_addr(raw_cmd->kernel_data);
	fd_set_dma_count(raw_cmd->length);
	virtual_dma_port = FDCS->address;
	fd_enable_dma();
	release_dma_lock(f);
#endif
	floppy_disable_hlt();
}

static void show_floppy(void);

/* waits until the fdc becomes ready */
static int wait_til_ready(void)
{
	int status;
	int counter;

	if (FDCS->reset)
		return -1;
	for (counter = 0; counter < 10000; counter++) {
		status = fd_inb(FD_STATUS);
		if (status & STATUS_READY)
			return status;
	}
	if (!initialising) {
		DPRINT("Getstatus times out (%x) on fdc %d\n", status, fdc);
		show_floppy();
	}
	FDCS->reset = 1;
	return -1;
}

/* sends a command byte to the fdc */
static int output_byte(char byte)
{
	int status;

	if ((status = wait_til_ready()) < 0)
		return -1;
	if ((status & (STATUS_READY | STATUS_DIR | STATUS_DMA)) == STATUS_READY) {
		fd_outb(byte, FD_DATA);
#ifdef FLOPPY_SANITY_CHECK
		output_log[output_log_pos].data = byte;
		output_log[output_log_pos].status = status;
		output_log[output_log_pos].jiffies = jiffies;
		output_log_pos = (output_log_pos + 1) % OLOGSIZE;
#endif
		return 0;
	}
	FDCS->reset = 1;
	if (!initialising) {
		DPRINT("Unable to send byte %x to FDC. Fdc=%x Status=%x\n",
		       byte, fdc, status);
		show_floppy();
	}
	return -1;
}

#define LAST_OUT(x) if (output_byte(x)<0){ reset_fdc();return;}

/* gets the response from the fdc */
static int result(void)
{
	int i;
	int status = 0;

	for (i = 0; i < MAX_REPLIES; i++) {
		if ((status = wait_til_ready()) < 0)
			break;
		status &= STATUS_DIR | STATUS_READY | STATUS_BUSY | STATUS_DMA;
		if ((status & ~STATUS_BUSY) == STATUS_READY) {
#ifdef FLOPPY_SANITY_CHECK
			resultjiffies = jiffies;
			resultsize = i;
#endif
			return i;
		}
		if (status == (STATUS_DIR | STATUS_READY | STATUS_BUSY))
			reply_buffer[i] = fd_inb(FD_DATA);
		else
			break;
	}
	if (!initialising) {
		DPRINT
		    ("get result error. Fdc=%d Last status=%x Read bytes=%d\n",
		     fdc, status, i);
		show_floppy();
	}
	FDCS->reset = 1;
	return -1;
}

#define MORE_OUTPUT -2
/* does the fdc need more output? */
static int need_more_output(void)
{
	int status;

	if ((status = wait_til_ready()) < 0)
		return -1;
	if ((status & (STATUS_READY | STATUS_DIR | STATUS_DMA)) == STATUS_READY)
		return MORE_OUTPUT;
	return result();
}

/* Set perpendicular mode as required, based on data rate, if supported.
 * 82077 Now tested. 1Mbps data rate only possible with 82077-1.
 */
static inline void perpendicular_mode(void)
{
	unsigned char perp_mode;

	if (raw_cmd->rate & 0x40) {
		switch (raw_cmd->rate & 3) {
		case 0:
			perp_mode = 2;
			break;
		case 3:
			perp_mode = 3;
			break;
		default:
			DPRINT("Invalid data rate for perpendicular mode!\n");
			cont->done(0);
			FDCS->reset = 1;	/* convenient way to return to
						 * redo without to much hassle (deep
						 * stack et al. */
			return;
		}
	} else
		perp_mode = 0;

	if (FDCS->perp_mode == perp_mode)
		return;
	if (FDCS->version >= FDC_82077_ORIG) {
		output_byte(FD_PERPENDICULAR);
		output_byte(perp_mode);
		FDCS->perp_mode = perp_mode;
	} else if (perp_mode) {
		DPRINT("perpendicular mode not supported by this FDC.\n");
	}
}				/* perpendicular_mode */

static int fifo_depth = 0xa;
static int no_fifo;

static int fdc_configure(void)
{
	/* Turn on FIFO */
	output_byte(FD_CONFIGURE);
	if (need_more_output() != MORE_OUTPUT)
		return 0;
	output_byte(0);
	output_byte(0x10 | (no_fifo & 0x20) | (fifo_depth & 0xf));
	output_byte(0);		/* pre-compensation from track
				   0 upwards */
	return 1;
}

#define NOMINAL_DTR 500

/* Issue a "SPECIFY" command to set the step rate time, head unload time,
 * head load time, and DMA disable flag to values needed by floppy.
 *
 * The value "dtr" is the data transfer rate in Kbps.  It is needed
 * to account for the data rate-based scaling done by the 82072 and 82077
 * FDC types.  This parameter is ignored for other types of FDCs (i.e.
 * 8272a).
 *
 * Note that changing the data transfer rate has a (probably deleterious)
 * effect on the parameters subject to scaling for 82072/82077 FDCs, so
 * fdc_specify is called again after each data transfer rate
 * change.
 *
 * srt: 1000 to 16000 in microseconds
 * hut: 16 to 240 milliseconds
 * hlt: 2 to 254 milliseconds
 *
 * These values are rounded up to the next highest available delay time.
 */
static void fdc_specify(void)
{
	unsigned char spec1;
	unsigned char spec2;
	unsigned long srt;
	unsigned long hlt;
	unsigned long hut;
	unsigned long dtr = NOMINAL_DTR;
	unsigned long scale_dtr = NOMINAL_DTR;
	int hlt_max_code = 0x7f;
	int hut_max_code = 0xf;

	if (FDCS->need_configure && FDCS->version >= FDC_82072A) {
		fdc_configure();
		FDCS->need_configure = 0;
	}

	switch (raw_cmd->rate & 0x03) {
	case 3:
		dtr = 1000;
		break;
	case 1:
		dtr = 300;
		if (FDCS->version >= FDC_82078) {
			/* chose the default rate table, not the one
			 * where 1 = 2 Mbps */
			output_byte(FD_DRIVESPEC);
			if (need_more_output() == MORE_OUTPUT) {
				output_byte(UNIT(current_drive));
				output_byte(0xc0);
			}
		}
		break;
	case 2:
		dtr = 250;
		break;
	}

	if (FDCS->version >= FDC_82072) {
		scale_dtr = dtr;
		hlt_max_code = 0x00;	/* 0==256msec*dtr0/dtr (not linear!) */
		hut_max_code = 0x0;	/* 0==256msec*dtr0/dtr (not linear!) */
	}

	/* Convert step rate from microseconds to milliseconds and 4 bits */
	srt = 16 - DIV_ROUND_UP(DP->srt * scale_dtr / 1000, NOMINAL_DTR);
	if (slow_floppy) {
		srt = srt / 4;
	}
	SUPBOUND(srt, 0xf);
	INFBOUND(srt, 0);

	hlt = DIV_ROUND_UP(DP->hlt * scale_dtr / 2, NOMINAL_DTR);
	if (hlt < 0x01)
		hlt = 0x01;
	else if (hlt > 0x7f)
		hlt = hlt_max_code;

	hut = DIV_ROUND_UP(DP->hut * scale_dtr / 16, NOMINAL_DTR);
	if (hut < 0x1)
		hut = 0x1;
	else if (hut > 0xf)
		hut = hut_max_code;

	spec1 = (srt << 4) | hut;
	spec2 = (hlt << 1) | (use_virtual_dma & 1);

	/* If these parameters did not change, just return with success */
	if (FDCS->spec1 != spec1 || FDCS->spec2 != spec2) {
		/* Go ahead and set spec1 and spec2 */
		output_byte(FD_SPECIFY);
		output_byte(FDCS->spec1 = spec1);
		output_byte(FDCS->spec2 = spec2);
	}
}				/* fdc_specify */

/* Set the FDC's data transfer rate on behalf of the specified drive.
 * NOTE: with 82072/82077 FDCs, changing the data rate requires a reissue
 * of the specify command (i.e. using the fdc_specify function).
 */
static int fdc_dtr(void)
{
	/* If data rate not already set to desired value, set it. */
	if ((raw_cmd->rate & 3) == FDCS->dtr)
		return 0;

	/* Set dtr */
	fd_outb(raw_cmd->rate & 3, FD_DCR);

	/* TODO: some FDC/drive combinations (C&T 82C711 with TEAC 1.2MB)
	 * need a stabilization period of several milliseconds to be
	 * enforced after data rate changes before R/W operations.
	 * Pause 5 msec to avoid trouble. (Needs to be 2 jiffies)
	 */
	FDCS->dtr = raw_cmd->rate & 3;
	return (fd_wait_for_completion(jiffies + 2UL * HZ / 100,
				       (timeout_fn) floppy_ready));
}				/* fdc_dtr */

static void tell_sector(void)
{
	printk(": track %d, head %d, sector %d, size %d",
	       R_TRACK, R_HEAD, R_SECTOR, R_SIZECODE);
}				/* tell_sector */

/*
 * OK, this error interpreting routine is called after a
 * DMA read/write has succeeded
 * or failed, so we check the results, and copy any buffers.
 * hhb: Added better error reporting.
 * ak: Made this into a separate routine.
 */
static int interpret_errors(void)
{
	char bad;

	if (inr != 7) {
		DPRINT("-- FDC reply error");
		FDCS->reset = 1;
		return 1;
	}

	/* check IC to find cause of interrupt */
	switch (ST0 & ST0_INTR) {
	case 0x40:		/* error occurred during command execution */
		if (ST1 & ST1_EOC)
			return 0;	/* occurs with pseudo-DMA */
		bad = 1;
		if (ST1 & ST1_WP) {
			DPRINT("Drive is write protected\n");
			CLEARF(FD_DISK_WRITABLE);
			cont->done(0);
			bad = 2;
		} else if (ST1 & ST1_ND) {
			SETF(FD_NEED_TWADDLE);
		} else if (ST1 & ST1_OR) {
			if (DP->flags & FTD_MSG)
				DPRINT("Over/Underrun - retrying\n");
			bad = 0;
		} else if (*errors >= DP->max_errors.reporting) {
			DPRINT("");
			if (ST0 & ST0_ECE) {
				printk("Recalibrate failed!");
			} else if (ST2 & ST2_CRC) {
				printk("data CRC error");
				tell_sector();
			} else if (ST1 & ST1_CRC) {
				printk("CRC error");
				tell_sector();
			} else if ((ST1 & (ST1_MAM | ST1_ND))
				   || (ST2 & ST2_MAM)) {
				if (!probing) {
					printk("sector not found");
					tell_sector();
				} else
					printk("probe failed...");
			} else if (ST2 & ST2_WC) {	/* seek error */
				printk("wrong cylinder");
			} else if (ST2 & ST2_BC) {	/* cylinder marked as bad */
				printk("bad cylinder");
			} else {
				printk
				    ("unknown error. ST[0..2] are: 0x%x 0x%x 0x%x",
				     ST0, ST1, ST2);
				tell_sector();
			}
			printk("\n");
		}
		if (ST2 & ST2_WC || ST2 & ST2_BC)
			/* wrong cylinder => recal */
			DRS->track = NEED_2_RECAL;
		return bad;
	case 0x80:		/* invalid command given */
		DPRINT("Invalid FDC command given!\n");
		cont->done(0);
		return 2;
	case 0xc0:
		DPRINT("Abnormal termination caused by polling\n");
		cont->error();
		return 2;
	default:		/* (0) Normal command termination */
		return 0;
	}
}

/*
 * This routine is called when everything should be correctly set up
 * for the transfer (i.e. floppy motor is on, the correct floppy is
 * selected, and the head is sitting on the right track).
 */
static void setup_rw_floppy(void)
{
	int i;
	int r;
	int flags;
	int dflags;
	unsigned long ready_date;
	timeout_fn function;

	flags = raw_cmd->flags;
	if (flags & (FD_RAW_READ | FD_RAW_WRITE))
		flags |= FD_RAW_INTR;

	if ((flags & FD_RAW_SPIN) && !(flags & FD_RAW_NO_MOTOR)) {
		ready_date = DRS->spinup_date + DP->spinup;
		/* If spinup will take a long time, rerun scandrives
		 * again just before spinup completion. Beware that
		 * after scandrives, we must again wait for selection.
		 */
		if (time_after(ready_date, jiffies + DP->select_delay)) {
			ready_date -= DP->select_delay;
			function = (timeout_fn) floppy_start;
		} else
			function = (timeout_fn) setup_rw_floppy;

		/* wait until the floppy is spinning fast enough */
		if (fd_wait_for_completion(ready_date, function))
			return;
	}
	dflags = DRS->flags;

	if ((flags & FD_RAW_READ) || (flags & FD_RAW_WRITE))
		setup_DMA();

	if (flags & FD_RAW_INTR)
		do_floppy = main_command_interrupt;

	r = 0;
	for (i = 0; i < raw_cmd->cmd_count; i++)
		r |= output_byte(raw_cmd->cmd[i]);

	debugt("rw_command: ");

	if (r) {
		cont->error();
		reset_fdc();
		return;
	}

	if (!(flags & FD_RAW_INTR)) {
		inr = result();
		cont->interrupt();
	} else if (flags & FD_RAW_NEED_DISK)
		fd_watchdog();
}

static int blind_seek;

/*
 * This is the routine called after every seek (or recalibrate) interrupt
 * from the floppy controller.
 */
static void seek_interrupt(void)
{
	debugt("seek interrupt:");
	if (inr != 2 || (ST0 & 0xF8) != 0x20) {
		DPRINT("seek failed\n");
		DRS->track = NEED_2_RECAL;
		cont->error();
		cont->redo();
		return;
	}
	if (DRS->track >= 0 && DRS->track != ST1 && !blind_seek) {
#ifdef DCL_DEBUG
		if (DP->flags & FD_DEBUG) {
			DPRINT
			    ("clearing NEWCHANGE flag because of effective seek\n");
			DPRINT("jiffies=%lu\n", jiffies);
		}
#endif
		CLEARF(FD_DISK_NEWCHANGE);	/* effective seek */
		DRS->select_date = jiffies;
	}
	DRS->track = ST1;
	floppy_ready();
}

static void check_wp(void)
{
	if (TESTF(FD_VERIFY)) {
		/* check write protection */
		output_byte(FD_GETSTATUS);
		output_byte(UNIT(current_drive));
		if (result() != 1) {
			FDCS->reset = 1;
			return;
		}
		CLEARF(FD_VERIFY);
		CLEARF(FD_NEED_TWADDLE);
#ifdef DCL_DEBUG
		if (DP->flags & FD_DEBUG) {
			DPRINT("checking whether disk is write protected\n");
			DPRINT("wp=%x\n", ST3 & 0x40);
		}
#endif
		if (!(ST3 & 0x40))
			SETF(FD_DISK_WRITABLE);
		else
			CLEARF(FD_DISK_WRITABLE);
	}
}

static void seek_floppy(void)
{
	int track;

	blind_seek = 0;

#ifdef DCL_DEBUG
	if (DP->flags & FD_DEBUG) {
		DPRINT("calling disk change from seek\n");
	}
#endif

	if (!TESTF(FD_DISK_NEWCHANGE) &&
	    disk_change(current_drive) && (raw_cmd->flags & FD_RAW_NEED_DISK)) {
		/* the media changed flag should be cleared after the seek.
		 * If it isn't, this means that there is really no disk in
		 * the drive.
		 */
		SETF(FD_DISK_CHANGED);
		cont->done(0);
		cont->redo();
		return;
	}
	if (DRS->track <= NEED_1_RECAL) {
		recalibrate_floppy();
		return;
	} else if (TESTF(FD_DISK_NEWCHANGE) &&
		   (raw_cmd->flags & FD_RAW_NEED_DISK) &&
		   (DRS->track <= NO_TRACK || DRS->track == raw_cmd->track)) {
		/* we seek to clear the media-changed condition. Does anybody
		 * know a more elegant way, which works on all drives? */
		if (raw_cmd->track)
			track = raw_cmd->track - 1;
		else {
			if (DP->flags & FD_SILENT_DCL_CLEAR) {
				set_dor(fdc, ~(0x10 << UNIT(current_drive)), 0);
				blind_seek = 1;
				raw_cmd->flags |= FD_RAW_NEED_SEEK;
			}
			track = 1;
		}
	} else {
		check_wp();
		if (raw_cmd->track != DRS->track &&
		    (raw_cmd->flags & FD_RAW_NEED_SEEK))
			track = raw_cmd->track;
		else {
			setup_rw_floppy();
			return;
		}
	}

	do_floppy = seek_interrupt;
	output_byte(FD_SEEK);
	output_byte(UNIT(current_drive));
	LAST_OUT(track);
	debugt("seek command:");
}

static void recal_interrupt(void)
{
	debugt("recal interrupt:");
	if (inr != 2)
		FDCS->reset = 1;
	else if (ST0 & ST0_ECE) {
		switch (DRS->track) {
		case NEED_1_RECAL:
			debugt("recal interrupt need 1 recal:");
			/* after a second recalibrate, we still haven't
			 * reached track 0. Probably no drive. Raise an
			 * error, as failing immediately might upset
			 * computers possessed by the Devil :-) */
			cont->error();
			cont->redo();
			return;
		case NEED_2_RECAL:
			debugt("recal interrupt need 2 recal:");
			/* If we already did a recalibrate,
			 * and we are not at track 0, this
			 * means we have moved. (The only way
			 * not to move at recalibration is to
			 * be already at track 0.) Clear the
			 * new change flag */
#ifdef DCL_DEBUG
			if (DP->flags & FD_DEBUG) {
				DPRINT
				    ("clearing NEWCHANGE flag because of second recalibrate\n");
			}
#endif

			CLEARF(FD_DISK_NEWCHANGE);
			DRS->select_date = jiffies;
			/* fall through */
		default:
			debugt("recal interrupt default:");
			/* Recalibrate moves the head by at
			 * most 80 steps. If after one
			 * recalibrate we don't have reached
			 * track 0, this might mean that we
			 * started beyond track 80.  Try
			 * again.  */
			DRS->track = NEED_1_RECAL;
			break;
		}
	} else
		DRS->track = ST1;
	floppy_ready();
}

static void print_result(char *message, int inr)
{
	int i;

	DPRINT("%s ", message);
	if (inr >= 0)
		for (i = 0; i < inr; i++)
			printk("repl[%d]=%x ", i, reply_buffer[i]);
	printk("\n");
}

/* interrupt handler. Note that this can be called externally on the Sparc */
irqreturn_t floppy_interrupt(int irq, void *dev_id)
{
	int do_print;
	unsigned long f;
	void (*handler)(void) = do_floppy;

	lasthandler = handler;
	interruptjiffies = jiffies;

	f = claim_dma_lock();
	fd_disable_dma();
	release_dma_lock(f);

	floppy_enable_hlt();
	do_floppy = NULL;
	if (fdc >= N_FDC || FDCS->address == -1) {
		/* we don't even know which FDC is the culprit */
		printk("DOR0=%x\n", fdc_state[0].dor);
		printk("floppy interrupt on bizarre fdc %d\n", fdc);
		printk("handler=%p\n", handler);
		is_alive("bizarre fdc");
		return IRQ_NONE;
	}

	FDCS->reset = 0;
	/* We have to clear the reset flag here, because apparently on boxes
	 * with level triggered interrupts (PS/2, Sparc, ...), it is needed to
	 * emit SENSEI's to clear the interrupt line. And FDCS->reset blocks the
	 * emission of the SENSEI's.
	 * It is OK to emit floppy commands because we are in an interrupt
	 * handler here, and thus we have to fear no interference of other
	 * activity.
	 */

	do_print = !handler && print_unex && !initialising;

	inr = result();
	if (do_print)
		print_result("unexpected interrupt", inr);
	if (inr == 0) {
		int max_sensei = 4;
		do {
			output_byte(FD_SENSEI);
			inr = result();
			if (do_print)
				print_result("sensei", inr);
			max_sensei--;
		} while ((ST0 & 0x83) != UNIT(current_drive) && inr == 2
			 && max_sensei);
	}
	if (!handler) {
		FDCS->reset = 1;
		return IRQ_NONE;
	}
	schedule_bh(handler);
	is_alive("normal interrupt end");

	/* FIXME! Was it really for us? */
	return IRQ_HANDLED;
}

static void recalibrate_floppy(void)
{
	debugt("recalibrate floppy:");
	do_floppy = recal_interrupt;
	output_byte(FD_RECALIBRATE);
	LAST_OUT(UNIT(current_drive));
}

/*
 * Must do 4 FD_SENSEIs after reset because of ``drive polling''.
 */
static void reset_interrupt(void)
{
	debugt("reset interrupt:");
	result();		/* get the status ready for set_fdc */
	if (FDCS->reset) {
		printk("reset set in interrupt, calling %p\n", cont->error);
		cont->error();	/* a reset just after a reset. BAD! */
	}
	cont->redo();
}

/*
 * reset is done by pulling bit 2 of DOR low for a while (old FDCs),
 * or by setting the self clearing bit 7 of STATUS (newer FDCs)
 */
static void reset_fdc(void)
{
	unsigned long flags;

	do_floppy = reset_interrupt;
	FDCS->reset = 0;
	reset_fdc_info(0);

	/* Pseudo-DMA may intercept 'reset finished' interrupt.  */
	/* Irrelevant for systems with true DMA (i386).          */

	flags = claim_dma_lock();
	fd_disable_dma();
	release_dma_lock(flags);

	if (FDCS->version >= FDC_82072A)
		fd_outb(0x80 | (FDCS->dtr & 3), FD_STATUS);
	else {
		fd_outb(FDCS->dor & ~0x04, FD_DOR);
		udelay(FD_RESET_DELAY);
		fd_outb(FDCS->dor, FD_DOR);
	}
}

static void show_floppy(void)
{
	int i;

	printk("\n");
	printk("floppy driver state\n");
	printk("-------------------\n");
	printk("now=%lu last interrupt=%lu diff=%lu last called handler=%p\n",
	       jiffies, interruptjiffies, jiffies - interruptjiffies,
	       lasthandler);

#ifdef FLOPPY_SANITY_CHECK
	printk("timeout_message=%s\n", timeout_message);
	printk("last output bytes:\n");
	for (i = 0; i < OLOGSIZE; i++)
		printk("%2x %2x %lu\n",
		       output_log[(i + output_log_pos) % OLOGSIZE].data,
		       output_log[(i + output_log_pos) % OLOGSIZE].status,
		       output_log[(i + output_log_pos) % OLOGSIZE].jiffies);
	printk("last result at %lu\n", resultjiffies);
	printk("last redo_fd_request at %lu\n", lastredo);
	for (i = 0; i < resultsize; i++) {
		printk("%2x ", reply_buffer[i]);
	}
	printk("\n");
#endif

	printk("status=%x\n", fd_inb(FD_STATUS));
	printk("fdc_busy=%lu\n", fdc_busy);
	if (do_floppy)
		printk("do_floppy=%p\n", do_floppy);
	if (work_pending(&floppy_work))
		printk("floppy_work.func=%p\n", floppy_work.func);
	if (timer_pending(&fd_timer))
		printk("fd_timer.function=%p\n", fd_timer.function);
	if (timer_pending(&fd_timeout)) {
		printk("timer_function=%p\n", fd_timeout.function);
		printk("expires=%lu\n", fd_timeout.expires - jiffies);
		printk("now=%lu\n", jiffies);
	}
	printk("cont=%p\n", cont);
	printk("current_req=%p\n", current_req);
	printk("command_status=%d\n", command_status);
	printk("\n");
}

static void floppy_shutdown(unsigned long data)
{
	unsigned long flags;

	if (!initialising)
		show_floppy();
	cancel_activity();

	floppy_enable_hlt();

	flags = claim_dma_lock();
	fd_disable_dma();
	release_dma_lock(flags);

	/* avoid dma going to a random drive after shutdown */

	if (!initialising)
		DPRINT("floppy timeout called\n");
	FDCS->reset = 1;
	if (cont) {
		cont->done(0);
		cont->redo();	/* this will recall reset when needed */
	} else {
		printk("no cont in shutdown!\n");
		process_fd_request();
	}
	is_alive("floppy shutdown");
}

/* start motor, check media-changed condition and write protection */
static int start_motor(void (*function)(void))
{
	int mask;
	int data;

	mask = 0xfc;
	data = UNIT(current_drive);
	if (!(raw_cmd->flags & FD_RAW_NO_MOTOR)) {
		if (!(FDCS->dor & (0x10 << UNIT(current_drive)))) {
			set_debugt();
			/* no read since this drive is running */
			DRS->first_read_date = 0;
			/* note motor start time if motor is not yet running */
			DRS->spinup_date = jiffies;
			data |= (0x10 << UNIT(current_drive));
		}
	} else if (FDCS->dor & (0x10 << UNIT(current_drive)))
		mask &= ~(0x10 << UNIT(current_drive));

	/* starts motor and selects floppy */
	del_timer(motor_off_timer + current_drive);
	set_dor(fdc, mask, data);

	/* wait_for_completion also schedules reset if needed. */
	return (fd_wait_for_completion(DRS->select_date + DP->select_delay,
				       (timeout_fn) function));
}

static void floppy_ready(void)
{
	CHECK_RESET;
	if (start_motor(floppy_ready))
		return;
	if (fdc_dtr())
		return;

#ifdef DCL_DEBUG
	if (DP->flags & FD_DEBUG) {
		DPRINT("calling disk change from floppy_ready\n");
	}
#endif
	if (!(raw_cmd->flags & FD_RAW_NO_MOTOR) &&
	    disk_change(current_drive) && !DP->select_delay)
		twaddle();	/* this clears the dcl on certain drive/controller
				 * combinations */

#ifdef fd_chose_dma_mode
	if ((raw_cmd->flags & FD_RAW_READ) || (raw_cmd->flags & FD_RAW_WRITE)) {
		unsigned long flags = claim_dma_lock();
		fd_chose_dma_mode(raw_cmd->kernel_data, raw_cmd->length);
		release_dma_lock(flags);
	}
#endif

	if (raw_cmd->flags & (FD_RAW_NEED_SEEK | FD_RAW_NEED_DISK)) {
		perpendicular_mode();
		fdc_specify();	/* must be done here because of hut, hlt ... */
		seek_floppy();
	} else {
		if ((raw_cmd->flags & FD_RAW_READ) ||
		    (raw_cmd->flags & FD_RAW_WRITE))
			fdc_specify();
		setup_rw_floppy();
	}
}

static void floppy_start(void)
{
	reschedule_timeout(current_reqD, "floppy start", 0);

	scandrives();
#ifdef DCL_DEBUG
	if (DP->flags & FD_DEBUG) {
		DPRINT("setting NEWCHANGE in floppy_start\n");
	}
#endif
	SETF(FD_DISK_NEWCHANGE);
	floppy_ready();
}

/*
 * ========================================================================
 * here ends the bottom half. Exported routines are:
 * floppy_start, floppy_off, floppy_ready, lock_fdc, unlock_fdc, set_fdc,
 * start_motor, reset_fdc, reset_fdc_info, interpret_errors.
 * Initialization also uses output_byte, result, set_dor, floppy_interrupt
 * and set_dor.
 * ========================================================================
 */
/*
 * General purpose continuations.
 * ==============================
 */

static void do_wakeup(void)
{
	reschedule_timeout(MAXTIMEOUT, "do wakeup", 0);
	cont = NULL;
	command_status += 2;
	wake_up(&command_done);
}

static struct cont_t wakeup_cont = {
	.interrupt	= empty,
	.redo		= do_wakeup,
	.error		= empty,
	.done		= (done_f)empty
};

static struct cont_t intr_cont = {
	.interrupt	= empty,
	.redo		= process_fd_request,
	.error		= empty,
	.done		= (done_f)empty
};

static int wait_til_done(void (*handler)(void), int interruptible)
{
	int ret;

	schedule_bh(handler);

	if (command_status < 2 && NO_SIGNAL) {
		DECLARE_WAITQUEUE(wait, current);

		add_wait_queue(&command_done, &wait);
		for (;;) {
			set_current_state(interruptible ?
					  TASK_INTERRUPTIBLE :
					  TASK_UNINTERRUPTIBLE);

			if (command_status >= 2 || !NO_SIGNAL)
				break;

			is_alive("wait_til_done");
			schedule();
		}

		set_current_state(TASK_RUNNING);
		remove_wait_queue(&command_done, &wait);
	}

	if (command_status < 2) {
		cancel_activity();
		cont = &intr_cont;
		reset_fdc();
		return -EINTR;
	}

	if (FDCS->reset)
		command_status = FD_COMMAND_ERROR;
	if (command_status == FD_COMMAND_OKAY)
		ret = 0;
	else
		ret = -EIO;
	command_status = FD_COMMAND_NONE;
	return ret;
}

static void generic_done(int result)
{
	command_status = result;
	cont = &wakeup_cont;
}

static void generic_success(void)
{
	cont->done(1);
}

static void generic_failure(void)
{
	cont->done(0);
}

static void success_and_wakeup(void)
{
	generic_success();
	cont->redo();
}

/*
 * formatting and rw support.
 * ==========================
 */

static int next_valid_format(void)
{
	int probed_format;

	probed_format = DRS->probed_format;
	while (1) {
		if (probed_format >= 8 || !DP->autodetect[probed_format]) {
			DRS->probed_format = 0;
			return 1;
		}
		if (floppy_type[DP->autodetect[probed_format]].sect) {
			DRS->probed_format = probed_format;
			return 0;
		}
		probed_format++;
	}
}

static void bad_flp_intr(void)
{
	int err_count;

	if (probing) {
		DRS->probed_format++;
		if (!next_valid_format())
			return;
	}
	err_count = ++(*errors);
	INFBOUND(DRWE->badness, err_count);
	if (err_count > DP->max_errors.abort)
		cont->done(0);
	if (err_count > DP->max_errors.reset)
		FDCS->reset = 1;
	else if (err_count > DP->max_errors.recal)
		DRS->track = NEED_2_RECAL;
}

static void set_floppy(int drive)
{
	int type = ITYPE(UDRS->fd_device);

	if (type)
		_floppy = floppy_type + type;
	else
		_floppy = current_type[drive];
}

/*
 * formatting support.
 * ===================
 */
static void format_interrupt(void)
{
	switch (interpret_errors()) {
	case 1:
		cont->error();
	case 2:
		break;
	case 0:
		cont->done(1);
	}
	cont->redo();
}

#define CODE2SIZE (ssize = ((1 << SIZECODE) + 3) >> 2)
#define FM_MODE(x,y) ((y) & ~(((x)->rate & 0x80) >>1))
#define CT(x) ((x) | 0xc0)
static void setup_format_params(int track)
{
	int n;
	int il;
	int count;
	int head_shift;
	int track_shift;
	struct fparm {
		unsigned char track, head, sect, size;
	} *here = (struct fparm *)floppy_track_buffer;

	raw_cmd = &default_raw_cmd;
	raw_cmd->track = track;

	raw_cmd->flags = FD_RAW_WRITE | FD_RAW_INTR | FD_RAW_SPIN |
	    FD_RAW_NEED_DISK | FD_RAW_NEED_SEEK;
	raw_cmd->rate = _floppy->rate & 0x43;
	raw_cmd->cmd_count = NR_F;
	COMMAND = FM_MODE(_floppy, FD_FORMAT);
	DR_SELECT = UNIT(current_drive) + PH_HEAD(_floppy, format_req.head);
	F_SIZECODE = FD_SIZECODE(_floppy);
	F_SECT_PER_TRACK = _floppy->sect << 2 >> F_SIZECODE;
	F_GAP = _floppy->fmt_gap;
	F_FILL = FD_FILL_BYTE;

	raw_cmd->kernel_data = floppy_track_buffer;
	raw_cmd->length = 4 * F_SECT_PER_TRACK;

	/* allow for about 30ms for data transport per track */
	head_shift = (F_SECT_PER_TRACK + 5) / 6;

	/* a ``cylinder'' is two tracks plus a little stepping time */
	track_shift = 2 * head_shift + 3;

	/* position of logical sector 1 on this track */
	n = (track_shift * format_req.track + head_shift * format_req.head)
	    % F_SECT_PER_TRACK;

	/* determine interleave */
	il = 1;
	if (_floppy->fmt_gap < 0x22)
		il++;

	/* initialize field */
	for (count = 0; count < F_SECT_PER_TRACK; ++count) {
		here[count].track = format_req.track;
		here[count].head = format_req.head;
		here[count].sect = 0;
		here[count].size = F_SIZECODE;
	}
	/* place logical sectors */
	for (count = 1; count <= F_SECT_PER_TRACK; ++count) {
		here[n].sect = count;
		n = (n + il) % F_SECT_PER_TRACK;
		if (here[n].sect) {	/* sector busy, find next free sector */
			++n;
			if (n >= F_SECT_PER_TRACK) {
				n -= F_SECT_PER_TRACK;
				while (here[n].sect)
					++n;
			}
		}
	}
	if (_floppy->stretch & FD_SECTBASEMASK) {
		for (count = 0; count < F_SECT_PER_TRACK; count++)
			here[count].sect += FD_SECTBASE(_floppy) - 1;
	}
}

static void redo_format(void)
{
	buffer_track = -1;
	setup_format_params(format_req.track << STRETCH(_floppy));
	floppy_start();
	debugt("queue format request");
}

static struct cont_t format_cont = {
	.interrupt	= format_interrupt,
	.redo		= redo_format,
	.error		= bad_flp_intr,
	.done		= generic_done
};

static int do_format(int drive, struct format_descr *tmp_format_req)
{
	int ret;

	LOCK_FDC(drive, 1);
	set_floppy(drive);
	if (!_floppy ||
	    _floppy->track > DP->tracks ||
	    tmp_format_req->track >= _floppy->track ||
	    tmp_format_req->head >= _floppy->head ||
	    (_floppy->sect << 2) % (1 << FD_SIZECODE(_floppy)) ||
	    !_floppy->fmt_gap) {
		process_fd_request();
		return -EINVAL;
	}
	format_req = *tmp_format_req;
	format_errors = 0;
	cont = &format_cont;
	errors = &format_errors;
	IWAIT(redo_format);
	process_fd_request();
	return ret;
}

/*
 * Buffer read/write and support
 * =============================
 */

static void floppy_end_request(struct request *req, int error)
{
	unsigned int nr_sectors = current_count_sectors;
	unsigned int drive = (unsigned long)req->rq_disk->private_data;

	/* current_count_sectors can be zero if transfer failed */
	if (error)
		nr_sectors = blk_rq_cur_sectors(req);
	if (__blk_end_request(req, error, nr_sectors << 9))
		return;

	/* We're done with the request */
	floppy_off(drive);
	current_req = NULL;
}

/* new request_done. Can handle physical sectors which are smaller than a
 * logical buffer */
static void request_done(int uptodate)
{
	struct request_queue *q = floppy_queue;
	struct request *req = current_req;
	unsigned long flags;
	int block;

	probing = 0;
	reschedule_timeout(MAXTIMEOUT, "request done %d", uptodate);

	if (!req) {
		printk("floppy.c: no request in request_done\n");
		return;
	}

	if (uptodate) {
		/* maintain values for invalidation on geometry
		 * change */
		block = current_count_sectors + blk_rq_pos(req);
		INFBOUND(DRS->maxblock, block);
		if (block > _floppy->sect)
			DRS->maxtrack = 1;

		/* unlock chained buffers */
		spin_lock_irqsave(q->queue_lock, flags);
		floppy_end_request(req, 0);
		spin_unlock_irqrestore(q->queue_lock, flags);
	} else {
		if (rq_data_dir(req) == WRITE) {
			/* record write error information */
			DRWE->write_errors++;
			if (DRWE->write_errors == 1) {
				DRWE->first_error_sector = blk_rq_pos(req);
				DRWE->first_error_generation = DRS->generation;
			}
			DRWE->last_error_sector = blk_rq_pos(req);
			DRWE->last_error_generation = DRS->generation;
		}
		spin_lock_irqsave(q->queue_lock, flags);
		floppy_end_request(req, -EIO);
		spin_unlock_irqrestore(q->queue_lock, flags);
	}
}

/* Interrupt handler evaluating the result of the r/w operation */
static void rw_interrupt(void)
{
	int eoc;
	int ssize;
	int heads;
	int nr_sectors;

	if (R_HEAD >= 2) {
		/* some Toshiba floppy controllers occasionnally seem to
		 * return bogus interrupts after read/write operations, which
		 * can be recognized by a bad head number (>= 2) */
		return;
	}

	if (!DRS->first_read_date)
		DRS->first_read_date = jiffies;

	nr_sectors = 0;
	CODE2SIZE;

	if (ST1 & ST1_EOC)
		eoc = 1;
	else
		eoc = 0;

	if (COMMAND & 0x80)
		heads = 2;
	else
		heads = 1;

	nr_sectors = (((R_TRACK - TRACK) * heads +
		       R_HEAD - HEAD) * SECT_PER_TRACK +
		      R_SECTOR - SECTOR + eoc) << SIZECODE >> 2;

#ifdef FLOPPY_SANITY_CHECK
	if (nr_sectors / ssize >
	    DIV_ROUND_UP(in_sector_offset + current_count_sectors, ssize)) {
		DPRINT("long rw: %x instead of %lx\n",
		       nr_sectors, current_count_sectors);
		printk("rs=%d s=%d\n", R_SECTOR, SECTOR);
		printk("rh=%d h=%d\n", R_HEAD, HEAD);
		printk("rt=%d t=%d\n", R_TRACK, TRACK);
		printk("heads=%d eoc=%d\n", heads, eoc);
		printk("spt=%d st=%d ss=%d\n", SECT_PER_TRACK,
		       fsector_t, ssize);
		printk("in_sector_offset=%d\n", in_sector_offset);
	}
#endif

	nr_sectors -= in_sector_offset;
	INFBOUND(nr_sectors, 0);
	SUPBOUND(current_count_sectors, nr_sectors);

	switch (interpret_errors()) {
	case 2:
		cont->redo();
		return;
	case 1:
		if (!current_count_sectors) {
			cont->error();
			cont->redo();
			return;
		}
		break;
	case 0:
		if (!current_count_sectors) {
			cont->redo();
			return;
		}
		current_type[current_drive] = _floppy;
		floppy_sizes[TOMINOR(current_drive)] = _floppy->size;
		break;
	}

	if (probing) {
		if (DP->flags & FTD_MSG)
			DPRINT("Auto-detected floppy type %s in fd%d\n",
			       _floppy->name, current_drive);
		current_type[current_drive] = _floppy;
		floppy_sizes[TOMINOR(current_drive)] = _floppy->size;
		probing = 0;
	}

	if (CT(COMMAND) != FD_READ ||
	    raw_cmd->kernel_data == current_req->buffer) {
		/* transfer directly from buffer */
		cont->done(1);
	} else if (CT(COMMAND) == FD_READ) {
		buffer_track = raw_cmd->track;
		buffer_drive = current_drive;
		INFBOUND(buffer_max, nr_sectors + fsector_t);
	}
	cont->redo();
}

/* Compute maximal contiguous buffer size. */
static int buffer_chain_size(void)
{
	struct bio_vec *bv;
	int size;
	struct req_iterator iter;
	char *base;

	base = bio_data(current_req->bio);
	size = 0;

	rq_for_each_segment(bv, current_req, iter) {
		if (page_address(bv->bv_page) + bv->bv_offset != base + size)
			break;

		size += bv->bv_len;
	}

	return size >> 9;
}

/* Compute the maximal transfer size */
static int transfer_size(int ssize, int max_sector, int max_size)
{
	SUPBOUND(max_sector, fsector_t + max_size);

	/* alignment */
	max_sector -= (max_sector % _floppy->sect) % ssize;

	/* transfer size, beginning not aligned */
	current_count_sectors = max_sector - fsector_t;

	return max_sector;
}

/*
 * Move data from/to the track buffer to/from the buffer cache.
 */
static void copy_buffer(int ssize, int max_sector, int max_sector_2)
{
	int remaining;		/* number of transferred 512-byte sectors */
	struct bio_vec *bv;
	char *buffer;
	char *dma_buffer;
	int size;
	struct req_iterator iter;

	max_sector = transfer_size(ssize,
				   min(max_sector, max_sector_2),
				   blk_rq_sectors(current_req));

	if (current_count_sectors <= 0 && CT(COMMAND) == FD_WRITE &&
	    buffer_max > fsector_t + blk_rq_sectors(current_req))
		current_count_sectors = min_t(int, buffer_max - fsector_t,
					      blk_rq_sectors(current_req));

	remaining = current_count_sectors << 9;
#ifdef FLOPPY_SANITY_CHECK
	if (remaining > blk_rq_bytes(current_req) && CT(COMMAND) == FD_WRITE) {
		DPRINT("in copy buffer\n");
		printk("current_count_sectors=%ld\n", current_count_sectors);
		printk("remaining=%d\n", remaining >> 9);
		printk("current_req->nr_sectors=%u\n",
		       blk_rq_sectors(current_req));
		printk("current_req->current_nr_sectors=%u\n",
		       blk_rq_cur_sectors(current_req));
		printk("max_sector=%d\n", max_sector);
		printk("ssize=%d\n", ssize);
	}
#endif

	buffer_max = max(max_sector, buffer_max);

	dma_buffer = floppy_track_buffer + ((fsector_t - buffer_min) << 9);

	size = blk_rq_cur_bytes(current_req);

	rq_for_each_segment(bv, current_req, iter) {
		if (!remaining)
			break;

		size = bv->bv_len;
		SUPBOUND(size, remaining);

		buffer = page_address(bv->bv_page) + bv->bv_offset;
#ifdef FLOPPY_SANITY_CHECK
		if (dma_buffer + size >
		    floppy_track_buffer + (max_buffer_sectors << 10) ||
		    dma_buffer < floppy_track_buffer) {
			DPRINT("buffer overrun in copy buffer %d\n",
			       (int)((floppy_track_buffer -
				      dma_buffer) >> 9));
			printk("fsector_t=%d buffer_min=%d\n",
			       fsector_t, buffer_min);
			printk("current_count_sectors=%ld\n",
			       current_count_sectors);
			if (CT(COMMAND) == FD_READ)
				printk("read\n");
			if (CT(COMMAND) == FD_WRITE)
				printk("write\n");
			break;
		}
		if (((unsigned long)buffer) % 512)
			DPRINT("%p buffer not aligned\n", buffer);
#endif
		if (CT(COMMAND) == FD_READ)
			memcpy(buffer, dma_buffer, size);
		else
			memcpy(dma_buffer, buffer, size);

		remaining -= size;
		dma_buffer += size;
	}
#ifdef FLOPPY_SANITY_CHECK
	if (remaining) {
		if (remaining > 0)
			max_sector -= remaining >> 9;
		DPRINT("weirdness: remaining %d\n", remaining >> 9);
	}
#endif
}

/* work around a bug in pseudo DMA
 * (on some FDCs) pseudo DMA does not stop when the CPU stops
 * sending data.  Hence we need a different way to signal the
 * transfer length:  We use SECT_PER_TRACK.  Unfortunately, this
 * does not work with MT, hence we can only transfer one head at
 * a time
 */
static void virtualdmabug_workaround(void)
{
	int hard_sectors;
	int end_sector;

	if (CT(COMMAND) == FD_WRITE) {
		COMMAND &= ~0x80;	/* switch off multiple track mode */

		hard_sectors = raw_cmd->length >> (7 + SIZECODE);
		end_sector = SECTOR + hard_sectors - 1;
#ifdef FLOPPY_SANITY_CHECK
		if (end_sector > SECT_PER_TRACK) {
			printk("too many sectors %d > %d\n",
			       end_sector, SECT_PER_TRACK);
			return;
		}
#endif
		SECT_PER_TRACK = end_sector;	/* make sure SECT_PER_TRACK points
						 * to end of transfer */
	}
}

/*
 * Formulate a read/write request.
 * this routine decides where to load the data (directly to buffer, or to
 * tmp floppy area), how much data to load (the size of the buffer, the whole
 * track, or a single sector)
 * All floppy_track_buffer handling goes in here. If we ever add track buffer
 * allocation on the fly, it should be done here. No other part should need
 * modification.
 */

static int make_raw_rw_request(void)
{
	int aligned_sector_t;
	int max_sector;
	int max_size;
	int tracksize;
	int ssize;

	if (max_buffer_sectors == 0) {
		printk("VFS: Block I/O scheduled on unopened device\n");
		return 0;
	}

	set_fdc((long)current_req->rq_disk->private_data);

	raw_cmd = &default_raw_cmd;
	raw_cmd->flags = FD_RAW_SPIN | FD_RAW_NEED_DISK | FD_RAW_NEED_DISK |
	    FD_RAW_NEED_SEEK;
	raw_cmd->cmd_count = NR_RW;
	if (rq_data_dir(current_req) == READ) {
		raw_cmd->flags |= FD_RAW_READ;
		COMMAND = FM_MODE(_floppy, FD_READ);
	} else if (rq_data_dir(current_req) == WRITE) {
		raw_cmd->flags |= FD_RAW_WRITE;
		COMMAND = FM_MODE(_floppy, FD_WRITE);
	} else {
		DPRINT("make_raw_rw_request: unknown command\n");
		return 0;
	}

	max_sector = _floppy->sect * _floppy->head;

	TRACK = (int)blk_rq_pos(current_req) / max_sector;
	fsector_t = (int)blk_rq_pos(current_req) % max_sector;
	if (_floppy->track && TRACK >= _floppy->track) {
		if (blk_rq_cur_sectors(current_req) & 1) {
			current_count_sectors = 1;
			return 1;
		} else
			return 0;
	}
	HEAD = fsector_t / _floppy->sect;

	if (((_floppy->stretch & (FD_SWAPSIDES | FD_SECTBASEMASK)) ||
	     TESTF(FD_NEED_TWADDLE)) && fsector_t < _floppy->sect)
		max_sector = _floppy->sect;

	/* 2M disks have phantom sectors on the first track */
	if ((_floppy->rate & FD_2M) && (!TRACK) && (!HEAD)) {
		max_sector = 2 * _floppy->sect / 3;
		if (fsector_t >= max_sector) {
			current_count_sectors =
			    min_t(int, _floppy->sect - fsector_t,
				  blk_rq_sectors(current_req));
			return 1;
		}
		SIZECODE = 2;
	} else
		SIZECODE = FD_SIZECODE(_floppy);
	raw_cmd->rate = _floppy->rate & 0x43;
	if ((_floppy->rate & FD_2M) && (TRACK || HEAD) && raw_cmd->rate == 2)
		raw_cmd->rate = 1;

	if (SIZECODE)
		SIZECODE2 = 0xff;
	else
		SIZECODE2 = 0x80;
	raw_cmd->track = TRACK << STRETCH(_floppy);
	DR_SELECT = UNIT(current_drive) + PH_HEAD(_floppy, HEAD);
	GAP = _floppy->gap;
	CODE2SIZE;
	SECT_PER_TRACK = _floppy->sect << 2 >> SIZECODE;
	SECTOR = ((fsector_t % _floppy->sect) << 2 >> SIZECODE) +
	    FD_SECTBASE(_floppy);

	/* tracksize describes the size which can be filled up with sectors
	 * of size ssize.
	 */
	tracksize = _floppy->sect - _floppy->sect % ssize;
	if (tracksize < _floppy->sect) {
		SECT_PER_TRACK++;
		if (tracksize <= fsector_t % _floppy->sect)
			SECTOR--;

		/* if we are beyond tracksize, fill up using smaller sectors */
		while (tracksize <= fsector_t % _floppy->sect) {
			while (tracksize + ssize > _floppy->sect) {
				SIZECODE--;
				ssize >>= 1;
			}
			SECTOR++;
			SECT_PER_TRACK++;
			tracksize += ssize;
		}
		max_sector = HEAD * _floppy->sect + tracksize;
	} else if (!TRACK && !HEAD && !(_floppy->rate & FD_2M) && probing) {
		max_sector = _floppy->sect;
	} else if (!HEAD && CT(COMMAND) == FD_WRITE) {
		/* for virtual DMA bug workaround */
		max_sector = _floppy->sect;
	}

	in_sector_offset = (fsector_t % _floppy->sect) % ssize;
	aligned_sector_t = fsector_t - in_sector_offset;
	max_size = blk_rq_sectors(current_req);
	if ((raw_cmd->track == buffer_track) &&
	    (current_drive == buffer_drive) &&
	    (fsector_t >= buffer_min) && (fsector_t < buffer_max)) {
		/* data already in track buffer */
		if (CT(COMMAND) == FD_READ) {
			copy_buffer(1, max_sector, buffer_max);
			return 1;
		}
	} else if (in_sector_offset || blk_rq_sectors(current_req) < ssize) {
		if (CT(COMMAND) == FD_WRITE) {
			if (fsector_t + blk_rq_sectors(current_req) > ssize &&
			    fsector_t + blk_rq_sectors(current_req) < ssize + ssize)
				max_size = ssize + ssize;
			else
				max_size = ssize;
		}
		raw_cmd->flags &= ~FD_RAW_WRITE;
		raw_cmd->flags |= FD_RAW_READ;
		COMMAND = FM_MODE(_floppy, FD_READ);
	} else if ((unsigned long)current_req->buffer < MAX_DMA_ADDRESS) {
		unsigned long dma_limit;
		int direct, indirect;

		indirect =
		    transfer_size(ssize, max_sector,
				  max_buffer_sectors * 2) - fsector_t;

		/*
		 * Do NOT use minimum() here---MAX_DMA_ADDRESS is 64 bits wide
		 * on a 64 bit machine!
		 */
		max_size = buffer_chain_size();
		dma_limit =
		    (MAX_DMA_ADDRESS -
		     ((unsigned long)current_req->buffer)) >> 9;
		if ((unsigned long)max_size > dma_limit) {
			max_size = dma_limit;
		}
		/* 64 kb boundaries */
		if (CROSS_64KB(current_req->buffer, max_size << 9))
			max_size = (K_64 -
				    ((unsigned long)current_req->buffer) %
				    K_64) >> 9;
		direct = transfer_size(ssize, max_sector, max_size) - fsector_t;
		/*
		 * We try to read tracks, but if we get too many errors, we
		 * go back to reading just one sector at a time.
		 *
		 * This means we should be able to read a sector even if there
		 * are other bad sectors on this track.
		 */
		if (!direct ||
		    (indirect * 2 > direct * 3 &&
		     *errors < DP->max_errors.read_track && ((!probing
		       || (DP->read_track & (1 << DRS->probed_format)))))) {
			max_size = blk_rq_sectors(current_req);
		} else {
			raw_cmd->kernel_data = current_req->buffer;
			raw_cmd->length = current_count_sectors << 9;
			if (raw_cmd->length == 0) {
				DPRINT
				    ("zero dma transfer attempted from make_raw_request\n");
				DPRINT("indirect=%d direct=%d fsector_t=%d",
				       indirect, direct, fsector_t);
				return 0;
			}
			virtualdmabug_workaround();
			return 2;
		}
	}

	if (CT(COMMAND) == FD_READ)
		max_size = max_sector;	/* unbounded */

	/* claim buffer track if needed */
	if (buffer_track != raw_cmd->track ||	/* bad track */
	    buffer_drive != current_drive ||	/* bad drive */
	    fsector_t > buffer_max ||
	    fsector_t < buffer_min ||
	    ((CT(COMMAND) == FD_READ ||
	      (!in_sector_offset && blk_rq_sectors(current_req) >= ssize)) &&
	     max_sector > 2 * max_buffer_sectors + buffer_min &&
	     max_size + fsector_t > 2 * max_buffer_sectors + buffer_min)
	    /* not enough space */
	    ) {
		buffer_track = -1;
		buffer_drive = current_drive;
		buffer_max = buffer_min = aligned_sector_t;
	}
	raw_cmd->kernel_data = floppy_track_buffer +
	    ((aligned_sector_t - buffer_min) << 9);

	if (CT(COMMAND) == FD_WRITE) {
		/* copy write buffer to track buffer.
		 * if we get here, we know that the write
		 * is either aligned or the data already in the buffer
		 * (buffer will be overwritten) */
#ifdef FLOPPY_SANITY_CHECK
		if (in_sector_offset && buffer_track == -1)
			DPRINT("internal error offset !=0 on write\n");
#endif
		buffer_track = raw_cmd->track;
		buffer_drive = current_drive;
		copy_buffer(ssize, max_sector,
			    2 * max_buffer_sectors + buffer_min);
	} else
		transfer_size(ssize, max_sector,
			      2 * max_buffer_sectors + buffer_min -
			      aligned_sector_t);

	/* round up current_count_sectors to get dma xfer size */
	raw_cmd->length = in_sector_offset + current_count_sectors;
	raw_cmd->length = ((raw_cmd->length - 1) | (ssize - 1)) + 1;
	raw_cmd->length <<= 9;
#ifdef FLOPPY_SANITY_CHECK
	if ((raw_cmd->length < current_count_sectors << 9) ||
	    (raw_cmd->kernel_data != current_req->buffer &&
	     CT(COMMAND) == FD_WRITE &&
	     (aligned_sector_t + (raw_cmd->length >> 9) > buffer_max ||
	      aligned_sector_t < buffer_min)) ||
	    raw_cmd->length % (128 << SIZECODE) ||
	    raw_cmd->length <= 0 || current_count_sectors <= 0) {
		DPRINT("fractionary current count b=%lx s=%lx\n",
		       raw_cmd->length, current_count_sectors);
		if (raw_cmd->kernel_data != current_req->buffer)
			printk("addr=%d, length=%ld\n",
			       (int)((raw_cmd->kernel_data -
				      floppy_track_buffer) >> 9),
			       current_count_sectors);
		printk("st=%d ast=%d mse=%d msi=%d\n",
		       fsector_t, aligned_sector_t, max_sector, max_size);
		printk("ssize=%x SIZECODE=%d\n", ssize, SIZECODE);
		printk("command=%x SECTOR=%d HEAD=%d, TRACK=%d\n",
		       COMMAND, SECTOR, HEAD, TRACK);
		printk("buffer drive=%d\n", buffer_drive);
		printk("buffer track=%d\n", buffer_track);
		printk("buffer_min=%d\n", buffer_min);
		printk("buffer_max=%d\n", buffer_max);
		return 0;
	}

	if (raw_cmd->kernel_data != current_req->buffer) {
		if (raw_cmd->kernel_data < floppy_track_buffer ||
		    current_count_sectors < 0 ||
		    raw_cmd->length < 0 ||
		    raw_cmd->kernel_data + raw_cmd->length >
		    floppy_track_buffer + (max_buffer_sectors << 10)) {
			DPRINT("buffer overrun in schedule dma\n");
			printk("fsector_t=%d buffer_min=%d current_count=%ld\n",
			       fsector_t, buffer_min, raw_cmd->length >> 9);
			printk("current_count_sectors=%ld\n",
			       current_count_sectors);
			if (CT(COMMAND) == FD_READ)
				printk("read\n");
			if (CT(COMMAND) == FD_WRITE)
				printk("write\n");
			return 0;
		}
	} else if (raw_cmd->length > blk_rq_bytes(current_req) ||
		   current_count_sectors > blk_rq_sectors(current_req)) {
		DPRINT("buffer overrun in direct transfer\n");
		return 0;
	} else if (raw_cmd->length < current_count_sectors << 9) {
		DPRINT("more sectors than bytes\n");
		printk("bytes=%ld\n", raw_cmd->length >> 9);
		printk("sectors=%ld\n", current_count_sectors);
	}
	if (raw_cmd->length == 0) {
		DPRINT("zero dma transfer attempted from make_raw_request\n");
		return 0;
	}
#endif

	virtualdmabug_workaround();
	return 2;
}

static void redo_fd_request(void)
{
#define REPEAT {request_done(0); continue; }
	int drive;
	int tmp;

	lastredo = jiffies;
	if (current_drive < N_DRIVE)
		floppy_off(current_drive);

	for (;;) {
		if (!current_req) {
			struct request *req;

			spin_lock_irq(floppy_queue->queue_lock);
			req = blk_fetch_request(floppy_queue);
			spin_unlock_irq(floppy_queue->queue_lock);
			if (!req) {
				do_floppy = NULL;
				unlock_fdc();
				return;
			}
			current_req = req;
		}
		drive = (long)current_req->rq_disk->private_data;
		set_fdc(drive);
		reschedule_timeout(current_reqD, "redo fd request", 0);

		set_floppy(drive);
		raw_cmd = &default_raw_cmd;
		raw_cmd->flags = 0;
		if (start_motor(redo_fd_request))
			return;
		disk_change(current_drive);
		if (test_bit(current_drive, &fake_change) ||
		    TESTF(FD_DISK_CHANGED)) {
			DPRINT("disk absent or changed during operation\n");
			REPEAT;
		}
		if (!_floppy) {	/* Autodetection */
			if (!probing) {
				DRS->probed_format = 0;
				if (next_valid_format()) {
					DPRINT("no autodetectable formats\n");
					_floppy = NULL;
					REPEAT;
				}
			}
			probing = 1;
			_floppy =
			    floppy_type + DP->autodetect[DRS->probed_format];
		} else
			probing = 0;
		errors = &(current_req->errors);
		tmp = make_raw_rw_request();
		if (tmp < 2) {
			request_done(tmp);
			continue;
		}

		if (TESTF(FD_NEED_TWADDLE))
			twaddle();
		schedule_bh(floppy_start);
		debugt("queue fd request");
		return;
	}
#undef REPEAT
}

static struct cont_t rw_cont = {
	.interrupt	= rw_interrupt,
	.redo		= redo_fd_request,
	.error		= bad_flp_intr,
	.done		= request_done
};

static void process_fd_request(void)
{
	cont = &rw_cont;
	schedule_bh(redo_fd_request);
}

static void do_fd_request(struct request_queue * q)
{
	if (max_buffer_sectors == 0) {
		printk("VFS: do_fd_request called on non-open device\n");
		return;
	}

	if (usage_count == 0) {
		printk("warning: usage count=0, current_req=%p exiting\n",
		       current_req);
		printk("sect=%ld type=%x flags=%x\n",
		       (long)blk_rq_pos(current_req), current_req->cmd_type,
		       current_req->cmd_flags);
		return;
	}
	if (test_bit(0, &fdc_busy)) {
		/* fdc busy, this new request will be treated when the
		   current one is done */
		is_alive("do fd request, old request running");
		return;
	}
	lock_fdc(MAXTIMEOUT, 0);
	process_fd_request();
	is_alive("do fd request");
}

static struct cont_t poll_cont = {
	.interrupt	= success_and_wakeup,
	.redo		= floppy_ready,
	.error		= generic_failure,
	.done		= generic_done
};

static int poll_drive(int interruptible, int flag)
{
	int ret;

	/* no auto-sense, just clear dcl */
	raw_cmd = &default_raw_cmd;
	raw_cmd->flags = flag;
	raw_cmd->track = 0;
	raw_cmd->cmd_count = 0;
	cont = &poll_cont;
#ifdef DCL_DEBUG
	if (DP->flags & FD_DEBUG) {
		DPRINT("setting NEWCHANGE in poll_drive\n");
	}
#endif
	SETF(FD_DISK_NEWCHANGE);
	WAIT(floppy_ready);
	return ret;
}

/*
 * User triggered reset
 * ====================
 */

static void reset_intr(void)
{
	printk("weird, reset interrupt called\n");
}

static struct cont_t reset_cont = {
	.interrupt	= reset_intr,
	.redo		= success_and_wakeup,
	.error		= generic_failure,
	.done		= generic_done
};

static int user_reset_fdc(int drive, int arg, int interruptible)
{
	int ret;

	ret = 0;
	LOCK_FDC(drive, interruptible);
	if (arg == FD_RESET_ALWAYS)
		FDCS->reset = 1;
	if (FDCS->reset) {
		cont = &reset_cont;
		WAIT(reset_fdc);
	}
	process_fd_request();
	return ret;
}

/*
 * Misc Ioctl's and support
 * ========================
 */
static inline int fd_copyout(void __user *param, const void *address,
			     unsigned long size)
{
	return copy_to_user(param, address, size) ? -EFAULT : 0;
}

static inline int fd_copyin(void __user *param, void *address, unsigned long size)
{
	return copy_from_user(address, param, size) ? -EFAULT : 0;
}

#define _COPYOUT(x) (copy_to_user((void __user *)param, &(x), sizeof(x)) ? -EFAULT : 0)
#define _COPYIN(x) (copy_from_user(&(x), (void __user *)param, sizeof(x)) ? -EFAULT : 0)

#define COPYOUT(x) ECALL(_COPYOUT(x))
#define COPYIN(x) ECALL(_COPYIN(x))

static inline const char *drive_name(int type, int drive)
{
	struct floppy_struct *floppy;

	if (type)
		floppy = floppy_type + type;
	else {
		if (UDP->native_format)
			floppy = floppy_type + UDP->native_format;
		else
			return "(null)";
	}
	if (floppy->name)
		return floppy->name;
	else
		return "(null)";
}

/* raw commands */
static void raw_cmd_done(int flag)
{
	int i;

	if (!flag) {
		raw_cmd->flags |= FD_RAW_FAILURE;
		raw_cmd->flags |= FD_RAW_HARDFAILURE;
	} else {
		raw_cmd->reply_count = inr;
		if (raw_cmd->reply_count > MAX_REPLIES)
			raw_cmd->reply_count = 0;
		for (i = 0; i < raw_cmd->reply_count; i++)
			raw_cmd->reply[i] = reply_buffer[i];

		if (raw_cmd->flags & (FD_RAW_READ | FD_RAW_WRITE)) {
			unsigned long flags;
			flags = claim_dma_lock();
			raw_cmd->length = fd_get_dma_residue();
			release_dma_lock(flags);
		}

		if ((raw_cmd->flags & FD_RAW_SOFTFAILURE) &&
		    (!raw_cmd->reply_count || (raw_cmd->reply[0] & 0xc0)))
			raw_cmd->flags |= FD_RAW_FAILURE;

		if (disk_change(current_drive))
			raw_cmd->flags |= FD_RAW_DISK_CHANGE;
		else
			raw_cmd->flags &= ~FD_RAW_DISK_CHANGE;
		if (raw_cmd->flags & FD_RAW_NO_MOTOR_AFTER)
			motor_off_callback(current_drive);

		if (raw_cmd->next &&
		    (!(raw_cmd->flags & FD_RAW_FAILURE) ||
		     !(raw_cmd->flags & FD_RAW_STOP_IF_FAILURE)) &&
		    ((raw_cmd->flags & FD_RAW_FAILURE) ||
		     !(raw_cmd->flags & FD_RAW_STOP_IF_SUCCESS))) {
			raw_cmd = raw_cmd->next;
			return;
		}
	}
	generic_done(flag);
}

static struct cont_t raw_cmd_cont = {
	.interrupt	= success_and_wakeup,
	.redo		= floppy_start,
	.error		= generic_failure,
	.done		= raw_cmd_done
};

static inline int raw_cmd_copyout(int cmd, char __user *param,
				  struct floppy_raw_cmd *ptr)
{
	int ret;

	while (ptr) {
		COPYOUT(*ptr);
		param += sizeof(struct floppy_raw_cmd);
		if ((ptr->flags & FD_RAW_READ) && ptr->buffer_length) {
			if (ptr->length >= 0
			    && ptr->length <= ptr->buffer_length)
				ECALL(fd_copyout
				      (ptr->data, ptr->kernel_data,
				       ptr->buffer_length - ptr->length));
		}
		ptr = ptr->next;
	}
	return 0;
}

static void raw_cmd_free(struct floppy_raw_cmd **ptr)
{
	struct floppy_raw_cmd *next;
	struct floppy_raw_cmd *this;

	this = *ptr;
	*ptr = NULL;
	while (this) {
		if (this->buffer_length) {
			fd_dma_mem_free((unsigned long)this->kernel_data,
					this->buffer_length);
			this->buffer_length = 0;
		}
		next = this->next;
		kfree(this);
		this = next;
	}
}

static inline int raw_cmd_copyin(int cmd, char __user *param,
				 struct floppy_raw_cmd **rcmd)
{
	struct floppy_raw_cmd *ptr;
	int ret;
	int i;

	*rcmd = NULL;
	while (1) {
		ptr = (struct floppy_raw_cmd *)
		    kmalloc(sizeof(struct floppy_raw_cmd), GFP_USER);
		if (!ptr)
			return -ENOMEM;
		*rcmd = ptr;
		COPYIN(*ptr);
		ptr->next = NULL;
		ptr->buffer_length = 0;
		param += sizeof(struct floppy_raw_cmd);
		if (ptr->cmd_count > 33)
			/* the command may now also take up the space
			 * initially intended for the reply & the
			 * reply count. Needed for long 82078 commands
			 * such as RESTORE, which takes ... 17 command
			 * bytes. Murphy's law #137: When you reserve
			 * 16 bytes for a structure, you'll one day
			 * discover that you really need 17...
			 */
			return -EINVAL;

		for (i = 0; i < 16; i++)
			ptr->reply[i] = 0;
		ptr->resultcode = 0;
		ptr->kernel_data = NULL;

		if (ptr->flags & (FD_RAW_READ | FD_RAW_WRITE)) {
			if (ptr->length <= 0)
				return -EINVAL;
			ptr->kernel_data =
			    (char *)fd_dma_mem_alloc(ptr->length);
			fallback_on_nodma_alloc(&ptr->kernel_data, ptr->length);
			if (!ptr->kernel_data)
				return -ENOMEM;
			ptr->buffer_length = ptr->length;
		}
		if (ptr->flags & FD_RAW_WRITE)
			ECALL(fd_copyin(ptr->data, ptr->kernel_data,
					ptr->length));
		rcmd = &(ptr->next);
		if (!(ptr->flags & FD_RAW_MORE))
			return 0;
		ptr->rate &= 0x43;
	}
}

static int raw_cmd_ioctl(int cmd, void __user *param)
{
	struct floppy_raw_cmd *my_raw_cmd;
	int drive;
	int ret2;
	int ret;

	if (FDCS->rawcmd <= 1)
		FDCS->rawcmd = 1;
	for (drive = 0; drive < N_DRIVE; drive++) {
		if (FDC(drive) != fdc)
			continue;
		if (drive == current_drive) {
			if (UDRS->fd_ref > 1) {
				FDCS->rawcmd = 2;
				break;
			}
		} else if (UDRS->fd_ref) {
			FDCS->rawcmd = 2;
			break;
		}
	}

	if (FDCS->reset)
		return -EIO;

	ret = raw_cmd_copyin(cmd, param, &my_raw_cmd);
	if (ret) {
		raw_cmd_free(&my_raw_cmd);
		return ret;
	}

	raw_cmd = my_raw_cmd;
	cont = &raw_cmd_cont;
	ret = wait_til_done(floppy_start, 1);
#ifdef DCL_DEBUG
	if (DP->flags & FD_DEBUG) {
		DPRINT("calling disk change from raw_cmd ioctl\n");
	}
#endif

	if (ret != -EINTR && FDCS->reset)
		ret = -EIO;

	DRS->track = NO_TRACK;

	ret2 = raw_cmd_copyout(cmd, param, my_raw_cmd);
	if (!ret)
		ret = ret2;
	raw_cmd_free(&my_raw_cmd);
	return ret;
}

static int invalidate_drive(struct block_device *bdev)
{
	/* invalidate the buffer track to force a reread */
	set_bit((long)bdev->bd_disk->private_data, &fake_change);
	process_fd_request();
	check_disk_change(bdev);
	return 0;
}

static inline int set_geometry(unsigned int cmd, struct floppy_struct *g,
			       int drive, int type, struct block_device *bdev)
{
	int cnt;

	/* sanity checking for parameters. */
	if (g->sect <= 0 ||
	    g->head <= 0 ||
	    g->track <= 0 || g->track > UDP->tracks >> STRETCH(g) ||
	    /* check if reserved bits are set */
	    (g->stretch & ~(FD_STRETCH | FD_SWAPSIDES | FD_SECTBASEMASK)) != 0)
		return -EINVAL;
	if (type) {
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		mutex_lock(&open_lock);
		if (lock_fdc(drive, 1)) {
			mutex_unlock(&open_lock);
			return -EINTR;
		}
		floppy_type[type] = *g;
		floppy_type[type].name = "user format";
		for (cnt = type << 2; cnt < (type << 2) + 4; cnt++)
			floppy_sizes[cnt] = floppy_sizes[cnt + 0x80] =
			    floppy_type[type].size + 1;
		process_fd_request();
		for (cnt = 0; cnt < N_DRIVE; cnt++) {
			struct block_device *bdev = opened_bdev[cnt];
			if (!bdev || ITYPE(drive_state[cnt].fd_device) != type)
				continue;
			__invalidate_device(bdev);
		}
		mutex_unlock(&open_lock);
	} else {
		int oldStretch;
		LOCK_FDC(drive, 1);
		if (cmd != FDDEFPRM)
			/* notice a disk change immediately, else
			 * we lose our settings immediately*/
			CALL(poll_drive(1, FD_RAW_NEED_DISK));
		oldStretch = g->stretch;
		user_params[drive] = *g;
		if (buffer_drive == drive)
			SUPBOUND(buffer_max, user_params[drive].sect);
		current_type[drive] = &user_params[drive];
		floppy_sizes[drive] = user_params[drive].size;
		if (cmd == FDDEFPRM)
			DRS->keep_data = -1;
		else
			DRS->keep_data = 1;
		/* invalidation. Invalidate only when needed, i.e.
		 * when there are already sectors in the buffer cache
		 * whose number will change. This is useful, because
		 * mtools often changes the geometry of the disk after
		 * looking at the boot block */
		if (DRS->maxblock > user_params[drive].sect ||
		    DRS->maxtrack ||
		    ((user_params[drive].sect ^ oldStretch) &
		     (FD_SWAPSIDES | FD_SECTBASEMASK)))
			invalidate_drive(bdev);
		else
			process_fd_request();
	}
	return 0;
}

/* handle obsolete ioctl's */
static int ioctl_table[] = {
	FDCLRPRM,
	FDSETPRM,
	FDDEFPRM,
	FDGETPRM,
	FDMSGON,
	FDMSGOFF,
	FDFMTBEG,
	FDFMTTRK,
	FDFMTEND,
	FDSETEMSGTRESH,
	FDFLUSH,
	FDSETMAXERRS,
	FDGETMAXERRS,
	FDGETDRVTYP,
	FDSETDRVPRM,
	FDGETDRVPRM,
	FDGETDRVSTAT,
	FDPOLLDRVSTAT,
	FDRESET,
	FDGETFDCSTAT,
	FDWERRORCLR,
	FDWERRORGET,
	FDRAWCMD,
	FDEJECT,
	FDTWADDLE
};

static inline int normalize_ioctl(int *cmd, int *size)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ioctl_table); i++) {
		if ((*cmd & 0xffff) == (ioctl_table[i] & 0xffff)) {
			*size = _IOC_SIZE(*cmd);
			*cmd = ioctl_table[i];
			if (*size > _IOC_SIZE(*cmd)) {
				printk("ioctl not yet supported\n");
				return -EFAULT;
			}
			return 0;
		}
	}
	return -EINVAL;
}

static int get_floppy_geometry(int drive, int type, struct floppy_struct **g)
{
	if (type)
		*g = &floppy_type[type];
	else {
		LOCK_FDC(drive, 0);
		CALL(poll_drive(0, 0));
		process_fd_request();
		*g = current_type[drive];
	}
	if (!*g)
		return -ENODEV;
	return 0;
}

static int fd_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	int drive = (long)bdev->bd_disk->private_data;
	int type = ITYPE(drive_state[drive].fd_device);
	struct floppy_struct *g;
	int ret;

	ret = get_floppy_geometry(drive, type, &g);
	if (ret)
		return ret;

	geo->heads = g->head;
	geo->sectors = g->sect;
	geo->cylinders = g->track;
	return 0;
}

static int fd_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd,
		    unsigned long param)
{
#define FD_IOCTL_ALLOWED (mode & (FMODE_WRITE|FMODE_WRITE_IOCTL))
#define OUT(c,x) case c: outparam = (const char *) (x); break
#define IN(c,x,tag) case c: *(x) = inparam. tag ; return 0

	int drive = (long)bdev->bd_disk->private_data;
	int type = ITYPE(UDRS->fd_device);
	int i;
	int ret;
	int size;
	union inparam {
		struct floppy_struct g;	/* geometry */
		struct format_descr f;
		struct floppy_max_errors max_errors;
		struct floppy_drive_params dp;
	} inparam;		/* parameters coming from user space */
	const char *outparam;	/* parameters passed back to user space */

	/* convert compatibility eject ioctls into floppy eject ioctl.
	 * We do this in order to provide a means to eject floppy disks before
	 * installing the new fdutils package */
	if (cmd == CDROMEJECT ||	/* CD-ROM eject */
	    cmd == 0x6470 /* SunOS floppy eject */ ) {
		DPRINT("obsolete eject ioctl\n");
		DPRINT("please use floppycontrol --eject\n");
		cmd = FDEJECT;
	}

	/* convert the old style command into a new style command */
	if ((cmd & 0xff00) == 0x0200) {
		ECALL(normalize_ioctl(&cmd, &size));
	} else
		return -EINVAL;

	/* permission checks */
	if (((cmd & 0x40) && !FD_IOCTL_ALLOWED) ||
	    ((cmd & 0x80) && !capable(CAP_SYS_ADMIN)))
		return -EPERM;

	/* copyin */
	CLEARSTRUCT(&inparam);
	if (_IOC_DIR(cmd) & _IOC_WRITE)
	    ECALL(fd_copyin((void __user *)param, &inparam, size))

		switch (cmd) {
		case FDEJECT:
			if (UDRS->fd_ref != 1)
				/* somebody else has this drive open */
				return -EBUSY;
			LOCK_FDC(drive, 1);

			/* do the actual eject. Fails on
			 * non-Sparc architectures */
			ret = fd_eject(UNIT(drive));

			USETF(FD_DISK_CHANGED);
			USETF(FD_VERIFY);
			process_fd_request();
			return ret;
		case FDCLRPRM:
			LOCK_FDC(drive, 1);
			current_type[drive] = NULL;
			floppy_sizes[drive] = MAX_DISK_SIZE << 1;
			UDRS->keep_data = 0;
			return invalidate_drive(bdev);
		case FDSETPRM:
		case FDDEFPRM:
			return set_geometry(cmd, &inparam.g,
					    drive, type, bdev);
		case FDGETPRM:
			ECALL(get_floppy_geometry(drive, type,
						  (struct floppy_struct **)
						  &outparam));
			break;

		case FDMSGON:
			UDP->flags |= FTD_MSG;
			return 0;
		case FDMSGOFF:
			UDP->flags &= ~FTD_MSG;
			return 0;

		case FDFMTBEG:
			LOCK_FDC(drive, 1);
			CALL(poll_drive(1, FD_RAW_NEED_DISK));
			ret = UDRS->flags;
			process_fd_request();
			if (ret & FD_VERIFY)
				return -ENODEV;
			if (!(ret & FD_DISK_WRITABLE))
				return -EROFS;
			return 0;
		case FDFMTTRK:
			if (UDRS->fd_ref != 1)
				return -EBUSY;
			return do_format(drive, &inparam.f);
		case FDFMTEND:
		case FDFLUSH:
			LOCK_FDC(drive, 1);
			return invalidate_drive(bdev);

		case FDSETEMSGTRESH:
			UDP->max_errors.reporting =
			    (unsigned short)(param & 0x0f);
			return 0;
			OUT(FDGETMAXERRS, &UDP->max_errors);
			IN(FDSETMAXERRS, &UDP->max_errors, max_errors);

		case FDGETDRVTYP:
			outparam = drive_name(type, drive);
			SUPBOUND(size, strlen(outparam) + 1);
			break;

			IN(FDSETDRVPRM, UDP, dp);
			OUT(FDGETDRVPRM, UDP);

		case FDPOLLDRVSTAT:
			LOCK_FDC(drive, 1);
			CALL(poll_drive(1, FD_RAW_NEED_DISK));
			process_fd_request();
			/* fall through */
			OUT(FDGETDRVSTAT, UDRS);

		case FDRESET:
			return user_reset_fdc(drive, (int)param, 1);

			OUT(FDGETFDCSTAT, UFDCS);

		case FDWERRORCLR:
			CLEARSTRUCT(UDRWE);
			return 0;
			OUT(FDWERRORGET, UDRWE);

		case FDRAWCMD:
			if (type)
				return -EINVAL;
			LOCK_FDC(drive, 1);
			set_floppy(drive);
			CALL(i = raw_cmd_ioctl(cmd, (void __user *)param));
			process_fd_request();
			return i;

		case FDTWADDLE:
			LOCK_FDC(drive, 1);
			twaddle();
			process_fd_request();
			return 0;

		default:
			return -EINVAL;
		}

	if (_IOC_DIR(cmd) & _IOC_READ)
		return fd_copyout((void __user *)param, outparam, size);
	else
		return 0;
#undef OUT
#undef IN
}

static void __init config_types(void)
{
	int first = 1;
	int drive;

	/* read drive info out of physical CMOS */
	drive = 0;
	if (!UDP->cmos)
		UDP->cmos = FLOPPY0_TYPE;
	drive = 1;
	if (!UDP->cmos && FLOPPY1_TYPE)
		UDP->cmos = FLOPPY1_TYPE;

	/* FIXME: additional physical CMOS drive detection should go here */

	for (drive = 0; drive < N_DRIVE; drive++) {
		unsigned int type = UDP->cmos;
		struct floppy_drive_params *params;
		const char *name = NULL;
		static char temparea[32];

		if (type < ARRAY_SIZE(default_drive_params)) {
			params = &default_drive_params[type].params;
			if (type) {
				name = default_drive_params[type].name;
				allowed_drive_mask |= 1 << drive;
			} else
				allowed_drive_mask &= ~(1 << drive);
		} else {
			params = &default_drive_params[0].params;
			sprintf(temparea, "unknown type %d (usb?)", type);
			name = temparea;
		}
		if (name) {
			const char *prepend = ",";
			if (first) {
				prepend = KERN_INFO "Floppy drive(s):";
				first = 0;
			}
			printk("%s fd%d is %s", prepend, drive, name);
		}
		*UDP = *params;
	}
	if (!first)
		printk("\n");
}

static int floppy_release(struct gendisk *disk, fmode_t mode)
{
	int drive = (long)disk->private_data;

	mutex_lock(&open_lock);
	if (UDRS->fd_ref < 0)
		UDRS->fd_ref = 0;
	else if (!UDRS->fd_ref--) {
		DPRINT("floppy_release with fd_ref == 0");
		UDRS->fd_ref = 0;
	}
	if (!UDRS->fd_ref)
		opened_bdev[drive] = NULL;
	mutex_unlock(&open_lock);

	return 0;
}

/*
 * floppy_open check for aliasing (/dev/fd0 can be the same as
 * /dev/PS0 etc), and disallows simultaneous access to the same
 * drive with different device numbers.
 */
static int floppy_open(struct block_device *bdev, fmode_t mode)
{
	int drive = (long)bdev->bd_disk->private_data;
	int old_dev, new_dev;
	int try;
	int res = -EBUSY;
	char *tmp;

	mutex_lock(&open_lock);
	old_dev = UDRS->fd_device;
	if (opened_bdev[drive] && opened_bdev[drive] != bdev)
		goto out2;

	if (!UDRS->fd_ref && (UDP->flags & FD_BROKEN_DCL)) {
		USETF(FD_DISK_CHANGED);
		USETF(FD_VERIFY);
	}

	if (UDRS->fd_ref == -1 || (UDRS->fd_ref && (mode & FMODE_EXCL)))
		goto out2;

	if (mode & FMODE_EXCL)
		UDRS->fd_ref = -1;
	else
		UDRS->fd_ref++;

	opened_bdev[drive] = bdev;

	res = -ENXIO;

	if (!floppy_track_buffer) {
		/* if opening an ED drive, reserve a big buffer,
		 * else reserve a small one */
		if ((UDP->cmos == 6) || (UDP->cmos == 5))
			try = 64;	/* Only 48 actually useful */
		else
			try = 32;	/* Only 24 actually useful */

		tmp = (char *)fd_dma_mem_alloc(1024 * try);
		if (!tmp && !floppy_track_buffer) {
			try >>= 1;	/* buffer only one side */
			INFBOUND(try, 16);
			tmp = (char *)fd_dma_mem_alloc(1024 * try);
		}
		if (!tmp && !floppy_track_buffer) {
			fallback_on_nodma_alloc(&tmp, 2048 * try);
		}
		if (!tmp && !floppy_track_buffer) {
			DPRINT("Unable to allocate DMA memory\n");
			goto out;
		}
		if (floppy_track_buffer) {
			if (tmp)
				fd_dma_mem_free((unsigned long)tmp, try * 1024);
		} else {
			buffer_min = buffer_max = -1;
			floppy_track_buffer = tmp;
			max_buffer_sectors = try;
		}
	}

	new_dev = MINOR(bdev->bd_dev);
	UDRS->fd_device = new_dev;
	set_capacity(disks[drive], floppy_sizes[new_dev]);
	if (old_dev != -1 && old_dev != new_dev) {
		if (buffer_drive == drive)
			buffer_track = -1;
	}

	if (UFDCS->rawcmd == 1)
		UFDCS->rawcmd = 2;

	if (!(mode & FMODE_NDELAY)) {
		if (mode & (FMODE_READ|FMODE_WRITE)) {
			UDRS->last_checked = 0;
			check_disk_change(bdev);
			if (UTESTF(FD_DISK_CHANGED))
				goto out;
		}
		res = -EROFS;
		if ((mode & FMODE_WRITE) && !(UTESTF(FD_DISK_WRITABLE)))
			goto out;
	}
	mutex_unlock(&open_lock);
	return 0;
out:
	if (UDRS->fd_ref < 0)
		UDRS->fd_ref = 0;
	else
		UDRS->fd_ref--;
	if (!UDRS->fd_ref)
		opened_bdev[drive] = NULL;
out2:
	mutex_unlock(&open_lock);
	return res;
}

/*
 * Check if the disk has been changed or if a change has been faked.
 */
static int check_floppy_change(struct gendisk *disk)
{
	int drive = (long)disk->private_data;

	if (UTESTF(FD_DISK_CHANGED) || UTESTF(FD_VERIFY))
		return 1;

	if (time_after(jiffies, UDRS->last_checked + UDP->checkfreq)) {
		lock_fdc(drive, 0);
		poll_drive(0, 0);
		process_fd_request();
	}

	if (UTESTF(FD_DISK_CHANGED) ||
	    UTESTF(FD_VERIFY) ||
	    test_bit(drive, &fake_change) ||
	    (!ITYPE(UDRS->fd_device) && !current_type[drive]))
		return 1;
	return 0;
}

/*
 * This implements "read block 0" for floppy_revalidate().
 * Needed for format autodetection, checking whether there is
 * a disk in the drive, and whether that disk is writable.
 */

static void floppy_rb0_complete(struct bio *bio,
			       int err)
{
	complete((struct completion *)bio->bi_private);
}

static int __floppy_read_block_0(struct block_device *bdev)
{
	struct bio bio;
	struct bio_vec bio_vec;
	struct completion complete;
	struct page *page;
	size_t size;

	page = alloc_page(GFP_NOIO);
	if (!page) {
		process_fd_request();
		return -ENOMEM;
	}

	size = bdev->bd_block_size;
	if (!size)
		size = 1024;

	bio_init(&bio);
	bio.bi_io_vec = &bio_vec;
	bio_vec.bv_page = page;
	bio_vec.bv_len = size;
	bio_vec.bv_offset = 0;
	bio.bi_vcnt = 1;
	bio.bi_idx = 0;
	bio.bi_size = size;
	bio.bi_bdev = bdev;
	bio.bi_sector = 0;
	init_completion(&complete);
	bio.bi_private = &complete;
	bio.bi_end_io = floppy_rb0_complete;

	submit_bio(READ, &bio);
	generic_unplug_device(bdev_get_queue(bdev));
	process_fd_request();
	wait_for_completion(&complete);

	__free_page(page);

	return 0;
}

/* revalidate the floppy disk, i.e. trigger format autodetection by reading
 * the bootblock (block 0). "Autodetection" is also needed to check whether
 * there is a disk in the drive at all... Thus we also do it for fixed
 * geometry formats */
static int floppy_revalidate(struct gendisk *disk)
{
	int drive = (long)disk->private_data;
#define NO_GEOM (!current_type[drive] && !ITYPE(UDRS->fd_device))
	int cf;
	int res = 0;

	if (UTESTF(FD_DISK_CHANGED) ||
	    UTESTF(FD_VERIFY) || test_bit(drive, &fake_change) || NO_GEOM) {
		if (usage_count == 0) {
			printk("VFS: revalidate called on non-open device.\n");
			return -EFAULT;
		}
		lock_fdc(drive, 0);
		cf = UTESTF(FD_DISK_CHANGED) || UTESTF(FD_VERIFY);
		if (!(cf || test_bit(drive, &fake_change) || NO_GEOM)) {
			process_fd_request();	/*already done by another thread */
			return 0;
		}
		UDRS->maxblock = 0;
		UDRS->maxtrack = 0;
		if (buffer_drive == drive)
			buffer_track = -1;
		clear_bit(drive, &fake_change);
		UCLEARF(FD_DISK_CHANGED);
		if (cf)
			UDRS->generation++;
		if (NO_GEOM) {
			/* auto-sensing */
			res = __floppy_read_block_0(opened_bdev[drive]);
		} else {
			if (cf)
				poll_drive(0, FD_RAW_NEED_DISK);
			process_fd_request();
		}
	}
	set_capacity(disk, floppy_sizes[UDRS->fd_device]);
	return res;
}

static const struct block_device_operations floppy_fops = {
	.owner			= THIS_MODULE,
	.open			= floppy_open,
	.release		= floppy_release,
	.locked_ioctl		= fd_ioctl,
	.getgeo			= fd_getgeo,
	.media_changed		= check_floppy_change,
	.revalidate_disk	= floppy_  linux/dr,
};

/*
 * Fck/fl Driver initializationight= *  Copyrlds
 (C) 1993,19934
 */pyri Determine thelo(C) 19vers controller typeAlai/* This rout Copwas written by David C. Niemi/*
 static char __2  L get_fdc_ 199ion(void)
{
	int r;

	output_byte(FD_DUMPREGS);	/* 82072 and better know  (e thin/*
 	if (FDCS->reset)
		return FDC_NONE; less (r = result()) <= 0x00umping in case of eryte Noer topupt nt ???ansrors,
 * s= 1) && (reply_buffer[0]erta0x80)) {
		printk(KERN_INFO " Ala%d is an 8272A\n", fdc);ully easier toke prs ea re72a/765 don'tg etc),
 * cmes fil}rors,
r != 10 to
 *  it w
		    (but Iway.nit:el. I sho: unexpected y easieofefuls eas.ogrado moremmin, rglock/fiesl. I sUNKNOWN; cherors,
!nd rconfigureode rors, and orking,ightgraceful 't like072ith  dri. I've tried to
I 072e only set
does.ightUrgeCONFIGUREuldct tomCopyinglems ierPERPENDICULAR. I'rs,
need_eral_is nee()d myMORE_OUTPUTpyrighis needed. A0. I'} elseyrightAs with hd.c, all.91 -ine call in trhis file can (and will) be havlled
 * byA as found on Sparcs. cauorvaon'tandled. A haUNLOCK. I'* soopyr codrrorsc, alstainly a mess. I've tried my besterrors, and th hd.c, all routines w/

/-1991but h7eerroset a special tim1992 t7_ORIGerstaPpyrightAutoma, forerrupts, s
	(alm * dire/" dire cautiock foorveral) ||
 * by entropy@win!s hopeften by W callgraceful ee ro" direvery. Seemsopyrre ar havobl
 */all "several l d1, 1y entropy@winttoightcorrect t track-bufferingrARTIDnriedbase*
 * d on dire who alsightd onock/fl-change sig overrtecorvarrup/

/right1992/7/22 -- Hennus Bergman: Adde checkinge signaeporthd.c fipreli by entropy@wintermute.pi.edu (Lawrence Foard). Linuerrup/
ostn by Werner neer vertide suppor
 * cformg wriRevisedWernerAA passesy-onpyrigtests.id.ethzswitch: Ee rou * 19still>> 5unteder tohop:
	 onlEi

/* abut h8-1 ooph H. HoSL runnle cat 5Volts fil
 * call "floppy-on" directly, but h8all "floppy-detection and for8;t/88 V1.81by
 properly.
 */

/* 1992/9/20
 *44piuse. Mabe a newightioctl() sho caube creams tt2impoabltic fmodifyff fmrruptT
/*
 St impBhis file can (and will) ve fiftossible to3format a
 * floppy as the first thNTorvaal Semiconductor PC87306 DMA allocation & can (and will) belpe;
	defaultformat a
 * floppy a allocatioppy-change sigt impovariant-- Henun Urgncording=%dd DMA allocation & DMMS-DOS992/is f fdforer that makes it imp repong, f}alme* 1992 * crecalibr/*
 n Knlilo8 Alterruthe k Vojtateack-te. - hh by ght (Cos4/7/lags(make*iner
 typeparam*
 * 1992/922.12.typei somallo(i = 0; i < ARRAY_SIZE(l. I ss_d1, 1_992/9s); i++rmat/ e is ce19fullrSupportby R bigger
 [i].me new. vers |=8ded Al ``SI cat unfortu codly-- Sde tw bugs too..20
 * &= ~1994 /9/1}
	DPRINT("%s .
 * 0x%xions t1994/9? "Set *
 " : "Clearlizeri *
  );
}gn thtic f * mw threedamerru*/

/20
 *  * 1992/9/4/8/* 1994/ojta -afftingSreewaerror fdpatchan: Adr: Supb.
 tely some new*by Roatggin fixn bugu stufn Holtman -- added logging of phselect_delaying bKoen Holtman -- added logging of physical
/* opatioFD_SILENT_DCL_CLEAR alos  -- nnot
pposit* re

/*fff fovers, leav
/* d onse* thiIde-down"2 * HZ / 10)
 *one (i.e. Commodore' stufn: Add * 19"up&/10/1* 19~)t.
 *
 * 1992/9/5/8/26 t be re sigs Assumve f- Dan (C) hardwareed to 
/* y spstandarMA fprbroken"/flop4/24tingDan Fandrich be cmokded support for dummy-- Mo 199or.5" disks
curreman -- a IDs aTODO: rrupt92/7/22 be ro
/*
 * 1wrong number 199es, bff
 
 * r CMOS\tch" lly easii
 */
ved def2  Lion oberge1]pt meah for NULL.
 */< 0so c for NULL.
 */>= 8aed fnnel-- Dabad L.
 */e fo7tingRush@atn& meau to un'0#if N_0
 *> 198/06/0* faiAlan Cox>= 4ten
!FDC2fullwn crrorx370;
#endif/

/->ilurt for Co21998
/*
 * 1sectiiz put s/24 -- D%dions t1999/0port
 */

/*
struc 1992/9_tablndreacons bugfor*namanerDan F(*fn)esbersupport for Commodore 1581 3.5998/ug ivarbe
/* u NULme neto storppy writ}difwintdded log]ow thredata =
 * {"an Fa minut bimask", NULL, &all rnaldo Carvalh, 0xff, 0}, ide-bsolet * 192* 199ldo Caso de Melo <acme@consuppva.com.br> mea-es, bridded ethz._regsus_pcis/suser/capable/
 */

/*
 * 2001338/26 er -irqs/suser/caFLOPPY_IRQ, 6k/fl8 Aladmaox
 (linger */

aDMA, 2list4/29er-- add*/ * 200/suser/c1list a/09/20
/*
 *vid{"two * codule is goDC)
 *y PSlist aftetion thrrvTorvang bortinltaparmn: Adde{"but kpadhe d, who a; movisk- A (0x AltFD_INVERTEDright aftevfs su_dclll rod onproperors  mearequires mBROKENobvious ch* byag- s/e.wppendent code.
 */

/* 200TD_MSGt aftesil NULLcl_cons tch dependent code.
 */

/* 2003/ures to asm/flop/

/*
 ebu02/0 dependent code.
 */

/* 2003/DEBUudit oft the duleon'tgcan_use_virtual_dmaquirefix * (0mniboohs/suser/caL_defiGyte deecto
 */
cal
 * liyes 2
# NULLe 
 * essages for unexpecrl
 * mea(fifo_dept/* do printc
 *
#defiting
#atic ssagTc
 *.h>
#includo_c
 *ting2nclude <liusee <y.c
 *fs.h>d.h>clude <linux/tiilur",meanailur/suser/c<linux/timslows/suser/caPATC_bellucected interrction.
 */_lmesbergeci <user/- Koit _ctioected interrnoreg.h
#included<linux/timer.#includhdnux/e>
<linux/timL40SX#includslabnclude <linux/sl

	EXTRA_one... PARAMS Copycode tostorandrich  the propup(minued ar5" disks
 * which * 19<linux/* 1992 wor
	st base* aophe ks(str, of the "str002/)edawreome prel/fcn
 * o defining bit 1 of the "strned longseaturan put sectux/slab.cmpc.h>	/upt.h>
#includi].'t leep, oat bugnit.h>h>
#inauKoen		h@atnf for Commod8	1* faiKoenat a<linux.c
 *platy Ro_deve jifftriedwx/slab. for invalidate_bfnvicetab for invalidate_b almeestionn <linoch@atnnnus /18 --s cedo Cinlinux/dribeproperbeide <linux/slab.hmutex.h>vax/i4681	or resourc%s=stoppeincl	upside199ips e*that takrmat1 - 1/4 get modoxtreme}ppy;
y easie1oppy;d.h>20
 * x/slab.hf the
/*
 * 1ck/fl taRid fod46818r [%s]*easisom)mer.inux/time<acme@clinux/tchine:.
 */
inuxude <linux/slab.hlmesbergeLOPPY_IRQ = 6;
stppy;f for ve %#inc for invalidate_bice.hoppyf for vees.
 */ -- Ando CresourceoefinisystemLOPPY_IRes.
 */
/*
 * 1Read Documenode on/bightdevLEAR

#.txializ &
y easie0ort
 */

/*
storhave_d.h>dc = -ENODEV <linux/slssize_es.h>
#inilur_show.c
  24
/* === *devthan  par-on"back o_attribuutionugh, <linuxbuf2.12.king24
 nvalidat not en p = toEBUG	/* debug di(ave to stordo Camer.L.
 */

p->idmises.ines wx/hdf(buf, "%Xions U.
 *
 * /flopDEVICE_ATTR(orkquS_IRUGO,yata d boomn rec,userOPPY/24 -- Dan Fbellucd not enreleaseuMA when not en vir2.12.noafe debu DM or morppsuthe fiIfixed
ot saftic fone..ufdc* by defin.
 *c0;n: A <ov - FBergbte neers,
lmesbeaddress/7/2-1 neea burcume
 * d (- 2003/RESET_ALWAYS, . Thing virtu2 =eset it inMA when no_pm_op- ystem.ock);

s/08/2.umiro. tual circumstan>
#incstorebug di_h" pas ho3ster_inux/slnot
nt/*
 	/* debor b99/flograb
statikes rtet_dor(voidf	 ==== = "bellucennus.pPPY_&te neeunsign/*
 }sberger masirq, te. n vir_id); not sc int se_irq_[N_DRIVEt(in the followingkobjng, *bellucdfindin o_en not for *part, Dan F/07 -ain KnaffL.
 */

(disab & 3) | (ludemot" pacoun Dan Fanrs,
-& DMA fs. By/sus||
 PATC!(<acme@conectiva.co & (1 <<ngt sa)lso puters/13 codee[FDCaededo)].-- Paul ==er to undfully easieusere routinese dri>>dma mach1f*/
#sof the "strbellucd"ups)/mc1ove maon't llx/mcmot of
 ly easie/mc1 199( 199s[c, ch(0x1),Alpha afc
 *fers() LOPPY_Iby ca allain Knaff , unit,0 usly easg uerrd_dr;
rmako
statid(o extr_PPC)id Weinmakerlegacy_f thet voi1h
 * ch ``Seises.
 *llowa bi
	raw_cmd =ppy heaby defidar m0; dr've t PCI pt(iput sectts refer] = chancmty cl1===
 m. Not(int irna chanperar misesMEMoppy;gotoer@n_need 199py.h.
e <linux/sla->majhar mone... MAJOppy.h for ter 2tfirst_minquestTOMINOR(dy entrte nee strucgupt.s	0x10000id doppyn othA
 
static voi 199_'t l, "fd%k buy_qux
 *hnit_n and(&drive_off
statibilitoppyd_ * a_t fresidue().07 -0/0;

#residue(FLOPPYring taDfuncux/ti= dma_residucallbacport>

al Doctregister_breque( *ved det_ref fd_y conrs,
ing  oncx/slab.hcomple
 */*/
staticis the ma
ate _free_t f(d_requesc, chapages( do ,
 * aorder(siunregmem_frex/blkma_porquekqueablkveM by (siz(deren_

/*estpt(ima_porin oome prel!_get_dma_pazirq.h>definel
 * ux/tim_residualloc(GFc, chaof '0'2-drstati_max_seuff swe x/timstati, 64ue(FL2-drloc
#def to _on(MKDEVe(size) ault) f0), 256, THIS_MODULEtual PATCete DEVIpy /suser/ca*/

loppring task  = 6;
256_mem_a once, ITYPE(i#defi__get_dmnd sate_urn_t flop"ups[safe tua].nd s9/ux/buffer_\n");
	* do  = (chMAX_DISKe "stit (1/e
#dasf viule NULLout(MAXTIMEOUT frestem.hue
#",d dmt.h>
# 
 * upt.h>
"upssllucortag

stemporaril1998i/

/inux/inie sevupt(	/flopSTRUCT and 
 * almesbedy.c
 -m/E: T4) << void 0x4;e <linux/asm/__s8.02__and ces fo flopmc68000__g ba/*o/
	icludn3xway002/A pr a DORsed ot ude <lwe And es.
m_memstuffo3.5"#ia_re C(x) ((
#den not eMACH_IS_SUN3X)on't  bitne UNIT(0). Bitson and forerrunit) + (ee
#sages for unexpyte t print_unex = 1;
 &sefin7on't;

#d0]. do ved taticefin * hat_e FDC])*/
static RWntly fdef Feegistee ffdx10000		
 * a_t l)
{
#iAME "ring taCAN_FALLBAEBUG	/e <limakov - Fix io nt_drive])1t_drive])
#E (&2 "ock/fl"

v*
 *verslow_ rSTF(arch. Maatedofection.
 */

#includemakerdefine
 * 	prwed driveandnexpsome prelze) ecaseS (&nd rate e[fdcrent_drive]1995/FBUSYgram_bit(x##_BIT, &DRS->fdisk)
-- Al 992/9/slude0x3frive])aker DMA.h>
 */

fnfigfine #includblkpge "sut sectR
#defixtor UDR) set_TF
#deset)
#defWE
 * aUSETFA ha memoNEWCHANG/
staticUTES, &UDRte
#defirun bT, &UDRS->VERIFYatic P (&fdcdvalhded */
statiar *)nodrnot ntropy_order(si		umpintropying in ;cause
 } hig
esbeSesbem10 msec * 1995to lee UCr ava annge;IT, &DP asesy,hea(drive)]he pro migf the "py@wme ned,rpy todc_stSned uent_dteuff *:ine /
	msleep(1
 *
 seon virt#define >>2) & 0x1fent_drive]e *flo UNIT(xFP_Kal 2)ent_driveS->flaRfineSIOu#incaultowed fdef Fmd[1]< 4fdc_sting bai (&drifine,[wed  Endle --cel. I sweefine DR/
stat ((f 3.5" u#I calmesberawdelay.helsee is cereable each I've ts capaue() ie stuty ejec/* fies.llocatsest_brve/7/2* 19s[s[current_drive]UDRS shift%d: " fonc
#de.S (&fdcse rooppyemd[3]*/
staticSE/
statimingIT(xcmd[0]
#def D underonclud->cmd[4]
fdef* 02r=
 *ce_er*  Copyright (C) 1 -- Adde.
 *
 * 19 #includR_TRAC2]
#94/7/13 -- PaulA:
 *d on aw_cmd->cmd[2]
* 19de tf
 R_TRAC6#define F_GAP_FILL raw_cmd-7#define F_SIZECODE2_FILL raw_cmd-8#define F_NR_RW 9psiden put s#ifd&fdc_stFiver:rent r_PER_TRACK_FILL pt(ine])
rivee FDC#defin== 2ten
aw_cmd->cmd[2]<ine DP (&dr_FILL 84
 *
 * 1992globalsoflude	rive <lifine Tle --iro.a98/06mFDCs seemd[3]be , 6 hto hand* sual 0). Bitscommandsevept anperly, sondif on /aw_cit(orply biro.au>ines /c-- P64KBd fr- Da headad_cmd->c
garbagee mu]
 */F->cmd[5]
#define SECT_PER_TRACK raw_cm/
#incI've trMAXrive])
#define UDRWE (&wr for NULL.
 */

f
 ge;
sve)])ootmr[3rentf (r reply_bufirqLOPPY_IRQ =nolt is used when4].
 *
 * es.
 */
definer reply_bufive])
#defineflush_widat#def
#) r[6])
#define  UFDCSraw_fdc_stateZECODE2 rSEheadcne FDClude0t_drive] Fanbelat0x1el. reply_buffer[5])
e_allowe[ved d(flostruct 8 2))
#defize (in kil* a dma mem_SI "md[8]
#dove macy_buffer fordaallowed drivea't ld 3ng])	/}r NUnd srs[cur_ add<fies smame values in jiffies s cleefine Nms[] = {sideNOTE:efine Nootin
#ifnve *oc
#defcodesuppcifferent oc
#de in jiffies  6

/*
 * e) __gdifferent SEL_DLY (2*Hyoppy)->s   |  ossi/drifile toime,flopp
 |   |  |rval,&Alan >1) boom. |   |H
 * unfer[p rate iFALLBABUG	/* debugsed)
/mess.be lkdeupt.up..92 - and ate nefine->priv|   vA)
#en*/

s *)(h>
#)impoin ms    |     Tfine 2)
		)(char **addrl, usec    |  own t.
 *
 * GENHD_FL_REMOVABLF_SEC  |   |   |   includfsloppo_fdne void(&drval,/*
 erval|   add.h>	/*lude <lifine thefine atic DEFI
aldo Cspinupp rat (jif:
	BUG	/* debug diPS
  |t.h> needed fonterval, usec   e, msec (not u:,getc (nf), 0, tdnot usome prelulluc_causjfull unknown.
 * [NIZECODE2 raw_cefine UFDCS (&fdc:ory2/02down offsDEBUG	/* debug di mor2umping in ;o more2-drown oup  |    (param*f
}

,t Max numberincludx number o  aw_cmd->down offs    |     it a_allCAN_FALLBAP_rkin:
 more     | P_KERN_dma != 2)
		rereeAN_Fr(size))
#en:
	whilclear--efines[current_drdma_residue(FLOPPY_
 * aze))
#enta_reue * )yte rlude <aserrort
 */

/*
 EFINE_SPI direrval, usown ofn_nodmaccode tod, 32 MA whenioS (&fdc
 * sk =offS->fk(DEV=)/2-dl}Z, 17, {3,])	/ shor8.)
 1 
 * /* efine DR+ 3nes addeTOR rFILL raw_cmd-pnp bioddedfomoistobotriedker  4, 2/2, 1 }, "360K PC"6nes ILL raw_s, ancmay0, 1taking baIDE.ine SUn Holtman sberAdaptppy)mesberger
 *t.12.:-(,*HZ/10,7, /*2 Almpt(int irnlock/flogran.
 * [Ncdrom->cmxow it is move malloc  40, 3*H, 0, 0, 0,1*p2.12.3*HZ, 2p/7/2 0,{ 4,22,at bugp-- */

/
 * [Now it md[3]
#define S+es, 4,22
 inux1nd st0, 16}
iffe* drddedbefEND(X) (&((X)[of the "strX)]))loppited; oistopcirecowing , 20{ 4,sec (no,,2}, 0, 833*HZ, 2020{3,1,2,2,0fdc_stITY/*y_in4,22,21,; p 1 of the 3*H{ 4,22,21,3; pput sectors !} DD*/
EL_DL0, SEL720k", SE/*3 1/2 DD"ups{{4,  50fake_ for"raw_cmd-by confi(C) 199io- GAPw sa04lxstruus<rgooc,  83, 3*HZ, 40, {3,1,2,0ormat 6, 16, 3000     1Z, 20HZ, 20 0, {LY, 5pormat >
#intual#define 20
 * tatic DEFINE_SPINLOC50 15,  5,  8, 3000, DLY,  { 7,,  85 8, , 1531,2  8, 3004, /*5 1*HZ, 20,0, { 75  -- 30088M" } /*3 1s nes-- add0, {fr7]
#define SIZECODE2 raw/

static0		/*er offfi headsinux/pin 3*HZ&drisavtepis theiZ/10, 3*HZ1995agpt(int ir |    |    fine USE it e
/*
whtati
irqretnvirt		o more    |   Nadete fo2,0,2 10, Sf foruencyded xpected inteethz.s valugned;
static voi ``Sectne SWeraw_print)-> |     |   iffer&dri(), wa, 20.wpiruct dcntl.t/

/UCT*(x)mown offset (  |   |   | of param_drive_  y_dridefine R_SECTOUnyte aultcurr IRQ%d0a mess. ht (C) 19stat DMA allocatioone... skitialnux/ bug disk  by dhecks */
};

static struct fl	];
static s*3 1/2msne FDC_ * 19ss. By de]alling bappy_drive_strrs[cu 8M" }1,29efin3)	/* dt drive_dma raw_rw_cmd, default_raw_cmd;

genDMAk * fixaw_cmd, default_raw_cmd;

tual 
/* ===DMA 6

/*
 *ve])
/

/*
 * global[c/24 -r, sizs[current_drive])
/

/*
 * globalscurre[] = dre.g. 360kB diskette inuraw_cmd-fd_raw_cmd;

   |T_bdevw_cmd, default_raw0, 16, MUTEX(open_ightignedlt_raw_cmd;

/*
k/flo#include*#includ,es in jif#includCopyright2.12.stpy_dri&fdc_ssHZ/2,  1,2,A prnnus Berge SECT_Pin/*
 *atrive_strmd[3]
#define Sedefinatic linux, 7 }y_drive_31,21}, 3*, 8 /*
 * ;
statilv. s" CP/M di.ch)defin excepe UCes.
07 -- 1581'ude gical-- add.
 */a;
/hyss).
 * '[5]
#defi_infofloppit (buoutb
 * 'stdorIO16, Oerge
ld probault of GE]
#dchar0, ~0, 8gs ea esult ommedi1995CODE2 raP

/* * 'stre gracall "llucmiss and
 * 1 on->cmd[3]
#define Sphysicreasneedveraletch Ber,withiamed'_struct Thifferenr wir bery, {3,nimum 0 = usl)
 d som Err/

/*
ruct to* Urgeifeplyy were/ 1*HZ*   st\"not, 3*H. Byult of GEirq|    4*HZ/10, tells  |     |   is on t ti,rse maphould sse od.
 *meout
&*    | --0x01>ux/ttic structatic Intow it is rather_bdev[N_DRIVE];
static DEFINE_MUTEX(open_lock);but with a side ID, and vice-versa.
 * This is the same as the SharpZ-80 5.25" t
 */

/*
 * 19elf-explanatory.h>
#in rato/

static intoldve tr6e FDC(*one... SANITY_CHECKr penrp
	/
) >> c Aypes.
rive_mait) + nit) + (-expltmp { 1, pres  |   |  |  | efinelf-explana(gap2) */Na.h>	_bdev[N_DRIVE];
static DEFINE_MUTEX(open_lock);fine--];
static struFe voidrive_params[N_DRIVE];
static struct floppy_drive_str_dr_cmd *ray_raw_cmd *raw_cmd, default_raw_cmd;

/*
 * This y.c
 ** For Am/fcnd Cte */
#d)
#e	/* * Oistopp2eaturamed0" },	/ (bued as 0xC0<<2ight 80,0,0x1B,0xPC w_cmd *ra 2 2) >>1) 9ded incluv - Fix io D360"  }1, ~8[0])	/load
	   ams[dre_raw__hd someer_list[8]
#d 0 (but disks&& # DMAs)16, 4dc_stP0,hope |2) *|/

0, 9,2,40,1,0x23,0H* 1024truct t /fmave f  |    |    )720KB 3.5" 9,2ams[{|   ec!
 CM.5"    */
	{ ve, et , #360"  },	/*  5 360K_HEAD(	,2,40,1mi1,0xxCF,0x6CaxE_NAMEtingE2 rait a/
	{ (*/
	{ 1,p2) 0x50 9,2,80perp,  500,,0x1BE2880" }p2) */
staSpec1 (step)

/*
 * this struct defin0])	*_SIZdifferens id Mipdc_s_por sng(dma_residue(FLO +ct {
	stq, voidL_DEBdma_r 10,  2880,%dirqre sa

/* AMI Bstructags)
#defin,yte  9 3.12M1,0xDfine UDRWE (x01,f for ve  0, {gh1440" slow0 1.44M:%res.,8,0x0c
 * Be   |{ 2400,10,x01,1C01,0x,040" },0rH16803*HZ, 4 auxiliarenever t,41,1xDF,0x0,"h7  8 by cons. Iot u10"  },	/* ma_porot uB 5.25"   */
ot u,0x02,01C,0x00 by coload
	   0x40 f*  8 for]rst
 * 199he first LSB (	   2(unitfli in s,0,0xmostparams,ff foriunknown.
 * [Now it is ratheryte 14" },	/*or4,25perp
	dma_me { 7, 4,2 linux15 1.7lopp not 250, 1w threeparseinclude_cfg_st- add* 16 4cfg }, "3linuxpts/io.*    | 3440, size_LOPPph>
#infg;8/
	{  72},	/*!= ' '0xD2 4102,"h1\t';0x2sectol" },	/*0xDF,2988,	},	/*8 1.0's side5.25senttioxDF,34pfcn   | 2-drivet(int2,42, 8, 4,2  |      |
om/

/*
. -Davmok butdif
 1;

#ng ba
	{ w
/*
 5.2583,"h4125,	/* 14" },	/2B ,0x0"ch,0x54,"E,0xCF,080"  /flop},	/"h8.25" /* 12 41004MB 3.5"0, { 7, 4,2s systemxx09  */
	{ 2240,1" },  D2000 */
, | rate, haic struct tim 9,2,80,0      |   | /*  8 2.88"h160Max0,
 7, 8, 4,25,(((x000, 4*HZ/10, 3*HZ, 1x1C,0x20,0xCF,0x2C,"h1600" }, /* 23| - 1.6Err00) for Com|    ppy_dri5.25" CPMips sve temasklorive])
#d_synce Sharp's loHZ/2,  8  |    7433*HZ,ive types.
 */
static struct {
	stn&&severalnt_drive]);
	const char *name;!izPortn kilos easer pghis stic struct tim   *520" |   Hemo 27 3Sing
 *				ect delay
  |    ,0x1C,0x20,0xC0x2C,"h1600" }, /40" }, /* 28 3.8484MB 3.5"   * 1.6ppy.h. o60 KB ,stru3,1 - gHZ, |      1C,0x1- get 20KB 9C,0xine UDRWE (&wr10,2,8E,"h4180,13,2,80,xDF,1non0 is on * 199s Max number o/
	{ 368,80,0,0x1C,0x10,0xCF,0    |  | |  | |    | ut*HZ/},e3ded * 27,q EP^odmaaow s_  sidn
sta15,0x4MBSIZE2 0,"D11225"   */
2 */
s 2240,1es, bion: Diil    p0,0,0x2l0xCF,next m_device *o  */
ccurs.  },but with a side boom.ppy_dri*_dma_me_AUTHOR("Ald toL.disks
ng &
rovideSUPPOnon-obt in (Z,  0, t_type LICENSE("GPLch" pes.2"1.2.5"   2,acor ulyd[5]88M d 6, 60iffen 8, 4,2e
#deit 2r struobing,0x1C,0x8M" , 3*HZ, 2,   31 1.61struc,  5pn   |*3 1/2  SELPNP0700", 0Z, 14{ }T*/
t_type it in oT 31 (pnp1995/16, 4 ===_ray.#   |:
_25,0x0* 1* Au6= 1995/40,1,0xu (noAuto-de25,0x02,0xDF4800"gs)
#defit_type dLIAS_Bdireual_size)_dma != 2)
		);