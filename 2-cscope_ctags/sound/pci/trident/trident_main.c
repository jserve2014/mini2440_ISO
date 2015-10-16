/*
 *  Maintained by Jaroslav Kysela <perex@perex.cz>
 *  Originated by audio@tridentmicro.com
 *  Fri Feb 19 15:55:28 MST 1999
 *  Routines for control of Trident 4DWave (DX and NX) chip
 *
 *  BUGS:
 *
 *  TODO:
 *    ---
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 *
 *  SiS7018 S/PDIF support by Thomas Winischhofer <thomas@winischhofer.net>
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/gameport.h>
#include <linux/dma-mapping.h>

#include <sound/core.h>
#include <sound/info.h>
#include <sound/control.h>
#include <sound/tlv.h>
#include <sound/trident.h>
#include <sound/asoundef.h>

#include <asm/io.h>

static int snd_trident_pcm_mixer_build(struct snd_trident *trident,
				       struct snd_trident_voice * voice,
				       struct snd_pcm_substream *substream);
static int snd_trident_pcm_mixer_free(struct snd_trident *trident,
				      struct snd_trident_voice * voice,
				      struct snd_pcm_substream *substream);
static irqreturn_t snd_trident_interrupt(int irq, void *dev_id);
static int snd_trident_sis_reset(struct snd_trident *trident);

static void snd_trident_clear_voices(struct snd_trident * trident,
				     unsigned short v_min, unsigned short v_max);
static int snd_trident_free(struct snd_trident *trident);

/*
 *  common I/O routines
 */


#if 0
static void snd_trident_print_voice_regs(struct snd_trident *trident, int voice)
{
	unsigned int val, tmp;

	printk(KERN_DEBUG "Trident voice %i:\n", voice);
	outb(voice, TRID_REG(trident, T4D_LFO_GC_CIR));
	val = inl(TRID_REG(trident, CH_LBA));
	printk(KERN_DEBUG "LBA: 0x%x\n", val);
	val = inl(TRID_REG(trident, CH_GVSEL_PAN_VOL_CTRL_EC));
	printk(KERN_DEBUG "GVSel: %i\n", val >> 31);
	printk(KERN_DEBUG "Pan: 0x%x\n", (val >> 24) & 0x7f);
	printk(KERN_DEBUG "Vol: 0x%x\n", (val >> 16) & 0xff);
	printk(KERN_DEBUG "CTRL: 0x%x\n", (val >> 12) & 0x0f);
	printk(KERN_DEBUG "EC: 0x%x\n", val & 0x0fff);
	if (trident->device != TRIDENT_DEVICE_ID_NX) {
		val = inl(TRID_REG(trident, CH_DX_CSO_ALPHA_FMS));
		printk(KERN_DEBUG "CSO: 0x%x\n", val >> 16);
		printk("Alpha: 0x%x\n", (val >> 4) & 0x0fff);
		printk(KERN_DEBUG "FMS: 0x%x\n", val & 0x0f);
		val = inl(TRID_REG(trident, CH_DX_ESO_DELTA));
		printk(KERN_DEBUG "ESO: 0x%x\n", val >> 16);
		printk(KERN_DEBUG "Delta: 0x%x\n", val & 0xffff);
		val = inl(TRID_REG(trident, CH_DX_FMC_RVOL_CVOL));
	} else {		// TRIDENT_DEVICE_ID_NX
		val = inl(TRID_REG(trident, CH_NX_DELTA_CSO));
		tmp = (val >> 24) & 0xff;
		printk(KERN_DEBUG "CSO: 0x%x\n", val & 0x00ffffff);
		val = inl(TRID_REG(trident, CH_NX_DELTA_ESO));
		tmp |= (val >> 16) & 0xff00;
		printk(KERN_DEBUG "Delta: 0x%x\n", tmp);
		printk(KERN_DEBUG "ESO: 0x%x\n", val & 0x00ffffff);
		val = inl(TRID_REG(trident, CH_NX_ALPHA_FMS_FMC_RVOL_CVOL));
		printk(KERN_DEBUG "Alpha: 0x%x\n", val >> 20);
		printk(KERN_DEBUG "FMS: 0x%x\n", (val >> 16) & 0x0f);
	}
	printk(KERN_DEBUG "FMC: 0x%x\n", (val >> 14) & 3);
	printk(KERN_DEBUG "RVol: 0x%x\n", (val >> 7) & 0x7f);
	printk(KERN_DEBUG "CVol: 0x%x\n", val & 0x7f);
}
#endif

/*---------------------------------------------------------------------------
   unsigned short snd_trident_codec_read(struct snd_ac97 *ac97, unsigned short reg)
  
   Description: This routine will do all of the reading from the external
                CODEC (AC97).
  
   Parameters:  ac97 - ac97 codec structure
                reg - CODEC register index, from AC97 Hal.
 
   returns:     16 bit value read from the AC97.
  
  ---------------------------------------------------------------------------*/
static unsigned short snd_trident_codec_read(struct snd_ac97 *ac97, unsigned short reg)
{
	unsigned int data = 0, treg;
	unsigned short count = 0xffff;
	unsigned long flags;
	struct snd_trident *trident = ac97->private_data;

	spin_lock_irqsave(&trident->reg_lock, flags);
	if (trident->device == TRIDENT_DEVICE_ID_DX) {
		data = (DX_AC97_BUSY_READ | (reg & 0x000000ff));
		outl(data, TRID_REG(trident, DX_ACR1_AC97_R));
		do {
			data = inl(TRID_REG(trident, DX_ACR1_AC97_R));
			if ((data & DX_AC97_BUSY_READ) == 0)
				break;
		} while (--count);
	} else if (trident->device == TRIDENT_DEVICE_ID_NX) {
		data = (NX_AC97_BUSY_READ | (reg & 0x000000ff));
		treg = ac97->num == 0 ? NX_ACR2_AC97_R_PRIMARY : NX_ACR3_AC97_R_SECONDARY;
		outl(data, TRID_REG(trident, treg));
		do {
			data = inl(TRID_REG(trident, treg));
			if ((data & 0x00000C00) == 0)
				break;
		} while (--count);
	} else if (trident->device == TRIDENT_DEVICE_ID_SI7018) {
		data = SI_AC97_BUSY_READ | SI_AC97_AUDIO_BUSY | (reg & 0x000000ff);
		if (ac97->num == 1)
			data |= SI_AC97_SECONDARY;
		outl(data, TRID_REG(trident, SI_AC97_READ));
		do {
			data = inl(TRID_REG(trident, SI_AC97_READ));
			if ((data & (SI_AC97_BUSY_READ)) == 0)
				break;
		} while (--count);
	}

	if (count == 0 && !trident->ac97_detect) {
		snd_printk(KERN_ERR "ac97 codec read TIMEOUT [0x%x/0x%x]!!!\n",
			   reg, data);
		data = 0;
	}

	spin_unlock_irqrestore(&trident->reg_lock, flags);
	return ((unsigned short) (data >> 16));
}

/*---------------------------------------------------------------------------
   void snd_trident_codec_write(struct snd_ac97 *ac97, unsigned short reg,
   unsigned short wdata)
  
   Description: This routine will do all of the writing to the external
                CODEC (AC97).
  
   Parameters:	ac97 - ac97 codec structure
   	        reg - CODEC register index, from AC97 Hal.
                data  - Lower 16 bits are the data to write to CODEC.
  
   returns:     TRUE if everything went ok, else FALSE.
  
  ---------------------------------------------------------------------------*/
static void snd_trident_codec_write(struct snd_ac97 *ac97, unsigned short reg,
				    unsigned short wdata)
{
	unsigned int address, data;
	unsigned short count = 0xffff;
	unsigned long flags;
	struct snd_trident *trident = ac97->private_data;

	data = ((unsigned long) wdata) << 16;

	spin_lock_irqsave(&trident->reg_lock, flags);
	if (trident->device == TRIDENT_DEVICE_ID_DX) {
		address = DX_ACR0_AC97_W;

		/* read AC-97 write register status */
		do {
			if ((inw(TRID_REG(trident, address)) & DX_AC97_BUSY_WRITE) == 0)
				break;
		} while (--count);

		data |= (DX_AC97_BUSY_WRITE | (reg & 0x000000ff));
	} else if (trident->device == TRIDENT_DEVICE_ID_NX) {
		address = NX_ACR1_AC97_W;

		/* read AC-97 write register status */
		do {
			if ((inw(TRID_REG(trident, address)) & NX_AC97_BUSY_WRITE) == 0)
				break;
		} while (--count);

		data |= (NX_AC97_BUSY_WRITE | (ac97->num << 8) | (reg & 0x000000ff));
	} else if (trident->device == TRIDENT_DEVICE_ID_SI7018) {
		address = SI_AC97_WRITE;

		/* read AC-97 write register status */
		do {
			if ((inw(TRID_REG(trident, address)) & (SI_AC97_BUSY_WRITE)) == 0)
				break;
		} while (--count);

		data |= SI_AC97_BUSY_WRITE | SI_AC97_AUDIO_BUSY | (reg & 0x000000ff);
		if (ac97->num == 1)
			data |= SI_AC97_SECONDARY;
	} else {
		address = 0;	/* keep GCC happy */
		count = 0;	/* return */
	}

	if (count == 0) {
		spin_unlock_irqrestore(&trident->reg_lock, flags);
		return;
	}
	outl(data, TRID_REG(trident, address));
	spin_unlock_irqrestore(&trident->reg_lock, flags);
}

/*---------------------------------------------------------------------------
   void snd_trident_enable_eso(struct snd_trident *trident)
  
   Description: This routine will enable end of loop interrupts.
                End of loop interrupts will occur when a running
                channel reaches ESO.
                Also enables middle of loop interrupts.
  
   Parameters:  trident - pointer to target device class for 4DWave.
  
  ---------------------------------------------------------------------------*/

static void snd_trident_enable_eso(struct snd_trident * trident)
{
	unsigned int val;

	val = inl(TRID_REG(trident, T4D_LFO_GC_CIR));
	val |= ENDLP_IE;
	val |= MIDLP_IE;
	if (trident->device == TRIDENT_DEVICE_ID_SI7018)
		val |= BANK_B_EN;
	outl(val, TRID_REG(trident, T4D_LFO_GC_CIR));
}

/*---------------------------------------------------------------------------
   void snd_trident_disable_eso(struct snd_trident *trident)
  
   Description: This routine will disable end of loop interrupts.
                End of loop interrupts will occur when a running
                channel reaches ESO.
                Also disables middle of loop interrupts.
  
   Parameters:  
                trident - pointer to target device class for 4DWave.
  
   returns:     TRUE if everything went ok, else FALSE.
  
  ---------------------------------------------------------------------------*/

static void snd_trident_disable_eso(struct snd_trident * trident)
{
	unsigned int tmp;

	tmp = inl(TRID_REG(trident, T4D_LFO_GC_CIR));
	tmp &= ~ENDLP_IE;
	tmp &= ~MIDLP_IE;
	outl(tmp, TRID_REG(trident, T4D_LFO_GC_CIR));
}

/*---------------------------------------------------------------------------
   void snd_trident_start_voice(struct snd_trident * trident, unsigned int voice)

    Description: Start a voice, any channel 0 thru 63.
                 This routine automatically handles the fact that there are
                 more than 32 channels available.

    Parameters : voice - Voice number 0 thru n.
                 trident - pointer to target device class for 4DWave.

    Return Value: None.

  ---------------------------------------------------------------------------*/

void snd_trident_start_voice(struct snd_trident * trident, unsigned int voice)
{
	unsigned int mask = 1 << (voice & 0x1f);
	unsigned int reg = (voice & 0x20) ? T4D_START_B : T4D_START_A;

	outl(mask, TRID_REG(trident, reg));
}

EXPORT_SYMBOL(snd_trident_start_voice);

/*---------------------------------------------------------------------------
   void snd_trident_stop_voice(struct snd_trident * trident, unsigned int voice)

    Description: Stop a voice, any channel 0 thru 63.
                 This routine automatically handles the fact that there are
                 more than 32 channels available.

    Parameters : voice - Voice number 0 thru n.
                 trident - pointer to target device class for 4DWave.

    Return Value: None.

  ---------------------------------------------------------------------------*/

void snd_trident_stop_voice(struct snd_trident * trident, unsigned int voice)
{
	unsigned int mask = 1 << (voice & 0x1f);
	unsigned int reg = (voice & 0x20) ? T4D_STOP_B : T4D_STOP_A;

	outl(mask, TRID_REG(trident, reg));
}

EXPORT_SYMBOL(snd_trident_stop_voice);

/*---------------------------------------------------------------------------
    int snd_trident_allocate_pcm_channel(struct snd_trident *trident)
  
    Description: Allocate hardware channel in Bank B (32-63).
  
    Parameters :  trident - pointer to target device class for 4DWave.
  
    Return Value: hardware channel - 32-63 or -1 when no channel is available
  
  ---------------------------------------------------------------------------*/

static int snd_trident_allocate_pcm_channel(struct snd_trident * trident)
{
	int idx;

	if (trident->ChanPCMcnt >= trident->ChanPCM)
		return -1;
	for (idx = 31; idx >= 0; idx--) {
		if (!(trident->ChanMap[T4D_BANK_B] & (1 << idx))) {
			trident->ChanMap[T4D_BANK_B] |= 1 << idx;
			trident->ChanPCMcnt++;
			return idx + 32;
		}
	}
	return -1;
}

/*---------------------------------------------------------------------------
    void snd_trident_free_pcm_channel(int channel)
  
    Description: Free hardware channel in Bank B (32-63)
  
    Parameters :  trident - pointer to target device class for 4DWave.
	          channel - hardware channel number 0-63
  
    Return Value: none
  
  ---------------------------------------------------------------------------*/

static void snd_trident_free_pcm_channel(struct snd_trident *trident, int channel)
{
	if (channel < 32 || channel > 63)
		return;
	channel &= 0x1f;
	if (trident->ChanMap[T4D_BANK_B] & (1 << channel)) {
		trident->ChanMap[T4D_BANK_B] &= ~(1 << channel);
		trident->ChanPCMcnt--;
	}
}

/*---------------------------------------------------------------------------
    unsigned int snd_trident_allocate_synth_channel(void)
  
    Description: Allocate hardware channel in Bank A (0-31).
  
    Parameters :  trident - pointer to target device class for 4DWave.
  
    Return Value: hardware channel - 0-31 or -1 when no channel is available
  
  ---------------------------------------------------------------------------*/

static int snd_trident_allocate_synth_channel(struct snd_trident * trident)
{
	int idx;

	for (idx = 31; idx >= 0; idx--) {
		if (!(trident->ChanMap[T4D_BANK_A] & (1 << idx))) {
			trident->ChanMap[T4D_BANK_A] |= 1 << idx;
			trident->synth.ChanSynthCount++;
			return idx;
		}
	}
	return -1;
}

/*---------------------------------------------------------------------------
    void snd_trident_free_synth_channel( int channel )
  
    Description: Free hardware channel in Bank B (0-31).
  
    Parameters :  trident - pointer to target device class for 4DWave.
	          channel - hardware channel number 0-63
  
    Return Value: none
  
  ---------------------------------------------------------------------------*/

static void snd_trident_free_synth_channel(struct snd_trident *trident, int channel)
{
	if (channel < 0 || channel > 31)
		return;
	channel &= 0x1f;
	if (trident->ChanMap[T4D_BANK_A] & (1 << channel)) {
		trident->ChanMap[T4D_BANK_A] &= ~(1 << channel);
		trident->synth.ChanSynthCount--;
	}
}

/*---------------------------------------------------------------------------
   snd_trident_write_voice_regs
  
   Description: This routine will complete and write the 5 hardware channel
                registers to hardware.
  
   Parameters:  trident - pointer to target device class for 4DWave.
                voice - synthesizer voice structure
                Each register field.
  
  ---------------------------------------------------------------------------*/

void snd_trident_write_voice_regs(struct snd_trident * trident,
				  struct snd_trident_voice * voice)
{
	unsigned int FmcRvolCvol;
	unsigned int regs[5];

	regs[1] = voice->LBA;
	regs[4] = (voice->GVSel << 31) |
		  ((voice->Pan & 0x0000007f) << 24) |
		  ((voice->CTRL & 0x0000000f) << 12);
	FmcRvolCvol = ((voice->FMC & 3) << 14) |
	              ((voice->RVol & 0x7f) << 7) |
	              (voice->CVol & 0x7f);

	switch (trident->device) {
	case TRIDENT_DEVICE_ID_SI7018:
		regs[4] |= voice->number > 31 ?
				(voice->Vol & 0x000003ff) :
				((voice->Vol & 0x00003fc) << (16-2)) |
				(voice->EC & 0x00000fff);
		regs[0] = (voice->CSO << 16) | ((voice->Alpha & 0x00000fff) << 4) |
			(voice->FMS & 0x0000000f);
		regs[2] = (voice->ESO << 16) | (voice->Delta & 0x0ffff);
		regs[3] = (voice->Attribute << 16) | FmcRvolCvol;
		break;
	case TRIDENT_DEVICE_ID_DX:
		regs[4] |= ((voice->Vol & 0x000003fc) << (16-2)) |
			   (voice->EC & 0x00000fff);
		regs[0] = (voice->CSO << 16) | ((voice->Alpha & 0x00000fff) << 4) |
			(voice->FMS & 0x0000000f);
		regs[2] = (voice->ESO << 16) | (voice->Delta & 0x0ffff);
		regs[3] = FmcRvolCvol;
		break;
	case TRIDENT_DEVICE_ID_NX:
		regs[4] |= ((voice->Vol & 0x000003fc) << (16-2)) |
			   (voice->EC & 0x00000fff);
		regs[0] = (voice->Delta << 24) | (voice->CSO & 0x00ffffff);
		regs[2] = ((voice->Delta << 16) & 0xff000000) |
			(voice->ESO & 0x00ffffff);
		regs[3] = (voice->Alpha << 20) |
			((voice->FMS & 0x0000000f) << 16) | FmcRvolCvol;
		break;
	default:
		snd_BUG();
		return;
	}

