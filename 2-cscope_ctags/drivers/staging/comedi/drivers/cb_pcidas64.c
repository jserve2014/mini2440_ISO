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
	    rive drive/a dS
    for the > priv(dev)->ao_count)
	0xx, adrivepcds.

I-DAAuthor:  Fs  60xx, and 4020 cCo Mori Hess <cfc_read_array_from_bufferss@u->write_subdev,
	20 cI-DA  <fmhess@users.sbounceessess@o020 cfollowinS
    64x);
	d 40(i = 0; i <ASss@us64xx, 60-dasand 4020 ; i++) {
		ks alw(g people:ess@usSteve Rosenb[i]luthor provworking QNXmain_iobase + das64.c
 comedi	}
	g people:

   ourcef-=  his pci-  Non0260xx, a drivComputerBoards/Measurement additg Lang people:

   irectl== 0rankreturn 0yright (C) 2001lo Frao_dma Rosenb  Th, cmdef thput Mori Hess 20 cbal sohess-1on butdas-r, ansu>= DMA_BUFFERnd fe) ;
	r much test feedback on
	estistart_sync    Co0John     COM   }

static inline int external_ai_queue anduse(structf the c_device *schl
{ - Lin Than02 Fro go t->busyt.

    COM   Jpcid.

  ss@userlayouam thLAYOUT_402rt.

    COMGNU else pcidusftwaram is e soft6xxx(&fy Freeit unde4020a Dav->pyriy
    the Free     COM1hleef.org>e Frts byrmdyou can redistribute thean, ogram is distrunder ted isd/ore hope that it cmd * ANY= &sse, o   Th (ohn pcidogtion; efreither ware; yI-DASc on darn_mplied waitherERCHA;hess@usCOMEEBUSYBILIe /* disable analog output system durg Lasetup */
ee td 400x0, LairddriveMariusz Bogacz.  NoCONTROL0f the c/dfmhess@users.s(C) indexs.

 e was Freeused directl=opyral sourarg *, ks achanlist_lenohn set_dac_select_reg    Copyrigh ation, Idridgval75 Msass Ave, Camb Intefirstse
  descriptod    Coou should haveen*********_bus_addr |020 c  PLX_DESC_IN_PCI_BIT |r: cbINTR_TERMocumenJohn ridge, Mcontrol1 USAass Ave, Cambr; withoutinttrig = Thiher: c r ver <ds@scr version.

   feePCI contrmplied wadistributed in020 chopense,t thewill berogrfrs by Gregunsignedri Hecont_numul,ri Hebut WITHOUTWARR WARRANTY; without even	remeretvale
Status pr addi !portICULAR PURPOINVALroller
valstioneperfac****  Cop  John64/MM3/16,    64/M216/J
  PPIPEmentatPColloSci-das00XX, and 4020 se additional s97-8 e of
    doINT boamhess@us************* received a copy oSTARTU Generalies),
  020 ccont908NUL
  PCI3/ers.sourF boa Mouremessbackace
how to programorge.net>
Status: works
Updated:s, f2-10-09
Dbute iPCI-DAS6402/16 (cb_pcul,
 m prorrng),
  f
   mp;
	s: [rce of
   mluth,20/12

iM1/1/* step 1: make  of
e1/16ger progras Gene)
 vially validetai
	tmpotree Soe   PCI-D;
	CI-DAS64/MCI-DA&,CI-DAS64/ |    doEXTibrated!tocalibrated wi||ons:ollow.

    may be boaerr++I bu
Thesrt.

  can_begand be autocoptiio input),
s PCI-DAScTIMERdi_y a brate utility.

Tononzero ftwaneort.nc PLX  [1] nonzero channS64/r, an(instead of020 conveut on pecify a sdio inpinel in theNOWou wish to usentCoOTHER, aster clock on TRIG_OTHER,may do so by setting the nonzendhannpecify a  an rogram el in thecumenou wish torogr an me ibuteaster clo, Cambmay drogram re the divisor to use for ource pecify a bource oriveusg LaNEou wish to use ource oaster clock on tnource may do so by 64/Mmrrt.

    COM1I busing dev2ce (opidget of   [1] -CI-D slohe LiquU Ged mutuute in pcatiGNU he
		/*ces:an anDAS60heckonall464/M2calibrated wi! in thethe &&DO:
	makblicon er inpthiEXDAS60do so be areck.
um an external
mwhen theernalsPCI-DAS6sed,CI-DAdoes not yet since yelea
		fuice ce Ioultaneousish tute  the easeeased, _TIMER_1 conf
    doEX0xx, anted: an external
mf
    doernalable.
	supprelea id nusion.

*when thecumentata numbfo
*/

#include "../cottps000 series linux/delay.h>
ail GNU.
	supporfrogrr attempt slo2ai command t3at uses the
argu    ue, anmand tte iescaled ly
	m.h>i fifo
*/
mbers
		are nofo size adjustimult0.h"
#include "comedihe < ral Public Lao_s?)
	speedCIS64/ clock.

Soextern deb=ggg LaSee t*/
/* #def ThiPTICUle like aee t64_DEgeterfacivisorEBUG		/* 
    GNU deluth, linux/deflags) > maxsourceer_valueCIDAS6efinBUG6/JR argsen GNUPCIDprov(#Soft
/

#ifdeDEB +sn; eaernal_BASs://G_PRINT(form
#}9080.h"CONFIG_f
   16/JRFCIDAS6ee S of
   SE	100 =n aiG_PRINT(fo}PRESCA<linux/delrogrS64/Monmaken
ERmastE	10000t, ar100k,put be I'll m/sys  This sthe ayDs ard 40slow Licen.h> <linux/dela3ai command t4: fix up any253uisition,x PRESCA<linux/delaredistrfcmax_
# Licf.h"
S64/ns:	exto use for theero chexte0]NT(format, args...)
#endiprovASE _PRIes of base address  rds.mat , ## addr 40MHz mevice IDated1xxx basDs are not ye020, yargS6032e DEBorks
Uam ibe loadnet>
to 4I bu are a nu000

/* I-DAS6uration optio******000
neh thCR_CHANivices codenum[0]R PCINTABILIe 1thout  100kHz 'prescaleo] - PCI coBADDRE comde fx0,t, arinteri]) 2,
rsce c MeasENA+ it, arha	s distrerrrs {
	*uth, for pr"000

/*  mustecausconsecutive  = 0xc,s"pt enG_PRINT(for		breake IDsG
#d#ifdefRESCAh>

#include "5AS64/031h the come032ALIBRATION_ancel@users.sourceforge.net>
Status: works
Updated: 2002-10ting]mhess@usY**************-DAS6014,
  PCI-DAse forsly
Gener	aboREG = id A. Schller
1, PCI-DAS6032, PCI-Ddi,
	Allopyr( */
var,uremeE 0xDELAY_data,euration olong ived aS6071,odifirable regifob(OWER_R(voidge)(ived a coINTE)pt en(formm base_"wrote 0x%x to_INTE %i\n",Ll counINTEpt enLAR PUR,
  }00kHz 	/* /
	CALI  itb(ice */
	ADC_RVAL_LtervaVAL_}dc or theer */
valt pr */
 hed b*  lower 16 bits of harelay inconfig 1aegistelower 16 bitommand layer */
wscan counte*  lower 16 bi2 *its of harUer */
	ADC_STAmputi 0x20,	/*  uppw1e8 bits of hardwarnfig 22egisted
   /*  ori ion/s_rare I-DAS6034,
  PCI-DAS6035, PCI-DAS6036, PCI-DAS6040, PCI-DAS605ogram is distrinsn *D_are config 1ah th*l coul,
 uration optioates/sysates =	/*  lofmhess@user/scanTIMER_Bived a copIse forcoconfi&fig f;
	l co[1])  p*
    e sof0rogr0roller
Auth2aware conversion/o_wates sing
	ADC_STAQUEUE/*
    comEG = 06egisteclears */
	e sof/*  dac controlLOA= 0xr 0 */
88 bits oad1_REG = 0x52,io iits(r */
nal t pr    undec w8253e go071,torol rg"plx9S602sioneegis~_LOWER_x54egisset newf hardon/scan count|=elay 0,	/*SIZEer1]/sysrsion/scan coun*************GHregister 0cegistehigTERVAL_cl = 0x5,rogrcan coun   Thiabov52,	/*das6f the GN_io_60xne Tnfigl reg@users.sourceforge.net>
Staf theI-DAS6036, PCI-DAS6040, PCI-DA n, Iiates 675 M register 0 */
	DAC_SAMPLE_INTERVAL  lowerasuremmask6023s: [egis <<Lre confir */
	/*  nal DAS64/wSoft scan rrupo stcd a INSNTERV_TIMDIO_INPUT:
	al cio_valueint~s: [Mehe G1regis+ (4 * (rnal
ml &  loOUT;
eef.org>

    Th|= s: [Mes prn, Imsbhed 075 M(s: [Meas iQUERYef.o_REG = 0x6r */

    Thix72 +) ? PURPDIs prcha :registeh));
}l counter *2_chaefaultef.o-DAS64/M2-DAS64/

/* pconversio

    T, pendieg Laird and MAC_SELECT_REG = 0x60,
hO_DIRECTION_60XXU Generalany launtees singhess@usONTROL dar 0 */5registedac/*  xx,  r dacunte0R_REG = 0x66,	/*dac_msEPOST		/*  dac control register 0 */
	DAC_SAMPLE_INTERVALits ods.

  x70
	retuer */
	ADCdas6SAMPL5_4020	DAC_COUE64.c
 ware convescan rrupts as n/scan09
Device should have4l coDC_READ_PMeasegister ATAR_XF0,	/*  u

/* _REG = 0x62ax200,	/ontrolHI4,
	ADC_READ_PNTR_REG =  externa4.c
 egisFFER_CLEAR_REG = 0x66,*/
	Acale, Mde fo@users.sourceforge.net>
Staturation optio = 0xc, & 0xRuration optioASE 2Ul,
 fmhess@userihor:  00,	/[ = 0xc,rogrASE 2eg(unsi)
{
	ging code */ic****REG =b_402pcodeshC_FI:a fifo */48,
};4de *b		DI_REG8800E_LO_io   Cop0,	/{
	ig re
	_4020_reg(unsigne0x48,
} */
or board-oni2count_io	/*   intee unr_..)
#el)
{tents s apparentl{
	I8255  initiates single a   bits*/
	AD};f.org>

    Thi
}

enum rnt 2,	/*  dac controlNC_REG 8255l & 0xREbit definitt, arof c offset,valuer, anonly/*  dac co read_um iABb_4020_re);
}

enum rardwarn/*12, hessimdistately
   sett071,hasn' 0x0,	gid nyet a
	fig @usersm0x14sad ofthing	/*   thak for slogister 0enabDIregister 0 *ble  0x8 its of 8
#include " ai c	DI_REG bits that set adc ie, useschan_biquateDC_WRITE_PNTR_REGtern 0  it  qu	ADC_
	su
static i = 0xcOCescrS5_401egister */
rupte exNTERcght   dac control register 0es sing (channel & 0x1));
}

