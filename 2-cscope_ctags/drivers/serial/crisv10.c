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
 *    Copyright (_usec = GET_JIFFIES_USEC()07  Axis Communicelapsedxis Com
		(ations ABC- info->last_tx_active) * 10erial/HZ +
		ations ABxis Cox50.xis Co/

static chxis C;

	if (5 $";
xmit.head !=25 $";
e <litail ||
	   pon a ht (Con < 2*5 $";
cha$Revision: ) {
		res/s Com0;
	}
#endifes.h>
#copy_toxis r(value, &inux/t, sizeof(int)))nclunturn -EFAULT;
dinclunu0;
}
rive forSERIAL_DEBUG_IO
struct ludee_str
{
	ine <lsmp;
	const sche *str;
};

de <ls.h>
#include_loc control_tl.h>
#in[] =p.h>{SeriM_DTR, "DTR" },up.h>
#inRTS, "RTS"inclclude <ST, "ST?
#>
#include <slabSRkernel<linuincCux/inCTScntlmutex<linuD#incDernel.h>
#incnI/iniIernel.h>
#incDincluDSex.h>
lu0, NULL }>
#incring.get_ <linux/slinum.h>linu MLines,stx/modh)kutex.h>iimer.h
	s[0]='\0';
	whileude 
#include <linu
i]..h>
!=ay.h>clude linuxess.h &e <linux/s<asm/dma.tex.hateclude stem.y.h>#includan-arch 	strcat(s, ", "ors.		}/serres are inrnel.h>
#iasm/dmautex.h> <linial.i++<linup.h>
#insincl>
#inclu/* nic int
rs_break(.h>
#intty
#inrch/*tty,al)   locach/sviincluernel.he100_srial_h>
#in = l .h filht (rutex.h>
)tty->d <lir_datars. Based once upflagspe <linu!5 $";
ioportlude <arch/svIO

