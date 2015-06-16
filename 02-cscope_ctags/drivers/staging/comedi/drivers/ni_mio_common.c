/*
    comedi/drivers/ni_mio_common.c
    Hardware driver for DAQ-STC based boards

    COMEDI - Linux Control and Measurement Device Interface
    Copyright (C) 1997-2001 David A. Schleef <ds@schleef.org>
    Copyright (C) 2002-2006 Frank Mori Hess <fmhess@users.sourceforge.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/*
	This file is meant to be included by another file, e.g.,
	ni_atmio.c or ni_pcimio.c.

	Interrupt support originally added by Truxton Fulton
	<trux@truxton.com>

	References (from ftp://ftp.natinst.com/support/manuals):

	   340747b.pdf  AT-MIO E series Register Level Programmer Manual
	   341079b.pdf  PCI E Series RLPM
	   340934b.pdf  DAQ-STC reference manual
	67xx and 611x registers (from http://www.ni.com/pdf/daq/us)
	release_ni611x.pdf
	release_ni67xx.pdf
	Other possibly relevant info:

	   320517c.pdf  User manual (obsolete)
	   320517f.pdf  User manual (new)
	   320889a.pdf  delete
	   320906c.pdf  maximum signal ratings
	   321066a.pdf  about 16x
	   321791a.pdf  discontinuation of at-mio-16e-10 rev. c
	   321808a.pdf  about at-mio-16e-10 rev P
	   321837a.pdf  discontinuation of at-mio-16de-10 rev d
	   321838a.pdf  about at-mio-16de-10 rev N

	ISSUES:

	 - the interrupt routine needs to be cleaned up

	2006-02-07: S-Series PCI-6143: Support has been added but is not
		fully tested as yet. Terry Barnaby, BEAM Ltd.
*/

/* #define DEBUG_INTERRUPT */
/* #define DEBUG_STATUS_A */
/* #define DEBUG_STATUS_B */

#include <linux/interrupt.h>
#include <linux/sched.h>
#include "8255.h"
#include "mite.h"
#include "comedi_fc.h"

#ifndef MDPRINTK
#define MDPRINTK(format, args...)
#endif

/* A timeout count */
#define NI_TIMEOUT 1000
static const unsigned old_RTSI_clock_channel = 7;

/* Note: this table must match the ai_gain_* definitions */
static const short ni_gainlkup[][16] = {
	[ai_gain_16] = {0, 1, 2, 3, 4, 5, 6, 7,
			0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x106, 0x107},
	[ai_gain_8] = {1, 2, 4, 7, 0x101, 0x102, 0x104, 0x107},
	[ai_gain_14] = {1, 2, 3, 4, 5, 6, 7,
			0x101, 0x102, 0x103, 0x104, 0x105, 0x106, 0x107},
	[ai_gain_4] = {0, 1, 4, 7},
	[ai_gain_611x] = {0x00a, 0x00b, 0x001, 0x002,
			  0x003, 0x004, 0x005, 0x006},
	[ai_gain_622x] = {0, 1, 4, 5},
	[ai_gain_628x] = {1, 2, 3, 4, 5, 6, 7},
	[ai_gain_6143] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};

static const struct comedi_lrange range_ni_E_ai = { 16, {
							 RANGE(-10, 10),
							 RANGE(-5, 5),
							 RANGE(-2.5, 2.5),
							 RANGE(-1, 1),
							 RANGE(-0.5, 0.5),
							 RANGE(-0.25, 0.25),
							 RANGE(-0.1, 0.1),
							 RANGE(-0.05, 0.05),
							 RANGE(0, 20),
							 RANGE(0, 10),
							 RANGE(0, 5),
							 RANGE(0, 2),
							 RANGE(0, 1),
							 RANGE(0, 0.5),
							 RANGE(0, 0.2),
							 RANGE(0, 0.1),
							 }
};

static const struct comedi_lrange range_ni_E_ai_limited = { 8, {
								RANGE(-10, 10),
								RANGE(-5, 5),
								RANGE(-1, 1),
								RANGE(-0.1,
								      0.1),
								RANGE(0, 10),
								RANGE(0, 5),
								RANGE(0, 1),
								RANGE(0, 0.1),
								}
};

static const struct comedi_lrange range_ni_E_ai_limited14 = { 14, {
								   RANGE(-10,
									 10),
								   RANGE(-5, 5),
								   RANGE(-2, 2),
								   RANGE(-1, 1),
								   RANGE(-0.5,
									 0.5),
								   RANGE(-0.2,
									 0.2),
								   RANGE(-0.1,
									 0.1),
								   RANGE(0, 10),
								   RANGE(0, 5),
								   RANGE(0, 2),
								   RANGE(0, 1),
								   RANGE(0,
									 0.5),
								   RANGE(0,
									 0.2),
								   RANGE(0,
									 0.1),
								   }
};

static const struct comedi_lrange range_ni_E_ai_bipolar4 = { 4, {
								 RANGE(-10, 10),
								 RANGE(-5, 5),
								 RANGE(-0.5,
								       0.5),
								 RANGE(-0.05,
								       0.05),
								 }
};

static const struct comedi_lrange range_ni_E_ai_611x = { 8, {
							     RANGE(-50, 50),
							     RANGE(-20, 20),
							     RANGE(-10, 10),
							     RANGE(-5, 5),
							     RANGE(-2, 2),
							     RANGE(-1, 1),
							     RANGE(-0.5, 0.5),
							     RANGE(-0.2, 0.2),
							     }
};

static const struct comedi_lrange range_ni_M_ai_622x = { 4, {
							     RANGE(-10, 10),
							     RANGE(-5, 5),
							     RANGE(-1, 1),
							     RANGE(-0.2, 0.2),
							     }
};

static const struct comedi_lrange range_ni_M_ai_628x = { 7, {
							     RANGE(-10, 10),
							     RANGE(-5, 5),
							     RANGE(-2, 2),
							     RANGE(-1, 1),
							     RANGE(-0.5, 0.5),
							     RANGE(-0.2, 0.2),
							     RANGE(-0.1, 0.1),
							     }
};

static const struct comedi_lrange range_ni_S_ai_6143 = { 1, {
							     RANGE(-5, +5),
							     }
};

static const struct comedi_lrange range_ni_E_ao_ext = { 4, {
							    RANGE(-10, 10),
							    RANGE(0, 10),
							    RANGE_ext(-1, 1),
							    RANGE_ext(0, 1),
							    }
};

static const struct comedi_lrange *const ni_range_lkup[] = {
	[ai_gain_16] = &range_ni_E_ai,
	[ai_gain_8] = &range_ni_E_ai_limited,
	[ai_gain_14] = &range_ni_E_ai_limited14,
	[ai_gain_4] = &range_ni_E_ai_bipolar4,
	[ai_gain_611x] = &range_ni_E_ai_611x,
	[ai_gain_622x] = &range_ni_M_ai_622x,
	[ai_gain_628x] = &range_ni_M_ai_628x,
	[ai_gain_6143] = &range_ni_S_ai_6143
};

static int ni_dio_insn_config(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_insn *insn, unsigned int *data);
static int ni_dio_insn_bits(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data);
static int ni_cdio_cmdtest(struct comedi_device *dev,
			   struct comedi_subdevice *s, struct comedi_cmd *cmd);
static int ni_cdio_cmd(struct comedi_device *dev, struct comedi_subdevice *s);
static int ni_cdio_cancel(struct comedi_device *dev,
			  struct comedi_subdevice *s);
static void handle_cdio_interrupt(struct comedi_device *dev);
static int ni_cdo_inttrig(struct comedi_device *dev, struct comedi_subdevice *s,
			  unsigned int trignum);

static int ni_serial_insn_config(struct comedi_device *dev,
				 struct comedi_subdevice *s,
				 struct comedi_insn *insn, unsigned int *data);
static int ni_serial_hw_readwrite8(struct comedi_device *dev,
				   struct comedi_subdevice *s,
				   unsigned char data_out,
				   unsigned char *data_in);
static int ni_serial_sw_readwrite8(struct comedi_device *dev,
				   struct comedi_subdevice *s,
				   unsigned char data_out,
				   unsigned char *data_in);

static int ni_calib_insn_read(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_insn *insn, unsigned int *data);
static int ni_calib_insn_write(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data);

static int ni_eeprom_insn_read(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data);
static int ni_m_series_eeprom_insn_read(struct comedi_device *dev,
					struct comedi_subdevice *s,
					struct comedi_insn *insn,
					unsigned int *data);

static int ni_pfi_insn_bits(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data);
static int ni_pfi_insn_config(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_insn *insn, unsigned int *data);
static unsigned ni_old_get_pfi_routing(struct comedi_device *dev,
				       unsigned chan);

static void ni_rtsi_init(struct comedi_device *dev);
static int ni_rtsi_insn_bits(struct comedi_device *dev,
			     struct comedi_subdevice *s,
			     struct comedi_insn *insn, unsigned int *data);
static int ni_rtsi_insn_config(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data);

static void caldac_setup(struct comedi_device *dev, struct comedi_subdevice *s);
static int ni_read_eeprom(struct comedi_device *dev, int addr);

#ifdef DEBUG_STATUS_A
static void ni_mio_print_status_a(int status);
#else
#define ni_mio_print_status_a(a)
#endif
#ifdef DEBUG_STATUS_B
static void ni_mio_print_status_b(int status);
#else
#define ni_mio_print_status_b(a)
#endif

static int ni_ai_reset(struct comedi_device *dev, struct comedi_subdevice *s);
#ifndef PCIDMA
static void ni_handle_fifo_half_full(struct comedi_device *dev);
static int ni_ao_fifo_half_empty(struct comedi_device *dev,
				 struct comedi_subdevice *s);
#endif
static void ni_handle_fifo_dregs(struct comedi_device *dev);
static int ni_ai_inttrig(struct comedi_device *dev, struct comedi_subdevice *s,
			 unsigned int trignum);
static void ni_load_channelgain_list(struct comedi_device *dev,
				     unsigned int n_chan, unsigned int *list);
static void shutdown_ai_command(struct comedi_device *dev);

static int ni_ao_inttrig(struct comedi_device *dev, struct comedi_subdevice *s,
			 unsigned int trignum);

static int ni_ao_reset(struct comedi_device *dev, struct comedi_subdevice *s);

static int ni_8255_callback(int dir, int port, int data, unsigned long arg);

static int ni_gpct_insn_write(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_insn *insn, unsigned int *data);
static int ni_gpct_insn_read(struct comedi_device *dev,
			     struct comedi_subdevice *s,
			     struct comedi_insn *insn, unsigned int *data);
static int ni_gpct_insn_config(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data);
static int ni_gpct_cmd(struct comedi_device *dev, struct comedi_subdevice *s);
static int ni_gpct_cmdtest(struct comedi_device *dev,
			   struct comedi_subdevice *s, struct comedi_cmd *cmd);
static int ni_gpct_cancel(struct comedi_device *dev,
			  struct comedi_subdevice *s);
static void handle_gpct_interrupt(struct comedi_device *dev,
				  unsigned short counter_index);

static int init_cs5529(struct comedi_device *dev);
static int cs5529_do_conversion(struct comedi_device *dev,
				unsigned short *data);
static int cs5529_ai_insn_read(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data);
#ifdef NI_CS5529_DEBUG
static unsigned int cs5529_config_read(struct comedi_device *dev,
				       unsigned int reg_select_bits);
#endif
static void cs5529_config_write(struct comedi_device *dev, unsigned int value,
				unsigned int reg_select_bits);

static int ni_m_series_pwm_config(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn, unsigned int *data);
static int ni_6143_pwm_config(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_insn *insn, unsigned int *data);

static int ni_set_master_clock(struct comedi_device *dev, unsigned source,
			       unsigned period_ns);
static void ack_a_interrupt(struct comedi_device *dev, unsigned short a_status);
static void ack_b_interrupt(struct comedi_device *dev, unsigned short b_status);

enum aimodes {
	AIMODE_NONE = 0,
	AIMODE_HALF_FULL = 1,
	AIMODE_SCAN = 2,
	AIMODE_SAMPLE = 3,
};

enum ni_common_subdevices {
	NI_AI_SUBDEV,
	NI_AO_SUBDEV,
	NI_DIO_SUBDEV,
	NI_8255_DIO_SUBDEV,
	NI_UNUSED_SUBDEV,
	NI_CALIBRATION_SUBDEV,
	NI_EEPROM_SUBDEV,
	NI_PFI_DIO_SUBDEV,
	NI_CS5529_CALIBRATION_SUBDEV,
	NI_SERIAL_SUBDEV,
	NI_RTSI_SUBDEV,
	NI_GPCT0_SUBDEV,
	NI_GPCT1_SUBDEV,
	NI_FREQ_OUT_SUBDEV,
	NI_NUM_SUBDEVICES
};
static inline unsigned NI_GPCT_SUBDEV(unsigned counter_index)
{
	switch (counter_index) {
	case 0:
		return NI_GPCT0_SUBDEV;
		break;
	case 1:
		return NI_GPCT1_SUBDEV;
		break;
	default:
		break;
	}
	BUG();
	return NI_GPCT0_SUBDEV;
}

enum timebase_nanoseconds {
	TIMEBASE_1_NS = 50,
	TIMEBASE_2_NS = 10000
};

#define SERIAL_DISABLED		0
#define SERIAL_600NS		600
#define SERIAL_1_2US		1200
#define SERIAL_10US			10000

static const int num_adc_stages_611x = 3;

static void handle_a_interrupt(struct comedi_device *dev, unsigned short status,
			       unsigned ai_mite_status);
static void handle_b_interrupt(struct comedi_device *dev, unsigned short status,
			       unsigned ao_mite_status);
static void get_last_sample_611x(struct comedi_device *dev);
static void get_last_sample_6143(struct comedi_device *dev);

static inline void ni_set_bitfield(struct comedi_device *dev, int reg,
				   unsigned bit_mask, unsigned bit_values)
{
	unsigned long flags;

	spin_lock_irqsave(&devpriv->soft_reg_copy_lock, flags);
	switch (reg) {
	case Interrupt_A_Enable_Register:
		devpriv->int_a_enable_reg &= ~bit_mask;
		devpriv->int_a_enable_reg |= bit_values & bit_mask;
		devpriv->stc_writew(dev, devpriv->int_a_enable_reg,
				    Interrupt_A_Enable_Register);
		break;
	case Interrupt_B_Enable_Register:
		devpriv->int_b_enable_reg &= ~bit_mask;
		devpriv->int_b_enable_reg |= bit_values & bit_mask;
		devpriv->stc_writew(dev, devpriv->int_b_enable_reg,
				    Interrupt_B_Enable_Register);
		break;
	case IO_Bidirection_Pin_Register:
		devpriv->io_bidirection_pin_reg &= ~bit_mask;
		devpriv->io_bidirection_pin_reg |= bit_values & bit_mask;
		devpriv->stc_writew(dev, devpriv->io_bidirection_pin_reg,
				    IO_Bidirection_Pin_Register);
		break;
	case AI_AO_Select:
		devpriv->ai_ao_select_reg &= ~bit_mask;
		devpriv->ai_ao_select_reg |= bit_values & bit_mask;
		ni_writeb(devpriv->ai_ao_select_reg, AI_AO_Select);
		break;
	case G0_G1_Select:
		devpriv->g0_g1_select_reg &= ~bit_mask;
		devpriv->g0_g1_select_reg |= bit_values & bit_mask;
		ni_writeb(devpriv->g0_g1_select_reg, G0_G1_Select);
		break;
	default:
		printk("Warning %s() called with invalid register\n", __func__);
		printk("reg is %d\n", reg);
		break;
	}
	mmiowb();
	spin_unlock_irqrestore(&devpriv->soft_reg_copy_lock, flags);
}

#ifdef PCIDMA
static int ni_ai_drain_dma(struct comedi_device *dev);

/* DMA channel setup */

/* negative channel means no channel */
static inline void ni_set_ai_dma_channel(struct comedi_device *dev, int channel)
{
	unsigned bitfield;

	if (channel >= 0) {
		bitfield =
		    (ni_stc_dma_channel_select_bitfield(channel) <<
		     AI_DMA_Select_Shift) & AI_DMA_Select_Mask;
	} else {
		bitfield = 0;
	}
	ni_set_bitfield(dev, AI_AO_Select, AI_DMA_Select_Mask, bitfield);
}

/* negative channel means no channel */
static inline void ni_set_ao_dma_channel(struct comedi_device *dev, int channel)
{
	unsigned bitfield;

	if (channel >= 0) {
		bitfield =
		    (ni_stc_dma_channel_select_bitfield(channel) <<
		     AO_DMA_Select_Shift) & AO_DMA_Select_Mask;
	} else {
		bitfield = 0;
	}
	ni_set_bitfield(dev, AI_AO_Select, AO_DMA_Select_Mask, bitfield);
}

/* negative mite_channel means no channel */
static inline void ni_set_gpct_dma_channel(struct comedi_device *dev,
					   unsigned gpct_index,
					   int mite_channel)
{
	unsigned bitfield;

	if (mite_channel >= 0) {
		bitfield = GPCT_DMA_Select_Bits(gpct_index, mite_channel);
	} else {
		bitfield = 0;
	}
	ni_set_bitfield(dev, G0_G1_Select, GPCT_DMA_Select_Mask(gpct_index),
			bitfield);
}

/* negative mite_channel means no channel */
static inline void ni_set_cdo_dma_channel(struct comedi_device *dev,
					  int mite_channel)
{
	unsigned long flags;

	spin_lock_irqsave(&devpriv->soft_reg_copy_lock, flags);
	devpriv->cdio_dma_select_reg &= ~CDO_DMA_Select_Mask;
	if (mite_channel >= 0) {
		/*XXX just guessing ni_stc_dma_channel_select_bitfield() returns the right bits,
		   under the assumption the cdio dma selection works just like ai/ao/gpct.
		   Definitely works for dma channels 0 and 1. */
		devpriv->cdio_dma_select_reg |=
		    (ni_stc_dma_channel_select_bitfield(mite_channel) <<
		     CDO_DMA_Select_Shift) & CDO_DMA_Select_Mask;
	}
	ni_writeb(devpriv->cdio_dma_select_reg, M_Offset_CDIO_DMA_Select);
	mmiowb();
	spin_unlock_irqrestore(&devpriv->soft_reg_copy_lock, flags);
}

static int ni_request_ai_mite_channel(struct comedi_device *dev)
{
	unsigned long flags;

	spin_lock_irqsave(&devpriv->mite_channel_lock, flags);
	BUG_ON(devpriv->ai_mite_chan);
	devpriv->ai_mite_chan =
	    mite_request_channel(devpriv->mite, devpriv->ai_mite_ring);
	if (devpriv->ai_mite_chan == NULL) {
		spin_unlock_irqrestore(&devpriv->mite_channel_lock, flags);
		comedi_error(dev,
			     "failed to reserve mite dma channel for analog input.");
		return -EBUSY;
	}
	devpriv->ai_mite_chan->dir = COMEDI_INPUT;
	ni_set_ai_dma_channel(dev, devpriv->ai_mite_chan->channel);
	spin_unlock_irqrestore(&devpriv->mite_channel_lock, flags);
	return 0;
}

static int ni_request_ao_mite_channel(struct comedi_device *dev)
{
	unsigned long flags;

	spin_lock_irqsave(&devpriv->mite_channel_lock, flags);
	BUG_ON(devpriv->ao_mite_chan);
	devpriv->ao_mite_chan =
	    mite_request_channel(devpriv->mite, devpriv->ao_mite_ring);
	if (devpriv->ao_mite_chan == NULL) {
		spin_unlock_irqrestore(&devpriv->mite_channel_lock, flags);
		comedi_error(dev,
			     "failed to reserve mite dma channel for analog outut.");
		return -EBUSY;
	}
	devpriv->ao_mite_chan->dir = COMEDI_OUTPUT;
	ni_set_ao_dma_channel(dev, devpriv->ao_mite_chan->channel);
	spin_unlock_irqrestore(&devpriv->mite_channel_lock, flags);
	return 0;
}

static int ni_request_gpct_mite_channel(struct comedi_device *dev,
					unsigned gpct_index,
					enum comedi_io_direction direction)
{
	unsigned long flags;
	struct mite_channel *mite_chan;

	BUG_ON(gpct_index >= NUM_GPCT);
	spin_lock_irqsave(&devpriv->mite_channel_lock, flags);
	BUG_ON(devpriv->counter_dev->counters[gpct_index].mite_chan);
	mite_chan =
	    mite_request_channel(devpriv->mite,
				 devpriv->gpct_mite_ring[gpct_index]);
	if (mite_chan == NULL) {
		spin_unlock_irqrestore(&devpriv->mite_channel_lock, flags);
		comedi_error(dev,
			     "failed to reserve mite dma channel for counter.");
		return -EBUSY;
	}
	mite_chan->dir = direction;
	ni_tio_set_mite_channel(&devpriv->counter_dev->counters[gpct_index],
				mite_chan);
	ni_set_gpct_dma_channel(dev, gpct_index, mite_chan->channel);
	spin_unlock_irqrestore(&devpriv->mite_channel_lock, flags);
	return 0;
}

#endif /*  PCIDMA */

static int ni_request_cdo_mite_channel(struct comedi_device *dev)
{
#ifdef PCIDMA
	unsigned long flags;

	spin_lock_irqsave(&devpriv->mite_channel_lock, flags);
	BUG_ON(devpriv->cdo_mite_chan);
	devpriv->cdo_mite_chan =
	    mite_request_channel(devpriv->mite, devpriv->cdo_mite_ring);
	if (devpriv->cdo_mite_chan == NULL) {
		spin_unlock_irqrestore(&devpriv->mite_channel_lock, flags);
		comedi_error(dev,
			     "failed to reserve mite dma channel for correlated digital outut.");
		return -EBUSY;
	}
	devpriv->cdo_mite_chan->dir = COMEDI_OUTPUT;
	ni_set_cdo_dma_channel(dev, devpriv->cdo_mite_chan->channel);
	spin_unlock_irqrestore(&devpriv->mite_channel_lock, flags);
#endif /*  PCIDMA */
	return 0;
}

static void ni_release_ai_mite_channel(struct comedi_device *dev)
{
#ifdef PCIDMA
	unsigned long flags;

	spin_lock_irqsave(&devpriv->mite_channel_lock, flags);
	if (devpriv->ai_mite_chan) {
		ni_set_ai_dma_channel(dev, -1);
		mite_release_channel(devpriv->ai_mite_chan);
		devpriv->ai_mite_chan = NULL;
	}
	spin_unlock_irqrestore(&devpriv->mite_channel_lock, flags);
#endif /*  PCIDMA */
}

static void ni_release_ao_mite_channel(struct comedi_device *dev)
{
#ifdef PCIDMA
	unsigned long flags;

	spin_lock_irqsave(&devpriv->mite_channel_lock, flags);
	if (devpriv->ao_mite_chan) {
		ni_set_ao_dma_channel(dev, -1);
		mite_release_channel(devpriv->ao_mite_chan);
		devpriv->ao_mite_chan = NULL;
	}
	spin_unlock_irqrestore(&devpriv->mite_channel_lock, flags);
#endif /*  PCIDMA */
}

void ni_release_gpct_mite_channel(struct comedi_device *dev,
				  unsigned gpct_index)
{
#ifdef PCIDMA
	unsigned long flags;

	BUG_ON(gpct_index >= NUM_GPCT);
	spin_lock_irqsave(&devpriv->mite_channel_lock, flags);
	if (devpriv->counter_dev->counters[gpct_index].mite_chan) {
		struct mite_channel *mite_chan =
		    devpriv->counter_dev->counters[gpct_index].mite_chan;

		ni_set_gpct_dma_channel(dev, gpct_index, -1);
		ni_tio_set_mite_channel(&devpriv->
					counter_dev->counters[gpct_index],
					NULL);
		mite_release_channel(mite_chan);
	}
	spin_unlock_irqrestore(&devpriv->mite_channel_lock, flags);
#endif /*  PCIDMA */
}

static void ni_release_cdo_mite_channel(struct comedi_device *dev)
{
#ifdef PCIDMA
	unsigned long flags;

	spin_lock_irqsave(&devpriv->mite_channel_lock, flags);
	if (devpriv->cdo_mite_chan) {
		ni_set_cdo_dma_channel(dev, -1);
		mite_release_channel(devpriv->cdo_mite_chan);
		devpriv->cdo_mite_chan = NULL;
	}
	spin_unlock_irqrestore(&devpriv->mite_channel_lock, flags);
#endif /*  PCIDMA */
}

/* e-series boards use the second irq signals to generate dma requests for their counters */
#ifdef PCIDMA
static void ni_e_series_enable_second_irq(struct comedi_device *dev,
					  unsigned gpct_index, short enable)
{
	if (boardtype.reg_type & ni_reg_m_series_mask)
		return;
	switch (gpct_index) {
	case 0:
		if (enable) {
			devpriv->stc_writew(dev, G0_Gate_Second_Irq_Enable,
					    Second_IRQ_A_Enable_Register);
		} else {
			devpriv->stc_writew(dev, 0,
					    Second_IRQ_A_Enable_Register);
		}
		break;
	case 1:
		if (enable) {
			devpriv->stc_writew(dev, G1_Gate_Second_Irq_Enable,
					    Second_IRQ_B_Enable_Register);
		} else {
			devpriv->stc_writew(dev, 0,
					    Second_IRQ_B_Enable_Register);
		}
		break;
	default:
		BUG();
		break;
	}
}
#endif /*  PCIDMA */

static void ni_clear_ai_fifo(struct comedi_device *dev)
{
	if (boardtype.reg_type == ni_reg_6143) {
		/*  Flush the 6143 data FIFO */
		ni_writel(0x10, AIFIFO_Control_6143);	/*  Flush fifo */
		ni_writel(0x00, AIFIFO_Control_6143);	/*  Flush fifo */
		while (ni_readl(AIFIFO_Status_6143) & 0x10) ;	/*  Wait for complete */
	} else {
		devpriv->stc_writew(dev, 1, ADC_FIFO_Clear);
		if (boardtype.reg_type == ni_reg_625x) {
			ni_writeb(0, M_Offset_Static_AI_Control(0));
			ni_writeb(1, M_Offset_Static_AI_Control(0));
#if 0
			/* the NI example code does 3 convert pulses for 625x boards,
			   but that appears to be wrong in practice. */
			devpriv->stc_writew(dev, AI_CONVERT_Pulse,
					    AI_Command_1_Register);
			devpriv->stc_writew(dev, AI_CONVERT_Pulse,
					    AI_Command_1_Register);
			devpriv->stc_writew(dev, AI_CONVERT_Pulse,
					    AI_Command_1_Register);
#endif
		}
	}
}

static void win_out2(struct comedi_device *dev, uint32_t data, int reg)
{
	devpriv->stc_writew(dev, data >> 16, reg);
	devpriv->stc_writew(dev, data & 0xffff, reg + 1);
}

static uint32_t win_in2(struct comedi_device *dev, int reg)
{
	uint32_t bits;
	bits = devpriv->stc_readw(dev, reg) << 16;
	bits |= devpriv->stc_readw(dev, reg + 1);
	return bits;
}

#define ao_win_out(data, addr) ni_ao_win_outw(dev, data, addr)
static inline void ni_ao_win_outw(struct comedi_device *dev, uint16_t data,
				  int addr)
{
	unsigned long flags;

	spin_lock_irqsave(&devpriv->window_lock, flags);
	ni_writew(addr, AO_Window_Address_611x);
	ni_writew(data, AO_Window_Data_611x);
	spin_unlock_irqrestore(&devpriv->window_lock, flags);
}

