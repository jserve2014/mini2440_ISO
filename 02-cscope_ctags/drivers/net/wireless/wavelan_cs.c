/*
 *	Wavelan Pcmcia driver
 *
 *		Jean II - HPLB '96
 *
 * Reorganisation and extension of the driver.
 * Original copyright follow. See wavelan_cs.p.h for details.
 *
 * This code is derived from Anthony D. Joseph's code and all the changes here
 * are also under the original copyright below.
 *
 * This code supports version 2.00 of WaveLAN/PCMCIA cards (2.4GHz), and
 * can work on Linux 2.0.36 with support of David Hinds' PCMCIA Card Services
 *
 * Joe Finney (joe@comp.lancs.ac.uk) at Lancaster University in UK added
 * critical code in the routine to initialize the Modem Management Controller.
 *
 * Thanks to Alan Cox and Bruce Janson for their advice.
 *
 *	-- Yunzhou Li (scip4166@nus.sg)
 *
#ifdef WAVELAN_ROAMING	
 * Roaming support added 07/22/98 by Justin Seger (jseger@media.mit.edu)
 * based on patch by Joe Finney from Lancaster University.
#endif
 *
 * Lucent (formerly AT&T GIS, formerly NCR) WaveLAN PCMCIA card: An
 * Ethernet-like radio transceiver controlled by an Intel 82593 coprocessor.
 *
 *   A non-shared memory PCMCIA ethernet driver for linux
 *
 * ISA version modified to support PCMCIA by Anthony Joseph (adj@lcs.mit.edu)
 *
 *
 * Joseph O'Sullivan & John Langford (josullvn@cs.cmu.edu & jcl@cs.cmu.edu)
 *
 * Apr 2 '98  made changes to bring the i82593 control/int handling in line
 *             with offical specs...
 *
 ****************************************************************************
 *   Copyright 1995
 *   Anthony D. Joseph
 *   Massachusetts Institute of Technology
 *
 *   Permission to use, copy, modify, and distribute this program
 *   for any purpose and without fee is hereby granted, provided
 *   that this copyright and permission notice appear on all copies
 *   and supporting documentation, the name of M.I.T. not be used
 *   in advertising or publicity pertaining to distribution of the
 *   program without specific prior permission, and notice be given
 *   in supporting documentation that copying and distribution is
 *   by permission of M.I.T.  M.I.T. makes no representations about
 *   the suitability of this software for any purpose.  It is pro-
 *   vided "as is" without express or implied warranty.         
 ****************************************************************************
 *
 */

/* Do *NOT* add other headers here, you are guaranteed to be wrong - Jean II */
#include "wavelan_cs.p.h"		/* Private header */

#ifdef WAVELAN_ROAMING
static void wl_cell_expiry(unsigned long data);
static void wl_del_wavepoint(wavepoint_history *wavepoint, struct net_local *lp);
static void wv_nwid_filter(unsigned char mode, net_local *lp);
#endif  /*  WAVELAN_ROAMING  */

/************************* MISC SUBROUTINES **************************/
/*
 * Subroutines which won't fit in one of the following category
 * (wavelan modem or i82593)
 */

/******************* MODEM MANAGEMENT SUBROUTINES *******************/
/*
 * Useful subroutines to manage the modem of the wavelan
 */

/*------------------------------------------------------------------*/
/*
 * Read from card's Host Adaptor Status Register.
 */
static inline u_char
hasr_read(u_long	base)
{
  return(inb(HASR(base)));
} /* hasr_read */

/*------------------------------------------------------------------*/
/*
 * Write to card's Host Adapter Command Register.
 */
static inline void
hacr_write(u_long	base,
	   u_char	hacr)
{
  outb(hacr, HACR(base));
} /* hacr_write */

/*------------------------------------------------------------------*/
/*
 * Write to card's Host Adapter Command Register. Include a delay for
 * those times when it is needed.
 */
static void
hacr_write_slow(u_long	base,
		u_char	hacr)
{
  hacr_write(base, hacr);
  /* delay might only be needed sometimes */
  mdelay(1);
} /* hacr_write_slow */

/*------------------------------------------------------------------*/
/*
 * Read the Parameter Storage Area from the WaveLAN card's memory
 */
static void
psa_read(struct net_device *	dev,
	 int		o,	/* offset in PSA */
	 u_char *	b,	/* buffer to fill */
	 int		n)	/* size to read */
{
  net_local *lp = netdev_priv(dev);
  u_char __iomem *ptr = lp->mem + PSA_ADDR + (o << 1);

  while(n-- > 0)
    {
      *b++ = readb(ptr);
      /* Due to a lack of address decode pins, the WaveLAN PCMCIA card
       * only supports reading even memory addresses. That means the
       * increment here MUST be two.
       * Because of that, we can't use memcpy_fromio()...
       */
      ptr += 2;
    }
} /* psa_read */

/*------------------------------------------------------------------*/
/*
 * Write the Parameter Storage Area to the WaveLAN card's memory
 */
static void
psa_write(struct net_device *	dev,
	  int		o,	/* Offset in psa */
	  u_char *	b,	/* Buffer in memory */
	  int		n)	/* Length of buffer */
{
  net_local *lp = netdev_priv(dev);
  u_char __iomem *ptr = lp->mem + PSA_ADDR + (o << 1);
  int		count = 0;
  unsigned int	base = dev->base_addr;
  /* As there seem to have no flag PSA_BUSY as in the ISA model, we are
   * oblige to verify this address to know when the PSA is ready... */
  volatile u_char __iomem *verify = lp->mem + PSA_ADDR +
    (psaoff(0, psa_comp_number) << 1);

  /* Authorize writing to PSA */
  hacr_write(base, HACR_PWR_STAT | HACR_ROM_WEN);

  while(n-- > 0)
    {
      /* write to PSA */
      writeb(*b++, ptr);
      ptr += 2;

      /* I don't have the spec, so I don't know what the correct
       * sequence to write is. This hack seem to work for me... */
      count = 0;
      while((readb(verify) != PSA_COMP_PCMCIA_915) && (count++ < 100))
	mdelay(1);
    }

  /* Put the host interface back in standard state */
  hacr_write(base, HACR_DEFAULT);
} /* psa_write */

#ifdef SET_PSA_CRC
/*------------------------------------------------------------------*/
/*
 * Calculate the PSA CRC
 * Thanks to Valster, Nico <NVALSTER@wcnd.nl.lucent.com> for the code
 * NOTE: By specifying a length including the CRC position the
 * returned value should be zero. (i.e. a correct checksum in the PSA)
 *
 * The Windows drivers don't use the CRC, but the AP and the PtP tool
 * depend on it.
 */
static u_short
psa_crc(unsigned char *	psa,	/* The PSA */
	int		size)	/* Number of short for CRC */
{
  int		byte_cnt;	/* Loop on the PSA */
  u_short	crc_bytes = 0;	/* Data in the PSA */
  int		bit_cnt;	/* Loop on the bits of the short */

  for(byte_cnt = 0; byte_cnt < size; byte_cnt++ )
    {
      crc_bytes ^= psa[byte_cnt];	/* Its an xor */

      for(bit_cnt = 1; bit_cnt < 9; bit_cnt++ )
	{
	  if(crc_bytes & 0x0001)
	    crc_bytes = (crc_bytes >> 1) ^ 0xA001;
	  else
	    crc_bytes >>= 1 ;
        }
    }

  return crc_bytes;
} /* psa_crc */
#endif	/* SET_PSA_CRC */

/*------------------------------------------------------------------*/
/*
 * update the checksum field in the Wavelan's PSA
 */
static void
update_psa_checksum(struct net_device *	dev)
{
#ifdef SET_PSA_CRC
  psa_t		psa;
  u_short	crc;

  /* read the parameter storage area */
  psa_read(dev, 0, (unsigned char *) &psa, sizeof(psa));

  /* update the checksum */
  crc = psa_crc((unsigned char *) &psa,
		sizeof(psa) - sizeof(psa.psa_crc[0]) - sizeof(psa.psa_crc[1])
		- sizeof(psa.psa_crc_status));

  psa.psa_crc[0] = crc & 0xFF;
  psa.psa_crc[1] = (crc & 0xFF00) >> 8;

  /* Write it ! */
  psa_write(dev, (char *)&psa.psa_crc - (char *)&psa,
	    (unsigned char *)&psa.psa_crc, 2);

#ifdef DEBUG_IOCTL_INFO
  printk (KERN_DEBUG "%s: update_psa_checksum(): crc = 0x%02x%02x\n",
          dev->name, psa.psa_crc[0], psa.psa_crc[1]);

  /* Check again (luxury !) */
  crc = psa_crc((unsigned char *) &psa,
		 sizeof(psa) - sizeof(psa.psa_crc_status));

  if(crc != 0)
    printk(KERN_WARNING "%s: update_psa_checksum(): CRC does not agree with PSA data (even after recalculating)\n", dev->name);
#endif /* DEBUG_IOCTL_INFO */
#endif	/* SET_PSA_CRC */
} /* update_psa_checksum */

/*------------------------------------------------------------------*/
/*
 * Write 1 byte to the MMC.
 */
static void
mmc_out(u_long		base,
	u_short		o,
	u_char		d)
{
  int count = 0;

  /* Wait for MMC to go idle */
  while((count++ < 100) && (inb(HASR(base)) & HASR_MMI_BUSY))
    udelay(10);

  outb((u_char)((o << 1) | MMR_MMI_WR), MMR(base));
  outb(d, MMD(base));
}

/*------------------------------------------------------------------*/
/*
 * Routine to write bytes to the Modem Management Controller.
 * We start by the end because it is the way it should be !
 */
static void
mmc_write(u_long	base,
	  u_char	o,
	  u_char *	b,
	  int		n)
{
  o += n;
  b += n;

  while(n-- > 0 )
    mmc_out(base, --o, *(--b));
} /* mmc_write */

/*------------------------------------------------------------------*/
/*
 * Read 1 byte from the MMC.
 * Optimised version for 1 byte, avoid using memory...
 */
static u_char
mmc_in(u_long	base,
       u_short	o)
{
  int count = 0;

  while((count++ < 100) && (inb(HASR(base)) & HASR_MMI_BUSY))
    udelay(10);
  outb(o << 1, MMR(base));		/* Set the read address */

  outb(0, MMD(base));			/* Required dummy write */

  while((count++ < 100) && (inb(HASR(base)) & HASR_MMI_BUSY))
    udelay(10);
  return (u_char) (inb(MMD(base)));	/* Now do the actual read */
}

/*------------------------------------------------------------------*/
/*
 * Routine to read bytes from the Modem Management Controller.
 * The implementation is complicated by a lack of address lines,
 * which prevents decoding of the low-order bit.
 * (code has just been moved in the above function)
 * We start by the end because it is the way it should be !
 */
static void
mmc_read(u_long		base,
	 u_char		o,
	 u_char *	b,
	 int		n)
{
  o += n;
  b += n;

  while(n-- > 0)
    *(--b) = mmc_in(base, --o);
} /* mmc_read */

/*------------------------------------------------------------------*/
/*
 * Get the type of encryption available...
 */
static inline int
mmc_encr(u_long		base)	/* i/o port of the card */
{
  int	temp;

  temp = mmc_in(base, mmroff(0, mmr_des_avail));
  if((temp != MMR_DES_AVAIL_DES) && (temp != MMR_DES_AVAIL_AES))
    return 0;
  else
    return temp;
}

/*------------------------------------------------------------------*/
/*
 * Wait for the frequency EEprom to complete a command...
 */
static void
fee_wait(u_long		base,	/* i/o port of the card */
	 int		delay,	/* Base delay to wait for */
	 int		number)	/* Number of time to wait */
{
  int		count = 0;	/* Wait only a limited time */

  while((count++ < number) &&
	(mmc_in(base, mmroff(0, mmr_fee_status)) & MMR_FEE_STATUS_BUSY))
    udelay(delay);
}

/*------------------------------------------------------------------*/
/*
 * Read bytes from the Frequency EEprom (frequency select cards).
 */
static void
fee_read(u_long		base,	/* i/o port of the card */
	 u_short	o,	/* destination offset */
	 u_short *	b,	/* data buffer */
	 int		n)	/* number of registers */
{
  b += n;		/* Position at the end of the area */

  /* Write the address */
  mmc_out(base, mmwoff(0, mmw_fee_addr), o + n - 1);

  /* Loop on all buffer */
  while(n-- > 0)
    {
      /* Write the read command */
      mmc_out(base, mmwoff(0, mmw_fee_ctrl), MMW_FEE_CTRL_READ);

      /* Wait until EEprom is ready (should be quick !) */
      fee_wait(base, 10, 100);

      /* Read the value */
      *--b = ((mmc_in(base, mmroff(0, mmr_fee_data_h)) << 8) |
	      mmc_in(base, mmroff(0, mmr_fee_data_l)));
    }
}


/*------------------------------------------------------------------*/
/*
 * Write bytes from the Frequency EEprom (frequency select cards).
 * This is a bit complicated, because the frequency eeprom has to
 * be unprotected and the write enabled.
 * Jean II
 */
static void
fee_write(u_long	base,	/* i/o port of the card */
	  u_short	o,	/* destination offset */
	  u_short *	b,	/* data buffer */
	  int		n)	/* number of registers */
{
  b += n;		/* Position at the end of the area */

#ifdef EEPROM_IS_PROTECTED	/* disabled */
#ifdef DOESNT_SEEM_TO_WORK	/* disabled */
  /* Ask to read the protected register */
  mmc_out(base, mmwoff(0, mmw_fee_ctrl), MMW_FEE_CTRL_PRREAD);

  fee_wait(base, 10, 100);

  /* Read the protected register */
  printk("Protected 2 : %02X-%02X\n",
	 mmc_in(base, mmroff(0, mmr_fee_data_h)),
	 mmc_in(base, mmroff(0, mmr_fee_data_l)));
#endif	/* DOESNT_SEEM_TO_WORK */

  /* Enable protected register */
  mmc_out(base, mmwoff(0, mmw_fee_addr), MMW_FEE_ADDR_EN);
  mmc_out(base, mmwoff(0, mmw_fee_ctrl), MMW_FEE_CTRL_PREN);

  fee_wait(base, 10, 100);

  /* Unprotect area */
  mmc_out(base, mmwoff(0, mmw_fee_addr), o + n);
  mmc_out(base, mmwoff(0, mmw_fee_ctrl), MMW_FEE_CTRL_PRWRITE);
#ifdef DOESNT_SEEM_TO_WORK	/* disabled */
  /* Or use : */
  mmc_out(base, mmwoff(0, mmw_fee_ctrl), MMW_FEE_CTRL_PRCLEAR);
#endif	/* DOESNT_SEEM_TO_WORK */

  fee_wait(base, 10, 100);
#endif	/* EEPROM_IS_PROTECTED */

  /* Write enable */
  mmc_out(base, mmwoff(0, mmw_fee_addr), MMW_FEE_ADDR_EN);
  mmc_out(base, mmwoff(0, mmw_fee_ctrl), MMW_FEE_CTRL_WREN);

  fee_wait(base, 10, 100);

  /* Write the EEprom address */
  mmc_out(base, mmwoff(0, mmw_fee_addr), o + n - 1);

  /* Loop on all buffer */
  while(n-- > 0)
    {
      /* Write the value */
      mmc_out(base, mmwoff(0, mmw_fee_data_h), (*--b) >> 8);
      mmc_out(base, mmwoff(0, mmw_fee_data_l), *b & 0xFF);

      /* Write the write command */
      mmc_out(base, mmwoff(0, mmw_fee_ctrl), MMW_FEE_CTRL_WRITE);

      /* Wavelan doc says : wait at least 10 ms for EEBUSY = 0 */
      mdelay(10);
      fee_wait(base, 10, 100);
    }

  /* Write disable */
  mmc_out(base, mmwoff(0, mmw_fee_addr), MMW_FEE_ADDR_DS);
  mmc_out(base, mmwoff(0, mmw_fee_ctrl), MMW_FEE_CTRL_WDS);

  fee_wait(base, 10, 100);

#ifdef EEPROM_IS_PROTECTED	/* disabled */
  /* Reprotect EEprom */
  mmc_out(base, mmwoff(0, mmw_fee_addr), 0x00);
  mmc_out(base, mmwoff(0, mmw_fee_ctrl), MMW_FEE_CTRL_PRWRITE);

  fee_wait(base, 10, 100);
#endif	/* EEPROM_IS_PROTECTED */
}

/******************* WaveLAN Roaming routines... ********************/

#ifdef WAVELAN_ROAMING	/* Conditional compile, see wavelan_cs.h */

static unsigned char WAVELAN_BEACON_ADDRESS[] = {0x09,0x00,0x0e,0x20,0x03,0x00};
  
static void wv_roam_init(struct net_device *dev)
{
  net_local  *lp= netdev_priv(dev);

  /* Do not remove this unless you have a good reason */
  printk(KERN_NOTICE "%s: Warning, you have enabled roaming on"
	 " device %s !\n", dev->name, dev->name);
  printk(KERN_NOTICE "Roaming is currently an experimental unsupported feature"
	 " of the Wavelan driver.\n");
  printk(KERN_NOTICE "It may work, but may also make the driver behave in"
	 " erratic ways or crash.\n");

  lp->wavepoint_table.head=NULL;           /* Initialise WavePoint table */
  lp->wavepoint_table.num_wavepoints=0;
  lp->wavepoint_table.locked=0;
  lp->curr_point=NULL;                        /* No default WavePoint */
  lp->cell_search=0;
  
  lp->cell_timer.data=(long)lp;               /* Start cell expiry timer */
  lp->cell_timer.function=wl_cell_expiry;
  lp->cell_timer.expires=jiffies+CELL_TIMEOUT;
  add_timer(&lp->cell_timer);
  
  wv_nwid_filter(NWID_PROMISC,lp) ;    /* Enter NWID promiscuous mode */
  /* to build up a good WavePoint */
                                           /* table... */
  printk(KERN_DEBUG "WaveLAN: Roaming enabled on device %s\n",dev->name);
}
 
static void wv_roam_cleanup(struct net_device *dev)
{
  wavepoint_history *ptr,*old_ptr;
  net_local *lp= netdev_priv(dev);
  
  printk(KERN_DEBUG "WaveLAN: Roaming Disabled on device %s\n",dev->name);
  
  /* Fixme : maybe we should check that the timer exist before deleting it */
  del_timer(&lp->cell_timer);          /* Remove cell expiry timer       */
  ptr=lp->wavepoint_table.head;        /* Clear device's WavePoint table */
  while(ptr!=NULL)
    {
      old_ptr=ptr;
      ptr=ptr->next;	
      wl_del_wavepoint(old_ptr,lp);	
    }
}

/* Enable/Disable NWID promiscuous mode on a given device */
static void wv_nwid_filter(unsigned char mode, net_local *lp)
{
  mm_t                  m;
  unsigned long         flags;
  
#ifdef WAVELAN_ROAMING_DEBUG
  printk(KERN_DEBUG "WaveLAN: NWID promisc %s, device %s\n",(mode==NWID_PROMISC) ? "on" : "off", lp->dev->name);
#endif
  
  /* Disable interrupts & save flags */
  spin_lock_irqsave(&lp->spinlock, flags);
  
  m.w.mmw_loopt_sel = (mode==NWID_PROMISC) ? MMW_LOOPT_SEL_DIS_NWID : 0x00;
  mmc_write(lp->dev->base_addr, (char *)&m.w.mmw_loopt_sel - (char *)&m, (unsigned char *)&m.w.mmw_loopt_sel, 1);
  
  if(mode==NWID_PROMISC)
    lp->cell_search=1;
  else
    lp->cell_search=0;

  /* ReEnable interrupts & restore flags */
  spin_unlock_irqrestore(&lp->spinlock, flags);
}

/* Find a record in the WavePoint table matching a given NWID */
static wavepoint_history *wl_roam_check(unsigned short nwid, net_local *lp)
{
  wavepoint_history	*ptr=lp->wavepoint_table.head;
  
  while(ptr!=NULL){
    if(ptr->nwid==nwid)
      return ptr;	
    ptr=ptr->next;
  }
  return NULL;
}