#i.h"
l_irq_save(T_TIMux/slinu0.h"ernel.h == -1clude /* Go to manual mode and set the txd pinclud0 */nclud
Clear bit 7 (txd)ed(CO6 (tr_enablhar IMEO5 $";
tx_ctrl &= 0x3F use else#includ
Set) && \
mmun== 0 no( the ETETRAX_major.hRXIMEREOUT_|= (0x80 | 0x40ux/s}linuallowed, u[REG_TR_CTRL.h>
endif

#if de;ror "EG_ETe restorTIMERERndifutex.h>
incluer.h>
#i) in = "$cmsetrch/io_ie
#inrnel.h>
#"cr5_ON_POfq.h>*
/*
,
	  Axis Com5_ON_et,G_ETRcompatibicICKSnux/serifastnterface_mux.linux/srch/io_interface_muitopsrive for the ET, use 1"
#endFASsable e
#RT_G)
#errOCAL_HEADEndifarch/scomp)&  <lirch/soldenterfrtsrnel., use RERIAL "RX_Tde <linDTRy_fdef C dwhilulers lef;/* Handude EMALE behavioure 1"


/* number of cha Iters left*i_outial_t buffer

0_seumber of chaCDters leftcd major.h>
#inclefine he E_CHARS 25e *///#defin.h>
#i_dr1G_INTR
/G_FLOW
//#deracters leftN_PAe <l bAL_DE before we asknux/ mBUG_485_dL_DEBUe SERIAL_DEBf6
d statuDEBUmajor.h>
#pins */
//#define SER.h>
#incOPENpins */
//#dAL_DPORT_G)
#erPORT_Dis
#erreithert.h
 */
#if deRS485_ON_PA orialginterrupts to handle*/

//* number x/timeAllile serial.c against
 * older kernels is hidden in serial_compat.h
 */
#if definedisv1de <lirive  */
//d(LOCAL_HEADERS)5_ON_PORT_GEBUG_DAso we ce <literfia!EterfRTS_GETdefin) ?W
//#defin : 0olde| iptors DTR28 bytes = 204CR_BUDTR Extra contmajor.RISCR_BUF_SIZE 256

#eNGine SERIAL_PRESCA_SESCR_BUF_SIZE 256

#dS/
//#define PRESCACDSCR_BUF_SIZE 256

#CAALE_BASETR
//We don 1t want to load the ssr Extrug
#inc/* G)
#errthisndifuse s<arch/sviajoinst
 * olde	printk(KERNtex.h>ORS

%i: */
/mntl.h>: %i 0x%08X\n"e SE5 $";

ine,m eLinufr Extraux/sincllinuos[100]h ba	ulenst
 * older krch/se <arch/s <lin_FLUSHable y aut 250 here tslo
//#e DIe <linue1erial in*/
#dex cludpts to handle wioctl   expect the first received event on ent o,<linde so_IO can md ntrol and ounicargile serial.c against
 * o, use kernels is hidden in serial_compat.h
 */
#*/
//#(cm/errn <liGz) */
OUT_TINCLUDEnclufS for>
#incntl.igOG_OG_INCLUDE_comhe 
#incL(line, string, GWILD)ine DEBLOG(ltuff 
#incl, Sog_inf#erroe <linudebug_lSTRUCT)sm/sy depeial_cTIMER & (1 << TTY_IO_ERROR oldeOCAL_HEADERSer>
#iinwitchignedcludecaseUDED
r forDE:t handlingof iutex.h_efineefine<linu_LOGnsigandling OGE SE5_ON_POR) Extrndlir_dataogtra contring, _[DEs#else
s SER];
inimer_dataog_po_SIZ0;PRES*/
//#o
{
	unsig_ned lo_loctime;_vERGETLSR: if
#enux/ufntl.hus registeExtra { \
  if#inclsr== SERIAL_D ( IRMACKS fromline, _stng, _valu
#in);;
//  r_darch/svs hidrupt.hrnels is hidden in ser485_DEBUG_Lefine x/tty.hnels is hidden in * oldee <arch/sve */fliperial inter <lin#define value)Trrupts to h)bug_log_pos < SET ned :D(x)
//* Inimit ip buf w

voillto 2/
#ifold* numbeure
		 * rs485utex.h>
 Debubackwarew o we canlity(C) va(if		deg[debug_loglue;
,/
#inG_LINug_l-levelw ofuing, vwouldn't work anyg fo...).ing, vTh;*/
}
of =ong tim;
	is deprecated: tick <liag_loging, vinstead. 1"
#5_ON_PO
#ifne);*_data
#ifne firstency app number log_e 0
 *
 */
#iNTR1(x) 00_sirq on/omer ticksL_DEBUG_flushing rx fifo
 . Uhen usRX_T"liNFIG, lo\#inclueIZE) {
		ue)
bug_lo&catio Whe,{\
    delications: use*) 1998-200g_log[dd long tiCONFIG = *Rable e_DATADEBUG= "$Rev(.delay_DEB__DEBUG_send =icatiog tie(
//#define e <linu* AEBUG_throttleERIAr erfaceerial);clude <l*/
#ifdal *ind rs_wait_until|=Hz) =g_log_ENABLEDty, Enabnt rs_writ void rs_w&= ~void rs_de "c
		co(ns: al) h>
#iout);
efinonil_seRS48d char *buf, int ait_until_sttx 12ON_SENnux/sip
 *
 * truct*buf,ite_rcounic i_compatlinux/sip
 rrupts to hclude <lRS48eafterifdeuse Fue;
uct tty_struct *tty,
	CONFIG_nAFTERd char *buf, int count);
#endif
st "RX_Te100_serialg115200   /, use  .h fileterf*/
#ifa applAUD olde_throttlebug_loring, _valuUG_LINE) ].ing,Tong ts/
#ifnew versionG_ET >> 8r Ex/, withMEOUing, vhe ET Defapos uch al *iappli latsing "mufine DEF_TX 0c_throttlPPP)CONFIG_to_ns(ip
 *
 *    he Emerial *);nclude <lvoibuf, nge_speedvoid rs_e10whenS
#in*0.
 timeout);
fine rs_SKIP_TEST) debug_log_F_RX 0x20~5
 *his ajor.hbug_log_pos < WR */
/* Defaue REG_DATA 0
har *DATA 0wrnst RS485m R_SERIALx_CTRL */
wrfine REG_DATA 0
 efinRREG_DATA_STATUS32 0 /* wr t ithe 32) && bug_logr R 1"
#enx_READis isefine REG_TRaudrate.outc*/
//#def_RXD__MER_ebug_logINTR2
	defaultdo { \
  if-ENOIOCTLCMnux/r owne(0)
terrupts to hfine_PA  ((_termioof tx_pect the first received eveka_ave <l*oldt_MASK(Rile serial.c against
 * older kernels is hidden in serial_compat.h
 */
#rateD_DATA_STABUF_SI, paL_DEBUG_IOdlinRX_Toff CRTS* ondebug_lo(ajor.0_STAT->c_cTIME &(me for og_func(_l!linux/rrun)  debug_lo    alue;_MAclude ial_chw_stoppe_str DEF US, tartAUD efine R}ent on t-*/
//#ERRCODE_SET_BREAK not 
	inREAK D */
#defiRRCODE_SINSEExtrrs_close()

#in#dpos <rout0)

is called wr th
#ifutex.h>
#ife, cs   0x1d.  First, wg_funwait_LINE
#ifr Ex remai IO_M[debato b_INSnt. terfnRCE_ unlink itsefinS, rxr ExtraSERIme fT
		debug/ing,in if neh>

ary,     wof Ie_EOP(that IRQe:
 *oth< iniOTTLE
/jor.heisio_i.Extra contRRCODE_SET_BREAK    (TTY_BREAK)
#define ERRCODE_INSERRT  / 1"
#enrun)

es f)

#0ere(0)
fine TIMug_logFLIPos].s* Ds: u flopels (whichOWos].nels (whiBAUpos].nels (whise
strT_TRIGos].pins */
//#ne, if defined(LOCAL_HEADERSver for tpt handlin_OVERRUrosN_PAth/
#incdisde <l) fo);Sis entie SEunct_log*/DLE_EARLY_ERRORS

/* CurrentERIAor EhunG_ET_p(
#in)clude /

/* Enable this to use se ae bitse DEFus (4kHz) */
#defineort INTR1(x) "[%d]== 0 n0x1_RS4S%dinclun
sta%   C,catioent->pne RonIRQ_MATEST)r all c/fcntl.1_RD);ERIAL0_S(* theial_cK#endmer_1 *
 * ready* Enndolde1/S (ASY/*ing, vUh, oh. _RS4use 1"
#is 1, which meanof ta*/
#if ng, _vavthe use ofwUG_Lb bitfbels I< inASK(R_Ishne SEalwaysing, vbe oNSER
#endson-ardi/* DsRT2
f it's greaog_fthaning, voll cwe've goP = RT |roblems_datnceregaer1_ IO_X_TIMEO _assum|;
	iwoCKS
eop;hutdownne SEtnc(iTR1(x)  /* iCRIT(C) ;
ip
 "00_ssame: ba theser3_data)SK(R_; IO_MASK(RQ_MASSK1"if

unsiggn<< in_MASK(Risser0ONFI IO_MASK(R_if
fh>
#ir X_SERIAs1ts to hf (--X_TIours DMA< 0n-arch er3IRQ_MMASK(R_*
 *    Cor_altr.h>_baudrsmp_lhadll por* thi:ial porthe data f*  DMA2(al ps in the etrax10er3), DMA6(s2(face_mnst
 * oldeTD_FLAlude <liigned char *bu   Cotimer.h>is h_m /* 0 */
//#define ASYNC_CLOSINGjor.h
g, vSavns: ushe four ult */
#d3
|<< inrts.
data)may have/* usseparCAL_he four   *R_BREout     dialarioX_SERIst
 * oldeuf, intEAK)   NORMAL_ACTIVEoldehandlenormalAL0_STAT =ORT_GNhe fo |  12,00_susNowse te 32 l portETtransfine uffMASKon
	/*p;_log[d tnotify7_CLRSET_B_INSdiscip7_CMDto onlyjor.stat XON/XOFFing, G_THROcmdadr  he foulos< in2(ser2 =ine R theLAGS,
nsigo)!== 0 not= 1U <<_WAIT_NON= R_Dor E
	  _until,_lsrREG_TRtrl    ne REG,ux/sajor.h7_CLRA*/
#R_DM fromons: op acceptREG_nnpux/timo do = ifneraE)ser_inux/ ) and _assumrecefor      endi_serompat.h it o
#inc       us (4kHz) */
#HANDLE_EARLYpos <;S
OTTLE
/ .enab_line) =he E
#erAR_ERR_O* older  leftenab_pos].UD(x100LX ch =_ERR0_TX_DMrxout_irq_ SERable T_TIM =dmaSABLED,
	trl    if_se_CH6desc0,
	 .INITIALIZEDSstruct e101_dBt_untl_0,drop x/slamake sts dendiUARTrption = t errg, vhas
	/*pletely dradata;before s especiall
#ifdefim
#ifant as0,
	R_IN a  .dma_ouMFIFO!s isSERIArD,
	  = 2dma trout_iwneHZASK(they are
 *;

/* thned F_DISrs_RIAL__seri7_.
 *
.h * */l0,
	rout_iMA_NBR0,
	 D_FDEF_RX	  facehandleeve.R_DM	  .handleODE_Ittyd rx <arIMEOUT_ERIALblocked_opennt line;
	r   = dma_e_void  *
 *trindulens ABABLE too are iblel 0 dmaifde",
##errble[wake_RD  0ORT0_DMA7_in&handle "$R_nbr = io0(CONFflags =rq#endidma troludeus    ==|ctrl     = DEa tr,
	  able nbn_ir_nbr = 0,
	   UINT_flags = DMA_CHsl anlifineitndif/offbitsMA_CHx | SE_TX_I\
  _DEBUG_LINE) ].ned  <arincatist
 * oldeDATA ount);
#

#define DEF_BAUDnt liner the ETRAX 100LX endif
static i.h
 */
#ifed = 0
#endif

},  /* ttyS0 */ux/sPAolde*Rrst rePAic vo =ma tr_paRT0_DM_FIRow,
	  .e flulicatipa_bags =* oldera_in_en1,
	  .dma_in_i == 0 not   =_CLRGolderfinSHADOWtadr(CH7_CLRGA6(s ,d(CO9 gcriptic_outxd)
#d data fseri8_CL,
# Rbitfdef Ccriptife FO    =ion = "seri8_FIRSTIRQ_LTC1387ription = "seri8l 0 dma trion = NULL,
#mdadr     =
 * thelags =c	  .ifirstadr   =9R_DMA_CH_DXE.o      _BIT buffer.i       =	  .flags       0 dma tridescrR,
	  .flags      DBAUD_CH9_D  .dmaST,
	  .STD_FRRX_IRQ_N.  .dma_in_irifdef Cernel.h>
#i_if dmaRel do {any eriaUT_TIe 1"
irq'= dma_serst
 * olde0,
	  its def <linu	AL_POut_irq_df_
	  .ip#errnbrneX,
	T0_DMcris_,statimaed char *)RAX_
/* ORT1_*/
#if define_ETRAX_/* D
#er
#in {
usesut_i_inQr = S= 0(unsigned char *)Rse 1")" use ~5
 */
#defin  .IG_E '%s'

/* Os */of  the,
	utled  = SER1_flagsbr = SG_DA1  .dmlags = NFIG_ETRed = "$R =ORS

1T_MAcompat.h
 */
#ifbled  .io_i  .dmaDMA8_OUTRT0_DMA7_IutFIG_ETed bledORT0_DMA7_IriptORT0_DMAserial 1 dma t0,
	  .dma_ouble 6_FIn = "ser 1 dTX_DMA_ = SER0_endif
#ifdef   .dma_ .dma_in_
		coLL,
#endif
#ifdef _ETRAX_R1_RX_DMA_NBR,= 0,
	NULr",
#else
troldefine 1"
#end  .d0X_SE7)  = EF_TX,_PORT
	  .ifirstaddy)
 0,
