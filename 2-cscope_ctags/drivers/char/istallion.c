/*****************************************************************************/

/*
 *	istallion.c  -- stallion intelligent multiport serial driver.
 *
 *	Copyright (C) 1996-1999  Stallion Technologies
 *	Copyright (C) 1994-1996  Greg Ungerer.
 *
 *	This code is loosely based on the Linux serial driver, written by
 *	Linus Torvalds, Theodore T'so and others.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 */

/*****************************************************************************/

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/seq_file.h>
#include <linux/cdk.h>
#include <linux/comstats.h>
#include <linux/istallion.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/eisa.h>
#include <linux/ctype.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#include <linux/pci.h>

/*****************************************************************************/

/*
 *	Define different board types. Not all of the following board types
 *	are supported by this driver. But I will use the standard "assigned"
 *	board numbers. Currently supported boards are abbreviated as:
 *	ECP = EasyConnection 8/64, ONB = ONboard, BBY = Brumby and
 *	STAL = Stallion.
 */
#define	BRD_UNKNOWN	0
#define	BRD_STALLION	1
#define	BRD_BRUMBY4	2
#define	BRD_ONBOARD2	3
#define	BRD_ONBOARD	4
#define	BRD_ONBOARDE	7
#define	BRD_ECP		23
#define BRD_ECPE	24
#define	BRD_ECPMC	25
#define	BRD_ECPPCI	29

#define	BRD_BRUMBY	BRD_BRUMBY4

/*
 *	Define a configuration structure to hold the board configuration.
 *	Need to set this up in the code (for now) with the boards that are
 *	to be configured into the system. This is what needs to be modified
 *	when adding/removing/modifying boards. Each line entry in the
 *	stli_brdconf[] array is a board. Each line contains io/irq/memory
 *	ranges for that board (as well as what type of board it is).
 *	Some examples:
 *		{ BRD_ECP, 0x2a0, 0, 0xcc000, 0, 0 },
 *	This line will configure an EasyConnection 8/64 at io address 2a0,
 *	and shared memory address of cc000. Multiple EasyConnection 8/64
 *	boards can share the same shared memory address space. No interrupt
 *	is required for this board type.
 *	Another example:
 *		{ BRD_ECPE, 0x5000, 0, 0x80000000, 0, 0 },
 *	This line will configure an EasyConnection 8/64 EISA in slot 5 and
 *	shared memory address of 0x80000000 (2 GByte). Multiple
 *	EasyConnection 8/64 EISA boards can share the same shared memory
 *	address space. No interrupt is required for this board type.
 *	Another example:
 *		{ BRD_ONBOARD, 0x240, 0, 0xd0000, 0, 0 },
 *	This line will configure an ONboard (ISA type) at io address 240,
 *	and shared memory address of d0000. Multiple ONboards can share
 *	the same shared memory address space. No interrupt required.
 *	Another example:
 *		{ BRD_BRUMBY4, 0x360, 0, 0xc8000, 0, 0 },
 *	This line will configure a Brumby board (any number of ports!) at
 *	io address 360 and shared memory address of c8000. All Brumby boards
 *	configured into a system must have their own separate io and memory
 *	addresses. No interrupt is required.
 *	Another example:
 *		{ BRD_STALLION, 0x330, 0, 0xd0000, 0, 0 },
 *	This line will configure an original Stallion board at io address 330
 *	and shared memory address d0000 (this would only be valid for a "V4.0"
 *	or Rev.O Stallion board). All Stallion boards configured into the
 *	system must have their own separate io and memory addresses. No
 *	interrupt is required.
 */

struct stlconf {
	int		brdtype;
	int		ioaddr1;
	int		ioaddr2;
	unsigned long	memaddr;
	int		irq;
	int		irqtype;
};

static unsigned int stli_nrbrds;

/* stli_lock must NOT be taken holding brd_lock */
static spinlock_t stli_lock;	/* TTY logic lock */
static spinlock_t brd_lock;	/* Board logic lock */

/*
 *	There is some experimental EISA board detection code in this driver.
 *	By default it is disabled, but for those that want to try it out,
 *	then set the define below to be 1.
 */
#define	STLI_EISAPROBE	0

/*****************************************************************************/

/*
 *	Define some important driver characteristics. Device major numbers
 *	allocated as per Linux Device Registry.
 */
#ifndef	STL_SIOMEMMAJOR
#define	STL_SIOMEMMAJOR		28
#endif
#ifndef	STL_SERIALMAJOR
#define	STL_SERIALMAJOR		24
#endif
#ifndef	STL_CALLOUTMAJOR
#define	STL_CALLOUTMAJOR	25
#endif

/*****************************************************************************/

/*
 *	Define our local driver identity first. Set up stuff to deal with
 *	all the local structures required by a serial tty driver.
 */
static char	*stli_drvtitle = "Stallion Intelligent Multiport Serial Driver";
static char	*stli_drvname = "istallion";
static char	*stli_drvversion = "5.6.0";
static char	*stli_serialname = "ttyE";

static struct tty_driver	*stli_serial;
static const struct tty_port_operations stli_port_ops;

#define	STLI_TXBUFSIZE		4096

/*
 *	Use a fast local buffer for cooked characters. Typically a whole
 *	bunch of cooked characters come in for a port, 1 at a time. So we
 *	save those up into a local buffer, then write out the whole lot
 *	with a large memcpy. Just use 1 buffer for all ports, since its
 *	use it is only need for short periods of time by each port.
 */
static char			*stli_txcookbuf;
static int			stli_txcooksize;
static int			stli_txcookrealsize;
static struct tty_struct	*stli_txcooktty;

/*
 *	Define a local default termios struct. All ports will be created
 *	with this termios initially. Basically all it defines is a raw port
 *	at 9600 baud, 8 data bits, no parity, 1 stop bit.
 */
static struct ktermios		stli_deftermios = {
	.c_cflag	= (B9600 | CS8 | CREAD | HUPCL | CLOCAL),
	.c_cc		= INIT_C_CC,
	.c_ispeed	= 9600,
	.c_ospeed	= 9600,
};

/*
 *	Define global stats structures. Not used often, and can be
 *	re-used for each stats call.
 */
static comstats_t	stli_comstats;
static combrd_t		stli_brdstats;
static struct asystats	stli_cdkstats;

/*****************************************************************************/

static DEFINE_MUTEX(stli_brdslock);
static struct stlibrd	*stli_brds[STL_MAXBRDS];

static int		stli_shared;

/*
 *	Per board state flags. Used with the state field of the board struct.
 *	Not really much here... All we need to do is keep track of whether
 *	the board has been detected, and whether it is actually running a slave
 *	or not.
 */
#define	BST_FOUND	0x1
#define	BST_STARTED	0x2
#define	BST_PROBED	0x4

/*
 *	Define the set of port state flags. These are marked for internal
 *	state purposes only, usually to do with the state of communications
 *	with the slave. Most of them need to be updated atomically, so always
 *	use the bit setting operations (unless protected by cli/sti).
 */
#define	ST_INITIALIZING	1
#define	ST_OPENING	2
#define	ST_CLOSING	3
#define	ST_CMDING	4
#define	ST_TXBUSY	5
#define	ST_RXING	6
#define	ST_DOFLUSHRX	7
#define	ST_DOFLUSHTX	8
#define	ST_DOSIGS	9
#define	ST_RXSTOP	10
#define	ST_GETSIGS	11

/*
 *	Define an array of board names as printable strings. Handy for
 *	referencing boards when printing trace and stuff.
 */
static char	*stli_brdnames[] = {
	"Unknown",
	"Stallion",
	"Brumby",
	"ONboard-MC",
	"ONboard",
	"Brumby",
	"Brumby",
	"ONboard-EI",
	NULL,
	"ONboard",
	"ONboard-MC",
	"ONboard-MC",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"EasyIO",
	"EC8/32-AT",
	"EC8/32-MC",
	"EC8/64-AT",
	"EC8/64-EI",
	"EC8/64-MC",
	"EC8/32-PCI",
	"EC8/64-PCI",
	"EasyIO-PCI",
	"EC/RA-PCI",
};

/*****************************************************************************/

/*
 *	Define some string labels for arguments passed from the module
 *	load line. These allow for easy board definitions, and easy
 *	modification of the io, memory and irq resoucres.
 */

static char	*board0[8];
static char	*board1[8];
static char	*board2[8];
static char	*board3[8];

static char	**stli_brdsp[] = {
	(char **) &board0,
	(char **) &board1,
	(char **) &board2,
	(char **) &board3
};

/*
 *	Define a set of common board names, and types. This is used to
 *	parse any module arguments.
 */

static struct stlibrdtype {
	char	*name;
	int	type;
} stli_brdstr[] = {
	{ "stallion", BRD_STALLION },
	{ "1", BRD_STALLION },
	{ "brumby", BRD_BRUMBY },
	{ "brumby4", BRD_BRUMBY },
	{ "brumby/4", BRD_BRUMBY },
	{ "brumby-4", BRD_BRUMBY },
	{ "brumby8", BRD_BRUMBY },
	{ "brumby/8", BRD_BRUMBY },
	{ "brumby-8", BRD_BRUMBY },
	{ "brumby16", BRD_BRUMBY },
	{ "brumby/16", BRD_BRUMBY },
	{ "brumby-16", BRD_BRUMBY },
	{ "2", BRD_BRUMBY },
	{ "onboard2", BRD_ONBOARD2 },
	{ "onboard-2", BRD_ONBOARD2 },
	{ "onboard/2", BRD_ONBOARD2 },
	{ "onboard-mc", BRD_ONBOARD2 },
	{ "onboard/mc", BRD_ONBOARD2 },
	{ "onboard-mca", BRD_ONBOARD2 },
	{ "onboard/mca", BRD_ONBOARD2 },
	{ "3", BRD_ONBOARD2 },
	{ "onboard", BRD_ONBOARD },
	{ "onboardat", BRD_ONBOARD },
	{ "4", BRD_ONBOARD },
	{ "onboarde", BRD_ONBOARDE },
	{ "onboard-e", BRD_ONBOARDE },
	{ "onboard/e", BRD_ONBOARDE },
	{ "onboard-ei", BRD_ONBOARDE },
	{ "onboard/ei", BRD_ONBOARDE },
	{ "7", BRD_ONBOARDE },
	{ "ecp", BRD_ECP },
	{ "ecpat", BRD_ECP },
	{ "ec8/64", BRD_ECP },
	{ "ec8/64-at", BRD_ECP },
	{ "ec8/64-isa", BRD_ECP },
	{ "23", BRD_ECP },
	{ "ecpe", BRD_ECPE },
	{ "ecpei", BRD_ECPE },
	{ "ec8/64-e", BRD_ECPE },
	{ "ec8/64-ei", BRD_ECPE },
	{ "24", BRD_ECPE },
	{ "ecpmc", BRD_ECPMC },
	{ "ec8/64-mc", BRD_ECPMC },
	{ "ec8/64-mca", BRD_ECPMC },
	{ "25", BRD_ECPMC },
	{ "ecppci", BRD_ECPPCI },
	{ "ec/ra", BRD_ECPPCI },
	{ "ec/ra-pc", BRD_ECPPCI },
	{ "ec/ra-pci", BRD_ECPPCI },
	{ "29", BRD_ECPPCI },
};

/*
 *	Define the module agruments.
 */
MODULE_AUTHOR("Greg Ungerer");
MODULE_DESCRIPTION("Stallion Intelligent Multiport Serial Driver");
MODULE_LICENSE("GPL");


module_param_array(board0, charp, NULL, 0);
MODULE_PARM_DESC(board0, "Board 0 config -> name[,ioaddr[,memaddr]");
module_param_array(board1, charp, NULL, 0);
MODULE_PARM_DESC(board1, "Board 1 config -> name[,ioaddr[,memaddr]");
module_param_array(board2, charp, NULL, 0);
MODULE_PARM_DESC(board2, "Board 2 config -> name[,ioaddr[,memaddr]");
module_param_array(board3, charp, NULL, 0);
MODULE_PARM_DESC(board3, "Board 3 config -> name[,ioaddr[,memaddr]");

#if STLI_EISAPROBE != 0
/*
 *	Set up a default memory address table for EISA board probing.
 *	The default addresses are all bellow 1Mbyte, which has to be the
 *	case anyway. They should be safe, since we only read values from
 *	them, and interrupts are disabled while we do it. If the higher
 *	memory support is compiled in then we also try probing around
 *	the 1Gb, 2Gb and 3Gb areas as well...
 */
static unsigned long	stli_eisamemprobeaddrs[] = {
	0xc0000,    0xd0000,    0xe0000,    0xf0000,
	0x80000000, 0x80010000, 0x80020000, 0x80030000,
	0x40000000, 0x40010000, 0x40020000, 0x40030000,
	0xc0000000, 0xc0010000, 0xc0020000, 0xc0030000,
	0xff000000, 0xff010000, 0xff020000, 0xff030000,
};

static int	stli_eisamempsize = ARRAY_SIZE(stli_eisamemprobeaddrs);
#endif

/*
 *	Define the Stallion PCI vendor and device IDs.
 */
#ifndef PCI_DEVICE_ID_ECRA
#define	PCI_DEVICE_ID_ECRA		0x0004
#endif

static struct pci_device_id istallion_pci_tbl[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_STALLION, PCI_DEVICE_ID_ECRA), },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, istallion_pci_tbl);

static struct pci_driver stli_pcidriver;

/*****************************************************************************/

/*
 *	Hardware configuration info for ECP boards. These defines apply
 *	to the directly accessible io ports of the ECP. There is a set of
 *	defines for each ECP board type, ISA, EISA, MCA and PCI.
 */
#define	ECP_IOSIZE	4

#define	ECP_MEMSIZE	(128 * 1024)
#define	ECP_PCIMEMSIZE	(256 * 1024)

#define	ECP_ATPAGESIZE	(4 * 1024)
#define	ECP_MCPAGESIZE	(4 * 1024)
#define	ECP_EIPAGESIZE	(64 * 1024)
#define	ECP_PCIPAGESIZE	(64 * 1024)

#define	STL_EISAID	0x8c4e

/*
 *	Important defines for the ISA class of ECP board.
 */
#define	ECP_ATIREG	0
#define	ECP_ATCONFR	1
#define	ECP_ATMEMAR	2
#define	ECP_ATMEMPR	3
#define	ECP_ATSTOP	0x1
#define	ECP_ATINTENAB	0x10
#define	ECP_ATENABLE	0x20
#define	ECP_ATDISABLE	0x00
#define	ECP_ATADDRMASK	0x3f000
#define	ECP_ATADDRSHFT	12

/*
 *	Important defines for the EISA class of ECP board.
 */
#define	ECP_EIIREG	0
#define	ECP_EIMEMARL	1
#define	ECP_EICONFR	2
#define	ECP_EIMEMARH	3
#define	ECP_EIENABLE	0x1
#define	ECP_EIDISABLE	0x0
#define	ECP_EISTOP	0x4
#define	ECP_EIEDGE	0x00
#define	ECP_EILEVEL	0x80
#define	ECP_EIADDRMASKL	0x00ff0000
#define	ECP_EIADDRSHFTL	16
#define	ECP_EIADDRMASKH	0xff000000
#define	ECP_EIADDRSHFTH	24
#define	ECP_EIBRDENAB	0xc84

#define	ECP_EISAID	0x4

/*
 *	Important defines for the Micro-channel class of ECP board.
 *	(It has a lot in common with the ISA boards.)
 */
#define	ECP_MCIREG	0
#define	ECP_MCCONFR	1
#define	ECP_MCSTOP	0x20
#define	ECP_MCENABLE	0x80
#define	ECP_MCDISABLE	0x00

/*
 *	Important defines for the PCI class of ECP board.
 *	(It has a lot in common with the other ECP boards.)
 */
#define	ECP_PCIIREG	0
#define	ECP_PCICONFR	1
#define	ECP_PCISTOP	0x01

/*
 *	Hardware configuration info for ONboard and Brumby boards. These
 *	defines apply to the directly accessible io ports of these boards.
 */
#define	ONB_IOSIZE	16
#define	ONB_MEMSIZE	(64 * 1024)
#define	ONB_ATPAGESIZE	(64 * 1024)
#define	ONB_MCPAGESIZE	(64 * 1024)
#define	ONB_EIMEMSIZE	(128 * 1024)
#define	ONB_EIPAGESIZE	(64 * 1024)

/*
 *	Important defines for the ISA class of ONboard board.
 */
#define	ONB_ATIREG	0
#define	ONB_ATMEMAR	1
#define	ONB_ATCONFR	2
#define	ONB_ATSTOP	0x4
#define	ONB_ATENABLE	0x01
#define	ONB_ATDISABLE	0x00
#define	ONB_ATADDRMASK	0xff0000
#define	ONB_ATADDRSHFT	16

#define	ONB_MEMENABLO	0
#define	ONB_MEMENABHI	0x02

/*
 *	Important defines for the EISA class of ONboard board.
 */
#define	ONB_EIIREG	0
#define	ONB_EIMEMARL	1
#define	ONB_EICONFR	2
#define	ONB_EIMEMARH	3
#define	ONB_EIENABLE	0x1
#define	ONB_EIDISABLE	0x0
#define	ONB_EISTOP	0x4
#define	ONB_EIEDGE	0x00
#define	ONB_EILEVEL	0x80
#define	ONB_EIADDRMASKL	0x00ff0000
#define	ONB_EIADDRSHFTL	16
#define	ONB_EIADDRMASKH	0xff000000
#define	ONB_EIADDRSHFTH	24
#define	ONB_EIBRDENAB	0xc84

#define	ONB_EISAID	0x1

/*
 *	Important defines for the Brumby boards. They are pretty simple,
 *	there is not much that is programmably configurable.
 */
#define	BBY_IOSIZE	16
#define	BBY_MEMSIZE	(64 * 1024)
#define	BBY_PAGESIZE	(16 * 1024)

#define	BBY_ATIREG	0
#define	BBY_ATCONFR	1
#define	BBY_ATSTOP	0x4

/*
 *	Important defines for the Stallion boards. They are pretty simple,
 *	there is not much that is programmably configurable.
 */
#define	STAL_IOSIZE	16
#define	STAL_MEMSIZE	(64 * 1024)
#define	STAL_PAGESIZE	(64 * 1024)

/*
 *	Define the set of status register values for EasyConnection panels.
 *	The signature will return with the status value for each panel. From
 *	this we can determine what is attached to the board - before we have
 *	actually down loaded any code to it.
 */
#define	ECH_PNLSTATUS	2
#define	ECH_PNL16PORT	0x20
#define	ECH_PNLIDMASK	0x07
#define	ECH_PNLXPID	0x40
#define	ECH_PNLINTRPEND	0x80

/*
 *	Define some macros to do things to the board. Even those these boards
 *	are somewhat related there is often significantly different ways of
 *	doing some operation on it (like enable, paging, reset, etc). So each
 *	board class has a set of functions which do the commonly required
 *	operations. The macros below basically just call these functions,
 *	generally checking for a NULL function - which means that the board
 *	needs nothing done to it to achieve this operation!
 */
#define	EBRDINIT(brdp)						\
	if (brdp->init != NULL)					\
		(* brdp->init)(brdp)

#define	EBRDENABLE(brdp)					\
	if (brdp->enable != NULL)				\
		(* brdp->enable)(brdp);

#define	EBRDDISABLE(brdp)					\
	if (brdp->disable != NULL)				\
		(* brdp->disable)(brdp);

