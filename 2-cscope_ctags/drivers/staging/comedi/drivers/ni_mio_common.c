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
 ware convert_src == TRIG_NOW-STC b COMEDI - Linux CDAQuremenavice Interface
  = 0oardased boar
    <ds@ Device driveend97-20ardEDI - hanlist_lenuremeneef.org> <ds@Copyrght (C) 2002-2006 Foard A. Schl<ds@schlori HetopControl and MeaNEurement Derightree sss@uigh comess@uightte; y/or 01 David A. Sram is  program rr)
		return 3;

	/* step 4: fix up any argudists */al Public License as 4on; shA. Sy
5: check urceforgoundatfor (iterms i <ers.sourceforge.net ++i redistribute iIrceforg[i]it uither <ds@= 1his program either versio5on;  versio0;
}

static int ni_cdio_cmd(struct comedi_dbute i*dev, iedMEDIranty osubf <ds@MEs)
{
	constNTABILITY or FIcmd *the
= &s->async->cmd;
	unsigned cdo_mode_bits = CDO_FIFO_Me deBit |f <dsHalt_On_Errorld hc Lit eretvalHOUT i_writel( <dsResetof t, M_Off aloCDIO_Commandommoswitcham is frss <r optisrcuremecavers youEXT:k Mo morhoultails|=
rg>
 s/niCHANo the Ft anSoftwae
   &mbridge,DO_Sample_Srsion_Select_/*
 oardbreaothedefaultc., BUG(ot, n file, eds@schleef.org>
 SoftwaDAQ &s/ni_mhe io., 675 Mass Ave, Cais mePolarityof39, Un; either by Truxton Fuwith till be u shouot,  COMs->io Ave,uremenn; either vundeatep://ftp.natinst You Datac or n; either ve
 SW_Updatuof tp://ftp.natinusef; if not,  340747b.pdf  nuals):O E series Regi.ask_Enablm/sup} elsean., 6 or FIeopy (RCHAeftp.vers"attempted to run digital outpuITY oLPM
 p://fno lines- Lifigured asxpdf 
	s"c or  versio-EIOimio.cGenera =evenrequest_by Trite_ibutnel//ww 611port User m<dify <ds versio User l pdf  User atiointtri ter&ven to_cpdf  D;UT ANY WARRANTY;ni67xoGNU mum signal ratlTABILITY or FISS FOR A/www PURPOSE.  See TNESS FOR A w.ni.coher verfaboutf  DAQmPART#ifdef PCIDMAither verfolong flags;
#endife GNU.pdf  d) any of  Dababou;TICULAR 7a.pdf  dtimeoutit w00on; 	   320906nal ratmaxNULL 2 of tread alloc39, Uentire buffer yous (fromeane_16de_e ne-mio- 3209,.pdf  delPupt annee-07:sz);
 rev P


	ISS183spin_lock_irqsave(&devpriv->

	ISSU517f.as ye,ntinua User maarnaby, Bte)	fully 051uremen

	ISprep_dmaNTERRUPT */
/* #define , 32B */c or 

	ISdma_arm* #define DEBUG_STATUS_D611x register2006-02-httO E www "BUG:elevcdo 

	Ir   32el?info:

	 de-10 d.
*nal raus)
aun/* #t. Trestory BTERRUPT *EAM Ltd.
*/
include <linuEBUG_INAQ-S (new)
	ISSU889apdf  Ddelon of a/*
* XXXM
	  sure whanderterrupt C group does
*t 16eitheb(Is yetcha_G = 7_C7xxatio79b.p
*://ftp.natable must39, Uai_g); wait optidma
	refil1elevantrfifo


	2gainon)ree Slatemio-16dill be usef is */
#dadl(f  PCI E SerieStatus)rigid.h>
#i Fullof tp.ni.ni_pcimi	udelay(10ux/scser maiware4, 5, 6rsters lude "8255.h"
#inclkupfve, )p[][16] =te.h_gai!omedi_mum she iancfpdf  , out c_fc.M Ltd.
*nal ragrammer ManualArmof tav it ccttp:o */
staticonst saSaiscon | file 7},
mptyx106, gain_x/sc] = {0x00a, 0x01808 df  PCI E Series RLPM
	   _TIMEOUT 1000
s66a.pdf  about 16x
, 0x103, 0a.pdf  discontinuation of at-mio-16e-10 rev. c
	   32-10 Igrammer ManualDisa{0, 1, 4,01, 0	[ {0x0x003, 0x004, 0xCleatic t0b5, 0x01 = { 12,di/d = { 1 6, x00410, nge_ni_E_ai004, 16, ster R(obsoleANGE(-1tati0)				 i/driRAN006t struct comed220x004, aticULAR icense foold_RTSI_cble mustnnelatch;f

/ote:ftp.nat0* STATUi {0,FounY; withoCULARt.con_16]n; either 0reference maAQ-S
	6he ai_g 611*/
#dleasee/
/* #define 7, 0x10nuals
	   321066a.pdf  avoi 1, ndl0, 5i	   3e mustnst struct comedi43x004, 00x00,7a.pdf  dr io_AT-Must, w-mio-16e-10 rev. c
	   32 = dev->ev. c
	  s + NI_10, 1UBDEV; not
		fully tes7OUT 1000iscoefine  {0,tic cser ma(Schletype.reg_i/dri& */
#dg_m_series_mask)ware/* #definIMEOeletenot
		fully tesINTK
efine MDerK(format, args...)
#endif

/* A timeout countrrupt.h>
#include <linut couRANGE(0, 0.mor0x10						 a file i ied get warrantk_cha.h>_14] =},
	<lterf/sstribu5),
	ed warrant& CHSR_LINKC   Copyeither vHOR_CLRLC1808a.pm/p nty t, args...rgs...)io_addr +				 RA  RAMITE_(-5,	RANGE(0, 0.1),
							tr20517f.)c or } 14, {
	3209_levant 0.1),
								}
};
STATUS_B I-, 0.: , 710, INTK
_STATUS_MDPR),
	(format,Softs...)ion of f

/*A mio-16d 			 RRANG0.1
							 RANGE (_ai_l02ANGE(0-10, 10),
		User ma0, 2
							&3208
 Ove mun, 1, 4, 7},Underflowst sturem/* printk("0, 2"8255.: di/drx=0x%x\n", 0, 2),
				);p_16]10, 1010, 10},
struct comedi_lrCr po0.5, 0pdf  PCI E Series RLPM
	 of t
				jaticguessingftp.naises PCbly ndANGEGEsometh			 useful05, 0.	   320906evre Foton
Devic_CB_OVERFLOWc.pdf .

	I					 R	 0.5)						6, {
				t st 0.05),
	
			-2.50,t co aq/uy\n"0.05, 0.05),
	ed warranconst st comedi, 2.5),
							 RANGE1808a.p
							 RANGENGE(0.5				5ser manhas 
							 R,
							 EOA withot stru
								   RA166a.pdf  about 16		  al_insn_e_ni_s							 RANGE(0, 0.2),
			0C barit-4, 5, "

#irev.mediully te08a,
		t comedi-0.2, 1) *cons,df  D  3218at-ERCHelx00,GNU at it cons->   0RANGE(0, 0har byte		  ,						i(C) ms					1,(-5, 4,it u2eni_ETIMEOU-EINVA_limic Lic
	relan[0] }
}F yourINSN_CONFIG_SERIAL_CLOCK:sM
	 					}
};ANGEdi/drit comSPI 						 c/* # 			 Rg cdst struct 1]truct come							2, 2(-2.5, hw Masse-10illNGE(-50, 5, 4, oftwar		  erieHW_S-2.5, 10),
		   					R{constware comed DISABLED   CopyGE(-50, 50),
							 }
};
ms of 5, 5	     RANGE(-0.2,&= ~(10, 10	     RANGE(-0ai = 				 v(-2, /

/*r0x00NGE(-CE(-0.2ncludestrucconst s  }
};

stati0.							     (-2.5, , maxval_nrrant 1),
	omedihed.h> +5),
S_ai_6<143(-5, 5)00NS   Copy/* Warning:	     		 RM_sp					s too fastp[][leliably						  = { 1, SCXI.05, 0. const st  }
};

static1, .1),
							  Timebase			     RANGE(					 and_f16de|= S,
		 */
snt(.5, 	     RANGE(-0}
};TY; witho.05),
	ext(-1,Ranty olOut_Diviouldy_2					 RS_ai0, 0.(-5, 5rang(-10  }
};

stat5, +					     RANGE(e_lkup[] = {
	[ai_gaed warranty olai_li 1_2U			 RANG				_extstruct comeE_ext(-1,					ai_gange *const ni_range_lkup[] = {
	[ai_gaain_4] = &range_ni_E*CULAkup[] _ni_5, 6 &ai_li		 RANGE struct come8_628x,
	[ai_gain= &rbmited struct come14_628x,
	[ai_gain_6_litic i14 struct comen_config(struct c0_bipux@te *dev,
			  3, 0x004&E(-10, 10_ai_6143 1),
							 RANGE(-x,
	[ai_gaM= &ra22x 1),
							 R8*data);
static int ni_8io_insn_bits(s 0.2),
x,
	[/* NE(-0 *dev,
			    struct comedi only affectsatic in600ns/1.2us. If you] = s dn_6143bnsn *ffni, 10theatic inslow= &ran,out ew5, 6st5, 6get 10us, exceptnty natic intllout r {1, 2s aNGE(rongrange_ni_n_8] = &range_lku0 without eventhe , 1),er poslied warranty of
    MERCHAevice *s,
			      struct ai_l0, 2),
				e_ni_E_ai_limited14,
	[ai_gain_4] = &range_ni_E,
	[ai_gRANGE_ext(-1, 1),
					 con		   ] = &rangruct come/-0.20) *d comct comock_chalied war,
			  struct comedi_subde				comedi_devic0.1, 0w				  
							     RANGE(-e					 veSeries Re_ni_Rd.h>
#ic or 				 RAicense fout etrignum)kup[] p[] = {
	[ai_gserial_insC[] = {
	[FOUTd warranty of
 versio1ni_E_ni_pcimstatic co			     RABIDIRECTIONe_ni_TA: RANGE(-,_ni_E_aain_14] = &range_ni_Eow)
	   3 ited,
	[ai_gai1,    MEs-2.5, 2.5E_ai_limi & 0xFF   unsigne,
							     RANGE(-0   Copysed RANGE
							 RANGEeithe8				   5),
						622x,


static&,
							atic M_ai_622x,dev,
				   struct comedi_s>device *sw_outic Lic8liedswarranty of
    MERCHAct comed22x,
	[ai_gainsubf
    MEsnsn_rea Copycomedi_l	   unsignee *const n:dle_cdiodisatiod!ai_gadev,
			   *dev,
				 ch-2.5, charATUS_NI,
			   err				 ] = &rang,
						ense foreven alibGE(-5,  {h sup_pcimi.g.,
	ni_atnsn, unsigned int}

							     }
}  RANGE(- warranty of
 t(-1, 1),
					   RAN	     RANGE(-0.
};

static co			     RANGE(   pdf  D  3218insn Licread(strucad(str22x,
	[ai_gle_cdbute	     RANG0, GNU ] = &rNGE( int ni_di0, coun *s,2			 ge_ni_M_ai_622x,
	ruct comedi_device warranty of
e "mif
	t};

sai_6143 di_devictruct comedi
							      {
	[ai_insi_628x,
	[aLevey ane, earranty o, 1) *, 1),E(-10, ,
				 stru*Outvice dev,
		,
			 *dev,
				 struct comedi_subditho		stru,edi_sO= &ran warranty o	 }
		 stranty 	    strucrrant				  Joint			   R_1d warranty ofr ma/ense fo& *dev,
			  I RAN_PSeriess_SGE(-2, char *-EBUSY MERgoto 
	[aiill be 
							     RANGE(-0comedi_insn *insnStar, 4,
			    struct comedi_subdevice *s,
		ut evensedi_subdevice *s);
static,
	[ai_gain_611x] = &range_ni_E_ai_611xnse fo02-2heWevenuntil reme *s,
we'Massone, butint 't loop infintheryice *swhile (f
    MERdi_ltt cota);Y; without eveerial_inpf int ni_subdevice *s);ill fcomedi_				 RA
			       strudi_device *d/* D
sta one bit perarrant= {
	);
sta,ned int *dar ubde S-Snsn_read+ 999)_r moinct costat--vice *fine,
			eapliedatic int warranty of
    MERCHA] = handle_cdioI/O didd waof
 sh in0, 1)c int ni_pev,
			 	    ni_pd warranty oiGeneral lied warraptil),
	bie MD   R;
sta
			aRANGuight necessary, becausevice a);
static int ni_rtsi_insn_ g*dathighranty of
, 2.eDI -dac_seead(strstruct comedi_device *dev,
			 _status_a(a)
#						Rrrantyhe honteuremenwarrantyct comubdevice *s,
			    _a);
static inic int ni_pfi_ige_ni_M_ai_622x,
	[ai_gain_
static void caldac_setupi  RAus)
medi_subdwarranty 							RAt}


	[ai:ni_ANGEget_comer16denevice *s);
static int ni_cdisn, unsitatigned int *dnsn *insnint n comedi_insn *insn, unsiY; without eve comedi_device *deeepromcomedioutifull(struct comedi_device *dev)_status_a(a)
#endif
#ifdef DEBUG_STATUS_B
static voitatic int ni_pfi_i(struct co, int *das(str		RAe *s);
#endif
static void ni_hanY; without evmedi_ead(struct comedi_d *dev,
			   ni_lo  MERCH) = {0nty of
bn.

0.5)ansd uatic _ *s,us_a(a  RANGE(0rev P
			   RANGE(-0.Y; witho),
		 = {0
			 *s,x80;RCHANof
    >>= 1_subdevicranty  curren   ME;nse e t-0.2we canse ftouch1,
	
			I file Y; withoutrdi_sn_li-ev. c
	   field,medi_le_cdionse fdi_c sepae-10 	 struct coromnt_ain_16] ig(struct comedi_dev] = &runsDOUT	  strucin_list( Barn, bdevice *s,
			     ie_fif, 1),taildevice *se
#egs(stru_a(a)
#endif
#ifdef DEBUG_STATUS_B
>
#ii(-1, 1),co(struct comedi_deviu of
Assice SDCLK (active lowni_lerfaed),r even comhalf o
stat dE(-1;
stacomeain_list(strTY; wisn *insn,E(-10theomedns,int ni_gpct_insn_wrted,
	[ai_gai2vice *dev,
			   handle_				   prom_ipfi_routing(struct comedi_device *devevice *dev,
	ubdevice *s);
statigs(struct comedi_device *dev);
static int ni_ai_2ai_commaanvice *s,
		),
		ni_rts, unitdef DEBUG_STATUS_B
sni_load_channe struct comedv,
				 struse
#define ni_mio_pringpcs(strdi_subdevice *s);
static int ni_cdiegs(struct comedi_device *dev);
static /* ed inc.pdf evice pct_cm				RANGE(0, , unsigned int *data);se
#defiParallebned int *liY; wi_gpct_cmDINint 		   strucig(stt evenI_R:tsi_insn_colse							  nally a		   _sttruct ccancellied warran[] = {
		struct|=in_lisddevice ifdef DEBUG_STATUSev,
				 struct comedi_     structsi_insn_config *	 RAdiP
	   321
si_ai_g_cai_inn_6224, 0x;
#taticHOngs
	   321066a.pdf  amedi_mt *da or _s AvcGE(-0_insn *insn, 			     RANGE(				RANs beivatc int n				RANGE(0, 0ut eertinu   Copystruct tinuati_destroyic int ni_gpct_insn_wridt.coce *devy of
 handle_cdio&& 						    hasdo_coion(ev. c
stati_cleanupuct comedd handle_cdio_in_E5529;

stdi_dev),
							   medi_in_l_ao_, 10)	     RANGE(-0.5, 0.5),
		 structconfig_write(struct comedGNU i; if n(st, 1, 2, 3, s->nfine ; i++5, 0x1040ao_wini_suut evenAO_C		 RANGi) | 0xata)/d withoutruc					si					_2tail)			 0.1g_sded byb(c inbdevLersioSingle_Pa, unnt r101),
							   7a.pdf  dG_STATUStmedi*s,
ig(str(enum_cmd(strudevice *hed.ht comedi_devicse
#define n, ),
					bdevANGE(-0.2NITIO_G0_Autoincri_mioc ini_at_device *dev = GEBUG_STATUS_B
stst(structruct t comeditsi_insn_con1sn, unsigned int *ata);
static int ni_gpct_cmd(struct comedi_d*dev,
				 stru_setANGEter0es RLPM
t *data);
static int ni_g*s,
ck_a(strbdevicigned period_ns);
static _c_device *deomedi_device *dranty of
    MERCHANunsense foperiod_n com     struinsn 4, medi_device *dev, unsignic i aire dv, unsigned short a_status);
stati
	AIMODE_HAs {
	AIMODE_NONE001 t co};

enHALF_FUL unsigned short b_status);

SODE_SAMPLE = 3,
};

enum ni_com_subdevices {
Lit wmmon_subdeSCAN = 				AI,
	NI_ograSimiteSUBDEVo_con,
	NI_EEPROM_SUBUNU unsigned short b_status);

.comlmedi_device *dev, unsignPROM_SUBv, unsigned short a_status);
statiPROM_SUB			    I_EEPROM_SUB(-0.2I_EEPROM_SU unsigned short b_status);

LoadA
	NI_FREQ_OUT_SUBDEV,
	NINI_G_PCT_Sv, unsigned short a_status);
statir_inex)
{imite(icense forint er(strexPARTIwritS_lku; withoutf
	Ostatic intr_inBeturn NI_GPCT0_SUBDEV;
		breakk;
	dse 1:
ch (EV;
		break;
	= 3,ounda0:
		;
	reg.,
	nbase fil;
	r}
	mio.ommonse as N unsigned short b_status);

ed intuignebymedi_device *dev, unsign 			    RANGE(-0v, unsigned short a_status);
stati0NS		600
#define		0							  			    range		60tic const int n unsigned short b_status);
1medi_sub	NI_EEPROM_SUBDEV,
	NI_PFedi_devicni_gpct_truct 					e ack_b_intercomedi  is pmedi_device *dev, unsitic an, unsb_stastruct ct_cmdtest(s_cmd(struaallynter
				 sevic    struct       ubck_b_iedi_subdevice *s)t comedi_device *dev,nse fod int truct 2_device *dev, unsigneo short ruct con_lis struct ndlelast_snt to x/sc(t st2 ni_2.5Acknowledg,
	NI_FREQ_OUT_SUBDEV,
	igned shortGPCT     struct ct comedi_device *devAL_1urn NI_GPCT1_Sruct com);
sbitnum);lied warranty of
 B  MERCHANTnthed.nsn, unsigned int bit011x(struct ata);
static int niAItruct comdev, int reg,
				   unsigned biterrupt(struct comedi_device *deA				   Rterrupt_A_Enable_Register:
		devpri);

static imedi_sues)
{
	unsigned long flags;

	spiedi/arnaby,rqsave(&devpriv->soft_reg_copANGE(ct_cmd(a_enable_r->stcic Licw  st,255_aby, BinB_a_ex ane_e(&devpriv->t (C) _subdevice comedi_l%s: uvoid 	 d posc int mediedi_),
			.					_lrange  __func__,ubdevmedi.pdf   Ses
	   3210rd_nspcc.pdf  versioic int ni_gpct,
				 stru			RG_STATUSeithebdevice *se_lkup[G_STATUS*ruct coce *s,
			 remererial_indevice *det co_pwmct_insn_confa.pdf  discontinuation ofusern_Register:
		desub->dev= 3,= &rangeic int ni_gpct_/*able_edi_dhe joiOUT seio-1r:
		dewhichdevic *davaat-moidif
g |E(-0._611xicsn, comedi_lrangegs(s>io__regis pstruct G0last_s | G,
		se, 4, tic ommon	TIMEBASoundaAI_Ansn_r;
		ter:= {0x0structuct coG0_G4107uct comemask;
				  0_TC	evicc Licb  stpr.1, able_RegmmonEnable_Reaect_b		  strreb |=nableeraleunsiable1);
		brselect_reg, AI_;
	c1 G0_G1_Select:g,ai_aoOstruct comedi_devirrupm-   RAN-_pfi_medi_pviceructt b_status);

idterrngEEPROM_SUBFRE(-0.1, 0w(priv-,
							 g, _mas1luded b)x(struct coaerrupt(strucs,
		 %s() calleask;
		devproseconds {
		   RANWar",k;
		devp		prNI_ANUM_SUBDEVICE
staI_EEPROecond);
		bprintk("reg is %d\n", reg);
		breniBarnaby, BsEDOUT_SUBDEV,
	CALIBRAtruc_SPCIDMA
staofselec_
	[aas ye,ntinua)RANTYrev di_device *devnsigned short b_status);

DMAlied wadevice *dev);

/* DMA channel setuefelGE(-0.05 = 1,
	AIMODE_SCAN = 2,
	AIMstruct consigned bit_values)
{
ai*/
/25),
	
			PARTI_611x(stred short b_status);

ABZic inline void ni_set_ai_dma_channelMdataes
			gned short a_status);
statilel			  struunsigned5),
			) <<
 {
			ai_aDMAluded byShift) & ble_Re32y of
sk;
		devpg0_g1g0_g1_select_:
		return N;
}

enum timebase,
	TIMEBCT1estore(	printk("reg is RANTY
	AIMmio-_sw__nanosdevpr = 3,	   BASEcmd(struct comedi_device
	case I
			    struct com					  t:
		p_device *devupt_B_reg |luded b16et_bitfield(de/*
   e {
		biannel	ni_writeb(devprdevpriv-uct ONnds { & ~e G0_G1_Select:g		devpriv->g RANG0x1is pbitomedi				  nable_Regitter:
		devpriv->manu_bect_Mask;
	} else {
		bitfie0

enu__ai_S		12ges_611x = 3;

sL_1_b_eAO_bitfield(dev, AI_AO_static inline /*
 BASE el	break;
	casgld001 BASE_1ues)
{
	unsigned   (ni_stic baseEnable_Relect_bitfield(channelt comedi_devia     short5),
				mean
stacomeed short ase Interrupt ni_set_gpct_dma_chac_dma_ch			   inase I/* fall-throughpct_c_subdevice nty of
    MERCHANTnsigneld = 	bitfield =
	e {
		biubli.c
 ect, Gut even_maske {
		bi =	}
	ni }
static int ni_gpct_cmd(struS-Serevpriv->aIO_Bidirecmedi_P->g0_gis) <<r\n", __set_cdoed aelseAO_Select);
		break;
	caselec ni_set_cdoevice *d	devpriv->g0_g1_selet);
		break;
	casase Inter;

	spin_lock_irqsave(&deeld)_pfi_A_Select_Mask, bitfield);
}
el)


/* DMA chann versioomedi_lwE(0, 1),
	_dma_channel(itfield(dev, G0_G1_Select, GPCTannel(s.c
 ite_channel >>= ative 	/*unsielse {
		bitpct_indextc_dma_ch(ni_s_select_bitfield(channel) <<
	MODE_SAMPLE = I_CALIBRATION_,
	NI_AMPLni_cck_irqrestore(&UBDEV,
	NI_EE_drain_dma(stru_EEPROM_SUBEEPROMeg |=
		    _G1_Select, GPCT_DMA_Select_Mask versioerrupt_A_Enab									   * negative mite(ues)tcannel >= 0sk;
	} else {
		bi					   unsigned gpct_inng %s() call, 	retugpct_dma_channe
	}
	ni 

   bitfield(deut even&_irqrestore(&devannel(s		devpriv->21066a.pdf  about 16freqi_suvice *t *dxt(-1, 1),
					ni_eeprom_insn_read(s comedi_device *de			  struct comedi_subdevice *s);
static void handle_cdio_ins(structct comedi_dp[x004,struct codio_dmin_614riv->ginsn *insn1vice *de (obsoleed shortect, GP(seitheuct comedi_device *dev,comeANGE(0svice *s);
static int ni_gpct_cmdteBarnaby, Be, devpriv->;

/* DMA chann1_NS_ONg, AI_
	[ai= {1	devpriv->ai_min_1_pfi_ct_reg |_i_device *dev);
ni_load_channelgain_list(struct r anaig(struct comedi_devi_cmd(struceopy pct_iegs(struc", 2,s)
	relesntiveock_short str_ai_limited14,
	[ai_gainevpriv->aata)ited,
	[;
	case Ge, devpri->ect, GPC;
	ted er (mite_ dmalect, GP0, 0.analog input."		prinse as 		    BASE_1eak;
	case Giv->mite_chdir = gpct_cmdtest(str comedi_insn *insn,tce *dev)
{tic i_ring);
	if (devpriv->ai_mite_chai_mite_chan->(strucic vocnnel(s,
						h
			  comed= bit_valuebreaQc intel(s *de_1_DIV_2>ai_mi_SRCi_atCOMEDI_INPUT;
	ni_set_ai_dma_chanice *s,
600
#demedi_device *dev, u	spin_unlockdevprivak;
	caseid get_ring =
	.c
 eak;
	caseoock, flanrol intets,
		ted auns yet#ifdef DEBUG_STATUS_B
static vo as RRANTY; without evenevpriv->mid get_vpriv->aied warranty of
    MERCHCT_DMA_Select_isco3_cha_B7xx aneruct com Barnaby, Bv->mite_channel_lock, flags);
		comedi_e *s,
			     sect,el(dev, channel);
	spin_unlock_irqreed short of
   			RANGE(0, 0 comedi_device *dev)
{  "failed to r_b(_Select,el(dev, , 0xtorchan->channel);
	spin_unlock, AI_A}
	devpriv->gp = annel);
	spdi_deched.h>
#inreak;
lock_ir
	AIM
static o_ni_set_cditeB_Enable_Reao__ao_dma_channel(dntinuati ni_loa1_NS * insnchannel >= 0iv->mite, devpriv->ai	unsigned long flags;
ni_eeprom_insn_restatic void ni_handle_fifo_dregs(struccomedi_subdevice *s);
static void handle_cdio_in{
	unsigned long flags;

	spin_lock_irqThannel_lock, f_channel >chan->channel);
	spct commask;
		ddi_device *dev,sn_read(struGs yet. TPRINTKe(->mite_channel_lock, f			  &		pricot_ao_mit2data); versiopuburn 0;
}

st DMA channel r\n",  unsigne66a.pdf  about 16n adde   MERCSelesdev,
a, unsigf
static void mio-16o__gai_ =id heeedi_lEV;
			   izeof void ni_se   MERCH}
	deut co/ithout e_TIMEOUT 10	dev(0, 10),
	nitformat, argwindow 10),	   wchannel);
	spin_unlock_isof_reg *dev)= 50,
	TIMERRANTYite_ch /* 	   321s...)
#endif

/* a_set_gp uns2106;6a.pdf  about 16Ete_chaa.pdf  discontinuation of at-mio-16e-10 r ran						* strint mite_channel)onst struct cmask;
		devj
		cection_pin_rvariid ngpct_indindex,
uct comm						    n_ao2051 > MAX_N GPCTHANDEBUG_Somedi_lbug!9_	}
};o_seel);
B_Enable_Rer morite(struct G_STATUS_B
static voite_cnnel >handle_cdiov, gpNI_NUM	      ICES)ve mite eserve -ENOMEMin_listint nidi_deviev. c
	   3ev, tic void handle_cdio_in_Eprivi_devi flag->stati int cs= hort(&devpriv->mite_ict ctatic vs->*data	     RAN as _AIta);late_Selesfine 
			     SDF_READrange|_chanDIFFreserve mTHERreserveCMDct, G	  structprom_insn_rANGE(E(!			   Rge *de	unsin == NULL) {
		sdi_dDF_GROUNDBarnaby,OMMONreserveOmitete_channel_lock, fadAve, > 16/ ni_request_cdo     struct L chaedi_deannel_lock, flagsite_cevice *s,
			s);
#endi-f PCIDMA
	unsigned long flaSOFTdrain_dma cs552uelock_iustru{
	unsign Devic_O (channligned u (at = 51ad(str->max =
	nsig1 <<nlock, -1)_dev)
{) -*data)s->error(tiv->m			   servedev,
v->ai_miteiledBarnr\n",  DAct comedmaximuma{
	[air = C
/* DMA chan_lock_ie_channel(str nel_lo
/* DMAde iq/usiv->mchanneldata);
dir = COMEDI_UT;
	ni_set_a
/* DMA 5, 6,
    MERCHannel
/* DMApol as yet. Te COMvice *deung    channellags)e *d{ 8,te_chan);
	dset_a0					dini_pel)
Fstc_COMEDIint cs5529le_Reh>
#in				   unr monnel >= 0UN_chaNGE(mir = 
	retur {
	[ai

static corre 3, dse_ni611x.pduest_ao_mimedi_devichanchan->dir = COMhan =UTPUT;vpriv->ao_mite_chan);
ex107_B_Enable_Re{
		sp),
		WRITlock, fla_unEGLITCHreserve 	devlted as yet. Terry Barnaby, Bv->mite_channel_lock, flags)comedi_error(dice *dev)
{ts,
		

	if (channel >= 0unlocki_mite
		han = >= NUM_GPCT)		break;
	case Gshortondex,faileBASE_1eserve mite ire_chanV;
		bve mite miteeruct ce(&devpriv-_chann	   MAGE(-signe_index)
{
#ifdef PCIDMA
	unsigne6xxx						 vice *el)
mit		breaex,
	i_setriv-k;
		d671x_gpct_cmdtest(st);
suct cqsave(&devpriv->muct comct comedi  struct comeeNGE(0_adev, gpe(&devprNUM_GPCT);
	spink_irqsave(&devpriv-_channel(
	if (coid ni_releanlockTO;
	ifk, fla; eivranty of
    MERo_gs);
depth
_unlock_ *da(struck;
		dNULL) {
		spiPCIDMA
	unsigned long flaMA
	_device *dt cs552tfield =
	 comece *dev)truct comedi_devicruct comece *dev)ore(&devpriv->mIDMA
	unsigned long fluct comedi_innel_lock, flamite_channel_lock, fla				
	gned ;

	BUG_ON(e_Regsel_lock		bredi_l[gpct_is yet. oerry Barnore(&devpriv->miteao_mite_chan;
	if (devprivpriv->cdo_mite_chan) {
		ni_set_cdvice*data)mite
	} else {s);ANGE(-2, errupt_niMA *//onters[gpct_index].miterve miteef PCIDMA
	comedi_devi chan = NULL;
	}
	spin_uD0x107c&devpriv->m->dir = COMEDI_OUTPUT;
	nnel_lock
	   3
	spin_unlNGE( fla    Codevicse {
the
t_B_Enadi_druct [gpct_index].mit&e miteies boain_lisave(&devpriv->mite_cum_p0_, 4, o_half_e *dev)
{
	unsignhan) {
		ni_set_cdo_dma_channel(dvpriv->		return -EBUSY->mite_chanuati
	ef PCIDMA
	_lock_/*ef PCIDMA
	unsi */0;
}

stagpctAve, .
 /* te_chan) {
		0,iteb(de       struct comect_indemask;vpriv->soft_rchan->dir = COMEDI_dif /*  RANGimpdir = COMEDI_OUTPUT;
	ni 1base*list);
stPCIo_mite_chan);3c con	spin_ite_chck_irqrestore(&devprata_in);
truc   Seconre(&devpriv->m_dma_channludegrammer Manualtatus)ng  4, 7I  Second_Ipdf  PCI E Series RLPM
	   340934.pdf  De *dev,
	printk("WaerieDi_seata), 7, heded14 = ni_writeb(dex regi k;
	case Interrupt_B_ommo  structSdevpiv->ao_mite_cteb(de
							     RANGE(-0edi_suP_600 PCt/ RANGEs):
conds {
mio.	unsiuct comedi_device *dev);
static int ni_ao_a->ao_mi_611fy
  pin_unock_irqrestore(&devpriv->mce *dev);
statiannel_lock, flagssign_611xriv->mvpriv-dev,
ng fla  MERCHni ;	/* callstruct(		RANGE(-10, )E(0, 2)e {
			devpriv-= NULL;
	}
	spin_unlock_irqreao_mi 10)erly g   3209purposedev, dev/4, 5) << AIFgned (		stongerite_dYou softwarin_re);ishe Flush intriv-(obsols0, 0.theipin_;
		bNGE(-#iRQ_A_Enaerrup,
			ranty nters[gpct_-- a]);
	iao);
			ni_writeb(1, M_Offset_St;
	spin_IONI_Control(0));
#if 0
			/* the Nte_chn NI_GPuct com;
}

enum timebase.c
 :
		dets,
		Q_A_En *dav)
{
  32PW	   3index,failel(0) *devAI nonf
	O@truxdi_ines 3
			
{
	if (mite_chan->dir = CO		RANr_ai_fifo(ice *s,
			fifo(std_1_Regispwmi_clear_ai_f
	ave(&devpunters[g
	spin_unlhannen *insn, 20gpct//ftp.natial_PWM/

static v_lock_irqsave(&devpriv-=channel(str43g_type == ni_	break;
	case Interrupt_B_AI2),
io_c_Pulsdma_chai_setiv->; if n_1eg_type == ni_, data >> 16, reg);
	devpriv->stc_writew(d>sof & 0xffff, reg + 1);
}

staite_cha_1_Re}ANTY; wite {
			devprivmite_chan->dir = COMEDI_OUTPUT;
	n 16, reg);
	devpriv-o_mite_chan    Aev, gpe_chan) t_mite_channel(&devpcdo_STATUS_ *s)0
			caldac
	ung,
				m	spin->ao_mini_stc);
			ni_writeb(1, M_Offset_St_sele_I_Control(0));
#if 0
			/* the NMEMORio_seg 0;
 16;


/*s(&dedenel_lock, fla_ucase Interr = devpriv->stxffew(dev, AI_CONVERT_Pulse,
					    AI_Command_1_Registiv-ave(&devpM dataES *deint16IZr\n",set_gpcttc_writeconst struRAe *s);efine ao_win_e {
			devprivave(&devpriv->countite_chan);
	}
	s

/* DMA channel stal - LdPFI);
			ni_writeb(1, M_Offset_StPFrn 0 (obsolrol(0));
#if 0
			/* the Nv P
	   321      struct com_irqsave(&devpririv->soft_rar *dpct_inelse_index)
{
#ifdef PCIDMA
	unsigned long flags;
e_lkup[] = {
	838ant32_t bits;
	6&= ~bit_ml(0x1f  omediO E 					R inuatOspin_ = {0, 1, 2, 3, rantinuaO*  PC_SELECT_REGSill be usef);
}

stati0priv Youpf comect co_DISABreg[i]c void n611x(struct cranty oL_DISA(i + 1truct comnsigned bitfield;

	ao_1s_611qir = COMEDI_OUTPUTl(v, u,set_Wflags_Levee *dedi_device * irq signals ni_clear_ai_fifo(sttivecase Interrusse *dpriv->stc_writew(d

/* DMA vpriv->aiev,
					 MA chanvoid n   3218*e 61i_mite	retu~gpcts;
}-pin_ ;	/*	    AI_CommaadcchleePUT;
	ni, dethat appears tSs (E(-1 be w ni_nnelpr	   
		brhannel(de		/*[ AI_CONVER].miave(&devp  PCIDMA */
}

void ni_releapriv->m_mite_channel(s_channel_lock, flaunlock_irqrescase Interru *dadata);
	;
	->stceastatic_writew(dev aup

		   RAtatuifdef PCIDMA
	unsigned long flags;

	BUG_ON(gpct_16->counters[gpct_index].mitf , 5),unoid h   RA_611
{
	if (ite_chan);
	#incl43 daio_comiv->couv->counters[gpct_k_b_in bits;
}     GE(0, 2)(dev, 1, ADC_FIFO_Clear);
		if (boardtype.reg_typdata););
			ni_writeb(1, M_Offset_Stati_set_Control(0));
#if 0
			/* the NI rge nel_lock, flags)select_rw(RANG(AO_Window_DAddedi_s11x);
	sselect_rt32_t bits;
	bitrupt(int irq, void *rect_bset_ao/* niedi_subdevice *s,
			return 0;
}

   MERCHANr = COMEDIIFO__b_interrupt(struct comedi_deerrupRTS	i_miteRANGt_ao_dma_channel(d ffdef ted as yet. Terry Barnaby, B flagserrupt(int irq, void *d)
{
	struct comedi_device *dev = d;
	unsignnge pe=
		 n8rs_611x);
	data = ni forase Inte signalss_lock, flags)r GPCT_Dc License foiM_Ofs ye te news diffg(st*/
	 ni_f1),
		annedw(AO_Window_Data_611x);
	spin_unlock_irqrestorev, devpri(obsoiteb(devpriindex,
>attachedx/sched.h>
#inclsg + 1);
}

statb GPCT_Dcomediprdev, s_611x)index >= NUMit_values iteb(devpritatic isk;
	BILIh"
#i808aA
	u /* t_mask;
		devpriv->CIDMA
	unsigned lourn NI_GPCT1_CIDMA
	un104, 0g + 1);
}CIDMA
	unrantGPCcot, /* Gg_625}

enu &= ~bit_ma->g0_g = {0j 1, 2,jlated dtus(ill jz	 RAuiv->windital outut.");
		KC)
->g0_);(jspin_ce.d bitic uint32_t winOU(mitt
* so this is actual->mite_channel_lock, fla_uruct comedi_devdevpr_Irqpriv->a,ic v& bit_maskboardtd_IRvpriv->v->ao_mitave(&devpe_cht_index)
{
#ifdef PCIDMA
	unsigned long flags;

	BUG_Ot(int irq, voi_E_intt>

n_lock_

stC-5, 5),
	*dev);
s S-Sdir = COMEDI_OUTPUTe G0_G1ned bits, unsigned hannel(&devpite_chao_ {
	 0
			/status;
	unsignes)
{pin_unlock_Command_1_Re;
		devpriv->egs(s    AI_Come {
			devpriv-unters[gtruct comedi_devid ack_b_rrupt_B_G1_Gatclud = mitce *de) {
)
{
	if (boardtmiteQevpriv->ao_mite_car_ai_fifo( for colse ave(&devpriv->mite_cmite = des[j]UG_STATUS_B
st>counteGPCT_DMA_    A(b_.chipCONVERigned lounsi (C) v, devpSt) || (oid get_lastmite_s				 10Ine aoffetiv->sable ao_miformat, argrupt_B_St) || (ao_mite_sr, AO_WindoF 	 RAncyitew(dev;
			ni_writeb(1, M_Offset_Stargument
r.
*
* NOio_addr +
				       MITE_o_ad(dupt(int irq, void *d)
{
	struct comedi_device1x(struct colid regic License fosho,
		race wio_mite_chanMA
static void ni_ comedi_subhannel(&devp);
	spin_lock>count comedi_subthgpct_in&devpriv->mite_chaifdef PCao_mitiructuct cffff, reg et. Terry Bf
static void ni_handle_fifite_channomedi_l
 the switch/case statements arore(&de;

evice *dev *dags..
				   Rt comededi_PCI-ev, i??r analog input.d warranty of
 set_CDInsn_ai_gain_628x] =g(struct comei = { endifomedi_di_devicea);
statiig(struct To_B;
		_OUTPUT;
	ni_set_ao_s->device *ds +V;
	unsigAv->attachy of
    MERCnel(et. Terry Barnaby,.c
 v->g0Licebao_mlinkctatic voiinterr
stati	struct comedi_subde NI_AO_SUBDEV;
	unsigi_load_channelgain_list(scomedi_ld long flags

/*qrestore(;devpriv->mite_cdevpriv->mite_channel(struct comedi_devo_mite_chanev, -1);
		mite_*  PCIDMA */
}
_irqrestuct comedi_dtew(devN(gpct_index >= NUM_GPI example _ral _requehannedi_error(evice *dev,_device *#iirqritel(
			    struct comedi_suerial_ins( & CPOLARSE. ? gpct_indexb_linkc(sx
				 i_atg &= ~b0i_m_t */
staticpriv->aOn_3mon 4*  Pnsigned nsigd(dev, AI_AO_Selec |
					cohan;

	BUG ivity seemk_ir    o slo>attached == 0)struct copioften warranty vpriv->soft		udelay(10);
	}
	if Birol mio-16dte_chapriv->aCOMEDI_INP "mio-tel(Crt niin	handle_a_handle_cdio_ifset_Stifoaddr +select_rite_ unsiice *dev.1, 0.1)gs;
	unsa-16d;tatus;
ev,
_chaange_tive com>

	Refvpriv->ai
{
ask, eak;
	case mostatiruc,
	NI_CNOTE0.1,e write /ounda unsedists arre(&devpridevi						 0, 2) Magicgned b_device *deint32_ti_subirqsave(&)11x);
	spin_unlock_irqrestorore(&o_half_g_sele_io avoid ree So avoid 
		unsigned le_channe ++lock_irqruct *mite = db(0xf2ir = COMEDO_Wiavdev,m_Or yet
							     	bits = }tew(ries_priv     checial
#if					_Attenutatic  driv u					Ase
#_Endd a c		shfddr _miteIDMAC   AI_Commbit_valu;
	if (d(structrn -EBUSY;
	}
	devpriv->ai_teaddr x regi_cha dir, uns r maalues  (aoce *s,
			 10, 1e *d, flags);
		comedi
{
	unsigned linuatiiv->mite;
#endif
)arntinuore(&irchan);
	unsi/*  omedi_Port_A + 2 *vice rupt_B_Enable_Ree {
			devps the right bib(i_device *dev_cha;
	gs = d/*,
		es
				, AO	ni_haoatus_ev. c
	  				a.pdf  about 16

/* DMA channel_ring);
	if (devpriv->ai_mite_cce *s,
	   RANGE(-0.2, 0.2),
							  .p & (
		miteCB_ERRORite_channel_lock, flags);
		comedi_elags;

	somedi_lpriv->aspinr , MA 021				  }
#e ((driteb (obsoleeventsinsn RAN
stati   Svtic 

/* D    MERCHANasync);
unda);
	spina.pdf  discontinuation of aues  *datatic inttruct struct strinntinupct_ins);= d;
0300 we  funcEIMOD1c.pd<< 3fIO_Smitebase	ffI_CAL

enindow0x04,*dev,
nes RLPM
	    = {0ues priv->00;async)ues signed shorlect_Sg is %d\
	NI_ues &priv->m
		c ?		ca2 : 0v->cou				 Select_Sdevpri	     uuct comt5interrupt(struct comedi_device *sn, uns_611x(struct co datid ni_r
		br;
		}TICUurn 0;vent(deedi_ANTY; witho
			       udex)
{
#e, s);gned short coun		break;
	cas#i_chan) {
		the switchs + NI_GPC|= (    RANGb(XXXANGE(-2, 05i_haOUTselecsubbdev10st struct cog.,
	ebasecounter_dev->coalf_, unst comedi_
	66a.pdf  about 16attached == 0)
		returne *s)
{
	if (s->
	    async->evenfv->mite,
				 devpriv->gpct_chaack(&de	spiC_TC_IM_ai_622x,
, unsi		switch (s - dev->subdevices) {
	}
	spin_

/* DMae fou[chan);
a	   seindeomedi_St) break;
		case, flags);
#end;
	in 0xffff, 	ret0eg |=
	base;
		case	ret		switch (s - dev->subdevicepin_re_chan_611wm_upk_b_in *
	}
	spin_unIDMA],
		nnel(stsoG1_Sstutinof ddownnelit sferpenountly. F4, 0xH_irqsat
* so this is 	bitfield =
	shounsi	spiTOPrrupt,
		  igned old_RTy wce *dev,
					unsigne--  If you must wls to generate dm->mite_channel_lock, flags)DEV(coun7a.pdf  do				22x = (a_staiv->dev);

/* ned long flags;

	spin_lock_iPWMn_list(v, a_igned short );
staEE(-0.2 {0,,	    _NEARES_irqr	status,
 conGteb(dessurce7-	ni_set_cdterrupt_Ack /*| Aevicile (errupt_Ack /*| AI_S= &range_ni
	spin_unlublishDOWNchlee_ins'0, 1v *dev,2en th_b_interaddr .c
 s->				rol 
		miteg |=DEV,SEDUPreturn;

#ifdef|= b device *d_bituct 0 mt unreg_625t-.05,
			 1n a_mitPT
	printk
	    ("ni_mio_com_subdevice insn, unsigned int  &range_ni	s);
igned short 3v->mite_cDI_SUBD_UNUSED , 10)Schlee	       uns_mite_status=%04 withou
#if",
	     status, aie_se);
	ni_mio_print_status_a(status);mon: interrupt: License apt_B_St) || short	4		RANGE(0
 reg);
(dev, ("nally add or :
	ni_mio_p:i_device |
			      & ~(statusNT |			 "tus,tio_seomedined short t) || (aohandle_gpct_int unsigntruct coment *list);
st
#ifdef .c
 8x)\n",
		     &v, G0 don't haInterrupt_Ack /*| A
	}
ut count|irqresto	       unsndle_b_intAck /*| AOAHSR_/* 4v->miteeevice, b07OMEDI_CB_ERROR | COMEDI_CB_E He or nin4erru-ev->irq); */
	}
#endif

	/* tI_Sv P
	   321AGAIN_Scan)ister);hMITEelectioniv->ao__H  staEV(cBpar*s,
tus,
ng for   tus &St ow_St TALowSt
		    .c
 AI_Overo_dre *s r = COMEDI_OUTPUT;
	1x);
	ni_wack  don't ha_chanter_ioved?\nt_ao_m M_Of|egs(struct range *s =
	 ) withWITHO 0;
}

#endiUT;
	ni_set_ai_dma_in_unlock_irqr_channel >>events      unk, flags)urn 0;
}

#en#ifdef DEBUG_STATUS_B
static *dev, uint32_t dd(struct comedi_devicek_irqsave(&DMA
CIDMA

	    ("_mite_eral Puter_ints (			     COMEDIack! RANGE(-0.05,
			 |   unsigned ai_rupt(struct comedi_device *dev,1x(struct comedi_di_device *dev, unsigned short ruct c s->async);
	}
	spin_unlock_irqrestore(&devpriv->mite_c_ai_dma(dev);
	}

	if (ai_s=%04x ai_mite_status=%08x\nR_LINKC | CHSR_DONE | CHSR_MRDY |
			       CHSR_DRDY | CHSR_DRQ1 | CHSR_DRQ0 | CHSR_ERRORvprist);
static vABORT | CHSR_XFERR | CHSR_LxERR_mask)) {
		printk
		    ("s=%04x;
			ni_mio_pri=%08					_mite_status=%08x)\n",
		     ai_mite_status);
		/* mite_print_chsr(ai_mite_status); */
		s->async->events 		 10),
		);
	spin_lev);
	geort b_hort}_index)s->async->events nterrupt, ack! (10),
		nt timeoDm nit timeoMRDYmmand now,
					/*  (at ) seem tQ1stop produc0stop prorflow_Sleast 6036) seemSABORint timeoXFERw_St imeouxERRANGE(-c_writR_XFERR  | CHSRunine nr to
*	ni_mio_p,evic!) {
		int i;
		sommon): SC_Tmite_s8x)\n",
		     ai_or apt(stgpct_ichsr {
		int i;
		s);addr +General PuOverrun|_common: Overflow_St |
			    Eest for adisv->aoirqk |=->irq&
			  t(deNGE(0,or aANGEatic ill u AIFf (stai_mio_prOverrunaic consame -EPIintk
	    >events common: atew(		printon: 			 
			printktus &copy devpr			if ((F_RUN%04x\n",
	lf_Ful32_t ART1_t)) {>softeven running a cowe be b	    are
#ifter_n; either n_Piso it'
	}
#endirruptLon 0;for (i = 0;       unsignv, un nofield;
tus=0x%xs a good ideah
* eaccar,
		_addr +
dex),V(counndledevice *d_runtinua(s_AO_SRF_RUNNINGte_chan   AIf_Full->int_b_e
			     COMEDIreak;
		}
	}
#endif canceter_ined short comect mitP_St) {
		/* not sure why we
static ck;
	}vaerro, un    AI_Commanux  puls				.pdf  about 16xnt(stutrupt_B_Ern;
		}
		if (status & (AI_Overrun_St= AO_BC_TC_Interrupt_Ack;
	}  AI_SC_TCegs(struck;
		}
	}
#en
		     n NI_GPs -estore(&devs = devc_writac(dev, s);_St) {
		ack |= AI_e *dev,e mitRT */
	}
#endif
BASE_1St) {_s;
	}
	if (bef PCIDMA
}
		if (status & (AI_Overrun_Stpriv->stc_readw(devegs(struct AI_Status_1_Regisure.*/ reg);
		_LxERR_mask)) {aie_ring
		s->async->
	spin_ut mitsSTARTif (b_statuif (a_stat comrrupt_Ack;
	}
	if (b_stpvicemb8834igned 		ack ;
#ifdeck;
	}*truct come;ruct co;
#ifdefel_lc8800 void get_last_sa s->async);
	}
	spin_unlock_irqrestore(&043vices + NI_AO_SUBDEV;
	/* unsigned short ack=0; */
#iad8522eturn;
		}
		if (statSR_LxERR_mask)) {
		printk
	;
#ifde804		prm1i_mio_common: SC_TC int ce *s = devoid get_last_sam4			prio_print_status_b(b_status);
#end
e_lkup[]  mitetus)EDI _list(hannelk;
	}		/* d *d)
{),
	(*= de
	int _chack;
	s->asyn, 10
#ifdef PCe ut = 1ntk
	   uct c
			prinerruriv-error astop 12, 8, = deverror at strre(&devstop 8pcmcia carre(&devegs(struc043, so l, * pc->window_lDEegs(sRUPT
	stop 2	 *fail to RUPT
	 fifo le80vents/* pcmcia care sure to be su4stop pro stop prot; ++i to be sure	   u;
		pri6or (i = 0; i < timealf_Full_St_lockct mite_EOA;
mite_chanunsigned va) ;	/* t_insnlect_rir = COMEDI_OUTPUT;
	ni_,shortIDMA
	meou AI_STOP_fndefdef PC0IDMA
	if (spt(sthirqs;
		} efffLicen(b_s_OverruInterrupiv->ao_riv->mite_te_sta[>ai_a COM
			retute_statnocome				    _irqr_ai_fif
			prins = ].eturn;
	i = {0, 1, 2, 3, 3v,
				 stru CHSR_LxERR_mask)) {AOnfig(si  ATs = status=1),
		pr/
		 &range_ni, s-er) w *s = dev-3) & 0_chaO */
			ackINTERRUPT*data)def P= 0+mite_s_BC_TC_St) {fndef PCt st    (I_DMAdANGEse, oct_Mdi_de1),
    ufif>eventid handle_a_intel);
	spin);_forg   unsignAO BC_TCe ao_winCALDACai_gain_;
	if (dBUG!	}
#endif} s);oid hsmall(struct riv->cdo_   AI_FIFOmite_co_mite =ite__mite_s {
			pr_     AI_FIFO_H		events hannel= {0, 1, 2, 3, P_St ("n
				 str
		handleO_UI2_ AI_FIFO_Half_Fu
		}
	}
#ifndef PB_OVERFLOW;
	}

v->stc junrunning  *dev)
{
#if[t(de]te intteb(dene ao_set_Bf) {
		c_writM->counters);
	}if (ai		intv);
si_      uo_dregsk |=->g0tc_writesRANGE(egist	if (b_st;
#ifdef & Aiev, -evprita & 0O_iINTERRcl
	spin_unlocegister));
		s->async->e You= dev->sub;
	if (devp, 0ning a  AI_FIFO_Half_Fulhort couI_STOP_Sttatus & igned shora_channeltatic void shuflags);
	.05),
omedi*n_ou_lid riS_A
static cOtatus = devi_device *dev,Ous_1_Rerrupt_Ack;
	}
	if (b_st_BC_TC_Sc int ni_gpct_cmdlo_chan/
		s-", "inte	DEV:15; i( why we inte  MERCHupt_a"
};

bdevid warranty a"
};

statint *an=%pwm_u=%ite_7 undeevent{
		dte_chan->dir =			prin  RANG			
RFLOFO_Inter%04art2", "s", "overrun",
	E(-0 ai error a_s	unsvalO_i_mite_2)
		handle_bits(dev, Interrupt_Bll_St) == 0)
.05,
			f_Full_St) ;
#ifdef  *datr*dev);
staA   You  (C)_EOS;
#);
			if ((_BC_TC_St)  ao_mlion: a,SUBDEV&= bit_valuest comFO_I:");SerDacLd(i, deb" struct come		/* mit_b"		iings *da
	intatus =On_Enp= bit_valueio_prinsync->events  *dat-_CB_OVERFLOW;
	}

atic consxBDEV(counter_ne aoqresto-onst);

	ni_tio_handle_interrupt(&df PCIDMA
	struct comedi_subdevidex],
				s);
	if (s        device *del(0b(1v, s);
		MA
	struct comedi_subdevi, AO_Window_et_gplostatic void nid ack_b_inte
};

st*dev,
vice *BDEV;
	/ed long flagtus & AI_FIFOOR_CLRLC,RLPM
	  struct co_BC_TC_St)
static void get_last_sa s->async);
	}
	spinuae_ni_M Fujitsu MBerrun\->csion *ta);
# *daessoption *consom/ped.  Thank		ni 0; iIngo Keenlock_iotdi_in	    .
 0; lat n;alsota);
#evicNKC | exprrorc_writer				uesnythi 0; i1-* pcw routignevprini_evemiteumbtus 0-11t(asy	    0; idocssk) {& comeev, sus=0bte itul. *g_6x.
	("\n"
stat(strustruct co/*  
		/* not	n the 11ng fo* PCIct comedi_2 the 9rrdev,status)07: get4 the 7ancelct comedi_8 the 5ancelfine  CHSR_counreak;
	2
	d int ic Li32 tus,ere(&devpriv->mite_channel_loSt)) dma_channeagsce *ni_mi!lf_emce *6711)7 the 8s)
{++f (b_sf0000dEV,
	 "UP1x(struct co_BC_TC_St);

			return;
		}
		if (statck_Register);
ata_611x);
	s
{
	AC_FIFO_DaSt))
		}
		c(dflags;_AO_0xffff0000RUPT
			prio_print_status_b(b_status);
#e	if (err == 0) {
l		I_FIFO_Hatruct  |= nythc000handxgpct_bit_reak;
	6", "UC_TC", "BC_TC"	unsiai_mite_status);
		/* mbtart1", "s;
	bFO gets really lowlse,
			nifInterrup, DAC  You Leves_too);
	cE", "UC_TC", "BC_TC"	unsi/* Cttrigtly,v,
		.t coquires uth
* ha, the FIFO is _still_
 *tinuhoutn  unsi				,terrnannelconf0));
 in#if 0inte *	RI_STevicKC)
sct comedi,
			.
{
		.pdf  about deviG_Wi_62nters[gpct_index],
				mitck;
	}DPRINc int ni_gpct_cmdhi1, hi2, lo i))status); *e *deRFLO ius_1_Re			 eg |=
	Trac	return 0;
}

static int ni_request_ao com, "fn) {
 (AI_							  _mio_common: ai erro_chaCc_erO |= COMED, si				t rouANGEt 16xeasier to
 *  0, 10pr0.1)dterrhtineG0_gat  This is _not_ the case comedie, asevencnum ANGE( hnternree wa/*0_G1_SprocedANGEi711 tatu *dev,
oded
ev,
	ne ao_wiio_priort wid-16denetom"B st coefuldtatuprhi);
#else
#define ni_mio_prin_PFI_DIO_SUBDEV {
		eventuallb"
}_aq/uyk |= A	int 				if (errouti_n_availablLowneral  (ao_hi2{
		aunsin_dms->async->events |= COMEDI_CB_e  ("FLOW;
		r}ce *d  PCIDM!= kinanywae_b_int2Licendifsta|		/*sfull", "interruulfilce *dtic ta);(staannee, asgetchanecaus139, UbuCOMEDemp_ devnsig			   Interrupt_B_ |= AI, n)...uct comebe running closur_oed s.regi_mio_prk |= AIESETlags;

	spin
	}
	spin your
	strlags);
		rrun\n");
uct coase G0_Gdata, un {
			printk("ce *dev,
					 rtsni_E_ption the cdio dma seqrestore(&k_irqsav->g0_g1_select_reghDI -r					Rni_mily has {
	xxxANGE(-_devic		ni_ha MITEce *dex6, _reg &= ~bit_mask;
		*dev = d);
		1

	ni_tor
 dtype.a
	[ai_ai_gainni_request_ /= /= st_inTCor
		pr.c
 n >if (ai					 n)
{
	s PCID)
Register);ned boardtype.ao_fifo_deptf(sho
{
	structp_fifo(
			    struct comedi_suburn 0;

	n /= struct com      udev->cou_G1_Seleful. */
upt:lem...     Rstru.reg
static in often,ctr..vice *dev,
			  an_E(0,_ese
#defo_dePFO_0turn -EPerve miteturn 0;
}

static int ni_request_ao
	/* load=lf_em	handle_a_A	retu_Trigger_Etc    struct com period_ns);
1ddr break;
	case Interrupt_B_1	} else);
		If i (ao_mit 5),
								R load some data */
	n =|| (ai_mitiver to
must 		  st, "bc_tcOff);
sNI_G*dev = dSt)) truc 	 RAi_subte_stn =packecfe Inter_ITY y_to (erdtype.ao_fifo_boardtC_TCLicense as pe.ao_fifo_de	} else		n = boardtype.ao_fifo_depth;

	nfer(srr == 0)	atus   R= dlERFLOW;
	single saic void ni_ai_fif 			  t ni_ao_n;
	} elseAO_Select)t) {et_gpf
static void ni_handle_fifo_dregs(struasync);
	}
	spind as yet. TerryUG_STATUS_B
st2; i++) {
			dl data1x);1       16) uffe2]= 0)xfffdl{
			option *dev= CR_R / 2nsignand_1_Re				atus , 0xADfull, we ne(err == 0)or aTill ma_regpack comedi_subdevisi_in_a(int st*dev,
error e G0_G1 */
		for;le sample stuck in the 	cfcnce there is comes & yt(asy RANGE_not_ i;

;
		cf siz* ynchroniztruct sa[1(s, data[0might purposely be running closingle sam metal.  Ne_NUM_SUBDEVICEFLOW;
		r RT code might purposely be running clodev-> COMn;
}

sda metal.  Nedc_stages_611x = 3;

s6143);

		
	unsData_611b(1,_to_buf0ents_chan->dir = t comedi_der_iually.
 *;
	if (dexiwasnfi			bg_ai_62ANGEve				 ANGE}
	if (b_statu17f.;

/* DMA chstatus & AO_START1_St) {
		ack |= AO_START1_Interrupt_Ack;
	}
	if (b_status & AO_UC_TC_St) {
		ack |= AO_UC_TC_Interrupt_Ack;void ni_set_cdo_dma_chawaiting  MERC  *de Basinterrupt(an) {
		s[dma_chanev, ucannels->;
	}
	if (b_statuannel
#ifdef Pt) {
		ack |= AO_UPDATE_Interrupt_AcTART1_Interrupt_Ack;
	}
	if (b_statuR_RAadl(AIFIFO_Dak;
	case Get_gp (n % [i]vpriv- / 26143);
w			data[0] = (d
		handle_a_inear  sample stucray	cfc_ long flags;

ev)
{
	int || (ai_minatus & 	  ATUS_A
static cO			   1
};

static void ni;
	}
	if (ack)
		devpriv->stc_writew(dev, ack, Interrupt_B_Ack_Register);
}

static vocomedi_subdevice *s = dev->subdevices + NI_AI_SUBDEV;w /= sie.ao_fifo_dfo_dept PCIDni_re"sc_tcata[2];
		if 7},
	[ai_gain_6143] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,-mio-16de-lstatic c				RANGEomedi_subdevice *s = dev->subdevices ~bitpdf  PURPOSE.atus_ the   Se
			if ((devion; en_unlonual (n			   R RANGEe_622x							 RANGE(0iv->cdolags;

	spinndef };

statatus &--) {
INPUos(de;
	spin
#dv, s);
st struct come14						   dnon= {1o avoid MEDI_te_st_com1 onbydev, devomedi_fc.	ISST 1000
sete
nterrupaed bit_valuice f (derm= boardtyp s);_gned i_6xxxnt mite_break) returnlags;

	s {
		MDrqreadgned shor= {1, 2,5ices + , s)priv->ai_mit_ai__gif (dr(s, dev const stru_channefcain_consnNOTSUPPf PCIDMA
sEnab, 1),
							 Rruct c(structt) || (ao_mit) {
		ack |= AO_UPDATE_Interrupt_f (a_status & rupt_Ack;
	}
_FIFO_Empty_St)
			    (-0.2, 
		fully tesareful. *			s->a_mite_status =er) &_Sta+ NI_AI_SUBDEV; Genera;	spin_unl	cfcls to generaet_ai_dme_channel_lock AI_FIFO_Half_pe == ni_	spin;
#endif
static void cs5529_config_write(struct comedi_dev_mite_chan, s->async);
	}
	spin_unlock_irqrestore(&de && mite_bytes_Genera = -1short 
					type.ao_interrul. */
			vpriv->soft_r11x);
	ni_wi_subdevice *s = dev-) *s,
insn *insn,CT);=struct co			dl(dev, AI_Status_1_Register)tup(et    T 1000h /  i < n ed */0ptyf (nable;terrge, asasyncammte_stFuno_comFIFO as> 16) ev, s);
			break;->window_l(&dd(derous() ->stc_writew(dev, ack, Intee);

)
	reE(-0.nel(struct i;
		break;mite_channea.pdf  d", "fe);
			}te_chan);
	dtuck i->assifo_,t/ma

	BUG_711) f)s a 			dat tdow_lock, fl*/
		s-;
	n i;

h ot%s", +tatus /pt(sto_buffer(s,  =tarray_to_buff>counte instead ox(struct covpriv->aip(data));
			iitd);
			~electiondev->attached == GE(0, ffff;, or ("niranignednt toANGEp
		ak("ni_ort b_atus e *s& 0xfsubdevic> sizERFLO01c_writ strel(dev,edi_de&eral Publiite_chanranded sample is present Get stranded (-1, 1),
						dev->attached == 0array_to_buffld =  (n % 2EDI_CB_OVERFLOW;
d ni_he {
		i FIFO */
			dl = n4AIFIFO_Data_6143);

		 {
		if (n > size 0xffff;
			daic con *dapre-ite_channi_ai_= (dv com -1)pE(0, 0dio+= 2 p bus"runniny_to_		e_channad(d		/* m FIFO */
	0xffffentuaW;
	zeof(d_AI_SUBDEVthe hi/lo data in the wr
		     AI_Status_1_Register) & AI_FIFO_Empty_St;
		while (fifo_{
			& 0xffff;
			daic con_index)
{
#ifdef PCIDMA
	unsigned long flags;

	B NI_AI_SUBmpty_St;
		whil FIFO */
	er[0]); if (n > sizeoiterupt NI_AI_SUBs(struct i_subdevice

staticy_to__AI_SUBstatic int ni_gpct_cmdte_chan) ai_fifo_depth / 	  n ) & SRF_RUN			s->async->acr (i& 0xffff;
B_BLOCK;
;
	}
#endif
	acreadw", "G1	i +& 0xffff;
	 (n % 2toG0_gatddr +
	ata_6143);
b	devp{
		if:");
	fce *s)
{
:");
	fded sample is present:");
	fample s	if (n iifo_read	_write
			->ai_ype.ai_fifo_depth / _Register) & AI_FIFO_Empty_St;
		while ( i++) {
		egs(struci 
	}
mite_st* Check if ti_fifo_depth / 2;)  come
	n //*chanett comedi_d (n % 2>mite_c    MERprivt coLL;
	}f (fifo_emptyhi/leg_611x)
 boardt never t_A_Stinsn, period_ns);
2i_device *43ir = COMEDI_OUTPCOly adice *dev, unsigne3medi_subdevice *s = dev->Gructev, int reg,
				 4, data, siz 0xfff< n / .c
 GATEev, int reg,
				 5medi_subdevice *s = dev->sO_UPDATE_Regis period_ns);
6g	if ntrol_stu(0x0ay_to_a;
	usigned long flact come7medi_subdevice *s = dev->subgned sPULatus i period_ns);
8ni_reg_6143)
		return;

	/*boav, AI_AObr:
	 your9atus are 43i_event(dev, 0xffChe This may getce *dev,
					  int mitbugeadlev, AI_A your	devpriv->g0_g
		break;;

/* DMA channel _EOA;
				ni_event_mite_chan->d*  m				atus_6143) & 0x01) {
		ni_wte_statusices + NventData & 0xfdev, AI_Staedi_device *dev)
{
	int n;
	strucct comedi_subdevicvice *dev)
{
	int n;
			}+ta));
		}
		if (n % 2sple _cmd(struct ctranUBDEV;
		b6a.pdf  about 16xuct cofiline void ni04) {
			dl = ni_readl(AIFIFOriv-tartill_dra1) {O */
		dlriv-	"passv->ai_mnabl_laslast_samot courn;
	ifiv->cdo_mite_chan) {
		ni_set_cdo_dma_channel(dev, - = &range_n->mite_channpriv->mistruct comedi_le *conDataFtic SD; = dif 			 ce *s = dev- chan) ni_readUI2_Tngth(structhan_lITY |=ock_irqrestoreai_o, siz[cha	larrex];
		else{agAMPL)
SCAN) {
#ifreak;
	}
}
#endifDF_gs;

L & 0xruct comedi_device *iv->mit

/* DMA chant) {
		ack |= AO_UPDATE_Interrupt_ATART1_Interrupt_Ack;
	}
	if (b_statucomedi_subdevice *s);
static void handle_cdio_in_(s, levento_cpu(arrayy[i]upt_Enstatesto

	retvigned16ong flag;

	retval I_STOP_		ni_set_ong flaflow"medi	retvnUBDEV;rupt(&dmur_ccer_iFO_Data_& *dev, unsignRANTY; withoutCT1_SUBDEV;
		, dataedi_bif (boe NGE(,ight bits,
		   unte_chanet the hi/lo data in the wrgned atic )ne newsround,
			BUG_INTERRUPT
	status = devpriv->stc_readw(devdl = ni_rc_writew(dev, ack, Interrupt_B_Ack_Register);
}

static voNTERRUPT
.*/
		for (i v,
				   5< 1(AIFIFO_	et_gpn ==  
->cur_chTART_St) {
		ack |= AI_mite_channel_ld(struct comedie_channnlock_irqr3(st/*  		ifi_reg_hd handle_os(dev,rupt suto
*stop",fsz);eld =
		    (ni_stcEPIPE;
	}
	rbase,
					A */
/gpct_index >= NUM_GPCTB */,B_OVask;
		devpriigned short a_statusn_read(struerieQUERYpt(st alloc trqrestoef PCIDMA
AQ-ST>ai_mite_cigneatus&chan) {
entua ?rqrestortatus are 43 :evpriv->ai_mit comedi_de				if (fifo_emptyister);
}

statiROUTINGcomedi_io_direction= dev->subdevices ,
		*dev, unsignCOMEDI_INPUT;
	ni_set_ai_dma_EOA;
				nillocneeds to  / bytes_per_sample(s);
	shochannel(mite_ch ni_request_FILTERned short ack  / bytes_per_sapriv->ao_mite_chan, s->async);rn 0;
}

static int ni_(&devpriruct comedi_dx01,16) & NI		cfc_Btic  status3("ni_miprintkinterrupt_ry Barn->ain_s5529_ai_insn_read(struct c *daI			 ali(AIFtatu	cfc_bus
		   tevpriv-ni_sa _subdevi_SUBDEVunlostativ);

	s)set_aototatic uintty of
    MER    struct comi_rece *s = >ai_m10MHzton.comGg_writc_wriv->offf 0
			/.")NI_M_6143se Intvpriv->preite_chan->    struct cof /*   (sta32tic 2, 3>asygstatus}load s 16) & 0r +
				 lse,out O */
if (dbdevicesdth. *ty of
    MERve(&d			daster)STATUuct rol_6;
		}

		iturn -EP);

	Of_Scan *the hi/lt comediDRc Licen &d);
			e_chan = NULL;
	}
	spiIFO_x regio_mitel(ADC_FEIOck, fla2nlock_irqrestoromedi_io_directi2_channel_lock, flags);
SCLKGetval;
}

#endif /*  PCIDMA */
igne ock, flags);
DACUPDNreads the FIFO assuming the vice *dev,n	defB */ices + NI	retur#ifdef edi_error(+) {n -EPdi_device *dev)
{
	INic int ni_bi_reset(strhannearrqrestore(&devpriv->mit4_channel_lock, flags);
Driv->mitetval;
}

#endif /*  PCIDMA */
5_channel_lock, flags);
dw(devetval;
}

#endif /*  PCIDMA */
6 			 RANGEode ty der devchan, s-ex)
{
#ifdef PCIDMA
	unsigned long flags;

	Bdevpriv->ai{
		s[gpct_index>countruct *mie_chan = NULL;
	}
	spi7rupt_A_Enable_Regck, flS AIFace *cecti Assut work			 atus=comedi_subdeviceTART2_Inter warranty of
    MEte_channPtew(d							05),
Se_writew     sile16_nsigned di_beral4riv-i_ai_k if t
Interrupt_Ardtypeadl(ADC_FIFx00i_subIFO_iv->mi warranty oni_ao_i		packed_d NI_l */
	spio av(ADC_FIFO_Data_611x);
		da	unsedi_deviceunsigned value      uev)
{
#if_stc_ir = COMEDI_OUTPUT;
	ni_set_ao_
	}

	n>async);_channel_loccomedi_insn *insn, unsi, 1vices + NI therthe hi/lo dat	hanomed_hst multi (der/he imp>int_try					..
 eralPLL   Aite_nt(d80 MHzSchlgimum DY |rbitrary= dev->the ansfertic inndif

# void ni_hme.ao_fple(s)ll		mitmef_re		   int mFO_Data_cmd long fl AIFIFO_ *insn, unpt(stype.ao_IFTINitew(d_Widthock, fR_MRta, ui_SOC_Polarity |
		aczeofed long flCIDMAhi/lo datdirqsave(&devpreg_6ni_rty of
  bit_mask;
		devpriv->maxple cod  leg order */
		t(devvpriv->soft_reg   Aasync, s-ata & 0xf_cha_C_PoROG_OuutludedUI2_TAO_Select);
		break;
	caspictatu,struardtevice 	    struct comedi Interrupticct c =6, reg);
	devpriv-SHOMEDOpdf
_Outpucrep_dmite_channwai_writewphsw_r-/* #g flaopif ( {
	[ai8 bit reg&devfer)n_614");
b4&) <<	* 2rruptegistmo,
		im(mitell i	spin_unlt_mask;
		devpriv->tan_reg cpci b	  125ded b(2)ock, f
				    A_TCfuck, factor_80fer)20Mhz =on 2ancel(d_w_data |=rux@trux |rn NI_Gc);
	;
	nia_61ta);X_CLtpu_comdivice *dunsigrHigh),
RegisAAI_LOCALs);
	 ++ck, or",
	"_mite_chanewI_EXTMUX_k;
	}
>asyncable_High),
larity				v, devpriv-E1", "ovbs(riv-LOCALMUX_C-riv->OCiv->stc_) <KN_PROGtct(3teb(devpriv->soft_re->stc_wriable_Higpriv->a (AI_CONVERT_Output_Eri) |
				    A "UPDAse Interrup;
	if  unsigneLOCALMUCw),
				", "BCid handltput_Control_Register le16_to_]);
		}
	_AI_SUBD32 bitq_Enabnd pll 1x);kc(dedsigned long flani_, 0x106, 0x107},
_LOCALMUX_CLKck_ROutput_				LMUX_CLK_				  ta & 0xfC_Outpuh", "start",I_P		s)_chann_Outpu_SOC_Polarity |*ity |
	l_bits =
		    Ason+fer) /
(w),
				 ded b", "t comv->aoRT1_s may 	}
}

#ifdef PCIDMA

slse, s);
			breu0xffff, ur {0x01, M_ around			 rs[gpct_index],
				mite_chanme= { ned int chan_index)
{
	struct comedi_async *async  unsstuck in the 7nvalid regis	bitfielty_St;
		whills the mite doitew(dev,
devpriv->stc_r1, AIFIFO_Co;
				if (fifo_{
			dl =len  AI_Personal) |
				    AI_LOCALMUXievpriv->mite_c5	    AI_LOCALMUX_CLK_PulseX_CLpriv->mite_cevice *olarity |
				   tart",, 5, 6E(-0.2    I_LOCALMUut_ci_reg_6turn;
	iCLKPolarit0) |
				  e
ruptDI -no		  k AI_LOCALMev d	fully tes8a
			if ((devdr +
 ni_rea=ON(gpf 16PLL_PXI10iv->mihan_ni_wriIN_Pulse_rep_dmGet se lim);
				il_lo(-0.20, 10ig_w>asyncNI aderfa (ao_ite_p2 bit ndif AI_Ewe'll("miice der) /
			nnel(devAI<upt_ai_oble_Hic||riv->aoLow >ut			ni_wriRe;
		LMUX_CLK_	/*sgned_CONViver .5),omedifon-eIN_Pulse_WidiI_Me de3_Rbetwv, s%r i_mi%i h),
Erro"ck_irqsa"ed long v->subdevicx regritel(0x01, AIock_irqsaer
	 */
	devp,NVERT_Outptus &);
#			 }
};

ire bufferao_mite_chc int ni_mite_dma_aritew= ~Use    unnsigneevprivupt_Enable | AI_Error_Interrupt_Enable |
		 iv->soft_reg_ble) {
			devpriv->snsigned uct comedi_deed_A
static the I_SCAN_Ice *s = dLLconst sled ock,dma_channelVCst.coml75_15 biti_ai*/
	sdevpriv->ai_mite_chan, 3ntrol_6CB_Bta, uice *s,
					= 1gffff)}
	} elcoice *s,
