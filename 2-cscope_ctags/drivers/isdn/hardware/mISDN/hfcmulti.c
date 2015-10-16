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
				} else {hfcmu	/* one extra count for the next event */ow levhc->chan[ch].nt_timer = cards    	And1_ hfc-[pollndreas] + 1evermu	HFC_outb(hc, R_ST_SEL,versberg (
 *
 * Authorport)ed to m/* undocumented: delay afterism:
 *		sed cardsued by(1ng-bytes.deallow G2 -> G3 transitionci driverqueue me * AiAg hfWng hATE, 2 |eter SprenV_SET_G2_G3yright }d to breaked tocase (1):d to ger (sprengerjoll)
 * E -tg-bytetest_and_clear_bit(FLG_ACTIVE, &dch->Flagsyright _-deve_data(ree sodev.D, PH_DEram iATE_INDPeter prenMISDN_ID_ANY, 0, NULL, GFP_ATOMICyright e) (spCopyright 4008  by berg (joleter Sprenjolly@eeter Spublished by
 *3the Free Software Foundation; either verrg.eu)
 setThis progify
 s fbute iftware; you can redistribute it and/or mify (spite)
 ehfc-8sterms offc-8sGNU General Public License as prranshedsuse}
	}
}

/r (spcalled4s/hfrive mode init message
ci d
static void
d to lti_  SePOSE(struct d(sprnel *dch)
{
	 detailhfc_of
 * *hc =ls.
->hw;
	u_char		a_st_wr_pliee, r_e1anty of;
	int		i, pt;

	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_tebyte"%s:  insred\n", __func__)ram; if ger (type == -devTYPE_E1) by Wger (spreger dslot].4s/h_tx; either ologne Chip AG4s/hfc-is great controller!
 */ESS FOR modconf; either A 02139,
 * msbyteC	 mor_ty of
 *ment.is automa,t even * bu.protocolder th-1e impt 0-7yright evenrg (j.ambr (wer= (arra *) recof
 * dbusyerg (j0001 = HFC-E1 (1be u* orlong) of ed to forerg (jseful,
rg (jyrighTY oCULA(i = 1; i <= 30 = ++tiA PAyA 02iger se the fotd to tersinueed toger (spreiodule parameters:
e DTMFd haect on aaramer ts:
ls via hardwalt (0),fc-8scy hardwaed.hfc-Or ui, out eP_NONE   = 0xode. (o0S (4TY o/* E1ci driA 02 Thisule -devCFG_REPORT_LOS, & * USA. hfc-B0800faulfg)ad of a-development.smLOS0, 255); /* 2 msci drivtopubl	Bit 13	de. 1pare00 = Us51 14rg (= 0TY o 12Bit 15 x0100(insFoOPTICALs inytesasr thPOSE. (otherwhi* MEu04000 = Use extRX0whise auCologhw. gre(ins0 | V_OUT_EN= Useti.c  to Cle 64 drea for(512steed by
 2 (spor= Use 7 1de. (o2se a = Uss:
 * t*	Bit 14 poV_ATXt 15 NTRIeverto)
 *	Bit 13	 Tanytse
 *	Bit 18yrigh Signal (Dual E1 1ith Watchdog)ed by
ll other bits are_FRsteax0 (sp(all  0x10 bits are_FR= 0xxf8ge, M0 = U8 *	Bit 1iver ernCRC4(512K)
 *	Bit 16    = 0x10000stead of anytad ofhardw2, Vd foMFt 15 TX_EheerveEG_Ed 128 lots instead of apare
2V_AUTO_RESYNCt 15 ter moiCO |hise 28 instead ofon PCMhfc-p bus (PCM
 *	Bitpubl (spmovi: (op (weal or required only1 dtmRrr thisalelium_t 0		Bit clockowing combina (wer hfc
2K)
slavTankticallMA 02not, wrion, Inc-8sFbuteSe hope ram ( Found)
 *	, Inc., 6E1 moviutedNT- mor AveANTY; wit C poridge,			t 2 a7    w17 0 = UsG0ci driv    e1_getn cap01,0x on PCs)
 instesshat iknow whathat iare doingype:= Usex0800erwhise4 = Disable E- * AneTE (No = 0x0001l discessing000 =example:whise  witse aF 0x000= 0x000 drivrts)4S w@every bits:
slots on PCM
 HIPr lisetup512K)
 *ipchip).
 *	Bit 1     =setuare
 dtmf3   E1_RXtical 128= Removi LOS	HFC-E1 08 = bitsone HFsrneliul rant 16  publE1CLOCK_GETly bits:
  *	BiReved (lar ths/hf32 B 0x0FC-E1 5 Report x002	Bitit 4   P17  FC-E1 8ame Mode, 1*	BitTurn off Cresem*	Bit 5     = 0x0020 =PCM_SLAted i	Bit 8     times/* m512K ( for 3		    (ci driv *		 received from port 1
 *
 *	HFC-E1 only bits:
 *	BANTY; witt 0     = 0x0001Bit 18 0xtern"t 11Report	Bit 18froms S/)E-ch,cessing)
 *Bit 4     = 0x0010 = ReCTRL dtmEXT_CLK0the F17   er tetup (
 *	Bit n caphfc-pved (later for S onNT  16 MASTERync ratht usede, 400ter Force putoth bibyteinterface,-e1 b675  TE 16   00 = Use  PCM els transs on PCdirdwarR dtmfkrn ofP
 *	sync rathtohan PLLne de		 Repo(E1 o0x00000 = Use 2-13e. (oX *	Bit= RepFFSdebug le 128 timesltne d HFC-4S withX clockre reserx010and shFC-4be 0(ony oncnot,  hfc-NOTE: 0x00 drivnot,ust be given for 	en     not,all caXll be 0enable debugging (ANTY;QUARTZc_mul)
 *n, I
 i.hcreasnot, whip).
s(ony once llo process.
elasral jitthannel procbuffer ( = spaJATTBy dith it!
 *	Valid is 8, 16,  4   SLIP
 ITY	BitF *mque-4S/d te8S pcm ATrd.
x9cHFC-E1(ony oregin capi drito)
 *	Bit 13	 PWM_MD dtmPWM0_MDf samples foral
 *	Bi PCmodu5one HFC-4S with dtmf PWM (we af-3),	Bit of
  machineograups S/erfane debde. (otE1blishedsit 2 e: 0x00 mustE1_LDshed card hfc Co6HFC-E1wa
 * t least 5,21uue musteyort 1not *
 * conn= For),00 foach c *	ThMultiframe Mode, use dPLXSD* debug:
 *	N defau    buggronized9s
 *	Gi	plxsd_check*	On	Or use= Use 6aster clock0010 evencrears:
 * type:
dware
 *llCRC-4 nnes via hardope 00 = Use 0 Repo5    e debug value ust be gived and se they iacitive mode)
 *			     = 0x10000 = Usports)
 * oce pepubli	Bit 180-therwhise a*	Bi for 32(4movialid  *	NOTE: o7therwhise a8 value8S (8ce pealid 			   no sup 4   - 2odule parameters:
 * type:
1driver. If 	Bit 10    nto masteard hasth the same ID 	Bitndriviseternedriv512K)
bits a16    = 0x10000 = Usme  for 16, w1as D-chom *
 d  = 0xE1be gR PURne deIn e: 0=conclockr D-channel.ich15 o. (oth give	If 1..15	Bit17..311 an alternate time slot is used fodeTis conpt = Use9non-E1 oE-ch whichareldwari5 Madebuuse they are not inte hfc-pP piyrigh.0020 =T ANYnsp   = by existitivverspci dri will hshall bcapacitinate ti be gh for Don't   = S0is unle *		 received from port 1
 *
 *	HFC-E1 only bits:
 *	B interconnecteST = 0x0%de: 0=. interface: 0=copper, 1=op  = it, yat id tell
 *
 *connect4lots on PCMiven  256DLY,ed
 , 
 *
 _n it, ytwarranty of
 stead.reser   Bitld),
 	Bwarractrl0[pt]= HFCST_MD= 0xSet both bits to7   eix010er th 4    app
 *	rts)
 * *	By dtmter mo IORPOSE. s OTE:memoryy on(M = *
 * debu: 0=copper, 1=opspecificy one cl, soNY Wcan witbe not gorce PCIt maylockusefullbytesete one clotoconne	Bit io2master2LY)
 olvrds aPCI ing)
  disblem = 0xae value!8000 = Use externNONCAP_TX(512K)
 *	Bi= Usr chip).his register is needed| HFCTX_Ldog aard is a USAne don't neThaXHFCtsr D-cy arprocess.
 *	By defclo0x40REGIy arePU
 *	N */ebug ptions)
 *
 *0x35gistegisteTRL3 */nsure,0x7c << 1erfaces(A_SLSE */ster, thime lis nebeca  = th busses with 8S on onl0,enger register is needeific ou d 0     = e given Yo so 0x0e musclockgi *	Ns/hfher ydrivene de-> |oncepren8000 = Use externDIS_ECHANNo nect giveFOR A caped by_ card contrnumber star2 = tE_IGNOrt AI= 0x0010 	Bit Mode, uoalue musalueA_ST_Cd
PCM ustBinterfacethis p, the k val cardceivt.
 *
 * 2,procBit 4_ENrd isB2* 	Ena, whichrt 1
 *			    cation, so don't change iiven fne mode vS/T interfacesonneSThvices with Xwill hts ainc8S (8ngvicesiource Allis conneses wrese each fifinterfacesacer,  (nce.
e wit *	NOsci_mskrtin1terfstridded devices with X)
 *	ruptfine HFC_REGISTER_DEthSCI_MSKreserved an
#incluclock valoset PCMnle t= 0x0to so 0x08000 = Use exthis recomme to handle e the value <lock cont&e Fou   ~(linuxologne Chip 	-1 meerE-chyrighth is connecects intnsure,  = 0 MA 02ou know what you are doing!
 *	t 2    Disabdnux/mIS'ee theTER_D}
implied wint
open_y are
 *to rave this paraor h, al hfc ; /* glo You ,
N andl hfc  /* glo_req *rqshoulodulern; ereseu_ car	 hopee, MA 02ou know what yoW_OPENS)

static  = 0x004 = Disabdev(%d) Cloce cpu %pLL.
 *			    
 *	prenacitive midClisbuiltin_return_address(0) cardif rq->ven for every card.
ONEhip)  be 0 -EINVALdefine	e must be given for e!tatiTYP_4S		4
 &&
ck; /c.
 * , 16ndreas E 6;	/*efauE1		1
#dots slotdefine	MAX_PORTS	(8 * MAX_MOD4
#de* if we have a master ca(sprge ven for e%x(see%xf u
 *	Te witM buriverrol regi0, 120, ,d va POLL_TIMER i;DS	8tatife must be given for every card.TEe for8 s *	Bis POLL_TIMER evel . (othLts slhip)l1_e1 bue mustl1, CLdete/T icar */
tatierna6mse:
 /* numb= 0xf* CLKDEL in Tterci.hs 	   (0x60 MUSs forefaulded!) Ttimesetailhr syte_l1r Be, card_l1A PAbacDNhw.hS	0x2errode, us fo TYer muste TB must be given for ev/* CLKDEded!)odulspin_ tel_irqsave(debug tel, yncmn, sd fy of
 * Ms/hfmoreou spe[MAX_CAunRDS];
strestors = 1	pcmS];
stac uint}, MA 02( for  DIP Switches for BSce for&&o	DIProrfaces = 3)slotISe
			2	vel DIP Switches	dslouintnesingic uX_CARollnt	i7t	clX_CAting nt	int	clockdedreasnt	i so disnt	clockdelay_te =1CLKDEL_TE;
static uint	clockdelay_ntatiic uin_NT;Switches HWID_Nt 8S+ till flA_ST_CLK_re e but WITHOUT ANY WARRthout even the;
sta /implied warra = 0xt	tim}
 for ch = in the * buitches !try_d thledriv(THISe:
 ULfor 

t 2     = 0xWARNIN.h>
8f the clgetRPOSulDor	Blist)efine Ht arecar0t	clsin cap_tmode capbstatglobal
 *	 g, ufine TULE_eneral Publ ph_ine eGenengee detailsace: 0=c*);R);
mod(structlay_, ui	*bved (lCLKD 8  * Th");
This /* glomapk;
stadr. /* gloitches ic ue_p	Bit(clWUSR) E1 carP_8S		8R);
mod_PORTS];
static uint	deb = 12
#definaram(clockdelay_e must be given once
 nks
 | S);
moduth 1te, uin;
, the fiWUSR)ng with 1_param_alue) +	clockdeD-chcarS_IRb int,ger (sprenger, m_arUR);
bch_MINIPt 2     = 0xERR*	Giv)
 *	nalhichULARhaluehastatiime0, 120,60, 30  };
#detatic uodule_param_arrayode */
 m is distributed be AX_CAR&b the hope 
ULE_LI S_IWUBUSY(gistbinterfacet wibe *	of WUSRsonn, the  This pron once
is distFILLEMPTYIWUShwidaram_t_mastyTHORtuffpe:
 *ic uint	clockype[ger (sprengerrx_of1   reseAUHFC_("Anc->queu * Th);
mMODULE_LICENSE("GPLal) \
	(hc-VERSIONor	B_ Free eg, val);, S_IRUGO | S not, al) \
L, S_IRGO_IWU_I intS FOR device 2 theol * dslot:the i);
module_p | S_IWUdg)
 bal hfc oid ph_state_);
modulemout ehc, rSR);
c
#define HFCtthis para	 copyt even the ith allent.th 	(hcwd	(hce, wd_cntrib;sint	t (cq->op_MINI by
 hout e onl RDIOPhe F
#defin=nonot, chani-devOPLE_LIpublishinw_inw_nodebug(hc, WDoing!:gist forec-8swved andDNdsp.brid, et 1PORTS &tiplLE_Lrege, C = !!witchp1 >> 4OSE.BR3 c	BitG2clockout (ca 1s) *SGc uint
 * 	And1_ hfc-[] = { NE__))	2
#defineFC_wai_parE.%mer;
sard i,r
 *	Nerpare40, 1moduleplx_loce mec, reg, v? "time" : "MANUAL"ambrid, mbedded de	(
 *
eg, valt(rg (jo#include <linux/delaTI_WD,EGISTER_DEB | (Cambrid<< R);
molve
 *	Pitbert_ne HFchis fohaniregde, u1 WD= Use:.
 *	Thc, relects interfaceoncrds aGivof 3se
 *	Bitg)g, val))
s:
 interfaceWDEnabDLY)
 hc) \
g, val))
#definfault.
 *ber *	Big,(hc-bug(c)			(nclude <linux/delaBERT HFC    outb(hc, reg,  __LINEre HFCREe6ms *h>ESS F2
#definIRQCOUNT_te tof

#ifdef COMISDNber starhwug(hc, reg))
#utpu-ICULASpeech-Desigerner@in-development.smGPIO/T ius V#if_IRUHF7debug ptions)
 *
 * pISTEREN1C_gist (alEN15UG
queue me_pcimeme detailOUTck sourcit(hc) Gener(hc-,
		conset tlti *hcse
H, int /*
 Cambride, CLI HFC_outb(hc, reg,RESEit(hc)efinhc)		mqueudefin, val))
#defival))
#defiLINE__))
#def Cambridi *hc, u_char i.c utb(hc, reg, e mechanihc->t lin#def20, 60,w (i };
#us id is 100 for the reg);
}
static u_)	g, val))
#defiline)
#else
)_func__, is givhe Feg, val, __func__, __LIN unceivn Op )
#def20,R__))
#define H#defin(hc->(lay_ym(clockdecensa co,LE_AE1 carret_inb(__))
#defin1	0x3(struct _nodetructid ph_stab_nou_
 * cmd,warra *argstatidlood ytruct hhar
	*devet 1ontainerefinctiow_nodebug(ne HFC_f,onnecteg) \
	(hc->HFC		ate_BUG
HFCon, int rdev);
module_param(c,ructw, valp#inc(hc)		(hc->HFCu_char reg, S_IRUGO | S_IWUSR)	*rq\
	(hc->detailreceicens *s	buggnsure,efineg (jol __LINEhe PCMm; /* if we have a master cacmd:%xc AX_C	(hc-);
}reg, val,_linee
HFtatiHFC_outbmd(hc->HFC_w	Bittic uinthe Frq =elaxRDS];HFC_outWUSRwitches DIam(c by
 mer;
static odebugioe detailbug;
he Frvalue must be given once
 this unlneTER_DEmulti *hc_DER to TABILe PCM which*aram(*	Bit uin, renctiorqe give teledHFC_
 *	ne hersltip2, 
static voidine HDEBUG
HFCic void
#debub(vint line)
else
! int *ambr0x004 
 * u_reTER_DEBUr reg, u_onnec, u_charse
HFC_wait, hc->pci_iobtion, int linTER_ndifhoulase);nst cC_wait_ reg, const;
static uinic uintX_CARDS];
static uinsti *hc, u_chimer, uiOTE:tion, int c_multi *t	clockdeioed
 g)
#endif
{
	oc uint	cl_multi *hc, u_char;
static uinthe Fi *hc, u_cha
#endif
(struc /**pts welood yaunsure, d PUR(yet)closehort
#i_STATteg)
#enhar reg,t, S_IRUGO idnwstruct hfce4S with be 0insideWUSR)hfc_.
 *->HFpuoutb_nodebug(huct hf#ifdef HFC_CONTROL->HFC_truct  hfc_multi *hc, u_char reg)
#endif
{
	ofc_mul/)
#endif
{
	r, u_clax();
 inechawait_piobase& V_BUSY)
		cpshort
GISTER, u_char reg, constmulti *hc, u_chaion, int linw* if we have a master cation, inDEBUeb(vbase + re, u_char reg,deC_IOct hfc_mulc, reg)neral UGO | Suct huct hfc_multi d telctl	NOTE: priv,ule_de "hfcFC_inw_pcimelist lock */
py lti *IWU *	Bid tel_ven fe "hfcodebug(hc, rfuncti
	re forializ
HFC_wR PUthe imhc, re hfct		(hc->Hrq,r systsometer feg, verfaceg, conss
 *nux/pci.h>.6defif  (0),dif
{
 8  eg, vtry agaher lghc)		(hc->HFb valu"xxxnt, S_IRUGO | S_IWUSR#else
HFfc_mult-EIO
HFC_wait_imem(st	arra	__iomem *C_ouacc
HFC_wait_;
moimem(struct hft hfc_multi *h8 * MAX_CARDS)

static  = 0x004 = Disab7tionssLL.
 *			     (E1t hfc_multi *hc, u_char reg)
#endif
{
	{
	retu
		if (hfc
/

#PCMve, uint,& 16));
	bc->HFmusdb(valN	"2.03"
irquct hlems.FIFO_IRQ, hc-	bits_hwirq(hchar }unction, int line)
#else
HFC_wait_regio(strhes fequThis			(& ->;
	wh);
	bits[	d	if (hf,f COF_SHARERRANTeverd teof
 *"not,D_MINIPeg, val, __func__, __56] =: C_inwer_n_IRUGx%02x=32))%d.STATUS) & Vx=%s); )
#endif
idgio( = 0xodule_para\0')delart[Mdif

#ifdef CONFIG_MISDN_HFCMULTI_8xx
#in hfc_multi *hc, u_c;
moDS];
s' + (!!(v& V_B;
mo	bi(G for,;
momembby
 + PLX_INTCSR %d, w whw(( & Vh th i_PCIIt liNABLnstawhile (receLINTI1, reame) voidval = He givee "hfc_gist& 	;
	11;
	b(val,hile
 * FoundatiISDN_H	 defi"xcat(reg"GNU Generg, constine	MAX_PORTS	(8 * MAX_CARDS)

static  = 0x004 = DisabIRQalue vers4% 4));US) & V_BUSY)
		cinb_debin _multi
#endifc_multnterfa/

/&ERSI)mer;
staticgotoal) \
";
	u
	 2  inof a+i]. (hfegnan, intx%02xCLKDEnneDWUSR Freeed,rfacan = '0rebug<linss[3]readyx%02x
#elNTABIRTICULA & 8)HFCvalu_HEA't d youat earlier			(/'0xxx";
	u_%02x=8 & 32)itbits!(valxxx";
e "hfc'0' + (!!l & r_names[i].reg == reg)
	   "queue mecc/

/
	u_t 2     = 0x004 = Dnoelay, inc_reset!!!\nGO |128)0et_cur   =  of
 (TASK_UNINTERRUPTIme)
256]cheline rg (out((100*HZ)/1(lategistT_not, w1ven iever/2S/le_p	prinf0, 1til.ug(h=or ao*	Bitely= "",*	NOT[[1;
	p hfc_multi *hc, u_char reg)
#endif
{
	s[0] = '0' + (!!HFC_inb(chip %d, %02x=%s) = 0x%02x=%s; in %s()	Bite (hfs[its[4] hile}m; if r= '0me[0]
 *
'r re
		strcpyi].name), "nterface);
m & 128)7;
	printk(KE;

	i = 0val & 128 hfc_muler_nameIP_8S	0x2s/hfait_nodebug(hc, __f*
 *	HFC-eg, const char *functionEA(debug, uint, S_IRu_char reinw(Lg) \
	(hc->and geall be 0er th*
 * debug:
 *	N defat 2     = 0xINFO "ignortem miss004 
		if (hfc[256]nwn %s()%d, %02x
g);
}
static u_m_aHFCe) {i].naint lgett);
	ro(val & 32)itdux004  foron, int
inb, u_ch(ts[regis
	retu
lockd:etere
HFCfdef HFC_REGISTER_DEBUG
{
	g, constame) 25regisme[256] =9		strcat(regname, hfc_r)			(egnamhfc_g)
#endif
{
	rethile (hfcram; onne;
04 o(regnae (hfc_+;
	bitstIRQserfacee (hfc_reg line=c voihfcmgistat_names[i].recenterfacee (hfc_register_names[++i].name) {
		if (hfc_register_n in ne);
= reg)rcat(regname, hfc_); inile		len -=MINIPit_p0xFC_w+ (!!(val 0;
	fc_multi *hc, u)+4);
	while (len>>2) {
		outl(cpu_to_le32(*(u32 *)data), hc->g ==r;
st=%dc_multi.h for ,G
st \
	(hc->HFif (regnhc, refind>1) ruct hfueb(va02x=t upthe implied wle_pation_chant, S_IRUGO | S_IWUSR);
moduleit_pcult*pISTEit_p() l hc->pci_i*istrice_R_DEBntu_char reg, um_map	*m (RE_le32(*(u32 * *)ent->driver be u02x=g
	retes[i].line + 4data += 4;
	:A"xxxRPanufacturer: '%s'le (leame)= 0;
cpu tel: %l;
}e + 4rem->vendor_le16,i_me"xxx+ A_	bitsDA tel2e + doubleHFC_inormal_BUS	printn>>2) {,t be  c_regista += 2; connUGO | S_IWUSR);
mef CONFIGt 4  2* debug:
 *	void
HFCC_in+ char
#=ebugB41e_MINIP		data +TA0hile	istr++egio);
	P FITNESS  [2] idnsurd_fifostruct hfc_multilineven fonw(chip %d(len>>2) {ISTERBUG
#defin in %s() line %d\n",
	    hc->16c->p16IRUGsE: o2)
HFC_inb__ {
		ow whbmulti <= ic voideg, val, __func__, __* hf} 4;
	}Noef HFCULA) {
e (lefoun		cpc);
module_pag, const its[4n>>2g == reSR); Oistrmemb* hfs,  HFC_REGISDEBUG;
mqueue m16 *)data =E, reg star004 u( hfc_mt line)regio(strmembase		l_CARDedit_pcidata,fdef HFata,faces i0xAFFEcharfdef HFhip)calsup= 0x0int l4);
	wh len)
{2) {ata += 4io u_chaFC_waita += 4nodebug(=plied2_to_cpu
#endif
{
	it_pcembhfc_regis_nt IFO__MULTImethodg, conwait_pcio)
#en)bebugse)		(hcANTY;e (let 11if
{
, _i = 0		(inl 4;
	}
		dine)
#el, u_HF0));
		dC_inhc->HFC_w-devIO}
	ifNT_DEB\
	(h+4);
	wh>2) {
	ine)
#e
		d_DEBUG
#define Icpead lction, in128nw(chutw(irit_n;
	ait_p-developgiven oelopcharmemcpu(inl(-devinrqe detainif
{
	outb(R_S{
	tion or	Birq_ctwl |= V_GLOB_IRQ_EN;8S (8iven o)
 *l |= V_GLOB_IRQ[2]  int if ( 0    _hl |= V_GLOB_IRQ_regiruct wirRQ_EN;
	HFl |= V_GLOB_IRQ;
		origead_fifo_r(REGIO
 * hfclrear *fu[0].e_fifLE_LI.hMEMBASE 1funcPLXata, Bng)
outb(rTBR3 ceV_GLOBs[0]_EE__)	HF_MINIPceg, val, __func__, w_regiu16 *)M) */= le1O-M	whilese + (inNUM_bhfc_muait_pdata (PCconsR_intk(KE].nam	*(u32 *)>2) {0 for , hc->pci_iobHFC_idata (
		data +fo_riore(cloHAN 32


inline voi%d\n8e HFC_inw( to g))

efinPubliease tcimeng)
  hfc_multi *hc,C_waitB_IRQ_HFfa6] =1to _pcimcat( b(reg, ll cce.r syse debugy(base);al) \
c_mu3 *	Bitwas _io beforg, hc} wasu_re_io belue mus/
}

inline ulen -= 4;
	}
	whic void
(struct hflec_multi *c):%#lx*
 *1
inline voibe 0creg)
			str(C_wait)REGIO) */
statil);
}
st
;

	if (!hc->t hfco_cpu(re
inline voieue mechanism

in0xX0
HFC_has C_outb(h(KERrlA0));
hfc_multFG, EC 2ss by wriM6) |pare *	B000;
	_DEBU		cd char data;

	if (!unsigni dr porteaad_f

inl=%s; in was BRG_ froCHFC_wait_p4   cipvGNU  */
	outw(cidata ram; if !tion, int line)
		 lock pvonnece ch
readpcibridgobase + 4) HFC_inb(c, ci%d\n4later fT be {
	HFCHF + 4)ase)); ;
	unsigned char dat	pv, hc->pci_iine unsigned chaioe R_nside *, hc- */
	outw(_IRQ_har data)it_rebase);

	/* restore R_CTRL for normal PCI read cycle speed ngio(
vel sby Wdpci_a gistre (le%it!
 d, %dtem ddress b(hc,y((hc,)data += HZalue for portv, h-t be = reg)
			stri = 0d,x4 *	Bi	unsign
	if (!hc-m; if a	unsigned char ngicallIP p
		*datwhile, HZ + (!!pcimete fici_RQ_EN;u(ata, worden) {
		*dat,ata, COMMA;
	bi((_ENA_MEMIs giv,->pci_iobase))data =e */
}CIMEMhe Fefore *!!(val hfc_multi *hc, u_cha_IRQ_EN;
	HFC_outb(hrl | SenN 32


inlibase);
		d 0x5800;

	/* select  the bridge daA0));
eneral Publilue mus
	/*
	 * write this 32 bit dword to the bridge dat&= ~((i_ioba)ort
	 HFC_inb(c, ciipviate a write sequence 
*/
C_outb(h data;
}

inlgname[e void
writepcibridge(struct hfoutw(=1 aase));
		dt line + 4ne va;
}

ifunction, int line)
#addreomode[e R/* see[0] 	whilecipv =EC 2cycleCARDedct hf	cipv = 0x5800;* sele0x0cipv;
	unsigned char d256lock _CTRL fa;

	if (!hc->pow whti *hc, unsigned choutw(cipv, hc->pci_iunsigned char addrchar r/* restore R_nside *
	unsigned short cipv;
	unsigned int datav;

	if (!hc->pci_iobase)
		return;

if (address == 0)
		HFC_outined but >=1 aine u58igneaddresapplicis ne ciplocalter nside **
 *ow wi bit  cips
	 */
	outl(datav, hc->char32*	NO dworsigned char d);

	/* dTER_DE4 iden, int ght 1e[0] ow whasequencuct hfa;
}_reaa;
}
|a <<_u32)dge(hc<< 8
	/*
	bytein = readpc1/
	/ = 0x%
	bytle (lreadpc2se);addrval &ow whath 17.obit dword to the bridge davpm_ow whinside */s	cipv = 0x58lti *hc, unsigned up to 4 writes lti *hc, unsi* address on the locallti *hc, unsi the number of write acclti *hc, unsi)
{
	HFhfcmle(GISTE)tiate a write sequence of up to 4 w data;
}

inls(strubefore */
}

inline unsigned c	(hc, R_CTRL, 0x0)
}

inline void
cpld_set_reg(struct hfc_multi *hc, unsigned char reg)
{
	/* Do data 	(hc-utw(c, 0lti * = 0 {
		*s(stru, 8, ");
	bits"ots slotd char reg, unsigned chanw(chip % */
	outw(char
vess)
{
	cpld_set_retruc)tem 
		o8in;
	unsigned short ci void
#ifort ciphighbit;cipv;
	unsigned char data;

	if (!hc->pci_iobase)
		return uc (address == 0)
		cipv =%s ction, but >=1 IOad_reg(xpplicnsigned char reg)
{
	unsie a 32 = 2;
		lefc_multi *c)
{
egio(sse
HFC_wait dworle32_tohc, reg);

	/* byte */
	HFC_outb(hc, R_GPIO_OUT1, reg);

	enablepcibridge(hc);
	bytein = readpcibr

	rei *hc, u_char reg, constion, int linge(hc);re16 *)data =Invalidld_rct h data (PC
	unsigned char data;

	if (!hc->pcchar reg, const ccndefFC_Rdrvbe uspcibridge(hc)+ A_F() liAg, vckdeoned hc->/hederg (d fuencewisne)
#e%s() liressed devstier, rege "hfc "";
	(hc->HFC_inb(ITNEruct mov  = 0xthe implied warranrese te_E-chnt, S_IRUGO | S_IWUSR);
module_param(clock,f (regnam,
	 ci is lse
HFC_wait_imem(stNE__))
#reas, ui *pb != cavone de#inclansface: 0=  = nlcye[0] sp	8
#define	MAX_PORTS	(8 * MAX_CARDS)

static s[5] = '0' + (!!(val & 	datant valu_inb(c, 30  };
#de. cardrote ifo pt >HFC_wdemust *)data =e */
}>>1

inlin(u 0   RRORvoid:em(stof r
#defint intk(KERring 30  };
#de.
		}
le sconnen;base));
		dt line)
#MEM) */
static vile (latic {
			vpm_outu32 *) & 1));n, 4; y== reg)
			strcat(regnam(wc,x00);s/hf(yt, S_IRUGO MINIP	0x6cvel ic uint	tyNhc->pciHFC_outb4S	0x1;
static uint	tidef HFC_REGIi]. & V_Bt(wc, x, hfc_multss byd conticallller!6 tap__fu/
HFC_wai{
	retunsame ID char datSoftware base)+4) hfc_multi *hc, u_char reg)
#endif
{
inputet 1S	.
 *
 * dslot:
		vpmdeTER_DEBue must be given meter.
 *
 * dslot:
 *+= 2;
	 (PCIMEM) */	const char *function, rt aC_outBitFITNESto te6));hc, _
#endif

#ifdef CONFIG_MISDN_HFCMULTI_8xx
#incG
 hc->/mISDde "/

/ even t
#endif

#define	ed by
 R3 cer
			vse
HFC_card isMode, utead.
uLaw (r required aLaw)umber starti>HFC_inb(ee number of samples for each fifo process.
 *	By defaultg(hc->HFects in_reg(ht value
 inputxverHFC_w\n			strcat(regname, hup GPO's */
+1ISTEright p
	 *[i].reg == regonst muERN_DEBUG "NLP , 30  };hir>id,nction, int line)
#else
HFC_wait_regio(str(wc,lti 
		d>pci_iobapb;
		prik
		d to ; i++u32 *)	ll cards are iel..31,dmulti *hc, u_char reg, constion, int line) PCM buIe 128 timex
	HF33 - i, (mask(hc)(iadpc3)
HFC0xffein;
vel Sbecauconverg;

	eratuct hfames[i].se
HFC_wa.h"gio(str ECHOPIO'et 1Saintabk-tim, int ss by wri bitle spine)e, u	/* d1 * for01;ames[i].reg =card hasUG "VPMconnev= CLKD S_IWUer tber starting with 1nto process.
 *	By defting witx10sablULE_ = 0; bug(wc,llows 24 wor2ucky anywaylti in(ng 5 x 2(2000);0d sh give */
	; i <e[i].reg == rege */
		fmultieverl cards (mask & (0x0x%x) Ave,ense
void
 givesInit bitse ech Swid a 	 * inputregio= uL'0' +TDM		if (mask& (0x0t[MAof
	& ( 0x000S (4 <e */
		f)IFO_D= 0;  hfc_multi *hc, u_char reg)
#endif
{
	o/*
 TDM_0000001 << i))1& (0xx20, reg);
1 x 10ms
		ay(2000ued by(e
 *ucky 
	unsigned char gpi2;

	gpi2 = HFC_inb(hctmp, R_GPI_IN2);

	if givesPuith  byp & 4POSE.*/
1r (i =_regio < MAX_TDM_ed by
 *ask & (0x0f (mask pmM bu00000001 <oid
v(0x0* UNUSudelay(200i
	HFC1ucky x00); * oase totk(KERN		for (i =errupt 0x%x from CHAN; i++) i2);
}
#endif / & (0x00000001 <oid
voutb();
		udelay(2000)78 +disable the HW EcITNESGISTER_U a faktimhip %d, %02x=%s) = 0x%02x=%s; in %s()id
HFC		vpm_out(w2u32 *)outl16 *base			hc->pci_m)
{
	 dir */
			vlay(2000);
		udeDion, int line)
ecificrrupt 0x%xc, u_charstatic & (0x0static*/
			vpm_out(wc, x, 0x1b0 + y, 0x00); /* GPIO sel */
		}

	itial, bug, uint, S_Ic sass moit * write thi;
	while (len>>2) {
		outl(cpu y;

	/* restoect oHFC_inb(););
		udelay(2000)1b0 + y
	HFCC_outb(ifdefsfunc	 * W EchocaSestrcat	 * 	!(val & 4));
			strcat(regname, hfhar () li	udelay(2000  };
#electpm_out(wcef HFC_REG* mi00);
		udelay(200embasblic LicuckyL		if G | 0x20 ase)+4);wait_refifo data (RE_LINE__))
#def void
write_fifo_regio(
			vpm_obase)+4);
	while (l++egister
	if (wly one mle32_;l code mm;
		uxadjl Public hc, Ri *hc, u_chaWrote= 0;
l code arckdeDinterfacs &wido proce instead )eslot;
	unsigned int unit;
	struct bchannel *bch = hc->c 0x20 aleadbocanefindce
 IbC_inb(c,n;
	regin = vpoline u/T ie0;_REGIx, 0x2ch, reg);
USEDo the sameh TXADt = c_multi *hc, uM bu so  sk_NOTE *skb %s(len)ITNEXADJ
		Omit clo&stead  = +) {
			vpm_out(wame) {
		if (hfc_register_names[i].reg == reg)
			
 * Found
			vpm_out(wc,u_char reg)_multi N_DEBUG "Hstc, x, 0*/
	ou we confcpu(inl(e taps
 *c->HFC_ulti *io= Force PCM dSTER_instead f (hc->chan[cn WARi4)*4 u_char  S_IWUSR)bOR("HFC_s a udeles[iR_DEANTY;g, uISDN_P_B_RAW)
		rac0; xIle t_dek_t H	pcishecknt timeslot;
	unsigned int unit;
	struct bchannel *bch = hc->eretu;
	strufwith ihc, reg) \
	 Publi 0x0e <lX clock	Biteinste ou0x%x)\n", & (0x000ERN_NOTICE "vpm_echocan_on ca+bits[4] u32 *)t[MAobase)+4);
	whe (lesu		*dafu 32)s a udc, x, 0x %s Ave = 0x%tiuct hfc_multi egname1HFC_waitw dword{
	cpld_set_reregvoid
enamemmFC_inw_pcime, int whiate_	unit vructvpm_outreg) \
	(hc->HF(hfcruct hfcplied anne[hout e];
sIDLEN];

# & V_Bkz Frec(sizeofreg) \
	(hc->HFC)_cnbch)99;
 \
	(	(hc-ol !;
#_iobase)
NOMEMvoid *hc, u_ch= ed chned char r	prichISTER_tap, ned DFRAME_LEN_ chaphe);
	rRUGO g3) != even toundandif
{
	 * bue)
 *			
		d-time.	2
#define HWI|ree *	comware
 = CLK; if not, wrBte to the Free PLX(n alterB_RAW &UG "%s: B *c)Kef TL_TE;
s->pci_iobnsigneHDLCreg(struchfc_mulne; if not, wri.senoundhandd chmA0);pacitive mod & 64))char reg, BUG "NLP "usnrimer, Freit 9     =i?w (i:, 0x == regIRUGOannel.x0000fdef HFC_REG

/*
 * mod & V_Bed chlti2);
}
#enderg.e}
st_IRQ_w3mbase + &hc->chip)) {
	undation; eitherbch; can llockx)
		_locktimetk(KERNst/T i &HFClist, =vpm_skip	(hc->HFC_i drivnc__Bit 0     ifdemnsure, =REGIS;
	e_paramer, ui");

	bi_3aticu_i
	(hc->HFC_inb(it_debug(struct hfrraf HF with 4CULA
		if (t, (hc->pci_iobashan[ch].sg(hc, re strAX_CAs[5;
	pc, x,ch %STER_DECger (sprengerck, s,s/hfc, &HF512)) {
	, __fCHIPDEBUG ,el(l(dast_bit(HFC) *
 *ip) & (0x0st_bit(HFCICE "v>C_ou 2;
		c_32) _alhc-r = h_reaWrotl(>chip)) {
	 hfcmpv acce& (0x0>HFC acceg);
t 0	_Ois wil += 4;val))ef HFC_safew(hc-			if (_I, bit;
}
#static &_param(k, ftwar);
>pci_iobab(&C_outb_ATbridghfc_m-
{
	oe funm; i val))
#oBUG "%s:FC_VOLbundatir;
		if (g disvidi drbrs:
 * r;
		if ( _DEBU_resytead add(;
	unit .LLMEd in the * b
		if (t uint	iu knHFC_REGISTER=t whcpu(inl((sprock:
E-ch.reg == FC_RE S_IWUSR)oeE_LI Seg) \
	(hc->HFC_inb();HFC_w={
	retu4);
	whrt *	Gt be b
{
			rTDM [Portdef ]FC_ou00his unl	(hc-gs configurable, ) 		pcmmaster = hc;
 0)
		cd charTFC_Vbokb =	data++=eg);
SY_reg(hc, r>2) {
	 cipv;
	TE	0x0f	/'tterfa1 x 10ms
		 */

		udelay(2000);
		udelay(2000);
		udelay(2000);
G "VTI_P|1
				&&crease debu)
 *		)
con, !
int l");
			hc->gned 
HFC_in 0; i < M3)
		prin 0; i < M-devbrid+ 0  ed by
 
read_fifo_regio(strucer
 *	ify
(er cac->12K)
 *	Bit 16    = 0x100static 	8!!(val &LOkb;
/mms odsptR, &hc->chip)) {
	nc |/*4++i].name) {
		if (hfc_register_names[i].reg == reg)
			
 * FoundhLX_GPIja_reg(hc, rnsure, synjatt2*	Bitstar byt* GPlti.c %d\n",
	   + _E1)  =ISDNm(structther cnot, write to the Fne is connetster (es[i].reg == reg)
;
		"id=%d%p) =			hcAI PCM
			nly one  ");
				ER_DEload. \n8
HFC_i valhcync |= )
				pcUSA.
 *
ring* Thanks  (0x0
		in PC= 0x0ryint ay, increast		*dster w= 0x *function, ;

#i		printk(KERN_DEBUG
					RN_DEBUG "%s:& DEBUG_or HFC-E1\n");
			 0x%0ScAIX_GPIload. e[0] rts)
 resync |=_EN;
e1nameuggi|
 *	; /* switch quartz */
			} else {
		ral Publid, hc);
			if (hc->ctype == HFC_TYPE_E1) {
				/* UsHFCMULTI_P    "QUARTZ 							"Sch: is automat		    "enabled by HFC-%dS\n", hc->ctype);
			}
			plx_acc_32 = hc->plx_membase + PLX_GPIOCs wi		pv = readl(plx_acc_32);
			pv |= PLX_SYNC_O_RDI; /* switch quartz */
			} else {
		2G_HFCM!rm_acc_3
 * FoundatiERRnc., noISDNd	MAX_CARDSis MUSTrtz */
		 withappenl, be!!!d bym_ar* GPnsigned &HFClnewm(struct		pv &=u;
mod}

	if (nkUARTntk(KERN_DE_outt hfc_mget SYNC_ */
				}
);
	RDI		pv = readl(plx_acc_32);
			pv |= PLX_SYNC_O_CR forMi_iotch quar! "
					"with QUARTZ100*
 * egname[0] == '\0')
		strcpy(r
					printk(KERN_DEBUG
					    "QUAchar on hc,4intk(KERcrease"			    "enabled by HFC-%dSAve,x,ked,its[			}
			plx_acc_32 = hc->plx_membase + PLXnnec	pv = readl(plx_acc_32);
			pv |= PL4S wite tha; if not, write to the Free 		plx_acc_3mes[i].reg == regnc., 6LOST syffcle t);
				 lyrtz */
	 0x%0ease t *
 *d te%dS>idporte)
#	pv |= idync |=hfc1_resync |f anddthe valf (!hc->phe PCM bus YNC_ * wrict hfc_multi *hc, int rm)
{
	if (hc->syncronized) {
		if (syn == NULL) "%s: Rline %d\reduce cpu );
				E1C_CHIP_* FI);

	_7    hc;
/
			ram; if not, write to thea;
}

iWrotnction, in 2;
		ouble frRDIbe given     *fuschanh quartz */
			} else {
		dge(__
		if (deoftct h uinls)
 *	kszeof(hc) {
			if;

#ce of up cirmta porSRES;pu	cipv = 0x58iomoressinitialized UART
	unsigse eUART initialized *acceif (tes pin read low byteIRMence of up p) && hamrds ah for dmthe Free SoftISDNdPLOTE: ozed "
					"with QUARTZ\8s: Gc_muewmasruct PCM e HFC_IO MODE_ardwrdfine	PLX
	uns	intEnableed by i 1;
	uni	u_inata onintk(KERN_DEBUG "%s: c->plx_membase) {
		if (debug & DEBUG_HFCMULTI_PLXSD)
			printk(KERN_DEBC-E1 only bits:
 04the Free Soft28, ed wjitci.hNOTE))
#defied "
					"with QUARTZ3x10 |	vpm_out(
 */

/*
 * modXom iVE=, &hc->chip)) {
>>1QUARTZpvPCM_MASTi *hc, int rm)
{
	if (hc->syncronized) {
		if (syncmaster == NULL) {
		e jatt P_reg(hc, r_EN_N;
spard				    "enabled by HFC-%dS\n", hc->ctype_DEBUG
								if (pot hDSo_hfcmultntk(KERN_DEBUG
				 requirlock_irqrestore(&plx_lock, Pts slotmci_iobas, funf HF Ff(c_multlti *hc, u_char lue mnit -e1.%d"f (debug & DE \
	(hcRN_Nout edelay(2000)023
	HF.. it gdS/4S c5800;

	/dev,C_ou6witches f/*
 0xKERN_DEBUG
								udelay(2000022,\n")w_char val,
DEBUG
			:x7e);
}

sFCMULTI_hol  timplx_membase)_multi *hc, u_
				of
 * ists, card)!
 */
static inlned pw we configur_multi *hc, u_cted PC,name[0] =e
HFC_waitLXSD)
	rfaceISTER_mult{% 4;

	prti *hc, u_char *			if *_PLXSD)
	FClist, list) {register");

	biHFC_CHIP__mul	udelay(10*/
				printk(Kcard %d" /* get SYNC_ */
				}
(KERN_DEBUBUG "%s: GOTnst ects intted PCMtb(reg,endif
{
	o newmasteer
 */
static void
release_io_hfcm
 * e[0]  hfc_multi *hcRES= 0x (mem(stru=0x%pc, x,if (c->pci_iobUG_HFCMULT
		if (debug & DEw	    "enLX_GPhc->XSD)
			t (other c
 */
static void
release_io_hfcmuti(struct hfc_multiustem 	printk(KEers:
 * tresync lti.c  lskb)bas(hfcping wg			;
					hc->ei +_iobahandle this+UG
sC_CHIP_PCM_MASTE &ct le(hc->p		if (/d th	pci_set_dr_outa val, valICE " hfcmu	e)
 *f TAW)
, S_IRUGO ti.c  l}
		}
	}

	/;
					hc->eebug, u>pcisXSD, &h->chip)) {
	c_multi	*pos, *next, 	pcmmaster = hc;
				if (hc->ctype == HF whig);
ifdeCCMULTI_Pf (debug & DEBUG_HFCMULTI_PLXSDKERN_DEBUG
							"SchR, &hc->c*plx DEBUG_HFCMULT* revision check */
	ifCM_ * (albug & DE&& ' + (hc) 	wrirvdata(hc->pags);
	/HFCMULTI_PLXSD)
					printk(KERN_DEBC-%dS\n", hc->ctype);
			}
			plx_acc_32ames[i].reg == reg)
CHIP_R"Sccrystalx%p) =		}

d 4)*4		pv |= P1) != _SYNC_O_100 = u_cetk(KERN_d by HFC} "HFon=%r FITflags, val, val2 = 0,MULTILXSD)
				p0, rev;
	int			i, err = 0;
	u_char			r_conf_en, rval;
	voihc->e (0x1_resINFO
	  itch quarER_DEtz */
"*
 * debu. Ave, (debug & DEBUGocan
 1) != 0x
{
	YNC_Ou_long			fl	p1) != 0xst_ags)
}

/(KERN_DEBUG "%s: entered\n",__func__);
	val = HFC_inb(e ja) {
			struct hfc_multi *hc insass Ave, C "
	__))
able *hc, int rm)
{
	if (hc->syncronized) {
		if (syncmaster == NUROTOCOLC_O_EN;
	s/hfupda & DEBUG_HFCMULTI_PLXSD)
				printk(KERN_DEBUG "%s: GOT sync on ;

	if (!bcer;
static ui1S/2S/4S};
#demer;
static uienablt_debug(struct hfn--;
: M   "HDLC trt_reg(hc, r*/

/%d: /* swi   master n, int  !test_bit(possiu_chER_D reg)		(hc->HFC_ay(2;

	ifflags,tion, indev
			writelse
HFC_ouiteliounmap80;
	hclateN
	structX clocclkupt   "plx_iRANT %s()revi hc->= %ld. T 0x0/

//== 0) 0x%0er is ner ( witworkd_set_)
 *disa=TI_INIT)
		3) != re	printk(KERN_ase)1must & DZmiline u8c__);
	;
moduIP_PCMEXR+->Z, 0x= 38* hf & D viaBUG "__func must->chip))C_CHIP_PCMEXRA"HFC_mu1_resync |neliumhe DSPEBUG
	of 1.alue musinclude zed "
					"with QUARTZ\n2 betck, 	bit 		prling. Olutw(ISDNTI_INIT)
ack "_DEBUd = Her (ood yslby
 ly to erULTI__names[++ierg.eureg(hc, r;
		hc->Fle				    "enabled by HFC-%dS\n", hc->ctype);
			}
			pl;

	if (!bcint)val);
		err = -EIO;
	. Set to -	pv = readl(plx_a*plx_lapv |= PLX_SYN | 0x20 | 0	(hc->HFC_iquartz */
			} else {
			et_bit(HFC_CHIP_RE_HFCMULTI_PLXSD)
					printk(KERN_DEBUG
		EBUG "%s: changing to 	if (hc->ctype ==lineit perfwhilnand_ser_names[++i].v >SYNC_Orev = HFC_inb(MULTI_G%02x=%gnedint)val);
		err = -EIO;
	*	Seisablion, = slotpe !1;
		hc->Flen = 0e
 *;
08 | 0x20 | 0	(hc->HFC_inw(hc, 			  ck */
	if (debug & DEBUG_th chpre1 baxhfc.%d-			ff HFC_REz static  & DF.r_ramware
ci *hc, ult (ig */
	80;
	hc->Zle				ed w0x80;
	hinstea *
 *EBUG "%s: initializing PLXSD card %d\ct h%dse PLXSDc->pcions are _ct hs hc->id + 1);
		spin_lock_irqsave(&plx_lock, plx_flags);
		PCI__u32)ND, ase) membase)pite to FO_DATAse = 0x1000;
	if += 2;
		l);
word2,ntk(KERN_DEBUG hc is currently tct the PCM 
		pv |= PLXI read cycle spk, plr lo%s() line sne)
#+ 1);
hfcmult, hc->pci_ie)+4);
	wh}
	}MAX_if (gs configurable, f>> 4) !ecv_Bc {
		ret_e)
		relea {
		
	spimem(schar val,
		coh(wc, ock, p->chan[chmplied dipX_MAtailmj  = 0x000dip) dae R_secharUGO |e JumpANTYption{
		i;

#lti fdef  & D=_outbatic l(plx_acdebug(struct hf = 'reny|= 2;s(masxa, _mulw(cipe PL*D-chwe (PCIMEM) */
sFC_REGISTEm_ar(t be[ pv);
	QUARTZ = hdef ERN_KERN_DEBUG "%s: do!int lt be ) {
			vpm_out(wc, x, 0x1ab(bcare d: C HFC'%s:	receg)
{
	< /
	/tangiti *hc, ulock,%d]					uns>2) li, cis S_IWUS0ms
		 */
c->pci_dev);cISTER_whispinigned short ciphfc-4=IFO h+ 1);, *next,lock, hfc_flags);
		p 0   ve(&	    FClist, li	_hw)_is geaLoaU_par *ne;

#
		 &pos DEBUehar ad"fir
		tY)
 	 * r, web(A_Fheiype !	s.ta (PCIMEM) */
s card off.
		 ) {
			ver = vpm_in(wc, x, 0x1a0);
			printk(KERN_DEBURame ID tup ry_sa{
	out(po);
		(l))
KERN_NOTICE "v
 * Authc i int which, unsigned short taIFO po dapos  "HFd%s\n",	iu kn0x7CM ier (nevbet+ress (strucune)
#el
		coext, &HFClist, list) {this paraf (debug & DEBUG_HFCMUh charct hfc_multi uct hfNo k_ON;CULA & DElock *betdata (PCIMEM) */
printk(K}= ((ch/4)*8)  wilX_CARDS]; hfc elaRtying u_chions are ca	plx_t bembase + ; y+ES_N;


	tim(KERN_ioundpin_lockbase + (ps
 pcmKERN_DEBUe of up DEBUG "%sio: PLn %s() lis wiA_FIhc->pERN_DEBUG< 0get al,
	const char *function, intllerB *ne.reg == ;

	/* slow down fame)E1|= 2; .
		l & 128)1eral Publ,ruct hfcm	"31m_bypa
	str cipv;
ode */
idebug & DEBUG_>d + 1) clocg & DEBUG_H
32c->pcice of up pcm_mdentrV_F0* mully "
hifn %s() li_REVI val0	hc-r*/
	te(&plx_lw ID */ walockh);

al Z2albase)e %d\eral Publentered_I"ot = (RUGO
 */
stHFC_CHIP_PCM_S     _nt valmULL) %s() li16EM) */
st4;anywads.
 *	Gifeabug %x\n",n = 0cc->chip))Zither() liock, hfc_flags);
== h_io beata, int len)
{
	outb(A_FULAW
read_fifo_regio(struilER_DEed cffa *)dalaw etfc_muPUG_Hmode\n",
			 ode\n",
og)
2le_p*hait 16  
static lxsd * Thilse
1) >FClist, king last_h be uebug  0;

	/* slow _irqrestore dal) \
:V_PCM_MD be u  *	Ismall,c->plx_pAW)
		fiwas Bit   3) s: eSDP_PLXSoffy, 0);
		udelay(, &hint)val)r requmbridgFCMULTI_INIT)[i Dis>ctype == H= '0'alue must be  can lat84;
ntk(KERN_Dti.c  ti *hc, u_c);

	/* ci_ix01)
read_fifo_regio(struct hf via (hc->pci_iobase)+4);
	whdb(hc->pci_membasCONrdl(plx_obase);
dl(plx_plxsd_master) {
		ird %d/
		pv &= ~PLXhc->pci_membaseFO_c)
{
	HFC_ohc->syncu& !lti *hc, u_c);

	/* re not, write to the Free Software	lti *c)
{,x80;
	hc->t line)+se);
_N;
		writel(pv, plre the taps
	else
		hc->hw2x=%s) =ock, hfc_flags);
4ar vLTI_PLXSD)
			printk(KERN_DEBUXRAMen--_inb(c, c initialized *
	if (te  = 0H8T, plx_acc_32);
		pv = r
		pv &= ~PLcty)) {
is same Idebudata++;
		let linC)
		HFC_outb(hc, R_RAhar DEBUG ata++;
		le6* hfTI_PLXSD)
			printk(KE2f (debug & DEBUG_HFC = %HFChc i	cipv = 0x5800;>ctySZES | V_PCMRES | V_STRES
			| V_RLWATCHDOG (hc->pci_iobaseC_inbd "regile32_to0x80;
byces ieg, u_charg			E, &hc->chip)NOTICE "Web(val, ut(w\n"ion check		printgned&&!(vaEBUG		vpm_op */
;
modution, er (38
#defduug &  whicBUG_HFn card %d"int lchar , re, R_Fv;
	i#ifdef cGISTERunc_FIG_hout e[0] = '0'8xxM_ON;testentere=%x\embedde = r, && h#requir {
			vpm_out(wc, x, 0x1aEMULTI_Peg(c_irqsaanywappliatt PLL\n| PLX_SLAVEv = readl code TERM_ON; ard irqsave(&p
	u_hfc_interruptsidSE.  ) {
			 (debuey_te,  W 0x20 |ion, int liard 			prin< 24);

	/*
_no "%s: do< 24);

	/*
cmHFC_wridge da(becauta pin read inwas e &&
D0, hw_MD;

	/* selec/* d90the c, uredebug(ts == 32)
		HFC_out)
 *;		pv;
	u-deval R HFCen = 00;< 24);

	/*
	 * write thet SYNC_dword to the bridge da== 0;
	 = hc->plx_	cipv = 0x58D1, 0x20);
	HFC_ up to 4 writes et SYNCN_N)statone  S_IRU for now workn = 0 0x000;
	hc- y < TDM +tiat}
	}

	/* Di(debug &
}

ise { PCwt
#ie(hc,e 3rdet */
	HFC_ourreg(cc->pcfto resock  SYNC )) {mi_I / Ow.r_ram_lternatest;
		hc->Flen = 0x20;ard* revision checkHFC_REGISTE \
	(hc->HFC_"%s: slave: MULTI__hw)s,_CHIP_IDt, the fire %d\n",
	   */tion, int l"HFC_8S onk);
		hc->hw.r_R, &hc->ch.r_ram6* hfc1;
		hc->Flen = 0c->c}= 1;
	m/* geG "%s: G32);C_CHI/
	e    "enabled by,
			    _res* Thwc, x, 0x2FCMULTI_ or	Bl(plx_acc_32);
	
		pc>pci_t both bi int
init_cpv>hw.r_ = hm_out(waste20);int,c_musame ID ed_EM cloi drivept--%s: cg7e);
}
membase);
 Authorpr= hcgs, & DErqrestoARM arch Enab;

#nb(c, cL, 0x0);(debug &++= ms ofP_Bdisp multi = 0xe &&ulti *hmI_INp_  __firqsachar/
		vphe F0' +MEM)Ge x, 
nc., 6sl2;
		leroN: This drivsG
		won che_PCMSMBSD, : (colk(KEcISTE 13/14/15  * writeIN & Don checPI 19/23*/) != _IN2(c, chc->hwoftware((~_outb()ar val,
		costat0C_ouE0)lse
5{
	u_		pcmmaster =
 * Found, __fase + Pde\n"3ifdes\n voi hc->plx_membaseifde *		P0nrds)
 /* ;

	)		(hc(TE/NT) jVE_EN_N)n",   _k(K(	/* ), R_GPIO_EN1, 0x3de\n"4) FC_outet both bi * module S/T ulti *hc, u_char reg,[i].revpm_i~ES */_outb(AVE, &hc->chip)) {
		f (h%s/* Rsdebug LX_SLAVc;
				if (hpl	N_DEBUG "%s: entered\n", __funoftw
 * Fr) {
	LX_MASTER_EN
	  8 revision check */
	iCLK_CFGc->hw.r_ram_	8S0+echaniword /Eterrupt (oaux(0x0nsta, u_charIP_NOTIPline %d\n",
	   RGLD_EPCFGlti *spacrystLKclock vaprep clo-		*datto;
}
 syncroni.outc, uebugr reg)by Ws(struc+_nodebusion cheobasedummn = atwe	Bit to the stoon chec0_C
 *	_re0;
		HFC_oube u_PCMSK_UN	hc->hw= hc->_F0_CNgite  othe	e to the Free Software
 * Foundatihc, R_CHIDesign PLi hc, NTug & af~e Free Software
 * ologneF;
	i_DEBUG /* ge  "H", __func__);
}val2_accll <<__);
}
ction, lin maskurn val;
}
statiBLE);
}

	print_tl, hHZ/;

	?:1)L we keeannering wrihanismy)\n",H)onst c
				plx_count++;
				if (pos != hc)oftw= 0x%02x=%1st try)\n",ase);
sion chehip)" 4;
	);
}

/*
n card %ddebug &&hc->lo*bytel);
	s *(&hc40{
		V 4/5/6/7ision checDEBUGug & DEBUG_HFgs);
c, R_GPIO_EN1, 0xiobas0xF0)>>		spc_val, f_acc_32;
	ockds connec * (alresync |nb(c, cxc && (val >> 4) != 0xe &&e DSP	hc->hw.r_ra__func_ad;
		i		"is, fun(regname, hfc_regiHFCs[3] = '0' + (!!ly one _taiplx_lC_ouME,adl(pr	"in*/sable the HW Echocan
 *
 *hc, int rm)
{
	i
#endE RAM 30  };
#), &		}
	l Pub for 32 select pb(hc->t the D for 32 irqsave(&plx_lo: 0=_s., 6p down b0T_CLK_Dbug( GPI0; xdatt_nodebug(h lock ;

xe &&
	 c->pci(KERNe */? :e(&hc->loght    "80;
	h>hw.r_pcm_md  masto resi] = -1;
	}

	/* struct hfc_multi  \
	(rr!
 *iobassBSD,ode\-> for_ownare reset cronized */ug(hc, A_c->chi/
				}ed devhar fun(debugchar )) {
controller_fail:
	= 0;
	while (hfc_register_names[i++].n writc void
HFC_wnb_dn = 0n");
		!(val%s()G_HFCMid
HFhc is c>chi PLX_SLAVEif (!bch)O=%x\n",s A PART		writel(pv, plx_cc_32);
		spin_unloc_irqrestore(_multi ERROR, no all r
			}
		 EBUGeexibug(hcs a udchar val,
		_SRES | V_HFC*hc, u_char reg, u_cha_e (le	if idrivt++;
			ad\n",f (debug & lx_flags);
		ifs, cax1a0);
			print only s a u
}

list lock e (le;
		le whiti *hc, uile (l whisubCM bus SLAsde)
bug(hc,{
			ifer (V_RL 2;
		leic intaore(&pl write to the subsystem_TRES
			| V_RLc., 6c, u_ile (l__func__,retrine);
	HFC_wait_nodebug(h*hc, int rm)
{
	ifhc, int rm)
{
it\n"fdef HFC_REGISTER_DEBUG
HFC_*hc, int rm)
{
	if} ->plx_memb A_FIFram(_TYPE_E1) {
				/* Use the R+;
			s, fuginghe c_32);
		ss lx_last_hc->plh].ITNES#3) != 0VENDOR_CCD	"_SYNC_O_EnywaAG"k(KERNth chTnodeBN	"nlock_irGmbHERN_DEBUG "%s: doDIG	"Digium = DiERN_DEBUGoncenodeJH	"Jungh % 4.NETunc__);
}	 keel2ontrolPRIM	"PrimuX"plx_flags	Bitoff:plx_a the DS*hc, map[ Dis{
/*0*/	{"%s: donh,		if (1Sach_en(miniN_DE)", 4,PLXSD, 3RN_IN
	    	if (0},
/*1n",
		    RR "tz */
2"10 ms= 0xva24) != 0xc &keep >=4;
	+8) { 2* 10x02 by HFCerg.eu)
 /
		 (2nd tryc, x, va) != 0xe &&
	    ( "HFC_/* it3* 1 ms */
				test_a4_INFO n chec4;
	2)2PCM_MASTER,
					&hc->4e ID aresync |=lti.c  "HFCg"controller is PCrs:
 * t_fai & DEBU&hc->5* 1 ms */
	ut meset */
	E "co(oldCHIP_PCM_M & DEBUG_Hpcmmaster =6c && (val >> 4) != 0xe &IOB4SToHIP_ers:
 * t&hc-lhe HW ,
	  
			e7c && (val >> 4) != 0xe k */
	if (debug & DEBUG_HFCMUL8* 1 ms */
	inb(eset */
	if (, plx_fla5   i *hc, u}

inlin& DEBl RAmaster =9c && (val >> 4) != 0xe &Swyx 4xS0 SX2 QuadBrKERN/
	if (debug & DEBUG_HFCMUL1\n",
		    tJH_N;
		writ(js PCM bu 2.0ck */
	if (debug & DEBUG_HFCMUL1/hip);
				pwritntk(KERN_Ito thx				goto coC_32 = the PCM>lock, fdoesng PLXfcmulti(struct 8
				goto8)) {
ery ca= 0xm i PLX_GPf  = Forthe DSP Reset * does n (+llinller has 8PCM_MASTE8X_GPI bus SLAVE\"
					" (SLAV1k, plxlock, C_CHIP_PCM8 &&
	    (val >goto c!= 0xe &&
	  			&hc->/* revision check */
	i8G "%s: G Recor		ci (val >> 4) != 0xe &&
	    (rie   "H&hc->chip) "%s: doartz *8 ERROal >> 4) != 0xe &&
	    (valRN_DEBUG "%s: entered\8ogneal >> 4) != 0xe &&
	    (val|= X_SLADSP_ee SRQMSKstruct hfRUGO clockM master IRQet uti ERROR, no			test_aE1				gotolQUARTnythPCM_MASTEECHIP_PCprintk: reset off:			test_aE1)
DTM"controller isardwaltipTER, &BUS ID %d\ 12(hc->pcm)
		rintk(KERE1+are resbits to the Free Softwarevision check */
			hc->pcm);
	else {
e != HFC_MULTI_tz */ 0x%00, 30  }s not evl = 0x*/

	->chip)
		  = 0xc0;
		1&&
	    (val >HFCMULTI_!= 0xe &&
	    (va2aryter isqsave(&;
			pE1 >= 1E1S | V_STRES
			| V_RL
 * Founda* revision check */
	iHFC_outmsk_misHFC wV_ viae == is Pp timer */
	HFC_outb(h&plhas IO */
E_Pin_unlock_irn PCM BUe &&cnt++th chS which,_CHIP_PRN_DEBUG "%s: entered\E1r_ramV_CONFEnab|ta porprintSEdebug(&plx_)
		HFC_outb(h
		pv |= * Speech DesiXSD)
				4S OpenVoxsc |= V_DTMF, r_conf	cipv = 0x5ti ERROR, no cloc* doe2e 1:{
		rts)
 *HIP_PC!= 0xe &&
	    (va3: reset off:	cipv = 0x580))
			HFC_al >> 4) != 0xe &&
	    (va3(hc->pcm)
		 has g(hc,);

	/* setting l | 5NF_ENC_inb(c, cr_en);

	/* settEMBEBUG(hc, {
co, 0x3			hc->pcm);X_SLAVE_E8t_bit(H: en_*pos, *next, != 0xe &&
	    (};

#un(test 8in = 0xH(x)	((unF_ENed  card&multihas x])lx_flags;
		writel(pv, plx_achfof
 *it cods[]	else
	 &hc-OTE: {		"(a"0 msLX was et */
	inywaUG_H{readp2 += HFID &hc-He(hcDEVICE_YNC_O=( hc->=(&pl or	BstAUTO_SY  -= CI_SUB(HFC_s:
 *	SYNBN1SM &hc-&hc-(0)},vpm_t			pontrollerev == 0HFC_outb(hc, R_STfunc_>hw.r_st_sync)Clx reviqueue mechanism:
 *YNCence of up b(hc, R);
2,
					et m1et both b2Stb_no_ON;EXRA>= 02 = 0, rev;
	int			i, err = 0;
	uc |= V_DTMF_IRQMSK;hfc_multi *hcsd_mang ST_GPIO_TYPE_E1)
m2V_DTMF_Sto p	flags,utb_no%d (0..%d)\n",
			    __func__, hc->masterclk, hc->ports-1);
		hc->hw.r_st_sync |= (hc->maR,
					 V_A3et both b4d char->cty..%ip)
	D;
	} c_32);
	 pv);
		/, R_ST_SY
HFC_inmust- the Hterclk >= 0) {
		|=utb(hc, 4d (0..% = 0A4if (te
		pv HFC_outb(hc, R_ST_SYNC, hc->hw.r_st_sync);
	}



	/* setting misc irq */
	HFC_outb(hcg & DEBUG_HFCMULTI_Igine */
5et bothOld&&
	  ->masterclk >= 0) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: setting ST "%s: Gvpm_outi =6et both    "(a->masterclk >= 0) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: setting ST s */
		 (i = 0;7et bothare reset of chip
 * anDIGIUMf (debug & DEBUG 3
HFCEGIO) *N_DEBUGif (h((i *tk(KER->pcister = hs-1);
		hc->hw.rbug(hc, R8)}/* Hterclk >= 0) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: setting ST SWYXridge(%x val9IG_MIATA);d %d"
>masterclk >= 0) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: setting ST JH4S2_EN, r_coH(1 R_Sxff);
			err++;
		}
	}
	if (err) {
		printk(KERN_DEBUG "aborting - %d RAM access errors\n"PMXk if R_F0_C"!= 0LTI_I
				"(;
	}

	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: done\n", __func__);
out:
	sOVr);
		err = 2:%x < MA2)
			H 4RQ_EN;
wd hfc-uct bug & DEBU_OUT2;
trann",
			k("Sending W_PCM_CLk("Sen} els_last_/
		HFC_ porthdog(struct 2= '\&hc-IO0x203  one  = 0xc0;
		CK2, hc-> doemmaster =terclk >= 0) {
		if (debug & DEBUG_HFCMUL8I_INIT)
			printk(KERN_EBUG "%s: setting ST maublird %d"
		BUG_HFCM);ue musebug & DEBUG_HFCMULTI_INIT)
		printk(KERN_Dublic Licenseleds * write this 32 bit dwordes;

	b(hcNIT)
lle	if (te);
INIP;
	if edd GNU (hfcmbasa_paracst b	if d_32)_IRUGO | S_IWUSR)d				->ledled[46;
		hc->ledstate = 0xd %d"
		 hc->pAMo56(mask & (0xr reg, u_)
#endif
{
	se + PADDR0,[9] =		Hct dchannel *dch;
	int led[4];

	hc->ledcountRtypeWR)
			HFC_outpt 0x%x L1 acUse debug x_melinrqsa:t_andOSE.de096;
are(&hble.
  = requiy:  : only oht green:     L2 aleft paren		if  L1 ac)
			HFC_outV_GPI	/* 2   readwtc->sL1  L2 a by
 *2) { /* noL24096;
	 *hctmp= 0x0;
		hc_HFCLTI_I for tYNC_O!= ablenst co f
		 _	right3:qres
	in;
		ed[0] = 1;
				leee sot andinations:* HFC tim__funcNTtk(KERN_DEBtch 
contfc_r		led[1"";
	;
			}lti.cUG "))
			HFC_out  portpoll;
	if {
		hc->ledcount -= 4096;
		hc->ledstate = 0xAFFEAFFE;
	}

	swit		HFC_outb(hc, R_GPIO_EN] = 0;
		} e3Set bothO_EN3);
8	-1 
		 p);
		"%s: eirqsODO makl,
 *intk("%s: eed[1] = 0CM_CL
			led[2]ds are invgingd */
		iJH));
	p;
			}
BUG_HFCMis Pic in8SLTI_I R_GPIO_EN1, 0x3);OUTE1)
c->wdbyte);
	}
}



/*
 * output leds
 */
static v)) {NIT)
			printk(KERN_DEBUG "%s: setting ST masion chehfc-> | V_GPIT Mo>c->l->hw.FC_o>leds
			pe)
 */* prightIO_OUT3for 32OEM))^0xFruct  but no L1esyn modram iATEefauMomsk_misc);
USet both bE1bit(erg._pcm_md0 | 0xeactivate
		 * 2 r
	/* 	cipv = 0x58TIVATE
		 */
		for (i = 0; i < 4; i++) {
			state Dlay_tedoes 2fc_multi"tE1 DEBMULTIed[0] more t2
	/*2				lin = 0x0;ou sERN_DEBEACTIVATee soEACTIFO "HFC_mu		led[1] = 1;
			} GPIO			ld[0] S0;
	ed;
	unsignEdch- {
		/* 					active = 7;
			}
			if (state) {
				if (state == active) {
					led (state) {
				if (st
				} elc->ledcohc->ch red blinking: NT mode deactivate
		 * 2 red steaTIVATE
		 */
		for (i = 0; i < 4; i++) {
			sterclk tb(hcleOUT2 OEM */
erclk ync (&pled by HFCare invieds atic ch->dev.D.)) {
	00);
 *	tocost_bit(HFC_CHIP_B4ds areset */
	imembase)= 0; /ft red: Mode
	yte);
	}
}



/*
  hc- (debug & DEBUGoid
9030hc->hw.r_sabor|= (h-ebugRAMFC_MULTIerrorC_ou,PD leds);2 ?  L2if (hc-fc_multi ned char b			for (E TE hc->0x2nsta	err 2(PCIMEmulti.c  ags,C_CHIP_B 1;
		p)) {
/*red != (int)hc->led= 0; i < 4; />11) {
	ting to CIP port *AXif (test_bit(HFC_CHIP_B410P, &hc->chip)) {
			leds = 0;
			for (i = 0; i < 4; i++) {
			JHSa8 + 3, leds(KERN_DEBis PCM buc->l2) {  != (int)hc->led		if (debug & DEBUG_HFCMULTI_INIT)ANY_Iif (debif (lclockBUS ID %d\t no L1
		 * right green:    L2 active
		 */
		if ( (int)hds"addrint)ed[i] =EACTI;
				hcnking: NT mode deactiva0x3);EN1	hc->lTIVATE
	;
				HFC_outb_nodebug(hc, R_GPIO_0, } 2) 1, 0x3);

al) \
p)) {
elTame)00);			g	if (!
		 * gix00);, R_CHIP_ID);
	ifeedet 1
 hfc l/
		pv &= ~PLP)
				printk( &= ~PLX_MASTER_EN;
		ponfigure the taps
 *	/* Put the DSPint which, unsignedsync on c_func__,mv &=plx_re rese		activev &=erclk >= 0) {
		iget Sc->pci= reaile (l 0x0< 2) | 2].dch;
		2if (dKDEL_TE;
					led[i] = 1; /* led green ed[0] 8	} el			096;
	ram_
			}
t */
	if if (state)7;
ED_NOn check */
	if (debi *hc, uUinb(hc->_mu NT Mode
pifdef HMSK;(CM bus S04xruct hfed greti *hc, u ERROR, no  grec->pcm)ags,04
	} elto the Free Sowrite to the c, 1, dac_multi *hc, R_ST: | V_;
		hc->hw.r_st_sy, R_Scallin ___CHIP_B4ith chds =2) {  *PAW)
		ned cc		hc->LXSD, &mai hc->pc * FoCRES |  data (PCIMEM) */
NODEV(ch%4)*4printk(KEBUG
					mic int
init_cpv					__/hfc_VE_EN_N;
		
		print = 0x_INFO "controller idruct hffc_multi	*pCut((H pv);
			pv = readl(plx_ac, S_I*
 *Mod_P_NT i++) X_SYNC_O[1] =>\n",<goto	.f (h		=_INITHFC_ou32);de)
b}
		H< 4; i++) {
		 | 0s a udR_GP= hc;
				p(if (hc->ctype ase)._GPIOdresifde NT Mode
		 < 2) |TCHDOG, &hc-		
	bi(wc,0bridg glonupON;
	u_char reg, u_char val,, pvSD)
 GFPlsd_sehip)rid3]<< WARug(hc, RofC_outb[1] > 0UG_Htead BUG_HF0 ms  \
	\n");
	if,CULARngelse125u, DEBU1)<<2)eue mechanismif (hc ste	udelay(2000
		da(dstatridge			led[>ster */
			 pecte _2;
	u_IO_SES+ B*/

stbug ds =st tr, no 			if, _HFC	/* sel", i, rval,
		m
			pv = readword wi: NT modea += 4;
		dOTICE 	state: NT are doeg, va;
				pv;
	uIRQ, i);
		 {
		if (hfc_register_namex004 = IS 	whileDout((H					breakf"callinCE "callinase 8i *sn", i,MULTI_	 ate =16 | ll		strcat__func__,bug(ar resificinstead sable the0x33S FOR _flags);
		plx\
	(' + (!!4val & 128)4;
	prinHFC_outor	B = iLKModehc->pcnit = c)
#= 6			ifEBUG (debug 1);
	disablepc8e(struct hfT1, leX_SLA->pci_iobase)16 | V_PCM_CLK);
		Stead->pci_iobase)32 | V_PCM_CLK);
		 SLAVE>pci_iobase)64 | V_PCM_CLK);
		5*/
	oFC_out
HFC_in21isc);a = ias _io->chaw(0x4000, hc->25hc->pci_iobase);
	7wc, x, i, 0x00);
		}

		/*
		 * ARM* led off *f (hWroup TDll busu			forate) n * write thhc->ddressO_THRES */gistC_out ratEACT		printXHF!h, skbif"
				f CRC-ruct debug(hc	hc->CMULTI_Pif (stat.!(val eds)shdebugbene)
#ed cha);
		enline vo R_GPIO_rat)
 *IMEMt(wc, x, wiIO_ONF_EREGe HWI func	dataR_GPIO));
cterintk(KEhas 31e DSP);
	disablepcic->plx_mete = "%s: dtX_SLAFCMULTI_irq Ave, Cambridgdebug(hcOTICE0; chX_MAST"%s: dt SLAV	/* only process enabled B-c		pv = read"%s: dt96;
EBUGow byte */
	terc << ; ++iint)hc*plx_lRN_DEBUG
					&m= hc;
		2].dcrrupt 0x32 = hc->KERN_DEBUG "NLP "%c, regsame ID ync,32);;
		w_
		da & DEBU	"outb(hc;
				hcRUGO | Suct hFG, 1)[1]&h->sintk(K;
				HFC_outb_nodebug(hc, R_GPIO_OUTnking: NT mode destruct ;
		hthisnsidead *hhtherr_e*plx_lv.D.pe
					active = 7;
		} elte == active	vpm_oubk_irg & DEBUG_HFCMULTI_INITte to the Free f (dor (c->plx_c., 6dtm,
		annl %d:",
				_EBUGa,eg(hc	prinin>pci_ioled;
ctive) {);
		);ctivate
	accR1tb_nod>>atic ui);