#define	EBRDINTR(brdp)						\
	if (brdp->intr != NULL)					\
		(* brdp->intr)(brdp);

#define	EBRDRESET(brdp)						\
	if (brdp->reset != NULL)				\
		(* brdp->reset)(brdp);

#define	EBRDGETMEMPTR(brdp,offset)				\
	(* brdp->getmemptr)(brdp, offset, __LINE__)

/*
 *	Define the maximal baud rate, and the default baud base for ports.
 */
#define	STL_MAXBAUD	460800
#define	STL_BAUDBASE	115200
#define	STL_CLOSEDELAY	(5 * HZ / 10)

/*****************************************************************************/

/*
 *	Define macros to extract a brd or port number from a minor number.
 */
#define	MINOR2BRD(min)		(((min) & 0xc0) >> 6)
#define	MINOR2PORT(min)		((min) & 0x3f)

/*****************************************************************************/

/*
 *	Prototype all functions in this driver!
 */

static int	stli_parsebrd(struct stlconf *confp, char **argp);
static int	stli_open(struct tty_struct *tty, struct file *filp);
static void	stli_close(struct tty_struct *tty, struct file *filp);
static int	stli_write(struct tty_struct *tty, const unsigned char *buf, int count);
static int	stli_putchar(struct tty_struct *tty, unsigned char ch);
static void	stli_flushchars(struct tty_struct *tty);
static int	stli_writeroom(struct tty_struct *tty);
static int	stli_charsinbuffer(struct tty_struct *tty);
static int	stli_ioctl(struct tty_struct *tty, struct file *file, unsigned int cmd, unsigned long arg);
static void	stli_settermios(struct tty_struct *tty, struct ktermios *old);
static void	stli_throttle(struct tty_struct *tty);
static void	stli_unthrottle(struct tty_struct *tty);
static void	stli_stop(struct tty_struct *tty);
static void	stli_start(struct tty_struct *tty);
static void	stli_flushbuffer(struct tty_struct *tty);
static int	stli_breakctl(struct tty_struct *tty, int state);
static void	stli_waituntilsent(struct tty_struct *tty, int timeout);
static void	stli_sendxchar(struct tty_struct *tty, char ch);
static void	stli_hangup(struct tty_struct *tty);

static int	stli_brdinit(struct stlibrd *brdp);
static int	stli_startbrd(struct stlibrd *brdp);
static ssize_t	stli_memread(struct file *fp, char __user *buf, size_t count, loff_t *offp);
static ssize_t	stli_memwrite(struct file *fp, const char __user *buf, size_t count, loff_t *offp);
static int	stli_memioctl(struct inode *ip, struct file *fp, unsigned int cmd, unsigned long arg);
static void	stli_brdpoll(struct stlibrd *brdp, cdkhdr_t __iomem *hdrp);
static void	stli_poll(unsigned long arg);
static int	stli_hostcmd(struct stlibrd *brdp, struct stliport *portp);
static int	stli_initopen(struct tty_struct *tty, struct stlibrd *brdp, struct stliport *portp);
static int	stli_rawopen(struct stlibrd *brdp, struct stliport *portp, unsigned long arg, int wait);
static int	stli_rawclose(struct stlibrd *brdp, struct stliport *portp, unsigned long arg, int wait);
static int	stli_setport(struct tty_struct *tty);
static int	stli_cmdwait(struct stlibrd *brdp, struct stliport *portp, unsigned long cmd, void *arg, int size, int copyback);
static void	stli_sendcmd(struct stlibrd *brdp, struct stliport *portp, unsigned long cmd, void *arg, int size, int copyback);
static void	__stli_sendcmd(struct stlibrd *brdp, struct stliport *portp, unsigned long cmd, void *arg, int size, int copyback);
static void	stli_dodelaycmd(struct stliport *portp, cdkctrl_t __iomem *cp);
static void	stli_mkasyport(struct tty_struct *tty, struct stliport *portp, asyport_t *pp, struct ktermios *tiosp);
static void	stli_mkasysigs(asysigs_t *sp, int dtr, int rts);
static long	stli_mktiocm(unsigned long sigvalue);
static void	stli_read(struct stlibrd *brdp, struct stliport *portp);
static int	stli_getserial(struct stliport *portp, struct serial_struct __user *sp);
static int	stli_setserial(struct tty_struct *tty, struct serial_struct __user *sp);
static int	stli_getbrdstats(combrd_t __user *bp);
static int	stli_getportstats(struct tty_struct *tty, struct stliport *portp, comstats_t __user *cp);
static int	stli_portcmdstats(struct tty_struct *tty, struct stliport *portp);
static int	stli_clrportstats(struct stliport *portp, comstats_t __user *cp);
static int	stli_getportstruct(struct stliport __user *arg);
static int	stli_getbrdstruct(struct stlibrd __user *arg);
static struct stlibrd *stli_allocbrd(void);

static void	stli_ecpinit(struct stlibrd *brdp);
static void	stli_ecpenable(struct stlibrd *brdp);
static void	stli_ecpdisable(struct stlibrd *brdp);
static void __iomem *stli_ecpgetmemptr(struct stlibrd *brdp, unsigned long offset, int line);
static void	stli_ecpreset(struct stlibrd *brdp);
static void	stli_ecpintr(struct stlibrd *brdp);
static void	stli_ecpeiinit(struct stlibrd *brdp);
static void	stli_ecpeienable(struct stlibrd *brdp);
static void	stli_ecpeidisable(struct stlibrd *brdp);
static void __iomem *stli_ecpeigetmemptr(struct stlibrd *brdp, unsigned long offset, int line);
static void	stli_ecpeireset(struct stlibrd *brdp);
static void	stli_ecpmcenable(struct stlibrd *brdp);
static void	stli_ecpmcdisable(struct stlibrd *brdp);
static void __iomem *stli_ecpmcgetmemptr(struct stlibrd *brdp, unsigned long offset, int line);
static void	stli_ecpmcreset(struct stlibrd *brdp);
static void	stli_ecppciinit(struct stlibrd *brdp);
static void __iomem *stli_ecppcigetmemptr(struct stlibrd *brdp, unsigned long offset, int line);
static void	stli_ecppcireset(struct stlibrd *brdp);

static void	stli_onbinit(struct stlibrd *brdp);
static void	stli_onbenable(struct stlibrd *brdp);
static void	stli_onbdisable(struct stlibrd *brdp);
static void __iomem *stli_onbgetmemptr(struct stlibrd *brdp, unsigned long offset, int line);
static void	stli_onbreset(struct stlibrd *brdp);
static void	stli_onbeinit(struct stlibrd *brdp);
static void	stli_onbeenable(struct stlibrd *brdp);
static void	stli_onbedisable(struct stlibrd *brdp);
static void __iomem *stli_onbegetmemptr(struct stlibrd *brdp, unsigned long offset, int line);
static void	stli_onbereset(struct stlibrd *brdp);
static void	stli_bbyinit(struct stlibrd *brdp);
static void __iomem *stli_bbygetmemptr(struct stlibrd *brdp, unsigned long offset, int line);
static void	stli_bbyreset(struct stlibrd *brdp);
static void	stli_stalinit(struct stlibrd *brdp);
static void __iomem *stli_stalgetmemptr(struct stlibrd *brdp, unsigned long offset, int line);
static void	stli_stalreset(struct stlibrd *brdp);

static struct stliport *stli_getport(unsigned int brdnr, unsigned int panelnr, unsigned int portnr);

static int	stli_initecp(struct stlibrd *brdp);
static int	stli_initonb(struct stlibrd *brdp);
#if STLI_EISAPROBE != 0
static int	stli_eisamemprobe(struct stlibrd *brdp);
#endif
static int	stli_initports(struct stlibrd *brdp);

/*****************************************************************************/

/*
 *	Define the driver info for a user level shared memory device. This
 *	device will work sort of like the /dev/kmem device - except that it
 *	will give access to the shared memory on the Stallion intelligent
 *	board. This is also a very useful debugging tool.
 */
static const struct file_operations	stli_fsiomem = {
	.owner		= THIS_MODULE,
	.read		= stli_memread,
	.write		= stli_memwrite,
	.ioctl		= stli_memioctl,
};

/*****************************************************************************/

/*
 *	Define a timer_list entry for our poll routine. The slave board
 *	is polled every so often to see if anything needs doing. This is
 *	much cheaper on host cpu than using interrupts. It turns out to
 *	not increase character latency by much either...
 */
static DEFINE_TIMER(stli_timerlist, stli_poll, 0, 0);

static int	stli_timeron;

/*
 *	Define the calculation for the timeout routine.
 */
#define	STLI_TIMEOUT	(jiffies + 1)

/*****************************************************************************/

static struct class *istallion_class;

static void stli_cleanup_ports(struct stlibrd *brdp)
{
	struct stliport *portp;
	unsigned int j;
	struct tty_struct *tty;

	for (j = 0; j < STL_MAXPORTS; j++) {
		portp = brdp->ports[j];
		if (portp != NULL) {
			tty = tty_port_tty_get(&portp->port);
			if (tty != NULL) {
				tty_hangup(tty);
				tty_kref_put(tty);
			}
			kfree(portp);
		}
	}
}

/*****************************************************************************/

/*
 *	Parse the supplied argument string, into the board conf struct.
 */

static int stli_parsebrd(struct stlconf *confp, char **argp)
{
	unsigned int i;
	char *sp;

	if (argp[0] == NULL || *argp[0] == 0)
		return 0;

	for (sp = argp[0], i = 0; ((*sp != 0) && (i < 25)); sp++, i++)
		*sp = tolower(*sp);

	for (i = 0; i < ARRAY_SIZE(stli_brdstr); i++) {
		if (strcmp(stli_brdstr[i].name, argp[0]) == 0)
			break;
	}
	if (i == ARRAY_SIZE(stli_brdstr)) {
		printk(KERN_WARNING "istallion: unknown board name, %s?\n", argp[0]);
		return 0;
	}

	confp->brdtype = stli_brdstr[i].type;
	if (argp[1] != NULL && *argp[1] != 0)
		confp->ioaddr1 = simple_strtoul(argp[1], NULL, 0);
	if (argp[2] !=  NULL && *argp[2] != 0)
		confp->memaddr = simple_strtoul(argp[2], NULL, 0);
	return(1);
}

/*****************************************************************************/

static int stli_open(struct tty_struct *tty, struct file *filp)
{
	struct stlibrd *brdp;
	struct stliport *portp;
	struct tty_port *port;
	unsigned int minordev, brdnr, portnr;
	int rc;

	minordev = tty->index;
	brdnr = MINOR2BRD(minordev);
	if (brdnr >= stli_nrbrds)
		return -ENODEV;
	brdp = stli_brds[brdnr];
	if (brdp == NULL)
		return -ENODEV;
	if ((brdp->state & BST_STARTED) == 0)
		return -ENODEV;
	portnr = MINOR2PORT(minordev);
	if (portnr > brdp->nrports)
		return -ENODEV;

	portp = brdp->ports[portnr];
	if (portp == NULL)
		return -ENODEV;
	if (portp->devnr < 1)
		return -ENODEV;
	port = &portp->port;

/*
 *	On the first open of the device setup the port hardware, and
 *	initialize the per port data structure. Since initializing the port
 *	requires several commands to the board we will need to wait for any
 *	other open that is already initializing the port.
 *
 *	Review - locking
 */
	tty_port_tty_set(port, tty);
	tty->driver_data = portp;
	port->count++;

	wait_event_interruptible(portp->raw_wait,
			!test_bit(ST_INITIALIZING, &portp->state));
	if (signal_pending(current))
		return -ERESTARTSYS;

	if ((portp->port.flags & ASYNC_INITIALIZED) == 0) {
		set_bit(ST_INITIALIZING, &portp->state);
		if ((rc = stli_initopen(tty, brdp, portp)) >= 0) {
			/* Locking */
			port->flags |= ASYNC_INITIALIZED;
			clear_bit(TTY_IO_ERROR, &tty->flags);
		}
		clear_bit(ST_INITIALIZING, &portp->state);
		wake_up_interruptible(&portp->raw_wait);
		if (rc < 0)
			return rc;
	}
	return tty_port_block_til_ready(&portp->port, tty, filp);
}

/*****************************************************************************/

static void stli_close(struct tty_struct *tty, struct file *filp)
{
	struct stlibrd *brdp;
	struct stliport *portp;
	struct tty_port *port;
	unsigned long flags;

	portp = tty->driver_data;
	if (portp == NULL)
		return;
	port = &portp->port;

	if (tty_port_close_start(port, tty, filp) == 0)
		return;

/*
 *	May want to wait for data to drain before closing. The BUSY flag
 *	keeps track of whether we are still transmitting or not. It is
 *	updated by messages from the slave - indicating when all chars
 *	really have drained.
 */
 	spin_lock_irqsave(&stli_lock, flags);
	if (tty == stli_txcooktty)
		stli_flushchars(tty);
	spin_unlock_irqrestore(&stli_lock, flags);

	/* We end up doing this twice for the moment. This needs looking at
	   eventually. Note we still use portp->closing_wait as a result */
	if (portp->closing_wait != ASYNC_CLOSING_WAIT_NONE)
		tty_wait_until_sent(tty, portp->closing_wait);

	/* FIXME: port locking here needs attending to */
	port->flags &= ~ASYNC_INITIALIZED;

	brdp = stli_brds[portp->brdnr];
	stli_rawclose(brdp, portp, 0, 0);
	if (tty->termios->c_cflag & HUPCL) {
		stli_mkasysigs(&portp->asig, 0, 0);
		if (test_bit(ST_CMDING, &portp->state))
			set_bit(ST_DOSIGS, &portp->state);
		else
			stli_sendcmd(brdp, portp, A_SETSIGNALS, &portp->asig,
				sizeof(asysigs_t), 0);
	}
	clear_bit(ST_TXBUSY, &portp->state);
	clear_bit(ST_RXSTOP, &portp->state);
	set_bit(TTY_IO_ERROR, &tty->flags);
	tty_ldisc_flush(tty);
	set_bit(ST_DOFLUSHRX, &portp->state);
	stli_flushbuffer(tty);

	tty_port_close_end(port, tty);
	tty_port_tty_set(port, NULL);
}

/*****************************************************************************/

/*
 *	Carry out first open operations on a port. This involves a number of
 *	commands to be sent to the slave. We need to open the port, set the
 *	notification events, set the initial port settings, get and set the
 *	initial signal values. We sleep and wait in between each one. But
 *	this still all happens pretty quickly.
 */

static int stli_initopen(struct tty_struct *tty,
				struct stlibrd *brdp, struct stliport *portp)
{
	asynotify_t nt;
	asyport_t aport;
	int rc;

	if ((rc = stli_rawopen(brdp, portp, 0, 1)) < 0)
		return rc;

	memset(&nt, 0, sizeof(asynotify_t));
	nt.data = (DT_TXLOW | DT_TXEMPTY | DT_RXBUSY | DT_RXBREAK);
	nt.signal = SG_DCD;
	if ((rc = stli_cmdwait(brdp, portp, A_SETNOTIFY, &nt,
	    sizeof(asynotify_t), 0)) < 0)
		return rc;

	stli_mkasyport(tty, portp, &aport, tty->termios);
	if ((rc = stli_cmdwait(brdp, portp, A_SETPORT, &aport,
	    sizeof(asyport_t), 0)) < 0)
		return rc;

	set_bit(ST_GETSIGS, &portp->state);
	if ((rc = stli_cmdwait(brdp, portp, A_GETSIGNALS, &portp->asig,
	    sizeof(asysigs_t), 1)) < 0)
		return rc;
	if (test_and_clear_bit(ST_GETSIGS, &portp->state))
		portp->sigs = stli_mktiocm(portp->asig.sigvalue);
	stli_mkasysigs(&portp->asig, 1, 1);
	if ((rc = stli_cmdwait(brdp, portp, A_SETSIGNALS, &portp->asig,
	    sizeof(asysigs_t), 0)) < 0)
		return rc;

	return 0;
}

/*****************************************************************************/

/*
 *	Send an open message to the slave. This will sleep waiting for the
 *	acknowledgement, so must have user context. We need to co-ordinate
 *	with close events here, since we don't want open and close events
 *	to overlap.
 */

static int stli_rawopen(struct stlibrd *brdp, struct stliport *portp, unsigned long arg, int wait)
{
	cdkhdr_t __iomem *hdrp;
	cdkctrl_t __iomem *cp;
	unsigned char __iomem *bits;
	unsigned long flags;
	int rc;

/*
 *	Send a message to the slave to open this port.
 */

/*
 *	Slave is already closing this port. This can happen if a hangup
 *	occurs on this port. So we must wait until it is complete. The
 *	order of opens and closes may not be preserved across shared
 *	memory, so we must wait until it is complete.
 */
	wait_event_interruptible(portp->raw_wait,
			!test_bit(ST_CLOSING, &portp->state));
	if (signal_pending(current)) {
		return -ERESTARTSYS;
	}

/*
 *	Everything is ready now, so write the open message into shared
 *	memory. Once the message is in set the service bits to say that
 *	this port wants service.
 */
	spin_lock_irqsave(&brd_lock, flags);
	EBRDENABLE(brdp);
	cp = &((cdkasy_t __iomem *) EBRDGETMEMPTR(brdp, portp->addr))->ctrl;
	writel(arg, &cp->openarg);
	writeb(1, &cp->open);
	hdrp = (cdkhdr_t __iomem *) EBRDGETMEMPTR(brdp, CDK_CDKADDR);
	bits = ((unsigned char __iomem *) hdrp) + brdp->slaveoffset +
		portp->portidx;
	writeb(readb(bits) | portp->portbit, bits);
	EBRDDISABLE(brdp);

	if (wait == 0) {
		spin_unlock_irqrestore(&brd_lock, flags);
		return 0;
	}

/*
 *	Slave is in action, so now we must wait for the open acknowledgment
 *	to come back.
 */
	rc = 0;
	set_bit(ST_OPENING, &portp->state);
	spin_unlock_irqrestore(&brd_lock, flags);

	wait_event_interruptible(portp->raw_wait,
			!test_bit(ST_OPENING, &portp->state));
	if (signal_pending(current))
		rc = -ERESTARTSYS;

	if ((rc == 0) && (portp->rc != 0))
		rc = -EIO;
	return rc;
}

/*****************************************************************************/

/*
 *	Send a close message to the slave. Normally this will sleep waiting
 *	for the acknowledgement, but if wait parameter is 0 it will not. If
 *	wait is true then must have user context (to sleep).
 */