static inline void ni_ao_win_outl(struct comedi_device *dev, uint32_t data,
				  int addr)
{
	unsigned long flags;

	spin_lock_irqsave(&devpriv->window_lock, flags);
	ni_writew(addr, AO_Window_Address_611x);
	ni_writel(data, AO_Window_Data_611x);
	spin_unlock_irqrestore(&devpriv->window_lock, flags);
}

static inline unsigned short ni_ao_win_inw(struct comedi_device *dev, int addr)
{
	unsigned long flags;
	unsigned short data;

	spin_lock_irqsave(&devpriv->window_lock, flags);
	ni_writew(addr, AO_Window_Address_611x);
	data = ni_readw(AO_Window_Data_611x);
	spin_unlock_irqrestore(&devpriv->window_lock, flags);
	return data;
}

/* ni_set_bits( ) allows different parts of the ni_mio_common driver to
* share registers (such as Interrupt_A_Register) without interfering with
* each other.
*
* NOTE: the switch/case statements are optimized out for a constant argument
* so this is actually quite fast---  If you must wrap another function around this
* make it inline to avoid a large speed penalty.
*
* value should only be 1 or 0.
*/
static inline void ni_set_bits(struct comedi_device *dev, int reg,
			       unsigned bits, unsigned value)
{
	unsigned bit_values;

	if (value)
		bit_values = bits;
	else
		bit_values = 0;
	ni_set_bitfield(dev, reg, bits, bit_values);
}

static irqreturn_t ni_E_interrupt(int irq, void *d)
{
	struct comedi_device *dev = d;
	unsigned short a_status;
	unsigned short b_status;
	unsigned int ai_mite_status = 0;
	unsigned int ao_mite_status = 0;
	unsigned long flags;
#ifdef PCIDMA
	struct mite_struct *mite = devpriv->mite;
#endif

	if (dev->attached == 0)
		return IRQ_NONE;
	smp_mb();		/*  make sure dev->attached is checked before handler does anything else. */

	/*  lock to avoid race with comedi_poll */
	spin_lock_irqsave(&dev->spinlock, flags);
	a_status = devpriv->stc_readw(dev, AI_Status_1_Register);
	b_status = devpriv->stc_readw(dev, AO_Status_1_Register);
#ifdef PCIDMA
	if (mite) {
		unsigned long flags_too;

		spin_lock_irqsave(&devpriv->mite_channel_lock, flags_too);
		if (devpriv->ai_mite_chan) {
			ai_mite_status = mite_get_status(devpriv->ai_mite_chan);
			if (ai_mite_status & CHSR_LINKC)
				writel(CHOR_CLRLC,
				       devpriv->mite->mite_io_addr +
				       MITE_CHOR(devpriv->
						 ai_mite_chan->channel));
		}
		if (devpriv->ao_mite_chan) {
			ao_mite_status = mite_get_status(devpriv->ao_mite_chan);
			if (ao_mite_status & CHSR_LINKC)
				writel(CHOR_CLRLC,
				       mite->mite_io_addr +
				       MITE_CHOR(devpriv->
						 ao_mite_chan->channel));
		}
		spin_unlock_irqrestore(&devpriv->mite_channel_lock, flags_too);
	}
#endif
	ack_a_interrupt(dev, a_status);
	ack_b_interrupt(dev, b_status);
	if ((a_status & Interrupt_A_St) || (ai_mite_status & CHSR_INT))
		handle_a_interrupt(dev, a_status, ai_mite_status);
	if ((b_status & Interrupt_B_St) || (ao_mite_status & CHSR_INT))
		handle_b_interrupt(dev, b_status, ao_mite_status);
	handle_gpct_interrupt(dev, 0);
	handle_gpct_interrupt(dev, 1);
	handle_cdio_interrupt(dev);

	spin_unlock_irqrestore(&dev->spinlock, flags);
	return IRQ_HANDLED;
}

#ifdef PCIDMA
static void ni_sync_ai_dma(struct comedi_device *dev)
{
	struct comedi_subdevice *s = dev->subdevices + NI_AI_SUBDEV;
	unsigned long flags;

	spin_lock_irqsave(&devpriv->mite_channel_lock, flags);
	if (devpriv->ai_mite_chan)
		mite_sync_input_dma(devpriv->ai_mite_chan, s->async);
	spin_unlock_irqrestore(&devpriv->mite_channel_lock, flags);
}

static void mite_handle_b_linkc(struct mite_struct *mite,
				struct comedi_device *dev)
{
	struct comedi_subdevice *s = dev->subdevices + NI_AO_SUBDEV;
	unsigned long flags;

	spin_lock_irqsave(&devpriv->mite_channel_lock, flags);
	if (devpriv->ao_mite_chan) {
		mite_sync_output_dma(devpriv->ao_mite_chan, s->async);
	}
	spin_unlock_irqrestore(&devpriv->mite_channel_lock, flags);
}

static int ni_ao_wait_for_dma_load(struct comedi_device *dev)
{
	static const int timeout = 10000;
	int i;
	for (i = 0; i < timeout; i++) {
		unsigned short b_status;

		b_status = devpriv->stc_readw(dev, AO_Status_1_Register);
		if (b_status & AO_FIFO_Half_Full_St)
			break;
		/* if we poll too often, the pci bus activity seems
		   to slow the dma transfer down */
		udelay(10);
	}
	if (i == timeout) {
		comedi_error(dev, "timed out waiting for dma load");
		return -EPIPE;
	}
	return 0;
}

#endif /* PCIDMA */
static void ni_handle_eos(struct comedi_device *dev, struct comedi_subdevice *s)
{
	if (devpriv->aimode == AIMODE_SCAN) {
#ifdef PCIDMA
		static const int timeout = 10;
		int i;

		for (i = 0; i < timeout; i++) {
			ni_sync_ai_dma(dev);
			if ((s->async->events & COMEDI_CB_EOS))
				break;
			udelay(1);
		}
#else
		ni_handle_fifo_dregs(dev);
		s->async->events |= COMEDI_CB_EOS;
#endif
	}
	/* handle special case of single scan using AI_End_On_End_Of_Scan */
	if ((devpriv->ai_cmd2 & AI_End_On_End_Of_Scan)) {
		shutdown_ai_command(dev);
	}
}

static void shutdown_ai_command(struct comedi_device *dev)
{
	struct comedi_subdevice *s = dev->subdevices + NI_AI_SUBDEV;

#ifdef PCIDMA
	ni_ai_drain_dma(dev);
#endif
	ni_handle_fifo_dregs(dev);
	get_last_sample_611x(dev);
	get_last_sample_6143(dev);

	s->async->events |= COMEDI_CB_EOA;
}

static void ni_event(struct comedi_device *dev, struct comedi_subdevice *s)
{
	if (s->
	    async->events & (COMEDI_CB_ERROR | COMEDI_CB_OVERFLOW |
			     COMEDI_CB_EOA)) {
		switch (s - dev->subdevices) {
		case NI_AI_SUBDEV:
			ni_ai_reset(dev, s);
			break;
		case NI_AO_SUBDEV:
			ni_ao_reset(dev, s);
			break;
		case NI_GPCT0_SUBDEV:
		case NI_GPCT1_SUBDEV:
			ni_gpct_cancel(dev, s);
			break;
		case NI_DIO_SUBDEV:
			ni_cdio_cancel(dev, s);
			break;
		default:
			break;
		}
	}
	comedi_event(dev, s);
}

static void handle_gpct_interrupt(struct comedi_device *dev,
				  unsigned short counter_index)
{
#ifdef PCIDMA
	struct comedi_subdevice *s =
	    dev->subdevices + NI_GPCT_SUBDEV(counter_index);

	ni_tio_handle_interrupt(&devpriv->counter_dev->counters[counter_index],
				s);
	if (s->async->events)
		ni_event(dev, s);
#endif
}

static void ack_a_interrupt(struct comedi_device *dev, unsigned short a_status)
{
	unsigned short ack = 0;

	if (a_status & AI_SC_TC_St) {
		ack |= AI_SC_TC_Interrupt_Ack;
	}
	if (a_status & AI_START1_St) {
		ack |= AI_START1_Interrupt_Ack;
	}
	if (a_status & AI_START_St) {
		ack |= AI_START_Interrupt_Ack;
	}
	if (a_status & AI_STOP_St) {
		/* not sure why we used to ack the START here also, instead of doing it independently. Frank Hess 2007-07-06 */
		ack |= AI_STOP_Interrupt_Ack /*| AI_START_Interrupt_Ack */ ;
	}
	if (ack)
		devpriv->stc_writew(dev, ack, Interrupt_A_Ack_Register);
}

static void handle_a_interrupt(struct comedi_device *dev, unsigned short status,
			       unsigned ai_mite_status)
{
	struct comedi_subdevice *s = dev->subdevices + NI_AI_SUBDEV;

	/* 67xx boards don't have ai subdevice, but their gpct0 might generate an a interrupt */
	if (s->type == COMEDI_SUBD_UNUSED)
		return;

#ifdef DEBUG_INTERRUPT
	printk
	    ("ni_mio_common: interrupt: a_status=%04x ai_mite_status=%08x\n",
	     status, ai_mite_status);
	ni_mio_print_status_a(status);
#endif
#ifdef PCIDMA
	if (ai_mite_status & CHSR_LINKC) {
		ni_sync_ai_dma(dev);
	}

	if (ai_mite_status & ~(CHSR_INT | CHSR_LINKC | CHSR_DONE | CHSR_MRDY |
			       CHSR_DRDY | CHSR_DRQ1 | CHSR_DRQ0 | CHSR_ERROR |
			       CHSR_SABORT | CHSR_XFERR | CHSR_LxERR_mask)) {
		printk
		    ("unknown mite interrupt, ack! (ai_mite_status=%08x)\n",
		     ai_mite_status);
		/* mite_print_chsr(ai_mite_status); */
		s->async->events |= COMEDI_CB_ERROR | COMEDI_CB_EOA;
		/* disable_irq(dev->irq); */
	}
#endif

	/* test for all uncommon interrupt events at the same time */
	if (status & (AI_Overrun_St | AI_Overflow_St | AI_SC_TC_Error_St |
		      AI_SC_TC_St | AI_START1_St)) {
		if (status == 0xffff) {
			printk
			    ("ni_mio_common: a_status=0xffff.  Card removed?\n");
			/* we probably aren't even running a command now,
			 * so it's a good idea to be careful. */
			if (comedi_get_subdevice_runflags(s) & SRF_RUNNING) {
				s->async->events |=
				    COMEDI_CB_ERROR | COMEDI_CB_EOA;
				ni_event(dev, s);
			}
			return;
		}
		if (status & (AI_Overrun_St | AI_Overflow_St |
			      AI_SC_TC_Error_St)) {
			printk("ni_mio_common: ai error a_status=%04x\n",
			       status);
			ni_mio_print_status_a(status);

			shutdown_ai_command(dev);

			s->async->events |= COMEDI_CB_ERROR;
			if (status & (AI_Overrun_St | AI_Overflow_St))
				s->async->events |= COMEDI_CB_OVERFLOW;

			ni_event(dev, s);

			return;
		}
		if (status & AI_SC_TC_St) {
#ifdef DEBUG_INTERRUPT
			printk("ni_mio_common: SC_TC interrupt\n");
#endif
			if (!devpriv->ai_continuous) {
				shutdown_ai_command(dev);
			}
		}
	}
#ifndef PCIDMA
	if (status & AI_FIFO_Half_Full_St) {
		int i;
		static const int timeout = 10;
		/* pcmcia cards (at least 6036) seem to stop producing interrupts if we
		 *fail to get the fifo less than half full, so loop to be sure.*/
		for (i = 0; i < timeout; ++i) {
			ni_handle_fifo_half_full(dev);
			if ((devpriv->stc_readw(dev,
						AI_Status_1_Register) &
			     AI_FIFO_Half_Full_St) == 0)
				break;
		}
	}
#endif /*  !PCIDMA */

	if ((status & AI_STOP_St)) {
		ni_handle_eos(dev, s);
	}

	ni_event(dev, s);

#ifdef DEBUG_INTERRUPT
	status = devpriv->stc_readw(dev, AI_Status_1_Register);
	if (status & Interrupt_A_St) {
		printk
		    ("handle_a_interrupt: didn't clear interrupt? status=0x%x\n",
		     status);
	}
#endif
}

static void ack_b_interrupt(struct comedi_device *dev, unsigned short b_status)
{
	unsigned short ack = 0;
	if (b_status & AO_BC_TC_St) {
		ack |= AO_BC_TC_Interrupt_Ack;
	}
	if (b_status & AO_Overrun_St) {
		ack |= AO_Error_Interrupt_Ack;
	}
	if (b_status & AO_START_St) {
		ack |= AO_START_Interrupt_Ack;
	}
	if (b_status & AO_START1_St) {
		ack |= AO_START1_Interrupt_Ack;
	}
	if (b_status & AO_UC_TC_St) {
		ack |= AO_UC_TC_Interrupt_Ack;
	}
	if (b_status & AO_UI2_TC_St) {
		ack |= AO_UI2_TC_Interrupt_Ack;
	}
	if (b_status & AO_UPDATE_St) {
		ack |= AO_UPDATE_Interrupt_Ack;
	}
	if (ack)
		devpriv->stc_writew(dev, ack, Interrupt_B_Ack_Register);
}

static void handle_b_interrupt(struct comedi_device *dev,
			       unsigned short b_status, unsigned ao_mite_status)
{
	struct comedi_subdevice *s = dev->subdevices + NI_AO_SUBDEV;
	/* unsigned short ack=0; */
#ifdef DEBUG_INTERRUPT
	printk("ni_mio_common: interrupt: b_status=%04x m1_status=%08x\n",
	       b_status, ao_mite_status);
	ni_mio_print_status_b(b_status);
#endif

#ifdef PCIDMA
	/* Currently, mite.c requires us to handle LINKC */
	if (ao_mite_status & CHSR_LINKC) {
		mite_handle_b_linkc(devpriv->mite, dev);
	}

	if (ao_mite_status & ~(CHSR_INT | CHSR_LINKC | CHSR_DONE | CHSR_MRDY |
			       CHSR_DRDY | CHSR_DRQ1 | CHSR_DRQ0 | CHSR_ERROR |
			       CHSR_SABORT | CHSR_XFERR | CHSR_LxERR_mask)) {
		printk
		    ("unknown mite interrupt, ack! (ao_mite_status=%08x)\n",
		     ao_mite_status);
		/* mite_print_chsr(ao_mite_status); */
		s->async->events |= COMEDI_CB_EOA | COMEDI_CB_ERROR;
	}
#endif

	if (b_status == 0xffff)
		return;
	if (b_status & AO_Overrun_St) {
		printk
		    ("ni_mio_common: AO FIFO underrun status=0x%04x status2=0x%04x\n",
		     b_status, devpriv->stc_readw(dev, AO_Status_2_Register));
		s->async->events |= COMEDI_CB_OVERFLOW;
	}

	if (b_status & AO_BC_TC_St) {
		MDPRINTK
		    ("ni_mio_common: AO BC_TC status=0x%04x status2=0x%04x\n",
		     b_status, devpriv->stc_readw(dev, AO_Status_2_Register));
		s->async->events |= COMEDI_CB_EOA;
	}
#ifndef PCIDMA
	if (b_status & AO_FIFO_Request_St) {
		int ret;

		ret = ni_ao_fifo_half_empty(dev, s);
		if (!ret) {
			printk("ni_mio_common: AO buffer underrun\n");
			ni_set_bits(dev, Interrupt_B_Enable_Register,
				    AO_FIFO_Interrupt_Enable |
				    AO_Error_Interrupt_Enable, 0);
			s->async->events |= COMEDI_CB_OVERFLOW;
		}
	}
#endif

	ni_event(dev, s);
}

#ifdef DEBUG_STATUS_A
static const char *const status_a_strings[] = {
	"passthru0", "fifo", "G0_gate", "G0_TC",
	"stop", "start", "sc_tc", "start1",
	"start2", "sc_tc_error", "overflow", "overrun",
	"fifo_empty", "fifo_half_full", "fifo_full", "interrupt_a"
};

static void ni_mio_print_status_a(int status)
{
	int i;

	printk("A status:");
	for (i = 15; i >= 0; i--) {
		if (status & (1 << i)) {
			printk(" %s", status_a_strings[i]);
		}
	}
	printk("\n");
}
#endif

#ifdef DEBUG_STATUS_B
static const char *const status_b_strings[] = {
	"passthru1", "fifo", "G1_gate", "G1_TC",
	"UI2_TC", "UPDATE", "UC_TC", "BC_TC",
	"start1", "overrun", "start", "bc_tc_error",
	"fifo_empty", "fifo_half_full", "fifo_full", "interrupt_b"
};

static void ni_mio_print_status_b(int status)
{
	int i;

	printk("B status:");
	for (i = 15; i >= 0; i--) {
		if (status & (1 << i)) {
			printk(" %s", status_b_strings[i]);
		}
	}
	printk("\n");
}
#endif

#ifndef PCIDMA

static void ni_ao_fifo_load(struct comedi_device *dev,
			    struct comedi_subdevice *s, int n)
{
	struct comedi_async *async = s->async;
	struct comedi_cmd *cmd = &async->cmd;
	int chan;
	int i;
	short d;
	u32 packed_data;
	int range;
	int err = 1;

	chan = async->cur_chan;
	for (i = 0; i < n; i++) {
		err &= comedi_buf_get(async, &d);
		if (err == 0)
			break;

		range = CR_RANGE(cmd->chanlist[chan]);

		if (boardtype.reg_type & ni_reg_6xxx_mask) {
			packed_data = d & 0xffff;
			/* 6711 only has 16 bit wide ao fifo */
			if (boardtype.reg_type != ni_reg_6711) {
				err &= comedi_buf_get(async, &d);
				if (err == 0)
					break;
				chan++;
				i++;
				packed_data |= (d << 16) & 0xffff0000;
			}
			ni_writel(packed_data, DAC_FIFO_Data_611x);
		} else {
			ni_writew(d, DAC_FIFO_Data);
		}
		chan++;
		chan %= cmd->chanlist_len;
	}
	async->cur_chan = chan;
	if (err == 0) {
		async->events |= COMEDI_CB_OVERFLOW;
	}
}

/*
 *  There's a small problem if the FIFO gets really low and we
 *  don't have the data to fill it.  Basically, if after we fill
 *  the FIFO with all the data available, the FIFO is _still_
 *  less than half full, we never clear the interrupt.  If the
 *  IRQ is in edge mode, we never get another interrupt, because
 *  this one wasn't cleared.  If in level mode, we get flooded
 *  with interrupts that we can't fulfill, because nothing ever
 *  gets put into the buffer.
 *
 *  This kind of situation is recoverable, but it is easier to
 *  just pretend we had a FIFO underrun, since there is a good
 *  chance it will happen anyway.  This is _not_ the case for
 *  RT code, as RT code might purposely be running close to the
 *  metal.  Needs to be fixed eventually.
 */
static int ni_ao_fifo_half_empty(struct comedi_device *dev,
				 struct comedi_subdevice *s)
{
	int n;

	n = comedi_buf_read_n_available(s->async);
	if (n == 0) {
		s->async->events |= COMEDI_CB_OVERFLOW;
		return 0;
	}

	n /= sizeof(short);
	if (n > boardtype.ao_fifo_depth / 2)
		n = boardtype.ao_fifo_depth / 2;

	ni_ao_fifo_load(dev, s, n);

	s->async->events |= COMEDI_CB_BLOCK;

	return 1;
}

static int ni_ao_prep_fifo(struct comedi_device *dev,
			   struct comedi_subdevice *s)
{
	int n;

	/* reset fifo */
	devpriv->stc_writew(dev, 1, DAC_FIFO_Clear);
	if (boardtype.reg_type & ni_reg_6xxx_mask)
		ni_ao_win_outl(dev, 0x6, AO_FIFO_Offset_Load_611x);

	/* load some data */
	n = comedi_buf_read_n_available(s->async);
	if (n == 0)
		return 0;

	n /= sizeof(short);
	if (n > boardtype.ao_fifo_depth)
		n = boardtype.ao_fifo_depth;

	ni_ao_fifo_load(dev, s, n);

	return n;
}

static void ni_ai_fifo_read(struct comedi_device *dev,
			    struct comedi_subdevice *s, int n)
{
	struct comedi_async *async = s->async;
	int i;

	if (boardtype.reg_type == ni_reg_611x) {
		short data[2];
		u32 dl;

		for (i = 0; i < n / 2; i++) {
			dl = ni_readl(ADC_FIFO_Data_611x);
			/* This may get the hi/lo data in the wrong order */
			data[0] = (dl >> 16) & 0xffff;
			data[1] = dl & 0xffff;
			cfc_write_array_to_buffer(s, data, sizeof(data));
		}
		/* Check if there's a single sample stuck in the FIFO */
		if (n % 2) {
			dl = ni_readl(ADC_FIFO_Data_611x);
			data[0] = dl & 0xffff;
			cfc_write_to_buffer(s, data[0]);
		}
	} else if (boardtype.reg_type == ni_reg_6143) {
		short data[2];
		u32 dl;

		/*  This just reads the FIFO assuming the data is present, no checks on the FIFO status are performed */
		for (i = 0; i < n / 2; i++) {
			dl = ni_readl(AIFIFO_Data_6143);

			data[0] = (dl >> 16) & 0xffff;
			data[1] = dl & 0xffff;
			cfc_write_array_to_buffer(s, data, sizeof(data));
		}
		if (n % 2) {
			/* Assume there is a single sample stuck in the FIFO */
			ni_writel(0x01, AIFIFO_Control_6143);	/*  Get stranded sample into FIFO */
			dl = ni_readl(AIFIFO_Data_6143);
			data[0] = (dl >> 16) & 0xffff;
			cfc_write_to_buffer(s, data[0]);
		}
	} else {
		if (n > sizeof(devpriv->ai_fifo_buffer) /
		    sizeof(devpriv->ai_fifo_buffer[0])) {
			comedi_error(dev, "bug! ai_fifo_buffer too small");
			async->events |= COMEDI_CB_ERROR;
			return;
		}
		for (i = 0; i < n; i++) {
			devpriv->ai_fifo_buffer[i] =
			    ni_readw(ADC_FIFO_Data_Register);
		}
		cfc_write_array_to_buffer(s, devpriv->ai_fifo_buffer,
					  n *
					  sizeof(devpriv->ai_fifo_buffer[0]));
	}
}

static void ni_handle_fifo_half_full(struct comedi_device *dev)
{
	int n;
	struct comedi_subdevice *s = dev->subdevices + NI_AI_SUBDEV;

	n = boardtype.ai_fifo_depth / 2;

	ni_ai_fifo_read(dev, s, n);
}
#endif

#ifdef PCIDMA
static int ni_ai_drain_dma(struct comedi_device *dev)
{
	int i;
	static const int timeout = 10000;
	unsigned long flags;
	int retval = 0;

	spin_lock_irqsave(&devpriv->mite_channel_lock, flags);
	if (devpriv->ai_mite_chan) {
		for (i = 0; i < timeout; i++) {
			if ((devpriv->stc_readw(dev,
						AI_Status_1_Register) &
			     AI_FIFO_Empty_St)
			    && mite_bytes_in_transit(devpriv->ai_mite_chan) ==
			    0)
				break;
			udelay(5);
		}
		if (i == timeout) {
			printk("ni_mio_common: wait for dma drain timed out\n");
			printk
			    ("mite_bytes_in_transit=%i, AI_Status1_Register=0x%x\n",
			     mite_bytes_in_transit(devpriv->ai_mite_chan),
			     devpriv->stc_readw(dev, AI_Status_1_Register));
			retval = -1;
		}
	}
	spin_unlock_irqrestore(&devpriv->mite_channel_lock, flags);

	ni_sync_ai_dma(dev);

	return retval;
}
#endif
/*
   Empties the AI fifo
*/
static void ni_handle_fifo_dregs(struct comedi_device *dev)
{
	struct comedi_subdevice *s = dev->subdevices + NI_AI_SUBDEV;
	short data[2];
	u32 dl;
	short fifo_empty;
	int i;

	if (boardtype.reg_type == ni_reg_611x) {
		while ((devpriv->stc_readw(dev,
					   AI_Status_1_Register) &
			AI_FIFO_Empty_St) == 0) {
			dl = ni_readl(ADC_FIFO_Data_611x);

			/* This may get the hi/lo data in the wrong order */
			data[0] = (dl >> 16);
			data[1] = (dl & 0xffff);
			cfc_write_array_to_buffer(s, data, sizeof(data));
		}
	} else if (boardtype.reg_type == ni_reg_6143) {
		i = 0;
		while (ni_readl(AIFIFO_Status_6143) & 0x04) {
			dl = ni_readl(AIFIFO_Data_6143);

			/* This may get the hi/lo data in the wrong order */
			data[0] = (dl >> 16);
			data[1] = (dl & 0xffff);
			cfc_write_array_to_buffer(s, data, sizeof(data));
			i += 2;
		}
		/*  Check if stranded sample is present */
		if (ni_readl(AIFIFO_Status_6143) & 0x01) {
			ni_writel(0x01, AIFIFO_Control_6143);	/*  Get stranded sample into FIFO */
			dl = ni_readl(AIFIFO_Data_6143);
			data[0] = (dl >> 16) & 0xffff;
			cfc_write_to_buffer(s, data[0]);
		}

	} else {
		fifo_empty =
		    devpriv->stc_readw(dev,
				       AI_Status_1_Register) & AI_FIFO_Empty_St;
		while (fifo_empty == 0) {
			for (i = 0;
			     i <
			     sizeof(devpriv->ai_fifo_buffer) /
			     sizeof(devpriv->ai_fifo_buffer[0]); i++) {
				fifo_empty =
				    devpriv->stc_readw(dev,
						       AI_Status_1_Register) &
				    AI_FIFO_Empty_St;
				if (fifo_empty)
					break;
				devpriv->ai_fifo_buffer[i] =
				    ni_readw(ADC_FIFO_Data_Register);
			}
			cfc_write_array_to_buffer(s, devpriv->ai_fifo_buffer,
						  i *
						  sizeof(devpriv->
							 ai_fifo_buffer[0]));
		}
	}
}

static void get_last_sample_611x(struct comedi_device *dev)
{
	struct comedi_subdevice *s = dev->subdevices + NI_AI_SUBDEV;
	short data;
	u32 dl;

	if (boardtype.reg_type != ni_reg_611x)
		return;

	/* Check if there's a single sample stuck in the FIFO */
	if (ni_readb(XXX_Status) & 0x80) {
		dl = ni_readl(ADC_FIFO_Data_611x);
		data = (dl & 0xffff);
		cfc_write_to_buffer(s, data);
	}
}

static void get_last_sample_6143(struct comedi_device *dev)
{
	struct comedi_subdevice *s = dev->subdevices + NI_AI_SUBDEV;
	short data;
	u32 dl;

	if (boardtype.reg_type != ni_reg_6143)
		return;

	/* Check if there's a single sample stuck in the FIFO */
	if (ni_readl(AIFIFO_Status_6143) & 0x01) {
		ni_writel(0x01, AIFIFO_Control_6143);	/*  Get stranded sample into FIFO */
		dl = ni_readl(AIFIFO_Data_6143);

		/* This may get the hi/lo data in the wrong order */
		data = (dl >> 16) & 0xffff;
		cfc_write_to_buffer(s, data);
	}
}

