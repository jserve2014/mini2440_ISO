/*
 * hfcmulti.c  low level driver for hfc-4s/hfc-8s/hfc-e1 based cards
 *
 * Author	Andreas Eversberg (jolly@eversberg.eu)
 * ported to mqueue mechanism:
 *		Peter Sprenger (sprengermoving-bytes.de)
 *
 * inspired by existing hfc-pci driver:
 * Copyright 1999  by Werner Cornelius (werner@isdn-development.de)
 * Copyright 2008  by Karsten Keil (kkeil@suse.de)
 * Copyright 2008  by Andreas Eversberg (jolly@eversberg.eu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 * Thanks to Cologne Chip AG for this great controller!
 */

/*
 * module parameters:
 * type:
 *	By default (0), the card is automatically detected.
 *	Or use the following combinations:
 *	Bit 0-7   = 0x00001 = HFC-E1 (1 port)
 * or	Bit 0-7   = 0x00004 = HFC-4S (4 ports)
 * or	Bit 0-7   = 0x00008 = HFC-8S (8 ports)
 *	Bit 8     = 0x00100 = uLaw (instead of aLaw)
 *	Bit 9     = 0x00200 = Disable DTMF detect on all B-channels via hardware
 *	Bit 10    = spare
 *	Bit 11    = 0x00800 = Force PCM bus into slave mode. (otherwhise auto)
 * or   Bit 12    = 0x01000 = Force PCM bus into master mode. (otherwhise auto)
 *	Bit 13	  = spare
 *	Bit 14    = 0x04000 = Use external ram (128K)
 *	Bit 15    = 0x08000 = Use external ram (512K)
 *	Bit 16    = 0x10000 = Use 64 timeslots instead of 32
 * or	Bit 17    = 0x20000 = Use 128 timeslots instead of anything else
 *	Bit 18    = spare
 *	Bit 19    = 0x80000 = Send the Watchdog a Signal (Dual E1 with Watchdog)
 * (all other bits are reserved and shall be 0)
 *	example: 0x20204 one HFC-4S with dtmf detection and 128 timeslots on PCM
 *		 bus (PCM master)
 *
 * port: (optional or required for all ports on all installed cards)
 *	HFC-4S/HFC-8S only bits:
 *	Bit 0	  = 0x001 = Use master clock for this S/T interface
 *			    (ony once per chip).
 *	Bit 1     = 0x002 = transmitter line setup (non capacitive mode)
 *			    Don't use this unless you know what you are doing!
 *	Bit 2     = 0x004 = Disable E-channel. (No E-channel processing)
 *	example: 0x0001,0x0000,0x0000,0x0000 one HFC-4S with master clock
 *		 received from port 1
 *
 *	HFC-E1 only bits:
 *	Bit 0     = 0x0001 = interface: 0=copper, 1=optical
 *	Bit 1     = 0x0002 = reserved (later for 32 B-channels transparent mode)
 *	Bit 2     = 0x0004 = Report LOS
 *	Bit 3     = 0x0008 = Report AIS
 *	Bit 4     = 0x0010 = Report SLIP
 *	Bit 5     = 0x0020 = Report RDI
 *	Bit 8     = 0x0100 = Turn off CRC-4 Multiframe Mode, use double frame
 *			     mode instead.
 *	Bit 9	  = 0x0200 = Force get clock from interface, even in NT mode.
 * or	Bit 10	  = 0x0400 = Force put clock to interface, even in TE mode.
 *	Bit 11    = 0x0800 = Use direct RX clock for PCM sync rather than PLL.
 *			     (E1 only)
 *	Bit 12-13 = 0xX000 = elastic jitter buffer (1-3), Set both bits to 0
 *			     for default.
 * (all other bits are reserved and shall be 0)
 *
 * debug:
 *	NOTE: only one debug value must be given for all cards
 *	enable debugging (see hfc_multi.h for debug options)
 *
 * poll:
 *	NOTE: only one poll value must be given for all cards
 *	Give the number of samples for each fifo process.
 *	By default 128 is used. Decrease to reduce delay, increase to
 *	reduce cpu load. If unsure, don't mess with it!
 *	Valid is 8, 16, 32, 64, 128, 256.
 *
 * pcm:
 *	NOTE: only one pcm value must be given for every card.
 *	The PCM bus id tells the mISDNdsp module about the connected PCM bus.
 *	By default (0), the PCM bus id is 100 for the card that is PCM master.
 *	If multiple cards are PCM master (because they are not interconnected),
 *	each card with PCM master will have increasing PCM id.
 *	All PCM busses with the same ID are expected to be connected and have
 *	common time slots slots.
 *	Only one chip of the PCM bus must be master, the others slave.
 *	-1 means no support of PCM bus not even.
 *	Omit this value, if all cards are interconnected or none is connected.
 *	If unsure, don't give this parameter.
 *
 * dslot:
 *	NOTE: only one dslot value must be given for every card.
 *	Also this value must be given for non-E1 cards. If omitted, the E1
 *	card has D-channel on time slot 16, which is default.
 *	If 1..15 or 17..31, an alternate time slot is used for D-channel.
 *	In this case, the application must be able to handle this.
 *	If -1 is given, the D-channel is disabled and all 31 slots can be used
 *	for B-channel. (only for specific applications)
 *	If you don't know how to use it, you don't need it!
 *
 * iomode:
 *	NOTE: only one mode value must be given for every card.
 *	-> See hfc_multi.h for HFC_IO_MODE_* values
 *	By default, the IO mode is pci memory IO (MEMIO).
 *	Some cards requre specific IO mode, so it cannot be changed.
 *	It may be usefull to set IO mode to register io (REGIO) to solve
 *	PCI bridge problems.
 *	If unsure, don't give this parameter.
 *
 * clockdelay_nt:
 *	NOTE: only one clockdelay_nt value must be given once for all cards.
 *	Give the value of the clock control register (A_ST_CLK_DLY)
 *	of the S/T interfaces in NT mode.
 *	This register is needed for the TBR3 certification, so don't change it.
 *
 * clockdelay_te:
 *	NOTE: only one clockdelay_te value must be given once
 *	Give the value of the clock control register (A_ST_CLK_DLY)
 *	of the S/T interfaces in TE mode.
 *	This register is needed for the TBR3 certification, so don't change it.
 *
 * clock:
 *	NOTE: only one clock value must be given once
 *	Selects interface with clock source for mISDN and applications.
 *	Set to card number starting with 1. Set to -1 to disable.
 *	By default, the first card is used as clock source.
 *
 * hwid:
 *	NOTE: only one hwid value must be given once
 * 	Enable special embedded devices with XHFC controllers.
 */

/*
 * debug register access (never use this, it will flood your system log)
 * #define HFC_REGISTER_DEBUG
 */

#define HFC_MULTI_VERSION	"2.03"

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/mISDNhw.h>
#include <linux/mISDNdsp.h>

/*
#define IRQCOUNT_DEBUG
#define IRQ_DEBUG
*/

#include "hfc_multi.h"
#ifdef ECHOPREP
#include "gaintab.h"
#endif

#define	MAX_CARDS	8
#define	MAX_PORTS	(8 * MAX_CARDS)

static LIST_HEAD(HFClist);
static spinlock_t HFClock; /* global hfc list lock */

static void ph_state_change(struct dchannel *);

static struct hfc_multi *syncmaster;
static int plxsd_master; /* if we have a master card (yet) */
static spinlock_t plx_lock; /* may not acquire other lock inside */

#define	TYP_E1		1
#define	TYP_4S		4
#define TYP_8S		8

static int poll_timer = 6;	/* default = 128 samples = 16ms */
/* number of POLL_TIMER interrupts for G2 timeout (ca 1s) */
static int nt_t1_count[] = { 3840, 1920, 960, 480, 240, 120, 60, 30  };
#define	CLKDEL_TE	0x0f	/* CLKDEL in TE mode */
#define	CLKDEL_NT	0x6c	/* CLKDEL in NT mode
				   (0x60 MUST be included!) */

#define	DIP_4S	0x1		/* DIP Switches for Beronet 1S/2S/4S cards */
#define	DIP_8S	0x2		/* DIP Switches for Beronet 8S+ cards */
#define	DIP_E1	0x3		/* DIP Switches for Beronet E1 cards */

/*
 * module stuff
 */

static uint	type[MAX_CARDS];
static int	pcm[MAX_CARDS];
static int	dslot[MAX_CARDS];
static uint	iomode[MAX_CARDS];
static uint	port[MAX_PORTS];
static uint	debug;
static uint	poll;
static int	clock;
static uint	timer;
static uint	clockdelay_te = CLKDEL_TE;
static uint	clockdelay_nt = CLKDEL_NT;
#define HWID_NONE	0
#define HWID_MINIP4	1
#define HWID_MINIP8	2
#define HWID_MINIP16	3
static uint	hwid = HWID_NONE;

static int	HFC_cnt, Port_cnt, PCM_cnt = 99;

MODULE_AUTHOR("Andreas Eversberg");
MODULE_LICENSE("GPL");
MODULE_VERSION(HFC_MULTI_VERSION);
module_param(debug, uint, S_IRUGO | S_IWUSR);
module_param(poll, uint, S_IRUGO | S_IWUSR);
module_param(clock, int, S_IRUGO | S_IWUSR);
module_param(timer, uint, S_IRUGO | S_IWUSR);
module_param(clockdelay_te, uint, S_IRUGO | S_IWUSR);
module_param(clockdelay_nt, uint, S_IRUGO | S_IWUSR);
module_param_array(type, uint, NULL, S_IRUGO | S_IWUSR);
module_param_array(pcm, int, NULL, S_IRUGO | S_IWUSR);
module_param_array(dslot, int, NULL, S_IRUGO | S_IWUSR);
module_param_array(iomode, uint, NULL, S_IRUGO | S_IWUSR);
module_param_array(port, uint, NULL, S_IRUGO | S_IWUSR);
module_param(hwid, uint, S_IRUGO | S_IWUSR); /* The hardware ID */

#ifdef HFC_REGISTER_DEBUG
#define HFC_outb(hc, reg, val) \
	(hc->HFC_outb(hc, reg, val, __func__, __LINE__))
#define HFC_outb_nodebug(hc, reg, val) \
	(hc->HFC_outb_nodebug(hc, reg, val, __func__, __LINE__))
#define HFC_inb(hc, reg) \
	(hc->HFC_inb(hc, reg, __func__, __LINE__))
#define HFC_inb_nodebug(hc, reg) \
	(hc->HFC_inb_nodebug(hc, reg, __func__, __LINE__))
#define HFC_inw(hc, reg) \
	(hc->HFC_inw(hc, reg, __func__, __LINE__))
#define HFC_inw_nodebug(hc, reg) \
	(hc->HFC_inw_nodebug(hc, reg, __func__, __LINE__))
#define HFC_wait(hc) \
	(hc->HFC_wait(hc, __func__, __LINE__))
#define HFC_wait_nodebug(hc) \
	(hc->HFC_wait_nodebug(hc, __func__, __LINE__))
#else
#define HFC_outb(hc, reg, val)		(hc->HFC_outb(hc, reg, val))
#define HFC_outb_nodebug(hc, reg, val)	(hc->HFC_outb_nodebug(hc, reg, val))
#define HFC_inb(hc, reg)		(hc->HFC_inb(hc, reg))
#define HFC_inb_nodebug(hc, reg)	(hc->HFC_inb_nodebug(hc, reg))
#define HFC_inw(hc, reg)		(hc->HFC_inw(hc, reg))
#define HFC_inw_nodebug(hc, reg)	(hc->HFC_inw_nodebug(hc, reg))
#define HFC_wait(hc)			(hc->HFC_wait(hc))
#define HFC_wait_nodebug(hc)		(hc->HFC_wait_nodebug(hc))
#endif

#ifdef CONFIG_MISDN_HFCMULTI_8xx
#include "hfc_multi_8xx.h"
#endif

/* HFC_IO_MODE_PCIMEM */
static void
#ifdef HFC_REGISTER_DEBUG
HFC_outb_pcimem(struct hfc_multi *hc, u_char reg, u_char val,
		const char *function, int line)
#else
HFC_outb_pcimem(struct hfc_multi *hc, u_char reg, u_char val)
#endif
{
	writeb(val, hc->pci_membase + reg);
}
static u_char
#ifdef HFC_REGISTER_DEBUG
HFC_inb_pcimem(struct hfc_multi *hc, u_char reg, const char *function, int line)
#else
HFC_inb_pcimem(struct hfc_multi *hc, u_char reg)
#endif
{
	return readb(hc->pci_membase + reg);
}
static u_short
#ifdef HFC_REGISTER_DEBUG
HFC_inw_pcimem(struct hfc_multi *hc, u_char reg, const char *function, int line)
#else
HFC_inw_pcimem(struct hfc_multi *hc, u_char reg)
#endif
{
	return readw(hc->pci_membase + reg);
}
static void
#ifdef HFC_REGISTER_DEBUG
HFC_wait_pcimem(struct hfc_multi *hc, const char *function, int line)
#else
HFC_wait_pcimem(struct hfc_multi *hc)
#endif
{
	while (readb(hc->pci_membase + R_STATUS) & V_BUSY)
		cpu_relax();
}

/* HFC_IO_MODE_REGIO */
static void
#ifdef HFC_REGISTER_DEBUG
HFC_outb_regio(struct hfc_multi *hc, u_char reg, u_char val,
	const char *function, int line)
#else
HFC_outb_regio(struct hfc_multi *hc, u_char reg, u_char val)
#endif
{
	outb(reg, hc->pci_iobase + 4);
	outb(val, hc->pci_iobase);
}
static u_char
#ifdef HFC_REGISTER_DEBUG
HFC_inb_regio(struct hfc_multi *hc, u_char reg, const char *function, int line)
#else
HFC_inb_regio(struct hfc_multi *hc, u_char reg)
#endif
{
	outb(reg, hc->pci_iobase + 4);
	return inb(hc->pci_iobase);
}
static u_short
#ifdef HFC_REGISTER_DEBUG
HFC_inw_regio(struct hfc_multi *hc, u_char reg, const char *function, int line)
#else
HFC_inw_regio(struct hfc_multi *hc, u_char reg)
#endif
{
	outb(reg, hc->pci_iobase + 4);
	return inw(hc->pci_iobase);
}
static void
#ifdef HFC_REGISTER_DEBUG
HFC_wait_regio(struct hfc_multi *hc, const char *function, int line)
#else
HFC_wait_regio(struct hfc_multi *hc)
#endif
{
	outb(R_STATUS, hc->pci_iobase + 4);
	while (inb(hc->pci_iobase) & V_BUSY)
		cpu_relax();
}

#ifdef HFC_REGISTER_DEBUG
static void
HFC_outb_debug(struct hfc_multi *hc, u_char reg, u_char val,
		const char *function, int line)
{
	char regname[256] = "", bits[9] = "xxxxxxxx";
	int i;

	i = -1;
	while (hfc_register_names[++i].name) {
		if (hfc_register_names[i].reg == reg)
			strcat(regname, hfc_register_names[i].name);
	}
	if (regname[0] == '\0')
		strcpy(regname, "register");

	bits[7] = '0' + (!!(val & 1));
	bits[6] = '0' + (!!(val & 2));
	bits[5] = '0' + (!!(val & 4));
	bits[4] = '0' + (!!(val & 8));
	bits[3] = '0' + (!!(val & 16));
	bits[2] = '0' + (!!(val & 32));
	bits[1] = '0' + (!!(val & 64));
	bits[0] = '0' + (!!(val & 128));
	printk(KERN_DEBUG
	    "HFC_outb(chip %d, %02x=%s, 0x%02x=%s); in %s() line %d\n",
	    hc->id, reg, regname, val, bits, function, line);
	HFC_outb_nodebug(hc, reg, val);
}
static u_char
HFC_inb_debug(struct hfc_multi *hc, u_char reg, const char *function, int line)
{
	char regname[256] = "", bits[9] = "xxxxxxxx";
	u_char val = HFC_inb_nodebug(hc, reg);
	int i;

	i = 0;
	while (hfc_register_names[i++].name)
		;
	while (hfc_register_names[++i].name) {
		if (hfc_register_names[i].reg == reg)
			strcat(regname, hfc_register_names[i].name);
	}
	if (regname[0] == '\0')
		strcpy(regname, "register");

	bits[7] = '0' + (!!(val & 1));
	bits[6] = '0' + (!!(val & 2));
	bits[5] = '0' + (!!(val & 4));
	bits[4] = '0' + (!!(val & 8));
	bits[3] = '0' + (!!(val & 16));
	bits[2] = '0' + (!!(val & 32));
	bits[1] = '0' + (!!(val & 64));
	bits[0] = '0' + (!!(val & 128));
	printk(KERN_DEBUG
	    "HFC_inb(chip %d, %02x=%s) = 0x%02x=%s; in %s() line %d\n",
	    hc->id, reg, regname, val, bits, function, line);
	return val;
}
static u_short
HFC_inw_debug(struct hfc_multi *hc, u_char reg, const char *function, int line)
{
	char regname[256] = "";
	u_short val = HFC_inw_nodebug(hc, reg);
	int i;

	i = 0;
	while (hfc_register_names[i++].name)
		;
	while (hfc_register_names[++i].name) {
		if (hfc_register_names[i].reg == reg)
			strcat(regname, hfc_register_names[i].name);
	}
	if (regname[0] == '\0')
		strcpy(regname, "register");

	printk(KERN_DEBUG
	    "HFC_inw(chip %d, %02x=%s) = 0x%04x; in %s() line %d\n",
	    hc->id, reg, regname, val, function, line);
	return val;
}
static void
HFC_wait_debug(struct hfc_multi *hc, const char *function, int line)
{
	printk(KERN_DEBUG "HFC_wait(chip %d); in %s() line %d\n",
	    hc->id, function, line);
	HFC_wait_nodebug(hc);
}
#endif

/* write fifo data (REGIO) */
static void
write_fifo_regio(struct hfc_multi *hc, u_char *data, int len)
{
	outb(A_FIFO_DATA0, (hc->pci_iobase)+4);
	while (len>>2) {
		outl(cpu_to_le32(*(u32 *)data), hc->pci_iobase);
		data += 4;
		len -= 4;
	}
	while (len>>1) {
		outw(cpu_to_le16(*(u16 *)data), hc->pci_iobase);
		data += 2;
		len -= 2;
	}
	while (len) {
		outb(*data, hc->pci_iobase);
		data++;
		len--;
	}
}
/* write fifo data (PCIMEM) */
static void
write_fifo_pcimem(struct hfc_multi *hc, u_char *data, int len)
{
	while (len>>2) {
		writel(cpu_to_le32(*(u32 *)data),
			hc->pci_membase + A_FIFO_DATA0);
		data += 4;
		len -= 4;
	}
	while (len>>1) {
		writew(cpu_to_le16(*(u16 *)data),
			hc->pci_membase + A_FIFO_DATA0);
		data += 2;
		len -= 2;
	}
	while (len) {
		writeb(*data, hc->pci_membase + A_FIFO_DATA0);
		data++;
		len--;
	}
}

/* read fifo data (REGIO) */
static void
read_fifo_regio(struct hfc_multi *hc, u_char *data, int len)
{
	outb(A_FIFO_DATA0, (hc->pci_iobase)+4);
	while (len>>2) {
		*(u32 *)data = le32_to_cpu(inl(hc->pci_iobase));
		data += 4;
		len -= 4;
	}
	while (len>>1) {
		*(u16 *)data = le16_to_cpu(inw(hc->pci_iobase));
		data += 2;
		len -= 2;
	}
	while (len) {
		*data = inb(hc->pci_iobase);
		data++;
		len--;
	}
}

/* read fifo data (PCIMEM) */
static void
read_fifo_pcimem(struct hfc_multi *hc, u_char *data, int len)
{
	while (len>>2) {
		*(u32 *)data =
			le32_to_cpu(readl(hc->pci_membase + A_FIFO_DATA0));
		data += 4;
		len -= 4;
	}
	while (len>>1) {
		*(u16 *)data =
			le16_to_cpu(readw(hc->pci_membase + A_FIFO_DATA0));
		data += 2;
		len -= 2;
	}
	while (len) {
		*data = readb(hc->pci_membase + A_FIFO_DATA0);
		data++;
		len--;
	}
}

static void
enable_hwirq(struct hfc_multi *hc)
{
	hc->hw.r_irq_ctrl |= V_GLOB_IRQ_EN;
	HFC_outb(hc, R_IRQ_CTRL, hc->hw.r_irq_ctrl);
}

static void
disable_hwirq(struct hfc_multi *hc)
{
	hc->hw.r_irq_ctrl &= ~((u_char)V_GLOB_IRQ_EN);
	HFC_outb(hc, R_IRQ_CTRL, hc->hw.r_irq_ctrl);
}

#define	NUM_EC 2
#define	MAX_TDM_CHAN 32


inline void
enablepcibridge(struct hfc_multi *c)
{
	HFC_outb(c, R_BRG_PCM_CFG, (0x0 << 6) | 0x3); /* was _io before */
}

inline void
disablepcibridge(struct hfc_multi *c)
{
	HFC_outb(c, R_BRG_PCM_CFG, (0x0 << 6) | 0x2); /* was _io before */
}

inline unsigned char
readpcibridge(struct hfc_multi *hc, unsigned char address)
{
	unsigned short cipv;
	unsigned char data;

	if (!hc->pci_iobase)
		return 0;

	/* slow down a PCI read access by 1 PCI clock cycle */
	HFC_outb(hc, R_CTRL, 0x4); /*was _io before*/

	if (address == 0)
		cipv = 0x4000;
	else
		cipv = 0x5800;

	/* select local bridge port address by writing to CIP port */
	/* data = HFC_inb(c, cipv); * was _io before */
	outw(cipv, hc->pci_iobase + 4);
	data = inb(hc->pci_iobase);

	/* restore R_CTRL for normal PCI read cycle speed */
	HFC_outb(hc, R_CTRL, 0x0); /* was _io before */

	return data;
}

inline void
writepcibridge(struct hfc_multi *hc, unsigned char address, unsigned char data)
{
	unsigned short cipv;
	unsigned int datav;

	if (!hc->pci_iobase)
		return;

	if (address == 0)
		cipv = 0x4000;
	else
		cipv = 0x5800;

	/* select local bridge port address by writing to CIP port */
	outw(cipv, hc->pci_iobase + 4);
	/* define a 32 bit dword with 4 identical bytes for write sequence */
	datav = data | ((__u32) data << 8) | ((__u32) data << 16) |
	    ((__u32) data << 24);

	/*
	 * write this 32 bit dword to the bridge data port
	 * this will initiate a write sequence of up to 4 writes to the same
	 * address on the local bus interface the number of write accesses
	 * is undefined but >=1 and depends on the next PCI transaction
	 * during write sequence on the local bus
	 */
	outl(datav, hc->pci_iobase);
}

inline void
cpld_set_reg(struct hfc_multi *hc, unsigned char reg)
{
	/* Do data pin read low byte */
	HFC_outb(hc, R_GPIO_OUT1, reg);
}

inline void
cpld_write_reg(struct hfc_multi *hc, unsigned char reg, unsigned char val)
{
	cpld_set_reg(hc, reg);

	enablepcibridge(hc);
	writepcibridge(hc, 1, val);
	disablepcibridge(hc);

	return;
}

inline unsigned char
cpld_read_reg(struct hfc_multi *hc, unsigned char reg)
{
	unsigned char bytein;

	cpld_set_reg(hc, reg);

	/* Do data pin read low byte */
	HFC_outb(hc, R_GPIO_OUT1, reg);

	enablepcibridge(hc);
	bytein = readpcibridge(hc, 1);
	disablepcibridge(hc);

	return bytein;
}

inline void
vpm_write_address(struct hfc_multi *hc, unsigned short addr)
{
	cpld_write_reg(hc, 0, 0xff & addr);
	cpld_write_reg(hc, 1, 0x01 & (addr >> 8));
}

inline unsigned short
vpm_read_address(struct hfc_multi *c)
{
	unsigned short addr;
	unsigned short highbit;

	addr = cpld_read_reg(c, 0);
	highbit = cpld_read_reg(c, 1);

	addr = addr | (highbit << 8);

	return addr & 0x1ff;
}

inline unsigned char
vpm_in(struct hfc_multi *c, int which, unsigned short addr)
{
	unsigned char res;

	vpm_write_address(c, addr);

	if (!which)
		cpld_set_reg(c, 2);
	else
		cpld_set_reg(c, 3);

	enablepcibridge(c);
	res = readpcibridge(c, 1);
	disablepcibridge(c);

	cpld_set_reg(c, 0);

	return res;
}

inline void
vpm_out(struct hfc_multi *c, int which, unsigned short addr,
    unsigned char data)
{
	vpm_write_address(c, addr);

	enablepcibridge(c);

	if (!which)
		cpld_set_reg(c, 2);
	else
		cpld_set_reg(c, 3);

	writepcibridge(c, 1, data);

	cpld_set_reg(c, 0);

	disablepcibridge(c);

	{
	unsigned char regin;
	regin = vpm_in(c, which, addr);
	if (regin != data)
		printk(KERN_DEBUG "Wrote 0x%x to register 0x%x but got back "
			"0x%x\n", data, addr, regin);
	}

}


static void
vpm_init(struct hfc_multi *wc)
{
	unsigned char reg;
	unsigned int mask;
	unsigned int i, x, y;
	unsigned int ver;

	for (x = 0; x < NUM_EC; x++) {
		/* Setup GPIO's */
		if (!x) {
			ver = vpm_in(wc, x, 0x1a0);
			printk(KERN_DEBUG "VPM: Chip %d: ver %02x\n", x, ver);
		}

		for (y = 0; y < 4; y++) {
			vpm_out(wc, x, 0x1a8 + y, 0x00); /* GPIO out */
			vpm_out(wc, x, 0x1ac + y, 0x00); /* GPIO dir */
			vpm_out(wc, x, 0x1b0 + y, 0x00); /* GPIO sel */
		}

		/* Setup TDM path - sets fsync and tdm_clk as inputs */
		reg = vpm_in(wc, x, 0x1a3); /* misc_con */
		vpm_out(wc, x, 0x1a3, reg & ~2);

		/* Setup Echo length (256 taps) */
		vpm_out(wc, x, 0x022, 1);
		vpm_out(wc, x, 0x023, 0xff);

		/* Setup timeslots */
		vpm_out(wc, x, 0x02f, 0x00);
		mask = 0x02020202 << (x * 4);

		/* Setup the tdm channel masks for all chips */
		for (i = 0; i < 4; i++)
			vpm_out(wc, x, 0x33 - i, (mask >> (i << 3)) & 0xff);

		/* Setup convergence rate */
		printk(KERN_DEBUG "VPM: A-law mode\n");
		reg = 0x00 | 0x10 | 0x01;
		vpm_out(wc, x, 0x20, reg);
		printk(KERN_DEBUG "VPM reg 0x20 is %x\n", reg);
		/*vpm_out(wc, x, 0x20, (0x00 | 0x08 | 0x20 | 0x10)); */

		vpm_out(wc, x, 0x24, 0x02);
		reg = vpm_in(wc, x, 0x24);
		printk(KERN_DEBUG "NLP Thresh is set to %d (0x%x)\n", reg, reg);

		/* Initialize echo cans */
		for (i = 0; i < MAX_TDM_CHAN; i++) {
			if (mask & (0x00000001 << i))
				vpm_out(wc, x, i, 0x00);
		}

		/*
		 * ARM arch at least disallows a udelay of
		 * more than 2ms... it gives a fake "__bad_udelay"
		 * reference at link-time.
		 * long delays in kernel code are pretty sucky anyway
		 * for now work around it using 5 x 2ms instead of 1 x 10ms
		 */

		udelay(2000);
		udelay(2000);
		udelay(2000);
		udelay(2000);
		udelay(2000);

		/* Put in bypass mode */
		for (i = 0; i < MAX_TDM_CHAN; i++) {
			if (mask & (0x00000001 << i))
				vpm_out(wc, x, i, 0x01);
		}

		/* Enable bypass */
		for (i = 0; i < MAX_TDM_CHAN; i++) {
			if (mask & (0x00000001 << i))
				vpm_out(wc, x, 0x78 + i, 0x01);
		}

	}
}

