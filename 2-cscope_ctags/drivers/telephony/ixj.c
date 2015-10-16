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
	****outb_/************high************
 *    D*
 *   }*
 * j->flags.play_first_frame = 0c
 *
  else	if (j->r forcodec == G723_63 &&*
 * Driveephon Quicknet T) {*
 * et P(cntechno eJAC< 24ite,
++Internet	BYTES *
 *    i;
*
 *  ifne,
 K= 12PhoIntern***** * SmartCCK Lx02 * Smarand
 * Smar * SBLE
 *nologi*CopyDev	, InoneCARD c) CopyrightCABLE) Copyright 1999-2001 PhoneJACK, ec.'s !Inter% 16)ding!IsTxReady(j)oneCes, IndlyCK Li Qui			while (/orCopy   modify it und redidly++ > 5lic Licenseder the terms of		breakSoftware ou can 		udelay(1nologi***ther vneJACcan ***********
 *    i later version.
 *
 * Author:          Ed Okerson, <eokerson@queneral ixj.c
 cknet Teicee Internet PhoneJACK, the termseg thehoneCARD  Lterne * Telephon, or _size * 2*     +=cknet.es, 
 * stribute LicandU General Public Licene Free SoftwarFree GNU General Published by eneral as p    shed by te Free tion; eitoundation; eiLicensesoneneral 2 ofFree et Ten@qui/* Add Inte 0 to G.729ther vs ckneree 8021.  Rm is now we don't do VAD/CNG  */tineseJAC        y card     i9 andPhoneJAC0 ||     , ors, <artis@mt20blic LicenrediTelwrite_buffer_rp[cnt]tribs,ing th*eral * MoBellucci, <b infod + 1a@tiscali.it>*
 * CoMore inform    
 about t2e hardware related to this driver can be fou3e hardware related to this driver can be fou4e hardware related to this driver can be fou5e hardware related to this driver can be fou6e hardware related to this driver can be fou7e hardware related to this driver can be fou8e hardware related to this driver can be fou9e hardhoneCARD /* someone is tryingal.cre in silence lets make this a type 0 *
 * .tro.com.(at your soft later version.
 *
 * AuthoCLAIMS ANY WARRANTIES     INCLt>
 *
 *ieshnolHE POSSIBILIT all     r *
 * Moare,LIED.1ICALLY DIST NOT LIMIT1D TO, THE IMPLIED UDING, BUT NOT LIMITED TO, THE IMPLIEDTED TO, THE *
 * Contr later vielere information  +erne later version.
 *
 * AuthoA PART HAS NO OBLIGATION     O PRt t************cknet.net>
 *
 *S, ENHANCEMENTS, OR MODIFIOree SoftwaS, ENHANCEMENTS, OR MODIFICNTS, <gherleiaines ENHANCEMENTS, OR MO=
 * MorBelluDavid W. Er*****DanAS NO OBLIGATION
 * >copy_re informationndPhoneJACy_*_user     minor leanup.*
 * CoRevon 4.8 ANY 3/07/09 19:39:s_empt as ITY wake_up_ihoneruptible(&raigs
 * q);ABILWCHNOany blockedneral rsCALLd John And     phon    2.2al.cpoll craig3 06:b>
 *s checkinselectdded isio++Free * More itennd J}
ES OF MERCHAj->dryormati3 06:}
K, Ij->ixj_signals[SIG_WRITE_READY]PhoneJgs
 kill_fasync(j, problem in hand, POLL_OUT
 *
 
}

static int idle(IXJ *j)
{03   ( PHOWe inDSPCommandIMITETED T))TABILDSP Idle_CODEC returne te:58:12j->ssris fre||et.neld@cepshoneJber     AdODEC pddit5onalephonmod, <g-1nd JFree PhoneJACMAGEhe termTeleec     Added agging o3  2record08/07 07:24c and e1;
gs
 mana
 * Added ixj-4 set_t>
 *
 *Da1/llow ,nter
 d W.07:5un* FiHONEhort cmd;
	ver
cntAddedsoftj Devicecid_ephonaec_level  crairai PHO* ;
	ed IXtopy of 6cknet.net>
 *       1 *    tcknet.nd ta 00ay o)****** *    ::58:tagbullowtargetal.callow auty configur Addet rathedsp   Th!9-202" BASPOswitch ( numbhoneJat yo30:t>
 cmd99-2007F terms/* Set Bt>
 F or (S* Auto 240 pg9-10 thiFCALLY D>
 *    
orrec2 PCMCIA installaAi      Startvid changesnet Pback16rd Linux compatibilityaigs
 * Adde1on 4.0  2001/08/54 12:33:12  craigs
 * New versio8using GNU autoconf
 *
 * Revidefaultn 4.0ging o1onal  * Added ixj-5 1/08 * Au== 30vid rc and eanumJCTLhnol cadQUICss
 * Add4.4 raRevertIXJCTLCs
 * d
 * @quirg of vert>
 PECIFIs free odsal.cree Ma  This
 FFsvision 4.1from Ragging of ver
 * Revisikefiles
 *r than ssy c:47s
 * Added ix3.1 *
 *
 * RJCTL/* If thisddedus c and edSUCHrsio t (ing 9 comp) this *
  failedCALLY Revert * Updated HOWTOe soft and thn to 30dB on Ine softPhoneJAgfixes from R Add
f PHOP_TYPEartDEC,ber 07:19Vojta <vojtat>
 55  eok      en craidInterd easReve     ayvid :   ionratmber iaded etvaion <dhde:47and ixj_cdsd soe .bso thichae .dthoneRevis Robclun 4.1/08/05ve
 * R3inste12hort PHOconvert_loadeing o7  s instelucci
 * Auet Pnd Jos instea <vojt term:16 initialved  to thgs
 * Add*
 * Rev100onal 1/57/02 fro18:27 initialiazjta@ipex.czf ixjdal.cECHNOdynamic*
 *
cer canposs* 2. termsari,pass /08/0 between func   
s 2001eadvid arconvindexxes fr TS85he way the  00:17:3kerph2lv>
 on 4.3  2ts85uring a PSTN call to allow local an6weringsion Fixed speaker Pube on.3  2001/ing nterns
 * Added dB on In48he way the POTS and PSTN ports interact during a PSTN call to allow local a9nce.  Thankscci
 * @ipe <cat@waulogy.sta drid.edu>.
 *
 * Revision 3.91he way the POTS and PSTN ports interact during a PSTN call to allow local a8  Werari,* Fixed speaker mode on Internet LineJACK.
 *
 * Revision Fixional 1/05/09 14:11:w pass IXJ * call to allow local a4rd, instead4 23:09:30OTS and PSTN pNow uses OF SkneJAl timernet Peach c9rd,rnet LineJACOF Set Pree entireckneton 4.3  2g729uring a PSTN cternet LineJACKerelat thi
 TECHN *
 * Renet>
when to spePhoneJARevis0xAon 4.0c
 * t *
 *
 *to al ansnce.  linear volioctl
 5/08 00:01:04  eover. inst2/5 01:00:0n functiineJof ixcadcall to allow local anisionducavidd W.oional chllerID4/25 22:06s insS and PSTN ps DavidquawkineJat beginnineJoBfnd Jocom>
3.1 visi * Revision 31:04  eoion 3.95 4/031/0442:
 * S and PSTN p.2  2 linear volumerson
 nd easyixedraw filter o marson
 * gs
 * Added ix in CallerID.
2inear vced size o9  eokerson
  checineJin CallerID.
QUIC. rint.
 *
 *fnet> structurlls.
 smizes.teract daller1:001/05/08 00:01:04  eo24  eokerson001/022:5922:02:59  eokerson
 isapn * ReapULAWn 4.n on POTS ports.
 *
 * Revisio 3.91  2001/02  2001/02/13 00:55:6  terms*
 * Revision fter chanteract .
 *
 * Revisiom A*    Kugevif ixj structall to allow local an.correctly.
 *
 .
 *ks to Fabio Fe4inear * Revision A 19:20W fr.
 *
 * Revision 3.89CallerID.
12 15:41een functio  eokerso*
 * Rrectly.
 *
 cs - TOF Sgaeokeweeithopnp ineJset cRevistlyion 3.87  2001/01/29ard, inst2/051/0425:4222:02:59  eokerson
 checup bugs with deregiste noLINEAR16* Revision 3.87  2001/01/29 21:00:39  eokerson
 * Fix from Fabio Fer 23:5 <fabio.ferrari@digitro.com.br> to properly harari <fabio.ferrEINTR during non-blocking write.2ndle EAGAIN and ight date.
 *
 * Resion/01/296CallerID1/23ionalvision 3.87  2001/01/29 21:00:39  eokerson
 * Fix from Fabio Ferrari <fabio.ferrari@digitro.com.br> to properly handle EAGAIN and EINTR during non-blocking write.
 * Updated copyright date.
 *
 * R * Rev * Revision WSSo propame *
 *pown <cat@ion 3.87  2001/01/29ion 3.95 1/221/0432:ux cS and PSTN pSdulebugfix New9:20ci
 * Huggins-DainS OF<dhd@cepstrE.
 om>3 06:AND F c craigty for 2.4.x kernels8 CallerID1/19 14:51:41 7S and PSTN pf ixj struckfree * Freadformatiit>
 all to allow local aasy cnow pass IXJ *S ports Fer on Inte. = NULLfrom FabiUpdated A.8 * Fixed01/kerson
 * Added ur pcmciic and eernet azer can  2001de PHOhon 3.1hange2y of vber in /prox/ixj  2001/ee softftwaaviourg wr Updated A
cmci101  EC re han3 06:IXJ}bss ij-re nEC res *
 *eallerojta <vojta@ipex.czbacFE * Res
 * Put thisgitame fullmand to  20CULAR:58:c* Fixbug & kerne2.x kp1:00k("ller%d Sange08/0Ron 3. C     %dsapn%ld\n"19:26boar 06:rnet LineJA, jiffies)er sizesls.
 *ass IXIXJCTL *
 * Ree, evraw f ils.
 dle EAllerIDthe w0  2001/0513S port*
 * Revision s Daviree wokerson
 pro. 2001*
 * Revision 3.9 21:0ernet PhoneC0s
 * TrueSpeech 8.5.4.x ker2001/02/13 00:55:ionalernet PhoneCls.
4*
 * Revisio4.8ubliie onevision hookvisiack oernet PhoneC4 POTS p
 *
d rawthtoconemevisioion dard caver deten Intwaulth footprson
 * Added capo mode ALAcmcia Pubernet PhoneC * Revn08 22:usf ixj structgfixes fJACK.
 *ert Vojta <vn be@ipex.czwrite a  Intern/01/19 00Robatteucceake dsize WaitineJkerne sizes.
  routevisteracr differeEC/k eokoc.7hat itlucci
 * Audit, GFP_ATOMIof the o PE a FS>.
 *
 * RhoneJAs.
 *
09  Mike 2000/6 froer canet.net>on Inlest 21*
 *!ow t6 03/12/0 laterdlected.ENOMEM0dB on Int New vedii, <atio of t1:323:08ack on aftMohe Fsizes 2001/01/1Revis404/2  5102nd a ee di2  cPoll Y_CO4.x kfor ges  and eRevia  2001/02n Calds
 * Addedari@di000/1ate.
 *1C0e  200om FabiL:5 *
 * rson
 * Fixed4he way the POTS and16  eerhart@quex.cz* FiEeviss
 * ster.
 erson
o mode on Intrt Ccapa.
 1r 2.4.x kernels6ional ion on ort gs
 * 21:00:395/0gy.stanfhoneC es, I *   :21  cra82001/02/13 00:55xer voi0/122001 14:11:23:cra8<vojta@ipex.czmixicenoibuto.coort +6dB1/0453
 *
an 0dB  2001/02/13 00:5567son
 * FFon
 * Fixed PSTN rrson
 * Fixed write signalF <vojta@ipex.czevision 3.66  2000/11/29 72N ri4TS port when kerson
 PSTNxj strility /30 21e.
 500/12/04 d PSTN rf ver * F1/29 07errorty for 2.4.x ken 3.66  2000/1checks rn before CallerID data.
 * ixed PSTN r 21:01/08/05j-ver. Callertines twhe  eokerson
test rom FabiRemov/01/17 02:0 eokerson
 *voidDaviAn not usiopay of vber i* beh c.4.x kernels7ing del per e 04educ52opp2/04 21:29:ord.kersowillrari,kerson
 * in76  2001/01/t he Prrevious  eo8 04:0 on Call Wa *
es for differeEC/AGC val.
 *
 * Revision 3.70 ne.
eokersIDpass IXJ> -1 additiona 3.68  ed PHONgort S2from F01/18 22:29:27  eokersnet Tty for 2.4.x kernel00:55dB
 * sion 38 11vadonvert_loadedargAdded dison 35.
 *
 * Revision 3.6 * Fix3Fon 37 1 3.82  ck on after cha 2000/stopE.729 l3.6ines.
 *
 * evis 06:depthonvert_loadeder.
 *dded diser.
 *> 601:3329erso= rson
 om>
 *and< 85vision * Rie work on G.729 loadrrors in80 +ocks as.
 *
 * 5 AGC 8:2able badtmf_presca Caller 7loadedeneraon
 y w4 05:35:17  craigs
 *CF07.729 loialisokerson
 D romimpletal ito retrieve mver
galu Fixeford.edu>  eoke init startup
 * Fixed spision5.729 loCallerIther than sh<< 8 rt *
 *
 * Ro retrieve mixer valurom elling/23 02:evisioelling52:11 everted IXJCTLRe=n 3._AGCina1  2 behlonvert1:38cat@3.58 nal per KERN_INFO d AGC /dev/    e modet12/041  2Thresholdrsio0x%4.4x/28 04:05:44  eclude "behav robertj
 * Added cvs c96.729 lo5n
 *rtuphanks David Cll.60 mista0dB on Int0ion 3e0ms DEBUG 0
#defng dMAXRINGS 5

#01/07de <lg GN/moduleC modes */
#om FabiVllingt.h>es */
#include <lsched_lock.h>
#include <lsquawk.h>	/* pr03tk() */es */
#include <lfsrt.hh>
#everythieokerson
 * Fixeokersonelling_ wink .
 *
 */

#include "ixj-vnt new>		/* ev 00:ecmax/12/04 21:2y of AEC modes in AUTO any cas<linux/interrupt.h>
slab_lock.h>
Lock.hde <asm/uaccemp_chec_lock.h>
#include <lmm_lock.h>ifh>		/* e> 10kerson(i19:3adedhone W codec 21:29:3
POTS OGIEshould nrivel versheRINGceins i>> 4)
rt.
er motable.imatie000/11fixeaus <mprevision 4ctectioablehardwOR A:309 1 Fixed 21 samLIED... */
0  QTI_Px/ixternn 4.
 * _lock3.10349:27:0rson
 * Fixed_id 
 *
ci_tbl[] __y tanitdata1rari <nclu cvs IMIT2/
#incl	/*V 3.66Leftapnp.h>
unm>
 *6dback on 
	  PCI_ANY_3D,TABLE(pciID, 0o Ferr0},
	{ }
};

MODULE_DEVICE_TThis(pciC
 * ReE(pcMono1********12********TABLEVENDOR_ID_QUgs
 pET, _LITEPCI_DEVICE_ID_QUIC C {**** ixjdebug meaningsFOR rariPCIABLE*******eaninICKftware****d
* Von 3.5canritees,  mapped instead of ultiple messagf ixj structlected.
  * Reinclude < = (DEVICE_ID_*/mm.h>
# /(ino;
	g *
 * Rinux/s.j,#include < log keywor craigines.
 .x kerne3 (0x0008)   2001"ix.
 *
 *"
rrno.h>	/PERFMON_STATSrrno.h>	/e <linux/errno.h>	/* error codes */
#iGes */
#include <lini\n"#include <linux/ioport.h>
#in801/0 <linu*/
#include <lerrnort.h>
# 2000 /io.h>
#inctails
* bit  t.x ke%2.2detai*
 *kersor than s later ve* Re2001/07/05 5:20 ter chanurame capanf
 *iesh.  T..<linux/interrupt.h>
acking
* bit  9 (0x0200) = CallUICKdenux/isapnp.h>7 (0x Add) =rson
  trang fr
* biSOFTW later version.
 *
 * Author:          Ed O/
xp aflloc-specCe diffn
 *te /mm.h>
#includelock.h>
#<linux/denclude <ln
 * _lock.h>
0);Revision 02/12  re ny taceD_QUgs are no mapped instead of =evel based
* Values caICKNETixj_pcer to turn on mNET_XJ*#include <mea pcm be or'ed bit mapp Cabet LineJACs
 * Re messages
*
* bit  0 or' portge Lice * Rurr G72m* bit  1 (0x00es
* devit  0
/*
 *01llocunds21  ureata) {
	1j->fskd2llocget>
 * f(!j->fskd b 2j->fskd
	{
		if(ixj[ce*/
 
 {
	f(ixjdeb4tk("IXJ*ted
elatj = kmallocode)rintk("IXJrted
/12/rintk("IXJ pessagbal messse)
		{
	*
 *JMAX; cnt++)
	{
		ifXJCTLigs
 * Added 00/1go iers,Callocate a free IXJ devit8
<lin 			j-nstead J *ixj_alloc()
{
	for(cnt=0; cnt			}
		}(MAX];
#* 256catep fouh Revisi58:5icients
 * Add*
 * Revi  eokerson  eokersoncofine Per i (0x0020) = To *gs
 *etailslude <lpci_loces */
#inclasm/ing
*es */se)
seet>
 lskdaturn &ixj /proc/ixj
 *vior fine PhoneJ2001/0ng 30ms ing... */
#inclu6  2001/01/0
statiArde <linvoid ixj_fsk_all inline v*j) {; Revision Bining d/11/j == fsk_6 19:B2.4.x ke4 05:35:17  craigs
 *E0267  2s
 * Movee de*
 * Res
 * Add *
 * Rn
 * Added checking toe3urn &ixxj strpa4 05:35:17  craigs
 *B0c16  /
 AddeEC Onfmon(x)tion triggersgs arerComm1ixj_pci_t AdvancedBellD 3.68  20 *
 * ReULL;* Revisie ixjEC_LO * Re	do { }  the (0)
#endif
k
 * Re
	}
}

#else

stati2 = offCommand********
*
* These are E01int ix 
sd(un* Fiavidj
 *,xed spFFFom>
 *ln Inta
 *
ibiliersios.kdatCF9on 3.533:12  cse)
Enaboaded Internet *kdataThesebe or UpdatedsingitoexoneJal			 */

#i****
	}
}

#elseMEskdat********
*
* These are f6* J, unsign:11 nterRevision 3xn mediumg ar0x0008)NC ix3.86  20
* enh*****XJ * J, u*********************008ision 4 later version.
 *
 * Author:          Ed Okerson, <eokerson@/Revision 4.4 Stub1/08/0 J, ality callo isarg)
{
	bers
  0*********ic HIGH_REGFUNCort, Pre*    = &e(IX
	}
}

#elsJxj_add_timer(Iost*   s freb;evision IXtimeout(unsignereWBell);
statistatic int read_filters(IXost*j);
static intevision 

#endif
 funknet T1/08/07 static in

#endif
 Bellu*, int mode);
static int ixj_seJ_RE_ng att mode);
static intAGC:, unanagggs
  messages(IX/* Fhone, <fhav>

#ipixedsup****inrt itatic iauto(inode) 44  urn &i000/11/andlnflic 200th itCALLY D*/

static int Stub(IXJ *x)	((x)++)Ame
 u 00:55d.ed08/0factornt r2static inobertile * * Rest mode);
static incluord.Monitots(IXJ *j,1* J, unsignHigD FIID generatFlooJ *j) modules to register
* enhance);
static67  Tr 200 06:Lockstatic in * Un-GFP_KERTonereturn j;
		1/08/07nnality call bac{
		j->fsRevisc_enerat1/08/07sometgene224e = &Stub 3.82  2signed short, IXJ *j);
1ision 4.4tt mode);
static int ixj_seplion 4.4  x*/

static int Stub(IXJ *

********4.4   Cabaeneratat 3dBatic int set_rec_codec(IXJ *j, 33->the 33:12  cEcho Suptic ser;
statget_recJMAX;tatic int/* Nrmatifcan*****ord_sGart(11  r pa or gs
 *c voectedilong c void ixj_write_frame(IXJ *CF9 J, unsign
static Iinumum gringc void ixj_write_frame(IXJ *j)sion 37ity cal0.125 (-18dB) daa_tonnt mode);
static int ixj_sl);
stj_Wr mode);
saxic vochar cr);invoidadt mode);
static 1);
static void 16 (24 *j)cr);_timer(de <linux/ioport.h>
#iniay_PE a1/08/07.****erso ixj_linetest(IXJ *j);
static int  8;
static void 8 (+XJ *j)tic t rate);
statcr); * Acat@1/08/atic void  DAA_Coenerat.id -s@mtesid ixj_aec_start(I4.4  F4
static void 2 seco fix(unit Fg, i250uvision 4.4  xj_cr);:07:1XJ *j);
static Austr

#enermany(Attion T;
stCo54  n
 * Aresigned short, IXJ *j);
sum
 *  lringbackmsion ;
static int ixj_init_filter(IXJER_RAW * jILTER * DecayJ *j);
static xj_seodec(I_rawhort, IXJt reFILTDONE * ti)fr)4096evision 4.4  t ixj_setstatic intE __us  8 (0_user *ERstati;
statD generatatic int ixermany(ff_Auosityi2;
static void 25% *j);
static int ixj_init_filter(IXJread_frame(IXJ *j);
static void ixj_write_frame(IXJ *j);*j, int moid i_linetes_potXJ *j, iXJ *j)nit_;
UTOeokerson@quicic int Stub(IXJ *e_Coeff_Australc void igned short, I_Coeff_Australinit_fily carowSCI(IXJ *j)ratRD wAddress

#endif
vJ *j);
stXJ *j);
statxj_build_filterr forinclud ixj_aec_start(IXJ *j, int tatic void DAA);
static void ixj * Ance(_ostatic int ixj_s);
sks.
*
*e_frame(IXJ *j, int size);
stffnality call backs.
*
*e_frame(IXJ *j, inttic ince(IXJ *j, Iixj_lnce(;
static int ixj_seraige_cid_bit(IXXJ *j)s
 * );
static void 2001/08/07 *j);
static void iof istat************
CT8020/CT8021 HostffProgrammers Model
Host****** set_base_frame(IXJ020/CT8021 Horsio(I of eDSPCommt<IXJMAX; cnion 3tic inlings
 * Added(/11/_timeDAA_C
staOr voic********fl	j->fsksize =8000;
}

#endif

#ifdef P1/08/07 07
	ice sk
 *
 = 800j);
ste are f
#ifdefTone dete(unsigned short, IXJ *j) mod
static
#, Inigned short, IXJ int Lhangednality call backs.
*
*07vision 4.e_cid(IXh int raigs
!
 *
 *neJACrol ash dr inp)
	atus R.86  20			 *	j)Only
BWORD wAddressECode
 ne(ICroc_#incles */
#incl          river to makhe .d initi"IXJ phonon
 *.rames            ****QUIC  2001/02/13 00:55.
 * Fixedormation the POTS and PSTN ports interact during a PSTN calle Bellucci
 * Au  cr 2001/05ort (buffe* 0  eokerson
 * Now uses one kernel timer for each ce on Intayon
 * 2000 06:-spec/12/3.58teract duof ixaits.covisites.****locanb_p(j-7 01:0g.
+ 9));
static vo8000;
} *j)I inttrol*    mtware Contruct fil3.75  20instead8 23:1n
 * Fix from Fabio Fed /06 19:t  9 (0fer xe int IsPCControlRe * Rev Th Control pccr1.by;
staontrolRe>XILINX*******3****r(IXJ *sr.bitte detinstead8ion 55:33cients
 * Added card-2000 messages
ilitint IsPCControlRis*j);necIsRxRhsr.bitssiontusrdy ? 1 :*j);
static voyte = inb_p(jR *    tic inline int Isquawkioops08 22:s arestruc* Fixed QUICetai
	return j->hsr.biather thaj)
{
	j->pccr1.byte = inb_p(j->XILINXbase + 3);
	return j->pd_Haf some G.723.1 calls.
 *
 * Revis dr
	return j->hsr.bit00:55:5  e void set_play_volume(IXJ *j, int volume)
{
	if (ixjdebug & 0x2000/ule compatibility for 2.4.x kernels.
 * Improved PSTN ring detection.
 * Fixed wink generation on POTS ports.
 *
 * Revision 3.91  2001/02/13  inb_p(jPC->XILINXbase + rnBUG 0sion can fe NUgs
 *ingdy ? 1 : 0;
}

statfootprint.
 *
 * 2001/02/12 16:4e int IsPCControlRe foot01/01.
 *
 * Revisioo 
#inclu\n"*****the d,generat****int mo***** Changed(0xCF02, j)35:16uleU autoconf
 *
ls.
 2..
 *
quawkision Improns i-specific DTMF pion***********winkf (!j-> for G72 2000/12/ty for 2.4.x kernels.
 *
 *:39  e3(volume > 100 || vol******inux/module 00:5etnes tord.ar P IXJVneratit}

	 2.4.x kernels._HSR(IXJ   eoke6:4e int IsPCControlRe */

#ALAW0x020c,44  eksto 0x%4.4x\n", j->board, volume);
	ixj_WriteDSPCommand(0xCF02W*
 * Revision 3.87  2001/01/29 21:00:39  eokerson
 * Fline int IsTxReady(  2001/01/23 21:30:36  eokerson
duledspplaymax 
		}be fo of tord_/12/05:20 }
	retuCK:
		if(j->port ==son
 *rved)		Read and + 9);
}

sta>DSPban 3.86  2001 *
 * Revision 3.8s in AUTO a29cale00:3able based conve;
	ing rin:
	e int Is-1;
];
#newenerati= (dsptic >
#i*alize th / 1ead 	 * Atic igned shj,ude lize the pr(IXJ *j);
static vo8000;
}

#enlay_volum
 * A*************
 * AContrshould 22:42:4AEC back on after cNC ixom>
 * autoconf
 *
.
 (ixjdebug & 0xSR(j)mmand(0x5280 + depth, j);
}

static inline int get be foplay_volume(j, newvolume);
	return 0;
}

st&ixj[(nline void set_play_depth(IXJ 2:02:59  eokerson
 0xf)

stWriteDSPCommand to decrement usage counter when c;
staeneratt hertz =hardw Revisiondepth, j);
}

static inline int get_play_volume(IXJ *j)
{
	ixj_WriteDSPCommand(0xCF00, j);
	return j->ssr.high << 8 | j-evision 3.86  2001 int moed volumes be
			decrem = Huj->f coune NUd rawc	ift ==/12/0== PORT_-spe)evel		);
}

stati= 0x48;
#des, In	break;
	default:
		r get_p];
#dounda;
	c****_id ixj_pci_tw bitrite;
	default:
		r38ume(jnewvoluixj_ng a3.94ad card-specing detecte informati) */
:57;
	return 0;
}

st1/01/18 + 9);
}

staRevision e;
}

static3.61  2000 3.5re informatio  eokerson
001/02/13 00:557 21:00:39* Revisi*
 *	Allocate a frerson
 Aephont Settinsizes. 
 *
->psccr.bffercate.
 *
, j->XILINXbase);
static votic ive (te bug ng 30m1/19psccr.bits.dev = 3;
	s in AUTO and AGC mode) {
	ca
		} n 3.62  2000/11/28 04:05:44  eoker         mproved PSTN ran 12erson
 */

 3;
	(IXJ *j)
while(0)
#endif
tection.
 * Fixedolume(jls.
 ng GNUfferences in tibutors:    Greg Herlein, <g.x k
 * Added1nt r******vior to 
{
	if (j->cardtype =331:3TATE_OC;
		ixj_r27ing detection.
 *et Phon2hanks erson
 * Added capabaulogy.stanfhone2es, sccr.bits.dev = 3;
	* Revisio2and 12:5 POTS port when 9:37 STN port is selected.
 ability for G722rn j->hs/12/08 22:41eits.controSUCHE_QUER05:20  eokerx kernel27ion 3.);

st801/041Added ioction.
 * Fixed********* by diG722Bsccr.bits.dev = 3;
	ines.
 *
2 * Afail5
 * Fix from 2 1;
				j->pslic.bitto havN) {
			dspint ixj_toneJ bef thi FSK.
 *
bits.txr
 */

# messages
* beayma_id ixj_pes, IXJCfrom1:58:1ellucci
 * AuditSTN po* Fixencluvior to ->pslicstatusrdy wAUTO anda <vosccr.bits.dev = 3;
	
 *
 *				fR6 03:/04/8) {
	case QTI_PHONE FSK.
 *
lerID ro}
/*ligs
 * Newrari lan Cox08/07nput)ANDBYriteme = (PLD_SLIC_S1; ACTIVsppla	c.'s T1);
	} * FiNO OBLIGAT					udit00/12igs
 * Added  ker22:29:27  eokerson +fRetV

stand tdown:
		me(jn -1;
	}
	volu		_HSR:29:27  eokerson_wlic.bits.ring1 = 0;aderdown = 1;
				j-on 3.60  202.67  2000/6  200ion routine.
 05:35:17  craig 21:00);

static 0;
}

static om Fabi@ipex.czixj2ude sr.low;
}

statiic.befficients
 * Added card-specific DTMF2C2
c.bi * Fixed write signa2Cavid K.
 *
 * Revision tro.com.iate, IXJ *j)
{
vision 3.67  2000/2C4;
			dev = 3ic.bi		dssccr.b4nt ring prson
 * Fixedess P:44  eokerson
 * Fixed PSTN ring dete2C5ase + 0x00);
			ixj_PCcontr5l_wait(j);
		}
	} elsefRetVal = false;
				break;
			}
			j->psccr6ase + 0x00);
			ixj_PCcontr6l_wait(j);
		}
	}pex.cz>erp);
d cvs c2:11  robertj
NC ixbeokere {
		/* S_driver to makdync(int2unctionable g 1;
d WriCtic v		dsld_	fRewstatu*****;
}

static i0x01****		j->psnd(0AX];
#d	ixj_fRetVa = trues.c1 b3wvolum	if (j->readers Tre work on G.729 l addREGFUalis driver canto
 * Fcxed Codec05:35:17  craigines.
 *
 * ReviTE_RINGING27  eokersondistic JDEBUG 0
#define AUTOsr.bi1  2cat@ccr.bit 1;
				}
				j-)1;
	r(IXJ *d_slicw.bitATE_OC
			;
	}
.'s !j->
{
	ixj_Wrd(0xCF00, j);
	rdown = 0;
					j-		breaj.bits.statusrdlic.bxj_PCcontanieleTATE_ly
E-j->cardtype) 05:35:17  craigs
 * 22********mo-F Hoepho;
			 *j,flush
	if (ds.ing0 2 re
ferent p
	ca9-40ite
C-D;
	retur's Tel of ty More work oACK
 * AdrounD_SLIC_STA4Only
nt++)
	{
ephon200		break	01/01/09/08/odule%;
stl to ale sin Calnow , jom>
 *_STATt */R			ixj_Pase PLD_3SLIC_id ixj_read_frame(IXJ *j);
static void ixAX];
#de(0x5Writentrotatic int rrsal[ly
2-3];rrno.h>		c voixj(b)	(&d_sl(b)])atic
 *	Ad_slicw.a fr.
 * FixePCcontrytoll(02/12 signe *Val =p,3 23ratCoeff_Gwaie NUse + 0x00);;
		maskr.byte =y of v Revisioxj(NUM(PLD_SL->f_path.dentry->d_in
statatus , j-****icw.bit, &inear ass I,******n; eit->XILI * Revisioase y >y Volullicw|=4.6  IN |4.6  RDNORM_CADEase urn &ixj[****icw.bytfRetVal ->readerits.c1 etVal = trueOUT

stb2eWRbits.c1 oe;
}rt ==case PLD_SLexstatus01);
			fRetVal PRI.bits.c3 =j->p		outwort ==en = 0;
	>XILnce(
	if (d/
ixj_l;
			dded disls.
);
	< 8,x01);
	0) & 0xFF;
		ixj_PCconec.bits.rINGSe AGC moff_US08/0.
cess.h000/11/28 04:05:44  ease mproved PSTN		if (j->rr ? 1 : 0;
}

stat>cardtyp Caller 6 set_plaase PLD_rt_jif_slioved PS
#en_SLIC_STATE_A
	}
	voe PLh>
#OHTrson
D_QUase forneJA=0;05:35:17  craigs
 *6 PLD_SLit1);
			fReutb_p(j->pld_slicw.by) {
	4
/*
 *JMAX; cnt++)IXJCTe1;
			stat.
 * Fixed Code_codee (Read) Data TE_RING_*****=			fs.b2newvolum		j->pld_slicw.bit6E0 *j, 	/* ActiveTse POn PeretVaalL_DS	j->pld_slicw.bitts.crrorINGters)w.bi		fRets.c3 = 1e;
		->pslic.bi*****PLD_SLIC_STATESCI_utin;
stSCIits.b2en = 0;
		s.c32  ts.c2d_scrr:h>
#srdy  Device;
}

scepstrx01);
>pots_Che d;



staci= SLICCTL_DSP_VERSION*
 * *j)
{origATS
#d ixj_ini3
 *    ts.c2 
	SLIC_	mslee.bits pre_to_msecst ==SLIC_STAinear 

	aders etSt) {et_recINGING:
				if (j->reade + 0x01);
	}st R/* error codes */
#iSCI j->t***** signeok%urn &ixj[*j)
{
	init_t
	}
}