static void ni_ai_munge(struct comedi_device *dev, struct comedi_subdevice *s,
			void *data, unsigned int num_bytes,
			unsigned int chan_index)
{
	struct comedi_async *async = s->async;
	unsigned int i;
	unsigned int length = num_bytes / bytes_per_sample(s);
	short *array = data;
	unsigned int *larray = data;
	for (i = 0; i < length; i++) {
#ifdef PCIDMA
		if (s->subdev_flags & SDF_LSAMPL)
			larray[i] = le32_to_cpu(larray[i]);
		else
			array[i] = le16_to_cpu(array[i]);
#endif
		if (s->subdev_flags & SDF_LSAMPL)
			larray[i] += devpriv->ai_offset[chan_index];
		else
			array[i] += devpriv->ai_offset[chan_index];
		chan_index++;
		chan_index %= async->cmd.chanlist_len;
	}
}

#ifdef PCIDMA

static int ni_ai_setup_MITE_dma(struct comedi_device *dev)
{
	struct comedi_subdevice *s = dev->subdevices + NI_AI_SUBDEV;
	int retval;
	unsigned long flags;

	retval = ni_request_ai_mite_channel(dev);
	if (retval)
		return retval;
/* printk("comedi_debug: using mite channel %i for ai.\n", devpriv->ai_mite_chan->channel); */

	/* write alloc the entire buffer */
	comedi_buf_write_alloc(s->async, s->async->prealloc_bufsz);

	spin_lock_irqsave(&devpriv->mite_channel_lock, flags);
	if (devpriv->ai_mite_chan == NULL) {
		spin_unlock_irqrestore(&devpriv->mite_channel_lock, flags);
		return -EIO;
	}

	switch (boardtype.reg_type) {
	case ni_reg_611x:
	case ni_reg_6143:
		mite_prep_dma(devpriv->ai_mite_chan, 32, 16);
		break;
	case ni_reg_628x:
		mite_prep_dma(devpriv->ai_mite_chan, 32, 32);
		break;
	default:
		mite_prep_dma(devpriv->ai_mite_chan, 16, 16);
		break;
	};
	/*start the MITE */
	mite_dma_arm(devpriv->ai_mite_chan);
	spin_unlock_irqrestore(&devpriv->mite_channel_lock, flags);

	return 0;
}

static int ni_ao_setup_MITE_dma(struct comedi_device *dev)
{
	struct comedi_subdevice *s = dev->subdevices + NI_AO_SUBDEV;
	int retval;
	unsigned long flags;

	retval = ni_request_ao_mite_channel(dev);
	if (retval)
		return retval;

	/* read alloc the entire buffer */
	comedi_buf_read_alloc(s->async, s->async->prealloc_bufsz);

	spin_lock_irqsave(&devpriv->mite_channel_lock, flags);
	if (devpriv->ao_mite_chan) {
		if (boardtype.reg_type & (ni_reg_611x | ni_reg_6713)) {
			mite_prep_dma(devpriv->ao_mite_chan, 32, 32);
		} else {
			/* doing 32 instead of 16 bit wide transfers from memory
			   makes the mite do 32 bit pci transfers, doubling pci bandwidth. */
			mite_prep_dma(devpriv->ao_mite_chan, 16, 32);
		}
		mite_dma_arm(devpriv->ao_mite_chan);
	} else
		retval = -EIO;
	spin_unlock_irqrestore(&devpriv->mite_channel_lock, flags);

	return retval;
}

#endif /*  PCIDMA */

/*
   used for both cancel ioctl and board initialization

   this is pretty harsh for a cancel, but it works...
 */

static int ni_ai_reset(struct comedi_device *dev, struct comedi_subdevice *s)
{
	ni_release_ai_mite_channel(dev);
	/* ai configuration */
	devpriv->stc_writew(dev, AI_Configuration_Start | AI_Reset,
			    Joint_Reset_Register);

	ni_set_bits(dev, Interrupt_A_Enable_Register,
		    AI_SC_TC_Interrupt_Enable | AI_START1_Interrupt_Enable |
		    AI_START2_Interrupt_Enable | AI_START_Interrupt_Enable |
		    AI_STOP_Interrupt_Enable | AI_Error_Interrupt_Enable |
		    AI_FIFO_Interrupt_Enable, 0);

	ni_clear_ai_fifo(dev);

	if (boardtype.reg_type != ni_reg_6143)
		ni_writeb(0, Misc_Command);

	devpriv->stc_writew(dev, AI_Disarm, AI_Command_1_Register);	/* reset pulses */
	devpriv->stc_writew(dev,
			    AI_Start_Stop | AI_Mode_1_Reserved
			    /*| AI_Trigger_Once */ ,
			    AI_Mode_1_Register);
	devpriv->stc_writew(dev, 0x0000, AI_Mode_2_Register);
	/* generate FIFO interrupts on non-empty */
	devpriv->stc_writew(dev, (0 << 6) | 0x0000, AI_Mode_3_Register);
	if (boardtype.reg_type == ni_reg_611x) {
		devpriv->stc_writew(dev, AI_SHIFTIN_Pulse_Width |
				    AI_SOC_Polarity |
				    AI_LOCALMUX_CLK_Pulse_Width,
				    AI_Personal_Register);
		devpriv->stc_writew(dev,
				    AI_SCAN_IN_PROG_Output_Select(3) |
				    AI_EXTMUX_CLK_Output_Select(0) |
				    AI_LOCALMUX_CLK_Output_Select(2) |
				    AI_SC_TC_Output_Select(3) |
				    AI_CONVERT_Output_Select
				    (AI_CONVERT_Output_Enable_High),
				    AI_Output_Control_Register);
	} else if (boardtype.reg_type == ni_reg_6143) {
		devpriv->stc_writew(dev, AI_SHIFTIN_Pulse_Width |
				    AI_SOC_Polarity |
				    AI_LOCALMUX_CLK_Pulse_Width,
				    AI_Personal_Register);
		devpriv->stc_writew(dev,
				    AI_SCAN_IN_PROG_Output_Select(3) |
				    AI_EXTMUX_CLK_Output_Select(0) |
				    AI_LOCALMUX_CLK_Output_Select(2) |
				    AI_SC_TC_Output_Select(3) |
				    AI_CONVERT_Output_Select
				    (AI_CONVERT_Output_Enable_Low),
				    AI_Output_Control_Register);
	} else {
		unsigned ai_output_control_bits;
		devpriv->stc_writew(dev, AI_SHIFTIN_Pulse_Width |
				    AI_SOC_Polarity |
				    AI_CONVERT_Pulse_Width |
				    AI_LOCALMUX_CLK_Pulse_Width,
				    AI_Personal_Register);
		ai_output_control_bits =
		    AI_SCAN_IN_PROG_Output_Select(3) |
		    AI_EXTMUX_CLK_Output_Select(0) |
		    AI_LOCALMUX_CLK_Output_Select(2) |
		    AI_SC_TC_Output_Select(3);
		if (boardtype.reg_type == ni_reg_622x)
			ai_output_control_bits |=
			    AI_CONVERT_Output_Select
			    (AI_CONVERT_Output_Enable_High);
		else
			ai_output_control_bits |=
			    AI_CONVERT_Output_Select
			    (AI_CONVERT_Output_Enable_Low);
		devpriv->stc_writew(dev, ai_output_control_bits,
				    AI_Output_Control_Register);
	}
	/* the following registers should not be changed, because there
	 * are no backup registers in devpriv.  If you want to change
	 * any of these, add a backup register and other appropriate code:
	 *      AI_Mode_1_Register
	 *      AI_Mode_3_Register
	 *      AI_Personal_Register
	 *      AI_Output_Control_Register
	 */
	devpriv->stc_writew(dev, AI_SC_TC_Error_Confirm | AI_START_Interrupt_Ack | AI_START2_Interrupt_Ack | AI_START1_Interrupt_Ack | AI_SC_TC_Interrupt_Ack | AI_Error_Interrupt_Ack | AI_STOP_Interrupt_Ack, Interrupt_A_Ack_Register);	/* clear interrupts */

	devpriv->stc_writew(dev, AI_Configuration_End, Joint_Reset_Register);

	return 0;
}

static int ni_ai_poll(struct comedi_device *dev, struct comedi_subdevice *s)
{
	unsigned long flags = 0;
	int count;

	/*  lock to avoid race with interrupt handler */
	if (in_interrupt() == 0)
		spin_lock_irqsave(&dev->spinlock, flags);
#ifndef PCIDMA
	ni_handle_fifo_dregs(dev);
#else
	ni_sync_ai_dma(dev);
#endif
	count = s->async->buf_write_count - s->async->buf_read_count;
	if (in_interrupt() == 0)
		spin_unlock_irqrestore(&dev->spinlock, flags);

	return count;
}

static int ni_ai_insn_read(struct comedi_device *dev,
			   struct comedi_subdevice *s, struct comedi_insn *insn,
			   unsigned int *data)
{
	int i, n;
	const unsigned int mask = (1 << boardtype.adbits) - 1;
	unsigned signbits;
	unsigned short d;
	unsigned long dl;

	ni_load_channelgain_list(dev, 1, &insn->chanspec);

	ni_clear_ai_fifo(dev);

	signbits = devpriv->ai_offset[0];
	if (boardtype.reg_type == ni_reg_611x) {
		for (n = 0; n < num_adc_stages_611x; n++) {
			devpriv->stc_writew(dev, AI_CONVERT_Pulse,
					    AI_Command_1_Register);
			udelay(1);
		}
		for (n = 0; n < insn->n; n++) {
			devpriv->stc_writew(dev, AI_CONVERT_Pulse,
					    AI_Command_1_Register);
			/* The 611x has screwy 32-bit FIFOs. */
			d = 0;
			for (i = 0; i < NI_TIMEOUT; i++) {
				if (ni_readb(XXX_Status) & 0x80) {
					d = (ni_readl(ADC_FIFO_Data_611x) >> 16)
					    & 0xffff;
					break;
				}
				if (!(devpriv->stc_readw(dev,
							 AI_Status_1_Register) &
				      AI_FIFO_Empty_St)) {
					d = ni_readl(ADC_FIFO_Data_611x) &
					    0xffff;
					break;
				}
			}
			if (i == NI_TIMEOUT) {
				printk
				    ("ni_mio_common: timeout in 611x ni_ai_insn_read\n");
				return -ETIME;
			}
			d += signbits;
			data[n] = d;
		}
	} else if (boardtype.reg_type == ni_reg_6143) {
		for (n = 0; n < insn->n; n++) {
			devpriv->stc_writew(dev, AI_CONVERT_Pulse,
					    AI_Command_1_Register);

			/* The 6143 has 32-bit FIFOs. You need to strobe a bit to move a single 16bit stranded sample into the FIFO */
			dl = 0;
			for (i = 0; i < NI_TIMEOUT; i++) {
				if (ni_readl(AIFIFO_Status_6143) & 0x01) {
					ni_writel(0x01, AIFIFO_Control_6143);	/*  Get stranded sample into FIFO */
					dl = ni_readl(AIFIFO_Data_6143);
					break;
				}
			}
			if (i == NI_TIMEOUT) {
				printk
				    ("ni_mio_common: timeout in 6143 ni_ai_insn_read\n");
				return -ETIME;
			}
			data[n] = (((dl >> 16) & 0xFFFF) + signbits) & 0xFFFF;
		}
	} else {
		for (n = 0; n < insn->n; n++) {
			devpriv->stc_writew(dev, AI_CONVERT_Pulse,
					    AI_Command_1_Register);
			for (i = 0; i < NI_TIMEOUT; i++) {
				if (!(devpriv->stc_readw(dev,
							 AI_Status_1_Register) &
				      AI_FIFO_Empty_St))
					break;
			}
			if (i == NI_TIMEOUT) {
				printk
				    ("ni_mio_common: timeout in ni_ai_insn_read\n");
				return -ETIME;
			}
			if (boardtype.reg_type & ni_reg_m_series_mask) {
				data[n] =
				    ni_readl(M_Offset_AI_FIFO_Data) & mask;
			} else {
				d = ni_readw(ADC_FIFO_Data_Register);
				d += signbits;	/* subtle: needs to be short addition */
				data[n] = d;
			}
		}
	}
	return insn->n;
}

void ni_prime_channelgain_list(struct comedi_device *dev)
{
	int i;
	devpriv->stc_writew(dev, AI_CONVERT_Pulse, AI_Command_1_Register);
	for (i = 0; i < NI_TIMEOUT; ++i) {
		if (!(devpriv->stc_readw(dev,
					 AI_Status_1_Register) &
		      AI_FIFO_Empty_St)) {
			devpriv->stc_writew(dev, 1, ADC_FIFO_Clear);
			return;
		}
		udelay(1);
	}
	printk("ni_mio_common: timeout loading channel/gain list\n");
}

static void ni_m_series_load_channelgain_list(struct comedi_device *dev,
					      unsigned int n_chan,
					      unsigned int *list)
{
	unsigned int chan, range, aref;
	unsigned int i;
	unsigned offset;
	unsigned int dither;
	unsigned range_code;

	devpriv->stc_writew(dev, 1, Configuration_Memory_Clear);

/* offset = 1 << (boardtype.adbits - 1); */
	if ((list[0] & CR_ALT_SOURCE)) {
		unsigned bypass_bits;
		chan = CR_CHAN(list[0]);
		range = CR_RANGE(list[0]);
		range_code = ni_gainlkup[boardtype.gainlkup][range];
		dither = ((list[0] & CR_ALT_FILTER) != 0);
		bypass_bits = MSeries_AI_Bypass_Config_FIFO_Bit;
		bypass_bits |= chan;
		bypass_bits |=
		    (devpriv->ai_calib_source) &
		    (MSeries_AI_Bypass_Cal_Sel_Pos_Mask |
		     MSeries_AI_Bypass_Cal_Sel_Neg_Mask |
		     MSeries_AI_Bypass_Mode_Mux_Mask |
		     MSeries_AO_Bypass_AO_Cal_Sel_Mask);
		bypass_bits |= MSeries_AI_Bypass_Gain_Bits(range_code);
		if (dither)
			bypass_bits |= MSeries_AI_Bypass_Dither_Bit;
		/*  don't use 2's complement encoding */
		bypass_bits |= MSeries_AI_Bypass_Polarity_Bit;
		ni_writel(bypass_bits, M_Offset_AI_Config_FIFO_Bypass);
	} else {
		ni_writel(0, M_Offset_AI_Config_FIFO_Bypass);
	}
	offset = 0;
	for (i = 0; i < n_chan; i++) {
		unsigned config_bits = 0;
		chan = CR_CHAN(list[i]);
		aref = CR_AREF(list[i]);
		range = CR_RANGE(list[i]);
		dither = ((list[i] & CR_ALT_FILTER) != 0);

		range_code = ni_gainlkup[boardtype.gainlkup][range];
		devpriv->ai_offset[i] = offset;
		switch (aref) {
		case AREF_DIFF:
			config_bits |=
			    MSeries_AI_Config_Channel_Type_Differential_Bits;
			break;
		case AREF_COMMON:
			config_bits |=
			    MSeries_AI_Config_Channel_Type_Common_Ref_Bits;
			break;
		case AREF_GROUND:
			config_bits |=
			    MSeries_AI_Config_Channel_Type_Ground_Ref_Bits;
			break;
		case AREF_OTHER:
			break;
		}
		config_bits |= MSeries_AI_Config_Channel_Bits(chan);
		config_bits |=
		    MSeries_AI_Config_Bank_Bits(boardtype.reg_type, chan);
		config_bits |= MSeries_AI_Config_Gain_Bits(range_code);
		if (i == n_chan - 1)
			config_bits |= MSeries_AI_Config_Last_Channel_Bit;
		if (dither)
			config_bits |= MSeries_AI_Config_Dither_Bit;
		/*  don't use 2's complement encoding */
		config_bits |= MSeries_AI_Config_Polarity_Bit;
		ni_writew(config_bits, M_Offset_AI_Config_FIFO_Data);
	}
	ni_prime_channelgain_list(dev);
}

/*
 * Notes on the 6110 and 6111:
 * These boards a slightly different than the rest of the series, since
 * they have multiple A/D converters.
 * From the driver side, the configuration memory is a
 * little different.
 * Configuration Memory Low:
 *   bits 15-9: same
 *   bit 8: unipolar/bipolar (should be 0 for bipolar)
 *   bits 0-3: gain.  This is 4 bits instead of 3 for the other boards
 *       1001 gain=0.1 (+/- 50)
 *       1010 0.2
 *       1011 0.1
 *       0001 1
 *       0010 2
 *       0011 5
 *       0100 10
 *       0101 20
 *       0110 50
 * Configuration Memory High:
 *   bits 12-14: Channel Type
 *       001 for differential
 *       000 for calibration
 *   bit 11: coupling  (this is not currently handled)
 *       1 AC coupling
 *       0 DC coupling
 *   bits 0-2: channel
 *       valid channels are 0-3
 */
static void ni_load_channelgain_list(struct comedi_device *dev,
				     unsigned int n_chan, unsigned int *list)
{
	unsigned int chan, range, aref;
	unsigned int i;
	unsigned int hi, lo;
	unsigned offset;
	unsigned int dither;

	if (boardtype.reg_type & ni_reg_m_series_mask) {
		ni_m_series_load_channelgain_list(dev, n_chan, list);
		return;
	}
	if (n_chan == 1 && (boardtype.reg_type != ni_reg_611x)
	    && (boardtype.reg_type != ni_reg_6143)) {
		if (devpriv->changain_state
		    && devpriv->changain_spec == list[0]) {
			/*  ready to go. */
			return;
		}
		devpriv->changain_state = 1;
		devpriv->changain_spec = list[0];
	} else {
		devpriv->changain_state = 0;
	}

	devpriv->stc_writew(dev, 1, Configuration_Memory_Clear);

	/*  Set up Calibration mode if required */
	if (boardtype.reg_type == ni_reg_6143) {
		if ((list[0] & CR_ALT_SOURCE)
		    && !devpriv->ai_calib_source_enabled) {
			/*  Strobe Relay enable bit */
			ni_writew(devpriv->ai_calib_source |
				  Calibration_Channel_6143_RelayOn,
				  Calibration_Channel_6143);
			ni_writew(devpriv->ai_calib_source,
				  Calibration_Channel_6143);
			devpriv->ai_calib_source_enabled = 1;
			msleep_interruptible(100);	/*  Allow relays to change */
		} else if (!(list[0] & CR_ALT_SOURCE)
			   && devpriv->ai_calib_source_enabled) {
			/*  Strobe Relay disable bit */
			ni_writew(devpriv->ai_calib_source |
				  Calibration_Channel_6143_RelayOff,
				  Calibration_Channel_6143);
			ni_writew(devpriv->ai_calib_source,
				  Calibration_Channel_6143);
			devpriv->ai_calib_source_enabled = 0;
			msleep_interruptible(100);	/*  Allow relays to change */
		}
	}

	offset = 1 << (boardtype.adbits - 1);
	for (i = 0; i < n_chan; i++) {
		if ((boardtype.reg_type != ni_reg_6143)
		    && (list[i] & CR_ALT_SOURCE)) {
			chan = devpriv->ai_calib_source;
		} else {
			chan = CR_CHAN(list[i]);
		}
		aref = CR_AREF(list[i]);
		range = CR_RANGE(list[i]);
		dither = ((list[i] & CR_ALT_FILTER) != 0);

		/* fix the external/internal range differences */
		range = ni_gainlkup[boardtype.gainlkup][range];
		if (boardtype.reg_type == ni_reg_611x)
			devpriv->ai_offset[i] = offset;
		else
			devpriv->ai_offset[i] = (range & 0x100) ? 0 : offset;

		hi = 0;
		if ((list[i] & CR_ALT_SOURCE)) {
			if (boardtype.reg_type == ni_reg_611x)
				ni_writew(CR_CHAN(list[i]) & 0x0003,
					  Calibration_Channel_Select_611x);
		} else {
			if (boardtype.reg_type == ni_reg_611x)
				aref = AREF_DIFF;
			else if (boardtype.reg_type == ni_reg_6143)
				aref = AREF_OTHER;
			switch (aref) {
			case AREF_DIFF:
				hi |= AI_DIFFERENTIAL;
				break;
			case AREF_COMMON:
				hi |= AI_COMMON;
				break;
			case AREF_GROUND:
				hi |= AI_GROUND;
				break;
			case AREF_OTHER:
				break;
			}
		}
		hi |= AI_CONFIG_CHANNEL(chan);

		ni_writew(hi, Configuration_Memory_High);

		if (boardtype.reg_type != ni_reg_6143) {
			lo = range;
			if (i == n_chan - 1)
				lo |= AI_LAST_CHANNEL;
			if (dither)
				lo |= AI_DITHER;

			ni_writew(lo, Configuration_Memory_Low);
		}
	}

	/* prime the channel/gain list */
	if ((boardtype.reg_type != ni_reg_611x)
	    && (boardtype.reg_type != ni_reg_6143)) {
		ni_prime_channelgain_list(dev);
	}
}

static int ni_ns_to_timer(const struct comedi_device *dev, unsigned nanosec,
			  int round_mode)
{
	int divider;
	switch (round_mode) {
	case TRIG_ROUND_NEAREST:
	default:
		divider = (nanosec + devpriv->clock_ns / 2) / devpriv->clock_ns;
		break;
	case TRIG_ROUND_DOWN:
		divider = (nanosec) / devpriv->clock_ns;
		break;
	case TRIG_ROUND_UP:
		divider = (nanosec + devpriv->clock_ns - 1) / devpriv->clock_ns;
		break;
	}
	return divider - 1;
}

static unsigned ni_timer_to_ns(const struct comedi_device *dev, int timer)
{
	return devpriv->clock_ns * (timer + 1);
}

static unsigned ni_min_ai_scan_period_ns(struct comedi_device *dev,
					 unsigned num_channels)
{
	switch (boardtype.reg_type) {
	case ni_reg_611x:
	case ni_reg_6143:
		/*  simultaneously-sampled inputs */
		return boardtype.ai_speed;
		break;
	default:
		/*  multiplexed inputs */
		break;
	};
	return boardtype.ai_speed * num_channels;
}

static int ni_ai_cmdtest(struct comedi_device *dev, struct comedi_subdevice *s,
			 struct comedi_cmd *cmd)
{
	int err = 0;
	int tmp;
	int sources;

	/* step 1: make sure trigger sources are trivially valid */

	if ((cmd->flags & CMDF_WRITE)) {
		cmd->flags &= ~CMDF_WRITE;
	}

	tmp = cmd->start_src;
	cmd->start_src &= TRIG_NOW | TRIG_INT | TRIG_EXT;
	if (!cmd->start_src || tmp != cmd->start_src)
		err++;

	tmp = cmd->scan_begin_src;
	cmd->scan_begin_src &= TRIG_TIMER | TRIG_EXT;
	if (!cmd->scan_begin_src || tmp != cmd->scan_begin_src)
		err++;

	tmp = cmd->convert_src;
	sources = TRIG_TIMER | TRIG_EXT;
	if ((boardtype.reg_type == ni_reg_611x)
	    || (boardtype.reg_type == ni_reg_6143))
		sources |= TRIG_NOW;
	cmd->convert_src &= sources;
	if (!cmd->convert_src || tmp != cmd->convert_src)
		err++;

	tmp = cmd->scan_end_src;
	cmd->scan_end_src &= TRIG_COUNT;
	if (!cmd->scan_end_src || tmp != cmd->scan_end_src)
		err++;

	tmp = cmd->stop_src;
	cmd->stop_src &= TRIG_COUNT | TRIG_NONE;
	if (!cmd->stop_src || tmp != cmd->stop_src)
		err++;

	if (err)
		return 1;

	/* step 2: make sure trigger sources are unique and mutually compatible */

	/* note that mutual compatiblity is not an issue here */
	if (cmd->start_src != TRIG_NOW &&
	    cmd->start_src != TRIG_INT && cmd->start_src != TRIG_EXT)
		err++;
	if (cmd->scan_begin_src != TRIG_TIMER &&
	    cmd->scan_begin_src != TRIG_EXT &&
	    cmd->scan_begin_src != TRIG_OTHER)
		err++;
	if (cmd->convert_src != TRIG_TIMER &&
	    cmd->convert_src != TRIG_EXT && cmd->convert_src != TRIG_NOW)
		err++;
	if (cmd->stop_src != TRIG_COUNT && cmd->stop_src != TRIG_NONE)
		err++;

	if (err)
		return 2;

	/* step 3: make sure arguments are trivially compatible */

	if (cmd->start_src == TRIG_EXT) {
		/* external trigger */
		unsigned int tmp = CR_CHAN(cmd->start_arg);

		if (tmp > 16)
			tmp = 16;
		tmp |= (cmd->start_arg & (CR_INVERT | CR_EDGE));
		if (cmd->start_arg != tmp) {
			cmd->start_arg = tmp;
			err++;
		}
	} else {
		if (cmd->start_arg != 0) {
			/* true for both TRIG_NOW and TRIG_INT */
			cmd->start_arg = 0;
			err++;
		}
	}
	if (cmd->scan_begin_src == TRIG_TIMER) {
		if (cmd->scan_begin_arg < ni_min_ai_scan_period_ns(dev,
								   cmd->
								   chanlist_len))
		{
			cmd->scan_begin_arg =
			    ni_min_ai_scan_period_ns(dev, cmd->chanlist_len);
			err++;
		}
		if (cmd->scan_begin_arg > devpriv->clock_ns * 0xffffff) {
			cmd->scan_begin_arg = devpriv->clock_ns * 0xffffff;
			err++;
		}
	} else if (cmd->scan_begin_src == TRIG_EXT) {
		/* external trigger */
		unsigned int tmp = CR_CHAN(cmd->scan_begin_arg);

		if (tmp > 16)
			tmp = 16;
		tmp |= (cmd->scan_begin_arg & (CR_INVERT | CR_EDGE));
		if (cmd->scan_begin_arg != tmp) {
			cmd->scan_begin_arg = tmp;
			err++;
		}
	} else {		/* TRIG_OTHER */
		if (cmd->scan_begin_arg) {
			cmd->scan_begin_arg = 0;
			err++;
		}
	}
	if (cmd->convert_src == TRIG_TIMER) {
		if ((boardtype.reg_type == ni_reg_611x)
		    || (boardtype.reg_type == ni_reg_6143)) {
			if (cmd->convert_arg != 0) {
				cmd->convert_arg = 0;
				err++;
			}
		} else {
			if (cmd->convert_arg < boardtype.ai_speed) {
				cmd->convert_arg = boardtype.ai_speed;
				err++;
			}
			if (cmd->convert_arg > devpriv->clock_ns * 0xffff) {
				cmd->convert_arg = devpriv->clock_ns * 0xffff;
				err++;
			}
		}
	} else if (cmd->convert_src == TRIG_EXT) {
		/* external trigger */
		unsigned int tmp = CR_CHAN(cmd->convert_arg);

		if (tmp > 16)
			tmp = 16;
		tmp |= (cmd->convert_arg & (CR_ALT_FILTER | CR_INVERT));
		if (cmd->convert_arg != tmp) {
			cmd->convert_arg = tmp;
			err++;
		}
	} else if (cmd->convert_src == TRIG_NOW) {
		if (cmd->convert_arg != 0) {
			cmd->convert_arg = 0;
			err++;
		}
	}

	if (cmd->scan_end_arg != cmd->chanlist_len) {
		cmd->scan_end_arg = cmd->chanlist_len;
		err++;
	}
	if (cmd->stop_src == TRIG_COUNT) {
		unsigned int max_count = 0x01000000;

		if (boardtype.reg_type == ni_reg_611x)
			max_count -= num_adc_stages_611x;
		if (cmd->stop_arg > max_count) {
			cmd->stop_arg = max_count;
			err++;
		}
		if (cmd->stop_arg < 1) {
			cmd->stop_arg = 1;
			err++;
		}
	} else {
		/* TRIG_NONE */
		if (cmd->stop_arg != 0) {
			cmd->stop_arg = 0;
			err++;
		}
	}