	outb(voice->number, TRID_REG(trident, T4D_LFO_GC_CIR));
	outl(regs[0], TRID_REG(trident, CH_START + 0));
	outl(regs[1], TRID_REG(trident, CH_START + 4));
	outl(regs[2], TRID_REG(trident, CH_START + 8));
	outl(regs[3], TRID_REG(trident, CH_START + 12));
	outl(regs[4], TRID_REG(trident, CH_START + 16));

#if 0
	printk(KERN_DEBUG "written %i channel:\n", voice->number);
	printk(KERN_DEBUG "  regs[0] = 0x%x/0x%x\n",
	       regs[0], inl(TRID_REG(trident, CH_START + 0)));
	printk(KERN_DEBUG "  regs[1] = 0x%x/0x%x\n",
	       regs[1], inl(TRID_REG(trident, CH_START + 4)));
	printk(KERN_DEBUG "  regs[2] = 0x%x/0x%x\n",
	       regs[2], inl(TRID_REG(trident, CH_START + 8)));
	printk(KERN_DEBUG "  regs[3] = 0x%x/0x%x\n",
	       regs[3], inl(TRID_REG(trident, CH_START + 12)));
	printk(KERN_DEBUG "  regs[4] = 0x%x/0x%x\n",
	       regs[4], inl(TRID_REG(trident, CH_START + 16)));
#endif
}

EXPORT_SYMBOL(snd_trident_write_voice_regs);

/*---------------------------------------------------------------------------
   snd_trident_write_cso_reg
  
   Description: This routine will write the new CSO offset
                register to hardware.
  
   Parameters:  trident - pointer to target device class for 4DWave.
                voice - synthesizer voice structure
                CSO - new CSO value
  
  ---------------------------------------------------------------------------*/

static void snd_trident_write_cso_reg(struct snd_trident * trident,
				      struct snd_trident_voice * voice,
				      unsigned int CSO)
{
	voice->CSO = CSO;
	outb(voice->number, TRID_REG(trident, T4D_LFO_GC_CIR));
	if (trident->device != TRIDENT_DEVICE_ID_NX) {
		outw(voice->CSO, TRID_REG(trident, CH_DX_CSO_ALPHA_FMS) + 2);
	} else {
		outl((voice->Delta << 24) |
		     (voice->CSO & 0x00ffffff), TRID_REG(trident, CH_NX_DELTA_CSO));
	}
}

/*---------------------------------------------------------------------------
   snd_trident_write_eso_reg
  
   Description: This routine will write the new ESO offset
                register to hardware.
  
   Parameters:  trident - pointer to target device class for 4DWave.
                voice - synthesizer voice structure
                ESO - new ESO value
  
  ---------------------------------------------------------------------------*/

static void snd_trident_write_eso_reg(struct snd_trident * trident,
				      struct snd_trident_voice * voice,
				      unsigned int ESO)
{
	voice->ESO = ESO;
	outb(voice->number, TRID_REG(trident, T4D_LFO_GC_CIR));
	if (trident->device != TRIDENT_DEVICE_ID_NX) {
		outw(voice->ESO, TRID_REG(trident, CH_DX_ESO_DELTA) + 2);
	} else {
		outl(((voice->Delta << 16) & 0xff000000) | (voice->ESO & 0x00ffffff),
		     TRID_REG(trident, CH_NX_DELTA_ESO));
	}
}

/*---------------------------------------------------------------------------
   snd_trident_write_vol_reg
  
   Description: This routine will write the new voice volume
                register to hardware.
  
   Parameters:  trident - pointer to target device class for 4DWave.
                voice - synthesizer voice structure
                Vol - new voice volume
  
  ---------------------------------------------------------------------------*/

static void snd_trident_write_vol_reg(struct snd_trident * trident,
				      struct snd_trident_voice * voice,
				      unsigned int Vol)
{
	voice->Vol = Vol;
	outb(voice->number, TRID_REG(trident, T4D_LFO_GC_CIR));
	switch (trident->device) {
	case TRIDENT_DEVICE_ID_DX:
	case TRIDENT_DEVICE_ID_NX:
		outb(voice->Vol >> 2, TRID_REG(trident, CH_GVSEL_PAN_VOL_CTRL_EC + 2));
		break;
	case TRIDENT_DEVICE_ID_SI7018:
		/* printk(KERN_DEBUG "voice->Vol = 0x%x\n", voice->Vol); */
		outw((voice->CTRL << 12) | voice->Vol,
		     TRID_REG(trident, CH_GVSEL_PAN_VOL_CTRL_EC));
		break;
	}
}

/*---------------------------------------------------------------------------
   snd_trident_write_pan_reg
  
   Description: This routine will write the new voice pan
                register to hardware.
  
   Parameters:  trident - pointer to target device class for 4DWave.
                voice - synthesizer voice structure
                Pan - new pan value
  
  ---------------------------------------------------------------------------*/

static void snd_trident_write_pan_reg(struct snd_trident * trident,
				      struct snd_trident_voice * voice,
				      unsigned int Pan)
{
	voice->Pan = Pan;
	outb(voice->number, TRID_REG(trident, T4D_LFO_GC_CIR));
	outb(((voice->GVSel & 0x01) << 7) | (voice->Pan & 0x7f),
	     TRID_REG(trident, CH_GVSEL_PAN_VOL_CTRL_EC + 3));
}

/*---------------------------------------------------------------------------
   snd_trident_write_rvol_reg
  
   Description: This routine will write the new reverb volume
                register to hardware.
  
   Parameters:  trident - pointer to target device class for 4DWave.
                voice - synthesizer voice structure
                RVol - new reverb volume
  
  ---------------------------------------------------------------------------*/

static void snd_trident_write_rvol_reg(struct snd_trident * trident,
				       struct snd_trident_voice * voice,
				       unsigned int RVol)
{
	voice->RVol = RVol;
	outb(voice->number, TRID_REG(trident, T4D_LFO_GC_CIR));
	outw(((voice->FMC & 0x0003) << 14) | ((voice->RVol & 0x007f) << 7) |
	     (voice->CVol & 0x007f),
	     TRID_REG(trident, trident->device == TRIDENT_DEVICE_ID_NX ?
		      CH_NX_ALPHA_FMS_FMC_RVOL_CVOL : CH_DX_FMC_RVOL_CVOL));
}

/*---------------------------------------------------------------------------
   snd_trident_write_cvol_reg
  
   Description: This routine will write the new chorus volume
                register to hardware.
  
   Parameters:  trident - pointer to target device class for 4DWave.
                voice - synthesizer voice structure
                CVol - new chorus volume
  
  ---------------------------------------------------------------------------*/

static void snd_trident_write_cvol_reg(struct snd_trident * trident,
				       struct snd_trident_voice * voice,
				       unsigned int CVol)
{
	voice->CVol = CVol;
	outb(voice->number, TRID_REG(trident, T4D_LFO_GC_CIR));
	outw(((voice->FMC & 0x0003) << 14) | ((voice->RVol & 0x007f) << 7) |
	     (voice->CVol & 0x007f),
	     TRID_REG(trident, trident->device == TRIDENT_DEVICE_ID_NX ?
		      CH_NX_ALPHA_FMS_FMC_RVOL_CVOL : CH_DX_FMC_RVOL_CVOL));
}

/*---------------------------------------------------------------------------
   snd_trident_convert_rate

   Description: This routine converts rate in HZ to hardware delta value.
  
   Parameters:  trident - pointer to target device class for 4DWave.
                rate - Real or Virtual channel number.
  
   Returns:     Delta value.
  
  ---------------------------------------------------------------------------*/
static unsigned int snd_trident_convert_rate(unsigned int rate)
{
	unsigned int delta;

	// We special case 44100 and 8000 since rounding with the equation
	// does not give us an accurate enough value. For 11025 and 22050
	// the equation gives us the best answer. All other frequencies will
	// also use the equation. JDW
	if (rate == 44100)
		delta = 0xeb3;
	else if (rate == 8000)
		delta = 0x2ab;
	else if (rate == 48000)
		delta = 0x1000;
	else
		delta = (((rate << 12) + 24000) / 48000) & 0x0000ffff;
	return delta;
}

/*---------------------------------------------------------------------------
   snd_trident_convert_adc_rate

   Description: This routine converts rate in HZ to hardware delta value.
  
   Parameters:  trident - pointer to target device class for 4DWave.
                rate - Real or Virtual channel number.
  
   Returns:     Delta value.
  
  ---------------------------------------------------------------------------*/
static unsigned int snd_trident_convert_adc_rate(unsigned int rate)
{
	unsigned int delta;

	// We special case 44100 and 8000 since rounding with the equation
	// does not give us an accurate enough value. For 11025 and 22050
	// the equation gives us the best answer. All other frequencies will
	// also use the equation. JDW
	if (rate == 44100)
		delta = 0x116a;
	else if (rate == 8000)
		delta = 0x6000;
	else if (rate == 48000)
		delta = 0x1000;
	else
		delta = ((48000 << 12) / rate) & 0x0000ffff;
	return delta;
}

/*---------------------------------------------------------------------------
   snd_trident_spurious_threshold

   Description: This routine converts rate in HZ to spurious threshold.
  
   Parameters:  trident - pointer to target device class for 4DWave.
                rate - Real or Virtual channel number.
  
   Returns:     Delta value.
  
  ---------------------------------------------------------------------------*/
static unsigned int snd_trident_spurious_threshold(unsigned int rate,
						   unsigned int period_size)
{
	unsigned int res = (rate * period_size) / 48000;
	if (res < 64)
		res = res / 2;
	else
		res -= 32;
	return res;
}

/*---------------------------------------------------------------------------
   snd_trident_control_mode

   Description: This routine returns a control mode for a PCM channel.
  
   Parameters:  trident - pointer to target device class for 4DWave.
                substream  - PCM substream
  
   Returns:     Control value.
  
  ---------------------------------------------------------------------------*/
static unsigned int snd_trident_control_mode(struct snd_pcm_substream *substream)
{
	unsigned int CTRL;
	struct snd_pcm_runtime *runtime = substream->runtime;

	/* set ctrl mode
	   CTRL default: 8-bit (unsigned) mono, loop mode enabled
	 */
	CTRL = 0x00000001;
	if (snd_pcm_format_width(runtime->format) == 16)
		CTRL |= 0x00000008;	// 16-bit data
	if (snd_pcm_format_signed(runtime->format))
		CTRL |= 0x00000002;	// signed data
	if (runtime->channels > 1)
		CTRL |= 0x00000004;	// stereo data
	return CTRL;
}

/*
 *  PCM part
 */

/*---------------------------------------------------------------------------
   snd_trident_ioctl
  
   Description: Device I/O control handler for playback/capture parameters.
  
   Parameters:   substream  - PCM substream class
                cmd     - what ioctl message to process
                arg     - additional message infoarg     
  
   Returns:     Error status
  
  ---------------------------------------------------------------------------*/

static int snd_trident_ioctl(struct snd_pcm_substream *substream,
			     unsigned int cmd,
			     void *arg)
{
	/* FIXME: it seems that with small periods the behaviour of
	          trident hardware is unpredictable and interrupt generator
	          is broken */
	return snd_pcm_lib_ioctl(substream, cmd, arg);
}

/*---------------------------------------------------------------------------
   snd_trident_allocate_pcm_mem
  
   Description: Allocate PCM ring buffer for given substream
  
   Parameters:  substream  - PCM substream class
		hw_params  - hardware parameters
  
   Returns:     Error status
  
  ---------------------------------------------------------------------------*/

static int snd_trident_allocate_pcm_mem(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *hw_params)
{
	struct snd_trident *trident = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_trident_voice *voice = runtime->private_data;
	int err;

	if ((err = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params))) < 0)
		return err;
	if (trident->tlb.entries) {
		if (err > 0) { /* change */
			if (voice->memblk)
				snd_trident_free_pages(trident, voice->memblk);
			voice->memblk = snd_trident_alloc_pages(trident, substream);
			if (voice->memblk == NULL)
				return -ENOMEM;
		}
	}
	return 0;
}

/*---------------------------------------------------------------------------
   snd_trident_allocate_evoice
  
   Description: Allocate extra voice as interrupt generator
  
   Parameters:  substream  - PCM substream class
		hw_params  - hardware parameters
  
   Returns:     Error status
  
  ---------------------------------------------------------------------------*/

static int snd_trident_allocate_evoice(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *hw_params)
{
	struct snd_trident *trident = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_trident_voice *voice = runtime->private_data;
	struct snd_trident_voice *evoice = voice->extra;

	/* voice management */

	if (params_buffer_size(hw_params) / 2 != params_period_size(hw_params)) {
		if (evoice == NULL) {
			evoice = snd_trident_alloc_voice(trident, SNDRV_TRIDENT_VOICE_TYPE_PCM, 0, 0);
			if (evoice == NULL)
				return -ENOMEM;
			voice->extra = evoice;
			evoice->substream = substream;
		}
	} else {
		if (evoice != NULL) {
			snd_trident_free_voice(trident, evoice);
			voice->extra = evoice = NULL;
		}
	}

	return 0;
}

/*---------------------------------------------------------------------------
   snd_trident_hw_params
  
   Description: Set the hardware parameters for the playback device.
  
   Parameters:  substream  - PCM substream class
		hw_params  - hardware parameters
  
   Returns:     Error status
  
  ---------------------------------------------------------------------------*/

static int snd_trident_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *hw_params)
{
	int err;

	err = snd_trident_allocate_pcm_mem(substream, hw_params);
	if (err >= 0)
		err = snd_trident_allocate_evoice(substream, hw_params);
	return err;
}

/*---------------------------------------------------------------------------
   snd_trident_playback_hw_free
  
   Description: Release the hardware resources for the playback device.
  
   Parameters:  substream  - PCM substream class
  
   Returns:     Error status
  
  ---------------------------------------------------------------------------*/

static int snd_trident_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_trident *trident = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_trident_voice *voice = runtime->private_data;
	struct snd_trident_voice *evoice = voice ? voice->extra : NULL;

	if (trident->tlb.entries) {
		if (voice && voice->memblk) {
			snd_trident_free_pages(trident, voice->memblk);
			voice->memblk = NULL;
		}
	}
	snd_pcm_lib_free_pages(substream);
	if (evoice != NULL) {
		snd_trident_free_voice(trident, evoice);
		voice->extra = NULL;
	}
	return 0;
}

/*---------------------------------------------------------------------------
   snd_trident_playback_prepare
  
   Description: Prepare playback device for playback.
  
   Parameters:  substream  - PCM substream class
  
   Returns:     Error status
  
  ---------------------------------------------------------------------------*/

static int snd_trident_playback_prepare(struct snd_pcm_substream *substream)
{
	struct snd_trident *trident = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_trident_voice *voice = runtime->private_data;
	struct snd_trident_voice *evoice = voice->extra;
	struct snd_trident_pcm_mixer *mix = &trident->pcm_mixer[substream->number];

	spin_lock_irq(&trident->reg_lock);	

	/* set delta (rate) value */
	voice->Delta = snd_trident_convert_rate(runtime->rate);
	voice->spurious_threshold = snd_trident_spurious_threshold(runtime->rate, runtime->period_size);

	/* set Loop Begin Address */
	if (voice->memblk)
		voice->LBA = voice->memblk->offset;
	else
		voice->LBA = runtime->dma_addr;
 
	voice->CSO = 0;
	voice->ESO = runtime->buffer_size - 1;	/* in samples */
	voice->CTRL = snd_trident_control_mode(substream);
	voice->FMC = 3;
	voice->GVSel = 1;
	voice->EC = 0;
	voice->Alpha = 0;
	voice->FMS = 0;
	voice->Vol = mix->vol;
	voice->RVol = mix->rvol;
	voice->CVol = mix->cvol;
	voice->Pan = mix->pan;
	voice->Attribute = 0;
#if 0
	voice->Attribute = (1<<(30-16))|(2<<(26-16))|
			   (0<<(24-16))|(0x1f<<(19-16));
#else
	voice->Attribute = 0;
#endif

	snd_trident_write_voice_regs(trident, voice);

	if (evoice != NULL) {
		evoice->Delta = voice->Delta;
		evoice->spurious_threshold = voice->spurious_threshold;
		evoice->LBA = voice->LBA;
		evoice->CSO = 0;
		evoice->ESO = (runtime->period_size * 2) + 4 - 1; /* in samples */
		evoice->CTRL = voice->CTRL;
		evoice->FMC = 3;
		evoice->GVSel = trident->device == TRIDENT_DEVICE_ID_SI7018 ? 0 : 1;
		evoice->EC = 0;
		evoice->Alpha = 0;
		evoice->FMS = 0;
		evoice->Vol = 0x3ff;			/* mute */
		evoice->RVol = evoice->CVol = 0x7f;	/* mute */
		evoice->Pan = 0x7f;			/* mute */
#if 0
		evoice->Attribute = (1<<(30-16))|(2<<(26-16))|
				    (0<<(24-16))|(0x1f<<(19-16));
#else
		evoice->Attribute = 0;
#endif
		snd_trident_write_voice_regs(trident, evoice);
		evoice->isync2 = 1;
		evoice->isync_mark = runtime->period_size;
		evoice->ESO = (runtime->period_size * 2) - 1;
	}

	spin_unlock_irq(&trident->reg_lock);

	return 0;
}

/*---------------------------------------------------------------------------
   snd_trident_capture_hw_params
  
   Description: Set the hardware parameters for the capture device.
  
   Parameters:  substream  - PCM substream class
		hw_params  - hardware parameters
  
   Returns:     Error status
  
  ---------------------------------------------------------------------------*/

static int snd_trident_capture_hw_params(struct snd_pcm_substream *substream,
					 struct snd_pcm_hw_params *hw_params)
{
	return snd_trident_allocate_pcm_mem(substream, hw_params);
}

/*---------------------------------------------------------------------------
   snd_trident_capture_prepare
  
   Description: Prepare capture device for playback.
  
   Parameters:  substream  - PCM substream class
  
   Returns:     Error status
  
  ---------------------------------------------------------------------------*/

