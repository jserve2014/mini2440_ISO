/*
 * Serial port driver for the ETRAX 100LX chip
 *
 *    Copyright (C) 1998-2007  Axis Communications AB
 *
 *    Many, many authors. Based once upon a time on serial.c for 16x50.
 *
 */

static char *serial_version = "$Revision: 1.25 $";

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/major.h>
#include <linux/smp_lock.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/bitops.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#include <asm/irq.h>
#include <asm/dma.h>
#include <asm/system.h>

#include <arch/svinto.h>

/* non-arch dependent serial structures are in linux/serial.h */
#include <linux/serial.h>
/* while we keep our own stuff (struct e100_serial) in a local .h file */
#include "crisv10.h"
#include <asm/fasttimer.h>
#include <arch/io_interface_mux.h>

#ifdef CONFIG_ETRAX_SERIAL_FAST_TIMER
#ifndef CONFIG_ETRAX_FAST_TIMER
#error "Enable FAST_TIMER to use SERIAL_FAST_TIMER"
#endif
#endif

#if defined(CONFIG_ETRAX_SERIAL_RX_TIMEOUT_TICKS) && \
           (CONFIG_ETRAX_SERIAL_RX_TIMEOUT_TICKS == 0)
#error "RX_TIMEOUT_TICKS == 0 not allowed, use 1"
#endif

#if defined(CONFIG_ETRAX_RS485_ON_PA) && defined(CONFIG_ETRAX_RS485_ON_PORT_G)
#error "Disable either CONFIG_ETRAX_RS485_ON_PA or CONFIG_ETRAX_RS485_ON_PORT_G"
#endif

/*
 * All of the compatibilty code so we can compile serial.c against
 * older kernels is hidden in serial_compat.h
 */
#if defined(LOCAL_HEADERS)
#include "serial_compat.h"
#endif

struct tty_driver *serial_driver;

/* number of characters left in xmit buffer before we ask for more */
#define WAKEUP_CHARS 256

//#define SERIAL_DEBUG_INTR
//#define SERIAL_DEBUG_OPEN
//#define SERIAL_DEBUG_FLOW
//#define SERIAL_DEBUG_DATA
//#define SERIAL_DEBUG_THROTTLE
//#define SERIAL_DEBUG_IO  /* Debug for Extra control and status pins */
//#define SERIAL_DEBUG_LINE 0 /* What serport we want to debug */

/* Enable this to use serial interrupts to handle when you
   expect the first received event on the serial port to
   be an error, break or similar. Used to be able to flash IRMA
   from eLinux */
#define SERIAL_HANDLE_EARLY_ERRORS

/* Currently 16 descriptors x 128 bytes = 2048 bytes */
#define SERIAL_DESCR_BUF_SIZE 256

#define SERIAL_PRESCALE_BASE 3125000 /* 3.125MHz */
#define DEF_BAUD_BASE SERIAL_PRESCALE_BASE

/* We don't want to load the system with massive fast timer interrupt
 * on high baudrates so limit it to 250 us (4kHz) */
#define MIN_FLUSH_TIME_USEC 250

/* Add an x here to log a lot of timer stuff */
#define TIMERD(x)
/* Debug details of interrupt handling */
#define DINTR1(x)  /* irq on/off, errors */
#define DINTR2(x)    /* tx and rx */
/* Debug flip buffer stuff */
#define DFLIP(x)
/* Debug flow control and overview of data flow */
#define DFLOW(x)
#define DBAUD(x)
#define DLOG_INT_TRIG(x)

//#define DEBUG_LOG_INCLUDED
#ifndef DEBUG_LOG_INCLUDED
#define DEBUG_LOG(line, string, value)
#else
struct debug_log_info
{
	unsigned long time;
	unsigned long timer_data;
//  int line;
	const char *string;
	int value;
};
#define DEBUG_LOG_SIZE 4096

struct debug_log_info debug_log[DEBUG_LOG_SIZE];
int debug_log_pos = 0;

#define DEBUG_LOG(_line, _string, _value) do { \
  if ((_line) == SERIAL_DEBUG_LINE) {\
    debug_log_func(_line, _string, _value); \
  }\
}while(0)

void debug_log_func(int line, const char *string, int value)
{
	if (debug_log_pos < DEBUG_LOG_SIZE) {
		debug_log[debug_log_pos].time = jiffies;
		debug_log[debug_log_pos].timer_data = *R_TIMER_DATA;
//    debug_log[debug_log_pos].line = line;
		debug_log[debug_log_pos].string = string;
		debug_log[debug_log_pos].value = value;
		debug_log_pos++;
	}
	/*printk(string, value);*/
}
#endif

#ifndef CONFIG_ETRAX_SERIAL_RX_TIMEOUT_TICKS
/* Default number of timer ticks before flushing rx fifo
 * When using "little data, low latency applications: use 0
 * When using "much data applications (PPP)" use ~5
 */
#define CONFIG_ETRAX_SERIAL_RX_TIMEOUT_TICKS 5
#endif

unsigned long timer_data_to_ns(unsigned long timer_data);

static void change_speed(struct e100_serial *info);
static void rs_throttle(struct tty_struct * tty);
static void rs_wait_until_sent(struct tty_struct *tty, int timeout);
static int rs_write(struct tty_struct *tty,
		const unsigned char *buf, int count);
#ifdef CONFIG_ETRAX_RS485
static int e100_write_rs485(struct tty_struct *tty,
		const unsigned char *buf, int count);
#endif
static int get_lsr_info(struct e100_serial *info, unsigned int *value);


#define DEF_BAUD 115200   /* 115.2 kbit/s */
#define STD_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)
#define DEF_RX 0x20  /* or SERIAL_CTRL_W >> 8 */
/* Default value of tx_ctrl register: has txd(bit 7)=1 (idle) as default */
#define DEF_TX 0x80  /* or SERIAL_CTRL_B */

/* offsets from R_SERIALx_CTRL */

#define REG_DATA 0
#define REG_DATA_STATUS32 0 /* this is the 32 bit register R_SERIALx_READ */
#define REG_TR_DATA 0
#define REG_STATUS 1
#define REG_TR_CTRL 1
#define REG_REC_CTRL 2
#define REG_BAUD 3
#define REG_XOFF 4  /* this is a 32 bit register */

/* The bitfields are the same for all serial ports */
#define SER_RXD_MASK         IO_MASK(R_SERIAL0_STATUS, rxd)
#define SER_DATA_AVAIL_MASK  IO_MASK(R_SERIAL0_STATUS, data_avail)
#define SER_FRAMING_ERR_MASK IO_MASK(R_SERIAL0_STATUS, framing_err)
#define SER_PAR_ERR_MASK     IO_MASK(R_SERIAL0_STATUS, par_err)
#define SER_OVERRUN_MASK     IO_MASK(R_SERIAL0_STATUS, overrun)

#define SER_ERROR_MASK (SER_OVERRUN_MASK | SER_PAR_ERR_MASK | SER_FRAMING_ERR_MASK)

/* Values for info->errorcode */
#define ERRCODE_SET_BREAK    (TTY_BREAK)
#define ERRCODE_INSERT        0x100
#define ERRCODE_INSERT_BREAK (ERRCODE_INSERT | TTY_BREAK)

#define FORCE_EOP(info)  *R_SET_EOP = 1U << info->iseteop;

/*
 * General note regarding the use of IO_* macros in this file:
 *
 * We will use the bits defined for DMA channel 6 when using various
 * IO_* macros (e.g. IO_STATE, IO_MASK, IO_EXTRACT) and _assume_ they are
 * the same for all channels (which of course they are).
 *
 * We will also use the bits defined for serial port 0 when writing commands
 * to the different ports, as these bits too are the same for all ports.
 */


/* Mask for the irqs possibly enabled in R_IRQ_MASK1_RD etc. */
static const unsigned long e100_ser_int_mask = 0
#ifdef CONFIG_ETRAX_SERIAL_PORT0
| IO_MASK(R_IRQ_MASK1_RD, ser0_data) | IO_MASK(R_IRQ_MASK1_RD, ser0_ready)
#endif
#ifdef CONFIG_ETRAX_SERIAL_PORT1
| IO_MASK(R_IRQ_MASK1_RD, ser1_data) | IO_MASK(R_IRQ_MASK1_RD, ser1_ready)
#endif
#ifdef CONFIG_ETRAX_SERIAL_PORT2
| IO_MASK(R_IRQ_MASK1_RD, ser2_data) | IO_MASK(R_IRQ_MASK1_RD, ser2_ready)
#endif
#ifdef CONFIG_ETRAX_SERIAL_PORT3
| IO_MASK(R_IRQ_MASK1_RD, ser3_data) | IO_MASK(R_IRQ_MASK1_RD, ser3_ready)
#endif
;
unsigned long r_alt_ser_baudrate_shadow = 0;

/* this is the data for the four serial ports in the etrax100 */
/*  DMA2(ser2), DMA4(ser3), DMA6(ser0) or DMA8(ser1) */
/* R_DMA_CHx_CLR_INTR, R_DMA_CHx_FIRST, R_DMA_CHx_CMD */

static struct e100_serial rs_table[] = {
	{ .baud        = DEF_BAUD,
	  .ioport        = (unsigned char *)R_SERIAL0_CTRL,
	  .irq         = 1U << 12, /* uses DMA 6 and 7 */
	  .oclrintradr = R_DMA_CH6_CLR_INTR,
	  .ofirstadr   = R_DMA_CH6_FIRST,
	  .ocmdadr     = R_DMA_CH6_CMD,
	  .ostatusadr  = R_DMA_CH6_STATUS,
	  .iclrintradr = R_DMA_CH7_CLR_INTR,
	  .ifirstadr   = R_DMA_CH7_FIRST,
	  .icmdadr     = R_DMA_CH7_CMD,
	  .idescradr   = R_DMA_CH7_DESCR,
	  .flags       = STD_FLAGS,
	  .rx_ctrl     = DEF_RX,
	  .tx_ctrl     = DEF_TX,
	  .iseteop     = 2,
	  .dma_owner   = dma_ser0,
	  .io_if       = if_serial_0,
#ifdef CONFIG_ETRAX_SERIAL_PORT0
          .enabled  = 1,
#ifdef CONFIG_ETRAX_SERIAL_PORT0_DMA6_OUT
	  .dma_out_enabled = 1,
	  .dma_out_nbr = SER0_TX_DMA_NBR,
	  .dma_out_irq_nbr = SER0_DMA_TX_IRQ_NBR,
	  .dma_out_irq_flags = IRQF_DISABLED,
	  .dma_out_irq_description = "serial 0 dma tr",
#else
	  .dma_out_enabled = 0,
	  .dma_out_nbr = UINT_MAX,
	  .dma_out_irq_nbr = 0,
	  .dma_out_irq_flags = 0,
	  .dma_out_irq_description = NULL,
#endif
#ifdef CONFIG_ETRAX_SERIAL_PORT0_DMA7_IN
	  .dma_in_enabled = 1,
	  .dma_in_nbr = SER0_RX_DMA_NBR,
	  .dma_in_irq_nbr = SER0_DMA_RX_IRQ_NBR,
	  .dma_in_irq_flags = IRQF_DISABLED,
	  .dma_in_irq_description = "serial 0 dma rec",
#else
	  .dma_in_enabled = 0,
	  .dma_in_nbr = UINT_MAX,
	  .dma_in_irq_nbr = 0,
	  .dma_in_irq_flags = 0,
	  .dma_in_irq_description = NULL,
#endif
#else
          .enabled  = 0,
	  .io_if_description = NULL,
	  .dma_out_enabled = 0,
	  .dma_in_enabled = 0
#endif

},  /* ttyS0 */
#ifndef CONFIG_SVINTO_SIM
	{ .baud        = DEF_BAUD,
	  .ioport        = (unsigned char *)R_SERIAL1_CTRL,
	  .irq         = 1U << 16, /* uses DMA 8 and 9 */
	  .oclrintradr = R_DMA_CH8_CLR_INTR,
	  .ofirstadr   = R_DMA_CH8_FIRST,
	  .ocmdadr     = R_DMA_CH8_CMD,
	  .ostatusadr  = R_DMA_CH8_STATUS,
	  .iclrintradr = R_DMA_CH9_CLR_INTR,
	  .ifirstadr   = R_DMA_CH9_FIRST,
	  .icmdadr     = R_DMA_CH9_CMD,
	  .idescradr   = R_DMA_CH9_DESCR,
	  .flags       = STD_FLAGS,
	  .rx_ctrl     = DEF_RX,
	  .tx_ctrl     = DEF_TX,
	  .iseteop     = 3,
	  .dma_owner   = dma_ser1,
	  .io_if       = if_serial_1,
#ifdef CONFIG_ETRAX_SERIAL_PORT1
          .enabled  = 1,
	  .io_if_description = "ser1",
#ifdef CONFIG_ETRAX_SERIAL_PORT1_DMA8_OUT
	  .dma_out_enabled = 1,
	  .dma_out_nbr = SER1_TX_DMA_NBR,
	  .dma_out_irq_nbr = SER1_DMA_TX_IRQ_NBR,
	  .dma_out_irq_flags = IRQF_DISABLED,
	  .dma_out_irq_description = "serial 1 dma tr",
#else
	  .dma_out_enabled = 0,
	  .dma_out_nbr = UINT_MAX,
	  .dma_out_irq_nbr = 0,
	  .dma_out_irq_flags = 0,
	  .dma_out_irq_description = NULL,
#endif
#ifdef CONFIG_ETRAX_SERIAL_PORT1_DMA9_IN
	  .dma_in_enabled = 1,
	  .dma_in_nbr = SER1_RX_DMA_NBR,
	  .dma_in_irq_nbr = SER1_DMA_RX_IRQ_NBR,
	  .dma_in_irq_flags = IRQF_DISABLED,
	  .dma_in_irq_description = "serial 1 dma rec",
#else
	  .dma_in_enabled = 0,
	  .dma_in_enabled = 0,
	  .dma_in_nbr = UINT_MAX,
	  .dma_in_irq_nbr = 0,
	  .dma_in_irq_flags = 0,
	  .dma_in_irq_description = NULL,
#endif
#else
          .enabled  = 0,
	  .io_if_description = NULL,
	  .dma_in_irq_nbr = 0,
	  .dma_out_enabled = 0,
	  .dma_in_enabled = 0
#endif
},  /* ttyS1 */

	{ .baud        = DEF_BAUD,
	  .ioport        = (unsigned char *)R_SERIAL2_CTRL,
	  .irq         = 1U << 4,  /* uses DMA 2 and 3 */
	  .oclrintradr = R_DMA_CH2_CLR_INTR,
	  .ofirstadr   = R_DMA_CH2_FIRST,
	  .ocmdadr     = R_DMA_CH2_CMD,
	  .ostatusadr  = R_DMA_CH2_STATUS,
	  .iclrintradr = R_DMA_CH3_CLR_INTR,
	  .ifirstadr   = R_DMA_CH3_FIRST,
	  .icmdadr     = R_DMA_CH3_CMD,
	  .idescradr   = R_DMA_CH3_DESCR,
	  .flags       = STD_FLAGS,
	  .rx_ctrl     = DEF_RX,
	  .tx_ctrl     = DEF_TX,
	  .iseteop     = 0,
	  .dma_owner   = dma_ser2,
	  .io_if       = if_serial_2,
#ifdef CONFIG_ETRAX_SERIAL_PORT2
          .enabled  = 1,
	  .io_if_description = "ser2",
#ifdef CONFIG_ETRAX_SERIAL_PORT2_DMA2_OUT
	  .dma_out_enabled = 1,
	  .dma_out_nbr = SER2_TX_DMA_NBR,
	  .dma_out_irq_nbr = SER2_DMA_TX_IRQ_NBR,
	  .dma_out_irq_flags = IRQF_DISABLED,
	  .dma_out_irq_description = "serial 2 dma tr",
#else
	  .dma_out_enabled = 0,
	  .dma_out_nbr = UINT_MAX,
	  .dma_out_irq_nbr = 0,
	  .dma_out_irq_flags = 0,
	  .dma_out_irq_description = NULL,
#endif
#ifdef CONFIG_ETRAX_SERIAL_PORT2_DMA3_IN
	  .dma_in_enabled = 1,
	  .dma_in_nbr = SER2_RX_DMA_NBR,
	  .dma_in_irq_nbr = SER2_DMA_RX_IRQ_NBR,
	  .dma_in_irq_flags = IRQF_DISABLED,
	  .dma_in_irq_description = "serial 2 dma rec",
#else
	  .dma_in_enabled = 0,
	  .dma_in_nbr = UINT_MAX,
	  .dma_in_irq_nbr = 0,
	  .dma_in_irq_flags = 0,
	  .dma_in_irq_description = NULL,
#endif
#else
          .enabled  = 0,
	  .io_if_description = NULL,
	  .dma_out_enabled = 0,
	  .dma_in_enabled = 0
#endif
 },  /* ttyS2 */

	{ .baud        = DEF_BAUD,
	  .ioport        = (unsigned char *)R_SERIAL3_CTRL,
	  .irq         = 1U << 8,  /* uses DMA 4 and 5 */
	  .oclrintradr = R_DMA_CH4_CLR_INTR,
	  .ofirstadr   = R_DMA_CH4_FIRST,
	  .ocmdadr     = R_DMA_CH4_CMD,
	  .ostatusadr  = R_DMA_CH4_STATUS,
	  .iclrintradr = R_DMA_CH5_CLR_INTR,
	  .ifirstadr   = R_DMA_CH5_FIRST,
	  .icmdadr     = R_DMA_CH5_CMD,
	  .idescradr   = R_DMA_CH5_DESCR,
	  .flags       = STD_FLAGS,
	  .rx_ctrl     = DEF_RX,
	  .tx_ctrl     = DEF_TX,
	  .iseteop     = 1,
	  .dma_owner   = dma_ser3,
	  .io_if       = if_serial_3,
#ifdef CONFIG_ETRAX_SERIAL_PORT3
          .enabled  = 1,
	  .io_if_description = "ser3",
#ifdef CONFIG_ETRAX_SERIAL_PORT3_DMA4_OUT
	  .dma_out_enabled = 1,
	  .dma_out_nbr = SER3_TX_DMA_NBR,
	  .dma_out_irq_nbr = SER3_DMA_TX_IRQ_NBR,
	  .dma_out_irq_flags = IRQF_DISABLED,
	  .dma_out_irq_description = "serial 3 dma tr",
#else
	  .dma_out_enabled = 0,
	  .dma_out_nbr = UINT_MAX,
	  .dma_out_irq_nbr = 0,
	  .dma_out_irq_flags = 0,
	  .dma_out_irq_description = NULL,
#endif
#ifdef CONFIG_ETRAX_SERIAL_PORT3_DMA5_IN
	  .dma_in_enabled = 1,
	  .dma_in_nbr = SER3_RX_DMA_NBR,
	  .dma_in_irq_nbr = SER3_DMA_RX_IRQ_NBR,
	  .dma_in_irq_flags = IRQF_DISABLED,
	  .dma_in_irq_description = "serial 3 dma rec",
#else
	  .dma_in_enabled = 0,
	  .dma_in_nbr = UINT_MAX,
	  .dma_in_irq_nbr = 0,
	  .dma_in_irq_flags = 0,
	  .dma_in_irq_description = NULL
#endif
#else
          .enabled  = 0,
	  .io_if_description = NULL,
	  .dma_out_enabled = 0,
	  .dma_in_enabled = 0
#endif
 }   /* ttyS3 */
#endif
};


#define NR_PORTS (sizeof(rs_table)/sizeof(struct e100_serial))

#ifdef CONFIG_ETRAX_SERIAL_FAST_TIMER
static struct fast_timer fast_timers[NR_PORTS];
#endif

#ifdef CONFIG_ETRAX_SERIAL_PROC_ENTRY
#define PROCSTAT(x) x
struct ser_statistics_type {
	int overrun_cnt;
	int early_errors_cnt;
	int ser_ints_ok_cnt;
	int errors_cnt;
	unsigned long int processing_flip;
	unsigned long processing_flip_still_room;
	unsigned long int timeout_flush_cnt;
	int rx_dma_ints;
	int tx_dma_ints;
	int rx_tot;
	int tx_tot;
};

static struct ser_statistics_type ser_stat[NR_PORTS];

#else

#define PROCSTAT(x)

#endif /* CONFIG_ETRAX_SERIAL_PROC_ENTRY */

/* RS-485 */
#if defined(CONFIG_ETRAX_RS485)
#ifdef CONFIG_ETRAX_FAST_TIMER
static struct fast_timer fast_timers_rs485[NR_PORTS];
#endif
#if defined(CONFIG_ETRAX_RS485_ON_PA)
static int rs485_pa_bit = CONFIG_ETRAX_RS485_ON_PA_BIT;
#endif
#if defined(CONFIG_ETRAX_RS485_ON_PORT_G)
static int rs485_port_g_bit = CONFIG_ETRAX_RS485_ON_PORT_G_BIT;
#endif
#endif