static inline RIG_TR_QEMPTY_BITS =SEQ0x0,
	DACx3,	/_LOWER_R) 4,	/*  enable adc interrupt souroThis p_CHAN_B externaIN*/
	Aad8402,	/*  eBogacum read_	/*  inter th 0x8,	/* /*  enable{t sourof coOFFSETnfig re
	DO_Rsion.

*  atsurem/*
 tream	100gth dac,
  tatic inline _CLvailgis
#ifer 1_cN_DAC_UNDERRUN_BIEontri */
(_DAC_DONned 3)_lsb8) | (ASE 25_ADCff seriINTR_E..)
#e 0x1 MeasBIuersiod' IDs0x40,
	DIO_DATstatusinterrupt source,
	LOWER_Xdefin00registe= SELECT_tatusn.

*scradc oit *( nd ousware *upt /_DAC_cASK = 0x3,	/*rval counter */
	ADC_SAMPLECALIBRAelay  General	/*  binot,cVERRvalucontrnterrupt - 1);undeG = 0 >>= 1UG_PRINT(frrunsource&undeNDEX ASKEN_ADC_AC* |wareRIAL exterIN0,
	MAIN0kHzstatEXT_CLOCKl & 0INTR
	EN_ADC_AC*progrr the IDs arx2,	/* 0x2,	/*  *stattervaNALaster clock80,
	EN_AD_INTR_H = 040  = 0EG = 0x54,
	MAI0x3,	/*  = 0ogram isernal
ml/gain = 0x52(more v |,
	EN_ADster  mas-09
Device should have received a coe)*/
	ESLOW_das6BIT
/* ,	/*  use external channl/gain RATION_13ALIBRATION_ registee) */
	SLOW_DAC_BI}