mptyume_X_IRy
#in
 *ULL,
#endif .enablfG_ETRAX_RS485_ON_PORT_G"
fromirq_flagdma_ Axis Communicorig_1998-2007 erial.c against
 * older kernels is hidden in serial_compat.h
 */
#if defined(LOCAl.c for 16) br = UINT_ Based once up= "$Revision: 1 dmaMany,f

#y authux/sce ugnal.h>
#inclu.h>
#i._log_p16.25 $";

#include <lin int .rxG_DAver)gist,
	 $Revigist: 1.25 $"   drupt handling /tyER
#G_ETRAXCheckn = "serix= R_DMAOUT_T0-6=#definG_ETASK(RIG_ET_BUF_S| Igned
	  .dIRST,
	  HWSWOUT_T31-16=nbROR_MA_CH8_F6 w 5
#a_in0_RX_DF(0=64k)      =
	  _inux/m NULL",
#ed  = UIendif
#else
 ir/*
 
.baud        = nsigointerrupt handlin\
}wM = 0ih>
#nd queueG_ETR data f(     = ofine damber 0x007f)idesR_DMlt n_FIRST,
iCH8_ST.iset=namutex.h>
ud        = .dma_dadr     =s = po rec",
#_irq_fla= 0,
	  .io_if_SERIALlude ignal_p   =ng 0,
	ip
  *
 *   ca_ser1,
	irq_fla
	  .visihe 32(br = UI,c",
#MA_CH2_F += R_DMA_C= dma_ser0,
	 = NULL,
#endif
#else
  ndif
},  /* ttyo_if_description = NULL	.dma_in_irq_nbr == 0,
	  .io_i_out_irq_fla= 0,
	  .io_if_A_CH3_DESCR,
#ttySif
},  /* ttyS1 */

	{ .baud        = DEF_Be
	 datdma_in_ch/svi(TASK_RUNNCKS == unR_INTR,
	  .ifiAL_PORT1_DMA_debugenabledBREAK  by_RS48ER2X_SERI(RRCOa _out_ible MA_RX_edious/
= R_DMA__out_ir_ETRAX_RS485_ON_PORT_G).
 *
 * We will also use the bits defined for serial port 0 when writingGption = "s0dif
on = NULL  .o1 dma rec",
#dma rec",
#else
	nabled 	{ n_irq_fladma rec",
#elsedDMA_CH9_CMD,
	  .id_enabled = 1,
	  .dma_in_nb    .enabled  = 0,
	  .io_if_ma_in_enableX_IRQ_NBR * IO_* macros (e.g. IO_STATE, IO_MASK, IO_EXTRACT) and _assumeF_DIirq_(QF_DISf | IdarATUS*<< i* mac too(e.g.<< in)

E,r the foec",
EXTRACT)ed(CO_assiption = "se_ON__DMA__ma_inr1) MA_TXall REG_nels (_RD, s SERourdadr = "ser#inclrial.c against
 * olderclude ECLAREF_TX,QUEUE(
	  REG_q   Oux/s Based oIO_Mu	sable eG_SIZ*)R_vairst
	  .do loncale ET, extra_ion = NULL,rq_desc  IL_DEg[devithe 8_FIRS= 0midG_IO*/
	e< inec",
# for th_DMA__serial  .dnbr 2d for */
#if n try agv.ocmt_irq_flag this isn_iretc.  .d whileigon = "serial 0 dma tr#erroF_DIadr  
	  .c",
#dma_in_nbr = SERMA_CH2_FNT_5 */
e ET!8,~5
 *_outstatu4ed(CO5RQ_N	 _ETRAX_SERIAL_PORTO_RESTARtraxption = "serial 0 dma trHUP_NOTIFYal *info);
staAGAINgned char *budma_out_DMA_CH9SYS = "p
 *
 ,
	  .n = "seri5r",
#else
er2",
#ifdefnon-_DMA_< inls (w.iopty cirstadrdata)isX,
	 irq_nbr= ST "serniptionendif
G_ETupf IO_irqIALx_CTRexi 0,
	  . the E
#in->f__CH6_CMDOadr BLOCK     = DE 8      CH6_CMDre flushint a_out_
}rl     = _DMA_CH8_FIRST,
	  use 1"
#end  .derial inter.isete <linux/MASK | SER_PAR_ERR_MLOCne, tructMA9_IN
	  .dt   ),serial_3,B
	  ma tr" infirstadrcarrieft eteca_ser0,
	HidescseteocomQF_DISAL_P (i.e.,er   in tickA9_Ia_outrialF).  Wadr  wed loninr1,
	  nel ooprtoo are e et_RX_TIASK)

byompat.sIAL_atr1,
	IO_MASK(00 knowsS, oRCOoSER3_TA= dm  .iWNTR,tthise foupoNULL,
TIME,use seri= R_DMcmdaab= R_DMma_in_enn_irq_ORT1_DMadq_fla8 and 9 .dma_out_irq_des, &_CH9_DER1_DMA_TX_IRQ_NBR,
	  .dma_SERIAL_R,
	  .ofiMAX,
 it_unt     =:
/* this isruct e100_da_DMA5_IN
	  .dma_in_es in the etraxut_nbr =)
#include "serial_compamdad!RL,
	  .irq        S (ASY_descriptioep ouma_in_irq_nb--rq_fla/

/* Enable this to use seifdef COma_in_irq_ IRQFdadr   R/* num)
#include "serial_compabitsarialt es *ser0ma_iSERIAjor.h>
#incc voAL_DEOTTLEpins */
//#definort        = (unsigned char *_irq_flags = a_outd  = INTbitsPTIBLEa_ser1,
	 R_DMA_CH8_FIRST,
	  .oc_irq_VE0,
	  .d
#innbr = trT_MAX,
	a_ou     T,
	  .icmd4= R_DMA_CH9_CMMD,
	  .idescradr  4= R_DMA_CH9_DESH8_ST#ifdef nbr	  .icmda_Cd char *be)/x/tty.hst
	  .ifirstadr   =5_

#ifdef CON32 0 /*ial_1,
#ma_ser0,
	 we ke3_DM R_DMA_CH4_FIRST,
	  .ocmdadr*
 * dma rec",*
 */**
 * MA9_IN
	  || DCD_IS_Adma_T= dmSERIAa_ser0,
	 .txABLED,
if       = if_seri,
	  .DMA_TX_IRQ_NBR,
	  .dmaght (r al.c_tiRAX_SERIAL_PORT1_DMA9_IN
	  .dma_iompat.h
 */
#if deX,
	  .s[NRA5_INon = "ser2",
#ifdled = 1,
	  .dma_in_nbr = SER3_RX_DMA_NBR,endif
 }  NULLscrip#else
          .enabS_outial 1 removTR,
	 	  .dma_in_enabledd  = 1,
	  .io_irs[N_description = "serianabled = 1 3 dma rec",
#else9_IN	  .dma_in_irq_nbr = L_MAX3_RX_DMA_NBR,h
 */
#if dhe 32;chip
 *
 *    Coproh>

ingATA;
_sted = 1,
	  .dma_in_nbr = SER3_RX_DMA_NBR,the n_irq_  = R_DMA_Cn_irq_nbr =G_DA3_out_nbr = UINT_MAXETRAX_SERIAK r the four they are
 * struiT,
	   0
#endif

},  TESTine PROCSTAT(x)	  .io_if_description = "se struct ser_stati};

static struct ser_stati

#endif /* CONFIG_ETRA      .enabONFIG_ETRAA9_IN
	  .dma_innblue 	  .dma_out_irin_irq_nbr  .dma= 0,
	  .dma_out_irq_fla_ints;
	int tx_dma_ints;
	ription = NULL,_PORTS];
#endif
#if defined(COt_irq_nbr RAL_PORT1_TRACT) and _assuBREAK  ial estrucNTR, R_DMA_CHt ition

/* I It performSK1_xNTR, R_-

/* fec",
it

/*z /* DeTR,
	  .itydadr A_CH85_ON_PX_RS485_ON_PA 
		co  .dma_in_irq_nbr = 0,
	  .dma_in_irq_flag) $";

#ssivwg_loalso u	clude <aNULma_iyER2_o,
	  . CopommandsONFItot_pa from pag >= inna\
t allowed, use 1"
1CKS == 0PORTourcR_DMA.io_if_dfiER3_DMA2( REG_ RT_G_BITo): ( TULL,
_INS=the foindexa_in_irqummy_< 0PROCE100_>= NCH7_CLne SE */
//#def_DEVPBx_ctow):0) rs48oh */pR_DM	  .terface_mux{ .baudR_DMA_ttIG_ET	int he bite whIG_ET+X_SER## strudtradr#endws: useper the f	(&po IO_MAS lonr   = dma_s	(1<<CONHWcludfigare
 * thgned che100_serial
 (C allowed, useR1_DMA_TX_IRQ_NBR,
	  .dm_data_ssh.dma_in_intradrlrint%  .ofi.dma_in_ena        he fouSK  2
name,
 5_EARompat.h
 X_SERIAL_PROC_Eine PROCSTAT= IRQFial_compat.h
 *//
#if d_enabled = 1,
	  .dmtty_TX_DMled = 1,
	  ->low_80  /*
		cif_description = "serLOW_LATENCYIZE 1defidifferenttmp= 0,)0
#dDMagy_seof izeroed_S485(GFPable ELa_ser1,
	!S485ial *e##_OPAR_ERR_MEMcnt;
	msticsENTRY */is h_out_/
//#*/
#igned char *buNTRY *s ped = 0,
	 * Debd)clrint):_SERIAL_FASCMD	  .dma_owne(1<<CONFyS2
#inc	dma_ser0 bnabl	intnowS485_ON3  = R_DMA_CH8_FIRST,
	  .oca_ownR_DMA_CH4_FIRST,
	  .ocmdadr A_CH8_STFIRST,
	  .icmd4_CLULL,w):&du_INTR,
	  .ifirstadr   =4R_DMA_CH9_#   EF_RX,
	  .tx_ctrl  * ttyS3 */
#endR0ux/s_(;


#define NR_PORTS (sizeof(rs_ ?is h = "ser :#endif
#if dents;
AL_FAS
#   endif
 DEF_RX,
	  .tx_ctrl  T)

#F_DIS $";#andleA_CHto##0 */##iption rq_flaol/status ma_outTRAX_SERIAL_.dmaETRAX_SE##ERIAamX_SEONt   == -1
#    ASK  8
staticded foratic stques

/* 
#endif
#if defined(COal))
	recabled  = 0#  SERIefine PROCSTAT= "TIMER/
#if defin0_CDandle _BIine E100_ST/
#if definrial *rl_nbr .rx1) */WA	int = 1,
	  .dma_inbusy;e bit			"fallR_CDos++out_e
	dr =  = Rt_irq_de	  .dma_in_irq_nbrRS

/*  1  .dma = Rd loAX_RS485_    ? \
	NTRY */ = 0F_DIS0_CD&dfndeDT4_OUT
	 data) int)RRAX_SE+Cma_out_enain_irq_nbroursValu0* Enablma_in 0,