/* Create a new wavepoint table entry */
static wavepoint_history *wl_new_wavepoint(unsigned short nwid, unsigned char seq, net_local* lp)
{
  wavepoint_history *new_wavepoint;

#ifdef WAVELAN_ROAMING_DEBUG	
  printk(KERN_DEBUG "WaveLAN: New Wavepoint, NWID:%.4X\n",nwid);
#endif
  
  if(lp->wavepoint_table.num_wavepoints==MAX_WAVEPOINTS)
    return NULL;
  
  new_wavepoint = kmalloc(sizeof(wavepoint_history),GFP_ATOMIC);
  if(new_wavepoint==NULL)
    return NULL;
  
  new_wavepoint->nwid=nwid;                       /* New WavePoints NWID */
  new_wavepoint->average_fast=0;                    /* Running Averages..*/
  new_wavepoint->average_slow=0;
  new_wavepoint->qualptr=0;                       /* Start of ringbuffer */
  new_wavepoint->last_seq=seq-1;                /* Last sequence no.seen */
  memset(new_wavepoint->sigqual,0,WAVEPOINT_HISTORY);/* Empty ringbuffer */
  
  new_wavepoint->next=lp->wavepoint_table.head;/* Add to wavepoint table */
  new_wavepoint->prev=NULL;
  
  if(lp->wavepoint_table.head!=NULL)
    lp->wavepoint_table.head->prev=new_wavepoint;
  
  lp->wavepoint_table.head=new_wavepoint;
  
  lp->wavepoint_table.num_wavepoints++;     /* no. of visible wavepoints */
  
  return new_wavepoint;
}

/* Remove a wavepoint entry from WavePoint table */
static void wl_del_wavepoint(wavepoint_history *wavepoint, struct net_local *lp)
{
  if(wavepoint==NULL)
    return;
  
  if(lp->curr_point==wavepoint)
    lp->curr_point=NULL;
  
  if(wavepoint->prev!=NULL)
    wavepoint->prev->next=wavepoint->next;
  
  if(wavepoint->next!=NULL)
    wavepoint->next->prev=wavepoint->prev;
  
  if(lp->wavepoint_table.head==wavepoint)
    lp->wavepoint_table.head=wavepoint->next;
  
  lp->wavepoint_table.num_wavepoints--;
  kfree(wavepoint);
}

/* Timer callback function - checks WavePoint table for stale entries */ 
static void wl_cell_expiry(unsigned long data)
{
  net_local *lp=(net_local *)data;
  wavepoint_history *wavepoint=lp->wavepoint_table.head,*old_point;
  
#if WAVELAN_ROAMING_DEBUG > 1
  printk(KERN_DEBUG "WaveLAN: Wavepoint timeout, dev %s\n",lp->dev->name);
#endif
  
  if(lp->wavepoint_table.locked)
    {
#if WAVELAN_ROAMING_DEBUG > 1
      printk(KERN_DEBUG "WaveLAN: Wavepoint table locked...\n");
#endif
      
      lp->cell_timer.expires=jiffies+1; /* If table in use, come back later */
      add_timer(&lp->cell_timer);
      return;
    }
  
  while(wavepoint!=NULL)
    {
      if(time_after(jiffies, wavepoint->last_seen + CELL_TIMEOUT))
	{
#ifdef WAVELAN_ROAMING_DEBUG
	  printk(KERN_DEBUG "WaveLAN: Bye bye %.4X\n",wavepoint->nwid);
#endif
	  
	  old_point=wavepoint;
	  wavepoint=wavepoint->next;
	  wl_del_wavepoint(old_point,lp);
	}
      else
	wavepoint=wavepoint->next;
    }
  lp->cell_timer.expires=jiffies+CELL_TIMEOUT;
  add_timer(&lp->cell_timer);
}

/* Update SNR history of a wavepoint */
static void wl_update_history(wavepoint_history *wavepoint, unsigned char sigqual, unsigned char seq)	
{
  int i=0,num_missed=0,ptr=0;
  int average_fast=0,average_slow=0;
  
  num_missed=(seq-wavepoint->last_seq)%WAVEPOINT_HISTORY;/* Have we missed
							    any beacons? */
  if(num_missed)
    for(i=0;i<num_missed;i++)
      {
	wavepoint->sigqual[wavepoint->qualptr++]=0; /* If so, enter them as 0's */
	wavepoint->qualptr %=WAVEPOINT_HISTORY;    /* in the ringbuffer. */
      }
  wavepoint->last_seen=jiffies;                 /* Add beacon to history */
  wavepoint->last_seq=seq;	
  wavepoint->sigqual[wavepoint->qualptr++]=sigqual;          
  wavepoint->qualptr %=WAVEPOINT_HISTORY;
  ptr=(wavepoint->qualptr-WAVEPOINT_FAST_HISTORY+WAVEPOINT_HISTORY)%WAVEPOINT_HISTORY;
  
  for(i=0;i<WAVEPOINT_FAST_HISTORY;i++)       /* Update running averages */
    {
      average_fast+=wavepoint->sigqual[ptr++];
      ptr %=WAVEPOINT_HISTORY;
    }
  
  average_slow=average_fast;
  for(i=WAVEPOINT_FAST_HISTORY;i<WAVEPOINT_HISTORY;i++)
    {
      average_slow+=wavepoint->sigqual[ptr++];
      ptr %=WAVEPOINT_HISTORY;
    }
  
  wavepoint->average_fast=average_fast/WAVEPOINT_FAST_HISTORY;
  wavepoint->average_slow=average_slow/WAVEPOINT_HISTORY;	
}

/* Perform a handover to a new WavePoint */
static void wv_roam_handover(wavepoint_history *wavepoint, net_local *lp)
{
  unsigned int		base = lp->dev->base_addr;
  mm_t                  m;
  unsigned long         flags;

  if(wavepoint==lp->curr_point)          /* Sanity check... */
    {
      wv_nwid_filter(!NWID_PROMISC,lp);
      return;
    }
  
#ifdef WAVELAN_ROAMING_DEBUG
  printk(KERN_DEBUG "WaveLAN: Doing handover to %.4X, dev %s\n",wavepoint->nwid,lp->dev->name);
#endif
 	
  /* Disable interrupts & save flags */
  spin_lock_irqsave(&lp->spinlock, flags);

  m.w.mmw_netw_id_l = wavepoint->nwid & 0xFF;
  m.w.mmw_netw_id_h = (wavepoint->nwid & 0xFF00) >> 8;
  
  mmc_write(base, (char *)&m.w.mmw_netw_id_l - (char *)&m, (unsigned char *)&m.w.mmw_netw_id_l, 2);
  
  /* ReEnable interrupts & restore flags */
  spin_unlock_irqrestore(&lp->spinlock, flags);

  wv_nwid_filter(!NWID_PROMISC,lp);
  lp->curr_point=wavepoint;
}

/* Called when a WavePoint beacon is received */
static void wl_roam_gather(struct net_device *  dev,
			   u_char *  hdr,   /* Beacon header */
			   u_char *  stats) /* SNR, Signal quality
						      of packet */
{
  wavepoint_beacon *beacon= (wavepoint_beacon *)hdr; /* Rcvd. Beacon */
  unsigned short nwid=ntohs(beacon->nwid);  
  unsigned short sigqual=stats[2] & MMR_SGNL_QUAL;   /* SNR of beacon */
  wavepoint_history *wavepoint=NULL;                /* WavePoint table entry */
  net_local *lp = netdev_priv(dev);              /* Device info */

#ifdef I_NEED_THIS_FEATURE
  /* Some people don't need this, some other may need it */
  nwid=nwid^ntohs(beacon->domain_id);
#endif

#if WAVELAN_ROAMING_DEBUG > 1
  printk(KERN_DEBUG "WaveLAN: beacon, dev %s:\n",dev->name);
  printk(KERN_DEBUG "Domain: %.4X NWID: %.4X SigQual=%d\n",ntohs(beacon->domain_id),nwid,sigqual);
#endif
  
  lp->wavepoint_table.locked=1;                            /* <Mutex> */
  
  wavepoint=wl_roam_check(nwid,lp);            /* Find WavePoint table entry */
  if(wavepoint==NULL)                    /* If no entry, Create a new one... */
    {
      wavepoint=wl_new_wavepoint(nwid,beacon->seq,lp);
      if(wavepoint==NULL)
	goto out;
    }
  if(lp->curr_point==NULL)             /* If this is the only WavePoint, */
    wv_roam_handover(wavepoint, lp);	         /* Jump on it! */
  
  wl_update_history(wavepoint, sigqual, beacon->seq); /* Update SNR history
							 stats. */
  
  if(lp->curr_point->average_slow < SEARCH_THRESH_LOW) /* If our current */
    if(!lp->cell_search)                  /* WavePoint is getting faint, */
      wv_nwid_filter(NWID_PROMISC,lp);    /* start looking for a new one */
  
  if(wavepoint->average_slow > 
     lp->curr_point->average_slow + WAVELAN_ROAMING_DELTA)
    wv_roam_handover(wavepoint, lp);   /* Handover to a better WavePoint */
  
  if(lp->curr_point->average_slow > SEARCH_THRESH_HIGH) /* If our SNR is */
    if(lp->cell_search)  /* getting better, drop out of cell search mode */
      wv_nwid_filter(!NWID_PROMISC,lp);
  
out:
  lp->wavepoint_table.locked=0;                        /* </MUTEX>   :-) */
}

/* Test this MAC frame a WavePoint beacon */
static inline int WAVELAN_BEACON(unsigned char *data)
{
  wavepoint_beacon *beacon= (wavepoint_beacon *)data;
  static const wavepoint_beacon beacon_template={0xaa,0xaa,0x03,0x08,0x00,0x0e,0x20,0x03,0x00};
  
  if(memcmp(beacon,&beacon_template,9)==0)
    return 1;
  else
    return 0;
}
#endif	/* WAVELAN_ROAMING */

/************************ I82593 SUBROUTINES *************************/
/*
 * Useful subroutines to manage the Ethernet controller
 */

/*------------------------------------------------------------------*/
/*
 * Routine to synchronously send a command to the i82593 chip. 
 * Should be called with interrupts disabled.
 * (called by wv_packet_write(), wv_ru_stop(), wv_ru_start(),
 *  wv_82593_config() & wv_diag())
 */
static int
wv_82593_cmd(struct net_device *	dev,
	     char *	str,
	     int	cmd,
	     int	result)
{
  unsigned int	base = dev->base_addr;
  int		status;
  int		wait_completed;
  long		spin;

  /* Spin until the chip finishes executing its current command (if any) */
  spin = 1000;
  do
    {
      /* Time calibration of the loop */
      udelay(10);

      /* Read the interrupt register */
      outb(OP0_NOP | CR0_STATUS_3, LCCR(base));
      status = inb(LCSR(base));
    }
  while(((status & SR3_EXEC_STATE_MASK) != SR3_EXEC_IDLE) && (spin-- > 0));

  /* If the interrupt hasn't been posted */
  if (spin < 0) {
#ifdef DEBUG_INTERRUPT_ERROR
      printk(KERN_INFO "wv_82593_cmd: %s timeout (previous command), status 0x%02x\n",
	     str, status);
#endif
      return(FALSE);
    }

  /* Issue the command to the controller */
  outb(cmd, LCCR(base));

  /* If we don't have to check the result of the command
   * Note : this mean that the irq handler will deal with that */
  if(result == SR0_NO_RESULT)
    return(TRUE);

  /* We are waiting for command completion */
  wait_completed = TRUE;

  /* Busy wait while the LAN controller executes the command. */
  spin = 1000;
  do
    {
      /* Time calibration of the loop */
      udelay(10);

      /* Read the interrupt register */
      outb(CR0_STATUS_0 | OP0_NOP, LCCR(base));
      status = inb(LCSR(base));

      /* Check if there was an interrupt posted */
      if((status & SR0_INTERRUPT))
	{
	  /* Acknowledge the interrupt */
	  outb(CR0_INT_ACK | OP0_NOP, LCCR(base));

	  /* Check if interrupt is a command completion */
	  if(((status & SR0_BOTH_RX_TX) != SR0_BOTH_RX_TX) &&
	     ((status & SR0_BOTH_RX_TX) != 0x0) &&
	     !(status & SR0_RECEPTION))
	    {
	      /* Signal command completion */
	      wait_completed = FALSE;
	    }
	  else
	    {
	      /* Note : Rx interrupts will be handled later, because we can
	       * handle multiple Rx packets at once */
#ifdef DEBUG_INTERRUPT_INFO
	      printk(KERN_INFO "wv_82593_cmd: not our interrupt\n");
#endif
	    }
	}
    }
  while(wait_completed && (spin-- > 0));

  /* If the interrupt hasn't be posted */
  if(wait_completed)
    {
#ifdef DEBUG_INTERRUPT_ERROR
      printk(KERN_INFO "wv_82593_cmd: %s timeout, status 0x%02x\n",
	     str, status);
#endif
      return(FALSE);
    }

  /* Check the return code returned by the card (see above) against
   * the expected return code provided by the caller */
  if((status & SR0_EVENT_MASK) != result)
    {
#ifdef DEBUG_INTERRUPT_ERROR
      printk(KERN_INFO "wv_82593_cmd: %s failed, status = 0x%x\n",
	     str, status);
#endif
      return(FALSE);
    }

  return(TRUE);
} /* wv_82593_cmd */

/*------------------------------------------------------------------*/
/*
 * This routine does a 593 op-code number 7, and obtains the diagnose
 * status for the WaveLAN.
 */
static inline int
wv_diag(struct net_device *	dev)
{
  return(wv_82593_cmd(dev, "wv_diag(): diagnose",
		      OP0_DIAGNOSE, SR0_DIAGNOSE_PASSED));
} /* wv_diag */

/*------------------------------------------------------------------*/
/*
 * Routine to read len bytes from the i82593's ring buffer, starting at
 * chip address addr. The results read from the chip are stored in buf.
 * The return value is the address to use for next the call.
 */
static int
read_ringbuf(struct net_device *	dev,
	     int	addr,
	     char *	buf,
	     int	len)
{
  unsigned int	base = dev->base_addr;
  int		ring_ptr = addr;
  int		chunk_len;
  char *	buf_ptr = buf;

  /* Get all the buffer */
  while(len > 0)
    {
      /* Position the Program I/O Register at the ring buffer pointer */
      outb(ring_ptr & 0xff, PIORL(base));
      outb(((ring_ptr >> 8) & PIORH_MASK), PIORH(base));

      /* First, determine how much we can read without wrapping around the
	 ring buffer */
      if((addr + len) < (RX_BASE + RX_SIZE))
	chunk_len = len;
      else
	chunk_len = RX_BASE + RX_SIZE - addr;
      insb(PIOP(base), buf_ptr, chunk_len);
      buf_ptr += chunk_len;
      len -= chunk_len;
      ring_ptr = (ring_ptr - RX_BASE + chunk_len) % RX_SIZE + RX_BASE;
    }
  return(ring_ptr);
} /* read_ringbuf */

/*------------------------------------------------------------------*/
/*
 * Reconfigure the i82593, or at least ask for it...
 * Because wv_82593_config use the transmission buffer, we must do it
 * when we are sure that there is no transmission, so we do it now
 * or in wavelan_packet_xmit() (I can't find any better place,
 * wavelan_interrupt is not an option...), so you may experience
 * some delay sometime...
 */
static void
wv_82593_reconfig(struct net_device *	dev)
{
  net_local *		lp = netdev_priv(dev);
  struct pcmcia_device *		link = lp->link;
  unsigned long		flags;

  /* Arm the flag, will be cleard in wv_82593_config() */
  lp->reconfig_82593 = TRUE;

  /* Check if we can do it now ! */
  if((link->open) && (netif_running(dev)) && !(netif_queue_stopped(dev)))
    {
      spin_lock_irqsave(&lp->spinlock, flags);	/* Disable interrupts */
      wv_82593_config(dev);
      spin_unlock_irqrestore(&lp->spinlock, flags);	/* Re-enable interrupts */
    }
  else
    {
#ifdef DEBUG_IOCTL_INFO
      printk(KERN_DEBUG
	     "%s: wv_82593_reconfig(): delayed (state = %lX, link = %d)\n",
	     dev->name, dev->state, link->open);
#endif
    }
}

/********************* DEBUG & INFO SUBROUTINES *********************/
/*
 * This routines are used in the code to show debug informations.
 * Most of the time, it dump the content of hardware structures...
 */

#ifdef DEBUG_PSA_SHOW
/*------------------------------------------------------------------*/
/*
 * Print the formatted contents of the Parameter Storage Area.
 */
static void
wv_psa_show(psa_t *	p)
{
  printk(KERN_DEBUG "##### wavelan psa contents: #####\n");
  printk(KERN_DEBUG "psa_io_base_addr_1: 0x%02X %02X %02X %02X\n",
	 p->psa_io_base_addr_1,
	 p->psa_io_base_addr_2,
	 p->psa_io_base_addr_3,
	 p->psa_io_base_addr_4);
  printk(KERN_DEBUG "psa_rem_boot_addr_1: 0x%02X %02X %02X\n",
	 p->psa_rem_boot_addr_1,
	 p->psa_rem_boot_addr_2,
	 p->psa_rem_boot_addr_3);
  printk(KERN_DEBUG "psa_holi_params: 0x%02x, ", p->psa_holi_params);
  printk("psa_int_req_no: %d\n", p->psa_int_req_no);
#ifdef DEBUG_SHOW_UNUSED
  printk(KERN_DEBUG "psa_unused0[]: %pM\n", p->psa_unused0);
#endif	/* DEBUG_SHOW_UNUSED */
  printk(KERN_DEBUG "psa_univ_mac_addr[]: %pM\n", p->psa_univ_mac_addr);
  printk(KERN_DEBUG "psa_local_mac_addr[]: %pM\n", p->psa_local_mac_addr);
  printk(KERN_DEBUG "psa_univ_local_sel: %d, ", p->psa_univ_local_sel);
  printk("psa_comp_number: %d, ", p->psa_comp_number);
  printk("psa_thr_pre_set: 0x%02x\n", p->psa_thr_pre_set);
  printk(KERN_DEBUG "psa_feature_select/decay_prm: 0x%02x, ",
	 p->psa_feature_select);
  printk("psa_subband/decay_update_prm: %d\n", p->psa_subband);
  printk(KERN_DEBUG "psa_quality_thr: 0x%02x, ", p->psa_quality_thr);
  printk("psa_mod_delay: 0x%02x\n", p->psa_mod_delay);
  printk(KERN_DEBUG "psa_nwid: 0x%02x%02x, ", p->psa_nwid[0], p->psa_nwid[1]);
  printk("psa_nwid_select: %d\n", p->psa_nwid_select);
  printk(KERN_DEBUG "psa_encryption_select: %d, ", p->psa_encryption_select);
  printk("psa_encryption_key[]: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
	 p->psa_encryption_key[0],
	 p->psa_encryption_key[1],
	 p->psa_encryption_key[2],
	 p->psa_encryption_key[3],
	 p->psa_encryption_key[4],
	 p->psa_encryption_key[5],
	 p->psa_encryption_key[6],
	 p->psa_encryption_key[7]);
  printk(KERN_DEBUG "psa_databus_width: %d\n", p->psa_databus_width);
  printk(KERN_DEBUG "psa_call_code/auto_squelch: 0x%02x, ",
	 p->psa_call_code[0]);
  printk("psa_call_code[]: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
	 p->psa_call_code[0],
	 p->psa_call_code[1],
	 p->psa_call_code[2],
	 p->psa_call_code[3],
	 p->psa_call_code[4],
	 p->psa_call_code[5],
	 p->psa_call_code[6],
	 p->psa_call_code[7]);
#ifdef DEBUG_SHOW_UNUSED
  printk(KERN_DEBUG "psa_reserved[]: %02X:%02X\n",
	 p->psa_reserved[0],
	 p->psa_reserved[1]);
#endif	/* DEBUG_SHOW_UNUSED */
  printk(KERN_DEBUG "psa_conf_status: %d, ", p->psa_conf_status);
  printk("psa_crc: 0x%02x%02x, ", p->psa_crc[0], p->psa_crc[1]);
  printk("psa_crc_status: 0x%02x\n", p->psa_crc_status);
} /* wv_psa_show */
#endif	/* DEBUG_PSA_SHOW */

#ifdef DEBUG_MMC_SHOW
/*------------------------------------------------------------------*/
/*
 * Print the formatted status of the Modem Management Controller.
 * This function need to be completed...
 */