	if (err)
		return 3;

	/* step 4: fix up any arguments */

	if (cmd->scan_begin_src == TRIG_TIMER) {
		tmp = cmd->scan_begin_arg;
		cmd->scan_begin_arg =
		    ni_timer_to_ns(dev, ni_ns_to_timer(dev,
						       cmd->scan_begin_arg,
						       cmd->
						       flags &
						       TRIG_ROUND_MASK));
		if (tmp != cmd->scan_begin_arg)
			err++;
	}
	if (cmd->convert_src == TRIG_TIMER) {
		if ((boardtype.reg_type != ni_reg_611x)
		    && (boardtype.reg_type != ni_reg_6143)) {
			tmp = cmd->convert_arg;
			cmd->convert_arg =
			    ni_timer_to_ns(dev, ni_ns_to_timer(dev,
							       cmd->convert_arg,
							       cmd->
							       flags &
							       TRIG_ROUND_MASK));
			if (tmp != cmd->convert_arg)
				err++;
			if (cmd->scan_begin_src == TRIG_TIMER &&
			    cmd->scan_begin_arg <
			    cmd->convert_arg * cmd->scan_end_arg) {
				cmd->scan_begin_arg =
				    cmd->convert_arg * cmd->scan_end_arg;
				err++;
			}
		}
	}

	if (err)
		return 4;

	return 0;
}

static int ni_ai_cmd(struct comedi_device *dev, struct comedi_subdevice *s)
{
	const struct comedi_cmd *cmd = &s->async->cmd;
	int timer;
	int mode1 = 0;		/* mode1 is needed for both stop and convert */
	int mode2 = 0;
	int start_stop_select = 0;
	unsigned int stop_count;
	int interrupt_a_enable = 0;

	MDPRINTK("ni_ai_cmd\n");
	if (dev->irq == 0) {
		comedi_error(dev, "cannot run command without an irq");
		return -EIO;
	}
	ni_clear_ai_fifo(dev);

	ni_load_channelgain_list(dev, cmd->chanlist_len, cmd->chanlist);

	/* start configuration */
	devpriv->stc_writew(dev, AI_Configuration_Start, Joint_Reset_Register);

	/* disable analog triggering for now, since it
	 * interferes with the use of pfi0 */
	devpriv->an_trig_etc_reg &= ~Analog_Trigger_Enable;
	devpriv->stc_writew(dev, devpriv->an_trig_etc_reg,
			    Analog_Trigger_Etc_Register);

	switch (cmd->start_src) {
	case TRIG_INT:
	case TRIG_NOW:
		devpriv->stc_writew(dev, AI_START2_Select(0) |
				    AI_START1_Sync | AI_START1_Edge |
				    AI_START1_Select(0),
				    AI_Trigger_Select_Register);
		break;
	case TRIG_EXT:
		{
			int chan = CR_CHAN(cmd->start_arg);
			unsigned int bits = AI_START2_Select(0) |
			    AI_START1_Sync | AI_START1_Select(chan + 1);

			if (cmd->start_arg & CR_INVERT)
				bits |= AI_START1_Polarity;
			if (cmd->start_arg & CR_EDGE)
				bits |= AI_START1_Edge;
			devpriv->stc_writew(dev, bits,
					    AI_Trigger_Select_Register);
			break;
		}
	}

	mode2 &= ~AI_Pre_Trigger;
	mode2 &= ~AI_SC_Initial_Load_Source;
	mode2 &= ~AI_SC_Reload_Mode;
	devpriv->stc_writew(dev, mode2, AI_Mode_2_Register);

	if (cmd->chanlist_len == 1 || (boardtype.reg_type == ni_reg_611x)
	    || (boardtype.reg_type == ni_reg_6143)) {
		start_stop_select |= AI_STOP_Polarity;
		start_stop_select |= AI_STOP_Select(31);	/*  logic low */
		start_stop_select |= AI_STOP_Sync;
	} else {
		start_stop_select |= AI_STOP_Select(19);	/*  ai configuration memory */
	}
	devpriv->stc_writew(dev, start_stop_select,
			    AI_START_STOP_Select_Register);

	devpriv->ai_cmd2 = 0;
	switch (cmd->stop_src) {
	case TRIG_COUNT:
		stop_count = cmd->stop_arg - 1;

		if (boardtype.reg_type == ni_reg_611x) {
			/*  have to take 3 stage adc pipeline into account */
			stop_count += num_adc_stages_611x;
		}
		/* stage number of scans */
		devpriv->stc_writel(dev, stop_count, AI_SC_Load_A_Registers);

		mode1 |= AI_Start_Stop | AI_Mode_1_Reserved | AI_Trigger_Once;
		devpriv->stc_writew(dev, mode1, AI_Mode_1_Register);
		/* load SC (Scan Count) */
		devpriv->stc_writew(dev, AI_SC_Load, AI_Command_1_Register);

		devpriv->ai_continuous = 0;
		if (stop_count == 0) {
			devpriv->ai_cmd2 |= AI_End_On_End_Of_Scan;
			interrupt_a_enable |= AI_STOP_Interrupt_Enable;
			/*  this is required to get the last sample for chanlist_len > 1, not sure why */
			if (cmd->chanlist_len > 1)
				start_stop_select |=
				    AI_STOP_Polarity | AI_STOP_Edge;
		}
		break;
	case TRIG_NONE:
		/* stage number of scans */
		devpriv->stc_writel(dev, 0, AI_SC_Load_A_Registers);

		mode1 |= AI_Start_Stop | AI_Mode_1_Reserved | AI_Continuous;
		devpriv->stc_writew(dev, mode1, AI_Mode_1_Register);

		/* load SC (Scan Count) */
		devpriv->stc_writew(dev, AI_SC_Load, AI_Command_1_Register);

		devpriv->ai_continuous = 1;

		break;
	}

	switch (cmd->scan_begin_src) {
	case TRIG_TIMER:
		/*
		   stop bits for non 611x boards
		   AI_SI_Special_Trigger_Delay=0
		   AI_Pre_Trigger=0
		   AI_START_STOP_Select_Register:
		   AI_START_Polarity=0 (?)      rising edge
		   AI_START_Edge=1              edge triggered
		   AI_START_Sync=1 (?)
		   AI_START_Select=0            SI_TC
		   AI_STOP_Polarity=0           rising edge
		   AI_STOP_Edge=0               level
		   AI_STOP_Sync=1
		   AI_STOP_Select=19            external pin (configuration mem)
		 */
		start_stop_select |= AI_START_Edge | AI_START_Sync;
		devpriv->stc_writew(dev, start_stop_select,
				    AI_START_STOP_Select_Register);

		mode2 |= AI_SI_Reload_Mode(0);
		/* AI_SI_Initial_Load_Source=A */
		mode2 &= ~AI_SI_Initial_Load_Source;
		/* mode2 |= AI_SC_Reload_Mode; */
		devpriv->stc_writew(dev, mode2, AI_Mode_2_Register);

		/* load SI */
		timer = ni_ns_to_timer(dev, cmd->scan_begin_arg,
				       TRIG_ROUND_NEAREST);
		devpriv->stc_writel(dev, timer, AI_SI_Load_A_Registers);
		devpriv->stc_writew(dev, AI_SI_Load, AI_Command_1_Register);
		break;
	case TRIG_EXT:
		if (cmd->scan_begin_arg & CR_EDGE)
			start_stop_select |= AI_START_Edge;
		/* AI_START_Polarity==1 is falling edge */
		if (cmd->scan_begin_arg & CR_INVERT)
			start_stop_select |= AI_START_Polarity;
		if (cmd->scan_begin_src != cmd->convert_src ||
		    (cmd->scan_begin_arg & ~CR_EDGE) !=
		    (cmd->convert_arg & ~CR_EDGE))
			start_stop_select |= AI_START_Sync;
		start_stop_select |=
		    AI_START_Select(1 + CR_CHAN(cmd->scan_begin_arg));
		devpriv->stc_writew(dev, start_stop_select,
				    AI_START_STOP_Select_Register);
		break;
	}

	switch (cmd->convert_src) {
	case TRIG_TIMER:
	case TRIG_NOW:
		if (cmd->convert_arg == 0 || cmd->convert_src == TRIG_NOW)
			timer = 1;
		else
			timer = ni_ns_to_timer(dev, cmd->convert_arg,
					       TRIG_ROUND_NEAREST);
		devpriv->stc_writew(dev, 1, AI_SI2_Load_A_Register);	/* 0,0 does not work. */
		devpriv->stc_writew(dev, timer, AI_SI2_Load_B_Register);

		/* AI_SI2_Reload_Mode = alternate */
		/* AI_SI2_Initial_Load_Source = A */
		mode2 &= ~AI_SI2_Initial_Load_Source;
		mode2 |= AI_SI2_Reload_Mode;
		devpriv->stc_writew(dev, mode2, AI_Mode_2_Register);

		/* AI_SI2_Load */
		devpriv->stc_writew(dev, AI_SI2_Load, AI_Command_1_Register);

		mode2 |= AI_SI2_Reload_Mode;	/*  alternate */
		mode2 |= AI_SI2_Initial_Load_Source;	/*  B */

		devpriv->stc_writew(dev, mode2, AI_Mode_2_Register);
		break;
	case TRIG_EXT:
		mode1 |= AI_CONVERT_Source_Select(1 + cmd->convert_arg);
		if ((cmd->convert_arg & CR_INVERT) == 0)
			mode1 |= AI_CONVERT_Source_Polarity;
		devpriv->stc_writew(dev, mode1, AI_Mode_1_Register);

		mode2 |= AI_Start_Stop_Gate_Enable | AI_SC_Gate_Enable;
		devpriv->stc_writew(dev, mode2, AI_Mode_2_Register);

		break;
	}

	if (dev->irq) {

		/* interrupt on FIFO, errors, SC_TC */
		interrupt_a_enable |= AI_Error_Interrupt_Enable |
		    AI_SC_TC_Interrupt_Enable;

#ifndef PCIDMA
		interrupt_a_enable |= AI_FIFO_Interrupt_Enable;
#endif

		if (cmd->flags & TRIG_WAKE_EOS
		    || (devpriv->ai_cmd2 & AI_End_On_End_Of_Scan)) {
			/* wake on end-of-scan */
			devpriv->aimode = AIMODE_SCAN;
		} else {
			devpriv->aimode = AIMODE_HALF_FULL;
		}

		switch (devpriv->aimode) {
		case AIMODE_HALF_FULL:
			/*generate FIFO interrupts and DMA requests on half-full */
#ifdef PCIDMA
			devpriv->stc_writew(dev, AI_FIFO_Mode_HF_to_E,
					    AI_Mode_3_Register);
#else
			devpriv->stc_writew(dev, AI_FIFO_Mode_HF,
					    AI_Mode_3_Register);
#endif
			break;
		case AIMODE_SAMPLE:
			/*generate FIFO interrupts on non-empty */
			devpriv->stc_writew(dev, AI_FIFO_Mode_NE,
					    AI_Mode_3_Register);
			break;
		case AIMODE_SCAN:
#ifdef PCIDMA
			devpriv->stc_writew(dev, AI_FIFO_Mode_NE,
					    AI_Mode_3_Register);
#else
			devpriv->stc_writew(dev, AI_FIFO_Mode_HF,
					    AI_Mode_3_Register);
#endif
			interrupt_a_enable |= AI_STOP_Interrupt_Enable;
			break;
		default:
			break;
		}

		devpriv->stc_writew(dev, AI_Error_Interrupt_Ack | AI_STOP_Interrupt_Ack | AI_START_Interrupt_Ack | AI_START2_Interrupt_Ack | AI_START1_Interrupt_Ack | AI_SC_TC_Interrupt_Ack | AI_SC_TC_Error_Confirm, Interrupt_A_Ack_Register);	/* clear interrupts */

		ni_set_bits(dev, Interrupt_A_Enable_Register,
			    interrupt_a_enable, 1);

		MDPRINTK("Interrupt_A_Enable_Register = 0x%04x\n",
			 devpriv->int_a_enable_reg);
	} else {
		/* interrupt on nothing */
		ni_set_bits(dev, Interrupt_A_Enable_Register, ~0, 0);

		/* XXX start polling if necessary */
		MDPRINTK("interrupting on nothing\n");
	}

	/* end configuration */
	devpriv->stc_writew(dev, AI_Configuration_End, Joint_Reset_Register);

	switch (cmd->scan_begin_src) {
	case TRIG_TIMER:
		devpriv->stc_writew(dev,
				    AI_SI2_Arm | AI_SI_Arm | AI_DIV_Arm |
				    AI_SC_Arm, AI_Command_1_Register);
		break;
	case TRIG_EXT:
		/* XXX AI_SI_Arm? */
		devpriv->stc_writew(dev,
				    AI_SI2_Arm | AI_SI_Arm | AI_DIV_Arm |
				    AI_SC_Arm, AI_Command_1_Register);
		break;
	}

#ifdef PCIDMA
	{
		int retval = ni_ai_setup_MITE_dma(dev);
		if (retval)
			return retval;
	}
	/* mite_dump_regs(devpriv->mite); */
#endif

	switch (cmd->start_src) {
	case TRIG_NOW:
		/* AI_START1_Pulse */
		devpriv->stc_writew(dev, AI_START1_Pulse | devpriv->ai_cmd2,
				    AI_Command_2_Register);
		s->async->inttrig = NULL;
		break;
	case TRIG_EXT:
		s->async->inttrig = NULL;
		break;
	case TRIG_INT:
		s->async->inttrig = &ni_ai_inttrig;
		break;
	}

	MDPRINTK("exit ni_ai_cmd\n");

	return 0;
}

static int ni_ai_inttrig(struct comedi_device *dev, struct comedi_subdevice *s,
			 unsigned int trignum)
{
	if (trignum != 0)
		return -EINVAL;

	devpriv->stc_writew(dev, AI_START1_Pulse | devpriv->ai_cmd2,
			    AI_Command_2_Register);
	s->async->inttrig = NULL;

	return 1;
}

static int ni_ai_config_analog_trig(struct comedi_device *dev,
				    struct comedi_subdevice *s,
				    struct comedi_insn *insn,
				    unsigned int *data);

static int ni_ai_insn_config(struct comedi_device *dev,
			     struct comedi_subdevice *s,
			     struct comedi_insn *insn, unsigned int *data)
{
	if (insn->n < 1)
		return -EINVAL;

	switch (data[0]) {
	case INSN_CONFIG_ANALOG_TRIG:
		return ni_ai_config_analog_trig(dev, s, insn, data);
	case INSN_CONFIG_ALT_SOURCE:
		if (boardtype.reg_type & ni_reg_m_series_mask) {
			if (data[1] & ~(MSeries_AI_Bypass_Cal_Sel_Pos_Mask |
					MSeries_AI_Bypass_Cal_Sel_Neg_Mask |
					MSeries_AI_Bypass_Mode_Mux_Mask |
					MSeries_AO_Bypass_AO_Cal_Sel_Mask)) {
				return -EINVAL;
			}
			devpriv->ai_calib_source = data[1];
		} else if (boardtype.reg_type == ni_reg_6143) {
			unsigned int calib_source;

			calib_source = data[1] & 0xf;

			if (calib_source > 0xF)
				return -EINVAL;

			devpriv->ai_calib_source = calib_source;
			ni_writew(calib_source, Calibration_Channel_6143);
		} else {
			unsigned int calib_source;
			unsigned int calib_source_adjust;

			calib_source = data[1] & 0xf;
			calib_source_adjust = (data[1] >> 4) & 0xff;

			if (calib_source >= 8)
				return -EINVAL;
			devpriv->ai_calib_source = calib_source;
			if (boardtype.reg_type == ni_reg_611x) {
				ni_writeb(calib_source_adjust,
					  Cal_Gain_Select_611x);
			}
		}
		return 2;
	default:
		break;
	}

	return -EINVAL;
}

static int ni_ai_config_analog_trig(struct comedi_device *dev,
				    struct comedi_subdevice *s,
				    struct comedi_insn *insn,
				    unsigned int *data)
{
	unsigned int a, b, modebits;
	int err = 0;

	/* data[1] is flags
	 * data[2] is analog line
	 * data[3] is set level
	 * data[4] is reset level */
	if (!boardtype.has_analog_trig)
		return -EINVAL;
	if ((data[1] & 0xffff0000) != COMEDI_EV_SCAN_BEGIN) {
		data[1] &= (COMEDI_EV_SCAN_BEGIN | 0xffff);
		err++;
	}
	if (data[2] >= boardtype.n_adchan) {
		data[2] = boardtype.n_adchan - 1;
		err++;
	}
	if (data[3] > 255) {	/* a */
		data[3] = 255;
		err++;
	}
	if (data[4] > 255) {	/* b */
		data[4] = 255;
		err++;
	}
	/*
	 * 00 ignore
	 * 01 set
	 * 10 reset
	 *
	 * modes:
	 *   1 level:                    +b-   +a-
	 *     high mode                00 00 01 10
	 *     low mode                 00 00 10 01
	 *   2 level: (a<b)
	 *     hysteresis low mode      10 00 00 01
	 *     hysteresis high mode     01 00 00 10
	 *     middle mode              10 01 01 10
	 */

	a = data[3];
	b = data[4];
	modebits = data[1] & 0xff;
	if (modebits & 0xf0) {
		/* two level mode */
		if (b < a) {
			/* swap order */
			a = data[4];
			b = data[3];
			modebits =
			    ((data[1] & 0xf) << 4) | ((data[1] & 0xf0) >> 4);
		}
		devpriv->atrig_low = a;
		devpriv->atrig_high = b;
		switch (modebits) {
		case 0x81:	/* low hysteresis mode */
			devpriv->atrig_mode = 6;
			break;
		case 0x42:	/* high hysteresis mode */
			devpriv->atrig_mode = 3;
			break;
		case 0x96:	/* middle window mode */
			devpriv->atrig_mode = 2;
			break;
		default:
			data[1] &= ~0xff;
			err++;
		}
	} else {
		/* one level mode */
		if (b != 0) {
			data[4] = 0;
			err++;
		}
		switch (modebits) {
		case 0x06:	/* high window mode */
			devpriv->atrig_high = a;
			devpriv->atrig_mode = 0;
			break;
		case 0x09:	/* low window mode */
			devpriv->atrig_low = a;
			devpriv->atrig_mode = 1;
			break;
		default:
			data[1] &= ~0xff;
			err++;
		}
	}
	if (err)
		return -EAGAIN;
	return 5;
}

/* munge data from unsigned to 2's complement for analog output bipolar modes */
static void ni_ao_munge(struct comedi_device *dev, struct comedi_subdevice *s,
			void *data, unsigned int num_bytes,
			unsigned int chan_index)
{
	struct comedi_async *async = s->async;
	unsigned int range;
	unsigned int i;
	unsigned int offset;
	unsigned int length = num_bytes / sizeof(short);
	short *array = data;

	offset = 1 << (boardtype.aobits - 1);
	for (i = 0; i < length; i++) {
		range = CR_RANGE(async->cmd.chanlist[chan_index]);
		if (boardtype.ao_unipolar == 0 || (range & 1) == 0)
			array[i] -= offset;
#ifdef PCIDMA
		array[i] = cpu_to_le16(array[i]);
#endif
		chan_index++;
		chan_index %= async->cmd.chanlist_len;
	}
}

static int ni_m_series_ao_config_chanlist(struct comedi_device *dev,
					  struct comedi_subdevice *s,
					  unsigned int chanspec[],
					  unsigned int n_chans, int timed)
{
	unsigned int range;
	unsigned int chan;
	unsigned int conf;
	int i;
	int invert = 0;

	if (timed) {
		for (i = 0; i < boardtype.n_aochan; ++i) {
			devpriv->ao_conf[i] &= ~MSeries_AO_Update_Timed_Bit;
			ni_writeb(devpriv->ao_conf[i],
				  M_Offset_AO_Config_Bank(i));
			ni_writeb(0xf, M_Offset_AO_Waveform_Order(i));
		}
	}
	for (i = 0; i < n_chans; i++) {
		const struct comedi_krange *krange;
		chan = CR_CHAN(chanspec[i]);
		range = CR_RANGE(chanspec[i]);
		krange = s->range_table->range + range;
		invert = 0;
		conf = 0;
		switch (krange->max - krange->min) {
		case 20000000:
			conf |= MSeries_AO_DAC_Reference_10V_Internal_Bits;
			ni_writeb(0, M_Offset_AO_Reference_Attenuation(chan));
			break;
		case 10000000:
			conf |= MSeries_AO_DAC_Reference_5V_Internal_Bits;
			ni_writeb(0, M_Offset_AO_Reference_Attenuation(chan));
			break;
		case 4000000:
			conf |= MSeries_AO_DAC_Reference_10V_Internal_Bits;
			ni_writeb(MSeries_Attenuate_x5_Bit,
				  M_Offset_AO_Reference_Attenuation(chan));
			break;
		case 2000000:
			conf |= MSeries_AO_DAC_Reference_5V_Internal_Bits;
			ni_writeb(MSeries_Attenuate_x5_Bit,
				  M_Offset_AO_Reference_Attenuation(chan));
			break;
		default:
			printk("%s: bug! unhandled ao reference voltage\n",
			       __func__);
			break;
		}
		switch (krange->max + krange->min) {
		case 0:
			conf |= MSeries_AO_DAC_Offset_0V_Bits;
			break;
		case 10000000:
			conf |= MSeries_AO_DAC_Offset_5V_Bits;
			break;
		default:
			printk("%s: bug! unhandled ao offset voltage\n",
			       __func__);
			break;
		}
		if (timed)
			conf |= MSeries_AO_Update_Timed_Bit;
		ni_writeb(conf, M_Offset_AO_Config_Bank(chan));
		devpriv->ao_conf[chan] = conf;
		ni_writeb(i, M_Offset_AO_Waveform_Order(chan));
	}
	return invert;
}

static int ni_old_ao_config_chanlist(struct comedi_device *dev,
				     struct comedi_subdevice *s,
				     unsigned int chanspec[],
				     unsigned int n_chans)
{
	unsigned int range;
	unsigned int chan;
	unsigned int conf;
	int i;
	int invert = 0;

	for (i = 0; i < n_chans; i++) {
		chan = CR_CHAN(chanspec[i]);
		range = CR_RANGE(chanspec[i]);
		conf = AO_Channel(chan);

		if (boardtype.ao_unipolar) {
			if ((range & 1) == 0) {
				conf |= AO_Bipolar;
				invert = (1 << (boardtype.aobits - 1));
			} else {
				invert = 0;
			}
			if (range & 2)
				conf |= AO_Ext_Ref;
		} else {
			conf |= AO_Bipolar;
			invert = (1 << (boardtype.aobits - 1));
		}

		/* not all boards can deglitch, but this shouldn't hurt */
		if (chanspec[i] & CR_DEGLITCH)
			conf |= AO_Deglitch;

		/* analog reference */
		/* AREF_OTHER connects AO ground to AI ground, i think */
		conf |= (CR_AREF(chanspec[i]) ==
			 AREF_OTHER) ? AO_Ground_Ref : 0;

		ni_writew(conf, AO_Configuration);
		devpriv->ao_conf[chan] = conf;
	}
	return invert;
}

static int ni_ao_config_chanlist(struct comedi_device *dev,
				 struct comedi_subdevice *s,
				 unsigned int chanspec[], unsigned int n_chans,
				 int timed)
{
	if (boardtype.reg_type & ni_reg_m_series_mask)
		return ni_m_series_ao_config_chanlist(dev, s, chanspec, n_chans,
						      timed);
	else
		return ni_old_ao_config_chanlist(dev, s, chanspec, n_chans);
}

static int ni_ao_insn_read(struct comedi_device *dev,
			   struct comedi_subdevice *s, struct comedi_insn *insn,
			   unsigned int *data)
{
	data[0] = devpriv->ao[CR_CHAN(insn->chanspec)];

	return 1;
}

static int ni_ao_insn_write(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data)
{
	unsigned int chan = CR_CHAN(insn->chanspec);
	unsigned int invert;

	invert = ni_ao_config_chanlist(dev, s, &insn->chanspec, 1, 0);

	devpriv->ao[chan] = data[0];

	if (boardtype.reg_type & ni_reg_m_series_mask) {
		ni_writew(data[0], M_Offset_DAC_Direct_Data(chan));
	} else
		ni_writew(data[0] ^ invert,
			  (chan) ? DAC1_Direct_Data : DAC0_Direct_Data);

	return 1;
}

static int ni_ao_insn_write_671x(struct comedi_device *dev,
				 struct comedi_subdevice *s,
				 struct comedi_insn *insn, unsigned int *data)
{
	unsigned int chan = CR_CHAN(insn->chanspec);
	unsigned int invert;

	ao_win_out(1 << chan, AO_Immediate_671x);
	invert = 1 << (boardtype.aobits - 1);

	ni_ao_config_chanlist(dev, s, &insn->chanspec, 1, 0);

	devpriv->ao[chan] = data[0];
	ao_win_out(data[0] ^ invert, DACx_Direct_Data_671x(chan));

	return 1;
}

static int ni_ao_insn_config(struct comedi_device *dev,
			     struct comedi_subdevice *s,
			     struct comedi_insn *insn, unsigned int *data)
{
	switch (data[0]) {
	case INSN_CONFIG_GET_HARDWARE_BUFFER_SIZE:
		switch (data[1]) {
		case COMEDI_OUTPUT:
			data[2] = 1 + boardtype.ao_fifo_depth * sizeof(short);
			if (devpriv->mite)
				data[2] += devpriv->mite->fifo_size;
			break;
		case COMEDI_INPUT:
			data[2] = 0;
			break;
		default:
			return -EINVAL;
			break;
		}
		return 0;
	default:
		break;
	}

	return -EINVAL;
}

static int ni_ao_inttrig(struct comedi_device *dev, struct comedi_subdevice *s,
			 unsigned int trignum)
{
	int ret;
	int interrupt_b_bits;
	int i;
	static const int timeout = 1000;

	if (trignum != 0)
		return -EINVAL;

	/* Null trig at beginning prevent ao start trigger from executing more than
	   once per command (and doing things like trying to allocate the ao dma channel
	   multiple times) */
	s->async->inttrig = NULL;

	ni_set_bits(dev, Interrupt_B_Enable_Register,
		    AO_FIFO_Interrupt_Enable | AO_Error_Interrupt_Enable, 0);
	interrupt_b_bits = AO_Error_Interrupt_Enable;
#ifdef PCIDMA
	devpriv->stc_writew(dev, 1, DAC_FIFO_Clear);
	if (boardtype.reg_type & ni_reg_6xxx_mask)
		ni_ao_win_outl(dev, 0x6, AO_FIFO_Offset_Load_611x);
	ret = ni_ao_setup_MITE_dma(dev);
	if (ret)
		return ret;
	ret = ni_ao_wait_for_dma_load(dev);
	if (ret < 0)
		return ret;
#else
	ret = ni_ao_prep_fifo(dev, s);
	if (ret == 0)
		return -EPIPE;

	interrupt_b_bits |= AO_FIFO_Interrupt_Enable;
#endif

	devpriv->stc_writew(dev, devpriv->ao_mode3 | AO_Not_An_UPDATE,
			    AO_Mode_3_Register);
	devpriv->stc_writew(dev, devpriv->ao_mode3, AO_Mode_3_Register);
	/* wait for DACs to be loaded */
	for (i = 0; i < timeout; i++) {
		udelay(1);
		if ((devpriv->stc_readw(dev,
					Joint_Status_2_Register) &
		     AO_TMRDACWRs_In_Progress_St) == 0)
			break;
	}
	if (i == timeout) {
		comedi_error(dev,
			     "timed out waiting for AO_TMRDACWRs_In_Progress_St to clear");
		return -EIO;
	}
	/*  stc manual says we are need to clear error interrupt after AO_TMRDACWRs_In_Progress_St clears */
	devpriv->stc_writew(dev, AO_Error_Interrupt_Ack,
			    Interrupt_B_Ack_Register);

	ni_set_bits(dev, Interrupt_B_Enable_Register, interrupt_b_bits, 1);

	devpriv->stc_writew(dev,
			    devpriv->ao_cmd1 | AO_UI_Arm | AO_UC_Arm | AO_BC_Arm
			    | AO_DAC1_Update_Mode | AO_DAC0_Update_Mode,
			    AO_Command_1_Register);

	devpriv->stc_writew(dev, devpriv->ao_cmd2 | AO_START1_Pulse,
			    AO_Command_2_Register);

	return 0;
}