#ifdef UNUSED
static void
vpm_check(struct hfc_multi *hctmp)
{
	unsigned char gpi2;

	gpi2 = HFC_inb(hctmp, R_GPI_IN2);

	if ((gpi2 & 0x3) != 0x3)
		printk(KERN_DEBUG "Got interrupt 0x%x from VPM!\n", gpi2);
}
#endif /* UNUSED */


/*
 * Interface to enable/disable the HW Echocan
 *
 * these functions are called within a spin_lock_irqsave on
 * the channel instance lock, so we are not disturbed by irqs
 *
 * we can later easily change the interface to make  other
 * things configurable, for now we configure the taps
 *
 */

static void
vpm_echocan_on(struct hfc_multi *hc, int ch, int taps)
{
	unsigned int timeslot;
	unsigned int unit;
	struct bchannel *bch = hc->chan[ch].bch;
#ifdef TXADJ
	int txadj = -4;
	struct sk_buff *skb;
#endif
	if (hc->chan[ch].protocol != ISDN_P_B_RAW)
		return;

	if (!bch)
		return;

#ifdef TXADJ
	skb = _alloc_mISDN_skb(PH_CONTROL_IND, HFC_VOL_CHANGE_TX,
		sizeof(int), &txadj, GFP_ATOMIC);
	if (skb)
		recv_Bchannel_skb(bch, skb);
#endif

	timeslot = ((ch/4)*8) + ((ch%4)*4) + 1;
	unit = ch % 4;

	printk(KERN_NOTICE "vpm_echocan_on called taps [%d] on timeslot %d\n",
	    taps, timeslot);

	vpm_out(hc, unit, timeslot, 0x7e);
}

static void
vpm_echocan_off(struct hfc_multi *hc, int ch)
{
	unsigned int timeslot;
	unsigned int unit;
	struct bchannel *bch = hc->chan[ch].bch;
#ifdef TXADJ
	int txadj = 0;
	struct sk_buff *skb;
#endif

	if (hc->chan[ch].protocol != ISDN_P_B_RAW)
		return;

	if (!bch)
		return;

#ifdef TXADJ
	skb = _alloc_mISDN_skb(PH_CONTROL_IND, HFC_VOL_CHANGE_TX,
		sizeof(int), &txadj, GFP_ATOMIC);
	if (skb)
		recv_Bchannel_skb(bch, skb);
#endif

	timeslot = ((ch/4)*8) + ((ch%4)*4) + 1;
	unit = ch % 4;

	printk(KERN_NOTICE "vpm_echocan_off called on timeslot %d\n",
	    timeslot);
	/* FILLME */
	vpm_out(hc, unit, timeslot, 0x01);
}


/*
 * Speech Design resync feature
 * NOTE: This is called sometimes outside interrupt handler.
 * We must lock irqsave, so no other interrupt (other card) will occurr!
 * Also multiple interrupts may nest, so must lock each access (lists, card)!
 */
static inline void
hfcmulti_resync(struct hfc_multi *locked, struct hfc_multi *newmaster, int rm)
{
	struct hfc_multi *hc, *next, *pcmmaster = NULL;
	void __iomem *plx_acc_32;
	u_int pv;
	u_long flags;

	spin_lock_irqsave(&HFClock, flags);
	spin_lock(&plx_lock); /* must be locked inside other locks */

	if (debug & DEBUG_HFCMULTI_PLXSD)
		printk(KERN_DEBUG "%s: RESYNC(syncmaster=0x%p)\n",
			__func__, syncmaster);

	/* select new master */
	if (newmaster) {
		if (debug & DEBUG_HFCMULTI_PLXSD)
			printk(KERN_DEBUG "using provided controller\n");
	} else {
		list_for_each_entry_safe(hc, next, &HFClist, list) {
			if (test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
				if (hc->syncronized) {
					newmaster = hc;
					break;
				}
			}
		}
	}

	/* Disable sync of all cards */
	list_for_each_entry_safe(hc, next, &HFClist, list) {
		if (test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
			plx_acc_32 = hc->plx_membase + PLX_GPIOC;
			pv = readl(plx_acc_32);
			pv &= ~PLX_SYNC_O_EN;
			writel(pv, plx_acc_32);
			if (test_bit(HFC_CHIP_PCM_MASTER, &hc->chip)) {
				pcmmaster = hc;
				if (hc->ctype == HFC_TYPE_E1) {
					if (debug & DEBUG_HFCMULTI_PLXSD)
						printk(KERN_DEBUG
							"Schedule SYNC_I\n");
					hc->e1_resync |= 1; /* get SYNC_I */
				}
			}
		}
	}

	if (newmaster) {
		hc = newmaster;
		if (debug & DEBUG_HFCMULTI_PLXSD)
			printk(KERN_DEBUG "id=%d (0x%p) = syncronized with "
				"interface.\n", hc->id, hc);
		/* Enable new sync master */
		plx_acc_32 = hc->plx_membase + PLX_GPIOC;
		pv = readl(plx_acc_32);
		pv |= PLX_SYNC_O_EN;
		writel(pv, plx_acc_32);
		/* switch to jatt PLL, if not disabled by RX_SYNC */
		if (hc->ctype == HFC_TYPE_E1
				&& !test_bit(HFC_CHIP_RX_SYNC, &hc->chip)) {
			if (debug & DEBUG_HFCMULTI_PLXSD)
				printk(KERN_DEBUG "Schedule jatt PLL\n");
			hc->e1_resync |= 2; /* switch to jatt */
		}
	} else {
		if (pcmmaster) {
			hc = pcmmaster;
			if (debug & DEBUG_HFCMULTI_PLXSD)
				printk(KERN_DEBUG
					"id=%d (0x%p) = PCM master syncronized "
					"with QUARTZ\n", hc->id, hc);
			if (hc->ctype == HFC_TYPE_E1) {
				/* Use the crystal clock for the PCM
				   master card */
				if (debug & DEBUG_HFCMULTI_PLXSD)
					printk(KERN_DEBUG
					    "Schedule QUARTZ for HFC-E1\n");
				hc->e1_resync |= 4; /* switch quartz */
			} else {
				if (debug & DEBUG_HFCMULTI_PLXSD)
					printk(KERN_DEBUG
					    "QUARTZ is automatically "
					    "enabled by HFC-%dS\n", hc->ctype);
			}
			plx_acc_32 = hc->plx_membase + PLX_GPIOC;
			pv = readl(plx_acc_32);
			pv |= PLX_SYNC_O_EN;
			writel(pv, plx_acc_32);
		} else
			if (!rm)
				printk(KERN_ERR "%s no pcm master, this MUST "
					"not happen!\n", __func__);
	}
	syncmaster = newmaster;

	spin_unlock(&plx_lock);
	spin_unlock_irqrestore(&HFClock, flags);
}

/* This must be called AND hc must be locked irqsave!!! */
inline void
plxsd_checksync(struct hfc_multi *hc, int rm)
{
	if (hc->syncronized) {
		if (syncmaster == NULL) {
			if (debug & DEBUG_HFCMULTI_PLXSD)
				printk(KERN_DEBUG "%s: GOT sync on card %d"
					" (id=%d)\n", __func__, hc->id + 1,
					hc->id);
			hfcmulti_resync(hc, hc, rm);
		}
	} else {
		if (syncmaster == hc) {
			if (debug & DEBUG_HFCMULTI_PLXSD)
				printk(KERN_DEBUG "%s: LOST sync on card %d"
					" (id=%d)\n", __func__, hc->id + 1,
					hc->id);
			hfcmulti_resync(hc, NULL, rm);
		}
	}
}


/*
 * free hardware resources used by driver
 */
static void
release_io_hfcmulti(struct hfc_multi *hc)
{
	void __iomem *plx_acc_32;
	u_int	pv;
	u_long	plx_flags;

	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: entered\n", __func__);

	/* soft reset also masks all interrupts */
	hc->hw.r_cirm |= V_SRES;
	HFC_outb(hc, R_CIRM, hc->hw.r_cirm);
	udelay(1000);
	hc->hw.r_cirm &= ~V_SRES;
	HFC_outb(hc, R_CIRM, hc->hw.r_cirm);
	udelay(1000); /* instead of 'wait' that may cause locking */

	/* release Speech Design card, if PLX was initialized */
	if (test_bit(HFC_CHIP_PLXSD, &hc->chip) && hc->plx_membase) {
		if (debug & DEBUG_HFCMULTI_PLXSD)
			printk(KERN_DEBUG "%s: release PLXSD card %d\n",
			    __func__, hc->id + 1);
		spin_lock_irqsave(&plx_lock, plx_flags);
		plx_acc_32 = hc->plx_membase + PLX_GPIOC;
		writel(PLX_GPIOC_INIT, plx_acc_32);
		pv = readl(plx_acc_32);
		/* Termination off */
		pv &= ~PLX_TERM_ON;
		/* Disconnect the PCM */
		pv |= PLX_SLAVE_EN_N;
		pv &= ~PLX_MASTER_EN;
		pv &= ~PLX_SYNC_O_EN;
		/* Put the DSP in Reset */
		pv &= ~PLX_DSP_RES_N;
		writel(pv, plx_acc_32);
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: PCM off: PLX_GPIO=%x\n",
				__func__, pv);
		spin_unlock_irqrestore(&plx_lock, plx_flags);
	}

	/* disable memory mapped ports / io ports */
	test_and_clear_bit(HFC_CHIP_PLXSD, &hc->chip); /* prevent resync */
	if (hc->pci_dev)
		pci_write_config_word(hc->pci_dev, PCI_COMMAND, 0);
	if (hc->pci_membase)
		iounmap(hc->pci_membase);
	if (hc->plx_membase)
		iounmap(hc->plx_membase);
	if (hc->pci_iobase)
		release_region(hc->pci_iobase, 8);
	if (hc->xhfc_membase)
		iounmap((void *)hc->xhfc_membase);

	if (hc->pci_dev) {
		pci_disable_device(hc->pci_dev);
		pci_set_drvdata(hc->pci_dev, NULL);
	}
	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: done\n", __func__);
}

/*
 * function called to reset the HFC chip. A complete software reset of chip
 * and fifos is done. All configuration of the chip is done.
 */

static int
init_chip(struct hfc_multi *hc)
{
	u_long			flags, val, val2 = 0, rev;
	int			i, err = 0;
	u_char			r_conf_en, rval;
	void __iomem		*plx_acc_32;
	u_int			pv;
	u_long			plx_flags, hfc_flags;
	int			plx_count;
	struct hfc_multi	*pos, *next, *plx_last_hc;

	spin_lock_irqsave(&hc->lock, flags);
	/* reset all registers */
	memset(&hc->hw, 0, sizeof(struct hfcm_hw));

	/* revision check */
	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: entered\n", __func__);
	val = HFC_inb(hc, R_CHIP_ID);
	if ((val >> 4) != 0x8 && (val >> 4) != 0xc && (val >> 4) != 0xe &&
	    (val >> 1) != 0x31) {
		printk(KERN_INFO "HFC_multi: unknown CHIP_ID:%x\n", (u_int)val);
		err = -EIO;
		goto out;
	}
	rev = HFC_inb(hc, R_CHIP_RV);
	printk(KERN_INFO
	    "HFC_multi: detected HFC with chip ID=0x%lx revision=%ld%s\n",
	    val, rev, (rev == 0 && (hc->ctype != HFC_TYPE_XHFC)) ?
		" (old FIFO handling)" : "");
	if (hc->ctype != HFC_TYPE_XHFC && rev == 0) {
		test_and_set_bit(HFC_CHIP_REVISION0, &hc->chip);
		printk(KERN_WARNING
		    "HFC_multi: NOTE: Your chip is revision 0, "
		    "ask Cologne Chip for update. Newer chips "
		    "have a better FIFO handling. Old chips "
		    "still work but may have slightly lower "
		    "HDLC transmit performance.\n");
	}
	if (rev > 1) {
		printk(KERN_WARNING "HFC_multi: WARNING: This driver doesn't "
		    "consider chip revision = %ld. The chip / "
		    "bridge may not work.\n", rev);
	}

	/* set s-ram size */
	hc->Flen = 0x10;
	hc->Zmin = 0x80;
	hc->Zlen = 384;
	hc->DTMFbase = 0x1000;
	if (test_bit(HFC_CHIP_EXRAM_128, &hc->chip)) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: changing to 128K extenal RAM\n",
			    __func__);
		hc->hw.r_ctrl |= V_EXT_RAM;
		hc->hw.r_ram_sz = 1;
		hc->Flen = 0x20;
		hc->Zmin = 0xc0;
		hc->Zlen = 1856;
		hc->DTMFbase = 0x2000;
	}
	if (test_bit(HFC_CHIP_EXRAM_512, &hc->chip)) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: changing to 512K extenal RAM\n",
			    __func__);
		hc->hw.r_ctrl |= V_EXT_RAM;
		hc->hw.r_ram_sz = 2;
		hc->Flen = 0x20;
		hc->Zmin = 0xc0;
		hc->Zlen = 8000;
		hc->DTMFbase = 0x2000;
	}
	if (hc->ctype == HFC_TYPE_XHFC) {
		hc->Flen = 0x8;
		hc->Zmin = 0x0;
		hc->Zlen = 64;
		hc->DTMFbase = 0x0;
	}
	hc->max_trans = poll << 1;
	if (hc->max_trans > hc->Zlen)
		hc->max_trans = hc->Zlen;

	/* Speech Design PLX bridge */
	if (test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
		if (debug & DEBUG_HFCMULTI_PLXSD)
			printk(KERN_DEBUG "%s: initializing PLXSD card %d\n",
			    __func__, hc->id + 1);
		spin_lock_irqsave(&plx_lock, plx_flags);
		plx_acc_32 = hc->plx_membase + PLX_GPIOC;
		writel(PLX_GPIOC_INIT, plx_acc_32);
		pv = readl(plx_acc_32);
		/* The first and the last cards are terminating the PCM bus */
		pv |= PLX_TERM_ON; /* hc is currently the last */
		/* Disconnect the PCM */
		pv |= PLX_SLAVE_EN_N;
		pv &= ~PLX_MASTER_EN;
		pv &= ~PLX_SYNC_O_EN;
		/* Put the DSP in Reset */
		pv &= ~PLX_DSP_RES_N;
		writel(pv, plx_acc_32);
		spin_unlock_irqrestore(&plx_lock, plx_flags);
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: slave/term: PLX_GPIO=%x\n",
				__func__, pv);
		/*
		 * If we are the 3rd PLXSD card or higher, we must turn
		 * termination of last PLXSD card off.
		 */
		spin_lock_irqsave(&HFClock, hfc_flags);
		plx_count = 0;
		plx_last_hc = NULL;
		list_for_each_entry_safe(pos, next, &HFClist, list) {
			if (test_bit(HFC_CHIP_PLXSD, &pos->chip)) {
				plx_count++;
				if (pos != hc)
					plx_last_hc = pos;
			}
		}
		if (plx_count >= 3) {
			if (debug & DEBUG_HFCMULTI_PLXSD)
				printk(KERN_DEBUG "%s: card %d is between, so "
					"we disable termination\n",
				    __func__, plx_last_hc->id + 1);
			spin_lock_irqsave(&plx_lock, plx_flags);
			plx_acc_32 = plx_last_hc->plx_membase + PLX_GPIOC;
			pv = readl(plx_acc_32);
			pv &= ~PLX_TERM_ON;
			writel(pv, plx_acc_32);
			spin_unlock_irqrestore(&plx_lock, plx_flags);
			if (debug & DEBUG_HFCMULTI_INIT)
				printk(KERN_DEBUG
				    "%s: term off: PLX_GPIO=%x\n",
				    __func__, pv);
		}
		spin_unlock_irqrestore(&HFClock, hfc_flags);
		hc->hw.r_pcm_md0 = V_F0_LEN; /* shift clock for DSP */
	}

	if (test_bit(HFC_CHIP_EMBSD, &hc->chip))
		hc->hw.r_pcm_md0 = V_F0_LEN; /* shift clock for DSP */

	/* we only want the real Z2 read-pointer for revision > 0 */
	if (!test_bit(HFC_CHIP_REVISION0, &hc->chip))
		hc->hw.r_ram_sz |= V_FZ_MD;

	/* select pcm mode */
	if (test_bit(HFC_CHIP_PCM_SLAVE, &hc->chip)) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: setting PCM into slave mode\n",
			    __func__);
	} else
	if (test_bit(HFC_CHIP_PCM_MASTER, &hc->chip) && !plxsd_master) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: setting PCM into master mode\n",
			    __func__);
		hc->hw.r_pcm_md0 |= V_PCM_MD;
	} else {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: performing PCM auto detect\n",
			    __func__);
	}

	/* soft reset */
	HFC_outb(hc, R_CTRL, hc->hw.r_ctrl);
	if (hc->ctype == HFC_TYPE_XHFC)
		HFC_outb(hc, 0x0C /* R_FIFO_THRES */,
				0x11 /* 16 Bytes TX/RX */);
	else
		HFC_outb(hc, R_RAM_SZ, hc->hw.r_ram_sz);
	HFC_outb(hc, R_FIFO_MD, 0);
	if (hc->ctype == HFC_TYPE_XHFC)
		hc->hw.r_cirm = V_SRES | V_HFCRES | V_PCMRES | V_STRES;
	else
		hc->hw.r_cirm = V_SRES | V_HFCRES | V_PCMRES | V_STRES
			| V_RLD_EPR;
	HFC_outb(hc, R_CIRM, hc->hw.r_cirm);
	udelay(100);
	hc->hw.r_cirm = 0;
	HFC_outb(hc, R_CIRM, hc->hw.r_cirm);
	udelay(100);
	if (hc->ctype != HFC_TYPE_XHFC)
		HFC_outb(hc, R_RAM_SZ, hc->hw.r_ram_sz);

	/* Speech Design PLX bridge pcm and sync mode */
	if (test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
		spin_lock_irqsave(&plx_lock, plx_flags);
		plx_acc_32 = hc->plx_membase + PLX_GPIOC;
		pv = readl(plx_acc_32);
		/* Connect PCM */
		if (hc->hw.r_pcm_md0 & V_PCM_MD) {
			pv |= PLX_MASTER_EN | PLX_SLAVE_EN_N;
			pv |= PLX_SYNC_O_EN;
			if (debug & DEBUG_HFCMULTI_INIT)
				printk(KERN_DEBUG "%s: master: PLX_GPIO=%x\n",
					__func__, pv);
		} else {
			pv &= ~(PLX_MASTER_EN | PLX_SLAVE_EN_N);
			pv &= ~PLX_SYNC_O_EN;
			if (debug & DEBUG_HFCMULTI_INIT)
				printk(KERN_DEBUG "%s: slave: PLX_GPIO=%x\n",
					__func__, pv);
		}
		writel(pv, plx_acc_32);
		spin_unlock_irqrestore(&plx_lock, plx_flags);
	}

	/* PCM setup */
	HFC_outb(hc, R_PCM_MD0, hc->hw.r_pcm_md0 | 0x90);
	if (hc->slots == 32)
		HFC_outb(hc, R_PCM_MD1, 0x00);
	if (hc->slots == 64)
		HFC_outb(hc, R_PCM_MD1, 0x10);
	if (hc->slots == 128)
		HFC_outb(hc, R_PCM_MD1, 0x20);
	HFC_outb(hc, R_PCM_MD0, hc->hw.r_pcm_md0 | 0xa0);
	if (test_bit(HFC_CHIP_PLXSD, &hc->chip))
		HFC_outb(hc, R_PCM_MD2, V_SYNC_SRC); /* sync via SYNC_I / O */
	else if (test_bit(HFC_CHIP_EMBSD, &hc->chip))
		HFC_outb(hc, R_PCM_MD2, 0x10); /* V_C2O_EN */
	else
		HFC_outb(hc, R_PCM_MD2, 0x00); /* sync from interface */
	HFC_outb(hc, R_PCM_MD0, hc->hw.r_pcm_md0 | 0x00);
	for (i = 0; i < 256; i++) {
		HFC_outb_nodebug(hc, R_SLOT, i);
		HFC_outb_nodebug(hc, A_SL_CFG, 0);
		if (hc->ctype != HFC_TYPE_XHFC)
			HFC_outb_nodebug(hc, A_CONF, 0);
		hc->slot_owner[i] = -1;
	}

	/* set clock speed */
	if (test_bit(HFC_CHIP_CLOCK2, &hc->chip)) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG
			    "%s: setting double clock\n", __func__);
		HFC_outb(hc, R_BRG_PCM_CFG, V_PCM_CLK);
	}

	if (test_bit(HFC_CHIP_EMBSD, &hc->chip))
		HFC_outb(hc, 0x02 /* R_CLK_CFG */, 0x40 /* V_CLKO_OFF */);

	/* B410P GPIO */
	if (test_bit(HFC_CHIP_B410P, &hc->chip)) {
		printk(KERN_NOTICE "Setting GPIOs\n");
		HFC_outb(hc, R_GPIO_SEL, 0x30);
		HFC_outb(hc, R_GPIO_EN1, 0x3);
		udelay(1000);
		printk(KERN_NOTICE "calling vpm_init\n");
		vpm_init(hc);
	}

	/* check if R_F0_CNT counts (8 kHz frame count) */
	val = HFC_inb(hc, R_F0_CNTL);
	val += HFC_inb(hc, R_F0_CNTH) << 8;
	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG
		    "HFC_multi F0_CNT %ld after reset\n", val);
	spin_unlock_irqrestore(&hc->lock, flags);
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout((HZ/100)?:1); /* Timeout minimum 10ms */
	spin_lock_irqsave(&hc->lock, flags);
	val2 = HFC_inb(hc, R_F0_CNTL);
	val2 += HFC_inb(hc, R_F0_CNTH) << 8;
	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG
			"HFC_multi F0_CNT %ld after 10 ms (1st try)\n",
		    val2);
	if (val2 >= val+8) { /* 1 ms */
		/* it counts, so we keep the pcm mode */
		if (test_bit(HFC_CHIP_PCM_MASTER, &hc->chip))
			printk(KERN_INFO "controller is PCM bus MASTER\n");
		else
		if (test_bit(HFC_CHIP_PCM_SLAVE, &hc->chip))
			printk(KERN_INFO "controller is PCM bus SLAVE\n");
		else {
			test_and_set_bit(HFC_CHIP_PCM_SLAVE, &hc->chip);
			printk(KERN_INFO "controller is PCM bus SLAVE "
				"(auto detected)\n");
		}
	} else {
		/* does not count */
		if (test_bit(HFC_CHIP_PCM_MASTER, &hc->chip)) {
controller_fail:
			printk(KERN_ERR "HFC_multi ERROR, getting no 125us "
			    "pulse. Seems that controller fails.\n");
			err = -EIO;
			goto out;
		}
		if (test_bit(HFC_CHIP_PCM_SLAVE, &hc->chip)) {
			printk(KERN_INFO "controller is PCM bus SLAVE "
				"(ignoring missing PCM clock)\n");
		} else {
			/* only one pcm master */
			if (test_bit(HFC_CHIP_PLXSD, &hc->chip)
				&& plxsd_master) {
				printk(KERN_ERR "HFC_multi ERROR, no clock "
				    "on another Speech Design card found. "
				    "Please be sure to connect PCM cable.\n");
				err = -EIO;
				goto out;
			}
			/* retry with master clock */
			if (test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
				spin_lock_irqsave(&plx_lock, plx_flags);
				plx_acc_32 = hc->plx_membase + PLX_GPIOC;
				pv = readl(plx_acc_32);
				pv |= PLX_MASTER_EN | PLX_SLAVE_EN_N;
				pv |= PLX_SYNC_O_EN;
				writel(pv, plx_acc_32);
				spin_unlock_irqrestore(&plx_lock, plx_flags);
				if (debug & DEBUG_HFCMULTI_INIT)
					printk(KERN_DEBUG "%s: master: "
					    "PLX_GPIO=%x\n", __func__, pv);
			}
			hc->hw.r_pcm_md0 |= V_PCM_MD;
			HFC_outb(hc, R_PCM_MD0, hc->hw.r_pcm_md0 | 0x00);
			spin_unlock_irqrestore(&hc->lock, flags);
			set_current_state(TASK_UNINTERRUPTIBLE);
			schedule_timeout((HZ/100)?:1); /* Timeout min. 10ms */
			spin_lock_irqsave(&hc->lock, flags);
			val2 = HFC_inb(hc, R_F0_CNTL);
			val2 += HFC_inb(hc, R_F0_CNTH) << 8;
			if (debug & DEBUG_HFCMULTI_INIT)
				printk(KERN_DEBUG "HFC_multi F0_CNT %ld after "
					"10 ms (2nd try)\n", val2);
			if (val2 >= val+8) { /* 1 ms */
				test_and_set_bit(HFC_CHIP_PCM_MASTER,
					&hc->chip);
				printk(KERN_INFO "controller is PCM bus MASTER "
					"(auto detected)\n");
			} else
				goto controller_fail;
		}
	}

	/* Release the DSP Reset */
	if (test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
		if (test_bit(HFC_CHIP_PCM_MASTER, &hc->chip))
			plxsd_master = 1;
		spin_lock_irqsave(&plx_lock, plx_flags);
		plx_acc_32 = hc->plx_membase + PLX_GPIOC;
		pv = readl(plx_acc_32);
		pv |=  PLX_DSP_RES_N;
		writel(pv, plx_acc_32);
		spin_unlock_irqrestore(&plx_lock, plx_flags);
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: reset off: PLX_GPIO=%x\n",
				__func__, pv);
	}

	/* pcm id */
	if (hc->pcm)
		printk(KERN_INFO "controller has given PCM BUS ID %d\n",
			hc->pcm);
	else {
		if (test_bit(HFC_CHIP_PCM_MASTER, &hc->chip)
		 || test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
			PCM_cnt++; /* SD has proprietary bridging */
		}
		hc->pcm = PCM_cnt;
		printk(KERN_INFO "controller has PCM BUS ID %d "
			"(auto selected)\n", hc->pcm);
	}

	/* set up timer */
	HFC_outb(hc, R_TI_WD, poll_timer);
	hc->hw.r_irqmsk_misc |= V_TI_IRQMSK;

	/* set E1 state machine IRQ */
	if (hc->ctype == HFC_TYPE_E1)
		hc->hw.r_irqmsk_misc |= V_STA_IRQMSK;

	/* set DTMF detection */
	if (test_bit(HFC_CHIP_DTMF, &hc->chip)) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: enabling DTMF detection "
			    "for all B-channel\n", __func__);
		hc->hw.r_dtmf = V_DTMF_EN | V_DTMF_STOP;
		if (test_bit(HFC_CHIP_ULAW, &hc->chip))
			hc->hw.r_dtmf |= V_ULAW_SEL;
		HFC_outb(hc, R_DTMF_N, 102 - 1);
		hc->hw.r_irqmsk_misc |= V_DTMF_IRQMSK;
	}

	/* conference engine */
	if (test_bit(HFC_CHIP_ULAW, &hc->chip))
		r_conf_en = V_CONF_EN | V_ULAW;
	else
		r_conf_en = V_CONF_EN;
	if (hc->ctype != HFC_TYPE_XHFC)
		HFC_outb(hc, R_CONF_EN, r_conf_en);

	/* setting leds */
	switch (hc->leds) {
	case 1: /* HFC-E1 OEM */
		if (test_bit(HFC_CHIP_WATCHDOG, &hc->chip))
			HFC_outb(hc, R_GPIO_SEL, 0x32);
		else
			HFC_outb(hc, R_GPIO_SEL, 0x30);

		HFC_outb(hc, R_GPIO_EN1, 0x0f);
		HFC_outb(hc, R_GPIO_OUT1, 0x00);

		HFC_outb(hc, R_GPIO_EN0, V_GPIO_EN2 | V_GPIO_EN3);
		break;

	case 2: /* HFC-4S OEM */
	case 3:
		HFC_outb(hc, R_GPIO_SEL, 0xf0);
		HFC_outb(hc, R_GPIO_EN1, 0xff);
		HFC_outb(hc, R_GPIO_OUT1, 0x00);
		break;
	}

	if (test_bit(HFC_CHIP_EMBSD, &hc->chip)) {
		hc->hw.r_st_sync = 0x10; /* V_AUTO_SYNCI */
		HFC_outb(hc, R_ST_SYNC, hc->hw.r_st_sync);
	}

	/* set master clock */
	if (hc->masterclk >= 0) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: setting ST master clock "
			    "to port %d (0..%d)\n",
			    __func__, hc->masterclk, hc->ports-1);
		hc->hw.r_st_sync |= (hc->masterclk | V_AUTO_SYNC);
		HFC_outb(hc, R_ST_SYNC, hc->hw.r_st_sync);
	}



	/* setting misc irq */
	HFC_outb(hc, R_IRQMSK_MISC, hc->hw.r_irqmsk_misc);
	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "r_irqmsk_misc.2: 0x%x\n",
		    hc->hw.r_irqmsk_misc);

	/* RAM access test */
	HFC_outb(hc, R_RAM_ADDR0, 0);
	HFC_outb(hc, R_RAM_ADDR1, 0);
	HFC_outb(hc, R_RAM_ADDR2, 0);
	for (i = 0; i < 256; i++) {
		HFC_outb_nodebug(hc, R_RAM_ADDR0, i);
		HFC_outb_nodebug(hc, R_RAM_DATA, ((i*3)&0xff));
	}
	for (i = 0; i < 256; i++) {
		HFC_outb_nodebug(hc, R_RAM_ADDR0, i);
		HFC_inb_nodebug(hc, R_RAM_DATA);
		rval = HFC_inb_nodebug(hc, R_INT_DATA);
		if (rval != ((i * 3) & 0xff)) {
			printk(KERN_DEBUG
			    "addr:%x val:%x should:%x\n", i, rval,
			    (i * 3) & 0xff);
			err++;
		}
	}
	if (err) {
		printk(KERN_DEBUG "aborting - %d RAM access errors\n", err);
		err = -EIO;
		goto out;
	}

	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: done\n", __func__);
