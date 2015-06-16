/****************************************************************************
 *    ixj.c
 *
 * Device Driver for Quicknet Technologies, Inc.'s Telephony cards
 * including the Internet PhoneJACK, Internet PhoneJACK Lite,
 * Internet PhoneJACK PCI, Internet LineJACK, Internet PhoneCARD and
 * SmartCABLE
 *
 *    (c) Copyright 1999-2001  Quicknet Technologies, Inc.
 *
 *    This program is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU General Public License
 *    as published by the Free Software Foundation; either version
 *    2 of the License, or (at your option) any later version.
 *
 * Author:          Ed Okerson, <eokerson@quicknet.net>
 *
 * Contributors:    Greg Herlein, <gherlein@quicknet.net>
 *                  David W. Erhart, <derhart@quicknet.net>
 *                  John Sellers, <jsellers@quicknet.net>
 *                  Mike Preston, <mpreston@quicknet.net>
 *    
 * Fixes:           David Huggins-Daines, <dhd@cepstral.com>
 *                  Fabio Ferrari, <fabio.ferrari@digitro.com.br>
 *                  Artis Kugevics, <artis@mt.lv>
 *                  Daniele Bellucci, <bellucda@tiscali.it>
 *
 * More information about the hardware related to this driver can be found  
 * at our website:    http://www.quicknet.net
 *
 * IN NO EVENT SHALL QUICKNET TECHNOLOGIES, INC. BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF QUICKNET
 * TECHNOLOGIES, INC. HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *    
 * QUICKNET TECHNOLOGIES, INC. SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND QUICKNET TECHNOLOGIES, INC. HAS NO OBLIGATION
 * TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 ***************************************************************************/

/*
 * Revision 4.8  2003/07/09 19:39:00  Daniele Bellucci
 * Audit some copy_*_user and minor cleanup.
 *
 * Revision 4.7  2001/08/13 06:19:33  craigs
 * Added additional changes from Alan Cox and John Anderson for
 * 2.2 to 2.4 cleanup and bounds checking
 *
 * Revision 4.6  2001/08/13 01:05:05  craigs
 * Really fixed PHONE_QUERY_CODEC problem this time
 *
 * Revision 4.5  2001/08/13 00:11:03  craigs
 * Fixed problem in handling of PHONE_QUERY_CODEC, thanks to Shane Anderson
 *
 * Revision 4.4  2001/08/07 07:58:12  craigs
 * Changed back to three digit version numbers
 * Added tagbuild target to allow automatic and easy tagging of versions
 *
 * Revision 4.3  2001/08/07 07:24:47  craigs
 * Added ixj-ver.h to allow easy configuration management of driver
 * Added display of version number in /prox/ixj
 *
 * Revision 4.2  2001/08/06 07:07:19  craigs
 * Reverted IXJCTL_DSP_TYPE and IXJCTL_DSP_VERSION files to original
 * behaviour of returning int rather than short *
 *
 * Revision 4.1  2001/08/05 00:17:37  craigs
 * More changes for correct PCMCIA installation
 * Start of changes for backward Linux compatibility
 *
 * Revision 4.0  2001/08/04 12:33:12  craigs
 * New version using GNU autoconf
 *
 * Revision 3.105  2001/07/20 23:14:32  eokerson
 * More work on CallerID generation when using ring cadences.
 *
 * Revision 3.104  2001/07/06 01:33:55  eokerson
 * Some bugfixes from Robert Vojta <vojta@ipex.cz> and a few mods to the Makefile.
 *
 * Revision 3.103  2001/07/05 19:20:16  eokerson
 * Updated HOWTO
 * Changed mic gain to 30dB on Internet LineJACK mic/speaker port.
 *
 * Revision 3.102  2001/07/03 23:51:21  eokerson
 * Un-mute mic on Internet LineJACK when in speakerphone mode.
 *
 * Revision 3.101  2001/07/02 19:26:56  eokerson
 * Removed initialiazation of ixjdebug and ixj_convert_loaded so they will go in the .bss instead of the .data
 *
 * Revision 3.100  2001/07/02 19:18:27  eokerson
 * Changed driver to make dynamic allocation possible.  We now pass IXJ * between functions instead of array indexes.
 * Fixed the way the POTS and PSTN ports interact during a PSTN call to allow local answering.
 * Fixed speaker mode on Internet LineJACK.
 *
 * Revision 3.99  2001/05/09 14:11:16  eokerson
 * Fixed kmalloc error in ixj_build_filter_cadence.  Thanks David Chan <cat@waulogy.stanford.edu>.
 *
 * Revision 3.98  2001/05/08 19:55:33  eokerson
 * Fixed POTS hookstate detection while it is connected to PSTN port.
 *
 * Revision 3.97  2001/05/08 00:01:04  eokerson
 * Fixed kernel oops when sending caller ID data.
 *
 * Revision 3.96  2001/05/04 23:09:30  eokerson
 * Now uses one kernel timer for each card, instead of one for the entire driver.
 *
 * Revision 3.95  2001/04/25 22:06:47  eokerson
 * Fixed squawking at beginning of some G.723.1 calls.
 *
 * Revision 3.94  2001/04/03 23:42:00  eokerson
 * Added linear volume ioctls
 * Added raw filter load ioctl
 *
 * Revision 3.93  2001/02/27 01:00:06  eokerson
 * Fixed blocking in CallerID.
 * Reduced size of ixj structure for smaller driver footprint.
 *
 * Revision 3.92  2001/02/20 22:02:59  eokerson
 * Fixed isapnp and pcmcia module compatibility for 2.4.x kernels.
 * Improved PSTN ring detection.
 * Fixed wink generation on POTS ports.
 *
 * Revision 3.91  2001/02/13 00:55:44  eokerson
 * Turn AEC back on after changing frame sizes.
 *
 * Revision 3.90  2001/02/12 16:42:00  eokerson
 * Added ALAW codec, thanks to Fabio Ferrari for the table based converters to make ALAW from ULAW.
 *
 * Revision 3.89  2001/02/12 15:41:16  eokerson
 * Fix from Artis Kugevics - Tone gains were not being set correctly.
 *
 * Revision 3.88  2001/02/05 23:25:42  eokerson
 * Fixed lockup bugs with deregister.
 *
 * Revision 3.87  2001/01/29 21:00:39  eokerson
 * Fix from Fabio Ferrari <fabio.ferrari@digitro.com.br> to properly handle EAGAIN and EINTR during non-blocking write.
 * Updated copyright date.
 *
 * Revision 3.86  2001/01/23 23:53:46  eokerson
 * Fixes to G.729 compatibility.
 *
 * Revision 3.85  2001/01/23 21:30:36  eokerson
 * Added verbage about cards supported.
 * Removed commands that put the card in low power mode at some times that it should not be in low power mode.
 *
 * Revision 3.84  2001/01/22 23:32:10  eokerson
 * Some bugfixes from David Huggins-Daines, <dhd@cepstral.com> and other cleanups.
 *
 * Revision 3.83  2001/01/19 14:51:41  eokerson
 * Fixed ixj_WriteDSPCommand to decrement usage counter when command fails.
 *
 * Revision 3.82  2001/01/19 00:34:49  eokerson
 * Added verbosity to write overlap errors.
 *
 * Revision 3.81  2001/01/18 23:56:54  eokerson
 * Fixed PSTN line test functions.
 *
 * Revision 3.80  2001/01/18 22:29:27  eokerson
 * Updated AEC/AGC values for different cards.
 *
 * Revision 3.79  2001/01/17 02:58:54  eokerson
 * Fixed AEC reset after Caller ID.
 * Fixed Codec lockup after Caller ID on Call Waiting when not using 30ms frames.
 *
 * Revision 3.78  2001/01/16 19:43:09  eokerson
 * Added support for Linux 2.4.x kernels.
 *
 * Revision 3.77  2001/01/09 04:00:52  eokerson
 * Linetest will now test the line, even if it has previously succeded.
 *
 * Revision 3.76  2001/01/08 19:27:00  eokerson
 * Fixed problem with standard cable on Internet PhoneCARD.
 *
 * Revision 3.75  2000/12/22 16:52:14  eokerson
 * Modified to allow hookstate detection on the POTS port when the PSTN port is selected.
 *
 * Revision 3.74  2000/12/08 22:41:50  eokerson
 * Added capability for G729B.
 *
 * Revision 3.73  2000/12/07 23:35:16  eokerson
 * Added capability to have different ring pattern before CallerID data.
 * Added hookstate checks in CallerID routines to stop FSK.
 *
 * Revision 3.72  2000/12/06 19:31:31  eokerson
 * Modified signal behavior to only send one signal per event.
 *
 * Revision 3.71  2000/12/06 03:23:08  eokerson
 * Fixed CallerID on Call Waiting.
 *
 * Revision 3.70  2000/12/04 21:29:37  eokerson
 * Added checking to Smart Cable gain functions.
 *
 * Revision 3.69  2000/12/04 21:05:20  eokerson
 * Changed ixjdebug levels.
 * Added ioctls to change gains in Internet Phone CARD Smart Cable.
 *
 * Revision 3.68  2000/12/04 00:17:21  craigs
 * Changed mixer voice gain to +6dB rather than 0dB
 *
 * Revision 3.67  2000/11/30 21:25:51  eokerson
 * Fixed write signal errors.
 *
 * Revision 3.66  2000/11/29 22:42:44  eokerson
 * Fixed PSTN ring detect problems.
 *
 * Revision 3.65  2000/11/29 07:31:55  craigs
 * Added new 425Hz filter co-efficients
 * Added card-specific DTMF prescaler initialisation
 *
 * Revision 3.64  2000/11/28 14:03:32  craigs
 * Changed certain mixer initialisations to be 0dB rather than 12dB
 * Added additional information to /proc/ixj
 *
 * Revision 3.63  2000/11/28 11:38:41  craigs
 * Added display of AEC modes in AUTO and AGC mode
 *
 * Revision 3.62  2000/11/28 04:05:44  eokerson
 * Improved PSTN ring detection routine.
 *
 * Revision 3.61  2000/11/27 21:53:12  eokerson
 * Fixed flash detection.
 *
 * Revision 3.60  2000/11/27 15:57:29  eokerson
 * More work on G.729 load routines.
 *
 * Revision 3.59  2000/11/25 21:55:12  eokerson
 * Fixed errors in G.729 load routine.
 *
 * Revision 3.58  2000/11/25 04:08:29  eokerson
 * Added board locks around G.729 and TS85 load routines.
 *
 * Revision 3.57  2000/11/24 05:35:17  craigs
 * Added ability to retrieve mixer values on LineJACK
 * Added complete initialisation of all mixer values at startup
 * Fixed spelling mistake
 *
 * Revision 3.56  2000/11/23 02:52:11  robertj
 * Added cvs change log keyword.
 * Fixed bug in capabilities list when using G.729 module.
 *
 */

#include "ixj-ver.h"

#define PERFMON_STATS
#define IXJDEBUG 0
#define MAXRINGS 5

#include <linux/module.h>

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/poll.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/pci.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#include <linux/isapnp.h>

#include "ixj.h"

#define TYPE(inode) (iminor(inode) >> 4)
#define NUM(inode) (iminor(inode) & 0xf)

static int ixjdebug;
static int hertz = HZ;
static int samplerate = 100;

module_param(ixjdebug, int, 0);

static struct pci_device_id ixj_pci_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_QUICKNET, PCI_DEVICE_ID_QUICKNET_XJ,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{ }
};

MODULE_DEVICE_TABLE(pci, ixj_pci_tbl);

/************************************************************************
*
* ixjdebug meanings are now bit mapped instead of level based
* Values can be or'ed together to turn on multiple messages
*
* bit  0 (0x0001) = any failure
* bit  1 (0x0002) = general messages
* bit  2 (0x0004) = POTS ringing related
* bit  3 (0x0008) = PSTN events
* bit  4 (0x0010) = PSTN Cadence state details
* bit  5 (0x0020) = Tone detection triggers
* bit  6 (0x0040) = Tone detection cadence details
* bit  7 (0x0080) = ioctl tracking
* bit  8 (0x0100) = signal tracking
* bit  9 (0x0200) = CallerID generation details
*
************************************************************************/

#ifdef IXJ_DYN_ALLOC

static IXJ *ixj[IXJMAX];
#define	get_ixj(b)	ixj[(b)]

/*
 *	Allocate a free IXJ device
 */
 
static IXJ *ixj_alloc()
{
	for(cnt=0; cnt<IXJMAX; cnt++)
	{
		if(ixj[cnt] == NULL || !ixj[cnt]->DSPbase)
		{
			j = kmalloc(sizeof(IXJ), GFP_KERNEL);
			if (j == NULL)
				return NULL;
			ixj[cnt] = j;
			return j;
		}
	}
	return NULL;
}

static void ixj_fsk_free(IXJ *j)
{
	kfree(j->fskdata);
	j->fskdata = NULL;
}

static void ixj_fsk_alloc(IXJ *j)
{
	if(!j->fskdata) {
		j->fskdata = kmalloc(8000, GFP_KERNEL);
		if (!j->fskdata) {
			if(ixjdebug & 0x0200) {
				printk("IXJ phone%d - allocate failed\n", j->board);
			}
			return;
		} else {
			j->fsksize = 8000;
			if(ixjdebug & 0x0200) {
				printk("IXJ phone%d - allocate succeded\n", j->board);
			}
		}
	}
}

#else

static IXJ ixj[IXJMAX];
#define	get_ixj(b)	(&ixj[(b)])

/*
 *	Allocate a free IXJ device
 */
 
static IXJ *ixj_alloc(void)
{
	int cnt;
	for(cnt=0; cnt<IXJMAX; cnt++) {
		if(!ixj[cnt].DSPbase)
			return &ixj[cnt];
	}
	return NULL;
}

static inline void ixj_fsk_free(IXJ *j) {;}

static inline void ixj_fsk_alloc(IXJ *j)
{
	j->fsksize = 8000;
}

#endif

#ifdef PERFMON_STATS
#define ixj_perfmon(x)	((x)++)
#else
#define ixj_perfmon(x)	do { } while(0)
#endif

static int ixj_convert_loaded;

static int ixj_WriteDSPCommand(unsigned short, IXJ *j);

/************************************************************************
*
* These are function definitions to allow external modules to register
* enhanced functionality call backs.
*
************************************************************************/

static int Stub(IXJ * J, unsigned long arg)
{
	return 0;
}

static IXJ_REGFUNC ixj_PreRead = &Stub;
static IXJ_REGFUNC ixj_PostRead = &Stub;
static IXJ_REGFUNC ixj_PreWrite = &Stub;
static IXJ_REGFUNC ixj_PostWrite = &Stub;

static void ixj_read_frame(IXJ *j);
static void ixj_write_frame(IXJ *j);
static void ixj_init_timer(IXJ *j);
static void ixj_add_timer(IXJ *	j);
static void ixj_timeout(unsigned long ptr);
static int read_filters(IXJ *j);
static int LineMonitor(IXJ *j);
static int ixj_fasync(int fd, struct file *, int mode);
static int ixj_set_port(IXJ *j, int arg);
static int ixj_set_pots(IXJ *j, int arg);
static int ixj_hookstate(IXJ *j);
static int ixj_record_start(IXJ *j);
static void ixj_record_stop(IXJ *j);
static void set_rec_volume(IXJ *j, int volume);
static int get_rec_volume(IXJ *j);
static int set_rec_codec(IXJ *j, int rate);
static void ixj_vad(IXJ *j, int arg);
static int ixj_play_start(IXJ *j);
static void ixj_play_stop(IXJ *j);
static int ixj_set_tone_on(unsigned short arg, IXJ *j);
static int ixj_set_tone_off(unsigned short, IXJ *j);
static int ixj_play_tone(IXJ *j, char tone);
static void ixj_aec_start(IXJ *j, int level);
static int idle(IXJ *j);
static void ixj_ring_on(IXJ *j);
static void ixj_ring_off(IXJ *j);
static void aec_stop(IXJ *j);
static void ixj_ringback(IXJ *j);
static void ixj_busytone(IXJ *j);
static void ixj_dialtone(IXJ *j);
static void ixj_cpt_stop(IXJ *j);
static char daa_int_read(IXJ *j);
static char daa_CR_read(IXJ *j, int cr);
static int daa_set_mode(IXJ *j, int mode);
static int ixj_linetest(IXJ *j);
static int ixj_daa_write(IXJ *j);
static int ixj_daa_cid_read(IXJ *j);
static void DAA_Coeff_US(IXJ *j);
static void DAA_Coeff_UK(IXJ *j);
static void DAA_Coeff_France(IXJ *j);
static void DAA_Coeff_Germany(IXJ *j);
static void DAA_Coeff_Australia(IXJ *j);
static void DAA_Coeff_Japan(IXJ *j);
static int ixj_init_filter(IXJ *j, IXJ_FILTER * jf);
static int ixj_init_filter_raw(IXJ *j, IXJ_FILTER_RAW * jfr);
static int ixj_init_tone(IXJ *j, IXJ_TONE * ti);
static int ixj_build_cadence(IXJ *j, IXJ_CADENCE __user * cp);
static int ixj_build_filter_cadence(IXJ *j, IXJ_FILTER_CADENCE __user * cp);
/* Serial Control Interface funtions */
static int SCI_Control(IXJ *j, int control);
static int SCI_Prepare(IXJ *j);
static int SCI_WaitHighSCI(IXJ *j);
static int SCI_WaitLowSCI(IXJ *j);
static DWORD PCIEE_GetSerialNumber(WORD wAddress);
static int ixj_PCcontrol_wait(IXJ *j);
static void ixj_pre_cid(IXJ *j);
static void ixj_write_cid(IXJ *j);
static void ixj_write_cid_bit(IXJ *j, int bit);
static int set_base_frame(IXJ *j, int size);
static int set_play_codec(IXJ *j, int rate);
static void set_rec_depth(IXJ *j, int depth);
static int ixj_mixer(long val, IXJ *j);

/************************************************************************
CT8020/CT8021 Host Programmers Model
Host address	Function					Access
DSPbase +
0-1		Aux Software Status Register (reserved)		Read Only
2-3		Software Status Register			Read Only
4-5		Aux Software Control Register (reserved)	Read Write
6-7		Software Control Register			Read Write
8-9		Hardware Status Register			Read Only
A-B		Hardware Control Register			Read Write
C-D Host Transmit (Write) Data Buffer Access Port (buffer input)Write Only
E-F Host Recieve (Read) Data Buffer Access Port (buffer input)	Read Only
************************************************************************/

static inline void ixj_read_HSR(IXJ *j)
{
	j->hsr.bytes.low = inb_p(j->DSPbase + 8);
	j->hsr.bytes.high = inb_p(j->DSPbase + 9);
}

static inline int IsControlReady(IXJ *j)
{
	ixj_read_HSR(j);
	return j->hsr.bits.controlrdy ? 1 : 0;
}

static inline int IsPCControlReady(IXJ *j)
{
	j->pccr1.byte = inb_p(j->XILINXbase + 3);
	return j->pccr1.bits.crr ? 1 : 0;
}

static inline int IsStatusReady(IXJ *j)
{
	ixj_read_HSR(j);
	return j->hsr.bits.statusrdy ? 1 : 0;
}

static inline int IsRxReady(IXJ *j)
{
	ixj_read_HSR(j);
	ixj_perfmon(j->rxreadycheck);
	return j->hsr.bits.rxrdy ? 1 : 0;
}

static inline int IsTxReady(IXJ *j)
{
	ixj_read_HSR(j);
	ixj_perfmon(j->txreadycheck);
	return j->hsr.bits.txrdy ? 1 : 0;
}

static inline void set_play_volume(IXJ *j, int volume)
{
	if (ixjdebug & 0x0002)
		printk(KERN_INFO "IXJ: /dev/phone%d Setting Play Volume to 0x%4.4x\n", j->board, volume);
	ixj_WriteDSPCommand(0xCF02, j);
	ixj_WriteDSPCommand(volume, j);
}

static int set_play_volume_linear(IXJ *j, int volume)
{
	int newvolume, dspplaymax;

	if (ixjdebug & 0x0002)
		printk(KERN_INFO "IXJ: /dev/phone %d Setting Linear Play Volume to 0x%4.4x\n", j->board, volume);
	if(volume > 100 || volume < 0) {
		return -1;
	}

	/* This should normalize the perceived volumes between the different cards caused by differences in the hardware */
	switch (j->cardtype) {
	case QTI_PHONEJACK:
		dspplaymax = 0x380;
		break;
	case QTI_LINEJACK:
		if(j->port == PORT_PSTN) {
			dspplaymax = 0x48;
		} else {
			dspplaymax = 0x100;
		}
		break;
	case QTI_PHONEJACK_LITE:
		dspplaymax = 0x380;
		break;
	case QTI_PHONEJACK_PCI:
		dspplaymax = 0x6C;
		break;
	case QTI_PHONECARD:
		dspplaymax = 0x50;
		break;
	default:
		return -1;
	}
	newvolume = (dspplaymax * volume) / 100;
	set_play_volume(j, newvolume);
	return 0;
}

static inline void set_play_depth(IXJ *j, int depth)
{
	if (depth > 60)
		depth = 60;
	if (depth < 0)
		depth = 0;
	ixj_WriteDSPCommand(0x5280 + depth, j);
}

static inline int get_play_volume(IXJ *j)
{
	ixj_WriteDSPCommand(0xCF00, j);
	return j->ssr.high << 8 | j->ssr.low;
}

static int get_play_volume_linear(IXJ *j)
{
	int volume, newvolume, dspplaymax;

	/* This should normalize the perceived volumes between the different cards caused by differences in the hardware */
	switch (j->cardtype) {
	case QTI_PHONEJACK:
		dspplaymax = 0x380;
		break;
	case QTI_LINEJACK:
		if(j->port == PORT_PSTN) {
			dspplaymax = 0x48;
		} else {
			dspplaymax = 0x100;
		}
		break;
	case QTI_PHONEJACK_LITE:
		dspplaymax = 0x380;
		break;
	case QTI_PHONEJACK_PCI:
		dspplaymax = 0x6C;
		break;
	case QTI_PHONECARD:
		dspplaymax = 100;
		break;
	default:
		return -1;
	}
	volume = get_play_volume(j);
	newvolume = (volume * 100) / dspplaymax;
	if(newvolume > 100)
		newvolume = 100;
	return newvolume;
}

static inline BYTE SLIC_GetState(IXJ *j)
{
	if (j->cardtype == QTI_PHONECARD) {
		j->pccr1.byte = 0;
		j->psccr.bits.dev = 3;
		j->psccr.bits.rw = 1;
		outw_p(j->psccr.byte << 8, j->XILINXbase + 0x00);
		ixj_PCcontrol_wait(j);
		j->pslic.byte = inw_p(j->XILINXbase + 0x00) & 0xFF;
		ixj_PCcontrol_wait(j);
		if (j->pslic.bits.powerdown)
			return PLD_SLIC_STATE_OC;
		else if (!j->pslic.bits.ring0 && !j->pslic.bits.ring1)
			return PLD_SLIC_STATE_ACTIVE;
		else
			return PLD_SLIC_STATE_RINGING;
	} else {
		j->pld_slicr.byte = inb_p(j->XILINXbase + 0x01);
	}
	return j->pld_slicr.bits.state;
}

static bool SLIC_SetState(BYTE byState, IXJ *j)
{
	bool fRetVal = false;

	if (j->cardtype == QTI_PHONECARD) {
		if (j->flags.pcmciasct) {
			switch (byState) {
			case PLD_SLIC_STATE_TIPOPEN:
			case PLD_SLIC_STATE_OC:
				j->pslic.bits.powerdown = 1;
				j->pslic.bits.ring0 = j->pslic.bits.ring1 = 0;
				fRetVal = true;
				break;
			case PLD_SLIC_STATE_RINGING:
				if (j->readers || j->writers) {
					j->pslic.bits.powerdown = 0;
					j->pslic.bits.ring0 = 1;
					j->pslic.bits.ring1 = 0;
					fRetVal = true;
				}
				break;
			case PLD_SLIC_STATE_OHT:	/* On-hook transmit */

			case PLD_SLIC_STATE_STANDBY:
			case PLD_SLIC_STATE_ACTIVE:
				if (j->readers || j->writers) {
					j->pslic.bits.powerdown = 0;
				} else {
					j->pslic.bits.powerdown = 1;
				}
				j->pslic.bits.ring0 = j->pslic.bits.ring1 = 0;
				fRetVal = true;
				break;
			case PLD_SLIC_STATE_APR:	/* Active polarity reversal */

			case PLD_SLIC_STATE_OHTPR:	/* OHT polarity reversal */

			default:
				fRetVal = false;
				break;
			}
			j->psccr.bits.dev = 3;
			j->psccr.bits.rw = 0;
			outw_p(j->psccr.byte << 8 | j->pslic.byte, j->XILINXbase + 0x00);
			ixj_PCcontrol_wait(j);
		}
	} else {
		/* Set the C1, C2, C3 & B2EN signals. */
		switch (byState) {
		case PLD_SLIC_STATE_OC:
			j->pld_slicw.bits.c1 = 0;
			j->pld_slicw.bits.c2 = 0;
			j->pld_slicw.bits.c3 = 0;
			j->pld_slicw.bits.b2en = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_RINGING:
			j->pld_slicw.bits.c1 = 1;
			j->pld_slicw.bits.c2 = 0;
			j->pld_slicw.bits.c3 = 0;
			j->pld_slicw.bits.b2en = 1;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_ACTIVE:
			j->pld_slicw.bits.c1 = 0;
			j->pld_slicw.bits.c2 = 1;
			j->pld_slicw.bits.c3 = 0;
			j->pld_slicw.bits.b2en = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_OHT:	/* On-hook transmit */

			j->pld_slicw.bits.c1 = 1;
			j->pld_slicw.bits.c2 = 1;
			j->pld_slicw.bits.c3 = 0;
			j->pld_slicw.bits.b2en = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_TIPOPEN:
			j->pld_slicw.bits.c1 = 0;
			j->pld_slicw.bits.c2 = 0;
			j->pld_slicw.bits.c3 = 1;
			j->pld_slicw.bits.b2en = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_STANDBY:
			j->pld_slicw.bits.c1 = 1;
			j->pld_slicw.bits.c2 = 0;
			j->pld_slicw.bits.c3 = 1;
			j->pld_slicw.bits.b2en = 1;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_APR:	/* Active polarity reversal */

			j->pld_slicw.bits.c1 = 0;
			j->pld_slicw.bits.c2 = 1;
			j->pld_slicw.bits.c3 = 1;
			j->pld_slicw.bits.b2en = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_OHTPR:	/* OHT polarity reversal */

			j->pld_slicw.bits.c1 = 1;
			j->pld_slicw.bits.c2 = 1;
			j->pld_slicw.bits.c3 = 1;
			j->pld_slicw.bits.b2en = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		default:
			fRetVal = false;
			break;
		}
	}

	return fRetVal;
}

static int ixj_wink(IXJ *j)
{
	BYTE slicnow;

	slicnow = SLIC_GetState(j);

	j->pots_winkstart = jiffies;
	SLIC_SetState(PLD_SLIC_STATE_OC, j);

	msleep(jiffies_to_msecs(j->winktime));

	SLIC_SetState(slicnow, j);
	return 0;
}

static void ixj_init_timer(IXJ *j)
{
	init_timer(&j->timer);
	j->timer.function = ixj_timeout;
	j->timer.data = (unsigned long)j;
}

static void ixj_add_timer(IXJ *j)
{
	j->timer.expires = jiffies + (hertz / samplerate);
	add_timer(&j->timer);
}

static void ixj_tone_timeout(IXJ *j)
{
	IXJ_TONE ti;

	j->tone_state++;
	if (j->tone_state == 3) {
		j->tone_state = 0;
		if (j->cadence_t) {
			j->tone_cadence_state++;
			if (j->tone_cadence_state >= j->cadence_t->elements_used) {
				switch (j->cadence_t->termination) {
				case PLAY_ONCE:
					ixj_cpt_stop(j);
					break;
				case REPEAT_LAST_ELEMENT:
					j->tone_cadence_state--;
					ixj_play_tone(j, j->cadence_t->ce[j->tone_cadence_state].index);
					break;
				case REPEAT_ALL:
					j->tone_cadence_state = 0;
					if (j->cadence_t->ce[j->tone_cadence_state].freq0) {
						ti.tone_index = j->cadence_t->ce[j->tone_cadence_state].index;
						ti.freq0 = j->cadence_t->ce[j->tone_cadence_state].freq0;
						ti.gain0 = j->cadence_t->ce[j->tone_cadence_state].gain0;
						ti.freq1 = j->cadence_t->ce[j->tone_cadence_state].freq1;
						ti.gain1 = j->cadence_t->ce[j->tone_cadence_state].gain1;
						ixj_init_tone(j, &ti);
					}
					ixj_set_tone_on(j->cadence_t->ce[0].tone_on_time, j);
					ixj_set_tone_off(j->cadence_t->ce[0].tone_off_time, j);
					ixj_play_tone(j, j->cadence_t->ce[0].index);
					break;
				}
			} else {
				if (j->cadence_t->ce[j->tone_cadence_state].gain0) {
					ti.tone_index = j->cadence_t->ce[j->tone_cadence_state].index;
					ti.freq0 = j->cadence_t->ce[j->tone_cadence_state].freq0;
					ti.gain0 = j->cadence_t->ce[j->tone_cadence_state].gain0;
					ti.freq1 = j->cadence_t->ce[j->tone_cadence_state].freq1;
					ti.gain1 = j->cadence_t->ce[j->tone_cadence_state].gain1;
					ixj_init_tone(j, &ti);
				}
				ixj_set_tone_on(j->cadence_t->ce[j->tone_cadence_state].tone_on_time, j);
				ixj_set_tone_off(j->cadence_t->ce[j->tone_cadence_state].tone_off_time, j);
				ixj_play_tone(j, j->cadence_t->ce[j->tone_cadence_state].index);
			}
		}
	}
}

static inline void ixj_kill_fasync(IXJ *j, IXJ_SIGEVENT event, int dir)
{
	if(j->ixj_signals[event]) {
		if(ixjdebug & 0x0100)
			printk("Sending signal for event %d\n", event);
			/* Send apps notice of change */
		/* see config.h for macro definition */
		kill_fasync(&(j->async_queue), j->ixj_signals[event], dir);
	}
}

static void ixj_pstn_state(IXJ *j)
{
	int var;
	union XOPXR0 XR0, daaint;

	var = 10;

	XR0.reg = j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.reg;
	daaint.reg = 0;
	XR0.bitreg.RMR = j->m_DAAShadowRegs.SOP_REGS.SOP.cr1.bitreg.RMR;

	j->pld_scrr.byte = inb_p(j->XILINXbase);
	if (j->pld_scrr.bits.daaflag) {
		daa_int_read(j);
		if(j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitreg.RING) {
			if(time_after(jiffies, j->pstn_sleeptil) && !(j->flags.pots_pstn && j->hookstate)) {
				daaint.bitreg.RING = 1;
				if(ixjdebug & 0x0008) {
					printk(KERN_INFO "IXJ DAA Ring Interrupt /dev/phone%d at %ld\n", j->board, jiffies);
				}
			} else {
				daa_set_mode(j, SOP_PU_RESET);
			}
		}
		if(j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitreg.Caller_ID) {
			daaint.bitreg.Caller_ID = 1;
			j->pstn_cid_intr = 1;
			j->pstn_cid_received = jiffies;
			if(ixjdebug & 0x0008) {
				printk(KERN_INFO "IXJ DAA Caller_ID Interrupt /dev/phone%d at %ld\n", j->board, jiffies);
			}
		}
		if(j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitreg.Cadence) {
			daaint.bitreg.Cadence = 1;
			if(ixjdebug & 0x0008) {
				printk(KERN_INFO "IXJ DAA Cadence Interrupt /dev/phone%d at %ld\n", j->board, jiffies);
			}
		}
		if(j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitreg.VDD_OK != XR0.bitreg.VDD_OK) {
			daaint.bitreg.VDD_OK = 1;
			daaint.bitreg.SI_0 = j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitreg.VDD_OK;
		}
	}
	daa_CR_read(j, 1);
	if(j->m_DAAShadowRegs.SOP_REGS.SOP.cr1.bitreg.RMR != XR0.bitreg.RMR && time_after(jiffies, j->pstn_sleeptil) && !(j->flags.pots_pstn && j->hookstate)) {
		daaint.bitreg.RMR = 1;
		daaint.bitreg.SI_1 = j->m_DAAShadowRegs.SOP_REGS.SOP.cr1.bitreg.RMR;
		if(ixjdebug & 0x0008) {
                        printk(KERN_INFO "IXJ DAA RMR /dev/phone%d was %s for %ld\n", j->board, XR0.bitreg.RMR?"on":"off", jiffies - j->pstn_last_rmr);
		}
		j->pstn_prev_rmr = j->pstn_last_rmr;
		j->pstn_last_rmr = jiffies;
	}
	switch(j->daa_mode) {
		case SOP_PU_SLEEP:
			if (daaint.bitreg.RING) {
				if (!j->flags.pstn_ringing) {
					if (j->daa_mode != SOP_PU_RINGING) {
						j->pstn_ring_int = jiffies;
						daa_set_mode(j, SOP_PU_RINGING);
					}
				}
			}
			break;
		case SOP_PU_RINGING:
			if (daaint.bitreg.RMR) {
				if (ixjdebug & 0x0008) {
					printk(KERN_INFO "IXJ Ring Cadence a state = %d /dev/phone%d at %ld\n", j->cadence_f[4].state, j->board, jiffies);
				}
				if (daaint.bitreg.SI_1) {                /* Rising edge of RMR */
					j->flags.pstn_rmr = 1;
					j->pstn_ring_start = jiffies;
					j->pstn_ring_stop = 0;
					j->ex.bits.pstn_ring = 0;
					if (j->cadence_f[4].state == 0) {
						j->cadence_f[4].state = 1;
						j->cadence_f[4].on1min = jiffies + (long)((j->cadence_f[4].on1 * hertz * (100 - var)) / 10000);
						j->cadence_f[4].on1dot = jiffies + (long)((j->cadence_f[4].on1 * hertz * (100)) / 10000);
						j->cadence_f[4].on1max = jiffies + (long)((j->cadence_f[4].on1 * hertz * (100 + var)) / 10000);
					} else if (j->cadence_f[4].state == 2) {
						if((time_after(jiffies, j->cadence_f[4].off1min) &&
						    time_before(jiffies, j->cadence_f[4].off1max))) {
							if (j->cadence_f[4].on2) {
								j->cadence_f[4].state = 3;
								j->cadence_f[4].on2min = jiffies + (long)((j->cadence_f[4].on2 * (hertz * (100 - var)) / 10000));
								j->cadence_f[4].on2dot = jiffies + (long)((j->cadence_f[4].on2 * (hertz * (100)) / 10000));
								j->cadence_f[4].on2max = jiffies + (long)((j->cadence_f[4].on2 * (hertz * (100 + var)) / 10000));
							} else {
								j->cadence_f[4].state = 7;
							}
						} else {
							if (ixjdebug & 0x0008) {
								printk(KERN_INFO "IXJ Ring Cadence fail state = %d /dev/phone%d at %ld should be %d\n",
										j->cadence_f[4].state, j->board, jiffies - j->pstn_prev_rmr,
										j->cadence_f[4].off1);
							}
							j->cadence_f[4].state = 0;
						}
					} else if (j->cadence_f[4].state == 4) {
						if((time_after(jiffies, j->cadence_f[4].off2min) &&
						    time_before(jiffies, j->cadence_f[4].off2max))) {
							if (j->cadence_f[4].on3) {
								j->cadence_f[4].state = 5;
								j->cadence_f[4].on3min = jiffies + (long)((j->cadence_f[4].on3 * (hertz * (100 - var)) / 10000));
								j->cadence_f[4].on3dot = jiffies + (long)((j->cadence_f[4].on3 * (hertz * (100)) / 10000));
								j->cadence_f[4].on3max = jiffies + (long)((j->cadence_f[4].on3 * (hertz * (100 + var)) / 10000));
							} else {
								j->cadence_f[4].state = 7;
							}
						} else {
							if (ixjdebug & 0x0008) {
								printk(KERN_INFO "IXJ Ring Cadence fail state = %d /dev/phone%d at %ld should be %d\n",
										j->cadence_f[4].state, j->board, jiffies - j->pstn_prev_rmr,
										j->cadence_f[4].off2);
							}
							j->cadence_f[4].state = 0;
						}
					} else if (j->cadence_f[4].state == 6) {
						if((time_after(jiffies, j->cadence_f[4].off3min) &&
						    time_before(jiffies, j->cadence_f[4].off3max))) {
							j->cadence_f[4].state = 7;
						} else {
							if (ixjdebug & 0x0008) {
								printk(KERN_INFO "IXJ Ring Cadence fail state = %d /dev/phone%d at %ld should be %d\n",
										j->cadence_f[4].state, j->board, jiffies - j->pstn_prev_rmr,
										j->cadence_f[4].off3);
							}
							j->cadence_f[4].state = 0;
						}
					} else {
						j->cadence_f[4].state = 0;
					}
				} else {                                /* Falling edge of RMR */
					j->pstn_ring_start = 0;
					j->pstn_ring_stop = jiffies;
					if (j->cadence_f[4].state == 1) {
						if(!j->cadence_f[4].on1) {
							j->cadence_f[4].state = 7;
						} else if((time_after(jiffies, j->cadence_f[4].on1min) &&
					          time_before(jiffies, j->cadence_f[4].on1max))) {
							if (j->cadence_f[4].off1) {
								j->cadence_f[4].state = 2;
								j->cadence_f[4].off1min = jiffies + (long)((j->cadence_f[4].off1 * (hertz * (100 - var)) / 10000));
								j->cadence_f[4].off1dot = jiffies + (long)((j->cadence_f[4].off1 * (hertz * (100)) / 10000));
								j->cadence_f[4].off1max = jiffies + (long)((j->cadence_f[4].off1 * (hertz * (100 + var)) / 10000));
							} else {
								j->cadence_f[4].state = 7;
							}
						} else {
							if (ixjdebug & 0x0008) {
								printk(KERN_INFO "IXJ Ring Cadence fail state = %d /dev/phone%d at %ld should be %d\n",
										j->cadence_f[4].state, j->board, jiffies - j->pstn_prev_rmr,
										j->cadence_f[4].on1);
							}
							j->cadence_f[4].state = 0;
						}
					} else if (j->cadence_f[4].state == 3) {
						if((time_after(jiffies, j->cadence_f[4].on2min) &&
						    time_before(jiffies, j->cadence_f[4].on2max))) {
							if (j->cadence_f[4].off2) {
								j->cadence_f[4].state = 4;
								j->cadence_f[4].off2min = jiffies + (long)((j->cadence_f[4].off2 * (hertz * (100 - var)) / 10000));
								j->cadence_f[4].off2dot = jiffies + (long)((j->cadence_f[4].off2 * (hertz * (100)) / 10000));
								j->cadence_f[4].off2max = jiffies + (long)((j->cadence_f[4].off2 * (hertz * (100 + var)) / 10000));
							} else {
								j->cadence_f[4].state = 7;
							}
						} else {
							if (ixjdebug & 0x0008) {
								printk(KERN_INFO "IXJ Ring Cadence fail state = %d /dev/phone%d at %ld should be %d\n",
										j->cadence_f[4].state, j->board, jiffies - j->pstn_prev_rmr,
										j->cadence_f[4].on2);
							}
							j->cadence_f[4].state = 0;
						}
					} else if (j->cadence_f[4].state == 5) {
						if((time_after(jiffies, j->cadence_f[4].on3min) &&
						    time_before(jiffies, j->cadence_f[4].on3max))) {
							if (j->cadence_f[4].off3) {
								j->cadence_f[4].state = 6;
								j->cadence_f[4].off3min = jiffies + (long)((j->cadence_f[4].off3 * (hertz * (100 - var)) / 10000));
								j->cadence_f[4].off3dot = jiffies + (long)((j->cadence_f[4].off3 * (hertz * (100)) / 10000));
								j->cadence_f[4].off3max = jiffies + (long)((j->cadence_f[4].off3 * (hertz * (100 + var)) / 10000));
							} else {
								j->cadence_f[4].state = 7;
							}
						} else {
							j->cadence_f[4].state = 0;
						}
					} else {
						if (ixjdebug & 0x0008) {
							printk(KERN_INFO "IXJ Ring Cadence fail state = %d /dev/phone%d at %ld should be %d\n",
									j->cadence_f[4].state, j->board, jiffies - j->pstn_prev_rmr,
									j->cadence_f[4].on3);
						}
						j->cadence_f[4].state = 0;
					}
				}
				if (ixjdebug & 0x0010) {
					printk(KERN_INFO "IXJ Ring Cadence b state = %d /dev/phone%d at %ld\n", j->cadence_f[4].state, j->board, jiffies);
				}
				if (ixjdebug & 0x0010) {
					switch(j->cadence_f[4].state) {
						case 1:
							printk(KERN_INFO "IXJ /dev/phone%d Next Ring Cadence state at %u min %ld - %ld - max %ld\n", j->board,
						j->cadence_f[4].on1, j->cadence_f[4].on1min, j->cadence_f[4].on1dot, j->cadence_f[4].on1max);
							break;
						case 2:
							printk(KERN_INFO "IXJ /dev/phone%d Next Ring Cadence state at %u min %ld - %ld - max %ld\n", j->board,
						j->cadence_f[4].off1, j->cadence_f[4].off1min, j->cadence_f[4].off1dot, j->cadence_f[4].off1max);
							break;
						case 3:
							printk(KERN_INFO "IXJ /dev/phone%d Next Ring Cadence state at %u min %ld - %ld - max %ld\n", j->board,
						j->cadence_f[4].on2, j->cadence_f[4].on2min, j->cadence_f[4].on2dot, j->cadence_f[4].on2max);
							break;
						case 4:
							printk(KERN_INFO "IXJ /dev/phone%d Next Ring Cadence state at %u min %ld - %ld - max %ld\n", j->board,
						j->cadence_f[4].off2, j->cadence_f[4].off2min, j->cadence_f[4].off2dot, j->cadence_f[4].off2max);
							break;
						case 5:
							printk(KERN_INFO "IXJ /dev/phone%d Next Ring Cadence state at %u min %ld - %ld - max %ld\n", j->board,
						j->cadence_f[4].on3, j->cadence_f[4].on3min, j->cadence_f[4].on3dot, j->cadence_f[4].on3max);
							break;
						case 6:	
							printk(KERN_INFO "IXJ /dev/phone%d Next Ring Cadence state at %u min %ld - %ld - max %ld\n", j->board,
						j->cadence_f[4].off3, j->cadence_f[4].off3min, j->cadence_f[4].off3dot, j->cadence_f[4].off3max);
							break;
					}
				}
			}
			if (j->cadence_f[4].state == 7) {
				j->cadence_f[4].state = 0;
				j->pstn_ring_stop = jiffies;
				j->ex.bits.pstn_ring = 1;
				ixj_kill_fasync(j, SIG_PSTN_RING, POLL_IN);
				if(ixjdebug & 0x0008) {
					printk(KERN_INFO "IXJ Ring int set /dev/phone%d at %ld\n", j->board, jiffies);
				}
			}
			if((j->pstn_ring_int != 0 && time_after(jiffies, j->pstn_ring_int + (hertz * 5)) && !j->flags.pstn_rmr) ||
			   (j->pstn_ring_stop != 0 && time_after(jiffies, j->pstn_ring_stop + (hertz * 5)))) {
				if(ixjdebug & 0x0008) {
					printk("IXJ DAA no ring in 5 seconds /dev/phone%d at %ld\n", j->board, jiffies);
					printk("IXJ DAA pstn ring int /dev/phone%d at %ld\n", j->board, j->pstn_ring_int);
					printk("IXJ DAA pstn ring stop /dev/phone%d at %ld\n", j->board, j->pstn_ring_stop);
				}
				j->pstn_ring_stop = j->pstn_ring_int = 0;
				daa_set_mode(j, SOP_PU_SLEEP);
			} 
			outb_p(j->pld_scrw.byte, j->XILINXbase);
			if (j->pstn_cid_intr && time_after(jiffies, j->pstn_cid_received + hertz)) {
				ixj_daa_cid_read(j);
				j->ex.bits.caller_id = 1;
				ixj_kill_fasync(j, SIG_CALLER_ID, POLL_IN);
				j->pstn_cid_intr = 0;
			}
			if (daaint.bitreg.Cadence) {
				if(ixjdebug & 0x0008) {
					printk("IXJ DAA Cadence interrupt going to sleep /dev/phone%d\n", j->board);
				}
				daa_set_mode(j, SOP_PU_SLEEP);
				j->ex.bits.pstn_ring = 0;
			}
			break;
		case SOP_PU_CONVERSATION:
			if (daaint.bitreg.VDD_OK) {
				if(!daaint.bitreg.SI_0) {
					if (!j->pstn_winkstart) {
						if(ixjdebug & 0x0008) {
							printk("IXJ DAA possible wink /dev/phone%d %ld\n", j->board, jiffies);
						}
						j->pstn_winkstart = jiffies;
					} 
				} else {
					if (j->pstn_winkstart) {
						if(ixjdebug & 0x0008) {
							printk("IXJ DAA possible wink end /dev/phone%d %ld\n", j->board, jiffies);
						}
						j->pstn_winkstart = 0;
					}
				}
			}
			if (j->pstn_winkstart && time_after(jiffies, j->pstn_winkstart + ((hertz * j->winktime) / 1000))) {
				if(ixjdebug & 0x0008) {
					printk("IXJ DAA wink detected going to sleep /dev/phone%d %ld\n", j->board, jiffies);
				}
				daa_set_mode(j, SOP_PU_SLEEP);
				j->pstn_winkstart = 0;
				j->ex.bits.pstn_wink = 1;
				ixj_kill_fasync(j, SIG_PSTN_WINK, POLL_IN);
			}
			break;
	}
}

static void ixj_timeout(unsigned long ptr)
{
	int board;
	unsigned long jifon;
	IXJ *j = (IXJ *)ptr;
	board = j->board;

	if (j->DSPbase && atomic_read(&j->DSPWrite) == 0 && test_and_set_bit(board, (void *)&j->busyflags) == 0) {
		ixj_perfmon(j->timerchecks);
		j->hookstate = ixj_hookstate(j);
		if (j->tone_state) {
			if (!(j->hookstate)) {
				ixj_cpt_stop(j);
				if (j->m_hook) {
					j->m_hook = 0;
					j->ex.bits.hookstate = 1;
					ixj_kill_fasync(j, SIG_HOOKSTATE, POLL_IN);
				}
				clear_bit(board, &j->busyflags);
				ixj_add_timer(j);
				return;
			}
			if (j->tone_state == 1)
				jifon = ((hertz * j->tone_on_time) * 25 / 100000);
			else
				jifon = ((hertz * j->tone_on_time) * 25 / 100000) + ((hertz * j->tone_off_time) * 25 / 100000);
			if (time_before(jiffies, j->tone_start_jif + jifon)) {
				if (j->tone_state == 1) {
					ixj_play_tone(j, j->tone_index);
					if (j->dsp.low == 0x20) {
						clear_bit(board, &j->busyflags);
						ixj_add_timer(j);
						return;
					}
				} else {
					ixj_play_tone(j, 0);
					if (j->dsp.low == 0x20) {
						clear_bit(board, &j->busyflags);
						ixj_add_timer(j);
						return;
					}
				}
			} else {
				ixj_tone_timeout(j);
				if (j->flags.dialtone) {
					ixj_dialtone(j);
				}
				if (j->flags.busytone) {
					ixj_busytone(j);
					if (j->dsp.low == 0x20) {
						clear_bit(board, &j->busyflags);
						ixj_add_timer(j);
						return;
					}
				}
				if (j->flags.ringback) {
					ixj_ringback(j);
					if (j->dsp.low == 0x20) {
						clear_bit(board, &j->busyflags);
						ixj_add_timer(j);
						return;
					}
				}
				if (!j->tone_state) {
					ixj_cpt_stop(j);
				}
			}
		}
		if (!(j->tone_state && j->dsp.low == 0x20)) {
			if (IsRxReady(j)) {
				ixj_read_frame(j);
			}
			if (IsTxReady(j)) {
				ixj_write_frame(j);
			}
		}
		if (j->flags.cringing) {
			if (j->hookstate & 1) {
				j->flags.cringing = 0;
				ixj_ring_off(j);
			} else if(j->cadence_f[5].enable && ((!j->cadence_f[5].en_filter) || (j->cadence_f[5].en_filter && j->flags.firstring))) {
				switch(j->cadence_f[5].state) {
					case 0:
						j->cadence_f[5].on1dot = jiffies + (long)((j->cadence_f[5].on1 * (hertz * 100) / 10000));
						if (time_before(jiffies, j->cadence_f[5].on1dot)) {
							if(ixjdebug & 0x0004) {
								printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].state, jiffies);
							}
							ixj_ring_on(j);
						}
						j->cadence_f[5].state = 1;
						break;
					case 1:
						if (time_after(jiffies, j->cadence_f[5].on1dot)) {
							j->cadence_f[5].off1dot = jiffies + (long)((j->cadence_f[5].off1 * (hertz * 100) / 10000));
							if(ixjdebug & 0x0004) {
								printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].state, jiffies);
							}
							ixj_ring_off(j);
							j->cadence_f[5].state = 2;
						}
						break;
					case 2:
						if (time_after(jiffies, j->cadence_f[5].off1dot)) {
							if(ixjdebug & 0x0004) {
								printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].state, jiffies);
							}
							ixj_ring_on(j);
							if (j->cadence_f[5].on2) {
								j->cadence_f[5].on2dot = jiffies + (long)((j->cadence_f[5].on2 * (hertz * 100) / 10000));
								j->cadence_f[5].state = 3;
							} else {
								j->cadence_f[5].state = 7;
							}
						}
						break;
					case 3:
						if (time_after(jiffies, j->cadence_f[5].on2dot)) {
							if(ixjdebug & 0x0004) {
								printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].state, jiffies);
							}
							ixj_ring_off(j);
							if (j->cadence_f[5].off2) {
								j->cadence_f[5].off2dot = jiffies + (long)((j->cadence_f[5].off2 * (hertz * 100) / 10000));
								j->cadence_f[5].state = 4;
							} else {
								j->cadence_f[5].state = 7;
							}
						}
						break;
					case 4:
						if (time_after(jiffies, j->cadence_f[5].off2dot)) {
							if(ixjdebug & 0x0004) {
								printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].state, jiffies);
							}
							ixj_ring_on(j);
							if (j->cadence_f[5].on3) {
								j->cadence_f[5].on3dot = jiffies + (long)((j->cadence_f[5].on3 * (hertz * 100) / 10000));
								j->cadence_f[5].state = 5;
							} else {
								j->cadence_f[5].state = 7;
							}
						}
						break;
					case 5:
						if (time_after(jiffies, j->cadence_f[5].on3dot)) {
							if(ixjdebug & 0x0004) {
								printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].state, jiffies);
							}
							ixj_ring_off(j);
							if (j->cadence_f[5].off3) {
								j->cadence_f[5].off3dot = jiffies + (long)((j->cadence_f[5].off3 * (hertz * 100) / 10000));
								j->cadence_f[5].state = 6;
							} else {
								j->cadence_f[5].state = 7;
							}
						}
						break;
					case 6:
						if (time_after(jiffies, j->cadence_f[5].off3dot)) {
							if(ixjdebug & 0x0004) {
								printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].state, jiffies);
							}
							j->cadence_f[5].state = 7;
						}
						break;
					case 7:
						if(ixjdebug & 0x0004) {
							printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].state, jiffies);
						}
						j->flags.cidring = 1;
						j->cadence_f[5].state = 0;
						break;
				}
				if (j->flags.cidring && !j->flags.cidsent) {
					j->flags.cidsent = 1;
					if(j->fskdcnt) {
						SLIC_SetState(PLD_SLIC_STATE_OHT, j);
						ixj_pre_cid(j);
					}
					j->flags.cidring = 0;
				}
				clear_bit(board, &j->busyflags);
				ixj_add_timer(j);
				return;
			} else {
				if (time_after(jiffies, j->ring_cadence_jif + (hertz / 2))) {
					if (j->flags.cidring && !j->flags.cidsent) {
						j->flags.cidsent = 1;
						if(j->fskdcnt) {
							SLIC_SetState(PLD_SLIC_STATE_OHT, j);
							ixj_pre_cid(j);
						}
						j->flags.cidring = 0;
					}
					j->ring_cadence_t--;
					if (j->ring_cadence_t == -1)
						j->ring_cadence_t = 15;
					j->ring_cadence_jif = jiffies;

					if (j->ring_cadence & 1 << j->ring_cadence_t) {
						if(j->flags.cidsent && j->cadence_f[5].en_filter)
							j->flags.firstring = 1;
						else
							ixj_ring_on(j);
					} else {
						ixj_ring_off(j);
						if(!j->flags.cidsent)
							j->flags.cidring = 1;
					}
				}
				clear_bit(board, &j->busyflags);
				ixj_add_timer(j);
				return;
			}
		}
		if (!j->flags.ringing) {
			if (j->hookstate) { /* & 1) { */
				if (j->dsp.low != 0x20 &&
				    SLIC_GetState(j) != PLD_SLIC_STATE_ACTIVE) {
					SLIC_SetState(PLD_SLIC_STATE_ACTIVE, j);
				}
				LineMonitor(j);
				read_filters(j);
				ixj_WriteDSPCommand(0x511B, j);
				j->proc_load = j->ssr.high << 8 | j->ssr.low;
				if (!j->m_hook && (j->hookstate & 1)) {
					j->m_hook = j->ex.bits.hookstate = 1;
					ixj_kill_fasync(j, SIG_HOOKSTATE, POLL_IN);
				}
			} else {
				if (j->ex.bits.dtmf_ready) {
					j->dtmf_wp = j->dtmf_rp = j->ex.bits.dtmf_ready = 0;
				}
				if (j->m_hook) {
					j->m_hook = 0;
					j->ex.bits.hookstate = 1;
					ixj_kill_fasync(j, SIG_HOOKSTATE, POLL_IN);
				}
			}
		}
		if (j->cardtype == QTI_LINEJACK && !j->flags.pstncheck && j->flags.pstn_present) {
			ixj_pstn_state(j);
		}
		if (j->ex.bytes) {
			wake_up_interruptible(&j->poll_q);	/* Wake any blocked selects */
		}
		clear_bit(board, &j->busyflags);
	}
	ixj_add_timer(j);
}

static int ixj_status_wait(IXJ *j)
{
	unsigned long jif;

	jif = jiffies + ((60 * hertz) / 100);
	while (!IsStatusReady(j)) {
		ixj_perfmon(j->statuswait);
		if (time_after(jiffies, jif)) {
			ixj_perfmon(j->statuswaitfail);
			return -1;
		}
	}
	return 0;
}

static int ixj_PCcontrol_wait(IXJ *j)
{
	unsigned long jif;

	jif = jiffies + ((60 * hertz) / 100);
	while (!IsPCControlReady(j)) {
		ixj_perfmon(j->pcontrolwait);
		if (time_after(jiffies, jif)) {
			ixj_perfmon(j->pcontrolwaitfail);
			return -1;
		}
	}
	return 0;
}

static int ixj_WriteDSPCommand(unsigned short cmd, IXJ *j)
{
	BYTES bytes;
	unsigned long jif;

	atomic_inc(&j->DSPWrite);
	if(atomic_read(&j->DSPWrite) > 1) {
		printk("IXJ %d DSP write overlap attempting command 0x%4.4x\n", j->board, cmd);
		return -1;
	}
	bytes.high = (cmd & 0xFF00) >> 8;
	bytes.low = cmd & 0x00FF;
	jif = jiffies + ((60 * hertz) / 100);
	while (!IsControlReady(j)) {
		ixj_perfmon(j->iscontrolready);
		if (time_after(jiffies, jif)) {
			ixj_perfmon(j->iscontrolreadyfail);
			atomic_dec(&j->DSPWrite);
			if(atomic_read(&j->DSPWrite) > 0) {
				printk("IXJ %d DSP overlaped command 0x%4.4x during control ready failure.\n", j->board, cmd);
				while(atomic_read(&j->DSPWrite) > 0) {
					atomic_dec(&j->DSPWrite);
				}
			}
			return -1;
		}
	}
	outb(bytes.low, j->DSPbase + 6);
	outb(bytes.high, j->DSPbase + 7);

	if (ixj_status_wait(j)) {
		j->ssr.low = 0xFF;
		j->ssr.high = 0xFF;
		atomic_dec(&j->DSPWrite);
		if(atomic_read(&j->DSPWrite) > 0) {
			printk("IXJ %d DSP overlaped command 0x%4.4x during status wait failure.\n", j->board, cmd);
			while(atomic_read(&j->DSPWrite) > 0) {
				atomic_dec(&j->DSPWrite);
			}
		}
		return -1;
	}
/* Read Software Status Register */
	j->ssr.low = inb_p(j->DSPbase + 2);
	j->ssr.high = inb_p(j->DSPbase + 3);
	atomic_dec(&j->DSPWrite);
	if(atomic_read(&j->DSPWrite) > 0) {
		printk("IXJ %d DSP overlaped command 0x%4.4x\n", j->board, cmd);
		while(atomic_read(&j->DSPWrite) > 0) {
			atomic_dec(&j->DSPWrite);
		}
	}
	return 0;
}

/***************************************************************************
*
*  General Purpose IO Register read routine
*
***************************************************************************/
static inline int ixj_gpio_read(IXJ *j)
{
	if (ixj_WriteDSPCommand(0x5143, j))
		return -1;

	j->gpio.bytes.low = j->ssr.low;
	j->gpio.bytes.high = j->ssr.high;

	return 0;
}

static inline void LED_SetState(int state, IXJ *j)
{
	if (j->cardtype == QTI_LINEJACK) {
		j->pld_scrw.bits.led1 = state & 0x1 ? 1 : 0;
		j->pld_scrw.bits.led2 = state & 0x2 ? 1 : 0;
		j->pld_scrw.bits.led3 = state & 0x4 ? 1 : 0;
		j->pld_scrw.bits.led4 = state & 0x8 ? 1 : 0;

		outb(j->pld_scrw.byte, j->XILINXbase);
	}
}

/*********************************************************************
*  GPIO Pins are configured as follows on the Quicknet Internet
*  PhoneJACK Telephony Cards
* 
* POTS Select        GPIO_6=0 GPIO_7=0
* Mic/Speaker Select GPIO_6=0 GPIO_7=1
* Handset Select     GPIO_6=1 GPIO_7=0
*
* SLIC Active        GPIO_1=0 GPIO_2=1 GPIO_5=0
* SLIC Ringing       GPIO_1=1 GPIO_2=1 GPIO_5=0
* SLIC Open Circuit  GPIO_1=0 GPIO_2=0 GPIO_5=0
*
* Hook Switch changes reported on GPIO_3
*********************************************************************/
static int ixj_set_port(IXJ *j, int arg)
{
	if (j->cardtype == QTI_PHONEJACK_LITE) {
		if (arg != PORT_POTS)
			return 10;
		else
			return 0;
	}
	switch (arg) {
	case PORT_POTS:
		j->port = PORT_POTS;
		switch (j->cardtype) {
		case QTI_PHONECARD:
			if (j->flags.pcmciasct == 1)
				SLIC_SetState(PLD_SLIC_STATE_ACTIVE, j);
			else
				return 11;
			break;
		case QTI_PHONEJACK_PCI:
			j->pld_slicw.pcib.mic = 0;
			j->pld_slicw.pcib.spk = 0;
			outb(j->pld_slicw.byte, j->XILINXbase + 0x01);
			break;
		case QTI_LINEJACK:
			ixj_set_pots(j, 0);			/* Disconnect POTS/PSTN relay */
			if (ixj_WriteDSPCommand(0xC528, j))		/* Write CODEC config to
									   Software Control Register */
				return 2;
			j->pld_scrw.bits.daafsyncen = 0;	/* Turn off DAA Frame Sync */

			outb(j->pld_scrw.byte, j->XILINXbase);
			j->pld_clock.byte = 0;
			outb(j->pld_clock.byte, j->XILINXbase + 0x04);
			j->pld_slicw.bits.rly1 = 1;
			j->pld_slicw.bits.spken = 0;
			outb(j->pld_slicw.byte, j->XILINXbase + 0x01);
			ixj_mixer(0x1200, j);	/* Turn Off MIC switch on mixer left */
			ixj_mixer(0x1401, j);	/* Turn On Mono1 switch on mixer left */
			ixj_mixer(0x1300, j);       /* Turn Off MIC switch on mixer right */
			ixj_mixer(0x1501, j);       /* Turn On Mono1 switch on mixer right */
			ixj_mixer(0x0E80, j);	/*Mic mute */
			ixj_mixer(0x0F00, j);	/* Set mono out (SLIC) to 0dB */
			ixj_mixer(0x0080, j);	/* Mute Master Left volume */
			ixj_mixer(0x0180, j);	/* Mute Master Right volume */
			SLIC_SetState(PLD_SLIC_STATE_STANDBY, j);
/*			SLIC_SetState(PLD_SLIC_STATE_ACTIVE, j); */
			break;
		case QTI_PHONEJACK:
			j->gpio.bytes.high = 0x0B;
			j->gpio.bits.gpio6 = 0;
			j->gpio.bits.gpio7 = 0;
			ixj_WriteDSPCommand(j->gpio.word, j);
			break;
		}
		break;
	case PORT_PSTN:
		if (j->cardtype == QTI_LINEJACK) {
			ixj_WriteDSPCommand(0xC534, j);	/* Write CODEC config to Software Control Register */

			j->pld_slicw.bits.rly3 = 0;
			j->pld_slicw.bits.rly1 = 1;
			j->pld_slicw.bits.spken = 0;
			outb(j->pld_slicw.byte, j->XILINXbase + 0x01);
			j->port = PORT_PSTN;
		} else {
			return 4;
		}
		break;
	case PORT_SPEAKER:
		j->port = PORT_SPEAKER;
		switch (j->cardtype) {
		case QTI_PHONECARD:
			if (j->flags.pcmciasct) {
				SLIC_SetState(PLD_SLIC_STATE_OC, j);
			}
			break;
		case QTI_PHONEJACK_PCI:
			j->pld_slicw.pcib.mic = 1;
			j->pld_slicw.pcib.spk = 1;
			outb(j->pld_slicw.byte, j->XILINXbase + 0x01);
			break;
		case QTI_LINEJACK:
			ixj_set_pots(j, 0);			/* Disconnect POTS/PSTN relay */
			if (ixj_WriteDSPCommand(0xC528, j))		/* Write CODEC config to
									   Software Control Register */
				return 2;
			j->pld_scrw.bits.daafsyncen = 0;	/* Turn off DAA Frame Sync */

			outb(j->pld_scrw.byte, j->XILINXbase);
			j->pld_clock.byte = 0;
			outb(j->pld_clock.byte, j->XILINXbase + 0x04);
			j->pld_slicw.bits.rly1 = 1;
			j->pld_slicw.bits.spken = 1;
			outb(j->pld_slicw.byte, j->XILINXbase + 0x01);
			ixj_mixer(0x1201, j);	/* Turn On MIC switch on mixer left */
			ixj_mixer(0x1400, j);	/* Turn Off Mono1 switch on mixer left */
			ixj_mixer(0x1301, j);       /* Turn On MIC switch on mixer right */
			ixj_mixer(0x1500, j);       /* Turn Off Mono1 switch on mixer right */
			ixj_mixer(0x0E06, j);	/*Mic un-mute 0dB */
			ixj_mixer(0x0F80, j);	/* Mute mono out (SLIC) */
			ixj_mixer(0x0000, j);	/* Set Master Left volume to 0dB */
			ixj_mixer(0x0100, j);	/* Set Master Right volume to 0dB */
			break;
		case QTI_PHONEJACK:
			j->gpio.bytes.high = 0x0B;
			j->gpio.bits.gpio6 = 0;
			j->gpio.bits.gpio7 = 1;
			ixj_WriteDSPCommand(j->gpio.word, j);
			break;
		}
		break;
	case PORT_HANDSET:
		if (j->cardtype != QTI_PHONEJACK) {
			return 5;
		} else {
			j->gpio.bytes.high = 0x0B;
			j->gpio.bits.gpio6 = 1;
			j->gpio.bits.gpio7 = 0;
			ixj_WriteDSPCommand(j->gpio.word, j);
			j->port = PORT_HANDSET;
		}
		break;
	default:
		return 6;
		break;
	}
	return 0;
}

static int ixj_set_pots(IXJ *j, int arg)
{
	if (j->cardtype == QTI_LINEJACK) {
		if (arg) {
			if (j->port == PORT_PSTN) {
				j->pld_slicw.bits.rly1 = 0;
				outb(j->pld_slicw.byte, j->XILINXbase + 0x01);
				j->flags.pots_pstn = 1;
				return 1;
			} else {
				j->flags.pots_pstn = 0;
				return 0;
			}
		} else {
			j->pld_slicw.bits.rly1 = 1;
			outb(j->pld_slicw.byte, j->XILINXbase + 0x01);
			j->flags.pots_pstn = 0;
			return 1;
		}
	} else {
		return 0;
	}
}

static void ixj_ring_on(IXJ *j)
{
	if (j->dsp.low == 0x20)	/* Internet PhoneJACK */
	 {
		if (ixjdebug & 0x0004)
			printk(KERN_INFO "IXJ Ring On /dev/phone%d\n", 	j->board);

		j->gpio.bytes.high = 0x0B;
		j->gpio.bytes.low = 0x00;
		j->gpio.bits.gpio1 = 1;
		j->gpio.bits.gpio2 = 1;
		j->gpio.bits.gpio5 = 0;
		ixj_WriteDSPCommand(j->gpio.word, j);	/* send the ring signal */
	} else			/* Internet LineJACK, Internet PhoneJACK Lite or Internet PhoneJACK PCI */
	{
		if (ixjdebug & 0x0004)
			printk(KERN_INFO "IXJ Ring On /dev/phone%d\n", j->board);

		SLIC_SetState(PLD_SLIC_STATE_RINGING, j);
	}
}

static int ixj_siadc(IXJ *j, int val)
{
	if(j->cardtype == QTI_PHONECARD){
		if(j->flags.pcmciascp){
			if(val == -1)
				return j->siadc.bits.rxg;

			if(val < 0 || val > 0x1F)
				return -1;

			j->siadc.bits.hom = 0;				/* Handset Out Mute */
			j->siadc.bits.lom = 0;				/* Line Out Mute */
			j->siadc.bits.rxg = val;			/*(0xC000 - 0x41C8) / 0x4EF;    RX PGA Gain */
			j->psccr.bits.addr = 6;				/* R/W Smart Cable Register Address */
			j->psccr.bits.rw = 0;				/* Read / Write flag */
			j->psccr.bits.dev = 0;
			outb(j->siadc.byte, j->XILINXbase + 0x00);
			outb(j->psccr.byte, j->XILINXbase + 0x01);
			ixj_PCcontrol_wait(j);
			return j->siadc.bits.rxg;
		}
	}
	return -1;
}

static int ixj_sidac(IXJ *j, int val)
{
	if(j->cardtype == QTI_PHONECARD){
		if(j->flags.pcmciascp){
			if(val == -1)
				return j->sidac.bits.txg;

			if(val < 0 || val > 0x1F)
				return -1;

			j->sidac.bits.srm = 1;				/* Speaker Right Mute */
			j->sidac.bits.slm = 1;				/* Speaker Left Mute */
			j->sidac.bits.txg = val;			/* (0xC000 - 0x45E4) / 0x5D3;	 TX PGA Gain */
			j->psccr.bits.addr = 7;				/* R/W Smart Cable Register Address */
			j->psccr.bits.rw = 0;				/* Read / Write flag */
			j->psccr.bits.dev = 0;
			outb(j->sidac.byte, j->XILINXbase + 0x00);
			outb(j->psccr.byte, j->XILINXbase + 0x01);
			ixj_PCcontrol_wait(j);
			return j->sidac.bits.txg;
		}
	}
	return -1;
}

static int ixj_pcmcia_cable_check(IXJ *j)
{
	j->pccr1.byte = inb_p(j->XILINXbase + 0x03);
	if (!j->flags.pcmciastate) {
		j->pccr2.byte = inb_p(j->XILINXbase + 0x02);
		if (j->pccr1.bits.drf || j->pccr2.bits.rstc) {
			j->flags.pcmciastate = 4;
			return 0;
		}
		if (j->pccr1.bits.ed) {
			j->pccr1.bits.ed = 0;
			j->psccr.bits.dev = 3;
			j->psccr.bits.rw = 1;
			outw_p(j->psccr.byte << 8, j->XILINXbase + 0x00);
			ixj_PCcontrol_wait(j);
			j->pslic.byte = inw_p(j->XILINXbase + 0x00) & 0xFF;
			j->pslic.bits.led2 = j->pslic.bits.det ? 1 : 0;
			j->psccr.bits.dev = 3;
			j->psccr.bits.rw = 0;
			outw_p(j->psccr.byte << 8 | j->pslic.byte, j->XILINXbase + 0x00);
			ixj_PCcontrol_wait(j);
			return j->pslic.bits.led2 ? 1 : 0;
		} else if (j->flags.pcmciasct) {
			return j->r_hook;
		} else {
			return 1;
		}
	} else if (j->flags.pcmciastate == 4) {
		if (!j->pccr1.bits.drf) {
			j->flags.pcmciastate = 3;
		}
		return 0;
	} else if (j->flags.pcmciastate == 3) {
		j->pccr2.bits.pwr = 0;
		j->pccr2.bits.rstc = 1;
		outb(j->pccr2.byte, j->XILINXbase + 0x02);
		j->checkwait = jiffies + (hertz * 2);
		j->flags.incheck = 1;
		j->flags.pcmciastate = 2;
		return 0;
	} else if (j->flags.pcmciastate == 2) {
		if (j->flags.incheck) {
			if (time_before(jiffies, j->checkwait)) {
				return 0;
			} else {
				j->flags.incheck = 0;
			}
		}
		j->pccr2.bits.pwr = 0;
		j->pccr2.bits.rstc = 0;
		outb_p(j->pccr2.byte, j->XILINXbase + 0x02);
		j->flags.pcmciastate = 1;
		return 0;
	} else if (j->flags.pcmciastate == 1) {
		j->flags.pcmciastate = 0;
		if (!j->pccr1.bits.drf) {
			j->psccr.bits.dev = 3;
			j->psccr.bits.rw = 1;
			outb_p(j->psccr.byte, j->XILINXbase + 0x01);
			ixj_PCcontrol_wait(j);
			j->flags.pcmciascp = 1;		/* Set Cable Present Flag */

			j->flags.pcmciasct = (inw_p(j->XILINXbase + 0x00) >> 8) & 0x03;		/* Get Cable Type */

			if (j->flags.pcmciasct == 3) {
				j->flags.pcmciastate = 4;
				return 0;
			} else if (j->flags.pcmciasct == 0) {
				j->pccr2.bits.pwr = 1;
				j->pccr2.bits.rstc = 0;
				outb_p(j->pccr2.byte, j->XILINXbase + 0x02);
				j->port = PORT_SPEAKER;
			} else {
				j->port = PORT_POTS;
			}
			j->sic1.bits.cpd = 0;				/* Chip Power Down */
			j->sic1.bits.mpd = 0;				/* MIC Bias Power Down */
			j->sic1.bits.hpd = 0;				/* Handset Bias Power Down */
			j->sic1.bits.lpd = 0;				/* Line Bias Power Down */
			j->sic1.bits.spd = 1;				/* Speaker Drive Power Down */
			j->psccr.bits.addr = 1;				/* R/W Smart Cable Register Address */
			j->psccr.bits.rw = 0;				/* Read / Write flag */
			j->psccr.bits.dev = 0;
			outb(j->sic1.byte, j->XILINXbase + 0x00);
			outb(j->psccr.byte, j->XILINXbase + 0x01);
			ixj_PCcontrol_wait(j);

			j->sic2.bits.al = 0;				/* Analog Loopback DAC analog -> ADC analog */
			j->sic2.bits.dl2 = 0;				/* Digital Loopback DAC -> ADC one bit */
			j->sic2.bits.dl1 = 0;				/* Digital Loopback ADC -> DAC one bit */
			j->sic2.bits.pll = 0;				/* 1 = div 10, 0 = div 5 */
			j->sic2.bits.hpd = 0;				/* HPF disable */
			j->psccr.bits.addr = 2;				/* R/W Smart Cable Register Address */
			j->psccr.bits.rw = 0;				/* Read / Write flag */
			j->psccr.bits.dev = 0;
			outb(j->sic2.byte, j->XILINXbase + 0x00);
			outb(j->psccr.byte, j->XILINXbase + 0x01);
			ixj_PCcontrol_wait(j);

			j->psccr.bits.addr = 3;				/* R/W Smart Cable Register Address */
			j->psccr.bits.rw = 0;				/* Read / Write flag */
			j->psccr.bits.dev = 0;
			outb(0x00, j->XILINXbase + 0x00);		/* PLL Divide N1 */
			outb(j->psccr.byte, j->XILINXbase + 0x01);
			ixj_PCcontrol_wait(j);

			j->psccr.bits.addr = 4;				/* R/W Smart Cable Register Address */
			j->psccr.bits.rw = 0;				/* Read / Write flag */
			j->psccr.bits.dev = 0;
			outb(0x09, j->XILINXbase + 0x00);		/* PLL Multiply M1 */
			outb(j->psccr.byte, j->XILINXbase + 0x01);
			ixj_PCcontrol_wait(j);

			j->sirxg.bits.lig = 1;				/* Line In Gain */
			j->sirxg.bits.lim = 1;				/* Line In Mute */
			j->sirxg.bits.mcg = 0;				/* MIC In Gain was 3 */
			j->sirxg.bits.mcm = 0;				/* MIC In Mute */
			j->sirxg.bits.him = 0;				/* Handset In Mute */
			j->sirxg.bits.iir = 1;				/* IIR */
			j->psccr.bits.addr = 5;				/* R/W Smart Cable Register Address */
			j->psccr.bits.rw = 0;				/* Read / Write flag */
			j->psccr.bits.dev = 0;
			outb(j->sirxg.byte, j->XILINXbase + 0x00);
			outb(j->psccr.byte, j->XILINXbase + 0x01);
			ixj_PCcontrol_wait(j);

			ixj_siadc(j, 0x17);
			ixj_sidac(j, 0x1D);

			j->siaatt.bits.sot = 0;
			j->psccr.bits.addr = 9;				/* R/W Smart Cable Register Address */
			j->psccr.bits.rw = 0;				/* Read / Write flag */
			j->psccr.bits.dev = 0;
			outb(j->siaatt.byte, j->XILINXbase + 0x00);
			outb(j->psccr.byte, j->XILINXbase + 0x01);
			ixj_PCcontrol_wait(j);

			if (j->flags.pcmciasct == 1 && !j->readers && !j->writers) {
				j->psccr.byte = j->pslic.byte = 0;
				j->pslic.bits.powerdown = 1;
				j->psccr.bits.dev = 3;
				j->psccr.bits.rw = 0;
				outw_p(j->psccr.byte << 8 | j->pslic.byte, j->XILINXbase + 0x00);
				ixj_PCcontrol_wait(j);
			}
		}
		return 0;
	} else {
		j->flags.pcmciascp = 0;
		return 0;
	}
	return 0;
}

static int ixj_hookstate(IXJ *j)
{
	int fOffHook = 0;

	switch (j->cardtype) {
	case QTI_PHONEJACK:
		ixj_gpio_read(j);
		fOffHook = j->gpio.bits.gpio3read ? 1 : 0;
		break;
	case QTI_LINEJACK:
	case QTI_PHONEJACK_LITE:
	case QTI_PHONEJACK_PCI:
		SLIC_GetState(j);
		if(j->cardtype == QTI_LINEJACK && j->flags.pots_pstn == 1 && (j->readers || j->writers)) {
			fOffHook = j->pld_slicr.bits.potspstn ? 1 : 0;
			if(fOffHook != j->p_hook) {
				if(!j->checkwait) {
					j->checkwait = jiffies;
				} 
				if(time_before(jiffies, j->checkwait + 2)) {
					fOffHook ^= 1;
				} else {
					j->checkwait = 0;
				}
				j->p_hook = fOffHook;
	 			printk("IXJ : /dev/phone%d pots-pstn hookstate check %d at %ld\n", j->board, fOffHook, jiffies);
			}
		} else {
			if (j->pld_slicr.bits.state == PLD_SLIC_STATE_ACTIVE ||
			    j->pld_slicr.bits.state == PLD_SLIC_STATE_STANDBY) {
				if (j->flags.ringing || j->flags.cringing) {
					if (!in_interrupt()) {
						msleep(20);
					}
					SLIC_GetState(j);
					if (j->pld_slicr.bits.state == PLD_SLIC_STATE_RINGING) {
						ixj_ring_on(j);
					}
				}
				if (j->cardtype == QTI_PHONEJACK_PCI) {
					j->pld_scrr.byte = inb_p(j->XILINXbase);
					fOffHook = j->pld_scrr.pcib.det ? 1 : 0;
				} else
					fOffHook = j->pld_slicr.bits.det ? 1 : 0;
			}
		}
		break;
	case QTI_PHONECARD:
		fOffHook = ixj_pcmcia_cable_check(j);
		break;
	}
	if (j->r_hook != fOffHook) {
		j->r_hook = fOffHook;
		if (j->port == PORT_SPEAKER || j->port == PORT_HANDSET) { // || (j->port == PORT_PSTN && j->flags.pots_pstn == 0)) {
			j->ex.bits.hookstate = 1;
			ixj_kill_fasync(j, SIG_HOOKSTATE, POLL_IN);
		} else if (!fOffHook) {
			j->flash_end = jiffies + ((60 * hertz) / 100);
		}
	}
	if (fOffHook) {
		if(time_before(jiffies, j->flash_end)) {
			j->ex.bits.flash = 1;
			j->flash_end = 0;
			ixj_kill_fasync(j, SIG_FLASH, POLL_IN);
		}
	} else {
		if(time_before(jiffies, j->flash_end)) {
			fOffHook = 1;
		}
	}

	if (j->port == PORT_PSTN && j->daa_mode == SOP_PU_CONVERSATION)
		fOffHook |= 2;

	if (j->port == PORT_SPEAKER) {
		if(j->cardtype == QTI_PHONECARD) {
			if(j->flags.pcmciascp && j->flags.pcmciasct) {
				fOffHook |= 2;
			}
		} else {
			fOffHook |= 2;
		}
	}

	if (j->port == PORT_HANDSET)
		fOffHook |= 2;

	return fOffHook;
}

static void ixj_ring_off(IXJ *j)
{
	if (j->dsp.low == 0x20)	/* Internet PhoneJACK */
	 {
		if (ixjdebug & 0x0004)
			printk(KERN_INFO "IXJ Ring Off\n");
		j->gpio.bytes.high = 0x0B;
		j->gpio.bytes.low = 0x00;
		j->gpio.bits.gpio1 = 0;
		j->gpio.bits.gpio2 = 1;
		j->gpio.bits.gpio5 = 0;
		ixj_WriteDSPCommand(j->gpio.word, j);
	} else			/* Internet LineJACK */
	{
		if (ixjdebug & 0x0004)
			printk(KERN_INFO "IXJ Ring Off\n");

		if(!j->flags.cidplay)
			SLIC_SetState(PLD_SLIC_STATE_STANDBY, j);

		SLIC_GetState(j);
	}
}

static void ixj_ring_start(IXJ *j)
{
	j->flags.cringing = 1;
	if (ixjdebug & 0x0004)
		printk(KERN_INFO "IXJ Cadence Ringing Start /dev/phone%d\n", j->board);
	if (ixj_hookstate(j) & 1) {
		if (j->port == PORT_POTS)
			ixj_ring_off(j);
		j->flags.cringing = 0;
		if (ixjdebug & 0x0004)
			printk(KERN_INFO "IXJ Cadence Ringing Stopped /dev/phone%d off hook\n", j->board);
	} else if(j->cadence_f[5].enable && (!j->cadence_f[5].en_filter)) {
		j->ring_cadence_jif = jiffies;
		j->flags.cidsent = j->flags.cidring = 0;
		j->cadence_f[5].state = 0;
		if(j->cadence_f[5].on1)
			ixj_ring_on(j);
	} else {
		j->ring_cadence_jif = jiffies;
		j->ring_cadence_t = 15;
		if (j->ring_cadence & 1 << j->ring_cadence_t) {
			ixj_ring_on(j);
		} else {
			ixj_ring_off(j);
		}
		j->flags.cidsent = j->flags.cidring = j->flags.firstring = 0;
	}
}

static int ixj_ring(IXJ *j)
{
	char cntr;
	unsigned long jif;

	j->flags.ringing = 1;
	if (ixj_hookstate(j) & 1) {
		ixj_ring_off(j);
		j->flags.ringing = 0;
		return 1;
	}
	for (cntr = 0; cntr < j->maxrings; cntr++) {
		jif = jiffies + (1 * hertz);
		ixj_ring_on(j);
		while (time_before(jiffies, jif)) {
			if (ixj_hookstate(j) & 1) {
				ixj_ring_off(j);
				j->flags.ringing = 0;
				return 1;
			}
			schedule_timeout_interruptible(1);
			if (signal_pending(current))
				break;
		}
		jif = jiffies + (3 * hertz);
		ixj_ring_off(j);
		while (time_before(jiffies, jif)) {
			if (ixj_hookstate(j) & 1) {
				msleep(10);
				if (ixj_hookstate(j) & 1) {
					j->flags.ringing = 0;
					return 1;
				}
			}
			schedule_timeout_interruptible(1);
			if (signal_pending(current))
				break;
		}
	}
	ixj_ring_off(j);
	j->flags.ringing = 0;
	return 0;
}

static int ixj_open(struct phone_device *p, struct file *file_p)
{
	IXJ *j = get_ixj(p->board);
	file_p->private_data = j;

	if (!j->DSPbase)
		return -ENODEV;

        if (file_p->f_mode & FMODE_READ) {
		if(!j->readers) {
	                j->readers++;
        	} else {
                	return -EBUSY;
		}
        }

	if (file_p->f_mode & FMODE_WRITE) {
		if(!j->writers) {
			j->writers++;
		} else {
			if (file_p->f_mode & FMODE_READ){
				j->readers--;
			}
			return -EBUSY;
		}
	}

	if (j->cardtype == QTI_PHONECARD) {
		j->pslic.bits.powerdown = 0;
		j->psccr.bits.dev = 3;
		j->psccr.bits.rw = 0;
		outw_p(j->psccr.byte << 8 | j->pslic.byte, j->XILINXbase + 0x00);
		ixj_PCcontrol_wait(j);
	}

	j->flags.cidplay = 0;
	j->flags.cidcw_ack = 0;

	if (ixjdebug & 0x0002)
		printk(KERN_INFO "Opening board %d\n", p->board);

	j->framesread = j->frameswritten = 0;
	return 0;
}

static int ixj_release(struct inode *inode, struct file *file_p)
{
	IXJ_TONE ti;
	int cnt;
	IXJ *j = file_p->private_data;
	int board = j->p.board;

	/*
	 *    Set up locks to ensure that only one process is talking to the DSP at a time.
	 *    This is necessary to keep the DSP from locking up.
	 */
	while(test_and_set_bit(board, (void *)&j->busyflags) != 0)
		schedule_timeout_interruptible(1);
	if (ixjdebug & 0x0002)
		printk(KERN_INFO "Closing board %d\n", NUM(inode));

	if (j->cardtype == QTI_PHONECARD)
		ixj_set_port(j, PORT_SPEAKER);
	else
		ixj_set_port(j, PORT_POTS);

	aec_stop(j);
	ixj_play_stop(j);
	ixj_record_stop(j);
	set_play_volume(j, 0x100);
	set_rec_volume(j, 0x100);
	ixj_ring_off(j);

	/* Restore the tone table to default settings. */
	ti.tone_index = 10;
	ti.gain0 = 1;
	ti.freq0 = hz941;
	ti.gain1 = 0;
	ti.freq1 = hz1209;
	ixj_init_tone(j, &ti);
	ti.tone_index = 11;
	ti.gain0 = 1;
	ti.freq0 = hz941;
	ti.gain1 = 0;
	ti.freq1 = hz1336;
	ixj_init_tone(j, &ti);
	ti.tone_index = 12;
	ti.gain0 = 1;
	ti.freq0 = hz941;
	ti.gain1 = 0;
	ti.freq1 = hz1477;
	ixj_init_tone(j, &ti);
	ti.tone_index = 13;
	ti.gain0 = 1;
	ti.freq0 = hz800;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);
	ti.tone_index = 14;
	ti.gain0 = 1;
	ti.freq0 = hz1000;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);
	ti.tone_index = 15;
	ti.gain0 = 1;
	ti.freq0 = hz1250;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);
	ti.tone_index = 16;
	ti.gain0 = 1;
	ti.freq0 = hz950;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);
	ti.tone_index = 17;
	ti.gain0 = 1;
	ti.freq0 = hz1100;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);
	ti.tone_index = 18;
	ti.gain0 = 1;
	ti.freq0 = hz1400;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);
	ti.tone_index = 19;
	ti.gain0 = 1;
	ti.freq0 = hz1500;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);
	ti.tone_index = 20;
	ti.gain0 = 1;
	ti.freq0 = hz1600;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);
	ti.tone_index = 21;
	ti.gain0 = 1;
	ti.freq0 = hz1800;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);
	ti.tone_index = 22;
	ti.gain0 = 1;
	ti.freq0 = hz2100;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);
	ti.tone_index = 23;
	ti.gain0 = 1;
	ti.freq0 = hz1300;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);
	ti.tone_index = 24;
	ti.gain0 = 1;
	ti.freq0 = hz2450;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);
	ti.tone_index = 25;
	ti.gain0 = 1;
	ti.freq0 = hz350;
	ti.gain1 = 0;
	ti.freq1 = hz440;
	ixj_init_tone(j, &ti);
	ti.tone_index = 26;
	ti.gain0 = 1;
	ti.freq0 = hz440;
	ti.gain1 = 0;
	ti.freq1 = hz480;
	ixj_init_tone(j, &ti);
	ti.tone_index = 27;
	ti.gain0 = 1;
	ti.freq0 = hz480;
	ti.gain1 = 0;
	ti.freq1 = hz620;
	ixj_init_tone(j, &ti);

	set_rec_depth(j, 2);	/* Set Record Channel Limit to 2 frames */

	set_play_depth(j, 2);	/* Set Playback Channel Limit to 2 frames */

	j->ex.bits.dtmf_ready = 0;
	j->dtmf_state = 0;
	j->dtmf_wp = j->dtmf_rp = 0;
	j->rec_mode = j->play_mode = -1;
	j->flags.ringing = 0;
	j->maxrings = MAXRINGS;
	j->ring_cadence = USA_RING_CADENCE;
	if(j->cadence_f[5].enable) {
		j->cadence_f[5].enable = j->cadence_f[5].en_filter = j->cadence_f[5].state = 0;
	}
	j->drybuffer = 0;
	j->winktime = 320;
	j->flags.dtmf_oob = 0;
	for (cnt = 0; cnt < 4; cnt++)
		j->cadence_f[cnt].enable = 0;

	idle(j);

	if(j->cardtype == QTI_PHONECARD) {
		SLIC_SetState(PLD_SLIC_STATE_OC, j);
	}

	if (file_p->f_mode & FMODE_READ)
		j->readers--;
	if (file_p->f_mode & FMODE_WRITE)
		j->writers--;

	if (j->read_buffer && !j->readers) {
		kfree(j->read_buffer);
		j->read_buffer = NULL;
		j->read_buffer_size = 0;
	}
	if (j->write_buffer && !j->writers) {
		kfree(j->write_buffer);
		j->write_buffer = NULL;
		j->write_buffer_size = 0;
	}
	j->rec_codec = j->play_codec = 0;
	j->rec_frame_size = j->play_frame_size = 0;
	j->flags.cidsent = j->flags.cidring = 0;

	if(j->cardtype == QTI_LINEJACK && !j->readers && !j->writers) {
		ixj_set_port(j, PORT_PSTN);
		daa_set_mode(j, SOP_PU_SLEEP);
		ixj_set_pots(j, 1);
	}
	ixj_WriteDSPCommand(0x0FE3, j);	/* Put the DSP in 1/5 power mode. */

	/* Set up the default signals for events */
	for (cnt = 0; cnt < 35; cnt++)
		j->ixj_signals[cnt] = SIGIO;

	/* Set the excetion signal enable flags */
	j->ex_sig.bits.dtmf_ready = j->ex_sig.bits.hookstate = j->ex_sig.bits.flash = j->ex_sig.bits.pstn_ring = 
	j->ex_sig.bits.caller_id = j->ex_sig.bits.pstn_wink = j->ex_sig.bits.f0 = j->ex_sig.bits.f1 = j->ex_sig.bits.f2 = 
	j->ex_sig.bits.f3 = j->ex_sig.bits.fc0 = j->ex_sig.bits.fc1 = j->ex_sig.bits.fc2 = j->ex_sig.bits.fc3 = 1;

	file_p->private_data = NULL;
	clear_bit(board, &j->busyflags);
	return 0;
}

static int read_filters(IXJ *j)
{
	unsigned short fc, cnt, trg;
	int var;

	trg = 0;
	if (ixj_WriteDSPCommand(0x5144, j)) {
		if(ixjdebug & 0x0001) {
			printk(KERN_INFO "Read Frame Counter failed!\n");
		}
		return -1;
	}
	fc = j->ssr.high << 8 | j->ssr.low;
	if (fc == j->frame_count)
		return 1;

	j->frame_count = fc;

	if (j->dtmf_proc)
		return 1;

	var = 10;

	for (cnt = 0; cnt < 4; cnt++) {
		if (ixj_WriteDSPCommand(0x5154 + cnt, j)) {
			if(ixjdebug & 0x0001) {
				printk(KERN_INFO "Select Filter %d failed!\n", cnt);
			}
			return -1;
		}
		if (ixj_WriteDSPCommand(0x515C, j)) {
			if(ixjdebug & 0x0001) {
				printk(KERN_INFO "Read Filter History %d failed!\n", cnt);
			}
			return -1;
		}
		j->filter_hist[cnt] = j->ssr.high << 8 | j->ssr.low;

		if (j->cadence_f[cnt].enable) {
			if (j->filter_hist[cnt] & 3 && !(j->filter_hist[cnt] & 12)) {
				if (j->cadence_f[cnt].state == 0) {
					j->cadence_f[cnt].state = 1;
					j->cadence_f[cnt].on1min = jiffies + (long)((j->cadence_f[cnt].on1 * (hertz * (100 - var)) / 10000));
					j->cadence_f[cnt].on1dot = jiffies + (long)((j->cadence_f[cnt].on1 * (hertz * (100)) / 10000));
					j->cadence_f[cnt].on1max = jiffies + (long)((j->cadence_f[cnt].on1 * (hertz * (100 + var)) / 10000));
				} else if (j->cadence_f[cnt].state == 2 &&
					   (time_after(jiffies, j->cadence_f[cnt].off1min) &&
					    time_before(jiffies, j->cadence_f[cnt].off1max))) {
					if (j->cadence_f[cnt].on2) {
						j->cadence_f[cnt].state = 3;
						j->cadence_f[cnt].on2min = jiffies + (long)((j->cadence_f[cnt].on2 * (hertz * (100 - var)) / 10000));
						j->cadence_f[cnt].on2dot = jiffies + (long)((j->cadence_f[cnt].on2 * (hertz * (100)) / 10000));
						j->cadence_f[cnt].on2max = jiffies + (long)((j->cadence_f[cnt].on2 * (hertz * (100 + var)) / 10000));
					} else {
						j->cadence_f[cnt].state = 7;
					}
				} else if (j->cadence_f[cnt].state == 4 &&
					   (time_after(jiffies, j->cadence_f[cnt].off2min) &&
					    time_before(jiffies, j->cadence_f[cnt].off2max))) {
					if (j->cadence_f[cnt].on3) {
						j->cadence_f[cnt].state = 5;
						j->cadence_f[cnt].on3min = jiffies + (long)((j->cadence_f[cnt].on3 * (hertz * (100 - var)) / 10000));
						j->cadence_f[cnt].on3dot = jiffies + (long)((j->cadence_f[cnt].on3 * (hertz * (100)) / 10000));
						j->cadence_f[cnt].on3max = jiffies + (long)((j->cadence_f[cnt].on3 * (hertz * (100 + var)) / 10000));
					} else {
						j->cadence_f[cnt].state = 7;
					}
				} else {
					j->cadence_f[cnt].state = 0;
				}
			} else if (j->filter_hist[cnt] & 12 && !(j->filter_hist[cnt] & 3)) {
				if (j->cadence_f[cnt].state == 1) {
					if(!j->cadence_f[cnt].on1) {
						j->cadence_f[cnt].state = 7;
					} else if((time_after(jiffies, j->cadence_f[cnt].on1min) &&
					  time_before(jiffies, j->cadence_f[cnt].on1max))) {
						if(j->cadence_f[cnt].off1) {
							j->cadence_f[cnt].state = 2;
							j->cadence_f[cnt].off1min = jiffies + (long)((j->cadence_f[cnt].off1 * (hertz * (100 - var)) / 10000));
							j->cadence_f[cnt].off1dot = jiffies + (long)((j->cadence_f[cnt].off1 * (hertz * (100)) / 10000));
							j->cadence_f[cnt].off1max = jiffies + (long)((j->cadence_f[cnt].off1 * (hertz * (100 + var)) / 10000));
						} else {
							j->cadence_f[cnt].state = 7;
						}
					} else {
						j->cadence_f[cnt].state = 0;
					}
				} else if (j->cadence_f[cnt].state == 3) {
					if((time_after(jiffies, j->cadence_f[cnt].on2min) &&
					    time_before(jiffies, j->cadence_f[cnt].on2max))) {
						if(j->cadence_f[cnt].off2) {
							j->cadence_f[cnt].state = 4;
							j->cadence_f[cnt].off2min = jiffies + (long)((j->cadence_f[cnt].off2 * (hertz * (100 - var)) / 10000));
							j->cadence_f[cnt].off2dot = jiffies + (long)((j->cadence_f[cnt].off2 * (hertz * (100)) / 10000));
							j->cadence_f[cnt].off2max = jiffies + (long)((j->cadence_f[cnt].off2 * (hertz * (100 + var)) / 10000));
						} else {
							j->cadence_f[cnt].state = 7;
						}
					} else {
						j->cadence_f[cnt].state = 0;
					}
				} else if (j->cadence_f[cnt].state == 5) {
					if ((time_after(jiffies, j->cadence_f[cnt].on3min) &&
					    time_before(jiffies, j->cadence_f[cnt].on3max))) {
						if(j->cadence_f[cnt].off3) {
							j->cadence_f[cnt].state = 6;
							j->cadence_f[cnt].off3min = jiffies + (long)((j->cadence_f[cnt].off3 * (hertz * (100 - var)) / 10000));
							j->cadence_f[cnt].off3dot = jiffies + (long)((j->cadence_f[cnt].off3 * (hertz * (100)) / 10000));
							j->cadence_f[cnt].off3max = jiffies + (long)((j->cadence_f[cnt].off3 * (hertz * (100 + var)) / 10000));
						} else {
							j->cadence_f[cnt].state = 7;
						}
					} else {
						j->cadence_f[cnt].state = 0;
					}
				} else {
					j->cadence_f[cnt].state = 0;
				}
			} else {
				switch(j->cadence_f[cnt].state) {
					case 1:
						if(time_after(jiffies, j->cadence_f[cnt].on1dot) &&
						   !j->cadence_f[cnt].off1 &&
						   !j->cadence_f[cnt].on2 && !j->cadence_f[cnt].off2 &&
						   !j->cadence_f[cnt].on3 && !j->cadence_f[cnt].off3) {
							j->cadence_f[cnt].state = 7;
						}
						break;
					case 3:
						if(time_after(jiffies, j->cadence_f[cnt].on2dot) &&
						   !j->cadence_f[cnt].off2 &&
						   !j->cadence_f[cnt].on3 && !j->cadence_f[cnt].off3) {
							j->cadence_f[cnt].state = 7;
						}
						break;
					case 5:
						if(time_after(jiffies, j->cadence_f[cnt].on3dot) &&
						   !j->cadence_f[cnt].off3) {
							j->cadence_f[cnt].state = 7;
						}
						break;
				}
			}

			if (ixjdebug & 0x0040) {
				printk(KERN_INFO "IXJ Tone Cadence state = %d /dev/phone%d at %ld\n", j->cadence_f[cnt].state, j->board, jiffies);
				switch(j->cadence_f[cnt].state) {
					case 0:
						printk(KERN_INFO "IXJ /dev/phone%d No Tone detected\n", j->board);
						break;
					case 1:
						printk(KERN_INFO "IXJ /dev/phone%d Next Tone Cadence state at %u %ld - %ld - %ld\n", j->board,
					j->cadence_f[cnt].on1, j->cadence_f[cnt].on1min, j->cadence_f[cnt].on1dot, j->cadence_f[cnt].on1max);
						break;
					case 2:
						printk(KERN_INFO "IXJ /dev/phone%d Next Tone Cadence state at %ld - %ld\n", j->board, j->cadence_f[cnt].off1min, 
															j->cadence_f[cnt].off1max);
						break;
					case 3:
						printk(KERN_INFO "IXJ /dev/phone%d Next Tone Cadence state at %ld - %ld\n", j->board, j->cadence_f[cnt].on2min,
															j->cadence_f[cnt].on2max);
						break;
					case 4:
						printk(KERN_INFO "IXJ /dev/phone%d Next Tone Cadence state at %ld - %ld\n", j->board, j->cadence_f[cnt].off2min,
															j->cadence_f[cnt].off2max);
						break;
					case 5:
						printk(KERN_INFO "IXJ /dev/phone%d Next Tone Cadence state at %ld - %ld\n", j->board, j->cadence_f[cnt].on3min,
															j->cadence_f[cnt].on3max);
						break;
					case 6:	
						printk(KERN_INFO "IXJ /dev/phone%d Next Tone Cadence state at %ld - %ld\n", j->board, j->cadence_f[cnt].off3min,
															j->cadence_f[cnt].off3max);
						break;
				}
			} 
		}
		if (j->cadence_f[cnt].state == 7) {
			j->cadence_f[cnt].state = 0;
			if (j->cadence_f[cnt].enable == 1)
				j->cadence_f[cnt].enable = 0;
			switch (cnt) {
			case 0:
				if(ixjdebug & 0x0020) {
					printk(KERN_INFO "Filter Cadence 0 triggered %ld\n", jiffies);
				}
				j->ex.bits.fc0 = 1;
				ixj_kill_fasync(j, SIG_FC0, POLL_IN);
				break;
			case 1:
				if(ixjdebug & 0x0020) {
					printk(KERN_INFO "Filter Cadence 1 triggered %ld\n", jiffies);
				}
				j->ex.bits.fc1 = 1;
				ixj_kill_fasync(j, SIG_FC1, POLL_IN);
				break;
			case 2:
				if(ixjdebug & 0x0020) {
					printk(KERN_INFO "Filter Cadence 2 triggered %ld\n", jiffies);
				}
				j->ex.bits.fc2 = 1;
				ixj_kill_fasync(j, SIG_FC2, POLL_IN);
				break;
			case 3:
				if(ixjdebug & 0x0020) {
					printk(KERN_INFO "Filter Cadence 3 triggered %ld\n", jiffies);
				}
				j->ex.bits.fc3 = 1;
				ixj_kill_fasync(j, SIG_FC3, POLL_IN);
				break;
			}
		}
		if (j->filter_en[cnt] && ((j->filter_hist[cnt] & 3 && !(j->filter_hist[cnt] & 12)) ||
					  (j->filter_hist[cnt] & 12 && !(j->filter_hist[cnt] & 3)))) {
			if((j->filter_hist[cnt] & 3 && !(j->filter_hist[cnt] & 12))) {
				trg = 1;
			} else if((j->filter_hist[cnt] & 12 && !(j->filter_hist[cnt] & 3))) {
				trg = 0;
			}
			switch (cnt) {
			case 0:
				if(ixjdebug & 0x0020) {
					printk(KERN_INFO "Filter 0 triggered %d at %ld\n", trg, jiffies);
				}
				j->ex.bits.f0 = 1;
				ixj_kill_fasync(j, SIG_F0, POLL_IN);
				break;
			case 1:
				if(ixjdebug & 0x0020) {
					printk(KERN_INFO "Filter 1 triggered %d at %ld\n", trg, jiffies);
				}
				j->ex.bits.f1 = 1;
				ixj_kill_fasync(j, SIG_F1, POLL_IN);
				break;
			case 2:
				if(ixjdebug & 0x0020) {
					printk(KERN_INFO "Filter 2 triggered %d at %ld\n", trg, jiffies);
				}
				j->ex.bits.f2 = 1;
				ixj_kill_fasync(j, SIG_F2, POLL_IN);
				break;
			case 3:
				if(ixjdebug & 0x0020) {
					printk(KERN_INFO "Filter 3 triggered %d at %ld\n", trg, jiffies);
				}
				j->ex.bits.f3 = 1;
				ixj_kill_fasync(j, SIG_F3, POLL_IN);
				break;
			}
		}
	}
	return 0;
}

static int LineMonitor(IXJ *j)
{
	if (j->dtmf_proc) {
		return -1;
	}
	j->dtmf_proc = 1;

	if (ixj_WriteDSPCommand(0x7000, j))		/* Line Monitor */
		return -1;

	j->dtmf.bytes.high = j->ssr.high;
	j->dtmf.bytes.low = j->ssr.low;
	if (!j->dtmf_state && j->dtmf.bits.dtmf_valid) {
		j->dtmf_state = 1;
		j->dtmf_current = j->dtmf.bits.digit;
	}
	if (j->dtmf_state && !j->dtmf.bits.dtmf_valid)	/* && j->dtmf_wp != j->dtmf_rp) */
	 {
		if(!j->cidcw_wait) {
			j->dtmfbuffer[j->dtmf_wp] = j->dtmf_current;
			j->dtmf_wp++;
			if (j->dtmf_wp == 79)
				j->dtmf_wp = 0;
			j->ex.bits.dtmf_ready = 1;
			if(j->ex_sig.bits.dtmf_ready) {
				ixj_kill_fasync(j, SIG_DTMF_READY, POLL_IN);
			}
		}
		else if(j->dtmf_current == 0x00 || j->dtmf_current == 0x0D) {
			if(ixjdebug & 0x0020) {
				printk("IXJ phone%d saw CIDCW Ack DTMF %d from display at %ld\n", j->board, j->dtmf_current, jiffies);
			}
			j->flags.cidcw_ack = 1;
		}
		j->dtmf_state = 0;
	}
	j->dtmf_proc = 0;

	return 0;
}

/************************************************************************
*
* Functions to allow alaw <-> ulaw conversions.
*
************************************************************************/

static void ulaw2alaw(unsigned char *buff, unsigned long len)
{
	static unsigned char table_ulaw2alaw[] =
	{
		0x2A, 0x2B, 0x28, 0x29, 0x2E, 0x2F, 0x2C, 0x2D, 
		0x22, 0x23, 0x20, 0x21, 0x26, 0x27, 0x24, 0x25, 
		0x3A, 0x3B, 0x38, 0x39, 0x3E, 0x3F, 0x3C, 0x3D, 
		0x32, 0x33, 0x30, 0x31, 0x36, 0x37, 0x34, 0x35, 
		0x0B, 0x08, 0x09, 0x0E, 0x0F, 0x0C, 0x0D, 0x02, 
		0x03, 0x00, 0x01, 0x06, 0x07, 0x04, 0x05, 0x1A, 
		0x1B, 0x18, 0x19, 0x1E, 0x1F, 0x1C, 0x1D, 0x12, 
		0x13, 0x10, 0x11, 0x16, 0x17, 0x14, 0x15, 0x6B, 
		0x68, 0x69, 0x6E, 0x6F, 0x6C, 0x6D, 0x62, 0x63, 
		0x60, 0x61, 0x66, 0x67, 0x64, 0x65, 0x7B, 0x79, 
		0x7E, 0x7F, 0x7C, 0x7D, 0x72, 0x73, 0x70, 0x71, 
		0x76, 0x77, 0x74, 0x75, 0x4B, 0x49, 0x4F, 0x4D, 
		0x42, 0x43, 0x40, 0x41, 0x46, 0x47, 0x44, 0x45, 
		0x5A, 0x5B, 0x58, 0x59, 0x5E, 0x5F, 0x5C, 0x5D, 
		0x52, 0x52, 0x53, 0x53, 0x50, 0x50, 0x51, 0x51, 
		0x56, 0x56, 0x57, 0x57, 0x54, 0x54, 0x55, 0xD5, 
		0xAA, 0xAB, 0xA8, 0xA9, 0xAE, 0xAF, 0xAC, 0xAD, 
		0xA2, 0xA3, 0xA0, 0xA1, 0xA6, 0xA7, 0xA4, 0xA5, 
		0xBA, 0xBB, 0xB8, 0xB9, 0xBE, 0xBF, 0xBC, 0xBD, 
		0xB2, 0xB3, 0xB0, 0xB1, 0xB6, 0xB7, 0xB4, 0xB5, 
		0x8B, 0x88, 0x89, 0x8E, 0x8F, 0x8C, 0x8D, 0x82, 
		0x83, 0x80, 0x81, 0x86, 0x87, 0x84, 0x85, 0x9A, 
		0x9B, 0x98, 0x99, 0x9E, 0x9F, 0x9C, 0x9D, 0x92, 
		0x93, 0x90, 0x91, 0x96, 0x97, 0x94, 0x95, 0xEB, 
		0xE8, 0xE9, 0xEE, 0xEF, 0xEC, 0xED, 0xE2, 0xE3, 
		0xE0, 0xE1, 0xE6, 0xE7, 0xE4, 0xE5, 0xFB, 0xF9, 
		0xFE, 0xFF, 0xFC, 0xFD, 0xF2, 0xF3, 0xF0, 0xF1, 
		0xF6, 0xF7, 0xF4, 0xF5, 0xCB, 0xC9, 0xCF, 0xCD, 
		0xC2, 0xC3, 0xC0, 0xC1, 0xC6, 0xC7, 0xC4, 0xC5, 
		0xDA, 0xDB, 0xD8, 0xD9, 0xDE, 0xDF, 0xDC, 0xDD, 
		0xD2, 0xD2, 0xD3, 0xD3, 0xD0, 0xD0, 0xD1, 0xD1, 
		0xD6, 0xD6, 0xD7, 0xD7, 0xD4, 0xD4, 0xD5, 0xD5
	};

	while (len--)
	{
		*buff = table_ulaw2alaw[*(unsigned char *)buff];
		buff++;
	}
}

static void alaw2ulaw(unsigned char *buff, unsigned long len)
{
	static unsigned char table_alaw2ulaw[] =
	{
		0x29, 0x2A, 0x27, 0x28, 0x2D, 0x2E, 0x2B, 0x2C, 
		0x21, 0x22, 0x1F, 0x20, 0x25, 0x26, 0x23, 0x24, 
		0x39, 0x3A, 0x37, 0x38, 0x3D, 0x3E, 0x3B, 0x3C, 
		0x31, 0x32, 0x2F, 0x30, 0x35, 0x36, 0x33, 0x34, 
		0x0A, 0x0B, 0x08, 0x09, 0x0E, 0x0F, 0x0C, 0x0D, 
		0x02, 0x03, 0x00, 0x01, 0x06, 0x07, 0x04, 0x05, 
		0x1A, 0x1B, 0x18, 0x19, 0x1E, 0x1F, 0x1C, 0x1D, 
		0x12, 0x13, 0x10, 0x11, 0x16, 0x17, 0x14, 0x15, 
		0x62, 0x63, 0x60, 0x61, 0x66, 0x67, 0x64, 0x65, 
		0x5D, 0x5D, 0x5C, 0x5C, 0x5F, 0x5F, 0x5E, 0x5E, 
		0x74, 0x76, 0x70, 0x72, 0x7C, 0x7E, 0x78, 0x7A, 
		0x6A, 0x6B, 0x68, 0x69, 0x6E, 0x6F, 0x6C, 0x6D, 
		0x48, 0x49, 0x46, 0x47, 0x4C, 0x4D, 0x4A, 0x4B, 
		0x40, 0x41, 0x3F, 0x3F, 0x44, 0x45, 0x42, 0x43, 
		0x56, 0x57, 0x54, 0x55, 0x5A, 0x5B, 0x58, 0x59, 
		0x4F, 0x4F, 0x4E, 0x4E, 0x52, 0x53, 0x50, 0x51, 
		0xA9, 0xAA, 0xA7, 0xA8, 0xAD, 0xAE, 0xAB, 0xAC, 
		0xA1, 0xA2, 0x9F, 0xA0, 0xA5, 0xA6, 0xA3, 0xA4, 
		0xB9, 0xBA, 0xB7, 0xB8, 0xBD, 0xBE, 0xBB, 0xBC, 
		0xB1, 0xB2, 0xAF, 0xB0, 0xB5, 0xB6, 0xB3, 0xB4, 
		0x8A, 0x8B, 0x88, 0x89, 0x8E, 0x8F, 0x8C, 0x8D, 
		0x82, 0x83, 0x80, 0x81, 0x86, 0x87, 0x84, 0x85, 
		0x9A, 0x9B, 0x98, 0x99, 0x9E, 0x9F, 0x9C, 0x9D, 
		0x92, 0x93, 0x90, 0x91, 0x96, 0x97, 0x94, 0x95, 
		0xE2, 0xE3, 0xE0, 0xE1, 0xE6, 0xE7, 0xE4, 0xE5, 
		0xDD, 0xDD, 0xDC, 0xDC, 0xDF, 0xDF, 0xDE, 0xDE, 
		0xF4, 0xF6, 0xF0, 0xF2, 0xFC, 0xFE, 0xF8, 0xFA, 
		0xEA, 0xEB, 0xE8, 0xE9, 0xEE, 0xEF, 0xEC, 0xED, 
		0xC8, 0xC9, 0xC6, 0xC7, 0xCC, 0xCD, 0xCA, 0xCB, 
		0xC0, 0xC1, 0xBF, 0xBF, 0xC4, 0xC5, 0xC2, 0xC3, 
		0xD6, 0xD7, 0xD4, 0xD5, 0xDA, 0xDB, 0xD8, 0xD9, 
		0xCF, 0xCF, 0xCE, 0xCE, 0xD2, 0xD3, 0xD0, 0xD1
	};

        while (len--)
        {
                *buff = table_alaw2ulaw[*(unsigned char *)buff];
                buff++;
	}
}

static ssize_t ixj_read(struct file * file_p, char __user *buf, size_t length, loff_t * ppos)
{
	unsigned long i = *ppos;
	IXJ * j = get_ixj(NUM(file_p->f_path.dentry->d_inode));

	DECLARE_WAITQUEUE(wait, current);

	if (j->flags.inread)
		return -EALREADY;

	j->flags.inread = 1;

	add_wait_queue(&j->read_q, &wait);
	set_current_state(TASK_INTERRUPTIBLE);
	mb();

	while (!j->read_buffer_ready || (j->dtmf_state && j->flags.dtmf_oob)) {
		++j->read_wait;
		if (file_p->f_flags & O_NONBLOCK) {
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&j->read_q, &wait);
			j->flags.inread = 0;
			return -EAGAIN;
		}
		if (!ixj_hookstate(j)) {
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&j->read_q, &wait);
			j->flags.inread = 0;
			return 0;
		}
		interruptible_sleep_on(&j->read_q);
		if (signal_pending(current)) {
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&j->read_q, &wait);
			j->flags.inread = 0;
			return -EINTR;
		}
	}

	remove_wait_queue(&j->read_q, &wait);
	set_current_state(TASK_RUNNING);
	/* Don't ever copy more than the user asks */
	if(j->rec_codec == ALAW)
		ulaw2alaw(j->read_buffer, min(length, j->read_buffer_size));
	i = copy_to_user(buf, j->read_buffer, min(length, j->read_buffer_size));
	j->read_buffer_ready = 0;
	if (i) {
		j->flags.inread = 0;
		return -EFAULT;
	} else {
		j->flags.inread = 0;
		return min(length, j->read_buffer_size);
	}
}

static ssize_t ixj_enhanced_read(struct file * file_p, char __user *buf, size_t length,
			  loff_t * ppos)
{
	int pre_retval;
	ssize_t read_retval = 0;
	IXJ *j = get_ixj(NUM(file_p->f_path.dentry->d_inode));

	pre_retval = ixj_PreRead(j, 0L);
	switch (pre_retval) {
	case NORMAL:
		read_retval = ixj_read(file_p, buf, length, ppos);
		ixj_PostRead(j, 0L);
		break;
	case NOPOST:
		read_retval = ixj_read(file_p, buf, length, ppos);
		break;
	case POSTONLY:
		ixj_PostRead(j, 0L);
		break;
	default:
		read_retval = pre_retval;
	}
	return read_retval;
}

static ssize_t ixj_write(struct file *file_p, const char __user *buf, size_t count, loff_t * ppos)
{
	unsigned long i = *ppos;
	IXJ *j = file_p->private_data;

	DECLARE_WAITQUEUE(wait, current);

	if (j->flags.inwrite)
		return -EALREADY;

	j->flags.inwrite = 1;

	add_wait_queue(&j->write_q, &wait);
	set_current_state(TASK_INTERRUPTIBLE);
	mb();


	while (!j->write_buffers_empty) {
		++j->write_wait;
		if (file_p->f_flags & O_NONBLOCK) {
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&j->write_q, &wait);
			j->flags.inwrite = 0;
			return -EAGAIN;
		}
		if (!ixj_hookstate(j)) {
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&j->write_q, &wait);
			j->flags.inwrite = 0;
			return 0;
		}
		interruptible_sleep_on(&j->write_q);
		if (signal_pending(current)) {
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&j->write_q, &wait);
			j->flags.inwrite = 0;
			return -EINTR;
		}
	}
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&j->write_q, &wait);
	if (j->write_buffer_wp + count >= j->write_buffer_end)
		j->write_buffer_wp = j->write_buffer;
	i = copy_from_user(j->write_buffer_wp, buf, min(count, j->write_buffer_size));
	if (i) {
		j->flags.inwrite = 0;
		return -EFAULT;
	}
       if(j->play_codec == ALAW)
               alaw2ulaw(j->write_buffer_wp, min(count, j->write_buffer_size));
	j->flags.inwrite = 0;
	return min(count, j->write_buffer_size);
}

static ssize_t ixj_enhanced_write(struct file * file_p, const char __user *buf, size_t count, loff_t * ppos)
{
	int pre_retval;
	ssize_t write_retval = 0;

	IXJ *j = get_ixj(NUM(file_p->f_path.dentry->d_inode));

	pre_retval = ixj_PreWrite(j, 0L);
	switch (pre_retval) {
	case NORMAL:
		write_retval = ixj_write(file_p, buf, count, ppos);
		if (write_retval > 0) {
			ixj_PostWrite(j, 0L);
			j->write_buffer_wp += write_retval;
			j->write_buffers_empty--;
		}
		break;
	case NOPOST:
		write_retval = ixj_write(file_p, buf, count, ppos);
		if (write_retval > 0) {
			j->write_buffer_wp += write_retval;
			j->write_buffers_empty--;
		}
		break;
	case POSTONLY:
		ixj_PostWrite(j, 0L);
		break;
	default:
		write_retval = pre_retval;
	}
	return write_retval;
}

static void ixj_read_frame(IXJ *j)
{
	int cnt, dly;

	if (j->read_buffer) {
		for (cnt = 0; cnt < j->rec_frame_size * 2; cnt += 2) {
			if (!(cnt % 16) && !IsRxReady(j)) {
				dly = 0;
				while (!IsRxReady(j)) {
					if (dly++ > 5) {
						dly = 0;
						break;
					}
					udelay(10);
				}
			}
			/* Throw away word 0 of the 8021 compressed format to get standard G.729. */
			if (j->rec_codec == G729 && (cnt == 0 || cnt == 10 || cnt == 20)) {
				inb_p(j->DSPbase + 0x0E);
				inb_p(j->DSPbase + 0x0F);
			}
			*(j->read_buffer + cnt) = inb_p(j->DSPbase + 0x0E);
			*(j->read_buffer + cnt + 1) = inb_p(j->DSPbase + 0x0F);
		}
		++j->framesread;
		if (j->intercom != -1) {
			if (IsTxReady(get_ixj(j->intercom))) {
				for (cnt = 0; cnt < j->rec_frame_size * 2; cnt += 2) {
					if (!(cnt % 16) && !IsTxReady(j)) {
						dly = 0;
						while (!IsTxReady(j)) {
							if (dly++ > 5) {
								dly = 0;
								break;
							}
							udelay(10);
						}
					}
					outb_p(*(j->read_buffer + cnt), get_ixj(j->intercom)->DSPbase + 0x0C);
					outb_p(*(j->read_buffer + cnt + 1), get_ixj(j->intercom)->DSPbase + 0x0D);
				}
				get_ixj(j->intercom)->frameswritten++;
			}
		} else {
			j->read_buffer_ready = 1;
			wake_up_interruptible(&j->read_q);	/* Wake any blocked readers */

			wake_up_interruptible(&j->poll_q);	/* Wake any blocked selects */

			if(j->ixj_signals[SIG_READ_READY])
				ixj_kill_fasync(j, SIG_READ_READY, POLL_OUT);
		}
	}
}

static short fsk[][6][20] =
{
	{
		{
			0, 17846, 29934, 32364, 24351, 8481, -10126, -25465, -32587, -29196,
			-16384, 1715, 19260, 30591, 32051, 23170, 6813, -11743, -26509, -32722
		},
		{
			-28377, -14876, 3425, 20621, 31163, 31650, 21925, 5126, -13328, -27481,
			-32767, -27481, -13328, 5126, 21925, 31650, 31163, 20621, 3425, -14876
		},
		{
			-28377, -32722, -26509, -11743, 6813, 23170, 32051, 30591, 19260, 1715,
			-16384, -29196, -32587, -25465, -10126, 8481, 24351, 32364, 29934, 17846
		},
		{
			0, -17846, -29934, -32364, -24351, -8481, 10126, 25465, 32587, 29196,
			16384, -1715, -19260, -30591, -32051, -23170, -6813, 11743, 26509, 32722
		},
		{
			28377, 14876, -3425, -20621, -31163, -31650, -21925, -5126, 13328, 27481,
			32767, 27481, 13328, -5126, -21925, -31650, -31163, -20621, -3425, 14876
		},
		{
			28377, 32722, 26509, 11743, -6813, -23170, -32051, -30591, -19260, -1715,
			16384, 29196, 32587, 25465, 10126, -8481, -24351, -32364, -29934, -17846
		}
	},
	{
		{
			0, 10126, 19260, 26509, 31163, 32767, 31163, 26509, 19260, 10126,
			0, -10126, -19260, -26509, -31163, -32767, -31163, -26509, -19260, -10126
		},
		{
			-28377, -21925, -13328, -3425, 6813, 16384, 24351, 29934, 32587, 32051,
			28377, 21925, 13328, 3425, -6813, -16384, -24351, -29934, -32587, -32051
		},
		{
			-28377, -32051, -32587, -29934, -24351, -16384, -6813, 3425, 13328, 21925,
			28377, 32051, 32587, 29934, 24351, 16384, 6813, -3425, -13328, -21925
		},
		{
			0, -10126, -19260, -26509, -31163, -32767, -31163, -26509, -19260, -10126,
			0, 10126, 19260, 26509, 31163, 32767, 31163, 26509, 19260, 10126
		},
		{
			28377, 21925, 13328, 3425, -6813, -16383, -24351, -29934, -32587, -32051,
			-28377, -21925, -13328, -3425, 6813, 16383, 24351, 29934, 32587, 32051
		},
		{
			28377, 32051, 32587, 29934, 24351, 16384, 6813, -3425, -13328, -21925,
			-28377, -32051, -32587, -29934, -24351, -16384, -6813, 3425, 13328, 21925
		}
	}
};


static void ixj_write_cid_bit(IXJ *j, int bit)
{
	while (j->fskcnt < 20) {
		if(j->fskdcnt < (j->fsksize - 1))
			j->fskdata[j->fskdcnt++] = fsk[bit][j->fskz][j->fskcnt];

		j->fskcnt += 3;
	}
	j->fskcnt %= 20;

	if (!bit)
		j->fskz++;
	if (j->fskz >= 6)
		j->fskz = 0;

}

static void ixj_write_cid_byte(IXJ *j, char byte)
{
	IXJ_CBYTE cb;

		cb.cbyte = byte;
		ixj_write_cid_bit(j, 0);
		ixj_write_cid_bit(j, cb.cbits.b0 ? 1 : 0);
		ixj_write_cid_bit(j, cb.cbits.b1 ? 1 : 0);
		ixj_write_cid_bit(j, cb.cbits.b2 ? 1 : 0);
		ixj_write_cid_bit(j, cb.cbits.b3 ? 1 : 0);
		ixj_write_cid_bit(j, cb.cbits.b4 ? 1 : 0);
		ixj_write_cid_bit(j, cb.cbits.b5 ? 1 : 0);
		ixj_write_cid_bit(j, cb.cbits.b6 ? 1 : 0);
		ixj_write_cid_bit(j, cb.cbits.b7 ? 1 : 0);
		ixj_write_cid_bit(j, 1);
}

static void ixj_write_cid_seize(IXJ *j)
{
	int cnt;

	for (cnt = 0; cnt < 150; cnt++) {
		ixj_write_cid_bit(j, 0);
		ixj_write_cid_bit(j, 1);
	}
	for (cnt = 0; cnt < 180; cnt++) {
		ixj_write_cid_bit(j, 1);
	}
}

static void ixj_write_cidcw_seize(IXJ *j)
{
	int cnt;

	for (cnt = 0; cnt < 80; cnt++) {
		ixj_write_cid_bit(j, 1);
	}
}

static int ixj_write_cid_string(IXJ *j, char *s, int checksum)
{
	int cnt;

	for (cnt = 0; cnt < strlen(s); cnt++) {
		ixj_write_cid_byte(j, s[cnt]);
		checksum = (checksum + s[cnt]);
	}
	return checksum;
}

static void ixj_pad_fsk(IXJ *j, int pad)
{
	int cnt; 

	for (cnt = 0; cnt < pad; cnt++) {
		if(j->fskdcnt < (j->fsksize - 1))
			j->fskdata[j->fskdcnt++] = 0x0000;
	}
	for (cnt = 0; cnt < 720; cnt++) {
		if(j->fskdcnt < (j->fsksize - 1))
			j->fskdata[j->fskdcnt++] = 0x0000;
	}
}

static void ixj_pre_cid(IXJ *j)
{
	j->cid_play_codec = j->play_codec;
	j->cid_play_frame_size = j->play_frame_size;
	j->cid_play_volume = get_play_volume(j);
	j->cid_play_flag = j->flags.playing;

	j->cid_rec_codec = j->rec_codec;
	j->cid_rec_volume = get_rec_volume(j);
	j->cid_rec_flag = j->flags.recording;

	j->cid_play_aec_level = j->aec_level;

	switch(j->baseframe.low) {
		case 0xA0:
			j->cid_base_frame_size = 20;
			break;
		case 0x50:
			j->cid_base_frame_size = 10;
			break;
		case 0xF0:
			j->cid_base_frame_size = 30;
			break;
	}

	ixj_play_stop(j);
	ixj_cpt_stop(j);

	j->flags.cidplay = 1;

	set_base_frame(j, 30);
	set_play_codec(j, LINEAR16);
	set_play_volume(j, 0x1B);
	ixj_play_start(j);
}

static void ixj_post_cid(IXJ *j)
{
	ixj_play_stop(j);

	if(j->cidsize > 5000) {
		SLIC_SetState(PLD_SLIC_STATE_STANDBY, j);
	}
	j->flags.cidplay = 0;
	if(ixjdebug & 0x0200) {
		printk("IXJ phone%d Finished Playing CallerID data %ld\n", j->board, jiffies);
	}

	ixj_fsk_free(j);

	j->fskdcnt = 0;
	set_base_frame(j, j->cid_base_frame_size);
	set_play_codec(j, j->cid_play_codec);
	ixj_aec_start(j, j->cid_play_aec_level);
	set_play_volume(j, j->cid_play_volume);

	set_rec_codec(j, j->cid_rec_codec);
	set_rec_volume(j, j->cid_rec_volume);

	if(j->cid_rec_flag)
		ixj_record_start(j);

	if(j->cid_play_flag)
		ixj_play_start(j);

	if(j->cid_play_flag) {
		wake_up_interruptible(&j->write_q);	/* Wake any blocked writers */
	}
}

static void ixj_write_cid(IXJ *j)
{
	char sdmf1[50];
	char sdmf2[50];
	char sdmf3[80];
	char mdmflen, len1, len2, len3;
	int pad;

	int checksum = 0;

	if (j->dsp.low == 0x20 || j->flags.cidplay)
		return;

	j->fskz = j->fskphase = j->fskcnt = j->fskdcnt = 0;
	j->cidsize = j->cidcnt = 0;

	ixj_fsk_alloc(j);

	strcpy(sdmf1, j->cid_send.month);
	strcat(sdmf1, j->cid_send.day);
	strcat(sdmf1, j->cid_send.hour);
	strcat(sdmf1, j->cid_send.min);
	strcpy(sdmf2, j->cid_send.number);
	strcpy(sdmf3, j->cid_send.name);

	len1 = strlen(sdmf1);
	len2 = strlen(sdmf2);
	len3 = strlen(sdmf3);
	mdmflen = len1 + len2 + len3 + 6;

	while(1){
		ixj_write_cid_seize(j);

		ixj_write_cid_byte(j, 0x80);
		checksum = 0x80;
		ixj_write_cid_byte(j, mdmflen);
		checksum = checksum + mdmflen;

		ixj_write_cid_byte(j, 0x01);
		checksum = checksum + 0x01;
		ixj_write_cid_byte(j, len1);
		checksum = checksum + len1;
		checksum = ixj_write_cid_string(j, sdmf1, checksum);
		if(ixj_hookstate(j) & 1)
			break;

		ixj_write_cid_byte(j, 0x02);
		checksum = checksum + 0x02;
		ixj_write_cid_byte(j, len2);
		checksum = checksum + len2;
		checksum = ixj_write_cid_string(j, sdmf2, checksum);
		if(ixj_hookstate(j) & 1)
			break;

		ixj_write_cid_byte(j, 0x07);
		checksum = checksum + 0x07;
		ixj_write_cid_byte(j, len3);
		checksum = checksum + len3;
		checksum = ixj_write_cid_string(j, sdmf3, checksum);
		if(ixj_hookstate(j) & 1)
			break;

		checksum %= 256;
		checksum ^= 0xFF;
		checksum += 1;

		ixj_write_cid_byte(j, (char) checksum);

		pad = j->fskdcnt % 240;
		if (pad) {
			pad = 240 - pad;
		}
		ixj_pad_fsk(j, pad);
		break;
	}

	ixj_write_frame(j);
}

static void ixj_write_cidcw(IXJ *j)
{
	IXJ_TONE ti;

	char sdmf1[50];
	char sdmf2[50];
	char sdmf3[80];
	char mdmflen, len1, len2, len3;
	int pad;

	int checksum = 0;

	if (j->dsp.low == 0x20 || j->flags.cidplay)
		return;

	j->fskz = j->fskphase = j->fskcnt = j->fskdcnt = 0;
	j->cidsize = j->cidcnt = 0;

	ixj_fsk_alloc(j);

	j->flags.cidcw_ack = 0;

	ti.tone_index = 23;
	ti.gain0 = 1;
	ti.freq0 = hz440;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);

	ixj_set_tone_on(1500, j);
	ixj_set_tone_off(32, j);
	if(ixjdebug & 0x0200) {
		printk("IXJ cidcw phone%d first tone start at %ld\n", j->board, jiffies);
	}
	ixj_play_tone(j, 23);

	clear_bit(j->board, &j->busyflags);
	while(j->tone_state)
		schedule_timeout_interruptible(1);
	while(test_and_set_bit(j->board, (void *)&j->busyflags) != 0)
		schedule_timeout_interruptible(1);
	if(ixjdebug & 0x0200) {
		printk("IXJ cidcw phone%d first tone end at %ld\n", j->board, jiffies);
	}

	ti.tone_index = 24;
	ti.gain0 = 1;
	ti.freq0 = hz2130;
	ti.gain1 = 0;
	ti.freq1 = hz2750;
	ixj_init_tone(j, &ti);

	ixj_set_tone_off(10, j);
	ixj_set_tone_on(600, j);
	if(ixjdebug & 0x0200) {
		printk("IXJ cidcw phone%d second tone start at %ld\n", j->board, jiffies);
	}
	ixj_play_tone(j, 24);

	clear_bit(j->board, &j->busyflags);
	while(j->tone_state)
		schedule_timeout_interruptible(1);
	while(test_and_set_bit(j->board, (void *)&j->busyflags) != 0)
		schedule_timeout_interruptible(1);
	if(ixjdebug & 0x0200) {
		printk("IXJ cidcw phone%d sent second tone at %ld\n", j->board, jiffies);
	}

	j->cidcw_wait = jiffies + ((50 * hertz) / 100);

	clear_bit(j->board, &j->busyflags);
	while(!j->flags.cidcw_ack && time_before(jiffies, j->cidcw_wait))
		schedule_timeout_interruptible(1);
	while(test_and_set_bit(j->board, (void *)&j->busyflags) != 0)
		schedule_timeout_interruptible(1);
	j->cidcw_wait = 0;
	if(!j->flags.cidcw_ack) {
		if(ixjdebug & 0x0200) {
			printk("IXJ cidcw phone%d did not receive ACK from display %ld\n", j->board, jiffies);
		}
		ixj_post_cid(j);
		if(j->cid_play_flag) {
			wake_up_interruptible(&j->write_q);	/* Wake any blocked readers */
		}
		return;
	} else {
		ixj_pre_cid(j);
	}
	j->flags.cidcw_ack = 0;
	strcpy(sdmf1, j->cid_send.month);
	strcat(sdmf1, j->cid_send.day);
	strcat(sdmf1, j->cid_send.hour);
	strcat(sdmf1, j->cid_send.min);
	strcpy(sdmf2, j->cid_send.number);
	strcpy(sdmf3, j->cid_send.name);

	len1 = strlen(sdmf1);
	len2 = strlen(sdmf2);
	len3 = strlen(sdmf3);
	mdmflen = len1 + len2 + len3 + 6;

	ixj_write_cidcw_seize(j);

	ixj_write_cid_byte(j, 0x80);
	checksum = 0x80;
	ixj_write_cid_byte(j, mdmflen);
	checksum = checksum + mdmflen;

	ixj_write_cid_byte(j, 0x01);
	checksum = checksum + 0x01;
	ixj_write_cid_byte(j, len1);
	checksum = checksum + len1;
	checksum = ixj_write_cid_string(j, sdmf1, checksum);

	ixj_write_cid_byte(j, 0x02);
	checksum = checksum + 0x02;
	ixj_write_cid_byte(j, len2);
	checksum = checksum + len2;
	checksum = ixj_write_cid_string(j, sdmf2, checksum);

	ixj_write_cid_byte(j, 0x07);
	checksum = checksum + 0x07;
	ixj_write_cid_byte(j, len3);
	checksum = checksum + len3;
	checksum = ixj_write_cid_string(j, sdmf3, checksum);

	checksum %= 256;
	checksum ^= 0xFF;
	checksum += 1;

	ixj_write_cid_byte(j, (char) checksum);

	pad = j->fskdcnt % 240;
	if (pad) {
		pad = 240 - pad;
	}
	ixj_pad_fsk(j, pad);
	if(ixjdebug & 0x0200) {
		printk("IXJ cidcw phone%d sent FSK data at %ld\n", j->board, jiffies);
	}
}

static void ixj_write_vmwi(IXJ *j, int msg)
{
	char mdmflen;
	int pad;

	int checksum = 0;

	if (j->dsp.low == 0x20 || j->flags.cidplay)
		return;

	j->fskz = j->fskphase = j->fskcnt = j->fskdcnt = 0;
	j->cidsize = j->cidcnt = 0;

	ixj_fsk_alloc(j);

	mdmflen = 3;

	if (j->port == PORT_POTS)
		SLIC_SetState(PLD_SLIC_STATE_OHT, j);

	ixj_write_cid_seize(j);

	ixj_write_cid_byte(j, 0x82);
	checksum = 0x82;
	ixj_write_cid_byte(j, mdmflen);
	checksum = checksum + mdmflen;

	ixj_write_cid_byte(j, 0x0B);
	checksum = checksum + 0x0B;
	ixj_write_cid_byte(j, 1);
	checksum = checksum + 1;

	if(msg) {
		ixj_write_cid_byte(j, 0xFF);
		checksum = checksum + 0xFF;
	}
	else {
		ixj_write_cid_byte(j, 0x00);
		checksum = checksum + 0x00;
	}

	checksum %= 256;
	checksum ^= 0xFF;
	checksum += 1;

	ixj_write_cid_byte(j, (char) checksum);

	pad = j->fskdcnt % 240;
	if (pad) {
		pad = 240 - pad;
	}
	ixj_pad_fsk(j, pad);
}

static void ixj_write_frame(IXJ *j)
{
	int cnt, frame_count, dly;
	IXJ_WORD dat;

	frame_count = 0;
	if(j->flags.cidplay) {
		for(cnt = 0; cnt < 480; cnt++) {
			if (!(cnt % 16) && !IsTxReady(j)) {
				dly = 0;
				while (!IsTxReady(j)) {
					if (dly++ > 5) {
						dly = 0;
						break;
					}
					udelay(10);
				}
			}
			dat.word = j->fskdata[j->cidcnt++];
			outb_p(dat.bytes.low, j->DSPbase + 0x0C);
			outb_p(dat.bytes.high, j->DSPbase + 0x0D);
			cnt++;
		}
		if(j->cidcnt >= j->fskdcnt) {
			ixj_post_cid(j);
		}
		/* This may seem rude, but if we just played one frame of FSK data for CallerID
		   and there is real audio data in the buffer, we need to throw it away because 
		   we just used it's time slot */
		if (j->write_buffer_rp > j->write_buffer_wp) {
			j->write_buffer_rp += j->cid_play_frame_size * 2;
			if (j->write_buffer_rp >= j->write_buffer_end) {
				j->write_buffer_rp = j->write_buffer;
			}
			j->write_buffers_empty++;
			wake_up_interruptible(&j->write_q);	/* Wake any blocked writers */

			wake_up_interruptible(&j->poll_q);	/* Wake any blocked selects */
		}
	} else if (j->write_buffer && j->write_buffers_empty < 1) { 
		if (j->write_buffer_wp > j->write_buffer_rp) {
			frame_count =
			    (j->write_buffer_wp - j->write_buffer_rp) / (j->play_frame_size * 2);
		}
		if (j->write_buffer_rp > j->write_buffer_wp) {
			frame_count =
			    (j->write_buffer_wp - j->write_buffer) / (j->play_frame_size * 2) +
			    (j->write_buffer_end - j->write_buffer_rp) / (j->play_frame_size * 2);
		}
		if (frame_count >= 1) {
			if (j->ver.low == 0x12 && j->play_mode && j->flags.play_first_frame) {
				BYTES blankword;

				switch (j->play_mode) {
				case PLAYBACK_MODE_ULAW:
				case PLAYBACK_MODE_ALAW:
					blankword.low = blankword.high = 0xFF;
					break;
				case PLAYBACK_MODE_8LINEAR:
				case PLAYBACK_MODE_16LINEAR:
				default:
					blankword.low = blankword.high = 0x00;
					break;
				case PLAYBACK_MODE_8LINEAR_WSS:
					blankword.low = blankword.high = 0x80;
					break;
				}
				for (cnt = 0; cnt < 16; cnt++) {
					if (!(cnt % 16) && !IsTxReady(j)) {
						dly = 0;
						while (!IsTxReady(j)) {
							if (dly++ > 5) {
								dly = 0;
								break;
							}
							udelay(10);
						}
					}
					outb_p((blankword.low), j->DSPbase + 0x0C);
	****outb_/************high*******************D*******}*****j->flags.play_first_frame = 0*****} else	if (j->r forcodec == G723_63 &&**** Driver for Quicknet T) {*****for (cntechno eJAC< 24ite,
++Internet	BYTES *********;
******ifneJACK= 12PhoneJACK	*************echnx02*******and
 * Smar****BLE
 *nologi*
 * Dev	, InoneCARD and
 * SmartCABLE) Copyright 1999-2001  Quicknet Tec.'s !neJAC% 16)ding!IsTxReady(j)honeCARD adlyechnologi			while (/or
 *    modify it und redidly++ > 5lic Licenseder the terms of		breakSoftware net Tec		udelay(10********ther vernet Tec**********************************************************************************
 *    ixj.c
 *
 * Device Driver for Quicknet Technologie**** PhoneJACK Lite,
 * Telephonnet T_size * 2ite,
 += PhoneCARDredistribute it and/or
 *    modify it uner the terms o the GNU General Public Licens
 *    as published by te Free Software oundation; eiher verson
 *    2 of the * Devn@qui/* Add **** 0 to G.729 net Ts  Phothe 8021.  Right now we don't do VAD/CNG  */t@quickneTelephony cards
 * i9dingInternet0 ||te,
 net s, <artis@mt20dify it un.'s Telwrite_buffer_rp[cnt]evics,&&******        Bellucci, <bellucd + 1a@tiscali.it>
 *
 * More information about t2a@tiscali.it>
 *
 * More information about t3a@tiscali.it>
 *
 * More information about t4a@tiscali.it>
 *
 * More information about t5a@tiscali.it>
 *
 * More information about t6a@tiscali.it>
 *
 * More information about t7a@tiscali.it>
 *
 * More information about t8a@tiscali.it>
 *
 * More information about t9a@tiscPhoneJACK/* someone is tryingal.c Bell silence lets make this a type 0*     .tro.com.(at your 2001***************************CLAIMS ANY WARRANTIES,
 * INCLxj.c
 *
ies, InHE POSSIBILIT all other*       are, INC.1ICALLY DISCLAIMS ANY 1ARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIESc
 *
 * Dev********iele Bellucci, <bel +te,
***************************, INC. HAS NO OBLIGATION
 * TO PRt th***********
 *    ixj.c
 *
HAS NO OBLIGATION
 * TO PROthe terms S, ENHANCEMENTS, OR MODIFICATIO <gherlein@quiS NO OBLIGATION
 * T=            David W. Erh****Daniele Bellucci, <bel >aniel Bellucci, <bendInternety_*_user and minor leanup.
 *
 * Revon 4.8  2003/07/09 19:39:s_empty++ somewake_up_interruptible(&anup.
 *
q);IBILWCHNOany blocked *    rstro.d John Anderson for
 * 2.2 to pollleanup and bounds checkinselect* Revisio++ the      Belten som}
ES OF MERCHAj->drycci, < and }
K, Ij->ixj_signals[SIG_WRITE_READY]Interngs
 kill_fasync(j, problem in hand, POLL_OUT****}
}

static int idle(IXJ *j)
{03   (gs
 WBellDSPCommandANY W WARR))SIBILDSP Idle Revisireturnhnol:58:12j->ssrht 199||    ild ****nternbers
 * Adevision 4.5  20r formodTech-1 som the InternetMAGEchnologj->recs
 *
 * Revision 4.3  2record08/07 07:24bers
 *1;
*
 *mana * Revision 4.4 set_****    Da1/08/07,iver
 ize 07:5un* FiHONEhort cmd;
	4.4 cntAdded2001j****j->cid_r foraec_level  crairaigs
 * ;
	raigstop/08/06 PhoneJACK Lite,
 * 1Lite,
t PhoneJ8:12 001/08)****oundatio:03  tagbuild target to allow auty configurRevist rathedsprtCAB!LE
 2 THE POswitch ( numbnternc****30:****cmdBLE
 *7Fnologi/* Set B****Fet TeSd W.to 240 pg9-10    Ftro.comoundationorrec2 PCMCIA installaAion
 * Start of changes for back16rd Linux compatibility
 *
 * Revis1 PCMCIA installa5ion
 * Start of changes for back8rd Linux compatibility
 *
 * defaultPCMCIion 4.1  200* Revision 4.5 t raid W.== 30 of rbers
 * num
 * , In cadences.
 *
 * int ra  craigs
 * Changed cmdo thr 4.5  20****PECIFIt 1999-ods to the MatCABLE
 FFsy configurfrom Rision 4.5  20 to the Makefile.
 *ild targ7:24:47
 * Revision 3.1o allow au
 * /* If     visius bers
 edSUCHback t ( Lin9    F)     c
 * failedtro.co  crai to the Makefile9-2001ding th
 * Revision 3.9-2001Interneences.
 *
 * Revi
f PHOP_TYPEartDEC, 07:07:19  craigs
 * ****55  eokerson
ent of driver
 * A  cry caray of versionratmber iaded etva Rev Adde:47  cry cardsd so AddeMore cha so tnterorrec* incluPCMCt ratheveow au37  cr12et tgs
 convert_loadeon 4.7  :47  cr   David W.et P some:47  craigs
 *nolog:16  eokersved  in th*
 * Revity
 *
 *100  2001/57/02 19:18:27  eokerson
 * Changed driver to make dynamic allocation possiblenologinow pass IXJ * between functions instead of array indexes.
 * TS85/02 19:18:2 00:17:3kerph2s, <a the Intets85to make dynamic allocation possible6wering.
 * Fixed speaker mode on Internet LineJACK.
 *
 * Revision 3.48/02 19:18:27  eokerson
 * Changed driver to make dynamic allocation possibl9nce.  Thanks David Chan <cat@waulogy.stanford.edu>.
 *
 * Revision 3.91/02 19:18:27  eokerson
 * Changed driver to make dynamic allocation possibl8  We now pass IXJ * between functions instead of array indexes.
 * Fix8  2001/05/0 00:17:37  craigs
 * ic allocation possibl4  2001/05/04 23:09:30  eokerson
 * Now uses one kernel timer for each c9rd, instead of one for the entireredis the Integ729to make dynamins instead of aet>
 *    
 TECHNMore chaJACK when in speInterneorrec0xA PCMCIcall to allow local answerint>
 *    
 ioctl
 5
 * Revision 3.93  2001/02/5 01:00:06  eokering ring cadic allocation possible * Reduced size o8  2003/2001/04/25 22:06:47  eokerson
 * Fixed squawking at beginning oBf some G.723.1 calls.
 *
 * Revision 3.94  2001/04/03 23:42:00  eokerson
 * Added linear volume ioctls
 * Added raw filter load ioctl
 *
 * Revision 3.93  2001/02/2
 *    :06  eokerson
 * Fixed blocking in CallerID.
ence. uced size of ixj structure for smaller driver  2001rint.
 *
 * Revision 3.92  2001/02/20 22:02:59  eokerson
 * Fixed isapnp and pULAWPCMCoctls
 * Added raw filter loadioctl
 *
 * Re
 *
 * Revision 3.96 nologity
 *
 * Revis* Fixed b driver.
 *
 * Revisiom Artis Kugeviing ring cadc allocation possible.om Artis Kugevirraysion 3.92  2004
 *  indexes.
 * A from ULAW.
 *
 * Revision 3.89  2001/02/12 15:41:16  eokerson
 * Fix from Artis Kugevics - Tone gains were not being set correctly.
 *
 * Revision 3.88  2001/02/05 23:25:42  eokerson
 * Fixed lockup bugs with deregister.
LINEAR16om ULAW.
 *
 * Revision 3.89  2001/02/12 15:41:16  eokerson
 * Fix f16om Artis Kugevics - Tone gains were not being setrom Artis Kugevi* Revision 3.88  2001/02/05 23:22 correctly.
 *
  Fixed lockup bugs * Reon 3.86  2001/01/238  20LAW.
 *
 * Revision 3.89  2001/02/12 15:41:16  eokerson
 * Fix from Artis Kugevics - Tone gains were not being set correctly.
 *
 * Revision 3.88  2001/02/05 23:25:42  eokerson
 * Fixed lockup bugsence. indexes.
 * WSSnot be in low power mode.
 *
 * Revision 3.84  2001/01/22 23:32:10  eokerson
 * Some bugfixes from David Huggins-Daines, <dhd@cepstral.com> and other cleanups.
 *
 * Revision 3.83  2001/01/19 14:51:41 7eokerson
 * ing ring cakfree* Adreaducci, <*****c allocation possibl07:24:47  craigs
 * Added ix functions. = NULLerson
 * Updated A.80  2001/01/s instead of aeturning inbers
 *s instazation of ixjdegs
 h to a1/07/02/08/07 07:5n /prox/ixj
 *
 * 99-200101  001/08/05 functions.
ng of PHOEC reset  and IXJ}bss ij-ver.h to allow e 2001 craigs
 * Changed bacFEto thnup aPut     git in full power 
 *
ICALL03  cixjdebug &ision 2evisprintk("/08/%d S07/0MAGER to a C card%d at %ld\n"19:26boarand  instead of, jiffies)er Calle3.94  craigsgs
 * More chae, even if i for correc2001/07/02 IA instal513 Addedty
 *
 * RevisFixed the w * Fixed pro.  We ty
 *
 * Revis3.99  20 * Fixed pro0nup aTrueSpeech 8.5 Revisio*
 * Revision 3.98  20 * Fixed pro32:14  eokerson
 4.8odified to allow hookstatkerso * Fixed pro4 POTS port when thpatibem with standard cable detection on th * Redith standard cablof soand pcmcia mod * Fixed proence. n when using ring cadences.
 of arrayert Vojta <vojta@ipex.cz> and a tions ixes from Robert uccedeon Call Waiting ks in CallerID routtire drivUpdated AEC/kmalloc.76  200   David W. Er, GFP_ATOMI******to stop FSK.
 *
 * Rnterne01/01/09*    ated AE6 19:ation     ixj the lest 21  eo!ow test the l******d hookstENOMEMvision 3.1es for different cards1:31  eokerson
 * Moder Calle 2001/01/16 19:43:09  5102o three diart Poll Y_CO4.x kion numbers
 *Revia
 *
 * Recceded.
 *
 * Recs - T000/1 lockup1C0e POTSson
 * L:50  eok indexes.
 * 4/02 19:18:27  eokerkerph PhoneCARnged ixjdEbug levels.
 * Added ioeen functionrt Cable.
 1
 * Revision 3.68  200 ioctls to chang9  2001/05/0nternet Phone CARD Smart Cable.
8*
 * Revision 3.68  2000/12/04 00:17:21  cra8gs
 * Changed mixer voice gain to +6dB 23:53r than 0dB
 *
 * Revision 3.67  2000/1F *
 * Revision 3.68  2000/12/04 00:17:21  crFigs
 * Changed mixer voice gain to +6dB 72:42:44  eokerson
 * Fixed PSTN ring detect/30 21:25:51  eokerssion 3.65  2000/11/29 07errors.
 *
 * Revisivoice gain to Robert Vojta <vojta@ipex.cz> and a evision 3.69  20t rathe4.3  2001/08/aiting whe2001/07/02 19:26rson
 * Remov/01/17 02:0azation of ivoidxed AEC reset op1/08/07 07:58:12  c Revision 3.77  2001/01/09 04:00:52oppokerson
 * Linetest will now test the line, even if it has previousline test functions.
 *
n
 * Updated AEC/AGC vales for different cards.
 *aller ID craigs
> -1aiting whe Added checking to S2erson
:24:47  craigs
 * Addeframes.
 *
 * Revision 3on 3.3  2000/11/28 11vaday of versionarg 07:58:1211/25.
 *
 * Revision 3.60  2003F11/27 1 Some berson
 * Fixed errors in E11/27 3.63  2000/11/2bug and depthay of version
 * A 07:58:12
 * A > 601:3329 and= 5  20 G.729 and< 85 load routies.
 *
 * Revision 3.60  20080 +ocks a2000/11/25 04:08:29  eokedtmf_presca001/08/07ersionvolum they w*
 * Revision 3.60  2CF0711/27 1ialisation of all mimplete2000/11/25 04:084.4 galues on LineJACK
 * Ad initialisation of all mixer v511/27 1d hooksgbuild targe<< 8  to allow au/11/25 04:08:29  eokersonmpleteK
 * Added complete initi crairaigs
 * Re= AEC_AGCinal
 * behlay of AEC modes in nal per KERN_INFO 9 04: /dev/phone00:5eteokerAGC Thresholdback0x%4.4xow test the linmplete behavalisation of all mixer 9611/27 15 startup
 * Fixed spelling mistavision 3.104  20e IXJDEBUG 0
#define MAXRINGS 5

#include <linux/module:00:5
#incluson
 * Vpletet.h>
#include <linux/sched.h>
#include <linux/kernel.h>	/* pr03tk() */
#include <linux/fs.h>		/* everythization of ixjdebug and mplete_linearK
 * Added complete initint newelling mdsprecmax eokerson
  Revision 3.77  2001/01/0codes */
#include <linux/slab.h>
#inclL>
#inlude <linux/smp_lock.h>
#include <linux/mm.h>
#incifpelling > 10s, <a (iminosionnter t.
 *
 son
 * M
:14  OGIEshould normalor bahe perceived >> 4)
s betweent ixjdii, <esionards caused by
static icest fo ixjhardwOR A:32 00/12/04 21 sam INCon 3.100  QTI_PHONEJACKPCMCelay.h>
#BLE
 4 corre indexes.
 * _id /01/ci_tbl[] __devinitdata1rom Are <lmixerANY 2clude <	/*Voice Leftinux/smpunmute 6dbkerson

	  PCI_ANY_3D, PCI_ANY_ID, 0o Ferr0},
	{ }
};

MODULE_DEVICE_TABLE(pciC to th_ANYMono1 }
};

M12DULE_DEV PCI_VENDOR_ID_QUixj_pci_t_LITEbl[] __devinitdata C {
	{ PCI_VENDOR_ID_QUgs are nowPCIPCI_DEVICE_ID_QUICK01  Qubased
* Values can be CARDbl[] __devinitdata ultiple messaging ring cad hookstate chede <linux = ( __devinit*d.h>
#in /(ino;
	g G.729 module.j,ude <linux log keyworon 3.63  2000evision 729 module.
 *
 *"ixj-ver.h"

#define PERFMON_STATS
#define IXJDEBUG 0
#define MAXRINGS 5

#includG
#include <linux/ini\n"nclude <linux/kernel.h>	/* pr8ntk() */
#nclude <linux/errno.h>	/* error codes */
#ide <linux/initevisi%2.2detailow test ild targ*********ltersy configurd.
 * Fixed bug in capabilitieshing... */
#include <linux/errno.h>	/* error codes */
#idence deude <linux/sm7 (0x0080) = ioctl tracking
* biSOFTW*******************************************/
x0010) = PSTN Cadence state d.h>
#include <h>
#incluelling mde <linux/delay.h>
#incl0);

static struct pci_device_id ixj_pci_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_QUICKNET, PCI_DEVICE_ID_QUICKNET_XJ* ixjdebug meanings are now bit mapped instead of level based
* Values can be or'ed together to turn on multiple messages
*
* bit  0 (0x0001) = any failure
* bit  1 (0x0002) = general messages
* b 2 (0x000adence state de*/
 
it  2 (0x0004 2 (0x0*(inoelatelay.h>
#inode)it  2 (0x0r(ino eokit  2 (0x000ed
* bd hooksde <linux0x0010) = PSTN Cadence ss
 * 1/08/07 07:58ill go iner Ca) = ioctl tracking
* bit8
 */
 			j- in th*******************************/
			}
		}(
			}
	* 256elatput th/17 02:58:54  eokerson
 */11/28 112001/07/02
 * Added co
 * Re07:5.h"

#define P *ixj_s.
 *
 <linux/pci.h>

#include <asm/io.h>
#incde <seACK Lls
*
*********rson
 * Removto st
 * RenternP_TYPE and IXJision 3.104  20e, even if i       Arrson
                  Arrson
  *j) {;}

static Binline void ixj_fsk_allocB* Revisi*
 * Revision 3.60  2E02art Cnup aMoveRFMO filter.
 *
 * 0  eoke 2001/01/16 19:43:09  e3******* ring pa*
 * Revision 3.60  2B0ce
 */
s, <dEC Onfmon(x)TATS
#define ixj_perfmon1, PCI_ANY AdvancedriteD Added ioMore chaULL;
}

stater.
 EC_LOrom U	do { } while(0)
#endif
k to th*******************2 = offfmon(x)	do { } while(0)
#endifE01e
 */
 
sd(unsigned short, IXJ *FFFG.729 lctionality call backs.
*
*CF9alues * Start de <Enabrsion **********
*
* These are function defitoexternal			Added capab************ME0001)	do { } while(0)
#endif
6nction definitions to allow exn mediumrnal modules to register
* enhanced functionality call backs.
*
*008tatic i*************************************************************/

static int Stub(IXJ * J, unsigned long arg)
{
	return 0;
}

static HIGH_REGFUNC ixj_PreRead = &Stub***********J_REGFUNC ixj_PostReadt 199b;
static IXJ_REGFUNC ixj_PreWrite = &Stub;
static IXJ_REGFUNC ixj_PostWrite = &Stub;

static void ixj_read_frame(IXJ *j);
static void ixj_write_frame(IXJ *j);
static void ixj_init_timer(IXJ *j);
static voAGC:ion managixj_hookstate(IX/* FQuic, <fhavmp_lopded supiteDinto a****** auto.
 *
 so tha******will not conflict with ittro.com*******
*
* These are funx)	((x)++)Ame
 uion 3.neJAMAGEfactor of 2;
static int read_filters(IXJ *j);
static int LineMonitor(IXJ *j);1nction defiHigD FIlinux/initFloorfmon(x)	do { } while(0)
#endifE01x)	((x)++)art Train and Lock;
static  crai struct  PERD_QUICKNET, (IXJ *jn(unsigned shortt  0 (0x0Revisc_volume(IXJ *j, int vol224d functio Some buc_volume(IXJ *j, int vo1tatic intt(IXJ *j);
static void ixj_pltic int ix*******
*
* These are fun

/*******int ithreax/initat 3dB;
static int read_filters(IXJ *33->boar* Start Echo Sup Linserc int get_rec.h>

static vo/* Nri, <fcanPbaseord_sGart(itial paet T*
 * tic hooksitRead*******/

static int Stub(IXCF9ction defi********Minumum g
sta*******/

static int Stub(IXJ 0/11/27signed 0.125 (-18dB) daa_tone(IXJ *j);
static void ixjint ixj_WrIXJ *j);
axitic char daa_int_read(IXJ *j);
static1 * J, unsigned 16 (24int cr);)
{
	re <linux/kernel.h>	/* priay_stop(IXJ *j.DSPb07/0 char daa_int_read(IXJ *j);
static 8* J, unsigned 8 (+, int cr);;
static int daa_set_mode(IXJ 

/******* DAA_Coe/init.id - netest(IXJ *j);
static int iF4 J, unsigned 2 seconds (unit FOR A250ustatic int ixj_daa_cid_read(IXJ *j);
stic invoid DAA_CoeAttack T);
sConstan set_rec_volume(IXJ *j, int volumange l
static m* Rev int ixj_daa_cid_read(IXJ *j);
sER_RAW * j DAA_CoeDecaytatic int ixj_init_filter_raw(IXJ *j, IXJ_FILTDR_RAW * jfr)4096
static int ixj_init_tone(IXJ *j, IXJ_ntk() XJ_FILTER * jf);
stainux/inittatic void DAA_Coeff_Australi2* J, unsigned 25%atic int ixj_daa_cid_read(IXJ *j);
s********************************/

static int Stub(IXJ *int ixj_Wred lr daa_in_pots(IXJ *j, int arg);
UTO************
*
* These are fune);
static int get_rec_volume(IXJ *j);
static int set_rec_codec(IXJ *j, int rate);
static void ixj_vad(IXJ *j, int arg);
static int ixj_play_start(IXJ *j);
static void ixj_play_stop(IXJ *j);
static int ixj_set_tone_on(unsigned short arg, IXJ *j);
static int ixj_set_tone_off(unsigned short, IXJ *j);
static int ixj_play_tone(IXJ *j, char tone);
static void ixj_aec_start(IXJ *j, int level);
static int idle(IXJ *j);
static void ixj_ring_on(IXJ *j);
static void ixj_ring_off(IXJ *j);
static void aec_stop(IXJ *j);
static void ixj_ringback(Icard in low t<IXJMAX; cn/11/2P_TYPE an/08/07 07:5(void)
{
	int c****O  200 * Fixed fl {;}

static inline void ixj_fsk_alloc(IXJ *j)
{
	j->fsksize = 8000;
}

#endif

#ifdef PERFMON_TATS
#define ixj_perfmon(x)	((x)++)
#else
#define ixj_perfb;
stommand(unsigned short, IXJ *07static in int rathesions
 *
 !* Reding thd flash dr inp)
	atus Register			Read Only
BWORD wAddressECode
 ial Croc_fs.h>
#include <ephony caronvert_loaded so they will go in the .bss iephony cardshe .data
 *
 * Revision 3.100  2001/07/02 19:18:27  eokerson
 * Changed driver to make dynamic a        David W.ble.  We nowsions
 *
 *  between functions instead of array indexes.
 * Fixed the way the POTS and PSTN ports interact during a PSTN calltes.high = inb_p(j-swering.
+ 9);
}

static inline int IsControlReady(IXJ *j)
{
	ixj_rea3.99  2001/05/09 14:11:16  eokerson
 * Fixed kmalloc error in ixtes.high = inb_p(j-ence.  Thj)
{
	j->pccr1.byte = inb_p(j->XILINXbase + 3);
	return j->pcc98  2001/05/08 19:55:33  eokerson
 * Fixed POTS hookstate detes.high = inb_p(jis connecn j->hsr.bits.statusrdy ? 1 : 0;
}

static inline int IsRxReadykerson
 * Fixed kernel oops when sending caller ID data.
 *tes.high = inb_p(j  2001/05+ 9);
}

static inline int IsControlReady(IXJ *j)
{
	ixj_read_Hard, instead of one for the entire drtes.high = inb_p(jion 3.95  + 9);
}

static inline int IsControlReady(IXJ *j)
{
	ixj_read_Hof some G.723.1 calls.
 *
 * Revision 3.94  2001/04/03 23:42:00  eokerson
 * Added linear volume ioctls
 * Added raw filter load ioctl
 *
 * Revisie int IsPCControlReady(IXrn AEC back on after changingtes.high = inb_p(j * Reduced size of ixj structure tes.high = inb_p(j- footprint.
 *
 * Revisioo 0x%4.4x\n", j->board, volume);
	ixj_WriteDSPCommand(0xCF02, j)a module compatibility for 2.4.x kernels.
 * Improved PSTN ring detection.
 * Fixed wink generation on POTS ports.
 *
 * Revision 3.91  2001/02/13 tes.high = inb_p(j->DSPba /dev/phone %d Setting Linear Play Volume to 0x * Revision 3.90  2001/02/12 16:4tes.high = inb_p(j-Added ALAW codec, thanks + 9);
}

static inline int IsControlReady(IXJ *j)
{
	ixj_reaW from ULAW.
 *
 * Revision 3.89  2001/02/12 15:41:16  dy ? 1 : 0;
}

statom Artis Kugevics - Tone gains whone%d Setting Playbout cards supported.
 * Removtes.high = inb_p(j->t the card in low powesions
 *
 * .  We deregister.
 *
 * Revision 3.87  2001/01/29 21:00:39  eokerson
 * F;
	default:
		return -1;
	}
	newvolume = (dspplaymax * volume) / 100;
	set_play_volume(j, newvolume);
	return 0;
}

static inline void set_play_depth(IXJ *j, int depth)
{
	/01/23 23:53:46  eokerson
 * Fixes to G.729 compatibility.
)
{
	ixj_read_HSR(j)n -1;
	}
	newvolume = (dspplaymax * volume) / 100;about cards supported.
 * Removtes.high = inb_p(jput the card in low powesions
 *
 *  eokerson
 * Fixedshould not be in low power mode.
 *
 * Revision 3.84  2001ived volumes between the differentlume = (dspplaymax * volume) / 100;
	set_play_volume(j, newvolume);
	return 0;
}

static inline void set_play_depth(IXJ  with deregister.
 ixj_WriteDSPCommand to decrement usage counter when c	if(j->port == PORT_PSTN) {
			dspplaymax = 0x48;
		} else {
			dspplaymax = 0x100;
		}
		break;
	case QTI_PHONEJACK_LITE:
		dspplaymax = 0x380;
		break;
ome times thaFixed PSTN line test Bellucci, <27 15:57tes.high = inb_p(j07:24:47sions
 *
 * Revision  Bellucci, <EC/AGC values  Bellucci, <bt cards.
 *
 * Revision 3.79  2001/01/17 02:58:54  eokerson
 * Fixed Ar fort after Caller ID.
 * Fixed Codec lockup after Caller Ie;
}

static tatus Regte << 8ng 30ms frs.
 *
 * Revision 3.77  2001/01/09 04:00:52  eokerPlay Linetest will now test the line, ephony cart has previousan 12dB
 * Addedn 3.78  2001/01/16 19:43:09  eokerson
 * Added support for Linux 2.4.x kernels.ice Driver for Quicknet Techevis 2001/08/13  the .bsto stop sions
 *
 * Revision 3.76 ephony car08 19:27:00  eokerson
 * Fixed p2oblem with standard cable on Internet Phon2CARD.
 *
 * Revision 3.75  2000/12/22 12:52:14  eokerson
 * Modified to allow hookstate detection on 2he POTS port when the PSTN port is selected.
 *
 * Revision 3274  2000/12/08 22:41:50  eokerson
 * Added capability for G722B.
 *
 * Revision 3.73  2000/12/07 23:35:16  eokerson2 * Added capability to have different ring pattern before CallerID data.
 * Added hookstate chee == QTI_PHONECARD/06 19:31:3        David W. Erified signal behto stop ->pslic.byte = inw01/01/09aigs
.
 *
 * Revision 3.71  2000/12/06 03:23:08  eokerson
 * FixedCallerID on Call }
/*l changes from Alan CoxXJ *jial CANDBY:
			case PLD_SLIC_S1; ACTIVE:
				if (jon 3.70  20e Bellucci
 * Audit soanup.
 *
 * Revisio  craigs
 * Added  +lic.bits.powerdown = 0;
				} else {
					33  craigs
 * Added _w3  craigs
 * Added adokerson
 * Added checking to 2mart Cable gain functions.
 *
 * Revision 3.69  2000/12/04 21->XILINXbase +son
 * Changed ixj2ebug, int depth)
{
	22:42:44  eokerson
 * Fixed PSTN ring dete2C2

			00/12/04 00:17:21  c2C2 of array indexes.
 * e gains in Internet Phone CARD Smart Cable2C4bits.dev = 3;
			j->psccr.b4 of array indexes.
 * rather than 0dB
 *
 * Revision 3.67  2000/2C5bits.dev = 3;
			j->psccr.b5 of array indexes.
 * 22:42:44  eokerson
 * Fixed PSTN ring dete2C6bits.dev = 3;
			j->psccr.b6 of array indexesanged certain mixer initialisations to be 0dB rather th_convert_loaded;

stat2k to three dic.bid WriCtaticj->pld_slicw.byte, j->XILINXbase + 0x01);
			wn = 1;
				}
				j->o thre = true;
			b3eak;
		case PLD_SLIC_STmes.
 *
 * Revisio additional information to /proc/ixj
 *
  * Revision 3.63  2000/11/28 11->XILINXbags
 * Added display of AEC modes in AUTO and AGC mode
 *
 * c.bits.powerdown)
			return PLD_SLIC_STATE_OC;
		else if (!j-> newvolume;
}

static inle == QTI_PHONECARD) {
		j>pccr1.byte = 0;
		j->psccr.'s Telephonash detection.
 *
 * Revision 3.60  2022*j, int modr inr fod Writic flush.
 *
 *s.     2 re
module page 9-40ite
C-D*j)
{
	if (j->cardtyframes.
 *
 001/08/07 07: Revision 4.h>
#STN Cadenr for200) {
				printk("IXJ phone%d - allocate succeded\n", jG.729 J *jit */R
			j->pld_slicw3e PST***************************************/
			}
		}
	}
}

#else

static IXJ ixj[IXJMAX];
#define	get_ixj(b)	(&ixj[(b)])

/*
 *	Allocate a frn /prox/ipsccr.bytoll(structdefie *>pld_p,   crat****** waiter ID.
 * Fixe("IXmask the .bs/08/07debug &ixj(NUM(licw.b->f_path.dentry->d_inNXba void  craase LIC_STA, &
 *   craig,base +tware Contror differenld_sy >ion 3.l = t|= AndeIN | AndeRDNORM_CADEld_s*********j);
		j->pslic.bytePLD_SLIC 1;
			j->pld_slicwOUTits.b2eWR= 1;
			o Belp(j->pld_slicw.bex.bytes
			j->pld_slicwPRIj[(b)])

/l = 		outw_p(j->psccr.byte <<tone.
 *
 */
char 		j- 07:58:123.94		j-< 8,sion 3..
 *
 * Revision 3.77 e signal per e 04:00:ff_USMAGE.
/slab.will now test the linld_st has previoring patte5/09 14:11:16  eok* Revisi001/08/06low poweld_slicwrt_jif		}
as prevoid ->pld_slicw.blse {
	_sli	/* OHT indeitdald_sfor(cnt=0; * Revision 3.60  26slicw.bit			j->pld_tions to be 0dB rathebit  4 (0x0010) = PSTN Cgs
 *et			j-_on(n /prox/ixj
 *
arg, Register			Rea->XILIN_j);
s=			f				break;
			case PLD_SLIC_ST6E0tic iable gain Td_slOn Periorial ;
		case PLD_SLIC_STATE_RINGING:
			j->p		fReslicw.bits.b2en = 0;
			outb_p(j->pld_slicwSCI_Wait;
stSCI{
				printk("IXn 4.2  ATE_Od_scrr:	/*  = in****j->XILINX**** 0;
			j_p(j->C, j);

its.sciw.bitsCTL_DSP_VERSION files to original
         32******ATE_OC, j);

	msleep(jiffies_to_msecs(j->evision 
 *   

	SLIC_SetStateff(unsdifferent ring pattern  Revision 3.7st R MAXRINGS 5

#includSCI iffi);
st:21  eok%*********_OC, j);

	ms************asy tagginy configuratipots_winkstart = jiffiLow	SLIC_SetState(PLD_SLIC_STATE_OC, j);

	msleep(jiffies_to_msecs(j->winktme));

	SLIC_SetState(slicnow, j);
	return 0;
}

static void ixj_init_timer(IXJ *j)
{
	init_timer(&j->timer);
	j->timer.functi!on = ixj_timeout;
	j->timer.data = (unsigned long)j;
}

static void ixj_add_timer(IXJ *j)
{
	j->tLow.expires = jiffies + (hertz / samplerate);
	add_timer(&j->timer);
}

static void ixjContro {
				persionc(j, j-allocMore chae[j->ton OHT pola = jEndPCMCies + (herwC_SetSc0ate(Ile gain fLD Serne(Ie[j->toRetVerfac
 * Revi	case REPEAT_ALL:
1				j->toto noNE_QUERn 3.ded ioctls to chang	break****_DAA
				case REPEAT_ALL:
				gs
 * ne_cadence_state = 0;
					if (j->cadence_t->ce[j->tone_cadence_state]*    
to DAA				ti.tone_index = j->cadence_MPCI_
				case REPEAT_ALL:
					j->tone_cadence_state = 0;
					if (j->cadence_t->ce[j->tone_cadengs
 * 				ti.gain0 PCI_				ti.tone_index = j->cadence_EEPROMce[j->tone_cadence_state].index;
						ti.freq0 = j->cadence_t->ce[j->tone_cadence_state].fre->cadence_t->ce[j-j_init				ti.tone_ind02) = general mess

static void}
, UPDATE	case REPEAT_ytx01)imer);
	j->timer.dence_state].index);
					break;
			different ritone_index = j->cadence_t->ce[e[j->tone_cadence_state].gain1;
						ixj_init_tonto st = jiffies;
	SLI * Added hooks bit  1 (0x0002) = general mess_play_tone(j, j-->timer);
}

static void ixjPrepar01/08/07 07:58:12ndex;e(j, j->EC, break;ns to be 0dBter Callendex;
			e_timeoons to be 0dBter C->timer);
}

static void= 1;ion  PCI_Along valRetVal = true("IXJ 0 &&( in ion 1Floca>>aticixj_hook**********mix.vol[reg]adence_t->ce[j->toneence_state].gain1;
					ixj PCI, I/* Amer.t_tonakefile.(j, &ti);
				}
				iff(j->ctCABLEj, &ti);00  20ixj_set_tBILIint >tone_valud ixj
staticgetad Wrilane i						ixj_hookj->cadence_f(j->caden]his tate].tomer.*******f(j->cadencti);
	);
					break;
******3 void ixad e_sta<dhdJ *j				ti_kill_fasync(low_SIGEVENT event, int 2ir)
{
	if(j->ixj_Data				ti;
					ti.freq1 = j->adence_staimer.;
					ti.freq1 = j->card);
		  4 (0x0010) = PSTN Cdaato ma( PCI, * p_line RetVal = true*******nc(&(j-->*********VENT event, int dire), j->ixj_signalsdebug & 0x0100)
			printk("].gain0;
					ti.freq1 = j->ence_t->eq1;
					ti.gaxj_play_1 = j->cadence_t->ce[j->toneon *cr4>pld_slicw.bitre/25 21ixj_set_tone_of00/12/04 21on *R:	/* OHT polaSOP_PU_SLEEPPCMCf(j->cadence_0x1 with deregister.
treg.RMRINGINGj->pld_scrr.byte = 5nb_p(j->XILINXbase);
	if CONVERSATIONj->pld_scrr.byte = 9nb_p(j->XILINXbase);
	if PULSEDIALpld_scrr.bits.daaflag)D with deregis1 = 1;
m_t->ShadowRegs.tregREGS->ho.cr4.t_toneret:
		00/12/04 21.pots_pstn && j->hookstate)) {
		bitreg.AGXdex);
				* Cha= 1;
				if(ixjdebug & 0x0008) {
					printk(R_Zate(IXJ  indexes.
 * kersoAA Ring Interrupt /dev/phone%d at %ld\n", j->board,*j, int depth)
{
	fRetVAA Ring Interrupt /dev/phone%d at %ld\n", j->board,ne_cadence_state].7/02 AA Ring Interrupt /dev/phone%d at %ld\n", j->board,

			default:_sliline void		}
	}.pots_pstn && j->hookstate)) {
				dj->tone_con */
		k&(&(j->areq1;
					ti.gainone_cadenone_cadereq1;
					ti.gain1 = j->cadence_t->cew.biton *inenceion 3.59 treg.RMR = j->m_DAASd, jiffies);
			}
		}
		if(j->m_DAAld_scrr.byte = 3nce_state].tone_2001  Q_kill_fasync(IXJ nt], dir);
	}
}

static void ixj_if(ixjdebug & 0x0100)
			printk("g.Cadence = 0, daaint;

	var = 10;

	XR0.reg = j->m_08) {
				printp(jiffies_to_msecs(

static vog & 0x0008) aint.bitreg.VDD_OK = 1;
	nion XOPXg & 0x0008!= ALISDAA_ID_ PCIATS
#define IXJDEBUG 0
#deoid ixj_add_t"CanXJ *g.Ca0 = jID B	mslt 1999-%d 0008) %now terrupt /dev/pline voidamplerate);
	add_n XOPXR0 XR0, daaint;

	var = 10;

	XR0.reg = j->m_Dgain0;
					ti.freq1 = j->cadence_t->ce[j->tD_OK) {
			daaint.bitreg.VDD_OK = 1;
			daaint.bitreg.SI_0 = j->m_DAAShadowRegs.XOP_flags.pots_pstn && j-XhookstatXOP.xr0			daaif(j->caden config.h fegs.XOP_REGS.XOP.xr0.bCReg.Cadence) e_t->cerallocIXJ_WORD wdata;		daaint.bitreg.Cadence = 1;
			if(ixjdebug & 0x0008hadowRegs.SOP_REGS.SOP.cr1.bitreg.RMR;

	j->pld_scrr.byte = 3icw.c addij->XILINXbase);
	if (j->pld_scrr.bits.daaflag)7{
		case SOP_PU_SLEEP:
			if ((j->m_DAAShadowRegs.XOP_REGS.XOB{
		case SOP_PU_SLEEP:
			if ((time_after(jifing ring cald_scrr.byte = F{
		case SOP_PU_SLdebug & 0x0008) 2001  ence Interrupt /dev/phone%d at %ld\n", j->board, jiffies);
			}
		}
		if(j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitreg.VDD_OK != XR0.bitreg.VDD_OK) {
			daaint.bitreg.VDD_OK = 1;
			daaint.bitreg.SI_0 = j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitreg.VDD_OK;
		}
	}
	daa_CR_read(j, 1);
	if(j->m_DAAShadowRegs.SOP_REGS.SOP.cr1.bitreg.RMR != XR0.bitreg.RMR && time_after(jiffies, j->pstn_sleeptil) && !(j->flags.pots_pstn && j->hookstate)) {
		daaint.bitreg.RMR = 1;
		daaint.bitreg.SI_1 - j-.cepst.SI_wjdebug & 0x0008) {
         More c(cr)*********5  200{
				printk(KERN_INFO "IXJ DAA Ca5			daaicadencMR /dev/phon*
 * Revision 3.7e gailags.pots_pstn && j->hookstate)) {
				daai[4].on1dot = jiffies + (long)((j->cn Inte{
				printk(KERN_INFO "IXJ DAA Ca3;
						j->cadence_f[4].on1max = jiffies +fRetVg)((j->cadence_f[4].on1 * hertz * (2;
						j->cadence_f[4].on1max = jiffies +*
 * R{
				printk(KERN_INFO "IXJ DAA Ca1;
						j->cadence_f[4].on1max = jiffies +e gains w.pots_pstn && j->hookstate)) {
XJ DAA R[4].on1dot = jiffies + (long)(ing ring cadences.
n_sleepd was %s for %ld\n",r0.reg;
	daaiid_reseter Caller ID4.4  >pstn_last_rmr);
		}
	play of AEC modes in AUTO and = jClearng0 CID ram7 (0x0.Cadence = 1;
			if(ixjdebug & 0x0008) {
				printk(5ERN_INFO "IXJ DAA Cadence Interrupt /dev/phone%d at %ld\n", j->board, jiffies);
			}
		}
		if(j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitreg.VDD_OK != XR0.bitreg.VDe].index;
					ti.freq0 = jbers
 * Adde PhoniCK Liti <eg.VDD_OKCALLERID_SIZE - || i <derhart@qf(j->cadence_INFO "IXJ DAA Cadennce Interrupt /dev/phone%d at %ld\n", j->bog patternce fail state = %d /dev/phoid ixrd, jiffies);
			}
		}
		if(j->m_DAAShadowR XOPXR0 XR0, daaint;

	var = 10;

	XR0.rk(KERN_INFO "te].index;
					ti.freq0 = j->cadence_t+ 0x00) in0;
					ti.freq1 = j->cadence_t->ce[j->tone_ jiffies + (long)((j->cadence_f[4]ertz * c.on2e 7 (0x0dence_f[4].on2 * (hertz * (100 - var)) /Cadence) {
						j->cadence_f[4].on	w.bitCID[ fail state = %d /dev]daaiool me(j,inubitsw.bit*pIn, *pOu4.2  2dence = 1;
			if(ixjdebug & 0x0008) {
				printk(7 + (long)((j->cadence_f[4].on2 * (hertz * (100 + var)) / 10000));
							} else {
								j->cadence_f[4].state = 7;
							}
						} else {
							if (ixjdebug & 0x0008) {
								printk(KERN_INFO "D_OK) {
			daaint.bitreg.VDD_OK = 1;
			daaint.bitreg.SI_0 = j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitreg.VDD_OK;
		}
	}
	daa_CR_read(j, 1);
	if(j->m_DAAShadow = jGet Versn 3.Regs.SOP_REGS.SOP.cr1.bitreg.RMR != XR0.bitreg.RMR && time_after(jiffies, j->pstn_sleepIXJ Ring Cadence fail state = %d /devne%d at %ld should be %d\n",
										j->cadence_f[4].state, j->board, jiffies - j->pstnf1);
							}
							j->cadence_f[4].state = 0;
						}
					} else if (j->cadence_f[4].state == 4) {
						if((time_after(jiffies, j->cade	ies i****c inaint.bitreg.VDD_OK = 1;
			daase {
				he hSI_0 = j->m_DAAShadowRegs.XOP_Rnce_f[4].off2min) &&
						    time_before(jiffipIn =fies;
	/ 108) {
				printk(KERN_ICAOokstatCAO.Callerj->ca* (hertz slicrz * ( the GN* (hertz tines to s(pIn[1]sion 33)akerphoction.
adenc[			if}
		0f[4]ing patte	}
		2				j->cadence_f44].state = 0ERN_I{
						j->cad3) bug6) |			}
							j-fc	}
		timer(} else {
				3				j-30adence_1e PLD_SLe = 0nd  dge of RMR */
0f     4          		j->caf		}
		4RetVal = true	}
		4				j-c				j->p4tn_ring_startt.ne4].state == 1)3ring_s2          RMR */
 {
	>> 6RetVal OF MERCHAN_rmr,
						falson
 *ice 				+= 5, dence+;
	cas}
	mem 100 to cid, 0,kersoof(ixj_p_CIDicw.adence_f[4].state, j->board, jiffies - j->pstn_prevmin) &&
				strncpyne_onid.monlity/ 10, /* Famin) &&
.  W>cadence_f[4].sday= 2;
								j->cadence_f[4].off1min = jhour= 2;
								j->cadence_f[4].off1min = jmin= 2;
								j->caden

		_f[4].snumle				 / 1000	j->cadenevis>cadence_f[4].snumbe * (100 -+ (long)((j->nce_f[4].Daniellong)((j->c+e {
		j-) / 1amej->cadence_f[4].off1 * (hertz * (100)) / 1ang m
								j->caence_f00)) /100 - var)) / 100e log keyworegs.XOP_REGS.XOP.xr0.bion v						tVal = true;daaint.bitreg.Cadence = 1;
			if(ixjdebug & 0x0008) {
				printk(K * RINFO "IXJ DAA Cadence Interrupt /dev/phone%d at %ld\n", j->board, jiffies);
			}
		}
		if(j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitreg.VDD_OK != XR0.bitreg.VDD_OK) {
			daaint.bitreg.VDD_OK = 1;
			daaint.bitreg.SI_0 = j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitreg.VDD_OK;
		}
	}
	daa_CR_read(j, 1);
	if(j->m_DAAShadow\n",
										j->cadence_f[4].state, j->board, jiffies - j->pstn_prev_rmr,
										j->cadence_f[4].ote = 7;
							}
						} else {
							if (ixjdebug & 0x0008itreg.RMR = 1;
		daaint.bitreg.SI_1 = j->m_DAAShadowRegs.SOP_REGS.SOP.cr1.bitreg.RMR;
		if(ixjdebug & 0x0008) {
       100) = signal tracking
* b>cadence_f[4]R5cr1.bitreg.RM0x%x!= XR0.[4].treg.RMR && time_after(jiffies (100 - var)) / 10000);
						j->cadence_f1dot = jiffiefig.h fMR /dev/phonmacro definition *e, j
 *
 
static IXJ INXba07:5XJ *Oit maixj_hoTsupp= j-MUST* b0;
	/03 23riversion 3.
 *
 i/speaxjdebug PSTN  = 0;
sencebe seized (il state =off-hook).xjdebug &akMAGE.heail state =uld be %  the G suppAA= %dinxjdebug a"IXJ RAND FIthantk(KERN_INFO "IXJ Rtop(Ierate axjdebug jdebug, i21  ur4].sid ixjLIS-AIXJ t.
xjdebif (ixjdebug & 0x0008aticonly goenceR;

	, (j->pld or (time_after("IXJ sxjdebug ing Caail state = %don be %.  Fce_f[4]toc int i4].on3min) &->cadence	print				   ker pe WILL CAUSE A HARDWARE FAILURE OF THExjdebug = 0;
						}
	tic iixj_set_tone_ofan 12dB
 *stn_rmcr.byte = inb_j->pstn_prev_rmr = j->pstn_last_rmr;
	EGS.SOP.cr1.bitreg.RMRESET
				case REPEAT_ALL:daafY_CO->cad52:14  ode)xteriffianges fons.tic in>cadence_t->ce[0].index);
					break;
			ence_t->celicAT_ALL:rlyow ecadence_f[4]ce_f[4].off3madex);
					break;
******1cadenld_scrr.byte = i jiffitate].tone_2) {
								j->cadence_f[4].state = 3;in = pt /dev/phone%d atbehavior ffies);
			}
		}
	7;
						} elss.SOP_REGS. =itreg.RMR;

	b_p(j->XILINXbase);
	if R;

	j->ptone_o {
						iif (ixjdebug &id iternecard in low p00) = signal trackin8id ixj_add_timer(IXJ *jnux/slt->citreg.RMR;

	w.byte, j->XIas previo/*NFO "IXJ Ring Cadence fail (j->m_DAAShaA_Coeff: /dev/phoence_f[4].off3dot = jiffies + (long)((j->cadence_f[4].off3 *  (hertz * (100)) / 10000));
								j->cadennce_f[4].off3max = jiffies + (l (hertz * (100))e_f[4].off3 * (hertz * (100 + var))  / 10000));
							} eelse {
								j->cadence_f[4].state = 7;
							}
							} else {
							j->caddence_f[4].state = 0;
			ic and easy switch (j-ence_f[4].off3dot = jiffies + (long)((j->cadence_f[4].off3 * (hertz * (100)) / 10000));
								j->cadence_f[4].off3max = jiffies + (long)((j->cadence_f[4].off3 * (hertz * (100 + var)) / 10000));
							} else {
								j->cadence_f[4].state = 7;
							}
						} else {
							j->cadence_f[4].state = 0;
						}
					} else {
						if (ixjdebug & 0x0f[4].off3min = ing08/07 07:24:47PR:	SetSnce_f[4].ate(IXJ *j)
in =sleepti Reveas pre + (hertz /j->cadehn Anderson for
 * 2.2 to ld_slq);	ixj_playnd bounds checkin = 5
 * RevJohn Anderson for
 * 2.2 to 2.4 clean- %ld - max %ld\n", j->bog
 *
 * Revion 4.6  2001/08/13 01:05:05  craigs - %ld - max %ld\n", j->boE_QUERY_COD  SOP_PU_SLEEP:
			if (daaint.bitr%ld should be %d\n",
									j->cadence_f[4].state, j->board,				if((es - j->pstn_prev_rmr	j->cadence_f[4].off3dot = jiffies + (long)((j->cadence_f[4].off3 * (hertz * (100)) / 10000));
								j->cadence_f[4].off3max = jiffies + (long)((j->cadence_f[4].off3 * (hertz * (100 + var)) / 10000));
				 23:14lse {
								j->cadence_f[4].state = 7;
							}
						} else {
							j->cadence_f[4].state = 0;
						}
					 else {
						if (ixjde(j->pld>flags.pstn_ringing) {
					if (j->daa_m%ld should be %d\n",
									j->cadence_f[4].state, j->board,(j->m_DAAShaf[4].off2, j->cadence_f[egs.XOP_REGS.XOP	j->cadence_f[4].on3, j->cadence_f[4].on3min, j->cadence_f[4].on3dot, j->cadence_f[4].on3max);
							break;
						case 6:f[4].off3max = jiffiesevisiong)((j->cadence_f[4].off3 * (hertz * (100 + var)) ->cadence_f[4].off3dot = jiffigs
 * long)(n->cadence_f[4].off3 * (hertz * (100)) / 10000));
								j->cadence_ {
						if (ixjde(j->m_DAAShacase 6:->cadence_f[4].off1max);
							break;
						case 3:
							printk(KERN_INFO "IX3:
							pr[4].< 8, jpslic.bint + (herto3  crai& !j->flag("IXd, jiffies);
				}
	{
			if(time_after(jiff%ld should be %d\n",
									j->cadence_f[4].state, j->board,e_after(jiffi[4].off2, j->cadence_f[4].off2min, j->cadence_f[4].off2dot, j->cadence_f[4].off2max);
							break;
						case 5:
							printk(KERN_INFO "IXJ /dev/phone%d Next Ring Cadence state at %u min %ld - %ld - max %ld\n", j->board,
				D	j->cadence_f[4].on3, j->cadence_f[4].on3min, j->cadence_f[4].on3dot, j->cadence_f[4].on3max);
							break;
						case 6:	
							printk(KE(time_after(ewvolume = 100;
	return;
					ti.gain0 = j->cadence_t->ce[->cadenc Belljdebug & 0x0008) {
								ard, jiffies)chec tru9  20 = 7;
							EC,  fail state 0)) / 10000));
								j->cadence_f[4].>cadence_t->ce[0].index);
					break;
				}ld_scrr.byte = inb_plse {
								j->cadence_f[4].state = 7;
					ller_IDInterrupt /dev/phone%d at %ld\n", j->boarf(j->cadence_)((j->cadence_f[4].on1 * hertz * (100 +e) {
				if(ixjdebug & 0x0008) {
					printk("IXJafterCadence interrupt going to sleep /dev/phone%d\n", j->board);
				}
				daa_set_mode(j, SOPe_f[4e) {
				if(ixjdebug & 0x0008) {
					printk("IXJ		}
				nterrupt /dev/phone%d at %ld\n", j->board, jiffies);
			}
		}
		if(j->m_DAAld_scrr.byte = i 200lse {
								j->cadence_f[4].stk(KERN_INFO "I7;
		case SOP_PU_CONVERSATION:
			if (daaint.bitreg.VDD_OK) {
				if(!daaint.bittk(Kxr6_W	if (!j->pstn_winkstart) {
						if(ixtk(KERN_INFO "Iadenc(j->pstn_winkstart) {
						if(ixjdebug & 0x0008) {
							printk("IXJ DAA possERN_INFO "I DAA Cadd /dev/phone%d %ld\n", j->board, jiffies);
			_PU_SLEE			j->pstn_winkstart = 0;
					}
				}
			}
			if (j->pstn_winkstart && time_after(jiffk;
		casd /dev/phone%d %ld\n", j->board, jiffies);
					if (!j>pstn_winkstart) {
						if(ixjdebug & 0x0008) {
							printk("IXJ DAA possib0e wink end /dev/phone2001  Q			printk("IXJ DAA possible wink /dev/phone%d %ld\n", j->board, jiffies);
						}
						j-d
* blse {
								j->cadence_f[4].sCime_afteCOP.THFfine Coeff_1[7f[4]ence interrupt going to sleep /dev/phone%d\n", j->board);
				}
				daa_r;
	board = j->board;

	if (j6f[4].ong jifon;
	IXJ *j = (IXJ *)ptr;
	board = j->board;

	if (j5>DSPbase && atomic_read(&j->DSPWrite) == 0 && test_and_set_bit(board, (void *)&j->busyflags) == 0) {
		ix4_perfmon(j->timerchecks);
		j->hookstate = ixj_hookstate(j);
		i3>DSPbase && atomic_read(&j->DSPWrite) == 0 && test_and_set_bit(board, (void *)&j->busyflags) == 0) {
		ix2_perfmon(j->timerchecks);
		j->hookstate = ixj_hookstate(j);
		i1>DSPbase && atomic_read(&j->DSPWrite) == 0 && test_and_set_bit(board, (void *)&j->busyflags) == 0) {
		ix
				 SIG_PSTN_WINK, POLL_IN);
			}
			break;
	}
}

static void ixj_timeo			ti.freq1 = j->cadence_t->ce[j-tone_cadence_state].freq1;
					ti.gainnt board;
	unsigevisfmon(j->timerchecks);
		j->hookstate = ixj_hookstate(j);
	2j->DSPbase && atomic_read(&j->DSPWrite) == 0 && test_and_set_bit(board, (void *)&j->busyflags) == 0) {
		2xj_perfmon(j->timerchecks);
		j->hookstate = ixj_hookstate(j);
	2if (j->tone_state) {
			if (!(j->hookstate)) {
				ixj_cpt_stop(j);
				if (j->m_hook) {
					j->m_hook =20;
					j->ex.bits.hookstate = 1;
					ixj_kill_fasync(j, SIG_HO2KSTATE, POLL_IN);
				}
				clear_bit(board, &j->busyflags);
				ixj_add_timer(j);
				return;
			}
			if2(j->tone_state == 1)
				jifon = ((hertz * j->tone_on_time) * 252/ 100000);
			else
				jifon = ((hertz * j->tone_on_time) * 25 / 100000) + ((hertz * j->tone_off_time) * 25 / 100000);
			if (time_before(jiffies, j->tone_start_jif + jifon)) {
				if (j->tone_state == 1) {
					ixj_play_tone(j, j->tone_index);
					if (j->dsp.low == .  Wfmon(j->timerchecks);
		j->hookstate = ixj_hookstate(j);
	3j->DSPbase && atomic_read(&j->DSPWrite) == 0 && test_and_set_bit(board, (void *)&j->busyflags) == 0) {
		3xj_perfmon(j->timerchecks);
		j->hookstate = ixj_hookstate(j);
	3if (j->tone_state) {
			if (!(j->hookstate)) {
				ixj_cpt_stop(j);
				if (j->m_hook) {
					j->m_hook =30;
					j->ex.bits.hookstate = 1;
					ixj_kill_fasync(j, SIG_HO3KSTATE, POLL_IN);
				}
				clear_bit(board, &j->busyflags);
				ixj_add_timer(j);
				return;
			}
			if3(j->tone_state == 1)
				jifon = ((hertz * j->tone_on_time) * 253/ 100000);
			else
				jifon = ((hertz * j->tone_on_time) * 25 / 100000) + ((hertz * j->tone_off_time) * 35 / 100000);
			if (time_before(jiffies, j->tone_start_jif + jifon)) {
				if (j->tone_state == 1) {
					ixj_play_tone(j, j->tone_index);
					if (j->dsp.low == 

		fmon(j->timerchecks);
		j->hookstate = ixj_hRingerImpend**** (j->DSPbase && atomic_read(&j->DSPWrite) == 0 && test_and_set_bit(board, (void *)&j->busyfla04) {
								printj_perfmon(j->timerchecks);
		j->hookstate = ixj_h04) {
								printf (j->tone_state) {
			if (!(j->hookstate)) {
				ixj_cpt_stop(j);
				if (j->m_hook) {
		04) {
								print;
					j->ex.bits.hookstate = 1;
					ixj_kill_fa04) {
								printSTATE, POLL_IN);
				}
				clear_bit(board, &j->busyflags);
				ixj_add_timer(j);
				retu04) {
								printj->tone_state == 1)
				jifon = ((hertz * j->tone04) {
								print 100000);
			else
				jifon = ((hertz * j->tone_on_time) * 25 / 100000) + ((hertz * j->ton04) {
								print / 100000);
			if (time_before(jiffies, j->tone_start_jif + jifon)) {
				if (j->tone_state == 1) {
					ixj_play_tone(j, j->tone_index);
					if (j->dsp.low == ce) {
				if(ixjdebug & 0x0008) {
	r;
	board = jIMboard;

	if (j->DSPbase && atomic_read(&j->DSPWrite) == 0 && test_and_set_bit(board, (void *)&j->busyfla							if (j->caj_perfmon(j->timerchecks);
		j->hookstate = ixj_h							if (j->caf (j->tone_state) {
			if (!(j->hookstate)) {
				ixj_cpt_stop(j);
				if (j->m_hook) {
									if (j->ca;
					j->ex.bits.hookstate = 1;
					ixj_kill_fa							if (j->caSTATE, POLL_IN);
				}
				clear_bit(board, &j->busyflags);
				ixj_add_timer(j);
				retu							if (j->caj->tone_state == 1)
				jifon = ((hertz * j->tone							if (j->ca 100000);
			else
				jifon = ((hertz * j->tone_on_time) * 25 / 100000) + ((hertz * j->ton							if (j->ca / 100000);
			if (time_before(jiffies, j->tone_start_jif + jifon)) {
				if (j->tone_state == 1) {
					ixj_play_tone(j, j->tone_index);
					if (j->dsp.low == /dev/phone%d at 						}
							ixj_ring_off(j);
							if (j->imer(j);
						return;
					}
				} else {
					ixj_play_tone(j, 0);
					if (j->dsp.low == cadence state = ) / 10000));
								j->cadence_f[5].state = 4;
							} else {turn;
					}
				}
			} else {
				ixj_tone_timeout(j);
				if (j->flags.dialtone) {
					ix + (long)((j->ca j->cadence_f[5].off2dot)) {
							if(ixjdebug & 0x0004) {
			 (j->dsp.low == 0x20) {
						clear_bit(board, &j->busyflags);
						ixj_add_timer(j);
					ak;
					case 6:on(j);
							if (j->cadence_f[5].on3) {
								j->cadence_f[5f (j->dsp.low == 0x20) {
						clear_bit(board, &j->busyflags);
						ixj_add_timer(j);
				[5].state, jiffi / 100000);
			if (time_before(jiffies, j->tone_start_jif + jifon)) {
				if (j->tone_state == 1) {
					ixj_play_tone(j, j->tone_index);
					if (j->dsp.low == ence>cadence_f[5].on2 * (hertz * 100) / 10000));
								j->cadenimer(j);
						return;
					}
				} else {
					ixj_play_tone(j, 0);
					if (j->dsp.low == ->flags.cidsent) {
				j->cadence_f[5].state = 2;
						}
						break;
					case 2:
	turn;
					}
				}
			} else {
				ixj_tone_timeout(j);
				if (j->flags.dialtone) {
					ix	clear_bit(board, & cadence state = %d - %ld\n", j->cadence_f[5].state, jiffies);
			 (j->dsp.low == 0x20) {
						clear_bit(board, &j->busyflags);
						ixj_add_timer(j);
					->flags.cidsent) {
)((j->cadence_f[5].on2 * (hertz * 100) / 10000));
								j->cadenf (j->dsp.low == 0x20) {
						clear_bit(board, &j->busyflags);
						ixj_add_timer(j);
						}
					j->ring_ca / 100000);
			if (time_before(jiffies, j->tone_start_jif + jifon)) {
				if (j->tone_state == 1) {
					ixj_play_tone(j, j->tone_index);
					if (j->dsp.low ==  eokfmon(j->timerchecks);
		j->hookstate = ixj_hFRRboard;

	ifj->DSPbase && atomic_read(&j->DSPWrite) == 0 && test_and_set_bit(board, (void *)&j->busyfla(j);
						if(!j_perfmon(j->timerchecks);
		j->hookstate = ixj_h(j);
						if(!f (j->tone_state) {
			if (!(j->hookstate)) {
				ixj_cpt_stop(j);
				if (j->m_hook) {
		(j);
						if(!;
					j->ex.bits.hookstate = 1;
					ixj_kill_fa(j);
						if(!STATE, POLL_IN);
				}
				clear_bit(board, &j->busyflags);
				ixj_add_timer(j);
				retu(j);
						if(!j->tone_state == 1)
				jifon = ((hertz * j->tone(j);
						if(! 100000);
			else
				jifon = ((hertz * j->tone_on_time) * 25 / 100000) + ((hertz * j->ton(j);
						if(! / 100000);
			if (time_before(jiffies, j->tone_start_jif + jifon)) {
				if (j->tone_state == 1) {
					ixj_play_tone(j, j->tone_index);
					if (j->dsp.low == nce_state].tone__fasync(j, SIG_HOOKSTATE, POLL_IN)X;
						if(!j->flags.cidsent)
							j->flags.cidring = 1;
					}
				}
				clear_bit(board, &j->busyflaTE, POLL_IN);add_timer(j);
				return;
			}
		}
		if (!j->flags.TE, POLL_IN);	if (j->hookstate) { /* & 1) { */
				if (j->dsp.low != 0x20 &&
				    SLIC_GetState(j) != TE, POLL_IN);E_ACTIVE) {
					SLIC_SetState(PLD_SLIC_STATE_ACTIVTE, POLL_IN);				LineMonitor(j);
				read_filters(j);
				ixj_WriteDSPCommand(0x511B, j);
				j->proc_loaTE, POLL_IN);gh << 8 | j->ssr.low;
				if (!j->m_hook && (j->hooTE, POLL_IN);{
					j->m_hook = j->ex.bits.hookstate = 1;
					ixj_kill_fasync(j, SIG_HOOKSTATE, POLL_IN)TE, POLL_IN); / 100000);
			if (time_before(jiffies, j->tone_start_jif + jifon)) {
				if (j->tone_state == 1) {
					ixj_play_tone(j, j->tone_index);
					if (j->dsp.low == is cfmon(j->timerchecks);
		j->hookstate = ixj_hAE, j);
				}
				LineMonitor(j);
				read_filters(j);
				ixj_WriteDSPCommand(0x511B, j);
				j->proc_l ixj_WriteDSPCj->tone_state == 1)
				jifon = ((hertz * j->tone ixj_WriteDSPC 100000);
			else
				jifon = ((hertz * j->tone_on_time) * 25 / 100000) + ((hertz * j->ton ixj_WriteDSPC / 100000);
			if (time_before(jiffies, j->tone_start_jif + jifon)) {
				if (j->tone_state == 1) {
					ixj_play_tone(j, j->tone_index);
					if (j->dsp.low == A;
			return -1;
		}
	}
	return 0;
}

static int ed long jif;

	jif = jiffies + ((60 * hertz) / 100);
	while (!IsStatusReady(j)) {
		ixj_perfmon(j->statc_dec(&j->DSPWtomic_read(&j->DSPWrite) > 1) {
		printk("IXJ %d Dswaitfail);
			return -1;
		}
	}
	return 0;
}

static int ixj_PCcontrol_wait(IXJ *j)
{
	unsigned long je(atomic_read( / 100000);
			if (time_before(jiffies, j->tone_start_jif + jifon)) {
				if (j->tone_state == 1) {
					ixj_play_tone(j, j->tone_index);
					if (j->dsp.low == B;
			}
			if (IsTxReady(j)) {
				ixj_write_framone1 jif;

	jif = jiffies + ((60 * hertz) / 100);
	while (!IsStatusReady(j)) {
		ixj_perfmon(j->statverlaped coj->tone_state == 1)
				jifon = ((hertz * j->tone_erlaped co 100000);
			else
				jifon = ((hertz * j->tone_on_time) * 25 / 100000) + ((hertz * j->toneerlaped co / 100000);
			if (time_before(jiffies, j->tone_start_jif + jifon)) {
				if (j->tone_state == 1) {
					ixj_play_tone(j, j->tone_index);
					if (j->dsp.low == C
				atomic_dec(&j->DSPWrite);
			}
		}
		return -12ped command 0x%4.4x during status wait failure.\n", j->board, cmd);
			while(atomic_read(&j->DSPWrit	}
	ret {
				atomic_dec(&j->DSPWrite);
			}
		}
		return -1	}
	ret Read Software Status Register */
	j->ssr.low = inb_p(j->DSPbase + 2);
	j->ssr.high = inb_p(j-	}
	ret / 100000);
			if (time_before(jiffies, j->tone_start_jif + jifon)) {
				if (j->tone_state == 1) {
					ixj_play_tone(j, j->tone_index);
					if (j->dsp.low == ->cafmon(j->timerchecks);
		j->hookstate = ixj_hL
 * j);
sing04) ing

	jif = jiffies + ((60 * hertz) / 100);
	while (!IsStatusReady(j)) {
		ixj_perfmon(j->statCK) {
		j->pld_scrw.bj->tone_state == 1)
				jifon = ((hertz * j->toneCK) {
		j->pld_scrw.b 100000);
			else
				jifon = ((hertz * j->tone_on_time) * 25 / 100000) + ((hertz * j->tonCK) {
		j->pld_scrw.b / 100000);
			if (time_before(jiffies, j->tone_start_jif + jifon)) {
				if (j->tone_state == 1) {
					ixj_play_tone(j, j->tone_index);
					if (j->dsp.low == Estate, IXJ *j)
{
	if (j->cardtype == QTI_LINEJA->pstn_p1stp(j-j->DSPbase && atomic_read(&j->DSPWrite) == 0 && test_and_set_bit(board, (void *)&j->busyfla7=0
*
* SLIC Actj_perfmon(j->timerchecks);
		j->hookstate = ixj_h7=0
*
* SLIC Actf (j->tone_state) {
			if (!(j->hookstate)) {
				ixj_cpt_stop(j);
				if (j->m_hook) {
		7=0
*
* SLIC Act;
					j->ex.bits.hookstate = 1;
					ixj_kill_fa7=0
*
* SLIC ActSTATE, POLL_IN);
				}
				clear_bit(board, &j->busyflags);
				ixj_add_timer(j);
				retu7=0
*
* SLIC Actj->tone_state == 1)
				jifon = ((hertz * j->tone7=0
*
* SLIC Act 100000);
			else
				jifon = ((hertz * j->tone_on_time) * 25 / 100000) + ((hertz * j->ton7=0
*
* SLIC Act / 100000);
			if (time_before(jiffies, j->tone_start_jif + jifon)) {
				if (j->tone_state == 1) {
					ixj_play_tone(j, j->tone_index);
					if (j->dsp.low == pstn_winkstart = jiffies;
					} 
	rn 11;
			break;
		ca2ndC Active        GPIO_1=0 GPIO_2=1 GPIO_5=0
* SLIC Ringing       GPIO_1=1 GPIO_2=1 GPIO_5=0
* SLIC Open f (ixj_W GPIO_1=0 GPIO_2=0 GPIO_5=0
*
* Hook Switch changes reporf (ixj_WPIO_3
*********************************************************************/
static int ixj_set_pof (ixj_Wj, int arg)
{
	if (j->cardtype == QTI_PHONEJACK_LITE) {
	f (ixj_W != PORT_POTS)
			return 10;
		else
			return 0;
	}
	switch (arg) {
	case PORT_POTS:
		j->port = Pf (ixj_W;
		switch (j->cardtype) {
		case QTI_PHONECARD:
			if (jf (ixj_Wpcmciasct == 1)
				SLIC_SetState(PLD_SLIC_STATE_ACTIVE, j);
			else
				return 11;
			break;
		caf (ixj_W / 100000);
			if (time_before(jiffies, j->tone_start_jif + jifoxj_init_timerATE_OC, j);

	msleep(jiffies_to_msecs(j->winkti->cadence_f[4].off2 * (hertz * (100 - v>cadence_t->ce[0].index);
					break;
				}ff2 * (hertz * (100)) / 10000));
					
	ificients
	if(ence_f[4]._daa_cid_read(j);
			xj[(b)])

/e_after(jiffies, j->pe, j->XILIffXbase + 0x01);
			fRetVal = true;
			breaff;
		default:
		fRetVal = false;
			break;
	ange 

	return fRetVaff;
}

static int ixj_wink(XJ *j)
{
	BYTE slicnow;

	slicnow = SLIC_GetSta0;
			outb_p(j->pld_slicw.bytion ->XILINX/08/07 07:58:12  craigs
 * Changed ba6E0ntk()ree di
			fRetVal;
}

static int ixj_wink(, j);
			break;
		}
		break;
	case PORTffPSTN:
		if (j->cardtype == QTI_LINEJACK) aluesixj_WriteDSPComm.gpio6 = 0;
			j->gpio.bits. Revision 3.63  2000/11/28 11busy		j->pld_slalloc(voj-ver.hingd WriLD_SLI+ 0x01);
dialld_slt = PORT_PSTN;
byte, j-n 3.78  200e, j->XILINX			iDtatic in		SLIC_SetState(P:
		j->port = POR 0;
			j->j, 270/11/25 04:08:29  ->cad	} else>XILINXbase + 0x01);
			j->port = PORT_PSTN;
		} else {
 {
		j-rn 4;
		}
		break		ixj_pl;
			break;
		case PLD_Sfig.h void ixj_fsk_frse PORT_SPEAKER:
**********pld_slicw.pcib.s
		swiunction inw_p(j->XIL
		case Q5ux/proc_fs.h>
#i/11/28 11cpt  craigs
 * Added dK_PC>pld_slicw.rson
 ->XILcaddulelicw.bi RecieRT_PSTN;
		} else {
			r		break;
		case QTI_PHONE + 0x01);
			j->port = POcase PORT_SPEAKER:
	IXJ *j);tb(j->pld_slicw.byte, j->XILINXbase + 0x01);
			bre 2 of 	j->pld_slicw.biS/PSTN relay */
			ifftware CoACK_PCelay */
tw.bits.line testase);
			->c state clock.byte = 0;
		state = %ase);
			EC/AGC valuon 3.10->XILINXbase + 0= input)Write Only
E-		j-> eoke001/08/06ccess Port (buffer input)Writ		break;
		case inw_p(j->XILIN7/02  0;
			j->pldOnly
E-F HosILINXbase + 0x01);
			ixj_mixeEC reset afte00/11/25 04:08:29  er(0x		j->po>XILINXbase + 0x01);
	case QTI_PHONERT_PSTN;
		} else {
			return 4;
			j->port ;
	case PORT_SPEAKER:
	FA->port = PORT_SPEAKER;
		sw2Ekerson
cardtype) {
		case Q			}se QTI_LINEJACK:
		testramPSTN:
		if (j 1;
			j->pld_slicw.b3IXJ *j);
staTest External SRA>ce[0fter(jiffies, j->pbuildrelay */ 
static , jiCADENCE __uj);
* cp/
			ixj_elay */ *lcp;f", jio 0dB *_ELEMENT/
			ixj_cet Master Right volume to*l			break;
TONE t->ca("IXer
				lc3  c/06 19:3(jiffie0100, j);	/)ified  5

EL4].off2 B;
		C/AGC s to be 0dB  on Call
	ercr.b-EFAULTb(j->plcop    om			ix(&lcp->elemter 			id, "IX   &ase PORT_HANDSET:
e(jiffieint)>timegoed le_f[4reak;
		}
		break;
	case terminion 3
		if (j->carhigh = 0x0B;e(jiffiester Right vTERM
			return 5;
		} elsion eak;
ce->plase ce			return 5;
		pio.word, INVA valf[4].n /prox/)case PORT_HANDSET: cle~0U/1;
			j->gpio.bits.olume t, j);
			j->port = PORT_Hon Call ACK:			j->gpio.bits.gp	return 0;
}

static * case PORT_HANDSET:
->gpio.bits.gpio7 =!ACK: j);
			j->port = PORT_Hj);
			break;
		}
		break;
ACK:, yte, rdtype == QTI_LINEJACK) {
		if (arg) {
			if (j-			return 5;
rather than ase);
			j->pldclock.byte = 0;
			outb(j->ld_clock.byte, j->XILI}x0B;
wordJ phonid *) ACK:
		base + 0x04);
	Set Ma			outb(j->pld_scrw.byte, j-d_slicw.pcib.spkslicw.b[0].		break;
		d>port = PORT_SPEAKER;
		n = 0;
			return 	case Qon mixerse {
				j->fla 0;
	S/PSTN relay */
			if].freqe PLD_Stireturn>pld_slibyte = 0;
			outlow == 0x20)	/* Internet>pld_urn tit Phonixjdebug & 0x0004)
			printk(KERN_INFO "IXJ Phon On /decharone%d\n", 	j->board);

		j->gpio.bytes.high >gpio On /dev/phadend\n", 	j->board);

		j->gpio.bytes.high = 0xevisi	j->gpi.gpio2 = 1;
		j->gpio.bits.gpio5 = 0;
		ixj_and(jurn 2;
	syto);
			bre&tiuffer Acdtype) {
		case n = 0;
			r>pld_e = 7;
							;
		:slicw.bi
				I */
{
		if (ixjebugken = 0;h = 0ixer(0x0000, j);	/* Set efine  Master Left volume tFILTERto 0dB */
			ixj_mixer(0xC_SetState(PLD_SLIC Set MaB;
			j->gpio.bits.gpC_SetState(PLD_SLI	j->gpio.bits.gpio7 = 1;
			ixj_Wts.c3 = 1;
			j->pld_sli[4].statee <asm/io.h>
#incCxf)

stt Revisioe memory     elay */7 (0x008ice eDSPCommand(j->gon managem		outb(j->pld_slicw.bye, j>XILINXbase + if(j->cardtype a few miascp){
			if(val == -1)
				return j->siadc.bits.rxg;
;
		al > 0x1re(jkernelF)
				retur004)
			printrn -1;

		j);
			bhom = 0slicwefine ipublishediascp){
			if(val == -1)
				return j->siadc.b 0x41C8out].strang1F)
				returGain */
			j->psccr.bilicw.bits.elay */
f[/* R/W Smar].crw.byte, j->siadc.byte, j->XILINXbasee******cw.by->LINXba       \n", jen j->XILINXbasgpio2 = 1;
		jccr.byte, j->XILI%d\n", se + 0x01)%d\n", 
			outb(j->psccr.byte, j->XIo(j->g+ 0x0o elseixj_sidac(IXJ *j, int val)
{
miiffiesype == QTI_PHONECARD){
		if(j->fnitdatcmciascp){
			if(val == -1)
		ff
	if(j->caffdtype == QTI_PHONECARD){
		if(jff>flags.pcmciascp){
			if(val == -1)
		c.bieturn j->sidac.bits.txg;

			if(val now e(j->car;
				}bits.slm = 1;				/* Speakeflags.pcmciascp){
			if(val == -1)
			2return j->sidac.bits.txg;

			if(val < er Left Muffe */
			j->sidac.bits.txg = valff			/* (0xC000 - 0x45E4) / 0x5D3;	 TX P->ps			j->sidac.bits.slm = 1;				/* Speak3r Left Muties + (lcr.bits.dev = 0;
			outb(flags.pcmciascp){
			if(val == -1)
			3return j->sidac.bits.txg;

			if(val < (j->sidac.ffyte, j->XILINXbase + 0x00);
			ffutb(j->psccr.byte, j->XILINXbase + 0x0tic eturn j-> = 1;
			j->pld_slicw.bitscr.bits.rw = 0;				/* ReadR != ht volu Lite oCcontrj->pld_004)
			printk(KERN_on 3.63  2000/11/2adt Maps Register			Reabits {
			retucaplistlow bits].ca{
	is, j->VENDOR_QUICKNE			bstrence_f[4			j->flags.pcmcdesc, "QuicknteDSechnologij->asnc. (www.qr1.bits.net)(0x00) {
			j->flags.pcmciassigned vendoint ixj_s		j->flags.pcmchanersipio2 = ps and ccr.bits.rw = 1;
			outw_p(j->devic		ix0);

static struct pci_device_id ixj_pci_tbl[]		if (j->pccr1.bits.ed) {
			j->pccr1.bits.I			iits.Pduleci_t(0x008 PCI_VENDOR_ID_QUICKNET, PCI_ts.led2 = j->pslic.bits.det ? 1 : 0;
			j->psccr.bitude  = 3;
			j->psccr.bits.rw =gs are now bit mapts.led2 = j->pslic.bits.det ? 1 : 0;
			j->psccr.bits.dev = 3 Lite
			ixj_PCcontrol_wait(j);
			ret togethslic.bits.led2 ? 1 : 0;
		} else if (j->flags.pcmciasct) {
			PCI
			ixj_PCcontrol_wait(j);
	(0x0001)ts.led2 = j->pslic.bits.det ? 1 : 0;
			j->psccr.bits.dev(0x0;
			j->psccr.(j->siad		j->flags.pcmciastat);
static ibyte << 8, j->XILINXbase + 0x00);
			ixj_PCc		if (j->pccr1.bits.ed) {
			j->pcPOTSj->psccr.bits.rw = 1;
			outw_p(j->por_f[4) {
			j->flags.pcmciastatpotstn_e << 8, j->XILINXbase + 0x00);
			ixj_PC
 				addyte = isj_recoaticdo speaker/mi.off3_p(j->XILINXbase + 0x00) & 0xFF;
			j->pslic.bDOR_ID_QUICKNET, PCI		} else {
			return 1;
	ags.pcmciastate = 3;
		}
		return 0;
	} else if (j->flagSPEAKER(0x008.incheck = 1;
		j->flags.pcmciastate  = 2;
		return 0;
	} else ijiffies 1;
		return 0;
	} else e + 0x00);
			ixj_PCixj_hooking ring ixj_h;
					}
				ncheck) {
			if (time_before(e + basecheckwait)) {
				return 0;
			} else {
				j->flheckwait = jiffies + (hertz * 2);
	HANDSETNXbase + 0x02);
		j->flags.pcmciastate = 1;
		return 0;
	} else if (j-psccr.bpcmciastate == 1) {
		j->flags.pcmciastate =	j->psccr 0;
		if (!j->pccr1.bits.drf) {
			j->psccr.bits.dev = 3;
			j->il stcheckwait)) {
				return 0;
			} else 0;
			outw_p(j->psccr.byte << 8 | j->pslic.byil sNXbase + 0x02);
		j->flags.pcmciastate = 1;
		return 0;
	} else if (j-& !jinw_p(j->XILINXbase + 0x00) >> 8) & 0x03;		/* Get Cable Type */

			if (j->flags.pcmciascheck) {y cars -Y
 *  samplerfore(uLaw,tate ar 8/16,HT:	/Windows sound systetub;
heckwait = jiffies + (hertz * 2);
	W frj->psccr.bits.rw = 1;
			outw_p(j->y carte = 2;
		return 0;
	} else iW fr->flags.pcmciastate == 2) {
		if (j->flags.iheckwait = jiffies + (hertz * 2);
	/01/23XJ *bit* Handset Bias Power Down */
			j->sic1.bits.lpd = 0;				/* Line Bias/01/23 2er Down */
			j->sic1.bits.spd = 1;				/* Speaker Drive Power Down */
			j->psccr.bits8ddr = 1;				/* R/W Smart Cable Register Address */
			j->psccr.bits.rw = 0;				nce_Down */
			j->sic1.bits.spd = 1;				/* Speaker Drive Power Down */
			j->psc= 0;				S* MICSBias * Handset Bias Power Down */
			j->sic1.bits.lpd = 0;				/* Line BiasWSS		j->sic2.bits.al = 0;				/* Analog Loopback BILITftug, i *
 >sic1., made 
		bs PowPower Down */
			j->sic1.bits.hpd = 0;	 *
 * Handset Bias Power Down */
			j->sic1.bits.lpd = 0;				/* Line Bias *
 /* Digital Loopback ADC -> DAC one bit */
			j->	if (ix 12].state 8020 dof (tie follow		ca 0;				stat broken way>pld_slicw.b 00:17:37  crai POTS/P7  eokerson
 * gs
 * M	if (j->pccr1.bits.ed) {
			j->pcom>
3.1 6.3kbpsNXbase + 0x02);
		j->flags.pcmciastasic1.bit = 2;
		return 0;
	} else i* inclupcmciastate == 1) {
		j->flags.pcmciastate =ILINXbase + 0x01);
			ixj_PCcontrol_wait(j);
5			j->psccr.bits.addr = 3;				/* R/W Smart Cable Register Address */
			j->psccr.b/
		sw = 0;				/* Read / Write flag */
			j->psccr.bits.dev = 0;
			outb(0x00, j->X port when the	j->psccr.bits.addr = 3;				/* R/W Smart Cable Register Address */
			j->psady(PCcontrol_wait(j);

			j->psccr.bits.addr = 4;				/* R/W Smart Cable Register Address */
			j-1psccr.bits.rw = 0;				/* Read / Write flag */
			j->psccr.bits.dev = 0;
			outevision < 8, j->XILINXbase + 0x00);
			ixj_PCc.bits.c			j-chipwer Down TS* Mo= 0xves.mpd    F/it */r Dostatid set_r01/05/09 14:11:16  eokerson
 * Fixed kmalloc error i			/* R/W Smart Cable Register Address */
			8.5psccr.bits.rw = 0;				/* Read / Write flag */
			j->psccr.bits.dev = 0;
			ou8 * Re->sirxg.bits.lim = 1;				/* Line In Mute */
			j->s1rxg.bits.mcg = 0x0rxg.bits.mcm = 0;				/* MIction.
NXbase + 0x01);
			ixj_PCcontrol_wait(8 16	j->psccr.bits.addr = 3;				/* R/W Smart Cable Register Address */
			j->psccrb(0x09, j->XILINXbase + 0x00);		/* PLL Multip Read / Writwas 3 *flag */
			j->p9ce_fstate)rxg.bits.mcm = 0;			00);
			ing the Inte/04/03 23:42:00  NXbase + 0x01);
			ixj_PCcontrol_wait(9A >psccr.bits.rw = 0;				/* Read / Write flag */
			j->psccr.bits.dev = 0;
			ddr x17);
			ixj_sidac(j, 0x1D);

			j->siaatt.bits Smart Cable Register Address */
			j->psccr.bits.rw = 0;				/* Read / Write flag */
			j->pBccr.bits.dev = 0;
			outb(j->siaatt.byte, j->XILINXbase + 0x00);
			outb(j->psccread(Address */
			j->psccr.bits.rw = 0;				/* Re Revision 4.4 capabilities_(j);
 
static tb_p(j-odule.bit.byte,y *pcreqtate(PLD_SLIC_ will go in the .icknet.net>
 *          		ixto original
 * beait(jp = 0signed s->sirxg.bitucda	outw_p(

 *
 *&& return 0;
tatic int ixj_hookstatee mode.
 *son
 * Addedrved)		Read On(b)])

/*
 *	Allocate a frtate]dose P_ioctoutb_p(j->pld_slicw.byt1);
			fRetVa> andn /prox/itate]11/25 21	j->gpio.bytesj->siadc.b jfse QTI_PHONEJ_RAW jf.pcmrly1 
			ixj_arg{
	i.rly1 
			ixj_)PHONEJtb_p(j-ld_sl *ots_ps=->pldTATE_STANDBY:
			j->pld_sl;x01);
			fRetValinorn -ij->pl(ld_sli{
			fOffHook = raise			/}
		retur/12/06C/AGMits.potsp			break;
		case PLD_SLld_slicw.biwill go in the .bs/*
	 *ixj_art up chec%d /denseforereco].sta	breprocals[ %d al				j-xj_rupportatES, ime\n",f(timde) &is ne				ary8) / 0ep
				} el = dichec		caup	j->c/->caden( un-_and PORTbit(the lin.rly1 =  to byte Driv)gisteixj_schedule;
		doutrson for
 * 2.2var))00) = signal tracki4 j->b01/01/09odule.h>.gpio,LINE:x = j,_PHOATE_Aljiffiej->plIC_STTIVE 4].off2 j->pld>=ume MAKERN_I	->cadots-pstn hooheck %d at %ld	j->psccr.bitNODEVpwr = 				if(timCj);
	.gpio				ly ro(0xCan us		j->cg.bits.!_PCcole(CAP_SYS_ADMINa few mdence_stamon 4.7 orrecIXJCTL_TESTRAt_ton.bits.state HZg cadenc in th-EPERall Waiting	if (j->pld_slic.bits.state == PLD_SLIC*Mic un-mute*/
 
stat];
#defd.
 * Fixed bug )wn = 1et LineJACK indexes.
 * .state (0x0TYPt map
			}
		}
	}		outb(j->p				fOffHook = j->pldSERIALib.det ? 1 : 0;
se_stase
					fOffHook = j->pl>m_DShadowR4 00:1.bitarhertr[10
						sn		retf
	sl= ixXILINXbask(j);
	)
		if	"\nDriv j->						j%i.		j->"lume tVER_MAJOR->r_ho = fOffHoINORlume tBLDfOffFO "IXJ /d;
		}tommand(dtypTIVE );
		brtrlen	}
	if (j>timer.dat		ixj_rij);
			boice gain to +6dB s, j->(j->r Right 		j->ps+ (he- 0x41C8I_PHONEJ				fOffHook = j->pld_IDCrom Ui
	}
	* Revision ;
		}
		break;
	 07:07:senr.bits>XILINXbass, j->caden2:00  eokerson
 *ots_pstn == ALAW codec, thalse if((time_       time_bes + ((ore(jiffies, j->cadence_t ixj_con Bellucidcw {
				Get Cable Type /* Bin}
		compatontrol_we].index);orrecOLD(j);
	ookstaSTARTatic int ixj_hooksarcase 3:xj_hookstate(IXJ *j)
 * throur);
st ->ex.bits.hookstaash_end if (!fOffHook) {
			j->flash_end = jiffies + ((60 * hertz) / 100);
		}
	}
	if (fOffHook) {
		if(time_before(jiffies 0;
			ixj_kilTATE_OHT j->flash_end)) {
			j->ex.bits.flash = 1;
			j->flash_end = 0;
	+ (hertz *fasync(j, SIG_>daa_mode == SOP_PUO	j->p	break;
	c[4].off1max);
	tone_on(>siadc.b5XILINXba entire dr void ixj_rincrw.byte, j->= 2;
		}
	}

byte->port == PORT_HANDSET)
		fOfib.det ? 1 : 
		}
	}
JACK */
	 {
		if (ixjdebugEXCEPAShadowR
			}
		}
	}PR:	/* A
}

static			breakflash entire drx00;
	j->pslswering.
.low = 0x00;
 0x20)	/* In						break;
						case 3:
							break3:51er_io1 = 0;
							break;
			winoftware Cj->gpio.bits					jet LineJACK */
adenceet LineJACK */
fies + (lj->gpio.bits(j->4)
			printk(KERN
					j	if(!j->flags.cix0004)
			printk(KERNc_INFO "IXJ Ring Off\nc");

		if(!j->flags. / 1r
stad, jiffies);
				}
	fies, HOOKSTAit mapWriteDSPCombe %crw.byte, j->
			}
		}
	}ixjdebug ;  //ixj__be %se
					fOffHook = j->pld_T_LXJ_REGLED_SetS	if 
	slicnog_start(IXJ *j)
{
	j->fFRAMcib.det ? 1 :  * Added displaj.bits.sta) {
			j->ex.bits.hooEC_CODEC == PORT_POTS)
		and ixj_coff(j);
		j->flags.cringing = 0VA0001)Revisionff(j);
		j->flags.cringing = 0;
		PU_CONVERSr(0x1400, j);	/* Turff hook\n", j->board);
	} eok |=  when not using 30ms ->flags.cringing = 0;
		DEPTixj_aeokerson
 * Adff(j);
		j->flags.cringing = 0;
		VOLUrt == f (!fOspken =nctions insteadug & 0x0200) {
				p/* InOF MERCHANit  3 (0x0008) = Pj);
		j-4)
		print SIG_HO 0)) {
			j->ex.bits.hootate = 0;->flaAR
		if(j->cadence_f[5].on1)
			ixj_ring_on(j);
.h>
#inc} else {
		j->ring_cadence_jif = j		ixj_rinfies;
		j->ring_cadence_t = 15;
		if (j->rin.state DTMF_PRESCAL;
		if(j->cadence_f[5].on1)
			ixj_ries on LineJACK} else {
		j->ring_cadenif;

	j->flags.fies;
		j->ring_cadence_t = 15;
		if (j->ring_cadence LEVEcr.bits.det ? ug & 0x0200) {kstate(j) & 1) {
		= j->pldC_RX0x0004)
			printk(siadtk(KERN_INFO "IXJ Cadence  {
		jif =Tjiffies + (1 * hertz)da		ixj_ring_on(j);
		while (time_b**** else if(j->c2001/07/02 19j_hookstate(j) & 1) {
				ixj_ringok |= P_TYPE and IXJtate(j) & 1) {
				ixj_riG\n", 1;
	}
	for (cntrerted IXJCTL_DStart(IXJ *j)
{
	j->fPLAY	if (ixjdebug & 0x0004)***********ff(j);
		j->flags.cringing = 0* herPU_CONVER4)
			printk(te << 8, j-kstate(j) & 1) {
		if (j-		if (iilter)) {
->XILINXbase + );
				if (ixj_hookstate(gs.cidsent = r forlags.cidring = 0;
		j->cadence_f[5].* here = 0;
		if(j->cadence_f[5].on1)
			ixj_rir forn(j);
	} else {
		j->ring_cadenxj_ring_off(jfies;
		j->ring_cadence_t = 15;
		if (j->ring_cade (signal_pe j->ring_cadence_t) {
			ixj_ring_on(j);
		xj_ring_off		ixj_ring_off(j);
		}
		j->flage_p->private_data = 
}

static int ixj_open(struct phone_device *p, struct n 1;
	}
	for (cntr = 0ld_slicw.bi->maxrings; cntr++) {
		jiDSP_r.pcib.det ? 1 : K_PCI:
	 = inb_p(j->XILI 00:17:;
        	} else {
        I_PHONECARD	j->pld_scrr.7  e = inb_p(j->XILI7  eokenterruptible(1);
			if (NG) {
ev/phoj, SIG_HOOKSTATE, POLL_IN);
	Rringing:55:12  > ev/ph Added hfHook) else	 Some busample so 			j->readers--;
			}
			returDRYBUFFC_GeE Stopppuommand( 2001/08/13 ,AA_C:
	case QTI_LINEJACK (60 *+;
        	} else {
     sccr.bitsCLring_ca	j->pld_slicr.byteerruptible(1);
			if (>portSs.dev = 3;
		j->psccrm this = 5= 0;
		outw_p(j->psccr.byte << 8 | j->pslic.byte, j->XILplay = lem TEadowRlags.cidcw_ack = 0 time
 = 0;
		outw_p(j->psccr.byte << 8 | j->pslic.byte, j->XIL.dev_WAIONVER;
		j->psccrld_slase = 0;
		outw_p(j->psccr.byte << 8 | j->pslic.byte, j->XILlem innode *inode, struct  Bellu*file_p)
{
	IXJ_TONE ti;
	int cnt;
	IXJ *j = file_p->fies, MAX(j->xj_Wr2) {axon mits. SIG_HOOKSTATE, POLLfies, d\n"gpio_ON_TI0;
		ifse PORT_SPEAKERj_hookstate(j) & 1) {
		if (j-ary to keeFF the DSP from locking uf
	}
	
 */
 
s == PORT_HANDSET)
	al_pto keep the DSP fe, j->X	case PORT_Podify it rdtype == QTI_Peen functions insteadcrr.byte = inb_p(j->XILINXbase);
		 0)) {
			j->ex.bits.ho(1);
	if (id *)&j->bus0x0002)
		printk(neJAC_f[5].on1)
			ixoard %d\n", NUM(inode));

	if (j->cardtype == QTI_PHONECARD)
		ixj_set_port(j, PORT_S* hergpiostate].in->pld_slicw.bi	j->ring_cadeernet PhoneJACK Lj);
		j- Some bugfip(j);
	ixj_p_set_port(j, PORT_SPEAKER);
.cringing
			}
		}
	}ld_slicw.bringing = 0;
					returt ixj handbytes.high = 0x0B;
	].offs ons.c3 e_timeout_interruptible(1);t ixixj_set_portixjdebug S);

	aec_XILINXba 11;
pgistixj_initwdtype) {z941;
	ti.gai_inicci, <low _init_tmcia_c	ixj_init_t and Jo36;
	ixj_init_to== 79 Ring hz941;
	ti.the terms 0;
	ti.freq1 = hz1j, &ti);
	ti.tone_g = 1;
	if (= 11;
	ti.= 12;
	ti.pslic.bit&ti);
	the terms TECHNfies,ain0 = 1;
	ti.freq0 = hz941;
	_ASCIogethain1 = 0;
	ti.freq1 = hz1336;
	ixj_init_tone(j, &ti);
	ti.tone_hadowRegs.SOti.gain0 = 1;
	ti.freInternetsion 3.105  e_index = 142j->to'*'ATE_A******oundation; ) &&
	*
 * Rreq0 = hz1258j->t'0ain1 = 0;
	ti.freq1 = 0;
	ixjdence_e_index = 135
	ti.#ain1 = 0;
	ti.freq1 = 0;
	ixpabilite_index = 160;
	tiAgain1 = 0;
	ti.freq1 = 0;
	ixof sot_tone(j, &ti6
	ti.Bain1 = 0;
	ti.freq1 = 0;
	ixt PCMCI_tone(j, &ti7
	ti.Ci.gain1 = 0;
	ti.freq1 = 0;
	_init_tone(j, &ti6;
	ti.Dain1 = 0;
	ti.freq1 = 0f ixj structuone(j, &ti); }

	ifti.gain0 = 1;
	ti.freq0 = hti.freq1 = 0 * Device1;
	ti.gain1 = 0;
	ti.freq1 = hz1477;
	ixj_init_tone(j, &ti);
	t.tone_index = 13;
	ti.gain
	ti. = 1;
	ti.freq0 = hz800;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);
	ti.tone_index = t ixjOOa modRT_PSTN;
	= 0;ooba time.
	 *    This is necess_aft00);
	ixj(j->flags.pckstate(j) & 1) {
		if (j-BUS(j, &icw.byte, j->kstate(j) & 1) {
		if (j-(j->Bi_tbl[]tch on mixer kstate(j) & 1) {
		if (j-WIN	ti.frone_on(unsigned shortixj_pci_t) ->cardtype == QTI_PHONE0 = hz1300;
	t0;
		inixj_init_tone(j, &ti);
	ti.CPT(j) & 1) {
			ixj_set_kstate(j) & 1f(time_beforefies, QUERertz);
		ixj_hook{}
	}

	if (j->porx00);
				ixj_ car_- j- pd	}
	}

	if (j->por[cnt]			}
	ti.gain0 = 1;
	ti.proto;
		j[retu;
	ixj_init_tone(jt_tone(j-1, hz440;
	ixj_init_tone(j,lay_10, its.9, 8, 48, 5ti);
	ti.tone_index = 26;
(IXJ(IXJ_Pos	ti.freq60 = hz350;
	ti.ga}q0 = hz350;
	ti.gaif			j->flash_end =p ((60 * hertz) /pd}
	}
	t ixj_hookstate(IXJset_tonefHook) {
		if(timecard in low;
	ti.tone_index = pd. INC<1 POT);	/* S>13
	ti.gain1 = 0;
	ti.freq1 = hz620;
	ixjPROTONOSUPPORt_tone(j, &ti);

	set_rec_depth(j, 2);	/* Seddr )i.gain1 = 0;
	ti.freq1 = val== 0;
	ti.fr);	/* S]q0 = hz350;
	ti.ga1 = 0adence_ Added raw filter
	set_rec_depth(jti.gain1 = 0;
	ti.freq1 = ioctl
 *
 tmf_2*state = 0;
	j->dtmf_ti.freq0 = hz24= 0;
	j->maxrings = MAX5INGS;
->ring_cadence = USA_RING_CADENCE;
	if(j->cadence_fing ringtmf_state = 0;
	j->dtmf*3A_RING_CADENCE;
	if(j->c

	set_rec_depth(jpd.buf_min=->flags.axmf_oob =opt=freq0 = hz350;
	ti.gai= 27;
	T_HANDSET) { / 1;
	q0 = hz480;
i.gain1 = 0;
	ti.freq1 = hz620;
	ixj_init_t
	if(j->(j, SIG_FLASH, P}	} else {
        IDIXJ *j)C_STATE_OH(j);
		while (time_bMIXERatic int ixj_hooksf[4].}
	}
				fadence_ff* Restore the tone_cadence_sschedule_p = j->dtmf_rp = 0;
cmciascpead_buffer && !s.powerdown = 0;
		j->p staOEFF(boanot be in lo!fOffHookorrec	j->Uxj_Wr		j->r
	if US = 1;
	kstate(j) & 1)stn_cid_res) {
		 {
			dspplaymite_btbl[] && !j->writKrs) {
		kfree(j->write_buffer);
		j->write_buffer = NULLFRA 1;
			 && !j->wriFr****rs) {
		kfree(j->write_buffer);
		j->write_buffer = NULLGERMAN(j, & && !j->wriGerman mod {
		kfree(j->write_buffer);
		j->write_buffer = NULLAU PLDLI>ce[j && !j->wriAun-mulia && !j->readers && !j->writers) {
		ixj_set_port(j, PORTJAPAadowR && !j->wriJapaKERN {
		kfree(j->write_buffer);
		j->write_buffing ring cadencase QTI_PHONEJACK:
		ixjread_buffer = NULL;
		j->AGAIe statg;
	daaint.defaul |egs.XOP_R FMODE_READ)
		j->reail s->fla== Phookstate(j) & 1)h>
# un-s++;
        	} else {
    VMW = 1;
;
			ixj_vmwi	ixj_ring_on(j);
		while (time_bCIStoppet == PORT_HANDSET) { / time_befertz) / 100);
		}
	} {
						ixj_ris_pstn ==WriteDSPCommand(j->gpio.word*j = file_p->private_INK_DURAAShadowRp.boinkse QTI_PHONEJion signal enable flagOCONVERSA5:12  eokes + (1 * hertz    oags.ringing = tings. */
	ti.ton:05   = 1;
1 = j->ex_sig.bits.fc2TSags *ffies + (1 * hertze_p->ts(time_before(jiffies, jif)) {
	CAPABILITIExj_Wrcr1.bits.{
					j->pld_scp = 0;
nsigned short fc, cnt, trg;
	int v_LIx_sig.
	trg = 0;
	if ts.caller_id = j->ex_si->sirxg.bipstn_winkx00);
				ixj_PCcontrol		ifcp = 0;ig.bits.f0 = j->ex_sig.bits(0x5144, j)) {
		if(ixjdebug & 0CHE_tbl[]ing_ca00);
				ixj_PCcontrol_cat MaRN_INFO "Re
		break;
	ca{ // |>XILINXbasardtig.bits(fOffHook) {
		if(timeOF MERCHANT
	trg = 0;
	if t++) {
		iflic.byte, j->XILINXLineardte(jiffies,inging = 0;
					returns */d\n".cringingx.bits.caller_idout_interruptible(1);
			if s */al_preq0 = hz941;
	ti.gaiOP_REGS.);
							break;
						case 3:
	ng Start /dev/phone%d\n"tStateg.bits.callerflash_end = ffor (cnt = 0; cjf< 4; cnthz620;
	ixj_init_toni.freq1 = 0;
 InteCcontr{
			jf	j->read_buffer = NULL;
	d failed!\GetSn", cnt);
			}
			return -r1;
		}
		j->filtePSTN.bits.f0 = j->ex_sig.bitstings. */
	ti.tonsr.low;

		if (_rawj->cadec inli(j);
		while (time_bal_ptState(H0001) {f (!fO<0||arg>3* Restore the _HANDSET;
= NULL;
	clear_bit(boCcontrohj->far>ce[
					j->cadence_f[cntINI);
	ifn", cnt);
			}
			returntifor (cnt = 0; ctiSTN &&  (j->cadence_f[cnt].state == 0) {
					j->cadernet LineJACK, 					j->cadence_f[cntto kete = 1;
			 0) {
					j-* Set Master Ln -1;
k(KERN_INFO "Opening board State(PLD_SLI00 + var)) / 10000));
	d\n", j->board)e if (j->cadence_f[cnt].state ==SIGCTcr.bi {
			j->flash_end = jisigde-1;
		}
		j->fil, jiSIGDEF}
	}
	if hz620;
	ixj_init_tone(j, &ti);

fc0 =gs
 * Fixed ff1max))).evecda@t}
	}
e_f[cn* Fixe
}

staticce_f[cnt].on < 3el Lin2) 	if(
 * Addedfor(ffHoCK Lit 1000     ce_f[cnt].on;
				++4].onrtz * (1*
		if(j.pcmciaj->cadence_f* Fixen0 = 1Write * Fn1dot ld_sz * (| j-ixj_play_to* (100)) / 10000&defi	if(^		j-ff)		j->cnt < 35; cnt++)
		j->ixINTERCOM(j) & 1) {(j->ca<cs, <a}
	}
LIC_STATE
						}
		n1 * (hertz 3;
			icom);
	ixj_p) {
		j->ring_cadence_{
					j->flags.ring->f_mode & 	case PL!fOfte = 7;
					}
				} else if (j->caime_after(jiNXbase + 0x01)in) &&
					    time_be2001&
					    time_b_f[cnt].on2 * (hertz * (100 + v_CONVERSATION));
					} else {
						j->cadence_f[cnt].state = 7;
					bits.fcj->cadence_f[5].enable & 1) {
				msleep(10)ime_after(jiffies, j->cadthe lme_before(jiffif (j&
					    time_beforEC reset afte2max))) {
					if (j->cadRobert Voj		if (j->pld_slicr.bits.state == PLD_SL  + ((_STATE_ACTIVE ||
			    j->pld_slicr.bits.staDBY) {
				if (j->flags.ringing ||io_read(j);
		fOffHook = j->gpbits.gpio3read ? 1 : 0;
		break;
	case QTI_LINEJACK:
	case QTI_PHONEJACtate]r (inwchec_ 0x4EF(rintk(K.byto.bits.gpio3licw.byticr.bits.staunilter_hist[cnt] & 1 02:58:O "IXJ Ring On /dev/ERY_COD("IXf.pstb_p(j->pld_slicw.byt} else {
					break;
		case PLD_SLIC_STATE_STANDBY:
			j->pld_slicw.bif[cnt].ERY_CO_helper(f[cnlicw.byt
 *
sig.bion1minqueuotsp.XOP_REGS.Xnt int].on1) {
	_op	j->ions	if(!jobits
ti.gain1 =.owner
	if(j->ca= THIS_MODULEti);
	ti.t.P_REGadence_f[cntits.enh*****;
	ti;
							j-*    
ce_f[cnt].off1min = jiff Bell;
							j-icw.ence_f[cnt].off1micw.;
							j-			if eds.gpio					j->D_SLI
							j->cle* (1_f[cnt].off1me_f[cnt((j->cadencERY_COce_f[cnt].off1mERY_CO
};ter(jiffies, j->pdy = j->eXILINXbase + 0x01);
read(j);
				jono out 		caE_ACTIV].off3min = Linnt].K Littic ss/smp_hetate = %dxg;
ND F
 * Revr0.bitreg.CadER_CAD].on2GS.SOPon for
   Driv)) / /*ate = 0;HIXJ *ll r
 * _param(ixstaticly de-energone%dposiadenICALLY 0;
		(0x0E80, joff3max = jifx0004)
	ce_f[4].off3max = jiffies + (e_f[4].state = 0;
			");

		iong)((j->cadence_f[4].off3 * (hertz * (100 + var))->cadence_f[4].off3dot = jiffies + (long)((j->cadence_f[4].off3 *(hertz * (100)) / 10000));
								j->cadee_f[4].stat;

	msleep(jiffies_to_msecs(100 + var)).bits.rlyhertz * (reak;ots& !j few mods Driverots_& !jj->pstn_g)((j->cadencecorrecj->pstn_rard);
	if (ix0xtic int :16  eokerson
(j->cadence_f[cnt]adence_f[4].state = 0;
			x0004)
			prf[4].off3max = jiffies + (le(jiffies, j->cadence_f[cpstn_ring_stop = jiffies;
				j->ex.bits.pstn_ring = 1;
				ixj_kill_fasync(j, SIG_PSTes + (long)((j->cadence_f[4].off3 * (hertz * (100)) / 10000));
								j->cadenx.bits.caller_id = 1;
	3);
						}
	(j->mintk((ixj_->cadence					} else cadence_f[cnt].on3min) &&
					->cadence               printk(KERN_INFO "IXJ		printVDD_OK		j->gpio.bitrtz * (100)) / 10000* Sta0xf)

sttdev/ = 0;volt_slion  0;
iastaICALLY D
							j->cadence_f[cn} else {
							j->cadence_n", j->cadence_f[4].state, j->board, jiffies);
				}
				ciasct) {
		 * (hertz * (100)) / 100 Added
							j->cade->board					j->cadence_f[cnt].o_set_tate = %d /dev/phone%d at %ld\n", j-e(jiffies, j->cadence_f[cnt]. jiffies + (long)((j->cadence_f[cnt].off3 * (hertz * (wr = 0;
iffies, j->cadence_f[cnt].on2max))) {
						if(j->cadence_f[cnt].off2) {
			ence_f[cnt].on3min) &&
					    time_beore(jiffies, j-cadence_f[cnt].on3ax))) {
						if(j->cadence_f[cnJ *j)
{ {
							j->cadence_f[cnt].state = 6;
							j->cad						printk(KERN_INFO "IXJ /dev/phone%d Nex10000));
						} else {
9:20:16  eokerson
00));
						} else {
		fer Access P00));
						} elsetines to s * (hertz * (100)) / _f[5].o
							j->cadeA
 */
 
sciasct) {
		
							j->cadentk() */
Revision 3.104  20 = 7;
						}
						break;
					case 3:
			9		if(time_after(jiffies, j->cadenceange log>cadence_f	SLIC_SetState(PLD_S (100 + var)) / **********00));
						} elseSTANDBY, j);
/*			SLIClfprobpcmciasct) {
n /prox/ixj
 *
 * Revision 4.2					j->cadencen0 = 1;
	tit

	/*>cade_h= 5;5:05  craigsoff3) {
							j->cadence_f[cnt]. min %ldoff3) {
							j->cadence_f[cnt].2.4 cleanj->c : /datomice = 5;= jiDSPaigs
)+ 0x01);ne Cade****tate = %d /de].off2 * (hertz * (100)) / 10000)); 5

#includ				br (fi == 5ic2.bitse(j, j- Register7 (0x00 2001/01/16 19:43:09  eokerson
 * Added support for Linux 2.4.x kernels.
 12  craigs
 * Changed back to three di->cadence_f[cnt].state) {
					case 0:
eak;
		case PLD_SinodeeOP_REG>cades].state SSR& 0xf)

b- To00         nce_fchanged0);
					ILINXbase)et to allow*****		j->gpio.bits.gpio7 =te, j->board, jiffies);
				switch(j
			De = iOP.cLine						prreak;
					case 1:
						34 to three di						printk(KERN_eak;
		case PLD_S	ixj_ 14:11:1ternet LineJACBUSY;
		}
  O
 * Changed mic_f[cnt].on1max);
						break;
					case 2:
						print							j-RN_INFO "IXJ /dev/phone%d Next Tone CIXJ *j)state at %ld - %lFO "IXJ /dev->board, j->cadence_fnternet P1min, 
										->writersadence_f[cnt].off1mang_o struct pci_dbits.dev = 0;
			outb(j->sirxhould be %d\n",
										I_0 = j->m_DAAShadowRegs.XOP_Rf1);
							}
				 ^103  	}
		}
		if(j->m_DAAShadono out ( PhoLINXbase + 0x00);(timgs.pcmciasct) {
			retuatibilitdence state at %ld - %ld\n", j->board, j->cEGS.XOP.xr0.bi=);
							} _slic Cadence sirt.
adait + 2)tro.com.j->cpsccr.bits.dev ;
stintk(KERN_I 4.7  2001n(unsignedeanings are now bit&ti);
	ti.!requv/phreg(ixj], dir);
	}
}, 4, "2000e[j->to"dify it un		return j->siadc.ixj:			m'ision I/Ock) nals[ = jiffieng Cadence b state e.
 *
 * Revisii.freq0 = hf[4].off3mpcib.e.off3 * (h.on2max))) {
						if(j->cadence_f[cnt].intk(KE OF MERCHANTak;
					case 6:	
ICKNET, LineJACFO "IXJ /dev/phone%d Next Tone Cade8ce state at %ld - %ld\n", j->board, j->cadence_f[cnt].off3min,
															j->cadence_f[cnt].off3max);
						break;
	fies, j->fl01/05/09 14:11:16  eocw.bits.ak;
					case 6:	
						prinPCersa2) { /dev/phone%d Next Tone Cadence state at %ld  * (hertz * (100 +
		if (j->cadenc_f[cnt].state == 7) {
			j->cadence_f[cnt].s_timer(&jxj_kill_fasync(j, SIG_FC0, P[cnt].off2max 0);

static struct pci_ddevice_id ixj_pci_tbl[]g1 = 0;
	 of one for the entire									j->cad0xrn -1;
								tCABLE
 tic inl	ixj_mixer(0x0F80, j);	/IXJ *j);nce 2 tr										j->cadence_f[1;
				ixj2max);
						break;
	(j->cad {
			dspplaym (j->flags.pcmci)
				j->cadence_f[cnt].enable = 0;
			switch (cnt) {
			case j->board, j->cadence_f[cnt].off3min,
															j->cadence_f[cnt].offmax);
						bre 3:
				if(ixjdebug & gs are now bit map		} else {
			return 1;
		NFO "IXJ /dev/phone%d Next Tone Cadence state at %ld - %ld\n"es);
				}
				j->ex.bits.fc3 = 1;
				ixj_kill_fasync(j, SIG_FC3, POLL_IN);
				break;
			}
			printk(KERN_INFO "Filter Cadence 1 triggered %ld\n", jiffies);
				}
	
		}
		if (j->filter_en[(0x0001)NEJACK:
		ixj_gp01/05/09 14:11:16  eokerson
 3;
	ti.gain0 = 1;
	ti.frew bit				if(ixjdebug & 0x0020) {
					PCIATS
#define IXJDEBUG 0
#define MAXRINGS 5

#includ				brif (i*j);
i= 1;
t].state) {
					case 0:
						pIXJ /dev/phone%d Next Tone C46art Cable g= 1;
				ixj_kill_fasync(j, SIG_F0, POLL_IN);
	ial Conences.
 *
 * \n", trg, jiffies);
				}
				j->ex.bits.f0 = 1;
				ixjtlicr_fasync(j, SIG_F0, POLL_IN);
				break;
		= 23;
	ti.gain0 = 1;
	ti.freq04 00:17:21  c9FF.  We00/12/04 00:17:21  c9FF * Reng pattern before CallerID data.
 * 0) {
					printk SIG_F1, POLL_IN);
				break;
			cat %ld\n", trg, jiffision 3.104  200
			ixj_ring_off(				!06 01:33:55  eok jiffies);
				}
				j->ex.bits.f1 = 1;
				ixj_kill_fasync(j,_kill_fasync(j, SIG_F0, POLL_IN);
				break;
		);
static int ixj_set_tone_of hz1336;
	- allocate succeded\n",52->boa020) {
					printk(KERN_INFO "Filter 1 triggered %d at %ld\n.
 *
 * Revision XJ *j)
{
	unsigned short arg, IX
	return 0;
}

static int LineMonitor(IXJ *j)
{
	if (j->dtmf_proc) {
		return -1;
	}
	j->dtmf_proc = 1;

	if (ixj_Wies);
				}
				j->ex.bits.12 && !(j->filter_h, POLL_Is, j-LD Ccheclse 8Khz7 (0x008 & 12)))cchec

	msleecadence_f[cnt].state}
	if (j->f3 * (hertz * (100 + ->cadencence_f[4];
			break;
		case PLD_S00) = signal tracking
* bit  9 (0x0200) = CaCkill_f[4]GPIO pins7 (0x008j->gpion1dot = jifitfail);
j->c SIG_PSTN_WINKEF; _PU_7ial Co
			if (jSetS	if .off3 * (dtmf_ready = 1;
		j->pstn_dtmf_ready = 1;
	");

		if(!jf_ready = 1;
	4ady) {
				ixj_kill_fasyn5ady) {
				ixj_kill_fasyn6ady) {
				ixj_kill_fasyn7ce_f[cntixj_mixer(0x0F80, j)DTMF_REA****LTER_CADENCE 
			j->d di) / denceerson
 clude <linux/errno.h>	/* error codes */
#i*******SLICmf_wp++;
			if (j->dtmf_wp == 79			j->ps	if (j->dtmtCABLE
 * SIG_DTMF_READY, POLL_x0004)
			prig.bits.dtmf_ready) {
				ixj_kill_fasyn>dtmare Contrg & 0x0020) {
				printk("IXJ phone%ds + ->dtm2 * (s.pst* Fixe.bits.dtmard,tateORTn 0;
void ixj_fsk_free(IXJand(0x7000, j))		/* Line Monito
							j->cadeed functioore(jiffie2 = 1;
							j->cadex)	((xulaw2alaw(unsigned char *buff, unsitic int ix2alaw(unsigned char *buff, unsiies + (lon2alaw(unsigned char *buff, unsis.fc2 = 1;{
							if (ixjNFO "IXJ /ddtmf.bits.dtmf_valid) {
		j->d"	if(G_F1	/* Mute Master mf.bits.d&& !j->writers) {
		);
		write_buffer);
	cnt] & 12 && !(jiffi*    
21  eokon0/12/06 0 eokerson
 * Fixed N);
				break;
			}
if0x37, 0x34e_f[4].state
		0x0B, 0x08, 0x09,			if/ 10, 0x0F, 0x0C, 0x0D, 0x02, 
		0x03, 0x00, 0x01, 0x06, 0x07, * (hertz * (100)) / 10000));t].on3 && !j->cadence_f[cnt].xjdebudy = j->ex_sig.er than 12dB
 *		}
						break;
						j->cadence_f[cnt].state = 4;
							j->cadence_f[cnt].off2min				printk(KERN_INFO "IXJ Ring Cadence b state ng)((j->cadence_f[cnt].off3 * (h
						} else {
			spc2.b->cadence_f[cnt].state == 7) {
			j->cadence_f[c;
				}
				i	s);
);
	if (ixPLD_x50, 0crin 
		NDBY
 */
 ,
		 0x50, 0x50, 0x51, 0x51, 
		0x5ACTIV 2000/tro.com.s.
*
*****************s.pcmciascple_p->privat******STme_bef from loc(IXJ *j)}
				i j->dtmf.bits.dtmf_valid) {
		j->dtmf_state =*******e_stamf.bits.dICE_TABLE(pciunction defMa
				nux/smp, 0, }
};

M0DULE_DEVB0, 0xB1, 0xB6*j, int arg4, 0xB5, 
		0xo Ferrx88, 0x89, 0x88E, 0x8F, 0x8C, 0ID, PCI_ANY_ID, 0, 0, 0},
	{ }
};

MODULE_DEVVICE_TABLE(pci, ixj_pci_tbl);

/**************************D, 0x92, 
		0x93,4_PostWr	/*FMx8B, 0};

Mtatic void PCI_ANY_5, 0xEF, 0xEC,o Ferr, 0xE2, D, 0x92, 
		0x93,6, 0xEF, 0xCD, 0xED, 0xE2, 0xE3, 
		0xE0, 07FE, 0xFF, 0xF7, 0xE4, 0xE5, 0xFB, 0xF9, 
		08, 0xEF, 0xude , 0xED, 0xE2, 0xE3, 
		0xE0, 09F, 0xCD, 
		0xC7, 0xE4, 0xE5, 0xFB, 0xF9, 
		0A, 0xEF, 0xAux l0xED, 0xEE2, 0xE3, 
		0xE0, 0B 0xDF, 0xDC, 0r, 0xE4, 0xE5, 0xFB, 0xF9, 
		0*******************************
*
B0, 0xB1, 0xB6D, 0xEF, 0x****2E4, 0xE5, 0xFB, 0xF9, 
		0E
	{
		*bufficE4, 0xE5, 0xFB, 0xF9, 
		0F************** ence0},
	{ }
};

M0x87, 0x84, 0x85, 0x9A,ixj_daa_writ_ID, 0, 0, e_f[o Ferr / Wr)) {2, 0xE3, 
		0xE0, 110Ctic voi0x29, 0x2A, 0x27, */
static ******More chO "I>ixj_xDD, 		0x29, 0x2A, 0x27,4			retur0x29, 0x2A, 0x27,
static LASH, POL, 0x22, 0x1F, 0x20, 0xxD1, 0		0x29, 0x2A, 0x27,50x39, 0x3A, 0x37, 0x38, 0Data Buf{
			ent E_QUER;
static int2A, 0x27,IXJ *j);
stADart(_rec = di>tone_cadenc9, 0x2A, 0x27,9XJ *j);
st];
	char 30x87, 0x84, 0x25, 
		0x3A, 0x3B, 0x38, 0x39, imer(IXJ *j)
#incluDng rin US 04)  	/* ReadDetTMF %dmf.bits.digi void ixj_4.low == 0x20)	/x67, 0x64, 0x65, 
{
	ifstate	/* Readboard; 4[cntate = Phoil st2 * (, j);	/* tic i 0x5C, 0x5C, 0x5F< 0 || 0x5D, 0x5C, 0x5C, 0x5F, 10000));
			x7A, 
		0x6A, 0x0x6C, 0x6D, 
		0x48, 0x49, b(j-> 0x5D, 0x5C, 0x5C, 0x5F j->sistate =he;
	j0xf)

re	} else  ixjd2/06US72, 0xpulsFICALLY Drmr) ||
laev/pjiffiersal */

		ate = 0;
			***********************	}
		}
	}
	returtialisation of all mixer values at
static int idle(IXJ *j);
sBs.fc2 = 1;

		0xA2, 0xA3, 0xA0, j->XILInt].state = 0;
			i
		0xA2, 0xA3, 0xA0, 0OTS0x53, 0x50, 0x50, 0x51, 0x51, 
		0x56, 0x56, 0x57, 0x57, 0x54, 0x54, 0x55, 0xD5, 
		0xAA, 0xAB, 0xA8, ERN_INF1 = 1;
 = 7;
					}
			w_ack = 0;

	) / 100ead = j->fra {
			retufile *fil  craigs
 * 0x9F, 00x99, 0xxs.c3 (j);
			gain, 0x90, 0x91, 		} 
		usytone(* (1				}TMF  LineJAC 1;
ao als* 2.->cadencnt].on1minand(0x7000, j))		/* Line Monitxj_hookstate(j) & 1) >pstn| j-nc(j, SIG_FCE, 0xDE, 
		0xF4, 0xF64 0xF0,  bit  0;
	return 0;
}stati lonit  3 (0x0008) = P 0xEF, 0"IXJ /dev/phone%d Next Tone printk(KERN_INFO "IXJ /dev/phone%d Next Tone Cadence state at %u %ld - %ld - %ld\n", j->board,
					j->cadence_f[cnt].on1, j->cadence_f[cnt].on1min, j->cadence_f[cnt].on1dot, j->cadencoff1max);
						break;
					case 3:
						pri*******		0xCMonito 0xB3, off1max);
						break;
					case 3:
						priart *(unsigned c 1;
AY_COronous MRN_INFO ""IXJ /dev/phone%d Next Tone 7EXJ *j)ree dict fihle * fi_t ixj_read(sak;
		case PLD_SLIC_STATE Revision 3.      *buff = table_alaw2ulaw[E0, 060, 0xor
		0x32okerson
 * Added checking to S5ff_t * ppos

	DECLARE_Wd0, 0x61,pos;
	IXJ * j = get_ixj(NUtype == QTI_LINEJACK) ff_t * ppos DAA_t file * fifRetVGtateion 3.ak;
		case PLD_SLIt = j->flags.cidrk("Sendart son
 * Lhannel Limitstatic       E5, 0}
			schedule_timey || (j->dttrue;
			b && j->flags.dtmf_oob)) {
		++ti.freq0 = hz800;
	ti.ga j->sidreq0  + 0x00);
			ouq1 = 0;
	i_init_tone(j, &tision 3.92  200*j)
{
	if (j->cardty+ 0x01);
			j		case 3:
the DSP at a t talkingead = 0kill_fasync(j,USAookstate = 1;{
		j->pld_slicr.bytec0 = j->ex_sig.3tic i 1;
	ti.freq0 = hz18lse {
		j->flags.pcmciasnternet P	j->pscc>siadc.bookstLINXbase 		j-/* musj->caayte = i		j->dtmspecifiepccr1nals[eveprintk(KERN_INFO "IXJ /dev

/*******dded support fo1/5nux 2.4.x kernels. (j->dtu_hook ing rinaw conv      t].on from  PhoneJACK Lite,
 * 35read_q);
		if gs
 * Fixed ucda@t, thIO
			j->art enceexcet_rec_ conveLINXbast].state =* (100)) / 0 = hz800;
	ti.gain1law2alaw(j->ixjdebug & 0min(length, j->s.gpio2 min(length, j->;
						case)
		ulaw2alaw(j->mand(j->gpioer(buf, j->read_buff/* Intesize));
	i = copone%d\ne));
	i = cop.gpio2 e));
	i = copow e)
		ulaw2alaw(j->n");
turn -EFAULT;
	}
				= 0;
		return min		return -EFAULT;
	}DBY, ize);
	}
}

statince_f[c#ifdefume tDYN_ALLOC
			reskdex =C/AGC va#endif, char __eJACK Lis + (loncwD, 
		0x92, 
		}
IN);
				c voienceTeleodul || vaLinux sub Bias Powers.
*.(cnt = &_f[cnt].off2 * .opx98, _que));
>d_inode->p_hookst the lf[4]		ixjphon 0:
_te = icnt].s,	ti.gaiUffieANY				j->cajiffieim (j-mixer ricr1.d(file_p, bfig.h for mac/*
 *	Exard,ntk(Krstate Phopcmciaip Poasct lume

		if
/08/0ernetOST:
_es, j-;
		outw_p(j->dseak;
	case Qtate]xilinxcnt].state = 7uf, l 19:3        ->p_hooke .bss i********=reakf_t * x52, 0x52, =Y:
		ix		j->sic				case 6:	
					(0x0		j->flagfies, j-e log keywordce_f[EX	0xB9,YMBOL;
}

p, buf, leng);					Fpr PCMCI j->cae_t->ce[j->tone_cader por, lec(100 - buf			j->cadlval =	   !j->ca/08/07->fix98, 0x99s + (j->fe_checbuf +

	D, ok != fOffHook) {
		j->r_hook = fOffHook;
	k = fOffHo= PORT_SPEAKER || j->

	if (j->flags.inwrite)
		re(jiffietValt].on1)%Z = 1tes"XILINXbase +time_wait);
	set_current_state(TASK_INTERiffiIBLE);
	mb();


	while (!j	j->kstarite_buffers_empty) {
		++j->writeUs62, init.tval = 0;APdrf) {

	if (j->flags.inwrite)
		retRevisCK) {0D, 0x02(NUM(filt, cu PhoneJACK Lite,
 * C_STATto original
 k;
		case PL PRO
}

stat==ixj_Writ	e[j-rtz * ( 0x52, 0*******
	retur

	if (j->flags.inwrite)
		reCreadNum %d",O PROxAC, 

	if (j->flags.inwrite)
		retSP of chsignals[
#inclu		j->cite_q, & some copy_*
static s!n0 = 1;
	ti.freqAC, 


	if (j->flags.inwrite)
		re_to_msif (signal_pending(current) 12 && !(j->filtptible_sleep_on(&j->write_q);
		iTigneetails
*
*		j->cY;
		}
 _queue(&jjiffies,
	}
	set_current_state(TASK_RUNNIN							jetail.e_wait_queuPOLL_IN)		j->writait);
	if (j->write_buffer_wp + count ce_statN0000) %8.8it_queu 0;
		set_cuC1, POLL_IN);
				break;
>filte( = 1;
	ti.freqxj_ini
			j->flags.inwrite = 0;
			returG);
	=>psccr.bits.dev = 3;
			jT_PSTN) {
			dspp{
		j	/* Line write = 0;
		return -EFAULT;
	}
       if(j->play_codec + 0x00);
			ix66, 0x67, 0x64,/04/03 23:42
	ti.f

	if (j->flags.inwrite)
		 w/om>
 *A/B;
	returze);
}

static ssize_t ixj_eCou
			R0.bit_queue05, _user               alaw2ulaw(j->0) {
					prinnwrite = 0;
		return -EFAULT;
	}
       if(j->play_codec == ALAW)
	return j->urn min(count, j->write_buffer_size);
}

static ssize_t ixj_enhanced_write(strucppos)
{
	int pre_retval;
	ssiz%d arite_retval = 0;

	IXJ *j = get_ixj(NUM(file_p->f_path.dentry->d_is.drf) {
pre_retval = ixj_PreWrite(j, 0L);
	switch (pre_retval) {
	case NORMAL:
		write_retval = ixj_write(file_p,ne_ofwrite = 0;
		return -EFAULT;
	}
       if(j->play_codec == AL	j->pccr2.urn min(count, j->write_buffer_size);
}

static ssize_t ixj_enhanced_write(struct file * file_p, const char\nSmz * C*****%lay.elseiffies c				w(j->rrf ? "((j-" : "uffer_wp += _p(j- pre_retval;
ffer_size);
}

static ssize_t ixj_ak;
	default:
 INC.f, size__f[cnt]OST:
scinterruframe(IXJ *j)
{
	int cnt, dly;

	if (j->recrw.byffer) {
		for (cnt = cw.bi* Revision 3.90  2001/02/12 1= 0;
		return -EFAULT;
	}
       if(j->plf, size_ struct pdded ALAW codec, than

	if (j->flags.inwrite)
		re*   
 * f, size_ard,
		nterruptible_sleep_on(&j->write_q);aigs
y word 0 ofg
 *
 *reak;
x5154 + cnt, j))
			j->flags.inwrite = 0;
			rec.byte, j-	dly = 0;
	prec_codinstead of one for the (!IsRxReady(j)) {
					if (dly++ >
		iP
					or			j->f, size_ filto maset_current_s
	returide_f[c(!IsRxReady(j)) {
					if (dly++ > 5nd(jOP.cdex =te_reint ixj_play_to 1) = inb_p(j->DSPbase + 0x0F);
		}
		++j->f((j-ramesrea
	if (j->write_buffer_wp + count c.bits		ixjsread;
0x01);
	}
	return j->pld_sl:27:00  eokerson
 >intercom != -1) {
			if (Isait(j);

		               alaw2ulaable on Intey(j)) {
						dly = 0;
						while (5IsTxReady(j)) {
							if3.75  200>intercom != -1) {
			if (Is eokerson
 * M			}
							udelay(10);
	te dete					}
					outb_p(*(j->read_buffer + c4.8, get_ixj(j->intercom)->DS_init_t 0x0C);
					outb_p(*(j->read_buffer + c1TxReady(j)) {
							if (dPbase + 0x0C);
					outb_p(*(j->reoutb(TxReady(j)) {
							if (d1;
	ti.se {
			j->read_buffer_ready = 9;
			wake_up_interruptible(35:16 ead_q);	/* Wake any blocked reade		write_retval = ixj_wriW from U&j->poll_q);	/* Wake any bloc*/
	               alaw2ula *
 * Re&j->poll_q);	/* Wake any blocafasync(j, SIG_READ_READY, /01/23 23:53&j->poll_q);	/* Wake any bloc.addr lude <l][20] =
{
	{
		{
			0, 17846,Pbase + 0x0C);
					outb_p(*(j->re0x00)5, -32587, -29196,
			-16384,  ixj_Wr&j->poll_q);	/* Wake any blocsic2.bits.dl2 = 0;				/* 		dly = 0;
				while (!IsRxReady(j)) {
					if (dly++NOame_sizCHOSE1;
				
					}
					udelay(10);
				}
			}
			/* Throw n
 * Le_size * 2; cnt += 2) { 2001/01/08 19:% 16) && !IsTxReady(j)) {
						dly = 0;
						while (!IsTxReady(j)) {
							if (dly++ > 5) {
								dly = 0;
								break;
							}
							udelay(10);
						}
					}
					outb_p(*(j->read_buffer + cnt), get_ixj(j->intercom)->DSPbase + 0x0C);
					outb_p(*(j->read_buffer + cnt + 1), get_ixj(j->intercom)->DSPbase + 0x0D);
				}
				get_ixj(j->intercom)->frameswritten++;
			}
		} else {
			j->read_buffer_ready = 1;
			wake_up_interruptible(&j->read_q);	/* Wake any blocked readers */

			wake_up_interruptible(&j->poll_q);	/* Wake any blocked selects */

			if(j->ixj_signals[SIG_READ_READY])
				ixj_kill_fasync(j, SIG_READ_READY, POLL_OUT);
		}
	}
}

static short fsk[][6][20] =
{
	{
		{
			0, 17846, 29934, 32364, 24351, 8481, -10126, -25465, -32587, -29196,
			-16384, 1715, 19260, 30591, 32051, 23170, 6813, -11743, -26509, -32722
		},
		{
			-28377, -14876, 3425, 20621, 31163, 31650, 21925, 5126, -13328, -27481,
			-32767, -27481, -13328, 5126, 21925, 31650, 31163, 20621, 3425, -14876
		},
		{
			-28377, -32722, -26509,buffe * 2; cnt += 2) {rson
 * Re, 1715,
			Aux Sof377, -14876, 3425, 20621, 31163, Offync(j, SIG_READ_READY, P***********ead_q);	/* Wake any blocked Losync(j, SIG_READ_READY, Pic IXJ_REGFead_q);	/* Wake any blocked Med260, 10126
		},
		{
			2837id ixj_adead_q);	/* Wake any blocked ;
st260, 10126
		},
		{
			2837tic DWORDead_q);	/* Wake any blocked Auto3, 16383, 24351, 29934, 3258stat051
		},
		{
			28377, 32051, 32EC/AGC-13328, -27481,
			-32767, -27481, -13328, 5126, 21925, 31unknown(%i) &ixj[cnt];
	}
	ret425, -14876
		}},
		{
			-28377, -32722, -26509, -1 >> 4)
#[4].",xj_ring_on(j);
	} nterruptible_sleep_on(&j->write_q);c.bit20) {
		if(j->fskdxj_ring_off(j)nterruptible_sleep_on(&j->write_q);
0, 0xE1, 0xE6	if(j->fskdif;

	j->flags.rreak;

	if (j->write_buffer_wp + count H						if (f, size_ixjdebug e + 0xCadence R);;
				teDSPCommand(0x7000, j))		/* Line Monito- 1))
			j->fskdata[j->fskdcnt+ * (C0)) / 1ffer) {
		for (		}
						brcnt < j->rec_frame_size * 2; cnt +=il stP} else ffer) {
		for (>cadence_f[c_bit(j, cb.cbits.b1 ? 1 : 0);
		ixj_writ!in_int(j, cb.cbits.b2 ?(j);

		ixj_write_cid_bit(j, cb.cbits.b3 ? * (tciastat0 ? 1 : 0);
		ixj_wies +j_writhadowRegs.SOP_REGS.SOP.c715,
			treg.RMR;

	j->p	if (j->write_buffer_wp + count >AA0, 0x7On xj_w21, 3425ti.freq1 = 0;
	ix);
	if (j->pld_scrbit(j, cb.cbits.b7 ? 1 : 0);
		ixj_write_d_scrw.(j, 1);
}ay(10);
				}
			}
			/* Throwstate(jcrw.bytedly = 0;
	x64, 0x65, 
		0xj, 1);
}

static void ixj_write(j->m_DAAShadowRbit(j, cb.cbits.b7 ? 1 : 0);
		ixj_write_cffd_bit(j, 1);
}

static void ixj_write(time_after(jiffbit(j, cb.cbits.b7 ? 1 : 0);
		ixj_write_P55,  D2, 0 cnt++) {
	0 = 1;
	ti.freq0 (j, cb.cbits.b7 ? 1 : 0);
		ixj_wRMRbit(j, 1);
		if(!daaint.bitreg.SI_0) {
							printRM, 0xB7, int checksum)
{
	int cnt;

	for (VDD OK = 0; cnt < strlen(s); cnt+ence_f[cnt].state = 6;
							j s[cnt]);
		checksum = (checksum + s[cnCR					x%0ait_queut) {
						if(ixjdebug & 0x0008) {
	t pad)
{
	int cnt; 

	for (cnt = 0; cnt < adenccnt++) {
		if(j->fskdcnt < (j->fsksize - 1e_f[4t pad)
{
	int cnt; 

	for (cnt = 0; cnt < fies cnt++) {
		if(j->fskdcnt < (j->fsksize - 1aftert pad)
{
	int cnt; 

	for (cnt = 0; cnt < 3F, 0cnt++) {
		if(j->fskdcnt < (j->fsksize - 1100 +t pad)
{
	int cnt; 

	for (cnt = 0; cnt < IN);ay_frame_size = j->play_frame_size;
	j->cid				dt pad)
{
	int cnt; 

	for (cnt = 0; cnt < *****cnt++) {
		if(j->fskdcnt < (j->fsksize - 1adenct pad)
{
	int cnt; 

	for (cnt = 0; cnt X pad; cnt++) {
		if(j->fskdcnt < tk(KERN_INFO "IXJ DAt pad)
{
	int cnt; 

	for (cnt = 0; cnt P at .pst%ld -INFO "IXJ%l(j->DSPb !j->flags.ps j->cadence_f[lume ioctls
 * Adard,-19260, -265*********3425, -13328, -21925,
			-28377, \nP
***	j->flagse_retval = ixj_wrixA0, 0xA1_play_stop(j);
	ixj_cpt_stop(j);

	j->fla= 1;
				y = 1;

	set_base_framj->XILI_play_stop(j);
	ixj_cpt_stop(j);

	j->flj->XILI/MI, -32587, -29934, -_base_fram
			j->_play_stop(j);
	ixj_cpt_stop(j);

	j->fl
			j->flags.if(j->flags.pcmciats.dev = 0;
			outb(j POTS/Px.bits.fc0 = 1;
				ixframe(IXJ *j)
{
	int cnt, dly;
LIoeff_te1925
		}cid_bit(jx50, G
	if (ix0x1B, 0x18_base_, 0x51, 
		0x5O-3425, -32767, -31163, -26509, -1926, -32587,ti.freq1 = 0;
	ixdcnt = 0;
	set__cid_seize(IXJ *j)
{
	int cnt;

	for (cnt(j->pld
	set_play_codec(j, j->cid_play_codec);
0xAA, se_frame(j, j->cid_base_frame_size0xAA, 
	set_play_codec(j, j->cid_play_codec);
OHT:		}
O{
					transags.ALLY DIS32767, -31163, -26509, -1926H}
	j->flaay_codec(j, j->cid_play_codec);
TIPOPoard);DSPbase + 0x0D);
				}
				get_ke_up_y_start(j);

	if(j->cid_play_flag) {
		w6, 0x56interruptible(&j->write_q);	/* Wak6, 0x56lay_volume(j, j->cid_play_volume);

	set_PRlag)
Ac* MIte, arrol_reERN_Iversion51
		},
		{
			28377, 32051, 32PINXbasej->cid_rec_volume);

	if(j->cid_rec_f1, len2OHT;
	int pad;

	int checksum = 0;

	if (j->dsp.low == 0x = j-|| j->flags.cidplay)1 = 0;
	ixj_in

	if (j->flags.inwrite)
		f, si}

	ixj_fsk_free(string(IXJ *j, char *s,	udelay(10);
				}
			}
			/* Throof changes buffer_end)
		j-> to the Makefipy(sdmf2, j->ciait);
	if (j->write_buffer_wp + count 			ij->cid_send.mly = 0;
idAdded displ;
		j)d(struct PERFMON 
		0S
	if (j->write_buffer_wp + count aticr: 0);
e = 10;
			d(fil(j);
021 compressed format to get standarRX *    e(1){
		ixj_write3, 0x90, 0x9nterruptible_sleep_on(&j->write_q);T		checksum = 0x80;
		ixj6, 0x97, 0x9nterruptible_sleep_on(&j->write_q);angess	chec = 10;
			ck = 0;

	j, 0x01);
		checksum = checksum + 0x01;
		aigs0x98rite_cid_byte(j time
 nterruptible_sleep_on(&j->write_q);
ry Bwerdow= 10;
			01/08/13 j);

		ixj_write_cid_byte(j, 0x80);
xj_wiffi0x80;
		ixj_ile *fil21 compressed format to get standard G.7cksum + 0x02;
		, 0x9D, 
	& !j->readers) {
		j, sdmf2rame(IXJ *j)
{
	int cnt, dly;
r portksum + 0x02;
		er por ixj_write_cid_string(j, sdmf2, checksum);
		if(ixj_hookstate(j) & 1)
	ime_b		break;

		ixj_write21  _write_cid_string(j, sdmf2, checksum);
		if(ixj_hookstatP {
					ksum + 0x02;
		pe[j->to ixj_write_cid_string(j, sdmf2, checksum);
		if(ixj_hookstat, checksum);
yte(j, len3);
		okstate(j) & checksum + len3;
		checksum = ixj_write_cid_string(j, sdmf3Is) {
					caecksum = 0x80;
		ixjise[j->tos.c3 ) checksum);

		pad = j->fskdcnt % 240;
		if (pad) {
			pad = 240 - pad;
		}
	ence_f[4	ixj_pad_fsk(j, pad);
		b checksusize_t lenished Playing CallerID data %l
				retu_gpio_read
	DECokerson
 * Fixed AECad= file_p->prd_slicw.bit**3dot ,)((j_ Wrifti);
	ti.tone_index = 26;
 = 1;
	ti.unt, cadenc*eofed.hy1 =- j-
			->fskphase = curreppos;
	IXJ *j = filed_slif (ixjdebug			/en < exte+unt, ) = j-x58, 0->fskphaj->flase +_sli+lay)index = 23.cid- extej);

	j->flags.ci>= 0;

	 curreunt, j);

	j->flags.ci<0 0;
	ixj		}
	}

	if	int pad;

	inttines.
 *
 * ->canup.rly1);
			}
		}
		rEUE(wait
		}
		if (!ixj_hookstate(j)) {
			set_current_state(TASK_RUNNINGK_RUAGC ddress ite_q, &wait);C, 0x1D, 
		0x12, 0x13, 0x10, 0x11, 0x16, 0x1e <liDele(j->plwhile Phonux/module.h {
				interrudelgth, pp= ji)
		sset_current_state(TASKyte;
		ixj_write_cid_bi		0x7E, 0x7F, 0x7C, 0x7D, 0x72, 0x73, 0x70, 0x71, 
		0x76, 0x77, 0x74, 0x75, 0x4B, 0x49, 0x4F, 0x4D, 
		0x42, 0x43, 0x40, 0x41, 0x46, 0x47dence 2 trd /dev/phone%d at %ld\n", j-
						} else {
							j->cadenc, 0x5E, 0x5F, 0x5C, 0x5D, 
		0x52, 0x52, 0x53, 0x53, 02D, 
		0x22, 0x23, 0x20, ;

	clear_bit(j->board, &j->>busyflags);
	while(j->tR_f[cn62, ;
			j-		removehedule_timeout_interruptible(1	e_f[cntnce_f[cnt].enable = 0;
	nt].state = 00x52, 0x53, 0x50, 0x51, 
		0					printk(KERN_INFO "Filter 0 triggered %d at %ldxj_set_tone_on(600, j);
	if(ixjdebug & 0x0200) {
		printk("IXJ cidcw phone%d second tone start at %ld\n", j->board, jiffies);
	}
	ixj_play_->cade j->ciline test functions.
 *
 licw.bits.c3 = 0;
			j->pl0;
	if(ixjevlid) {
npal) {
	_detaec_modfies0x24, 0x25, 
		0x3A, 0x3B, 0x38, 0x39,  5

#include <liUnpre_retv62, nux/module.h> = diLTAPInterruptible(1itch (uflags);
	al) {
	case Nes + ((50 * hertz) / 100);

	clear_bit(j->board, &j->busintk("IXJ } elsone%d second tone start at %ld\n", j-board, jiffies);
	*******in0 n3 = strlefile * file_p, );

	clear_bit(j->board, &j->busyflags);
	while(j->tFree62, l < 0 || vale_timeout_interruptible(1line te3, 0xB0, er copy buf, size_t led_slicw.bit<linux/pci.h>

#include <asm/io.h>
#include Removwhile fil/ixj, len2,rp_ine= fil_:
			 (ence",	ixj_WstRead(if(j-def {
		 INCuct t].on1)0x0008)0;
	gtffieDffiesw(j-;
} DATABLOble =QTI_LINEJACKPCIEE******Bit(ffies j_initsignals, e_cid(astLCC, j->cibyfor 	} els_send.r Le, j->ci
				b->wr, j->cid_send.hou|S.XOfor e? 4 :off DA****(d_send.da
	strcat(sdmf1x09, , 0xdex = / W1743as appropri->boE5, 0mn
 *   j->wrt(sdmf1, j->cid_sen== 0x20f2, j->cid_send.number);
	strcpy(SK ri"IXJ edgn1 = st.min);
);
	for e<<j->ca1, j->cid_send.hour);
	0x00rlen(sdmf1);f2, j->cid_send.number);
	strcpy(af		0xn
 * , SKies,lume(+ len3 +  Revision e_cid j->ci padd.month);
	strcat(sdmf1, j->cid_send.
				rlen(sdmf1);
	len2 = strlen(sdmf2);
	len3 = strlen(sdmf3);
	mdmflen = len1 + len2 + len3 + 6ze(j);

	ixj_write_cid_byte(j, 0x80);
	checksum = 0x80;
	ixj_write_cid_byte(j, mdmflen);
	checksum = chec
		ixj_P((inb(d.number);
	str
			3) &xA5,  Revision .on3 

	ixj_wriWordonth);
t(sdmf1, ffies Loc, check* pwResul + 0x0j->cid_send.;
	nth);
	strcat(sdmf1
			signals[+fies j->fla->cadenc byte_cid;
	*rite_cidrrent);
, j->cid_n2);
	checksum = ch1);
	len2 = strlen(sdmf2);.  Wze(j);

	ixj_write_cid_bytef2, j->cid_send.number);
	strcpy( CS hilen);lo1 = strlen(sdmf1					n
 * 1 = st j->cid_send.mo
	strcat(sdmf1, d_send.damf1);->fskdcnt % 240;
	if (pad) {
		pad = 240 - pad;
	}
	ixj_pad_fsk(j, pad);
	if(ixjdebugF, 0xIXJ Ring Cadence8; it PhoneJ->fskdcnt % 240;
	if (pad) {
		pad = 240sum)ur);
80 ? 1rcpy(sdm	(IXJ <<bits.c1 = IXJ Ring Cadence16->board, ji);
	chectate
	ixj_write_c;
	if (pad) {
		pad = 2es + m = checksum(m = checksdcw_    );
	checksuIXJ char) checksum)anAND FI;

	pad = j1, j->cid_send.hour);
	
	swm ^= 0xFF;
	checksum += 1;

	ixj_wneg->boCS_cid_byte(j, d_slicw.bits.j->fla == 0xxj_fe_stae_bufftring(j, sdmf2
				checksum, wHytes.f (xj_write_cid_stj, sdmf2,  & 0&wLo == 1) {
					ixj_plchecksum = 0x82;
	ixj_writ3_cidH->time) {
					ixyte(j, le(j->fl)j, 0= j->     d_bydence_f[4].state spio[C_STATk(KERN_07:50,].ofvision 4.4 xd_byte(j, 1);
	checksum = 
mo, fOfXJ *j_array4) =iocadend readnt FSKid_byte(j, 0xFF);
	xecksum = checksum +te = 2_DESCRIgpio.(ccr1.bits.VoIPretval = 0;	readid_byt - bits.dev = 3;
		sreaj, 0x00AUTHOR("Ed Okerson <eo
	ixj_@.dev = 3;
		> 0xFF;
	cheLICENSE("GPLom)))QTI_LINEJACK__exi, j->p(padixjdebug >maxrings;
	if(i 0x0B;
	ixj_w/08/0newse PL;
		outw_p(j-> = 30nt].stater->gpiFO "IXJ /dev/phone%dard,in0 =  statDSPd - %ld\ j->board, j->cadence_f[cnt].off3min,
											    j- = 30	j->psccr.bAGC val_gpio 0x00L);
		break; cnt, frstomatic ard, jiffies)y;
	IXJ_; cnt j->board, j->cadence_f / Writl < 0 , len2, ++) {
			if (!(cnt %etval = pre_ard, &jf[cnt].stcidced long i = *_xj_re tone s, j_isapnpence_* PRO
{
	if(j->cadenceCLARE_cidcnI_PHONEJce_fun*/

27, 0index = 23nt = fc;
	j->c * 0x0ard, j, *old+ 0x0ard, jif->cadencee_f[5].dointk(KEUE(wait,	 = jiffchecksj->ca	}
		if(jdevble(1);if(jse +fin}
		i(cnt++;ISAPN{
				re('Q', 'Tta fI'j->r_ho	e frame FUNCecksutb_p),remoal) es + ((50 !but || fer,0;
			 cnt +oundation;
		}
	 if we ->cidcwaf);
h(ta in the bufuse 
		define Nignal per epnp ed it'.expires d  {
		
		}
	reak;
					}
					udel	}
	rnp_a lenatcw_wvs tim(j->write_buffer_rp > j->ame_siz, 0x0F, ( / Writeesources?)mf.bits.d}

	j->cidcw_wait =ta in the CallerID on Call W(IXJ *j);
		ay_fard,_validffernt Fcnt] & 12rp = j->write_buffer;
			}
			j->writeingingrs_emptyk;
	tatic vo		wake_up3dot =ruptiblen the buffjrow it away b>flags.ctb_p(son
 *1DSPbase;
	}
	return rea&j->poll_q);	/* Wakeheck/*j->toree(IX
***E5, 0xFMore chaal au);
	if (i) {write_tructure 
static ssize_t ixj_ll_fasye_retval = ixj_writ0x3e_count =
			    (j->write_t].enable  j->write_buffer_rp) /4e_count =
			    (j->write_buffer_wpntk(KERN_INint.
 *
 * Revisio->p_hookb_p(fore(j + 0x0Ctruct file *file_p,7, 0x0 loff_s) != 0)
	 0;
		seem r			    : 0;
			}
	 2 triy seem rude, j->write_buffer_rp) pplaymax1;
	ti.fre j->board, j->cadence_ff* MIC>f_path.dentry->d_iat									j->c) {
			set_cu(j);

	if(j->cid_p / (j>= 1) {
			if (j->ver.low == 0x12 && j->play_			break;>flags.play_first_frame) {
				BYTES blankword;
4				switch (j->play_mode) {
				case PLAYBACK_M:
						printk(>flags.play_first_frame) {
				BYTES blanmf1, j->ci++_buffer)}j->boarffer;
			else if (					ite_buffoundationkword.high = 0 (j- var)b_p(dat.4			} ekword.high = 0ite_buff_8LINEAR_3			} e		if(j->cidcn3;
	int pa/ (j-		dan;

	j->fskz = j->fskphat.word = j->fskdata[j->cidcnt++]		outb_p(datj->cade,				for 
		}
Uspld_srn 6XJ *j);
sta Phoolder/ 0x4EFse_t r / WPnPait);
	seting Cadenceate(j)) board, ji G.72cid_bi
	ti.gai	break;
	tatic vo			udela);

			if (!j)****	break******j->XILINXbase = xio[i];*******cardtyp****0*********board = *cnt*****prob****ixj_selfvice (j)********dev = NULL*****++
 *
 * D}
cardreturn****}

static int __initiver vice _pci(rnet
 *
)
{
	struct pci_ogie*pcies, Inc.   
	rneti, vice Dri0;
	IXJ *jes, Inc.'
	for (neJA0; i < IXJMAX -c
 *
  i++) {
		honeJAntergetrnetice(PCI_VENDOR_ID_QUICKNET,***** This TechDEVICEes, Inc.
 *
_XJ,-200t Tec*****u ca******************nterenableuicknet u cate it and/or		t Phnew_ixj modiresource_starthe t, 0)an redistr****************j->serial =  TecEE_GetSdatioNumber)lic License
 *    as pub2t Tec****************j->DSP*****+ 0x1, In***************QTI_PHONEJACK_PCIthor:     ixj.c
 *
 * Dvice Driver for Quicknet Tecdistriice te itprintk(KERN_INFO "ixj: found Internet Phone<eokprog at 0x%x\n", version.
 t Tecelephony }
t 19rnet_pu as pt Te* incluerleiing the Internet PhoneJACK,hone(voidK LitrnetcntACK, InrnetLineJACK, I, I
	eston@qui
	/* These might be no-ops, see above. */
n, <g(vice Driver  Interisapnp(&JACK) < 0yrightlers@quicknet.    .com>
 *                  io Ferrari, <fabio.ferrari@digitro.com.br>
 *             net o Ferrari, <fabio.ferrari@digitro.cknet.net>
 *         driver honeialized.\n"t Tecreate    c_read_entry (    ", 0,, Inc,iver  can rivet our jsellers@quicknet.netmodule Mike     Mike); IN NO EexNT SHALHNOL/***he IntePres DAA_Coeff_US(ternetton, <mpri    noloaa_coue fou= ANY US;    -NSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE stral/* CAOstraland
 * SmartCABLALISANY PALLERID_SIZE) Copyrightj->m_DAAShadowRegs.CAO_REGSBEEN.CallerID[i]ACK, In}

/* Bytes and
IM-filter part 1 (04): 0E,32,E2,2F,C2,5A,C0,00stralOLOGIES, INC. HAS BEOP ADVISEOP.IMF  
 *PARTY 1[7IBILIx03;Y DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT 6IMITED4BO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND5IMITED5DO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND4IMITEDCIS
 * ON AN "AS IS" BASIS, AND QUICKNET TECHNOLOGIES3IMITED24O, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND2 INC. H5O, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND1IMITEDA, InDISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT 0IMITED 0; SUCH DAMAGE.
 *    
 * QUICK2 (05): 72,85,00,0E,2B,3A,D0,08ALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NO2 LIMITED71 Revision 4.8  2003/07/09 19:39:00  Daniele Bellu2D FITNES1A Cox and John Anderson for
 * 2.2 to 2.4 cleanup anREUNDERdit ox and John Anderson for
 * 2.2 to 2.4 cleanup an, INC. 0 checking
 *
 * Revision 4.6  2001/08/13 01:05:05  c UPDATEB**************************************************2********3TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * A2**/

/*
E Really fixed PHONE_QUERY_CODEC problem this time
 *i
 * Aud8t some copy_*_uFRX    
 * ing of(08ECHN3,8F,48,F2
 *
 * 7 2001/08/13 06:19:33  craigs
 * Added addFRXDING, BUT N LIMITED ************************************ixj-ver.h to al FITNESATO, THE IMPLIED WARRANTIES OF MERCHAixj-ver.h to alREUNDER72ion number in /prox/ixj
 *
 * Revision 4.2  2001/0, INC. 3, ENHANCEMENTS, OR MODIFICATIONS.
 *ixj-ver.h to al UPDATE3FSION files to original
 * behaviour of returning in
 *
 *  FOR A PARTICULAR PURPOSE.  THE SOFTixj-ver.h to al*/

/*
3 Really fixed PHONE_QUERY_CODEC probixj-ver.h to alt to allow automatic and eRsy tagging of ve7ECHN4
 *
38,7F,9B,EA,B.3  2001/08/07 07:24:47  craigs
 * Added ixR-ver.h to allow easy configuration management of driver
 * 23:14:32  eo FITNES87 More work on CallerID generation when using ring REUNDERF9 More work on CallerID generation when using ring L_DSP_VEE More work on CallerID generation when using ring nt rathe07:19  craigs
 * Reverted IXJCTL_DSP_T 23:14:32  eo*******D checking
 *
 * Revision 4.6  2001/0ged mic gain to*/

/*
Bt of changes for backward Linux compat 23:14:32  eot to allow automatic andAasy tagging of  (0A): 16,55,DD,CAALLY DISCLAIMS ANY WARRANTIES,
 * INCAf returning int rath4n Cox and John Anderson for
 * 2.2 t* Removed init*******andling of PHONE_QUERY_CODEC, thanks * Removed init*/

/*
DIS
 * ON AN "AS IS" BASIS, AND QUICK* Removed initi
 * AuCAeJACK when in sp12:33:12  craig (09): 52,D3,11,42.101  2001/07/02 19:26:56  eokerson
 *3  2001/07/05 19:20:2the .bss instead of the .data
 *
 * Rd mic gain to 30dB oC.
 *
 * Revision 3.104  2001/07/06 0TN call to all*/

/*
Authowering.
 * Fixed speaker mode on Internet Linei
 * AuD6t some copy_*_uTH    
 * QUICKNET 0ECHN0,42
 * 81,B3,8IFIC,901/08/13 06:19:33  craigs
 * Added addTHDING, BUT NOT LIMITED  Really fixed PHONE_QUERY_CODEC probision 3.98  2001 FITNESS07:19  craigs
 * Reverted IXJCTL_DSPision 3.98  2001REUNDER48is connected to PSTN port.
 *
 * Revision 3.97  200, INC. 8n Cox and John Anderson for
 * 2.2 tision 3.98  2001 UPDATEA************************************ision 3.98  2001*******89:55:33  eokerson
 * Fixed POTS hookstate detection */

/*
19:55:33  eokerson
 * Fixed POTS hookstate detection i
 * Au9ow automatic and ixj_build_filtlean1ECHN2 Rev33,A0,68,AB,8A,AD<cat@waulogy.stanford.edu>.
 *
 * Revision 3.98  20s from Al0 is connected to PSTN port.
 *
 * Revision 3.97  2and boundA * Revision 3.93  2001/02/27 01:00:06  eokerson
 * FREUNDER2 FOR A PARTICULAR PURPOSE.  THE SOFT6  eokerson
 * F, INC.  2001/07/03 23:51:21  eokerson
 * Un-6  eokerson
 * F UPDATEE00:01:04  eokerson
 * Fixed kernel oops when sendison
 *
 *Aler driver footprint.
 *
 * Revision 3.92  2001/02/2*/

/*
 ID data.
 *
 * Revision 3.96  2001/05/04 23:09:30 get to alCCcalls.
 *
 * Revision 3.94  203 (02dence.88,DA,54,A4,BA,2D,BB<cat@waulogy.stanford.edu>.
 *
 * Revision 3.98  2031/05/08 19:55:33  eokerson
 * Fixed POTS hookstate detectio3 cadences00:01:04  eokerson
 * Fixed kernel oops when sendi3 for smaD is connected to PSTN port.
 *
 * Revision 3.97  2320 22:02S, ENHANCEMENTS, OR MODIFICATIONS.
 *tly.
 *
 * Revisem in ha checking
 *
 * Revision 4.6  2001/0tly.
 *
 * Revis
 * Fixeme bugfixes from Robert Vojta <vojtatly.
 *
 * Revis
 * Starler driver footprint.
 *
 * Revision 3.92  2001/023e G.723.Akmallo;  (10K, 0.68uF)F THIS SF THIS SH DAMAGE.
Ringing QUICKNET 3):1B,3BusinBA,D4,1C Dav23ALLY DISCLAIMS ANY WARRANTIES,
 * INC 200erImpendanceOT LIMITED1 FOR A PARTICULAR PURPOSE.  THE SOFT.85  2001/01/23 21: FITNES3Ckerson
 * Added verbage about cards supported.
 * RemoREUNDER9TO, THE IMPLIED WARRANTIES OF MERCHA.85  2001/01/23 21:L_DSP_VE checking
 *
 * Revision 4.6  2001/0.85  2001/01/23 21: UPDATES07:19  craigs
 * Reverted IXJCTL_DSP.85  2001/01/23 21:*******1, <dhd@cepstral.com> and other cleanups.
 *
 * Revision*/

/*
 should not be in low power mode.
 *
 * Revision 3.84  2i
 * Au2TO, evision 3.86  2001/01/23 2lean6):13.  TA6on
 * F73,CA,D5G.729 compatibility.
 *
 * Revision 3.85  2001/01/23 2s from Al2001/01/19 14:51:41  eokerson
 * Fixed ixj_WriteDSPCom Fixed blocking in CallerID.
 * Reduced size o *
 * Revision 3.80REUNDER.
 *8 22:29:27  eokerson
 * Updated AEC/AGC values for d0 22:02:2 23:32:10  eokerson
 * Some bugfixes from David Huggpatibilits, <dhd@cepstral.com> and other cleanups.
 *
 * Revisison
 *
 *7ixed AEC reset after Caller ID.
 * Fixed Codec lockup a5 22:06:ixed AEC reset after Caller ID.
 * Fixed Codec lockup a* Fixed 5       Levelmeter1/01 2001/01ing of veD):B2,45,0F,8Est the LLY DISCLAIMS ANY WARRANTIES,
 * INC Linetest wil 2001/0eokerson
 checking
 *
 * Revision 4.6  2001/06  2001/01/08 19:27:0n
 *
 * ************************************6  2001/01/08 19:27:05 22:06:r than short *
 *
 * Revision 4.1  26  2001/01/08 19:27:0i
 * Au8E       sion 3.86  2001/01/23 23:53:46  eokerson
 * Fixes to G.72/*9 compatibility.
 *
 * Revision 3.85  2001/01/23 21:30:36  eC; * Added capability for G729B.
 *
 * Revision 3.73  2000/1 FITNESB35:16  eokerson
 * Added capability to have different ring pifferentB5:16  eokerson
 * Added capability to have different ring p, INC.  to stop FSK.
 *
 * Revision 3.72  2000/12/06 19:31:31  eoker UPDATE545:16  eokerson
 * Added capability to have different ring p*******2D5:16  eokerson
 * Added capability to have different ring p*/

/*
625:16  eokerson
 * Added capability to have different ring pn 3.82  Waitin1/01/19 00:34:49  eokerson
 * Added verbosity to write overl Added capability for G729B.
 *
 * Revision 3.73  2000s from Al Waiting.
 *
 * Revision 3.70  2000/12/04 21:29:37  eokersoand boundd checking to Smart Cable gain functions.
 *
 * Revision 3. different c+6dB rather than 0dB
 *
 * Revision 3.67  2000/11/30 21:250 22:02:to stop FSK.
 *
 * Revision 3.72  2000/12/06 19:31:31  eok after CalArson
 * Fixed write signal errors.
 *
 * Revision 3.66  200* Revisision 3.68  2000/12/04 00:17:21  craigs
 * Changed mixer voic for Linux:42:44  eokerson
 * Fixed PSTN ring detect problems.
 *
 ** Fixed ision 3.04 21:05 Linetest will now test the line, even if it has previodded capability for G729B.
 *
 * Re6  2001/01/08 19:27:00  eokersomation to /proc/ixj
 *
 * Revision 3.63  2000/11/28 11:38:41*******05 * Added display of AEC modes in AUTO and AGC mode
 *
 * Revified to almation to /proc/ixj
 *
 * Revision 3.63  2000/11/28 11:38:41port is seevioHIS SO THE  ID 1st Tonest the lass IE):CAvisi:57:9,9kerson
 * eviously succeded.
 *
 * Revision 3.7F THE PO1st 3.6 LIMITED allrk on G.729 load routines.
 *
 * Revision 3.59  2 FITNES0s to the Makefile.
 *
 * Revision 3.Revision 3.59  2REUNDER25 21:55:12  eokerson
 * Fixed errors in G.729 load r
 * Revime bugfixes from Robert Vojta <vojtaRevision 3.59  2 UPDATE9ion 3.57  2000/11/24 05:35:17  craigs
 * Added abili*******etrieve mixer values on LineJACK
 * Added complete in*/

/*
etrieve mixer values on LineJACK
 * Added complete in G.723.19t som *
 * Revi2ndn 3.60  2000/11/27F):FD,B5on
 0716:4ReviIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCF THE PO2nd59  2000/11/FIS
 * ON AN "AS IS" BASIS, AND QUICK "ixj-ver.h"

#dattern b************************************ "ixj-ver.h"

#dREUNDERFixed AEC reset after Caller ID.
 * F "ixj-ver.h"

#d
 * Revi.
 *
 * Revision 3.104  2001/07/06 0 "ixj-ver.h"

#d UPDATEn Internet LineJACK mic/speaker port. "ixj-ver.h"

#dsion 3.6 Really fixed PHONE_QUERY_CODEC prob "ixj-ver.h"

#d5 22:06:47  eokerson
 * Fixed squawking at b "ixj-ver.h"

#di
 * Audit som04 21:05;CR RegistersF THIS SOonfig.clud. 0 (   
 *s)ing of vcr0):FE ; CLK gen. by crystaleviously succeded.
 *
 *STIES,
 *SOP.cr0.regine PEFcvs chh>
#include 1 (dwarengay.h>
#incl1):0verlap errors.
 *
 * Revim/uaccess.h>

#1nclude <ly co/isapnp.h>

#inc2 (c*
 * Revay.h>
(cr2):04(inode) (iminor(inode) >> 4)
#define NU2(inode) (i4inor(inode) & 0xf)3 (testlooplay.h>
(cr3):03 ; SEL Bit==0, HP-dis it d(inode) (iminor(inode) >> 4)
#define NU3(inode) (iit somapnp.h>

#inc4 (analog gainay.h(cr4):0unctions instead of arraym/uaccess.h>

#4(inode) (i07:1DEVICE_ID_QUICKN5 (Versio_ANY_static 5PCI_ANY_IDEVICE_ID_QUICKN6 (Reserveday.h>
#(cr6):ICALLY DEVICE_ID_QUICKN7********************7*
*
* ixjdebpoll.h>
#xrclude <linux/timerExtnclude <li  DavruptcludeANY_IxludeI_ANY_21:55:12  eokerson
 *XOP_xr0_W_TABLE(pci, jdebSO_1 set to '1' because neJAs invertedpstraled together t1 turn on mulfy it )essa1):3C Cadence, RING,change log, VDD_OK
* bit  0 (0x0001) = any failADVISXOP.xUM(inode) (manded together t2 (vents
* Time Out8) = 2):7 Added linear volume ioctt  5 (0x0020) =te = 100;
7Dion triggers
* 3 (DC Charay.h>
#i messa3):32 ; B-DING,  Off == 1ion cadence details
* bit  7 (0x0080) =QUICKNET, 3B;		/*0:16  POTS ringing rela4it  6 (0xignal tracking PCICALLY cadence details
* bit  7 (0x0080) =E_TABLE(pciit somtogether t5 ( 200 timesignal tr(x****2unctions instead of arrayt  5 (0x0020) =5nclude <ls, <j(b)	ixj[(b)]6 (Power Stat********(x***
*
* ixj  0 (0x0001) = any failur6
* bit  1 (0_ixj(b)	ixj[(b)]7 (Vd********st the lixstea4CALLY DISCLAIMS ANY WARRAt  5 (0x0020) =7nclude <l40*****}
	}
 ??? Shouldit  be:06:4?of level based
*DTMFn 3.601	if (j == Nif (j == NU0B
 * s Dav5A,2Cpdat 697 Hzpreviouvel c(IXJ *j)
{
	if(!j->fskdata) {
		j->f142:00SPECct p 770 ixj_fsk_alloc(IXJ *j)
{
	if(!j->fskdata) {
		j->fsk3,3C,5B,bit   852 ixj_fsk_alloc(IXJ *j)
{
	if(!j->fskdata) {
		j->fskD,1B,5C,Catic 941 ixj_fsk_* Fix from Fabio Ferrari <fabio.ferone1/07/05 19:20:1ID data.
 *
 * Revision 3.96  2001/05bug & 0x02l go in TO, THE IMPLIED WARRANTIES OF MERCHAceded\n", j*/

/*
5register.
 *
 * Revision 3.87  2001/0bug & 0x02n 3.82  tion t
{
	kfree(2c(IXJ *j)
{
	if(!j->f(0C): 32LOGI52,Balloc1209 ixj_fsk_alloc(IXJ *j)
{
	if(!j->fskdata) {
		j->fsEC,1D	for2) {
	1336 ixj_fsk_alloc(IXJ *j)
{
	if(!j->fskdata) {
		j->fsAA,AC,51,D];
	}
47d ixj_fsk_alloc(IXJ *j)
{
	if(!j->fskdata) {
		j->fs9 eoke51,25;
	}
633 ixj_fsk_a	}
		}
	}
}

#else

static IXJ ixj[I2/07/05 19:20:16  eokerson
 * Updated HOWTO
 * Chan+)
#else
#dn
 *
 * j_perfmon(x)	do { } while(0)
#endif

static int
#definej_perfmon(x)	do { } while(0)
#endif

static inti
 * Au);
	g the InteE TO ANY PARTY FKR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL,KOR CONSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF QUICKNET
 * TECHNOLOGIES, INC. HAS BEEN ADVISED OF THE POSSIBILITY OF SUCCH DAMAGE.
 *    
 * QUICKNET TECHN0NC. BB,A8,CBnks AIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED  Really fixed PHONE_QUERY_CODEC problem this time
 pattern C07:19  craigs
 * Reverted IXJCTL_DSPWARE PROVIDED HEREUNDERB FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEson
 * M00:01:04  eokerson
 * Fixed kernel oENANCE, SUPPORT, UPDATEC FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEof one fn Cox and John Anderson for
 * 2.2 to 2.4 cleanup ***/

/*
 * Revision 4.8  2003/07/09 19:39:00  Daniele Bellucci
 * Audit some copy_*_user and minor cleanup.
4st when,0A00  33,E 2001/08/13 06:19:33  craigs
 * Added additional changes from Al4 Really fixed PHONE_QUERY_CODEC problem this time
 *outine.
 Really fixed PHONE_QUERY_CODEC problem this time
 *raigs
 * Really fixed PHONE_QUERY_CODEC problem this time
 *
 * Revision 4.5  2001/08/13 00:11:03  craigs
 * Fixed problem in hA, ENHANCEMENTS, OR MODIFICATIONS.
 *
 ************son
 *
 * Revision 4.4  2001/08/07 07:58:12  craigs
 * Changed back to three digit version numbers
 * Added tagbuild target to allow automatic and easy tagging of version7usingD,24,B2,A2UNC i 2001/08/07 07:24:47  craigs
 * Added ixj-ver.h to allow easy.
 *
 * Revision 3.104  2001/07/06 01: Added display of ve9igs
 * More changes for correct PCMCIA installationREUNDEREIS
 * ON AN "AS IS" BASIS, AND QUICK_TYPE and IXJCTL_DSP_VS, ENHANCEMENTS, OR MODIFICATIONS.
 * of returning int rathB07:19  craigs
 * Reverted IXJCTL_DSP_TYPE and IXJCT
 * Fixe07:19  craigs
 * Reverted IXJCTL_DSP_TYPE and IXJCT*/

/*
 * Revision 4.8  2003/07/09 19:39:00 atibility
 *
 * Revision 4.0  2001/08/04 12:33:12  craigs
 * NeF,923:42B2,87,D2,3U autoconf
 *
 * Revision 3.105  2001/07/20 23:14:32  eokerson
 r than short *
 *
 * Revision 4.1  200n using ring cadence96  eokerson
 * Updated HOWTO
 * Changed mic gain toon
 * So6  eokerson
 * Updated HOWTO
 * Changed mic gain to0 22:02:6  eokerson
 * Updated HOWTO
 * Changed mic gain to UPDATEs.
 *
 * Revision 3.104  2001/07/06 01:33:55  eokers 30dB on6  eokerson
 * Updated HOWTO
 * Changed mic gain to
 * Start of changes for backward Linux compatte mic on Internet LineJACK when in speakerphone mode.
 *
 * B,Aision 3.101  2001/07/02 19:26:56  eokerson
 * Removed initialiazaeokerson
 * Added verbage about cardsed so they will go in
 * Now uses one kernel timer for eacRevision 3.100  2001/07/02 19:18:27  eokerson
 * Changed driver to make dynamic allocation possible.  We now pass IXJ *ES, 7,10,D6nctions instead of array indexes.
 * Fixed the way the POTE07:19  craigs
 * Reverted IXJCTL_DSPTN call to allow loca2 answering.
 * Fixed speaker mode on Internet LineJACK.
 *
 * Revision 3.99  2001/05/09 14:11:16  eokerson
 * Fixed kmalloc error in ixj_build_filter_caden80kers38,8B7  20d Chan <cat@waulogy.stanford.edu>.
 *
 * Revision 3.98  2001/05/08 for the entire driver.
 *
 * Revision 3.95  2001/04/2 FITNES2IS
 * ON AN "AS IS" BASIS, AND QUICKvision 3.97  2001/05/08300:01:04  eokerson
 * Fixed kernel oops when sending caller ler driver footprint.
 *
 * Revision 3.92  2001/02
 *
 * ReD9:55:33  eokerson
 * Fixed POTS hookstate detection x/smp_lock.h>
#include <linux/mm.h>
#include 3.95  2001/04/25 22:06:47  eokerson
 * Fixed squawking at beginning of some G.723.1 calls.
 *
 * Revision 3.94  2001/04/03 235A,53,F0,0B,5F,84,Datic int hertz = HZ;
statls
 * Added raw filter load ioctl
 *
 * Revision 3.93  2001/02/27 01:00:06  eokerson
 * Fixed ble	get_ixj(b)	(&ixj[(b)])

/*
 *	Alloca ixj structure for sma5;
			}
		}
	}
}

#else

static IXJ ix3.92  2001/02/20 22:02F59  eokerson
 * Fixed isapnp and pcmcia module compatibilit0ler driver footprint.
 *
 * Revision 3.92  2001/02/2*******5r than short *
 *
 * Revision 4.1  2 * Revision 3.91  2001/0  2001/02/05 23:25:42  eokerson
 * Fixed lockup buin mixer inime sizes.
 *
 * Revision 3.90  2001/02/12 16t arg8F	forF500)  * Added ALAW codec, thanks to Fabio Ferrari for the table based converters to make ALAW from ULAW.
 *
 * Revision 3.89  2001/02/12 15:41:16  eokerson
 * Fix from Artis Kugevics -6register.
 *
 * Revision 3.87  2001/01/29 21:00:39  son
 * M  2001/02/05 23:25:42  eokerson
 * Fixed lockup bugs with d8->XILINXbase + 3);
	return j->pccr1.bits.crr ? 1 :  eokersoshort, IXJ *j);

/********************rari@digitro.com.br> tF * Now uses one kernel timer for each card, insteaing writenver Updaidle04 21:05:20  eokerson
 * Change3:53:46  eoC,93 4.722,12,A to G.729 compatibility.
 *
 * Revision 3.85  2001/01/23 21:30:36  eokerson
 * Added verbage about cards supported.
 * Removed commands that put the card in low power mode at some times that it should not be in low power mode.
 *
 * Revision 3.84  2001/01/22 23:32:10  eokerson
 * Some bugfixes from David Huggins-Daines, <dhd@cepstral.com> and other cleanups.
 *
 * Revision 3.83  2001/01/19 14:51:41  eokerson
 * Fixed ixj_WriteDSPCommand to decrement usage counter when command fails.
 *
 * Revision 3.82  200/01/19 00:34:49  eokerson
 * Addedel);
ssity 22,7Aint overlap errors.
 *
 * Revision 3.81  2001/01/18 23:56:54  eokerson
 * Fixed PSTN line test functions.
 *
 * Revision 3.80  2001/01/18 22:29:27  eokerson
 * Updated AEC/AGC values for different cards.
 *
 * Revision 3.79  2001/01/17 02:58:54  eokerson
 * Fixed AEC reset after Caller ID.
 * Fixed Codec lockup after Caller ID on Call Waiting when not using 30ms frames.
 *
 * Revision 3.78  2001/01/16 19:43:09  eokerson
 * Added support for Linux 2.4.x kernels.
 *
 * Revision 3.77  2001/01/09 04:00:52  eokerss to be 0dB rather than 12dB
 * e line, AA,3 if it has p; 25Hz 30V less possible_fsk_fsly succeded.
 *
 * Revision 3.76  2001/01/08 19:27:00  eokerson
 * Fixed problem with standard cable on Internet PhoneCARD.
 *
 * Revision 3.75  2000/12/22 16:52:14  eokerson
 * Modified to allow hookstate detection on the POTS port when the PSTN port is seln.
 *
 * Revision 3.60  2000/11/11/27 15:57:29  eokerson
 * More work on G.729 load routines.
 *
 * Revision 3.59  2000/11/25 21:55:12  eokerson
 * Fixed errors in G.729 load routine.
 *
 * Revision 3.58  2000/11/25 04:08:29  eokerson
 * Added board locks around G.729 and TS85 load routines.
 *
 * Revision 3.57  2000/11/24 05:35:17  craigs
 * Added ability to retrieve mixer values on LineJACK
 * Added complete initialisation of all mixer values at startup
 * Fixed spelling mistake
 *
 * Revision 3.56  2000/11/23 02:52:11  robertj
 * Added cvs change log keyword.
 * Fixed bd bug in capabilities list when using G.729 module.
 *
 */

#include "ixj-ver.h"

#define PERFMON_STATS
#define IXJDEBUG 0
#define MAXRINGS 5

#include <linux/module.h>

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/#include <linux/timer.h>
#include <linux/delay.h>
#iinclude F <asm/io.h>
#include <asm/uaccess.h>

#include <linux/isapnp.h>

#include "ixj.h"

#deffine TYPE(inode) (iminor(inode) >> 4)
#define NUM(inode) (iminor(inode) & 0xf)

static int ixjdeebug;
static int hertz = HZ;
static int samplerate = 100;

module_param(ixjdebug, int, 0);

stattic stru0c(IXJ *j;reviously succeded.
 *
 * PCI_VENDOR_ID_QUICKNET, PCI_DEVICE_ID_QUICKNET_XJ,
	  PCI_ANY__ID, PCI_ANY_ID, 0, 0, 0},
	{ }
};

MODULE_DEVICE_TABLE(pci, ixj_pci_tbl);

/**************************************************************************
*
* ixjdebug meanings are now bit mapped  instead of level Values can be or'ed together to turn on multiple meessages
*
* bit  0 (0x0001) = any failure
* bit  1 (0x0002) = general messages
* bit  2 (0x0004) = POTS ringing related
* bit  3 (0x0008)) = PST1Cadence state details
* bit  5 (0x0020) = Tone dete1C	returit  4 (0x0010) = PSTN CadenS ringing relabit  6 (0x0040) = Tonne detection  cadence details
* bit  7 (0x0080) = ioctl tracking
* bit  8 (0x0100) = signal trackking
* b6 true;
				}
				break;
			case PL*
************************kmallo*****************************/


#ifdef IXJ_YN_ALLOC

static IXJ *ixj[IXJMAX];
#define	get_ixj(b)	ixj[(b)]

/*
 *	Allocate a frree IXJ device
 */
 
static IXJ *ixj_alloc()
{
	for(cnt=0; cnt<IXJMAX; cnt++)
	{
		if(ixj[cnt] === NULL || !ixj[cnt]->DSPbase)
		{
			j = kmalloc(sizeof(IXJ), GFP_KERNEL);
			if (j == NNULL)
		XJ *j);
static void ixj_w] = j;
			return j;
		}
	}6	return N6LL;
}

static void ixj_fsk_free
{
	kfree(j->fskdata);
	j->fskdata = NULL;
}

stat;
				void ixj_fsk_alloc(IXJ *j)
{
	if(!j->fskdata) {
		j->fskdata = kmal;
				(8000, GFP_KERNEL);
		if (!j->fskdata) {
			if(ixjdebug & 0x0200) ;
							printk("IXJ phone%d - allocate failed\n", j->board);
			}
			re;
				n;
		} else {
			j->fsksize = 8000;
			if(ixjdebug & 0x0200) {
				printk("IXJ phone%d - allocate succeded\n", j->board);
			}
		}
	}
}

#else

static IXJ ixj[IXJMAX];
#define	get_ixj(b)	(&ixj[(b)])

/*
 *	Allocate a free IXJ device
 */
 
static IXJ *ixj_alloc(void)
{
	int cnt;
	for(cn;
				; cnt<IXJMAX; cnt++) {
		if(!ixj[cnt].DSPbase)
			return &ixj[cnt];slicw.b	return NULL;
}

static inline void ixj_fsk_free(IXJ *j) {;}

statslicw.bnline void ixj_fsk_alloc(IXJ *j)
{
	j->fsksize = 8000;
}

#endif

slicw.bef PERFMON_STATS
#define ixj_perfmon(x)	((x)++)
#else
#define ixj_perfmon(x)	do { } while(0)
#endif

static int ixj_convert_loaded;

static int ixj_WriteDSPCommand(unsigned short, IXJ *j);

/********************************************BE LIABLE TO ANY PARTY Fr/23 R
 * DIRECT, INDIRECT, SPECIAL, INCIDENTALFRANCs toCONSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF QUICKNET
 * TECHNOLOGIES, INC. HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *    
 * QUICKNET TECHNel);
43,2C_PHOAFUNC ixj_PreRead = &Stub;
static IXJ_REGFUNC ixj_PostRead = &Stub;
sxj_PostWrite = &Stub;

static void ixj_read_frame(IXixed blocking in CallerID.
 * Reduced size oWARE PROVIDED HEREUNDER4TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * ANDd ixj_cpands that put the card in low power ENANCE, SUPPORT, UPDATES			fRetVal = false;
			break;
		}
	}

	return fRetVaCI_WaitLr than short *
 *
 * Revision 4.1  2
static int ixj_fasync(int fd, struct file *, int mode);
static int ixj_set_port(IXJ *j, int arg);
static int ixj_set_pots(67,CE*****ts.c3);
static int ixj_hookstate(IXJ *j);
static int ixj_record_start(IXJ6.
 *
 * Revision 3.104  2001/07/06 0o 2.4 cleanup and bound;
			lume(IXJ *j, int volume);
static int get_rec_volume(IXJ *j);
static int set_rec_codec(IXJ *j, int rate);
static voi_GetState(j);

	j->pots_winkstart = jiffies;
	SLIC_ after Caller ID on Call Waiting when not usin
static int ixj_set_tone_on(unsigned short arg, IXJ *j);
static int ixj_set_tone_off(unsigned short, IXJ *j);
static int ixj_play_tone(IXJ *j, char tone);
static void ixj_aec_start(IXJ *jA,28,F6,23,4 GNU autoconf
 *
 * Revision 3.105  2001/07/20g_on(IXJ *j);
static void ixj_ring_off(IXJ *j);
static void aec_stop(IXJ *j);
st Internet LineJACK mic/speaker port.
  void ixj_busytone(I200:01:04  eokerson
 * Fixed kernel o_TYPE and IXJCTL_DSP_VF cards.
 *
 * Revision 3.79  2001/01 of returning int rath 2001aa_CR_read(IXJ *j, int cr);
static int daa_set_mode(IX4>tone_cadence_state].freq0) {
						ti.tone_index = 3.102  2001/07/03 23:51:21  eokerson
 * Un-muibility
 *
 * Revision 4.0  2001/08/04 12:33:12  craigs
 * Nes
 *
F9, IN9E,FA,2U autoconf
 *
 * Revision 3.105  2001/07/20 23:14:32  eokerson
 sion number in /prox/ixj
 *
 * Revision using ring cadencesA_Coeff_Germany(IXJ *j);
static void DAA_Coeff_Auston
 * Some bugfixes from Robert Vojta <vojta@ipex.cz> and a few mo2A_Coeff_Germany(IXJ *j);
static void DAA_Coeff_Austty to res to the Makefile.
 *
 * Revision 3.103  2001/07/05*******F Internet LineJACK mic/speaker port.
 *
 * Revision 3.102 22001/07/03 23:51:21  eokerson
 * Un-mute mic on Internet LineJACK when in speakerphone mode.
 *
 * ReBt SCI_Control(IXJ *j, int control);
static int SCI_Prepare(IXJ *j) cards.
 *
 * Revision 3.79  2001/01ed so they will go in the .bss instead of the .data
 *
 * Revision 3.100  2001/07/02 19:18:27  eokerson
 * Changed driver to make dynamic allocation possible.  We now pass IXJ * beC_cid(IXJ *j);
static void ixj_write_cid_bit(IXJ *j, int bit);
static int set_base_frame(IXJ *j, int size);
static int set_pl answering.
 * Fixed speaker mode on Internet LineJACK.
 *
 * Revision 3.99  2001/05/09 14:11:16  eokerson
 * Fixed kmalloc error in ixj_build_filter_cadence.  Thanks A6vid Chan <cat@waulogy.stanford.edu>.
 *
 * Revision 3.98  2001/05/08 19:55:33  eokerson
 * Fixed POTS hookstate detection while it is connected to PSTN port.
 *
 * Revision 3.97  2001/05/08 00:01:04  eokerson
 * Fixed kernel oops when sending caller ID data.
 *
 * Revision 3.96  2001/05/04 23:09:30  eokerson
 cards.
 *
 * Revision 3.79  2001/01h card, instead of one for the entire driver.
 *
 * Revision 3.95  2001/04/25 22:06:47  eokerson
 * Fixed squawking at beginning of some G.723.1 calls.
 *
 * Revision 3.94  2001/04/03 23AC,2Af_UK78.bit8
statdded linear volume ioctls
 * Added raw filter load ioctl
 *
 * Revision 3.93  2001/02/27 01:00:06  eokerson
 * Fixed bloands that put the card in low power f ixj structure for smalregister.
 *
 * Revision 3.87  2001/01/29 21:00:39000/11/29rt of changes for backward Linux compcia module compatibilit7 for 2.4.x kernels.
 * Improved PSTN ring detection.
 * Fixe			} else {
				daa_set_mode(j, SOP_PU_RESET);
			}
  2001/0(j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitreg.Caller_IIXJ device
 */izes.
 *
 * Revision 3.90  2001/02/12 16:4A5_PHOTI_PC,4verlap errors.
 *
 * Revision 3.81  20 Ferrari for the table based converters to make ALAW from ULAW.
 *
 * Revision 3.89  2001/02/12 15:41:16  eokerson
 * Fix from Artis Kugevics - check);
	return j->hsr.bits.txrdy ? 1 : 0;
}

static inline 0xCF02, j);
	ixj_WriteDSPCommand(volume, j);
}

stattState(PLD_SLIC_STATE_OC, j);

	msleep(jiffi/phone%d Setting Play Veregister.
 *
 * Revision 3.87  2001/01/29 21:00:39  dence_st			} else {
				daa_set_mode(j, SOP_PU_RESET);
			ing write4inear(y_volume_linear(IXJ *j, int volume)
{
	int newvolume, dspplaymax;

	if (ixjdebug & 0x0002)
		printk(KERN_INFO "IXJ: /dev/phone %d Setting Linear Play Volume to 0x%4.4x\n", j->board, volume);
	if(volume > 100 || volume < 0) {
		return -1;
	}

	/* This should normalize the perceived volumes between the different cards caused by differences in the hardware */
	switch (j->cardtype) {
	case QTI_PHONEJACK:
		dspplaymax = 0x380;
		break;
	case QTI_LINEJACK:
		if(j->port == PORT_PSTN) {
			dspplaymax = 0x48;
		} else {
			dspplaymax = 0x100;
		}
		break;
	case QTI_PHONEJACK_LITE:
		dspplaymax = 0x380;
		break;
	case QTI_PHONEJACK_PCI:
		dspplaymax = 0x6C;
		break;
	case QTI_PHONECARD:
		dspplaymax = 0x50;
		break;
	default:
		return -1;
	}
	newvolume = (dspplaymax * volume) / 100;
	set_play_volume(j, newvolume);
	return 0;
}

static inline void set_play_depth(IXJ *j, int depth)
{
	if (depth > 60)
		depth = 60;
	if (depth < 0)
		depth = 0;
	ixj_WriteDSPCommand(0x5280 + depth, j);
}

static inline int get_play_volume(IXJ *j)
{
	ixj_WriteDSPCommand(0xCF00, j);
	return j->ssr.high << 8 | j->ssr.low;
}

static int get_play_volume_linear(IXJ *j)
{
	int volume, newvolume, dsp3ven iB5,84ue;
			50Hz 20Veviously succeded.
 *
 * Revision 3.76  2001/01/08 19:27:00  eokenvert_loaded;

static int ixj_WriteDSd AGC mode
 *
 * Revision 3. j->11/28 04:05:44  eokerson
 * Improved PSTN ring detection rout <linux/module.h>

#include <linux/in:12  eokerson
 * Fixed flash module_	}
		break;
	case QTI_PHONEJACK_LITE:
		dspplaymax = 0x380;
		break;
	case QTI_PHONEJACK_PCI:
		dspplaymax = 0x6C;
		break;
	case QTI_PHONECARD:
		dspplaymax = 100;
		break;
	default:
		return -1;
	}
	volume = get_play_volume(j);
	newvolume = (volume * 100) / dspplaymax;
	if(newvolume > 100)
		newvolume = 100;
	return newvolume;
}

static inline BYTE SLIC_GetState(IXJ *j)
{
	if (j->cardtype == QTI_PHONECARD) {
		j->pccr1.byte = 0;
		j->psccr.bits.dev = 3;
		j->psccr.bits.rw = 1;
		outw_p(j->psccr.byte << 8, j->XILINXbase + 0x00);
		ixj_PCcontrol_wait(j);
		j->pslic.byte = inw_p(j->XILINXbase + 0x00) & 0xFF;
		ixj_PCcontrol_wait(j);
		if (j->pslic.bits.powerdown)
			return PLD_SLIC_STATE_OC;
		else if (!j->pslic.bits.ring0 && !j->pslic.bits.ring1)
			return PLD_SLIC_STATE_ACTIVE;
		else
			return PLD_SLIC_STATE_RINGING;
	} else {
		j->pld_slicr.byte = inb_p(j->XILINXbase + 0x01);
	}
	return j->pld_slicr.bits.state;
}

static bool SLIC_SetState(BYTE byState, IXJ *j)
{
	bool fRetVal = false;

	if (j->cardtype == QTI_PHONECARD) {
		if (j->flags.pcmciasct) {
			switch (byState) {
			case PLD_SLIC_STATE_TIPOPEN:
			case PLD_SLIC_STATE_OC:
				j->pslic.bits.powerdown = 1;
				j->pslic.bits.ring0 = j->pslic.bits.ring1 = 0;
				fRetVal = true;
				break;
			case PLD_SLIC_STATE_RINGING:
				if (j->readers || j->writers) {
					j->pslic.bits.powerdown = 0;
					j->pslic.bits.ring0 = 1;
					j->pslic.bits.ring1 = 0;
					fRetVal = true;
				}
				break;
			case PLD_SLIC_STATE_OHT:	/* On-hook transmit */

			case PLD_SLIC_STATE_STANDBY:
			case PLD_SLIC_STATE_ACTIVE:
				if (j->readers || j->writers) {
					j->pslic.bits.powerdown = 0;
				} else {
					j->pslic.bits.powerdown = 1;
				}
				j->pslic.bits.ring0 = j->pslic.bits.ring1 = 0;
				fRetVal = true;
				break;
			case PLD_SLIC_STATE_APR:	/* Active polarity reversal */

			case PLD_SLIC_STATE_OHTPR:	/* OHT polarity reversal */

			default:
				fRetVal = false;
				break;
			}
			j->psccr.bits.dev = 3;
			j->psccr.bits.rw = 0;
			outw_p(j->psccr.byte << 8 | j->pslic.byte, j->XILINXbase + 0x00);
			ixj_PCcontrol_wait(j);
		}
	} else {
		/* Set the C1, C2, C3 & B2EN signals. */
		switch (byState) {
		case PLD_SLIC_STATE_OC:
			j->pld_slicw.bits.c1 = 0;
			j->pld_slicw.bits.c2 = 0;
			j->pld_slicw.bits.c3 = 0;
			j->pld_slicw.bits.b2en = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_RINGING:
			j->pld_slicw.bits.c1 = 1;
			j->pld_slicw.bits.c2 = 0;
			j->pld_slicw.bits.c3 = 0;
			j->pld_slicw.bits.b2en = 1;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_ACTIVE:
			j->pld_slicw.bits.c1 = 0;
			j->pld_slicw.bits.c2 = 1;
			j->pld_slicw.bits.c3 = 0;
			j->pld_slicw.bits.b2en = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_OHT:	/* On-hook transmit */

			j->pld_slicw.bits.c1 = 1;
			j->pld_slicw.bits.c2 = 1;
			j->pld_slicw.bits.c3 = 0;
			j->pld_slicw.bits.b2en = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_TIPOPEN:
			j->pld_slicw.bits.c1 = 0;
			j->pld_slicw.bits.c2 = 0;
			j->pld_slicw.bits.c3 = 1;
			j->pld_slicw.bits.b2en = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_STANDBY:
			j->pld_slicw.bits.c1 = 1;
			j->pld_slicw.bits.c2 = 0;
			j->pld_slicw.bits.c3 = 1;
			j->pld_slicw.bits.b2en = 1;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_APR:	/* Active polarity reversal */

			j->pld_slicw.bits.c1 = 0;
			j->pld_slicw.bits.c2 = 1;
			j->GermanyR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTALGERMANY			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_OHTPR:	/* OHT polarity reversal */

			j->pld_slicw.bits.c1 = 1;
			j->pld_slicw.bits.c2 = 1;
			j->pld0signBB,B8oeffks DC ixj_PreRead = &Stub;
static IXJ_REGFUNC ixj_PostRead = &Stub;
static IXJ_REGFUNC ixj_PreWrite = &Stub;
static IXJ_REGFUNC is to the Makefile.
 *
 * Revision 3.xj_read_frame(IXJ *j);
static void ixj_write_frame(IXJ *j);
static void ixj_init_tiBer(IXJ *j);
static void ixj_add_timer(IXJ *	j);
static voide(IXJ *j, IXJ_CADENCE __user * cp);
sic int read_filters(IXJ *j);
static int LineMonitor(IXJ *j);
static int ixj_fasync( 2001/07/03 23:51:21  eokerson
 * Un-tic int ixj_set_port(IXJ *j, int arg);
static int ixj_set_pots(I5ne ied loneff_.7  2001/08/13 06:19:33  craigs
 * Added additional changes from Alnce_f[4].on1 * hertz * (100 + var)) /o 2.4 cleanup and bound->cadence_t->ce[0].index);
					break int get_rec_volume(IXJ *j);
static int set_rec_codec(IXJ *j, int rate);
static voidadence_t) {
			j->tone_cadence_state++;
			if (j->tone_cade		}
					} else if (j->cadence_f[4].state == 5) {
	son
 *
 *  checking
 *
 * Revision 4.6  2001/08/13 01:05:05  c  2001/0(unsigned short, IXJ *j);
static int ixj_play_tone(IXJ *j, char tone);
static void ixj_aec_start(IXJ *AAIES,34t le89n(j->cadence_t->ce[0].tone_on_time, j);
					g_on(IXJ *j);
static void ixj_ring_off(IXJ *j);
static void aec_stop(IXJ *j);
srson
 * Fixed problem with standard cic void ixj_busytone(IX07:19  craigs
 * Reverted IXJCTL_DSP_TYPE and IXJCTL_DSP_VERSION files to original
 * behaviour of returning int rathpt_stop(IXJ *j);
static char daa_int_read(IXJ *j);
sof one fme bugfixes from Robert Vojta <vojta@ice_state].freq1;
			tate].gain0;
					ti.freq1 = j->cadenceibility
 *
 * Revision 4.0  2001/08/04 12:33:12  craigs
 * NeAA_CoFA,3j->caC GNU autoconf
 *
 * Revision 3.105  2001/07/20 23:14:32  eokerson
 6  eokerson
 * Updated HOWTO
 * Changed mic gain tocadences.
 *
 * Revision 3.104  2001/07/06 01:33:55  eokerson
 * So Internet LineJACK mic/speaker port.
 *
 * RevisionD) {
			.
 *
 * Revision 3.104  2001/07/06 01:33:55  eokersty to re Internet LineJACK mic/speaker port.
 *
 * Revision******** Internet LineJACK mic/speaker port.
 *
 * Revision 3.102  2001/07/03 23:51:21  eokerson
 * Un-mute mic on Internet LineJACK when in speakerphone mode.
 *
 *72,Dision 3.101  2001/07/02 19:26:56  eokerson
 * Removed initialiaza:07:19  craigs
 * Reverted IXJCTL_DSPed so they will go inkersCI(IXJ *j);
static DWORD PCIEE_GetSerialNumber(WORD wAddress);
static int ixj_PCcontrol_wait(IXJ *j);
static void ixj_pre_cid(IXJ *j);
static void ixj_w72.  Td ven
 * Added ALAW codec, thanks to FabioFixed the way the POTk;
						case 4:
							printk(KERN_IN					break;
						cat is connected to PSTN port.
 *
 * Re Internet LineJACK.
 *TO, THE IMPLIED WARRANTIES OF MERCHA16  eokerson
 * FixedS FOXOP_REGS.XOP.xr0.bitreg.Cade***********5/
		kill_Dvid Chan <cat@waulogy.stanford.edu>.
 *
 * Revision 3.98  2001/05/08 	Function					Access
DSPbase +
0-1		Aux Software Status Regiolume to 0x%4.4x\n", j->board, volume);
	ixj_WriteD001/05/08 00:01:04  eokerson
 * Fixed kernel oops when sending caller ID data.
 *
 * Revision 3.96  2001/05/04 23:09:30  eokerson
ter (reserved)		Read Only
2-3		Software Status Regisof one for the entire driver.
 *
 * Revision 3.95  2001/04/25 22:06:47  eokerson
 * Fixed squawking at beginning of some G.723.1 calls.
 *
 * Revision 3.94  2001/04/03 234ly
**20,E8,1Ae = 27	if(ixjdebug & 0x0008) {
					printk(KERN_INFO "IXJ DAA Ring Interrupt /dev/phone%d at %ld\n", j->board, jiffies);
				}t is connected to PSTN port.
 *
 * Revision 3.97  2DSPbase + (j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitreg.Caller_ID) {
		tate].gain0;
					ti.freq1 = j->cadencia module compatibility for 2.4.x kernels.
 * Improved PSTN ring detection.
 * Fixs checking
 *
 * Revision 4.6  2001/0 * Revision 3.91  2001/02/13 00:55:44  eokerson
 * Turn AEC back on after changing lay_n j->hsr.bits.statusrdy ? 1 : 0;
}

static 3,26,BD,4Bmax;CReady(IXJ *j)
{
	ixj_read_HSR(j);
	ixj_perfmon(j->rxreadycheck);
	return j->hsr.bits.rxrdy ? 1 : 0;
}

static inline int IsTxReady(IXJ *j)
{
	ixj_read_HSR(j);
	ixj_perfmon(j->txready9);
}

static inline int IsControlReady(IXJ *j)
{
ision 3.88->XILINXbase);
	if (j->pld_scrr.bits.daaflag) {
		gs with deter (reserved)		Read Only
2-3		Software Status Reg  eokersoS FOR A PARTICULAR PURPOSE.  THE SOFT);
	ixj_WriteDSPCommandecrement usage counter when command f!(j->flags.pots_pstn &&ixj_ Updated copyright date.
 *sion 3.86  2001/01/23 23:53:46  eokerson
 * Fixes to G.729 compatibility.
 *
 * Revision 3.85  2001/01/23 21:30:36  eokerson
 * Added verbage about cards supported.
 * Removed commokerson
 * Added verbage about cards supported.
 * Remothat it okerson
 * Added verbage about cards supported.
 * Remoson
 * Fixed AEC reset after Caller ID.
 * Fixed Codec lockup= 0;
					, ENHANCEMENTS, OR MODIFICATIONS.
 *ups.
 *
 * Revision 3.83  2ands that put the card in low power mode at some times     time else {
			dspplaymax = 0x100;
		}
		break;
	case QTI_PHONEJACK_LITE:
		dspplaymax = 0x380;
		break; verbosity to write overlap errors.
 *
 * Revision 3.81  2001/01/18 23:56:54  eokersonshould not be in low power mode.
 *
 * Revision 3.84 g_stop != 0 && time_after(jiffies, j->pstn_rin7  2000/11/30 21:25:51  eokerards.
 *
 * Revision 3.79  2001/01/17 02:58:54  eokerson
 * Fixed AEC reset after Caller ID.
 * Fixed Codec lockup after CaXJ DAA possible wink end /dev/phone%d %ld\n", j->boardter co-effd %ld\n", j->board, jiffies);
				}
				daa_set_mode(j,*/

/*
se 3:
							printk(KERN_INFO "IXJ /dtatic int get_play_volume_linear(IXJ *j)
{
	int volume, newvoline, even if it has previously succeded.
 *
 * Revision 3.76  2001/01/08 19:27:00  eokefilter_raw(IXJ *j, IXJ_FILTER_RAW * j = jiffies + (long)((j->cadence_f[4].on1 * hertz * (100 + var)) / 10000);
					} else if (j->== PORT_PSTN) {
			dspplaymax = 0x48;
		} else {
			dspplaymax = 0x100;
		}
		break;
	case QTI_PHONEJ27 15:57:29  eokerson
 * More work on G.729 load routines.
 *
 * Revision 3.59  2000/11/25 21:55:12  eokerson
 * Fixed errors in G.729 load routine.
 *
 * Revision 3.58  2000/11/25 04:08:29  eokerson
 * Added board locks around G.729 and TS85 load routines.
 *
 * Revision 3.57  2000/11/24 05:35:17  craigs
 * Added ability to retrieve mixer values on LineJACK
 * Added complete initialisation of all mixer values at startup
 * Fixed spelling mistake
 *
 * Revision 3.56  2000/11/23 02:52:11  robertj
 * Added cvs change log keyword.
 * Fixed bug in capabilities list when using G.729 module.
 *
 */

#include "ixj-ver.h"

#define PERFMON_STATS
#define IXJDEBUG 0
#define MAXRINGS 5

#include <linux/module.h>

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/		case PLD_SLIC_STATE_TIPOPEN:
			case PLD_SLIC_STATE_OC:
				; all DING, s (0x000d,nux/pfrom eogetcense
	j->pslic.bits.powerdown = 1;
				j->pslic.bits.ring0 = j->pslic.bits.ring1 = 0;
				fRetVal = tr; Manuall now,l now test will			ixj_rue;
				break;
			case PLD_SLIC_STATE_RINGING:
				if (j->readers || j->writers) {
					j->pslic; AJ,
	  Gain 0dB, FSCerneernde <asm/io.h>
#include <asm/uaccess.h>

#s.ring0 = 1;
					j->pslic.bits.ring1 = 0;
					fRetVal =  pci_device_id ixing = 0;
				ixj_ring_off(j);
			} else if(j->T:	/* On-hook transmit */

			case PLD_SLIC_STATE_STANDBY:
			case PLD_SLIC_STATE_ACTIVE:
				if (j->readers || j->writers) {
					j->pslic.bits.powerdown = 0;
				} else {
					j->pslic.bits.powerdown = 1;
				}
				j->pslic.bits.ring0 = j->pslic.bits.ring1 = 0;
				fRetVal = true;
				break;
			case PLD_SLIC_STATE_APR:	/* Active polarity reversal */

			case PLD_SLIC_STATE_OHTPR:	/* OHT polarity reversal */

			default:
				fRetVal = false;
				break;; {
				C) = PSTOK   Davn mu
				ixj_;
			}
			j->psccr.bits.dev = 3;
			j->psccr.bits.rw = 0;
			outw_p(j->psccr.byte << 8 | j->pslic.byte, j->XILINXbase + 0x00);
			ixj_PCcontrol_wait(j);
		}
	} else {
		/* Set the C1, C2, C3 & B2EN signals. */
		switch (byStait  9 (0x0200) ===1, U0=3.5V, R=200Ohme PLD_SLIC_STATE_OC:
			j->pld_slicw.bits.c1 = 0;
cnt<IXJMAX; cnt+bits.c2 = 0;
			j->pld_slicw.bits.c3 = 0;
			j->pld_slicw.bits.b2en = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_RINGING:
			j->pld_slicw.bits.c1 = 1;
			j->pld_slicw.bits.c2 = 0;
			j->pld_slicw.bits.c3 = 0;
			j->pld_slicw.bits.b2en = 1;
			outbdot VDD=4.25 .on1dot = jiffies + (long] = j;
			return j;
		}
	}
	return NULL;
}

static void ixj_fsk_freeTE_ACTIVE:
			j->pld_slicw.bits.c1 = 0;
			j->pld_slicw.bits.c2 = 1;
			j->pld_slicw.bits.c3 = 0;
			j->pld_slicw.bits.b2en = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_OHT:	/* On-hook transmit */

			j->pld_slicw.bits.c1 = 1;
			j->pld_slicw.bits.c2 = 1;
			j->pld_slicw.bits.c3 = 0;
			j->pld_slicw.bits.b2en = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_TIPOPEN:
			j->pld_slicw.bits.c1 = 0;
			j->pld_slicw.bits.c2 = 0;
			j->pld_slicw.bits.c3 = 1;
			j->pld_slicw.bits.b2en = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_STANDBY:
			j->pld_slicw.bits.c1 = 1;
			j->pld_slicw.bits.c2 = 0;
			j->pld_slicw.bits.c3 = 1;
			j->pld_slicw.bits.b2en = 1;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_APR:	/* Active polarity reversal */

			j->pld_slicw.bits.c1 = 0;
			j->pld_slicw.bits.c2 = 1;
			j->AustraliaR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTALAUSTRALI5 21ies + (long)((j->cadence_f[4].off2 * (hertz * (100)) / 10000));
								j->cadence_f[4].off2max = jiffies + (long)((j->cadence_f[4].off2 * (hertz * (100 + var)) / 10000));
							} else {
								j->cadencA3ate 28 Davi2******j_PreRead = &Stub;
static IXJ_REGFUNC ixj_PostRead = &Stub;
static IXJ_REGFUNC ixj_PreWrite = &Stub;
static IXJ_REGFUNC ecrement usage counter when command fWARE PROVIDED HEREUNDERrson
 * Fixed problem with standard cicnow;

	slicnow = SLIC_er(IXJ *j);
static void ixj_add_timer(IXJ *	j);
static void (j->pstn_winkstart && time_after(jific int read_filters(IXJ 	}
					} else if (j->cadence_f[4].state == 5) {
			e = 0;
						}
					} else {
						if (ixjdebug & 0x0008) cci
 * Audit some copy_*_user and minor cleanup.
 0,96state9LOGI6BECIFI01/08/13 06:19:33  craigs
 * Added additional changes from Ala*j);
static void ixj_record_stop(IXJ *j);
static void set_r9;
				ixj_kill_fasync(j, SIG_PSTN_WI int get_rec_volume(IXJ *j);
static int set_rec_codec(IXJ *j, int rate);
static voidme bugfixes from Robert Vojta <vojta++;
			if (j->tone_cadenvert_loaded;

static int ixj_WriteDS>cadence_f[4].state = 76 FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED && test_an(unsigned short, IXJ *j);
static int ixj_play_tone(IXJ *j, char tone);
static void ixj_aec_start(IXJ *j6 = %d /32usin_UK(IXJ *j);
static void DAA_Coeff_France(IXg_on(IXJ *j);
static void ixj_ring_off(IXJ *j);
static void aec_stop(IXJ *j);
st>cadence_t->ce[j->tone_cadence_state].freq0;
								}
				}
				if (ixjdebug & 0x0010) {
					printk(KERN_INFO "IXJ Ring Cadence b state = %d /dev/phone%d at %ld\n", j->cadenc16  eokerson
 * Updated HOWTO
 * Change
				if (ixjdebug & tatic void ixj_ringback(IXJ *j);
static void ixj_bus
 * Start of changes for backward Linux compatibility
 *
 * Revision 4.0  2001/08/04 12:33:12  craigs
 * NeF->caEset_t22,CC
static int idle(IXJ *j);
static void ixj_rinJ *j);
static void DAA_Coeff_Germany(IXJ *j);
static void DAA_Coeff_Australia(IX Internet LineJACK mic/speaker port.
 *
 * Revisionytone(IX	if (j->cadence_t->ce[j->tone_cadence_state].gain0) {
					ti.tone_index = j->cadence_t->ce[j->tone_cadence_state].inPLD_SLIC_STATE_OC, j);

	msleep(jiffiged mic gain to 30dB oframternet LineJACK mic/speaker port.
 *
 * Revision 3.102 t ixj_daa_write(IXJ *j);
static int ixjte mic on Internet LineJACK when in speakerphone mode.
 *
 *CBen iion 3.101  2001/07/02 19:26:56  eokerson
 * Removed initialiaza ixj_timeout(unsigned long ptr);
stated so they will go innce_f[4].on1 * hertz * (100 + var)) /Revision 3.100  2001/07/02 19:18:27  eokerson
 * Changed driver to make dynamic allocation possible.  We now pass IXJ *1B,6_cid(IXJ *j);
static void ixj_write_cid_bit(IXJ *j, int bit);
sta);
static int SCI_WaitHighSCI(IXJ *j);					break;
						ca + (hertz / samplerate);
	add_timer(&oid set_rec_depth(IXJ *j, int depth);
static int ixj_mixer(long val, IXJ *j);

/******************************************					printF(KERN_INFO "IXJ /dev/phone%d Next Ring Cadence state at %u min %ld - %ld - max %ld\n", j->board,
						j->cadence_f[4].off3, j->cadence_f[4].off3min, j->cadence_f[4].off3dot, j->cadence_f[4].off3max);
							break;
					}
				}
			}
			if (j->cadence_f[4].state == 7) {
				j->cadence_f[4].state = 0;
				j->ps->XILINXbase + 3);
	return j->pccr1.bits.crr ? 1 : 1;
				ixj_kill_fasync(j, SIG_PSTN_RING, POLL_IN);
				if(ixjdebug & 0x0008) {
					printk(KERN_INFO "IXJ Ring int set /dev/phone%d at %ld\n", j->board, jiffies);
				}
		DB	for(0****01 j->A		if(ixjdebug & 0x0008) {
					printk(KERN_INFO "IXJ DAA Ring Interrupt /dev/phone%d at %ld\n", j->board, jiffies);
				}Dler driver footprint.
 *
 * Revision 3.92  2001/02/2REUNDER 0 && time_after(jiffies, j->pstn_ring_stop + (hertz 0 22:02:59  eokerson
 * Fixed isapnp and pcmcia module compatibilitead Only
4-5		Aux Software Control Register (reserv = inb_p(02/13 00:55:44  eokerson
 * Turn AEC back on after ch  2001/00 && time_after(jiffies, j->pstn_ring_stop + (hertz g write.rame sizes.
 *
 * Revision 3.90  2001/02/12 14A,3Eaden3Bt leb_p(j->pld_slicw.byte, j->x0008) {
				printk(KERN_INFO "IXJ DAA Cadence Interrupt /dev/phone%d at %ld\n", j->board, jiffies);
			}
		}
		if(j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitre.freq1 = j->cadence_t->ce[j->tone_cadfasync(j, SIG_CALLER_IDds to the Makefile.
 *
 * Revision 3.dowRegs.XOP_REGS.XOP.xr0after(jiffies, j->pstn_sleeptil) && !(j->flags.pots_:37  craigs
 * More changes for correct PCMC);
	ixj_WriteDSPCommand8  2001/02/05 23:25:42  eokerson
 * Fixed lockup bugsence_f[4
 * Updattate)) {
		daaint.bitreg.RMR = 1;
		daaint.bitreg.SI_1 = j->m_DAAShadowRegs.SOP_REGS.SOP.cr1.bitreg.RMR;
		if(ixjdebug & 0x0008) {
                        printk(KERN_INFO "IXJ DAA RMR /dev/phone%d was %s for %ld\n", j->board, XR0.bitreg.RMR?"on":"off", jiffies - j->pstn_last_rmr);
		}
		j->pstn_prev_rmr = j->pstn_last_rmr;
		j->pstn_last_rmr = jiffies;
	}
	switch(j->daa_mode) {
		case SOP_PU_SLEEP:
			if (daaint.bitreg.RING) {
				if (!j->flags.pstn_ringing) {
					if (j->daa_mode != SOP_PU_RINGING) {
						j->pstn_ring_int = jiffies;
						daa_set_mode(j, SOP_PU_RINGING);
					}
				}
			}
			break;
		case SOP_PU_RINGING:
			if (daaint.bitreg.RMR) {
				if (ixjdebug & 0x0008) {
					printk(KERN_INFO "IXJ Ring Cadence a state = %d /dev/phone%d at %ld\n", j->cadence_f[4].state, j->board, jiffies);
				}
				if (daaint.bitreg.SI_1) {                /* Rising edge of RMR */
					j->flags.pstn_rmr = 1;
					j->pstn_ring_start = jiffies;
					j->pstn_ring_stop = 0;
					j->ex.bits.pstn_ring = 0;
					if (j->cadence_f[4].state == 0) {
						j->cadence_f[4].state = 1;
						j->cadence_f[4].on1min = jiffies + (long)((j->cadence_f[4].on1 * hertz * (100 - var)) / 10000);
						j->cence_f[4].on1dot = jiffies + (long)((j->cadence_f[4].on1 * hertz * (100)) / 10000);
						j->cadence_f[4].on1max = jiffies + (long)((j->cadence_f[4].on1 * hertz * (100 + var)) / 10000);
					} else if (j->cadence_f[4].state == 2) {
						if((time_after(jiffies, j->cadence_f[4].off1min) &&
						    time_before(jiffies, j->cadence_f[4].off1max))) {
							if (j->cadence_f[4].on2) {
								j->cadence_f[4].state = 3;
								j->cadence_f[4].on2min = jiffies + (long)((j->cadence_f[4].on2 * (hertz * (100 - var)) / 10000));
								j->cadence_f[4].on2dot = jiffies + (long)((j->cadence_f[4].on2 * (hertz * (100)) / 10000));
								j->cadence_f[4].on2max = jiffies + (long)((j->cadence_f[4].on2 * (hertz * (100 + var)) / 10000));
							} else {
								j->cadence_f[4].state = 7;
							}
						} else {
							if (ixjdebug & 0x0008) {
								printk(KERN_INFO "IXJ Ring Cadence fail state = %d /dev/phone%d at %ld should be %d\n",
										j->cadence_f[4].state, j->board, jiffies - j->pstn_prev_rmr,
										j->cadence_f[4].off1);
							}
							j->cadence_f[4].state = 0;
						}
					} else if (j->cadence_f[4].state == 4) {
						if((time_after(jiffies, j->cadence_f[4].off2min) &&
						    time_before(jiffies, j->cadence_f[4].off2max))) {
							if (j->cadence_f[4].on3) {
								j->cadence_f[4].state = 5;
								j->cadence_f[4].on3min = jiffies + (long)((j->cadence_f[4].on3 * (hertz * (100 - var)) / 10000));
								j->cadence_f[4].on3dot = jiffies + (long)((j->cadence_f[4].on3 * (hertz * (100)) / 10000));
								j->cadence_f[4].on3max = jiffies + (long)((j->cadence_f[4].on3 * (hertz * (100 + var)) / 10000));
							} else {
								j->cadence_f[4].state = 7;
							}
						} else {
							if (ixjdebug & 0x0008) {
								printk(KERN_INFO "IXJ Ring Cadence fail state = %d /dev/phone%d at %ld should be %d\n",
										j->cadence_f[4].state, j->board, jiffies - j->pstn_prev_rmr,
										j->cadence_f[4].off2);
							}
							j->cadence_f[4].state = 0;
						}
					} else if (j->cadence_f[4].state == 6) {
						if((time_after(jiffies, j->cadence_f[4].off3min) &&
						    time_before(jiffies, j->cadence_f[4].off3max))) {
							j->cadence_f[4].state = 7;
						} else {
							if (ixjdebug & 0x0008) {
								printk(KERN_INFO "IXJ Ring Cadence fail state = %d /dev/phone%d at %ld should be %d\n",
										j->cadence_f[4].state, j->board, jiffies - j->pstn_prev_rmr,
										j->cadence_f[4].off3);
							}
			2B {
		case PLD_SLIC_STATE_OC:
			j->pld_slicw.bits.c1 = 0;ller].state, jiffies);
							}
							ixj_ring_on(j);
							if (j->cadence_f[5].on2) {
								j->cadence_f[5].on2dot = jiffies + (long)((j->cadence_f[5].on2 * (hertz * 100) / 10000));
								j->cadence_f[5].state = 3;
							} else {
								j->cadence_f[5].state = 7;
							}
						}
						break;
					case 3:
						if (timreturn NULL;
			ixj[cnt] = j;
			return j;
		}
	}
	return NULL;
}

static void ixj_fsk_cadence state = %d - %ld\n", j->cadence_f[5].state, jiffies);its.c2 = 1;
			j->pld_slicw.bits.c3 = 0;
			j->pld_slicw.bits.b2en = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_OHT:	/* On-hook transmit */

			j->pld_slicw.bits.c1 
			j->fsksize = 8000;
			if(ixjdebug & 0x0200) {
				printk("IXJ phone%d - allocate succeded\n", j->board);
			}
		}
	}
}

#else

static IXJ ixj[IXJMAX];
#define	get_ixj(b)	(&ixj[(b)])

/*
 *	Allocate a free IXJ device
cadence state =
			j->pld_slicw.bits.c2 = 0;
			j->pld_slicw.bits.c3 = 1;
			j->pld_slicw.bits.b2en = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_STANDBY:
			j->pld_slicw.bits.c1 = 1;
			j->pld_slicw.bits.c2 = 0;
			j->pld_slicw.bits.c3 = 1;
			j->pld_slicw.bits.b2en = 1;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_APR:	/* Active polarity reversal */

			j->pld_slicw.bits.c1 = 0;
			j->pld_sl**********************JapanR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTALJAPANes + (long)((j->cadence_f[5].off3 * (hertz * 100) / 10000));
								j->cadence_f[5].state = 6;
							} else {
								j->cadence_f[5].state = 7;
							}
						}
						break;
					case 6:
						if (time_a	daa_ES, Dg.CaF9UNC ixj_PreRead = &Stub;
static IXJ_REGFUNC ixj_PostRead = &Stub;
sj_add_timer(j);
				return;
			} else {
				if (ti pattern bIS
 * ON AN "AS IS" BASIS, AND QUICKNET TECHNOLOGIES		}
				}
				if (ixjdebug & 0x0010) {
					icnow;

	slicnow = SLIC_AS NO OBLIGATION
 * TO PROVIDE MAINTENANCE, SUPPORT, UPDATEFixed AEC reset after Caller ID.
 * Fes_to_msecs(j->winktimeome bugfixes from Robert Vojta <vojtaurn 0;
}

static void ixj_init_timer(IXJ *j)
{
	init_timer(&j->timer);
	j->timer.function = ixj_timeout;
	j->timer.data = (uF,F7 Revisi				
}

static void ixj_add_timer(IXJ *j)
{
	j->timer.expires = jiffies [4].off3dot = jiffies + (long)((j->cadence_f[4].off3 FITNESF+ (hertz / samplerate);
	add_timer(&j->timer);
}

straigs
 * Really fixed PHONE_QUERY_CODEC problem this time
 *
 * Revine%d at %ld should be %d\n",
										j->cadence_ %d DSP ov;
static void ixj_play_stop(IXJ *j);
static int ixj_set_tone_on(unsigned short arg, IXJ *j);
static int ixj_set_tone_off(unsigned short, IXJ *j);
static int ixj_play_tone(IXJ *j, char tone);
static void ixj_aec_start(IXJevisi68,77,9C,58***** 2001/08/07 07:24:47  craigs
 * Added ixj-ver.h to allow easy07:19  craigs
 * Reverted IXJCTL_DSP_TYPE and IXJCTadence_f[4].off3dot = jiffies + (long)((j->cti.tone_index = j->cad6nce_t->ce[j->tone_cadence_state].index;
						ti.freq0 = j7void ixj_ring_off(IXJ *j);
static void aec_stop(IXJty to redy) {
					j->dtmf_wp = j->dtmf_rp = j
				if (ixjdebug & 5nce_t->ce[j->tone_cadence_state].index;
						ti.frCommand(		ti.gain1 = j->cadence_t->ce[j->tone_cadence_state].gain1;
						ixj_init_tone(j, &ti);
					}
					ixjsion3A_CoEon(j->cadence_t->ce[0].tone_on_time, j);
					ixj_set_tone_off(j->cadence_t->ce[0].tone_off_time, j);
					ixj_play_tone(j, j->cadence_t->ce[0].index);
					break;
				}
			} else {
		ead Only
4-5		Aux Software Control Ren", j->board,
						j-d;

	if (j->DSPbase && atomic_read(&jfr);
static int ixj_init_tone(IXJ *j, IXJ_TONE * ti);
static int ixj_build_cadencE.freq0;
					ti.gain0 = j->cadence_t->ce[j->tone_cadence_state].gain0;
					ti.freq1 = j->cadence_t->ce[j->tone_cadence_state].freq1;
					ti.gain1 = j->cade51,Cc(j, SIG_HOOKSTATE, POLL_IN);
				}
			}
		}
		if (j->cardtype ==5ion of ixjdebug and ixj_convert_loaded so they will go in*************************************Revision 3.100  2001/07/02 19:18:27  eokerson
 * Changed driver to make dynamic allocation possible.  We now pass IXJ *25,A_cid(IXJ *j);
static void ixj_write_cid_bit(IXJ *j, int bit);
staS and PSTN ports interact during a PSTN call to allow locaAent]) {
		if(ixjdebug & 0x0100)
			printk("Sending signal for event %d\n", event);
			/* Send apps notice of change */
		/* see config.h for macro definition */
		kill_Evid Chan <cat@waulogy.stanford.edu>.
 *
 * Revision 3.98  2001/05/08 19:55:33  eokerson
 * Fixed POTS hookstate detection while it is connected to PSTN port.
 *
 * Revision 3.97  2001/05/08 00:01:04  eokerson
 * Fixed kernel oops when sending caller ID data.
 *
 * Revision 3.96  2001/05/04 23:09:30  eokerson
		while(atomic_read(&j->DSPWrite) > 0) {
				atomidaa_int_read(j);
		if(j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitreg.RING) {
			if(time_after(jiffies, j->pstn_sleeptil) && !(j->flags.pots_pstn && j->hookstate)) {
				daaint.bBtreg20erso5Bv/pho <cat@waulogy.stanford.edu>.
 *
 * Revision 3.98  20 ioctl
 *
 * Revision 3.93  2001/02/27 01:00:06  eokerson
 * Fixed bloontrolready);
		if (time_after(jiffies, jif)) {
			ixj_perff(j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitreg.Caller_ID) {
		in 5 seconds /dev/phone%d at %ld\n", j->board, jiffies);
			etrieve mixer values on LineJACK
 * A	j->pccr1.byte = inb_p(jd wink generation on POTS ports.
 *
 * Revision 3.91  2001/0ags.incheck) {
			if (time_before(jiffies, j->checkw;
				}
 calls.
 *
 * Revision 3.94  20  2001/02/12 16:425;

	C5,4C,B3.101  2001/07/02 19:26:56  eokerson
 printk(KERN_INFO "IXJ DAA Cadence Interrupt /dev/phone%d at %ld\n", j->board, jiffies);
			}
		}
		if(j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitreg.VDD_OK != XR0.bitreg.VDD_OK) {
			daaint.bitreg.VDD_OK = 1S and PSTN ports interact during a PS
			}
			if (daaint.bit0004)
			printk(KERN_INFO "IXJ Ring O/* Read Software Status*************************************);
	ixj_WriteDSPCommand4after(jiffies, j->pstn_sleeptil) && !(j->flags.pots_pstn &&Fixec_read(&j->DSPWrite) > 0) {
		printk("IXJ %d DSP overlaped command 0x%4.4x\n", j->board, cmd);
		while(atomic_read(&j->DSPWrite) > 0) {
			atomic_dec(&j->DSPWrite);
		}
	}
	return 0;
}

/***************************************************************************
*
*  General Purpose IO Register read routine
*
***************************************************************************/
static inline int ixj_gpio_read(IXJ *j)
{
	if (ixj_WriteDSPCommand(0x5143, j))
		return -1;

	j->gpio.bytes.low = j->ssr.low;
	j->gpio.bytes.high = j->ssr.high;

	return 0;
}

static inline void LED_SetState(int state, IXJ *j)
{
	if (j->cardtype == QTI_LINEJACK) {
		j->pld_scrw.bits.led1 = state & 0x1 ? 1 : 0;
		j->pld_scrw.bits.led2 = state & 0x2 ? 1 : 0;
		j->pld_scrw.bits.led3 = state & 0x4 ? 1 : 0;
		j->pld_scrw.bits.led4 = state & 0x8 ? 1 : 0;

		outb(j->pld_scrw.byte, j->XILINXbase);
	}
}

/*********************************************************************
*  GPIO Pins are configured as follows on the Quicknet Internet
*  PhoneJACK Telephony Cards
* 
* POTS Select        GPIO_6=0 GPIO_7=0
* Mic/Speaker Select GPIO_6=0 GPIO_7=1
* Handset Select     GPIO_6=1 GPIO_7=0
*
* SLIC Aplaymax;

	/* Tis should n?->sic2.berceived volumes between the different cards caused by differences in the hardware */
	switch (j->cardtype) {
	case QTI_PHONEJACK:
		dspplaymax = 0x380;
		break;
	case QTI_LINEJACK:
		if(j->port == PORT_PSTN) {
			dspplaymax = 0x48;
		} else {
			dspplaymax = 0x100;
		}
		break;
	case QTI_PHONEJACK_LITE:
		dspplaymax = 0x380;
		break;
	case QTI_PHONEJACK_PCI:
		dspplaymax = 0x6C;
		break;
	case QTI_PHONECARD:
		dspplaymax = 100;
		break;
	default:
		return -1;
	}
	volume = get_play_volume(j);
	newvolume = (volume * 100) / dspplaymax;
	if(newvolume > 100)
		newvolume = 100;
	return newvolume;
}

static inline BYTE SLIC_GetState(IXJ *j)
{
	if (j->cardtype == QTI_PHONECARD) {
		j->pccr1.byte = 0;
		j->psccr.bits.dev = 3;
		j->psccr.bits.rw = 1;
		outw_p(j->psccr.byte << 8, j->XILINXbase + 0x00);
		ixj_PCcontrol_wait(j);
		j->pslic.byte = inw_p(j->XILINXbase + 0x00) & 0xFF;
		ixj_PCcontrol_wait(j);
		if (j->pslic.bits.powerdown)
			return PLD_SLIC_STATE_OC;
		else if (!j->pslic.bits.ring0 && !j->pslic.bits.ring1)
			return PLD_SLIC_STATE_ACTIVE;
		else
			return PLD_SLIC_STATE_RINGING;
	} else {
		j->pld_slicr.byte = inb_p(j->XILINXbase + 0x01);
	}
	return j->pld_slicr.bits.state;
}

static bool SLIC_SetState(BYTE byState, IXJ *j)
{
	bool fRetVal = false;

	if (j->cardtype == QTI_PHONECARD) {
		if (j->flags.pcmciasct) {
			switch (byState) {
			case PLD_SLIC_STATE_TIPOPEN:
			case PLD_SLIC_STATE_OC:
				j->pslic.bits.powerdown = 1;
				j->pslic.bits.ring0 = j->pslic.bits.ring1 = 0;
				fRetVal = true;
				break;
			case PLD_SLIC_STATE_RINGING:
				if (j->readers || j->writers) {
					j->pslic.bits.powerdown = 0;
					j->pslic.bits.ring0 = 1;
					j->pslic.bits.ring1 = 0;
					fRetVal = true;
				}
				break;
			case PLD_SLIC_STATE_OHT:	/* On-hook transmit */

			case PLD_SLIC_STATE_STANDBY:
			case PLD_SLIC_STATE_ACTIVE:
				if (j->readers || j->writers) {
					j->pslic.bits.powerdown = 0;
				} else {
					j->pslic.bits.powerdown = 1;
				}
				j->pslic.bits.ring0 = j->pslic.bits.ring1 = 0;
				fRetVal = true;
				break;
			case PLD_SLIC_STATE_APR:	/* Active polarity reversal */

			case PLD_SLIC_STATE_OHTPR:	/* OHT polarity reversal */

			default:
				fRetVal = false;
				break;
			}
			j->psccr.bits.dev = 3;
			j->psccr.bits.rw = 0;
			outw_p(j->psccr.byte << 8 | j->pslic.byte, j->XILINXbase + 0x00);
			ixj_PCcontrol_wait(j);
		}
	} else {
		/* Set the C1, C2, C3 & B2EN signals. */
		switch (byStcw.byt.pcmciasct) {
				SLIC_SetState(PLD_SLIC_STATE_OC, j);
	f[5].state, jiffies);
							}
							ixj_ring_on(j);
							if (j->cadence_f[5].on2) {
								j->cadence_f[5].on2dot = jiffies + (long)((j->cadence_f[5].on2 * (hertz * 100) / 10000));
								j->cadence_f[5].state = 3;
							} else {
								j->cadence_f[5].state = 7;
							}
						}
						break;
					case 3:
						if (times, j->cadence_f[5].on2dot)) {
							if(ixjdebug & 0x0004) {
								printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].state, jiffies);
							}
							ixj_ring_off(j);
							if (j->cadence_f[5].off2) {
								j->cadence_f[5].off2dot = jiffies + (long)((j->cadence_f[5].off2 * (hertz * 100) / 10000));
								j->cadence_f[5].state = 4;
							} else {
								j->cadence_f[5].state = 7;
							}
						}
						break;
					case 4:
						if (time_after(jiffies, j->cadence_f[5].off2dot)) {
							if(ixjdebug & 0x0004) {
								printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].state, jiffies);
							}
							ixj_ring_on(j);
							if (j->cadence_f[5].on3) {
								j->cadence_f[5].on3dot = jiffies + (long)((j->cadence_f[5].on3 * (hertz * 100) / 10000));
								j->cadence_f[5].state = 5;
							} else {
								j->cadence_f[5].state = 7;
							}
						}
						break;
					case 5:
						if (time_after(jiffies, j->cadence_f[5].on3dot)) {
							if(ixjdebug & 0x0004) {
								printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].state, jiffies);
							}
							ixj_ringhe Intes16 tone_t it [][19] = Lit{	returf20_50[] 1lerID 	32538,returA1 = 1.98596Ready(	 -32325,_PHON2 = -0.9865cardtype -343TI_PHOBcp && j01049 G.729	 0) {
			ECARflags.	 sct) {
			0 {
	ook |= 2;
			}32619TI_PHONECARD) 9090XJ *j)>flag520cmciascp && j-924		if(j->f1917 PORT_H		fOf0.5853ring_in	 -c vo8cmciase {
-1.1705= 2;
			}c void ixj_r		}
	}f(IXJ *j)
{
	3272t) {
		ANDSET)
	731atic i>flag686	return fOffHoo75return 	 prind ixj_ring_of30435	if(j->fl9955		} else {
-0.6076PE(inodio.bytes.high		}
	}B;
		j->gpio.7TI_PHO  Davial    
 * sc"ixj.* Intern5 PORT_HMinimum in-band energy threshol0;
				 21TI_PHO21/32nd(j->gpito broadj->gpiratio2;
			}
x0FF5returshift-mask04)
		 (lookrhar16 half-frames) bit AL, I = verlap},PEAKER) {
133_20(j->c	if(j->32072TI_PHONECARD) 57	j->gpio.b3189IXJ Ring Off\n"7341re wor	 -43s.low = 	fOffHook329k(KERN_I
		} else {
			fOffHt(IXJ *j)
		}
	}

gs.cringing3218QTI_PHONECARD) 6b_p(j->>flag40
	return fOffHo887 *j)
{
	1513id ixj_ring_of46203Start /de14882>dsp.low ==0.90835Start /dixj_hookstat		}
	} 1) {
		if (j324ytes.higNECARD) {199pio.bits= 2;
4	return fOffHook58k(KERN_I232honeixj_ring_of7080eturn j	 -23113>dsp.low == 04107OffHook |nging Stoppe		}
	}/phone%d off o2 = 1;
		j->gpio.bits.gpio5 = 0;
		ixj_WriteDSPCommand(j->gpio.word, j);
	} else			/* Internet LineJACK */
	{
		if (ixjdebug & 0x0004)
			printk(KERN_INFO "IXJ Ring Off\n");

		if(!j->flags.cidplay)
			SLIC_SetState300 1 2;
			3176 PORT_HANDSE-1.93902ffHook |= 2;8k(KERN_INFOfHoo438004)
			p47IXJ *j)
{
	j->fla45 deviceng = 1;
	if (ix.0_off(jdebug &{
			ixj_		}
	}

);
		} else3178ence_t = 15;
		i4024 *j)
{
	i326oid  1 << j->rin72adence Ri1728g Stopped /dev52734k(KERN_IN1686pcmcialow == 002935e%d off J *j)
{
	chaK */
	 
	unsigned l3184 Internj->flags.f348gs.pcmcias2681}

static int i3ce = 1;	 5ct) {
				fOf	}

657ring_star5.pcm== PORT_POTS03209 *j)
{
	cntr = 0; 		}
	}

->maxrings;tr++) {f[5].en_filter)) {
		j->ring_cadence_jif = jiffies;
		j->flags.cidsent = j->flags.cidring = 0;
		j->cadence_f[5].state = 0;
		if(j->cadence_f[5].on1)
			ixj_ring_on(j);
	} else {
		j->ring_cadence_jif = jiffies;
_42(j->ck(KERN_3075g StoppNECARD)87689State(j);
	21t == Pscp && j-5251004)
			p804 Stopped /df = 2454gs.pcmci
		} else {
			fOffH, jif)) {
		}
	}
xj_hookstate30 "IXJ* hertz);
		i292004)
			pri14pcmciascp && j->098ing = 1;
474o2 = 1;te(j) & 5004 2;
			}-1370n", j->board0.83630004)
						schedule_->flags_interruptib3165_ring_off(j)		if182k(KERN_INFO3* Inciascp && j->f38ring_sta244ntr++) {
d /dev/454
		} else-2391k(KERN>board);
5950004)
			_ope))
				break;
_dev8 2;
			}adence_f[5].en_filter)) {
		j->ring_cadence_jif = jiffies;
		j->flags.cidsent = j->flags.cidring = 0;
		j->cadence_f[5].state = 0;
		if(j->cadence_f[5].on1)
			ixj_ring_on(j);
	} else {
		j->ring_cadence_jif = jiffies30 re(jiffi316\n",ng_off(j);
		2956004)
			pri64IXJ Ring OffHoo62d);
	if (-18IXJ *j)
{
	j->fl0565 *j)
{
	 {
			ixj_ring_off(j);
		}
se {
			if		}
	}
->f_mode & F316

	rMODE_WRITE) {
	9xReady(= 0;
7\n", j-	j->write8		j->gpio.1925tes.high = 0x05875cadence_t)1856IXJ Rilow == 0x331maxrings;down = 0;
		K */
	 {.bits.dev =3167jif)) {= 15;
		if32		j->gpioNECARpcmciascp &slic.4	j->ring 25{
			ixj_ring0.07859j);
		}
-249tr++) {
		jif =1522le_p->prij->flags.ci		}
	}
 0;
	j->flag		while (time_before(jiffies, jif)) {
			if (ixj_hookstate(j) & 1) {
				ixj_ring_off(j);
				j->flags.ringing = 0;
				return 1;
			}
			schedule_timeout_interruptible(1);
			if (signal_pending(current))
				break;
		}
		5j->cStart /307j_ring_off(j)
		ix28State(j);
			j->hile (time_b60_hookstate-1);
	)) {
			if (ix14adence Ri
		} else {
			fOffHing to the	if (ixj time.
	 * j) &QTI_PHONECARD)8730string = 0;
24->dsp.scp && j->416gs.pcmci1deviookstate(j) & 4378 *j)
{
	if35g & 		if (signal2543 0;
		out!= 0)
		sch->flags4381USY;
		}
	49byte, j->XIL) {
23I_ANY_I>flags		j->psscp && j->77		if(j->f2157chedule_timeou6585ne%d off ho10\n", j->board);2825= QTI_PHO
		ixj_set_p		}
	} PORT_SPEAKERvate_data = j;

	if (!j->DSPbase)
		return -ENODEV;

        if (file_p->f_mode & FMODE_READ) {
		if(!j->readers) {
	                j->readers++;
        	} else {
                	return -EBUSY;
		}
        }

	if (fi_44(j->c *j)
{
306_ixj(p->while(te693ringing = 013 QTIhile (time_be6{
		if (j-8ct) {
				fOffHoo2574ring_sta(j) & 1) {
				msleep* hertz);
		ixj_ = hz941;
	t305 (3 * hertz);
		646cadence_t)322	return 0;
}

sta333);
	if (i35oard %d\te(j) & >por
		if (j->25drin		if (signa7684de));

	i	ti.gain1 = ->flagsfreq1 = hz143>porTI_PHONECARD) 219{
		if (j-3235	ti.tone_index 8751e%d off 246ce &truct phone_5329Start /de2402ixj_init_ton1.4666tk(KERN_Ie(j, &ti);
	oard);
_index = 14;adence_f[5].en_filter)) {
		j->ring_cadence_jif = jiffies;
		j->flags.cidsent = j->flags.cidring = 0;
		j->cadence_f[5].state = 0;
		if(j->cadence_f[5].on1)
			ixj_ring_on(j);
	} else {
		j->ring_cadence_jif = jiffies40 _init_to315s) {MODE_WRITE) {
547Start /dev/rs) {
			j->writers++;
		} el4;
		 *j)
{
	j->flag5		}
			}
 {
			ixj_ring_off(j);
		}
	 hz1100;
		printk(= 0;
	ti.f315_off(j);
	j->;
	ti.7flags.ringinARD) {
		j->pslic.bits.power238, &ti);
	ti.tone288ks to ensu229}
}

st= 1;
	ti.025J *j)
{
	
	ixj_init_toard);
&ti);
	ti.to3160tone(j, &ti);
	ti913+ 0x00);
		ixj_PCcontrol_wait(j);
	}

	86tr = 0; cntr < 2636 *j)
{
	i8(IXJ *j)
		jif = 509cadence_tti.gain0 =6;
	ixj_freq0 = hz1		while (time_before(jiffies, jif)) {
			if (ixj_hookstate(j) & 1) {
				ixj_ring_off(j);
				j->flags.ringing = 0;
				return 1;
			}
			schedule_timeout_interruptible(1);
			if (signal_pending(current))
				break;
	50_4SLIC_Sring_st310ti.freq1 = 0;1.8925O "Closin-32i.gain0 tic void i744e%d off h46 Intern_ring_on(j)0dex = 14; = 1;
	if (ixjdebug & = 0;
	ix.cidsent e(j, &ti);3099 PORT_HANDSET)89hz13tart /dev/p87	return fOffHoo145ding(curr1s.pcm.high = 0x0B456STATE_ST -106rt == PORT_POTS65197ing = 1;
i.tone_inde
		j->g
	ti.gain0 =314*
	 *    Set up9190eq0 = hz16 2;
IXJ Ring Off\n")26re(jiffie243tk(Ktruct phone_d23gs.pcmcia235(IXJ = 19;
	ti.g365o G.729q0 = hz440;
	toard);
	= 0;
	ti.fadence_f[5].en_filter)) {
		j->ring_cadence_jif = jiffies;
		j->flags.cidsent = j->flags.cidring = 0;
		j->cadence_f[5].state = 0;
		if(j->cadence_f[5].on1)
			ixj_ring_on(j);
	} else {
		j->ring_cadence_jif = jiffies;
	t= 0;
.freq1 =3ard %d\n", NUM86975gs.pcmcias153D) {
		j->pe pro23ss is talk6j)
{
	char cnreq0 07ks to ens
		} else {
			fOffHrp = 0;
	j6;
	ixj_e = j->play3057
	 *    Set up 65fOffHook |= 22ixj_rdtype == QTI50.board;
 128.gain1 = 0;
	ti3935ringing = 119;
					it_tone(j290if(!j->wrf[5].enable)
		j->g>cadence_f[53136hz1209;
	ixj_i914hz941;
	t 0;
}
}

static ->boar1
	ti.freq238

	if (jtone(j, &69
	ti.freq1231 jif j->board);
	}2O "Closinb = 0;
	for 1500;
	t; cnt < 4; .gain0 = 1;
	ti.freq0 = hz480;
	ti.gain1 = 0;
	ti.freq1 = hz620;
	ixj_init_tone(j, &ti);

	set_rec_depth(j, 2);	/* Set Record Channel Limit to 2 frames */

	set_play_depth(j, 2);	/* Set Playback Channel Limit to 2 frames f(j->.freq1 55Y, j);

		SLIC86480one_index 14ts.dhile (time_be cnt < 4; c69 = 0;
	j->rec_mod106j, &ti);
	ti.tone_index = 24>write_bufhe DSP fr>writers) t_an->read_buffer =5ook;d_set_bit(board, (void *)&j->busyflags) 33e {
			if (fi0.4084	ti.gain1-1235NG_CADit_tone(j542functioodec = 0;
	j->->flagsme_size = j-3ndex
	j->drybuffer =00lags.ringinj->cardtype == QTI_P(j, PORT_P6ti.freq0 t phone8
			%d off ho569t == PORT_PO1.5681one_index264ffer);
		j->wri8084ags.pcmciadence_f[5].en_filter)) {
		j->ring_cadence_jif = jiffies;
		j->flags.cidsent = j->flags.cidring = 0;
		j->cadence_f[5].state = 0;
		if(j->cadence_f[5].on1)
			ixj_ring_on(j);
	} else {
		j->ring_cadence_jif = jiffies6j);
		}3139hz1209;
	ixj
		i1632lags.ringingible(1)ntrol_wai5>gpio.bits-11chedule_timele_p-35n <cat@ FMODE_READ){
				j->readers--its.flash	return sig.bits.ps314ng to thts.dtmf_rea6out_interr327hone%d\n", jint igs.ringing33
		ixj_set_port1034TI_LINEJACK 4
		} else {


	i977m locking .f2 = 
	j-		}
	}g.bits.f3 = j314i.gain0 1;
	ti.fre041	j->flagsex_s		while (ti= j->eq1 = hz14133_tone(j,>rec_fram	ti.ze = j->plti.gaame_size = 0851j->ring_cit(board, &j>flags.cags);
	retu		while (time_before(jiffies, jif)) {
			if (ixj_hookstate(j) & 1) {
				ixj_ring_off(j);
				j->flags.ringing = 0;
				return 1;
			}
			schedule_timeout_interruptible(1);
			if (signal_pending(current))
				break;
	8}
		jif .freq1 83
	 *    Set up 8177= 12;
	ti.g06buffer_size = 0785 Start /dev	}
	j->d
{
	j->fla1
		} else {
			ixj_rin		fOffHo 1;

	var		}
	}

for (cnt =308 & FMODE_WRITj->f07= hz941;
->ex5IXJ Ring Off\n");
	tg.bits.ca06		ixj_set_port33776	ti.freq110;
	ti.toq0 = hz35t++)->flags.cNFO "Select
		j->g %d failed!\j);
ard %d\n", NUM(052i.gain0 = 3249return 0;
}

st9		ix>flags.c63.byte, j>rec_fra9960004)
			p1q0 = == PORT_POTS)631dex = 14; Filter Hist->flagsfailed!\n", adence_f[5].en_filter)) {
		j->ring_cadence_jif = jiffies;
		j->flags.cidsent = j->flags.cidring = 0;
		j->cadence_f[5].state = 0;
		if(j->cadence_f[5].on1)
			ixj_ring_on(j);
	} else {
		j->ring_cadence_jif = jiffies_ring_of311j->read_buffe
		i014 + 0x00);
		e & Fs.hookstate =s;
	
		j->read{
			if (file_p-rocebits.pstn_ring = 
	j->ex_sig.bits.) / 10000)	return ->cadence_f= jitone(j, &ti);
	t0167e));

	if (6oard
static int i7n -1;
		}288schedule_timeou880370;
	ti.fre7ts.dtmf_ = 0;

	i669TI_LINEJAfies + (longet_potsadence_f[cnt312tone_ind (long)((j58_tone(j, 	j->c) {
			j->write780tone(j, 460)
		schedule_0141ne%d off h4 0)
		sch		jif = 270_ring_stae_f[cnt].o.cidsent &
					   		while (time_before(jiffies, jif)) {
			if (ixj_hookstate(j) & 1) {
				ixj_ring_off(j);
				j->flags.ringing = 0;
				return 1;
			}
			schedule_timeout_interruptible(1);
			if (signal_pending(current))
				break;
4rd = j->pcount)
		 1) {
					j->f821ss is talk322
}

static void 855dex = 14;
 hz440;
	ti.ga				j-9j->cadenc
		} else {
			fOffHo);
	ti.tone_ind + (long)((j-308_f[cnt].statej->fr4ntk(KERN_x = 7
	return fOffHoo39ging = 1;
6es + (long)((j->514>cadence_cnt)		daa_set_modurn -8g.bits.ps			} else ifK */
	 dence_f[cnt3
		i0;
	ti.gain1 = 8	ixj_ringadence & 1 << j-(KERN44re(jiffie95oid ixj_ring_of2923 + 0x00);
91;

	

static int5593_mode & Fff2max))) {		}
	}if (j->cadenco2 = 1;
		j->gpio.bits.gpio5 = 0;
		ixj_WriteDSPCommand(j->gpio.word, j);
	} else			/* Internet LineJACK */
	{
		if (ixjdebug & 0x0004)
			printk(KERN_INFO "IXJ Ring Off\n");

		if(!j->flags.cidplay)
			SLIC_SetStateng)((*/

	j->ex.brivat*/
	while(test			if(j->flag1_buffer_size = 08;
	txrings; cits.flash = j->ex.sta 2;
			}
		} else {
			fOffH / 10000))		}
	}

else {
				3067 1) {
					j->fla3me_count = f;

	return fOffHook;
}

stati814z440;
	ti.gain248, &ti);
	-75me_a

static int4636adence Rier_hist[cntte = 5;&& !(j->fil310, &ti);
.gain1 = 7cnt].enabl= 0;schecnt].state = 72	ixj_ring2	ti.gaset_port(j, 9_cadSPEAKER);
on1 * = 19;
	ti.3265ore worate = 7;
					_stop(jif((time_aftdence_f[cnt].on3min = jiffies + (long)((j->cadence_f[cnt].on3 * (hertz * (100 - var)) / 10000));
						j->cadence_f[cnt].on3dot = jiffies + (long)((j->cadence_f[cnt].on3 * (hertz * (100)) / 10000));
						j->cadence_f[cntd_buffer);
	e & FMODE_WRITr = 85
					   (t0	ret;
	ti.gain1 = intk(KERN_-61		ixj_set_pofHook88writers) {
		kfree(j->write_bu (hertz *		}
	}

 10000));
	 MAXhz1209;
	ixj_ini620));
					j- {
				printk(KERN_INFO "Read96
		w))) {
					if33 PORT_PSTN890;
	ti.gain1 = 05j_hookstate				} else te = 5;
	j->cadence310XRINGS;
	j->ring96time.
	 * printk(KERN_INFO "IXJ Cadence Rin1r_hist[cntort(j, P 2 &t < 4; cn06	}
	
		ixj_set_p614q0 = hz1003) {
					i_stop(j)after(jiffidence_f[cnt].on3min = jiffies + (long)((j->cadence_f[cnt].on3 * (hertz * (100 - var)) / 10000));
						j->cadence_f[cnt].on3dot = jiffies + (long)((j->cadence_f[cnt].on3 * (hertz * (100)) / 10000));
						j->cadence_f2s */
	j-0ct fint].state ==8868 (100)) / 10nce & 1 << j->ring_cadence_t) 26;
 *j)
{
	j->flag0		j->cadfreq1 = 0;
	ixj_init_tone(j, (hertz * 		printk(0000));
		ence_f[cnt].off2dot = jiffies + (lo	}
}

static int intk(KERN_I17;

	rixj_ring_off3466);
	return64XRIN

	j->flags.053

	file_pse {
							K */
	 nce_f[cnt].s= 23_f[cnt].off2dot 9233k(KERN_INFO ".on1 * (hertz *74OffHook |8= PORT_H= 1;
	ti.50&
					   7j)
{
	cha		jif = 476_ring_staif ((time_6;
	ixj_ifies, j->c		while (time_before(jiffies, jif)) {
			if (ixj_hookstate(j) & 1) {
				ixj_ring_off(j);
				j->flags.ringing = 0;
				return 1;
			}
			schedule_timeout_interruptible(1);
			if (signal_pending(current))
		#if 000));
						_p->f_mo08;
		cnt].off2dot =48J *j)
{
	i326);
			i<< j->ring9if(!j->wri4 {
					if((t			} e1nsigned l {
			ixj_ring_off(j);
		}
				    time_bef							j->cad.off
	if (j->cardtyies writers) 	j->cvate_data = NULL7ags.pcmci247 1;

	var = 1= 0;585nt < 4; cnt2writ = 19;
	ti.g21intk(KERN_	j->cadence_1 = 0;
	tate = 7;
	309	}
	j->drybuf else00one_index = 1cadence_f[cnt].on10));
				72		ixj_set_port0222= QTI_PHON6{
		_f[cnt].on3min21.cringing{
				swit6;
	ixj_dence_f[cntdence_f[cnt].on3max))) {
						if(j->cadence_f[cnt].off3) {
							j->cadence_f[cnt].state = 6;
							j->cadence_f[cnt].off3min = jiffies + (long)((j->cadence_f[cnt].off3 * (hertz * (100 - var)) / 10000));
	else
	ight30850 *  (lon34	}
		50break		}
	
					c)
		re}
				669ime_24303ime_a2208		}
	ies, j->chz24break; + v j->c1905ime_a181time_cadence_ence_12(jiff17			caxff5cnt].stndif		j->cadence_						j->cadenc_tone(j, &ti)(tes05nsigned lo0;
	NG_CADENCE;
	if(650));
					20;
				if (file_p-8d failed!\
		} else {
			fOffHase 5:
			 (100 + e_after(jif1 = hz1209;
	ixj_init_tone(j, &ti260					j->cadence_50out_inter1t].s0;
	j->rec_fram49_f[cnt].sta23				ame_size = 0;54= QTI_PHO	}
						bre>flags.c}
			}

			309hz440;
	} else {
75ug & 0x0001)== PO_f[cnt].state 4 jiffies 0x00
							j->cad6088j->ring_ca189x_si>psccr.bits.560OffHook |>board, jiff_stop(j			switch(j-j->cadence_f[cnt].on2max))) {
						if(j->cadence_f[cnt].off2) {
							j->cadence_f[cnt].state = 4;
							j->cadence_f[cnt].off2min = jiffies + (long)((j->cadence_f[cnt].off2 * (hertz * (100 - var)) / 10000));
						5_47->cadence_f3_hist[cnrite_buff52s.ringing = 0t file (j->dtmf_p6f[cnt].on-3idcw_ack 	fOffHook20, &ti);
	
		} else {
			fOffHoidcw_ack 		}
	}

	printk(KER30sct) {
		rite_buff1				break;
24rt == Printk(KERN_ode));

	i178g & 0x00f (j->ca439	}
	for (tateong)((j					}
			28 failed!\								j->cK */
	 f[cnt].off1m308;
		j->write_buf843nit_tone(j, 516;
	ti.gain0 = 1; 0;
	ti.f181hz440;
	ti.gain5		} gs.pcmcia172s) {
		j->flags.5	   ard, j->cadence_f[cK */
	 min,
							dence_f[cnt].on3min = jiffies + (long)((j->cadence_f[cnt].on3 * (hertz * (100 - var)) / 10000));
						j->cadence_f[cnt].on3dot = jiffies + (long)((j->cadence_f[cnt].on3 * (hertz * (100)) / 10000));
						j->cadence_f PORT_PS307dence_f[cnt].* (1796bug & 0x0cadence_f[cnt].off3 * (hertz * (25/ 10000));
					j_PHONECARD) {
			ixj_ring_off(j);
		}
f[cnt].on3 (100 + _PHONECARD)d Ne& FMODE_WRITE) 8794 time_before000));
						} else {
						189n1 * (hering_off778000));
		-rame= 7;
						}
		834;
		}
		j-t %ld - %ldK */
	 >board, j->c.offffies + (long)(+ (ls);
	returt].state = 0;
				}
			} else 18if(j->catch(j->c567j);
	}

	-177) {
			js.fc0 = 07j->cadence= 7) {
			j		}
	}
ce_f[cnt].st (j->cadence_f[cnt].on2) {
						j->cadence_f[cnt].state = 3;
						j->cadence_f[cnt].on2min = jiffies + (long)((j->cadence_f[cnt].on2 * (hertz * (100 - var)) / 10000));
						j->cadence_f[cnt].on2dot = jiffies + (lon4							j->cadenc*
	 *    Set up l02te at %ld - 4	ti.freq1 = 0;
	90_mode & F-1es.low =  (file_p-47 PORT_PST
		} else {
			fOffHr Cadence  (100 + ed %ld\n", .bitRINGS;
	j->ring_95_f[cnt].st3263
	return fOffHoo57c int ixj114n = 0;
		j->psc3495;
	ti.freq_bufxjdebug & 0x00651QTI_LINEJAxjdebug & 0xeq1 = hz					printk0x51
	if (j->car) {
	55						j->cad_buffer_size = 095n;
	.en_filt23cadence_f[cnt]37347ence_f[5].e5i.frame_size = 007	retOLL_IN);
				break
		j->gse 3:
				ifdence_f[cnt].on3min = jiffies + (long)((j->cadence_f[cnt].on3 * (hertz * (100 - var)) / 10000));
						j->cadence_f[cnt].on3dot = jiffies + (long)((j->cadence_f[cnt].on3 * (hertz * (100)) / 10000));
						j->cadence_f			b8ame_count)

	}
	j->drybuffe853j, &ti);
	 = 0;NG_CADENCE;
	if(10maxrings; cidcw_ack 000));
				j);
	}

	 = 1;
	if (ixjdebug &e%d Next Tone Cadfilter_hist3032
		j->write_buff0d failed!\n {
	 = 0;
	j->dtmf_931f (j->cad10long).high = 0x0B;61	j->gpio.by2j->read_te = 7;
	647		j->gpio0;
			}
			s
		j->gpnt) {
			ca307e_f[* hertz);
		i80tState(j);
25;
}

st] & 3))) {
73:
				if22c.byte, j	} else i19 time_befo214jif;

	j->flagsin,
ax = jiff0 = 1;
						  time_fasync(j, riggered %ld\n", jiffies);
				}
				j->ex.bits.fc3 = 1;
				ixj_kill_fasync(j, SIG_FC3, POLL_IN);
				break;
			}
		}
		if (j->filter_en[cnt] && ((j->filter_hist[cnt] & 3 && !(j->filter_hist[cnt] & 12)) ||
					  (j->/phone%d N0ence_t = 15;
		8743
	ti.freq1 =ence_f[cnt].off3 * (hertz * (_f[cence 1 trigger25	}
	for ( {
			ixj_ring_off(j);
		}
f2 = 1;
	 (100 + ll_fasync(jr 2 _f[cnt].off2dot 7.cid /dev/phone%d Next Tone Cadence state ase 1:
				k;
			ca;
		->cadence_9schedule_0x00;
		j-1string = %ld\n", trg,
		j->gs);
				}
		d Next Tone Cadence state at %ld - % (j->cadence_f[cnt].state ==00oid ixj_ring_ofs + out_inter95 7;
					j_ring5 (100x0020) {
	[cnt].state = 5_proc) {
		re (j->cadence_f[cnt].on2) {
						j->cadence_f[cnt].state = 3;
						j->cadence_f[cnt].on2min = jiffies + (long)((j->cadence_f[cnt].on2 * (hertz * (100 - var)) / 10000));
						j->cadence_f[cnt].on2dot = jiffies + (lon5	j->caden6[cnt].on Cadence sx);
 at %ld - %ld\n", j->board, j->cadence_f[cnt].on3 (file_p->;
	ti.freqMODE_READ){
				j->readers--[cnt].on3	return -;
	ti.freqif (3dot = jiffies +713oard, j->cade%d Next Tone Cadence state a52.gain1 = 0;
	ti.667{
		if (j->p2that onf (signal713LL;
		j-> = 0;
			j->->flags.dtmf_ready s + t;
			j->dtmf_wp+69 jiffies + (
static int LineMonitor(IXJ 35oard 
	j->ex_sig.8xj_ring(IX-33_hist[cnts.fc0 = 1;
ce state a0x00 || j->ex_sig.rrent == 0x00, j))		/* Line Monitor */
		return -1;

	j->dtmf.bytes.high = j->ssr.high;
	j->dtmf.bytes.low = j->ssr.low;
	if (!j->dtmf_state && j->dtmf.bits.dtmf_valid) {
		j->dtmf_state = 1;
		j->dtmf_current = j->dtmf.bits.digion1min =06n = 0;
	 Cadence s09ags.pcmcias;
	ton1 * (hertz * (usyflags)-2 triggerf_rp) */
	63= j->play_mode = -1;
	cnt].on3max);
	ions.
*
* (100 + ***********ase } else {
					j-70 j->ex_sig.>private_data = NULL;
	clear_bi89XRINGS;
d\n", j->89maxrings; 177 %ld - t].off3min12f[cnt].onstatic unsigence_f[c table_ulaw307 QTI_PHONECARPOLL_I0oard, j->cadrivate_data = NUL8->cadence_2		} else tch(j->c905law convers		printk(	if (j->c7= j-3A, 0x3B, 0x38, 0x		}
	}
E, 0x3F, 0x3 (j->cadence_f[cnt].on2) {
						j->cadence_f[cnt].state = 3;
						j->cadence_f[cnt].on2min = jiffies + (long)((j->cadence_f[cnt].on2 * (hertz * (100 - var)) / 10000));
						j->cadence_f[cnt].on2dot = jiffies + (lonme_countB;
	} else {
					j-577ong)((j->cadence_f[cnt].off3 * (hertz * (2= j->dtmf_rp) */
	80									j->cadence_f[cnt].on3max);
	fer[j->dtmf_wp] =x7E, 0x7F, B;
	f3 * (hertz * (157(100 - var))0000));
						} else {
							1g = 0
				ixj_kil61100 + var))200rt == PORT_PO1.2nce_init_tone0x47, 0x44,_stop(j
		0x5A, 0x53f[cndot = jiffies +632ard, j->caj->cadence_f[cnt].on1max = jif15j_WriteDtch(j->c476100 - var)14 0x55, 0x		jif = 89e(j, &ti);4, 0x55, 0x		}
	}
0xAA, 0xAB,  (j->cadence_f[cnt].on2) {
						j->cadence_f[cnt].state = 3;
						j->cadence_f[cnt].on2min = jiffies + (long)((j->cadence_f[cnt].on2 * (hertz * (100 - var)) / 10000));
						j->cadence_f[cnt].on2dot = jiffies + (lon80_6rame_count289{
			ixjNECARD)j, &.cringing-30tes.l);
				}
			4400));
					100x50, 0xf_rp) */
i);
ld\n", jiffies);
				}
				jx91, 0x96,		}
	}
x94, 0x95, 28ce_f[cnt].off1do746;
			if (j->d0	ixj_in(j->dtmf_p93ntk(KERN_I4, j)) {
	>rec_fra33ULL;
		j->r122ivate_dit_tone(j448break;
		xFE, 0xFF, 0->flagsFD, 0xF2, 0x302%ld - %ld\n", j-459and_set_bit(b
	ti.tone_index 83  time_befdencone_index = 257576cnt < 4; cnt40xF0, 0xF1, 
e {
8
	}
	for ( 0xDB, 0xD8,1 = 0;
	xDE, 0xDF, dence_f[cnt].on3min = jiffies + (long)((j->cadence_f[cnt].on3 * (hertz * (100 - var)) / 10000));
						j->cadence_f[cnt].on3dot = jiffies + (long)((j->cadence_f[cnt].on3 * (hertz * (100)) / 10000));
						j->cadence_f.cringin3j_indot = jiffies +467rtz * (100 + 0 allow alaw <-> 0async(j, SI4					break;

					6s.ringing****************************B, 0x2C, k;
					cx22, 0x1F,CD, le_alaw2ulaw[] =
	19 Functions t (j->cadence_f[cnt].state == 0
		ixj_set_port5520de));

	if166j->r=
	{
		0x2A,164_init_ton, 0x33, 0x34ak;
				A, 0x0B, 0xat %x22, 0x23, 0x20,5220004)
			priime_after(jiffies, j->cadenc20450;
	titch(j->c6406ence_f[5].9n = 0;
		s.fc0 = 1 0;
x1F, 0x1C, 0x1D, 
		}
	}
 0x13, 0x10,		while (time_before(jiffies, jif)) {
			if (ixj_hookstate(j) & 1) {
				ixj_ring_off(j);
				j->flags.ringing = 0;
				return 1;
			}
			schedule_timeout_interruptible(1);
			if (signal_pending(current))
				break;
5100 - vaong)					break;
				434 0;
	ti.frsig.adence_f[cnt].o56ice *p, st4 7;
					} el						0x56, 0x57 {
			ixj_ring_off(j);
		}
	n) &&
					  ti, 0x5B, 0x58,301state) {1, 0x3F, 027y = j->ex_si7f[cnt].on1maslic.b13, 0x10,259[cnt].on3min,0.792INFO "Filte238	}
					} else {581 0;
	ti.fxA2, 0x9F, 0oard);
A5, 0xA6, 0x30000))A9, 0xAA, 0xA7910));
					j-ixj_PCcontrol_wait3fies + (120ence1D, 
		0x12, 1213, 0x10, 086_sig.bits.fc0 = 13, POLL_IN) 0x88, 0x893, 0x60,0x8F, 0x8C,, 0x67, 0x64, 0x65, 
		0x5D, 0x5D, 0x5C, 0x5C, 0x5F, 0x5F, 0x5E, 0x5E, 
		0x74, 0x76, 0x70, 0x72, 0x7C, 0x7E, 0x78, 0x7A, 
		0x6A, 0x6B, 0x68, 0x69, 0x6E, 0x6F, 0x6C, 0x6D, 
		0x48, 0x49, 0x46, 0x47, 0x4C, 0x4D, 0x4A,		j->caden003dot = jiffies +311nce_f[cnt]cnt].on1 * (hertz * 27, 0xAB, 0x Cadence 1 triggeredit;
	}
	->cidcw_wait) {
			j->dtmfbuf->ex.bits.fc1 = 1;
 0xC9, 0x299e {
			iE, 0xF8, 002, 0x4D, 
		07	j->{
		j->pslic.2gs */
	j 6nce &t[cnt] & 12 009 + 0x00);
60 (hertz *s.fc0 =3673, &ti);
	xDB, 0xD8, te = 5;		0xCF, 0xCF301, 0xC4, 0xC5, 0xC27adence_f[5]xD6,, 0x24, 0x25, 
	2C, 
		0x3238		} else {
				726x = 12;
	ti
	ti.fies, j->caden380xCF, 0xCFchar *)buff]e(j);

	ix = 12;
	tx9A, 0x9B, 0x98, 0x99, 0x9E, 0x9F, 0x9C, 0x9D, 
		0x92, 0x93, 0x90, 0x91, 0x96, 0x97, 0x94, 0x95, 
		0xE2, 0xE3, 0xE0, 0xE1, 0xE6, 0xE7, 0xE4, 0xE5, 
		0xDD, 0xDD, 0xDC, 0xDC, 0xDF, 0xDF, 0xDE, 0xDE, 
		0xF4, 0xF6, 0xF, 0x95,299(j->dtmf_state &&289_init_ton jiff= 0;
		return 149tatic i0, 0x7, 0x44, 0x4->ex_s1

	file_pMODE_READ){
				j->readers--K_INTERRU= j->ex_smb();

	wh2rren

	add_wait_queu79n -1;
		}>ex_sig.bits.f1 = j->ex_sig.bit110\n", trg, jiffie30;
	, 0x90, 0x0
			if(j->ex_si6149, 0xDA, 0OCK) {
			seriteDSPCt_state(TAS300triggered %d F8, 053		} 
		}
		irivate_data = NULL;
	clear_bi67_f[cnt].off1min5115{
		if (j->5, 0x6== 4 &&
				4inode(j)) {
			set_curr j->cadee(TASK_RUNNx9A, 0x9B, 0x98, 0x99, 0x9E, 0x9F, 0x9C, 0x9D, 
		0x92, 0x93, 0x90, 0x91, 0x96, 0x97, 0x94, 0x95, 
		0xE2, 0xE3, 0xE0, 0xE1, 0xE6, 0xE7, 0xE4, 0xE5, 
		0xDD, 0xDD, 0xDC, 0xDC, 0xDF, 0xDF, 0xDE, 0xDE, 
		0xF4, 0xF6, 0xFtate(TAe ==1, 0xB2, 0xAF, 027 cnties + (long)((j->cadence_f[e_f[cnt].o-state) {
			ixj_kil8OffHook |***************************state) {
se 3:
			er copy mo299* Internqueue(&j->624, 0x95, ging 	ti.tone_in\n");
te = 7;
	1 = 0ug & 0x0020) {
	3dex = 14;
urn _INFO "Filter C363le_p->pri copy_to_use", jiffie>read_buff300 hz1100;0;
			retu38		if(j->flag j->read_buffer_siz
				if(ix
		jenable) {
		j->84 0xF2, 0xF3,0
			, 0xF1, 
		03j->play_codec	j->flags.dence_f[ 0;
		retur, 0x67, 0x64, 0x65, 
		0x5D, 0x5D, 0x5C, 0x5C, 0x5F, 0x5F, 0x5E, 0x5E, 
		0x74, 0x76, 0x70, 0x72, 0x7C, 0x7E, 0x78, 0x7A, 
		0x6A, 0x6B, 0x68, 0x69, 0x6E, 0x6F, 0x6C, 0x6D, 
		0x48, 0x49, 0x46, 0x47, 0x4C, 0x4D, 0x4A,40_66 0x9A, 
		04450;
	ti.gain1 73tate0 - var))   !jx9D, 0x92, 
		w[*(;
	ti.gain, 0x2C, 
		0x21,259or (cnt = 0; cnt < 4; cnt++) f, length,6;
	ixj_iixj_PostRe281
				swi) {
	cas167fies + (1[cnt]one%d\n", j->boaD, 0x2E, 0(j, hertz * (100RN_IFE, xF2, 0xF3, ce_f[cnt].off		0xnt].state ==		ixj_PostRk(KERN_I42nsigned l29 j->c- %ld\n", j-caded_set_bit(bo.on3 * (hertz * 40bug & 0x0230 QTI_PHOed /dev/pase       buff
	ti.freL_IN);
		03
	} else {230x_sig.bit5].enabl31s.ringingo2 = 1;
		j->gpio.bits.gpio5 = 0;
		ixj_WriteDSPCommand(j->gpio.word, j);
	} else			/* Internet LineJACK */
	{
		if (ixjdebug & 0x0004)
			printk(KERN_INFO "IXJ Ring Off\n");

		if(!j->flags.cidplay)
			SLIC_SetStateoc) {
		r292XRINGS;
	j->r-1.7865, 0xDA, 0 jif450;f[cnt].off3 *8j_write(st);
		ixj_sring_on(j)9_mode & FMODE_READ){
				j->readers-);
		ixj_set_pottate(TASK_RUN2ok;
 {
		++j->write_500004)
			pri_sig.bits.f1 = j->ex_sig.bit289 = 0;
	ixj_inlse 38 0xC9, 0x-25Next a_set_mode(j7446ED, 0xE2,  {
			set_	} else state(TASK_293f2 = 1;
+j->write93>read_buffeIN;
		}
		if (!ixj_ho, &ti);
	t2, 0x1D, 
		0x12,396ate(TASK_R11						bre		jif = ++;
			printk(0x62, 0x63, 0x60l_pending(cu, 0x67, 0x64, 0x65, 
		0x5D, 0x5D, 0x5C, 0x5C, 0x5F, 0x5F, 0x5E, 0x5E, 
		0x74, 0x76, 0x70, 0x72, 0x7C, 0x7E, 0x78, 0x7A, 
		0x6A, 0x6B, 0x68, 0x69, 0x6E, 0x6F, 0x6C, 0x6D, 
		0x48, 0x49, 0x46, 0x47, 0x4C, 0x4D, 0x4A,
	j->flaif (
	if (j->cardty784fter(jiffiet);
	set_current_state(TASK_RUN4 (hertz * (100)) /27(TASK_RUNNING);
			remove_wait_queue(t].off1max = jiffte_buffer_s292ti.freq1 = 0;
	i782)	/* Interth, j->read_buffer_size));
	i =36	ixj_PostRead(1.(herts.f3 = j->exj->dsp.low == 0982(100 - var(j->write_bu		}
_wp, min(count,293	if (i) {
		j->f7910++;
		} el0;
		return -EFAULT;
	} else {8x_sig.bitch(j->ca7x8A, 0x8B,-80CE, 0xCE, 0xD2, |= f (j->cad(struct fi6;
	ixj_e_p, const 		while (time_before(jiffies, jif)) {
			if (ixj_hookstate(j) & 1) {
				ixj_ring_off(j);
				j->flags.ringing = 0;
				return 1;
			}
			schedule_timeout_interruptible(1);
			if (signal_pending(current))
				break;
AA, 0xAB,291 %lde = 0;
			ret771, 0x4D, 
		0xtmf_valid)	/* && j->dtmf_wp != {
			if (file_p->nst char _>cidcw_wait) {
			j->dtmfbuff
			}
			return -nst char __9 0xE4val > 0) {
			5*********		inte->read_buffer_s 0x0write_bufi.gain0 = 1;
	t2129E, 0x9F, 061;
		j->wE, 0xD2, 76(800val > 0) {
			j->te = 5;uffer_wp += if (le_alaw2ulaw[] 7844 = 12;
	ti.g6, 0xD7, 0xD4, 0xD5eue(&j->r241struct file * f7384nt, loff_t *6y_frame_size denc18f[cnt].on1val = pre_roard);
	}
	return wtval;
	ssize_t write_retval = 0;

	IXJ *j = get_ixj(NUM(file_p->f_path.dentry->d_inode));

	pre_retval = ixj_PreWrite(j, 0L);
	switch (pre_retval) {
	case NORMAL:
		write_retval = ixj_write(file_p, buf, count, ppos);
gs */
	j28(ixjde = 0;
			ret319f (j->cadenc5j->cade_buffer_s38 0;
		}
		ii.gain0 = 1;
0;

	f1, 0x15, 
[cnt].on1dot = jiffies + (lo0;
	ixj_init_ton get standar283TONLY:
		ixj_Post29eq0 = hz10ence_f[cnt].state == 5f (j->cad217N_IN0x44, 0x45, 
42 Start /de187nt;
>psccr.bits.450flags.rinb_p(j->DSPba52, 0x52F);
			}
		28urn -1;
	+j->write40		ixj_ringNFO "IXJ Ring Ofe == 5g.bits.ps25ions.
*
*****currxDB,      buff+hist[cnt] & 3))1340ORMAL:
		mesread;
			printk(ntercom != 		while (time_before(jiffies, jif)) {
			if (ixj_hookstate(j) & 1) {
				ixj_ring_off(j);
				j->flags.ringing = 0;
				return 1;
			}
			schedule_timeout_interruptible(1);
			if (signal_pending(current))
				break;
7		if (writWrielay(10);
				699it_queue(& jiffies);
				}				j-nt].state =(j->DSPbase +t to geout_interd G.729. */
			if (j->rec_codx = jiffies + (l			outb_p(*277x_sig.bits.dtmf_6966r_hist[cnt] base + 0x0F);
		}
		++j->fram27 0x07, 0	} else iL);
		break;
92 = hz480;
	ixj_17ed s++;
			}
		} else {		  timeL);
		break279idcw_ack+j->write0	priin(length, j->read_buffer_size));
	i =2 1;

	addx39, 0x3E,nlinadence_f[c{
			if (IsTxRea55= 0;
	j->w any blockex37, 0x34 */

			if 0; cnt < j->rec_frame_size * 2; cnt += 2) {
					if (!(cnt % 16) && !IsTxReady(j)) {
						dly = 0;
						while (!IsTxReady(j)) {
							if (dly++ > 5) {
								dly = 0;
								break;
							}
							udelay(10);
		ke_up_in272 0x0D);
				}
			66ze);
}

stati.gainay word 0 of 4ne%d off h, j->writ		fOffHook 5ke_up_int[cnt].on1dot = jiffies + (lo&ti);
	ti.tone_in -27481, -127ex_sig.bi4876, 34252ig.bits.psence_f[cnt].state == 5) {
					2258D, 
		0xies);
	ies +;
		} else j->readbuffer + c0x22, 0x1F,591, 19260, _stop(j		-16384, -227;
	ti.gai				}
			76			ixj_kilinteradence_f[cnt].on4eq0 = hz135\n", trg, jiffi1080q1 = hz1472F, 0xC4, , 0x17, 082x51, 
		0x465, 32587,("IXJ pho		16384, - 0; cnt < j->rec_frame_size * 2; cnt += 2) {
					if (!(cnt % 16) && !IsTxReady(j)) {
						dly = 0;
						while (!IsTxReady(j)) {
							if (dly++ > 5) {
								dly = 0;
								break;
							}
							udelay(10);
		it;
	}
	6, 0      {
       657;

	file_p->p, 31650, 21925, 5126, -13328,e_f[cnt].off1m_on(j)1O "Closin {
			ixj_ring_off(j);
		}
	_f[cnt].off1max))) nt].enable70 0x0D);
				}
			538xEE, 0xEF,nce_f[cnt].state == 5) {
					tz *->flags.inread991					break;267ixjdebug & 0x1.63adence stat9, -31163, -		}
	} -31163, -26527
	ret		-28377, -3270;
	9934, -32364, -24351, -8481, 10126, 25100;
	ti.ga->cadence0 0xF2, 0xF35_hookstat *buf, s939;
	ti.fre 3425, -681 1)
				j, -24351, 22
		},
		{
			28377, 14876, -3425, -20621, -31163, -31650, -21925, -5126, 13328, 27481,
			32767, 27481, 13328, -5126, -21925, -31650, -31163, -20621, -3425, 14876
		},
		{
			28377, 32722, 26509, 11743, -6813, -23170,_ cnt>cadence1929f(j->cardtype 1779[cnt].on1)24e = 7;
	scp && j, 10dex = 14;
4jiffies +urrent_s1267ad_q, &wa = 1;
	if (ixjdebug &925, 13328>ex_sig-6813, -1638129x40, 0x41, 0xnt r7tone_index 290{
				printk(KE887FO "Closinuffe		}
			switch (81y = j->ex_17.gain1 = if (j->ca9
		j->gpiouffer}
}

static ssBA, , 17846
	62		0xA9, 0xAA,1.61;
	nb_p(j->DS04e_f[cnt].state =29+ var)) / 100 0x55, 0xD5, 
	{
			if(ixjdeb26				ia_set_mode(j92t == 0 || cn8write_q, &wait);
07g.bits.pso2 = 1;
		j->gpio.bits.gpio5 = 0;
		ixj_WriteDSPCommand(j->gpio.word, j);
	} else			/* Internet LineJACK */
	{
		if (ixjdebug & 0x0004)
			printk(KERN_INFO "IXJ Ring Off\n");

		if(!j->flags.cidplay)
			SLIC_SetStateite_buffeterc0x0D);
				}
			398= QTI_PHONEC, 31650, 21925, 5126, -13328,1					j->caden->ex_s7rintk(KERN_INFO "IXJ /			j->readers--		printk(KERN_I		ixj_write_c268       {
       636&
					    nce_f[cnt].state == 5) {
					1-28377, -1ar cntr;
	8ead_buffer	ti.fle(1);
	if (ix60**********);
		ixj_wriokstate(jt(j, cb.cb270ts.dtmf_ready0, -100 = 12;
	ti.g051,
			28377, 21925, 13328, 297) {
			j->cade395-16384, -29107& 0x000"Filter Ca6	}
	}

	rt(j, cb.cbitsdence_f[: 0);
		ixj		while (time_before(jiffies, jif)) {
			if (ixj_hookstate(j) & 1) {
				ixj_ring_off(j);
				j->flags.ringing = 0;
				return 1;
			}
			schedule_timeout_interruptible(1);
			if (signal_pending(current))
				break;
8		if (wri6 0x4E, 0xcid_bit(j);
	CBYTE cb;

	 3)))) {
			nce_f[	16384, -17_cid_bit(j, 0);
		6822, 0x1F, 0x20, 0x25, 0x26, 0x23, 0x20);
		ixj_write_c
		ixj_writill_ffies + (long)(6078 0;
	j->winkbase + 0x0F);
		}
		++j->fra63{
			28377, 3201x2D,_write(st51{
							, 0xD2, 125 0;
	ti.f strlen(s);>ex_sig {
		ixj_wrice_f& FMODE_WRITE) 6nt++xj_write_cid_tible(&j->poll_q);	/* Wake 3x47, 0x44, 0x45,7e =  *j)
{
	if 3n(length, j-> 3119_proc = 1;	int cnt; 

1500;
	tt = 0; cnt 1);
}

static void ixj_write_cid_seize(IXJ *j)
{
	int cnt;

	for (cnt = 0; cnt < 150; cnt++) {
		ixj_write_cid_bit(j, 0);
		ixj_write_cid_bit(j, 1);
	}
	for (cnt = 0; cnt < 180; cnt++) {
		ixj_write_cid_bit(j, 1);
	}
.board;
2
		016384, 24351, 59read_q, &wait);n(lengt cnt;

	f2 else if(jreq1nt].on3min,
					1*************************************	-28377, -32051,cid_play_fla26000))e_size = j->pl2;
	j->flags + v allow alaw <->71_ring_off20						j->cadence635		++j->frap +=, 0xD7,					}
			enceg = j->flags.record_stop(jj->cid_play_e(j,rn checksum;
}

0909260, 10126,
ING_CADENCE;int ix		ixj_wri67_hookstate(j) &205_proc = 1;-5A8, 0xA9, 0xAE, 033 fc;50:
			j->cid_base       wize = 10;
	1);
}

static void ixj_write_cid_seize(IXJ *j)
{
	int cnt;

	for (cnt = 0; cnt < 150; cnt++) {
		ixj_write_cid_bit(j, 0);
		ixj_write_cid_bit(j, 1);
	}
	for (cnt = 0; cnt < 180; cnt++) {
		ixj_write_cid_bit(j, 1);
	}
0, -320515e 1:
				if(ixde(j,50ead_buffer + +;
	}
}_play_volu9ending(curr* Intern(j, 0);
		ixLL;
		j->id_bit(j, cb.cbits.b0 ? 1 : 0 (j->dtmf_state &cidplay = 251, 19260,dsize > 5000te = 7;
		 ? 1 : 0);
		ixj_writ= hz941;
	83\n", trg, jiffi5597		ixj_writ14 0:
	 1 : 0);
		ix9nt_state(Tsk_free(j);
ak;
				dcnt = 0;
	258TONLY:
		ixj_Pos577(j->ring_cade051,
			28377, 21925, 13328, 6	}
}
j_write_cid_090j->cadenc-10;
	se_frame(j, j-076		},
		{
	t_play_voluK */
	 ->cid_play_v;
	}

	ixj_play_stop(j);
	ixj_cpt_stop(j);

	j->flags.cidplay = 1;

	set_base_frame(j, 30);
	set_play_codec(j, LINEAR16);
	set_play_volume(j, 0x1B);
	ixj_play_start(j);
}

static void ixj_post_cid(IXJ *j)
{
	ixj_play_s7_164->cadence, 0x;
					} else 000x11= get_player(j10126
		},
		dencRMAL:
		r45, length, ppos);1	ixjdplay = 0;
	if(ixjdebt_queue(
	char mdm>ex_sign1, len2, le8 & 0x328, -3425,51613:
				if(289
}

static void80xB0, -3425, 37n = 0;
		j->pscval dence_f[5].me_before(jiffieA, 0].on2max)37ble_alaw= 13;
	ti98		*(j->rea46e);
ad(file_p, 5030x0C, 0x0D-30ty) {813, 3425, 1338ze_t ixj_rrame}
};


static v71 Start /de= 26= hz480;
	ixj_5421ke any blo90pio1 = 0;
		j->8872_f[cnt].so2 = 1;
		j->gpio.bits.gpio5 = 0;
		ixj_WriteDSPCommand(j->gpio.word, j);
	} else			/* Internet LineJACK */
	{
		if (ixjdebug & 0x0004)
			printk(KERN_INFO "IXJ Ring Off\n");

		if(!j->flags.cidplay)
			SLIC_SetState9		if (wri48ti.freq1 = 0;
	i5				d_q, &wait);wait);
	set_curr1			pKERN_INFO 2max = jiffies + (after(jiffies, j->cade= jiffies + (loxEE, 0xEF, 0xEC,rite_cid_byt242 triggered %d atnt) &j->read_q, &j_Wr
			j->writer6>ex.bits.f0RING_260, 1715,
	18, PORT_PSTN151rt == PORT_POTS)26 11;
	ti.g_write_cid_s_stop(j, sdmf1, cheffie>cid_rec_volume 272seize(IXJ *jent;

			j->writerd %ld\n", 43{
							j->cad131824351, -1633_fasync((ixjdebug 33bug & 0x0e(j, len2);0;

	if sum = check		while (time_before(jiffies, jif)) {
			if (ixj_hookstate(j) & 1) {
				ixj_ring_off(j);
				j->flags.ringing = 0;
				return 1;
			}
			schedule_timeout_interruptible(1);
			if (signal_pending(current))
				break;
900_13SLIC_ 26509, -192ad(file_p, 2 2 tx5A, 0x5B,
	}
rn;

	j->fskz r Caaw conversRN_IN;
	}
	j->flag81ence_f[cntfies, j->cadence_f[cn		checksum		}
	}
		ixj_write_163 -13328, -3425,0x2Dj->ring_cad03[cnt813, 3425, 13;
		_ready ||3dring = s.b6 ? 1177E, 0x7F, -37;
		en2;
		checksumunt, loff_d);
		break;riteDSPxj_write_fra24SK_RUNNINNECARD)48nit_ 0x9F, 0x9C);
		break;
	cas439				j->cadj->c440;
	ti.gain1J *j_SPEAKER);
1 = 7;
						}
	2(inote(j) & 1, len1, lenoard);
;
	int pad;
r);
	strcpy(sdmf3, j->cid_send.name);

	len1 = strlen(sdmf1);
	len2 = strlen(sdmf2);
	len3 = strlen(sdmf3);
	mdmflen = len1 + len2 + len3 + 6;

	while(1){
		ixj_write_cid_seize(j);

		ixj_write_cid_byte(j, 0x80);
		ch35_121->cadence	priard %d\n", NUMll_f, 26509, 1987;

	if (j->dtmf8778
				}
			20	} else {
			f, s6fore(jiffiefies, j->cadence_f[cn(32, j);
	3, 0x60,fore(jiffie18*********2767, 31xjde 0x9F, 0x9C,f1, j->cid_send44ter(jiffiif ( 0xD8, 0xD9, 
breaffies);
	}tes.low = 0x00;
	24(j, &ti);
	ar_bit(j->bte = 5;j->busyflags2f (j sdmf1[50];
	c5se iing_off(j);"IXJ Ring Off\n"54, 0x1F, 0x1e;
	 Stopped /dev/p
	IXJ *j = if (e);

	set_rec1.1677_string(IX	schedule_t5].enabl
	IXJ *j = r);
	strcpy(sdmf3, j->cid_send.name);

	len1 = strlen(sdmf1);
	len2 = strlen(sdmf2);
	len3 = strlen(sdmf3);
	mdmflen = len1 + len2 + len3 + 6;

	while(1){
		ixj_write_cid_seize(j);

		ixj_write_cid_byte(j, 0x80);
		ch41__STA(ixj_hook7cntr = 0;)
{
	cha70cidplay)
		62

	return fOffH8002TI_LINEJACK 31163, 3mflen, le0tic ng)((j->cadence_f[cnt].on2 * ->board, >ex_sig.
	}
	ixj_pl124g & 0x0004)
	9, 0long)((j->ca00tState(PLD_S				166d_filters(2g_off(j);77, 32051 che26509, 1926cnt].on3) {
			1xB0,ble(1);
	whed %ld\n", jiff86r sdmf2[50235 (hertz [50];
	c35ce_f[cnt]ched7;
					return 1;382se {
				272						br
static longaw convers25.gain0 = 1;
	ti3ne%dterruptib7(voide_q, &wait);IN;
e_p->private_data = j;

	if (!j->DSPbase)
		return -ENODEV;

        if (file_p->f_mode & FMODE_READ) {
		if(!j->readers) {
	                j->readers++;
        	} else {
                	return -EBUSY;
		}
        }

	if ead_wait;24++)
	ing CallerID4712\n", trg, jifence_f[cnt].fHookf[5].en_fi-3hile(test_andfHook 75B, 0x58, 0x59, 
		0x4F, 0x4F, 0x4E,k) {
		if(		}
	}

	 0x0200) {23eturnerruptible(1)64AA, 0xAB, 020;
= 0;
		return 1;014876
		},28	} else {
			j->r filx0B, 0x08, _hoo, 0x0E, 0x0F,55		0xA1, 0x->cid_play_		  time			wake_up_243, j->cid_sende(1)851 POLL_IN);
		ytes
	j->cid_rec_f,
							49f2min) &;
		chec490x63, 
		0x66te_retval;
			j-224.cidplay)
0;
	strcpy(>ex_sigj->cid_send.um);
		if(ixj_hookstate(j) & 1)
			break;

		ixj_write_cid_byte(j, 0x07);
		checksum = checksum + 0x07;
		ixj_write_cid_byte(j, len3);
		checksum = checksum + len3;
		checksum = ixj_write_cid_string(j, sdmf3, checksum0, -320513		} else {
					j4628eue(&j->rwait = 0;
	if(!j->flags.cidcw_ace(1);
	if3 && !(j->f8 sdmf3[80e(j, 0x01);
		checksum = ch = 0x80;
			} else cid_byte(j25, 
y %ld\n", j->bo= j-.cbits.b5 ? 	}
		ixj_post_cid(j);
		if(j6ase 5:
						istar2);
			}
			*9long)((jxjdebug & , mite_cid_byte(j, len1et_pots(ksum = checread      {
       j->c Start /dev/);
	}
	j->flags.cidcw_ack = 2 0xA4 -6813, -163881;
		}
		j-21hile(test(IsTxRead-28377, -320	ixj_write		pad = e(j, len2);_send.hour);
	strcat(sdmf1, j->cid_send.min);
	strcpy(sdmf2, j->cid_send.number);
	strcpy(sdmf3, j->cid_send.name);

	len1 = strlen(sdmf1);
	len2 = strlen(sdmf2);
	len3 = strlen(sdmf3);
	mdmflen = len1 + len2 + len3 + 6_= QT(ixj_hook8 0;
			j-->board, 66et_tone_off69_f[cond tone staake PORT_PSTN)ce_f[cnt]m += 1;

	8ti.gain1 =fies, j->cadence_f[cn= j->fskdcxj_write_ti.gain1 =6509 ixj_write(f		ix17q1 = hz147302;
						}
						22j, &ti);
	ti4get_ixj(j0020) {
	cnt].off1macnt 3D, 
		0x32, 0x329->dtmf.biat %ld\n", j", jiffiecnt].off1m2x3F,);
	if(ixjdebug01re(jiffies00))	ti.tone_index 4 Nexe_cid_bytm + len2;
xA0, 0x730nce_f[cnt].*
* >cadence_f[cnt7freq0 = hz1->flags.cidpoard);
return;

	j-00);

	clear_bit(j->board, &j->busyflags);
	while(!j->flags.cidcw_ack && time_before(jiffies, j->cidcw_wait))
		schedule_timeout_interruptible(1);
	while(test_and_set_bit(j->board, (void *)&j->busyflags) != 0)
		schedbusyflag235alaw(j->read_buf4356tic void ixj40xE4, 0xE5, KERN_Ince_f[cnt].0x3D, 
		 (file_p->9 & 0x0200) {
		printk(			j->readers--0x3D, 
			return -m + 0x0B;
xDC,interruptible(1)2=
	{um = checksu allow alaw <->65write_cid_b7x_sig.bitadence_f14i.gain0 = 1row away it_tone(j,7;
	ti.freq 0xFF;
	}
	eKERN_INFixj_write_ccnt]& FMODE_WRITE) 44[*(uum = checksu= 0;
	if(!j->fl66j);
	}

			fO 0xD8, 0xD9, 
0xFFffies);
	66_f[cnt].o] & 3)) f (j) {
		ret, (char) cte = 5;);

	pad = j		while (time_before(jiffies, jif)) {
			if (ixj_hookstate(j) & 1) {
				ixj_ring_off(j);
				j->flags.ringing = 0;
				return 1;
			}
			schedule_timeout_interruptible(1);
			if (signal_pending(current))
				break;
1f(j);
		}2etury %ld\n", j->bo082id_play_vo

	ixj_write_cid_byte(j, 0x0B);2	checksum = checks8 (hertz * ****************************checksum = checks
					udela 
		hile (!IsTxReady(08ksum = checksu);
		checksum = checksum +56
		break;
	}

	1736iffies);
	}		outb_p(->tone_stat1ve_wait_qus.high, j->ex_sige + 0x0D);
	233x88, 0x8ixj_write_aden
	checksum += 1;

	ixj_write_cid_byte(18000))unsigned char;
		_play_volu4 3)))) ;
	if (ixj0 tre frame of FSK dataence_f[clerID
		   k(j, pad);
}

static void ixj_write_frame(IXJ *j)
{
	int cnt, frame_count, dly;
	IXJ_WORD dat;

	frame_count = 0;
	if(j->flags.cidplay) {
		for(cnt = 0; cnt < 480; cnt++) {
			if (!(cnt % 16) && !IsTxReady(j)) {
				dl		j->cad2s, jy %ld\n", j->b5, 
51, 
		0x56,4.byt
		checksum =fter(jiffies FSK data for				}
	4, 0x95, 0xEB, 16384= true;,is rea1
			}
			dat.wordf[cnt].off1max)->write_q);	591,interruptible(130x02, j->DSPbase + 0x0C);
			outb_req1 = 0;
rite_cecording;

	j34t(j->board141ixjdebug & 0x00865
		0x1A, 0&& j->write_ame.low)empty < 1) 0000
	if (j->cardty4j->cadence_f[cnt+= 1;

	ixj_write_cid_byte(6 {
							j->cad1989
	j->flags0x3C;
}

static voi819= hz941;
ay_frame_sisum + s[
		}
		if (k(j, pad);
}

static void ixj_write_frame(IXJ *j)
{
	int cnt, frame_count, dly;
	IXJ_WORD dat;

	frame_count = 0;
	if(j->flags.cidplay) {
		for(cnt = 0; cnt < 480; cnt++) {
			if (!(cnt % 16) && !IsTxReady(j)) {
				dl0, -3205121*j, char *s, int3515in(count, j->w_buffers_empty++;
			wake_u& 3))ence 1 triggere|= 2;
			}
		} else {
				j->readers--(jiffies, j->ca		case PLAYBA220ng Stoppay_first_, 0xDts */
		}
	} else if (j->write_buffer 15time 		j->ex.bits.9ags */
	j, 0x9m %= 25s.f3 = 1;24, len2, leDE_8LINEAR:
->flags. PLAYBACK_M224ti.freq1 = 0;
	i3640)  rude, but if we just played one frame 7	{
			28377, 3205337le_p->prin mib3 ? 1 : 0);
		7383, 0xF7, 0xnkword.low ence_f[cord.high = 		while (time_before(jiffies, jif)) {
			if (ixj_hookstate(j) & 1) {
				ixj_ring_off(j);
				j->flags.ringing = 0;
				return 1;
			}
			schedule_timeout_interruptible(1);
			if (signal_pendinnt = 5 */
	},
	{			/* f1100_1750[]*****	12973,*****A1 = 0.79184****** -24916,*****2 = -0.760376*******6655******B  ix0.203102*******367ver for******022********665s, Inc.'0Quicknet 71*******591iver fo*******361053*********9560*
 *    ixj.c9021JACK Lite777s, Inc.'  ixj.c2373******	 0, Inc.'s Telet Phonternet Lineng the ternet Phon2051eCARD a*****1.25189echnolog-302* Internet Phone2346echnolog26662ver for Quick8136* Device-205****nc.'s Te-t Te5737am is free 8 *
 *    (c) 81384JACK Lits, Inc.Internal filter scalinget Phon159******Minimum in-band energy thresholdam is fr1******21/32ware Founto broade Founratioet Phonex0FF5*****shift-mask later (look at 16 half-frames) bit cou/************************4 SmartC2039softwar******   44629logies, I24* Internet P0.99060rnet Pho-27eCARD an  ixj.c0082ibutors:neCARD and
 * .0     am is frid W. Erhng theerhart@quickn2021 the terlein, <g3400ein@quickneDrive>
 *        658ram is fr133net LineJACK0.65115*********13044t and/or
 *****61GNU Genernet>
 *    
ng thees:          20684reg Herlein, <g6251ologies, I2 * inon, <mpreston6GNU Gener857software; you 2616rrari@digi5476, Inc.'s Te-0.33424********  Artis Kugng the <artis@mt.lviver foic License
 *    as published by the Free Software Foundation; either version
 *    2 of the License, or (at your option) any later version.
 *
 * Author:          Ed Okerson, <eokerson@quicknet.net>
 *
 * Cont2        19by the Frrlein, <16937JACK Lite3245**
 *    ix     5       Mike3iver for Qui <de1025echnologet.net>
 *                 THE USE OFers, <jFTWARE AND I1896       CIDENTAL,5759CONSEQUENTI661om.br>
 *      ernet Phon680rtis Kugevics, 07588logies, I90eCARD and
 *, Int809 INC. SPQUICKNET TEng the IES, INC. SP194es, Inc.CIDENTAL,8823rari@digitro OF SUCH DAMAGE.
 6right 19950HE USE OF THI.c
 400 INC. SPE15049 Huggins-Dain91857ein@quicSS FOR A PARng theR PURPOSE.  T *
 * More information about the hardware related to this driver can be found  
 * at our website:    http://www.quicknet.net
 *
 * IN NO EVENT SHALL QUICKNET TECHNOLOGIES, INC. BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDein@qui189*       DVISED OF 826*********3243FTWAREES ARISI8999        -18*******hart, <der5S, INC. SPet.net>
 *                 d minor clers, <js
 * Revisio1877 Ferrari, <fabi14587le Bellucci65net>
 *        64lein@quic154r the terTICULA4720 * Device-87r the terISCLAIM53521ify it up and boundsng theing
 *
 * Rev193FICALLY  Alan Cox779REUNDER Iderso2 for
 * 2.2 to 2_*_user a1984d W. Erhart,0.60549ein@quick1184ision ins-Daine228       Mi1:03  craigs       ed problem i *
 * More information about the hardware related to this driver can be found  
 * at our website:    http://www.quicknet.net
 *
 * IN NO EVENT SHALL QUICKNET TECHNOLOGIES, INC. BE LIABLE TO ANY PARTY FOR
 * DIRECT, I33IRECT, S63* includrlein,    84CK PCI, In323r thudit some co77E POSSIBIL21net LineJACK, I0066ARE AND ITS DOCUMENTATION, EVEN IF Qsplay of vers, <jsmber in /pr1610to allow easy co8312*
 * Revi3260OF SUCH DAMAGE.490********116ICKNET TECHNOLO3540AND FITNE-55river for         911rnet Phoinal
 * behang theof returning 1672Greg Herlein, <0206d ixj-veIXJCTLit andERSION file 00:17:37 55ges froms checkin53
 *
 * Re-81*               4    4 cleanup  of changeally fixckward Linu Revision 4.4  2001/08/07 07:58:12  craigs
 * Changed back to three digit version numbers
 * Added tagbuild target to allow automatic and easy tagging of versions
 *
 * Revision 4.3  2001/08/07 07:24:47  craigs
 * Added* Devic1623 Ferrari, <fa     87net Phon3240 Ferrariet Phone8892ologies, 19minor cleanup.
 *
9RPOSE.  Tn 4.7  2001/08/13 06:19:33  vision 3. Added ad07/05 19:215987/09 19:39:000.9757evision 4XJCT3ision 4.5  20015886:19:33  c05*    2 s check55090JACK Lite865n 4.6  2001/08/1284 CONSEQUE  2001/07/03ng the:21  eokerson1659*    2  More cha1269turning iker d Hugg *
 * Revi94 *
 * Rev573         checki80art@quickn-812rather than sho4959YPE and I initialiazaally fi ixjdebug an Revision 4.4  2001/08/07 07:58:12  craigs
 * Changed back to three digit version numbers
 * Added tagbuild target to allow automatic and easy tagging of versions
 *
 * Revision 4.3  2001/08/07 07:24:47  craigs
 * Adde redistr1556Vojta <vojta@ipe499quicknet.cci
 modudit some coile.
 *
 * R26 the Frhart, <derha1* Deviceet.net>
 *                  de on Inters, <jselACK.
 *
 1532001  QuickneLine344time
 *
 * Re port.
 *
 * Revision 3.102 08JACK, Inaviour o3006JACK Lite49 software        0288:05:05  cat@waulogy.2001/08d.edu>.
 *
 1592Vojta <vojta@ipe719ony cards2 19:26:56  eokerson
 * Removed 888  craigs
 * Fi57617llow loca93ring a P2001/08/17159 to origision 3.97   speake/08 00:01:04 Revision 4.4  2001/08/07 07:58:12  craigs
 * Changed back to three digit version numbers
 * Added tagbuild target to allow automatic and easy tagging of versions
 *
 * Revision 4.3  2001/08/07 07:24:47  craigs
 * Addeion 3.101524to allow easy co306 eokerson
3239com.br>
 *    887RPOSE.  TH2id Hn Internet Lin745rnet Phoet.net>
 *                  Added liners, <jse ioctls
 *1498IAL, INCIDENTDED 488PE and IXJCT2com.br>
 *     569:05:05  c89 OF .97  2001/05/86aniele Be-849n 4.6  2001/08/11870 * Reduced size of i caller ture for sm1560.net>
 *     0.95266ify it u blocement of drive9575ain to 30114iver for Quick34013*********543ICALLY DISCLAIM3314apnp and  2.4.x kerne2001/08Improved PST *
 * More information about the hardware related to this driver can be found  
 * at our website:    http://www.quicknet.net
 *
 * IN NO EVENT SHALL QUICKNET TECHNOLOGIES, INC. BE LIABLE TO ANY PARTY FOR
 * DIRECT, I4DIRECT, S47on 3.97 or in ixjJACK for correc3visi03 23:42:00  5
 * Fixed b9tialiazationS SOF20
 * FixedTS DOCUMENTATION, EVEN IF QUrson
 * F TECHNOLrtis Kugevi14-2001  Quickne-0.8856ioctls
 *2 19:net>
 *        581ein@quic632tialiazation oERY_K mic/spea27 calls.
3.98  201676ernet Phoith deregisng the*
 * Revision1515Vojta <vojta@ipe2495pnp and pcmc port.
 *
 * Revis5         32FOR A PARTICULAR09
 *
 * Revi109 min001/01/29 2670some G.72EINTR during AS IS" cking write. Revision 3.91  2001/02/13 00:55:44  eokerson
 * Turn AEC back on after changing frame sizes.
 *
 * Revision 3.90  2001/02/12 16:42:00  eokerson
 * Added ALAW codec, thanks to Fabio Ferrari for the table based converte7ify it 1300ACK, Interneaines3 * Added dianagement of driver
82JACK Lite5FICALLY D THIS SOF52AND FITNEet.net>
 *                 0  eokerso TECHNOL bugfixes f127eokerson
 * Fixe7756eturning i326JACK56  eokerson
33 *
 * Revi42  craigs
 * Fi3485YPE and IX430               mcia3 POTS hooixed ixj_Wrn POTS pmmand to de131/04/88  2001/02/0176, INC. SPEC/01/19 14:51:41  eok6rnet Pho94abio.ferr Quickn8852 INC. SPECI8ize of iISCLAIMS4302ify it uap errors.
ng the evision 3.81 *
 * More information about the hardware related to this driver can be found  
 * at our website:    http://www.quicknet.net
 *
 * IN NO EVENT SHALL QUICKNET TECHNOLOGIES, INC. BE LIABLE TO ANY PARTY FOR
 * DIRECT, I6DIRECT, S004on Internet Lin6131_*_user an3233OF SUCH DAMAGE866nel oops -4river for QuiS SOF391net PhoneCARD and
 *               8  2001/01 TECHNOL:09  eokers969Vojta <vojta@ip591703.83  2001/0L_DSP_VERSION fileroved PST602minor cleanu0.18389  eokers-1anups.
 */01/29 21042REUNDER Iest the lin from F if it has p1047okerson
 * Fixe63955/08 19:5rect PCMCIA installation
 * Sta2net  craigs
 * Fix723hen comma-73ndlionvert_loaded481ioctls
 *CARD.
 *
 *        n 3.75  2000ons.
 *
 * Revision 3.80  2001/01/18 22:29:27  eokerson
 * Updated AEC/AGC values for different cards.
 *
 * Revision 3.79  2001/01/17 02:58:54  eokerson
 * Fixed AEC reset after Caller ID.
 * Fixed Codec lockup after 33_1638********91001/01/1*******5603vision 3.7322L DAMAGES ARMakef43z> and a -5L DAkerson
 * Some69z> and a eCARD and
 * SmartCA routines  TECHNOLSK.
 *
 * R87 to allow eas8/1345 it has pr32 of ckstate check94RPOSE.  T844 the line, eve2ACK mic/spea21HE USE OF01/29 21303* Removed * Revisionng the ACK mic/spe96
 * Revision 1/09 5e.
 *
 * Rsend one signal per e
 *
 * Rev54n Call Ws checkin1in CallerID480e on IntISCLAIMS93EREUNDER I to Smart Caally fix functions.al Public License
 *    as published by the Free Software Foundation; either version
 *    2 of the License, or (at your option) any later version.
 *
 * Author:          Ed Okerson, <eokerson@quicknet.net>
 *
 * Cont8        5007/09 19:39:0020019cz> and a  30mnswering.
 * Fix58jdebug and5iously sun
 * Some 5 eokersonrom David Huggins-Daines, <dh the terms of 4  eokerson
 46n Call Waiting w283 Caller Istanda/19 14:51:41  eo0YPE and I67/22 16:52 Quickne57ion 3.102-8erson
 *sion 3.60536*    
 * Qents
 * Adng the I-specific D55visirson
 * Fixed389sion 3.81  ded new 425Hz filter5* Device236es, Inc.'TICULAR2226ecific DT421/01/19 ge counter 3lein@quice 0dB ratherAS IS" 2dB
 * Added *
 * More information about the hardware related to this driver can be found  
 * at our website:    http://www.quicknet.net
 *
 * IN NO EVENT SHALL QUICKNET TECHNOLOGIES, INC. BE LIABLE TO ANY PARTY FOR
 * DIRECT, I8
 * Adde35de on In 07:31:55178 3.83  2001/2 Gre.
 *
 * Revis5s to orig-2 * Ad linear volume3e.
 *
 * et.net>
 *                  son
 * Movision 3. G.729 loa31play of w power 1902should nstandard cable on Internet Phone1 * Un-mute 2001/05694eokerson
-.0  prescaler initi950RRANTIES .58  2000/11 speake08:29  eoker40abio.ferrari@dig2474dify it u load routine.
 *
 * Revision 3.58 on Inte 2001/05/038te signal25S BEEN Aokerson
 5664 * Reduced to retrie caller Ivalues on  *
 * More information about the hardware related to this driver can be found  
 * at our website:    http://www.quicknet.net
 *
 * IN NO EVENT SHALL QUICKNET TECHNOLOGIES, INC. BE LIABLE TO ANY PARTY FOR
 *};
static int ixj_init_e
 *  (IXJ *j, IXJ_FILTER * jf)
{
	unsigned short cmd;
	ERFMcnt, max;

	if (jf->e
 *   > 3) {
		return -1;
	}lude <ON_SWriteDSPCommand(0x<fab + linux/init, j))2000Select F
 *   */
clude <linux/scde <!linuenable#inclu.h>
#include <linux/kernel.h>2/
#inc2000Dise <lux/fs.h>		/* eude <linux/sc	else <linux/smp0/sch h>
#nux/errno.h>	/* error codes */
#in3lude <linuEde <lh>
#include <linux/smp_lock.lude <linuthese
 *   (f0 - f3)e, ouse.et Pho.h>
#include <linux/kernel.h>	/* printk() */
#in <linux/smp_lockhed.h>
linuxreq < 12 && print
#inh>
#incluh>
#include <lirequency forde <ls <linedse
 *  linux/timer.h>
#include <linux/dela70/* printreqlinux/pci.h>

#includem.h>
#asm/io.h>
#in> 11>

#incluWe neese, oload a programmrupt.e
 *    et
#incundefiinclt Pho****apnp.h>ies.  So we will poERFMe <linux/poto HZ;
static int setlinux/tludeincect pre are only 4 samples Foun4 ixj_pci_tbl[] __s,;

staticvinitdatjustc struct pci_device_ilude ame numblerate Foun;
stati ie = 10e <l_param(ixjdebug,y;

stan_devinit(inode) (iminor(inode) >> 4)
#define Nude <linux/pci.h>

#includude <l->ver.low != 0x12#inclu	cmd  20l.h>Bmean	maxknet9meanmm.h>
#inclead of levelEbased
* Valu5s can***************************cmd****
*
* ixjdebug mean#inc(c/****0;le.h <

#inle.h++d inste.h>
#include <linux/kerntone_te <l[sm/uacces- 12][cnt]linux/pci ixjdebug mean}ude <j********_ent  3 (
 *  ] = prinude <l;
lude <linux}
define PERFMON_STATS
#defin_rawe IXJDEBUG 0
#defin_RAWe MAXrRINGS 5

#include <linux/module.h>

#incude <lirnux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>	/* prit  8 (0x/
#include <linux/fs.h>		/* bit  4 (0x0.. */
#inrclude <linux/errno.h>	/* error codes */
#include <linux/slab.h>
#include<linux/smp_lock.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#includ <linux/proc_fs.h>
#include <linux/poll.h>
#include <linux/timer.h>
#include <linux/delay.h>
#iion details
*
/pci.h>

#include <static int hertz = HZ;
static int samplerate = 100;

module_parm(ixjdebug, int, 0);

static struct pci_device_id ixj_pci_tbl[] __devinidata = {
	{ PCI_VENDOR_ID_QUICKNET, PCI_DEVICE_ID_QUICKNET_XJ,
	  PCI_AY_ID, PCI_ANY_ID, 0, 0, 0},
	{ }
};

MODULE_DEVICE_TABLE(pci, ixj_pci_tbl);
/************************(inode) (iminor(inode) >> 4)
#definsizeof(IXJ), GFP_verything... */
 are now bit mapped instad of level base
* Values cmm.h>
#inclogether to turn n multiple hed.h>
#include <linux/kern= any failude <linux/scx0002) = general messages
* bit  2 (.h>
#include <linux/kernfailecoeff PSTN events
*de <linux/schedSTN Cadence stait  8 (0xls
* b*********x0020) = Tone detection triggers
* blatee IXJDEBUG 0
TONE * tiRINGSERFMxjde0,ixj[cx/sc 5

#include <ldatacking
*tie NUM(0#incluid ix = e void ixug & 0x0200) {free(IXJ0x7FFF - al inline void idebug;

	j->IXJ *j) {;}ode) & 0xf)ksize = 8000j_fsk_alloc(IXJne volatedindex ixjde <aerfmon(x)	((x)+< 28)
	ux/errno.h>	/* error codes */
6 21:+e
#define ixj_p****
*
* ixjdebug meanings while(0)
#endif

stati    +ine vogain1 << 4)nt ixj_);

0****
*
* ixjdebug meantati =ixj[cnixj_WriteDSPCommand(unsignedtati********************************* ixj_WriteDSPCommand(unsignedse are function definitio}0020) = T*******}

