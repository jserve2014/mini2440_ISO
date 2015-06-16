/*
 * Device driver for the SYMBIOS/LSILOGIC 53C8XX and 53C1010 family 
 * of PCI-SCSI IO processors.
 *
 * Copyright (C) 1999-2001  Gerard Roudier <groudier@free.fr>
 * Copyright (c) 2003-2005  Matthew Wilcox <matthew@wil.cx>
 *
 * This driver is derived from the Linux sym53c8xx driver.
 * Copyright (C) 1998-2000  Gerard Roudier
 *
 * The sym53c8xx driver is derived from the ncr53c8xx driver that had been 
 * a port of the FreeBSD ncr driver to Linux-1.2.13.
 *
 * The original ncr driver has been written for 386bsd and FreeBSD by
 *         Wolfgang Stanglmeier        <wolf@cologne.de>
 *         Stefan Esser                <se@mi.Uni-Koeln.de>
 * Copyright (C) 1994  Wolfgang Stanglmeier
 *
 * Other major contributions:
 *
 * NVRAM detection and reading.
 * Copyright (C) 1997 Richard Waltham <dormouse@farsrobt.demon.co.uk>
 *
 *-----------------------------------------------------------------------------
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/slab.h>
#include <asm/param.h>		/* for timeouts in units of HZ */

#include "sym_glue.h"
#include "sym_nvram.h"

#if 0
#define SYM_DEBUG_GENERIC_SUPPORT
#endif

/*
 *  Needed function prototypes.
 */
static void sym_int_ma (struct sym_hcb *np);
static void sym_int_sir(struct sym_hcb *);
static struct sym_ccb *sym_alloc_ccb(struct sym_hcb *np);
static struct sym_ccb *sym_ccb_from_dsa(struct sym_hcb *np, u32 dsa);
static void sym_alloc_lcb_tags (struct sym_hcb *np, u_char tn, u_char ln);
static void sym_complete_error (struct sym_hcb *np, struct sym_ccb *cp);
static void sym_complete_ok (struct sym_hcb *np, struct sym_ccb *cp);
static int sym_compute_residual(struct sym_hcb *np, struct sym_ccb *cp);

/*
 *  Print a buffer in hexadecimal format with a ".\n" at end.
 */
static void sym_printl_hex(u_char *p, int n)
{
	while (n-- > 0)
		printf (" %x", *p++);
	printf (".\n");
}

static void sym_print_msg(struct sym_ccb *cp, char *label, u_char *msg)
{
	if (label)
		sym_print_addr(cp->cmd, "%s: ", label);
	else
		sym_print_addr(cp->cmd, "");

	spi_print_msg(msg);
	printf("\n");
}

static void sym_print_nego_msg(struct sym_hcb *np, int target, char *label, u_char *msg)
{
	struct sym_tcb *tp = &np->target[target];
	dev_info(&tp->starget->dev, "%s: ", label);

	spi_print_msg(msg);
	printf("\n");
}

/*
 *  Print something that tells about extended errors.
 */
void sym_print_xerr(struct scsi_cmnd *cmd, int x_status)
{
	if (x_status & XE_PARITY_ERR) {
		sym_print_addr(cmd, "unrecovered SCSI parity error.\n");
	}
	if (x_status & XE_EXTRA_DATA) {
		sym_print_addr(cmd, "extraneous data discarded.\n");
	}
	if (x_status & XE_BAD_PHASE) {
		sym_print_addr(cmd, "illegal scsi phase (4/5).\n");
	}
	if (x_status & XE_SODL_UNRUN) {
		sym_print_addr(cmd, "ODD transfer in DATA OUT phase.\n");
	}
	if (x_status & XE_SWIDE_OVRUN) {
		sym_print_addr(cmd, "ODD transfer in DATA IN phase.\n");
	}
}

/*
 *  Return a string for SCSI BUS mode.
 */
static char *sym_scsi_bus_mode(int mode)
{
	switch(mode) {
	case SMODE_HVD:	return "HVD";
	case SMODE_SE:	return "SE";
	case SMODE_LVD: return "LVD";
	}
	return "??";
}

/*
 *  Soft reset the chip.
 *
 *  Raising SRST when the chip is running may cause 
 *  problems on dual function chips (see below).
 *  On the other hand, LVD devices need some delay 
 *  to settle and report actual BUS mode in STEST4.
 */
static void sym_chip_reset (struct sym_hcb *np)
{
	OUTB(np, nc_istat, SRST);
	INB(np, nc_mbox1);
	udelay(10);
	OUTB(np, nc_istat, 0);
	INB(np, nc_mbox1);
	udelay(2000);	/* For BUS MODE to settle */
}

/*
 *  Really soft reset the chip.:)
 *
 *  Some 896 and 876 chip revisions may hang-up if we set 
 *  the SRST (soft reset) bit at the wrong time when SCRIPTS 
 *  are running.
 *  So, we need to abort the current operation prior to 
 *  soft resetting the chip.
 */
static void sym_soft_reset (struct sym_hcb *np)
{
	u_char istat = 0;
	int i;

	if (!(np->features & FE_ISTAT1) || !(INB(np, nc_istat1) & SCRUN))
		goto do_chip_reset;

	OUTB(np, nc_istat, CABRT);
	for (i = 100000 ; i ; --i) {
		istat = INB(np, nc_istat);
		if (istat & SIP) {
			INW(np, nc_sist);
		}
		else if (istat & DIP) {
			if (INB(np, nc_dstat) & ABRT)
				break;
		}
		udelay(5);
	}
	OUTB(np, nc_istat, 0);
	if (!i)
		printf("%s: unable to abort current chip operation, "
		       "ISTAT=0x%02x.\n", sym_name(np), istat);
do_chip_reset:
	sym_chip_reset(np);
}

/*
 *  Start reset process.
 *
 *  The interrupt handler will reinitialize the chip.
 */
static void sym_start_reset(struct sym_hcb *np)
{
	sym_reset_scsi_bus(np, 1);
}
 
int sym_reset_scsi_bus(struct sym_hcb *np, int enab_int)
{
	u32 term;
	int retv = 0;

	sym_soft_reset(np);	/* Soft reset the chip */
	if (enab_int)
		OUTW(np, nc_sien, RST);
	/*
	 *  Enable Tolerant, reset IRQD if present and 
	 *  properly set IRQ mode, prior to resetting the bus.
	 */
	OUTB(np, nc_stest3, TE);
	OUTB(np, nc_dcntl, (np->rv_dcntl & IRQM));
	OUTB(np, nc_scntl1, CRST);
	INB(np, nc_mbox1);
	udelay(200);

	if (!SYM_SETUP_SCSI_BUS_CHECK)
		goto out;
	/*
	 *  Check for no terminators or SCSI bus shorts to ground.
	 *  Read SCSI data bus, data parity bits and control signals.
	 *  We are expecting RESET to be TRUE and other signals to be 
	 *  FALSE.
	 */
	term =	INB(np, nc_sstat0);
	term =	((term & 2) << 7) + ((term & 1) << 17);	/* rst sdp0 */
	term |= ((INB(np, nc_sstat2) & 0x01) << 26) |	/* sdp1     */
		((INW(np, nc_sbdl) & 0xff)   << 9)  |	/* d7-0     */
		((INW(np, nc_sbdl) & 0xff00) << 10) |	/* d15-8    */
		INB(np, nc_sbcl);	/* req ack bsy sel atn msg cd io    */

	if (!np->maxwide)
		term &= 0x3ffff;

	if (term != (2<<7)) {
		printf("%s: suspicious SCSI data while resetting the BUS.\n",
			sym_name(np));
		printf("%s: %sdp0,d7-0,rst,req,ack,bsy,sel,atn,msg,c/d,i/o = "
			"0x%lx, expecting 0x%lx\n",
			sym_name(np),
			(np->features & FE_WIDE) ? "dp1,d15-8," : "",
			(u_long)term, (u_long)(2<<7));
		if (SYM_SETUP_SCSI_BUS_CHECK == 1)
			retv = 1;
	}
out:
	OUTB(np, nc_scntl1, 0);
	return retv;
}

/*
 *  Select SCSI clock frequency
 */
static void sym_selectclock(struct sym_hcb *np, u_char scntl3)
{
	/*
	 *  If multiplier not present or not selected, leave here.
	 */
	if (np->multiplier <= 1) {
		OUTB(np, nc_scntl3, scntl3);
		return;
	}

	if (sym_verbose >= 2)
		printf ("%s: enabling clock multiplier\n", sym_name(np));

	OUTB(np, nc_stest1, DBLEN);	   /* Enable clock multiplier */
	/*
	 *  Wait for the LCKFRQ bit to be set if supported by the chip.
	 *  Otherwise wait 50 micro-seconds (at least).
	 */
	if (np->features & FE_LCKFRQ) {
		int i = 20;
		while (!(INB(np, nc_stest4) & LCKFRQ) && --i > 0)
			udelay(20);
		if (!i)
			printf("%s: the chip cannot lock the frequency\n",
				sym_name(np));
	} else {
		INB(np, nc_mbox1);
		udelay(50+10);
	}
	OUTB(np, nc_stest3, HSC);		/* Halt the scsi clock	*/
	OUTB(np, nc_scntl3, scntl3);
	OUTB(np, nc_stest1, (DBLEN|DBLSEL));/* Select clock multiplier	*/
	OUTB(np, nc_stest3, 0x00);		/* Restart scsi clock 	*/
}


/*
 *  Determine the chip's clock frequency.
 *
 *  This is essential for the negotiation of the synchronous 
 *  transfer rate.
 *
 *  Note: we have to return the correct value.
 *  THERE IS NO SAFE DEFAULT VALUE.
 *
 *  Most NCR/SYMBIOS boards are delivered with a 40 Mhz clock.
 *  53C860 and 53C875 rev. 1 support fast20 transfers but 
 *  do not have a clock doubler and so are provided with a 
 *  80 MHz clock. All other fast20 boards incorporate a doubler 
 *  and so should be delivered with a 40 MHz clock.
 *  The recent fast40 chips (895/896/895A/1010) use a 40 Mhz base 
 *  clock and provide a clock quadrupler (160 Mhz).
 */

/*
 *  calculate SCSI clock frequency (in KHz)
 */
static unsigned getfreq (struct sym_hcb *np, int gen)
{
	unsigned int ms = 0;
	unsigned int f;

	/*
	 * Measure GEN timer delay in order 
	 * to calculate SCSI clock frequency
	 *
	 * This code will never execute too
	 * many loop iterations (if DELAY is 
	 * reasonably correct). It could get
	 * too low a delay (too high a freq.)
	 * if the CPU is slow executing the 
	 * loop for some reason (an NMI, for
	 * example). For this reason we will
	 * if multiple measurements are to be 
	 * performed trust the higher delay 
	 * (lower frequency returned).
	 */
	OUTW(np, nc_sien, 0);	/* mask all scsi interrupts */
	INW(np, nc_sist);	/* clear pending scsi interrupt */
	OUTB(np, nc_dien, 0);	/* mask all dma interrupts */
	INW(np, nc_sist);	/* another one, just to be sure :) */
	/*
	 * The C1010-33 core does not report GEN in SIST,
	 * if this interrupt is masked in SIEN.
	 * I don't know yet if the C1010-66 behaves the same way.
	 */
	if (np->features & FE_C10) {
		OUTW(np, nc_sien, GEN);
		OUTB(np, nc_istat1, SIRQD);
	}
	OUTB(np, nc_scntl3, 4);	   /* set pre-scaler to divide by 3 */
	OUTB(np, nc_stime1, 0);	   /* disable general purpose timer */
	OUTB(np, nc_stime1, gen);  /* set to nominal delay of 1<<gen * 125us */
	while (!(INW(np, nc_sist) & GEN) && ms++ < 100000)
		udelay(1000/4);    /* count in 1/4 of ms */
	OUTB(np, nc_stime1, 0);    /* disable general purpose timer */
	/*
	 * Undo C1010-33 specific settings.
	 */
	if (np->features & FE_C10) {
		OUTW(np, nc_sien, 0);
		OUTB(np, nc_istat1, 0);
	}
 	/*
 	 * set prescaler to divide by whatever 0 means
 	 * 0 ought to choose divide by 2, but appears
 	 * to set divide by 3.5 mode in my 53c810 ...
 	 */
 	OUTB(np, nc_scntl3, 0);

  	/*
 	 * adjust for prescaler, and convert into KHz 
  	 */
	f = ms ? ((1 << gen) * (4340*4)) / ms : 0;

	/*
	 * The C1010-33 result is biased by a factor 
	 * of 2/3 compared to earlier chips.
	 */
	if (np->features & FE_C10)
		f = (f * 2) / 3;

	if (sym_verbose >= 2)
		printf ("%s: Delay (GEN=%d): %u msec, %u KHz\n",
			sym_name(np), gen, ms/4, f);

	return f;
}

static unsigned sym_getfreq (struct sym_hcb *np)
{
	u_int f1, f2;
	int gen = 8;

	getfreq (np, gen);	/* throw away first result */
	f1 = getfreq (np, gen);
	f2 = getfreq (np, gen);
	if (f1 > f2) f1 = f2;		/* trust lower result	*/
	return f1;
}

/*
 *  Get/probe chip SCSI clock frequency
 */
static void sym_getclock (struct sym_hcb *np, int mult)
{
	unsigned char scntl3 = np->sv_scntl3;
	unsigned char stest1 = np->sv_stest1;
	unsigned f1;

	np->multiplier = 1;
	f1 = 40000;
	/*
	 *  True with 875/895/896/895A with clock multiplier selected
	 */
	if (mult > 1 && (stest1 & (DBLEN+DBLSEL)) == DBLEN+DBLSEL) {
		if (sym_verbose >= 2)
			printf ("%s: clock multiplier found\n", sym_name(np));
		np->multiplier = mult;
	}

	/*
	 *  If multiplier not found or scntl3 not 7,5,3,
	 *  reset chip and get frequency from general purpose timer.
	 *  Otherwise trust scntl3 BIOS setting.
	 */
	if (np->multiplier != mult || (scntl3 & 7) < 3 || !(scntl3 & 1)) {
		OUTB(np, nc_stest1, 0);		/* make sure doubler is OFF */
		f1 = sym_getfreq (np);

		if (sym_verbose)
			printf ("%s: chip clock is %uKHz\n", sym_name(np), f1);

		if	(f1 <	45000)		f1 =  40000;
		else if (f1 <	55000)		f1 =  50000;
		else				f1 =  80000;

		if (f1 < 80000 && mult > 1) {
			if (sym_verbose >= 2)
				printf ("%s: clock multiplier assumed\n",
					sym_name(np));
			np->multiplier	= mult;
		}
	} else {
		if	((scntl3 & 7) == 3)	f1 =  40000;
		else if	((scntl3 & 7) == 5)	f1 =  80000;
		else 				f1 = 160000;

		f1 /= np->multiplier;
	}

	/*
	 *  Compute controller synchronous parameters.
	 */
	f1		*= np->multiplier;
	np->clock_khz	= f1;
}

/*
 *  Get/probe PCI clock frequency
 */
