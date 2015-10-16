/*****************************************************************************/

/*
 *	stallion.c  -- stallion multiport serial driver.
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
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*****************************************************************************/

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/seq_file.h>
#include <linux/cd1400.h>
#include <linux/sc26198.h>
#include <linux/comstats.h>
#include <linux/stallion.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/smp_lock.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/ctype.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#include <linux/pci.h>

/*****************************************************************************/

/*
 *	Define different board types. Use the standard Stallion "assigned"
 *	board numbers. Boards supported in this driver are abbreviated as
 *	EIO = EasyIO and ECH = EasyConnection 8/32.
 */
#define	BRD_EASYIO	20
#define	BRD_ECH		21
#define	BRD_ECHMC	22
#define	BRD_ECHPCI	26
#define	BRD_ECH64PCI	27
#define	BRD_EASYIOPCI	28

struct stlconf {
	unsigned int	brdtype;
	int		ioaddr1;
	int		ioaddr2;
	unsigned long	memaddr;
	int		irq;
	int		irqtype;
};

static unsigned int stl_nrbrds;

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

/*
 *	Set the TX buffer size. Bigger is better, but we don't want
 *	to chew too much memory with buffers!
 */
#define	STL_TXBUFLOW		512
#define	STL_TXBUFSIZE		4096

/*****************************************************************************/

/*
 *	Define our local driver identity first. Set up stuff to deal with
 *	all the local structures required by a serial tty driver.
 */
static char	*stl_drvtitle = "Stallion Multiport Serial Driver";
static char	*stl_drvname = "stallion";
static char	*stl_drvversion = "5.6.0";

static struct tty_driver	*stl_serial;

/*
 *	Define a local default termios struct. All ports will be created
 *	with this termios initially. Basically all it defines is a raw port
 *	at 9600, 8 data bits, 1 stop bit.
 */
static struct ktermios		stl_deftermios = {
	.c_cflag	= (B9600 | CS8 | CREAD | HUPCL | CLOCAL),
	.c_cc		= INIT_C_CC,
	.c_ispeed	= 9600,
	.c_ospeed	= 9600,
};

/*
 *	Define global place to put buffer overflow characters.
 */
static char		stl_unwanted[SC26198_RXFIFOSIZE];

/*****************************************************************************/

static DEFINE_MUTEX(stl_brdslock);
static struct stlbrd		*stl_brds[STL_MAXBRDS];

static const struct tty_port_operations stl_port_ops;

/*
 *	Per board state flags. Used with the state field of the board struct.
 *	Not really much here!
 */
#define	BRD_FOUND	0x1
#define STL_PROBED	0x2


/*
 *	Define the port structure istate flags. These set of flags are
 *	modified at interrupt time - so setting and reseting them needs
 *	to be atomic. Use the bit clear/setting routines for this.
 */
#define	ASYI_TXBUSY	1
#define	ASYI_TXLOW	2
#define	ASYI_TXFLOWED	3

/*
 *	Define an array of board names as printable strings. Handy for
 *	referencing boards when printing trace and stuff.
 */
static char	*stl_brdnames[] = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
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
	NULL,
	NULL,
	NULL,
	"EC8/32-PCI",
	"EC8/64-PCI",
	"EasyIO-PCI",
};

/*****************************************************************************/

/*
 *	Define some string labels for arguments passed from the module
 *	load line. These allow for easy board definitions, and easy
 *	modification of the io, memory and irq resoucres.
 */
static unsigned int stl_nargs;
static char	*board0[4];
static char	*board1[4];
static char	*board2[4];
static char	*board3[4];

static char	**stl_brdsp[] = {
	(char **) &board0,
	(char **) &board1,
	(char **) &board2,
	(char **) &board3
};

/*
 *	Define a set of common board names, and types. This is used to
 *	parse any module arguments.
 */

static struct {
	char	*name;
	int	type;
} stl_brdstr[] = {
	{ "easyio", BRD_EASYIO },
	{ "eio", BRD_EASYIO },
	{ "20", BRD_EASYIO },
	{ "ec8/32", BRD_ECH },
	{ "ec8/32-at", BRD_ECH },
	{ "ec8/32-isa", BRD_ECH },
	{ "ech", BRD_ECH },
	{ "echat", BRD_ECH },
	{ "21", BRD_ECH },
	{ "ec8/32-mc", BRD_ECHMC },
	{ "ec8/32-mca", BRD_ECHMC },
	{ "echmc", BRD_ECHMC },
	{ "echmca", BRD_ECHMC },
	{ "22", BRD_ECHMC },
	{ "ec8/32-pc", BRD_ECHPCI },
	{ "ec8/32-pci", BRD_ECHPCI },
	{ "26", BRD_ECHPCI },
	{ "ec8/64-pc", BRD_ECH64PCI },
	{ "ec8/64-pci", BRD_ECH64PCI },
	{ "ech-pci", BRD_ECH64PCI },
	{ "echpci", BRD_ECH64PCI },
	{ "echpc", BRD_ECH64PCI },
	{ "27", BRD_ECH64PCI },
	{ "easyio-pc", BRD_EASYIOPCI },
	{ "easyio-pci", BRD_EASYIOPCI },
	{ "eio-pci", BRD_EASYIOPCI },
	{ "eiopci", BRD_EASYIOPCI },
	{ "28", BRD_EASYIOPCI },
};

/*
 *	Define the module agruments.
 */

module_param_array(board0, charp, &stl_nargs, 0);
MODULE_PARM_DESC(board0, "Board 0 config -> name[,ioaddr[,ioaddr2][,irq]]");
module_param_array(board1, charp, &stl_nargs, 0);
MODULE_PARM_DESC(board1, "Board 1 config -> name[,ioaddr[,ioaddr2][,irq]]");
module_param_array(board2, charp, &stl_nargs, 0);
MODULE_PARM_DESC(board2, "Board 2 config -> name[,ioaddr[,ioaddr2][,irq]]");
module_param_array(board3, charp, &stl_nargs, 0);
MODULE_PARM_DESC(board3, "Board 3 config -> name[,ioaddr[,ioaddr2][,irq]]");

/*****************************************************************************/

/*
 *	Hardware ID bits for the EasyIO and ECH boards. These defines apply
 *	to the directly accessible io ports of these boards (not the uarts -
 *	they are in cd1400.h and sc26198.h).
 */
#define	EIO_8PORTRS	0x04
#define	EIO_4PORTRS	0x05
#define	EIO_8PORTDI	0x00
#define	EIO_8PORTM	0x06
#define	EIO_MK3		0x03
#define	EIO_IDBITMASK	0x07

#define	EIO_BRDMASK	0xf0
#define	ID_BRD4		0x10
#define	ID_BRD8		0x20
#define	ID_BRD16	0x30

#define	EIO_INTRPEND	0x08
#define	EIO_INTEDGE	0x00
#define	EIO_INTLEVEL	0x08
#define	EIO_0WS		0x10

#define	ECH_ID		0xa0
#define	ECH_IDBITMASK	0xe0
#define	ECH_BRDENABLE	0x08
#define	ECH_BRDDISABLE	0x00
#define	ECH_INTENABLE	0x01
#define	ECH_INTDISABLE	0x00
#define	ECH_INTLEVEL	0x02
#define	ECH_INTEDGE	0x00
#define	ECH_INTRPEND	0x01
#define	ECH_BRDRESET	0x01

#define	ECHMC_INTENABLE	0x01
#define	ECHMC_BRDRESET	0x02

#define	ECH_PNLSTATUS	2
#define	ECH_PNL16PORT	0x20
#define	ECH_PNLIDMASK	0x07
#define	ECH_PNLXPID	0x40
#define	ECH_PNLINTRPEND	0x80

#define	ECH_ADDR2MASK	0x1e0

/*
 *	Define the vector mapping bits for the programmable interrupt board
 *	hardware. These bits encode the interrupt for the board to use - it
 *	is software selectable (except the EIO-8M).
 */
static unsigned char	stl_vecmap[] = {
	0xff, 0xff, 0xff, 0x04, 0x06, 0x05, 0xff, 0x07,
	0xff, 0xff, 0x00, 0x02, 0x01, 0xff, 0xff, 0x03
};

/*
 *	Lock ordering is that you may not take stallion_lock holding
 *	brd_lock.
 */

static spinlock_t brd_lock; 		/* Guard the board mapping */
static spinlock_t stallion_lock;	/* Guard the tty driver */

/*
 *	Set up enable and disable macros for the ECH boards. They require
 *	the secondary io address space to be activated and deactivated.
 *	This way all ECH boards can share their secondary io region.
 *	If this is an ECH-PCI board then also need to set the page pointer
 *	to point to the correct page.
 */
#define	BRDENABLE(brdnr,pagenr)						\
	if (stl_brds[(brdnr)]->brdtype == BRD_ECH)			\
		outb((stl_brds[(brdnr)]->ioctrlval | ECH_BRDENABLE),	\
			stl_brds[(brdnr)]->ioctrl);			\
	else if (stl_brds[(brdnr)]->brdtype == BRD_ECHPCI)		\
		outb((pagenr), stl_brds[(brdnr)]->ioctrl);

#define	BRDDISABLE(brdnr)						\
	if (stl_brds[(brdnr)]->brdtype == BRD_ECH)			\
		outb((stl_brds[(brdnr)]->ioctrlval | ECH_BRDDISABLE),	\
			stl_brds[(brdnr)]->ioctrl);

#define	STL_CD1400MAXBAUD	230400
#define	STL_SC26198MAXBAUD	460800

#define	STL_BAUDBASE		115200
#define	STL_CLOSEDELAY		(5 * HZ / 10)

/*****************************************************************************/

/*
 *	Define the Stallion PCI vendor and device IDs.
 */
#ifndef	PCI_VENDOR_ID_STALLION
#define	PCI_VENDOR_ID_STALLION		0x124d
#endif
#ifndef PCI_DEVICE_ID_ECHPCI832
#define	PCI_DEVICE_ID_ECHPCI832		0x0000
#endif
#ifndef PCI_DEVICE_ID_ECHPCI864
#define	PCI_DEVICE_ID_ECHPCI864		0x0002
#endif
#ifndef PCI_DEVICE_ID_EIOPCI
#define	PCI_DEVICE_ID_EIOPCI		0x0003
#endif

/*
 *	Define structure to hold all Stallion PCI boards.
 */

static struct pci_device_id stl_pcibrds[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_STALLION, PCI_DEVICE_ID_ECHPCI864),
		.driver_data = BRD_ECH64PCI },
	{ PCI_DEVICE(PCI_VENDOR_ID_STALLION, PCI_DEVICE_ID_EIOPCI),
		.driver_data = BRD_EASYIOPCI },
	{ PCI_DEVICE(PCI_VENDOR_ID_STALLION, PCI_DEVICE_ID_ECHPCI832),
		.driver_data = BRD_ECHPCI },
	{ PCI_DEVICE(PCI_VENDOR_ID_NS, PCI_DEVICE_ID_NS_87410),
		.driver_data = BRD_ECHPCI },
	{ }
};
MODULE_DEVICE_TABLE(pci, stl_pcibrds);

/*****************************************************************************/

/*
 *	Define macros to extract a brd/port number from a minor number.
 */
#define	MINOR2BRD(min)		(((min) & 0xc0) >> 6)
#define	MINOR2PORT(min)		((min) & 0x3f)

/*
 *	Define a baud rate table that converts termios baud rate selector
 *	into the actual baud rate value. All baud rate calculations are
 *	based on the actual baud rate required.
 */
static unsigned int	stl_baudrates[] = {
	0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800,
	9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600
};

/*****************************************************************************/

/*
 *	Declare all those functions in this driver!
 */

static int	stl_memioctl(struct inode *ip, struct file *fp, unsigned int cmd, unsigned long arg);
static int	stl_brdinit(struct stlbrd *brdp);
static int	stl_getportstats(struct tty_struct *tty, struct stlport *portp, comstats_t __user *cp);
static int	stl_clrportstats(struct stlport *portp, comstats_t __user *cp);

/*
 *	CD1400 uart specific handling functions.
 */
static void	stl_cd1400setreg(struct stlport *portp, int regnr, int value);
static int	stl_cd1400getreg(struct stlport *portp, int regnr);
static int	stl_cd1400updatereg(struct stlport *portp, int regnr, int value);
static int	stl_cd1400panelinit(struct stlbrd *brdp, struct stlpanel *panelp);
static void	stl_cd1400portinit(struct stlbrd *brdp, struct stlpanel *panelp, struct stlport *portp);
static void	stl_cd1400setport(struct stlport *portp, struct ktermios *tiosp);
static int	stl_cd1400getsignals(struct stlport *portp);
static void	stl_cd1400setsignals(struct stlport *portp, int dtr, int rts);
static void	stl_cd1400ccrwait(struct stlport *portp);
static void	stl_cd1400enablerxtx(struct stlport *portp, int rx, int tx);
static void	stl_cd1400startrxtx(struct stlport *portp, int rx, int tx);
static void	stl_cd1400disableintrs(struct stlport *portp);
static void	stl_cd1400sendbreak(struct stlport *portp, int len);
static void	stl_cd1400flowctrl(struct stlport *portp, int state);
static void	stl_cd1400sendflow(struct stlport *portp, int state);
static void	stl_cd1400flush(struct stlport *portp);
static int	stl_cd1400datastate(struct stlport *portp);
static void	stl_cd1400eiointr(struct stlpanel *panelp, unsigned int iobase);
static void	stl_cd1400echintr(struct stlpanel *panelp, unsigned int iobase);
static void	stl_cd1400txisr(struct stlpanel *panelp, int ioaddr);
static void	stl_cd1400rxisr(struct stlpanel *panelp, int ioaddr);
static void	stl_cd1400mdmisr(struct stlpanel *panelp, int ioaddr);

static inline int	stl_cd1400breakisr(struct stlport *portp, int ioaddr);

/*
 *	SC26198 uart specific handling functions.
 */
static void	stl_sc26198setreg(struct stlport *portp, int regnr, int value);
static int	stl_sc26198getreg(struct stlport *portp, int regnr);
static int	stl_sc26198updatereg(struct stlport *portp, int regnr, int value);
static int	stl_sc26198getglobreg(struct stlport *portp, int regnr);
static int	stl_sc26198panelinit(struct stlbrd *brdp, struct stlpanel *panelp);
static void	stl_sc26198portinit(struct stlbrd *brdp, struct stlpanel *panelp, struct stlport *portp);
static void	stl_sc26198setport(struct stlport *portp, struct ktermios *tiosp);
static int	stl_sc26198getsignals(struct stlport *portp);
static void	stl_sc26198setsignals(struct stlport *portp, int dtr, int rts);
static void	stl_sc26198enablerxtx(struct stlport *portp, int rx, int tx);
static void	stl_sc26198startrxtx(struct stlport *portp, int rx, int tx);
static void	stl_sc26198disableintrs(struct stlport *portp);
static void	stl_sc26198sendbreak(struct stlport *portp, int len);
static void	stl_sc26198flowctrl(struct stlport *portp, int state);
static void	stl_sc26198sendflow(struct stlport *portp, int state);
static void	stl_sc26198flush(struct stlport *portp);
static int	stl_sc26198datastate(struct stlport *portp);
static void	stl_sc26198wait(struct stlport *portp);
static void	stl_sc26198txunflow(struct stlport *portp, struct tty_struct *tty);
static void	stl_sc26198intr(struct stlpanel *panelp, unsigned int iobase);
static void	stl_sc26198txisr(struct stlport *port);
static void	stl_sc26198rxisr(struct stlport *port, unsigned int iack);
static void	stl_sc26198rxbadch(struct stlport *portp, unsigned char status, char ch);
static void	stl_sc26198rxbadchars(struct stlport *portp);
static void	stl_sc26198otherisr(struct stlport *port, unsigned int iack);

/*****************************************************************************/

/*
 *	Generic UART support structure.
 */
typedef struct uart {
	int	(*panelinit)(struct stlbrd *brdp, struct stlpanel *panelp);
	void	(*portinit)(struct stlbrd *brdp, struct stlpanel *panelp, struct stlport *portp);
	void	(*setport)(struct stlport *portp, struct ktermios *tiosp);
	int	(*getsignals)(struct stlport *portp);
	void	(*setsignals)(struct stlport *portp, int dtr, int rts);
	void	(*enablerxtx)(struct stlport *portp, int rx, int tx);
	void	(*startrxtx)(struct stlport *portp, int rx, int tx);
	void	(*disableintrs)(struct stlport *portp);
	void	(*sendbreak)(struct stlport *portp, int len);
	void	(*flowctrl)(struct stlport *portp, int state);
	void	(*sendflow)(struct stlport *portp, int state);
	void	(*flush)(struct stlport *portp);
	int	(*datastate)(struct stlport *portp);
	void	(*intr)(struct stlpanel *panelp, unsigned int iobase);
} uart_t;

/*
 *	Define some macros to make calling these functions nice and clean.
 */
#define	stl_panelinit		(* ((uart_t *) panelp->uartp)->panelinit)
#define	stl_portinit		(* ((uart_t *) portp->uartp)->portinit)
#define	stl_setport		(* ((uart_t *) portp->uartp)->setport)
#define	stl_getsignals		(* ((uart_t *) portp->uartp)->getsignals)
#define	stl_setsignals		(* ((uart_t *) portp->uartp)->setsignals)
#define	stl_enablerxtx		(* ((uart_t *) portp->uartp)->enablerxtx)
#define	stl_startrxtx		(* ((uart_t *) portp->uartp)->startrxtx)
#define	stl_disableintrs	(* ((uart_t *) portp->uartp)->disableintrs)
#define	stl_sendbreak		(* ((uart_t *) portp->uartp)->sendbreak)
#define	stl_flowctrl		(* ((uart_t *) portp->uartp)->flowctrl)
#define	stl_sendflow		(* ((uart_t *) portp->uartp)->sendflow)
#define	stl_flush		(* ((uart_t *) portp->uartp)->flush)
#define	stl_datastate		(* ((uart_t *) portp->uartp)->datastate)

/*****************************************************************************/

/*
 *	CD1400 UART specific data initialization.
 */
static uart_t stl_cd1400uart = {
	stl_cd1400panelinit,
	stl_cd1400portinit,
	stl_cd1400setport,
	stl_cd1400getsignals,
	stl_cd1400setsignals,
	stl_cd1400enablerxtx,
	stl_cd1400startrxtx,
	stl_cd1400disableintrs,
	stl_cd1400sendbreak,
	stl_cd1400flowctrl,
	stl_cd1400sendflow,
	stl_cd1400flush,
	stl_cd1400datastate,
	stl_cd1400eiointr
};

/*
 *	Define the offsets within the register bank of a cd1400 based panel.
 *	These io address offsets are common to the EasyIO board as well.
 */
#define	EREG_ADDR	0
#define	EREG_DATA	4
#define	EREG_RXACK	5
#define	EREG_TXACK	6
#define	EREG_MDACK	7

#define	EREG_BANKSIZE	8

#define	CD1400_CLK	25000000
#define	CD1400_CLK8M	20000000

/*
 *	Define the cd1400 baud rate clocks. These are used when calculating
 *	what clock and divisor to use for the required baud rate. Also
 *	define the maximum baud rate allowed, and the default base baud.
 */
static int	stl_cd1400clkdivs[] = {
	CD1400_CLK0, CD1400_CLK1, CD1400_CLK2, CD1400_CLK3, CD1400_CLK4
};

/*****************************************************************************/

/*
 *	SC26198 UART specific data initization.
 */
static uart_t stl_sc26198uart = {
	stl_sc26198panelinit,
	stl_sc26198portinit,
	stl_sc26198setport,
	stl_sc26198getsignals,
	stl_sc26198setsignals,
	stl_sc26198enablerxtx,
	stl_sc26198startrxtx,
	stl_sc26198disableintrs,
	stl_sc26198sendbreak,
	stl_sc26198flowctrl,
	stl_sc26198sendflow,
	stl_sc26198flush,
	stl_sc26198datastate,
	stl_sc26198intr
};

