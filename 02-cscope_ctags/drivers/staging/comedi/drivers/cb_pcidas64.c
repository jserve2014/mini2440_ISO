/*
    comedi/drivers/cb_pcidas64.c
    This is a driver for the ComputerBoards/MeasurementComputing PCI-DAS
    64xx, 60xx, and 4020 cards.

    Author:  Frank Mori Hess <fmhess@users.sourceforge.net>
    Copyright (C) 2001, 2002 Frank Mori Hess

    Thanks also go to the following people:

    Steve Rosenbluth, for providing the source code for
    his pci-das6402 driver, and source code for working QNX pci-6402
    drivers by Greg Laird and Mariusz Bogacz.  None of the code was
    used directly here, but it was useful as an additional source of
    documentation on how to program the boards.

    John Sims, for much testing and feedback on pcidas-4020 support.

    COMEDI - Linux Control and Measurement Device Interface
    Copyright (C) 1997-8 David A. Schleef <ds@schleef.org>

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

************************************************************************/

/*

Driver: cb_pcidas64
Description: MeasurementComputing PCI-DAS64xx, 60XX, and 4020 series with the PLX 9080 PCI controller
Author: Frank Mori Hess <fmhess@users.sourceforge.net>
Status: works
Updated: 2002-10-09
Devices: [Measurement Computing] PCI-DAS6402/16 (cb_pcidas64),
  PCI-DAS6402/12, PCI-DAS64/M1/16, PCI-DAS64/M2/16,
  PCI-DAS64/M3/16, PCI-DAS6402/16/JR, PCI-DAS64/M1/16/JR,
  PCI-DAS64/M2/16/JR, PCI-DAS64/M3/16/JR, PCI-DAS64/M1/14,
  PCI-DAS64/M2/14, PCI-DAS64/M3/14, PCI-DAS6013, PCI-DAS6014,
  PCI-DAS6023, PCI-DAS6025, PCI-DAS6030,
  PCI-DAS6031, PCI-DAS6032, PCI-DAS6033, PCI-DAS6034,
  PCI-DAS6035, PCI-DAS6036, PCI-DAS6040, PCI-DAS6052,
  PCI-DAS6070, PCI-DAS6071, PCI-DAS4020/12

Configuration options:
   [0] - PCI bus of device (optional)
   [1] - PCI slot of device (optional)

These boards may be autocalibrated with the comedi_calibrate utility.

To select the bnc trigger input on the 4020 (instead of the dio input),
specify a nonzero channel in the chanspec.  If you wish to use an external
master clock on the 4020, you may do so by setting the scan_begin_src
to TRIG_OTHER, and using an INSN_CONFIG_TIMER_1 configuration insn
to configure the divisor to use for the external clock.

Some devices are not identified because the PCI device IDs are not yet
known. If you have such a board, please file a bug report at
https://bugs.comedi.org.

*/

/*

TODO:
	make it return error if user attempts an ai command that uses the
		external queue, and an ao command simultaneously
	user counter subdevice
	there are a number of boards this driver will support when they are
		fully released, but does not yet since the pci device id numbers
		are not yet available.
	support prescaled 100khz clock for slow pacing (not available on 6000 series?)
	make ao fifo size adjustable like ai fifo
*/

#include "../comedidev.h"
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <asm/system.h>

#include "comedi_pci.h"
#include "8253.h"
#include "8255.h"
#include "plx9080.h"
#include "comedi_fc.h"

#undef PCIDAS64_DEBUG		/*  disable debugging code */
/* #define PCIDAS64_DEBUG         enable debugging code */

#ifdef PCIDAS64_DEBUG
#define DEBUG_PRINT(format, args...)  printk(format , ## args)
#else
#define DEBUG_PRINT(format, args...)
#endif

#define TIMER_BASE 25		/*  40MHz master clock */
#define PRESCALED_TIMER_BASE	10000	/*  100kHz 'prescaled' clock for slow aquisition, maybe I'll support this someday */
#define DMA_BUFFER_SIZE 0x1000

/* maximum value that can be loaded into board's 24-bit counters*/
static const int max_counter_value = 0xffffff;

/* PCI-DAS64xxx base addresses */

/* indices of base address regions */
enum base_address_regions {
	PLX9080_BADDRINDEX = 0,
	MAIN_BADDRINDEX = 2,
	DIO_COUNTER_BADDRINDEX = 3,
};

/* priv(dev)->main_iobase registers */
enum write_only_registers {
	INTR_ENABLE_REG = 0x0,	/*  interrupt enable register */
	HW_CONFIG_REG = 0x2,	/*  hardware config register */
	DAQ_SYNC_REG = 0xc,
	DAQ_ATRIG_LOW_4020_REG = 0xc,
	ADC_CONTROL0_REG = 0x10,	/*  adc control register 0 */
	ADC_CONTROL1_REG = 0x12,	/*  adc control register 1 */
	CALIBRATION_REG = 0x14,
	ADC_SAMPLE_INTERVAL_LOWER_REG = 0x16,	/*  lower 16 bits of adc sample interval counter */
	ADC_SAMPLE_INTERVAL_UPPER_REG = 0x18,	/*  upper 8 bits of adc sample interval counter */
	ADC_DELAY_INTERVAL_LOWER_REG = 0x1a,	/*  lower 16 bits of delay interval counter */
	ADC_DELAY_INTERVAL_UPPER_REG = 0x1c,	/*  upper 8 bits of delay interval counter */
	ADC_COUNT_LOWER_REG = 0x1e,	/*  lower 16 bits of hardware conversion/scan counter */
	ADC_COUNT_UPPER_REG = 0x20,	/*  upper 8 bits of hardware conversion/scan counter */
	ADC_START_REG = 0x22,	/*  software trigger to start aquisition */
	ADC_CONVERT_REG = 0x24,	/*  initiates single conversion */
	ADC_QUEUE_CLEAR_REG = 0x26,	/*  clears adc queue */
	ADC_QUEUE_LOAD_REG = 0x28,	/*  loads adc queue */
	ADC_BUFFER_CLEAR_REG = 0x2a,
	ADC_QUEUE_HIGH_REG = 0x2c,	/*  high channel for internal queue, use adc_chan_bits() inline above */
	DAC_CONTROL0_REG = 0x50,	/*  dac control register 0 */
	DAC_CONTROL1_REG = 0x52,	/*  dac control register 0 */
	DAC_SAMPLE_INTERVAL_LOWER_REG = 0x54,	/*  lower 16 bits of dac sample interval counter */
	DAC_SAMPLE_INTERVAL_UPPER_REG = 0x56,	/*  upper 8 bits of dac sample interval counter */
	DAC_SELECT_REG = 0x60,
	DAC_START_REG = 0x64,
	DAC_BUFFER_CLEAR_REG = 0x66,	/*  clear dac buffer */
};
static inline unsigned int dac_convert_reg(unsigned int channel)
{
	return 0x70 + (2 * (channel & 0x1));
}

static inline unsigned int dac_lsb_4020_reg(unsigned int channel)
{
	return 0x70 + (4 * (channel & 0x1));
}

static inline unsigned int dac_msb_4020_reg(unsigned int channel)
{
	return 0x72 + (4 * (channel & 0x1));
}

enum read_only_registers {
	HW_STATUS_REG = 0x0,	/*  hardware status register, reading this apparently clears pending interrupts as well */
	PIPE1_READ_REG = 0x4,
	ADC_READ_PNTR_REG = 0x8,
	LOWER_XFER_REG = 0x10,
	ADC_WRITE_PNTR_REG = 0xc,
	PREPOST_REG = 0x14,
};

enum read_write_registers {
	I8255_4020_REG = 0x48,	/*  8255 offset, for 4020 only */
	ADC_QUEUE_FIFO_REG = 0x100,	/*  external channel/gain queue, uses same bits as ADC_QUEUE_LOAD_REG */
	ADC_FIFO_REG = 0x200,	/* adc data fifo */
	DAC_FIFO_REG = 0x300,	/* dac data fifo, has weird interactions with external channel queue */
};

/* priv(dev)->dio_counter_iobase registers */
enum dio_counter_registers {
	DIO_8255_OFFSET = 0x0,
	DO_REG = 0x20,
	DI_REG = 0x28,
	DIO_DIRECTION_60XX_REG = 0x40,
	DIO_DATA_60XX_REG = 0x48,
};

/* bit definitions for write-only registers */

enum intr_enable_contents {
	ADC_INTR_SRC_MASK = 0x3,	/*  bits that set adc interrupt source */
	ADC_INTR_QFULL_BITS = 0x0,	/*  interrupt fifo quater full */
	ADC_INTR_EOC_BITS = 0x1,	/*  interrupt end of conversion */
	ADC_INTR_EOSCAN_BITS = 0x2,	/*  interrupt end of scan */
	ADC_INTR_EOSEQ_BITS = 0x3,	/*  interrupt end of sequence (probably wont use this it's pretty fancy) */
	EN_ADC_INTR_SRC_BIT = 0x4,	/*  enable adc interrupt source */
	EN_ADC_DONE_INTR_BIT = 0x8,	/*  enable adc aquisition done interrupt */
	DAC_INTR_SRC_MASK = 0x30,
	DAC_INTR_QEMPTY_BITS = 0x0,
	DAC_INTR_HIGH_CHAN_BITS = 0x10,
	EN_DAC_INTR_SRC_BIT = 0x40,	/*  enable dac interrupt source */
	EN_DAC_DONE_INTR_BIT = 0x80,
	EN_ADC_ACTIVE_INTR_BIT = 0x200,	/*  enable adc active interrupt */
	EN_ADC_STOP_INTR_BIT = 0x400,	/*  enable adc stop trigger interrupt */
	EN_DAC_ACTIVE_INTR_BIT = 0x800,	/*  enable dac active interrupt */
	EN_DAC_UNDERRUN_BIT = 0x4000,	/*  enable dac underrun status bit */
	EN_ADC_OVERRUN_BIT = 0x8000,	/*  enable adc overrun status bit */
};

enum hw_config_contents {
	MASTER_CLOCK_4020_MASK = 0x3,	/*  bits that specify master clock source for 4020 */
	INTERNAL_CLOCK_4020_BITS = 0x1,	/*  use 40 MHz internal master clock for 4020 */
	BNC_CLOCK_4020_BITS = 0x2,	/*  use BNC input for master clock */
	EXT_CLOCK_4020_BITS = 0x3,	/*  use dio input for master clock */
	EXT_QUEUE_BIT = 0x200,	/*  use external channel/gain queue (more versatile than internal queue) */
	SLOW_DAC_BIT = 0x400,	/*  use 225 nanosec strobe when loading dac instead of 50 nanosec */
	HW_CONFIG_DUMMY_BITS = 0x2000,	/*  bit with unknown function yet given as default value in pci-das64 manual */
	DMA_CH_SELECT_BIT = 0x8000,	/*  bit selects channels 1/0 for analog input/output, otherwise 0/1 */
	FIFO_SIZE_REG = 0x4,	/*  allows adjustment of fifo sizes */
	DAC_FIFO_SIZE_MASK = 0xff00,	/*  bits that set dac fifo size */
	DAC_FIFO_BITS = 0xf800,	/* 8k sample ao fifo */
};
#define DAC_FIFO_SIZE 0x2000

enum daq_atrig_low_4020_contents {
	EXT_AGATE_BNC_BIT = 0x8000,	/*  use trig/ext clk bnc input for analog gate signal */
	EXT_STOP_TRIG_BNC_BIT = 0x4000,	/*  use trig/ext clk bnc input for external stop trigger signal */
	EXT_START_TRIG_BNC_BIT = 0x2000,	/*  use trig/ext clk bnc input for external start trigger signal */
};
static inline uint16_t analog_trig_low_threshold_bits(uint16_t threshold)
{
	return threshold & 0xfff;
}

enum adc_control0_contents {
	ADC_GATE_SRC_MASK = 0x3,	/*  bits that select gate */
	ADC_SOFT_GATE_BITS = 0x1,	/*  software gate */
	ADC_EXT_GATE_BITS = 0x2,	/*  external digital gate */
	ADC_ANALOG_GATE_BITS = 0x3,	/*  analog level gate */
	ADC_GATE_LEVEL_BIT = 0x4,	/*  level-sensitive gate (for digital) */
	ADC_GATE_POLARITY_BIT = 0x8,	/*  gate active low */
	ADC_START_TRIG_SOFT_BITS = 0x10,
	ADC_START_TRIG_EXT_BITS = 0x20,
	ADC_START_TRIG_ANALOG_BITS = 0x30,
	ADC_START_TRIG_MASK = 0x30,
	ADC_START_TRIG_FALLING_BIT = 0x40,	/*  trig 1 uses falling edge */
	ADC_EXT_CONV_FALLING_BIT = 0x800,	/*  external pacing uses falling edge */
	ADC_SAMPLE_COUNTER_EN_BIT = 0x1000,	/*  enable hardware scan counter */
	ADC_DMA_DISABLE_BIT = 0x4000,	/*  disables dma */
	ADC_ENABLE_BIT = 0x8000,	/*  master adc enable */
};

enum adc_control1_contents {
	ADC_QUEUE_CONFIG_BIT = 0x1,	/*  should be set for boards with > 16 channels */
	CONVERT_POLARITY_BIT = 0x10,
	EOC_POLARITY_BIT = 0x20,
	ADC_SW_GATE_BIT = 0x40,	/*  software gate of adc */
	ADC_DITHER_BIT = 0x200,	/*  turn on extra noise for dithering */
	RETRIGGER_BIT = 0x800,
	ADC_LO_CHANNEL_4020_MASK = 0x300,
	ADC_HI_CHANNEL_4020_MASK = 0xc00,
	TWO_CHANNEL_4020_BITS = 0x1000,	/*  two channel mode for 4020 */
	FOUR_CHANNEL_4020_BITS = 0x2000,	/*  four channel mode for 4020 */
	CHANNEL_MODE_4020_MASK = 0x3000,
	ADC_MODE_MASK = 0xf000,
};
static inline uint16_t adc_lo_chan_4020_bits(unsigned int channel)
{
	return (channel & 0x3) << 8;
};

static inline uint16_t adc_hi_chan_4020_bits(unsigned int channel)
{
	return (channel & 0x3) << 10;
};

static inline uint16_t adc_mode_bits(unsigned int mode)
{
	return (mode & 0xf) << 12;
};

enum calibration_contents {
	SELECT_8800_BIT = 0x1,
	SELECT_8402_64XX_BIT = 0x2,
	SELECT_1590_60XX_BIT = 0x2,
	CAL_EN_64XX_BIT = 0x40,	/*  calibration enable for 64xx series */
	SERIAL_DATA_IN_BIT = 0x80,
	SERIAL_CLOCK_BIT = 0x100,
	CAL_EN_60XX_BIT = 0x200,	/*  calibration enable for 60xx series */
	CAL_GAIN_BIT = 0x800,
};
/* calibration sources for 6025 are:
 *  0 : ground
 *  1 : 10V
 *  2 : 5V
 *  3 : 0.5V
 *  4 : 0.05V
 *  5 : ground
 *  6 : dac channel 0
 *  7 : dac channel 1
 */
static inline uint16_t adc_src_bits(unsigned int source)
{
	return (source & 0xf) << 3;
};

static inline uint16_t adc_convert_chan_4020_bits(unsigned int channel)
{
	return (channel & 0x3) << 8;
};

enum adc_queue_load_contents {
	UNIP_BIT = 0x800,	/*  unipolar/bipolar bit */
	ADC_SE_DIFF_BIT = 0x1000,	/*  single-ended/ differential bit */
	ADC_COMMON_BIT = 0x2000,	/*  non-referenced single-ended (common-mode input) */
	QUEUE_EOSEQ_BIT = 0x4000,	/*  queue end of sequence */
	QUEUE_EOSCAN_BIT = 0x8000,	/*  queue end of scan */
};
static inline uint16_t adc_chan_bits(unsigned int channel)
{
	return channel & 0x3f;
};

enum dac_control0_contents {
	DAC_ENABLE_BIT = 0x8000,	/*  dac controller enable bit */
	DAC_CYCLIC_STOP_BIT = 0x4000,
	DAC_WAVEFORM_MODE_BIT = 0x100,
	DAC_EXT_UPDATE_FALLING_BIT = 0x80,
	DAC_EXT_UPDATE_ENABLE_BIT = 0x40,
	WAVEFORM_TRIG_MASK = 0x30,
	WAVEFORM_TRIG_DISABLED_BITS = 0x0,
	WAVEFORM_TRIG_SOFT_BITS = 0x10,
	WAVEFORM_TRIG_EXT_BITS = 0x20,
	WAVEFORM_TRIG_ADC1_BITS = 0x30,
	WAVEFORM_TRIG_FALLING_BIT = 0x8,
	WAVEFORM_GATE_LEVEL_BIT = 0x4,
	WAVEFORM_GATE_ENABLE_BIT = 0x2,
	WAVEFORM_GATE_SELECT_BIT = 0x1,
};

enum dac_control1_contents {
	DAC_WRITE_POLARITY_BIT = 0x800,	/* board-dependent setting */
	DAC1_EXT_REF_BIT = 0x200,
	DAC0_EXT_REF_BIT = 0x100,
	DAC_OUTPUT_ENABLE_BIT = 0x80,	/*  dac output enable bit */
	DAC_UPDATE_POLARITY_BIT = 0x40,	/* board-dependent setting */
	DAC_SW_GATE_BIT = 0x20,
	DAC1_UNIPOLAR_BIT = 0x8,
	DAC0_UNIPOLAR_BIT = 0x2,
};

/* bit definitions for read-only registers */
enum hw_status_contents {
	DAC_UNDERRUN_BIT = 0x1,
	ADC_OVERRUN_BIT = 0x2,
	DAC_ACTIVE_BIT = 0x4,
	ADC_ACTIVE_BIT = 0x8,
	DAC_INTR_PENDING_BIT = 0x10,
	ADC_INTR_PENDING_BIT = 0x20,
	DAC_DONE_BIT = 0x40,
	ADC_DONE_BIT = 0x80,
	EXT_INTR_PENDING_BIT = 0x100,
	ADC_STOP_BIT = 0x200,
};
static inline uint16_t pipe_full_bits(uint16_t hw_status_bits)
{
	return (hw_status_bits >> 10) & 0x3;
};

static inline unsigned int dma_chain_flag_bits(uint16_t prepost_bits)
{
	return (prepost_bits >> 6) & 0x3;
}

static inline unsigned int adc_upper_read_ptr_code(uint16_t prepost_bits)
{
	return (prepost_bits >> 12) & 0x3;
}

static inline unsigned int adc_upper_write_ptr_code(uint16_t prepost_bits)
{
	return (prepost_bits >> 14) & 0x3;
}

/* I2C addresses for 4020 */
enum i2c_addresses {
	RANGE_CAL_I2C_ADDR = 0x20,
	CALDAC0_I2C_ADDR = 0xc,
	CALDAC1_I2C_ADDR = 0xd,
};

enum range_cal_i2c_contents {
	ADC_SRC_4020_MASK = 0x70,	/*  bits that set what source the adc converter measures */
	BNC_TRIG_THRESHOLD_0V_BIT = 0x80,	/*  make bnc trig/ext clock threshold 0V instead of 2.5V */
};
static inline uint8_t adc_src_4020_bits(unsigned int source)
{
	return (source << 4) & ADC_SRC_4020_MASK;
};

static inline uint8_t attenuate_bit(unsigned int channel)
{
	/*  attenuate channel (+-5V input range) */
	return 1 << (channel & 0x3);
};

/* analog input ranges for 64xx boards */
static const struct comedi_lrange ai_ranges_64xx = {
	8,
	{
	 BIP_RANGE(10),
	 BIP_RANGE(5),
	 BIP_RANGE(2.5),
	 BIP_RANGE(1.25),
	 UNI_RANGE(10),
	 UNI_RANGE(5),
	 UNI_RANGE(2.5),
	 UNI_RANGE(1.25)
	 }
};

/* analog input ranges for 60xx boards */
static const struct comedi_lrange ai_ranges_60xx = {
	4,
	{
	 BIP_RANGE(10),
	 BIP_RANGE(5),
	 BIP_RANGE(0.5),
	 BIP_RANGE(0.05),
	 }
};

/* analog input ranges for 6030, etc boards */
static const struct comedi_lrange ai_ranges_6030 = {
	14,
	{
	 BIP_RANGE(10),
	 BIP_RANGE(5),
	 BIP_RANGE(2),
	 BIP_RANGE(1),
	 BIP_RANGE(0.5),
	 BIP_RANGE(0.2),
	 BIP_RANGE(0.1),
	 UNI_RANGE(10),
	 UNI_RANGE(5),
	 UNI_RANGE(2),
	 UNI_RANGE(1),
	 UNI_RANGE(0.5),
	 UNI_RANGE(0.2),
	 UNI_RANGE(0.1),
	 }
};

/* analog input ranges for 6052, etc boards */
static const struct comedi_lrange ai_ranges_6052 = {
	15,
	{
	 BIP_RANGE(10),
	 BIP_RANGE(5),
	 BIP_RANGE(2.5),
	 BIP_RANGE(1),
	 BIP_RANGE(0.5),
	 BIP_RANGE(0.25),
	 BIP_RANGE(0.1),
	 BIP_RANGE(0.05),
	 UNI_RANGE(10),
	 UNI_RANGE(5),
	 UNI_RANGE(2),
	 UNI_RANGE(1),
	 UNI_RANGE(0.5),
	 UNI_RANGE(0.2),
	 UNI_RANGE(0.1),
	 }
};