static void
wv_mmc_show(struct net_device *	dev)
{
  unsigned int	base = dev->base_addr;
  net_local *	lp = netdev_priv(dev);
  mmr_t		m;

  /* Basic check */
  if(hasr_read(base) & HASR_NO_CLK)
    {
      printk(KERN_WARNING "%s: wv_mmc_show: modem not connected\n",
	     dev->name);
      return;
    }

  spin_lock_irqsave(&lp->spinlock, flags);

  /* Read the mmc */
  mmc_out(base, mmwoff(0, mmw_freeze), 1);
  mmc_read(base, 0, (u_char *)&m, sizeof(m));
  mmc_out(base, mmwoff(0, mmw_freeze), 0);

  /* Don't forget to update statistics */
  lp->wstats.discard.nwid += (m.mmr_wrong_nwid_h << 8) | m.mmr_wrong_nwid_l;

  spin_unlock_irqrestore(&lp->spinlock, flags);

  printk(KERN_DEBUG "##### wavelan modem status registers: #####\n");
#ifdef DEBUG_SHOW_UNUSED
  printk(KERN_DEBUG "mmc_unused0[]: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
	 m.mmr_unused0[0],
	 m.mmr_unused0[1],
	 m.mmr_unused0[2],
	 m.mmr_unused0[3],
	 m.mmr_unused0[4],
	 m.mmr_unused0[5],
	 m.mmr_unused0[6],
	 m.mmr_unused0[7]);
#endif	/* DEBUG_SHOW_UNUSED */
  printk(KERN_DEBUG "Encryption algorithm: %02X - Status: %02X\n",
	 m.mmr_des_avail, m.mmr_des_status);
#ifdef DEBUG_SHOW_UNUSED
  printk(KERN_DEBUG "mmc_unused1[]: %02X:%02X:%02X:%02X:%02X\n",
	 m.mmr_unused1[0],
	 m.mmr_unused1[1],
	 m.mmr_unused1[2],
	 m.mmr_unused1[3],
	 m.mmr_unused1[4]);
#endif	/* DEBUG_SHOW_UNUSED */
  printk(KERN_DEBUG "dce_status: 0x%x [%s%s%s%s]\n",
	 m.mmr_dce_status,
	 (m.mmr_dce_status & MMR_DCE_STATUS_RX_BUSY) ? "energy detected,":"",
	 (m.mmr_dce_status & MMR_DCE_STATUS_LOOPT_IND) ?
	 "loop test indicated," : "",
	 (m.mmr_dce_status & MMR_DCE_STATUS_TX_BUSY) ? "transmitter on," : "",
	 (m.mmr_dce_status & MMR_DCE_STATUS_JBR_EXPIRED) ?
	 "jabber timer expired," : "");
  printk(KERN_DEBUG "Dsp ID: %02X\n",
	 m.mmr_dsp_id);
#ifdef DEBUG_SHOW_UNUSED
  printk(KERN_DEBUG "mmc_unused2[]: %02X:%02X\n",
	 m.mmr_unused2[0],
	 m.mmr_unused2[1]);
#endif	/* DEBUG_SHOW_UNUSED */
  printk(KERN_DEBUG "# correct_nwid: %d, # wrong_nwid: %d\n",
	 (m.mmr_correct_nwid_h << 8) | m.mmr_correct_nwid_l,
	 (m.mmr_wrong_nwid_h << 8) | m.mmr_wrong_nwid_l);
  printk(KERN_DEBUG "thr_pre_set: 0x%x [current signal %s]\n",
	 m.mmr_thr_pre_set & MMR_THR_PRE_SET,
	 (m.mmr_thr_pre_set & MMR_THR_PRE_SET_CUR) ? "above" : "below");
  printk(KERN_DEBUG "signal_lvl: %d [%s], ",
	 m.mmr_signal_lvl & MMR_SIGNAL_LVL,
	 (m.mmr_signal_lvl & MMR_SIGNAL_LVL_VALID) ? "new msg" : "no new msg");
  printk("silence_lvl: %d [%s], ", m.mmr_silence_lvl & MMR_SILENCE_LVL,
	 (m.mmr_silence_lvl & MMR_SILENCE_LVL_VALID) ? "update done" : "no new update");
  printk("sgnl_qual: 0x%x [%s]\n", m.mmr_sgnl_qual & MMR_SGNL_QUAL,
	 (m.mmr_sgnl_qual & MMR_SGNL_QUAL_ANT) ? "Antenna 1" : "Antenna 0");
#ifdef DEBUG_SHOW_UNUSED
  printk(KERN_DEBUG "netw_id_l: %x\n", m.mmr_netw_id_l);
#endif	/* DEBUG_SHOW_UNUSED */
} /* wv_mmc_show */
#endif	/* DEBUG_MMC_SHOW */

#ifdef DEBUG_I82593_SHOW
/*------------------------------------------------------------------*/
/*
 * Print the formatted status of the i82593's receive unit.
 */
static void
wv_ru_show(struct net_device *	dev)
{
  net_local *lp = netdev_priv(dev);

  printk(KERN_DEBUG "##### wavelan i82593 receiver status: #####\n");
  printk(KERN_DEBUG "ru: rfp %d stop %d", lp->rfp, lp->stop);
  /*
   * Not implemented yet...
   */
  printk("\n");
} /* wv_ru_show */
#endif	/* DEBUG_I82593_SHOW */

#ifdef DEBUG_DEVICE_SHOW
/*------------------------------------------------------------------*/
/*
 * Print the formatted status of the WaveLAN PCMCIA device driver.
 */
static void
wv_dev_show(struct net_device *	dev)
{
  printk(KERN_DEBUG "dev:");
  printk(" state=%lX,", dev->state);
  printk(" trans_start=%ld,", dev->trans_start);
  printk(" flags=0x%x,", dev->flags);
  printk("\n");
} /* wv_dev_show */

/*------------------------------------------------------------------*/
/*
 * Print the formatted status of the WaveLAN PCMCIA device driver's
 * private information.
 */
static void
wv_local_show(struct net_device *	dev)
{
  net_local *lp = netdev_priv(dev);

  printk(KERN_DEBUG "local:");
  /*
   * Not implemented yet...
   */
  printk("\n");
} /* wv_local_show */
#endif	/* DEBUG_DEVICE_SHOW */

#if defined(DEBUG_RX_INFO) || defined(DEBUG_TX_INFO)
/*------------------------------------------------------------------*/
/*
 * Dump packet header (and content if necessary) on the screen
 */
static void
wv_packet_info(u_char *		p,		/* Packet to dump */
	       int		length,		/* Length of the packet */
	       char *		msg1,		/* Name of the device */
	       char *		msg2)		/* Name of the function */
{
  int		i;
  int		maxi;

  printk(KERN_DEBUG "%s: %s(): dest %pM, length %d\n",
	 msg1, msg2, p, length);
  printk(KERN_DEBUG "%s: %s(): src %pM, type 0x%02X%02X\n",
	 msg1, msg2, &p[6], p[12], p[13]);

#ifdef DEBUG_PACKET_DUMP

  printk(KERN_DEBUG "data=\"");

  if((maxi = length) > DEBUG_PACKET_DUMP)
    maxi = DEBUG_PACKET_DUMP;
  for(i = 14; i < maxi; i++)
    if(p[i] >= ' ' && p[i] <= '~')
      printk(" %c", p[i]);
    else
      printk("%02X", p[i]);
  if(maxi < length)
    printk("..");
  printk("\"\n");
  printk(KERN_DEBUG "\n");
#endif	/* DEBUG_PACKET_DUMP */
}
#endif	/* defined(DEBUG_RX_INFO) || defined(DEBUG_TX_INFO) */

/*------------------------------------------------------------------*/
/*
 * This is the information which is displayed by the driver at startup
 * There  is a lot of flag to configure it at your will...
 */
static void
wv_init_info(struct net_device *	dev)
{
  unsigned int	base = dev->base_addr;
  psa_t		psa;

  /* Read the parameter storage area */
  psa_read(dev, 0, (unsigned char *) &psa, sizeof(psa));

#ifdef DEBUG_PSA_SHOW
  wv_psa_show(&psa);
#endif
#ifdef DEBUG_MMC_SHOW
  wv_mmc_show(dev);
#endif
#ifdef DEBUG_I82593_SHOW
  wv_ru_show(dev);
#endif

#ifdef DEBUG_BASIC_SHOW
  /* Now, let's go for the basic stuff */
  printk(KERN_NOTICE "%s: WaveLAN: port %#x, irq %d, hw_addr %pM",
	 dev->name, base, dev->irq, dev->dev_addr);

  /* Print current network id */
  if(psa.psa_nwid_select)
    printk(", nwid 0x%02X-%02X", psa.psa_nwid[0], psa.psa_nwid[1]);
  else
    printk(", nwid off");

  /* If 2.00 card */
  if(!(mmc_in(base, mmroff(0, mmr_fee_status)) &
       (MMR_FEE_STATUS_DWLD | MMR_FEE_STATUS_BUSY)))
    {
      unsigned short	freq;

      /* Ask the EEprom to read the frequency from the first area */
      fee_read(base, 0x00 /* 1st area - frequency... */,
	       &freq, 1);

      /* Print frequency */
      printk(", 2.00, %ld", (freq >> 6) + 2400L);

      /* Hack !!! */
      if(freq & 0x20)
	printk(".5");
    }
  else
    {
      printk(", PCMCIA, ");
      switch (psa.psa_subband)
	{
	case PSA_SUBBAND_915:
	  printk("915");
	  break;
	case PSA_SUBBAND_2425:
	  printk("2425");
	  break;
	case PSA_SUBBAND_2460:
	  printk("2460");
	  break;
	case PSA_SUBBAND_2484:
	  printk("2484");
	  break;
	case PSA_SUBBAND_2430_5:
	  printk("2430.5");
	  break;
	default:
	  printk("unknown");
	}
    }

  printk(" MHz\n");
#endif	/* DEBUG_BASIC_SHOW */

#ifdef DEBUG_VERSION_SHOW
  /* Print version information */
  printk(KERN_NOTICE "%s", version);
#endif
} /* wv_init_info */

/********************* IOCTL, STATS & RECONFIG *********************/
/*
 * We found here routines that are called by Linux on differents
 * occasions after the configuration and not for transmitting data
 * These may be called when the user use ifconfig, /proc/net/dev
 * or wireless extensions
 */


/*------------------------------------------------------------------*/
/*
 * Set or clear the multicast filter for this adaptor.
 * num_addrs == -1	Promiscuous mode, receive all packets
 * num_addrs == 0	Normal mode, clear multicast list
 * num_addrs > 0	Multicast mode, receive normal and MC packets,
 *			and do best-effort filtering.
 */

static void
wavelan_set_multicast_list(struct net_device *	dev)
{
  net_local *	lp = netdev_priv(dev);

#ifdef DEBUG_IOCTL_TRACE
  printk(KERN_DEBUG "%s: ->wavelan_set_multicast_list()\n", dev->name);
#endif

#ifdef DEBUG_IOCTL_INFO
  printk(KERN_DEBUG "%s: wavelan_set_multicast_list(): setting Rx mode %02X to %d addresses.\n",
	 dev->name, dev->flags, dev->mc_count);
#endif

  if(dev->flags & IFF_PROMISC)
    {
      /*
       * Enable promiscuous mode: receive all packets.
       */
      if(!lp->promiscuous)
	{
	  lp->promiscuous = 1;
	  lp->allmulticast = 0;
	  lp->mc_count = 0;

	  wv_82593_reconfig(dev);
	}
    }
  else
    /* If all multicast addresses
     * or too much multicast addresses for the hardware filter */
    if((dev->flags & IFF_ALLMULTI) ||
       (dev->mc_count > I82593_MAX_MULTICAST_ADDRESSES))
      {
	/*
	 * Disable promiscuous mode, but active the all multicast mode
	 */
	if(!lp->allmulticast)
	  {
	    lp->promiscuous = 0;
	    lp->allmulticast = 1;
	    lp->mc_count = 0;

	    wv_82593_reconfig(dev);
	  }
      }
    else
      /* If there is some multicast addresses to send */
      if(dev->mc_list != (struct dev_mc_list *) NULL)
	{
	  /*
	   * Disable promiscuous mode, but receive all packets
	   * in multicast list
	   */
#ifdef MULTICAST_AVOID
	  if(lp->promiscuous || lp->allmulticast ||
	     (dev->mc_count != lp->mc_count))
#endif
	    {
	      lp->promiscuous = 0;
	      lp->allmulticast = 0;
	      lp->mc_count = dev->mc_count;

	      wv_82593_reconfig(dev);
	    }
	}
      else
	{
	  /*
	   * Switch to normal mode: disable promiscuous mode and 
	   * clear the multicast list.
	   */
	  if(lp->promiscuous || lp->mc_count == 0)
	    {
	      lp->promiscuous = 0;
	      lp->allmulticast = 0;
	      lp->mc_count = 0;

	      wv_82593_reconfig(dev);
	    }
	}
#ifdef DEBUG_IOCTL_TRACE
  printk(KERN_DEBUG "%s: <-wavelan_set_multicast_list()\n", dev->name);
#endif
}

/*------------------------------------------------------------------*/
/*
 * This function doesn't exist...
 * (Note : it was a nice way to test the reconfigure stuff...)
 */
#ifdef SET_MAC_ADDRESS
static int
wavelan_set_mac_address(struct net_device *	dev,
			void *		addr)
{
  struct sockaddr *	mac = addr;

  /* Copy the address */
  memcpy(dev->dev_addr, mac->sa_data, WAVELAN_ADDR_SIZE);

  /* Reconfig the beast */
  wv_82593_reconfig(dev);

  return 0;
}
#endif	/* SET_MAC_ADDRESS */


/*------------------------------------------------------------------*/
/*
 * Frequency setting (for hardware able of it)
 * It's a bit complicated and you don't really want to look into it...
 */
static int
wv_set_frequency(u_long		base,	/* i/o port of the card */
		 iw_freq *	frequency)
{
  const int	BAND_NUM = 10;	/* Number of bands */
  long		freq = 0L;	/* offset to 2.4 GHz in .5 MHz */
#ifdef DEBUG_IOCTL_INFO
  int		i;
#endif

  /* Setting by frequency */
  /* Theoritically, you may set any frequency between
   * the two limits with a 0.5 MHz precision. In practice,
   * I don't want you to have trouble with local
   * regulations... */
  if((frequency->e == 1) &&
     (frequency->m >= (int) 2.412e8) && (frequency->m <= (int) 2.487e8))
    {
      freq = ((frequency->m / 10000) - 24000L) / 5;
    }

  /* Setting by channel (same as wfreqsel) */
  /* Warning : each channel is 22MHz wide, so some of the channels
   * will interfere... */
  if((frequency->e == 0) &&
     (frequency->m >= 0) && (frequency->m < BAND_NUM))
    {
      /* Get frequency offset. */
      freq = channel_bands[frequency->m] >> 1;
    }

  /* Verify if the frequency is allowed */
  if(freq != 0L)
    {
      u_short	table[10];	/* Authorized frequency table */

      /* Read the frequency table */
      fee_read(base, 0x71 /* frequency table */,
	       table, 10);

#ifdef DEBUG_IOCTL_INFO
      printk(KERN_DEBUG "Frequency table :");
      for(i = 0; i < 10; i++)
	{
	  printk(" %04X",
		 table[i]);
	}
      printk("\n");
#endif

      /* Look in the table if the frequency is allowed */
      if(!(table[9 - ((freq - 24) / 16)] &
	   (1 << ((freq - 24) % 16))))
	return -EINVAL;		/* not allowed */
    }
  else
    return -EINVAL;

  /* If we get a usable frequency */
  if(freq != 0L)
    {
      unsigned short	area[16];
      unsigned short	dac[2];
      unsigned short	area_verify[16];
      unsigned short	dac_verify[2];
      /* Corresponding gain (in the power adjust value table)
       * see AT&T Wavelan Data Manual, REF 407-024689/E, page 3-8
       * & WCIN062D.DOC, page 6.2.9 */
      unsigned short	power_limit[] = { 40, 80, 120, 160, 0 };
      int		power_band = 0;		/* Selected band */
      unsigned short	power_adjust;		/* Correct value */

      /* Search for the gain */
      power_band = 0;
      while((freq > power_limit[power_band]) &&
	    (power_limit[++power_band] != 0))
	;

      /* Read the first area */
      fee_read(base, 0x00,
	       area, 16);

      /* Read the DAC */
      fee_read(base, 0x60,
	       dac, 2);

      /* Read the new power adjust value */
      fee_read(base, 0x6B - (power_band >> 1),
	       &power_adjust, 1);
      if(power_band & 0x1)
	power_adjust >>= 8;
      else
	power_adjust &= 0xFF;

#ifdef DEBUG_IOCTL_INFO
      printk(KERN_DEBUG "Wavelan EEprom Area 1 :");
      for(i = 0; i < 16; i++)
	{
	  printk(" %04X",
		 area[i]);
	}
      printk("\n");

      printk(KERN_DEBUG "Wavelan EEprom DAC : %04X %04X\n",
	     dac[0], dac[1]);
#endif

      /* Frequency offset (for info only...) */
      area[0] = ((freq << 5) & 0xFFE0) | (area[0] & 0x1F);

      /* Receiver Principle main divider coefficient */
      area[3] = (freq >> 1) + 2400L - 352L;
      area[2] = ((freq & 0x1) << 4) | (area[2] & 0xFFEF);

      /* Transmitter Main divider coefficient */
      area[13] = (freq >> 1) + 2400L;
      area[12] = ((freq & 0x1) << 4) | (area[2] & 0xFFEF);

      /* Others part of the area are flags, bit streams or unused... */

      /* Set the value in the DAC */
      dac[1] = ((power_adjust >> 1) & 0x7F) | (dac[1] & 0xFF80);
      dac[0] = ((power_adjust & 0x1) << 4) | (dac[0] & 0xFFEF);

      /* Write the first area */
      fee_write(base, 0x00,
		area, 16);

      /* Write the DAC */
      fee_write(base, 0x60,
		dac, 2);

      /* We now should verify here that the EEprom writing was ok */

      /* ReRead the first area */
      fee_read(base, 0x00,
	       area_verify, 16);

      /* ReRead the DAC */
      fee_read(base, 0x60,
	       dac_verify, 2);

      /* Compare */
      if(memcmp(area, area_verify, 16 * 2) ||
	 memcmp(dac, dac_verify, 2 * 2))
	{
#ifdef DEBUG_IOCTL_ERROR
	  printk(KERN_INFO "Wavelan: wv_set_frequency : unable to write new frequency to EEprom (?)\n");
#endif
	  return -EOPNOTSUPP;
	}

      /* We must download the frequency parameters to the
       * synthetisers (from the EEprom - area 1)
       * Note : as the EEprom is auto decremented, we set the end
       * if the area... */
      mmc_out(base, mmwoff(0, mmw_fee_addr), 0x0F);
      mmc_out(base, mmwoff(0, mmw_fee_ctrl),
	      MMW_FEE_CTRL_READ | MMW_FEE_CTRL_DWLD);

      /* Wait until the download is finished */
      fee_wait(base, 100, 100);

      /* We must now download the power adjust value (gain) to
       * the synthetisers (from the EEprom - area 7 - DAC) */
      mmc_out(base, mmwoff(0, mmw_fee_addr), 0x61);
      mmc_out(base, mmwoff(0, mmw_fee_ctrl),
	      MMW_FEE_CTRL_READ | MMW_FEE_CTRL_DWLD);

      /* Wait until the download is finished */
      fee_wait(base, 100, 100);

#ifdef DEBUG_IOCTL_INFO
      /* Verification of what we have done... */

      printk(KERN_DEBUG "Wavelan EEprom Area 1 :");
      for(i = 0; i < 16; i++)
	{
	  printk(" %04X",
		 area_verify[i]);
	}
      printk("\n");

      printk(KERN_DEBUG "Wavelan EEprom DAC : %04X %04X\n",
	     dac_verify[0], dac_verify[1]);
#endif

      return 0;
    }
  else
    return -EINVAL;		/* Bah, never get there... */
}

/*------------------------------------------------------------------*/
/*
 * Give the list of available frequencies
 */
static int
wv_frequency_list(u_long	base,	/* i/o port of the card */
		  iw_freq *	list,	/* List of frequency to fill */
		  int		max)	/* Maximum number of frequencies */
{
  u_short	table[10];	/* Authorized frequency table */
  long		freq = 0L;	/* offset to 2.4 GHz in .5 MHz + 12 MHz */
  int		i;		/* index in the table */
  const int	BAND_NUM = 10;	/* Number of bands */
  int		c = 0;		/* Channel number */

  /* Read the frequency table */
  fee_read(base, 0x71 /* frequency table */,
	   table, 10);

  /* Look all frequencies */
  i = 0;
  for(freq = 0; freq < 150; freq++)
    /* Look in the table if the frequency is allowed */
    if(table[9 - (freq / 16)] & (1 << (freq % 16)))
      {
	/* Compute approximate channel number */
	while((((channel_bands[c] >> 1) - 24) < freq) &&
	      (c < BAND_NUM))
	  c++;
	list[i].i = c;	/* Set the list index */

	/* put in the list */
	list[i].m = (((freq + 24) * 5) + 24000L) * 10000;
	list[i++].e = 1;

	/* Check number */
	if(i >= max)
	  return(i);
      }

  return(i);
}

#ifdef IW_WIRELESS_SPY
/*------------------------------------------------------------------*/
/*
 * Gather wireless spy statistics : for each packet, compare the source
 * address with out list, and if match, get the stats...
 * Sorry, but this function really need wireless extensions...
 */