/*
 *	Define the offsets within the register bank of a sc26198 based panel.
 */
#define	XP_DATA		0
#define	XP_ADDR		1
#define	XP_MODID	2
#define	XP_STATUS	2
#define	XP_IACK		3

#define	XP_BANKSIZE	4

/*
 *	Define the sc26198 baud rate table. Offsets within the table
 *	represent the actual baud rate selector of sc26198 registers.
 */
static unsigned int	sc26198_baudtable[] = {
	50, 75, 150, 200, 300, 450, 600, 900, 1200, 1800, 2400, 3600,
	4800, 7200, 9600, 14400, 19200, 28800, 38400, 57600, 115200,
	230400, 460800, 921600
};

#define	SC26198_NRBAUDS		ARRAY_SIZE(sc26198_baudtable)

/*****************************************************************************/

/*
 *	Define the driver info for a user level control device. Used mainly
 *	to get at port stats - only not using the port device itself.
 */
static const struct file_operations	stl_fsiomem = {
	.owner		= THIS_MODULE,
	.ioctl		= stl_memioctl,
};

static struct class *stallion_class;

static void stl_cd_change(struct stlport *portp)
{
	unsigned int oldsigs = portp->sigs;
	struct tty_struct *tty = tty_port_tty_get(&portp->port);

	if (!tty)
		return;

	portp->sigs = stl_getsignals(portp);

	if ((portp->sigs & TIOCM_CD) && ((oldsigs & TIOCM_CD) == 0))
		wake_up_interruptible(&portp->port.open_wait);

	if ((oldsigs & TIOCM_CD) && ((portp->sigs & TIOCM_CD) == 0))
		if (portp->port.flags & ASYNC_CHECK_CD)
			tty_hangup(tty);
	tty_kref_put(tty);
}

/*
 *	Check for any arguments passed in on the module load command line.
 */

/*****************************************************************************/

/*
 *	Parse the supplied argument string, into the board conf struct.
 */

static int __init stl_parsebrd(struct stlconf *confp, char **argp)
{
	char	*sp;
	unsigned int i;

	pr_debug("stl_parsebrd(confp=%p,argp=%p)\n", confp, argp);

	if ((argp[0] == NULL) || (*argp[0] == 0))
		return 0;

	for (sp = argp[0], i = 0; (*sp != 0) && (i < 25); sp++, i++)
		*sp = tolower(*sp);

	for (i = 0; i < ARRAY_SIZE(stl_brdstr); i++)
		if (strcmp(stl_brdstr[i].name, argp[0]) == 0)
			break;

	if (i == ARRAY_SIZE(stl_brdstr)) {
		printk("STALLION: unknown board name, %s?\n", argp[0]);
		return 0;
	}

	confp->brdtype = stl_brdstr[i].type;

	i = 1;
	if ((argp[i] != NULL) && (*argp[i] != 0))
		confp->ioaddr1 = simple_strtoul(argp[i], NULL, 0);
	i++;
	if (confp->brdtype == BRD_ECH) {
		if ((argp[i] != NULL) && (*argp[i] != 0))
			confp->ioaddr2 = simple_strtoul(argp[i], NULL, 0);
		i++;
	}
	if ((argp[i] != NULL) && (*argp[i] != 0))
		confp->irq = simple_strtoul(argp[i], NULL, 0);
	return 1;
}

/*****************************************************************************/

/*
 *	Allocate a new board structure. Fill out the basic info in it.
 */

static struct stlbrd *stl_allocbrd(void)
{
	struct stlbrd	*brdp;

	brdp = kzalloc(sizeof(struct stlbrd), GFP_KERNEL);
	if (!brdp) {
		printk("STALLION: failed to allocate memory (size=%Zd)\n",
			sizeof(struct stlbrd));
		return NULL;
	}

	brdp->magic = STL_BOARDMAGIC;
	return brdp;
}

/*****************************************************************************/

static int stl_open(struct tty_struct *tty, struct file *filp)
{
	struct stlport	*portp;
	struct stlbrd	*brdp;
	struct tty_port *port;
	unsigned int	minordev, brdnr, panelnr;
	int		portnr;

	pr_debug("stl_open(tty=%p,filp=%p): device=%s\n", tty, filp, tty->name);

	minordev = tty->index;
	brdnr = MINOR2BRD(minordev);
	if (brdnr >= stl_nrbrds)
		return -ENODEV;
	brdp = stl_brds[brdnr];
	if (brdp == NULL)
		return -ENODEV;

	minordev = MINOR2PORT(minordev);
	for (portnr = -1, panelnr = 0; panelnr < STL_MAXPANELS; panelnr++) {
		if (brdp->panels[panelnr] == NULL)
			break;
		if (minordev < brdp->panels[panelnr]->nrports) {
			portnr = minordev;
			break;
		}
		minordev -= brdp->panels[panelnr]->nrports;
	}
	if (portnr < 0)
		return -ENODEV;

	portp = brdp->panels[panelnr]->ports[portnr];
	if (portp == NULL)
		return -ENODEV;
	port = &portp->port;

/*
 *	On the first open of the device setup the port hardware, and
 *	initialize the per port data structure.
 */
	tty_port_tty_set(port, tty);
	tty->driver_data = portp;
	port->count++;

	if ((port->flags & ASYNC_INITIALIZED) == 0) {
		if (!portp->tx.buf) {
			portp->tx.buf = kmalloc(STL_TXBUFSIZE, GFP_KERNEL);
			if (!portp->tx.buf)
				return -ENOMEM;
			portp->tx.head = portp->tx.buf;
			portp->tx.tail = portp->tx.buf;
		}
		stl_setport(portp, tty->termios);
		portp->sigs = stl_getsignals(portp);
		stl_setsignals(portp, 1, 1);
		stl_enablerxtx(portp, 1, 1);
		stl_startrxtx(portp, 1, 0);
		clear_bit(TTY_IO_ERROR, &tty->flags);
		port->flags |= ASYNC_INITIALIZED;
	}
	return tty_port_block_til_ready(port, tty, filp);
}

/*****************************************************************************/

static int stl_carrier_raised(struct tty_port *port)
{
	struct stlport *portp = container_of(port, struct stlport, port);
	return (portp->sigs & TIOCM_CD) ? 1 : 0;
}

static void stl_dtr_rts(struct tty_port *port, int on)
{
	struct stlport *portp = container_of(port, struct stlport, port);
	/* Takes brd_lock internally */
	stl_setsignals(portp, on, on);
}

/*****************************************************************************/

static void stl_flushbuffer(struct tty_struct *tty)
{
	struct stlport	*portp;

	pr_debug("stl_flushbuffer(tty=%p)\n", tty);

	portp = tty->driver_data;
	if (portp == NULL)
		return;

	stl_flush(portp);
	tty_wakeup(tty);
}

/*****************************************************************************/

static void stl_waituntilsent(struct tty_struct *tty, int timeout)
{
	struct stlport	*portp;
	unsigned long	tend;

	pr_debug("stl_waituntilsent(tty=%p,timeout=%d)\n", tty, timeout);

	portp = tty->driver_data;
	if (portp == NULL)
		return;

	if (timeout == 0)
		timeout = HZ;
	tend = jiffies + timeout;

	lock_kernel();
	while (stl_datastate(portp)) {
		if (signal_pending(current))
			break;
		msleep_interruptible(20);
		if (time_after_eq(jiffies, tend))
			break;
	}
	unlock_kernel();
}

/*****************************************************************************/

static void stl_close(struct tty_struct *tty, struct file *filp)
{
	struct stlport	*portp;
	struct tty_port *port;
	unsigned long	flags;

	pr_debug("stl_close(tty=%p,filp=%p)\n", tty, filp);

	portp = tty->driver_data;
	BUG_ON(portp == NULL);

	port = &portp->port;

	if (tty_port_close_start(port, tty, filp) == 0)
		return;
/*
 *	May want to wait for any data to drain before closing. The BUSY
 *	flag keeps track of whether we are still sending or not - it is
 *	very accurate for the cd1400, not quite so for the sc26198.
 *	(The sc26198 has no "end-of-data" interrupt only empty FIFO)
 */
	stl_waituntilsent(tty, (HZ / 2));

	spin_lock_irqsave(&port->lock, flags);
	portp->port.flags &= ~ASYNC_INITIALIZED;
	spin_unlock_irqrestore(&port->lock, flags);

	stl_disableintrs(portp);
	if (tty->termios->c_cflag & HUPCL)
		stl_setsignals(portp, 0, 0);
	stl_enablerxtx(portp, 0, 0);
	stl_flushbuffer(tty);
	portp->istate = 0;
	if (portp->tx.buf != NULL) {
		kfree(portp->tx.buf);
		portp->tx.buf = NULL;
		portp->tx.head = NULL;
		portp->tx.tail = NULL;
	}

	tty_port_close_end(port, tty);
	tty_port_tty_set(port, NULL);
}

/*****************************************************************************/

/*
 *	Write routine. Take data and stuff it in to the TX ring queue.
 *	If transmit interrupts are not running then start them.
 */

static int stl_write(struct tty_struct *tty, const unsigned char *buf, int count)
{
	struct stlport	*portp;
	unsigned int	len, stlen;
	unsigned char	*chbuf;
	char		*head, *tail;

	pr_debug("stl_write(tty=%p,buf=%p,count=%d)\n", tty, buf, count);

	portp = tty->driver_data;
	if (portp == NULL)
		return 0;
	if (portp->tx.buf == NULL)
		return 0;

/*
 *	If copying direct from user space we must cater for page faults,
 *	causing us to "sleep" here for a while. To handle this copy in all
 *	the data we need now, into a local buffer. Then when we got it all
 *	copy it into the TX buffer.
 */
	chbuf = (unsigned char *) buf;

	head = portp->tx.head;
	tail = portp->tx.tail;
	if (head >= tail) {
		len = STL_TXBUFSIZE - (head - tail) - 1;
		stlen = STL_TXBUFSIZE - (head - portp->tx.buf);
	} else {
		len = tail - head - 1;
		stlen = len;
	}

	len = min(len, (unsigned int)count);
	count = 0;
	while (len > 0) {
		stlen = min(len, stlen);
		memcpy(head, chbuf, stlen);
		len -= stlen;
		chbuf += stlen;
		count += stlen;
		head += stlen;
		if (head >= (portp->tx.buf + STL_TXBUFSIZE)) {
			head = portp->tx.buf;
			stlen = tail - head;
		}
	}
	portp->tx.head = head;

	clear_bit(ASYI_TXLOW, &portp->istate);
	stl_startrxtx(portp, -1, 1);

	return count;
}

/*****************************************************************************/

static int stl_putchar(struct tty_struct *tty, unsigned char ch)
{
	struct stlport	*portp;
	unsigned int	len;
	char		*head, *tail;

	pr_debug("stl_putchar(tty=%p,ch=%x)\n", tty, ch);

	portp = tty->driver_data;
	if (portp == NULL)
		return -EINVAL;
	if (portp->tx.buf == NULL)
		return -EINVAL;

	head = portp->tx.head;
	tail = portp->tx.tail;

	len = (head >= tail) ? (STL_TXBUFSIZE - (head - tail)) : (tail - head);
	len--;

	if (len > 0) {
		*head++ = ch;
		if (head >= (portp->tx.buf + STL_TXBUFSIZE))
			head = portp->tx.buf;
	}	
	portp->tx.head = head;
	return 0;
}

/*****************************************************************************/

/*
 *	If there are any characters in the buffer then make sure that TX
 *	interrupts are on and get'em out. Normally used after the putchar
 *	routine has been called.
 */

static void stl_flushchars(struct tty_struct *tty)
{
	struct stlport	*portp;

	pr_debug("stl_flushchars(tty=%p)\n", tty);

	portp = tty->driver_data;
	if (portp == NULL)
		return;
	if (portp->tx.buf == NULL)
		return;

	stl_startrxtx(portp, -1, 1);
}

/*****************************************************************************/

static int stl_writeroom(struct tty_struct *tty)
{
	struct stlport	*portp;
	char		*head, *tail;

	pr_debug("stl_writeroom(tty=%p)\n", tty);

	portp = tty->driver_data;
	if (portp == NULL)
		return 0;
	if (portp->tx.buf == NULL)
		return 0;

	head = portp->tx.head;
	tail = portp->tx.tail;
	return (head >= tail) ? (STL_TXBUFSIZE - (head - tail) - 1) : (tail - head - 1);
}

/*****************************************************************************/

/*
 *	Return number of chars in the TX buffer. Normally we would just
 *	calculate the number of chars in the buffer and return that, but if
 *	the buffer is empty and TX interrupts are still on then we return
 *	that the buffer still has 1 char in it. This way whoever called us
 *	will not think that ALL chars have drained - since the UART still
 *	must have some chars in it (we are busy after all).
 */

static int stl_charsinbuffer(struct tty_struct *tty)
{
	struct stlport	*portp;
	unsigned int	size;
	char		*head, *tail;

	pr_debug("stl_charsinbuffer(tty=%p)\n", tty);

	portp = tty->driver_data;
	if (portp == NULL)
		return 0;
	if (portp->tx.buf == NULL)
		return 0;

	head = portp->tx.head;
	tail = portp->tx.tail;
	size = (head >= tail) ? (head - tail) : (STL_TXBUFSIZE - (tail - head));
	if ((size == 0) && test_bit(ASYI_TXBUSY, &portp->istate))
		size = 1;
	return size;
}

/*****************************************************************************/

/*
 *	Generate the serial struct info.
 */

static int stl_getserial(struct stlport *portp, struct serial_struct __user *sp)
{
	struct serial_struct	sio;
	struct stlbrd		*brdp;

	pr_debug("stl_getserial(portp=%p,sp=%p)\n", portp, sp);

	memset(&sio, 0, sizeof(struct serial_struct));
	sio.line = portp->portnr;
	sio.port = portp->ioaddr;
	sio.flags = portp->port.flags;
	sio.baud_base = portp->baud_base;
	sio.close_delay = portp->close_delay;
	sio.closing_wait = portp->closing_wait;
	sio.custom_divisor = portp->custom_divisor;
	sio.hub6 = 0;
	if (portp->uartp == &stl_cd1400uart) {
		sio.type = PORT_CIRRUS;
		sio.xmit_fifo_size = CD1400_TXFIFOSIZE;
	} else {
		sio.type = PORT_UNKNOWN;
		sio.xmit_fifo_size = SC26198_TXFIFOSIZE;
	}

	brdp = stl_brds[portp->brdnr];
	if (brdp != NULL)
		sio.irq = brdp->irq;

	return copy_to_user(sp, &sio, sizeof(struct serial_struct)) ? -EFAULT : 0;
}

/*****************************************************************************/

/*
 *	Set port according to the serial struct info.
 *	At this point we do not do any auto-configure stuff, so we will
 *	just quietly ignore any requests to change irq, etc.
 */

static int stl_setserial(struct tty_struct *tty, struct serial_struct __user *sp)
{
	struct stlport *	portp = tty->driver_data;
	struct serial_struct	sio;

	pr_debug("stl_setserial(portp=%p,sp=%p)\n", portp, sp);

	if (copy_from_user(&sio, sp, sizeof(struct serial_struct)))
		return -EFAULT;
	if (!capable(CAP_SYS_ADMIN)) {
		if ((sio.baud_base != portp->baud_base) ||
		    (sio.close_delay != portp->close_delay) ||
		    ((sio.flags & ~ASYNC_USR_MASK) !=
		    (portp->port.flags & ~ASYNC_USR_MASK)))
			return -EPERM;
	} 

	portp->port.flags = (portp->port.flags & ~ASYNC_USR_MASK) |
		(sio.flags & ASYNC_USR_MASK);
	portp->baud_base = sio.baud_base;
	portp->close_delay = sio.close_delay;
	portp->closing_wait = sio.closing_wait;
	portp->custom_divisor = sio.custom_divisor;
	stl_setport(portp, tty->termios);
	return 0;
}

/*****************************************************************************/

static int stl_tiocmget(struct tty_struct *tty, struct file *file)
{
	struct stlport	*portp;

	portp = tty->driver_data;
	if (portp == NULL)
		return -ENODEV;
	if (tty->flags & (1 << TTY_IO_ERROR))
		return -EIO;

	return stl_getsignals(portp);
}

static int stl_tiocmset(struct tty_struct *tty, struct file *file,
			unsigned int set, unsigned int clear)
{
	struct stlport	*portp;
	int rts = -1, dtr = -1;

	portp = tty->driver_data;
	if (portp == NULL)
		return -ENODEV;
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

	stl_setsignals(portp, dtr, rts);
	return 0;
}

