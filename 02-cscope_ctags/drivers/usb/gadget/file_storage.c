/*
 * file_storage.c -- File-backed USB Storage Gadget, for USB development
 *
 * Copyright (C) 2003-2008 Alan Stern
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the above-listed copyright holders may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * The File-backed Storage Gadget acts as a USB Mass Storage device,
 * appearing to the host as a disk drive or as a CD-ROM drive.  In addition
 * to providing an example of a genuinely useful gadget driver for a USB
 * device, it also illustrates a technique of double-buffering for increased
 * throughput.  Last but not least, it gives an easy way to probe the
 * behavior of the Mass Storage drivers in a USB host.
 *
 * Backing storage is provided by a regular file or a block device, specified
 * by the "file" module parameter.  Access can be limited to read-only by
 * setting the optional "ro" module parameter.  (For CD-ROM emulation,
 * access is always read-only.)  The gadget will indicate that it has
 * removable media if the optional "removable" module parameter is set.
 *
 * The gadget supports the Control-Bulk (CB), Control-Bulk-Interrupt (CBI),
 * and Bulk-Only (also known as Bulk-Bulk-Bulk or BBB) transports, selected
 * by the optional "transport" module parameter.  It also supports the
 * following protocols: RBC (0x01), ATAPI or SFF-8020i (0x02), QIC-157 (0c03),
 * UFI (0x04), SFF-8070i (0x05), and transparent SCSI (0x06), selected by
 * the optional "protocol" module parameter.  In addition, the default
 * Vendor ID, Product ID, and release number can be overridden.
 *
 * There is support for multiple logical units (LUNs), each of which has
 * its own backing file.  The number of LUNs can be set using the optional
 * "luns" module parameter (anywhere from 1 to 8), and the corresponding
 * files are specified using comma-separated lists for "file" and "ro".
 * The default number of LUNs is taken from the number of "file" elements;
 * it is 1 if "file" is not given.  If "removable" is not set then a backing
 * file must be specified for each LUN.  If it is set, then an unspecified
 * or empty backing filename means the LUN's medium is not loaded.  Ideally
 * each LUN would be settable independently as a disk drive or a CD-ROM
 * drive, but currently all LUNs have to be the same type.  The CD-ROM
 * emulation includes a single data track and no audio tracks; hence there
 * need be only one backing file per LUN.  Note also that the CD-ROM block
 * length is set to 512 rather than the more common value 2048.
 *
 * Requirements are modest; only a bulk-in and a bulk-out endpoint are
 * needed (an interrupt-out endpoint is also needed for CBI).  The memory
 * requirement amounts to two 16K buffers, size configurable by a parameter.
 * Support is included for both full-speed and high-speed operation.
 *
 * Note that the driver is slightly non-portable in that it assumes a
 * single memory/DMA buffer will be useable for bulk-in, bulk-out, and
 * interrupt-in endpoints.  With most device controllers this isn't an
 * issue, but there may be some with hardware restrictions that prevent
 * a buffer from being used by more than one endpoint.
 *
 * Module options:
 *
 *	file=filename[,filename...]
 *				Required if "removable" is not set, names of
 *					the files or block devices used for
 *					backing storage
 *	ro=b[,b...]		Default false, booleans for read-only access
 *	removable		Default false, boolean for removable media
 *	luns=N			Default N = number of filenames, number of
 *					LUNs to support
 *	stall			Default determined according to the type of
 *					USB device controller (usually true),
 *					boolean to permit the driver to halt
 *					bulk endpoints
 *	cdrom			Default false, boolean for whether to emulate
 *					a CD-ROM drive
 *	transport=XXX		Default BBB, transport name (CB, CBI, or BBB)
 *	protocol=YYY		Default SCSI, protocol name (RBC, 8020 or
 *					ATAPI, QIC, UFI, 8070, or SCSI;
 *					also 1 - 6)
 *	vendor=0xVVVV		Default 0x0525 (NetChip), USB Vendor ID
 *	product=0xPPPP		Default 0xa4a5 (FSG), USB Product ID
 *	release=0xRRRR		Override the USB release number (bcdDevice)
 *	buflen=N		Default N=16384, buffer size used (will be
 *					rounded down to a multiple of
 *					PAGE_CACHE_SIZE)
 *
 * If CONFIG_USB_FILE_STORAGE_TEST is not set, only the "file", "ro",
 * "removable", "luns", "stall", and "cdrom" options are available; default
 * values are used for everything else.
 *
 * The pathnames of the backing files and the ro settings are available in
 * the attribute files "file" and "ro" in the lun<n> subdirectory of the
 * gadget's sysfs directory.  If the "removable" option is set, writing to
 * these files will simulate ejecting/loading the medium (writing an empty
 * line means eject) and adjusting a write-enable tab.  Changes to the ro
 * setting are not allowed when the medium is loaded or if CD-ROM emulation
 * is being used.
 *
 * This gadget driver is heavily based on "Gadget Zero" by David Brownell.
 * The driver's SCSI command interface was based on the "Information
 * technology - Small Computer System Interface - 2" document from
 * X3T9.2 Project 375D, Revision 10L, 7-SEP-93, available at
 * <http://www.t10.org/ftp/t10/drafts/s2/s2-r10l.pdf>.  The single exception
 * is opcode 0x23 (READ FORMAT CAPACITIES), which was based on the
 * "Universal Serial Bus Mass Storage Class UFI Command Specification"
 * document, Revision 1.0, December 14, 1998, available at
 * <http://www.usb.org/developers/devclass_docs/usbmass-ufi10.pdf>.
 */


/*
 *				Driver Design
 *
 * The FSG driver is fairly straightforward.  There is a main kernel
 * thread that handles most of the work.  Interrupt routines field
 * callbacks from the controller driver: bulk- and interrupt-request
 * completion notifications, endpoint-0 events, and disconnect events.
 * Completion events are passed to the main thread by wakeup calls.  Many
 * ep0 requests are handled at interrupt time, but SetInterface,
 * SetConfiguration, and device reset requests are forwarded to the
 * thread in the form of "exceptions" using SIGUSR1 signals (since they
 * should interrupt any ongoing file I/O operations).
 *
 * The thread's main routine implements the standard command/data/status
 * parts of a SCSI interaction.  It and its subroutines are full of tests
 * for pending signals/exceptions -- all this polling is necessary since
 * the kernel has no setjmp/longjmp equivalents.  (Maybe this is an
 * indication that the driver really wants to be running in userspace.)
 * An important point is that so long as the thread is alive it keeps an
 * open reference to the backing file.  This will prevent unmounting
 * the backing file's underlying filesystem and could cause problems
 * during system shutdown, for example.  To prevent such problems, the
 * thread catches INT, TERM, and KILL signals and converts them into
 * an EXIT exception.
 *
 * In normal operation the main thread is started during the gadget's
 * fsg_bind() callback and stopped during fsg_unbind().  But it can also
 * exit when it receives a signal, and there's no point leaving the
 * gadget running when the thread is dead.  So just before the thread
 * exits, it deregisters the gadget driver.  This makes things a little
 * tricky: The driver is deregistered at two places, and the exiting
 * thread can indirectly call fsg_unbind() which in turn can tell the
 * thread to exit.  The first problem is resolved through the use of the
 * REGISTERED atomic bitflag; the driver will only be deregistered once.
 * The second problem is resolved by having fsg_unbind() check
 * fsg->state; it won't try to stop the thread if the state is already
 * FSG_STATE_TERMINATED.
 *
 * To provide maximum throughput, the driver uses a circular pipeline of
 * buffer heads (struct fsg_buffhd).  In principle the pipeline can be
 * arbitrarily long; in practice the benefits don't justify having more
 * than 2 stages (i.e., double buffering).  But it helps to think of the
 * pipeline as being a long one.  Each buffer head contains a bulk-in and
 * a bulk-out request pointer (since the buffer can be used for both
 * output and input -- directions always are given from the host's
 * point of view) as well as a pointer to the buffer and various state
 * variables.
 *
 * Use of the pipeline follows a simple protocol.  There is a variable
 * (fsg->next_buffhd_to_fill) that points to the next buffer head to use.
 * At any time that buffer head may still be in use from an earlier
 * request, so each buffer head has a state variable indicating whether
 * it is EMPTY, FULL, or BUSY.  Typical use involves waiting for the
 * buffer head to be EMPTY, filling the buffer either by file I/O or by
 * USB I/O (during which the buffer head is BUSY), and marking the buffer
 * head FULL when the I/O is complete.  Then the buffer will be emptied
 * (again possibly by USB I/O, during which it is marked BUSY) and
 * finally marked EMPTY again (possibly by a completion routine).
 *
 * A module parameter tells the driver to avoid stalling the bulk
 * endpoints wherever the transport specification allows.  This is
 * necessary for some UDCs like the SuperH, which cannot reliably clear a
 * halt on a bulk endpoint.  However, under certain circumstances the
 * Bulk-only specification requires a stall.  In such cases the driver
 * will halt the endpoint and set a flag indicating that it should clear
 * the halt in software during the next device reset.  Hopefully this
 * will permit everything to work correctly.  Furthermore, although the
 * specification allows the bulk-out endpoint to halt when the host sends
 * too much data, implementing this would cause an unavoidable race.
 * The driver will always use the "no-stall" approach for OUT transfers.
 *
 * One subtle point concerns sending status-stage responses for ep0
 * requests.  Some of these requests, such as device reset, can involve
 * interrupting an ongoing file I/O operation, which might take an
 * arbitrarily long time.  During that delay the host might give up on
 * the original ep0 request and issue a new one.  When that happens the
 * driver should not notify the host about completion of the original
 * request, as the host will no longer be waiting for it.  So the driver
 * assigns to each ep0 request a unique tag, and it keeps track of the
 * tag value of the request associated with a long-running exception
 * (device-reset, interface-change, or configuration-change).  When the
 * exception handler is finished, the status-stage response is submitted
 * only if the current ep0 request tag is equal to the exception request
 * tag.  Thus only the most recently received ep0 request will get a
 * status-stage response.
 *
 * Warning: This driver source file is too long.  It ought to be split up
 * into a header file plus about 3 separate .c files, to handle the details
 * of the Gadget, USB Mass Storage, and SCSI protocols.
 */


/* #define VERBOSE_DEBUG */
/* #define DUMP_MSGS */


#include <linux/blkdev.h>
#include <linux/completion.h>
#include <linux/dcache.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kref.h>
#include <linux/kthread.h>
#include <linux/limits.h>
#include <linux/rwsem.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/freezer.h>
#include <linux/utsname.h>

#include <asm/unaligned.h>

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>

#include "gadget_chips.h"



/*
 * Kbuild is not very cooperative with respect to linking separately
 * compiled library objects into one module.  So for now we won't use
 * separate compilation ... ensuring init/exit sections work to shrink
 * the runtime footprint, and giving us at least some parts of what
 * a "gcc --combine ... part1.c part2.c part3.c ... " build would.
 */
#include "usbstring.c"
#include "config.c"
#include "epautoconf.c"

/*-------------------------------------------------------------------------*/

#define DRIVER_DESC		"File-backed Storage Gadget"
#define DRIVER_NAME		"g_file_storage"
#define DRIVER_VERSION		"20 November 2008"

static const char longname[] = DRIVER_DESC;
static const char shortname[] = DRIVER_NAME;

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("Alan Stern");
MODULE_LICENSE("Dual BSD/GPL");

/* Thanks to NetChip Technologies for donating this product ID.
 *
 * DO NOT REUSE THESE IDs with any other driver!!  Ever!!
 * Instead:  allocate your own, using normal USB-IF procedures. */
#define DRIVER_VENDOR_ID	0x0525	// NetChip
#define DRIVER_PRODUCT_ID	0xa4a5	// Linux-USB File-backed Storage Gadget


/*
 * This driver assumes self-powered hardware and has no way for users to
 * trigger remote wakeup.  It uses autoconfiguration to select endpoints
 * and endpoint addresses.
 */


/*-------------------------------------------------------------------------*/