static inline void
wl_spy_gather(struct net_device *	dev,
	      u_char *	mac,		/* MAC address */
	      u_char *	stats)		/* Statistics to gather */
{
  struct iw_quality wstats;

  wstats.qual = stats[2] & MMR_SGNL_QUAL;
  wstats.level = stats[0] & MMR_SIGNAL_LVL;
  wstats.noise = stats[1] & MMR_SILENCE_LVL;
  wstats.updated = 0x7;

  /* Update spy records */
  wireless_spy_update(dev, mac, &wstats);
}
#endif	/* IW_WIRELESS_SPY */

#ifdef HISTOGRAM
/*------------------------------------------------------------------*/
/*
 * This function calculate an histogram on the signal level.
 * As the noise is quite constant, it's like doing it on the SNR.
 * We have defined a set of interval (lp->his_range), and each time
 * the level goes in that interval, we increment the count (lp->his_sum).
 * With this histogram you may detect if one wavelan is really weak,
 * or you may also calculate the mean and standard deviation of the level...
 */
static inline void
wl_his_gather(struct net_device *	dev,
	      u_char *	stats)		/* Statistics to gather */
{
  net_local *	lp = netdev_priv(dev);
  u_char	level = stats[0] & MMR_SIGNAL_LVL;
  int		i;

  /* Find the correct interval */
  i = 0;
  while((i < (lp->his_number - 1)) && (level >= lp->his_range[i++]))
    ;

  /* Increment interval counter */
  (lp->his_sum[i])++;
}
#endif	/* HISTOGRAM */

static void wl_get_drvinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
	strncpy(info->driver, "wavelan_cs", sizeof(info->driver)-1);
}

static const struct ethtool_ops ops = {
	.get_drvinfo = wl_get_drvinfo
};

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get protocol name
 */