static int stl_ioctl(struct tty_struct *tty, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct stlport	*portp;
	int		rc;
	void __user *argp = (void __user *)arg;

	pr_debug("stl_ioctl(tty=%p,file=%p,cmd=%x,arg=%lx)\n", tty, file, cmd,
			arg);

	portp = tty->driver_data;
	if (portp == NULL)
		return -ENODEV;

	if ((cmd != TIOCGSERIAL) && (cmd != TIOCSSERIAL) &&
 	    (cmd != COM_GETPORTSTATS) && (cmd != COM_CLRPORTSTATS))
		if (tty->flags & (1 << TTY_IO_ERROR))
			return -EIO;

	rc = 0;

	lock_kernel();

	switch (cmd) {
	case TIOCGSERIAL:
		rc = stl_getserial(portp, argp);
		break;
	case TIOCSSERIAL:
		rc = stl_setserial(tty, argp);
		break;
	case COM_GETPORTSTATS:
		rc = stl_getportstats(tty, portp, argp);
		break;
	case COM_CLRPORTSTATS:
		rc = stl_clrportstats(portp, argp);
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
	unlock_kernel();
	return rc;
}

/*****************************************************************************/

/*
 *	Start the transmitter again. Just turn TX interrupts back on.
 */

static void stl_start(struct tty_struct *tty)
{
	struct stlport	*portp;

	pr_debug("stl_start(tty=%p)\n", tty);

	portp = tty->driver_data;
	if (portp == NULL)
		return;
	stl_startrxtx(portp, -1, 1);
}

/*****************************************************************************/

static void stl_settermios(struct tty_struct *tty, struct ktermios *old)
{
	struct stlport	*portp;
	struct ktermios	*tiosp;

	pr_debug("stl_settermios(tty=%p,old=%p)\n", tty, old);

	portp = tty->driver_data;
	if (portp == NULL)
		return;

	tiosp = tty->termios;
	if ((tiosp->c_cflag == old->c_cflag) &&
	    (tiosp->c_iflag == old->c_iflag))
		return;

	stl_setport(portp, tiosp);
	stl_setsignals(portp, ((tiosp->c_cflag & (CBAUD & ~CBAUDEX)) ? 1 : 0),
		-1);
	if ((old->c_cflag & CRTSCTS) && ((tiosp->c_cflag & CRTSCTS) == 0)) {
		tty->hw_stopped = 0;
		stl_start(tty);
	}
	if (((old->c_cflag & CLOCAL) == 0) && (tiosp->c_cflag & CLOCAL))
		wake_up_interruptible(&portp->port.open_wait);
}

/*****************************************************************************/

/*
 *	Attempt to flow control who ever is sending us data. Based on termios
 *	settings use software or/and hardware flow control.
 */

static void stl_throttle(struct tty_struct *tty)
{
	struct stlport	*portp;

	pr_debug("stl_throttle(tty=%p)\n", tty);

	portp = tty->driver_data;
	if (portp == NULL)
		return;
	stl_flowctrl(portp, 0);
}

/*****************************************************************************/

/*
 *	Unflow control the device sending us data...
 */

static void stl_unthrottle(struct tty_struct *tty)
{
	struct stlport	*portp;

	pr_debug("stl_unthrottle(tty=%p)\n", tty);

	portp = tty->driver_data;
	if (portp == NULL)
		return;
	stl_flowctrl(portp, 1);
}

/*****************************************************************************/

/*
 *	Stop the transmitter. Basically to do this we will just turn TX
 *	interrupts off.
 */

static void stl_stop(struct tty_struct *tty)
{
	struct stlport	*portp;

	pr_debug("stl_stop(tty=%p)\n", tty);

	portp = tty->driver_data;
	if (portp == NULL)
		return;
	stl_startrxtx(portp, -1, 0);
}

/*****************************************************************************/

/*
 *	Hangup this port. This is pretty much like closing the port, only
 *	a little more brutal. No waiting for data to drain. Shutdown the
 *	port and maybe drop signals.
 */

static void stl_hangup(struct tty_struct *tty)
{
	struct stlport	*portp;
	struct tty_port *port;
	unsigned long flags;

	pr_debug("stl_hangup(tty=%p)\n", tty);

	portp = tty->driver_data;
	if (portp == NULL)
		return;
	port = &portp->port;

	spin_lock_irqsave(&port->lock, flags);
	port->flags &= ~ASYNC_INITIALIZED;
	spin_unlock_irqrestore(&port->lock, flags);

	stl_disableintrs(portp);
	if (tty->termios->c_cflag & HUPCL)
		stl_setsignals(portp, 0, 0);
	stl_enablerxtx(portp, 0, 0);
	stl_flushbuffer(tty);
	portp->istate = 0;
	set_bit(TTY_IO_ERROR, &tty->flags);
	if (portp->tx.buf != NULL) {
		kfree(portp->tx.buf);
		portp->tx.buf = NULL;
		portp->tx.head = NULL;
		portp->tx.tail = NULL;
	}
	tty_port_hangup(port);
}

/*****************************************************************************/

static int stl_breakctl(struct tty_struct *tty, int state)
{
	struct stlport	*portp;

	pr_debug("stl_breakctl(tty=%p,state=%d)\n", tty, state);

	portp = tty->driver_data;
	if (portp == NULL)
		return -EINVAL;

	stl_sendbreak(portp, ((state == -1) ? 1 : 2));
	return 0;
}

/*****************************************************************************/

static void stl_sendxchar(struct tty_struct *tty, char ch)
{
	struct stlport	*portp;

	pr_debug("stl_sendxchar(tty=%p,ch=%x)\n", tty, ch);

	portp = tty->driver_data;
	if (portp == NULL)
		return;

	if (ch == STOP_CHAR(tty))
		stl_sendflow(portp, 0);
	else if (ch == START_CHAR(tty))
		stl_sendflow(portp, 1);
	else
		stl_putchar(tty, ch);
}

static void stl_portinfo(struct seq_file *m, struct stlport *portp, int portnr)
{
	int	sigs;
	char sep;

	seq_printf(m, "%d: uart:%s tx:%d rx:%d",
		portnr, (portp->hwid == 1) ? "SC26198" : "CD1400",
		(int) portp->stats.txtotal, (int) portp->stats.rxtotal);

	if (portp->stats.rxframing)
		seq_printf(m, " fe:%d", (int) portp->stats.rxframing);
	if (portp->stats.rxparity)
		seq_printf(m, " pe:%d", (int) portp->stats.rxparity);
	if (portp->stats.rxbreaks)
		seq_printf(m, " brk:%d", (int) portp->stats.rxbreaks);
	if (portp->stats.rxoverrun)
		seq_printf(m, " oe:%d", (int) portp->stats.rxoverrun);

	sigs = stl_getsignals(portp);
	sep = ' ';
	if (sigs & TIOCM_RTS) {
		seq_printf(m, "%c%s", sep, "RTS");
		sep = '|';
	}
	if (sigs & TIOCM_CTS) {
		seq_printf(m, "%c%s", sep, "CTS");
		sep = '|';
	}
	if (sigs & TIOCM_DTR) {
		seq_printf(m, "%c%s", sep, "DTR");
		sep = '|';
	}
	if (sigs & TIOCM_CD) {
		seq_printf(m, "%c%s", sep, "DCD");
		sep = '|';
	}
	if (sigs & TIOCM_DSR) {
		seq_printf(m, "%c%s", sep, "DSR");
		sep = '|';
	}
	seq_putc(m, '\n');
}

/*****************************************************************************/

/*
 *	Port info, read from the /proc file system.
 */

static int stl_proc_show(struct seq_file *m, void *v)
{
	struct stlbrd	*brdp;
	struct stlpanel	*panelp;
	struct stlport	*portp;
	unsigned int	brdnr, panelnr, portnr;
	int		totalport;

	totalport = 0;

	seq_printf(m, "%s: version %s\n", stl_drvtitle, stl_drvversion);

/*
 *	We scan through for each board, panel and port. The offset is
 *	calculated on the fly, and irrelevant ports are skipped.
 */
	for (brdnr = 0; brdnr < stl_nrbrds; brdnr++) {
		brdp = stl_brds[brdnr];
		if (brdp == NULL)
			continue;
		if (brdp->state == 0)
			continue;

		totalport = brdnr * STL_MAXPORTS;
		for (panelnr = 0; panelnr < brdp->nrpanels; panelnr++) {
			panelp = brdp->panels[panelnr];
			if (panelp == NULL)
				continue;

			for (portnr = 0; portnr < panelp->nrports; portnr++,
			    totalport++) {
				portp = panelp->ports[portnr];
				if (portp == NULL)
					continue;
				stl_portinfo(m, portp, totalport);
			}
		}
	}
	return 0;
}

static int stl_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, stl_proc_show, NULL);
}

static const struct file_operations stl_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= stl_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/*****************************************************************************/

/*
 *	All board interrupts are vectored through here first. This code then
 *	calls off to the approrpriate board interrupt handlers.
 */

static irqreturn_t stl_intr(int irq, void *dev_id)
{
	struct stlbrd *brdp = dev_id;

	pr_debug("stl_intr(brdp=%p,irq=%d)\n", brdp, brdp->irq);

	return IRQ_RETVAL((* brdp->isr)(brdp));
}

/*****************************************************************************/

/*
 *	Interrupt service routine for EasyIO board types.
 */

static int stl_eiointr(struct stlbrd *brdp)
{
	struct stlpanel	*panelp;
	unsigned int	iobase;
	int		handled = 0;

	spin_lock(&brd_lock);
	panelp = brdp->panels[0];
	iobase = panelp->iobase;
	while (inb(brdp->iostatus) & EIO_INTRPEND) {
		handled = 1;
		(* panelp->isr)(panelp, iobase);
	}
	spin_unlock(&brd_lock);
	return handled;
}

/*****************************************************************************/

/*
 *	Interrupt service routine for ECH-AT board types.
 */

static int stl_echatintr(struct stlbrd *brdp)
{
	struct stlpanel	*panelp;
	unsigned int	ioaddr, bnknr;
	int		handled = 0;

	outb((brdp->ioctrlval | ECH_BRDENABLE), brdp->ioctrl);

	while (inb(brdp->iostatus) & ECH_INTRPEND) {
		handled = 1;
		for (bnknr = 0; bnknr < brdp->nrbnks; bnknr++) {
			ioaddr = brdp->bnkstataddr[bnknr];
			if (inb(ioaddr) & ECH_PNLINTRPEND) {
				panelp = brdp->bnk2panel[bnknr];
				(* panelp->isr)(panelp, (ioaddr & 0xfffc));
			}
		}
	}

	outb((brdp->ioctrlval | ECH_BRDDISABLE), brdp->ioctrl);

	return handled;
}

/*****************************************************************************/

/*
 *	Interrupt service routine for ECH-MCA board types.
 */

static int stl_echmcaintr(struct stlbrd *brdp)
{
	struct stlpanel	*panelp;
	unsigned int	ioaddr, bnknr;
	int		handled = 0;

	while (inb(brdp->iostatus) & ECH_INTRPEND) {
		handled = 1;
		for (bnknr = 0; bnknr < brdp->nrbnks; bnknr++) {
			ioaddr = brdp->bnkstataddr[bnknr];
			if (inb(ioaddr) & ECH_PNLINTRPEND) {
				panelp = brdp->bnk2panel[bnknr];
				(* panelp->isr)(panelp, (ioaddr & 0xfffc));
			}
		}
	}
	return handled;
}

/*****************************************************************************/

/*
 *	Interrupt service routine for ECH-PCI board types.
 */

static int stl_echpciintr(struct stlbrd *brdp)
{
	struct stlpanel	*panelp;
	unsigned int	ioaddr, bnknr, recheck;
	int		handled = 0;

	while (1) {
		recheck = 0;
		for (bnknr = 0; bnknr < brdp->nrbnks; bnknr++) {
			outb(brdp->bnkpageaddr[bnknr], brdp->ioctrl);
			ioaddr = brdp->bnkstataddr[bnknr];
			if (inb(ioaddr) & ECH_PNLINTRPEND) {
				panelp = brdp->bnk2panel[bnknr];
				(* panelp->isr)(panelp, (ioaddr & 0xfffc));
				recheck++;
				handled = 1;
			}
		}
		if (! recheck)
			break;
	}
	return handled;
}

/*****************************************************************************/

/*
 *	Interrupt service routine for ECH-8/64-PCI board types.
 */

static int stl_echpci64intr(struct stlbrd *brdp)
{
	struct stlpanel	*panelp;
	unsigned int	ioaddr, bnknr;
	int		handled = 0;

	while (inb(brdp->ioctrl) & 0x1) {
		handled = 1;
		for (bnknr = 0; bnknr < brdp->nrbnks; bnknr++) {
			ioaddr = brdp->bnkstataddr[bnknr];
			if (inb(ioaddr) & ECH_PNLINTRPEND) {
				panelp = brdp->bnk2panel[bnknr];
				(* panelp->isr)(panelp, (ioaddr & 0xfffc));
			}
		}
	}

	return handled;
}

/*****************************************************************************/

/*
 *	Initialize all the ports on a panel.
 */

static int __devinit stl_initports(struct stlbrd *brdp, struct stlpanel *panelp)
{
	struct stlport *portp;
	unsigned int i;
	int chipmask;

	pr_debug("stl_initports(brdp=%p,panelp=%p)\n", brdp, panelp);

	chipmask = stl_panelinit(brdp, panelp);

/*
 *	All UART's are initialized (if found!). Now go through and setup
 *	each ports data structures.
 */
	for (i = 0; i < panelp->nrports; i++) {
		portp = kzalloc(sizeof(struct stlport), GFP_KERNEL);
		if (!portp) {
			printk("STALLION: failed to allocate memory "
				"(size=%Zd)\n", sizeof(struct stlport));
			break;
		}
		tty_port_init(&portp->port);
		portp->port.ops = &stl_port_ops;
		portp->magic = STL_PORTMAGIC;
		portp->portnr = i;
		portp->brdnr = panelp->brdnr;
		portp->panelnr = panelp->panelnr;
		portp->uartp = panelp->uartp;
		portp->clk = brdp->clk;
		portp->baud_base = STL_BAUDBASE;
		portp->close_delay = STL_CLOSEDELAY;
		portp->closing_wait = 30 * HZ;
		init_waitqueue_head(&portp->port.open_wait);
		init_waitqueue_head(&portp->port.close_wait);
		portp->stats.brd = portp->brdnr;
		portp->stats.panel = portp->panelnr;
		portp->stats.port = portp->portnr;
		panelp->ports[i] = portp;
		stl_portinit(brdp, panelp, portp);
	}

	return 0;
}

static void stl_cleanup_panels(struct stlbrd *brdp)
{
	struct stlpanel *panelp;
	struct stlport *portp;
	unsigned int j, k;
	struct tty_struct *tty;

	for (j = 0; j < STL_MAXPANELS; j++) {
		panelp = brdp->panels[j];
		if (panelp == NULL)
			continue;
		for (k = 0; k < STL_PORTSPERPANEL; k++) {
			portp = panelp->ports[k];
			if (portp == NULL)
				continue;
			tty = tty_port_tty_get(&portp->port);
			if (tty != NULL) {
				stl_hangup(tty);
				tty_kref_put(tty);
			}
			kfree(portp->tx.buf);
			kfree(portp);
		}
		kfree(panelp);
	}
}

/*****************************************************************************/

/*
 *	Try to find and initialize an EasyIO board.
 */

static int __devinit stl_initeio(struct stlbrd *brdp)
{
	struct stlpanel	*panelp;
	unsigned int	status;
	char		*name;
	int		retval;

	pr_debug("stl_initeio(brdp=%p)\n", brdp);

	brdp->ioctrl = brdp->ioaddr1 + 1;
	brdp->iostatus = brdp->ioaddr1 + 2;

	status = inb(brdp->iostatus);
	if ((status & EIO_IDBITMASK) == EIO_MK3)
		brdp->ioctrl++;

/*
 *	Handle board specific stuff now. The real difference is PCI
 *	or not PCI.
 */
	if (brdp->brdtype == BRD_EASYIOPCI) {
		brdp->iosize1 = 0x80;
		brdp->iosize2 = 0x80;
		name = "serial(EIO-PCI)";
		outb(0x41, (brdp->ioaddr2 + 0x4c));
	} else {
		brdp->iosize1 = 8;
		name = "serial(EIO)";
		if ((brdp->irq < 0) || (brdp->irq > 15) ||
		    (stl_vecmap[brdp->irq] == (unsigned char) 0xff)) {
			printk("STALLION: invalid irq=%d for brd=%d\n",
				brdp->irq, brdp->brdnr);
			retval = -EINVAL;
			goto err;
		}
		outb((stl_vecmap[brdp->irq] | EIO_0WS |
			((brdp->irqtype) ? EIO_INTLEVEL : EIO_INTEDGE)),
			brdp->ioctrl);
	}

	retval = -EBUSY;
	if (!request_region(brdp->ioaddr1, brdp->iosize1, name)) {
		printk(KERN_WARNING "STALLION: Warning, board %d I/O address "
			"%x conflicts with another device\n", brdp->brdnr, 
			brdp->ioaddr1);
		goto err;
	}
	
	if (brdp->iosize2 > 0)
		if (!request_region(brdp->ioaddr2, brdp->iosize2, name)) {
			printk(KERN_WARNING "STALLION: Warning, board %d I/O "
				"address %x conflicts with another device\n",
				brdp->brdnr, brdp->ioaddr2);
			printk(KERN_WARNING "STALLION: Warning, also "
				"releasing board %d I/O address %x \n", 
				brdp->brdnr, brdp->ioaddr1);
			goto err_rel1;
		}

/*
 *	Everything looks OK, so let's go ahead and probe for the hardware.
 */
	brdp->clk = CD1400_CLK;
	brdp->isr = stl_eiointr;

	retval = -ENODEV;
	switch (status & EIO_IDBITMASK) {
	case EIO_8PORTM:
		brdp->clk = CD1400_CLK8M;
		/* fall thru */
	case EIO_8PORTRS:
	case EIO_8PORTDI:
		brdp->nrports = 8;
		break;
	case EIO_4PORTRS:
		brdp->nrports = 4;
		break;
	case EIO_MK3:
		switch (status & EIO_BRDMASK) {
		case ID_BRD4:
			brdp->nrports = 4;
			break;
		case ID_BRD8:
			brdp->nrports = 8;
			break;
		case ID_BRD16:
			brdp->nrports = 16;
			break;
		default:
			goto err_rel2;
		}
		break;
	default:
		goto err_rel2;
	}

/*
 *	We have verified that the board is actually present, so now we
 *	can complete the setup.
 */

	panelp = kzalloc(sizeof(struct stlpanel), GFP_KERNEL);
	if (!panelp) {
		printk(KERN_WARNING "STALLION: failed to allocate memory "
			"(size=%Zd)\n", sizeof(struct stlpanel));
		retval = -ENOMEM;
		goto err_rel2;
	}

	panelp->magic = STL_PANELMAGIC;
	panelp->brdnr = brdp->brdnr;
	panelp->panelnr = 0;
	panelp->nrports = brdp->nrports;
	panelp->iobase = brdp->ioaddr1;
	panelp->hwid = status;
	if ((status & EIO_IDBITMASK) == EIO_MK3) {
		panelp->uartp = &stl_sc26198uart;
		panelp->isr = stl_sc26198intr;
	} else {
		panelp->uartp = &stl_cd1400uart;
		panelp->isr = stl_cd1400eiointr;
	}

	brdp->panels[0] = panelp;
	brdp->nrpanels = 1;
	brdp->state |= BRD_FOUND;
	brdp->hwid = status;
	if (request_irq(brdp->irq, stl_intr, IRQF_SHARED, name, brdp) != 0) {
		printk("STALLION: failed to register interrupt "
		    "routine for %s irq=%d\n", name, brdp->irq);
		retval = -ENODEV;
		goto err_fr;
	}

	return 0;
err_fr:
	stl_cleanup_panels(brdp);
err_rel2:
	if (brdp->iosize2 > 0)
		release_region(brdp->ioaddr2, brdp->iosize2);
err_rel1:
	release_region(brdp->ioaddr1, brdp->iosize1);
err:
	return retval;
}

/*****************************************************************************/

/*
 *	Try to find an ECH board and initialize it. This code is capable of
 *	dealing with all types of ECH board.
 */