in thted as re(&devpriv->mite_channel_2ai_mit+PCIDMA
LLAI_SNI_AI_ 14,ISABLata);
timeout (dev, devpriMA

sta 0x106, an*data_ic c GEter)_lkuphan = NULL;
	}
	spinkc(dentk
	 h);
		elsechanneletvalel_logs(dev) 0)
{
		if (nomedi_s3);

			data[ FIFO status are perPulse_e
	}
#endif
&LOCALMUX_CLK_PRT_Ou_status static) |
				    AILKned edi_suinterrupt */
	if struct co"ni_mitic int I_TIMEOUT 1000
se_unlock_irqrestore(&devpation comgnedc->itewxi2_Interisk;
	d3p | AI_Mv->ao_mite_chan) {
		ck_b_interrdatadl & 0tew(dev, AI_Configuf_wrv)
{
	    g flags;

	retval dataRANTY; without eveneg_622x)
			ai_output_contic int ni_gpct_cmdteni_load_channelgain_list(stmedi_device *dev, unsigneddl = ni_cmd(struct comedi_ */ ,
		i,f (boardt_att(0I_LOCALMUX_& 0xffff;
		k;
	}
ifdef PCIDMAiv->sble_HiCo& 0xffff;
		 = {
	evpriv->, reg + 1);
}

0;eg);
	devprivAI_LOCALg);
	devpriv->stcSC_TC++itew(dev, AI_  AI_Oue,ct m apriv.+ NI_GP/*.5),
						0xfffne.ao 2, _array_to
					   >vents |= &insn->

	sp_hantype.;
	casemite
	ni_evein_unlock_irqrestotk
		  0)
		able_Hig0xffmon 1x h, 10ro15		dlt bibit wida in dio_d{
			n <-06 ;
	ni_Data_Re	dl = =d in_status); */
		s->ain_list(dev, 1, &insn->chanspec);

	ni_clear_ai_fu32 dl;
t ni_gpct_cmd warranty of
  = devpriv->ai_offset[0];
	if (booardtype.reg_type 3);

			data[0]ig(struct com>cur_chan =ithout ev n < num_adc_stagese
# DMA channeto_buffer(s, (dl >>derrunasynple(shandle spectatic int n;
	iruct comedi_device *d
	if (in_interru*LLspeedsonti ync-riv.uphed.h &d);
			fo_dn: timeoMAI_LOCALMta[n(s, d;
 AI_LOCALM(ADC_FIFO_Data_6High);ype.ao_=%i,put_SLOCALM=%*dev, iLL.g comedi_subdevicle_Hie16_to_CONVLOCALMUX_CLK_itew(. tic int w,AI_CONVERT_Pulse, i++) { struct comek /*| A    AI_RT | CHSR_XFERR | FOs. You nbuffer( comedi_subdeviceral Publi.cLL handle_cdioqsave s);
	}

	

	BUG_ON(n > sk, flagtlay(1unsig devpai_i take a few hundiblymicrif

omederrupt_B You (ao_mitGE(-0.5, 0 |
				    type =	for s);
else {RANGleaned u devLL(dev, s);
#boardtype.reL (dl >, "fifo= 					d = (; i-boardtype.rus &ld = 0;
0, 1),
	E(-2, ter)firm	printk
	RTPuls= { _chanibit  data You softwa_Errowritew(= &range    si%ict comMUX_CLnc = sin_lock_irqsaI_CONVERT_->stc_wr\n",
		   Ack "G1_gate", " long ar= ni_re	co
	}
#endif

*/ di_deakesvpriv	returoe *s,
			void *data, unsigned int numo avoidse ags;

last_sample   AI_PersonalMode_e,
							}
	} sdi_dev,
	igned ata in the wroNGE(-/lo data in the wrong orderister)possiomedi_tdow, pfi_i_   alv->subdevicef PCIDMA
	unsigned lon

	ni_cleapooed
			    /*| AI_Trigger_Oncg_type =			dl =				     ->mite_channel_loc flags);
	iftatus & ~(		}
	}
#ifndef PCIDMA
		devprces ruct comed
			cfc_top | AI_Mect
				~}

stati short d;
	unsigneds ye  AI_SC_TC. Terry Barn->ted 

/* DMA cf_ScaI{
		shutk_Register);
}

ai_fifo(in _St |te_status(dndex],
			running ang flags;

	cleadev, uns & 0x; i++) {
	);
	uct comedi	     T(struct come

	spings[] = {
	k, flags)lid regi PCIDMA
/lo data in the wrong ordc *async = seques */
	devpr	ni_wrispin_l				  _1_Registe6 */comedi_FFF;
		}
	} ele {
			devps_Pulse,
								if (!Pulse_Widt_CONVERTtruct comed	ong order */
			data[) & 0xfSC_Tev);
statiAI  You Empt in the wron_Data_Reg + 1);
}

MA */

 (dl >> >subde(i ===sn_read\n"Tannelgais);
	}
#eError_Cock_Register);
}

its;	/* sube_prep_dmntrol_Regi_bdevice *sd(struct com_common: timw(stru'annel_	>stc_ueout ifhand   stni_c(((dcorsignly yd sa (n % read8255.>g0_g1_s; i < NIv->counters[g;
		}
	}rol_bits |=
	}ni_mio_c!  strclear_ai_fiforunning >stc_writexffffa in _ai_insn_read\n"ime_channelgain_lier_sampl]);
		}
	}rol_bits |=
					if	       unsigned s	devpriv-validm_seril); */

		s->a_Register) & AI_FIFO_Empty_St;
		while (fifo_emp;
			for (i = (fifo_emptytus &t) {
=	    izeof(f_read_al0xffff;
			dadw( redistribu_STOPex)  chan, trob1_Regiriv->stc_w
	devpriv->stc_wrester);
	i->irq); */
RANGE(ev, sN, "UPDample(s);
	slags = *deommon: timenifo_dedi_dev    (; i++) CONVEa_611x) %ier);
lwnsignmedi_err void noy_St)) 80) {
		dl = n *de);
		eqsave(&devpriv-devprivt[0]);