static int snd_trident_capture_prepare(struct snd_pcm_substream *substream)
{
	struct snd_trident *trident = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_trident_voice *voice = runtime->private_data;
	unsigned int val, ESO_bytes;

	spin_lock_irq(&trident->reg_lock);

	// Initilize the channel and set channel Mode
	outb(0, TRID_REG(trident, LEGACY_DMAR15));

	// Set DMA channel operation mode register
	outb(0x54, TRID_REG(trident, LEGACY_DMAR11));

	// Set channel buffer Address, DMAR0 expects contiguous PCI memory area	
	voice->LBA = runtime->dma_addr;
	outl(voice->LBA, TRID_REG(trident, LEGACY_DMAR0));
	if (voice->memblk)
		voice->LBA = voice->memblk->offset;

	// set ESO
	ESO_bytes = snd_pcm_lib_buffer_bytes(substream) - 1;
	outb((ESO_bytes & 0x00ff0000) >> 16, TRID_REG(trident, LEGACY_DMAR6));
	outw((ESO_bytes & 0x0000ffff), TRID_REG(trident, LEGACY_DMAR4));
	ESO_bytes++;

	// Set channel sample rate, 4.12 format
	val = (((unsigned int) 48000L << 12) + (runtime->rate/2)) / runtime->rate;
	outw(val, TRID_REG(trident, T4D_SBDELTA_DELTA_R));

	// Set channel interrupt blk length
	if (snd_pcm_format_width(runtime->format) == 16) {
		val = (unsigned short) ((ESO_bytes >> 1) - 1);
	} else {
		val = (unsigned short) (ESO_bytes - 1);
	}

	outl((val << 16) | val, TRID_REG(trident, T4D_SBBL_SBCL));

	// Right now, set format and start to run captureing, 
	// continuous run loop enable.
	trident->bDMAStart = 0x19;	// 0001 1001b

	if (snd_pcm_format_width(runtime->format) == 16)
		trident->bDMAStart |= 0x80;
	if (snd_pcm_format_signed(runtime->format))
		trident->bDMAStart |= 0x20;
	if (runtime->channels > 1)
		trident->bDMAStart |= 0x40;

	// Prepare capture intr channel

	voice->Delta = snd_trident_convert_rate(runtime->rate);
	voice->spurious_threshold = snd_trident_spurious_threshold(runtime->rate, runtime->period_size);
	voice->isync = 1;
	voice->isync_mark = runtime->period_size;
	voice->isync_max = runtime->buffer_size;

	// Set voice parameters
	voice->CSO = 0;
	voice->ESO = voice->isync_ESO = (runtime->period_size * 2) + 6 - 1;
	voice->CTRL = snd_trident_control_mode(substream);
	voice->FMC = 3;
	voice->RVol = 0x7f;
	voice->CVol = 0x7f;
	voice->GVSel = 1;
	voice->Pan = 0x7f;		/* mute */
	voice->Vol = 0x3ff;		/* mute */
	voice->EC = 0;
	voice->Alpha = 0;
	voice->FMS = 0;
	voice->Attribute = 0;

	snd_trident_write_voice_regs(trident, voice);

	spin_unlock_irq(&trident->reg_lock);
	return 0;
}

/*---------------------------------------------------------------------------
   snd_trident_si7018_capture_hw_params
  
   Description: Set the hardware parameters for the capture device.
  
   Parameters:  substream  - PCM substream class
		hw_params  - hardware parameters
  
   Returns:     Error status
  
  ---------------------------------------------------------------------------*/

static int snd_trident_si7018_capture_hw_params(struct snd_pcm_substream *substream,
						struct snd_pcm_hw_params *hw_params)
{
	int err;

	if ((err = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params))) < 0)
		return err;

	return snd_trident_allocate_evoice(substream, hw_params);
}

/*---------------------------------------------------------------------------
   snd_trident_si7018_capture_hw_free
  
   Description: Release the hardware resources for the capture device.
  
   Parameters:  substream  - PCM substream class
  
   Returns:     Error status
  
  ---------------------------------------------------------------------------*/

static int snd_trident_si7018_capture_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_trident *trident = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_trident_voice *voice = runtime->private_data;
	struct snd_trident_voice *evoice = voice ? voice->extra : NULL;

	snd_pcm_lib_free_pages(substream);
	if (evoice != NULL) {
		snd_trident_free_voice(trident, evoice);
		voice->extra = NULL;
	}
	return 0;
}

/*---------------------------------------------------------------------------
   snd_trident_si7018_capture_prepare
  
   Description: Prepare capture device for playback.
  
   Parameters:  substream  - PCM substream class
  
   Returns:     Error status
  
  ---------------------------------------------------------------------------*/

static int snd_trident_si7018_capture_prepare(struct snd_pcm_substream *substream)
{
	struct snd_trident *trident = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_trident_voice *voice = runtime->private_data;
	struct snd_trident_voice *evoice = voice->extra;

	spin_lock_irq(&trident->reg_lock);

	voice->LBA = runtime->dma_addr;
	voice->Delta = snd_trident_convert_adc_rate(runtime->rate);
	voice->spurious_threshold = snd_trident_spurious_threshold(runtime->rate, runtime->period_size);

	// Set voice parameters
	voice->CSO = 0;
	voice->ESO = runtime->buffer_size - 1;		/* in samples */
	voice->CTRL = snd_trident_control_mode(substream);
	voice->FMC = 0;
	voice->RVol = 0;
	voice->CVol = 0;
	voice->GVSel = 1;
	voice->Pan = T4D_DEFAULT_PCM_PAN;
	voice->Vol = 0;
	voice->EC = 0;
	voice->Alpha = 0;
	voice->FMS = 0;

	voice->Attribute = (2 << (30-16)) |
			   (2 << (26-16)) |
			   (2 << (24-16)) |
			   (1 << (23-16));

	snd_trident_write_voice_regs(trident, voice);

	if (evoice != NULL) {
		evoice->Delta = snd_trident_convert_rate(runtime->rate);
		evoice->spurious_threshold = voice->spurious_threshold;
		evoice->LBA = voice->LBA;
		evoice->CSO = 0;
		evoice->ESO = (runtime->period_size * 2) + 20 - 1; /* in samples, 20 means correction */
		evoice->CTRL = voice->CTRL;
		evoice->FMC = 3;
		evoice->GVSel = 0;
		evoice->EC = 0;
		evoice->Alpha = 0;
		evoice->FMS = 0;
		evoice->Vol = 0x3ff;			/* mute */
		evoice->RVol = evoice->CVol = 0x7f;	/* mute */
		evoice->Pan = 0x7f;			/* mute */
		evoice->Attribute = 0;
		snd_trident_write_voice_regs(trident, evoice);
		evoice->isync2 = 1;
		evoice->isync_mark = runtime->period_size;
		evoice->ESO = (runtime->period_size * 2) - 1;
	}
	
	spin_unlock_irq(&trident->reg_lock);
	return 0;
}

/*---------------------------------------------------------------------------
   snd_trident_foldback_prepare
  
   Description: Prepare foldback capture device for playback.
  
   Parameters:  substream  - PCM substream class
  
   Returns:     Error status
  
  ---------------------------------------------------------------------------*/

static int snd_trident_foldback_prepare(struct snd_pcm_substream *substream)
{
	struct snd_trident *trident = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_trident_voice *voice = runtime->private_data;
	struct snd_trident_voice *evoice = voice->extra;

	spin_lock_irq(&trident->reg_lock);

	/* Set channel buffer Address */
	if (voice->memblk)
		voice->LBA = voice->memblk->offset;
	else
		voice->LBA = runtime->dma_addr;

	/* set target ESO for channel */
	voice->ESO = runtime->buffer_size - 1;	/* in samples */

	/* set sample rate */
	voice->Delta = 0x1000;
	voice->spurious_threshold = snd_trident_spurious_threshold(48000, runtime->period_size);

	voice->CSO = 0;
	voice->CTRL = snd_trident_control_mode(substream);
	voice->FMC = 3;
	voice->RVol = 0x7f;
	voice->CVol = 0x7f;
	voice->GVSel = 1;
	voice->Pan = 0x7f;	/* mute */
	voice->Vol = 0x3ff;	/* mute */
	voice->EC = 0;
	voice->Alpha = 0;
	voice->FMS = 0;
	voice->Attribute = 0;

	/* set up capture channel */
	outb(((voice->number & 0x3f) | 0x80), TRID_REG(trident, T4D_RCI + voice->foldback_chan));

	snd_trident_write_voice_regs(trident, voice);

	if (evoice != NULL) {
		evoice->Delta = voice->Delta;
		evoice->spurious_threshold = voice->spurious_threshold;
		evoice->LBA = voice->LBA;
		evoice->CSO = 0;
		evoice->ESO = (runtime->period_size * 2) + 4 - 1; /* in samples */
		evoice->CTRL = voice->CTRL;
		evoice->FMC = 3;
		evoice->GVSel = trident->device == TRIDENT_DEVICE_ID_SI7018 ? 0 : 1;
		evoice->EC = 0;
		evoice->Alpha = 0;
		evoice->FMS = 0;
		evoice->Vol = 0x3ff;			/* mute */
		evoice->RVol = evoice->CVol = 0x7f;	/* mute */
		evoice->Pan = 0x7f;			/* mute */
		evoice->Attribute = 0;
		snd_trident_write_voice_regs(trident, evoice);
		evoice->isync2 = 1;
		evoice->isync_mark = runtime->period_size;
		evoice->ESO = (runtime->period_size * 2) - 1;
	}

	spin_unlock_irq(&trident->reg_lock);
	return 0;
}

/*---------------------------------------------------------------------------
   snd_trident_spdif_hw_params
  
   Description: Set the hardware parameters for the spdif device.
  
   Parameters:  substream  - PCM substream class
		hw_params  - hardware parameters
  
   Returns:     Error status
  
  ---------------------------------------------------------------------------*/

static int snd_trident_spdif_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *hw_params)
{
	struct snd_trident *trident = snd_pcm_substream_chip(substream);
	unsigned int old_bits = 0, change = 0;
	int err;

	err = snd_trident_allocate_pcm_mem(substream, hw_params);
	if (err < 0)
		return err;

	if (trident->device == TRIDENT_DEVICE_ID_SI7018) {
		err = snd_trident_allocate_evoice(substream, hw_params);
		if (err < 0)
			return err;
	}

	/* prepare SPDIF channel */
	spin_lock_irq(&trident->reg_lock);
	old_bits = trident->spdif_pcm_bits;
	if (old_bits & IEC958_AES0_PROFESSIONAL)
		trident->spdif_pcm_bits &= ~IEC958_AES0_PRO_FS;
	else
		trident->spdif_pcm_bits &= ~(IEC958_AES3_CON_FS << 24);
	if (params_rate(hw_params) >= 48000) {
		trident->spdif_pcm_ctrl = 0x3c;	// 48000 Hz
		trident->spdif_pcm_bits |=
			trident->spdif_bits & IEC958_AES0_PROFESSIONAL ?
				IEC958_AES0_PRO_FS_48000 :
				(IEC958_AES3_CON_FS_48000 << 24);
	}
	else if (params_rate(hw_params) >= 44100) {
		trident->spdif_pcm_ctrl = 0x3e;	// 44100 Hz
		trident->spdif_pcm_bits |=
			trident->spdif_bits & IEC958_AES0_PROFESSIONAL ?
				IEC958_AES0_PRO_FS_44100 :
				(IEC958_AES3_CON_FS_44100 << 24);
	}
	else {
		trident->spdif_pcm_ctrl = 0x3d;	// 32000 Hz
		trident->spdif_pcm_bits |=
			trident->spdif_bits & IEC958_AES0_PROFESSIONAL ?
				IEC958_AES0_PRO_FS_32000 :
				(IEC958_AES3_CON_FS_32000 << 24);
	}
	change = old_bits != trident->spdif_pcm_bits;
	spin_unlock_irq(&trident->reg_lock);

	if (change)
		snd_ctl_notify(trident->card, SNDRV_CTL_EVENT_MASK_VALUE, &trident->spdif_pcm_ctl->id);

	return 0;
}

/*---------------------------------------------------------------------------
   snd_trident_spdif_prepare
  
   Description: Prepare SPDIF device for playback.
  
   Parameters:  substream  - PCM substream class
  
   Returns:     Error status
  
  ---------------------------------------------------------------------------*/

static int snd_trident_spdif_prepare(struct snd_pcm_substream *substream)
{
	struct snd_trident *trident = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_trident_voice *voice = runtime->private_data;
	struct snd_trident_voice *evoice = voice->extra;
	struct snd_trident_pcm_mixer *mix = &trident->pcm_mixer[substream->number];
	unsigned int RESO, LBAO;
	unsigned int temp;

	spin_lock_irq(&trident->reg_lock);

	if (trident->device != TRIDENT_DEVICE_ID_SI7018) {

		/* set delta (rate) value */
		voice->Delta = snd_trident_convert_rate(runtime->rate);
		voice->spurious_threshold = snd_trident_spurious_threshold(runtime->rate, runtime->period_size);

		/* set Loop Back Address */
		LBAO = runtime->dma_addr;
		if (voice->memblk)
			voice->LBA = voice->memblk->offset;
		else
			voice->LBA = LBAO;

		voice->isync = 1;
		voice->isync3 = 1;
		voice->isync_mark = runtime->period_size;
		voice->isync_max = runtime->buffer_size;

		/* set target ESO for channel */
		RESO = runtime->buffer_size - 1;
		voice->ESO = voice->isync_ESO = (runtime->period_size * 2) + 6 - 1;

		/* set ctrl mode */
		voice->CTRL = snd_trident_control_mode(substream);

		voice->FMC = 3;
		voice->RVol = 0x7f;
		voice->CVol = 0x7f;
		voice->GVSel = 1;
		voice->Pan = 0x7f;
		voice->Vol = 0x3ff;
		voice->EC = 0;
		voice->CSO = 0;
		voice->Alpha = 0;
		voice->FMS = 0;
		voice->Attribute = 0;

		/* prepare surrogate IRQ channel */
		snd_trident_write_voice_regs(trident, voice);

		outw((RESO & 0xffff), TRID_REG(trident, NX_SPESO));
		outb((RESO >> 16), TRID_REG(trident, NX_SPESO + 2));
		outl((LBAO & 0xfffffffc), TRID_REG(trident, NX_SPLBA));
		outw((voice->CSO & 0xffff), TRID_REG(trident, NX_SPCTRL_SPCSO));
		outb((voice->CSO >> 16), TRID_REG(trident, NX_SPCTRL_SPCSO + 2));

		/* set SPDIF setting */
		outb(trident->spdif_pcm_ctrl, TRID_REG(trident, NX_SPCTRL_SPCSO + 3));
		outl(trident->spdif_pcm_bits, TRID_REG(trident, NX_SPCSTATUS));

	} else {	/* SiS */
	
		/* set delta (rate) value */
		voice->Delta = 0x800;
		voice->spurious_threshold = snd_trident_spurious_threshold(48000, runtime->period_size);

		/* set Loop Begin Address */
		if (voice->memblk)
			voice->LBA = voice->memblk->offset;
		else
			voice->LBA = runtime->dma_addr;

		voice->CSO = 0;
		voice->ESO = runtime->buffer_size - 1;	/* in samples */
		voice->CTRL = snd_trident_control_mode(substream);
		voice->FMC = 3;
		voice->GVSel = 1;
		voice->EC = 0;
		voice->Alpha = 0;
		voice->FMS = 0;
		voice->Vol = mix->vol;
		voice->RVol = mix->rvol;
		voice->CVol = mix->cvol;
		voice->Pan = mix->pan;
		voice->Attribute = (1<<(30-16))|(7<<(26-16))|
				   (0<<(24-16))|(0<<(19-16));

		snd_trident_write_voice_regs(trident, voice);

		if (evoice != NULL) {
			evoice->Delta = voice->Delta;
			evoice->spurious_threshold = voice->spurious_threshold;
			evoice->LBA = voice->LBA;
			evoice->CSO = 0;
			evoice->ESO = (runtime->period_size * 2) + 4 - 1; /* in samples */
			evoice->CTRL = voice->CTRL;
			evoice->FMC = 3;
			evoice->GVSel = trident->device == TRIDENT_DEVICE_ID_SI7018 ? 0 : 1;
			evoice->EC = 0;
			evoice->Alpha = 0;
			evoice->FMS = 0;
			evoice->Vol = 0x3ff;			/* mute */
			evoice->RVol = evoice->CVol = 0x7f;	/* mute */
			evoice->Pan = 0x7f;			/* mute */
			evoice->Attribute = 0;
			snd_trident_write_voice_regs(trident, evoice);
			evoice->isync2 = 1;
			evoice->isync_mark = runtime->period_size;
			evoice->ESO = (runtime->period_size * 2) - 1;
		}

		outl(trident->spdif_pcm_bits, TRID_REG(trident, SI_SPDIF_CS));
		temp = inl(TRID_REG(trident, T4D_LFO_GC_CIR));
		temp &= ~(1<<19);
		outl(temp, TRID_REG(trident, T4D_LFO_GC_CIR));
		temp = inl(TRID_REG(trident, SI_SERIAL_INTF_CTRL));
		temp |= SPDIF_EN;
		outl(temp, TRID_REG(trident, SI_SERIAL_INTF_CTRL));
	}

	spin_unlock_irq(&trident->reg_lock);

	return 0;
}

/*---------------------------------------------------------------------------
   snd_trident_trigger
  
   Description: Start/stop devices
  
   Parameters:  substream  - PCM substream class
   		cmd	- trigger command (STOP, GO)
  
   Returns:     Error status
  
  ---------------------------------------------------------------------------*/

static int snd_trident_trigger(struct snd_pcm_substream *substream,
			       int cmd)
				    