static int __devinit stl_initech(struct stlbrd *brdp)
{
	struct stlpanel	*panelp;
	unsigned int	status, nxtid, ioaddr, conflict, panelnr, banknr, i;
	int		retval;
	char		*name;

	pr_debug("stl_initech(brdp=%p)\n", brdp);

	status = 0;
	conflict = 0;

/*
 *	Set up the initial board register contents for boards. This varies a
 *	bit between the different board types. So we need to handle each
 *	separately. Also do a check that the supplied IRQ is good.
 */
	switch (brdp->brdtype) {

	case BRD_ECH:
		brdp->isr = stl_echatintr;
		brdp->ioctrl = brdp->ioaddr1 + 1;
		brdp->iostatus = brdp->ioaddr1 + 1;
		status = inb(brdp->iostatus);
		if ((status & ECH_IDBITMASK) != ECH_ID) {
			retval = -ENODEV;
			goto err;
		}
		if ((brdp->irq < 0) || (brdp->irq > 15) ||
		    (stl_vecmap[brdp->irq] == (unsigned char) 0xff)) {
			printk("STALLION: invalid irq=%d for brd=%d\n",
				brdp->irq, brdp->brdnr);
			retval = -EINVAL;
			goto err;
		}
		status = ((brdp->ioaddr2 & ECH_ADDR2MASK) >> 1);
		status |= (stl_vecmap[brdp->irq] << 1);
		outb((status | ECH_BRDRESET), brdp->ioaddr1);
		brdp->ioctrlval = ECH_INTENABLE |
			((brdp->irqtype) ? ECH_INTLEVEL : ECH_INTEDGE);
		for (i = 0; i < 10; i++)
			outb((brdp->ioctrlval | ECH_BRDENABLE), brdp->ioctrl);
		brdp->iosize1 = 2;
		brdp->iosize2 = 32;
		name = "serial(EC8/32)";
		outb(status, brdp->ioaddr1);
		break;

	case BRD_ECHMC:
		brdp->isr = stl_echmcaintr;
		brdp->ioctrl = brdp->ioaddr1 + 0x20;
		brdp->iostatus = brdp->ioctrl;
		status = inb(brdp->iostatus);
		if ((status & ECH_IDBITMASK) != ECH_ID) {
			retval = -ENODEV;
			goto err;
		}
		if ((brdp->irq < 0) || (brdp->irq > 15) ||
		    (stl_vecmap[brdp->irq] == (unsigned char) 0xff)) {
			printk("STALLION: invalid irq=%d for brd=%d\n",
				brdp->irq, brdp->brdnr);
			retval = -EINVAL;
			goto err;
		}
		outb(ECHMC_BRDRESET, brdp->ioctrl);
		outb(ECHMC_INTENABLE, brdp->ioctrl);
		brdp->iosize1 = 64;
		name = "serial(EC8/32-MC)";
		break;

	case BRD_ECHPCI:
		brdp->isr = stl_echpciintr;
		brdp->ioctrl = brdp->ioaddr1 + 2;
		brdp->iosize1 = 4;
		brdp->iosize2 = 8;
		name = "serial(EC8/32-PCI)";
		break;

	case BRD_ECH64PCI:
		brdp->isr = stl_echpci64intr;
		brdp->ioctrl = brdp->ioaddr2 + 0x40;
		outb(0x43, (brdp->ioaddr1 + 0x4c));
		brdp->iosize1 = 0x80;
		brdp->iosize2 = 0x80;
		name = "serial(EC8/64-PCI)";
		break;

	default:
		printk("STALLION: unknown board type=%d\n", brdp->brdtype);
		retval = -EINVAL;
		goto err;
	}

/*
 *	Check boards for possible IO address conflicts and return fail status 
 * 	if an IO conflict found.
 */
	retval = -EBUSY;
	if (!request_region(brdp->ioaddr1, brdp->iosize1, name)) {
		printk(KERN_WARNING "STALLION: Warning, board %d I/O address "
			"%x conflicts with another device\n", brdp->brdnr, 
			brdp->ioaddr1);
		goto err;
	}
	
	if (brdp->iosize2 > 0)
		if (!request_region(brdp->ioaddr2, brdp->iosize2, name)) {
			printk(KERN_WARNING "STALLION: Warning, board %d I/O "
				"address %x conflicts with another device\n",
				brdp->brdnr, brdp->ioaddr2);
			printk(KERN_WARNING "STALLION: Warning, also "
				"releasing board %d I/O address %x \n", 
				brdp->brdnr, brdp->ioaddr1);
			goto err_rel1;
		}

/*
 *	Scan through the secondary io address space looking for panels.
 *	As we find'em allocate and initialize panel structures for each.
 */
	brdp->clk = CD1400_CLK;
	brdp->hwid = status;

	ioaddr = brdp->ioaddr2;
	banknr = 0;
	panelnr = 0;
	nxtid = 0;

	for (i = 0; i < STL_MAXPANELS; i++) {
		if (brdp->brdtype == BRD_ECHPCI) {
			outb(nxtid, brdp->ioctrl);
			ioaddr = brdp->ioaddr2;
		}
		status = inb(ioaddr + ECH_PNLSTATUS);
		if ((status & ECH_PNLIDMASK) != nxtid)
			break;
		panelp = kzalloc(sizeof(struct stlpanel), GFP_KERNEL);
		if (!panelp) {
			printk("STALLION: failed to allocate memory "
				"(size=%Zd)\n", sizeof(struct stlpanel));
			retval = -ENOMEM;
			goto err_fr;
		}
		panelp->magic = STL_PANELMAGIC;
		panelp->brdnr = brdp->brdnr;
		panelp->panelnr = panelnr;
		panelp->iobase = ioaddr;
		panelp->pagenr = nxtid;
		panelp->hwid = status;
		brdp->bnk2panel[banknr] = panelp;
		brdp->bnkpageaddr[banknr] = nxtid;
		brdp->bnkstataddr[banknr++] = ioaddr + ECH_PNLSTATUS;

		if (status & ECH_PNLXPID) {
			panelp->uartp = &stl_sc26198uart;
			panelp->isr = stl_sc26198intr;
			if (status & ECH_PNL16PORT) {
				panelp->nrports = 16;
				brdp->bnk2panel[banknr] = panelp;
				brdp->bnkpageaddr[banknr] = nxtid;
				brdp->bnkstataddr[banknr++] = ioaddr + 4 +
					ECH_PNLSTATUS;
			} else
				panelp->nrports = 8;
		} else {
			panelp->uartp = &stl_cd1400uart;
			panelp->isr = stl_cd1400echintr;
			if (status & ECH_PNL16PORT) {
				panelp->nrports = 16;
				panelp->ackmask = 0x80;
				if (brdp->brdtype != BRD_ECHPCI)
					ioaddr += EREG_BANKSIZE;
				brdp->bnk2panel[banknr] = panelp;
				brdp->bnkpageaddr[banknr] = ++nxtid;
				brdp->bnkstataddr[banknr++] = ioaddr +
					ECH_PNLSTATUS;
			} else {
				panelp->nrports = 8;
				panelp->ackmask = 0xc0;
			}
		}

		nxtid++;
		ioaddr += EREG_BANKSIZE;
		brdp->nrports += panelp->nrports;
		brdp->panels[panelnr++] = panelp;
		if ((brdp->brdtype != BRD_ECHPCI) &&
		    (ioaddr >= (brdp->ioaddr2 + brdp->iosize2))) {
			retval = -EINVAL;
			goto err_fr;
		}
	}

	brdp->nrpanels = panelnr;
	brdp->nrbnks = banknr;
	if (brdp->brdtype == BRD_ECH)
		outb((brdp->ioctrlval | ECH_BRDDISABLE), brdp->ioctrl);

	brdp->state |= BRD_FOUND;
	if (request_irq(brdp->irq, stl_intr, IRQF_SHARED, name, brdp) != 0) {
		printk("STALLION: failed to register interrupt "
		    "routine for %s irq=%d\n", name, brdp->irq);
		retval = -ENODEV;
		goto err_fr;
	}

	return 0;
err_fr:
	stl_cleanup_panels(brdp);
	if (brdp->iosize2 > 0)
		release_region(brdp->ioaddr2, brdp->iosize2);
err_rel1:
	release_region(brdp->ioaddr1, brdp->iosize1);
err:
	return retval;
}

/*****************************************************************************/

/*
 *	Initialize and configure the specified board.
 *	Scan through all the boards in the configuration and see what we
 *	can find. Handle EIO and the ECH boards a little differently here
 *	since the initial search and setup is very different.
 */

static int __devinit stl_brdinit(struct stlbrd *brdp)
{
	int i, retval;

	pr_debug("stl_brdinit(brdp=%p)\n", brdp);

	switch (brdp->brdtype) {
	case BRD_EASYIO:
	case BRD_EASYIOPCI:
		retval = stl_initeio(brdp);
		if (retval)
			goto err;
		break;
	case BRD_ECH:
	case BRD_ECHMC:
	case BRD_ECHPCI:
	case BRD_ECH64PCI:
		retval = stl_initech(brdp);
		if (retval)
			goto err;
		break;
	default:
		printk("STALLION: board=%d is unknown board type=%d\n",
			brdp->brdnr, brdp->brdtype);
		retval = -ENODEV;
		goto err;
	}

	if ((brdp->state & BRD_FOUND) == 0) {
		printk("STALLION: %s board not found, board=%d io=%x irq=%d\n",
			stl_brdnames[brdp->brdtype], brdp->brdnr,
			brdp->ioaddr1, brdp->irq);
		goto err_free;
	}

	for (i = 0; i < STL_MAXPANELS; i++)
		if (brdp->panels[i] != NULL)
			stl_initports(brdp, brdp->panels[i]);

	printk("STALLION: %s found, board=%d io=%x irq=%d "
		"nrpanels=%d nrports=%d\n", stl_brdnames[brdp->brdtype],
		brdp->brdnr, brdp->ioaddr1, brdp->irq, brdp->nrpanels,
		brdp->nrports);

	return 0;
err_free:
	free_irq(brdp->irq, brdp);

	stl_cleanup_panels(brdp);

	release_region(brdp->ioaddr1, brdp->iosize1);
	if (brdp->iosize2 > 0)
		release_region(brdp->ioaddr2, brdp->iosize2);
err:
	return retval;
}

/*****************************************************************************/

/*
 *	Find the next available board number that is free.
 */

static int __devinit stl_getbrdnr(void)
{
	unsigned int i;

	for (i = 0; i < STL_MAXBRDS; i++)
		if (stl_brds[i] == NULL) {
			if (i >= stl_nrbrds)
				stl_nrbrds = i + 1;
			return i;
		}

	return -1;
}

/*****************************************************************************/
/*
 *	We have a Stallion board. Allocate a board structure and
 *	initialize it. Read its IO and IRQ resources from PCI
 *	configuration space.
 */

static int __devinit stl_pciprobe(struct pci_dev *pdev,
		const struct pci_device_id *ent)
{
	struct stlbrd *brdp;
	unsigned int i, brdtype = ent->driver_data;
	int brdnr, retval = -ENODEV;

	if ((pdev->class >> 8) == PCI_CLASS_STORAGE_IDE)
		goto err;

	retval = pci_enable_device(pdev);
	if (retval)
		goto err;
	brdp = stl_allocbrd();
	if (brdp == NULL) {
		retval = -ENOMEM;
		goto err;
	}
	mutex_lock(&stl_brdslock);
	brdnr = stl_getbrdnr();
	if (brdnr < 0) {
		dev_err(&pdev->dev, "too many boards found, "
			"maximum supported %d\n", STL_MAXBRDS);
		mutex_unlock(&stl_brdslock);
		retval = -ENODEV;
		goto err_fr;
	}
	brdp->brdnr = (unsigned int)brdnr;
	stl_brds[brdp->brdnr] = brdp;
	mutex_unlock(&stl_brdslock);

	brdp->brdtype = brdtype;
	brdp->state |= STL_PROBED;

/*
 *	We have all resources from the board, so let's setup the actual
 *	board structure now.
 */
	switch (brdtype) {
	case BRD_ECHPCI:
		brdp->ioaddr2 = pci_resource_start(pdev, 0);
		brdp->ioaddr1 = pci_resource_start(pdev, 1);
		break;
	case BRD_ECH64PCI:
		brdp->ioaddr2 = pci_resource_start(pdev, 2);
		brdp->ioaddr1 = pci_resource_start(pdev, 1);
		break;
	case BRD_EASYIOPCI:
		brdp->ioaddr1 = pci_resource_start(pdev, 2);
		brdp->ioaddr2 = pci_resource_start(pdev, 1);
		break;
	default:
		dev_err(&pdev->dev, "unknown PCI board type=%u\n", brdtype);
		break;
	}

	brdp->irq = pdev->irq;
	retval = stl_brdinit(brdp);
	if (retval)
		goto err_null;

	pci_set_drvdata(pdev, brdp);

	for (i = 0; i < brdp->nrports; i++)
		tty_register_device(stl_serial,
				brdp->brdnr * STL_MAXPORTS + i, &pdev->dev);

	return 0;
err_null:
	stl_brds[brdp->brdnr] = NULL;
err_fr:
	kfree(brdp);
err:
	return retval;
}

static void __devexit stl_pciremove(struct pci_dev *pdev)
{
	struct stlbrd *brdp = pci_get_drvdata(pdev);
	unsigned int i;

	free_irq(brdp->irq, brdp);

	stl_cleanup_panels(brdp);

	release_region(brdp->ioaddr1, brdp->iosize1);
	if (brdp->iosize2 > 0)
		release_region(brdp->ioaddr2, brdp->iosize2);

	for (i = 0; i < brdp->nrports; i++)
		tty_unregister_device(stl_serial,
				brdp->brdnr * STL_MAXPORTS + i);

	stl_brds[brdp->brdnr] = NULL;
	kfree(brdp);
}

static struct pci_driver stl_pcidriver = {
	.name = "stallion",
	.id_table = stl_pcibrds,
	.probe = stl_pciprobe,
	.remove = __devexit_p(stl_pciremove)
};

/*****************************************************************************/

/*
 *	Return the board stats structure to user app.
 */

static int stl_getbrdstats(combrd_t __user *bp)
{
	combrd_t	stl_brdstats;
	struct stlbrd	*brdp;
	struct stlpanel	*panelp;
	unsigned int i;

	if (copy_from_user(&stl_brdstats, bp, sizeof(combrd_t)))
		return -EFAULT;
	if (stl_brdstats.brd >= STL_MAXBRDS)
		return -ENODEV;
	brdp = stl_brds[stl_brdstats.brd];
	if (brdp == NULL)
		return -ENODEV;

	memset(&stl_brdstats, 0, sizeof(combrd_t));
	stl_brdstats.brd = brdp->brdnr;
	stl_brdstats.type = brdp->brdtype;
	stl_brdstats.hwid = brdp->hwid;
	stl_brdstats.state = brdp->state;
	stl_brdstats.ioaddr = brdp->ioaddr1;
	stl_brdstats.ioaddr2 = brdp->ioaddr2;
	stl_brdstats.irq = brdp->irq;
	stl_brdstats.nrpanels = brdp->nrpanels;
	stl_brdstats.nrports = brdp->nrports;
	for (i = 0; i < brdp->nrpanels; i++) {
		panelp = brdp->panels[i];
		stl_brdstats.panels[i].panel = i;
		stl_brdstats.panels[i].hwid = panelp->hwid;
		stl_brdstats.panels[i].nrports = panelp->nrports;
	}

	return copy_to_user(bp, &stl_brdstats, sizeof(combrd_t)) ? -EFAULT : 0;
}

/*****************************************************************************/

/*
 *	Resolve the referenced port number into a port struct pointer.
 */

static struct stlport *stl_getport(int brdnr, int panelnr, int portnr)
{
	struct stlbrd	*brdp;
	struct stlpanel	*panelp;

	if (brdnr < 0 || brdnr >= STL_MAXBRDS)
		return NULL;
	brdp = stl_brds[brdnr];
	if (brdp == NULL)
		return NULL;
	if (panelnr < 0 || (unsigned int)panelnr >= brdp->nrpanels)
		return NULL;
	panelp = brdp->panels[panelnr];
	if (panelp == NULL)
		return NULL;
	if (portnr < 0 || (unsigned int)portnr >= panelp->nrports)
		return NULL;
	return panelp->ports[portnr];
}

/*****************************************************************************/

/*
 *	Return the port stats structure to user app. A NULL port struct
 *	pointer passed in means that we need to find out from the app
 *	what port to get stats for (used through board control device).
 */

static int stl_getportstats(struct tty_struct *tty, struct stlport *portp, comstats_t __user *cp)
{
	comstats_t	stl_comstats;
	unsigned char	*head, *tail;
	unsigned long	flags;

	if (!portp) {
		if (copy_from_user(&stl_comstats, cp, sizeof(comstats_t)))
			return -EFAULT;
		portp = stl_getport(stl_comstats.brd, stl_comstats.panel,
			stl_comstats.port);
		if (portp == NULL)
			return -ENODEV;
	}

	portp->stats.state = portp->istate;
	portp->stats.flags = portp->port.flags;
	portp->stats.hwid = portp->hwid;

	portp->stats.ttystate = 0;
	portp->stats.cflags = 0;
	portp->stats.iflags = 0;
	portp->stats.oflags = 0;
	portp->stats.lflags = 0;
	portp->stats.rxbuffered = 0;

	spin_lock_irqsave(&stallion_lock, flags);
	if (tty != NULL && portp->port.tty == tty) {
		portp->stats.ttystate = tty->flags;
		/* No longer available as a statistic */
		portp->stats.rxbuffered = 1; /*tty->flip.count; */
		if (tty->termios != NULL) {
			portp->stats.cflags = tty->termios->c_cflag;
			portp->stats.iflags = tty->termios->c_iflag;
			portp->stats.oflags = tty->termios->c_oflag;
			portp->stats.lflags = tty->termios->c_lflag;
		}
	}
	spin_unlock_irqrestore(&stallion_lock, flags);

	head = portp->tx.head;
	tail = portp->tx.tail;
	portp->stats.txbuffered = (head >= tail) ? (head - tail) :
		(STL_TXBUFSIZE - (tail - head));

	portp->stats.signals = (unsigned long) stl_getsignals(portp);

	return copy_to_user(cp, &portp->stats,
			    sizeof(comstats_t)) ? -EFAULT : 0;
}

/*****************************************************************************/

/*
 *	Clear the port stats structure. We also return it zeroed out...
 */

static int stl_clrportstats(struct stlport *portp, comstats_t __user *cp)
{
	comstats_t	stl_comstats;

	if (!portp) {
		if (copy_from_user(&stl_comstats, cp, sizeof(comstats_t)))
			return -EFAULT;
		portp = stl_getport(stl_comstats.brd, stl_comstats.panel,
			stl_comstats.port);
		if (portp == NULL)
			return -ENODEV;
	}

	memset(&portp->stats, 0, sizeof(comstats_t));
	portp->stats.brd = portp->brdnr;
	portp->stats.panel = portp->panelnr;
	portp->stats.port = portp->portnr;
	return copy_to_user(cp, &portp->stats,
			    sizeof(comstats_t)) ? -EFAULT : 0;
}

/*****************************************************************************/

/*
 *	Return the entire driver ports structure to a user app.
 */

static int stl_getportstruct(struct stlport __user *arg)
{
	struct stlport	stl_dummyport;
	struct stlport	*portp;

	if (copy_from_user(&stl_dummyport, arg, sizeof(struct stlport)))
		return -EFAULT;
	portp = stl_getport(stl_dummyport.brdnr, stl_dummyport.panelnr,
		 stl_dummyport.portnr);
	if (!portp)
		return -ENODEV;
	return copy_to_user(arg, portp, sizeof(struct stlport)) ? -EFAULT : 0;
}

/*****************************************************************************/

/*
 *	Return the entire driver board structure to a user app.
 */

static int stl_getbrdstruct(struct stlbrd __user *arg)
{
	struct stlbrd	stl_dummybrd;
	struct stlbrd	*brdp;

	if (copy_from_user(&stl_dummybrd, arg, sizeof(struct stlbrd)))
		return -EFAULT;
	if (stl_dummybrd.brdnr >= STL_MAXBRDS)
		return -ENODEV;
	brdp = stl_brds[stl_dummybrd.brdnr];
	if (!brdp)
		return -ENODEV;
	return copy_to_user(arg, brdp, sizeof(struct stlbrd)) ? -EFAULT : 0;
}

/*****************************************************************************/

/*
 *	The "staliomem" device is also required to do some special operations
 *	on the board and/or ports. In this driver it is mostly used for stats
 *	collection.
 */

static int stl_memioctl(struct inode *ip, struct file *fp, unsigned int cmd, unsigned long arg)
{
	int	brdnr, rc;
	void __user *argp = (void __user *)arg;

	pr_debug("stl_memioctl(ip=%p,fp=%p,cmd=%x,arg=%lx)\n", ip, fp, cmd,arg);

	brdnr = iminor(ip);
	if (brdnr >= STL_MAXBRDS)
		return -ENODEV;
	rc = 0;

	switch (cmd) {
	case COM_GETPORTSTATS:
		rc = stl_getportstats(NULL, NULL, argp);
		break;
	case COM_CLRPORTSTATS:
		rc = stl_clrportstats(NULL, argp);
		break;
	case COM_GETBRDSTATS:
		rc = stl_getbrdstats(argp);
		break;
	case COM_READPORT:
		rc = stl_getportstruct(argp);
		break;
	case COM_READBOARD:
		rc = stl_getbrdstruct(argp);
		break;
	default:
		rc = -ENOIOCTLCMD;
		break;
	}

	return rc;
}

static const struct tty_operations stl_ops = {
	.open = stl_open,
	.close = stl_close,
	.write = stl_write,
	.put_char = stl_putchar,
	.flush_chars = stl_flushchars,
	.write_room = stl_writeroom,
	.chars_in_buffer = stl_charsinbuffer,
	.ioctl = stl_ioctl,
	.set_termios = stl_settermios,
	.throttle = stl_throttle,
	.unthrottle = stl_unthrottle,
	.stop = stl_stop,
	.start = stl_start,
	.hangup = stl_hangup,
	.flush_buffer = stl_flushbuffer,
	.break_ctl = stl_breakctl,
	.wait_until_sent = stl_waituntilsent,
	.send_xchar = stl_sendxchar,
	.tiocmget = stl_tiocmget,
	.tiocmset = stl_tiocmset,
	.proc_fops = &stl_proc_fops,
};