static int sym_getpciclock (struct sym_hcb *np)
{
	int f = 0;

	/*
	 *  For now, we only need to know about the actual 
	 *  PCI BUS clock frequency for C1010-66 chips.
	 */
#if 1
	if (np->features & FE_66MHZ) {
#else
	if (1) {
#endif
		OUTB(np, nc_stest1, SCLK); /* Use the PCI clock as SCSI clock */
		f = sym_getfreq(np);
		OUTB(np, nc_stest1, 0);
	}
	np->pciclk_khz = f;

	return f;
}

/*
 *  SYMBIOS chip clock divisor table.
 *
 *  Divisors are multiplied by 10,000,000 in order to make 
 *  calculations more simple.
 */
#define _5M 5000000
static const u32 div_10M[] = {2*_5M, 3*_5M, 4*_5M, 6*_5M, 8*_5M, 12*_5M, 16*_5M};

/*
 *  Get clock factor and sync divisor for a given 
 *  synchronous factor period.
 */
static int 
sym_getsync(struct sym_hcb *np, u_char dt, u_char sfac, u_char *divp, u_char *fakp)
{
	u32	clk = np->clock_khz;	/* SCSI clock frequency in kHz	*/
	int	div = np->clock_divn;	/* Number of divisors supported	*/
	u32	fak;			/* Sync factor in sxfer		*/
	u32	per;			/* Period in tenths of ns	*/
	u32	kpc;			/* (per * clk)			*/
	int	ret;

	/*
	 *  Compute the synchronous period in tenths of nano-seconds
	 */
	if (dt && sfac <= 9)	per = 125;
	else if	(sfac <= 10)	per = 250;
	else if	(sfac == 11)	per = 303;
	else if	(sfac == 12)	per = 500;
	else			per = 40 * sfac;
	ret = per;

	kpc = per * clk;
	if (dt)
		kpc <<= 1;

	/*
	 *  For earliest C10 revision 0, we cannot use extra 
	 *  clocks for the setting of the SCSI clocking.
	 *  Note that this limits the lowest sync data transfer 
	 *  to 5 Mega-transfers per second and may result in
	 *  using higher clock divisors.
	 */
#if 1
	if ((np->features & (FE_C10|FE_U3EN)) == FE_C10) {
		/*
		 *  Look for the lowest clock divisor that allows an 
		 *  output speed not faster than the period.
		 */
		while (div > 0) {
			--div;
			if (kpc > (div_10M[div] << 2)) {
				++div;
				break;
			}
		}
		fak = 0;			/* No extra clocks */
		if (div == np->clock_divn) {	/* Are we too fast ? */
			ret = -1;
		}
		*divp = div;
		*fakp = fak;
		return ret;
	}
#endif

	/*
	 *  Look for the greatest clock divisor that allows an 
	 *  input speed faster than the period.
	 */
	while (div-- > 0)
		if (kpc >= (div_10M[div] << 2)) break;

	/*
	 *  Calculate the lowest clock factor that allows an output 
	 *  speed not faster than the period, and the max output speed.
	 *  If fak >= 1 we will set both XCLKH_ST and XCLKH_DT.
	 *  If fak >= 2 we will also set XCLKS_ST and XCLKS_DT.
	 */
	if (dt) {
		fak = (kpc - 1) / (div_10M[div] << 1) + 1 - 2;
		/* ret = ((2+fak)*div_10M[div])/np->clock_khz; */
	} else {
		fak = (kpc - 1) / div_10M[div] + 1 - 4;
		/* ret = ((4+fak)*div_10M[div])/np->clock_khz; */
	}

	/*
	 *  Check against our hardware limits, or bugs :).
	 */
	if (fak > 2) {
		fak = 2;
		ret = -1;
	}

	/*
	 *  Compute and return sync parameters.
	 */
	*divp = div;
	*fakp = fak;

	return ret;
}

/*
 *  SYMBIOS chips allow burst lengths of 2, 4, 8, 16, 32, 64,
 *  128 transfers. All chips support at least 16 transfers 
 *  bursts. The 825A, 875 and 895 chips support bursts of up 
 *  to 128 transfers and the 895A and 896 support bursts of up
 *  to 64 transfers. All other chips support up to 16 
 *  transfers bursts.
 *
 *  For PCI 32 bit data transfers each transfer is a DWORD.
 *  It is a QUADWORD (8 bytes) for PCI 64 bit data transfers.
 *
 *  We use log base 2 (burst length) as internal code, with 
 *  value 0 meaning "burst disabled".
 */

/*
 *  Burst length from burst code.
 */
#define burst_length(bc) (!(bc))? 0 : 1 << (bc)

/*
 *  Burst code from io register bits.
 */
#define burst_code(dmode, ctest4, ctest5) \
	(ctest4) & 0x80? 0 : (((dmode) & 0xc0) >> 6) + ((ctest5) & 0x04) + 1

/*
 *  Set initial io register bits from burst code.
 */
static inline void sym_init_burst(struct sym_hcb *np, u_char bc)
{
	np->rv_ctest4	&= ~0x80;
	np->rv_dmode	&= ~(0x3 << 6);
	np->rv_ctest5	&= ~0x4;

	if (!bc) {
		np->rv_ctest4	|= 0x80;
	}
	else {
		--bc;
		np->rv_dmode	|= ((bc & 0x3) << 6);
		np->rv_ctest5	|= (bc & 0x4);
	}
}

/*
 *  Save initial settings of some IO registers.
 *  Assumed to have been set by BIOS.
 *  We cannot reset the chip prior to reading the 
 *  IO registers, since informations will be lost.
 *  Since the SCRIPTS processor may be running, this 
 *  is not safe on paper, but it seems to work quite 
 *  well. :)
 */
static void sym_save_initial_setting (struct sym_hcb *np)
{
	np->sv_scntl0	= INB(np, nc_scntl0) & 0x0a;
	np->sv_scntl3	= INB(np, nc_scntl3) & 0x07;
	np->sv_dmode	= INB(np, nc_dmode)  & 0xce;
	np->sv_dcntl	= INB(np, nc_dcntl)  & 0xa8;
	np->sv_ctest3	= INB(np, nc_ctest3) & 0x01;
	np->sv_ctest4	= INB(np, nc_ctest4) & 0x80;
	np->sv_gpcntl	= INB(np, nc_gpcntl);
	np->sv_stest1	= INB(np, nc_stest1);
	np->sv_stest2	= INB(np, nc_stest2) & 0x20;
	np->sv_stest4	= INB(np, nc_stest4);
	if (np->features & FE_C10) {	/* Always large DMA fifo + ultra3 */
		np->sv_scntl4	= INB(np, nc_scntl4);
		np->sv_ctest5	= INB(np, nc_ctest5) & 0x04;
	}
	else
		np->sv_ctest5	= INB(np, nc_ctest5) & 0x24;
}

/*
 *  Set SCSI BUS mode.
 *  - LVD capable chips (895/895A/896/1010) report the current BUS mode
 *    through the STEST4 IO register.
 *  - For previous generation chips (825/825A/875), the user has to tell us
 *    how to check against HVD, since a 100% safe algorithm is not possible.
 */
static void sym_set_bus_mode(struct sym_hcb *np, struct sym_nvram *nvram)
{
	if (np->scsi_mode)
		return;

	np->scsi_mode = SMODE_SE;
	if (np->features & (FE_ULTRA2|FE_ULTRA3))
		np->scsi_mode = (np->sv_stest4 & SMODE);
	else if	(np->features & FE_DIFF) {
		if (SYM_SETUP_SCSI_DIFF == 1) {
			if (np->sv_scntl3) {
				if (np->sv_stest2 & 0x20)
					np->scsi_mode = SMODE_HVD;
			} else if (nvram->type == SYM_SYMBIOS_NVRAM) {
				if (!(INB(np, nc_gpreg) & 0x08))
					np->scsi_mode = SMODE_HVD;
			}
		} else if (SYM_SETUP_SCSI_DIFF == 2)
			np->scsi_mode = SMODE_HVD;
	}
	if (np->scsi_mode == SMODE_HVD)
		np->rv_stest2 |= 0x20;
}

/*
 *  Prepare io register values used by sym_start_up() 
 *  according to selected and supported features.
 */
static int sym_prepare_setting(struct Scsi_Host *shost, struct sym_hcb *np, struct sym_nvram *nvram)
{
	struct sym_data *sym_data = shost_priv(shost);
	struct pci_dev *pdev = sym_data->pdev;
	u_char	burst_max;
	u32	period;
	int i;

	np->maxwide = (np->features & FE_WIDE) ? 1 : 0;

	/*
	 *  Guess the frequency of the chip's clock.
	 */
	if	(np->features & (FE_ULTRA3 | FE_ULTRA2))
		np->clock_khz = 160000;
	else if	(np->features & FE_ULTRA)
		np->clock_khz = 80000;
	else
		np->clock_khz = 40000;

	/*
	 *  Get the clock multiplier factor.
 	 */
	if	(np->features & FE_QUAD)
		np->multiplier	= 4;
	else if	(np->features & FE_DBLR)
		np->multiplier	= 2;
	else
		np->multiplier	= 1;

	/*
	 *  Measure SCSI clock frequency for chips 
	 *  it may vary from assumed one.
	 */
	if (np->features & FE_VARCLK)
		sym_getclock(np, np->multiplier);

	/*
	 * Divisor to be used for async (timer pre-scaler).
	 */
	i = np->clock_divn - 1;
	while (--i >= 0) {
		if (10ul * SYM_CONF_MIN_ASYNC * np->clock_khz > div_10M[i]) {
			++i;
			break;
		}
	}
	np->rv_scntl3 = i+1;

	/*
	 * The C1010 uses hardwired divisors for async.
	 * So, we just throw away, the async. divisor.:-)
	 */
	if (np->features & FE_C10)
		np->rv_scntl3 = 0;

	/*
	 * Minimum synchronous period factor supported by the chip.
	 * Btw, 'period' is in tenths of nanoseconds.
	 */
	period = (4 * div_10M[0] + np->clock_khz - 1) / np->clock_khz;

	if	(period <= 250)		np->minsync = 10;
	else if	(period <= 303)		np->minsync = 11;
	else if	(period <= 500)		np->minsync = 12;
	else				np->minsync = (period + 40 - 1) / 40;

	/*
	 * Check against chip SCSI standard support (SCSI-2,ULTRA,ULTRA2).
	 */
	if	(np->minsync < 25 &&
		 !(np->features & (FE_ULTRA|FE_ULTRA2|FE_ULTRA3)))
		np->minsync = 25;
	else if	(np->minsync < 12 &&
		 !(np->features & (FE_ULTRA2|FE_ULTRA3)))
		np->minsync = 12;

	/*
	 * Maximum synchronous period factor supported by the chip.
	 */
	period = (11 * div_10M[np->clock_divn - 1]) / (4 * np->clock_khz);
	np->maxsync = period > 2540 ? 254 : period / 10;

	/*
	 * If chip is a C1010, guess the sync limits in DT mode.
	 */
	if ((np->features & (FE_C10|FE_ULTRA3)) == (FE_C10|FE_ULTRA3)) {
		if (np->clock_khz == 160000) {
			np->minsync_dt = 9;
			np->maxsync_dt = 50;
			np->maxoffs_dt = nvram->type ? 62 : 31;
		}
	}
	
	/*
	 *  64 bit addressing  (895A/896/1010) ?
	 */
	if (np->features & FE_DAC) {
		if (!use_dac(np))
			np->rv_ccntl1 |= (DDAC);
		else if (SYM_CONF_DMA_ADDRESSING_MODE == 1)
			np->rv_ccntl1 |= (XTIMOD | EXTIBMV);
		else if (SYM_CONF_DMA_ADDRESSING_MODE == 2)
			np->rv_ccntl1 |= (0 | EXTIBMV);
	}

	/*
	 *  Phase mismatch handled by SCRIPTS (895A/896/1010) ?
  	 */
	if (np->features & FE_NOPM)
		np->rv_ccntl0	|= (ENPMJ);

 	/*
	 *  C1010-33 Errata: Part Number:609-039638 (rev. 1) is fixed.
	 *  In dual channel mode, contention occurs if internal cycles
	 *  are used. Disable internal cycles.
	 */
	if (pdev->device == PCI_DEVICE_ID_LSI_53C1010_33 &&
	    pdev->revision < 0x1)
		np->rv_ccntl0	|=  DILS;

	/*
	 *  Select burst length (dwords)
	 */
	burst_max	= SYM_SETUP_BURST_ORDER;
	if (burst_max == 255)
		burst_max = burst_code(np->sv_dmode, np->sv_ctest4,
				       np->sv_ctest5);
	if (burst_max > 7)
		burst_max = 7;
	if (burst_max > np->maxburst)
		burst_max = np->maxburst;

	/*
	 *  DEL 352 - 53C810 Rev x11 - Part Number 609-0392140 - ITEM 2.
	 *  This chip and the 860 Rev 1 may wrongly use PCI cache line 
	 *  based transactions on LOAD/STORE instructions. So we have 
	 *  to prevent these chips from using such PCI transactions in 
	 *  this driver. The generic ncr driver that does not use 
	 *  LOAD/STORE instructions does not need this work-around.
	 */
	if ((pdev->device == PCI_DEVICE_ID_NCR_53C810 &&
	     pdev->revision >= 0x10 && pdev->revision <= 0x11) ||
	    (pdev->device == PCI_DEVICE_ID_NCR_53C860 &&
	     pdev->revision <= 0x1))
		np->features &= ~(FE_WRIE|FE_ERL|FE_ERMP);

	/*
	 *  Select all supported special features.
	 *  If we are using on-board RAM for scripts, prefetch (PFEN) 
	 *  does not help, but burst op fetch (BOF) does.
	 *  Disabling PFEN makes sure BOF will be used.
	 */
	if (np->features & FE_ERL)
		np->rv_dmode	|= ERL;		/* Enable Read Line */
	if (np->features & FE_BOF)
		np->rv_dmode	|= BOF;		/* Burst Opcode Fetch */
	if (np->features & FE_ERMP)
		np->rv_dmode	|= ERMP;	/* Enable Read Multiple */
#if 1
	if ((np->features & FE_PFEN) && !np->ram_ba)
#else
	if (np->features & FE_PFEN)
#endif
		np->rv_dcntl	|= PFEN;	/* Prefetch Enable */
	if (np->features & FE_CLSE)
		np->rv_dcntl	|= CLSE;	/* Cache Line Size Enable */
	if (np->features & FE_WRIE)
		np->rv_ctest3	|= WRIE;	/* Write and Invalidate */
	if (np->features & FE_DFS)
		np->rv_ctest5	|= DFS;		/* Dma Fifo Size */

	/*
	 *  Select some other
	 */
	np->rv_ctest4	|= MPEE; /* Master parity checking */
	np->rv_scntl0	|= 0x0a; /*  full arb., ena parity, par->ATN  */

	/*
	 *  Get parity checking, host ID and verbose mode from NVRAM
	 */
	np->myaddr = 255;
	np->scsi_mode = 0;
	sym_nvram_setup_host(shost, np, nvram);

	/*
	 *  Get SCSI addr of host adapter (set by bios?).
	 */
	if (np->myaddr == 255) {
		np->myaddr = INB(np, nc_scid) & 0x07;
		if (!np->myaddr)
			np->myaddr = SYM_SETUP_HOST_ID;
	}

	/*
	 *  Prepare initial io register bits for burst length
	 */
	sym_init_burst(np, burst_max);

	sym_set_bus_mode(np, nvram);

	/*
	 *  Set LED support from SCRIPTS.
	 *  Ignore this feature for boards known to use a 
	 *  specific GPIO wiring and for the 895A, 896 
	 *  and 1010 that drive the LED directly.
	 */
	if ((SYM_SETUP_SCSI_LED || 
	     (nvram->type == SYM_SYMBIOS_NVRAM ||
	      (nvram->type == SYM_TEKRAM_NVRAM &&
	       pdev->device == PCI_DEVICE_ID_NCR_53C895))) &&
	    !(np->features & FE_LEDC) && !(np->sv_gpcntl & 0x01))
		np->features |= FE_LED0;

	/*
	 *  Set irq mode.
	 */
	switch(SYM_SETUP_IRQ_MODE & 3) {
	case 2:
		np->rv_dcntl	|= IRQM;
		break;
	case 1:
		np->rv_dcntl	|= (np->sv_dcntl & IRQM);
		break;
	default:
		break;
	}

	/*
	 *  Configure targets according to driver setup.
	 *  If NVRAM present get targets setup from NVRAM.
	 */
	for (i = 0 ; i < SYM_CONF_MAX_TARGET ; i++) {
		struct sym_tcb *tp = &np->target[i];

		tp->usrflags |= (SYM_DISC_ENABLED | SYM_TAGS_ENABLED);
		tp->usrtags = SYM_SETUP_MAX_TAG;
		tp->usr_width = np->maxwide;
		tp->usr_period = 9;

		sym_nvram_setup_target(tp, i, nvram);

		if (!tp->usrtags)
			tp->usrflags &= ~SYM_TAGS_ENABLED;
	}

	/*
	 *  Let user know about the settings.
	 */
	printf("%s: %s, ID %d, Fast-%d, %s, %s\n", sym_name(np),
		sym_nvram_type(nvram), np->myaddr,
		(np->features & FE_ULTRA3) ? 80 : 
		(np->features & FE_ULTRA2) ? 40 : 
		(np->features & FE_ULTRA)  ? 20 : 10,
		sym_scsi_bus_mode(np->scsi_mode),
		(np->rv_scntl0 & 0xa)	? "parity checking" : "NO parity");
	/*
	 *  Tell him more on demand.
	 */
	if (sym_verbose) {
		printf("%s: %s IRQ line driver%s\n",
			sym_name(np),
			np->rv_dcntl & IRQM ? "totem pole" : "open drain",
			np->ram_ba ? ", using on-chip SRAM" : "");
		printf("%s: using %s firmware.\n", sym_name(np), np->fw_name);
		if (np->features & FE_NOPM)
			printf("%s: handling phase mismatch from SCRIPTS.\n", 
			       sym_name(np));
	}
	/*
	 *  And still more.
	 */
	if (sym_verbose >= 2) {
		printf ("%s: initial SCNTL3/DMODE/DCNTL/CTEST3/4/5 = "
			"(hex) %02x/%02x/%02x/%02x/%02x/%02x\n",
			sym_name(np), np->sv_scntl3, np->sv_dmode, np->sv_dcntl,
			np->sv_ctest3, np->sv_ctest4, np->sv_ctest5);

		printf ("%s: final   SCNTL3/DMODE/DCNTL/CTEST3/4/5 = "
			"(hex) %02x/%02x/%02x/%02x/%02x/%02x\n",
			sym_name(np), np->rv_scntl3, np->rv_dmode, np->rv_dcntl,
			np->rv_ctest3, np->rv_ctest4, np->rv_ctest5);
	}

	return 0;
}

/*
 *  Test the pci bus snoop logic :-(
 *
 *  Has to be called with interrupts disabled.
 */
#ifdef CONFIG_SCSI_SYM53C8XX_MMIO
static int sym_regtest(struct sym_hcb *np)
{
	register volatile u32 data;
	/*
	 *  chip registers may NOT be cached.
	 *  write 0xffffffff to a read only register area,
	 *  and try to read it back.
	 */
	data = 0xffffffff;
	OUTL(np, nc_dstat, data);
	data = INL(np, nc_dstat);
#if 1
	if (data == 0xffffffff) {
#else
	if ((data & 0xe2f0fffd) != 0x02000080) {
#endif
		printf ("CACHE TEST FAILED: reg dstat-sstat2 readback %x.\n",
			(unsigned) data);
		return 0x10;
	}
	return 0;
}
#else
static inline int sym_regtest(struct sym_hcb *np)
{
	return 0;
}
#endif

static int sym_snooptest(struct sym_hcb *np)
{
	u32 sym_rd, sym_wr, sym_bk, host_rd, host_wr, pc, dstat;
	int i, err;

	err = sym_regtest(np);
	if (err)
		return err;
restart_test:
	/*
	 *  Enable Master Parity Checking as we intend 
	 *  to enable it for normal operations.
	 */
	OUTB(np, nc_ctest4, (np->rv_ctest4 & MPEE));
	/*
	 *  init
	 */
	pc  = SCRIPTZ_BA(np, snooptest);
	host_wr = 1;
	sym_wr  = 2;
	/*
	 *  Set memory and register.
	 */
	np->scratch = cpu_to_scr(host_wr);
	OUTL(np, nc_temp, sym_wr);
	/*
	 *  Start script (exchange values)
	 */
	OUTL(np, nc_dsa, np->hcb_ba);
	OUTL_DSP(np, pc);
	/*
	 *  Wait 'til done (with timeout)
	 */
	for (i=0; i<SYM_SNOOP_TIMEOUT; i++)
		if (INB(np, nc_istat) & (INTF|SIP|DIP))
			break;
	if (i>=SYM_SNOOP_TIMEOUT) {
		printf ("CACHE TEST FAILED: timeout.\n");
		return (0x20);
	}
	/*
	 *  Check for fatal DMA errors.
	 */
	dstat = INB(np, nc_dstat);
#if 1	/* Band aiding for broken hardwares that fail PCI parity */
	if ((dstat & MDPE) && (np->rv_ctest4 & MPEE)) {
		printf ("%s: PCI DATA PARITY ERROR DETECTED - "
			"DISABLING MASTER DATA PARITY CHECKING.\n",
			sym_name(np));
		np->rv_ctest4 &= ~MPEE;
		goto restart_test;
	}
#endif
	if (dstat & (MDPE|BF|IID)) {
		printf ("CACHE TEST FAILED: DMA error (dstat=0x%02x).", dstat);
		return (0x80);
	}
	/*
	 *  Save termination position.
	 */
	pc = INL(np, nc_dsp);
	/*
	 *  Read memory and register.
	 */
	host_rd = scr_to_cpu(np->scratch);
	sym_rd  = INL(np, nc_scratcha);
	sym_bk  = INL(np, nc_temp);
	/*
	 *  Check termination position.
	 */
	if (pc != SCRIPTZ_BA(np, snoopend)+8) {
		printf ("CACHE TEST FAILED: script execution failed.\n");
		printf ("start=%08lx, pc=%08lx, end=%08lx\n", 
			(u_long) SCRIPTZ_BA(np, snooptest), (u_long) pc,
			(u_long) SCRIPTZ_BA(np, snoopend) +8);
		return (0x40);
	}
	/*
	 *  Show results.
	 */
	if (host_wr != sym_rd) {
		printf ("CACHE TEST FAILED: host wrote %d, chip read %d.\n",
			(int) host_wr, (int) sym_rd);
		err |= 1;
	}
	if (host_rd != sym_wr) {
		printf ("CACHE TEST FAILED: chip wrote %d, host read %d.\n",
			(int) sym_wr, (int) host_rd);
		err |= 2;
	}
	if (sym_bk != sym_wr) {
		printf ("CACHE TEST FAILED: chip wrote %d, read back %d.\n",
			(int) sym_wr, (int) sym_bk);
		err |= 4;
	}

	return err;
}

/*
 *  log message for real hard errors
 *
 *  sym0 targ 0?: ERROR (ds:si) (so-si-sd) (sx/s3/s4) @ name (dsp:dbc).
 *  	      reg: r0 r1 r2 r3 r4 r5 r6 ..... rf.
 *
 *  exception register:
 *  	ds:	dstat
 *  	si:	sist
 *
 *  SCSI bus lines:
 *  	so:	control lines as driven by chip.
 *  	si:	control lines as seen by chip.
 *  	sd:	scsi data lines as seen by chip.
 *
 *  wide/fastmode:
 *  	sx:	sxfer  (see the manual)
 *  	s3:	scntl3 (see the manual)
 *  	s4:	scntl4 (see the manual)
 *
 *  current script command:
 *  	dsp:	script address (relative to start of script).
 *  	dbc:	first word of script command.
 *
 *  First 24 register of the chip:
 *  	r0..rf
 */
static void sym_log_hard_error(struct Scsi_Host *shost, u_short sist, u_char dstat)
{
	struct sym_hcb *np = sym_get_hcb(shost);
	u32	dsp;
	int	script_ofs;
	int	script_size;
	char	*script_name;
	u_char	*script_base;
	int	i;

	dsp	= INL(np, nc_dsp);

	if	(dsp > np->scripta_ba &&
		 dsp <= np->scripta_ba + np->scripta_sz) {
		script_ofs	= dsp - np->scripta_ba;
		script_size	= np->scripta_sz;
		script_base	= (u_char *) np->scripta0;
		script_name	= "scripta";
	}
	else if (np->scriptb_ba < dsp && 
		 dsp <= np->scriptb_ba + np->scriptb_sz) {
		script_ofs	= dsp - np->scriptb_ba;
		script_size	= np->scriptb_sz;
		script_base	= (u_char *) np->scriptb0;
		script_name	= "scriptb";
	} else {
		script_ofs	= dsp;
		script_size	= 0;
		script_base	= NULL;
		script_name	= "mem";
	}

	printf ("%s:%d: ERROR (%x:%x) (%x-%x-%x) (%x/%x/%x) @ (%s %x:%08x).\n",
		sym_name(np), (unsigned)INB(np, nc_sdid)&0x0f, dstat, sist,
		(unsigned)INB(np, nc_socl), (unsigned)INB(np, nc_sbcl),
		(unsigned)INB(np, nc_sbdl), (unsigned)INB(np, nc_sxfer),
		(unsigned)INB(np, nc_scntl3),
		(np->features & FE_C10) ?  (unsigned)INB(np, nc_scntl4) : 0,
		script_name, script_ofs,   (unsigned)INL(np, nc_dbc));

	if (((script_ofs & 3) == 0) &&
	    (unsigned)script_ofs < script_size) {
		printf ("%s: script cmd = %08x\n", sym_name(np),
			scr_to_cpu((int) *(u32 *)(script_base + script_ofs)));
	}

	printf("%s: regdump:", sym_name(np));
	for (i = 0; i < 24; i++)
		printf(" %02x", (unsigned)INB_OFF(np, i));
	printf(".\n");

	/*
	 *  PCI BUS error.
	 */
	if (dstat & (MDPE|BF))
		sym_log_bus_error(shost);
}

void sym_dump_registers(struct Scsi_Host *shost)
{
	struct sym_hcb *np = sym_get_hcb(shost);
	u_short sist;
	u_char dstat;

	sist = INW(np, nc_sist);
	dstat = INB(np, nc_dstat);
	sym_log_hard_error(shost, sist, dstat);
}

static struct sym_chip sym_dev_table[] = {
 {PCI_DEVICE_ID_NCR_53C810, 0x0f, "810", 4, 8, 4, 64,
 FE_ERL}
 ,
#ifdef SYM_DEBUG_GENERIC_SUPPORT
 {PCI_DEVICE_ID_NCR_53C810, 0xff, "810a", 4,  8, 4, 1,
 FE_BOF}
 ,
#else
 {PCI_DEVICE_ID_NCR_53C810, 0xff, "810a", 4,  8, 4, 1,
 FE_CACHE_SET|FE_LDSTR|FE_PFEN|FE_BOF}
 ,
#endif
 {PCI_DEVICE_ID_NCR_53C815, 0xff, "815", 4,  8, 4, 64,
 FE_BOF|FE_ERL}
 ,
 {PCI_DEVICE_ID_NCR_53C825, 0x0f, "825", 6,  8, 4, 64,
 FE_WIDE|FE_BOF|FE_ERL|FE_DIFF}
 ,
 {PCI_DEVICE_ID_NCR_53C825, 0xff, "825a", 6,  8, 4, 2,
 FE_WIDE|FE_CACHE0_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|FE_RAM|FE_DIFF}
 ,
 {PCI_DEVICE_ID_NCR_53C860, 0xff, "860", 4,  8, 5, 1,
 FE_ULTRA|FE_CACHE_SET|FE_BOF|FE_LDSTR|FE_PFEN}
 ,
 {PCI_DEVICE_ID_NCR_53C875, 0x01, "875", 6, 16, 5, 2,
 FE_WIDE|FE_ULTRA|FE_CACHE0_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_DIFF|FE_VARCLK}
 ,
 {PCI_DEVICE_ID_NCR_53C875, 0xff, "875", 6, 16, 5, 2,
 FE_WIDE|FE_ULTRA|FE_DBLR|FE_CACHE0_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_DIFF|FE_VARCLK}
 ,
 {PCI_DEVICE_ID_NCR_53C875J, 0xff, "875J", 6, 16, 5, 2,
 FE_WIDE|FE_ULTRA|FE_DBLR|FE_CACHE0_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_DIFF|FE_VARCLK}
 ,
 {PCI_DEVICE_ID_NCR_53C885, 0xff, "885", 6, 16, 5, 2,
 FE_WIDE|FE_ULTRA|FE_DBLR|FE_CACHE0_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_DIFF|FE_VARCLK}
 ,
#ifdef SYM_DEBUG_GENERIC_SUPPORT
 {PCI_DEVICE_ID_NCR_53C895, 0xff, "895", 6, 31, 7, 2,
 FE_WIDE|FE_ULTRA2|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFS|
 FE_RAM|FE_LCKFRQ}
 ,
#else
 {PCI_DEVICE_ID_NCR_53C895, 0xff, "895", 6, 31, 7, 2,
 FE_WIDE|FE_ULTRA2|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_LCKFRQ}
 ,
#endif
 {PCI_DEVICE_ID_NCR_53C896, 0xff, "896", 6, 31, 7, 4,
 FE_WIDE|FE_ULTRA2|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_RAM8K|FE_64BIT|FE_DAC|FE_IO256|FE_NOPM|FE_LEDC|FE_LCKFRQ}
 ,
 {PCI_DEVICE_ID_LSI_53C895A, 0xff, "895a", 6, 31, 7, 4,
 FE_WIDE|FE_ULTRA2|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_RAM8K|FE_DAC|FE_IO256|FE_NOPM|FE_LEDC|FE_LCKFRQ}
 ,
 {PCI_DEVICE_ID_LSI_53C875A, 0xff, "875a", 6, 31, 7, 4,
 FE_WIDE|FE_ULTRA|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_DAC|FE_IO256|FE_NOPM|FE_LEDC|FE_LCKFRQ}
 ,
 {PCI_DEVICE_ID_LSI_53C1010_33, 0x00, "1010-33", 6, 31, 7, 8,
 FE_WIDE|FE_ULTRA3|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFBC|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_RAM8K|FE_64BIT|FE_DAC|FE_IO256|FE_NOPM|FE_LEDC|FE_CRC|
 FE_C10}
 ,
 {PCI_DEVICE_ID_LSI_53C1010_33, 0xff, "1010-33", 6, 31, 7, 8,
 FE_WIDE|FE_ULTRA3|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFBC|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_RAM8K|FE_64BIT|FE_DAC|FE_IO256|FE_NOPM|FE_LEDC|FE_CRC|
 FE_C10|FE_U3EN}
 ,
 {PCI_DEVICE_ID_LSI_53C1010_66, 0xff, "1010-66", 6, 31, 7, 8,
 FE_WIDE|FE_ULTRA3|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFBC|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_RAM8K|FE_64BIT|FE_DAC|FE_IO256|FE_NOPM|FE_LEDC|FE_66MHZ|FE_CRC|
 FE_C10|FE_U3EN}
 ,
 {PCI_DEVICE_ID_LSI_53C1510, 0xff, "1510d", 6, 31, 7, 4,
 FE_WIDE|FE_ULTRA2|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_IO256|FE_LEDC}
};

#define sym_num_devs (ARRAY_SIZE(sym_dev_table))

/*
 *  Look up the chip table.
 *
 *  Return a pointer to the chip entry if found, 
 *  zero otherwise.
 */
struct sym_chip *
sym_lookup_chip_table (u_short device_id, u_char revision)
{
	struct	sym_chip *chip;
	int	i;

	for (i = 0; i < sym_num_devs; i++) {
		chip = &sym_dev_table[i];
		if (device_id != chip->device_id)
			continue;
		if (revision > chip->revision_id)
			continue;
		return chip;
	}

	return NULL;
}

#if SYM_CONF_DMA_ADDRESSING_MODE == 2
/*
 *  Lookup the 64 bit DMA segments map.
 *  This is only used if the direct mapping 
 *  has been unsuccessful.
 */
int sym_lookup_dmap(struct sym_hcb *np, u32 h, int s)
{
	int i;

	if (!use_dac(np))
		goto weird;

	/* Look up existing mappings */
	for (i = SYM_DMAP_SIZE-1; i > 0; i--) {
		if (h == np->dmap_bah[i])
			return i;
	}
	/* If direct mapping is free, get it */
	if (!np->dmap_bah[s])
		goto new;
	/* Collision -> lookup free mappings */
	for (s = SYM_DMAP_SIZE-1; s > 0; s--) {
		if (!np->dmap_bah[s])
			goto new;
	}
weird:
	panic("sym: ran out of 64 bit DMA segment registers");
	return -1;
new:
	np->dmap_bah[s] = h;
	np->dmap_dirty = 1;
	return s;
}

/*
 *  Update IO registers scratch C..R so they will be 
 *  in sync. with queued CCB expectations.
 */
static void sym_update_dmap_regs(struct sym_hcb *np)
{
	int o, i;

	if (!np->dmap_dirty)
		return;
	o = offsetof(struct sym_reg, nc_scrx[0]);
	for (i = 0; i < SYM_DMAP_SIZE; i++) {
		OUTL_OFF(np, o, np->dmap_bah[i]);
		o += 4;
	}
	np->dmap_dirty = 0;
}
#endif

/* Enforce all the fiddly SPI rules and the chip limitations */
static void sym_check_goals(struct sym_hcb *np, struct scsi_target *starget,
		struct sym_trans *goal)
{
	if (!spi_support_wide(starget))
		goal->width = 0;

	if (!spi_support_sync(starget)) {
		goal->iu = 0;
		goal->dt = 0;
		goal->qas = 0;
		goal->offset = 0;
		return;
	}

	if (spi_support_dt(starget)) {
		if (spi_support_dt_only(starget))
			goal->dt = 1;

		if (goal->offset == 0)
			goal->dt = 0;
	} else {
		goal->dt = 0;
	}

	/* Some targets fail to properly negotiate DT in SE mode */
	if ((np->scsi_mode != SMODE_LVD) || !(np->features & FE_U3EN))
		goal->dt = 0;

	if (goal->dt) {
		/* all DT transfers must be wide */
		goal->width = 1;
		if (goal->offset > np->maxoffs_dt)
			goal->offset = np->maxoffs_dt;
		if (goal->period < np->minsync_dt)
			goal->period = np->minsync_dt;
		if (goal->period > np->maxsync_dt)
			goal->period = np->maxsync_dt;
	} else {
		goal->iu = goal->qas = 0;
		if (goal->offset > np->maxoffs)
			goal->offset = np->maxoffs;
		if (goal->period < np->minsync)
			goal->period = np->minsync;
		if (goal->period > np->maxsync)
			goal->period = np->maxsync;
	}
}

/*
 *  Prepare the next negotiation message if needed.
 *
 *  Fill in the part of message buffer that contains the 
 *  negotiation and the nego_status field of the CCB.
 *  Returns the size of the message in bytes.
 */
static int sym_prepare_nego(struct sym_hcb *np, struct sym_ccb *cp, u_char *msgptr)
{
	struct sym_tcb *tp = &np->target[cp->target];
	struct scsi_target *starget = tp->starget;
	struct sym_trans *goal = &tp->tgoal;
	int msglen = 0;
	int nego;

	sym_check_goals(np, starget, goal);

	/*
	 * Many devices implement PPR in a buggy way, so only use it if we
	 * really want to.
	 */
	if (goal->renego == NS_PPR || (goal->offset &&
	    (goal->iu || goal->dt || goal->qas || (goal->period < 0xa)))) {
		nego = NS_PPR;
	} else if (goal->renego == NS_WIDE || goal->width) {
		nego = NS_WIDE;
	} else if (goal->renego == NS_SYNC || goal->offset) {
		nego = NS_SYNC;
	} else {
		goal->check_nego = 0;
		nego = 0;
	}

	switch (nego) {
	case NS_SYNC:
		msglen += spi_populate_sync_msg(msgptr + msglen, goal->period,
				goal->offset);
		break;
	case NS_WIDE:
		msglen += spi_populate_width_msg(msgptr + msglen, goal->width);
		break;
	case NS_PPR:
		msglen += spi_populate_ppr_msg(msgptr + msglen, goal->period,
				goal->offset, goal->width,
				(goal->iu ? PPR_OPT_IU : 0) |
					(goal->dt ? PPR_OPT_DT : 0) |
					(goal->qas ? PPR_OPT_QAS : 0));
		break;
	}

	cp->nego_status = nego;

	if (nego) {
		tp->nego_cp = cp; /* Keep track a nego will be performed */
		if (DEBUG_FLAGS & DEBUG_NEGO) {
			sym_print_nego_msg(np, cp->target, 
					  nego == NS_SYNC ? "sync msgout" :
					  nego == NS_WIDE ? "wide msgout" :
					  "ppr msgout", msgptr);
		}
	}

	return msglen;
}

/*
 *  Insert a job into the start queue.
 */
void sym_put_start_queue(struct sym_hcb *np, struct sym_ccb *cp)
{
	u_short	qidx;

#ifdef SYM_CONF_IARB_SUPPORT
	/*
	 *  If the previously queued CCB is not yet done, 
	 *  set the IARB hint. The SCRIPTS will go with IARB 
	 *  for this job when starting the previous one.
	 *  We leave devices a chance to win arbitration by 
	 *  not using more than 'iarb_max' consecutive 
	 *  immediate arbitrations.
	 */
	if (np->last_cp && np->iarb_count < np->iarb_max) {
		np->last_cp->host_flags |= HF_HINT_IARB;
		++np->iarb_count;
	}
	else
		np->iarb_count = 0;
	np->last_cp = cp;
#endif

#if   SYM_CONF_DMA_ADDRESSING_MODE == 2
	/*
	 *  Make SCRIPTS aware of the 64 bit DMA 
	 *  segment registers not being up-to-date.
	 */
	if (np->dmap_dirty)
		cp->host_xflags |= HX_DMAP_DIRTY;
#endif

	/*
	 *  Insert first the idle task and then our job.
	 *  The MBs should ensure proper ordering.
	 */
	qidx = np->squeueput + 2;
	if (qidx >= MAX_QUEUE*2) qidx = 0;

	np->squeue [qidx]	   = cpu_to_scr(np->idletask_ba);
	MEMORY_WRITE_BARRIER();
	np->squeue [np->squeueput] = cpu_to_scr(cp->ccb_ba);

	np->squeueput = qidx;

	if (DEBUG_FLAGS & DEBUG_QUEUE)
		scmd_printk(KERN_DEBUG, cp->cmd, "queuepos=%d\n",
							np->squeueput);

	/*
	 *  Script processor may be waiting for reselect.
	 *  Wake it up.
	 */
	MEMORY_WRITE_BARRIER();
	OUTB(np, nc_istat, SIGP|np->istat_sem);
}

#ifdef SYM_OPT_HANDLE_DEVICE_QUEUEING
/*
 *  Start next ready-to-start CCBs.
 */
void sym_start_next_ccbs(struct sym_hcb *np, struct sym_lcb *lp, int maxn)
{
	SYM_QUEHEAD *qp;
	struct sym_ccb *cp;

	/* 
	 *  Paranoia, as usual. :-)
	 */
	assert(!lp->started_tags || !lp->started_no_tag);

	/*
	 *  Try to start as many commands as asked by caller.
	 *  Prevent from having both tagged and untagged 
	 *  commands queued to the device at the same time.
	 */
	while (maxn--) {
		qp = sym_remque_head(&lp->waiting_ccbq);
		if (!qp)
			break;
		cp = sym_que_entry(qp, struct sym_ccb, link2_ccbq);
		if (cp->tag != NO_TAG) {
			if (lp->started_no_tag ||
			    lp->started_tags >= lp->started_max) {
				sym_insque_head(qp, &lp->waiting_ccbq);
				break;
			}
			lp->itlq_tbl[cp->tag] = cpu_to_scr(cp->ccb_ba);
			lp->head.resel_sa =
				cpu_to_scr(SCRIPTA_BA(np, resel_tag));
			++lp->started_tags;
		} else {
			if (lp->started_no_tag || lp->started_tags) {
				sym_insque_head(qp, &lp->waiting_ccbq);
				break;
			}
			lp->head.itl_task_sa = cpu_to_scr(cp->ccb_ba);
			lp->head.resel_sa =
			      cpu_to_scr(SCRIPTA_BA(np, resel_no_tag));
			++lp->started_no_tag;
		}
		cp->started = 1;
		sym_insque_tail(qp, &lp->started_ccbq);
		sym_put_start_queue(np, cp);
	}
}
#endif /* SYM_OPT_HANDLE_DEVICE_QUEUEING */

/*
 *  The chip may have completed jobs. Look at the DONE QUEUE.
 *
 *  On paper, memory read barriers may be needed here to 
 *  prevent out of order LOADs by the CPU from having 
 *  prefetched stale data prior to DMA having occurred.
 */
static int sym_wakeup_done (struct sym_hcb *np)
{
	struct sym_ccb *cp;
	int i, n;
	u32 dsa;

	n = 0;
	i = np->dqueueget;

	/* MEMORY_READ_BARRIER(); */
	while (1) {
		dsa = scr_to_cpu(np->dqueue[i]);
		if (!dsa)
			break;
		np->dqueue[i] = 0;
		if ((i = i+2) >= MAX_QUEUE*2)
			i = 0;

		cp = sym_ccb_from_dsa(np, dsa);
		if (cp) {
			MEMORY_READ_BARRIER();
			sym_complete_ok (np, cp);
			++n;
		}
		else
			printf ("%s: bad DSA (%x) in done queue.\n",
				sym_name(np), (u_int) dsa);
	}
	np->dqueueget = i;

	return n;
}

/*
 *  Complete all CCBs queued to the COMP queue.
 *
 *  These CCBs are assumed:
 *  - Not to be referenced either by devices or 
 *    SCRIPTS-related queues and datas.
 *  - To have to be completed with an error condition 
 *    or requeued.
 *
 *  The device queue freeze count is incremented 
 *  for each CCB that does not prevent this.
 *  This function is called when all CCBs involved 
 *  in error handling/recovery have been reaped.
 */
static void sym_flush_comp_queue(struct sym_hcb *np, int cam_status)
{
	SYM_QUEHEAD *qp;
	struct sym_ccb *cp;

	while ((qp = sym_remque_head(&np->comp_ccbq)) != NULL) {
		struct scsi_cmnd *cmd;
		cp = sym_que_entry(qp, struct sym_ccb, link_ccbq);
		sym_insque_tail(&cp->link_ccbq, &np->busy_ccbq);
		/* Leave quiet CCBs waiting for resources */
		if (cp->host_status == HS_WAIT)
			continue;
		cmd = cp->cmd;
		if (cam_status)
			sym_set_cam_status(cmd, cam_status);
#ifdef SYM_OPT_HANDLE_DEVICE_QUEUEING
		if (sym_get_cam_status(cmd) == DID_SOFT_ERROR) {
			struct sym_tcb *tp = &np->target[cp->target];
			struct sym_lcb *lp = sym_lp(tp, cp->lun);
			if (lp) {
				sym_remque(&cp->link2_ccbq);
				sym_insque_tail(&cp->link2_ccbq,
				                &lp->waiting_ccbq);
				if (cp->started) {
					if (cp->tag != NO_TAG)
						--lp->started_tags;
					else
						--lp->started_no_tag;
				}
			}
			cp->started = 0;
			continue;
		}
#endif
		sym_free_ccb(np, cp);
		sym_xpt_done(np, cmd);
	}
}

/*
 *  Complete all active CCBs with error.
 *  Used on CHIP/SCSI RESET.
 */
static void sym_flush_busy_queue (struct sym_hcb *np, int cam_status)
{
	/*
	 *  Move all active CCBs to the COMP queue 
	 *  and flush this queue.
	 */
	sym_que_splice(&np->busy_ccbq, &np->comp_ccbq);
	sym_que_init(&np->busy_ccbq);
	sym_flush_comp_queue(np, cam_status);
}

/*
 *  Start chip.
 *
 *  'reason' means:
 *     0: initialisation.
 *     1: SCSI BUS RESET delivered or received.
 *     2: SCSI BUS MODE changed.
 */
void sym_start_up(struct Scsi_Host *shost, int reason)
{
	struct sym_data *sym_data = shost_priv(shost);
	struct pci_dev *pdev = sym_data->pdev;
	struct sym_hcb *np = sym_data->ncb;
 	int	i;
	u32	phys;

 	/*
	 *  Reset chip if asked, otherwise just clear fifos.
 	 */
	if (reason == 1)
		sym_soft_reset(np);
	else {
		OUTB(np, nc_stest3, TE|CSF);
		OUTONB(np, nc_ctest3, CLF);
	}
 
	/*
	 *  Clear Start Queue
	 */
	phys = np->squeue_ba;
	for (i = 0; i < MAX_QUEUE*2; i += 2) {
		np->squeue[i]   = cpu_to_scr(np->idletask_ba);
		np->squeue[i+1] = cpu_to_scr(phys + (i+2)*4);
	}
	np->squeue[MAX_QUEUE*2-1] = cpu_to_scr(phys);

	/*
	 *  Start at first entry.
	 */
	np->squeueput = 0;

	/*
	 *  Clear Done Queue
	 */
	phys = np->dqueue_ba;
	for (i = 0; i < MAX_QUEUE*2; i += 2) {
		np->dqueue[i]   = 0;
		np->dqueue[i+1] = cpu_to_scr(phys + (i+2)*4);
	}
	np->dqueue[MAX_QUEUE*2-1] = cpu_to_scr(phys);

	/*
	 *  Start at first entry.
	 */
	np->dqueueget = 0;

	/*
	 *  Install patches in scripts.
	 *  This also let point to first position the start 
	 *  and done queue pointers used from SCRIPTS.
	 */
	np->fw_patch(shost);

	/*
	 *  Wakeup all pending jobs.
	 */
	sym_flush_busy_queue(np, DID_RESET);

	/*
	 *  Init chip.
	 */
	OUTB(np, nc_istat,  0x00);			/*  Remove Reset, abort */
	INB(np, nc_mbox1);
	udelay(2000); /* The 895 needs time for the bus mode to settle */

	OUTB(np, nc_scntl0, np->rv_scntl0 | 0xc0);
					/*  full arb., ena parity, par->ATN  */
	OUTB(np, nc_scntl1, 0x00);		/*  odd parity, and remove CRST!! */

	sym_selectclock(np, np->rv_scntl3);	/* Select SCSI clock */

	OUTB(np, nc_scid  , RRE|np->myaddr);	/* Adapter SCSI address */
	OUTW(np, nc_respid, 1ul<<np->myaddr);	/* Id to respond to */
	OUTB(np, nc_istat , SIGP	);		/*  Signal Process */
	OUTB(np, nc_dmode , np->rv_dmode);		/* Burst length, dma mode */
	OUTB(np, nc_ctest5, np->rv_ctest5);	/* Large fifo + large burst */

	OUTB(np, nc_dcntl , NOCOM|np->rv_dcntl);	/* Protect SFBR */
	OUTB(np, nc_ctest3, np->rv_ctest3);	/* Write and invalidate */
	OUTB(np, nc_ctest4, np->rv_ctest4);	/* Master parity checking */

	/* Extended Sreq/Sack filtering not supported on the C10 */
	if (np->features & FE_C10)
		OUTB(np, nc_stest2, np->rv_stest2);
	else
		OUTB(np, nc_stest2, EXT|np->rv_stest2);

	OUTB(np, nc_stest3, TE);			/* TolerANT enable */
	OUTB(np, nc_stime0, 0x0c);			/* HTH disabled  STO 0.25 sec */

	/*
	 *  For now, disable AIP generation on C1010-66.
	 */
	if (pdev->device == PCI_DEVICE_ID_LSI_53C1010_66)
		OUTB(np, nc_aipcntl1, DISAIP);

	/*
	 *  C10101 rev. 0 errata.
	 *  Errant SGE's when in narrow. Write bits 4 & 5 of
	 *  STEST1 register to disable SGE. We probably should do 
	 *  that from SCRIPTS for each selection/reselection, but 
	 *  I just don't want. :)
	 */
	if (pdev->device == PCI_DEVICE_ID_LSI_53C1010_33 &&
	    pdev->revision < 1)
		OUTB(np, nc_stest1, INB(np, nc_stest1) | 0x30);

	/*
	 *  DEL 441 - 53C876 Rev 5 - Part Number 609-0392787/2788 - ITEM 2.
	 *  Disable overlapped arbitration for some dual function devices, 
	 *  regardless revision id (kind of post-chip-design feature. ;-))
	 */
	if (pdev->device == PCI_DEVICE_ID_NCR_53C875)
		OUTB(np, nc_ctest0, (1<<5));
	else if (pdev->device == PCI_DEVICE_ID_NCR_53C896)
		np->rv_ccntl0 |= DPR;

	/*
	 *  Write CCNTL0/CCNTL1 for chips capable of 64 bit addressing 
	 *  and/or hardware phase mismatch, since only such chips 
	 *  seem to support those IO registers.
	 */
	if (np->features & (FE_DAC|FE_NOPM)) {
		OUTB(np, nc_ccntl0, np->rv_ccntl0);
		OUTB(np, nc_ccntl1, np->rv_ccntl1);
	}

#if	SYM_CONF_DMA_ADDRESSING_MODE == 2
	/*
	 *  Set up scratch C and DRS IO registers to map the 32 bit 
	 *  DMA address range our data structures are located in.
	 */
	if (use_dac(np)) {
		np->dmap_bah[0] = 0;	/* ??? */
		OUTL(np, nc_scrx[0], np->dmap_bah[0]);
		OUTL(np, nc_drs, np->dmap_bah[0]);
	}
#endif

	/*
	 *  If phase mismatch handled by scripts (895A/896/1010),
	 *  set PM jump addresses.
	 */
	if (np->features & FE_NOPM) {
		OUTL(np, nc_pmjad1, SCRIPTB_BA(np, pm_handle));
		OUTL(np, nc_pmjad2, SCRIPTB_BA(np, pm_handle));
	}

	/*
	 *    Enable GPIO0 pin for writing if LED support from SCRIPTS.
	 *    Also set GPIO5 and clear GPIO6 if hardware LED control.
	 */
	if (np->features & FE_LED0)
		OUTB(np, nc_gpcntl, INB(np, nc_gpcntl) & ~0x01);
	else if (np->features & FE_LEDC)
		OUTB(np, nc_gpcntl, (INB(np, nc_gpcntl) & ~0x41) | 0x20);

	/*
	 *      enable ints
	 */
	OUTW(np, nc_sien , STO|HTH|MA|SGE|UDC|RST|PAR);
	OUTB(np, nc_dien , MDPE|BF|SSI|SIR|IID);

	/*
	 *  For 895/6 enable SBMC interrupt and save current SCSI bus mode.
	 *  Try to eat the spurious SBMC interrupt that may occur when 
	 *  we reset the chip but not the SCSI BUS (at initialization).
	 */
	if (np->features & (FE_ULTRA2|FE_ULTRA3)) {
		OUTONW(np, nc_sien, SBMC);
		if (reason == 0) {
			INB(np, nc_mbox1);
			mdelay(100);
			INW(np, nc_sist);
		}
		np->scsi_mode = INB(np, nc_stest4) & SMODE;
	}

	/*
	 *  Fill in target structure.
	 *  Reinitialize usrsync.
	 *  Reinitialize usrwide.
	 *  Prepare sync negotiation according to actual SCSI bus mode.
	 */
	for (i=0;i<SYM_CONF_MAX_TARGET;i++) {
		struct sym_tcb *tp = &np->target[i];

		tp->to_reset  = 0;
		tp->head.sval = 0;
		tp->head.wval = np->rv_scntl3;
		tp->head.uval = 0;
		if (tp->lun0p)
			tp->lun0p->to_clear = 0;
		if (tp->lunmp) {
			int ln;

			for (ln = 1; ln < SYM_CONF_MAX_LUN; ln++)
				if (tp->lunmp[ln])
					tp->lunmp[ln]->to_clear = 0;
		}
	}

	/*
	 *  Download SCSI SCRIPTS to on-chip RAM if present,
	 *  and start script processor.
	 *  We do the download preferently from the CPU.
	 *  For platforms that may not support PCI memory mapping,
	 *  we use simple SCRIPTS that performs MEMORY MOVEs.
	 */
	phys = SCRIPTA_BA(np, init);
	if (np->ram_ba) {
		if (sym_verbose >= 2)
			printf("%s: Downloading SCSI SCRIPTS.\n", sym_name(np));
		memcpy_toio(np->s.ramaddr, np->scripta0, np->scripta_sz);
		if (np->features & FE_RAM8K) {
			memcpy_toio(np->s.ramaddr + 4096, np->scriptb0, np->scriptb_sz);
			phys = scr_to_cpu(np->scr_ram_seg);
			OUTL(np, nc_mmws, phys);
			OUTL(np, nc_mmrs, phys);
			OUTL(np, nc_sfs,  phys);
			phys = SCRIPTB_BA(np, start64);
		}
	}

	np->istat_sem = 0;

	OUTL(np, nc_dsa, np->hcb_ba);
	OUTL_DSP(np, phys);

	/*
	 *  Notify the XPT about the RESET condition.
	 */
	if (reason != 0)
		sym_xpt_async_bus_reset(np);
}

/*
 *  Switch trans mode for current job and its target.
 */
static void sym_settrans(struct sym_hcb *np, int target, u_char opts, u_char ofs,
			 u_char per, u_char wide, u_char div, u_char fak)
{
	SYM_QUEHEAD *qp;
	u_char sval, wval, uval;
	struct sym_tcb *tp = &np->target[target];

	assert(target == (INB(np, nc_sdid) & 0x0f));

	sval = tp->head.sval;
	wval = tp->head.wval;
	uval = tp->head.uval;

#if 0
	printf("XXXX sval=%x wval=%x uval=%x (%x)\n", 
		sval, wval, uval, np->rv_scntl3);
#endif
	/*
	 *  Set the offset.
	 */
	if (!(np->features & FE_C10))
		sval = (sval & ~0x1f) | ofs;
	else
		sval = (sval & ~0x3f) | ofs;

	/*
	 *  Set the sync divisor and extra clock factor.
	 */
	if (ofs != 0) {
		wval = (wval & ~0x70) | ((div+1) << 4);
		if (!(np->features & FE_C10))
			sval = (sval & ~0xe0) | (fak << 5);
		else {
			uval = uval & ~(XCLKH_ST|XCLKH_DT|XCLKS_ST|XCLKS_DT);
			if (fak >= 1) uval |= (XCLKH_ST|XCLKH_DT);
			if (fak >= 2) uval |= (XCLKS_ST|XCLKS_DT);
		}
	}

	/*
	 *  Set the bus width.
	 */
	wval = wval & ~EWS;
	if (wide != 0)
		wval |= EWS;

	/*
	 *  Set misc. ultra enable bits.
	 */
	if (np->features & FE_C10) {
		uval = uval & ~(U3EN|AIPCKEN);
		if (opts)	{
			assert(np->features & FE_U3EN);
			uval |= U3EN;
		}
	} else {
		wval = wval & ~ULTRA;
		if (per <= 12)	wval |= ULTRA;
	}

	/*
	 *   Stop there if sync parameters are unchanged.
	 */
	if (tp->head.sval == sval && 
	    tp->head.wval == wval &&
	    tp->head.uval == uval)
		return;
	tp->head.sval = sval;
	tp->head.wval = wval;
	tp->head.uval = uval;

	/*
	 *  Disable extended Sreq/Sack filtering if per < 50.
	 *  Not supported on the C1010.
	 */
	if (per < 50 && !(np->features & FE_C10))
		OUTOFFB(np, nc_stest2, EXT);

	/*
	 *  set actual value and sync_status
	 */
	OUTB(np, nc_sxfer,  tp->head.sval);
	OUTB(np, nc_scntl3, tp->head.wval);

	if (np->features & FE_C10) {
		OUTB(np, nc_scntl4, tp->head.uval);
	}

	/*
	 *  patch ALL busy ccbs of this target.
	 */
	FOR_EACH_QUEUED_ELEMENT(&np->busy_ccbq, qp) {
		struct sym_ccb *cp;
		cp = sym_que_entry(qp, struct sym_ccb, link_ccbq);
		if (cp->target != target)
			continue;
		cp->phys.select.sel_scntl3 = tp->head.wval;
		cp->phys.select.sel_sxfer  = tp->head.sval;
		if (np->features & FE_C10) {
			cp->phys.select.sel_scntl4 = tp->head.uval;
		}
	}
}

static void sym_announce_transfer_rate(struct sym_tcb *tp)
{
	struct scsi_target *starget = tp->starget;

	if (tp->tprint.period != spi_period(starget) ||
	    tp->tprint.offset != spi_offset(starget) ||
	    tp->tprint.width != spi_width(starget) ||
	    tp->tprint.iu != spi_iu(starget) ||
	    tp->tprint.dt != spi_dt(starget) ||
	    tp->tprint.qas != spi_qas(starget) ||
	    !tp->tprint.check_nego) {
		tp->tprint.period = spi_period(starget);
		tp->tprint.offset = spi_offset(starget);
		tp->tprint.width = spi_width(starget);
		tp->tprint.iu = spi_iu(starget);
		tp->tprint.dt = spi_dt(starget);
		tp->tprint.qas = spi_qas(starget);
		tp->tprint.check_nego = 1;

		spi_display_xfer_agreement(starget);
	}
}

/*
 *  We received a WDTR.
 *  Let everything be aware of the changes.
 */
static void sym_setwide(struct sym_hcb *np, int target, u_char wide)
{
	struct sym_tcb *tp = &np->target[target];
	struct scsi_target *starget = tp->starget;

	sym_settrans(np, target, 0, 0, 0, wide, 0, 0);

	if (wide)
		tp->tgoal.renego = NS_WIDE;
	else
		tp->tgoal.renego = 0;
	tp->tgoal.check_nego = 0;
	tp->tgoal.width = wide;
	spi_offset(starget) = 0;
	spi_period(starget) = 0;
	spi_width(starget) = wide;
	spi_iu(starget) = 0;
	spi_dt(starget) = 0;
	spi_qas(starget) = 0;

	if (sym_verbose >= 3)
		sym_announce_transfer_rate(tp);
}

/*
 *  We received a SDTR.
 *  Let everything be aware of the changes.
 */
static void
sym_setsync(struct sym_hcb *np, int target,
            u_char ofs, u_char per, u_char div, u_char fak)
{
	struct sym_tcb *tp = &np->target[target];
	struct scsi_target *starget = tp->starget;
	u_char wide = (tp->head.wval & EWS) ? BUS_16_BIT : BUS_8_BIT;

	sym_settrans(np, target, 0, ofs, per, wide, div, fak);

	if (wide)
		tp->tgoal.renego = NS_WIDE;
	else if (ofs)
		tp->tgoal.renego = NS_SYNC;
	else
		tp->tgoal.renego = 0;
	spi_period(starget) = per;
	spi_offset(starget) = ofs;
	spi_iu(starget) = spi_dt(starget) = spi_qas(starget) = 0;

	if (!tp->tgoal.dt && !tp->tgoal.iu && !tp->tgoal.qas) {
		tp->tgoal.period = per;
		tp->tgoal.offset = ofs;
		tp->tgoal.check_nego = 0;
	}

	sym_announce_transfer_rate(tp);
}

/*
 *  We received a PPR.
 *  Let everything be aware of the changes.
 */
static void 
sym_setpprot(struct sym_hcb *np, int target, u_char opts, u_char ofs,
             u_char per, u_char wide, u_char div, u_char fak)
{
	struct sym_tcb *tp = &np->target[target];
	struct scsi_target *starget = tp->starget;

	sym_settrans(np, target, opts, ofs, per, wide, div, fak);

	if (wide || ofs)
		tp->tgoal.renego = NS_PPR;
	else
		tp->tgoal.renego = 0;
	spi_width(starget) = tp->tgoal.width = wide;
	spi_period(starget) = tp->tgoal.period = per;
	spi_offset(starget) = tp->tgoal.offset = ofs;
	spi_iu(starget) = tp->tgoal.iu = !!(opts & PPR_OPT_IU);
	spi_dt(starget) = tp->tgoal.dt = !!(opts & PPR_OPT_DT);
	spi_qas(starget) = tp->tgoal.qas = !!(opts & PPR_OPT_QAS);
	tp->tgoal.check_nego = 0;

	sym_announce_transfer_rate(tp);
}

/*
 *  generic recovery from scsi interrupt
 *
 *  The doc says that when the chip gets an SCSI interrupt,
 *  it tries to stop in an orderly fashion, by completing 
 *  an instruction fetch that had started or by flushing 
 *  the DMA fifo for a write to memory that was executing.
 *  Such a fashion is not enough to know if the instruction 
 *  that was just before the current DSP value has been 
 *  executed or not.
 *
 *  There are some small SCRIPTS sections that deal with 
 *  the start queue and the done queue that may break any 
 *  assomption from the C code if we are interrupted 
 *  inside, so we reset if this happens. Btw, since these 
 *  SCRIPTS sections are executed while the SCRIPTS hasn't 
 *  started SCSI operations, it is very unlikely to happen.
 *
 *  All the driver data structures are supposed to be 
 *  allocated from the same 4 GB memory window, so there 
 *  is a 1 to 1 relationship between DSA and driver data 
 *  structures. Since we are careful :) to invalidate the 
 *  DSA when we complete a command or when the SCRIPTS 
 *  pushes a DSA into a queue, we can trust it when it 
 *  points to a CCB.
 */
static void sym_recover_scsi_int (struct sym_hcb *np, u_char hsts)
{
	u32	dsp	= INL(np, nc_dsp);
	u32	dsa	= INL(np, nc_dsa);
	struct sym_ccb *cp	= sym_ccb_from_dsa(np, dsa);

	/*
	 *  If we haven't been interrupted inside the SCRIPTS 
	 *  critical pathes, we can safely restart the SCRIPTS 
	 *  and trust the DSA value if it matches a CCB.
	 */
	if ((!(dsp > SCRIPTA_BA(np, getjob_begin) &&
	       dsp < SCRIPTA_BA(np, getjob_end) + 1)) &&
	    (!(dsp > SCRIPTA_BA(np, ungetjob) &&
	       dsp < SCRIPTA_BA(np, reselect) + 1)) &&
	    (!(dsp > SCRIPTB_BA(np, sel_for_abort) &&
	       dsp < SCRIPTB_BA(np, sel_for_abort_1) + 1)) &&
	    (!(dsp > SCRIPTA_BA(np, done) &&
	       dsp < SCRIPTA_BA(np, done_end) + 1))) {
		OUTB(np, nc_ctest3, np->rv_ctest3 | CLF); /* clear dma fifo  */
		OUTB(np, nc_stest3, TE|CSF);		/* clear scsi fifo */
		/*
		 *  If we have a CCB, let the SCRIPTS call us back for 
		 *  the handling of the error with SCRATCHA filled with 
		 *  STARTPOS. This way, we will be able to freeze the 
		 *  device queue and requeue awaiting IOs.
		 */
		if (cp) {
			cp->host_status = hsts;
			OUTL_DSP(np, SCRIPTA_BA(np, complete_error));
		}
		/*
		 *  Otherwise just restart the SCRIPTS.
		 */
		else {
			OUTL(np, nc_dsa, 0xffffff);
			OUTL_DSP(np, SCRIPTA_BA(np, start));
		}
	}
	else
		goto reset_all;

	return;

reset_all:
	sym_start_reset(np);
}

/*
 *  chip exception handler for selection timeout
 */
static void sym_int_sto (struct sym_hcb *np)
{
	u32 dsp	= INL(np, nc_dsp);

	if (DEBUG_FLAGS & DEBUG_TINY) printf ("T");

	if (dsp == SCRIPTA_BA(np, wf_sel_done) + 8)
		sym_recover_scsi_int(np, HS_SEL_TIMEOUT);
	else
		sym_start_reset(np);
}

/*
 *  chip exception handler for unexpected disconnect
 */
static void sym_int_udc (struct sym_hcb *np)
{
	printf ("%s: unexpected disconnect\n", sym_name(np));
	sym_recover_scsi_int(np, HS_UNEXPECTED);
}

/*
 *  chip exception handler for SCSI bus mode change
 *
 *  spi2-r12 11.2.3 says a transceiver mode change must 
 *  generate a reset event and a device that detects a reset 
 *  event shall initiate a hard reset. It says also that a
 *  device that detects a mode change shall set data transfer 
 *  mode to eight bit asynchronous, etc...
 *  So, just reinitializing all except chip should be enough.
 */
static void sym_int_sbmc(struct Scsi_Host *shost)
{
	struct sym_hcb *np = sym_get_hcb(shost);
	u_char scsi_mode = INB(np, nc_stest4) & SMODE;

	/*
	 *  Notify user.
	 */
	printf("%s: SCSI BUS mode change from %s to %s.\n", sym_name(np),
		sym_scsi_bus_mode(np->scsi_mode), sym_scsi_bus_mode(scsi_mode));

	/*
	 *  Should suspend command processing for a few seconds and 
	 *  reinitialize all except the chip.
	 */
	sym_start_up(shost, 2);
}

/*
 *  chip exception handler for SCSI parity error.
 *
 *  When the chip detects a SCSI parity error and is 
 *  currently executing a (CH)MOV instruction, it does 
 *  not interrupt immediately, but tries to finish the 
 *  transfer of the current scatter entry before 
 *  interrupting. The following situations may occur:
 *
 *  - The complete scatter entry has been transferred 
 *    without the device having changed phase.
 *    The chip will then interrupt with the DSP pointing 
 *    to the instruction that follows the MOV.
 *
 *  - A phase mismatch occurs before the MOV finished 
 *    and phase errors are to be handled by the C code.
 *    The chip will then interrupt with both PAR and MA 
 *    conditions set.
 *
 *  - A phase mismatch occurs before the MOV finished and 
 *    phase errors are to be handled by SCRIPTS.
 *    The chip will load the DSP with the phase mismatch 
 *    JUMP address and interrupt the host processor.
 */
static void sym_int_par (struct sym_hcb *np, u_short sist)
{
	u_char	hsts	= INB(np, HS_PRT);
	u32	dsp	= INL(np, nc_dsp);
	u32	dbc	= INL(np, nc_dbc);
	u32	dsa	= INL(np, nc_dsa);
	u_char	sbcl	= INB(np, nc_sbcl);
	u_char	cmd	= dbc >> 24;
	int phase	= cmd & 7;
	struct sym_ccb *cp	= sym_ccb_from_dsa(np, dsa);

	if (printk_ratelimit())
		printf("%s: SCSI parity error detected: SCR1=%d DBC=%x SBCL=%x\n",
			sym_name(np), hsts, dbc, sbcl);

	/*
	 *  Check that the chip is connected to the SCSI BUS.
	 */
	if (!(INB(np, nc_scntl1) & ISCON)) {
		sym_recover_scsi_int(np, HS_UNEXPECTED);
		return;
	}

	/*
	 *  If the nexus is not clearly identified, reset the bus.
	 *  We will try to do better later.
	 */
	if (!cp)
		goto reset_all;

	/*
	 *  Check instruction was a MOV, direction was INPUT and 
	 *  ATN is asserted.
	 */
	if ((cmd & 0xc0) || !(phase & 1) || !(sbcl & 0x8))
		goto reset_all;

	/*
	 *  Keep track of the parity error.
	 */
	OUTONB(np, HF_PRT, HF_EXT_ERR);
	cp->xerr_status |= XE_PARITY_ERR;

	/*
	 *  Prepare the message to send to the device.
	 */
	np->msgout[0] = (phase == 7) ? M_PARITY : M_ID_ERROR;

	/*
	 *  If the old phase was DATA IN phase, we have to deal with
	 *  the 3 situations described above.
	 *  For other input phases (MSG IN and STATUS), the device 
	 *  must resend the whole thing that failed parity checking 
	 *  or signal error. So, jumping to dispatcher should be OK.
	 */
	if (phase == 1 || phase == 5) {
		/* Phase mismatch handled by SCRIPTS */
		if (dsp == SCRIPTB_BA(np, pm_handle))
			OUTL_DSP(np, dsp);
		/* Phase mismatch handled by the C code */
		else if (sist & MA)
			sym_int_ma (np);
		/* No phase mismatch occurred */
		else {
			sym_set_script_dp (np, cp, dsp);
			OUTL_DSP(np, SCRIPTA_BA(np, dispatch));
		}
	}
	else if (phase == 7)	/* We definitely cannot handle parity errors */
#if 1				/* in message-in phase due to the relection  */
		goto reset_all; /* path and various message anticipations.   */
#else
		OUTL_DSP(np, SCRIPTA_BA(np, clrack));
#endif
	else
		OUTL_DSP(np, SCRIPTA_BA(np, dispatch));
	return;

reset_all:
	sym_start_reset(np);
	return;
}

/*
 *  chip exception handler for phase errors.
 *
 *  We have to construct a new transfer descriptor,
 *  to transfer the rest of the current block.
 */
static void sym_int_ma (struct sym_hcb *np)
{
	u32	dbc;
	u32	rest;
	u32	dsp;
	u32	dsa;
	u32	nxtdsp;
	u32	*vdsp;
	u32	oadr, olen;
	u32	*tblp;
        u32	newcmd;
	u_int	delta;
	u_char	cmd;
	u_char	hflags, hflags0;
	struct	sym_pmc *pm;
	struct sym_ccb *cp;

	dsp	= INL(np, nc_dsp);
	dbc	= INL(np, nc_dbc);
	dsa	= INL(np, nc_dsa);

	cmd	= dbc >> 24;
	rest	= dbc & 0xffffff;
	delta	= 0;

	/*
	 *  locate matching cp if any.
	 */
	cp = sym_ccb_from_dsa(np, dsa);

	/*
	 *  Donnot take into account dma fifo and various buffers in 
	 *  INPUT phase since the chip flushes everything before 
	 *  raising the MA interrupt for interrupted INPUT phases.
	 *  For DATA IN phase, we will check for the SWIDE later.
	 */
	if ((cmd & 7) != 1 && (cmd & 7) != 5) {
		u_char ss0, ss2;

		if (np->features & FE_DFBC)
			delta = INW(np, nc_dfbc);
		else {
			u32 dfifo;

			/*
			 * Read DFIFO, CTEST[4-6] using 1 PCI bus ownership.
			 */
			dfifo = INL(np, nc_dfifo);

			/*
			 *  Calculate remaining bytes in DMA fifo.
			 *  (CTEST5 = dfifo >> 16)
			 */
			if (dfifo & (DFS << 16))
				delta = ((((dfifo >> 8) & 0x300) |
				          (dfifo & 0xff)) - rest) & 0x3ff;
			else
				delta = ((dfifo & 0xff) - rest) & 0x7f;
		}

		/*
		 *  The data in the dma fifo has not been transfered to
		 *  the target -> add the amount to the rest
		 *  and clear the data.
		 *  Check the sstat2 register in case of wide transfer.
		 */
		rest += delta;
		ss0  = INB(np, nc_sstat0);
		if (ss0 & OLF) rest++;
		if (!(np->features & FE_C10))
			if (ss0 & ORF) rest++;
		if (cp && (cp->phys.select.sel_scntl3 & EWS)) {
			ss2 = INB(np, nc_sstat2);
			if (ss2 & OLF1) rest++;
			if (!(np->features & FE_C10))
				if (ss2 & ORF1) rest++;
		}

		/*
		 *  Clear fifos.
		 */
		OUTB(np, nc_ctest3, np->rv_ctest3 | CLF);	/* dma fifo  */
		OUTB(np, nc_stest3, TE|CSF);		/* scsi fifo */
	}

	/*
	 *  log the information
	 */
	if (DEBUG_FLAGS & (DEBUG_TINY|DEBUG_PHASE))
		printf ("P%x%x RL=%d D=%d ", cmd&7, INB(np, nc_sbcl)&7,
			(unsigned) rest, (unsigned) delta);

	/*
	 *  try to find the interrupted script command,
	 *  and the address at which to continue.
	 */
	vdsp	= NULL;
	nxtdsp	= 0;
	if	(dsp >  np->scripta_ba &&
		 dsp <= np->scripta_ba + np->scripta_sz) {
		vdsp = (u32 *)((char*)np->scripta0 + (dsp-np->scripta_ba-8));
		nxtdsp = dsp;
	}
	else if	(dsp >  np->scriptb_ba &&
		 dsp <= np->scriptb_ba + np->scriptb_sz) {
		vdsp = (u32 *)((char*)np->scriptb0 + (dsp-np->scriptb_ba-8));
		nxtdsp = dsp;
	}

	/*
	 *  log the information
	 */
	if (DEBUG_FLAGS & DEBUG_PHASE) {
		printf ("\nCP=%p DSP=%x NXT=%x VDSP=%p CMD=%x ",
			cp, (unsigned)dsp, (unsigned)nxtdsp, vdsp, cmd);
	}

	if (!vdsp) {
		printf ("%s: interrupted SCRIPT address not found.\n", 
			sym_name (np));
		goto reset_all;
	}

	if (!cp) {
		printf ("%s: SCSI phase error fixup: CCB already dequeued.\n", 
			sym_name (np));
		goto reset_all;
	}

	/*
	 *  get old startaddress and old length.
	 */
	oadr = scr_to_cpu(vdsp[1]);

	if (cmd & 0x10) {	/* Table indirect */
		tblp = (u32 *) ((char*) &cp->phys + oadr);
		olen = scr_to_cpu(tblp[0]);
		oadr = scr_to_cpu(tblp[1]);
	} else {
		tblp = (u32 *) 0;
		olen = scr_to_cpu(vdsp[0]) & 0xffffff;
	}

	if (DEBUG_FLAGS & DEBUG_PHASE) {
		printf ("OCMD=%x\nTBLP=%p OLEN=%x OADR=%x\n",
			(unsigned) (scr_to_cpu(vdsp[0]) >> 24),
			tblp,
			(unsigned) olen,
			(unsigned) oadr);
	}

	/*
	 *  check cmd against assumed interrupted script command.
	 *  If dt data phase, the MOVE instruction hasn't bit 4 of 
	 *  the phase.
	 */
	if (((cmd & 2) ? cmd : (cmd & ~4)) != (scr_to_cpu(vdsp[0]) >> 24)) {
		sym_print_addr(cp->cmd,
			"internal error: cmd=%02x != %02x=(vdsp[0] >> 24)\n",
			cmd, scr_to_cpu(vdsp[0]) >> 24);

		goto reset_all;
	}

	/*
	 *  if old phase not dataphase, leave here.
	 */
	if (cmd & 2) {
		sym_print_addr(cp->cmd,
			"phase change %x-%x %d@%08x resid=%d.\n",
			cmd&7, INB(np, nc_sbcl)&7, (unsigned)olen,
			(unsigned)oadr, (unsigned)rest);
		goto unexpected_phase;
	}

	/*
	 *  Choose the correct PM save area.
	 *
	 *  Look at the PM_SAVE SCRIPT if you want to understand 
	 *  this stuff. The equivalent code is implemented in 
	 *  SCRIPTS for the 895A, 896 and 1010 that are able to 
	 *  handle PM from the SCRIPTS processor.
	 */
	hflags0 = INB(np, HF_PRT);
	hflags = hflags0;

	if (hflags & (HF_IN_PM0 | HF_IN_PM1 | HF_DP_SAVED)) {
		if (hflags & HF_IN_PM0)
			nxtdsp = scr_to_cpu(cp->phys.pm0.ret);
		else if	(hflags & HF_IN_PM1)
			nxtdsp = scr_to_cpu(cp->phys.pm1.ret);

		if (hflags & HF_DP_SAVED)
			hflags ^= HF_ACT_PM;
	}

	if (!(hflags & HF_ACT_PM)) {
		pm = &cp->phys.pm0;
		newcmd = SCRIPTA_BA(np, pm0_data);
	}
	else {
		pm = &cp->phys.pm1;
		newcmd = SCRIPTA_BA(np, pm1_data);
	}

	hflags &= ~(HF_IN_PM0 | HF_IN_PM1 | HF_DP_SAVED);
	if (hflags != hflags0)
		OUTB(np, HF_PRT, hflags);

	/*
	 *  fillin the phase mismatch context
	 */
	pm->sg.addr = cpu_to_scr(oadr + olen - rest);
	pm->sg.size = cpu_to_scr(rest);
	pm->ret     = cpu_to_scr(nxtdsp);

	/*
	 *  If we have a SWIDE,
	 *  - prepare the address to write the SWIDE from SCRIPTS,
	 *  - compute the SCRIPTS address to restart from,
	 *  - move current data pointer context by one byte.
	 */
	nxtdsp = SCRIPTA_BA(np, dispatch);
	if ((cmd & 7) == 1 && cp && (cp->phys.select.sel_scntl3 & EWS) &&
	    (INB(np, nc_scntl2) & WSR)) {
		u32 tmp;

		/*
		 *  Set up the table indirect for the MOVE
		 *  of the residual byte and adjust the data 
		 *  pointer context.
		 */
		tmp = scr_to_cpu(pm->sg.addr);
		cp->phys.wresid.addr = cpu_to_scr(tmp);
		pm->sg.addr = cpu_to_scr(tmp + 1);
		tmp = scr_to_cpu(pm->sg.size);
		cp->phys.wresid.size = cpu_to_scr((tmp&0xff000000) | 1);
		pm->sg.size = cpu_to_scr(tmp - 1);

		/*
		 *  If only the residual byte is to be moved, 
		 *  no PM context is needed.
		 */
		if ((tmp&0xffffff) == 1)
			newcmd = pm->ret;

		/*
		 *  Prepare the address of SCRIPTS that will 
		 *  move the residual byte to memory.
		 */
		nxtdsp = SCRIPTB_BA(np, wsr_ma_helper);
	}

	if (DEBUG_FLAGS & DEBUG_PHASE) {
		sym_print_addr(cp->cmd, "PM %x %x %x / %x %x %x.\n",
			hflags0, hflags, newcmd,
			(unsigned)scr_to_cpu(pm->sg.addr),
			(unsigned)scr_to_cpu(pm->sg.size),
			(unsigned)scr_to_cpu(pm->ret));
	}

	/*
	 *  Restart the SCRIPTS processor.
	 */
	sym_set_script_dp (np, cp, newcmd);
	OUTL_DSP(np, nxtdsp);
	return;

	/*
	 *  Unexpected phase changes that occurs when the current phase 
	 *  is not a DATA IN or DATA OUT phase are due to error conditions.
	 *  Such event may only happen when the SCRIPTS is using a 
	 *  multibyte SCSI MOVE.
	 *
	 *  Phase change		Some possible cause
	 *
	 *  COMMAND  --> MSG IN	SCSI parity error detected by target.
	 *  COMMAND  --> STATUS	Bad command or refused by target.
	 *  MSG OUT  --> MSG IN     Message rejected by target.
	 *  MSG OUT  --> COMMAND    Bogus target that discards extended
	 *  			negotiation messages.
	 *
	 *  The code below does not care of the new phase and so 
	 *  trusts the target. Why to annoy it ?
	 *  If the interrupted phase is COMMAND phase, we restart at
	 *  dispatcher.
	 *  If a target does not get all the messages after selection, 
	 *  the code assumes blindly that the target discards extended 
	 *  messages and clears the negotiation status.
	 *  If the target does not want all our response to negotiation,
	 *  we force a SIR_NEGO_PROTO interrupt (it is a hack that avoids 
	 *  bloat for such a should_not_happen situation).
	 *  In all other situation, we reset the BUS.
	 *  Are these assumptions reasonnable ? (Wait and see ...)
	 */
unexpected_phase:
	dsp -= 8;
	nxtdsp = 0;

	switch (cmd & 7) {
	case 2:	/* COMMAND phase */
		nxtdsp = SCRIPTA_BA(np, dispatch);
		break;
#if 0
	case 3:	/* STATUS  phase */
		nxtdsp = SCRIPTA_BA(np, dispatch);
		break;
#endif
	case 6:	/* MSG OUT phase */
		/*
		 *  If the device may want to use untagged when we want 
		 *  tagged, we prepare an IDENTIFY without disc. granted, 
		 *  since we will not be able to handle reselect.
		 *  Otherwise, we just don't care.
		 */
		if	(dsp == SCRIPTA_BA(np, send_ident)) {
			if (cp->tag != NO_TAG && olen - rest <= 3) {
				cp->host_status = HS_BUSY;
				np->msgout[0] = IDENTIFY(0, cp->lun);
				nxtdsp = SCRIPTB_BA(np, ident_break_atn);
			}
			else
				nxtdsp = SCRIPTB_BA(np, ident_break);
		}
		else if	(dsp == SCRIPTB_BA(np, send_wdtr) ||
			 dsp == SCRIPTB_BA(np, send_sdtr) ||
			 dsp == SCRIPTB_BA(np, send_ppr)) {
			nxtdsp = SCRIPTB_BA(np, nego_bad_phase);
			if (dsp == SCRIPTB_BA(np, send_ppr)) {
				struct scsi_device *dev = cp->cmd->device;
				dev->ppr = 0;
			}
		}
		break;
#if 0
	case 7:	/* MSG IN  phase */
		nxtdsp = SCRIPTA_BA(np, clrack);
		break;
#endif
	}

	if (nxtdsp) {
		OUTL_DSP(np, nxtdsp);
		return;
	}

reset_all:
	sym_start_reset(np);
}

/*
 *  chip interrupt handler
 *
 *  In normal situations, interrupt conditions occur one at 
 *  a time. But when something bad happens on the SCSI BUS, 
 *  the chip may raise several interrupt flags before 
 *  stopping and interrupting the CPU. The additionnal 
 *  interrupt flags are stacked in some extra registers 
 *  after the SIP and/or DIP flag has been raised in the 
 *  ISTAT. After the CPU has read the interrupt condition 
 *  flag from SIST or DSTAT, the chip unstacks the other 
 *  interrupt flags and sets the corresponding bits in 
 *  SIST or DSTAT. Since the chip starts stacking once the 
 *  SIP or DIP flag is set, there is a small window of time 
 *  where the stacking does not occur.
 *
 *  Typically, multiple interrupt conditions may happen in 
 *  the following situations:
 *
 *  - SCSI parity error + Phase mismatch  (PAR|MA)
 *    When an parity error is detected in input phase 
 *    and the device switches to msg-in phase inside a 
 *    block MOV.
 *  - SCSI parity error + Unexpected disconnect (PAR|UDC)
 *    When a stupid device does not want to handle the 
 *    recovery of an SCSI parity error.
 *  - Some combinations of STO, PAR, UDC, ...
 *    When using non compliant SCSI stuff, when user is 
 *    doing non compliant hot tampering on the BUS, when 
 *    something really bad happens to a device, etc ...
 *
 *  The heuristic suggested by SYMBIOS to handle 
 *  multiple interrupts is to try unstacking all 
 *  interrupts conditions and to handle them on some 
 *  priority based on error severity.
 *  This will work when the unstacking has been 
 *  successful, but we cannot be 100 % sure of that, 
 *  since the CPU may have been faster to unstack than 
 *  the chip is able to stack. Hmmm ... But it seems that 
 *  such a situation is very unlikely to happen.
 *
 *  If this happen, for example STO caught by the CPU 
 *  then UDC happenning before the CPU have restarted 
 *  the SCRIPTS, the driver may wrongly complete the 
 *  same command on UDC, since the SCRIPTS didn't restart 
 *  and the DSA still points to the same command.
 *  We avoid this situation by setting the DSA to an 
 *  invalid value when the CCB is completed and before 
 *  restarting the SCRIPTS.
 *
 *  Another issue is that we need some section of our 
 *  recovery procedures to be somehow uninterruptible but 
 *  the SCRIPTS processor does not provides such a 
 *  feature. For this reason, we handle recovery preferently 
 *  from the C code and check against some SCRIPTS critical 
 *  sections from the C code.
 *
 *  Hopefully, the interrupt handling of the driver is now 
 *  able to resist to weird BUS error conditions, but donnot 
 *  ask me for any guarantee that it will never fail. :-)
 *  Use at your own decision and risk.
 */

irqreturn_t sym_interrupt(struct Scsi_Host *shost)
{
	struct sym_data *sym_data = shost_priv(shost);
	struct sym_hcb *np = sym_data->ncb;
	struct pci_dev *pdev = sym_data->pdev;
	u_char	istat, istatc;
	u_char	dstat;
	u_short	sist;

	/*
	 *  interrupt on the fly ?
	 *  (SCRIPTS may still be running)
	 *
	 *  A `dummy read' is needed to ensure that the 
	 *  clear of the INTF flag reaches the device 
	 *  and that posted writes are flushed to memory
	 *  before the scanning of the DONE queue.
	 *  Note that SCRIPTS also (dummy) read to memory 
	 *  prior to deliver the INTF interrupt condition.
	 */
	istat = INB(np, nc_istat);
	if (istat & INTF) {
		OUTB(np, nc_istat, (istat & SIGP) | INTF | np->istat_sem);
		istat |= INB(np, nc_istat);		/* DUMMY READ */
		if (DEBUG_FLAGS & DEBUG_TINY) printf ("F ");
		sym_wakeup_done(np);
	}

	if (!(istat & (SIP|DIP)))
		return (istat & INTF) ? IRQ_HANDLED : IRQ_NONE;

#if 0	/* We should never get this one */
	if (istat & CABRT)
		OUTB(np, nc_istat, CABRT);
#endif

	/*
	 *  PAR and MA interrupts may occur at the same time,
	 *  and we need to know of both in order to handle 
	 *  this situation properly. We try to unstack SCSI 
	 *  interrupts for that reason. BTW, I dislike a LOT 
	 *  such a loop inside the interrupt routine.
	 *  Even if DMA interrupt stacking is very unlikely to 
	 *  happen, we also try unstacking these ones, since 
	 *  this has no performance impact.
	 */
	sist	= 0;
	dstat	= 0;
	istatc	= istat;
	do {
		if (istatc & SIP)
			sist  |= INW(np, nc_sist);
		if (istatc & DIP)
			dstat |= INB(np, nc_dstat);
		istatc = INB(np, nc_istat);
		istat |= istatc;

		/* Prevent deadlock waiting on a condition that may
		 * never clear. */
		if (unlikely(sist == 0xffff && dstat == 0xff)) {
			if (pci_channel_offline(pdev))
				return IRQ_NONE;
		}
	} while (istatc & (SIP|DIP));

	if (DEBUG_FLAGS & DEBUG_TINY)
		printf ("<%d|%x:%x|%x:%x>",
			(int)INB(np, nc_scr0),
			dstat,sist,
			(unsigned)INL(np, nc_dsp),
			(unsigned)INL(np, nc_dbc));
	/*
	 *  On paper, a memory read barrier may be needed here to 
	 *  prevent out of order LOADs by the CPU from having 
	 *  prefetched stale data prior to DMA having occurred.
	 *  And since we are paranoid ... :)
	 */
	MEMORY_READ_BARRIER();

	/*
	 *  First, interrupts we want to service cleanly.
	 *
	 *  Phase mismatch (MA) is the most frequent interrupt 
	 *  for chip earlier than the 896 and so we have to service 
	 *  it as quickly as possible.
	 *  A SCSI parity error (PAR) may be combined with a phase 
	 *  mismatch condition (MA).
	 *  Programmed interrupts (SIR) are used to call the C code 
	 *  from SCRIPTS.
	 *  The single step interrupt (SSI) is not used in this 
	 *  driver.
	 */
	if (!(sist  & (STO|GEN|HTH|SGE|UDC|SBMC|RST)) &&
	    !(dstat & (MDPE|BF|ABRT|IID))) {
		if	(sist & PAR)	sym_int_par (np, sist);
		else if (sist & MA)	sym_int_ma (np);
		else if (dstat & SIR)	sym_int_sir(np);
		else if (dstat & SSI)	OUTONB_STD();
		else			goto unknown_int;
		return IRQ_HANDLED;
	}

	/*
	 *  Now, interrupts that donnot happen in normal 
	 *  situations and that we may need to recover from.
	 *
	 *  On SCSI RESET (RST), we reset everything.
	 *  On SCSI BUS MODE CHANGE (SBMC), we complete all 
	 *  active CCBs with RESET status, prepare all devices 
	 *  for negotiating again and restart the SCRIPTS.
	 *  On STO and UDC, we complete the CCB with the corres- 
	 *  ponding status and restart the SCRIPTS.
	 */
	if (sist & RST) {
		printf("%s: SCSI BUS reset detected.\n", sym_name(np));
		sym_start_up(shost, 1);
		return IRQ_HANDLED;
	}

	OUTB(np, nc_ctest3, np->rv_ctest3 | CLF);	/* clear dma fifo  */
	OUTB(np, nc_stest3, TE|CSF);		/* clear scsi fifo */

	if (!(sist  & (GEN|HTH|SGE)) &&
	    !(dstat & (MDPE|BF|ABRT|IID))) {
		if	(sist & SBMC)	sym_int_sbmc(shost);
		else if (sist & STO)	sym_int_sto (np);
		else if (sist & UDC)	sym_int_udc (np);
		else			goto unknown_int;
		return IRQ_HANDLED;
	}

	/*
	 *  Now, interrupts we are not able to recover cleanly.
	 *
	 *  Log message for hard errors.
	 *  Reset everything.
	 */

	sym_log_hard_error(shost, sist, dstat);

	if ((sist & (GEN|HTH|SGE)) ||
		(dstat & (MDPE|BF|ABRT|IID))) {
		sym_start_reset(np);
		return IRQ_HANDLED;
	}

unknown_int:
	/*
	 *  We just miss the cause of the interrupt. :(
	 *  Print a message. The timeout will do the real work.
	 */
	printf(	"%s: unknown interrupt(s) ignored, "
		"ISTAT=0x%x DSTAT=0x%x SIST=0x%x\n",
		sym_name(np), istat, dstat, sist);
	return IRQ_NONE;
}

/*
 *  Dequeue from the START queue all CCBs that match 
 *  a given target/lun/task condition (-1 means all),
 *  and move them from the BUSY queue to the COMP queue 
 *  with DID_SOFT_ERROR status condition.
 *  This function is used during error handling/recovery.
 *  It is called with SCRIPTS not running.
 */
static int 
sym_dequeue_from_squeue(struct sym_hcb *np, int i, int target, int lun, int task)
{
	int j;
	struct sym_ccb *cp;

	/*
	 *  Make sure the starting index is within range.
	 */
	assert((i >= 0) && (i < 2*MAX_QUEUE));

	/*
	 *  Walk until end of START queue and dequeue every job 
	 *  that matches the target/lun/task condition.
	 */
	j = i;
	while (i != np->squeueput) {
		cp = sym_ccb_from_dsa(np, scr_to_cpu(np->squeue[i]));
		assert(cp);
#ifdef SYM_CONF_IARB_SUPPORT
		/* Forget hints for IARB, they may be no longer relevant */
		cp->host_flags &= ~HF_HINT_IARB;
#endif
		if ((target == -1 || cp->target == target) &&
		    (lun    == -1 || cp->lun    == lun)    &&
		    (task   == -1 || cp->tag    == task)) {
			sym_set_cam_status(cp->cmd, DID_SOFT_ERROR);
			sym_remque(&cp->link_ccbq);
			sym_insque_tail(&cp->link_ccbq, &np->comp_ccbq);
		}
		else {
			if (i != j)
				np->squeue[j] = np->squeue[i];
			if ((j += 2) >= MAX_QUEUE*2) j = 0;
		}
		if ((i += 2) >= MAX_QUEUE*2) i = 0;
	}
	if (i != j)		/* Copy back the idle task if needed */
		np->squeue[j] = np->squeue[i];
	np->squeueput = j;	/* Update our current start queue pointer */

	return (i - j) / 2;
}

/*
 *  chip handler for bad SCSI status condition
 *
 *  In case of bad SCSI status, we unqueue all the tasks 
 *  currently queued to the controller but not yet started 
 *  and then restart the SCRIPTS processor immediately.
 *
 *  QUEUE FULL and BUSY conditions are handled the same way.
 *  Basically all the not yet started tasks are requeued in 
 *  device queue and the queue is frozen until a completion.
 *
 *  For CHECK CONDITION and COMMAND TERMINATED status, we use 
 *  the CCB of the failed command to prepare a REQUEST SENSE 
 *  SCSI command and queue it to the controller queue.
 *
 *  SCRATCHA is assumed to have been loaded with STARTPOS 
 *  before the SCRIPTS called the C code.
 */
static void sym_sir_bad_scsi_status(struct sym_hcb *np, int num, struct sym_ccb *cp)
{
	u32		startp;
	u_char		s_status = cp->ssss_status;
	u_char		h_flags  = cp->host_flags;
	int		msglen;
	int		i;

	/*
	 *  Compute the index of the next job to start from SCRIPTS.
	 */
	i = (INL(np, nc_scratcha) - np->squeue_ba) / 4;

	/*
	 *  The last CCB queued used for IARB hint may be 
	 *  no longer relevant. Forget it.
	 */
#ifdef SYM_CONF_IARB_SUPPORT
	if (np->last_cp)
		np->last_cp = 0;
#endif

	/*
	 *  Now deal with the SCSI status.
	 */
	switch(s_status) {
	case S_BUSY:
	case S_QUEUE_FULL:
		if (sym_verbose >= 2) {
			sym_print_addr(cp->cmd, "%s\n",
			        s_status == S_BUSY ? "BUSY" : "QUEUE FULL\n");
		}
	default:	/* S_INT, S_INT_COND_MET, S_CONFLICT */
		sym_complete_error (np, cp);
		break;
	case S_TERMINATED:
	case S_CHECK_COND:
		/*
		 *  If we get an SCSI error when requesting sense, give up.
		 */
		if (h_flags & HF_SENSE) {
			sym_complete_error (np, cp);
			break;
		}

		/*
		 *  Dequeue all queued CCBs for that device not yet started,
		 *  and restart the SCRIPTS processor immediately.
		 */
		sym_dequeue_from_squeue(np, i, cp->target, cp->lun, -1);
		OUTL_DSP(np, SCRIPTA_BA(np, start));

 		/*
		 *  Save some info of the actual IO.
		 *  Compute the data residual.
		 */
		cp->sv_scsi_status = cp->ssss_status;
		cp->sv_xerr_status = cp->xerr_status;
		cp->sv_resid = sym_compute_residual(np, cp);

		/*
		 *  Prepare all needed data structures for 
		 *  requesting sense data.
		 */

		cp->scsi_smsg2[0] = IDENTIFY(0, cp->lun);
		msglen = 1;

		/*
		 *  If we are currently using anything different from 
		 *  async. 8 bit data transfers with that target,
		 *  start a negotiation, since the device may want 
		 *  to report us a UNIT ATTENTION condition due to 
		 *  a cause we currently ignore, and we donnot want 
		 *  to be stuck with WIDE and/or SYNC data transfer.
		 *
		 *  cp->nego_status is filled by sym_prepare_nego().
		 */
		cp->nego_status = 0;
		msglen += sym_prepare_nego(np, cp, &cp->scsi_smsg2[msglen]);
		/*
		 *  Message table indirect structure.
		 */
		cp->phys.smsg.addr	= CCB_BA(cp, scsi_smsg2);
		cp->phys.smsg.size	= cpu_to_scr(msglen);

		/*
		 *  sense command
		 */
		cp->phys.cmd.addr	= CCB_BA(cp, sensecmd);
		cp->phys.cmd.size	= cpu_to_scr(6);

		/*
		 *  patch requested size into sense command
		 */
		cp->sensecmd[0]		= REQUEST_SENSE;
		cp->sensecmd[1]		= 0;
		if (cp->cmd->device->scsi_level <= SCSI_2 && cp->lun <= 7)
			cp->sensecmd[1]	= cp->lun << 5;
		cp->sensecmd[4]		= SYM_SNS_BBUF_LEN;
		cp->data_len		= SYM_SNS_BBUF_LEN;

		/*
		 *  sense data
		 */
		memset(cp->sns_bbuf, 0, SYM_SNS_BBUF_LEN);
		cp->phys.sense.addr	= CCB_BA(cp, sns_bbuf);
		cp->phys.sense.size	= cpu_to_scr(SYM_SNS_BBUF_LEN);

		/*
		 *  requeue the command.
		 */
		startp = SCRIPTB_BA(np, sdata_in);

		cp->phys.head.savep	= cpu_to_scr(startp);
		cp->phys.head.lastp	= cpu_to_scr(startp);
		cp->startp		= cpu_to_scr(startp);
		cp->goalp		= cpu_to_scr(startp + 16);

		cp->host_xflags = 0;
		cp->host_status	= cp->nego_status ? HS_NEGOTIATE : HS_BUSY;
		cp->ssss_status = S_ILLEGAL;
		cp->host_flags	= (HF_SENSE|HF_DATA_IN);
		cp->xerr_status = 0;
		cp->extra_bytes = 0;

		cp->phys.head.go.start = cpu_to_scr(SCRIPTA_BA(np, select));

		/*
		 *  Requeue the command.
		 */
		sym_put_start_queue(np, cp);

		/*
		 *  Give back to upper layer everything we have dequeued.
		 */
		sym_flush_comp_queue(np, 0);
		break;
	}
}

/*
 *  After a device has accepted some management message 
 *  as BUS DEVICE RESET, ABORT TASK, etc ..., or when 
 *  a device signals a UNIT ATTENTION condition, some 
 *  tasks are thrown away by the device. We are required 
 *  to reflect that on our tasks list since the device 
 *  will never complete these tasks.
 *
 *  This function move from the BUSY queue to the COMP 
 *  queue all disconnected CCBs for a given target that 
 *  match the following criteria:
 *  - lun=-1  means any logical UNIT otherwise a given one.
 *  - task=-1 means any task, otherwise a given one.
 */
int sym_clear_tasks(struct sym_hcb *np, int cam_status, int target, int lun, int task)
{
	SYM_QUEHEAD qtmp, *qp;
	int i = 0;
	struct sym_ccb *cp;

	/*
	 *  Move the entire BUSY queue to our temporary queue.
	 */
	sym_que_init(&qtmp);
	sym_que_splice(&np->busy_ccbq, &qtmp);
	sym_que_init(&np->busy_ccbq);

	/*
	 *  Put all CCBs that matches our criteria into 
	 *  the COMP queue and put back other ones into 
	 *  the BUSY queue.
	 */
	while ((qp = sym_remque_head(&qtmp)) != NULL) {
		struct scsi_cmnd *cmd;
		cp = sym_que_entry(qp, struct sym_ccb, link_ccbq);
		cmd = cp->cmd;
		if (cp->host_status != HS_DISCONNECT ||
		    cp->target != target	     ||
		    (lun  != -1 && cp->lun != lun)   ||
		    (task != -1 && 
			(cp->tag != NO_TAG && cp->scsi_smsg[2] != task))) {
			sym_insque_tail(&cp->link_ccbq, &np->busy_ccbq);
			continue;
		}
		sym_insque_tail(&cp->link_ccbq, &np->comp_ccbq);

		/* Preserve the software timeout condition */
		if (sym_get_cam_status(cmd) != DID_TIME_OUT)
			sym_set_cam_status(cmd, cam_status);
		++i;
#if 0
printf("XXXX TASK @%p CLEARED\n", cp);
#endif
	}
	return i;
}

/*
 *  chip handler for TASKS recovery
 *
 *  We cannot safely abort a command, while the SCRIPTS 
 *  processor is running, since we just would be in race 
 *  with it.
 *
 *  As long as we have tasks to abort, we keep the SEM 
 *  bit set in the ISTAT. When this bit is set, the 
 *  SCRIPTS processor interrupts (SIR_SCRIPT_STOPPED) 
 *  each time it enters the scheduler.
 *
 *  If we have to reset a target, clear tasks of a unit,
 *  or to perform the abort of a disconnected job, we 
 *  restart the SCRIPTS for selecting the target. Once 
 *  selected, the SCRIPTS interrupts (SIR_TARGET_SELECTED).
 *  If it loses arbitration, the SCRIPTS will interrupt again 
 *  the next time it will enter its scheduler, and so on ...
 *
 *  On SIR_TARGET_SELECTED, we scan for the more 
 *  appropriate thing to do:
 *
 *  - If nothing, we just sent a M_ABORT message to the 
 *    target to get rid of the useless SCSI bus ownership.
 *    According to the specs, no tasks shall be affected.
 *  - If the target is to be reset, we send it a M_RESET 
 *    message.
 *  - If a logical UNIT is to be cleared , we send the 
 *    IDENTIFY(lun) + M_ABORT.
 *  - If an untagged task is to be aborted, we send the 
 *    IDENTIFY(lun) + M_ABORT.
 *  - If a tagged task is to be aborted, we send the 
 *    IDENTIFY(lun) + task attributes + M_ABORT_TAG.
 *
 *  Once our 'kiss of death' :) message has been accepted 
 *  by the target, the SCRIPTS interrupts again 
 *  (SIR_ABORT_SENT). On this interrupt, we complete 
 *  all the CCBs that should have been aborted by the 
 *  target according to our message.
 */
static void sym_sir_task_recovery(struct sym_hcb *np, int num)
{
	SYM_QUEHEAD *qp;
	struct sym_ccb *cp;
	struct sym_tcb *tp = NULL; /* gcc isn't quite smart enough yet */
	struct scsi_target *starget;
	int target=-1, lun=-1, task;
	int i, k;

	switch(num) {
	/*
	 *  The SCRIPTS processor stopped before starting
	 *  the next command in order to allow us to perform 
	 *  some task recovery.
	 */
	case SIR_SCRIPT_STOPPED:
		/*
		 *  Do we have any target to reset or unit to clear ?
		 */
		for (i = 0 ; i < SYM_CONF_MAX_TARGET ; i++) {
			tp = &np->target[i];
			if (tp->to_reset || 
			    (tp->lun0p && tp->lun0p->to_clear)) {
				target = i;
				break;
			}
			if (!tp->lunmp)
				continue;
			for (k = 1 ; k < SYM_CONF_MAX_LUN ; k++) {
				if (tp->lunmp[k] && tp->lunmp[k]->to_clear) {
					target	= i;
					break;
				}
			}
			if (target != -1)
				break;
		}

		/*
		 *  If not, walk the busy queue for any 
		 *  disconnected CCB to be aborted.
		 */
		if (target == -1) {
			FOR_EACH_QUEUED_ELEMENT(&np->busy_ccbq, qp) {
				cp = sym_que_entry(qp,struct sym_ccb,link_ccbq);
				if (cp->host_status != HS_DISCONNECT)
					continue;
				if (cp->to_abort) {
					target = cp->target;
					break;
				}
			}
		}

		/*
		 *  If some target is to be selected, 
		 *  prepare and start the selection.
		 */
		if (target != -1) {
			tp = &np->target[target];
			np->abrt_sel.sel_id	= target;
			np->abrt_sel.sel_scntl3 = tp->head.wval;
			np->abrt_sel.sel_sxfer  = tp->head.sval;
			OUTL(np, nc_dsa, np->hcb_ba);
			OUTL_DSP(np, SCRIPTB_BA(np, sel_for_abort));
			return;
		}

		/*
		 *  Now look for a CCB to abort that haven't started yet.
		 *  Btw, the SCRIPTS processor is still stopped, so 
		 *  we are not in race.
		 */
		i = 0;
		cp = NULL;
		FOR_EACH_QUEUED_ELEMENT(&np->busy_ccbq, qp) {
			cp = sym_que_entry(qp, struct sym_ccb, link_ccbq);
			if (cp->host_status != HS_BUSY &&
			    cp->host_status != HS_NEGOTIATE)
				continue;
			if (!cp->to_abort)
				continue;
#ifdef SYM_CONF_IARB_SUPPORT
			/*
			 *    If we are using IMMEDIATE ARBITRATION, we donnot 
			 *    want to cancel the last queued CCB, since the 
			 *    SCRIPTS may have anticipated the selection.
			 */
			if (cp == np->last_cp) {
				cp->to_abort = 0;
				continue;
			}
#endif
			i = 1;	/* Means we have found some */
			break;
		}
		if (!i) {
			/*
			 *  We are done, so we donnot need 
			 *  to synchronize with the SCRIPTS anylonger.
			 *  Remove the SEM flag from the ISTAT.
			 */
			np->istat_sem = 0;
			OUTB(np, nc_istat, SIGP);
			break;
		}
		/*
		 *  Compute index of next position in the start 
		 *  queue the SCRIPTS intends to start and dequeue 
		 *  all CCBs for that device that haven't been started.
		 */
		i = (INL(np, nc_scratcha) - np->squeue_ba) / 4;
		i = sym_dequeue_from_squeue(np, i, cp->target, cp->lun, -1);

		/*
		 *  Make sure at least our IO to abort has been dequeued.
		 */
#ifndef SYM_OPT_HANDLE_DEVICE_QUEUEING
		assert(i && sym_get_cam_status(cp->cmd) == DID_SOFT_ERROR);
#else
		sym_remque(&cp->link_ccbq);
		sym_insque_tail(&cp->link_ccbq, &np->comp_ccbq);
#endif
		/*
		 *  Keep track in cam status of the reason of the abort.
		 */
		if (cp->to_abort == 2)
			sym_set_cam_status(cp->cmd, DID_TIME_OUT);
		else
			sym_set_cam_status(cp->cmd, DID_ABORT);

		/*
		 *  Complete with error everything that we have dequeued.
	 	 */
		sym_flush_comp_queue(np, 0);
		break;
	/*
	 *  The SCRIPTS processor has selected a target 
	 *  we may have some manual recovery to perform for.
	 */
	case SIR_TARGET_SELECTED:
		target = INB(np, nc_sdid) & 0xf;
		tp = &np->target[target];

		np->abrt_tbl.addr = cpu_to_scr(vtobus(np->abrt_msg));

		/*
		 *  If the target is to be reset, prepare a 
		 *  M_RESET message and clear the to_reset flag 
		 *  since we donnot expect this operation to fail.
		 */
		if (tp->to_reset) {
			np->abrt_msg[0] = M_RESET;
			np->abrt_tbl.size = 1;
			tp->to_reset = 0;
			break;
		}

		/*
		 *  Otherwise, look for some logical unit to be cleared.
		 */
		if (tp->lun0p && tp->lun0p->to_clear)
			lun = 0;
		else if (tp->lunmp) {
			for (k = 1 ; k < SYM_CONF_MAX_LUN ; k++) {
				if (tp->lunmp[k] && tp->lunmp[k]->to_clear) {
					lun = k;
					break;
				}
			}
		}

		/*
		 *  If a logical unit is to be cleared, prepare 
		 *  an IDENTIFY(lun) + ABORT MESSAGE.
		 */
		if (lun != -1) {
			struct sym_lcb *lp = sym_lp(tp, lun);
			lp->to_clear = 0; /* We don't expect to fail here */
			np->abrt_msg[0] = IDENTIFY(0, lun);
			np->abrt_msg[1] = M_ABORT;
			np->abrt_tbl.size = 2;
			break;
		}

		/*
		 *  Otherwise, look for some disconnected job to 
		 *  abort for this target.
		 */
		i = 0;
		cp = NULL;
		FOR_EACH_QUEUED_ELEMENT(&np->busy_ccbq, qp) {
			cp = sym_que_entry(qp, struct sym_ccb, link_ccbq);
			if (cp->host_status != HS_DISCONNECT)
				continue;
			if (cp->target != target)
				continue;
			if (!cp->to_abort)
				continue;
			i = 1;	/* Means we have some */
			break;
		}

		/*
		 *  If we have none, probably since the device has 
		 *  completed the command before we won abitration,
		 *  send a M_ABORT message without IDENTIFY.
		 *  According to the specs, the device must just 
		 *  disconnect the BUS and not abort any task.
		 */
		if (!i) {
			np->abrt_msg[0] = M_ABORT;
			np->abrt_tbl.size = 1;
			break;
		}

		/*
		 *  We have some task to abort.
		 *  Set the IDENTIFY(lun)
		 */
		np->abrt_msg[0] = IDENTIFY(0, cp->lun);

		/*
		 *  If we want to abort an untagged command, we 
		 *  will send a IDENTIFY + M_ABORT.
		 *  Otherwise (tagged command), we will send 
		 *  a IDENTITFY + task attributes + ABORT TAG.
		 */
		if (cp->tag == NO_TAG) {
			np->abrt_msg[1] = M_ABORT;
			np->abrt_tbl.size = 2;
		} else {
			np->abrt_msg[1] = cp->scsi_smsg[1];
			np->abrt_msg[2] = cp->scsi_smsg[2];
			np->abrt_msg[3] = M_ABORT_TAG;
			np->abrt_tbl.size = 4;
		}
		/*
		 *  Keep track of software timeout condition, since the 
		 *  peripheral driver may not count retries on abort 
		 *  conditions not due to timeout.
		 */
		if (cp->to_abort == 2)
			sym_set_cam_status(cp->cmd, DID_TIME_OUT);
		cp->to_abort = 0; /* We donnot expect to fail here */
		break;

	/*
	 *  The target has accepted our message and switched 
	 *  to BUS FREE phase as we expected.
	 */
	case SIR_ABORT_SENT:
		target = INB(np, nc_sdid) & 0xf;
		tp = &np->target[target];
		starget = tp->starget;
		
		/*
		**  If we didn't abort anything, leave here.
		*/
		if (np->abrt_msg[0] == M_ABORT)
			break;

		/*
		 *  If we sent a M_RESET, then a hardware reset has 
		 *  been performed by the target.
		 *  - Reset everything to async 8 bit
		 *  - Tell ourself to negotiate next time :-)
		 *  - Prepare to clear all disconnected CCBs for 
		 *    this target from our task list (lun=task=-1)
		 */
		lun = -1;
		task = -1;
		if (np->abrt_msg[0] == M_RESET) {
			tp->head.sval = 0;
			tp->head.wval = np->rv_scntl3;
			tp->head.uval = 0;
			spi_period(starget) = 0;
			spi_offset(starget) = 0;
			spi_width(starget) = 0;
			spi_iu(starget) = 0;
			spi_dt(starget) = 0;
			spi_qas(starget) = 0;
			tp->tgoal.check_nego = 1;
			tp->tgoal.renego = 0;
		}

		/*
		 *  Otherwise, check for the LUN and TASK(s) 
		 *  concerned by the cancelation.
		 *  If it is not ABORT_TAG then it is CLEAR_QUEUE 
		 *  or an ABORT message :-)
		 */
		else {
			lun = np->abrt_msg[0] & 0x3f;
			if (np->abrt_msg[1] == M_ABORT_TAG)
				task = np->abrt_msg[2];
		}

		/*
		 *  Complete all the CCBs the device should have 
		 *  aborted due to our 'kiss of death' message.
		 */
		i = (INL(np, nc_scratcha) - np->squeue_ba) / 4;
		sym_dequeue_from_squeue(np, i, target, lun, -1);
		sym_clear_tasks(np, DID_ABORT, target, lun, task);
		sym_flush_comp_queue(np, 0);

 		/*
		 *  If we sent a BDR, make upper layer aware of that.
 		 */
		if (np->abrt_msg[0] == M_RESET)
			starget_printk(KERN_NOTICE, starget,
							"has been reset\n");
		break;
	}

	/*
	 *  Print to the log the message we intend to send.
	 */
	if (num == SIR_TARGET_SELECTED) {
		dev_info(&tp->starget->dev, "control msgout:");
		sym_printl_hex(np->abrt_msg, np->abrt_tbl.size);
		np->abrt_tbl.size = cpu_to_scr(np->abrt_tbl.size);
	}

	/*
	 *  Let the SCRIPTS processor continue.
	 */
	OUTONB_STD();
}

/*
 *  Gerard's alchemy:) that deals with with the data 
 *  pointer for both MDP and the residual calculation.
 *
 *  I didn't want to bloat the code by more than 200 
 *  lines for the handling of both MDP and the residual.
 *  This has been achieved by using a data pointer 
 *  representation consisting in an index in the data 
 *  array (dp_sg) and a negative offset (dp_ofs) that 
 *  have the following meaning:
 *
 *  - dp_sg = SYM_CONF_MAX_SG
 *    we are at the end of the data script.
 *  - dp_sg < SYM_CONF_MAX_SG
 *    dp_sg points to the next entry of the scatter array 
 *    we want to transfer.
 *  - dp_ofs < 0
 *    dp_ofs represents the residual of bytes of the 
 *    previous entry scatter entry we will send first.
 *  - dp_ofs = 0
 *    no residual to send first.
 *
 *  The function sym_evaluate_dp() accepts an arbitray 
 *  offset (basically from the MDP message) and returns 
 *  the corresponding values of dp_sg and dp_ofs.
 */

static int sym_evaluate_dp(struct sym_hcb *np, struct sym_ccb *cp, u32 scr, int *ofs)
{
	u32	dp_scr;
	int	dp_ofs, dp_sg, dp_sgmin;
	int	tmp;
	struct sym_pmc *pm;

	/*
	 *  Compute the resulted data pointer in term of a script 
	 *  address within some DATA script and a signed byte offset.
	 */
	dp_scr = scr;
	dp_ofs = *ofs;
	if	(dp_scr == SCRIPTA_BA(np, pm0_data))
		pm = &cp->phys.pm0;
	else if (dp_scr == SCRIPTA_BA(np, pm1_data))
		pm = &cp->phys.pm1;
	else
		pm = NULL;

	if (pm) {
		dp_scr  = scr_to_cpu(pm->ret);
		dp_ofs -= scr_to_cpu(pm->sg.size) & 0x00ffffff;
	}

	/*
	 *  If we are auto-sensing, then we are done.
	 */
	if (cp->host_flags & HF_SENSE) {
		*ofs = dp_ofs;
		return 0;
	}

	/*
	 *  Deduce the index of the sg entry.
	 *  Keep track of the index of the first valid entry.
	 *  If result is dp_sg = SYM_CONF_MAX_SG, then we are at the 
	 *  end of the data.
	 */
	tmp = scr_to_cpu(cp->goalp);
	dp_sg = SYM_CONF_MAX_SG;
	if (dp_scr != tmp)
		dp_sg -= (tmp - 8 - (int)dp_scr) / (2*4);
	dp_sgmin = SYM_CONF_MAX_SG - cp->segments;

	/*
	 *  Move to the sg entry the data pointer belongs to.
	 *
	 *  If we are inside the data area, we expect result to be:
	 *
	 *  Either,
	 *      dp_ofs = 0 and dp_sg is the index of the sg entry
	 *      the data pointer belongs to (or the end of the data)
	 *  Or,
	 *      dp_ofs < 0 and dp_sg is the index of the sg entry 
	 *      the data pointer belongs to + 1.
	 */
	if (dp_ofs < 0) {
		int n;
		while (dp_sg > dp_sgmin) {
			--dp_sg;
			tmp = scr_to_cpu(cp->phys.data[dp_sg].size);
			n = dp_ofs + (tmp & 0xffffff);
			if (n > 0) {
				++dp_sg;
				break;
			}
			dp_ofs = n;
		}
	}
	else if (dp_ofs > 0) {
		while (dp_sg < SYM_CONF_MAX_SG) {
			tmp = scr_to_cpu(cp->phys.data[dp_sg].size);
			dp_ofs -= (tmp & 0xffffff);
			++dp_sg;
			if (dp_ofs <= 0)
				break;
		}
	}

	/*
	 *  Make sure the data pointer is inside the data area.
	 *  If not, return some error.
	 */
	if	(dp_sg < dp_sgmin || (dp_sg == dp_sgmin && dp_ofs < 0))
		goto out_err;
	else if	(dp_sg > SYM_CONF_MAX_SG ||
		 (dp_sg == SYM_CONF_MAX_SG && dp_ofs > 0))
		goto out_err;

	/*
	 *  Save the extreme pointer if needed.
	 */
	if (dp_sg > cp->ext_sg ||
            (dp_sg == cp->ext_sg && dp_ofs > cp->ext_ofs)) {
		cp->ext_sg  = dp_sg;
		cp->ext_ofs = dp_ofs;
	}

	/*
	 *  Return data.
	 */
	*ofs = dp_ofs;
	return dp_sg;

out_err:
	return -1;
}

/*
 *  chip handler for MODIFY DATA POINTER MESSAGE
 *
 *  We also call this function on IGNORE WIDE RESIDUE 
 *  messages that do not match a SWIDE full condition.
 *  Btw, we assume in that situation that such a message 
 *  is equivalent to a MODIFY DATA POINTER (offset=-1).
 */

static void sym_modify_dp(struct sym_hcb *np, struct sym_tcb *tp, struct sym_ccb *cp, int ofs)
{
	int dp_ofs	= ofs;
	u32	dp_scr	= sym_get_script_dp (np, cp);
	u32	dp_ret;
	u32	tmp;
	u_char	hflags;
	int	dp_sg;
	struct	sym_pmc *pm;

	/*
	 *  Not supported for auto-sense.
	 */
	if (cp->host_flags & HF_SENSE)
		goto out_reject;

	/*
	 *  Apply our alchemy:) (see comments in sym_evaluate_dp()), 
	 *  to the resulted data pointer.
	 */
	dp_sg = sym_evaluate_dp(np, cp, dp_scr, &dp_ofs);
	if (dp_sg < 0)
		goto out_reject;

	/*
	 *  And our alchemy:) allows to easily calculate the data 
	 *  script address we want to return for the next data phase.
	 */
	dp_ret = cpu_to_scr(cp->goalp);
	dp_ret = dp_ret - 8 - (SYM_CONF_MAX_SG - dp_sg) * (2*4);

	/*
	 *  If offset / scatter entry is zero we donnot need 
	 *  a context for the new current data pointer.
	 */
	if (dp_ofs == 0) {
		dp_scr = dp_ret;
		goto out_ok;
	}

	/*
	 *  Get a context for the new current data pointer.
	 */
	hflags = INB(np, HF_PRT);

	if (hflags & HF_DP_SAVED)
		hflags ^= HF_ACT_PM;

	if (!(hflags & HF_ACT_PM)) {
		pm  = &cp->phys.pm0;
		dp_scr = SCRIPTA_BA(np, pm0_data);
	}
	else {
		pm = &cp->phys.pm1;
		dp_scr = SCRIPTA_BA(np, pm1_data);
	}

	hflags &= ~(HF_DP_SAVED);

	OUTB(np, HF_PRT, hflags);

	/*
	 *  Set up the new current data pointer.
	 *  ofs < 0 there, and for the next data phase, we 
	 *  want to transfer part of the data of the sg entry 
	 *  corresponding to index dp_sg-1 prior to returning 
	 *  to the main data script.
	 */
	pm->ret = cpu_to_scr(dp_ret);
	tmp  = scr_to_cpu(cp->phys.data[dp_sg-1].addr);
	tmp += scr_to_cpu(cp->phys.data[dp_sg-1].size) + dp_ofs;
	pm->sg.addr = cpu_to_scr(tmp);
	pm->sg.size = cpu_to_scr(-dp_ofs);

out_ok:
	sym_set_script_dp (np, cp, dp_scr);
	OUTL_DSP(np, SCRIPTA_BA(np, clrack));
	return;

out_reject:
	OUTL_DSP(np, SCRIPTB_BA(np, msg_bad));
}


/*
 *  chip calculation of the data residual.
 *
 *  As I used to say, the requirement of data residual 
 *  in SCSI is broken, useless and cannot be achieved 
 *  without huge complexity.
 *  But most OSes and even the official CAM require it.
 *  When stupidity happens to be so widely spread inside 
 *  a community, it gets hard to convince.
 *
 *  Anyway, I don't care, since I am not going to use 
 *  any software that considers this data residual as 
 *  a relevant information. :)
 */

int sym_compute_residual(struct sym_hcb *np, struct sym_ccb *cp)
{
	int dp_sg, dp_sgmin, resid = 0;
	int dp_ofs = 0;

	/*
	 *  Check for some data lost or just thrown away.
	 *  We are not required to be quite accurate in this 
	 *  situation. Btw, if we are odd for output and the 
	 *  device claims some more data, it may well happen 
	 *  than our residual be zero. :-)*
 */
	if (cp->xerr_status & (XE_EXTRA_DATA|XE_SODL_UNRUNI-SCSWIDE_OVRUN)) {
	C 53C8XX and 53C1010 faily 
 * of PC)
			ver f -= 8XX extra_bytes;ight (C) 1999-2001  Gerard RSI IO procoudie++ver f(c) 2003-2005  Matthew Wilcox s.
 *
 * Coudie--
 *
 * T}

	/**
 * DIf all data has been transferred,*
 * Devere is noiver for .SILOGIC 53C8XX phys.head.lastp =fr>
 *goalpsym5returniver f;.
 * Copyrightno) 1998rard Rou occurs, or if* Th) 199*
 * Dpointersym5weird, been 
 full8xx driver is derived frostarxx driver m the ncr53c8xx ||
	    sym_evaluate_dp(np, cp, scr_to_cpud from the ncr53c8xx),udieier &dp_ofs) < 0pyrighbeen 
 8XX  199_len -Copyroddright_adjustmentiver.
 * Copyrightwe whe sauto-sensing,* Thnor care doner is derived frohost_flag0 faHF_SENSEoeln.de>
 * C-se@mi.iver.
 * CopyriW detecnown writcomfortable in.
 *
computation/*
 * Do3.
 *
 * Tiver for t(2's-----lemeie)r is derdp_sgmin = SYM_CONF_MAX_SG 1994  segmeiemousr@free= ->
 * Codormousfor ( * Th fr>
 * Co_sg;  * it <ogram is free so; ++ * Thpyrighu_int tmp =>
 *         Stefan E 199[ * Th].size)(c) r@free+= (shed& 0xfLicenither.
 r@free.fr>
 *Wolfgang Stanglmeier

 * CopyriHope wriy * NViverultsym53ct too wrongr is derbeen 
 * a por}

/*
 * DNegoti------difys.
  and SYNCHRONOUS f PC TRANSFER.
 * but WNVRAM dtry to nITHOUT e,AM deppendm is ITY or F----message but tom is identify; wit(maybe) si---- tagOSE.  Serant  The  Cop 3C1010 fieldsym5sein t HS_NEGOTIATEa comark this.
 *
situ PURPranty of
I3.
 *
target doesn't answerl PubOSE.  Se immedr FIly.
 *
(as required bym is standard)am is SIR of t_FAILED nal nrupt.
 *
wil the raised eventuallys.
 *
 * Yoandlerivemovesm is py of the GNU3C1010,; witsetparam.
 *
ITY or FId   <wee
 * GNUdefatrib(async /o.ukide) along with ws diceive a matchingot, writ Software
 TNESScheck i1-1307difyvalidity in unitsm is sym_gsif 0
#define SYM_DEBUG_GEReject the Free Software
 TNESS ssumeof HZ */

#includ-----200failed in unf (C)backa comple Plaid sym_int_ma (struct sym_hcb *nICULAR PURPOSE.  Se whilco.ut----py of the GNlic Lit FITNit's as prograiniclude "
static str.demoprepeteca* Founhis progr)totypeot, wri,ved a driparameteux-1n unitndc_ccb(e the
 to t, write * GNU progr along with this program; if nofetchm is t, writ(noom_dsa(stout phase),-1307 truct sym_hc;
static struct sym_ccb *sym_alloc_ccb(strincludelic Liettings (te 330, BPROTO MA  02111#if 0
#def * MERCHatic void sym_TNESS tangl* Thm---- (C)ccb000 long_SUPe the
 * Gis;
stati,----------ntroller'atiogistd sy wit-------"m th"tion pe rec-------intf (" %x", struct     hcbrant/,
 * but chip
#includedifyhout even the implied war REQUEST (SDTR)e details.
 /
aticic MA   
    ym_n_ITY _ded f(t sym_ccb *cp, *ogneym_preq,ct sym_ccb *ct_mscp)
{
	ublisprografr>
 *m_prin;
	u_char	chg, ofs, pd syfak, divion. 53CDEBUG_FLAGS & , u_chof tpyrigh    printcp->cmmsglogne (n-- > "ym_nvmsgin", np->fo(&te, or
 ** CopyriGe;
	pruesde "sym_gsr is dercht un0;
	per =>starget->[3];
	ofs
}

/*
 *  Pr4]ion.
 *
 * TCed funt_msg againscomplelimitg(msg);
	 53Cmi.Unright (Comet>>stargaxoffssym53{printf1;np, 
}

/*
 tatus);} or
 *ruct scsi_cmnd *c");
<>starginym_n
{
	if (x_statu");
}

/*
 ed SCS) {
		sy: ", label);new u_chaym_nhronousete_error (errors----
 *
 iv =rgetntf("\nd *cmd, &&     getym_nlogne0int tar&div, &fakUni-Ko
		gotc8xx;
st_ision.abel, u_char *msg)
{
	struct sym_tcb *tp = &addrC8XX cmer
 			"sdtr:np, =%d.\n", "Odiv, "Ofak, "Ochg=%d.\n"m_prinp, int tarsym_pget, chgdev, "%s: ", labIf it wact snp, strur coabliso change,/*
 * Deven_priiif notccep-----.np);
staitr is derived!reqx_st_SWIcmd, "illegal scsi ph* CopyriApplyTA) {nt_msg(msg);
	nt_ad& XE_B et[target];
	}
	if (x_status & )ort of the Frrint_addr(cmd, cb *netection and readingI BUhad been 
 0se SMODE_SE:	return ;

	spi_. P, u_chardr(cmd, " details.	switchpi_populolf@ddr(carget[argetoutint tarmi.Ur *label, u_char *msg)
{
	struct sym_tcb *tp = &np->target[target];
	dev_info(outp->stargetoute, or
 *starget-> [0] = M_NOOPion.return "??"legal scs:tch(mode) {
	case SMODE_HVD0uct sym_hithe Waltham1ful,
lse
		svoid     ddr(cp->c);

	spi_print_msg(mst sym_ccb *tt_mst(10);
	OUTB(np

static void sy BUSstatox1);
	ustriion.
 *
 * TR
	spi_1.2.t, writ?xerr(structINBlogneHS_PRT) dripy of the GNsi_cmnOUTt reset the set tBUSYither 53C8XX p->cm3C1010 f&Copyrf we set 
 *!= NS_houtsym53, "illegal scsi er vdela("\nr.
 * Copyrinded e witamode(int mode)
{
	switcdistrib=ST);
	INB(np, md, "")ogne	princpithe 53C	/* Fo)	/* No}
}

/*
 *  oft rn a stLOGICe wrong time when  the cq) { */
Woft reset thsym_nd;	/*ponse.t (strhe SRST (soft reet) bit aen SOUTL_DSP  sofSCRIPTB_BA  sof_add_(!(n)e, or
	else	istat = 0;
	(np->fTNESS tection t (str_istat1) & SCRUN))
A	goto doclracket;

been 
4.
 */
static v_istat1) & SCRUN))
		goto domsg_badet;
l,
 * but u_char *msg)
{
	iPARALLEL n hexCOLr(cp->cmd,PPs: ", label);
	else
		sym_pint_apprcp->cmd, "");

	spi_print_msg(msg);
	prind sym_princ voi);
	OUTB(np, nc_is = &causm_prin[m_prinnt sunsigne OUTarrget, char id sydtXE_SWPTS 
 x.\n", sym_name(");
}

/*
 *  Print s\n", sym_name(omething that te5art reset process.h"


}

/*
 *  Pr6art reset process.
ptething that te7] & PPR_OPT_MASKr *label, u_char *msg)
{
	struct sym_tcb *tp = &np->target[target];
	dpprnfo(&tp->starget->dev, "%s: ", labnded errors.
 */
void sym_print_xerr(structreiniint x_stah"

#i_cmn (x_stat
		reinitializee chip;

	OU "LVDreini|| ! causfeature0 faFE_U3EN)
 */id sym_t:
	sd *cmd sy!=caseart_reset(struct sym_hcb RQD if (enab_i
	d ope *  ptruct sym_DTt and 
	 scsi_cmn\n", sym_name(ITY_ERR = dt ?E_PARITY_ERR_dt :E_PARITY_ERR)cmnd *cmd, instatus)
i_cmnif (enab_intsomethic_scntl1, C {
		sym_print_addr((np, nc_dcntl, ed SCSv_dcntl & IRQed SCSOUTB(np, ncf (x_stdr(cmd, "unrered SCSImbox1);
	udelay(20");
}
CSI bus shoatus & XE_EXTRA_DATA) {
		sym_print_addr(cmd, "extraneous data discarded.\n");
	}
	if (x_status & XE_BAD_PH_chi) {
		sym_print_addr(cmd, "illegal scsi phN) {
		sym_print_addr(cmd, "ODD transfer in DATA IN phase.\n");
	}
}

/*
 *  Return a string for SCSI BUS mode.
 */
static char *sym_scsi_bus_mode(int mode)
{
	switch(mode)pprotase SMODE_HVD:ptx-1., int tarh"

 "HVD";
	case SMODE_SE:	return "SE";
	case SMODE_LVD: return "LVD";
	}
	return "??";
}

/*
 *  Soft reset the chip.
 *
 *  Raising SRST when the chip is runnc_imay cause 
 *  problems d15-8   ) <<on dual function chips (see below).
 *  On the other hand, LVD devices ym_hcb e delay 
 *  to settle and report actual BUS mode in STEST4.
 */
static void sym nc_sp_reset (struct sym_hcb m_hcb *np)N) {
		sym_priuct  devics disnp->fDevit shoulf (!(tribun /*
 * DSTTNESSmayD transfeANTAa legace(inTHOUT ANY s rurring for SCSI BUS mo!			syi_cmnt    that.perioribuper shoncy
 */
sttus)int_normouselectclock(width =15-8 cb *np, u_chariu = ncy
 */
sts.
	 ncy
 */
stqaresent  *np, u_charded fcp->celay(20}p)
{
	OUTB(np, nc_istat, SRST);
nc_istatnc_mbox1);
	udelay(10);
	OUTB(np, nc_istat, 0);
	INB(np, nc_mbox1);
	udelay(2000);	/* For BUS MODE to settle */
}

/*
 *  Really soft reset the chip.:)
 *
 *  Some 896 and 876 chip revisions may hang-up if we set 
 *  the SRST (soft reset) bak;
 the wrong time when SCRIPTS 
 *  are running.
 *  So, we need to abort the current operatinc_istat, 0);
	 soft reset     "ISting the chip.
 */
static void sym_soft_reset (struct sym_hcb *np)
{
	u_char istat = 0;
	int i;

	if (!(np->features & FE_ISTAT1) || !(INPPR, nc_istat1) & SCRUN))
		goto donc_i_reset;

	OUTB(np, nc_istat, CABRT);
	for (i = 100000 ; i ; --i) {
		istat = INB(np, nc_istat);
		if (istat & SIP) {
			INW(np, nc_sist);
		}
		else if (istat & DIP) {
			if (INB(npANTY;sym_print_addr(cp->cmd,W"%s: ", label);
	else
		sym_print_ah"

cp->cmd, "");

	spi_print_msg(msg);
	printf("\n");
}

static void sym_print_nego_msg(struct sym_hcb *n
{
	/**label, u_char *msg)
{
	struct sym_tcb *tp = &np->target[target];
	dreinifo(&tp->starget->dev, "%s: ", label);

	spi_print_msg(msg);
	prinPTS 
 *reinitialize the int 
	u32 term;
	int retv = 0;

	sym_soft_reset(np);	/* Soft reset the chip */
	if (enab_int)
		OUTW(np, nc_sien, Rphase (4/5).\n");
	}
	if (x_status & XE_SODL_UNRUN) {
		sym "waddr(h"

TA OUT phase.\n");
	5-8   _SWIDE_OVRUN) {
		sym_print_addr(cmd, "ODD transfer in DATA IN phase.\n");
	}
}

/*
 *  Return a string for SCSI BUS mode.
 */
static char *sym_scsi_bus_mode(int mode)
{
	switch(mode)reiniet[target];
	h"

#se SMODE_SE:	return "SE";
	case SMODE_LVD: return "LVD";
	}
	return "??";
}

/*
 *  Soft reset the chip.
 *
 *  Raising SRST when the chip is run scntata while resettinp, int gnd report actual BUS mode the correct value.
 *  THERE IS NO SAFE DEFAULT VALUE.
 *
 *  Most NCR/SYMe delay 
 *  to settle ain STEST4.
 */
static v
{
	OUTB(np, nc_istat, SRST);
*
 *  Thinc_mbox1);
	udelay(10);
	OUTB(np, nc_istat, 0);
	INB(np, nc_mbox1);
	udelay(2000);	/* For BUS MODE to settle */
}

/*
 *  Really soft reset the chip.:)
 *
 *  Some 896 and 876 chip revisions may hang-up if we set 
 *  the SRST (soft reset) bs.
 ise wait 50 micro-seconds (at least).
	 */
	if (np->features & FE_LCKFRQ) {
		int i = 20;
		*
 *  This is es soft resetting the chip.
 */
static void sym_soft_reset (struct sym_hcb *np)
{
	u_char istat = 0;
	int i;

	if (!(np->features & FE_ISTAT1) || !(INs.
 , nc_istat1) & SCRUN))
		goto dowithnc_stest3,  TB(n {p, nc_istat, CABRT)00000 ;* Co
 * WITHOUT e
{
	if (lendif

/*
 * afl ncANTY;(!(np->fe
		OUTTruct llowsABILITY or FI
{
	ibothRANTY; without -----
 * a 
 *
le SCSI----mPubliSuggpi_priby Justin Gibbs----ILOGICELAY lectclock(struct*  Reade chip is running may cause 
 *  pncy
 */
static vm_prinelectclock(structint gime1, , u_char *msg)
{
	struct sym_t_tcb *tp = &np->target[tat4) & LCKFm_prinier disable general need some delay 
 *  to sett bitox1);& FE_ISTAT1) || !(INB(np, nc6 and 876 chip revisi *
 *  Som
	if (nptat1) & SCRUN))
		goto do_chip_reset;

.de>
 * ty biFE_C1W(np, nc_sien, 0);
		OU = INB(np, nc_istat)e CPU is s (istat & SIP) {
			INW(np, nc_sist);
		}
		else if (istat & DIPReuct DT,p, nc_sp, nc_sccb *cp);
 *  Printranty of
Called wase.b *sym_ccb_from; isym_hsucceincli Thent n)
o
 * ;
st----or  	 nc_socol errorranty of
Aym_printUS_CHundermple if (PPRom_dsa(stECK == nevo KHz 
 soft_reseprintfSE. _SUPwith);
		ifvery unlikenux/slabSo,.13.-33 resakes1 <<blemstatic	OUToid s transfee the
scntl1, 0);
	return retv;
}

/*;
	else
		s, SRST);
ior tincludenc_mbox1);
	udelay(10);
	OUTB(np, nc_istat, 0);
	INB(np, nc_mboxswim_hc-up if we set 
 frequcase nc_mbo:
#if 0m_tcb *? "dp1,d15-8,"_stime1, 0)			(u_long)term, (u_#t presce1, gen);  /* atic vorecovered SCSI parncy
 */
static voidor SCSI bus shorts lectclock(struct int x_status)
{
	ilectclock(struct syp, nc_scntl1, C *  If multiplier not present or not selected, leave here.
	 */
	if (np->mult#endif
		break;)
{
	u_inthout:int gen =  {
	case Sq (np, gen);	/* throw UTW(n char scntl3 = ns.
 v_scntl3;
	 (struct sq (np, gen);	est1;
	unsign} many loop iterations (if many looout/895A with cloc3 specific setting0(istat & DIP) {
			if (INB(npMESSAGE REJECTYM_DEBUGtf ("UP_SCSI_BUm_verboPPR,, nc_s
	if (label)
		truct sym_hcGEN=%d): %u msec, %u KHz\n*/
	f edym_name(np), gen, ms/4, f);

	return f;
}

static unsigned sym_ge%u KHz\n",
			syt[tarf1 = UTW( and 876 chip revisions maystat & DIP) {
	ex
/*
struct*msg)
{
	iprograSoft MA  02111l3, 0=%d): %u msec, %u  = &sir);

	spi_print_msg(c voit sym_hnum	= oft reseinatspsym_	u32	dsa		OUTL(np, nc_stat1, t, 0);
	INB(np, nc	peraticcb_fromuble */
	tler ist sym_hm_prin		OUTB(np, nc_sdid) the 0f is OFF */
		f, nc_is	
		       "ISTAT=0x%02x.int	tmpurn the correct value.
 *  TTINY) tp = f ("I#%dp->sumint getfreq (	f1  { f2;
 ogram is fDMA_ADDRESSING_MODE dri2_long)(2<<RUN))
S tell uparaat * 2) / hav>= 2)updivid*
 * D64 bit DMA  you ca *p++);
	g(msg);
	p
	u_te 3DMAP_DIRTYv_scntl3sumed\_dmap_regs(nfreque wronout{
	unsigneu32 term;e gener-2000  Ge------de "o ear* (43rintdi------------r= 5)	f1 = butions:
ediplier	= mult;
		COMPLETE_ERRORv_scntl3 80000;
_* (43logne.dither v/*
 	 ** Copyri * YC cod sym5currentlyose _SUP"illeco a fp);
 sometC_SU when  Typiclinu, us, "Otransfeabortlock ble geneiplier	= mult;
		RUN))
_STOPPED:	= mult;
		TARGET_SELECT*  For now, weABOR neeNTv_scntl3;ir_task_/probe y(np, nf1 = *= np->multiplier;
	np-SYM_SETdidf nogo GetMSG OUT;
statUTB(np,hav, intcy
 *0  Gesellier 	else ATN..demodosym_h_getpcict scnt: clocb *np)
{
	int f =EL_ATN_NO_MSG_OU
	 *  cmd*tp = k(KERN_WARNING1 = 40	sym_printN & FE_66MHZ) {
#else
	OUTB(n----nc_stest\n"f1 =  40000;
_stucr sc.
	 */
#if 1
	if (np->featetfreq s & FE_INHZ) {
#else
	{
#endif (1) reOUTB(np,  GNU b_tags4340lier	= mult;
		RE	f =_getfreINp);
		OUTB(np, nc_stest1, 0);
	}
	np->pciclk_khz = ,000 in order talculatiionhip clock divisor table.
 *
 *  DA_5M, 16*_5M};

/am is SYM_SETs;
		aom_dsa(st clockaif no{
#endan IDENTIFY
 */
#define _5M 5000000
s
sym_getp);
		OUTB(np, nc_stest1, 0);
	}
	np->pciclk_kh
sym_get2*_5M, 16*_5M};

/*
 *  Get clock factor and sync dif 1
	if (nalculationsa LUN"ODDK); /* k.uk>abouas SCSI clock */
	500000BAD_LU consk multiplier selec5000T1 =  40000;
		 kHz	*/
	int	div = np->clock_didifyan untagg (strxuct sd"ODDo make 
 ef not,ysupported	*/
	u32	fak;			/*I_T_Lnc factor in sxfer		*/e act	per;			/* Period in tnths of ns	*/
	u32	kpc;			/ mor* clk)			* clock mK); /* 	/*
mpute the synchronous period in _Qtenths of nano-seconds
	 */_TAG	per;			/* Period in tenths				prinlet"%s: divi cloc
 *  synchr-200grabbed--------using SRST*/
	in USAlock (e_rejob
 */
#define _5M 500000e act*  Fofacto3c8xmit unk multiplier 1010k multiplier selected
	 *
		OUTB(np, nc_stest1, 0);
	}
	np->pcic"t C10 re%xronouson baf (!(_5M};

/*
 ->starf the Sf1 =  40000;
		t = per;

	kpc = per * clk;
	if (dt)
	 factor pe-2000  Ge{
#end convssprogrronousue.h"
#inM_SE
 */
#define _5M tfreq(n_DON->mulng of the SCSI clocking.
	 *  Note that this limits the l/* SCK ==  SYM_linu cr (i-----aet (str 53Cng of the SCSal BPARITY *  c > (div_10M[div]IDler sy*  Read 53C8Xnt in 1/8XX and 53C1010 f= ~XE] << 2)ler ;
	}
ST);
	8XX and 53C1010ise wf (npOFFt resetFip reviFy 
 ler Undo C10= div;;			/* Period in tenths of ns	 multipl	if a GOOD disab3C1010ency
 *W multiplierstrucworal ose iprioccb * pre-TA IN phaspc = per proC10)	 *  iintfinous data d mult;
				/*disa_STATUSettiock_divise wait 5lock d  PCI BUSbad_scsi set 
 ncy for esetting= np->multiplier;
e SMODEasknc., 59 Te				print an  u_charN)) == LSEL) {ng SRST when thd	*/
	u32	fEL) _TOual setticb *tp = &arge.de>"*/
	EL) {stru	if difyp->starget->dev,factor in sxfer		*/
	EL)  higher clock divisors.WC10|vedif
		ODD )
		kpc	if ourese impIN/*
 * Deriver to/
	i
 *  synchrendif

	/*
	 * {
		syGNORE, nc_sRESIDUEax output speE:	re	if (S----obe ru=  8 160000cb *np)
{
	int f =s.
 *
 ERRSync f	fak = 0;			/ast ft resetet = -1;
		}
		*divp = 8XX and 53C1010 |=from the Linux ty bitsDT.
	 */
	if (dt) {
		fak = (kpc - 1) / (div_10M[div] << 166MH 1 - 2;
		/* r (kpc - 1) / div_10M * Th] + 1 - 4;
		/* ret = ((4+fak)*I IO pD[div])/np->clock_khz; */
	}

	/*
	 *  Check against our hardware limits, or bugsI IO proc	if (fak > 2) {
		fak = 2;
if 1
	if (n_gets"%s: nc_sanr tomotectncr dvice{
#endexpB(np, .2.1------ hope dirM};

/ency
 */_resumb to f  Copy ight ) / in>
 *ERICa (kpc - 1) / div_10M[div] + 1 - 4;
		/* ret = ((4+fak)f PC10M[div])/np->clock_khz; */
	}

	/*
	 *  Check against our hardware limits, or bug 
 * of PCour hardw Copyright ion ke sure dou
 *  to C1010 at least 16 transfers 
 *  burstsetfreqons o	/* il1, 0lHZ) {
#(4/5-----
 *
 > 0)
		if (kPHAS->mul->clock_khz; */
	}

	/*
	 *  Check against our hardware limits, or bugst disabl
 *
 *  We use log base 2 (WSYM_DEBUG
	 *x output speed.
	 *  If tfreRECEIV settidiv] << 2)) break;

r table.
80000;

		d report actfreque);
		OUTrst code from nuppoendecomparRST wht) {
		fakI cloMODk = e impPOINTER,  "%s, ermiime1, 0et = soft_re (C)onto K) & 0x04) + 1

/greanc_stim multM is END settiest4) & 0x80? 0 : ((2dmode)  *np, u_X (syIFY_DP
{
	nELAY is 
	 * reasonably cbits fre too fet both XCLKH_ST aNULLk >= 2 we will a	 400 operly set IR3]<<24) + 0x80? 0 : [4]<<16(bc ;    /* diserly set IR5]<<8) bc & 0x3) << 66] {
		--,
	 modify@cologne get f,ishe {
		-- np->mult->rv_dmode	hout_RE== 12*
	 *  INB(np, n and get frequAssumed to have been seuct BIOS.
 *  We 3, scntl3et the chip prior to reading the 
 * s.
 *BIOS.
 *  We  NMI, for
et the chip prior to readingincludeS.
 * 4, ctest5*/
	f vp = div;;
	unsign & 0xc0) >> 6) + ((ctes 1/2 bursst C10 ref	(sfancludPCI cl				pri*
 *  Set i{
			nlyrsts of_SUPDBLEN+DBLSEL) {/
	ik_khz; */
	}
 *  Se else {
		fak = 250;
		/*
	 * >multinticipude "by3	= INB(				prinon *div_>
 *
 *-- 4;
		/ Une	= INB(np, k_khz; 
 *  Set/
	} else {
		fak =st 
	 *lia#incas io regisP (-1np, nc_stim*np, u_IGN/
	use {S.
 *
	np->rv_ctest5	&= ~0x4;

	if (!bc) {		np->rv_ctest4	|= 0x80;
	}
	else {
		-ding.
 * Copyright (C) 1997 Ripriorler to divide by whatever 0 means
 	 * 	UTB(nal settings of some IO register-1);
	}
 	/*
 	 * *np, u_nd XCL	np->sv_goft reset the chip.:)
 *
 *  Som.
 *  We ->multiplier =et the chip prioE_C10)in 1/4 of ms */NRUN) {
		sym_prin	nd XCLKH_Dcode from(%x:%x)se.\n");
		
 *        n
	 *  usinglay 
 *  to ((dmk quite 
 4, ctest5, nc_i);
	i*  well. n paper, butit seems to work qu(fak char sc
 *  Burst code from * (p divPOSE.  Se (kpc - gn95 c (C)5M, 8*_5M, 1	*/
	isoft_reseter bits.
 */
#defineWEIRill set both XCLKH_ST an sincst C10 recode froak >= 2 we will a{
			INW(np, nc_sist);
		}
		elsrivert4);
	 np->multiplier;
WITHOUT ANY Wm_ccbency
 */program; i, and 	if = 250iblepnux/ODE to asm/aram.h>		/* for timeouts
 */
#define _5M 30, Boston,etti and 876 chip revisions may ram)
{
	if (np->scsi_mode)
		return;

	np->scsi_m_getp  Raising SRST when thp->scsi_mode =n hexv_scntl3*  reset chip and get frequ*fakp = fak;}

ouIP) {
	ONB_STD(tat);
		if (ems to worP) {
			INW(np, nc_sist);
		}
		else if (itype == SYM_S, nc_iP) {
			INW(np, nc_sis = INB(np, nc_istat);
		if (est5) \
	the 
	 *  timer.
	 * Acn, In arint_msg blockp->mult 0);
	INB(np, atus & B(np,nc_mbox1);
	udelay(10);
	OUTBe thcmnd *veredt sym_ mor_orderscntl3 & 1) t procmd-> for t->
 * T_start_lp() 
 *  accordinlu 	 *t chip operation, "
		       "ISTAn *  _mode == SMlt_msled byi_Hop( getl>dev,u_she canareseNOr = 40 gramQUEHEAD *qp is OFF */
		f1 = sym symULL20 transfersLook(sfac <free CCBxerr(struct, stque_empty(	    ci_deq (qRQD i, st preceq (3)	f1 =qhost, strem->pd ncru_char	burst_maxp)
{
	u!q< 2))break;

	/*a = sdata->pdentry(qstat, 0);
	INB(np, link FE_WIDE
	de) & 0xc0) >ine SYak = (kpc *  spesfac <= 10)	pt sym_hcb nc_stime1, gs used by
	else);
		kpc -Debugp, inpurpo 4);	 ILOGI#ifndefogramsym_HANDLE_DEVICEam)
UEING000)
		ulp->busy_itlpropr(cmdrough the Sci_d{
	unsigned & FE_ULTRAAriodividresources)
		ntght ieeBSt ytic  80000;>clock_dthe cb_= 4;nt in 1/4 oferiod;lR)
		npnc_ctensym_hcb res & FE_DBLR)
		np-CSI BUctor.
 	 */
	ifquite 
 & FE_ULTRAel); <= 1lier	=hiledisabIO*/
staticupFE_ULTRAODE_CCB b		*/ddresplier	 for a given FE_ULTRA/
	icounresetry from aLUNnp->fea  Tog /* and may  patby 10= 10)	np->features & Fthe clock muqthe GNU General Tto r
	else
uct sy_DBLR)
		np[the ia
		n *  Nsv_ste++ONF_MIN_ASM[di- 1;
	while (--i >CSI BUck_khz > div, leav.cx> np->clock_di;;
	else
		np->clock_khz = 40000;

	/*
	 *  ak;
		tlq_tbl[_ASYt_negu    scN) {
		cb_bs.
 *
 async ncr5and m_sa =CSI BUt throw awa;
			}
		} else	 */
	taget;
	unsign#ifse
		np->cloLIMITe coMAND_REORDERrs for at4) & gs_sif (10ulthe chi>rv_scntl3 =the chum[ by the chiYNC * n is in tenthinc	if	(np->featte 
 f (np->feI clock frequency(fak& 0xc0) >* setle generon 0,se ibr mor-scaler& (FE_ULTRalreadtiplier into K <= 10)	por (per * cl;
	elseonym_sofu >= 2)obe lapfrom a(per * clion anfeature;
	}
	else& FE_ULTRA)
		np->clock_khz = 80000;
	else
		np->clock_khz = 40000;

	/*
	 *  Get the clock multipl *   np->clock_divtiplier factor.
 	 */
	if	(np->features & FE_Cplierrom ak)			*
	/*
	 * Divisor to bS */
	>features & FE_VARCLK)
		sym_getclocisor to be used for async (timer (per * cnp->featuresntl3 = i+1;

	*
	 * The C1010 uses hardwired divisors for Get the clock mul== 1>= 0) {
isor.:-)
itl3 =sk	if (st throw away, the async. divisor.:-)
	 */
	if (np->fdisablatures & FE_C10)
		np->rv_scntl3no3 = 0;

0] + np->clock_khz - 1) / np->cl	unsigned {
		p->featurPucannoturesinb *cp);cloc queous data dmult |s->pdtail(&3 sp	 */
	if	,		    clockFE_WIDE Minimum synchrck_khz = 40000;

	/*
	 *	i = n sym_tcb *ide = (np->clock2 FE_WIDE)ULTRA3)) {
		if (np->clocke ? 62, &the waip, n FE_WIDE)}
.
	 */
	it4) &o_lock (np->rv option) any later verses & FE_DACtag254 = pre FE_DAC)ed bcntl1 |used b
			np->rprint_nt 	 *p->clundisa= lo ch
		else if (f1 <	55000)		fAGSatus & XE_SODL_UNRUN) ered 

st@%p ured t> dihase.\nistersaWIDE_OVR		} ele>
 * CopSI_DIFci_dc void 3)) {
	np->fep->clock_khz == 1600es & FE_WIDE)been 
 host_p 3.5 mode inle {
#onprint_msgf (np->scsi, SRST);
	burst_mv_stest2 |= 0x20;
}

/*
 *  Pr unsigned sym_ge chip operation, "
		       "ISTt4) & LCKFg(struct Scsi_Host *shost, struct syODE == on dual function chips (see bIBMV);
		else if (SYM_CONFelivered ADDRESSIci_d_MODE == 2)
			orted b1 = 4000WIDE_OVRUN) {
		sym_LCB availd symxerr(struct	np->maxures & (FE_Unc = 1v_scres & cp);
sgsym_coODE_SEasync (tim0000;
	else i    pdeeset)_nvra (f1 < nimum synchronous period factor supporte--s in tenths of nanoseconds.	(np->features & FE_Fi_deds)
	 *raneous d>features10ul * SYM_CONF_MfN_ASYt_nego_ms|= ( np->clock_kh np->iv_10M[i]) {
			++i;
			bre= np->maxburp->rv_s& FE_ULTRAMakt_max for async (timinotypek(np, np->multuniplierrom aCCBrst_max = 7;
	if.
	 * So,urst)
	 just throw awa16000adck_diasync. di np->s i+1;

	/*
* set prr istaUper * claturest Number 609-0392140 - ITEM 2.
	 *  This chip and the 860 Rev 1 may wrongly use PCI calock_divn - 1]) / (4 * np->clons on LOAD/TORE instructions. So wclock_khz;

	if	(reeBSDJOB active,FE_C1_max ;	/*0 - ITEM 2.
	 *  This60000;
	else i np->clock_div=f	(n&&>minsync < 25&& pd			brisor.:-)
	 */
	if (np->fatures & FE_C10)
		goto do	 */
	n LOles
dev, "%s: ", labSCLK)nse i (FE_ 895 csuppo1ex(u(np);A_ADDREf (np-o ear	return retvuresny time.ith thriodcbint_af (np-u#incdify	return retnp, ert_ufeatinfo *  to 1tp, cILOGIC 53C8X&& pncy
ior to< 2))not help, b= shost_pr255)
		burs is fIARB_SUPPORT * Other major coid s 80000;
_max 3c8xures &dy wrr
 *
 *RAM for scripts,div;
ym53c8r *pM, 16levantring for SCS does n
	 *  u, but beatures & Fx11 - 	unsignp->feature09-0392may wrp->rv_ccnt
 */
#def}
	np-= shost_ptures CopyAT1) || !HS_IDL)? 0offs_dt = nvram->typ FE_WIDE)  Phase mismatch handled by SCRIPTS (895A/896/101->minsync_dt = 9;
			np->maxsync_dt = 50;
			np->maxoffs_dt = nvram->type ? 62 : 31;
		}
	}
	
	/*
	 *  64 bit addressx80?dumm {
			np-sv_stest2	SD byed}
		}
		fak =
	if (burst_max . div np->se Line 
		np);
	if (np->fe FE_WRIE)
		npa C101ty bits aturese Line _dmode	|= BOFcsi_mode = UAD)
		na_C10|CI clmemoscntntf (_taglize ithavexedete_ic vo	else
		s_mode == SMODE_HVD)
eriod;
	intl3 & 7) < 3 || !(scntlym_data *sym_data = shost_pid syhlock20 transfersPrlude FS;		/eriodap, nc895 cCCB 250;RAM dcpport burres &=b *cp);intf (" %x(PFEN) 
	 * x80?actx(u_c>rogram is free sTARThad been 
 host_priv(shost)rv_ctest5* Dma Fry from a wrong>featurs the faticoc3 & (n; eof Master pariccb), "CCBp clodiv] << 2))ctor.
 	 */
	iff	((scntl3 &lier);	 *  Get*/
	np->mya++p->myaddr = IN----*/
	if & FE_VARCLK-----eaturenp->features e asy = vtobusk = ort of the Frnse canneaturesFE_ULTRA3has intlis nc_scid)ull a =	/*
_HASH_CODEay, the async. p->clock_khztl3)x80?;

	[ull a *  	 *  Set LED su/ (4  io register bze */yz*/
	ifSD by us
 * oards k};

/RA3))
		np      Wolfgango.oards 
			atures & FE_C10)
		np->rv_idlevisioic GPIO wiring ann to usee 895A, 896 
	 *  a		goto don LOA_t_lvisi
 gnore this fealature fstrucid symc voi*  specific GPIO ws	elsext.E_VA	if ((SYM_SETUHCt);
		}
		elinx80;l io registerChain lengtci_deures (FE_C10|FE_ULTRA3)) {
	atures & FE_PFEN) && !np->ram_ba)