{
	struct snd_trident *trident = snd_pcm_substream_chip(substream);
	struct snd_pcm_substream *s;
	unsigned int what, whati, capture_flag, spdif_flag;
	struct snd_trident_voice *voice, *evoice;
	unsigned int val, go;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		go = 1;
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		go = 0;
		break;
	default:
		return -EINVAL;
	}
	what = whati = capture_flag = spdif_flag = 0;
	spin_lock(&trident->reg_lock);
	val = inl(TRID_REG(trident, T4D_STIMER)) & 0x00ffffff;
	snd_pcm_group_for_each_entry(s, substream) {
		if ((struct snd_trident *) snd_pcm_substream_chip(s) == trident) {
			voice = s->runtime->private_data;
			evoice = voice->extra;
			what |= 1 << (voice->number & 0x1f);
			if (evoice == NULL) {
				whati |= 1 << (voice->number & 0x1f);
			} else {
				what |= 1 << (evoice->number & 0x1f);
				whati |= 1 << (evoice->number & 0x1f);
				if (go)
					evoice->stimer = val;
			}
			if (go) {
				voice->running = 1;
				voice->stimer = val;
			} else {
				voice->running = 0;
			}
			snd_pcm_trigger_done(s, substream);
			if (voice->capture)
				capture_flag = 1;
			if (voice->spdif)
				spdif_flag = 1;
		}
	}
	if (spdif_flag) {
		if (trident->device != TRIDENT_DEVICE_ID_SI7018) {
			outl(trident->spdif_pcm_bits, TRID_REG(trident, NX_SPCSTATUS));
			val = trident->spdif_pcm_ctrl;
			if (!go)
				val &= ~(0x28);
			outb(val, TRID_REG(trident, NX_SPCTRL_SPCSO + 3));
		} else {
			outl(trident->spdif_pcm_bits, TRID_REG(trident, SI_SPDIF_CS));
			val = inl(TRID_REG(trident, SI_SERIAL_INTF_CTRL)) | SPDIF_EN;
			outl(val, TRID_REG(trident, SI_SERIAL_INTF_CTRL));
		}
	}
	if (!go)
		outl(what, TRID_REG(trident, T4D_STOP_B));
	val = inl(TRID_REG(trident, T4D_AINTEN_B));
	if (go) {
		val |= whati;
	} else {
		val &= ~whati;
	}
	outl(val, TRID_REG(trident, T4D_AINTEN_B));
	if (go) {
		outl(what, TRID_REG(trident, T4D_START_B));

		if (capture_flag && trident->device != TRIDENT_DEVICE_ID_SI7018)
			outb(trident->bDMAStart, TRID_REG(trident, T4D_SBCTRL_SBE2R_SBDD));
	} else {
		if (capture_flag && trident->device != TRIDENT_DEVICE_ID_SI7018)
			outb(0x00, TRID_REG(trident, T4D_SBCTRL_SBE2R_SBDD));
	}
	spin_unlock(&trident->reg_lock);
	return 0;
}

/*---------------------------------------------------------------------------
   snd_trident_playback_pointer
  
   Description: This routine return the playback position
                
   Parameters:	substream  - PCM substream class

   Returns:     position of buffer
  
  ---------------------------------------------------------------------------*/

static snd_pcm_uframes_t snd_trident_playback_pointer(struct snd_pcm_substream *substream)
{
	struct snd_trident *trident = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_trident_voice *voice = runtime->private_data;
	unsigned int cso;

	if (!voice->running)
		return 0;

	spin_lock(&trident->reg_lock);

	outb(voice->number, TRID_REG(trident, T4D_LFO_GC_CIR));

	if (trident->device != TRIDENT_DEVICE_ID_NX) {
		cso = inw(TRID_REG(trident, CH_DX_CSO_ALPHA_FMS + 2));
	} else {		// ID_4DWAVE_NX
		cso = (unsigned int) inl(TRID_REG(trident, CH_NX_DELTA_CSO)) & 0x00ffffff;
	}

	spin_unlock(&trident->reg_lock);

	if (cso >= runtime->buffer_size)
		cso = 0;

	return cso;
}

/*---------------------------------------------------------------------------
   snd_trident_capture_pointer
  
   Description: This routine return the capture position
                
   Parameters:   pcm1    - PCM device class

   Returns:     position of buffer
  
  ---------------------------------------------------------------------------*/

static snd_pcm_uframes_t snd_trident_capture_pointer(struct snd_pcm_substream *substream)
{
	struct snd_trident *trident = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_trident_voice *voice = runtime->private_data;
	unsigned int result;

	if (!voice->running)
		return 0;

	result = inw(TRID_REG(trident, T4D_SBBL_SBCL));
	if (runtime->channels > 1)
		result >>= 1;
	if (result > 0)
		result = runtime->buffer_size - result;

	return result;
}

/*---------------------------------------------------------------------------
   snd_trident_spdif_pointer
  
   Description: This routine return the SPDIF playback position
                
   Parameters:	substream  - PCM substream class

   Returns:     position of buffer
  
  ---------------------------------------------------------------------------*/

static snd_pcm_uframes_t snd_trident_spdif_pointer(struct snd_pcm_substream *substream)
{
	struct snd_trident *trident = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_trident_voice *voice = runtime->private_data;
	unsigned int result;

	if (!voice->running)
		return 0;

	result = inl(TRID_REG(trident, NX_SPCTRL_SPCSO)) & 0x00ffffff;

	return result;
}

/*
 *  Playback support device description
 */

static struct snd_pcm_hardware snd_trident_playback =
{
	.info =			(SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
				 SNDRV_PCM_INFO_BLOCK_TRANSFER |
				 SNDRV_PCM_INFO_MMAP_VALID | SNDRV_PCM_INFO_SYNC_START |
				 SNDRV_PCM_INFO_PAUSE /* | SNDRV_PCM_INFO_RESUME */),
	.formats =		(SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S16_LE |
				 SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_U16_LE),
	.rates =		SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000,
	.rate_min =		4000,
	.rate_max =		48000,
	.channels_min =		1,
	.channels_max =		2,
	.buffer_bytes_max =	(256*1024),
	.period_bytes_min =	64,
	.period_bytes_max =	(256*1024),
	.periods_min =		1,
	.periods_max =		1024,
	.fifo_size =		0,
};

/*
 *  Capture support device description
 */

static struct snd_pcm_hardware snd_trident_capture =
{
	.info =			(SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
				 SNDRV_PCM_INFO_BLOCK_TRANSFER |
				 SNDRV_PCM_INFO_MMAP_VALID | SNDRV_PCM_INFO_SYNC_START |
				 SNDRV_PCM_INFO_PAUSE /* | SNDRV_PCM_INFO_RESUME */),
	.formats =		(SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S16_LE |
				 SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_U16_LE),
	.rates =		SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000,
	.rate_min =		4000,
	.rate_max =		48000,
	.channels_min =		1,
	.channels_max =		2,
	.buffer_bytes_max =	(128*1024),
	.period_bytes_min =	64,
	.period_bytes_max =	(128*1024),
	.periods_min =		1,
	.periods_max =		1024,
	.fifo_size =		0,
};

/*
 *  Foldback capture support device description
 */

static struct snd_pcm_hardware snd_trident_foldback =
{
	.info =			(SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
				 SNDRV_PCM_INFO_BLOCK_TRANSFER |
				 SNDRV_PCM_INFO_MMAP_VALID | SNDRV_PCM_INFO_SYNC_START |
				 SNDRV_PCM_INFO_PAUSE /* | SNDRV_PCM_INFO_RESUME */),
	.formats =		SNDRV_PCM_FMTBIT_S16_LE,
	.rates =		SNDRV_PCM_RATE_48000,
	.rate_min =		48000,
	.rate_max =		48000,
	.channels_min =		2,
	.channels_max =		2,
	.buffer_bytes_max =	(128*1024),
	.period_bytes_min =	64,
	.period_bytes_max =	(128*1024),
	.periods_min =		1,
	.periods_max =		1024,
	.fifo_size =		0,
};

/*
 *  SPDIF playback support device description
 */

static struct snd_pcm_hardware snd_trident_spdif =
{
	.info =			(SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
				 SNDRV_PCM_INFO_BLOCK_TRANSFER |
				 SNDRV_PCM_INFO_MMAP_VALID | SNDRV_PCM_INFO_SYNC_START |
				 SNDRV_PCM_INFO_PAUSE /* | SNDRV_PCM_INFO_RESUME */),
	.formats =		SNDRV_PCM_FMTBIT_S16_LE,
	.rates =		(SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |
				 SNDRV_PCM_RATE_48000),
	.rate_min =		32000,
	.rate_max =		48000,
	.channels_min =		2,
	.channels_max =		2,
	.buffer_bytes_max =	(128*1024),
	.period_bytes_min =	64,
	.period_bytes_max =	(128*1024),
	.periods_min =		1,
	.periods_max =		1024,
	.fifo_size =		0,
};

static struct snd_pcm_hardware snd_trident_spdif_7018 =
{
	.info =			(SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
				 SNDRV_PCM_INFO_BLOCK_TRANSFER |
				 SNDRV_PCM_INFO_MMAP_VALID | SNDRV_PCM_INFO_SYNC_START |
				 SNDRV_PCM_INFO_PAUSE /* | SNDRV_PCM_INFO_RESUME */),
	.formats =		SNDRV_PCM_FMTBIT_S16_LE,
	.rates =		SNDRV_PCM_RATE_48000,
	.rate_min =		48000,
	.rate_max =		48000,
	.channels_min =		2,
	.channels_max =		2,
	.buffer_bytes_max =	(128*1024),
	.period_bytes_min =	64,
	.period_bytes_max =	(128*1024),
	.periods_min =		1,
	.periods_max =		1024,
	.fifo_size =		0,
};

static void snd_trident_pcm_free_substream(struct snd_pcm_runtime *runtime)
{
	struct snd_trident_voice *voice = runtime->private_data;
	struct snd_trident *trident;

	if (voice) {
		trident = voice->trident;
		snd_trident_free_voice(trident, voice);
	}
}

static int snd_trident_playback_open(struct snd_pcm_substream *substream)
{
	struct snd_trident *trident = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_trident_voice *voice;

	voice = snd_trident_alloc_voice(trident, SNDRV_TRIDENT_VOICE_TYPE_PCM, 0, 0);
	if (voice == NULL)
		return -EAGAIN;
	snd_trident_pcm_mixer_build(trident, voice, substream);
	voice->substream = substream;
	runtime->private_data = voice;
	runtime->private_free = snd_trident_pcm_free_substream;
	runtime->hw = snd_trident_playback;
	snd_pcm_set_sync(substream);
	snd_pcm_hw_constraint_minmax(runtime, SNDRV_PCM_HW_PARAM_BUFFER_SIZE, 0, 64*1024);
	return 0;
}

/*---------------------------------------------------------------------------
   snd_trident_playback_close
  
   Description: This routine will close the 4DWave playback device. For now 
                we will simply free the dma transfer buffer.
                
   Parameters:	substream  - PCM substream class

  ---------------------------------------------------------------------------*/
static int snd_trident_playback_close(struct snd_pcm_substream *substream)
{
	struct snd_trident *trident = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_trident_voice *voice = runtime->private_data;

	snd_trident_pcm_mixer_free(trident, voice, substream);
	return 0;
}

/*---------------------------------------------------------------------------
   snd_trident_spdif_open
  
   Description: This routine will open the 4DWave SPDIF device.

   Parameters:	substream  - PCM substream class

   Returns:     status  - success or failure flag
  
  ---------------------------------------------------------------------------*/

static int snd_trident_spdif_open(struct snd_pcm_substream *substream)
{
	struct snd_trident *trident = snd_pcm_substream_chip(substream);
	struct snd_trident_voice *voice;
	struct snd_pcm_runtime *runtime = substream->runtime;
	
	voice = snd_trident_alloc_voice(trident, SNDRV_TRIDENT_VOICE_TYPE_PCM, 0, 0);
	if (voice == NULL)
		return -EAGAIN;
	voice->spdif = 1;
	voice->substream = substream;
	spin_lock_irq(&trident->reg_lock);
	trident->spdif_pcm_bits = trident->spdif_bits;
	spin_unlock_irq(&trident->reg_lock);

	runtime->private_data = voice;
	runtime->private_free = snd_trident_pcm_free_substream;
	if (trident->device == TRIDENT_DEVICE_ID_SI7018) {
		runtime->hw = snd_trident_spdif;
	} else {
		runtime->hw = snd_trident_spdif_7018;
	}

	trident->spdif_pcm_ctl->vd[0].access &= ~SNDRV_CTL_ELEM_ACCESS_INACTIVE;
	snd_ctl_notify(trident->card, SNDRV_CTL_EVENT_MASK_VALUE |
		       SNDRV_CTL_EVENT_MASK_INFO, &trident->spdif_pcm_ctl->id);

	snd_pcm_hw_constraint_minmax(runtime, SNDRV_PCM_HW_PARAM_BUFFER_SIZE, 0, 64*1024);
	return 0;
}


/*---------------------------------------------------------------------------
   snd_trident_spdif_close
  
   Description: This routine will close the 4DWave SPDIF device.
                
   Parameters:	substream  - PCM substream class

  ---------------------------------------------------------------------------*/

static int snd_trident_spdif_close(struct snd_pcm_substream *substream)
{
	struct snd_trident *trident = snd_pcm_substream_chip(substream);
	unsigned int temp;

	spin_lock_irq(&trident->reg_lock);
	// restore default SPDIF setting
	if (trident->device != TRIDENT_DEVICE_ID_SI7018) {
		outb(trident->spdif_ctrl, TRID_REG(trident, NX_SPCTRL_SPCSO + 3));
		outl(trident->spdif_bits, TRID_REG(trident, NX_SPCSTATUS));
	} else {
		outl(trident->spdif_bits, TRID_REG(trident, SI_SPDIF_CS));
		temp = inl(TRID_REG(trident, SI_SERIAL_INTF_CTRL));
		if (trident->spdif_ctrl) {
			temp |= SPDIF_EN;
		} else {
			temp &= ~SPDIF_EN;
		}
		outl(temp, TRID_REG(trident, SI_SERIAL_INTF_CTRL));
	}
	spin_unlock_irq(&trident->reg_lock);
	trident->spdif_pcm_ctl->vd[0].access |= SNDRV_CTL_ELEM_ACCESS_INACTIVE;
	snd_ctl_notify(trident->card, SNDRV_CTL_EVENT_MASK_VALUE |
		       SNDRV_CTL_EVENT_MASK_INFO, &trident->spdif_pcm_ctl->id);
	return 0;
}

/*---------------------------------------------------------------------------
   snd_trident_capture_open
  
   Description: This routine will open the 4DWave capture device.

   Parameters:	substream  - PCM substream class

   Returns:     status  - success or failure flag

  ---------------------------------------------------------------------------*/

static int snd_trident_capture_open(struct snd_pcm_substream *substream)
{
	struct snd_trident *trident = snd_pcm_substream_chip(substream);
	struct snd_trident_voice *voice;
	struct snd_pcm_runtime *runtime = substream->runtime;

	voice = snd_trident_alloc_voice(trident, SNDRV_TRIDENT_VOICE_TYPE_PCM, 0, 0);
	if (voice == NULL)
		return -EAGAIN;
	voice->capture = 1;
	voice->substream = substream;
	runtime->private_data = voice;
	runtime->private_free = snd_trident_pcm_free_substream;
	runtime->hw = snd_trident_capture;
	snd_pcm_set_sync(substream);
	snd_pcm_hw_constraint_minmax(runtime, SNDRV_PCM_HW_PARAM_BUFFER_SIZE, 0, 64*1024);
	return 0;
}

/*---------------------------------------------------------------------------
   snd_trident_capture_close
  
   Description: This routine will close the 4DWave capture device. For now 
                we will simply free the dma transfer buffer.
                
   Parameters:	substream  - PCM substream class

  ---------------------------------------------------------------------------*/
static int snd_trident_capture_close(struct snd_pcm_substream *substream)
{
	return 0;
}

/*---------------------------------------------------------------------------
   snd_trident_foldback_open
  
   Description: This routine will open the 4DWave foldback capture device.

   Parameters:	substream  - PCM substream class

   Returns:     status  - success or failure flag

  ---------------------------------------------------------------------------*/

static int snd_trident_foldback_open(struct snd_pcm_substream *substream)
{
	struct snd_trident *trident = snd_pcm_substream_chip(substream);
	struct snd_trident_voice *voice;
	struct snd_pcm_runtime *runtime = substream->runtime;

	voice = snd_trident_alloc_voice(trident, SNDRV_TRIDENT_VOICE_TYPE_PCM, 0, 0);
	if (voice == NULL)
		return -EAGAIN;
	voice->foldback_chan = substream->number;
	voice->substream = substream;
	runtime->private_data = voice;
	runtime->private_free = snd_trident_pcm_free_substream;
	runtime->hw = snd_trident_foldback;
	snd_pcm_hw_constraint_minmax(runtime, SNDRV_PCM_HW_PARAM_BUFFER_SIZE, 0, 64*1024);
	return 0;
}

/*---------------------------------------------------------------------------
   snd_trident_foldback_close
  
   Description: This routine will close the 4DWave foldback capture device. 
		For now we will simply free the dma transfer buffer.
                
   Parameters:	substream  - PCM substream class

  ---------------------------------------------------------------------------*/
static int snd_trident_foldback_close(struct snd_pcm_substream *substream)
{
	struct snd_trident *trident = snd_pcm_substream_chip(substream);
	struct snd_trident_voice *voice;
	struct snd_pcm_runtime *runtime = substream->runtime;
	voice = runtime->private_data;
	
	/* stop capture channel */
	spin_lock_irq(&trident->reg_lock);
	outb(0x00, TRID_REG(trident, T4D_RCI + voice->foldback_chan));
	spin_unlock_irq(&trident->reg_lock);
	return 0;
}

/*---------------------------------------------------------------------------
   PCM operations
  ---------------------------------------------------------------------------*/

static struct snd_pcm_ops snd_trident_playback_ops = {
	.open =		snd_trident_playback_open,
	.close =	snd_trident_playback_close,
	.ioctl =	snd_trident_ioctl,
	.hw_params =	snd_trident_hw_params,
	.hw_free =	snd_trident_hw_free,
	.prepare =	snd_trident_playback_prepare,
	.trigger =	snd_trident_trigger,
	.pointer =	snd_trident_playback_pointer,
};

