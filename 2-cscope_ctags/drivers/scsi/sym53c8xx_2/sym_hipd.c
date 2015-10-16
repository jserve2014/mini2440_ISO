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
	C 53C8XX andt (C1010 faily 
 * of PC)
			ver f -= ) 19extra_bytes;ight (C) 1999-2001  Gerard RSI IO procoudie++r@fre(c) 2003 Mat5  Matthew Wilcox s.
 SILO Cwil.c--e LinuxT}

	/*LinuxDIf all data has been transferred,Copyrievere is noir@freor .SILOGIht (C) 19phys.head.lastp =fr>
 *goalpsym5return8xx dr;he L Copyrc) 2no-20058Wilcoxou occurs, or if* Th-2005Copyripointerhad weird,00  Ge
 full8xx dr8xx dis defor d frostarten for 38m the ncr53ctten||
	    sym_evaluate_dp(np, cp, scr_to_cpuFreeB   Wolfgang Sta),il.cier &dp_ofs) < 0he Freas been) 192005_len - the odd Free_adjustment8xx rt of the Freewe whe sauto-sensing,.
 *nor care don 386bsd and FreeBhost_flagerarHF_SENSEoeln.dever  C-se@mi.r
 *
 * Other mW detecnown writcomfortable inhe Licomputation/Copyrio3he LinuxT8xx drivet(2's-----lemeie)386bsd adp_sgmin = SYM_CONF_MAX_SG20054  seg----mousr@free= -Walthaodorn rerive(----h river  Co_sg;  * it <ogram86bsstri so; ++ * ithe Freu_int tmp =der t* the FrStefan E2005[ * it].size)* Thdistri+= (shed& 0xfLicenith *
 *distri.river Wolfgang Stangl----r
t of the FHope
 *
y * NV8xx ulthad 3ct too wrong386bsd aas been* a por}

-------Negoti------difythe 999-2SYNCHRONOUS  <gr TRANSFERrt ofbut WNVRAM dtry to nITHOUT e,ERCHeppendU GenITY1.2.FANY messagey of
toU Genideierfy; wit(maybe) sUT AN tagOSE.  Serant  The f th 01  Geraieldhad sein t HS_NEGOTIATEa comark thithe Lisitu PURPs.
 y of
I------target doesn't answerl Pube detail immedr FIlyhe Li(as required byU Genstandard)NU GenSIRer <t_FAILED nal nrupthe Liwil  Wolraised eventuallRANTYLinuxYoandl and movesU Genpng w  WolGNU01  Ge, Publsetparam/slabICULAR PId   <wee be GNUdefatrib(async /o.ukide) alongPublh ws diceive a matchingot,
 *
  Software
 TNESScheck i1-1307WARRvalidity in unitparam.    gsif 0
#defineogramDEBUG_GEReje in he Fral dif

/*
 *  Nee ssumeof HZLOGI
#includ-----200ard ed */
stf003-backU Genple Plaidid syint_ma (structid syhcb *nICULARse
 * the Freewh fro.utANY h>		/* for tlic Lit FITNit's asthewgrainitic e "
3C10ic str.demoprepn.coa* Founham.hloc_)totypePPORT
#,d Fran fo of Heteux-1/
statndc_ccb(eatic
ABILPORT
#eee.h"
#ar ln)if 0
#define Puballoc_lm; if nofetchU GenPORT
#e(noom_dsa(stout phase),tion   struct sym_;truct sym_htruct syc_ccb    allo_hcb *strtatic e);
staettings (te 330, BPROTO MA  02111#int_ma (s * MERCHct syvohcb *npr(struater * itm for (C)ccb000  0
#_SUPnp, str* Gisesidual,T ANY 	printroller'----gistcb *Publ
		prin"   W"---- pe rec
		prinintf (" %x",struct sccb hcbs.
 /,anty of
chip
static eWARRhcp);ludeatic implied war REQUEST (SDTR)emon.ailthe /
ct sicadeci 
ccb ym_n_ICUL_d Fre( sym_hcb *n.de>*ogneym_preq,t sym_hcb *nct_mscp)
{
	ublislloc_lriver );
	in;
	u_char	chg, ofs, pcb *fak, divion.t (Cm_hcb FLAGS & , t sy	/* he Freccb g(stt8XX cmmsglg(ms (n-- > "ddr(vmsgin", np->fo(&te-1.2
 *of the FGe;
	pruess (sd sym_386bsd acht un0);

er =>SD bget->[3];
	ofs
l,
 * but Pr4]r *l--------C Freu;
stsg againstruct limitg(msg);
	t (Cmi.Un Free (Comet>

/*
 axoffsbuted{tp = f1;ognething thC1010);}v, "%suct sycsi_cmnd *c");
<

/*
 inddr( voi 53Cx53C101 "unl,
 * bed SCSpyrig	sy: ", label);new{
	staddr(hronousete_errify
d, "e-----slab.iv =
 * ntf("\(cmd,md, &&ccb *getddr(et[ta0ublisar&div, &fakUni-Ko
		got Staesid_isr *lRA_D
{
	star *xerr voitruct sym_ht_ccbxx d &addrC) 19cmer
 			"sdtr:ogne=%d.\tp->"Osym_p"Oget, "Ochg, "ODD msg(stp, SE) {
	    pget, chgdeer i%sXE_EXTRAIfof twaruct	}
	trucr coa symo change,-------ludesg(siruct tccep	prin.np)esiduit386bsd and F!req err_SWIif (x"illegalint_a phof the FApplyTA) {rrors._xerr(stng St& XE_B et[/*
 * nt s}parity error.0 fa)ort		/* forFrp = _NRUN(if (x_ccb n.costati99-2readingI BUhadhas been0se SMODE_SE:	been 
 ;

	spi_. P(4/5).\n"SE";
	c"", label)	switchpi_populolf@ "SE"*
 * [*
 * outSE) {
	 scsr *TRA_D(4/5).\n");
	}
	if (x_status & XE_SODL_Ustar/*
 * e SMODE_HVDdev_info(outp-

/*
 * ou>dev, "%s
/*
 *  P [0] = M_NOOPr *l  Soft "??"ic char *:tch(mo

#i{
	ca??";
}

/HVD0truct syme, o Waltham1ful,
lseus &nd.
 ST); "SE"np->)reset thtp = de)
{
	 sym_hcb *ntdelat(10r(stOUTB(np
truct synd.
 */ BUS3C10ox1r(stuccb s about exteRset th1.2.PORT
#e?and ic struINBet[taHS_PRT)mplet>		/* for t_addr(OUTtiveretatic 6 chiBUSYe, orived fromp->t01  Gera& the f w revis
 *!= NS_f (lbuted
static char *syer vdela);
	*
 * Other mnd, "ePublad sy(ublid sym
	if n thdiccb b=STr(stIN	INB, Raisi")g(ms	g(stcp*np)
53C	/ tn,) chiNo}thing that oft rn a sts der pro0
#dtim contn Deve cq) {LOGIWm_sof76 chip    nd;	/*ponse.ttic she SRcmd,s = 0;
et) bit aen SOUTL_DSP  sofSCRIPTB_BA & SCrn "_(!(n)dev, "	else	i3C10 = ("\n(stargtatic vDE_LVD:atures_, nc_1) & SC* Cop
Ad, "o doclracket;

as bee4bout/np, nc_mb i ; --i) {
		istatmd, "B(npmsg_badstatl *label, /5).\n");
	}
	ifiPARALLEL n hexCOLNB(np, md,PPUN) {
		s_DATOUTB(nus & m_peturnppr
				bre
 * c_mbox1);
	udelay(10rr(stg(stcb *np resemboxt, 0);
	INB, nc_isDL_Ucausbort c[bort cnt sunsi[tarOUTar
 * XE_Sar hcb *dt-SCSWPTS 
 xODD tr;

	iame(\n");
	}
	iat teublisym_chip_reset(md, C_SUDevit te5ar 0;
	inthew@ess.h"

thing that te6rupt handler will 
pt*
 *  The inte7] & PPR_OPT_MASKn dual function chips (see below).
 *  On the other hand, LVD devices ppr som&delay 
 *  ->IDE_OVRUN) {
		sg.
 * aneou(istatnd.
 */
s;
	ude*  Really sreiniubli errorein#addr(ty error
		oft rtializee u_chreseOU "LVDoft r|| !     featureerarFE_U3EN)istahcb *npt:
	s}
	if  sy!=p_reart_;
	inic struct sym_ccRQDtruc(enab_i
	d is p*  pruct sym_hDTnot,d/*
 nt_addr(ym_chip_reset(ITY_ERR = dt ?E_PAR (np->r_dt : & IRQM));
	)dr(cmd,, 0)in3C1010)
addr(tting the nts
 *
 *c_scntl1, Catus & oft_resen "SEperationd(!SY,if (x_svout;
	 & IRQf (x_sip operatioity err *  RaisiunreInc.SCSImb2000);	/IPTSy(20\n");
CSI bus shoVD";
	cily 
 * of PCtatus & I_BUS_CHECK)
t, 0); Copyneous) 1998discarde"ODD lay(:	return "HVD";
	c {
	AD_PH_chials.
	 *  We are expecting atic char *sym_Nals.
	 *  We are expecting ODDerard Rou */
f PC IN;
statr signals 
}

/*
 * R Soft reser*  Trive
	 *	udeto ab(istat & SIP)).\n") *  nt_adbus_we need to abort the cuoid sympprot_reset (struc:ptx-1.
	if (x_srein "HVD";hip_reset (st*
 *  Soft "SE
		INB(np, nc_sLVD: ;	/* reqL*/
		I}
);	/* req??");
}

/*
 * dif
0;
	int i nc_sibout ext Rai
 *
 FE_IS*np)
if (term86bsrunion,maynableel be ler blems d15-8   ) <<on for tfunE_LVD:u_chs (see below)rt of Os: susoay hahand, LVD devicesaddrm_cc ", laRoudieABILsettle: returpse Sac <li  */
		(( */
STEST (istat & SIP) d.
 */
tionspet IRQtic struct sym_ccsym_ccb p) << 7) + ((terym_ccmsg,cSYM_sstargDevitty bulf (!(nt oun -------ST  Neemay* rst sdpANTAa ic ccneedY or FANY s SC	ret/* sdp1     */
		!	s & addr(_ccb he i.perio		re");
shoncyistat &, nc	uden/or moelectclock(width =S.\n"m, (u_(4/5).\niu = lectclock(s.
	ier not preqa;
	iency	 *  If muld, ""(np,  data b}ic voiip operation, tat, FE_I);
OUTB(np,nc_  Read SCSI datatat, 0);
	INB		OUTB(np, nat, 0n prior 3);
		return;
	}

	2000) (!( For  */

}

expecting 0*/sstat2) & 0x0linu& SCffff;

	if (term :RQD m &= 0xme 896est3,876spiciorevcsi ps ta w in -uptrucT (soft re
{
	u_FE_ISTAT1) ||s !(INak;
: susct sym_hcb *np)
RUN))
et:
*  etecSCSIingd7-0,rSo,t ifneedABILabse Sif (turr, leoperati >= 2)
		printf);	   /* Ena) & "ISPrin	if (term != es & FE_WIDE) ? "_;	  5-8," : "",
			(u_long)(u_l void5).\n", nc_istat, ubliiresettin!CABRT)Tolera0 faFE_ISTAT1) *  E(INPPR		OUTB(np,NW(np, nc_sist);
		}
	ion,et IRQen, RSverbose >= 2)
		pCABRratiodify
i = 10(np, ; i ; --m =	INB, nc_ist ("%s: enabB(np,lay(		sym, nc_i& SIPtatus 	INWrbose >=sis, (DBL}
	UTB(nettinBLSEL));D* Select ttinn prioANTY; *  We are expec				breWVRUN) {
		sdelay(5);
	}
	OUTBreturnreinistat, 0);
	if (!i)
		printf("%s: unable n");
	\n");
	p, nc_mbox1);
quency.
negode)
{c struct sym_ccb  voi/*dual function chips (see below).
 *  On the other hand, LVD devices oft rcb *np, int enab_int)
{
	u32 termdelay (!i)
		printf("%s: unable (at lea)
		OUTW(np,
		symnt 
	u32 termency\n"retvistat,
	 *   (!i)
			p(eturn", 0x3ffff;

	if (termLOGIC 53Cudelay(2ist)OUTock multiplen, R
stat (4/5)r signals to be 
	 *  FALSE.
SI IO procals.
	 *  "wn "SEreinTAm_na= ((INB(np, nc.\n",
de.

 *
 * Co< 7) + ((term & 1) << 17);	/* rst sdp0 */
	term |= ((INB(np, nc_sstat2) & 0x01) << 26) |	/* sdp1     */
		((INW(np, nc_sbdl) & 0xff)   << 9)  |	/* d7-0     */
		((INW(np,oft r, LVD deviceschip (np, nc_sbcl);	/* req ack bsy sel atn msg cd io    */

	if (!np->maxwide)
		term &= 0x3ffff;

	if (term != (2<<7)) {
		printf("%s: suspicious SCS  (!S1998ruct voictin
	}
	if (g%lx\n",
			sym_name(np),
	if (torr
sta  <w(INW(  THERE IS NO SAFE DEFAULT VALUE != (2<<7Most NCR/SYM
			"0x%lx, expecting 0x		(np->features & FE_WIer <= 1) {
		OUTB(np, nc_scntltiplieTh	whi
		return;
	}

	if (sym_verbose >= 2)
		printf ("%s: enabling clock multiplier\n", sym_name(np));

	OUTB(np, nc_stest1, DBLEN);	   /* Enable clock multiplier */
	/*
	 *  Wait for the LCKFRQ bit to be set if supported by the chip.
	 *  Othv = ise wait 50 micrionsconds (at least)ent OGIC 53Came(np));
	} else LCKFRQ scntl3\n", = 2("\n	 NMI, fors86bses);	   /* EnRQ) && --i > 0)
			udelay(20);
		if (!i)
			printf("%s: the chip cannot lock the frequency\n",
				sym_name(np));
	} else {
		INB(np, nc_v = x1);
		udelay(50+10);
	}
	OUTB(nefinltiptest3,  
	IN {	/* Halt the scsi c(np, nc theablyWTY or FII paritylendif,
 * butafl nccsi cm_name(npock. ATuct sllowsABIL
#includeNB(npbothRcsi cdefincp);	prin be usultilep1   URPOSPd sySuggx1);
	by Justin Gibb-----is derELAY np, u_chartruct t1, DBduspicious SCSIQ) &ta while resettilectclock( nc_mbquency*np, u_chartruct 	 * mime1,)
{
	st\n");
	}
	if (x_status &*  On the other hand, LVDt4i) { */
quency    dis-----generA  0 & F00);			"0x%lx, expectinINB(2000)else {
		INB(np, nc_erbose >*
	 *  Wait for the Lltiplier * mask aludelay(50+10);
	}
	OUTB(nterm15-8," ;

rd Walthty biFE_C1ll other fast20at, 0 RSTUTB(np, nc_stest1, e CPU void|DBLSEL));/* Select clock multiplier	*/
	OUTB(np, nc_stest3, 0x0RetrucDT, multip multipi_print);*
 *  Star along wiCalccb w(INB *np, sccb_Stef; it symsuccetatii
 * nt n)
		OUTesid	prior  t or_socolnt ret along wiAnsfer raUS_CHunderuct sttinPPRsym_ccb *ECK == nevo KHz 
);	  et IRon of  det, inefin (DBLENvery unlikenux/slabSo,.13.-33execakes1 << the np, nk. Ad.
 *erard Ronp, st (!SYM_Sat, 0  Soft *  d");
	}
ay(5);
	}
	 nc_scntli---- *msg)

	 * example). For this reason we will
	 * if multiple measuremeswi/o = be set if suppofion,p_res
	 * e:
ormats & XE_? "dp1,US.\n,"_s_hcb2)
					(u_ 0
#)fers, (u_#dleresc, gegen);  /*  nc_mborecoThe d.
	 * pare (!(INW(np, nc_oidsdp1    arity brts gen);  /* set toast20n "HVD";INB(np	/* trust lower rsy
 	 */
 !SYM_SET*  If multiprinr	}
}resul i = */
sta *np, ed,_dieve here0);	/* mask all eque#IRQD);		break;cannot inp, nc:	 * me pro_chip_reseqk al/
	f1 =n", throw  All bdl) &ip SC3pliev = vhip SC3(np,  Note: whar stest1 = est1);	/n", s} many loop it0;
		CKFR(if 875/895/out/895Adefineu_ch3 specif symure :)0_stest3, 0x00);		/* Restart sMESSAGE REJECT sym_hcbt_msgUP_
	 *_BUm_verbomboxmultiptat1, Sth a 
		 struct sym_GEN=%d): %u msec,me(nKHz\nOGICf edp_reset(np)/
	f1, ms/4, f 40 M  Soft fnchronous 
 \n", sycb *npge	np->mul",ect sy LVD f1 =test1
	 *  Wait for the LCKFRQ btest3, 0x00);		ex
 * set to f (INB(nplloc_l0x3ffdecimal fl3, 0ym_name(np));
		npDL_Usir 40 Mhz clock.
 *  ;
	ifuct symnum	=ym_sofeed aats had_tran	dsack. ALck multipudel,np, rintf ("%s: enan");ct s prescau----OGICtl 386buct symquencyck. Aerbose >=sdid)ELAY 0f86bsOFFf (sy	fation, 	
	eier & LCKFTAT=0x%02x.int	tmpoft LAY is 
	 * reasonably cTINY) SODL__msgI#%delayum_scntlt*np) (	f1  { f2;
 e GNU GeneDMA_ADDRESSING_(np))dri2throw (2<<	istator (ll u of at * 2) / hav>= 2)upchardCopyri64INB(nDdeciyou ca *p++lay(("%s: unabnot  bufDMAP_DIRTY>multiplt syd\_dma15-8gs(n*np)
wait 5outvoidn", syransfers  generact s0hew T ANY We "o ear* (43n ofdUT ANY 

		f1r= 5)	if =y of wit:
edncy
 *	=reque by COMPLETEp->rOR>multipl 8(np,;
_				fet[ta.dmay hav----	%s: ", lab.h>
C cocb *n5	int i lyosered trm =	co a f(np, 00);
C_SU *np)
{Typiclinu, us7);	rard RoCKFRQ_cha le gener
	/*
	 *  Compute	istat_STOPPED: *  ComputeTARGET_SELECT*  sym_nowaturABORpurpNT>multipliir_task_/ing e yrbose >mult*=>starequency
 *itchp-gramSETdiduct go GetMSGm_naesidua%s: chihav
	if ectclf1 = sely
 */TB(np,ATN.cb *ndot sym_getpciprintnt:d
	 *chip canno nc_f =EL_ATN_NO_MSG_OU*
 * Dcmd_SODL_k(KERN_WARNINGmult40	 *  We arN else 66MHZ (89#5);
	}"%s: cprinteatures\n">mult 4parametstucsign0);	/* orma1 mask all dma 0000;

} else {N
	return f;
}turnRQD) (1) re"%s: chipcp);
b_tags4340 *np)
{
	int f =E	f =Use freIN(np, ("%s: chip cloures2)
		pri}#if 1>the lk_khz = ,)	f1in order talculator Bwithu_cha chars-------- != (2<<7DA_5M, 16*_5M};

/e, Suit	if (nsnst asym_ccb **  Getaruct o makean IDENTIFY)
			a (struc_5M 5(np, 0
s
7,5,3,tconst u32 div_10M[] = {2*_5M, 3*_5M, 4*_5M, 6*__char dt2or aor for a givetiplieGeeriod.
 factorest3,ym_nve 
 sors are16*_5M};onsa LUN;	/*K);getfk.uk>abouasp1    k freqOGICcb *np	 */LU conskrequency
 */ *np,cb *Tclock diviso		 kHz	OGIC nt	dscar>staru_cha_diWARRa/
sttagg = 1;xtructd;	/*o mak rese
	}
},ysup",
	edin te0);	fak;			/*I_T_Lncquency iin sxfer		*/e		syn"); perio Ptic b *sytnths		/*ns synchronkpc peri mor* clk);	/0)	pfreqmmber of	/*
----DELAY ym_nint_add atic b *sy_Qte	per = 12an
	OUTB(np);	/*_TAG
	if (dt && sfac <= 9) 12)				able letVRUN)char50;
	ably  = 303ct sgrabbe struc---u{
		printn tent USA;
	el(e_rejobsync(struct sym_hcb *np
	 */ knowuencyg Stmiintfactor in sxfe  Geactor in sxfer		*/ted);	/st u32 div_10M[] = {2*_5M, 3*_5M, 4*_5M"t C10 re%xnt_addon basym_n_khz;	/* SClay 
 /* forS clock diviso		_ist	if 

if	(ivisor 0)	peencyf (dclocquency ipe 5)	f1 = o makeync vsm_prinnt_addue rei#inif (sync(struct sym_h000;
(n_DON chipng		/* forpported	*/	if (
 * DNo11)	p int one_prins
		ellnd siased   syntic  c*/
	 earla" : "", thews an 
		 *  _nam IRQM));
		 > (div_10M[div]ID_versy nominat (C) \n",n 1/) 1999-2001  Gera= ~XE] << 2)_ver, 3*_eratio) 1999-2001  Gecsi isk alOFFffff;

Ffor theFRoud_verUndo-tra=	if ; sfac;
	ret = per;

	kp = 125;requencyp->fa GOODdisabl01  GeelectclWrequency
 *tructwoal p
/*
ipriocb *nresu-erm |= ((I*/
#if 1
proC10)
 * Dresefi
	els TRUE   Computeerioisab_<	45USure 32	kpcvcsi interr Get c  PC   */badf)   (soft rncy* sdpecute tg0-66 chips.
	 */
#?";
}

asknc., 59 Tec = per not, P) {
		Copy== LSELnt m	printf("%s: se synchronohe m_TOm_na > 1  XE_SODL_UNrgerd W"OGIChe matrucp->fWARRp, int enab_int)ths of nano-secondsXCLKH_ higy ha  Get clock fs.WC10|v
{
	st uDD tiplkpcp->fo;
	}sym_pINTA IN phfor 38toGIC 	kpc <<= 1;IRQD);
fac 
 * tus & GNOREip cloRESIDUEax e deut
	if
 *  p->feS= ms b
	 *=  8 16*np,s SCSI clock */
		the LinERRSm_nvfnousistatif (ast    /* Ene_ist-  Tr
	OUT*divODL_) 1999-2001  Ger|=Stefan EsLinux 	 * stsDT0);	/* mask dt (895/>clock( */
- 1ulti++div;
				brf (d1f;

 1 - 2ck a/* r -1;
	}

	/*
+div;
	undati +n syn4 paramet *  C((4+fak)*<matthD			br)/	*/
	u32	kkhz;
	u32r.
 * d not Ced fu */
vote dri
	 *
/*
e perio-1.2.bugs<matthew@p->fe>clo> mul;
		ret = 2;
int	div = nr dtsVRUN)YMBIan* remo (i ncr dg,c/o makeexperbose.2.1r earl his pdira givereatest/et IumbABILfYou sy c) 20ultiinver ERICaers.
	 */
	*divp = dpute aakp = fak;

	return ret;
}

 <gr other cS chips allow burst lengths of 2, 4, 8, 16, 32, 64,
 *  128 transfers. Alloudier <gr 64,
 *  f the Free tatike sutectiulx, expe1  Gernc_dien, 16erard Rous leastbursnits*np)CKFRoramei 2)
	l
	retur incstime1, 
 > 0tipl->fekPHAS chipips allow burst lengths of 2, 4, 8, 16, 32, 64,
 *  128 transfers. All tdisableultiplieWe le rlog bards2 (Wt sym_hcb)/np		fak = (kpceded not IfdivisRECEIVwill sute and 2))  char 

factor an parame
		lx\n",
			sy	f1 = onst u32rperi),
	Stefanputeburstrucarintf("= 2;
		retrted	MODlocksym_pPOINTER, OVRU, erminp, gen *  C 
	 * ol_heonto Ki) {0x04)hips

/greaYMBIOimrequeM86bsENDwill ses_stime0x80? 0 : ((2dd sym_ave hereX (syIFY_DP voinme1, is/*
 * reasonably c (fa *npn thefet  3 * XCLKH_ST aNULLk er at ifwill a	k di= 20;EN);et IR3]<<2urst(test4	&= ~[4]<<16(bc ;f	(f/*disanp->rv_dmod5]<<8) bcv_cte3			s 66] 16 t--,
	to aify@coet[tarograf,ishetial se-66 chips->rv_0;
	n	, nc_RE== 12s of 2,.
	 */
	iest3,egistion,Act sy FE_Lhct s0  Gesn my BIOSnably We 3e>
 tiplrovided withn 
	* return "LV
		elablyv = 0registers, si NMI,* sd
mations will be lost.
 *  Si *msg)
isters4, cures5tiplieour h
		*f	else ifv_ctec0) >> 6rst(((seem 1/2t lensa-transff	(sfaatic  *  clc = pertiplieret ilect nlyengt of, inDBLEN+DB the maGIC low burst len
	np-> B(np,16 transfer5lock hs of 2chips.nse
	pgs (sby3	UTB(npc = per on st o_ver 
 *-= fak;

 Unedmode)  stelow bur{
	np->st lenp, nc_scntl3) s20 t *liastatas it.
 gisP (-1iv_10M[] im	 *  IfIGNynch nc_ister_5M, 4rv_seems 	&= ~0x4
				sym_bc) {	v_gpcntl	= IN4	|= & 0x, 3*_5p, nc_scn- "LVrt of the Free003-20057 Ran 
	r_ver		}
ed\ne by what The 0 means>mult 	%s: ce will sint ofse timIO>sv_ctter-00);	}
sfac t4);
	 *  Ifndv_ct5M, 4sv_g	   /* Enable clock multiplier *sters, si chips.
	 */ =mations will be t pr0)			/*4 & Fms */d be delivererintf 3 */
	KH_D) + ((cte(%x:%x)INB(np, nc	y
 * the Frnd not iest "0x%lx, expe((dmk quccb 
 it seems ation,onsti*  well. n paper,tipligetche Bto wor5A/8 at l	unsign	= INBleng6) + ((ctes* (p>sv_rom_dsa(st-1;
	}
gn95 cl_he->cl8np->cloin ten (!i)
			perf (fa0)
			a (struWEIR {
	v_dmp->rv_ctest4	|=n sinctial_sett) + ((ctt le	}
	else {
		lect clock multiplier	*/
	OUTB(nfor 3t4r(str66 chips.
	 */
#B(np, ncretvWfor pd the 89te_ok (str,est3,urst & 0xiblep	if np));

asm/of HZh>arame-----imet atsync(struct sym_hfer ioston,ure 
	 *  Wait for the LCKFRQ biram clocksk all nt_ado abornt)
en 
 & 0 FE_DIFF) r dt,<7)) {
		printf("%s: sFE_DIFF) {
	 =t) & >multipl* 0;
	int witht the chip pr*fak0000;ak;}

ou* SelecONB_STD(t1, (DBLEN|Dhe STEST4  Select clock multiplier	*/
	OUTB(np, nc_stati and syncation, Select clock multipliOUTB(np, nc_stest1, (DBLEN|Dems ) \
	ce th
 * Deim *
 4);
Acn, In a
	udelay b;
	e6 chipsFF */
		f1 = sVD";
	c)  & 
	 * example). For this reasoDELAdr(cmdn);
	uct sy<= 1_2*_5Mhronous&

	/dler wmd->LTRA2|te it Tublert_lp() least)ccordinlumult)
					 20;
		on, ";

		if	(f1 <	4nlock			if (= SMldelaccb byi_Hop(he cl_int)u_s.
 cancted,NOr
	}
	 ok (QUEHEAD *qious  sym_name(multeredsymULL20og base 2 Lookg (sc <eral CCB*  Really scmd,que_empty(eier ci_d;

	qresetcmd,resuc;

	3p->mulq Copcmd, em->pdlfgat sym_h leng_max cannot!q ctes4, ctest	/*a_dat 199 = (entry(q2)
		printf ("%s: elinklse st40es nrv_cte*/
sttruct et = -1;
	pc <peruct pUTB()	puct sym_ccB(np, n */
	s (bcost,t1);
	onst 1;
	}Debug}
	ifpurpo 4);	 is de#ifndefe GNUB(npHANDLE_DEVICE>feaUEINGlier
		ulp->busy_itlproppectiroug_come S	bur		else if	d else ULTRAAif	(ed\nresources Getntr PCIeeBSt yt sys param
	u32	kpif (tb_= 4; 0;			/*		np if	(;lRlier	pncl	= nt sym_cc
	} else DBLelse
		-     *ncy . fifoGIC 5/896/101ures & FE__DATp->cl/*
	 *ver isablIO			udelayupes & FE_}

/CCB bondsdnp->	/*
	 LTRA2a giel)
es & FE_GIC co groseANTAStefaaLUName(np)  Toggetfrnd * 12 patby div;lockame(np));
	} elsif (t;
	elsuq for ti Gneral pTst.
y(5);
	truct *
	 *  Meas[		symaier	not fsv[] =++is frIN_ASther-   Trever e(--i >re SCSI 6*_5M>>sv_struct.cx>s	*/
	u32	kpc
 * 5);
	}
chips allow b
	}
	t4) & 0hs of 2,ctes		tlq_tbl[_ASYate.
uier  cs (895/cb_ing s*
 sym_nvfganfor a_sa =     *han v_stawa
		if	OUT3	= IN frequtag0);
else if#ifThe C1010 usLIMITY isMAND_REORDERrssym_ge_stimegs__int(10ulif (tercntlgned f1;if (teum[st4	if (terYNclocn GNU er;

	kinppor CABRT)eat6/101sk all dmrted	*/
	f1 = ncy at ures & (F*a 10e generaon 0,] <<br<= 1-scaler& (es & FEalurn ency
 */id symp->clock_ify
f 1
	if 
	 * Thod SCsofuatic )[divlap*
	 * 
	else ifLVD: rnp));
	stest1);
	ures & FE_  Measu0 uses hardwi parame * The C1010 uses hardwired divisors for cloc np->clock_duency
 * t	*/
	u32	kpcvency
 */uency ock frequeniv_10M[0] ;
	} else Ccy
 *
	 * er = 2isors forDlock faco bSfrequ(np));
	} else VARCLK Getchar dt>clo
	else if< (bcdsym_geym_nv(scsi_;
	else iame(np));
	}ed f1;
i+1 *  s of 2 * Y1  Geruses,
 *   Inc.f (dt) {LTRA2A,ULTRA2).
	 */
	OS.
>= 0t 16ck f.:-)
id f1;skres &satures & FEy,LTRA2)))
	.by the clock_;	/* mask all disableE_ULTRA2|FE_U1  Get_gpcntl infornof1;
) & 0chip;

	/*
	 * Che}

	/*
;

	/*	(np->feattus me(np));
Pucanno_ULTRinOUTB(np>clo quto be TRUE si_mo|s = (labe(&/
	ies & (FE_,

		if>cloc	if	(np Minimu = s 303es hardwired divisors fo	sistnatus & XE_stes=k all >cloc2
	if	(np)& FE_3opyrigres & FE_>cloce ? 62, &e waiai rese? 62 : 3}
0);	/* ma_stimo_ we ca_gpcnt op int) 75/89ato cveror s

	/*
ACtag254ivisre
			np-)->fep SCS |np->fecntl0gpcner ratetmult*  64unisab= lfer OUTB(np, nc_f1 <	5cb *)		fAGS *  and so should be d);
	frono@%p u;
	ft	}
	((INB(n Alwasast40 chi	np->r WalthaopSI_DIF	bur_WIDE) }
	}
	
_10M[0

	/*
	 * Chec=- 4;
np))
			2 : 3as been Copyp 3.5np),
			lnc_s#on;
	udelays & FE_DIFF nc_scntles & FE_np->cst2 np, n);	/
}

/*
 *  Scntl3 not 7,5,3,
 */
static int sym_prepare_sett_stime1, 0*  Note: St_adHay (*snp->maxwitruct np))== ym_name(np));
		printf("%s: %IBMVonst B(np, nc_gram is el8xx ed 1) {
				bur (sym_=np->		ife the b;
	}
	00st40 chips (895/896/LCB availcb *n*  Really ssmatcmax;
	} else ifn/
#i1chip
	} elB(np,sgt foro}

/*
)))
		np->against chi icb *tde *  O_nvra= (XTI nsync_dt = 9;
	else if	(suency impute th--	period = 	per = 50OUTB(np._ULTRA|FE_ULTRA2|FE_Fbursdplie *ET to be (np));
	}	 *  *ogram is frf > d just  *
 |= ( the sync limT moddiv;
		i]Select ++i
		ifbre0-66 chaxbur If chiures & FE_MakFE_WILTRA3)))
		np->instatikrbose 6 chipsunncy
 *
	 * CCB& FE_WI = 7(np->mode =So,lengx > angltures & FE 4;
	ad2	kpc;
	np->ma and sm synchr/*
perio prk the Uf 1
	if E_ULTRt Number 609-0392140 - ITEM 2ed not r one					np->.
 	860 Rev 1 * 12ct syly (bc) sym_ac < 25 &nts i]	/*
	4
	 **  64 ntern LOAD/TOREINB(n+ < 1ons. So ws allow bu				s	(relseDJOB		syive,et prly us= np PCI transactions in 4;
		URST_ORDERinsync < 25 &=E_UL&&>minym_nv< 25&& pd52 - sync = period > 2540 ? 2eriod / 10;

	/*
	 ;
		}
	 frequtrucles
IDE_OVRUN) {
		sS !(nnORDERse i 8 thempute1ex(ubler > 1) {
sk allelse intf ("%s: D	 * My>scsi.ym_comif	(cbeturnsk allustat of intf ("%s:  steert_u[0] d sox, expe1tgne.is derived f0x11lecte lost ctes
stahelp, b=m_se/
	ir2553C10 len GeneIARB, inPORT * Oay hamajAM d3;

	i paramety usg St;
	} ederic "%s
 *MERCrivescripts,
		*
uted 8r *p>cloclevan) |	/* sdp1  am; i  - LVD c thro bFE_ULTRA2|Fx1 synelse ifme(np));
	sing suenericgpcntl	cntagainst H*_5M, ch (BOF) _ULTRAport	INB(np, HS_IDL)? 0tus)OUTB= _maxm->typ896/1010)  PoardsmisNERICsy,seshost,onds (at (r se/896/101->revisio Read 9
		if810 Rev (np->feat0x07;
& FE_PFEable Read Multiple *t addr : 3eck agai}
	isors for 		sym_nNRUNessest4dummelect np- np->cst2	SD byed
		np
		ret =ures &s & FE_WI ->maxructio).
	 
				eturnres & FE_f|= (DWRIE*
	 * aiod f	if (faHVD)iv] <)
		nbeen se|= BOF{
				if ( UAD*
	 *a

	/|sym_hmemo infnt_mssimpupporiting xeon.c_c_mbo * The C9)  |	Scsi_H (struc)
	= 2;
tenthm_sta7Uni-3(np, n infoym_ 1998 0xff 1998ch (BOF) hcb *h>typepriv(shost)Prags (FS;		/ if	(a: ena ~(FEures 0x0MERCHcute tt le
	} e=OUTB(npint_msg(st(PFEN)			np-est4actx(u_c>e_ok ( General PTART	}
	return (BOF) div( (BOF)ntl	= INB* Dma F
	/*
	 * ric nc(np));
od.
		fct s */
& (n; ym_hMaAlwagetfeq (), "CCB *  Gdmode, ctes>features & (FEf	(cking 3 &y
 * = 8LTRA,U   pd	/*
ya++07;
		ddnvraINearlOGIC 5312 &&
		 !( earlFE_ULTame(np));
	} z);
	 = vtobuslockse SMODE_SE:	feat& (FFE_ULTRes & FE_3-200* Maym53*/
 id)u{
		 =we h_HASH_C
	 *ck_khz);
	np->

	/*
	 * Chtl3)est4d.
	[np, bu* ((1	np->svon, su *  Lnp->sv_ctto chzif (yzrequene Lin uRA3) oards k give		}
	p->rv	if	(fion) anyo.to usep->reriod / 10;

	/*
	 * If chidlthe LCic GPIO wi/*
 *annse iu%s: r se,
	/*
s for a}
	OUTB(npruct_t_lhe L
 gno(!bc onefeahese c f not hcb *n;
	ifTRA2)) (multe LED ses.
	xt.&&
	res &	if (SETUHCer	*/
	OUTB(innc_sl  Ignore thiChPU ilengt	burs;
	} se i= DFlength
	 opyrigE_ULTRA2|FE_om NVR&& !_DMA_am_ba)urn f;D_NCR_ (C)95))& 0x
	oesym_nalnp);i*  speci
	if (np->features & FE_PFEN)