static int wavelan_get_name(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu,
			    char *extra)
{
	strcpy(wrqu->name, "WaveLAN");
	return 0;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : set NWID
 */
static int wavelan_set_nwid(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu,
			    char *extra)
{
	unsigned int base = dev->base_addr;
	net_local *lp = netdev_priv(dev);
	psa_t psa;
	mm_t m;
	unsigned long flags;
	int ret = 0;

	/* Disable interrupts and save flags. */
	spin_lock_irqsave(&lp->spinlock, flags);
	
	/* Set NWID in WaveLAN. */
	if (!wrqu->nwid.disabled) {
		/* Set NWID in psa */
		psa.psa_nwid[0] = (wrqu->nwid.value & 0xFF00) >> 8;
		psa.psa_nwid[1] = wrqu->nwid.value & 0xFF;
		psa.psa_nwid_select = 0x01;
		psa_write(dev,
			  (char *) psa.psa_nwid - (char *) &psa,
			  (unsigned char *) psa.psa_nwid, 3);

		/* Set NWID in mmc. */
		m.w.mmw_netw_id_l = psa.psa_nwid[1];
		m.w.mmw_netw_id_h = psa.psa_nwid[0];
		mmc_write(base,
			  (char *) &m.w.mmw_netw_id_l -
			  (char *) &m,
			  (unsigned char *) &m.w.mmw_netw_id_l, 2);
		mmc_out(base, mmwoff(0, mmw_loopt_sel), 0x00);
	} else {
		/* Disable NWID in the psa. */
		psa.psa_nwid_select = 0x00;
		psa_write(dev,
			  (char *) &psa.psa_nwid_select -
			  (char *) &psa,
			  (unsigned char *) &psa.psa_nwid_select,
			  1);

		/* Disable NWID in the mmc (no filtering). */
		mmc_out(base, mmwoff(0, mmw_loopt_sel),
			MMW_LOOPT_SEL_DIS_NWID);
	}
	/* update the Wavelan checksum */
	update_psa_checksum(dev);

	/* Enable interrupts and restore flags. */
	spin_unlock_irqrestore(&lp->spinlock, flags);

	return ret;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get NWID 
 */
static int wavelan_get_nwid(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu,
			    char *extra)
{
	net_local *lp = netdev_priv(dev);
	psa_t psa;
	unsigned long flags;
	int ret = 0;

	/* Disable interrupts and save flags. */
	spin_lock_irqsave(&lp->spinlock, flags);
	
	/* Read the NWID. */
	psa_read(dev,
		 (char *) psa.psa_nwid - (char *) &psa,
		 (unsigned char *) psa.psa_nwid, 3);
	wrqu->nwid.value = (psa.psa_nwid[0] << 8) + psa.psa_nwid[1];
	wrqu->nwid.disabled = !(psa.psa_nwid_select);
	wrqu->nwid.fixed = 1;	/* Superfluous */

	/* Enable interrupts and restore flags. */
	spin_unlock_irqrestore(&lp->spinlock, flags);

	return ret;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : set frequency
 */
static int wavelan_set_freq(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu,
			    char *extra)
{
	unsigned int base = dev->base_addr;
	net_local *lp = netdev_priv(dev);
	unsigned long flags;
	int ret;

	/* Disable interrupts and save flags. */
	spin_lock_irqsave(&lp->spinlock, flags);
	
	/* Attempt to recognise 2.00 cards (2.4 GHz frequency selectable). */
	if (!(mmc_in(base, mmroff(0, mmr_fee_status)) &
	      (MMR_FEE_STATUS_DWLD | MMR_FEE_STATUS_BUSY)))
		ret = wv_set_frequency(base, &(wrqu->freq));
	else
		ret = -EOPNOTSUPP;

	/* Enable interrupts and restore flags. */
	spin_unlock_irqrestore(&lp->spinlock, flags);

	return ret;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get frequency
 */
static int wavelan_get_freq(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu,
			    char *extra)
{
	unsigned int base = dev->base_addr;
	net_local *lp = netdev_priv(dev);
	psa_t psa;
	unsigned long flags;
	int ret = 0;

	/* Disable interrupts and save flags. */
	spin_lock_irqsave(&lp->spinlock, flags);
	
	/* Attempt to recognise 2.00 cards (2.4 GHz frequency selectable).
	 * Does it work for everybody, especially old cards? */
	if (!(mmc_in(base, mmroff(0, mmr_fee_status)) &
	      (MMR_FEE_STATUS_DWLD | MMR_FEE_STATUS_BUSY))) {
		unsigned short freq;

		/* Ask the EEPROM to read the frequency from the first area. */
		fee_read(base, 0x00, &freq, 1);
		wrqu->freq.m = ((freq >> 5) * 5 + 24000L) * 10000;
		wrqu->freq.e = 1;
	} else {
		psa_read(dev,
			 (char *) &psa.psa_subband - (char *) &psa,
			 (unsigned char *) &psa.psa_subband, 1);

		if (psa.psa_subband <= 4) {
			wrqu->freq.m = fixed_bands[psa.psa_subband];
			wrqu->freq.e = (psa.psa_subband != 0);
		} else
			ret = -EOPNOTSUPP;
	}

	/* Enable interrupts and restore flags. */
	spin_unlock_irqrestore(&lp->spinlock, flags);

	return ret;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : set level threshold
 */
static int wavelan_set_sens(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu,
			    char *extra)
{
	unsigned int base = dev->base_addr;
	net_local *lp = netdev_priv(dev);
	psa_t psa;
	unsigned long flags;
	int ret = 0;

	/* Disable interrupts and save flags. */
	spin_lock_irqsave(&lp->spinlock, flags);
	
	/* Set the level threshold. */
	/* We should complain loudly if wrqu->sens.fixed = 0, because we
	 * can't set auto mode... */
	psa.psa_thr_pre_set = wrqu->sens.value & 0x3F;
	psa_write(dev,
		  (char *) &psa.psa_thr_pre_set - (char *) &psa,
		  (unsigned char *) &psa.psa_thr_pre_set, 1);
	/* update the Wavelan checksum */
	update_psa_checksum(dev);
	mmc_out(base, mmwoff(0, mmw_thr_pre_set),
		psa.psa_thr_pre_set);

	/* Enable interrupts and restore flags. */
	spin_unlock_irqrestore(&lp->spinlock, flags);

	return ret;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get level threshold
 */
static int wavelan_get_sens(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu,
			    char *extra)
{
	net_local *lp = netdev_priv(dev);
	psa_t psa;
	unsigned long flags;
	int ret = 0;

	/* Disable interrupts and save flags. */
	spin_lock_irqsave(&lp->spinlock, flags);
	
	/* Read the level threshold. */
	psa_read(dev,
		 (char *) &psa.psa_thr_pre_set - (char *) &psa,
		 (unsigned char *) &psa.psa_thr_pre_set, 1);
	wrqu->sens.value = psa.psa_thr_pre_set & 0x3F;
	wrqu->sens.fixed = 1;

	/* Enable interrupts and restore flags. */
	spin_unlock_irqrestore(&lp->spinlock, flags);

	return ret;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : set encryption key
 */
static int wavelan_set_encode(struct net_device *dev,
			      struct iw_request_info *info,
			      union iwreq_data *wrqu,
			      char *extra)
{
	unsigned int base = dev->base_addr;
	net_local *lp = netdev_priv(dev);
	unsigned long flags;
	psa_t psa;
	int ret = 0;

	/* Disable interrupts and save flags. */
	spin_lock_irqsave(&lp->spinlock, flags);

	/* Check if capable of encryption */
	if (!mmc_encr(base)) {
		ret = -EOPNOTSUPP;
	}

	/* Check the size of the key */
	if((wrqu->encoding.length != 8) && (wrqu->encoding.length != 0)) {
		ret = -EINVAL;
	}

	if(!ret) {
		/* Basic checking... */
		if (wrqu->encoding.length == 8) {
			/* Copy the key in the driver */
			memcpy(psa.psa_encryption_key, extra,
			       wrqu->encoding.length);
			psa.psa_encryption_select = 1;

			psa_write(dev,
				  (char *) &psa.psa_encryption_select -
				  (char *) &psa,
				  (unsigned char *) &psa.
				  psa_encryption_select, 8 + 1);

			mmc_out(base, mmwoff(0, mmw_encr_enable),
				MMW_ENCR_ENABLE_EN | MMW_ENCR_ENABLE_MODE);
			mmc_write(base, mmwoff(0, mmw_encr_key),
				  (unsigned char *) &psa.
				  psa_encryption_key, 8);
		}

		/* disable encryption */
		if (wrqu->encoding.flags & IW_ENCODE_DISABLED) {
			psa.psa_encryption_select = 0;
			psa_write(dev,
				  (char *) &psa.psa_encryption_select -
				  (char *) &psa,
				  (unsigned char *) &psa.
				  psa_encryption_select, 1);

			mmc_out(base, mmwoff(0, mmw_encr_enable), 0);
		}
		/* update the Wavelan checksum */
		update_psa_checksum(dev);
	}

	/* Enable interrupts and restore flags. */
	spin_unlock_irqrestore(&lp->spinlock, flags);

	return ret;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get encryption key
 */
static int wavelan_get_encode(struct net_device *dev,
			      struct iw_request_info *info,
			      union iwreq_data *wrqu,
			      char *extra)
{
	unsigned int base = dev->base_addr;
	net_local *lp = netdev_priv(dev);
	psa_t psa;
	unsigned long flags;
	int ret = 0;

	/* Disable interrupts and save flags. */
	spin_lock_irqsave(&lp->spinlock, flags);
	
	/* Check if encryption is available */
	if (!mmc_encr(base)) {
		ret = -EOPNOTSUPP;
	} else {
		/* Read the encryption key */
		psa_read(dev,
			 (char *) &psa.psa_encryption_select -
			 (char *) &psa,
			 (unsigned char *) &psa.
			 psa_encryption_select, 1 + 8);

		/* encryption is enabled ? */
		if (psa.psa_encryption_select)
			wrqu->encoding.flags = IW_ENCODE_ENABLED;
		else
			wrqu->encoding.flags = IW_ENCODE_DISABLED;
		wrqu->encoding.flags |= mmc_encr(base);

		/* Copy the key to the user buffer */
		wrqu->encoding.length = 8;
		memcpy(extra, psa.psa_encryption_key, wrqu->encoding.length);
	}

	/* Enable interrupts and restore flags. */
	spin_unlock_irqrestore(&lp->spinlock, flags);

	return ret;
}

#ifdef WAVELAN_ROAMING_EXT
/*------------------------------------------------------------------*/
/*
 * Wireless Handler : set ESSID (domain)
 */
static int wavelan_set_essid(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu,
			     char *extra)
{
	net_local *lp = netdev_priv(dev);
	unsigned long flags;
	int ret = 0;

	/* Disable interrupts and save flags. */
	spin_lock_irqsave(&lp->spinlock, flags);
	
	/* Check if disable */
	if(wrqu->data.flags == 0)
		lp->filter_domains = 0;
	else {
		char	essid[IW_ESSID_MAX_SIZE + 1];
		char *	endp;

		/* Terminate the string */
		memcpy(essid, extra, wrqu->data.length);
		essid[IW_ESSID_MAX_SIZE] = '\0';

#ifdef DEBUG_IOCTL_INFO
		printk(KERN_DEBUG "SetEssid : ``%s''\n", essid);
#endif	/* DEBUG_IOCTL_INFO */

		/* Convert to a number (note : Wavelan specific) */
		lp->domain_id = simple_strtoul(essid, &endp, 16);
		/* Has it worked  ? */
		if(endp > essid)
			lp->filter_domains = 1;
		else {
			lp->filter_domains = 0;
			ret = -EINVAL;
		}
	}

	/* Enable interrupts and restore flags. */
	spin_unlock_irqrestore(&lp->spinlock, flags);

	return ret;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get ESSID (domain)
 */
static int wavelan_get_essid(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu,
			     char *extra)
{
	net_local *lp = netdev_priv(dev);

	/* Is the domain ID active ? */
	wrqu->data.flags = lp->filter_domains;

	/* Copy Domain ID into a string (Wavelan specific) */
	/* Sound crazy, be we can't have a snprintf in the kernel !!! */
	sprintf(extra, "%lX", lp->domain_id);
	extra[IW_ESSID_MAX_SIZE] = '\0';

	/* Set the length */
	wrqu->data.length = strlen(extra);

	return 0;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : set AP address
 */
static int wavelan_set_wap(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu,
			   char *extra)
{
#ifdef DEBUG_IOCTL_INFO
	printk(KERN_DEBUG "Set AP to : %pM\n", wrqu->ap_addr.sa_data);
#endif	/* DEBUG_IOCTL_INFO */

	return -EOPNOTSUPP;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get AP address
 */
static int wavelan_get_wap(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu,
			   char *extra)
{
	/* Should get the real McCoy instead of own Ethernet address */
	memcpy(wrqu->ap_addr.sa_data, dev->dev_addr, WAVELAN_ADDR_SIZE);
	wrqu->ap_addr.sa_family = ARPHRD_ETHER;

	return -EOPNOTSUPP;
}
#endif	/* WAVELAN_ROAMING_EXT */

#ifdef WAVELAN_ROAMING
/*------------------------------------------------------------------*/
/*
 * Wireless Handler : set mode
 */
static int wavelan_set_mode(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu,
			    char *extra)
{
	net_local *lp = netdev_priv(dev);
	unsigned long flags;
	int ret = 0;

	/* Disable interrupts and save flags. */
	spin_lock_irqsave(&lp->spinlock, flags);

	/* Check mode */
	switch(wrqu->mode) {
	case IW_MODE_ADHOC:
		if(do_roaming) {
			wv_roam_cleanup(dev);
			do_roaming = 0;
		}
		break;
	case IW_MODE_INFRA:
		if(!do_roaming) {
			wv_roam_init(dev);
			do_roaming = 1;
		}
		break;
	default:
		ret = -EINVAL;
	}

	/* Enable interrupts and restore flags. */
	spin_unlock_irqrestore(&lp->spinlock, flags);

	return ret;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get mode
 */
static int wavelan_get_mode(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu,
			    char *extra)
{
	if(do_roaming)
		wrqu->mode = IW_MODE_INFRA;
	else
		wrqu->mode = IW_MODE_ADHOC;

	return 0;
}
#endif	/* WAVELAN_ROAMING */

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get range info
 */
static int wavelan_get_range(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu,
			     char *extra)
{
	unsigned int base = dev->base_addr;
	net_local *lp = netdev_priv(dev);
	struct iw_range *range = (struct iw_range *) extra;
	unsigned long flags;
	int ret = 0;

	/* Set the length (very important for backward compatibility) */
	wrqu->data.length = sizeof(struct iw_range);

	/* Set all the info we don't care or don't know about to zero */
	memset(range, 0, sizeof(struct iw_range));

	/* Set the Wireless Extension versions */
	range->we_version_compiled = WIRELESS_EXT;
	range->we_version_source = 9;

	/* Set information in the range struct.  */
	range->throughput = 1.4 * 1000 * 1000;	/* don't argue on this ! */
	range->min_nwid = 0x0000;
	range->max_nwid = 0xFFFF;

	range->sensitivity = 0x3F;
	range->max_qual.qual = MMR_SGNL_QUAL;
	range->max_qual.level = MMR_SIGNAL_LVL;
	range->max_qual.noise = MMR_SILENCE_LVL;
	range->avg_qual.qual = MMR_SGNL_QUAL; /* Always max */
	/* Need to get better values for those two */
	range->avg_qual.level = 30;
	range->avg_qual.noise = 8;

	range->num_bitrates = 1;
	range->bitrate[0] = 2000000;	/* 2 Mb/s */

	/* Event capability (kernel + driver) */
	range->event_capa[0] = (IW_EVENT_CAPA_MASK(0x8B02) |
				IW_EVENT_CAPA_MASK(0x8B04) |
				IW_EVENT_CAPA_MASK(0x8B06));
	range->event_capa[1] = IW_EVENT_CAPA_K_1;

	/* Disable interrupts and save flags. */
	spin_lock_irqsave(&lp->spinlock, flags);
	
	/* Attempt to recognise 2.00 cards (2.4 GHz frequency selectable). */
	if (!(mmc_in(base, mmroff(0, mmr_fee_status)) &
	      (MMR_FEE_STATUS_DWLD | MMR_FEE_STATUS_BUSY))) {
		range->num_channels = 10;
		range->num_frequency = wv_frequency_list(base, range->freq,
							IW_MAX_FREQUENCIES);
	} else
		range->num_channels = range->num_frequency = 0;

	/* Encryption supported ? */
	if (mmc_encr(base)) {
		range->encoding_size[0] = 8;	/* DES = 64 bits key */
		range->num_encoding_sizes = 1;
		range->max_encoding_tokens = 1;	/* Only one key possible */
	} else {
		range->num_encoding_sizes = 0;
		range->max_encoding_tokens = 0;
	}

	/* Enable interrupts and restore flags. */
	spin_unlock_irqrestore(&lp->spinlock, flags);

	return ret;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Private Handler : set quality threshold
 */
static int wavelan_set_qthr(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu,
			    char *extra)
{
	unsigned int base = dev->base_addr;
	net_local *lp = netdev_priv(dev);
	psa_t psa;
	unsigned long flags;

	/* Disable interrupts and save flags. */
	spin_lock_irqsave(&lp->spinlock, flags);
	
	psa.psa_quality_thr = *(extra) & 0x0F;
	psa_write(dev,
		  (char *) &psa.psa_quality_thr - (char *) &psa,
		  (unsigned char *) &psa.psa_quality_thr, 1);
	/* update the Wavelan checksum */
	update_psa_checksum(dev);
	mmc_out(base, mmwoff(0, mmw_quality_thr),
		psa.psa_quality_thr);

	/* Enable interrupts and restore flags. */
	spin_unlock_irqrestore(&lp->spinlock, flags);

	return 0;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Private Handler : get quality threshold
 */
static int wavelan_get_qthr(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu,
			    char *extra)
{
	net_local *lp = netdev_priv(dev);
	psa_t psa;
	unsigned long flags;

	/* Disable interrupts and save flags. */
	spin_lock_irqsave(&lp->spinlock, flags);
	
	psa_read(dev,
		 (char *) &psa.psa_quality_thr - (char *) &psa,
		 (unsigned char *) &psa.psa_quality_thr, 1);
	*(extra) = psa.psa_quality_thr & 0x0F;

	/* Enable interrupts and restore flags. */
	spin_unlock_irqrestore(&lp->spinlock, flags);

	return 0;
}

#ifdef WAVELAN_ROAMING
/*------------------------------------------------------------------*/
/*
 * Wireless Private Handler : set roaming
 */
static int wavelan_set_roam(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu,
			    char *extra)
{
	net_local *lp = netdev_priv(dev);
	unsigned long flags;

	/* Disable interrupts and save flags. */
	spin_lock_irqsave(&lp->spinlock, flags);
	
	/* Note : should check if user == root */
	if(do_roaming && (*extra)==0)
		wv_roam_cleanup(dev);
	else if(do_roaming==0 && (*extra)!=0)
		wv_roam_init(dev);

	do_roaming = (*extra);

	/* Enable interrupts and restore flags. */
	spin_unlock_irqrestore(&lp->spinlock, flags);

	return 0;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Private Handler : get quality threshold
 */
static int wavelan_get_roam(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu,
			    char *extra)
{
	*(extra) = do_roaming;

	return 0;
}
#endif	/* WAVELAN_ROAMING */

#ifdef HISTOGRAM
/*------------------------------------------------------------------*/
/*
 * Wireless Private Handler : set histogram
 */
static int wavelan_set_histo(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu,
			     char *extra)
{
	net_local *lp = netdev_priv(dev);

	/* Check the number of intervals. */
	if (wrqu->data.length > 16) {
		return(-E2BIG);
	}

	/* Disable histo while we copy the addresses.
	 * As we don't disable interrupts, we need to do this */
	lp->his_number = 0;

	/* Are there ranges to copy? */
	if (wrqu->data.length > 0) {
		/* Copy interval ranges to the driver */
		memcpy(lp->his_range, extra, wrqu->data.length);

		{
		  int i;
		  printk(KERN_DEBUG "Histo :");
		  for(i = 0; i < wrqu->data.length; i++)
		    printk(" %d", lp->his_range[i]);
		  printk("\n");
		}

		/* Reset result structure. */
		memset(lp->his_sum, 0x00, sizeof(long) * 16);
	}

	/* Now we can set the number of ranges */
	lp->his_number = wrqu->data.length;

	return(0);
}

/*------------------------------------------------------------------*/
/*
 * Wireless Private Handler : get histogram
 */
static int wavelan_get_histo(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu,
			     char *extra)
{
	net_local *lp = netdev_priv(dev);

	/* Set the number of intervals. */
	wrqu->data.length = lp->his_number;

	/* Give back the distribution statistics */
	if(lp->his_number > 0)
		memcpy(extra, lp->his_sum, sizeof(long) * lp->his_number);

	return(0);
}
#endif			/* HISTOGRAM */

/*------------------------------------------------------------------*/
/*
 * Structures to export the Wireless Handlers
 */

static const struct iw_priv_args wavelan_private_args[] = {
/*{ cmd,         set_args,                            get_args, name } */
  { SIOCSIPQTHR, IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1, 0, "setqualthr" },
  { SIOCGIPQTHR, 0, IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1, "getqualthr" },
  { SIOCSIPROAM, IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1, 0, "setroam" },
  { SIOCGIPROAM, 0, IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1, "getroam" },
  { SIOCSIPHISTO, IW_PRIV_TYPE_BYTE | 16,                    0, "sethisto" },
  { SIOCGIPHISTO, 0,                     IW_PRIV_TYPE_INT | 16, "gethisto" },
};

static const iw_handler		wavelan_handler[] =
{
	NULL,				/* SIOCSIWNAME */
	wavelan_get_name,		/* SIOCGIWNAME */
	wavelan_set_nwid,		/* SIOCSIWNWID */
	wavelan_get_nwid,		/* SIOCGIWNWID */
	wavelan_set_freq,		/* SIOCSIWFREQ */
	wavelan_get_freq,		/* SIOCGIWFREQ */
#ifdef WAVELAN_ROAMING
	wavelan_set_mode,		/* SIOCSIWMODE */
	wavelan_get_mode,		/* SIOCGIWMODE */
#else	/* WAVELAN_ROAMING */
	NULL,				/* SIOCSIWMODE */
	NULL,				/* SIOCGIWMODE */
#endif	/* WAVELAN_ROAMING */
	wavelan_set_sens,		/* SIOCSIWSENS */
	wavelan_get_sens,		/* SIOCGIWSENS */
	NULL,				/* SIOCSIWRANGE */
	wavelan_get_range,		/* SIOCGIWRANGE */
	NULL,				/* SIOCSIWPRIV */
	NULL,				/* SIOCGIWPRIV */
	NULL,				/* SIOCSIWSTATS */
	NULL,				/* SIOCGIWSTATS */
	iw_handler_set_spy,		/* SIOCSIWSPY */
	iw_handler_get_spy,		/* SIOCGIWSPY */
	iw_handler_set_thrspy,		/* SIOCSIWTHRSPY */
	iw_handler_get_thrspy,		/* SIOCGIWTHRSPY */
#ifdef WAVELAN_ROAMING_EXT
	wavelan_set_wap,		/* SIOCSIWAP */
	wavelan_get_wap,		/* SIOCGIWAP */
	NULL,				/* -- hole -- */
	NULL,				/* SIOCGIWAPLIST */
	NULL,				/* -- hole -- */
	NULL,				/* -- hole -- */
	wavelan_set_essid,		/* SIOCSIWESSID */
	wavelan_get_essid,		/* SIOCGIWESSID */
#else	/* WAVELAN_ROAMING_EXT */
	NULL,				/* SIOCSIWAP */
	NULL,				/* SIOCGIWAP */
	NULL,				/* -- hole -- */
	NULL,				/* SIOCGIWAPLIST */
	NULL,				/* -- hole -- */
	NULL,				/* -- hole -- */
	NULL,				/* SIOCSIWESSID */
	NULL,				/* SIOCGIWESSID */
#endif	/* WAVELAN_ROAMING_EXT */
	NULL,				/* SIOCSIWNICKN */
	NULL,				/* SIOCGIWNICKN */
	NULL,				/* -- hole -- */
	NULL,				/* -- hole -- */
	NULL,				/* SIOCSIWRATE */
	NULL,				/* SIOCGIWRATE */
	NULL,				/* SIOCSIWRTS */
	NULL,				/* SIOCGIWRTS */
	NULL,				/* SIOCSIWFRAG */
	NULL,				/* SIOCGIWFRAG */
	NULL,				/* SIOCSIWTXPOW */
	NULL,				/* SIOCGIWTXPOW */
	NULL,				/* SIOCSIWRETRY */
	NULL,				/* SIOCGIWRETRY */
	wavelan_set_encode,		/* SIOCSIWENCODE */
	wavelan_get_encode,		/* SIOCGIWENCODE */
};

static const iw_handler		wavelan_private_handler[] =
{
	wavelan_set_qthr,		/* SIOCIWFIRSTPRIV */
	wavelan_get_qthr,		/* SIOCIWFIRSTPRIV + 1 */
#ifdef WAVELAN_ROAMING
	wavelan_set_roam,		/* SIOCIWFIRSTPRIV + 2 */
	wavelan_get_roam,		/* SIOCIWFIRSTPRIV + 3 */
#else	/* WAVELAN_ROAMING */
	NULL,				/* SIOCIWFIRSTPRIV + 2 */
	NULL,				/* SIOCIWFIRSTPRIV + 3 */
#endif	/* WAVELAN_ROAMING */
#ifdef HISTOGRAM
	wavelan_set_histo,		/* SIOCIWFIRSTPRIV + 4 */
	wavelan_get_histo,		/* SIOCIWFIRSTPRIV + 5 */
#endif	/* HISTOGRAM */
};

static const struct iw_handler_def	wavelan_handler_def =
{
	.num_standard	= ARRAY_SIZE(wavelan_handler),
	.num_private	= ARRAY_SIZE(wavelan_private_handler),
	.num_private_args = ARRAY_SIZE(wavelan_private_args),
	.standard	= wavelan_handler,
	.private	= wavelan_private_handler,
	.private_args	= wavelan_private_args,
	.get_wireless_stats = wavelan_get_wireless_stats,
};

/*------------------------------------------------------------------*/
/*
 * Get wireless statistics
 * Called by /proc/net/wireless...
 */
static iw_stats *
wavelan_get_wireless_stats(struct net_device *	dev)
{
  unsigned int		base = dev->base_addr;
  net_local *		lp = netdev_priv(dev);
  mmr_t			m;
  iw_stats *		wstats;
  unsigned long		flags;

#ifdef DEBUG_IOCTL_TRACE
  printk(KERN_DEBUG "%s: ->wavelan_get_wireless_stats()\n", dev->name);
#endif

  /* Disable interrupts & save flags */
  spin_lock_irqsave(&lp->spinlock, flags);

  wstats = &lp->wstats;

  /* Get data from the mmc */
  mmc_out(base, mmwoff(0, mmw_freeze), 1);

  mmc_read(base, mmroff(0, mmr_dce_status), &m.mmr_dce_status, 1);
  mmc_read(base, mmroff(0, mmr_wrong_nwid_l), &m.mmr_wrong_nwid_l, 2);
  mmc_read(base, mmroff(0, mmr_thr_pre_set), &m.mmr_thr_pre_set, 4);

  mmc_out(base, mmwoff(0, mmw_freeze), 0);

  /* Copy data to wireless stuff */
  wstats->status = m.mmr_dce_status & MMR_DCE_STATUS;
  wstats->qual.qual = m.mmr_sgnl_qual & MMR_SGNL_QUAL;
  wstats->qual.level = m.mmr_signal_lvl & MMR_SIGNAL_LVL;
  wstats->qual.noise = m.mmr_silence_lvl & MMR_SILENCE_LVL;
  wstats->qual.updated = (((m.mmr_signal_lvl & MMR_SIGNAL_LVL_VALID) >> 7) |
			  ((m.mmr_signal_lvl & MMR_SIGNAL_LVL_VALID) >> 6) |
			  ((m.mmr_silence_lvl & MMR_SILENCE_LVL_VALID) >> 5));
  wstats->discard.nwid += (m.mmr_wrong_nwid_h << 8) | m.mmr_wrong_nwid_l;
  wstats->discard.code = 0L;
  wstats->discard.misc = 0L;

  /* ReEnable interrupts & restore flags */
  spin_unlock_irqrestore(&lp->spinlock, flags);

#ifdef DEBUG_IOCTL_TRACE
  printk(KERN_DEBUG "%s: <-wavelan_get_wireless_stats()\n", dev->name);
#endif
  return &lp->wstats;
}

/************************* PACKET RECEPTION *************************/
/*
 * This part deal with receiving the packets.
 * The interrupt handler get an interrupt when a packet has been
 * successfully received and called this part...
 */

/*------------------------------------------------------------------*/
/*
 * Calculate the starting address of the frame pointed to by the receive
 * frame pointer and verify that the frame seem correct
 * (called by wv_packet_rcv())
 */
static int
wv_start_of_frame(struct net_device *	dev,
		  int		rfp,	/* end of frame */
		  int		wrap)	/* start of buffer */
{
  unsigned int	base = dev->base_addr;
  int		rp;
  int		len;

  rp = (rfp - 5 + RX_SIZE) % RX_SIZE;
  outb(rp & 0xff, PIORL(base));
  outb(((rp >> 8) & PIORH_MASK), PIORH(base));
  len = inb(PIOP(base));
  len |= inb(PIOP(base)) << 8;

  /* Sanity checks on size */
  /* Frame too big */
  if(len > MAXDATAZ + 100)
    {
#ifdef DEBUG_RX_ERROR
      printk(KERN_INFO "%s: wv_start_of_frame: Received frame too large, rfp %d len 0x%x\n",
	     dev->name, rfp, len);
#endif
      return(-1);
    }
  
  /* Frame too short */
  if(len < 7)
    {
#ifdef DEBUG_RX_ERROR
      printk(KERN_INFO "%s: wv_start_of_frame: Received null frame, rfp %d len 0x%x\n",
	     dev->name, rfp, len);
#endif
      return(-1);
    }
  
  /* Wrap around buffer */
  if(len > ((wrap - (rfp - len) + RX_SIZE) % RX_SIZE))	/* magic formula ! */
    {
#ifdef DEBUG_RX_ERROR
      printk(KERN_INFO "%s: wv_start_of_frame: wrap around buffer, wrap %d rfp %d len 0x%x\n",
	     dev->name, wrap, rfp, len);
#endif
      return(-1);
    }

  return((rp - len + RX_SIZE) % RX_SIZE);
} /* wv_start_of_frame */

/*------------------------------------------------------------------*/
/*
 * This routine does the actual copy of data (including the ethernet
 * header structure) from the WaveLAN card to an sk_buff chain that
 * will be passed up to the network interface layer. NOTE: We
 * currently don't handle trailer protocols (neither does the rest of
 * the network interface), so if that is needed, it will (at least in
 * part) be added here.  The contents of the receive ring buffer are
 * copied to a message chain that is then passed to the kernel.
 *
 * Note: if any errors occur, the packet is "dropped on the floor"
 * (called by wv_packet_rcv())
 */
static void
wv_packet_read(struct net_device *		dev,
	       int		fd_p,
	       int		sksize)
{
  net_local *		lp = netdev_priv(dev);
  struct sk_buff *	skb;

#ifdef DEBUG_RX_TRACE
  printk(KERN_DEBUG "%s: ->wv_packet_read(0x%X, %d)\n",
	 dev->name, fd_p, sksize);
#endif

  /* Allocate some buffer for the new packet */
  if((skb = dev_alloc_skb(sksize+2)) == (struct sk_buff *) NULL)
    {
#ifdef DEBUG_RX_ERROR
      printk(KERN_INFO "%s: wv_packet_read(): could not alloc_skb(%d, GFP_ATOMIC)\n",
	     dev->name, sksize);
#endif
      dev->stats.rx_dropped++;
      /*
       * Not only do we want to return here, but we also need to drop the
       * packet on the floor to clear the interrupt.
       */
      return;
    }

  skb_reserve(skb, 2);
  fd_p = read_ringbuf(dev, fd_p, (char *) skb_put(skb, sksize), sksize);
  skb->protocol = eth_type_trans(skb, dev);

#ifdef DEBUG_RX_INFO
  wv_packet_info(skb_mac_header(skb), sksize, dev->name, "wv_packet_read");
#endif	/* DEBUG_RX_INFO */
     
  /* Statistics gathering & stuff associated.
   * It seem a bit messy with all the define, but it's really simple... */
  if(
#ifdef IW_WIRELESS_SPY
     (lp->spy_data.spy_number > 0) ||
#endif	/* IW_WIRELESS_SPY */
#ifdef HISTOGRAM
     (lp->his_number > 0) ||
#endif	/* HISTOGRAM */
#ifdef WAVELAN_ROAMING
     (do_roaming) ||
#endif	/* WAVELAN_ROAMING */
     0)
    {
      u_char	stats[3];	/* Signal level, Noise level, Signal quality */

      /* read signal level, silence level and signal quality bytes */
      fd_p = read_ringbuf(dev, (fd_p + 4) % RX_SIZE + RX_BASE,
			  stats, 3);
#ifdef DEBUG_RX_INFO
      printk(KERN_DEBUG "%s: wv_packet_read(): Signal level %d/63, Silence level %d/63, signal quality %d/16\n",
	     dev->name, stats[0] & 0x3F, stats[1] & 0x3F, stats[2] & 0x0F);
#endif

#ifdef WAVELAN_ROAMING
      if(do_roaming)
	if(WAVELAN_BEACON(skb->data))
	  wl_roam_gather(dev, skb->data, stats);
#endif	/* WAVELAN_ROAMING */
	  
#ifdef WIRELESS_SPY
      wl_spy_gather(dev, skb_mac_header(skb) + WAVELAN_ADDR_SIZE, stats);
#endif	/* WIRELESS_SPY */
#ifdef HISTOGRAM
      wl_his_gather(dev, stats);
#endif	/* HISTOGRAM */
    }

  /*
   * Hand the packet to the Network Module
   */
  netif_rx(skb);

  /* Keep stats up to date */
  dev->stats.rx_packets++;
  dev->stats.rx_bytes += sksize;

#ifdef DEBUG_RX_TRACE
  printk(KERN_DEBUG "%s: <-wv_packet_read()\n", dev->name);
#endif
  return;
}

/*------------------------------------------------------------------*/
/*
 * This routine is called by the interrupt handler to initiate a
 * packet transfer from the card to the network interface layer above
 * this driver.  This routine checks if a buffer has been successfully
 * received by the WaveLAN card.  If so, the routine wv_packet_read is
 * called to do the actual transfer of the card's data including the
 * ethernet header into a packet consisting of an sk_buff chain.
 * (called by wavelan_interrupt())
 * Note : the spinlock is already grabbed for us and irq are disabled.
 */
static void
wv_packet_rcv(struct net_device *	dev)
{
  unsigned int	base = dev->base_addr;
  net_local *	lp = netdev_priv(dev);
  int		newrfp;
  int		rp;
  int		len;
  int		f_start;
  int		status;
  int		i593_rfp;
  int		stat_ptr;
  u_char	c[4];

#ifdef DEBUG_RX_TRACE
  printk(KERN_DEBUG "%s: ->wv_packet_rcv()\n", dev->name);
#endif

  /* Get the new receive frame pointer from the i82593 chip */
  outb(CR0_STATUS_2 | OP0_NOP, LCCR(base));
  i593_rfp = inb(LCSR(base));
  i593_rfp |= inb(LCSR(base)) << 8;
  i593_rfp %= RX_SIZE;

  /* Get the new receive frame pointer from the WaveLAN card.
   * It is 3 bytes more than the increment of the i82593 receive
   * frame pointer, for each packet. This is because it includes the
   * 3 roaming bytes added by the mmc.
   */
  newrfp = inb(RPLL(base));
  newrfp |= inb(RPLH(base)) << 8;
  newrfp %= RX_SIZE;

#ifdef DEBUG_RX_INFO
  printk(KERN_DEBUG "%s: wv_packet_rcv(): i593_rfp %d stop %d newrfp %d lp->rfp %d\n",
	 dev->name, i593_rfp, lp->stop, newrfp, lp->rfp);
#endif

#ifdef DEBUG_RX_ERROR
  /* If no new frame pointer... */
  if(lp->overrunning || newrfp == lp->rfp)
    printk(KERN_INFO "%s: wv_packet_rcv(): no new frame: i593_rfp %d stop %d newrfp %d lp->rfp %d\n",
	   dev->name, i593_rfp, lp->stop, newrfp, lp->rfp);
#endif

  /* Read all frames (packets) received */
  while(newrfp != lp->rfp)
    {
      /* A frame is composed of the packet, followed by a status word,
       * the length of the frame (word) and the mmc info (SNR & qual).
       * It's because the length is at the end that we can only scan
       * frames backward. */

      /* Find the first frame by skipping backwards over the frames */
      rp = newrfp;	/* End of last frame */
      while(((f_start = wv_start_of_frame(dev, rp, newrfp)) != lp->rfp) &&
	    (f_start != -1))
	  rp = f_start;

      /* If we had a problem */
      if(f_start == -1)
	{
#ifdef DEBUG_RX_ERROR
	  printk(KERN_INFO "wavelan_cs: cannot find start of frame ");
	  printk(" i593_rfp %d stop %d newrfp %d lp->rfp %d\n",
		 i593_rfp, lp->stop, newrfp, lp->rfp);
#endif
	  lp->rfp = rp;		/* Get to the last usable frame */
	  continue;
	}

      /* f_start point to the beggining of the first frame received
       * and rp to the beggining of the next one */

      /* Read status & length of the frame */
      stat_ptr = (rp - 7 + RX_SIZE) % RX_SIZE;
      stat_ptr = read_ringbuf(dev, stat_ptr, c, 4);
      status = c[0] | (c[1] << 8);
      len = c[2] | (c[3] << 8);

      /* Check status */
      if((status & RX_RCV_OK) != RX_RCV_OK)
	{
	  dev->stats.rx_errors++;
	  if(status & RX_NO_SFD)
	    dev->stats.rx_frame_errors++;
	  if(status & RX_CRC_ERR)
	    dev->stats.rx_crc_errors++;
	  if(status & RX_OVRRUN)
	    dev->stats.rx_over_errors++;

#ifdef DEBUG_RX_FAIL
	  printk(KERN_DEBUG "%s: wv_packet_rcv(): packet not received ok, status = 0x%x\n",
		 dev->name, status);
#endif
	}
      else
	/* Read the packet and transmit to Linux */
	wv_packet_read(dev, f_start, len - 2);

      /* One frame has been processed, skip it */
      lp->rfp = rp;
    }

  /*
   * Update the frame stop register, but set it to less than
   * the full 8K to allow space for 3 bytes of signal strength
   * per packet.
   */
  lp->stop = (i593_rfp + RX_SIZE - ((RX_SIZE / 64) * 3)) % RX_SIZE;
  outb(OP0_SWIT_TO_PORT_1 | CR0_CHNL, LCCR(base));
  outb(CR1_STOP_REG_UPDATE | (lp->stop >> RX_SIZE_SHIFT), LCCR(base));
  outb(OP1_SWIT_TO_PORT_0, LCCR(base));

#ifdef DEBUG_RX_TRACE
  printk(KERN_DEBUG "%s: <-wv_packet_rcv()\n", dev->name);
#endif
}

/*********************** PACKET TRANSMISSION ***********************/
/*
 * This part deal with sending packet through the wavelan
 * We copy the packet to the send buffer and then issue the send
 * command to the i82593. The result of this operation will be
 * checked in wavelan_interrupt()
 */

/*------------------------------------------------------------------*/
/*
 * This routine fills in the appropriate registers and memory
 * locations on the WaveLAN card and starts the card off on
 * the transmit.
 * (called in wavelan_packet_xmit())
 */
static void
wv_packet_write(struct net_device *	dev,
		void *		buf,
		short		length)
{
  net_local *		lp = netdev_priv(dev);
  unsigned int		base = dev->base_addr;
  unsigned long		flags;
  int			clen = length;
  register u_short	xmtdata_base = TX_BASE;

#ifdef DEBUG_TX_TRACE
  printk(KERN_DEBUG "%s: ->wv_packet_write(%d)\n", dev->name, length);
#endif

  spin_lock_irqsave(&lp->spinlock, flags);

  /* Write the length of data buffer followed by the buffer */
  outb(xmtdata_base & 0xff, PIORL(base));
  outb(((xmtdata_base >> 8) & PIORH_MASK) | PIORH_SEL_TX, PIORH(base));
  outb(clen & 0xff, PIOP(base));	/* lsb */
  outb(clen >> 8, PIOP(base));  	/* msb */

  /* Send the data */
  outsb(PIOP(base), buf, clen);

  /* Indicate end of transmit chain */
  outb(OP0_NOP, PIOP(base));
  /* josullvn@cs.cmu.edu: need to send a second NOP for alignment... */
  outb(OP0_NOP, PIOP(base));

  /* Reset the transmit DMA pointer */
  hacr_write_slow(base, HACR_PWR_STAT | HACR_TX_DMA_RESET);
  hacr_write(base, HACR_DEFAULT);
  /* Send the transmit command */
  wv_82593_cmd(dev, "wv_packet_write(): transmit",
	       OP0_TRANSMIT, SR0_NO_RESULT);

  /* Make sure the watchdog will keep quiet for a while */
  dev->trans_start = jiffies;

  /* Keep stats up to date */
  dev->stats.tx_bytes += length;

  spin_unlock_irqrestore(&lp->spinlock, flags);

#ifdef DEBUG_TX_INFO
  wv_packet_info((u_char *) buf, length, dev->name, "wv_packet_write");
#endif	/* DEBUG_TX_INFO */

#ifdef DEBUG_TX_TRACE
  printk(KERN_DEBUG "%s: <-wv_packet_write()\n", dev->name);
#endif
}

/*------------------------------------------------------------------*/
/*
 * This routine is called when we want to send a packet (NET3 callback)
 * In this routine, we check if the harware is ready to accept
 * the packet. We also prevent reentrance. Then, we call the function
 * to send the packet...
 */
static netdev_tx_t
wavelan_packet_xmit(struct sk_buff *	skb,
		    struct net_device *		dev)
{
  net_local *		lp = netdev_priv(dev);
  unsigned long		flags;

#ifdef DEBUG_TX_TRACE
  printk(KERN_DEBUG "%s: ->wavelan_packet_xmit(0x%X)\n", dev->name,
	 (unsigned) skb);
#endif

  /*
   * Block a timer-based transmit from overlapping a previous transmit.
   * In other words, prevent reentering this routine.
   */
  netif_stop_queue(dev);

  /* If somebody has asked to reconfigure the controller,
   * we can do it now */
  if(lp->reconfig_82593)
    {
      spin_lock_irqsave(&lp->spinlock, flags);	/* Disable interrupts */
      wv_82593_config(dev);
      spin_unlock_irqrestore(&lp->spinlock, flags);	/* Re-enable interrupts */
      /* Note : the configure procedure was totally synchronous,
       * so the Tx buffer is now free */
    }

	/* Check if we need some padding */
	/* Note : on wireless the propagation time is in the order of 1us,
	 * and we don't have the Ethernet specific requirement of beeing
	 * able to detect collisions, therefore in theory we don't really
	 * need to pad. Jean II */
	if (skb_padto(skb, ETH_ZLEN))
		return NETDEV_TX_OK;

  wv_packet_write(dev, skb->data, skb->len);

  dev_kfree_skb(skb);

#ifdef DEBUG_TX_TRACE
  printk(KERN_DEBUG "%s: <-wavelan_packet_xmit()\n", dev->name);
#endif
  return NETDEV_TX_OK;
}

/********************** HARDWARE CONFIGURATION **********************/
/*
 * This part do the real job of starting and configuring the hardware.
 */

/*------------------------------------------------------------------*/
/*
 * Routine to initialize the Modem Management Controller.
 * (called by wv_hw_config())
 */
static int
wv_mmc_init(struct net_device *	dev)
{
  unsigned int	base = dev->base_addr;
  psa_t		psa;
  mmw_t		m;
  int		configured;
  int		i;		/* Loop counter */

#ifdef DEBUG_CONFIG_TRACE
  printk(KERN_DEBUG "%s: ->wv_mmc_init()\n", dev->name);
#endif

  /* Read the parameter storage area */
  psa_read(dev, 0, (unsigned char *) &psa, sizeof(psa));

  /*
   * Check the first three octets of the MAC addr for the manufacturer's code.
   * Note: If you get the error message below, you've got a
   * non-NCR/AT&T/Lucent PCMCIA cards, see wavelan_cs.h for detail on
   * how to configure your card...
   */
  for (i = 0; i < ARRAY_SIZE(MAC_ADDRESSES); i++)
    if ((psa.psa_univ_mac_addr[0] == MAC_ADDRESSES[i][0]) &&
        (psa.psa_univ_mac_addr[1] == MAC_ADDRESSES[i][1]) &&
        (psa.psa_univ_mac_addr[2] == MAC_ADDRESSES[i][2]))
      break;

  /* If we have not found it... */
  if (i == ARRAY_SIZE(MAC_ADDRESSES))
    {
#ifdef DEBUG_CONFIG_ERRORS
      printk(KERN_WARNING "%s: wv_mmc_init(): Invalid MAC address: %02X:%02X:%02X:...\n",
	     dev->name, psa.psa_univ_mac_addr[0],
	     psa.psa_univ_mac_addr[1], psa.psa_univ_mac_addr[2]);
#endif
      return FALSE;
    }

  /* Get the MAC address */
  memcpy(&dev->dev_addr[0], &psa.psa_univ_mac_addr[0], WAVELAN_ADDR_SIZE);

#ifdef USE_PSA_CONFIG
  configured = psa.psa_conf_status & 1;
#else
  configured = 0;
#endif

  /* Is the PSA is not configured */
  if(!configured)
    {
      /* User will be able to configure NWID after (with iwconfig) */
      psa.psa_nwid[0] = 0;
      psa.psa_nwid[1] = 0;

      /* As NWID is not set : no NWID checking */
      psa.psa_nwid_select = 0;

      /* Disable encryption */
      psa.psa_encryption_select = 0;

      /* Set to standard values
       * 0x04 for AT,
       * 0x01 for MCA,
       * 0x04 for PCMCIA and 2.00 card (AT&T 407-024689/E document)
       */
      if (psa.psa_comp_number & 1)
	psa.psa_thr_pre_set = 0x01;
      else
	psa.psa_thr_pre_set = 0x04;
      psa.psa_quality_thr = 0x03;

      /* It is configured */
      psa.psa_conf_status |= 1;

#ifdef USE_PSA_CONFIG
      /* Write the psa */
      psa_write(dev, (char *)psa.psa_nwid - (char *)&psa,
		(unsigned char *)psa.psa_nwid, 4);
      psa_write(dev, (char *)&psa.psa_thr_pre_set - (char *)&psa,
		(unsigned char *)&psa.psa_thr_pre_set, 1);
      psa_write(dev, (char *)&psa.psa_quality_thr - (char *)&psa,
		(unsigned char *)&psa.psa_quality_thr, 1);
      psa_write(dev, (char *)&psa.psa_conf_status - (char *)&psa,
		(unsigned char *)&psa.psa_conf_status, 1);
      /* update the Wavelan checksum */
      update_psa_checksum(dev);
#endif	/* USE_PSA_CONFIG */
    }

  /* Zero the mmc structure */
  memset(&m, 0x00, sizeof(m));

  /* Copy PSA info to the mmc */
  m.mmw_netw_id_l = psa.psa_nwid[1];
  m.mmw_netw_id_h = psa.psa_nwid[0];
  
  if(psa.psa_nwid_select & 1)
    m.mmw_loopt_sel = 0x00;
  else
    m.mmw_loopt_sel = MMW_LOOPT_SEL_DIS_NWID;

  memcpy(&m.mmw_encr_key, &psa.psa_encryption_key, 
	 sizeof(m.mmw_encr_key));

  if(psa.psa_encryption_select)
    m.mmw_encr_enable = MMW_ENCR_ENABLE_EN | MMW_ENCR_ENABLE_MODE;
  else
    m.mmw_encr_enable = 0;

  m.mmw_thr_pre_set = psa.psa_thr_pre_set & 0x3F;
  m.mmw_quality_thr = psa.psa_quality_thr & 0x0F;

  /*
   * Set default modem control parameters.
   * See NCR document 407-0024326 Rev. A.
   */
  m.mmw_jabber_enable = 0x01;
  m.mmw_anten_sel = MMW_ANTEN_SEL_ALG_EN;
  m.mmw_ifs = 0x20;
  m.mmw_mod_delay = 0x04;
  m.mmw_jam_time = 0x38;

  m.mmw_des_io_invert = 0;
  m.mmw_freeze = 0;
  m.mmw_decay_prm = 0;
  m.mmw_decay_updat_prm = 0;

  /* Write all info to mmc */
  mmc_write(base, 0, (u_char *)&m, sizeof(m));

  /* The following code start the modem of the 2.00 frequency
   * selectable cards at power on. It's not strictly needed for the
   * following boots...
   * The original patch was by Joe Finney for the PCMCIA driver, but
   * I've cleaned it a bit and add documentation.
   * Thanks to Loeke Brederveld from Lucent for the info.
   */

  /* Attempt to recognise 2.00 cards (2.4 GHz frequency selectable)
   * (does it work for everybody ? - especially old cards...) */
  /* Note : WFREQSEL verify that it is able to read from EEprom
   * a sensible frequency (address 0x00) + that MMR_FEE_STATUS_ID
   * is 0xA (Xilinx version) or 0xB (Ariadne version).
   * My test is more crude but do work... */
  if(!(mmc_in(base, mmroff(0, mmr_fee_status)) &
       (MMR_FEE_STATUS_DWLD | MMR_FEE_STATUS_BUSY)))
    {
      /* We must download the frequency parameters to the
       * synthetisers (from the EEprom - area 1)
       * Note : as the EEprom is auto decremented, we set the end
       * if the area... */
      m.mmw_fee_addr = 0x0F;
      m.mmw_fee_ctrl = MMW_FEE_CTRL_READ | MMW_FEE_CTRL_DWLD;
      mmc_write(base, (char *)&m.mmw_fee_ctrl - (char *)&m,
		(unsigned char *)&m.mmw_fee_ctrl, 2);

      /* Wait until the download is finished */
      fee_wait(base, 100, 100);

#ifdef DEBUG_CONFIG_INFO
      /* The frequency was in the last word downloaded... */
      mmc_read(base, (char *)&m.mmw_fee_data_l - (char *)&m,
	       (unsigned char *)&m.mmw_fee_data_l, 2);

      /* Print some info for the user */
      printk(KERN_DEBUG "%s: Wavelan 2.00 recognised (frequency select) : Current frequency = %ld\n",
	     dev->name,
	     ((m.mmw_fee_data_h << 4) |
	      (m.mmw_fee_data_l >> 4)) * 5 / 2 + 24000L);
#endif

      /* We must now download the power adjust value (gain) to
       * the synthetisers (from the EEprom - area 7 - DAC) */
      m.mmw_fee_addr = 0x61;
      m.mmw_fee_ctrl = MMW_FEE_CTRL_READ | MMW_FEE_CTRL_DWLD;
      mmc_write(base, (char *)&m.mmw_fee_ctrl - (char *)&m,
		(unsigned char *)&m.mmw_fee_ctrl, 2);

      /* Wait until the download is finished */
    }	/* if 2.00 card */

#ifdef DEBUG_CONFIG_TRACE
  printk(KERN_DEBUG "%s: <-wv_mmc_init()\n", dev->name);
#endif
  return TRUE;
}

/*------------------------------------------------------------------*/
/*
 * Routine to gracefully turn off reception, and wait for any commands
 * to complete.
 * (called in wv_ru_start() and wavelan_close() and wavelan_event())
 */
static int
wv_ru_stop(struct net_device *	dev)
{
  unsigned int	base = dev->base_addr;
  net_local *	lp = netdev_priv(dev);
  unsigned long	flags;
  int		status;
  int		spin;

#ifdef DEBUG_CONFIG_TRACE
  printk(KERN_DEBUG "%s: ->wv_ru_stop()\n", dev->name);
#endif

  spin_lock_irqsave(&lp->spinlock, flags);

  /* First, send the LAN controller a stop receive command */
  wv_82593_cmd(dev, "wv_graceful_shutdown(): stop-rcv",
	       OP0_STOP_RCV, SR0_NO_RESULT);

  /* Then, spin until the receive unit goes idle */
  spin = 300;
  do
    {
      udelay(10);
      outb(OP0_NOP | CR0_STATUS_3, LCCR(base));
      status = inb(LCSR(base));
    }
  while(((status & SR3_RCV_STATE_MASK) != SR3_RCV_IDLE) && (spin-- > 0));

  /* Now, spin until the chip finishes executing its current command */
  do
    {
      udelay(10);
      outb(OP0_NOP | CR0_STATUS_3, LCCR(base));
      status = inb(LCSR(base));
    }
  while(((status & SR3_EXEC_STATE_MASK) != SR3_EXEC_IDLE) && (spin-- > 0));

  spin_unlock_irqrestore(&lp->spinlock, flags);

  /* If there was a problem */
  if(spin <= 0)
    {
#ifdef DEBUG_CONFIG_ERRORS
      printk(KERN_INFO "%s: wv_ru_stop(): The chip doesn't want to stop...\n",
	     dev->name);
#endif
      return FALSE;
    }

#ifdef DEBUG_CONFIG_TRACE
  printk(KERN_DEBUG "%s: <-wv_ru_stop()\n", dev->name);
#endif
  return TRUE;
} /* wv_ru_stop */

/*------------------------------------------------------------------*/
/*
 * This routine starts the receive unit running.  First, it checks if
 * the card is actually ready. Then the card is instructed to receive
 * packets again.
 * (called in wv_hw_reset() & wavelan_open())
 */
static int
wv_ru_start(struct net_device *	dev)
{
  unsigned int	base = dev->base_addr;
  net_local *	lp = netdev_priv(dev);
  unsigned long	flags;

#ifdef DEBUG_CONFIG_TRACE
  printk(KERN_DEBUG "%s: ->wv_ru_start()\n", dev->name);
#endif

  /*
   * We need to start from a quiescent state. To do so, we could check
   * if the card is already running, but instead we just try to shut
   * it down. First, we disable reception (in case it was already enabled).
   */
  if(!wv_ru_stop(dev))
    return FALSE;

  spin_lock_irqsave(&lp->spinlock, flags);

  /* Now we know that no command is being executed. */

  /* Set the receive frame pointer and stop pointer */
  lp->rfp = 0;
  outb(OP0_SWIT_TO_PORT_1 | CR0_CHNL, LCCR(base));

  /* Reset ring management.  This sets the receive frame pointer to 1 */
  outb(OP1_RESET_RING_MNGMT, LCCR(base));

#if 0
  /* XXX the i82593 manual page 6-4 seems to indicate that the stop register
     should be set as below */
  /* outb(CR1_STOP_REG_UPDATE|((RX_SIZE - 0x40)>> RX_SIZE_SHIFT),LCCR(base));*/
#elif 0
  /* but I set it 0 instead */
  lp->stop = 0;
#else
  /* but I set it to 3 bytes per packet less than 8K */
  lp->stop = (0 + RX_SIZE - ((RX_SIZE / 64) * 3)) % RX_SIZE;
#endif
  outb(CR1_STOP_REG_UPDATE | (lp->stop >> RX_SIZE_SHIFT), LCCR(base));
  outb(OP1_INT_ENABLE, LCCR(base));
  outb(OP1_SWIT_TO_PORT_0, LCCR(base));

  /* Reset receive DMA pointer */
  hacr_write_slow(base, HACR_PWR_STAT | HACR_TX_DMA_RESET);
  hacr_write_slow(base, HACR_DEFAULT);

  /* Receive DMA on channel 1 */
  wv_82593_cmd(dev, "wv_ru_start(): rcv-enable",
	       CR0_CHNL | OP0_RCV_ENABLE, SR0_NO_RESULT);

#ifdef DEBUG_I82593_SHOW
  {
    int	status;
    int	opri;
    int	spin = 10000;

    /* spin until the chip starts receiving */
    do
      {
	outb(OP0_NOP | CR0_STATUS_3, LCCR(base));
	status = inb(LCSR(base));
	if(spin-- <= 0)
	  break;
      }
    while(((status & SR3_RCV_STATE_MASK) != SR3_RCV_ACTIVE) &&
	  ((status & SR3_RCV_STATE_MASK) != SR3_RCV_READY));
    printk(KERN_DEBUG "rcv status is 0x%x [i:%d]\n",
	   (status & SR3_RCV_STATE_MASK), i);
  }
#endif

  spin_unlock_irqrestore(&lp->spinlock, flags);

#ifdef DEBUG_CONFIG_TRACE
  printk(KERN_DEBUG "%s: <-wv_ru_start()\n", dev->name);
#endif
  return TRUE;
}

/*------------------------------------------------------------------*/
/*
 * This routine does a standard config of the WaveLAN controller (i82593).
 * In the ISA driver, this is integrated in wavelan_hardware_reset()
 * (called by wv_hw_config(), wv_82593_reconfig() & wavelan_packet_xmit())
 */
static int
wv_82593_config(struct net_device *	dev)
{
  unsigned int			base = dev->base_addr;
  net_local *			lp = netdev_priv(dev);
  struct i82593_conf_block	cfblk;
  int				ret = TRUE;

#ifdef DEBUG_CONFIG_TRACE
  printk(KERN_DEBUG "%s: ->wv_82593_config()\n", dev->name);
#endif

  /* Create & fill i82593 config block
   *
   * Now conform to Wavelan document WCIN085B
   */
  memset(&cfblk, 0x00, sizeof(struct i82593_conf_block));
  cfblk.d6mod = FALSE;  	/* Run in i82593 advanced mode */
  cfblk.fifo_limit = 5;         /* = 56 B rx and 40 B tx fifo thresholds */
  cfblk.forgnesi = FALSE;       /* 0=82C501, 1=AMD7992B compatibility */
  cfblk.fifo_32 = 1;
  cfblk.throttle_enb = FALSE;
  cfblk.contin = TRUE;          /* enable continuous mode */
  cfblk.cntrxint = FALSE;       /* enable continuous mode receive interrupts */
  cfblk.addr_len = WAVELAN_ADDR_SIZE;
  cfblk.acloc = TRUE;           /* Disable source addr insertion by i82593 */
  cfblk.preamb_len = 0;         /* 2 bytes preamble (SFD) */
  cfblk.loopback = FALSE;
  cfblk.lin_prio = 0;   	/* conform to 802.3 backoff algorithm */
  cfblk.exp_prio = 5;	        /* conform to 802.3 backoff algorithm */
  cfblk.bof_met = 1;	        /* conform to 802.3 backoff algorithm */
  cfblk.ifrm_spc = 0x20 >> 4;	/* 32 bit times interframe spacing */
  cfblk.slottim_low = 0x20 >> 5;	/* 32 bit times slot time */
  cfblk.slottim_hi = 0x0;
  cfblk.max_retr = 15;
  cfblk.prmisc = ((lp->promiscuous) ? TRUE: FALSE);	/* Promiscuous mode */
  cfblk.bc_dis = FALSE;         /* Enable broadcast reception */
  cfblk.crs_1 = TRUE;		/* Transmit without carrier sense */
  cfblk.nocrc_ins = FALSE;	/* i82593 generates CRC */	
  cfblk.crc_1632 = FALSE;	/* 32-bit Autodin-II CRC */
  cfblk.crs_cdt = FALSE;	/* CD not to be interpreted as CS */
  cfblk.cs_filter = 0;  	/* CS is recognized immediately */
  cfblk.crs_src = FALSE;	/* External carrier sense */
  cfblk.cd_filter = 0;  WaveCD is recognized immediately*		Jean II -min_fr_len = ETH_ZLEN >> 2; w. S/* Minimum frame length 64 bytes*		Jean II -lng_typ = /*
 *	WaveLor detfield > 1500 = typeD. Jose * This code isfld = TRUE; ReorDisable 802.3 for det. Josecheckhe changes hrxcrc_xfare alsounderon't transfer CRC to memor the driver.
artxorts versunder the orautomatic reof Wamissionhe changes hsarec * can work on Linux source addr trig ofrgan		Jean II -tx_jabb96
 *s version  the or Lancasjam sequenc *		Jean II -hash_1rived fro ReorUse bits 0-5 in mcinneyess  rouhe changes hebpkpolare also undeLoopback pin active higller.
 *
 * Tfd
 * /*
 *	Wunder the orfull duplex operatf David Hinds' dummy_6
 *
x3fitialiall on *
 * This codmult_iarived from AnNo  Jusiple individualement Co *
 * This coddis_boport/*
 *	Waveity in Uthe uce off algorithm ?!AVELAN_ROAMING	
 *ine  also undesetCMCI1lancs.ac.uk) atifs_h suig = 3rsionHmm... Universthe c#ifdef MULTICAST_ALL/22/98 by c_rt a= (lp->alledia.cast ?ormer: Lanca)rsionAllowort a*   A non*
 * #elseel 82593 coprocessYunzhou Li r@media. non-rnet od *		J#endif * This cocv_moal c0ou Li Monitory Anthdransceiver ice.
 *
 rag_acptorts version 2 not acceptcs.pgmenfor lis.ac.uk) strttrs Seger (jseger@mstar0 of Wart of Dathresho the changes hfred
 * can work oFIFO 2.0.36 with support of David Hinds' Pyncrqe changes  ReorSynchronous DRQ deasseref Wdio vid Hinds' Pttinal c also  Reor6ls.
 ng ttusatigister*
 * This codrx_eoerivght 1995
 *SigcmciEOP on packetatioepef WAVELAN_ROAMIttts Institute of Technology
 *
 *   Permsupport of David Hinds' rbuf_size = RX_SIZE>>11rsionSermissior tbufveLAee isse and withocvst Institute Wavelnhe orRed
 *  SrmisRoseph
 copy controlDEBUG_I82593_SHOW
  print_hex_dump(KERN_ docu, "wavelan_cs: config block: ", DUMP_PREFIX_NONE,
		 16, 1, &n II ,this of(struct intatiotisi_or pu), false);y Joseph
See wCopyity.
tising or puCMCIty.
withoucopyrioutb(TX_BASE & 0xff, PIORL(base));entation(n that co>> 8) &nd diH_MASK) |f M.I.TSEL_TXand diHtribution is
 * 
 *   program without specific popying and dPtributio. See wlsbcumentation   the suitability of this softwassio any purpose. Wavemro-
 *   visb(ny purpose, (char *)on of the
 *   program without specific p); and nore) Wav82593 t DMA poin and su  hacr_write_slowtribu, HACR_PWR_STAT |eed toTX_DMA_RESETion ihere, you uaranteed toDEFAULlude "if(!wv_ithout md(devd
 *#ifdef WAising()ertisingure" dist  OP0_CONFIGURE, SRed long dat_DONE))
w. Sreullv/*
 *	 and noInitiale isadapter's ethernet MACement Conumentation that copying and distribution is
 *   by permission of M.I.T.  M.I.T. makes no representations about
 *WAVELAN_ADDReby gwarranty.          Anthcount pro-
 *   vide0warranty.      u Li ***********
 *************************&dev->dev_nney[0], ****** MISC SUBRO**
 *
 */

/* Do *NOT* add other headers here, you are guaranteed to be wrong - Jean II */
#include "wavelan_cs.p.h"		/* Private header */

#ifdef WAVELAN_ROAMING
static void wlia-setupiry(unsignedIA_SETUPa);
st-----*/
 wl_del_wavepoint(wavepoincontrol****** MROAMING_wavehistf roamingnisaeppeard, joirol/e "Beacon Rritist"t driver f group*********nlineBut only if it'su & jbase)
re already (formeif(do_char
hal_wavevelacoprdELAN_R****** MBEACO MISC ESS or i82593)
 */

/*, 1rmission,Wavester.
 */
statid supnline u_anypport PCMCIAent ContoR) Wa--------r.
 mc_*****l_wave{read   rogram ------*list *	dmion i----int			_chasginal c****** MISC SUBRO * cr, HACR(bastatus Regi docum long _INFO-------name k not be use "%s: wv_hwtic void wl) Wa%dbase,
	   u_char	hes:\niry(-----* (waname,----------*/
ion i(u_lfor(dmi=* (wa/*-----; ----r)
{=dmi->next)
	 Command Register.  %pMeeder)
{->dmian mormission, andead */
tory *wavepoint, struct net_loe times when it is sr_read ntation that copying and distribution i---------  by permission of M.I.T.  M.I.T. makes no representations abo-------------------re for any purpose. *******************/
/*
---*/
/*
 ----------ssionead(struct net_device *	dev,

 *******u_long	base,
		u_char	hacr)
{
  hacr_write(base******************ly be needed _char __iomem *len**
 *
ead */


/* Do *NOT* add other headers     here, you are guaranteed to be wrong - Jean II */
#include "    *b++ = rea.p.h"		/* Private header-----*/

#ifdef WAVELAN_ROAMING
static void wlmc--------------    gnedMC---*/
/*
 * incremen wl_del_	point(wavepoi
     cr, HACR(bas =
stati-----*/
/Waverememncasto avoid repeate   p
{
  outb  }write(uJob done, clearse)
{flag  outbr.
 tionsingifdef daptor Status Regie to card's HTRACEhe name and Register. Incl<-MING
static void ht onltatic vormission, vepoiurn(re_slo}

/*-uct net_device *	dev,
	  int		o,	/* Offset in psa */
	  u_char *	*/
/*
 * Readse)
{Acc ConCcell_exdef WA
 *   an, perform a software2;
   ,Buffanin men re-ead(u_ be giard's
{
  net_.
  BuffIf I underst netcorrectly :<< 1);

he pcmcia----erface@comtheBuff*   in tr = (calceivbyludeic void )
  ouony ic----
wv_nt = 0_;
   program net_devial c	dev)
 /* ---------- specreg_t	reerne{ 0, CS_READ, CISRE carR, 0 }se orogram PSA_BUSodel, we linkesso(SA mlocal *)netvelaprivLAN_))-> __i-----------------*/
/*
 * Write the Parameter Storage ->ag PSA_BUSY as iN card's memory
 */
stati * o =  volaticl@csss there		n)	/*fy teph
 ( __i, &regeader * (i != 0e));
} /* hacrcs_erro  {
    ory */	  int		n)	/*Length of i    * onlvoid
p Because of }e of th
 * Write to card's Host Ada Command Register. Inclu   in ato PSA */
  ha:
	  int Josnisa0x%xeeded.
static void(u_int) Jos.Valuermission, anderifAcef WA=o knWRITuse oerify) != =00))
	mdela| COR_SW
#inclader  | HACR_ROM_WEN);

  while(n-- > 0)
    {
      /* write to PSA */
      writeb(*b++, ptr);
      ptr += 2;

      /* I don't have the spec, so I don't know whaCMCIA_915) && (count++ < 100))
	mdelay( /* LEVEL_IRQ

  /*  long host interface back in standard state */
  hacr_write(base, HACR_DEFAULT);
} /* psa_write */

#ifdef SET_PSA_CRC
/*----------------------------------------------------------*/
/*
 * Write the Parameter Storage Area to PSA */
  hacr_write(base, HACR_PWR_the spec, alsote(struct net_device *	dev,
	  int		o,	/* Offset in psa */
	  u_char *	b,	/* Buffhack see a delay fonisaaddr;
 af anda CARD_INSERTION event isbaseded
 * d,  */_cell_exp		couhack se hardm *ptr = Not_bytat		couission to will beread(u_l----hack se->open(), supporthe odel, w forcell_expd but idle.. the Puffer s------o eth
hasn foons:the 	1. Aunt = 0;{
  net_local  (us
hasdon't use the CRCeem 	2_cnt]owerr */

  ;
    DMAt_cnt3. R int		couLAN byttroller_cnt4.ay(1);
} /* nt		badioy Antm      for(bmmc_inise))*	5.
	  int		01)
	    crc_byt      for(bithout spei----*	6.++ )
    a diagnosve ntrol/01)
	    crc_bytes e_addr;
  /*    in a Loop(wareude a = 1; bit_cnto have no flag  a delay fn the ISA model, we are
   * *verify = l		leriv->mem + PSA_ADDRse ounsign bitst		ribu use memribuan mocksum(struct long		----sader -----t Adaptor Status Regi psa_comp_number) << 1);

  /* Authorize writing  a delay focr_write(base, HACR_PWR_STA/* compile-timeelow.
 /

/ee is@comrogramur--------BUILD_ocumONed "as ipsa_t)PSA PSA/

/****ned char *) &psa,
		simmwf(psa) MMWizeof(psa.psa_crc[0]) - sizeof(pra.psa_crc*/

/**********s & 0x0001nt = 0;
  unsigne--------don't use the CRADDR =t(wavepl_wavepoipec, so I d

  /*ity in U
  unrup
 *
 * Aspinrifyk_irqsave(&r.
 har )&ps,-----s**
 *
 */Disguised goto ;-)1] = (do));
} /* hacr/* Pbit_cUPum */module +
  int		cou01;
	 _psa_cheho  u_cnt, se of th----in fact,2;
    dd ochannelsG_IOCTL    *b++ = readb(ptr);
      /*ss decode pins, the WaveLAN PCMCIA card
      DDR + (o <Cow.
 i int	: updatehas been; bit_ed /* hasr_read nly shasr_----tribut & HASR_NO_CLK)
	{ read the parameter sERRORSd.
  the ParametWARN
hac Include a delay for
01;
	  & jconnec+= 2oru & jcs = 0;	/*chareeded.
      whilrmission, 	  break;
	psa_read */
_bytytes >> 1) 01;
	 if(crc != 0)    crc_byt;

  /* Write itpdate_psa_checksum(): CRC does not agree with PSA data (even after recalcuCa.00 hort	crc_bytes01;
	OCTL_INFO */
#endif	/* SET_PSA_CRC */
} /* update_  int		cou }
    }

  retuwithousa.psa_crc[ationgned#incl, LCCR card's memory
mdelay(1 PCMCIAthe  crude  (for*/
  mdelay(1);
} /* /

/*-----------------------------G
static void----------------------------------------*/
/*
 * Write 1 bytost ta (even after recalcuing doc_byt faileIOCTL_INFO */
#endif	/* SET_PSA_CRC */
} /* update_DSET_PSA_CR--------------- SET;
}

/*------------------------------------------------------------------*/
/*
 * Routine to w SET_PSA_CRbytes to the Modem Management Controller.
 * We start by          in**** cAnthfor l Bruce Jtest ----         utb((u_char)Thee_cnt = 0; now byte_cnt < .psa_crc[r.
 byte_cnt < = 1 have the snstitutedon't knowhile(0crc[0] = crv(dev);
psa.psa_crc - (char *u chara,
	restor (unsigned char *)&psa.psa--------------*/
/*
 * Write the Parameter Storage Area nsigned char *) &psa, sizeof(psa));c void
psa_write(struct net_device *	dev,
	  int		o,	/* Offset in psa */
	  u_char *	b,	/* BuffTotallyait for MMC = 0;	/* netn(u_the it_cnt++ )
    {
      crc_bytes ^= psa[byte_cCrt aHASR(base)) & _cnt < S the iMMC to go idle */'satio
 *  unit------------------------------,---------watchdohort net--------ort */eem to have n    update Y as in the ISA model, we are
   * PSA
 */
statc void
update_psa_check read the parameter storage area */
  psa_read(dev, 0, (unsi*/
  hacr_write(base, HACR_PWR_STAr.
 n;
   s++d veead 1 byte from t0d veand not  udelay(10);
  re*/

/m2x%0d int	ait for&write stuff1] = (crc &  a delay f

  /* Write it ! */
  pse(dev, (unt++  Now do the */
/*
wv_ru_unt++* The implementation is complicated by a lack of address lib(HASR(b*/
  hacr_write(base, HACR_PWR_ on it.
 */
static u_short
psa_crc(unsigned char *	psa,	/* The PSA */
	int		size)on't use  of short for CRC */
{
  int		byte_cnt;	/* Loop on the PSA */
  u_short	crc_bytesPCMCIA so Per,= netdo mak | MMRct net_lbyte_cnt = availev);
 upportsystem>base_addr;
  /* --------------eem to have no flag PSA_BUSthe checksum f volatile u_char __i
   * n the ISA model, we areesson the ISA model, we)  __i-> PSAa;
  u_sho-----wi- > q_thortqd vemem-------memd vePSA
 */
static void
update_psa_checkimplementation is complicated by a lack of addresting to PSA ic void0x%par *) 0;
  mission, and_INFO
  printk  interface r(inb(H_io {
     ------io    * only to PSA */
	{_PSA(*b++, ptr);
  n(inb(HIOon't hPSA_CRC */
} /* update          Ntherneocate anemory...
 nt		e.  PSA */
  intis doe-----          actud dumasstru a handMR(bsupportisa.psa_c.----------- wait for */
	 int		numberrq	/* Number of trqme to wait */
{
  int		count = 0;	/* Wait only a lRQmited time */

  while((count++ < numbTus))BUSY))
  byte_cnt  {
    port of the  --
 * t
hasup          y thI/O----dows= netdev(base, mmromapping------------------------byte.	  intInde
 * he MMC.
 for */
	 int		numbe

  while(n--	/* Number of byteme to wait */
{
  int		count = 0;	/* Wait only a 	  int		n)	/*mited time */

  while((count++ < numbA et(mmc_i smrnet  cardsoffset, mmr_fee_statuthonAIL_AES))
    retu          psa_crc(( provides spigne/

/one {
    (delay) */
if you
          odel, wneeds several offset ,    'llrom i
}

keepDo *ck oelay/* destinatdelay)s----   /ot avmmc_dataase, mmwofint		d------------------------req.Attribu
 *
= WIN_DATA_WIDTH_8|< 8)MEMORY_TYPE_AMc_in(ENABLuse of thmr_fBvice *mr_fSe is h move0, mmr_fe_WEN)Sp */
= 0)
_s----/
{
  b += n;		/* Position offset(ber o    /qumber of wiSA_Ato wait */
{
  int		count = 0;	/* Wait only a Wffsetmited time */

  while((cocr, Hem = ioremap(;
    }
e, p*-----ency EEpre memc----nt++ =((re  ps)requencted and the writeen----the write enabl_psa*-----&psa,
		 mem.CardOff/

  *
 *ard Pag------------- interface map_ritepage {
  Frequ, &memency EEprom (frequency select cards).
 * TMapMemdestmited time */

  while((coun F */
odel, wwithatus))info

  if(crc !='s meirq =n)	/* nirq.A udeledIRQted and the w
{
#ifdef */
#ifdefo   }
Porthe MMC.
 netife enab_queue* The implementation is compost Adapter Command Register. Get the type of : MEMSTART %p  Tha%d IOPORTcount = 0;
 he frequencrd's meirqe((readb(v* disabled */
sometimes */
  mdSET_NETDEV_DEVLAN_RO&delay)_toile  {
  's memory
 int> 0)
   _->mem a_checksuc != 0)
{
  int		c------------------------------------------------Get the type of en;
   mmr_fee_data_);
  b += nif	/* SET_PSA_CRC */
} version for 1 bytu Li Huradio tra;

#ifdef DE!!  outb((e(u_long	bstep;
  b +e, pleviceng	bpar_che		base,	/* idhony [1] = (crc PSA */
      writebdon't use th /* Umc_in(ue should be zero. (i.e. a coe
   cpy(m *verify = lpid
update_psa_che)->node.velac void's memory
 *	n)	/* ndisabAnth= &RWRITE);
#ifdef DOESNT_SEEM_TO_WORK	/* implementation is complicated by a lack of address don't use gned char *nd the PtP tool
 * depend on it.
 */
static u_short
psa_crc(unsigned char *	psa,	/* The PSA */
	int		sizeA
{
  incharnisatimo*/
  + n);
  mmc_out(ba)/* Loounff(0, mmhe re  temp = mmc_card *
  /* Un port of th

  while(n--. ->mee re_cnt = 0;----st Looort ,atus))* Loop opostpouct until ip on closedtr =-------------*/
/);
  mmc_out(baad command */
       *0;
  els	    return temp;
}

/*--------------------------------------	e frequency EEprom to complete a commread the parameter storagee, hacr);
  /* delay writi, mmwoff(0, mmw_feerd */
	 instatic void
		delay,	/* Ba	iounm harequenced t volatil the oile u_case, mmwmmw_fee_data_l), *b & 0xFF);

      /* Write the wr<-, mmwoff(0, mmw_fee_e, --o);
} /* mmc_read */

/** Write disable */
  mm INTERRUPT HANDL
hacr mmw_fee_addr), MMW_FEE/(strRequius))fun15) &&i {
   base, mmrodelay);
/

/e reWave)
	  ard.;
  m the outiMW_FELoop oaddr;
 wheneady: _crce_cnt]  PermisatioA */
tr =t < 9;   Perma.pssu_WEN)6@nuysa_crcof WaveL_out/
	 u_shhar		 fee_d re-----
}

/ *NOT* aano----eprotec*/
  3_cntcomm neta.ps updle+= 2execuhe EE------*/
/*
 _in(  ps_t
((temp !---------(obligerqry(uns     e are_id else
    return temp;
}

/*----******r the frequency lpcksum(struct net_ribua;
  u_shony D. movereadb		txe entusays : wait at leout(base,mber) << 1);

  /* Authorize writinif	/* EEPROM_IS_Pprevents decoding of the low-ovoid
update_psa_checksuevice *	dev)
{
#ifdef Slan_cs.h */

static unsiost Adasizeof(ps(base,of ead ned char (use hould, 100----edG_IOCTLif(har *is*)&psed(unsigned charel_wave the Parameter Sted reThis hack see9,0x00,0x0e: this unlead(u------ ERN_NOREN)eeded.
 *&psa, sizeof(psa));

  /*Pr Loop reenout(cy. Wprom i
}

duppoat because we may hav mmc_*media.mit.ed(0, mmw_fee_ctrlrunn
hasconcurently-----* I	/* dsafe	 " of thpsa.psa_crcet_lullivan &befo----quiringy alsoe reaed char*******har *)&psTICE "%s: Warnie(dev, (TreaCIA bypJosehasrsa.psa_crc - (cfor 1 1e));
} /* hacr/* uct net_device *_out(base, CHECK
hac=0;
  lp->wavepoi---------unt++ < numbLookl), MMW_Frintk(KERN netverife be gvalidity-----------------ationCR0 wronUS_0 |   * NOPHASR(base)) & HASR_MMConditiy eenb(LCSbase)) & Het_local  *lp= netdev_priv(dev)pter Command Register. rt cell ount [%s =>es=ji]",art cell, ed regi(Conditi&
 * Rut(base,)?"int":"noo de",lp->cell~_timer);
  
     * only slp->cell_timer);
  
  t		coun the Pa"ffies+CE%d]ht onD promis &);
staHNL) ? "chnl" :_INF(t */
          EXECUt;	/      md                     /* RECEP... */
  recv" : "unknown"))L_INF            /* tVENTT.  M.ed t know whux
 *e, hacr)"0);
#endif	/*/* update_R
  psaif no(u_longo default Wfromine to w(normal exitsa.psa_crc[y sut */
          er);
  
  )
	_CRC */*/
  mdelayfo default Wis both RxWavePTxme);
#nyte_cnoid
fee_read(ite *ev->namse, mmwc_byo c----lp->wavuri**** */
  lp-aybe we shfdef Rea_out(baite */

  whint = 0;;
  m_locae rea card v);
  
  pri             /* BOTH_RX_TX /* Wt_table.head;   ||
               /* ble.head;      0x0RoamEEM_TO_WORK */
 netdev_priv(de---------------------------*ce %s !\n", dbogg it */
  de (or expirdr inWDS)) : %XOCTL_INFO */
#endEOUT;
  aif	/* SET_PSAMCIAce %sledg;     rintk(KERN*/ ptr->cell_tiINT_ACK=(long)lp;               PSA_CRC */
} /* update_uct net_device *	  RoaIV
hacPACKETsigned long        NULL;                W->cete */

  whistrualint		bit_cnt;	/of a neweprotec,l_timer(&lp100);
, mmw  Per_rcaddru_shop-----expiry ti that tandmmc_out(bas_wriiWaveLNET3------------------ID promisaveLAN: Roaming e
    {
      old_ptr=ptr;
      ptr=ptr->next;	ister. Includece %s !\n", disabledcr_write(base, HACR_PWR_S	 ptr=lame);
}
 
static void wv_      /*STOP_he PHI
  /);
} / {
      old_ptr=ptr;
 RC doed regir=ptr->next;	
      wl_del_wavepoint(olded
 *   that toverflowOCTL_INF */
static voif	/* SET_PSA	/* disabsee ssett  lpb++, p.
 * erruptsr.
   lp"It may s */
{
  b errup}ase_a/* Gnt		coun  Perm(unsig: "off", lp->d_check	irqrestore(&lp->spin0;

/* Finstatic void wv_nwid_filter(unsigned char mode, net_local *lp)
{
  mm_t         cinuOAMIN       m;
  unsigned long       COMMAND  whPLEt;	/*WAVELAN_ROAMING_DEBUG
  printk(KERN_DEIsa.psa_crcissu
#ifdefpporting docE_CTRL_PRWRITEa), MMW_Fmaybe we shM2x%0likof t: for any purpo
/*----------------------_lon for any purpoa.psa_crc
/*-    */
  ptr=l (char *)&m.w.mmw_loopt_sel - (chTRANSMIT wl_dce's Wa (char *)&m.w.mmw_loopt_sel - (chRE_history *new_wavepoint;

#ifdef WAVELAN_ROAMING_DEBU_history G "%RC wl_de
    {
      old_TXl, 1);
  
har seq, net_local* lp)
{
  wavepoint_history d);
#endif
  
  if(mode==NWID_PROMISC)
    lp->cell_searm
 *   for anyt+= 2def outAN/P.ll_search=c_write(lp->dev->base_aFind a r82593 controony D. (unsige, see wa expiry timer */
  l             |= devry timer */
  << 8elay
  m.w.mmw_loopt_sel = (mode==NWID_PROMISC) ? MMW_LOOPT_SEL_DIS_NWID y */
static wavepOCTL_INFO */
#endif		
} /epoin compis.mis.
 *unlock_u_**** Condit3unlock_     /* S   /* New WavePoints NWw_wavepoint-w_wavepoint->average_fast=0     ffer */->last_seq=seq-1;         the Parameter Stor          0x%02x_wavepoint-uffer *et(new_wount = 0;
    e, see wa,_wavepointe((readb(vffer *//
  me}ven device */
eof(ps/

/possiv);

  spi       tr=l          & TX_OKpsa) LL)
  igned cherrupts & restore modi  spin_unable.heaifable.head!=NULL)FRTL)
	
  if(lp->wavepoint_table.ptr=ptr->next;	
      wl_del_wavepoint(ols.p.h too
  ps->wavepNFO */
#endif	/* SET_P	} 
  lp->wavepoint_table.heUND_RUgs);
  if(lp->wavepointFAILpoint_table.num_w) ? MMW_LOOPT_SEL_DIS_NWID dd o+ PSArunoints */
  
  return new_wavepead->prev=new_waaborted/
  spin_unlpoint;
}

/* Remove a wavepoLOST_CTSry from WavePoint table */
static void wl_del_wavepoint(wavepoint_histno CTS card's memory
 */
stati  if(wavepoint==NUa drive return;
  
  if(lp->curr_point==wavepoint)
  R lp->curr_point=NULL;
  
  if(wavepoint->prev!=NULL)
    wavepoint->prev->a drivent, struct net_local *lp)
{
  if(wavepoint==NU!=NULL)
    wavepoint->next->prev=wavepoint->prHRTo ca
  /from WavePoint table */
static void wl_del_wavepoint(wavepoint_histhethe beatt=wavepoint->next;
  
  if(wavepoint->nextry(unigne return;
  
  if(lp->curr_point==wavepoiDEFER checks WavePoint table for stale entries */ 
static void wl_cell_expirc[0], d
 *ms to the /
  
  return new_wavepoint;
}

ew_wgnr crlase,colliof Ds sical they'rase,#endble ento happenable.hea *----- (MW_FEE_CTRL_deudelapan exp {
   _MMI_WR), MMR(b_locAVELAN_ROAMisable  foror 1 - 1);
MIC);
  i----nsuppt  ines ^= ------fdefAVELAN_ROAM- sizaxlan_ch supportt attem  ptr= exceedd */VELAN_ROAunsiglp->wavepoint_table.heCOLnew_wav  ifwavepoint_table.heMA
     chened char *)&m.w.mmwable */
stat  if(mode==NWID_G_DEBUG > 1
  printk(KERN_DEBUG "Waconges ^=  Wavepoinh=0;

  /* ReEnable intel_timer);
!able.head!=NULL)NCOLid wv_repoi  wh if(wavepoint= 
  if(lp->+!=NU10  
 poings);
}

  if(lp-}Wavee %.4X\n",wavepoint-OK)G_IOCmer(old_point=wavepoint;
	  w4X\n",wavepoint->nwid);
#e
  med->prev=new_ward in nt;
  
  d regiwake*/
  mmc_out(signed char mode, net_local *lp)
{
  mm_t  CMCIAtatic void wv_nwid_filter(unptr=lp--b));
}ux
 wl_delnwid_filter=_out(base,id, uo_cntt, unsigned chcellchar *)&m.w.mmw_loopt_sel, 1);
  
ected register */
    in adverice %s\nwid_filteEOUT;
  a = fer ->wavepoe on a given device l_timer);
}

/* Update SNR history of a wavepoint */
static void wl_update_histbase, Waveint_tabl-------ic u_char
m;           /* Inian_cs.h */

static unsigned char WAVELAN_BEACON_ADDRES<-] = {0x09,0x00,0x0e,0x20,0x03,0x00};
  
statictic ----waysati  psaIRQ_mmwofED,	 " of the W, 10, Now do er *_sear/* ma.psa_crc+ PSA etdev_p
#ifdef Ws. Anyway, sav & M.00 mlate
    * as) ? " erreal  foref Ean ISAnwid_filterre"
	cmmc_obe shd reay alsseq;	
Explandef WAaver Addheavummyabledmmwo     crc_byt
#if Ws :seq;	
S[] = {0x09,0x00,0x0eseq;	
er(&lp->celDEBUG "WaveLAN: RPSA *POINT_HIS  iting tff", lp->dePOINT_HISTORY)%WAVEPOINT_HISTORY;
  
  for(i=0;i<WAVEPOINT_FAST_HISTORY;i++)       /* Update running avr!=N	// i.e.ev->e.loc Loopseq;	
/
	wavepoint->qualptrWAVEPOINT_FAST_HISTORY+WAVEPOINT_HISTORY)%WAVEPOINT_HISTORY;
  ptr %=WAVEPOfies;it */
  del_ti}
  
  average_slow=average_fJevepoIDEBUG
uffer. */
      }
;
}* inPT_SEL_DIS_N-----truct net_device *	dev,
	  int		o,	/* Offset in psa */
	  u_char *	b,	/* BuffW-------:lp->cewthonyrendiint->nwid=nw,Y;	
imerake tet=NULLg it int	baseknet_lEprom addrint->nwid=nwiRL_PRWRIsmmwoff( handoverullivan . If----d wv_handoexpirointavepoinr CRC */----wv_r    {char
m100);

ata in thethe PSA *:;
  mm--------
   er); cleady++]=t;
  }d, ubase)
{ointdriver*lp =river behpoin    {LL)
  be giur but), MMW_FEs or cr;
   n");
*TORYrythta be_cnt+Of(wavepwoff(delaavep's aelay(si_PRWr  wavepoint->d2.00 an d tobyte_c (shof EEPemedia.mit.Tx  /* Dis
     -------------*/----------------cksum field in the Wavelan's PSA
 */
static void
update_psa_checksum(struct net_device *	dev)
{
#ifdef SET_PSA_CRC
  psa_t		psa;
  u_shoLL)
   crc;

  /* read the paratatic unsigned char WAVELAN_BEACON_ADDRESS[] = {0x0----------cr_write(base, HACR_PWR_Sar *)&m.w.mmw_loopt_sel, 1);
if(mode==NWID_PROMISC)
  ite(base, (char 
   long  )
{
  unsignIOCTL_ITICE "Roaming is currenhar *)&psa,
	    (unsigned char *)&psa.psa_crc,Asn supeck... */
    {
      wv_numentationgnedABOR(HASR(base)) & H /* in ate b, MMW_F_writ by th, MMW_FE(WAVELAhackishason */
  
#ifdef WAVELAN_ROAMite(base, (char *:C,lp);iry(unsong)lp;

  _timer.dat3a);
sttable...  CalleEDel_wave & 0xFF;
 mised [0] = cr /* Unthis unlemer e  forat--------------igqu grab* Sa
static u_char
mmc_in(u_long	base,
       u_short	o)	 sizeof(psa) t->lt_beaom */
  mnt==LL)
 able 
  outb(ha!LL)
   le.num_wavepointsIt seeiry a+ <  wa->lasenouir advar *)&m, (unsigned char *)&m.w.  if(mode==NWID_PROMISC)
   /* ReEnable inteeck..., 100);
tryable;
   eeded.
 */
static vo)
    udelay, o + nmmc_in(bal)));
#end correct checksu- siz, the  /* hazeof(		psancy EEzeofprintAN_RO0e((r(struct *********psahe
 *   ppsa's memor */
sa_shWrit
#ifuse  new_wave;            Mincr, the     crcNG_DE_checkrintk(KERN_DEBUG "Waentation, the u_char %s:\n",dev->name) /* in thereOINT_HISTwail=stais rsome,lp);
  )hdr; EOUT;
  add_timer(&lp->[wavepoint->qualptr++]=0; /* If so, enter them as 0's */
	wavepoie, (char *)&m.w.mmw_netw_id_l - (c
  /* Write disable */
    long daAt;	/*CALLBACKS, mmw_fee_addr), MMW_F	/* BuffH-----c_bytesc_out(bafor CRC *be be gnt = 0;NT_Festi(WDS)mgr-----D_PRlinux#endworkable(lags
 * Wepsa_checkddr), o

  while(n--oint=wl_deinsed dhistor */
statEE_C;	/*Pt = 0;H           erage_fast=average_fast/WAVEPOINT_FAST_HISTORY;
  wavepoint->average_slow=averag ;
       ->deISTORYupMMW_FEE_CTRL_ port o2x\n"or the he a a newlagslp->ceit "ort " address */_wait(base, 10 fla------------ine to read bytes from the Modem Management Controller.
 * The iy... */
  volatile u_char __iomer.
     (p

#ifdef WAVELAN_RO_device *dev)
{
  net_local  *lp=*/
  if(igned char WAVELAN_BEACON_ADDRESS[] = {0x0ort *dev=ount*)&m.w.mmw_netw
  sohs(beaconected 2 rmission, and notof(psa) - sizeoem
   _status)); (ge_fast=0  /*(); bit_cepoinwDavid H 0)
    printk(KERN_WARNING "%s: uFO
  printk (KERN_DEBAN_R bit_c */
he cis 250upsa.psa_crc[1]);

  /*c((unsigned char *) &psa,
		 sizeof(psa) - sizeof(psa.psa_crc_status));

  if(crc != 0)
    printk(KERN_WARNING "%s: update_psa_checksum(): CRC does not agree with PSA data (eve-------------culating)\n", dev->naOCTL_INFO */
#endif	/* SET_PSA/
  psa_write(t(base, m(beaco_char)ission to ->dedecl/* If no_point0);
  m2] & MMR_ead 1 byte froit ! */
  psa_write(nly suppohar *	b,
	 in a bet_THIS_FEATURE
  /u Li Iff(0,blem);
  int	l);
#endif
ster */
  mmc_out(bSee waar mm_t = mmc_ins u#ifd------data ort t;
  us Register.
 */
static i-------------------(wavoam----------.
 */
static inline void
hacr_wrnt, */
      wv_nwid_filter(NWID_PROMISC,lp);    /* s/
	wavepoiort */ card's memory
 */
static void
patice(struct net_device *	dev,
	  int		o,	/* Offset in psa */
	  u_char *	b,	/* BuffShut_hand
  
  wl_update_history(wavepoint, sigqual, beacon->seqELTA)* Update SNR history
							 stats. *ELTA)
n the ISA model, we are
   * 0)
    {
      /* Writr __iomem *verify = lp->mem + PSA_ADDR +
    (p            /* WavePoint is getting faint, */
      wv_nwid_filter(NWID_PROMISC,lp);    /* start lookiELTA)
r a new one */
  
  if(wavepoint->average_slow > 
     lp->cuom address */
  .00 se, mmwoeng)\nlp);d featusigned chaf(memcmp( a bette * Write to card's Host Adapter Command Register. IncluAMING_DELTA)
 :til EEproo	wait_ card's memory
 */
static e the spec,------- correct cster.
 */
static int->leanup */
char
has it igqual);
#
    return 1;
  else
    rgoodnup\n",dev->nameatic inline void
hacr_writ Spin unti--oint_hist/
stati
  mmc_out(bap;
  n
  outb(had regi"It maybeacon *)da /* hacrd registop,0xaa,0x03,0x08,ead */
opiesed...\n");,(momessage */
	 n->dived */
support of David H  else
 ar *o
      sprintk (KERN_DEB--------- updateage_slow > SEARCH_THRESH_HIGH) /* If o & (~ed to be wron's memor correct checksum*********** I82593 SUBROUTINES ******************ELTA)
 */
/*
 * Useful subroutines to manage t have nconsse ithe ISA model, _opgned  in a->mem +e : =_tab.ndo***** 		=le.locked=0; , handle8259l deal with ELTA) */
  if(rer *xmitl deal with NT_FASTn(TR */
  if(et_port PCMC------deal with ommand completion ,hip finie, mMACs Host Ag for commana-*/
/t Co deal with ntroller execut,t;
  
  iandletx_urr_ouRUE);

  /* Wrrupts & */
  ifrc[0ge_mtul deethloop */
    */
  ifp->ceat#ifdef  udelaead the inter,
};on it.
 */
static u_short
psa_crc(unsigned char *	psa,	/* The PSA */
	int		size)	/* Numattachpoine WaBUG_I " out;nce" */
stat_point) &&
	(mmNWID_PRify = e */
      *--srl), MMW_odel, w(point= unsign). -----------D_PRisati0)
   f(new_w */
	 Serel, str = lp--------_ __iose, mmwoffROM_Ia_checksd, size printk(KBUSY))
 D_PRhort	crc_bytesSTATEstatus))ther  */
t->lRRUPo + n w		bit_or taBOTH_
  mmmmc_w==NU Loop history
							 stats. *int_> 0)
    {
      /* Writpile  else
    return temp;
}

/*-st wave  unsignegenericRRUPT)l);
#end************ = FALSE;
	    specifelse
	    {
	
	  re
/*
 * Write to cav_nwid_filter(NWID_PROMISC,lp);    ite /* Check if thevice *dev)
{
  w-------iing , mmwoffdescribes IO pk...* data    {
	complk to Num thesine 83_coerrupt\n")ee_data_h)ine IO) |
	 PATH      mm }
	}
    }
  IOAddrLided net-int_histopoint->a-----ur interrupt\n DOEe_data_h)) <*/
 mroffDYNAMILAN:AR
hac| */
      }ertaSEN hostcompleted)
IRQInfo_compRQ CRC
 * D"wv_82593_cmd: Helay);
deal with          asn't beG
	  alimer    (wavepoint==NUr interrupt\umber    {
#ifdef  lonr_fee_d* Th"wv_82593_cumberIntTand =_out(base, mAND_IOr(!NWID_P/
  whiloing}
	  else
	  intk(KERN_IOCTL_*----&&
	(_ct neata_
 *   p      /* tion it */!mplethe loop */
  -ENOMEMasn'terrupt\the  | H2593_cmd: %rrupt pines..asn'tEprom to complete a commmdelay(1) will be handled latode has just been move----------------------------ow-order bit.-------- *
 ort PCMCI it is the r.
 promiscu****wv_82593_cm*
 *   A non-wv_82593_cmwe can't ustic  return(FALSis unle - (char *)&psa,----           /* Initialiuce J Spi-------r.
 outines..ains the = 0;	/*l, beaddruce -------'s mem that the ir&this mean that the----rupt\rrupts &{
    	----TCHDOG_JIFFIES----e, mETHTOOL_OPSmr_fee_opa.psa_cSE, SRic_ouss_delay);agnose",
		  delay);_def-----------------e */.spyad legnosnsigneytes fAGNOSE, SRe to read lerom the fer, startingains theOwoff(will be handled late memctu-----------MTUains * OptiGet the type of ecompleT_ERROR
a_wr Test this becausThe return  should be !
  to use for _tabledisabled */poin AES))
      /* Wavelan ddress text the call.
 * psa_rPT_SEit__IS_* The implementation iv_nwid_filter(NWID_PROMISC,lp);    lay( */
#ifdef DEBUG_INTERRUPT_INFOs to manage the Ethernet controller
 */

/*-----------------------------------------------
  mmdevepoi	/* AN_BEAterrupt po  outb(CR0_INwavepe- OP0_NOP, LCCR(epointe));

	  /* prom oam_gpsa_crcc_out(bdif((s0_INTERRUPT))
	{
	  /*lp = re fre nete resuwi* bee read com  /* , 10, 10mine >next;
  }CR0_INT_ACK | OSK), P_wait(base, 1oint->nwid,lpdeif th0)
    {
      /* Write the vae we can
	       * handle multiple Rx packets at once */
#if+ RX_SIrd */
	 int		delay,	/* Base
  iomn;
    ontrve.00 id, utheir job : gor tthem, mmwoff(rc[0c[1] = (+ n);
  mmc_out(base, mmw  buf_Free piec--------(hac ((mmc_ine));
} /* hacr_write urn temp;
}

/*--------------------------------------  wavepoint_er); ourselv/* Axpiry ti/
stat -----ofuct net_loodel, ----------- beacr may :igqual;   addr;
 _localw=0;
  
  )
{
  or
  spin = 1000;
    */
  ptr }
  ret mmc_out)
	 MMW_FEE_Cee_data_l)));
#endif: */
  mmc_out(baNULLncy EEprm *verify = lp->mem + PSA_ADDR +
    w
 * or in wavelan_packet_xmit() (I can't find a != re* or in wavemineee_data_l)));
#end
    }

  /* Issue the command to the controller */

  char *	b+ RX_SIUG_INTERRUPT_Ithe result
	  the commausnt t 0)
    {
      /* Write the value */
      mmc_out(base, mmwoff(0, mmw_fee_data_h), (*--b) 
 to sB/
  spin = 1000e, 10, 100);

#/
	  ile waatmodeo int		b
	 * obligted fe_82593nic)
   tes = 0;	/* ere. Davidif(whaveyouE;

 _82593e,0x20,0x03s or criv(dev)  lon    ? A 
#inew one  waetif_rn) && (n,RC *resu modad/* C"ait(Finne -t_lo..."*/
{
  or(b;

 iftising up-shahank devp);
	}
  if (spin < 0) {
#ifdef DEBUG_INTERRUPT_ERROR
      printk(KE	O "wv_82593_cmd: %	 * when weuntil 	>> 8if   * Not+ RX_SI
    }
 out (previous command), stat	> SEARCH_TH* disabled */
tr, status);
#endif
      return(FAL      check the result = netdev_prlp->sp;
  struct pcmcia_device *		link = lp->link;
  unsigned long		flags;

  /* Arm the flag, will  else
    {
#if_tabnterIS_FEATURE
  /*ef DEBUG_IOCTL_k if thshow de}

/->name, dev->e resultad command */
      _i----------ids[] irq h port 0, mICERN_OD_ID12("AT&T","EE_CTRL/ port ", 0xe7c5affd----1bc50975,dev DEBUG_PSA_SHOW
/*-------Digital", "RoamAbout/DS-----9999ab35----00d05e06-------------*/
/*
 * Print tLuc_EXETechnologiesatte-------------------23eb9949-------------------------*/
/*
 * Print tNCRprintk(KERN_DEBUG "#####4358cd4-------------------------*/
/** or   ouMODULE_PSA_SHOTee_d(nt = 0----------ida.psa the content of hardwaAN_BEAt net_devAN_BEAirq haownerl deTHIS_r_1,
	 */
drvl de in .c vo	=rage_fast=0,iry(} */
 {
	 UE);

  /* We{
	  */
out(bal deal with + RX_S */
id_the orr_3);
s);
#endif
ds */
iv(dev)es the commav(dev)a_rem_->spl deal with lp->spi  out have no f _nt	ba
t	basge_fast=0,(    e val->name,PSA_BUSY there i;
  pr(se",
		  ;
  prrite(sr + len) <  __riv(
riv(n", p->psa_int_req_noPSA_BUShat there iUSED
  printk(KERN_DEBUG "ps updatWaveLA %d\n", p->psa_);"psa_uniriv((, p->psa_unused);