static struct snd_pcm_ops snd_trident_nx_playback_ops = {
	.open =		snd_trident_playback_open,
	.close =	snd_trident_playback_close,
	.ioctl =	snd_trident_ioctl,
	.hw_params =	snd_trident_hw_params,
	.hw_free =	snd_trident_hw_free,
	.prepare =	snd_trident_playback_prepare,
	.trigger =	snd_trident_trigger,
	.pointer =	snd_trident_playback_pointer,
	.page =		snd_pcm_sgbuf_ops_page,
};

static struct snd_pcm_ops snd_trident_capture_ops = {
	.open =		snd_trident_capture_open,
	.close =	snd_trident_capture_close,
	.ioctl =	snd_trident_ioctl,
	.hw_params =	snd_trident_capture_hw_params,
	.hw_free =	snd_trident_hw_free,
	.prepare =	snd_trident_capture_prepare,
	.trigger =	snd_trident_trigger,
	.pointer =	snd_trident_capture_pointer,
};

static struct snd_pcm_ops snd_trident_si7018_capture_ops = {
	.open =		snd_trident_capture_open,
	.close =	snd_trident_capture_close,
	.ioctl =	snd_trident_ioctl,
	.hw_params =	snd_trident_si7018_capture_hw_params,
	.hw_free =	snd_trident_si7018_capture_hw_free,
	.prepare =	snd_trident_si7018_capture_prepare,
	.trigger =	snd_trident_trigger,
	.pointer =	snd_trident_playback_pointer,
};

static struct snd_pcm_ops snd_trident_foldback_ops = {
	.open =		snd_trident_foldback_open,
	.close =	snd_trident_foldback_close,
	.ioctl =	snd_trident_ioctl,
	.hw_params =	snd_trident_hw_params,
	.hw_free =	snd_trident_hw_free,
	.prepare =	snd_trident_foldback_prepare,
	.trigger =	snd_trident_trigger,
	.pointer =	snd_trident_playback_pointer,
};

static struct snd_pcm_ops snd_trident_nx_foldback_ops = {
	.open =		snd_trident_foldback_open,
	.close =	snd_trident_foldback_close,
	.ioctl =	snd_trident_ioctl,
	.hw_params =	snd_trident_hw_params,
	.hw_free =	snd_trident_hw_free,
	.prepare =	snd_trident_foldback_prepare,
	.trigger =	snd_trident_trigger,
	.pointer =	snd_trident_playback_pointer,
	.page =		snd_pcm_sgbuf_ops_page,
};

static struct snd_pcm_ops snd_trident_spdif_ops = {
	.open =		snd_trident_spdif_open,
	.close =	snd_trident_spdif_close,
	.ioctl =	snd_trident_ioctl,
	.hw_params =	snd_trident_spdif_hw_params,
	.hw_free =	snd_trident_hw_free,
	.prepare =	snd_trident_spdif_prepare,
	.trigger =	snd_trident_trigger,
	.pointer =	snd_trident_spdif_pointer,
};

static struct snd_pcm_ops snd_trident_spdif_7018_ops = {
	.open =		snd_trident_spdif_open,
	.close =	snd_trident_spdif_close,
	.ioctl =	snd_trident_ioctl,
	.hw_params =	snd_trident_spdif_hw_params,
	.hw_free =	snd_trident_hw_free,
	.prepare =	snd_trident_spdif_prepare,
	.trigger =	snd_trident_trigger,
	.pointer =	snd_trident_playback_pointer,
};

/*---------------------------------------------------------------------------
   snd_trident_pcm
  
   Description: This routine registers the 4DWave device for PCM support.
                
   Parameters:  trident - pointer to target device class for 4DWave.

   Returns:     None
  
  ---------------------------------------------------------------------------*/

int __devinit snd_trident_pcm(struct snd_trident * trident,
			      int device, struct snd_pcm ** rpcm)
{
	struct snd_pcm *pcm;
	int err;

	if (rpcm)
		*rpcm = NULL;
	if ((err = snd_pcm_new(trident->card, "trident_dx_nx", device, trident->ChanPCM, 1, &pcm)) < 0)
		return err;

	pcm->private_data = trident;

	if (trident->tlb.entries) {
		snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &snd_trident_nx_playback_ops);
	} else {
		snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &snd_trident_playback_ops);
	}
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE,
			trident->device != TRIDENT_DEVICE_ID_SI7018 ?
			&snd_trident_capture_ops :
			&snd_trident_si7018_capture_ops);

	pcm->info_flags = 0;
	pcm->dev_subclass = SNDRV_PCM_SUBCLASS_GENERIC_MIX;
	strcpy(pcm->name, "Trident 4DWave");
	trident->pcm = pcm;

	if (trident->tlb.entries) {
		struct snd_pcm_substream *substream;
		for (substream = pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream; substream; substream = substream->next)
			snd_pcm_lib_preallocate_pages(substream, SNDRV_DMA_TYPE_DEV_SG,
						      snd_dma_pci_data(trident->pci),
						      64*1024, 128*1024);
		snd_pcm_lib_preallocate_pages(pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream,
					      SNDRV_DMA_TYPE_DEV, snd_dma_pci_data(trident->pci),
					      64*1024, 128*1024);
	} else {
		snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV,
						      snd_dma_pci_data(trident->pci), 64*1024, 128*1024);
	}

	if (rpcm)
		*rpcm = pcm;
	return 0;
}

/*---------------------------------------------------------------------------
   snd_trident_foldback_pcm
  
   Description: This routine registers the 4DWave device for foldback PCM support.
                
   Parameters:  trident - pointer to target device class for 4DWave.

   Returns:     None
  
  ---------------------------------------------------------------------------*/

int __devinit snd_trident_foldback_pcm(struct snd_trident * trident,
				       int device, struct snd_pcm ** rpcm)
{
	struct snd_pcm *foldback;
	int err;
	int num_chan = 3;
	struct snd_pcm_substream *substream;

	if (rpcm)
		*rpcm = NULL;
	if (trident->device == TRIDENT_DEVICE_ID_NX)
		num_chan = 4;
	if ((err = snd_pcm_new(trident->card, "trident_dx_nx", device, 0, num_chan, &foldback)) < 0)
		return err;

	foldback->private_data = trident;
	if (trident->tlb.entries)
		snd_pcm_set_ops(foldback, SNDRV_PCM_STREAM_CAPTURE, &snd_trident_nx_foldback_ops);
	else
		snd_pcm_set_ops(foldback, SNDRV_PCM_STREAM_CAPTURE, &snd_trident_foldback_ops);
	foldback->info_flags = 0;
	strcpy(foldback->name, "Trident 4DWave");
	substream = foldback->streams[SNDRV_PCM_STREAM_CAPTURE].substream;
	strcpy(substream->name, "Front Mixer");
	substream = substream->next;
	strcpy(substream->name, "Reverb Mixer");
	substream = substream->next;
	strcpy(substream->name, "Chorus Mixer");
	if (num_chan == 4) {
		substream = substream->next;
		strcpy(substream->name, "Second AC'97 ADC");
	}
	trident->foldback = foldback;

	if (trident->tlb.entries)
		snd_pcm_lib_preallocate_pages_for_all(foldback, SNDRV_DMA_TYPE_DEV_SG,
						      snd_dma_pci_data(trident->pci), 0, 128*1024);
	else
		snd_pcm_lib_preallocate_pages_for_all(foldback, SNDRV_DMA_TYPE_DEV,
						      snd_dma_pci_data(trident->pci), 64*1024, 128*1024);

	if (rpcm)
		*rpcm = foldback;
	return 0;
}

/*---------------------------------------------------------------------------
   snd_trident_spdif
  
   Description: This routine registers the 4DWave-NX device for SPDIF support.
                
   Parameters:  trident - pointer to target device class for 4DWave-NX.

   Returns:     None
  
  ---------------------------------------------------------------------------*/

int __devinit snd_trident_spdif_pcm(struct snd_trident * trident,
				    int device, struct snd_pcm ** rpcm)
{
	struct snd_pcm *spdif;
	int err;

	if (rpcm)
		*rpcm = NULL;
	if ((err = snd_pcm_new(trident->card, "trident_dx_nx IEC958", device, 1, 0, &spdif)) < 0)
		return err;

	spdif->private_data = trident;
	if (trident->device != TRIDENT_DEVICE_ID_SI7018) {
		snd_pcm_set_ops(spdif, SNDRV_PCM_STREAM_PLAYBACK, &snd_trident_spdif_ops);
	} else {
		snd_pcm_set_ops(spdif, SNDRV_PCM_STREAM_PLAYBACK, &snd_trident_spdif_7018_ops);
	}
	spdif->info_flags = 0;
	strcpy(spdif->name, "Trident 4DWave IEC958");
	trident->spdif = spdif;

	snd_pcm_lib_preallocate_pages_for_all(spdif, SNDRV_DMA_TYPE_DEV, snd_dma_pci_data(trident->pci), 64*1024, 128*1024);

	if (rpcm)
		*rpcm = spdif;
	return 0;
}

/*
 *  Mixer part
 */


/*---------------------------------------------------------------------------
    snd_trident_spdif_control

    Description: enable/disable S/PDIF out from ac97 mixer
  ---------------------------------------------------------------------------*/

#define snd_trident_spdif_control_info	snd_ctl_boolean_mono_info

static int snd_trident_spdif_control_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_trident *trident = snd_kcontrol_chip(kcontrol);
	unsigned char val;

	spin_lock_irq(&trident->reg_lock);
	val = trident->spdif_ctrl;
	ucontrol->value.integer.value[0] = val == kcontrol->private_value;
	spin_unlock_irq(&trident->reg_lock);
	return 0;
}

static int snd_trident_spdif_control_put(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_trident *trident = snd_kcontrol_chip(kcontrol);
	unsigned char val;
	int change;

	val = ucontrol->value.integer.value[0] ? (unsigned char) kcontrol->private_value : 0x00;
	spin_lock_irq(&trident->reg_lock);
	/* S/PDIF C Channel bits 0-31 : 48khz, SCMS disabled */
	change = trident->spdif_ctrl != val;
	trident->spdif_ctrl = val;
	if (trident->device != TRIDENT_DEVICE_ID_SI7018) {
		if ((inb(TRID_REG(trident, NX_SPCTRL_SPCSO + 3)) & 0x10) == 0) {
			outl(trident->spdif_bits, TRID_REG(trident, NX_SPCSTATUS));
			outb(trident->spdif_ctrl, TRID_REG(trident, NX_SPCTRL_SPCSO + 3));
		}
	} else {
		if (trident->spdif == NULL) {
			unsigned int temp;
			outl(trident->spdif_bits, TRID_REG(trident, SI_SPDIF_CS));
			temp = inl(TRID_REG(trident, SI_SERIAL_INTF_CTRL)) & ~SPDIF_EN;
			if (val)
				temp |= SPDIF_EN;
			outl(temp, TRID_REG(trident, SI_SERIAL_INTF_CTRL));
		}
	}
	spin_unlock_irq(&trident->reg_lock);
	return change;
}

static struct snd_kcontrol_new snd_trident_spdif_control __devinitdata =
{
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =         SNDRV_CTL_NAME_IEC958("",PLAYBACK,SWITCH),
	.info =		snd_trident_spdif_control_info,
	.get =		snd_trident_spdif_control_get,
	.put =		snd_trident_spdif_control_put,
	.private_value = 0x28,
};

/*---------------------------------------------------------------------------
    snd_trident_spdif_default

    Description: put/get the S/PDIF default settings
  ---------------------------------------------------------------------------*/

static int snd_trident_spdif_default_info(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;
	return 0;
}

static int snd_trident_spdif_default_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_trident *trident = snd_kcontrol_chip(kcontrol);

	spin_lock_irq(&trident->reg_lock);
	ucontrol->value.iec958.status[0] = (trident->spdif_bits >> 0) & 0xff;
	ucontrol->value.iec958.status[1] = (trident->spdif_bits >> 8) & 0xff;
	ucontrol->value.iec958.status[2] = (trident->spdif_bits >> 16) & 0xff;
	ucontrol->value.iec958.status[3] = (trident->spdif_bits >> 24) & 0xff;
	spin_unlock_irq(&trident->reg_lock);
	return 0;
}

static int snd_trident_spdif_default_put(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_trident *trident = snd_kcontrol_chip(kcontrol);
	unsigned int val;
	int change;

	val = (ucontrol->value.iec958.status[0] << 0) |
	      (ucontrol->value.iec958.status[1] << 8) |
	      (ucontrol->value.iec958.status[2] << 16) |
	      (ucontrol->value.iec958.status[3] << 24);
	spin_lock_irq(&trident->reg_lock);
	change = trident->spdif_bits != val;
	trident->spdif_bits = val;
	if (trident->device != TRIDENT_DEVICE_ID_SI7018) {
		if ((inb(TRID_REG(trident, NX_SPCTRL_SPCSO + 3)) & 0x10) == 0)
			outl(trident->spdif_bits, TRID_REG(trident, NX_SPCSTATUS));
	} else {
		if (trident->spdif == NULL)
			outl(trident->spdif_bits, TRID_REG(trident, SI_SPDIF_CS));
	}
	spin_unlock_irq(&trident->reg_lock);
	return change;
}

static struct snd_kcontrol_new snd_trident_spdif_default __devinitdata =
{
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =         SNDRV_CTL_NAME_IEC958("",PLAYBACK,DEFAULT),
	.info =		snd_trident_spdif_default_info,
	.get =		snd_trident_spdif_default_get,
	.put =		snd_trident_spdif_default_put
};

/*---------------------------------------------------------------------------
    snd_trident_spdif_mask

    Description: put/get the S/PDIF mask
  ---------------------------------------------------------------------------*/

static int snd_trident_spdif_mask_info(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;
	return 0;
}

static int snd_trident_spdif_mask_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.iec958.status[0] = 0xff;
	ucontrol->value.iec958.status[1] = 0xff;
	ucontrol->value.iec958.status[2] = 0xff;
	ucontrol->value.iec958.status[3] = 0xff;
	return 0;
}

static struct snd_kcontrol_new snd_trident_spdif_mask __devinitdata =
{
	.access =	SNDRV_CTL_ELEM_ACCESS_READ,
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =         SNDRV_CTL_NAME_IEC958("",PLAYBACK,MASK),
	.info =		snd_trident_spdif_mask_info,
	.get =		snd_trident_spdif_mask_get,
};

/*---------------------------------------------------------------------------
    snd_trident_spdif_stream

    Description: put/get the S/PDIF stream settings
  ---------------------------------------------------------------------------*/

static int snd_trident_spdif_stream_info(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;
	return 0;
}

static int snd_trident_spdif_stream_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_trident *trident = snd_kcontrol_chip(kcontrol);

	spin_lock_irq(&trident->reg_lock);
	ucontrol->value.iec958.status[0] = (trident->spdif_pcm_bits >> 0) & 0xff;
	ucontrol->value.iec958.status[1] = (trident->spdif_pcm_bits >> 8) & 0xff;
	ucontrol->value.iec958.status[2] = (trident->spdif_pcm_bits >> 16) & 0xff;
	ucontrol->value.iec958.status[3] = (trident->spdif_pcm_bits >> 24) & 0xff;
	spin_unlock_irq(&trident->reg_lock);
	return 0;
}

static int snd_trident_spdif_stream_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_trident *trident = snd_kcontrol_chip(kcontrol);
	unsigned int val;
	int change;

	val = (ucontrol->value.iec958.status[0] << 0) |
	      (ucontrol->value.iec958.status[1] << 8) |
	      (ucontrol->value.iec958.status[2] << 16) |
	      (ucontrol->value.iec958.status[3] << 24);
	spin_lock_irq(&trident->reg_lock);
	change = trident->spdif_pcm_bits != val;
	trident->spdif_pcm_bits = val;
	if (trident->spdif != NULL) {
		if (trident->device != TRIDENT_DEVICE_ID_SI7018) {
			outl(trident->spdif_pcm_bits, TRID_REG(trident, NX_SPCSTATUS));
		} else {
			outl(trident->spdif_bits, TRID_REG(trident, SI_SPDIF_CS));
		}
	}
	spin_unlock_irq(&trident->reg_lock);
	return change;
}

static struct snd_kcontrol_new snd_trident_spdif_stream __devinitdata =
{
	.access =	SNDRV_CTL_ELEM_ACCESS_READWRITE | SNDRV_CTL_ELEM_ACCESS_INACTIVE,
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =         SNDRV_CTL_NAME_IEC958("",PLAYBACK,PCM_STREAM),
	.info =		snd_trident_spdif_stream_info,
	.get =		snd_trident_spdif_stream_get,
	.put =		snd_trident_spdif_stream_put
};

/*---------------------------------------------------------------------------
    snd_trident_ac97_control

    Description: enable/disable rear path for ac97
  ---------------------------------------------------------------------------*/

#define snd_trident_ac97_control_info	snd_ctl_boolean_mono_info

static int snd_trident_ac97_control_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_trident *trident = snd_kcontrol_chip(kcontrol);
	unsigned char val;

	spin_lock_irq(&trident->reg_lock);
	val = trident->ac97_ctrl = inl(TRID_REG(trident, NX_ACR0_AC97_COM_STAT));
	ucontrol->value.integer.value[0] = (val & (1 << kcontrol->private_value)) ? 1 : 0;
	spin_unlock_irq(&trident->reg_lock);
	return 0;
}

static int snd_trident_ac97_control_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_trident *trident = snd_kcontrol_chip(kcontrol);
	unsigned char val;
	int change = 0;

	spin_lock_irq(&trident->reg_lock);
	val = trident->ac97_ctrl = inl(TRID_REG(trident, NX_ACR0_AC97_COM_STAT));
	val &= ~(1 << kcontrol->private_value);
	if (ucontrol->value.integer.value[0])
		val |= 1 << kcontrol->private_value;
	change = val != trident->ac97_ctrl;
	trident->ac97_ctrl = val;
	outl(trident->ac97_ctrl = val, TRID_REG(trident, NX_ACR0_AC97_COM_STAT));
	spin_unlock_irq(&trident->reg_lock);
	return change;
}