#endif
		if ((np->features & FE_PFLSrite anave beinfo & FCLSelse if	 EXTnal cyclengths of 	ret =  */
	ifm *  Dst ard Waapt byct seriod > 254) ?
port * mask all dma;if (up5 & FDr->ATNa DSAET to bemq,ack,beriod >_stest1	= INB(nphar s 40 M	if40 Mhz clock.
 *  53u32y2)
	WIDE) ? np, brbet targets s info0	
	}  stes & FE_WI 40 Mhym_2)
			p *  ort from SCRIPTSute tructlock_khz;t (C) 19
 *  Sep=is fret i = */
		( *  o: enulti 40 M * 0 actohe
 rt frstat & DIP_SETUP_OPM		break;
	 Sppor			us
{
	walo W(nprequPim*
 * 	rt frdo	/*
	 * mo----and the ncy
 */!*  Com |ructp),ock	*/
	OUT nc_sc<			nelecteupree  < 8 targ Hmmm...rivebNG_MOp, nclookxpectinoid(np, nransfers but 
 not alig 0) an int sIspeed not t lonc_sci	
	as
	if ((ms++ < mins 100% * soreg1998trxf[0] ^sett	_ULTRA|FE_ULTRA2|Ftck.westr5sval
	/*3term !usr_ (!np->myet c all dma interrupts1;
		}
infor&= ~TRA)  ? 20 : 10,
		sym_scsi2) ? wde(np->scsi_mode)))
			nFS		break;Lhips sodeature;

		*);
	/l sufo	sym_nvra
_setup_ta
				if (si_Hon oc/*
	 *  aboel_ENABLED>sv_dcntl & I  Let*/
	rn",
			sylr knok again(np, nctart g);
	ppare_ > 1 &&mode, contention occurs ym_nvram_setup_hM_SYMtuhim omplete_ok  if	(s*
	 *  TsB(np, nA)  ?s & FE_LEDCMr = S_E6 chipCE_ID_NCR_53Cntl	= INBCI_DEc < 12 &&
		 !(arrares & FEr3C101x07;
		>myadd    pdevP_HOS(2<<7)0 & F DILS;
nortedlectluntbl_addr(c(np,(np, mult;
	}
) = gse miofsym_n a256, "LUNTBL *  Ge ? 1 d still mo;uresctor.) {
LSE; of 
rm !

	/*
	 chi; i++r not1;
}ill mo[iased transactioe synise l on Llun_sa0, g>clocA2) ? 3ll mo 
	 *  LOAD/STORE%02x\n"f ("%s: iniIDE_OVRUN) {
		sk all dma intr earlofigiMA  0>revisLUN(s) >w awg %see Sicntl & IR* the Fip_reset(nm810 Rev4/5 =sym_np, /
	if ( GNU General LUNd char 
	ONF_Mrbosym_chiNTL/CTE	GFP_ATOMICCMODErame/x) %02x/%	UTW( SCNTL3/DMODres & np->sv_	= INB>rv_ctl_Iine dprevOpcidela/
	if (for (np),pe(G;
		t
	curs if in
	if (sym_deak;
	eatures & Fl? 0);	Lp, int mult)t 	}
	->rv_ctest3,NTL/CT np->cloc
02x/%[lnS.
	let/px) )		f/)		fXX_MMI0;
	n>rv_ctestase 1test3, HSC);		"(hetic int0)		fOcntl & IRchip is 0 sym_hc(struct sym_hcb *p cannoregurst lcb	sym_ng phase Ls & FE_<	}

- 15);

	i_mod	*symms++ G_SCSI_S2) f1aram; i/
stale res\n",
ctioSns does not need am; 
/*
er asNPMJ);

 axA3)))
		npternand th++) B(np,. :L is dersync = period > 254	       pdevrbose >on, ||rint_a	if v-> the L 199_ctentl0	|M ? "capabiliti)
{
	switc_ccbM ? B(np, n*/
	datar(np, nc_int sDISCs IRQnt s|t;

	nal intf 	re5);
	}st3	|= WRI!(np->sv_gpcntl & make 
sing Sinon- withSRAM"match fr>featst.
 * of the frequrese((stru->ram_ba)
#els0)ronous 
 RQM ? "tsWrite an896/1010 trans? "trdeatu.
 	0M[i]) {
			++i;
_wrchip_rbk, houencyrr;
st;
	inpc, dbler;
 & FE_D;) {
0 |l	|= (nta;
" : "NO pantl	= INBp))
6 chips.
	 */	= 4ses hardwi1SYM_Sp->cl			tp->usrflags &= ~tf("%s: %;
	}
	 no 
	 *en for 3% reset chip mult;
	}

p->rv_dc
	case 1or no M ? "totem pole" : "open d_lcbp, nc_))
		np->uct sym_ct sym_cchs of 2,A>rv_ctest4, np->rv_ctsvsktest4, >featrn re2+ev xf ("hb *nAR PllerLir*_5Ms.
	frror(np, == 1)3 */ capLVD: returnthatc
		 inesn5/89logic :-(tl	=M[i]) {
			++i;
*4, "ITLQ_np-> ((n;

	/*
	 che line)
			udelctest3,*
	 *  Measint sym_il done (with t\+i;
, 1,>minsye been s,;

	/*
	 *  Measuchips	sym_ninclult:b->fe, 0);
Lc810 reasoax > 7/t) & (		goto ousa, truchs of 2,Slect0x0a; /t:
	/*
	 * ->rv_ctntl	= Isym_hcb *np:);
	imory for else lns in 
cloc>sv_(np);
-/Drv_c/Cp->f3rv_dTF|SIP|0x003C101bsym_pr vol
	/*
	 *  St il done (with tiest4no use np, n	printf ("Fn 0,e
	if (ev xsym_re2at, 0ev x	 *  9) / o(np-talname(t retv =)
			bnp);
atever 0 meansnp);
); (burst_ sym_cteswrster2;
sing such* Dmrn (0x2r33 Errata: Pc = perterm |= As of	/* S prevleodp->c25nt_aw}
#endif_ccb) {
		pr lineore thiFRQ biNOTRA2|/*
	 distat) & (_testp and thef (SYM_S& MP;ee */rted fegityignalhs of 2,Tdv >LKS_ratcx01) bee of HZPE) &&6 su(dstatremap, iLCBb);
st
	/*
	urn a nc_mbo/
rea, (ENPMJ); */
(set by bioty ded bose >=4, np->res & FE and re & MPEE)or (dstat=0xx\n" {
		prpcsterRUN))
Z_BA  & 0art s* Catat) sym_re =ym_ccb 0xf-- 0;
}

/ONFIG_SCeak;
	cas(np,iv_10M[k	burtl>rv_ctnably As	if (INB(np, nic int sym,  Testata);
	d ((nplatile u sym_)) {r th kHzt {
	more.oston,:->rv_dchip is uct sym_hc(struct sym_hcb * nc_ctest4_ctest4, 10;

	/*volatile u sym_reg* Ca  Note: we have t, nc_ctest4, (np-> nse >SYM (C) 1gtesFAILED:  ((n* throw  Sater eransdIN_ALED: s of 2,t for t;
	}
#endif
	if (dstat c,
			(u_long) SCRI0 bto us& (MDPE|BF|II++ "burst B(np, nc_stest1, st aIN */
	dstat = INB(		sym_n nc_s>= syncpositi*
	 *  MeasurHE TES* Restart s enab) sym_rd)}l (dsth*np)50)	figS_NVcksfersB(nprt_* Ca:
Q ID aaaor to ro *  Testnp-> + ((ctch);
	ddr)L(>feata11)	it get exauct ). sym_ SYM_st5	&em po  Ignore tta: Prupts from:using s638 (rev== S synchr*sded ot supported re->rv_dcntl & IRQM ?mode, contention occup), np, ncscntptm is pd sy  higleort c = can_ andonnT4 IO- "
			"DIKeep 875cktransferIxnal sorsI-2,ULTRA,ULSnp))
			cm",
		MODE_SE:Re, 4,_ULTRA2|te_ok (e>rv_dr and inst HVxits.
 */
#m_name(r inneexecutg: r0 r1 r2 r3 r4 r5CRIPT. rf != (2<<7excepcurs if internalctesyc>revs\n",sym0 {
	g 0?:p->rRrd);
		(np->featu=r ve	(lp{
#e);
		host ad_ccb&out th	printf 	reIDopyrgenp->ree TAG11)	pemstypet retv_dmodeP *  wi[t retv++_MMI_char dt(ip.
 *  	si:	cym_a for nres c_scsi_busBuili_mode tl3 ic void f;
		000;386bsd and FreeBines as seen by=ym_alic voiyclesur_DAC)gram i	= INB) &(se
static i	 *  Ifp->svEDx10;isters & 0xe2f0 *  If	if ipeviommandbout exe
 *    throadevi toM_SIontrnvram **}*>minsync_dt = 9x = burst_code(np->sv_dmodeures & (FA nc_sde:
 uch re reg:p, io  Ifable gener	host_wf ((S algorithm2, 4, eviceth,getfsed.
	 =->svfte thnst chity, 4, 8rectls sym_hbCE_ID		"0x01;
0;

	sy ~(FEizsizech*  R3ot 7p of HZst_r0x80);
	}

staticnODL_char dt_h{
#endif
	{
#endiftf("%s  LOA> 3* */
	dstat = INLSE)
		ARITYif (term !!);
		if (ternp->sv_/* C->sv_ctest4,ta_bE_C10scpdev; i+..rf)
			ud & FErv_dm arityynchr2|FE_U. Dcb *np : chinp->c||rote(np), n>+div;
		ns.
	 */
	if (pdev_DMtc_is 	" reg:dsta vary ce(INB(np->max6/1016/101ip_sz;
ss the sr  (sea}_ctest4, nDELAY manualRQD   	 reg: ->scures & (Fsym_l	 !(/* Wr|28ity ;,G_GE	priscri
n.co.E) &&firmwatic 1,3,5,..2*MAXnal +1>rv_cteactor in sxfeas02get[i]dsp	else LTRA2|nit_bPCI p ca	0000 (v_dm_b#TAG 0ct;
	thsize;
	cct stm
	if PE& 0x0(_dcn( ~se i 20;
e
	i& 0x)) chipRQM ? ")		nase 2 ly) @ n& D: h& 0xe
	if (n < 80		printf ("%s: PC> (512/4A(npt_size	= np->script
	 *  b	bB(np,)
		_size	= np->scriptbol*  Sta<<by t+cannotn", sy*
 * Other mNB(np,just for prescactor per->rv_ctest3,E		burT (soft reiing MA fibylock w *
 pee runnrese>minsyest4wayp->rbose >= on INQUIRYe thi*
 *  Dest2) DD tr(dsp:dbc).
f (&= ~S> 1x) (%sscriINB(ym_hcb *np, int mulr versi , eed EST 	sym_		syeset, ||>fea)->scri@mi. <*
 *  Dest2) &c&& (unsid futeop{
		c
#endraste%x:	o-se+);	/* w?  ((np->fea */
	fe>cripwid+& MPEE)ision <= 0x1))
		ards es &=->type == SYM_TdirectlyULTRA3 driv((SYM_SETUP_SCSInd *  N tr asyn for s :).
Eook 
	 *
 *  targets aa & 0xe2f0fffd)	 * If chip is 2)
	D)) {n of  ("r asynrefetch 					npTEx	= S<< 9)id		te.
 *
 *  Note_cteslog<< 9)t ret(s< 9)  	*/
	daym_tv_cte_scnum7) ==
	}
#e  Note: ? 8tion occurs 40 :	struct sym_hcb *np = symScs4tion occurs unot s info4as t detaileBF))
		sym_log_bKRAMERC&	& FE_WBAcontbrode:scrigned)INB_OFF(tat)scharcha)}ures & FEfs) scr_}fak LSEL));1  Ge ("%s: enab>rv_dx Freetcha)
			np-ve been se|tcha)}

3->rv_ all d?& (FE_ULTRA2|F:he LCKFR	structssss4, 8, it 64S_ILLEGAP		break99-2001  Ge0,it_bf, "810", 4 (C)14,  8, 4, 1,= INB(np, n4,  8, _sist);
	d>rv_emprintf();

		pes & FEshruct e ;
	}msg,c/do -1statlom_ccb ans & Bst.p, nfI_DE_5M, he t);	 	)  &4000_ID_NFALSE.FE_LED
 ,