#elseasy ratgi    ion 4.1atiaitL_e QTinclu = has pLow_timeout;
	j->(ld_slicw.bits.c2 
{
	init_timer(&j->timer);
	j->timee QTtme)	initmeout(IXJ *j)
, j-now
	SLICDSPCommand(0xCF00, j)tic int ixj_set_pots(IXJ *jnly
4-_set_pots&tatig atj->cstate >=. Updat!o->pslterte >outj->cadence_tLL;
			nality calixj_)j) {
			j->tone_cadencadpld_++;
			if (j->ttatiLow.expires ixj_tone80;
(hertz / strplONEJthe p
					ixj__state >= j-{
			j->tone_caden->XILIbits.b2eloadedODEC,j-6 19:ision 3.7[tation			j-pol
			jEndn 4. REPEAT_LAweout;
c0te(IX* Active LD SneJA(Iate].inRetVerfa3:51:				me = (REPEAT_ALL:
1;
			j-toto noNE_QUER);
	68  20gain to +6dB 		fRetfies DAA1;
		e_t->ce[j->tone_cs.b2IXJCTLne_ve diff****->pld_slics) {
			ld_s		ti.ft->cate].ind						ti.freq0 ]ear voto Intdencti.state * Re	brece[j->toneMABLEce[j->tone_cadence_state].ce_sta
						ti.freq0 = j->cadence_t->ce[j->tone_cadence_state].freIXJCTLj->cadeo.co0basedj->cadence_t->ce[j->tone_cadencEEPROMdence_state].freq0;
					. * Re>cadenccadefreq0j->tone_cadenc_cadence_state].freq0;
					n(j-ce[j->tone_cadencedence(j->cadence_t->c;
		if (!j->fskdatnc(int fd, st}
, UPDATE->tone_cadencyt->ple >= j->cadence_t;
					}
					ixj_ld_sli			fRetVal =INGING:
				nce_t->ce[j->tone_cadenc_cadenne_on_time, j);
					ixj_o.cots.c1 =	perce_cadencevior 			case RE;	j->t				j->pslic.b, GFP_KERNEL);
		if (!j->fskdatt ixj_mixerj->toe_state--;
					ixj_play_tonPrepar
			j->pld_sd disixj_s			ti.g> 19:newvol_p(j->pld_slj->XILINXixj_set_te) {
		b_p(j->pld_slj->XIe_state--;
					ixj_playbitspe ==ABLE(ixj_ival				aj->plruRetVaJ 0 &&(IXJ *on 1Fto a>>ic vxj_hooks
	}
}

#elmix.vol[reg]j->tone_cadence_statj->cadence_t->ce[j->tone2000PCI, I, <dce_tize);d HOWTO
		ti&t>timc1 = 1;
		if:
		dc  This->ce[j->
 * Fi int sizeBILI *j)->cade		j-ersal
staticgeta;
			la= in>tone_cadeectetone_cadencstate]j->t]OGIE		ixj_toce_turn &ix}
}

static[j->tof (j->cadence_turn &i3one_cadead .freqded int tone_o_NE_QUERY_CODlow_SIGEVENT  200tXJ *j)2ir1;
			j->celterDatatone_oj->tone_on(j->1j->ton			ti.freqnce_t%d\n", event);
			/*  of ->ton		outb_p(j->pld_slicwdaauring(ixj_se* p.h>
# in1;
					ixjurn &ixnc(&(.gaiurn &ixj[0x0100)
			printdire******gs
 * Fixed Revisi	j->p00= 0;
01/01/09_t->ce->cadencevent);
			/*  {
					eq[j->tonet->ceint ixj_
		/* see->ce[0].tone_on_tim****cr4SLIC_STATE_Aitre 20011
static void se_SLIC_STATE****R

	msx);
				SOP_PU_SLEEPn 4.async(IXJ *e_0x1evision 3.86  2001treg.RM	BYTE sD_SLIC_Scrc1 = 0;= 5 ? 1 : 0;
}

statihe pef CON
	reNTS, _scrr.bits.daaflag)9{
		daa_int_read(j);
		ifPULSEDIALrr.bits.dase + aa Dri)Devision 3.86 
			1;
m				ShadowRegs.);
	REGS->ho.cr4ff(j->ereWrite_SLIC_STATE.}

stpstn && 1 : iasct) {)		breaXR0.bg.AGX		if (j->crity bits.c1 =if(
 * Revisi	j->008.bits.b2ar;
	uniR_Zte(IXJJh deregister.
rdownAA RretuPhoneor
 inux/modulest will now should norm>ssr.low;
}

statif				lse {
				daa_set_mode(j, SOP_PU_RESET);
			}
		}
	ime, j);
					ixj_the wlse {
				daa_set_mode(j, SOP_PU_RESET);
			}
		}
	bits.
	ixj_Wre PL00;
}

#e_slicw;
				if(ixjdebug & 0x0008) {
					d j->caden****	j->k&signa>at);
.reg = j->m_ilogi					td, jiffiat %ld\n", j->boarwRegs.XOP_REGS.XOP.x
	XR0****inbytey Volu59 );
	if (j->tonm_t->Sdt has prev>toneld_slicing sigtreg.r.bits.daaflag)3;
					ixj_>cadesoftwar]) {
		if(ixjjiffnt],tic ;
		 * Revision tic int ixterrupt /dev/pho
	int var;
	uniog.)
	{
		i= 0, *j)intinitvar>fla0initXR0.regint.bitre%d at %ld01/01r(&j->timer);
	j->t_ringback(Idev/phone%d OP.x	XR0.bg.VDD_OK.bits.cnti);XOPXdev/phone%!= ALISermaID_ixj_ion triggers
* bit  6 (0x0AY_ONCE:
				"Can08/0owRecadenID Bt_tis free %d one%d %1/28 0a_set_mode(j00;
}

#eMENT:
					j->ton_REGS.R0 XREGS.XOP.xr0.bitreg.VDD_OK != XR0.bitreg.DXOPXR0 XR0, daaint;

	var OP_REGS.XOP.xr0.reShad		break;XOP.x= j->m_DAAShadowRegs.AAShadowRegs.SOPSI_cadencetreg.Cpstn && j-XOP_ Driver				if(ixjdebuX & 0x000XOP.xr01.bitre}
}

stati&j->tim.h f
       ksta     "IX.bCReowRegs.XO) 					tir6 19:t reccess QUIC;AAShadowRegs.SOPRegs.XOP_R%ld\n"nterrupt /dev/phone%pstn && j-tregd\n",SOP.ts.stj->m_DRMRinit_scrr.bits.daaflag)3LD_Scd_sliaa_int_read(j);
		ifLD_SLIC_Sffies, j->pstn_sl7breae = (treg.RMR;

	tate]f (d & 0x00080008) {
       d\n", jBf (!j->flags.pstn_ringing) {
				ix_Setti(&j-f ixj strucr.bits.daaflag)Ff (!j->flags.pstn_pt /dev/phone%d softwa	get_		daa_set_mode(j, SOP_PU_RESET);
			}
		}
	ence = 1;
			if(ixjdebug & 0x0008->daa_mode != SOP_PU_R->boardj->m_DAAShado!=& !(g Cadence a st->m_DAAShadowRegs.SOP_REGS.SOP.cr1.bitreg.RMR;
		if(ixjdebug & 0x0008) {
       NFO "IXJ Ring Cadence a stme(j);
}
ASha_CR_init_j, pld_s0008) {
					printk(KE->pstn_last_rmr = jiffies;
ate = %d /dev/p = j&&volumiffies;
		e RE*****if(i_timertil)stn_!ne%d                 pring & 0x0008) {
			ShadowRegs.SOPdaaint%ld\nitreg.RMR;
		if(i1 - j-.erbosif(iwupt /dev/phone%d at ixj_init *
 * (cr of rn1 * * Reviits.b2en = 0; 5

#incluinit_DAA Ca51.bitreOP_REGMRinux/modulr.bits.dev = 3;
	 Acti				if (j->cadence_f[4].state == 0) {state [4].on1dondex;
					PEATtion)					culogy. (100 - var)) / 10000);
						j->c;
			i1 = j-	}
		}
	}	j->cadult:
	[4].on1maRegs.iffies + 10000);
					} * _LAST_* (200 + var)) / 10000);
					} else if (j->caetVal  (100 - var)) / 10000);
						j->c[j->tone_)) / 10000);
					} else if (j->catro.com.b;
				if(ixjdebug & 0x0008) {
						j-R	j->cadence_f[4].on1max = jiffity to have differebits.psd was %New veESET);
r= XR0 0;
aaixj_insej->XILINXbasever
 >ex.bilast_rmd at ing break;
		case PLD_SLIC_STATE_A= jClearng0 C *
 a]

/*
 
		}
		j->pstn_prev_rmr = j->pstn_las.bits.b2en = 0;5/ 10000);
						j->ce	get_SOP_PU_RINGING:
			if (daaint.bitreg.RMR) {
				if (ixjdebug & 0x0008) {
					printk(KERN_INFO "IXJ Ring Cadence a state = %d /dev/pho				ixj_set_ton_on(j->cadenc and easydej)
{
it>
 * i <m_DAAShadULARERID_SIZE - || i <derhart@qld_scrr.byte long)((j->cadence_ff[4].on2 * (hertz * (100 + var)) / 10000));f (j->reaceallocif(ixj[=	dspnux/modu#endi) {
				if (ixjdebug & 0x0008) {
					printptil) && !(j->flags.pots_pstn && j->hook)) / 10000);
					ixj_set_ton_on(j->cadence_t->ce[0]		j->0) aaint.bitreg.RMR = 1;
		daaint.bitreg.SI_1d, jf[4].on1max = jiffies + 10000);
		f((timec.on2e 

/*
 10000);
					eak;T_LAST_* (evic- var)) /.bitreg.R = j->pff1max))) {
								
	XR0CID[										j->cadence_]- vaoofskd(j,inus, j
	XR0*pIn, *pOuIC_ST2 10000));
								j->cadence_f[4].on2max = jiffie7ce_f[4].off2max))) {
												j->cadence_f[4+.state =t ge00 {
	 + varts.ring0 = j->p>cadence_f[4].on3mireq0 = j7adence_f[4dence_f4].on3max = jifff (drrupt /dev/phone%d at %ld\00 - var)) / 10000);
ne%d at %ld\n", j->cadence_f[4].state, j->board, jiffies);
				}
				if (daaint.bitreg.SI_1) {                /* Rising edge of RMR */
					j->flags.pstn_rmr_f[4Get Vers);
	 = 1;
					j->pstn_ring_start = jiffies;
					j->pstn_ring_stop = 0;
					j->ex.bits.psjiff {
		nce_f[4]									j->cadence_SOP_PU_RESEll bf)

b		dsT);
{
							fies + (long)((j->cadentreg.RMR) {
				if caden>ex.bfpld_slic1 = 1;
		j->cadence_f[4].state === j->cadenc* (hertz-1;
	}
e_t->ce[j->tone((j->cadence= 4					j->caifnt = jiffies;
		
					j->x)))	time(IXJ j);
adowRegs.SOP_REGS.SOP.cr1.bitrng0 = j->he hes);
				}
				if (daaint.bitre.on3 * (heff2minringdence_f ixjring_ers ||(&j->tpIn =				ti.				.on2max = jiffie 5

#iCAOted.
 CAO.sizes.			} 		j->cade, j-rdencequickne		j->cadet
 * 			os(pIn[1]x01);
3)han phocardtypce_f[[caden (he0;
			if (j->r* (he2_f[4].off3min) &4j->cadence_05

#i			j->cadence_3)****6) |>cadence_f[4].fc* (he		ixj_s.ring0 = j->3_f[4].3bug & 0_1(j->read    tine,dgoard, = j*/
0/speak4void ixj_fcadence reve		4in1;
					ixjif (j-_f[4].c_f[4].op4tn21 Hosinclut.ne3max))) {
		1)3j->cad2void ixj_fj->pstn				>> 6in1;
		sion 4.5 N[4].else if falom Fabibutbefo+= 5, e	get+lume =}
	memt ge to +id, 0,rdownoterru_p_CIDLD_Snce_f[4].state == 6) {
						if((time_after(jin Livphone%d at %strncpy;
staid.monc.bi				,K miF  eone%d >DSP + (long)((j->cday=stat if (j->cadence_f[4].staoff1mi->psjhouries + (long)((j->cadence_f[4].off1 * (heminies + (long)((j->cadebitsoff3maxnumle %ld 					jff1max)))			j + (long)((j->cevisience_f[4]ax = jiffies 4].off3ma copy_= jiffies ++}
	volj-					ame(j->cadence_f[4].off			j->cadence_f[						a* ev+ (long)((j->c[4].of) / 10_f[4].state =t gees
* bit  4 int.bitreg.SI_1) {     on vt_tone_;
					ixj;tn_last_rmr);
		}
		j->pstn_prev_rmr = j->pstn_las_f[4].state, j->->calong)((j->cadence_f[4].on2 * (hertz * (100 + var)) / 10000));
							} else {
								j->cadence_f[4].state = 7;
							}
						} else {
							if (ixjdebugne%d at %ld\n", j->cadence_f[4].state, j->board, jiffies);
				}
				if (daaint.bitreg.SI_1) {                /* Rising edge of RMR */
					j->flags.pstn_rmr	} else if (j->cadence_f[4].state == 6) {
						if((time_after(jiff1) _after(jiffi->cadence_f[4].on3minence_f[4].on3 * (hertz * (100 + var)) / 10000));
							} el->cadence_f[4].state = 1;
						j->cdebug & 0x0008) {
   ->pstn_last_rmr = jiffies;
	} Interrupt /dev/phone%d at ixj_in)) / =c.bits.rfree IXJ devax))) {
					R5mr = jiffies;0x%xte = %d* (h				j->pstn_ring_stop = 0;
			ce_f[4].state =				j->_f[4].off1max))) {
	dence_f[4].ond was %1dot = jiffimacr0x6CXJ_REGFU *= 6)fRetV 0;
			j->pl

state].08/0O
{
	k.indexTolum->toMUST* b->wied PSTInterx01);
	fRetVi/d Chnclude <-specpld_sl  eokbe seized (							j->off- & 0). * Revisiakj->plhe								j->			}
			cr.bytGTATE_AA>cadinnclude <linit_RANtati4  er)) / 10000);
				R0-1		:
			 adence faclude , i:23:urj->c#endifLIS-Ajifft.
nclud 10000));
							} elic vonly g * Ad
	}
	,(daaint. Phonring_stop =init_snclude <
													j->cadot  0 %.  F].off3mtoustrali (her3j->cadmax))) {

			da %ld shan <pe WILL CAUSE A HARDWARE FAILUREse iTHEnclude <  time_before( *j, 
static void sepslic.bits.offrm.c1 = 0;rdy ? f[4].off2) {
			ar)) /cadence_f[4].;
	n_last_rmr = jiffies;
ESETce[j->tone_cadence_st>pstions		} e= QTI_P j->long= 7;
 * New ns. *j, I	daaint.bitreg.0{
				if (j->cadence_t->c {
					tilic->tone_rlyon 3x))) {
					nce_f[4].of3ma		if (j->cadence_tn1 * h1x))) r.bits.daaflag)i if (jNFO "IXJ DAAse {
							es + (long)((j->cadence_3; * (het_mode(j, SOP_PU_cludeior e = 1;
			if(ixjdef[4].on3 -1;
					j->cade =jiffies;
	}
	
		daa_int_read(j);
		if
	}
	swite);
st			j->cade 10000));
				#endoneJAine void set_ff2 * (hertz * (100)8Y_ONCE:
					ixj_cpt_stuacces_cadjiffies;
	}
	_SLIC_STATE_Roved PSTN/*ence_f[4].
							}
							08) {
					pterfacelinux/modudence_f[4].of3ence_f[4].on1max = jiffies +cadence_f[4].of3pe) (100 + var)) / 1000		j->cadence_f[_f[4].sta & 0x0010) {
 else if (j->ca (l		printk(KERN_IN 0x0010) {
			 (100)) / 10000));
									j->cadence_f[4]..on3max = jiffies + (long)((j->cadence_f[4].on3 * (hertz  * (100 + var)) /			} e3min) &&
						    time_bcallnd e	addMore cha(j);
	.state = 0;
					}
				}
				if (ixjdebug & 0x0010) {
				printk(KERN_INFO "IXJ Ring Cadence b state= %d /dev/phone%d at %ld\n", jard,
						j->cadence_f[4].on1, j->cadence00));
								j->cadence_f[4].on3max = jiffies + (long)((j->cadence_f[4].on3 * (hertz * (100 + var)) /es + (long)((j->cadence_time_before(jiffies, j-		j->caden0000));
							}adence_f[4 * (hingllow easy c:47PR:	ut;
.on1dot, rd, jifff (j * (ts.pstncadeeoved PPEAT_LAST_E			} eln 4.6  2001/08/13 01:05:05ase Pq);percetic Really fixed PHONag) ersal *ion 4.6  2001/08/13 01:05:052sion cra-= 0;
- >
#iies - j->pstn_fRetVal = tts.c3hat it sh_slic Red5:05ic.bits. on2min, j->cadence_f[4].oeq0) {ionsD lags.pstn_ringing) {
	ShadowRegs. 0;
						}
					} else if (j-cadence_f[4].state == 6) {
					>cadenceence_f[4].off2) {
			((j->cadence_f[4].of- max %ld\n", j->board,
						j->cadence_f[4].on1, j->cadence_f[4].on1min, j->cadence_f[4].on1dot, j->cadence_f[4].on1max);
							break;
						case 2:
							printk(KERN_INFO "IXJ /dev/ptVal14 Next Ring Cadence state at %u min %ld - %ld - max %ld\n", j->board,
						j->cadence_f[4].off1, j->cadence_f[4]].on3max = jifdence_f[4LD_SLIC
					ifin = iinear					j->cf (daaiedgeme state at %u min %ld - %ld - max %ld\n", j->board,
						j->c08) {
					p %d /dev/					} el.on1doint.bitreg.SI_1)ff1max))) {
							3ce_f[4].off3min, j->cadee_f[4].off3max);
				dote_f[4].off3max);
					aif (j->ca			fRetVal =[j->tone6:d /dev/phone%d at %ld\SLIC_ST
							break;
						case 2:
							printk(KERN_I4].off2min, j->cadence_f[4].ofIXJCTLx);
		n				j->cadence_f[4].on1, j->cadence_f[4].on1min, j->cadence_f[4].on1	
							printk(KERN_I{
					pdence_fj->cadence_f[4].off1].state == 7) {
				j->cadence3freq1 =0 - var)) / 10000);
		0 && time_af);
	licw je, j->X *j)EAT_LASolic.bit& d_sl Drifies {
				if (ixjdeif (breakife_f[4].state = 7e state at %u min %ld - %ld - max %ld\n", j->board,
						j->cg_stop = 0;
	j->cadence_f[4].off3mind /dev/phoDAA no ring in 5 secon		}
			if (j->cadence_ev/p].state == 7) {
				j->cadence5 && time_after(jiffies, j->psJ_mode(j, SOP_PNext	j->cadence_f[req0 =willu:19:n2min, 2min, j->cadence_f[4].oiffieop !D].off3dot, j->cadence_f[4].off3max);
							break;
					}
				}
			}
			if (j->cadence_f[4].state == 7) {
				j->cadence_f	&& time_after(jife_after(jiff0 + depth,  get_pbers
 ld\n", j->boarcadence_t->ce[0].tonejdebug &e;
}
));
							} else {
							R) {
				if (PLD_		ix5  200e_f[4].on3 * 19:						j->cade_f[4].on1min, j->cadence_f[4].on1dot, (hertz * (100)) / 10000));
								j->cad	}/ 10000));
					ntroon3max = jiffies + (long)((j->cadence_f[4].on3zes._ID		daa_set_mode(j, SOP_PU_RESET);
			}
		}ld_scrr.byte f[4].state == 2) {
						if((time_rintk						j->nterrupt /dev/phone%d at %ld\n", j->>boaSettince_f[4]son for
  go.60  20timer_mode(j, SOP_T);
			}
		}
->tone_cadenc*j);
static vj,lags%ld -LEEP);
				j->ex.bits.pstn_ring = 0;
			}
			breafore(jifOP_PU_RINGING:
			if (daaint.bitreg.RMR) {
				if (ixjdebug & 0x0008) {
			/ 10000));
						200on3max = jiffies + (long)((j->ca)) / 10000);
	f[4].j->flags.pst(j->m_DAAShae%d Next Ring Cadencv/phone%d at %ld			j!ShadowRegs			}xr6_Wg Cadd_sldev/patic void				j->cadencix			} else {
			].offcw.bitev/phone%d %ld\n", j->board));
							} else {
						en = 0;
			o		j-loca/ 10000);
	>cadencedence_f[4]SOP_Pint.bitreg.RMR) {
				if (ixjdeg.RMR;

		ixj_PCev/phone%d %f1, j->cade (hert (her (herf (daainjdebug & 0x000tn_ring_stop = 0;
;
			j->pstn_winkstart + ((hertz * j->winktime) / 1000A wink!j			j->pstn_winkstart = 0;
					}
				}
			}
			if (j->pstn_winkstart && timeib0ese QTIenpstn_winkstasoftwartn_wink = 1;
				ixj_killlfasync(j->board, jiffies);
				}
				daa_set_mode(j, * (hertz j- {
		on3max = jiffies + (long)((j->cCing_stopCOP.THF.h>	/rface 1[7					 SOP_PU_CONVERSATION:
			if (daaint.bitreg.VDD_OK) {
				if(!daaint.bit (10%ld\nar)) /%ld\ninitf (da6x);
			g if ostn_/08/07ce_t/08/0)pt*)&j->busyflags) == 0) {
		ix5*********&& urn ic_init__staDSP:
		d){
		tone*****_andt sizbit(RMR) {
Read  *)_stabusy Drivtate))					jix4 Only
A-Bstate >=PLD_S1;
			_f[4].state s_used)ookstate(Islicw.i3 (j->tone_state) {
			if (!(j->hookstate)) {
				ixj_cpt_stop(j);
				if (j->m_hook) {
					j->m_hook = 02
					j->ex.bits.hookstate = 1;
					ixj_kill_fasync(j, SIG_HOOK1 (j->tone_state) {
			if (!(j->hookstate)) {
				ixj_cpt_stop(j);
				if (j->m_hook) {
					j->m_hook = 0
					prob-spe_WINKe AnderIN;
			if(ix		fRetVal%ld\n", j->board, jif {
		itreg.RMR = 1;
		daaint.bitreg.SI_time, j);
					ixj_set %ld\n", j->boarnt ) == 0)	ality			j		j->ex.bits.hookstate = 1;
					ixj_kill_fasync(j, SIG_HOagbu(j->tone_state) {
			if (!(j->hookstate)) {
				ixj_cpt_stop(j);
				if (j->m_hook) {
					j->m_hook =2ad Only
A-B					clear_bit(board, &j->busyflags);
						ixj_add_tif (daaiies, x0008)in, j-SOP_Poard(100)) / 10000);
	s, jcpt+
0-1	slicw.A wink demfasyn					j->ctone) {
	 =2->cadencar =xes, j-
					ixj_kie, j);
				]) {
		if(ixj {
	IG_HO2K*j)
{time_before(jif	printkf[4]rtop(j);
				_hook) {
					flags.dCE:
					ixj_->flags.(j->pstn_ci DAA wi2;
					}
				}
					1;
			->tice_tj->cadenc
					}
on {
		)
			52O "IXJ 100 + v.c1 =gs.ringback) {
					ixj_ringback(j);
					irtz * (10)->fl{
					ixj_ringbacffbusyflags);
						ixjtn_prevme_aftee %d\n",
							j->			}
			arity  +(j->ti_timeout(jrn;
					}
				}
					ext Ring int ixj_mixer	ti.frnce_t->ce[mode(j, SOP_encesp****{
		>DSPit(board, &j->busyflags);
						ixj_add_timer(j);
						re3mer(j);
						return;
					}
				} else {
					ixj_play_tone(j, 0);
					if (j->dsp.low == 0x20) {
						3lear_bit(board, &j->busyflags);
						ixj_add_timer(j);
						re3 (!(j->tone_state
			} else {
				ixj_tone_timeout(j);
				if (j->flags.dialtone) {
					ixj_dialtone(j);3				}
				if (j->flags.busytone) {
					ixj_busytone(j);
					if3(j->dsp.low == 0x20) {
						clear_bit(board, &j->busyflags);
						ixj_add_timer(j);
						return;
					3
				}
				if (j->flags.ringback) {
					ixj_ringback(j);
					i3 (j->dsp.low == 0x20) {
						clear_bit(board, &j->busyflags);
						ixj_add_timer(j);
						return;
				3}
				}
				if (!j->tone_state) {
					ixj_cpt_stop(j);
				}
			}
		}
		if (!(j->tone_state && j->dsp.low == 0x20)) {
			if (IsRxReady(j)) {
				ixj_read_frame(jbitsit(board, &j->busyflags);
						ixj_add_timej->cerImpenialtoixj_r(j->tone_state) {
			if (!(j->hookstate)) {
				ixj_cpt_stop(j);
				if (j->m_hook) {
		0					j->cadpstn_wiear_bit(board, &j->busyflags);
						ixj_add_timexj_ring_off(j);
			rn;
					}
				}
			} else {
				ixj_tone_timeout(j);
				if (j->flags.dialtone) {
					ixxj_ring_off(j);
						}
				if (j->flags.busytone) {
					ixj_busytoxj_ring_off(j);
			j->dsp.low == 0x20) {
						clear_bit(board, &j->busyflags);
						ixj_add_timer(j);
					xj_ring_off(j);
							}
				if (j->flags.ringback) {
					ixj_ringbxj_ring_off(j);
			(j->dsp.low == 0x20) {
						clear_bit(board, &j->busyflags);
						ixj_add_timer(j);
				
								j->cadence_ (j->dsp.low =!j->tone_state) {
					ixj_cpt_stop(j);
				}
			}
		}
		if (!(j->tone_state && j->dsp.low == 0x20)) {
			if (IsRxReady(j)) {
				ixj_read_frame(j							j->nterrupt /dev/phone%d at *)&j->busyflaIM) == 0) {
		ixk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].state, jiffies);
							}
							ivar)) / 100			} ear_bit(board, &j->busyflags);
						ixj_add_time2 * (hertz * 100rn;
					}
				}
			} else {
				ixj_tone_timeout(j);
				if (j->flags.dialtone) {
					ixj_ddence_t->ce[j			}
				if (j->flags.busytone) {
					ixj_busytoe_after(jiffies,j->dsp.low == 0x20) {
						clear_bit(board, &j->busyflags);
						ixj_add_timer(j);
					2 * (hertz * 100)			}
				if (j->flags.ringback) {
					ixj_ringbe_after(jiffies,(j->dsp.low == 0x20) {
						clear_bit(board, &j->busyflags);
						ixj_add_timer(j);
				j->cadence_f[5].o
						if (time_after(jiffies, j->cadence_f[5].on2dot)) {
							if(ixjdebug & 0x0004) {
								printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].stmode(j, SOP_PU_R j->cadence_f[4 address	Func->flags.gs.dialtond_timer(j);
	
						return		printk(.ring0 = j->p == 0x20)) {
			i100 + varj->cadence_f[5].sta			printk("IX= f[4].on1min, j->cadence_f[4].on1do5->cadence_4adence_f[4].on3ma>cadence_f[5].statj->pl.ring0 = j->n)) {d, j {
				j->flags.dialton DrivedialJ *j)ffies);
		n1max);
							b>tone_cadencoff3dd\n", j10000);
	ffies);
							}
										j->ixj_read_frame(j0x2w.bits.b2			clear_bit(board, &j->busyflags);
								ixj_add_timer(j);
		
				j->cdence_f(boacadence_f[>cadence_f[4].offate n3	j->cadence_f[4].state = 5	ixj_read_frame(je_after(jiffies, j->cadence_f[5].off3dot)) {
							if(ixjdebug & 0x0004) {ff3dot = {
				i5;
							} else {
								j->cadence_f[5].state = 7;
							}
						}
						break;
					case 5:
						if (time_after(jiffies, j->cadence_f[5].on3dot)) {
							basee = %d - %ld\n",				j->cadencoff2 O "IXJ Ring Cadence b state= %d - %ld\n", j->cadence_f[5].state, jiffies);
							}
							ixj_ring_off(j);
							if ([5].statcidsen%ld\n", jdence_f[5].off3dot = jifs + (long (hertz ) {
				j->ce = (2:
	cadence_f[5].off3 * (hertz * 100) / 10000));
								j->cadence_f[5].state = 6;
							} els		clear_bit(board, (j->cadence_f[5].(j->pESET);
			}ce_f[5].off3dot = {
				if (ixjde6:
						if (time_after(jiffies, j->cadence_f[5].off3dot)) {
							if(ixjdebug & 0x0004) {
);
						ixj_pre_ci (ixjdebug & 0x0	}
				if (j->flags.cidring && !j->flags.cidsent) fies);
							}
							j->cadence_f[5].state = 7;
						}
						break;
					case 7:
						if= 0;
				Tele Hos= 5;
							} else {
								j->cadence_f[5].state = 7;
							}
						}
						break;
					case 5:
						if (time_after(jiffies, j->cadence_f[5].on3dot)) {
							iresit(board, &j->busyflags);
						ixj_add_timeFRR) == 0) {
	adence_f[5].off2) {
								j->cadence_f[5].off2dot = jiffies + (long)((j->cadence_f[5].offnging cadenintkear_bit(board, &j->busyflags);
						ixj_add_timelags);
				ixj_rn;
					}
				}
			} else {
				ixj_tone_timeout(j);
				if (j->flags.dialtone) {
					ixlags);
				ixj_			}
				if (j->flags.busytone) {
					ixj_busytolags);
				ixj_j->dsp.low == 0x20) {
						clear_bit(board, &j->busyflags);
						ixj_add_timer(j);
					lags);
				ixj_a			}
				if (j->flags.ringback) {
					ixj_ringblags);
				ixj_(j->dsp.low == 0x20) {
						clear_bit(board, &j->busyflags);
						ixj_add_timer(j);
				ookstate & 1)) {
						if (time_after(jiffies, j->cadence_f[5].on2dot)) {
							if(ixjdebug & 0x0004) {
								printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].stERN_INFO "IXJ DAUERY_CODEC,				ifO(j->dsp.low == 0X = j->ssr.high 
						ixj_preard,
						
						ixof ixne) {
				printk("IXJ		clear_bit(board, &j->busyfldsp.low == 0xxj_add_timer(j);
						return;
		jdebugk end  Drivedsp.low == 0xce state & 0x0008) {K mi& j->dt /dev_off(j);
							i!
					e%d at % ixjadersGIXJ *j)
j)		cldsp.low == 0xE_j->wri						} emeout(IXJ *j)
{
	IXJ_TONE ti;j->wrup_interruptib			ic void ixj_er(j);
			c_codec(IXJ->flags.drceived volumes betwe511Bf (j->ccnow;

	Realoresent) {
			ighXILIN |xj_rssr****flags.dialigh tone(j)&&(&j->poldsp.low == 0x		ixj_dialtone(j);var = (j->flags.busytone) {
					ixj_busytone(j);
					ifnc(j, SIG_HOOKSTAdsp.low == 0xelse {
				if (j->ex.bits.dtmf_ready) {
					j->dtmf_wp = j->dtmf_rp = j->ex.bits.dtmf_ready = 0;
				}
				if (j->m_hook) {
					j->m_hook = 0;
					j->ex.bits.ts.rit(board, &j->busyflags);
						ixj_add_timeAEixj_perfmo (hertf = jiffies + ((60 * hertz) / 100);
	while (!IsStatusReady(j)) {
		ixj_perfmon(j->statK_PCI:
		dspplgh << 8 | j->ssr.low;
				if (!j->m_hook && (j->hK_PCI:
		dsppl(j->dsp.low == 0x20) {
						clear_bit(board, &j->busyflags);
						ixj_add_timer(j);
				DSP write overl
						if (time_after(jiffies, j->cadence_f[5].on2dot)) {
							if(ixjdebug & 0x0004) {
								printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].stAperfmDSPCommand(0Rising SPCommand(0xCF00, j);
t id ixj_ijif	}
	sif%d at %ld\n", (6are _LASTidring 		if the GN!Is
	j-us*    modhook = 0ear_bit(boardreq0c_void (!(j->hte) {
			if (!(j->hooksta> j->dsp.en = 0;
			o%d Dsase 21  		if (j_perfmon(j->iscontrolreadyfail);
			atomicmer(ICc>XILIN_ase  + 3);
	retutic void ixj_ije(ate) {
			ifytes.low = cmd & 0x00FF;
	jif = jiffies + ((60 * hertz) / 100);
	while (!IsControlReady(j)) {
		ixj_perfmon(j->iscontrolready);
		if (time_after(jiffies, jif)) {Beturn;
					 (or
 *    modtimeout(j);
t_port(IXJone1->DSPWrite);
			if(atomic_read(&j->DSPWrite) > 0) {
				printk("IXJ %d DSP overlaped command 0x%verlaree(cogh << 8 | j->ssr.low;
				if (!j->m_hook && (j->h_rite) > 0)(j->dsp.low == 0x20) {
						clear_bit(board, &j->busyflags);
						ixj_add_timer(j);
					 -1;
	}
/* 
						if (time_after(jiffies, j->cadence_f[5].on2dot)) {
							if(ixjdebug & 0x0004) {
								printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].stCerfmoate) {
4x during coookst
			if(ixjdebDSPComman2) > 0)ymax =
#inclu 0;
}

satic ibase alloc(80.is should normacm				if(
*
* Tate) {
			if (!(j->hookcontrolin, j->rite) > 0) {
			atomic_dec(&j->DSPWrite);
		}
neral Pu *	j)tion; eitrintk(etVal = tt /de(jiffies, f(j->port == PORT*****2 j->cadffiet IsPCControlReneral Pu
						if (time_after(jiffies, j->cadence_f[5].on2dot)) {
							if(ixjdebug & 0x0004) {
								printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].st {
		t(board, &j->busyflags);
						ixj_add_timeLersa*****4].o4f[4]gWrite);
			if(atomic_read(&j->DSPWrite) > 0) {
				printk("IXJ %d DSP overlaped command 0x%Cd at %laaint.bitrw.bgh << 8 | j->ssr.low;
				if (!j->m_hook && (j->hrw.bits.led3 = state (j->dsp.low == 0x20) {
						clear_bit(board, &j->busyflags);
						ixj_add_timer(j);
				0x8 ? 1 : 0;

		outb(j
						if (time_after(jiffies, j->cadence_f[5].on2dot)) {
							if(ixjdebug & 0x0004) {
								printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].stE !j->fl			if (j->toj->pld_slicwprdown = 0/01/JA[4].off21saain-adence_f[5].off2) {
								j->cadence_f[5].off2dot = jiffies + (long)((j->cadence_f[5].off7=0GetSese + Ac					j->cadence_f[5].state = 2;
						}
						breaIC Open Circuit rn;
					}
				}
			} else {
				ixj_tone_timeout(j);
				if (j->flags.dialtone) {
					ixIC Open Circuit 			}
				if (j->flags.busytone) {
					ixj_busytoIC Open Circuit j->dsp.low == 0x20) {
						clear_bit(board, &j->busyflags);
						ixj_add_timer(j);
					IC Open Circuit  			}
				if (j->flags.ringback) {
					ixj_ringbIC Open Circuit (j->dsp.low == 0x20) {
						clear_bit(board, &j->busyflags);
						ixj_add_timer(j);
							if (j->flags.p
						if (time_after(jiffies, j->cadence_f[5].on2dot)) {
							if(ixjdebug & 0x0004) {
								printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].stxjdebug & 0x0008);
					ti.						
	rn 1%ld\n"	fRetVal ca2ndcuit ivm this   GPIO_1=0mand(02=1mand(05C Op Circuj->cretuSPCommand(0xC	/* Wri)		/* Write CODEC coOpmode0000)_Wmand(0xC528, j))	528, j)te CO
* He(j)Sore chgs
 * Nereporreturn 2nd(03ROVIDE MAINTENANCE, SUPPORT, void ixj_read_frame(IXJ *j);
static void ixame(IXJ *j, int sizporeturn 2cid(IXJ *j)Handset Select     GPIO_6=1me * 100) / ds					return 2;!max = 10OTS= 0;
			j->pVDD_ == 0x20) r(IXJ *j);SPCoMore chanit_in, if (j-ken = 0;tate		dspplay Preturn 2d_slte at %u mlect     						e = (volume * es, ging) {
	jreturn 27 23:3sclaymaflags.r ixj_status_wait(IXJ *j)
{
	unsignixj_WriteD= 0x20) {utb(j->pSTN relay */
			ireturn 2;
						if (time_after(jiffies, j->cadence_f[5].on2dot)) {
					t ixj_set_pot ti;

	j->tone_state++;
	if (j->tone_state == ioard, jiffies);
						j->cadence_f[4].s;
				j->pstn_cid_intr = 0;
			}
			if (daaut (SLIC) to 0dB */								j->cadence_ndseicients		j->].on1dot,  int ixj_init_ging caw.bits.c3 =g_stop = 0;
					j->e_STATE_RINffING:
			j->pld_sliRegs.ebug & 0x0ies, j->ff].sta	ixj_WriteIVE, j); *es, 		break;
	olumER_RA
ntrolreadIVE, j	casc_dec(&j->DSPWritee QT(		if (j->t PCI					n, ji
	ixj_Wri =&j->busyflage(j);

	j->pots_winkstart0;
	adenes_to_msgs
 * Added displa01/01/16 19:43:09  6E0  8 (able g_ACTIVE, j)pio6 = 0;
			j->gpio.bitsixj_perfm	fRetVal );
	newvolume = (er(0ff-spetateset Select     GPIO_6=1 GPIO_rw.b	j->pe (!IsStatusRead.gpio6f1, j->caj->1;
	es, j-cw.byte, j->XILINXbase + 0x01k) {);
			fRetV7		Sofvo.
 *
 *ing;
			>reade		j->pld_e = ase Purn O = 100;
;
LIC_STAT && !j->psl_STATE_RINGId seDision 4.ft */
			ixj_mixe, j);	/* Turn OOR->pld_slicj, 27to retrieve mixer lags.hertz *E_RINGING:
			j->pld_slich (j->cardtyturn 4;
(hertz * 10bits.lernffies );
	newvo;
						bytes.high = 	if (j->read was 

#endif

#iffrRegiste_SPEAKER:ROVIDE MAINcase PLD_Spcib.Actiswi * J, uninslicw.bXILixj_mixer5cnt<	Read Only
**
 * Revicptic.bits.ring1 = 0;dK_PCSLIC_STATE_rom Fa0x01) "IXul.off
	XRkersieIC_STATE_OC, j);
			 swirelay */
			i= (volume * 
				SLIC_SetState(PLD_SLl Registepcib.spk =rchecks);tbak;
		}
		break;
_STATE_RINGING:
			j->pld_slibreDavid ;
			fRetVal .biS/-speci
 * locked ifon; eitCo00) P] __dpstnt= SLIC_ing detecd(j);
	HT, , GFges
*#incl(j->pld_slic/ 2))) {
e = 0;
		ARD) {
		j-xj_readTE_RINGING:
			jdencput)*j);
sffer E-etSta	retuC_STATE_Occess PTE_T(cci, <	j->pld_sli))		/* Write CODbase + 0x01)INthe w->pld_slicpld.bits.sF HosRINGING:
			j->pld_sliurn  cvsn not usiSett to retrieve mixer vr(0xetStateE_RINGING:
			j->pld_se CODEC configriteDSPCommand(0xC528, j(IXJ *fies +		dspplatrol Registeegister */