static struct snd_kcontrol_new snd_trident_ac97_rear_control __devinitdata =
{
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =         "Rear Path",
	.info =		snd_trident_ac97_control_info,
	.get =		snd_trident_ac97_control_get,
	.put =		snd_trident_ac97_control_put,
	.private_value = 4,
};

/*---------------------------------------------------------------------------
    snd_trident_vol_control

    Description: wave & music volume control
  ---------------------------------------------------------------------------*/

static int snd_trident_vol_control_info(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 2;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 255;
	return 0;
}

static int snd_trident_vol_control_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_trident *trident = snd_kcontrol_chip(kcontrol);
	unsigned int val;

	val = trident->musicvol_wavevol;
	ucontrol->value.integer.value[0] = 255 - ((val >> kcontrol->private_value) & 0xff);
	ucontrol->value.integer.value[1] = 255 - ((val >> (kcontrol->private_value + 8)) & 0xff);
	return 0;
}

static const DECLARE_TLV_DB_SCALE(db_scale_gvol, -6375, 25, 0);

static int snd_trident_vol_control_put(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_trident *trident = snd_kcontrol_chip(kcontrol);
	unsigned int val;
	int change = 0;

	spin_lock_irq(&trident->reg_lock);
	val = trident->musicvol_wavevol;
	val &= ~(0xffff << kcontrol->private_value);
	val |= ((255 - (ucontrol->value.integer.value[0] & 0xff)) |
	        ((255 - (ucontrol->value.integer.value[1] & 0xff)) << 8)) << kcontrol->private_value;
	change = val != trident->musicvol_wavevol;
	outl(trident->musicvol_wavevol = val, TRID_REG(trident, T4D_MUSICVOL_WAVEVOL));
	spin_unlock_irq(&trident->reg_lock);
	return change;
}

static struct snd_kcontrol_new snd_trident_vol_music_control __devinitdata =
{
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =         "Music Playback Volume",
	.info =		snd_trident_vol_control_info,
	.get =		snd_trident_vol_control_get,
	.put =		snd_trident_vol_control_put,
	.private_value = 16,
	.tlv = { .p = db_scale_gvol },
};

static struct snd_kcontrol_new snd_trident_vol_wave_control __devinitdata =
{
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =         "Wave Playback Volume",
	.info =		snd_trident_vol_control_info,
	.get =		snd_trident_vol_control_get,
	.put =		snd_trident_vol_control_put,
	.private_value = 0,
	.tlv = { .p = db_scale_gvol },
};

/*---------------------------------------------------------------------------
    snd_trident_pcm_vol_control

    Description: PCM front volume control
  ---------------------------------------------------------------------------*/

static int snd_trident_pcm_vol_control_info(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_info *uinfo)
{
	struct snd_trident *trident = snd_kcontrol_chip(kcontrol);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 255;
	if (trident->device == TRIDENT_DEVICE_ID_SI7018)
		uinfo->value.integer.max = 1023;
	return 0;
}

static int snd_trident_pcm_vol_control_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_trident *trident = snd_kcontrol_chip(kcontrol);
	struct snd_trident_pcm_mixer *mix = &trident->pcm_mixer[snd_ctl_get_ioffnum(kcontrol, &ucontrol->id)];

	if (trident->device == TRIDENT_DEVICE_ID_SI7018) {
		ucontrol->value.integer.value[0] = 1023 - mix->vol;
	} else {
		ucontrol->value.integer.value[0] = 255 - (mix->vol>>2);
	}
	return 0;
}

static int snd_trident_pcm_vol_control_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_trident *trident = snd_kcontrol_chip(kcontrol);
	struct snd_trident_pcm_mixer *mix = &trident->pcm_mixer[snd_ctl_get_ioffnum(kcontrol, &ucontrol->id)];
	unsigned int val;
	int change = 0;

	if (trident->device == TRIDENT_DEVICE_ID_SI7018) {
		val = 1023 - (ucontrol->value.integer.value[0] & 1023);
	} else {
		val = (255 - (ucontrol->value.integer.value[0] & 255)) << 2;
	}
	spin_lock_irq(&trident->reg_lock);
	change = val != mix->vol;
	mix->vol = val;
	if (mix->voice != NULL)
		snd_trident_write_vol_reg(trident, mix->voice, val);
	spin_unlock_irq(&trident->reg_lock);
	return change;
}

static struct snd_kcontrol_new snd_trident_pcm_vol_control __devinitdata =
{
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =         "PCM Front Playback Volume",
	.access =	SNDRV_CTL_ELEM_ACCESS_READWRITE | SNDRV_CTL_ELEM_ACCESS_INACTIVE,
	.count =	32,
	.info =		snd_trident_pcm_vol_control_info,
	.get =		snd_trident_pcm_vol_control_get,
	.put =		snd_trident_pcm_vol_control_put,
	/* FIXME: no tlv yet */
};

/*---------------------------------------------------------------------------
    snd_trident_pcm_pan_control

    Description: PCM front pan control
  ---------------------------------------------------------------------------*/

static int snd_trident_pcm_pan_control_info(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 127;
	return 0;
}

static int snd_trident_pcm_pan_control_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_trident *trident = snd_kcontrol_chip(kcontrol);
	struct snd_trident_pcm_mixer *mix = &trident->pcm_mixer[snd_ctl_get_ioffnum(kcontrol, &ucontrol->id)];

	ucontrol->value.integer.value[0] = mix->pan;
	if (ucontrol->value.integer.value[0] & 0x40) {
		ucontrol->value.integer.value[0] = (0x3f - (ucontrol->value.integer.value[0] & 0x3f));
	} else {
		ucontrol->value.integer.value[0] |= 0x40;
	}
	return 0;
}

static int snd_trident_pcm_pan_control_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_trident *trident = snd_kcontrol_chip(kcontrol);
	struct snd_trident_pcm_mixer *mix = &trident->pcm_mixer[snd_ctl_get_ioffnum(kcontrol, &ucontrol->id)];
	unsigned char val;
	int change = 0;

	if (ucontrol->value.integer.value[0] & 0x40)
		val = ucontrol->value.integer.value[0] & 0x3f;
	else
		val = (0x3f - (ucontrol->value.integer.value[0] & 0x3f)) | 0x40;
	spin_lock_irq(&trident->reg_lock);
	change = val != mix->pan;
	mix->pan = val;
	if (mix->voice != NULL)
		snd_trident_write_pan_reg(trident, mix->voice, val);
	spin_unlock_irq(&trident->reg_lock);
	return change;
}

static struct snd_kcontrol_new snd_trident_pcm_pan_control __devinitdata =
{
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =         "PCM Pan Playback Control",
	.access =	SNDRV_CTL_ELEM_ACCESS_READWRITE | SNDRV_CTL_ELEM_ACCESS_INACTIVE,
	.count =	32,
	.info =		snd_trident_pcm_pan_control_info,
	.get =		snd_trident_pcm_pan_control_get,
	.put =		snd_trident_pcm_pan_control_put,
};

/*---------------------------------------------------------------------------
    snd_trident_pcm_rvol_control

    Description: PCM reverb volume control
  ---------------------------------------------------------------------------*/

static int snd_trident_pcm_rvol_control_info(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 127;
	return 0;
}

static int snd_trident_pcm_rvol_control_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_trident *trident = snd_kcontrol_chip(kcontrol);
	struct snd_trident_pcm_mixer *mix = &trident->pcm_mixer[snd_ctl_get_ioffnum(kcontrol, &ucontrol->id)];

	ucontrol->value.integer.value[0] = 127 - mix->rvol;
	return 0;
}

static int snd_trident_pcm_rvol_control_put(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_trident *trident = snd_kcontrol_chip(kcontrol);
	struct snd_trident_pcm_mixer *mix = &trident->pcm_mixer[snd_ctl_get_ioffnum(kcontrol, &ucontrol->id)];
	unsigned short val;
	int change = 0;

	val = 0x7f - (ucontrol->value.integer.value[0] & 0x7f);
	spin_lock_irq(&trident->reg_lock);
	change = val != mix->rvol;
	mix->rvol = val;
	if (mix->voice != NULL)
		snd_trident_write_rvol_reg(trident, mix->voice, val);
	spin_unlock_irq(&trident->reg_lock);
	return change;
}

static const DECLARE_TLV_DB_SCALE(db_scale_crvol, -3175, 25, 1);

static struct snd_kcontrol_new snd_trident_pcm_rvol_control __devinitdata =
{
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =         "PCM Reverb Playback Volume",
	.access =	SNDRV_CTL_ELEM_ACCESS_READWRITE | SNDRV_CTL_ELEM_ACCESS_INACTIVE,
	.count = 	32,
	.info =		snd_trident_pcm_rvol_control_info,
	.get =		snd_trident_pcm_rvol_control_get,
	.put =		snd_trident_pcm_rvol_control_put,
	.tlv = { .p = db_scale_crvol },
};

/*---------------------------------------------------------------------------
    snd_trident_pcm_cvol_control

    Description: PCM chorus volume control
  ---------------------------------------------------------------------------*/

static int snd_trident_pcm_cvol_control_info(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 127;
	return 0;
}

static int snd_trident_pcm_cvol_control_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_trident *trident = snd_kcontrol_chip(kcontrol);
	struct snd_trident_pcm_mixer *mix = &trident->pcm_mixer[snd_ctl_get_ioffnum(kcontrol, &ucontrol->id)];

	ucontrol->value.integer.value[0] = 127 - mix->cvol;
	return 0;
}

static int snd_trident_pcm_cvol_control_put(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_trident *trident = snd_kcontrol_chip(kcontrol);
	struct snd_trident_pcm_mixer *mix = &trident->pcm_mixer[snd_ctl_get_ioffnum(kcontrol, &ucontrol->id)];
	unsigned short val;
	int change = 0;

	val = 0x7f - (ucontrol->value.integer.value[0] & 0x7f);
	spin_lock_irq(&trident->reg_lock);
	change = val != mix->cvol;
	mix->cvol = val;
	if (mix->voice != NULL)
		snd_trident_write_cvol_reg(trident, mix->voice, val);
	spin_unlock_irq(&trident->reg_lock);
	return change;
}

static struct snd_kcontrol_new snd_trident_pcm_cvol_control __devinitdata =
{
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =         "PCM Chorus Playback Volume",
	.access =	SNDRV_CTL_ELEM_ACCESS_READWRITE | SNDRV_CTL_ELEM_ACCESS_INACTIVE,
	.count =	32,
	.info =		snd_trident_pcm_cvol_control_info,
	.get =		snd_trident_pcm_cvol_control_get,
	.put =		snd_trident_pcm_cvol_control_put,
	.tlv = { .p = db_scale_crvol },
};

static void snd_trident_notify_pcm_change1(struct snd_card *card,
					   struct snd_kcontrol *kctl,
					   int num, int activate)
{
	struct snd_ctl_elem_id id;

	if (! kctl)
		return;
	if (activate)
		kctl->vd[num].access &= ~SNDRV_CTL_ELEM_ACCESS_INACTIVE;
	else
		kctl->vd[num].access |= SNDRV_CTL_ELEM_ACCESS_INACTIVE;
	snd_ctl_notify(card, SNDRV_CTL_EVENT_MASK_VALUE |
		       SNDRV_CTL_EVENT_MASK_INFO,
		       snd_ctl_build_ioff(&id, kctl, num));
}

static void snd_trident_notify_pcm_change(struct snd_trident *trident,
					  struct snd_trident_pcm_mixer *tmix,
					  int num, int activate)
{
	snd_trident_notify_pcm_change1(trident->card, trident->ctl_vol, num, activate);
	snd_trident_notify_pcm_change1(trident->card, trident->ctl_pan, num, activate);
	snd_trident_notify_pcm_change1(trident->card, trident->ctl_rvol, num, activate);
	snd_trident_notify_pcm_change1(trident->card, trident->ctl_cvol, num, activate);
}

static int snd_trident_pcm_mixer_build(struct snd_trident *trident,
				       struct snd_trident_voice *voice,
				       struct snd_pcm_substream *substream)
{
	struct snd_trident_pcm_mixer *tmix;

	if (snd_BUG_ON(!trident || !voice || !substream))
		return -EINVAL;
	tmix = &trident->pcm_mixer[substream->number];
	tmix->voice = voice;
	tmix->vol = T4D_DEFAULT_PCM_VOL;
	tmix->pan = T4D_DEFAULT_PCM_PAN;
	tmix->rvol = T4D_DEFAULT_PCM_RVOL;
	tmix->cvol = T4D_DEFAULT_PCM_CVOL;
	snd_trident_notify_pcm_change(trident, tmix, substream->number, 1);
	return 0;
}

static int snd_trident_pcm_mixer_free(struct snd_trident *trident, struct snd_trident_voice *voice, struct snd_pcm_substream *substream)
{
	struct snd_trident_pcm_mixer *tmix;

	if (snd_BUG_ON(!trident || !substream))
		return -EINVAL;
	tmix = &trident->pcm_mixer[substream->number];
	tmix->voice = NULL;
	snd_trident_notify_pcm_change(trident, tmix, substream->number, 0);
	return 0;
}

/*---------------------------------------------------------------------------
   snd_trident_mixer
  
   Description: This routine registers the 4DWave device for mixer support.
                
   Parameters:  trident - pointer to target device class for 4DWave.

   Returns:     None
  
  ---------------------------------------------------------------------------*/

static int __devinit snd_trident_mixer(struct snd_trident * trident, int pcm_spdif_device)
{
	struct snd_ac97_template _ac97;
	struct snd_card *card = trident->card;
	struct snd_kcontrol *kctl;
	struct snd_ctl_elem_value *uctl;
	int idx, err, retries = 2;
	static struct snd_ac97_bus_ops ops = {
		.write = snd_trident_codec_write,
		.read = snd_trident_codec_read,
	};

	uctl = kzalloc(sizeof(*uctl), GFP_KERNEL);
	if (!uctl)
		return -ENOMEM;

	if ((err = snd_ac97_bus(trident->card, 0, &ops, NULL, &trident->ac97_bus)) < 0)
		goto __out;

	memset(&_ac97, 0, sizeof(_ac97));
	_ac97.private_data = trident;
	trident->ac97_detect = 1;

      __again:
	if ((err = snd_ac97_mixer(trident->ac97_bus, &_ac97, &trident->ac97)) < 0) {
		if (trident->device == TRIDENT_DEVICE_ID_SI7018) {
			if ((err = snd_trident_sis_reset(trident)) < 0)
				goto __out;
			if (retries-- > 0)
				goto __again;
			err = -EIO;
		}
		goto __out;
	}
	
	/* secondary codec? */
	if (trident->device == TRIDENT_DEVICE_ID_SI7018 &&
	    (inl(TRID_REG(trident, SI_SERIAL_INTF_CTRL)) & SI_AC97_PRIMARY_READY) != 0) {
		_ac97.num = 1;
		err = snd_ac97_mixer(trident->ac97_bus, &_ac97, &trident->ac97_sec);
		if (err < 0)
			snd_printk(KERN_ERR "SI7018: the secondary codec - invalid access\n");
#if 0	// only for my testing purpose --jk
		{
			struct snd_ac97 *mc97;
			err = snd_ac97_modem(trident->card, &_ac97, &mc97);
			if (err < 0)
				snd_printk(KERN_ERR "snd_ac97_modem returned error %i\n", err);
		}
#endif
	}
	
	trident->ac97_detect = 0;

	if (trident->device != TRIDENT_DEVICE_ID_SI7018) {
		if ((err = snd_ctl_add(card, kctl = snd_ctl_new1(&snd_trident_vol_wave_control, trident))) < 0)
			goto __out;
		kctl->put(kctl, uctl);
		if ((err = snd_ctl_add(card, kctl = snd_ctl_new1(&snd_trident_vol_music_control, trident))) < 0)
			goto __out;
		kctl->put(kctl, uctl);
		outl(trident->musicvol_wavevol = 0x00000000, TRID_REG(trident, T4D_MUSICVOL_WAVEVOL));
	} else {
		outl(trident->musicvol_wavevol = 0xffff0000, TRID_REG(trident, T4D_MUSICVOL_WAVEVOL));
	}

	for (idx = 0; idx < 32; idx++) {
		struct snd_trident_pcm_mixer *tmix;
		
		tmix = &trident->pcm_mixer[idx];
		tmix->voice = NULL;
	}
	if ((trident->ctl_vol = snd_ctl_new1(&snd_trident_pcm_vol_control, trident)) == NULL)
		goto __nomem;
	if ((err = snd_ctl_add(card, trident->ctl_vol)))
		goto __out;
		
	if ((trident->ctl_pan = snd_ctl_new1(&snd_trident_pcm_pan_control, trident)) == NULL)
		goto __nomem;
	if ((err = snd_ctl_add(card, trident->ctl_pan)))
		goto __out;

	if ((trident->ctl_rvol = snd_ctl_new1(&snd_trident_pcm_rvol_control, trident)) == NULL)
		goto __nomem;
	if ((err = snd_ctl_add(card, trident->ctl_rvol)))
		goto __out;

	if ((trident->ctl_cvol = snd_ctl_new1(&snd_trident_pcm_cvol_control, trident)) == NULL)
		goto __nomem;
	if ((err = snd_ctl_add(card, trident->ctl_cvol)))
		goto __out;

	if (trident->device == TRIDENT_DEVICE_ID_NX) {
		if ((err = snd_ctl_add(card, kctl = snd_ctl_new1(&snd_trident_ac97_rear_control, trident))) < 0)
			goto __out;
		kctl->put(kctl, uctl);
	}
	if (trident->device == TRIDENT_DEVICE_ID_NX || trident->device == TRIDENT_DEVICE_ID_SI7018) {

		kctl = snd_ctl_new1(&snd_trident_spdif_control, trident);
		if (kctl == NULL) {
			err = -ENOMEM;
			goto __out;
		}
		if (trident->ac97->ext_id & AC97_EI_SPDIF)
			kctl->id.index++;
		if (trident->ac97_sec && (trident->ac97_sec->ext_id & AC97_EI_SPDIF))
			kctl->id.index++;
		idx = kctl->id.index;
		if ((err = snd_ctl_add(card, kctl)) < 0)
			goto __out;
		kctl->put(kctl, uctl);

		kctl = snd_ctl_new1(&snd_trident_spdif_default, trident);
		if (kctl == NULL) {
			err = -ENOMEM;
			goto __out;
		}
		kctl->id.index = idx;
		kctl->id.device = pcm_spdif_device;
		if ((err = snd_ctl_add(card, kctl)) < 0)
			goto __out;

		kctl = snd_ctl_new1(&snd_trident_spdif_mask, trident);
		if (kctl == NULL) {
			err = -ENOMEM;
			goto __out;
		}
		kctl->id.index = idx;
		kctl->id.device = pcm_spdif_device;
		if ((err = snd_ctl_add(card, kctl)) < 0)
			goto __out;

		kctl = snd_ctl_new1(&snd_trident_spdif_stream, trident);
		if (kctl == NULL) {
			err = -ENOMEM;
			goto __out;
		}
		kctl->id.index = idx;
		kctl->id.device = pcm_spdif_device;
		if ((err = snd_ctl_add(card, kctl)) < 0)
			goto __out;
		trident->spdif_pcm_ctl = kctl;
	}

	err = 0;
	goto __out;

 __nomem:
	err = -ENOMEM;

 __out:
	kfree(uctl);

	return err;
}