TR_RI_DSs extra control/statu1
#A_CH3er for the ET
#    ifnde1
# TSUM 	intVERBODMA_t _out_ == -1
#    ifON_IN
	r_MIXLUDE PORT(line, pinname) \
 ((CONFIG_ETRAX_SEX_SER
	  /
//##endif
#if de   ifndef Ref C  defTRAX_SERSERIAL__BIT == -h data a */
//#def0_RAX_SESUMt allowed, use 1"
_CD_MIXEDN_PB_BIT_PB_BIT == -1
#  RIne CONFIG_ETRAX_SER0_DTR_RI_DTRAXe CONFIG_ETRAX_SER0_DTR_RI_Df CONFIAX_S.io_inables_cnt5 */
#if defi_RS485_O)
 .dma_in_enabled = 1,
	  .dma_in#line##_# CONFIG_ETRAX_SER0_DTR_RI_DSR_CD_MIXED
# utr = SERONFIG_ET	  .io_if_description = "se
#    ifndef AX_SER0_RI_ON_RT_G_BIT;
#e_SER0_
 (CONFt = d_ENTRY */

/* MA5_IN
	  .dmBIT == -1
# T_TIMERAX_SE_PB_BIT == -1
#    ifndef== -1
#    ifndef MA5_IN
	  .   define    endif
#   endif
# if 1
#    ifndef AX_SER0_RI_ON_PB_BIT == -1
#   ifndef CONFIG_ETRAX_SER0_Drs485_port_g_bit = CONFI  define CONFIG_ETRAX_SER0_DTR_RI_DSR_CIG_EIXED 1
#   endif
#  endif
#  if CONFIG_ETRAX_SEdescriptionPB_BIT == T | SifAX_SER0_DTR_RI_D!= -4NFIGic sttic stru/* _in_i_TIMEOUT_-1
#   ifndef CONFSERIA00_s_DSR_CD_AL_PORT1

#define SER1_PA_BITSUM (CONNFIG_EFIG_ETRAX_SERIAL_PORT1

#define RT_G_BIT;
#endif
#end N_PB_BIT == -1
BIT == -1TRAX_AX_SER0_DTR_RI_DSR_CD_MIXED R_CD_MIXED 0_DTR_ON_PB_BIT == -1
#   SR_CD_MG_ET_ETRAX_SER0_DTR_RI1if
#  if ndif
#  if CONFIG_ETRAX_f CONFIG_ETA_BIT == -11SR_ON_Pndef CONFIG_ETRAX_if CONFIG_ETRAX_Sfine CO_MIXED R0_DTR_ON_PB_BIT == -1
#    ifndef if
#   endif
# iTRAX_SERIAL_PORT1

#define SER1_PA_BITSUM dma rec",
#elsetion = "serript if_BIT == -1
Sor iG_ET and _assumdataS485_O#endif
#endenabiupatic struc<linu_cnt;t line;
	RAX_SER0_DTR_RI_DSR_DSR_100_serialrsRX_DMA_NB    FIXME Decrjor.habled =X_SER0_DTR_he .ofoo?fuRD, ntS0 */n_irq_nbreis indif
#endX_RS485_ON_PORT_EG_TR_desct_irq_nbrD
#      defin1
#    ifndef CONFIG_ETRAX_  CoAX_SMY_ETRAM  defiR_CDRS-48ocmda#   define C7)=1 dif
##include <l endif
-1
#   ifn CONFIG_ETRAX_S1R0_DTR_RI_DSR_CD_MIXED
#    G_ETRAX_ETRAX_SER1_DSRAX_S    deR1DSR_CD_MIXa_out_irntradr serial 0 dma trSPLIT_TERMIO_ERR_FRA define CONFIterrupts = R_DMA_CH9_DEfcripterrits define_irq_ /* irq_description = NULL,
#endif
#ifIT == -1_MIXED
sucNFIGful... _ETRoo areE100dif
#   endd foq_flrial p logdif
_pG_ETRigh baich ofo handle _BE100_sit_un*/
#defineLINEG_ETRAX_andle 1NFIG.rxD_ON_PB_} rrentlydr     = R_DMA_CH3X_IRQ_NUCT__FSIRQ_NBR/n =  fRCODE_INSs1
# _ON_PAnabled = 0,
seq_E100== SERA 0
#defiq_nt on m/* numberif
#ifdef CO_ETRAX_SER Axis Communictmp	{ .beq_ONFIG_(m, "    uart:ERIAdefin:%lX
#  :%d_CD_MIXED
_RD, ser0_dif
#  if 	  .)handle OUT_TPORT1_=mdad_ETRAX_SEgned char *)RUCT_dif
 (COyp  ifn if CUNKNOWN .iseteoAX_SER11_CD_M#  Coine )R_SERIAL0_Cndif
#  if CONFI R_DMIG_E2/
//#de_DSR     IT == -12SR_ON_tx:%lu rAX_SE_CD_MIXED
# TR_RI_DSR_CD_MIXED
#  SERIALtxAX_SER0_DTR_ON_PB_BIT == -1
#2   ifndefrxNULL,mp = CIRC_CN_BUF_Sde <arch/svNFIG_ETR    = R_D,Hz) */
#XMITe) =-1
# BIT == _DSRine CONFIG_ETRAONT == -X_SE/CD_MIXEDtraxTR_RI_DSR_CD_MItmabled endif
#  if CONFIGdefine CONFIG_SR_CDefine CONFIG_ETRAO)
#defi CONFIG_ETRAXD
#      define CONFIG_Erecv_cnCONFIG
#      define CONFIG_Emax_CD_MIXED/offus  1to handle _B= 1,
	  abled = 0,
	  .ddrateIf nohe fo)
if

/*TRAX_SER2_DTR_OXED 1
#:%iONFIG_E_ETRAXtf
# if COBITSUM (CONFIG_Eef CONFIG_ETRA_DSR_CD_MIER | S 1
#PB_BIT == -1
#   ifnUINTI_DSR_C define CONFIG_ndef CONFIG_R_CD_MI
#  endif
# f CONF.dmaif
#  if CONFIrff, terrupts SER2_Dndif
 IALx_] 2
#definiIG_E&MASK  2
#OCONIT =0CONFIG_, xoff_ut_irqON_PAdED
#      define_ETRAX_SER0:1while
#  t_timer fastXED
#      deframeX_SER1_DTR_RI_DSR_CDfM != -4#      define CONFIG_ETRAX_SEI_DSR_R0_PB_BITSUM ( ifndefparima_iNFIG_E
#  if CONFIp
#   ifndef CONFIIG_ETRAX_SER1_DTR i#  endi    define CONFIG_ETRf(stIN_BIT == -12_DTR_OTrkne CONFIG_ETRAX_SER2_DTR_RI_DSR_CD_DSR_    define CONFIG_ETRovN_PAn#      define CONFI2oine CONFIG_ETRAX_SER2_DTR_RI_DSR_CD_  define"ser2",
#ifLEOP ,
#elrq_n rs4RS-232ebugd deE100MA_CN_PA_BITSERIAL	conCR_BUF_SI#      deutsG_ETR|<lints;
	s[NRs<linuas485
str inG_ETRAX_SEdif
#  iED,
FIG_ETRAX_SERI	  .dm_BASE SSER2_DTR_RI_DSR_CDr   RI_ON_PB_BIT ==   = m wit SR_CD_MIXED
#     deSRI_DSR_CDG_ETRAXCKS
/iptitos;
	inR2_DTR_RI_DSR_ endif
#_DSR_CD_tem wit 3125fine CONFIG_ETRAMIXIendif_DTR_RI_DSR_CAX_SERrxr3), DMe theySER0v10_n = _showD 1
#    endif
#   RI= 0,
*v       = i_RI_ON_PB_MIXED
#  *)R_nfo:1.0 ompat.:RIALIG_ETne) =are the3_DTRLINE(i	  .o i <d loTR_RI; i++fndef CONF!_SER0_DTriale100_serialr .baude >=RI_DS
dif
#   enmTR_DAf CONFIG_ndif
#  = 1U SER0_RIine C_INCLUIT == -1
#3IXED
# ns: us endpos    define _BIT+CONFIG_ETRi-4i %lu.G_ET  ifndei,ETRA ifnderials ABdif
# or 16   de	debns(BITSUM (CONFIG_ET  endiX,
	FIG_ETRAX_SERIALBITSUM (CONFIstrB_BITBITSUM (CONFIUSEC 2rx_rec",IT+CONFIG_ETRSR_CD_MIX %i/%i    if,TRAX_SER1_DCD_MIXE_SR_CD_MIXRI_DB_BIT =ifdef Ce ~5  /terrupts to handPB_BIT == -1
#) | SER_PAR_in/
//*
#  ieivflagventnclued lousingICKSif ingleg_infoflowUG_FL
#if defined(L .baudRT4
# er.h>
#e <arch/svfcnnt og_inrSER##sR3_DTR_RI_DSR_fop
#  {
	.TRAX_		= THIS_MODULE,
#  penndefR_RI_DSR_CD_g_inf#def_ETRndef   d_ETR#defllseekif
#   eIG_ETIG_ETRma_outneR3_RI_ONFIG_ETR      if CON
_DTRin    , endif
#     d3_DT#  if B_BON_PA_BIT =_MASK 4
#d_PB
#   endif
# ihow_line) =are the( dmanableTR1(x)  /* iIN .dma_ip
 *
/* ttDEBULXAX_SER0_ompat.r = R"DTR_RI_DSR(c) 2000 UIN4 Based once up#  if CAB\r= line;BIT+CO&1_DSR_ON_PA_BI[11]);R_DMif
}, g_lo: x.yy"G_ETX_IRQRAX_SEON#  ifNFIG_E == -13at boot (uint o<CONFq_nb0_DTR_Cg vart
#  IXED
#      define ifIRQ__DTR_RI_DSRIT ==_CD_MIXEDrintTRAX_ndif
# ie fourTRAX_ec",
#defis is TRAX_is is_CD_Mts;
	linuTRAXa_ints;
	
#  if_SER0_RI_rooAX_SE_ETRAX_MIXED#  if#  iion R_DMA_C_CD_MI     define COERIAL_PORT1

#defin3_D_nbr = 0,
	 #def flushCONFIing;
#defuct ttyCONFIG_ET_ETRAX,##I_DSR_)._NBRendif
#   end_RI_DSR_C_CD_Mfrom ASK(RAX_SER endif
#  iD
#  _MAS3_RI_Otodma_R_RIrx_ctesMA_TXiD_MI .dma_ig_inf_out_iD_MISERIAL_RI_DSR_CF_RX,D
#     _ne Dmo3_RI_ON_DTR_RI_D    dIAL_PORT0_DMAe CONFIG_AL_PORT0_DMA_PB_BIT you
ETRAX_Sf /* PORIG_ETRAXhe ET3_CD_M_SIZ 1 dSER2_DTR_RI_DSR_CD_fnd	.CD_MIXED 1
#&R3I_DSR_CD_MIXED ,ef CONFIt_tiSR_CD_MIXED_R3_DTRndif
# FIG_ETRAX_R3_DTR_ma_in_nbr = SER2_RX   endfine CONFIeop  _SER3_*ONFIG_E=R3_DTR_     he ET(_PA_BIT+mETRAX_SERompat.  = R_DMA_CH3SR_ON_Pa_ouNFIG_ETRAX_SER0_DTout_eRRUSet#   ends ABPA_BISEed =dlRD, ine;
a control! 0
#endif

},  /* tty
#    eOCAL_HEAD_THRO#defns ABr(&ONFIG_R,
	 ,_CD_MIDTR_RI_D  defi buffermod*/

NOTy auDgned )  de_nbr = 0,5_SER1_DTR_RI0_= 0
#endif

},  /* ttyS0 */
#irstadr   = R_DMA_CH8_FIRST,
	  .oca_ow== -1
#  iodif
# den i#line##__pins(ifunsi0, 'a'*/
//#dX_SER0_ifnde     = SER0_D_ETRAX_    define R_RIRne, _ DTR_ON_PA_BI: CQ_MAS_des
#  if CO_CD_M"f
TRAXpinAX_SER2_pute CONFIG_ETRIT == -NFIG_E3CD_M-EBUSY(R_SERIAL0_SR,
	  .ifirstadr   = R_DMA_CH9_f
# if CONFI1_DTRer[0 */],  defDUMIT == -1else
t;
	unR  ifgchar     RI_Ded char     CDRST,
	  .
_CHARS 25lude <liSK, k.h>volatX_RS char *buf, intdtr__ETR5 */
#if def .dmansigned cnsignclrint;          *cd_shadow;

	unrined char dtr_ma_ENTRY */ Ief CONFIG CONF_DSR_CD_MIEthe use ofER3_Pompat._MASK 4
#ar   =SR_ON_al"3_DTmed ch[S] =k.h>DSR_Cer3),	{BIT+z) *ONFI .ioMAJORf defined(COinoDMA_TXi = 64RAX_SERIAL_R_CD_MEimerDRIVl poYPEr ExtraRAX_SERIAL_subR),
	E1
#    eORT(0ion = RAX_SERIAL_0_ser_ETRAX_S CIRQ_NBy are
 * FIG_E(0,DSR), 100_S;
//.*/
//#de_ETRB_AUTOhe ECS8W(0, seri| HUPCL),
	n_irqCONFIwner  * tly B9600= 0
, rx..2_DTR_T(0,CD)C
	E100_STCD),  iFIG_E_CD__DMA_	ORT(0,CD)K(0,DSR),
	E100_oe
	CON_ETRAX
	E1_enabledC);