static int ni_ao_cmd(struct comedi_device *dev, struct comedi_subdevice *s)
{
	const struct comedi_cmd *cmd = &s->async->cmd;
	int bits;
	int i;
	unsigned trigvar;

	if (dev->irq == 0) {
		comedi_error(dev, "cannot run command without an irq");
		return -EIO;
	}

	devpriv->stc_writew(dev, AO_Configuration_Start, Joint_Reset_Register);

	devpriv->stc_writew(dev, AO_Disarm, AO_Command_1_Register);

	if (boardtype.reg_type & ni_reg_6xxx_mask) {
		ao_win_out(CLEAR_WG, AO_Misc_611x);

		bits = 0;
		for (i = 0; i < cmd->chanlist_len; i++) {
			int chan;

			chan = CR_CHAN(cmd->chanlist[i]);
			bits |= 1 << chan;
			ao_win_out(chan, AO_Waveform_Generation_611x);
		}
		ao_win_out(bits, AO_Timed_611x);
	}

	ni_ao_config_chanlist(dev, s, cmd->chanlist, cmd->chanlist_len, 1);

	if (cmd->stop_src == TRIG_NONE) {
		devpriv->ao_mode1 |= AO_Continuous;
		devpriv->ao_mode1 &= ~AO_Trigger_Once;
	} else {
		devpriv->ao_mode1 &= ~AO_Continuous;
		devpriv->ao_mode1 |= AO_Trigger_Once;
	}
	devpriv->stc_writew(dev, devpriv->ao_mode1, AO_Mode_1_Register);
	switch (cmd->start_src) {
	case TRIG_INT:
	case TRIG_NOW:
		devpriv->ao_trigger_select &=
		    ~(AO_START1_Polarity | AO_START1_Select(-1));
		devpriv->ao_trigger_select |= AO_START1_Edge | AO_START1_Sync;
		devpriv->stc_writew(dev, devpriv->ao_trigger_select,
				    AO_Trigger_Select_Register);
		break;
	case TRIG_EXT:
		devpriv->ao_trigger_select =
		    AO_START1_Select(CR_CHAN(cmd->start_arg) + 1);
		if (cmd->start_arg & CR_INVERT)
			devpriv->ao_trigger_select |= AO_START1_Polarity;	/*  0=active high, 1=active low. see daq-stc 3-24 (p186) */
		if (cmd->start_arg & CR_EDGE)
			devpriv->ao_trigger_select |= AO_START1_Edge;	/*  0=edge detection disabled, 1=enabled */
		devpriv->stc_writew(dev, devpriv->ao_trigger_select,
				    AO_Trigger_Select_Register);
		break;
	default:
		BUG();
		break;
	}
	devpriv->ao_mode3 &= ~AO_Trigger_Length;
	devpriv->stc_writew(dev, devpriv->ao_mode3, AO_Mode_3_Register);

	devpriv->stc_writew(dev, devpriv->ao_mode1, AO_Mode_1_Register);
	devpriv->ao_mode2 &= ~AO_BC_Initial_Load_Source;
	devpriv->stc_writew(dev, devpriv->ao_mode2, AO_Mode_2_Register);
	if (cmd->stop_src == TRIG_NONE) {
		devpriv->stc_writel(dev, 0xffffff, AO_BC_Load_A_Register);
	} else {
		devpriv->stc_writel(dev, 0, AO_BC_Load_A_Register);
	}
	devpriv->stc_writew(dev, AO_BC_Load, AO_Command_1_Register);
	devpriv->ao_mode2 &= ~AO_UC_Initial_Load_Source;
	devpriv->stc_writew(dev, devpriv->ao_mode2, AO_Mode_2_Register);
	switch (cmd->stop_src) {
	case TRIG_COUNT:
		if (boardtype.reg_type & ni_reg_m_series_mask) {
			/*  this is how the NI example code does it for m-series boards, verified correct with 6259 */
			devpriv->stc_writel(dev, cmd->stop_arg - 1,
					    AO_UC_Load_A_Register);
			devpriv->stc_writew(dev, AO_UC_Load,
					    AO_Command_1_Register);
		} else {
			devpriv->stc_writel(dev, cmd->stop_arg,
					    AO_UC_Load_A_Register);
			devpriv->stc_writew(dev, AO_UC_Load,
					    AO_Command_1_Register);
			devpriv->stc_writel(dev, cmd->stop_arg - 1,
					    AO_UC_Load_A_Register);
		}
		break;
	case TRIG_NONE:
		devpriv->stc_writel(dev, 0xffffff, AO_UC_Load_A_Register);
		devpriv->stc_writew(dev, AO_UC_Load, AO_Command_1_Register);
		devpriv->stc_writel(dev, 0xffffff, AO_UC_Load_A_Register);
		break;
	default:
		devpriv->stc_writel(dev, 0, AO_UC_Load_A_Register);
		devpriv->stc_writew(dev, AO_UC_Load, AO_Command_1_Register);
		devpriv->stc_writel(dev, cmd->stop_arg, AO_UC_Load_A_Register);
	}

	devpriv->ao_mode1 &=
	    ~(AO_UI_Source_Select(0x1f) | AO_UI_Source_Polarity |
	      AO_UPDATE_Source_Select(0x1f) | AO_UPDATE_Source_Polarity);
	switch (cmd->scan_begin_src) {
	case TRIG_TIMER:
		devpriv->ao_cmd2 &= ~AO_BC_Gate_Enable;
		trigvar =
		    ni_ns_to_timer(dev, cmd->scan_begin_arg,
				   TRIG_ROUND_NEAREST);
		devpriv->stc_writel(dev, 1, AO_UI_Load_A_Register);
		devpriv->stc_writew(dev, AO_UI_Load, AO_Command_1_Register);
		devpriv->stc_writel(dev, trigvar, AO_UI_Load_A_Register);
		break;
	case TRIG_EXT:
		devpriv->ao_mode1 |=
		    AO_UPDATE_Source_Select(cmd->scan_begin_arg);
		if (cmd->scan_begin_arg & CR_INVERT)
			devpriv->ao_mode1 |= AO_UPDATE_Source_Polarity;
		devpriv->ao_cmd2 |= AO_BC_Gate_Enable;
		break;
	default:
		BUG();
		break;
	}
	devpriv->stc_writew(dev, devpriv->ao_cmd2, AO_Command_2_Register);
	devpriv->stc_writew(dev, devpriv->ao_mode1, AO_Mode_1_Register);
	devpriv->ao_mode2 &=
	    ~(AO_UI_Reload_Mode(3) | AO_UI_Initial_Load_Source);
	devpriv->stc_writew(dev, devpriv->ao_mode2, AO_Mode_2_Register);

	if (cmd->scan_end_arg > 1) {
		devpriv->ao_mode1 |= AO_Multiple_Channels;
		devpriv->stc_writew(dev,
				    AO_Number_Of_Channels(cmd->scan_end_arg -
							  1) |
				    AO_UPDATE_Output_Select
				    (AO_Update_Output_High_Z),
				    AO_Output_Control_Register);
	} else {
		unsigned bits;
		devpriv->ao_mode1 &= ~AO_Multiple_Channels;
		bits = AO_UPDATE_Output_Select(AO_Update_Output_High_Z);
		if (boardtype.
		    reg_type & (ni_reg_m_series_mask | ni_reg_6xxx_mask)) {
			bits |= AO_Number_Of_Channels(0);
		} else {
			bits |=
			    AO_Number_Of_Channels(CR_CHAN(cmd->chanlist[0]));
		}
		devpriv->stc_writew(dev, bits, AO_Output_Control_Register);
	}
	devpriv->stc_writew(dev, devpriv->ao_mode1, AO_Mode_1_Register);

	devpriv->stc_writew(dev, AO_DAC0_Update_Mode | AO_DAC1_Update_Mode,
			    AO_Command_1_Register);

	devpriv->ao_mode3 |= AO_Stop_On_Overrun_Error;
	devpriv->stc_writew(dev, devpriv->ao_mode3, AO_Mode_3_Register);

	devpriv->ao_mode2 &= ~AO_FIFO_Mode_Mask;
#ifdef PCIDMA
	devpriv->ao_mode2 |= AO_FIFO_Mode_HF_to_F;
#else
	devpriv->ao_mode2 |= AO_FIFO_Mode_HF;
#endif
	devpriv->ao_mode2 &= ~AO_FIFO_Retransmit_Enable;
	devpriv->stc_writew(dev, devpriv->ao_mode2, AO_Mode_2_Register);

	bits = AO_BC_Source_Select | AO_UPDATE_Pulse_Width |
	    AO_TMRDACWR_Pulse_Width;
	if (boardtype.ao_fifo_depth)
		bits |= AO_FIFO_Enable;
	else
		bits |= AO_DMA_PIO_Control;
#if 0
	/* F Hess: windows driver does not set AO_Number_Of_DAC_Packages bit for 6281,
	   verified with bus analyzer. */
	if (boardtype.reg_type & ni_reg_m_series_mask)
		bits |= AO_Number_Of_DAC_Packages;
#endif
	devpriv->stc_writew(dev, bits, AO_Personal_Register);
	/*  enable sending of ao dma requests */
	devpriv->stc_writew(dev, AO_AOFREQ_Enable, AO_Start_Select_Register);

	devpriv->stc_writew(dev, AO_Configuration_End, Joint_Reset_Register);

	if (cmd->stop_src == TRIG_COUNT) {
		devpriv->stc_writew(dev, AO_BC_TC_Interrupt_Ack,
				    Interrupt_B_Ack_Register);
		ni_set_bits(dev, Interrupt_B_Enable_Register,
			    AO_BC_TC_Interrupt_Enable, 1);
	}

	s->async->inttrig = &ni_ao_inttrig;

	return 0;
}

static int ni_ao_cmdtest(struct comedi_device *dev, struct comedi_subdevice *s,
			 struct comedi_cmd *cmd)
{
	int err = 0;
	int tmp;

	/* step 1: make sure trigger sources are trivially valid */

	if ((cmd->flags & CMDF_WRITE) == 0) {
		cmd->flags |= CMDF_WRITE;
	}

	tmp = cmd->start_src;
	cmd->start_src &= TRIG_INT | TRIG_EXT;
	if (!cmd->start_src || tmp != cmd->start_src)
		err++;

	tmp = cmd->scan_begin_src;
	cmd->scan_begin_src &= TRIG_TIMER | TRIG_EXT;
	if (!cmd->scan_begin_src || tmp != cmd->scan_begin_src)
		err++;

	tmp = cmd->convert_src;
	cmd->convert_src &= TRIG_NOW;
	if (!cmd->convert_src || tmp != cmd->convert_src)
		err++;

	tmp = cmd->scan_end_src;
	cmd->scan_end_src &= TRIG_COUNT;
	if (!cmd->scan_end_src || tmp != cmd->scan_end_src)
		err++;

	tmp = cmd->stop_src;
	cmd->stop_src &= TRIG_COUNT | TRIG_NONE;
	if (!cmd->stop_src || tmp != cmd->stop_src)
		err++;

	if (err)
		return 1;

	/* step 2: make sure trigger sources are unique and mutually compatible */

	if (cmd->stop_src != TRIG_COUNT && cmd->stop_src != TRIG_NONE)
		err++;

	if (err)
		return 2;

	/* step 3: make sure arguments are trivially compatible */

	if (cmd->start_src == TRIG_EXT) {
		/* external trigger */
		unsigned int tmp = CR_CHAN(cmd->start_arg);

		if (tmp > 18)
			tmp = 18;
		tmp |= (cmd->start_arg & (CR_INVERT | CR_EDGE));
		if (cmd->start_arg != tmp) {
			cmd->start_arg = tmp;
			err++;
		}
	} else {
		if (cmd->start_arg != 0) {
			/* true for both TRIG_NOW and TRIG_INT */
			cmd->start_arg = 0;
			err++;
		}
	}
	if (cmd->scan_begin_src == TRIG_TIMER) {
		if (cmd->scan_begin_arg < boardtype.ao_speed) {
			cmd->scan_begin_arg = boardtype.ao_speed;
			err++;
		}
		if (cmd->scan_begin_arg > devpriv->clock_ns * 0xffffff) {	/* XXX check */
			cmd->scan_begin_arg = devpriv->clock_ns * 0xffffff;
			err++;
		}
	}
	if (cmd->convert_arg != 0) {
		cmd->convert_arg = 0;
		err++;
	}
	if (cmd->scan_end_arg != cmd->chanlist_len) {
		cmd->scan_end_arg = cmd->chanlist_len;
		err++;
	}
	if (cmd->stop_src == TRIG_COUNT) {	/* XXX check */
		if (cmd->stop_arg > 0x00ffffff) {
			cmd->stop_arg = 0x00ffffff;
			err++;
		}
	} else {
		/* TRIG_NONE */
		if (cmd->stop_arg != 0) {
			cmd->stop_arg = 0;
			err++;
		}
	}

	if (err)
		return 3;

	/* step 4: fix up any arguments */
	if (cmd->scan_begin_src == TRIG_TIMER) {
		tmp = cmd->scan_begin_arg;
		cmd->scan_begin_arg =
		    ni_timer_to_ns(dev, ni_ns_to_timer(dev,
						       cmd->scan_begin_arg,
						       cmd->
						       flags &
						       TRIG_ROUND_MASK));
		if (tmp != cmd->scan_begin_arg)
			err++;
	}
	if (err)
		return 4;

	/* step 5: fix up chanlist */

	if (err)
		return 5;

	return 0;
}

static int ni_ao_reset(struct comedi_device *dev, struct comedi_subdevice *s)
{
	/* devpriv->ao0p=0x0000; */
	/* ni_writew(devpriv->ao0p,AO_Configuration); */

	/* devpriv->ao1p=AO_Channel(1); */
	/* ni_writew(devpriv->ao1p,AO_Configuration); */

	ni_release_ao_mite_channel(dev);

	devpriv->stc_writew(dev, AO_Configuration_Start, Joint_Reset_Register);
	devpriv->stc_writew(dev, AO_Disarm, AO_Command_1_Register);
	ni_set_bits(dev, Interrupt_B_Enable_Register, ~0, 0);
	devpriv->stc_writew(dev, AO_BC_Source_Select, AO_Personal_Register);
	devpriv->stc_writew(dev, 0x3f98, Interrupt_B_Ack_Register);
	devpriv->stc_writew(dev, AO_BC_Source_Select | AO_UPDATE_Pulse_Width |
			    AO_TMRDACWR_Pulse_Width, AO_Personal_Register);
	devpriv->stc_writew(dev, 0, AO_Output_Control_Register);
	devpriv->stc_writew(dev, 0, AO_Start_Select_Register);
	devpriv->ao_cmd1 = 0;
	devpriv->stc_writew(dev, devpriv->ao_cmd1, AO_Command_1_Register);
	devpriv->ao_cmd2 = 0;
	devpriv->stc_writew(dev, devpriv->ao_cmd2, AO_Command_2_Register);
	devpriv->ao_mode1 = 0;
	devpriv->stc_writew(dev, devpriv->ao_mode1, AO_Mode_1_Register);
	devpriv->ao_mode2 = 0;
	devpriv->stc_writew(dev, devpriv->ao_mode2, AO_Mode_2_Register);
	if (boardtype.reg_type & ni_reg_m_series_mask)
		devpriv->ao_mode3 = AO_Last_Gate_Disable;
	else
		devpriv->ao_mode3 = 0;
	devpriv->stc_writew(dev, devpriv->ao_mode3, AO_Mode_3_Register);
	devpriv->ao_trigger_select = 0;
	devpriv->stc_writew(dev, devpriv->ao_trigger_select,
			    AO_Trigger_Select_Register);
	if (boardtype.reg_type & ni_reg_6xxx_mask) {
		unsigned immediate_bits = 0;
		unsigned i;
		for (i = 0; i < s->n_chan; ++i) {
			immediate_bits |= 1 << i;
		}
		ao_win_out(immediate_bits, AO_Immediate_671x);
		ao_win_out(CLEAR_WG, AO_Misc_611x);
	}
	devpriv->stc_writew(dev, AO_Configuration_End, Joint_Reset_Register);

	return 0;
}

/* digital io */

static int ni_dio_insn_config(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_insn *insn, unsigned int *data)
{
#ifdef DEBUG_DIO
	printk("ni_dio_insn_config() chan=%d io=%d\n",
	       CR_CHAN(insn->chanspec), data[0]);
#endif
	switch (data[0]) {
	case INSN_CONFIG_DIO_OUTPUT:
		s->io_bits |= 1 << CR_CHAN(insn->chanspec);
		break;
	case INSN_CONFIG_DIO_INPUT:
		s->io_bits &= ~(1 << CR_CHAN(insn->chanspec));
		break;
	case INSN_CONFIG_DIO_QUERY:
		data[1] =
		    (s->
		     io_bits & (1 << CR_CHAN(insn->chanspec))) ? COMEDI_OUTPUT :
		    COMEDI_INPUT;
		return insn->n;
		break;
	default:
		return -EINVAL;
	}

	devpriv->dio_control &= ~DIO_Pins_Dir_Mask;
	devpriv->dio_control |= DIO_Pins_Dir(s->io_bits);
	devpriv->stc_writew(dev, devpriv->dio_control, DIO_Control_Register);

	return 1;
}

static int ni_dio_insn_bits(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data)
{
#ifdef DEBUG_DIO
	printk("ni_dio_insn_bits() mask=0x%x bits=0x%x\n", data[0], data[1]);
#endif
	if (insn->n != 2)
		return -EINVAL;
	if (data[0]) {
		/* Perform check to make sure we're not using the
		   serial part of the dio */
		if ((data[0] & (DIO_SDIN | DIO_SDOUT))
		    && devpriv->serial_interval_ns)
			return -EBUSY;

		s->state &= ~data[0];
		s->state |= (data[0] & data[1]);
		devpriv->dio_output &= ~DIO_Parallel_Data_Mask;
		devpriv->dio_output |= DIO_Parallel_Data_Out(s->state);
		devpriv->stc_writew(dev, devpriv->dio_output,
				    DIO_Output_Register);
	}
	data[1] = devpriv->stc_readw(dev, DIO_Parallel_Input_Register);

	return 2;
}

static int ni_m_series_dio_insn_config(struct comedi_device *dev,
				       struct comedi_subdevice *s,
				       struct comedi_insn *insn,
				       unsigned int *data)
{
#ifdef DEBUG_DIO
	printk("ni_m_series_dio_insn_config() chan=%d io=%d\n",
	       CR_CHAN(insn->chanspec), data[0]);
#endif
	switch (data[0]) {
	case INSN_CONFIG_DIO_OUTPUT:
		s->io_bits |= 1 << CR_CHAN(insn->chanspec);
		break;
	case INSN_CONFIG_DIO_INPUT:
		s->io_bits &= ~(1 << CR_CHAN(insn->chanspec));
		break;
	case INSN_CONFIG_DIO_QUERY:
		data[1] =
		    (s->
		     io_bits & (1 << CR_CHAN(insn->chanspec))) ? COMEDI_OUTPUT :
		    COMEDI_INPUT;
		return insn->n;
		break;
	default:
		return -EINVAL;
	}

	ni_writel(s->io_bits, M_Offset_DIO_Direction);

	return 1;
}

static int ni_m_series_dio_insn_bits(struct comedi_device *dev,
				     struct comedi_subdevice *s,
				     struct comedi_insn *insn,
				     unsigned int *data)
{
#ifdef DEBUG_DIO
	printk("ni_m_series_dio_insn_bits() mask=0x%x bits=0x%x\n", data[0],
	       data[1]);
#endif
	if (insn->n != 2)
		return -EINVAL;
	if (data[0]) {
		s->state &= ~data[0];
		s->state |= (data[0] & data[1]);
		ni_writel(s->state, M_Offset_Static_Digital_Output);
	}
	data[1] = ni_readl(M_Offset_Static_Digital_Input);

	return 2;
}

static int ni_cdio_cmdtest(struct comedi_device *dev,
			   struct comedi_subdevice *s, struct comedi_cmd *cmd)
{
	int err = 0;
	int tmp;
	int sources;
	unsigned i;

	/* step 1: make sure trigger sources are trivially valid */

	tmp = cmd->start_src;
	sources = TRIG_INT;
	cmd->start_src &= sources;
	if (!cmd->start_src || tmp != cmd->start_src)
		err++;

	tmp = cmd->scan_begin_src;
	cmd->scan_begin_src &= TRIG_EXT;
	if (!cmd->scan_begin_src || tmp != cmd->scan_begin_src)
		err++;

	tmp = cmd->convert_src;
	cmd->convert_src &= TRIG_NOW;
	if (!cmd->convert_src || tmp != cmd->convert_src)
		err++;

	tmp = cmd->scan_end_src;
	cmd->scan_end_src &= TRIG_COUNT;
	if (!cmd->scan_end_src || tmp != cmd->scan_end_src)
		err++;

	tmp = cmd->stop_src;
	cmd->stop_src &= TRIG_NONE;
	if (!cmd->stop_src || tmp != cmd->stop_src)
		err++;

	if (err)
		return 1;

	/* step 2: make sure trigger sources are unique... */

	if (cmd->start_src != TRIG_INT)
		err++;
	if (cmd->scan_begin_src != TRIG_EXT)
		err++;
	if (cmd->convert_src != TRIG_NOW)
		err++;
	if (cmd->stop_src != TRIG_NONE)
		err++;
	/* ... and mutually compatible */

	if (err)
		return 2;

	/* step 3: make sure arguments are trivially compatible */
	if (cmd->start_src == TRIG_INT) {
		if (cmd->start_arg != 0) {
			cmd->start_arg = 0;
			err++;
		}
	}
	if (cmd->scan_begin_src == TRIG_EXT) {
		tmp = cmd->scan_begin_arg;
		tmp &= CR_PACK_FLAGS(CDO_Sample_Source_Select_Mask, 0   c
				 iverCR_INVERT);
		if (tmp != cmd->scan_begin_arg) {di/derr++mmon}
ds

.c
 ware convert_src == TRIG_NOW-STC b COMEDI - Linux CDAQ-STC bavice Interface
  = 0mmonased boards

     COMEDI -driveend97-20ardware chanlist_len-STC beef.org>
    Copyrght (C) 2002-2006 Fmmonsed boar<ds@schleef.ortopControl and MeaNEurement Device ree sopyrigh0   Copyrightt and/or 01 David A. Schleef <ds@schlerr)
		return 3;