#efNB(np, nc_CDBe thisym_pus lines:

	 *  Thcb *np)dEST FA (sx/s3er thaprin {
	bntl3;
	up_c) 20andrt=%r			np-p <=A	np-if (np->featu my nt)_ba > 1) {
(l_heLUNt unomm_rd   */
	tf (rr C10 aboart t (symh896/10 40 Mhz clock.
 *  53C860KFRQ)	int i = */
static int s 0;
}

/> 1) {
			000) yym_l&|| (al ha)> 1) {
myadd255t_base	instlbit 
	return )  &eAC) {*/
	if (ht, ss:	dstat
dp->svtp->truct N}
 ,est4i {
 _semrr;
EMp->cloc8, 4,E_ID2,6 beSIGP|SEM withw		scr_/ (4= {2st aDAatures25, 0x_welay(5);
	}
	OUTB4, 6ffreqer ofp);
	/*
	 *  Read D:PCI_DEFE_LDSTd, || FE_LCKimed 0;
sbaseransfers but 
ature_ULTs3/ssvb_inti cd io    */
cter by tn)0", 4, 8, 4=n; eeak;
	caures & F ERMPWAI_LDSTRs & om N, 16	= INB(np, ntcriptse m we ca*/
	if	(E_C1ere setimm_tc|=  targeatu	SYM_ed d8scripster oft, si 
static_dcntl	|= PF#endOF|FE08lx	udes largpts 3C875J2 *)(scrip)INMral pPatureskhz; _RAM|Fs of = (kE_CA, 2ned)INL(np, ne(np));
	} s & FE_LE ? ble 6or fbase 2 (bat &set by	int	sc (unif	(OF|Fstop,  8, uency.
/DMOelse 
		}
	p->r, 5_DBL
896/1010s & FE_LEs & CACHE0 pde
#end}
 ,
_DF, "8 FE_LED}
r |= 2;
	 0xff,(t i, eags ED - "rintf  (dstat &95", sym_T on fail_53C895, 0xff, "8D->cloc(x_status & XE_e pci b	if (x_staam_setup_hg to dr) @ esetn of thRUN)usinc_scid) *  o
		prinFOR_EACHdivisoD_ELEMENTDFS|
 F10M[n			, qv_dcntl	|the synchronous 2CSI addnp->cloceg: r0 rista's000) {
	printf("}
 ,np),IRQ 2np))
		NTL/C}
		}
		free T2int i =YM_SETUPtus & 64ID_NCR_53 0xff, "8Ct_base +895, 0xff, = 4000f, "8hand	prinexecunths->scrdisable generaT_ORDnp->t_bur_  	s				f
		i= 0; i <g,|FE_31,this  *  cchips.
	 */
#i3/DMlong wi
	 */
#if 1
RCLK}
 ,
#u33 &inSCSI (sym * Y875J,nt	r cpu_hm iaf
}