/*
 * gameport interface
 */

#if defined(CONFIG_GAMEPORT) || (defined(MODULE) && defined(CONFIG_GAMEPORT_MODULE))

static unsigned char snd_trident_gameport_read(struct gameport *gameport)
{
	struct snd_trident *chip = gameport_get_port_data(gameport);

	if (snd_BUG_ON(!chip))
		return 0;
	return inb(TRID_REG(chip, GAMEPORT_LEGACY));
}

static void snd_trident_gameport_trigger(struct gameport *gameport)
{
	struct snd_trident *chip = gameport_get_port_data(gameport);

	if (snd_BUG_ON(!chip))
		return;
	outb(0xff, TRID_REG(chip, GAMEPORT_LEGACY));
}

static int snd_trident_gameport_cooked_read(struct gameport *gameport, int *axes, int *buttons)
{
	struct snd_trident *chip = gameport_get_port_data(gameport);
	int i;

	if (snd_BUG_ON(!chip))
		return 0;

	*buttons = (~inb(TRID_REG(chip, GAMEPORT_LEGACY)) >> 4) & 0xf;

	for (i = 0; i < 4; i++) {
		axes[i] = inw(TRID_REG(chip, GAMEPORT_AXES + i * 2));
		if (axes[i] == 0xffff) axes[i] = -1;
	}
        
        return 0;
}

static int snd_trident_gameport_open(struct gameport *gameport, int mode)
{
	struct snd_trident *chip = gameport_get_port_data(gameport);

	if (snd_BUG_ON(!chip))
		return 0;

	switch (mode) {
		case GAMEPORT_MODE_COOKED:
			outb(GAMEPORT_MODE_ADC, TRID_REG(chip, GAMEPORT_GCR));
			msleep(20);
			return 0;
		case GAMEPORT_MODE_RAW:
			outb(0, TRID_REG(chip, GAMEPORT_GCR));
			return 0;
		default:
			return -1;
	}
}

int __devinit snd_trident_create_gameport(struct snd_trident *chip)
{
	struct gameport *gp;

	chip->gameport = gp = gameport_allocate_port();
	if (!gp) {
		printk(KERN_ERR "trident: cannot allocate memory for gameport\n");
		return -ENOMEM;
	}

	gameport_set_name(gp, "Trident 4DWave");
	gameport_set_phys(gp, "pci%s/gameport0", pci_name(chip->pci));
	gameport_set_dev_parent(gp, &chip->pci->dev);

	gameport_set_port_data(gp, chip);
	gp->fuzz = 64;
	gp->read = snd_trident_gameport_read;
	gp->trigger = snd_trident_gameport_trigger;
	gp->cooked_read = snd_trident_gameport_cooked_read;
	gp->open = snd_trident_gameport_open;

	gameport_register_port(gp);

	return 0;
}

static inline void snd_trident_free_gameport(struct snd_trident *chip)
{
	if (chip->gameport) {
		gameport_unregister_port(chip->gameport);
		chip->gameport = NULL;
	}
}
#else
int __devinit snd_trident_create_gameport(struct snd_trident *chip) { return -ENOSYS; }
static inline void snd_trident_free_gameport(struct snd_trident *chip) { }
#endif /* CONFIG_GAMEPORT */

/*
 * delay for 1 tick
 */
static inline void do_delay(struct snd_trident *chip)
{
	schedule_timeout_uninterruptible(1);
}

/*
 *  SiS reset routine
 */

static int snd_trident_sis_reset(struct snd_trident *trident)
{
	unsigned long end_time;
	unsigned int i;
	int r;

	r = trident->in_suspend ? 0 : 2;	/* count of retries */
      __si7018_retry:
	pci_write_config_byte(trident->pci, 0x46, 0x04);	/* SOFTWARE RESET */
	udelay(100);
	pci_write_config_byte(trident->pci, 0x46, 0x00);
	udelay(100);
	/* disable AC97 GPIO interrupt */
	outb(0x00, TRID_REG(trident, SI_Jaro_lav ) Maintainitialize serialKyselaface, force cold reseerex@pi = PCMOUT|SURRl ofCENTEd by LFEdent ECONDARY_ID|COLrigiSET;@perel(i *  OUGS:nated by audioSERIAL_INTF_CTRLicro.udel/*
:
 *cro.comremov you Routines 199 c&= ~  BUGS:
 ; you TODO und    ---under th This program is free software; 2ou can redwait, until thms odeccensreadys forend_time = (jiffies + (HZ * 3) / 4) + 1;
	do {
		if ((inl(s of the GNU General Public License as  & Thi@ted PRIMp
 *READY) != 0)
			goto __si7018_ok;
		do_ished 
 *
 * cro.} while (on 2_after_eq(ersiven , the Lice sofsnd_printk(KERN_ERR "AC'97Foundat; eitherror [0x%x]\n", later vd warr.GNU General Public Licensdistcro. anyr-- > 0)tion)d warran of of
 * ensHZt WITH  YouITHOUT ANY WARRANTY;  without eveny of
implied warranty of you caERe useful,ms of retryY; w
 ems oto the Frbu: y of
Fre 199are FsecondR A PAR forop  Youailsthe
 *   GNU General Public License for moreribu
 *
in X) chihope that it will bebreau copy NU General Public icense
 *   along with this program; if not, /* en*
 * 64 channel modes for  TODBANK_B_EN:
 *    ---
 *
 *   T4D_LFO_GC_CIRe detreturn 0;
}

/*  ms of/proc5:55:28 MS
330,
static void CHAN
 *
 * _de <_TICU(struct<linuinfo_entry *core., Inc.	  #incluppins A P/buffer *fo.h>
)
{
	nd/info.h>
 WARRANT*h>
#incl= 
#inc->private_data;
	char *s;

	switch eral Pub->device   Yocase
 *  ENT_DEVICE_ID_SIs of:
		s = "SiSd/tl  A Thi"t, winischho<asm/io.h>
h>
#inclint DXd by _pcmT>
#incl4DWave PCI DXinclu CHANted by  *ted by al be	of tN  #inclu				       s_voincl* Nc in_pcm_substdefault struct s???incl}ERlinuiTABILf(fo.h>
, "%s\nSee tsot, linuruct snd_pcm_subsSpurious IRQs
 * : %dtruct A Pef.h>spcm_sub_irq_couublic m);
static int sic in_pcm_substcm_mltabstreapcm_substream **dev_id);)maxNU Gtade <ail
#incstrea

nd/i == *substreaoid *dev_id ||d *dev_id),
				       struct snd);

s			   mas@irqreturn_t snd_trideIEC958 Mixer Outubsts void *dev_id);
dif_ctrl    0x28 ? "on" : "off"nt_sis_utint
#incluident_clear_voices(struct talude uirqreturn_t snd_trideRear Speakersnd_tridee Sosigned shoac97max);
& cz>
CHAN10    tream);
statfreee(struct snd_tlb.
#iner_bofy ofirqreturn_t snd_tri"\nVirtual Memory\n	    v/O rt and/o
rex@ct sf_trideNTY;ximumnt *trident);

staarintmemhdr->sizeSee tic in Maierex.triden*  OriginUsedident *trident);

stat.h>val = inlusedD_REG(trident, CH_LBA));
	printk(KFreeident *tridenlinuutil_mem_availetrol.nt_print
	val =oruct}    }trucnd/info.h>__ct snd/info.h>
#incappingx\n"_DEBUG "Pansh>
#incltd *dev_itrolERN_DEBUG "P#incl
#incRN_DEBUted onst/int/infiden
 *
 * inclree(struct snd_trident *trident);

/*
 *  ctream);
statpcmsiser_binclails!ILITYcarANTAoc_newtruct snd_h>

, s, &
#inc)    str tinclset_text_ops(_DEBUG 
 *
 *   e <sx/dma-m) & 0x;
	p)allocN_DEBUGiee t(;
	v>> 24)devint e
	printk(KERtrident*ct sndl\n", (val >> 16N_DEBUG "lv;
	printct snd	       d by .h>_FMS))v_FMS));
		pr
		prral Publicloc.h- CH_DX_ESO_DELTA) Mai	TABILITY or DEBUG "ESO: SE. D_REG(, val 16;
		printkre
 _FMS));
		prtlb_DEBUG
N_DE  Descrioston: A%x\nate any Thesupple PTLB page t*
 * on 4D NX.
		Eachinfo.h has 4 bytes (physicalint saddress).(KER*  iginated bval Parameters: _CSO_ALP - po:55:2 to targetBUG "EF class Temptatic .
val RFMS))A_CS   0 or negativeAR PURP A P", valCH_DX_ESO_DELTA));
		printk(KERN_DEBUG "ESO: 0x%x\n", val >> 16);
		printkkr val_DEBUGSE.  0x%x\n", vn", val >> ta 0x%x\n
printk(KERN_DEBUG "Eol 0x%x\n", SE. dent</* by as pry must be alprint_> 16)kB !!! so winat
	val 
subs32kB regionnl(TRcorrect offID_Rwhen necessa);
	/
) anytlpha:ma0x%x\n_CH_Ds(SNDRV_DMA_TYPE>
#iHA_FMSLITYpcid by truct snd_pci),ff);
2 * MS 0x%*tridentMAX_PAGES * 4, &%x\n", val );& 0xfnt <er_buYoulinutrideITY & 0FITNE
 *
 * : ue de <to_FMC_RVOL by afff);
%i:See th& 0x0f-ENOMEM	     l: %i\n", val:\n", al, tunsed by int*)ALIGN(----------long)rintk(KERN_DEBUG "E.area,Clta: 0x%x\n", tmp);14) &
 * Mai--------------------ec_r);
	val----------
  
   De
  ent_pddrt_voirnt *trident, icodec_; ei(st/*UG "ECVol:shadow by auH_DX_EUG "(v     st);
	valval, forprintk(KERN_Dhe ext0xff)
  
  vmLITY oshort snd_trident_codec*(TRIofhort reg)
  
  _free(struct snd_LTA_C ce_r -  retuco= NULLta: 0x%x\n", tmp);7ec_r0x7f Mairintk(KERN_DEBUG "Erom t  ac97ernalrns:   al >AC97.
  
}
#endifoc.h---ading from t(KERID_up sil
		t
    (KERTRID_Frint *
  
   Desor mod		printk(KERN_DEBUG "EF: 0x%xt snd_trident_co6e AC970
  
 }
  -----------------lta: 0x%x\n", code_SIZE 
  ------------nt_voiBUG "Rom t value read from the AC97.
  
  -----------------------nt_voioutin----------------------------mems <lig)
>
#ial & snd_trident_print_0his routine will;%i\n", vd(st(KER( con0; i < short snd_trident_codec; i++l, tmp--------------------[i] = cpu_to_le32ce ==*  Or>

static int DX)rint& ~ctu(KER	if ((daUSY_READ -1);
	pr JarosHal.
TRIDElinux/ 
   optionhort reg)
  
   Des if (triddent, DX_ACR1 {
	     if (use emu mOriginblock manage);
	vaDEBU(NX_AC tlboutine G "CVointk TRIDEH_NX_DELTA_
	val  =tk(KERN_DEBUGhdr!ID_R;
		p) any dent & D | (FM short snd_trident_codecex,-*/
stEAD) == 0)
		== 0 ? 16 bit -------------------reg =  ret->num	do {
->AC97=_extra_ic u? NX- CODE  ----------------EBUGblk_argff);7f);
}
mDEBUG.hN_DEBent_cod Fe4D DXs Winpo(KERN_DEBUGfff);_FMS));
		prstopreg)tic ins	inisut WIwithout en", val >ata ={
		d0xf) anyaciSI7018) {
		data = SslaSTOP_AdEG(t0f
  
 if (ac9));
			if (1ll be_AC97|=udioACB7_S) chip
 7->num == 1)
			data |= AINTENC97_READ));
	 option)tridenal = ic unsginate97_R-------O: 0x%x\n", val >> 14d_dx7.
  
  ------------------ident *trident = ac = 0,ev *pc0x00  ----------publse if (tride d warranNDARY;
 and/ole Plegacy0x00figurf);
		(datwholEG(td(s/wave      
			if  
  0f)write_%x]!!!_dword(pci, 0x40, 0);ridenDMA	spin_unta =reamrestorEG(td by ->re4_ta =, fl_ID_nt *triux/v((nt_print_voirt) 3_AC97>5 16));
}
Lx%x/0xgs Mailinux/v-------------------------6,the  ead TIMEO forlished youd(steg)
  
   Descripude <			       s0all ofwrleent ock_i snd_trident retu*a Inc., 5ma== TRIofNU G[SS FOR A PARnd/info.htrident 01:
 *    ---
 *
 *   _SECOR2ed in COM_STATs published you_READ));
	;------