out:
	spin_unlock_irqrestore(&hc->lock, flags);
	return err;
}


/*
 * control the watchdog
 */
static void
hfcmulti_watchdog(struct hfc_multi *hc)
{
	hc->wdcount++;

	if (hc->wdcount > 10) {
		hc->wdcount = 0;
		hc->wdbyte = hc->wdbyte == V_GPIO_OUT2 ?
		    V_GPIO_OUT3 : V_GPIO_OUT2;

	/* printk("Sending Watchdog Kill %x\n",hc->wdbyte); */
		HFC_outb(hc, R_GPIO_EN0, V_GPIO_EN2 | V_GPIO_EN3);
		HFC_outb(hc, R_GPIO_OUT0, hc->wdbyte);
	}
}



/*
 * output leds
 */
static void
hfcmulti_leds(struct hfc_multi *hc)
{
	unsigned long lled;
	unsigned long leddw;
	int i, state, active, leds;
	struct dchannel *dch;
	int led[4];

	hc->ledcount += poll;
	if (hc->ledcount > 4096) {
		hc->ledcount -= 4096;
		hc->ledstate = 0xAFFEAFFE;
	}

	switch (hc->leds) {
	case 1: /* HFC-E1 OEM */
		/* 2 red blinking: NT mode deactivate
		 * 2 red steady:   TE mode deactivate
		 * left green:     L1 active
		 * left red:       frame sync, but no L1
		 * right green:    L2 active
		 */
		if (hc->chan[hc->dslot].sync != 2) { /* no frame sync */
			if (hc->chan[hc->dslot].dch->dev.D.protocol
				!= ISDN_P_NT_E1) {
				led[0] = 1;
				led[1] = 1;
			} else if (hc->ledcount>>11) {
				led[0] = 1;
				led[1] = 1;
			} else {
				led[0] = 0;
				led[1] = 0;
			}
			led[2] = 0;
			led[3] = 0;
		} else { /* with frame sync */
			/* TODO make it work */
			led[0] = 0;
			led[1] = 0;
			led[2] = 0;
			led[3] = 1;
		}
		leds = (led[0] | (led[1]<<2) | (led[2]<<1) | (led[3]<<3))^0xF;
			/* leds are inverted */
		if (leds != (int)hc->ledstate) {
			HFC_outb_nodebug(hc, R_GPIO_OUT1, leds);
			hc->ledstate = leds;
		}
		break;

	case 2: /* HFC-4S OEM */
		/* red blinking = PH_DEACTIVATE NT Mode
		 * red steady   = PH_DEACTIVATE TE Mode
		 * green steady = PH_ACTIVATE
		 */
		for (i = 0; i < 4; i++) {
			state = 0;
			active = -1;
			dch = hc->chan[(i << 2) | 2].dch;
			if (dch) {
				state = dch->state;
				if (dch->dev.D.protocol == ISDN_P_NT_S0)
					active = 3;
				else
					active = 7;
			}
			if (state) {
				if (state == active) {
					led[i] = 1; /* led green */
				} else
					if (dch->dev.D.protocol == ISDN_P_TE_S0)
						/* TE mode: led red */
						led[i] = 2;
					else
						if (hc->ledcount>>11)
							/* led red */
							led[i] = 2;
						else
							/* led off */
							led[i] = 0;
			} else
				led[i] = 0; /* led off */
		}
		if (test_bit(HFC_CHIP_B410P, &hc->chip)) {
			leds = 0;
			for (i = 0; i < 4; i++) {
				if (led[i] == 1) {
					/*green*/
					leds |= (0x2 << (i * 2));
				} else if (led[i] == 2) {
					/*red*/
					leds |= (0x1 << (i * 2));
				}
			}
			if (leds != (int)hc->ledstate) {
				vpm_out(hc, 0, 0x1a8 + 3, leds);
				hc->ledstate = leds;
			}
		} else {
			leds = ((led[3] > 0) << 0) | ((led[1] > 0) << 1) |
			    ((led[0] > 0) << 2) | ((led[2] > 0) << 3) |
			    ((led[3] & 1) << 4) | ((led[1] & 1) << 5) |
			    ((led[0] & 1) << 6) | ((led[2] & 1) << 7);
			if (leds != (int)hc->ledstate) {
				HFC_outb_nodebug(hc, R_GPIO_EN1, leds & 0x0F);
				HFC_outb_nodebug(hc, R_GPIO_OUT1, leds >> 4);
				hc->ledstate = leds;
			}
		}
		break;

	case 3: /* HFC 1S/2S Beronet */
		/* red blinking = PH_DEACTIVATE NT Mode
		 * red steady   = PH_DEACTIVATE TE Mode
		 * green steady = PH_ACTIVATE
		 */
		for (i = 0; i < 2; i++) {
			state = 0;
			active = -1;
			dch = hc->chan[(i << 2) | 2].dch;
			if (dch) {
				state = dch->state;
				if (dch->dev.D.protocol == ISDN_P_NT_S0)
					active = 3;
				else
					active = 7;
			}
			if (state) {
				if (state == active) {
					led[i] = 1; /* led green */
				} else
					if (dch->dev.D.protocol == ISDN_P_TE_S0)
						/* TE mode: led red */
						led[i] = 2;
					else
						if (hc->ledcount >> 11)
							/* led red */
							led[i] = 2;
						else
							/* led off */
							led[i] = 0;
			} else
				led[i] = 0; /* led off */
		}


		leds = (led[0] > 0) | ((led[1] > 0)<<1) | ((led[0]&1)<<2)
			| ((led[1]&1)<<3);
		if (leds != (int)hc->ledstate) {
			HFC_outb_nodebug(hc, R_GPIO_EN1,
			    ((led[0] > 0) << 2) | ((led[1] > 0) << 3));
			HFC_outb_nodebug(hc, R_GPIO_OUT1,
			    ((led[0] & 1) << 2) | ((led[1] & 1) << 3));
			hc->ledstate = leds;
		}
		break;
	case 8: /* HFC 8S+ Beronet */
		lled = 0;

		for (i = 0; i < 8; i++) {
			state = 0;
			active = -1;
			dch = hc->chan[(i << 2) | 2].dch;
			if (dch) {
				state = dch->state;
				if (dch->dev.D.protocol == ISDN_P_NT_S0)
					active = 3;
				else
					active = 7;
			}
			if (state) {
				if (state == active) {
					lled |= 0 << i;
				} else
					if (hc->ledcount >> 11)
						lled |= 0 << i;
					else
						lled |= 1 << i;
			} else
				lled |= 1 << i;
		}
		leddw = lled << 24 | lled << 16 | lled << 8 | lled;
		if (leddw != hc->ledstate) {
			/* HFC_outb(hc, R_BRG_PCM_CFG, 1);
			HFC_outb(c, R_BRG_PCM_CFG, (0x0 << 6) | 0x3); */
			/* was _io before */
			HFC_outb_nodebug(hc, R_BRG_PCM_CFG, 1 | V_PCM_CLK);
			outw(0x4000, hc->pci_iobase + 4);
			outl(leddw, hc->pci_iobase);
			HFC_outb_nodebug(hc, R_BRG_PCM_CFG, V_PCM_CLK);
			hc->ledstate = leddw;
		}
		break;
	}
}
/*
 * read dtmf coefficients
 */

static void
hfcmulti_dtmf(struct hfc_multi *hc)
{
	s32		*coeff;
	u_int		mantissa;
	int		co, ch;
	struct bchannel	*bch = NULL;
	u8		exponent;
	int		dtmf = 0;
	int		addr;
	u16		w_float;
	struct sk_buff	*skb;
	struct mISDNhead *hh;

	if (debug & DEBUG_HFCMULTI_DTMF)
		printk(KERN_DEBUG "%s: dtmf detection irq\n", __func__);
	for (ch = 0; ch <= 31; ch++) {
		/* only process enabled B-channels */
		bch = hc->chan[ch].bch;
		if (!bch)
			continue;
		if (!hc->created[hc->chan[ch].port])
			continue;
		if (!test_bit(FLG_TRANSPARENT, &bch->Flags))
			continue;
		if (debug & DEBUG_HFCMULTI_DTMF)
			printk(KERN_DEBUG "%s: dtmf channel %d:",
				__func__, ch);
		coeff = &(hc->chan[ch].coeff[hc->chan[ch].coeff_count * 16]);
		dtmf = 1;
		for (co = 0; co < 8; co++) {
			/* read W(n-1) coefficient */
			addr = hc->DTMFbase + ((co<<7) | (ch<<2));
			HFC_outb_nodebug(hc, R_RAM_ADDR0, addr);
			HFC_outb_nodebug(hc, R_RAM_ADDR1, addr>>8);
			HFC_outb_nodebug(hc, R_RAM_ADDR2, (addr>>16)
				| V_ADDR_INC);
			w_float = HFC_inb_nodebug(hc, R_RAM_DATA);
			w_float |= (HFC_inb_nodebug(hc, R_RAM_DATA) << 8);
			if (debug & DEBUG_HFCMULTI_DTMF)
				printk(" %04x", w_float);

			/* decode float (see chip doc) */
			mantissa = w_float & 0x0fff;
			if (w_float & 0x8000)
				mantissa |= 0xfffff000;
			exponent = (w_float>>12) & 0x7;
			if (exponent) {
				mantissa ^= 0x1000;
				mantissa <<= (exponent-1);
			}

			/* store coefficient */
			coeff[co<<1] = mantissa;

			/* read W(n) coefficient */
			w_float = HFC_inb_nodebug(hc, R_RAM_DATA);
			w_float |= (HFC_inb_nodebug(hc, R_RAM_DATA) << 8);
			if (debug & DEBUG_HFCMULTI_DTMF)
				printk(" %04x", w_float);

			/* decode float (see chip doc) */
			mantissa = w_float & 0x0fff;
			if (w_float & 0x8000)
				mantissa |= 0xfffff000;
			exponent = (w_float>>12) & 0x7;
			if (exponent) {
				mantissa ^= 0x1000;
				mantissa <<= (exponent-1);
			}

			/* store coefficient */
			coeff[(co<<1)|1] = mantissa;
		}
		if (debug & DEBUG_HFCMULTI_DTMF)
			printk(" DTMF ready %08x %08x %08x %08x "
			    "%08x %08x %08x %08x\n",
			    coeff[0], coeff[1], coeff[2], coeff[3],
			    coeff[4], coeff[5], coeff[6], coeff[7]);
		hc->chan[ch].coeff_count++;
		if (hc->chan[ch].coeff_count == 8) {
			hc->chan[ch].coeff_count = 0;
			skb = mI_alloc_skb(512, GFP_ATOMIC);
			if (!skb) {
				printk(KERN_DEBUG "%s: No memory for skb\n",
				    __func__);
				continue;
			}
			hh = mISDN_HEAD_P(skb);
			hh->prim = PH_CONTROL_IND;
			hh->id = DTMF_HFC_COEF;
			memcpy(skb_put(skb, 512), hc->chan[ch].coeff, 512);
			recv_Bchannel_skb(bch, skb);
		}
	}

	/* restart DTMF processing */
	hc->dtmf = dtmf;
	if (dtmf)
		HFC_outb_nodebug(hc, R_DTMF, hc->hw.r_dtmf | V_RST_DTMF);
}


/*
 * fill fifo as much as possible
 */

static void
hfcmulti_tx(struct hfc_multi *hc, int ch)
{
	int i, ii, temp, len = 0;
	int Zspace, z1, z2; /* must be int for calculation */
	int Fspace, f1, f2;
	u_char *d;
	int *txpending, slot_tx;
	struct	bchannel *bch;
	struct  dchannel *dch;
	struct  sk_buff **sp = NULL;
	int *idxp;

	bch = hc->chan[ch].bch;
	dch = hc->chan[ch].dch;
	if ((!dch) && (!bch))
		return;

	txpending = &hc->chan[ch].txpending;
	slot_tx = hc->chan[ch].slot_tx;
	if (dch) {
		if (!test_bit(FLG_ACTIVE, &dch->Flags))
			return;
		sp = &dch->tx_skb;
		idxp = &dch->tx_idx;
	} else {
		if (!test_bit(FLG_ACTIVE, &bch->Flags))
			return;
		sp = &bch->tx_skb;
		idxp = &bch->tx_idx;
	}
	if (*sp)
		len = (*sp)->len;

	if ((!len) && *txpending != 1)
		return; /* no data */

	if (test_bit(HFC_CHIP_B410P, &hc->chip) &&
	    (hc->chan[ch].protocol == ISDN_P_B_RAW) &&
	    (hc->chan[ch].slot_rx < 0) &&
	    (hc->chan[ch].slot_tx < 0))
		HFC_outb_nodebug(hc, R_FIFO, 0x20 | (ch << 1));
	else
		HFC_outb_nodebug(hc, R_FIFO, ch << 1);
	HFC_wait_nodebug(hc);

	if (*txpending == 2) {
		/* reset fifo */
		HFC_outb_nodebug(hc, R_INC_RES_FIFO, V_RES_F);
		HFC_wait_nodebug(hc);
		HFC_outb(hc, A_SUBCH_CFG, 0);
		*txpending = 1;
	}
next_frame:
	if (dch || test_bit(FLG_HDLC, &bch->Flags)) {
		f1 = HFC_inb_nodebug(hc, A_F1);
		f2 = HFC_inb_nodebug(hc, A_F2);
		while (f2 != (temp = HFC_inb_nodebug(hc, A_F2))) {
			if (debug & DEBUG_HFCMULTI_FIFO)
				printk(KERN_DEBUG
				    "%s(card %d): reread f2 because %d!=%d\n",
				    __func__, hc->id + 1, temp, f2);
			f2 = temp; /* repeat until F2 is equal */
		}
		Fspace = f2 - f1 - 1;
		if (Fspace < 0)
			Fspace += hc->Flen;
		/*
		 * Old FIFO handling doesn't give us the current Z2 read
		 * pointer, so we cannot send the next frame before the fifo
		 * is empty. It makes no difference except for a slightly
		 * lower performance.
		 */
		if (test_bit(HFC_CHIP_REVISION0, &hc->chip)) {
			if (f1 != f2)
				Fspace = 0;
			else
				Fspace = 1;
		}
		/* one frame only for ST D-channels, to allow resending */
		if (hc->ctype != HFC_TYPE_E1 && dch) {
			if (f1 != f2)
				Fspace = 0;
		}
		/* F-counter full condition */
		if (Fspace == 0)
			return;
	}
	z1 = HFC_inw_nodebug(hc, A_Z1) - hc->Zmin;
	z2 = HFC_inw_nodebug(hc, A_Z2) - hc->Zmin;
	while (z2 != (temp = (HFC_inw_nodebug(hc, A_Z2) - hc->Zmin))) {
		if (debug & DEBUG_HFCMULTI_FIFO)
			printk(KERN_DEBUG "%s(card %d): reread z2 because "
				"%d!=%d\n", __func__, hc->id + 1, temp, z2);
		z2 = temp; /* repeat unti Z2 is equal */
	}
	hc->chan[ch].Zfill = z1 - z2;
	if (hc->chan[ch].Zfill < 0)
		hc->chan[ch].Zfill += hc->Zlen;
	Zspace = z2 - z1;
	if (Zspace <= 0)
		Zspace += hc->Zlen;
	Zspace -= 4; /* keep not too full, so pointers will not overrun */
	/* fill transparent data only to maxinum transparent load (minus 4) */
	if (bch && test_bit(FLG_TRANSPARENT, &bch->Flags))
		Zspace = Zspace - hc->Zlen + hc->max_trans;
	if (Zspace <= 0) /* no space of 4 bytes */
		return;

	/* if no data */
	if (!len) {
		if (z1 == z2) { /* empty */
			/* if done with FIFO audio data during PCM connection */
			if (bch && (!test_bit(FLG_HDLC, &bch->Flags)) &&
			    *txpending && slot_tx >= 0) {
				if (debug & DEBUG_HFCMULTI_MODE)
					printk(KERN_DEBUG
					    "%s: reconnecting PCM due to no "
					    "more FIFO data: channel %d "
					    "slot_tx %d\n",
					    __func__, ch, slot_tx);
				/* connect slot */
				if (hc->ctype == HFC_TYPE_XHFC)
					HFC_outb(hc, A_CON_HDLC, 0xc0
					    | 0x07 << 2 | V_HDLC_TRP | V_IFF);
						/* Enable FIFO, no interrupt */
				else
					HFC_outb(hc, A_CON_HDLC, 0xc0 | 0x00 |
					    V_HDLC_TRP | V_IFF);
				HFC_outb_nodebug(hc, R_FIFO, ch<<1 | 1);
				HFC_wait_nodebug(hc);
				if (hc->ctype == HFC_TYPE_XHFC)
					HFC_outb(hc, A_CON_HDLC, 0xc0
					    | 0x07 << 2 | V_HDLC_TRP | V_IFF);
						/* Enable FIFO, no interrupt */
				else
					HFC_outb(hc, A_CON_HDLC, 0xc0 | 0x00 |
					    V_HDLC_TRP | V_IFF);
				HFC_outb_nodebug(hc, R_FIFO, ch<<1);
				HFC_wait_nodebug(hc);
			}
			*txpending = 0;
		}
		return; /* no data */
	}

	/* "fill fifo if empty" feature */
	if (bch && test_bit(FLG_FILLEMPTY, &bch->Flags)
		&& !test_bit(FLG_HDLC, &bch->Flags) && z2 == z1) {
		if (debug & DEBUG_HFCMULTI_FILL)
			printk(KERN_DEBUG "%s: buffer empty, so we have "
				"underrun\n", __func__);
		/* fill buffer, to prevent future underrun */
		hc->write_fifo(hc, hc->silence_data, poll >> 1);
		Zspace -= (poll >> 1);
	}

	/* if audio data and connected slot */
	if (bch && (!test_bit(FLG_HDLC, &bch->Flags)) && (!*txpending)
		&& slot_tx >= 0) {
		if (debug & DEBUG_HFCMULTI_MODE)
			printk(KERN_DEBUG "%s: disconnecting PCM due to "
			    "FIFO data: channel %d slot_tx %d\n",
			    __func__, ch, slot_tx);
		/* disconnect slot */
		if (hc->ctype == HFC_TYPE_XHFC)
			HFC_outb(hc, A_CON_HDLC, 0x80
			    | 0x07 << 2 | V_HDLC_TRP | V_IFF);
				/* Enable FIFO, no interrupt */
		else
			HFC_outb(hc, A_CON_HDLC, 0x80 | 0x00 |
			    V_HDLC_TRP | V_IFF);
		HFC_outb_nodebug(hc, R_FIFO, ch<<1 | 1);
		HFC_wait_nodebug(hc);
		if (hc->ctype == HFC_TYPE_XHFC)
			HFC_outb(hc, A_CON_HDLC, 0x80
			    | 0x07 << 2 | V_HDLC_TRP | V_IFF);
				/* Enable FIFO, no interrupt */
		else
			HFC_outb(hc, A_CON_HDLC, 0x80 | 0x00 |
			    V_HDLC_TRP | V_IFF);
		HFC_outb_nodebug(hc, R_FIFO, ch<<1);
		HFC_wait_nodebug(hc);
	}
	*txpending = 1;

	/* show activity */
	hc->activity[hc->chan[ch].port] = 1;

	/* fill fifo to what we have left */
	ii = len;
	if (dch || test_bit(FLG_HDLC, &bch->Flags))
		temp = 1;
	else
		temp = 0;
	i = *idxp;
	d = (*sp)->data + i;
	if (ii - i > Zspace)
		ii = Zspace + i;
	if (debug & DEBUG_HFCMULTI_FIFO)
		printk(KERN_DEBUG "%s(card %d): fifo(%d) has %d bytes space "
		    "left (z1=%04x, z2=%04x) sending %d of %d bytes %s\n",
			__func__, hc->id + 1, ch, Zspace, z1, z2, ii-i, len-i,
			temp ? "HDLC" : "TRANS");

	/* Have to prep the audio data */
	hc->write_fifo(hc, d, ii - i);
	hc->chan[ch].Zfill += ii - i;
	*idxp = ii;

	/* if not all data has been written */
	if (ii != len) {
		/* NOTE: fifo is started by the calling function */
		return;
	}

	/* if all data has been written, terminate frame */
	if (dch || test_bit(FLG_HDLC, &bch->Flags)) {
		/* increment f-counter */
		HFC_outb_nodebug(hc, R_INC_RES_FIFO, V_INC_F);
		HFC_wait_nodebug(hc);
	}

	/* send confirm, since get_net_bframe will not do it with trans */
	if (bch && test_bit(FLG_TRANSPARENT, &bch->Flags))
		confirm_Bsend(bch);

	/* check for next frame */
	dev_kfree_skb(*sp);
	if (bch && get_next_bframe(bch)) { /* hdlc is confirmed here */
		len = (*sp)->len;
		goto next_frame;
	}
	if (dch && get_next_dframe(dch)) {
		len = (*sp)->len;
		goto next_frame;
	}

	/*
	 * now we have no more data, so in case of transparent,
	 * we set the last byte in fifo to 'silence' in case we will get
	 * no more data at all. this prevents sending an undefined value.
	 */
	if (bch && test_bit(FLG_TRANSPARENT, &bch->Flags))
		HFC_outb_nodebug(hc, A_FIFO_DATA0_NOINC, hc->silence);
}