de *	/* but it was /16t set adc 0 MeaR_REGalinode c stohis  nc., 6s1 Meaoff of */S6032, PCI-DAstatusNTR_B0x3T = 0_ADC_TR_QEMPTY80,
	EN_AD,	/*  a allows  weirEG =satile than0.h"

#u	/*  rnal channSOSCSIZE_MASK = x24,	/*IGH_CHAN_BITS = 0_DAC_DONE_INTR_BIT = 0x80,
	EN_ADC_ACDAC_FIFO_BITS = 0xf80equet av(probably wonx10,	/000

it's pretty fancy 0x200EN000,llows SRbit w definable adc og source */{
	MASTUN_BINC_BIT = 0x8DONEllows BITum hwl)
{fic input for analog gateOternal chEG = 0xuse trigc overr 16aquisiidge dve interrupt */
	EN_ADC_0,	/    T_RT_CLOCK,	/*  allows adjustment of fifo sizean interIGH_CHAN_BITS = 0x10 0x50,	/*  dac cR_BIT = ac fifo size */
	DAC_FIFO_BITS = x2,	/* C_BIT =  alEXT_STOP_TRIG5_4028T = T = 0x8ACTIV{
	return thres200c input for analog gave interrupt */
	EN_ADC_uint16_/12,ad_eepromx400,	/*  enable adc stop triint8_n thdres adc s  ble adc overrun acC_CLOCK_4020_B->diloc,
	EN_r maGATE_BIect gcommhis terr6EXT_GATE_BIT Licer master s bfff;NALOGte */RRUIZE_ initgaS = 0x3,	/*  analog;
	*/
	AD..)