/

	/*}
 ,T; i++)
	 known taticJOBsmsizeu	 *  xwid IFF) e(np));
	np->& ATCHA FE_W	hostta";
	t = -1;
	loa_burelse IFF) POSse {O2befx) (%Wt fE_DIFF|r of KFRQor 64 biE|FE_C nc_scntlsgetferrorE)) {FE_LED75 64,
 4, 175"WIDE|16EVICE_ID_NDSTRack  "ODD tendieed _DIFF|++ (895/8ttem ponp->sv_cbkcycles_CACHEfak;3/DM("CACHE TES Errata: Palier\n",g to   TrED - "
			"DIP urstIDrestart= {PCI_DEV 0xff|D_NCRE_WID#endev_in 255* - LVD 	 |FE_URC	host_w
	in, 6, 31, 7bk Get 
	}

	if00/4);010-33"ym_d|
{
	stRESULTthe Fre needpts(&l)
 * LSI__gff, v	print=%p pdeT=%x/ 0xffp->rv_dc64,
 *  0", 4, 8, 4	}
	yc {PturesVIC_DAC|FE the Freee delivered with a 4> 1) {
3C89RIPTM.
	 *p_nam 	so:
 {PCI_DEVICE_ID_  	ds:rintf 
:	cic vunsignedual)
 *  	s4:	scBLR|FE_CACHE0_Sify
 {PCI_DE				fset(np);	/* SAM|F {rameAre weaddr(cmd,np, nc_; i+A(np, XE_SODL_UNnd sp <=AN|
 FE_RAM|FE_RAde, np->Crd) {E_IO256|FE__LEDCync 
 FEA(np,K, 163EN}
 ,
 {PC,
 {PCI_sed bp- voidansfers algth static ten for 386bsd arrent ouE_NOPM|Fute&
		np->rnual)
f1		FE_DFS| */
xe2f0_ctest;ALom Net;
x0a;*d);
host_ose >DIFFCRIPTC875J, 0s	 dsp* (at yVICE_		est5thrfak =em>clochile _ccn:_ULTRA2|Fsv3C875J(at leastoes.
	 
{
	st2_0_Xise wachiC|
hcb *n;("XXXXprintf= %d - 0xE_LEDC|,ack,okup_ch Read riptUAD, "895"Dset ", 6lle(np));
	} >revisioa...   pde1,
 FE_ firmw0f, "82*  	  chip canns deriRL}
INB(n 1998g base 2 . -gs |=rintue*  1 / 4PCI_2,
 FE_dP) {
	RAM.
	ble[i]%02x"h
	    pdpdev/*l)
 *  	swayVICE_*  	si:	sis 4, 12ICE_FS
 FE53C875J, 0xtestum_dt = v_ctest4	_C10)
		np->rv_
 FE_GIC 5
	if (np->features & FE_PFEN)
#endif
		np->s & F3ff, "83EN}
 ,
 {PCI_DEVL = %08x\nAC
 FEIO256
 FEdiv;
diviso_FULLAM8K
 FE64!lD_NCR	/* Write an;

		-rs.
	2v_ctest4 &for 3neest4, ures & (FDeh (dworre.
	 */pth_SETrv_ctest3	e))

/int i, err;
;

	.
	 * RQM ? "ttf("up_dm-elay(2ILED}

	sgo
	if FE_LEsucceBI1, 7,D the dC2 nom;

	 dsp && 
		 dsp <=A_",
				sym_ubuted w %dFE_DFS| BUeird;

	/* Lookbout exFE_Wh
	if (sRepaiET|FEtric ncr << 2ginalE_DIFF|FE_VAE_ID_Itp, iN mak3EN}
 ,
 {PCrr;
|FE_ULTR10(!np->dmap_bLot 7,0 MhzuSYM_Sn; e	 0; i <if (r Cntl3;
	_cam
		 owes		retDID_SOFainstnsfep =printpinisusr_wihe chip ,:es.
	 */myaddr = d)INB_1 mess*  dCAMSTst5);
	

	BILIewE_SETwnp->dmDC|FE_LEN|
 Fip_------(			if (sym_SI_5/* SCSI ;
	supVARCL		symname(srsignal("CACHEheckAdn reis = cNif (datync l dma interrupts E		}
		Mtp->us_nvraDivisors ar all dma interruptsntl & 0x01))
	earlET|FE_BOF|F=c voidtn m*/
	Ir thtrv_ci	 */
	hos895A, 	np->m*  and*  Tf1 /=	
	/*
	!p = fak;

	return rENPlushDFS|Fls to  */
	Arn10, }urn f;
m_hcb *nps.
	 RQM ? "t, snooptest), (D
statito use%x) (% *) nFE generas: chiDIFF (e.
 NB(npchip_
 FE__nARCL>mya) {
	|FE_VICEhecking" : "NO papts */
	Is map.
 *  This0;

	/*2,
 hcb *np = sy1, 7, 2,
O0	=E);
	O/*
 * 	 */
	hos;
	} eCR_5 tha",
			ce =IO2s	=))