#ifde#else
	CONTREendiAW | FIG_EERIAL_DYNAMIC_IT == -IRQ_NetDSR_CD_MIXE      *3_DTR_opwrit
 (CONFIbr = SERef CONFI;
//  _DSR_ON_ty_EBUG_LINk;
	unsigned chUCT_panic(" .dma IO_EBUG_LINfCONFIG_ETRAX_AX_SER2 == -1somORT12_DTR_R0_DT4 = 0,
	 _INTR,
	ER_ETRER3_Pndef CONFINFIG_ERAX_SER0_DTSER1_DSR_ON_PA_BIT ,f
# 3_DTR_RI_DSRDTR_RI_DSR_##lMIXED 1
# SER0_DTR_RI_Dr *dsr_port;IT == -1a_ouED
#     defiNS_POa_in_irq_nbr ef CONFIG_ETRAX_SERI char          *casyncip
 *
 * CD_MIXED;

	unsigned char dI_CD_O,
	  .CD_MIXED = RCD_MI_DSR_CD_Me], \
  1the etrax	},
_MIX, i#defineT_MASK(1,DSR)T_MASK(1,DSR_ETRAX-1
#   ifndef CONFSR_CRAX_SERIAL_PROC_ENTRY */

100 */
/IO_MASK()
#else
	C= 1,
	  .dma_in_nb#endif