/* NOTE: only called if E1 card is in active state */
static void
hfcmulti_rx(struct hfc_multi *hc, int ch)
{
	int temp;
	int Zsize, z1, z2 = 0; /* = 0, to make GCC happy */
	int f1 = 0, f2 = 0; /* = 0, to make GCC happy */
	int again = 0;
	struct	bchannel *bch;
	struct  dchannel *dch;
	struct sk_buff	*skb, **sp = NULL;
	int	maxlen;

	bch = hc->chan[ch].bch;
	dch = hc->chan[ch].dch;
	if ((!dch) && (!bch))
		return;
	if (dch) {
		if (!test_bit(FLG_ACTIVE, &dch->Flags))
			return;
		sp = &dch->rx_skb;
		maxlen = dch->maxlen;
	} else {
		if (!test_bit(FLG_ACTIVE, &bch->Flags))
			return;
		sp = &bch->rx_skb;
		maxlen = bch->maxlen;
	}
next_frame:
	/* on first AND before getting next valid frame, R_FIFO must be written
	   to. */
	if (test_bit(HFC_CHIP_B410P, &hc->chip) &&
	    (hc->chan[ch].protocol == ISDN_P_B_RAW) &&
	    (hc->chan[ch].slot_rx < 0) &&
	    (hc->chan[ch].slot_tx < 0))
		HFC_outb_nodebug(hc, R_FIFO, 0x20 | (ch<<1) | 1);
	else
		HFC_outb_nodebug(hc, R_FIFO, (ch<<1)|1);
	HFC_wait_nodebug(hc);

	/* ignore if rx is off BUT change fifo (above) to start pending TX */
	if (hc->chan[ch].rx_off)
		return;

	if (dch || test_bit(FLG_HDLC, &bch->Flags)) {
		f1 = HFC_inb_nodebug(hc, A_F1);
		while (f1 != (temp = HFC_inb_nodebug(hc, A_F1))) {
			if (debug & DEBUG_HFCMULTI_FIFO)
				printk(KERN_DEBUG
				    "%s(card %d): reread f1 because %d!=%d\n",
				    __func__, hc->id + 1, temp, f1);
			f1 = temp; /* repeat until F1 is equal */
		}
		f2 = HFC_inb_nodebug(hc, A_F2);
	}
	z1 = HFC_inw_nodebug(hc, A_Z1) - hc->Zmin;
	while (z1 != (temp = (HFC_inw_nodebug(hc, A_Z1) - hc->Zmin))) {
		if (debug & DEBUG_HFCMULTI_FIFO)
			printk(KERN_DEBUG "%s(card %d): reread z2 because "
				"%d!=%d\n", __func__, hc->id + 1, temp, z2);
		z1 = temp; /* repeat until Z1 is equal */
	}
	z2 = HFC_inw_nodebug(hc, A_Z2) - hc->Zmin;
	Zsize = z1 - z2;
	if ((dch || test_bit(FLG_HDLC, &bch->Flags)) && f1 != f2)
		/* complete hdlc frame */
		Zsize++;
	if (Zsize < 0)
		Zsize += hc->Zlen;
	/* if buffer is empty */
	if (Zsize <= 0)
		return;

	if (*sp == NULL) {
		*sp = mI_alloc_skb(maxlen + 3, GFP_ATOMIC);
		if (*sp == NULL) {
			printk(KERN_DEBUG "%s: No mem for rx_skb\n",
			    __func__);
			return;
		}
	}
	/* show activity */
	hc->activity[hc->chan[ch].port] = 1;

	/* empty fifo with what we have */
	if (dch || test_bit(FLG_HDLC, &bch->Flags)) {
		if (debug & DEBUG_HFCMULTI_FIFO)
			printk(KERN_DEBUG "%s(card %d): fifo(%d) reading %d "
			    "bytes (z1=%04x, z2=%04x) HDLC %s (f1=%d, f2=%d) "
			    "got=%d (again %d)\n", __func__, hc->id + 1, ch,
			    Zsize, z1, z2, (f1 == f2) ? "fragment" : "COMPLETE",
			    f1, f2, Zsize + (*sp)->len, again);
		/* HDLC */
		if ((Zsize + (*sp)->len) > (maxlen + 3)) {
			if (debug & DEBUG_HFCMULTI_FIFO)
				printk(KERN_DEBUG
				    "%s(card %d): hdlc-frame too large.\n",
				    __func__, hc->id + 1);
			skb_trim(*sp, 0);
			HFC_outb_nodebug(hc, R_INC_RES_FIFO, V_RES_F);
			HFC_wait_nodebug(hc);
			return;
		}

		hc->read_fifo(hc, skb_put(*sp, Zsize), Zsize);

		if (f1 != f2) {
			/* increment Z2,F2-counter */
			HFC_outb_nodebug(hc, R_INC_RES_FIFO, V_INC_F);
			HFC_wait_nodebug(hc);
			/* check size */
			if ((*sp)->len < 4) {
				if (debug & DEBUG_HFCMULTI_FIFO)
					printk(KERN_DEBUG
					    "%s(card %d): Frame below minimum "
					    "size\n", __func__, hc->id + 1);
				skb_trim(*sp, 0);
				goto next_frame;
			}
			/* there is at least one complete frame, check crc */
			if ((*sp)->data[(*sp)->len - 1]) {
				if (debug & DEBUG_HFCMULTI_CRC)
					printk(KERN_DEBUG
					    "%s: CRC-error\n", __func__);
				skb_trim(*sp, 0);
				goto next_frame;
			}
			skb_trim(*sp, (*sp)->len - 3);
			if ((*sp)->len < MISDN_COPY_SIZE) {
				skb = *sp;
				*sp = mI_alloc_skb(skb->len, GFP_ATOMIC);
				if (*sp) {
					memcpy(skb_put(*sp, skb->len),
					    skb->data, skb->len);
					skb_trim(skb, 0);
				} else {
					printk(KERN_DEBUG "%s: No mem\n",
					    __func__);
					*sp = skb;
					skb = NULL;
				}
			} else {
				skb = NULL;
			}
			if (debug & DEBUG_HFCMULTI_FIFO) {
				printk(KERN_DEBUG "%s(card %d):",
					__func__, hc->id + 1);
				temp = 0;
				while (temp < (*sp)->len)
					printk(" %02x", (*sp)->data[temp++]);
				printk("\n");
			}
			if (dch)
				recv_Dchannel(dch);
			else
				recv_Bchannel(bch, MISDN_ID_ANY);
			*sp = skb;
			again++;
			goto next_frame;
		}
		/* there is an incomplete frame */
	} else {
		/* transparent */
		if (Zsize > skb_tailroom(*sp))
			Zsize = skb_tailroom(*sp);
		hc->read_fifo(hc, skb_put(*sp, Zsize), Zsize);
		if (((*sp)->len) < MISDN_COPY_SIZE) {
			skb = *sp;
			*sp = mI_alloc_skb(skb->len, GFP_ATOMIC);
			if (*sp) {
				memcpy(skb_put(*sp, skb->len),
				    skb->data, skb->len);
				skb_trim(skb, 0);
			} else {
				printk(KERN_DEBUG "%s: No mem\n", __func__);
				*sp = skb;
				skb = NULL;
			}
		} else {
			skb = NULL;
		}
		if (debug & DEBUG_HFCMULTI_FIFO)
			printk(KERN_DEBUG
			    "%s(card %d): fifo(%d) reading %d bytes "
			    "(z1=%04x, z2=%04x) TRANS\n",
				__func__, hc->id + 1, ch, Zsize, z1, z2);
		/* only bch is transparent */
		recv_Bchannel(bch, hc->chan[ch].Zfill);
		*sp = skb;
	}
}


/*
 * Interrupt handler
 */
static void
signal_state_up(struct dchannel *dch, int info, char *msg)
{
	struct sk_buff	*skb;
	int		id, data = info;

	if (debug & DEBUG_HFCMULTI_STATE)
		printk(KERN_DEBUG "%s: %s\n", __func__, msg);

	id = TEI_SAPI | (GROUP_TEI << 8); /* manager address */

	skb = _alloc_mISDN_skb(MPH_INFORMATION_IND, id, sizeof(data), &data,
		GFP_ATOMIC);
	if (!skb)
		return;
	recv_Dchannel_skb(dch, skb);
}

static inline void
handle_timer_irq(struct hfc_multi *hc)
{
	int		ch, temp;
	struct dchannel	*dch;
	u_long		flags;

	/* process queued resync jobs */
	if (hc->e1_resync) {
		/* lock, so e1_resync gets not changed */
		spin_lock_irqsave(&HFClock, flags);
		if (hc->e1_resync & 1) {
			if (debug & DEBUG_HFCMULTI_PLXSD)
				printk(KERN_DEBUG "Enable SYNC_I\n");
			HFC_outb(hc, R_SYNC_CTRL, V_EXT_CLK_SYNC);
			/* disable JATT, if RX_SYNC is set */
			if (test_bit(HFC_CHIP_RX_SYNC, &hc->chip))
				HFC_outb(hc, R_SYNC_OUT, V_SYNC_E1_RX);
		}
		if (hc->e1_resync & 2) {
			if (debug & DEBUG_HFCMULTI_PLXSD)
				printk(KERN_DEBUG "Enable jatt PLL\n");
			HFC_outb(hc, R_SYNC_CTRL, V_SYNC_OFFS);
		}
		if (hc->e1_resync & 4) {
			if (debug & DEBUG_HFCMULTI_PLXSD)
				printk(KERN_DEBUG
				    "Enable QUARTZ for HFC-E1\n");
			/* set jatt to quartz */
			HFC_outb(hc, R_SYNC_CTRL, V_EXT_CLK_SYNC
				| V_JATT_OFF);
			/* switch to JATT, in case it is not already */
			HFC_outb(hc, R_SYNC_OUT, 0);
		}
		hc->e1_resync = 0;
		spin_unlock_irqrestore(&HFClock, flags);
	}

	if (hc->ctype != HFC_TYPE_E1 || hc->e1_state == 1)
		for (ch = 0; ch <= 31; ch++) {
			if (hc->created[hc->chan[ch].port]) {
				hfcmulti_tx(hc, ch);
				/* fifo is started when switching to rx-fifo */
				hfcmulti_rx(hc, ch);
				if (hc->chan[ch].dch &&
				    hc->chan[ch].nt_timer > -1) {
					dch = hc->chan[ch].dch;
					if (!(--hc->chan[ch].nt_timer)) {
						schedule_event(dch,
						    FLG_PHCHANGE);
						if (debug &
						    DEBUG_HFCMULTI_STATE)
							printk(KERN_DEBUG
							    "%s: nt_timer at "
							    "state %x\n",
							    __func__,
							    dch->state);
					}
				}
			}
		}
	if (hc->ctype == HFC_TYPE_E1 && hc->created[0]) {
		dch = hc->chan[hc->dslot].dch;
		if (test_bit(HFC_CFG_REPORT_LOS, &hc->chan[hc->dslot].cfg)) {
			/* LOS */
			temp = HFC_inb_nodebug(hc, R_SYNC_STA) & V_SIG_LOS;
			if (!temp && hc->chan[hc->dslot].los)
				signal_state_up(dch, L1_SIGNAL_LOS_ON,
				    "LOS detected");
			if (temp && !hc->chan[hc->dslot].los)
				signal_state_up(dch, L1_SIGNAL_LOS_OFF,
				    "LOS gone");
			hc->chan[hc->dslot].los = temp;
		}
		if (test_bit(HFC_CFG_REPORT_AIS, &hc->chan[hc->dslot].cfg)) {
			/* AIS */
			temp = HFC_inb_nodebug(hc, R_SYNC_STA) & V_AIS;
			if (!temp && hc->chan[hc->dslot].ais)
				signal_state_up(dch, L1_SIGNAL_AIS_ON,
				    "AIS detected");
			if (temp && !hc->chan[hc->dslot].ais)
				signal_state_up(dch, L1_SIGNAL_AIS_OFF,
				    "AIS gone");
			hc->chan[hc->dslot].ais = temp;
		}
		if (test_bit(HFC_CFG_REPORT_SLIP, &hc->chan[hc->dslot].cfg)) {
			/* SLIP */
			temp = HFC_inb_nodebug(hc, R_SLIP) & V_FOSLIP_RX;
			if (!temp && hc->chan[hc->dslot].slip_rx)
				signal_state_up(dch, L1_SIGNAL_SLIP_RX,
				    " bit SLIP detected RX");
			hc->chan[hc->dslot].slip_rx = temp;
			temp = HFC_inb_nodebug(hc, R_SLIP) & V_FOSLIP_TX;
			if (!temp && hc->chan[hc->dslot].slip_tx)
				signal_state_up(dch, L1_SIGNAL_SLIP_TX,
				    " bit SLIP detected TX");
			hc->chan[hc->dslot].slip_tx = temp;
		}
		if (test_bit(HFC_CFG_REPORT_RDI, &hc->chan[hc->dslot].cfg)) {
			/* RDI */
			temp = HFC_inb_nodebug(hc, R_RX_SL0_0) & V_A;
			if (!temp && hc->chan[hc->dslot].rdi)
				signal_state_up(dch, L1_SIGNAL_RDI_ON,
				    "RDI detected");
			if (temp && !hc->chan[hc->dslot].rdi)
				signal_state_up(dch, L1_SIGNAL_RDI_OFF,
				    "RDI gone");
			hc->chan[hc->dslot].rdi = temp;
		}
		temp = HFC_inb_nodebug(hc, R_JATT_DIR);
		switch (hc->chan[hc->dslot].sync) {
		case 0:
			if ((temp & 0x60) == 0x60) {
				if (debug & DEBUG_HFCMULTI_SYNC)
					printk(KERN_DEBUG
					    "%s: (id=%d) E1 now "
					    "in clock sync\n",
					    __func__, hc->id);
				HFC_outb(hc, R_RX_OFF,
				    hc->chan[hc->dslot].jitter | V_RX_INIT);
				HFC_outb(hc, R_TX_OFF,
				    hc->chan[hc->dslot].jitter | V_RX_INIT);
				hc->chan[hc->dslot].sync = 1;
				goto check_framesync;
			}
			break;
		case 1:
			if ((temp & 0x60) != 0x60) {
				if (debug & DEBUG_HFCMULTI_SYNC)
					printk(KERN_DEBUG
					    "%s: (id=%d) E1 "
					    "lost clock sync\n",
					    __func__, hc->id);
				hc->chan[hc->dslot].sync = 0;
				break;
			}
check_framesync:
			temp = HFC_inb_nodebug(hc, R_SYNC_STA);
			if (temp == 0x27) {
				if (debug & DEBUG_HFCMULTI_SYNC)
					printk(KERN_DEBUG
					    "%s: (id=%d) E1 "
					    "now in frame sync\n",
					    __func__, hc->id);
				hc->chan[hc->dslot].sync = 2;
			}
			break;
		case 2:
			if ((temp & 0x60) != 0x60) {
				if (debug & DEBUG_HFCMULTI_SYNC)
					printk(KERN_DEBUG
					    "%s: (id=%d) E1 lost "
					    "clock & frame sync\n",
					    __func__, hc->id);
				hc->chan[hc->dslot].sync = 0;
				break;
			}
			temp = HFC_inb_nodebug(hc, R_SYNC_STA);
			if (temp != 0x27) {
				if (debug & DEBUG_HFCMULTI_SYNC)
					printk(KERN_DEBUG
					    "%s: (id=%d) E1 "
					    "lost frame sync\n",
					    __func__, hc->id);
				hc->chan[hc->dslot].sync = 1;
			}
			break;
		}
	}

	if (test_bit(HFC_CHIP_WATCHDOG, &hc->chip))
		hfcmulti_watchdog(hc);

	if (hc->leds)
		hfcmulti_leds(hc);
}

static void
ph_state_irq(struct hfc_multi *hc, u_char r_irq_statech)
{
	struct dchannel	*dch;
	int		ch;
	int		active;
	u_char		st_status, temp;

	/* state machine */
	for (ch = 0; ch <= 31; ch++) {
		if (hc->chan[ch].dch) {
			dch = hc->chan[ch].dch;
			if (r_irq_statech & 1) {
				HFC_outb_nodebug(hc, R_ST_SEL,
					hc->chan[ch].port);
				/* undocumented: delay after R_ST_SEL */
				udelay(1);
				/* undocumented: status changes during read */
				st_status = HFC_inb_nodebug(hc, A_ST_RD_STATE);
				while (st_status != (temp =
					HFC_inb_nodebug(hc, A_ST_RD_STATE))) {
					if (debug & DEBUG_HFCMULTI_STATE)
						printk(KERN_DEBUG "%s: reread "
						    "STATE because %d!=%d\n",
						    __func__, temp,
						    st_status);
					st_status = temp; /* repeat */
				}

				/* Speech Design TE-sync indication */
				if (test_bit(HFC_CHIP_PLXSD, &hc->chip) &&
					dch->dev.D.protocol == ISDN_P_TE_S0) {
					if (st_status & V_FR_SYNC_ST)
						hc->syncronized |=
						    (1 << hc->chan[ch].port);
					else
						hc->syncronized &=
						   ~(1 << hc->chan[ch].port);
				}
				dch->state = st_status & 0x0f;
				if (dch->dev.D.protocol == ISDN_P_NT_S0)
					active = 3;
				else
					active = 7;
				if (dch->state == active) {
					HFC_outb_nodebug(hc, R_FIFO,
						(ch << 1) | 1);
					HFC_wait_nodebug(hc);
					HFC_outb_nodebug(hc,
						R_INC_RES_FIFO, V_RES_F);
					HFC_wait_nodebug(hc);
					dch->tx_idx = 0;
				}
				schedule_event(dch, FLG_PHCHANGE);
				if (debug & DEBUG_HFCMULTI_STATE)
					printk(KERN_DEBUG
					    "%s: S/T newstate %x port %d\n",
					    __func__, dch->state,
					    hc->chan[ch].port);
			}
			r_irq_statech >>= 1;
		}
	}
	if (test_bit(HFC_CHIP_PLXSD, &hc->chip))
		plxsd_checksync(hc, 0);
}

static void
fifo_irq(struct hfc_multi *hc, int block)
{
	int	ch, j;
	struct dchannel	*dch;
	struct bchannel	*bch;
	u_char r_irq_fifo_bl;

	r_irq_fifo_bl = HFC_inb_nodebug(hc, R_IRQ_FIFO_BL0 + block);
	j = 0;
	while (j < 8) {
		ch = (block << 2) + (j >> 1);
		dch = hc->chan[ch].dch;
		bch = hc->chan[ch].bch;
		if (((!dch) && (!bch)) || (!hc->created[hc->chan[ch].port])) {
			j += 2;
			continue;
		}
		if (dch && (r_irq_fifo_bl & (1 << j)) &&
		    test_bit(FLG_ACTIVE, &dch->Flags)) {
			hfcmulti_tx(hc, ch);
			/* start fifo */
			HFC_outb_nodebug(hc, R_FIFO, 0);
			HFC_wait_nodebug(hc);
		}
		if (bch && (r_irq_fifo_bl & (1 << j)) &&
		    test_bit(FLG_ACTIVE, &bch->Flags)) {
			hfcmulti_tx(hc, ch);
			/* start fifo */
			HFC_outb_nodebug(hc, R_FIFO, 0);
			HFC_wait_nodebug(hc);
		}
		j++;
		if (dch && (r_irq_fifo_bl & (1 << j)) &&
		    test_bit(FLG_ACTIVE, &dch->Flags)) {
			hfcmulti_rx(hc, ch);
		}
		if (bch && (r_irq_fifo_bl & (1 << j)) &&
		    test_bit(FLG_ACTIVE, &bch->Flags)) {
			hfcmulti_rx(hc, ch);
		}
		j++;
	}
}

#ifdef IRQ_DEBUG
int irqsem;
#endif
static irqreturn_t
hfcmulti_interrupt(int intno, void *dev_id)
{
#ifdef IRQCOUNT_DEBUG
	static int iq1 = 0, iq2 = 0, iq3 = 0, iq4 = 0,
	    iq5 = 0, iq6 = 0, iqcnt = 0;
#endif
	struct hfc_multi	*hc = dev_id;
	struct dchannel		*dch;
	u_char			r_irq_statech, status, r_irq_misc, r_irq_oview;
	int			i;
	void __iomem		*plx_acc;
	u_short			wval;
	u_char			e1_syncsta, temp;
	u_long			flags;

	if (!hc) {
		printk(KERN_ERR "HFC-multi: Spurious interrupt!\n");
		return IRQ_NONE;
	}

	spin_lock(&hc->lock);

#ifdef IRQ_DEBUG
	if (irqsem)
		printk(KERN_ERR "irq for card %d during irq from "
		"card %d, this is no bug.\n", hc->id + 1, irqsem);
	irqsem = hc->id + 1;
#endif
#ifdef CONFIG_MISDN_HFCMULTI_8xx
	if (hc->immap->im_cpm.cp_pbdat & hc->pb_irqmsk)
		goto irq_notforus;
#endif
	if (test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
		spin_lock_irqsave(&plx_lock, flags);
		plx_acc = hc->plx_membase + PLX_INTCSR;
		wval = readw(plx_acc);
		spin_unlock_irqrestore(&plx_lock, flags);
		if (!(wval & PLX_INTCSR_LINTI1_STATUS))
			goto irq_notforus;
	}

	status = HFC_inb_nodebug(hc, R_STATUS);
	r_irq_statech = HFC_inb_nodebug(hc, R_IRQ_STATECH);
#ifdef IRQCOUNT_DEBUG
	if (r_irq_statech)
		iq1++;
	if (status & V_DTMF_STA)
		iq2++;
	if (status & V_LOST_STA)
		iq3++;
	if (status & V_EXT_IRQSTA)
		iq4++;
	if (status & V_MISC_IRQSTA)
		iq5++;
	if (status & V_FR_IRQSTA)
		iq6++;
	if (iqcnt++ > 5000) {
		printk(KERN_ERR "iq1:%x iq2:%x iq3:%x iq4:%x iq5:%x iq6:%x\n",
		    iq1, iq2, iq3, iq4, iq5, iq6);
		iqcnt = 0;
	}
#endif

	if (!r_irq_statech &&
	    !(status & (V_DTMF_STA | V_LOST_STA | V_EXT_IRQSTA |
	    V_MISC_IRQSTA | V_FR_IRQSTA))) {
		/* irq is not for us */
		goto irq_notforus;
	}
	hc->irqcnt++;
	if (r_irq_statech) {
		if (hc->ctype != HFC_TYPE_E1)
			ph_state_irq(hc, r_irq_statech);
	}
	if (status & V_EXT_IRQSTA)
		; /* external IRQ */
	if (status & V_LOST_STA) {
		/* LOST IRQ */
		HFC_outb(hc, R_INC_RES_FIFO, V_RES_LOST); /* clear irq! */
	}
	if (status & V_MISC_IRQSTA) {
		/* misc IRQ */
		r_irq_misc = HFC_inb_nodebug(hc, R_IRQ_MISC);
		r_irq_misc &= hc->hw.r_irqmsk_misc; /* ignore disabled irqs */
		if (r_irq_misc & V_STA_IRQ) {
			if (hc->ctype == HFC_TYPE_E1) {
				/* state machine */
				dch = hc->chan[hc->dslot].dch;
				e1_syncsta = HFC_inb_nodebug(hc, R_SYNC_STA);
				if (test_bit(HFC_CHIP_PLXSD, &hc->chip)
				 && hc->e1_getclock) {
					if (e1_syncsta & V_FR_SYNC_E1)
						hc->syncronized = 1;
					else
						hc->syncronized = 0;
				}
				/* undocumented: status changes during read */
				dch->state = HFC_inb_nodebug(hc, R_E1_RD_STA);
				while (dch->state != (temp =
					HFC_inb_nodebug(hc, R_E1_RD_STA))) {
					if (debug & DEBUG_HFCMULTI_STATE)
						printk(KERN_DEBUG "%s: reread "
						    "STATE because %d!=%d\n",
						    __func__, temp,
						    dch->state);
					dch->state = temp; /* repeat */
				}
				dch->state = HFC_inb_nodebug(hc, R_E1_RD_STA)
					& 0x7;
				schedule_event(dch, FLG_PHCHANGE);
				if (debug & DEBUG_HFCMULTI_STATE)
					printk(KERN_DEBUG
					    "%s: E1 (id=%d) newstate %x\n",
					    __func__, hc->id, dch->state);
				if (test_bit(HFC_CHIP_PLXSD, &hc->chip))
					plxsd_checksync(hc, 0);
			}
		}
		if (r_irq_misc & V_TI_IRQ) {
			if (hc->iclock_on)
				mISDN_clock_update(hc->iclock, poll, NULL);
			handle_timer_irq(hc);
		}

		if (r_irq_misc & V_DTMF_IRQ)
			hfcmulti_dtmf(hc);

		if (r_irq_misc & V_IRQ_PROC) {
			static int irq_proc_cnt;
			if (!irq_proc_cnt++)
				printk(KERN_DEBUG "%s: got V_IRQ_PROC -"
				    " this should not happen\n", __func__);
		}

	}
	if (status & V_FR_IRQSTA) {
		/* FIFO IRQ */
		r_irq_oview = HFC_inb_nodebug(hc, R_IRQ_OVIEW);
		for (i = 0; i < 8; i++) {
			if (r_irq_oview & (1 << i))
				fifo_irq(hc, i);
		}
	}

#ifdef IRQ_DEBUG
	irqsem = 0;
#endif
	spin_unlock(&hc->lock);
	return IRQ_HANDLED;

irq_notforus:
#ifdef IRQ_DEBUG
	irqsem = 0;
#endif
	spin_unlock(&hc->lock);
	return IRQ_NONE;
}


/*
 * timer callback for D-chan busy resolution. Currently no function
 */

static void
hfcmulti_dbusy_timer(struct hfc_multi *hc)
{
}


/*
 * activate/deactivate hardware for selected channels and mode
 *
 * configure B-channel with the given protocol
 * ch eqals to the HFC-channel (0-31)
 * ch is the number of channel (0-4,4-7,8-11,12-15,16-19,20-23,24-27,28-31
 * for S/T, 1-31 for E1)
 * the hdlc interrupts will be set/unset
 */