static const struct tty_port_operations stl_port_ops = {
	.carrier_raised = stl_carrier_raised,
	.dtr_rts = stl_dtr_rts,
};

/*****************************************************************************/
/*                       CD1400 HARDWARE FUNCTIONS                           */
/*****************************************************************************/

/*
 *	These functions get/set/update the registers of the cd1400 UARTs.
 *	Access to the cd1400 registers is via an address/data io port pair.
 *	(Maybe should make this inline...)
 */

static int stl_cd1400getreg(struct stlport *portp, int regnr)
{
	outb((regnr + portp->uartaddr), portp->ioaddr);
	return inb(portp->ioaddr + EREG_DATA);
}

static void stl_cd1400setreg(struct stlport *portp, int regnr, int value)
{
	outb(regnr + portp->uartaddr, portp->ioaddr);
	outb(value, portp->ioaddr + EREG_DATA);
}

static int stl_cd1400updatereg(struct stlport *portp, int regnr, int value)
{
	outb(regnr + portp->uartaddr, portp->ioaddr);
	if (inb(portp->ioaddr + EREG_DATA) != value) {
		outb(value, portp->ioaddr + EREG_DATA);
		return 1;
	}
	return 0;
}

/*****************************************************************************/

/*
 *	Inbitialize the UARTs in a panel. We don't care what sort of board
 *	these ports are on - since the port io registers are almost
 *	identical when dealing with ports.
 */

static int stl_cd1400panelinit(struct stlbrd *brdp, struct stlpanel *panelp)
{
	unsigned int	gfrcr;
	int		chipmask, i, j;
	int		nrchips, uartaddr, ioaddr;
	unsigned long   flags;

	pr_debug("stl_panelinit(brdp=%p,panelp=%p)\n", brdp, panelp);

	spin_lock_irqsave(&brd_lock, flags);
	BRDENABLE(panelp->brdnr, panelp->pagenr);

/*
 *	Check that each chip is present and started up OK.
 */
	chipmask = 0;
	nrchips = panelp->nrports / CD1400_PORTS;
	for (i = 0; i < nrchips; i++) {
		if (brdp->brdtype == BRD_ECHPCI) {
			outb((panelp->pagenr + (i >> 1)), brdp->ioctrl);
			ioaddr = panelp->iobase;
		} else
			ioaddr = panelp->iobase + (EREG_BANKSIZE * (i >> 1));
		uartaddr = (i & 0x01) ? 0x080 : 0;
		outb((GFRCR + uartaddr), ioaddr);
		outb(0, (ioaddr + EREG_DATA));
		outb((CCR + uartaddr), ioaddr);
		outb(CCR_RESETFULL, (ioaddr + EREG_DATA));
		outb(CCR_RESETFULL, (ioaddr + EREG_DATA));
		outb((GFRCR + uartaddr), ioaddr);
		for (j = 0; j < CCR_MAXWAIT; j++)
			if ((gfrcr = inb(ioaddr + EREG_DATA)) != 0)
				break;

		if ((j >= CCR_MAXWAIT) || (gfrcr < 0x40) || (gfrcr > 0x60)) {
			printk("STALLION: cd1400 not responding, "
				"brd=%d panel=%d chip=%d\n",
				panelp->brdnr, panelp->panelnr, i);
			continue;
		}
		chipmask |= (0x1 << i);
		outb((PPR + uartaddr), ioaddr);
		outb(PPR_SCALAR, (ioaddr + EREG_DATA));
	}

	BRDDISABLE(panelp->brdnr);
	spin_unlock_irqrestore(&brd_lock, flags);
	return chipmask;
}

/*****************************************************************************/

/*
 *	Initialize hardware specific port registers.
 */

static void stl_cd1400portinit(struct stlbrd *brdp, struct stlpanel *panelp, struct stlport *portp)
{
	unsigned long flags;
	pr_debug("stl_cd1400portinit(brdp=%p,panelp=%p,portp=%p)\n", brdp,
			panelp, portp);

	if ((brdp == NULL) || (panelp == NULL) ||
	    (portp == NULL))
		return;

	spin_lock_irqsave(&brd_lock, flags);
	portp->ioaddr = panelp->iobase + (((brdp->brdtype == BRD_ECHPCI) ||
		(portp->portnr < 8)) ? 0 : EREG_BANKSIZE);
	portp->uartaddr = (portp->portnr & 0x04) << 5;
	portp->pagenr = panelp->pagenr + (portp->portnr >> 3);

	BRDENABLE(portp->brdnr, portp->pagenr);
	stl_cd1400setreg(portp, CAR, (portp->portnr & 0x03));
	stl_cd1400setreg(portp, LIVR, (portp->portnr << 3));
	portp->hwid = stl_cd1400getreg(portp, GFRCR);
	BRDDISABLE(portp->brdnr);
	spin_unlock_irqrestore(&brd_lock, flags);
}

/*****************************************************************************/

/*
 *	Wait for the command register to be ready. We will poll this,
 *	since it won't usually take too long to be ready.
 */

static void stl_cd1400ccrwait(struct stlport *portp)
{
	int	i;

	for (i = 0; i < CCR_MAXWAIT; i++)
		if (stl_cd1400getreg(portp, CCR) == 0)
			return;

	printk("STALLION: cd1400 not responding, port=%d panel=%d brd=%d\n",
		portp->portnr, portp->panelnr, portp->brdnr);
}

/*****************************************************************************/

/*
 *	Set up the cd1400 registers for a port based on the termios port
 *	settings.
 */

static void stl_cd1400setport(struct stlport *portp, struct ktermios *tiosp)
{
	struct stlbrd	*brdp;
	unsigned long	flags;
	unsigned int	clkdiv, baudrate;
	unsigned char	cor1, cor2, cor3;
	unsigned char	cor4, cor5, ccr;
	unsigned char	srer, sreron, sreroff;
	unsigned char	mcor1, mcor2, rtpr;
	unsigned char	clk, div;

	cor1 = 0;
	cor2 = 0;
	cor3 = 0;
	cor4 = 0;
	cor5 = 0;
	ccr = 0;
	rtpr = 0;
	clk = 0;
	div = 0;
	mcor1 = 0;
	mcor2 = 0;
	sreron = 0;
	sreroff = 0;

	brdp = stl_brds[portp->brdnr];
	if (brdp == NULL)
		return;

/*
 *	Set up the RX char ignore mask with those RX error types we
 *	can ignore. We can get the cd1400 to help us out a little here,
 *	it will ignore parity errors and breaks for us.
 */
	portp->rxignoremsk = 0;
	if (tiosp->c_iflag & IGNPAR) {
		portp->rxignoremsk |= (ST_PARITY | ST_FRAMING | ST_OVERRUN);
		cor1 |= COR1_PARIGNORE;
	}
	if (tiosp->c_iflag & IGNBRK) {
		portp->rxignoremsk |= ST_BREAK;
		cor4 |= COR4_IGNBRK;
	}

	portp->rxmarkmsk = ST_OVERRUN;
	if (tiosp->c_iflag & (INPCK | PARMRK))
		portp->rxmarkmsk |= (ST_PARITY | ST_FRAMING);
	if (tiosp->c_iflag & BRKINT)
		portp->rxmarkmsk |= ST_BREAK;

/*
 *	Go through the char size, parity and stop bits and set all the
 *	option register appropriately.
 */
	switch (tiosp->c_cflag & CSIZE) {
	case CS5:
		cor1 |= COR1_CHL5;
		break;
	case CS6:
		cor1 |= COR1_CHL6;
		break;
	case CS7:
		cor1 |= COR1_CHL7;
		break;
	default:
		cor1 |= COR1_CHL8;
		break;
	}

	if (tiosp->c_cflag & CSTOPB)
		cor1 |= COR1_STOP2;
	else
		cor1 |= COR1_STOP1;

	if (tiosp->c_cflag & PARENB) {
		if (tiosp->c_cflag & PARODD)
			cor1 |= (COR1_PARENB | COR1_PARODD);
		else
			cor1 |= (COR1_PARENB | COR1_PAREVEN);
	} else {
		cor1 |= COR1_PARNONE;
	}

/*
 *	Set the RX FIFO threshold at 6 chars. This gives a bit of breathing
 *	space for hardware flow control and the like. This should be set to
 *	VMIN. Also here we will set the RX data timeout to 10ms - this should
 *	really be based on VTIME.
 */
	cor3 |= FIFO_RXTHRESHOLD;
	rtpr = 2;

/*
 *	Calculate the baud rate timers. For now we will just assume that
 *	the input and output baud are the same. Could have used a baud
 *	table here, but this way we can generate virtually any baud rate
 *	we like!
 */
	baudrate = tiosp->c_cflag & CBAUD;
	if (baudrate & CBAUDEX) {
		baudrate &= ~CBAUDEX;
		if ((baudrate < 1) || (baudrate > 4))
			tiosp->c_cflag &= ~CBAUDEX;
		else
			baudrate += 15;
	}
	baudrate = stl_baudrates[baudrate];
	if ((tiosp->c_cflag & CBAUD) == B38400) {
		if ((portp->port.flags & ASYNC_SPD_MASK) == ASYNC_SPD_HI)
			baudrate = 57600;
		else if ((portp->port.flags & ASYNC_SPD_MASK) == ASYNC_SPD_VHI)
			baudrate = 115200;
		else if ((portp->port.flags & ASYNC_SPD_MASK) == ASYNC_SPD_SHI)
			baudrate = 230400;
		else if ((portp->port.flags & ASYNC_SPD_MASK) == ASYNC_SPD_WARP)
			baudrate = 460800;
		else if ((portp->port.flags & ASYNC_SPD_MASK) == ASYNC_SPD_CUST)
			baudrate = (portp->baud_base / portp->custom_divisor);
	}
	if (baudrate > STL_CD1400MAXBAUD)
		baudrate = STL_CD1400MAXBAUD;

	if (baudrate > 0) {
		for (clk = 0; clk < CD1400_NUMCLKS; clk++) {
			clkdiv = (portp->clk / stl_cd1400clkdivs[clk]) / baudrate;
			if (clkdiv < 0x100)
				break;
		}
		div = (unsigned char) clkdiv;
	}

/*
 *	Check what form of modem signaling is required and set it up.
 */
	if ((tiosp->c_cflag & CLOCAL) == 0) {
		mcor1 |= MCOR1_DCD;
		mcor2 |= MCOR2_DCD;
		sreron |= SRER_MODEM;
		portp->port.flags |= ASYNC_CHECK_CD;
	} else
		portp->port.flags &= ~ASYNC_CHECK_CD;

/*
 *	Setup cd1400 enhanced modes if we can. In particular we want to
 *	handle as much of the flow control as possible automatically. As
 *	well as saving a few CPU cycles it will also greatly improve flow
 *	control reliability.
 */
	if (tiosp->c_iflag & IXON) {
		cor2 |= COR2_TXIBE;
		cor3 |= COR3_SCD12;
		if (tiosp->c_iflag & IXANY)
			cor2 |= COR2_IXM;
	}

	if (tiosp->c_cflag & CRTSCTS) {
		cor2 |= COR2_CTSAE;
		mcor1 |= FIFO_RTSTHRESHOLD;
	}

/*
 *	All cd1400 register values calculated so go through and set
 *	them all up.
 */

	pr_debug("SETPORT: portnr=%d panelnr=%d brdnr=%d\n",
		portp->portnr, portp->panelnr, portp->brdnr);
	pr_debug("    cor1=%x cor2=%x cor3=%x cor4=%x cor5=%x\n",
		cor1, cor2, cor3, cor4, cor5);
	pr_debug("    mcor1=%x mcor2=%x rtpr=%x sreron=%x sreroff=%x\n",
		mcor1, mcor2, rtpr, sreron, sreroff);
	pr_debug("    tcor=%x tbpr=%x rcor=%x rbpr=%x\n", clk, div, clk, div);
	pr_debug("    schr1=%x schr2=%x schr3=%x schr4=%x\n",
		tiosp->c_cc[VSTART], tiosp->c_cc[VSTOP],
		tiosp->c_cc[VSTART], tiosp->c_cc[VSTOP]);

	spin_lock_irqsave(&brd_lock, flags);
	BRDENABLE(portp->brdnr, portp->pagenr);
	stl_cd1400setreg(portp, CAR, (portp->portnr & 0x3));
	srer = stl_cd1400getreg(portp, SRER);
	stl_cd1400setreg(portp, SRER, 0);
	if (stl_cd1400updatereg(portp, COR1, cor1))
		ccr = 1;
	if (stl_cd1400updatereg(portp, COR2, cor2))
		ccr = 1;
	if (stl_cd1400updatereg(portp, COR3, cor3))
		ccr = 1;
	if (ccr) {
		stl_cd1400ccrwait(portp);
		stl_cd1400setreg(portp, CCR, CCR_CORCHANGE);
	}
	stl_cd1400setreg(portp, COR4, cor4);
	stl_cd1400setreg(portp, COR5, cor5);
	stl_cd1400setreg(portp, MCOR1, mcor1);
	stl_cd1400setreg(portp, MCOR2, mcor2);
	if (baudrate > 0) {
		stl_cd1400setreg(portp, TCOR, clk);
		stl_cd1400setreg(portp, TBPR, div);
		stl_cd1400setreg(portp, RCOR, clk);
		stl_cd1400setreg(portp, RBPR, div);
	}
	stl_cd1400setreg(portp, SCHR1, tiosp->c_cc[VSTART]);
	stl_cd1400setreg(portp, SCHR2, tiosp->c_cc[VSTOP]);
	stl_cd1400setreg(portp, SCHR3, tiosp->c_cc[VSTART]);
	stl_cd1400setreg(portp, SCHR4, tiosp->c_cc[VSTOP]);
	stl_cd1400setreg(portp, RTPR, rtpr);
	mcor1 = stl_cd1400getreg(portp, MSVR1);
	if (mcor1 & MSVR1_DCD)
		portp->sigs |= TIOCM_CD;
	else
		portp->sigs &= ~TIOCM_CD;
	stl_cd1400setreg(portp, SRER, ((srer & ~sreroff) | sreron));
	BRDDISABLE(portp->brdnr);
	spin_unlock_irqrestore(&brd_lock, flags);
}

/*****************************************************************************/

/*
 *	Set the state of the DTR and RTS signals.
 */

static void stl_cd1400setsignals(struct stlport *portp, int dtr, int rts)
{
	unsigned char	msvr1, msvr2;
	unsigned long	flags;

	pr_debug("stl_cd1400setsignals(portp=%p,dtr=%d,rts=%d)\n",
			portp, dtr, rts);

	msvr1 = 0;
	msvr2 = 0;
	if (dtr > 0)
		msvr1 = MSVR1_DTR;
	if (rts > 0)
		msvr2 = MSVR2_RTS;

	spin_lock_irqsave(&brd_lock, flags);
	BRDENABLE(portp->brdnr, portp->pagenr);
	stl_cd1400setreg(portp, CAR, (portp->portnr & 0x03));
	if (rts >= 0)
		stl_cd1400setreg(portp, MSVR2, msvr2);
	if (dtr >= 0)
		stl_cd1400setreg(portp, MSVR1, msvr1);
	BRDDISABLE(portp->brdnr);
	spin_unlock_irqrestore(&brd_lock, flags);
}

/*****************************************************************************/

/*
 *	Return the state of the signals.
 */

static int stl_cd1400getsignals(struct stlport *portp)
{
	unsigned char	msvr1, msvr2;
	unsigned long	flags;
	int		sigs;

	pr_debug("stl_cd1400getsignals(portp=%p)\n", portp);

	spin_lock_irqsave(&brd_lock, flags);
	BRDENABLE(portp->brdnr, portp->pagenr);
	stl_cd1400setreg(portp, CAR, (portp->portnr & 0x03));
	msvr1 = stl_cd1400getreg(portp, MSVR1);
	msvr2 = stl_cd1400getreg(portp, MSVR2);
	BRDDISABLE(portp->brdnr);
	spin_unlock_irqrestore(&brd_lock, flags);

	sigs = 0;
	sigs |= (msvr1 & MSVR1_DCD) ? TIOCM_CD : 0;
	sigs |= (msvr1 & MSVR1_CTS) ? TIOCM_CTS : 0;
	sigs |= (msvr1 & MSVR1_DTR) ? TIOCM_DTR : 0;
	sigs |= (msvr2 & MSVR2_RTS) ? TIOCM_RTS : 0;
#if 0
	sigs |= (msvr1 & MSVR1_RI) ? TIOCM_RI : 0;
	sigs |= (msvr1 & MSVR1_DSR) ? TIOCM_DSR : 0;
#else
	sigs |= TIOCM_DSR;
#endif
	return sigs;
}

/*****************************************************************************/

/*
 *	Enable/Disable the Transmitter and/or Receiver.
 */

static void stl_cd1400enablerxtx(struct stlport *portp, int rx, int tx)
{
	unsigned char	ccr;
	unsigned long	flags;

	pr_debug("stl_cd1400enablerxtx(portp=%p,rx=%d,tx=%d)\n", portp, rx, tx);

	ccr = 0;

	if (tx == 0)
		ccr |= CCR_TXDISABLE;
	else if (tx > 0)
		ccr |= CCR_TXENABLE;
	if (rx == 0)
		ccr |= CCR_RXDISABLE;
	else if (rx > 0)
		ccr |= CCR_RXENABLE;

	spin_lock_irqsave(&brd_lock, flags);
	BRDENABLE(portp->brdnr, portp->pagenr);
	stl_cd1400setreg(portp, CAR, (portp->portnr & 0x03));
	stl_cd1400ccrwait(portp);
	stl_cd1400setreg(portp, CCR, ccr);
	stl_cd1400ccrwait(portp);
	BRDDISABLE(portp->brdnr);
	spin_unlock_irqrestore(&brd_lock, flags);
}

/*****************************************************************************/

/*
 *	Start/stop the Transmitter and/or Receiver.
 */

static void stl_cd1400startrxtx(struct stlport *portp, int rx, int tx)
{
	unsigned char	sreron, sreroff;
	unsigned long	flags;

	pr_debug("stl_cd1400startrxtx(portp=%p,rx=%d,tx=%d)\n", portp, rx, tx);

	sreron = 0;
	sreroff = 0;
	if (tx == 0)
		sreroff |= (SRER_TXDATA | SRER_TXEMPTY);
	else if (tx == 1)
		sreron |= SRER_TXDATA;
	else if (tx >= 2)
		sreron |= SRER_TXEMPTY;
	if (rx == 0)
		sreroff |= SRER_RXDATA;
	else if (rx > 0)
		sreron |= SRER_RXDATA;

	spin_lock_irqsave(&brd_lock, flags);
	BRDENABLE(portp->brdnr, portp->pagenr);
	stl_cd1400setreg(portp, CAR, (portp->portnr & 0x03));
	stl_cd1400setreg(portp, SRER,
		((stl_cd1400getreg(portp, SRER) & ~sreroff) | sreron));
	BRDDISABLE(portp->brdnr);
	if (tx > 0)
		set_bit(ASYI_TXBUSY, &portp->istate);
	spin_unlock_irqrestore(&brd_lock, flags);
}

/*****************************************************************************/

/*
 *	Disable all interrupts from this port.
 */

static void stl_cd1400disableintrs(struct stlport *portp)
{
	unsigned long	flags;

	pr_debug("stl_cd1400disableintrs(portp=%p)\n", portp);

	spin_lock_irqsave(&brd_lock, flags);
	BRDENABLE(portp->brdnr, portp->pagenr);
	stl_cd1400setreg(portp, CAR, (portp->portnr & 0x03));
	stl_cd1400setreg(portp, SRER, 0);
	BRDDISABLE(portp->brdnr);
	spin_unlock_irqrestore(&brd_lock, flags);
}

/*****************************************************************************/