static int stli_rawclose(struct stlibrd *brdp, struct stliport *portp, unsigned long arg, int wait)
{
	cdkhdr_t __iomem *hdrp;
	cdkctrl_t __iomem *cp;
	unsigned char __iomem *bits;
	unsigned long flags;
	int rc;

/*
 *	Slave is already closing this port. This can happen if a hangup
 *	occurs on this port.
 */
	if (wait) {
		wait_event_interruptible(portp->raw_wait,
				!test_bit(ST_CLOSING, &portp->state));
		if (signal_pending(current)) {
			return -ERESTARTSYS;
		}
	}

/*
 *	Write the close command into shared memory.
 */
	spin_lock_irqsave(&brd_lock, flags);
	EBRDENABLE(brdp);
	cp = &((cdkasy_t __iomem *) EBRDGETMEMPTR(brdp, portp->addr))->ctrl;
	writel(arg, &cp->closearg);
	writeb(1, &cp->close);
	hdrp = (cdkhdr_t __iomem *) EBRDGETMEMPTR(brdp, CDK_CDKADDR);
	bits = ((unsigned char __iomem *) hdrp) + brdp->slaveoffset +
		portp->portidx;
	writeb(readb(bits) |portp->portbit, bits);
	EBRDDISABLE(brdp);

	set_bit(ST_CLOSING, &portp->state);
	spin_unlock_irqrestore(&brd_lock, flags);

	if (wait == 0)
		return 0;

/*
 *	Slave is in action, so now we must wait for the open acknowledgment
 *	to come back.
 */
	rc = 0;
	wait_event_interruptible(portp->raw_wait,
			!test_bit(ST_CLOSING, &portp->state));
	if (signal_pending(current))
		rc = -ERESTARTSYS;

	if ((rc == 0) && (portp->rc != 0))
		rc = -EIO;
	return rc;
}

/*****************************************************************************/

/*
 *	Send a command to the slave and wait for the response. This must
 *	have user context (it sleeps). This routine is generic in that it
 *	can send any type of command. Its purpose is to wait for that command
 *	to complete (as opposed to initiating the command then returning).
 */

static int stli_cmdwait(struct stlibrd *brdp, struct stliport *portp, unsigned long cmd, void *arg, int size, int copyback)
{
	wait_event_interruptible(portp->raw_wait,
			!test_bit(ST_CMDING, &portp->state));
	if (signal_pending(current))
		return -ERESTARTSYS;

	stli_sendcmd(brdp, portp, cmd, arg, size, copyback);

	wait_event_interruptible(portp->raw_wait,
			!test_bit(ST_CMDING, &portp->state));
	if (signal_pending(current))
		return -ERESTARTSYS;

	if (portp->rc != 0)
		return -EIO;
	return 0;
}

/*****************************************************************************/

/*
 *	Send the termios settings for this port to the slave. This sleeps
 *	waiting for the command to complete - so must have user context.
 */

static int stli_setport(struct tty_struct *tty)
{
	struct stliport *portp = tty->driver_data;
	struct stlibrd *brdp;
	asyport_t aport;

	if (portp == NULL)
		return -ENODEV;
	if (portp->brdnr >= stli_nrbrds)
		return -ENODEV;
	brdp = stli_brds[portp->brdnr];
	if (brdp == NULL)
		return -ENODEV;

	stli_mkasyport(tty, portp, &aport, tty->termios);
	return(stli_cmdwait(brdp, portp, A_SETPORT, &aport, sizeof(asyport_t), 0));
}

/*****************************************************************************/

static int stli_carrier_raised(struct tty_port *port)
{
	struct stliport *portp = container_of(port, struct stliport, port);
	return (portp->sigs & TIOCM_CD) ? 1 : 0;
}

static void stli_dtr_rts(struct tty_port *port, int on)
{
	struct stliport *portp = container_of(port, struct stliport, port);
	struct stlibrd *brdp = stli_brds[portp->brdnr];
	stli_mkasysigs(&portp->asig, on, on);
	if (stli_cmdwait(brdp, portp, A_SETSIGNALS, &portp->asig,
		sizeof(asysigs_t), 0) < 0)
			printk(KERN_WARNING "istallion: dtr set failed.\n");
}


/*****************************************************************************/

/*
 *	Write routine. Take the data and put it in the shared memory ring
 *	queue. If port is not already sending chars then need to mark the
 *	service bits for this port.
 */

static int stli_write(struct tty_struct *tty, const unsigned char *buf, int count)
{
	cdkasy_t __iomem *ap;
	cdkhdr_t __iomem *hdrp;
	unsigned char __iomem *bits;
	unsigned char __iomem *shbuf;
	unsigned char *chbuf;
	struct stliport *portp;
	struct stlibrd *brdp;
	unsigned int len, stlen, head, tail, size;
	unsigned long flags;

	if (tty == stli_txcooktty)
		stli_flushchars(tty);
	portp = tty->driver_data;
	if (portp == NULL)
		return 0;
	if (portp->brdnr >= stli_nrbrds)
		return 0;
	brdp = stli_brds[portp->brdnr];
	if (brdp == NULL)
		return 0;
	chbuf = (unsigned char *) buf;

/*
 *	All data is now local, shove as much as possible into shared memory.
 */
	spin_lock_irqsave(&brd_lock, flags);
	EBRDENABLE(brdp);
	ap = (cdkasy_t __iomem *) EBRDGETMEMPTR(brdp, portp->addr);
	head = (unsigned int) readw(&ap->txq.head);
	tail = (unsigned int) readw(&ap->txq.tail);
	if (tail != ((unsigned int) readw(&ap->txq.tail)))
		tail = (unsigned int) readw(&ap->txq.tail);
	size = portp->txsize;
	if (head >= tail) {
		len = size - (head - tail) - 1;
		stlen = size - head;
	} else {
		len = tail - head - 1;
		stlen = len;
	}

	len = min(len, (unsigned int)count);
	count = 0;
	shbuf = (char __iomem *) EBRDGETMEMPTR(brdp, portp->txoffset);

	while (len > 0) {
		stlen = min(len, stlen);
		memcpy_toio(shbuf + head, chbuf, stlen);
		chbuf += stlen;
		len -= stlen;
		count += stlen;
		head += stlen;
		if (head >= size) {
			head = 0;
			stlen = tail;
		}
	}

	ap = (cdkasy_t __iomem *) EBRDGETMEMPTR(brdp, portp->addr);
	writew(head, &ap->txq.head);
	if (test_bit(ST_TXBUSY, &portp->state)) {
		if (readl(&ap->changed.data) & DT_TXEMPTY)
			writel(readl(&ap->changed.data) & ~DT_TXEMPTY, &ap->changed.data);
	}
	hdrp = (cdkhdr_t __iomem *) EBRDGETMEMPTR(brdp, CDK_CDKADDR);
	bits = ((unsigned char __iomem *) hdrp) + brdp->slaveoffset +
		portp->portidx;
	writeb(readb(bits) | portp->portbit, bits);
	set_bit(ST_TXBUSY, &portp->state);
	EBRDDISABLE(brdp);
	spin_unlock_irqrestore(&brd_lock, flags);

	return(count);
}

/*****************************************************************************/

/*
 *	Output a single character. We put it into a temporary local buffer
 *	(for speed) then write out that buffer when the flushchars routine
 *	is called. There is a safety catch here so that if some other port
 *	writes chars before the current buffer has been, then we write them
 *	first them do the new ports.
 */

static int stli_putchar(struct tty_struct *tty, unsigned char ch)
{
	if (tty != stli_txcooktty) {
		if (stli_txcooktty != NULL)
			stli_flushchars(stli_txcooktty);
		stli_txcooktty = tty;
	}

	stli_txcookbuf[stli_txcooksize++] = ch;
	return 0;
}

/*****************************************************************************/

/*
 *	Transfer characters from the local TX cooking buffer to the board.
 *	We sort of ignore the tty that gets passed in here. We rely on the
 *	info stored with the TX cook buffer to tell us which port to flush
 *	the data on. In any case we clean out the TX cook buffer, for re-use
 *	by someone else.
 */

static void stli_flushchars(struct tty_struct *tty)
{
	cdkhdr_t __iomem *hdrp;
	unsigned char __iomem *bits;
	cdkasy_t __iomem *ap;
	struct tty_struct *cooktty;
	struct stliport *portp;
	struct stlibrd *brdp;
	unsigned int len, stlen, head, tail, size, count, cooksize;
	unsigned char *buf;
	unsigned char __iomem *shbuf;
	unsigned long flags;

	cooksize = stli_txcooksize;
	cooktty = stli_txcooktty;
	stli_txcooksize = 0;
	stli_txcookrealsize = 0;
	stli_txcooktty = NULL;

	if (cooktty == NULL)
		return;
	if (tty != cooktty)
		tty = cooktty;
	if (cooksize == 0)
		return;

	portp = tty->driver_data;
	if (portp == NULL)
		return;
	if (portp->brdnr >= stli_nrbrds)
		return;
	brdp = stli_brds[portp->brdnr];
	if (brdp == NULL)
		return;

	spin_lock_irqsave(&brd_lock, flags);
	EBRDENABLE(brdp);

	ap = (cdkasy_t __iomem *) EBRDGETMEMPTR(brdp, portp->addr);
	head = (unsigned int) readw(&ap->txq.head);
	tail = (unsigned int) readw(&ap->txq.tail);
	if (tail != ((unsigned int) readw(&ap->txq.tail)))
		tail = (unsigned int) readw(&ap->txq.tail);
	size = portp->txsize;
	if (head >= tail) {
		len = size - (head - tail) - 1;
		stlen = size - head;
	} else {
		len = tail - head - 1;
		stlen = len;
	}

	len = min(len, cooksize);
	count = 0;
	shbuf = EBRDGETMEMPTR(brdp, portp->txoffset);
	buf = stli_txcookbuf;

	while (len > 0) {
		stlen = min(len, stlen);
		memcpy_toio(shbuf + head, buf, stlen);
		buf += stlen;
		len -= stlen;
		count += stlen;
		head += stlen;
		if (head >= size) {
			head = 0;
			stlen = tail;
		}
	}

	ap = (cdkasy_t __iomem *) EBRDGETMEMPTR(brdp, portp->addr);
	writew(head, &ap->txq.head);

	if (test_bit(ST_TXBUSY, &portp->state)) {
		if (readl(&ap->changed.data) & DT_TXEMPTY)
			writel(readl(&ap->changed.data) & ~DT_TXEMPTY, &ap->changed.data);
	}
	hdrp = (cdkhdr_t __iomem *) EBRDGETMEMPTR(brdp, CDK_CDKADDR);
	bits = ((unsigned char __iomem *) hdrp) + brdp->slaveoffset +
		portp->portidx;
	writeb(readb(bits) | portp->portbit, bits);
	set_bit(ST_TXBUSY, &portp->state);

	EBRDDISABLE(brdp);
	spin_unlock_irqrestore(&brd_lock, flags);
}

/*****************************************************************************/

static int stli_writeroom(struct tty_struct *tty)
{
	cdkasyrq_t __iomem *rp;
	struct stliport *portp;
	struct stlibrd *brdp;
	unsigned int head, tail, len;
	unsigned long flags;

	if (tty == stli_txcooktty) {
		if (stli_txcookrealsize != 0) {
			len = stli_txcookrealsize - stli_txcooksize;
			return len;
		}
	}

	portp = tty->driver_data;
	if (portp == NULL)
		return 0;
	if (portp->brdnr >= stli_nrbrds)
		return 0;
	brdp = stli_brds[portp->brdnr];
	if (brdp == NULL)
		return 0;

	spin_lock_irqsave(&brd_lock, flags);
	EBRDENABLE(brdp);
	rp = &((cdkasy_t __iomem *) EBRDGETMEMPTR(brdp, portp->addr))->txq;
	head = (unsigned int) readw(&rp->head);
	tail = (unsigned int) readw(&rp->tail);
	if (tail != ((unsigned int) readw(&rp->tail)))
		tail = (unsigned int) readw(&rp->tail);
	len = (head >= tail) ? (portp->txsize - (head - tail)) : (tail - head);
	len--;
	EBRDDISABLE(brdp);
	spin_unlock_irqrestore(&brd_lock, flags);

	if (tty == stli_txcooktty) {
		stli_txcookrealsize = len;
		len -= stli_txcooksize;
	}
	return len;
}

/*****************************************************************************/

/*
 *	Return the number of characters in the transmit buffer. Normally we
 *	will return the number of chars in the shared memory ring queue.
 *	We need to kludge around the case where the shared memory buffer is
 *	empty but not all characters have drained yet, for this case just
 *	return that there is 1 character in the buffer!
 */

static int stli_charsinbuffer(struct tty_struct *tty)
{
	cdkasyrq_t __iomem *rp;
	struct stliport *portp;
	struct stlibrd *brdp;
	unsigned int head, tail, len;
	unsigned long flags;

	if (tty == stli_txcooktty)
		stli_flushchars(tty);
	portp = tty->driver_data;
	if (portp == NULL)
		return 0;
	if (portp->brdnr >= stli_nrbrds)
		return 0;
	brdp = stli_brds[portp->brdnr];
	if (brdp == NULL)
		return 0;

	spin_lock_irqsave(&brd_lock, flags);
	EBRDENABLE(brdp);
	rp = &((cdkasy_t __iomem *) EBRDGETMEMPTR(brdp, portp->addr))->txq;
	head = (unsigned int) readw(&rp->head);
	tail = (unsigned int) readw(&rp->tail);
	if (tail != ((unsigned int) readw(&rp->tail)))
		tail = (unsigned int) readw(&rp->tail);
	len = (head >= tail) ? (head - tail) : (portp->txsize - (tail - head));
	if ((len == 0) && test_bit(ST_TXBUSY, &portp->state))
		len = 1;
	EBRDDISABLE(brdp);
	spin_unlock_irqrestore(&brd_lock, flags);

	return len;
}

/*****************************************************************************/

/*
 *	Generate the serial struct info.
 */

static int stli_getserial(struct stliport *portp, struct serial_struct __user *sp)
{
	struct serial_struct sio;
	struct stlibrd *brdp;

	memset(&sio, 0, sizeof(struct serial_struct));
	sio.type = PORT_UNKNOWN;
	sio.line = portp->portnr;
	sio.irq = 0;
	sio.flags = portp->port.flags;
	sio.baud_base = portp->baud_base;
	sio.close_delay = portp->port.close_delay;
	sio.closing_wait = portp->closing_wait;
	sio.custom_divisor = portp->custom_divisor;
	sio.xmit_fifo_size = 0;
	sio.hub6 = 0;

	brdp = stli_brds[portp->brdnr];
	if (brdp != NULL)
		sio.port = brdp->iobase;
		
	return copy_to_user(sp, &sio, sizeof(struct serial_struct)) ?
			-EFAULT : 0;
}

/*****************************************************************************/

/*
 *	Set port according to the serial struct info.
 *	At this point we do not do any auto-configure stuff, so we will
 *	just quietly ignore any requests to change irq, etc.
 */

static int stli_setserial(struct tty_struct *tty, struct serial_struct __user *sp)
{
	struct serial_struct sio;
	int rc;
	struct stliport *portp = tty->driver_data;

	if (copy_from_user(&sio, sp, sizeof(struct serial_struct)))
		return -EFAULT;
	if (!capable(CAP_SYS_ADMIN)) {
		if ((sio.baud_base != portp->baud_base) ||
		    (sio.close_delay != portp->port.close_delay) ||
		    ((sio.flags & ~ASYNC_USR_MASK) !=
		    (portp->port.flags & ~ASYNC_USR_MASK)))
			return -EPERM;
	} 

	portp->port.flags = (portp->port.flags & ~ASYNC_USR_MASK) |
		(sio.flags & ASYNC_USR_MASK);
	portp->baud_base = sio.baud_base;
	portp->port.close_delay = sio.close_delay;
	portp->closing_wait = sio.closing_wait;
	portp->custom_divisor = sio.custom_divisor;

	if ((rc = stli_setport(tty)) < 0)
		return rc;
	return 0;
}

/*****************************************************************************/

static int stli_tiocmget(struct tty_struct *tty, struct file *file)
{
	struct stliport *portp = tty->driver_data;
	struct stlibrd *brdp;
	int rc;

	if (portp == NULL)
		return -ENODEV;
	if (portp->brdnr >= stli_nrbrds)
		return 0;
	brdp = stli_brds[portp->brdnr];
	if (brdp == NULL)
		return 0;
	if (tty->flags & (1 << TTY_IO_ERROR))
		return -EIO;

	if ((rc = stli_cmdwait(brdp, portp, A_GETSIGNALS,
			       &portp->asig, sizeof(asysigs_t), 1)) < 0)
		return rc;

	return stli_mktiocm(portp->asig.sigvalue);
}

static int stli_tiocmset(struct tty_struct *tty, struct file *file,
			 unsigned int set, unsigned int clear)
{
	struct stliport *portp = tty->driver_data;
	struct stlibrd *brdp;
	int rts = -1, dtr = -1;

	if (portp == NULL)
		return -ENODEV;
	if (portp->brdnr >= stli_nrbrds)
		return 0;
	brdp = stli_brds[portp->brdnr];
	if (brdp == NULL)
		return 0;
	if (tty->flags & (1 << TTY_IO_ERROR))
		return -EIO;

	if (set & TIOCM_RTS)
		rts = 1;
	if (set & TIOCM_DTR)
		dtr = 1;
	if (clear & TIOCM_RTS)
		rts = 0;
	if (clear & TIOCM_DTR)
		dtr = 0;

	stli_mkasysigs(&portp->asig, dtr, rts);

	return stli_cmdwait(brdp, portp, A_SETSIGNALS, &portp->asig,
			    sizeof(asysigs_t), 0);
}

static int stli_ioctl(struct tty_struct *tty, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct stliport *portp;
	struct stlibrd *brdp;
	int rc;
	void __user *argp = (void __user *)arg;

	portp = tty->driver_data;
	if (portp == NULL)
		return -ENODEV;
	if (portp->brdnr >= stli_nrbrds)
		return 0;
	brdp = stli_brds[portp->brdnr];
	if (brdp == NULL)
		return 0;

	if ((cmd != TIOCGSERIAL) && (cmd != TIOCSSERIAL) &&
 	    (cmd != COM_GETPORTSTATS) && (cmd != COM_CLRPORTSTATS)) {
		if (tty->flags & (1 << TTY_IO_ERROR))
			return -EIO;
	}

	rc = 0;

	switch (cmd) {
	case TIOCGSERIAL:
		rc = stli_getserial(portp, argp);
		break;
	case TIOCSSERIAL:
		rc = stli_setserial(tty, argp);
		break;
	case STL_GETPFLAG:
		rc = put_user(portp->pflag, (unsigned __user *)argp);
		break;
	case STL_SETPFLAG:
		if ((rc = get_user(portp->pflag, (unsigned __user *)argp)) == 0)
			stli_setport(tty);
		break;
	case COM_GETPORTSTATS:
		rc = stli_getportstats(tty, portp, argp);
		break;
	case COM_CLRPORTSTATS:
		rc = stli_clrportstats(portp, argp);
		break;
	case TIOCSERCONFIG:
	case TIOCSERGWILD:
	case TIOCSERSWILD:
	case TIOCSERGETLSR:
	case TIOCSERGSTRUCT:
	case TIOCSERGETMULTI:
	case TIOCSERSETMULTI:
	default:
		rc = -ENOIOCTLCMD;
		break;
	}

	return rc;
}

/*****************************************************************************/

/*
 *	This routine assumes that we have user context and can sleep.
 *	Looks like it is true for the current ttys implementation..!!
 */

static void stli_settermios(struct tty_struct *tty, struct ktermios *old)
{
	struct stliport *portp;
	struct stlibrd *brdp;
	struct ktermios *tiosp;
	asyport_t aport;

	portp = tty->driver_data;
	if (portp == NULL)
		return;
	if (portp->brdnr >= stli_nrbrds)
		return;
	brdp = stli_brds[portp->brdnr];
	if (brdp == NULL)
		return;

	tiosp = tty->termios;

	stli_mkasyport(tty, portp, &aport, tiosp);
	stli_cmdwait(brdp, portp, A_SETPORT, &aport, sizeof(asyport_t), 0);
	stli_mkasysigs(&portp->asig, ((tiosp->c_cflag & CBAUD) ? 1 : 0), -1);
	stli_cmdwait(brdp, portp, A_SETSIGNALS, &portp->asig,
		sizeof(asysigs_t), 0);
	if ((old->c_cflag & CRTSCTS) && ((tiosp->c_cflag & CRTSCTS) == 0))
		tty->hw_stopped = 0;
	if (((old->c_cflag & CLOCAL) == 0) && (tiosp->c_cflag & CLOCAL))
		wake_up_interruptible(&portp->port.open_wait);
}