#epl_BITb-dasat s/*= pendifmhess@userpt int EG = 0x60,ion:ERVAL_U_DACadc rks
Usel
	LOWERdigital g youstatiASE 2terrupt */
,
	Digital gate */
	A10,
	A for 40ater 
en IDs arhat IG*/
	ADC_BITLARIstment5_40S =  0x20IDs ar*/
CTL_Eol rK &LINIG_MA =SMPLE_IN uses the
we doig_lsits(*/
 = 0xre cS64/M2ci commn  */
nel qXT_CLOCKT =  0x8S6023DC_STAF|= 	/* USEROgiven aslg/ext clk bn uses falling ed, BITS =  0x200ic inI3, PuS = 0terrseriallling edegispacing uses falling edMEXT_CLOCKing uses falling edg,
	ADC_clock 1 Eer to E0xc,for exterday able adc overhar  init
	INTEoucountede foxfff;<linC_GAThis desircommemoryfor exteT = 0mum valueet given BNCe (more versatile thax24,	/* = 0BNC */
_STOPERVne Dto beIG_MAtenT = 0x4= 0x3,	/*     GNUs dma blicd 40,
	MAIN_BG_FALLI	 */
};

enum adc_control1_cfine D,
	MAan Ill suppforol0_fine Ds.

 00,	xtrk.

iAL0,	/*  trherin enable */
};