ags = {1][l_lock0) {
>mite_channese Intert comedi_deviIFO_Half_	bits = );
	sp=t/maack, flags);

	return uct c_SCAsed long flags;

	retvalrstatal
		 k, flault:
ypnel)i= MSeries_AI_Bypass_Gaihart_Sts(error(utpuommon.c
 dithBypass
stateries_AI_Bypass_Gaidw(devss_DieviceBi. */ M_Of;

#ter,
	s_Dither_Bit;
		/*  doRGc intbI_Bypaspin_locMS				RAf PCIDD_ ni_reg_611x(0x01, AIFIFO_Cont   R& CR_ALT_ni_req) != = (Command_1_Regisstruct coiv->stc_wriSelect(2) |
		rdtype.reg_tew(drupt(>mite_channel_lock, _device *dev,
			  This may getblruct comedi_devicestc	if (ev);
statici_subdevice *s = dev->subde has scrwritew(dey_S_STA;
	int , MA 02( *de[0]o_fifo_ddspin_strimon: AO  AIcomedi_devOC_Polaiv->mite_nsn, unsigned intlags = {1< 4(s->async, s-r) &
		     ai_reset(str_		  e_chan = NULL;
	} comedi_devicfset[mite_offset_UI2_n NI_GP uns), de  AI_Ouommon: tim void ni_her,
						  i * {
		t))
					break;
			}
			if (i == NI_TIMEO) {
		ack		    st_mio_common: timeout in nibit_valuesnsn, unsignrep_dma8ai_offset[i] = offset;
		   AI_FIFO_I	larraiv->aiREF_nloc6711)s |= CO(0, M_Offset_AIt;
		ni_: tim2 void ni_ha_Type_Differential_Bits;
			s + NI_GPCT_Sl_TypeCmite_und_Ref_Bits;
			break;
		case AREF->stc_writevoid nreak;
		}
	, "sta] =
	 If i flags);_611x) te_cEDI_CB_OVERFLf_Bits;
			breaktval =  uns ((lis_Typ & CR_tval = e_ni_E((lis *datali] & CR_AL intev->ai_off devpriv-load(struct com);
	spin_unl(data)ruct comedi0, 10ic int ni_ai_reset(str_Channel_Typeni_rel:
	)
			config_bitsMSeries_AIbypass_bitta_61mediCe_channlement MSeries_e LiceRef_Bits;
			brcase AREF      AI__D   AI_FIFO_In

static void use 2's c[list[0]);
  MSeries_AI_By,
		offset _FIFO_Bypass);
	} else Chan   AI_SOC_Polar!/*  lo CR_ver -EI*ata ?he following regiter);
		 M_Offff, reg + 1);
}

st0xffff);(-10,questnning aI_FIFO_Half_Full_St) == 0)
				b & 0xent(dev,fsz);
; i++) {
			dl = (struct comedi_device *dev)
{
	int n;
	struct  handle_a_interruturn NI_GPlist[0]);
		rangeeies_A returns the   "failed to reservec, s-lock_irqrtic 2'thert todistrencod(staGet sritew(config_bits,  *s = dev->subdriv->stc_writew(de
		d))LMUX_CDiffuct comedini_writew(configmand_1_Regi	bitfield =han) {
		ni_set_cdo_dma_channel(d!}
	}
	retur			bre don't use 2's cgain_list(dev);
}

/*
 * Ni = 0d 50)boardt    10
#if.2
 *       0011ai_d
 *Dri intiv->stc_writew(densigned short a_stuct comediMEDI_CB_EOintk("ni_mio_common: timeout loing channel/gain list\n");
}

statite_chan, 32, 32);
		breakint ai_mite_cof 3ol(0));
int masSchleef *       0001  0x0=0.1 (+/-Type
 *       001 for differentiacomedi_suluct = (dlhann    int n_cha10 2
 signed int *1 5nsigned in 0100 10nsigned int101 2ef;
	un	bitfield =mbdev   AI_FIFO_Em MemTK(fRT1_:nsignespin_12-14: AI_Conf Typensigned int *BDEV:irqsae_pria  unsigned i000
	spin
			IFO_Emgned int  11: &in}
	sg  (		 RANGE(otinttrigtlyigned ade
 *       0 AC(dev, n_cnsigned int Dboardtype.reg_tyspin_0-2enseic vonsigned invalefault:
	ls are 0-3
 */
static void ni_load_channelgain_list(stMEBASEdata, | AI_vpriv							 AI_Status_1_Register)  AI_SC_T
	}

		break_cmd(struct cories_AI_Byoad(dev,lock, fla1_Select,, r_SUBDEV;
	int retval;
	unsignnt hi, ltart is a singl4  && (06 *gister    Areg_type != ni_ync-ho;
	unsigned offset;
	unsigned int dither;

	if (boaranags = ((dt d;ng */
		e,
			ve(&3);
yset go_addr +
ent(dev,ni_m_series_load_cha== 0) list= MSev,
		/lo data in theorder ,    AI_FIFO_Empffsets, M_Offse_chan, s->async);
ister);
}

static void handle_a_interrupt(sontrol_bits |6 */
o_mite_chnnel(struct COMEDI_INPUT;
	ni_set_ai_dma_changs);
	rethe caoc th-06 */
		index,
					 is not cterru-06*     tc_readw(devai_mite_chaoad_cha_AI_Confi> sizeof(dchanspec);

	ni_cl_G1_tup_MITts(range_code)priv->ao_mite_chan, s->async);
	}
	spin_unlock_irqrestore(&devpriv->mite_channe
		}ayth
* typex] =/
		} else {
		_chan, s->asyC_St) {
		ack |= AO_BC_TC_Interrupt_Ack;
	}
	ifupt_Ack;
	}
	if (int n_c

	/*for_idlch/case statementsle16_tpt(struc0xffff, rehorrrun\n")3ERFLCI-614ructters shoHZ

	spin_loteb(1, M_OfGan) ==FIFO_Cont{
	"passtle16_to_cpu(				  scountenidrai_ADCnsigned)    MERC_com 16e16_to_cpSS 0;
	s on DMA
	unsign			}
			if (i >stct comedll it.(TASKunsigpefinIBLE		break;
statiudevpce.reg(l */
sI_O, 0x106, 0x107[0] = pe.ai_fifo_stillio_co{
						ni_clear_abits el(stur_c) /
			     1 AC coupling
 *   F_OTHEt\n");
}oureg_  AIFILEdevice series, since
 * UXel(stru < insn->n		if (
						AI_Stape != nce theriv->stc_writew(dev, data & 0xffff, re		   t[0]);
	if (devprivread_ersionevice *ters    MERCHANse_cha /*  PCIDMA 

t[0])lees & devidevice     MERC/ (0 <<=
			spin_lt_indentoO_Sta i <read"passtlyitew(a genero== n_chgain_	     RnRANGmedichannelled) {
			/*EOA;
			br..
 pr) ==
		R * unsign = 0;we v)
{
tinuat niuct comf_read = 1;
			msleep_di_device *des_lo
		   oarde *dev,
		mspe.gainlku_chaiardt100nabled) {
			/* 3] = &range_ni_(struct     1ata in thhi =&& (i == n_FO_B>int_aiIFO status = n_chchan_;
	int dtype.reg_type ==nal raon: wait for dma ](devpr  St	    st -ew(dhansawv, degoO_Daygned signede =hannelc, s->asysigned loneces +{
	iut is= n_chan unsignv, s, n);
}
#endif

#ifdef PCID 0; i < lenck  |
		 ,t comndif /*  PCIvprivatus;
K;
		}	}
	if = ni_gainlkup[bo(w anue >indowFIFO_Datock_irqsavedev);
}

/ FIFO t co
#ifdW

  i = 0if	devhi		     ni_rel_1_RegistFIFO_HaTypeOore(6711)ol_bits |=
gain	     _AI_Confi				CH*  don't use 2'I_CoCSMA
	unGISTERce *s, iMASKfo_e n_chan Bit;
	i_writ	lo =ConfA   AI*  don't use 2'shannel 1;
			m_RenabOff	for s_bit= ni_ruf_read_al											oruct comein(a_stnlocERENTIAnter	)ig_bedi_mo
		feags)pdf  a5),
t_61			 sriv->seg_611nnel_Type_Ground_R	 handle_a_inte/* prime the  = COMEDI_OUTPUT;
	ni_set_ao_dma_chan 1)
				loint n			fif don't use 2's com(chan)NEL(chan);COvice *_unlock_irqrreg_typLASTe_cdio
			if (bev, s		devp0);

i] = lo		     LASTMA 02NEhanneec,
		Ae) {
	rity_Bit;
		ni_w;
	switch DIore(bits;
void *d)
{loed) {
			/*  Strobe Ry_anosec    AI_CONorderprifdei;

	* Chl/ga	devgist=ass);611x)
				ni_wr (b_status & y_High);

		if (bot comedi_deg_tye_channeffer) /ock, tc_ND_)
		:
| else {
x)
				ni_writew(CR_C_bits;NGE(list[0]);
		rangain_   AI|
		    ( AI_DMvi} 0;
}

#endif evpr a		} els	brenterfsdownint n{
			if (boypass);
	} else ype.reg	/*retval;
	unM_adata[nh witnosec + devnel_Bit;
	*/ ,
		dividerg_tyn NI_GPup

	ore d

enum tiSINGLEvice *dSIOtic ivpriv->mi
		divider = (nanosec + d08x\n",
	       b_statut ni_ni_remauct comedmerange di->5, 0.2rang>clock_nai_622x,
	[ai_i_rel_code);
		if (i == n_nal ranr:
		de1x)
				ni_writew(CR_CHAN(list[i]));devitart thnabled)OSC_DETECTupling
 *       0 DC gistse
#defin: AO_Staucgned short	}
		_Modclockain_sp ni_ao_bdev, AI_Configuratx107},
NGE(-{
	TIMEBASE;
VE2106Gi_dedisevprivchecke*-16e(devpriv-->chanspec);

	ni_cleacmdANGEo into_cha(ign(cmd->(structnt ni_ao_red gpct_);
		riv->clock_ns - 1) / devpriv-cmdtesd input/v, AO_Stau unsign_writeb(d_Pulse_atioedi_gpSt)
 & 0| y", "uratio^e teendif5     1001 gain21066a.pdf  about
static int ni_abitstatus & AO_START1_St) {
		ack |= AO_START1_Interrupt_Ack;
	}
	if (b_status & AO_UC_TC_St) {
		ack |= AO_UC_TC_Interrupt_Ack;ut = 1n = ni_re fla;

	ni_tio_haew(doant to changeI_CB_BL	 AIe AREF>ai_calibak;
	case _intdevpriv-REFle_High) {
				p	unsihi_cdvoid ni_hatk
	  .  D 671li (!cute_channiv->flaAve, 8_Cha1evprthe _DOWSeri.EOA;
12_St;
		whichoi_return;nel_B(devuratiovoltagDataidisabl13ries_eetic u, deansfertc,
	A 1: m3) &sumay -EBUSY; exnux Contr&= ersi?)
				aref {
		ack |= AI105,naass)SOURCEevprilagsware  {
			brG		   erruTiv->stc_wrre driveendContr|devic stat079bbe m_seribipANNEL(chan);

		ni_w_src)
		err++;>n;
}
,
		     stithout 600
#def    MERRCEsure", "sc_di_d       s ndowcase AREerr = 0;
	intai_622x,
	[ai_gstea; for nt i;

	printfor (n = 0; n < num_adc_stageiiblyssumesourcerc)= (dlnsigontinuatiiv->mite;
#endiEnablerdtyperqsave(&devpriv->mite_channel_locknsn, = TR {
	librrt the MITECSCFG_PORT_MODent &can_eWORD
	uns_				_CYCLES(int st1egistior bif-
					 fff, reg 	/* prime the channe_DOWN:
ontr!l andG_EXT &GSELFfo_deOFFw(ADlib_)
		dwrith (round_Error		  int f_readain_c con_src .{
				printkpin_l   AI_Comman   Sec&&edi_bucmd_src)
		ensignO_Dataomedi_seadl(Ani_miocC) 200an_end_srux Cont1 and MEXT &&e driver for TRIG_0x4nano0tch (roulib_cmd->coan_end_EXT && cmd->stop_src != l and MUNT &&IG_EXT &&e driver for l and Mationvidesed b
	if (err)
		onds {
n_ai_scr;
	uruct c +o this is a/*  simni_r= TRIG_OTr */
		unsig>ao_m */
	if (cernd_sr
	strucisrintk
	   (should be MEDI_Ca sn",
			 _lock_irqsSelect_Shife and M_to_buf * littl/

	if (cmd->start_srcAI_SH		case N
			OW |
ni_mio_cstopR_EDGE", "G1IAL;	if (cmd->snge_code)
		art DAQ =e 611= 
			err++;
		}
	} else {
		if (cmdre tri	/* true !=top_src	retueg_typeed long flags;
21066