/*****************************************************************************/

/*
 *	Attempt to flow control who ever is sending us data. We won't really
 *	do any flow control action here. We can't directly, and even if we
 *	wanted to we would have to send a command to the slave. The slave
 *	knows how to flow control, and will do so when its buffers reach its
 *	internal high water marks. So what we will do is set a local state
 *	bit that will stop us sending any RX data up from the poll routine
 *	(which is the place where RX data from the slave is handled).
 */

static void stli_throttle(struct tty_struct *tty)
{
	struct stliport	*portp = tty->driver_data;
	if (portp == NULL)
		return;
	set_bit(ST_RXSTOP, &portp->state);
}

/*****************************************************************************/

/*
 *	Unflow control the device sending us data... That means that all
 *	we have to do is clear the RXSTOP state bit. The next poll call
 *	will then be able to pass the RX data back up.
 */

static void stli_unthrottle(struct tty_struct *tty)
{
	struct stliport	*portp = tty->driver_data;
	if (portp == NULL)
		return;
	clear_bit(ST_RXSTOP, &portp->state);
}

/*****************************************************************************/

/*
 *	Stop the transmitter.
 */

static void stli_stop(struct tty_struct *tty)
{
}

/*****************************************************************************/

/*
 *	Start the transmitter again.
 */

static void stli_start(struct tty_struct *tty)
{
}

/*****************************************************************************/

/*
 *	Hangup this port. This is pretty much like closing the port, only
 *	a little more brutal. No waiting for data to drain. Shutdown the
 *	port and maybe drop signals. This is rather tricky really. We want
 *	to close the port as well.
 */

static void stli_hangup(struct tty_struct *tty)
{
	struct stliport *portp;
	struct stlibrd *brdp;
	struct tty_port *port;
	unsigned long flags;

	portp = tty->driver_data;
	if (portp == NULL)
		return;
	if (portp->brdnr >= stli_nrbrds)
		return;
	brdp = stli_brds[portp->brdnr];
	if (brdp == NULL)
		return;
	port = &portp->port;

	spin_lock_irqsave(&port->lock, flags);
	port->flags &= ~ASYNC_INITIALIZED;
	spin_unlock_irqrestore(&port->lock, flags);

	if (!test_bit(ST_CLOSING, &portp->state))
		stli_rawclose(brdp, portp, 0, 0);

	spin_lock_irqsave(&stli_lock, flags);
	if (tty->termios->c_cflag & HUPCL) {
		stli_mkasysigs(&portp->asig, 0, 0);
		if (test_bit(ST_CMDING, &portp->state)) {
			set_bit(ST_DOSIGS, &portp->state);
			set_bit(ST_DOFLUSHTX, &portp->state);
			set_bit(ST_DOFLUSHRX, &portp->state);
		} else {
			stli_sendcmd(brdp, portp, A_SETSIGNALSF,
				&portp->asig, sizeof(asysigs_t), 0);
		}
	}

	clear_bit(ST_TXBUSY, &portp->state);
	clear_bit(ST_RXSTOP, &portp->state);
	set_bit(TTY_IO_ERROR, &tty->flags);
	spin_unlock_irqrestore(&stli_lock, flags);

	tty_port_hangup(port);
}

/*****************************************************************************/

/*
 *	Flush characters from the lower buffer. We may not have user context
 *	so we cannot sleep waiting for it to complete. Also we need to check
 *	if there is chars for this port in the TX cook buffer, and flush them
 *	as well.
 */

static void stli_flushbuffer(struct tty_struct *tty)
{
	struct stliport *portp;
	struct stlibrd *brdp;
	unsigned long ftype, flags;

	portp = tty->driver_data;
	if (portp == NULL)
		return;
	if (portp->brdnr >= stli_nrbrds)
		return;
	brdp = stli_brds[portp->brdnr];
	if (brdp == NULL)
		return;

	spin_lock_irqsave(&brd_lock, flags);
	if (tty == stli_txcooktty) {
		stli_txcooktty = NULL;
		stli_txcooksize = 0;
		stli_txcookrealsize = 0;
	}
	if (test_bit(ST_CMDING, &portp->state)) {
		set_bit(ST_DOFLUSHTX, &portp->state);
	} else {
		ftype = FLUSHTX;
		if (test_bit(ST_DOFLUSHRX, &portp->state)) {
			ftype |= FLUSHRX;
			clear_bit(ST_DOFLUSHRX, &portp->state);
		}
		__stli_sendcmd(brdp, portp, A_FLUSH, &ftype, sizeof(u32), 0);
	}
	spin_unlock_irqrestore(&brd_lock, flags);
	tty_wakeup(tty);
}

/*****************************************************************************/

static int stli_breakctl(struct tty_struct *tty, int state)
{
	struct stlibrd	*brdp;
	struct stliport	*portp;
	long		arg;

	portp = tty->driver_data;
	if (portp == NULL)
		return -EINVAL;
	if (portp->brdnr >= stli_nrbrds)
		return -EINVAL;
	brdp = stli_brds[portp->brdnr];
	if (brdp == NULL)
		return -EINVAL;

	arg = (state == -1) ? BREAKON : BREAKOFF;
	stli_cmdwait(brdp, portp, A_BREAK, &arg, sizeof(long), 0);
	return 0;
}

/*****************************************************************************/

static void stli_waituntilsent(struct tty_struct *tty, int timeout)
{
	struct stliport *portp;
	unsigned long tend;

	portp = tty->driver_data;
	if (portp == NULL)
		return;

	if (timeout == 0)
		timeout = HZ;
	tend = jiffies + timeout;

	while (test_bit(ST_TXBUSY, &portp->state)) {
		if (signal_pending(current))
			break;
		msleep_interruptible(20);
		if (time_after_eq(jiffies, tend))
			break;
	}
}

/*****************************************************************************/

static void stli_sendxchar(struct tty_struct *tty, char ch)
{
	struct stlibrd	*brdp;
	struct stliport	*portp;
	asyctrl_t	actrl;

	portp = tty->driver_data;
	if (portp == NULL)
		return;
	if (portp->brdnr >= stli_nrbrds)
		return;
	brdp = stli_brds[portp->brdnr];
	if (brdp == NULL)
		return;

	memset(&actrl, 0, sizeof(asyctrl_t));
	if (ch == STOP_CHAR(tty)) {
		actrl.rxctrl = CT_STOPFLOW;
	} else if (ch == START_CHAR(tty)) {
		actrl.rxctrl = CT_STARTFLOW;
	} else {
		actrl.txctrl = CT_SENDCHR;
		actrl.tximdch = ch;
	}
	stli_cmdwait(brdp, portp, A_PORTCTRL, &actrl, sizeof(asyctrl_t), 0);
}

static void stli_portinfo(struct seq_file *m, struct stlibrd *brdp, struct stliport *portp, int portnr)
{
	char *uart;
	int rc;

	rc = stli_portcmdstats(NULL, portp);

	uart = "UNKNOWN";
	if (brdp->state & BST_STARTED) {
		switch (stli_comstats.hwid) {
		case 0:	uart = "2681"; break;
		case 1:	uart = "SC26198"; break;
		default:uart = "CD1400"; break;
		}
	}
	seq_printf(m, "%d: uart:%s ", portnr, uart);

	if ((brdp->state & BST_STARTED) && (rc >= 0)) {
		char sep;

		seq_printf(m, "tx:%d rx:%d", (int) stli_comstats.txtotal,
			(int) stli_comstats.rxtotal);

		if (stli_comstats.rxframing)
			seq_printf(m, " fe:%d",
				(int) stli_comstats.rxframing);
		if (stli_comstats.rxparity)
			seq_printf(m, " pe:%d",
				(int) stli_comstats.rxparity);
		if (stli_comstats.rxbreaks)
			seq_printf(m, " brk:%d",
				(int) stli_comstats.rxbreaks);
		if (stli_comstats.rxoverrun)
			seq_printf(m, " oe:%d",
				(int) stli_comstats.rxoverrun);

		sep = ' ';
		if (stli_comstats.signals & TIOCM_RTS) {
			seq_printf(m, "%c%s", sep, "RTS");
			sep = '|';
		}
		if (stli_comstats.signals & TIOCM_CTS) {
			seq_printf(m, "%c%s", sep, "CTS");
			sep = '|';
		}
		if (stli_comstats.signals & TIOCM_DTR) {
			seq_printf(m, "%c%s", sep, "DTR");
			sep = '|';
		}
		if (stli_comstats.signals & TIOCM_CD) {
			seq_printf(m, "%c%s", sep, "DCD");
			sep = '|';
		}
		if (stli_comstats.signals & TIOCM_DSR) {
			seq_printf(m, "%c%s", sep, "DSR");
			sep = '|';
		}
	}
	seq_putc(m, '\n');
}

/*****************************************************************************/

/*
 *	Port info, read from the /proc file system.
 */

static int stli_proc_show(struct seq_file *m, void *v)
{
	struct stlibrd *brdp;
	struct stliport *portp;
	unsigned int brdnr, portnr, totalport;

	totalport = 0;

	seq_printf(m, "%s: version %s\n", stli_drvtitle, stli_drvversion);

/*
 *	We scan through for each board, panel and port. The offset is
 *	calculated on the fly, and irrelevant ports are skipped.
 */
	for (brdnr = 0; (brdnr < stli_nrbrds); brdnr++) {
		brdp = stli_brds[brdnr];
		if (brdp == NULL)
			continue;
		if (brdp->state == 0)
			continue;

		totalport = brdnr * STL_MAXPORTS;
		for (portnr = 0; (portnr < brdp->nrports); portnr++,
		    totalport++) {
			portp = brdp->ports[portnr];
			if (portp == NULL)
				continue;
			stli_portinfo(m, brdp, portp, totalport);
		}
	}
	return 0;
}

static int stli_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, stli_proc_show, NULL);
}

static const struct file_operations stli_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= stli_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/*****************************************************************************/

/*
 *	Generic send command routine. This will send a message to the slave,
 *	of the specified type with the specified argument. Must be very
 *	careful of data that will be copied out from shared memory -
 *	containing command results. The command completion is all done from
 *	a poll routine that does not have user context. Therefore you cannot
 *	copy back directly into user space, or to the kernel stack of a
 *	process. This routine does not sleep, so can be called from anywhere.
 *
 *	The caller must hold the brd_lock (see also stli_sendcmd the usual
 *	entry point)
 */

static void __stli_sendcmd(struct stlibrd *brdp, struct stliport *portp, unsigned long cmd, void *arg, int size, int copyback)
{
	cdkhdr_t __iomem *hdrp;
	cdkctrl_t __iomem *cp;
	unsigned char __iomem *bits;

	if (test_bit(ST_CMDING, &portp->state)) {
		printk(KERN_ERR "istallion: command already busy, cmd=%x!\n",
				(int) cmd);
		return;
	}

	EBRDENABLE(brdp);
	cp = &((cdkasy_t __iomem *) EBRDGETMEMPTR(brdp, portp->addr))->ctrl;
	if (size > 0) {
		memcpy_toio((void __iomem *) &(cp->args[0]), arg, size);
		if (copyback) {
			portp->argp = arg;
			portp->argsize = size;
		}
	}
	writel(0, &cp->status);
	writel(cmd, &cp->cmd);
	hdrp = (cdkhdr_t __iomem *) EBRDGETMEMPTR(brdp, CDK_CDKADDR);
	bits = ((unsigned char __iomem *) hdrp) + brdp->slaveoffset +
		portp->portidx;
	writeb(readb(bits) | portp->portbit, bits);
	set_bit(ST_CMDING, &portp->state);
	EBRDDISABLE(brdp);
}

static void stli_sendcmd(struct stlibrd *brdp, struct stliport *portp, unsigned long cmd, void *arg, int size, int copyback)
{
	unsigned long		flags;

	spin_lock_irqsave(&brd_lock, flags);
	__stli_sendcmd(brdp, portp, cmd, arg, size, copyback);
	spin_unlock_irqrestore(&brd_lock, flags);
}

/*****************************************************************************/

/*
 *	Read data from shared memory. This assumes that the shared memory
 *	is enabled and that interrupts are off. Basically we just empty out
 *	the shared memory buffer into the tty buffer. Must be careful to
 *	handle the case where we fill up the tty buffer, but still have
 *	more chars to unload.
 */

static void stli_read(struct stlibrd *brdp, struct stliport *portp)
{
	cdkasyrq_t __iomem *rp;
	char __iomem *shbuf;
	struct tty_struct	*tty;
	unsigned int head, tail, size;
	unsigned int len, stlen;

	if (test_bit(ST_RXSTOP, &portp->state))
		return;
	tty = tty_port_tty_get(&portp->port);
	if (tty == NULL)
		return;

	rp = &((cdkasy_t __iomem *) EBRDGETMEMPTR(brdp, portp->addr))->rxq;
	head = (unsigned int) readw(&rp->head);
	if (head != ((unsigned int) readw(&rp->head)))
		head = (unsigned int) readw(&rp->head);
	tail = (unsigned int) readw(&rp->tail);
	size = portp->rxsize;
	if (head >= tail) {
		len = head - tail;
		stlen = len;
	} else {
		len = size - (tail - head);
		stlen = size - tail;
	}

	len = tty_buffer_request_room(tty, len);

	shbuf = (char __iomem *) EBRDGETMEMPTR(brdp, portp->rxoffset);

	while (len > 0) {
		unsigned char *cptr;

		stlen = min(len, stlen);
		tty_prepare_flip_string(tty, &cptr, stlen);
		memcpy_fromio(cptr, shbuf + tail, stlen);
		len -= stlen;
		tail += stlen;
		if (tail >= size) {
			tail = 0;
			stlen = head;
		}
	}
	rp = &((cdkasy_t __iomem *) EBRDGETMEMPTR(brdp, portp->addr))->rxq;
	writew(tail, &rp->tail);

	if (head != tail)
		set_bit(ST_RXING, &portp->state);

	tty_schedule_flip(tty);
	tty_kref_put(tty);
}

/*****************************************************************************/

/*
 *	Set up and carry out any delayed commands. There is only a small set
 *	of slave commands that can be done "off-level". So it is not too
 *	difficult to deal with them here.
 */

static void stli_dodelaycmd(struct stliport *portp, cdkctrl_t __iomem *cp)
{
	int cmd;

	if (test_bit(ST_DOSIGS, &portp->state)) {
		if (test_bit(ST_DOFLUSHTX, &portp->state) &&
		    test_bit(ST_DOFLUSHRX, &portp->state))
			cmd = A_SETSIGNALSF;
		else if (test_bit(ST_DOFLUSHTX, &portp->state))
			cmd = A_SETSIGNALSFTX;
		else if (test_bit(ST_DOFLUSHRX, &portp->state))
			cmd = A_SETSIGNALSFRX;
		else
			cmd = A_SETSIGNALS;
		clear_bit(ST_DOFLUSHTX, &portp->state);
		clear_bit(ST_DOFLUSHRX, &portp->state);
		clear_bit(ST_DOSIGS, &portp->state);
		memcpy_toio((void __iomem *) &(cp->args[0]), (void *) &portp->asig,
			sizeof(asysigs_t));
		writel(0, &cp->status);
		writel(cmd, &cp->cmd);
		set_bit(ST_CMDING, &portp->state);
	} else if (test_bit(ST_DOFLUSHTX, &portp->state) ||
	    test_bit(ST_DOFLUSHRX, &portp->state)) {
		cmd = ((test_bit(ST_DOFLUSHTX, &portp->state)) ? FLUSHTX : 0);
		cmd |= ((test_bit(ST_DOFLUSHRX, &portp->state)) ? FLUSHRX : 0);
		clear_bit(ST_DOFLUSHTX, &portp->state);
		clear_bit(ST_DOFLUSHRX, &portp->state);
		memcpy_toio((void __iomem *) &(cp->args[0]), (void *) &cmd, sizeof(int));
		writel(0, &cp->status);
		writel(A_FLUSH, &cp->cmd);
		set_bit(ST_CMDING, &portp->state);
	}
}

/*****************************************************************************/

/*
 *	Host command service checking. This handles commands or messages
 *	coming from the slave to the host. Must have board shared memory
 *	enabled and interrupts off when called. Notice that by servicing the
 *	read data last we don't need to change the shared memory pointer
 *	during processing (which is a slow IO operation).
 *	Return value indicates if this port is still awaiting actions from
 *	the slave (like open, command, or even TX data being sent). If 0
 *	then port is still busy, otherwise no longer busy.
 */

static int stli_hostcmd(struct stlibrd *brdp, struct stliport *portp)
{
	cdkasy_t __iomem *ap;
	cdkctrl_t __iomem *cp;
	struct tty_struct *tty;
	asynotify_t nt;
	unsigned long oldsigs;
	int rc, donerx;

	ap = (cdkasy_t __iomem *) EBRDGETMEMPTR(brdp, portp->addr);
	cp = &ap->ctrl;

/*
 *	Check if we are waiting for an open completion message.
 */
	if (test_bit(ST_OPENING, &portp->state)) {
		rc = readl(&cp->openarg);
		if (readb(&cp->open) == 0 && rc != 0) {
			if (rc > 0)
				rc--;
			writel(0, &cp->openarg);
			portp->rc = rc;
			clear_bit(ST_OPENING, &portp->state);
			wake_up_interruptible(&portp->raw_wait);
		}
	}

/*
 *	Check if we are waiting for a close completion message.
 */
	if (test_bit(ST_CLOSING, &portp->state)) {
		rc = (int) readl(&cp->closearg);
		if (readb(&cp->close) == 0 && rc != 0) {
			if (rc > 0)
				rc--;
			writel(0, &cp->closearg);
			portp->rc = rc;
			clear_bit(ST_CLOSING, &portp->state);
			wake_up_interruptible(&portp->raw_wait);
		}
	}

/*
 *	Check if we are waiting for a command completion message. We may
 *	need to copy out the command results associated with this command.
 */
	if (test_bit(ST_CMDING, &portp->state)) {
		rc = readl(&cp->status);
		if (readl(&cp->cmd) == 0 && rc != 0) {
			if (rc > 0)
				rc--;
			if (portp->argp != NULL) {
				memcpy_fromio(portp->argp, (void __iomem *) &(cp->args[0]),
					portp->argsize);
				portp->argp = NULL;
			}
			writel(0, &cp->status);
			portp->rc = rc;
			clear_bit(ST_CMDING, &portp->state);
			stli_dodelaycmd(portp, cp);
			wake_up_interruptible(&portp->raw_wait);
		}
	}

/*
 *	Check for any notification messages ready. This includes lots of
 *	different types of events - RX chars ready, RX break received,
 *	TX data low or empty in the slave, modem signals changed state.
 */
	donerx = 0;

	if (ap->notify) {
		nt = ap->changed;
		ap->notify = 0;
		tty = tty_port_tty_get(&portp->port);

		if (nt.signal & SG_DCD) {
			oldsigs = portp->sigs;
			portp->sigs = stli_mktiocm(nt.sigvalue);
			clear_bit(ST_GETSIGS, &portp->state);
			if ((portp->sigs & TIOCM_CD) &&
			    ((oldsigs & TIOCM_CD) == 0))
				wake_up_interruptible(&portp->port.open_wait);
			if ((oldsigs & TIOCM_CD) &&
			    ((portp->sigs & TIOCM_CD) == 0)) {
				if (portp->port.flags & ASYNC_CHECK_CD) {
					if (tty)
						tty_hangup(tty);
				}
			}
		}

		if (nt.data & DT_TXEMPTY)
			clear_bit(ST_TXBUSY, &portp->state);
		if (nt.data & (DT_TXEMPTY | DT_TXLOW)) {
			if (tty != NULL) {
				tty_wakeup(tty);
				EBRDENABLE(brdp);
			}
		}

		if ((nt.data & DT_RXBREAK) && (portp->rxmarkmsk & BRKINT)) {
			if (tty != NULL) {
				tty_insert_flip_char(tty, 0, TTY_BREAK);
				if (portp->port.flags & ASYNC_SAK) {
					do_SAK(tty);
					EBRDENABLE(brdp);
				}
				tty_schedule_flip(tty);
			}
		}
		tty_kref_put(tty);

		if (nt.data & DT_RXBUSY) {
			donerx++;
			stli_read(brdp, portp);
		}
	}

/*
 *	It might seem odd that we are checking for more RX chars here.
 *	But, we need to handle the case where the tty buffer was previously
 *	filled, but we had more characters to pass up. The slave will not
 *	send any more RX notify messages until the RX buffer has been emptied.
 *	But it will leave the service bits on (since the buffer is not empty).
 *	So from here we can try to process more RX chars.
 */
	if ((!donerx) && test_bit(ST_RXING, &portp->state)) {
		clear_bit(ST_RXING, &portp->state);
		stli_read(brdp, portp);
	}

	return((test_bit(ST_OPENING, &portp->state) ||
		test_bit(ST_CLOSING, &portp->state) ||
		test_bit(ST_CMDING, &portp->state) ||
		test_bit(ST_TXBUSY, &portp->state) ||
		test_bit(ST_RXING, &portp->state)) ? 0 : 1);
}