static int
mode_hfcmulti(struct hfc_multi *hc, int ch, int protocol, int slot_tx,
    int bank_tx, int slot_rx, int bank_rx)
{
	int flow_tx = 0, flow_rx = 0, routing = 0;
	int oslot_tx, oslot_rx;
	int conf;

	if (ch < 0 || ch > 31)
		return EINVAL;
	oslot_tx = hc->chan[ch].slot_tx;
	oslot_rx = hc->chan[ch].slot_rx;
	conf = hc->chan[ch].conf;

	if (debug & DEBUG_HFCMULTI_MODE)
		printk(KERN_DEBUG
		    "%s: card %d channel %d protocol %x slot old=%d new=%d "
		    "bank new=%d (TX) slot old=%d new=%d bank new=%d (RX)\n",
		    __func__, hc->id, ch, protocol, oslot_tx, slot_tx,
		    bank_tx, oslot_rx, slot_rx, bank_rx);

	if (oslot_tx >= 0 && slot_tx != oslot_tx) {
		/* remove from slot */
		if (debug & DEBUG_HFCMULTI_MODE)
			printk(KERN_DEBUG "%s: remove from slot %d (TX)\n",
			    __func__, oslot_tx);
		if (hc->slot_owner[oslot_tx<<1] == ch) {
			HFC_outb(hc, R_SLOT, oslot_tx << 1);
			HFC_outb(hc, A_SL_CFG, 0);
			if (hc->ctype != HFC_TYPE_XHFC)
				HFC_outb(hc, A_CONF, 0);
			hc->slot_owner[oslot_tx<<1] = -1;
		} else {
			if (debug & DEBUG_HFCMULTI_MODE)
				printk(KERN_DEBUG
				    "%s: we are not owner of this tx slot "
				    "anymore, channel %d is.\n",
				    __func__, hc->slot_owner[oslot_tx<<1]);
		}
	}

	if (oslot_rx >= 0 && slot_rx != oslot_rx) {
		/* remove from slot */
		if (debug & DEBUG_HFCMULTI_MODE)
			printk(KERN_DEBUG
			    "%s: remove from slot %d (RX)\n",
			    __func__, oslot_rx);
		if (hc->slot_owner[(oslot_rx << 1) | 1] == ch) {
			HFC_outb(hc, R_SLOT, (oslot_rx << 1) | V_SL_DIR);
			HFC_outb(hc, A_SL_CFG, 0);
			hc->slot_owner[(oslot_rx << 1) | 1] = -1;
		} else {
			if (debug & DEBUG_HFCMULTI_MODE)
				printk(KERN_DEBUG
				    "%s: we are not owner of this rx slot "
				    "anymore, channel %d is.\n",
				    __func__,
				    hc->slot_owner[(oslot_rx << 1) | 1]);
		}
	}

	if (slot_tx < 0) {
		flow_tx = 0x80; /* FIFO->ST */
		/* disable pcm slot */
		hc->chan[ch].slot_tx = -1;
		hc->chan[ch].bank_tx = 0;
	} else {
		/* set pcm slot */
		if (hc->chan[ch].txpending)
			flow_tx = 0x80; /* FIFO->ST */
		else
			flow_tx = 0xc0; /* PCM->ST */
		/* put on slot */
		routing = bank_tx ? 0xc0 : 0x80;
		if (conf >= 0 || bank_tx > 1)
			routing = 0x40; /* loop */
		if (debug & DEBUG_HFCMULTI_MODE)
			printk(KERN_DEBUG "%s: put channel %d to slot %d bank"
			    " %d flow %02x routing %02x conf %d (TX)\n",
			    __func__, ch, slot_tx, bank_tx,
			    flow_tx, routing, conf);
		HFC_outb(hc, R_SLOT, slot_tx << 1);
		HFC_outb(hc, A_SL_CFG, (ch<<1) | routing);
		if (hc->ctype != HFC_TYPE_XHFC)
			HFC_outb(hc, A_CONF,
				(conf < 0) ? 0 : (conf | V_CONF_SL));
		hc->slot_owner[slot_tx << 1] = ch;
		hc->chan[ch].slot_tx = slot_tx;
		hc->chan[ch].bank_tx = bank_tx;
	}
	if (slot_rx < 0) {
		/* disable pcm slot */
		flow_rx = 0x80; /* ST->FIFO */
		hc->chan[ch].slot_rx = -1;
		hc->chan[ch].bank_rx = 0;
	} else {
		/* set pcm slot */
		if (hc->chan[ch].txpending)
			flow_rx = 0x80; /* ST->FIFO */
		else
			flow_rx = 0xc0; /* ST->(FIFO,PCM) */
		/* put on slot */
		routing = bank_rx ? 0x80 : 0xc0; /* reversed */
		if (conf >= 0 || bank_rx > 1)
			routing = 0x40; /* loop */
		if (debug & DEBUG_HFCMULTI_MODE)
			printk(KERN_DEBUG "%s: put channel %d to slot %d bank"
			    " %d flow %02x routing %02x conf %d (RX)\n",
			    __func__, ch, slot_rx, bank_rx,
			    flow_rx, routing, conf);
		HFC_outb(hc, R_SLOT, (slot_rx<<1) | V_SL_DIR);
		HFC_outb(hc, A_SL_CFG, (ch<<1) | V_CH_DIR | routing);
		hc->slot_owner[(slot_rx<<1)|1] = ch;
		hc->chan[ch].slot_rx = slot_rx;
		hc->chan[ch].bank_rx = bank_rx;
	}

	switch (protocol) {
	case (ISDN_P_NONE):
		/* disable TX fifo */
		HFC_outb(hc, R_FIFO, ch << 1);
		HFC_wait(hc);
		HFC_outb(hc, A_CON_HDLC, flow_tx | 0x00 | V_IFF);
		HFC_outb(hc, A_SUBCH_CFG, 0);
		HFC_outb(hc, A_IRQ_MSK, 0);
		HFC_outb(hc, R_INC_RES_FIFO, V_RES_F);
		HFC_wait(hc);
		/* disable RX fifo */
		HFC_outb(hc, R_FIFO, (ch<<1)|1);
		HFC_wait(hc);
		HFC_outb(hc, A_CON_HDLC, flow_rx | 0x00);
		HFC_outb(hc, A_SUBCH_CFG, 0);
		HFC_outb(hc, A_IRQ_MSK, 0);
		HFC_outb(hc, R_INC_RES_FIFO, V_RES_F);
		HFC_wait(hc);
		if (hc->chan[ch].bch && hc->ctype != HFC_TYPE_E1) {
			hc->hw.a_st_ctrl0[hc->chan[ch].port] &=
			    ((ch & 0x3) == 0) ? ~V_B1_EN : ~V_B2_EN;
			HFC_outb(hc, R_ST_SEL, hc->chan[ch].port);
			/* undocumented: delay after R_ST_SEL */
			udelay(1);
			HFC_outb(hc, A_ST_CTRL0,
			    hc->hw.a_st_ctrl0[hc->chan[ch].port]);
		}
		if (hc->chan[ch].bch) {
			test_and_clear_bit(FLG_HDLC, &hc->chan[ch].bch->Flags);
			test_and_clear_bit(FLG_TRANSPARENT,
			    &hc->chan[ch].bch->Flags);
		}
		break;
	case (ISDN_P_B_RAW): /* B-channel */

		if (test_bit(HFC_CHIP_B410P, &hc->chip) &&
		    (hc->chan[ch].slot_rx < 0) &&
		    (hc->chan[ch].slot_tx < 0)) {

			printk(KERN_DEBUG
			    "Setting B-channel %d to echo cancelable "
			    "state on PCM slot %d\n", ch,
			    ((ch / 4) * 8) + ((ch % 4) * 4) + 1);
			printk(KERN_DEBUG
			    "Enabling pass through for channel\n");
			vpm_out(hc, ch, ((ch / 4) * 8) +
			    ((ch % 4) * 4) + 1, 0x01);
			/* rx path */
			/* S/T -> PCM */
			HFC_outb(hc, R_FIFO, (ch << 1));
			HFC_wait(hc);
			HFC_outb(hc, A_CON_HDLC, 0xc0 | V_HDLC_TRP | V_IFF);
			HFC_outb(hc, R_SLOT, (((ch / 4) * 8) +
			    ((ch % 4) * 4) + 1) << 1);
			HFC_outb(hc, A_SL_CFG, 0x80 | (ch << 1));

			/* PCM -> FIFO */
			HFC_outb(hc, R_FIFO, 0x20 | (ch << 1) | 1);
			HFC_wait(hc);
			HFC_outb(hc, A_CON_HDLC, 0x20 | V_HDLC_TRP | V_IFF);
			HFC_outb(hc, A_SUBCH_CFG, 0);
			HFC_outb(hc, A_IRQ_MSK, 0);
			HFC_outb(hc, R_INC_RES_FIFO, V_RES_F);
			HFC_wait(hc);
			HFC_outb(hc, R_SLOT, ((((ch / 4) * 8) +
			    ((ch % 4) * 4) + 1) << 1) | 1);
			HFC_outb(hc, A_SL_CFG, 0x80 | 0x20 | (ch << 1) | 1);

			/* tx path */
			/* PCM -> S/T */
			HFC_outb(hc, R_FIFO, (ch << 1) | 1);
			HFC_wait(hc);
			HFC_outb(hc, A_CON_HDLC, 0xc0 | V_HDLC_TRP | V_IFF);
			HFC_outb(hc, R_SLOT, ((((ch / 4) * 8) +
			    ((ch % 4) * 4)) << 1) | 1);
			HFC_outb(hc, A_SL_CFG, 0x80 | 0x40 | (ch << 1) | 1);

			/* FIFO -> PCM */
			HFC_outb(hc, R_FIFO, 0x20 | (ch << 1));
			HFC_wait(hc);
			HFC_outb(hc, A_CON_HDLC, 0x20 | V_HDLC_TRP | V_IFF);
			HFC_outb(hc, A_SUBCH_CFG, 0);
			HFC_outb(hc, A_IRQ_MSK, 0);
			HFC_outb(hc, R_INC_RES_FIFO, V_RES_F);
			HFC_wait(hc);
			/* tx silence */
			HFC_outb_nodebug(hc, A_FIFO_DATA0_NOINC, hc->silence);
			HFC_outb(hc, R_SLOT, (((ch / 4) * 8) +
			    ((ch % 4) * 4)) << 1);
			HFC_outb(hc, A_SL_CFG, 0x80 | 0x20 | (ch << 1));
		} else {
			/* enable TX fifo */
			HFC_outb(hc, R_FIFO, ch << 1);
			HFC_wait(hc);
			if (hc->ctype == HFC_TYPE_XHFC)
				HFC_outb(hc, A_CON_HDLC, flow_tx | 0x07 << 2 |
					V_HDLC_TRP | V_IFF);
					/* Enable FIFO, no interrupt */
			else
				HFC_outb(hc, A_CON_HDLC, flow_tx | 0x00 |
					V_HDLC_TRP | V_IFF);
			HFC_outb(hc, A_SUBCH_CFG, 0);
			HFC_outb(hc, A_IRQ_MSK, 0);
			HFC_outb(hc, R_INC_RES_FIFO, V_RES_F);
			HFC_wait(hc);
			/* tx silence */
			HFC_outb_nodebug(hc, A_FIFO_DATA0_NOINC, hc->silence);
			/* enable RX fifo */
			HFC_outb(hc, R_FIFO, (ch<<1)|1);
			HFC_wait(hc);
			if (hc->ctype == HFC_TYPE_XHFC)
				HFC_outb(hc, A_CON_HDLC, flow_rx | 0x07 << 2 |
					V_HDLC_TRP);
					/* Enable FIFO, no interrupt*/
			else
				HFC_outb(hc, A_CON_HDLC, flow_rx | 0x00 |
						V_HDLC_TRP);
			HFC_outb(hc, A_SUBCH_CFG, 0);
			HFC_outb(hc, A_IRQ_MSK, 0);
			HFC_outb(hc, R_INC_RES_FIFO, V_RES_F);
			HFC_wait(hc);
		}
		if (hc->ctype != HFC_TYPE_E1) {
			hc->hw.a_st_ctrl0[hc->chan[ch].port] |=
			    ((ch & 0x3) == 0) ? V_B1_EN : V_B2_EN;
			HFC_outb(hc, R_ST_SEL, hc->chan[ch].port);
			/* undocumented: delay after R_ST_SEL */
			udelay(1);
			HFC_outb(hc, A_ST_CTRL0,
			    hc->hw.a_st_ctrl0[hc->chan[ch].port]);
		}
		if (hc->chan[ch].bch)
			test_and_set_bit(FLG_TRANSPARENT,
			    &hc->chan[ch].bch->Flags);
		break;
	case (ISDN_P_B_HDLC): /* B-channel */
	case (ISDN_P_TE_S0): /* D-channel */
	case (ISDN_P_NT_S0):
	case (ISDN_P_TE_E1):
	case (ISDN_P_NT_E1):
		/* enable TX fifo */
		HFC_outb(hc, R_FIFO, ch<<1);
		HFC_wait(hc);
		if (hc->ctype == HFC_TYPE_E1 || hc->chan[ch].bch) {
			/* E1 or B-channel */
			HFC_outb(hc, A_CON_HDLC, flow_tx | 0x04);
			HFC_outb(hc, A_SUBCH_CFG, 0);
		} else {
			/* D-Channel without HDLC fill flags */
			HFC_outb(hc, A_CON_HDLC, flow_tx | 0x04 | V_IFF);
			HFC_outb(hc, A_SUBCH_CFG, 2);
		}
		HFC_outb(hc, A_IRQ_MSK, V_IRQ);
		HFC_outb(hc, R_INC_RES_FIFO, V_RES_F);
		HFC_wait(hc);
		/* enable RX fifo */
		HFC_outb(hc, R_FIFO, (ch<<1)|1);
		HFC_wait(hc);
		HFC_outb(hc, A_CON_HDLC, flow_rx | 0x04);
		if (hc->ctype == HFC_TYPE_E1 || hc->chan[ch].bch)
			HFC_outb(hc, A_SUBCH_CFG, 0); /* full 8 bits */
		else
			HFC_outb(hc, A_SUBCH_CFG, 2); /* 2 bits dchannel */
		HFC_outb(hc, A_IRQ_MSK, V_IRQ);
		HFC_outb(hc, R_INC_RES_FIFO, V_RES_F);
		HFC_wait(hc);
		if (hc->chan[ch].bch) {
			test_and_set_bit(FLG_HDLC, &hc->chan[ch].bch->Flags);
			if (hc->ctype != HFC_TYPE_E1) {
				hc->hw.a_st_ctrl0[hc->chan[ch].port] |=
				  ((ch&0x3) == 0) ? V_B1_EN : V_B2_EN;
				HFC_outb(hc, R_ST_SEL, hc->chan[ch].port);
				/* undocumented: delay after R_ST_SEL */
				udelay(1);
				HFC_outb(hc, A_ST_CTRL0,
				  hc->hw.a_st_ctrl0[hc->chan[ch].port]);
			}
		}
		break;
	default:
		printk(KERN_DEBUG "%s: protocol not known %x\n",
		    __func__, protocol);
		hc->chan[ch].protocol = ISDN_P_NONE;
		return -ENOPROTOOPT;
	}
	hc->chan[ch].protocol = protocol;
	return 0;
}


/*
 * connect/disconnect PCM
 */

static void
hfcmulti_pcm(struct hfc_multi *hc, int ch, int slot_tx, int bank_tx,
    int slot_rx, int bank_rx)
{
	if (slot_rx < 0 || slot_rx < 0 || bank_tx < 0 || bank_rx < 0) {
		/* disable PCM */
		mode_hfcmulti(hc, ch, hc->chan[ch].protocol, -1, 0, -1, 0);
		return;
	}

	/* enable pcm */
	mode_hfcmulti(hc, ch, hc->chan[ch].protocol, slot_tx, bank_tx,
		slot_rx, bank_rx);
}

/*
 * set/disable conference
 */

static void
hfcmulti_conf(struct hfc_multi *hc, int ch, int num)
{
	if (num >= 0 && num <= 7)
		hc->chan[ch].conf = num;
	else
		hc->chan[ch].conf = -1;
	mode_hfcmulti(hc, ch, hc->chan[ch].protocol, hc->chan[ch].slot_tx,
	    hc->chan[ch].bank_tx, hc->chan[ch].slot_rx,
	    hc->chan[ch].bank_rx);
}


/*
 * set/disable sample loop
 */

/* NOTE: this function is experimental and therefore disabled */

/*
 * Layer 1 callback function
 */
static int
hfcm_l1callback(struct dchannel *dch, u_int cmd)
{
	struct hfc_multi	*hc = dch->hw;
	u_long	flags;

	switch (cmd) {
	case INFO3_P8:
	case INFO3_P10:
		break;
	case HW_RESET_REQ:
		/* start activation */
		spin_lock_irqsave(&hc->lock, flags);
		if (hc->ctype == HFC_TYPE_E1) {
			if (debug & DEBUG_HFCMULTI_MSG)
				printk(KERN_DEBUG
				    "%s: HW_RESET_REQ no BRI\n",
				    __func__);
		} else {
			HFC_outb(hc, R_ST_SEL, hc->chan[dch->slot].port);
			/* undocumented: delay after R_ST_SEL */
			udelay(1);
			HFC_outb(hc, A_ST_WR_STATE, V_ST_LD_STA | 3); /* F3 */
			udelay(6); /* wait at least 5,21us */
			HFC_outb(hc, A_ST_WR_STATE, 3);
			HFC_outb(hc, A_ST_WR_STATE, 3 | (V_ST_ACT*3));
				/* activate */
		}
		spin_unlock_irqrestore(&hc->lock, flags);
		l1_event(dch->l1, HW_POWERUP_IND);
		break;
	case HW_DEACT_REQ:
		/* start deactivation */
		spin_lock_irqsave(&hc->lock, flags);
		if (hc->ctype == HFC_TYPE_E1) {
			if (debug & DEBUG_HFCMULTI_MSG)
				printk(KERN_DEBUG
				    "%s: HW_DEACT_REQ no BRI\n",
				    __func__);
		} else {
			HFC_outb(hc, R_ST_SEL, hc->chan[dch->slot].port);
			/* undocumented: delay after R_ST_SEL */
			udelay(1);
			HFC_outb(hc, A_ST_WR_STATE, V_ST_ACT*2);
				/* deactivate */
			if (test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
				hc->syncronized &=
				   ~(1 << hc->chan[dch->slot].port);
				plxsd_checksync(hc, 0);
			}
		}
		skb_queue_purge(&dch->squeue);
		if (dch->tx_skb) {
			dev_kfree_skb(dch->tx_skb);
			dch->tx_skb = NULL;
		}
		dch->tx_idx = 0;
		if (dch->rx_skb) {
			dev_kfree_skb(dch->rx_skb);
			dch->rx_skb = NULL;
		}
		test_and_clear_bit(FLG_TX_BUSY, &dch->Flags);
		if (test_and_clear_bit(FLG_BUSY_TIMER, &dch->Flags))
			del_timer(&dch->timer);
		spin_unlock_irqrestore(&hc->lock, flags);
		break;
	case HW_POWERUP_REQ:
		spin_lock_irqsave(&hc->lock, flags);
		if (hc->ctype == HFC_TYPE_E1) {
			if (debug & DEBUG_HFCMULTI_MSG)
				printk(KERN_DEBUG
				    "%s: HW_POWERUP_REQ no BRI\n",
				    __func__);
		} else {
			HFC_outb(hc, R_ST_SEL, hc->chan[dch->slot].port);
			/* undocumented: delay after R_ST_SEL */
			udelay(1);
			HFC_outb(hc, A_ST_WR_STATE, 3 | 0x10); /* activate */
			udelay(6); /* wait at least 5,21us */
			HFC_outb(hc, A_ST_WR_STATE, 3); /* activate */
		}
		spin_unlock_irqrestore(&hc->lock, flags);
		break;
	case PH_ACTIVATE_IND:
		test_and_set_bit(FLG_ACTIVE, &dch->Flags);
		_queue_data(&dch->dev.D, cmd, MISDN_ID_ANY, 0, NULL,
			GFP_ATOMIC);
		break;
	case PH_DEACTIVATE_IND:
		test_and_clear_bit(FLG_ACTIVE, &dch->Flags);
		_queue_data(&dch->dev.D, cmd, MISDN_ID_ANY, 0, NULL,
			GFP_ATOMIC);
		break;
	default:
		if (dch->debug & DEBUG_HW)
			printk(KERN_DEBUG "%s: unknown command %x\n",
			    __func__, cmd);
		return -1;
	}
	return 0;
}

/*
 * Layer2 -> Layer 1 Transfer
 */

static int
handle_dmsg(struct mISDNchannel *ch, struct sk_buff *skb)
{
	struct mISDNdevice	*dev = container_of(ch, struct mISDNdevice, D);
	struct dchannel		*dch = container_of(dev, struct dchannel, dev);
	struct hfc_multi	*hc = dch->hw;
	struct mISDNhead	*hh = mISDN_HEAD_P(skb);
	int			ret = -EINVAL;
	unsigned int		id;
	u_long			flags;

	switch (hh->prim) {
	case PH_DATA_REQ:
		if (skb->len < 1)
			break;
		spin_lock_irqsave(&hc->lock, flags);
		ret = dchannel_senddata(dch, skb);
		if (ret > 0) { /* direct TX */
			id = hh->id; /* skb can be freed */
			hfcmulti_tx(hc, dch->slot);
			ret = 0;
			/* start fifo */
			HFC_outb(hc, R_FIFO, 0);
			HFC_wait(hc);
			spin_unlock_irqrestore(&hc->lock, flags);
			queue_ch_frame(ch, PH_DATA_CNF, id, NULL);
		} else
			spin_unlock_irqrestore(&hc->lock, flags);
		return ret;
	case PH_ACTIVATE_REQ:
		if (dch->dev.D.protocol != ISDN_P_TE_S0) {
			spin_lock_irqsave(&hc->lock, flags);
			ret = 0;
			if (debug & DEBUG_HFCMULTI_MSG)
				printk(KERN_DEBUG
				    "%s: PH_ACTIVATE port %d (0..%d)\n",
				    __func__, hc->chan[dch->slot].port,
				    hc->ports-1);
			/* start activation */
			if (hc->ctype == HFC_TYPE_E1) {
				ph_state_change(dch);
				if (debug & DEBUG_HFCMULTI_STATE)
					printk(KERN_DEBUG
					    "%s: E1 report state %x \n",
					    __func__, dch->state);
			} else {
				HFC_outb(hc, R_ST_SEL,
				    hc->chan[dch->slot].port);
				/* undocumented: delay after R_ST_SEL */
				udelay(1);
				HFC_outb(hc, A_ST_WR_STATE, V_ST_LD_STA | 1);
				    /* G1 */
				udelay(6); /* wait at least 5,21us */
				HFC_outb(hc, A_ST_WR_STATE, 1);
				HFC_outb(hc, A_ST_WR_STATE, 1 |
				    (V_ST_ACT*3)); /* activate */
				dch->state = 1;
			}
			spin_unlock_irqrestore(&hc->lock, flags);
		} else
			ret = l1_event(dch->l1, hh->prim);
		break;
	case PH_DEACTIVATE_REQ:
		test_and_clear_bit(FLG_L2_ACTIVATED, &dch->Flags);
		if (dch->dev.D.protocol != ISDN_P_TE_S0) {
			spin_lock_irqsave(&hc->lock, flags);
			if (debug & DEBUG_HFCMULTI_MSG)
				printk(KERN_DEBUG
				    "%s: PH_DEACTIVATE port %d (0..%d)\n",
				    __func__, hc->chan[dch->slot].port,
				    hc->ports-1);
			/* start deactivation */
			if (hc->ctype == HFC_TYPE_E1) {
				if (debug & DEBUG_HFCMULTI_MSG)
					printk(KERN_DEBUG
					    "%s: PH_DEACTIVATE no BRI\n",
					    __func__);
			} else {
				HFC_outb(hc, R_ST_SEL,
				    hc->chan[dch->slot].port);
				/* undocumented: delay after R_ST_SEL */
				udelay(1);
				HFC_outb(hc, A_ST_WR_STATE, V_ST_ACT * 2);
				    /* deactivate */
				dch->state = 1;
			}
			skb_queue_purge(&dch->squeue);
			if (dch->tx_skb) {
				dev_kfree_skb(dch->tx_skb);
				dch->tx_skb = NULL;
			}
			dch->tx_idx = 0;
			if (dch->rx_skb) {
				dev_kfree_skb(dch->rx_skb);
				dch->rx_skb = NULL;
			}
			test_and_clear_bit(FLG_TX_BUSY, &dch->Flags);
			if (test_and_clear_bit(FLG_BUSY_TIMER, &dch->Flags))
				del_timer(&dch->timer);
#ifdef FIXME
			if (test_and_clear_bit(FLG_L1_BUSY, &dch->Flags))
				dchannel_sched_event(&hc->dch, D_CLEARBUSY);
#endif
			ret = 0;
			spin_unlock_irqrestore(&hc->lock, flags);
		} else
			ret = l1_event(dch->l1, hh->prim);
		break;
	}
	if (!ret)
		dev_kfree_skb(skb);
	return ret;
}

static void
deactivate_bchannel(struct bchannel *bch)
{
	struct hfc_multi	*hc = bch->hw;
	u_long			flags;

	spin_lock_irqsave(&hc->lock, flags);
	mISDN_clear_bchannel(bch);
	hc->chan[bch->slot].coeff_count = 0;
	hc->chan[bch->slot].rx_off = 0;
	hc->chan[bch->slot].conf = -1;
	mode_hfcmulti(hc, bch->slot, ISDN_P_NONE, -1, 0, -1, 0);
	spin_unlock_irqrestore(&hc->lock, flags);
}

static int
handle_bmsg(struct mISDNchannel *ch, struct sk_buff *skb)
{
	struct bchannel		*bch = container_of(ch, struct bchannel, ch);
	struct hfc_multi	*hc = bch->hw;
	int			ret = -EINVAL;
	struct mISDNhead	*hh = mISDN_HEAD_P(skb);
	unsigned int		id;
	u_long			flags;

	switch (hh->prim) {
	case PH_DATA_REQ:
		if (!skb->len)
			break;
		spin_lock_irqsave(&hc->lock, flags);
		ret = bchannel_senddata(bch, skb);
		if (ret > 0) { /* direct TX */
			id = hh->id; /* skb can be freed */
			hfcmulti_tx(hc, bch->slot);
			ret = 0;
			/* start fifo */
			HFC_outb_nodebug(hc, R_FIFO, 0);
			HFC_wait_nodebug(hc);
			if (!test_bit(FLG_TRANSPARENT, &bch->Flags)) {
				spin_unlock_irqrestore(&hc->lock, flags);
				queue_ch_frame(ch, PH_DATA_CNF, id, NULL);
			} else
				spin_unlock_irqrestore(&hc->lock, flags);
		} else
			spin_unlock_irqrestore(&hc->lock, flags);
		return ret;
	case PH_ACTIVATE_REQ:
		if (debug & DEBUG_HFCMULTI_MSG)
			printk(KERN_DEBUG "%s: PH_ACTIVATE ch %d (0..32)\n",
				__func__, bch->slot);
		spin_lock_irqsave(&hc->lock, flags);
		/* activate B-channel if not already activated */
		if (!test_and_set_bit(FLG_ACTIVE, &bch->Flags)) {
			hc->chan[bch->slot].txpending = 0;
			ret = mode_hfcmulti(hc, bch->slot,
				ch->protocol,
				hc->chan[bch->slot].slot_tx,
				hc->chan[bch->slot].bank_tx,
				hc->chan[bch->slot].slot_rx,
				hc->chan[bch->slot].bank_rx);
			if (!ret) {
				if (ch->protocol == ISDN_P_B_RAW && !hc->dtmf
					&& test_bit(HFC_CHIP_DTMF, &hc->chip)) {
					/* start decoder */
					hc->dtmf = 1;
					if (debug & DEBUG_HFCMULTI_DTMF)
						printk(KERN_DEBUG
						    "%s: start dtmf decoder\n",
							__func__);
					HFC_outb(hc, R_DTMF, hc->hw.r_dtmf |
					    V_RST_DTMF);
				}
			}
		} else
			ret = 0;
		spin_unlock_irqrestore(&hc->lock, flags);
		if (!ret)
			_queue_data(ch, PH_ACTIVATE_IND, MISDN_ID_ANY, 0, NULL,
				GFP_KERNEL);
		break;
	case PH_CONTROL_REQ:
		spin_lock_irqsave(&hc->lock, flags);
		switch (hh->id) {
		case HFC_SPL_LOOP_ON: /* set sample loop */
			if (debug & DEBUG_HFCMULTI_MSG)
				printk(KERN_DEBUG
				    "%s: HFC_SPL_LOOP_ON (len = %d)\n",
				    __func__, skb->len);
			ret = 0;
			break;
		case HFC_SPL_LOOP_OFF: /* set silence */
			if (debug & DEBUG_HFCMULTI_MSG)
				printk(KERN_DEBUG "%s: HFC_SPL_LOOP_OFF\n",
				    __func__);
			ret = 0;
			break;
		default:
			printk(KERN_ERR
			     "%s: unknown PH_CONTROL_REQ info %x\n",
			     __func__, hh->id);
			ret = -EINVAL;
		}
		spin_unlock_irqrestore(&hc->lock, flags);
		break;
	case PH_DEACTIVATE_REQ:
		deactivate_bchannel(bch); /* locked there */
		_queue_data(ch, PH_DEACTIVATE_IND, MISDN_ID_ANY, 0, NULL,
			GFP_KERNEL);
		ret = 0;
		break;
	}
	if (!ret)
		dev_kfree_skb(skb);
	return ret;
}

/*
 * bchannel control function
 */