/* Info and macros needed for each ports extra control/status signals. */
#define E100_STRUCT_PORT(line, pinname) \
 ((CONFIG_ETRAX_SER##line##_##pinname##_ON_PA_BIT >= 0)? \
		(R_PORT_PA_DATA): ( \
 (CONFIG_ETRAX_SER##line##_##pinname##_ON_PB_BIT >= 0)? \
		(R_PORT_PB_DATA):&dummy_ser[line]))

#define E100_STRUCT_SHADOW(line, pinname) \
 ((CONFIG_ETRAX_SER##line##_##pinname##_ON_PA_BIT >= 0)? \
		(&port_pa_data_shadow): ( \
 (CONFIG_ETRAX_SER##line##_##pinname##_ON_PB_BIT >= 0)? \
		(&port_pb_data_shadow):&dummy_ser[line]))
#define E100_STRUCT_MASK(line, pinname) \
 ((CONFIG_ETRAX_SER##line##_##pinname##_ON_PA_BIT >= 0)? \
		(1<<CONFIG_ETRAX_SER##line##_##pinname##_ON_PA_BIT): ( \
 (CONFIG_ETRAX_SER##line##_##pinname##_ON_PB_BIT >= 0)? \
		(1<<CONFIG_ETRAX_SER##line##_##pinname##_ON_PB_BIT):DUMMY_##pinname##_MASK))

#define DUMMY_DTR_MASK 1
#define DUMMY_RI_MASK  2
#define DUMMY_DSR_MASK 4
#define DUMMY_CD_MASK  8
static unsigned char dummy_ser[NR_PORTS] = {0xFF, 0xFF, 0xFF,0xFF};

/* If not all status pins are used or disabled, use mixed mode */
#ifdef CONFIG_ETRAX_SERIAL_PORT0

#define SER0_PA_BITSUM (CONFIG_ETRAX_SER0_DTR_ON_PA_BIT+CONFIG_ETRAX_SER0_RI_ON_PA_BIT+CONFIG_ETRAX_SER0_DSR_ON_PA_BIT+CONFIG_ETRAX_SER0_CD_ON_PA_BIT)

#if SER0_PA_BITSUM != -4
#  if CONFIG_ETRAX_SER0_DTR_ON_PA_BIT == -1
#    ifndef CONFIG_ETRAX_SER0_DTR_RI_DSR_CD_MIXED
#      define CONFIG_ETRAX_SER0_DTR_RI_DSR_CD_MIXED 1
#    endif
#   endif
# if CONFIG_ETRAX_SER0_RI_ON_PA_BIT == -1
#   ifndef CONFIG_ETRAX_SER0_DTR_RI_DSR_CD_MIXED
#     define CONFIG_ETRAX_SER0_DTR_RI_DSR_CD_MIXED 1
#   endif
#  endif
#  if CONFIG_ETRAX_SER0_DSR_ON_PA_BIT == -1
#    ifndef CONFIG_ETRAX_SER0_DTR_RI_DSR_CD_MIXED
#      define CONFIG_ETRAX_SER0_DTR_RI_DSR_CD_MIXED 1
#    endif
#  endif
#  if CONFIG_ETRAX_SER0_CD_ON_PA_BIT == -1
#    ifndef CONFIG_ETRAX_SER0_DTR_RI_DSR_CD_MIXED
#      define CONFIG_ETRAX_SER0_DTR_RI_DSR_CD_MIXED 1
#    endif
#  endif
#endif

#define SER0_PB_BITSUM (CONFIG_ETRAX_SER0_DTR_ON_PB_BIT+CONFIG_ETRAX_SER0_RI_ON_PB_BIT+CONFIG_ETRAX_SER0_DSR_ON_PB_BIT+CONFIG_ETRAX_SER0_CD_ON_PB_BIT)

#if SER0_PB_BITSUM != -4
#  if CONFIG_ETRAX_SER0_DTR_ON_PB_BIT == -1
#    ifndef CONFIG_ETRAX_SER0_DTR_RI_DSR_CD_MIXED
#      define CONFIG_ETRAX_SER0_DTR_RI_DSR_CD_MIXED 1
#    endif
#   endif
# if CONFIG_ETRAX_SER0_RI_ON_PB_BIT == -1
#   ifndef CONFIG_ETRAX_SER0_DTR_RI_DSR_CD_MIXED
#     define CONFIG_ETRAX_SER0_DTR_RI_DSR_CD_MIXED 1
#   endif
#  endif
#  if CONFIG_ETRAX_SER0_DSR_ON_PB_BIT == -1
#    ifndef CONFIG_ETRAX_SER0_DTR_RI_DSR_CD_MIXED
#      define CONFIG_ETRAX_SER0_DTR_RI_DSR_CD_MIXED 1
#    endif
#  endif
#  if CONFIG_ETRAX_SER0_CD_ON_PB_BIT == -1
#    ifndef CONFIG_ETRAX_SER0_DTR_RI_DSR_CD_MIXED
#      define CONFIG_ETRAX_SER0_DTR_RI_DSR_CD_MIXED 1
#    endif
#  endif
#endif

#endif /* PORT0 */


#ifdef CONFIG_ETRAX_SERIAL_PORT1

#define SER1_PA_BITSUM (CONFIG_ETRAX_SER1_DTR_ON_PA_BIT+CONFIG_ETRAX_SER1_RI_ON_PA_BIT+CONFIG_ETRAX_SER1_DSR_ON_PA_BIT+CONFIG_ETRAX_SER1_CD_ON_PA_BIT)

#if SER1_PA_BITSUM != -4
#  if CONFIG_ETRAX_SER1_DTR_ON_PA_BIT == -1
#    ifndef CONFIG_ETRAX_SER1_DTR_RI_DSR_CD_MIXED
#      define CONFIG_ETRAX_SER1_DTR_RI_DSR_CD_MIXED 1
#    endif
#   endif
# if CONFIG_ETRAX_SER1_RI_ON_PA_BIT == -1
#   ifndef CONFIG_ETRAX_SER1_DTR_RI_DSR_CD_MIXED
#     define CONFIG_ETRAX_SER1_DTR_RI_DSR_CD_MIXED 1
#   endif
#  endif
#  if CONFIG_ETRAX_SER1_DSR_ON_PA_BIT == -1
#    ifndef CONFIG_ETRAX_SER1_DTR_RI_DSR_CD_MIXED
#      define CONFIG_ETRAX_SER1_DTR_RI_DSR_CD_MIXED 1
#    endif
#  endif
#  if CONFIG_ETRAX_SER1_CD_ON_PA_BIT == -1
#    ifndef CONFIG_ETRAX_SER1_DTR_RI_DSR_CD_MIXED
#      define CONFIG_ETRAX_SER1_DTR_RI_DSR_CD_MIXED 1
#    endif
#  endif
#endif

#define SER1_PB_BITSUM (CONFIG_ETRAX_SER1_DTR_ON_PB_BIT+CONFIG_ETRAX_SER1_RI_ON_PB_BIT+CONFIG_ETRAX_SER1_DSR_ON_PB_BIT+CONFIG_ETRAX_SER1_CD_ON_PB_BIT)

#if SER1_PB_BITSUM != -4
#  if CONFIG_ETRAX_SER1_DTR_ON_PB_BIT == -1
#    ifndef CONFIG_ETRAX_SER1_DTR_RI_DSR_CD_MIXED
#      define CONFIG_ETRAX_SER1_DTR_RI_DSR_CD_MIXED 1
#    endif
#   endif
# if CONFIG_ETRAX_SER1_RI_ON_PB_BIT == -1
#   ifndef CONFIG_ETRAX_SER1_DTR_RI_DSR_CD_MIXED
#     define CONFIG_ETRAX_SER1_DTR_RI_DSR_CD_MIXED 1
#   endif
#  endif
#  if CONFIG_ETRAX_SER1_DSR_ON_PB_BIT == -1
#    ifndef CONFIG_ETRAX_SER1_DTR_RI_DSR_CD_MIXED
#      define CONFIG_ETRAX_SER1_DTR_RI_DSR_CD_MIXED 1
#    endif
#  endif
#  if CONFIG_ETRAX_SER1_CD_ON_PB_BIT == -1
#    ifndef CONFIG_ETRAX_SER1_DTR_RI_DSR_CD_MIXED
#      define CONFIG_ETRAX_SER1_DTR_RI_DSR_CD_MIXED 1
#    endif
#  endif
#endif

#endif /* PORT1 */

#ifdef CONFIG_ETRAX_SERIAL_PORT2

#define SER2_PA_BITSUM (CONFIG_ETRAX_SER2_DTR_ON_PA_BIT+CONFIG_ETRAX_SER2_RI_ON_PA_BIT+CONFIG_ETRAX_SER2_DSR_ON_PA_BIT+CONFIG_ETRAX_SER2_CD_ON_PA_BIT)

#if SER2_PA_BITSUM != -4
#  if CONFIG_ETRAX_SER2_DTR_ON_PA_BIT == -1
#    ifndef CONFIG_ETRAX_SER2_DTR_RI_DSR_CD_MIXED
#      define CONFIG_ETRAX_SER2_DTR_RI_DSR_CD_MIXED 1
#    endif
#   endif
# if CONFIG_ETRAX_SER2_RI_ON_PA_BIT == -1
#   ifndef CONFIG_ETRAX_SER2_DTR_RI_DSR_CD_MIXED
#     define CONFIG_ETRAX_SER2_DTR_RI_DSR_CD_MIXED 1
#   endif
#  endif
#  if CONFIG_ETRAX_SER2_DSR_ON_PA_BIT == -1
#    ifndef CONFIG_ETRAX_SER2_DTR_RI_DSR_CD_MIXED
#      define CONFIG_ETRAX_SER2_DTR_RI_DSR_CD_MIXED 1
#    endif
#  endif
#  if CONFIG_ETRAX_SER2_CD_ON_PA_BIT == -1
#    ifndef CONFIG_ETRAX_SER2_DTR_RI_DSR_CD_MIXED
#      define CONFIG_ETRAX_SER2_DTR_RI_DSR_CD_MIXED 1
#    endif
#  endif
#endif

#define SER2_PB_BITSUM (CONFIG_ETRAX_SER2_DTR_ON_PB_BIT+CONFIG_ETRAX_SER2_RI_ON_PB_BIT+CONFIG_ETRAX_SER2_DSR_ON_PB_BIT+CONFIG_ETRAX_SER2_CD_ON_PB_BIT)

#if SER2_PB_BITSUM != -4
#  if CONFIG_ETRAX_SER2_DTR_ON_PB_BIT == -1
#    ifndef CONFIG_ETRAX_SER2_DTR_RI_DSR_CD_MIXED
#      define CONFIG_ETRAX_SER2_DTR_RI_DSR_CD_MIXED 1
#    endif
#   endif
# if CONFIG_ETRAX_SER2_RI_ON_PB_BIT == -1
#   ifndef CONFIG_ETRAX_SER2_DTR_RI_DSR_CD_MIXED
#     define CONFIG_ETRAX_SER2_DTR_RI_DSR_CD_MIXED 1
#   endif
#  endif
#  if CONFIG_ETRAX_SER2_DSR_ON_PB_BIT == -1
#    ifndef CONFIG_ETRAX_SER2_DTR_RI_DSR_CD_MIXED
#      define CONFIG_ETRAX_SER2_DTR_RI_DSR_CD_MIXED 1
#    endif
#  endif
#  if CONFIG_ETRAX_SER2_CD_ON_PB_BIT == -1
#    ifndef CONFIG_ETRAX_SER2_DTR_RI_DSR_CD_MIXED
#      define CONFIG_ETRAX_SER2_DTR_RI_DSR_CD_MIXED 1
#    endif
#  endif
#endif

#endif /* PORT2 */

#ifdef CONFIG_ETRAX_SERIAL_PORT3

#define SER3_PA_BITSUM (CONFIG_ETRAX_SER3_DTR_ON_PA_BIT+CONFIG_ETRAX_SER3_RI_ON_PA_BIT+CONFIG_ETRAX_SER3_DSR_ON_PA_BIT+CONFIG_ETRAX_SER3_CD_ON_PA_BIT)

#if SER3_PA_BITSUM != -4
#  if CONFIG_ETRAX_SER3_DTR_ON_PA_BIT == -1
#    ifndef CONFIG_ETRAX_SER3_DTR_RI_DSR_CD_MIXED
#      define CONFIG_ETRAX_SER3_DTR_RI_DSR_CD_MIXED 1
#    endif
#   endif
# if CONFIG_ETRAX_SER3_RI_ON_PA_BIT == -1
#   ifndef CONFIG_ETRAX_SER3_DTR_RI_DSR_CD_MIXED
#     define CONFIG_ETRAX_SER3_DTR_RI_DSR_CD_MIXED 1
#   endif
#  endif
#  if CONFIG_ETRAX_SER3_DSR_ON_PA_BIT == -1
#    ifndef CONFIG_ETRAX_SER3_DTR_RI_DSR_CD_MIXED
#      define CONFIG_ETRAX_SER3_DTR_RI_DSR_CD_MIXED 1
#    endif
#  endif
#  if CONFIG_ETRAX_SER3_CD_ON_PA_BIT == -1
#    ifndef CONFIG_ETRAX_SER3_DTR_RI_DSR_CD_MIXED
#      define CONFIG_ETRAX_SER3_DTR_RI_DSR_CD_MIXED 1
#    endif
#  endif
#endif

#define SER3_PB_BITSUM (CONFIG_ETRAX_SER3_DTR_ON_PB_BIT+CONFIG_ETRAX_SER3_RI_ON_PB_BIT+CONFIG_ETRAX_SER3_DSR_ON_PB_BIT+CONFIG_ETRAX_SER3_CD_ON_PB_BIT)

#if SER3_PB_BITSUM != -4
#  if CONFIG_ETRAX_SER3_DTR_ON_PB_BIT == -1
#    ifndef CONFIG_ETRAX_SER3_DTR_RI_DSR_CD_MIXED
#      define CONFIG_ETRAX_SER3_DTR_RI_DSR_CD_MIXED 1
#    endif
#   endif
# if CONFIG_ETRAX_SER3_RI_ON_PB_BIT == -1
#   ifndef CONFIG_ETRAX_SER3_DTR_RI_DSR_CD_MIXED
#     define CONFIG_ETRAX_SER3_DTR_RI_DSR_CD_MIXED 1
#   endif
#  endif
#  if CONFIG_ETRAX_SER3_DSR_ON_PB_BIT == -1
#    ifndef CONFIG_ETRAX_SER3_DTR_RI_DSR_CD_MIXED
#      define CONFIG_ETRAX_SER3_DTR_RI_DSR_CD_MIXED 1
#    endif
#  endif
#  if CONFIG_ETRAX_SER3_CD_ON_PB_BIT == -1
#    ifndef CONFIG_ETRAX_SER3_DTR_RI_DSR_CD_MIXED
#      define CONFIG_ETRAX_SER3_DTR_RI_DSR_CD_MIXED 1
#    endif
#  endif
#endif

#endif /* PORT3 */


#if defined(CONFIG_ETRAX_SER0_DTR_RI_DSR_CD_MIXED) || \
    defined(CONFIG_ETRAX_SER1_DTR_RI_DSR_CD_MIXED) || \
    defined(CONFIG_ETRAX_SER2_DTR_RI_DSR_CD_MIXED) || \
    defined(CONFIG_ETRAX_SER3_DTR_RI_DSR_CD_MIXED)
#define CONFIG_ETRAX_SERX_DTR_RI_DSR_CD_MIXED
#endif

#ifdef CONFIG_ETRAX_SERX_DTR_RI_DSR_CD_MIXED
/* The pins can be mixed on PA and PB */
#define CONTROL_PINS_PORT_NOT_USED(line) \
  &dummy_ser[line], &dummy_ser[line], \
  &dummy_ser[line], &dummy_ser[line], \
  &dummy_ser[line], &dummy_ser[line], \
  &dummy_ser[line], &dummy_ser[line], \
  DUMMY_DTR_MASK, DUMMY_RI_MASK, DUMMY_DSR_MASK, DUMMY_CD_MASK


struct control_pins
{
	volatile unsigned char *dtr_port;
	unsigned char          *dtr_shadow;
	volatile unsigned char *ri_port;
	unsigned char          *ri_shadow;
	volatile unsigned char *dsr_port;
	unsigned char          *dsr_shadow;
	volatile unsigned char *cd_port;
	unsigned char          *cd_shadow;

	unsigned char dtr_mask;
	unsigned char ri_mask;
	unsigned char dsr_mask;
	unsigned char cd_mask;
};

static const struct control_pins e100_modem_pins[NR_PORTS] =
{
	/* Ser 0 */
	{
#ifdef CONFIG_ETRAX_SERIAL_PORT0
	E100_STRUCT_PORT(0,DTR), E100_STRUCT_SHADOW(0,DTR),
	E100_STRUCT_PORT(0,RI),  E100_STRUCT_SHADOW(0,RI),
	E100_STRUCT_PORT(0,DSR), E100_STRUCT_SHADOW(0,DSR),
	E100_STRUCT_PORT(0,CD),  E100_STRUCT_SHADOW(0,CD),
	E100_STRUCT_MASK(0,DTR),
	E100_STRUCT_MASK(0,RI),
	E100_STRUCT_MASK(0,DSR),
	E100_STRUCT_MASK(0,CD)
#else
	CONTROL_PINS_PORT_NOT_USED(0)
#endif
	},

	/* Ser 1 */
	{
#ifdef CONFIG_ETRAX_SERIAL_PORT1
	E100_STRUCT_PORT(1,DTR), E100_STRUCT_SHADOW(1,DTR),
	E100_STRUCT_PORT(1,RI),  E100_STRUCT_SHADOW(1,RI),
	E100_STRUCT_PORT(1,DSR), E100_STRUCT_SHADOW(1,DSR),
	E100_STRUCT_PORT(1,CD),  E100_STRUCT_SHADOW(1,CD),
	E100_STRUCT_MASK(1,DTR),
	E100_STRUCT_MASK(1,RI),
	E100_STRUCT_MASK(1,DSR),
	E100_STRUCT_MASK(1,CD)
#else
	CONTROL_PINS_PORT_NOT_USED(1)
#endif
	},

	/* Ser 2 */
	{
#ifdef CONFIG_ETRAX_SERIAL_PORT2
	E100_STRUCT_PORT(2,DTR), E100_STRUCT_SHADOW(2,DTR),
	E100_STRUCT_PORT(2,RI),  E100_STRUCT_SHADOW(2,RI),
	E100_STRUCT_PORT(2,DSR), E100_STRUCT_SHADOW(2,DSR),
	E100_STRUCT_PORT(2,CD),  E100_STRUCT_SHADOW(2,CD),
	E100_STRUCT_MASK(2,DTR),
	E100_STRUCT_MASK(2,RI),
	E100_STRUCT_MASK(2,DSR),
	E100_STRUCT_MASK(2,CD)
#else
	CONTROL_PINS_PORT_NOT_USED(2)
#endif
	},

	/* Ser 3 */
	{
#ifdef CONFIG_ETRAX_SERIAL_PORT3
	E100_STRUCT_PORT(3,DTR), E100_STRUCT_SHADOW(3,DTR),
	E100_STRUCT_PORT(3,RI),  E100_STRUCT_SHADOW(3,RI),
	E100_STRUCT_PORT(3,DSR), E100_STRUCT_SHADOW(3,DSR),
	E100_STRUCT_PORT(3,CD),  E100_STRUCT_SHADOW(3,CD),
	E100_STRUCT_MASK(3,DTR),
	E100_STRUCT_MASK(3,RI),
	E100_STRUCT_MASK(3,DSR),
	E100_STRUCT_MASK(3,CD)
#else
	CONTROL_PINS_PORT_NOT_USED(3)
#endif
	}
};
#else  /* CONFIG_ETRAX_SERX_DTR_RI_DSR_CD_MIXED */

/* All pins are on either PA or PB for each serial port */
#define CONTROL_PINS_PORT_NOT_USED(line) \
  &dummy_ser[line], &dummy_ser[line], \
  DUMMY_DTR_MASK, DUMMY_RI_MASK, DUMMY_DSR_MASK, DUMMY_CD_MASK


struct control_pins
{
	volatile unsigned char *port;
	unsigned char          *shadow;

	unsigned char dtr_mask;
	unsigned char ri_mask;
	unsigned char dsr_mask;
	unsigned char cd_mask;
};

#define dtr_port port
#define dtr_shadow shadow
#define ri_port port
#define ri_shadow shadow
#define dsr_port port
#define dsr_shadow shadow
#define cd_port port
#define cd_shadow shadow

static const struct control_pins e100_modem_pins[NR_PORTS] =
{
	/* Ser 0 */
	{
#ifdef CONFIG_ETRAX_SERIAL_PORT0
	E100_STRUCT_PORT(0,DTR), E100_STRUCT_SHADOW(0,DTR),
	E100_STRUCT_MASK(0,DTR),
	E100_STRUCT_MASK(0,RI),
	E100_STRUCT_MASK(0,DSR),
	E100_STRUCT_MASK(0,CD)
#else
	CONTROL_PINS_PORT_NOT_USED(0)
#endif
	},

	/* Ser 1 */
	{
#ifdef CONFIG_ETRAX_SERIAL_PORT1
	E100_STRUCT_PORT(1,DTR), E100_STRUCT_SHADOW(1,DTR),
	E100_STRUCT_MASK(1,DTR),
	E100_STRUCT_MASK(1,RI),
	E100_STRUCT_MASK(1,DSR),
	E100_STRUCT_MASK(1,CD)
#else
	CONTROL_PINS_PORT_NOT_USED(1)
#endif
	},

	/* Ser 2 */
	{
#ifdef CONFIG_ETRAX_SERIAL_PORT2
	E100_STRUCT_PORT(2,DTR), E100_STRUCT_SHADOW(2,DTR),
	E100_STRUCT_MASK(2,DTR),
	E100_STRUCT_MASK(2,RI),
	E100_STRUCT_MASK(2,DSR),
	E100_STRUCT_MASK(2,CD)
#else
	CONTROL_PINS_PORT_NOT_USED(2)
#endif
	},

	/* Ser 3 */
	{
#ifdef CONFIG_ETRAX_SERIAL_PORT3
	E100_STRUCT_PORT(3,DTR), E100_STRUCT_SHADOW(3,DTR),
	E100_STRUCT_MASK(3,DTR),
	E100_STRUCT_MASK(3,RI),
	E100_STRUCT_MASK(3,DSR),
	E100_STRUCT_MASK(3,CD)
#else
	CONTROL_PINS_PORT_NOT_USED(3)
#endif
	}
};
#endif /* !CONFIG_ETRAX_SERX_DTR_RI_DSR_CD_MIXED */

#define E100_RTS_MASK 0x20
#define E100_CTS_MASK 0x40

/* All serial port signals are active low:
 * active   = 0 -> 3.3V to RS-232 driver -> -12V on RS-232 level
 * inactive = 1 -> 0V   to RS-232 driver -> +12V on RS-232 level
 *
 * These macros returns the pin value: 0=0V, >=1 = 3.3V on ETRAX chip
 */

/* Output */
#define E100_RTS_GET(info) ((info)->rx_ctrl & E100_RTS_MASK)
/* Input */
#define E100_CTS_GET(info) ((info)->ioport[REG_STATUS] & E100_CTS_MASK)

/* These are typically PA or PB and 0 means 0V, 1 means 3.3V */
/* Is an output */
#define E100_DTR_GET(info) ((*e100_modem_pins[(info)->line].dtr_shadow) & e100_modem_pins[(info)->line].dtr_mask)

/* Normally inputs */
#define E100_RI_GET(info) ((*e100_modem_pins[(info)->line].ri_port) & e100_modem_pins[(info)->line].ri_mask)
#define E100_CD_GET(info) ((*e100_modem_pins[(info)->line].cd_port) & e100_modem_pins[(info)->line].cd_mask)

/* Input */
#define E100_DSR_GET(info) ((*e100_modem_pins[(info)->line].dsr_port) & e100_modem_pins[(info)->line].dsr_mask)


/*
 * tmp_buf is used as a temporary buffer by serial_write.  We need to
 * lock it in case the memcpy_fromfs blocks while swapping in a page,
 * and some other program tries to do a serial write at the same time.
 * Since the lock will only come under contention when the system is
 * swapping and available memory is low, it makes sense to share one
 * buffer across all the serial ports, since it significantly saves
 * memory if large numbers of serial ports are open.
 */
static unsigned char *tmp_buf;
static DEFINE_MUTEX(tmp_buf_mutex);

/* Calculate the chartime depending on baudrate, numbor of bits etc. */
static void update_char_time(struct e100_serial * info)
{
	tcflag_t cflags = info->port.tty->termios->c_cflag;
	int bits;

	/* calc. number of bits / data byte */
	/* databits + startbit and 1 stopbit */
	if ((cflags & CSIZE) == CS7)
		bits = 9;
	else
		bits = 10;

	if (cflags & CSTOPB)     /* 2 stopbits ? */
		bits++;

	if (cflags & PARENB)     /* parity bit ? */
		bits++;

	/* calc timeout */
	info->char_time_usec = ((bits * 1000000) / info->baud) + 1;
	info->flush_time_usec = 4*info->char_time_usec;
	if (info->flush_time_usec < MIN_FLUSH_TIME_USEC)
		info->flush_time_usec = MIN_FLUSH_TIME_USEC;

}

/*
 * This function maps from the Bxxxx defines in asm/termbits.h into real
 * baud rates.
 */

static int
cflag_to_baud(unsigned int cflag)
{
	static int baud_table[] = {
		0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400,
		4800, 9600, 19200, 38400 };

	static int ext_baud_table[] = {
		0, 57600, 115200, 230400, 460800, 921600, 1843200, 6250000,
                0, 0, 0, 0, 0, 0, 0, 0 };

	if (cflag & CBAUDEX)
		return ext_baud_table[(cflag & CBAUD) & ~CBAUDEX];
	else
		return baud_table[cflag & CBAUD];
}

/* and this maps to an etrax100 hardware baud constant */

static unsigned char
cflag_to_etrax_baud(unsigned int cflag)
{
	char retval;

	static char baud_table[] = {
		-1, -1, -1, -1, -1, -1, -1, 0, 1, 2, -1, 3, 4, 5, 6, 7 };

	static char ext_baud_table[] = {
		-1, 8, 9, 10, 11, 12, 13, 14, -1, -1, -1, -1, -1, -1, -1, -1 };

	if (cflag & CBAUDEX)
		retval = ext_baud_table[(cflag & CBAUD) & ~CBAUDEX];
	else
		retval = baud_table[cflag & CBAUD];

	if (retval < 0) {
		printk(KERN_WARNING "serdriver tried setting invalid baud rate, flags %x.\n", cflag);
		retval = 5; /* choose default 9600 instead */
	}

	return retval | (retval << 4); /* choose same for both TX and RX */
}


/* Various static support functions */

/* Functions to set or clear DTR/RTS on the requested line */
/* It is complicated by the fact that RTS is a serial port register, while
 * DTR might not be implemented in the HW at all, and if it is, it can be on
 * any general port.
 */


static inline void
e100_dtr(struct e100_serial *info, int set)
{
#ifndef CONFIG_SVINTO_SIM
	unsigned char mask = e100_modem_pins[info->line].dtr_mask;

#ifdef SERIAL_DEBUG_IO
	printk("ser%i dtr %i mask: 0x%02X\n", info->line, set, mask);
	printk("ser%i shadow before 0x%02X get: %i\n",
	       info->line, *e100_modem_pins[info->line].dtr_shadow,
	       E100_DTR_GET(info));
#endif
	/* DTR is active low */
	{
		unsigned long flags;

		local_irq_save(flags);
		*e100_modem_pins[info->line].dtr_shadow &= ~mask;
		*e100_modem_pins[info->line].dtr_shadow |= (set ? 0 : mask);
		*e100_modem_pins[info->line].dtr_port = *e100_modem_pins[info->line].dtr_shadow;
		local_irq_restore(flags);
	}

#ifdef SERIAL_DEBUG_IO
	printk("ser%i shadow after 0x%02X get: %i\n",
	       info->line, *e100_modem_pins[info->line].dtr_shadow,
	       E100_DTR_GET(info));
#endif
#endif
}

/* set = 0 means 3.3V on the pin, bitvalue: 0=active, 1=inactive
 *                                          0=0V    , 1=3.3V
 */
static inline void
e100_rts(struct e100_serial *info, int set)
{
#ifndef CONFIG_SVINTO_SIM
	unsigned long flags;
	local_irq_save(flags);
	info->rx_ctrl &= ~E100_RTS_MASK;
	info->rx_ctrl |= (set ? 0 : E100_RTS_MASK);  /* RTS is active low */
	info->ioport[REG_REC_CTRL] = info->rx_ctrl;
	local_irq_restore(flags);
#ifdef SERIAL_DEBUG_IO
	printk("ser%i rts %i\n", info->line, set);
#endif
#endif
}


/* If this behaves as a modem, RI and CD is an output */
static inline void
e100_ri_out(struct e100_serial *info, int set)
{
#ifndef CONFIG_SVINTO_SIM
	/* RI is active low */
	{
		unsigned char mask = e100_modem_pins[info->line].ri_mask;
		unsigned long flags;

		local_irq_save(flags);
		*e100_modem_pins[info->line].ri_shadow &= ~mask;
		*e100_modem_pins[info->line].ri_shadow |= (set ? 0 : mask);
		*e100_modem_pins[info->line].ri_port = *e100_modem_pins[info->line].ri_shadow;
		local_irq_restore(flags);
	}
#endif
}
static inline void
e100_cd_out(struct e100_serial *info, int set)
{
#ifndef CONFIG_SVINTO_SIM
	/* CD is active low */
	{
		unsigned char mask = e100_modem_pins[info->line].cd_mask;
		unsigned long flags;

		local_irq_save(flags);
		*e100_modem_pins[info->line].cd_shadow &= ~mask;
		*e100_modem_pins[info->line].cd_shadow |= (set ? 0 : mask);
		*e100_modem_pins[info->line].cd_port = *e100_modem_pins[info->line].cd_shadow;
		local_irq_restore(flags);
	}
#endif
}

static inline void
e100_disable_rx(struct e100_serial *info)
{
#ifndef CONFIG_SVINTO_SIM
	/* disable the receiver */
	info->ioport[REG_REC_CTRL] =
		(info->rx_ctrl &= ~IO_MASK(R_SERIAL0_REC_CTRL, rec_enable));
#endif
}

static inline void
e100_enable_rx(struct e100_serial *info)
{
#ifndef CONFIG_SVINTO_SIM
	/* enable the receiver */
	info->ioport[REG_REC_CTRL] =
		(info->rx_ctrl |= IO_MASK(R_SERIAL0_REC_CTRL, rec_enable));
#endif
}

/* the rx DMA uses both the dma_descr and the dma_eop interrupts */

static inline void
e100_disable_rxdma_irq(struct e100_serial *info)
{
#ifdef SERIAL_DEBUG_INTR
	printk("rxdma_irq(%d): 0\n",info->line);
#endif
	DINTR1(DEBUG_LOG(info->line,"IRQ disable_rxdma_irq %i\n", info->line));
	*R_IRQ_MASK2_CLR = (info->irq << 2) | (info->irq << 3);
}

static inline void
e100_enable_rxdma_irq(struct e100_serial *info)
{
#ifdef SERIAL_DEBUG_INTR
	printk("rxdma_irq(%d): 1\n",info->line);
#endif
	DINTR1(DEBUG_LOG(info->line,"IRQ enable_rxdma_irq %i\n", info->line));
	*R_IRQ_MASK2_SET = (info->irq << 2) | (info->irq << 3);
}

/* the tx DMA uses only dma_descr interrupt */

static void e100_disable_txdma_irq(struct e100_serial *info)
{
#ifdef SERIAL_DEBUG_INTR
	printk("txdma_irq(%d): 0\n",info->line);
#endif
	DINTR1(DEBUG_LOG(info->line,"IRQ disable_txdma_irq %i\n", info->line));
	*R_IRQ_MASK2_CLR = info->irq;
}

static void e100_enable_txdma_irq(struct e100_serial *info)
{
#ifdef SERIAL_DEBUG_INTR
	printk("txdma_irq(%d): 1\n",info->line);
#endif
	DINTR1(DEBUG_LOG(info->line,"IRQ enable_txdma_irq %i\n", info->line));
	*R_IRQ_MASK2_SET = info->irq;
}

static void e100_disable_txdma_channel(struct e100_serial *info)
{
	unsigned long flags;

	/* Disable output DMA channel for the serial port in question
	 * ( set to something other than serialX)
	 */
	local_irq_save(flags);
	DFLOW(DEBUG_LOG(info->line, "disable_txdma_channel %i\n", info->line));
	if (info->line == 0) {
		if ((genconfig_shadow & IO_MASK(R_GEN_CONFIG, dma6)) ==
		    IO_STATE(R_GEN_CONFIG, dma6, serial0)) {
			genconfig_shadow &=  ~IO_MASK(R_GEN_CONFIG, dma6);
			genconfig_shadow |= IO_STATE(R_GEN_CONFIG, dma6, unused);
		}
	} else if (info->line == 1) {
		if ((genconfig_shadow & IO_MASK(R_GEN_CONFIG, dma8)) ==
		    IO_STATE(R_GEN_CONFIG, dma8, serial1)) {
			genconfig_shadow &=  ~IO_MASK(R_GEN_CONFIG, dma8);
			genconfig_shadow |= IO_STATE(R_GEN_CONFIG, dma8, usb);
		}
	} else if (info->line == 2) {
		if ((genconfig_shadow & IO_MASK(R_GEN_CONFIG, dma2)) ==
		    IO_STATE(R_GEN_CONFIG, dma2, serial2)) {
			genconfig_shadow &=  ~IO_MASK(R_GEN_CONFIG, dma2);
			genconfig_shadow |= IO_STATE(R_GEN_CONFIG, dma2, par0);
		}
	} else if (info->line == 3) {
		if ((genconfig_shadow & IO_MASK(R_GEN_CONFIG, dma4)) ==
		    IO_STATE(R_GEN_CONFIG, dma4, serial3)) {
			genconfig_shadow &=  ~IO_MASK(R_GEN_CONFIG, dma4);
			genconfig_shadow |= IO_STATE(R_GEN_CONFIG, dma4, par1);
		}
	}
	*R_GEN_CONFIG = genconfig_shadow;
	local_irq_restore(flags);
}


static void e100_enable_txdma_channel(struct e100_serial *info)
{
	unsigned long flags;

	local_irq_save(flags);
	DFLOW(DEBUG_LOG(info->line, "enable_txdma_channel %i\n", info->line));
	/* Enable output DMA channel for the serial port in question */
	if (info->line == 0) {
		genconfig_shadow &=  ~IO_MASK(R_GEN_CONFIG, dma6);
		genconfig_shadow |= IO_STATE(R_GEN_CONFIG, dma6, serial0);
	} else if (info->line == 1) {
		genconfig_shadow &=  ~IO_MASK(R_GEN_CONFIG, dma8);
		genconfig_shadow |= IO_STATE(R_GEN_CONFIG, dma8, serial1);
	} else if (info->line == 2) {
		genconfig_shadow &=  ~IO_MASK(R_GEN_CONFIG, dma2);
		genconfig_shadow |= IO_STATE(R_GEN_CONFIG, dma2, serial2);
	} else if (info->line == 3) {
		genconfig_shadow &=  ~IO_MASK(R_GEN_CONFIG, dma4);
		genconfig_shadow |= IO_STATE(R_GEN_CONFIG, dma4, serial3);
	}
	*R_GEN_CONFIG = genconfig_shadow;
	local_irq_restore(flags);
}

static void e100_disable_rxdma_channel(struct e100_serial *info)
{
	unsigned long flags;

	/* Disable input DMA channel for the serial port in question
	 * ( set to something other than serialX)
	 */
	local_irq_save(flags);
	if (info->line == 0) {
		if ((genconfig_shadow & IO_MASK(R_GEN_CONFIG, dma7)) ==
		    IO_STATE(R_GEN_CONFIG, dma7, serial0)) {
			genconfig_shadow &=  ~IO_MASK(R_GEN_CONFIG, dma7);
			genconfig_shadow |= IO_STATE(R_GEN_CONFIG, dma7, unused);
		}
	} else if (info->line == 1) {
		if ((genconfig_shadow & IO_MASK(R_GEN_CONFIG, dma9)) ==
		    IO_STATE(R_GEN_CONFIG, dma9, serial1)) {
			genconfig_shadow &=  ~IO_MASK(R_GEN_CONFIG, dma9);
			genconfig_shadow |= IO_STATE(R_GEN_CONFIG, dma9, usb);
		}
	} else if (info->line == 2) {
		if ((genconfig_shadow & IO_MASK(R_GEN_CONFIG, dma3)) ==
		    IO_STATE(R_GEN_CONFIG, dma3, serial2)) {
			genconfig_shadow &=  ~IO_MASK(R_GEN_CONFIG, dma3);
			genconfig_shadow |= IO_STATE(R_GEN_CONFIG, dma3, par0);
		}
	} else if (info->line == 3) {
		if ((genconfig_shadow & IO_MASK(R_GEN_CONFIG, dma5)) ==
		    IO_STATE(R_GEN_CONFIG, dma5, serial3)) {
			genconfig_shadow &=  ~IO_MASK(R_GEN_CONFIG, dma5);
			genconfig_shadow |= IO_STATE(R_GEN_CONFIG, dma5, par1);
		}
	}
	*R_GEN_CONFIG = genconfig_shadow;
	local_irq_restore(flags);
}


static void e100_enable_rxdma_channel(struct e100_serial *info)
{
	unsigned long flags;

	local_irq_save(flags);
	/* Enable input DMA channel for the serial port in question */
	if (info->line == 0) {
		genconfig_shadow &=  ~IO_MASK(R_GEN_CONFIG, dma7);
		genconfig_shadow |= IO_STATE(R_GEN_CONFIG, dma7, serial0);
	} else if (info->line == 1) {
		genconfig_shadow &=  ~IO_MASK(R_GEN_CONFIG, dma9);
		genconfig_shadow |= IO_STATE(R_GEN_CONFIG, dma9, serial1);
	} else if (info->line == 2) {
		genconfig_shadow &=  ~IO_MASK(R_GEN_CONFIG, dma3);
		genconfig_shadow |= IO_STATE(R_GEN_CONFIG, dma3, serial2);
	} else if (info->line == 3) {
		genconfig_shadow &=  ~IO_MASK(R_GEN_CONFIG, dma5);
		genconfig_shadow |= IO_STATE(R_GEN_CONFIG, dma5, serial3);
	}
	*R_GEN_CONFIG = genconfig_shadow;
	local_irq_restore(flags);
}

#ifdef SERIAL_HANDLE_EARLY_ERRORS
/* in order to detect and fix errors on the first byte
   we have to use the serial interrupts as well. */

static inline void
e100_disable_serial_data_irq(struct e100_serial *info)
{
#ifdef SERIAL_DEBUG_INTR
	printk("ser_irq(%d): 0\n",info->line);
#endif
	DINTR1(DEBUG_LOG(info->line,"IRQ disable data_irq %i\n", info->line));
	*R_IRQ_MASK1_CLR = (1U << (8+2*info->line));
}

static inline void
e100_enable_serial_data_irq(struct e100_serial *info)
{
#ifdef SERIAL_DEBUG_INTR
	printk("ser_irq(%d): 1\n",info->line);
	printk("**** %d = %d\n",
	       (8+2*info->line),
	       (1U << (8+2*info->line)));
#endif
	DINTR1(DEBUG_LOG(info->line,"IRQ enable data_irq %i\n", info->line));
	*R_IRQ_MASK1_SET = (1U << (8+2*info->line));
}
#endif

static inline void
e100_disable_serial_tx_ready_irq(struct e100_serial *info)
{
#ifdef SERIAL_DEBUG_INTR
	printk("ser_tx_irq(%d): 0\n",info->line);
#endif
	DINTR1(DEBUG_LOG(info->line,"IRQ disable ready_irq %i\n", info->line));
	*R_IRQ_MASK1_CLR = (1U << (8+1+2*info->line));
}

static inline void
e100_enable_serial_tx_ready_irq(struct e100_serial *info)
{
#ifdef SERIAL_DEBUG_INTR
	printk("ser_tx_irq(%d): 1\n",info->line);
	printk("**** %d = %d\n",
	       (8+1+2*info->line),
	       (1U << (8+1+2*info->line)));
#endif
	DINTR2(DEBUG_LOG(info->line,"IRQ enable ready_irq %i\n", info->line));
	*R_IRQ_MASK1_SET = (1U << (8+1+2*info->line));
}

static inline void e100_enable_rx_irq(struct e100_serial *info)
{
	if (info->uses_dma_in)
		e100_enable_rxdma_irq(info);
	else
		e100_enable_serial_data_irq(info);
}
static inline void e100_disable_rx_irq(struct e100_serial *info)
{
	if (info->uses_dma_in)
		e100_disable_rxdma_irq(info);
	else
		e100_disable_serial_data_irq(info);
}

#if defined(CONFIG_ETRAX_RS485)
/* Enable RS-485 mode on selected port. This is UGLY. */
static int
e100_enable_rs485(struct tty_struct *tty, struct serial_rs485 *r)
{
	struct e100_serial * info = (struct e100_serial *)tty->driver_data;

#if defined(CONFIG_ETRAX_RS485_ON_PA)
	*R_PORT_PA_DATA = port_pa_data_shadow |= (1 << rs485_pa_bit);
#endif
#if defined(CONFIG_ETRAX_RS485_ON_PORT_G)
	REG_SHADOW_SET(R_PORT_G_DATA,  port_g_data_shadow,
		       rs485_port_g_bit, 1);
#endif
#if defined(CONFIG_ETRAX_RS485_LTC1387)
	REG_SHADOW_SET(R_PORT_G_DATA, port_g_data_shadow,
		       CONFIG_ETRAX_RS485_LTC1387_DXEN_PORT_G_BIT, 1);
	REG_SHADOW_SET(R_PORT_G_DATA, port_g_data_shadow,
		       CONFIG_ETRAX_RS485_LTC1387_RXEN_PORT_G_BIT, 1);
#endif

	info->rs485.flags = r->flags;
	if (r->delay_rts_before_send >= 1000)
		info->rs485.delay_rts_before_send = 1000;
	else
		info->rs485.delay_rts_before_send = r->delay_rts_before_send;
/*	printk("rts: on send = %i, after = %i, enabled = %i",
		    info->rs485.rts_on_send,
		    info->rs485.rts_after_sent,
		    info->rs485.enabled
	);
*/
	return 0;
}

static int
e100_write_rs485(struct tty_struct *tty,
                 const unsigned char *buf, int count)
{
	struct e100_serial * info = (struct e100_serial *)tty->driver_data;
	int old_value = (info->rs485.flags) & SER_RS485_ENABLED;

	/* rs485 is always implicitly enabled if we're using the ioctl()
	 * but it doesn't have to be set in the serial_rs485
	 * (to be backward compatible with old apps)
	 * So we store, set and restore it.
	 */
	info->rs485.flags |= SER_RS485_ENABLED;
	/* rs_write now deals with RS485 if enabled */
	count = rs_write(tty, buf, count);
	if (!old_value)
		info->rs485.flags &= ~(SER_RS485_ENABLED);
	return count;
}

#ifdef CONFIG_ETRAX_FAST_TIMER
/* Timer function to toggle RTS when using FAST_TIMER */
static void rs485_toggle_rts_timer_function(unsigned long data)
{
	struct e100_serial *info = (struct e100_serial *)data;

	fast_timers_rs485[info->line].function = NULL;
	e100_rts(info, (info->rs485.flags & SER_RS485_RTS_AFTER_SEND));
#if defined(CONFIG_ETRAX_RS485_DISABLE_RECEIVER)
	e100_enable_rx(info);
	e100_enable_rx_irq(info);
#endif
}
#endif
#endif /* CONFIG_ETRAX_RS485 */

/*
 * ------------------------------------------------------------
 * rs_stop() and rs_start()
 *
 * This routines are called before setting or resetting tty->stopped.
 * They enable or disable transmitter using the XOFF registers, as necessary.
 * ------------------------------------------------------------
 */

static void
rs_stop(struct tty_struct *tty)
{
	struct e100_serial *info = (struct e100_serial *)tty->driver_data;
	if (info) {
		unsigned long flags;
		unsigned long xoff;

		local_irq_save(flags);
		DFLOW(DEBUG_LOG(info->line, "XOFF rs_stop xmit %i\n",
				CIRC_CNT(info->xmit.head,
					 info->xmit.tail,SERIAL_XMIT_SIZE)));

		xoff = IO_FIELD(R_SERIAL0_XOFF, xoff_char,
				STOP_CHAR(info->port.tty));
		xoff |= IO_STATE(R_SERIAL0_XOFF, tx_stop, stop);
		if (tty->termios->c_iflag & IXON ) {
			xoff |= IO_STATE(R_SERIAL0_XOFF, auto_xoff, enable);
		}

		*((unsigned long *)&info->ioport[REG_XOFF]) = xoff;
		local_irq_restore(flags);
	}
}

static void
rs_start(struct tty_struct *tty)
{
	struct e100_serial *info = (struct e100_serial *)tty->driver_data;
	if (info) {
		unsigned long flags;
		unsigned long xoff;

		local_irq_save(flags);
		DFLOW(DEBUG_LOG(info->line, "XOFF rs_start xmit %i\n",
				CIRC_CNT(info->xmit.head,
					 info->xmit.tail,SERIAL_XMIT_SIZE)));
		xoff = IO_FIELD(R_SERIAL0_XOFF, xoff_char, STOP_CHAR(tty));
		xoff |= IO_STATE(R_SERIAL0_XOFF, tx_stop, enable);
		if (tty->termios->c_iflag & IXON ) {
			xoff |= IO_STATE(R_SERIAL0_XOFF, auto_xoff, enable);
		}

		*((unsigned long *)&info->ioport[REG_XOFF]) = xoff;
		if (!info->uses_dma_out &&
		    info->xmit.head != info->xmit.tail && info->xmit.buf)
			e100_enable_serial_tx_ready_irq(info);

		local_irq_restore(flags);
	}
}

/*
 * ----------------------------------------------------------------------
 *
 * Here starts the interrupt handling routines.  All of the following
 * subroutines are declared as inline and are folded into
 * rs_interrupt().  They were separated out for readability's sake.
 *
 * Note: rs_interrupt() is a "fast" interrupt, which means that it
 * runs with interrupts turned off.  People who may want to modify
 * rs_interrupt() should try to keep the interrupt handler as fast as
 * possible.  After you are done making modifications, it is not a bad
 * idea to do:
 *
 * gcc -S -DKERNEL -Wall -Wstrict-prototypes -O6 -fomit-frame-pointer serial.c
 *
 * and look at the resulting assemble code in serial.s.
 *
 * 				- Ted Ts'o (tytso@mit.edu), 7-Mar-93
 * -----------------------------------------------------------------------
 */

/*
 * This routine is used by the interrupt handler to schedule
 * processing in the software interrupt portion of the driver.
 */
static void rs_sched_event(struct e100_serial *info, int event)
{
	if (info->event & (1 << event))
		return;
	info->event |= 1 << event;
	schedule_work(&info->work);
}

/* The output DMA channel is free - use it to send as many chars as possible
 * NOTES:
 *   We don't pay attention to info->x_char, which means if the TTY wants to
 *   use XON/XOFF it will set info->x_char but we won't send any X char!
 *
 *   To implement this, we'd just start a DMA send of 1 byte pointing at a
 *   buffer containing the X char, and skip updating xmit. We'd also have to
 *   check if the last sent char was the X char when we enter this function
 *   the next time, to avoid updating xmit with the sent X value.
 */

static void
transmit_chars_dma(struct e100_serial *info)
{
	unsigned int c, sentl;
	struct etrax_dma_descr *descr;

#ifdef CONFIG_SVINTO_SIM
	/* This will output too little if tail is not 0 always since
	 * we don't reloop to send the other part. Anyway this SHOULD be a
	 * no-op - transmit_chars_dma would never really be called during sim
	 * since rs_write does not write into the xmit buffer then.
	 */
	if (info->xmit.tail)
		printk("Error in serial.c:transmit_chars-dma(), tail!=0\n");
	if (info->xmit.head != info->xmit.tail) {
		SIMCOUT(info->xmit.buf + info->xmit.tail,
			CIRC_CNT(info->xmit.head,
				 info->xmit.tail,
				 SERIAL_XMIT_SIZE));
		info->xmit.head = info->xmit.tail;  /* move back head */
		info->tr_running = 0;
	}
	return;
#endif
	/* acknowledge both dma_descr and dma_eop irq in R_DMA_CHx_CLR_INTR */
	*info->oclrintradr =
		IO_STATE(R_DMA_CH6_CLR_INTR, clr_descr, do) |
		IO_STATE(R_DMA_CH6_CLR_INTR, clr_eop, do);

#ifdef SERIAL_DEBUG_INTR
	if (info->line == SERIAL_DEBUG_LINE)
		printk("tc\n");
#endif
	if (!info->tr_running) {
		/* weirdo... we shouldn't get here! */
		printk(KERN_WARNING "Achtung: transmit_chars_dma with !tr_running\n");
		return;
	}

	descr = &info->tr_descr;

	/* first get the amount of bytes sent during the last DMA transfer,
	   and update xmit accordingly */

	/* if the stop bit was not set, all data has been sent */
	if (!(descr->status & d_stop)) {
		sentl = descr->sw_len;
	} else
		/* otherwise we find the amount of data sent here */
		sentl = descr->hw_len;

	DFLOW(DEBUG_LOG(info->line, "TX %i done\n", sentl));

	/* update stats */
	info->icount.tx += sentl;

	/* update xmit buffer */
	info->xmit.tail = (info->xmit.tail + sentl) & (SERIAL_XMIT_SIZE - 1);

	/* if there is only a few chars left in the buf, wake up the blocked
	   write if any */
	if (CIRC_CNT(info->xmit.head,
		     info->xmit.tail,
		     SERIAL_XMIT_SIZE) < WAKEUP_CHARS)
		rs_sched_event(info, RS_EVENT_WRITE_WAKEUP);

	/* find out the largest amount of consecutive bytes we want to send now */

	c = CIRC_CNT_TO_END(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE);

	/* Don't send all in one DMA transfer - divide it so we wake up
	 * application before all is sent
	 */

	if (c >= 4*WAKEUP_CHARS)
		c = c/2;

	if (c <= 0) {
		/* our job here is done, don't schedule any new DMA transfer */
		info->tr_running = 0;

#if defined(CONFIG_ETRAX_RS485) && defined(CONFIG_ETRAX_FAST_TIMER)
		if (info->rs485.flags & SER_RS485_ENABLED) {
			/* Set a short timer to toggle RTS */
			start_one_shot_timer(&fast_timers_rs485[info->line],
			                     rs485_toggle_rts_timer_function,
			                     (unsigned long)info,
			                     info->char_time_usec*2,
			                     "RS-485");
		}
#endif /* RS485 */
		return;
	}

	/* ok we can schedule a dma send of c chars starting at info->xmit.tail */
	/* set up the descriptor correctly for output */
	DFLOW(DEBUG_LOG(info->line, "TX %i\n", c));
	descr->ctrl = d_int | d_eol | d_wait; /* Wait needed for tty_wait_until_sent() */
	descr->sw_len = c;
	descr->buf = virt_to_phys(info->xmit.buf + info->xmit.tail);
	descr->status = 0;

	*info->ofirstadr = virt_to_phys(descr); /* write to R_DMAx_FIRST */
	*info->ocmdadr = IO_STATE(R_DMA_CH6_CMD, cmd, start);

	/* DMA is now running (hopefully) */
} /* transmit_chars_dma */

static void
start_transmit(struct e100_serial *info)
{
#if 0
	if (info->line == SERIAL_DEBUG_LINE)
		printk("x\n");
#endif

	info->tr_descr.sw_len = 0;
	info->tr_descr.hw_len = 0;
	info->tr_descr.status = 0;
	info->tr_running = 1;
	if (info->uses_dma_out)
		transmit_chars_dma(info);
	else
		e100_enable_serial_tx_ready_irq(info);
} /* start_transmit */

#ifdef CONFIG_ETRAX_SERIAL_FAST_TIMER
static int serial_fast_timer_started = 0;
static int serial_fast_timer_expired = 0;
static void flush_timeout_function(unsigned long data);
#define START_FLUSH_FAST_TIMER_TIME(info, string, usec) {\
  unsigned long timer_flags; \
  local_irq_save(timer_flags); \
  if (fast_timers[info->line].function == NULL) { \
    serial_fast_timer_started++; \
    TIMERD(DEBUG_LOG(info->line, "start_timer %i ", info->line)); \
    TIMERD(DEBUG_LOG(info->line, "num started: %i\n", serial_fast_timer_started)); \
    start_one_shot_timer(&fast_timers[info->line], \
                         flush_timeout_function, \
                         (unsigned long)info, \
                         (usec), \
                         string); \
  } \
  else { \
    TIMERD(DEBUG_LOG(info->line, "timer %i already running\n", info->line)); \
  } \
  local_irq_restore(timer_flags); \
}
#define START_FLUSH_FAST_TIMER(info, string) START_FLUSH_FAST_TIMER_TIME(info, string, info->flush_time_usec)

#else
#define START_FLUSH_FAST_TIMER_TIME(info, string, usec)
#define START_FLUSH_FAST_TIMER(info, string)
#endif

static struct etrax_recv_buffer *
alloc_recv_buffer(unsigned int size)
{
	struct etrax_recv_buffer *buffer;

	if (!(buffer = kmalloc(sizeof *buffer + size, GFP_ATOMIC)))
		return NULL;

	buffer->next = NULL;
	buffer->length = 0;
	buffer->error = TTY_NORMAL;

	return buffer;
}

static void
append_recv_buffer(struct e100_serial *info, struct etrax_recv_buffer *buffer)
{
	unsigned long flags;

	local_irq_save(flags);

	if (!info->first_recv_buffer)
		info->first_recv_buffer = buffer;
	else
		info->last_recv_buffer->next = buffer;

	info->last_recv_buffer = buffer;

	info->recv_cnt += buffer->length;
	if (info->recv_cnt > info->max_recv_cnt)
		info->max_recv_cnt = info->recv_cnt;

	local_irq_restore(flags);
}

static int
add_char_and_flag(struct e100_serial *info, unsigned char data, unsigned char flag)
{
	struct etrax_recv_buffer *buffer;
	if (info->uses_dma_in) {
		if (!(buffer = alloc_recv_buffer(4)))
			return 0;

		buffer->length = 1;
		buffer->error = flag;
		buffer->buffer[0] = data;

		append_recv_buffer(info, buffer);

		info->icount.rx++;
	} else {
		struct tty_struct *tty = info->port.tty;
		tty_insert_flip_char(tty, data, flag);
		info->icount.rx++;
	}

	return 1;
}

static unsigned int handle_descr_data(struct e100_serial *info,
				      struct etrax_dma_descr *descr,
				      unsigned int recvl)
{
	struct etrax_recv_buffer *buffer = phys_to_virt(descr->buf) - sizeof *buffer;

	if (info->recv_cnt + recvl > 65536) {
		printk(KERN_CRIT
		       "%s: Too much pending incoming serial data! Dropping %u bytes.\n", __func__, recvl);
		return 0;
	}

	buffer->length = recvl;

	if (info->errorcode == ERRCODE_SET_BREAK)
		buffer->error = TTY_BREAK;
	info->errorcode = 0;

	append_recv_buffer(info, buffer);

	if (!(buffer = alloc_recv_buffer(SERIAL_DESCR_BUF_SIZE)))
		panic("%s: Failed to allocate memory for receive buffer!\n", __func__);

	descr->buf = virt_to_phys(buffer->buffer);

	return recvl;
}

static unsigned int handle_all_descr_data(struct e100_serial *info)
{
	struct etrax_dma_descr *descr;
	unsigned int recvl;
	unsigned int ret = 0;

	while (1)
	{
		descr = &info->rec_descr[info->cur_rec_descr];

		if (descr == phys_to_virt(*info->idescradr))
			break;

		if (++info->cur_rec_descr == SERIAL_RECV_DESCRIPTORS)
			info->cur_rec_descr = 0;

		/* find out how many bytes were read */

		/* if the eop bit was not set, all data has been received */
		if (!(descr->status & d_eop)) {
			recvl = descr->sw_len;
		} else {
			/* otherwise we find the amount of data received here */
			recvl = descr->hw_len;
		}

		/* Reset the status information */
		descr->status = 0;

		DFLOW(  DEBUG_LOG(info->line, "RX %lu\n", recvl);
			if (info->port.tty->stopped) {
				unsigned char *buf = phys_to_virt(descr->buf);
				DEBUG_LOG(info->line, "rx 0x%02X\n", buf[0]);
				DEBUG_LOG(info->line, "rx 0x%02X\n", buf[1]);
				DEBUG_LOG(info->line, "rx 0x%02X\n", buf[2]);
			}
			);

		/* update stats */
		info->icount.rx += recvl;

		ret += handle_descr_data(info, descr, recvl);
	}

	return ret;
}

static void receive_chars_dma(struct e100_serial *info)
{
	struct tty_struct *tty;
	unsigned char rstat;

#ifdef CONFIG_SVINTO_SIM
	/* No receive in the simulator.  Will probably be when the rest of
	 * the serial interface works, and this piece will just be removed.
	 */
	return;
#endif

	/* Acknowledge both dma_descr and dma_eop irq in R_DMA_CHx_CLR_INTR */
	*info->iclrintradr =
		IO_STATE(R_DMA_CH6_CLR_INTR, clr_descr, do) |
		IO_STATE(R_DMA_CH6_CLR_INTR, clr_eop, do);

	tty = info->port.tty;
	if (!tty) /* Something wrong... */
		return;

#ifdef SERIAL_HANDLE_EARLY_ERRORS
	if (info->uses_dma_in)
		e100_enable_serial_data_irq(info);
#endif

	if (info->errorcode == ERRCODE_INSERT_BREAK)
		add_char_and_flag(info, '\0', TTY_BREAK);

	handle_all_descr_data(info);

	/* Read the status register to detect errors */
	rstat = info->ioport[REG_STATUS];
	if (rstat & IO_MASK(R_SERIAL0_STATUS, xoff_detect) ) {
		DFLOW(DEBUG_LOG(info->line, "XOFF detect stat %x\n", rstat));
	}

	if (rstat & SER_ERROR_MASK) {
		/* If we got an error, we must reset it by reading the
		 * data_in field
		 */
		unsigned char data = info->ioport[REG_DATA];

		PROCSTAT(ser_stat[info->line].errors_cnt++);
		DEBUG_LOG(info->line, "#dERR: s d 0x%04X\n",
			  ((rstat & SER_ERROR_MASK) << 8) | data);

		if (rstat & SER_PAR_ERR_MASK)
			add_char_and_flag(info, data, TTY_PARITY);
		else if (rstat & SER_OVERRUN_MASK)
			add_char_and_flag(info, data, TTY_OVERRUN);
		else if (rstat & SER_FRAMING_ERR_MASK)
			add_char_and_flag(info, data, TTY_FRAME);
	}

	START_FLUSH_FAST_TIMER(info, "receive_chars");

	/* Restart the receiving DMA */
	*info->icmdadr = IO_STATE(R_DMA_CH6_CMD, cmd, restart);
}

static int start_recv_dma(struct e100_serial *info)
{
	struct etrax_dma_descr *descr = info->rec_descr;
	struct etrax_recv_buffer *buffer;
        int i;

	/* Set up the receiving descriptors */
	for (i = 0; i < SERIAL_RECV_DESCRIPTORS; i++) {
		if (!(buffer = alloc_recv_buffer(SERIAL_DESCR_BUF_SIZE)))
			panic("%s: Failed to allocate memory for receive buffer!\n", __func__);

		descr[i].ctrl = d_int;
		descr[i].buf = virt_to_phys(buffer->buffer);
		descr[i].sw_len = SERIAL_DESCR_BUF_SIZE;
		descr[i].hw_len = 0;
		descr[i].status = 0;
		descr[i].next = virt_to_phys(&descr[i+1]);
	}

	/* Link the last descriptor to the first */
	descr[i-1].next = virt_to_phys(&descr[0]);

	/* Start with the first descriptor in the list */
	info->cur_rec_descr = 0;

	/* Start the DMA */
	*info->ifirstadr = virt_to_phys(&descr[info->cur_rec_descr]);
	*info->icmdadr = IO_STATE(R_DMA_CH6_CMD, cmd, start);

	/* Input DMA should be running now */
	return 1;
}

static void
start_receive(struct e100_serial *info)
{
#ifdef CONFIG_SVINTO_SIM
	/* No receive in the simulator.  Will probably be when the rest of
	 * the serial interface works, and this piece will just be removed.
	 */
	return;
#endif
	if (info->uses_dma_in) {
		/* reset the input dma channel to be sure it works */

		*info->icmdadr = IO_STATE(R_DMA_CH6_CMD, cmd, reset);
		while (IO_EXTRACT(R_DMA_CH6_CMD, cmd, *info->icmdadr) ==
		       IO_STATE_VALUE(R_DMA_CH6_CMD, cmd, reset));

		start_recv_dma(info);
	}
}


/* the bits in the MASK2 register are laid out like this:
   DMAI_EOP DMAI_DESCR DMAO_EOP DMAO_DESCR
   where I is the input channel and O is the output channel for the port.
   info->irq is the bit number for the DMAO_DESCR so to check the others we
   shift info->irq to the left.
*/

/* dma output channel interrupt handler
   this interrupt is called from DMA2(ser2), DMA4(ser3), DMA6(ser0) or
   DMA8(ser1) when they have finished a descriptor with the intr flag set.
*/

static irqreturn_t
tr_interrupt(int irq, void *dev_id)
{
	struct e100_serial *info;
	unsigned long ireg;
	int i;
	int handled = 0;

#ifdef CONFIG_SVINTO_SIM
	/* No receive in the simulator.  Will probably be when the rest of
	 * the serial interface works, and this piece will just be removed.
	 */
	{
		const char *s = "What? tr_interrupt in simulator??\n";
		SIMCOUT(s,strlen(s));
	}
	return IRQ_HANDLED;
#endif

	/* find out the line that caused this irq and get it from rs_table */

	ireg = *R_IRQ_MASK2_RD;  /* get the active irq bits for the dma channels */

	for (i = 0; i < NR_PORTS; i++) {
		info = rs_table + i;
		if (!info->enabled || !info->uses_dma_out)
			continue;
		/* check for dma_descr (don't need to check for dma_eop in output dma for serial */
		if (ireg & info->irq) {
			handled = 1;
			/* we can send a new dma bunch. make it so. */
			DINTR2(DEBUG_LOG(info->line, "tr_interrupt %i\n", i));
			/* Read jiffies_usec first,
			 * we want this time to be as late as possible
			 */
 			PROCSTAT(ser_stat[info->line].tx_dma_ints++);
			info->last_tx_active_usec = GET_JIFFIES_USEC();
			info->last_tx_active = jiffies;
			transmit_chars_dma(info);
		}

		/* FIXME: here we should really check for a change in the
		   status lines and if so call status_handle(info) */
	}
	return IRQ_RETVAL(handled);
} /* tr_interrupt */

/* dma input channel interrupt handler */

static irqreturn_t
rec_interrupt(int irq, void *dev_id)
{
	struct e100_serial *info;
	unsigned long ireg;
	int i;
	int handled = 0;

#ifdef CONFIG_SVINTO_SIM
	/* No receive in the simulator.  Will probably be when the rest of
	 * the serial interface works, and this piece will just be removed.
	 */
	{
		const char *s = "What? rec_interrupt in simulator??\n";
		SIMCOUT(s,strlen(s));
	}
	return IRQ_HANDLED;
#endif

	/* find out the line that caused this irq and get it from rs_table */

	ireg = *R_IRQ_MASK2_RD;  /* get the active irq bits for the dma channels */

	for (i = 0; i < NR_PORTS; i++) {
		info = rs_table + i;
		if (!info->enabled || !info->uses_dma_in)
			continue;
		/* check for both dma_eop and dma_descr for the input dma channel */
		if (ireg & ((info->irq << 2) | (info->irq << 3))) {
			handled = 1;
			/* we have received something */
			receive_chars_dma(info);
		}

		/* FIXME: here we should really check for a change in the
		   status lines and if so call status_handle(info) */
	}
	return IRQ_RETVAL(handled);
} /* rec_interrupt */

static int force_eop_if_needed(struct e100_serial *info)
{
	/* We check data_avail bit to determine if data has
	 * arrived since last time
	 */
	unsigned char rstat = info->ioport[REG_STATUS];

	/* error or datavail? */
	if (rstat & SER_ERROR_MASK) {
		/* Some error has occurred. If there has been valid data, an
		 * EOP interrupt will be made automatically. If no data, the
		 * normal ser_interrupt should be enabled and handle it.
		 * So do nothing!
		 */
		DEBUG_LOG(info->line, "timeout err: rstat 0x%03X\n",
		          rstat | (info->line << 8));
		return 0;
	}

	if (rstat & SER_DATA_AVAIL_MASK) {
		/* Ok data, no error, count it */
		TIMERD(DEBUG_LOG(info->line, "timeout: rstat 0x%03X\n",
		          rstat | (info->line << 8)));
		/* Read data to clear status flags */
		(void)info->ioport[REG_DATA];

		info->forced_eop = 0;
		START_FLUSH_FAST_TIMER(info, "magic");
		return 0;
	}

	/* hit the timeout, force an EOP for the input
	 * dma channel if we haven't already
	 */
	if (!info->forced_eop) {
		info->forced_eop = 1;
		PROCSTAT(ser_stat[info->line].timeout_flush_cnt++);
		TIMERD(DEBUG_LOG(info->line, "timeout EOP %i\n", info->line));
		FORCE_EOP(info);
	}

	return 1;
}

static void flush_to_flip_buffer(struct e100_serial *info)
{
	struct tty_struct *tty;
	struct etrax_recv_buffer *buffer;
	unsigned long flags;

	local_irq_save(flags);
	tty = info->port.tty;

	if (!tty) {
		local_irq_restore(flags);
		return;
	}

	while ((buffer = info->first_recv_buffer) != NULL) {
		unsigned int count = buffer->length;

		tty_insert_flip_string(tty, buffer->buffer, count);
		info->recv_cnt -= count;

		if (count == buffer->length) {
			info->first_recv_buffer = buffer->next;
			kfree(buffer);
		} else {
			buffer->length -= count;
			memmove(buffer->buffer, buffer->buffer + count, buffer->length);
			buffer->error = TTY_NORMAL;
		}
	}

	if (!info->first_recv_buffer)
		info->last_recv_buffer = NULL;

	local_irq_restore(flags);

	/* This includes a check for low-latency */
	tty_flip_buffer_push(tty);
}

static void check_flush_timeout(struct e100_serial *info)
{
	/* Flip what we've got (if we can) */
	flush_to_flip_buffer(info);

	/* We might need to flip later, but not to fast
	 * since the system is busy processing input... */
	if (info->first_recv_buffer)
		START_FLUSH_FAST_TIMER_TIME(info, "flip", 2000);

	/* Force eop last, since data might have come while we're processing
	 * and if we started the slow timer above, we won't start a fast
	 * below.
	 */
	force_eop_if_needed(info);
}

#ifdef CONFIG_ETRAX_SERIAL_FAST_TIMER
static void flush_timeout_function(unsigned long data)
{
	struct e100_serial *info = (struct e100_serial *)data;

	fast_timers[info->line].function = NULL;
	serial_fast_timer_expired++;
	TIMERD(DEBUG_LOG(info->line, "flush_timout %i ", info->line));
	TIMERD(DEBUG_LOG(info->line, "num expired: %i\n", serial_fast_timer_expired));
	check_flush_timeout(info);
}

#else

/* dma fifo/buffer timeout handler
   forces an end-of-packet for the dma input channel if no chars
   have been received for CONFIG_ETRAX_SERIAL_RX_TIMEOUT_TICKS/100 s.
*/

static struct timer_list flush_timer;

static void
timed_flush_handler(unsigned long ptr)
{
	struct e100_serial *info;
	int i;

#ifdef CONFIG_SVINTO_SIM
	return;
#endif

	for (i = 0; i < NR_PORTS; i++) {
		info = rs_table + i;
		if (info->uses_dma_in)
			check_flush_timeout(info);
	}

	/* restart flush timer */
	mod_timer(&flush_timer, jiffies + CONFIG_ETRAX_SERIAL_RX_TIMEOUT_TICKS);
}
#endif

#ifdef SERIAL_HANDLE_EARLY_ERRORS

/* If there is an error (ie break) when the DMA is running and
 * there are no bytes in the fifo the DMA is stopped and we get no
 * eop interrupt. Thus we have to monitor the first bytes on a DMA
 * transfer, and if it is without error we can turn the serial
 * interrupts off.
 */

/*
BREAK handling on ETRAX 100:
ETRAX will generate interrupt although there is no stop bit between the
characters.

Depending on how long the break sequence is, the end of the breaksequence
will look differently:
| indicates start/end of a character.

B= Break character (0x00) with framing error.
E= Error byte with parity error received after B characters.
F= "Faked" valid byte received immediately after B characters.
V= Valid byte

1.
    B          BL         ___________________________ V
.._|__________|__________|                           |valid data |

Multiple frame errors with data == 0x00 (B),
the timing matches up "perfectly" so no extra ending char is detected.
The RXD pin is 1 in the last interrupt, in that case
we set info->errorcode = ERRCODE_INSERT_BREAK, but we can't really
know if another byte will come and this really is case 2. below
(e.g F=0xFF or 0xFE)
If RXD pin is 0 we can expect another character (see 2. below).


2.

    B          B          E or F__________________..__ V
.._|__________|__________|______    |                 |valid data
                          "valid" or
                          parity error

Multiple frame errors with data == 0x00 (B),
but the part of the break trigs is interpreted as a start bit (and possibly
some 0 bits followed by a number of 1 bits and a stop bit).
Depending on parity settings etc. this last character can be either
a fake "valid" char (F) or have a parity error (E).

If the character is valid it will be put in the buffer,
we set info->errorcode = ERRCODE_SET_BREAK so the receive interrupt
will set the flags so the tty will handle it,
if it's an error byte it will not be put in the buffer
and we set info->errorcode = ERRCODE_INSERT_BREAK.

To distinguish a V byte in 1. from an F byte in 2. we keep a timestamp
of the last faulty char (B) and compares it with the current time:
If the time elapsed time is less then 2*char_time_usec we will assume
it's a faked F char and not a Valid char and set
info->errorcode = ERRCODE_SET_BREAK.

Flaws in the above solution:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
We use the timer to distinguish a F character from a V character,
if a V character is to close after the break we might make the wrong decision.

TODO: The break will be delayed until an F or V character is received.

*/

static
struct e100_serial * handle_ser_rx_interrupt_no_dma(struct e100_serial *info)
{
	unsigned long data_read;
	struct tty_struct *tty = info->port.tty;

	if (!tty) {
		printk("!NO TTY!\n");
		return info;
	}

	/* Read data and status at the same time */
	data_read = *((unsigned long *)&info->ioport[REG_DATA_STATUS32]);
more_data:
	if (data_read & IO_MASK(R_SERIAL0_READ, xoff_detect) ) {
		DFLOW(DEBUG_LOG(info->line, "XOFF detect\n", 0));
	}
	DINTR2(DEBUG_LOG(info->line, "ser_rx   %c\n", IO_EXTRACT(R_SERIAL0_READ, data_in, data_read)));

	if (data_read & ( IO_MASK(R_SERIAL0_READ, framing_err) |
			  IO_MASK(R_SERIAL0_READ, par_err) |
			  IO_MASK(R_SERIAL0_READ, overrun) )) {
		/* An error */
		info->last_rx_active_usec = GET_JIFFIES_USEC();
		info->last_rx_active = jiffies;
		DINTR1(DEBUG_LOG(info->line, "ser_rx err stat_data %04X\n", data_read));
		DLOG_INT_TRIG(
		if (!log_int_trig1_pos) {
			log_int_trig1_pos = log_int_pos;
			log_int(rdpc(), 0, 0);
		}
		);


		if ( ((data_read & IO_MASK(R_SERIAL0_READ, data_in)) == 0) &&
		     (data_read & IO_MASK(R_SERIAL0_READ, framing_err)) ) {
			/* Most likely a break, but we get interrupts over and
			 * over again.
			 */

			if (!info->break_detected_cnt) {
				DEBUG_LOG(info->line, "#BRK start\n", 0);
			}
			if (data_read & IO_MASK(R_SERIAL0_READ, rxd)) {
				/* The RX pin is high now, so the break
				 * must be over, but....
				 * we can't really know if we will get another
				 * last byte ending the break or not.
				 * And we don't know if the byte (if any) will
				 * have an error or look valid.
				 */
				DEBUG_LOG(info->line, "# BL BRK\n", 0);
				info->errorcode = ERRCODE_INSERT_BREAK;
			}
			info->break_detected_cnt++;
		} else {
			/* The error does not look like a break, but could be
			 * the end of one
			 */
			if (info->break_detected_cnt) {
				DEBUG_LOG(info->line, "EBRK %i\n", info->break_detected_cnt);
				info->errorcode = ERRCODE_INSERT_BREAK;
			} else {
				unsigned char data = IO_EXTRACT(R_SERIAL0_READ,
					data_in, data_read);
				char flag = TTY_NORMAL;
				if (info->errorcode == ERRCODE_INSERT_BREAK) {
					struct tty_struct *tty = info->port.tty;
					tty_insert_flip_char(tty, 0, flag);
					info->icount.rx++;
				}

				if (data_read & IO_MASK(R_SERIAL0_READ, par_err)) {
					info->icount.parity++;
					flag = TTY_PARITY;
				} else if (data_read & IO_MASK(R_SERIAL0_READ, overrun)) {
					info->icount.overrun++;
					flag = TTY_OVERRUN;
				} else if (data_read & IO_MASK(R_SERIAL0_READ, framing_err)) {
					info->icount.frame++;
					flag = TTY_FRAME;
				}
				tty_insert_flip_char(tty, data, flag);
				info->errorcode = 0;
			}
			info->break_detected_cnt = 0;
		}
	} else if (data_read & IO_MASK(R_SERIAL0_READ, data_avail)) {
		/* No error */
		DLOG_INT_TRIG(
		if (!log_int_trig1_pos) {
			if (log_int_pos >= log_int_size) {
				log_int_pos = 0;
			}
			log_int_trig0_pos = log_int_pos;
			log_int(rdpc(), 0, 0);
		}
		);
		tty_insert_flip_char(tty,
			IO_EXTRACT(R_SERIAL0_READ, data_in, data_read),
			TTY_NORMAL);
	} else {
		DEBUG_LOG(info->line, "ser_rx int but no data_avail  %08lX\n", data_read);
	}


	info->icount.rx++;
	data_read = *((unsigned long *)&info->ioport[REG_DATA_STATUS32]);
	if (data_read & IO_MASK(R_SERIAL0_READ, data_avail)) {
		DEBUG_LOG(info->line, "ser_rx   %c in loop\n", IO_EXTRACT(R_SERIAL0_READ, data_in, data_read));
		goto more_data;
	}

	tty_flip_buffer_push(info->port.tty);
	return info;
}

static struct e100_serial* handle_ser_rx_interrupt(struct e100_serial *info)
{
	unsigned char rstat;

#ifdef SERIAL_DEBUG_INTR
	printk("Interrupt from serport %d\n", i);
#endif
/*	DEBUG_LOG(info->line, "ser_interrupt stat %03X\n", rstat | (i << 8)); */
	if (!info->uses_dma_in) {
		return handle_ser_rx_interrupt_no_dma(info);
	}
	/* DMA is used */
	rstat = info->ioport[REG_STATUS];
	if (rstat & IO_MASK(R_SERIAL0_STATUS, xoff_detect) ) {
		DFLOW(DEBUG_LOG(info->line, "XOFF detect\n", 0));
	}

	if (rstat & SER_ERROR_MASK) {
		unsigned char data;

		info->last_rx_active_usec = GET_JIFFIES_USEC();
		info->last_rx_active = jiffies;
		/* If we got an error, we must reset it by reading the
		 * data_in field
		 */
		data = info->ioport[REG_DATA];
		DINTR1(DEBUG_LOG(info->line, "ser_rx!  %c\n", data));
		DINTR1(DEBUG_LOG(info->line, "ser_rx err stat %02X\n", rstat));
		if (!data && (rstat & SER_FRAMING_ERR_MASK)) {
			/* Most likely a break, but we get interrupts over and
			 * over again.
			 */

			if (!info->break_detected_cnt) {
				DEBUG_LOG(info->line, "#BRK start\n", 0);
			}
			if (rstat & SER_RXD_MASK) {
				/* The RX pin is high now, so the break
				 * must be over, but....
				 * we can't really know if we will get another
				 * last byte ending the break or not.
				 * And we don't know if the byte (if any) will
				 * have an error or look valid.
				 */
				DEBUG_LOG(info->line, "# BL BRK\n", 0);
				info->errorcode = ERRCODE_INSERT_BREAK;
			}
			info->break_detected_cnt++;
		} else {
			/* The error does not look like a break, but could be
			 * the end of one
			 */
			if (info->break_detected_cnt) {
				DEBUG_LOG(info->line, "EBRK %i\n", info->break_detected_cnt);
				info->errorcode = ERRCODE_INSERT_BREAK;
			} else {
				if (info->errorcode == ERRCODE_INSERT_BREAK) {
					info->icount.brk++;
					add_char_and_flag(info, '\0', TTY_BREAK);
				}

				if (rstat & SER_PAR_ERR_MASK) {
					info->icount.parity++;
					add_char_and_flag(info, data, TTY_PARITY);
				} else if (rstat & SER_OVERRUN_MASK) {
					info->icount.overrun++;
					add_char_and_flag(info, data, TTY_OVERRUN);
				} else if (rstat & SER_FRAMING_ERR_MASK) {
					info->icount.frame++;
					add_char_and_flag(info, data, TTY_FRAME);
				}

				info->errorcode = 0;
			}
			info->break_detected_cnt = 0;
			DEBUG_LOG(info->line, "#iERR s d %04X\n",
			          ((rstat & SER_ERROR_MASK) << 8) | data);
		}
		PROCSTAT(ser_stat[info->line].early_errors_cnt++);
	} else { /* It was a valid byte, now let the DMA do the rest */
		unsigned long curr_time_u = GET_JIFFIES_USEC();
		unsigned long curr_time = jiffies;

		if (info->break_detected_cnt) {
			/* Detect if this character is a new valid char or the
			 * last char in a break sequence: If LSBits are 0 and
			 * MSBits are high AND the time is close to the
			 * previous interrupt we should discard it.
			 */
			long elapsed_usec =
			  (curr_time - info->last_rx_active) * (1000000/HZ) +
			  curr_time_u - info->last_rx_active_usec;
			if (elapsed_usec < 2*info->char_time_usec) {
				DEBUG_LOG(info->line, "FBRK %i\n", info->line);
				/* Report as BREAK (error) and let
				 * receive_chars_dma() handle it
				 */
				info->errorcode = ERRCODE_SET_BREAK;
			} else {
				DEBUG_LOG(info->line, "Not end of BRK (V)%i\n", info->line);
			}
			DEBUG_LOG(info->line, "num brk %i\n", info->break_detected_cnt);
		}

#ifdef SERIAL_DEBUG_INTR
		printk("** OK, disabling ser_interrupts\n");
#endif
		e100_disable_serial_data_irq(info);
		DINTR2(DEBUG_LOG(info->line, "ser_rx OK %d\n", info->line));
		info->break_detected_cnt = 0;

		PROCSTAT(ser_stat[info->line].ser_ints_ok_cnt++);
	}
	/* Restarting the DMA never hurts */
	*info->icmdadr = IO_STATE(R_DMA_CH6_CMD, cmd, restart);
	START_FLUSH_FAST_TIMER(info, "ser_int");
	return info;
} /* handle_ser_rx_interrupt */

static void handle_ser_tx_interrupt(struct e100_serial *info)
{
	unsigned long flags;

	if (info->x_char) {
		unsigned char rstat;
		DFLOW(DEBUG_LOG(info->line, "tx_int: xchar 0x%02X\n", info->x_char));
		local_irq_save(flags);
		rstat = info->ioport[REG_STATUS];
		DFLOW(DEBUG_LOG(info->line, "stat %x\n", rstat));

		info->ioport[REG_TR_DATA] = info->x_char;
		info->icount.tx++;
		info->x_char = 0;
		/* We must enable since it is disabled in ser_interrupt */
		e100_enable_serial_tx_ready_irq(info);
		local_irq_restore(flags);
		return;
	}
	if (info->uses_dma_out) {
		unsigned char rstat;
		int i;
		/* We only use normal tx interrupt when sending x_char */
		DFLOW(DEBUG_LOG(info->line, "tx_int: xchar sent\n", 0));
		local_irq_save(flags);
		rstat = info->ioport[REG_STATUS];
		DFLOW(DEBUG_LOG(info->line, "stat %x\n", rstat));
		e100_disable_serial_tx_ready_irq(info);
		if (info->port.tty->stopped)
			rs_stop(info->port.tty);
		/* Enable the DMA channel and tell it to continue */
		e100_enable_txdma_channel(info);
		/* Wait 12 cycles before doing the DMA command */
		for(i = 6;  i > 0; i--)
			nop();

		*info->ocmdadr = IO_STATE(R_DMA_CH6_CMD, cmd, continue);
		local_irq_restore(flags);
		return;
	}
	/* Normal char-by-char interrupt */
	if (info->xmit.head == info->xmit.tail
	    || info->port.tty->stopped
	    || info->port.tty->hw_stopped) {
		DFLOW(DEBUG_LOG(info->line, "tx_int: stopped %i\n",
				info->port.tty->stopped));
		e100_disable_serial_tx_ready_irq(info);
		info->tr_running = 0;
		return;
	}
	DINTR2(DEBUG_LOG(info->line, "tx_int %c\n", info->xmit.buf[info->xmit.tail]));
	/* Send a byte, rs485 timing is critical so turn of ints */
	local_irq_save(flags);
	info->ioport[REG_TR_DATA] = info->xmit.buf[info->xmit.tail];
	info->xmit.tail = (info->xmit.tail + 1) & (SERIAL_XMIT_SIZE-1);
	info->icount.tx++;
	if (info->xmit.head == info->xmit.tail) {
#if defined(CONFIG_ETRAX_RS485) && defined(CONFIG_ETRAX_FAST_TIMER)
		if (info->rs485.flags & SER_RS485_ENABLED) {
			/* Set a short timer to toggle RTS */
			start_one_shot_timer(&fast_timers_rs485[info->line],
			                     rs485_toggle_rts_timer_function,
			                     (unsigned long)info,
			                     info->char_time_usec*2,
			                     "RS-485");
		}
#endif /* RS485 */
		info->last_tx_active_usec = GET_JIFFIES_USEC();
		info->last_tx_active = jiffies;
		e100_disable_serial_tx_ready_irq(info);
		info->tr_running = 0;
		DFLOW(DEBUG_LOG(info->line, "tx_int: stop2\n", 0));
	} else {
		/* We must enable since it is disabled in ser_interrupt */
		e100_enable_serial_tx_ready_irq(info);
	}
	local_irq_restore(flags);

	if (CIRC_CNT(info->xmit.head,
		     info->xmit.tail,
		     SERIAL_XMIT_SIZE) < WAKEUP_CHARS)
		rs_sched_event(info, RS_EVENT_WRITE_WAKEUP);

} /* handle_ser_tx_interrupt */

/* result of time measurements:
 * RX duration 54-60 us when doing something, otherwise 6-9 us
 * ser_int duration: just sending: 8-15 us normally, up to 73 us
 */
static irqreturn_t
ser_interrupt(int irq, void *dev_id)
{
	static volatile int tx_started = 0;
	struct e100_serial *info;
	int i;
	unsigned long flags;
	unsigned long irq_mask1_rd;
	unsigned long data_mask = (1 << (8+2*0)); /* ser0 data_avail */
	int handled = 0;
	static volatile unsigned long reentered_ready_mask = 0;

	local_irq_save(flags);
	irq_mask1_rd = *R_IRQ_MASK1_RD;
	/* First handle all rx interrupts with ints disabled */
	info = rs_table;
	irq_mask1_rd &= e100_ser_int_mask;
	for (i = 0; i < NR_PORTS; i++) {
		/* Which line caused the data irq? */
		if (irq_mask1_rd & data_mask) {
			handled = 1;
			handle_ser_rx_interrupt(info);
		}
		info += 1;
		data_mask <<= 2;
	}
	/* Handle tx interrupts with interrupts enabled so we
	 * can take care of new data interrupts while transmitting
	 * We protect the tx part with the tx_started flag.
	 * We disable the tr_ready interrupts we are about to handle and
	 * unblock the serial interrupt so new serial interrupts may come.
	 *
	 * If we get a new interrupt:
	 *  - it migth be due to synchronous serial ports.
	 *  - serial irq will be blocked by general irq handler.
	 *  - async data will be handled above (sync will be ignored).
	 *  - tx_started flag will prevent us from trying to send again and
	 *    we will exit fast - no need to unblock serial irq.
	 *  - Next (sync) serial interrupt handler will be runned with
	 *    disabled interrupt due to restore_flags() at end of function,
	 *    so sync handler will not be preempted or reentered.
	 */
	if (!tx_started) {
		unsigned long ready_mask;
		unsigned long
		tx_started = 1;
		/* Only the tr_ready interrupts left */
		irq_mask1_rd &= (IO_MASK(R_IRQ_MASK1_RD, ser0_ready) |
				 IO_MASK(R_IRQ_MASK1_RD, ser1_ready) |
				 IO_MASK(R_IRQ_MASK1_RD, ser2_ready) |
				 IO_MASK(R_IRQ_MASK1_RD, ser3_ready));
		while (irq_mask1_rd) {
			/* Disable those we are about to handle */
			*R_IRQ_MASK1_CLR = irq_mask1_rd;
			/* Unblock the serial interrupt */
			*R_VECT_MASK_SET = IO_STATE(R_VECT_MASK_SET, serial, set);

			local_irq_enable();
			ready_mask = (1 << (8+1+2*0)); /* ser0 tr_ready */
			info = rs_table;
			for (i = 0; i < NR_PORTS; i++) {
				/* Which line caused the ready irq? */
				if (irq_mask1_rd & ready_mask) {
					handled = 1;
					handle_ser_tx_interrupt(info);
				}
				info += 1;
				ready_mask <<= 2;
			}
			/* handle_ser_tx_interrupt enables tr_ready interrupts */
			local_irq_disable();
			/* Handle reentered TX interrupt */
			irq_mask1_rd = reentered_ready_mask;
		}
		local_irq_disable();
		tx_started = 0;
	} else {
		unsigned long ready_mask;
		ready_mask = irq_mask1_rd & (IO_MASK(R_IRQ_MASK1_RD, ser0_ready) |
					     IO_MASK(R_IRQ_MASK1_RD, ser1_ready) |
					     IO_MASK(R_IRQ_MASK1_RD, ser2_ready) |
					     IO_MASK(R_IRQ_MASK1_RD, ser3_ready));
		if (ready_mask) {
			reentered_ready_mask |= ready_mask;
			/* Disable those we are about to handle */
			*R_IRQ_MASK1_CLR = ready_mask;
			DFLOW(DEBUG_LOG(SERIAL_DEBUG_LINE, "ser_int reentered with TX %X\n", ready_mask));
		}
	}

	local_irq_restore(flags);
	return IRQ_RETVAL(handled);
} /* ser_interrupt */
#endif

/*
 * -------------------------------------------------------------------
 * Here ends the serial interrupt routines.
 * -------------------------------------------------------------------
 */

/*
 * This routine is used to handle the "bottom half" processing for the
 * serial driver, known also the "software interrupt" processing.
 * This processing is done at the kernel interrupt level, after the
 * rs_interrupt() has returned, BUT WITH INTERRUPTS TURNED ON.  This
 * is where time-consuming activities which can not be done in the
 * interrupt driver proper are done; the interrupt driver schedules
 * them using rs_sched_event(), and they get done here.
 */
static void
do_softint(struct work_struct *work)
{
	struct e100_serial	*info;
	struct tty_struct	*tty;

	info = container_of(work, struct e100_serial, work);

	tty = info->port.tty;
	if (!tty)
		return;

	if (test_and_clear_bit(RS_EVENT_WRITE_WAKEUP, &info->event))
		tty_wakeup(tty);
}

static int
startup(struct e100_serial * info)
{
	unsigned long flags;
	unsigned long xmit_page;
	int i;

	xmit_page = get_zeroed_page(GFP_KERNEL);
	if (!xmit_page)
		return -ENOMEM;

	local_irq_save(flags);

	/* if it was already initialized, skip this */

	if (info->flags & ASYNC_INITIALIZED) {
		local_irq_restore(flags);
		free_page(xmit_page);
		return 0;
	}

	if (info->xmit.buf)
		free_page(xmit_page);
	else
		info->xmit.buf = (unsigned char *) xmit_page;

#ifdef SERIAL_DEBUG_OPEN
	printk("starting up ttyS%d (xmit_buf 0x%p)...\n", info->line, info->xmit.buf);
#endif

#ifdef CONFIG_SVINTO_SIM
	/* Bits and pieces collected from below.  Better to have them
	   in one ifdef:ed clause than to mix in a lot of ifdefs,
	   right? */
	if (info->port.tty)
		clear_bit(TTY_IO_ERROR, &info->port.tty->flags);

	info->xmit.head = info->xmit.tail = 0;
	info->first_recv_buffer = info->last_recv_buffer = NULL;
	info->recv_cnt = info->max_recv_cnt = 0;

	for (i = 0; i < SERIAL_RECV_DESCRIPTORS; i++)
		info->rec_descr[i].buf = NULL;

	/* No real action in the simulator, but may set info important
	   to ioctl. */
	change_speed(info);
#else

	/*
	 * Clear the FIFO buffers and disable them
	 * (they will be reenabled in change_speed())
	 */

	/*
	 * Reset the DMA channels and make sure their interrupts are cleared
	 */

	if (info->dma_in_enabled) {
		info->uses_dma_in = 1;
		e100_enable_rxdma_channel(info);

		*info->icmdadr = IO_STATE(R_DMA_CH6_CMD, cmd, reset);

		/* Wait until reset cycle is complete */
		while (IO_EXTRACT(R_DMA_CH6_CMD, cmd, *info->icmdadr) ==
		       IO_STATE_VALUE(R_DMA_CH6_CMD, cmd, reset));

		/* Make sure the irqs are cleared */
		*info->iclrintradr =
			IO_STATE(R_DMA_CH6_CLR_INTR, clr_descr, do) |
			IO_STATE(R_DMA_CH6_CLR_INTR, clr_eop, do);
	} else {
		e100_disable_rxdma_channel(info);
	}

	if (info->dma_out_enabled) {
		info->uses_dma_out = 1;
		e100_enable_txdma_channel(info);
		*info->ocmdadr = IO_STATE(R_DMA_CH6_CMD, cmd, reset);

		while (IO_EXTRACT(R_DMA_CH6_CMD, cmd, *info->ocmdadr) ==
		       IO_STATE_VALUE(R_DMA_CH6_CMD, cmd, reset));

		/* Make sure the irqs are cleared */
		*info->oclrintradr =
			IO_STATE(R_DMA_CH6_CLR_INTR, clr_descr, do) |
			IO_STATE(R_DMA_CH6_CLR_INTR, clr_eop, do);
	} else {
		e100_disable_txdma_channel(info);
	}

	if (info->port.tty)
		clear_bit(TTY_IO_ERROR, &info->port.tty->flags);

	info->xmit.head = info->xmit.tail = 0;
	info->first_recv_buffer = info->last_recv_buffer = NULL;
	info->recv_cnt = info->max_recv_cnt = 0;

	for (i = 0; i < SERIAL_RECV_DESCRIPTORS; i++)
		info->rec_descr[i].buf = 0;

	/*
	 * and set the speed and other flags of the serial port
	 * this will start the rx/tx as well
	 */
#ifdef SERIAL_HANDLE_EARLY_ERRORS
	e100_enable_serial_data_irq(info);
#endif
	change_speed(info);

	/* dummy read to reset any serial errors */

	(void)info->ioport[REG_DATA];

	/* enable the interrupts */
	if (info->uses_dma_out)
		e100_enable_txdma_irq(info);

	e100_enable_rx_irq(info);

	info->tr_running = 0; /* to be sure we don't lock up the transmitter */

	/* setup the dma input descriptor and start dma */

	start_receive(info);

	/* for safety, make sure the descriptors last result is 0 bytes written */

	info->tr_descr.sw_len = 0;
	info->tr_descr.hw_len = 0;
	info->tr_descr.status = 0;

	/* enable RTS/DTR last */

	e100_rts(info, 1);
	e100_dtr(info, 1);

#endif /* CONFIG_SVINTO_SIM */

	info->flags |= ASYNC_INITIALIZED;

	local_irq_restore(flags);
	return 0;
}

/*
 * This routine will shutdown a serial port; interrupts are disabled, and
 * DTR is dropped if the hangup on close termio flag is on.
 */
static void
shutdown(struct e100_serial * info)
{
	unsigned long flags;
	struct etrax_dma_descr *descr = info->rec_descr;
	struct etrax_recv_buffer *buffer;
	int i;

#ifndef CONFIG_SVINTO_SIM
	/* shut down the transmitter and receiver */
	DFLOW(DEBUG_LOG(info->line, "shutdown %i\n", info->line));
	e100_disable_rx(info);
	info->ioport[REG_TR_CTRL] = (info->tx_ctrl &= ~0x40);

	/* disable interrupts, reset dma channels */
	if (info->uses_dma_in) {
		e100_disable_rxdma_irq(info);
		*info->icmdadr = IO_STATE(R_DMA_CH6_CMD, cmd, reset);
		info->uses_dma_in = 0;
	} else {
		e100_disable_serial_data_irq(info);
	}

	if (info->uses_dma_out) {
		e100_disable_txdma_irq(info);
		info->tr_running = 0;
		*info->ocmdadr = IO_STATE(R_DMA_CH6_CMD, cmd, reset);
		info->uses_dma_out = 0;
	} else {
		e100_disable_serial_tx_ready_irq(info);
		info->tr_running = 0;
	}

#endif /* CONFIG_SVINTO_SIM */

	if (!(info->flags & ASYNC_INITIALIZED))
		return;

#ifdef SERIAL_DEBUG_OPEN
	printk("Shutting down serial port %d (irq %d)....\n", info->line,
	       info->irq);
#endif

	local_irq_save(flags);

	if (info->xmit.buf) {
		free_page((unsigned long)info->xmit.buf);
		info->xmit.buf = NULL;
	}

	for (i = 0; i < SERIAL_RECV_DESCRIPTORS; i++)
		if (descr[i].buf) {
			buffer = phys_to_virt(descr[i].buf) - sizeof *buffer;
			kfree(buffer);
			descr[i].buf = 0;
		}

	if (!info->port.tty || (info->port.tty->termios->c_cflag & HUPCL)) {
		/* hang up DTR and RTS if HUPCL is enabled */
		e100_dtr(info, 0);
		e100_rts(info, 0); /* could check CRTSCTS before doing this */
	}

	if (info->port.tty)
		set_bit(TTY_IO_ERROR, &info->port.tty->flags);

	info->flags &= ~ASYNC_INITIALIZED;
	local_irq_restore(flags);
}


/* change baud rate and other assorted parameters */

static void
change_speed(struct e100_serial *info)
{
	unsigned int cflag;
	unsigned long xoff;
	unsigned long flags;
	/* first some safety checks */

	if (!info->port.tty || !info->port.tty->termios)
		return;
	if (!info->ioport)
		return;

	cflag = info->port.tty->termios->c_cflag;

	/* possibly, the tx/rx should be disabled first to do this safely */

	/* change baud-rate and write it to the hardware */
	if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_CUST) {
		/* Special baudrate */
		u32 mask = 0xFF << (info->line*8); /* Each port has 8 bits */
		unsigned long alt_source =
				IO_STATE(R_ALT_SER_BAUDRATE, ser0_rec, normal) |
				IO_STATE(R_ALT_SER_BAUDRATE, ser0_tr, normal);
		/* R_ALT_SER_BAUDRATE selects the source */
		DBAUD(printk("Custom baudrate: baud_base/divisor %lu/%i\n",
		       (unsigned long)info->baud_base, info->custom_divisor));
		if (info->baud_base == SERIAL_PRESCALE_BASE) {
			/* 0, 2-65535 (0=65536) */
			u16 divisor = info->custom_divisor;
			/* R_SERIAL_PRESCALE (upper 16 bits of R_CLOCK_PRESCALE) */
			/* baudrate is 3.125MHz/custom_divisor */
			alt_source =
				IO_STATE(R_ALT_SER_BAUDRATE, ser0_rec, prescale) |
				IO_STATE(R_ALT_SER_BAUDRATE, ser0_tr, prescale);
			alt_source = 0x11;
			DBAUD(printk("Writing SERIAL_PRESCALE: divisor %i\n", divisor));
			*R_SERIAL_PRESCALE = divisor;
			info->baud = SERIAL_PRESCALE_BASE/divisor;
		}
#ifdef CONFIG_ETRAX_EXTERN_PB6CLK_ENABLED
		else if ((info->baud_base==CONFIG_ETRAX_EXTERN_PB6CLK_FREQ/8 &&
			  info->custom_divisor == 1) ||
			 (info->baud_base==CONFIG_ETRAX_EXTERN_PB6CLK_FREQ &&
			  info->custom_divisor == 8)) {
				/* ext_clk selected */
				alt_source =
					IO_STATE(R_ALT_SER_BAUDRATE, ser0_rec, extern) |
					IO_STATE(R_ALT_SER_BAUDRATE, ser0_tr, extern);
				DBAUD(printk("using external baudrate: %lu\n", CONFIG_ETRAX_EXTERN_PB6CLK_FREQ/8));
				info->baud = CONFIG_ETRAX_EXTERN_PB6CLK_FREQ/8;
			}
#endif
		else
		{
			/* Bad baudbase, we don't support using timer0
			 * for baudrate.
			 */
			printk(KERN_WARNING "Bad baud_base/custom_divisor: %lu/%i\n",
			       (unsigned long)info->baud_base, info->custom_divisor);
		}
		r_alt_ser_baudrate_shadow &= ~mask;
		r_alt_ser_baudrate_shadow |= (alt_source << (info->line*8));
		*R_ALT_SER_BAUDRATE = r_alt_ser_baudrate_shadow;
	} else {
		/* Normal baudrate */
		/* Make sure we use normal baudrate */
		u32 mask = 0xFF << (info->line*8); /* Each port has 8 bits */
		unsigned long alt_source =
			IO_STATE(R_ALT_SER_BAUDRATE, ser0_rec, normal) |
			IO_STATE(R_ALT_SER_BAUDRATE, ser0_tr, normal);
		r_alt_ser_baudrate_shadow &= ~mask;
		r_alt_ser_baudrate_shadow |= (alt_source << (info->line*8));
#ifndef CONFIG_SVINTO_SIM
		*R_ALT_SER_BAUDRATE = r_alt_ser_baudrate_shadow;
#endif /* CONFIG_SVINTO_SIM */

		info->baud = cflag_to_baud(cflag);
#ifndef CONFIG_SVINTO_SIM
		info->ioport[REG_BAUD] = cflag_to_etrax_baud(cflag);
#endif /* CONFIG_SVINTO_SIM */
	}

#ifndef CONFIG_SVINTO_SIM
	/* start with default settings and then fill in changes */
	local_irq_save(flags);
	/* 8 bit, no/even parity */
	info->rx_ctrl &= ~(IO_MASK(R_SERIAL0_REC_CTRL, rec_bitnr) |
			   IO_MASK(R_SERIAL0_REC_CTRL, rec_par_en) |
			   IO_MASK(R_SERIAL0_REC_CTRL, rec_par));

	/* 8 bit, no/even parity, 1 stop bit, no cts */
	info->tx_ctrl &= ~(IO_MASK(R_SERIAL0_TR_CTRL, tr_bitnr) |
			   IO_MASK(R_SERIAL0_TR_CTRL, tr_par_en) |
			   IO_MASK(R_SERIAL0_TR_CTRL, tr_par) |
			   IO_MASK(R_SERIAL0_TR_CTRL, stop_bits) |
			   IO_MASK(R_SERIAL0_TR_CTRL, auto_cts));

	if ((cflag & CSIZE) == CS7) {
		/* set 7 bit mode */
		info->tx_ctrl |= IO_STATE(R_SERIAL0_TR_CTRL, tr_bitnr, tr_7bit);
		info->rx_ctrl |= IO_STATE(R_SERIAL0_REC_CTRL, rec_bitnr, rec_7bit);
	}

	if (cflag & CSTOPB) {
		/* set 2 stop bit mode */
		info->tx_ctrl |= IO_STATE(R_SERIAL0_TR_CTRL, stop_bits, two_bits);
	}

	if (cflag & PARENB) {
		/* enable parity */
		info->tx_ctrl |= IO_STATE(R_SERIAL0_TR_CTRL, tr_par_en, enable);
		info->rx_ctrl |= IO_STATE(R_SERIAL0_REC_CTRL, rec_par_en, enable);
	}

	if (cflag & CMSPAR) {
		/* enable stick parity, PARODD mean Mark which matches ETRAX */
		info->tx_ctrl |= IO_STATE(R_SERIAL0_TR_CTRL, tr_stick_par, stick);
		info->rx_ctrl |= IO_STATE(R_SERIAL0_REC_CTRL, rec_stick_par, stick);
	}
	if (cflag & PARODD) {
		/* set odd parity (or Mark if CMSPAR) */
		info->tx_ctrl |= IO_STATE(R_SERIAL0_TR_CTRL, tr_par, odd);
		info->rx_ctrl |= IO_STATE(R_SERIAL0_REC_CTRL, rec_par, odd);
	}

	if (cflag & CRTSCTS) {
		/* enable automatic CTS handling */
		DFLOW(DEBUG_LOG(info->line, "FLOW auto_cts enabled\n", 0));
		info->tx_ctrl |= IO_STATE(R_SERIAL0_TR_CTRL, auto_cts, active);
	}

	/* make sure the tx and rx are enabled */

	info->tx_ctrl |= IO_STATE(R_SERIAL0_TR_CTRL, tr_enable, enable);
	info->rx_ctrl |= IO_STATE(R_SERIAL0_REC_CTRL, rec_enable, enable);

	/* actually write the control regs to the hardware */

	info->ioport[REG_TR_CTRL] = info->tx_ctrl;
	info->ioport[REG_REC_CTRL] = info->rx_ctrl;
	xoff = IO_FIELD(R_SERIAL0_XOFF, xoff_char, STOP_CHAR(info->port.tty));
	xoff |= IO_STATE(R_SERIAL0_XOFF, tx_stop, enable);
	if (info->port.tty->termios->c_iflag & IXON ) {
		DFLOW(DEBUG_LOG(info->line, "FLOW XOFF enabled 0x%02X\n",
				STOP_CHAR(info->port.tty)));
		xoff |= IO_STATE(R_SERIAL0_XOFF, auto_xoff, enable);
	}

	*((unsigned long *)&info->ioport[REG_XOFF]) = xoff;
	local_irq_restore(flags);
#endif /* !CONFIG_SVINTO_SIM */

	update_char_time(info);

} /* change_speed */

/* start transmitting chars NOW */

static void
rs_flush_chars(struct tty_struct *tty)
{
	struct e100_serial *info = (struct e100_serial *)tty->driver_data;
	unsigned long flags;

	if (info->tr_running ||
	    info->xmit.head == info->xmit.tail ||
	    tty->stopped ||
	    tty->hw_stopped ||
	    !info->xmit.buf)
		return;

#ifdef SERIAL_DEBUG_FLOW
	printk("rs_flush_chars\n");
#endif

	/* this protection might not exactly be necessary here */

	local_irq_save(flags);
	start_transmit(info);
	local_irq_restore(flags);
}

static int rs_raw_write(struct tty_struct *tty,
			const unsigned char *buf, int count)
{
	int	c, ret = 0;
	struct e100_serial *info = (struct e100_serial *)tty->driver_data;
	unsigned long flags;

	/* first some sanity checks */

	if (!tty || !info->xmit.buf || !tmp_buf)
		return 0;

#ifdef SERIAL_DEBUG_DATA
	if (info->line == SERIAL_DEBUG_LINE)
		printk("rs_raw_write (%d), status %d\n",
		       count, info->ioport[REG_STATUS]);
#endif

#ifdef CONFIG_SVINTO_SIM
	/* Really simple.  The output is here and now. */
	SIMCOUT(buf, count);
	return count;
#endif
	local_save_flags(flags);
	DFLOW(DEBUG_LOG(info->line, "write count %i ", count));
	DFLOW(DEBUG_LOG(info->line, "ldisc %i\n", tty->ldisc.chars_in_buffer(tty)));


	/* The local_irq_disable/restore_flags pairs below are needed
	 * because the DMA interrupt handler moves the info->xmit values.
	 * the memcpy needs to be in the critical region unfortunately,
	 * because we need to read xmit values, memcpy, write xmit values
	 * in one atomic operation... this could perhaps be avoided by
	 * more clever design.
	 */
	local_irq_disable();
		while (count) {
			c = CIRC_SPACE_TO_END(info->xmit.head,
					      info->xmit.tail,
					      SERIAL_XMIT_SIZE);

			if (count < c)
				c = count;
			if (c <= 0)
				break;

			memcpy(info->xmit.buf + info->xmit.head, buf, c);
			info->xmit.head = (info->xmit.head + c) &
				(SERIAL_XMIT_SIZE-1);
			buf += c;
			count -= c;
			ret += c;
		}
	local_irq_restore(flags);

	/* enable transmitter if not running, unless the tty is stopped
	 * this does not need IRQ protection since if tr_running == 0
	 * the IRQ's are not running anyway for this port.
	 */
	DFLOW(DEBUG_LOG(info->line, "write ret %i\n", ret));

	if (info->xmit.head != info->xmit.tail &&
	    !tty->stopped &&
	    !tty->hw_stopped &&
	    !info->tr_running) {
		start_transmit(info);
	}

	return ret;
} /* raw_raw_write() */

static int
rs_write(struct tty_struct *tty,
	 const unsigned char *buf, int count)
{
#if defined(CONFIG_ETRAX_RS485)
	struct e100_serial *info = (struct e100_serial *)tty->driver_data;

	if (info->rs485.flags & SER_RS485_ENABLED)
	{
		/* If we are in RS-485 mode, we need to toggle RTS and disable
		 * the receiver before initiating a DMA transfer
		 */
#ifdef CONFIG_ETRAX_FAST_TIMER
		/* Abort any started timer */
		fast_timers_rs485[info->line].function = NULL;
		del_fast_timer(&fast_timers_rs485[info->line]);
#endif
		e100_rts(info, (info->rs485.flags & SER_RS485_RTS_ON_SEND));
#if defined(CONFIG_ETRAX_RS485_DISABLE_RECEIVER)
		e100_disable_rx(info);
		e100_enable_rx_irq(info);
#endif

		if (info->rs485.delay_rts_before_send > 0)
			msleep(info->rs485.delay_rts_before_send);
	}
#endif /* CONFIG_ETRAX_RS485 */

	count = rs_raw_write(tty, buf, count);

#if defined(CONFIG_ETRAX_RS485)
	if (info->rs485.flags & SER_RS485_ENABLED)
	{
		unsigned int val;
		/* If we are in RS-485 mode the following has to be done:
		 * wait until DMA is ready
		 * wait on transmit shift register
		 * toggle RTS
		 * enable the receiver
		 */

		/* Sleep until all sent */
		tty_wait_until_sent(tty, 0);
#ifdef CONFIG_ETRAX_FAST_TIMER
		/* Now sleep a little more so that shift register is empty */
		schedule_usleep(info->char_time_usec * 2);
#endif
		/* wait on transmit shift register */
		do{
			get_lsr_info(info, &val);
		}while (!(val & TIOCSER_TEMT));

		e100_rts(info, (info->rs485.flags & SER_RS485_RTS_AFTER_SEND));

#if defined(CONFIG_ETRAX_RS485_DISABLE_RECEIVER)
		e100_enable_rx(info);
		e100_enable_rxdma_irq(info);
#endif
	}
#endif /* CONFIG_ETRAX_RS485 */

	return count;
} /* rs_write */


/* how much space is available in the xmit buffer? */

static int
rs_write_room(struct tty_struct *tty)
{
	struct e100_serial *info = (struct e100_serial *)tty->driver_data;

	return CIRC_SPACE(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE);
}

/* How many chars are in the xmit buffer?
 * This does not include any chars in the transmitter FIFO.
 * Use wait_until_sent for waiting for FIFO drain.
 */

static int
rs_chars_in_buffer(struct tty_struct *tty)
{
	struct e100_serial *info = (struct e100_serial *)tty->driver_data;

	return CIRC_CNT(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE);
}

/* discard everything in the xmit buffer */

static void
rs_flush_buffer(struct tty_struct *tty)
{
	struct e100_serial *info = (struct e100_serial *)tty->driver_data;
	unsigned long flags;

	local_irq_save(flags);
	info->xmit.head = info->xmit.tail = 0;
	local_irq_restore(flags);

	tty_wakeup(tty);
}

/*
 * This function is used to send a high-priority XON/XOFF character to
 * the device
 *
 * Since we use DMA we don't check for info->x_char in transmit_chars_dma(),
 * but we do it in handle_ser_tx_interrupt().
 * We disable DMA channel and enable tx ready interrupt and write the
 * character when possible.
 */
static void rs_send_xchar(struct tty_struct *tty, char ch)
{
	struct e100_serial *info = (struct e100_serial *)tty->driver_data;
	unsigned long flags;
	local_irq_save(flags);
	if (info->uses_dma_out) {
		/* Put the DMA on hold and disable the channel */
		*info->ocmdadr = IO_STATE(R_DMA_CH6_CMD, cmd, hold);
		while (IO_EXTRACT(R_DMA_CH6_CMD, cmd, *info->ocmdadr) !=
		       IO_STATE_VALUE(R_DMA_CH6_CMD, cmd, hold));
		e100_disable_txdma_channel(info);
	}

	/* Must make sure transmitter is not stopped before we can transmit */
	if (tty->stopped)
		rs_start(tty);

	/* Enable manual transmit interrupt and send from there */
	DFLOW(DEBUG_LOG(info->line, "rs_send_xchar 0x%02X\n", ch));
	info->x_char = ch;
	e100_enable_serial_tx_ready_irq(info);
	local_irq_restore(flags);
}

/*
 * ------------------------------------------------------------
 * rs_throttle()
 *
 * This routine is called by the upper-layer tty layer to signal that
 * incoming characters should be throttled.
 * ------------------------------------------------------------
 */
static void
rs_throttle(struct tty_struct * tty)
{
	struct e100_serial *info = (struct e100_serial *)tty->driver_data;
#ifdef SERIAL_DEBUG_THROTTLE
	char	buf[64];

	printk("throttle %s: %lu....\n", tty_name(tty, buf),
	       (unsigned long)tty->ldisc.chars_in_buffer(tty));
#endif
	DFLOW(DEBUG_LOG(info->line,"rs_throttle %lu\n", tty->ldisc.chars_in_buffer(tty)));

	/* Do RTS before XOFF since XOFF might take some time */
	if (tty->termios->c_cflag & CRTSCTS) {
		/* Turn off RTS line */
		e100_rts(info, 0);
	}
	if (I_IXOFF(tty))
		rs_send_xchar(tty, STOP_CHAR(tty));

}

static void
rs_unthrottle(struct tty_struct * tty)
{
	struct e100_serial *info = (struct e100_serial *)tty->driver_data;
#ifdef SERIAL_DEBUG_THROTTLE
	char	buf[64];

	printk("unthrottle %s: %lu....\n", tty_name(tty, buf),
	       (unsigned long)tty->ldisc.chars_in_buffer(tty));
#endif
	DFLOW(DEBUG_LOG(info->line,"rs_unthrottle ldisc %d\n", tty->ldisc.chars_in_buffer(tty)));
	DFLOW(DEBUG_LOG(info->line,"rs_unthrottle flip.count: %i\n", tty->flip.count));
	/* Do RTS before XOFF since XOFF might take some time */
	if (tty->termios->c_cflag & CRTSCTS) {
		/* Assert RTS line  */
		e100_rts(info, 1);
	}

	if (I_IXOFF(tty)) {
		if (info->x_char)
			info->x_char = 0;
		else
			rs_send_xchar(tty, START_CHAR(tty));
	}

}

/*
 * ------------------------------------------------------------
 * rs_ioctl() and friends
 * ------------------------------------------------------------
 */

static int
get_serial_info(struct e100_serial * info,
		struct serial_struct * retinfo)
{
	struct serial_struct tmp;

	/* this is all probably wrong, there are a lot of fields
	 * here that we don't have in e100_serial and maybe we
	 * should set them to something else than 0.
	 */

	if (!retinfo)
		return -EFAULT;
	memset(&tmp, 0, sizeof(tmp));
	tmp.type = info->type;
	tmp.line = info->line;
	tmp.port = (int)info->ioport;
	tmp.irq = info->irq;
	tmp.flags = info->flags;
	tmp.baud_base = info->baud_base;
	tmp.close_delay = info->close_delay;
	tmp.closing_wait = info->closing_wait;
	tmp.custom_divisor = info->custom_divisor;
	if (copy_to_user(retinfo, &tmp, sizeof(*retinfo)))
		return -EFAULT;
	return 0;
}

static int
set_serial_info(struct e100_serial *info,
		struct serial_struct *new_info)
{
	struct serial_struct new_serial;
	struct e100_serial old_info;
	int retval = 0;

	if (copy_from_user(&new_serial, new_info, sizeof(new_serial)))
		return -EFAULT;

	old_info = *info;

	if (!capable(CAP_SYS_ADMIN)) {
		if ((new_serial.type != info->type) ||
		    (new_serial.close_delay != info->close_delay) ||
		    ((new_serial.flags & ~ASYNC_USR_MASK) !=
		     (info->flags & ~ASYNC_USR_MASK)))
			return -EPERM;
		info->flags = ((info->flags & ~ASYNC_USR_MASK) |
			       (new_serial.flags & ASYNC_USR_MASK));
		goto check_and_exit;
	}

	if (info->count > 1)
		return -EBUSY;

	/*
	 * OK, past this point, all the error checking has been done.
	 * At this point, we start making changes.....
	 */

	info->baud_base = new_serial.baud_base;
	info->flags = ((info->flags & ~ASYNC_FLAGS) |
		       (new_serial.flags & ASYNC_FLAGS));
	info->custom_divisor = new_serial.custom_divisor;
	info->type = new_serial.type;
	info->close_delay = new_serial.close_delay;
	info->closing_wait = new_serial.closing_wait;
	info->port.tty->low_latency = (info->flags & ASYNC_LOW_LATENCY) ? 1 : 0;

 check_and_exit:
	if (info->flags & ASYNC_INITIALIZED) {
		change_speed(info);
	} else
		retval = startup(info);
	return retval;
}

/*
 * get_lsr_info - get line status register info
 *
 * Purpose: Let user call ioctl() to get info when the UART physically
 * 	    is emptied.  On bus types like RS485, the transmitter must
 * 	    release the bus after transmitting. This must be done when
 * 	    the transmit shift register is empty, not be done when the
 * 	    transmit holding register is empty.  This functionality
 * 	    allows an RS485 driver to be written in user space.
 */
static int
get_lsr_info(struct e100_serial * info, unsigned int *value)
{
	unsigned int result = TIOCSER_TEMT;
#ifndef CONFIG_SVINTO_SIM
	unsigned long curr_time = jiffies;chip
 *
 *    Copyright (_usec = GET_JIFFIES_USEC()07  Axis Communicelapsed
 *
 * 
		(pyright (C- info->last_tx_active) * 10erial/HZ +
		ations AB
 *
 *x50.
 *
 */

static ch
 *
 ;

	if (0.
 *
xmit.head !=50.
 *
e <litail ||
	   pon a time on < 2*0.
 *
chaons AB
 *
 ) {
		res/*
 * 0;
	}
#endifes.h>
#copy_to
 *
r(value, &inux/t, sizeof(int))) <linturn -EFAULT;
de <linu0;
}
rive forSERIAL_DEBUG_IO
struct state_str
{
	innux/smp;
	const sche *str;
};

inux/se <linux/smp_loc control_x/smp_loc[] =de <{SeriM_DTR, "DTR" },ude <linuRTS, "RTS"include <liST, "ST?
#include <liSslabSRkernel.h>
#incCux/inCTSnux/mutex.h>
#DcludD
#include <linI/iniI
#include <liDlude DS>
#inclu0, NULL }includring.get_lude <linux/mm.h>h>
# MLines,string.h)k.h>
#iniimer.h
	s[0]='\0';
	whilenux/de <linux/mm.h>
i].#inc!=incluude <lh>
#cess.h &clude <linux/mm.h>
h>
#iateude <lstem.ncluclude <an-arch 	strcat(s, ", "ors.		}/serres are in#include <asm/dma.h>
#inux/seial.i++.h>
#de <linusludeinclude /* nic int
rs_break(e <linutty_loclinu*tty,al)   locainux/me <li
#inclue100_serial <linu = l .h filtimer.h>
#inc)tty->driver_data07  Axis Communicflagspes.h>
#!0.
 *
ioportclude <linux/IO

#ilocal_irq_save(T_TIMors.h>
#0.h"
#inclu == -1ude <l/* Go to manual mode and set the txd pinndif0 */endif
Clear bit 7 (txd)ed(CO6 (tr_enablhar IMEO0.
 *
tx_ctrl &= 0x3F.h>
 else
#endif
Set) && \
           (CONFIG_ETRAX_SERIAL_RX_TIMEOUT_|= (0x80 | 0x40ors.}h>
#CONFIG_ETR[REG_TR_CTRL#incAL_RX_TIMEOUT;ror "Enable restorT_TIMER to .h>
#include 0_serial) in atiocmsetl .h file */
#include "cr
#inclufq.h>*
/*
,
	hip
 *
 * 
#incet, the compatibicICKSde <asm/fasttimer.h>
#include <arch/io_interface_mux.h>

#ifdef CONFIG_ETRAX_SERIAL_FAST_TIMER
#r "Enable FAST_TIMER to <linuxICKS)& Serilinuxcludtimerrts#incl, RAX_R.h"
#endif

structDTRy_driver dnux/ul_driver;/* Handle FEMALE behaviourX_SER.h"
#endif

struct Iy_driver *i_outrial_driver;

/* number of chaCDy_driver cd SERIAL_DEBUG_Iefine NFIG

struct tty_driver *serial_dr1ver;

/*G_FLOW
//#deracters left in xmit b
//#d before we ask for more */
#define G_FLOW
//#def6

//#define SERIAL_DEB
//#define SERIAL_DEBL_DEBUG_OPEN
//#define S
//#PORT_G)
#error "Disable either CONFIG_ETRAX_RS485_ON_PA or COgFIG_ETRAX_RS485_ON_PORT_G"
#endif

/*
 * Allde <asm/fasttimer.h>
#include <arch/io_interface_mux.h>

#ifdef CONFIG_ETRAX_SERIAisv1inux/t
#if defined(LOCAL_HEADERS)
#include "serial_compatinux/timeria!EimerRTS_GET xmit) ?
struct tt : 0clud| iptors DTR28 bytes = 2048 bytDTR*/
#define SERIALRI28 bytes = 2048 byteNG*/
#define SERIAL_SESCR_BUF_SIZE 256

#dSfine SERIAL_PRESCACD28 bytes = 2048 bytCAALE_BASE

/* We don 128 bytes = 2048 bytCs */
#dug */

/* Enable this to use s <linux/major.h>
#includ	printk(KERNh>
#in "ser%i:definmux/smp: %i 0x%08X\n" of 0.
 *
 ine,m eLinuf */
#deors.e <lux/mos[100]ug *	ule.h>
#include <linuxde <linuxux/se_FLUSH_TIME_USEC 250 here tslot ine DIstruct e1er CONFI eLinux ude AX_RS485_ON_PA ioctlG_ETRAX_RS485_ON_PORT_G"
#endif

/*
 *

/*
,
	code so we can md code so we   Coargde <asm/fasttimer.h>
#incETRAX <arch/io_interface_mux.h>

#ifdef CONFIG_Eefine (cm/errnSeriGmajor.) && INCLUDED
#ifSdef DEBUG_ux/sigOG_INCLUDED
#ER the EBUG_LOG_INCLUDED
#ERGWILD) DEBUG_LOG(line, string, Sog_infelse
struct debug_lSTRUCT)sm/system.>

#iT_TIM & (1 << TTY_IO_ERRORncludFAST_TIMER
#er	}
#inwitchG(linude <caseD
#ifndef DE:lude <linuule..h>
#i_xmit xmit ructu/sigLOG(e <linuxOG_SIZ
#includ)*/
#d <liebug_log#defineebug_log[DEsUG_LOG_SIZE];
int debug_log_pos = 0;

#define DEBUG_LOG(_line, _string, _vERGETLSR: if
#et stufux/smus registe/
#defg_log[DEBUG_lsrZE];
int de ( IRMA
   fromG(_line, tring, _value);;
//  ebuglinux/interrupt.hrch/io_interface_mux.h*/
#ebug_loxmit bx/tty.hch/io_interface_muincludde <linux/tty_fliper CONFIG_Erive defined( the ETETRAX_RS485)tring, _value)SET line:D(x)
//* In this p buf w

voill useG_ETRold
#endifure
		 * rs485.h>
#inc for backward compatibilitye = va(if		deg[deefine Dlue;
,G_ETn_log_upt.-level codue = vawouldn't work anymore...).e = vaTh;*/
}
of = string;
	is deprecated:*/
}
Seria linee = vainstead._SERI
#inclulue;
		debug_llue;
5_ON_POency app#endif

#ifne 0
 *NFIG_ETNTR1(x)  /* irq on/omer ticks before flushing rx fifo
 . Uhen using "lidata, lo\nnux/selinux/intfromrupt.h& 0
 * Whe, = 0;

#dlue;
		debug_l*) jiffies;x/tty.h 0
 * Wher_data = *R_TIMER_DATA;;
//ations (.delay *se_before_send =e 0
 * Whee(struct tty_struct * A;
//_throttle char mer.h>er_da);
static IG_ETRddata =ruct tty_struct|=/maj= line_ENABLEDty, #errnt rs_write(struct t&= ~(struct *tty,
		co(debu int timeout);
t ttonct * int rs_write(struct tty_struct *ttx 12ON_SENonst unsigned char *buf, int count);
#ifdef 	const unsiETRAX_RS485
static int eafterct *RAX_Fs485(struct tty_struct *tty,
		conAFTER unsigned char *buf, int count);
#endif
static int g115200   /ETRAX_e <linutimerIG_ETR

#ifn	concludations (define _string, _vg_log_pos].striT strisG_ETRnew versionks b >> 8 */
/, with_ctre = vaNFIG_pos].valuuch data appli latency appch data applications (PPP)r_data_to_ns(unsigned lonNFIGmer_data);

static void change_speed(struct e10 or Sial *info);
static void rs_SKIP_TEST)
#define DEF_RX 0x20  /* or SERIAL_tring, _value)WRg_log_pos].s_data);

statwrite;

stawrty, int ta_to_ns(unsigned lonwrmer_data);

stat REG_Rchange_speed(struct e10wr is the 32 bit register R_SERIALx_READ REG_ne DEF_RX 0x*/

/* .outcefine SER_RXD__x/ttdefine DINTR2
	defaultebug_log[DE-ENOIOCTLCMonstr own stufG_ETRAX_RS485voidin a ((_termiosG_ETRAX_RS485_ON_PORT_G"
#endif
ka_avail *oldta_availde <asm/fasttimer.h>
#include <arch/io_interface_mux.h>

#ifdef CONFIG_E
/* Dnge_speedytes =, pa before we<lining off CRTS* on#define (ERIAL0_STAT->c_cT_TI &(R_SERIA DEBUG_LOG!	constrrun)

#define SER_ERROR_MAude <l>

#ihw_stoppe ttytty, US, tart	conSERIAL_}

/*
 * -efine ERRCODE_SET_BREAK    (TTY_BREAK)
#define ERRCODE_INSE/
#drs_close()
 */
#dvaluerout0)

is called w CON_ETR.h>
#in_ETREBUGs   0x1d.  First, we/
#dwaitlog_p_ETR */
 remai IO_M defato bE_INnt. imernRCE_ unlink its/
#dSault */
#de_ns(R_SETinterrup/striin if necessary,ed(COwof Ie_EOP(that IRQe:
 *othIO_Mis left RIALheis fil./
#define ERRCODE_SET_BREAK    (TTY_BREAK)
#define ERRCODE_INSERT  /_SERIAL0_STATUS,  0x10er stuff */
#define DFLIP(x)
/* Debug flopdefine DFLOW(x)
#define DBAUD(x)
#define DLOG_INT_TRIG(x)

//#define DEBUTRAX_SERIAL_FAST_TIMER
#ifndef COclude <lin_OVERRUros in ths are disatic )  *R_Sis enti of unctiste*/ERS)
#include "serial_compat.h"
e */hung_up_p( are)ude <lRT_G)
#error "Disable eithe as theseIAL_<linux/major.h>
#incOPENIN_FLUSH_"[%d]      0x1le *S%dludeun
 * %long,opyrient->pid conIRQ_MAtimer stuff linux/s1_RD);e DINTR2(ATUS,>

#iK1_RD, = 1igned ready)
#endclud1/
stati/*e = vaUh, oh. le *RAX_SERIis 1, which meanof taIG_ETRAtring, vult */
#dewg_lob the bdefiIIO_MASK(R_IshT_TI alwayse = vabe oINSEn usiseude disk fsRT2
f it's greaog_fthane = vaouff we've goP = RT |roblemsbug_nce itser1_readue = vaINSERT | TTYwoCKS
eop;hutdownr of tnc(i_FLUSH_TIME_CRITe = ;
unsi"the same: baCONFSERT | TTY
#end;O_MASK(R_IRQ_MASK1"if
;
unsign IO_MASK(R_Iisser0_datready)
#endif
f timer K1_RD, s1X_RS485f (--0 */
/*  DMA< 0ude <lser3_ready)
#endgned long r_alt_ser_baudrate_shad)  *R_IRQ_M:ser0_daif
;
unsigK1_RD, ser0_ready)
#endif
100 */
/*  DMA2(r.h>
#.h>
#includ
#endistatic const unsigned long e100_ser_int_mask timer ruct tty_ASYNC_CLOSINGRIAL_
= vaSavdebug__MASK(R_pos].valu3
| IO_Mrts.
| TTYmay have/* usseparAST__MASK(R_og_p_BREouted(COdialarioK1_RD,h>
#includ char *s      NORMAL_ACTIVEclud5_ON_Pnormalta_avail =ude "N_MASK |  12, /* usNowse tinfo)  *R_SETtransmit buffy)
#on comp;ll use tnotify/* us_SET_0)

discip7_CMDto onlyRIAL * W XON/XOFFstrinacterscmdadr  _MASK(losIO_M2(ser2 = DEF_BAUDLAGS,
_info)!        = 1U <<_WAIT_NON= R_De */info_until, uns_RX 0x     = DEF_RX,
	  ERIAL_/* usAIG_ER_DMAisv1	debuop acceptchannnpu/*
 *o do = ifRCE_E) as e for CODE_INSERT receef Ced(COusinDMAifdef COs too are cmdadr <linux/major.hHANDLE_EARLYvalue;S
s left e for _LOG_SIZNFIGablene SER_Oinclude iver for the ETRAX 100LX ch = SER0_TX_DMrxne SER_O_out_irq_flags =dma_out_irq_     = R_DMA_CH6_CMD,
	  .INITIALIZEDSK1_RD, ser1_dB_strul_0,drop x/slamake s
#deusinUARTr   = R_Dtere = vahas
	/*pletely draug_l; = stris especiallring, vim_ETRant asl_0,R_IN ar   = R_DMFIFO!ASK1_RD, rs     = 2,
	  .dma_owneHZK(R_SERIAL0_STAK(R_IRQ_ IRQF_DISrs_flush_A_CH7_nfo->erre */l,
	 r = SEMA_NBR,
	  D_FLAGS,
	  r.h>5_ON_Peve.baud    5_ON_P_ETR.tty ude <arx_ctrl     =blocked_opensm/system.     = DEFe_(struignedscheduleght (out_ros in thiblel 0 dma rec",
#elseble[wake_RD  0,
	  .dma_in&5_ON_Ption0,
	  .io0_CTRL,
	  .irqcount,
	  .ostatusadr  =|      = 1U <<	  .ma_in_irq_nbr = 0,
	  .dma_in rec",,
	  .ioudrates so limit it to 250 ERRUdrate_)

#dor thlog[debug_log_pos].line = line;
	h>
#includ0  /*f, int c_struct *tty,
		cosm/syst CONFIG_SVINTO_SIMount);
#ifdef CONFIG_ETRlog[debug_log_pos].line = linenst PAclud*R_PORT_PA_DATA =
	  ._pa
	  .dshadow      ring;lue;
	pa_b	  .iinclude_SERIAL1_CTRL,
	  .irq         = 1U* usGcludefinSHADOW_SET(, /* usG DMA ,and 9 g
	  .oclrintebug_;
unsigA_CH8_Cr  = Rbitdriver
	  .ofirstadr   = R_DMA_CH8_FIRST,
	 LTC1387     = R_DMA_CH8_CMD,
	  .ostatusadr  = R_DMA_CH8_STATUS,
	  .ictadr   = R_DMA_CH9_FIRST,
_DXE.ocmdadr_BITdriver;.icmdadr     = R_DMA_CH9_CMD,
	  .idescradr   = R_DMA_CH9_DESCR,
	  .flags       = STD_FRAGS,
	  .rx_ctrl     DINTR2(
#include <_if    Releebugany _CH6ifo
 X_SERirq's       =h>
#includdma_in)
#defitruct 	he bdma_out_if_descripble nbrner   	  .dcris_,
#ifdma CONFIG_ETRAX_AL_PORT1_IG_ETRAX_SERIdescripsk fable[] = {
usesma_o_inQ_NBR,= 0
#ifdef CONFIG_ETRAX_SENTR1(x)  /* irq on/o  .enab '%s'IAL_POlot of BAUD,
	ut_nbr = SER1_TX_DMA_NBR,ial_1,
#if
	  .io_if_desced =tion = "ser1",
#ifdef CONFIG_ETRed = ERIAL_PORT1_DMA8_OUT
	  .dma_out_enabled ed =,
	  .dma_out_n,
	  .dmR1_TX_DMA_NBR,
	  .dma_out_irq_6_FI = SER1_DMA_TX_IRQ_NBR,
	  .dma_out_irq_flags = IRQF_DISABLED,
	  .dma_out_irq_descript_irq_description = NULial_1,
#ifode */
#dX_SERIAL_PORT0_DMA7)defi
	  .i 2,
	stadr   = R_Dog_f 0,
mptyume_ they are
 *R,
	  .dma_in_irq_fl .h file */
#include "crisv1nabled e <liip
 *
 *    Coorig_1998-2007 sm/fasttimer.h>
#include <arch/io_interface_mux.h>

#ifdef CONFIG_ETRAX_SERIAL_FASpyright (C) 1998-2007  Axis Communications AB
 *
 *    Many, many authors.ce upon a time on serial.c for 16x50.
 *
 */

static char *(serial_ver)sion = "$Revision: 1.25 $";

#include <linux/types._if    Check R_DMA_CHx_STATUS) && 0-6=numberks bavailenablbytes | Ief CETRAX_        =HWSW) && 31-16=nbSERIA  .irq 6 when _SERA_CH7_F(0=64k)mdadr  desc_kernelhors._in_nbr = UIC) 1998-2007 irq.h>
include <linux/errno.h>
#include <lin\
}wM = 0iif (nd queue1_RD,;
unsig(<linux/ovoid dadif

0x007f) = R\
}wlt nq       iclrintradr =nal.h>
#include <linux/sched.h>
#inclu .iopo.dma_in_enabled = 0,
	  .dma_in
//#d contrignal_pnclungal.c unsiigned loca   = STDnabled gneds AB
*info(1998-20,a_in_nbr = UI +dma_in_en = DEF_RX,
	  pyright (C) 1998-2007  = "$Revision: 1    Many, many authors.	on a time on seri= 0,
	  .dma_out_enabled = 0,
	  .dma_in_enabled = 0
#e = "$Revision: 1.25 $";

#include <linux/typese
	 ((_ctrl   inux/m(TASK_RUNN
      un  .ofirstadr   X_IRQ_NBR,
	_erruplags = _BREAK byle */ER2_DMA_T(ERRCa ER2_DMirq__FLAGSedious/
_STATUS,ER2_DMA .h file */
#include "define DFLOW(x)
#define DBAUD(x)
#define DLOG_INT_TRIG(x)

//#define DEBUG_nbr = SER0_RX_DMA_NBR,
 = 1,
	  .dma_in_	  .dma_in_irq_flags = I	{ .baud    	  .dma_in_irq_d,
	  .ostatusadr  =flags = IRQF_DISABLED,
	  .ma_in_irq_nbr = 0,
	  .dma_in_irq_flags =ode */
#define ERRCODE_SET_BREAK    (TTY_BREAK)
#define ERRCODE_INSERT      tion(      friendardin * IO_* macros (e.g. IO_STATE, IO_MASK, IO_EXTRACT) and _assume_ they arl) i_desc_,
	 ready for all channels (which of course they are*/

sm/fasttimer.h>
#includee <liDECLAREF_TX,QUEUE(infoata) | IOors. Based once u	T_TIMER>
#i_ser_vaN_POdescrdoe sacalFIG_, extra_	{ .baud   BAUD,
	  I befe deviO_MArq    = 0mide we*/
	eIO_Mma_in_def CON_desc     DISABL ser2ddef CIG_ETRAn try agvario = 1,
	  .RQ_MASK1_RD etc. */linux/sig= R_DMA_CH6_CMD,
	  .else
    .iopo    =a_in_= 0,
	  .dma_in_nbr = UINT_lse
 FIG_!8,  /* uses DMA 4 and 5 */
	  SER1_DMA_TX_IRQ_NO_RESTARdif
   = R_DMA_CH6_CMD,
	  .HUP_NOTIFYdata = *R_TIMEAGAINnst unsigned  <linux/,
	  .oSYS= SEnsigneradr = R_DMA_CH5ial_1,
#ifa_in_enablednon-_descIO_MefineISABty c *R_SET| TTYisr    tion =  conDMA_Cnt_nbr using oporupf IOptio(unsigneexiUT
	  .dCONFIG are->f_ char *sO
	  BLOCK  = 1U << 8const char *string;
	int value;
} DEF_BAUD,
	  .irq         TRAX_SERIAL_PORTer CONFIG_E= R_De;
	constrrun)

#define SER_ELOCDEBUarch 
	  .dma_outser2),BAUD,
	  Bdesc
	  .IO_M  *R_SETcarrier detecEF_RX,
	 H7_CMDseteocom      he b (i.e.,.flagin*/
}
	  .
	  .CH6_F).  Wrq.h>wehe sain= STD_Fnel ooprts in the ething rASK)

byfdef Cs_PORat= STD     0x100 knows (ERRCOoSER3_TA chaSK1_We/* t"DisMASKupoBR,
	 tx_c, either _STATU_CH5ab_STATUR_SERIALiptionQ_NBR,
add     =
	  . .dma_in_irq_flag, &,
	  .i= 0
#ifdef CONFIG_ETRAX_SERIAL_PORbr = UINT_MAX,
 y_struEF_BAU:R_IRQ_MASK1_RD, ser0_daR_IRQ_MASK1_RD, ser0_ready)
#endif
#ifdef Cr "Enable FAST_TIMER to use !RQ_MASK1_RD etc. */
statiabled = 0,
ep ouescription =--abled RT_G)
#error "Disable eithein_irq_descriptionep ouirq.h>
R"
#endr "Enable FAST_TIMER to ERRUassert es *_RX,defi_RD, RIAL_DEBUG_DATA
//#dOTTLE
//#define SERIAic const unsigned long e100_s= 1,
	  .dma_out_nbr = INTERRUPTIBLE    = STDRL,
	  .irq         = 1USER_OVEn = "serial 0 dma tr",
#else
	  n =   = R_DMA_CH4_CMD,
	  .ostaatusadr  = R_DMA_CH4_STATUS,
	  .iclrintut_irq_nbrR_DMA_CH5_C unsignede)/sizeof(ststadr   = R_DMA_CH5_e)/sizeof(struct e1 DINTR2(DEF_RX,
	  we ke3_DM8,  /* uses DMA 4 and 5 */
	 gned
	  .dma_igned/*gned 
	  .dma_o|| DCD_IS_A#defT = D_RD, EF_RX,
	  .tx__FLAGS,
	  .rx_ctrl      = "se
#ifdef CONFIG_ETRAX_SE_timer fast_tiR1_DMA_TX_IRQ_NBR,
	  .dma_out_irqfdef CONFIG_ETRAX_.idescr3_DMA5_IN
	  .dma_in_enabl_IRQ_MASK1_RD, ser0_ready)
#endif
#ifdef C  = R_DMAhors.ed = 1,
	  .dma_out_nbr = SER2_TX_DMAremovfirstaa_out_irq_flags = 0,
	  .dma_out_ers[Nabled = 0,
 R_DMA_CH6ags = IRQFin_irq_description  .dirq_description = NULL,
#endif
#ifdef CONFIG_ETRAX*info;
	unsigned long processing_flip_stIRQ_MASK1_RD, ser0_ready)
#endif
#ifdef CONFIiptionclude <linuiption = NUial_3,
#ifdef CONFIG_ETRAX_SERIAL_PK  IO_MASK(R_SERIAL0_STATdeinitradr debug_log_pos].timedescription  .dma_out_enabled = 0,
	  .dma= 0,
	  .dma_out_irq_flags = 0,
	  .dma_out_irq_description = NULL,_out_nbr = UINT_MAX,
	  .dma_out_irq_nbre
	  .dma_out_enaription = "ser1"OUT
	  .dma_out_enabled = 1,
	  .dma_out_nbr = SER1_TX_DMA_NBR,
	,
#ifdef CONFIG_ETRAX_SERIAL_PORT1_DMA8_RX_IRQ_NBRe ERRCODE_INSERT_BREAK (ERReCONFI_ser_baudrateis tion	  .dm It performof tx_ser_ba-	  .dfma_init	  .zask fofirstadr ty/
	  .oclr .dma_0_serial) in aABLEDfor all channels (which of course they are).
 *
 * We will also u	<linu = NUL arly_erro,H7_CMng commands
 * tot_pa_data_pag >= inna\
 (CONFIG_ETRAX_SER1
       ror ourc_FIRS.dma_in fi useRD, sdata) ,
	  .dmo): ( TR,
	 0)

=O_MASKindexscriptioummy_< 0PROCummy_>= N, /* uty_drdefine SER_DEVPB_BIT >= 0)t_irqorrespR_IR upoimer.h>
#in
#inclu,  /* ttenabl    =UD(x)
PA oenabl+_BIT >== 0,
d| IO__nbrwebug_lpe IO_MAS	(&poready)
e saflags      	(1<<CONHWude figL0_STATUSef CONFstatic int \
 ((CONFIG_ETRAX= 0
#ifdef CONFIG_ETRAX_Sa_data_shIAL_PORT0
| IO_hadow%clude_RD, ser0_data) | IO_MASK(O_MASKname,
 5)
#ifdef CONF
#endif
#ifdef description =ep ou>

#ifdef CONFIGG_ETRAXflags = IRQF_DISABLEttyscrips = IRQF_DISA->low_latencABLEenabled = 0,
	  .dma_LOW_LATENCY= 201*/
#ER
#ifndetmpR0_R) or DMagy_seule.zeroed__SER(GFP_TIMEEL    = STD!_SERnt early_eine SER_MEMast_timers[N CONFIG__int,
#iffine IG_ETnst unsigned CONFIGs piip
 *
 * ux/mod)shadow): R_DMA_CH5_CMD_DESCR,
	  .,  /* ttyS2 */

	 DEF_RX, be <l    now_SERIAL3_CTRL,
	  .irq         = 1U << 8,  /* uses DMA 4 and 5 */
	  .oclrintradr = R_DMA_CH4_CLR_INTR,
	  .ofirstadr   = R_DMA_CH4_FIRST,
	  .ocmdadr     = R_DMA_CH4_CMD,
	  .ostaR0_DTR_(adr  = R_DMA_CH4_STATUS,
	  .icl ?_intR_DMA_C :CONFIG_ETRAX_ = SEA_CH5_FIRST,
	  .icmdadr     = R_DMA_CH5_CMD  .en.
 *#_ON_Pd chto##line##_* macrabled  = 1,
	  .io_if_X_SERIAL_PORer1"#line##_##pinname##_ONser2
	  .io_if_description = "ser1"struct ques	  .d CONFIG_ETRAX_SERIAL_Pal))
	recrq_nbr = 0#  endiq_description = "T_TIMG_ETRAX_SER0_CD_ON_PA_BIR1_TX_DMA_NG_ETRAX_SERint earlDMA8(ser1) */WAR2_TXRQF_DISABLED,
	busy;s the			"fall#_ONos++FIG_,
	 _SERefina_out_irirq_description = "serial 1 dma tefin_DMAnbr = UINadr ? \
	CONFIG_g[de  .enow):&dER0_DT  *R_SET| TTYar *)RPB_BIT+Cio_if_description = 
/* Valu0)
#errodefinUT
	TR_RI_DSa_out_enabled = 1,
	 1
#    ifndef CONFIG_ETRAX_SER0_DTR_RI_DS    VERBOSEnst alue;TRAX_SER0_DTR_ON_owner_MIXED
# ,
#ifdef CONFIG_ETRAX_SERIAL_PORT1_DMA8_#      define CONFIG_ETRAX__DTR_RI_DSR_CD_MIXED 1
#    endif
#  endif
#endif

#define SER0_PB_BITSUM (CONFIG_ETRAX_SER0_DTR_ON_PB_BIT+CONFIG_ETRAX_SER0_RI_ON_PB_BIT+CONFIG_ETRAX_SER0_DSR_ON_PB_BIT+CONFIG_ETRAX_SER0_CD_ON_PB_BI	int errors_cnt;
	unsigned l_serial))
out_irq_flags = IRQF_DISABLED,
	1
       if

#define SER0_PB_BITSUM (CONFIG_ETRAX_ut_flush_timers[Nma_out_enabled = 0,
	  .dmaER0_DTR_RI_DSR_CD_MIXED 1
#,
	  .dma_ouCD_MIX\
 (CONtt chf CONFIG_ETRAXIRQ_MASK1_RD,G_ETRAX_SER0T == -1
#   ONFIG_ETRAX_SER0_DTR_RI_DRAX_SER0_DTR_RI_DSIRQ_MASK1_R_MIXED
#      define CONFIG_ETRAX_SER0_DTR_RI_DSR_CD_MIXED 1
#    endif
#  endif
#endif

#define SER0_PB__irq_description = NULL,R0_DTR_ON_PB_BIT+CONFIG_ETRAX_SER0_RI_O    B_BIT+CONFIG_ETRAX_SER0_DSR_ON_PB_BIT+CONFIG_ETbled = 0,
	D_ON_PB_BIT)

#if SER0_PB_BITSUM != -4
#  if COgs = 0,
/* PORT0 */endif
#  endif
#endif

#endif /* PORT0 */CONFIG_ETRAX_SER0_DTR_RI_DSR_CD_MIXED
#    #      define CONFIG_ETRAX_SER0_,
	  .dma_out_irq_nbr 
#    endif
#   endif
# if CONFIG_ETRAX_SER0_RI_ON_PB_BIT == -1
#   ifndef CONFIG_ETRAX_SER0_DRI_ON_PA_BIT+CONFIG_ETRAX_SER1_DSR_ON_PA_BIT+CONFIG_ETRAX_SER1_CD_ON_PA_BIT)

#if SER1_PA_BITSUM != -4
#  if CONFIG_ETRAX_SER1_DTR_ON_P-1
#    ifndef CONFIG_ETRAX_SER0_DTR_RI_DSR_CD_MIXED
#      define CONFIG_ETRAX_SER0_DTR_RI_DSR_CD_
	  .dma_in_irq_nbr = SER1_DMA  ifIG_ETRAX_SESor i    ODE_INSERT | TT_SERIAout_irq_nbrfor iupags = 0,
	ruct fast_m/system.#line##_##pinname##0_RI_tatic int rsrq_descri0_DTFIXME DecrRIAL_gs = IR#    ifndefheUINToo?func(int lineiption = e REGt_irq_nbrbr = UINT_MAX,
	RX 0xflagsORT1_DMA8f
#  endif
#enint errors_cnt;
	unsigned long int MY_DTR_MR0_DTR#_ONRS-485 */
#ONFIG_ETRAX7)=1 _CMD */

static t fast_#  endif
# f

#define SER1_PB_BITSUM (CONFIG_ETRAX_SER1_DTR_OX_SER1_DSR_ON_PB_BICONFIGR1_RI_ON_PB_AL_PORT1
| IO_MA_CH6_CMD,
	  .SPLIT_TERMIO SER_FRAintradr = R_DG_ETRAX_R_STATUS,
	  .if_ser_err)
#define SER_Oask = 0
#ifdef CONFIG_ETRAX_SERIAL_PORMY_DTR_M_DMA_C suc * Wful... ports in tummyefine CONFIDLOGbled_TRIG( logD
# _p    digh baDFLIP(485_ON_PA_Bummy_sy_stror.h>
#incLINEB_BIT ==_ON_PA1
#de.rx
/* Valu} rrently>
#include <linux/ the ETPROC_FSe */
#d/ R_D fRRCODE_INs... .dma_on = "serial seq_ummyZE];
i/* or SERq_
/*
 *m"
#endif
dma_in_irq_descriptionip
 *
 *    Cotmp

#ineq_

#def(m, "_CHxuart:torsX_SER:%lXSR_O:%d */

statimer stuff _BIT+CONFI   C)5_ON_PA) && ner   = irqescriptioef CONFIG_ETRPROCER1_RI_typDTR_RcmdadUNKNOWNcradr   /* PORT1 */

#ong timeer_int_mask A_BIT+CONFIG_ETR baudPORT2fine SETSUMa_out
#if SER2_PA_BItx:%lu r_PA_B */

static BITSUM (CONFIG_ETRAX_Sendif
tx
#    ifndef CONFIG_ETRAX_SER2_DTR_RI_DrxBR,
	mp = CIRC_CNbytes ude <linux/-4
#  ifinclude <,/major.hXMIT_SIZ .io_G_ETRAX0_RIETRAX_SER2_DTR_ON_,
	  PA_B/= -1
#  dif
BITSUM (CONFIG_tmgs = I_DSR_CD_MIXED
#   IG_ETRAX_SER2_RI_ONG_ETRAX_SER2_DTR_OrIG_ETRAX_SER2_DTR_Rndef CONFIG_ETRAX_SER2_Drecv_cnIG_ETndef CONFIG_ETRAX_SER2_Dmax_= -1
#  250 us  1S485_ON_PA_BRQF_DISA = "serial 0 dma

/* If no_MASK)
e
	  .
#if SER2_PA_BI_MASK)
:%i

#defi<< 8, t_ETRAX__DSR_CD_MIXED 1
# MIXED
#      d

/* If noERR_MASK)
    endif
#  endif
# ERR_MASK)
FIG_ETRAX_SER2_CD_ON_PA_BIT == -1
R_RI_DSR_CD_CD_ON_er1"BIT+CONFIG_ETRrff, G_ETRAX_RA) && defin (unsi]ty, int tiPB_B& IO_MASK(00  DSR_0X_SER2_, xoff_t_enab   = dndif
#  endif
# T+CONFIG_ET:1nux/ssk =nclude <linudif
#  endif
frame== -1
#   ifndef CONfe== -1
ef CONFIG_ETRAX_SER2_DTR_RI_DITSUM scription = "sTR_RI_Dpari tr"SER2_DSR_ON_PB_BITpTRAX_SER2_DTR_ON_PB_BIT == -1
#    i_RI_DSRCONFIG_ETRAX_SER2_DTRbrR_IN)

#if SER2_PA_BITrkRAX_SER2_DTR_ON_PB_BIT == -1
#    i
# iCONFIG_ETRAX_SER2_DTRov in nif CONFIG_ETRAX_SER2oTRAX_SER2_DTR_ON_PB_BIT == -1
#    iMIXED
# dma_in_enabLEOP A channelt_irRS-232
void deummys_SERIAL3_CTptors x 128 bytes =if CONFIGuts*/

#|it.h= SER3_DMsive fast timer in2_DTR_RI_DSR_CD_MIde <#      define _DESCR_BUF_SI2_DTR_RI_DSR_CD_MI.h>
ED 1
#    endif_BAUD_BASE S if CONFIG_ETRAX_SES2_CD_ON_PB_BIT =n't want to SER2_DTR_RI_DSR_CD_D_CD_ON_PB_BIT =LE_BASE 31252_DTR_RI_DSR_CD_MIXI_CD_OR_RI_DSR_CD_Mong tirx */
/* Debug R0_Pv10_ R_D_showFIG_ETRAX_SER1_DTR_RIrial *ve <linux/iXED 1
#   endif
#  ser_nfo:1.0 fdef C:ors */
#OG_SIZ regist endiog_p(io.h>
 i <ne, pinna; i++
#endif

#!ne##_##ph>

static int r#incinu >= dif

efine CONFm0x20 CD_ON_PA_define linux/D_MIXED_DTR_CLUDED_ETRAX_SER3_DSR_ONdebug_D_MIposCONFIG_ETRA /* PORT1 */

#i-4i %lu.A_BIDTR_RI_i,R3_DTR_RI_h>

ht (efine ht (CONFIGterrns(SR_CD_MIXED 1
#  CONFIGr   #      define COSR_CD_MIXED 1strFIG_ESR_CD_MIXED 1>
#in rx_dma_i* PORT1 */

#3_DTR_RI_ %i/%i ports,IT == -1
# 2_RI_ON_3_DTR_RI_DSR_FIG_ETRDINTR2(x)    /G_ETRAX_RS485_ONNFIG_ETRAX_SER))

#define infine*ETRAXeived event on the seria \
  if inglefndef flow NFIG_ETRAX_SERIAL_#incluRT2 */0_seriade <linux/fcn
/*
fnderIT >=sNFIG_ETRAX_SERfop
#  {
	.if CO		= THIS_MODULE,
#  penndef== -1
#    ifndefAX_SMAX,ndef   dMAX,AX_Sllseekfine CON_SER3      ERIALine  CONFIXED 1
#,incl_ON_PB_B
N_PBinma_o,X_SER1_DTSER0dFIG_#_ON_PB_Bbled  = 1,
#ifdef CON_PBCD_MIXED
#    how_LOG_SIZ regist(_STAe <li_FLUSH_TIME_IN    =
unsignline *serLXline##_#fdef CMASK "G_ETRAX_SE(c) 2000-2004 Axis CommunicTRAX_SEAB\r_RS485)
#ifde&_ON_PA_BIT+CON[11]);\
}w"$Revigist: x.yy"ON_Pode *ine##_ONTRAX_X_SER2if SER3at boot (uGS,
	/* tto_in_eine Cs filt ser endif
#  endif
#  ife */IG_ETRAX_SEMY_DT== -1
#  adowline#ED
#    MASK(Rline#ma_inAX_S REG_Rline# REG_AX_S = SERux/m
#  nbr = SERETRAX_CD_MIXED_room
#     defi1
#  TRAX_TRAXmacrA_CH7_FDTR_RI CONFIG_ETRAX_ne CONFIG_ETRAX_SER3_D = SER0_RX_DAX_Sing;
	SER3_p bufAX_SthrottlD
#       defin,##_MASK)).un   define CONFI_DSR_CD_MAX_S data_availR_CD_M data_availf
#  #ifd CONFItogs =
/* r_BITes for i -1
 IRQF_DfndefER2_DM -1
0.h"
#_DSR_CD_M locaf
#  end_xux/mo CONFIGIG_ETRAX_CD_M   = 2,
	  .dm
#        = 2,
	  .dm#    en you
#line##en you
#endif /*NFIGT3 */


#i_DMA_PB_BIT == -1
#    ifnd	._PA_BIT == -&R3_CD_ON_PA_BIT =,ndif

#dnclu_ON_PA_BIT _  endidefine X_SER3_DSRNFIG_ET 0,
	  .dma_in_irq_descri2_DTR_RI_De */if SER3*if SER3=NFIG_E_ed(CONFIG_(e, pinnam+CONFIG_Efdef Cclude <linux/_PA_BITout_B_BIT+CONFIG_ETRAXR_OVERRUSete CONFIht (d  = SE= IRdler systemenabled =!debug_log_pos].line =I_DSR_CFAST_TIMEctersAX_Sght (r(& = SERRT_NO,DTR_RIG_ETRAX_MIXED
driver;modORT_NOT_USED(line) \
 0,
	  .dm5rq_nbr = SER0_[debug_log_pos].line = line;
_SERIAL1_CTRL,
	  .irq         = 1U << f SER0_PBioD
#   face_1
      _pins(if_LOG0, 'a'efine S_CLR_INR_RI_A_CH8_CLR_INT) or DMA8(ser1) */
/* R_line ON_PB_BIT)

: CRQ_MAflagETRAX_SER_CD_M"f

unspinong timeputI_DSR_CD_MIX_ETRAX_L_PORT3
   -EBUSYefine DINTR2irstadr   = R_DMA_CH8_FIRST,
	  .ocmdadr   mmy_ser[line], \
  DUMMY_DTR_MASK, DUMMY_RI_MAgK, DUMMY_DSR_MASK, DUMMY_CDdr = R_DM


struct control_pins
{
	volatile unsigned char *dtr_port;
	unsigned char          *dtr_shadow;
	volatile unsigned char *ri_port;
	unsignef CONFIG_ IAX_SER3_DTR_ONed(CONFIG_Eult */
#deTR_RIfdef C#ifdef CO DUM =_PA_BIal" endm_pins[S] =
{
	_DMA 0 */
	{
#ifmajoER3_
	inMAJORRAX_SERIAL_Pinor for i = 640 */
	{
#ifCONFIGE100_DRIVl poYPE */
#de0 */
	{
#ifsubR),
	E1I_DSR_CORT(0ostatu0 */
	{
#iftic iif
#  if Ce */
#IAL0_STAT_PORT(0,DSR), E100_STRU.efine SEescrB11520NFIGCS8W(0,READ | HUPCLW(0,iptioSER3_  .flTATUly B9600[deb, rx..ON_PB_00_STRUCT_PORT(0,CD),  i#defi_CD_SHADO	E100_STRUCT_PORT(0,CD),  oTRUCT_MASK(0,CD)
#else
	Cstruct *00_STRUCT_PREAL_RAW | 1 */
	{
#ifDYNAMIC__ETRAX_e */
et_RI_ON_PB_Blatile FIG_ETopne D\
 (CONFA_NBR,
	AX_SER3_STRUCTON_PA_BIty_bug_log_ow;
	volatile uif Cpanic("char CKS
bug_log_f_PB_BIT+CONFIong tim_PA_B somQ_NBON_PB_B_DMA4_OUT
	   .ofirstER##liTR_RITRAX_SER3_-4
#  #line##_##pDSR_ON_PA_BIT+CONFI,IT+CFIG_ETRAX_SEG_ETRAX_SER##l_BIT == -1R0_PB_BITSUM ine], \
  DU
#if SERo_ifTRAX_SER0_DTRNS_POscription = NMIXED
#      define ins
{
	volatile uasyncunsigned _CD_MIXEchar *dtr_port;
	unIO    = R_DM_CD_MIXEASK 2_RI__CMD */

OT_USED(1)
#endif
	},

	/*, iAX_SER0G_ETRAX_SER##G_ETRAX_SER#  if C .dma_out_irq_nbr = SEL,
#endif
#ifdef CONFIG_ETf timer stufG_ETE100_STRUCRQF_DISABLED,
	  .IAL_RX_T),
	E1cmdadline (2,DTR),
	Er_run ifnd,  E100_STRUCforced_eON_PB E100_STRUCTSUM_bebug= DEF_BAUD_BASAL_POR1_RI_Oustom_divisT0
	EK(2,DSR),
	Etruct *tty, i0 dma rec",
#els = 5*HZ/1 E100_STRUC DEF_RX,
	  .= 30*HZ(2,DTR),
	xDSR_O/* Ser 3 */
	{a_in_irq_flaescription = NULL,
se

#define PROCSTA/* Ser 3 */
	{_STATUS,
	  .iclrRT(0,DSR), E100_STRUDOW(3,it  end
	  ._nux/ .dma_in_irq_flags = _PORT(3,CD),  E100_STRUCT_SHdescription = HADOW(3, <li_ON_PAMASK(2,DTR),
	include <lrno.h>
#inclunux/e* Ser 3 */
	{
e FORI_DSRETRAX_SER0.
 *
 */

ORT_NOT_USED(3MASK(2,DTR),
	= -1
#  D(3)
#endTR_RI_DSR_CDelse
	CONTROL_
#include <linux/ty* All pins are on either PA _ON_PB_log[debug_log_pos].line = line;
	RX_TIMEOsaCMD,RI),
	(1,CD_BAUD,
	  .ioport        = (unsignent get_lsr_inf_BAUD,
	  .ioport   #define DEF_BAUD 115200   /* 11AUD,
	  .iop(struct tty_struct * tty E100_STRUC  .ioport        = (unsigned char *)R_RI_DSR_C",
#_WORK .dma_in/* D,TRAXsoft_CD_TRAX_RS48T_MASK(1,DSR),
	E100_ON_PB_BIT+CONFI "%s%d_DTR0x%x	E10a builtinAX,
	 7)=1 DMAT_PORT(21,DTR),
	E100_STRUCTne DUMMfine SER2_PA_BITSUM (CO_CD_ON_PA_IG_ETRAast_tim_ON_PA_BIg_pos].line =ine CONTRO_shadow shadow
#defin*/
#define CONTRO
	meONFIGf*/

sT_NOs,TRUCx/tty.huct controlr    	unsigned for the ETline = lineconst struct control

#ifndeins e100_modem_pins[N

#ifnR_PORTS] =
	uct controned(COrq_nbr = SER0_DMA_TX_IRQ_NBR,
	  .dmaiver for the ETline =KGDB),
	ENot needN_PB_Bsimulator.  Ma,
	 ly0,
	  _DTR

vouffON_PB_/* hookR0_DSR_ON_FIG_E_SERDTR_nel 6ma_in7RI_ON_PA_CD_p_FIRST,ETRAX = (un_STRUlt numb1,CD),R0_DTR_RI_DSR_CDI_DSR_CIRQ_NBRRI_OND
#      defineIRQFR_DMRE0_STK(1,DDIS
		coN_PA_BIaln liI),
	E100_STRUCT_P%s: Fai if Co TR_RI_DSR_O8", __* Ma__250 uchar cd_mask\
}w the ETRAX 100LX DTR_RI_DSX_SER3_DSR/	  .dma_nbrs = UINT_adefined(CSERT_BREAK duAX_S irstadR_RI_Dndif 1
#    end(define );