/*****************************************************************************/

/*
 *	Service all ports on a particular board. Assumes that the boards
 *	shared memory is enabled, and that the page pointer is pointed
 *	at the cdk header structure.
 */

static void stli_brdpoll(struct stlibrd *brdp, cdkhdr_t __iomem *hdrp)
{
	struct stliport *portp;
	unsigned char hostbits[(STL_MAXCHANS / 8) + 1];
	unsigned char slavebits[(STL_MAXCHANS / 8) + 1];
	unsigned char __iomem *slavep;
	int bitpos, bitat, bitsize;
	int channr, nrdevs, slavebitchange;

	bitsize = brdp->bitsize;
	nrdevs = brdp->nrdevs;

/*
 *	Check if slave wants any service. Basically we try to do as
 *	little work as possible here. There are 2 levels of service
 *	bits. So if there is nothing to do we bail early. We check
 *	8 service bits at a time in the inner loop, so we can bypass
 *	the lot if none of them want service.
 */
	memcpy_fromio(&hostbits[0], (((unsigned char __iomem *) hdrp) + brdp->hostoffset),
		bitsize);

	memset(&slavebits[0], 0, bitsize);
	slavebitchange = 0;

	for (bitpos = 0; (bitpos < bitsize); bitpos++) {
		if (hostbits[bitpos] == 0)
			continue;
		channr = bitpos * 8;
		for (bitat = 0x1; (channr < nrdevs); channr++, bitat <<= 1) {
			if (hostbits[bitpos] & bitat) {
				portp = brdp->ports[(channr - 1)];
				if (stli_hostcmd(brdp, portp)) {
					slavebitchange++;
					slavebits[bitpos] |= bitat;
				}
			}
		}
	}

/*
 *	If any of the ports are no longer busy then update them in the
 *	slave request bits. We need to do this after, since a host port
 *	service may initiate more slave requests.
 */
	if (slavebitchange) {
		hdrp = (cdkhdr_t __iomem *) EBRDGETMEMPTR(brdp, CDK_CDKADDR);
		slavep = ((unsigned char __iomem *) hdrp) + brdp->slaveoffset;
		for (bitpos = 0; (bitpos < bitsize); bitpos++) {
			if (readb(slavebits + bitpos))
				writeb(readb(slavep + bitpos) & ~slavebits[bitpos], slavebits + bitpos);
		}
	}
}

/*****************************************************************************/

/*
 *	Driver poll routine. This routine polls the boards in use and passes
 *	messages back up to host when necessary. This is actually very
 *	CPU efficient, since we will always have the kernel poll clock, it
 *	adds only a few cycles when idle (since board service can be
 *	determined very easily), but when loaded generates no interrupts
 *	(with their expensive associated context change).
 */

static void stli_poll(unsigned long arg)
{
	cdkhdr_t __iomem *hdrp;
	struct stlibrd *brdp;
	unsigned int brdnr;

	mod_timer(&stli_timerlist, STLI_TIMEOUT);

/*
 *	Check each board and do any servicing required.
 */
	for (brdnr = 0; (brdnr < stli_nrbrds); brdnr++) {
		brdp = stli_brds[brdnr];
		if (brdp == NULL)
			continue;
		if ((brdp->state & BST_STARTED) == 0)
			continue;

		spin_lock(&brd_lock);
		EBRDENABLE(brdp);
		hdrp = (cdkhdr_t __iomem *) EBRDGETMEMPTR(brdp, CDK_CDKADDR);
		if (readb(&hdrp->hostreq))
			stli_brdpoll(brdp, hdrp);
		EBRDDISABLE(brdp);
		spin_unlock(&brd_lock);
	}
}

/*****************************************************************************/

/*
 *	Translate the termios settings into the port setting structure of
 *	the slave.
 */

static void stli_mkasyport(struct tty_struct *tty, struct stliport *portp,
				asyport_t *pp, struct ktermios *tiosp)
{
	memset(pp, 0, sizeof(asyport_t));

/*
 *	Start of by setting the baud, char size, parity and stop bit info.
 */
	pp->baudout = tty_get_baud_rate(tty);
	if ((tiosp->c_cflag & CBAUD) == B38400) {
		if ((portp->port.flags & ASYNC_SPD_MASK) == ASYNC_SPD_HI)
			pp->baudout = 57600;
		else if ((portp->port.flags & ASYNC_SPD_MASK) == ASYNC_SPD_VHI)
			pp->baudout = 115200;
		else if ((portp->port.flags & ASYNC_SPD_MASK) == ASYNC_SPD_SHI)
			pp->baudout = 230400;
		else if ((portp->port.flags & ASYNC_SPD_MASK) == ASYNC_SPD_WARP)
			pp->baudout = 460800;
		else if ((portp->port.flags & ASYNC_SPD_MASK) == ASYNC_SPD_CUST)
			pp->baudout = (portp->baud_base / portp->custom_divisor);
	}
	if (pp->baudout > STL_MAXBAUD)
		pp->baudout = STL_MAXBAUD;
	pp->baudin = pp->baudout;

	switch (tiosp->c_cflag & CSIZE) {
	case CS5:
		pp->csize = 5;
		break;
	case CS6:
		pp->csize = 6;
		break;
	case CS7:
		pp->csize = 7;
		break;
	default:
		pp->csize = 8;
		break;
	}

	if (tiosp->c_cflag & CSTOPB)
		pp->stopbs = PT_STOP2;
	else
		pp->stopbs = PT_STOP1;

	if (tiosp->c_cflag & PARENB) {
		if (tiosp->c_cflag & PARODD)
			pp->parity = PT_ODDPARITY;
		else
			pp->parity = PT_EVENPARITY;
	} else {
		pp->parity = PT_NOPARITY;
	}

/*
 *	Set up any flow control options enabled.
 */
	if (tiosp->c_iflag & IXON) {
		pp->flow |= F_IXON;
		if (tiosp->c_iflag & IXANY)
			pp->flow |= F_IXANY;
	}
	if (tiosp->c_cflag & CRTSCTS)
		pp->flow |= (F_RTSFLOW | F_CTSFLOW);

	pp->startin = tiosp->c_cc[VSTART];
	pp->stopin = tiosp->c_cc[VSTOP];
	pp->startout = tiosp->c_cc[VSTART];
	pp->stopout = tiosp->c_cc[VSTOP];

/*
 *	Set up the RX char marking mask with those RX error types we must
 *	catch. We can get the slave to help us out a little here, it will
 *	ignore parity errors and breaks for us, and mark parity errors in
 *	the data stream.
 */
	if (tiosp->c_iflag & IGNPAR)
		pp->iflag |= FI_IGNRXERRS;
	if (tiosp->c_iflag & IGNBRK)
		pp->iflag |= FI_IGNBREAK;

	portp->rxmarkmsk = 0;
	if (tiosp->c_iflag & (INPCK | PARMRK))
		pp->iflag |= FI_1MARKRXERRS;
	if (tiosp->c_iflag & BRKINT)
		portp->rxmarkmsk |= BRKINT;

/*
 *	Set up clocal processing as required.
 */
	if (tiosp->c_cflag & CLOCAL)
		portp->port.flags &= ~ASYNC_CHECK_CD;
	else
		portp->port.flags |= ASYNC_CHECK_CD;

/*
 *	Transfer any persistent flags into the asyport structure.
 */
	pp->pflag = (portp->pflag & 0xffff);
	pp->vmin = (portp->pflag & P_RXIMIN) ? 1 : 0;
	pp->vtime = (portp->pflag & P_RXITIME) ? 1 : 0;
	pp->cc[1] = (portp->pflag & P_RXTHOLD) ? 1 : 0;
}

/*****************************************************************************/

/*
 *	Construct a slave signals structure for setting the DTR and RTS
 *	signals as specified.
 */

static void stli_mkasysigs(asysigs_t *sp, int dtr, int rts)
{
	memset(sp, 0, sizeof(asysigs_t));
	if (dtr >= 0) {
		sp->signal |= SG_DTR;
		sp->sigvalue |= ((dtr > 0) ? SG_DTR : 0);
	}
	if (rts >= 0) {
		sp->signal |= SG_RTS;
		sp->sigvalue |= ((rts > 0) ? SG_RTS : 0);
	}
}

/*****************************************************************************/

/*
 *	Convert the signals returned from the slave into a local TIOCM type
 *	signals value. We keep them locally in TIOCM format.
 */

static long stli_mktiocm(unsigned long sigvalue)
{
	long	tiocm = 0;
	tiocm |= ((sigvalue & SG_DCD) ? TIOCM_CD : 0);
	tiocm |= ((sigvalue & SG_CTS) ? TIOCM_CTS : 0);
	tiocm |= ((sigvalue & SG_RI) ? TIOCM_RI : 0);
	tiocm |= ((sigvalue & SG_DSR) ? TIOCM_DSR : 0);
	tiocm |= ((sigvalue & SG_DTR) ? TIOCM_DTR : 0);
	tiocm |= ((sigvalue & SG_RTS) ? TIOCM_RTS : 0);
	return(tiocm);
}

/*****************************************************************************/

/*
 *	All panels and ports actually attached have been worked out. All
 *	we need to do here is set up the appropriate per port data structures.
 */

static int stli_initports(struct stlibrd *brdp)
{
	struct stliport	*portp;
	unsigned int i, panelnr, panelport;

	for (i = 0, panelnr = 0, panelport = 0; (i < brdp->nrports); i++) {
		portp = kzalloc(sizeof(struct stliport), GFP_KERNEL);
		if (!portp) {
			printk(KERN_WARNING "istallion: failed to allocate port structure\n");
			continue;
		}
		tty_port_init(&portp->port);
		portp->port.ops = &stli_port_ops;
		portp->magic = STLI_PORTMAGIC;
		portp->portnr = i;
		portp->brdnr = brdp->brdnr;
		portp->panelnr = panelnr;
		portp->baud_base = STL_BAUDBASE;
		portp->port.close_delay = STL_CLOSEDELAY;
		portp->closing_wait = 30 * HZ;
		init_waitqueue_head(&portp->port.open_wait);
		init_waitqueue_head(&portp->port.close_wait);
		init_waitqueue_head(&portp->raw_wait);
		panelport++;
		if (panelport >= brdp->panels[panelnr]) {
			panelport = 0;
			panelnr++;
		}
		brdp->ports[i] = portp;
	}

	return 0;
}

/*****************************************************************************/

/*
 *	All the following routines are board specific hardware operations.
 */

static void stli_ecpinit(struct stlibrd *brdp)
{
	unsigned long	memconf;

	outb(ECP_ATSTOP, (brdp->iobase + ECP_ATCONFR));
	udelay(10);
	outb(ECP_ATDISABLE, (brdp->iobase + ECP_ATCONFR));
	udelay(100);

	memconf = (brdp->memaddr & ECP_ATADDRMASK) >> ECP_ATADDRSHFT;
	outb(memconf, (brdp->iobase + ECP_ATMEMAR));
}

/*****************************************************************************/

static void stli_ecpenable(struct stlibrd *brdp)
{	
	outb(ECP_ATENABLE, (brdp->iobase + ECP_ATCONFR));
}

/*****************************************************************************/

static void stli_ecpdisable(struct stlibrd *brdp)
{	
	outb(ECP_ATDISABLE, (brdp->iobase + ECP_ATCONFR));
}

/*****************************************************************************/

static void __iomem *stli_ecpgetmemptr(struct stlibrd *brdp, unsigned long offset, int line)
{	
	void __iomem *ptr;
	unsigned char val;

	if (offset > brdp->memsize) {
		printk(KERN_ERR "istallion: shared memory pointer=%x out of "
				"range at line=%d(%d), brd=%d\n",
			(int) offset, line, __LINE__, brdp->brdnr);
		ptr = NULL;
		val = 0;
	} else {
		ptr = brdp->membase + (offset % ECP_ATPAGESIZE);
		val = (unsigned char) (offset / ECP_ATPAGESIZE);
	}
	outb(val, (brdp->iobase + ECP_ATMEMPR));
	return(ptr);
}

/*****************************************************************************/

static void stli_ecpreset(struct stlibrd *brdp)
{	
	outb(ECP_ATSTOP, (brdp->iobase + ECP_ATCONFR));
	udelay(10);
	outb(ECP_ATDISABLE, (brdp->iobase + ECP_ATCONFR));
	udelay(500);
}

/*****************************************************************************/

static void stli_ecpintr(struct stlibrd *brdp)
{	
	outb(0x1, brdp->iobase);
}

/*****************************************************************************/

/*
 *	The following set of functions act on ECP EISA boards.
 */

static void stli_ecpeiinit(struct stlibrd *brdp)
{
	unsigned long	memconf;

	outb(0x1, (brdp->iobase + ECP_EIBRDENAB));
	outb(ECP_EISTOP, (brdp->iobase + ECP_EICONFR));
	udelay(10);
	outb(ECP_EIDISABLE, (brdp->iobase + ECP_EICONFR));
	udelay(500);

	memconf = (brdp->memaddr & ECP_EIADDRMASKL) >> ECP_EIADDRSHFTL;
	outb(memconf, (brdp->iobase + ECP_EIMEMARL));
	memconf = (brdp->memaddr & ECP_EIADDRMASKH) >> ECP_EIADDRSHFTH;
	outb(memconf, (brdp->iobase + ECP_EIMEMARH));
}

/*****************************************************************************/

static void stli_ecpeienable(struct stlibrd *brdp)
{	
	outb(ECP_EIENABLE, (brdp->iobase + ECP_EICONFR));
}

/*****************************************************************************/

static void stli_ecpeidisable(struct stlibrd *brdp)
{	
	outb(ECP_EIDISABLE, (brdp->iobase + ECP_EICONFR));
}

/*****************************************************************************/

static void __iomem *stli_ecpeigetmemptr(struct stlibrd *brdp, unsigned long offset, int line)
{	
	void __iomem *ptr;
	unsigned char	val;

	if (offset > brdp->memsize) {
		printk(KERN_ERR "istallion: shared memory pointer=%x out of "
				"range at line=%d(%d), brd=%d\n",
			(int) offset, line, __LINE__, brdp->brdnr);
		ptr = NULL;
		val = 0;
	} else {
		ptr = brdp->membase + (offset % ECP_EIPAGESIZE);
		if (offset < ECP_EIPAGESIZE)
			val = ECP_EIENABLE;
		else
			val = ECP_EIENABLE | 0x40;
	}
	outb(val, (brdp->iobase + ECP_EICONFR));
	return(ptr);
}

/*****************************************************************************/

static void stli_ecpeireset(struct stlibrd *brdp)
{	
	outb(ECP_EISTOP, (brdp->iobase + ECP_EICONFR));
	udelay(10);
	outb(ECP_EIDISABLE, (brdp->iobase + ECP_EICONFR));
	udelay(500);
}

/*****************************************************************************/

/*
 *	The following set of functions act on ECP MCA boards.
 */

static void stli_ecpmcenable(struct stlibrd *brdp)
{	
	outb(ECP_MCENABLE, (brdp->iobase + ECP_MCCONFR));
}

/*****************************************************************************/

static void stli_ecpmcdisable(struct stlibrd *brdp)
{	
	outb(ECP_MCDISABLE, (brdp->iobase + ECP_MCCONFR));
}

/*****************************************************************************/

static void __iomem *stli_ecpmcgetmemptr(struct stlibrd *brdp, unsigned long offset, int line)
{	
	void __iomem *ptr;
	unsigned char val;

	if (offset > brdp->memsize) {
		printk(KERN_ERR "istallion: shared memory pointer=%x out of "
				"range at line=%d(%d), brd=%d\n",
			(int) offset, line, __LINE__, brdp->brdnr);
		ptr = NULL;
		val = 0;
	} else {
		ptr = brdp->membase + (offset % ECP_MCPAGESIZE);
		val = ((unsigned char) (offset / ECP_MCPAGESIZE)) | ECP_MCENABLE;
	}
	outb(val, (brdp->iobase + ECP_MCCONFR));
	return(ptr);
}

/*****************************************************************************/

static void stli_ecpmcreset(struct stlibrd *brdp)
{	
	outb(ECP_MCSTOP, (brdp->iobase + ECP_MCCONFR));
	udelay(10);
	outb(ECP_MCDISABLE, (brdp->iobase + ECP_MCCONFR));
	udelay(500);
}

/*****************************************************************************/

/*
 *	The following set of functions act on ECP PCI boards.
 */

static void stli_ecppciinit(struct stlibrd *brdp)
{
	outb(ECP_PCISTOP, (brdp->iobase + ECP_PCICONFR));
	udelay(10);
	outb(0, (brdp->iobase + ECP_PCICONFR));
	udelay(500);
}

/*****************************************************************************/

static void __iomem *stli_ecppcigetmemptr(struct stlibrd *brdp, unsigned long offset, int line)
{	
	void __iomem *ptr;
	unsigned char	val;

	if (offset > brdp->memsize) {
		printk(KERN_ERR "istallion: shared memory pointer=%x out of "
				"range at line=%d(%d), board=%d\n",
				(int) offset, line, __LINE__, brdp->brdnr);
		ptr = NULL;
		val = 0;
	} else {
		ptr = brdp->membase + (offset % ECP_PCIPAGESIZE);
		val = (offset / ECP_PCIPAGESIZE) << 1;
	}
	outb(val, (brdp->iobase + ECP_PCICONFR));
	return(ptr);
}