static int
channel_bctrl(struct bchannel *bch, struct mISDN_ctrl_req *cq)
{
	int			ret = 0;
	struct dsp_features	*features =
		(struct dsp_features *)(*((u_long *)&cq->p1));
	struct hfc_multi	*hc = bch->hw;
	int			slot_tx;
	int			bank_tx;
	int			slot_rx;
	int			bank_rx;
	int			num;

	switch (cq->op) {
	case MISDN_CTRL_GETOP:
		cq->op = MISDN_CTRL_HFC_OP | MISDN_CTRL_HW_FEATURES_OP
			| MISDN_CTRL_RX_OFF | MISDN_CTRL_FILL_EMPTY;
		break;
	case MISDN_CTRL_RX_OFF: /* turn off / on rx stream */
		hc->chan[bch->slot].rx_off = !!cq->p1;
		if (!hc->chan[bch->slot].rx_off) {
			/* reset fifo on rx on */
			HFC_outb_nodebug(hc, R_FIFO, (bch->slot << 1) | 1);
			HFC_wait_nodebug(hc);
			HFC_outb_nodebug(hc, R_INC_RES_FIFO, V_RES_F);
			HFC_wait_nodebug(hc);
		}
		if (debug & DEBUG_HFCMULTI_MSG)
			printk(KERN_DEBUG "%s: RX_OFF request (nr=%d off=%d)\n",
			    __func__, bch->nr, hc->chan[bch->slot].rx_off);
		break;
	case MISDN_CTRL_FILL_EMPTY: /* fill fifo, if empty */
		test_and_set_bit(FLG_FILLEMPTY, &bch->Flags);
		if (debug & DEBUG_HFCMULTI_MSG)
			printk(KERN_DEBUG "%s: FILL_EMPTY request (nr=%d "
				"off=%d)\n", __func__, bch->nr, !!cq->p1);
		break;
	case MISDN_CTRL_HW_FEATURES: /* fill features structure */
		if (debug & DEBUG_HFCMULTI_MSG)
			printk(KERN_DEBUG "%s: HW_FEATURE request\n",
			    __func__);
		/* create confirm */
		features->hfc_id = hc->id;
		if (test_bit(HFC_CHIP_DTMF, &hc->chip))
			features->hfc_dtmf = 1;
		if (test_bit(HFC_CHIP_CONF, &hc->chip))
			features->hfc_conf = 1;
		features->hfc_loops = 0;
		if (test_bit(HFC_CHIP_B410P, &hc->chip)) {
			features->hfc_echocanhw = 1;
		} else {
			features->pcm_id = hc->pcm;
			features->pcm_slots = hc->slots;
			features->pcm_banks = 2;
		}
		break;
	case MISDN_CTRL_HFC_PCM_CONN: /* connect to pcm timeslot (0..N) */
		slot_tx = cq->p1 & 0xff;
		bank_tx = cq->p1 >> 8;
		slot_rx = cq->p2 & 0xff;
		bank_rx = cq->p2 >> 8;
		if (debug & DEBUG_HFCMULTI_MSG)
			printk(KERN_DEBUG
			    "%s: HFC_PCM_CONN slot %d bank %d (TX) "
			    "slot %d bank %d (RX)\n",
			    __func__, slot_tx, bank_tx,
			    slot_rx, bank_rx);
		if (slot_tx < hc->slots && bank_tx <= 2 &&
		    slot_rx < hc->slots && bank_rx <= 2)
			hfcmulti_pcm(hc, bch->slot,
			    slot_tx, bank_tx, slot_rx, bank_rx);
		else {
			printk(KERN_WARNING
			    "%s: HFC_PCM_CONN slot %d bank %d (TX) "
			    "slot %d bank %d (RX) out of range\n",
			    __func__, slot_tx, bank_tx,
			    slot_rx, bank_rx);
			ret = -EINVAL;
		}
		break;
	case MISDN_CTRL_HFC_PCM_DISC: /* release interface from pcm timeslot */
		if (debug & DEBUG_HFCMULTI_MSG)
			printk(KERN_DEBUG "%s: HFC_PCM_DISC\n",
			    __func__);
		hfcmulti_pcm(hc, bch->slot, -1, 0, -1, 0);
		break;
	case MISDN_CTRL_HFC_CONF_JOIN: /* join conference (0..7) */
		num = cq->p1 & 0xff;
		if (debug & DEBUG_HFCMULTI_MSG)
			printk(KERN_DEBUG "%s: HFC_CONF_JOIN conf %d\n",
			    __func__, num);
		if (num <= 7)
			hfcmulti_conf(hc, bch->slot, num);
		else {
			printk(KERN_WARNING
			    "%s: HW_CONF_JOIN conf %d out of range\n",
			    __func__, num);
			ret = -EINVAL;
		}
		break;
	case MISDN_CTRL_HFC_CONF_SPLIT: /* split conference */
		if (debug & DEBUG_HFCMULTI_MSG)
			printk(KERN_DEBUG "%s: HFC_CONF_SPLIT\n", __func__);
		hfcmulti_conf(hc, bch->slot, -1);
		break;
	case MISDN_CTRL_HFC_ECHOCAN_ON:
		if (debug & DEBUG_HFCMULTI_MSG)
			printk(KERN_DEBUG "%s: HFC_ECHOCAN_ON\n", __func__);
		if (test_bit(HFC_CHIP_B410P, &hc->chip))
			vpm_echocan_on(hc, bch->slot, cq->p1);
		else
			ret = -EINVAL;
		break;

	case MISDN_CTRL_HFC_ECHOCAN_OFF:
		if (debug & DEBUG_HFCMULTI_MSG)
			printk(KERN_DEBUG "%s: HFC_ECHOCAN_OFF\n",
				__func__);
		if (test_bit(HFC_CHIP_B410P, &hc->chip))
			vpm_echocan_off(hc, bch->slot);
		else
			ret = -EINVAL;
		break;
	default:
		printk(KERN_WARNING "%s: unknown Op %x\n",
		    __func__, cq->op);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int
hfcm_bctrl(struct mISDNchannel *ch, u_int cmd, void *arg)
{
	struct bchannel		*bch = container_of(ch, struct bchannel, ch);
	struct hfc_multi	*hc = bch->hw;
	int			err = -EINVAL;
	u_long	flags;

	if (bch->debug & DEBUG_HW)
		printk(KERN_DEBUG "%s: cmd:%x %p\n",
		    __func__, cmd, arg);
	switch (cmd) {
	case CLOSE_CHANNEL:
		test_and_clear_bit(FLG_OPEN, &bch->Flags);
		if (test_bit(FLG_ACTIVE, &bch->Flags))
			deactivate_bchannel(bch); /* locked there */
		ch->protocol = ISDN_P_NONE;
		ch->peer = NULL;
		module_put(THIS_MODULE);
		err = 0;
		break;
	case CONTROL_CHANNEL:
		spin_lock_irqsave(&hc->lock, flags);
		err = channel_bctrl(bch, arg);
		spin_unlock_irqrestore(&hc->lock, flags);
		break;
	default:
		printk(KERN_WARNING "%s: unknown prim(%x)\n",
			__func__, cmd);
	}
	return err;
}

/*
 * handle D-channel events
 *
 * handle state change event
 */
static void
ph_state_change(struct dchannel *dch)
{
	struct hfc_multi *hc;
	int ch, i;

	if (!dch) {
		printk(KERN_WARNING "%s: ERROR given dch is NULL\n", __func__);
		return;
	}
	hc = dch->hw;
	ch = dch->slot;

	if (hc->ctype == HFC_TYPE_E1) {
		if (dch->dev.D.protocol == ISDN_P_TE_E1) {
			if (debug & DEBUG_HFCMULTI_STATE)
				printk(KERN_DEBUG
				    "%s: E1 TE (id=%d) newstate %x\n",
				    __func__, hc->id, dch->state);
		} else {
			if (debug & DEBUG_HFCMULTI_STATE)
				printk(KERN_DEBUG
				    "%s: E1 NT (id=%d) newstate %x\n",
				    __func__, hc->id, dch->state);
		}
		switch (dch->state) {
		case (1):
			if (hc->e1_state != 1) {
				for (i = 1; i <= 31; i++) {
					/* reset fifos on e1 activation */
					HFC_outb_nodebug(hc, R_FIFO,
						(i << 1) | 1);
					HFC_wait_nodebug(hc);
					HFC_outb_nodebug(hc, R_INC_RES_FIFO,
						V_RES_F);
					HFC_wait_nodebug(hc);
				}
			}
			test_and_set_bit(FLG_ACTIVE, &dch->Flags);
			_queue_data(&dch->dev.D, PH_ACTIVATE_IND,
			    MISDN_ID_ANY, 0, NULL, GFP_ATOMIC);
			break;

		default:
			if (hc->e1_state != 1)
				return;
			test_and_clear_bit(FLG_ACTIVE, &dch->Flags);
			_queue_data(&dch->dev.D, PH_DEACTIVATE_IND,
			    MISDN_ID_ANY, 0, NULL, GFP_ATOMIC);
		}
		hc->e1_state = dch->state;
	} else {
		if (dch->dev.D.protocol == ISDN_P_TE_S0) {
			if (debug & DEBUG_HFCMULTI_STATE)
				printk(KERN_DEBUG
				    "%s: S/T TE newstate %x\n",
				    __func__, dch->state);
			switch (dch->state) {
			case (0):
				l1_event(dch->l1, HW_RESET_IND);
				break;
			case (3):
				l1_event(dch->l1, HW_DEACT_IND);
				break;
			case (5):
			case (8):
				l1_event(dch->l1, ANYSIGNAL);
				break;
			case (6):
				l1_event(dch->l1, INFO2);
				break;
			case (7):
				l1_event(dch->l1, INFO4_P8);
				break;
			}
		} else {
			if (debug & DEBUG_HFCMULTI_STATE)
				printk(KERN_DEBUG "%s: S/T NT newstate %x\n",
				    __func__, dch->state);
			switch (dch->state) {
			case (2):
				if (hc->chan[ch].nt_timer == 0) {
					hc->chan[ch].nt_timer = -1;
					HFC_outb(hc, R_ST_SEL,
					    hc->chan[ch].port);
					/* undocumented: delay after R_ST_SEL */
					udelay(1);
					HFC_outb(hc, A_ST_WR_STATE, 4 |
					    V_ST_LD_STA); /* G4 */
					udelay(6); /* wait at least 5,21us */
					HFC_outb(hc, A_ST_WR_STATE, 4);
					dch->state = 4;
				} else {hfcmu	/* one extra count for the next event */ow levhc->chan[ch].nt_timer =ow lev    	And1_ hfc-[pollndreas] + 1 hfcmu	HFC_outb(hc, R_ST_SEL,versberg (
 *
 * Authorport)ed to m/* undocumented: delay afterism:
 *		sed cardsued by(1ng-bytes.deallow G2 -> G3 transitionsed cardsqueue mechaniAm:
 Wsm:
ATE, 2 |versberg (V_SET_G2_G3ng-byte}hfcmubreak hfcmcase (1):hfcmu
 *
 * Author	Andreas E -ted to test_and_clear_bit(FLG_ACTIVE, &dch->Flagsng-byte_queue_data(ree sodev.D, PH_DEram iATE_INDPeter rg (MISDN_ID_ANY, 0, NULL, GFP_ATOMICng-bytee)
 * Copyright 4008  by Andreas Eversberg (jolly@eversbe)
 * Copyright 3008  by Andreas Eversberg (jolly@eversberg.eu)
 setThis program is free software; you can redistribute it and/or mify
 * it under the terms of the GNU General Public License as publishedsuse}
	}
}

/*
 * called4s/hfcard mode init message
sed 
static void
hfcmulti_  SePOSE(struct d * Anel *dch)
{
	 detailhfc_icens *hc =ls.
->hw;
	u_char		a_st_wr_enere, r_e1lic Lic;
	int		i, pt;

	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_te to "%s:  insred\n", __func__)ram; if 
 *
 type == queuTYPE_E1) low 
 *
 * Au
 *
dslot]. for_txolly@everologne Chip AG for this great controller!
 */

/*
 * modconfolly@everA 02139,G fors to C	POSE_c Licenschaniis automa, of tht and.protocolPeter -1NU Get 0-7ng-bytof thdreas.ambr (wer= (Publ *) recicensedbusyndreas0001 = HFC-E1 (1istr* orlong)ls.
 hfcm  Sendreasibute idreasng-bysuses/hf(i = 1; i <= 30 = ++tically if i
 *
is automathfcmucontinue hfcm
 *
 * Aui this great controe DTMF detect on aarameters:
e DTMF detectlt (0), the cy detected.
 *	Or ui, ms ofP_NONE   = 0x   = 0x00001suse/* E1sed ca if erg.ehis queuCFG_REPORT_LOS, & * type:
 *	By defaulfg)ticallyqueue mechanismLOS0, 255); /* 2 mssed carto)
 *	Bit 13	  = 1pare
 *	Bit51 14    = 0suse 12    = 0x01000 = FoOPTICALs into master mode. (otherwhise auto)
 *	Bit 13	 RX0 0x00001 
 *
hw.r_tx0 = 0 | V_OUT_ENe autti.c  low le 64 timeslots instepyright2
 * or	Bit 17 1  = 0x20000 = Ustroller or	Bit1 = V_ATX  = 0NTRI hfcqueue mechanismTnste2
 * or	Bit ng-by Signal (Dual E1 1ith Watchdog)pyrigh Signal (Dual E1 _FRsteax0
 * (all other bits are_FR 0-7xf8ge, M= 0x08000 = Use externCRC4s into master mode. (otherwhimeslots instead ofdetec2, Vd foMF  = 0TX_Ehe WatEG_Ed 128 e 64 timeslots in 0x202V_AUTO_RESYNC  = 0:
 *	BiCO |0x00028 timeslots on PCM
 *		 bus (PCM master)
 *
 * port: (optional or required only1bitsRrts on alransm_t 0	ster clockowing combinations:
 *
nto slavTanks to C; if not, write to the Free Softwarere
 * Foundation, Inc., 6E1 movi is NT-POSE Aveer the te Cambridge,			
 * along w17  *	BitG0sed car
 *
e1_getclock01,0x= Use 128 timesss you know what you are doing!
 *	Bit 2     = 0x004 = Disable E-channeTE (No E-channel processing)
 *	example: 0x0001,0x0000F0x0000,0x0000 one HFC-4S wted t   = 0x08000 = Use extHIPr lit 0	s into maip(optional or requiredt 0	0x20bits 3   E1_RX
 *	ei.c = Report LOS
 *	Bit 3     = 0
 * (as transparent mode)
 *E1CLOCK_GET    = 0x0004 = Reved (later for 32 B-cha*	Bit 5     = 0x0020 = Report P = 0*	Bit 8     = 0x0100 = Turn off Cith ms transparent mode)
 *PCM_SLAis fr = 0x0004  low l/* m int (HFC-4Smaster)sed carss you know what you are doing!
 *	Bit 2     = 0x004 =er the teDisable E-channer	Bit 10	  = "t 11    = or	Bit from PCM) Ave, Cambridge, Report LOS
 *	Bit 3   CTRLbitsEXT_CLK0008   = 0 frometup ( master clock
 *		x0000 one HFC-4 in NT  modMASTER or	Bit slave 0x0400  = Force put clock to interface, even  in TE mode.
 *	Bit 11     = 0x0800 = Use direct Rbits k for PPCM sync rathtohan PLL.
 *			     (E1 oonly)
 *	Bit 12-13 = 0xX000 = 3    FFS (E1 olti.c  low let.
 * (all other 10	  = 0x0400 rved and shall be 0)
 *
 * debug:
 *	NOTE: only one debu= 0x0800 = Use di	enable deburect RX clock for PPCM sync rather thQUARTZPLL.
rease to
 i.h for debug options)
 *
 * poll:
 *	NOTE: elastic jitteer the te buffer (1  = 0JATTy ondebug options)
 *
 * poll:
port SLIP
 ITY or F *	HFC-4S/HFC-8S pcm ATrt Sx9c *	Bit)
 *
 regiclocked caqueue mechanismPWM_MDbitsPWM0_MDhall be 0)
 *	example PCmodu5
 * (all other bits aPWMtion af-3), /* Licen machine setup PCM bus.
 *	By defauE1 * Copyse
 * along w  = 0E1_LDCopy-3), :
 * Co6 *	Bitwait at least 5,21u    = 0ey are not interconnected),
 *	each cLIP
 *	Bit 5     = 0x0020 =PLXSDerface, even in NT 
 *
syncronized9	  = 0x	plxsd_check*	Onchaniise auto)se 128 time0010of th forntroller!
 */ect on all B-channes via hardware
 *	Bit 10     spare
 *	Bit 11    = 0x0800 = Force PCM bus iowing combinations:
de. (otherwhise au= HFC-E1 (1 port)
 * or	Bit 0-   = 0x00004 = HFC-4S (4ports)
 * or	Bit 0-7   = 0x00008= HFC-8S (8 ports)
 *	Bit 8no support - 2 this great controller!
 */1 cards. If arameters:
 * type:
1 cards interconnected or none is conne car into slave mode. (otherwhise aume slot 16, w1ds. If omitted, the E1
 *	card .
 *	In annel on time slot 16, w1ich is default.
 *	If 1..15 or 17..311 into slave mode. (otherwhise aumodeT PCM bpt 	Bit 9upport omovicards arelect i5 Maface PCM bus.
 *	By defau:
 *		P ping-by.de)
 *
 * inspired by existing hfc-pci dri:
 * Copyrigh capacitive mode)
 *			    Don't use S0is unless you know what you are doing!
 *	Bit 2     = 0x004 =t 11    = 0x08ST E-cha%dannel. (No E-channel processing)use it, yyou HFC-4Sed by   = 0x04000 = Use ede)
 sticDLY,mode, ed by_ning-byt Public Licen0100 =000,r   Bitlse
 *	B Publctrl0[pt]= SenST_MDth master clock
 *		 received from port 1
 *
 *	HFC-E1 only bits:
 *	B IO mode is pci memory IO (M = interface: 0=copper, 1=opspecific IO mode, so it cannot be changed.
 *	It may be usefull to seteIO mode to register io20x00022 to solve
 *	PCI bridge problemith mae value!   = 0x01000 = FoNONCAP_TXs into maste	Bit: (optiove
 *	PCI bridge probl| SenTX_Ldog aA 02139, USA.
 *
 *
 * ThaXHFCts slots.
  *	NOTE: only one clo0x40REGIs.
 *PU 0xX0 */(E1 only)
 *	Bit 10x35REGI*	It mTRL3 */nsure,0x7c << 1gister (A_SLSE */se auto)
 * lter (because they are not ins in TE m0,enger *	PCI bridge probit, you disable E-.
 *
 * Y Bit 12 e must be given for every card.
 *	-> | * prg (   = 0x01000 = FoDIS_ECHANNo us it.
 *
 * clockdelay_on't change it.
 *
 * 2 = tE_IGNOrt AIS
 *	Bit 4     = 0x00o disable.
  be used
en mustBbe given receiv *	If you don't knows in TE m2, *	NBRepo_EN  = 0B2* 	Ena cards are PCM master (because they are not inde)
 * Copyrighe to register  = 0STh PCM master will have increasing PCM id.
 *	All PCM busses with 
 * debug register access (nused for  or	Bsci_msk clo1egisogrards are PCM master ationruptAll PCM busses with thSCI_MSKith Watchdo
#incluit, you doset *	On on E-cha  Bit 12    = 0x01000 =ve
 *	common time slots slots.
 *	Only one c&Evers   ~(linux
 *
 * Au
 *	-1 meermoving-bythe PCM bus must be master, them; if not, write to the Free Software
 * Fouc., 6don E-ch't mess with}
 General int
open_s.
 *
 *e detailreceived a co,  details.
 *
 * You ,
rg ( detail.
 *
 *_req *rqshoul thierjollith u_7   	ftwarram; if not, write to tW_OPENware
 * Foundation, Inc., 6dev(%d) Clocher th%p Ave, Cambridnsurrg (owing comidClisbuiltin_return_address(0)-3), if rq->e)
 *			    Don't usONEopti lock  -EINVALdefine	pacitive mode)
 *			 !ine	TYP_4S		4
 &&
ck; /c int poll_timer = 6;	/*TYP_E1		1
#dn in NT if not, write to the Free MOD4
#dee
 * Foundation, Inc., 6 * Age e)
 *			 %x(see%xf unsure for all cardsed.
 *	If unsure, don POLL_TIMER i;DS	8
#defpacitive mode)
 *			    Don't uTE*	-> 8 samplesTYP_E1		1
#de	/* defaulL in Noptil1_e1 bapacitil1, CLOSE_ to car#define	 = 16ms */
/* number of POLL_TIMER iterrupts TYP_E1		1
#define	TYP_4L in NTlow lruct hcreate_l1r Be,7   =_l1A PAbacDNhw.hupts err= 0x00fine TYer4S (4e TBacitive mode)
 *			  /* CLKDEL in odulspin_FC-4_irqsave(face,FC-4, yncmaused f License for moreou spe[MAX_CAunRDS];
strestorc int	pcm[MAX_CARDS];
}am; if (	TYP_E1		1
#define	TYP_4S *	-> &&or Beroister i= 3)or mISmples2		/* DIP Switches for Beronet atic uint	poll;
st7tic int	clock;
static uint	timer;
stse thisatic uint	poll;
st1tic int	clock;
static uint	timer;
statiCLKDEL_NT;
#define HWID_Nt 8S+ t will be useful,
 * but WITHOUT ANY WARRerms of the GNlock; / General Publndatitches}
	TYP_ch =free sot anddefine	!try_moduleone (THIS */
ULE */


 * FoundatiWARNINt 128 cannot getRPOSulD(HFClist);
statit E1 car0tic sinlock_t HFClockb /* global hfc list lock */

static void ph_state_change(struct dchannel *);

stati detailpoll, ui	*bx0000 thisx000berg");
rg.e.
 *
 *map2		/*adr..
 *
 *define	CLKDe_param(cl */

fine TYP_8S		8

statiTYP_E1		1
#define	TYP_4S		4
#define TYP_8S		8

stati139, USA.
 *
 *
 * Thanks
 | S
staticlay_te, uin;
IS
 *	BiWUSR)ockdelay_te, uin dis) +ic uint	lot carS_IRbWUSR)
 *
 * Author, S_IRUg");bcht 8S+ 
 * FoundatiERR= 0x0ationnalrds ULARhy IOhas notimef unsur for all cards_CARDS]fine TYP_8S		8

s	8
#def This program is distster, &be software
module_paramBUSY(REGIbbe given can be only) */
son
 *	If erg.eu)
 *
 * This progFILLEMPTYram(hwid, uint_arrayTHORtuff
 */

static uint	type[
 *
 * Authorrx_of(0),ith AUTHOR("Anc->HFC_sberg");
MODULE_LICENSE("GPL");
MODULE_VERSION(HFC_MULTI_VERSION);
module_param(debug, uint, S_IRUGO | S_IWUSRS FOR device 200 rol  port)
  GNU inlock_t HF dchannedidgee details.
 *
 * You 
static vmms ofidgeel *);codule_param(treceived 	 copy of the GNU thirechanith  thiwdDULEe, wd_cngram;switch (cq->opt 8S+rightrms ofTE m RDIOP08  define =nodebug(hc, queuOPodule)
 * Coinw_nodebug(hc, queuWD Soft:REGI  Seec-8swatchdogDNdsp.nc__, s */YP_E1 &on aodulreg, __ = !!#defip1 >> 4ode value or G2 timeout (ca 1s) *SGtatic int nt_t1_count[] = { NE__))
#define HFC_waiRPOSE.%s for    = ,r hfc-er 0x240, 1ock_t plx_locoutb(ne HFC_w? ":
 *" : "MANUAL"func__,  cards are	(hc->HFC_wait(dreas l PCM busses with thTI_WD, erg.eu)
 * | (_func__<< 4
#deflse
 *	Bitbert_reg, chifinehc, reg 0x001 WD	Bit : SLIP
 *	Bie must be given once
 *	Givof 32
 * or	Bg)	(hc->HFCtrol registerWDEnabDLY)
 hc) \
	(hc->HFC_wait(onnected and rereg, val reg, val PCM busses with thBERTg)	(0), )
#define HFC_inw_nodreg)	(REe pollh>

/*
#define IRQCOUNT_DEBUG
#define IRQ_DEBU.
 *
 * hwhc->HFC_wait(outpu-4s/hfSpeech-Desigerner@isqueue mechanismGPIO to us V#ifdef HF7 (E1 only)
 *	Bit 12-ifdefEN1C_REGISTEREN15UG
HFC_outb_pcimem(struct OUTck sourcereg, u_char val,
		const chEGISTERnst _char e TB_func__, __LINE__))
#define HFCRESEit(hc)ait(hc)			HFC_wait((hc->HFC_waitc->HFC_wait_nodebug(hc, __func__, __LINE__))
#else
#define HFC_outb(hc, reg,r val0, 120, 60, 30  };
#hall be 0)
 *	examplFC_wait_nodebug(hc)		(hc->HFC_wait_nodebug(hc))e)
 * Codefault08  VERSION(HFC_MULTI_VERSIO unknown Op 240, 120,R);
module_paraefine array(polly_8S		8

sulti *hc,LE_Aine TYretIWUSR);
module_p1	0x3(hc, reg) \
	c, re.
 *
 * Yb_nou_int cmd, Publ *argshould have c, reg, __f	*devs */ontainer_of(b_nodebug(hc, reg, __f, bus i details.
 *
 *		You ti *hc, u_char rdev
static void ph_s,eg, w(hc->pci_mdefine HFC_inw(hc, reg) \
(struct dchannel *	*rq_IRUGO |truct hfc_multi *s	syncmaster;
stareas Eic int plxsd_mware
 * Foundation, Inc., 6cmd:%xc spinl reg);
}
static u_linee
HFhc, E__))
#dmd HFC_inw_nsterDIP Swit08  rq =elaxpe[MAE__))
# */
#define	DIP_8Srights for Berone\
	(hgio(struct bug;
08  bA 02139, USA.
 *
 *
 * Thanks to Cne)
#els_REGISTER_DERCHANTABILITY ocards *Clock; /* glob/

snb_norq *	BitFC-4ed_mulr *	If yersion 2,  *hc, u_chare HWIulti *hc, u_char reoutb(vhar val,
	const!char *function, int line)
#else
HFC_outb_regio(struct hfc_multi *hc, u_char reg, u_char val)
#endif
{
	outb(reg, hc->pci u_char regMAX_CARDS];
static int	pcm[MAX_CARDS];
shfc_multi *hpoll, uin reg, u_char ruct hfc_tic uint	iomode[MAX_CARDS];
static uinuct hfc_multi *hc,		/* DIP Swit08  ;
static int plxsd_master; /** if we have a master card (yet)closestatic spinltb_nodeb30  };
#define	CLKDidnw_regio(stre other lock inside */

#defted _LICpuSE("GPL");
MODg, __func__, __LICONTROLC_inw_regio(AX_CARDS];
static int	pcm[MAX_CARDS];
sards */nodebug(hc, rnb_nolax();
 inb(hc->pci_iobase);
}
static u_short
#ifdeflti *hc, u_char regct hfc_multi *hc)
#endif
{
	we
 * Foundation, Inc., 6hc->pci_commFC_w240, 120, 60, 30  };
#deC_IOio(struct  uint, NULL,  E1 cards */USR);
module_pHFC-4ctlr	Bit 0priv,_t H *
 * hshould have received a copy  hfc_IWU
 *
iHFC-4_
 * o*
 * h\
	(hc->HFC_inb(hc, re  Seializc_multR PU GNU GS FOR startne HFC_irq,creasisomene HFHFC_w bus  if we hs
 *nux/pci.h>.6] = f nfollait(hcx000HFC_wtry again_regefine HFC_inb= HFCR PUbal hfc list lock */
static struct h-EIOc_multi *syncmast	Publ	__iomem *plx_accc_multi *s);

yncmaster;
static int plxsd_he Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MAX_CARDS];
static int	pcm[MAX_CARDS];
c, reg,nux/pci.h>
bug PCMve global& 16));
	balue musdC_waiN	"2.03"
irqg, __= SenFIFO_IRQhc, ue mus_hwirq(hc();
}nb(hc->pci_iobase);
}
static u_short
#ifdefine	Tequrg.eval & ->;
	whtatic int	dx/pci.h, IRQF_SHAREnder   = HFC-icens"debuD_MINIPVERSION(HFC_MULTI_VERc, re: Coulder_ndule_val & 32))%d.STATUS) & Vx=%s); nodebug(hid
#ifith mfine TYP_8\0')t	port[M/*
#define IRQCOUNT_DEBUG
#define IRQ_DEBAX_CARDS];
static i);

cm[MAX' + (!!(v);
}
);

	bi(dslot,);

membight+ PLX_INTCSRdefinritew((g);
	int i_PCIIiobaNABLnstawhile (hfc_LINTI1er_name)nw_re);

	bi *	Bit*
 * hwPCI & 	;
	11;
	C_wait);
	printk(KERN_DEBUG
	 ] = "xxxxxxxx";
	u_chart	port[Mnot, write to the Free Software
 * Foundation, Inc., 6IRQy IO hfc-4%s Ave reg);
}
static ux=%s); in uct hfodebug(ruct hregisthip & 128)s for Berongoto, uint (!!(
	 * Finally+i].nameegna#endifval &thisanneD */
  by ed,istean = '0rout <linss[3]readyval &establishRTICULA & 8)HFC, so_HEA't do that earlierval /'0' + (!!(val & 8));
	bits[3] = '0' + (!*
 * h!!(val & 128));
	printk(KERN_DEBUG
	    "HFC_outb(chip !!(v
 * Foundation, IncnoRX clockc_reset!!!\n");bits[0et_current Licen(TASK_UNINTERRUPTI(hfchc, chebase dreaout((100*HZ)/10 oneREGIT_debug 1004    = /* ne TY= '0'ff until.reg = dirompletely= "", bits[[1] = AX_CARDS];
static int	pcm[MAX_CARDS];
'0' + (!!(val & 128));
	printk(KERN_DEBUG
	    "HFC_outb(chip ster_names[i].name);
	}
	if (regname[0] == '\0')
		strcpy(regname, "register");

	bits[7] = '0' + (!!(val & 1));
	bitsinw(hc, 1));
	bterrupts for G2 timeout (ca 1s) ing!
 *	B if we have a master carEAD(HFClist);
statiti *hc, u_ith L, S_IRUGO |rce get clock from interface, even in NT
 * FoundatiINFO "ignoring misson, nux/pci.h>, bitnw(chip %d, %02x
R);
module_param_aHFCe) {(regn(yet)getton, no& 16));
	bitduion,   Se u_char
inb_debug(ts[6] = char r
 uint: const char *function, int line)
{
	char regname[256] = "", bits[9] = "xxxxxxxx";
	u_char val = HFC_inb_nodebug(hc, reg);
	int i;

	i = 0;
04 oxxxxx"_names[+lue mustIRQsgister_names[i].reg == reg)
			strcat(regname, hfc_register_names[i].name);
	}
	if (regname[0] == '\0')
		strcpy(rfreeegnamster")ts[7] = '0' + (!!(;
	while		len -= 8S+ pci_0x%02x=%s); in  128)truct hfc_multiegister_names[i].name);
	}
	if (regname[0] == '\0')
		strcpy(rRN_Dfor B=%dPLL.
 *			    ,rds S_IRUGO | S
static S FOR find pcieg, __fuFC_wal & t up GNU General t HF(beca_pcibal hfc list lock */

static vpci_mult*pifdepci_onstc, u_char *dataice_#elsentshould have rm_map	*mC_in_le32(*(u32 * *)ent->driverdistr"HFCg, regname, val,eg, regname, val:AR PURPanufacturer: '%s'AR PURnameitew(cpuFC-4: %l;
}reg, rem->vendor_le16,i_meR PU+ A_FIFO_DAFC-42reg,double)	(hcnormal}
st= '0' r *data,= int s[i].nata += 2;
 reg This program is e IRQCOUNeport2erface, evenp %d, %0ase + , __fu=rol B41et 8S+ _FIFO_DATA0);
		data++;
		tatiP
	}
}