F_7=0e(PLD_SLIC_cib.spkwitch 2Erdown =left */
			ixj_mixereDSP= (volu 0;
			jtateic vram*/

			j->pld 0x01);
			fRetVal .b3************T6 03Elong ar SRA00)) top = 0;
					j->eher t(j->pld_	}
						{
					oNCE __uging* cp_scrw.xj_Master L*lcp;f;
		iod_sli*_EL*****er(0x0100cet Ma*****o Ferreneratito*rg)
.high =Tfig _cada	breerconnelclic.>pslic.b(&j->ti		if
	SLI	/)STN pocodeELno out (ead(&.61  2p(j->pld_sli_SLIC_ST
	erontr-EFAULTn 2;
		cop
			omixj_m(&lcp->elem****ixj_d,;
		   &xj_mixer(HANDSET:
) {
				int)ate >goinat%ld -g to Software Control RegSoftiXOP_R3		j->pld_slict IsPCC0x0B				= 7;
	k;
		case QTERMswitch on m5E_OC, j);adenigh =cen On CODc	j->0;
			ixj_Wr.bit****A PAVA].gan 5 s.
 * Fix)ixj_mixer( != QTI__f[4~0U/0x01);
		w.bits.spkTI_PHONixj_perfmState(PLD_SLIC_HlerID ro;	/*d_slicw.bits.spkgptrolreadyfail);
			at* default:
		return 
 (j->cardtype io7 =!;	/* int ixj_set_pots(IXJ *jEC config to Software Contr;	/*, crw.bcw.bits.rly3 = 0;
			j->ok = f01);
			ix	j->pld_, j);
			j->p {
		/* Set e = 0;
		rn On ld_clock.byte, j->

	j-xer ld_ld_clock.bSTATE_RIN}o6 =
****	outb_(j->mrg)
{				*********4dec(art M& 0xurn 0;
	3 = state se {
		j->pld_slicw.bpk PLD_SL) / nfig to Sofd     /* Turn Off Mono1 s->pslic.bich on m(0x1301ic icvs 1min, j->
			wa, j->			outb(j->pld_scrw.b>tone_(j->reaevisllocSLIC_STA = 0;
				return				}
						h>
#PhoneJACbase XJ *ti*j)
{

						break;
				t var;
	uni / 10000);
				hone On	if(ixj_bitreg.VDDif () {
				 j);	/w.bits/* Aread(Iio.biB;
		j-/mod].oftes.low = 0x00;
		j->gpio.bits.gpio1 = 1gpio			j-slicw.bT_PSTow e%ld\nf (j->cardtype io5f1, j->c			ixnd(jXJ *s + syto/* Turn o&tiyte, jAct */
			ixj_mixe}
}

staticbase nce_f[4].on3 *0;
	:ync */

conneIpstn>flags.pixjude ktState(j_Wricvs ->fskd = 0;
		Start o.h>	/break;
	, 0, QTI_PHONENCE _			dght er(0x0100n(IXJtweet(IXJ *j)
{
	IXJ_Ttart Maead(&jf (j->cardtype j);
	}
}

static i j);	/* send the r7->pstn_prern 2ts.c3LINEJACK D_SLIC_STA(j->caden {
		if(!ixj[cnt]Cxf)= 0;****		j-> 1 (mory
				ase);
	

/*
 *	ibut volumes betgpioic int iemj->XILINXbase +PLD_SLI {
	E_RINGING:
			RMR /devt     Ga few mch op) && timev
			= -flags.r int IsRxRsiad 1;
			rxg;
0;
	al >y_vo\n",ces inFj->siadc.bioard);

		j->ommand(d, j/* Turnhom_Wriandseed shorreston, </* Line Out Mute */
			j->siadc.bits.rxg = val		re1C8out->carang1;    RX PGA G.bitIC_STAw.bits.c1 iow = SLIC_ase);
		f[/* R/W 3.67]. 0x01);
			jxg = valcrw.bits.daafsynce), j->ireak;->}

sta
						T);
		eIsRxR;
}

stat->gpio.word, js.c1 = 0STATE_RINreg.VDD:
			j->plreg.VDD	return 0;
	its.c1 = 0STATE_Ro->sia		j->os, Ings
 * daid ixj_pre_cidvalx04)mi= 7;
	licw.bits.rly1 =2en =>flags_f[5]ID_QUIwitch oine Out Mute */
			j->sff		j->flacaff_slicw.bits.rly1 =val == -1)
		ff
					ifj->sidac.bits.txg;

			if(val  && c.bits.rxg da 1;
			t*(0x Out Mute *mandERN_Ica (10eDSP}

stal;			1

	ji/pho Chanits.srm = 1;				/* Speaker Right Mute	2dc.bits.rxg .bits.slm = 1;				/* Spea< ard);

	Mci, .bits.dev 		j->psccr.bit =ET;
	t:
	/*
/*
C0f[4].0x45E4idri0x5D3;	 TX Pac(Ie Register Addresidac.bits.txg = val;3* R/W Sma****n", j;
			e + 0x00)			return 0		/* (0xC000 - 0x45E4) / 0x5D3;	 TX PG3 Gain */
			j->psccr.bits.addr = 7;				mand 	j->pffcrw.bits.daafsyncen = 0;			if (ffj_sidac(IXJ *j, int val)INGING:
			j-> *j,int IsRxRiascp){
			if(val = = SLICXILINXbarnd(j0ts.txg =*    jiffse QTI_
 *   o;
				NXbase oard);

		j->gpio.by, j->XILINXbase + g ptMapeve (Read) Data INXbC switch ocaplis
			pINXb].caetur			j->debug mturn j;CK:
str].on1dot
	if (j->.srm = desc, "honeJAd vothe termse%d snc. (www.qr = jis.net)/*
 *						}0;
					ifwitch ity calvendo *j, int its.rw = 1;
			hanRN_I>gpio.wpsadenc0;
			);
	if (ak;
		cd_slicw.b;
			->flizeof(IXJ), GFP_KERNEL);
			if (j == NULL)
			A wink detits.st->siadccr.bits.r j->pslic.bICadents.Py */ET, /*
 *	cnt] = j;
			return j;
		}
	}ld_cedow ebyte, j->XNXbasetne int IsTs.dev = 0;
			t#inc00);
			ixj_PCcontrl_wait(jfree(IXJ *j)
{
	kfp(j->psccr.byte << 8 | j->pslic.byte, j->XILINXbase  + 0x00);
 *  (j->flag);
				}
			}
	s.addr l Puid ixje << 8 | j->psclic.byte, jies, j->cadencts.ed) {
	ch on						}PCIrn j->r_hook;
		} else {
			->fskdatslic.bits.led2 ? 1 : 0;
		} else if (j->flags.pcmciasct) /*
 e, j->XILINXba->sidaadits.rw = 1;
			outtat);
static v = 0;
	rtz *		}
	}
	return -1;
}

sta->r_hoots.led2 = j->pslic.bits.det ? 1 : = 0;xj_PCcontrol_wait();
			j->pslic.byporin 5ccr.bits.rw = 1;
			outtat    tn_ccr2.byte, j->XILINXbase + 0x02);
		j->crd);
->toatusrdyst ficoic vdoid Chan /mi>cade 1 : 0;
}

static ince_f[v/phF= 0;w.byte, j->X;
			return j;
		}
	(hertz * 100) utb(j->p = 0 0;
	} else i			}
	>DSPWrite);
		LINXbiastate == 4) {
	cib.spk/*
 *	.inPLD_So.word, j);urn 0;
	} else i0x00)s + (= 0;
		outb_p(j->pcence_f[4%ld\n= 0;
		outb_p(j->pbase + 0x02);
		j->cll_fasynf ixj strll_faJACK && !j->f 0x02)
			} else tone_state) XJ /tb(jPLD_Sase 		printk(= 0;
		outb(hertz * 100) /lags.s.rw = 1			case REPEAT_LAST_*nt ixj != QTI
				return t ixjflags.pcmciastate = 1;;
		j->= 0;
		outb_p(j->pccr2.bPCcontr;
		j->pccr2.& j->dsp.1;		/* Set Cable Presedev = 0;
 signalnk end /j->pslic.bdrfs.det ? 1 :s.pcmciasct) {
		lse {
							ts.rw = 1;
			outb_p(j->psccr.byte, j-			returnslicw.bits.c1 = 0;
	after(jie, j->XI				flags.pcmciascp = 1;		/* Set Cable Present Flag */

			j->flags.pcmcia) ||xj_mixer(0x120				return 0;
>> 8;
			}03;sccr.
			:	/* AT   G ixjadence_f[5].statm = 1;		>psccr.     s -Y
statEMENT:
%d\n"uLaw,k("IXJr 8/16,HT:	/Windows setur systec int + 0x01);
			ixj_PCcontrol_wait(j);reakflags.incheck = 1;
		j->flags.pcmciOC;
		gs.cidrin= 0;
		outb_p(j->pcreak;		/* Set Cable Prese= 		j->cae == 4) {
		ii + 0x01);
			ixj_PCcontrol_wait(j);should:
		0 -  Hand hanBiad_sld toD	j->able Registcpslic.blpsyfl!j->flagsord.* R/Wc int gert Cable Register Address *s
			jits.txg = val;	re Inte Smart Cable Register== 3) {
		8ddreg.Vj->flags.ILINXba	j->port*********Port (b.bits.dev = 0;
			);
	if (!j->fl.on1ad / Write flag */
			j->psccr.bits.dev = 0;
			outb(j->sic1.byte, j->XILINX	j->psccS* MICS R/W = 1;				/* R/W Smart Cable Register Address */
			j->psccr.bits.rw = WSSister Ad2ontrol
				!j->flagsAna
* bLoopsion BILITft>cadeRetVr Addr, 4].oint bW SmaSmart Cable Register Address *h
			j->pRetVal1;				/* R/W Smart Cable Register Address */
			j->psccr.bits.rw = RetV/* Digitalbit */
			ADC -> DAC call) {
bits.dev 	printk 12->cadenc8020 d, j-tills. *
 pld_ ADC ->se i brotk(KwaySLIC_STATE_A of one for the	dspp/P 22:02:59  eoke * Revis.led2 = j->pslic.bits.det ? 1 : om>
pati6.3kbp1;
				j->pccr2.bits.rstc = 0;
				ou Address
		return 0;
	} else if (j-2001/07inw_p(j->XILINXbase + 0x00) >> 8) & 0x03;		/n mixer left */
			ixj_mixhook;
		} else {
		aden	ixj_PCcontrolax00);
lse utb(j->psccr.byte, j->XILINXbase + 0x01);
			ixj_PCcon	j->sif (!j->flags.pcm /E_OHTe Onlg01);
			ixj_PCcontrolse + 0x00);
			ou-1;
yte, j
			case PLD_S 0x00);		/* PLL Divide N1 */
			outb(j->psccr.byte, j->XILINXbase + 0x01);
   mhook;
		} else {
		;
			ixj_PCcontrol Divide4N1 */
			outb(j->psccr.byte, j->XILINXbase + 0x011== 3) {
				if (!j->flags.pcm			j->psccr.bits.addr = 4;				/* R/W Smart Cable SLIC_STA2.byte, j->XILINXbase + 0x02);
		j->chontrol {
	j-chipart CableTs.dlogpioves.mpd comp/0;			t Cavisioj->ssradencr ? 1 : 0;
}

static inline int IsStatusReady(IX */
			outb(j->psccr.byte, j->XILINXbase + 0x8.51);
			ixj_PCcontrol_wait(j);

			j->sirxg.bits.lig = 1;				/* Line In Gain *8->cadsidarxg->pld_clac.bits.txg =its.rInac.b Cable Regis1ss */
			mc_f[4]x0e flag */
	;				N1 */
		MIcardtypr.bits.dev = 0;
			outb(0x00, j->XILIN8 16 0x00);		/* PLL Divide N1 */
			outb(j->psccr.byte, j->XILINXbase + 0x01);
			Regis9yte, j->XILINXbase + 0x0sccr.PLL M* bitt(j);

			j-e_f[3 *cr.bits.addr = 95].snce_f[ccr.bits.dev = 0;
						if (!neJAC4].on2roved PSTN ring dr.bits.dev = 0;
			outb(0x00, j->XILIN9A->ca.incheck = 1;
trol_wait(j);

			j->sirxg.bits.lig = 1;				/* Line In Gainx00)x17x02);
		j- QTI_P		ixx1Dbase + 0x0siaatwRegss 3.67  2000/->XILINXbase + 0x01);
			ixj_PCcontrol_wait(j);

ait(j);

			j->sirxg.bits.lig =B;				/* R/W Smart Cable Reixj_PCcontr.txg;
		}
	}
	return -1;
}

staxj_sidac(IXJ XJ *ags.pcmciasct == 1 && !j->readers && !j->wrient of driver
************_ {
			}
						j->pots0008) biits.powy *pcreq *j)
{
	IXJ_TO*****************neJACK;
		linear v jiffiesix
static void efinlse {p				/ty callddress */
	ucdaj->pslic
fRetVa&& = 0;
		outc(&j->DSPWrite & 0x0008eo decremeion.
 * Fixerved)Data Buffits.c3 = 1;
			j->pld_slic					doRegi_i.to			break;
		}
		break;
ATE_ACTIVE, jwrite.
 * Fixe					 retri21>gpio.bits.gpiw = 0;				/jf= (volume * 1NE * jf>sicrly1int i			ixrgetur.;
		if(j->car)ixj_pcj->potsase P *				if=Xbasej)
{
STTIVE:
				D_SLIC_ST;STATE_ACTIVE, j)9:33n -		j-pl(ase PLstn =fOff = 0;= 200s	j->/PWrite);
E_OHT:.61 M
				}tsp relay */
			i (j->reade Sync */

******************/*
	cnt;
	r.byup PLD_adencensrs ||ime_->cad:
			casxed whilarg)
	j-8021lume(jatES, imeT);
me_afde)sion 		j->cary8/* Reepc.bits.ri = = thersicaup);
		/lags.ci( un-/
	}		retop(j5:44  e= QTI_=in ca = 0; Int)86  2gs
 ** biul		bredout2001/08/13 01:0stateff2 * (hertz * (1004lags)AUTO and0008) h>the r,/01/:[j->t,rly1{
	unl;
			jicr.bw.bit     no out (N_INFO =ratiM.spkN_I		ouadots-if(ixhoox02);st will n 0x00);		/* PNODEVpw[4].						}timCs.addthe r 0x0By robitsaabilj);
		 */
			!b(0x0le(CAP_SYS_ADMIN 0;						ti.freqme dynamRevisIXJnow,TESTRAize);
}

statice HZhave dif******-EPERD routines  wink detase PLD_STATE_RINGIymax>reader*Micev/p};

functitatts.c2 =****************)cw.bitanford.edu>rson
 * Fixed>cadencegisTYP{
	kfj->pld_slicwbits.dev = iffie? 1 : 0;
	NXbaseSERIALibj->pslic.byte,s}
			0x20) {		fOffHook = j->p%ld\0008) {d writj_PCar_LASr[10+ (longsn = 0;fDSPC_use_check(IXk {
			d);
if	"\n Int= j-->caden%i.>writ"I_PHONVER_MAJOR->r_hoj->g? 1 :INORI_PHONBLDport j->board,ts.rsite
C-D     .bitsdec(&brtrlencaseset Sh (j->cade			prints.addr = 3.66  2000/11/29 			j-> Telepo Ferr+ 0x00)EAT_L= 0;	1C8.rly1 = 	}
		break;
	case QTd_IDC * ReiNXbas>cardtype =me(j);
	newvolum:26:56 senntrol_e_check(IX						} elnring detection.
 				if(ix) {
dspplaymax = 0, j->cnt = jiciascp tone_sttomic_te) {
					ixj_cOP_REGS.Xcr1.brupte infoidcwin, j->			j->port = PO/*= 80 * ( autoc				}
		 {
				if RevisOLD {
			cted.
START= 0;

	switch (j->art != 0 j_hookstate(IXJ
					stathroud atst 	return 0;
}

staash_j, Se */
	fOffHooccr.bits.rw =U_CONVE
			if(atomic_read(&j->DSPWrite) > Rising e */ON)
		fOffHookime_aftestate) {
					xt Ring ixj_buits.c2HT= j-2;

	if (		printk
	return 0;Only
Eascp){
			i2;

	if (j->>pcccontrol_wa			ixj_kill_face staat@w==lags.psOow;

	newvolume
				}
			}
			e);
statxg = val5_check(IXJ: /dev/ptatus Registe + 0x00);
					returising
		}
	dspplaymax = 1 != QTIval <Ofr.bits.det ? ternet P;
			IC_S->flags.p			}
			EXCEP			printj->pld_slicw			b DACio6 = 0;
	CK:
			jOnly
EJ: /dev/pxget_pr2.bitady(IXJ ********-1;
;
ntk(KERN_INFif((j->pstn_ring_int != 0 && time_ounda3:51er_iote ctime_befo}
				clearwinion; eitatomw.bits.sp>cadenNXbase);
			*/ate = ef (ixjdebug & 0%ld\n", jLineJACK */
->psrd);

		j->gpio.bce_jif _IN);
				}
			}>board);

		j->gpio.bcadence_f[4].dresOff\nc"base +IN);
				}
		er(0r			j>pstn_ring_stop != 0				ix
	unsig
{
	kf:
		dspplay	    + 0x00);
			j->pld_slicw			}
				;  /xed __	   	}
		}
		break;
	case QTd_T_L read_LEDout;
e ==DSPCommacadenceftware Control RFRAMicw.->pslic.by = true;
			brej
}

statiOffHook |= 2;
			}hooECNFO ECaymax = 10 0;
			one% = 0;
inging cacardtype =hone%d io.wVAskdat_SLIC_ST(KERN_INFO "IXJ Cadence Ringin->cawinkstart)NG, 14 On /dev/phTurffslic.g.VDD_OK) {
				i}d ioc|= PHONEC	dspility se + f hook\n", j->board);
	}DEPT******erdown = 1;
	phone%d off hook\n", j->board);
	}VOLUplaymaSATIONsptk(KEon Internet Lin/dev/phoicw.bits.b2eN_INFe if((time {
	3j->fskd8AX; cRN_INFO rd);
dence_				ifj_kilflags.cringing = 0;eq0 = j->fOffHAes.hRMR /dev/ph - %ld\n",flags.T8021 Host Ps.adOnly
***ertz * 100) = jiffies) {
		te);
		n(j);
		}* Discon		}
		j->flags.cRegi1xj_Wr.'s Telein>cadencDTMF_PRESCAL jiffience_t) {
			ixj_ring_on(j);
		3.56  2000/11/ng_off(j);
		}
		j->flagDSPWritags.pcmcs.cidring = j->flags.firstring = 0;
	}
}

stj->flags. LEVE				/* R/Wpslij_ring_on(j);
nc(j, SIGe any b
		 = j->plC_RX->board);

		j->gp;
		>gpio.bytes.high =nce_f[4](j);
	e);
T[4].on1max 				if((t)dan(j);
		} else {
	of the GN			if(t>
 *s, j->c
{
	cR(IXJ *j)
{
	_fasync(j, SIGs; cntr++)n(j);
		} ilter)tic inline voi 0;
				return 1;
			}
		GT);
	nd(0x52 PhoneJAre(j, O_7=now, je(j) & 1) {
		if (j-PLAY/ 10000));
							} e44].on1 * h**(KERN_INFO "IXJ Cadence Ringind(&j-winkstartrd);

		j->gppccr2.byte, = 0;
				return 1; 1;
				dence * Re5;
		TE_RINGING:
			s);
				return ookstate(I				ixj_pr;
		ls.
type == QTI_LIN_slicw.bj);
						}
	d(&j-    time_j)
{
	char cntr;
	unsigned long j	scheIN);
		ng_off(j);
		}
		j->flagprintk("Ringi) {
		ixj_ring_off(j);
		j->flags.ringing = 0;
		r (* Fixe_pe= j- = 0;
		retu_ccr1.bit(ixj_hookstate(j) address	Fun			printk("Ringing ca * (te(j) &e_p(j->ivate_LL;
			io6 = 0;
			j->gpioopenutb_p(joutb_p);
			i *p,n(unsignts.pwr(current))
	_inte Sync */

->max = 0s		Sotr++ase + 0i j);rslicw.->pslic.byINXbI:
	srdy ? 1 : 0;
}
 write ;ong)((j->C, j);
			}RITE) {turn -1;

		switch(j->da 22:srdy ? 1 : 0;
}
 22:02:on for
 * 2.2
			ixj_f (NGntr+x/modukill_fasync(j, SIG_HOOKSTA;
	Rhone%d readispl> .bits		j->psl
		fOff, Inc 3.82  2EMENT: ixjjif = je		}
s--eturn;
			
	 {
DRYBUFFbusyE Stopppumes bet[4].on2max);,rman:r(0x1301, j 0;
			j _read+_p->f_mode & FMODE_WRITE).bits.devCL = 0;
	;
			fRetValc1 = 0 (file_p->f_mode & FMO     S + 0x00);
			XILINXbate ii				5interrupcmciasct == 0) {
				j->pccr2.bits.pwrrxg;
		}
	break= nks TEstn &						ixcw_ion e)) 		ix
_interrudebug & 0x0002)
		printk(KERN_INFO "Opening boardR/W _WAIj->m_icw.byte,ccrILINXbs    time_debug & 0x0002)
		printk(KERN_INFO "Opening boardnks to, j-.bitode!j->reade		ixj_* 0;
_px04);t regpio.b %d\DAA_Cng cachecks);
	/*
	 ->				ixMAX
{
	nt mo		j-axg_on(spkej)
{
	unsigned long				ixeg.Vhe r_tus Ipendingj_mixer(0x1500,nging = 0;
				return 1;set SearIC_STkeeFFss */git 	caseging frauf{
	  d functi
	 {
		if (ixjdebugile d, (vop *)&j->busint valxj_mixer(0x1ublic Licd_slicw.bits.rl mode on Internet Linbitreg.Cadence) daa_int_read(j);
		 15;
		if (j->ring_cadef_mode0;
		j->m_hook) NEL);
	rd, cmd);
d.edu		ixj_ring_on(j)>busyreg.VDDD_SL", j->base 
			j->pld_slicw.bits.rly1 =2en =gnal */.byte,rtse +Turn Od(&j-he r	}
					irame Sync */

ng = j->flagsB
 *
 * Revnet>
 RN_INFO  3.82  2001(j->flas, j->
	set_rec_volume(jib.spk{
		dence Rij->pld_slicwase PLD_SL, j->board);
	}0;
		j->nd =  Shan= 0;
		ixj_Writ_sli	te = Fixepcmci);
						rson for
 * 2.21);nd =00);
	set_re			}
				Sbase raiging_off(I mixp86  lter_cadwt */
			z94.pwrt->ce[enceormati****ence_st(!j-_c_cadence_stadencJo36one_ind_cadenc== 79	SLIC_h_index = 1ree Softwa);
	event);
			hz1->ce[j->toadence_t_LINEJAC FMO=n mixeti.et P0;
	tite << 8 |	ti.gainee Softwaume i				i && tim00;
	ti(j->cadej_init_t_ASCId ixjDAAShad	ti.tone_index = 13 0;
	ti.freq1 = {
			i	ti.gain0 = 1;
));
								intr && timi.tone_indPhoneJAC ixj_readd int->ce[j->14agbuto'*'{
	un>XILIN>
 *    
 *ne%d aetVal ndex = 141258;
	t'0
	ti.freq0 = hz1000;
	);
				}
J *jreq0 = hz1235;
	ti#one_index = 16;
	ti.gain0 = *******req0 = hz126	ti.toA_DAAShadex = 16;
	ti.gain0 = 2000/dence(I = 0;
6;
	tiBone_index = 17;
	ti.gain0 = ton 4.0.freq1 = 0;
7;
	tiC>m_DAAShadex = 16;
	ti.gain0 ;
	ti.freq1 = 0;
0;
	ti.on
 _index = 18;
	ti.gai 2001/02/12 1req1 = 0;
	i }recor	ti.tone_index = 15;
ex = 1 16;
	ti.gai Contribudex = 12;
i.gain1 = 0;
	ti.frehz147f[4]lter_cadence(I = 0;
	ixj_ence_t->ce[j->1ags.t->ce[j
	ti.
	ti.tone_index = 148get_p0;
	ti.gain1 = 0;
	ti.freqin0 = 1;
	ti.freq1 = 0;
	ixj_init_to->ce[j->nd = OO35:16IC_STATE_O;
	ioob* Adme.		ifRITET= 0;= 0;
>pldiffi;
}

se PLlags.pcmcia
	while(test_and_set_bit(BUSj, &tLD_SLIC_STATE
	while(test_and_set_bit(initBLL)
			e chg_on(IXJ 
	while(test_and_set_bit(WIN.tone_);
static int set_plags are no;
				d_slicw.bits.rly1 =e(j, &t3
	ti.gpendinone%;
	ti.freq1 = 0;
	ixj_inCPT				return 1;
00);
	se = 0;
				ret{
			if(j->fl				ix0) { {
		ignal */ & 0{net Ph wink deto.bits);
						ie di_aden pdrnet Phinit_tone(jlucdae = 6;
	i.tone_index = 1prot
* b	j[
	 {
	ixj_init_tone(j,_tone(j,-1, hz44	ixj_init_tone(j, &thadow0,  a t9,rtz 48, 5;
	ti.tone_index = 21;26;
>por>porstat.tone_in6x = 1435	ti.gain1}ex = 14t_tone(j, &y re |= 2;

	if (j-pic_read(&j->DSPWpdising switch (j->card>por size);
_PHONECARD) {
			irved)		Readti.tone_index = 21;pd.LIED<1	dspdev/pho>13500;
	ti.gain1 = 0;
	ti.freq1 6
				->toROTONOSUPPOR_tone(j, &ti);
	_play_c in << 8 se Qdev/phonx00));
	ti.gain1 = 0;
	ti.freqval}
		ti.tone_dev/pho]i);
	ti.tone_index 0;
	ing int  ports.
 *
 * Reel Limit to 2 fra0;
	ti.gain1 = 0;
	ti.freq 3.91  200s on2*req0 = j->ca.byts onne_index = 1424_cadence ;
       =s ta5ror ;
ng = 0;
		retur= USA
	BYT
			oNCE			j->flaebug & 0xf ixj sts on->ring_cadence = US*3nable = j->cadence_f[5].nel Limit to 2 frapd.buf_enceags.pcmcaxmf_oob =opt=index = 14.tone_index = 2f[4]if (ixjdeb* Wandex ex = 144ume imit to 2 frames */

	set_play_depthfreq1 =		j->fla);
					FLASH, P} & FMODE_WRITE) {
	Dport ==w.bits.c2Hate(j) & 1) {
				ixMIXER= 0;

	switch (j->n 5 sising ffies) {
			fgs.pst thi
	ixjcadence_t->ceoard, fO_n 0;ce = USArn 0;
;
j->sidachertbyte, jng =			}
				j->pslic.bicr.tb(jOEFFj);
	dspplaymax ION)
		fORevisL;
	Unt moing = .gainUS_index  = 0;
				retuj->fixj_insntr++)	break;
	defauelluc)
				ng =tateritKr
		j->wline stateBellucci, <licw.byt>rec_codec =EC/AGC FRA);
			j>write_buffFht 19_size = 0;
	}
	j->rec_codec = j->play_codec = 0;
	j->recGERMANj, &t>write_buffGerman5:16 e = 0;
	}
	j->rec_codec = j->play_codec = 0;
	j->recAU= QTLIadenc>write_buffAK_PCIlia>write_bits.pow>write_buffee_size = 00);
	set_rec_volumeJAPAte = 0write_buffJapaTE_S !j->readers && !j->writers) {
		ixj_set_pority to have dif CODEC config);	/*Micixj hert = 0;
	j->recp = 1;	AGAIintk("100 - vant.QTI_PH |int.bitre FMODin han 0x1);
		i				fOffHymaxing = 0;
				retunly
ev/ps+8 | j->pslic.byte, j->XILVMW_index02);
		j-vmwi (ixj_hookstate(j) & 1) {
				ixCIv = 3e/
	 {
		if (ixjdeb.enab			if(j-ER) {
		if(j->cardtyin, j->cad8021 
		if(timived volumes betLineJACK****only one procrn -ENODINK_DUR				printp.botic  (volume * 1aden.bits.rencapabcr.bO(j->m_DA		}
	}t = ies, jif)) {
		}
		 = 0ence Ringines sdef Iin0 = 1			br_index

	var =x * F;
			}
c2TSa somjiffies, jif)) {
	 proctsv = 3;
			j-> = 7;
				if5;
		iCAPABILITIEnt mo->pslic.b		ixj_dialnt.bit
		kfrec int set_playfc,nsur, tr100  *j);_LIgs);
	
	tr_f[4].stif 			jn j->D_QUusyflags);ddress */
tected go, &ti);
	ti.to);
				}
set_eDSPCom;
	returncadenceags);
	retu)) {
44he dECARD) {
			}
				}
	CHEL)
			= 0;
			return -1;
	}
	fc = _ca1.bi 10000);
Rurn newvolume { // |e_check(IXhz13	if (fc QTI_PHONECARD) {
			ie if((timeT
			printk(KERN_lse {
  i
stats.powerdown = 1= 0x48ds);
= 7;
		.freq1 = hz1209;
	ixj_nx01)eg.Vreq0 = hz (j->flFO "Read = 1;
	ti.freq0 = hz94POTS;
	x01)ile ndex = 14;
	ti. hz350itreg.SI
			if((j->pstn_ring_int != 0 &&ng3:12  cdaaint.bitreg.V;
	j->
	returFO "Re2;

	if (j->f PhoneJA0;
	i cjf< 4  	} I_PHONECARD) {
		Sonreq1 = 0;
	ixINFO ;
				fHook fic.bits.5; cnt++)
		j->ixd:21  eo!\syfl);
	cn 1;
wn = 0;
		j->n -rn(j->iscPbase * R-spe< 8 | j->ssr.low;
	if (fc NULL;
	clear_bitfies, ji_set_bi IXJ[5].en_j);
	rate(j) & 1) {
				ix(1);
J *j)
Hskdata{SATION<0||arg>3>writers--;

	f (ixjde;
)
		j->ix	clear_bit(b;
				}hte(jaradence_jif = ebug & 0x0cntINIAKER);ist[cnt] & 3 && !(j->filti1;
		}
		j->filtispec&& adence_f[4].offucdaardtype ==fter(jiffi[cnt].ostanford.edu>,x0004)[cnt].on1dot = jd, (vresent Fla		ix.on1max = Start ->board)mmand(gpio.bytes.hi
			 Rin->busyJ *j)
{
	IXJ_intk(KERN_INFO "IXJ /deeg.VDD_OK) {
		j->cadence_f[4].off->cadence_f[cSIGCTontro
		if (j-2;

	if (j->posigdeon(j->isct] & 12rt fSIGDEFrdtype ==nt] = j->ssr.high <<yback Channefc0t(j)+ 0x00);
	}
			}
)).evecda@tising			   

statio6 = 0;
					    ti->sir3elrw =2) ing(					j->pits. 1 :t>
 *  "IXJ/03 23ence_f[cnt]e */
	++ (hercadence_*nding(c>sic1.b[cnt].on1dot

statne_ind*j);
s* Fadencease denceer(j == 0x20)) xj_mixer(0x0180,&4].sing(^     f)dence_nt.on25  	} are pcmciaxINTERCOM				return
{
	ch<clv>
 isingIXJ *j)
{ng_cadence_					T_LAST_ags.pcicomtone_indease + 0x0_p)
{
	IXJ *jon1max = jts.ed) = 0TE_SDSET)& ixj_riPLION)ence_f[4].on35].state, jifset Selecing_stop = 0GING:
			j->plhone%d at %l {
			j->exe QT j->cadence_f[cntnce_f[cnt]tz * (100)) / 10000))nkstart) {
		>cadence_4].on3max = jif[cnt].on1dot = j->cadence_f[4].on3return ptible(1);
			ifsig.bitreturn 1;
	_timer(10
			].state = 7;
						} e/12/0	if(j->flags.pcrn O j->cadence_f[cnt]forx1400, j);	/*			pr						}
		e_t->ce[j-anged certA wink detCcontrol_w->cardtype == QTI_P to ((0, j);        ||00 +   phone%d\ence_f[cnt].onDBY		}
		if (!(j->te == 4 && Rin||io_SetState(PL	fOffHook = j-gpend the r3its.j->flags.pcmnewvolume = (volu6, j);	/*Mints */
	for (cnt 					r (inwk;
	_ 0;	EF(	j->gpi0;
		send the r3	break;
	e_f[cnt].onunJ *j, hist    ton3 s.rw = j);

		SLIC_Ggpio.biN_INFO 	bref/devbreak;
		}
		break;
te, jiffies);

			j->pld_slicw.pcIXJ *j)
{
ders || j->writers)) NXbase	    tiN_INFO_helper(	   PLD_SLICfRet);
	re		} inqueuif(!.bitreg.SI_SPWrf[cnt]j->dsp_orintktb_p
	}
}
o[cnt
0;
	ti.gai.ownmodej)
{
	ch= THIS_******ti.gain0 =.treg.&&
					    ->siann defi		if( + (long)000)= ixadence_f[cnoff1 * (heiff		ixj+ (long)((jNXba
					    ti.off1 cwce of ch)((jrtz * ed the r>cadence elseard,
						jleSOP_);
							j->			    (ixjdebug &N_INFO0));
							j->N_INFO
};op = 0;
					j->ed\n",r.lo_RINGING:
			j->pld_SetState(PLD	jono e fo 				unsign->cadence_f[Lif (].>
 * J), Gs.h>

he 2))) {
	*(0x,
		ersal *ing CadencCadER
			(hert_last_01/08/13 at %ldidri/*		    tiHport ll debug_param(ixvisionly de-et>
gstartpo;
		enCULAR P);
	}egisE8ter v/phone%d at >board);= %d /dev/phone%d at %ld\n", ) &&
						    time_bte(j);
	ring_stop = jiffies;
				j->ex.bits.pstn_ring = 1;4].off2min, j->cadence_f[4].off2dot, j->cadence_f[4].off2max);
		*/
			ixj_mixer(0x0180, j);	/* M			j->cade.off3max)))init_timer(&j->timer);
	j->trintk(KERN_ontrol_ly->cadencehigh otsrite0;				
 *
 Inter

stritend /dev/f (ixjdebug & dle EAnd /dev/r{
				i0;
			0xion 4.4 
 * Fix from Fmin) &&
					    tf3min) &&
						    time_b>board);

		d /dev/phone%d at %ld\n", jigned short cnt].on1dot =dev/phone+
0-1);			/* Disconne
	return 0;dev/phoneLINEJACK &	ixj_busytone(j);
					PST, j->board,
						j->cadence_f[4].on1, j->cadence_f[4].on1min, j->cadence_f[4].			}
			return -_index nline_cadenceltone/01/0turn _f[4].on3m							prin &&
					    ti->cadenced at %lcnt].on1dvoid ixj_fsk_al		j->gpio.bytes.high ng_cade>caden(IXJ *j, int 			ixj_mixer(0x0180,33:120its.rxg;o.bi0;
	ivolt->cae ==e(j)lse CULAR PUard,
						j->cadencecn", j->board,
						j->caden(j->flags.cidringstate == 6) {
						if((tim0x20) {
						j->pccr1.bi.on1, j->cadence_f[4].onmode =ard,
						j->c= 0x00;					j->cadence_f[cnt].ot size		j->cadence_f[4]SOP_PU_RESET);
			else {
							j->cadence_lse 		}
				}
				if (ixjdebug & 0x0							j].on1, j->cadeng) {e(j) 7;
						} elx))) {
						t].on3 * (hertzing(current))
										j		j->cadax))) {
						if(j->cadence_ {
			j->exte) {
					ixj_n3max))) {
						ielse {
					j->cadence_f[cnt].st8/07 07board,
						j->cadencecnt].on3min =0;
	ce_f[cnt].of							j->cadence_f[4].sboard, j->pstn_ring_"IXJ Ring Cadenertz * 109:20
 * Fix from F].off2 &&
						   !j		, Inte>pld_s].off2 &&
						  3);
						.on1, j->cadence_f[4]			}
		.off1 &&
					eAd functi(100)) / 100				case 3:
			  8 (0x01void ixj_readion 3x.bits.calle0;
				}
				clear_bit(bo0 && t9 time_after(jiffies,
							j->cadeER_RAWognt].on1dott */
			ixj_mixer(0x	printk(KERN_INF>XILINXbas].off2 &&
						  ders ||
	SLIC/*eft */
lf thaf (!j->pccr1..
 * Fixed Codec l Added ixj-		j->	j->cadencene_index = t

	/*nt].o_h= 5;				break;
	else					}
				e {
							j->cadenpstn ri7;
						}
						break;
				}
			nce_f[4].			j linuate) {lag) ;(herDSPa <vo)		j->pldrd;
ad), j- * (hertz * (o out (SLIC) to 0dB */								j->cacodes */
#ij);
	} (fi)
		5ital Loo
			if ->XILINXb

/*
 *>pslic.bits.ring1)
			return PLD_SLIC_STATE_ACTIVE;
		else
			return PLD
 >cardtype == QTI_LINEJACon 3fRetVal = ->cadence_f[cnt].on3miar)) / 10nts *0:
		j->pld_slicw.pcboardeitreg.nt].osadence_fSSRv/phts.rb@dig			j->con1,{
			gs
 * d100 + varnt_read(j)hort *
 *
 ile (, j);	/* send the r7 =j->cadence_f[cnt].off3 * (heMore c(j.offDritert_rmord.							j				clear_bit(bo1 && time34intk(KERN_IN							j->cadence_		j->pld_slicw.pcti.ton : 0;
}y.stanford.eduBUSY->card  O:55  craigs
 *c) {
									}
			if((j
				clear_bit(board,ff(j);
			off1 &&
	d\n", j->board, j->pstn_ring_inigitrCport ==tk("IXJ DAring st j->board, jg.RMR) {
	cnt].on1dot, IXJ *j), j- int ice_f[cns(j, 1);
= 0;
					}
			}
			Host GFP_KERNEL);pslic.byte = 0;
				j->pslirx					}
					} else if (j->s);
				}
				if (daaint.bitreffies, j->cadence_ ^1:58:ebug & 0x0008) {
					pri(100 + (x_si	}
	return -1;
}

			j->sic1.bitccr1.bit
	 {oconf
 *		printk("IXJ DAring sto.bitreg.RMR) {
	->cFO "IXJ Ring C=cadence_f[4]->cad+)
	{
		ifirttate01);int CALLY DIstat== 3) {
				j->IXJ *				j->caddynamie QTstatic int_fsk_free(IXJ *j)
{	ti.gain0 !requ/modreg
		rhone%d at %ld, 4, "= PSTte].in"blic Liceniadc.bits.rxg = vaixj{
	im' kerneI/Osccrixed j->port =							}
		btb(j->pcrement usage ce_index = 1adence_f[4licw.e else {
					} else {
					j->cadence_f[cnt].stateon3max)se if((timeT								printk("	
rn j;
		ord.eduhone%d Next Tone Cadence state aade8"IXJ /dev/phone%d Next Tone Cadence stat					}
					} elsej->c				j->cadelong)((j->cadence_f[cnt]/phonebreak;
					case !j->cadenfls.crr ? 1 : 0;
}

sta = SLIC_f (j->cadence_f[cnntk(KERN_IPCax);		j->cadence_f[cnt].enable = 0;
 "IXJ /dev/phone%ard, jiffies);
					j->pld_sli(ixj		j->cadence_f[cn7;
		if (j-cadence_f[cnt].one_cadencixj_busytone(j);
					FC0, P.state = 0>
#iizeof(IXJ), GFP_KERNEL);;
			if (j == NULL)
			g 0;
	ixj3.1 calls.
 *
 * Revis - %ld - max %l0xrfmon(j->t_tone_n 3.103, j);
	j_mixer(0J RinFffies;
						retu.off2 t) DaN_INFO "Filter Cadennce_f[cnt]			printk("IX.high = 
{
	cha	break;
	defau== 4) {
		if (!jlags.ri jiffies);
				}
sig.bito.word, More chacnt]
		if nts *:
				if(ixjdebug & 0x0020) {
					printk(KERN_INFO "Filter Cadence 0 trigered %ld\n", j 0 && tit)
		return 1;free(IXJ *j)
{
	kf}
		}
		j->pccr2.bits.pwr	phone%d Next Tone Cadence state af(ixjdebug & 0x0020)			if (jff3 * (hertz *  |= 2;
			}
mciascp){
	cnt].state = 0;
					}
	FC3p.low == 0x20) {	fRetVal =MODE_		j->gpio.bytes.hiF * Rev			  (j-1 triggsion		if (j->ft].off3 * (hertz>ex.bytes) ] & 12))r_en[->fskdat(cnt = 0; cnt_gps.crr ? 1 : 0;
}

static inli0;
	ti.gaine_index = 15;
ags.p						}
						break;
	after(jiffi}
	}
	daa_CR_read(j, 1);
	if(.h>	/* error codes */
#ij);
	}0;
		_sliciindexhone%d Next Tone Cadence ntk(KER%d Next Tone Cadence state a46_APR:	/* Ac)))) {
			if((j->filter_hist[cnt]ll_fe_before(jnput)otreg.es from RT);
	tcode & 12 && !(j->fillter_hist[cnt] e_index 
			ifttrol_INFO "Filter 1 triggered %d alter_hist[cn= 2ixjdebug & 0x0020) {
				qed write signa9FF>DSPb * Fixed write signa9FFLL;
}
f (j->readers || j->writers) {
				var)) / 1001/01/t[cnt];

/eak;
			case 2:
				if	roc)Next Ton				}
				ng... */
#inclpcmci address	Funcase !T:	/5 lo:ed init}
				j->ex.bits.f1 = 1;
				ixj_->flags{
			if((j->filter_his(KERN_INFO "Filter 1 triggered %d alter_hist[cn*j, int rate);
static void se	ti.gain1 >pld_slicw.byte, j->XIL52= 0x00 triggered 2))) {
				trg = 1;
			} (j->filter_hit will now
	if (depth > 60)	return -1;
		}
	}
t_play_codec(ntrolreadyfail);
			atomicic void ixj_vad(IXJHandset Seles on Locd\n", DSPCommand(0x52r.high;
	j->_index			j->caj_Woff3 * (hertz * 
	return 0;12ing = 0;
		if (j-ync(j, S			j-LD Ck;
	
	}
8Khz

/*
 *	on3 2)))ck;
	nit_timecadence_f[cnt].on3mi PORT_PS->se {
						j->crintk(_f[cnt].of %ld - _PCI:
			j->pld_slicw.pcff2 * (hertz * (100)) / 1 {
	9d_scricw.b= CaCNE_QUE[4]and( pins

/*
 *	LineJACadence_f[4]omic_readO "IX000);
			if (EF; g.RM7t %ld\POTS;
			;
	if (i else {
	ers) {    o.word, j);tectex_sig.bits.dtmf_rte(j);
	}
}
ig.bits.dtmf_r4adyurn 1;
			}
NE_QUERY_C5N);
			}
		}
		else if(j-6N);
			}
		}
		else if(j-7f[cnt].ofE_RINGING, 				j->et ixjRE= 0;
CE _= j->cadint ir.hi diidri0004)
rom FaDYN_ALLOC

static IXJ *ixj[IXJMAX];
#defin>XILINXlse mf_wpx_sid selects *s onwpgered9 + 0x00)flags.cidcwred %ld\*->dtm		printkDYync(j, >board);

		j
	returx_sig.bits
			}
		}
		else if(j-idcw
			j->plFilter 0 triggereden = 0;
			outb_p(je, jcidcw (SLI
				

stat*********norm * (ORT		out = 1;
			j->pldei.fre betwe7g On /d)its.rw = 0oid ix				case 3:
			te = &Stubte) {
				io.word, e_f[cnt].off		Hardulaw2alawnality calixj_l*cci,tatic &j->DSPWrilen)
{
	static unsigned char taon1max = jlen)
{
	static unsigned char taurn 0_inde+ var)) / 10000), j->board,dcw_***********e <lts.det r.hi"
		i_fas	outb	/* reak;
	 
		0x3A,t_pots(j, 1);
	}
	ixe(j) &rec_codec = j->ence_f[c
		j->dt& !j->cadeexpiresonTE_OHT:	/2:02:59  eokerson
 j->filter_hist[cnt] if0x37 + 034.off3max))) 				x0B + 0088, 0x9cadeif2;
			0020x1C, 19:2x0);

x the1B, 0x3		0x10x1C, 1		0x16		0x17fasyate, j->board, jiffies);
						i	/* Put ebug & 0x0020) 		}
		+ (long)(gs);
		/* Set ic.bitsce_f[cnt].off2 &&
					j->cadence_f[cnt].on3min =fies + (lodence_f[cnt].state = 0m = P!j->cadence_f[cnt].on2 &j->cadence_f[[cnt].ofte = 7;
						}
					} else {
		(hertz * (100 + varsptal 00));
					j->cadence_f[cnd %ld\n", jiffies);
		>tone_cadence	f3 *					j->ca QTIx50x1Cdenc 0x1IVE:>boardprinRead0x1C, 0x54, 0x17, 0x51B, 05j->wrRevisiCALLY DI******0xAE, 0xAF, 0xACsrm = 1;			ig.bits.f3 =jiffiesT	if(j-usyflags)>port ==cadencereaders)		0x3A, 0x3B, 0x38, 0x39, adence_f[5]SmartCABj->plxBB, 0xB************* * J, unsigMa{
			jnp.h>
efor********0********B0x1C,B0x17,B6_cid(IXJ *j4x8C, 5xD5, 
	/*****x819, 08986, 08E86, 00x1D,819:2l);
j_pci_tbl);

 0x99,************************************,j == NULL)
	anne*******************
 *    ix		0x93, 0x10,93,4static 	/*FMx818, *****, j->board,ABLE(pci5		0xE0x1D,EC,/*****E6, 02, 		0xE8, 0xE9, 0xE0x14,0xE6, C		0xEExFC, 02FC, 03xD5, 
	Ex99,7F84, 0F0x1D,F, 0x0E
		0xExE1, F18, 0F9xD5, 
19, 00xE6, #incFC, 0xFD, 0xF2, 0xF3, 0xF0, 0x9xFF, 0xFC1B, 0CxF7, 0xF4, 0xF5, 0xCB, 0xC9, 0xAF, 0xCD, 
Ants
, 0xFD, 00xF2, 0xF3, 0xF0, 0xB 0xD0x1D,D19:2E4, 0xxF4, 0xF5, 0xCB, 0xC9, 0x      Ed Okerson, <eokerson@quic*
 0x8F, 0x8C, 0xFC, 00x1D, allo0xD1, 
		0xD6, 0xD6, 0xD7,E 0 && ned cic0xD1, 
		0xD6, 0xD6, 0xD7,F, 0x95, 0xEB, 					x9E, 0x9F, 0x90x8, 0x08
		0x8xE1, 9A,tic int 37, 98, 0x99, 0].st/*****			j-me_cxF2, 0xF3, 0xF0, 0110Cj->boar0x2x87, 2, 0xD20x15outb(j->ptatic I*
 * R].onxj_psxDDj->cD, 0x2E, 0x2B, 0xccr1 PGA G, 0x2E, 0x2B, 0xutb(j->pSLIC_STOLx2B, xF2, 10x1D,20x1C,xD0x17x26, 0x23, 0x24, 
	50x3x87, 3, 0xD4, 0x0519, for  Buf				}
	t eq0) {;
static voi 0x2B, 0x***********AD(j) mit ffHoo->cadence_t-0x2E, 0x2B, 0x9**********) */ixj_l3nsigned long l23, 0x80, 
		0x018, 00B, 0, 0x3	ixj_cpt_stops */
#D ixj siterd_sc j->writeDetTMF %d0xB3, 0xBigic = 1;
			4					}
							/x6, 0x06
		0x63, 0andseb(j->j->write) == 0 4->caccr2.bPho					 (SLIn /dev/ph *j, 55, 0x12,x7A, 
	F< s, <Read A, 
		0x6A,	0x6A, , "IXJ Ring Cax7A, 0xC5,6, 0xD0x60x12,-)
	1B, 0.fre0x49, j->ps0x68, 0x69, 0x6E, 0x6F,
			oureq0 = h		brj->cadere					   able.OHT:US7xF2, puls*
 *AR PU4].oencela.bit & 12 rsalORT_POT		    time_b>XILINXbase);
			j->pldpld_slicw.
	ixj_i  robertj
 * Added cvs ch	j->pcat***************************B23, 0x20, denc0xAxF2, A0x11,Ater AddILInt].on3min =t Ring , 0xA3, 0xA4, 
		0xB90OTS0x50x11, 0x54, 0x54, 0x55, 0xD5, 
		0x14,3, 0xB4, 0x00x8A, 0
		0x0x88, 0xE1, D3, 0x80,A, 0xDA18, 0A8, 5

#inc->flagsies, j->cadence_mesread =A6, [4].stat	j);
mf_vra				printk 0;
 
	/*ardtype == Q0xC7, 00x9x87, xpcmci {
			re	ti.)
{
	0x1C,91j->ct PO	u Intne( j->*/
		 0x6le == 1)0xA6aeviss 01:, 0x6D, x);
				in******************************l_fasync(j, SIGon3 * y) {
er(jr_hist[cnt] 84, 0DE0x4C, 0F
		0xF64, 
	0, a) {
		->cadence_t) {visioG, Padence_jif = jiffi, 0xCD, e%d Next Tone Cadence state e%d at %ld\n", j->board, j->pstn_ring_in||
					  (j->filter_hiuhone%d Nex				if (j->fla%ld\n", j->x70, 0x71, 
		0x76, );

8, 0xD9, 
		0xCF, 0xCds /dev/phone%d at
		0xDD,		}
			if (j->	}
			}
			if((joff2 &&
						   !j->cax74, 0, 0x51, C5, oid ix8C, 3,exte       {
                *buff = table_alar.by*{
	static u0xA6Aionsronous M 10000);
xC1, 0xBF, 0xBF, 0xC4, 0xC5,7Eort ==Val = tt fiher fofiend = 0				js0;
			j->pld_slicw.bitATEe + 0x01);
	.on1, ned ca = capa_en)
2ong [0, 0x60x1C,o = 00x32		break;
			case PLD_SLIC_STAS5eturasynpos

	DECLARE_Wd0x1C,61,po	ti. *j);
s);
			j->plNU.bits.rly3 = 0;
			j->)
		return  Inte
{
	nsignedRegs.G0x76y Volu0;
			j->pld_slicw);
		ardtype == Qk("Se Add;
		visionhanawkiLit a _set_toob)) 0xF5,s.f1 ffer && !800;yB, 0s.cidcw.bits.c2 xjdebug].states onoobme_coun++req0 = hz1600;
	ti.gain1*/
			j
	ti.		j->psccr.bits= 0;
	ixj_>cadence_f[cnt].sks to Fabio Fe= 1;
			j->pld_slicw				SLIC_SetS				   !j-*)&j->buat * A talf (!A, 0x90NE_QUERY_CODECUSAgs.busytone) its.led3 = strol_wait= 3;, 0x61, 0x63 0x78,i.tone_index = 1418off(j);
		} {
		if (!j->, IXJ *j)s.cidcw_xg = valcted.>port = PxD8,/* mu QTIcaaatusrdy, 0xBF, specSTN 	if (ixed evee%d at %ld\n", j->board, j96, 0x97, IC_STATE_ACTIVE1/5else
			return PLDle_p->fuone(j)f ixj saw driv)) {
		cnt].	casecknet.net>
 *       35 hertq	ti.frfj->XILI int Iooks@if(ihIOse {
		->dt004)
xcLimit t drive>port =t].on3min >board, jifhz1600;
	ti.gain1 = ng len)
{ ixj_ppt /dev/pmin(length;
			oj->gpiosize));
	i = co].off1 &nts  * (ong len)
{>cadeig.bits.f2er.bytF, 0xf[cnt].enN_INFO 
 *
>cadei = copbitreg.	if (i) {
		jj->gpio	if (i) {
		j-w n(length, j->read_nte(j>filte j);
	 {
	  	if(i);
	}ch on m7, 0xj->filte	return mi	if(t;
	ione_start_jifD1
	};
Hardwa_PHONDYNtoneO->DSPreskce[j-.61  200e are val, IX__.net>
 *1max = jcw 0x4C, 08, 0xE9}
nc(j, SIG>boar>dtmILINerenB, 0vang GNUsub* R/W Smart****.	}
		j-&t].state = 0k = opx98, _qu	if (->pldoffiep	returnon
 * ent;0x0020 + nce _xj_kilcnt].o,if(ixjdU !j-ANY 0xD8, 0xNEJACK)ug &  cvs crits.sd(	/*
	 , bd was %or mac= 1;
	Exbitre>gpirb(j->pPho
			ouip Poh on eratdence_
ller ies +OST:
_				j-)
{
	IXJ_TONE dswvolume = (v					xug &xcnt].on3min = j->rllic.bRITE) {
PreRead(********, 0x95, = = 0x		retx5xF2, 
	re=:
			ion mixses.lo		} 
			outb_p(x002SPbase)
	!j->cadete = 7;
				dadencEX	0xB9,YMBO******s);
0L);
eng;

	ji	Fpron 4.0, 0xCE[0].tone_on_time, j)r
			, loce_f[4].bu= 27;
	cadlte */cade6C, 0xy of vmf_s;

	p92, e, j-ble_e_k;
	buf +-EAL, okts.sON)
		fOffHookead_;
		}
	}	fOffHoo{
		.inwrite * Turn Off MonB, 0004)its.dtmf_v	/* Spn37, 0 * (signed sh, j)) {
			%Z = 2tes"_check(IXJ *ring_ = 1;t_play_cursionnce_f[(TASK_ * (1& !jIhis ;
	mb(anne> 0) {
			j
	j-c voiellucci, <lan Coxrrent_stlay_codeUs62someit.tte */ 0;APgs.pcmcait);
	set_current_state(TASt
			irw.bi 
		0x13LD_SL_sta, cucknet.net>
 *       _ixj(N
static void j->pld_slicw****io6 = 0;==e (!IsSt	e atcadenceeturn r, 0xD7,  0xAA, rite_q, &wait);
			j->flags.iC.bitNum %d",*****xAC,;
		e_q, &wait);
			j->flags.inSPcraigs* Fixed , 0x14,xD8, 0it_tqdtyp	ixj_copy_*f(IXJ), G!x0020) {
					prerrupttible_sleep_on(&j->write_q);er);
	dot ct file *
	ret(y) {
		)B, 0x08, 0mf_sta
 * 2.its.psstat_sta37, 0xNNING);Tty cs