/* analog input ranges for 4020 board */
static const struct comedi_lrange ai_ranges_4020 = {
	2,
	{
	 BIP_RANGE(5),
	 BIP_RANGE(1),
	 }
};

/* analog output ranges */
static const struct comedi_lrange ao_ranges_64xx = {
	4,
	{
	 BIP_RANGE(5),
	 BIP_RANGE(10),
	 UNI_RANGE(5),
	 UNI_RANGE(10),
	 }
};

static const int ao_range_code_64xx[] = {
	0x0,
	0x1,
	0x2,
	0x3,
};

static const struct comedi_lrange ao_ranges_60xx = {
	1,
	{
	 BIP_RANGE(10),
	 }
};

static const int ao_range_code_60xx[] = {
	0x0,
};

static const struct comedi_lrange ao_ranges_6030 = {
	2,
	{
	 BIP_RANGE(10),
	 UNI_RANGE(10),
	 }
};

static const int ao_range_code_6030[] = {
	0x0,
	0x2,
};

static const struct comedi_lrange ao_ranges_4020 = {
	2,
	{
	 BIP_RANGE(5),
	 BIP_RANGE(10),
	 }
};

static const int ao_range_code_4020[] = {
	0x1,
	0x0,
};

enum register_layout {
	LAYOUT_60XX,
	LAYOUT_64XX,
	LAYOUT_4020,
};

struct hw_fifo_info {
	unsigned int num_segments;
	unsigned int max_segment_length;
	unsigned int sample_packing_ratio;
	uint16_t fifo_size_reg_mask;
};

struct pcidas64_board {
	const char *name;
	int device_id;		/*  pci device id */
	int ai_se_chans;	/*  number of ai inputs in single-ended mode */
	int ai_bits;		/*  analog input resolution */
	int ai_speed;		/*  fastest conversion period in ns */
	const struct comedi_lrange *ai_range_table;
	int ao_nchan;		/*  number of analog out channels */
	int ao_bits;		/*  analog output resolution */
	int ao_scan_speed;	/*  analog output speed (for a scan, not conversion) */
	const struct comedi_lrange *ao_range_table;
	const int *ao_range_code;
	const struct hw_fifo_info *const ai_fifo;
	enum register_layout layout;	/*  different board families have slightly different registers */
	unsigned has_8255:1;
};

static const struct hw_fifo_info ai_fifo_4020 = {
	.num_segments = 2,
	.max_segment_length = 0x8000,
	.sample_packing_ratio = 2,
	.fifo_size_reg_mask = 0x7f,
};

static const struct hw_fifo_info ai_fifo_64xx = {
	.num_segments = 4,
	.max_segment_length = 0x800,
	.sample_packing_ratio = 1,
	.fifo_size_reg_mask = 0x3f,
};

static const struct hw_fifo_info ai_fifo_60xx = {
	.num_segments = 4,
	.max_segment_length = 0x800,
	.sample_packing_ratio = 1,
	.fifo_size_reg_mask = 0x7f,
};

/* maximum number of dma transfers we will chain together into a ring
 * (and the maximum number of dma buffers we maintain) */
#define MAX_AI_DMA_RING_COUNT (0x80000 / DMA_BUFFER_SIZE)
#define MIN_AI_DMA_RING_COUNT (0x10000 / DMA_BUFFER_SIZE)
#define AO_DMA_RING_COUNT (0x10000 / DMA_BUFFER_SIZE)
static inline unsigned int ai_dma_ring_count(struct pcidas64_board *board)
{
	if (board->layout == LAYOUT_4020)
		return MAX_AI_DMA_RING_COUNT;
	else
		return MIN_AI_DMA_RING_COUNT;
}

static const int bytes_in_sample = 2;

static const struct pcidas64_board pcidas64_boards[] = {
	{
	 .name = "pci-das6402/16",
	 .device_id = 0x1d,
	 .ai_se_chans = 64,
	 .ai_bits = 16,
	 .ai_speed = 5000,
	 .ao_nchan = 2,
	 .ao_bits = 16,
	 .ao_scan_speed = 10000,
	 .layout = LAYOUT_64XX,
	 .ai_range_table = &ai_ranges_64xx,
	 .ao_range_table = &ao_ranges_64xx,
	 .ao_range_code = ao_range_code_64xx,
	 .ai_fifo = &ai_fifo_64xx,
	 .has_8255 = 1,
	 },
	{
	 .name = "pci-das6402/12",	/*  XXX check */
	 .device_id = 0x1e,
	 .ai_se_chans = 64,
	 .ai_bits = 12,
	 .ai_speed = 5000,
	 .ao_nchan = 2,
	 .ao_bits = 12,
	 .ao_scan_speed = 10000,
	 .layout = LAYOUT_64XX,
	 .ai_range_table = &ai_ranges_64xx,
	 .ao_range_table = &ao_ranges_64xx,
	 .ao_range_code = ao_range_code_64xx,
	 .ai_fifo = &ai_fifo_64xx,
	 .has_8255 = 1,
	 },
	{
	 .name = "pci-das64/m1/16",
	 .device_id = 0x35,
	 .ai_se_chans = 64,
	 .ai_bits = 16,
	 .ai_speed = 1000,
	 .ao_nchan = 2,
	 .ao_bits = 16,
	 .ao_scan_speed = 10000,
	 .layout = LAYOUT_64XX,
	 .ai_range_table = &ai_ranges_64xx,
	 .ao_range_table = &ao_ranges_64xx,
	 .ao_range_code = ao_range_code_64xx,
	 .ai_fifo = &ai_fifo_64xx,
	 .has_8255 = 1,
	 },
	{
	 .name = "pci-das64/m2/16",
	 .device_id = 0x36,
	 .ai_se_chans = 64,
	 .ai_bits = 16,
	 .ai_speed = 500,
	 .ao_nchan = 2,
	 .ao_bits = 16,
	 .ao_scan_speed = 10000,
	 .layout = LAYOUT_64XX,
	 .ai_range_table = &ai_ranges_64xx,
	 .ao_range_table = &ao_ranges_64xx,
	 .ao_range_code = ao_range_code_64xx,
	 .ai_fifo = &ai_fifo_64xx,
	 .has_8255 = 1,
	 },
	{
	 .name = "pci-das64/m3/16",
	 .device_id = 0x37,
	 .ai_se_chans = 64,
	 .ai_bits = 16,
	 .ai_speed = 333,
	 .ao_nchan = 2,
	 .ao_bits = 16,
	 .ao_scan_speed = 10000,
	 .layout = LAYOUT_64XX,
	 .ai_range_table = &ai_ranges_64xx,
	 .ao_range_table = &ao_ranges_64xx,
	 .ao_range_code = ao_range_code_64xx,
	 .ai_fifo = &ai_fifo_64xx,
	 .has_8255 = 1,
	 },
	{
	 .name = "pci-das6013",
	 .device_id = 0x78,
	 .ai_se_chans = 16,
	 .ai_bits = 16,
	 .ai_speed = 5000,
	 .ao_nchan = 0,
	 .ao_bits = 16,
	 .layout = LAYOUT_60XX,
	 .ai_range_table = &ai_ranges_60xx,
	 .ao_range_table = &ao_ranges_60xx,
	 .ao_range_code = ao_range_code_60xx,
	 .ai_fifo = &ai_fifo_60xx,
	 .has_8255 = 0,
	 },
	{
	 .name = "pci-das6014",
	 .device_id = 0x79,
	 .ai_se_chans = 16,
	 .ai_bits = 16,
	 .ai_speed = 5000,
	 .ao_nchan = 2,
	 .ao_bits = 16,
	 .ao_scan_speed = 100000,
	 .layout = LAYOUT_60XX,
	 .ai_range_table = &ai_ranges_60xx,
	 .ao_range_table = &ao_ranges_60xx,
	 .ao_range_code = ao_range_code_60xx,
	 .ai_fifo = &ai_fifo_60xx,
	 .has_8255 = 0,
	 },
	{
	 .name = "pci-das6023",
	 .device_id = 0x5d,
	 .ai_se_chans = 16,
	 .ai_bits = 12,
	 .ai_speed = 5000,
	 .ao_nchan = 0,
	 .ao_scan_speed = 100000,
	 .layout = LAYOUT_60XX,
	 .ai_range_table = &ai_ranges_60xx,
	 .ao_range_table = &ao_ranges_60xx,
	 .ao_range_code = ao_range_code_60xx,
	 .ai_fifo = &ai_fifo_60xx,
	 .has_8255 = 1,
	 },
	{
	 .name = "pci-das6025",
	 .device_id = 0x5e,
	 .ai_se_chans = 16,
	 .ai_bits = 12,
	 .ai_speed = 5000,
	 .ao_nchan = 2,
	 .ao_bits = 12,
	 .ao_scan_speed = 100000,
	 .layout = LAYOUT_60XX,
	 .ai_range_table = &ai_ranges_60xx,
	 .ao_range_table = &ao_ranges_60xx,
	 .ao_range_code = ao_range_code_60xx,
	 .ai_fifo = &ai_fifo_60xx,
	 .has_8255 = 1,
	 },
	{
	 .name = "pci-das6030",
	 .device_id = 0x5f,
	 .ai_se_chans = 16,
	 .ai_bits = 16,
	 .ai_speed = 10000,
	 .ao_nchan = 2,
	 .ao_bits = 16,
	 .ao_scan_speed = 10000,
	 .layout = LAYOUT_60XX,
	 .ai_range_table = &ai_ranges_6030,
	 .ao_range_table = &ao_ranges_6030,
	 .ao_range_code = ao_range_code_6030,
	 .ai_fifo = &ai_fifo_60xx,
	 .has_8255 = 0,
	 },
	{
	 .name = "pci-das6031",
	 .device_id = 0x60,
	 .ai_se_chans = 64,
	 .ai_bits = 16,
	 .ai_speed = 10000,
	 .ao_nchan = 2,
	 .ao_bits = 16,
	 .ao_scan_speed = 10000,
	 .layout = LAYOUT_60XX,
	 .ai_range_table = &ai_ranges_6030,
	 .ao_range_table = &ao_ranges_6030,
	 .ao_range_code = ao_range_code_6030,
	 .ai_fifo = &ai_fifo_60xx,
	 .has_8255 = 0,
	 },
	{
	 .name = "pci-das6032",
	 .device_id = 0x61,
	 .ai_se_chans = 16,
	 .ai_bits = 16,
	 .ai_speed = 10000,
	 .ao_nchan = 0,
	 .layout = LAYOUT_60XX,
	 .ai_range_table = &ai_ranges_6030,
	 .ai_fifo = &ai_fifo_60xx,
	 .has_8255 = 0,
	 },
	{
	 .name = "pci-das6033",
	 .device_id = 0x62,
	 .ai_se_chans = 64,
	 .ai_bits = 16,
	 .ai_speed = 10000,
	 .ao_nchan = 0,
	 .layout = LAYOUT_60XX,
	 .ai_range_table = &ai_ranges_6030,
	 .ai_fifo = &ai_fifo_60xx,
	 .has_8255 = 0,
	 },
	{
	 .name = "pci-das6034",
	 .device_id = 0x63,
	 .ai_se_chans = 16,
	 .ai_bits = 16,
	 .ai_speed = 5000,
	 .ao_nchan = 0,
	 .ao_scan_speed = 0,
	 .layout = LAYOUT_60XX,
	 .ai_range_table = &ai_ranges_60xx,
	 .ai_fifo = &ai_fifo_60xx,
	 .has_8255 = 0,
	 },
	{
	 .name = "pci-das6035",
	 .device_id = 0x64,
	 .ai_se_chans = 16,
	 .ai_bits = 16,
	 .ai_speed = 5000,
	 .ao_nchan = 2,
	 .ao_bits = 12,
	 .ao_scan_speed = 100000,
	 .layout = LAYOUT_60XX,
	 .ai_range_table = &ai_ranges_60xx,
	 .ao_range_table = &ao_ranges_60xx,
	 .ao_range_code = ao_range_code_60xx,
	 .ai_fifo = &ai_fifo_60xx,
	 .has_8255 = 0,
	 },
	{
	 .name = "pci-das6036",
	 .device_id = 0x6f,
	 .ai_se_chans = 16,
	 .ai_bits = 16,
	 .ai_speed = 5000,
	 .ao_nchan = 2,
	 .ao_bits = 16,
	 .ao_scan_speed = 100000,
	 .layout = LAYOUT_60XX,
	 .ai_range_table = &ai_ranges_60xx,
	 .ao_range_table = &ao_ranges_60xx,
	 .ao_range_code = ao_range_code_60xx,
	 .ai_fifo = &ai_fifo_60xx,
	 .has_8255 = 0,
	 },
	{
	 .name = "pci-das6040",
	 .device_id = 0x65,
	 .ai_se_chans = 16,
	 .ai_bits = 12,
	 .ai_speed = 2000,
	 .ao_nchan = 2,
	 .ao_bits = 12,
	 .ao_scan_speed = 1000,
	 .layout = LAYOUT_60XX,
	 .ai_range_table = &ai_ranges_6052,
	 .ao_range_table = &ao_ranges_6030,
	 .ao_range_code = ao_range_code_6030,
	 .ai_fifo = &ai_fifo_60xx,
	 .has_8255 = 0,
	 },
	{
	 .name = "pci-das6052",
	 .device_id = 0x66,
	 .ai_se_chans = 16,
	 .ai_bits = 16,
	 .ai_speed = 3333,
	 .ao_nchan = 2,
	 .ao_bits = 16,
	 .ao_scan_speed = 3333,
	 .layout = LAYOUT_60XX,
	 .ai_range_table = &ai_ranges_6052,
	 .ao_range_table = &ao_ranges_6030,
	 .ao_range_code = ao_range_code_6030,
	 .ai_fifo = &ai_fifo_60xx,
	 .has_8255 = 0,
	 },
	{
	 .name = "pci-das6070",
	 .device_id = 0x67,
	 .ai_se_chans = 16,
	 .ai_bits = 12,
	 .ai_speed = 800,
	 .ao_nchan = 2,
	 .ao_bits = 12,
	 .ao_scan_speed = 1000,
	 .layout = LAYOUT_60XX,
	 .ai_range_table = &ai_ranges_6052,
	 .ao_range_table = &ao_ranges_6030,
	 .ao_range_code = ao_range_code_6030,
	 .ai_fifo = &ai_fifo_60xx,
	 .has_8255 = 0,
	 },
	{
	 .name = "pci-das6071",
	 .device_id = 0x68,
	 .ai_se_chans = 64,
	 .ai_bits = 12,
	 .ai_speed = 800,
	 .ao_nchan = 2,
	 .ao_bits = 12,
	 .ao_scan_speed = 1000,
	 .layout = LAYOUT_60XX,
	 .ai_range_table = &ai_ranges_6052,
	 .ao_range_table = &ao_ranges_6030,
	 .ao_range_code = ao_range_code_6030,
	 .ai_fifo = &ai_fifo_60xx,
	 .has_8255 = 0,
	 },
	{
	 .name = "pci-das4020/12",
	 .device_id = 0x52,
	 .ai_se_chans = 4,
	 .ai_bits = 12,
	 .ai_speed = 50,
	 .ao_bits = 12,
	 .ao_nchan = 2,
	 .ao_scan_speed = 0,	/*  no hardware pacing on ao */
	 .layout = LAYOUT_4020,
	 .ai_range_table = &ai_ranges_4020,
	 .ao_range_table = &ao_ranges_4020,
	 .ao_range_code = ao_range_code_4020,
	 .ai_fifo = &ai_fifo_4020,
	 .has_8255 = 1,
	 },
#if 0
	{
	 .name = "pci-das6402/16/jr",
	 .device_id = 0		/*  XXX, */
	 .ai_se_chans = 64,
	 .ai_bits = 16,
	 .ai_speed = 5000,
	 .ao_nchan = 0,
	 .ao_scan_speed = 10000,
	 .layout = LAYOUT_64XX,
	 .ai_range_table = &ai_ranges_64xx,
	 .ai_fifo = ai_fifo_64xx,
	 .has_8255 = 1,
	 },
	{
	 .name = "pci-das64/m1/16/jr",
	 .device_id = 0		/*  XXX, */
	 .ai_se_chans = 64,
	 .ai_bits = 16,
	 .ai_speed = 1000,
	 .ao_nchan = 0,
	 .ao_scan_speed = 10000,
	 .layout = LAYOUT_64XX,
	 .ai_range_table = &ai_ranges_64xx,
	 .ai_fifo = ai_fifo_64xx,
	 .has_8255 = 1,
	 },
	{
	 .name = "pci-das64/m2/16/jr",
	 .device_id = 0		/*  XXX, */
	 .ai_se_chans = 64,
	 .ai_bits = 16,
	 .ai_speed = 500,
	 .ao_nchan = 0,
	 .ao_scan_speed = 10000,
	 .layout = LAYOUT_64XX,
	 .ai_range_table = &ai_ranges_64xx,
	 .ai_fifo = ai_fifo_64xx,
	 .has_8255 = 1,
	 },
	{
	 .name = "pci-das64/m3/16/jr",
	 .device_id = 0		/*  XXX, */
	 .ai_se_chans = 64,
	 .ai_bits = 16,
	 .ai_speed = 333,
	 .ao_nchan = 0,
	 .ao_scan_speed = 10000,
	 .layout = LAYOUT_64XX,
	 .ai_range_table = &ai_ranges_64xx,
	 .ai_fifo = ai_fifo_64xx,
	 .has_8255 = 1,
	 },
	{
	 .name = "pci-das64/m1/14",
	 .device_id = 0,	/*  XXX */
	 .ai_se_chans = 64,
	 .ai_bits = 14,
	 .ai_speed = 1000,
	 .ao_nchan = 2,
	 .ao_scan_speed = 10000,
	 .layout = LAYOUT_64XX,
	 .ai_range_table = &ai_ranges_64xx,
	 .ai_fifo = ai_fifo_64xx,
	 .has_8255 = 1,
	 },
	{
	 .name = "pci-das64/m2/14",
	 .device_id = 0,	/*  XXX */
	 .ai_se_chans = 64,
	 .ai_bits = 14,
	 .ai_speed = 500,
	 .ao_nchan = 2,
	 .ao_scan_speed = 10000,
	 .layout = LAYOUT_64XX,
	 .ai_range_table = &ai_ranges_64xx,
	 .ai_fifo = ai_fifo_64xx,
	 .has_8255 = 1,
	 },
	{
	 .name = "pci-das64/m3/14",
	 .device_id = 0,	/*  XXX */
	 .ai_se_chans = 64,
	 .ai_bits = 14,
	 .ai_speed = 333,
	 .ao_nchan = 2,
	 .ao_scan_speed = 10000,
	 .layout = LAYOUT_64XX,
	 .ai_range_table = &ai_ranges_64xx,
	 .ai_fifo = ai_fifo_64xx,
	 .has_8255 = 1,
	 },
#endif
};