static void stl_cd1400sendbreak(struct stlport *portp, int len)
{
	unsigned long	flags;

	pr_debug("stl_cd1400sendbreak(portp=%p,len=%d)\n", portp, len);

	spin_lock_irqsave(&brd_lock, flags);
	BRDENABLE(portp->brdnr, portp->pagenr);
	stl_cd1400setreg(portp, CAR, (portp->portnr & 0x03));
	stl_cd1400setreg(portp, SRER,
		((stl_cd1400getreg(portp, SRER) & ~SRER_TXDATA) |
		SRER_TXEMPTY));
	BRDDISABLE(portp->brdnr);
	portp->brklen = len;
	if (len == 1)
		portp->stats.txbreaks++;
	spin_unlock_irqrestore(&brd_lock, flags);
}

/*****************************************************************************/

/*
 *	Take flow control actions...
 */

static void stl_cd1400flowctrl(struct stlport *portp, int state)
{
	struct tty_struct	*tty;
	unsigned long		flags;

	pr_debug("stl_cd1400flowctrl(portp=%p,state=%x)\n", portp, state);

	if (portp == NULL)
		return;
	tty = tty_port_tty_get(&portp->port);
	if (tty == NULL)
		return;

	spin_lock_irqsave(&brd_lock, flags);
	BRDENABLE(portp->brdnr, portp->pagenr);
	stl_cd1400setreg(portp, CAR, (portp->portnr & 0x03));

	if (state) {
		if (tty->termios->c_iflag & IXOFF) {
			stl_cd1400ccrwait(portp);
			stl_cd1400setreg(portp, CCR, CCR_SENDSCHR1);
			portp->stats.rxxon++;
			stl_cd1400ccrwait(portp);
		}
/*
 *		Question: should we return RTS to what it was before? It may
 *		have been set by an ioctl... Suppose not, since if you have
 *		hardware flow control set then it is pretty silly to go and
 *		set the RTS line by hand.
 */
		if (tty->termios->c_cflag & CRTSCTS) {
			stl_cd1400setreg(portp, MCOR1,
				(stl_cd1400getreg(portp, MCOR1) |
				FIFO_RTSTHRESHOLD));
			stl_cd1400setreg(portp, MSVR2, MSVR2_RTS);
			portp->stats.rxrtson++;
		}
	} else {
		if (tty->termios->c_iflag & IXOFF) {
			stl_cd1400ccrwait(portp);
			stl_cd1400setreg(portp, CCR, CCR_SENDSCHR2);
			portp->stats.rxxoff++;
			stl_cd1400ccrwait(portp);
		}
		if (tty->termios->c_cflag & CRTSCTS) {
			stl_cd1400setreg(portp, MCOR1,
				(stl_cd1400getreg(portp, MCOR1) & 0xf0));
			stl_cd1400setreg(portp, MSVR2, 0);
			portp->stats.rxrtsoff++;
		}
	}

	BRDDISABLE(portp->brdnr);
	spin_unlock_irqrestore(&brd_lock, flags);
	tty_kref_put(tty);
}

/*****************************************************************************/

/*
 *	Send a flow control character...
 */

static void stl_cd1400sendflow(struct stlport *portp, int state)
{
	struct tty_struct	*tty;
	unsigned long		flags;

	pr_debug("stl_cd1400sendflow(portp=%p,state=%x)\n", portp, state);

	if (portp == NULL)
		return;
	tty = tty_port_tty_get(&portp->port);
	if (tty == NULL)
		return;

	spin_lock_irqsave(&brd_lock, flags);
	BRDENABLE(portp->brdnr, portp->pagenr);
	stl_cd1400setreg(portp, CAR, (portp->portnr & 0x03));
	if (state) {
		stl_cd1400ccrwait(portp);
		stl_cd1400setreg(portp, CCR, CCR_SENDSCHR1);
		portp->stats.rxxon++;
		stl_cd1400ccrwait(portp);
	} else {
		stl_cd1400ccrwait(portp);
		stl_cd1400setreg(portp, CCR, CCR_SENDSCHR2);
		portp->stats.rxxoff++;
		stl_cd1400ccrwait(portp);
	}
	BRDDISABLE(portp->brdnr);
	spin_unlock_irqrestore(&brd_lock, flags);
	tty_kref_put(tty);
}

/*****************************************************************************/

static void stl_cd1400flush(struct stlport *portp)
{
	unsigned long	flags;

	pr_debug("stl_cd1400flush(portp=%p)\n", portp);

	if (portp == NULL)
		return;

	spin_lock_irqsave(&brd_lock, flags);
	BRDENABLE(portp->brdnr, portp->pagenr);
	stl_cd1400setreg(portp, CAR, (portp->portnr & 0x03));
	stl_cd1400ccrwait(portp);
	stl_cd1400setreg(portp, CCR, CCR_TXFLUSHFIFO);
	stl_cd1400ccrwait(portp);
	portp->tx.tail = portp->tx.head;
	BRDDISABLE(portp->brdnr);
	spin_unlock_irqrestore(&brd_lock, flags);
}

/*****************************************************************************/

/*
 *	Return the current state of data flow on this port. This is only
 *	really interresting when determining if data has fully completed
 *	transmission or not... This is easy for the cd1400, it accurately
 *	maintains the busy port flag.
 */

static int stl_cd1400datastate(struct stlport *portp)
{
	pr_debug("stl_cd1400datastate(portp=%p)\n", portp);

	if (portp == NULL)
		return 0;

	return test_bit(ASYI_TXBUSY, &portp->istate) ? 1 : 0;
}

/*****************************************************************************/

/*
 *	Interrupt service routine for cd1400 EasyIO boards.
 */

static void stl_cd1400eiointr(struct stlpanel *panelp, unsigned int iobase)
{
	unsigned char	svrtype;

	pr_debug("stl_cd1400eiointr(panelp=%p,iobase=%x)\n", panelp, iobase);

	spin_lock(&brd_lock);
	outb(SVRR, iobase);
	svrtype = inb(iobase + EREG_DATA);
	if (panelp->nrports > 4) {
		outb((SVRR + 0x80), iobase);
		svrtype |= inb(iobase + EREG_DATA);
	}

	if (svrtype & SVRR_RX)
		stl_cd1400rxisr(panelp, iobase);
	else if (svrtype & SVRR_TX)
		stl_cd1400txisr(panelp, iobase);
	else if (svrtype & SVRR_MDM)
		stl_cd1400mdmisr(panelp, iobase);

	spin_unlock(&brd_lock);
}

/*****************************************************************************/

/*
 *	Interrupt service routine for cd1400 panels.
 */

static void stl_cd1400echintr(struct stlpanel *panelp, unsigned int iobase)
{
	unsigned char	svrtype;

	pr_debug("stl_cd1400echintr(panelp=%p,iobase=%x)\n", panelp, iobase);

	outb(SVRR, iobase);
	svrtype = inb(iobase + EREG_DATA);
	outb((SVRR + 0x80), iobase);
	svrtype |= inb(iobase + EREG_DATA);
	if (svrtype & SVRR_RX)
		stl_cd1400rxisr(panelp, iobase);
	else if (svrtype & SVRR_TX)
		stl_cd1400txisr(panelp, iobase);
	else if (svrtype & SVRR_MDM)
		stl_cd1400mdmisr(panelp, iobase);
}


/*****************************************************************************/

/*
 *	Unfortunately we need to handle breaks in the TX data stream, since
 *	this is the only way to generate them on the cd1400.
 */

static int stl_cd1400breakisr(struct stlport *portp, int ioaddr)
{
	if (portp->brklen == 1) {
		outb((COR2 + portp->uartaddr), ioaddr);
		outb((inb(ioaddr + EREG_DATA) | COR2_ETC),
			(ioaddr + EREG_DATA));
		outb((TDR + portp->uartaddr), ioaddr);
		outb(ETC_CMD, (ioaddr + EREG_DATA));
		outb(ETC_STARTBREAK, (ioaddr + EREG_DATA));
		outb((SRER + portp->uartaddr), ioaddr);
		outb((inb(ioaddr + EREG_DATA) & ~(SRER_TXDATA | SRER_TXEMPTY)),
			(ioaddr + EREG_DATA));
		return 1;
	} else if (portp->brklen > 1) {
		outb((TDR + portp->uartaddr), ioaddr);
		outb(ETC_CMD, (ioaddr + EREG_DATA));
		outb(ETC_STOPBREAK, (ioaddr + EREG_DATA));
		portp->brklen = -1;
		return 1;
	} else {
		outb((COR2 + portp->uartaddr), ioaddr);
		outb((inb(ioaddr + EREG_DATA) & ~COR2_ETC),
			(ioaddr + EREG_DATA));
		portp->brklen = 0;
	}
	return 0;
}

/*****************************************************************************/

/*
 *	Transmit interrupt handler. This has gotta be fast!  Handling TX
 *	chars is pretty simple, stuff as many as possible from the TX buffer
 *	into the cd1400 FIFO. Must also handle TX breaks here, since they
 *	are embedded as commands in the data stream. Oh no, had to use a goto!
 *	This could be optimized more, will do when I get time...
 *	In practice it is possible that interrupts are enabled but that the
 *	port has been hung up. Need to handle not having any TX buffer here,
 *	this is done by using the side effect that head and tail will also
 *	be NULL if the buffer has been freed.
 */

static void stl_cd1400txisr(struct stlpanel *panelp, int ioaddr)
{
	struct stlport	*portp;
	int		len, stlen;
	char		*head, *tail;
	unsigned char	ioack, srer;
	struct tty_struct *tty;

	pr_debug("stl_cd1400txisr(panelp=%p,ioaddr=%x)\n", panelp, ioaddr);

	ioack = inb(ioaddr + EREG_TXACK);
	if (((ioack & panelp->ackmask) != 0) ||
	    ((ioack & ACK_TYPMASK) != ACK_TYPTX)) {
		printk("STALLION: bad TX interrupt ack value=%x\n", ioack);
		return;
	}
	portp = panelp->ports[(ioack >> 3)];

/*
 *	Unfortunately we need to handle breaks in the data stream, since
 *	this is the only way to generate them on the cd1400. Do it now if
 *	a break is to be sent.
 */
	if (portp->brklen != 0)
		if (stl_cd1400breakisr(portp, ioaddr))
			goto stl_txalldone;

	head = portp->tx.head;
	tail = portp->tx.tail;
	len = (head >= tail) ? (head - tail) : (STL_TXBUFSIZE - (tail - head));
	if ((len == 0) || ((len < STL_TXBUFLOW) &&
	    (test_bit(ASYI_TXLOW, &portp->istate) == 0))) {
		set_bit(ASYI_TXLOW, &portp->istate);
		tty = tty_port_tty_get(&portp->port);
		if (tty) {
			tty_wakeup(tty);
			tty_kref_put(tty);
		}
	}

	if (len == 0) {
		outb((SRER + portp->uartaddr), ioaddr);
		srer = inb(ioaddr + EREG_DATA);
		if (srer & SRER_TXDATA) {
			srer = (srer & ~SRER_TXDATA) | SRER_TXEMPTY;
		} else {
			srer &= ~(SRER_TXDATA | SRER_TXEMPTY);
			clear_bit(ASYI_TXBUSY, &portp->istate);
		}
		outb(srer, (ioaddr + EREG_DATA));
	} else {
		len = min(len, CD1400_TXFIFOSIZE);
		portp->stats.txtotal += len;
		stlen = min_t(unsigned int, len,
				(portp->tx.buf + STL_TXBUFSIZE) - tail);
		outb((TDR + portp->uartaddr), ioaddr);
		outsb((ioaddr + EREG_DATA), tail, stlen);
		len -= stlen;
		tail += stlen;
		if (tail >= (portp->tx.buf + STL_TXBUFSIZE))
			tail = portp->tx.buf;
		if (len > 0) {
			outsb((ioaddr + EREG_DATA), tail, len);
			tail += len;
		}
		portp->tx.tail = tail;
	}

stl_txalldone:
	outb((EOSRR + portp->uartaddr), ioaddr);
	outb(0, (ioaddr + EREG_DATA));
}

/*****************************************************************************/

/*
 *	Receive character interrupt handler. Determine if we have good chars
 *	or bad chars and then process appropriately. Good chars are easy
 *	just shove the lot into the RX buffer and set all status byte to 0.
 *	If a bad RX char then process as required. This routine needs to be
 *	fast!  In practice it is possible that we get an interrupt on a port
 *	that is closed. This can happen on hangups - since they completely
 *	shutdown a port not in user context. Need to handle this case.
 */

static void stl_cd1400rxisr(struct stlpanel *panelp, int ioaddr)
{
	struct stlport		*portp;
	struct tty_struct	*tty;
	unsigned int		ioack, len, buflen;
	unsigned char		status;
	char			ch;

	pr_debug("stl_cd1400rxisr(panelp=%p,ioaddr=%x)\n", panelp, ioaddr);

	ioack = inb(ioaddr + EREG_RXACK);
	if ((ioack & panelp->ackmask) != 0) {
		printk("STALLION: bad RX interrupt ack value=%x\n", ioack);
		return;
	}
	portp = panelp->ports[(ioack >> 3)];
	tty = tty_port_tty_get(&portp->port);

	if ((ioack & ACK_TYPMASK) == ACK_TYPRXGOOD) {
		outb((RDCR + portp->uartaddr), ioaddr);
		len = inb(ioaddr + EREG_DATA);
		if (tty == NULL || (buflen = tty_buffer_request_room(tty, len)) == 0) {
			len = min_t(unsigned int, len, sizeof(stl_unwanted));
			outb((RDSR + portp->uartaddr), ioaddr);
			insb((ioaddr + EREG_DATA), &stl_unwanted[0], len);
			portp->stats.rxlost += len;
			portp->stats.rxtotal += len;
		} else {
			len = min(len, buflen);
			if (len > 0) {
				unsigned char *ptr;
				outb((RDSR + portp->uartaddr), ioaddr);
				tty_prepare_flip_string(tty, &ptr, len);
				insb((ioaddr + EREG_DATA), ptr, len);
				tty_schedule_flip(tty);
				portp->stats.rxtotal += len;
			}
		}
	} else if ((ioack & ACK_TYPMASK) == ACK_TYPRXBAD) {
		outb((RDSR + portp->uartaddr), ioaddr);
		status = inb(ioaddr + EREG_DATA);
		ch = inb(ioaddr + EREG_DATA);
		if (status & ST_PARITY)
			portp->stats.rxparity++;
		if (status & ST_FRAMING)
			portp->stats.rxframing++;
		if (status & ST_OVERRUN)
			portp->stats.rxoverrun++;
		if (status & ST_BREAK)
			portp->stats.rxbreaks++;
		if (status & ST_SCHARMASK) {
			if ((status & ST_SCHARMASK) == ST_SCHAR1)
				portp->stats.txxon++;
			if ((status & ST_SCHARMASK) == ST_SCHAR2)
				portp->stats.txxoff++;
			goto stl_rxalldone;
		}
		if (tty != NULL && (portp->rxignoremsk & status) == 0) {
			if (portp->rxmarkmsk & status) {
				if (status & ST_BREAK) {
					status = TTY_BREAK;
					if (portp->port.flags & ASYNC_SAK) {
						do_SAK(tty);
						BRDENABLE(portp->brdnr, portp->pagenr);
					}
				} else if (status & ST_PARITY)
					status = TTY_PARITY;
				else if (status & ST_FRAMING)
					status = TTY_FRAME;
				else if(status & ST_OVERRUN)
					status = TTY_OVERRUN;
				else
					status = 0;
			} else
				status = 0;
			tty_insert_flip_char(tty, ch, status);
			tty_schedule_flip(tty);
		}
	} else {
		printk("STALLION: bad RX interrupt ack value=%x\n", ioack);
		tty_kref_put(tty);
		return;
	}

stl_rxalldone:
	tty_kref_put(tty);
	outb((EOSRR + portp->uartaddr), ioaddr);
	outb(0, (ioaddr + EREG_DATA));
}

/*****************************************************************************/

/*
 *	Modem interrupt handler. The is called when the modem signal line
 *	(DCD) has changed state. Leave most of the work to the off-level
 *	processing routine.
 */

static void stl_cd1400mdmisr(struct stlpanel *panelp, int ioaddr)
{
	struct stlport	*portp;
	unsigned int	ioack;
	unsigned char	misr;

	pr_debug("stl_cd1400mdmisr(panelp=%p)\n", panelp);

	ioack = inb(ioaddr + EREG_MDACK);
	if (((ioack & panelp->ackmask) != 0) ||
	    ((ioack & ACK_TYPMASK) != ACK_TYPMDM)) {
		printk("STALLION: bad MODEM interrupt ack value=%x\n", ioack);
		return;
	}
	portp = panelp->ports[(ioack >> 3)];

	outb((MISR + portp->uartaddr), ioaddr);
	misr = inb(ioaddr + EREG_DATA);
	if (misr & MISR_DCD) {
		stl_cd_change(portp);
		portp->stats.modem++;
	}

	outb((EOSRR + portp->uartaddr), ioaddr);
	outb(0, (ioaddr + EREG_DATA));
}

/*****************************************************************************/
/*                      SC26198 HARDWARE FUNCTIONS                           */
/*****************************************************************************/

/*
 *	These functions get/set/update the registers of the sc26198 UARTs.
 *	Access to the sc26198 registers is via an address/data io port pair.
 *	(Maybe should make this inline...)
 */

static int stl_sc26198getreg(struct stlport *portp, int regnr)
{
	outb((regnr | portp->uartaddr), (portp->ioaddr + XP_ADDR));
	return inb(portp->ioaddr + XP_DATA);
}

static void stl_sc26198setreg(struct stlport *portp, int regnr, int value)
{
	outb((regnr | portp->uartaddr), (portp->ioaddr + XP_ADDR));
	outb(value, (portp->ioaddr + XP_DATA));
}

static int stl_sc26198updatereg(struct stlport *portp, int regnr, int value)
{
	outb((regnr | portp->uartaddr), (portp->ioaddr + XP_ADDR));
	if (inb(portp->ioaddr + XP_DATA) != value) {
		outb(value, (portp->ioaddr + XP_DATA));
		return 1;
	}
	return 0;
}

/*****************************************************************************/

/*
 *	Functions to get and set the sc26198 global registers.
 */

static int stl_sc26198getglobreg(struct stlport *portp, int regnr)
{
	outb(regnr, (portp->ioaddr + XP_ADDR));
	return inb(portp->ioaddr + XP_DATA);
}

#if 0
static void stl_sc26198setglobreg(struct stlport *portp, int regnr, int value)
{
	outb(regnr, (portp->ioaddr + XP_ADDR));
	outb(value, (portp->ioaddr + XP_DATA));
}
#endif

/*****************************************************************************/

/*
 *	Inbitialize the UARTs in a panel. We don't care what sort of board
 *	these ports are on - since the port io registers are almost
 *	identical when dealing with ports.
 */

static int stl_sc26198panelinit(struct stlbrd *brdp, struct stlpanel *panelp)
{
	int	chipmask, i;
	int	nrchips, ioaddr;

	pr_debug("stl_sc26198panelinit(brdp=%p,panelp=%p)\n", brdp, panelp);

	BRDENABLE(panelp->brdnr, panelp->pagenr);

/*
 *	Check that each chip is present and started up OK.
 */
	chipmask = 0;
	nrchips = (panelp->nrports + 4) / SC26198_PORTS;
	if (brdp->brdtype == BRD_ECHPCI)
		outb(panelp->pagenr, brdp->ioctrl);

	for (i = 0; i < nrchips; i++) {
		ioaddr = panelp->iobase + (i * 4); 
		outb(SCCR, (ioaddr + XP_ADDR));
		outb(CR_RESETALL, (ioaddr + XP_DATA));
		outb(TSTR, (ioaddr + XP_ADDR));
		if (inb(ioaddr + XP_DATA) != 0) {
			printk("STALLION: sc26198 not responding, "
				"brd=%d panel=%d chip=%d\n",
				panelp->brdnr, panelp->panelnr, i);
			continue;
		}
		chipmask |= (0x1 << i);
		outb(GCCR, (ioaddr + XP_ADDR));
		outb(GCCR_IVRTYPCHANACK, (ioaddr + XP_DATA));
		outb(WDTRCR, (ioaddr + XP_ADDR));
		outb(0xff, (ioaddr + XP_DATA));
	}

	BRDDISABLE(panelp->brdnr);
	return chipmask;
}