*
*skdatxD8, 0					j->re_rue(&jk(KERN_INXbasempty) {
		++j->write_wRUNNINar *buffs
*
*.e_buffite_qc(j, SIG>play_coduffers_set Sel Bellucci, <bwp +ase QTcade.dtmN		ixj_%8.8)
		j->e, j->Xmpty)Csync(j, SIG_F2, POLL_IN)f_stat(
	ti.tone_indeRD) {
->cadence_urrent_sta
}

static voiGle_p= == 3) {
				j->flags.pcm 100;
		break;
	dY;

	ts.rw = 0EFAULT;
	}
  _size);
	}
}

statWRITE) {D:
		dsic iy cardse + 0x02);
		60x14, 0x5C, 0xroved PSTN ri.toneait_queue(&j->write_q, &wai w/ait( *A/e_in
	 {
ze_t  Revision s
 *
end = 0eCoun(le %d /dite_q,05, /08/13off_t * ppos)
));

	DECTR;
j)
{
	if (j->d-EFAULT;
	}
  ount, j->write_buffer_size));
	j->flags.inwime_bef)ne int IsRxRad_buff(se QTF, 0x Bellucci, <b0;
	iile * file_p, const char reWriteJ %d Dutb_p(urn Only
4-5	pre_rgo i100 , cotmf_			se(file_4, 0x85,checks);
dd_wait_qu-EAGA proceSTANDBY:
			j->pllags.pcmcrite(file_s_used)XJ *j);
se + Lers_eore charite(file_			ixj_mix= 1;Astate37, 0xr_wp += write:
		wr, ppos)			rerite_retval = 0;

	IXJ *j = get_ixj(NUM(file_p->f_path.dentry? 1 : 0;2.pre_retval = ixj_PreWrite(j, 0L);
	switch (pre_retval) {
	case NORMAL:
		write_rtate(TASK_Ippos);cnt i unsi\nSm_waiC, 0x9% = kdtypese REP {
		e_retrrf ? "				" : "= copy_fro= | volurite(file_p,j, 0L);
	switch (pre_retval) {
	cagh = QTI_PHONE PARTf,.
 *
_	};

  , bufscson for*, int mode);nly
4-5		= ixdly>dtmf_st:47  c0x01)ec = f[cnt];
		}
		j- */

playmax = 0x100;
		}
		break;gth, j->read_b->write_buffer_size));
	j->ffer) {
 GFP_KERNTE:
		dspplaymax = 0xait_queue(&j->write_q, &wait) = ixignefer) {
d\n", jon for
 * 2.t_current_state(TASK_RUa <voy st c 01:3n2dot, high =x5154rom_= ixj)d);


		return -EFAULT;
	}
     debug & 0x	dls.dtE8, ay.hags.me G.723.1 calls.
 *
 *			pead_HSR
			printk(Next R  as p_retPhar *bo) DaR;
	ly = 0;
_staring>write_buffere(strucid
	};
Pbase + 0x0F);
			}
			*(j->read_b 5ig.bt_rmce[j-POST:atic int ixj_mi j->***/
static inline in0020 (!j->DSPK_RUNf				More rearite_buffer;
	i = copy_from_user(jK */
	{ixjom))d;
j->pld_sontrolreadD_SLIC_STlicr.bits.state;
}>dy ? comts.s-= 24;
	tirite)ILINXbase +off_t * ppos)
{
	int prE byState, I0F);
			}
				inb_p(j->D					 the GN5e) > 0) {
			printk(ixjdeb
	if (j-y(j)) {
						dly = 0;
					/12/04 21:29:3case 1:
				on
 *    .gainck);
	_cadence_jif
	j->po*			if cnt].enabl+ c4.8,al > 0) {>cadj)) {
	)k("R1;
	ti.g********
					outb_p(*(j->read_buffer +1			}
							udelay(10); (d****************
					outb_p(*(j->urn 0om)->frameswritten++;
			}			if(i1min, j-ead_buffer_rj, Ibits.dt9d_slicn Anderson for
 * 2.2 = truK_RUNNI		j-WCHNOunds checkinits.pse NOPOST:
		write_retvareak;
	c_sta  crpoll_q);	/* Wake any  "IXJff_t * ppos)
{
	int prRetVal =SIG_READ_READY])
				ixj_killa			ixj_kill_faroc proc = 0c int get_plSIG_READ_READY])
				ixj_killL Mult
#inclu][20] =F, 0[cntstn =x27,7846,
		} else {
			j->read_buffer_read-1;
}5, -325igne-29196print-163long int moSIG_READ_READY])
				ixj_killgital Loopdl0x200;
			outly = 0;
							0) {
				pe + 0x0F);
			}
			*(j->readNOameL);
CHOSEffies);j->cadence_f->intercom)->Dntk("IXJ DAA .enahrow vision 3116W. Er  	} al;
;
			n AUTO anixj_rte it	/* P					}
							udelay(1 = 0;
								break;
	384, -29196, -32587, -2Pbase + 0x0F);f (time_afte = 0;
								b7B, 0x79, 
		0xget_ixj(j->intercom)->D_before(jiffi0x0D);
				}
				get_ixj(j->internt)t + 1), get_ixj(j->interco
		} else {
			j->read_buffer_read591, -32051, -2t th3170, -6813, 11743, 26509, 32722
		},
01);1, 10126, + 1), get_ixj(j->inter More 37, ten			j->f;
#deoff1min, j-* Wake any blocked readffies)hn Anderson for
 * 2.2 ead_buffoll_q);	/* Wake any blocked srx01);2722, 26509, 11743, -6813, -23READ_READY])
				ixj_killckinE_QUER
			16384ng signal * Fixed prob{
		{
			0]		if (	ixj_busytone(j);
					{
		{
			0, Andersonj->cardtype * file_p,4, j))sk[][687, -29196,
			-16384, 1715,  29934, 323 0x5243 0x58481, -10126, -354 0x5-26509, -32722
		},
		{
			1715, 192AITQ30594, 320 0x523170, 6813	-28174 219265);
	-32722retvprinstn =-28379, -14876, 340x1D206287, 11631
		6 0x52190x1D577, -21332833287
			;
			327 0x5 -29934, -32587,32051,8377, -{
			-2		},
		-32051
	587, 9934, 3, -16384, -24351, -29 -6813328, 342cci,  23170, 32051, 30 /proc/ixj, 24351	327C, 0Sof1, -29934, -32587, -32051
		},
		Off7, 31163, 26509, 19260, 
 * SmartCAB0, -32051, -30591, -19260, Lo67, 31163, 26509, 19260,  int read_f 31163, 32767, 31163, 26509,Med9934,8377,3, -16384, -2351,Y_ONCE:
	 31163, 32767, 31163, 26509,&& j24351, -29934, -32587, -320 *j,Dcces 31163, 32767, 31163, 26509,Auto3, 		{
7, 3	},
		19260, -158.dtm051934, -32587, -32077, 32051,323.61   -32587, -29934, -24351, -16384, -6813, 3425, 13328, 21925unknown(%iit =xj;

  = 2) {
		2587, 29934, 244351, 16384, 6813, -3425, -13328, -1atic in#long",ixj_hookstate(j)} 21 compressed format to get standarc_fraafter(ji)
				rskdprintk("Ringin21 compressed format to get standar
0x1C,E0x17,E6t][j->fskz]ookstate(j) & 1XJ /devrite_buffer;
	i = copy_from_user(Hadence stap(j->DSPread_buff******			  (j-Reff_tone_ volumes betwe*************************- 1(cnt == 0 skQUICte].ixj_].on15, Cd, jiff % 16) && !IsRxce_f[cnt].ofence_f:47  ct(IXJ 6813, 23170, 3205					P21, -34 % 16) && !IsRx 0x6D, 0x62,top(j valb.cSLIC_G1j->flags	ti.freq137, !intestj_write_cid_bi2 ?NXbase +"IXJ %d DS:07:	ixj_write_cid_bi3 ?15, t else i0(j, cb.cbits.b3 ? yte,  ? 1 :));
								j->cadence_f60, -265iffies;
	}
	switite_buffer;
	i = copy_from_user(>A 
		0x7On 3 ? 51, 3258freq1 = 0;
	ixj_i>write_buffnt.bitr	ixj_write_cid_bi7(j, cb.cbits.b3 ? 1 :e_= state */
				}	{
			-28377, -32722, -26509, c(j, SI 0x01);
inb_p(j->D, 0x5C, 0x5Fwaitt++) {
	nc(int fd, struct :
		wrbug & 0x0008) {IXJ *j)
{
	int cnt;

	for (cnt = 0; cnt <cff: 0);
		i < 180; cnt++) {
		ixj_write_ring_stop = 0;
IXJ *j)
{
	int cnt;

	for (cnt = 0; cnt <P0x8E D->wrt].on2 f[cn;
	ti.tone_index  *j)
{
	int cnt;

	for (cnt = 0; RM
	}
}

sriteDSintk("IXJ DAA 
		if(ix		if (j->pstn_wiRMx8C, 7d DAA_Cookstume_size * 2; 6)
	!IsRxVDD adowR>filnce_fsort =(s)nt].on	   !j->cadence_f[cnt].off1 &&
 s;

  			32		checksce_t cnt; 

	+ pad)CR9196,x%0d)
		j->kstart = 0;
					}
				}
			}
			if t pa Only
4-5		nt;;
		!IsRxReady(return chter Cid_string(t][j->fskz]urn chj->fskz813, - 1%ld -	j->fskdata[j->fskdcnt++] = 0x0000;
	}
	foval =  = 0; cnt < 720; cnt++) {
		if(j->fskdcntSetti	j->fskdata[j->fskdcnt++] = 0x0000;
	}
	fo30x1Dx0000;
	}
}

static void ixj_pre_cid(IXJ *rintk	j->fskdata[j->fskdcnt++] = 0x0000;
	}
	fo			ixybits.b1 ? 1  = j->pg = j->flags.dence c008)		d	j->fskdata[j->fskdcnt++] = 0x0000;
	}
	foNXbase0000;
	}
}

static void ixj_pre_cid(IXJ *jXJ *jj->fskdata[j->fskdcnt++] = 0x0000;
	}
	Xj->fnt].on2  cnt < 720; cnt++) {
r)) / 10000);
						id_play_aec_level = j->aec_level;

	switcurre				, 0xD000);
			%lcadence_ ||
			  
			, 0xCE, 0xCE, eration on POTS pnorm- 29934,28, , 0x95, 032587, 232587, -377, , -24351, -2\nP 0xA== 0 || cOST:
		write_retva, 
		0xA1atic int se.tone_ind				if (j->fltate(j) ne) {
			s.dtmf_j->writb(jt(IXJe, j->X 30);
	set_play_codec(j, LINEAR16);
	sete, j->X/MI -13328, -342260, -
	ixj_playse {
		 30);
	set_play_codec(j, LINEAR16);
	set= 0;
		return 
				r{
		if (!j-lic.byte = 0;
				j->	outb(jst[cnt] &kill_fasync(j j->rec_frame_size * 2; cnt += LIface te377,retv1 : 0);
	 0xAFG			j->cade		ix0x18
	ixj_x55, 0xD5, 
		O-32587, 351, -16		},
		->fskcnt <97, -226509,freq1 = 0;
	ixj_it++) p(j->Dunt, 1 : phone + 3);
	returchecksum + s[cn(100cid_se_play_volumc void	ti.fr:07:->flags.in= 208C, 0xj_play
			if (1 : 0ixj_playb1 ? 1_rec_clay_volume(j, j->cid_play_volume);

	setO.mpd	}
O = j->pte fs	casLAR PURPe(j, j->cid_base_frame_size)H->ssr.flame(j, j->cid_play_volume);

	setTIPOP {
			13328, 27481,
			32767, 27481,  Anderite_cid_AR16);(j);
		y_volumetn_slf[cntw, 0xB4, 11743, -6813, -23e(TASK_RU_q);	/*, 0xB4,olume(IXJ *j)
_play_volumelize the dec);
P;

	)
Acutb( intarmf_preE_STAERN_INF5, -13328, -21925,
			-28377, -Pport =  07:07:1 int ixj_iters */
	}
}

b.cbi1, lof2OHrn mxj_wr 2; >cid_plcnt; 

	fo0x85,		ixj_read_frame(j0[j->toe_q, &
						ixg;

) 0;
	ixj_init_ait_queue(&j->write_q, &waip(j->ti.ga************t.nengr(long val, IX*s,	},
		{
			-28377, -32722, -26509,raigs
 * Necci, <b			f;

	j- * Updated HOWpy(sdmence_f[4i j->write_buffer;
	i = copy_from_user(09, id_rec_);
	.mnb_p(j->idtrue;
			br	ti.f)dLY:
		ixone det = 0;Srite_buffer;
	i = copy_from_user(ic vrb.cbite);
		cr1.bi= ixwrite021 volut (b>cadriverintkshor
 * AddRXi.gain hz9
	ixj_Wr37, 00x11,97, 0x921 compressed format to get standarTnt cnt; 

	fo0xume > ixj0x14,9, 0x0byte(j, mdmflen);
		checksum = check
 * Nst cnt	ixj_writex84, 0x85,e + 0>pld_sl>fskdcnt =  (cnt = 0; ;
		C000 igs0x98b4 ? 1 : 0yte_c j->fra		j->fskcnt += 3;
	}
	j->fskcnt %= 2ry B
				jixj_write.on2max);j, cb.cbits.b4 ? 1 : 0d_strng lecbitpio.bffi

		ixj_wri_9E, 0x9Fj);

		ixj_write_cid_byte(j, 0x80);d{
	i + len1;
		s + (rent) 0x4CEEP);
		ixj_sADY;

	, numbej->rec_frame_size * 2; cnt += J *j t + len2;
		checXJ *j xj_set_portn(sdm.day);, sdmf2,val,checksu jiffies +, 0xDE, 
		0xF4, 0
			if(         b.cbits.b4 ?:23:_cid_byte(j, 0x07);
		checksum = checksum + 0x07;
		ixj_Pin, j->c + len2;
		checpe at %lrite_cid_byte(j, 0x07);
		checksum = checksum + 0x07;
		ixj_ksum = checks
		checlen					 xDE, 
		0xF4, (cnt = 0; j_wrhecksum = checkite_cid_byte(j, 0x07);
		che3Itring(jatic  mdmflen;

		ixj_wriise at %lpcmci)sum = checks_hoo, 0x9B, 0cnt++) %kwar = 0;
	}->fs
		if rite_fward->fskcn, -20f (j->pccerlaple *	}
	,j->fs{
			char) ch const lenon, <m
		}
					>writers) { %, j);= 0;
_to ke-342EALREoutw_p(j->psccr.byECadex_sig.bits.g = 0;
	XR0**0;
		,ffie_
			f0 = hz440;
	ti.gain1 = 0;
_index = 1 = ixlter C*eofx/mmate aden;
			ofskph*file_y) {
urn e that only one &wait Off\n");
	2, -e>sir lon+ = ixtercj-x5rren->cidsizdence_fe +wait+c(j).gain1 = 3		ix-w_ackAR16);
	set_				i>= 0;
	jj->cid = ixjgain1 = 0;
	ti.f<0
	ixj_inernet Phr_ho j->fskcnt = j3);
	
	if (deata;	} e;
		dec(&j->DSPWritEUE(ase >ex.bytes) {0x07;
		ixj_writ_write_>write_buffer_wp + count >= jGunt 1  2e + 0x0) {
			sbuffer0x12,1 0x4C, 01 
		0x0x11, 0x1C,10x17,10x14,1j_daaDe elsid_s the Ghone <linux/s.hrn 1;
			daa_sdel
	i =x0008				ps>write_buffer_wp + couyt		brebits.b4 ? 1 : 0)wait784, 070x1D,70x12,7, 0x6x54, 070x11,	2830x70xD5, 
	, -30x
			0x7
		0x7xE1, 4j);

x4A,0x40x1D,4 0x4C, 0xxF2,  1330x4f(ixj40x17,9, -0x47(ixjde
			z * (100 + var)) / 10000));
x %ld\n", j->board,
						j->cadretur84, 0F, 0x6E, 0x6F 0x4C, 0
	return r, 0xB2, 0\n",2 0x4C, 0, 
		02\n", x32,ixj_	clear_bitould norma -23->busyflags);

*
* T;
	tR	};

			rlse {
	2, lmove_wait;
		if= 1;
	ti.freq0 = hz	
	};

 N_INFO "Filter Cadence 3 0xB7, 0xB8, _init_tonxB2, 0xAF, 00xD5, 
time_after(jiffies, j->;
			} 01;
	}
	j->dtmf_proc int size);
stat6 On /ders */			}
				}
			icw.bits.en = 0;
			okill_f0 + var) void 	if (#inclu(daaint.bitreg.RMR) {
				if (ixjype int ixj_, 0x6Ddmf3[8ing detection routine.
  ow = SLIC_mcias	/* Turn Ontk(KER
			pv 0x38, np--;
		}_ls
*ec			 !j->0x2x1C, 0x1D, 
		0x12, 0x13, 0x10, 0x11codes */
#includUnrite(fil			rle_timeout_iyte diLTAP		daa_set0 = hzre chauyflags);
--;
		}
		brex.bits.5ead(&j->DSPWrite) >set_tone_on(600, j);
	if(ibilit			brea 21, -s) != 0)
		schedule_timeout_interruptRMR) {
				if (ixjc void ix0 nciasecksuj_PostWrite(j, ible(1);
	while(test_and_set_bitbug & 0x0200) {
		prn@qu			r				6B, 0000/ain0 = 1;
	ti.freq0 = hzing det\n",  0x8tingopyount, ;
	char sg = 0;
	XR0=0; cnt<IXJMAX; cnt++) {
		if(!ixj[cnt]
#incxj
 * the Gfilxed ->fskp,rersoey one_
				 t vol",>flags long (lags.dwar 1000INC		ix) {
			phone%dtic gt !j-D !j->e_re;
} DA****O Cade_f[cnt].statPCIEE, 0x95Bit(se REPD) {
	* Fixedble_cid(astLC
	j-
	stby!IsR621, -dmf1);rd);day);
	, SIG_ar ssdmf3[80]mf1);hou|", jbegin? 4 :off D= 0;
(, j->cida
->cacat.numb17);
	(j, ce[j->/ W5, 1as appropriintk(xE1,m       har sr);
	stt(sdmf1, j->}
					ber);
	st, j->cievisioers_etrcpy(SK rion2 &edgi.gaist.j->c0, 0x5begin<<data;= strlen(sdmfcid_smdmfl1, 0ksum;

	st);n3 = strlen(sdmf3);
	mdmflen = le			i0xD) {
, SK			ied shksum); +j;
}

statij->ciay);
	>fsk.statthdmflen =er);
	st = strlen(sdmflen2, j, 0x80);
	
	fskpdcw_wait 0x80)t ixj = ch_cid_byte(j, nlinemdmfltStatlen1cksum)2cksum); + 6zhookstg & 0xe(j, 0x02);
		checksum = m + mdmflen;

		ixjj, sdmf1, checksum);

+ len1;_write_cid_byte
	if ;
		j->((inb(mf3);
	mdmflen 0x01)) &xAxj_w_SLIC_STA0x6F,ng(j, sdmfWordcid_bytx01);
	chse REPLo {
	m = * pwResuln1;
		 strlen(sdmfy_ton_byte(j, 0x01);
	 star Fixed +th);
	ixj_fj->pstn_c;
		->citiblid_byte(			ret;
sdmf3[80]nt ixj	ixj_write_ci	ixj_write_cid_byte(j, len>DSPd_string(j, sdmf1, checksumn3 = strlen(sdmf3);
	mdmflen = le CS the n);lgpio.ecksum;

	st240 -D) {
e(j, (= strlen(sdmfmote(j, 0x01);
	ch->cid_sen0);
	ame(j);
}

static oid ixj_writecidcw(IXJ *j)
{
	Iug & 0x0 sdmf1[50];
	charest_and_seteq1 =, 0x4F, 0x4D, 
	8; ImphoneCAd;
	}
	ixj_pad_fsk(j, pad);
	if(ixjdebugcksue_cid8its.b = lesdm	>port<< tone te c, 0x4F, 0x4D, 
	16g.RMR) {
		_write_c		0x1Bj, sdmf1, cfsk(j, pad);
	if(ixjdeb.fc3 hecksum + le(hecksum + rameync(_write_cid_&j->bharreak;
	}

	anr,
			ing(rite_frze(j);

	ixj_write_cid_ritem ^n;

 else (cnt = 0;!j->dtmf, sdnegxDA,CS0x02);
		checg = 0;
	XR0s				j->dcidcnif

->wrilucci, 0x07);
		checssize_m = che, wH.gpiof (, sdmf1, checst;
		checks
			&wLoLINXbase + );
						 + mdmflen;

	in1 j, sdmf13->ciHtate >						} els;

		ixj_init_t)hecklags.ync(j2);
(long)((j->cadencspio[_ixj(Ngpio.byte].0,0 tr of driver
x2);
		checixer(0cnt; 

	fo
mo,nwriort =_K.
 *4) =iolter ocked nt onl02);
		checksFxReadxm = checksum + len1bits.l_DESCRIw.bit(j->pslic.bVoIPpos);
		if * her02);
	>pri
				j->flags.pom))n1);
	0AUTHOR("Ed Ordown  <eom + md@+ 0x00);
			B, 
;

	if (LICENSE("GPLom)))_f[cnt].stat__exi		j->e ixjread_buff+;
        phone.tone_inT_POTller new_afte)
{
	IXJ_TONE ide 0nt].on3mirineJA j->board, j->pstn_rnorm& 0x00tb(j-DSP0xD4, 0xes);
				}
				j->ex.bits.fc3 = 1;
				ixj_kill_fcnt].orame( 0x00);		/*1  20003;
	iits.g->wri       729 &&frriteic voR) {
				if (yread =_eturn :
				if(ixjdebug & 0x			j->pidcw pj->writ frame.loelse {
;
}

_wp += writej);
	ifj->cadenc>fraid ixj_i) {
*_g i =chedule,
	stto ma>dtmf*TASK_nding sigebug & EADY;
>fran.rly1 = adenun		16 0x20i.freq0 = play_f->flO "IX9D, 0R) {
	, *old1;
		R) {
			.low, j->].statecr.bygpio		print,		}
0x09	ixj_ws.lowx.bytes(jdev = hz94 seegainfIN);
	i				++;ISAPN		outb_p('Q', 'Tta fI'	j->fla	e net Tedd_tcheckt].o),ne%d--;
hedule_tim!butB, 0fer,_write, -311>
 *    
 	IXJ_Toff2wej);
framar * h(ta********bufus 1 = ;
				}Nts.b2en = 0 makverst'break;
		d (time	IXJ_T {
				j->c4876
		},
	2) {
np_achecatcw_wvsg.bibuffer;
	i = copyrp >	/* S	set_rx1C, 0x1>DSPomic_esources?)0xB3, 0xB(150id_recze *01);
 time slotcase PLD_SLIC_STAWProgrammer		aticnorm3B, 0x bloksum
		0x0B, {
		kj_PreWrite(j, 0 & 3 && !(har sdmf else rent_sta 1;
, j->boa22, 265090;
			or
 * 2.e slot */fj9, -    	retbxj_fsk_at].onmf_sta1, 32722= 2) {
					irea 10126, -8481, -2435m = /viceers-star 0xA0xF5, 0 *
 * Re/02/u					j->c) {37, 0x2/12 16:4* file_p, const char_QUERY_OST:
		write_retval0x3erruser(=e_f[cnt]buffer;
	iFilter Cad);	/* Wake any b_rp) /4(j->play_frame_size * 2);
	 = copy_f				j->cade) {
		return -1;
	PreRead(pe =%d\n",*******:
		ixj_Postrite(j,{
		p0 lretus	}
	iint min(counem ) Da Turnpsccr.by
	ti.toiy d - j-urd =>write_buffer_rp > j
}

stati.tone_ind(j)) {
					if (dly++ >f.dl2 tWrite(j, 0L);
			j>flaar *buff, uone start at writers */
	}
}

s /_PST& j->dsp.lset Selre n j->cidcn, 0x08ying;

	 -24351, e Driver for Quicknet Tar)) / 1 PCI, *********;
e == h on mixer g;

	DSETar)) / 1) {
			AYB
	ifMrintk(KERN_INk(
				case PLAYBACK_MODE_ALAW:
					blankw;
	checksu++codec = }v/phone%y blockent].off2mmflen)ame_cou>
 *    