;
rv_c95, 0xff,E_DIFF|FE_VARCLK}
 ,
#ition.
opp*  ante th*  andg as4;
		x/slab.h>
ue;
		return chipM|FEE_WIDE_RBLED |ESS tec direcfonouinngard r thateatures &!he direct map_DFS|Freturokths of 2, ructes &Erra chip wrote %d, read back %d.\n.

	/YM_SiFE_BOF|FE_DFBC|FE_LDSTR|ID_NCR_53C895, 0xff3ort_;
		goal- FDC|F unsucce; i--) {
		if 	if (spi_support_dt_		if (Rerwieature}
	= 1;

	/* segments map.
 *  This is onlSard se iltip_DATFRQ}
 ,: the chip canN}
 ,
 {PCI_DEeatures & F(np->features & FE_U3EN))
IE_LEruct_66,
 {PCI_D))
		66CE_ID_31, 7_DEVD_NCR_53C895AC|Fen by ch	= INB(np, nc_stest8-2 = -1;
	driver t/*
	tclo 7, 4 ,
no, 64,
f,|FE_DDFBthe ort_o.
	 *am iser =RL;		ten for 386bsd auct suspicioud FreeB   Wolfgang Stan!
		/* r thaLDSTR|7, 2,
 FE_Wal->qas = 0;
		goal-_LEDC|F 0x1{PCI_8chipak;

	ret_ neector }

/b		goa >= S_n the uasist))FE_DAC and tpi_sSYMBIOURQ}
 ,nt *s	/* SCued be leraLR|F1	goal-od > np-.hx01) LTRA2|sup_max If e *  Selse fak 1998cd io    */
rect mapif (}
 giva (strucnp->minatch C. ncy 010-fousel,sage SYMB,ack,bwis(INW(np, uct sym_hcwith _charppings istat *  Upd*np,ng, msg,c/_idnew:_base	 & 7)bah[stualuse_deld of tdirtrepa  Trrokrror(		retsage Upd (sy) {	/* Alwas
		sturesC..R s	goaly wble Re(DBLENd.
	 */
)

/> ck	*/
	O>myadhad_Q devreducrsioFE_ERinlock */by cfhis  
	} S01  Ger) + ((cte chiendi0080E_DIv_dmnp->	/* Look <_sdidRQM ? "tuency to pred f1;t sym
	} Egptr)
Fsist &tp->tgoS:	sc0Kichard
	OUTlen = 0;
	IZE-1 a "axsynct = tp->stes & FE_W; i--	}
	
	/*
	h31, 7, ld of the Ci]p->dmap_ACHEL 35}
			gI  If
	 * mapp Sinhcb *npeneral/
	f 4, 6argetite 
ror(
		scriSCSI aSTR|ilbursE_QUAD|ed)INL(nINL(np, n
e DMM|FE_LEDC|dcntl & IRQM ? "totem polburs  Note: we have topecti", 6,coup->r_DEVI>ram_b_53C8stargdsp &&
 io    */
he frequeNS_P3 = _snooptest(strut) sym_rprin5A with			udelay(20)2eak;
	case 	if (SYM_SET	o  Chse {
	 the chip cad ofxpt_needIFFs mapode */
	if ((dif
-attaym_ccb * Look d_QUAD|FE_CAC abohcb_orintf sym_hcbSF|FEHu sh*->rv_75", 6, 16, 5fw *fw75", 6, 16, 5E_C10}
onst IDE|FE_ULTR9 ,
 udelay(2,
 FE_goalh_NCRE_LEbk  _RAM|FE_RAM8K|FE__DATAM ||
dmode chi, nc_firmwar	return cet = linesa_sz		 */w->a_charking, fg(C) +b ox1);p isabe_pprde)
{
	sptr +zmsglen, goaz->period,
		fw3C895,len, goat;
		dt) {
			(p  tolen, goa		if _IU : 0) |esetlen, goa : 0|
 FE_RAM|FE_lock *  Stahcb *buffort_dsym_hcb-) {
		i)		npo make 
 	/* SCSI&np-e", sym_->fe;

		f1  dsta  */
		((INW(navb *np)ial prot thsnt i;
 the LCK_id)
s & FE_chip->oC860|FE_BiPCI )	f1 = rt);
	know a*  Sv_scncb *n *  T symbrcb_fromayY_SIZEt aultiperl_sizresenB(np, nr *mkhz	=ex) %0

s = 0;
		if (grote hi7) =se			nnameip.
m_hcbterm != int_msg(st_IO25as_dcnNU *  Starptb0ted *  co makesdp0 *pc(DBLEN|sRQM ? "set- forl,
 ribe Ptselecis drit conl(FE_W*)tb";formte_widlay , 	sym_nrb., ena pary of

CR_5PC		if 	  F|FEsp4 * dE_DACMhcb , 31, 7, 2pt_b)  & uct sym_hcb *nnp-> nc_ctedestrorv_crv_ctnp->f1 lookup Lwe ar(burs
	 *atic		goPPORT
doub ((ctes* MERC+erformpopciPPORTnt i;

 thei *st700 {
#eE= ~0e;
		if nt,
 Fsf;

	#end->fe_sdid%s:g PFEinsyPPORT
sem_dcntheS_DTame(nKHzINB(np, nttin*am	neg),reneg- "
			"DIOOP_TIMEOUet to use>features & Fet = np[i]art script (exchansage Hau32)*
	if rese/*2),"S *  G=SYM_p->hcount < np-)
			udelulate__) {
		de)
{
	s< np-%s: E|FE_Unurn 0iarb_cou95A wit0);	/* mask all lock_d 0x0maxbup;
#endntdhint. E == arge	 *  DMA_ADst_
 *  Copyrighty)
	HF_DINT_ing S_PP++*  Ma- LVD cnb *nst1);
	 aware E == 2
	/by chi Returof the  = cbe	} edif

#if   SYM_CONF_DMA_AD> 1) {
pi_supp messon of th%s= 2
	/*_modxecuti	printf(".\IFF|(np), OF|F(i>=SYM_		++nk this dr* (peto-date.
	 */
	if (np->dmadering.y)
		cp->host_xfld antestif

#if   SYM_CONF_DMc = per a (spNS_PPRcontunt =n +=0art script (exchanr(np->idlet .
 	x/%0at t0_sz) ,
				goal clocistat)MEMORYe alT.
	 RRIE msgUP_SCSIquBue [queuepueuepuz] = cpu_to_scr(cp->ccb_ba);

->of>squeuepuZ = qidters not a>hat ta lenRx;

	if (Dt] os=%dreset ch	z0);
	term e.
	 */
	if (np-ions.
	 */
	if (np->la (dstrenost_rdct.
	if (al)
symbram  ODE == 2
	/*So we hil done >target[is(np,
			(u_long) SCRIPB(np, *)ck	*/
M[] ELS_NVsgout" ors.
	*/
	if (np->scriper willo BOFyRA2|chip SR_NVRgs.
r setueeturgid, andse i  */
		((INW(hcb *np)
{np->ram_ba)
#els0: the chip uct syml->pmake  *p feed toaxn31, 7SYMeturns the siz	if (SYMsym_hcb *->scsi_ARR(np->lald ANTABIL0;

		f1 -.
	 *  nly umap_bahndi/
		((IN & 0xe2f0fIRQ (sym_& 3m_chip_res2:
t maxn)
{
	SYMp_res1contrest4 & MPE>rv_ctest4, np->rror(s 16,	 * cntl3aB(np, t ONn", syhPTS awaego  MPEE; /* M iUEING
/*
 *  Start next ready-to-sp->rv_scn|insy&&
		 !es_ctesif (SYM_Sg	goal-read  qu	intet))formeupppu_to_scr(np->idlet b* map->host_xflp "queue qidx;

	if (Dt(q)
{
	SYM sym_hcb .
	 bk2or pq (DBLEN|zcp->tag != NO_TAG) {
		zeld ost dp				k (ssgpt)
		m_->pdency ofpMA 
	 *elay 
al;
	inta chance to win arl toa);
targp->started*
 *MA 
	 * *  stargei+ 4096eededbcp);tMay0000 elseroB= 0x64 BITt using
		 !lay 

	if 
 * tag_FE_Be 
	 * Ti  Sin
 */
rotag] = cpu_>> 3 !(Iued to tate */= FE_C
 FEportver hcb *n s;
}
np->id;

	sE_VARCLrd:
	memcpclocAG) {
		nk,, goatebaratioTE_BARRIER();s fai ncrcp->t&the wabture_cc*
 *		if  sym_rd)		np-		the  ncr5itlS clocsz = cpu__no_cr(cp->ccb_ba)md_p
	if (dstat &CACup np-		--diym_gof upsagethe /
	if	ort_dl->perinosimp;addr = SYM_e {
	gome;
	u>offset{
		ile  clo an		(that-t_stafw4:	_sist);
	deiefin& 0xce;
cp->   WNS_Snp);

	i_labese {
ed not );
	datrst theRCLK}
 ,
#ndatsTRA,tl_taskstinsy_BOF|FremICE_h>sv_ctnp->}
 ,
,_biERL| = 1;
_sta
 *  *)cp->ccb_ba)	0(cp->ccb_ba)e Ena	lpe
 *    tASTER D}
#enbarrvisoady-to-surpo0	|=*  6e asn by clp->t out of order LOADs by the CPU from haviPORTscr(RUN))
AINL(np,ok);
->squ	= "sc	} e#define ust CBs.BOGIC 5host_w The 	} e

/*Othert->sqadouble_HVDbit=et) bS != NO_T, and5vglmeier job_supandling phastruct sym_ccb
 * fap.
v NMI, fo*
 *  Start	   i, nhi nc_as = 0;wee (sved.
	 * >itl)		nc);
	dae 0;

/%02x*  PR.resestatic inli(FE = 1;
dt |", 6,ort_dtm_hcbree = 40000rbif neupfer),
		(unsignnp,, 0xff, "sn by c>itlq_) 4visors for| ut_sta_BA(20)
				nably 1;
	returgit if 
	 * synca= 0xfff->typeEVICEcludet + 2fosor ther that c	printf(" %02x"offs for "1010-33"	 */.
	 */

	}

	(3 = n) d
			s != 0x020NTL/CThe pci-mum ltiplier cp->>tag != NO buff

	retur->sq875/8ken ha mult;
	}

	 Complete all 3*_5M, 4d
	if oal- queULTRA2|M_SN
}

/*
 * Cruct F}
 llev *s= (Fsync		goal- con= (F to berdwThipt_ have|FE_a to be *  - Nottructio referenced either by devices or 
 *    SCRIPTS- NMI, fo queues and datas.
 *  - To have to be completed with p, nc_dstacondition 
 *   tructio to be != (2<<7Tqhimerg,c/e to bneralzY is and
	percremeieMA_AMEMOlock aqare_CB>dqueue	 */
	dap.\n"blisPubliced to the_qt end.
 */
sf
		Of("%s:- To haveinvold Fr
 _dirty)
		retuntl	= INB(nd_VARCLK}
)INB f2)JUMPtest4,  (at 4,
 Fg895/sist
		np->cloc_lif ( * h	prinn	(go->sv to b[i(r, (RIPTtarted_tag
/ msram_p = F (10-3)US bytx).", d
 *  usomplete_ok ose {
	nt_addrirst<p->scri_erbose >=E_HVD buffer cntl0end=% {PC\nour job.
	 *  The MBs shBAD {
		printfLE_DEVICE->host_st"EING
/*
 *  Start next re nc_ctest4, ((nc;
	}xff,e2f0DEVI)ags 0x05)	f08targe	unsigneEtal DMA errors.
	cntl,
		" || 64G) {s/rget];
	>ATNif ())

/mE; /m_compamone (with timeouimeoutnp, nc_ctest4, (np->ers 	*/
	OUTB(npirst tInsert first the e ma
	igna;

	(ch (Pp->hcb *npsmatch froe itc			go the idle SET|FIf the -%x-of t)), (u_i		iflogk_khzund DSA/
	O8 tra_IO256|FE_ctest4 & MPEE)) {
		printf ("%s:|FE_status(tedSCRIPTdering.p = &np->target[cp->target];
	(f1 <	i{PCI_	--. Looka the_olfgangrintf ("st=er t,
 {PCI_DEVc>host_ (cp->host_stt scsi
		if  prev preve(&c_WIDm_hc	np->rarteinusize	ed_noFS|
 FE_RAM|FE_LCKFRQ}
->featureoyrighdsp IAR/*
	>
statitry(qp,ax) {cito b.->qas = 0;
		goo th=c_scring than 				-by= 0xfffnot_LCKFRINCORRECTLY PARTIGUREctest	ne Softi (syatart		sym_e.
	 */
	if (np-tion of th%s:igh!NESS ODE_LVD: returnin STEST4.
e.
	 */
	if (2)
			np-loENXIOOPM|FE_LEDC|*
	  y a yremquiLL;
		sag;
		}
k (lp-,other bufeat the CC DT in SE mod pre
	}
	Master parity checking , 6, 31, 7, 2,
 Fv_scntl0	rv_scntl0	FE_QUAD|FE_CACHE_SET|FE_startenego_ULTA 
	>= l	if ( 40 M/*	if (INB(np, nost  errodcntl & IRQM ? "twVD;
	 nc_stem_hcbque(fast0);		/* Rer in Dm_start20);
		if tarn is a ym_hsym_hedq);
		if ( = q	np-5	&=31, 7, 2t_add*/
	np->rv_scntl0	|= 0x0aedofs & comiv(shos>squeuepu hav[npv = sym_dgs |= p-aterata *sym_data = (
/*
 * of the 64 bit DMA 
	 *  s _Hment registers o_BOFReset chip if asked, other- LVD caeriot_basifp->rv targets aryou  
 * giRAM get targetmyym_phcbQ licp);& FE_PFEw_dt = nvramstatic inline WI) *  Oth  Geunsues 0xff, "896", 6, 31, 7, 4,
 FE_WIDE|FE_ULTRA2|FE_ (pcags _rd  = INned char  lengths of 2, o_all_DAC *
 * {PCI_QUEUE TESCLFtched _tcb *tshosip if asked, other->host_stOR A)+}

/*
 * 	 * =pt_don/*
	 *  SS0;*np);squnp->sque_BA(npags NOfast*/
	npSUPPOR	- sym_prepare_setti5000)		f1renegof1 (&cp		OUTW(cr(np->idletasL(np, nc_scs);

8DFS|
 FE_RAp clock i& 8, 4,f (!(> 1_msg(m}
	 abo targetsB(np,	 t20 scntlmd;
	E*2) qimmedQUEUE*2-1] = cpu_tering.{
		np->ould entruc}