/*****************************************************************************/

/*
 *	Initialize hardware specific port registers.
 */

static void stl_sc26198portinit(struct stlbrd *brdp, struct stlpanel *panelp, struct stlport *portp)
{
	pr_debug("stl_sc26198portinit(brdp=%p,panelp=%p,portp=%p)\n", brdp,
			panelp, portp);

	if ((brdp == NULL) || (panelp == NULL) ||
	    (portp == NULL))
		return;

	portp->ioaddr = panelp->iobase + ((portp->portnr < 8) ? 0 : 4);
	portp->uartaddr = (portp->portnr & 0x07) << 4;
	portp->pagenr = panelp->pagenr;
	portp->hwid = 0x1;

	BRDENABLE(portp->brdnr, portp->pagenr);
	stl_sc26198setreg(portp, IOPCR, IOPCR_SETSIGS);
	BRDDISABLE(portp->brdnr);
}

/*****************************************************************************/

/*
 *	Set up the sc26198 registers for a port based on the termios port
 *	settings.
 */

static void stl_sc26198setport(struct stlport *portp, struct ktermios *tiosp)
{
	struct stlbrd	*brdp;
	unsigned long	flags;
	unsigned int	baudrate;
	unsigned char	mr0, mr1, mr2, clk;
	unsigned char	imron, imroff, iopr, ipr;

	mr0 = 0;
	mr1 = 0;
	mr2 = 0;
	clk = 0;
	iopr = 0;
	imron = 0;
	imroff = 0;

	brdp = stl_brds[portp->brdnr];
	if (brdp == NULL)
		return;

/*
 *	Set up the RX char ignore mask with those RX error types we
 *	can ignore.
 */
	portp->rxignoremsk = 0;
	if (tiosp->c_iflag & IGNPAR)
		portp->rxignoremsk |= (SR_RXPARITY | SR_RXFRAMING |
			SR_RXOVERRUN);
	if (tiosp->c_iflag & IGNBRK)
		portp->rxignoremsk |= SR_RXBREAK;

	portp->rxmarkmsk = SR_RXOVERRUN;
	if (tiosp->c_iflag & (INPCK | PARMRK))
		portp->rxmarkmsk |= (SR_RXPARITY | SR_RXFRAMING);
	if (tiosp->c_iflag & BRKINT)
		portp->rxmarkmsk |= SR_RXBREAK;

/*
 *	Go through the char size, parity and stop bits and set all the
 *	option register appropriately.
 */
	switch (tiosp->c_cflag & CSIZE) {
	case CS5:
		mr1 |= MR1_CS5;
		break;
	case CS6:
		mr1 |= MR1_CS6;
		break;
	case CS7:
		mr1 |= MR1_CS7;
		break;
	default:
		mr1 |= MR1_CS8;
		break;
	}

	if (tiosp->c_cflag & CSTOPB)
		mr2 |= MR2_STOP2;
	else
		mr2 |= MR2_STOP1;

	if (tiosp->c_cflag & PARENB) {
		if (tiosp->c_cflag & PARODD)
			mr1 |= (MR1_PARENB | MR1_PARODD);
		else
			mr1 |= (MR1_PARENB | MR1_PAREVEN);
	} else
		mr1 |= MR1_PARNONE;

	mr1 |= MR1_ERRBLOCK;

/*
 *	Set the RX FIFO threshold at 8 chars. This gives a bit of breathing
 *	space for hardware flow control and the like. This should be set to
 *	VMIN.
 */
	mr2 |= MR2_RXFIFOHALF;

/*
 *	Calculate the baud rate timers. For now we will just assume that
 *	the input and output baud are the same. The sc26198 has a fixed
 *	baud rate table, so only discrete baud rates possible.
 */
	baudrate = tiosp->c_cflag & CBAUD;
	if (baudrate & CBAUDEX) {
		baudrate &= ~CBAUDEX;
		if ((baudrate < 1) || (baudrate > 4))
			tiosp->c_cflag &= ~CBAUDEX;
		else
			baudrate += 15;
	}
	baudrate = stl_baudrates[baudrate];
	if ((tiosp->c_cflag & CBAUD) == B38400) {
		if ((portp->port.flags & ASYNC_SPD_MASK) == ASYNC_SPD_HI)
			baudrate = 57600;
		else if ((portp->port.flags & ASYNC_SPD_MASK) == ASYNC_SPD_VHI)
			baudrate = 115200;
		else if ((portp->port.flags & ASYNC_SPD_MASK) == ASYNC_SPD_SHI)
			baudrate = 230400;
		else if ((portp->port.flags & ASYNC_SPD_MASK) == ASYNC_SPD_WARP)
			baudrate = 460800;
		else if ((portp->port.flags & ASYNC_SPD_MASK) == ASYNC_SPD_CUST)
			baudrate = (portp->baud_base / portp->custom_divisor);
	}
	if (baudrate > STL_SC26198MAXBAUD)
		baudrate = STL_SC26198MAXBAUD;

	if (baudrate > 0)
		for (clk = 0; clk < SC26198_NRBAUDS; clk++)
			if (baudrate <= sc26198_baudtable[clk])
				break;

/*
 *	Check what form of modem signaling is required and set it up.
 */
	if (tiosp->c_cflag & CLOCAL) {
		portp->port.flags &= ~ASYNC_CHECK_CD;
	} else {
		iopr |= IOPR_DCDCOS;
		imron |= IR_IOPORT;
		portp->port.flags |= ASYNC_CHECK_CD;
	}

/*
 *	Setup sc26198 enhanced modes if we can. In particular we want to
 *	handle as much of the flow control as possible automatically. As
 *	well as saving a few CPU cycles it will also greatly improve flow
 *	control reliability.
 */
	if (tiosp->c_iflag & IXON) {
		mr0 |= MR0_SWFTX | MR0_SWFT;
		imron |= IR_XONXOFF;
	} else
		imroff |= IR_XONXOFF;

	if (tiosp->c_iflag & IXOFF)
		mr0 |= MR0_SWFRX;

	if (tiosp->c_cflag & CRTSCTS) {
		mr2 |= MR2_AUTOCTS;
		mr1 |= MR1_AUTORTS;
	}

/*
 *	All sc26198 register values calculated so go through and set
 *	them all up.
 */

	pr_debug("SETPORT: portnr=%d panelnr=%d brdnr=%d\n",
		portp->portnr, portp->panelnr, portp->brdnr);
	pr_debug("    mr0=%x mr1=%x mr2=%x clk=%x\n", mr0, mr1, mr2, clk);
	pr_debug("    iopr=%x imron=%x imroff=%x\n", iopr, imron, imroff);
	pr_debug("    schr1=%x schr2=%x schr3=%x schr4=%x\n",
		tiosp->c_cc[VSTART], tiosp->c_cc[VSTOP],
		tiosp->c_cc[VSTART], tiosp->c_cc[VSTOP]);

	spin_lock_irqsave(&brd_lock, flags);
	BRDENABLE(portp->brdnr, portp->pagenr);
	stl_sc26198setreg(portp, IMR, 0);
	stl_sc26198updatereg(portp, MR0, mr0);
	stl_sc26198updatereg(portp, MR1, mr1);
	stl_sc26198setreg(portp, SCCR, CR_RXERRBLOCK);
	stl_sc26198updatereg(portp, MR2, mr2);
	stl_sc26198updatereg(portp, IOPIOR,
		((stl_sc26198getreg(portp, IOPIOR) & ~IPR_CHANGEMASK) | iopr));

	if (baudrate > 0) {
		stl_sc26198setreg(portp, TXCSR, clk);
		stl_sc26198setreg(portp, RXCSR, clk);
	}

	stl_sc26198setreg(portp, XONCR, tiosp->c_cc[VSTART]);
	stl_sc26198setreg(portp, XOFFCR, tiosp->c_cc[VSTOP]);

	ipr = stl_sc26198getreg(portp, IPR);
	if (ipr & IPR_DCD)
		portp->sigs &= ~TIOCM_CD;
	else
		portp->sigs |= TIOCM_CD;

	portp->imr = (portp->imr & ~imroff) | imron;
	stl_sc26198setreg(portp, IMR, portp->imr);
	BRDDISABLE(portp->brdnr);
	spin_unlock_irqrestore(&brd_lock, flags);
}

/*****************************************************************************/

/*
 *	Set the state of the DTR and RTS signals.
 */

static void stl_sc26198setsignals(struct stlport *portp, int dtr, int rts)
{
	unsigned char	iopioron, iopioroff;
	unsigned long	flags;

	pr_debug("stl_sc26198setsignals(portp=%p,dtr=%d,rts=%d)\n", portp,
			dtr, rts);

	iopioron = 0;
	iopioroff = 0;
	if (dtr == 0)
		iopioroff |= IPR_DTR;
	else if (dtr > 0)
		iopioron |= IPR_DTR;
	if (rts == 0)
		iopioroff********RTS;
	else ********>***********/
/**********/


	spin_lock_irqsave(&brd stal, flags);
	BRDENABLE(portp->brdnr, yright pagenrver.stl_sc26198setregp6-199, IOPIOR,
		((Technologieg
 *	Coopyrigt (C) 1) & ~********/) |*****/

/)ion 
 *DISer.
 *
 *	This coion T -- un serlionrestorltiyrig serial driver}

/* *ererThis program is free software; you can redistribute it and/or modify
 /s.
 ererReturn thet sete oferms oignalit u*	itof tic int 96  Greg Ungerneral P(struc publ6-19 *6-199)
{
	un	theed char	ipr;ither versilong	and o***/nt		siat y
	pr_debug("ublished byblicrms Fre6-199=%p)\n"996-199)on.c  *	Lus Torva muheodore T'so and otheublierer.
 *
 *	This coibuted 9  S that itipr =wareon.
 *
 *	Ter.
 *
 *	ThPRLinux T'so and othe, written*
 *	ThTY os Torvalds, Theodore T'soand/ othe
ptt it= 0 by
se f|= (plie&***/

CD) ? 0 : TIOCM_CDmGenedetai PubliblicYoCTShould have rece***/
 a copy of the GNU uTReral ublic Li***/

 a copy of the GNU Re	the F if not, write to the f not, S***/rrms tense ) rof the GNhis program is free software; you can redistribute it and/or modify
 *ic L underEnable/Dis/

#NU GeTransmTICUrand/or mReceothewarrc License void warranty ofen th#rxtxee Sftware; Foundanse , a prox
#incltx; ehe License,on 2 ccerms .h>
#in, or****(on) any la.h>
includef the .h>e <linudam is fd,rx=%d,t <listribute d ,ludetxty_flccied96-1999 cre <lin****/

tx********/e <l&= ~CR_TXerer.
*********/

 <linux/costat|= linux/sestatin***/
t.h>ux/comof tse <linR/ioport.h>ux/us Torrne <linux/sede <lin#include <the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the im96  Greg Unserer.
 *
 *	TSCCR, ccULAR r FITNESS FOR A PARTICULAR nux/ologiee <li =Y or R PURPOSE.  See the
 *	GNU General Public Li39, USA.
 */

/***************************************************************************Start/stop <linuxmodulek.h>
#include <lin****heder are abbreviated aslabssupp abbreviated ainterrupter are abbreviated attyk.h>
#include <lm
#defi_fliper are abbreviated as'so ae <linux/seasyConnectq_fiiver are abbreviated acd1400e <led aver im <linux/c26#defin	BRD_EASYIOPComonf it.hI <liRDY<linux/smp_lon.== 1ir opioalion.clong	mem>
#include <lin	memddr1(Ited RDY |dr;
RXBREAKigned inWATCHDOGbut ed asmp stale <linu	mem};

;
cLinu_nrbrds;t Technrbrds;s.
 ****n the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the ime <asm/uaccess.h>

#inclu MR, imx/pcie <l*******fndef	STL_SIOMEMMAJOR
#	ioa{
******	brdtypfndef	Sset_bit(ASYI_TXBUSY, &AJOR
#deicenhLAR PURPOSE.  See the
 *	GNU General Public Li39, USA.
 */

/***************************************************************************include all#inc/
#def freom thised asas
 *	EIO = EasyIO and ECH = Edncludeintree s<linux/interrupt.h>/tty.h>
#incluMC	22
#define	BRD_ECHPCI	26
#defineed inte	STL_Tq_file.hlinux/cd140iterms hope thatit awill be useful,****but WITHOUT ANY WARRANTY; without eveterms imIALMAJOR***** ved allocad140incler TY or Device0TY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public Li39, USA.
 */

/*********************************************************************IO = EasyIOPubliECH"5.6.endbreaktion 8/32.
 */
#define	BRD_Elen*****XBUFSIZE		4096
 */
#ifndef	STL_SIOMEMMAJOR
#ct ne	BdriPCI	27
#dlenD_EASYIOPCI	28

ine *****************/

/*
 *	Define our local driver identity first. Set up stuff to dpf (lennrbrdl {28
# <asm/uaccesit.h>>
#include <le <liSTART*****but dealt. Sicens.txeateds++;
	}*****mios		Techdefter INI = {
	.c_cl dr	= (B96OP| CS8 | stry.
 */
#ifndef	STL_SIOMEMMSTL_SERIALMAJOR		24
#endif
#ifndef	STL_CALLOUTMAJOR
#define	STL_CALLOUTMAJOR	25
#endif

/*
 *	Set the TX buffer size. Bigger is bettTake flow control acnse s..as
 *	EIO = EasyIO and ECH = Ell be rltion 8/32.
 */
#define	BRD_Ewith
 tty.XBUFLOWne	BXBUFLO	*ttyx/tty_flip.h>
#innclude <oh>
#include <l	mr0ine	BRD_ECHPCI	26
#definetl_****loPCI	27
#d
#def=%xASYIOPCI	28

with
 *	tanse 6-199nrbrNULLCambridge,;
	tty =XBRDS6-19_defiget(RIALMAJO6-19
of ticunde/
t the ly much herethe hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the iL_PROBEith
 c_ispPROBED	->.c_ispe->c_iand  to XOFFettingt_oped warranty of****MERCHANTABMRy a sINIT_C_CC,
	.c_ispeed	= 960MR0, (. Us& ~MR0_SWFRXTXes reINIT_C_CC,
	.c_ispeed	= 9600,
	.c_ospeeENDXONW	2
#d1
#d|=ocalifndef of bEAD | HUPCL |rxxon, 960INIT_C_CC,
	.waittublices for chewubli/with
 *	aifndef	STL_mrnen pr}*******	Quesfnde: seral Pwe uch he RTS to w*****/

as before? It mayflagsublicbeen set by an ioctl... Supposes Ave,sin, writou cL,
	
	real,rdwarfndef	STL_SIOMENULLthe forNew tretracsillyLL,
goe <lflags
	"6.0"NULL,lineLL,
hand/schedngPubliresetomiermsm ncedess.h>RTSGe
 *tominux/ng trace and stuff.
 */
1994-		9ublished by
 *ut WITHOUTTMR1baseMR1_AUTO8/32y of bnd stuff.
 */FLOWED	3s.
 *NTY; wi4-#ifndef	STL_SIOMEMMAJOR
#d**is pcode|loaNULL*
 *	Defils ofrings. Hanrtsy pri
 *}600,c		=atomi,
	"EC8/64-PCI",
	"EesyIO-PCto *	Da*****1
#d****e bit clear/s atomi routic charinting trace and stuff.
 */
sta	1efine some strinLO*	Define some string labels for****Dh
 *	
	NUarrFF cle boardit.hmes#incluint/

#asy board defdyffons, a	referenced i char	 whensp[] =-PCI"racrd3[rt suff traceTL_PROBon 2 *I",ULL,C8/64-PCine a s.0";
 common}********* This is used to
 *	parse any module arguments.
 */

static struch
 *	 on the char	*boardsom	(char * labeln prinarguments passed *	to chemodiul/32-	 The 	NUL& ~These e lowargs;easyd1,
	( ine sitd0,
	(chnd end typchar	*boardglobal pl	(chto put buffer over******haractt Seria3
};
!
 *_kref_putd rear	*boarddi2-isentO },
	{typesucres.
 */stand,
	{en the im"as******"ur lo},
	{numbt Se B,
	(chend maddrEMMAJOR
#def 2actstruo", E= "5.6.0";

static struct tR
#deck);
};

/*
e Sof*****brd		*Tech****[****MA#defi];

};

/*
 ons****-pc",ne	Byrig. Usrpt.h>s*****D_ECHPC******o", PerO },
	{of thBRD_ECHMCRD_E. Seerms of thefieldatic
 */
,
	{ "e-pc"ublicNo2


/*
 *	Define tECH ce and stu64-pFOUND	0xc char	*b **** and re0x2
*name;
	int	typ },
dore TPCI }ure i},
	{ " dri.*	GNse
	NULof
	{ "e e all*	odify<lin**** */
#def ime - so
	NUoarducres.
 */
static unsigned int stl_narg***************************tatic char	*board1[4];
stati char	*board2[4];
static char	*board3[4];
ataticmca", atl_brdsp[] = 
	(char **) &boarions, ar **) &board1,
	(char **) board2,
	(char **) &board3
};

/*
 *	ASYII },
	{opci", 64-pcifndOPCI },
	{ "28s, 0);
MODULE_PARM_D},
	{ "ec8/32-at" },
	{ "ei agreasyioch", B
	{ "ei_param_nargs(D_ECH0,CH },p, &*****ar
};

/* *	DefPCI },
	{p[] =  BRD_ar y ba&, charp BRD_>DULE_[,;
};

,ioaddr22][,irq]]");e_param_array(board2, cha1p, &_DESC(ec8/32-ats, 0);
MCHRM_DESC(, 0);
MisaULE_PARM_DESC(board3h 3 config -> name[,ioODULE_PARM_DESC(boar21ULE_PARM_DESC(board3, "BomcULE_PARM_DMC*****************d 3 config M BRD_ECHPDEFINE_MUTEX(stlushocal ;
st	51Define som a local default termios struct. All ports will b },
Erray*********************CI },
	{ "echpci", BRD_ECH64Pr are abbreviated adelaine	BRD_ECH		21
#e <li_ECHios = {
	.c_cflstl_iok.h>
#include stl_deftermios = {
	.c_cflag	= (BRESET
#define	EIO_8PORTDI	0x00
#define	E***************gistry.
 */
#ifndef	STL_SIOMEMMAJOR
#dtx.opy  {
	unsigntx.hea *
 *	ThSERIALMAJOR		24
#endif
#ifndef	STL_CALLOUTMAJOR
#define	STL_CALLOUTMAJOR	25
#endif

/*
 *	Set the TX buffer size. Bigger is bettr[,io.c_is curreECHPCI }e GNdath******o.c_i	"ECoo m T	0xeis onlO",
Ureae a on't wmes[ng w0";
d64-PCInCH_B****ECH_has fue a completeCne atis drislude orO",
realT Genreg Und#defina****chemecan not*****	Co	0x0/64-DISwe dfine	ECINactu0x08
drained, soLL,
, me to****checke	ECHnterrci",usy regiit.h>and irsure/sched.h>
#incinclwarranty ofne	Eci", asyIO and ECH boards. These defines apply
 *	to  },
	{ "ec8/64-sL,
	NULL,
	NULL,40
#define	NLSTATUS	****ci", BsBRD_ECHs (notr[,ioua****-*	This yIOPC inwe dt and esndif
#ifndef ThesSE0x1
#def with
 { "ec8/32-a1 CI	28

shar **********) trace and stuEIO_8PORTRSE	0x4 encode the i4terrupt fo5 so sete the bit clear/setting rouSITY or FITNESS FOR A PARTICULAR PURPOSE.  Sedr[,iI },GNU };

/e Fr/sched.h>
NULL,
	(sthe S <linMPTYeral Pub1ALLOUTprogrammabls. ThesC0xff, 0x00,	25
#er mapD_ECH64Se_ADDR2TX8/32-isasize. Biggeris fbetteh).
= {
	{ smine	amountfineUime, set ivEIO_8PECH_INTRa****nc	NULL	{ "roterm_21",mmset ****/

/*	Hardware ID bits for RD_ECXBUFLOW		512
#define	STL_Tur oiine	BRD_ECHPCI	26
#defined1,
	(char	0x80

#define	ECH_ADDR2MASK	0x1e0

/*
 *	Defiboar sta(i*	we di < 2io a++	miosblished by
 *, BRer.
 *
 *	TTST EIO39, USA.
 */

/***************************************************************************IfLL,
L,
	TXx40
#define	Elede <l*	thIXidenmodrt.h>nhen also	NUORTM		0 u>
#incule
 *	loe th We gotta doSKne	Ebecaushrd_lo G****autom
#inc	NULL,
	NULL,
t},
		if (seGreg Unas
 *	EIO = EasyIO and ECH = Etxech", tion 8/32.
 */
#define	B"ec8/32-pci", BRDense /tty.h>
#include <lc", BRDucres.
 */
static unsigned int stl_narSYIOPCI },
};

/*
 *	Define the module agruments.
 */
IT_C_CC,
	.c_ispeed	= 9600,
	.c_osHOSTargs, 0r **) &board1,
	(char **)board2,
	(char **) &board3
};