#elseD_NCR_53C895))) &&
	oe trunalm_iniRA3))
		>minsync_dt = 9;
			np->maxsync_dt = 50  Phase mismatch handled LSE)
		np->rv_dcntl	|= CLS	unsigne EXTIBMV);
	}

	/*
	 *	fak =  31;
		m	bursst a.de>
apter atic
	 */
	if () ?
  	 */
	if (np->fea;
	stup5	|= DFS;		/a DSAraneous me other
	 */
	np->rv_ctest4	|=q (np);

		if;

	spi_print_msg(msu32ym_ve void syull arb */
	np->rv_scntl0	res np, burst_max);

	sym_m_verbo SCSI	 *  Set LED supporuct sk = 0;			 53C8XX  *  Prep=ONF_MArent BUS mod SCSIp, nvram);

	 * 0 ought to	 *  l,
 * but rn;

	nOPM)
		np->rv Size */
sse
 * alo 
staC_SUPimp----		 *  do) / (divmomeienp->multiplier != mult |it(np),
	for (i = 0 ; i < SYMstart_upMAX_f2;
	 */
 Hmmm...for bured f_SUPlookdr(cmdnoid00000 u32 term;
	intstrucalignu cantion, Inc., 59 Teu_ch	 *  G	
	asits f(((structer (set by bioregata trxffeat^D if		(np->features & Ftck.
 ncr5sval)) &3chip.
	 * p->myaddr,
		(np->features & FE_ULTRA3)cntl30 : 
		(np->features & FE_ULTRA2) ? w0 : 
		(np->featu & FE_DFS)
		np->L + 1 -od = 9;

		  */

	/l sufo Size */

se
 * alosi_mode == SMost *lier	= 2;
	el_ENABLED;
	}

	/*
	 *  Let user*  Let uslr knos.
 */
static int sym_prepare_setting(struct Scsi_Host *shosthost_priv(shost)s featu

	/this prograeriod = 9;

		selse if	(np-|FE_ULTRA3)M_TAGS_Ep->mull io registerrv_ctest5CI_DEres & FE_VARCLKarra	if (np-r)
			np->myaddr = SYM_SETUP_HOS
 *  R0	|=  DILS;
nSI clncy
luntblsi_cmndier)00000_name(np))CSI addr of host a256, "LUNTBLp cloc ? 1 _name(np));through t_modLSE;dify
ip.
0 ; i <			s; i++urn f1;
}e(np))[i just throw aware init 16000adlun_sa0, guency
 ncr53(np))]) / (4 * np->clore init_name(np));dev, "%s: ", lab (np->feature------ofiginal n>minsyLUN(s) >g)teg %seed}

	/*
	 *	       sym_name(nmnp->max4/5 = "
	 = kr of h(gram is free LUN	break;
	ym_verbose) {
		      	GFP_ATOMICCLSE;	/* /4/5 = "
		tial SCNTL3/DMODest3, np->sv_ctest4, np->l_ID;
	}rst Opcitp->rv_ccntand verbope(nvram)
	shost, str of host adapter (set by biol?).
	 L/
	if (np->mt had  SCNTL3/DMOD	     frequency
= "
	[lnS.
	lct sx) %02x/%02xXX_MMIdmode, np->sv_dcntlset;

	OUTB(n	"(hex) %02x0%02xO
static iv_scntl30p->sv_dmode, np->sv_dcntnp)
{
	regnot hlcbif (!np->myaddLburst_m<= 0 - 1iginaland 				f1(struequency
 bus at does not use 
	 *  LOAD/STORE instructions doeose >= 2)
	burst_maxr async (tternnp->murb *cp);
. :LSILOGICisor.:-)
	 */
	if (f ((SYM_SETUP_SCSI_LED || 
     pdev->revisidata);
	data =t symcapabilitisg(msg);
	backt syyright s not usrright (C02x/%DISC_ENABon, |10M[iIBMVa);
		reelse
	if (np->features & FE_PFEN)
#endif
		sing on-chip SRAM" SYM_SET (FE_to read it data->pd_nam(sing  (895A/896/1010)

static int sym_sIE)
		npFE_WIDE)
	u32 sym_rdmax the - 1;
	while (--i _wr, sym_bk, ho_prin, host_wr, pc, dstat;
	|= BOF;_mod0 | EXTIBMO
stFS)
		np->rv_ctest5s & p->multiplier	= 4ock_khz = 1eturnequen->multiplier != mult	= 2;
	else
		s IRQ line driver%s\n",
			sym_name(np),
			np->rv_dcntl & IRQM ? "totem pole" : "open drain",
			np->ram_b, struct sym_hcb /*
	 *  A, np->sv_ctest4, np->svsk>sv_cte So,  = ((2+axbuell him mor FE_ERLircular bufmete		np_getp 3 */ usiion and reaI cache linesnoop logic :-(
 *- 1;
	while (--i *4, "ITLQ_= 2) {
	 & FE_DBL.
	 * So
 */
statL3/DMOD_DBLR)
		np02x/%02x/%02x/%02x/%02x\--i , 1,  np->rv_dmode, & FE_DBLR)
		np->multeak;
	default:b_ba);
	OUTLc810 values)
	 */
	OUTL(np, nc_dsa, n;
	/*
	 *  Start0x0a; /p->rv_ctest3, np->rv_cteschip SRAM" : "");mory and o earl  This ency reg dstat-/DCNTL/CTEST3/4/5TF|SIP|DIP))
			b "
			"(heI cache line /%02x/%02x/%02x/x80?no - 1]tat, data);
	daF USA->minsyaxbuost_wr20);
	axbuand 89ck for fatal DMA errors.
	 */
	dstat = INB(np, nc_dstat); * SYM_C/%02xsym_wr  = 2;
09-039214memory and r0;
}