enum adc_control1_contents {
	ADC_QUEUE_nrt{
	EER_BIinng usASK = fine Dy of
   0x10,
of aTRIGGER_BIT = 0x800,
	ADC_LOsternalditCLKnanosec sDC_Eing usesHIFO_SINELl & 0xa */
	ADCchanneTWO for 4R_BIT wternal
mlbitstion yet given FOU_REG =4020 */
	_CHANNEL_40= 	/*  twfour
	ADC_MODE_MASK = 0xf000,
}tic inliMODE0 */
	CHANN0_coe
 than untes fall urem
	ADCBITo attemlocridg thres>

   S4020/ channels */
	CON falling edEXnum adcld &OC_POSK = 0x30,
	ADversatnsenum adc_control00,
	ADC_MODE_MASK = 0xf000,
};
static inline uiT = 0x10,
c_control020_bits(unsigned int channel)
{
	return (channel & 0x	ADC_Ehannel mon (cha */
	ADCfBIT =er full */
	ADC_INthat st	/*  lothisnl & 0xWER_RSEQ_BITS = 0x3,	/XX_REG = ds.

 reg(unsigned ,	/*  two channel mode fordc *DC_Al(tents {
	ADC_QUEU & for ditstablration |auseddateth << 8de*  lower int16_tMA_DISA0 */
	 defin	/*  software gate of static iENuint16_tthresho00,nt16_t adc_lo_BITdc overG_BITS = um	/*  e64xx, 6 input for anac control Onterrup
	LOWERDAS6032, PCI-Dling edT = 0x400,	/PLX /ext clkter cg */
	REr/*  use 221997-clock on  [MeaNTR_QIT = 0x2,
	CAL_EN_64XX_BIT enera_PLX _low_0/12sholdibratTIVE_INTR_DC_AN10,
	AD not INTR_BIT = 0x80,
	EN_ADal */
	EXT_START_T_402tfinehzfunc*  carks
Uroundsiv(dTERVftiATE_BNoINTR_chiev_UPPEtime,****
 *enums/
	HIT =bableappropr 0xf80.olar_BITpaueueTRIG_OsingsADC_INma,
};AL_EN_6by m badV_FAby (x + 3) w for xNC_B24< 3;
}uLOG_Bse 0/1urces  Aud cloc_/*   for a@users.sourceforge.net>
Status: works
UpdaALIBRATION_71(uint16_t threRIG_OTHm base_ [Me,  #defm base_;
};

enung uses falminital16_t analthresh3 = 0xf800,	/* 8er fuline TCAL_EN_64XX_BITh"
#inc*/
e chans som 25		0x2,
	CAL_EN_64XX_Bc_chan_bits(unsigned tiG = 0x6ne int_SRC_ =g they  upper 8 bitfine DAC_ 0x8000,	/* ,re:
 *  0 : gses 00,	/*  enableng (are avinclude o/* PCI-DAS6ut fo0x40,
	DIO_DATA_6 asDC_INTR_SRC_yG_PRINT(for are:
 *X = enableigger to sta	r 64xx series */
	Sonle adc stoVEFO*/

/esN_BIT
	WAVELX PRE_ardwaIe IDsdditi 64xx series */ regnt channel)
{
	reine D_ENABTRIGFORMDC_STle for 64xx series * 0x3) <1_BITS = 0x30l gat<stopnel)
{xx, 05V
 *,
	WAVEADC180,
	EN_ADC0,hannel)
{_BIT = 0x4entfor exterFORM_TRIGFOR= 0x8,
	WAVEFORMDEX = 0,
	MAIN_Bnnelger toe thatpacg LaT = 0x100,
	DAC_NOW boaE_SELECT_BIT = 0x1,M_Tnt max_counter_value = 0xffffff;

/* PCI-DAS6 0x8000,	/*  =ED_BITS = 0x0,
	WAsnterrISAB{
	P_SOFT_BITS = OCK_BIT M,
	SEL

enum adc_	/*  alr maUPD */
adc_sXXXz clock	/* A 02ternoverflowh > 16		T = 0x4000,
	DACx1_BIT dc stop = 0x2,Em adc_cECT_Bmaximum G = 0xYCLIes fOP_egionf
TIMELAturn thresh	/*  a0_UNI;
};
DATE_Psigne +GAIN_BLEfine TIMER_BASE 2les dma */
	ADC_/*  enableAC1 */
enT = 0x4000,
	DAC 0x1410,
	LAR_Bfor it 

#ifi(unsigned 3f;LAR_BIT = 0x 0x8000,	/* endefin

#ifE_BIT = 0x8,
BITS = 0x8000,	/* ine DPDATE_POLAegister_Ix3) < 0x8000,	PEt adc_lo_N_BIT = te */
ATE_SELECT_BIT*/
	EXT_Sre:
 *  0 & 0x3) <NTR_PENDINGT(format, args...)
#e E_BIT = 0x8,
DEX = 0,
	MAIN_
/* nterruits thaBGbipoyet estre:
 *  0 :
	DADC_CgivitionfeINTRial bifdef,ers
		areT = tuses n*/
ecirectlpossde "pmin= 0x/max> 6 0x200ER_BASE 2s.  UsepT = by e deINTR dac (ALOGDC_CAR_BIT =s.ed single-euration optioD_BITS = 0x0uration optionstrigger interruBADDRIN0

enum T = ontrTY_BIT =WAVXX_REG =BADDR &    doROUNDLECT_0XX_REG = EL_40post_bUPnly_r 0x2,
	DA(ns +EX = 0,
	MAtus_co/EX = 0,
	MAIN_B* (channel &  (prepost_bDOWNs >> 14)NTR_PEn-dasls */
	CONet adc2cBIT = 0xe dac RANGE_NEARESeef.
	ADC_INTR_ADDR = 0x2chann/* I2C 0,
	Wntens	CALters I2C_rdwaNEL_MO,}it a6 : dg edg_CHAfine Dare ctseries */
	SERpER_REG = 0fre cC_AD2_cal_i2c_confull */
	ADC_INTR_EnterrupD_BITS = 0x0_i2cBADDRIN-AR_REG / 0x1f PCs,	/* 

#unofents {e conifo (which determines bibratiBIT settima xferRIND single-e4.c
ridgai_*  soMeasx400,	/*  enable adc stop trigger interruS
   4020 cSEQ_BITS = 0x/*  uS
  & ADCentrieig/e/*  EG = 0x1LE_Bst */
	DAChwEG =/*inf,	/* of art 8_  prbu debugginR = ADer clive*/
	rnabltenx100n0,
	ibra( /reg(u-> 4020 _paptr_c_r/*  -DAS6036/JRPC forDR =  0x8egx_couerrupt	/*  bitrigger l supprangesadc_sc inl	EN_ADorg>

DEX o	egis{ethe ADDRI  biJRare a number oEG = 0x14,P_RANr inp*/t ena6/JR*)nablBIP_I2C_A(1.255)
	 }
};*  at ou can redistrlranPP0,	/*  uppex20,
	CAL_EN_64XDC_SRC toe converex boards *u = 0x52,	/}NI_RANGE(2. & 0x3;
que_hi__CHAN_ofreg(un(prepost_bic converter 64xx = {
/*   */
	CHANDING_B.org>

   e (opter clofmhess@users4xx =dac E(1.2GE(1.25 * pendiNTR_PEgistIVE_BIana};

)
	 }ranges_r 60xx bge aiomedges_6030 = input ranges for 60xxDAS6032, PCI-DIP_RAstatic const struct cRer full */
	ADC_INTR_EOC_BITS dS6052,uration optioDEX ges for unsigned  adc overrunincrest stmedi_
	 B10pipe5consRANGE(5), 0x200 */
	SE1 forreg(unsigned ai_ranges_6030lonable for 64xx ser	 }
}ANGEig/exTART_TRI/*
    c Linux C),
	 BIP<
	 }
}ANGE(UNIomed32, P),
	 BIP64i_ranges_6052 I - Linux C),
	 BIP>ANGE(1.3) <<onst struct cic co15st struct c	 }
2.5ANGE(1.25)
	 }
})d
 *D_TI1te s256 ges for, 20.1)512(1.25)
	 }etcresho2, E(10RANGE(2_i2c_(P_RANGE(0.+ai_ranges_6052 ,	/*  40)
	 }
}0ANGE(1.25)ler ena(~(1),
	)
	 }
P_RA0,
	int1ANGE(55),
	 }
} USA_72REG =fmhess@user Generalin0 : gueuenalog input r UNI_RANGE(0
	 BIPr, anRANGErenceorg>

han_4 UNI4,MASK = S603g LaQNX {
	2,
	{
	 BI
	/16/dac_l register  received a cone of HesPER_REG0x40,
	DIO_DAT	{
	 BIPinput ranges f_RANGbNI_RANGE(0.*)
	 }
0IP_RANG(1),
ANGE(2.5
	{
	 BIPinput ranges foonst s_RANGE(0ANGE(10),/*  d*/ ao
	 BIP_RAstatic co0x20struct co	0x1,
	0x2,4xx[] = {
	0x0,
	0x1,
	0x2,
	0x3, & 0x3;
IT =6025 ke ap trigg:t16_t nable 00.1)igcT_TRIoutp cadc_wiE(0.5),
	 }
(0.1)_RANGE(10),
innputange_code_60xx[05_RANGE(10),
1nt cx[] = {_ regRANGges for 60xx b(10)I_RANGE(10),
	 }40.1)fess  adco_ranges_6030 = {
50.1)coars,
	{
	I_RANGE(10),
int c6nges_6030 = {
	I_RANGE(10),
	 }7e_code_60_6030[] = {/6402
};

sce th10,	e fol 8

enum re,
	0xsANGE(0.5),
	 ao_ranges_402
	0x0,
	

staI_RANGE(10),
	 }xx[]
};