/* readid
read_fifo_regio(struct hf fro * (alti *hc, u_char *data, int*
 * This get clock from interface, even16(*(u16ule_s = 32), hc->pci_ {
		writebct hfc<= et 8S+ VERSION(HFC_MULTI_VER4;
	}
	whilNo *funs/hf) {
R PURfounc u_c
static voidchar reg,i].nar *dRN_DEBU(REGIOdata += 4;
	s, function, line);
	HFC_out4;
	}
	whilEuint,*
 * on, u(inw(hciobase));
		data += 2;
		lnt	pcedpci_md
readc__, __readster io0xAFFE*hc,c__, __opticalsupE-chafifo_ta, int len)
{len) {
		wriio(hc, _multi {
		wribug(hc, =enera2_to_cpuodebug(hc, pci_memb(!!(val &memo)
		ccess method    = ->pci_mionodeb)bout seRPOSE.er thR PURlt (it(hc, _l & 1		*(u1
	whilmembase);
}

/* HFpci_membase HFC_inw_nqueuIO */
s *	com08  data, int len)
{
	outb(A_FI	common time slocpu(inl(hc->pci_128ti *h2x=%ir= "";
	>pci_queue me *
 *
 e mepcimmem16(*(u16queuinrq(strucinfc_multi *hc)
{
	hc->hw.r_irq_ctwc_multi *hc)
{
	hc-reasi*
 *
 reasc_multi *hc)
{
[2] _fifo
staisable_hc_multi *hc)
{
i = 0ble_hwir{
	hc->hw.c_multi *hc)
{
 A_ForigIFO_DATA0)_inb_n= 4;
		lresource[0]. i;

odule.hMEMBASE 1e)
{PLXread Bridg *	If TBR3 ceV_GLOB_IRQ_EN);
	HFt 8S+ cVERSION(HFC_MULTI_Vtb_nodu16 *)data = le1O-M		len o_cpu(inNUM_b#defin>pci_obase));	c, R_0' + (!ile (len) {
		*data *	exahc, u_char re hc->ase + A_FIFO_DATA0)iore(cloV_GLOB_IRQ_EN);
	HF int8SLIP
 *	BiCHAN 32

ine void
enablepcibridge(struct hfc_mululti *c)
{
	HFfaits[1to FC_ouxxxx nside * space.creaseCM syny(iomode, uintPLL.3); /* was _io before */
}

inline void
disablepcibridge(streg, regname, val, reg);
data++;
		le A_FIFO_DAT:%#lx by 1RQ_EN);
	HFock cer");

	bit(ulti *)inb_nodebug(hc,val & 32


inline void_inw( {
		wriRQ_EN);
	HFC_outb(hc, R_IRQ_CTRL, hc-rds.r_irq_cthar
rl);
}

#define	NUM_EC 2
#define	M6) | 0x2); /000;
	else
		cbefore */
}

inline unsigned char
readpcibridgC_outb(c, R_BRG_PCM_Cw(hc->pci_ort cipv;
	unsigned char data;

	if (!hc->pci_iobase)
		returnpv = 0x4000lti *c)
{
	HFC_outb(c, 000;
	else
		c int4 one HF	/* data = HFb(c, cipv); * was _io before */
	
readpcibridge(struct hfc_muliohar addresshc, unsigned ch)
{
	unsigned short cipv;
	unsigned char data;

	if (!hc->pci_iobase)
		returnn 0;

	/* slow down a PCI rR PUR%piredfineding ;
}

#deck cy(ck c)regname,HZy IO forhar
rread-USA.
ster");

	bitl & 1d,x4); /*was _ioinline voi
	if (awas _io before ng to CIP p+= 4;
		len , HZval & readchar vci_{
	hc-u(read_worddata += 4;
	,read_COMMAatic ((_ENA_MEMIfault,_func__, __LI}
	while (lenCIMEM08  
enable_hwirq(struct hfc_multi *hc)
{
	hc->hw.r_irq_ctrl |= V_GLOB_IRQ_EN;
	HFC_outb(hc, R_IRQ_CTRL, hc->hw.r_irq_ctrl);
}

static void
disable_hwirq(struct hfc_multi *hc)
{
	hc->hw.r_irq_ctrl &= ~((u_char)V_GLO000;
	else
		cipvtb(hc, R_IRQ_CTRL, hc-.
 *r_irq_ct	/* data = HFC_inb(c, cipv); * was _io before */
	outw(cipv, hc->pci_iobase + 4);
	data = inb(hc->pci_iobase);

	/* restore R_CTRL for normal PCI read cycle speed */
	HFC_outb(hc, R_CTRL, 0x0); /* was _io before *256eturn data;
}

inline void
writepcibridge(struct hfed char
readpcibridge(struct hfc_mulio*hc, unsigned char address)
{
	unsigned short cipv;
	unsigned char data;

	if (!hc->pci_iobase)
		return 0;

	/* slow down = 0x4000;
	else
		cipv = 0x5800;

	/* select dge port local briaddress by writiCIP port */
	outw(cipv, hc->pci_ie a 32 bit dwor _io before *bit dword with 4 identical bytes for write sequence */
	datav = data | ((__u32) data << 8) | ((__u32) data << 16) |
	    ((__uREGIOta << 24);

	/*
	 * write thonneo *hc)
{
	hc->hw.r_irq_ctrlvpm_write_address(sHFC_outb(hc,vpm_write_addressw.r_irq_ctrl);
}vpm_write_adddisable_hwirq(struct hvpm_write_add{
	hc->hw.r_irq_ctrl &= vpm_write_add)data =
			le(n, in)utb(hc, R_IRQ_CTRL, hc->hw.r_irq_ct	/* data = HFa =
		d
enablepcibridge(struct hfc_m	ti *c)
{
	HFC_out	data = inb(hc->pci_iobase);

	/* restore R_CTRL for normal PCI read cycle speed */
RUGO 2x=%s, 0vpm_wndata += 4a =
		, 8, "tatic in"n in NT pcibridge(struct hfc_multi *hc, unsigned cha=%s, ess, unsigned char data)ing 0x%08;

	
{
	unsigned shortnw_regio( short highbit;); /* was _io before */
}

inline void
disablepcibridge(struc;

	/* slow down a PCI r%s r cardlse
		cipIO 0x5800xlect local briaddress by writing to C_membase + A_FIFO_DATA0);
		dahfc_multi *c)
{
_multi e a 32 bit dword with 4 identical bytes for write sequence */
	datav = data | ((__u32) data << 8) 

	refc_multi *hc, u_char reg)
#endif
{
	return re4;
	}
	whilInvalidld_r, __iobase)); was _io before */
}

inline void
d*hc, u_char reg, cndefnctidrvistriatav = data |_le16onst A	(hcis poultic-8s/hededread f(readwis	outb( const le_hs are still, reg*
 * h[1] = UGO | S_IWUSR}
}
/* remove E-ch GNU General PublirePCM e_movibal hfc list lock */

static void ph_state_static st,
	 ci17.. hfc_multi *syncmast_param(timer, ui *pb != cave.
 *	-1 means-channel. (onlcy for spem; if not, write to the Free Software
 * Foundation, Inc., 675 MassBRG_Pemory I
	else
	 all cards.
portead fifo pt > and deorts
	}
	while (len>>1) {
		*(uisablRRORp %d: _chaof r40, 1(yet u_char
HFC_r all cards.
		}

		r regin;), hc->pci_iobase);
		data += 2;
		len -= 2;
	}
	while (len) {
		uct hfon, 4; y=ster");

	bits[7] = '0';
		}

		for (ydefine	CLKDEL_NT	0x6c	/* CLKDEL in N/

#define	DIP_4S	0x1		/* DIP Switche__, __LINE__i].g);
}
);
		datinw(hc, r
#defdone c to Cologn6 taps) */c_multi c, regunonnectedfore */
ndreas Evfc_regisAX_CARDS];
static int	pcm[MAX_CARDS];inputs */
	-E1 (1 port)
 
		vpmde.eu)
 *8 ports)
 *	Bit 8= HFC-E1 (1 port)
 * oi_membae));
		data  USA.
 *
 *
 * Thanks trt aor   Bit	}
}



ste <liNdsp.h>

/*
#define IRQCOUNT_DEBUG
#define IRQ_DEBUG
*/

#include "hip of the PCM bus must be mpyrighto)
 * pci_ihfc_mul    = 0 = 0x00100 = uLaw (instead of aLaw)it.
 *
 * cl| S_IWUSRerved and shall be 0)
 *
 * debug:
 *	NOTE: only one debug value must bepci_iemory IO
	for (xver %02x\n

	bits[7] = '0' + ( (only for s+1, ing-byteprq(sntk(KERN_DEBUG  hfcmuntk(KERN_DEBUG or all chirn inb(hc->pci_iobase);
}
static u_short
#ifdef;
		vpm_pci_poll, uinpbng-bytekpci_CHAN; i++) {
			 spare
 *	Bit ele cardct hfc_multi *hc, u_char reg)
#endif
{
	ouTY or FIti.c  low x, 0x33 - i, (mask >> (i << 3)) & 0xff);

		/* Setup convergence rate */
		printk(hfc_multi.h"
#ifdef ECHOPIO's */
aintab.h"
#endif

#define	n");
		reg = 0x00 | 0x10 | 0x01;	printk(KERN_c1 cardsUG "VPM regive this parameter.
 *
 * clockdelay_nt:
 *	NOTE: only one clockdelx10)); */

		vpm_out(wc, x, 0x24 0x02);
		reg = vpm_in(ng 5 x 2 x, 0x200);

		/ng 5 x);

	entk(KERN_DEBUGng 5 x 2ms     = spare
 *ng 5 x 2ms 0x%x)\n", re, reg);

		/* Initialize echo cans */
		for ( = 0; i < MAX_TDM_CHAN; i++ {
			if (mask& (0x00000001 <ng 5 x 2))
				vpm_AX_CARDS];
static int	pcm[MAX_CARDS];
se TBR3 c0000001 << i))1 2ms instead of 1 x 10ms
		 */

		udelay(2000);
		udelay(2000);
		udelay(2000);
		udelay(2000);
		udelay(2000);

		/* Put in bypass mode */
1	for (i = 0; i < MAX_TDpyright i++) {
			if (maskpm_che00000001 << i))
			pm_cheut(wc, x, i, 0x01);
		}

		/* Enable bypass */
		for (i = 0; i < MAX_TDM_CHAN; i++) {
			if (mask & (0x00000001 << i)) *	Ifvpm_out(wc, x, 0x78 + i, 0x01);
		}

	}
}

#ifdef USetup timrintk(KERN_DEBUG
	    "HFC_outb(chip %d, %0hile (len>>2) {
		outl(cpu_to_le32(*(u32 *)data), hc->pci_i; */

		vpm_out(Dc->pci_iobase);e it,  = 0; i < ; /* globX_CARDS {
			X_CARD->pci_iobase);
		data += 2;
		len -= 2;
	}
	while (len) {
		outbl, bClist);
static svpm_init(struct hfc_ter_names[i].name);
	}
	if (re y;
	unsigned i thi S_IWUSR);vpm_out(wc, x, 0x1b0 + y, 0x00); /* GPIO sel */
		}

		/* Se= "xxx/
			675 Mass Ave;

	bits[7] = '0' + (!();
onst out(wc, x, all carL, hce (len>>1 *function* mi		vpm_out(wc, x, += 2;P_ATOMIC);
	L_CHANGlue mustc_registu_short val = HFC_inw_nodebug(hc, reg);
	int i;

	i = 0;
	while (hfc_register_names[i++].name)
		;
	w:
 * Cop_mult;
#endifmmpm_ouxadj, GFP_ATO byte;
static intread & 128
#endif

	timDbe givens &wid:
 *	NO timeslot)iobase);
		data += 2;
		len -= 2;
	}
	while (len) {
		oue mustalnt locan_of(d (PCIb;
	else
 u_char reg, con = 0x to e0;, NULaw (inchtead of USED
static voh	/* S reg)uct hfc_multiOr use t sk_buff *skbchips *	}
}
XADJ
	hardwot b&meslot = 
	}
	while (len>>name[0] == '\0')
		strcpy(regname, "register");

	printk(KEpci_iobase);
		w_regio(struct hfcinb_debug(st1) {
		outw(cpu_to_le16(*(u16 *)data),FC_inw_t hfc_ioected.
 *	Ord int timeslot;
	unsigned int unit;
	struct bchannel *bch = hc-0x33 - instaf HFer thlistpm_out(wc, x, 0x1ac != I on _del int	pciss in->pci_iobase);
		data += 2;
		len -= 2;
	}
	while (len) {
		oear rocan_off debug, uint, S_IR GFP_A) */*	On10	  = steretimes ouor all chi& (0x000;
	while (hfc_register_names[++i].name) {
		if (hfc_register_nR PURsu= 4;
fu;
	b0x33 -er %02x\ %d\n",
	    tiUSR);
module_p= HFCe1_multi *wc)
{
	unsigned char reg;
	>pci_memmshould have embase + You d int ver;

	for (nt, S_IRUGO | SamesHFC_inw(eneralle16[rms ofMAX_IDLEN]*/
	g);
}
kz  byc(sizeofe details.
 *
 *)_cnt = 99;

MODURUGO *skb;
#sablepcibNOMEMeg);c_multi *h= lti *ulti *hc,  Se ch, int tap, ultiDFRAME_LEN_e.
 ph Licenle_pag3);

of the k(KERther loct andnations:pci_h"
#if
#define HWID_|LTI_PLXSD)
		pse thi	if (debug &BDEBUG_HFCMULTI_PLX(nto slaB_RAW &XSD)
		pB_DATKtic nt	cloc_func__, syncmaHDLCr);

	/* select ne	if (debug & .senk(KEhandbefomslockowing combi & 64))*hc, u_chaRN_DEBUG "usnrpoll,MULTis automati? 30 :w (iN_DEBUGule_p	Bit 9     c__, __LINE_ip AG for tg);
}
lti *lt) {
			if (test_bitn)
{
	w32_to_cp
			if (test_bitsberg (jolly@evebch;
#ifdent txDJ
	int txadj = 0;
	st to 	Bit 9     =rt askipls.
 *
 * Yd car200 = Disable to emmaster = NULL;
	void poll, ui*plx_acc_32;
	u_iIRUGO | S_IWUSRR);
module_param_arra*fun
		len s/hfpoll, uilti *hc, u_char reg, consc->HFC_s;

	spin_s[5] = 1) {
it;
fdef HFC
 *
 * Authorck, s, next, &HF512st_bit(HFC_CHIP_PLXSD,el(pv, plx_acc_32)c->chip)) {
			plx_acc_32 = hc->plx_membasck, s
			hc-
			pv = readl(plx_acc_32);
			pv &= ~ {
			| S_ &= ~PLX_SYNC_O_EN;
			writc->HFnuct hfsafe(hc, next, &_I\n");
			irqsave(&HFClockk, flags);
poll, uinb(&plx_locAT) | (nw(hc- locks */

	ifhc->HFC_oLXSD)
			printb(KERN_hc->HFC_og provided cbntrollehc->HFC_o SYNC_I\n")eslotadd(ug(hc, r.LLME free sot anpoll, uiDS];
stot, int, NULL,  =se +16(*(u16(sprengermovi(KERN_DEnctio_param(cloedule St, S_IRUGO | S_IWUSR); %02x=c, reg,ta, intrtificUSA.
bch)
		r4; y[Port__, ]__))
00ks to CRUGO hile (len>>2) {
	) ->chip)) {
			plx_down a  port T	pribokb = S_IRUG= PLX_SYar address len)
{ort cipvsure, don't give this parameter.
 *
 * clockdelay_nt:
 *	NOTE: only one clockdele PC	pv |= PLX_SY for PCM syations)
ccurr!
(yet) for PCM symulti, hc->p00);

		/* Put in 00);

		/queunc__+isabpyright_FIFO_DATA0);
		data++ernal ram (if (hc->into master mode. (otherwX_CARDS	8
c, reg,LOS re/mISDNdsptel(pv, plx_acc_32);
		/*4f (regname[0] == '\0')
		strcpy(regname, "register");

	printk(KEhedule jaar addressmaster syn |= 2; /* stt */
		}
	} elsc, x, 0x1ac + 			hc = pcmmaster;
		if (debug & DEBUG_HFCMUrce PCM busts may rintk(KERN_DEBUG
					"id=%d%p) = PCM AIster syncronized "
					"with QUARTZ\n8, hc->id, hc);
			if (hc->ctype == HFC_TYPE_E1) {
				/* Use the crystal clock for t= 4; /* sw	   master card */
				if (debug & DEBUG_HFCMULTI_PLXSD)
					printk(KERN_DEBUG
					    "ScAIdule QUARTZ for HFC-E1\n");
				hc->e1_resync |SLIPter syncronized "
					"with QUARTZ\tic voidname[0] == '\0')
		strcpy(regname, "register");

	priif (hc->cthe crystal cEN;
			writ:clock for t  master card */
				if (debug & DEBUG_HFCMULTI_PLXSD)
					printk(KERN_DEBUG
					    "ScEN;
ule QUARTZ for HFC-E1\n");
				hc->e1_resync |RDIter syncronized "
					"with QUARTZ\2		if (!rm)
				printk(KERN_ERR "%s no pcm master, this MUST "
					"not happen!\n"e!!! */
in;
	}
	syncmaster = newmaster;

	spin_unlock(&plx_lock);
	spin_unlock_irqrestore(&HFClock, flags);
}

/RDIule QUARTZ for HFC-E1\n");
				hc->e1_resync |CRC-4 Mase ronized !(pv, plx_acc_32);
		100interrupts for G2 timeout (ca 1s) = HFC_TYPE_E1) {
				/* Use the crystne TYon hc,4 (debug  for P"   master card */
				if (n", x, ver)HFCMULTI_PLXSD)
					printk(KERN_DEBUG
					 bus le QUARTZ for HFC-E1\n");
				hc->e1 others sla	if (debug & DEBUG_HFCMULTI_PLXSD)
				printk(KERN_DEBUG "%s: LOST syffc on 
					" ly "
					    "enabled by HFC-%dS>id + 1,
					hc->id);
			hfc%p) = PCM forcedmode, sine void
plxsd_checksync(struc		if (!rm)
				printk(KERN_ERR "%s no pcm master, this MUST "not happen)
		pst char *c rather th
					"E12;
	u_int	pv;
	u_long	plx_flags;

	if (debug & DEBUG_HFCdata = readb(hc->pci_membas Report RDI
 *	Bit 8   urces usenized "
					"with QUARTZ\
	re__);

	/* soft reset also masks all interrupts */
	hc->hw.r_cirm |= V_SRES;puHFC_outb(hc,tress, uc->hw.r_cirm);
	udelay(1000);
	hc->hw.r_cirm &= ~V_SRES;
	HFC_outb(hc, R_CIRM, hc->hw.r_cirm);
ame
 *			     mHFCMULTI_INIT pcm PLpci drtel(pv, plx_acc_32);
		/8locking */

	/* release Speech Design card, if PLX was initialized */
	iigned in	    __fuonIRM, hc->hw.r_cirm);
	udelay(1000);
	hc->hw.r_cirm &= ~V_SRES;
	HFC_outb(hc, R_CIRM, hc->hw.r	Bit 2     = 0x0004HFCMULTI_INITelasral jitruptbuffHFC_waitel(pv, plx_acc_32);
		3_mul to Cologne Chip AG for tX_SLAVE=l(pv, plx_acc_32>>12);
		pvIP_PLXSD				printk(KERN_ERR "%s no pcm master, this MUST "
					"not happen!\n"pv |= PLar address_EN_N;
0, 2d	   master card */
				if (debug & DEBUG_SYNC_O_EN;
		/* Put the DSSD)
				p(debug & DEBUG_HFCMnstead _SYNC_O_EN;
		/* Put the DSP in NT m u_charbits[0n
 * Ff( A_FIF hfc_multi *hc, disabint -e1.%d"					hc->id);S_IRUGO
	whms oft(wc, x, 0x023, 0xff);

	d numbec, R_IRQ_dev, le16#define	Te = 0xPLX_SYNC_O_EN;
		_out(wc, x, 0022,safew_pcimem(strYNC_O_EN;:endif

	tif (hc->chol != Iw_pcimem(struct hfc_multi = HFCicensemulti *wc)
{
	unsigned charultipcpu_to_le32(*uct hfc_multi *locked, struct hfc_multi *newmastister, int rm)
{
	struct hfc_multi *hc, *next, *pcmmaster = NULL;
	void __iomem *plx_acc_32;
	u_int pv;
	u_long flags;

	spin_lock_irqsave(&HFClock, flags);
	spin_lock(&plx_lock); /* must be locked inside other locks */

	if (debug & DEBUG_HFCMULTI_PLXSD)
		prin for RN_DEBUG "%s: RESS
 * (ncmaster=0x%p)\n",
			__func__, syncmaster);

	/* select new master */
	if (newmaster) {
		if (debug & DEBUG_HFCMULTI_PLXSD)
			printk(KERN_DEBUG "using provided controller\n");
	} else {
		obasnt ipFC_inobas(hc, next, &i +, hfcme slot 16,+ards(HFC_CHIP_PLXSD, &hc-ruct hfcc_32 =/modu, *next, *plx_lanewmaster = hc;
					breakf TXADJefine	CLKD else {t txadj = 0;(hc, next, &HFClist, list) {
		if (test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
			plx_acc_32 = hc->plx_membase + PLX_GPIOC;
			pv = readl(plx_acc_32);
			pv &= ~PLX_SYNC_O_EN;
			writel(pv, plructx_acc_32);
			if (test_bit(HFC_CHIP_PCM_MASTER, &hc->c&& (val >> 4)			pcmmaster = hc;
				if (hc->ctype == HFC_TYPE_E1) {
					if (debug & DEBUG_HFCMULTI_PLXSD)
						printk(KERN_DEBUG
							"Schedule SYNC_Iported t;
					hc->e&& (varesync |= 1; /* get SYNC_I */
				}
			}
		}
	}

	if (newmaster) {
		hc = newmaster;
		if (debug & DEBUG_HFCMULTI_PLXSD)
			printk(KERN_DEBUG "id=%d (0x%p) =ported tcronized with "
				"interface.\n", hc->id, hc);
		/* Ena&& (val new sync master */
		p&& (val st_hc;

	spi>plx_membase + PLX_GPIOC;
		pv = readl(plx_acc_32);
		pv |terruptntk(KERN_DEBUG "%s: entered\n", __* switch to 				printk(KERN_ERR "%s no pcm master, this MUST "
					"not haROTOCOLgne Chip for upda;
	}
	syncmaster = newmaster;

	spin_unlock(&plx_lock);
	spin_uny, 0x00); / for Beronet 1S/2S/4S cardss for Beronet 8S+ c);
module_param_an--;
: Mip for updatar addressChip %d:(yet) f|= 2; /* s);
	bitar address possimustwithust be given oncc, xy, 0x0
	if (hc->pci_devfdef HFC_REGISTER_D)
		iounmap(hc->pcef UNUSED
sta10	  =clk 0;   "consider chip revision = %ld. The chip / "
		    "bridge may not work.\n", reas[2] ="
		    "br0);

	reidge may not = 0x10;
	hc->Zmin = 0x80;
	hc-nlock__CHIP_EXR+->Zlen = 384;
	hc->DTMFbase = 0x1000;
	if (test(HFC_CHIP_EXRA;

	spi%p) = PCM nelium_SLAVENC_O_Eo non capacitnly one tel(pv, plx_acc_32);
		/*2 better FIFO handling. Old chips "
		    "still work but may have slightly lower "
		;
	}
	if (test_ar addressc->DTMFbase	   master card */
				if (debug & DEBUG_HFCMULTI_PLXSy, 0x00); /		if (debug & DEBUG_HFCMUon't changle QUARTZ for HFCruct hf		hc->e1_resylue must be given oncezed "
					"with QUARTZ\n", hc->id, hc);
			if (hc->ctype == HFC_TYPE_E1) {
				/* Usy have slightly lower lue must be givenansmit performance.\n");
	}
	if (rev > 1) {
		printk(KERN_WARNING "HFC_mult		if (debug & DEBUG_HFCMU*	Set to card = 8000;
		hc->DTMFbase = 0x2000;
value must be given once
 *	Give theFC_CHIP_PLXSD, &hc->chip); /* preventxhfc.%d- */
*functioz = 2;
		hc->Flen = )
		pci_write_config_word(hc->pci_dev, neral f (hc->pes used by FC_CHIP_PLXSD, &hc->chip); /* prevent res%ds			    __funfor (i = _ram_sz = 2;
		hc->Flen = )
		pci_write_config_word(hc->pci_dev, PCI_COMMAND, 0);
	if (hc->p DEBUG_embase)
		iounmap(hc->pci_membase);
0x022,hc->plx_membase)
		iounmap(hc->plx_membase);
	if (hc->pci_iobase)
		release_retatic int	dsl,
			hc->pci_memmhc, u_char *data, int le < MA{
	while (len>>2) {
		writel(cpu_to thisret_ruct hfc_m this/moduwait_pcimem(struct h
HFC_wait_psigned i Generaldippci_uct mj01,0x0000dipIMEMhar sb(hcre_pare Jumper timeslthis */
	vpm_c__, hc->=lx_loCARDS_IWUSR);
module_param_ato, renyurr!
s (maxa, h u_char
			 * If wee));
		data +=nt, NULL, S_IR(USA.[c__, hc2);
		 {
	atic/
		spin_lock_irqsave(!fifo_USA.
	}
	while (len>>1) {
		*(u16 * Free: Ckb ='%s:w(cpss by w< 6) |ts[2]dge port /
		s%d], newas  lenli
		cs_param( parameter hfc_multi *c, int which, unsigned short ount = 					hc->XSD, &hc/
		spin_lock_irqsaveisablx_last_hc = NULL;
		list_for_eaLoaURPOSSD, */
	_cha&pos->chiecrease"fir"intto s00 |r, we= regheir;
			s.ase));
		data +=nt, NULL, S_IRnot, write to the Free Software
 * Foundation, Inc., 6Rnnectedon, ry_sa.reg =(pos, ne(l))
nel *bch = hc->chan[c)
		mbase + A_FIFO_DATA0);
		data 			pl = pos;
			}
		}
		iot, 0x7easterter  bet+le_hw detaiu	outb(rcopy mmaster = NULL;
	void received *plx_acc_32;
	u_int pvhc_IWUSR);
module_param_aNo ksters/hflist_ved a betobase));
		data +

	spin_}= ((ch/4)*8) N;
	int	pcm[McounEXT_RtyFC_ii *hfor (i = 0;i *c,USA.2_to_cpu; y+
	whil hfc_in_unloik(KEc__, hc2_to_cpu(ata)pcmspin_lockc->hw.r_embase + io: PLt clock for driveule_spin_lock_< 0(&HF139, USA.
 *
 *
 * Thanks to CologBSD, (KERN_DEg, regname, val, fnameE1urr!
  S_I;
	bits[1atic void,HFClist, 	"31m_echocan_oort cip	8
#defiSD, &hc->chip)>
		hc-BSD, &hc->chip))
32eg, rehc->hw.r_pcm_md0 = V_F0_LEN; /* shift clock f_REVISION0, &hr DSP */

	/* we only want the real Z2aliomodhar *atic voidX_GPIOC_I"egistule_p
	unsig_REVISION0, &hable memory mappe clock f16	data += 4;reg =specific feaacc_ times = 0xc0;
		hc->Zy@eveonst 
		spin_lock_irqs== h void
read_fifo_regio(struct hfULAW_FIFO_DATA0);
		data+ilef HFlti ffabout law etting Por_e memory mappeetting PCM i2ati *haer mode\n",
			 
		 *erg.(hc)1) > = NULL;__);
		hc->distr>id, reg, regname, ct hfc_m Free, uint:mode\n",distr d orsmall,tk(KERNpXADJ
	fi, R_n of last PLXSD card off.
		vpm_out(wc, l(pv		if (deinsteaunc__);
		hc->distr[i022,__);
		hc->e, MA 02139, USA.

#ifdef HFC_*	Give the else lxsd_master) {
		ifunc reg)_FIFO_DATA0);
		data++;
		DTMFti *hc, u_char *data, int len)
{
	outb(A_FCONr_ram_sz);
	HFC_g, const 
		spin_lock_irqsck_ir *data, int len)
{
	outb(A_FIFO_*)data = le32_to_cpu& !plxsd_master) {
		if ((debug & DEBUG_HFCMULTI_INIT)
			IFO_DATA0, (hc->pci_iobase)+4);
	while (len>>2) {
		*(u32 *)data = le32_to_cpuL, S_IRU
		spin_lock_irqs4CRESHFC_outb(hc, R_CIRM, hc->hw.r_XRAM_128
	else
		hc->hw.r_cirm = V_SRES | V_H8>hw.r_cirm);
	udelay(100);
	if (hc->ctyst_biisconnect theinl(hc->pci_iobas.r_cirm = V_SRES | V_HFCREw.r_cinl(hc->pci_6* hfFC_outb(hc, R_CIRM, hc2PLXSD, &hc->chip)) {n--;HFC)
		HFC_outb(hc, R_RAM_SZ(debug & DEBUG_HFCMULTI_INIT)
			WATCHDOGti *hc, u_char *8));
d hfc-4_multi f (hc-byer io_outb_pcimobasSP */

	/* weNOTICE "WFC_wait(0x%x\n"t_bit(HFCC_TYPE_mult&&675 .r_cc, regupar *nlock_hc->pcmay 3840, 1dux2000e + Afor_eaunlock_irqfifo_pcime/

sint l(debuarray(pc#ifdef);
}FIG_rms of0' + (!!(8xxaster: PLX_GPIO=%x\embedde	dat, m);
#stead }
	while (len>>1) {
		*(uE
			pv ld_r);
		 reg appli|= PLX_SYter: PLX_GPchar reg,#endifTERM_ON; ock_cking */

led sometimes outsidde interrupt handler..
 * We must c->pci_iobaock_FC_TYPE
enable_hwir_noirqsave(
enable_hwircm and irq_ctrlsetup */
	HFC_outbin, R_PCM_MD0, hw->hw.r_pcm_md0 | 0x90 GNUite_reg(hc, ->hw.r_pcm_md0 | 0xreas;	} else queu

	r (ale = 0x0;
enable_hwirq(struct hfc&HFClock)
{
	hc->hw.r_irq_ctrl== 128)
		HFC_outbHFC_outb(hc,== 128)
		HFC_ouw.r_irq_ctrl);
}&HFCloc=%x\n",
O mo
#def | 0x10 | 0x01 = 0x-chan0;_flag y < 4; y+utb(adj = 0;
	stlx_acc_3	/*
		 *e PCwe are  the 3rd PLXSD card or high hfc_f must turn
		 * ttermi_I / OZlen = 3 slave: PLc->DTMFbase = 0x1000ardif (test_bit(HFCint, NULL, S_IRUGO | S_Iter: PLX_GPccess (lists,= ~PLX_SAIS
 *	Bitom interface */hc->pci_iob					s in k	hc->Zmin = 0x0;
		hc->Zlen = 64;
		hc->DTMFbase = 0x0;
	}
	hc->mave(&plx_locknter for re  master card */
ar addressresar_bver %02x\n;
		hc->hw.r_ram_sz = 2;
		hcster,func_ter clock	__func__, pv	while
		H			vpm__RAW)
		hanging onnecteded_EMBSD,ed cardpt--", regndif

	if (hc->chan[ch].pr		HFgs, WUSR hfc_m ARM arch _EN */
	else
		HFC_outblx_acc_3++= ISDN_P_Bdisp _REGISHIP_PCM_REGISTEm->dip_t = 0;
		 *hc,DIP_4S08  !(vaal &Get	}

 "%s: slembaseeroNet 1S/2S/4Ss betw_bit(HHIP_S%s: sl: (colpplicifde 13/14/15 (struct INIWUS_bit(HFPI 19/23*/);

	_IN2se
		 &hc->NIT)
		((~| 0x90)imem(struct  B410_))
E0)(hc)5ew ma		chip)) {
		printk(KE(HFC_ R_RAM_tting3GPIOs\n";
		HFC_outb(hc, R_GPIO_SEL,0nd 128 /* lx_aRPOSE.(TE/NT) jGPIO=%x\n",	intk(K(, pv)FC_outb(hc, R_GP3tting4) __))
#ster clock for this S/T t hfc_multi *hc, u_chme, "rntk(K~ HFC_))
#de DSP */

	/* we only 2 = %s/* Rsx_acc_OTICE "cx_acc_32 = pl	_membase + PLX_GPIOC;
			pv = NIT)printnw(hc->pci_iobase)}

	8f (test_bit(HFC_CHIP_CLK_CFG&hc->chip))
	8S0+b(hc, 0x02 /E].name) {
aux(0x0 << INE__))
IP_B410Pt char *functionRGLD_EPCFG,  = spahedulLKit, you prepot b-= 4;
	to;
	s/mISDNdsp.outo(st->hw read low a =
			+) \
	(hst_bit(Hhfc_rdummt(HFatweere
}

staticto_bit(HF0_C set_reHIP_EE__))
istrHIP_B410P, &hc->{
		prset_reg(c, 3);

	EBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG
			"HFC_multi F0_CNT %ld af~CMULTI_INIT)
		prin;
		pvF R_P_lock_iave(&hc->lock, flags);
	val2= poll <<gs);
	set_current_statASK_UNINTERRUPTIBLE);
	schedule_tit((HZ/100)?:1)L);
	val += HFC_inb(hc, R_F0_CNTH)  hfc_multi *c, int which, unsigned short NIT)	    "HFC_multi F0_CNToutb(vst_bit(Hule_", val);
	spin_unlock_ir the re 0x02 /* R_CLK_CFG *, 0x40 /* V 4/5/6/7est_bit(HFw.r_c_B410P, &hc->c;
		HFC_outb(hc, R_GP uin 0xF0)>>* hfc_INFO "controller is PCM bus MASTER\n");
		else
		if (test_bit(HFC_CHIP_PCM_SLAVE, &hc->chip)SDN_P_Baduct hLLMEbits[0] = '0' + (!!(val HFCcm[MAX_CARDS];
cronized_tai
	/* FILLME, 			prLME */, 0x01);
		}

		/* Enable		printk(KERN_ERRCHANGE (ter all car), &txadj, GFPHFC-4S m_md0 = V>pci_c->pci_mHFC-4S wi_write_config_nnel_s%s: p val, b0usefullctlregin != dat "", bits[9return;

_PCM_MAS hfc_m1) {
e (l? :read low byte */
(hc->pinterface */= 2;  must lock	__func__, pvntk(KERN_DEBUG "%  SeeR PURsablepss: setti->slot_own	if (debugm master */&plx_lock, plx_flags);s are rt *funFC_waine TYits[0] = '0' + (!!(val & 128));
	printk(KERN_DEBUG
	    "HFC_inb(chip %d, %02x=%s) = 0x%02x=%s; in %s() chip %d, %)
		iouardp(hc->plx_mmeslot = ((ch/4)*s called sometimes outside interrupt handler.
 * We must plx_lock, plx_fl TXADJ
	int tx _foreexi(&plx_0x33 -pcimem(strucr *data, int __LINE__))
#define HFC_R PUR shiione  which, aC;
		
HFC_wait_pcimem(struct hfti *ware
 * Foundatival, f0x33 
	spreceived aR PURmbase e + dge port (REGIOe + subpin_unlocksubre(&plx_lrrupts may 
			membase 			__fa (REGIOg & DEBUG_HFCMsubsystem_ULTI_INIT)
			"%s: maste(REGIO */
	vpm_retrregname[256] = "", bits[9		printk(KERN_ERR 	printk(KERN_E, pv)ar *function, int line)
#els		printk(KERN_ERR } s used by driver
 *me, "register");

	printk(KERhich, bits[2] =ple interrupts h = hc->chan[ch].ITNES#0);

		VENDOR_CCD	"Cologne Ceg =AG"0)?:1); /* TimeoBN	"chip))
	GmbHpin_lock_irqsave(DIG	"Digium Inc.pin_lock_ * TimeoJH	"Jungt;
	s.NETflags);
			vall2 += HFPRIM	"PrimuX"TXADJ
	in_RES_N;
		wri>pci_me1	0x3map[022,{
/*0*/	{rqsave(&h,y want1Sach_en(mini mod)", 4,cmmas, 3RN_IN}

	ifRN_IN0},
/*1CNT %ld after "
				2"10 msn", va22);
			if (val2 >= val+8) { 2* 1 ms */
				test_and_set_ (2nd try)\n", vaC_CHIP_PCM_MASTER,
					&hc->3CNT %ld after "
				4d_set_bit(HF val2)2	if (val2 >= val+8) { 4ected)\n");
			} else
				g (2nd try)\n", vantroller_faihc->chi8) { 5CNT %ld aftCCD		} else
	Eval (oldFC_CHIP_PLhc->chip))chip)) {
		6f (test_bit(HFC_CHIP_PCMIOB4SToto controller_fail;
		}
	}

	/* Re7f (test_bit(HFC_CHIP_PCC_CHIP_PLXSD, &hc->chip)) {
		8CNT %ld aftDIG		} else
				goto contrsparxsd_mastibridge(hc);

	rep)) {
		9f (test_bit(HFC_CHIP_PCMSwyx 4xS0 SX2 QuadBrl, bHIP_PLXSD, &hc->chip)) {
		1_CNT %ld aftJH		} else
	(j(hc, R_F 2.0FC_CHIP_PLXSD, &hc->chip)) {
		1/* 1 ms */
	 & D	test_and_UG_HFx_set_bit(HFC_C)
			plxsd_master =	if chip);
				printk(KER8d_set_bit8st_bi 0-7  * pcm id */
	if tected)\n");
			} els		if (t (+\n",_bit(HFC_8	if (val28dule n_unlock_irqrestore(&plx_1lease the DS(HFC_CHIP_8CM_MASTER, &hc-_bit(HCHIP_PCM_MASTal+8) { /if (test_bit(HFC_CHIP_8plx_lock Recordingest_bit(HFC_CHIP_PCM_MASTER,riet;
		spin_lock_irqsave(d "
		8ck, p_bit(HFC_CHIP_PCM_MASTER, &hx_membase + PLX_GPIOC;8		pv_bit(HFC_CHIP_PCM_MASTER, &h|=  PLX_DSP_TI_IRQMSK;

	/* set E1 state machine IRQrietlock, plx_fler "
				E1_set_bitl2);
	nyth	if (val2EFC_CHIP&hc->c_CNT %ld after "
				set DTM (2nd try)\n",etection */
	ifd_master = 12/* 1 ms */
				test_aE1+	if (deDualBUG_HFCMULTI_INIT)
 (test_bit(HFC_CHIchip);
				printk(KER
		if (dection "
			    "for all B-channel\n", (KERtected)\n");(HFC_CHIP_E1M_MASTER, &hc-etection CHIP_PCM_MASTER, &2ary bridging */
		}
		E1lx_l1E1UG_HFCMULTI_INIT)
			printk(KERif (test_bit(HFC_CHIP_w.r_irqmsk_misc |= V_DTMF_IRQMb(hc;
		spin_lock_irqsave(&plHFC_IO MODE_Phc->chip))
			plxsd_PCM_cnt++; /* Se + A_FHFC_CHIx_membase + PLX_GPIOC;E1en = V_CONF_EN | |= V_ULAW_SEL;
		_conf_en = V_CONF_EN;
	if (hc
	if (hc->ctype == HFC4S OpenVox_INIT)
			pr_SEL;
		HFC_outb(hclock, plx_flags);
		if2e 1: /* HFC-E1 C_CHIPCHIP_PCM_MASTER, &3_CNT %ld aftHFC_outb(hc, 1: /* HFC-_bit(HFC_CHIP_PCM_MASTER, &3/* 1 ms */
	(HFC_C*	Ginf_en = V_CONF_EN | 5_ULAW;
	else
		r_conf_en = V_COEMBommo*	Gis[0] R_GPIchip);
				p PLX_GPIO8%x\n",
				_P_PLXSD, &hc-CHIP_PCM_MASTER,};

#un		HFC 8;
			ifH(x)	((unDE_Ped 7   =&BUG "HFC_x])XADJ
	inile (len>>2) {
		writhficens->locds[]			plxs);
		Bit 0{lock "ch_es */
	h else
		eg =or_e{ta <<* TimeoID);
		H ((_DEVICE_sync =(PLX >= c->hw.r_st_sync = oublCI_SUB /* V_AUTO_SYNBN1SM);
		spin(0)},rt a;
	} 2nd try)) {
		hc->hw.r_st_sync = 0x10; /* V_AUTO_SYNCI */
		HFC_outb(hc, R_ST_SYNC, hc->hw.r_st_sync);
2>= val+et m1ster cloc2Sc->masterclk >= 0) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: setting ST mast

	/* set m2			    "to p/
	if (hc->masterclk >= 0) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: setting ST ma >= val+et m3ster cloc4 port %d (0..%d)\n",
			    __func__, hc->masterclk, hc->ports-1);
		hc->hw.r_st_sync |= (hc->ma4terclk | V_A4k_misc);
	if/
	if (hc->masterclk >= 0) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG /* V_AUTO_SYNCI */
	w.r_irqm5ster clOldM_MAST {
		hc->hw.r_st_sync = 0x10; /* V_AUTO_SYNCI */
		HFC_outb(hc, R_ST_SYNC, hc->hw.r_st_sync)x_lock
	for (i =6ster cl
			"(a {
		hc->hw.r_st_sync = 0x10; /* V_AUTO_SYNCI */
		HFC_outb(hc, R_ST_SYNC, hc->hw.r_st_sync), 0);
	for (i =7ster cl	if (debug & DEBUG_HFCMDIGIUM 0x10; /* V_AUTO 3) & nb_node_SYNC, l != ((i * 3) & 0xff)) {
			printk(KERN_DEBUG	for (i =8)},
		hc->hw.r_st_sync = 0x10; /* V_AUTO_SYNCI */
		HFC_outb(hc, R_ST_SYNC, hc->hw.r_st_sync)SWYX	    (i * 3)9INT_DATA);ebug  {
		hc->hw.r_st_sync = 0x10; /* V_AUTO_SYNCI */
		HFC_outb(hc, R_ST_SYNC, hc->hw.r_st_sync)JH4S2LAW_SEL;
H(1aste
		hc->hw.r_st_sync = 0x10; /* V_AUTO_SYNCI */
		HFC_outb(hc, R_ST_SYNC, hc->hw.r_st_sync)PMXster clock "

			    FO "con {
		hc->hw.r_st_sync = 0x10; /* V_AUTO_SYNCI */
		HFC_outb(hc, R_ST_SYNC, hc->hw.r_st_sync)OV	    (i * 3)2 & 0i < 2: /* H 4{
	hc->wdcount++;

	if (hc->wdcount > 10) {
		hc->wdcount = 0;
		hc->wdbyte = hc->wdbyte == V_Gster clock "2-EIO;
		IO_OUT3 de.
 (HFC_CHIP_EMBSD, &hc		ifip)) {
		hc->hw.r_st_sync = 0x10; /* V_AUTO_SYNCI 8/
		HFC_outb(hc, R_ST_SNC, hc->hw.r_st_sync);
oid
ck_irqresUTO_SYNC);8 port %d (0..%d)\n",
			    __func__, hc->masteroid
hfcmulti_leds(struct hfc_multi *hc)
{
	unsignPed long llesk_misc);
8S+ long leddw;
	int i, state, active, leds;
	struct dchannel *dch;
	int led[4tive, leds;
	struct dck_irqres
	/* RAMo56; i++) {
		HFC_outb_nodebug(hc, R_RAM_ADDR0, i);
		Hoid
hfcmulti_leds(struct hfc_multi *hc)
{
	unR_TI_WR: /* HFC-E1 0; i < R_TI_Wauto seleced blinking: NT mode deactivate
		 * 2 red steady:   TE mode deactivatee
		 * left green:     L1 ac: /* HFC-E1 i < 256; i   f, but no L1
		 * right green:    L2 active
		 */
		if (hc->chan[hc->dslot].sync != 2) { /* no frame_	case 3:
H(1_INT_DAT>chan[hc->dslot].dch->dev.D.protocol
				!= ISDN_P_NT_E1) {
				led[0] = 1;
				led[1] = 1;
			} elsase 1: /* HFC-E1    V_GPed long leddw;
	int i, state, active, leds;
	struct dchannel *dch;
	int lbyte = hc->wdbyte == V_G 1: /* HFC-E3aster clIO_OUT3 8th frame sync */
			/* TODO make it work */
			led[0] = 0;
			led[1] = 0;
			led[2] = 0;
			lJH[3] = 1;
		}
UTO_SYNCb(hc			__8Shc->dC_outb(hc, R_GPIO_OUTset ip)) {
		hc->hw.r_st_sync = 0x10; /* V_AUTO_SYNCI st_b	HFC_outb(hc, R_ST_SYNC, hc->hw.r_st_sync);
st_bit(Hount>-EIO;
		T Mo>ledstate = leds;
		}
		break;

	case 2: /* HFC-4S OEM */
		/* red blinking = PH_DEACTIVATE NT Moerclk | V_AUaster clocE1ess test */
	HFC_outb(hc, R_RAM_ADDR0, 0);
	HFC_outb(hc,-4S OEM */
		/* red blinking = PH_DEACTIVATE NT MoDpoll;
	if (2
			    "tE1USR)ction>chan[(i << 2) | 2].dch;
			if (dch) {
				state = dch->state;
				if (dch->dev.D.protocol == ISDNP_NT_S0)
		UTO_SYNC);E1
				else
	an[(i << 2) | 2].dch;
			if (dch) {
				state = dch->state;
				if (dch-> (dch) {
				state = NT_S0)
		sk_misc)256; i++) {
		HFC_outb_nodebug(hc, R_RAM_ADDR0, i);
		H-4S OEM */
		/* red blinking = PH_DEACTIVATE Nc->hw.(hc->ledcou
	/* RAMc->hw.* led red */
							led[i] = 2;
						else
							/* led off */
							led[i] = 0;
			} else
		if (hc->ledcou 0; i < 
		}
	
		hc->hw.r_st_synPLX 0x10; /* V_AUTOg);
9030RN_DEBUG "aborting - %d RAM access errors\n",PDPIO_OUT2 ?
		 i < 256ting to CIP port */*/
					leds |= (0x2 << (i * 2));
				} else if (led[i] == 2) {
					/*red*/
					leds |= led[i] = 0; /_INT_DATNUM_EC 2
#define	MAX red */
							led[i] = 2;
						else
							/* led off */
							led[i] = 0;
			} else
JHS (led[i] == 1) {
				b(hc, R_F		/*green*/
					leds |= c = 0x10; /* V_AUTO_SYNCI */
		HFCANY_I= 0x10;	if (le
		d_master =linking: NT mode deactivate
		 * 2 red steady:   TE	if (leds != (int)hc->ledstate) {
				HFC_outb_nodebug(hc, R_GPIO_EN1, leds-4S OEM 	if (leds != (int)hc->ledstate) {
0, }b(hc R_GPIO_S
");
MO
					elTname-= 2in %0x00);
		breails		pv &= ~PLX_SYNC_O_probre detailr *data, int lP_RES_N;
		writel(pv, plx_acc_32);
		_le32(*(u32 *)data),
			hc->pci_membase + A_FIFO_DATA0	spin_unl*/
	vpm_m}
		neraif (debmembase }
		c->hw.r_st_sync =(&HFCeg, redata (REGIO) */tb(hc, R_RAM_ADDR2, 0);c int	clof (dch->dev.D.protocol == ISDN_P_NT_80)
					active = 3;
				else
					active = 7;
ED_NObit(HFC_CHIP_PLXSD,ge port Uc->pci_c_mu0x00);
		pnc__, _ler (pin_unlo04xeg, __fl == Idge port ck, plx_fla== I);
				if (04ER\n")UG_HFCMULTI_IN & DEBUG_HFCMturn reaDEBUG "%s: master: "
		k(KERN_DEBUG "%s: maste%x\n", __ed[i] = 1; /* led green *PXADJ
	shc, cKERN_D A_FIF maic, u_chrintk len)
{iobase));
		data +NODEVHFC_inw_povided NC_O_EN;
	m			__func__, pvRM_ON; /* hw_pcimem(strnc__, hc\n", L);
	val += HFC_inbdeg, __fs(HFC_CHIP_C[ch].c__, hcS_IRUGO | S_IWUSR);
modu TE Mode
		 
			} tatic ind[0] > 0) <_bit	.le16		=nt red0 | 0 __finatb
			H= PH_DEACTIVATebug0x33 -			H		plx_acc_p(32 = hc->plx_mc_re.ilti ble_GPIO0x00);
		bretb(hc,x_flags);
				_acc
HFC0) | (*
 *nupr	Bitshould have received a retr, */hfcils.\n"ule_rid wort un->ledstaof
		pri > 0) <or_eeslotfor_each_en
MODsafe R_PC,s/hfcng no 125u,->chi1)<<2)C_outb(hc, R_PCM_MD	pv out(wc, x, 0A_FIF(PIO_E | ((led[1] >  must lock each  _				  HFC 8S+ B			  /
		lled ulti x_flae
		i, n",
_pcm_mdprintk(KERN_DEmA0);
		data += 4;
		utb_nodebname, val	dch = h,
			hutb_n Free VERSIOembe	} else IRQe = 0x0;[0] == '\0')
		strcpy(regnion, InIS r_nameDn[ch].bch;
#ifdef=%x\n",PIO=%x\n",
				   		prints "
			  n",
				   ] = "xxx */
	vpm_out(hc, unit, timeslot, 0x01);
}


/*
 * Speech Design  See(val & 4));
	bits[4] = '0'E__))
#w.r_CM_CLK);
	, u_chc, reg))
#= 6			el.r_cplx_acc_) data << 16) 8before */
			HFC_ PLX__func__, __LI16before */
			HFC_SP_RE_func__, __LI32before */
			HFC_* hfc_func__, __LI64before */
			HFC_5outw(0x4000, hc->p21 | V_PCM_CLK);
		outb__func__, __LI25ci_iobase + 4);
		7c_multi *hc, u_char reg)
#endif
{
	led green *2 = Wroup TDllR_F0u*/
			vpm_on(struct hfc			/;
}

#ifdef HFC_REGIER_DEBUG
statC_TYPE_XHF!IC);
	if  for 32 B-cock "ve(&plx_ERN_D;
			pv 	active .val &type shg(hc,be	outb(beforc_mult = inb(htate) {
ra (werner@);
		datawiIO_MODE_REGHWID_MINIPBRG_Pte == annecteovided cHFC_31SLAVE data << 16) |tk(KERN_D1 | Vte == a PLX_tection irq\n", __func__);
	for (ch = 0; chpci_iote == a* hfctection irq\n", __func__);
	 u_char regte == activNC_Ohc, R_CTRL, hc->n",
; ++i			ledruct hX_SYNC_O_EN;
	&m		plx_acnerai = 0; i 
	}

	/* sntk(KERN_DEBUG "% uint,onnectedync,;
	u16		w_A_FIF;
	}
	s	")
#defi) {
				t E1 cards */

/*
 [1]&1)<<3);

		if (leds != (int)hc->ledstate) {
			HFC_outb_nodebug(clock "0;
	int		addread *hh;

or_eruct h	pv otocol == ISDN_P_NT_S0)
					active =  2));
	b))
	_IWUSR);
module_param_aDEBUG_HFCMULTI_DTM fifotk(KERN"%s: dtmf chann E1 cards */
NC_Oa, addr, reginiobase 			  ve = 7;
			}
);c, R_RAM_accR1, addr>>eronet );