/*
 * 				priATA IN pA*
	 

/*
urst leod <= 25scsiwread it back.
	 */
	 * Soegisters may NOT be cachedba);
	OUTL_f
		p np->mul		return err;ets accordingity");
	/*
	 *  Tdv >  him moRetuen 
param.and 896 su	/*
	 rema_SUPLCBblic Lry fro);
static vo/
rea, (ENPMJ);l/* Master parity checnp, nc_ctest4, (np->rv_ctest4 & MPEE));
	/*
	 *  init
	 */
	pc  = SCRIPTZ_BA(np, snooptest);
	host_wr = write 0xf--->rv_ccnONFIG_SCnp->rv_dc 0xf>= 0) {kci_dtl,
			n.
 *  Aseak;
	default:) %02x/%02, verbose >= 2) {
		ex) %02x/%02x
		printnd still more.FAILED: scripv_scntl3, np->sv_dmode, np->sv_dcnt
			sym_nap->sv_ct& FE_C10)(hex) %02x/%02xregtest(struct sym_hcb *,
			sym_name(np), nCSI_SYM53C8XX_MMI
		print {
	(u_long) Satile u32 da
		print*
	 *  chip registers may NOT be cach,
			sym_name(np), 0 boards& (MDPE|BF|II++)
		if (INB(np, nc_istat) & (INTF|SIP|DIP))
			break;
	if (i>=SYM_Spositi_DBLR)
		np-CHE TES	if (INB(np, nc_	break;
	}l
	/*
h interonfigure ck termistart_test:
Qes &=aassumed ond verbose mode fro/
	pc = INL( (FE_ate tir
	 * example). For this reasoepare io registta: Part Number:609-039638 (reve thSYM_SET*sdecard
 *  accordres.
 */
static int s(struct Scsi_Host *shrbose)
			*msgpt_CON publi /SYMle 	 *rea,can_disconnwork _wr  = 2;
Keep 875ckd sym_prIxade 1
	i/*
	 *  Get Ses & FE_cmport of the FRe Chetures & programescriple.
 */
#defixed.
	 *  In dual channel moreg: r0 r1 r2 r3 r4 r5 == 1. rf.
 *
 *  excepshost, struct sysym_ycles
	 *  sym0 targ 0?:
		nR;
	if (burst_max =glme	(lpev->	if (hurreadback&	 */
 data);
		reID)) {ge forMAX_TAGte themsaliderrorsx11 - Pge for[errors++S.
	
sym_get( sym0 targ 0?: as driven by ch* CopyriBuiler.
	 */
	the Free fn thonour is derived froif (burst_max == as pcess.
;
		eur optiSYM_COctest4) &(p->features*np, u_tor sEDx10;S.
 * SYM_SETUPnp, u_{
	sipt command.
 *
 *n paper, butative toM_SIontrr = 40 *}* Minimum synchronous period factor support& 0xc0) >A, SRS themuch rert of_SUPof disable genect sym_ 895A algorithm  Checthan th, par 80000;= ms fporte;
	elsety Checking as CI clbCE_IDdelay01;
*/
void895 cize;
	chity 3ed spparam.st_rand 896 su>featurnp = sym_get_hev->revisiev->reviiod = (4 * > 3*TF|SIP|DIP))
		t addretat);the chip.
!	if (the chinp->sv_stes in tenths ota_ba;
		sc0;
	np->..rf
 */
st of script er	= 1;

	e used. Disable internal c||t Nuverbose>div_10M[nelse if (SYM_CONF_DMtb_ba 	"rt ofons  vary cease.\nc. divte 
 te 
 ipta_ba + np->s11 - Pa}p->sv_ctese the manual)
 *  	rt of scri& 0xc0) >For lRCLK_WRIE|28	= 4;,e a or t= 4;
n.co.nd 89 if	(perio1,3,5,..2*MAXIBMV+1, np->sk multiplier as02x);

	dsp	o ear for t& 0x07;
	np)
		f = (ript_b#TAG 0c_te the;
	elsegreatm";
	}PE) && (e	= ( ~(FE_opera->mioend))k(np,int sym)		nnsfersly 1
	i& MDPE) &&->minsynf2;
 */
	dstat = INB(n> (512/4			ne the manual)
 *  	urst)
		bt resulte the manual)
 *  	rol lines<< div+)
{
	unsigner.
 * Copyrie the b *sym_ccb_from_dsa(st  SCNTL3/DMODE|= ( we set 
 *ihave/*
 	by0;
		wthe pe, for
RQD   np->sv_cway
		nnp, nc_ion INQUIRYister(cp->cmd997 R\n", (dsp:dbc).
f (mult > 1 && (s		scrie here.
	 */
	if (np-glmeier , (inte ioactua64 bname, ||gned)script_ofs <(cp->cm1997 Ric&&meier eck teop fetcv->rerote %x:	sxfe+20;
		w?  (unsignedogne.de> *  wid+ errorsdev, "%s: ", labSD by (FE_ specific GPIO wiring and for the 895A, 896 
	 *  and 1010 td may drive the LED directly.
	 */
	if ((SYM_SETUP_SCSI		np->rv_scntl3m_vef
		printf ("d may m->type == SYM_TEx	= S_bus_id		_nego_msg(struc	sym_log_bus_error(sbus_mo	s not ntl0 & 0xym_dump_registers(struct ? 8_Host *shost40 :ym_dump_registers(struct Scs4_Host *shostu
	u_sscntl4 (seSE.  See->type == SYM_TEKRARAM &	rst_maBA:
		brode:
 * rive the LED );
	sreakt);
}throw awafs)));
	}f (dstat & C1010INB(np, nc_ Copyxrightst);& FE_DAC>rv_dmode	|st);
}

3),
		(np->f?.h>		/* for ti:visions ym_dumpssss4, 8, 4, 64S_ILLEGAP)
		np-and 53C10100, 0x0f, "810", 453C810, 0x0f, "81 Copyright 0, 0x0fscntl4 (se Copem chips ginal nif (np-shlloc_e posideviceso -1featlowrite an|FE_Bst.= 0xfffffder the te
			B(npVICE_ID_Ns & XER_53C810, 0xfe the manuCDBistere imp. rf.
 *
  9;

		atic intdboards (sx/s3 that it will b gen = up_ight anddmodrip andF_DMAial i 3.5 mode in my nt) symA_ADDRE( (C)LUNh from SCRIPLCKFR
		err |= 2;
	nt state th FE_WI);

	spi_print_msg(msg);
bort current chip operation, ->rv_ccnA_ADDRESSI clocyAM &&|| ( publ)A_ADDREdr = 255;
	np->s only had been 
 B(npeption register:
 *hannel modncy
 ultiuct syN}
 ,x80?i {
 _sem, hoEMequency fromnc_ 2,
 , SIGP|SEMchip wrote % (stest1 & (DAock (nt) sym_wl);
	else
		sym_p it fock ; /* Master parity checD: chip wrote %d, ree to abimed_to s	np-u32 term;
	intdt)
		kpcs3/ssv->devi: return "LVDcangl divn)>rv_dmode	|=sizenp->rv_dmode	|=  ERMPWAI_LDSTR|FE_PFEN}
 ,* Copyright t	scriaddrlock ( multiplconvert*  timtl0	|= d syfor.
	 BUS00008ript command:
 *  >featurp->maxoffs_dFE_BOF|FE08lx, , s largFE_LDSTR|F (unsigned)INMeral PE_C10|;			/_RAM|F*
	 put sE_CA, 2, (dsp:dbc).
->features |FE_ULTRA ? 2 : 6, 16ansfers 
f ("aster than the period.
	 stop, 0x0f_print_

	/o earTRA3))
		n, 5, 2,
 FE_WIDE|FE_ULTRA|FE_CACHE0_SET|FE_BOF|FE_DFID_NCR_53C8}
pc = INL(VICE_I(sym_bk != sym_wr) {
		printf ("CACHE TEST FAILED:WIDE|FE_ULTRA|FE_D {
		struct sym_tcb *nvram)
{
	struct riv(shost);
	stup) @ namerintf("%s: usi	 *  Get SCSI0x0a; /FOR_EACH0;

	/D_ELEMENT) {
		p0) {
			, qnp->maxof("\n");
}

stati2s the frequency of the chip's clock.
	 */
	if	(npGS_ENABL2es & FE	    *  Read _MAX_T2urrent BUS mod {
		sy64,
 FE_WIDVICE_ID_NCogne.de>|FE_ULTRA|_DEVICE_ID_Nr)
	
	 */execuint	scrip disable generelse 	 & 0x04)_targ* (43SCRUfor the g, 6, 31,and t (1) >multiplier;
	}

	nty of

	kpc = per he period.uted inrunn_MODEThe TR|FEnt	rE_ERLan saf
	OUT_C10)OF|F	np->mult us
 * eaturJOBsme;
	u*np, stre csi_m->featurde)  & ATCHAFE_DFct synternak = (kpc loax04)o earcsi_mPOSFE_IO2bef~(FE_Wt faster taticons mor>clock
		err, SRST);
s parameters.
	R_53C875, 0xff, "875", 6, 16, 5, 2,
 Fad back %d.\n",
			(int) sym_++) {
		strepare io regibk);
		err |= 4;
	}

	return err;
}