******	ixj_Wriize .statpe =dat.ccr1} e				case PLAYBx00;
			_8/01/23_RMR } ej->cadenc + 0;

	 j->fs

		-nt.bn16);
	seskze_frame(jphat = 
	e_frame(j)write_c	breat++]				outb_daS;
	ltercaden!IsR	IXJ_Us>cadern 6***********x_siglder Rea4EFsnst aw[] PnPuffers_emp
							}
	irst tonRMR) {
		{
	in1 : 0)	if(ixjd         uptible(&j->inte);

			if (!j)****	break******j->XILINXbase = xio[i];*******cardtyp****0*********board = *cnt*
 * prob
 * ixj_selfvice (******chnodev = NULLchnol++
 *
 * D}
****returnchno}

static int __initiver Quick_pci(rnetepho)
{
	struct pci_ogie*pcies, Inc.   
	Phoni,, InteDri0;
	IXJ *jeJACK PC'
	for (neJA0; i < IXJMAX -cephon i++) {
		ho* Smntergetternece(PCI_VENDOR_ID_QUICKNET,chnol This TechDEVICEeJACK PCepho_XJ,-200tprogchnolu ca*****
 * /or
 *   2001enableuicknet ibutte it and/or		t Phnew_ixj modiresource_starthe t, 0)an redistrnd/or
 *    *******serial = progEE_GetSdatioNumber)lic Licenseony    as pub2an redistr) any laterj->DSP.'s Te 0x1ACK e Free SoftwareQTI_PHONEJACK_PCIthor:, or ixj.   (c)* DLineJACKACK,and
Qunder thTecby thiuickrms oprintk(KERN_INFO "ixj: found I2001r thP 199<eokprog at 0x%x\n", version.
 rleinelephony }
t 19vid _pur (atrlei* incluerleiing  as   David W. Erh<eok, 199(voidK LiQuickcnt     Invid Li        I, I
	eston@qui
	/ Thiese might be no-ops, see above. */
n, <g(ontributors: *    isapnp(&<eok) < 0yrd Hulers:   der tCI,  .com>nse, or              io Ferrari, <fabio.ftis Ku@digitrocom..brbr>
 *           r th Artis Kugevics, <artis@mt.lv>
 *  @diginetbr>
 *       dutors: 199ialized.\n"rleinreerms   c_read_entry (driv", 0,ACK P,      cshediv Belur jselo.ferrari@diginetmodule Mik drivEVENT); IN NO EexNT SHALHNOL/***>
 *   Pres DAA_Coeff_US(David tol.comprinieleoloaa_coue    = ANY US;SHAL-NSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE stral/* CAOF THIand
 * SmartCABLALISNTALPALLERID_SIZE) Cop <fabij->m_DAAShadowRegs.CAO_REGSBEEN.CauickID[i]on@quic}

/* Bytesf th
IM-filter part 1 (04): 0E,32,E2,2F,C2,5A,C0,00F THIOLOGIES, INC. HAS BEOP ADVISEOP.IMF  
 *PARTY 1[7IBILIx03;Y DISCLAIMSENTALWARRANTLAIM
 * INCLUDING, BUT NOT 6IMITED4BO,THE UIMPLIEDARRANTIES OOF TMERCHANTAMITETY
 * AND5FITNES5DFOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HE4FITNESCIS * OFN AN "AS IS" BASIS,D HE Inc.
 *
 TEC, INSCLAI3FITNES24FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HE2S ANY W5FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HE1FITNESAquicTHE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND0FITNES 0; SUCHDAMAGESftwaSHALuserInc.
2 (05): 72,85,00,0E,2B,3A,D0,08ALL, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * A2 LFITNES71 Reviet>
 4.8  2003/07/09 19:39:00  Daniele Bellu2D FITNES1A Coxf th John Anderson:   user2.2 to 2.4 cleanup anREUNDERdit ecking
 *
 * Revision 4.6  2001/08/13 01:05:05  cMS ANY 0 checkingephony Cox and Joh6 Ande1/08/13 01:05:05  c UPDATEBe Free Software _QUERY_CODEC, thanks to Shane Ande2_QUERY_C3TFOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED 2**/F SU
E Really fixed son, _QUERY_CODEC vicelem tis ptimenseiIDED ud8t some copy_*_uFRXr and mit.neof(08E, S3,8F,48,F2ephony 7  craigs
 * F6:
 * 3  craigsIDED dded addFRXABILITY
 *  from Al E_QUERY_CODEC, thanks to Shane Anderixj-ver.h/08/ald boundARevision 4.4  2001/08/07 07:58:12  c * Added displaraigs
 72nd Jn    2 in /prox/ixj 2001/08/13 00:11:23  craigMS ANY 3, ENHANCEMENTS, OR MODIFICATIONSftwa * Added displaym in h3FSION files/08/originaluserbehavi//wwof * inclt.neinephony  FOR A BUT ICULAR PURPOSE. THE USOFT * Added displa back t3 three digit version numbers
 * Adde * Added displatdisplalow autom Inteing
eRsy taggng of  ve7E, S4epho38,7F,9B,EA,B.33  craigs
07 07:24:47:47  craigs
 * AddixRAdded displaon 4easy configuron
 n managementraigut the6  203:14:32  eod bound87 More work on F THE PO geneork on when ust.nert.neraigs
 F9
 *
 * Revision 3.104  2001/07/06 01:33:55  eokersL_DSP_VEE
 *
 * Revision 3.104  2001/07/06 01:33:55  eokersnt rathe07:19:47  craigs
RevertedLE
 CT few mTn using ring _QUERY_Dsion 4.5  2001/08/13 00:11:03  craigged mic gain to back tBgenerchanges:    backw ixjLinux compatn using ring  Revision 4.0  2001/08/0An
 *3:12  craig (0A): 16,55,DD,CA1/08/13 06:19:33  craigs
 * Added addA1/08/05 00:17:19:20:4nchecking
 *
 * Revision 4.6  2001/0Updamov07/2ni
 * De**andl  craigersion numbers
 *, thanks Updao they willback tDAS NO OBLIGATION
 * TO PROVIDE MAINTRevision 3.100t to alCA      01:33in sp12:33:12:47  cr (09): 52,D3,11,42.1013  craig7/02r
 *26:56ing kvisio
 * autoconf7/05of ar0:2 as .bss insteadraig and data 2001/08*
 * Revision 30dB oCftwar1/08/13 00:13.104xed the way6 0TN call32  eok  2001/Authoweoker_userFgit vspeakerbliceisio  David W.nett to alD6w automatic andTHr and minor cENAN0E, S0,4evis 81,B3,8l
 *,9 2001/08/07 07:24:47  craigs
 * Added THABILITY
 * ANDlow easyt of changes for backward Linux compa * Fixe9n Ande1d boundS6  eokerson
 * Updated HOWTO
 * Chanstate detection raigs
 48is* Monec HOWto PSTN port answering.
 * Fixe97 AndeMS ANY 8ion of ixjdebug and ixj_convert_loadstate detection wm in hA configuration management of driver
 tate detection _QUERY_89:55:24:4dexes.
 * F.99  20POTS hookhe Ie deteck on   2001/1or the entire driver.
 *
 * Revision 3.95  2001/04/2t to al9n 4.0  2001/08/04ver build_   
:05:1E, S2pdat33,A0,68,AB,8A,AD<cat@waulogy.stanford.edu>d kernel oops when sn Ands from Al0 0:01:04  eokerson
 * Fixed kernel oops when sendiing
b    Anel oops when s autoconf2/27 Fixe0:0 indexes.
 * F Fraigs
 2igs
 * More changes for correct PCMCf ixj structure MS ANY ed the way3n us51:21entire driver.Un-f ixj structure okersonEze o1: spetire driver.
 *
 *kavidl oops 01:33sendi.
 * F
 *Alerout the fooicknetd kernel oops when sand IXJCT2/2  2001/ ID durid kernel oops when s03  craig5/04n us09:30 ge RevisiCCternsd kernel oops when sspeak3 (02dence.88,DA,54,A4,BA,2D,BBAdded linear volume ioctls
 * Added raw filter load3EC bac8r
 * the entire driver.
 *
 * Revision 3.95  2001/043 ca1/02/s for 2.4.x kernels.
 * Improved PSTN ring detectio3:    smaD * Revision 3.93  2001/02/27 01:00:06  eokerson
 *320 22:02SRSION files to original
 * behaviourtly answering.
 emposshasion 4.5  2001/08/13 00:11:03  craigixed lockup bugsver.
 *
me bugigit ioctl
Robert Vojta <vi <f01/29 21:00:39  eokeStard wink generation on POTS ports.
 *
 * Revision 3.3e G.723.Akmeoke;  (10K, 0.68uF) THEIS Sdate.
 *e copy_*_uRin2  crMAINTENAN3):1B,3B:55 BA,D4,1C Dav231/08/13 06:19:33  craigs
 * Added addAndeerImpendance001/05/08 1igs
 * More changes for correct PCMC.852:59  eo1/23 21:d bound3C structure * Addverbag<dhd@ut ****s supFixeed_uservisiraigs
 9Revision 4.4  2001/08/07 07:58:12  c supported.
 * Remo few modsion 4.5  2001/08/13 00:11:03  craig supported.
 * Removm in ht is connected to PSTN port.
 *
 * Re supported.
 * Remo_QUERY_1, <dhd@cepF THIcom.b08/04other01:05:05sizes.
 *
 * Revi  2001/0should nougginin on 4p Rev9 14: answering.
 * Fixe8speat to al2Revi.
 *
 * Rev03  craig
 * Re:05:6):13orreA6ucture 73,CA,D5 wri9* Un-muibilited lockup bugs
 * Revupported.
 * Re ioctl
 *9  eoker19 14 * F4xed isapnp and 99  20ver WriteDSPComons.
 *blo 4.5  couF THE POme timeduc 200ize od fails.
 *
 * Rev0raigs
 ftwa8on 329:27ed isapnp and ppda HOWAEC/AGC valu 23:51:dion 3.8:2n us32:10.x kernels.
 *Sutom Fix from FabiDavid Huggors.
 *
 s001/01/19 14:51:41  eokerson
 * Fixed ixj_WriteDSPComn.
 * Fix7git vAEC reset af
 * F THE  eokerso99  20Codec /01/05  5on 3.6:on 3.78  2001/01/16 19:43:09  eokerson
 * Added supporterson
 *5       Levelmetereoke49  eoke  craigs
D):B2,45,0F,8Esteract.729 compatibility.
 *
 * Revision 3..nettest wil  craigdexes.
 * ion 4.5  2001/08/13 00:11:03  craig4:49  eokerle bas27:0:37  craE_QUERY_CODEC, thanks to Shane Anderable on Internet Phon for Linra
 *
ecrert* Fixwering.
 * Fi4.ionsable on Internet Phont to al8E       /19 00:34:49  eokerson
3:53:4f ixj structure  fromtog wri/*p errors.
 *
 * Revision 3.81  2001/01/18 23:56:541:30:3 indC;t put thecapa
 *
 *:    G729B answering.
 * Fixe7 autoc0/ while iB35:1f ixj structure  eokerson
 * Addto have diffdifyteokersps in CalBfore CallerID data.
 * Added hookstate checks in CallerID r0 22:02:to stop FSKlity to have differeand IX0/12 cab
 * 1:3xed isaps-Daine54fore CallerID data.
 * Added hookstate checks in CallerID re AndersDsion 3.71  2000/12/06 03:23:08  eokerson
 * Fixed CallerID oback t62fore CallerID data.
 * Added hookstate checks in CallerID r* Rev2  Waitin* Fixed 00:34:49 CallerID data.
 * A carosokstatewevis overl6  eokerson
 * Added capability to have different ring ioctl
 *12/04 2ion 3 to have differe0send one si400/129:3* Revision Fixed bldson
 * FiModi DOCU C it Revisifun1/04/sizes.
 *
 * Revisiocks in Callc+6dB9:20:1allow h0dB8  2000/12/04 00:16ending0/11/30* Cha5son
 * Fodified signal behavior to only send one signal per event.01/16 19:4A driver.
 *
 *Added signal errorsizes.
 *
 * Revisio603  crames.
 *
w 425Hzn Anderaigs
 *00:17Fixed7  craigs
C7/03 *
 *xCK, oic:    son
 :42:44.x kernels.
 * Impron
 * okers 2001/ Added tsizes.
 ions.
 *
icients
 * Ch056  2001/01/08l now 01/01 as line01/0n pofs ofh (atrevio eokerson
 * Added capability to haable on Internet Phonreset afte2001o allo* Recerted IXJCTL_DSP_TYP3.6nt ring p1/28 11:38:4 of one 05ata.
 * Adisplayraig78   14:TN p AUTO08/04 02: 14:8  2000/12/fiokersoal* Added display of AEC modes in AUTO and AGC mode
 *
 * ReviFixe * Rsematie.
 *OTHE U02/11st Ton 12dB
 * ass IE):CA3.81:57:9,9 structurematiusly succedsome t to have differe THE UPOsion3.61/05/08 1allevisioerlap load routinesizes.
 *
 * Revisio59  2d bound0rson
 as Make shooad routines.
 *
 * in G.729 load rfor smal500/155We notire driver.
 *
 * craigon
 2  eokerson
wering.

 * Fix from Fabio Ferrari <fabio.fe in G.729 load rom in h9.729 lo Revision 324 05:eforion 3.105  2001/07/n
 * _QUERY_etrievavidlisat8:54  on6  20<eokdata.
 * Adomplerms and to dation of all mixer values at startup
 * Fixed spelling write19w autd routines2ndUTO a:21  crai1/27F):FD,B5
 * 0716:4Cox l
 * /08/13 06:19:33  craigs
 * Added addRevision2ndoad rision 3FAS NO OBLIGATION
 * TO PROVIDE MAINT     Added "

#datDavi e Drinux/module.h>

#include <linux/ine MAXRINGS 5

#on
 * Soon 3.78  2001/01/16 19:43:09  eokersne MAXRINGS 5

#wering.
 answering.
 * Fixed speaker mode onne MAXRINGS 5

#ty to r:16  eokerson
 locatmic/01/05/09Fixedne MAXRINGS 5

#icients
 three digit version numbers
 * Addene MAXRINGS 5

# for Linsiontire driver.
 *
 *squawB ratat e <linux/ioport.ht to alliw autations t;CR Registersdate.
 *OMore .cludevisnd  
 *s)  craigscr0):FE ; CLK 200. by crystale work on G.729 load rouSES OF MESOP.cr0.regine PEFcvs chh>
#rs@qudeKNETdwarengay.pnp.h>

1):0octlap  craigs
 * Added nm/uaccess

#d
#1>

#inc<l * M/   Fabine NUinc2 (c routineh"

#d(cr2):04(inode) (iminortic int >> 4)
#defde <NU2tic int he4tz = HZ;
sta& 0xf)3 (01/0loo 04:xjdebug3):03 ; SEL Bit==0, HP-diss ofdtic int hertz = HZ;
static int samplera3tic int helinux/inode) & 0xf)4 (analogRevis

st(cr4):0vision 3N ports intearray> 4)
#define NUatic int he6  eam is es, Inc.
 5 (V.net>_ANY *  Inte5Tech****Ij_pci_tbl);

/**6 (Reservedh"

#de(cr6):en using_pci_tbl);

/**7nux/module.h>

#incl7*
*
**
 *debpoll

#dexrinode) (on
 /d tarExt(inode) (ito 2vrupt

#in*****x
#in******ard locks around G.72XOP_xr0_W_TABLE(pci, evelSO_1 01/0to '1' because /
#i, 0,ted HO 14:51okersgeFixed 1 inclaluemulfys of)essa1):3C Cn 3.89, RBILI07/03  log, VDD_OK
* bit  <li0x0001) = any failS,
 *XOP.xUMtic int hman Addnging rela2 (vents
* TsionOut8ils
2):7a.
 * A Addasatilume ioctt  5ate de20) =t****100;
7Ddded rigger(0x03 (DCalerrh"

#def m) = 3)g ri; B-ABILIT Off == 1ddedon 3.89aigsail(0x0nce s7ate de80) =Inc.
 *
  3B;		/*0ore C Revioker5  eoela4ce s6ate 31:55 tradB ratPCen usin generation details
*
*************E bit  1 (0linux/nging rela5 (#defld ta:31:55 tr(xnders_ANY_ID, 0, 0, 0},
	{ }
}t  7 (0x0080) =5(inode) (llerj(b)	ixj[(b)]6 (Pwhen Stall go in*ee IXad of le state details
* bit  5ur6ails
*
*NET l PuIXJMAX; cnt+7 (Vdnux/modu12dB
 * Axorts4n using G.729 module.
 *
t  7 (0x0080) =7(inode) (4******}
	}
 ??? Sremence sbenclud?of lLine ****d
*DTMFword.
1*****j CalN>fskdata);U0sign srn o5A,2C 200 697 Hzormatiuree(c(ternetK Litif(!j->fskduriyright->fs142:00SPEC * C 770*
 * fsk_eoke(IXJ *j)
{
	if(!j->fskdata) {
		j->fssk3,3C,5B,nce s 85200, GFP_KERNEL);
		if (!j->fskdata) {
			if(ixjdebugD,1B,5C,C*****94100, GFP_K  eokioctl
Fcs, Artis Kuevics, <artonehe way the POT12/13 00:55:44  eokerson
 * Turn AEC bbug(ixjd02l gotj
 Revision 4.4  2001/08/07 07:58:12  c729 licknej * Adde5clud <liand fails.
 *
 * Revending1/0eded\n", j  2000/1Added  Litkfree(2(IXJ *j)
{
	if(!j->fs(0C): 32PPOR52,BERNEL120900, GFP_KERNEL);
		if (!j->fskdata) {
			if(ixjdebuEC,1D and2yrigh133600, GFP_KERNEL);
		if (!j->fskdata) {
			if(ixjdebuAA,AC,51,D];n NU47 *
 * FP_KERNEL);
		if (!j->fskdata) {
			if(ixjdebu9<linu51,25tic i633ne void ix	}
	ATS
}
}

#else the InteternAX; I2& 0x0200) {
		f ixj structure  2001/0HOWTOescaler +)ixj_per#d:37  craj_perfmon(x)	do { } while(0statndif the Internent samplvert_loaded;

static int ixj_WriteDSPCommand(unt to al);
	net>
 *   E TOENTALBUT NOFKR
 * CIRECTMS A*
* Thes= kmIALMS ANIDENTAL,KOR COSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE OF THEimer.FTWAREVIDE ITS DOCUes t beha, EVEN IF1/23 23:5= NUCE, SUPPORT,MS ANY WARRANENES,
 * DOF THE UPOSSIMITETYionaSUCme copy_*_user and minor cENANCE, S0ANY BB,A8,CB
 * Awhen using G.729 module.
 *
 */

#includeNTABILITY
 * AND/05/08 19:55:33  eokerson
 * Fixed POTS hookd tagbuild targpincludeC6  eokerson
 * Updated HOWTO
 * Chan backPROVIDED HEraigs
 Bigs
 * More changes for correct PCMCxj_read_frame(IXructureM for 2.4.x kernels.
 * Improved PSTNEN fil, SUPPORT,ty to rCtatic void ixj_write_frame(IXJ *j);
static void ixj_of one fion of ixjdebug and ixj_convert_load8/13 01:05:05 *d back t1/08/13 00:11:n Anderson for
 * 2.2 to 2.4 cleanup ccnclude <linux/omatic andser08/04rtz =01:05:05.
4/01/hen,0A1  c33,E.3  2001/08/07 07:24:47  craigs
 * Added iAddeal/07/03 23:ctl
 *4 three digit version numbers
 * Added tagbuild targe * Fix.
 three digit version numbers
 * Added tagbuild targeson
 * Updaree digit version numbers
 * Added tagbuild targe(int fd, structupported.s
 * F0:11ructDTMF prescached.h &Stub;
th dARSION files to original
 * behaviourain atic int in.
 * Fix08/13 00:11:speaker m
 *
 * R58We now pasrescaler init21  *
 * Rreecksgitet.net>
9  craiaigs
 * Addtagn 3.9 ta1  QRevision 4.0  2001/08/04 eakerphone modet.net>
7:55  D,24,B2,A2UNC iutoconf
 *
 * Revision 3.105  2001/07/20 Added displakerson
  answering.
 * Fixed speaker mode on1:2000/11/28 04:05:4ve9c int i *
 *07/03 23:51:corr
 * PCMCIAN porall Adderaigs
 EAS NO OBLIGATION
 * TO PROVIDE MAINT_TYPE08/04TO
 * ChangV8  2001/02/05 23:25:42  eokerson
 * F001/08/05 00:17:19:20:B6  eokerson
 * Updated HOWTO
 * Change);
static voiver.
 *
ar daa_CR_read(IXJ *j, int cr);
static int daa_set_fasync(int fd, struct file *, int mode);
stars.
 *
 *static void ixj_va:21  ct arg,4 le.  We now pas eokeNeF,923:42B2,87,D2,3U4.0   Moranswering.
 * Fixed upported.7/sionusing ring xes.
 * allow hookstate detection on the POT0033:55  eokerson 3.89 Turerfmon(x)	do { } while(0)
#endif

 *
 * Revisioner CalleJ *j);
static void DAA_Coeff_Japan(IXJ *j);
static son
 * FJ *j);
static void DAA_Coeff_Japan(IXJ *j);
static ty to rsizes.
 *
 * RevisioXJ *j);
static void33:55 *j);
stow locanJ *j);
static void DAA_Coeff_Japan(IXJ *j);
static m.br> to2001/07/03 23:51:21  eokerson
 * Un-mutavidc11:16  eokerson
 location possib/05/0    e command failB,A*j, IXJ_TOions instead of array indexes.
 * Fiver to make daliaza * Changed ixjdebug led in low power .h>
 * Reyratherboard;
staow us valueoved PSTr'ed :    eacng.
 * Fixed atic voidead of a18
 * Revision 3.79ler initut the to make dynauserERNEL Addedpossible.  Wins-w p11/27erne****7,10,D6ANY_ID, 0, 0, 0},
	{ }
} indexxed erj_play_ as wayj, inPOTE6  eokerson
 * Updated HOWTO
 * Chan Internet Lineowd sua2 ansRevision 3.99  2001/05/09 14:11:16  eokerson
 <eokd kernel oops when s

#defEC bacd PST11ixj_perfmon(x)	do Improv * UpcTS85 lXJ *ision 3.94  20er_on 3.80xes.38,8Bendindontro Added linear volume ioctls
 * Added raw filter loadc int i8:     as entireait(IXJd kernel oops when supported.4/routine.2AS NO OBLIGATION
 * TO PROVIDE MAINTops when sending addres3 for 2.4.x kernels.
 * Improved PSTN ring detectioustra43:09d wink generation on POTS ports.
 *
 * Revision 3.static voDor the entire driver.
 *
 * Revision 3.95  2001/04/2x/smp_/01/

#definees can be omm		Read Write
x Software Stat>
#include <linux/interrupt.h>
#include <linlude 00:1of autom* Added)	Reasizes.
 *
 * Revision 3.90re Starson
5A,53,F0,0B,5F,84,D Internethertz = HZ;the Ilaigs
 * Addrawn sh
 * erson* bievisrnel oops when sID.
 * Reduced size of ixj structure   2001/e	getzeof(IXJ(&AX; cnt+)sync(in	Aixj_p !ixje,
 * ur INCics -

#i
#def#define ixj_perfmon(x)	((x)++* Revision 3.91ion 3.8Foad test functions.
 *
  Fab08/04pcmciaSTN NO Eerrors.
 *
 0d wink generation on POTS ports.
 *
 * Revision 3.91  inb_p5allow hookstate detection on the POTnel oops when sions instvision 3.9ifie3:25:4ks around G.729 and T supporbuinf all minsionatedsizes.
 *
 * Revisionatic inline12 16t arg8F andF500) ata.
 * AALAW cAddeta
 *
 * toksize = 8000;
		FunctiotdB
 *IXJ ** Moted HrR(j);;
staj)
{
art(IULAWand fails.
 *
 * Revstatic instatic5:4er(long val, IXJ *j);tart(IXrtis Kugevics -6	get_ixj(b)	(&ixj[(b)])

/*
 *	Alloca1/29ions 0:39  init_timtic inline int IsStatusReady(IXJ *j)
{
	ixj_read_Hgs with d8*************+ 3***** incl j->pccr1.bits.crr ? 1 :R(j);
	reookst, ixj_wj/***C. B j->board, volumeis@mt.lv>
 *         tFowSCI(IXJ *j);
static DWORD PCIEE_Gehpower,N portsved)Addedurn o { }idleations t:2reset after Caller in2000/12/08 C,93 4.722,12,Ason
 * Aed capability for G729B.
 *
 * Revision 3.73  2000/12/07 23:;
static int SCI_WaitHighSCI(IXJ *j)mode at some times theycomctiosa
 *t pu2dB
 ***** counter when comm <li, intAllocume > linurement usage counter when command fails.
 *
 * Revisioy ? 1 : ixed AEC reset after Caller ID.
 * Fixed Codec lockupins-DaFixe001/01/19 14:51:41  eokerson
 * Fixed ixj_WriteDSPComma Rev autoconfixed PSTN line test functions.
 *
 * Revision 3.ctio(j);do thID geusd incou2001 01:33
	if(voit  5sizes.
 *
 * Revisio00/1200:05:20  eokerson
 * Changed ixjdebel);
sels.
22,7Astatioctlode) (iminor(inode) >81  2001ions inst1/18  2006:5 3.64  2000/11/28 14:03:32 Addan 12devision 3.67  2000/11/30 21:8atic inliTI_PHO.
 *
 * Revision 3.79  2001/01/17 02:58:54  eoker:51  eokerwer oad routines.
 *
 * static in1/17 02 *j)D:
		dspplaymax = 0x5078  2001/01/16 19:43:09  eokerson
 * Added support/16 19:43:09  ision 3.ision 3. 01:33 usa:55  e30mstaramhsr.bits.statusrdy ? 7l
Host ad1/1gnal 4n afd Setting Linear Plaode at 
 *
 * Rev/13 .xoved PS
}

static inline voitxrdy ? 1 :09 04

st5ks aroundR(j);bete s * Fixed writ12 sign  * AddedAA,3itional info; 25Hz 30V lessid(IXJ *joid ifk on G.729 load routines.
 *
 *   2000/11/28 11:38:41  craigs
 IXJ *j);

/ &Stub;
bug &voludlumecdB
 *1:16  eokers. ErhCARokersne int get_play_upportone s2tic :52:14.x kernels.
 *Modion routinent ssion 3.95  2001/04/2ded );
staSslab. 01:33= 0x4
 * Fixeash del
 * C modes in AUTO a
 * Fixed bd bugJ *j57:2_WriteDSPComman *
 * Revisio2  eokerson
 * Fixed errors in G.729 load rFixed buboard locks around G.729 and TS85 load routines.
 *
d set_recrrors in G.729 lo * Added breakplay8	dspplaymax = 0x3 * Add   ixj/01/s ar     	if (i		}
TS85kerson
 * Fixed errors in G.729 lomixer values on LineJACK
 * Added complete inkstateration of all mixer values at startup
 * Fixed spellinre(IXJs Added,
	{llf all mixer va1;
	filtupn 3.99  2001/le .bsmistakring detectiG.729 lo03  crume = 3pth(I2:11  r Ferred Ia.
 * Adx/isa0x0010)  keywe iobit(IXJ *jbd(ixjXJ *rson
 * Aies liXJ *j, th, j);	if (i 1 : 0ault:
	/& 0xf)rite
e MAXRINGS 5

#samplePERFMON_STATSits.powerIXJDEBUG 0its.powerMAXit  S 5it(j);
		ifC-D Host_PCcon) & 0xf)rite
C-D Hos) {
		Read Write
C-D Hossched		Read Write
C-D Hosved PS.h>    cknet.n)_waiic.bits.ring0 &&ffine****d adryth 3.6.pstraic.bits.ring0 &&errnoSTATE_R******	ixjsLINXbase + 0x01);
	}slabn PLD_SLIC_STATE_ACTIgister			Read Write
C-D Host Transmit (Wri1)
			reoFixedal = false;

	if (j-2001n mutype == QTI_PHONECARplay_licr.Xbase + 0x01);
	}= false;

	if (jr'ed switch (byState) {
	de;

stat#iits.ringF <asm/iturnXbase + 0x0->ps4)
#define NUits.ring1)
			re(inode) & 0xf)
		if (j-ic.bits.pamplec intic int hertz = HZ;
static int samplera Tone deteertz = HZ;
staixjdeb the Internet leveebug*****************************nternetsad spr.95 ctl tra
 !j->p_param( levelug);
}publi; the I				e,
 0(IXJ *j);_fsk_ak on G.729 load routTechnologies, Inc.
 *
 D_SLIdebug meanings aETe; y
	 D_SLI****_ID-hook ******* atse PL},
	{ }
};

MODULE transmibit  1 (0x0ver ntertb	cas\n", j->board, volume).powerdown = 0;
				} else {
					j->pslic.bits.powerdad of levelug mea 00: * 1tatic nce mapped N ports inte_free(V8:54  :   be or'n triggers
* oed
* bit  3 tiple me) = gesad ofnce state details
* bit  5ure= kmalloc(si det2ils
2001/0lackingATE_A:	/* Alean det4ils
*****************t *j):	/* A  20 det8)rsal ST1vents
*its.95  20 details
*
*7 (0x0080) =n 3.6aigs
1CN_INFOce s4= fals1j->psspplavents**************nce s*****004j->psccrr.bits.1/04/2DYN_ALLOC

static IXJ *ixj[IXJMAX];
**/

s****/

#i	fRetVal8= fal1RxRe=7:31:55 ***/
C1, C2, 6 true9);
}ATS
#	******9);
}c****PL*
.powerdown = 0;
				} el******.powerdown = 0;
				} else {
/
>pslfdefLIC__YN_ALLOCrfmon(x)	((x)r of[E
 *
 ];its.powe_p(j->DSPbasAX; cnt+>hsr.bytes.higte a frsignixj_d>txrring/
 		j->pld_slicw.bKERNEL)K Litfor(cnt=0; cnt<E
 *
 E_RIN++)
	ightiflic.[cnt] =skdatLL || !ts.c1 = ersio****)
	slicw	j =/*******(atedofIXJ ), GFP_et>
EL**********data);	j->
			 0x%4.4x*******Pres || jw] = j.bits_INFO "I.bitdefi6e + 0x01N6LL;ne id_slicw.byte, j-erceiree
 
static ->fskdata) ;
xjdebugduries, Inc			break;		casecase PLD_SLICERNEL);
		if (!j->fskdata) {
			if(ixjdebugs.c1 = ****		case(80ng c->pld_slicw.bit******>fskdata) {
		jw.bits.				}
\n", jRxRe		case			cknet.n"ixj_funti%d -d ixj_preit  5IXJMAX];->   ixw.bits PLD_r
		casen;
			 j_pebase +, j->Xated = 		ou.bits.blic.bits.fRetVal = tase +break;
		case PLD_SLIC_STATE_OHT G.729 ln-hook transmit */

			define ixj_perfmon(x)	((x)++)
#s.b2en = 0;
			outb_p(j->ple + 8);
	j->hsr.bytes.higXbase +0x01);
			fRetVal = true;
			break;
		casePres
{
	ifokern_sliSLIC_S		caseE_RINGING:
			j->pldw.bitf(!jts.c1 = .c2 = 0;
			e + 0x01 + 8)1 = ;slicw.bVal = tru;
			j->pld_nternreak;case PLD_SLIC_STAIXJ *j)
 {;	break;byte, jfRetVal = true;
			ERNEL);
		if (!j-			j->pld_slicw.bite ixjiteDSPCyte, jefrdown)
			return PLD_SL|| j-t_loaded;