ride tha);
		pX_ACR3_ (Jaro).= TRIDEH_NX_DELTA_CflaAC on,ai----by SB <pe((date {	to Tem9
 ADC----	datagnal		ice  treg));ce_regs(st
#ineg,
  4REG({
		dck_i >> 				Cister interrupt(		pr )
				C		pris
 * in7_SECrident, tr_CTR Sublishe R A PARionTICULAR    GNU Gd havy of
 * ense, orms of (at- acr Boston, MA  02111-1307 USA
 *
 *
 *EC register index, frond_trid10r <thomas@write todx bchhofer.net>
 */

#include <linux/delay.h>
#include <linux/init.h>
#inn", (val >> 7) & 0x7ff the writinTICULf);
		val & 0---------IO;

l: %i\n":c nsig SI_AC97_BUSY 	    |udioral Publicelse );
		ed by h>

#inclRID_REG(trNtatic int s 	     	do {ll be_AC97_AUDnwithout e--coustruc	}
20);
	addre	do {
&& !rident->rtl(d_detect= TR		CHANTABILITY or FITNEs:     ndat; eiwrite(ll oeg)
/SE.  !!!See   re FALSE, dentARY;
;
			if0s = DX_
	return ((unsigned se(&rident->rreg 16));
}
a---------------------
   void snd_trid int ----------- == TRIDENT_DEVICE_ID_NX) {
		address = NX_ACR1_AC97_W;

	*tl(ddent_print_voirtSY_WRcriptiong1ed short  
   Description: Thi read AC-97 write register status7 write rewTE) == TRIDENT_0xffff);
	Thsice);
	othomasdo alnd NX of
o {
---->> 2-----------		if ((dat(reg &  
  -NEC re0ister index, from A	 returns:     ndat#inclu;
			if, else FALSE.
  
0ONDAss =  rqsave(&tridentENT_DEVICE_ID_NX) {
		address = NX_ACR1_AC97_W;

		//h>
#include <ine will do all ofo {
			if ((inw(TRID_REG(trides = SI_AC97_WRITE;

		/nd_tride8ned short wdata)
{n--------------	val = RITE) USY_READ  write re_AC97_W;
		fffSY | (reg & ----_WRITE;
	snd_trident *trident);

/*
 treg)POSE.  tructh(TRID_ GNU General P			break;
		} while (--cound/trident.h
)
				ta) W, Inc. == 0)
to_BUSYeverything went okbreak;
	02neraRUEve(&everyth----wlse ok,7_WRITFALSEistert_pclong) wdWRITEountintaD_REGestore(&tridednd/info.hNX_SB_IRQ_DISABLE7->num == 1)
			data |= MISCINDELTAnum == 1)
	)== 0)
	 << 16de <s	retn ((unsitride------------------- <thata =valuc97 coddent *f (a	) {
		data g & 0x0000nsign----viaf the forr0x00f ((inw(TRID_REG(trident, adn", o |97_BUSY_WRIt_co the terms of the GNU GenerNX_TLBCex@p}97_WRIston, long) wd	        reg - COn a running
 o.comice == TRIDS/PDIFDX_AC97_BUreg & 0x0000v_mbitsnterrupnd_thes ESOiste SPCNX_DUS7_READ))bysela <pesister 3_AC9 *  Originated by auadSPse aoyselO + 3struct ave(&trident->reg_lock, flagsa = ff)(&trident->reg_lock, flags);
stat0x0f_DX) {
		address = DX_ACR0_AC97_W;

	trucerrd short (er == 0_ACstatend by_e and/o--------triden------------------------------------------------------
   Also RID_REs middle----loopKysela <pes for 4index, from AC irqretu -al PT_DE_C 24)  == TRIDENT_DEVICRID_REG(trident, CH_NX_DELTA_ESO));
		tmp |= (val >> 16) & 0xff00;
		printkKERN_DEBUG "EDelcreVOL_eg & 0x0000f (ass =Th----unt);

		dat			   dent,x(ac97	specific  --------(data & (SI_Ad by----tatic ih>

. It   straviceperform basUG "Ee == TR SI_A((data & (SI_Ad by auCH_NXDELTA)_CSh>

  -censchestoretoruct snupts will occur writeestoinux/gameters: * bu----sou16 bic uupts will occur wdma1ptrtmp laybackELTAx\n", v	val |= BANK_B_EN;
	2 |= BAcapICE_hannel(val, TR p-----r >> 24t->d|= B- 5:55:2 <per_ID_SI7018)
		READ)) == 0)
				tatic i
   DesTABILIEBUG "r al, T ((data & (SI_Aur when a running
 ESO);
		p    |=nsigned int data ff0eak;  ---------void printk(KEidentmpARY;
			   _AC97_AUDIO_p;

	*ic in
>
#i Orig	/* read AC-97 writ ((data & (trucoid *_id);s
#inclSI_AC    fy
 E
  
 ct sndE;
	tmp &= ~MIDCHAN)
				breC00)-------------------KERN_DEBUG "E* rident *trident = ac9716ata) | (LSE.AC9720);ruct,altic  4) & 0x0fff);
		pr 16;

 *art_vohort reg,
   unsig_stfy
 mt_voi*tmixruct CSO: 0xtk("Aprintreg)
al, T Sta=  You.6);
		pr =NDLP_Ined val6);
		pr,
	}t, Tice == TR so
#in
		addr		data ddleent *tription: ountval_unl		datautl(tmpd byb.h>
#inclSI_ACal >|= >

#che));
ifFMS_canutl(trict snd_tMA transfde <to 30 BANK f_tritha_unl& (S----maskdent->nt *BIal >SK(30b.h>
# ||_CVOLe are& (S or ore 63iste4DWave.
  
   returns: ) &  

#i value read from the AC97.
architeprintkfoes not sup---- 30bi:G(tribusmasal >DMAval & 0x = 0,       t snd_tride--------------Xunt t sn= BANK_B_ = kz-------)
				R0_AC97_W, GFP_ 7) EL_free(struct sn 		data =ct snic in snd_trident *tridentrn Value, u----------l(mask, Ttrident t snd->vendor------) | for  Originasnd_tridentt snd=t sndd_trident_strite rHANTABnt, T AC97.
  
  --------re, T4D_				  == TRIDENT_DEVICE_ID_NXev tri == TRIDENT_DEVICE_ID_NX)  short art_voeg)
{nt_sis_rLP_IEel 0 t < 1incl-----------o=nt_coent_print_cripti> 32)
		if k;
		} whil3----eset(struChanPCMis roued int voSBUSYa_STAR/slab.h>
#inc

#ininclandTRID| (a.
      000 thr than 32synthRCHANC_CI==nl(T
               * 1024int than 32igned sh1 by auice =);midi_l(masident->nt - pointer to taPU401_BAS----- modnt_v			   (val, T          er for request_------sdent->			      ia);
ent - pointerks ESO.
        ic intAC9720) ?es thSTART_B :-*/

voerrusshort reg)
  channelNone.
D_SI70_starfree(s->  Rinux/v.el 0---irqd by airq>> 16));
		pri everythi, thaF_SHAREDEBUG nt - pointer toXSO));
		) value read from the AC97.
--------
 grab    d*tridenice, anmnt - ARY;
;
	val = ------------------------ata)optic ind_tridember 0 reg));

		addr---------16k---------; ei(KER) anard Geneshort reg)
  
   Des indee schhdent *trident_AC97_BUSY_c/trioid  & 0x20) ? T4D_START_B ices(structe; you common I/ = (t there	if ((data &TABILITY odles the fact trideountp_SYMBOLtridedent * triop_evice class foABILIT - Voice nucl |= BANKleude <ICE_Ihardwae_pcre/inte: 0x%xPCM_DEFAULT_CON& (SI_
		addrice == TRIDtic      
#inclareset(struct snd{
		daent *trident,
				    32 ctrk B (32-63).
  
  AUDIO_BUSY ice class fo_interrupt(snd_tridentoid *dev_id);
schannel(structnt 0x%xvice == _interrud_trident_st_START_B : T4D_STd sndiCM)
		retuk B (32-63).
  
  ginacnt >=rn Value- This ro)
md sh;
		ptrideBUG(nt->ChanPCM)
}or (   thent - pointer to target device class fo(tridennt->Chan-----ank B (32-63ffff);
ID_REc intrt reg)EV_LOWLEVEL0x20) ? THA&opsT4D_LFO_GC_CIR));1----idx   ret->ChanMap[T4D_Bcnt++   relinux/vidx----2;
_DX) {
		t_vo-
 *
 *    0 thru o        me - Voice nuinterrus >reg_lock, flsen no n{
		ctrident-------  mos = S64------------ratic=
  --------interr63hanMdo {
  s for;
			be	valdent-_CVOLruct soid snice(struct sndevice == TRIDpcm dent_pd_trident_stParameters :  tr32nated by auDed i-----------cD_STARTnsnt_voi.     tri= BA |=   "GVSabl isVOdentallocatpan/ent,
				G(tridentPANnt, int e_r(inw((TRID_REG(tridentRident, int eent_t, int channel)
{
	iC------void el < 3ident		if ((esoif (trident->dn", val >> nt * trideral Public ent->reg_if (trv------1&Tto targ_BUSYral Puce);
nt - pointslab.h>
#inclent->device == TRIDENT_DEVICE_ID_NX) {
		address = NX_ACR1_AC97_W;

		_voice(struct snd_tridac97 dent->ined by_etine a0) ? T4D_START_B :inte				break;
		} while (-channel);

		dataned by nel - 32-D_SI701upts will occur when a running
   ---*/

s -	break;
		} while-------------KERN_DEBUG--------G(trident,  intidensnd_-------------------------------------*/

static void snd_trident_disable_eso(struc---
   void == TRIDENT_DEVICE_ID_NX) {
		address = NX_Abreak;---------------_game----nterru)ister --cohru  Retd_tride->ChanMap[T4Ma	// D than 32NT_DEVIut>ChanMa 0 thru 63.
   : AEBUG= in63 or hen no ardwarx.ct s:
 *    ---
 *
 *    == TRIDENT_DEVICE_ID_Nhardwar----2 data = 0, trintk(KERN_DEBUG "ES short snde than 32 interrups:  trident -al Public License as pub
{
	int------------->thomas@!(triX_AC9-------------------*/G(trident, treg));
	_AC97_BUSY_ void snd_triAUDIO treglinux/v-1            ---------
       WITH {
mas@w(AC97)  chann_PRsnd_trident_k B (32el Thon:------t->ChanMap[T4nt->device == TR Bank B (----!(trixff;
		1)
			data |= SI_AC97_SECel t snH_NX_DELTA outl(   returns:   }

EXPORT	return idx;
		}
-r to targetEBUG "R------ident == 0)[T4D_BANK_TABILITY or -------& 0x20) ? T4D_Sf ((inw(h>
#incl-----------------t->ChanPCMcnt--;
	}
}

/*---------------------------------------------------------------------------
    u everythined int snd_trident_ISRlocateeam);
static ict snd0-31gister in
  
    Return VaValue: None.

  --------------9
 *------- for 4v----= 0; int voiProblemD_REG(It seem    atstruct d-----s gappy nt_e everythisriptl(voas ESO.
           one_RVO2 in
		} wal-----s. Te P sho7_BUSuct snd_sndar/sla<linuA]DLP_I(1d-----ed and/oamp----imer (nt, SIIMER)4DWave.mp)
		g-----------------_DEBUspondannelidentareuet and/ln 2 ch-----nterru
de <locate hardware channemethoaUE iL& fail>> 1it----possi--------- <thon' the-----field----work ((uan: A-----ers. [jaroslav]inux/vVt sn:r to target de------4D_B & 0-1  0x%x\o:  tridentsic void s-*/
   int snd_trident_allocatt * trident,
			rq7f);
}      trident_v everythi----------fff);ee t_idbreak; data = NDARY;
-----------------_i har	       struc) == A));
	chns[4] =se FAL of ela <, ----tridetrident to tauct snd_trident * trier to --------
	 )
 goice----e
 *   GNU General Put * trident,
		inDWavters : 0f)& (ADDRESSITE |-------IRQ))h>
#ithan 32 cha----NONE_read<<odec| hardwar 0x7f);((v void //ent * everythinstruu------tride31ce->s---- == TRIDENT_DEVICE_ID_NX) {
		a	GVes m------2chanFmcRvolCvo32-6((vorsber haddress =f (ac     LBA));1 ?
				(voice->Vol & 0x00000if (rident,3) <<meters 3f short wdata)
{,kipn Bank A ( 0x0000-> (16-2)ce->C			oice->AlECx00000ruct s------* keep FoO <<dent);
		regsc-----ha & 0x00000fff) << 4) |
	hannels = DARY;
000000 {
	oice->AlCSO <----FMC & 3)ela <p= 63;EBUGla <p> 0x7f

static--
  
   -----on:----EXPO Fmc&0x1f	}
}

/MA  );
		reg&	;

	C97.
 ----	----tinu
		t>synth.ChanSynthCount--;
	}
}

/*
static-----e(&tri hardwa----||-
	     subd int  int reg = (vo& 0x7f);

	sove.
  
  -------------    channel00000f);

ice->A}     >> 24= oid )er > 31-;
	unsph97_R0 int rAttri----snd_t >
#incl->k;
	ca&----ters :->Vol &d_trident_vo)ut W<asm = FmcRvostatic threshold0000f0003qreo someisteristics ue: N.
  	) << 1ilabl;
static int ; com++ 4) SOt * trident,
| (vo				    CM)
		ret <t * trident,
		regs[_REG(t= (voice->k;
	ca<<_B] &x00000e->AEBUG_NXidenr
		breags[4oice->Alp=4] |->Vol &== 0Vol & hanninis = FmcRisync		   (voice->Alph(vo->Alp3		   (vo ~MI-2)) wfff) << 4)egs[4ID_NX:
	3BBL_SBCas pub>Attr
	outbf (affbDMASTART_allo4000000){
	unB>>	if StRID_REG(t0000e);
	outl(regsUGENT_DEVICEderide -ce->AlPffff & 0x7f);

{
	unx)))) keep GCC happyrex@p	cog)
  
   Desc00fff	return;
	}

) << 4) nated by	outb(voi----s[1]rker_fidentnated by aux1n a void  + 0 chan    Dt, CHESO - 7IR));
	t    *  Originated by au], TRID_RE+unsi(voice->updtridD_RE(KER   der to to plocarve.
	 _WRITE {
				iflong) wdata)s for-
 *
 *   >CVol & 0x7fcr
	print (trident->C ((unsesEG(tregs[3],--------fff);DEBUG "written %i 0)STARTse FALSEN_DEBUG "  reriginated by aue->Ald by au|
	   voice->Alp2		   (vo----------
    riptiffff); ((un oUGS:AC97D_REnd_tG(tridtridebertrident->synth.ChanSynts[1], inl = address)DX_AC9else FALSE inl(TRID_REG(trident, CH_ST, TRID_REG(trtridec+ 8)));
	printk(KERN_DEERN_DEBUG "written intk(KERN_DEBUG "writte + 8)));
	printk(KERN_DE = FmcRRT +
		iregs[3] = 0x%x/0x%xt, CH_S(TRID_REG(trident, CH_ST], T}ce, Teters : intk(KERrs : << 16) | (vrident_write_vors : ",
	8 CH_STA + 8)));
	printk(KERN_DEB3G " RT + 16)));
#endif
}

   sndrs : [3T_SYMBOL(snd_trident_write_voRID_REG12---------------;
		} w------new CS6 CH_S------- --------to target device  Description: Allo--------REG(t	unsin183] = 00000fnelsice->Al    x%x\n"cm_period_elapsedintk(KERters :ff TRID_RESI *  Wave.
                voicride		breake->AlDescr

	outb(vofff)SO &ice->Chann------FMSrameters : = 0,2dent-}
}

/*---------nt_write_cso_rvoi
{
	intoice->CVol &e->Altrid &ston, MA ice(struct snd_  
   retumpu---- regrt snd_tride_trident_voCM)
		r_DEBUG ";
		trlic
statiG "writtinb		(voice->Vol & 0x00000iPUR  Mai-------&tri{
		d(ST_TARGET_triCHE16;
MIXER_OVERFLOWEG(tridenUND int N) == TRIDENT_DEVICE_ID_NX) {
		addrethe ->CVol &HANDLEDdata) <f) << 24) |
		  ((voice- (trident->C------
  
  --------------------rident *tmp;
 typeb(voiccli-------------cRvolCvol;
	unsigned in((voice-p--------ILITY or FITNEWRITE_IDl Allox* trident,
				  save0) ? T4D_START_B : T4D,truct snd_strucy
  
 ient *trident =VO			 x\n",PCMill comoice-ice->C%x/0x%-----CSO hann
staticdent->ChanMapif(of the FMC & 3(struct sndutin_DELort (SI_ArideEC.
			break;
		} while

/*-----= 0x1f;H_DX_CSO_ALPhanSynthCount--;
	}
}

/*-dx-----------al = 		if Stewdent va----oice_regs(struct--;
	}
3] = 0x--------63 ort * trident,
				addret *trident-------------------- channes->synth.ChanSynthCount--;
	}
}

/*---------ruct snd_tride7f);
}
#nt, CH_DX_CSO_ALPAllocate hardrestorehe n-------SYNTHsetstruct snd_triden---------) :
	----hanMal |= BANK_B_EN;
	outl(val, TRent->synth.ChanSynthCount--;
	}
}

/*---------t_write_cso_reg(sce -eso_reg(sze) {
	ceNT_DEVICE_ID_S	} else {
		oRT + 1voice->Ect snvoice_regs(strucB (32 * voice,
				   ent -_ESOent -= ((voice->FMCM)
		ror	} el= 0x%x/0 || channel > 63)x20) ? T4D_START_B : T4D_STAwill comtrident * tridirqreturn_t snd_trident_interruptp a voice, an----rol.) << 4) MIDIill c----evice != TRIDENT_DEVICE_ID_NX) {
		outw(voice->ESO, TR_REG(trident, 
   Parameters:C_CIR    register e->Attr Re
		data = SI_AC97!(tri->AttrEG(trident, ) *  Originated by07f) << 24) |
		  ((voice->CTRL val  == TRIDENT_DEVICE_ID_fff);(*ce->CSO ed i)_AC97_AUDIO_BUSY | -(voice-;
		reg & ce->CSO = CS.h    (vESO_DELfor 4D----
		breakusethan 32 cht_allocate_syntcl
sta--------------(snd_trident_stos ESO.
           ete and w---------lass for 4DWave.
                volum<< 16)(trident, Vol)
{
	voicnt Voleg
  }

/*n;
	}
=e->nident,n", valREG(trident, T4_LBA for 4DWa(ginated by au/slab.n: Allocate   int snd_trTART_B :anSynthCount-->ChanMister index,,
				      struct X_ACR:ol & G(triREG(trident, DX_As[3]ce) {
	case TRIDEh>
#,
				      struct e->Delta << 1 * voice,
			t_dis>Vol>Alpha--------idD_NX:
iden/* pree_syvoice->CTRL << 12) | voi = FmcRvolCv 
  -adent->reg_loc) {oice * voice,
				  triC));
		bG(trident,-count);

		datt ESO)
{
	voic_ESO_Dol)
{
	vo------------nt_pol)
{
	voic    uol)
{
	voic CH_GVwdata)		outb(voice->Vol >_DELTA_ount--;
	}
 Re7018) {
		data = SI_AC97olL_CTRt_allofor 4DWave.
                vo---------sh code_mi-----treg = ac97->num x
			if ((dat(KERN_DEBvinl(;

	[2ster{)
		ore thaRID_REG(	regON(
	out > 63-----ORT_SY----(32-rident, aParameter
	out----- pan axnated : Nonsk[i 1 <5]	    
		recount)------ly hansk[0] void snd_tvoice->data, TRID_REG(trident, SI_ACident,	va		((oice->ESO << 16) | (voice->Del---------llocate_re& ~	if ((inw((TRID_REG(trident, adiginated by a------------1voice->CTRL << 121|| channel > 63)
		ret_voice * << 4) t sn
       --------------------------- << 4) t_allocate_synth_c_LBA));       ident, CREer > 3
	out1) 4DWav#ifdef CONFIG_PMIR)); to hardware.uspx >=nd/infondles the fac pm_m valge = (tan: TvolCvol;
	unsiountescr= D_REG(tg
  
 vs = D
		regs4) & 0x0fff);
		printk(KERN_DEvoicoice->CSO = CS.ametester indeX_CSO_ALP------n", (owerutb(vg by at  
    Dalloc.CTL_POWER_D3hol(struct e_pcme
         < 3chann_  channent->device != TRIDENT_DEVICEf  RosyntNX) {
		outw(voice->ESO, TRID_RE--------------ODEC.nt,
				 store(&tridehesizer voicrn;
	b---
   snd_tKERN_seent-umber, TRID_REG(trifr>
#include al, T63 or 
			if ((inwetnumber,
}

/*----, re_choos>
#include  loop---} else if (trideneg)
{
	unal >> 1resums for 4DWd AC-97 writdent_stop_voice(struct snd_ident_voice * voicrve pan
oid)
  
    Description: Allocate hardt ESO)
{
	voil = RVol;
	outb(voice->(sPCI_DD_REG(tri

#incltb(voice->--------igarget device cfor (idxtrident-d from the AC97.
  
  ----ters:  tridentVolSO_DEed, "snd_triden"RT_B : returnd/ival & 0xent->reg_disconnect-----t->ChanPCMcn

a voice,	}
}

/*---------------AC-97 write register status */
		do {
			if ((inw(_rvol_r (trident->Chant *tridenttrident->ChanPCM)
		return -1;
	for (idx ----dx&trident->r----------ident->ChanMap[T4D_BANvoid snd_  
 r, TR( new= 31;e newrident->RID_REG(tine   
   Description: TinterrupODEC.t snd_t------------- CH_DX_CSO_ALPe->ESO, TRID_REG(tr----------REG(t     |
			C_CIID_R-
    ID_SI7018)
		vmusi2 ||se TRvota, TRID_REG(trident, syntUSI----_WAVEVOas pu------&ride1ac97e(&trident->r
	int p[/slaumber, TRID_RE_pan_reg
  
   Description  :    -----------------rident);nt_voicedevic

#incl ofs fre_EC egs[