/*****************************************************************************/

static void stli_ecppcireset(struct stlibrd *brdp)
{	
	outb(ECP_PCISTOP, (brdp->iobase + ECP_PCICONFR));
	udelay(10);
	outb(0, (brdp->iobase + ECP_PCICONFR));
	udelay(500);
}

/*****************************************************************************/

/*
 *	The following routines act on ONboards.
 */

static void stli_onbinit(struct stlibrd *brdp)
{
	unsigned long	memconf;

	outb(ONB_ATSTOP, (brdp->iobase + ONB_ATCONFR));
	udelay(10);
	outb(ONB_ATDISABLE, (brdp->iobase + ONB_ATCONFR));
	mdelay(1000);

	memconf = (brdp->memaddr & ONB_ATADDRMASK) >> ONB_ATADDRSHFT;
	outb(memconf, (brdp->iobase + ONB_ATMEMAR));
	outb(0x1, brdp->iobase);
	mdelay(1);
}

/*****************************************************************************/

static void stli_onbenable(struct stlibrd *brdp)
{	
	outb((brdp->enabval | ONB_ATENABLE), (brdp->iobase + ONB_ATCONFR));
}

/*****************************************************************************/

static void stli_onbdisable(struct stlibrd *brdp)
{	
	outb((brdp->enabval | ONB_ATDISABLE), (brdp->iobase + ONB_ATCONFR));
}

/*****************************************************************************/

static void __iomem *stli_onbgetmemptr(struct stlibrd *brdp, unsigned long offset, int line)
{	
	void __iomem *ptr;

	if (offset > brdp->memsize) {
		printk(KERN_ERR "istallion: shared memory pointer=%x out of "
				"range at line=%d(%d), brd=%d\n",
				(int) offset, line, __LINE__, brdp->brdnr);
		ptr = NULL;
	} else {
		ptr = brdp->membase + (offset % ONB_ATPAGESIZE);
	}
	return(ptr);
}

/*****************************************************************************/

static void stli_onbreset(struct stlibrd *brdp)
{	
	outb(ONB_ATSTOP, (brdp->iobase + ONB_ATCONFR));
	udelay(10);
	outb(ONB_ATDISABLE, (brdp->iobase + ONB_ATCONFR));
	mdelay(1000);
}

/*****************************************************************************/

/*
 *	The following routines act on ONboard EISA.
 */

static void stli_onbeinit(struct stlibrd *brdp)
{
	unsigned long	memconf;

	outb(0x1, (brdp->iobase + ONB_EIBRDENAB));
	outb(ONB_EISTOP, (brdp->iobase + ONB_EICONFR));
	udelay(10);
	outb(ONB_EIDISABLE, (brdp->iobase + ONB_EICONFR));
	mdelay(1000);

	memconf = (brdp->memaddr & ONB_EIADDRMASKL) >> ONB_EIADDRSHFTL;
	outb(memconf, (brdp->iobase + ONB_EIMEMARL));
	memconf = (brdp->memaddr & ONB_EIADDRMASKH) >> ONB_EIADDRSHFTH;
	outb(memconf, (brdp->iobase + ONB_EIMEMARH));
	outb(0x1, brdp->iobase);
	mdelay(1);
}

/*****************************************************************************/

static void stli_onbeenable(struct stlibrd *brdp)
{	
	outb(ONB_EIENABLE, (brdp->iobase + ONB_EICONFR));
}

/*****************************************************************************/

static void stli_onbedisable(struct stlibrd *brdp)
{	
	outb(ONB_EIDISABLE, (brdp->iobase + ONB_EICONFR));
}

/*****************************************************************************/

static void __iomem *stli_onbegetmemptr(struct stlibrd *brdp, unsigned long offset, int line)
{	
	void __iomem *ptr;
	unsigned char val;

	if (offset > brdp->memsize) {
		printk(KERN_ERR "istallion: shared memory pointer=%x out of "
				"range at line=%d(%d), brd=%d\n",
			(int) offset, line, __LINE__, brdp->brdnr);
		ptr = NULL;
		val = 0;
	} else {
		ptr = brdp->membase + (offset % ONB_EIPAGESIZE);
		if (offset < ONB_EIPAGESIZE)
			val = ONB_EIENABLE;
		else
			val = ONB_EIENABLE | 0x40;
	}
	outb(val, (brdp->iobase + ONB_EICONFR));
	return(ptr);
}

/*****************************************************************************/

static void stli_onbereset(struct stlibrd *brdp)
{	
	outb(ONB_EISTOP, (brdp->iobase + ONB_EICONFR));
	udelay(10);
	outb(ONB_EIDISABLE, (brdp->iobase + ONB_EICONFR));
	mdelay(1000);
}

/*****************************************************************************/

/*
 *	The following routines act on Brumby boards.
 */

static void stli_bbyinit(struct stlibrd *brdp)
{
	outb(BBY_ATSTOP, (brdp->iobase + BBY_ATCONFR));
	udelay(10);
	outb(0, (brdp->iobase + BBY_ATCONFR));
	mdelay(1000);
	outb(0x1, brdp->iobase);
	mdelay(1);
}

/*****************************************************************************/

static void __iomem *stli_bbygetmemptr(struct stlibrd *brdp, unsigned long offset, int line)
{	
	void __iomem *ptr;
	unsigned char val;

	BUG_ON(offset > brdp->memsize);

	ptr = brdp->membase + (offset % BBY_PAGESIZE);
	val = (unsigned char) (offset / BBY_PAGESIZE);
	outb(val, (brdp->iobase + BBY_ATCONFR));
	return(ptr);
}

/*****************************************************************************/

static void stli_bbyreset(struct stlibrd *brdp)
{	
	outb(BBY_ATSTOP, (brdp->iobase + BBY_ATCONFR));
	udelay(10);
	outb(0, (brdp->iobase + BBY_ATCONFR));
	mdelay(1000);
}

/*****************************************************************************/

/*
 *	The following routines act on original old Stallion boards.
 */

static void stli_stalinit(struct stlibrd *brdp)
{
	outb(0x1, brdp->iobase);
	mdelay(1000);
}

/*****************************************************************************/

static void __iomem *stli_stalgetmemptr(struct stlibrd *brdp, unsigned long offset, int line)
{	
	BUG_ON(offset > brdp->memsize);
	return brdp->membase + (offset % STAL_PAGESIZE);
}

/*****************************************************************************/

static void stli_stalreset(struct stlibrd *brdp)
{	
	u32 __iomem *vecp;

	vecp = (u32 __iomem *) (brdp->membase + 0x30);
	writel(0xffff0000, vecp);
	outb(0, brdp->iobase);
	mdelay(1000);
}

/*****************************************************************************/

/*
 *	Try to find an ECP board and initialize it. This handles only ECP
 *	board types.
 */

static int stli_initecp(struct stlibrd *brdp)
{
	cdkecpsig_t sig;
	cdkecpsig_t __iomem *sigsp;
	unsigned int status, nxtid;
	char *name;
	int retval, panelnr, nrports;

	if ((brdp->iobase == 0) || (brdp->memaddr == 0)) {
		retval = -ENODEV;
		goto err;
	}

	brdp->iosize = ECP_IOSIZE;

	if (!request_region(brdp->iobase, brdp->iosize, "istallion")) {
		retval = -EIO;
		goto err;
	}

/*
 *	Based on the specific board type setup the common vars to access
 *	and enable shared memory. Set all board specific information now
 *	as well.
 */
	switch (brdp->brdtype) {
	case BRD_ECP:
		brdp->memsize = ECP_MEMSIZE;
		brdp->pagesize = ECP_ATPAGESIZE;
		brdp->init = stli_ecpinit;
		brdp->enable = stli_ecpenable;
		brdp->reenable = stli_ecpenable;
		brdp->disable = stli_ecpdisable;
		brdp->getmemptr = stli_ecpgetmemptr;
		brdp->intr = stli_ecpintr;
		brdp->reset = stli_ecpreset;
		name = "serial(EC8/64)";
		break;

	case BRD_ECPE:
		brdp->memsize = ECP_MEMSIZE;
		brdp->pagesize = ECP_EIPAGESIZE;
		brdp->init = stli_ecpeiinit;
		brdp->enable = stli_ecpeienable;
		brdp->reenable = stli_ecpeienable;
		brdp->disable = stli_ecpeidisable;
		brdp->getmemptr = stli_ecpeigetmemptr;
		brdp->intr = stli_ecpintr;
		brdp->reset = stli_ecpeireset;
		name = "serial(EC8/64-EI)";
		break;

	case BRD_ECPMC:
		brdp->memsize = ECP_MEMSIZE;
		brdp->pagesize = ECP_MCPAGESIZE;
		brdp->init = NULL;
		brdp->enable = stli_ecpmcenable;
		brdp->reenable = stli_ecpmcenable;
		brdp->disable = stli_ecpmcdisable;
		brdp->getmemptr = stli_ecpmcgetmemptr;
		brdp->intr = stli_ecpintr;
		brdp->reset = stli_ecpmcreset;
		name = "serial(EC8/64-MCA)";
		break;

	case BRD_ECPPCI:
		brdp->memsize = ECP_PCIMEMSIZE;
		brdp->pagesize = ECP_PCIPAGESIZE;
		brdp->init = stli_ecppciinit;
		brdp->enable = NULL;
		brdp->reenable = NULL;
		brdp->disable = NULL;
		brdp->getmemptr = stli_ecppcigetmemptr;
		brdp->intr = stli_ecpintr;
		brdp->reset = stli_ecppcireset;
		name = "serial(EC/RA-PCI)";
		break;

	default:
		retval = -EINVAL;
		goto err_reg;
	}

/*
 *	The per-board operations structure is all set up, so now let's go
 *	and get the board operational. Firstly initialize board configuration
 *	registers. Set the memory mapping info so we can get at the boards
 *	shared memory.
 */
	EBRDINIT(brdp);

	brdp->membase = ioremap_nocache(brdp->memaddr, brdp->memsize);
	if (brdp->membase == NULL) {
		retval = -ENOMEM;
		goto err_reg;
	}

/*
 *	Now that all specific code is set up, enable the shared memory and
 *	look for the a signature area that will tell us exactly what board
 *	this is, and what it is connected to it.
 */
	EBRDENABLE(brdp);
	sigsp = (cdkecpsig_t __iomem *) EBRDGETMEMPTR(brdp, CDK_SIGADDR);
	memcpy_fromio(&sig, sigsp, sizeof(cdkecpsig_t));
	EBRDDISABLE(brdp);

	if (sig.magic != cpu_to_le32(ECP_MAGIC)) {
		retval = -ENODEV;
		goto err_unmap;
	}

/*
 *	Scan through the signature looking at the panels connected to the
 *	board. Calculate the total number of ports as we go.
 */
	for (panelnr = 0, nxtid = 0; (panelnr < STL_MAXPANELS); panelnr++) {
		status = sig.panelid[nxtid];
		if ((status & ECH_PNLIDMASK) != nxtid)
			break;

		brdp->panelids[panelnr] = status;
		nrports = (status & ECH_PNL16PORT) ? 16 : 8;
		if ((nrports == 16) && ((status & ECH_PNLXPID) == 0))
			nxtid++;
		brdp->panels[panelnr] = nrports;
		brdp->nrports += nrports;
		nxtid++;
		brdp->nrpanels++;
	}


	brdp->state |= BST_FOUND;
	return 0;
err_unmap:
	iounmap(brdp->membase);
	brdp->membase = NULL;
err_reg:
	release_region(brdp->iobase, brdp->iosize);
err:
	return retval;
}

/*****************************************************************************/

/*
 *	Try to find an ONboard, Brumby or Stallion board and initialize it.
 *	This handles only these board types.
 */

static int stli_initonb(struct stlibrd *brdp)
{
	cdkonbsig_t sig;
	cdkonbsig_t __iomem *sigsp;
	char *name;
	int i, retval;

/*
 *	Do a basic sanity check on the IO and memory addresses.
 */
	if (brdp->iobase == 0 || brdp->memaddr == 0) {
		retval = -ENODEV;
		goto err;
	}

	brdp->iosize = ONB_IOSIZE;
	
	if (!request_region(brdp->iobase, brdp->iosize, "istallion")) {
		retval = -EIO;
		goto err;
	}

/*
 *	Based on the specific board type setup the common vars to access
 *	and enable shared memory. Set all board specific information now
 *	as well.
 */
	switch (brdp->brdtype) {
	case BRD_ONBOARD:
	case BRD_ONBOARD2:
		brdp->memsize = ONB_MEMSIZE;
		brdp->pagesize = ONB_ATPAGESIZE;
		brdp->init = stli_onbinit;
		brdp->enable = stli_onbenable;
		brdp->reenable = stli_onbenable;
		brdp->disable = stli_onbdisable;
		brdp->getmemptr = stli_onbgetmemptr;
		brdp->intr = stli_ecpintr;
		brdp->reset = stli_onbreset;
		if (brdp->memaddr > 0x100000)
			brdp->enabval = ONB_MEMENABHI;
		else
			brdp->enabval = ONB_MEMENABLO;
		name = "serial(ONBoard)";
		break;

	case BRD_ONBOARDE:
		brdp->memsize = ONB_EIMEMSIZE;
		brdp->pagesize = ONB_EIPAGESIZE;
		brdp->init = stli_onbeinit;
		brdp->enable = stli_onbeenable;
		brdp->reenable = stli_onbeenable;
		brdp->disable = stli_onbedisable;
		brdp->getmemptr = stli_onbegetmemptr;
		brdp->intr = stli_ecpintr;
		brdp->reset = stli_onbereset;
		name = "serial(ONBoard/E)";
		break;

	case BRD_BRUMBY4:
		brdp->memsize = BBY_MEMSIZE;
		brdp->pagesize = BBY_PAGESIZE;
		brdp->init = stli_bbyinit;
		brdp->enable = NULL;
		brdp->reenable = NULL;
		brdp->disable = NULL;
		brdp->getmemptr = stli_bbygetmemptr;
		brdp->intr = stli_ecpintr;
		brdp->reset = stli_bbyreset;
		name = "serial(Brumby)";
		break;

	case BRD_STALLION:
		brdp->memsize = STAL_MEMSIZE;
		brdp->pagesize = STAL_PAGESIZE;
		brdp->init = stli_stalinit;
		brdp->enable = NULL;
		brdp->reenable = NULL;
		brdp->disable = NULL;
		brdp->getmemptr = stli_stalgetmemptr;
		brdp->intr = stli_ecpintr;
		brdp->reset = stli_stalreset;
		name = "serial(Stallion)";
		break;

	default:
		retval = -EINVAL;
		goto err_reg;
	}

/*
 *	The per-board operations structure is all set up, so now let's go
 *	and get the board operational. Firstly initialize board configuration
 *	registers. Set the memory mapping info so we can get at the boards
 *	shared memory.
 */
	EBRDINIT(brdp);

	brdp->membase = ioremap_nocache(brdp->memaddr, brdp->memsize);
	if (brdp->membase == NULL) {
		retval = -ENOMEM;
		goto err_reg;
	}

/*
 *	Now that all specific code is set up, enable the shared memory and
 *	look for the a signature area that will tell us exactly what board
 *	this is, and how many ports.
 */
	EBRDENABLE(brdp);
	sigsp = (cdkonbsig_t __iomem *) EBRDGETMEMPTR(brdp, CDK_SIGADDR);
	memcpy_fromio(&sig, sigsp, sizeof(cdkonbsig_t));
	EBRDDISABLE(brdp);

	if (sig.magic0 != cpu_to_le16(ONB_MAGIC0) ||
	    sig.magic1 != cpu_to_le16(ONB_MAGIC1) ||
	    sig.magic2 != cpu_to_le16(ONB_MAGIC2) ||
	    sig.magic3 != cpu_to_le16(ONB_MAGIC3)) {
		retval = -ENODEV;
		goto err_unmap;
	}

/*
 *	Scan through the signature alive mask and calculate how many ports
 *	there are on this board.
 */
	brdp->nrpanels = 1;
	if (sig.amask1) {
		brdp->nrports = 32;
	} else {
		for (i = 0; (i < 16); i++) {
			if (((sig.amask0 << i) & 0x8000) == 0)
				break;
		}
		brdp->nrports = i;
	}
	brdp->panels[0] = brdp->nrports;


	brdp->state |= BST_FOUND;
	return 0;
err_unmap:
	iounmap(brdp->membase);
	brdp->membase = NULL;
err_reg:
	release_region(brdp->iobase, brdp->iosize);
err:
	return retval;
}

/*****************************************************************************/

/*
 *	Start up a running board. This routine is only called after the
 *	code has been down loaded to the board and is operational. It will
 *	read in the memory map, and get the show on the road...
 */

static int stli_startbrd(struct stlibrd *brdp)
{
	cdkhdr_t __iomem *hdrp;
	cdkmem_t __iomem *memp;
	cdkasy_t __iomem *ap;
	unsigned long flags;
	unsigned int portnr, nrdevs, i;
	struct stliport *portp;
	int rc = 0;
	u32 memoff;

	spin_lock_irqsave(&brd_lock, flags);
	EBRDENABLE(brdp);
	hdrp = (cdkhdr_t __iomem *) EBRDGETMEMPTR(brdp, CDK_CDKADDR);
	nrdevs = hdrp->nrdevs;

#if 0
	printk("%s(%d): CDK version %d.%d.%d --> "
		"nrdevs=%d memp=%x hostp=%x slavep=%x\n",
		 __FILE__, __LINE__, readb(&hdrp->ver_release), readb(&hdrp->ver_modification),
		 readb(&hdrp->ver_fix), nrdevs, (int) readl(&hdrp->memp), readl(&hdrp->hostp),
		 readl(&hdrp->slavep));
#endif

	if (nrdevs < (brdp->nrports + 1)) {
		printk(KERN_ERR "istallion: slave failed to allocate memory for "
				"all devices, devices=%d\n", nrdevs);
		brdp->nrports = nrdevs - 1;
	}
	brdp->nrdevs = nrdevs;
	brdp->hostoffset = hdrp->hostp - CDK_CDKADDR;
	brdp->slaveoffset = hdrp->slavep - CDK_CDKADDR;
	brdp->bitsize = (nrdevs + 7) / 8;
	memoff = readl(&hdrp->memp);
	if (memoff > brdp->memsize) {
		printk(KERN_ERR "istallion: corrupted shared memory region?\n");
		rc = -EIO;
		goto stli_donestartup;
	}
	memp = (cdkmem_t __iomem *) EBRDGETMEMPTR(brdp, memoff);
	if (readw(&memp->dtype) != TYP_ASYNCTRL) {
		printk(KERN_ERR "istallion: no slave control device found\n");
		goto stli_donestartup;
	}
	memp++;

/*
 *	Cycle through memory allocation of each port. We are guaranteed to
 *	have all ports inside the first page of slave window, so no need to
 *	change pages while reading memory map.
 */
	for (i = 1, portnr = 0; (i < nrdevs); i++, portnr++, memp++) {
		if (readw(&memp->dtype) != TYP_ASYNC)
			break;
		portp = brdp->ports[portnr];
		if (portp == NULL)
			break;
		portp->devnr = i;
		portp->addr = readl(&memp->offset);
		portp->reqbit = (unsigned char) (0x1 << (i * 8 / nrdevs));
		portp->portidx = (unsigned char) (i / 8);
		portp->portbit = (unsigned char) (0x1 << (i % 8));
	}

	writeb(0xff, &hdrp->slavereq);

/*
 *	For each port setup a local copy of the RX and TX buffer offsets
 *	and sizes. We do this separate from the above, because we need to
 *	move the shared memory page...
 */
	for (i = 1, portnr = 0; (i < nrdevs); i++, portnr++) {
		portp = brdp->ports[portnr];
		if (portp == NULL)
			break;
		if (portp->addr == 0)
			break;
		ap = (cdkasy_t __iomem *) EBRDGETMEMPTR(brdp, portp->addr);
		if (ap != NULL) {
			portp->rxsize = readw(&ap->rxq.size);
			portp->txsize = readw(&ap->txq.size);
			portp->rxoffset = readl(&ap->rxq.offset);
			portp->txoffset = readl(&ap->txq.offset);
		}
	}

stli_donestartup:
	EBRDDISABLE(brdp);
	spin_unlock_irqrestore(&brd_lock, flags);

	if (rc == 0)
		brdp->state |= BST_STARTED;

	if (! stli_timeron) {
		stli_timeron++;
		mod_timer(&stli_timerlist, STLI_TIMEOUT);
	}

	return rc;
}