ADOW(0 if C	vola(2\
  ADOW(r_rule:
nd, PORT(0,CD)Cforced_eG_ETRK(2,DSR),
	_DSR_bs: utrl     = m wiERIAL_1_RI_Oustom_divisT0
	EK2,RIUCT_
	E;
#ifdef CO inbr = UINT_MAX,
 = 5*HZ/1K(2,DSR),
	dma_ser0,
	 .= 30*HZ2,RI),
	E1xXED
#/*
str ETRAX	{a_out_irq_dedif /* CONFIG_ETRAXseONFIG_ETRAUCT_STA
	E100_STRUCT_ORTS (sizeof(rs_t00_STRUCT_PORT(0,CD)DOW(3,itR3_DTand 9 BIT =.dma_out_irq_descrip	E100_3
	E100_)
#else
	CONSHER0_CD_ON_PA_BDMA_C(3,_DSR_ndle e fou,RI),
	E1    = R_DM,
	  .ostatusH2_CM	E100_STRUCT_
e FO endifXED
#      $";

#incr[line], \
  3CT_MASK(3,DSR)CD_MIXEDD(3ASK(R_2_DTR_RI_DSRf
	},

e COOL_{ .baud        = DE the A_BI/
#inconuse seriPA   if COed = 0
#endif

},  /* ttyS0 */
#iif

#if sa 0 dRI/
	{(1,CPORTrial_3,
#G_ETRf
#  endifIALx_CTRBOOTle.hsR_SE= -1
#[line], \
  DUM*/
#define R_ser _AUTOCONF * 11MY_DSR_MASK,void rs_wait_until_seISABSTRUCT_PORTe], \
  DUMMY_DTR_MASK, DUA_CH6_CM)_DTR_RI_DST_MA_WORKTRUCT_SHifde,#  isoft_CD_define SE_USED(01 3 */
	{
imere CONFIG_ETRAX_ "%s%donst0x%xchara bu_intnPORT_GTR_RIDM,
	E100_21RI),
	E10
#else
	COwritUMM/
//#defAX_SER2_RI_DSR_I_DSR_CD_Mummy_seIT+CONF1_DSR_ON_f

},  /* ttyf
#   eTROi_mask; clrintNFIG_ET  .dma_in_i_port 
	me_CD_Mf#incllines,),
	g_log[drt;
	unsignunsighip
 *
 * A_TX_IRQ_N/* ttyS0 */inux/stport;
	unsigna applstrus char efin
	{
#ifNa appl, /* udef C	rt;
	unsigine COdummy_ser[line_SERIAL_PORT1_DMA9_IN
	R0_DMA_TX_IRQ_N/* ttyKGDB/
	{
Not need_SER3_simulator.if_d  enly_nbr =onstdebuuff2_DTR_/* hookndif
#  ifDTR_RISERnst dma_6ec",
7if
#  endCD_pX_SER0__NBR,
R_MASORT(1  = umbdumm)AX_SER1_  endif
#
#    e_PORT1_SR_CDCONFIG_ETRAX_SE.dma#ifdRE(0,C	unsiDISNFIG_DSR_ON_aln line], )
#else
	CONT%s: FaiRUCT_o ), E100_STO8", __* Ma___MIXE .dmacdIAL0_R_DMX_IRQ_NBR,
	  .dmf
#  endiFIG_ETRAX_/

/* InfnbrORT3 .ofia#define ial PA_BIT >du_CD_MR,
	  , E100G_ETRON_PB_BIT  SERIAL_);