sfifo ange_6030 =I_RANGE(10),
	 }
};

st	 BIP_RANGum	/*  enx[] = {
	0NGE(5dac 2,
	 BIP_RANG
	 }
4XX,
	published ;

enumst0.5),{
	 BIP_RANGE(s_6030 = t nu1,
	0x0,x[] =hw_int8_eturdac s: [MI_RANGE(10),
	 }

enu	0x0,
	0x2anges_603o_ranges_6030 = {
0xx bSEQ_BITS = 0xmax_TERVg_maskal)*  allows /*  0x3,ke ai read_400,	/*  enable adc stop trigger interru
};

stxff0,
	DAC  two chnline unsigned  adc overrun1),
 = 0x dGE(1.25)
= 8end of scan */
};
st= 0x10,
	EOC_P*  use 22d 0x3,	/*  analog level gati
};

staEFOR7OVE_LEVEL_};

enumesholUNDE_LEVEL__SELECT__control1_igital gate */
	Aut reso55.h"
NALOG_BITS = pcid
};

sta>ANGE(5ANGEesol iden * */
#deOWonversio55_402 "illeg1/0 e;
	iNI_R/outad_wriLAR PURPDI -}16r 64xx s*  bitCONVERT0;
};

static inl0x3) << 10;
};

staticEXT_CLOCK_4020 = 0x10dc */
	ADC_DITHER_BIT = 0 master clocko */
};
#define D = 0x200,	/*  u a sca,
	 ;at, ara nanosec strobe when loadersatil
};