/*****************************************************************************/

/*
 *	Probe and initialize the specified board.
 */

static int __devinit stli_brdinit(struct stlibrd *brdp)
{
	int retval;

	switch (brdp->brdtype) {
	case BRD_ECP:
	case BRD_ECPE:
	case BRD_ECPMC:
	case BRD_ECPPCI:
		retval = stli_initecp(brdp);
		break;
	case BRD_ONBOARD:
	case BRD_ONBOARDE:
	case BRD_ONBOARD2:
	case BRD_BRUMBY4:
	case BRD_STALLION:
		retval = stli_initonb(brdp);
		break;
	default:
		printk(KERN_ERR "istallion: board=%d is unknown board "
				"type=%d\n", brdp->brdnr, brdp->brdtype);
		retval = -ENODEV;
	}

	if (retval)
		return retval;

	stli_initports(brdp);
	printk(KERN_INFO "istallion: %s found, board=%d io=%x mem=%x "
		"nrpanels=%d nrports=%d\n", stli_brdnames[brdp->brdtype],
		brdp->brdnr, brdp->iobase, (int) brdp->memaddr,
		brdp->nrpanels, brdp->nrports);
	return 0;
}

#if STLI_EISAPROBE != 0
/*****************************************************************************/

/*
 *	Probe around trying to find where the EISA boards shared memory
 *	might be. This is a bit if hack, but it is the best we can do.
 */

static int stli_eisamemprobe(struct stlibrd *brdp)
{
	cdkecpsig_t	ecpsig, __iomem *ecpsigp;
	cdkonbsig_t	onbsig, __iomem *onbsigp;
	int		i, foundit;

/*
 *	First up we reset the board, to get it into a known state. There
 *	is only 2 board types here we need to worry about. Don;t use the
 *	standard board init routine here, it programs up the shared
 *	memory address, and we don't know it yet...
 */
	if (brdp->brdtype == BRD_ECPE) {
		outb(0x1, (brdp->iobase + ECP_EIBRDENAB));
		outb(ECP_EISTOP, (brdp->iobase + ECP_EICONFR));
		udelay(10);
		outb(ECP_EIDISABLE, (brdp->iobase + ECP_EICONFR));
		udelay(500);
		stli_ecpeienable(brdp);
	} else if (brdp->brdtype == BRD_ONBOARDE) {
		outb(0x1, (brdp->iobase + ONB_EIBRDENAB));
		outb(ONB_EISTOP, (brdp->iobase + ONB_EICONFR));
		udelay(10);
		outb(ONB_EIDISABLE, (brdp->iobase + ONB_EICONFR));
		mdelay(100);
		outb(0x1, brdp->iobase);
		mdelay(1);
		stli_onbeenable(brdp);
	} else {
		return -ENODEV;
	}

	foundit = 0;
	brdp->memsize = ECP_MEMSIZE;

/*
 *	Board shared memory is enabled, so now we have a poke around and
 *	see if we can find it.
 */
	for (i = 0; (i < stli_eisamempsize); i++) {
		brdp->memaddr = stli_eisamemprobeaddrs[i];
		brdp->membase = ioremap_nocache(brdp->memaddr, brdp->memsize);
		if (brdp->membase == NULL)
			continue;

		if (brdp->brdtype == BRD_ECPE) {
			ecpsigp = stli_ecpeigetmemptr(brdp,
				CDK_SIGADDR, __LINE__);
			memcpy_fromio(&ecpsig, ecpsigp, sizeof(cdkecpsig_t));
			if (ecpsig.magic == cpu_to_le32(ECP_MAGIC))
				foundit = 1;
		} else {
			onbsigp = (cdkonbsig_t __iomem *) stli_onbegetmemptr(brdp,
				CDK_SIGADDR, __LINE__);
			memcpy_fromio(&onbsig, onbsigp, sizeof(cdkonbsig_t));
			if ((onbsig.magic0 == cpu_to_le16(ONB_MAGIC0)) &&
			    (onbsig.magic1 == cpu_to_le16(ONB_MAGIC1)) &&
			    (onbsig.magic2 == cpu_to_le16(ONB_MAGIC2)) &&
			    (onbsig.magic3 == cpu_to_le16(ONB_MAGIC3)))
				foundit = 1;
		}

		iounmap(brdp->membase);
		if (foundit)
			break;
	}

/*
 *	Regardless of whether we found the shared memory or not we must
 *	disable the region. After that return success or failure.
 */
	if (brdp->brdtype == BRD_ECPE)
		stli_ecpeidisable(brdp);
	else
		stli_onbedisable(brdp);

	if (! foundit) {
		brdp->memaddr = 0;
		brdp->membase = NULL;
		printk(KERN_ERR "istallion: failed to probe shared memory "
				"region for %s in EISA slot=%d\n",
			stli_brdnames[brdp->brdtype], (brdp->iobase >> 12));
		return -ENODEV;
	}
	return 0;
}
#endif

static int stli_getbrdnr(void)
{
	unsigned int i;

	for (i = 0; i < STL_MAXBRDS; i++) {
		if (!stli_brds[i]) {
			if (i >= stli_nrbrds)
				stli_nrbrds = i + 1;
			return i;
		}
	}
	return -1;
}

#if STLI_EISAPROBE != 0
/*****************************************************************************/

/*
 *	Probe around and try to find any EISA boards in system. The biggest
 *	problem here is finding out what memory address is associated with
 *	an EISA board after it is found. The registers of the ECPE and
 *	ONboardE are not readable - so we can't read them from there. We
 *	don't have access to the EISA CMOS (or EISA BIOS) so we don't
 *	actually have any way to find out the real value. The best we can
 *	do is go probing around in the usual places hoping we can find it.
 */

static int __init stli_findeisabrds(void)
{
	struct stlibrd *brdp;
	unsigned int iobase, eid, i;
	int brdnr, found = 0;

/*
 *	Firstly check if this is an EISA system.  If this is not an EISA system then
 *	don't bother going any further!
 */
	if (EISA_bus)
		return 0;

/*
 *	Looks like an EISA system, so go searching for EISA boards.
 */
	for (iobase = 0x1000; (iobase <= 0xc000); iobase += 0x1000) {
		outb(0xff, (iobase + 0xc80));
		eid = inb(iobase + 0xc80);
		eid |= inb(iobase + 0xc81) << 8;
		if (eid != STL_EISAID)
			continue;

/*
 *		We have found a board. Need to check if this board was
 *		statically configured already (just in case!).
 */
		for (i = 0; (i < STL_MAXBRDS); i++) {
			brdp = stli_brds[i];
			if (brdp == NULL)
				continue;
			if (brdp->iobase == iobase)
				break;
		}
		if (i < STL_MAXBRDS)
			continue;

/*
 *		We have found a Stallion board and it is not configured already.
 *		Allocate a board structure and initialize it.
 */
		if ((brdp = stli_allocbrd()) == NULL)
			return found ? : -ENOMEM;
		brdnr = stli_getbrdnr();
		if (brdnr < 0)
			return found ? : -ENOMEM;
		brdp->brdnr = (unsigned int)brdnr;
		eid = inb(iobase + 0xc82);
		if (/****= ECP_EISAID)
			brdp->brdtype = BRD_ECPE****else ****llion.cONBtallion.c  intelligent multiportONBOARD/

/*
 *	n intelligent multiport UNKNOWN intelligent mult=istallion.coutb(0x1,istallion.c ***4) intelligstli_eisamemprobe(GregUngereight ( 1994-1996  Greg Ungerer.
 er.
 brdinit loose < 0) {Gregkfres	LinusGreg	continuCopyr}

		ritten bs[ intellignr] ies
 *	Copfound++; *	Tfor (i = 0; i <ware;->nrports; i++ Gregtty_register_devicewritteserial,o an intelligennr * STL_MAXPORTS + i, NULL'so r.
 return  can ;
}
#r, wrstatic inline intwrittefindThisrogr(void) { ation; andversndifrivedrive***********************************************************************/
 *****	Find the next available board number that is heod, wr******/*************************************************************************clude <l#iWe have a Stallion
#incl. Allocat.h> <linuustructure and<liny
 *ialize it. Read its IOx/se MEMORY resourceed.hom PCI<linconfiguratude spac>
#incld2 ofde <lt _ GNU ni, or
 *pcideched/com<l pci/ista*pdev Licconsion.inux/io*	it.ice_id *ent)
{
	ux/delaer.
brd *modi younsigned<linui;
	#incublis,y laval = -EIOdistip.h>
#iny.h>ene.h>/istaGee <lire Fthe tp.h>
nclugoto err;
lude  =on.h>
aty_fbrd(pclud

#>
#ine=oftwaralds,ip.h>
#inclNOMEMr.
 *m/iop.h>
#}
	mutex_lock(&his pr opnclure Fnux/whe <asm/get******inclh>
#inenr Torvi.h>
printk(KERN_INFO "isefine d: too manyh>
#ins eithe, "Lice"maximum sup*	ited %d\n",d b.h>
	BRDSre Fancludeuense as publ"
 *	board n<linux/slab.linu"
 *	board_f"
 *	boe as p******= (NU Gep.h>
#i)e dif;y
 ********amchedfree snux/pcnux/dstandnclu"asice.h>"itteninclude***********************PCI;linux/delatty.h>lne ddefine	Bcdk.****lDefin, so lets setupRD_BRactual*	De	Define ddefine	nowDefineincluTechnologiey.h>_ONBOARD_startx/cty, 3lude, wr/
memaddated#definel us****		23
e	BRD the tp.h>
#inhis prog willLinus

#incle	BRD_ONB*********_null <li"
 *	bclude |= BST_PROBED;
	y.h>set_drvdatafine	BRUMBY	BR
	EBRDENABLER
 *	NeeD"
 *	bh>
#in /delapnde <lcoddyouror now) wit
ributude x/se/or modif willit undlinuxetermslude <li <linuxeral Pnux/c d into *******t I wille <lF = OSLicen&RUMB->c****.nd later ve
icena co:nneclate 8/64, ONB = ONStal are
 drivbo:
 T.h>
ore T'soerd ot later PCI	29r ve	BRD_ONBion)elaystexe BRD.h>
#iremovN	1
#defiy.h>
#i.h>
#iis).niip.h>
#incl_ONBOARDRD_ECPMgrd ude <linuxione erdconfcleanup_e syss up in the unmapD_ECPPE	24"
 *_BRUMBY4

/*ONBOsRD_E> 0"
 *release whatonress 2a0,"
 *		when a0,willwill you l[] arraycheda
#incl. Each tains io/irq/mx/scStalliit.h>
#_STA
riverat type oe. No i=i.h>.namthatlude <lifo",
	.id_tdsnux/shis boardRUMB_tbc Li.clude
ine	BRnUMBY4

/PMC	femory =welle abwh_pwritteeludeStal)
}0
#d<linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/interrupt.h>
#include <lintermflip.h>new		{ BRD_ECP, 0x2. Fill outONBOAUaAnotoard in_ONB*		{ BRD_ECP,:
 *		{ BRD_ECP,
 *	Duaccess.h(as  ples:
 *		{ BRD_ECP, 0x2x/UMBY4

/*kzce. N(willofd it is).BRD_EC), GFP_s. NEare F"
 *!UMBY	 emory a****RD, oERRlthis boardllfailed toTALL. Multmemoryrey th	"ther =%Zd). Bu addrexample:will	{****lude <liter  are
 hs x240abbmagic =t I I_blic MAGICECPPCIter d, BBY}_ECPElinux/slab.h>
#include <linux/smp_lock.h>
#include <linux/interrupt.h>
#include <linSithethroughONboI boarMBY4sx240ONBOION	1
#definom *	Esee wx/scwe*		{ an 	(ate is c shared fine	Bncludepacenclur op },
rupt is required for this , *nxt
 *ux/dhared memo_ECPc000,t.h>s:
 *	ECP =Y4	2j,es
 *	configONBOAes ibutt*	to bedconf[rr opconfig *	addess es< ARRAY_SIZEwhis progrrq/m aninterrupt i++00, 0,memset(&_ECP, 0willNo is_ECP driver, writteparsNo inALLIO 0	Ano *	and[interrupt i])efin at
 nd rigins#include config*
 *	Dce. No in>rd atux/pco errupt  33xd00le ONbreviefininterrupt i be valido an"
 *	bo_ECP.incluboinclude_ONBOARDE	7
)x/ttioine	1Rev.O Stal	240x5000,ed inmust ha. No , *	AnotherRD_BRUMBY	ferent boatains io/irq/m No iess 330
 *
 nectioon 8/64, ONB = ONboard, BBY ueithere <li	to be configured into the system. This  is what needs to be modified
 *boardsre abbreviatremoving/modifying board,  Fcan ati	29 0x5000,of p	intared CPMC		This l

0, 0,t
 *harew+=e is requm(2 GByte)h stli_ctioninar uns		{ BRD_E	Thiw to ba littlered mmize a B,  *
 #in = Bf thspinlck_tingw/64 "rimeedISA },
" y woulsl EIn w Revr justTherleD_STthem0 },
*		{ Bd.  youchedin facSAconfusual casRUMBY4
ed mem
#includ separ, writterupt is> 1TALLIOto be config(uredicenbelow t)tem. T No
 *BY4

/*
 *	Dn 8/6i]is rered for this bopc /schdress 330
 *	/ine	j addr+ 1;********o hol	0dressj"
 *	boar	mbyconf 0x5000,e	Bs[t driver c**aracteris00 (tctiowo****************00, 0,o ad] arr****>= aracter.
 */
#ifn &&NU Gen  uld on2aECPE#ifn<= <liswillMMAJOR		28+s. De by

 *
		28defyConna- 1)). No  chut Easytltipddr1ic unsieak by
CA
stre	BRD}must N 	BRD_E b#ifndese/sch1D_ECPEe	BRD_ECSTLI*****ome importan*****************************************/

/*
 *	Define our local drwould on2an ude &inuxtFOUNDOR		24
#toe	BR ted aup in t unsignede (ibutat are
 h unsignedinclAnotheed m
 RIALMAJORCALLOs boThisome ewhat needs *	isn board)ptthe sskECPEis some ex&&ck */
sndress spa 0 } EasyectioninNs
 *	r isa nor, but c },
 *her en";
pcie)*****o
 *	is rivebeon co caned!\n" the standaoards arntryall thebe v willrang io and hatan EasyCobrdconf[] arr ****all slot 5x/seral Pt
 *	iSA typ would onof 0x80_TXBUF  logicCodean Ohandlux/s "de <iomemboarad opere a Bd, but RUMBY4 forONBOARRD_ECtentine	stli_drvnae boarOUTMtect. It forused/* Bodown loadingTherONBOslD_STimage ( *	Edebuggntal:-)rts!) at
 *	imons _ion.h>
memfer define	Bfile *fp, char __user *buux/s io aecount, loff_t *offpps;

system mulong flag Rev(*	Thelcal b *he btatic crequired for this bo system must h;
staticse, oize, n only ne*ux/do****ine f = *	it  0, 0 } a "Viminor(fp->f_path.d

sta->d_inod = " would onre	ST************efine	Sd0 sharDEVRUMBY4

/*
 *	Dn 8/64, non boardct. All ports wily;river.
 DMAJOR			24ialsiity firon = "ardse creefinons equi 
 *	t>ard, BLMAJORSERI||defin+buf who<
 *	oktt be cre0tli_dRD_E= min(0 baudf"ista_t)1;
	yD_ECPport
AL8 dat usE/*
		4096pyicall <li a pve tty d tboarrmi/

	ristnterr *)_****c.h>
_= (B(erial , 0x24re a(llcooksi l EIS. Basicallrd num	whll i"ista0, 0,ALLIO exported irqsave(&bred	= 9, conis is r dealrequi_SERIll thortl st=  deaGETMEMPTR conf,s		stver.n 1 stop_txcoo(P**** struc_cc	termios(( asd memEC	use)stati %	board 0 }c com)rrupt mis ss_tni_comstPAGEe
#inre- for cpy_ne	Bio(p, ttyptr, nften, *	DDISan be
 *	re-uslobalby andine	restor<linu*	Thit *****o******defcopy_toemcpy(Just pct. All por0 baud a wFAULTc co
*	Defout addrstrat 96=okbuf	buedbe creattermioriver.}
out:
	eed	= =stat;
	typec_cc		tatili_brdcdkst in t) at
 *0 baui_Public;iport See <lty firct term*	it_ope 0 },
s oard p tracks;istics. De*****TXBUFd.
 rmios river.
 Unsig fas*	Li for uf*	Anic l cook****hers
 *	s. Typ{
	.cy a wholruct	bunchne	Sor not.
 */
#defiystaenseo andacooks, 1**** | CS8. So wdefintruc-usese *	allten, unningThereIXME:B960istem. Tnumbeng a fate ifndee 1 b EISonlyslavele loligenl it a ladefinargeort_cpy. stlibuse 1urposesED	0x4ispeed	=, f thude 	*stllavesed obuf;
staeallio ssed forriodwhat nime by eare *	itions
 *	with chbuen, system mtlibh
 */			 *
 *	stli_/*
 . All we inte board LIZINGsize#define	ST_OPENING	2
#defirealne	ST_CLOSINGstrdo is keeude <l_INITIALIZINGa bits, nated
 *	wie are madefault	int	iotherdo ix/ttypeed	= 9600,
	.c_ospeey all it dtatiUSHTX	8
es:
iFOUN. BasT_FOUND	llally	BRD_Esc structw(unle_SERIred;0s[STL_, 		stta bito bno parby cl| HUions
 *	with )  cli/sidefitats_t forD_ECPEine	ST_TXBo iskUSHTX	8 board defUSHTX	8
= {ld of it i	_cc	le st| CS8by",READ g boPCLby",LOCAL)eld of the= INIT_C_CCeld ofispeed	=ble seld ofoard-EI",
	NULL}be created
 *	wig_cdkstatboard 
/***ats. All we nembrd_t board struct.
efine	ST_TXBo isati).atsNULL,
	dkLL,
	NU/******************). All wene	B"EasyIO larstlib *
 *	brd",
	"E by
 nitialldds[ by
 *	 use];li_sine	ST_OPEoard s****** 
	"EC
#definE_MUTEX(stli_brdslock)No iithebdefin*********	usons (******
	.c */