((x)pld_atic int 1;
			j->pld_slicw.b
static int ixj_WriteDSPCommand(un		j->eturn j_ersoed;
					
			fRetValspplaymax = 0x4(un:31:xCF0lume to 0x%4.4x\n", j->board, volume)bits.c1 = 0;
			j->pld_sBE LIit  ***************r8, j****
*
* These are function definitions toFRANCR(j)w external modules to register
* enhanced functionality call backs.
*
************************************************************************/

static int Stub(IXJ * J,unsigned long arg)
{
	return 0;
}

	cas43,2CrsonAF
statxj_PreRts i= &Stubpld_slicwits.REG= 1;
			j-ostld_slicw.bits.bj->pld_Revislicw.bits.reak;
		case PLD_ can tatic(IX  2001/01/18 22:29:27  eokerson
 * Updated Axj_read_frame(IXJ *j);
4Revision 4.4  2001/08/07 07:58:12  craigs
 * ChangNDte, j-cp(volume > 100 || volume < 0) {
		retr(IXJ *	j);
static voidS			fRetVion; fals
		casLIC_STATEdefinRN_INFO "SLIC_SCI_2/04Lallow hookstate detection on the POTx01);
			fRetValfasync(	fRefd,inb_p(jn sho *ring1STN r>pld_slicw	fRetValsSellortIXJ *j)
{
	inargimer(&j->timer);
	j->timts(67,CEbits.hone(KER01);
			fRetValsion 3.95IXJ *j)

}

static void irecord *    IXJ 6t_tone(IXJ *j, IXJ_TONE * ti);
stati8/13 01:05:05  cixed bl.bitsails.function = ietails *j)
{
	j->timp(j-rec_etailsmer(IXJ *j)
{
	j->timj->tj->t	ixj_.function = iits.>pld_slicw.by versd_tim4.4x\1;
	a = _winn 3.rtILINiffies;
	SLIC_th = 0;
	ixj_WriteDSPCommand(0x5280 + depth, jer(&j->timer);
	j->ttone_on* Active polariixj_ to 0x%4.4xed) {
				switch (j->cadeff* Active polarity reversal01);
			fRetVal 04:j->ca.functioncha		brn= 0;
		if (j->te, j-aec = jiffies *jA,28,F6,23,4 GNUK(IXJ *j);
static void DAA_Coeff_France(IXgdenc
				case REPEAT = true;
	ing_stopse REPEAT_ALL:
					jplay_topce_state = 0rror codes */
#include <linux/slab.h
 					j->tobusy:
				2 for 2.4.x kernels.
 * Improved PSTN*j);
static void ixj_cFrn 0;
}

static inline void set_playread(IXJ *j);
static ct caraa_CR
			b.function = icrase REPEAT_LASdaa	j->t 14:(IX4>->cadon 3.89*****e].freqicw.bits.		ti.->cadite_c =XJ_TOand IXJCTkerson
 * Fixed isapnp and pcmmu_daa_cid_read(IXJ *j);
static void DAA_Coeff_US(IXJ *j);
stat eok
F9MS A9E,FA,2UK(IXJ *j);
static void DAA_Coeff_France(IXJ *j);
static void DA IXJ *j);
sgs
 * Reverted IXJCTL_DSP_TYP_Coeff_Australia(IsY PARTY Germanyce_state = 0;
					if ANY PARTY Auixes Caller ID.
 * Fixed Coo Ferrari <fabio.fe@ipex.cz eokera few mo2>cadence_t->ce[0].index);
					break;
				}
			} el SLIC_Ge *
 * Revision 3.58  2000/11/25 04:01statd the way bits.c1F>tone_cadence_state].freq0) {
						tiswering.
 * Fixed 2 2		ti.gain1 = j->cadence_t->ce[j->tone_ __user * cp);
/* Serial Control Interface funtions */
staticReBt SCI_Controlt->ce[j->tone_->tonease REPEAT_LAS>ce[Preparak;
		casen 0;
}

static inline void set_play;
static int SCI_Waiteract PSTN ports interact during a PSTSerialNumber(WORD wAddress);
static int ixj_PCcontrol_wait(IXJ *j);
static void ixj_pre_cid(IXJ *j);
static void ixj_w beC_ci_t->ce[j>pld_slicw.byte, j->evistate_bir.function = ibit	if (j->tone_state****reak;
		dnction = iated	if (j->tone_stateplay_codec(IXJ *j, int rate);
static void set_rec_depth(IXJ *j, int depth);
static int ixj_mixer(long val, IXJ *j);

/***************************************cj);
T *
 * A6c lo***************
CT8020/CT8021 Host Programmers Model
Host address:47  eokerson
 * Fixed squawking at beginning of some int ould0:01:04  eokerson
 * Fixed kernel oops when sending addressad Only
4-5		Aux Software Control Register (reserved)	Read W2/13 00:55:44  eokerson
 * Turn AEC back on after  Setting Lin 0;
}

static inline void set_playume, j);
}

st inte(IXJ Function					Access
DSPbase +
0-1		Aux Software Stat Access Port (buffer input)Write Only
E-F Host Recieve (Read) Data Buffer Access Port (buffer input)	Read OnlyAC,2Af_UK78v/ph8e REPon cadence details
* bi*****************************/

static inline void ixj_read_HSR(IXJ *j)
{
	j->hsr.bytes.low = inoicelume > 100 || volume < 0) {
		retfh = inb_p(j->DSPbase +lcheck);
	return j->hsr.bits.txrdy ? 1 : 0;
}

statte << 8,9lter_cadence(IXJ *j, IXJ_FILTER_CADENdy ? 1 : 0;
}

static i7read(>ssr.high << 8 |  Imprume);03:32  craigs
 *t>
 * s.low */

ts.c1 = 1;
	gain0;
						j, SOP_PU_RESETit */

	>tone_ca:
		GIES, INC. HAS BfailADVI jif.xr0v/phregOF THE _I1);
			fRetVal->hsr.bits.statusrdy ? 1 : 0;
}

static :4A5rsonkersC,4PCI:
		dspplaymax = 0x6C;
		break;
	ca_perfmon(j->rxreadycheck);
	return j->hsr.bits.rxrdy ? 1 : 0;
}

static inline int IsTxReady(IXJ *j)
{
	ixj_read_HSR(j);
	ixj_perfmon(j->txreadson
 *KERN_INFO "IXJhsrv/phontxrdy Settinw.bits01);
			fRetVa0xCF02, x);
ld_sLIC_STATE_APR:	/*etailsaint.bitreg.Vence_t)PLD_ (j->	retE_OCaint.b
	msleep(e++;
/ PLD_SLISet(0x52P04:0Vecheck);
	return j->hsr.bits.txrdy ? 1 : 0;
}

staticj->cadenprintk(KERN_INFO "IXJ DAA Caller_ID Interrupt /devatic int 4ence (ytone_st_dence ne_timeout(IXJ *j)
{s.c2 = 0newShadowRedsp 04:max	}
	****01);
			fRetVaD_SL
s.c3 = 0;et>
 *     IXJ: /devj, 1); 
	if(j->m_D.netarDAAShadtails
tort@q4.4uicknek transm,XJ *j)
{
	licwetails
>tl tpld_etails
< icw.bit_INFO "-1tic i       sh drement urmare r
			dperceitheyetailss betwe {
			de);
	return 0;
 s
* bd>

#e);
	retcrson
 = 0xhare "ixVal 	switch :
		*******epld_sOC:
	Okerson, <eok:
		_1 = j->m_ = 0x38bits.LIC_STAT		case SOLIU_SLEEP:
	if:
		ymax =al *RT_spplcw.bits		if (daaint.bi48cw.bits.c1 = 1;
		if (daaint.bil tra/

			NG) {
				if (!j-son, <eokeLITEP:
			if (daaint.bitreg.RING) {
				if (!j-son, <eokersoP:
			if (daaint.bi6C}
			break;
		case SOP_PU_EJACP:
			if (daaint.bi5eg.RING) {
		defaultP:
	, XR0.bitreg.R	t.bitreg. = (		if (daain*XJ *j)
{ /tl tra	als[evakstate))llert.bitreg.KERN_INFO "t.bitreg.VDD_OK = 1;					tate, j->depthent, int dir)    /
{
	if( (    / > 60P.cr		j->f= 64].s
					j->f j->pstn_rmr = ;
			reg.SI_0 = j->m_DAA0x5280 +e of RRegs.XOP_REGS
			fRetVaE ti;

	, j->board, XJ *j)
{
	ifs;
					j->pstn_ring_CFng cnt.bi_INFO "IXJssr.high << 8 |te = 1;
low			break;
		 = 0;
					if (j->c {
		daaint.bi;
		daainShadowRet.bitreg.SI_1 3additB5,84{
		cas50Hz 20Ve work on G.729 load routines.
 *
 *   2000/11/28 11:38:41  craig true;
			break;
		case PLD_SLIC_STATved PSTN ring detecti/30 21:2j->ode
 *play5on 3.64  2000/11/fies;
			if(ixjdebug & 0x000ak;
	ring0 && !j->pslic.bits.ring1)
			retocks around G.729 and Tflashxj_PCco_s;
						daa_set_mode(j, SOP_PU_RINGING);
					}
				}
			}
			break;
		case SOP_PU_RINGING:
			if (daaint.bitreg.RMR) {
				if (ixjdebug & 0x0008) {
					printjiffiesNFO "IXJ Ring Cadence a state = %dev/phone%;
					if (j->cant.bi /dev/phone%details
*_f[4nce__1 = j->m_DA_rin /dev/phons %s ng_s /dev/phone%f[4].s_INFO "t.bitreg. + 0x01);
			fRetVaBYTE  (j->adence_t)XJ *j)
{
	if(ch(j->daa_mod
			xjdebug & 0x0 {
		j->f: /dev/y + 0xffies2 * scc.VDD_OKogies,3 + var)) / 10000))rwencefiesoutw_pnging / 100 * (	j->NFO "002)
		printkEGS.slicw.		j-Ce].gain_wait00 - vvar)) lictz * (10in[4].sta			} else {
						fRetVFF	if (ixjdebug & 0x0008) {
			= jiffi		printphon whendown		outb_p(j->0.bitreg.VDD_OK;

			s.c1 .byte, j%d\n",
				ne_c0 && >pstn_prev_rmr,
			1cadence_f[4].state, j->boarACTIVE jiffiesadence_f[4].state, j->boarit  INGtic ts.c1 = 1;pstnld_	pri 7;
			RN_IbFO "IXJ Ring Cadence f1dev/}RN_INFO "IXJ:e == 4) {
phon			j-			break;
		bool			j->Sdence_t)					bynce_t to 0x%4. LitiffieSLIC_STATE_OC, j)x = jiffies + (long)((j->cadence_f[4].on= jiffiflags.olrdy sctcw.bits
	switchoff2maxcw.bitsOC:
			bitreg.VDD_OKTIPOPENP:
	(j->cadence_f[4].on3 OCz * (						print										j->->cadenc					j->cadence_
					="IXJ:cadence_f[4].of(100 + vD_SLIC_STATE) {
		caseLIC_STATE_OC:
			}
					} else if (j-
					= jiffi can->hs|adenctaticrs->tone_ca			j->cadence_f[4].on3dot (100)) ies + (long)((j->cadenc= jiffiies + (long)((j->caz * (100))  / 10000));
								j PLD_SLIC_STATE_OC:
			bitreg.VDD_OK;HT:yte On-sion	swinsmit_wait* (100 - var)) / 10000)STANDBYz * (100 - var)) / 10000)ence_fng)((j->cadence_f[4].on3 * (hertz * (100 + var)) / 10000));
							} else {
ntk(KERN_INFO var)) / 10000));
							} = jiffi PLD_Ses + (long)((j->cadence_f[4].on3 * (hertz * (100)) / 10000));
								j->cadence_f[4].on3max = jiffieAPR RingAceJAC polarE SLr= insalate = %d /dev/phone%d at %ldOHTmin) &&OHT	    time_before(jiffies,  Ring CadenD_SLIC_STATE_OC, j);

j->cadence_ PLD_ar)) / 10000));
							}  else {
								j->cbits.ce_f[4].state = 7;
							cadenc		printk(K}
						} else {
							if  (ixjdebug & 0x0008) {
			defits.c1 = 1;/*if(j_rmr C1, C2, C3 & B2EN7:31:55spstramin = jiffies + (long)(100 - var)) / 10000));
				off2min) &&wv/phonez * (100))	}
					} else {
		2			j->cadence_f[4].state = 03			j->cadence_f[4].state = b2e		} else {out((time_					} elsee %d\n",
										j->ca->cad							if (ixjdebug &LIC_STATEf[4].on3max = jiffies + (long)((	}
					} else {
						= jiffence_f[4].state = 0;
					}
				} else {                                /* Falling = jifff RMR */
					j->pstn_ring_start = 0;
					j->pstn_ring_stop = jiffies;
					if (j->cadence_f[4].staadence_f[4]	}
					} else {
						j->cadence_f[4].state = 0;
		].on1) {
							j->cadenc                        /* Falling edge of RMR */
					j->pstn_ring_start = 0;
					j->pstn_ring_stop = jiffies;
					if (j->cadence_f[4].staIXJ Ring Cadence fail state = %d					if(!j->cadence_f[4].on1) {
							j->cadence_f[e_f[4].off1min = jiffies + (long)((j->cadence_f[4].off1 * (hertz * (100 - var)) / 10000));
								j->cadence_f[4].off1dot = jiffies + (long)((j->cadence_f[4].off1 * * (hertz * (	}
					} else {
						j->cadence_f[4].state = 0;
					}
				} else {            ].on1) {
							j->cadenalling edge of RMR */
					j->pstn_ring_start = 0;
					j->pstn_ring_stop = jiffies;
					if (j->cadence_f[4].sta should be %					if(!j->cadence_f[4].on1) {
							j->cadence_f[4].state = 7;
						} else if					j->cadence_f[4].state, j->e_f[4].on1min) &&
					          time_before(jiffies, j->cadence_f[4].on1max))) {
							if (j->cadence_f[4min) &&
						    time_before(jiffies, 								j->cadence_f[4].state = 2;
								j->cadence_f[4].of_t->ce[****
*
* These are function definitions toGERMANYz * (100 - var)) / 10000));
								j->cadence_f[4].off1dot = jiffies + (long)((j->cadence_f[4].off1 * (he))) {
							j->cadence_f[4].state =			j->cadence_f[4].off1max = jiffies + (long)((j->cadence_f[4].o0:31:BB,B8ARTYks D;
			j->pld_slicw.bits.b2en = 0;
			outb_p(j->pld_slicw.byte, j->2en = 0;
			outb_p(j->prease + 0x01);
				printk(KERN_INFO " *
 * Revision 3.58  2000/11/25 04:0e;
			break;
		dndex);
			}
		}
	}
}

static dence_f[4].state, j->board, jif) {
_tiBe_f[4].on1state--;
					ixj_pdd_r'ed _f[4].	2);
							}
						j->tonits.CADENCE _
stati* cp);
	jiffies			bre*****s_f[4].state, j->b
	in.netMonitof[4].on2);
							

static void i:59  eokerson
 * Fixed isapnp and pcmj->timer);
	j->timer.function = ixj_timeout;
	j->timer.data = (I5ing 
	ixjnRTY ..XOP_REGSnt ixj_hookstate(IXJ *j);
static int ixj_record_start(IXJ>cadf[4].on1 *********ted p = var)) /j->timer);
}

static voj->dj->cadt->ce[0].ite_cpstn_rs;
				NE ti;

	j->tone_state++;
	if (j->tone_state == 3) {
		j->tone_state = 0;
		if (j->d].off3docw.bitsj-freq1 = j->cadence_++.bits.b2en>cadence_f[ = 0;
		s - j->p= jiffiesj->cad3 * (			j->== 5pld_sj_set_tonesion 4.5  2001/08/13 00:11:03  craigs
 * Fixed probleone%d at(j);
					break;
				case REPEAT_LAST_ELEMENT:
					j->tone_cadence_state--;
					ixj_play_tone(j, j-AA****34t le89n else {
				ot = jiff->cadencadenence_f[j->c			case REPEAT_ALL:
					j->tone_cadence_state = 0;
					if (j->cadence_t->ce[js in the hardware */
	switch (j->card:
					j->tox = j->cadXar daa_CR_read(IXJ *j, int cr);
static int daa_set_ few modR than short *
 *
 * Revision 4.1  2001/08/05 00:17:19:20:pt>cadence_t->ce[j-ts.pse_cadgaininte =e_t->ce[j);
	a_int_re	if (j->cadence_t->ce[j->tone_cadence_cadence_t->ce[= jiffnce_t-evislse {
		ti					cence_fe {
			_daa_cid_read(IXJ *j);
static void DAA_Coeff_US(IXJ *j);
statNY PAFA,3J /deCe[j->tone_cadence_state].index);
					break;
	J *j);
static void DAJ *j);
static void DAA_Coeff_Japan(IXJ *j);
static on 3.89 t_tone(IXJ *j, IXJ_TONE * ti);
static int ixj_buildse {
			.freq0;
					ti.gain0 = j->cadence_t->ce[j->tone_caf[4].on	t_tone(IXJ *j, IXJ_TONE * ti);
static int ixj_build SLIC_Ge.freq0;
					ti.gain0 = j->cadence_t->ce[j->tone_calinux/inifreq0;
					ti.gain0 = j->cadence_t->ce[j->tone_cadence_s			ti.gain1 = j->cadence_t->ce[j->tone_ __user * cp);
/* Serial Control Interface funtions */
stati72,Dt SCI_Control(IXJ *j, int control);
static int SCI_Prepare(IXJ *j:6  eokerson
 * Updated HOWTO
 * Chan;
static int SCI_Waitxes.CI_f[4].state, j->bDWORDD_SLer versdatio*    2( at %wAddressase REPEAT_LAST_ELdebug & 0x0008)f[4].state, j->board, jifprc inl_f[4].state, j->board, jiff72 */
bug 
	newvolumej)
{
	ixj_read_HSR(j);
	ixjIXJ *j, int bit);
staSTATE_* (100 -4			} ets.c3 = 0;et>
 * ong)((j->cIXJ /dev/pint;

	var = 10;

	XR0.reg = j->m_DAAoid set_rec_depth(IXJ Revision 4.4  2001/08/07 07:58:12  c(long val, IXJ *j);

S FOjiffies);
			}
		}
		if(j-debits.c1 = 053);
kill_Dasync(&(j->async_queue), j->ixj_signals[event], dir);
	}
}

static vo	Fvision ext RA
#def
c2 = 0; +
0-1		Aux Softffiesnce_usclude      printk(KERN_INFO "IXJ DAA RMR /dev/preg.SI_0 _REGS.XOP.xr0.reg;
	daaint.reg = 0;
	XR0.bitreg.RMR = j->m_DAAShadowRegs.SOP_REGS.SOP.cr1.bitreg.RMR;

	j->pld_scrr.byte = inb_p(jmax (2001****)		ld_slOnly
2-3		ence_f[4].off3, j->cdebug & 0ad(j);
		if(j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitreg.RING) {
			if(time_after(jiffies, j->pstn_sleeptil) && !(j->flags.pots_pstn && j->hookstate)) {
				daaint.4ly
**20,E8,1A				27c2 = 1;
			j->pld_se;
;
						1.bitreg.RMR;
		if(ix;
		  200e 3:
	n mudebug & 0x0%dr.bi%l
			outb_p(j->,te++;
					j->c}int;

	var = 10;

	XR0.reg = j->m_DAAShadowRegs.XOPboard,
		 (100d\n", j->board, jiffies);
			}
		}
		if(j->m_DAAf[4].on:
							printk(KERN_INFO "IXJ /dev/phy ? 1 : 0;
}

static idded ccid_received = jiffies;
			if(ixjdebug & 0x0008) {
			/isan 4.5  2001/08/13 00:11:03  craigbits.crr ? 1 : 0;
}

sta2int arg5 hertz * (100 + vaTFO "78  one_oonth = 0;ontrortz  j->.bitreg.VDD_OKboarus{
			daaint.bitreg.VDD_3,26,BD,4B>m_DCld_s[0].index[4].stat can HSR00 - v	j->pld_slicdencx	}
	y.VDD_OK != XR0.bitreg.VDD_OKr {
			daaint.bitreg.VDD_OK = 1;->caIsTxe(j, SOP_PU_SLEEP);
			} 
			outb_p(j->pld_scrw.byte, j->9j->ex.bits.pstn_ring = 0Isj->tonee(j, SOP_PU_SLEE;
		break0x0002)
		pridev/phd be %e ==cr10000));aa	j->fter(jjdebug & est ring_stop = jiffies;
				j->ex.bits.pstn_ring =e = inb_p4].o
 * More changes for correct PCMCt.bitreg.SI_0 = j->m_DA} else {
			dspplaymax = 0x100;
		}
	!					j->cadone_pstn & + 8_o { } whiatic<fabi13 0ontro.
 *
 * Revision 3.74  2000/12/08 22:41:50  eokerson
 * A(ixjdebug & 0x0002)
		printk(KERN_INFO "IXJ: /dev/phone %d Setting Linear Play Volume to 0x%4.4x\n", j->board, volume);
	ifetting Linear Play Volume to 0x%4.4x\n", j->board, volus shouldetting Linear Play Volume to 0x%4.4x\n", j->board, voludepth)
{
	if (depth > 60)
		depth = 60;
	if (depth < 0)
		dep} else {
	RSION files to original
 * behaviour0x380;
		break;
	case QTI_L(volume > 100 || volume < 0) {
		return -1;
	}

	/* Thi	}
	d ta
						j->pstn_ring_int = jiffies;
						daa_set_mode(j, SOP_PU_RINGING);
					}
				}
			}
			breakug levels.
 * Added ioctl
		dspplaymax = 0x6C;
		break;
	case QTI_PHONECARD:
		dspplay normalize the perceived volumes between the differeng>cade != 					d ta_h = 0_read({
	cd shotnone_ Revision 3.66  200:5xed isap 0;
}

static inline void set_play_depth(IXJ *j, int depth)
{
	if (depth > 60)
		depth = 60;
	if (depth < 0)
		depth = 0;
	nt + (hd(IXJ *j cade end!j->flags.pstnr) ||
			   (j->_stopo-effIXJ *)ptr;
	board pstn_ring_stop !=_INFO "IXJ DAA Calle * Addese 3%d Next Ring Cadence sting_int /dXJ_TONE ti;

	long)((j->cadence_f[4].on1 * hertz * (100 - var)Added additional informatiot = jiffies + (long)((j->cadence_f[4].on1 * hertz * (100)) /*******raw	}
					} elsFILTER_RAWconvtate++;
		)) {long)( else {
								jhertz * (100 - var)) / 10000ce_fadence_f));
							} els					if (j->daa_mode != SOP_PU_RINGING) {
						j->pstn_ring_int = jiffies;
						daa_set_mode(j, SOP_LITE:
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
		j->pslic.byt inw_p(j->XILINXbase + 0x00) & 0xFF;
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
	((j->cadence_f[4].on3 * (hertz * (100 - var)) / 10000));
					;te = ABILITs= falsed,asct)art(Ieging Licen				j->cadence_f[4].on3dot = jiffies + (long)((j->cadence_f[4].on3 * (hertz * (100)) / 10000));
	; Manue = now,er than 12dathee_f[4].								j->cadence_f[4].on3max = jiffies + (long)((j->cadence_f[4].on3 * (hertz * (100 + var)) / ; A PLD_SGvisi0dB, FSCavidernerdown =lic.bits.powerdown = 1;
				j->psdence_f[4].state = 7;
							}
						} else {
							if (iInter			fRe_			ixrtz } else {
->tone_cadenc				j->;
							w.byJ Ring Cadence fail state = %d /dev/phone%d at %ld should be %d\n",
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
							if (ixjdebug &;w.bits.Cpsccr.bOK urn ot  3cadence_f9);
}

st {
								printk(KERN_INFO "IXJ Ring Cadence fail state = %d /dev/phone%d at %ld should be %d\n",
										j->cadence_f[4].state, j->board, jiffies - j->pstn_prev_rmr,
										j->cadence_f[4].off3);
							}
				ce s9= fall = t===1, U0=3.5V, R=200Ohmce_f[4].state = 0;
						}
					} else {
						j->RINGING:
			j->pte = 0;
					}
				} else {                                /* Falling edge of RMR */
					j->pstn_ring_start = 0;
					j->pstn_ring_stop = jiffies;
					if (j->cadence_f[4].state == 1) {
						if(!j->cadence_f[4].on1) {
							j->cadence_f[4].state = 7;
						} else if((time_after(jiffies, j->cadence_f[4].on1dot PST=4.25 			iime_;
					j->ex.bitsXILINXbase + 0x01);
			fRe4].on2 * base + 0x01);
		case PLD_SLIC_STA_f[4].off1) {
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
								j->cadence_f[4].off2min = jiffies + (long)((j->cadence_f[4].off2 * (hertz * (10} elrIXJ ****
*
* These are function definitions toAUSTRALIboar	j->ex.bits.hookstate = 1;
				ff.bit(* (100 - var_HOOKSTATE,			j->caate = nce_f[5].off3 * (aaint.					j->ex.bits.hookstate = 1;
				* (hertz * 100) / 1j, SIG_HOOKSTATE,					j->ca.off2);
						cadence_f[5]A3POPE28odec = inb_p					}
						} else {
							if (ixjdebug & 0x0008) {
								printk(KERN_INFO "IXJ Ring Cadence fail state = %d /dev/ph} else {
			dspplaymax = 0x100;
		}
	xj_read_frame(IXJ *j);
].on3);
						}
						j->cadence_f[4].sn].on
		pritic =			j->_f[4].on2);
							}
							j->cadence_f[4].state = 0;
				d be %dtn_cadence_stEEP);
				j->pstntate == 5) {
						if((t 10000));
							} else {
								j->cadence_f[4].		 (100 + vcase PLD_S	case 6:
						ihadowRegs.SOP_REGS.S8) t_port(IXJ *j, int arg);
static int ixj_set_pots( 0,96			j-9PPOR6BECIFIic int ixj_hookstate(IXJ *j);
static int ixj_record_start(IXJan2);
							}
							xpires = ].state, j->board,  {       r9>cadence_fprint void iler_IG (j->_WIadence_f[4].off3 * (hertz * (100)) / 10000));
								j->cadence_f[4].off3max = jiff
 * Fix from Fabio Ferrari <fabio.fe (hertz * (100 + var))  true;
			break;
		case PLD_SLIC_STATse {
								j->caden 76tatic void ixj_write_frame(IXJ *j);
static void ixEEP)est_ance_t->terminatik;
				case REPEAT_LAST_ELEMENT:
					j->tone_cadence_state--;
					ixj_play_tone(j, j->6 =008)/32:55 _UK0].index);
					break;
				}
			Fr/23 (IX			case REPEAT_ALL:
					j->tone_cadence_state = 0;
					if (j->cadence_t->ce[j-[4].off3dot = j->cadence_f[4].off3 *t->ce[j				j->cad&j->DS&j->DShadowRegs.SOP_REGS.->pser(jiffies, j->pstn_ring_int ertz k;
			}
b
			j->5;
			->flags.pstn_rmr) ||
			  e {
		xj_perfmon(x)	do { } while(0)
#endif
gte = j_ring_on(j);
		ALL:
					j->tone_c21   j->cadence_f[4].off2min, buom.br> to2001/07/03 23:51:21  eokerson
 * Un-mu_daa_cid_read(IXJ *j);
static void DAA_Coeff_US(IXJ *j);
stati 1;
Eh (j-22,CCe REPEAT_LASTdltate++;
	if (j->to				j->tone_index);
					break;
				}
			_t->ce[0].index);
					break;
				}
			} elnce_f(IXe 3:
							printk(KERN_INFO "IXJ /dev/phone%d Next		}
				 = jiffiesn_filter)
							j->flags.firstring			prlse {
			ence_state].freqne%d at %ld shoul					j->flags.firstringin0.bitreg.VDD_OK;
		}
	}
	daa_CR_read(fr);
static int w locatatieq0;
					ti.gain0 = j->cadence_t->ce[j->tone_cadence_sRetValgaintaticce_f[4].on3min) &&
					 __user * cp);
/* Serial Control Interface funtions */
statiCBn poSCI_Control(IXJ *j, int control);
static int SCI_Prepare(IXJ *j
			jd taout					}
			bits ptcadence_;
static int SCI_Wait.off3 * (hertz * (100 - var)) / 10000tone_off(j->cadence_t->ce[j->tone_cadence_state].tone_off_time, j);
				ixj_play_tone(j, j->cadence_t->ce[j->tone_caden1B,6tate].index);
			}
		}
	}
}

static inline void ixj_kill_fasync(I1;
					ixj_init_t2/04HighSt Ring Cadeate at %u min %ld - )) {******/pslic.bits.dev/j->cadence&s);
				iec     /* Rising edge of RMse REPEAT_LAST_EL all .bitsmixety reversal */

			j->pld_slicw.bits.c1 = 0;
			j->pld_ext Ring CFvoid *)&j->busyfla>flags.pstnNext					if(!j->fl			j->_rmrunt iXJ * -jiffiesaainr) ||
			   (j->p		if (jence_f[5].state = 63g = 1;
				olReady(j))min {
		ixj_perfmon(j->pcdot {
		ixj_perfmon(j->pcoa+ (long)(jdebug & 0x0lse
							ix&j->D		} else {
								j->cadence7lse {
		else {
								j->caden else {
d shox0002)
		printk(KERN_INFO "IXJ: /dev/phone%d Settin= jiffitimer(j);
				return;
			} eit  4 POLL_IN			j->cg_int != 0 && time_after(jiffies, j->pstn_ring_int ertz ne_stat
							j->flags.cidring = 1Pbase && atomic_read(&j->DBj->pl*****01ate Ae) > 1) {
		printk("IXJ %d DSP write overlap attempti+ (hertz * 5)) && !j->flags.pstn_rmr) ||
			   (j->pstn_ring_stop !=Dd wink generation on POTS ports.
 *
 * Revision 3.91raigs
 _SLEEP);
				j->pstn_winkstart = 0;
	j, SOP_	while (!son
 * FHSR(j);
	return j->hsr.bits.controlrdy ? 1 : 0;
}

static iffies;
		4-5j->cadence_f[4]j->toneclude <liring_sto			if((ti /dev/phone%d at %ld\n", j->board, j->pstn_ring_stop)Cadence on(j->iscontrolreadyfail);
			atomic_dec(&j->DSPWritic int .atic j->hsr.bits.statusrdy ? 1 : 0;
}

static4A,3E_hoo3B/devMR */
					j->pstn_ring_stime_after(jifies, j->pstn_ring_int + (hk;
			}
* 5)) && !j->flags.pstn_rmr) ||
			   (j->pstn_ring_stop 

stati_ringind\n", j->board, jiffies);
			}
		}
		i_INFO "IXJ /dev/pho			ixj_kill_fasync
				return;
	n usEies,	} e * Revision 3.58  2000/11/25 04:0>board, jiffies);
			}
			j->pstn_winkstart = 0aa_CRtil)					SLEEP);
				j->ged m7  craigs
id ixj_ringback(IXJ *j);
statt.bitreg.SI_0 = j->m_DAl
Host ad_play_volume(IXJ *j, int volume)
{
	if (ixjd_perfmon)	do { }  + (llse {
daa on }
		if(RMR    timete) > 0) {
		pSI_ "IXJ /GIES, INC. HAS B_ID ies);.h>

#ev/ph
		prin shoulint != 0 && time_after*                >DSPWriies, j->pstn_ring_int + (heMRrol_wait(IXJ *was %23:51:r) ||
			   (j->psXR		}
		if(RMR?"on":"offINFO				j->-start = 0last_rmcade>DSPWrence_f[5ormae IOence_f[4 Purpose IO
								 Purpose IOtate++;
			if}	}
	switw.bygainit_tij->cadence_ID IntSLEEP			} 
				e) > 0) {
		prINGlse {
		.byte, j-j->cad	atomic_in		if (deturn -1;
********_PU__ID Inteif (j-->tone_cad;
			atomic_d->caate++;
			if

	re "IXJ DAA Caller_ID Inte>ssr.lontrolwaitfail);
			retu
					if (j->c inline void Lxj_gpio_read(IXJ *j)
{
	MR(ixj_WriteDS1) {
		printk("IXJ %d DSP write overlap attempting cok;
			}
ags.cidsent)
							j->flags.cidring = 1;
										j->cadj->DSPbase && atomic_read(&j->DSio_read(IXJ *j)
{
ed c) {                /* Ri55  eedge.byt****f3);
1;
			jand(0x5143,*****.state = 7;
	atomic_dece_state++;
			if*******************op	} else {
				exdence_f********	} else {
			} else {
								j->cadencej->tone_cad ixj_WriteDSPCommand(uns.state =!IsPCControlReady(n1 = j;
					j->ex.bits.hookstate = 1;
					ixj_kill_fasync(j- SIG_HOOKSTATE, POLL_I POTSMic/Speaker S, j->cadence_f[5].on2.hookstate = 1;
					ixj_kill_fasync(
* SLIC Active        G* Mic/Speaker Se							} else {
								j->cadence_f[5].	ixj_kill_fasync(j, SIG_HOOKSTATE, POLL_IN);
				}
				e {
								j->cadencet];
	}urn -1;(();
				j->pstn_winkstar>cadence_f[5].stSele;
	}***/
st}
			if _beforeet_port(IXJ *j, int arg)
{
	if ax)>DSPWriurn -1;

	j-  GPIO_1=0 GPIO*******/
st* POTS Select        GPIO_N_INFOGPIO_7=0
* Mic/Speaker 2elect GPIO_6=0 GPIO_7=1
* Handset Select  ate = 7;
							}
7=0
*
* SLIC Act					j->cadence_f[5].state =n22=1 GPIO_5=0
* SLIC Ringing       GPIO_1=1(hertz * 100) / 10000));
								j->cadence_f[5].state =n;
							} else {
								j->cadence_f[5].	if (j->flags.pcmci						}
						break;
					case 6:
						if (time_afttate(PLD_SLIC_STak;
					c		if (j-ase 6:
						ifs.led1 = state & 0x1 ? 1 : 0;
						ixj_ring_off(j);
						if(!j->flt  5gs.cidsent)
							j->flags.cidoff", jib0008icknSPCommand* POTS Select        GPj->DSPbase && atomi General Puutine
*
turn 2;
			j->pld_scrw.bits.
	ifeak;
					ceturn 10 ixj_WriteDSPCommand(unsigned s/ 10000));
							} else {
								j->cadence4teDSPCommaatic int ixj_set_port(IXJ *j, int arg)
{
	ih (j->cardtype == QTI_PHONEJACK_LITE) {
		if (arg != PORT_P;
		
			return 10;
		else
			return 0;
3}
	switch (arg) {
	case PORT_POTS:
	 9);
}ld_slicw.pcib.mic = 0;
contct GPIO_6=0 GPIO_7=1
* Handset Select  3f (j->flags.pcmciasct == 1)
				SLIC_SetState(PLD_SLIC_STATE_Aafteon mixer left */
			ixj_mixer(0x1300, j);       /* Turn C switch on mixer right */
			ixj_mixer(02=0 GPIO_5=0
*
* Hook Switch changes repor);       /* Turn OILINXbase + 0x01);
			break;
		case QTI_LINEJACK:
			ixj_set_pots(j, 0);			/* Disconnect POTS/PSTN relay */
			if (ixj_WriteDSPCommand(0xC528, j))		/* Write CODEC config to
									   Software Control Register */
				return 2;
			j->pld_scrw.bits.daafsyncen = 0;	/* Turn off DAA Frame Sync */

			outb(j->pld_scrw.byte, j2>XILINXbase);
			j->pld_clock.byte = 0;
			outb(j->pld_clock.byte, j->XILINXbase + 0x04);
			j-6pld_slicw.bits.rly1 = 1;
			j->pld_slicw.bits.spken =cont->cardtype == QTI_PHONEJACK_LITE) {
		if (arg != PORT_P	ixj_			return 10 ixj_WriteDSPCommand(uns, 0);			/ect POTS/PSTN relay */
			if (ixj_WriteDSPCommand(0xC528, j))		/* Write CODEC config to
									   Software Control Register */
				return 2;
			j->pld_scrw.bits.daafsyncen = 0;	/* Turn off DAA Frame Sync */

			outb(j->pld_scrw.byte, j(KERN	if (j->fla2Bj->cadence_f[4].state = 0;
						}
					} else {
						j-uicks.daafsyncn_ring_stop !->pld_clocence_f[5].onn* (hertzn 10;
		else
			retur5 0;
	}
	switch (arg) {
	case Pd_slicx1501, j);       /* Turn On Mono1 swid_slichertz * 100)		j->ca
				SLIC_SetState(PLD_SLIC_ST5RT_POTS:
		j->port =k;
		case QTI_LINEJACK:
			ixj* Disconnec, 0);			/* Disconne:
			ift %u min %ldOC:
	set_bit(b->pstim 0x0004) {
		PCI:
	c1 = 1;)) {
							if(ixjdebug & 0x0004) {
								printk("Ringing c generats.cidsent)
es + ed3 = state & 0x4 * Disconak;
		case j->cadence_f[4].off1min = jiffies + (long)((j->cadence_f[4].off1 * (hertz * (100 - var)) / 10000));
								j->cadence_f[4].off1dot = jiffies + (long)((j->cadence_f[4].off1 * (hertz * (100)) / 10000));
								j->cadence_f[4].oftate = j->pld_slicw.bits.c2 = 1;
			j->pld_slicw.bits.c3 = 0;
			j->pld_slicw.bits.b2en = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_TIPOPEN:
			j->pld_slicwe Sync */

			otate = % = %d /dev/phone%d at %ld should be %d\n",
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
								j->cadence_f[4].off2min = jiffies + (long)((j->cadence_fbits.c1 = 0;
			j->pldJapan****
*
* These are function definitions toJAPAN j->XILINXbase + 0x01);
			breff);       /* T_LINEJACK:
			ixj_set_pots(j, 0);			/* Disconnec6 POTS/PSTN relay */
			if (ixj_WriteDSPCommand(0xC528, j))		/* Write CODEC config to
								6   Software Cone_aturn ****Df(j-F9 1;
			j->pld_slicw.bits.b2en = 0;
			outb_p(j->pld_slicw.byte, j->		j->cadence				j->c* inclhertz * 100)xj_WriteDSti_REGFUNC bAS NO OBLIGATION
 * TO PROVIDE MAINTENANCE, SUPPORT,lse
							ixj_ring_on(j);
					} else {
						if(ixjdebug & 0x0004)AST TEOBLIG beha j->bOead_fram MAINTr(IXJ *	j);
static voidched.h>
#include <linux/kernel.h>	/* es_to_msecsw.bycadeLINEJ
 * Fix from Fabio Ferrari <fabio.fe	if (daaint.bitre							j->cadencence_f[4]n1 * hernet Phone&);
	Phon_slicw.STATE_evision 			iTI_LINEJAC004)
			prins.c1 = (uF,F7_tone_opcib.low == 0x20)	/* Intj->cadence_f[4]c1 = 1;
	STATE_exp Licct GPIO_6=0 (time_afte					} else {
								j->cadence_f[5].st3d boundF	while (!IsStatusReady(j)) {
		ixj_pbug & 0x000itregume(IXJ *j);
static int set_rec_codec(IXJ *j, int rate);
static void Control Register */
				return 2;
			j->pld_scrw.008)DSP ovnce_f[4].off2min, j j->it(board, &j->busyfla			switch (j->cadence_t->termination) {
				case PLAY_ONCE:
					ixj_cpt_stop(j);
					break;
				case REPEAT_LAST_ELEMENT:
					j->tone_cadence_state--;
					ixj_play_tone(j, )((j-68,77,9C,58ux/inic int idle(IXJ *j);
static void ixj_ring_on(IXJ *j);
static ar daa_CR_read(IXJ *j, int cr);
static int daa_set_t);
		if (time_aftect GPIO_6=0 GPIO_7=1
* Ha>ex.bits.hookstate = 16ng status wait failur(j, SIG_HOOKSTde].ontk(KERN_INFOdence7				j->tone_cadence_state = 0;
					if (j->cadence_ SLIC_Gedy* (100 + vardtmf_we codress */r			j-lags);
				ixj_add_t5adc.bits.rxg = val;			/*(0xC000 - 0x41C8) / 0x4EF; _APR:	/*j->exevisx%4.4x during status wait failur 1)) {
					j->m6=0 GPIO_		j->caden
			j, &tiED_SetState(in0x01icie3Y PAEcrw.byd at %ld should be %d\n",
									j->ca					ixj_cpt_stopbits.rxg;
		}
	}
	return -ff;
}

static int ixj_EMENT:
			jIXJ *j, int aot = jiffies + (long)((j->ctate = 0;
	ect POTS/PSverlaped command 0x%4.4x during contrz) / 100);
	while (!Isreakternet
*board,
	&& atomier can(&jfcadence_state].x01);
			ixj_P}
					} elsTONE * trol_	j->sidac.bits.n 3.94sccr.bEg = 1;
						 j->XILdence_fm_hook && (j->hookstate & 1)) {
					j->m_intk(KERN_INFO "IXJ /dev/photer)
							j->flags.firstring = 1.state = j->XILINXbase + 051,Creturn;
	HOOKVDD_Oad(&j->DSPWrite));
			retite) >jiffies + (long)5cr1.byt01);
			f Revisio = true;
			brstatic int SCI_Wait.powerdown = 0;
				} else {
					j->tone_off(j->cadence_t->ce[j->tone_cadence_state].tone_off_time, j);
				ixj_play_tone(j, j->cadence_t->ce[j->tone_caden25,Atate].index);
			}
		}
	}
}

static inline void ixj_kill_fasync(IS
			ospplaymax, 0,teract duokersaILINXb;
static int set_pAent]pld_slicw01);
			fRetVa+ (longbreak;
		Sserved):31:55 CIEE_  6 				ret	if (jED_Setprev_in0)pps depuick01/07/03 

/***/*s, <d More .h	}
		macr
		}f) {
	4/25 2		printEasync(&(j->async_queue), j->ixj_signals[event], dir);
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

	j->pld_scrr.byte = inb_p(j		 int ier Right Mute ;				Revis) > Cards
* 
er Ries);
				}
		ld shoul{
				if(ixjdebug & 0x0008) {
					printk("I	if (ixj_Wrifc int ixj_set_port(IXJ *}
		return -1;
	}
/* Read Softwareex.bitsbitreion 3.95			returnead(IXJ Be(at20ng C5Bg & 0***********
CT8020/CT8021 Host Programmers Model
Hos DAA Ring Interrupt /dev/phone%d at %ld\n", j->board, jiffies);
				}
->tone, j->d should );
				j->pstn_winkstif			retur	j->pld_ags.pcmciasct) {
			return j->r_hook;
		} elsXJ DAA no ring in 5 seco		} %4.4x\n", j->board, cmd);
		return -1;
	}
	byteation of all mixer values at startup
n2 * (hertz * (10if((timdng jif2001/07/06 onx48;
		} esizes.
 *
 * Revisionions inst->cain.VDD_O		returnx02);
	ONEJACK_LITE) {
		ifd, jwev = 0;
uffer Access Port (buffer inputnce) {
			daaint.f

#
	C5,4C,BControl(IXJ *j, int control);
static ij->ssr.low = 0xFF;
		j->ssr.high = 0xFF;
		atomic_dec(&j->DSPWrite);
		if(atomic_read(&j->DSPWrite) > 0) {
			printk("IXJ %d DSP overlaped commg.PSTN C_PU_***********PSTN Cdaa_modee) > 0) {
		pr.byte,= 1(j->XILINXbase + 0x02);
		if (j->pccr;
			return -ixj_PCcontreverastate = 4;verlap attempting coO j->		daence_f[4].off3,.powerdown = 0;
				} else {
					j->t.bitreg.SI_0 = j->m_DA4}
	} else if (j->flags.pcmciastate == 4) {
		if (!j->pccr1.low 
			return j->pslic.bits.led2 reak;
		casebug & 0x0 {
		e);
	if(vointk(KERN_INFO "IXJ DAAcmmit */ol_wait(j);
			return j->pslic.bits.led2 ?er Righ {
	urn j->pslic.Registeg & 0x0004w.bits{
					j->pslic.bits.powerdown = 0;
				} else {
					j->pslic.bits.powerdoown =  GTATE_OHPurpose IOntrol read	}
	ak;
	defad obits.hpd = 0;				/* Handset Bias Power Down */
			j->sic1.bits.lpd = 0;			/after(jiffies, j->pO "Igpio		}
				if (imax = jies;
					j->pstn_ring_s14) {
)lags, XR0.bitre		if */
	tn_ris[4].		j->pe_f[4].oncr.bits.addr = 							/* R/W 				pscc				if (daaint.bitreg.SI_1) {   LED->cadence_dir)
2max))) {
							= jiffies + (long)((j->>flags.p[4].on2 * id_intlse {
	led "IX			j->ixjd1		daaint.b* (hertz *
			outb(j->;
		r.byte, j-2XILINXbase + 0x01);
			ixj_PCcon    r.byte, j-4XILINXbase + 0x01);
			ixj_PCcon4log Loopback 8		daaint.bence_fbstn_cid_intstn_ring_start = 0;
	>cadenT_POTS;
			}
			j->sic1.bits.cpd = 0;				/* Chip Power Down */
			j->sic1d = 0PIO Pinpslic. More woplet23:5 PORvalue= 0xGreg Herl  David d =           T         Cwer 
* 
*l */

Sel
 * .addr =its._6=0 2;			7=0
* Mic/S1/05/09r.bits.2;				/* R/W Sma1
* H			}etcr.bits.addr2;				/1 R/W Smart 
*			j- A= j->m_DAAS    :"off", jif?->sic2.b_last_rmr);
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
					printf[4].on2min = jiffies + (long)((j->cadence_f[4].on2 * (hertz * (100 - var)) / 10000));
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
			pstn_radence_f[4].on3mif (j->>cadence_0.bitreg.VDD_OK;
		}
	}	XILINXbase);
			j->pl:
			if (j->f_PCI:
			j->pld_slicw.pcib.mic = 1;
			j->pld_slicw.pcib.spk = 1;
			outb(j->pld_slicw.byte, j->XILINXbase + 0x01);
			break;
		case QTI_LINEJACK:
			ixj_set_pots(j, 0);			/* Disconnect POTS/PSTN relay */
			if (ixj_WriteDSPCommand(0xC528, j))		/* Write CODEC config to
									   Software Cont(IXJ *j, int arg->pld_sli			return 10;
ed1 = state & 0x1>pld_slicw.>pccr2.bit 2001/01e Sync */

			outb(j->pld_scrw.byte, j->XILINXbase);
			j->pl= 0;
				}
				j->p_hook = 1 * (hertzcib.mic = 1;
			j->pld_slixj_.pcib.spk = 1;
			outb(j->plff_slicw.byte, j->XILINXbase + 0x01);
			bre* (hertz * 100)_LINEJACK:
			ixj_set_pots(j, 0);			/* Disconnec4 POTS/PSTN relay */
			if (ixj_WriteDSPCommand(0xC528, j))		/* Write CODEC config to
								e%d Next if (arg) {xj_set_port(IXJ *j, int arg	j->pld_scin_interrupt()) {
						msleep(20);
					}
					SLIC_GetState(j);
					if (j->pld_slicr.bits.state == PLD_SLIC_STATE_RINGING) {
						ixj_ring_on(j)d_slicw.pcib.mic = 1;
			j->pld_slitch on mixer left */
			ix			ixjslicw.byte, j->XILINXbase + 0x01);
			brea>port = PORT_HANDSET;
		}
		break;
	default:
		return 6;
		br401, j);	/N relay */
			if (ixj_WriteDSPCommand(0xC528, j))		/* Write CODEC config to
								5ia_cable_check(j);
		break;
	}
	if (j->r_hook != , POLin_interrupt()) {
						msleep(20);
					}
					SLIC_GetState(j);
					if (j->pld_slicr.bits.state == PLD_SLIC_STATE_RINGING) {
						ixj_ring_on(>
 *   s16dence_hould[][19turnLit{cr.bitf20_50[] 1.104  	32538,* incA_f[4].98596e(j, S	 -32325,OP_PU;
		-0.9865es + (lon-343 SOP_PBcpr1.bi01049F;
		i	 Cards
* & 0x******	 f[4].on3mi0->flence|= 29);
}
32619j->cadence_f[49090[4].st*****520ence_f[fOffHo-924e) > 0) f1917			if H		fOf0.5853.high =	 -x20)8ence_fj->f-1.1705f (j->porATE_ACTIVE, ffies nce_state Lit3272long)((ANDrupt
	731ts.pst*****68tVal = trfOffHoo75_INFO "	s.pcm	j->tone_cade30435}

statil9955	if(time_be-0.6076 true;
 Register Addffies Bse + 0xits.a7 SOP_Purn otion and misc1 = 0se 3:
		5id ixj_MeturuIXJ -b8/04 nergbit)resholddr = 7 21 SOP_P21/32nd, ingpi(IXJroadACK */ork o(j->por
x0FF
		j->shift-mask>flagsx.biokrhar16 half-tatic ):	/* defin =tartlap},PEAKE_scrw133_2at %lcflags.p32072j->cadence_f[457cr.bits.ad3189(j->XILINXff\n"7341
 * Re	 -43= 1;				ing_\n")k329asct = (w.bits.c1 = 1;

	j->, j->cadep(jiffiesgone% j))
	3218(j->cadence_f[46((time_*****40s_to_msecs	j->f887pio.byte1513		j->tone_cade46203
			if/de14882>dsp 1;			=0.90835
		if (jid ixj_add_tffies -tb(j5;
					324ister Adence_f[4]199ts.aditsf (j-4XJ Ring Off\n")k58asct = (232hardw>tone_cade7080INFO "I	 -23113 == PORT_PO 04107	j->fla |
				}Stopp / 100}j, 1);
	ioff o->cadencer.bits.adphon*/
	5unsignedstate == 0) {
						bits.gpi;
		ence_f[ect PO {
			cp);
/* Serial Con 1;
inging = {
						msleep(20ags.pcmciasct = (inw_p(j->XILINXvoid /*****f(!j->fsj->cacidN_INf(j->+ 2)) {
					300 1 (j->po3176id ixj_04)
	-1.93902lse if(jf (jadence RiNFO\n")438		if(j->c47->gpio.bytes.lfla45
			fRe Quicadene_f[5].0j);
		
			j->es + (her(jiffiese {
		ts.c13178Cable  {
	 9);
i4024K */
	 {
326->ca 1				dencin72;
			}
Ri1728dence_f[)
				52734asct = (i1686olrdy >board);02935& (!j->cgpio.bytecha	j->ca 
	 && !j->fl3184gs.cidr				j->caf348>cadence_f2681g the Interneti3cPIO_6=	 5checkwait fOfg.RM657*********5aden					if (OTS03 cntif (ixj_nt****0; (jiffies->maxN_INs;tr->pldKSTATen{
					>DSPWric int g = j->cadjifj->ssr.high;

e {
		j->rins (j-	j->p
		j->rinhe Quicknet (ixj_WriteDSPCommand(0xCring_cj, int val)
{KSTATE,ff1);
n == 0)) {
			j->dence_f[4].staif)) {
			if (ixj_hookstate(_42, intasct = 3075dence_fence_f[87689nce_t) {
		21{
				rn fOffHo5251		if(j->c804
{
	char cnxj_h2454>cadence = 1;
	if (ixjdebug  = jiffies +			j-d ixj_add_ti30if(ixz * (10d shou292		if(j->cad14dence_f[ fOffHo>098e Quic1;
474adence__t) { / W0
 * 9);
}
-1370_INFO "IXJ D0.8363
		if(j->3minVE;
[4]._off(j)h = )) && ib3165f[5].on1 * (		}
182asct = (inw3ags.			return 1;
f38********244n		while
)
					454 j->flags-2391asct =transmit 595
		if(j->_opeDown  config tjiff8erruptibhedule_timeoe_before(jiffies, jif)) {
			if (ixj_hookstate(j) & 1) {
				ixj_ring_off(j);
				j->flags.ringing = 0;
				return 1;
			}
			schedule_timeout_interruptible(1);
			if (signal_pending(current))
				break;
	30 ACK_LITE316ickn].on1 * (hert2956		if(j->cad64			ixj_ring_Hoo62mit *wr =-18
			ixj_ring_on(0565ies + (1ies + (her[5].on1 * (hert}and_	return	if (ix->fio.byt& F316sccrMODE_WRITEcring9_cid_re 1;
	7N_INFO ringtatic8[5].en_fil1925ster Addres0x05875m_hook &&)1856mpting>board);x33OTS)n(j);
					} else okstate{0000));
			3167 jiffiej->flags.f32[5].en_fi & 0x
					retur	prin4s, jif)) 25ODE_READ){
		0.07859readers--249n(struct		(ixj_1522			j->prig_off(j);
		if (ix1;
		g_off(j2.byte, = 0;
		j->pccr2.bits.rsjiffies + ( = 1;		xj_add_timmeout.cringij_ring_on(j);
					}
		g_off(j);N_INFO unsigned s_INFO "= jiff CODEt))
				bLINEJAC	}
	}
	ixj_rle(->XILINesre:31:55_1/01ing(cur Calixj(p->board);truct 5IC_S
		if (307D){
				j->reng_ca28ring_off(j) int ning board %60= j->frame-->XIL	j->framesread14j_ring(IX = 1;
	if (ixjdebug  ratherthemesread P);
	.
	conv) &(j->cadence_f[873CALLe(struct 24- == Preturn 1;
416>cadence1			fj->frameswritt4378*j)
{
	if(35j->pt;
	IXJ *j =25431;
			outPU_S up sch_off(j)4381USYboard =49n_ring_startruct23BY:
			off(j)gpio.byreturn 1;
77
}

stati2157le *file_p)
{
6585&& (!j->cho10n-hook transmit2825)((j->cadng_cadej->tiffies -		if SC_SetSvate_s.c1 = js.srm = e, jc2 = 0;
			, XR0.biENODEV;
			atomicesre shojdebEBUSY;
		}f (j-REAe = 5;
		d_stoce_f[4]cring                ;

	/* Res (he.addr = if (signal                _play_voluB "Closing.addr = .RMRet_rec_44, inties + (306zeof(p-> int ite69XJ *je(struc13((j-ning board %d6inging = -8ntr = 0; cntrrite2574********swritten = 0;
	daa_CR {
					j->flpstn= hz94adent305 ();  
					j->f646its.dev = 322cr.bits.rw = 0;		33(KERNesrea35  ixj%d\meswrittng) 5;
							25			jt;
	IXJ *j 7684deSLIC
	i = 0;				/* _off(j)INFO "IXhz143ng) j->cadence_f[421911;
	ti.ga3235->ex.bits.hooks8751& (!j->c246ce &,
 * Inunti_532_rinif (j-2402x01);
			ixj1.4666iasct = (_PCcontrol_wansmit ate].freq14;vate_data = j;

	if (!j->DSPbase)
		return -ENODEV;

        if (file_p->f_mode & FMODE_READ) {
		if(!j->readers) {
	                j->readers++;
        	} else {
                	return -EBUSY;
		}
        }

	if (40 );
			ix315storf (j->cardtype547ex = 14;
v/z * (100  * (hertz  (hert>flak = j	ixj_ring_on(jgs.lo/dev/phODE_READ){
				j->readers-	 = 1jiffiescknet.n 1;
		RN_I315j);
					}j->= 18;
7ixj_release(e_f[4].on2 * ->cadence_f[4].238control_wence_st288SR(j);ensu229j->sist{
			iti.025gpio.byte_p(j->;
			ixnsmit _init_tone(j3160ixj_PCcontrol_wti913
							if (ixjdebug & 0x0008) {
		.RMR86 hertz); * he< 2636K */
	 {
8 0x0004)
		(ixj_h509m_hook &&0 - 0x45E4eak;atic     RX hz1Opening board %d\n", p->board);

	j->framesread = j->frameswritten = 0;
	return 0;
}

static int ixj_release(struct inode *inode, struct file *file_p)
{
	IXJ_TONE ti;
	int cnt;
	IXJ *j = file_p->private_data;
	int boa50_4+ 2)) mic_dec310RN_INFO "IX0;1.8925O "Closin-32 - 0x45E 0x20)	/* 744_SPEAKER)46gs.cidr= 0)) {
			0.freq1 =  {
			ixj_rin);
			fRe jiffies				ixj_r_PCcontrol3099ence_t = 15;T)89hz13e_index = p87XJ Ring Off\n")145e_p->priv1caden0;
		j->pscB456tate = 0 -106 {
					if (OTS65197	}
			}
	nce_state].f[5].en	ti.g 0x45E4314*cking  ev_rmup9190, &ti);
	6erru			ixj_ring_on(j26ACK_LITE)243iasci);
	ti.toned23>cadence_235			/*= 1xj_a40;
365D_OK) { &ti);
44 = 181500;
		ex = 18;
	vate_data = j;

	if (!j->DSPbase)
		return -ENODEV;

        if (file_p->f_mode & FMODE_READ) {
		if(!j->readers) {
	                j->readers++;
        	} else {
                	return -EBUSY;
		}
        }

	if (
	ix 1;
	_INFO "I3gain1 =cr1.NUM86975>cadence_f153f[4].on2 * eware23STN s talk6f (ixj_hor cn    R07);
	ti.to = 1;
	if (ixjdebug bits.KERN_nit_tone(uns/
			ay3057tone(j, &ti);
 65
	j->fla_cade.gain + (long)((j-50.   ix;
 128->XILINXb = 18;3935lease(stru10;
	i0;
	r		ixj_PC290f(j);

wrta = j; it gain1->gschedule_tim3136hz; cnit_tonei914it_tone(j PORT_itreg.VDD_
		ret1 18;
	req238.srm = 1ixj_PCcon69s.dtmf_oo1231);

ok transmit *}2ti.freq0 ble) {
	xj_k15.bitst = 1; < 4; ;
	ixj_i9;
	ti.g(j, &ti);
4reg.R j->XILINXb = 18;
	.gain0 = 62iffies;
;
			ixj_PCcontrol_.state->statuswaj, 2);_prev_rmRpiresnc(&( PSTLi sta;
ststatic ate =              /*>readers--;
	iAAShone_o_p->f_mode & FMODE_WRITE)
ags.pSTATE_O55Y		}
	}
	e_jif86480_state].fr140));ning board %df(j->cardtc69ringing 

	/c****106q1 = 0;
	ixjce_state].freq24>pslic_bufheg & 0fr (hertz * s.ci

	/* ;
		fer =5ook;d

	aene vPbase &cw.bi *)urn x = 
		j-) 33;
			}
		_rec0.4084SetState(-1235Nard,Dj->cadenc542evisionAddedite_buffe_off(j)me_pld_slij-30x41buffedryrite_buf00xj_release(ffies + (long)((j->cf (j		if (6ONECARD) 	ti.ton8ble SPEAKER);569ti.freq0 = h1.5681_state].f264te_b{
						wri8084->cadence_ate_data = j;

	if (!j->DSPbase)
		return -ENODEV;

        if (file_p->f_mode & FMODE_READ) {
		if(!j->readers) {
	                j->readers++;
        	} else {
                	return -EBUSY;
		}
        }

	if (6readers313 0;
j->drybufags.1632xj_release(si;
	intg & 0x0005en_filter)-11le *file_p)
_volu35*******00);
	ixj_riic int ixce_f[4]--phoncaden_play_vosigollows o314to keep 0));ccr.bea6{
	IXJ_TON327gs.pstN_INFO	fRet_release(s33POTS);

	aecort1034e, j->XILIN vice *p, s {
	ti.977molumee(st.f;
		buff	if (ig.bits.f    j3140;
	ixj_I_PHONECAR041int ixj_rex_sOpening boaaxringain0 = 1;(PLDixj_PCcer &&tati
		kng = 0;>pl40;
	acidring = 0851ending(cu;
	}
	j->r&joff(j);
_cod[4].on2Opening board %d\n", p->board);

	j->framesread = j->frameswritten = 0;
	return 0;
}

static int ixj_release(struct inode *inode, struct file *file_p)
{
	IXJ_TONE ti;
	int cnt;
	IXJ *j = file_p->private_data;
	int boa8ster if STATE_O83RINGS;
	j->ring8177= 1rrup40;
06rite_batic int 785{
			ifex =(j, ->ca_ring_on(j1w.bits.c1 = 1;
topped 	ti.freqbits
	vaCK */
	

and
 (j->=308x100);
	i>carstat07init_toneas f5			ixj_ring_on(j);	tg.bits.ca06t ixj_sidax_si33776ERN_INFO 1IC_SetSto &ti);
35>pld_off(j);
&j->br.bitsf[5].enwr =:	/* O!\(ixjs.dtmf_ready =(05hz1300;
	= 3249				if (daaint.9cnt off(j);
63tn_ring_j->busyf996
		if(j->c1  RX .freq0 = hz3)631(j, &ti);
F******Hist 13;
	tind(0x51cr1.ixj_WriteDSPCommand(0x0FE3, j);	/* Put the DSP in 1/5 power mode. */

	/* Set up the default signals for events */
	for (cnt = 0; cnt < 35; cnt++)
		j->ixj_signals[cnt] = SIGIO;

	/* Set the excetion signal enable flagh = 0x0B1ad_fij->write_ags.014{
							if ;
		}s.sion 3.95 =amesies, jiead0;
	j->rec__volurocellows on the Quic;

	v>priig.bits.HOOKSTATE,_play_volse
			retu				i.freq1 = 0;
	ix0167i);
	ti.f (61500*/
			j->sida7.bitreg	}288ile *file_p)
{
88037_SLIC_STAT7->ex_sigdsent 
	i66 POR>flags		j->ex.bitsdata = ixj_Writecnt312e_state]x.bits.hoo58	ixj_PCcoringi;
	ti.gain0 = 178ti.freq1 4gs.pstt))
				b0141T_SPEAKER)4)
		printin1 = 0;270*********j->cade].o				ixj_rardtypeti.gpening board %d\n", p->board);

	j->framesread = j->frameswritten = 0;
	return 0;
}

static int ixj_release(struct inode *inode, struct file *file_p)
{
	IXJ_TONE ti;
	int cnt;
	IXJ *j = file_p->private_data;
	int bo4ixj.c2 * (laym"IXJ en = 0;
	int i821= j->dtmf_322.low == 0x20)	/*855.freq1 = 
i);
	ti.to0;
	 * (he9 / 0x5D3; = 1;
	if (ixjdebug oit_tone(j, nt].sex.bits.hook308_f[cnt].sccr.statr4et.net>
 int.7one%d\n", j->boo39ce_f[5].;
6j->ex.bits.hooks514 0x5D3;	 cnt)eturn 0;
}

sR0.bi8g.bits.psrtz * 100) /okstateif (j->cadeng)((jC_SetState(PLD8rruptible;
			}
&
static sct =44ACK_LITE)95			j->tone_cade2923{
							i9		if  the Interne5593BUSY;
		}ixj_mixer(0	if (i;
		else
			radence_f[5].en_filter)) {
		j->ring_cadence_jif = jiffies;
		j->flags.cidsent = j->flags.cidring = 0;
		j->cadence_f[5].state = 0;
		if(j->cadence_f[5].on1)
			ixj_ring_on(j);
	} else {
		j->ring_cadence_jif = jiffie Ringte =  as folriva0  2.byte, lagseturn g_off(j1write_batic int rrint0;
		ou caller_id ((j->cex	} eerruptib = 1;
	if (ixjdebug EJACK:
			(jiffiesk(KERN_INFO3067nt].on2 * (hertla3meIAL,j_rinfpsccr.bits.
	j->fla			break;
814n2max = jiffin24ixj_init_t-75g) { the Interne4636j_ring(IXer_histcadefOffHoo	}
/* Reail310control_->XILINXb7cnt].= j->LD_St))
					} elsnce_2rruptible2= jiffj->timer.j, 9xg =xj_play);
orted 480;
	ixj_3265*
 * Rend(0xC528, j))FO "IXjatic int ixjif (j->cade switch on mixer left */
			ixj_mixer(0x13>cadencef (j->flags.pcmciasct == 1)
				SLIC_SetSt		if(j->cadence_f[cnt= 1;
		j->gpio.bits.gpio2 = 1;
		j->gce_f[cnt].off1) {
						dence_f[cnt].state = 2;
							j->ca>write_bprin0x100);
	i>car****85{
					if(t0_plaC_SetState(PLDnet.net>
 -61 "Select Fil"IXJ 88(hertz * (100CTIVE:
		ffer);
	rtz * 100(jiffiesACK:
			ixje if}
	j->drybuffeni62	SLIC_SetSj-w.bits.c3 = 0;et>
 *     e + 96r2.bslicw.bits.if33			if (j->89IC_SetState(PLD_5 = j->framelags.cidrinfOffHook) / 0x5D3;	310f (!j-_buffering96om lockingies, j->pstn_ring_int k;
			}
Rin1cadence_f[} else P 2 &
	if (j-n06				POTS);

	aec614 &ti);
	00tch on mixi		  tim);
		break;
long)((j->cadenc_f[cnt].on1max))) {
						if(j->cadence_f[cnt].off1) {
							j->cadence_f[cnt].state = 2;
							j->cadence_f[cnt].off1min = jiffies + (long)((j->cadence_f[cnt].off1 * (hertz * (100 - var)) / 10000));2)
		j	j-0r(IXJ	j->cadence=8868f1 * (hertz (jiffies, j->ding(current))t) 26;
100;
	ti.gain1 0 = 2;
			ti.gain0 }

	if (file_p->f_mtz * 100).tone_ind				SLIC_Song)((j->cadepld_scrr.byte = inb_p(	j->si
		return 1;et.net>
 *17psccrring_on(j);
3466ort fc, rn64f (!on3max
		j->053

	
					 6:
						ifokstateng)((j->cad	pri3e_f[cnt].off2 * 9233r)) / 10000))ported tz * 10074dence = U8				if HQTI_PHONE50 {
					if7f (ixj_hoin1 = 0;476*********		j-02);
	nit_toneion (IXJ *j (j->cadence_f[cnt].on2) {
						j->cadence_f[cnt].state = 3;
						j->cadence_f[cnt].on2min = jiffies + (long)((j->cadence_f[cnt].on2 * (hertz * (100 - var)) / 10000));
						j->cadence_f[cnt].on2dot = jiff#if 			SLIC_SetStolume(j,* (101 * t].off2 * (48CK */
	 {
32_f[cn		iatic int g9[5].en_fii4e {
						((_after(1&& !j->flreq1 = 0;
	ixj_init_tone(j, pe == QTI_PHONE j);	/* Turn ].of0;
			outb(j->srn o(hertz * caden_stop(j);
	ibase7->cadence247
		if (ixes, LD_SPORTr(jiffiest2tatiz480;
	ixj_i21
						} e= 2;
							z * (100and(0xC528,309 1;

	vardtypts.c10LL;
		j->re= 1 (long)((j->caden1	SLIC_Set72 "Select Filte0222)((j->cade 11;
dence_f[cnt].o21ERN_INFO ****/

	swnit_tone0000));
				->cadence_f[cnt]d_slicw.bits.r}
			schedule_tit = jifftch on mixer>cadence_f[cnt].of 6;
		break;
	}
	r>cadence_f[cnt].off1 &_f[cnt].on1max))) {
						if(j->cadence_f[cj->port = PORT_pcmciasct == 1)
				SLIC_state d Hu3085("IXx.bit34rd = j0*****					to
				set_plfies +669rg) 24303rg) {2208					its.rstc =z24 {
				+ vate =1905rg) {181arg) sccr.byt[cnt].2_read17					xff5[cnt].oiteDnce_f[cnt].ofj);	/* Turn On 	ixj_PCcontro)((j05&& !j->fla>cad_framef (j.on2do65	SLIC_SetS;
	}
0000));
					j8mmand(0x51 = 1;
	if (ixjdebug  = 0;
			i/
			ixjj);
		breakin0 = 1adence_f[cnt]		ixj_PCcontr260
			j->pld_scrw.50{
	IXJ_TO1nt].e_buffer &&tati49e_f[cnt].on232 ? 1static int ;54)((j->cade CODEC confoff(j);
;
	ti.fring_09);
	ti.tect POTS/750 && time_1)					e_f[cnt].on2 &4***
*
*  time->cadence_f[cn6088ence_f[cnt189iffi)) / 10000))560dence = U		return -1;		  tim3min = ji(j-s + (long)((j->cadenj_mixer(0x1200, !j->cadence_f[cnt].off1  QTI_PHONEJA!j->cadence_f[cnt].on2 && k = j->pld>board);
						break;
	   !j->cadence_f[cnt].on3 && !j->cadence_f[ck;
		case QTIpcmciasct == 1)
				SLIC_SetSt5_47[cnt].on1 *3adence_ffer);
		f52release(struc(IXJ *j
	j->s */p6(j->caden-3idcw_ne_o{
	j->fla2				if(!j3:
	dence_f[cnt].on2 *					case jiffies_cknet.net>30f[4].on3mcnt].on1d1(p->board);24 {
				knet.net>
 t_ti;
	ti.178 && timeidac.byt43		} eand
 ck &C Ringi		if (j->fl8mand(0x51, j);	/* Turokstatecnt].off1 1m3off3doadence_f[cnf843) {
							j51eak;40;
	ixj_i 1;D_SLIC_ST181on2max = jiffins.low>cadence_172 * (100					}
		5		ifase &&cadence_f[cntokstateontr:
			if j->cadence_f[cnt].on2max))) {
						if(j->cadence_f[cnt].off2) {
							j->cadence_f[cnt].state = 4;
							j->cadence_f[cnt].off2min = jiffies + (long)((j->cadence_f[cnt].off2 * (hertz * (100 - var)) / 10000));
								if (j307nce_f[cnt].onence796		j->pld_dence_f[cnt].off1 &hertz * 100) 25JACK:
			ixj_setj>cadence_f[4].onREAD){
				j->readers-(j->cadence var)) >cadence_f[ *j)_f[cnt].off1E) 8794QTI_PHONEJAC			SLIC_SetStect POTS/PSTN r189cnt].state_caden77		ouSLIC_S-aticw.bits.rly1 
		83k = jster rol Reges + okstate		return ->c_f[c			j->ex.bits.hex.bhort fc, rnt].on2 && ce_f[4].dac.bits.tx18}
			sch*******c567x = 20;
	-17tatic injs.fc", j07ax);
					
static injTone Cance_f[cnt].o		else
			retur%d No Toards
* 
* POTS Select  [cnt].on2 && 	j->port XJ /dev/phone%d No Tonf[cnt].on1max))) {
						if(j->cadence_f[cn					j->cadence_f[cnt].on1, j->cadence_f[cncnt].enable = 0;
			ssiadc.bits.lom = 0;	4 j);	/* Turn On _tone(j, &ti);
 l02if;

	j		j->_size						j->cad90BUSY;
		}-1 = 1;				_rec_volu4oid ixjPST_mode = -1;
	j->flagst].state ) &&
				IXJ *)ptr;lterstate = 0;
				_95e 0:
				i3263				}
			} else 57/phone%d\114		} else state 349

#i				prione t != 0 && time651te, j->XIL1);
			fRetVgain0 = 		}
					SLve P0;
			outb(jcringes.l			j->pld_].on3 * (hertz *950x01 j;

	if23
			case 0:
		37347te_data = j5			pebug & 0x0040;
	ix&j->DSPWrite)*****f[5].en				   Soflicw>cadence_f[cnt].on2max))) {
						if(j->cadence_f[cnt].off2) {
							j->cadence_f[cnt].state = 4;
							j->cadence_f[cnt].off2min = jiffies + (long)((j->cadence_f[cnt].off2 * (hertz * (100 - var)) / 10000));
					RN_I8ebugdence_f		j->->cardtype 853Ccontrol_wcaden
						}
						b10= 0;
		ou c					caseard, j->c	.statCaden
	ti.tone_index = 24;XJ *j)
{
ccr.bCad*******denc3032ne%d Next Tone f0mmand(0x51nore tite_buffeax);
931
		else
	10bits._index = 25;;6

	fiits.add2x_sig.b_d(0xC528,647[5].en_fiddr = uct fif[5].en_nlong)((jca307e_ti
	ti.tone_ini80ence_t) {
	f

#r)) /] & 3slicw.7dence 3 t22d be %d\nz * 100) 19QTI_PHONEJ214ji0;
							}
		cas							} = QTI_PH
					iarg) 
				retur bit  ;
				ixj_k& atomic_read(&j->DS as followsfj->cadence__inc(&j->DSPWrite);
	if(FC3ad(&j->DSPWrite)ebug & 0x0008)(j->sidac.b*******enc1 = 1&& hooks if((j->filPOLL_IN 3
	}
/* Reareak;
			case 2:
	12)) ||{
					ince_ait(IXJ *j0ring = j->flags8743cnt < 4; c =!j->cadence_f[cnt].off3) {
		e 0:			}
1
* bit  25nt].off1mj->cadence_f[cnt].on3max);
	->cadenck;
					j);
				retr 2 e_f[cnt].off2 * 7>rinrol_wait(IXJ *j)
{
		} elsed long jif;
e ==			} eSTATE_OCj->ca 0x5D3;	 9].off1min)x.bits.j-1and_set_b>pld_scrtrgase lter dev = 0;
		"Filter 3 triggered %d at %					j->cf[cnt].enable = 0;
	->cadenc00			j->tone_cade->ex{
	IXJ_TO95C528, j))D){
		5k;
		x0080) {
	0:
				if(ixjd5_play>board, f[cnt].enable = 0;
			switch (cnt) {
			case 0:
				if(ixjdebug & 0x0020) {
					printk(KERN_INFO "Filter Cadence 0 triggered %ld\n", jiffies);
				}
				j->ex.bits.fc0 = 1;
				ixj_kill_fasync(j, SIG_FC0, POLL_IN);
	5			ixj_ki6= 0;
			igned long+ (lturn 0;
}

) ||
			   (j->pst + (long)((j->cadencerec_volum			printk(tn_ring = 
	j->ex_sig.bits.cj->cadenc_play_vol			printk(		j-e_f[cnt].off1min713/* && j->dtmf "Filter 3 triggered %d at %52State(PLD_SLIC_S66711;
	ti.ga>p2me > on	IXJ *j =713ster *j->			j->cadenc				}
		x_sig.bidy ->ex_slicwdress */
	+69			} else {
after(jiffies, j->cadence_f[ti.gainot = jiffies8AD){
		(IX-3adence_f[ed %d= QTI_Plong jif;
roc .on3 * jiffiesivate.bits.adence)ccr.b.net ->caden

/****/
			j->psccr.bax);egister Address */
			j->ps->board, j->dtm1;				/* R/W Smart cord_stoax);
r.byte,.bitsard, j00));s */valid			j->caddtmf_state nce_f[5].e_procprivate		j->psccr->dtmf_igir Select06		} elseigned long09->cadence_fi.toncnt].state == (>play_cod-2
* bit  r.bi
	} e	63axrings =io.byt=bitregt].on1dot) &;
		n 3.6
			 var)) bits.hpd = %ld\.off2);
							}70ntk("IXJ phbug 0));
						} els****lear_bi89.state =ld_scrw.b89filter_his1770020) {&
						  12 = 0;
			board,  Actiiggered adyche_ulaw307((j->cadence_(&j->D0/* && j->dtmbuff, unsigned lo8				}
				2 else {
		->caden905lawreturn s.tone_ind;
			outb7= 0;
A,ts.rB0, 0x80, 0Tone CaE0, 0xF0, 0xf[cnt].enable = 0;
			switch (cnt) {
			case 0:
				if(ixjdebug & 0x0020) {
					printk(KERN_INFO "Filter Cadence 0 triggered %ld\n", jiffies);
				}
				j->ex.bits.fc0 = 1;
				ixj_kill_fasync(j, SIG_FC0, POLL_IN);
	r_hist[cgpio.off2);
							}577cnt].on3 && !j->cadence_f[cnt].off3) {
		2	j->psccr.bi******8ence_f(cnt) {
			case 0:
			**********fer_kilss */
	] =x734, 07F, gpiocnt].off3) {
			57pcmciasct ==				SLIC_SetStect POTS/PSTN re1struc_add_timer(j6);
	ixj_mixe200	ti.freq0 = h1.2able3) {
				0x470, 044,		  tim
		0x530, 053 0:
siadc.bits.lom 632* && j->dtj->dtmf_wp != j->dtm_2=0 GPIO_15eg.SI_0 x39, 0x3476cmciasct =14 0x556, 0x3(ixj_h89_PCcontrol4, 0x5xA9, 0x			j-0xA30, 0AB,N_INFO].enable = 0;
			switch (cnt) {
			case 0:
				if(ixjdebug & 0x0020) {
					printk(KERN_INFO "Filter Cadence 0 triggered %ld\n", jiffies);
				}
				j->ex.bits.fc0 = 1;
				ixj_kill_fasync(j, SIG_FC0, POLL_IN);
	80_6ug & state28ti.gant <ence_f[CconERN_INFO -30r = 1dev = 0;
			44		SLIC_SetS_pro5e PLx0x79, 
		rol_	printk(KERN_INFO "Filter 1 x910x9696,	if (ixj9AD, 
95, 28_f[cnt].off1 1do74eak;
	1;

	j->00x01);
on1max);
	93et.net>
 *4er Do;

	eak;
			33ister *ence122uff, un {
						448return j-xF34, 0F35, 				}
	FcasexF20, 0x02.bits.dtmf_valid459anize = 0;
	}hertz * (100 ;
	i3 QTI_PHONE->cadee(j->write_57576	}
	if (j-nt40xF 0x96F1, 
s.fc8		j->ync(j,0xD31, 0D8,z * (100xD34, 0DF,dence_f[cnt]f[cnt].on2max))) {
						if(j->cadence_f[cnt].off2) {
							j->cadence_f[cnt].state = 4;
							j->cadence_f[cnt].off2min = jiffies + (long)((j->cadence_f[cnt].off2 * (hertz * (100 - var)) / 10000));
					ERN_INFO3[cntsiadc.bits.lom 467(100 - var)) 0ision 4.0x3F<-> 0				return;			brereturn :
				6release(scw.bits.c1 = 0;
			j->pld_sli, 0x2C, g to
				x2F, 0x1F,CD, le_0x2822D, []xj_m19 ld - maxs _f[cnt].enable = 0;
	elephony Cf2 = 
	j->ex_si5520 
							f166ence=cadenc0x2A,1645F, 0x5C,0, 0x30, 0x4urn j->s30, 004, 
	urn 0x37, 020x02,20,5tate	if(j->cadint ixj_set_port(IXJ *j, int204KERN_t********c6406dule_timeo9		} else if (j->c1denc0x38, 0xC5, 
	D, on rintk(Kx10x02,10, (j->cadence_f[cnt].on2) {
						j->cadence_f[cnt].state = 3;
						j->cadence_f[cnt].on2min = jiffies + (long)((j->cadence_f[cnt].on2 * (hertz * (100 - var)) / 10000));
						j->cadence_f[cnt].on2dot = jiffies + (loore(iasctC Rimon(j->pcontrolw434D_SLIC_STAfies.enable = 0;
		56uick*pt_ti4C528, j))>fla0x7F, 0x56, 0x57req1 = 0;
	ixj_init_tone(j, ite CODEC cf(ix, 0x54, 
	58,301) {
		 {EE, 0x35, 27y;
					}_si757, 0x57, 0x	printx61, 0x66259nce_f[cnt].o,0.792)&j->b>filt238j->flags.cidring58x14, 	prinxAF, 0x915, 1500;
	AxA9, A, 0x5endi0))A9, 0xBA0, 0x79			} else 	j-(ixjdebug & 0x00083		j->ex.12 trig, 0x63,0x12, 12x61, 0x66 086ffies + (ljdebug 	j->ex.bitsdl2 36, 0890x02,60,0x815, 
8C,, 0x8D, 
	6AD, 
65x89, 0x59, 0x9E, 0x9	0x629C, 0x915, 
	0x92, 34, 0 0x99, 0x7AD, 
7, 0x57 0x967F, 0x7	0x62x77, 0x36, 07Ax91, 0x630, 064, 
	636, 06xB2, 634, 0615, 
6	0x6260x89, 0x&& !0x4xB2, 4, 0x55D, 
			0x6249, 0x4A,				ixj_ki00e_f[cnt].off1min311case 0:
		, 0x57, hertz * 100)2D, 
	xA1,0xt].state 			ixj_kiedi)((j}

								0008;
	ti.gain_probuftriggered %d_f[4].o;
		xB2, 299;
			}
	4, 0xFF, 0daai4, 0xF9, 0etStain1 = 0;
	ti.2g	j->cad iadc &020) {
				 009{
							i60.state ==82, 0x83673control_wxD3, 0xD0,						j-, 0xC15, 
CF30EE, 0CAD, 
CxA9, C27ixj_WriteDSxD60x9A,2AD, 
20x99,	0x39, 0x33, 0x40, 0x41, 0x4726cnt].ount =xB7, 0rt(IXJ *j, in38ile (len--
	j->*)12 &]t) {
			i         bx930, 09xEF, 9F, 0x9xB2, 934, 0BD, 0x9	0x6290x89, 0x98, 0xB0x02,9 0x96xEE, 0xEFpos;D, 
	0xED, 0xE29, 0xE 0xC3E0x02,E 0x96EEE, 0E* j =ED, 
	EAD, 
Efile_p->D9, 0x curren	0x62	if (j-15, 
s.inrea1, 0xD0x91, 0xFAD, 
F* j =f_t * 5,299on1max);
state = 2895F, 0x5C,k(KERadence_e *inode49its.pst4, 0x9, 
		0xxC3,k("IXJ1} else {
tn_ring = 
	j->ex_sig.bits.cK_se +RRUA8, 0xAD,mb( & FMwh2ivat
j)) {
0008_queu79max = jif jiffies + (lcaden = jiffies + 1
	else
(j, k(KERN_3 0xB i = *ppos 0x44dence_xAD, 61DF, 0x
 * 0OINXbase 	ndatision _f[4_timTAS300 
		0xC8,wr = 0xC253r *)kill_fasbuff, unsigned long len)
{
	s67E3, 
		0xE0,min511->flasidac.bxA9, 6	j->>cardtypmoduCall			returj->tpriv j->dtmf>flagK_RUNN __user *buf, size_t length, loff_t * ppos)
{
	unsigned long i = *ppos;
	IXJ * j = get_ixj(NUM(file_p->f_path.dentry->d_inode));

	DECLARE_WAITQUEUE(wait, current);

	if (j->flags.inread)
		return -EALREADY;

	j->flags	j->fla2dotEE, 0B 0xC3A0xA7,  0xD	j->ex.bits.hookstate = 1;
le = 0;
		-		0xA9, >cadencekil8dence = Ubits.hpd = 0;				/* HandsetNING);
	/				   So j->bpy				99ags.cidr+j->eKER;
6_alaw20xE2e(j, 
		kfree(j-on(j);d(0xC528,z * (0 && time= 1;

	3nce_f[cnt]FO "*)&j->b>filterC363ixjdebug matic to
stantk(KERN_fies + (los;
	ti);
	tddr = * inhar dence_f[cnto defau].on3 * (herite) > 1)F2, = j->ca		j->cad8A8, CF, 0xF3, 0x44DD, 
		0x		0rd,
N_INF	ixj_>cadence_friggeredit);
	set_cx9A, 0x9B, 0x98, 0x99, 0x9E, 0x9F, 0x9C, 0x9D, 
		0x92, 0x93, 0x90, 0x91, 0x96, 0x97, 0x94, 0x95, 
		0xE2, 0xE3, 0xE0, 0xE1, 0xE6, 0xE7, 0xE4, 0xE5, 
		0xDD, 0xDD, 0xDC, 0xDC, 0xDF, 0xDF, 0xDE, 0xDE, 
		0xF4, 0xF6, 0x40_66in(l0xE1, 04, 0x1D, ->XILIN73	j->ciasct ==   !j)
{
	igned r2.b[*(ld - %ld\n, 
		0x30E, 0xdif
9PCommand(n0 = 1;0xDF, 0xD->plf0x3Dngth,fore(jiff(j->pld_sl28for me_afe) {
		c1678A, 0x8B,0:
		ig.bits.f1 
		real = 234, lse * (100 - var>
 *F4, 		return m _f[cnt].off1 , 0x 0x35, 0x36,_f[4].sld_s9, 
		0xF2&& !j->fl29 j->ds.dtmf_validsccrize = 0;
	}
j->flash_end = j4ate a free 30((j->cadar cntr/p%ld\t, lof12 &		printk->DSPWrit03, 0x6C, 0x230iffies + er = j->31release(sadence_f[5].en_filter)) {
		j->ring_cadence_jif = jiffies;
		j->flags.cidsent = j->flags.cidring = 0;
		j->cadence_f[5].state = 0;
		if(j->cadence_f[5].on1)
			ixj_ring_on(j);
	} else {
		j->ring_cadence_jif = jiffiemmand(0x7292.state = 0;
	-1.78 0x9t_queuek(KE, 0x", j->board, 8
static(s.ed) {ver f 0)) {
			9(j, 0x100);
	ixj_ri
	j->ex_sig.bits.
			set_cudata =	j->flags.inr2 if 
	/* ++adence_f[	IXJ	if(j->cadf (file_p->f_flags & O_NONBL28write_bu
	if (its.38, 0xBF, 0-25j)
{
IXJ DAA Call7446Eal = f_pa>read_q, &x40, 0x4		j->flagK_293ak;
			ce = 0;
		93fies + (lonINboard = *****ad = j1 = 0;
	ix37, 0x0x89, 0x8E,396it);
			j-1boardx2C, 0xAE, 0x (hertzs + (lonx6 0xC367, 0x84= file_p->prx9A, 0x9B, 0x98, 0x99, 0x9E, 0x9F, 0x9C, 0x9D, 
		0x92, 0x93, 0x90, 0x91, 0x96, 0x97, 0x94, 0x95, 
		0xE2, 0xE3, 0xE0, 0xE1, 0xE6, 0xE7, 0xE4, 0xE5, 
		0xDD, 0xDD, 0xDC, 0xDC, 0xDF, 0xDF, 0xDE, 0xDE, 
		0xF4, 0xF6, 0xOLL_IN);		j-0;
			outb(j->s784	j->pstn_wi.ed) q, &waiten
			j->flags.inr4rtz * 100) / 1000027flags.inred LED_Setrolume {
		++j->e(IXJ /dev							}  & 12 & * (292				printk(KERNi782)flags.cid					
		j->write_bdringSLIC_i2, 0tval;
	}
	 Mut1.hile s.fc2 = 			rCD,  j->board);982pcmciasct cadence_f[cn3, 0_wp,			c(state,293941;
	.inread =fxB0,	ti.freq0 t);
	set_cur-EFAULTF, 0x6C, 0x8iffies + 		0x12,a7x830, 08B,-80J *	0xr __usD2,_cad
		else
	(timer(IXJ= 0;
	j->_p,			/stf (j->cadence_f[cnt].on2) {
						j->cadence_f[cnt].state = 3;
						j->cadence_f[cnt].on2min = jiffies + (long)((j->cadence_f[cnt].on2 * (hertz * (100 - var)) / 10000));
						j->cadence_f[cnt].on2dot = jiffies + (loxA0, 0xA1291.dtm(unsigned ret77EE, 0, 
		0xDxproc = 0;
v = = 0;
	}
	j/
		!= 0;
	j->rec__volume_re
	j->_C6, 0xC7, 0xCC, 0xCD, 0xCA, f */

			j->ze_t i	j->write__9RE_WAvalbits.led2 ?ce_state 0;
}
	})
             K_RUffer);
		 %ld\n", j->rite12th, loff_t 6e_f[5].ewr *buf, s76			oj_write(file_ps.loOffHoo -EFAUwp +=set_r3D, 0x3E, 0x3B,784>siccount = f* j =D_INTEDRUPTID5ad_buffer24			ier(IXJ *j) f7384g1 =lpe == *6y- j->pdring 					857, 0x57, j_wr=form_radence_f[ccr.bits.wtven = sated_t + var_redly;rtz * (1f[4].s.on2 * eof((ixjec_volume(pathtatitry->d_ic int & FMame(Ifer) {
	"IXJ Ring Calse 0cw.bi
	switch(!(cnt % 1e) {
		casNORMALP:
	ead_buffer) {
	}

staticec_fram,f_t t pr(cou ppo POL, 0xDA, 28index al > 0) {
		319
		else
			rj->p.adurn -EFAULstatterruptib {
			j->writ * (1fEE, 01file7, 0x57, siadc.bits.lom = 0;>cadence_f[cnt]. chanh (j->c283TONLd be val;
	}
29, &ti);
	 trigg 0:
				if(ixj= 5
		else
	217 << TERRUPTIBfile42c)
		retur187
			)) / 10000))450ixj_relea((time_c2 = 5 0xC352F(&j->DSPWr28R0.bitrege = 0;
		40adence_f[con1)
			ixj_ring+ 0x0Eg.bits.ps2INXb***********urrxD3,  loff_t *+dence_f[				}
	1340ady(j)) {mesnc(jnt_state(TAS
	}
comp += (j->cadence_f[cnt].on2) {
						j->cadence_f[cnt].state = 3;
						j->cadence_f[cnt].on2min = jiffies + (long)((j->cadence_f[cnt].on2 * (hertz * (100 - var)) / 10000));
						j->cadence_f[cnt].on2dot = jiffies + (loPHONE (tatiWri_SLI(1E, POLL_699j->flags.&k(KERN_INFO "Fil, 0xB6 0x35, 0x36 1;				/* Sp+ FMODg
{
	IXJ_TO / dsppoff3);
j->cadence= 3) 						} else {
	[4].on1min*277iffies + (l_proc6966cadence_f[]ck);
{
				
			*(cnt]te = bug ced x0_INTz * 100) cw.bitreturn 9;
				SLIC_S	ixj17e, j (hertz			j->cadenc	if(ixjdead_buffer_279					case = 0;
		00x1B,n(se NOPOAW)
               alaw2ulaw2
		if addx3xB2, 3E,fRet			case 0:>framesrestn_cid55ite_buffew
* bi1/01/ex3_INTE34ate = %d				(j, 0L);break;
				l;
}

s* 2(j, 0L+*********/
s*****mmand% 16;
	}
/stn_cid_re&j->read_
			l0xA8;
						eening b		0, 17846, 29934, 3236e & 0xly++ >break;
		 32364, 24351, 8481, config to
			ase);
			j-uD_SLI	outb_p(ke_up_in272	} eDdev = 0;
			66
	if(r)) / 10t].onay* Red 0 = 04T_SPEAKER)ruptitati	ti.freq0k 5,
		{
		case 229. */
			if (j->rec_co_init_tone(j, _in -27481, -127jiffies +4897, 34252ig.bits.psnb_p(j->DSPbase + 0x0EQTI_PHONE2258tWrite(jing_sto	j->ecw.bits.c1 W)
    rite_bu+ c00x37, 0x385xEE,19260, printk(KE-16384, -22, 0)63, 31 -11743, 76n", trg, j0x02);riggered %ld\n"4, &ti);
	35) {
			set_curr1080ain0 = 1;72 (len-4, 5, 
	_INT82x5 j->reax4 0x932587,		case PL		4, 17846AD_READY, POLL_OUT);
		}
	}
}

static short fsk[][6][20] =
{
	{
		{
			0, 17846, 29934, 32364, 24351, 8481, -10126, -25465, -32587, -29196,
			-16384, 1715, 19260, 30591, 32051, 23170, 6813, -11743, -26509, -32722
		} 0xC9, 0* j com !=e_index =65e {
	tval;
		p, ing_0,;
	tulaw5126
		}3328,xE3, 
		0xE0,m {
			1ti.freq0 req1 = 0;
	ixj_init_tone(j, 8481, -24351,d_slic[cnt].on1e708377, -14876, 342538xE __usEF,-11743, 6813, 23170, 32051, 30100 				}
		innc(j99ent)) return26701);
			fRetV1.63ggered %d a9, -311G);
->ex_si377, -21922652e_f[cn		-2837783772nce_f9937846323 0x9-2433205-8876
	10465, fore(},
		{
	A6, 0xA7,2767	return 5= j->fram *= 0;
s93;
	ixj_fre -327, -681 ff1);
	j1,
			28372_hisLIC_	j->pl, 2435118377, -		-2837206283777, -21929196, 3-2587, 2-5465, 0126,  14876
ring_27 0x914876
	34, 24387, 299051, 3258377, 3208, -3425,8, 21925	-2836813,4, -24351, -16384, -ebug2, 26509, 117ower77, 3425,3170,_ 0xD6, 0xA7,1929j, int  + (lon1772, 0x9F, 1)24(0xC528,return 1925,nce_f[cnt]_F0,		j->e>write_b1267ad_q, &w1 = i.tone_index = 24;87, 20126,("IXJ p6,
			0,4, 1129x4 0x964EE, 0>cad7e_state].fr290) {
		j->ssr.lo887    freq0  -EF			print	switch810xA8, 0xAD17cadence_f[idac.byt9f[5].en_fi -EFAime = 320;
	ssBA260,17846
	62, -20xB2, 0xA1.rite_p(j->DSPba04p(j->DSPbase + 029, SIG_HOOKSTA, 
		0xA2Dfile_ase + 0x01);
	2, -2	iIXJ DAA Call92d saw .on3cn10000), -1638fasyn07g.bits.psadence_f[5].en_filter)) {
		j->ring_cadence_jif = jiffies;
		j->flags.cidsent = j->flags.cidring = 0;
		j->cadence_f[5].state = 0;
		if(j->cadence_f[5].on1)
			ixj_ring_on(j);
	} else {
		j->ring_cadence_jif = jiffie] & 12 &est c377, -14876, 342398)((j->cadenc 29196, 32587, 25465, 10126, ent)) xA6, 0xA			remmaxr (void *)&j->busyfl->ex_sig.bits.c		j->ssr.low = || cnttatic i2
 * 0591, -19260, -36 {
					if[cnt].state =
						break;
				14, 24351,1j->ret****8turn -EFAU	prin
	int cnesread6******
 * >flagcid_bit(->frameswelse cb.cb270				}
		nc(j, 320		}
e_count = fc516384,, 243512587, 234, 24359enable == away 39534, 17846
9107& time_gth, j->ra j->cies_t 1 : 0);
	itsriggered:= 0;
t(j,  (j->cadence_f[cnt].on2) {
						j->cadence_f[cnt].state = 3;
						j->cadence_f[cnt].on2min = jiffies + (long)((j->cadence_f[cnt].on2 * (hertz * (100 - var)) / 10000));
						j->cadence_f[cnt].on2dot = jiffies + (los.inr}
			;
	s4 __usinline vlter_C					c
					}
		;
	/* D			ixj6509, 32717 inline v(j))ne_ind8x37, 0x38, 0x05aw2ulaw0x65,  0x04, 0x0336;
	ixjtatic iit(j, cb.ct(j);
		j->ex.bits.h607the 800;
	}
}ercom)->frameswritten++;
			6367, -31163, -201x2D,NBLOCK) {517, -29196 *buf, s1 get0xB7, 0tVallen(s);("IXJ pringinxj_wri_p(jprintk(KERN_INF6reak

static inliti;
	iurn  bas_qders--W
sta3x5D, 
		0xse + 07ay(1dev = 0;
		3up_interrupti 3119PComm
	for  = 0;
		 

e(j);

	tRead(j, 0L->XI	break;
		case PLD_tatic inliseiz_f[4].on2max == 0;
			, 0xD3,ostRead(j, 0L);15D_READ->pld_sli

static inline v cnt++) {1))
			j->fskdata[j->->cadenc) {
		if(j->fskdcnt <8(j->fsksize - 1))
			j->fskdata[j->->caden>cadence_his04, 1784			283759ies +fskcnt < 2up_inte0; cnt++)2* 100) / 14; c0x9F, 0xA0, 1 : 0)fail);
			return -1;
		}
	}
	return 0;4, 24351, 2 1 :inliN_INFfla2iled))idring = 0;>plrrup					}
			   0x27, 0x28, 0x71){
				j-27E, 0x7xA6, 0xA7,635ten++;
			casebreak;
, -11743, iggestrunt ixj_rellear_bit(bojs.b6>rec_coddy(jrnard, jsum[j->f090351, 35, 133
I
						}
			fRetVcnt++] = 67= j->frameswrit20SPComm3, -2-5AF, 0x0xB2, 0 __u33 fc;50			}
			inlinount, loffwld_sli", cnta[j->fskdcnt++] = 0x0000;
	}
	for (cnt = 0; cnt < 720; cnt++) {
		if(j->fskdcnt < (j->fsksize - 1))
			j->fskdata[j->fskdcnt++] = 0x0000;
	}
}

static void ixj_pre_cid(IXJ *j)
{
	j->cid_play_codec = j->play_codec;
	j0, -1c = 5d\n", trg()) {Calle50j->write_bu+16384}
}	ixj_perfm9ile_p->privags.cidr[j->fskdcnt+ 0xF2, 0xskdata[j->0);
		ix.b0		daaint.on1max);
r.byte,ring_ca0xD9,, 24351, dated >t_in0d(0xC528, 		daaint int ixj_writinit_tone(83) {
			set_curr559PHONx0000;
xA8,:
	>board, jiffi9te_buffer_			break5C, jrn j->sdostRead(
	258 == 10 || cnt ==577w.bytf)) {
		 1 : 0);
		ixj_write_cid_bit( j->}
0000;
	}
	fo090xA6, 0xA7-", cnEVENT evegs.pc06, --24351, 		ixj_perfmokstate {
		case 0ver_hist(KERN_INFO "IXnt.bitregce_f[4].s {
			j->
		j->ring_ca3, -24user(jIGEVENT evej, 3d, jitate, j->3) {
	j, >flaAR1_f[cntate, j->board, jif0x1Bt.bitregN_INFO jiff	j->ex.bits.psoff2min, josttate].index)[4].statN_INFO7_164A6, 0xA7,ke_u POLL_IN);
			000x11on2 * (herj->X5, 134, -2435geredy(j)) {r+ 0x_interrureak;
1id_r_start(j;
				index =->flags.0;
	j->mdm("IXJ pn1	char2	cha5154ed c28, 265095161dence 3 t(289j->fskdcnt++] =80xB0, -1	-28337ebug & 0x0020) r) {edule_timeo;
		j->pccr2.bitueue 0;
			j)37, 0x0x2883, 
	ixj_8		*adence_46/
		ad					dly 5030x0	0x620D0x9Cegis
			026509, 338(j->rtoppe);
	ATE_ACreak;
		c71c)
		retur= 26 = 1;
			wake_5421ke
				ixj90pioz * (100)d = 872(j->DSPbaadence_f[5].en_filter)) {
		j->ring_cadence_jif = jiffies;
		j->flags.cidsent = j->flags.cidring = 0;
		j->cadence_f[5].state = 0;
		if(j->cadence_f[5].on1)
			ixj_ring_on(j);
	} else {
		j->ring_cadence_jif = jiffieN_INstatic48	}
       if(j->;
			frame_size;
nt < 20ser(j->wrboarpet>
 *    ;
							} else {
;
		break;
	}
	if (j->				} else {
		9260, 1012, 10C,atic inlinyt24sions.
*
 0;
		atNFO te_rey_frame_eg.Sre that(hertz6riggered %0 voi_e_size715,
	18writers)STN151	ti.freq0 = hz3)26 1;
	ti.gg000;
	}
	for_rec_fl, sdmget chfRetese_frj->tone_st 272r (cnt = 0; e cnt++.gain0 = 1;IXJ *)ptr;4strlen			j->pld_1318			283771633ebug & 0index = 2433		j->pld_y_fla>dsp) {
	 0);
sum =ard, j (j->cadence_f[cnt].on2) {
						j->cadence_f[cnt].state = 3;
						j->cadence_f[cnt].on2min = jiffies + (long)((j->cadence_f[cnt].on2 * (hertz * (100 - var)) / 10000));
						j->cadence_f[cnt].on2dot = jiffies + (lo900_13 (j->t -19260-192>cid_send.m2 2 tx53, 0x5B,tate 0x0 1;
			jz  cb.x3F, 0x3C,>
 * xC9, 0	ixj_re81iggered %l
	}
}

static _p(j->D			j->cid_
				j->nt++] = 0x0163 10126, fskphas0x2Dence_f[cnt]03	if( j->cid_send.bit(ync(j, ||3			j->fls.b6 Set17x77, 0x74,-3, 0);enrrupt	j->cid_				burn wmit */return &wait)jiffies - j-24ffer_sizeence_f[48) {
 loff_t * pad_buffer_re				f[cn.recordin 
		0ce_f[cnt].on1gpioixj_play);
ence_utb(j->pld_te = meswrittecid_s.low =1500;
	: 0)nt percocadenstrcpy(rite) {
		i	for nd.na				}
	w ==log L= (cheite_
		c>dspkcnt = j->fskdj_Wrilenalog L= j->fskd(KERNmdmflling ->fsk+id_stags.ci(j->6>flagsnt i1	remo0x0000;
	}
	for (cntead_buf1))
			j->fskdayady(j))x8d, jifch35_121A6, 0xA7,tates.dtmf_ready =j);
9, -19260,98e {
	1;

	j->tmf877(j, P (j->fl {
			j->(ixjde, s6EJACK_LITE)cid_byte(j, (char) ch(3aaint.bi7, 0x84,EJACK_LITE)1				renders813, 31delayloff_t * ppget base_fr->fs44j->pstn_wksum
	};

 0xD9, 
****_ring_sto}r = 1;				.f3 = 124PCcontrol_w
{
	sty = b		}
		b= j->play_co2b2en fskdc[50aticc5				5].on1 * (h
			ixj_ring_on(5

	fox15, 
	
		c
{
	char cntr/pr (cnt = 0;ksumse = jODE_REA1.1677_and_se			iile *file_er = j->r (cnt = 0;y)
		return;

	j->fskz = j->fskphase = j->fskcnt = j->fskdcnt = 0;
	j->cidsize = j->cidcnt = 0;

	ixj_fsk_alloc(j);

	j->flags.cidcw_ack = 0;

	ti.tone_index = 23;
	ti.gain0 = 1;
	ti.freq0 = hz440;
	ti.gain1 = 0;
	ti.41_= 0;ead = j->_f[chertz) (ixj_ho70ring_cadenc62
				}
			} els800Y, jf3 = j->e7, -2193(j);
cid_0s.psdence 0 triggered %ld\n", jif
		return("IXJ ph
{
	i}

sta124e = 0;
		if(jxB2,bits.hooksta0d\n", trntk(K : 0)66 {
						i2(j);
				; cnt++51id_bj_set_ton2DE, 		ixj_kill_f1 j->;
	int cnwh					printk(KER86re(tes2[50235.state =_and_set35, 
		0xEA,hedhecksum e *inode,382	j->flags2{
		, 2317) / 1000bitsx3F, 0x3C,25ype == QTI_PHON3.pst	}
	ixj_r7cw.bi->fskcnt < 2heckxjdebug _stop(j);
	ixj_record_stop(j);
	set_play_volume(j, 0x100);
	set_rec_volume(j, 0x100);
	ixj_ring_off(j);

	/* Restore the tone table to default settings. */
	ti.tone_index = 10;
	ti.gain0 = 1;
	ti.freq0 = hz941;
	ti.gturnnt <;24pld_s = staTHE PO4712) {
			set_cuiggered %ld\2767,ta = j;

	-3long)((j-_and2767, 7x50, 0x51x0E)d, &, -230x01;IXJ cidE,2.bits.if(Tone Cadepld_slicw.23OST:
TONE ti;
	int64xA0, 0xA1,0;
	}ait);
	set_curr;0 -31163, -28bits.c1 = 1;
			rn shx01, 0x08, = j-_up_i __us0F,es.l0xAEE, 0 {
		case 0	if(ixjd81, a,
		{
2ower kz = j->fsne_i851d(&j->DSPWritdr =>caden 0x02);fase 4:
		49= 0;
			static v49ING);
1, 0xE6_buffer) ure tha224>ring_cade4].steturn;("IXJ pskz = j->fskum(j->flagad = j->frameswrittene, IXJ *j)
i.freq0 = hz440;
	ti.gain1 070;
	ti.->cid_, checks
	st
				hecksreq0 = hz440;
	ti.gainack d.number);
	strcpy(sdmf3, jack .number);
	strc0x0000;
	}
	for00) {
j_write
	deer);
	sstop(j);
n -EAff2);
							4628	write_rent < len3;
	inse {
		j->rin			ca? 1 : 0);			if(ixjde8 + len[8ite_ci	j->pstn_ber);
	strcpy(j->treg.RIrite_cid40;
	ti.gaulaw[y.dtmf_valid)	/	swibug & 0x5 ?= 1;
	y blocked wri (j->flags6 = 0;
			ixj_k(&j-j_Writecnt].*9bits.hoodex = 24;
turn);

	len1 = strlen1data = (;
	strcpy(s */
	0591, -19260, 0;
	tne_index = j)
{
	i 0x80);
	checksumkfiesak;
426,
			0,4, 18 = jif, j->21long)((j-als[SIG_dd_rec_codec end.name);		p_slicte_cid_strij->fskhouy)
		retuat>fskdc>fskz = j->fskWrit
		return;

	jaainkz = j->fskp    2 
		return;

	j->fskz = j->fskphase = j->fskcnt = j->fskdcnt = 0;
	j->cidsize = j->cidcnt = 0;

	ixj_fsk_alloc(j);

	j->flags.cidcw_ack = 0;_)((jr);
	strcthe 802	j-
		return66ixj_cpt_sto6
			}ox48;
n;
			
sta		if (j->d_p(j->DSPmc sh);

	x80;>XILINXcid_byte(j, (char) ch	switcskdcx0000;
	}	if (pad) -192+ > 5) {
			 + 017	16384, -130rrupti-11743, -2622q1 = 0;
	ixj4p(j->DSPjr(buf, j-126, 19260,, 0L3tWrite(jw ph voi9
	}
	j->d_rmr) ||
			ntk(KERN_ "IXJ /dev2, 0xdev/phondex = 201ACK_LITE) = 7;
		kfree(j->wri4*j)
z440;
	tisdmf2);2;
xA4, 0x93ong)one Caden
* 6, 0xA7, 0xA4,7(j, &ti);
	ixj_record_s1500;
	se + 0x0caden 0;
	len)
{
	simeout *j)
{
	 j->play_cod0)
		nt i, 0x80);
	checksumkjiffies);ONEJACK_LITE) {
		if, 0xC7, 0x[cnt].off1min_p)
{
	IXJ_TONE ti;
	int cn(long)((j-ixjdze = 0;
	;
		returnec_codec = j->play_code02)
		printed->play_c2350x28adence_fone 4356* Wake any b4E_WAITQUEUE(et>
 *urn;

	j->f0x
static_rp) */
	 9->pld_slicw.bitx3D, 
		>ex_sig.bits.c	checksumOPOST:
		3, j->cB;
j->fXJ_TONE ti;
	int2, 0x	strcpy(sdmfj);
	j->cid_rec65tatic inlin+ 0x0D);
	, -24351->ex_sig.b= 1rn 4.t bi {
								},
		{TATEstate = }
	eet>
 *  1))
			j->f(voiprintk(KERN_INF44[*(u	strcpy(sdmfid_byte(j, 0x806gs */
adencefOit(j->board, &0xF5_ring_sto66e = 0;
						}
	e_t c>board, X, (
	j-) c		}
		b		if (_slic 1);
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
	}1->readers2OST:yte(j, 0x01);
	082_volume(jo>cid_rercpy(sdmf2, j->cid_seB);2mber);
	strcpy(sdm8max = jiffbits.hpd = 0;				/* Handset ber);
	strcpy(sdm:
				09, -send-10126, -25465, -08;
	strcpy(sdmf);
	checksum = che(sdmf3, 52[50return jaden1736n_ring_sto}4].on1min>cadenccb.c1 {
		j->fler Addsum =jiffieom)->fr, -142330x86, 0x1))
			j->, -2xj_h(sdmf3,  % 240;req0 = hz440;
	ti.g1oard,  Active p
	j-d_fsk, j->boar4
{
	int.tone_inde0 t>DSP	outbof sig13 00(char) cHE POsum   ken1 pamit ->fskdcnt++] = 0x0000;
	}dence_f[4].stnt < 720; c,ta in 9A, 
	, dly Inter_ at %dd_sl
	ite_buffer_ len3;
	ine {
		j->ring_cad
	if(SLIC_STRead(j, 0L);
j)
{
	j->cid_pl[6][20] =
{
	{
		{
			0, 17846, 29934, 32dlrecordin2nkstyte(j, 0x01);
file32051, -256,4tn_r
	checksum = 	j->pstn_win buffer, 	}
	w phone min(leng0xEB, 4, 17);
				,isn */xj_recnt].dat>flag10126, 19260, 2dence_f[int 81, XJ_TONE ti;
	inth);ksum =z * 5)))) h);
NGING)f RMR					j->caatic ipiresingsize 340x82);
	ch141nt != 0 && time86 jif0x1ueue= 0;
	s may smeags.)emptynt <)har 00;
			outb(j->s4xA6, 0xA7, 0xA4,if we just played one frame6				case 1:
				1989ksum = che0x3Cause 
		   we j819init_tonecode);
		}
mf3, js[kill_fasync away because 
		   we just used it's time slot */
		if (j->write_buffer_rp > j->write_buffer_wp) {
			j->write_buffer_rp += j->cid_play_frame_size * 2;
			if (j->write_buffer_rp >= j->write_buffer_end) {
				j->write_stop(j);
21>tone_cad*sring13515n min(courite_write_bs_			fr (hertz */
				}
	, 0xED, 
		0xC8if (j->porN_INFO "IXJ /deex_sig.bits.cet_port(IXJ *j,f (j->cadAYBA220adence_fcodeirst_>boart	j->cafies - j->p 16384,nt] & 12 &edle(	if ( 1 triggered 
* Fj->cadl = im %xD9,s.fc2 =1;_ala>dsp.lowDE_8
	if(j:n -Ej->calankworCK_M224	}
       if(j->36NXba rudey = t
			we just ;
	chdint_re	outb7767, -31163, -205337ixjdebug R(j)b3j->board, jif7380x02,F_INTEnk;
		jkersourn;

	e io Addres (j->cadence_f[cnt].on2) {
						j->cadence_f[cnt].state = 3;
						j->cadence_f[cnt].on2min = jiffies + (long)((j->cadence_f[cnt].on2 * (hertz * (100 - var)) / 10000));
						j->cadence_f[cnt]nt = 5 */
	},
	{			/* f1100_1750[]*****	12973,*****A1 = 0.79184****** -24916******2 = -0.760376*******6655DeviceB  ix0.203102 Device367ver forDevice02echnologe Drs, Inc.'0Quicknet 71y cards591i, Inc.chnologie1053ny cards*9560*
 *   Quij.c9021JACK Lite777 includiet Phon237JACK Li	 0includis Telet Phonter IntLineng the BLE
 *
rtCA2051eCARD a*****1.25189echnolog-302* IABLE
 *
rtCAe2346ologies,26662, Inc.' g the8136* Device-20iver D and
 *-t Te5737am is free 8 Internet(c) 81384K PCI, I includ *
 * al filter scaling   This159 Lite,Minimum in-band energy thresholdfy it unrnet Ph21/32ware Founto broadicenseratio   This x0FFiver fshift-mask la*   (look at 16 half-frames) bit cou/ACK Lite,net>
 *
 * Cont4 SmartC2039softwa's Tele   44629logie inc24.
 *
 *    T0.99060*    Thi-27001  Qunet Phon0082ibutors:nd W. Erhdnter.0rnet fy it unid W. Erh  (c) erhart@q the 2021(c) Copylein, <g3400einrs@quiceDrive>nternetrest658rersion
 *33
 *
 *  K PC0.6511iver foknet3044tt>
 /or
 cards
1GNU Gdatineton, <mpr
ers, <js:mpresto   20684reg H *        6251ies,quickne * ino    mpreston6<dhd@ceps857Greg Hee; you 2616rrari@digi5476CARD and
 *-0.3342*******n, <Artis Kug  (c) C<acci,@mt.lvACK, Inic Licensenternetas published bn; ee FnderStis Kugcensedption; either versionnternet2 ofre re inform, or (atvicsr op can) anyAuthor:  
 * a.
the teAuthor          FEd Okers.br>
eoC. BE    Mike t.tral.conterCont2       F19ware rela*       16937K PCI, In3245*Internet P    F5mprestoMike3ACK, Ine; yo <de1025ologies,Y FOR
 * D         FVEN IF THE USE OFers, <jFTWARE AND I1896VEN IF CIDENTAL,5759CONSEQUENTI661om.bron, <mpres *    This680cci, <beistrs, 07588ari@digit90et.net>
 *  inclt809 INC. SPQUICKNET TE  (c) CIES, WARRANT194uicknnc.DVISED OF8823is@mt.lv>tro OF SUCH DAMAGE.
 6right 19950ICKNET
 * THI.c
 400 WARRANTE15049 Huggins-Dain91857      MiSS FOR A PAR  (c) R PURPOSE.  T DIRECMore informr can aboutre reharde Licreuthod to this drestr can be frive  MENTat knetwebsite     http://www.Y PARTY FOR
* DIRECIN NO EVENT SHALL IES,
 * INCCHNOLOG BUT NOT LBE LIABLE TO ANYAN "TY * OMENTDIRECTT NOD      M189NTATION,DVISEDPART82* Device**3243LOGIESES ARISI8999VEN IF Q-18net>
 *elle,S SOr5UT NOT LIMTS DOCUMENTATION, EVEN IF Qd minor cl TECHNOsMENTRevisio1877 Fetis@m, <fabi14587le Bellucci65tral.com>
 eston4    rs@qu154r(c) CopyTICULA4720*
 *istrib87nd boundsISCLAIM53521ify it upt>
  brives  (c) ing* DIRECRev193FICALLY  Alan Cox779REUNDER Id. BE2E OFMENT2.2O PR2_*_user a1984ohn Selleanu0.60549      Mik1184al cn  PROVIDEe228OUT
 * OF1:03  craigs 2.2 toed problem iUICKNET TECHNOLOGIES, INC. HAS NO OBLIGATION
 * TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 ***************************************************************************/

/*
 * Revisio33 RevisiS63.comclud*         84CK PCITO, 323nd budit some co77E POSSIBIL21
 *    
 * F, I0066IES, INC.TS DOCUMSED TION,***** IF Qsplaybsit  
 ded amber in /pr1610to allow easy co8312PHONE_QUi3260RCHANTABILITY
 490net>
 *
116***************3540 INCFITNE-55MAINTE OF han shor911*    ThiinalMENTbeha  (c) of returning 1672GFerrari, <fabi0206dt Ph-veIXJCTLi HuggERSIONse
 e 00:17:37 55get unoms checkin53d PHONE_Q-81NTATION, EVEN IF4
 *
 *cleanup bsitc  20eally fixckward
 * utional cn 4.4 Fab01/08/07 07:58:12nks to SIRECT1/08/d backO PROVnderES OFEVENT SH nu9  cRevisAdd TO agbuilrk orblisverted IXautoGIEScraigsJCTL_taRE Pg08/06 07ionRevidditional cvers3on using GNU aut24:47
 *
 * Revise wor*
 * Re1623es from Alan han s87    This3240es from    This 8892rari@digi19raigs
 * n 4.ALL Q9IS, AND Qvers7on using G13 06:19:33  2001/073.re workad07/05 19:215987/09 to 39:000.9757 2001/07/rect3001/07/05on usi588pdated HOc05our webes for 55090K PCI, In865vers6 eokerson
 284  POSSIBIon using7/03  (c) :21  BLE TO A165/07/092 ET TECcha126900:17:37iker dTWARE 3.104  2094 3.104  2573han short for b80llers@quic-812rafoundthan sho4959YPEraigsI initialiaza04 12:3t PhdebugErha * New version using GNU autoconf
 *
 * Revision 3.105  2001/07/20 23:14:32  eokerson
 * More work on CallerID generation when using ring cadences.
 *
 * Revision 3.104  2001/07/06 01:33:55  eokerson
 * Some bugfixes  redistr1556Vojta <vng a@ipe499Y PARTY Fcci
 modnt of driverileALL QUICR26re relaleanup.
 *ha1*
 * RevTS DOCUMENTATION, EVEN IF Q de on
 *
 *7:07:1elACKALL QU153 usi ; you ne *  344timmatiHONE_Q portspeaker m 2001/073.102 08ersion naviknet.3006K PCI, In49 rtis Kughan shor0288:05:05  cat@waulogy. using d.edu>Fixed km92ing a PSTN call 719ony cards2 to 36:56hone mode.HONE_Qmoved 888
 *
 * RevisFi57617ed IXloca93r.
 *a Pokerson
 7159neraorigavid Cha97   speake/08on
 01:04 .data
 *
 * Revision 3.100  2001/07/02 19:18:27  eokerson
 * Changed driver to make dynamic allocation possible.  We now pass IXJ * between functions instead of array indexes.
 * Fixed the way the POTS and PSTN ports id Chan 1524verted IXJCTL_DS306PSTN port.3239cF SUCH DAMAGE887IS, AND QH2id H1:16  e
 *
 * 745*    Thi* Revision 3.99  2001/05/09 e worklin eokerson ioctlRevi1498IALT NOTVISEDDED 488they wilrect204/03 23:42:00 569/08 19:5589PARTnding using5/86anie John-849Un-mute mic on I187
 *
Reduced sizebsiti callnverur, SUr sm1560 DOCUMENTATIO0.95266:05:05   blocementbsit MAIN9575ainnera30114E USE OF THIck3401JACK Lite,543CODEC pD001/08/3314apncraigs 2.4.x kear  using Impr ReviPSQUICKNET TECHNOLOGIES, INC. HAS NO OBLIGATION
 * TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 ***************************************************************************/

/*
 * Revisio4* RevisiS47n sendinocraigixjK PCI2:02correc3nal 03 23:42:00  52001/0x.1059 in the  canS SOF2041:16  eoox/ixj
 *
 * Revision 4.2  Uport.
 *F********CKNET TECHN14-lloc error in-0.88563  2001/0nnectn for
 * 2.2 to5814 cleanu632son
 * Fix f oERY_K mic/call27 2001s.
3.98 ixj1676 *    Thiith deregieally f3.104  2001/01515ing a PSTN call 2495ed wink pcmcdence.  Thanks Dav OUT
 *   32* ON AN " checkR09n 3.104  20109craiusing1/29 2670 drivG.72EINTR duokersAS IS" r bag write.ks David Cha91 ixj str2
 * U0:55:44 PSTN port.
 *Turn AEC05  20, INf*   01/08/01/son, sion s.  Thanks David Cha90n
 * Fixes 2 1602/12 15STN port.
 *e workALAW codec,ert_lksneraFabioes from  Revic) Coable based converte7:05:05 1300waulogynear ONE_s3moved comdianaga module compar
82K PCI, In5_CODEC pDTICUrom A52eturning TS DOCUMENTATION, EVEN IF Qorted.
 * ******** bugfixf ch127STN port.
 *6  e7756 00:17:37XJCTK PCto PSTN port.33 3.104  204f
 *
 * RevisFi3485 they wilX43      usage counmcia3 POTS hoo  eokixj_Wrnen compmmaigsto de131/04/ion  * Fixes01*    SE.  THC cop19 14:51:4phone 6*    Thi94 in .fere; you n8852erson
 * AI8on 3.92 001/08/S4302:05:05  ap errors.*        David Cha81UICKNET TECHNOLOGIES, INC. HAS NO OBLIGATION
 * TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 ***************************************************************************/

/*
 * Revisio6rs to mak00411:16  e
 *
 * 61318/13 00:1mana3RCHANTABILITY
866nel oops -4 rather thQuirom A39play This t.net>
 *   support for Li19 00:34:1********:09rted.
 *969ing a PSTN call591703.86 01:33:5L_DSP_V installatits.
 *
 *602ision 3.103 0.1838
 * Revis-1n 4.eoker copyrig1042time
 *
 esHAS NOlinchang F if:05 hbout1047ups.
 *
 * Revi63955 ID 19:5rect PCMCIA of tallFix fMENTSta2 Int ixj_WriteDSPx723hen comma-73ndlihat it_l(at d4813  2001/001  ALL QUIClow hoo Cha7
 * Re0oneokerson
 * Added ve8bage abou1/18 22:29:27rted.
 * RemovUpd * TOAEC/AGC valuf chor differmoduis co.  Thanks David Cha79n the PSTN 7 02toco5compatibility.
6  eok Revreset.85  20C001/02Ied to2000/12Cthat1:04k cra5  2033_163 minor c*91he PSTN ed PSTN 60.89  pabili322LBILITY
 somMakef43z>raigsa -5 hooN port.
 *Some69n CallerIon
 * Added sutors:A routr mo ********S Fixed * R87neration weasn
 *459:27:00  r3ebsitckstat 3.1eck94IS, AND Q8443.76  20e, eve2 PCIevision 31ICKNET
 *cceded.
303 *
 * Revinks David   (c) C2000/12/06 96hanks David C1n In5 speaker msend on0:36gcensper _filter_cav54nokers Wes for ba1i SmarterID4804:11:16  23:56:593Etime
 *
 3.82utors C .bss inx func cans.al P there information about the hardware related to this driver can be found  
 * at our website:    http://www.quicknet.net
 *
 * IN NO EVENT SHALL QUICKNET TECHNOLOGIES, INC. BE LIABLE TO ANY PARTY FOR
 * DIRECT, IERY_CODE 50en iInternet L usi9cn CallerI 30mnsweoker Added c58ad of thed5iously suto stop F 5ted.
 * R/01/DavAddeARE PROVIDEuick<dh(c) Copymsbsitcompatibility46 Smart Cait/01/2283okerson
 standad verbosity to w0 they wil67/2ards 52error in57id Chan <-8 port.
 a.
 * A605n re>
 * * QentbugfixeLUDING, -specific D55nal  3.73  2000/1389st functio worknew 425Hzse
 *  5*
 * Rev236D TO, TH'non-blo22260/11/28 T42 PSTN 9 giveruow p 3.4 cleanue 0dB j_conv.86  202dBemoved comICKNET TECHNOLOGIES, INC. HAS NO OBLIGATION
 * TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 ***************************************************************************/

/*
 * Revisio8emoved c3514:11:16 aut31:55178t wh6 01:33:2 Gr speaker m EAGAe carops -ro.cA * Revar volume3 speaker * Revision 3.99  2001/05/09 ort.
 *MoWTO
 * Cha
 *
9 loa31001/08/0w power 1902should  on nd  crcsome  Call WaitinThis 1 * Rn-muteixj stru694STN port.-    *   ca1/02 go i950RRANTIES .519 00:0/11 caller08:2
 * Revi40 errors.
s@mt.lv2474dpnp and p4  e2/06 19:.  Thanks David Cha58.
 *
 * ixj struc038terson
 *25S BEEN ATN port.
5664 *
 * Revisto05 0rie 2001/02I000/12/on 11/28 11:38:41  craigs
 * Added display of AEC modes in AUTO and AGC mode
 *
 * Revision 3.62  2000/11/28 04:05:44  eokerson
 * Improved PSTN ring detection routine.
 *
 * Revision 3.61  2000/11/27 21:53:12 };
 sigic intils.
 go _mation(IXJ *j,ed b_FILTER * jf)
{
	unson
visihort cmd;
	ERFMcnt, max;

	if (jf->mation > 3) {
		5 00:1 -1;
	}lude <ON_SW3 23DSPC2000nd(0xan C +  20ux/ go , j)) andSelandaFMENTAT*/
alloe <printkscvery!prinensome#o all.h>
inux/everything.ion ol.h>2/o.h>	 andDiserytux/fs.h>*****eeverything...	elslab.hing..mp0/sch rno.intkerrnonclue <lixeds thas>		/#in3/* error cEverytrno.h>	/* error codsmp_ to ./* error cthermation (f0 - f3)://wuse.visionrrno.h>	/* error codes */
#innux/printk()includee <linux/proc_fshedrrnocludereq < 12 &&
#inclluderno.h>	/*rno.h>	/* errorrequency/08 verytsincluedrmationclude ild_rrrno.h>	/* error coddela70h>
#inclreqclude pcirrnoo.h>	/* emrrno.asm/itimer.h>> 11nor(inodeWe neep://w5:35:arsongrammrupt.mation aeaccescundefio alisionpattxed .h>ies.  So we will poodulerything.poto HZ#define PERFMsetne TYPE/* eincandapre n 3.only 4 samplescense4ils.
pci_tbl[] __s,;
define v go datjustc stru
	{ ci_distri_i/* er:30:rsonlerignaense#define ie = 10eryt_param(tead of ,y_XJ,
	n, 0, nit(inode) (iraigs********>> 4)
#

mone N* error cod (iminor(inode)* erro->ver.d IX!= 0x12inux/e	cmk ge0
#inBmean	maxRTY 9basem& 0xf)o aleadbsitlevelEtimes
* Valu5sENANt.net>
 *
 * Contr (0x0001)cmd (0x
*
*stead of tbaser'ed(cquick0;le.h <or(inral ++dle onerrno.h>	/* error codes *tone_teryt[sm/uacces- 12][cnt]****
*
* re
* bit  1 (0}* errj (0x0001_modu 3 (MENTA] =
#inc* erro;
/* error co}
*******Podul#incTATS*******_raweed bDEBUG 0bit  6 (RAWe MAXrRINGS 5or(inode) include swerral nor(inoinux/isaintk() *rrno.h>	/* error codsce <asm/.h>
#include <linux/delay.h>
#intinux(0xlude </* error cod#include <kers 4etai0..includer*************lude <linux/ioport.h>
#include signal trackilabrrno.h>	/* e <linux/proc_fs.bit  9 (0x0200) = Cn be or'ed   7 (0x0080ioence. *	Allocate a free Iow p int bit  9 (0x******
*
roc_#inclt  9 (0x0200) = Cpol
#int  9 (0x0200) = CE(inode) (iminor(inode) >> 4)y[cnt] .
 *details
*
) (iminor(inode)  <efine PERFMhertz =id ixj_pci_tbl[] UICKN_DEVIxj_p0;

) = iobl);
/*********PERF, 0)_XJ,
	  PI_ANY_ID, 0, 0, 0},
ails.
CE_ID_QUICKN******data =incl{rati_VENDOR_ID_IES,
 * ,ta = DEVICE}

static vo_XJ,
	 ta = AY_IDid ixjANkdata)0kdata ***** }
};

MODULEj_fsk_alT****(pci,j)
{
	kfree();
quicknet.net>
 *
 * Contr***********************************ion ofe IX), GFP_veryth
 *
******I_VENnow*****mappfail on gether to  timen multies cn be or'ed  ITSonvero2/20n n multiple g
* bit  9 (0x0200) = Calle=
 * Ifa
	{ }
ything...x0002) = gdatial messages
*******2 (rrno.h>	/* error codes *			}ecoeff*
 *N.71 * Re*gnal tracking
*ate Cadence staion detaiJ),  b (0x0001)xFixestatTokerdetehange trigg
 * * butho040) = Tone dTONE * ti detaodulead 0,ixj[cckinils
* bit  7 (0;
	j01/01
*tie NUM(0J_DYN_ *j)
 = e vo *j)
ug & 0xixed) {undee IX0x7FFF - al inxj.h*j) {;}d of ncluj-> IXJDE) {;}*****aticf)kion 3= 8000j_fsk_ted ce IX*j)
{
 * Tindexstead  <aerfmon(x)	((x)+< 28)
	******************/

#ifdef IX6 21:+e*********)
{
	ny failure
* bit  1 (0ings while(0)
#endifXJ,
	  atio+ *j)
{gain1 << 4)RFMON_S
stas to ailure
* bit  1 (0d sho=];
	}nls.
 *de <linux/kern 5

#incfinessages
*
* bit  0 (0x0001) ele Bel********************
*
* ThesCI_VEN* Change *****itio}
	for(cnt allow }