	/* step 4: fix up any arguments */al Public License as 4ublished by
5: check  2002-20oundatfor (iterms i <ers.sourceforge.net ++iurement Device I2002-20[i]righiLice
    = 1his programic License as 5ublinse as 0;
}

static int ni_cdio_cmd(struct comedi_device *dev, ied warranty osubf
    MEs)
{
	constNTABILITY or FIcmd *the
= &s->async->cmd;
	unsigned cdo_mode_bits = CDO_FIFO_Me deBit |

   Halt_On_Errorld h;
	ut eretvalublini_writel(
   Resetld h, M_Off aloCDIO_Commandommoswitchhleef.org>
 r for src-STC case and MEXT:k Mo more details|=
driverCR_CHANo the Free SoftwaDAQ-S&mbridge,DO_Sample_Source_Select_/*
 mmonbreaothedefaultc., BUG(ommon file, e    COMEDI -driver for DAQ &s/ni_mio_co., 675 Mass Ave, Cais mePolarityof the blic Licen675 Mass Ave,with this prou shouommo.c
 s->ioetail-STC bblic Licensundeatep://ftp.natinst You Dataommonblic License
 SW_Updatuld hwith this program; if not,  340747b.pdf  nuals):p://ftp.natinst.ask_Enablm/sup} elseank Moanty oeopy (RCHAe thivers"attempted to run digital outpuarranif n with no lines configured asx.pdf
	s"ommonnse as -EIOimio.cGenera =evenrequest_675 Mite_ibutnel//ww/support User m<dify
   nse as General io.cGeneral Puinttri ter&ven to_c.pdf  ;UT ANY WARRANTY; without even tignal ratlied warranty of
    MERCHANTABILITY or FITNESS FOR A w.ni.coicense fout edf  numPART#ifdef PCIDMA License folong flags;
#endife GNU Generaterms odf  about ;TICULAR icense fotimeoutit w00ubli	   320906c.pdf  maxNULLublisheread alloc the entire bufferounds (from buf_outi_e needf  eral , General Pupree nee-07:sz);
 rev P
	   32183spin_lock_irqsave(&devpriv->	   320517f.as ye,ntinua/supportarnaby, Bte)
	   32051-STC b	   3prep_dmaNTERRUPT */
/* #define , 32B */ommon	   3dma_armNTERRUPT */
/* #define D611x registers (from http://www "BUG:x.pdcdo 	   r
   nel?info:

	 de-10 0517c.pdf ted auns yet. Trestory Barnaby, BEAM Ltd.
*/

/* #define DEBUG_INnual (new)
  320889a.pdf  delon of a/*
* XXX not sure whatt aterrupt C group does
*evenc Licb(Ilock_cha_G = 7_C7xx and79b.p
*ith this pable must the ai_g); wait optidma
	refil1x.pdf
	rfifo


	2option) any latemio-16dhis program is ual (adl(th this prograStatus)rigiegister Fullld hpe th file, e	udelay(10ux/scupportirol mio-16drank Molude "8255.h"
#inclkupfails)
	re16] =te.h_gai!info:
ven the iancf.pdf ,  DEBU

	   320517c.pdf blic License
 Armld have reccopy oable must e ai_gaSalong  |mbrid 7},
mptyister gain_611x] = {0x00a, 0x0w.ni ith this program; if not, 20889a.pdf  delNTY; without even the i103, 0ied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTIblic License
 Disa{0, 1, 4, 7},
	[ai_gain_611x] = {0x0Clea of t0b, 0x001, 0x002,
			  0x003, 0x004, 0xnge_ni_E_ai = { 16,  You R(obsoleANGE(-10, 10),
							 RAN006},
	[ai_gain_622x] = {0, 1,onst unsigned old_RTSI_clock_channel = 7;

/*ote: this t0* definitions */
static const shon_16]blic Licen0reference manual
	67xx and 611ual (leaseete)
	   320517f.pdf  Use ANY WARRANTY; withovoid handl0, 5iignalck_cha 7},
	[ai_gain_6143] = {0xPARTIicense for io_AT-Must, wABILITY or FITNESS FOR A  = dev->TNESS FORs + NI_, 0x1UBDEV; rev P
	   321837a.pdf  discontinuation of aupport(boardtype.reg_				 &nual (g_m_series_mask)rol )
	   320889aeletenot
		fully tested as yet. Terry Barnaby, BEAM Ltd.
*/

/* #define DEBUG_INTERRUPT */
/* #define DEBUG_icense for mor   3
					 ambridge strugetuct comerrupt.h>
#include <linux/snt Devnst struct come& CHSR_LINKC-STC bac LicensHOR_CLRLCw.ni.com/p mediaby, BEAM  BEAM Lio_addr +						   RAMITE_(-5,NTERRUPT */
/* #define tributnel)ommon}lude <liral _.pdf
	*/
/* #define DEBUG_STATUS_B I-6143: , 7, 0xINTK
#define MDPRINTK(format, args...)
#endif

/* A timeout 					RANG0.1),
					manual (, 0x102, 0x103, 0x104, 0xsupport0, 2),
					& nse
 Ovck_cnld have recUnderflow7},
	-STC/* printk("0.1) http:: 
				x=0x%x\n", 0.1),
					);p

	2, 0x00, 0x00},

	[ai_gain_611x]Cr po{0, 1,with this program; if notishe unsijust guessing this is needbly nd RANGEsometh				usefulatic cGeneral Puevre Foton
OMEDI_CB_OVERFLOWimio.c.

	I								 0.5), 0x106, 0x002,},
									   RANGE(0,_gai aq/uy\n"static const struct com
							 RANGE(-10, 10),
							 RANw.ni.co),
							 RANGE(-0.5, 0.5upport has ),
								 RANGE(-0EOAtatic },
	[a),
		0x104, 0x1NTY; without even				al_insn_er pos 7},
	[ai_gain_6143] = {0x0di/drit-mio-16e-10 rev. c
	   321808a	     RANGE(-0.2, 1) *, 1),pdf  about at-*develARTIut eat it , 1)->   0icense forhar byte				,ANGE(-interms 0x101, = { 4,righ2e NI_TIMEOU-EINVAerrupwrite to ran[0]    FoundaINSN_CONFIG_SERIAL_CLOCK:s not
		fDEBUGE_ai
				   RANSPI ANGE(- cs ye e_ni_g cd							   1]			   RANG		NGE(-2, 2ANGE(-1hwore dit willGE(-10, 10the iontrol				ograHW_SNGE(-1xx and    UG_INT{
				rol 			    DISABLED-STC baE(-10, 10),
							     RA David5, 5),
							     RA&= ~((-2, 2),
							    0b, 0i/driv, 0x1oftwarx00aGE(-1C     Rommon	, 1),
						     RANGE(-00.2),
							 ANGE(-1, terval_n come{
					   R regisE(-1, 1),
		<143 = { 1600NS-STC ba/* Warning:			 RA_ni_M_speout s too fast
	releliably.ni.com	     RASCXI.atic c,
							     RANGE(-0.1, (-2, 2),
					Timebase0.2),
							 _ni_M_and_f16de|= S		 0able nt(0, 1),
							    }
};

static const s			    Romedi_lOut_Diviouldy_2nge_ni_S_ai_6143 = { 1range			     RANGE(-5, +5),
							     }
};

static const struct comedi_lrange 1_2Ue_ni_E_aANGE_ext(-1, 1),
							    RANGE_ext(0, 1),
							    }
};

static const struct comedi_lrange *cons;

sta  RA6] = &range_ni_E_ai,
	[ai_gain_8] = &range_ni_E__ai_bmited,
	[ai_gain_14] = &range_ni_E_ai_limited14,
	[ai_gain_4] = &range_ni_E_0_bipolar4,
	[ai_gain_611x] = &NGE(-2, 2),
					,
	[ai_gain_622x] = &range_ni_M_ai_622x,
	[ai_gain_628x] = &range_ni_M_ai_628x,
	[ai_gain_6143] = &rang/* Note:ai_628x,
	[ai_gain_6143] =  only affectse_ni_M_600ns/1.2us. If you e as d_ai,
	bnsn *ffni67xxthee_ni_M_slow						, int w6] =st6] =get 10us, exceptmedine_ni_M_all intr {1, 2s ald_Rrong			    RA_ni_S_ai_6143
};
0tatic int ni_dio_insn_config(struct comedi_device *dev,olar4,
	[ai_gain_611x] = &rang0.1),
							     }
};

static const struct comedi_lrange range_ni,
							     RANGE(-0.2, 0.2),
_S_ai_6141, 1),
		/10 r0) *do_indio_interrupt(struct ,
							     }
};

staticRANGnterrupt(stc: thisw0x104,5, 5),
							     Redi/driverogram;    R_Registerommon,
			  unsigned int trignum);

sta
static const edi/driverCstatic coFOUTruct comedi_dense as 1 RANG file, GE(-0.2, 0.2),
				BIDIRECTION   RATA:RANGE(-1,  RANGE(-5, +5),
							    odify
    		     RANGE(-1,vice *sNGE(-10,   }
};

s & 0xFF RANGE(-1,(-10, 10),
							    -STC basedmanual),
							 RANc Lic80x104,  10),
				 struc				   R&0),
					   R const stru  RANGE(-5, +5),
							   >dify
    w_readwrite8(strsct comedi_device *dev,
				   struct comedi_subdevice *s,
				 TC ba		   RAN  RANGE(-1, 1),
					:ge rangedis andd!e_ni_e *s,
				   unsigned chNGE(-1w_refine NIs,
				 errGE(-5_S_ai_6140),
				signed c ni_calib = { 4, {her file, e.g.,
	ni_at,
				   unsigned}
 2),
							     RANGE(-1uct comedi_dev	     RANGE(-0.5, 0.5),
							       RANGE(-0.2, 0.2),
							    .pdf  about   RArite		   struc      struct comee ranevic			 RANGE(0, ut e
					will_M_ai_622x0, count,
	2	   tatic const structruct comedi_subdevuct comedi_de:x.pdf
	t				),
						di_subde			   RANGE(5, 5),
						.pdf
	rin_16] = &rangeLevey anothet comedi_insn *insn,NGE(-2,signed int *Out1, 1)e *s,
	s,
			  unsigned int trignum);

static .pdf
	,sn_coO						uct comedi_	 }
ed incomedi		  unsign come0x104,Jointx104, 0_1ruct comedi_dport/signed &ai_628x,
	[aI	  0_Progress_S04, 0x1w_read-EBUSYe *dgoto copy his pro5, 5),
							     RANGE(-2, 2),
					Starthe ,
			  unsigned int trignum);

static int ni_ssn_config(struct comedi_dANGE_ext(-1, 1),
							    RANGE_ext(0igned nlisheW ni_until STC statiwe're done, but_ins't loop infinLicey			   while (device *dambrt *data);
static int nidi/drivepfi_insn_config(struct chis f		   RA,
			      struct comedi_subdevice/* D1, 2 one bit pert cometic c {1, 2,nsigned char data_out,
				  + 999)_cdo_int					   -- int new)
	   3ead(stre_ni_M_auct comedi_device *dev,
			range rangeI/O didruct_devsh in02, 0di_insn *i *s,
			TIMEsn *iruct comedi_ieef <ds@(struct coptil10),bit. T	 RA{1, 2NGE(absoluice  necessary, because int ,
			      struct comedi_sub gANGEhighcomedi_de-10,eare *dev,
       struct comedi_subdevice *s,
			       struct BUG_INT comedrighinte-STC bct comed int *data);
static int ni_,
			      snedi_insn *insn,tatic const struct comedi_lt comedi_device *dev,
			i)
#eus)
),
						ct comedition of at}

copy :ni_old_get_pfi_routing(struct comedi_device *dev,
				       unsigned ch2),
					_insn2),
							     RANGE(-1
static int ni);

static int ni_eeprom_insn_read(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn,    RANGE(,comedi				   ies_eeprom_insn_read(struct comedi_d
static int n				struct comedi_subdevice *s,
					strucce *dev)optiomedi_debefore transd up

	2_status_a(a)
#endif
#ifdef DEBUG_STATUS_B
static void optioNGE(statx80;dev, _devic >>= 1config(stcomedi currenice *;ignee tTSI_we cangnedtouch1,
	AT-MImbridg
static itr);

evic-TNESS FOR field,),
		e rangeigneddi_c separate d int trignromnt_s			    t comedi_insn *insn,
					unsDOUT					   device * &dev, ipolar4,
	[ai_gain_6i_pfi_insn_bitsic int n*dat
			    struct comedi_subdevice *s,
			    sterial_insn_cocomedi_insn *insn, ui_deAssert SDCLK (active lowstrunux ed),rt ni_gainhalf oRANG  ds to{1, 2gnumadevice *s,


statt ni_gains toothen, uns, struct comedi_subd	     RANGE(-2, struct comedi_lrange rGE(-5, 5),
		signed int trignum);

static int ni_serial_insn_config(struct comedi_			       struct comedi_subdevice *s,
			       2ic void an);

static void ni_rtsi_initbdevice *s,
			     struct comedi_insn *insn, unsigned int *data);
static int ni_gpct_insn_config(struct comedi_device *dev,
			       struct comedi_subdevice *s,
/* a)
#einttrig(struv,
			G_INTERRUPT *ce *s,
			     struct *data);
Paralleb(a)
#endif

statdev,
			DINe NI				 struct comnt ni_I_R:omedi_subdelse
#define ni_mio_print_sti_gpct_cancel(struct comstatic c	ruct c|=deviced_eepromsubdevice *s,
			 unsigned int trignum);
static voiomedi_subdevice *somedief PCIDMA
si_8255_caedi_nt status);
#omediHOUT ANY WARRANTY; witho),
		m  strmmon_detach							 RANGE(0, 0.2),
							 G_INTERs beivatserial_G_INTERRUPT */int erof
 -STC bani_gpctof
    _destroy    struct comedi_subded short comedi_devrange range&&  5),
					has_8255ion(TNESS  unsi_cleanup trignum)_lrange range_ni_E5529__ai_limite2, 2),
						),
		devi_ao_67xx 7},
	[ai_gain_6143] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,ut eiommand(stn) any lates->n32051; i++

	   340ao_win datint ni_AO_C0517f.pi) | 0xmedi/dtatic istatr possiation_2bits), 7, 0xg_select_b(serifig(Laedi_Single_Pfi_in  3410-2, 2),
						icense fo*s,
			 t),
	tatit come(enumunsigned ta);
sta regi_insn *insn,  *data);
stat, write tofig(   FoundaNITIO_G0_Autoincreare _insc.,  *data);
sta = Gce *s,
			      st;
statuct co file, eomedi_subdev1ce *s,
			      struct comedi_insn *insn, unsigned int *data);1
static int ni_set_master0am; if n  struct comedi_insn *insid ack_a_indata);

static int ni_set_master_cid ack_a_interrupt(struct comedi_device *dev, unssigned period_ns);
static vo 2),ave  struct comedi_insn *insenum aimodedata);

static int ni_set_master_cenum aimodes {
	AIMODE_NONE = 0,
	AIMODE_HALF_FULsigned period_ns);
static voSnum aimodes {
	AIMODE_NONE = 0,IMODE_HALF_FULL = 1,
	AIMODE_SCAN = 2,
	AI,
	NI_DIO_SUBDEV,
	NI_8255_DIO_SUBDEV,
	NI_UNUsigned period_ns);
static voshoul struct comedi_insn *insEV,
	NI_data);

static int ni_set_master_cEV,
	NI_SERIAL_SUBDEV,
	NI_RTSI_SUBDEV,
	NIsigned period_ns);
static voLoadA	NI_SERIAL_SUBDEV,
	NI_RTNI_G_PCT_Sdata);

static int ni_set_master_cNI_GPCT_SUBDEV(unsigned counter_index)
{
	switS
};
static inline unsigned NI_GBCT_SUBDEV(unsigned counter_indk;
	d
	switch (counter_index) {
	case 0:
		k;
	default:
		break;
	}
	BUG();
	return Nsigned period_ns);
static voa)
#enuded by struct comedi_insn *ins SERIAL_DISABLEDdata);

static int ni_set_master_c SERIAL_DISABLED		0
#define SERIAL_600NS		600
#define SERIAsigned period_ns);
static v1nsn_confO_SUBDEV,
	NI_8255_DIO_SUstruct co *dev,
	 void handle_a_interruptpfi_in   alo struct comedi_insn *imite_status);
st status,
			       unsigned ai_mite_signed ig(sttatic void handle_b_intersn_config(struct medi_device *dev, unsigned short status2
			       unsigned ao_mite_status);eviceic void get_last_sample_611x(sE(-2.5, 2.5Acknowledg
	NI_SERIAL_SUBDEV,
	NI_

static ininlistatic void get_last_sample_611x(AL_1static inline void ni_set_bitfield(struct comedi_devBce *dev, int reg,
				   unsigned bit0signed shorruct comedi_insn *iAIstatus);
static void get_last_sample_611x(pt(struct comedi_device *dev, uA0x104, 0;
static void get_last_sample_611x(sE(-2.5, 2.5),
				i_set_bitfield(struct comedi_devic
		devpriv int reg,
				   unsigned bit_mask, unsig
		devpriv->stc_writew(dev, devpriv->inB_a_enable_reg,
				    Inter.g.,
	ni_at		   RAN%s: un					 dnfig_insn ),
	struwrite .				mbridge   __func__,nfig(tus,mio.c or  ANY WARRAr ni_pcimio.cnse as _device *dev,
signed int reg_*s,
			 c Licata);
stat}
};

s*s,
			 * comedict comedi_lSTC rdi/driveic int ni_6143_pwm_config(struied warranty of
    MERCHusercomedict comedi_sub->dev {
							  _device *dev,
	/*vprivstruche joia.pdseU Ge_enablewhichic inRANGvaat-moin_reg |Foundunsigic(0, f  about at-
			 >io_143_ aloruct coG0status | Gig(ssethe ister);
		break;
	case AI_A0, 0.5),
_a_e ai_garuct c int *G0_G4107Register);
		bresele0_TC		ni_writeb(devpr&= ~bit_mask;
		devpriv->ai_ao_select_reb |= bit_values & bit_1ask;
		ni_writeb(devpriv->1i_ao_select_reg, AI_AO
			      struct cew(dm-					R-*insntion_pinin_Regns);
static void;
stngBDEV,
	NI_FREte: thisw(STC reference mg, G0_G1_Select)gned short a_status);
static _G1_Select);
		break;
	default:
		printk("War", __func__);
	NI_AI_SUBDEV,
	NI_AO_SUBDEV,econdask;
	
		break;
	default:
		printk("Warni&devpriv->sED_SUBDEV,
	NI_CALIBRATION_Sdevpriv->soft_reg_copy_lock, flags);
}

#ifdtruct comedi_igned period_ns);
static voDMA(structoft_reg_copy_lock, flags);
}

#ifdefel */
stat
static int ni_set_master_cel */
static inline void ni_set_ai_dma_channnnel)
{
	unsigned period_ns);
static voABZoft_reg_copy_lock, flags);
}

#ifdefMomedesnnel
static int ni_set_master_clel_select_bitfield(channel) <<
		   1 AI_DMA_Select_Shift) & it_mas32di_de(devpriv->g0_g1_select_reg, NI_GPCT_SUBDdex) {
	case 0:
		return NCT1_SUBDEV;
		break;
	defa;
}

enum timebase_nanoseconds {
	TIMEBASEnsigned int *data);
statv->stc_w,
			  unsigned in 0x104,STC re *data);
sta(dev, AI_AO_Select16AI_DMA_Select_Mask, bitfield);
}
Register);
		break;
	casst sONlt:
	 & ~ai_ao_select_reg |= bit_valu101, 0x1 alobitnum);0x104,evpriv->int_a_enable_reg,
	->io_bai_ao_select_reg |= bit_valu0) {
	_1_2US		1200
#define SERIAL_1    AO_DMA_Select_Shift) & AO_DMA_Select_Mask;
	} el
		devpriv->gld = 0;
	}
	ni_set_bitfield(dev, AI_ster:
		devpriv->_Select_Mask, bitfiel
		devpriv->gative mite_channel means no chaai_mite_status);
statit) & AO_DMA_Select_MO_Select:
		devpstc_w/* fall-throughv,
		.g.,
	ni_atmedi_device *dev, int channel)
{
	unsigned bitfield;

	if (channeint ni_ {
		bitfield =
		    }comedi_insn *insn, unsigned S-Serk;
	case IO_Bidirection_Pin_Regisd);
static rection_pin_reg &= ~bit_mask;
		devpriv->io_bidirection_pin_reg |= bit_values & bit_mask;
		devpriv->stc_writebit_values & bit_mask;
		ni_ *insn(devpriv->g0_g1_select_reg, el *lock, flags);
nse as 	   RANwx102, 0x10Select_Mask;
medi_device *dev, int channel)
Mask;
	if (mite_channel >= 0) {
		/*XXX ct_bitfield((dev, AI_AO_Select, AI_DMA_Select_Mask, bitfield);
}
enum aimodes {_SCAN = 2,
	AIMODE_SAMPLE = 
	NI_AO_SUBDEV,
	NI_DIO_SUBDI_CALIBRATION_SUBDEV,
	NI_EEPROM_SUBDEV,
	NIv, int channel)
{
	unsigned bitfnse as 
static void hand 0x104, tfield =
		    (ni_stc_dma_channel_select_bitfield}
	ni_set_bitfield(dev, G0_G1_Select, GPCT_DMA_Select_Mask<
		     CDO_DMA_Select_int ni_& CDO_DMA_Select_Mask;
	able_reg,
	RRANTY; without evenfreq dat1, 1), RAN		     RANGE(-0.5, 0.5),
							     RANGE(-0.2, 0.2),
							     }
};

static const struct comedi_lrange range_ni		      int *data);p[] = {
	[ai_gai *insnE_ai,
r_valu 2),
					1 int ni_request_ai_mite_channel(sc Licuct comedi_subdevice *s);
#endif
s(struct comedi_device *dev,
			   &devpriv->mite_channel_lock, flags);
	BUG_ON(devprange_lkup[] = {
	[ai_gain_1*insng, AI_AO_vice *dev,
				 struct comedi_subdevice *s,
				 strut comedi_insn *insn, unsigned ierror(dev,
			     "failed to resn =
	    mite_r    }
};

static const stru_channel(dev			     Rpriv->ai_mite_chan->channel);
	spinerve mite dma channel for analog input.");
		return -EBUSY;
	}
	devpriv->ai_mite_chan->dir = ev,
			       st2),
							     RANt_mite_chan_ni_Muct comedi_subdevice *s);
#endif
n *insn, unsi
statise incask;
	rite to han);
	devprct comedi_s_FREQ_insn);
sBASE_1_DIV_2  }
};_SRCc., error(dev,
			     "failed to res, 1),
		AL_DISAtus,
			       unsi_channel(devpriv->mevpriv->ao_mite_ring);
	if (devpriv->ao);
	spinn == NULL) {
		spin_unlock__subdevice *s,
			       structurn 0;
}

static int ni_request_ao_mite_channel(struct comedi_device *dev)
{
	unsigned long3rupt_B_Enable_Registe(&devpriv->mite_channel_lock, flags);
	BUG_ON(devpt comedi_lrangechan =
	    priv->mite_channel_lock, flaperiod_ni_devic_INTERRUPT */	devpriv->ai_mite_chan == NULL) {
		_b(int chan =
	    he itore(&devpriv->mite_channel_locv->int ni_request_gp = iv->mite_chlimitx registers_index,
					enum comedi_io_directionite, devpriv->ao_
{
	unsigned long flags;
	struct1_NS * _gai_channel meaest_ai_mite_channel(s
							     RANGE(-0.5, 0.5),
							 ad(struct comedi_device *dev,
			     }
};

static const struct comedi_lrange range_ni),
							     RANGE(-0.2, 0.2),
						Tvpriv->ao_mitee_channel e(&devpriv->mite_chtrignu{
							s,
			       un, 0.2),
				Glock_irqrestore(te_chan->channel);
	spx104,&);
		co");
		re2omedi_nse as pub dma channel, flags);
}

statiRANGE(-1,NTY; without evenn addeice *dent cs5529_ai_insn_read(struct coNU Geno_fifo_ =ne needev->counx104, izeof IO_Bidirecice *devut count */tic int 20889a.pdf |= bed as yet.nit Barnaby, Bwindowas yeot, wpriv->mite_channel_lock,sof143_p_copy);
	return 0;
}

#endif /*  PCIDMAEAM Ltd.
*/

/* #ao_fifo_halfRRAN;TY; without evenEte_chaied warranty of
    MERCHANTABILITY or FIdever pos *,
	[k;
		devpriv->io_TNESS FOR A  {
							  j;
	ic int ni_614vari_Bid comedi_chan =
oid ni_m 5),
					n_aoibut > MAX_N_statHAN-STC b		   RANbug!9_DEBUG
sta>mite, devpriv->cdo_mitei_insn **s,
			       structite_cdma_chrange rangev, gpNI_NUM_limiteICES)_unlock_irqrest-ENOMEMdevice analogtruct cTNESS FOR Andat comedi_lrange range_ni_E
	caimited e_ri->S-SerTNESS  = ite_annel(devpriv->midine DEBUG_s->ANGE(			 RANGEurn _AIedi_t coint cstinuaedi_lrangSDF_READGE(- |n->chDIFF	spin_unlTHER	spin_uCMDhanne					    5),
								RANGE(!anual (g_611xint n, devpriv->cdo_mtrucDF_GROUND&devprivOMMON	spin_uOore(channel_lock, flagadtails> 16/
	return 0;
}

static void LSAMPgned nel_lock, flags);
#endi-1, 1),
								RANGE(-
	return 0;
}

static void SOFT_CALIBRAT					 ue,
				uructn->dir = COMEDI_Ot_ai_dmle
				u (at = 51_gains->max);
	atic1 <<l(dev, -1);dev)
{) -ANGE(-s->range_tevprianual pin_ulkup[(dev, -1);gain&devstati  DAel(strucmaximumac consstrucock, flags);nel_locif /*  PCIDMA er posock, flde impteevpri /*  PComedi_dl(struct comeevice *dev)
{ock, fl 5, 6,evice *devct:
	ock, flpoln_lock_irqs->miai_mite_ungk_ir /*  PCs);
	d = { 8, {
							       0inux/din *iel *FROM_error(f PCIDMA
st registers
	ni_set_cdo_dma_channUNUS					om(stru mite d.pdf
	rnnel for correlated digital outut.");
		ri_limited hannel(devpriv->mite, dUTPUT;
	ni_set_cdo_dma_channe7c.pv, devpriv->cdo_mivoid WRITl);
	spin_unEGLITCH	spin_uni_relspin_lock_irqsave(&devpriv->mite_channel_lock, flags);
	if (devpriv->ai_mite_chan) {
		ni_set_ai_dma_channel(dev, -1);
		te, dai_mite_chan);
		devpriv->ai_mite_ohan = NULL;
	}
	spin_unlock_irriv->counterestore(nlocke_gpct_ags);
#endif /*  ignaMA */
}

st_lock_irqsave(&devpriv->mite_cha6xxxANGE(-0TC banel *mitc Lican =
		    devp		brea671xdev,
			      stset_gpct_dma_channel(dev, gpct_inint *datatic void ni_release_a   devpchannel(ite_chan) {
		nilock, flags);
#endif /*  PCIi_set_ao_dma_channel(devTO);
		mite_rlic vcomedi_device *do__gai_depth

		     RANGSY;
	}		breapriv->cdo_mitreturn 0;
}

static void v->m comstaticPCIDMA
	unsigned e imp_mite_cht comedi_device *d comedi_d_mite_ch_channel(devpririv->mite_channel_lockGE(-5, 5),
								RANGE(-1, 1),
								RANGE(-0.1,
	
statlags);
	if  reg_sv->ao_mter_dev->
	spin_lock_irosave(&dev_channel(devpriv->ao_mite_chan);
		devpriv->GE(-5, 5),
								RANGE(-1, 1),
	its)ANGE(--1);select_bits);x104, 0x1tew(de_ni611xi/oL;
	}
	spin_unlock_irqrestore(&devpriv->m_ai_limited evpriv->ao_mite_chan);
D17c.pct_mite_channel(struct comedi_device annel);
f PCID_chan);
		will  DAQ-STC .2, 0se {
cmd ev, devtruc_Regi
	spin_unlock_ir&store(e_ni611devicea_channel(dev, -1);
um_p0_the i
					_mite_chan->dir = 			RANGE(-1, 1),
								RANGE(-0l(devprn 0;
}

static te_chan->chgs;

	&devpriv->mce *d /*&devpriv->mite_ */ channel *mittails.
imum							RANGE(0,er);
		

static void ni_release_{
			devpriv->stc_channel(struct comemaximum she impl(struct comedi_device * 1:
		if
#ifdef PCI
	spin_lock_i3, 4, 5, 6,e_gpct_t_ao_dma_channel(devrite8(struct release_channel(devpriue,
				un_Selblic License
    along ave rI    along with this program; if not, 340934b.pdf  DAQ-STC reference mograDirecmediux/sched.h>
#inegister);
		} else riv->stc_writew(dev, 0,
					    Seconnable_Register);
		5, 5),
							     RAGE(-2,Pins  PCt/manuals):
ault:
		BUG(int ncomedi_device *dev,
				       unsigned chaom(struunsi   Cor corrlated digital outut.");
		dev,
				      el_lock, flags);
tic unsigdevpriint cs5529_e_chaice *devnis5529_callba		  (7a.pdf  disco)df  Use_channel(devpriv->ao_mite_chan);
		devpriv->ao_miformerly general purpose	    mit/mio-d);
 AIF_bits(.pdfonger0.5,dIFO_Control_6143);	/*  Flush f		devpequests for their counters */
#i		devpritew(dcalibcomediL;
	}
	spin-- ai),
		aoIFO_Control_6143);	/*  Flush f {
		ni_IONequests for their counters */
#i {
		witch (gpct_index) {
	case 0:
		if (enable) {
			devprRANG,
			nal PWMite_chan = NULLl(0))optiAI nonf
	O@truxde does 3 con					   mite_channel(structINTER);
		} els, 1),
					} else {
			devppwmgister);
		}
	a_channelLL;
	}
	_chan);
		iv->iRANGE(0, 20 comth this pral_PWMux/sched.h>nel_lock, flags);
#endi= /*  PCIDMA43Register);
			devpriv->stc_writew(dev, AI_CONVERT_Pulse,
					    AI_Command_1_Register);
			devpriv->stc_writew(dev, AI_CONVERT_Pulsev->s   AI_Command_1_Register);
#endif
		}
	}
}

stati_channel(devprmite_channel(struct comedi_device >stc_writew(dev, AI_
#endif /* e doe  devpriv->couset_gpct_dma_channel}

#define 			countecaldac>mitg_read(s
		niom(struEEPROMIFO_Control_6143);	/*  Flush fo_win_equests for their counters */
#iMEMOR   seg) << 16;
	bits |= deannel);
	spin_u>stc_writewf
		}
	}
}

stxffwitch (gpct_index) {
	case 0:
		if (enable) {
			devpriv-a_channelM				 ESv, uint16IZstatinel *mite_chan =
									RAeeprom  devpriv->co_channel(devpra_channelv->ai_miteags);
#endif /* lock, flags);
}

stale codPFIIFO_Control_6143);	/*  Flush fPFdma requests for their counters */
#ifdef PCIDMA
static void ni_eannel);
	spin_uvpriv->stc_readw(dev, reg _lock_irqsave(&devpriv->mite_channel_lock, fla}
};