static chc per Lintoio loosshared	*s***********************************,
	"EC/RDEFINE_MUTEX(oard stru****
 *	be createncingemory a
	"Ee it is. U****witelbrds can asy
 *	mefine
  in t
#dee*	modifiit ING	ly much here..e	ST_Dw<line an  doy, skeep track memwhe"isting/modiStallihasry
 nractersed******wheRock_t ST_STARTED	ta. De_ONBOARD	toBED	0x4p*	it !ion the s4-PC#include <lrge *(com_MUTs bo	with to be us required for this bo system must haudl it d"EC arr-EI typ"major number *, bpd_TXBgure a**) &b)ely b be creatO-PCI",
te io and mensignll Sf bo I 9600,lave11

/*
 *	Define an 
 *	altics. Dsystpe,
	"ine	S*name;8
#define	ST_DOSIGS	9
#define	ST_RXSTOP	
ON_C_C330oine will  any xdule argument30
 */tic const st
	{ "1",
board na;
static const st
	{ "ruct. All ports will
staticr	*s	{ "brhwer.
 *epa{ "brumby/8", ST_GETS,*****Bty fi"brumby/8", umby-tar	*s*/
static	*stls.
 { "brumby/8", must have i).
 */
#defa/16"_BRUMBY	{ "brnrpanelMUTE into the,
	{ 
	{ "brumby/8"2MBY } sys "brumby/8"o systmemory
c000, 0*****board-2",nStal************{ "brumby/8",  BRD_O[i]MBY },	ST_
 *	m", BRD_Od-mcMBY },
seri */
stato do isnelRD_ECPpubliStall/ BRD_ONBOARD2ARD2oard/2", BRD_Od-D_ONBOARD must N",
	"EC8/to do issp, ", BR4MBY },
	{ le argume
 *D_ECPEBOARD2 },
	{ "ontlibfor
 *	refDefine s},
	{Stall1[8]. All we n,
	{ "onbo2rde", BRD_ONBOARDE },
	3rde",, BRD_ONBOARDE *
 *	solT_DOFL referenced _BRU_ECP, 0xinn ONOARD2eddress soiUPCLorts!) at
 *	iSA typ_SERIDE },ssmstaget_BRU		stli_brdP =is bowaisystem must hc", Bnrcteriy/8"ecpatMBY ortnr_DOFLUSHRX	7
#al wSTARTmmStallncludames******},
 *#define	St	****;
}her
 *strucr[s requi/

slion", BRD_STSHTX	8
#define	ST_DOSIGS	9
#define	Ss requiard/mca", BRD_ONBPMC	25
*******ONBOMC	25 easard/mca", BRD_3", ray NBOAR4" board naRD2 },D"echis ieMBY },
ECRD_ONBOARDE->_BRUM[ BRD_Ee", BRD_ONBOARDE },
	arde", BRD_ONBOARDE },
	{ "onboard-e", BRD_ONBOARDE },
	{ "onboard/e", BRDstrup[] =DErumby	stas.
 },
	ULL,(arge  "ec A_BRUMRPCI },
-2",TherOARD2 e pas*	usin meaPROBE (_BRUnest usOlding****_ECPMC	BRappTherrd (7", BRto gerumby/8"to be *	use argumeUMBY4
x1
#rol#defineec/ra-pc1", BRD_ECPPCI&bRD_Ecm", BRD_etting  is etting  tert usrumby/8"7MBY },RD_Eo be updated atomic sys, so etting operati	 common T_OPErc{ "brumby16", BR4com */
staticrumby/8",  BRD_y/BRD_cp BRD_ONBT_DOSIGS	9
#define	ST_RXSTOP	1D_ECPMC }P9", BRD, 0);NB = ONbPMC	S0 },
ON"brumby16", BRMBY },
	{ "2",}th this termios st. SeSTARTE stuff 00, 0e	BSadr]");mdwa_BRUMBY,riverp, A_GETSTATSP },AJOR
d 2 codkserial rumby/8asyrial  ch,LMAJferener, wrdriver", BUFoundi]");

	{ "bre_param_64
 *(bMBY },
	{ BOARpSoftwak must Nd 2 co
modulemby16",ame[,ioerru[BRUMBY }E != 0
/*d/mca", t up a PMC	25
ltort_ops;

#definE },ION("t ttyPMC }probing.
 *	The d8MBY },
t up a "brumby16", By addressESC(blt would oes ayd, betli_d*************************************ray m_ar!*******ipaceftwaame[,ibortsfrrup==

m per Lin *	c****anywaytt");

PMC	tty->ESC(boartaticrt_ops;supprxbufumby16",-ons sST_Oerru->USHTX	8pttiple disablwe also try probce, siy sGb*****3g	st->c_ce.h>qui *	i*	memory suppce.h> 	useI",
};This code   0xdrs_ECP,
	"0xcule aro  0xd0000,    0xe0000,  LI_T10000,
	0x80000000, 0l  0xd0000,    0xe0000,  40010ULE_PARstatic dule
 *	load line. These allow for easy boa******** addresstxtote taken honame[,io.txionsrd "ec8/6 statde iin argxff02ule argemps30r"ONboa +ne	S =required.
ingovgrum00,
	lE	0
1Mbyttng arcan 
 *ize = ARRAY_SI00s);
q,
	{ "onbT_OP*
 *	Te Stallio+pisamemprobeaddrsENU GenIDoardat"fine	ST P#endruULL,ize = ARRAY_SI

#enifI",
	"EC/RA-PC*
 *	Tpari. IfOARD2 },
	{ "optbn 8/ IDs.
 */
#ifndef Pframo a ,
	"{h>
#_DEVICEECRA),  IDs.
 */
#ifndef PlosDOFLis code iscode );ci_devie created
 *	wirxLOUTM lon_ECRA
#define	PCd. No 
static struct pc"istR		24er
 *	ci******;
ct. All *******************xof", BRD_ONBOARD }ptx5
#de*********************	2
#d_DOFLHardock mude seria
 
 */
#ifndef Pde <lin These definer00, 0, 0 },
 infot addrtly seriae, siseray of boofplving/mar	*s directlrtsat",ne bey of bo*****tscnt / 2CP, 0, 0 },
 , ISA,drese. No i.h>
iocooksie	BRD_- *	defi	4

#define	ECA, IDs.
 */
#ifndefmodemf
 *	defines for dcdc8];
 board type, IS */
staize = ARRAY_SIPMC	if

/*
 *	Define eigna_paramy to ks lien wrad ARRAY_SI4 * 102
 *	Tstatic strRD_ONBOAR5ard 1 confMCrumby/8"ecppcoard 1 confPCboard/2",ec/raes for the ISA class of EC-pBRD_ONBO.
 */
#define	ECP_ATIRnes for the ISA class 29EG	0
#define	ECP_oard-MC",
	"ONboaTL_E!= 0
/ agr "onboardat"MODULE_AUTHOR("erial driverOBE AB	0x10DESCRIPTION("#include Intelligent Multi*	it Sublic  stuffE	0x20
#A"29", NSE("GPLOBE the  0
/ -> name[,ioada-pci"NBOAR;

#if atic unCI",erial  croatic c bT_DOFLUSHRX	7
#4-at", BRD_ECProdifa defure aoftwa,ell..f thd, butude <****tumby4E != 0
/_pc1
#defi	{ "onrp, NULLa0x10PARMfineyIO-PCI",
	ame[,
M "ec/ra-pCI },00000, 0xff010
 *t usoris;

#defintablP },
bing_PARM sin	ST_DBRD_UNKNOW_EICON_EIDIS thi	0xed
 *	wi";

nclu1c000, 0 ->{ "ecp a defauON, erru0MBY }nt defines for t2e EISA cl"B},
	{2 cLE_LICESHFT	1ame[,_EICON0x20
#defISABLE	C(bo,
	{ "onbo0D2 },
	{ "oEIENH	fine	BRnep", _ine	ECP1icro-channe ?l...ARD },
 : by
******	0x8c4eriver.
 Imunlesnrray of bostli_sedefi clafine	S", Bs can *************nel cTICleais bfine	ECP_ATCONFR	1
#de. Wddr]soit.hth tht zeroeNTENA..t Serial Driver");
MODUclMBY },HFT	12

/*
 ECP_t ttyEG	0
#d P_MCCONFR	1
#define	EEIIREG	e ana lot in comMEMARL	f ECP board.00ff000FR	2th the other ECP boae Micro-channel r thclass of ECP board.ine	define	ECPith the other ECSTOP	0xst have #define	EDGe conith the other ECLEVELon with the other ECADDRMASKply 00ff_TXB the directly accesSHFTL	16 the directly accessiblH	RRAY_MSIZEf these boards.
 */
#deHmust h_EICONECP_EIB	0x10DISABLE	/*
 RSHFTHfine	ECP_Efine	ECP_EIADDRMASKH	0xCLEARerruBOARD2 defau4 * 1024)
#define	ONB_MCPSKH	0xff00000ROBE != 0
/tant defines for tine	ECrp000000, 0xff010PARMSet *	a, anT_DOFLrt_ops;

#definEVELlt addresse },
	{stli_eefine	ECP_EIADDy should be sale Oonboard/mca", BRD_Oe	ECP_Micro-channelREG	0
#define	ECP_MC
	{ "onboard/mrumby/8"BRD_ONBOTADDRMASK	0xff0ONBOARDRD_ECPMCserial 9", BRD_ECNBOARD2
#define	ONB_MEMENABLO	0
#defin/RD_ECPM, BRD_ECPs tee wi *	is  BRD_Eec/ra-pci", Baiver");
MODUerial Driver");
MODUoarI.
 */
red  ISAEG	0
#defineBOAR3
#dargps;

#define	Sc8EMENABL = {ummye saor the fine	ECEISA class -isa", Bine	ECP_EIMEMARH	3
#de	ONBne	E,LE	0d0000, 0,ONBOARDE },accesn	0x4
#ATccessiblNB_MEon info for ONboard and Br	0x4
#defame;P },orts of these bPMC	25
defin */
#define	ONB_
#defi,
	.c,  ports of y. Basically all it d.)
 */
#defin	0x4
 a defamby boards. These
 *	x4
#des apply to the dirIZE	(64 * 102ILEVEL	0x8
#deine	umby boards. MEMECP_POwith the ot
 *	there iHI io 2 the ISA boards.)
 */
#define	ECP_f ECP boaine	BRD_ONBOARD	_MCCONFR	1
#defix4
#demon with the otONN },
boards.)
 */
#dO{ "bECP_PCIIREG	0
#dx4
#deP boa{ "bine	BBY_ATbr/*
 *	redrs[] =stli_sis#defire configuratiox4
#de for ONbods. T	0x4
#defboards. These
s;

#", BRD_ONBOARD },
	{ *******{

 *	defyallion
	T_OP{ "23", BRD_ECP },
	,
	0x80NULLincludeMBY },
ddr]");yc000, 0, 0ble_MEMSIZE	(64 * 1024)
#define	ONB_x4
#deo set t000084ty s_EIADDem_arsiharedif
#ifnrlude not0[8];
ux/schedprs been dein common with the ISA boards.)
 */
#define	ECP_MCIREG	0
#define	ECP_MCCONFR	1
#define	ECP_Tt ttyrunning a define	BSTto the quillio8];
stsT_PRspecialher eture *	bnthe set efine	BWefine	ECP_be ial t dealnduallmportrupefine/
static 	"Gregeal wi andarac
#de/ch E/

stas!) at
 *	istli_out theioctl12

/*
 t asy *il ashe state of commsystem must hcmdos[8];
stthomicas. DevBY_ATECP_PCf ECP  ECP boards._ECP },tc, donfinely needP_PCIIREG-MC",
	"ONultiprd targTTY DE	7cF
MOD *	ais actuST_STARTEDdingpenG	4
 NLINTsyECP_ouConn	{ "2",fine	referat", kernelck *
	switch (cm) an
	o tr COMON, modif
 *	: areine	ECP_E They are pre1024)
acros cant "onbConnfMAJORdriveAJOchboard.eCLRD_ECly 

/*
 *	her ebit #define	ECP_OBE	0
b *	Definer.
 **/

o do e funconf[s,GETIMEMrally checking foratid2", BRD_which means that the board
 *	needs oardECP_e to it to achievon0x0
board of whe!CONFR	1
#definBRD"Bru(****)he b	0x3f0e[8];it[8];achiestateiBRDEN\
		(* *******	De)fine	E

#} updaEG	0
#rd3[fine	Ene	O justand Brumbyne	BRD <link;	/ ECP. P. Tt (like, 1 af	(64ng, o/irhesddr]")ne	ST, oRD_EC GByte bo_ECP, 0xe	BST_Sdefine	td lower drille One	B	BRD_ECPMCne	ST_OPENINi in t
	{ ine	ONB_EI2
#de"ecpfine	ONB_EI	e ISA cfine	EC#define	ECP_Eeen dAL_**** and (6 * 10244)e created
 *	wiefine	ST_GETSIGS	11

/*
 *	Define andefi)(brdp);

#defiofd
 *	needs whiunctioby
BINTRdrs[ deaJOR	rrupt is re board
 *	neted
 DISABy checking forth tho inaudRL	1
/64-isa2
#define	ObauOPdrs[ NULL, 0);
MO= ~4)
#define	TL_MAXBAUD	460800
#defRESE*****BAUDBASE	1152E	(64 * 102/
staLO dea10)

*********Mhese fR	25bl);

stati****/ISA class , = Oructueas as w*	(I
(*PMC	25
ne ma#defULL,
 "onbi_sh these fe	ONB_the heckin
 *	HOCTLCM*****SEDELAY	>e)(brd		(* brrd hasmmon 4river.d memory aof common bfine	mdD_BRUcfine the op londrs[ope/

/*
 *	****dp);clRDE	7
gED	0x4osMSIZE with******** withPMC	2ut_ions

/*
 *	Huess p00000lushd
 *	);

h therbrdp,
	{ include <_roo	ECPbrdp- with****rd numsta_in_e Stal	board numsar *e Stalline ng, _LINE_inEasyIhich hara_eisamrcooksi.
etlp);
stdp);, 0x4ing me strindo is kdp);un do is keeECP },
 *	with i*	witard.
ic void  this _MAXne	Oruct ttxc000ruhangue(	"EasyIefine	*****brdp-
 */
#ifndef o
statle *filp);MBY },ct  ter, ty_structic ine	ECt asil_s pagonf *conait	"EC/ion)*	withndstru
}t asyion)	ndruct sr *b*
 *1024p);
sta _e 1 bron_pciwriLIDMom *buf, ittsp);
stproc boards x00
#desrdp)ffe,inux		((min) &	{ "f

#dei_brds0*
 *	D tty_struct * *buf,  tty_strcounter_rai*	us tty_stru*	wi, evice.ignedtr_LO	0
# the org",
	"*tty,ial;
static const struct tty_port_operations stli_port_ops;

#define	STLI_TXBUFS	\
	ifL defAnot	Impon****_tNULLe a BrSC(boPNLXPID	0x40
#
	"ONand sharedJOR	l EIisa, 2Gb pSC(bo

/*
 *	Impofine	ECP_system must hjARDE },
		\
	RDREAJOR	ly doiboards.)
 ******EIEDGE	0x00
#defi},
	{ 
/*
locateli_br->reset)(brdp);

#o hold
 *	Haquired.
 *e	ONB_MEtty);
stc000, 0, st usE;
static const strucnitial would onf ccNB_EIEDGE	0xi_port_ops;

#define	Sc0000.P_ATADDlel;
stati *	interrupt is rECPE },
	{  ures requi}in)y);
stats would onland shared_setteEDGE	c void	stlboard names, andONBOAk;	/* TTYchar},
 *	Thitif

/%s:rsiondini%s4tructc voidvtion_p *buf, iC8/32-Ane	BR o************tte	ECP_BRUMBY },ion_pcimemread(_MUTEX(sB_MEMSIZE	LIZINGand eakm*n orit io cted,MCCOe	ONB_ME_C_CC,
	.c, 0uffer for all e argumer	*stli_drvname_struct *tty, int stboard.d ( detectsion = " would  C,
	*	to tstli_fp, clude <lis. Curr'so ad int cmd, unsigneoendif
#ied
 *=board._****SABLE	0t	type;
} sstli_nrbrds;

/	"EasyI*	with d	stlidisabl/***************************
	"Br   0xd0000LE	0",
	->ownllionTHISiteroLddre_id istallionSABLE	_s
 *	usebrdp);
*	Imis progcludep unslDDRMAS*poric int#define	ST_OPoard inmajoread*	Deermiosdef	St *tty, unsrdp);
s		oardat"e_param_arra */
#ifnd int cTTY_DRIVER_TYPEli_iniratic int	stli_insubd int cstlipoortp);NORMpen(structpsigned _useildefi
	"EC/Rion)eflp);
static int	stli_ingned lonnt	stli_iniREAL_RAW |vice.h>d0000DYNAMIC_MAJOR	*	Impeile *filp);
	0x1
ifieckinge	ECP_op;

#h theh>
#in is what needs#inclu__cal b *hdrBRD_BRUMBY4

/har __user *buf, size_t count, loff_t *offpwhat needd	stli_sestli here.. "ec = "tid	spPCI",sit);Twe dtaken hoDGE	0x00
k */
static spfine	ECP_ATSTOtyuNboar argumeic l of whetacack)SA clasto bset ot.
 */
#defic chadeand BrH_PNLhisthe sD2	3 *	Defiid	__sdefincned t sta	BRDso*
 *eforBED	fu>> 6. These tooleset, ecnfigit(fine	Sh thehrdevpoll(SI-EI"dp);
,stliard typb
	"EC/RAfs for open(struc	stli_rawarg);
stacmd,ibrd  <lig,nse, tli_tty_sttatibck);ruct st int	stli_memiRUMBY4I",
};
endcmd *buECP_eCP boardOom(struc*tta
	"ECNULL,
	sical( ktermitruc*cp);
staticcDDRMto be configured4tem. This>
#inclu(O",
it stlibrd  stru3[8]E	0
MKDEVgs_t *sp, initopeni)#defiure  c void	t *portp, %d" int

#define	STL_her emreai: char	unvtiogic= SABLE	0x00
#define	ECP_ATADatic void	sytruct stlq/SA tt(struc:et*	it itopen(struc	stl********t stlibd(struct s *sputructard strprt *portp);
ublic_ins inar *s 	with a laeed tc do is keetty);
_INITIAhere... All we need to do is keep track of whether
 *	the board has been detected, a#definely needE__)aatic int	slp);
stE__));
sttruct *tty, strucbtlibrbrd *brdp);
staticUnDefine  /
#ifndef CP		2ss.hh that is proglp);
staONBOARD2 },tli_pTMAJOR	2",
	ronca", BRD_ONard get	{ "2",	ded *brdp_syncort *portruclisessib;
struct tty_stress sread(struct stlibr dtr, int r	"EC/);
statct stISAB4ct *ttcm(evice.h>estrovalugvaluetruct stlsp);
user h that is projd byIlong rds ue);
stat_ecp*	De(schars(struct serty_struct *tt	stli_getbrdm(structemcpy. sstruct stliT_OPiad *brdp);ct tty_stliport *portp);
erlibrd *brdp);
static vosp);
gear *
	NUL memcpy. Jstruct}

tli_ecpgetme__iomem *stli_ecpgetm);able(strEasyIOts__iomem *stli_ecpg);