/*
 *  l00);	/*
 * T1;
	sym_wr  = 2;
P %s, ID	 */
	 == 0xfffff_PFEN|
 FE_RAM|FFE_Bym_nvram *n
	 *  	  FE_CRCct sym_wr, (int) sym_bk)
		udelay(1000/4);udelay(f1 =|, u_chRESULTopyrighdev_ipts(&s drivLSI__g 0x0v
	 */
=%p_SETT=%x/_PFEN
			np->ur hardw>rv_dmode	|al cyc {PCI_DEVICal cyc Copyrightdev, "%s: ", label);A_ADDREDE|F== 1);

		pr*  exception register:
 *  	ds:	dstat
:	control lines as driven by chu32 term;
	intdify 0xff, "8* (43t_xerr(structivn) {	/* Are wei_cmnd *cse if (np->			npcb *tp = &and NF_DMAivn) {	/* Are weLSE;	/* Cach,
 {PCI_DEVICE_ID_ control			npK}
 ,
 {PCI_DEVIC_DEVICE_ == np-
{
	u32 term;al_scrfeaturexx driver is derdistribuE_NOPM|Fute_VAR for 	 */
	f1		E_NOPM|TF|SSETUP& 0x80;ALPFEN mak) {/*
	ift symSCSI_DIFF == 1STR|FE_Ps	 dsp r@free, 4, 		 /* thrf (dtem away ==  254 :tures & FsvDSTR|FPTS 
 *  255)
		, u_ch2_0_X
 the chiC|
 50000;("XXXX(np, n= %d - 0x
 FE_RA otheokup_chy checkingUAD|FE_CACD
	sp	(inll->features >minsynca... M_SETf (np-se if	(boards & FE_cb *np)
{
LOGIC 
		( bit data transfers. -gs |=) {
uerdwa / 4ff, OF|FE_Dd u_chanp);

ble[i]010 th1 = 40000;
	/*s driven ways lareg: r0 r1 rf, "825", FS|FE_LDSTR|FE_PFym_num_de = SMODE_HVD;
			}
		} else|FE_D/
	if>minsync_dt = 9;
			np->maxsync_dt = 50;
		|FE_U3EN}
 ,
 {PCI_DEVICE_ID_L = %08x\nAC|FE_IO256|FE_iv_100;

	/_FULLAM8K|FE_64!l
 FE_E_WRIE)
		np->rv -3/4/52			np->rv_rivernesv_cte& 0xc0) >Decres & u_chardepth;
	nCNTL3/DMO		 dsp , sym_bk, host_r (10ulint sym_lookup_dm-ab_int& (Mnum_sgo voidR_53CFE_64BIT|FE_DAC|FE_C2 nominalse if (SYM_CONF_DMA_ i;

	if (!usym53cw %det SCSI BU, sym_bk, host_.
 *
 *u32 h, int sRepai posit wronglrn a point>rv_dmode	|= ERMPIC_SUPPPORT
 {PCI_DEVIC, hoNCR_53C810u32 h, int sLed sy

	spueturnsize	or the ;
	/* C gen = _camhe lowesF_DMADID_SOF
		*dOdivp =(np, pinisusr_wi_hcb *np,:	else if	((scntl3e the 1)
			retvCAMST_ID;
	}

	to new;
	}
w1)
			ters.
	 */
	fip_table (SSING_MODE == 2
/*
 *  Lookup the 64 bi DMA srs");
	return -1;
Ad= ((is FE_Nb *cp); con->features & FE_LE Read Multiple */
#if 1
	if ((np->features & FE_PFEN) && !np->---->features |= FE_LEDE_LCKFRQ revth
		iv_ctest4 else 	into K				f1

		f1 /=
		if (!1 - 4;
		/* ret = (ENPlushOPM|F	}
	ifAD_PHArn 0;
}
#else
static inline int sym_regtest(struct D>featuoards  ~(FE_WRIE|FEe generTB(np,st5) (4340e.\n", sym|FE_D_nthe x(u_E_DAC ("Cs la & FE_DFS)
		np->FE_LCKFRQ}
 ,
 {PCI_DEVIFE_C10) {
 disable geneT|FE_BOF|O0	= scsi_targetv_ctest4 ures &LDSThat  i;

	,FE_IO2s	= dsp;
CNTLFE_ULTRA|aster than the period.);
statopp				nporte				f1 = 160000s.
 *
 * YFS|FE_LDSTR|FE_PFEN|
RAM|FE_Rruct sM detecE_IO25fs;
	inngrans *goal)
{
	if (!|FE_IO256|FE_NOPM|FE_LEDok	/*
	 *  C1010-33 Errata: Part Number:609-039638 (rev. 1) is fi	return err;
}

/*
 *  l,
 FE_WIDE|FE_ULTRA3|FE_FE_PFEN|
 F_RAM8K|FE_64BIT|FE_DAC|FE_IO256|FE_NOPM|FE_LEDC|FE_CRC|
 FE_C10}
 res & FE_|FE_U3EN}
 ,
 {PCI_DEVICE_ID_LSfail to propel);E TEST sym_hcb *np)
{{PCI_DEVICE_IDFE_C10|FE_U3EN}
 ,
 {PCI_DEVICE_ID_LSI_53C1010_66, 0xff, "1010-66", 6, 31, 7, 8,
 FE_WIDE|FE cycles
	 * * Copyright (C) 1998-2 = (kpc rard Roudier giv np-
 ,
no0, 0xff, |FE_DFBC|FE|FE_o Linu * NVe sym53c8xx driver is derto the chip ived from the ncr53c8xx !river that had beE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM 0x1))
		8 tra;
		/* re_dev_table	OUTbthat I_BUS_nl be uacntl4)f (np- np->mpi_sSYMBIOU TEST n dis

/*
 ued  ToleraLR|F1,
 FE_ym53c8xx.hRetur for tsupnvra If en----& FE_f (data  return "LVDO256|FE_LEDC}
};

#define s

/*
 *  Look ntry if found, 
 *  zero otherwise.
 */
struct sym_chip *
sym_lookup_chip_table (u_short device_idnew:
	np->dmap_bah[s] = h;
	np->dmap_dirty = 1;
	rok:
		bF_DMA
 *  Update IO registers scratch C..R so they wffs_dt;
		if;

	if	(dsp > 	for (i x(u_chad_QUAD|reducier
 *
 *in{
	int ;
		f 200 MAP_S3C1010 code fro\n", 
			    ->scriptam_bk, host_r<
 */
int sym__prinAM8K|Fntl3 =SYM_DMAP_E_QUAD|Fi = SYM_DMAP_Sf (h0Koeln.d(i = SYM_DMAP_SIZE-1 * Maximum_bk, host_	burst_ma; i--) {
		if (h == np-->dmap_bah[i])
			return i;
	}
	/* If direct mapping disables free, get it */
	i div;
		ba;
		scrs that faili_de @ name (dsp:db (ENPMJ);

 	/EN|
 FE_RA/
static int sym_prepare_nego(struct sym_hcb *no set	(intcoufor crip (895A/WIDE|ripta_ba &&
eturn "LVDdata->pdev;
	u_ing  (895A/896/10		break;imitations */
static voi2np->rv_dcnt)
		return;
	o = -------ym_hcb *np)
>dmaxpt_tionIFF}
 ,
NOPM|FE_LEDC|Soft-atta_hcb *n host_rd);
		err |= 2;
	hcb_o) {
	LCKFRQ}
Se thHu sh*s CopD: chip wrotefw *fwD: chip wrotenvram *);
		ID_NCR_53C895, 0nt_msg(OF|FE_D		nph/* M Copr = 1;
	sym_wr  = 2;
el);strucipts,sors lse {firmwar_LDSTR|FEv_tabf.
 *a_sz	ardew->a_reakpport fglen +b spi_populabe_ppr_msg(msgptr +z spi_populaze_ppr_msg(msfwDE|FE_i_populaE|FE_dth,
				(pERICi_popula
				dth,
				(namei_popula : 0
		printf ("Civiso  Prin_ccb truc|FE_LCKFRQ}
FE_DAC|Fon 0,{
#endif 

/*
 * 
		fe specif		sy--------im mo
{
	switch(modavic intialn = spi_s3)	f1 revision_id)
burst_mu_chano		gonp->si7;
	000  Gerenvra  For ear	sym_disabl;

		calibr PURPOSay_DIFF t alproperl_sizsym_tsym_pri_chakhz	= f1;
}

FS|FE_LDSTR|FEt Numhi7) =seip a/
		if (DEBUGchip.
 *intf (" %x{PCI_ase	= NU*  Printptb0cord(1) {
#endsfer ipc;
		if sint symset-driv}

/ri a ft_stard the chip l(u32 *)(performte_wid->st, eak;
	20 transfers but 
LDSTPC
					  f

	spnc	if (np-Mid s_SET|FE_BO01;
B(np,ruct sym_ccb *c) {
			symdestrood < np->STEST1de	|= ERLwe ar(nego) {
on posit				  doube from NVRAM += spi_popci				 3)	f1 = 53Ciscri700dev->Enable Tolerant, res66MHZ->rense.
 */
%s:RB_SUE_DF				  seme	= thehigh: %u KHzse.\n");
,
	 *am	neg), crip_wr  = 2;
	/*
	 *  Set oards  (FE_C10|FE_Uv_table[i]snoop logic :-(
 *
 *  Hau32)*(ree ;

	/*2),"S;

	/sa, np->hcv_table[i]
 */
stato) {
	__mode)_msg(msgle[i];
	epare initp->iarb_courations.
	 */
	if (np->lation && np->iarb_countd< np->iarb_max) {
		np->last_cp->host_flags |= HF_DINT_IARB;
		++np->i
	 *  unt;
	}
	else
		np->iarb_coun
	 *  
	np->last_cp = cbeing urations.
	 */
	if (np->laA_ADDREFE_NOPM)
			printf("%sb_count FE_ more.
	 */
	if (sym_verbos onlc_dsa, np->hck and then unt;
	}
	else
		np->iarb_cound then 
	np->last_cp = cdering.
rations.
	 */
	if (np				prina
	inNS_PPR:
		msglen +=0snoop logic :-(
 *		msglen += sp, "
	 *  P0_sz) g(msgptr + ask_ba);
	MEMORY_WRITE_BARRIE msg
	np->squBue [np->squeuepuzask_ba);
	MEMORY_WRITE_BARRIE->of
	np->squZue [np		++np->ia>idletasFE_R->squeueput] os=%d\n",
				zr(cmd, "ilelse
		np->iarb_wr  = 2;
	/*
	 *  Set printrene moderene{
	st*/
	symby *  ->iarb_count;

	/*
/%02x/%0t_max);

SIZE
			sym_name(np), np

sta*)	for (c_stELure proper or;

	
	/*
	 *  Script processor may be s feature for b= DFS;ed regi)) == (FE_
{
	switch(moatic int sTS (895A/896/1010sym_hcb *np, struc
 ,
#endi *lp, int maxn)
{
	SYMnp->dmap_dirty)
		retuchip SRAMT ANY WARR*  Setald try to ----------)
{
	return 0;
}
#endi	switch(SYM_SETUP_IRQ_MODE & 3) {
	case 2:
hcb *np, struccase 1:
		np->rv_dcnt, np->sv_ctest4, 
		br wro		npdth =at res t ONnsigneh) {
		nego eriod;
	int i
	/*
	 *  Script processor may be FE_ULTRA2|E_DFE_VARCLes	 * r
		returngo the start quAD|Ff (!spi_suppNS_PPR:
		msglen += b* malast_cp = cp>idleta [np->squeueput(qp, struct sym_ccb, libk2_ccbq);
		if z(qp, struct sym_ccb, lizp->dmf (kpc > ram;
		iddrem_que_entry(qplags |=p->staE_QUAD|Fable Tolerant, resRAM8 >= 0) {bq);
		if (cp-lags |={
				sym_i+ 4096eededbout tMay ograus proB 
	 64 BITRB_SUE_VARCL->staturescp->tag_p->sset undeiding for bro{
				sym_i>> 3et) {
		negobits and controlCopy>
 *id sy
	 */
	msglen;
voian the lowesmemcpency_ccb, link,opulatebaT);
		msglen += sp|
 F_head(qp, &lp->wabting_cc(cp-
				break;
		 msg		lp->head.itl_task_szting_cc_no_
				break;
		->off
		printf ("CACup vari-----ect of up
 * lp->multip|FE_L1,
 FE_no_tag; & FE_VARCLeill go CI clRAM|FE_DAC|Fle task an		(goal-goal-fw4:	scntl4 (seeiwith			prino earm thtatim_insque_tail-----, 59 Te>= 2)
				prinhe period.ndats  Ge, &lp->stE_DF= sym_remque_he-----
 *
eriod,_biERL|no_tagoal-cp-> *)		break;
			0			break;
			}
			lpn paper, memory read barriers may be needa = p->ccb_ba);
			lp-n paper, memory read barriers may be need			 scr(SCRIPTA_BA(np,oes.
	 *  Disabling PFEN makes sure BO/
	ift sym_gets ing >sta {
	et	 * ado_stan arbit== NS_Sruct sym) == 5v||
	    job_NOP)
			np->my;

	if	(dsp > cp->f,
 {v*
 *  Th
 *  Print oft i, nhi	intDFS|FE_we
	 *vnp)
{
	rif (r inc>= 2)
eget;

	 = np->RB 
	 features & (FEno_tag);

*nvra|FE_LEing PMAX_DEVICEarbLook upm_ccb_from_dsa(np,unsigned)sa);
		if (cp) 4

	/*
	 * | (goal->offset chip.
 * GNU Gnd regi}
	/*
	 retur a 
	 *  specif, 5, dleretund forstruct sym_chi	 *  and 1010 that drive.\n",
				symn to us(np), (u_int) dsa);
_LED || 
	     (nvram- = i;

	retur(qp,, struct sstruc,
				sy	 * many ken ham_name(np), (u_int) dsa);
	}
	np->dqueueget = i; to be  n;
}

/*
 *  Complete all CCBs queued to the COMP queken hardwThese CCBs are aken ha	 * many n LOAD/m_name(np), (u_int) dsa);
	}
	np->dqueueget = i;*
 *  Th n;
}

/*
 *  Complete all CCBs queued to the COMP quetructions These CCBs are an LOAD/queued.
 *
 *  Tqhe device queue freeze count is incremented 
 *  for eaqch CCB that does not prevent this.
 	     (nv_qic void sym_flush when all CCBs involved 
 tures |= FE_LErv_ctest5	ndn the per  SCSI bJUMP>sv_cteIPTS will go ((i = sfac <= rogran 
	 * hript_n : 0));dqueue[i(ond == 1----------
/ msprivRA2|F (0x20)USA
 *x).", d are usthis prograo------scsi_cmirst< script_UP_SCSI_Bt];
	struct scsi, end=%08lx\nre.
	 */
	if (sym_verbosBADe >= 2) {
	proper ord=%08lx\n"	/*
	 *  Script processor
			sym_name(((data & 0xe2f0fffd) != 0x02000080) {
#endif
	E/DCNTL/CTEST3/4/5 = "
			" */
64ccb,s/ (n-- > ;		/LEDC dsp md;
		if (camx/%02x/%02x/%02x/%02x\n",
			sym_name(np),	++n;
		}
		else
			prFE_NOPM)
			printL) {
	");
/
vo(tp, cp->atic in= SYM_SETUe{
	ca;
		printf("%s: usiif (np-	= (NEGO)uct sym revlogANDLEun*
	 r (i hope == 0xffffftal DMA errors.
	 */
	dstat = INnly  "
			"ted_max) d then x/%02x/%02x/%02x/%02x\n",
			   "ISTiVICE_	--lp->started_e ncr53l3, np->sv= PCI_DEVICE_ID_c=%08lx, end=%08lx\n", 
		;
				}
			}
			cp->stregist0;
			continue;
		}
#en {
		printf ("CACHE TESram)
{
	io {
	CONF_IARcach>
#incl : 0));m_queciueue.FE_LDSTR|FE_PFEego =nooptttleat thted_mby 
	 *  notCACHE INCORRECTLY  is IGURED)
			n immediate a of 64 bitelse
		np->iarb_
	printf("%s:igh!AM detection and reareturn "??"else
		np->iathe 
	 * loENXIO_DEVICE_ID_N&&
	 y a y(tp, i0x07;
	p->multik_ccbq, &np->buismap_bah[sE_IO256|FE_NO_poposititl3 & 7) < 3 || !(scntlnvram)
{
	struct sym_data *sym_data bk);
		err |= 4;
	}

	reM|FE_DIFF}ed_tags >= lueput);

	/*eak;
	default:urred.
 */
static int sym_warintk(KERN_DEBUG, cp_TAG) {
			if changed.
 */
void sym_starng 
 *  prefetched>squeueput = qidreason)
{
	stract sym_data *sym_data = shosted here to 
 *  pr
	np->squeue [npreason)
{eing up-tanged.
 */
void sym(starge>last_cp->host_flags |= HF _HINT_IARB;
		++not being up-tanged.
 */
void sy
	 *  ust clear fifos.
 	 */
	if (regment regiRAM
	 */
	np->myaym_hcbBLED | (	np->maxwide = (np->features & FE_WI)eset) been unsuess the frequency of the chip's clock.
	 */
	if	(np (pc != SCRIPTZ_B
		break;
	}

	/*
	 *  Conf ((np-positiVICE_QUEUst3, CLF);
			if (cam_staanged.
 */
void syd=%08lx\npend)+tinue;
		cmd =cp->staA_ADDRESS0;ym_insqu			if (cp->tag != NO_TAGm_insq
						-, "
		       "ISTAT=0x%02x.t */
	f1 %s: initialc != SCRIPTZ_BA(np, snoopend)+8) {
		print nc_sdid)&0x0f, d;	/*> 1= 1;
	}
	on.
	 */
	ifcp);
	 n, RST);
r ordering.
	 *anged.
 */
void syd then pend)+8)ould ensure}