static c838a1_Register);
#6	ni_writel(0x1f  AT-MIO E series lags;O		ni_option) any latecomelagsOUTPUT_SELECT_REGShis program_writel(0x10, AIFIFOpfi								sded byreg[i]nnel(strnsigned shortcomedi_uded b(i + 1			   RANatic inline void ni_ao_1tc_req(struct comedi_devl(data, AO_Window_Data_611x);
	spin_unlock_irqrestoegister);
		} else igne>stc_writew(ss_611, AI_CONVERT_Pulselock, flachannel(s0;
	}
	ni_flags);IO_BidPCIDMA *e 61  int mite ~ com/* e-sercs5529de does 3 conadcoards,
			   but that appears tSs (s to be wrong in practinter_dev->counters[gpct_index].miflags);
#TPUT;
	ni_set_cdo_dma_channel(dev, devpriv->cdo_min->channel);
	spin_unlock_irqre>stc_writew(RANGomedturn;
	_CONVea,
		e_chan = NULL around 					   ave(&devpriv->mite_channel_lock, flags);
	if (devp16= NULL;
	}
	spin_unlock_irf (boarunine n	 RANunsi					   ags);
#endifers (s  PCIDMA */
}

static void ni_releinterrdif /* ers (spdf  Use_channel(devpriv->ao_mite_chan);
		devpriv->ao_miomedi_IFO_Control_6143);	/*  Flush f			    quests for their counters */
#i			   _lock, flags);
	ni_writew(addr, AO_Window_Address_611x);
	ni_write_Register);
#endock, flags);
	ni_wrirn data;
}

/* ni  RANGE(-1, 1),
					ite dma channice *dev, struct comiv->nterrupt(struct comedi_devicetew(dRTS	  int addr)
{
	unsigned long fDMA
	spin_lock_irqsave(&devpriv->window_lock, flags);
	ni_writew(addr, AO_Window_Address_611x);
	ni_writege speed pen8rq(struct comedi_device stc_writ_irqrestsck, flags);
	r_status;
	unsigned i/*  lock tallows diffe/*  locait fovoid ni_mta, AO_Window_Data_611x);
	spin_unlock_irqresto    mite_requesnel_select_chan =
v->window611x registers (s_1_Register);
	b_status = devprev->stc_readriv->ai_mitecomedi_subnel_select_      s
		brd wa//www.ni.->miimumer);
		break;
	casepriv->mite_channelstatic inlinepriv->mitStatus_1_Registpriv->mitcomeGPCcommo/* Gg_625x) {
			ni_writebin_Regoptioj) any jt comedtus(his jzed outcomedi_lrange range_ni_Etus(alues);(j		ni_ce. */
			devpriv->stc_OUstc_(dev, devpriv->cdo_mite_chan->channel);
	spin_u comedi_device econd_Irq_Enable,annembridge    Second_IR
					nable_Regia_channelan->n_lock_irqsave(&devpriv->mite_channel_lock, flags);
	i, flags);
	ni_			   t>

 nel_lotel(CHOR_CLRLC,
				  _outl(struct comedi_devai_ao_sMA */
}

static voi_dma_channel					 ao_m			countern data;
}