/*
 *	tic u mapping bitg laber the programmablneis pway"20"tic sdefine	an rshion_their seco-mc"y ioLE	0x
#incl	Ib((pagis fantic  comRD_ECH6TLEVELE	0x0red bint s0x00 staECH_INTRpane 0xff, 0x04,#incsyIO and ECH = E Theo- stal;	or a5 *r)]->brp,ine ial.h>incliobas},
	{ "ec8/32x/inter	upt.h>DMAagenr07
# otheackboard
 *	hardeodore T'spt t/* ****Work arerru bugpoinECH_INTRchiprealCfine	LL,

 A6 };

essl_brdrealof UART hrigharp, &v "ec
/*
, 0xNULL,
	pASYI0);
MPCIoutb(tati
#defi + 1", B
 ID_Es. Tnb832	alli00XP_IAC0,
}C,
	{ "e		****I },
	{s[(appin& IVR_ing e St) + (PCI_DEVI& 0x4) << 1)]rl);IOPCARM_DPCI864RXDATA},
	{  irqctivarxisr
 *
 *	TID_Ex40
#define	ECI
#define	TCI_DEVICE_ID_ELE_PA	e	STL033
};
#define,ioaddr2][,irqport boards.
 

/*
 *	L,
	{ "ec8/32- IDg trace if
/*
 *	De_C_CCunEL	0ed[SClogie_RXFIFOdefa", B/*
 *	stallion= BRD_ECH64PCI },
	{ PCI_DEVICE(is drivINDBASE		11 setlercode thNTRP((C) 1be fast!  HON, 0
#dTXx01

#arne	ECC0);
MAT"0x01
	{ uffPCI_manyI_VEposs#inclu= BRchso ne/32-is****int Sta   --stallHPCIwarr	In pDESCefinne a seTALLION,t_STALTLEVEL	0
 STALe <lin_STAboaraboard64-pnterrNTRP_STALhung up. Nec9  St(PCI Pe corLL,0
#dDOR_l StallCH.
 *
,ck; 	fine	ECdo_STAL	us0
#dff, 0xde effe/32-7410e	ID
		.pBRD4	definalsI_DE
bic Laline 		.dci,*****data = Bree 0);
MPC},
	{ _EIOPC)]->LL,
rlval boarXBUFLOW		512
#define	STL_T brd/port number f);		he BRD_ECHMCne	Svo_DEVhe Stallion ine sc", Boriveplen
	{ le64PC8/64-p*****, *****ine	BRD_ECHPCI	26
#define	STL_BA;
mo	0x80

#define	ECH_A
 *	aleithe*****2, 0xMINION, P0x10efine somID_BR***** baud rate cte tabx3f) = (******=
 *	loou*
 *aud- thereq: (STLbits  defa - (*****s pass", BRPROB
	{ "26"0) || 50, 75< *********LOW) &&
	   vectr moappoard1itLOWr the programmablosely b.c_ispel_pcibping bit	9600, 192 92138400,s, aI },
	{ "echpc", BRD_ECH64PCI },
	{ "27dificati	.drive_DESwakeup]");

/= BRD_E2][,irq]]");

/ARM_DESC(ec8/32-pc",0	.drivi, st(MR0 |the iIDBMASK are)CI832EIOPul StaADDR", BRDucres.ef6)
#-pc",inodeold a,
	NULL,
c chartl_bTX0002
#=ULE_PAf, llio,
*****(brdnr)]->i;

s****ong	memerCI },
IMR_PROB);
sTechmemLL,
	ee Sofsigned  *ip *	Dru int	srogrammabltty_struct *ttyint cm	*nam0
#d
#def*	Co),	\s for the programmablne	 &stl_nargs,);
MODUL_nrbrdloCI },
g,
	{ " your oCI },
	etportstatmrefin32td th__user *cp,
	{ ECH arp, &stl_ruct aminuct cludeg Un,	\
IFOuartPCI8Rle strings. H Ctotal +=lly.[,ioaddregee So_t(xc0) >> 6)
#/*
 .syio", aud rate cbuf + 0
};

00,n2


", BRD_tt *pi, stGrtp, comstats_t __use, struct soutsb mapions.
 */
stati,lace tD_ECH6ee So0get-e thetl_cd1*****+rd *brdtruc and ****>= },
	{ 
	{ "ec8/tstats(sCI	28
uJOR		,
	{ "OPCI },****dbu********* stl= BRc8/3(sue,
	{ "ec8/400portinit(st	(5 ltrucPCI }32-pc", Banel *ptlbrdte calculatns ar oterms}IO_INTRPEND	0x08
#define	EIO_INTEDGE	0x00
#define	EIO_INTLEVEL	0x08
#define	EIO_0WS		0x10

#dIO_8P**********ORStalSVICE(PCI6)
#_DEDGGE	0x0he LnULL,
Ngoolude <4d
#eor baidrt sp= BRD_***** Gu BRD_ppropriately. Gludert spint dtase corjustual  "ec8/3loNTABL994-.dR(p*
 *	De****LE_Dine	INTENA bytee	PCd alld****ccrwaRX*********dore *yrighstatiuirencode th2, 0x01,ORTMsort *pxtx(nt rxri
#defiRM_DESCint dtall (int VENDwe gic vnOR_ID_STALLon a*****rdiverSTALs clos
};

/*
van rhappenk(sthangups -the cortheEDGE	0x1
#*
 *	Cshutdow32-pc", DULE_inDefiL,
	NUextstlport *}ig -MOD
		.dcasMCp, srect pax0*****************EVICEion 8/32.
 */
#define	B global place t
/*
	{ "ec8/32-pci", BRD_ECHPCI },
	{ "26"ef P3f)

buf***** thevalulinux/serial.h>
#includoid	stds.
 0x04
	{ ,
	{ "ech-pci", i,
	{ ] =I },
	{ "echpc", BRD_ECH64PCI },
	{ "27"_EIOPuared.
  ric void	},
	stlIBrt.hy_struct *tty, struct  stlee s*fp,**************c0000_ID_EIOPBRD_EASYIOto YPE,
	{ "ec8rt *portp);*	modificatiECH64PCI, 134ic voice and ci, st_t tx0, 2roomd reportin, 57600 strucxisr(streg(st iobap, int rx, ULL,zeof*****N, PCI_ed	= 9t ioaddrR;

static in****reg 199****val	intlport *portp);
static &0portinit(st[0]portinit(st
	(char **) &bocp);400portinit3
};

/*
stl_cd1rt *portp);
stattic ininux/inlp, int ioaddr),
	{ ddr);

el *void	truct sc },
	{ "ec8/64 *ptaluekisr int ioaddr);

static in****;
};

);
******repareBRD_Ei", ingdmtati&ptrportinit(st*
 *	LockICE_ID2MASK specilt stlport *por-pci"
 *	uct sinnelp,);
staic int	stl_sc26198updatereg(, inaddr)d	stl_cd1400seo	STL int ioabadMODULd rate);
s39, U, BRD_);
salVICE_SET	0
	NULyio-p***** stle#incl	{ " stluct theinit(rds);	/| ECH_
	{ "echpc",
 *	Cop(C) 19 Stall)	port(\******s  a brd/poleint]t (C)_ECH****64-pci"tpor\
		D_ECHr thhe 1800, 2400, 4800,

	sr(strt a brd/port nu ioaddr)anBRD_E!2


/*
, 120 921680C8/64-PCI",hnologies
 ****als int ioaddr);
 io, memory anANYt *port0xc0) >> 6)
#d|tic _BCI	28

e thhULL,funcoaddr2][,irq]]");

/*****************************************************************************/

/*
 ******Pt *portpnxegnrr boa(same[,io	stl_sc26198disablei****2"ne	Pruct)rtinit(stfhe E int ioaddr);

statstruc	stl_c,;

/*
 *	nse as portinit(stine of th int ioaddr);

st stlport *portstl_cd140CI	28
echstlpanel *pac voidreg(struct s**********DULE_PARM_us5stl_fRXPARITYt the asy board defparityfor
 stl_cd140are. ThflFRAMINGt ioaddr);

static frstrugflowctrl(strare. Thine OVERRUNt ioaddr);

static ovEVELions, nt	stl_sc26198data 9600, ioaddr);

static LOCAL)for

	0, 50Technologies
 sigals(s, int);
rxigntatis#defd	stl_ct stlp,*	modifictl_sc2619marknt ioaddr)porte*/

Technologietxun****l_cd1ddr);

d	stl_s= TTY_port nse as **********0
#deand o & ASYNC_Sddr);

sta	dort, tlport *por WITHOUT ANY WARRANTY; without even the im, BRD_Ent ioaddrt	stl_sc26198datat *portp, t rx,,
	{ "ec8/t *pornt rx
#define	Itl_sc26198wait *portp, ic void	stl_sc26t *pIO_8har, int d	stl_sc2619ort *pnt ioaddc void	stl_sc26unsignertatic inereg(sa },
	
/min) easy
  ue. All**************_inset ia *paMODU int vch, BRD_u	but lport *portp, int,
	{ "ecg(strucvoid	stl******* int ioaddr);

staticfor
 d	stl,
	{ "ec8/char status, chassuppnclunt	stl_sc26198panelinit(struct stlrdp, struct stlpanel *panelp);dis/

#	STL, int l***********s8flordp, stregn_ECH)			\def. Cmin)		char (*nt rx6198sendflt stlwell,
	{ " *brdp, struct stlpanWportp, in- so sstlpofunc	(*ge)*******);
statir);
pe;
} moold ac chant rx,  while w stlportatic voTo198se	(*eexactr, int rts)errgs;
ypportmios *aticwitch funct),
CHARx)(strulpane( int rx,whyncluport *ptm****/*
 ncludmplbrdNUint */

 struct stlpanel *panelp);
t tty_dr(st
	{ XBUFLOW		512
#define	STL_TXBUFSIZE	de <lchar stamr1r	*board3ching b******pdatereg(precisex)(stru	void staeachr, int rts);o nt ioaddr);

stb400,stlbrd *brdp, structstats(str1es.
 */
static unsigned int st1atic int	stlct a brd/port numbe1atic 1ct stchaERRBLOCK3
};

isable witid	stl_ion_selec= {
	((exce#ifnio pYIO 8dataRDrd strucT_C_CC,
	.c_ispeed	= 9600,
	.c_osCLEARRXEh(st o	chint racrosuct m****c TorPCI"lport ]");
module_paraortp, ir-pci", BRD_d	stl_char status, lowportcorars(sR_ID_STALLcl},
	{ shleintrs) s  int rx, )}2MAS	(*pt	(*portp);
s*) portp->t *portp, int state);
stmrr)(s39, USA.
 */

/***************************************************************************Ored stlport *portp, int dtVICnux/s64-plpanme
 *	FoPo anowBRD_E**************, etc. Ml_scstaticis leftructoff-leve INTLEVEL	0rs(stt *poruct stm26198updatereg(_getsistruct stldred b_iduct stlport *portp, int len);
st>set
	{ "ec8/.h>
#include <lit iopr,  a mc26198sendfloi stlpanel *ableintrs*****, int state);
statieg(st****t *) cioftware selectablehar *deIOPCI	teCIart_
	right u(low	&>sen_SUB_sc26198se{
	 dtr,r ch)		COS:g(stloftware selectable (except thILart_tllion ith this p s areGE	.driver_dacdD_ECngNTRPENICE(PCI_ruct stlpane>	voiLE_PARM_D	eatedr	*b-pc",defiarXON0);
->flfine panelp->uartp)->panelinit)
XIe EIO-ologierer.&nitia_RXXONGOT	.driver, 230400, 4608ruct stlport *portp);
sio((strucc8/3port t -> name[,i*board3BED	0xtl_sc261artFF******CI	2tic int	stl_clrpo_cd1400panelinit,
	stl_cdtic vortc8/3,
26198s
	{ ,har *BRDMASK	0xf0
#define	I(* (

/*nd clean.
 */
#define	stl_panelini1400sORTMoid	6	(* ((uart_t *) po struct stlpan*********default

/*********spee****) portp->uart>uar_x/seqds(clud_DEVICE(PClinitportpane thexc0) >> 6)
#dr*por
#define	STL};

_ID_nriD	460spacct stlpaneCI	2 panelp are[i] *poruct sefinerdtrings.p, saud  and r ioaddr_sc26ueef stffaticrqe	EREGlion, 5
#ddne	ER	.drileanup_ne	PC 	5
#dTXACK	release_GE	0on	*	is sonse a15
#def(minI832->getologi	28
_CLK	2502f, 0ce tde th#defBANKdefau8
efine so2CDine	CD1400_CLTXACK	k>uarCK	7the K	6
#strucwel *portp)_ispee*ti	(*pLoatastatmotruc c8/3ializ1400 )-pc",/schedSET	0x02

#__rgs;(* (	unsi_toDefinly. BasicaE	0x0.h>
bankND	0	board
  XBUFLOW		5confA	4
* ((_startrxtx		(* (jinde i(minva= {
	(c261k(KERN_INFO "%s:ial.Tt	st%stribu_ID_drvtitp);
st, CD1CLK2, C Basineli

#deftic  &},
	aximuPCI_VEN*****************ICE(PCI_VENnty_dr,'so s are loc", BRdr_8PObaud /32-pci *min)	MAXPD_ECHl_cd140!har	*name; kteo baud r = -ENOMEMd whgototl_ctl_cd1
static i
 ->owne	CD1THIS__t _rt.h>tl_sc2619porTbreg(_na(stru4ig -> tic set stlbrdTA	4
 int dt"ttyE"26198setsignalsmajot,
	aud r the progrs[STble)
#dtsigninor_	BRD_ry io  thelo
	stl_s	voidardwareRIVEportp,	void	(26198setsignalssubtsignalvoid	(wer f,NORMuct stlport **********addr);

dtr,,
	ef4-PCI",26198setsignals	(* ((sc26198dataowREAL_RAW |in the regisDYNAMIC porARM_DES00uaPCI  tx); lenort *porcifrtp-optl_b4stl_ted	= struct stlpoRc26198irt void	_sc	irq****rt *Xc26198};

/40"t *portp: fai Statouct stlpodlinit(sc26198\n"ftwah800porti_fr= {
	0cstalhar Fi[,iony dynamic0x01
sNABLEtStalchar	oid	(*enabvia6198u****oalportr mapptl_cd14port);
#define	EG_RXA *	DecoEND	G_RXargdtabmmc26198mems_ECHefin, 0inI832*	th32),t *) t ioaul_arrsebrd 921450,enanelcupgnalublicigna0	r the boaelp, .6.0";

D_ECHICE_I,
	4 ioadd

/*
 *	, 288
};

/**0

/*
is copane02, 0x0ICE_IDtsignal 120.defa(scDS		ARRAY_ort *poCE_ID_baort *po)		.driver_data 2**************2***********rq***************/

/*struc(are. Th_bahend ot tab00, 9600lARRAY_SIZ_Noard base b 9212****beintrs	e use	.driver_dal*********ablein1
#d8MAXwha,
	{ "2r	Defi tabr, int value
#defjne	STLj <baud ranr);
st; jp },
	{", BR	Define soed ra		d rate cal *porttrol deviceRtizpt.h>ic voi + j,;

#d9	(char **8_baudpane#definARM_DESC(/* ioadd_MODUort *_a6198_ isahe adportport(struct serport2
#dort *XP_MpciD	Define somXP_STP_ADDpcic26198ruct *ttIACK		 &1400s****t *pole.hFOR A P baud rateERR 
 *	Define4can't,
	ne the pci8elp, unsign= {
	. O] = uns_ECH6*******tabk orup therdnt ioadc26198 portper unsig
 *
audode thCH_maineioin ioaddr);do(* ((s******ic iefine	EMC********(&port(min)		chrdevbaud SI#defdisab, "},
	iomem"	XP_ADDfsASYI&rtp->ruct *tt= stl_gets-> name[,ioaddr[,ioare. Thuptibl;
st,
tp->si#def********((uar=*sed i_creLINTTechnologieeak,._EASYI&he act&porS>sigased o", BRD_Ei****!
 *_l_cd14(ttyver.****,
	{ "	{ "eion thetp->sigese io address o4 commoe tk for oaddr2][************** rtp,l, MKDEV)*****f pyright poi)ts(st9216 dPCI f line.
 */
%d"nt _
#defin06, 0;
 recei0, 1:rdp, suntty_struct *tty NLINTDefine s26198_bwi:
	prtp)-FO#define;
any later vest:onfp, chabaud rat> name[,ioaddr[__exdr[,ioa*****melp, u*arg allowed, and the defauat base bstrucbreak(.
 );
stlinux/serialcode thR26198 (stri *ch>
# baud rate1

	foUn	0x0portne	CD14****baud rate3dstr); i++)er leve		.driver_datasigs &lree recees[] = ca&porresourcesait);STAL0, 1 (pyri***********tp)	(*pmemBLE	0xdr);

s_NS, . AsHPCIst	if ( stlp *portwe register* d stlport.CI	2eceievery ocharp->ua-* ((tra set0, 57 ouCI	2ygp[0]);
tk("STt (Cportodefi_brdst****si0

#dress oesle[] = {_baudtab5, 150, 0, 57600, 115200alculat400, s encode th#def *ip.ioctl		= s BRD_I_DE 28800, 38t ioaI	27HPCI },
	{  loadfsags &ed	= 96owr	*sp;l baud rad for 	.LL,
	****ts(strLL,
	fig ->{ "ec8/32-pc", ((uachar stD_ECH64P3600erms oup.
 *d	{ "easyidestroygdefin, NULL char	PCI },
	{ */
staticefin_unsitrtoul(argp)******.
 */

static intline.
 */

 stlpo0, 5oul(argp[i], E_DEV 0);*s0, 1cistrtoul(argp[efine	=", BRD_ECH****getl_sc2"stl_pars ((ot *) porboard structure. Fi=%p)\n", confrd(rsebp=%p,**/
s distrnt vnf}

)
		retnsign[0] == 0))
		retnsig); kze loce te fun**********rthe teate. logieer bHOR("************0;
	KSIZE	4DESCRIPTION(" Camb;
	}Mwill, GFPSD_ECH6Dbreg(LLIOy (lineLICENSE("GPlly m