#define LDBG(lun,fmt,args...) \
	dev_dbg(&(lun)->dev , fmt , ## args)
#define MDBG(fmt,args...) \
	pr_debug(DRIVER_NAME ": " fmt , ## args)

#ifndef DEBUG
#undef VERBOSE_DEBUG
#undef DUMP_MSGS
#endif /* !DEBUG */

#ifdef VERBOSE_DEBUG
#define VLDBG	LDBG
#else
#define VLDBG(lun,fmt,args...) \
	do { } while (0)
#endif /* VERBOSE_DEBUG */

#define LERROR(lun,fmt,args...) \
	dev_err(&(lun)->dev , fmt , ## args)
#define LWARN(lun,fmt,args...) \
	dev_warn(&(lun)->dev , fmt , ## args)
#define LINFO(lun,fmt,args...) \
	dev_info(&(lun)->dev , fmt , ## args)

#define MINFO(fmt,args...) \
	pr_info(DRIVER_NAME ": " fmt , ## args)

#define DBG(d, fmt, args...) \
	dev_dbg(&(d)->gadget->dev , fmt , ## args)
#define VDBG(d, fmt, args...) \
	dev_vdbg(&(d)->gadget->dev , fmt , ## args)
#define ERROR(d, fmt, args...) \
	dev_err(&(d)->gadget->dev , fmt , ## args)
#define WARNING(d, fmt, args...) \
	dev_warn(&(d)->gadget->dev , fmt , ## args)
#define INFO(d, fmt, args...) \
	dev_info(&(d)->gadget->dev , fmt , ## args)


/*-------------------------------------------------------------------------*/

/* Encapsulate the module parameter settings */

#define MAX_LUNS	8

static struct {
	char		*file[MAX_LUNS];
	int		ro[MAX_LUNS];
	unsigned int	num_filenames;
	unsigned int	num_ros;
	unsigned int	nluns;

	int		removable;
	int		can_stall;
	int		cdrom;

	char		*transport_parm;
	char		*protocol_parm;
	unsigned short	vendor;
	unsigned short	product;
	unsigned short	release;
	unsigned int	buflen;

	int		transport_type;
	char		*transport_name;
	int		protocol_type;
	char		*protocol_name;

} mod_data = {					// Default values
	.transport_parm		= "BBB",
	.protocol_parm		= "SCSI",
	.removable		= 0,
	.can_stall		= 1,
	.cdrom			= 0,
	.vendor			= DRIVER_VENDOR_ID,
	.product		= DRIVER_PRODUCT_ID,
	.release		= 0xffff,	// Use controller chip type
	.buflen			= 16384,
	};


module_param_array_named(file, mod_data.file, charp, &mod_data.num_filenames,
		S_IRUGO);
MODULE_PARM_DESC(file, "names of backing files or devices");

module_param_array_named(ro, mod_data.ro, bool, &mod_data.num_ros, S_IRUGO);
MODULE_PARM_DESC(ro, "true to force read-only");

module_param_named(luns, mod_data.nluns, uint, S_IRUGO);
MODULE_PARM_DESC(luns, "number of LUNs");

module_param_named(removable, mod_data.removable, bool, S_IRUGO);
MODULE_PARM_DESC(removable, "true to simulate removable media");

module_param_named(stall, mod_data.can_stall, bool, S_IRUGO);
MODULE_PARM_DESC(stall, "false to prevent bulk stalls");

module_param_named(cdrom, mod_data.cdrom, bool, S_IRUGO);
MODULE_PARM_DESC(cdrom, "true to emulate cdrom instead of disk");


/* In the non-TEST version, only the module parameters listed above
 * are available. */
#ifdef CONFIG_USB_FILE_STORAGE_TEST

module_param_named(transport, mod_data.transport_parm, charp, S_IRUGO);
MODULE_PARM_DESC(transport, "type of transport (BBB, CBI, or CB)");

module_param_named(protocol, mod_data.protocol_parm, charp, S_IRUGO);
MODULE_PARM_DESC(protocol, "type of protocol (RBC, 8020, QIC, UFI, "
		"8070, or SCSI)");

module_param_named(vendor, mod_data.vendor, ushort, S_IRUGO);
MODULE_PARM_DESC(vendor, "USB Vendor ID");

module_param_named(product, mod_data.product, ushort, S_IRUGO);
MODULE_PARM_DESC(product, "USB Product ID");

module_param_named(release, mod_data.release, ushort, S_IRUGO);
MODULE_PARM_DESC(release, "USB release number");

module_param_named(buflen, mod_data.buflen, uint, S_IRUGO);
MODULE_PARM_DESC(buflen, "I/O buffer size");

#endif /* CONFIG_USB_FILE_STORAGE_TEST */


/*-------------------------------------------------------------------------*/

/* SCSI device types */
#define TYPE_DISK	0x00
#define TYPE_CDROM	0x05

/* USB protocol value = the transport method */
#define USB_PR_CBI	0x00		// Control/Bulk/Interrupt
#define USB_PR_CB	0x01		// Control/Bulk w/o interrupt
#define USB_PR_BULK	0x50		// Bulk-only

/* USB subclass value = the protocol encapsulation */
#define USB_SC_RBC	0x01		// Reduced Block Commands (flash)
#define USB_SC_8020	0x02		// SFF-8020i, MMC-2, ATAPI (CD-ROM)
#define USB_SC_QIC	0x03		// QIC-157 (tape)
#define USB_SC_UFI	0x04		// UFI (floppy)
#define USB_SC_8070	0x05		// SFF-8070i (removable)
#define USB_SC_SCSI	0x06		// Transparent SCSI

/* Bulk-only data structures */

/* Command Block Wrapper */
struct bulk_cb_wrap {
	__le32	Signature;		// Contains 'USBC'
	u32	Tag;			// Unique per command id
	__le32	DataTransferLength;	// Size of the data
	u8	Flags;			// Direction in bit 7
	u8	Lun;			// LUN (normally 0)
	u8	Length;			// Of the CDB, <= MAX_COMMAND_SIZE
	u8	CDB[16];		// Command Data Block
};

#define USB_BULK_CB_WRAP_LEN	31
#define USB_BULK_CB_SIG		0x43425355	// Spells out USBC
#define USB_BULK_IN_FLAG	0x80

/* Command Status Wrapper */
struct bulk_cs_wrap {
	__le32	Signature;		// Should = 'USBS'
	u32	Tag;			// Same as original command
	__le32	Residue;		// Amount not transferred
	u8	Status;			// See below
};

#define USB_BULK_CS_WRAP_LEN	13
#define USB_BULK_CS_SIG		0x53425355	// Spells out 'USBS'
#define USB_STATUS_PASS		0
#define USB_STATUS_FAIL		1
#define USB_STATUS_PHASE_ERROR	2

/* Bulk-only class specific requests */
#define USB_BULK_RESET_REQUEST		0xff
#define USB_BULK_GET_MAX_LUN_REQUEST	0xfe


/* CBI Interrupt data structure */
struct interrupt_data {
	u8	bType;
	u8	bValue;
};

#define CBI_INTERRUPT_DATA_LEN		2

/* CBI Accept Device-Specific Command request */
#define USB_CBI_ADSC_REQUEST		0x00


#define MAX_COMMAND_SIZE	16	// Length of a SCSI Command Data Block

/* SCSI commands that we recognize */
#define SC_FORMAT_UNIT			0x04
#define SC_INQUIRY			0x12
#define SC_MODE_SELECT_6		0x15
#define SC_MODE_SELECT_10		0x55
#define SC_MODE_SENSE_6			0x1a
#define SC_MODE_SENSE_10		0x5a
#define SC_PREVENT_ALLOW_MEDIUM_REMOVAL	0x1e
#define SC_READ_6			0x08
#define SC_READ_10			0x28
#define SC_READ_12			0xa8
#define SC_READ_CAPACITY		0x25
#define SC_READ_FORMAT_CAPACITIES	0x23
#define SC_READ_HEADER			0x44
#define SC_READ_TOC			0x43
#define SC_RELEASE			0x17
#define SC_REQUEST_SENSE		0x03
#define SC_RESERVE			0x16
#define SC_SEND_DIAGNOSTIC		0x1d
#define SC_START_STOP_UNIT		0x1b
#define SC_SYNCHRONIZE_CACHE		0x35
#define SC_TEST_UNIT_READY		0x00
#define SC_VERIFY			0x2f
#define SC_WRITE_6			0x0a
#define SC_WRITE_10			0x2a
#define SC_WRITE_12			0xaa

/* SCSI Sense Key/Additional Sense Code/ASC Qualifier values */
#define SS_NO_SENSE				0
#define SS_COMMUNICATION_FAILURE		0x040800
#define SS_INVALID_COMMAND			0x052000
#define SS_INVALID_FIELD_IN_CDB			0x052400
#define SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE	0x052100
#define SS_LOGICAL_UNIT_NOT_SUPPORTED		0x052500
#define SS_MEDIUM_NOT_PRESENT			0x023a00
#define SS_MEDIUM_REMOVAL_PREVENTED		0x055302
#define SS_NOT_READY_TO_READY_TRANSITION	0x062800
#define SS_RESET_OCCURRED			0x062900
#define SS_SAVING_PARAMETERS_NOT_SUPPORTED	0x053900
#define SS_UNRECOVERED_READ_ERROR		0x031100
#define SS_WRITE_ERROR				0x030c02
#define SS_WRITE_PROTECTED			0x072700

#define SK(x)		((u8) ((x) >> 16))	// Sense Key byte, etc.
#define ASC(x)		((u8) ((x) >> 8))
#define ASCQ(x)		((u8) (x))


/*-------------------------------------------------------------------------*/

/*
 * These definitions will permit the compiler to avoid generating code for
 * parts of the driver that aren't used in the non-TEST version.  Even gcc
 * can recognize when a test of a constant expression yields a dead code
 * path.
 */

#ifdef CONFIG_USB_FILE_STORAGE_TEST

#define transport_is_bbb()	(mod_data.transport_type == USB_PR_BULK)
#define transport_is_cbi()	(mod_data.transport_type == USB_PR_CBI)
#define protocol_is_scsi()	(mod_data.protocol_type == USB_SC_SCSI)

#else

#define transport_is_bbb()	1
#define transport_is_cbi()	0
#define protocol_is_scsi()	1

#endif /* CONFIG_USB_FILE_STORAGE_TEST */


struct lun {
	struct file	*filp;
	loff_t		file_length;
	loff_t		num_sectors;

	unsigned int	ro : 1;
	unsigned int	prevent_medium_removal : 1;
	unsigned int	registered : 1;
	unsigned int	info_valid : 1;

	u32		sense_data;
	u32		sense_data_info;
	u32		unit_attention_data;

	struct device	dev;
};

#define backing_file_is_open(curlun)	((curlun)->filp != NULL)

static struct lun *dev_to_lun(struct device *dev)
{
	return container_of(dev, struct lun, dev);
}


/* Big enough to hold our biggest descriptor */
#define EP0_BUFSIZE	256
#define DELAYED_STATUS	(EP0_BUFSIZE + 999)	// An impossibly large value

/* Number of buffers we will use.  2 is enough for double-buffering */
#define NUM_BUFFERS	2

enum fsg_buffer_state {
	BUF_STATE_EMPTY = 0,
	BUF_STATE_FULL,
	BUF_STATE_BUSY
};

struct fsg_buffhd {
	void				*buf;
	enum fsg_buffer_state		state;
	struct fsg_buffhd		*next;

	/* The NetChip 2280 is faster, and handles some protocol faults
	 * better, if we don't submit any short bulk-out read requests.
	 * So we will record the intended request length here. */
	unsigned int			bulk_out_intended_length;

	struct usb_request		*inreq;
	int				inreq_busy;
	struct usb_request		*outreq;
	int				outreq_busy;
};

enum fsg_state {
	FSG_STATE_COMMAND_PHASE = -10,		// This one isn't used anywhere
	FSG_STATE_DATA_PHASE,
	FSG_STATE_STATUS_PHASE,

	FSG_STATE_IDLE = 0,
	FSG_STATE_ABORT_BULK_OUT,
	FSG_STATE_RESET,
	FSG_STATE_INTERFACE_CHANGE,
	FSG_STATE_CONFIG_CHANGE,
	FSG_STATE_DISCONNECT,
	FSG_STATE_EXIT,
	FSG_STATE_TERMINATED
};

enum data_direction {
	DATA_DIR_UNKNOWN = 0,
	DATA_DIR_FROM_HOST,
	DATA_DIR_TO_HOST,
	DATA_DIR_NONE
};

struct fsg_dev {
	/* lock protects: state, all the req_busy's, and cbbuf_cmnd */
	spinlock_t		lock;
	struct usb_gadget	*gadget;

	/* filesem protects: backing files in use */
	struct rw_semaphore	filesem;

	/* reference counting: wait until all LUNs are released */
	struct kref		ref;

	struct usb_ep		*ep0;		// Handy copy of gadget->ep0
	struct usb_request	*ep0req;	// For control responses
	unsigned int		ep0_req_tag;
	const char		*ep0req_name;

	struct usb_request	*intreq;	// For interrupt responses
	int			intreq_busy;
	struct fsg_buffhd	*intr_buffhd;

 	unsigned int		bulk_out_maxpacket;
	enum fsg_state		state;		// For exception handling
	unsigned int		exception_req_tag;

	u8			config, new_config;

	unsigned int		running : 1;
	unsigned int		bulk_in_enabled : 1;
	unsigned int		bulk_out_enabled : 1;
	unsigned int		intr_in_enabled : 1;
	unsigned int		phase_error : 1;
	unsigned int		short_packet_received : 1;
	unsigned int		bad_lun_okay : 1;

	unsigned long		atomic_bitflags;
#define REGISTERED		0
#define IGNORE_BULK_OUT		1
#define SUSPENDED		2

	struct usb_ep		*bulk_in;
	struct usb_ep		*bulk_out;
	struct usb_ep		*intr_in;

	struct fsg_buffhd	*next_buffhd_to_fill;
	struct fsg_buffhd	*next_buffhd_to_drain;
	struct fsg_buffhd	buffhds[NUM_BUFFERS];

	int			thread_wakeup_needed;
	struct completion	thread_notifier;
	struct task_struct	*thread_task;

	int			cmnd_size;
	u8			cmnd[MAX_COMMAND_SIZE];
	enum data_direction	data_dir;
	u32			data_size;
	u32			data_size_from_cmnd;
	u32			tag;
	unsigned int		lun;
	u32			residue;
	u32			usb_amount_left;

	/* The CB protocol offers no way for a host to know when a command
	 * has completed.  As a result the next command may arrive early,
	 * and we will still have to handle it.  For that reason we need
	 * a buffer to store new commands when using CB (or CBI, which
	 * does not oblige a host to wait for command completion either). */
	int			cbbuf_cmnd_size;
	u8			cbbuf_cmnd[MAX_COMMAND_SIZE];

	unsigned int		nluns;
	struct lun		*luns;
	struct lun		*curlun;
};

typedef void (*fsg_routine_t)(struct fsg_dev *);

static int exception_in_progress(struct fsg_dev *fsg)
{
	return (fsg->state > FSG_STATE_IDLE);
}

/* Make bulk-out requests be divisible by the maxpacket size */
static void set_bulk_out_req_length(struct fsg_dev *fsg,
		struct fsg_buffhd *bh, unsigned int length)
{
	unsigned int	rem;

	bh->bulk_out_intended_length = length;
	rem = length % fsg->bulk_out_maxpacket;
	if (rem > 0)
		length += fsg->bulk_out_maxpacket - rem;
	bh->outreq->length = length;
}

static struct fsg_dev			*the_fsg;
static struct usb_gadget_driver		fsg_driver;

static void	close_backing_file(struct lun *curlun);


/*-------------------------------------------------------------------------*/

#ifdef DUMP_MSGS

static void dump_msg(struct fsg_dev *fsg, const char *label,
		const u8 *buf, unsigned int length)
{
	if (length < 512) {
		DBG(fsg, "%s, length %u:\n", label, length);
		print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_OFFSET,
				16, 1, buf, length, 0);
	}
}

static void dump_cdb(struct fsg_dev *fsg)
{}

#else

static void dump_msg(struct fsg_dev *fsg, const char *label,
		const u8 *buf, unsigned int length)
{}

#ifdef VERBOSE_DEBUG

static void dump_cdb(struct fsg_dev *fsg)
{
	print_hex_dump(KERN_DEBUG, "SCSI CDB: ", DUMP_PREFIX_NONE,
			16, 1, fsg->cmnd, fsg->cmnd_size, 0);
}

#else

static void dump_cdb(struct fsg_dev *fsg)
{}

#endif /* VERBOSE_DEBUG */
#endif /* DUMP_MSGS */


static int fsg_set_halt(struct fsg_dev *fsg, struct usb_ep *ep)
{
	const char	*name;

	if (ep == fsg->bulk_in)
		name = "bulk-in";
	else if (ep == fsg->bulk_out)
		name = "bulk-out";
	else
		name = ep->name;
	DBG(fsg, "%s set halt\n", name);
	return usb_ep_set_halt(ep);
}


/*-------------------------------------------------------------------------*/

/* Routines for unaligned data access */

static u32 get_unaligned_be24(u8 *buf)
{
	return 0xffffff & (u32) get_unaligned_be32(buf - 1);
}


/*-------------------------------------------------------------------------*/

/*
 * DESCRIPTORS ... most are static, but strings and (full) configuration
 * descriptors are built on demand.  Also the (static) config and interface
 * descriptors are adjusted during fsg_bind().
 */
#define STRING_MANUFACTURER	1
#define STRING_PRODUCT		2
#define STRING_SERIAL		3
#define STRING_CONFIG		4
#define STRING_INTERFACE	5

/* There is only one configuration. */
#define	CONFIG_VALUE		1

static struct usb_device_descriptor
device_desc = {
	.bLength =		sizeof device_desc,
	.bDescriptorType =	USB_DT_DEVICE,

	.bcdUSB =		cpu_to_le16(0x0200),
	.bDeviceClass =		USB_CLASS_PER_INTERFACE,

	/* The next three values can be overridden by module parameters */
	.idVendor =		cpu_to_le16(DRIVER_VENDOR_ID),
	.idProduct =		cpu_to_le16(DRIVER_PRODUCT_ID),
	.bcdDevice =		cpu_to_le16(0xffff),

	.iManufacturer =	STRING_MANUFACTURER,
	.iProduct =		STRING_PRODUCT,
	.iSerialNumber =	STRING_SERIAL,
	.bNumConfigurations =	1,
};

static struct usb_config_descriptor
config_desc = {
	.bLength =		sizeof config_desc,
	.bDescriptorType =	USB_DT_CONFIG,

	/* wTotalLength computed by usb_gadget_config_buf() */
	.bNumInterfaces =	1,
	.bConfigurationValue =	CONFIG_VALUE,
	.iConfiguration =	STRING_CONFIG,
	.bmAttributes =		USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
	.bMaxPower =		CONFIG_USB_GADGET_VBUS_DRAW / 2,
};

static struct usb_otg_descriptor
otg_desc = {
	.bLength =		sizeof(otg_desc),
	.bDescriptorType =	USB_DT_OTG,

	.bmAttributes =		USB_OTG_SRP,
};

/* There is only one interface. */

static struct usb_interface_descriptor
intf_desc = {
	.bLength =		sizeof intf_desc,
	.bDescriptorType =	USB_DT_INTERFACE,

	.bNumEndpoints =	2,		// Adjusted during fsg_bind()
	.bInterfaceClass =	USB_CLASS_MASS_STORAGE,
	.bInterfaceSubClass =	USB_SC_SCSI,	// Adjusted during fsg_bind()
	.bInterfaceProtocol =	USB_PR_BULK,	// Adjusted during fsg_bind()
	.iInterface =		STRING_INTERFACE,
};

/* Three full-speed endpoint descriptors: bulk-in, bulk-out,
 * and interrupt-in. */

static struct usb_endpoint_descriptor
fs_bulk_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	/* wMaxPacketSize set by autoconfiguration */
};

static struct usb_endpoint_descriptor
fs_bulk_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	/* wMaxPacketSize set by autoconfiguration */
};

static struct usb_endpoint_descriptor
fs_intr_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(2),
	.bInterval =		32,	// frames -> 32 ms
};

static const struct usb_descriptor_header *fs_function[] = {
	(struct usb_descriptor_header *) &otg_desc,
	(struct usb_descriptor_header *) &intf_desc,
	(struct usb_descriptor_header *) &fs_bulk_in_desc,
	(struct usb_descriptor_header *) &fs_bulk_out_desc,
	(struct usb_descriptor_header *) &fs_intr_in_desc,
	NULL,
};
#define FS_FUNCTION_PRE_EP_ENTRIES	2


/*
 * USB 2.0 devices need to expose both high speed and full speed
 * descriptors, unless they only run at full speed.
 *
 * That means alternate endpoint descriptors (bigger packets)
 * and a "device qualifier" ... plus more construction options
 * for the config descriptor.
 */
static struct usb_qualifier_descriptor
dev_qualifier = {
	.bLength =		sizeof dev_qualifier,
	.bDescriptorType =	USB_DT_DEVICE_QUALIFIER,

	.bcdUSB =		cpu_to_le16(0x0200),
	.bDeviceClass =		USB_CLASS_PER_INTERFACE,

	.bNumConfigurations =	1,
};

static struct usb_endpoint_descriptor
hs_bulk_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	/* bEndpointAddress copied from fs_bulk_in_desc during fsg_bind() */
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_endpoint_descriptor
hs_bulk_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	/* bEndpointAddress copied from fs_bulk_out_desc during fsg_bind() */
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
	.bInterval =		1,	// NAK every 1 uframe
};

static struct usb_endpoint_descriptor
hs_intr_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	/* bEndpointAddress copied from fs_intr_in_desc during fsg_bind() */
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(2),
	.bInterval =		9,	// 2**(9-1) = 256 uframes -> 32 ms
};

static const struct usb_descriptor_header *hs_function[] = {
	(struct usb_descriptor_header *) &otg_desc,
	(struct usb_descriptor_header *) &intf_desc,
	(struct usb_descriptor_header *) &hs_bulk_in_desc,
	(struct usb_descriptor_header *) &hs_bulk_out_desc,
	(struct usb_descriptor_header *) &hs_intr_in_desc,
	NULL,
};
#define HS_FUNCTION_PRE_EP_ENTRIES	2

/* Maxpacket and other transfer characteristics vary by speed. */
static struct usb_endpoint_descriptor *
ep_desc(struct usb_gadget *g, struct usb_endpoint_descriptor *fs,
		struct usb_endpoint_descriptor *hs)
{
	if (gadget_is_dualspeed(g) && g->speed == USB_SPEED_HIGH)
		return hs;
	return fs;
}


/* The CBI specification limits the serial string to 12 uppercase hexadecimal
 * characters. */
static char				manufacturer[64];
static char				serial[13];

/* Static strings, in UTF-8 (for simplicity we use only ASCII characters) */
static struct usb_string		strings[] = {
	{STRING_MANUFACTURER,	manufacturer},
	{STRING_PRODUCT,	longname},
	{STRING_SERIAL,		serial},
	{STRING_CONFIG,		"Self-powered"},
	{STRING_INTERFACE,	"Mass Storage"},
	{}
};

static struct usb_gadget_strings	stringtab = {
	.language	= 0x0409,		// en-us
	.strings	= strings,
};


/*
 * Config descriptors must agree with the code that sets configurations
 * and with code managing interfaces and their altsettings.  They must
 * also handle different speeds and other-speed requests.
 */
static int populate_config_buf(struct usb_gadget *gadget,
		u8 *buf, u8 type, unsigned index)
{
	enum usb_device_speed			speed = gadget->speed;
	int					len;
	const struct usb_descriptor_header	**function;

	if (index > 0)
		return -EINVAL;

	if (gadget_is_dualspeed(gadget) && type == USB_DT_OTHER_SPEED_CONFIG)
		speed = (USB_SPEED_FULL + USB_SPEED_HIGH) - speed;
	if (gadget_is_dualspeed(gadget) && speed == USB_SPEED_HIGH)
		function = hs_function;
	else
		function = fs_function;

	/* for now, don't advertise srp-only devices */
	if (!gadget_is_otg(gadget))
		function++;

	len = usb_gadget_config_buf(&config_desc, buf, EP0_BUFSIZE, function);
	((struct usb_config_descriptor *) buf)->bDescriptorType = type;
	return len;
}


/*-------------------------------------------------------------------------*/

/* These routines may be called in process context or in_irq */

/* Caller must hold fsg->lock */
static void wakeup_thread(struct fsg_dev *fsg)
{
	/* Tell the main thread that something has happened */
	fsg->thread_wakeup_needed = 1;
	if (fsg->thread_task)
		wake_up_process(fsg->thread_task);
}


static void raise_exception(struct fsg_dev *fsg, enum fsg_state new_state)
{
	unsigned long		flags;

	/* Do nothing if a higher-priority exception is already in progress.
	 * If a lower-or-equal priority exception is in progress, preempt it
	 * and notify the main thread by sending it a signal. */
	spin_lock_irqsave(&fsg->lock, flags);
	if (fsg->state <= new_state) {
		fsg->exception_req_tag = fsg->ep0_req_tag;
		fsg->state = new_state;
		if (fsg->thread_task)
			send_sig_info(SIGUSR1, SEND_SIG_FORCED,
					fsg->thread_task);
	}
	spin_unlock_irqrestore(&fsg->lock, flags);
}


/*-------------------------------------------------------------------------*/

/* The disconnect callback and ep0 routines.  These always run in_irq,
 * except that ep0_queue() is called in the main thread to acknowledge
 * completion of various requests: set config, set interface, and
 * Bulk-only device reset. */

static void fsg_disconnect(struct usb_gadget *gadget)
{
	struct fsg_dev		*fsg = get_gadget_data(gadget);

	DBG(fsg, "disconnect or port reset\n");
	raise_exception(fsg, FSG_STATE_DISCONNECT);
}


static int ep0_queue(struct fsg_dev *fsg)
{
	int	rc;

	rc = usb_ep_queue(fsg->ep0, fsg->ep0req, GFP_ATOMIC);
	if (rc != 0 && rc != -ESHUTDOWN) {

		/* We can't do much more than wait for a reset */
		WARNING(fsg, "error in submission: %s --> %d\n",
				fsg->ep0->name, rc);
	}
	return rc;
}

static void ep0_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct fsg_dev		*fsg = ep->driver_data;

	if (req->actual > 0)
		dump_msg(fsg, fsg->ep0req_name, req->buf, req->actual);
	if (req->status || req->actual != req->length)
		DBG(fsg, "%s --> %d, %u/%u\n", __func__,
				req->status, req->actual, req->length);
	if (req->status == -ECONNRESET)		// Request was cancelled
		usb_ep_fifo_flush(ep);

	if (req->status == 0 && req->context)
		((fsg_routine_t) (req->context))(fsg);
}


/*-------------------------------------------------------------------------*/

/* Bulk and interrupt endpoint completion handlers.
 * These always run in_irq. */

static void bulk_in_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct fsg_dev		*fsg = ep->driver_data;
	struct fsg_buffhd	*bh = req->context;

	if (req->status || req->actual != req->length)
		DBG(fsg, "%s --> %d, %u/%u\n", __func__,
				req->status, req->actual, req->length);
	if (req->status == -ECONNRESET)		// Request was cancelled
		usb_ep_fifo_flush(ep);

	/* Hold the lock while we update the request and buffer states */
	smp_wmb();
	spin_lock(&fsg->lock);
	bh->inreq_busy = 0;
	bh->state = BUF_STATE_EMPTY;
	wakeup_thread(fsg);
	spin_unlock(&fsg->lock);
}

static void bulk_out_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct fsg_dev		*fsg = ep->driver_data;
	struct fsg_buffhd	*bh = req->context;

	dump_msg(fsg, "bulk-out", req->buf, req->actual);
	if (req->status || req->actual != bh->bulk_out_intended_length)
		DBG(fsg, "%s --> %d, %u/%u\n", __func__,
				req->status, req->actual,
				bh->bulk_out_intended_length);
	if (req->status == -ECONNRESET)		// Request was cancelled
		usb_ep_fifo_flush(ep);

	/* Hold the lock while we update the request and buffer states */
	smp_wmb();
	spin_lock(&fsg->lock);
	bh->outreq_busy = 0;
	bh->state = BUF_STATE_FULL;
	wakeup_thread(fsg);
	spin_unlock(&fsg->lock);
}


#ifdef CONFIG_USB_FILE_STORAGE_TEST
static void intr_in_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct fsg_dev		*fsg = ep->driver_data;
	struct fsg_buffhd	*bh = req->context;

	if (req->status || req->actual != req->length)
		DBG(fsg, "%s --> %d, %u/%u\n", __func__,
				req->status, req->actual, req->length);
	if (req->status == -ECONNRESET)		// Request was cancelled
		usb_ep_fifo_flush(ep);

	/* Hold the lock while we update the request and buffer states */
	smp_wmb();
	spin_lock(&fsg->lock);
	fsg->intreq_busy = 0;
	bh->state = BUF_STATE_EMPTY;
	wakeup_thread(fsg);
	spin_unlock(&fsg->lock);
}

#else
static void intr_in_complete(struct usb_ep *ep, struct usb_request *req)
{}
#endif /* CONFIG_USB_FILE_STORAGE_TEST */


/*-------------------------------------------------------------------------*/

/* Ep0 class-specific handlers.  These always run in_irq. */

#ifdef CONFIG_USB_FILE_STORAGE_TEST
static void received_cbi_adsc(struct fsg_dev *fsg, struct fsg_buffhd *bh)
{
	struct usb_request	*req = fsg->ep0req;
	static u8		cbi_reset_cmnd[6] = {
			SC_SEND_DIAGNOSTIC, 4, 0xff, 0xff, 0xff, 0xff};

	/* Error in command transfer? */
	if (req->status || req->length != req->actual ||
			req->actual < 6 || req->actual > MAX_COMMAND_SIZE) {

		/* Not all controllers allow a protocol stall after
		 * receiving control-out data, but we'll try anyway. */
		fsg_set_halt(fsg, fsg->ep0);
		return;			// Wait for reset
	}

	/* Is it the special reset command? */
	if (req->actual >= sizeof cbi_reset_cmnd &&
			memcmp(req->buf, cbi_reset_cmnd,
				sizeof cbi_reset_cmnd) == 0) {

		/* Raise an exception to stop the current operation
		 * and reinitialize our state. */
		DBG(fsg, "cbi reset request\n");
		raise_exception(fsg, FSG_STATE_RESET);
		return;
	}

	VDBG(fsg, "CB[I] accept device-specific command\n");
	spin_lock(&fsg->lock);

	/* Save the command for later */
	if (fsg->cbbuf_cmnd_size)
		WARNING(fsg, "CB[I] overwriting previous command\n");
	fsg->cbbuf_cmnd_size = req->actual;
	memcpy(fsg->cbbuf_cmnd, req->buf, fsg->cbbuf_cmnd_size);

	wakeup_thread(fsg);
	spin_unlock(&fsg->lock);
}

#else
static void received_cbi_adsc(struct fsg_dev *fsg, struct fsg_buffhd *bh)
{}
#endif /* CONFIG_USB_FILE_STORAGE_TEST */


static int class_setup_req(struct fsg_dev *fsg,
		const struct usb_ctrlrequest *ctrl)
{
	struct usb_request	*req = fsg->ep0req;
	int			value = -EOPNOTSUPP;
	u16			w_index = le16_to_cpu(ctrl->wIndex);
	u16                     w_value = le16_to_cpu(ctrl->wValue);
	u16			w_length = le16_to_cpu(ctrl->wLength);

	if (!fsg->config)
		return value;

	/* Handle Bulk-only class-specific requests */
	if (transport_is_bbb()) {
		switch (ctrl->bRequest) {

		case USB_BULK_RESET_REQUEST:
			if (ctrl->bRequestType != (USB_DIR_OUT |
					USB_TYPE_CLASS | USB_RECIP_INTERFACE))
				break;
			if (w_index != 0 || w_value != 0) {
				value = -EDOM;
				break;
			}

			/* Raise an exception to stop the current operation
			 * and reinitialize our state. */
			DBG(fsg, "bulk reset request\n");
			raise_exception(fsg, FSG_STATE_RESET);
			value = DELAYED_STATUS;
			break;

		case USB_BULK_GET_MAX_LUN_REQUEST:
			if (ctrl->bRequestType != (USB_DIR_IN |
					USB_TYPE_CLASS | USB_RECIP_INTERFACE))
				break;
			if (w_index != 0 || w_value != 0) {
				value = -EDOM;
				break;
			}
			VDBG(fsg, "get max LUN\n");
			*(u8 *) req->buf = fsg->nluns - 1;
			value = 1;
			break;
		}
	}

	/* Handle CBI class-specific requests */
	else {
		switch (ctrl->bRequest) {

		case USB_CBI_ADSC_REQUEST:
			if (ctrl->bRequestType != (USB_DIR_OUT |
					USB_TYPE_CLASS | USB_RECIP_INTERFACE))
				break;
			if (w_index != 0 || w_value != 0) {
				value = -EDOM;
				break;
			}
			if (w_length > MAX_COMMAND_SIZE) {
				value = -EOVERFLOW;
				break;
			}
			value = w_length;
			fsg->ep0req->context = received_cbi_adsc;
			break;
		}
	}

	if (value == -EOPNOTSUPP)
		VDBG(fsg,
			"unknown class-specific control req "
			"%02x.%02x v%04x i%04x l%u\n",
			ctrl->bRequestType, ctrl->bRequest,
			le16_to_cpu(ctrl->wValue), w_index, w_length);
	return value;
}


/*-------------------------------------------------------------------------*/

/* Ep0 standard request handlers.  These always run in_irq. */

static int standard_setup_req(struct fsg_dev *fsg,
		const struct usb_ctrlrequest *ctrl)
{
	struct usb_request	*req = fsg->ep0req;
	int			value = -EOPNOTSUPP;
	u16			w_index = le16_to_cpu(ctrl->wIndex);
	u16			w_value = le16_to_cpu(ctrl->wValue);

	/* Usually this just stores reply data in the pre-allocated ep0 buffer,
	 * but config change events will also reconfigure hardware. */
	switch (ctrl->bRequest) {

	case USB_REQ_GET_DESCRIPTOR:
		if (ctrl->bRequestType != (USB_DIR_IN | USB_TYPE_STANDARD |
				USB_RECIP_DEVICE))
			break;
		switch (w_value >> 8) {

		case USB_DT_DEVICE:
			VDBG(fsg, "get device descriptor\n");
			value = sizeof device_desc;
			memcpy(req->buf, &device_desc, value);
			break;
		case USB_DT_DEVICE_QUALIFIER:
			VDBG(fsg, "get device qualifier\n");
			if (!gadget_is_dualspeed(fsg->gadget))
				break;
			value = sizeof dev_qualifier;
			memcpy(req->buf, &dev_qualifier, value);
			break;

		case USB_DT_OTHER_SPEED_CONFIG:
			VDBG(fsg, "get other-speed config descriptor\n");
			if (!gadget_is_dualspeed(fsg->gadget))
				break;
			goto get_config;
		case USB_DT_CONFIG:
			VDBG(fsg, "get configuration descriptor\n");
get_config:
			value = populate_config_buf(fsg->gadget,
					req->buf,
					w_value >> 8,
					w_value & 0xff);
			break;

		case USB_DT_STRING:
			VDBG(fsg, "get string descriptor\n");

			/* wIndex == language code */
			value = usb_gadget_get_string(&stringtab,
					w_value & 0xff, req->buf);
			break;
		}
		break;

	/* One config, two speeds */
	case USB_REQ_SET_CONFIGURATION:
		if (ctrl->bRequestType != (USB_DIR_OUT | USB_TYPE_STANDARD |
				USB_RECIP_DEVICE))
			break;
		VDBG(fsg, "set configuration\n");
		if (w_value == CONFIG_VALUE || w_value == 0) {
			fsg->new_config = w_value;

			/* Raise an exception to wipe out previous transaction
			 * state (queued bufs, etc) and set the new config. */
			raise_exception(fsg, FSG_STATE_CONFIG_CHANGE);
			value = DELAYED_STATUS;
		}
		break;
	case USB_REQ_GET_CONFIGURATION:
		if (ctrl->bRequestType != (USB_DIR_IN | USB_TYPE_STANDARD |
				USB_RECIP_DEVICE))
			break;
		VDBG(fsg, "get configuration\n");
		*(u8 *) req->buf = fsg->config;
		value = 1;
		break;

	case USB_REQ_SET_INTERFACE:
		if (ctrl->bRequestType != (USB_DIR_OUT| USB_TYPE_STANDARD |
				USB_RECIP_INTERFACE))
			break;
		if (fsg->config && w_index == 0) {

			/* Raise an exception to wipe out previous transaction
			 * state (queued bufs, etc) and install the new
			 * interface altsetting. */
			raise_exception(fsg, FSG_STATE_INTERFACE_CHANGE);
			value = DELAYED_STATUS;
		}
		break;
	case USB_REQ_GET_INTERFACE:
		if (ctrl->bRequestType != (USB_DIR_IN | USB_TYPE_STANDARD |
				USB_RECIP_INTERFACE))
			break;
		if (!fsg->config)
			break;
		if (w_index != 0) {
			value = -EDOM;
			break;
		}
		VDBG(fsg, "get interface\n");
		*(u8 *) req->buf = 0;
		value = 1;
		break;

	default:
		VDBG(fsg,
			"unknown control req %02x.%02x v%04x i%04x l%u\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, le16_to_cpu(ctrl->wLength));
	}

	return value;
}


static int fsg_setup(struct usb_gadget *gadget,
		const struct usb_ctrlrequest *ctrl)
{
	struct fsg_dev		*fsg = get_gadget_data(gadget);
	int			rc;
	int			w_length = le16_to_cpu(ctrl->wLength);

	++fsg->ep0_req_tag;		// Record arrival of a new request
	fsg->ep0req->context = NULL;
	fsg->ep0req->length = 0;
	dump_msg(fsg, "ep0-setup", (u8 *) ctrl, sizeof(*ctrl));

	if ((ctrl->bRequestType & USB_TYPE_MASK) == USB_TYPE_CLASS)
		rc = class_setup_req(fsg, ctrl);
	else
		rc = standard_setup_req(fsg, ctrl);

	/* Respond with data/status or defer until later? */
	if (rc >= 0 && rc != DELAYED_STATUS) {
		rc = min(rc, w_length);
		fsg->ep0req->length = rc;
		fsg->ep0req->zero = rc < w_length;
		fsg->ep0req_name = (ctrl->bRequestType & USB_DIR_IN ?
				"ep0-in" : "ep0-out");
		rc = ep0_queue(fsg);
	}

	/* Device either stalls (rc < 0) or reports success */
	return rc;
}


/*-------------------------------------------------------------------------*/

/* All the following routines run in process context */


/* Use this for bulk or interrupt transfers, not ep0 */
static void start_transfer(struct fsg_dev *fsg, struct usb_ep *ep,
		struct usb_request *req, int *pbusy,
		enum fsg_buffer_state *state)
{
	int	rc;

	if (ep == fsg->bulk_in)
		dump_msg(fsg, "bulk-in", req->buf, req->length);
	else if (ep == fsg->intr_in)
		dump_msg(fsg, "intr-in", req->buf, req->length);

	spin_lock_irq(&fsg->lock);
	*pbusy = 1;
	*state = BUF_STATE_BUSY;
	spin_unlock_irq(&fsg->lock);
	rc = usb_ep_queue(ep, req, GFP_KERNEL);
	if (rc != 0) {
		*pbusy = 0;
		*state = BUF_STATE_EMPTY;

		/* We can't do much more than wait for a reset */

		/* Note: currently the net2280 driver fails zero-length
		 * submissions if DMA is enabled. */
		if (rc != -ESHUTDOWN && !(rc == -EOPNOTSUPP &&
						req->length == 0))
			WARNING(fsg, "error in submission: %s --> %d\n",
					ep->name, rc);
	}
}


static int sleep_thread(struct fsg_dev *fsg)
{
	int	rc = 0;

	/* Wait until a signal arrives or we are woken up */
	for (;;) {
		try_to_freeze();
		set_current_state(TASK_INTERRUPTIBLE);
		if (signal_pending(current)) {
			rc = -EINTR;
			break;
		}
		if (fsg->thread_wakeup_needed)
			break;
		schedule();
	}
	__set_current_state(TASK_RUNNING);
	fsg->thread_wakeup_needed = 0;
	return rc;
}


/*-------------------------------------------------------------------------*/

static int do_read(struct fsg_dev *fsg)
{
	struct lun		*curlun = fsg->curlun;
	u32			lba;
	struct fsg_buffhd	*bh;
	int			rc;
	u32			amount_left;
	loff_t			file_offset, file_offset_tmp;
	unsigned int		amount;
	unsigned int		partial_page;
	ssize_t			nread;

	/* Get the starting Logical Block Address and check that it's
	 * not too big */
	if (fsg->cmnd[0] == SC_READ_6)
		lba = get_unaligned_be24(&fsg->cmnd[1]);
	else {
		lba = get_unaligned_be32(&fsg->cmnd[2]);

		/* We allow DPO (Disable Page Out = don't save data in the
		 * cache) and FUA (Force Unit Access = don't read from the
		 * cache), but we don't implement them. */
		if ((fsg->cmnd[1] & ~0x18) != 0) {
			curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
			return -EINVAL;
		}
	}
	if (lba >= curlun->num_sectors) {
		curlun->sense_data = SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
		return -EINVAL;
	}
	file_offset = ((loff_t) lba) << 9;

	/* Carry out the file reads */
	amount_left = fsg->data_size_from_cmnd;
	if (unlikely(amount_left == 0))
		return -EIO;		// No default reply

	for (;;) {

		/* Figure out how much we need to read:
		 * Try to read the remaining amount.
		 * But don't read more than the buffer size.
		 * And don't try to read past the end of the file.
		 * Finally, if we're not at a page boundary, don't read past
		 *	the next page.
		 * If this means reading 0 then we were asked to read past
		 *	the end of file. */
		amount = min((unsigned int) amount_left, mod_data.buflen);
		amount = min((loff_t) amount,
				curlun->file_length - file_offset);
		partial_page = file_offset & (PAGE_CACHE_SIZE - 1);
		if (partial_page > 0)
			amount = min(amount, (unsigned int) PAGE_CACHE_SIZE -
					partial_page);

		/* Wait for the next buffer to become available */
		bh = fsg->next_buffhd_to_fill;
		while (bh->state != BUF_STATE_EMPTY) {
			rc = sleep_thread(fsg);
			if (rc)
				return rc;
		}

		/* If we were asked to read past the end of file,
		 * end with an empty buffer. */
		if (amount == 0) {
			curlun->sense_data =
					SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
			curlun->sense_data_info = file_offset >> 9;
			curlun->info_valid = 1;
			bh->inreq->length = 0;
			bh->state = BUF_STATE_FULL;
			break;
		}

		/* Perform the read */
		file_offset_tmp = file_offset;
		nread = vfs_read(curlun->filp,
				(char __user *) bh->buf,
				amount, &file_offset_tmp);
		VLDBG(curlun, "file read %u @ %llu -> %d\n", amount,
				(unsigned long long) file_offset,
				(int) nread);
		if (signal_pending(current))
			return -EINTR;

		if (nread < 0) {
			LDBG(curlun, "error in file read: %d\n",
					(int) nread);
			nread = 0;
		} else if (nread < amount) {
			LDBG(curlun, "partial file read: %d/%u\n",
					(int) nread, amount);
			nread -= (nread & 511);	// Round down to a block
		}
		file_offset  += nread;
		amount_left  -= nread;
		fsg->residue -= nread;
		bh->inreq->length = nread;
		bh->state = BUF_STATE_FULL;

		/* If an error occurred, report it and its position */
		if (nread < amount) {
			curlun->sense_data = SS_UNRECOVERED_READ_ERROR;
			curlun->sense_data_info = file_offset >> 9;
			curlun->info_valid = 1;
			break;
		}

		if (amount_left == 0)
			break;		// No more left to read

		/* Send this buffer and go read some more */
		bh->inreq->zero = 0;
		start_transfer(fsg, fsg->bulk_in, bh->inreq,
				&bh->inreq_busy, &bh->state);
		fsg->next_buffhd_to_fill = bh->next;
	}

	return -EIO;		// No default reply
}


/*-------------------------------------------------------------------------*/

static int do_write(struct fsg_dev *fsg)
{
	struct lun		*curlun = fsg->curlun;
	u32			lba;
	struct fsg_buffhd	*bh;
	int			get_some_more;
	u32			amount_left_to_req, amount_left_to_write;
	loff_t			usb_offset, file_offset, file_offset_tmp;
	unsigned int		amount;
	unsigned int		partial_page;
	ssize_t			nwritten;
	int			rc;

	if (curlun->ro) {
		curlun->sense_data = SS_WRITE_PROTECTED;
		return -EINVAL;
	}
	spin_lock(&curlun->filp->f_lock);
	curlun->filp->f_flags &= ~O_SYNC;	// Default is not to wait
	spin_unlock(&curlun->filp->f_lock);

	/* Get the starting Logical Block Address and check that it's
	 * not too big */
	if (fsg->cmnd[0] == SC_WRITE_6)
		lba = get_unaligned_be24(&fsg->cmnd[1]);
	else {
		lba = get_unaligned_be32(&fsg->cmnd[2]);

		/* We allow DPO (Disable Page Out = don't save data in the
		 * cache) and FUA (Force Unit Access = write directly to the
		 * medium).  We don't implement DPO; we implement FUA by
		 * performing synchronous output. */
		if ((fsg->cmnd[1] & ~0x18) != 0) {
			curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
			return -EINVAL;
		}
		if (fsg->cmnd[1] & 0x08) {	// FUA
			spin_lock(&curlun->filp->f_lock);
			curlun->filp->f_flags |= O_SYNC;
			spin_unlock(&curlun->filp->f_lock);
		}
	}
	if (lba >= curlun->num_sectors) {
		curlun->sense_data = SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
		return -EINVAL;
	}

	/* Carry out the file writes */
	get_some_more = 1;
	file_offset = usb_offset = ((loff_t) lba) << 9;
	amount_left_to_req = amount_left_to_write = fsg->data_size_from_cmnd;

	while (amount_left_to_write > 0) {

		/* Queue a request for more data from the host */
		bh = fsg->next_buffhd_to_fill;
		if (bh->state == BUF_STATE_EMPTY && get_some_more) {

			/* Figure out how much we want to get:
			 * Try to get the remaining amount.
			 * But don't get more than the buffer size.
			 * And don't try to go past the end of the file.
			 * If we're not at a page boundary,
			 *	don't go past the next page.
			 * If this means getting 0, then we were asked
			 *	to write past the end of file.
			 * Finally, round down to a block boundary. */
			amount = min(amount_left_to_req, mod_data.buflen);
			amount = min((loff_t) amount, curlun->file_length -
					usb_offset);
			partial_page = usb_offset & (PAGE_CACHE_SIZE - 1);
			if (partial_page > 0)
				amount = min(amount,
	(unsigned int) PAGE_CACHE_SIZE - partial_page);

			if (amount == 0) {
				get_some_more = 0;
				curlun->sense_data =
					SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
				curlun->sense_data_info = usb_offset >> 9;
				curlun->info_valid = 1;
				continue;
			}
			amount -= (amount & 511);
			if (amount == 0) {

				/* Why were we were asked to transfer a
				 * partial block? */
				get_some_more = 0;
				continue;
			}

			/* Get the next buffer */
			usb_offset += amount;
			fsg->usb_amount_left -= amount;
			amount_left_to_req -= amount;
			if (amount_left_to_req == 0)
				get_some_more = 0;

			/* amount is always divisible by 512, hence by
			 * the bulk-out maxpacket size */
			bh->outreq->length = bh->bulk_out_intended_length =
					amount;
			bh->outreq->short_not_ok = 1;
			start_transfer(fsg, fsg->bulk_out, bh->outreq,
					&bh->outreq_busy, &bh->state);
			fsg->next_buffhd_to_fill = bh->next;
			continue;
		}

		/* Write the received data to the backing file */
		bh = fsg->next_buffhd_to_drain;
		if (bh->state == BUF_STATE_EMPTY && !get_some_more)
			break;			// We stopped early
		if (bh->state == BUF_STATE_FULL) {
			smp_rmb();
			fsg->next_buffhd_to_drain = bh->next;
			bh->state = BUF_STATE_EMPTY;

			/* Did something go wrong with the transfer? */
			if (bh->outreq->status != 0) {
				curlun->sense_data = SS_COMMUNICATION_FAILURE;
				curlun->sense_data_info = file_offset >> 9;
				curlun->info_valid = 1;
				break;
			}

			amount = bh->outreq->actual;
			if (curlun->file_length - file_offset < amount) {
				LERROR(curlun,
	"write %u @ %llu beyond end %llu\n",
	amount, (unsigned long long) file_offset,
	(unsigned long long) curlun->file_length);
				amount = curlun->file_length - file_offset;
			}

			/* Perform the write */
			file_offset_tmp = file_offset;
			nwritten = vfs_write(curlun->filp,
					(char __user *) bh->buf,
					amount, &file_offset_tmp);
			VLDBG(curlun, "file write %u @ %llu -> %d\n", amount,
					(unsigned long long) file_offset,
					(int) nwritten);
			if (signal_pending(current))
				return -EINTR;		// Interrupted!

			if (nwritten < 0) {
				LDBG(curlun, "error in file write: %d\n",
						(int) nwritten);
				nwritten = 0;
			} else if (nwritten < amount) {
				LDBG(curlun, "partial file write: %d/%u\n",
						(int) nwritten, amount);
				nwritten -= (nwritten & 511);
						// Round down to a block
			}
			file_offset += nwritten;
			amount_left_to_write -= nwritten;
			fsg->residue -= nwritten;

			/* If an error occurred, report it and its position */
			if (nwritten < amount) {
				curlun->sense_data = SS_WRITE_ERROR;
				curlun->sense_data_info = file_offset >> 9;
				curlun->info_valid = 1;
				break;
			}

			/* Did the host decide to stop early? */
			if (bh->outreq->actual != bh->outreq->length) {
				fsg->short_packet_received = 1;
				break;
			}
			continue;
		}

		/* Wait for something to happen */
		rc = sleep_thread(fsg);
		if (rc)
			return rc;
	}

	return -EIO;		// No default reply
}


/*-------------------------------------------------------------------------*/

/* Sync the file data, don't bother with the metadata.
 * This code was copied from fs/buffer.c:sys_fdatasync(). */
static int fsync_sub(struct lun *curlun)
{
	struct file	*filp = curlun->filp;

	if (curlun->ro || !filp)
		return 0;
	return vfs_fsync(filp, filp->f_path.dentry, 1);
}

static void fsync_all(struct fsg_dev *fsg)
{
	int	i;

	for (i = 0; i < fsg->nluns; ++i)
		fsync_sub(&fsg->luns[i]);
}

static int do_synchronize_cache(struct fsg_dev *fsg)
{
	struct lun	*curlun = fsg->curlun;
	int		rc;

	/* We ignore the requested LBA and write out all file's
	 * dirty data buffers. */
	rc = fsync_sub(curlun);
	if (rc)
		curlun->sense_data = SS_WRITE_ERROR;
	return 0;
}


/*-------------------------------------------------------------------------*/

static void invalidate_sub(struct lun *curlun)
{
	struct file	*filp = curlun->filp;
	struct inode	*inode = filp->f_path.dentry->d_inode;
	unsigned long	rc;

	rc = invalidate_mapping_pages(inode->i_mapping, 0, -1);
	VLDBG(curlun, "invalidate_inode_pages -> %ld\n", rc);
}

static int do_verify(struct fsg_dev *fsg)
{
	struct lun		*curlun = fsg->curlun;
	u32			lba;
	u32			verification_length;
	struct fsg_buffhd	*bh = fsg->next_buffhd_to_fill;
	loff_t			file_offset, file_offset_tmp;
	u32			amount_left;
	unsigned int		amount;
	ssize_t			nread;

	/* Get the starting Logical Block Address and check that it's
	 * not too big */
	lba = get_unaligned_be32(&fsg->cmnd[2]);
	if (lba >= curlun->num_sectors) {
		curlun->sense_data = SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
		return -EINVAL;
	}

	/* We allow DPO (Disable Page Out = don't save data in the
	 * cache) but we don't implement it. */
	if ((fsg->cmnd[1] & ~0x10) != 0) {
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}

	verification_length = get_unaligned_be16(&fsg->cmnd[7]);
	if (unlikely(verification_length == 0))
		return -EIO;		// No default reply

	/* Prepare to carry out the file verify */
	amount_left = verification_length << 9;
	file_offset = ((loff_t) lba) << 9;

	/* Write out all the dirty buffers before invalidating them */
	fsync_sub(curlun);
	if (signal_pending(current))
		return -EINTR;

	invalidate_sub(curlun);
	if (signal_pending(current))
		return -EINTR;

	/* Just try to read the requested blocks */
	while (amount_left > 0) {

		/* Figure out how much we need to read:
		 * Try to read the remaining amount, but not more than
		 * the buffer size.
		 * And don't try to read past the end of the file.
		 * If this means reading 0 then we were asked to read
		 * past the end of file. */
		amount = min((unsigned int) amount_left, mod_data.buflen);
		amount = min((loff_t) amount,
				curlun->file_length - file_offset);
		if (amount == 0) {
			curlun->sense_data =
					SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
			curlun->sense_data_info = file_offset >> 9;
			curlun->info_valid = 1;
			break;
		}

		/* Perform the read */
		file_offset_tmp = file_offset;
		nread = vfs_read(curlun->filp,
				(char __user *) bh->buf,
				amount, &file_offset_tmp);
		VLDBG(curlun, "file read %u @ %llu -> %d\n", amount,
				(unsigned long long) file_offset,
				(int) nread);
		if (signal_pending(current))
			return -EINTR;

		if (nread < 0) {
			LDBG(curlun, "error in file verify: %d\n",
					(int) nread);
			nread = 0;
		} else if (nread < amount) {
			LDBG(curlun, "partial file verify: %d/%u\n",
					(int) nread, amount);
			nread -= (nread & 511);	// Round down to a sector
		}
		if (nread == 0) {
			curlun->sense_data = SS_UNRECOVERED_READ_ERROR;
			curlun->sense_data_info = file_offset >> 9;
			curlun->info_valid = 1;
			break;
		}
		file_offset += nread;
		amount_left -= nread;
	}
	return 0;
}


/*-------------------------------------------------------------------------*/

static int do_inquiry(struct fsg_dev *fsg, struct fsg_buffhd *bh)
{
	u8	*buf = (u8 *) bh->buf;

	static char vendor_id[] = "Linux   ";
	static char product_disk_id[] = "File-Stor Gadget";
	static char product_cdrom_id[] = "File-CD Gadget  ";

	if (!fsg->curlun) {		// Unsupported LUNs are okay
		fsg->bad_lun_okay = 1;
		memset(buf, 0, 36);
		buf[0] = 0x7f;		// Unsupported, no device-type
		buf[4] = 31;		// Additional length
		return 36;
	}

	memset(buf, 0, 8);
	buf[0] = (mod_data.cdrom ? TYPE_CDROM : TYPE_DISK);
	if (mod_data.removable)
		buf[1] = 0x80;
	buf[2] = 2;		// ANSI SCSI level 2
	buf[3] = 2;		// SCSI-2 INQUIRY data format
	buf[4] = 31;		// Additional length
				// No special options
	sprintf(buf + 8, "%-8s%-16s%04x", vendor_id,
			(mod_data.cdrom ? product_cdrom_id :
				product_disk_id),
			mod_data.release);
	return 36;
}


static int do_request_sense(struct fsg_dev *fsg, struct fsg_buffhd *bh)
{
	struct lun	*curlun = fsg->curlun;
	u8		*buf = (u8 *) bh->buf;
	u32		sd, sdinfo;
	int		valid;

	/*
	 * From the SCSI-2 spec., section 7.9 (Unit attention condition):
	 *
	 * If a REQUEST SENSE command is received from an initiator
	 * with a pending unit attention condition (before the target
	 * generates the contingent allegiance condition), then the
	 * target shall either:
	 *   a) report any pending sense data and preserve the unit
	 *	attention condition on the logical unit, or,
	 *   b) report the unit attention condition, may discard any
	 *	pending sense data, and clear the unit attention
	 *	condition on the logical unit for that initiator.
	 *
	 * FSG normally uses option a); enable this code to use option b).
	 */
#if 0
	if (curlun && curlun->unit_attention_data != SS_NO_SENSE) {
		curlun->sense_data = curlun->unit_attention_data;
		curlun->unit_attention_data = SS_NO_SENSE;
	}
#endif

	if (!curlun) {		// Unsupported LUNs are okay
		fsg->bad_lun_okay = 1;
		sd = SS_LOGICAL_UNIT_NOT_SUPPORTED;
		sdinfo = 0;
		valid = 0;
	} else {
		sd = curlun->sense_data;
		sdinfo = curlun->sense_data_info;
		valid = curlun->info_valid << 7;
		curlun->sense_data = SS_NO_SENSE;
		curlun->sense_data_info = 0;
		curlun->info_valid = 0;
	}

	memset(buf, 0, 18);
	buf[0] = valid | 0x70;			// Valid, current error
	buf[2] = SK(sd);
	put_unaligned_be32(sdinfo, &buf[3]);	/* Sense information */
	buf[7] = 18 - 8;			// Additional sense length
	buf[12] = ASC(sd);
	buf[13] = ASCQ(sd);
	return 18;
}


static int do_read_capacity(struct fsg_dev *fsg, struct fsg_buffhd *bh)
{
	struct lun	*curlun = fsg->curlun;
	u32		lba = get_unaligned_be32(&fsg->cmnd[2]);
	int		pmi = fsg->cmnd[8];
	u8		*buf = (u8 *) bh->buf;

	/* Check the PMI and LBA fields */
	if (pmi > 1 || (pmi == 0 && lba != 0)) {
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}

	put_unaligned_be32(curlun->num_sectors - 1, &buf[0]);
						/* Max logical block */
	put_unaligned_be32(512, &buf[4]);	/* Block length */
	return 8;
}


static void store_cdrom_address(u8 *dest, int msf, u32 addr)
{
	if (msf) {
		/* Convert to Minutes-Seconds-Frames */
		addr >>= 2;		/* Convert to 2048-byte frames */
		addr += 2*75;		/* Lead-in occupies 2 seconds */
		dest[3] = addr % 75;	/* Frames */
		addr /= 75;
		dest[2] = addr % 60;	/* Seconds */
		addr /= 60;
		dest[1] = addr;		/* Minutes */
		dest[0] = 0;		/* Reserved */
	} else {
		/* Absolute sector */
		put_unaligned_be32(addr, dest);
	}
}

static int do_read_header(struct fsg_dev *fsg, struct fsg_buffhd *bh)
{
	struct lun	*curlun = fsg->curlun;
	int		msf = fsg->cmnd[1] & 0x02;
	u32		lba = get_unaligned_be32(&fsg->cmnd[2]);
	u8		*buf = (u8 *) bh->buf;

	if ((fsg->cmnd[1] & ~0x02) != 0) {		/* Mask away MSF */
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}
	if (lba >= curlun->num_sectors) {
		curlun->sense_data = SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
		return -EINVAL;
	}

	memset(buf, 0, 8);
	buf[0] = 0x01;		/* 2048 bytes of user data, rest is EC */
	store_cdrom_address(&buf[4], msf, lba);
	return 8;
}


static int do_read_toc(struct fsg_dev *fsg, struct fsg_buffhd *bh)
{
	struct lun	*curlun = fsg->curlun;
	int		msf = fsg->cmnd[1] & 0x02;
	int		start_track = fsg->cmnd[6];
	u8		*buf = (u8 *) bh->buf;

	if ((fsg->cmnd[1] & ~0x02) != 0 ||		/* Mask away MSF */
			start_track > 1) {
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}

	memset(buf, 0, 20);
	buf[1] = (20-2);		/* TOC data length */
	buf[2] = 1;			/* First track number */
	buf[3] = 1;			/* Last track number */
	buf[5] = 0x16;			/* Data track, copying allowed */
	buf[6] = 0x01;			/* Only track is number 1 */
	store_cdrom_address(&buf[8], msf, 0);

	buf[13] = 0x16;			/* Lead-out track is data */
	buf[14] = 0xAA;			/* Lead-out track number */
	store_cdrom_address(&buf[16], msf, curlun->num_sectors);
	return 20;
}


static int do_mode_sense(struct fsg_dev *fsg, struct fsg_buffhd *bh)
{
	struct lun	*curlun = fsg->curlun;
	int		mscmnd = fsg->cmnd[0];
	u8		*buf = (u8 *) bh->buf;
	u8		*buf0 = buf;
	int		pc, page_code;
	int		changeable_values, all_pages;
	int		valid_page = 0;
	int		len, limit;

	if ((fsg->cmnd[1] & ~0x08) != 0) {		// Mask away DBD
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}
	pc = fsg->cmnd[2] >> 6;
	page_code = fsg->cmnd[2] & 0x3f;
	if (pc == 3) {
		curlun->sense_data = SS_SAVING_PARAMETERS_NOT_SUPPORTED;
		return -EINVAL;
	}
	changeable_values = (pc == 1);
	all_pages = (page_code == 0x3f);

	/* Write the mode parameter header.  Fixed values are: default
	 * medium type, no cache control (DPOFUA), and no block descriptors.
	 * The only variable value is the WriteProtect bit.  We will fill in
	 * the mode data length later. */
	memset(buf, 0, 8);
	if (mscmnd == SC_MODE_SENSE_6) {
		buf[2] = (curlun->ro ? 0x80 : 0x00);		// WP, DPOFUA
		buf += 4;
		limit = 255;
	} else {			// SC_MODE_SENSE_10
		buf[3] = (curlun->ro ? 0x80 : 0x00);		// WP, DPOFUA
		buf += 8;
		limit = 65535;		// Should really be mod_data.buflen
	}

	/* No block descriptors */

	/* The mode pages, in numerical order.  The only page we support
	 * is the Caching page. */
	if (page_code == 0x08 || all_pages) {
		valid_page = 1;
		buf[0] = 0x08;		// Page code
		buf[1] = 10;		// Page length
		memset(buf+2, 0, 10);	// None of the fields are changeable

		if (!changeable_values) {
			buf[2] = 0x04;	// Write cache enable,
					// Read cache not disabled
					// No cache retention priorities
			put_unaligned_be16(0xffff, &buf[4]);
					/* Don't disable prefetch */
					/* Minimum prefetch = 0 */
			put_unaligned_be16(0xffff, &buf[8]);
					/* Maximum prefetch */
			put_unaligned_be16(0xffff, &buf[10]);
					/* Maximum prefetch ceiling */
		}
		buf += 12;
	}

	/* Check that a valid page was requested and the mode data length
	 * isn't too long. */
	len = buf - buf0;
	if (!valid_page || len > limit) {
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}

	/*  Store the mode data length */
	if (mscmnd == SC_MODE_SENSE_6)
		buf0[0] = len - 1;
	else
		put_unaligned_be16(len - 2, buf0);
	return len;
}


static int do_start_stop(struct fsg_dev *fsg)
{
	struct lun	*curlun = fsg->curlun;
	int		loej, start;

	if (!mod_data.removable) {
		curlun->sense_data = SS_INVALID_COMMAND;
		return -EINVAL;
	}

	// int immed = fsg->cmnd[1] & 0x01;
	loej = fsg->cmnd[4] & 0x02;
	start = fsg->cmnd[4] & 0x01;

#ifdef CONFIG_USB_FILE_STORAGE_TEST
	if ((fsg->cmnd[1] & ~0x01) != 0 ||		// Mask away Immed
			(fsg->cmnd[4] & ~0x03) != 0) {	// Mask LoEj, Start
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}

	if (!start) {

		/* Are we allowed to unload the media? */
		if (curlun->prevent_medium_removal) {
			LDBG(curlun, "unload attempt prevented\n");
			curlun->sense_data = SS_MEDIUM_REMOVAL_PREVENTED;
			return -EINVAL;
		}
		if (loej) {		// Simulate an unload/eject
			up_read(&fsg->filesem);
			down_write(&fsg->filesem);
			close_backing_file(curlun);
			up_write(&fsg->filesem);
			down_read(&fsg->filesem);
		}
	} else {

		/* Our emulation doesn't support mounting; the medium is
		 * available for use as soon as it is loaded. */
		if (!backing_file_is_open(curlun)) {
			curlun->sense_data = SS_MEDIUM_NOT_PRESENT;
			return -EINVAL;
		}
	}
#endif
	return 0;
}


static int do_prevent_allow(struct fsg_dev *fsg)
{
	struct lun	*curlun = fsg->curlun;
	int		prevent;

	if (!mod_data.removable) {
		curlun->sense_data = SS_INVALID_COMMAND;
		return -EINVAL;
	}

	prevent = fsg->cmnd[4] & 0x01;
	if ((fsg->cmnd[4] & ~0x01) != 0) {		// Mask away Prevent
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}

	if (curlun->prevent_medium_removal && !prevent)
		fsync_sub(curlun);
	curlun->prevent_medium_removal = prevent;
	return 0;
}


static int do_read_format_capacities(struct fsg_dev *fsg,
			struct fsg_buffhd *bh)
{
	struct lun	*curlun = fsg->curlun;
	u8		*buf = (u8 *) bh->buf;

	buf[0] = buf[1] = buf[2] = 0;
	buf[3] = 8;		// Only the Current/Maximum Capacity Descriptor
	buf += 4;

	put_unaligned_be32(curlun->num_sectors, &buf[0]);
						/* Number of blocks */
	put_unaligned_be32(512, &buf[4]);	/* Block length */
	buf[4] = 0x02;				/* Current capacity */
	return 12;
}


static int do_mode_select(struct fsg_dev *fsg, struct fsg_buffhd *bh)
{
	struct lun	*curlun = fsg->curlun;

	/* We don't support MODE SELECT */
	curlun->sense_data = SS_INVALID_COMMAND;
	return -EINVAL;
}


/*-------------------------------------------------------------------------*/

static int halt_bulk_in_endpoint(struct fsg_dev *fsg)
{
	int	rc;

	rc = fsg_set_halt(fsg, fsg->bulk_in);
	if (rc == -EAGAIN)
		VDBG(fsg, "delayed bulk-in endpoint halt\n");
	while (rc != 0) {
		if (rc != -EAGAIN) {
			WARNING(fsg, "usb_ep_set_halt -> %d\n", rc);
			rc = 0;
			break;
		}

		/* Wait for a short time and then try again */
		if (msleep_interruptible(100) != 0)
			return -EINTR;
		rc = usb_ep_set_halt(fsg->bulk_in);
	}
	return rc;
}

static int wedge_bulk_in_endpoint(struct fsg_dev *fsg)
{
	int	rc;

	DBG(fsg, "bulk-in set wedge\n");
	rc = usb_ep_set_wedge(fsg->bulk_in);
	if (rc == -EAGAIN)
		VDBG(fsg, "delayed bulk-in endpoint wedge\n");
	while (rc != 0) {
		if (rc != -EAGAIN) {
			WARNING(fsg, "usb_ep_set_wedge -> %d\n", rc);
			rc = 0;
			break;
		}

		/* Wait for a short time and then try again */
		if (msleep_interruptible(100) != 0)
			return -EINTR;
		rc = usb_ep_set_wedge(fsg->bulk_in);
	}
	return rc;
}

static int pad_with_zeros(struct fsg_dev *fsg)
{
	struct fsg_buffhd	*bh = fsg->next_buffhd_to_fill;
	u32			nkeep = bh->inreq->length;
	u32			nsend;
	int			rc;

	bh->state = BUF_STATE_EMPTY;		// For the first iteration
	fsg->usb_amount_left = nkeep + fsg->residue;
	while (fsg->usb_amount_left > 0) {

		/* Wait for the next buffer to be free */
		while (bh->state != BUF_STATE_EMPTY) {
			rc = sleep_thread(fsg);
			if (rc)
				return rc;
		}

		nsend = min(fsg->usb_amount_left, (u32) mod_data.buflen);
		memset(bh->buf + nkeep, 0, nsend - nkeep);
		bh->inreq->length = nsend;
		bh->inreq->zero = 0;
		start_transfer(fsg, fsg->bulk_in, bh->inreq,
				&bh->inreq_busy, &bh->state);
		bh = fsg->next_buffhd_to_fill = bh->next;
		fsg->usb_amount_left -= nsend;
		nkeep = 0;
	}
	return 0;
}

static int throw_away_data(struct fsg_dev *fsg)
{
	struct fsg_buffhd	*bh;
	u32			amount;
	int			rc;

	while ((bh = fsg->next_buffhd_to_drain)->state != BUF_STATE_EMPTY ||
			fsg->usb_amount_left > 0) {

		/* Throw away the data in a filled buffer */
		if (bh->state == BUF_STATE_FULL) {
			smp_rmb();
			bh->state = BUF_STATE_EMPTY;
			fsg->next_buffhd_to_drain = bh->next;

			/* A short packet or an error ends everything */
			if (bh->outreq->actual != bh->outreq->length ||
					bh->outreq->status != 0) {
				raise_exception(fsg, FSG_STATE_ABORT_BULK_OUT);
				return -EINTR;
			}
			continue;
		}

		/* Try to submit another request if we need one */
		bh = fsg->next_buffhd_to_fill;
		if (bh->state == BUF_STATE_EMPTY && fsg->usb_amount_left > 0) {
			amount = min(fsg->usb_amount_left,
					(u32) mod_data.buflen);

			/* amount is always divisible by 512, hence by
			 * the bulk-out maxpacket size */
			bh->outreq->length = bh->bulk_out_intended_length =
					amount;
			bh->outreq->short_not_ok = 1;
			start_transfer(fsg, fsg->bulk_out, bh->outreq,
					&bh->outreq_busy, &bh->state);
			fsg->next_buffhd_to_fill = bh->next;
			fsg->usb_amount_left -= amount;
			continue;
		}

		/* Otherwise wait for something to happen */
		rc = sleep_thread(fsg);
		if (rc)
			return rc;
	}
	return 0;
}


static int finish_reply(struct fsg_dev *fsg)
{
	struct fsg_buffhd	*bh = fsg->next_buffhd_to_fill;
	int			rc = 0;

	switch (fsg->data_dir) {
	case DATA_DIR_NONE:
		break;			// Nothing to send

	/* If we don't know whether the host wants to read or write,
	 * this must be CB or CBI with an unknown command.  We mustn't
	 * try to send or receive any data.  So stall both bulk pipes
	 * if we can and wait for a reset. */
	case DATA_DIR_UNKNOWN:
		if (mod_data.can_stall) {
			fsg_set_halt(fsg, fsg->bulk_out);
			rc = halt_bulk_in_endpoint(fsg);
		}
		break;

	/* All but the last buffer of data must have already been sent */
	case DATA_DIR_TO_HOST:
		if (fsg->data_size == 0)
			;		// Nothing to send

		/* If there's no residue, simply send the last buffer */
		else if (fsg->residue == 0) {
			bh->inreq->zero = 0;
			start_transfer(fsg, fsg->bulk_in, bh->inreq,
					&bh->inreq_busy, &bh->state);
			fsg->next_buffhd_to_fill = bh->next;
		}

		/* There is a residue.  For CB and CBI, simply mark the end
		 * of the data with a short packet.  However, if we are
		 * allowed to stall, there was no data at all (residue ==
		 * data_size), and the command failed (invalid LUN or
		 * sense data is set), then halt the bulk-in endpoint
		 * instead. */
		else if (!transport_is_bbb()) {
			if (mod_data.can_stall &&
					fsg->residue == fsg->data_size &&
	(!fsg->curlun || fsg->curlun->sense_data != SS_NO_SENSE)) {
				bh->state = BUF_STATE_EMPTY;
				rc = halt_bulk_in_endpoint(fsg);
			} else {
				bh->inreq->zero = 1;
				start_transfer(fsg, fsg->bulk_in, bh->inreq,
						&bh->inreq_busy, &bh->state);
				fsg->next_buffhd_to_fill = bh->next;
			}
		}

		/* For Bulk-only, if we're allowed to stall then send the
		 * short packet and halt the bulk-in endpoint.  If we can't
		 * stall, pad out the remaining data with 0's. */
		else {
			if (mod_data.can_stall) {
				bh->inreq->zero = 1;
				start_transfer(fsg, fsg->bulk_in, bh->inreq,
						&bh->inreq_busy, &bh->state);
				fsg->next_buffhd_to_fill = bh->next;
				rc = halt_bulk_in_endpoint(fsg);
			} else
				rc = pad_with_zeros(fsg);
		}
		break;

	/* We have processed all we want from the data the host has sent.
	 * There may still be outstanding bulk-out requests. */
	case DATA_DIR_FROM_HOST:
		if (fsg->residue == 0)
			;		// Nothing to receive

		/* Did the host stop sending unexpectedly early? */
		else if (fsg->short_packet_received) {
			raise_exception(fsg, FSG_STATE_ABORT_BULK_OUT);
			rc = -EINTR;
		}

		/* We haven't processed all the incoming data.  Even though
		 * we may be allowed to stall, doing so would cause a race.
		 * The controller may already have ACK'ed all the remaining
		 * bulk-out packets, in which case the host wouldn't see a
		 * STALL.  Not realizing the endpoint was halted, it wouldn't
		 * clear the halt -- leading to problems later on. */
#if 0
		else if (mod_data.can_stall) {
			fsg_set_halt(fsg, fsg->bulk_out);
			raise_exception(fsg, FSG_STATE_ABORT_BULK_OUT);
			rc = -EINTR;
		}
#endif

		/* We can't stall.  Read in the excess data and throw it
		 * all away. */
		else
			rc = throw_away_data(fsg);
		break;
	}
	return rc;
}


static int send_status(struct fsg_dev *fsg)
{
	struct lun		*curlun = fsg->curlun;
	struct fsg_buffhd	*bh;
	int			rc;
	u8			status = USB_STATUS_PASS;
	u32			sd, sdinfo = 0;

	/* Wait for the next buffer to become available */
	bh = fsg->next_buffhd_to_fill;
	while (bh->state != BUF_STATE_EMPTY) {
		rc = sleep_thread(fsg);
		if (rc)
			return rc;
	}

	if (curlun) {
		sd = curlun->sense_data;
		sdinfo = curlun->sense_data_info;
	} else if (fsg->bad_lun_okay)
		sd = SS_NO_SENSE;
	else
		sd = SS_LOGICAL_UNIT_NOT_SUPPORTED;

	if (fsg->phase_error) {
		DBG(fsg, "sending phase-error status\n");
		status = USB_STATUS_PHASE_ERROR;
		sd = SS_INVALID_COMMAND;
	} else if (sd != SS_NO_SENSE) {
		DBG(fsg, "sending command-failure status\n");
		status = USB_STATUS_FAIL;
		VDBG(fsg, "  sense data: SK x%02x, ASC x%02x, ASCQ x%02x;"
				"  info x%x\n",
				SK(sd), ASC(sd), ASCQ(sd), sdinfo);
	}

	if (transport_is_bbb()) {
		struct bulk_cs_wrap	*csw = bh->buf;

		/* Store and send the Bulk-only CSW */
		csw->Signature = cpu_to_le32(USB_BULK_CS_SIG);
		csw->Tag = fsg->tag;
		csw->Residue = cpu_to_le32(fsg->residue);
		csw->Status = status;

		bh->inreq->length = USB_BULK_CS_WRAP_LEN;
		bh->inreq->zero = 0;
		start_transfer(fsg, fsg->bulk_in, bh->inreq,
				&bh->inreq_busy, &bh->state);

	} else if (mod_data.transport_type == USB_PR_CB) {

		/* Control-Bulk transport has no status phase! */
		return 0;

	} else {			// USB_PR_CBI
		struct interrupt_data	*buf = bh->buf;

		/* Store and send the Interrupt data.  UFI sends the ASC
		 * and ASCQ bytes.  Everything else sends a Type (which
		 * is always 0) and the status Value. */
		if (mod_data.protocol_type == USB_SC_UFI) {
			buf->bType = ASC(sd);
			buf->bValue = ASCQ(sd);
		} else {
			buf->bType = 0;
			buf->bValue = status;
		}
		fsg->intreq->length = CBI_INTERRUPT_DATA_LEN;

		fsg->intr_buffhd = bh;		// Point to the right buffhd
		fsg->intreq->buf = bh->inreq->buf;
		fsg->intreq->context = bh;
		start_transfer(fsg, fsg->intr_in, fsg->intreq,
				&fsg->intreq_busy, &bh->state);
	}

	fsg->next_buffhd_to_fill = bh->next;
	return 0;
}


/*-------------------------------------------------------------------------*/

/* Check whether the command is properly formed and whether its data size
 * and direction agree with the values we already have. */
static int check_command(struct fsg_dev *fsg, int cmnd_size,
		enum data_direction data_dir, unsigned int mask,
		int needs_medium, const char *name)
{
	int			i;
	int			lun = fsg->cmnd[1] >> 5;
	static const char	dirletter[4] = {'u', 'o', 'i', 'n'};
	char			hdlen[20];
	struct lun		*curlun;

	/* Adjust the expected cmnd_size for protocol encapsulation padding.
	 * Transparent SCSI doesn't pad. */
	if (protocol_is_scsi())
		;

	/* There's some disagreement as to whether RBC pads commands or not.
	 * We'll play it safe and accept either form. */
	else if (mod_data.protocol_type == USB_SC_RBC) {
		if (fsg->cmnd_size == 12)
			cmnd_size = 12;

	/* All the other protocols pad to 12 bytes */
	} else
		cmnd_size = 12;

	hdlen[0] = 0;
	if (fsg->data_dir != DATA_DIR_UNKNOWN)
		sprintf(hdlen, ", H%c=%u", dirletter[(int) fsg->data_dir],
				fsg->data_size);
	VDBG(fsg, "SCSI command: %s;  Dc=%d, D%c=%u;  Hc=%d%s\n",
			name, cmnd_size, dirletter[(int) data_dir],
			fsg->data_size_from_cmnd, fsg->cmnd_size, hdlen);

	/* We can't reply at all until we know the correct data direction
	 * and size. */
	if (fsg->data_size_from_cmnd == 0)
		data_dir = DATA_DIR_NONE;
	if (fsg->data_dir == DATA_DIR_UNKNOWN) {	// CB or CBI
		fsg->data_dir = data_dir;
		fsg->data_size = fsg->data_size_from_cmnd;

	} else {					// Bulk-only
		if (fsg->data_size < fsg->data_size_from_cmnd) {

			/* Host data size < Device data size is a phase error.
			 * Carry out the command, but only transfer as much
			 * as we are allowed. */
			fsg->data_size_from_cmnd = fsg->data_size;
			fsg->phase_error = 1;
		}
	}
	fsg->residue = fsg->usb_amount_left = fsg->data_size;

	/* Conflicting data directions is a phase error */
	if (fsg->data_dir != data_dir && fsg->data_size_from_cmnd > 0) {
		fsg->phase_error = 1;
		return -EINVAL;
	}

	/* Verify the length of the command itself */
	if (cmnd_size != fsg->cmnd_size) {

		/* Special case workaround: There are plenty of buggy SCSI
		 * implementations. Many have issues with cbw->Length
		 * field passing a wrong command size. For those cases we
		 * always try to work around the problem by using the length
		 * sent by the host side provided it is at least as large
		 * as the correct command length.
		 * Examples of such cases would be MS-Windows, which issues
		 * REQUEST SENSE with cbw->Length == 12 where it should
		 * be 6, and xbox360 issuing INQUIRY, TEST UNIT READY and
		 * REQUEST SENSE with cbw->Length == 10 where it should
		 * be 6 as well.
		 */
		if (cmnd_size <= fsg->cmnd_size) {
			DBG(fsg, "%s is buggy! Expected length %d "
					"but we got %d\n", name,
					cmnd_size, fsg->cmnd_size);
			cmnd_size = fsg->cmnd_size;
		} else {
			fsg->phase_error = 1;
			return -EINVAL;
		}
	}

	/* Check that the LUN values are consistent */
	if (transport_is_bbb()) {
		if (fsg->lun != lun)
			DBG(fsg, "using LUN %d from CBW, "
					"not LUN %d from CDB\n",
					fsg->lun, lun);
	} else
		fsg->lun = lun;		// Use LUN from the command

	/* Check the LUN */
	if (fsg->lun >= 0 && fsg->lun < fsg->nluns) {
		fsg->curlun = curlun = &fsg->luns[fsg->lun];
		if (fsg->cmnd[0] != SC_REQUEST_SENSE) {
			curlun->sense_data = SS_NO_SENSE;
			curlun->sense_data_info = 0;
			curlun->info_valid = 0;
		}
	} else {
		fsg->curlun = curlun = NULL;
		fsg->bad_lun_okay = 0;

		/* INQUIRY and REQUEST SENSE commands are explicitly allowed
		 * to use unsupported LUNs; all others may not. */
		if (fsg->cmnd[0] != SC_INQUIRY &&
				fsg->cmnd[0] != SC_REQUEST_SENSE) {
			DBG(fsg, "unsupported LUN %d\n", fsg->lun);
			return -EINVAL;
		}
	}

	/* If a unit attention condition exists, only INQUIRY and
	 * REQUEST SENSE commands are allowed; anything else must fail. */
	if (curlun && curlun->unit_attention_data != SS_NO_SENSE &&
			fsg->cmnd[0] != SC_INQUIRY &&
			fsg->cmnd[0] != SC_REQUEST_SENSE) {
		curlun->sense_data = curlun->unit_attention_data;
		curlun->unit_attention_data = SS_NO_SENSE;
		return -EINVAL;
	}

	/* Check that only command bytes listed in the mask are non-zero */
	fsg->cmnd[1] &= 0x1f;			// Mask away the LUN
	for (i = 1; i < cmnd_size; ++i) {
		if (fsg->cmnd[i] && !(mask & (1 << i))) {
			if (curlun)
				curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
			return -EINVAL;
		}
	}

	/* If the medium isn't mounted and the command needs to access
	 * it, return an error. */
	if (curlun && !backing_file_is_open(curlun) && needs_medium) {
		curlun->sense_data = SS_MEDIUM_NOT_PRESENT;
		return -EINVAL;
	}

	return 0;
}


static int do_scsi_command(struct fsg_dev *fsg)
{
	struct fsg_buffhd	*bh;
	int			rc;
	int			reply = -EINVAL;
	int			i;
	static char		unknown[16];

	dump_cdb(fsg);

	/* Wait for the next buffer to become available for data or status */
	bh = fsg->next_buffhd_to_drain = fsg->next_buffhd_to_fill;
	while (bh->state != BUF_STATE_EMPTY) {
		rc = sleep_thread(fsg);
		if (rc)
			return rc;
	}
	fsg->phase_error = 0;
	fsg->short_packet_received = 0;

	down_read(&fsg->filesem);	// We're using the backing file
	switch (fsg->cmnd[0]) {

	case SC_INQUIRY:
		fsg->data_size_from_cmnd = fsg->cmnd[4];
		if ((reply = check_command(fsg, 6, DATA_DIR_TO_HOST,
				(1<<4), 0,
				"INQUIRY")) == 0)
			reply = do_inquiry(fsg, bh);
		break;

	case SC_MODE_SELECT_6:
		fsg->data_size_from_cmnd = fsg->cmnd[4];
		if ((reply = check_command(fsg, 6, DATA_DIR_FROM_HOST,
				(1<<1) | (1<<4), 0,
				"MODE SELECT(6)")) == 0)
			reply = do_mode_select(fsg, bh);
		break;

	case SC_MODE_SELECT_10:
		fsg->data_size_from_cmnd = get_unaligned_be16(&fsg->cmnd[7]);
		if ((reply = check_command(fsg, 10, DATA_DIR_FROM_HOST,
				(1<<1) | (3<<7), 0,
				"MODE SELECT(10)")) == 0)
			reply = do_mode_select(fsg, bh);
		break;

	case SC_MODE_SENSE_6:
		fsg->data_size_from_cmnd = fsg->cmnd[4];
		if ((reply = check_command(fsg, 6, DATA_DIR_TO_HOST,
				(1<<1) | (1<<2) | (1<<4), 0,
				"MODE SENSE(6)")) == 0)
			reply = do_mode_sense(fsg, bh);
		break;

	case SC_MODE_SENSE_10:
		fsg->data_size_from_cmnd = get_unaligned_be16(&fsg->cmnd[7]);
		if ((reply = check_command(fsg, 10, DATA_DIR_TO_HOST,
				(1<<1) | (1<<2) | (3<<7), 0,
				"MODE SENSE(10)")) == 0)
			reply = do_mode_sense(fsg, bh);
		break;

	case SC_PREVENT_ALLOW_MEDIUM_REMOVAL:
		fsg->data_size_from_cmnd = 0;
		if ((reply = check_command(fsg, 6, DATA_DIR_NONE,
				(1<<4), 0,
				"PREVENT-ALLOW MEDIUM REMOVAL")) == 0)
			reply = do_prevent_allow(fsg);
		break;

	case SC_READ_6:
		i = fsg->cmnd[4];
		fsg->data_size_from_cmnd = (i == 0 ? 256 : i) << 9;
		if ((reply = check_command(fsg, 6, DATA_DIR_TO_HOST,
				(7<<1) | (1<<4), 1,
				"READ(6)")) == 0)
			reply = do_read(fsg);
		break;

	case SC_READ_10:
		fsg->data_size_from_cmnd =
				get_unaligned_be16(&fsg->cmnd[7]) << 9;
		if ((reply = check_command(fsg, 10, DATA_DIR_TO_HOST,
				(1<<1) | (0xf<<2) | (3<<7), 1,
				"READ(10)")) == 0)
			reply = do_read(fsg);
		break;

	case SC_READ_12:
		fsg->data_size_from_cmnd =
				get_unaligned_be32(&fsg->cmnd[6]) << 9;
		if ((reply = check_command(fsg, 12, DATA_DIR_TO_HOST,
				(1<<1) | (0xf<<2) | (0xf<<6), 1,
				"READ(12)")) == 0)
			reply = do_read(fsg);
		break;

	case SC_READ_CAPACITY:
		fsg->data_size_from_cmnd = 8;
		if ((reply = check_command(fsg, 10, DATA_DIR_TO_HOST,
				(0xf<<2) | (1<<8), 1,
				"READ CAPACITY")) == 0)
			reply = do_read_capacity(fsg, bh);
		break;

	case SC_READ_HEADER:
		if (!mod_data.cdrom)
			goto unknown_cmnd;
		fsg->data_size_from_cmnd = get_unaligned_be16(&fsg->cmnd[7]);
		if ((reply = check_command(fsg, 10, DATA_DIR_TO_HOST,
				(3<<7) | (0x1f<<1), 1,
				"READ HEADER")) == 0)
			reply = do_read_header(fsg, bh);
		break;

	case SC_READ_TOC:
		if (!mod_data.cdrom)
			goto unknown_cmnd;
		fsg->data_size_from_cmnd = get_unaligned_be16(&fsg->cmnd[7]);
		if ((reply = check_command(fsg, 10, DATA_DIR_TO_HOST,
				(7<<6) | (1<<1), 1,
				"READ TOC")) == 0)
			reply = do_read_toc(fsg, bh);
		break;

	case SC_READ_FORMAT_CAPACITIES:
		fsg->data_size_from_cmnd = get_unaligned_be16(&fsg->cmnd[7]);
		if ((reply = check_command(fsg, 10, DATA_DIR_TO_HOST,
				(3<<7), 1,
				"READ FORMAT CAPACITIES")) == 0)
			reply = do_read_format_capacities(fsg, bh);
		break;

	case SC_REQUEST_SENSE:
		fsg->data_size_from_cmnd = fsg->cmnd[4];
		if ((reply = check_command(fsg, 6, DATA_DIR_TO_HOST,
				(1<<4), 0,
				"REQUEST SENSE")) == 0)
			reply = do_request_sense(fsg, bh);
		break;

	case SC_START_STOP_UNIT:
		fsg->data_size_from_cmnd = 0;
		if ((reply = check_command(fsg, 6, DATA_DIR_NONE,
				(1<<1) | (1<<4), 0,
				"START-STOP UNIT")) == 0)
			reply = do_start_stop(fsg);
		break;

	case SC_SYNCHRONIZE_CACHE:
		fsg->data_size_from_cmnd = 0;
		if ((reply = check_command(fsg, 10, DATA_DIR_NONE,
				(0xf<<2) | (3<<7), 1,
				"SYNCHRONIZE CACHE")) == 0)
			reply = do_synchronize_cache(fsg);
		break;

	case SC_TEST_UNIT_READY:
		fsg->data_size_from_cmnd = 0;
		reply = check_command(fsg, 6, DATA_DIR_NONE,
				0, 1,
				"TEST UNIT READY");
		break;

	/* Although optional, this command is used by MS-Windows.  We
	 * support a minimal version: BytChk must be 0. */
	case SC_VERIFY:
		fsg->data_size_from_cmnd = 0;
		if ((reply = check_command(fsg, 10, DATA_DIR_NONE,
				(1<<1) | (0xf<<2) | (3<<7), 1,
				"VERIFY")) == 0)
			reply = do_verify(fsg);
		break;

	case SC_WRITE_6:
		i = fsg->cmnd[4];
		fsg->data_size_from_cmnd = (i == 0 ? 256 : i) << 9;
		if ((reply = check_command(fsg, 6, DATA_DIR_FROM_HOST,
				(7<<1) | (1<<4), 1,
				"WRITE(6)")) == 0)
			reply = do_write(fsg);
		break;

	case SC_WRITE_10:
		fsg->data_size_from_cmnd =
				get_unaligned_be16(&fsg->cmnd[7]) << 9;
		if ((reply = check_command(fsg, 10, DATA_DIR_FROM_HOST,
				(1<<1) | (0xf<<2) | (3<<7), 1,
				"WRITE(10)")) == 0)
			reply = do_write(fsg);
		break;

	case SC_WRITE_12:
		fsg->data_size_from_cmnd =
				get_unaligned_be32(&fsg->cmnd[6]) << 9;
		if ((reply = check_command(fsg, 12, DATA_DIR_FROM_HOST,
				(1<<1) | (0xf<<2) | (0xf<<6), 1,
				"WRITE(12)")) == 0)
			reply = do_write(fsg);
		break;

	/* Some mandatory commands that we recognize but don't implement.
	 * They don't mean much in this setting.  It's left as an exercise
	 * for anyone interested to implement RESERVE and RELEASE in terms
	 * of Posix locks. */
	case SC_FORMAT_UNIT:
	case SC_RELEASE:
	case SC_RESERVE:
	case SC_SEND_DIAGNOSTIC:
		// Fall through

	default:
 unknown_cmnd:
		fsg->data_size_from_cmnd = 0;
		sprintf(unknown, "Unknown x%02x", fsg->cmnd[0]);
		if ((reply = check_command(fsg, fsg->cmnd_size,
				DATA_DIR_UNKNOWN, 0xff, 0, unknown)) == 0) {
			fsg->curlun->sense_data = SS_INVALID_COMMAND;
			reply = -EINVAL;
		}
		break;
	}
	up_read(&fsg->filesem);

	if (reply == -EINTR || signal_pending(current))
		return -EINTR;

	/* Set up the single reply buffer for finish_reply() */
	if (reply == -EINVAL)
		reply = 0;		// Error reply length
	if (reply >= 0 && fsg->data_dir == DATA_DIR_TO_HOST) {
		reply = min((u32) reply, fsg->data_size_from_cmnd);
		bh->inreq->length = reply;
		bh->state = BUF_STATE_FULL;
		fsg->residue -= reply;
	}				// Otherwise it's already set

	return 0;
}


/*-------------------------------------------------------------------------*/

static int received_cbw(struct fsg_dev *fsg, struct fsg_buffhd *bh)
{
	struct usb_request	*req = bh->outreq;
	struct bulk_cb_wrap	*cbw = req->buf;

	/* Was this a real packet?  Should it be ignored? */
	if (req->status || test_bit(IGNORE_BULK_OUT, &fsg->atomic_bitflags))
		return -EINVAL;

	/* Is the CBW valid? */
	if (req->actual != USB_BULK_CB_WRAP_LEN ||
			cbw->Signature != cpu_to_le32(
				USB_BULK_CB_SIG)) {
		DBG(fsg, "invalid CBW: len %u sig 0x%x\n",
				req->actual,
				le32_to_cpu(cbw->Signature));

		/* The Bulk-only spec says we MUST stall the IN endpoint
		 * (6.6.1), so it's unavoidable.  It also says we must
		 * retain this state until the next reset, but there's
		 * no way to tell the controller driver it should ignore
		 * Clear-Feature(HALT) requests.
		 *
		 * We aren't required to halt the OUT endpoint; instead
		 * we can simply accept and discard any data received
		 * until the next reset. */
		wedge_bulk_in_endpoint(fsg);
		set_bit(IGNORE_BULK_OUT, &fsg->atomic_bitflags);
		return -EINVAL;
	}

	/* Is the CBW meaningful? */
	if (cbw->Lun >= MAX_LUNS || cbw->Flags & ~USB_BULK_IN_FLAG ||
			cbw->Length <= 0 || cbw->Length > MAX_COMMAND_SIZE) {
		DBG(fsg, "non-meaningful CBW: lun = %u, flags = 0x%x, "
				"cmdlen %u\n",
				cbw->Lun, cbw->Flags, cbw->Length);

		/* We can do anything we want here, so let's stall the
		 * bulk pipes if we are allowed to. */
		if (mod_data.can_stall) {
			fsg_set_halt(fsg, fsg->bulk_out);
			halt_bulk_in_endpoint(fsg);
		}
		return -EINVAL;
	}

	/* Save the command for later */
	fsg->cmnd_size = cbw->Length;
	memcpy(fsg->cmnd, cbw->CDB, fsg->cmnd_size);
	if (cbw->Flags & USB_BULK_IN_FLAG)
		fsg->data_dir = DATA_DIR_TO_HOST;
	else
		fsg->data_dir = DATA_DIR_FROM_HOST;
	fsg->data_size = le32_to_cpu(cbw->DataTransferLength);
	if (fsg->data_size == 0)
		fsg->data_dir = DATA_DIR_NONE;
	fsg->lun = cbw->Lun;
	fsg->tag = cbw->Tag;
	return 0;
}


static int get_next_command(struct fsg_dev *fsg)
{
	struct fsg_buffhd	*bh;
	int			rc = 0;

	if (transport_is_bbb()) {

		/* Wait for the next buffer to become available */
		bh = fsg->next_buffhd_to_fill;
		while (bh->state != BUF_STATE_EMPTY) {
			rc = sleep_thread(fsg);
			if (rc)
				return rc;
		}

		/* Queue a request to read a Bulk-only CBW */
		set_bulk_out_req_length(fsg, bh, USB_BULK_CB_WRAP_LEN);
		bh->outreq->short_not_ok = 1;
		start_transfer(fsg, fsg->bulk_out, bh->outreq,
				&bh->outreq_busy, &bh->state);

		/* We will drain the buffer in software, which means we
		 * can reuse it for the next filling.  No need to advance
		 * next_buffhd_to_fill. */

		/* Wait for the CBW to arrive */
		while (bh->state != BUF_STATE_FULL) {
			rc = sleep_thread(fsg);
			if (rc)
				return rc;
		}
		smp_rmb();
		rc = received_cbw(fsg, bh);
		bh->state = BUF_STATE_EMPTY;

	} else {		// USB_PR_CB or USB_PR_CBI

		/* Wait for the next command to arrive */
		while (fsg->cbbuf_cmnd_size == 0) {
			rc = sleep_thread(fsg);
			if (rc)
				return rc;
		}

		/* Is the previous status interrupt request still busy?
		 * The host is allowed to skip reading the status,
		 * so we must cancel it. */
		if (fsg->intreq_busy)
			usb_ep_dequeue(fsg->intr_in, fsg->intreq);

		/* Copy the command and mark the buffer empty */
		fsg->data_dir = DATA_DIR_UNKNOWN;
		spin_lock_irq(&fsg->lock);
		fsg->cmnd_size = fsg->cbbuf_cmnd_size;
		memcpy(fsg->cmnd, fsg->cbbuf_cmnd, fsg->cmnd_size);
		fsg->cbbuf_cmnd_size = 0;
		spin_unlock_irq(&fsg->lock);
	}
	return rc;
}


/*-------------------------------------------------------------------------*/

static int enable_endpoint(struct fsg_dev *fsg, struct usb_ep *ep,
		const struct usb_endpoint_descriptor *d)
{
	int	rc;

	ep->driver_data = fsg;
	rc = usb_ep_enable(ep, d);
	if (rc)
		ERROR(fsg, "can't enable %s, result %d\n", ep->name, rc);
	return rc;
}

static int alloc_request(struct fsg_dev *fsg, struct usb_ep *ep,
		struct usb_request **preq)
{
	*preq = usb_ep_alloc_request(ep, GFP_ATOMIC);
	if (*preq)
		return 0;
	ERROR(fsg, "can't allocate request for %s\n", ep->name);
	return -ENOMEM;
}

/*
 * Reset interface setting and re-init endpoint state (toggle etc).
 * Call with altsetting < 0 to disable the interface.  The only other
 * available altsetting is 0, which enables the interface.
 */
static int do_set_interface(struct fsg_dev *fsg, int altsetting)
{
	int	rc = 0;
	int	i;
	const struct usb_endpoint_descriptor	*d;

	if (fsg->running)
		DBG(fsg, "reset interface\n");

reset:
	/* Deallocate the requests */
	for (i = 0; i < NUM_BUFFERS; ++i) {
		struct fsg_buffhd *bh = &fsg->buffhds[i];

		if (bh->inreq) {
			usb_ep_free_request(fsg->bulk_in, bh->inreq);
			bh->inreq = NULL;
		}
		if (bh->outreq) {
			usb_ep_free_request(fsg->bulk_out, bh->outreq);
			bh->outreq = NULL;
		}
	}
	if (fsg->intreq) {
		usb_ep_free_request(fsg->intr_in, fsg->intreq);
		fsg->intreq = NULL;
	}

	/* Disable the endpoints */
	if (fsg->bulk_in_enabled) {
		usb_ep_disable(fsg->bulk_in);
		fsg->bulk_in_enabled = 0;
	}
	if (fsg->bulk_out_enabled) {
		usb_ep_disable(fsg->bulk_out);
		fsg->bulk_out_enabled = 0;
	}
	if (fsg->intr_in_enabled) {
		usb_ep_disable(fsg->intr_in);
		fsg->intr_in_enabled = 0;
	}

	fsg->running = 0;
	if (altsetting < 0 || rc != 0)
		return rc;

	DBG(fsg, "set interface %d\n", altsetting);

	/* Enable the endpoints */
	d = ep_desc(fsg->gadget, &fs_bulk_in_desc, &hs_bulk_in_desc);
	if ((rc = enable_endpoint(fsg, fsg->bulk_in, d)) != 0)
		goto reset;
	fsg->bulk_in_enabled = 1;

	d = ep_desc(fsg->gadget, &fs_bulk_out_desc, &hs_bulk_out_desc);
	if ((rc = enable_endpoint(fsg, fsg->bulk_out, d)) != 0)
		goto reset;
	fsg->bulk_out_enabled = 1;
	fsg->bulk_out_maxpacket = le16_to_cpu(d->wMaxPacketSize);
	clear_bit(IGNORE_BULK_OUT, &fsg->atomic_bitflags);

	if (transport_is_cbi()) {
		d = ep_desc(fsg->gadget, &fs_intr_in_desc, &hs_intr_in_desc);
		if ((rc = enable_endpoint(fsg, fsg->intr_in, d)) != 0)
			goto reset;
		fsg->intr_in_enabled = 1;
	}

	/* Allocate the requests */
	for (i = 0; i < NUM_BUFFERS; ++i) {
		struct fsg_buffhd	*bh = &fsg->buffhds[i];

		if ((rc = alloc_request(fsg, fsg->bulk_in, &bh->inreq)) != 0)
			goto reset;
		if ((rc = alloc_request(fsg, fsg->bulk_out, &bh->outreq)) != 0)
			goto reset;
		bh->inreq->buf = bh->outreq->buf = bh->buf;
		bh->inreq->context = bh->outreq->context = bh;
		bh->inreq->complete = bulk_in_complete;
		bh->outreq->complete = bulk_out_complete;
	}
	if (transport_is_cbi()) {
		if ((rc = alloc_request(fsg, fsg->intr_in, &fsg->intreq)) != 0)
			goto reset;
		fsg->intreq->complete = intr_in_complete;
	}

	fsg->running = 1;
	for (i = 0; i < fsg->nluns; ++i)
		fsg->luns[i].unit_attention_data = SS_RESET_OCCURRED;
	return rc;
}


/*
 * Change our operational configuration.  This code must agree with the code
 * that returns config descriptors, and with interface altsetting code.
 *
 * It's also responsible for power management interactions.  Some
 * configurations might not work with our current power sources.
 * For now we just assume the gadget is always self-powered.
 */
static int do_set_config(struct fsg_dev *fsg, u8 new_config)
{
	int	rc = 0;

	/* Disable the single interface */
	if (fsg->config != 0) {
		DBG(fsg, "reset config\n");
		fsg->config = 0;
		rc = do_set_interface(fsg, -1);
	}

	/* Enable the interface */
	if (new_config != 0) {
		fsg->config = new_config;
		if ((rc = do_set_interface(fsg, 0)) != 0)
			fsg->config = 0;	// Reset on errors
		else {
			char *speed;

			switch (fsg->gadget->speed) {
			case USB_SPEED_LOW:	speed = "low";	break;
			case USB_SPEED_FULL:	speed = "full";	break;
			case USB_SPEED_HIGH:	speed = "high";	break;
			default: 		speed = "?";	break;
			}
			INFO(fsg, "%s speed config #%d\n", speed, fsg->config);
		}
	}
	return rc;
}


/*-------------------------------------------------------------------------*/

static void handle_exception(struct fsg_dev *fsg)
{
	siginfo_t		info;
	int			sig;
	int			i;
	int			num_active;
	struct fsg_buffhd	*bh;
	enum fsg_state		old_state;
	u8			new_config;
	struct lun		*curlun;
	unsigned int		exception_req_tag;
	int			rc;

	/* Clear the existing signals.  Anything but SIGUSR1 is converted
	 * into a high-priority EXIT exception. */
	for (;;) {
		sig = dequeue_signal_lock(current, &current->blocked, &info);
		if (!sig)
			break;
		if (sig != SIGUSR1) {
			if (fsg->state < FSG_STATE_EXIT)
				DBG(fsg, "Main thread exiting on signal\n");
			raise_exception(fsg, FSG_STATE_EXIT);
		}
	}

	/* Cancel all the pending transfers */
	if (fsg->intreq_busy)
		usb_ep_dequeue(fsg->intr_in, fsg->intreq);
	for (i = 0; i < NUM_BUFFERS; ++i) {
		bh = &fsg->buffhds[i];
		if (bh->inreq_busy)
			usb_ep_dequeue(fsg->bulk_in, bh->inreq);
		if (bh->outreq_busy)
			usb_ep_dequeue(fsg->bulk_out, bh->outreq);
	}

	/* Wait until everything is idle */
	for (;;) {
		num_active = fsg->intreq_busy;
		for (i = 0; i < NUM_BUFFERS; ++i) {
			bh = &fsg->buffhds[i];
			num_active += bh->inreq_busy + bh->outreq_busy;
		}
		if (num_active == 0)
			break;
		if (sleep_thread(fsg))
			return;
	}

	/* Clear out the controller's fifos */
	if (fsg->bulk_in_enabled)
		usb_ep_fifo_flush(fsg->bulk_in);
	if (fsg->bulk_out_enabled)
		usb_ep_fifo_flush(fsg->bulk_out);
	if (fsg->intr_in_enabled)
		usb_ep_fifo_flush(fsg->intr_in);

	/* Reset the I/O buffer states and pointers, the SCSI
	 * state, and the exception.  Then invoke the handler. */
	spin_lock_irq(&fsg->lock);

	for (i = 0; i < NUM_BUFFERS; ++i) {
		bh = &fsg->buffhds[i];
		bh->state = BUF_STATE_EMPTY;
	}
	fsg->next_buffhd_to_fill = fsg->next_buffhd_to_drain =
			&fsg->buffhds[0];

	exception_req_tag = fsg->exception_req_tag;
	new_config = fsg->new_config;
	old_state = fsg->state;

	if (old_state == FSG_STATE_ABORT_BULK_OUT)
		fsg->state = FSG_STATE_STATUS_PHASE;
	else {
		for (i = 0; i < fsg->nluns; ++i) {
			curlun = &fsg->luns[i];
			curlun->prevent_medium_removal = 0;
			curlun->sense_data = curlun->unit_attention_data =
					SS_NO_SENSE;
			curlun->sense_data_info = 0;
			curlun->info_valid = 0;
		}
		fsg->state = FSG_STATE_IDLE;
	}
	spin_unlock_irq(&fsg->lock);

	/* Carry out any extra actions required for the exception */
	switch (old_state) {
	default:
		break;

	case FSG_STATE_ABORT_BULK_OUT:
		send_status(fsg);
		spin_lock_irq(&fsg->lock);
		if (fsg->state == FSG_STATE_STATUS_PHASE)
			fsg->state = FSG_STATE_IDLE;
		spin_unlock_irq(&fsg->lock);
		break;

	case FSG_STATE_RESET:
		/* In case we were forced against our will to halt a
		 * bulk endpoint, clear the halt now.  (The SuperH UDC
		 * requires this.) */
		if (test_and_clear_bit(IGNORE_BULK_OUT, &fsg->atomic_bitflags))
			usb_ep_clear_halt(fsg->bulk_in);

		if (transport_is_bbb()) {
			if (fsg->ep0_req_tag == exception_req_tag)
				ep0_queue(fsg);	// Complete the status stage

		} else if (transport_is_cbi())
			send_status(fsg);	// Status by interrupt pipe

		/* Technically this should go here, but it would only be
		 * a waste of time.  Ditto for the INTERFACE_CHANGE and
		 * CONFIG_CHANGE cases. */
		// for (i = 0; i < fsg->nluns; ++i)
		//	fsg->luns[i].unit_attention_data = SS_RESET_OCCURRED;
		break;

	case FSG_STATE_INTERFACE_CHANGE:
		rc = do_set_interface(fsg, 0);
		if (fsg->ep0_req_tag != exception_req_tag)
			break;
		if (rc != 0)			// STALL on errors
			fsg_set_halt(fsg, fsg->ep0);
		else				// Complete the status stage
			ep0_queue(fsg);
		break;

	case FSG_STATE_CONFIG_CHANGE:
		rc = do_set_config(fsg, new_config);
		if (fsg->ep0_req_tag != exception_req_tag)
			break;
		if (rc != 0)			// STALL on errors
			fsg_set_halt(fsg, fsg->ep0);
		else				// Complete the status stage
			ep0_queue(fsg);
		break;

	case FSG_STATE_DISCONNECT:
		fsync_all(fsg);
		do_set_config(fsg, 0);		// Unconfigured state
		break;

	case FSG_STATE_EXIT:
	case FSG_STATE_TERMINATED:
		do_set_config(fsg, 0);			// Free resources
		spin_lock_irq(&fsg->lock);
		fsg->state = FSG_STATE_TERMINATED;	// Stop the thread
		spin_unlock_irq(&fsg->lock);
		break;
	}
}


/*-------------------------------------------------------------------------*/

static int fsg_main_thread(void *fsg_)
{
	struct fsg_dev		*fsg = fsg_;

	/* Allow the thread to be killed by a signal, but set the signal mask
	 * to block everything but INT, TERM, KILL, and USR1. */
	allow_signal(SIGINT);
	allow_signal(SIGTERM);
	allow_signal(SIGKILL);
	allow_signal(SIGUSR1);

	/* Allow the thread to be frozen */
	set_freezable();

	/* Arrange for userspace references to be interpreted as kernel
	 * pointers.  That way we can pass a kernel pointer to a routine
	 * that expects a __user pointer and it will work okay. */
	set_fs(get_ds());

	/* The main loop */
	while (fsg->state != FSG_STATE_TERMINATED) {
		if (exception_in_progress(fsg) || signal_pending(current)) {
			handle_exception(fsg);
			continueile_}

		if (!fsg->running) {le_ssleep_thread
 * file_storage.c -- File-backget_next_commandevelo)le_storage.c -
		spin_lock_irq(&d USB
 *
file_backeexception_in_progresstern
 * Ald USBstate = FSG_STATE_DATA_PHASEile_rved.un
 *
 * Redistribution  and usdo_scsi8 Alan Stern
 || finish_replytern
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modifimodiUSon, are permitted provided that the following send_ wituy forms, wil rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modifiIDLre permitted provided that the fol- Filerved.
 *
 * Redistribution ad USB USB d_task = NULL;other med provided that the follo/* If we are exiting because of a signal, unregister the
	 * gadget driver. */
nd ustest_and_clear_bit(REGISTERED, distriatomichoutflags
 * Ausb_oducts_dorse or p_deriveedistY, thisd copyriLetromo unbind and e winup routines knowbuted USB d hasrs maed fromcompleteftwars maedistriistribunotifier, 0);
}


/*- either version 2 of that License or (at your option) any
 * later versi*/ion,ightuted-200 twof the
 * Gldercalled whilral eroducts is rse or ped,
 *GHT HY THEr must own d USBfilesem for wrmay nd fro
 witic int open_backing_ OR (struct lun *curlun, const char * OR name)
{
	int				ro The * THE OR 			ERCHp.
 * 3. Th AND FITc = -EINVA. The * THEinodTICULHALL .
 * 3. Thloff_ND FIsizc -- OR
 * CONnum_sectorsUTORS BE LIABminFOR ANY DIopyriR/W iht hocan, R/OIAL,
 *ND ANfromro = IED WA->TNESSbackeroe GadgAR
 * PAR
 _UT N(RCHANTAB, O_RDWR | O_LARGEFILE Foundand us-EROFS == PTR_ERR PROp
 * AlTIAL 1;
	}CLUDINro * AT LIMITED TO,
 * PROCUREMENT OFONLYUBSTITUTE GOODS OR SEbackISE, DATA, OR GadgLINFO(IED WARR"unabRIGHoBUT N T LIMIT PART: %s\n", PARTNTABI SERreturnF USE, DATA, OS; ORCLUDINGATA, ->f_mPYRI& FMODE_WRITE
 * APROFITS;ABILITTHE USE path.dentry * ACOPYRIGHOF THE POSSIBILITY ->d_HALL IABILITCOPYRI&& S_ISBLKorage ->i OF TIN CONTbackbdev_tribuonly as a USB viceOR
 * PROFITS; O elseIAL,(!COPYRI|| !et aREG as a USB Mass StoragRACT, STRICT LIinvalid PART typG
 * NEGLIGENCE OR OTHEgo, ORutY WAY Oyright hocan't PubliHT H OR , it's no good.te pr it also illWARRIGHT Ha tech useillustr-ppead from thi!THE USE opM dri OF THE Pop->Publi:
 * y way to paio,
 * IN CONTRACT, STRICT LInuinenollustrILITseful gadget driver for a USB
 * deOUT OF THE USE to pincreathe
 * behavior of increE, EVEN IF ADVITRIB = i_TRIB,
 *  as a USB Mapping->hostLIABILITle pa< 0N CONTRACT, STRICT LIABILITY, Ofder nuineTRIB
 * NEGLIGENCE OR OTHERLAIM(int)r.  ( is provided by a LE FOR ANY  =r.  ( >> 9;	// Feter.  ( in 512-byte b
 *
s
RECT, INCIDEOFITS; as amod_data.cdromN CONTThe gadget w&= ~3e thaReducTY, Oa multiplsed
 2048
IRECT, INCIDE = 300*4e thaSm THEst track AND300 framesrage deThe gadget w>= 256*60*75*4e GadgeThe gadget wil(lk-Bulk o - 1) * 4ile_sorage drivers in a Utoo big
 * NEGLIGENCE OR OTHETRACT, STRICT LIAsNCLUppea first %d media NEGLs th		cess iLE FOR ANY and/or y a reguThe gadget w< ECT, INCIDEass Storage drivers in a Uaramspt (For CD-ROM emulation,
 * ac-ETOOSMA3. Thor a USB
 * devi* ALD TO,IN ANY WDAMAGES (I = TNESSDAMAGES T LIMITED Tndor ID, Produe_lengthwill inndor ID, PrThe gadget wilLE FOR ANY DIRLDBG STRICT LIR TORT (INCLUDING
 * NEGLIGENCE OR OTH * ac0;

out:
INTER_closdition, DAMrentSS OR Iits oWISE) rcndatioNCLUDINvoie teoseOT LIMITED TO,
 * THE IMPLIED WAILITY f  STRICTProducN CONTRple logical u moduRT (INCLUDING\n" OTHEfput the correspon" mothe correspo.
 * 3. Th}ptional
 * "ster. t show_ro,
 * THEdevice *dev, om the number_attribute * * i,S OF MEbufILITY
 * THE IM	LIED WA = num_to_lun( hosADVIsing thsprintf(buf, "%dNEGLIDAMAGES (Iundatiber of LUNs is takenD TO,
 * THEnumber of "file" elements;
 * it is 1 if "fe" ae" is not given.  If "removable" is not set then a S FOR A Pftwaev	*fsg" is no* ALdrvmoduN would N's 		*and LUNs is,
 *ADVIdown  Accedistri OR IMPLIABILITT LIMITED TO_isO,
 * m 1 to ) { thaGributed") as pu OSSINTAB
		* ThdPOSSI(&the correspoHE POSSI, ust bPAGE_SIZE the  SERVICEY, WHET OR
 * PLAIM USE, DAfile" e or Gadge.  Nostrlen that t	memmovemust bp, rcfile_sbuf[rc] = '\n';	 thaAdd a newlintracthe m++ore co0157 (0c0ve or {SFF-// Nle ptechsing th0 able (als noare modeown back (0cuply all LUNs have to be sing the optional
 * "LUNs is ttoren from the number of "file" elements;
 * it is 1 if "fe" aNTIES OF MEust bUNs is countt givedrive, butAL Ds inld be settded.  Ideally
 * each LUN would be settable independently as a disk drive or  AND iADVISED sscan must be s", &i) != 1, EVEWISE) ED. IN NOvice,All Generaincre-eBILITYutionsY, Ochangecols: OPYRIGHT HT (INCLUDINGte pris" modudd fromrently all LUNs have to be the same type.  The CD-ROM
 * emulang
 * files are s.  Last b* interr-in endpreventespece optional "BUSY drive or ns te default
 * !!i" mohat prevent
 * a buffer from besribuo  specified for each errupt-out endpoint is also needed for CBI)  The memory
 * requin an unspecified
 * or empty backing filename means the LUN parameter.
 * Support is includednd high-speed operation.
 *
 * Note that the driver is slightly non-portable in that it own backi and the corrd by mo_medium_removalGadg some with hardware restrictions that prevent
 *eject attempted by more than onell be usint.
bulk-in"Doorolle
 *
ed"* device,Rall	e a),
 ilNCLU
 *
 *  from thil-spe > 0Defauuf[l-spe-1]S OFmmon * A((N's me)alse), boolean f 0 valueUgh!for buEB dev LUNs c rt
 *	 isn't an
incressue, but there may be some with hardware restrictions t module parameter (m 1 to le" and "ro"unit
 * enourcemodu = SS_MEDIUM_NOT_PRESENT * device,Loadndpoult BBB, tradrom			Default false,0]ions th fulUT NOT LIMITED TO,IED WARRe
 * SERVICEh fu= 0 * AllTAPI, QIC, UFI, 8070, or SCSr SFF-SSalso READY_TOse numbeRANSITIONnot set, t name (CB, CBI, or BBBrue),
 B Pr< 0 ? rc : is incndation, Tbulk-out permissions the  requixxx pointerIDED BquiriEXPRE_nder()EQUENCLUDINDEVICE_ATTR(ro, 0444, taken f, * 3.);STORAGE_TEST is not d a bu only the "d a bu", "ro"ion, either version 2 of that License or (at your option) any
 * later version.
 al
 * "luns"FIG_releasO,
 * THEkref *reot given.  Ifable independentoraainer_of(re* Sugs are availabulk(FSG
	kfree
 * ->lunset uhe lun<n>ch LUN.  If iluns"lutly king files annumber of "settings are available in
 tly as a disk drive or
	d th_ted distrite fihe backing e
 * gadget's sysf/* __iC, Uorhed b */ding  under,
 * THEn.
 *
 * A *oductssettings are availabdependen* AL*
 * ALsk drle tab.SE ARE DIfile	Default faemovableum is loadn.
 request	*reqAMAGn> sep0reql siple fsg LIABnder than oe without
 *    specific prior written permi for buUorse or promo sysfsviceit is 1 OR I the I coLUNsEQUENIED (i-ROM  i <ed.
 *nubdi; ++i protocvable" idistribuns[i] SERVICEDAMAGES (CONTRIBUTe Gadgements;
tall					bacdio trackand "&s nointe_is nott 375D, Revision 10L, 7-SEP-93, available at
d a file_stol name (RBC, 8020 or
 *					 375D, Rdorse or p, 7-SEP-93, afile_st from
 * X3T9.2 Pare modest; evice, itneral Publiis illalSB dy dead, tellhputtors ma U Gefrom thith or witho!ut
 * modifiTERMINATEDr ID
 *aise_ in sourcet dri
 * modifiEXITFSG),wait_for8 Alas phttp the Free Software
 *follow down tterms of the
 * s/desLIED thller_docs/usb also docuL") as pumass-ufi10.pdf>.
 */


/*l SerialFreIGHT Hor SCbuffE)
 nformation
 * technoNUM_BUFFERSll Com
	the lun<n> sost hd- 2".5 (FSGread that hand * is b the ost ofLIED endE_SIZ 0 documentreqions the lunreqrom tFSG),n.
 ep_e lu
 * is bt, Revep0bulkq
 * thresng are not allowed whcdrom" optional
 * "plet
 * li check_paOnlyZE)
,
 * THEable in epenILITY ANDproeed  ANDgcnumr driveSrequ handlefault value the wle" modultransport_ly u = USB_PR_BULK;vice reset requests aNTAB = "Bulkast b"e
 * thread ime,ocolare forwardeSC_SCSIusing SIGUSR1 signalsm of "exTequesaDefau* sh"rface,
 ommultiipheral * throIS" IDED BNU GnUSB hto be ILITY, te prhalt bulk * compleer irrect butuffeonsed
 themollepresent,te prdisnd
 * inllsble-bx0525 (*
 * ALis_sht, Revle tab. devfor pendinat91signals/excep* cale" moduleanbutillmber of
 *		le" modulrectoryroducxffffulationPp0 reque wage Clset
 *				Drisa1100main routinolleSB hsupsts e ("GPLs
 * for pendingat thpolling is necessetIntenal TS; the Coint is thatn.
 *
 * ALain routin_numbersignals/excep SERVICE is th>duct ID
 setjmp/longjmp eq 0x0Bulk+ tInterf the CD-ROM WARNINget driv driver rea'%s'USB hoscognizre thor SFFignals/exce->upports this will prevent unmount99iversal Serime,will docs_strtolo setjmp/l1 signalsparmcdrom" Found
#ifdef CONFIG_ardeGOOD_STORed bTESTumes a
trnicmpo setjmp/lrequests ad KILL"BBB", 10)roductdule  drive
snfiguratiosetay ndrive or as a.
 *
 * In normal operation the maiCthread is started ce reset requests are forwarded tCB bac* thread in the form of "exCin rou-cept usibind() callback and stopped during fsg_unbind().I  But it can also
 * exit when it receives a signal,hould there's no point leaving the
 * gadget-IIZE)rupt running whns tERRORet driv of a gerequests 
 * NEGLIn normal operation the  OTHERWISE) ED. IN NO AY OUT O.
 *
 * In normal  TERM, and KILL"tionsread is st ||ointms, thce they
 * sharted during the gadget's
 * fsg_bind() callback and stopped do exit.  The firRBCroblem is resolved through the usRBCn also
 * exit 1 signals (since they
 RBCThis makes thpt any ongoing fi * Trunning when the thread is dead.o exit.  The fir8020", 4m is resolvedready
 * FSG_STATE_TERMINATED.
 *
ATAPproblem is resolved through the us * Tving fsg_unbind() check
 * fsg->state; * T won't try to stop the thread  * Ti (cular)the state is already
 * FSG_STATE_TERMINATED.
 *
QI The3m is resolved through the usQIaving fsg_unbind() check
 * fsg->state;QIt won't try to stop the thread QIC-157the state is already
 * FSG_STATE_TERMINATED.
 *
UFproblem is resolved through the usUFf the
 sg_unbind() check
 * fsg->state;UF This makes thpt any ongoing fince he state is already
 * FSG_STATE_TERMINATED.
 *
 *7To provide maximumffer heads (struc7 fsg_buffhd).  In principle the pipeline 7an be
 * arbitrarily long; in pra70ieregistered at two places, and the 1 signal* thread can indir TERM, and KI_unbind() which in turn cale" modulbuflen
 * eed bCACHE_MAShe
 s no setjmp/lfrom an< started two places, and the from asually true),
 *	protocol" m}
#endifmptyrts them into
 * an EXIT excES, Ir size u0ead by wakeup calls.  MaFIG_USB_Fusting a write-enable tab.  Changes to the ro
 * settthe_fsgSE ARE DIut cthe medium is loaded or if CD-ROM emulation
ep		*eM
 * emulation
 * is being or a CD-R	*OSSIust bOM
 h the doducts =ns -- aum is are passed to the mai * filed.
 *
 *ossibly b * Coarked BUSY)->derive or SCSIe buf, so eB Proany
 * ep0 requestern
  1.0ct IDr a USB
 *has no setjmp/lonll		bleulationEBILITY,n thPAGE_CACHinterface (al2-r10l.pdf>. .inte.OF TH= 064ionale transport sp* endock
 *					ba SERVICE!le" module parameter  This is
ropecification allows. nnot reliablycessary for soTNESSrsal SerialFder out how manythe "Ithe
 * houldtandests
 = can indir- Smaled Storroduct IDrequiaxo setjmp/lLE FGENCE ORs, 1uLIABILITi > MAX_LUNSa state variable indicatin * opeed
 he ": if "remie optional ". IN NO or a USB
 * device,Creithoon the ",OR TORtheirmost device cs, the rse or promote prLUNhe "rems CONmmandd from<n> subdi = kzalloc(i *Suppoof,
 * THE IM), GFP_KERNE"ro"-backed USBubdir ID
 *	pro-ENOMEM is provided by a logy - Smaaramied
 ation
 * technology - Small Computer System Interface - 2" doce default
 *  setjmp/loo 2" documenle" module parach was based ROFITS; for OUT tdevprevent unms directoryge responses forO opera= &d
 * findevge responses forderivem InterY, thislve
 * ws.  ThiSB I disk dr 7-SEP-93, ava * file_ile I/O NTABon, which migh"%s-lunmoryon a elays notraril as device )ware dning inmpleti375D, RevRMAT CAPACITIES), whmodule D-ROM ACT, t drivfaiHE Ctorectly.  FLUN%din softwarher than tr a USB
 *  (0che original ep0 recly thn 10L, 7-SEP-93, avr SFF-ilable at
 * moduleesolved	 original
 * request, as the host will no longer be waf>.  one.  When th23 (READ FORMAT CAPACITIES), which wut completion ofas based on the
 * "Uage rmulatgeejecting/lo
 * the orle" moduld a [i]Gadg*e-change, or conWhen the originalduct=0xPPPP		Default 0xa4r SFF-ation-change).  Wmodule paruest associatedve or as a  driver to avoid stalsubmtwo places, nand a  givenLIED d notoftware durring the next devest associated wtain circumse kelk
 s
 * partswe willroughestsand disauignanfi bacsetowed when te* Th*
 * Warning: Thio the mai&fs_state andes thand use p parameterning: T_the
e filPTY again (possibly lk-inclaimt a
 * status cause lit up
 = the  file is too long.  It ought to be split uout * into a header file plus about 3 separate .c files, to handle the details
 * of the Gadget, USB tancs Storag thisequests ais_cbi(IN CONTile is too long.  It ought to be spintrup
 * into aa header fille plus about 3 separaate .c files, to handle the details
 * of the Gacause #includude <li request wx uptConfigscripntrolisn'tents;
/lim.bMaxPacketSize) anarked EMPTmaxph>
#i;clude <linux/ridVendo intcpuot see16ce-change,vk.h>
host <linux/spinloProd THEinclude <linux/string.hp.h>
#ilude <linux/freebcdDumber include <linux/string.hthe mediumn requcache.h>
#include < ? 3 : 2)e thaN
 * the hs
 * parts BUSfinux/rwNumE
 * partsvoidauild is not iver faceSubClasativsg_unbind() check
 * f with respect to linkinP signaltely
 * comprequests are farked_func/usb[i + FS_FUNC *	b 1 -_EP_ENTRIESD-RO* 3. Tts
 * for pendindualspeedowed wheg is eh compilation ..H ensuring init/exit sections work to r bulssume SY) uselk-on s of .h>
#inclon, anLIED both ime f
#incln
 * qualre
 *rwsem.h>
#include <linux/slab.h>
#includmbine ... part completaddnaryVIDED B2.c part3ould.
 */
#include "uhsplit up
 * in.by cooperA------ the US split up
 * in

#define DRIVER_>
#i--------VERBOSE_

#define DRIVER_DESC		"File-bacAME		"g_file_storage"
#deine DRI#include <li

#define DRIVER_DESC		"Fil const char longname[] = DRIVturn can tefor pendinotought toecessotle ir lomA wherever |rwardeOTG_HNPa bacta, implementor bulk-ic this
 *and interrupt-request
 * completion nod.
 *
 * Th * Vng th*
 * Waral Bect events.
 * Comp when the host sendletidevice reset. nt-0 evellowpt (oc(EP0tinee onHESE IDs with any other 0 evenriver!!  Ever!!
 * s a singls St0ss_docs/s res("Dual BSD/GPL")les most of the work.  Interrupt routines field
 * is eles "file" m the 	*b be distrim the contf.c"

/*-al BSD/GSG drie/stat-in---------.  We a.. partha#incS "AS
 t-requee resstraiwork withd has no wtanc(andemote SIZE)is d-in) for users de "ubh* Instead:  alloach buffer headHESE IDs with ana head*/


/*
#include mpletio*/

SOFTW= bh +ITS; OR er assumes seroutines fi the].
#definver assumes se0f-pow downis specifirefl dev hasactualroducts power sourer o.
 *
 **
 * ALSB IselfAME " footprintmoduln file mmanufdebur * Fout enthe
DUMP_MSGS
#"%s %sigurat%sthat d* linutstrari)->sy VLDB, #define VLDBG	LDrectory, the system shutdowfine On alustll ep0 r, serial[] wecificatloaded fromultipanee Galk-otorages to
 jD ANencPYRIllinR(lue DRIe
 * iver of
ilesANTIES, le race.
 * The dout endpE_DEB) - 2tech+= 2torageun   tedS OF 		letiDRIVER_VERSION[i / 2f-poweas a c sendbreake permfile m&SE_DEBUi]
#if02Xcifi
 * threthe distribution.
 kistriburequesappe_mainr USB dht ta, thein a - \
	dev-oductshan oILITY, WHETHhe distributiont, and .  Note also ->dev , fmt , ##  device reset.  Hopat happensfmt,argDESC ",efine LW: ",fmt,args...) \ " than oat happens chips.h"

he "=ag.  Thlogy - Sma whilill be tead:  alloPATH_MAXHESE IDs with ane race.
 * The driver will always use the "no-stall" approach fy be some with hardware restrictions tck an* 3. Th...) \ill be g is eqck and no audio tracks; hence therr SFF-  le da
 * nee, argsfile_sacking file per LUNt , ## args)


n of "transport" modro=%ne DING
 * NEGLodule ing status, (p ? p : "(error)")-157 (0c03)he lun--------is gadget drivache.h>
#=%s (x%02x)lems
 * * thread in the form ofd can indirectly callly u \
	adget driv1 signalgned int	num_ros;
	unsigned pt any ongoino use.
 * At any timcan_stall;
	int		cck.h>
ID=x%04x, r.h>
#i	product;
Rectoryroductm_ros;
	unsigned >
#inco use.
 * At a>
#id can indirthe mediumll;
	int		co avoid sLUNS	the kLUNS	e parLUNS	from a=%um_ros;
	unsigned o avoid sd can indir
 * the kos;
	unsigned e pard can indirfrom aame;
	int		proI/Oal Publipit notify ttion_pid_nen refe , fmt , ## amoduletd on "Gadget Zero" by David Brownell.
 * The dT Speneral PublitocharrtoconfNCLUmt,awake_upnd bcary forfmt, args...) \
	* buffer h
s about 3 sep:
qual to the eABILITY, Orning: Thiurel gets
 * part than o;
MODULE_TSUPern"ng fileh or without
 * modifi 14, 1998,e thawn tss Storag Comm(file and adjrence to trward.  There is a main kernel
 * sing the optionons are available; default
 * values are used for everything else.
 *
 * The pathnames of the bsuspeadjusting a write-enable tab.  Changes to the ro
 * setting are not allowed when e;
	int		proGO);
MO than oR_VENDORSUSPENDZero" by David Brownell.
 * * gadget's sysfhe bac. paDULE_PARM_DESC(luns, "number of LUNs");

module_param_named(removable, mod_data.removable, a");

heavily based on _PARM_DESC(removable, "true to simulat_PARM_DESC(ro, "true to force read-only");

module_param_named(luns, mod_data.nluns, ui/O is complare not erivencludnon-TES = {nd converts them inGADGET_DUALSPEED
	.ime f		ce theyove
_HIGH,
#ng as  are available. */
#FsignBUSY.  
	.ompilati	= to emulatlongol_pa
	.ndervailFIG_USB_ranser is _parm, cer is ransdisconnectparm, c(transportranssetup_parm, c(BBB,ort (O);
MOCBI, or O);
MOransa");

_parm, ca");

,
ESC(n-TEST ly t		.NTABrp, to emulatshortta.tran	.ownharp, THIS_MODULEocol// prevent unm...BC, 802amed(pr UFI, "
		"80a");

 UFI, "
},
}optito be EMPTY, filling t  alloluns.  Changes to the ro
 * sied
 * llows the bendif /epenlt when the host sends
 er will be usplementirved.
 *
 *n by the Fbution a* linrwseml LUNs have to be mulatO);
MODULE_set, iSC(pros_docs/usbmass-ufi10.pdf>.
 */


/*
 h the bssibly b * buffer head to be EMPTY, filling tO);
M_IRUGO);
mes, nuld be settable indepen by a completi ushort, Sonse is subsing the op, "USB h the buffe originaln.
 *
 * ALRNATIVELY, this software maymodule parmulate ejecting/loading the medium_IRUGO);
MODULmodu  Th);
M);

modu optiadget's sysf_means versterms oS_IRUGO);
MODULE_PARM_DESng which the bufThe driver's SCSI cogs)
#deifBus Mass Storh an
 *ass UFI Cractraightf this software without
 *    specific prior written permission.
 *
 * ALTERNATIVELY, this software may be disWadefiand has0xffff,	//* 1. Re <lr chipdevclass_docs/usbmass-ufi10.pdf>.
 */


/*
 mulate ejecting/loading the medium (-------ed byine TYPE_DI);