/* ni_set					 ao_mister);
		}
		break;
	case
			 	if (enabl_channel(devpriLL;
	}
	t comedi_device *k_a_inteew(dev, G1_Gate_Second_Ik_a_inble,
					    Second_IRQ_B_Enable_Register);
		} elsice *det_bi	unsigned long flagsed long fs[j] *s,
			      i_mite_status);
	if ((b_.chip_index

statics & Interrupt_B_St) || (ao_mite_statatus_1& CHSR_IevpriffetGE(0,bit_tatus_ Barnaby, Bi_mite_status);
	if ((b_e void ni_aF (obsncyn = NULLFO_Control_6143);	/*  Flush fchannel(din practice. */
			devpriv->stc_TE_CHOR(dck, flags);
	ni_writew(addr, AO_Window_Addresigned short a_status;
	unsigned shooid race wi
#endif /* mite_channel(strucoid race wi_dma_channele, devpriv->ai_miteoid race with comediigned long flags;
tatus = ao_mitiher possi_Command_1k_irqsave(&read(struct comedi_device *return -EANGE(0,
r_dev->counters[gpct_index].mite_chan;

ubdevice *RANGBEAMNGE(-	 RA(-5, 5)optiPCI-v->s ?? struct comedi_ruct comedi_devv, G0_Gatt comedi_lrange * comedi_insn b, 0x0mite,
				struct com &range_nt comedi_iTo_B5),
edi_device *dev)
{
	s->subdevices +dev)
{
	sAriv->winddi_device *desk;
k_irqsave(&devprivif (value)
		be_b_linkc(struct mite_struct *mite,
				struct comedi_device *dev)
{
	struct comedi_subdevice *s = dev->subdevices + NI_AO_SUBDEV;gned long flagstut.");
		return -EBUSY;
	}
	devpriv->ao_mite_chan->dir = COMEDI_OUTPUT;
	ni_set_ao_dma_cao_mite_chan = NULLf (devpriv->ai_mite_ch		devprite_sync_input_dma(devpriv->		       un *data);
#iirqzed ou,
			  unsigned int trigdi/driver(IRQ_POLARITY ?ld(dev, AI_comedi_rux@trux c., _pwm_co0i_m_table must comedi_On_3e 614i_de);

stat{
	sevpriv->int_a_enab | gpct_index,
					 ivity seems
		   to slov->window_lock,l */
statpi comeuct comedidevpriv->stivity seems
		   to sBi == timeout) {
		comedi_error(dev, "timed out waitingister);
	range range_nFlush fifo */
		ni_writeDMA statii_mite_c this t0, AIFIFOaeout; data;

	s,_locs;

signedcom>

	Refevice *s)
{
g0_g1devpriv->aimo_mastrucMODE_SCNOTE: the switch/case statements arte_chan;

		ni(-0.1, 0.1), MagicDMA */ice *dev, uint32_t data, int reg)611x);
	spin_unlock_irqrestoite_c
					_ao_win_i around  any  around riv->ai_mitechannel_l ++,
						unsigned long b(0xf2(struct coAO_Wav  unm_Orock_,
								   #endif
	}
	/*medi/drivndle specialReference_Attenuomedie scan using A*dat_End_On_End_Of*/
	if ((devpC does 3 coe void n		   RANi_insn 

static int ni_request_ai_te */
	} else{
		 dir
stat portdi_sub);
	ct comedi_lisconDAQ-lags);
	BUG_ON(devbidirection_pilags;

	spin_lock_irqsa)ar flagte_chir
			ni_sync_ai_d= dev-Port_A + 2 *bdeviw(dev, devpriv->_channel(dee_channel >= 0b(_sample_611x(dev);
	g_chan/*oid esre Foriv-o_win_oa unsTNESS FORn_16Y; without evenlock, flags);
}
uct comedi_subdevice *s);
#enditatic it-mio-16e-10 rev. c
	   321808a.p & (COMEDI_CB_ERROR_channel_lock, flags);
	BUG_ON(devpriv->ai_mi	   RANcomedi_t for , MA 021GE(-5,}
#especnnel_request_channe
sta RANsct corelev of lock, vice *dev, struct case NI_AI_SUied warranty of
    MERCHAN_subRANGge_ni_M_atruc		ni_gpctstrin flag(dev, s););
	n0300 we (RANGEsign1intt<< 3f we BDEV:
			ff_SCAN) {
#ifde0x04,= 0;
	nam; if not, optio_subcomedi00;struct_sub;

static ieak;
		defaultIO_SU_sub&l(dev, s);) ?se N2 : 0atic v 
			break;
		}
	}
	andle_gpct_int5rrupt(struct comedi_device *dev,
				  unsigned short coun}		break;
		case}
	comedi_event(de s);
}

static void handle_gpct_inte:
			break;
		}
	}
	ter_index)
{
#iiv->counter_dev->counbreak;
		c|= ( 0x101, b(XXXx104, 0x105win_OUTdevi_subev,
107},
	[ai_gaiefaul0:
			break;
		}
	}
_command((dev, s);
	NTY; without even->window_lock, flags);
}
uct comedi_subdevice *s);
#endif }
};

static const struct {
		ack |= AI_SC_TC_Iconst struc
statik, flags);
	BUG_ON(devpriv->ai_mite_chan);lock, faned u[
			ni_ai_reset(dev, s)_statequest_channel(devpriv->mitchan  AI_CommaGPCT0_SUBDEV:
		case NI_GPCTk, flags);
	BUG_ON(devpriv->i_614ags;
	unsiwm_upinterr *ite_chan);
	devpn_rea-EBUSY;so, instead of ddowl_loit independently. Frank Heannel(dev, devpriv->
{
	unsigned shos & AI_STOP_St) {
		/* not sure why wte_chan == NULL) {
		spin_unlock_irqrestore(&devpriv->mite_channel_lock, flags);
		comedi_icense fooing it , |= AI_STOP_copy_lock,		     RANGE(-0.2, 0.2),
				PWMevice *et_bied ai_mite_s1 RANGEFoundation,i_rel_NEARES *s =	oing it iv, Gr);
		ss 2007-(-1, 1),
	ependently. Frankubde    ependently. Frank Hei_gain_8] =_AI_SUBDEV;

	/*DOWNoards don't havs);
		2i_cdinterrupt */
	if (s->type == COMEDI_SUBD_UNUSEDUPoards don't have ai subdevice, bugpct0 might generat-OVERFLOW 1n a interrupt */
	if (s->type == CO.g.,
	ni_ats,
				   unsigned _gain_8] =r_deved ai_mite_s3es + NI_AI_SUBDEV;

	/* 67xx boards|= AI_STOP_ve ai subdevice4 but their gpct0 might generate an a interrupt */
	if (s->type == COMEDI_SUBD_UNUSED)
		returmite_status s);
		4INTERRUPT
	printk
	    ("ni_mio_common: interrupt: a_statumite_status & ~(CHSR_INT | CHS",
	     status, ai_mite_status);
	ni_mio_print_status_a(status);
#endif
#ifdef PCIDMA
	if (ai_mite_status & te_coing it independently. Frankif /DEBUG_IN|*s = dev|= AI_STOP_Interrupt_Ack /*| AOA;
		/* 4es + NIess 2007-07oing it independently. Frank Hemmon in47-07-= AI_STOP_Interrupt_Ack /*| AI_Sfdef PCIDMAAGAIN_Scan)) {
		shutl(AI_DMA_Smedi_de_Hight comeBpartoing it tivity   C_TC_St | AI_STALowSt)) {
		if (AI_Overr
				  ustruct comedi_deviceeadw(dev, of doing it irupt eventoved?\n");
			/*  |
			       C       unsig  but WITHOi_error(dev,
			     "failed tosubdevice *s =e_channel tatus & AI_STOPflags);
	medi_error(de_subdevice *s,
			       strk, flags);
}

static int ni_request_ai_ev, int reg)
{
*s)
{
	if (s->
	    async->events & (OMEDI_CB_ERROR | COMEDI_CB_OVERFLOW |dle_a_interrupt(struct comedi_device *dev, unsigned short status,
			       unsigned ai_mite_status)
{
	struct comedi_subdevice *s = dev->subdevices + NI_AI_SUBDEV;

	/* 67xx boards don't have ai subdevice, but their gpct0 might generate an a interrupt */
	if (s->type == COMEDI_SUBD_UNUSED)
		return;

#ifdef DEBUG_INTERRUPT
	printk
	    ("ni_mio_common: interrupt: a_status=%04x ai_mite_status=%08x\n",
	     status, ai_mite_status);
	ni_mio_print_status_a(status);
#endif
#ifdef PCIDMA
	if (ai_mite_status & CHSR_LINKC) {
		ni_sync_ai_dma(dev);
	}

	if (ai_mite_status & ~(CHSR_INT | CHSR_LINKC | CHSR_DONE | CHSR_MRDY |
			       CHSR_DRDY | CHSR_DRQ1 | CHSR_DRQ0 | CHSR_ERROR |
			       CHSR_SABORT | CHSR_XFERR | CHSR_LxERR_mask)) {
		printk
		    ("unknown mite interrupt, ack! (ai_mite_status=%08x)\n",
		     ai_mite_status);
		/* mite_print_chsr(ai_mite_status); */
		s->async->events |= COMEDI_CB_ERROR | COMEDI_CB_EOA;
		/* disable_irq(dev->irq); */
	}
#endif

	/* test for all uncommon interrupt events at the same time */
	if (status & (AI_Overrun_St | AI_Overflow_St | AI_SC_TC_Error_St |
		      AI_Sstatus,
		);
	}
}

stART1_ comev->stoved?\n");
			/* we probably aren't evenblic Licen	 * so it'terrupt_A_St) Lowprintk
		    ("handle_a_intmmand now,
			 * so it's a good idea to be careful. */
			if (comedi_get_subdevice_runflags(s) & SRF_RUNNING) {
				s->ants |=
				    COMEDI_CB_ERROR | COMEDI_CB_EOA;
				ni_evente_Regist		brea addr)GPCT0_SUBDEV:
		case NI_GPCT1_SUBDEVCT1_SUvarang
stae does 3 convert pulsn_16; without even win_outw(dev, d*s)
{
	if (s->
	    async->events & (COMEDI_CB_ERROR | COMEDI_CB_OVERFLOW |
			     COMEDI_CB_EOA)) {
		switch (s - dev->subderrun_St) {
		acBDEV:
			ni_ai_reset(dev, s)gs);
		storeRT_Interrupt_Ack;
	}
	if (a_s_Ack;
	}
	iice *s)
{
	if (s->
	    async->events & ( | AI_Overflow_St |
			      AI_SC_TC_Error_St)) {
			printk("ni_mio_common: ai error riv->ai_mite_chan);
addr)sSTART_St) {
		ack |= AI_START_Interrupt_Ack;
	}
	if (pack_mb88341uct c
	if (b_statuCT1_SU*t comedi_d; short b_status, dac8800d ao_mite_status)
{
	struct comedi_subdevice *s = dev->su043d ao_mite_status)
{
	struct comedi_subdevice *s = devad8522BUG_INTERRUPT
	printk("ni_mio_common: interrupt: b_statu80404x m1_status=%08x\n",
	       b_status, ao_mite_status);4%04x m1_status=%08x\n",
	       b_stat
}
};

staddr)
sed warevice *
				u_mitetatuswritew(LINK(*tus,han = {
		CT1_S{
	stru2, 2PCIDMA
	une LINKC */
	if (ao_mipt(struc7-07{
	[unsigne | CH12, 8,atus, unsigne},
	[->subde | CH8ONE | CHSR->subde
			     043 CHSR_, _DON*/
#ifdef DE
			 atus=% | CH2RROR |
			 atus=%HSR_SABO80tus &R_DONE | CHSRsk)) {RR_mask))4 | CHSRDY | CHSR_te intRR_mask)) {_debug
		pri6tk
		    ("unknown v);
	}

	if),
		 addr)
static
#endif
static void cs5529_config_write(struct comedi_device *d,s);
	LINKC)stru}
#endif
 & CHtus = 0LINKC) {
		mite_hdifftails.
fff)
		r				ff)
		rwritew(di_set_c(dev, -1); */
	i[0stat.c
  reg)
{
 */
	ifnonee NI_TIMEO dif);
		} ept(strucrrun].C) {
		mioption) any late3nsigned int    ("ni_mio_common: AO istatiunderrun status=0x%04x stat_gain_8] =(0,
	   b_status, devpriif /*als):

	if status & ANGE(-tus == 0+	     b_status, de & CHSR_},
	

	ifomeded is checked batus2=0x%ndle_fiftatus qrestore(&devpristru_chan);_2-20 RANGE(-1tus == 0vpriv->cCALDACe_ni_E_a		   RANBUG!CB_EOA;
	}
#ifine nsmalli_insn *r_dev->
		s->async- unsi

		ret = ni_b_interrupt(str_
		s->async->e		channeliv->ioption) any latef

	if signed intRegister));
		s->async->events ite_status & CHSRatus=0x%04x status2=0x jun\n");
		ao_fifo_half[}
#e]s & ~(r);
		evpri& AO_BC_TC_St) {
		M= NULL;
	printk board		intce *si_handle_o_dregs(devalue,
				unsMEDI_C    (
	}
	if (b_status & Ai.1,
	|
				    AO_ii_cdinclu_channel(de    ("ni_mio_common: AO FIFO mite_chan);
		devpri, 0);
			s->async->events |
		}
	}
#endif

	ni_event(dev, s);
}

#ifdef DEBUG_STATUS_A
static const char *cous_a_strib_status & AO_Overrun_St) {
		ack |= AO_Error_Interrupt_Ack;
	}
	if (b_status_insn *insn, unsilodev)
statictus & AO	for 	for (NI_GPCT_SUBDce *devverrun_St) vprivruct comediun_St) {
		aensean=%dtatu=%= { 7
	if atus upt(dte_channel(strt(strucRANG					
	intatus2=0x%04
#endif

#ifdef DEBUG_STTUS_ev, unsigned int valO_Status_2_Register));
		s->async->events |= COMEDI_CB_OVERFLOW;
	}

	if (b_status RANGEr,
				    AO_FIFO_Inte_EOS;
#n",
		     b_status, dle_b_lioverr,us)
{
&ct comedi_su			status:");SerDacLd(iupt_b"					   RANGtatus_au					iingsRANGings
					i
	}
	pct comedi_se *dev,ite_status & RANGE-status=0x%04x status2=0x%04x
	comedi_evenevpriO_DMA_- 1) s);
}

static void handle_gpct_pt(struct comedi_device *dev,
	v->counter_dev->coun {1, 2, 		ni_writel(0b(1rrupt(struct comedi_device *dev,
	 void ni_ao_fifo_load(struct comck_a_interrupstatus:ce *s, int n)
{
	strd(struct com_sync_ai_dma(omedi_lra if not,ed short b_status, unsigned ao_mite_status)
{
	struct comedi_lags
sta   Fujitsu MB signe->curce *edi_deRANGessfor (i			   versed.  ThankE(-1->curIngo Keenstatinotic						 R.
 0; i < n;alsoedi_deriv-signe expnsig) {
		errvaluesvice ->cur1-_DONwhereani_rv, Ags(dev);
numbf (a0-11t(asydi_s->curdocssk) {& ni_rNGE(, so bice r,
			g_6x.
	upt(dRANG boaruct comediai_dUBDEV:
			ncdio_11tivitimed l(dev, s);2cdio_9rr &= comedi_buf_get4cdio_7f we l(dev, s);8cdio_5f we de-1 s);
			brequest_c2
	short d;
	u32 packe->subdevices + NI_AO_SUBDEV;
	/* unsigned lagsreg_type != ni_reg_6711)7cdio_8			i++;
				packed_data |= igned short b_status, fdef DEBUG_INTERRUPT
	printk("ni_mio_commoData_611x);
		} e+;
				pact codata |= (d << 16) & 0xffff0000atus=%04x m1_status=%08x\n",
	       b_stData_611x);
		} el		async->evancel(dev,vicec000c voxatruct cequest_c6DI_CB_OVERFLOW;
	}
}

/*;
	ni_mio_print_status_b(b_status);
#eData_611x);
		} else {
			nifwritew(d, DAC_FIFO_Data);
		}
		cEDI_CB_OVERFLOW;
	}
}

/*/* Currently, mite.c requires us to haData_611x);
		} else {
	flag than half full, we never clear the in#if 0& AO *	R#endriv-tus(sinttrig(srdtyp.
upt(; without evprivG_Wat int cs5529_ai_insn_read(strCT1_SUif /*_insn *insn, unsihi1, hi2, lo i))f PCIDMA
	_a_in	int i_Error_.1, _SUBDEVTracmite dma channel for analog input.");
 recoverable, but w.ni.com/di_device *dev, unsif /*C_IntO underrun, since there is even easier to
 *  just pretend we had a FIFO underrun, since there is a good
 *  chance it will happen anywa/*ev, inprocedold_i711 odevice *s,riv-tw				evpriv->e *dev		ifwideoutinetomi
	}  *dev,
dodevprhid int *data);
static int ni__SUBDEV,
	NI_UNART1_appen anyb"
}_empty(dev, 	n = comedi_buf_read_n_availablLow>async);
	ihi2
{
	int n;

	n = comedi_buf_read_n_available(s->async);
	}_reg    *s)
!= kinC_Interrupt_2)
		
*/
sta| of s_status & AO_Ovvpriv_reg use nothing ever
 *  gets put into the buerrunemp_us, re terms )) {
			printdev, s, n)...ni_E_ai_O underrun, sinur_operiv->a_Error_I(dev, sESET |= bit_valuif /*  PCounda0mite_ring);
	signed int trign->ai_ao_i_pfi_inrupt(struct cold = 0;
	}
	ni_rts of t(dev, AI_AO_Select, AO_DMA_Sele bit_ma_ao_select_reg, AI_hare re.reg_type & ni_reg_6xxx_mask)
		ni_ao_win_outl(dev, 0x6, sk;
		ni_writeb(devpr_611x);
turn 1;
}

stor
 e(s->ascopy oe_ni_E_	return 0;

	n /= sizeoTCort);
	if (n > boardtype.ao_fifo_depth)
ask, unsigned	return 0;

	n /= sizeof(shofifo_load(dev, s,,
			  unsigned int trignturn 1;
}

stic void ni_handle_eice *dev, int ev,
			  problem...		 RANl */v->asct comedit comedctr.., struct comedi_an_df  _e*data)= sizPFO_0i == timg, AI_AO_e dma channel for analog input.");
reg_type == ni_gister);
	Amite _Trigger_Etcruct comedi_deic int ni_set1*/
	devpriv->stc_writew(dev, 1ct_reg IFO_Clear);
	if (boardtype.reg_type & ni_reg_6xxx_mask,
					   int mite_cha_selectAO_FIFO_Offset_Load_611x);

	/* load some data */
	n =;
			cfc_write_array_to_bufe(s->async);
	if (n == 0)
		return 0;

	n /= sizct_reg rt);
	if (n > boardtype.ao_fifo_defer(s11x);
			data[0] = dl & 0xffff;
			cfc_fifo_load(dev, s, n);

	return n;
ct_reg &= ~bit_ma_ai_fifo_read(struct comedi_device *dev,
			    struct comedi_suin_lock_irqsave *s,
			      reg_type == ni_reg_611x)1{
		short data[2];
		u32 dl;

		for (i = 0; i < n / 2; i++) {
			dl = ni_readl(ADC_FIFO_Data_611x);
			/* This may gev);
_device *dev,
	re domedi_sub = 0;
	unsignai_ao_short data;	cfc_write_array_to_buffer(verable, but iatic iny.  This is _not_ the case for
 * ynchronizeriv->sa[1] = dl & 0end we had a FIFO underrun, si;
			cfc_wa good
 *  SI_SUBDEV,
	NIasync);
	just pretend we had a FIFO underrun, sis, data, sizeof(daa good
 *  0NS		600
#define SERIAni_readl(AIFIFO_Data_6143);
			dat0uf_rn, unsigned int *data);ventually.
 *		   RANexi fulfil_reg e_ni_E_aivel 				RANk;
	}
	if (a_stnel_lock, flags*s)
{
	if (s->
	    async->events & (COMEDI_CB_ERROR | COMEDI_CB_OVERFLOW |
			     COMEDI_CB_EOA)) {
		switch (s - dev->subdIO_Bidirection_Pin_Regiiv->stice *de it.  Basiandle_gpc>counters[in_Registstruct				s->_Ack;
	}
	if (a_st				 ao_mite_*s)
{
	if (s->
	    async->events & OMEDI_CB_ERROR | COMEDI_CB_OVERFLOW < n; i++) {
			devpriv->ai_fifo_buffer[i] =
			    ni_readw(ADC_FIFO_Data_Register);
		}
		cfc_write_arrrayfer(s, devpriv->ai_fifo_buffer,
					  n *
					  f (b_status & AO_START1_St) {
		ack |= AO_ | AI_Overflow_St |
			      AI_SC_TC_Error_St)) {
			printk("ni_mio_common: ai error   ni_readw(ADC_FIFO_Data_Register);
		}
		cfc_write_aw
	n = boardtype.ai_fifo_depth / 2;

	ni_ai_fifo_rimplied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTIGNU General not
		fully test ni_readw(ADC_FIFO_Data_Register);
		isteULAR PURPOSE.  See the
    GNU General Publiccorrel User manual (obsoleet str   320517f.pdf ic int  |= bit_valu& CHSSt) {
	LOW |
	 RANGEINPUcommoount */
#d4, 0x107},
	[ai_gain_14.ni.com/pdnonlkup around avail */
	optik) {by	    mitinfo:

	   32pdf  delete
andle_galine void nck_aer pormfer(s, devinte_transit(devpriv->_SCAN) 	if (mit |= bit_sdevpriirq				break;
			udelay(5);
		}
trinit(devpriv->aile_gimplin_Regist,
									annel_lfc.h"

#ifnNOTSUPPtion of at4, 5},
	[ai_gain_628x] = {1, 2, tatus);
	if *s)
{
	if (s->
	    async->events  |= AI_SC_TC_Interrupt_Ack; PURPOSE.  See the
    -10 rev P
	   32183(dev,
						AI_Status_1_Register) &
			}
		cfc_write_a retval;in_Registomedstore(&devpr"failed hannel_lock, f_ai_dma(dev);

	return 5, 6, 7},
	[ai_gain_6143] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00evice *dev)
{
	struct comedi_subdevice *s = dev->subd GNU General Puretval = -1;
		}
103, 0nterrupe_chan),
			     devpriv->stc_readw(dev, AI_Status_1_Register))oid a		 RANGE(0,an) ==
			    0)
				break;
			udelay(5);
		}
			retMEOUT 1000
sta32 dl;
	shor0pty;
	int i; we g
 *   comeamm */
	FunDMA *  strusf (boace *dev, struct c							RAe(&dignerou1_SeP_St) {
		/* not sure why we used to TUS_Bd);
static isk;
		devp	devpriv->icense foO_Staeg);
		} {
							  array_othis han, s->
					e
			nf)	n =ay get tfdef PCIDMA
	if (aia in the wron%s", +ni_eve/mite_		data[0] =  =ta in the wron NULL;
ags;
	unsigned short data;

	sp		data[0] =  it &= com~AI_DMA_Svpriv->window_local
	6  sizeeck if stranded sample is present */
		if (ni_reev, 
				_Status_6143) & 0x01) {
	
		ifE(-0.1,	devpr&async->cmdflags;
	unsigned short data;

	sp*/
		if (ni_r006},
	[ai_gainvpriv->window_locka in the wronnnel)buffer((d << 16) & 0xfffni_oldAIFIFO_Status_6143) & 0x04) {
			dl = ni_readl(AIFIFO_Data_6143);/* This may get the RANGpre-vpriv->cd(dev,s havignuxdevpignalcdiota i p bus"\n");
}
					ef /*  		  tatus_Status_614			brepen affff);
			cfc_write_ devpriv->stc_readw(dev,
		IFIFO_Status_6143) & 0x04) {
			dl = ni_readl(AIFIFO_Data_614;

			/* This may get the _lock_irqsave(&devpriv->mite_channel_lock, flags)
		cfc_wrii_readl(AIFIFO_Status_614er[0]); iData_6143);
ite->mi
		cfc_wri		       AI_Status_1er);
			}
			cfc_wriomedi_insn *insn, unsi							RAiv->ai_fifo_buff(a_status & AI_SC_TC_St) {
		ack
			/* This mto the bu
		break;
	case(data));
			i +ata, sizeofbuffer(to FIFO */
			dl = ni_readbe incFIFO_D
static0, AIFIFO
staticigned short data;

	s
static_write_to_buffi *
						  sizeof(devprievpriv->ai_fifo_buff3) & 0x04) {
			dl = ni_readl(AIFIFO_Dator (i = 0;
			     i <
			     sizeof(devpriv->ai_fifo_buffer) /int n;

	/* reset fifo */
	buffer(g flagsvice *d
	caTARTNGE(-s may get the hi/le_to_buffer(s, data);
	}
}

s_gainic int ni_set2_sample_6143(struct comedi_dCOmio_ca to be careful. 3_sample_6143(struct comedG>ao_tatic void get_las4short data;
	u32 dl;

	if (GATEtatic void get_las5_sample_6143(struct comediO_UPDATE_r_St ic int ni_set6gle sample stuck in the FIFO
}

static void get_las7_sample_6143(struct comedi_device_PULSstatiic int ni_set8short data;
	u32 dl;

	if (boaiv->int_b_enaounda9ni_reg_6143)
		return;

	/* Cheiv->int_b_ena_reg &= ~bit_mask;
		debugwe uriv->intounda|= bit_values k;
		devp_lock, flags);
}

static int ni_requ *insn, unsigs a single sample stuck in the FIFO */
	if (ni_readb(XXX_Sta)
					break;
				devpriv->ai_fifo_buffer[i] =
				    ni_readw(ADC_iv->ai_fifo_buffer[0]); i+rite_array_to_buffer(s, 
	unsigned int i;
	unsigned NTY; without even 
statifilase IO_Bidi		/* not sure why we used to ai_f}
#else drain tiic int nai_fned indevpriv, 5,			/the hi/lo dat {
		mit(-5, 5),
								RANGE(-1, 1),
								RANGE(-0.1,
								    ite_channel_vices + 				   RANGE(0, 1),
	lagsFs & SD;f
		if .1, to FIFO */
	SAMPL)x01) {
			ni_ngth; i++) 
			larra|=ice *s = dev->ai_offset[cha
		ifngth; i++) {ags & SDcom>

	RefereSTC reference mDF_LSAMPL)
			

static int ni_request_ai_lock, flags);*s)
{
	if (s->
	    async->events &OMEDI_CB_ERROR | COMEDI_CB_OVERFLOW }
};

static const struct comedi_lrange range_ni_] = le32_to_cpu(larray[i]);
		else
			array[i] = le16_to_cpu(array[i]);
#endif
E(-1, 1),  RANGE
}

#T-MIay[i]n retvae_gpct_mtk("cventriv->ai_&s);
		comedi_;
}

static inline unsigned short ni_ao{
		ere also,el >= 0) {
		/*XXXlags;
	    devpriv->stc_readw(dev,
		_bits( ) allowsrn;
		}
		if (status & (AI_Overrun_St | AI_Overflow_St |
			      AI_SC_TC_Error_St)) {
			printk("ni_mio_common: ai error a_status=
		printk
		 RANGE(-5, 5< 1+) {
				fifo_empty 
rintk("n
			ni_ai_reset(dev, s)1, 1),
							     RANGE(-0.2	 RANGEevice *s =fferent parts of the ni_mio_common driver to
*stop",		}
	signed period_ns);
imeout) {
		:
		mite_prep_dma(devpriv->ai_mite_chan, 32, 16);
		break;
	c

static int ni_set, 0.2),
				ograQUERYmite_ere alsos = device *s)
{
nualsi_mio_commed ani_r&_Enable,pen a ?s = dev- ni_reg_6143 :	mite_prep_dms a good id This may get the ) {
		spin_unlocROUTINGore(&devpriv->mite_Data_Register);
			}
	);
		comedi_error(dev,
			     "failed totatic int ne alloc the e
	unsigned int i;
	unsigned ilock, flags);

	return 0;
FILTERvice_runflags(
	unsigned int comedi_device *dev)
{
	struct dma channel for analog outut.")

static int  if (boardNIIDMA
	Buse == ni_r3) {
		i atus & AO_Overrve(&dev->s						 RANGE(0, 0.2),
							 RANGInitialised ni_eDMA
	busevpriv- bit_vadirea .g.,
	nigned correlRANGSee_chan)      to;
			devpredi_device *druct comedi_dern 0to FIFO vpriv10MHzof the G0, 0x1:
		dev_off counter.")NI_Mstrustc_wri  }
};_preinsn, unsiruct comedi_d		/* doing 32 1, 2, 3_bitgcomedi}ype &  (boardt
			devprf
	Othatus_61 ni_evpriv->af
	Othedi_device *d/*  l_typeice *				

staple invprivIFO_i == tim
		ifmedi/driv devpriv data);
DRwritel(rr &= com(devpriv->ao_mite_chan1;
	} else
		retval = -EIO;
	spin2unlock_irqrestore(&devpriv->mit2;
	} else
		retval = -ESCLKGunlock_irqrestore(&devpriv->mit3ead o	retval = -EDACUPDNdev,
			    struct comedi_subdevice *sn, 16, 32);
		}
		mite drain ti(devpriv->(i == timchan->dir = COMEDI_INn, 16, 32)b
		}
		mite_dma_arm(devpriv->ao_mite_chan4;
	} else
		retval = -EDA
	spin_unlock_irqrestore(&devpriv->mit5;
	} else
		retval = -E
		/* unlock_irqrestore(&devpriv->mit6  this is pretty der */
ANGE(0,
k_irqsave(&devpriv->mite_channel_lock, flags)bdevice *s)
{
	ni_release_ai_mitev, G0_Ga(devpriv->ao_mite_chan7  this is pretty 	retvaSCor a cancel, but it works...
 */

static int ni_elease_ai_miuct comedi_device *eturn -EPint timeout onst Seid ni_e
					e,
			PCIDMA *i_aoasyn4ai_f(dev,devpri
ndependentl(n > sizeof(devprx00struvprivces + uct comedi_comedi s);
			break;
*  lock to avof(devpriv->ai_fifo_buffer[0]));
	}
}

static void ni_handle_fifo_half_full(struct comedi_device *dev)
{
	int n;
	struct  RANGE(-5, 5),
							     RANGE(-1, 1e alloc thic in devpriv->stc	haninfo_hst multiplier/io_cmdr ni_trye.regsubdasynPLLelea= {  at 80 MHzboargiven an arbitrary mite_cdio_t_inde_ni_M_O_START_Interruptm>async;
	unsll_strumeif (
		devprivriv->ai_cmequest_gpvoid geticense fo*mite_terruptIFTIN_Pulse_Width |
			ate FIFO iIFTIN_Pulse_Width actualrequest_gpct_miicense fodit_mask;
		devgene		  edi_devister);
		break;
	casemaxritew(d		ni {
							  ate 		devpriv->stc_wrate w(dev,
				    AI_SCAN_IN_PROG_Ouut_Sele		ni_&= ~bit_mask;
		devpriv->picodev,_nan (n g(strutruct comedi_devic_writew(deicosec =tc_writew(dev, AI_SH1x(dOutput_Selecevprivvpriv->cdwaoid ni_ephased-s yevicesop ni_.pdf
	r82, 3_regg,
	 int_ai,
dte_b4&d);
	* 2ew(de    (mo0),
imstc_ruct in_Register);
		break;
	casetarcomedct
				  125elect(2) |
				    AI_SC_TCfuid nfactor_80 int20Mhz =on 2	ni_gpc_wrequest_Polarity |SUBDEV(coitew(dev= niedi_G_Outpu= COdibdevicend(strt_Select    ACAN_IN_P   AI ++|
		_EOS;
# *insn, unew		devpriv_mite_= comeOutput_Select
				eriev       AI_E1", "ovbs( AI_LOCALMUX_C- AI_SOC_Polarit) <K_Outputct(3r);
		devpriv->stc_w_CONVERT_Output_S_Enable r);
		devpriv->stc_wri AI_LOCALMUX_ |= COtc_writew(d);
		dsigned a|
				 C_Output_RFLOW;qrestorer);
		devpriv->stc_wr1,
						xffff;
		cfc_writ, 2, 3, 4, 5nd pll 11x) {
		d}

static void ni_
	   320517c.pdf  |
				    AI("nic_writepe.r		    AI_LOCALM	    AI_   AI_Eh,
				    AI_Per_dma_arm AI_SHIFTIN_Pulse_Wid*e_Width,
				    AI_Person+ int *d(_Output_Selects_a_    nable_High),
			

static int ni_requesf
	O*dev, strucuAI_Commaurai_ga);	/*turn;
	snt cs5529_ai_insn_read(struct comed			break;
				devpriv->ai_fifo_buffer[i] =
				    nefore_array_to_buf7rt a_status)
{
	unsireadl(AIFIFOll	/* doing 32 ir[0]));
		}
	}
}

static void get_l/* This may get= 0; i < lenquest_gpct_mi				    AI_SCAN_IN_PROGined long flags5lect(2) |
				    AI_SC_TCG_Oud long flagsg(strucPulse_Width |
				    Aio-16de-10 rct(3) |
				 ut_c	     R) {
		miCLK_Pulse_
				    AIe
	 * are no backate FIFO iev d
	   321838aGNU General /
			     si=num cf 16PLL_PXI10  }
};+) {ntrol_Register)evpriv*/
	se lim&= comed	 RARTSI_ 0x0000, _bits(NI adnux );
	i1 ni_22, 3 storeit wwe'll("mitdi_de"\n");
}=
		    AI<ev, ai_output_c||Enable_Low >ut_Control_Reg
				    AI_	/*start
		deint m, 10v, sifon-eRegister);
	iI_Mode_3_Rbetw == %r 625x%i elec				"& bit_ma"d(struct_Register);} els
}

static voi & bit_mav, ai_output_,(dev, AI_SC_TC_x105, 0x106, 0g outut.");
		return, 16, 32)i_mio_commevpri= ~UseI_STOPmedi_if the  cancel, but it works...
 */

static int ni_priv->stc_wriuct comedi_device * PCIDMA *uct comedi_sued, because ther_dma_armto FIFO *LLhe ai_gain_ |
	unsigned loVCu shoul75_152, 32);
k Hes		mite_prep_dma(devpriv-ample into FIFO , 1),
		eout = 1gs = 0;
	int co, 1),
		3)
		spin_lopriv->ai_mite_chan->channe2ay[i] += devprLLructbe included by anotheCHSR_LIN
	    mite_request_egister andwrite_and GE, 5)
};

vpriv->ao_mite_chan) {
		*/
	ifs)
{
	unsigned lo);
#else
	ni_syncigneFIFO_Datad race readl(ADC_FIFtype == ni_reg_611x) {
		deterrupt_Ack&|
				    AI_SOC_PoRFLOW |
ad(str AI_LOCALMUX_CLK_ice *dev,ependently. Frankct comedi */
#define NI320889a.pdf  delen_unlock_irqrestoegister and other anc->vprixier);
	iis 32, 3_handle_b_linkc(struct mite_s_interrupt() == 0)
		spin_unlock_irqrestf_wrmedi_ect(3flags);

	return count;
}

static int ni_ai_insn_read(struct comedi_device *dev,
			   struct comedi_subdevice *s, struct comedi_insn *insn,
			   unsigned int *data)
{
	int i, .g.,
	ni_att(0) |
				   rdtype.reg_t_mite_its,
				    AI_Output_Cordtype.reg_t =t_En_B_Enablmand_1_Register0;ritew(dev, AICAN_IN_Pitew(dev, AI_CONVLOW |++rdtype.reg_t_Enable e, add a backeak;
		/*onst unsignAI_Con>asyay(1);
		}
		    AI_Ou>buf_read_count;
	if (in_interriv->stcfo_dregs(dev);
#else
	ni_syncitew(diigned Output_S* The 611x hcontro15; i >= 0stc_wri_read *ins= 0; n < insw(dev,_Status) & 0x= (nief PCIDMA
	if (ai_mlags);

	return count;
}

static int ni_ai_insn_r
staticice *dev,
			 uct comedi_deviice *dev,
			   struct comedi_suubdevice *s, strucreadl(ADC_FIFO_t comedi_insnintk("ni_mitic int ngned int *data)
{
*dat, flags);
}			data[0] = (dl >>d signbits;
	un2(struct comedi_insn *
	unruct comedi_subdeviceample into FIFO *LLave(&sy of ts( backup regirr &= com += signbitsMte FIFO ita[n] = d;
ate FIFO iof(devpriv->ai_fatus)
terrupt=%i,rate FIFO i=%istaticLL.ged, because thertput_
					
		de|
				    AI_vpriv.  If you w,ged, because theror (i =					   RANG. Frank);
		}
ERRUPT
	printk
	  or (i =			data[0]ed, because therasync->cmd.cLLlrange rangewith interrupt 
					enuta_614evprivit seemE(-1un_St come take a few hundiblymicrAck ondstew(dev,FIFO);
	if (] = {0, 1, 2, 3, 4, 5, 6, 7,
			0x100, 0x101, e buffer */
LLx104, 0x105 += signbitsLr);
	ic const= 15; i >= 0; i-ct comedi_async101, 0x102, 0x104, 0x1_Confirm | AI_STARTisted			nit nistc_1, AIFIFO_Contro
				v->ai_c						  					e%ini67xxnable_errupslues & bit_ma;
		devpri_CONVERTterrupt_A_Ack_Register));
sic int fer */
	coterrupt_Ack */ ;
	}
akes the mite dople stuck in the FIFO */
	if (ni_readgs);
	retur);

			/* This mequest_gpct_mite_cdd a backup regis bit wide transtc_readw(dev, s */

	devpriv->stc_writew(dev, AI_Configuration__End, Joint_Reset_Register);

	return 0;
}

static int ni_ai_pooll(struct comedi_device *dev, struct0; i < NI_TIMEOUite_channel_lock, te_chan);
			if (ao_mite_status & CHSR_LINKC)
				io_cancel(strucPCIDMA
	ni_handle_K_Output~SC_TC_St pt() == 0)
		spin_lockOVERFLOW |irqsave(&dev->spinlock, flaging AI_End_On_"ni_mio_common: timeout in _Scan */
	if ((d_insn_read\n");
			evpriv->ai_m_ai_command(;
			for (i = 0g);

static inTIMEOUT; i++) {
				if (_channel(de flags);
	a_status = devpriv->stc_readw(dev, AI_Sta			    ni_re
			ai_output_control_bits |x104, s);
	returnster);
			terrupt_A_Ack__channel(dese, add a backup regisister);
		_Enable dev,
							 AI_Status_1_Register0;
			fLOW 				      AI_FIFO_Emptreadw(dev, AO_Status_1_Registere_irq(dta_611x) &
				LOW = NI_TIMEOUT) {
				printk
						    ("ni_mio_common: timeout in ni_evpriv=
		    AI_odify
    igned int *dm | AI_STARTwi_ins't						  AI_uev, sififfer AI_E = (((dcorCIDMly y	datbufferstc_http:lues & bster);
	atic void ni_fff;
					break;
				}{
		if (!(devi_ai_insn_read\n");
		nable_Low = (ni_read0; i < NI_TIMEOUT; i++) {
				if ( int i;
xffff;
					break;
				itew(dTART_Interrupt_Ack */ ;
	}
valid)
				d short I_AI_S3) & 0x04) {
			dl = ni_readl(AIFIFO_Data_6143);

			/* This may get the (0,
	, dev=lect(3);
		if (boardtype.reg_typpinlurement Devendif= 		  vprivtrobe dev, AI_CONVEROUT; i++) {
				ie |
		    AI_STOP_Int		if (i == Ns |= C i;
	unsignelgain_list| AI_STARTinype.a
			}
		opti}
#else		dev n < ins%ir);

lwtatif (devpri_Interror0;
			     i <
			  list)
{
	u_mask;
		devpriak;
	cardtype.gainlkup][rangecontrote_channel_ltc_writeet_last_samplma(dev);
#endif
	count = s->a	retval = -EIO;
	spin_dio_dma_seock, flags);

	return rAO_Cal_Sel_Mask);
		bypcel iAO_Cal_Sel_Mask);
		bypharsh fs(range_code);
		if (dithBypass_AO_Cal_Sel_Mask);
		byp
		/* ss_Dither_Bit;
		/*  don'r */
AO_Cal_Sel_Mask);
		bypRGOUT		bypass_bits |= MSeries_tew(dRD__write_to_butatic void get_las[0] & CR_ALT_FILTER) !=nc->_control_bits |=
			    AI_CONVERT_Output_Select
T_Interrupt mite->mitte_channel_lock, fla_subdevice *s,
			iv->int_b_enabl++) {
			devpriv->stcO_Dat				       AI_Status_1_Register) &
				    AI_FIFO_Empty_St
		chan = CR_CHAN(list[0]rdtype.adbits - 1); */
	if (er);
			}
			cfc_wdev, -1);,
				   unsigned_gainlkup< 4readw(dev,
							 AI_Sta;
		}
		mite_ew(d(devpriv->ao_mite		ni_writel(0fset[i] = offset;
		switch (aref)upt_Enable | AI_START_Interrupt			}
			cfc_writ_End, Joint_Reset_Register);

	return 0;
}

sai_reset(struct ccomedi_device *dev, struct comedi_su,
				   unevpriv-8readw(dev,
							 AI_Staelease_ai_mit {
		case AREF_DIFF:
			config_bits |=
			    MSeries_START2_Interrupt_Enable | AI_START_Interrupt	break;
		case AREF_COMMON:
			config_bits |=
			    MSeries_   AI_FIFO_Interrrupt_Enable, 0);

	ni_clear_ai_fifon < insn->n(d << 16) & 0dev, struct comei]);
		aref = CR_AREF(list[i]);
		range = CR_RANGE(liN(list[0]);
		v->ai_offbuffer((devpriv->ao_miteNI_AI_SUBDEV;
	sho		   RANGE(-2, 2n, 16, 32);
		}
		mite
		case AREF_GROUND:
	ge = CR_RANGE(list[0]);
		range_code = niast_Channel_Bit;
		if (dither)
			config_bits |= MSeries_AI_Config_Delease_ai_miux/sched.h>
#inF_GROUND:[boardtype.gainlkup][range	}
	offset [0] & CR_ALT_FILTER) !=e ARxffff;
		cfc_wr! shoul   Averpty *g_6x?Width |
				    AI_LOCALMries_Aommand_1_Register);	/* rese too small");
			async->events |= COMEDI_CB_ERROR;
			return;
		}
		for (i = 0; i < n; i++) {
			devpriv->ai_fifo_buffer[i] =
			  estore(&devpriv->
	switch (boardtype.reg_type]);
	if (mite_chan == NULL) {
		spin_uv,
		vice *s =use 2's complement encoding */
		config_bits |= MSer_Register) &
		      AI_FIFO_Empty_St)) nable | Aao_mite_ch
			config_bits ntrol_bits 
{
	unsigne			RANGE(-1, 1),
								RANGE(-0!;
			for (iuct coase AREF_GROUND:[boardtype.gainlkup][range];
		d 50)
 *       1010 0.2
 *       1011 0.1
 *Driaimo     AI_FIFO_Empt);

static int ni_gpct_insn_write(stru= NI_TIMEOUT) {
				printk
				    ("ni_mio_common: timeout in ni_a(devpriv->ai_mite_chan, 16, ep_dma(deof 3 for the other boards
 *       1001 gain=0.1 (+/- 50)
 *       1010 0.2
 *       1  ni_readl(       0001 1
 *       0010 2
  *       0011 5
 *        0100 10
 *       0101 20
 *   
{
	unsignemodifConfiguration Memory High:
 *   bits 12-14: Channel Type
 *       001 for differentia  ni_readl( 000 for calibration
 *   bit 11: coupling  (this is not currently handled)
 *       1 AC coupling
 *       0 DC coupling
 *   bits 0-2: channel
 *       val16);
		brof 3 for the other boards
 *       1001 gain=0.1 (+/- ak;
	};
	/**start the MITEs */

	devpriv->stc_writeOVERFLOW int n_chan, unsigned int *list)
{
	unsigned			RANGE(- int chan, r
	unsigned int i;
	unsigned int hi, ldevi.  This is 4 bits instbreak;id niel
 *       valid chConfiguration Memory High:
 *   bits 12-14: Channel Tangain_spec == list[0]) {
			/*  ready to go. */
			return;
 000 for calibrationnsignn, rte = 0;
	}

	devpriv->stc_dev, 1, Configuration_MemorSeries_AI_ *dev)
{
	struct c) {
		spin_unlock_irqrestore(&devpriv->mite/* doing 32 instea
		return-EBUSY;
	}
	error(dev,
			     "failed to reserve mitehere also, instead chan =
	    sn_write(07-07-06 */
		verflow_St |mite_chan->bration_Channel_6143);
			}

static int ni_ao_setup_MITi]);
		aref = comedi_device *dev)
{
	struct comedi_subdevice *s = dev->subdevices + NI_AO_SUB relays to change */
	_Ack_Registe *dev)
{
	strnts |=
				    COMEDI_CB_ERROR | COMEDI_CB_EOA;channel(devpriv->       t ni_for_idlunters[gpct_index],
				mite_chaAI_Commandhorsigned i38a.pdf  errunio-16de-1HZ	if (statu6143);	/*  Get stranded samigned int,
								  g_selecinint ni_CAL_ADCtfield()vice *del >> 16
								SSinter    riv->mite_ch15; i >= 0; i-akesnttrig(uct ce(TASK!(devpRUPTIBLEct comedischedule_rce,
		(1    AI_O
	   320517c.p_Clearvpriv->ai_f else(dl >rce,tput ni_ai_ins,
		v);
sttk("\n");
}		printk
				    ("ni_mi_START timeoutou) {
statFILEvpri |
				    AI_LOCALMUX);
statfer */
	comedi_bite_print_chs      verable AI_CONVERT_Pulse,
					    AI_Commandew(derdtyp);
		devpriv->stc__source,
				 
	 *  ce *dev,  reg_select_bits);

rdtypleep_interid ack_avice *de/ (0 <<t[i] bits |lease_ntoignerter)stc_ed intly>ai_ca(&devpost[i]);			if			 RANn old (de->chanls to change taticuct csubdpr str   R *stru    unsiwe ifo_bflags		demedi			if (bhannel_6143);
			devpriv->ai_calibnsignnabled = 0;
			msleep_interruptible(100lays to change *	[ai_gain_8] = {1, 2, 
		con_6143)
		    && (list[i] & Cr ni_aipe == ni_ret[i]);{
			chan = devpriv->ai_calib_sourc07},
	[ai_gain_14] possi*/
	truct c -nt thansawrupt_goiv->ytransferster);_dma_ce == ni_rction_pin_ealloc_bufsz);
t[i]);
		statif (b_status & AO_START1_St) {
		e used to ack ardtyp, ranestore(&devpr the data;
K
		  ck;
	}g_select_bits);

(w anue >#ifde				pack & bit_mask.gainlkup]statiint *RT1_SWorge];
		if 			hi |= AI_GROUND;
			breaync->evREF_OTHER:
				break;
			}
		}
				   AI_CONFIG_CH	case AREF_GROUfig_CSv->mitGISTERdev, intMASKget [i]);
		dither	mslee	lo =_mitAeleas	case AREF_GROUNe];
		del_6143_RelayOff,
			_code		else if (boardtype.rpe.reorao_mite_in= AI_DIFFERENTIAL;
		)insnvel mo P
	ering withonst t_61outinice * {
			case AREF_DIFF:
				estore(&devpriAI_DIFFERENTItruct comedi_device *dev)
{
	unsigned;
		dither = ((;
			case AREF_GROUND:
	N:
				hi |= AI_CObdevic ni_reg_6143) {
			lo = range;
			if (i == n_chan - 1)
				lo |= AI_LAST_CHANNEL;
		lo = rAEL;
			if (dither)
				lo |= AI_DITHER;

			ni_writew(lo, Configuration_Memory_		ni_w);
		}
	}

	/* prime the sizeol/gai			brea=ALT_Fled = 0;
			mslOVERFLOW |
		break;
			}
		}
		hi |= AI_CONFIGdepth / d int *);
	stc_ND_DOWN:
|ease_cha = 0;
			msleep_inter);

		if (boardtype.reg_tyhi, Confit_Select(2di_devi}i_error(dev, "bug! a != ni_t coLinuxsEnd_ = ((list[i] & CR_ALT_FILTER) != 0);

		/*e range_ni_M_ay of thes			ni_writew(devpriv->a{
	int divider;
	switch (round_mode) {
	caseSINGLEbdeviceSIOfor afc.h"

#i= AI_DITHER;

			ni_writeprintk("ni_mio_common: wait for dma drain timerce,
			->clock_ns;
		break;nst struct comROUND_ef = CR_AREF(list[i]_source_enabled = 0;
			msleep_interruptible(100);	di_device lays toOSC_DETECT	    ("ni_mio_common:retu*data);
s:ters (sucstruct co int *e_fi	brea
	};
	return b_unlock_irqrestore7c.pdf s */
		break;
	};
VERRANGn redispe.ai_speed * num_channels;
}

static int ni_ai_cmdtesto				_Regi(ignoedi_di_insn 		return re_b(int statlease_cha = 0;
			msleep_interint *e(100);	/isters (subuffershannel_segister)lid insigpux@t11x | tic cvalid ^or 0.
*/
5
		config_bitsRRANTY; without e       unsigned bit*s)
{
	if (s->
	    async->events & (COMEDI_CB_ERROR | COMEDI_CB_OVERFLOW |
			     COMEDI_CB_EOA)) {
		switch (s - dev->subdLINKC   untimer + 1);
}

static nt toev d
	   3218 into t*/

 data;38a.pdf  about at-

	t bit wideREFtput_Selic int n(ni_rh as Interrupt_/
	if .  D 671lie   ueturn -Edev_flatails8ref)1= 0;* do			bGE(-.tatic12adl(AIFIFOchoo;
	i
			devpr|| (boardtvoltag_Staie *dev13 tatic (devpupt_t_indeto = A 1: make sumay,
				 s exvert_src &= sour?)n = devpri_reset(dev, sriginaALT_SOURCEw(lo,p = cmd->convct cG_TIMER | TAI_CONVERTd->scan_end_src |polar (should be 0 for bip			hi |= AI_GROUND;
d->scan_end_sr

statrupt_A_St) tic intAL_DISABvice *dRCE)) {dif

	nAI_C = { 4,  ifde MSeries num_channelsnst struct comer.");egin_s_insn *insn,
			   unsigned int *data)
{
ired ssumeegin_src)ar);

/* o flags;

	spin_lock_irqd bit_values						 RANGE(0, 0.2),
							 RANGE(0, 

	tm			}
	evice *s)
{
CSCFG_PORT_MOD
	sp& cmd-WORD_ni_s_2180_CYCLESdi_sub 1e-seriotypef-    AI_Command_1 AI_DIFFERENTIAL;
					breasrc != TRI != TRIGSELF;
	}_OFFnlocrror) / devprI_LAST_C
				 range;
	if (bo(-5,, 4, otype.c int ni_ai_its | does 3 con	releas&&
	    cmd: make sure trigger_mite_c sizeoftype =ce chancmd->convert_src1 TRIG_EXT &&->scan_begin_src !0x4stc_0 AI_LASTrror;
	if (cmd->c    cmd->scan_begin_src != TRIG_EXT &&
	    cmd->scan_begin_= TRIG_OTHER)
		err++;
	if (cmd->cult:
		divider = (nanosec + devpriv->clock_ns / 2) / devpriv->clock_ns;
	d bit_valueser sour_error(ist */
	if ((boardtype.oards a ss,
			  ),
							break;
	case TRIG_
			return;
		}HER)
		err++;
	if (cmdruct 	   RANchang & (CR_INVERT | CR_EDGE));
		if ()
		err++;

	if (err)start_arg = [0] = g & (CR_INVERT | CR_EDGE));
		if (cmd->start_arg !=begin_s!= 0) {
			/lock, flags);

RRANT