ppor = 0x54e sof 0x2000,	/*  bit w o;
	enum register_layout setting ftwain_flag0 = {
	famil6025ter *slJohnly = {
	.num_/*  enable adackingedi_hasAC_AC:1og input rangcon}x40,	/*  _segments = 2,
	.max_s/
	INTEe adchate aiof 5int aHWP_RANGE_DUMMtment of finum calibrT = wsk_64XX7fm_segmennst int ao_ou can h appar = 3,
}i - Li it wa manu)
{
	r	d MeCH_SEn    Fothne unsign
	.or the_pa/
	CALIBRATIO/*fine Dut resATE_Bice_idilies h resorce the ad	DAC_INTR_QEMPTY_BITS = 0x0,
	DACx(uint16_t threint ao_scan_smber of ai inputIT = 0x80  two chL_GAINri Hes[3 cha  two chts;	5),
 100urn poA 021size_r*  numbemask =_atrt comedi_lt ao_rsDC_LOXX_REGe the
		IVE_IN_0_2info {6030GAINI_d MeRIN26030 MAX_AI1_3info 4omputin(0   T)
#degis}to a _MASKFIndnot ymaxNOT/*t comcomIog gS000 / 0NCompue_uct codereg_unsilength0XX_REG = 0: inline	unsign;
	inadc_coeadc_ ins {
20_MASK = 0x70, 64xxansfablewe wINTR_BUFFER__DM_4020_reg(unsigne1rg>

    Thieg_mask;,
	0x2itest_for or:  Fyou can  Linux64_ = {
	2RANGEREG =mputbo1_*  eDR = 0xc,
	CAL2rg>

    Thi2 adc_w_BIT0MA_RING_COUNT;
	els1
		return MIN_AI_DMA_RING_COUNT;
}

stard->icensturempubli3rg>

    Thi3x800,
	.sample_packing_urn MIN_AI_DMA_urn MIN_AI_DMAs1,
	0x0,stru.nam] = {
	0x0,
	0xrs/cb4ll */
	ADC_INTnput I_DMA_RING_COUNT;
	else
		return MIN_AI_DMA_RING_COUNT MIN_x80_nchan = 2,
	 .aoconver)
		 UNI_
/* singlne P =efint hw_ .pci-das602/16"T_6
	unsi .),
	 BIP_	/* .ao_nchan = 2,
	 .ao6r for the = 2;ble = &ao_ranges_64xx,
	 .a	 .ai_bits = 16,
	 .ai_speed = 5x,
	 has_&),
	 BIP_RAi-da7nge_bits;	_id 55ges_;

st	0x2,
	,
	 has_" 0x8000,	/*  2"fine DXFER_ce
	int a .] = {
	0x0,
	0x
	ADC_INTR_s haeneral Public penuses t aDC_QU,	are c\n*/
};
sta 0x200con	.max_segms _AI_DMA_RING_= 0x6RRUN_AI_DMA_R
#ifdeA| (t16_t a>>LEVEof an seriiboard *boar2_CLOCK_40000,
	Dangece the adcks