static DEFINE_PCI_DEVICE_TABLE(pcidas64_pci_table) = {
	{
	PCI_VENDOR_ID_COMPUTERBOARDS, 0x001d, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{
	PCI_VENDOR_ID_COMPUTERBOARDS, 0x001e, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{
	PCI_VENDOR_ID_COMPUTERBOARDS, 0x0035, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{
	PCI_VENDOR_ID_COMPUTERBOARDS, 0x0036, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{
	PCI_VENDOR_ID_COMPUTERBOARDS, 0x0037, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{
	PCI_VENDOR_ID_COMPUTERBOARDS, 0x0052, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{
	PCI_VENDOR_ID_COMPUTERBOARDS, 0x005d, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{
	PCI_VENDOR_ID_COMPUTERBOARDS, 0x005e, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{
	PCI_VENDOR_ID_COMPUTERBOARDS, 0x005f, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{
	PCI_VENDOR_ID_COMPUTERBOARDS, 0x0061, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{
	PCI_VENDOR_ID_COMPUTERBOARDS, 0x0062, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{
	PCI_VENDOR_ID_COMPUTERBOARDS, 0x0063, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{
	PCI_VENDOR_ID_COMPUTERBOARDS, 0x0064, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{
	PCI_VENDOR_ID_COMPUTERBOARDS, 0x0066, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{
	PCI_VENDOR_ID_COMPUTERBOARDS, 0x0067, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{
	PCI_VENDOR_ID_COMPUTERBOARDS, 0x0068, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{
	PCI_VENDOR_ID_COMPUTERBOARDS, 0x006f, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{
	PCI_VENDOR_ID_COMPUTERBOARDS, 0x0078, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{
	PCI_VENDOR_ID_COMPUTERBOARDS, 0x0079, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{
	0}
};

MODULE_DEVICE_TABLE(pci, pcidas64_pci_table);

static inline struct pcidas64_board *board(const struct comedi_device *dev)
{
	return (struct pcidas64_board *)dev->board_ptr;
}

static inline unsigned short se_diff_bit_6xxx(struct comedi_device *dev,
					      int use_differential)
{
	if ((board(dev)->layout == LAYOUT_64XX && !use_differential) ||
	    (board(dev)->layout == LAYOUT_60XX && use_differential))
		return ADC_SE_DIFF_BIT;
	else
		return 0;
};

struct ext_clock_info {
	unsigned int divisor;	/*  master clock divisor to use for scans with external master clock */
	unsigned int chanspec;	/*  chanspec for master clock input when used as scan begin src */
};

/* this structure is for data unique to this hardware driver. */
struct pcidas64_private {

	struct pci_dev *hw_dev;	/*  pointer to board's pci_dev struct */
	/*  base addresses (physical) */
	resource_size_t plx9080_phys_iobase;
	resource_size_t main_phys_iobase;
	resource_size_t dio_counter_phys_iobase;
	/*  base addresses (ioremapped) */
	void *plx9080_iobase;
	void *main_iobase;
	void *dio_counter_iobase;
	/*  local address (used by dma controller) */
	uint32_t local0_iobase;
	uint32_t local1_iobase;
	volatile unsigned int ai_count;	/*  number of analog input samples remaining */
	uint16_t *ai_buffer[MAX_AI_DMA_RING_COUNT];	/*  dma buffers for analog input */
	dma_addr_t ai_buffer_bus_addr[MAX_AI_DMA_RING_COUNT];	/*  physical addresses of ai dma buffers */
	struct plx_dma_desc *ai_dma_desc;	/*  array of ai dma descriptors read by plx9080, allocated to get proper alignment */
	dma_addr_t ai_dma_desc_bus_addr;	/*  physical address of ai dma descriptor array */
	volatile unsigned int ai_dma_index;	/*  index of the ai dma descriptor/buffer that is currently being used */
	uint16_t *ao_buffer[AO_DMA_RING_COUNT];	/*  dma buffers for analog output */
	dma_addr_t ao_buffer_bus_addr[AO_DMA_RING_COUNT];	/*  physical addresses of ao dma buffers */
	struct plx_dma_desc *ao_dma_desc;
	dma_addr_t ao_dma_desc_bus_addr;
	volatile unsigned int ao_dma_index;	/*  keeps track of buffer where the next ao sample should go */
	volatile unsigned long ao_count;	/*  number of analog output samples remaining */
	volatile unsigned int ao_value[2];	/*  remember what the analog outputs are set to, to allow readback */
	unsigned int hw_revision;	/*  stc chip hardware revision number */
	volatile unsigned int intr_enable_bits;	/*  last bits sent to INTR_ENABLE_REG register */
	volatile uint16_t adc_control1_bits;	/*  last bits sent to ADC_CONTROL1_REG register */
	volatile uint16_t fifo_size_bits;	/*  last bits sent to FIFO_SIZE_REG register */
	volatile uint16_t hw_config_bits;	/*  last bits sent to HW_CONFIG_REG register */
	volatile uint16_t dac_control1_bits;
	volatile uint32_t plx_control_bits;	/*  last bits written to plx9080 control register */
	volatile uint32_t plx_intcsr_bits;	/*  last bits written to plx interrupt control and status register */
	volatile int calibration_source;	/*  index of calibration source readable through ai ch0 */
	volatile uint8_t i2c_cal_range_bits;	/*  bits written to i2c calibration/range register */
	volatile unsigned int ext_trig_falling;	/*  configure digital triggers to trigger on falling edge */
	/*  states of various devices stored to enable read-back */
	unsigned int ad8402_state[2];
	unsigned int caldac_state[8];
	volatile short ai_cmd_running;
	unsigned int ai_fifo_segment_length;
	struct ext_clock_info ext_clock;
	short ao_bounce_buffer[DAC_FIFO_SIZE];
};

/* inline function that makes it easier to
 * access the private structure.
 */
static inline struct pcidas64_private *priv(struct comedi_device *dev)
{
	return dev->private;
}

/*
 * The comedi_driver structure tells the Comedi core module
 * which functions to call to configure/deconfigure (attach/detach)
 * the board, and also about the kernel module that contains
 * the device code.
 */
static int attach(struct comedi_device *dev, struct comedi_devconfig *it);
static int detach(struct comedi_device *dev);
static struct comedi_driver driver_cb_pcidas = {
	.driver_name = "cb_pcidas64",
	.module = THIS_MODULE,
	.attach = attach,
	.detach = detach,
};

static int ai_rinsn(struct comedi_device *dev, struct comedi_subdevice *s,
		    struct comedi_insn *insn, unsigned int *data);
static int ai_config_insn(struct comedi_device *dev, struct comedi_subdevice *s,
			  struct comedi_insn *insn, unsigned int *data);
static int ao_winsn(struct comedi_device *dev, struct comedi_subdevice *s,
		    struct comedi_insn *insn, unsigned int *data);
static int ao_readback_insn(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data);
static int ai_cmd(struct comedi_device *dev, struct comedi_subdevice *s);
static int ai_cmdtest(struct comedi_device *dev, struct comedi_subdevice *s,
		      struct comedi_cmd *cmd);
static int ao_cmd(struct comedi_device *dev, struct comedi_subdevice *s);
static int ao_inttrig(struct comedi_device *dev,
		      struct comedi_subdevice *subdev, unsigned int trig_num);
static int ao_cmdtest(struct comedi_device *dev, struct comedi_subdevice *s,
		      struct comedi_cmd *cmd);
static irqreturn_t handle_interrupt(int irq, void *d);
static int ai_cancel(struct comedi_device *dev, struct comedi_subdevice *s);
static int ao_cancel(struct comedi_device *dev, struct comedi_subdevice *s);
static int dio_callback(int dir, int port, int data, unsigned long arg);
static int dio_callback_4020(int dir, int port, int data, unsigned long arg);
static int di_rbits(struct comedi_device *dev, struct comedi_subdevice *s,
		    struct comedi_insn *insn, unsigned int *data);
static int do_wbits(struct comedi_device *dev, struct comedi_subdevice *s,
		    struct comedi_insn *insn, unsigned int *data);
static int dio_60xx_config_insn(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data);
static int dio_60xx_wbits(struct comedi_device *dev, struct comedi_subdevice *s,
			  struct comedi_insn *insn, unsigned int *data);
static int calib_read_insn(struct comedi_device *dev,
			   struct comedi_subdevice *s, struct comedi_insn *insn,
			   unsigned int *data);
static int calib_write_insn(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data);
static int ad8402_read_insn(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data);
static void ad8402_write(struct comedi_device *dev, unsigned int channel,
			 unsigned int value);
static int ad8402_write_insn(struct comedi_device *dev,
			     struct comedi_subdevice *s,
			     struct comedi_insn *insn, unsigned int *data);
static int eeprom_read_insn(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data);
static void check_adc_timing(struct comedi_device *dev, struct comedi_cmd *cmd);
static unsigned int get_divisor(unsigned int ns, unsigned int flags);
static void i2c_write(struct comedi_device *dev, unsigned int address,
		      const uint8_t * data, unsigned int length);
static void caldac_write(struct comedi_device *dev, unsigned int channel,
			 unsigned int value);
static int caldac_8800_write(struct comedi_device *dev, unsigned int address,
			     uint8_t value);
/* static int dac_1590_write(struct comedi_device *dev, unsigned int dac_a, unsigned int dac_b); */
static int caldac_i2c_write(struct comedi_device *dev,
			    unsigned int caldac_channel, unsigned int value);
static void abort_dma(struct comedi_device *dev, unsigned int channel);
static void disable_plx_interrupts(struct comedi_device *dev);
static int set_ai_fifo_size(struct comedi_device *dev,
			    unsigned int num_samples);
static unsigned int ai_fifo_size(struct comedi_device *dev);
static int set_ai_fifo_segment_length(struct comedi_device *dev,
				      unsigned int num_entries);
static void disable_ai_pacing(struct comedi_device *dev);
static void disable_ai_interrupts(struct comedi_device *dev);
static void enable_ai_interrupts(struct comedi_device *dev,
				 const struct comedi_cmd *cmd);
static unsigned int get_ao_divisor(unsigned int ns, unsigned int flags);
static void load_ao_dma(struct comedi_device *dev,
			const struct comedi_cmd *cmd);

COMEDI_PCI_INITCLEANUP(driver_cb_pcidas, pcidas64_pci_table);

static unsigned int ai_range_bits_6xxx(const struct comedi_device *dev,
				       unsigned int range_index)
{
	const struct comedi_krange *range =
	    &board(dev)->ai_range_table->range[range_index];
	unsigned int bits = 0;

	switch (range->max) {
	case 10000000:
		bits = 0x000;
		break;
	case 5000000:
		bits = 0x100;
		break;
	case 2000000:
	case 2500000:
		bits = 0x200;
		break;
	case 1000000:
	case 1250000:
		bits = 0x300;
		break;
	case 500000:
		bits = 0x400;
		break;
	case 200000:
	case 250000:
		bits = 0x500;
		break;
	case 100000:
		bits = 0x600;
		break;
	case 50000:
		bits = 0x700;
		break;
	default:
		comedi_error(dev, "bug! in ai_range_bits_6xxx");
		break;
	}
	if (range->min == 0)
		bits += 0x900;
	return bits;
}

static unsigned int hw_revision(const struct comedi_device *dev,
				uint16_t hw_status_bits)
{
	if (board(dev)->layout == LAYOUT_4020)
		return (hw_status_bits >> 13) & 0x7;

	return (hw_status_bits >> 12) & 0xf;
}

static void set_dac_range_bits(struct comedi_device *dev,
			       volatile uint16_t * bits, unsigned int channel,
			       unsigned int range)
{
	unsigned int code = board(dev)->ao_range_code[range];

	if (channel > 1)
		comedi_error(dev, "bug! bad channel?");
	if (code & ~0x3)
		comedi_error(dev, "bug! bad range code?");

	*bits &= ~(0x3 << (2 * channel));
	*bits |= code << (2 * channel);
};

static inline int ao_cmd_is_supported(const struct pcidas64_board *board)
{
	return board->ao_nchan && board->layout != LAYOUT_4020;
}

/* initialize plx9080 chip */
static void init_plx9080(struct comedi_device *dev)
{
	uint32_t bits;
	void *plx_iobase = priv(dev)->plx9080_iobase;

	priv(dev)->plx_control_bits =
	    readl(priv(dev)->plx9080_iobase + PLX_CONTROL_REG);

	/*  plx9080 dump */
	DEBUG_PRINT(" plx interrupt status 0x%x\n",
		    readl(plx_iobase + PLX_INTRCS_REG));
	DEBUG_PRINT(" plx id bits 0x%x\n", readl(plx_iobase + PLX_ID_REG));
	DEBUG_PRINT(" plx control reg 0x%x\n", priv(dev)->plx_control_bits);
	DEBUG_PRINT(" plx mode/arbitration reg 0x%x\n",
		    readl(plx_iobase + PLX_MARB_REG));
	DEBUG_PRINT(" plx region0 reg 0x%x\n",
		    readl(plx_iobase + PLX_REGION0_REG));
	DEBUG_PRINT(" plx region1 reg 0x%x\n",
		    readl(plx_iobase + PLX_REGION1_REG));

	DEBUG_PRINT(" plx revision 0x%x\n",
		    readl(plx_iobase + PLX_REVISION_REG));
	DEBUG_PRINT(" plx dma channel 0 mode 0x%x\n",
		    readl(plx_iobase + PLX_DMA0_MODE_REG));
	DEBUG_PRINT(" plx dma channel 1 mode 0x%x\n",
		    readl(plx_iobase + PLX_DMA1_MODE_REG));
	DEBUG_PRINT(" plx dma channel 0 pci address 0x%x\n",
		    readl(plx_iobase + PLX_DMA0_PCI_ADDRESS_REG));
	DEBUG_PRINT(" plx dma channel 0 local address 0x%x\n",
		    readl(plx_iobase + PLX_DMA0_LOCAL_ADDRESS_REG));
	DEBUG_PRINT(" plx dma channel 0 transfer size 0x%x\n",
		    readl(plx_iobase + PLX_DMA0_TRANSFER_SIZE_REG));
	DEBUG_PRINT(" plx dma channel 0 descriptor 0x%x\n",
		    readl(plx_iobase + PLX_DMA0_DESCRIPTOR_REG));
	DEBUG_PRINT(" plx dma channel 0 command status 0x%x\n",
		    readb(plx_iobase + PLX_DMA0_CS_REG));
	DEBUG_PRINT(" plx dma channel 0 threshold 0x%x\n",
		    readl(plx_iobase + PLX_DMA0_THRESHOLD_REG));
	DEBUG_PRINT(" plx bigend 0x%x\n", readl(plx_iobase + PLX_BIGEND_REG));

#ifdef __BIG_ENDIAN
	bits = BIGEND_DMA0 | BIGEND_DMA1;
#else
	bits = 0;
#endif
	writel(bits, priv(dev)->plx9080_iobase + PLX_BIGEND_REG);

	disable_plx_interrupts(dev);

	abort_dma(dev, 0);
	abort_dma(dev, 1);

	/*  configure dma0 mode */
	bits = 0;
	/*  enable ready input, not sure if this is necessary */
	bits |= PLX_DMA_EN_READYIN_BIT;
	/*  enable bterm, not sure if this is necessary */
	bits |= PLX_EN_BTERM_BIT;
	/*  enable dma chaining */
	bits |= PLX_EN_CHAIN_BIT;
	/*  enable interrupt on dma done (probably don't need this, since chain never finishes) */
	bits |= PLX_EN_DMA_DONE_INTR_BIT;
	/*  don't increment local address during transfers (we are transferring from a fixed fifo register) */
	bits |= PLX_LOCAL_ADDR_CONST_BIT;
	/*  route dma interrupt to pci bus */
	bits |= PLX_DMA_INTR_PCI_BIT;
	/*  enable demand mode */
	bits |= PLX_DEMAND_MODE_BIT;
	/*  enable local burst mode */
	bits |= PLX_DMA_LOCAL_BURST_EN_BIT;
	/*  4020 uses 32 bit dma */
	if (board(dev)->layout == LAYOUT_4020) {
		bits |= PLX_LOCAL_BUS_32_WIDE_BITS;
	} else {		/*  localspace0 bus is 16 bits wide */
		bits |= PLX_LOCAL_BUS_16_WIDE_BITS;
	}
	writel(bits, plx_iobase + PLX_DMA1_MODE_REG);
	if (ao_cmd_is_supported(board(dev)))
		writel(bits, plx_iobase + PLX_DMA0_MODE_REG);

	/*  enable interrupts on plx 9080 */
	priv(dev)->plx_intcsr_bits |=
	    ICS_AERR | ICS_PERR | ICS_PIE | ICS_PLIE | ICS_PAIE | ICS_LIE |
	    ICS_DMA0_E | ICS_DMA1_E;
	writel(priv(dev)->plx_intcsr_bits,
	       priv(dev)->plx9080_iobase + PLX_INTRCS_REG);
}

/* Allocate and initialize the subdevice structures.
 */
static int setup_subdevices(struct comedi_device *dev)
{
	struct comedi_subdevice *s;
	void *dio_8255_iobase;
	int i;

	if (alloc_subdevices(dev, 10) < 0)
		return -ENOMEM;

	s = dev->subdevices + 0;
	/* analog input subdevice */
	dev->read_subdev = s;
	s->type = COMEDI_SUBD_AI;
	s->subdev_flags = SDF_READABLE | SDF_GROUND | SDF_DITHER | SDF_CMD_READ;
	if (board(dev)->layout == LAYOUT_60XX)
		s->subdev_flags |= SDF_COMMON | SDF_DIFF;
	else if (board(dev)->layout == LAYOUT_64XX)
		s->subdev_flags |= SDF_DIFF;
	/* XXX Number of inputs in differential mode is ignored */
	s->n_chan = board(dev)->ai_se_chans;
	s->len_chanlist = 0x2000;
	s->maxdata = (1 << board(dev)->ai_bits) - 1;
	s->range_table = board(dev)->ai_range_table;
	s->insn_read = ai_rinsn;
	s->insn_config = ai_config_insn;
	s->do_cmd = ai_cmd;
	s->do_cmdtest = ai_cmdtest;
	s->cancel = ai_cancel;
	if (board(dev)->layout == LAYOUT_4020) {
		unsigned int i;
		uint8_t data;
		/*  set adc to read from inputs (not internal calibration sources) */
		priv(dev)->i2c_cal_range_bits = adc_src_4020_bits(4);
		/*  set channels to +-5 volt input ranges */
		for (i = 0; i < s->n_chan; i++)
			priv(dev)->i2c_cal_range_bits |= attenuate_bit(i);
		data = priv(dev)->i2c_cal_range_bits;
		i2c_write(dev, RANGE_CAL_I2C_ADDR, &data, sizeof(data));
	}

	/* analog output subdevice */
	s = dev->subdevices + 1;
	if (board(dev)->ao_nchan) {
		s->type = COMEDI_SUBD_AO;
		s->subdev_flags =
		    SDF_READABLE | SDF_WRITABLE | SDF_GROUND | SDF_CMD_WRITE;
		s->n_chan = board(dev)->ao_nchan;
		s->maxdata = (1 << board(dev)->ao_bits) - 1;
		s->range_table = board(dev)->ao_range_table;
		s->insn_read = ao_readback_insn;
		s->insn_write = ao_winsn;
		if (ao_cmd_is_supported(board(dev))) {
			dev->write_subdev = s;
			s->do_cmdtest = ao_cmdtest;
			s->do_cmd = ao_cmd;
			s->len_chanlist = board(dev)->ao_nchan;
			s->cancel = ao_cancel;
		}
	} else {
		s->type = COMEDI_SUBD_UNUSED;
	}

	/*  digital input */
	s = dev->subdevices + 2;
	if (board(dev)->layout == LAYOUT_64XX) {
		s->type = COMEDI_SUBD_DI;
		s->subdev_flags = SDF_READABLE;
		s->n_chan = 4;
		s->maxdata = 1;
		s->range_table = &range_digital;
		s->insn_bits = di_rbits;
	} else
		s->type = COMEDI_SUBD_UNUSED;

	/*  digital output */
	if (board(dev)->layout == LAYOUT_64XX) {
		s = dev->subdevices + 3;
		s->type = COMEDI_SUBD_DO;
		s->subdev_flags = SDF_WRITABLE | SDF_READABLE;
		s->n_chan = 4;
		s->maxdata = 1;
		s->range_table = &range_digital;
		s->insn_bits = do_wbits;
	} else
		s->type = COMEDI_SUBD_UNUSED;

	/* 8255 */
	s = dev->subdevices + 4;
	if (board(dev)->has_8255) {
		if (board(dev)->layout == LAYOUT_4020) {
			dio_8255_iobase =
			    priv(dev)->main_iobase + I8255_4020_REG;
			subdev_8255_init(dev, s, dio_callback_4020,
					 (unsigned long)dio_8255_iobase);
		} else {
			dio_8255_iobase =
			    priv(dev)->dio_counter_iobase + DIO_8255_OFFSET;
			subdev_8255_init(dev, s, dio_callback,
					 (unsigned long)dio_8255_iobase);
		}
	} else
		s->type = COMEDI_SUBD_UNUSED;

	/*  8 channel dio for 60xx */
	s = dev->subdevices + 5;
	if (board(dev)->layout == LAYOUT_60XX) {
		s->type = COMEDI_SUBD_DIO;
		s->subdev_flags = SDF_WRITABLE | SDF_READABLE;
		s->n_chan = 8;
		s->maxdata = 1;
		s->range_table = &range_digital;
		s->insn_config = dio_60xx_config_insn;
		s->insn_bits = dio_60xx_wbits;
	} else
		s->type = COMEDI_SUBD_UNUSED;

	/*  caldac */
	s = dev->subdevices + 6;
	s->type = COMEDI_SUBD_CALIB;
	s->subdev_flags = SDF_READABLE | SDF_WRITABLE | SDF_INTERNAL;
	s->n_chan = 8;
	if (board(dev)->layout == LAYOUT_4020)
		s->maxdata = 0xfff;
	else
		s->maxdata = 0xff;
	s->insn_read = calib_read_insn;
	s->insn_write = calib_write_insn;
	for (i = 0; i < s->n_chan; i++)
		caldac_write(dev, i, s->maxdata / 2);

	/*  2 channel ad8402 potentiometer */
	s = dev->subdevices + 7;
	if (board(dev)->layout == LAYOUT_64XX) {
		s->type = COMEDI_SUBD_CALIB;
		s->subdev_flags = SDF_READABLE | SDF_WRITABLE | SDF_INTERNAL;
		s->n_chan = 2;
		s->insn_read = ad8402_read_insn;
		s->insn_write = ad8402_write_insn;
		s->maxdata = 0xff;
		for (i = 0; i < s->n_chan; i++)
			ad8402_write(dev, i, s->maxdata / 2);
	} else
		s->type = COMEDI_SUBD_UNUSED;

	/* serial EEPROM, if present */
	s = dev->subdevices + 8;
	if (readl(priv(dev)->plx9080_iobase + PLX_CONTROL_REG) & CTL_EECHK) {
		s->type = COMEDI_SUBD_MEMORY;
		s->subdev_flags = SDF_READABLE | SDF_INTERNAL;
		s->n_chan = 128;
		s->maxdata = 0xffff;
		s->insn_read = eeprom_read_insn;
	} else
		s->type = COMEDI_SUBD_UNUSED;

	/*  user counter subd XXX */
	s = dev->subdevices + 9;
	s->type = COMEDI_SUBD_UNUSED;

	return 0;
}

static void disable_plx_interrupts(struct comedi_device *dev)
{
	priv(dev)->plx_intcsr_bits = 0;
	writel(priv(dev)->plx_intcsr_bits,
	       priv(dev)->plx9080_iobase + PLX_INTRCS_REG);
}

static void init_stc_registers(struct comedi_device *dev)
{
	uint16_t bits;
	unsigned long flags;

	spin_lock_irqsave(&dev->spinlock, flags);

	/*  bit should be set for 6025, although docs say boards with <= 16 chans should be cleared XXX */
	if (1)
		priv(dev)->adc_control1_bits |= ADC_QUEUE_CONFIG_BIT;
	writew(priv(dev)->adc_control1_bits,
	       priv(dev)->main_iobase + ADC_CONTROL1_REG);

	/*  6402/16 manual says this register must be initialized to 0xff? */
	writew(0xff, priv(dev)->main_iobase + ADC_SAMPLE_INTERVAL_UPPER_REG);

	bits = SLOW_DAC_BIT | DMA_CH_SELECT_BIT;
	if (board(dev)->layout == LAYOUT_4020)
		bits |= INTERNAL_CLOCK_4020_BITS;
	priv(dev)->hw_config_bits |= bits;
	writew(priv(dev)->hw_config_bits,
	       priv(dev)->main_iobase + HW_CONFIG_REG);

	writew(0, priv(dev)->main_iobase + DAQ_SYNC_REG);
	writew(0, priv(dev)->main_iobase + CALIBRATION_REG);

	spin_unlock_irqrestore(&dev->spinlock, flags);

	/*  set fifos to maximum size */
	priv(dev)->fifo_size_bits |= DAC_FIFO_BITS;
	set_ai_fifo_segment_length(dev,
				   board(dev)->ai_fifo->max_segment_length);

	priv(dev)->dac_control1_bits = DAC_OUTPUT_ENABLE_BIT;
	priv(dev)->intr_enable_bits =	/* EN_DAC_INTR_SRC_BIT | DAC_INTR_QEMPTY_BITS | */
	    EN_DAC_DONE_INTR_BIT | EN_DAC_UNDERRUN_BIT;
	writew(priv(dev)->intr_enable_bits,
	       priv(dev)->main_iobase + INTR_ENABLE_REG);

	disable_ai_pacing(dev);
};

int alloc_and_init_dma_members(struct comedi_device *dev)
{
	int i;

	/*  alocate pci dma buffers */
	for (i = 0; i < ai_dma_ring_count(board(dev)); i++) {
		priv(dev)->ai_buffer[i] =
		    pci_alloc_consistent(priv(dev)->hw_dev, DMA_BUFFER_SIZE,
					 &priv(dev)->ai_buffer_bus_addr[i]);
		if (priv(dev)->ai_buffer[i] == NULL) {
			return -ENOMEM;
		}
	}
	for (i = 0; i < AO_DMA_RING_COUNT; i++) {
		if (ao_cmd_is_supported(board(dev))) {
			priv(dev)->ao_buffer[i] =
			    pci_alloc_consistent(priv(dev)->hw_dev,
						 DMA_BUFFER_SIZE,
						 &priv(dev)->
						 ao_buffer_bus_addr[i]);
			if (priv(dev)->ao_buffer[i] == NULL) {
				return -ENOMEM;
			}
		}
	}
	/*  allocate dma descriptors */
	priv(dev)->ai_dma_desc =
	    pci_alloc_consistent(priv(dev)->hw_dev,
				 sizeof(struct plx_dma_desc) *
				 ai_dma_ring_count(board(dev)),
				 &priv(dev)->ai_dma_desc_bus_addr);
	if (priv(dev)->ai_dma_desc == NULL) {
		return -ENOMEM;
	}
	DEBUG_PRINT("ai dma descriptors start at bus addr 0x%x\n",
		    priv(dev)->ai_dma_desc_bus_addr);
	if (ao_cmd_is_supported(board(dev))) {
		priv(dev)->ao_dma_desc =
		    pci_alloc_consistent(priv(dev)->hw_dev,
					 sizeof(struct plx_dma_desc) *
					 AO_DMA_RING_COUNT,
					 &priv(dev)->ao_dma_desc_bus_addr);
		if (priv(dev)->ao_dma_desc == NULL) {
			return -ENOMEM;
		}
		DEBUG_PRINT("ao dma descriptors start at bus addr 0x%x\n",
			    priv(dev)->ao_dma_desc_bus_addr);
	}
	/*  initialize dma descriptors */
	for (i = 0; i < ai_dma_ring_count(board(dev)); i++) {
		priv(dev)->ai_dma_desc[i].pci_start_addr =
		    cpu_to_le32(priv(dev)->ai_buffer_bus_addr[i]);
		if (board(dev)->layout == LAYOUT_4020)
			priv(dev)->ai_dma_desc[i].local_start_addr =
			    cpu_to_le32(priv(dev)->local1_iobase +
					ADC_FIFO_REG);
		else
			priv(dev)->ai_dma_desc[i].local_start_addr =
			    cpu_to_le32(priv(dev)->local0_iobase +
					ADC_FIFO_REG);
		priv(dev)->ai_dma_desc[i].transfer_size = cpu_to_le32(0);
		priv(dev)->ai_dma_desc[i].next =
		    cpu_to_le32((priv(dev)->ai_dma_desc_bus_addr + ((i +
								     1) %
								    ai_dma_ring_count
								    (board
								     (dev))) *
				 sizeof(priv(dev)->ai_dma_desc[0])) |
				PLX_DESC_IN_PCI_BIT | PLX_INTR_TERM_COUNT |
				PLX_XFER_LOCAL_TO_PCI);
	}
	if (ao_cmd_is_supported(board(dev))) {
		for (i = 0; i < AO_DMA_RING_COUNT; i++) {
			priv(dev)->ao_dma_desc[i].pci_start_addr =
			    cpu_to_le32(priv(dev)->ao_buffer_bus_addr[i]);
			priv(dev)->ao_dma_desc[i].local_start_addr =
			    cpu_to_le32(priv(dev)->local0_iobase +
					DAC_FIFO_REG);
			priv(dev)->ao_dma_desc[i].transfer_size =
			    cpu_to_le32(0);
			priv(dev)->ao_dma_desc[i].next =
			    cpu_to_le32((priv(dev)->ao_dma_desc_bus_addr +
					 ((i + 1) % (AO_DMA_RING_COUNT)) *
					 sizeof(priv(dev)->ao_dma_desc[0])) |
					PLX_DESC_IN_PCI_BIT |
					PLX_INTR_TERM_COUNT);
		}
	}
	return 0;
}

static inline void warn_external_queue(struct comedi_device *dev)
{
	comedi_error(dev,
		     "AO command and AI external channel queue cannot be used simultaneously.");
	comedi_error(dev,
		     "Use internal AI channel queue (channels must be consecutive and use same range/aref)");
}

/*
 * Attach is called by the Comedi core to configure the driver
 * for a particular board.
 */
static int attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	struct pci_dev *pcidev;
	int index;
	uint32_t local_range, local_decode;
	int retval;

	printk("comedi%d: cb_pcidas64\n", dev->minor);

/*
 * Allocate the private structure area.
 */
	if (alloc_private(dev, sizeof(struct pcidas64_private)) < 0)
		return -ENOMEM;

/*
 * Probe the device to determine what device in the series it is.
 */

	for (pcidev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, NULL);
	     pcidev != NULL;
	     pcidev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, pcidev)) {
		/*  is it not a computer boards card? */
		if (pcidev->vendor != PCI_VENDOR_ID_COMPUTERBOARDS)
			continue;
		/*  loop through cards supported by this driver */
		for (index = 0; index < ARRAY_SIZE(pcidas64_boards); index++) {
			if (pcidas64_boards[index].device_id != pcidev->device)
				continue;
			/*  was a particular bus/slot requested? */
			if (it->options[0] || it->options[1]) {
				/*  are we on the wrong bus/slot? */
				if (pcidev->bus->number != it->options[0] ||
				    PCI_SLOT(pcidev->devfn) != it->options[1]) {
					continue;
				}
			}
			priv(dev)->hw_dev = pcidev;
			dev->board_ptr = pcidas64_boards + index;
			break;
		}
		if (dev->board_ptr)
			break;
	}

	if (dev->board_ptr == NULL) {
		printk
		    ("No supported ComputerBoards/MeasurementComputing card found\n");
		return -EIO;
	}

	printk("Found %s on bus %i, slot %i\n", board(dev)->name,
	       pcidev->bus->number, PCI_SLOT(pcidev->devfn));

	if (comedi_pci_enable(pcidev, driver_cb_pcidas.driver_name)) {
		printk(KERN_WARNING
		       " failed to enable PCI device and request regions\n");
		return -EIO;
	}
	pci_set_master(pcidev);

	/* Initialize dev->board_name */
	dev->board_name = board(dev)->name;

	priv(dev)->plx9080_phys_iobase =
	    pci_resource_start(pcidev, PLX9080_BADDRINDEX);
	priv(dev)->main_phys_iobase =
	    pci_resource_start(pcidev, MAIN_BADDRINDEX);
	priv(dev)->dio_counter_phys_iobase =
	    pci_resource_start(pcidev, DIO_COUNTER_BADDRINDEX);

	/*  remap, won't work with 2.0 kernels but who cares */
	priv(dev)->plx9080_iobase = ioremap(priv(dev)->plx9080_phys_iobase,
					    pci_resource_len(pcidev,
							     PLX9080_BADDRINDEX));
	priv(dev)->main_iobase =
	    ioremap(priv(dev)->main_phys_iobase,
		    pci_resource_len(pcidev, MAIN_BADDRINDEX));
	priv(dev)->dio_counter_iobase =
	    ioremap(priv(dev)->dio_counter_phys_iobase,
		    pci_resource_len(pcidev, DIO_COUNTER_BADDRINDEX));

	if (!priv(dev)->plx9080_iobase || !priv(dev)->main_iobase
	    || !priv(dev)->dio_counter_iobase) {
		printk(" failed to remap io memory\n");
		return -ENOMEM;
	}

	DEBUG_PRINT(" plx9080 remapped to 0x%p\n", priv(dev)->plx9080_iobase);
	DEBUG_PRINT(" main remapped to 0x%p\n", priv(dev)->main_iobase);
	DEBUG_PRINT(" diocounter remapped to 0x%p\n",
		    priv(dev)->dio_counter_iobase);

	/*  figure out what local addresses are */
	local_range =
	    readl(priv(dev)->plx9080_iobase + PLX_LAS0RNG_REG) & LRNG_MEM_MASK;
	local_decode =
	    readl(priv(dev)->plx9080_iobase +
		  PLX_LAS0MAP_REG) & local_range & LMAP_MEM_MASK;
	priv(dev)->local0_iobase =
	    ((uint32_t) priv(dev)->main_phys_iobase & ~local_range) |
	    local_decode;
	local_range =
	    readl(priv(dev)->plx9080_iobase + PLX_LAS1RNG_REG) & LRNG_MEM_MASK;
	local_decode =
	    readl(priv(dev)->plx9080_iobase +
		  PLX_LAS1MAP_REG) & local_range & LMAP_MEM_MASK;
	priv(dev)->local1_iobase =
	    ((uint32_t) priv(dev)->dio_counter_phys_iobase & ~local_range) |
	    local_decode;

	DEBUG_PRINT(" local 0 io addr 0x%x\n", priv(dev)->local0_iobase);
	DEBUG_PRINT(" local 1 io addr 0x%x\n", priv(dev)->local1_iobase);

	retval = alloc_and_init_dma_members(dev);
	if (retval < 0)
		return retval;

	priv(dev)->hw_revision =
	    hw_revision(dev, readw(priv(dev)->main_iobase + HW_STATUS_REG));
	printk(" stc hardware revision %i\n", priv(dev)->hw_revision);
	init_plx9080(dev);
	init_stc_registers(dev);
	/*  get irq */
	if (request_irq(pcidev->irq, handle_interrupt, IRQF_SHARED,
			"cb_pcidas64", dev)) {
		printk(" unable to allocate irq %u\n", pcidev->irq);
		return -EINVAL;
	}
	dev->irq = pcidev->irq;
	printk(" irq %u\n", dev->irq);

	retval = setup_subdevices(dev);
	if (retval < 0) {
		return retval;
	}

	return 0;
}

/*
 * _detach is called to deconfigure a device.  It should deallocate
 * resources.
 * This function is also called when _attach() fails, so it should be
 * careful not to release resources that were not necessarily
 * allocated by _attach().  dev->private and dev->subdevices are
 * deallocated automatically by the core.
 */
static int detach(struct comedi_device *dev)
{
	unsigned int i;

	printk("comedi%d: cb_pcidas: remove\n", dev->minor);

	if (dev->irq)
		free_irq(dev->irq, dev);
	if (priv(dev)) {
		if (priv(dev)->hw_dev) {
			if (priv(dev)->plx9080_iobase) {
				disable_plx_interrupts(dev);
				iounmap((void *)priv(dev)->plx9080_iobase);
			}
			if (priv(dev)->main_iobase)
				iounmap((void *)priv(dev)->main_iobase);
			if (priv(dev)->dio_counter_iobase)
				iounmap((void *)priv(dev)->dio_counter_iobase);
			/*  free pci dma buffers */
			for (i = 0; i < ai_dma_ring_count(board(dev)); i++) {
				if (priv(dev)->ai_buffer[i])
					pci_free_consistent(priv(dev)->hw_dev,
							    DMA_BUFFER_SIZE,
							    priv(dev)->
							    ai_buffer[i],
							    priv
							    (dev)->ai_buffer_bus_addr
							    [i]);
			}
			for (i = 0; i < AO_DMA_RING_COUNT; i++) {
				if (priv(dev)->ao_buffer[i])
					pci_free_consistent(priv(dev)->hw_dev,
							    DMA_BUFFER_SIZE,
							    priv(dev)->
							    ao_buffer[i],
							    priv
							    (dev)->ao_buffer_bus_addr
							    [i]);
			}
			/*  free dma descriptors */
			if (priv(dev)->ai_dma_desc)
				pci_free_consistent(priv(dev)->hw_dev,
						    sizeof(struct plx_dma_desc)
						    *
						    ai_dma_ring_count(board
								      (dev)),
						    priv(dev)->ai_dma_desc,
						    priv(dev)->
						    ai_dma_desc_bus_addr);
			if (priv(dev)->ao_dma_desc)
				pci_free_consistent(priv(dev)->hw_dev,
						    sizeof(struct plx_dma_desc)
						    * AO_DMA_RING_COUNT,
						    priv(dev)->ao_dma_desc,
						    priv(dev)->
						    ao_dma_desc_bus_addr);
			if (priv(dev)->main_phys_iobase) {
				comedi_pci_disable(priv(dev)->hw_dev);
			}
			pci_dev_put(priv(dev)->hw_dev);
		}
	}
	if (dev->subdevices)
		subdev_8255_cleanup(dev, dev->subdevices + 4);

	return 0;
}

static int ai_rinsn(struct comedi_device *dev, struct comedi_subdevice *s,
		    struct comedi_insn *insn, unsigned int *data)
{
	unsigned int bits = 0, n, i;
	unsigned int channel, range, aref;
	unsigned long flags;
	static const int timeout = 100;

	DEBUG_PRINT("chanspec 0x%x\n", insn->chanspec);
	channel = CR_CHAN(insn->chanspec);
	range = CR_RANGE(insn->chanspec);
	aref = CR_AREF(insn->chanspec);

	/*  disable card's analog input interrupt sources and pacing */
	/*  4020 generates dac done interrupts even though they are disabled */
	disable_ai_pacing(dev);

	spin_lock_irqsave(&dev->spinlock, flags);
	if (insn->chanspec & CR_ALT_FILTER)
		priv(dev)->adc_control1_bits |= ADC_DITHER_BIT;
	else
		priv(dev)->adc_control1_bits &= ~ADC_DITHER_BIT;
	writew(priv(dev)->adc_control1_bits,
	       priv(dev)->main_iobase + ADC_CONTROL1_REG);
	spin_unlock_irqrestore(&dev->spinlock, flags);

	if (board(dev)->layout != LAYOUT_4020) {
		/*  use internal queue */
		priv(dev)->hw_config_bits &= ~EXT_QUEUE_BIT;
		writew(priv(dev)->hw_config_bits,
		       priv(dev)->main_iobase + HW_CONFIG_REG);

		/*  ALT_SOURCE is internal calibration reference */
		if (insn->chanspec & CR_ALT_SOURCE) {
			unsigned int cal_en_bit;

			DEBUG_PRINT("reading calibration source\n");
			if (board(dev)->layout == LAYOUT_60XX)
				cal_en_bit = CAL_EN_60XX_BIT;
			else
				cal_en_bit = CAL_EN_64XX_BIT;
			/*  select internal reference source to connect to channel 0 */
			writew(cal_en_bit |
			       adc_src_bits(priv(dev)->calibration_source),
			       priv(dev)->main_iobase + CALIBRATION_REG);
		} else {
			/*  make sure internal calibration source is turned off */
			writew(0, priv(dev)->main_iobase + CALIBRATION_REG);
		}
		/*  load internal queue */
		bits = 0;
		/*  set gain */
		bits |= ai_range_bits_6xxx(dev, CR_RANGE(insn->chanspec));
		/*  set single-ended / differential */
		bits |= se_diff_bit_6xxx(dev, aref == AREF_DIFF);
		if (aref == AREF_COMMON)
			bits |= ADC_COMMON_BIT;
		bits |= adc_chan_bits(channel);
		/*  set stop channel */
		writew(adc_chan_bits(channel),
		       priv(dev)->main_iobase + ADC_QUEUE_HIGH_REG);
		/*  set start channel, and rest of settings */
		writew(bits, priv(dev)->main_iobase + ADC_QUEUE_LOAD_REG);
	} else {
		uint8_t old_cal_range_bits = priv(dev)->i2c_cal_range_bits;

		priv(dev)->i2c_cal_range_bits &= ~ADC_SRC_4020_MASK;
		if (insn->chanspec & CR_ALT_SOURCE) {
			DEBUG_PRINT("reading calibration source\n");
			priv(dev)->i2c_cal_range_bits |=
			    adc_src_4020_bits(priv(dev)->calibration_source);
		} else {	/* select BNC inputs */
			priv(dev)->i2c_cal_range_bits |= adc_src_4020_bits(4);
		}
		/*  select range */
		if (range == 0)
			priv(dev)->i2c_cal_range_bits |= attenuate_bit(channel);
		else
			priv(dev)->i2c_cal_range_bits &=
			    ~attenuate_bit(channel);
		/*  update calibration/range i2c register only if necessary, as it is very slow */
		if (old_cal_range_bits != priv(dev)->i2c_cal_range_bits) {
			uint8_t i2c_data = priv(dev)->i2c_cal_range_bits;
			i2c_write(dev, RANGE_CAL_I2C_ADDR, &i2c_data,
				  sizeof(i2c_data));
		}

		/* 4020 manual asks that sample interval register to be set before writing to convert register.
		 * Using somewhat arbitrary setting of 4 master clock ticks = 0.1 usec */
		writew(0,
		       priv(dev)->main_iobase + ADC_SAMPLE_INTERVAL_UPPER_REG);
		writew(2,
		       priv(dev)->main_iobase + ADC_SAMPLE_INTERVAL_LOWER_REG);
	}

	for (n = 0; n < insn->n; n++) {

		/*  clear adc buffer (inside loop for 4020 sake) */
		writew(0, priv(dev)->main_iobase + ADC_BUFFER_CLEAR_REG);

		/* trigger conversion, bits sent only matter for 4020 */
		writew(adc_convert_chan_4020_bits(CR_CHAN(insn->chanspec)),
		       priv(dev)->main_iobase + ADC_CONVERT_REG);

		/*  wait for data */
		for (i = 0; i < timeout; i++) {
			bits = readw(priv(dev)->main_iobase + HW_STATUS_REG);
			DEBUG_PRINT(" pipe bits 0x%x\n", pipe_full_bits(bits));
			if (board(dev)->layout == LAYOUT_4020) {
				if (readw(priv(dev)->main_iobase +
					  ADC_WRITE_PNTR_REG))
					break;
			} else {
				if (pipe_full_bits(bits))
					break;
			}
			udelay(1);
		}
		DEBUG_PRINT(" looped %i times waiting for data\n", i);
		if (i == timeout) {
			comedi_error(dev, " analog input read insn timed out");
			printk(" status 0x%x\n", bits);
			return -ETIME;
		}
		if (board(dev)->layout == LAYOUT_4020)
			data[n] =
			    readl(priv(dev)->dio_counter_iobase +
				  ADC_FIFO_REG) & 0xffff;
		else
			data[n] =
			    readw(priv(dev)->main_iobase + PIPE1_READ_REG);
	}

	return n;
}

static int ai_config_calibration_source(struct comedi_device *dev,
					unsigned int *data)
{
	unsigned int source = data[1];
	int num_calibration_sources;

	if (board(dev)->layout == LAYOUT_60XX)
		num_calibration_sources = 16;
	else
		num_calibration_sources = 8;
	if (source >= num_calibration_sources) {
		printk("invalid calibration source: %i\n", source);
		return -EINVAL;
	}

	DEBUG_PRINT("setting calibration source to %i\n", source);
	priv(dev)->calibration_source = source;

	return 2;
}

static int ai_config_block_size(struct comedi_device *dev, unsigned int *data)
{
	int fifo_size;
	const struct hw_fifo_info *const fifo = board(dev)->ai_fifo;
	unsigned int block_size, requested_block_size;
	int retval;

	requested_block_size = data[1];

	if (requested_block_size) {
		fifo_size =
		    requested_block_size * fifo->num_segments / bytes_in_sample;

		retval = set_ai_fifo_size(dev, fifo_size);
		if (retval < 0)
			return retval;

	}

	block_size = ai_fifo_size(dev) / fifo->num_segments * bytes_in_sample;

	data[1] = block_size;

	return 2;
}

static int ai_config_master_clock_4020(struct comedi_device *dev,
				       unsigned int *data)
{
	unsigned int divisor = data[4];
	int retval = 0;

	if (divisor < 2) {
		divisor = 2;
		retval = -EAGAIN;
	}

	switch (data[1]) {
	case COMEDI_EV_SCAN_BEGIN:
		priv(dev)->ext_clock.divisor = divisor;
		priv(dev)->ext_clock.chanspec = data[2];
		break;
	default:
		return -EINVAL;
		break;
	}

	data[4] = divisor;

	return retval ? retval : 5;
}

/* XXX could add support for 60xx series */
static int ai_config_master_clock(struct comedi_device *dev, unsigned int *data)
{

	switch (board(dev)->layout) {
	case LAYOUT_4020:
		return ai_config_master_clock_4020(dev, data);
		break;
	default:
		return -EINVAL;
		break;
	}

	return -EINVAL;
}

static int ai_config_insn(struct comedi_device *dev, struct comedi_subdevice *s,
			  struct comedi_insn *insn, unsigned int *data)
{
	int id = data[0];

	switch (id) {
	case INSN_CONFIG_ALT_SOURCE:
		return ai_config_calibration_source(dev, data);
		break;
	case INSN_CONFIG_BLOCK_SIZE:
		return ai_config_block_size(dev, data);
		break;
	case INSN_CONFIG_TIMER_1:
		return ai_config_master_clock(dev, data);
		break;
	default:
		return -EINVAL;
		break;
	}
	return -EINVAL;
}

static int ai_cmdtest(struct comedi_device *dev, struct comedi_subdevice *s,
		      struct comedi_cmd *cmd)
{
	int err = 0;
	int tmp;
	unsigned int tmp_arg, tmp_arg2;
	int i;
	int aref;
	unsigned int triggers;

	/* step 1: make sure trigger sources are trivially valid */

	tmp = cmd->start_src;
	cmd->start_src &= TRIG_NOW | TRIG_EXT;
	if (!cmd->start_src || tmp != cmd->start_src)
		err++;

	tmp = cmd->scan_begin_src;
	triggers = TRIG_TIMER;
	if (board(dev)->layout == LAYOUT_4020)
		triggers |= TRIG_OTHER;
	else
		triggers |= TRIG_FOLLOW;
	cmd->scan_begin_src &= triggers;
	if (!cmd->scan_begin_src || tmp != cmd->scan_begin_src)
		err++;

	tmp = cmd->convert_src;
	triggers = TRIG_TIMER;
	if (board(dev)->layout == LAYOUT_4020)
		triggers |= TRIG_NOW;
	else
		triggers |= TRIG_EXT;
	cmd->convert_src &= triggers;
	if (!cmd->convert_src || tmp != cmd->convert_src)
		err++;

	tmp = cmd->scan_end_src;
	cmd->scan_end_src &= TRIG_COUNT;
	if (!cmd->scan_end_src || tmp != cmd->scan_end_src)
		err++;

	tmp = cmd->stop_src;
	cmd->stop_src &= TRIG_COUNT | TRIG_EXT | TRIG_NONE;
	if (!cmd->stop_src || tmp != cmd->stop_src)
		err++;

	if (err)
		return 1;

	/* step 2: make sure trigger sources are unique and mutually compatible */

	/*  uniqueness check */
	if (cmd->start_src != TRIG_NOW && cmd->start_src != TRIG_EXT)
		err++;
	if (cmd->scan_begin_src != TRIG_TIMER &&
	    cmd->scan_begin_src != TRIG_OTHER &&
	    cmd->scan_begin_src != TRIG_FOLLOW)
		err++;
	if (cmd->convert_src != TRIG_TIMER &&
	    cmd->convert_src != TRIG_EXT && cmd->convert_src != TRIG_NOW)
		err++;
	if (cmd->stop_src != TRIG_COUNT &&
	    cmd->stop_src != TRIG_NONE && cmd->stop_src != TRIG_EXT)
		err++;

	/*  compatibility check */
	if (cmd->convert_src == TRIG_EXT && cmd->scan_begin_src == TRIG_TIMER)
		err++;
	if (cmd->stop_src != TRIG_COUNT &&
	    cmd->stop_src != TRIG_NONE && cmd->stop_src != TRIG_EXT)
		err++;

	if (err)
		return 2;

	/* step 3: make sure arguments are trivially compatible */

	if (cmd->convert_src == TRIG_TIMER) {
		if (board(dev)->layout == LAYOUT_4020) {
			if (cmd->convert_arg) {
				cmd->convert_arg = 0;
				err++;
			}
		} else {
			if (cmd->convert_arg < board(dev)->ai_speed) {
				cmd->convert_arg = board(dev)->ai_speed;
				err++;
			}
			if (cmd->scan_begin_src == TRIG_TIMER) {
				/*  if scans are timed faster than conversion rate allows */
				if (cmd->convert_arg * cmd->chanlist_len >
				    cmd->scan_begin_arg) {
					cmd->scan_begin_arg =
					    cmd->convert_arg *
					    cmd->chanlist_len;
					err++;
				}
			}
		}
	}

	if (!cmd->chanlist_len) {
		cmd->chanlist_len = 1;
		err++;
	}
	if (cmd->scan_end_arg != cmd->chanlist_len) {
		cmd->scan_end_arg = cmd->chanlist_len;
		err++;
	}

	switch (cmd->stop_src) {
	case TRIG_EXT:
		break;
	case TRIG_COUNT:
		if (!cmd->stop_arg) {
			cmd->stop_arg = 1;
			err++;
		}
		break;
	case TRIG_NONE:
		if (cmd->stop_arg != 0) {
			cmd->stop_arg = 0;
			err++;
		}
		break;
	default:
		break;
	}

	if (err)
		return 3;

	/* step 4: fix up any arguments */

	if (cmd->convert_src == TRIG_TIMER) {
		tmp_arg = cmd->convert_arg;
		tmp_arg2 = cmd->scan_begin_arg;
		check_adc_timing(dev, cmd);
		if (tmp_arg != cmd->convert_arg)
			err++;
		if (tmp_arg2 != cmd->scan_begin_arg)
			err++;
	}

	if (err)
		return 4;

	/*  make sure user is doesn't change analog reference mid chanlist */
	if (cmd->chanlist) {
		aref = CR_AREF(cmd->chanlist[0]);
		for (i = 1; i < cmd->chanlist_len; i++) {
			if (aref != CR_AREF(cmd->chanlist[i])) {
				comedi_error(dev,
					     "all elements in chanlist must use the same analog reference");
				err++;
				break;
			}
		}
		/*  check 4020 chanlist */
		if (board(dev)->layout == LAYOUT_4020) {
			unsigned int first_channel = CR_CHAN(cmd->chanlist[0]);
			for (i = 1; i < cmd->chanlist_len; i++) {
				if (CR_CHAN(cmd->chanlist[i]) !=
				    first_channel + i) {
					comedi_error(dev,
						     "chanlist must use consecutive channels");
					err++;
					break;
				}
			}
			if (cmd->chanlist_len == 3) {
				comedi_error(dev,
					     "chanlist cannot be 3 channels long, use 1, 2, or 4 channels");
				err++;
			}
		}
	}

	if (err)
		return 5;

	return 0;
}

static int use_hw_sample_counter(struct comedi_cmd *cmd)
{
/* disable for now until I work out a race */
	return 0;

	if (cmd->stop_src == TRIG_COUNT && cmd->stop_arg <= max_counter_value)
		return 1;
	else
		return 0;
}

static void setup_sample_counters(struct comedi_device *dev,
				  struct comedi_cmd *cmd)
{
	if (cmd->stop_src == TRIG_COUNT) {
		/*  set software count */
		priv(dev)->ai_count = cmd->stop_arg * cmd->chanlist_len;
	}
	/*  load hardware conversion counter */
	if (use_hw_sample_counter(cmd)) {
		writew(cmd->stop_arg & 0xffff,
		       priv(dev)->main_iobase + ADC_COUNT_LOWER_REG);
		writew((cmd->stop_arg >> 16) & 0xff,
		       priv(dev)->main_iobase + ADC_COUNT_UPPER_REG);
	} else {
		writew(1, priv(dev)->main_iobase + ADC_COUNT_LOWER_REG);
	}
}

static inline unsigned int dma_transfer_size(struct comedi_device *dev)
{
	unsigned int num_samples;

	num_samples =
	    priv(dev)->ai_fifo_segment_length *
	    board(dev)->ai_fifo->sample_packing_ratio;
	if (num_samples > DMA_BUFFER_SIZE / sizeof(uint16_t))
		num_samples = DMA_BUFFER_SIZE / sizeof(uint16_t);

	return num_samples;
}

static void disable_ai_pacing(struct comedi_device *dev)
{
	unsigned long flags;

	disable_ai_interrupts(dev);

	spin_lock_irqsave(&dev->spinlock, flags);
	priv(dev)->adc_control1_bits &= ~ADC_SW_GATE_BIT;
	writew(priv(dev)->adc_control1_bits,
	       priv(dev)->main_iobase + ADC_CONTROL1_REG);
	spin_unlock_irqrestore(&dev->spinlock, flags);

	/* disable pacing, triggering, etc */
	writew(ADC_DMA_DISABLE_BIT | ADC_SOFT_GATE_BITS | ADC_GATE_LEVEL_BIT,
	       priv(dev)->main_iobase + ADC_CONTROL0_REG);
}

static void disable_ai_interrupts(struct comedi_device *dev)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->spinlock, flags);
	priv(dev)->intr_enable_bits &=
	    ~EN_ADC_INTR_SRC_BIT & ~EN_ADC_DONE_INTR_BIT &
	    ~EN_ADC_ACTIVE_INTR_BIT & ~EN_ADC_STOP_INTR_BIT &
	    ~EN_ADC_OVERRUN_BIT & ~ADC_INTR_SRC_MASK;
	writew(priv(dev)->intr_enable_bits,
	       priv(dev)->main_iobase + INTR_ENABLE_REG);
	spin_unlock_irqrestore(&dev->spinlock, flags);

	DEBUG_PRINT("intr enable bits 0x%x\n", priv(dev)->intr_enable_bits);
}

static void enable_ai_interrupts(struct comedi_device *dev,
				 const struct comedi_cmd *cmd)
{
	uint32_t bits;
	unsigned long flags;

	bits = EN_ADC_OVERRUN_BIT | EN_ADC_DONE_INTR_BIT |
	    EN_ADC_ACTIVE_INTR_BIT | EN_ADC_STOP_INTR_BIT;
	/*  Use pio transfer and interrupt on end of conversion if TRIG_WAKE_EOS flag is set. */
	if (cmd->flags & TRIG_WAKE_EOS) {
		/*  4020 doesn't support pio transfers except for fifo dregs */
		if (board(dev)->layout != LAYOUT_4020)
			bits |= ADC_INTR_EOSCAN_BITS | EN_ADC_INTR_SRC_BIT;
	}
	spin_lock_irqsave(&dev->spinlock, flags);
	priv(dev)->intr_enable_bits |= bits;
	writew(priv(dev)->intr_enable_bits,
	       priv(dev)->main_iobase + INTR_ENABLE_REG);
	DEBUG_PRINT("intr enable bits 0x%x\n", priv(dev)->intr_enable_bits);
	spin_unlock_irqrestore(&dev->spinlock, flags);
}

static uint32_t ai_convert_counter_6xxx(const struct comedi_device *dev,
					const struct comedi_cmd *cmd)
{
	/*  supposed to load counter with desired divisor minus 3 */
	return cmd->convert_arg / TIMER_BASE - 3;
}

static uint32_t ai_scan_counter_6xxx(struct comedi_device *dev,
				     struct comedi_cmd *cmd)
{
	uint32_t count;
	/*  figure out how long we need to delay at end of scan */
	switch (cmd->scan_begin_src) {
	case TRIG_TIMER:
		count = (cmd->scan_begin_arg -
			 (cmd->convert_arg * (cmd->chanlist_len - 1)))
		    / TIMER_BASE;
		break;
	case TRIG_FOLLOW:
		count = cmd->convert_arg / TIMER_BASE;
		break;
	default:
		return 0;
		break;
	}
	return count - 3;
}

static uint32_t ai_convert_counter_4020(struct comedi_device *dev,
					struct comedi_cmd *cmd)
{
	unsigned int divisor;

	switch (cmd->scan_begin_src) {
	case TRIG_TIMER:
		divisor = cmd->scan_begin_arg / TIMER_BASE;
		break;
	case TRIG_OTHER:
		divisor = priv(dev)->ext_clock.divisor;
		break;
	default:		/*  should never happen */
		comedi_error(dev, "bug! failed to set ai pacing!");
		divisor = 1000;
		break;
	}

	/*  supposed to load counter with desired divisor minus 2 for 4020 */
	return divisor - 2;
}

static void select_master_clock_4020(struct comedi_device *dev,
				     const struct comedi_cmd *cmd)
{
	/*  select internal/external master clock */
	priv(dev)->hw_config_bits &= ~MASTER_CLOCK_4020_MASK;
	if (cmd->scan_begin_src == TRIG_OTHER) {
		int chanspec = priv(dev)->ext_clock.chanspec;

		if (CR_CHAN(chanspec))
			priv(dev)->hw_config_bits |= BNC_CLOCK_4020_BITS;
		else
			priv(dev)->hw_config_bits |= EXT_CLOCK_4020_BITS;
	} else {
		priv(dev)->hw_config_bits |= INTERNAL_CLOCK_4020_BITS;
	}
	writew(priv(dev)->hw_config_bits,
	       priv(dev)->main_iobase + HW_CONFIG_REG);
}

static void select_master_clock(struct comedi_device *dev,
				const struct comedi_cmd *cmd)
{
	switch (board(dev)->layout) {
	case LAYOUT_4020:
		select_master_clock_4020(dev, cmd);
		break;
	default:
		break;
	}
}

static inline void dma_start_sync(struct comedi_device *dev,
				  unsigned int channel)
{
	unsigned long flags;

	/*  spinlock for plx dma control/status reg */
	spin_lock_irqsave(&dev->spinlock, flags);
	if (channel)
		writeb(PLX_DMA_EN_BIT | PLX_DMA_START_BIT |
		       PLX_CLEAR_DMA_INTR_BIT,
		       priv(dev)->plx9080_iobase + PLX_DMA1_CS_REG);
	else
		writeb(PLX_DMA_EN_BIT | PLX_DMA_START_BIT |
		       PLX_CLEAR_DMA_INTR_BIT,
		       priv(dev)->plx9080_iobase + PLX_DMA0_CS_REG);
	spin_unlock_irqrestore(&dev->spinlock, flags);
}

static void set_ai_pacing(struct comedi_device *dev, struct comedi_cmd *cmd)
{
	uint32_t convert_counter = 0, scan_counter = 0;

	check_adc_timing(dev, cmd);

	select_master_clock(dev, cmd);

	if (board(dev)->layout == LAYOUT_4020) {
		convert_counter = ai_convert_counter_4020(dev, cmd);
	} else {
		convert_counter = ai_convert_counter_6xxx(dev, cmd);
		scan_counter = ai_scan_counter_6xxx(dev, cmd);
	}

	/*  load lower 16 bits of convert interval */
	writew(convert_counter & 0xffff,
	       priv(dev)->main_iobase + ADC_SAMPLE_INTERVAL_LOWER_REG);
	DEBUG_PRINT("convert counter 0x%x\n", convert_counter);
	/*  load upper 8 bits of convert interval */
	writew((convert_counter >> 16) & 0xff,
	       priv(dev)->main_iobase + ADC_SAMPLE_INTERVAL_UPPER_REG);
	/*  load lower 16 bits of scan delay */
	writew(scan_counter & 0xffff,
	       priv(dev)->main_iobase + ADC_DELAY_INTERVAL_LOWER_REG);
	/*  load upper 8 bits of scan delay */
	writew((scan_counter >> 16) & 0xff,
	       priv(dev)->main_iobase + ADC_DELAY_INTERVAL_UPPER_REG);
	DEBUG_PRINT("scan counter 0x%x\n", scan_counter);
}

static int use_internal_queue_6xxx(const struct comedi_cmd *cmd)
{
	int i;
	for (i = 0; i + 1 < cmd->chanlist_len; i++) {
		if (CR_CHAN(cmd->chanlist[i + 1]) !=
		    CR_CHAN(cmd->chanlist[i]) + 1)
			return 0;
		if (CR_RANGE(cmd->chanlist[i + 1]) !=
		    CR_RANGE(cmd->chanlist[i]))
			return 0;
		if (CR_AREF(cmd->chanlist[i + 1]) != CR_AREF(cmd->chanlist[i]))
			return 0;
	}
	return 1;
}

static int setup_channel_queue(struct comedi_device *dev,
			       const struct comedi_cmd *cmd)
{
	unsigned short bits;
	int i;

	if (board(dev)->layout != LAYOUT_4020) {
		if (use_internal_queue_6xxx(cmd)) {
			priv(dev)->hw_config_bits &= ~EXT_QUEUE_BIT;
			writew(priv(dev)->hw_config_bits,
			       priv(dev)->main_iobase + HW_CONFIG_REG);
			bits = 0;
			/*  set channel */
			bits |= adc_chan_bits(CR_CHAN(cmd->chanlist[0]));
			/*  set gain */
			bits |= ai_range_bits_6xxx(dev,
						   CR_RANGE(cmd->chanlist[0]));
			/*  set single-ended / differential */
			bits |= se_diff_bit_6xxx(dev,
						 CR_AREF(cmd->chanlist[0]) ==
						 AREF_DIFF);
			if (CR_AREF(cmd->chanlist[0]) == AREF_COMMON)
				bits |= ADC_COMMON_BIT;
			/*  set stop channel */
			writew(adc_chan_bits
			       (CR_CHAN(cmd->chanlist[cmd->chanlist_len - 1])),
			       priv(dev)->main_iobase + ADC_QUEUE_HIGH_REG);
			/*  set start channel, and rest of settings */
			writew(bits,
			       priv(dev)->main_iobase + ADC_QUEUE_LOAD_REG);
		} else {
			/*  use external queue */
			if (dev->write_subdev && dev->write_subdev->busy) {
				warn_external_queue(dev);
				return -EBUSY;
			}
			priv(dev)->hw_config_bits |= EXT_QUEUE_BIT;
			writew(priv(dev)->hw_config_bits,
			       priv(dev)->main_iobase + HW_CONFIG_REG);
			/*  clear DAC buffer to prevent weird interactions */
			writew(0,
			       priv(dev)->main_iobase + DAC_BUFFER_CLEAR_REG);
			/*  clear queue pointer */
			writew(0, priv(dev)->main_iobase + ADC_QUEUE_CLEAR_REG);
			/*  load external queue */
			for (i = 0; i < cmd->chanlist_len; i++) {
				bits = 0;
				/*  set channel */
				bits |=
				    adc_chan_bits(CR_CHAN(cmd->chanlist[i]));
				/*  set gain */
				bits |= ai_range_bits_6xxx(dev,
							   CR_RANGE(cmd->
								    chanlist
								    [i]));
				/*  set single-ended / differential */
				bits |= se_diff_bit_6xxx(dev,
							 CR_AREF(cmd->
								 chanlist[i]) ==
							 AREF_DIFF);
				if (CR_AREF(cmd->chanlist[i]) == AREF_COMMON)
					bits |= ADC_COMMON_BIT;
				/*  mark end of queue */
				if (i == cmd->chanlist_len - 1)
					bits |= QUEUE_EOSCAN_BIT |
					    QUEUE_EOSEQ_BIT;
				writew(bits,
				       priv(dev)->main_iobase +
				       ADC_QUEUE_FIFO_REG);
				DEBUG_PRINT
				    ("wrote 0x%x to external channel queue\n",
				     bits);
			}
			/* doing a queue clear is not specified in board docs,
			 * but required for reliable operation */
			writew(0, priv(dev)->main_iobase + ADC_QUEUE_CLEAR_REG);
			/*  prime queue holding register */
			writew(0, priv(dev)->main_iobase + ADC_QUEUE_LOAD_REG);
		}
	} else {
		unsigned short old_cal_range_bits =
		    priv(dev)->i2c_cal_range_bits;

		priv(dev)->i2c_cal_range_bits &= ~ADC_SRC_4020_MASK;
		/* select BNC inputs */
		priv(dev)->i2c_cal_range_bits |= adc_src_4020_bits(4);
		/*  select ranges */
		for (i = 0; i < cmd->chanlist_len; i++) {
			unsigned int channel = CR_CHAN(cmd->chanlist[i]);
			unsigned int range = CR_RANGE(cmd->chanlist[i]);

			if (range == 0)
				priv(dev)->i2c_cal_range_bits |=
				    attenuate_bit(channel);
			else
				priv(dev)->i2c_cal_range_bits &=
				    ~attenuate_bit(channel);
		}
		/*  update calibration/range i2c register only if necessary, as it is very slow */
		if (old_cal_range_bits != priv(dev)->i2c_cal_range_bits) {
			uint8_t i2c_data = priv(dev)->i2c_cal_range_bits;
			i2c_write(dev, RANGE_CAL_I2C_ADDR, &i2c_data,
				  sizeof(i2c_data));
		}
	}
	return 0;
}

static inline void load_first_dma_descriptor(struct comedi_device *dev,
					     unsigned int dma_channel,
					     unsigned int descriptor_bits)
{
	/* The transfer size, pci address, and local address registers
	 * are supposedly unused during chained dma,
	 * but I have found that left over values from last operation
	 * occasionally cause problems with transfer of first dma
	 * block.  Initializing them to zero seems to fix the problem. */
	if (dma_channel) {
		writel(0,
		       priv(dev)->plx9080_iobase + PLX_DMA1_TRANSFER_SIZE_REG);
		writel(0, priv(dev)->plx9080_iobase + PLX_DMA1_PCI_ADDRESS_REG);
		writel(0,
		       priv(dev)->plx9080_iobase + PLX_DMA1_LOCAL_ADDRESS_REG);
		writel(descriptor_bits,
		       priv(dev)->plx9080_iobase + PLX_DMA1_DESCRIPTOR_REG);
	} else {
		writel(0,
		       priv(dev)->plx9080_iobase + PLX_DMA0_TRANSFER_SIZE_REG);
		writel(0, priv(dev)->plx9080_iobase + PLX_DMA0_PCI_ADDRESS_REG);
		writel(0,
		       priv(dev)->plx9080_iobase + PLX_DMA0_LOCAL_ADDRESS_REG);
		writel(descriptor_bits,
		       priv(dev)->plx9080_iobase + PLX_DMA0_DESCRIPTOR_REG);
	}
}

static int ai_cmd(struct comedi_device *dev, struct comedi_subdevice *s)
{
	struct comedi_async *async = s->async;
	struct comedi_cmd *cmd = &async->cmd;
	uint32_t bits;
	unsigned int i;
	unsigned long flags;
	int retval;

	disable_ai_pacing(dev);
	abort_dma(dev, 1);

	retval = setup_channel_queue(dev, cmd);
	if (retval < 0)
		return retval;

	/*  make sure internal calibration source is turned off */
	writew(0, priv(dev)->main_iobase + CALIBRATION_REG);

	set_ai_pacing(dev, cmd);

	setup_sample_counters(dev, cmd);

	enable_ai_interrupts(dev, cmd);

	spin_lock_irqsave(&dev->spinlock, flags);
	/* set mode, allow conversions through software gate */
	priv(dev)->adc_control1_bits |= ADC_SW_GATE_BIT;
	priv(dev)->adc_control1_bits &= ~ADC_DITHER_BIT;
	if (board(dev)->layout != LAYOUT_4020) {
		priv(dev)->adc_control1_bits &= ~ADC_MODE_MASK;
		if (cmd->convert_src == TRIG_EXT)
			priv(dev)->adc_control1_bits |= adc_mode_bits(13);	/*  good old mode 13 */
		else
			priv(dev)->adc_control1_bits |= adc_mode_bits(8);	/*  mode 8.  What else could you need? */
	} else {
		priv(dev)->adc_control1_bits &= ~CHANNEL_MODE_4020_MASK;
		if (cmd->chanlist_len == 4)
			priv(dev)->adc_control1_bits |= FOUR_CHANNEL_4020_BITS;
		else if (cmd->chanlist_len == 2)
			priv(dev)->adc_control1_bits |= TWO_CHANNEL_4020_BITS;
		priv(dev)->adc_control1_bits &= ~ADC_LO_CHANNEL_4020_MASK;
		priv(dev)->adc_control1_bits |=
		    adc_lo_chan_4020_bits(CR_CHAN(cmd->chanlist[0]));
		priv(dev)->adc_control1_bits &= ~ADC_HI_CHANNEL_4020_MASK;
		priv(dev)->adc_control1_bits |=
		    adc_hi_chan_4020_bits(CR_CHAN
					  (cmd->
					   chanlist[cmd->chanlist_len - 1]));
	}
	writew(priv(dev)->adc_control1_bits,
	       priv(dev)->main_iobase + ADC_CONTROL1_REG);
	DEBUG_PRINT("control1 bits 0x%x\n", priv(dev)->adc_control1_bits);
	spin_unlock_irqrestore(&dev->spinlock, flags);

	/*  clear adc buffer */
	writew(0, priv(dev)->main_iobase + ADC_BUFFER_CLEAR_REG);

	if ((cmd->flags & TRIG_WAKE_EOS) == 0 ||
	    board(dev)->layout == LAYOUT_4020) {
		priv(dev)->ai_dma_index = 0;

		/*  set dma transfer size */
		for (i = 0; i < ai_dma_ring_count(board(dev)); i++)
			priv(dev)->ai_dma_desc[i].transfer_size =
			    cpu_to_le32(dma_transfer_size(dev) *
					sizeof(uint16_t));

		/*  give location of first dma descriptor */
		load_first_dma_descriptor(dev, 1,
					  priv(dev)->ai_dma_desc_bus_addr |
					  PLX_DESC_IN_PCI_BIT |
					  PLX_INTR_TERM_COUNT |
					  PLX_XFER_LOCAL_TO_PCI);

		dma_start_sync(dev, 1);
	}

	if (board(dev)->layout == LAYOUT_4020) {
		/* set source for external triggers */
		bits = 0;
		if (cmd->start_src == TRIG_EXT && CR_CHAN(cmd->start_arg))
			bits |= EXT_START_TRIG_BNC_BIT;
		if (cmd->stop_src == TRIG_EXT && CR_CHAN(cmd->stop_arg))
			bits |= EXT_STOP_TRIG_BNC_BIT;
		writew(bits, priv(dev)->main_iobase + DAQ_ATRIG_LOW_4020_REG);
	}

	spin_lock_irqsave(&dev->spinlock, flags);

	/* enable pacing, triggering, etc */
	bits = ADC_ENABLE_BIT | ADC_SOFT_GATE_BITS | ADC_GATE_LEVEL_BIT;
	if (cmd->flags & TRIG_WAKE_EOS)
		bits |= ADC_DMA_DISABLE_BIT;
	/*  set start trigger */
	if (cmd->start_src == TRIG_EXT) {
		bits |= ADC_START_TRIG_EXT_BITS;
		if (cmd->start_arg & CR_INVERT)
			bits |= ADC_START_TRIG_FALLING_BIT;
	} else if (cmd->start_src == TRIG_NOW)
		bits |= ADC_START_TRIG_SOFT_BITS;
	if (use_hw_sample_counter(cmd))
		bits |= ADC_SAMPLE_COUNTER_EN_BIT;
	writew(bits, priv(dev)->main_iobase + ADC_CONTROL0_REG);
	DEBUG_PRINT("control0 bits 0x%x\n", bits);

	priv(dev)->ai_cmd_running = 1;

	spin_unlock_irqrestore(&dev->spinlock, flags);

	/*  start aquisition */
	if (cmd->start_src == TRIG_NOW) {
		writew(0, priv(dev)->main_iobase + ADC_START_REG);
		DEBUG_PRINT("soft trig\n");
	}

	return 0;
}

/* read num_samples from 16 bit wide ai fifo */
static void pio_drain_ai_fifo_16(struct comedi_device *dev)
{
	struct comedi_subdevice *s = dev->read_subdev;
	struct comedi_async *async = s->async;
	struct comedi_cmd *cmd = &async->cmd;
	unsigned int i;
	uint16_t prepost_bits;
	int read_segment, read_index, write_segment, write_index;
	int num_samples;

	do {
		/*  get least significant 15 bits */
		read_index =
		    readw(priv(dev)->main_iobase + ADC_READ_PNTR_REG) & 0x7fff;
		write_index =
		    readw(priv(dev)->main_iobase + ADC_WRITE_PNTR_REG) & 0x7fff;
		/* Get most significant bits (grey code).  Different boards use different code
		 * so use a scheme that doesn't depend on encoding.  This read must
		 * occur after reading least significant 15 bits to avoid race
		 * with fifo switching to next segment. */
		prepost_bits = readw(priv(dev)->main_iobase + PREPOST_REG);

		/* if read and write pointers are not on the same fifo segment, read to the
		 * end of the read segment */
		read_segment = adc_upper_read_ptr_code(prepost_bits);
		write_segment = adc_upper_write_ptr_code(prepost_bits);

		DEBUG_PRINT(" rd seg %i, wrt seg %i, rd idx %i, wrt idx %i\n",
			    read_segment, write_segment, read_index,
			    write_index);

		if (read_segment != write_segment)
			num_samples =
			    priv(dev)->ai_fifo_segment_length - read_index;
		else
			num_samples = write_index - read_index;

		if (cmd->stop_src == TRIG_COUNT) {
			if (priv(dev)->ai_count == 0)
				break;
			if (num_samples > priv(dev)->ai_count) {
				num_samples = priv(dev)->ai_count;
			}
			priv(dev)->ai_count -= num_samples;
		}

		if (num_samples < 0) {
			printk(" cb_pcidas64: bug! num_samples < 0\n");
			break;
		}

		DEBUG_PRINT(" read %i samples from fifo\n", num_samples);

		for (i = 0; i < num_samples; i++) {
			cfc_write_to_buffer(s,
					    readw(priv(dev)->main_iobase +
						  ADC_FIFO_REG));
		}

	} while (read_segment != write_segment);
}

/* Read from 32 bit wide ai fifo of 4020 - deal with insane grey coding of pointers.
 * The pci-4020 hardware only supports
 * dma transfers (it only supports the use of pio for draining the last remaining
 * points from the fifo when a data aquisition operation has completed).
 */
static void pio_drain_ai_fifo_32(struct comedi_device *dev)
{
	struct comedi_subdevice *s = dev->read_subdev;
	struct comedi_async *async = s->async;
	struct comedi_cmd *cmd = &async->cmd;
	unsigned int i;
	unsigned int max_transfer = 100000;
	uint32_t fifo_data;
	int write_code =
	    readw(priv(dev)->main_iobase + ADC_WRITE_PNTR_REG) & 0x7fff;
	int read_code =
	    readw(priv(dev)->main_iobase + ADC_READ_PNTR_REG) & 0x7fff;

	if (cmd->stop_src == TRIG_COUNT) {
		if (max_transfer > priv(dev)->ai_count) {
			max_transfer = priv(dev)->ai_count;
		}
	}
	for (i = 0; read_code != write_code && i < max_transfer;) {
		fifo_data = readl(priv(dev)->dio_counter_iobase + ADC_FIFO_REG);
		cfc_write_to_buffer(s, fifo_data & 0xffff);
		i++;
		if (i < max_transfer) {
			cfc_write_to_buffer(s, (fifo_data >> 16) & 0xffff);
			i++;
		}
		read_code =
		    readw(priv(dev)->main_iobase + ADC_READ_PNTR_REG) & 0x7fff;
	}
	priv(dev)->ai_count -= i;
}

/* empty fifo */
static void pio_drain_ai_fifo(struct comedi_device *dev)
{
	if (board(dev)->layout == LAYOUT_4020) {
		pio_drain_ai_fifo_32(dev);
	} else
		pio_drain_ai_fifo_16(dev);
}

static void drain_dma_buffers(struct comedi_device *dev, unsigned int channel)
{
	struct comedi_async *async = dev->read_subdev->async;
	uint32_t next_transfer_addr;
	int j;
	int num_samples = 0;
	void *pci_addr_reg;

	if (channel)
		pci_addr_reg =
		    priv(dev)->plx9080_iobase + PLX_DMA1_PCI_ADDRESS_REG;
	else
		pci_addr_reg =
		    priv(dev)->plx9080_iobase + PLX_DMA0_PCI_ADDRESS_REG;

	/*  loop until we have read all the full buffers */
	for (j = 0, next_transfer_addr = readl(pci_addr_reg);
	     (next_transfer_addr <
	      priv(dev)->ai_buffer_bus_addr[priv(dev)->ai_dma_index]
	      || next_transfer_addr >=
	      priv(dev)->ai_buffer_bus_addr[priv(dev)->ai_dma_index] +
	      DMA_BUFFER_SIZE) && j < ai_dma_ring_count(board(dev)); j++) {
		/*  transfer data from dma buffer to comedi buffer */
		num_samples = dma_transfer_size(dev);
		if (async->cmd.stop_src == TRIG_COUNT) {
			if (num_samples > priv(dev)->ai_count)
				num_samples = priv(dev)->ai_count;
			priv(dev)->ai_count -= num_samples;
		}
		cfc_write_array_to_buffer(dev->read_subdev,
					  priv(dev)->ai_buffer[priv(dev)->
							       ai_dma_index],
					  num_samples * sizeof(uint16_t));
		priv(dev)->ai_dma_index =
		    (priv(dev)->ai_dma_index +
		     1) % ai_dma_ring_count(board(dev));

		DEBUG_PRINT("next buffer addr 0x%lx\n",
			    (unsigned long)priv(dev)->
			    ai_buffer_bus_addr[priv(dev)->ai_dma_index]);
		DEBUG_PRINT("pci addr reg 0x%x\n", next_transfer_addr);
	}
	/* XXX check for dma ring buffer overrun (use end-of-chain bit to mark last
	 * unused buffer) */
}

void handle_ai_interrupt(struct comedi_device *dev, unsigned short status,
			 unsigned int plx_status)
{
	struct comedi_subdevice *s = dev->read_subdev;
	struct comedi_async *async = s->async;
	struct comedi_cmd *cmd = &async->cmd;
	uint8_t dma1_status;
	unsigned long flags;

	/*  check for fifo overrun */
	if (status & ADC_OVERRUN_BIT) {
		comedi_error(dev, "fifo overrun");
		async->events |= COMEDI_CB_EOA | COMEDI_CB_ERROR;
	}
	/*  spin lock makes sure noone else changes plx dma control reg */
	spin_lock_irqsave(&dev->spinlock, flags);
	dma1_status = readb(priv(dev)->plx9080_iobase + PLX_DMA1_CS_REG);
	if (plx_status & ICS_DMA1_A) {	/*  dma chan 1 interrupt */
		writeb((dma1_status & PLX_DMA_EN_BIT) | PLX_CLEAR_DMA_INTR_BIT,
		       priv(dev)->plx9080_iobase + PLX_DMA1_CS_REG);
		DEBUG_PRINT("dma1 status 0x%x\n", dma1_status);

		if (dma1_status & PLX_DMA_EN_BIT) {
			drain_dma_buffers(dev, 1);
		}
		DEBUG_PRINT(" cleared dma ch1 interrupt\n");
	}
	spin_unlock_irqrestore(&dev->spinlock, flags);

	if (status & ADC_DONE_BIT)
		DEBUG_PRINT("adc done interrupt\n");

	/*  drain fifo with pio */
	if ((status & ADC_DONE_BIT) ||
	    ((cmd->flags & TRIG_WAKE_EOS) &&
	     (status & ADC_INTR_PENDING_BIT) &&
	     (board(dev)->layout != LAYOUT_4020))) {
		DEBUG_PRINT("pio fifo drain\n");
		spin_lock_irqsave(&dev->spinlock, flags);
		if (priv(dev)->ai_cmd_running) {
			spin_unlock_irqrestore(&dev->spinlock, flags);
			pio_drain_ai_fifo(dev);
		} else
			spin_unlock_irqrestore(&dev->spinlock, flags);
	}
	/*  if we are have all the data, then quit */
	if ((cmd->stop_src == TRIG_COUNT && priv(dev)->ai_count <= 0) ||
	    (cmd->stop_src == TRIG_EXT && (status & ADC_STOP_BIT))) {
		async->events |= COMEDI_CB_EOA;
	}

	cfc_handle_events(dev, s);
}

static inline unsigned int prev_ao_dma_index(struct comedi_device *dev)
{
	unsigned int buffer_index;

	if (priv(dev)->ao_dma_index == 0)
		buffer_index = AO_DMA_RING_COUNT - 1;
	else
		buffer_index = priv(dev)->ao_dma_index - 1;
	return buffer_index;
}

static int last_ao_dma_load_completed(struct comedi_device *dev)
{
	unsigned int buffer_index;
	unsigned int transfer_address;
	unsigned short dma_status;

	buffer_index = prev_ao_dma_index(dev);
	dma_status = readb(priv(dev)->plx9080_iobase + PLX_DMA0_CS_REG);
	if ((dma_status & PLX_DMA_DONE_BIT) == 0)
		return 0;

	transfer_address =
	    readl(priv(dev)->plx9080_iobase + PLX_DMA0_PCI_ADDRESS_REG);
	if (transfer_address != priv(dev)->ao_buffer_bus_addr[buffer_index])
		return 0;

	return 1;
}

static int ao_stopped_by_error(struct comedi_device *dev,
			       const struct comedi_cmd *cmd)
{
	if (cmd->stop_src == TRIG_NONE)
		return 1;
	if (cmd->stop_src == TRIG_COUNT) {
		if (priv(dev)->ao_count)
			return 1;
		if (last_ao_dma_load_completed(dev) == 0)
			return 1;
	}
	return 0;
}

static inline int ao_dma_needs_restart(struct comedi_device *dev,
				       unsigned short dma_status)
{
	if ((dma_status & PLX_DMA_DONE_BIT) == 0 ||
	    (dma_status & PLX_DMA_EN_BIT) == 0)
		return 0;
	if (last_ao_dma_load_completed(dev))
		return 0;

	return 1;
}

static void restart_ao_dma(struct comedi_device *dev)
{
	unsigned int dma_desc_bits;

	dma_desc_bits =
	    readl(priv(dev)->plx9080_iobase + PLX_DMA0_DESCRIPTOR_REG);
	dma_desc_bits &= ~PLX_END_OF_CHAIN_BIT;
	DEBUG_PRINT("restarting ao dma, descriptor reg 0x%x\n", dma_desc_bits);
	load_first_dma_descriptor(dev, 0, dma_desc_bits);

	dma_start_sync(dev, 0);
}

static void handle_ao_interrupt(struct comedi_device *dev,
				unsigned short status, unsigned int plx_status)
{
	struct comedi_subdevice *s = dev->write_subdev;
	struct comedi_async *async;
	struct comedi_cmd *cmd;
	uint8_t dma0_status;
	unsigned long flags;

	/* board might not support ao, in which case write_subdev is NULL */
	if (s == NULL)
		return;
	async = s->async;
	cmd = &async->cmd;

	/*  spin lock makes sure noone else changes plx dma control reg */
	spin_lock_irqsave(&dev->spinlock, flags);
	dma0_status = readb(priv(dev)->plx9080_iobase + PLX_DMA0_CS_REG);
	if (plx_status & ICS_DMA0_A) {	/*  dma chan 0 interrupt */
		if ((dma0_status & PLX_DMA_EN_BIT)
		    && !(dma0_status & PLX_DMA_DONE_BIT))
			writeb(PLX_DMA_EN_BIT | PLX_CLEAR_DMA_INTR_BIT,
			       priv(dev)->plx9080_iobase + PLX_DMA0_CS_REG);
		else
			writeb(PLX_CLEAR_DMA_INTR_BIT,
			       priv(dev)->plx9080_iobase + PLX_DMA0_CS_REG);
		spin_unlock_irqrestore(&dev->spinlock, flags);
		DEBUG_PRINT("dma0 status 0x%x\n", dma0_status);
		if (dma0_status & PLX_DMA_EN_BIT) {
			load_ao_dma(dev, cmd);
			/* try to recover from dma end-of-chain event */
			if (ao_dma_needs_restart(dev, dma0_status))
				restart_ao_dma(dev);
		}
		DEBUG_PRINT(" cleared dma ch0 interrupt\n");
	} else
		spin_unlock_irqrestore(&dev->spinlock, flags);

	if ((status & DAC_DONE_BIT)) {
		async->events |= COMEDI_CB_EOA;
		if (ao_stopped_by_error(dev, cmd))
			async->events |= COMEDI_CB_ERROR;
		DEBUG_PRINT("plx dma0 desc reg 0x%x\n",
			    readl(priv(dev)->plx9080_iobase +
				  PLX_DMA0_DESCRIPTOR_REG));
		DEBUG_PRINT("plx dma0 address reg 0x%x\n",
			    readl(priv(dev)->plx9080_iobase +
				  PLX_DMA0_PCI_ADDRESS_REG));
	}
	cfc_handle_events(dev, s);
}

static irqreturn_t handle_interrupt(int irq, void *d)
{
	struct comedi_device *dev = d;
	unsigned short status;
	uint32_t plx_status;
	uint32_t plx_bits;

	plx_status = readl(priv(dev)->plx9080_iobase + PLX_INTRCS_REG);
	status = readw(priv(dev)->main_iobase + HW_STATUS_REG);

	DEBUG_PRINT("cb_pcidas64: hw status 0x%x ", status);
	DEBUG_PRINT("plx status 0x%x\n", plx_status);

	/* an interrupt before all the postconfig stuff gets done could
	 * cause a NULL dereference if we continue through the
	 * interrupt handler */
	if (dev->attached == 0) {
		DEBUG_PRINT("cb_pcidas64: premature interrupt, ignoring",
			    status);
		return IRQ_HANDLED;
	}
	handle_ai_interrupt(dev, status, plx_status);
	handle_ao_interrupt(dev, status, plx_status);

	/*  clear possible plx9080 interrupt sources */
	if (plx_status & ICS_LDIA) {	/*  clear local doorbell interrupt */
		plx_bits = readl(priv(dev)->plx9080_iobase + PLX_DBR_OUT_REG);
		writel(plx_bits, priv(dev)->plx9080_iobase + PLX_DBR_OUT_REG);
		DEBUG_PRINT(" cleared local doorbell bits 0x%x\n", plx_bits);
	}

	DEBUG_PRINT("exiting handler\n");

	return IRQ_HANDLED;
}

void abort_dma(struct comedi_device *dev, unsigned int channel)
{
	unsigned long flags;

	/*  spinlock for plx dma control/status reg */
	spin_lock_irqsave(&dev->spinlock, flags);

	plx9080_abort_dma(priv(dev)->plx9080_iobase, channel);

	spin_unlock_irqrestore(&dev->spinlock, flags);
}

static int ai_cancel(struct comedi_device *dev, struct comedi_subdevice *s)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->spinlock, flags);
	if (priv(dev)->ai_cmd_running == 0) {
		spin_unlock_irqrestore(&dev->spinlock, flags);
		return 0;
	}
	priv(dev)->ai_cmd_running = 0;
	spin_unlock_irqrestore(&dev->spinlock, flags);

	disable_ai_pacing(dev);

	abort_dma(dev, 1);

	DEBUG_PRINT("ai canceled\n");
	return 0;
}

static int ao_winsn(struct comedi_device *dev, struct comedi_subdevice *s,
		    struct comedi_insn *insn, unsigned int *data)
{
	int chan = CR_CHAN(insn->chanspec);
	int range = CR_RANGE(insn->chanspec);

	/*  do some initializing */
	writew(0, priv(dev)->main_iobase + DAC_CONTROL0_REG);

	/*  set range */
	set_dac_range_bits(dev, &priv(dev)->dac_control1_bits, chan, range);
	writew(priv(dev)->dac_control1_bits,
	       priv(dev)->main_iobase + DAC_CONTROL1_REG);

	/*  write to channel */
	if (board(dev)->layout == LAYOUT_4020) {
		writew(data[0] & 0xff,
		       priv(dev)->main_iobase + dac_lsb_4020_reg(chan));
		writew((data[0] >> 8) & 0xf,
		       priv(dev)->main_iobase + dac_msb_4020_reg(chan));
	} else {
		writew(data[0], priv(dev)->main_iobase + dac_convert_reg(chan));
	}

	/*  remember output value */
	priv(dev)->ao_value[chan] = data[0];

	return 1;
}

static int ao_readback_insn(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data)
{
	data[0] = priv(dev)->ao_value[CR_CHAN(insn->chanspec)];

	return 1;
}

static void set_dac_control0_reg(struct comedi_device *dev,
				 const struct comedi_cmd *cmd)
{
	unsigned int bits = DAC_ENABLE_BIT | WAVEFORM_GATE_LEVEL_BIT |
	    WAVEFORM_GATE_ENABLE_BIT | WAVEFORM_GATE_SELECT_BIT;

	if (cmd->start_src == TRIG_EXT) {
		bits |= WAVEFORM_TRIG_EXT_BITS;
		if (cmd->start_arg & CR_INVERT)
			bits |= WAVEFORM_TRIG_FALLING_BIT;
	} else {
		bits |= WAVEFORM_TRIG_SOFT_BITS;
	}
	if (cmd->scan_begin_src == TRIG_EXT) {
		bits |= DAC_EXT_UPDATE_ENABLE_BIT;
		if (cmd->scan_begin_arg & CR_INVERT)
			bits |= DAC_EXT_UPDATE_FALLING_BIT;
	}
	writew(bits, priv(dev)->main_iobase + DAC_CONTROL0_REG);
}

static void set_dac_control1_reg(struct comedi_device *dev,
				 const struct comedi_cmd *cmd)
{
	int i;

	for (i = 0; i < cmd->chanlist_len; i++) {
		int channel, range;

		channel = CR_CHAN(cmd->chanlist[i]);
		range = CR_RANGE(cmd->chanlist[i]);
		set_dac_range_bits(dev, &priv(dev)->dac_control1_bits, channel,
				   range);
	}
	priv(dev)->dac_control1_bits |= DAC_SW_GATE_BIT;
	writew(priv(dev)->dac_control1_bits,
	       priv(dev)->main_iobase + DAC_CONTROL1_REG);
}

static void set_dac_select_reg(struct comedi_device *dev,
			       const struct comedi_cmd *cmd)
{
	uint16_t bits;
	unsigned int first_channel, last_channel;

	first_channel = CR_CHAN(cmd->chanlist[0]);
	last_channel = CR_CHAN(cmd->chanlist[cmd->chanlist_len - 1]);
	if (last_channel < first_channel)
		comedi_error(dev, "bug! last ao channel < first ao channel");

	bits = (first_channel & 0x7) | (last_channel & 0x7) << 3;

	writew(bits, priv(dev)->main_iobase + DAC_SELECT_REG);
}

static void set_dac_interval_regs(struct comedi_device *dev,
				  const struct comedi_cmd *cmd)
{
	unsigned int divisor;

	if (cmd->scan_begin_src != TRIG_TIMER)
		return;

	divisor = get_ao_divisor(cmd->scan_begin_arg, cmd->flags);
	if (divisor > max_counter_value) {
		comedi_error(dev, "bug! ao divisor too big");
		divisor = max_counter_value;
	}
	writew(divisor & 0xffff,
	       priv(dev)->main_iobase + DAC_SAMPLE_INTERVAL_LOWER_REG);
	writew((divisor >> 16) & 0xff,
	       priv(dev)->main_iobase + DAC_SAMPLE_INTERVAL_UPPER_REG);
}

static unsigned int load_ao_dma_buffer(struct comedi_device *dev,
				       const struct comedi_cmd *cmd)
{
	unsigned int num_bytes, buffer_index, prev_buffer_index;
	unsigned int next_bits;

	buffer_index = priv(dev)->ao_dma_index;
	prev_buffer_index = prev_ao_dma_index(dev);

	DEBUG_PRINT("attempting to load ao buffer %i (0x%x)\n", buffer_index,
		    priv(dev)->ao_buffer_bus_addr[buffer_index]);

	num_bytes = comedi_buf_read_n_available(dev->write_subdev->async);
	if (num_bytes > DMA_BUFFER_SIZE)
		num_bytes = DMA_BUFFER_SIZE;
	if (cmd->stop_src == TRIG_COUNT && num_bytes > priv(dev)->ao_count)
		num_bytes = priv(dev)->ao_count;
	num_bytes -= num_bytes % bytes_in_sample;

	if (num_bytes == 0)
		return 0;

	DEBUG_PRINT("loading %i bytes\n", num_bytes);

	num_bytes = cfc_read_array_from_buffer(dev->write_subdev,
					       priv(dev)->
					       ao_buffer[buffer_index],
					       num_bytes);
	priv(dev)->ao_dma_desc[buffer_index].transfer_size =
	    cpu_to_le32(num_bytes);
	/* set end of chain bit so we catch underruns */
	next_bits = le32_to_cpu(priv(dev)->ao_dma_desc[buffer_index].next);
	next_bits |= PLX_END_OF_CHAIN_BIT;
	priv(dev)->ao_dma_desc[buffer_index].next = cpu_to_le32(next_bits);
	/* clear end of chain bit on previous buffer now that we have set it
	 * for the last buffer */
	next_bits = le32_to_cpu(priv(dev)->ao_dma_desc[prev_buffer_index].next);
	next_bits &= ~PLX_END_OF_CHAIN_BIT;
	priv(dev)->ao_dma_desc[prev_buffer_index].next = cpu_to_le32(next_bits);

	priv(dev)->ao_dma_index = (buffer_index + 1) % AO_DMA_RING_COUNT;
	priv(dev)->ao_count -= num_bytes;

	return num_bytes;
}

static void load_ao_dma(struct comedi_device *dev, const struct comedi_cmd *cmd)
{
	unsigned int num_bytes;
	unsigned int next_transfer_addr;
	void *pci_addr_reg =
	    priv(dev)->plx9080_iobase + PLX_DMA0_PCI_ADDRESS_REG;
	unsigned int buffer_index;

	do {
		buffer_index = priv(dev)->ao_dma_index;
		/* don't overwrite data that hasn't been transferred yet */
		next_transfer_addr = readl(pci_addr_reg);
		if (next_transfer_addr >=
		    priv(dev)->ao_buffer_bus_addr[buffer_index]
		    && next_transfer_addr <
		    priv(dev)->ao_buffer_bus_addr[buffer_index] +
		    DMA_BUFFER_SIZE)
			return;
		num_bytes = load_ao_dma_buffer(dev, cmd);
	} while (num_bytes >= DMA_BUFFER_SIZE);
}

static int prep_ao_dma(struct comedi_device *dev, const struct comedi_cmd *cmd)
{
	unsigned int num_bytes;
	int i;

	/* clear queue pointer too, since external queue has
	 * weird interactions with ao fifo */
	writew(0, priv(dev)->main_iobase + ADC_QUEUE_CLEAR_REG);
	writew(0, priv(dev)->main_iobase + DAC_BUFFER_CLEAR_REG);

	num_bytes = (DAC_FIFO_SIZE / 2) * rs/cb_in_sample;
	if (cmd->stop_src == TRIG_COUNT &&
	    rivers/cb_/a driver for the > priv(dev)->ao_count)
	drivers/cb_pcds.

    Author:  Fs a driver for the Corivers/cb_pccfc_read_array_from_buffer
   ->write_subdev,
	the I-DA  <fmhess@users.sbounceess

  o the followinrivers/cb);
	for (i = 0; i <AS
    64xx, 60xx, and 4020 ; i++) {
		ks alw(g people:

    Steve Rosenb[i]o thollowing people:

main_iobase + das64.c
 comedi	}
	fmhess@users.sourcef-=  his pci-das6402 driver, andComputerBoards/MeasurementComputing <fmhess@users.sourcef== 0rankreturn 0yright (C) 2001lo Frao_dmaess

    Th, cmde computrivers/cb_pthe boards.

 -1on pcidas-4020 su>= DMA_BUFFER    T) ;
	r much test feedback on
	estistart_sync feedb0ight rds.

    }

static inline int external_ai_queueer fuse(struct comedi_device *    
{n pcid Than02 Fro go t->busy boards.

    Jmputboard
    Aulayouam thLAYOUT_402e boards.

    Jelse mputusftwaram is e soft6xxx(&fy
    it under tha Dav->ack  boards.

    Jrds.

 1hleef.org>

  t thormdyou can redistribute it an, ou can redistro go tte itsd/or ou can redistrcmd * ANY= &sse, or
    (ght mputogram is free software; y    rce codarn_ogram is e sofERCHA;.

    COMEEBUSY code /* disable analog output system during setup */
ode for 0x0, Laird and Mariusz Bogacz.  NoCONTROL0 comedi/dds.

    Authoestiindexde foe was
    used directl=backoards/Marg *, writchanlist_lenght set_dac_select_reg feedback on ation, Idatioval75 Ms feedback on r mucfirstse
  descriptod feedbou should haveense
  ****_bus_addr | the   PLX_DESC_IN_PCI_BIT |r: cbINTR_TERMomputiight ation, Icontrol175 Mass Ave, Cambrse, or
  inttrig = Thihe PLX hleef <ds@schleef.org>

    Thihe PLX rogram is distributed in the hope that it will be usefrs by Gregunsignedri HePLX _numul,
    but WITHOUT ANY WARRANTY; without even	i Heretvalen the int Compu !port.

    COMEEINVALhleef <vals <fmeperface
    Copyrighthe iM3/16, <AS64/M2/16,
  PPIPEuting PCI-DAS64xx, 075 Mass Ave, CamComputerBoard97-8 asurementCoINTranks.

    u should have received a copy oSTART comedi/dies with the PLX 908NULAS64/M3/Author: Frank Mori Hess cmdtest@users.sourceforge.net>
Status: works
Updated: 2002-10-09
Device   but WITHOUT ANY WARd/or m prorrng withremenmp;
	s: [Measuremenmo the20/12

iM1/1/* step 1: make sureent Cger sources anal)
 vially validetai
	tmpot, write S64/M2/;
	 PCI-DAS64/M2/1&, PCI-DAS6 |mentCoEXTDAS64/M! PCI-DAS64/M2/1||ons:I-DA boards may berankerr++M1/1
These boardcan_begr fobe autocalidio input),
sith the cTIMERdi_calibrate utility.

Tononzero channehe bnc trigger idio input),
sthe 4020 (instead of theconvemay be autocal scan_beginith the cNOWe utility.

TTRIG_OTHER, he bnc trigger  scan_beginthe 4020 (instead of the dio end),
specify a nonz externith the cmputiou wish to use an me devicaster clock on the 4 externthe 4020 (instead of the ds/Meas autocalibs/Measuand using NEe utility.

To s/Measuhe bnc trigger ins/Measthe 4020 (inshe imrr boards.

 1I bus of dev2ce (optional)
   [1] - PCI slot uniquU Ged mutuvice compatiGNU nal)
/*ces:an aness checketail4,
  PCI-DAS64/M2/1!h the comed&&igger input on oards thiEX64/M34020 (ie are a numnonzero channeards thi chansg PCI-DAsed, but does not yet since y are
		fully er coultaneousilityvice
	there are a nuTRIG_OTHER, rementCoEXdriver willnonzero channerementCo chanre
		fully released, b.org.

*ards thimputing PCI-DAfo
*/

#include "../cottpsriver will #include "../coailable.
	supporf user attempts an2I bus of dev3ce (optionalargument slot of device ltaneously
	usereleased, but does not yefo size adjustce coeleased, but does nothe < ral Public Lao_s?)
	speedCIDAS6cify a nonzero chthe =gging code */
/* #define PTICU		fully rode 64_DEgetch teivisorEBUG		/*  disable deo the #includeflags) > maxor:  Fer_valueCIDAS64_DEBUG         enablePCIDowin(#else
#define DEB +s is a chan_BASs://IDAS64_DEBUG
#}9080.h"CONFIG_tware
    FCIDAS6ee Software
    F =n aiIDAS64_DEB}080.h"
#include  extthe ion insn
ER_BASE	10000	/*  100k, maybe I'll upport this somedaylock for slow ystem.h>

#include "3I bus of dev4: fix up any253.h"
#inclx9080.h"
#include "comedi_fc.h"

#undef PCIDAS6ns:
   d of the dio input)   [0]NT(format, args...)
#endiowine DE_PRINT(format, args...)  primat , ## args 40MHz master cl64/M1xxx baslock on the 4020, yargrank value that can be loaded into 4M1/14,
  PCI-this somCIDAS6s: [Measureme******thisne, PCCR_CHANisters */
enum[0]RTICUrce code 1or
   ee Software
    Fo source coBADDRE_REG = 0x0,	/*  interi]) 2,
rs {
	INTR_ENA+ i	/*  ha	redistrerr******* the follow"this som must use consecutive INTR_ENs"RTICUIDAS64_DEBU		breakr cloG
#dfine PRESCAer attempts an5-DAS6031, PCI-DAS6032, PCI-DAS60ancelrogram is distributed in the hope that it will be useful,
 s.

    You should have received a copy of the GNU Gener	abor*****id A. Schller
Author: Frank Mori Hedi,
	Allback(ntervar,ri HeportDELAY_data,es: [Measulong z Bogad/or modifirrce code fob(OWER_R(votion)(z Bogacz.INTE)RTICUDEBUG_PRINT("wrote 0x%x to_INTE %i\n",LOWER_RINTERTICULAR PURwith} Softwe co6031, P02 Fb(ter */
	ADC_DELAY_INTERVAL_}dc sample interval counter hed b*/
	ADC_DELAY_INTERVAL_LOWER_REG = 0x1a,	/*  lower 16 bits of delay interwal counter */
	ADC_DELAY_2 *_INTERVAL_Uunter */
	ADC_COUNT_LOWER_REG = 0w1e,	/*  lower 16 bi = 0x22,	/*  dware conversion/s_rbits@users.sourceforge.net>
Status: works
Updated: 2002-10-09
Deviou can redistrinsn *D_RE_REG = 0x1a, PC*OWERd/or s: [Measuremeonvepporonve =G = 0x1ds.

    Auval c
#definz Bogacz. If the coEG = &= 0xf;
	OWER[1]le dCLEAR queue0 use0hleef <ds@s2adc sample intervo_wonversion */
	ADC_QUEUE_CLEAR_REG = 0x26,	/*  clears adc queue */
	ADC_QUEUE_LOAD_REG = 0x28,	/*  loads adc queue *an_bits(internal  couzeroe adc w8253e goor mtorol rgy
	usS602.orgeREG ~an_bitsx54,	/*set newwer 16erval counter|=LOWERER_RE upper1]pporinterval counteu should haveGH_REG = 0x2c,	/*  higof the cl queue, usel countenline above */
	DAC_CONTROL0_io_60xlse
nfigAD_RErogram is distributed in thCONTRus: works
Updated: 2002-10-09
 dac_convert_regD_REG = 0x28,	/*  loads adc queue */
	ADC_BUFFERmaskTARTunsiegis <<LE_REG = D_RErs */
specI-DAS6witch al corrupNT_LcogacINSNof tFIG_DIO_INPUT:
	S602io_ for int~unsignTROL1_REG + (4 * (channel & 0x1OUT;
}

static inline|= unsignint dac_msb_4020_reg(unsigned iQUERY

stqueue, useintec inline  unsi) ? COMEDIint cha :0,	/*  h));
}TICULAR PUR2_chaefault

st2/16,
  PCI-DAS6hat cample intec inlin,PCI-DAwing people:

GH_REG = 0x2c,	/*  highO_DIRECTION_60XX comedi/dany later version.

     clear daG = 0x50,	/*  dac control register 0 */
	DAC_CONTROL1_REG = 0x5	/
	ADC_QUEUE_LOAD_REG = 0x28,	/*  loads adc queue *modifeturn 0x70 al counter */
	DAC_SAMPL = 0x56,	/*  UE_FIFO_ bits of dal coample interval cs by Greg Laird and M4,
	ADC_READ_PNTR_REG = 0xATAR_XFER_REG =hat cqueue, use2a,
	ADC_QUEUE_HIGH_REG = 0x2c,	/*  high */
	DAC_FIFO_REG ine above */
	DAC_CONTer */caln, Iks alrogram is distributed in thes: [MeasuremeINTR_EN4020_Rs: [Measuremee DEBU/or ds.

    Auio_counounte[INTR_EN usee DEBchannel)
{
	ral Public License0x70 + (4 publish_XFE:_REG = 0x48,
};4
/* b	io_coun8800nter_iofeedbaers {
	 0x0,
	 (4 * (channel & published bor write-oni2unter_ios */

enum intr_enable_contents s apparentlOL1_REG dware conversioncalibnter_ier */
};
static inline unsigned int  */
	ADC_QUEUE_LOArs {
	I8255_4020_REREG = 0x48,	/*  8255 offset, for 4020 only */
	ADC_Qgisters {
ABLE_REG = g(unsigned int chan/*12, .

 imdistatelywaresettor mhasn'istersged, since
	= 0xrogrammdac shese things is slowthere areEG = 0x20,
	DI_REG = 0x28,
	DIO_DI  upper 8attempts an ai cio_counter_ios */

enum intE_FIFO_R queue */
}er version.

    terrup02 Fr quater full */
	ADC_INTR_EOC_BITS = 01,	/*  interrupt end of con
	ADC_QUEUE_LOAD_REG = 0x2version,	/*  loads adc queue */
	ADC_BUFFERscan */
	ADC_INTR_EOSEQ_BITS = 0x3,	/an_bits() EG = 0x20,
	DI_REG = 0x28,
	DIO_one interrupt */
	DAC_INer */ad8402nter_iobase registers */
enum dio_counter_registers {
	DIO_8255_OFFSET = 0x0,
	DO_R.org>

*  atUFFER_CLEtream   Fgthegiswith/
	ADC_BUFFER_CL, regisefin adc_ch/
	ADC_BUFFER_CLEive irs {(scan */
& 0x3)_lsb8) | (ASE 25_ADCffCambr,	/*  enable dacINTR_BIudelayd' cloal Public LiceNTR_BI = 0x28,
	DIO_DIRECTION_60= 0x4000,	/*  = SELECT_TR_BIns fescr enabit *( status bit *ble_/scan c= 0x4000,	/* u should have received a coCALIBRALOWERcomedi/drce cbinot,c_lsb fortive interrupt- 1);e ad= 0x2 >>= 1CIDAS64_DErrun statu&e adNDEX ASK = 0x3,	/* | bitRIAL*/
	DAIN master Soft*/
	EXT_CLOCK_4020 unsTS = 0x3,	/*  use dio  clock source for 4020 **/
	INTERNAL_CLOCK_4020_BITS = 0x1,	/*  use 40 MHz internal master00,	/*  use external channel/gain queue (more v |ITS = 0xCLOCKescrrs by Greg Laird and Mariusz Bogacz.e) */
	SLOW_DAC_BITat c clock source for 4020 */
	INTEI-DAS6013, PCI-DAS6014,
  PCHz internal master}

/* rce pci-das6402/16/

enum i 0INTRGeneralinblicgaino com selects1INTRoffTERV*/rank Mori HesNTR_BIT = 00x30,
	DAC_INTR_QEMPTY_BITS = 0x0,
	DACAC_INTR_HIGH_CHAN_BITS = 0x10fo sizes */
	DAC_FIFO_SOSCAN_BITS = 0x2,	/*  interrupt end of scan */
	ADC_INTR_EOSEQ_BITS = 0x3,	/*  interrupt end of sequence (probably wont use this it's pretty fancy) */
	EN_ADC_INTR_SRC_BIT = 0x4,	/*  enabg_contents {
	MASTER_CL */
	EN_ADC_DONE_INTR_BITum hw_config_contents {
	MASTER_CLO	DAC_SAMP
	INTR_BIT = 0xnable adc aquisition done interrupt */
	DAC_INTR_S_SIZE_RK = 0x30,
	DAC_INTR_QEMPTY_BITS = 0x0,
	DACx1,	/*  interrupt end of conversion */
	ADC_INTR_EOSCAN_BITS = 0x2,	/*  interrupt endsource */
	EN_DAC_DONE_INTR_BIT = 0x80,
	EN_ADC_ACTIVE_INTR_BIT = 0x200g_contents {
	MASTER_one interrupt */
	DAC_INuint16_/12,ad_eepromobase registers */
enum dio_cint8_IT =dresful,
   	/*  enable dac active interrupt */cloc
	ADC_EXT_GATE_BIect gcomm comnter6 enable dac underrun status bDC_ANALOG_GATERRUN_BIware ga enable dac underru;
	er */
enablepldac bxx, */

/*=PCI-DAds.

    Auplx90802c,	/*  hi: cbf the G mas enathat selECTION_digital gate */
	Ae DEBnterrupt */TS =
	ADC_EXT_GATE_BIate */s bit */
};

en clock T_TRIG_ANALOG_BITLARITY_BIT = 0ital) */
clock */
CTL_EE_CLK &LING_BIT =Sx54,	/* (optionalwe doly wsends*/
N_ADC samthe i2c bus on ed b_REG SK = 0x30,
	ADC_START_TRIG_F|= NG_BUSERO0 */
	INlx4,	/*  enab	ADC_START_TRIG_, digital) */
	ADC_BIT couactivnterserialRT_TRIG_REG 0x30,
	ADC_START_TRIG_MASK = 0x30,
	ADC_START_TRIG_Fe */
	A trig 1 E_COUNTER_EN_BIT = 0x1000,	/*  enable hardware scan couunter ks alADC_A
#inG_GAT comdesired memory_BIT = 0_REG lock for 4020 */
	BNC_CLOCK_4020_BITS = 0x2,	/*  use BNC inpE_INTERV/*  to beG_BITten_REG 0x4000,	/*  disables dma put for master clock */
	ER_EN_BIT = 0x1000,	/*  ena,	/*  mastean I input fo0x200,	/*  turn on extra noiALLING_BIT herinE_COUNTER_EN_BIT = 0x1000,	/*  enable hardware scan counrt prelock in,
	AD= 0x40,	/*  software gate of a0x200,	/*  turn on extra noise for ditCLKannel/gai0x300,
	ADC_HI_CHANNEL_4020_MASK = 0xc00,
	TWO_CHANN	/*  two channel mode for 4020 */
	FOUR_CHANNEL_4020_ALLING_BIT = 0,	/*  four channel mode for 4020 */
	CHANNEL_MODE_4020_MASK e the
= 0x1,ter _START ri HABLE_BITor boarlocatioT = 0xic inlg withlock for 4020 */
START_TRIG_EX = 0x10,
	EOC_POLARITY_BIT = 0020_BInse = 0x1000,	/*  two channel mode for 4020 */
	FOUR_CHANNEL_4020_BITS = 0x2000,	/*  four channel mode for 4020 */
	CHANNEL_MODE_4020_MASK = 0x3000,
	ADC_MODE_MASK = 0xf000,
};
static inline uint16_t adc_lo_chan_4020_bits(unsigned int channel)
{
	return (channel & 0x40,	/*  software gate of adc *ect l(e hardware scan c &*  mastestablits(uns|se adt with << 8de*/
	ADC_DBLE_BITMA_DISA0 for  = 0x4000,	/*  disables dma */
	ADC_ENABLE_BIT = 0x8000,ALLING_BIT = adc enable */
};

enum adc_control1_contents {
	ADC_QUEUE_COinterruECTION_ Frank Mori HeT_TRIG_00,	/*  use trig/ext clk bnc input for external start trigger signal */
};
static inline uint16_t analog_trig_low_threshold_bitsan_bits() ect gate */
n theDC_INTR_EOSEQ_BITS = 0xdone interrupt */
/* ut100khzfuncn_402that rounds be set ftiancy) o an achiev GNU time,ould
 *ences/
	Ht adbers approprd of s.olaradc paCI s scan_sionsline uma4000line uiby _PRIdV_FAby (x + 3) where xINTR246_t anut */
se 0/1 */
	v)->dice
	_adc_nts {
rogram is distributed in the hope that it , PCI-DAS6071source */
	EN_scan_be_PRINT(sign, s?)
	_PRINT(TS = 0x20,
	ADC_STARminac bOSCAN_BIT = 0x83end of scan */
};
staelse
nline uint16_t PCI-DAfine TIMER_BASE 25		atic inline uint16_end of scan */
};
static,	/*  queue hed b =g thiEG = 0x1a,	/* ,	/*  int,	/*  queue ,BIT = 0x8000,	/* ase registers ng (not available odef PCIDAS64_DEral Public License as published byIDAS64_DEBUE_BIT =  base withADC_COUNT_LO	 channel)
{
	returnons */
enum base_addres0x40,
	WAVELX9080_BADDRIr clomputechannel)
{
	ret)
#else
nline uint16_	/*  30,
	WAVEFORM_TRIGd int channel)
{
	re0x10,
	WAVEFORM_TRIG_EXT_<um dac_control0_contM_TRIG_ADC1_BITS = 0x30,
 dac_control0_content_BIT = 0x40,
	WAVEFORVEFORM_TRIG_EXT_ 40MHz master clfineC_COUN slow pacing (not available oNOWrankIT = 0x40,
	WAVEFORM_T080.h"
#include "comedi_fc.h"

#undef PCIDAS6,	/*  queue  =/
enum base_address_regions {
	PLX9080_BADDRIof adc *M_MODE_BIT = 0x100,
	DAC_EXT_UPDATE_ for XXXvice
	trce dati [1]overflowh > 16		IT = 0x8000,	/* x1,
};

enum dac_contE 0x1000

/* maximum 	DAC_CYCLIC_STOP_#endif

#deLAR_BIT = 0x8,
	DAC0_UNIPOLAR_BIT =el &  +_DISABLE#else
#define DEB_TRIG_MASK = 0x3= 0x20,
	DAC1_UNIPOIT = 0x8000,	/*  dac  0x2,
};

/* bit definiannel & 0x3f;
};

enum da,	/*  queue endUG
#defin
/* bit defin)
#els,	/*  queue 	/*  E_BIT = 0x8,
	DAC_I10,
	ADC_INTR_PEG_BIT = 0x20,
	DA_GATE__BIT = 0x40,
	ADC_DONE_BIT = 0x80 0x10,
	ADC_INTR_PE_DEBUG         enable 
/* bit defin 40MHz master cat cinterrSELECT_BGbiponearestBIT = 0x800,	/*V_FAgivendifferential bine P, does notolart(optinUNIPcourcefpossusly
minimum/max> 6) */
	efine DEBs.  Usepolarby otherents {
	(commV_FA};

enums.se 0/1 */
	s: [Measureme
enum base_as: [Measuremenso_counter_regis## argsequence */
	QUEU0,
	DAC_WAVnel)
{
	## ar &mentCoROUND_MASK0x70 + (4 t prepost_bUPnly_rAC1_UNIPO(ns +40MHz mastetus_co/40MHz master clt dac_msb_402t prepost_bDOWNs >> 14) & 0x3nxx, r 4020 */
enum i2c_addresses {
	RANGE_NEARES}

ss apparentl 14) & 0x3;
}

/* I2C addreis is	CALDAC0_I2C_ADDR = 0xc,} *  6 : dRIG_FALLI,	/*  bits ts)
{
	return (pBUG_PRINT(fits >> 12) & 0x3;
}

static inline unsiginterru
enum base_a0x3;## args-e */
	D/= 0xjustsING_Bsize of hardwts ofifo (which determines b_bits(
starce dma xferRIND 0/1 */
	FIFOatioai_int8_gnedobase registers */
enum dio_counter_regisriveor the unsigned int adc_urive& ADCentrie0x4,02/12, PCI-D30,
stC_INTR_Hhw
{
	/*infote (for int8_e debugging cod) & ADdi/drive{
	/*  atten0x20nuate_bit( /(chan->or the_paptr_c_ran_4S64/M3/16, PC << 4) & ADC_eg"
#interrup = 0xc,
	ADC_COinput ranges for _BIT S = 0xtatic xx bo	8,
	{e bit64/M1/16/JR,
  PCI-DAS64/2, PCI-DASxx boards */t_ch/16, *),
	 BIP_RANGE(1.25_RANGE(1.const struct comedi_lranPPER_REG = 0xTERV inline uint8_gned tots of deenuate_bit(u queue */
}xx boards *SELECT_Bque_hi_erruptof(chann prepost_bits)
{
	retur4) & ADC_SRC_4020_MASK;
};

static inl make bnc trds.

    Aut4xx = {
	8,
	{
	 BIP_ *PCI-DA & 0x3);
};

/* ana1.25)
	 }
};

/*edi_lrange ai_ranges_6030 =const struct comedi_lr Frank Mori Heges_64xx = {
	8,
	{
	 BIP_R};
static inline unsigned int dDevices: [Measuremexx b  atten 0x800,	/*  enable dacincre,
	{
gned nter10	ADC5V input range) */
	return 1 << (channel & 0x3);
};

/* analogned int channel)
NGE(1),
	0x4,	that sel_CLEAR_Rpcidas-4ges for <ANGE(1),
	 UNI_rank Morges for 64NGE(1),
	 UNI_on pcidas-4ges for >),
	 BI0,
	E	8,
	{
	 BIP_= {
	15,
	{
	 BIP_NGE(2.5),
	 BIP_RANGE(1)E_CONFIG1te s256   atten, 2te s512 BIP_RANGEetc 0x802, etc boards 0x3;
(5),
	 BIP_+ANGE(1),
	 UNI_R_SRC_40RANGE(10),
	 BIP_REG = 0x(~UNI_R_RANGE(5),
ITS BIT 6030 =& ADC_SRC75 M_72 + (4ds.

    Au analog in0 : groun};

/* analog input ranges for 4020 board */
static cation 0x4,de for working QNXard */
static 
	PIPE1_READ_REG = 0xariusz Bogacz.4.c
    TU General Public Licestatic const struct co64xx b_RANGE(5),
*RANGE(0.5),
	 UNI_Roards */
static const struct com	8,
	{ANGE(5),lrange aiions */ ao_ranges_64xx = {
	4,
	{
	 BIP_ = {
	4,
	{
 ao_ranges_64xx = {
	4,
	{
	 BIP_SELECT_B 0x86025 ly rdio_cou:T = 0x with 0te sigct selects ctherwiGE(10),
	 }
(0.1)tatic const inanalGE(10),
	 }
(0.05tatic const 1nt ao_range_code_603ct comedi_lrange astatic const str4te sf Thi0x10t ao_range_code_605te scoars}
};

static const int a6_range_code_603static const str710),
	 }
};

static /60xx = {
	/*  b10,	s all 8rol regis int sRANGE(10),
	 }
};

static const in
	 }
static const strxx[] = {
	0x0,
};

ge_codestatic const struct comedi_lrangeum registo_ranges_6030 = {
	2,
	{
	 BIP_RANGE(4XX,
	LAYOUT_4020,
};

st(10),omedi_lrange ange_code_4020[] = {
	o_ranhw_fifo_info {
	unsigstatic const str,
};
onst int ao_range_ct ao_range_code_60_lranunsigned int max_set ao_ranal)	DAC_INTR_SRC_e-only registerbase registers */
enum dio_counter_regis0),
	 }xff00,	/* /*  softBIT = 0x800,	/*  enable dacNI_R  pci d
	 BIP_RA= 8TS = 0x20,
	ADC_STARTS = 0x2,	/*  external dable dac underrun status bi0),
	 }
_ADC7OVERRUN_BIN_BIT = _DAC_UNDERRUN_BIT = 0x4000,	/*  en
	ADC_EXT_GATE_BI  pci device bit */
};

enmput0),
	 }
>64xx but resolution *00	/*  OW_4020_REG = 0x "illegalint ao_nput/outter 0     COMEDI -}16 channels */
	CONVERT_POLARITY_BIT = 0x10,
	EOC_POLARITY_BITASK = 0x3,	/*  b	ADC_ut for master clock */
	EXT_CLOCK_4020_BITS = 0x3,	/*  use dio  clock nt ao_bits;		/*  ahannel/gain queue (more versatile than internal queue) */
	SLOW_DAC_BIT EXT_CLOCK_4020_BITS = 0xstead of   different board families have slightly different registers */
	unsigned has_8255:1;
};

static con} 0x4000,	 board families have s/scan cits thaty reof 5 */
	HW_CONFIG_DUMMY_BITS = 0x2000,	/*  bit wsk = 0x7f,
};

static const struct hefault value in pci-das64 manual */
	DMA_CH_SEnt_length = 0x800,
	.sample_pa6031, PCI-DAS/*,	/*    pci h > 1ice_id;		/*  pci d	/*  bits er full */
	ADC_INTR_EOC_BITS = 0xsource */
	EN_t resolution o_counter_regis0x0,
	DO_R/*  softMA_DISers/cb[3AMPL/*  soft	/* 0),
ee S PCIpodatioatic c_BIT = 0manualobab analog intherwisa noinel)
{eional		OFFSET_0_2RANGE(e_coGAINI_DMA_RIN2e_co MAX_AI1_3RANGE4_COUNT (0SIZE)
#d8,
	}to a rinE_FInd the maxNOT/*
    comISTERS000 / 0NG_COUe_ptr_codereg_mask = 0x70x70 + (4 0:NNEL_40han int ao_rx1000,ether inABLEALDAC0_I2C_ADDRchannansfers we wts()  MAX_AI_DM (4 * (channel & 1atic inline  ao_rang int ai_dma_ring_count(struct pcidas64_board *board)
{
	if (bo1_dc_cm i2c_addresse2atic inline 2otherwise 0 ai_dma_ring_count(1truct pcidas64_board *board)
{
	if (board->layout == LAYOU3atic inline 3;

static const struct pcidas64_board pcidas64_boards[] = {
	{
	 .namatic const int bytes4atic inline unanaloint ai_dma_ring_count(struct pcidas64_board *board)
{
NT (0x80c const int bytes5_4020)
		returao_scan_speed = 10000,
	 .layout = LAYOUT_64XX,
	 .ai_range_tablatic const int bytes6in_sample = 2;ao_scan_speed = 10000,
	 .l4_board pcidas64_boards[] = {
	{_table = &ai_ranges_64xx,7
	 .device_id 55 = 1,
	 },
	{
	 .name = "pci-das6402/12",	/*  XXX check */
	 .atic const int s apparentl*  analog output spen(optio a scan, not c\nonversion) */
	con0,	/*  bits 4_board *boar, useFFER_SIZE)
#define A| (T = 0x8>>RUN_000,	Cambrinsfers we w2_DIRECTIO8000,	/,
	 /*  bits that ether in,transfers we , 3nfo ai_fifo_60xx = {TheirBIT =requirRANG huge bit */onence (proine uinE(10ta high int som = 0xs4020_bANGE(2),
	 UNI_RAN2c_16,
		/*  analo00.5)	 .ao_nchan = 2,
	 .alowts = 16,
	 .;x = {AX_AIT =ts =   Thi16,
	or SRC_BITingle-ended 	 .aatiosda/
	int ai_se_chans;	/*  numb),
	 unte 0x800,	/*  enable dac10000 / ABLENEL_4020_Mer */
digital) */
	ADC_GOLARITY_BIT = 0x8,	/*  gate active low */
	ADCoutputcode =e maximumAX_Arange_table = &c conrces for 6025 are:
 *  0 : groun10000 / ,	/*  four channel mode for 4020 */
	CHANNEL_MODE_4020_MASK = 0x3000	 .ao_bits = 16EG = C_COUNTi_se_chans = 64,
	 .SRC_BITfor 4020 */
	FOUR_CHANNEL_4020_BITan = 2,
	 .ao_bits = 16,
	 .ao_scan_speed = 10000,
	 .layout = LAYOUT_64XX,
	 .a.layout = ,	/*  ini4XX,
	 .ai_020_BI_table = &ai_ranges_64xx,
	 .ao_range_tabcSAMPLE_INTERVAL_LOWER_REG = 0nge_code = ao_range_code_64xx,
020_Bi_fifo = &aDC_SAMPL64xx,
	 .has_8255 = 1,
	 },
	{
	 .name = "pci-das64/m2/16",
	 .device_id = 0x36,
	 .ai_se_chansdevice_id = 0x37ts = 16,
	 .ai_speed = 500,
	 .ao_nch
	 .ao_sc	 .ao_bits = 16,
	 .ao_scan_speed = 10000,
	 .layout = LAYOUT_64XX,
	 .ai_range_table = &ai_ranges_64xx,
device_id =e_table = &ao_ranges_64xx,
	 .ao_range_coifo = &ai_fifo_64xx,
	 .has_8255 = 1,
	 },
	{
	 .name = "pci-das6013",
	 .device_.name = "pci-das64/64xx,
	 .ao_rangks alsrs/c
	ADC_SOFT_GATE_BITS = 0x1,	/*  softrs/cmber of dma t&ai_fi(10),
	 UNI_RANGE,	/*  b8ANGE(5),
	 UNI_RAks aV_FALLIIT = y	/*  upof defifo er clock for 4020 */
0,
	 },
	ITS = 0x2,	/*  use BNC inpu	 .ai_bits   upper 8 w_fif(e_id clock 	/*  range_table n the1 bit input foed = 100000,
	 .lo_bits _nchan = 2,
	 .aayoutas64/m3/we caly wrevice  0x1,NG_B  Th10,
o fts)
{ise 0/1 */
	FIFOe = 02 Frackyou can redistribute it and/or m_nchan = 2,
	 .ao_bited = 100000,
	 .layoute = &ai_ranges_60xx,
o ai_fifo_6board-interruange_acknowledge6_t adc_64/m3/16nd_codr16_t adc_64xx,
	 .ao_range .ao.ai_fifo = &ai_fifo_60xx,
	 .has_8255 = 0,
	 },
023",
	 .devicpci-das6023",
	 .devic.ai_range_taed = 5000,
	 op_nchan = 0,
	 .ao_scan_speop.ai_fifo = &ai_fifo_60xx,
	 .has_8255 = 0,
	 },
	{
	 .name = "pci-das6	{
	 .name = "_range_table = &ai_ranges_60xx,
	  = &ao_ranges_60xx,
	 .a/
	int ai_se_chans;	/*  number of ai inputs in singlPE1_RE5V inp/*  soft*LOWER_REG = 0x1a of NGE(1),
equence */
	QUEUil chain tog	BNC_CLOCl digital gate */
	ADC_AN_fifo 0x;

e/*-depenet foutepper 8revc cosimultaneous attemp5V *returith BLE_BIT comIT = 0x8	user cou falling edge */	ADC_EXT_CONV_FALLIBLE_BIT = 0rces for 6025 are:
 *  0 : ground
 *  1 : 1",
	 .d_cod PARTICan_speed = PARTIPLE_INTEshou),
	 }
 com_BIT == 0x1000rrun status b	 .ao_bi<<s_co& ~_table =eed = 1
	 .ao_rangn the	BNC_CLOCao_nchan geinlis = 12,
	 here areode_60xx,
	 .es fI-DAS6ed;	/*  analog output spe2c= 16,
	failed: nretus = 12,
	ter 0  16,
	 .ai_speed >> 10) &x3) << 8_BIT =ts = rs/cb_> 16 chanode for
   ao_rang source coo_64xx,
	nge_table =E_FIFis as A&ao_ranges_6030,
	 .ao_range_codde = ao_range_code_6030,
	 .ai_fifo = &ai_fifo_60xx,
	 .haas_8255 = 0,
	 },

	{
	 .nam2,	/* as_8255 = 0,
	 }}