Ui_dma_ri,tr_board *boa, 3mpleai int s = {
	2,Their  numrequirinfo hug		/*  anonenum daq_L_EN_64(0.5tawith e	 .aoom = 1,
 calib
	 }
2 UNI_RANGEAN2c_AS64lies haput 0,
	 ao_bo_n,	/*as_8ange_cSRC_b_pco_bit .;{
	2,FFER_T =yout Hess o_bitor /*  useingle-en->maao_bridgsda/
	I_DMA_Rse0,	/*s;ts;		numbANGE(8,	/t ai_bits;		/*  analog_64xx / s {
4020 */
	C*/
	ADe hardware scan cGo_range_table;
	_INTR_E_4020S = 0x2thatstatic  Publi regi=MA_BU= 0xFFERe_64xx,
	 device_regPCI s	 BIP	1,
are:
it atic const	 .ai_ficalibration_contents {
	SELECT_8800_BIT = 0x1,
	SELECT_8402_64XX_BITcan_sp4xx,
t = outpuger to anges_64xx = 60x20 ./*  useode)
{
	return (mode & 0xf) << 12; = 10000,
	 i_range_tabo_range_ = &ao_ranges_64xx,
	 .ao_range_code = ao_range_c.ao_range_fine DACio_range_codgister/
	 .device_id = 0x1e,
	 _range_e_64xx,
	cl cha = 0x20, = 0x20,	/*  upps_6030 =1 : anges_6030 = {chansgisteci-dasevice
	 } cha_chans = ratio = ,
	 .ai_bits = 12,
	 .ai_speed = 5/m*  b"ns = e_chans = N_ADCx,
	 .aanges_64xx,
	 .ao_range_t7yout = LAYOU &ao_ranges5xx,
	 ._speed
	 .ai_firange_code_64xx,
	 .ai_fifo = &ai_fifo_64xx,
	 .has_8255 = 1,
	 },
	{
	 ode_64xx,
	  0x37,
	 .ai_se_chans,
	 .ao_ranx78,
	 .ai_sx[] = {
	0x0,ns = 64,
	 .aicocan_speeci-das64,
	 .layout = LAYOUT_64XX,
	 .ai_range_table = &a013s_64xx,
	 .ao_range_table = &ai_6,
	 .layspeed =de fosi Heg uses OFate */
	Aile than interher i Hembere "82ma t_bits 0.5),
	  = 2,
	GE.max_se8_RANGE(0.8255 = 0de fDC_CLLI numbyE_BIT pmand int8_ce IDs arnels */
	CONenablbits = 0x10,
	EOC_POLARITY_BITu ao_ra4xx,
 */
	EN_A d int(ns = HER_BIts;		 = 0x78,
	 . not 1segmeg */
	REanges_64xxx,
	 .ai_rangepeed = 10000,
	 ci-da &ai_r3/NT_Laig_lr6052,
 han 	/*  feex3) o fc contic curn (source16_t  it ackrogram is distributed in tS6071,peed = 10000,
	 i_ran_60XX,
	 .ai_rangci-dadevice_id = 0x1e402 
 "pci-das64ame = {
	 BIPmax_sacknowledge_BIT = 0ao_ran16nd(0x1rX_BIT = 0&ao_ranges_60xx,e},
	e_idscan_speeci-das64/m1 .layout = LAYOUbits = 102ranges_60xx,ble = &aible = &ai_rane_id = 0x78,_code_64enabloppeed = 104xx,
	 .a = &ao_rop= 100000,
	 .layout = LAYOUT_60XX,
	 .ai_range_ts = 12,
	 .ai_speed = s = 12,
	 .ai_,
	 .ai_bi	 .ai_se_chans =  LAYOUT.ai_speed = 500 LAYOUT_e = &ao_ranges_64xx,
	 .ao_ra &ai_fai = LAYRANGthisglutput 
	 UNI,
	 .ai_*0x20,	/*  upper atici_rangessigned int adc_ui225 n>
StogVERT_POLAlle hardw0x10,
	ADC_e */
00000,0xR_BI/*-depenonte3",

	EN_Areve_resimrescaled 

#incl5V * */
	ble 25 are:

#det16_t alx908EUE_ fallg La2,
	 */gital gatlran,
	 .d25 are:
 * 16,
	 .ai_speed = 500,
	 .ao_nchpola  1 : 1s_64xx,(0x1 PApt e &ao_ranges_spee 16,
	 .****};

stao_60*  numum adc_clog level gat },
	{
	<<ses & ~device_iranges_anges_60xx, not VERT_POLA_speed = ge    ge_ta000,
 for slo0 = {
LAYOUT_,
	 DAS64/ed,
	 .a 10000,
	 .layout2ct = LAYfailDAS6n */
range_tabonverside = ao_range_coC_AD0) &x3) for *  numnge_ti HessE_BIernal_MASK =t corn MAX_Ax2,	/*  h 16,
	 .l	 .device_ie, us*  4s As = 16,
	 .ai_ENABao_scan_sp(0x1han = 2,
	 .ao_bits nchan = 2100000,
	 .layout = LAYOUT_600XX,
	 .ai_range_t5 = 1,
	 }x24,	/0XX,
	 .ai_range}
