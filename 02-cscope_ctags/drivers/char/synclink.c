/*
 * linux/drivers/char/synclink.c
 *
 * $Id: synclink.c,v 4.38 2005/11/07 16:30:34 paulkf Exp $
 *
 * Device driver for Microgate SyncLink ISA and PCI
 * high speed multiprotocol serial adapters.
 *
 * written by Paul Fulghum for Microgate Corporation
 * paulkf@microgate.com
 *
 * Microgate and SyncLink are trademarks of Microgate Corporation
 *
 * Derived from serial.c written by Theodore Ts'o and Linus Torvalds
 *
 * Original release 01/11/99
 *
 * This code is released under the GNU General Public License (GPL)
 *
 * This driver is primarily intended for use in synchronous
 * HDLC mode. Asynchronous mode is also provided.
 *
 * When operating in synchronous mode, each call to mgsl_write()
 * contains exactly one complete HDLC frame. Calling mgsl_put_char
 * will start assembling an HDLC frame that will not be sent until
 * mgsl_flush_chars or mgsl_write is called.
 * 
 * Synchronous receive data is reported as complete frames. To accomplish
 * this, the TTY flip buffer is bypassed (too small to hold largest
 * frame and may fragment frames) and the line discipline
 * receive entry point is called directly.
 *
 * This driver has been tested with a slightly modified ppp.c driver
 * for synchronous PPP.
 *
 * 2000/02/16
 * Added interface for syncppp.c driver (an alternate synchronous PPP
 * implementation that also supports Cisco HDLC). Each device instance
 * registers as a tty device AND a network device (if dosyncppp option
 * is set for the device). The functionality is determined by which
 * device interface is opened.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(__i386__)
#  define BREAKPOINT() asm("   int $3");
#else
#  define BREAKPOINT() { }
#endif

#define MAX_ISA_DEVICES 10
#define MAX_PCI_DEVICES 10
#define MAX_TOTAL_DEVICES 20

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/synclink.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/dma.h>
#include <linux/bitops.h>
#include <asm/types.h>
#include <linux/termios.h>
#include <linux/workqueue.h>
#include <linux/hdlc.h>
#include <linux/dma-mapping.h>

#if defined(CONFIG_HDLC) || (defined(CONFIG_HDLC_MODULE) && defined(CONFIG_SYNCLINK_MODULE))
#define SYNCLINK_GENERIC_HDLC 1
#else
#define SYNCLINK_GENERIC_HDLC 0
#endif

#define GET_USER(error,value,addr) error = get_user(value,addr)
#define COPY_FROM_USER(error,dest,src,size) error = copy_from_user(dest,src,size) ? -EFAULT : 0
#define PUT_USER(error,value,addr) error = put_user(value,addr)
#define COPY_TO_USER(error,dest,src,size) error = copy_to_user(dest,src,size) ? -EFAULT : 0

#include <asm/uaccess.h>

#define RCLRVALUE 0xffff

static MGSL_PARAMS default_params = {
	MGSL_MODE_HDLC,			/* unsigned long mode */
	0,				/* unsigned char loopback; */
	HDLC_FLAG_UNDERRUN_ABORT15,	/* unsigned short flags; */
	HDLC_ENCODING_NRZI_SPACE,	/* unsigned char encoding; */
	0,				/* unsigned long clock_speed; */
	0xff,				/* unsigned char addr_filter; */
	HDLC_CRC_16_CCITT,		/* unsigned short crc_type; */
	HDLC_PREAMBLE_LENGTH_8BITS,	/* unsigned char preamble_length; */
	HDLC_PREAMBLE_PATTERN_NONE,	/* unsigned char preamble; */
	9600,				/* unsigned long data_rate; */
	8,				/* unsigned char data_bits; */
	1,				/* unsigned char stop_bits; */
	ASYNC_PARITY_NONE		/* unsigned char parity; */
};

#define SHARED_MEM_ADDRESS_SIZE 0x40000
#define BUFFERLISTSIZE 4096
#define DMABUFFERSIZE 4096
#define MAXRXFRAMES 7

typedef struct _DMABUFFERENTRY
{
	u32 phys_addr;	/* 32-bit flat physical address of data buffer */
	volatile u16 count;	/* buffer size/data count */
	volatile u16 status;	/* Control/status field */
	volatile u16 rcc;	/* character count field */
	u16 reserved;	/* padding required by 16C32 */
	u32 link;	/* 32-bit flat link to next buffer entry */
	char *virt_addr;	/* virtual address of data buffer */
	u32 phys_entry;	/* physical address of this buffer entry */
	dma_addr_t dma_addr;
} DMABUFFERENTRY, *DMAPBUFFERENTRY;

/* The queue of BH actions to be performed */

#define BH_RECEIVE  1
#define BH_TRANSMIT 2
#define BH_STATUS   4

#define IO_PIN_SHUTDOWN_LIMIT 100

struct	_input_signal_events {
	int	ri_up;	
	int	ri_down;
	int	dsr_up;
	int	dsr_down;
	int	dcd_up;
	int	dcd_down;
	int	cts_up;
	int	cts_down;
};

/* transmit holding buffer definitions*/
#define MAX_TX_HOLDING_BUFFERS 5
struct tx_holding_buffer {
	int	buffer_size;
	unsigned char *	buffer;
};


/*
 * Device instance data structure
 */
 
struct mgsl_struct {
	int			magic;
	struct tty_port		port;
	int			line;
	int                     hw_version;
	
	struct mgsl_icount	icount;
	
	int			timeout;
	int			x_char;		/* xon/xoff character */
	u16			read_status_mask;
	u16			ignore_status_mask;	
	unsigned char 		*xmit_buf;
	int			xmit_head;
	int			xmit_tail;
	int			xmit_cnt;
	
	wait_queue_head_t	status_event_wait_q;
	wait_queue_head_t	event_wait_q;
	struct timer_list	tx_timer;	/* HDLC transmit timeout timer */
	struct mgsl_struct	*next_device;	/* device list link */
	
	spinlock_t irq_spinlock;		/* spinlock for synchronizing with ISR */
	struct work_struct task;		/* task structure for scheduling bh */

	u32 EventMask;			/* event trigger mask */
	u32 RecordedEvents;		/* pending events */

	u32 max_frame_size;		/* as set by device config */

	u32 pending_bh;

	bool bh_running;		/* Protection from multiple */
	int isr_overflow;
	bool bh_requested;
	
	int dcd_chkcount;		/* check counts to prevent */
	int cts_chkcount;		/* too many IRQs if a signal */
	int dsr_chkcount;		/* is floating */
	int ri_chkcount;

	char *buffer_list;		/* virtual address of Rx & Tx buffer lists */
	u32 buffer_list_phys;
	dma_addr_t buffer_list_dma_addr;

	unsigned int rx_buffer_count;	/* count of total allocated Rx buffers */
	DMABUFFERENTRY *rx_buffer_list;	/* list of receive buffer entries */
	unsigned int current_rx_buffer;

	int num_tx_dma_buffers;		/* number of tx dma frames required */
 	int tx_dma_buffers_used;
	unsigned int tx_buffer_count;	/* count of total allocated Tx buffers */
	DMABUFFERENTRY *tx_buffer_list;	/* list of transmit buffer entries */
	int start_tx_dma_buffer;	/* tx dma buffer to start tx dma operation */
	int current_tx_buffer;          /* next tx dma buffer to be loaded */
	
	unsigned char *intermediate_rxbuffer;

	int num_tx_holding_buffers;	/* number of tx holding buffer allocated */
	int get_tx_holding_index;  	/* next tx holding buffer for adapter to load */
	int put_tx_holding_index;  	/* next tx holding buffer to store user request */
	int tx_holding_count;		/* number of tx holding buffers waiting */
	struct tx_holding_buffer tx_holding_buffers[MAX_TX_HOLDING_BUFFERS];

	bool rx_enabled;
	bool rx_overflow;
	bool rx_rcc_underrun;

	bool tx_enabled;
	bool tx_active;
	u32 idle_mode;

	u16 cmr_value;
	u16 tcsr_value;

	char device_name[25];		/* device instance name */

	unsigned int bus_type;	/* expansion bus type (ISA,EISA,PCI) */
	unsigned char bus;		/* expansion bus number (zero based) */
	unsigned char function;		/* PCI device number */

	unsigned int io_base;		/* base I/O address of adapter */
	unsigned int io_addr_size;	/* size of the I/O address range */
	bool io_addr_requested;		/* true if I/O address requested */
	
	unsigned int irq_level;		/* interrupt level */
	unsigned long irq_flags;
	bool irq_requested;		/* true if IRQ requested */
	
	unsigned int dma_level;		/* DMA channel */
	bool dma_requested;		/* true if dma channel requested */

	u16 mbre_bit;
	u16 loopback_bits;
	u16 usc_idle_mode;

	MGSL_PARAMS params;		/* communications parameters */

	unsigned char serial_signals;	/* current serial signal states */

	bool irq_occurred;		/* for diagnostics use */
	unsigned int init_error;	/* Initialization startup error 		(DIAGS)	*/
	int	fDiagnosticsmode;	/* Driver in Diagnostic mode?			(DIAGS)	*/

	u32 last_mem_alloc;
	unsigned char* memory_base;	/* shared memory address (PCI only) */
	u32 phys_memory_base;
	bool shared_mem_requested;

	unsigned char* lcr_base;	/* local config registers (PCI only) */
	u32 phys_lcr_base;
	u32 lcr_offset;
	bool lcr_mem_requested;

	u32 misc_ctrl_value;
	char flag_buf[MAX_ASYNC_BUFFER_SIZE];
	char char_buf[MAX_ASYNC_BUFFER_SIZE];	
	bool drop_rts_on_tx_done;

	bool loopmode_insert_requested;
	bool loopmode_send_done_requested;
	
	struct	_input_signal_events	input_signal_events;

	/* generic HDLC device parts */
	int netcount;
	spinlock_t netlock;

#if SYNCLINK_GENERIC_HDLC
	struct net_device *netdev;
#endif
};

#define MGSL_MAGIC 0x5401

/*
 * The size of the serial xmit buffer is 1 page, or 4096 bytes
 */
#ifndef SERIAL_XMIT_SIZE
#define SERIAL_XMIT_SIZE 4096
#endif

/*
 * These macros define the offsets used in calculating the
 * I/O address of the specified USC registers.
 */


#define DCPIN 2		/* Bit 1 of I/O address */
#define SDPIN 4		/* Bit 2 of I/O address */

#define DCAR 0		/* DMA command/address register */
#define CCAR SDPIN		/* channel command/address register */
#define DATAREG DCPIN + SDPIN	/* serial data register */
#define MSBONLY 0x41
#define LSBONLY 0x40

/*
 * These macros define the register address (ordinal number)
 * used for writing address/value pairs to the USC.
 */

#define CMR	0x02	/* Channel mode Register */
#define CCSR	0x04	/* Channel Command/status Register */
#define CCR	0x06	/* Channel Control Register */
#define PSR	0x08	/* Port status Register */
#define PCR	0x0a	/* Port Control Register */
#define TMDR	0x0c	/* Test mode Data Register */
#define TMCR	0x0e	/* Test mode Control Register */
#define CMCR	0x10	/* Clock mode Control Register */
#define HCR	0x12	/* Hardware Configuration Register */
#define IVR	0x14	/* Interrupt Vector Register */
#define IOCR	0x16	/* Input/Output Control Register */
#define ICR	0x18	/* Interrupt Control Register */
#define DCCR	0x1a	/* Daisy Chain Control Register */
#define MISR	0x1c	/* Misc Interrupt status Register */
#define SICR	0x1e	/* status Interrupt Control Register */
#define RDR	0x20	/* Receive Data Register */
#define RMR	0x22	/* Receive mode Register */
#define RCSR	0x24	/* Receive Command/status Register */
#define RICR	0x26	/* Receive Interrupt Control Register */
#define RSR	0x28	/* Receive Sync Register */
#define RCLR	0x2a	/* Receive count Limit Register */
#define RCCR	0x2c	/* Receive Character count Register */
#define TC0R	0x2e	/* Time Constant 0 Register */
#define TDR	0x30	/* Transmit Data Register */
#define TMR	0x32	/* Transmit mode Register */
#define TCSR	0x34	/* Transmit Command/status Register */
#define TICR	0x36	/* Transmit Interrupt Control Register */
#define TSR	0x38	/* Transmit Sync Register */
#define TCLR	0x3a	/* Transmit count Limit Register */
#define TCCR	0x3c	/* Transmit Character count Register */
#define TC1R	0x3e	/* Time Constant 1 Register */


/*
 * MACRO DEFINITIONS FOR DMA REGISTERS
 */

#define DCR	0x06	/* DMA Control Register (shared) */
#define DACR	0x08	/* DMA Array count Register (shared) */
#define BDCR	0x12	/* Burst/Dwell Control Register (shared) */
#define DIVR	0x14	/* DMA Interrupt Vector Register (shared) */	
#define DICR	0x18	/* DMA Interrupt Control Register (shared) */
#define CDIR	0x1a	/* Clear DMA Interrupt Register (shared) */
#define SDIR	0x1c	/* Set DMA Interrupt Register (shared) */

#define TDMR	0x02	/* Transmit DMA mode Register */
#define TDIAR	0x1e	/* Transmit DMA Interrupt Arm Register */
#define TBCR	0x2a	/* Transmit Byte count Register */
#define TARL	0x2c	/* Transmit Address Register (low) */
#define TARU	0x2e	/* Transmit Address Register (high) */
#define NTBCR	0x3a	/* Next Transmit Byte count Register */
#define NTARL	0x3c	/* Next Transmit Address Register (low) */
#define NTARU	0x3e	/* Next Transmit Address Register (high) */

#define RDMR	0x82	/* Receive DMA mode Register (non-shared) */
#define RDIAR	0x9e	/* Receive DMA Interrupt Arm Register */
#define RBCR	0xaa	/* Receive Byte count Register */
#define RARL	0xac	/* Receive Address Register (low) */
#define RARU	0xae	/* Receive Address Register (high) */
#define NRBCR	0xba	/* Next Receive Byte count Register */
#define NRARL	0xbc	/* Next Receive Address Register (low) */
#define NRARU	0xbe	/* Next Receive Address Register (high) */


/*
 * MACRO DEFINITIONS FOR MODEM STATUS BITS
 */

#define MODEMSTATUS_DTR 0x80
#define MODEMSTATUS_DSR 0x40
#define MODEMSTATUS_RTS 0x20
#define MODEMSTATUS_CTS 0x10
#define MODEMSTATUS_RI  0x04
#define MODEMSTATUS_DCD 0x01


/*
 * Channel Command/Address Register (CCAR) Command Codes
 */

#define RTCmd_Null			0x0000
#define RTCmd_ResetHighestIus		0x1000
#define RTCmd_TriggerChannelLoadDma	0x2000
#define RTCmd_TriggerRxDma		0x2800
#define RTCmd_TriggerTxDma		0x3000
#define RTCmd_TriggerRxAndTxDma		0x3800
#define RTCmd_PurgeRxFifo		0x4800
#define RTCmd_PurgeTxFifo		0x5000
#define RTCmd_PurgeRxAndTxFifo		0x5800
#define RTCmd_LoadRcc			0x6800
#define RTCmd_LoadTcc			0x7000
#define RTCmd_LoadRccAndTcc		0x7800
#define RTCmd_LoadTC0			0x8800
#define RTCmd_LoadTC1			0x9000
#define RTCmd_LoadTC0AndTC1		0x9800
#define RTCmd_SerialDataLSBFirst	0xa000
#define RTCmd_SerialDataMSBFirst	0xa800
#define RTCmd_SelectBigEndian		0xb000
#define RTCmd_SelectLittleEndian	0xb800


/*
 * DMA Command/Address Register (DCAR) Command Codes
 */

#define DmaCmd_Null			0x0000
#define DmaCmd_ResetTxChannel		0x1000
#define DmaCmd_ResetRxChannel		0x1200
#define DmaCmd_StartTxChannel		0x2000
#define DmaCmd_StartRxChannel		0x2200
#define DmaCmd_ContinueTxChannel	0x3000
#define DmaCmd_ContinueRxChannel	0x3200
#define DmaCmd_PauseTxChannel		0x4000
#define DmaCmd_PauseRxChannel		0x4200
#define DmaCmd_AbortTxChannel		0x5000
#define DmaCmd_AbortRxChannel		0x5200
#define DmaCmd_InitTxChannel		0x7000
#define DmaCmd_InitRxChannel		0x7200
#define DmaCmd_ResetHighestDmaIus	0x8000
#define DmaCmd_ResetAllChannels		0x9000
#define DmaCmd_StartAllChannels		0xa000
#define DmaCmd_ContinueAllChannels	0xb000
#define DmaCmd_PauseAllChannels		0xc000
#define DmaCmd_AbortAllChannels		0xd000
#define DmaCmd_InitAllChannels		0xf000

#define TCmd_Null			0x0000
#define TCmd_ClearTxCRC			0x2000
#define TCmd_SelectTicrTtsaData		0x4000
#define TCmd_SelectTicrTxFifostatus	0x5000
#define TCmd_SelectTicrIntLevel		0x6000
#define TCmd_SelectTicrdma_level		0x7000
#define TCmd_SendFrame			0x8000
#define TCmd_SendAbort			0x9000
#define TCmd_EnableDleInsertion		0xc000
#define TCmd_DisableDleInsertion	0xd000
#define TCmd_ClearEofEom		0xe000
#define TCmd_SetEofEom			0xf000

#define RCmd_Null			0x0000
#define RCmd_ClearRxCRC			0x2000
#define RCmd_EnterHuntmode		0x3000
#define RCmd_SelectRicrRtsaData		0x4000
#define RCmd_SelectRicrRxFifostatus	0x5000
#define RCmd_SelectRicrIntLevel		0x6000
#define RCmd_SelectRicrdma_level		0x7000

/*
 * Bits for enabling and disabling IRQs in Interrupt Control Register (ICR)
 */
 
#define RECEIVE_STATUS		BIT5
#define RECEIVE_DATA		BIT4
#define TRANSMIT_STATUS		BIT3
#define TRANSMIT_DATA		BIT2
#define IO_PIN			BIT1
#define MISC			BIT0


/*
 * Receive status Bits in Receive Command/status Register RCSR
 */

#define RXSTATUS_SHORT_FRAME		BIT8
#define RXSTATUS_CODE_VIOLATION		BIT8
#define RXSTATUS_EXITED_HUNT		BIT7
#define RXSTATUS_IDLE_RECEIVED		BIT6
#define RXSTATUS_BREAK_RECEIVED		BIT5
#define RXSTATUS_ABORT_RECEIVED		BIT5
#define RXSTATUS_RXBOUND		BIT4
#define RXSTATUS_CRC_ERROR		BIT3
#define RXSTATUS_FRAMING_ERROR		BIT3
#define RXSTATUS_ABORT			BIT2
#define RXSTATUS_PARITY_ERROR		BIT2
#define RXSTATUS_OVERRUN		BIT1
#define RXSTATUS_DATA_AVAILABLE		BIT0
#define RXSTATUS_ALL			0x01f6
#define usc_UnlatchRxstatusBits(a,b) usc_OutReg( (a), RCSR, (u16)((b) & RXSTATUS_ALL) )

/*
 * Values for setting transmit idle mode in 
 * Transmit Control/status Register (TCSR)
 */
#define IDLEMODE_FLAGS			0x0000
#define IDLEMODE_ALT_ONE_ZERO		0x0100
#define IDLEMODE_ZERO			0x0200
#define IDLEMODE_ONE			0x0300
#define IDLEMODE_ALT_MARK_SPACE		0x0500
#define IDLEMODE_SPACE			0x0600
#define IDLEMODE_MARK			0x0700
#define IDLEMODE_MASK			0x0700

/*
 * IUSC revision identifiers
 */
#define	IUSC_SL1660			0x4d44
#define IUSC_PRE_SL1660			0x4553

/*
 * Transmit status Bits in Transmit Command/status Register (TCSR)
 */

#define TCSR_PRESERVE			0x0F00

#define TCSR_UNDERWAIT			BIT11
#define TXSTATUS_PREAMBLE_SENT		BIT7
#define TXSTATUS_IDLE_SENT		BIT6
#define TXSTATUS_ABORT_SENT		BIT5
#define TXSTATUS_EOF_SENT		BIT4
#define TXSTATUS_EOM_SENT		BIT4
#define TXSTATUS_CRC_SENT		BIT3
#define TXSTATUS_ALL_SENT		BIT2
#define TXSTATUS_UNDERRUN		BIT1
#define TXSTATUS_FIFO_EMPTY		BIT0
#define TXSTATUS_ALL			0x00fa
#define usc_UnlatchTxstatusBits(a,b) usc_OutReg( (a), TCSR, (u16)((a)->tcsr_value + ((b) & 0x00FF)) )
				

#define MISCSTATUS_RXC_LATCHED		BIT15
#define MISCSTATUS_RXC			BIT14
#define MISCSTATUS_TXC_LATCHED		BIT13
#define MISCSTATUS_TXC			BIT12
#define MISCSTATUS_RI_LATCHED		BIT11
#define MISCSTATUS_RI			BIT10
#define MISCSTATUS_DSR_LATCHED		BIT9
#define MISCSTATUS_DSR			BIT8
#define MISCSTATUS_DCD_LATCHED		BIT7
#define MISCSTATUS_DCD			BIT6
#define MISCSTATUS_CTS_LATCHED		BIT5
#define MISCSTATUS_CTS			BIT4
#define MISCSTATUS_RCC_UNDERRUN		BIT3
#define MISCSTATUS_DPLL_NO_SYNC		BIT2
#define MISCSTATUS_BRG1_ZERO		BIT1
#define MISCSTATUS_BRG0_ZERO		BIT0

#define usc_UnlatchIostatusBits(a,b) usc_OutReg((a),MISR,(u16)((b) & 0xaaa0))
#define usc_UnlatchMiscstatusBits(a,b) usc_OutReg((a),MISR,(u16)((b) & 0x000f))

#define SICR_RXC_ACTIVE			BIT15
#define SICR_RXC_INACTIVE		BIT14
#define SICR_RXC			(BIT15+BIT14)
#define SICR_TXC_ACTIVE			BIT13
#define SICR_TXC_INACTIVE		BIT12
#define SICR_TXC			(BIT13+BIT12)
#define SICR_RI_ACTIVE			BIT11
#define SICR_RI_INACTIVE		BIT10
#define SICR_RI				(BIT11+BIT10)
#define SICR_DSR_ACTIVE			BIT9
#define SICR_DSR_INACTIVE		BIT8
#define SICR_DSR			(BIT9+BIT8)
#define SICR_DCD_ACTIVE			BIT7
#define SICR_DCD_INACTIVE		BIT6
#define SICR_DCD			(BIT7+BIT6)
#define SICR_CTS_ACTIVE			BIT5
#define SICR_CTS_INACTIVE		BIT4
#define SICR_CTS			(BIT5+BIT4)
#define SICR_RCC_UNDERFLOW		BIT3
#define SICR_DPLL_NO_SYNC		BIT2
#define SICR_BRG1_ZERO			BIT1
#define SICR_BRG0_ZERO			BIT0

void usc_DisableMasterIrqBit( struct mgsl_struct *info );
void usc_EnableMasterIrqBit( struct mgsl_struct *info );
void usc_EnableInterrupts( struct mgsl_struct *info, u16 IrqMask );
void usc_DisableInterrupts( struct mgsl_struct *info, u16 IrqMask );
void usc_ClearIrqPendingBits( struct mgsl_struct *info, u16 IrqMask );

#define usc_EnableInterrupts( a, b ) \
	usc_OutReg( (a), ICR, (u16)((usc_InReg((a),ICR) & 0xff00) + 0xc0 + (b)) )

#define usc_DisableInterrupts( a, b ) \
	usc_OutReg( (a), ICR, (u16)((usc_InReg((a),ICR) & 0xff00) + 0x80 + (b)) )

#define usc_EnableMasterIrqBit(a) \
	usc_OutReg( (a), ICR, (u16)((usc_InReg((a),ICR) & 0x0f00) + 0xb000) )

#define usc_DisableMasterIrqBit(a) \
	usc_OutReg( (a), ICR, (u16)(usc_InReg((a),ICR) & 0x7f00) )

#define usc_ClearIrqPendingBits( a, b ) usc_OutReg( (a), DCCR, 0x40 + (b) )

/*
 * Transmit status Bits in Transmit Control status Register (TCSR)
 * and Transmit Interrupt Control Register (TICR) (except BIT2, BIT0)
 */

#define TXSTATUS_PREAMBLE_SENT	BIT7
#define TXSTATUS_IDLE_SENT	BIT6
#define TXSTATUS_ABORT_SENT	BIT5
#define TXSTATUS_EOF		BIT4
#define TXSTATUS_CRC_SENT	BIT3
#define TXSTATUS_ALL_SENT	BIT2
#define TXSTATUS_UNDERRUN	BIT1
#define TXSTATUS_FIFO_EMPTY	BIT0

#define DICR_MASTER		BIT15
#define DICR_TRANSMIT		BIT0
#define DICR_RECEIVE		BIT1

#define usc_EnableDmaInterrupts(a,b) \
	usc_OutDmaReg( (a), DICR, (u16)(usc_InDmaReg((a),DICR) | (b)) )

#define usc_DisableDmaInterrupts(a,b) \
	usc_OutDmaReg( (a), DICR, (u16)(usc_InDmaReg((a),DICR) & ~(b)) )

#define usc_EnableStatusIrqs(a,b) \
	usc_OutReg( (a), SICR, (u16)(usc_InReg((a),SICR) | (b)) )

#define usc_DisablestatusIrqs(a,b) \
	usc_OutReg( (a), SICR, (u16)(usc_InReg((a),SICR) & ~(b)) )

/* Transmit status Bits in Transmit Control status Register (TCSR) */
/* and Transmit Interrupt Control Register (TICR) (except BIT2, BIT0) */


#define DISABLE_UNCONDITIONAL    0
#define DISABLE_END_OF_FRAME     1
#define ENABLE_UNCONDITIONAL     2
#define ENABLE_AUTO_CTS          3
#define ENABLE_AUTO_DCD          3
#define usc_EnableTransmitter(a,b) \
	usc_OutReg( (a), TMR, (u16)((usc_InReg((a),TMR) & 0xfffc) | (b)) )
#define usc_EnableReceiver(a,b) \
	usc_OutReg( (a), RMR, (u16)((usc_InReg((a),RMR) & 0xfffc) | (b)) )

static u16  usc_InDmaReg( struct mgsl_struct *info, u16 Port );
static void usc_OutDmaReg( struct mgsl_struct *info, u16 Port, u16 Value );
static void usc_DmaCmd( struct mgsl_struct *info, u16 Cmd );

static u16  usc_InReg( struct mgsl_struct *info, u16 Port );
static void usc_OutReg( struct mgsl_struct *info, u16 Port, u16 Value );
static void usc_RTCmd( struct mgsl_struct *info, u16 Cmd );
void usc_RCmd( struct mgsl_struct *info, u16 Cmd );
void usc_TCmd( struct mgsl_struct *info, u16 Cmd );

#define usc_TCmd(a,b) usc_OutReg((a), TCSR, (u16)((a)->tcsr_value + (b)))
#define usc_RCmd(a,b) usc_OutReg((a), RCSR, (b))

#define usc_SetTransmitSyncChars(a,s0,s1) usc_OutReg((a), TSR, (u16)(((u16)s0<<8)|(u16)s1))

static void usc_process_rxoverrun_sync( struct mgsl_struct *info );
static void usc_start_receiver( struct mgsl_struct *info );
static void usc_stop_receiver( struct mgsl_struct *info );

static void usc_start_transmitter( struct mgsl_struct *info );
static void usc_stop_transmitter( struct mgsl_struct *info );
static void usc_set_txidle( struct mgsl_struct *info );
static void usc_load_txfifo( struct mgsl_struct *info );

static void usc_enable_aux_clock( struct mgsl_struct *info, u32 DataRate );
static void usc_enable_loopback( struct mgsl_struct *info, int enable );

static void usc_get_serial_signals( struct mgsl_struct *info );
static void usc_set_serial_signals( struct mgsl_struct *info );

static void usc_reset( struct mgsl_struct *info );

static void usc_set_sync_mode( struct mgsl_struct *info );
static void usc_set_sdlc_mode( struct mgsl_struct *info );
static void usc_set_async_mode( struct mgsl_struct *info );
static void usc_enable_async_clock( struct mgsl_struct *info, u32 DataRate );

static void usc_loopback_frame( struct mgsl_struct *info );

static void mgsl_tx_timeout(unsigned long context);


static void usc_loopmode_cancel_transmit( struct mgsl_struct * info );
static void usc_loopmode_insert_request( struct mgsl_struct * info );
static int usc_loopmode_active( struct mgsl_struct * info);
static void usc_loopmode_send_done( struct mgsl_struct * info );

static int mgsl_ioctl_common(struct mgsl_struct *info, unsigned int cmd, unsigned long arg);

#if SYNCLINK_GENERIC_HDLC
#define dev_to_port(D) (dev_to_hdlc(D)->priv)
static void hdlcdev_tx_done(struct mgsl_struct *info);
static void hdlcdev_rx(struct mgsl_struct *info, char *buf, int size);
static int  hdlcdev_init(struct mgsl_struct *info);
static void hdlcdev_exit(struct mgsl_struct *info);
#endif

/*
 * Defines a BUS descriptor value for the PCI adapter
 * local bus address ranges.
 */

#define BUS_DESCRIPTOR( WrHold, WrDly, RdDly, Nwdd, Nwad, Nxda, Nrdd, Nrad ) \
(0x00400020 + \
((WrHold) << 30) + \
((WrDly)  << 28) + \
((RdDly)  << 26) + \
((Nwdd)   << 20) + \
((Nwad)   << 15) + \
((Nxda)   << 13) + \
((Nrdd)   << 11) + \
((Nrad)   <<  6) )

static void mgsl_trace_block(struct mgsl_struct *info,const char* data, int count, int xmit);

/*
 * Adapter diagnostic routines
 */
static bool mgsl_register_test( struct mgsl_struct *info );
static bool mgsl_irq_test( struct mgsl_struct *info );
static bool mgsl_dma_test( struct mgsl_struct *info );
static bool mgsl_memory_test( struct mgsl_struct *info );
static int mgsl_adapter_test( struct mgsl_struct *info );

/*
 * device and resource management routines
 */
static int mgsl_claim_resources(struct mgsl_struct *info);
static void mgsl_release_resources(struct mgsl_struct *info);
static void mgsl_add_device(struct mgsl_struct *info);
static struct mgsl_struct* mgsl_allocate_device(void);

/*
 * DMA buffer manupulation functions.
 */
static void mgsl_free_rx_frame_buffers( struct mgsl_struct *info, unsigned int StartIndex, unsigned int EndIndex );
static bool mgsl_get_rx_frame( struct mgsl_struct *info );
static bool mgsl_get_raw_rx_frame( struct mgsl_struct *info );
static void mgsl_reset_rx_dma_buffers( struct mgsl_struct *info );
static void mgsl_reset_tx_dma_buffers( struct mgsl_struct *info );
static int num_free_tx_dma_buffers(struct mgsl_struct *info);
static void mgsl_load_tx_dma_buffer( struct mgsl_struct *info, const char *Buffer, unsigned int BufferSize);
static void mgsl_load_pci_memory(char* TargetPtr, const char* SourcePtr, unsigned short count);

/*
 * DMA and Shared Memory buffer allocation and formatting
 */
static int  mgsl_allocate_dma_buffers(struct mgsl_struct *info);
static void mgsl_free_dma_buffers(struct mgsl_struct *info);
static int  mgsl_alloc_frame_memory(struct mgsl_struct *info, DMABUFFERENTRY *BufferList,int Buffercount);
static void mgsl_free_frame_memory(struct mgsl_struct *info, DMABUFFERENTRY *BufferList,int Buffercount);
static int  mgsl_alloc_buffer_list_memory(struct mgsl_struct *info);
static void mgsl_free_buffer_list_memory(struct mgsl_struct *info);
static int mgsl_alloc_intermediate_rxbuffer_memory(struct mgsl_struct *info);
static void mgsl_free_intermediate_rxbuffer_memory(struct mgsl_struct *info);
static int mgsl_alloc_intermediate_txbuffer_memory(struct mgsl_struct *info);
static void mgsl_free_intermediate_txbuffer_memory(struct mgsl_struct *info);
static bool load_next_tx_holding_buffer(struct mgsl_struct *info);
static int save_tx_buffer_request(struct mgsl_struct *info,const char *Buffer, unsigned int BufferSize);

/*
 * Bottom half interrupt handlers
 */
static void mgsl_bh_handler(struct work_struct *work);
static void mgsl_bh_receive(struct mgsl_struct *info);
static void mgsl_bh_transmit(struct mgsl_struct *info);
static void mgsl_bh_status(struct mgsl_struct *info);

/*
 * Interrupt handler routines and dispatch table.
 */
static void mgsl_isr_null( struct mgsl_struct *info );
static void mgsl_isr_transmit_data( struct mgsl_struct *info );
static void mgsl_isr_receive_data( struct mgsl_struct *info );
static void mgsl_isr_receive_status( struct mgsl_struct *info );
static void mgsl_isr_transmit_status( struct mgsl_struct *info );
static void mgsl_isr_io_pin( struct mgsl_struct *info );
static void mgsl_isr_misc( struct mgsl_struct *info );
static void mgsl_isr_receive_dma( struct mgsl_struct *info );
static void mgsl_isr_transmit_dma( struct mgsl_struct *info );

typedef void (*isr_dispatch_func)(struct mgsl_struct *);

static isr_dispatch_func UscIsrTable[7] =
{
	mgsl_isr_null,
	mgsl_isr_misc,
	mgsl_isr_io_pin,
	mgsl_isr_transmit_data,
	mgsl_isr_transmit_status,
	mgsl_isr_receive_data,
	mgsl_isr_receive_status
};

/*
 * ioctl call handlers
 */
static int tiocmget(struct tty_struct *tty, struct file *file);
static int tiocmset(struct tty_struct *tty, struct file *file,
		    unsigned int set, unsigned int clear);
static int mgsl_get_stats(struct mgsl_struct * info, struct mgsl_icount
	__user *user_icount);
static int mgsl_get_params(struct mgsl_struct * info, MGSL_PARAMS  __user *user_params);
static int mgsl_set_params(struct mgsl_struct * info, MGSL_PARAMS  __user *new_params);
static int mgsl_get_txidle(struct mgsl_struct * info, int __user *idle_mode);
static int mgsl_set_txidle(struct mgsl_struct * info, int idle_mode);
static int mgsl_txenable(struct mgsl_struct * info, int enable);
static int mgsl_txabort(struct mgsl_struct * info);
static int mgsl_rxenable(struct mgsl_struct * info, int enable);
static int mgsl_wait_event(struct mgsl_struct * info, int __user *mask);
static int mgsl_loopmode_send_done( struct mgsl_struct * info );

/* set non-zero on successful registration with PCI subsystem */
static bool pci_registered;

/*
 * Global linked list of SyncLink devices
 */
static struct mgsl_struct *mgsl_device_list;
static int mgsl_device_count;

/*
 * Set this param to non-zero to load eax with the
 * .text section address and breakpoint on module load.
 * This is useful for use with gdb and add-symbol-file command.
 */
static int break_on_load;

/*
 * Driver major number, defaults to zero to get auto
 * assigned major number. May be forced as module parameter.
 */
static int ttymajor;

/*
 * Array of user specified options for ISA adapters.
 */
static int io[MAX_ISA_DEVICES];
static int irq[MAX_ISA_DEVICES];
static int dma[MAX_ISA_DEVICES];
static int debug_level;
static int maxframe[MAX_TOTAL_DEVICES];
static int txdmabufs[MAX_TOTAL_DEVICES];
static int txholdbufs[MAX_TOTAL_DEVICES];
	
module_param(break_on_load, bool, 0);
module_param(ttymajor, int, 0);
module_param_array(io, int, NULL, 0);
module_param_array(irq, int, NULL, 0);
module_param_array(dma, int, NULL, 0);
module_param(debug_level, int, 0);
module_param_array(maxframe, int, NULL, 0);
module_param_array(txdmabufs, int, NULL, 0);
module_param_array(txholdbufs, int, NULL, 0);

static char *driver_name = "SyncLink serial driver";
static char *driver_version = "$Revision: 4.38 $";

static int synclink_init_one (struct pci_dev *dev,
				     const struct pci_device_id *ent);
static void synclink_remove_one (struct pci_dev *dev);

static struct pci_device_id synclink_pci_tbl[] = {
	{ PCI_VENDOR_ID_MICROGATE, PCI_DEVICE_ID_MICROGATE_USC, PCI_ANY_ID, PCI_ANY_ID, },
	{ PCI_VENDOR_ID_MICROGATE, 0x0210, PCI_ANY_ID, PCI_ANY_ID, },
	{ 0, }, /* terminate list */
};
MODULE_DEVICE_TABLE(pci, synclink_pci_tbl);

MODULE_LICENSE("GPL");

static struct pci_driver synclink_pci_driver = {
	.name		= "synclink",
	.id_table	= synclink_pci_tbl,
	.probe		= synclink_init_one,
	.remove		= __devexit_p(synclink_remove_one),
};

static struct tty_driver *serial_driver;

/* number of characters left in xmit buffer before we ask for more */
#define WAKEUP_CHARS 256


static void mgsl_change_params(struct mgsl_struct *info);
static void mgsl_wait_until_sent(struct tty_struct *tty, int timeout);

/*
 * 1st function defined in .text section. Calling this function in
 * init_module() followed by a breakpoint allows a remote debugger
 * (gdb) to get the .text address for the add-symbol-file command.
 * This allows remote debugging of dynamically loadable modules.
 */
static void* mgsl_get_text_ptr(void)
{
	return mgsl_get_text_ptr;
}

static inline int mgsl_paranoia_check(struct mgsl_struct *info,
					char *name, const char *routine)
{
#ifdef MGSL_PARANOIA_CHECK
	static const char *badmagic =
		"Warning: bad magic number for mgsl struct (%s) in %s\n";
	static const char *badinfo =
		"Warning: null mgsl_struct for (%s) in %s\n";

	if (!info) {
		printk(badinfo, name, routine);
		return 1;
	}
	if (info->magic != MGSL_MAGIC) {
		printk(badmagic, name, routine);
		return 1;
	}
#else
	if (!info)
		return 1;
#endif
	return 0;
}

/**
 * line discipline callback wrappers
 *
 * The wrappers maintain line discipline references
 * while calling into the line discipline.
 *
 * ldisc_receive_buf  - pass receive data to line discipline
 */

static void ldisc_receive_buf(struct tty_struct *tty,
			      const __u8 *data, char *flags, int count)
{
	struct tty_ldisc *ld;
	if (!tty)
		return;
	ld = tty_ldisc_ref(tty);
	if (ld) {
		if (ld->ops->receive_buf)
			ld->ops->receive_buf(tty, data, flags, count);
		tty_ldisc_deref(ld);
	}
}

/* mgsl_stop()		throttle (stop) transmitter
 * 	
 * Arguments:		tty	pointer to tty info structure
 * Return Value:	None
 */
static void mgsl_stop(struct tty_struct *tty)
{
	struct mgsl_struct *info = tty->driver_data;
	unsigned long flags;
	
	if (mgsl_paranoia_check(info, tty->name, "mgsl_stop"))
		return;
	
	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk("mgsl_stop(%s)\n",info->device_name);	
		
	spin_lock_irqsave(&info->irq_spinlock,flags);
	if (info->tx_enabled)
	 	usc_stop_transmitter(info);
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	
}	/* end of mgsl_stop() */

/* mgsl_start()		release (start) transmitter
 * 	
 * Arguments:		tty	pointer to tty info structure
 * Return Value:	None
 */
static void mgsl_start(struct tty_struct *tty)
{
	struct mgsl_struct *info = tty->driver_data;
	unsigned long flags;
	
	if (mgsl_paranoia_check(info, tty->name, "mgsl_start"))
		return;
	
	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk("mgsl_start(%s)\n",info->device_name);	
		
	spin_lock_irqsave(&info->irq_spinlock,flags);
	if (!info->tx_enabled)
	 	usc_start_transmitter(info);
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	
}	/* end of mgsl_start() */

/*
 * Bottom half work queue access functions
 */

/* mgsl_bh_action()	Return next bottom half action to perform.
 * Return Value:	BH action code or 0 if nothing to do.
 */
static int mgsl_bh_action(struct mgsl_struct *info)
{
	unsigned long flags;
	int rc = 0;
	
	spin_lock_irqsave(&info->irq_spinlock,flags);

	if (info->pending_bh & BH_RECEIVE) {
		info->pending_bh &= ~BH_RECEIVE;
		rc = BH_RECEIVE;
	} else if (info->pending_bh & BH_TRANSMIT) {
		info->pending_bh &= ~BH_TRANSMIT;
		rc = BH_TRANSMIT;
	} else if (info->pending_bh & BH_STATUS) {
		info->pending_bh &= ~BH_STATUS;
		rc = BH_STATUS;
	}

	if (!rc) {
		/* Mark BH routine as complete */
		info->bh_running = false;
		info->bh_requested = false;
	}
	
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	
	return rc;
}

/*
 * 	Perform bottom half processing of work items queued by ISR.
 */
static void mgsl_bh_handler(struct work_struct *work)
{
	struct mgsl_struct *info =
		container_of(work, struct mgsl_struct, task);
	int action;

	if (!info)
		return;
		
	if ( debug_level >= DEBUG_LEVEL_BH )
		printk( "%s(%d):mgsl_bh_handler(%s) entry\n",
			__FILE__,__LINE__,info->device_name);
	
	info->bh_running = true;

	while((action = mgsl_bh_action(info)) != 0) {
	
		/* Process work item */
		if ( debug_level >= DEBUG_LEVEL_BH )
			printk( "%s(%d):mgsl_bh_handler() work item action=%d\n",
				__FILE__,__LINE__,action);

		switch (action) {
		
		case BH_RECEIVE:
			mgsl_bh_receive(info);
			break;
		case BH_TRANSMIT:
			mgsl_bh_transmit(info);
			break;
		case BH_STATUS:
			mgsl_bh_status(info);
			break;
		default:
			/* unknown work item ID */
			printk("Unknown work item ID=%08X!\n", action);
			break;
		}
	}

	if ( debug_level >= DEBUG_LEVEL_BH )
		printk( "%s(%d):mgsl_bh_handler(%s) exit\n",
			__FILE__,__LINE__,info->device_name);
}

static void mgsl_bh_receive(struct mgsl_struct *info)
{
	bool (*get_rx_frame)(struct mgsl_struct *info) =
		(info->params.mode == MGSL_MODE_HDLC ? mgsl_get_rx_frame : mgsl_get_raw_rx_frame);

	if ( debug_level >= DEBUG_LEVEL_BH )
		printk( "%s(%d):mgsl_bh_receive(%s)\n",
			__FILE__,__LINE__,info->device_name);
	
	do
	{
		if (info->rx_rcc_underrun) {
			unsigned long flags;
			spin_lock_irqsave(&info->irq_spinlock,flags);
			usc_start_receiver(info);
			spin_unlock_irqrestore(&info->irq_spinlock,flags);
			return;
		}
	} while(get_rx_frame(info));
}

static void mgsl_bh_transmit(struct mgsl_struct *info)
{
	struct tty_struct *tty = info->port.tty;
	unsigned long flags;
	
	if ( debug_level >= DEBUG_LEVEL_BH )
		printk( "%s(%d):mgsl_bh_transmit() entry on %s\n",
			__FILE__,__LINE__,info->device_name);

	if (tty)
		tty_wakeup(tty);

	/* if transmitter idle and loopmode_send_done_requested
	 * then start echoing RxD to TxD
	 */
	spin_lock_irqsave(&info->irq_spinlock,flags);
 	if ( !info->tx_active && info->loopmode_send_done_requested )
 		usc_loopmode_send_done( info );
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
}

static void mgsl_bh_status(struct mgsl_struct *info)
{
	if ( debug_level >= DEBUG_LEVEL_BH )
		printk( "%s(%d):mgsl_bh_status() entry on %s\n",
			__FILE__,__LINE__,info->device_name);

	info->ri_chkcount = 0;
	info->dsr_chkcount = 0;
	info->dcd_chkcount = 0;
	info->cts_chkcount = 0;
}

/* mgsl_isr_receive_status()
 * 
 *	Service a receive status interrupt. The type of status
 *	interrupt is indicated by the state of the RCSR.
 *	This is only used for HDLC mode.
 *
 * Arguments:		info	pointer to device instance data
 * Return Value:	None
 */
static void mgsl_isr_receive_status( struct mgsl_struct *info )
{
	u16 status = usc_InReg( info, RCSR );

	if ( debug_level >= DEBUG_LEVEL_ISR )	
		printk("%s(%d):mgsl_isr_receive_status status=%04X\n",
			__FILE__,__LINE__,status);
			
 	if ( (status & RXSTATUS_ABORT_RECEIVED) && 
		info->loopmode_insert_requested &&
 		usc_loopmode_active(info) )
 	{
		++info->icount.rxabort;
	 	info->loopmode_insert_requested = false;
 
 		/* clear CMR:13 to start echoing RxD to TxD */
		info->cmr_value &= ~BIT13;
 		usc_OutReg(info, CMR, info->cmr_value);
 
		/* disable received abort irq (no longer required) */
	 	usc_OutReg(info, RICR,
 			(usc_InReg(info, RICR) & ~RXSTATUS_ABORT_RECEIVED));
 	}

	if (status & (RXSTATUS_EXITED_HUNT + RXSTATUS_IDLE_RECEIVED)) {
		if (status & RXSTATUS_EXITED_HUNT)
			info->icount.exithunt++;
		if (status & RXSTATUS_IDLE_RECEIVED)
			info->icount.rxidle++;
		wake_up_interruptible(&info->event_wait_q);
	}

	if (status & RXSTATUS_OVERRUN){
		info->icount.rxover++;
		usc_process_rxoverrun_sync( info );
	}

	usc_ClearIrqPendingBits( info, RECEIVE_STATUS );
	usc_UnlatchRxstatusBits( info, status );

}	/* end of mgsl_isr_receive_status() */

/* mgsl_isr_transmit_status()
 * 
 * 	Service a transmit status interrupt
 *	HDLC mode :end of transmit frame
 *	Async mode:all data is sent
 * 	transmit status is indicated by bits in the TCSR.
 * 
 * Arguments:		info	       pointer to device instance data
 * Return Value:	None
 */
static void mgsl_isr_transmit_status( struct mgsl_struct *info )
{
	u16 status = usc_InReg( info, TCSR );

	if ( debug_level >= DEBUG_LEVEL_ISR )	
		printk("%s(%d):mgsl_isr_transmit_status status=%04X\n",
			__FILE__,__LINE__,status);
	
	usc_ClearIrqPendingBits( info, TRANSMIT_STATUS );
	usc_UnlatchTxstatusBits( info, status );
	
	if ( status & (TXSTATUS_UNDERRUN | TXSTATUS_ABORT_SENT) )
	{
		/* finished sending HDLC abort. This may leave	*/
		/* the TxFifo with data from the aborted frame	*/
		/* so purge the TxFifo. Also shutdown the DMA	*/
		/* channel in case there is data remaining in 	*/
		/* the DMA buffer				*/
 		usc_DmaCmd( info, DmaCmd_ResetTxChannel );
 		usc_RTCmd( info, RTCmd_PurgeTxFifo );
	}
 
	if ( status & TXSTATUS_EOF_SENT )
		info->icount.txok++;
	else if ( status & TXSTATUS_UNDERRUN )
		info->icount.txunder++;
	else if ( status & TXSTATUS_ABORT_SENT )
		info->icount.txabort++;
	else
		info->icount.txunder++;
			
	info->tx_active = false;
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;
	del_timer(&info->tx_timer);	
	
	if ( info->drop_rts_on_tx_done ) {
		usc_get_serial_signals( info );
		if ( info->serial_signals & SerialSignal_RTS ) {
			info->serial_signals &= ~SerialSignal_RTS;
			usc_set_serial_signals( info );
		}
		info->drop_rts_on_tx_done = false;
	}

#if SYNCLINK_GENERIC_HDLC
	if (info->netcount)
		hdlcdev_tx_done(info);
	else 
#endif
	{
		if (info->port.tty->stopped || info->port.tty->hw_stopped) {
			usc_stop_transmitter(info);
			return;
		}
		info->pending_bh |= BH_TRANSMIT;
	}

}	/* end of mgsl_isr_transmit_status() */

/* mgsl_isr_io_pin()
 * 
 * 	Service an Input/Output pin interrupt. The type of
 * 	interrupt is indicated by bits in the MISR
 * 	
 * Arguments:		info	       pointer to device instance data
 * Return Value:	None
 */
static void mgsl_isr_io_pin( struct mgsl_struct *info )
{
 	struct	mgsl_icount *icount;
	u16 status = usc_InReg( info, MISR );

	if ( debug_level >= DEBUG_LEVEL_ISR )	
		printk("%s(%d):mgsl_isr_io_pin status=%04X\n",
			__FILE__,__LINE__,status);
			
	usc_ClearIrqPendingBits( info, IO_PIN );
	usc_UnlatchIostatusBits( info, status );

	if (status & (MISCSTATUS_CTS_LATCHED | MISCSTATUS_DCD_LATCHED |
	              MISCSTATUS_DSR_LATCHED | MISCSTATUS_RI_LATCHED) ) {
		icount = &info->icount;
		/* update input line counters */
		if (status & MISCSTATUS_RI_LATCHED) {
			if ((info->ri_chkcount)++ >= IO_PIN_SHUTDOWN_LIMIT)
				usc_DisablestatusIrqs(info,SICR_RI);
			icount->rng++;
			if ( status & MISCSTATUS_RI )
				info->input_signal_events.ri_up++;	
			else
				info->input_signal_events.ri_down++;	
		}
		if (status & MISCSTATUS_DSR_LATCHED) {
			if ((info->dsr_chkcount)++ >= IO_PIN_SHUTDOWN_LIMIT)
				usc_DisablestatusIrqs(info,SICR_DSR);
			icount->dsr++;
			if ( status & MISCSTATUS_DSR )
				info->input_signal_events.dsr_up++;
			else
				info->input_signal_events.dsr_down++;
		}
		if (status & MISCSTATUS_DCD_LATCHED) {
			if ((info->dcd_chkcount)++ >= IO_PIN_SHUTDOWN_LIMIT)
				usc_DisablestatusIrqs(info,SICR_DCD);
			icount->dcd++;
			if (status & MISCSTATUS_DCD) {
				info->input_signal_events.dcd_up++;
			} else
				info->input_signal_events.dcd_down++;
#if SYNCLINK_GENERIC_HDLC
			if (info->netcount) {
				if (status & MISCSTATUS_DCD)
					netif_carrier_on(info->netdev);
				else
					netif_carrier_off(info->netdev);
			}
#endif
		}
		if (status & MISCSTATUS_CTS_LATCHED)
		{
			if ((info->cts_chkcount)++ >= IO_PIN_SHUTDOWN_LIMIT)
				usc_DisablestatusIrqs(info,SICR_CTS);
			icount->cts++;
			if ( status & MISCSTATUS_CTS )
				info->input_signal_events.cts_up++;
			else
				info->input_signal_events.cts_down++;
		}
		wake_up_interruptible(&info->status_event_wait_q);
		wake_up_interruptible(&info->event_wait_q);

		if ( (info->port.flags & ASYNC_CHECK_CD) && 
		     (status & MISCSTATUS_DCD_LATCHED) ) {
			if ( debug_level >= DEBUG_LEVEL_ISR )
				printk("%s CD now %s...", info->device_name,
				       (status & MISCSTATUS_DCD) ? "on" : "off");
			if (status & MISCSTATUS_DCD)
				wake_up_interruptible(&info->port.open_wait);
			else {
				if ( debug_level >= DEBUG_LEVEL_ISR )
					printk("doing serial hangup...");
				if (info->port.tty)
					tty_hangup(info->port.tty);
			}
		}
	
		if ( (info->port.flags & ASYNC_CTS_FLOW) && 
		     (status & MISCSTATUS_CTS_LATCHED) ) {
			if (info->port.tty->hw_stopped) {
				if (status & MISCSTATUS_CTS) {
					if ( debug_level >= DEBUG_LEVEL_ISR )
						printk("CTS tx start...");
					if (info->port.tty)
						info->port.tty->hw_stopped = 0;
					usc_start_transmitter(info);
					info->pending_bh |= BH_TRANSMIT;
					return;
				}
			} else {
				if (!(status & MISCSTATUS_CTS)) {
					if ( debug_level >= DEBUG_LEVEL_ISR )
						printk("CTS tx stop...");
					if (info->port.tty)
						info->port.tty->hw_stopped = 1;
					usc_stop_transmitter(info);
				}
			}
		}
	}

	info->pending_bh |= BH_STATUS;
	
	/* for diagnostics set IRQ flag */
	if ( status & MISCSTATUS_TXC_LATCHED ){
		usc_OutReg( info, SICR,
			(unsigned short)(usc_InReg(info,SICR) & ~(SICR_TXC_ACTIVE+SICR_TXC_INACTIVE)) );
		usc_UnlatchIostatusBits( info, MISCSTATUS_TXC_LATCHED );
		info->irq_occurred = true;
	}

}	/* end of mgsl_isr_io_pin() */

/* mgsl_isr_transmit_data()
 * 
 * 	Service a transmit data interrupt (async mode only).
 * 
 * Arguments:		info	pointer to device instance data
 * Return Value:	None
 */
static void mgsl_isr_transmit_data( struct mgsl_struct *info )
{
	if ( debug_level >= DEBUG_LEVEL_ISR )	
		printk("%s(%d):mgsl_isr_transmit_data xmit_cnt=%d\n",
			__FILE__,__LINE__,info->xmit_cnt);
			
	usc_ClearIrqPendingBits( info, TRANSMIT_DATA );
	
	if (info->port.tty->stopped || info->port.tty->hw_stopped) {
		usc_stop_transmitter(info);
		return;
	}
	
	if ( info->xmit_cnt )
		usc_load_txfifo( info );
	else
		info->tx_active = false;
		
	if (info->xmit_cnt < WAKEUP_CHARS)
		info->pending_bh |= BH_TRANSMIT;

}	/* end of mgsl_isr_transmit_data() */

/* mgsl_isr_receive_data()
 * 
 * 	Service a receive data interrupt. This occurs
 * 	when operating in asynchronous interrupt transfer mode.
 *	The receive data FIFO is flushed to the receive data buffers. 
 * 
 * Arguments:		info		pointer to device instance data
 * Return Value:	None
 */
static void mgsl_isr_receive_data( struct mgsl_struct *info )
{
	int Fifocount;
	u16 status;
	int work = 0;
	unsigned char DataByte;
 	struct tty_struct *tty = info->port.tty;
 	struct	mgsl_icount *icount = &info->icount;
	
	if ( debug_level >= DEBUG_LEVEL_ISR )	
		printk("%s(%d):mgsl_isr_receive_data\n",
			__FILE__,__LINE__);

	usc_ClearIrqPendingBits( info, RECEIVE_DATA );
	
	/* select FIFO status for RICR readback */
	usc_RCmd( info, RCmd_SelectRicrRxFifostatus );

	/* clear the Wordstatus bit so that status readback */
	/* only reflects the status of this byte */
	usc_OutReg( info, RICR+LSBONLY, (u16)(usc_InReg(info, RICR+LSBONLY) & ~BIT3 ));

	/* flush the receive FIFO */

	while( (Fifocount = (usc_InReg(info,RICR) >> 8)) ) {
		int flag;

		/* read one byte from RxFIFO */
		outw( (inw(info->io_base + CCAR) & 0x0780) | (RDR+LSBONLY),
		      info->io_base + CCAR );
		DataByte = inb( info->io_base + CCAR );

		/* get the status of the received byte */
		status = usc_InReg(info, RCSR);
		if ( status & (RXSTATUS_FRAMING_ERROR + RXSTATUS_PARITY_ERROR +
				RXSTATUS_OVERRUN + RXSTATUS_BREAK_RECEIVED) )
			usc_UnlatchRxstatusBits(info,RXSTATUS_ALL);
		
		icount->rx++;
		
		flag = 0;
		if ( status & (RXSTATUS_FRAMING_ERROR + RXSTATUS_PARITY_ERROR +
				RXSTATUS_OVERRUN + RXSTATUS_BREAK_RECEIVED) ) {
			printk("rxerr=%04X\n",status);					
			/* update error statistics */
			if ( status & RXSTATUS_BREAK_RECEIVED ) {
				status &= ~(RXSTATUS_FRAMING_ERROR + RXSTATUS_PARITY_ERROR);
				icount->brk++;
			} else if (status & RXSTATUS_PARITY_ERROR) 
				icount->parity++;
			else if (status & RXSTATUS_FRAMING_ERROR)
				icount->frame++;
			else if (status & RXSTATUS_OVERRUN) {
				/* must issue purge fifo cmd before */
				/* 16C32 accepts more receive chars */
				usc_RTCmd(info,RTCmd_PurgeRxFifo);
				icount->overrun++;
			}

			/* discard char if tty control flags say so */					
			if (status & info->ignore_status_mask)
				continue;
				
			status &= info->read_status_mask;
		
			if (status & RXSTATUS_BREAK_RECEIVED) {
				flag = TTY_BREAK;
				if (info->port.flags & ASYNC_SAK)
					do_SAK(tty);
			} else if (status & RXSTATUS_PARITY_ERROR)
				flag = TTY_PARITY;
			else if (status & RXSTATUS_FRAMING_ERROR)
				flag = TTY_FRAME;
		}	/* end of if (error) */
		tty_insert_flip_char(tty, DataByte, flag);
		if (status & RXSTATUS_OVERRUN) {
			/* Overrun is special, since it's
			 * reported immediately, and doesn't
			 * affect the current character
			 */
			work += tty_insert_flip_char(tty, 0, TTY_OVERRUN);
		}
	}

	if ( debug_level >= DEBUG_LEVEL_ISR ) {
		printk("%s(%d):rx=%d brk=%d parity=%d frame=%d overrun=%d\n",
			__FILE__,__LINE__,icount->rx,icount->brk,
			icount->parity,icount->frame,icount->overrun);
	}
			
	if(work)
		tty_flip_buffer_push(tty);
}

/* mgsl_isr_misc()
 * 
 * 	Service a miscellaneous interrupt source.
 * 	
 * Arguments:		info		pointer to device extension (instance data)
 * Return Value:	None
 */
static void mgsl_isr_misc( struct mgsl_struct *info )
{
	u16 status = usc_InReg( info, MISR );

	if ( debug_level >= DEBUG_LEVEL_ISR )	
		printk("%s(%d):mgsl_isr_misc status=%04X\n",
			__FILE__,__LINE__,status);
			
	if ((status & MISCSTATUS_RCC_UNDERRUN) &&
	    (info->params.mode == MGSL_MODE_HDLC)) {

		/* turn off receiver and rx DMA */
		usc_EnableReceiver(info,DISABLE_UNCONDITIONAL);
		usc_DmaCmd(info, DmaCmd_ResetRxChannel);
		usc_UnlatchRxstatusBits(info, RXSTATUS_ALL);
		usc_ClearIrqPendingBits(info, RECEIVE_DATA + RECEIVE_STATUS);
		usc_DisableInterrupts(info, RECEIVE_DATA + RECEIVE_STATUS);

		/* schedule BH handler to restart receiver */
		info->pending_bh |= BH_RECEIVE;
		info->rx_rcc_underrun = true;
	}

	usc_ClearIrqPendingBits( info, MISC );
	usc_UnlatchMiscstatusBits( info, status );

}	/* end of mgsl_isr_misc() */

/* mgsl_isr_null()
 *
 * 	Services undefined interrupt vectors from the
 * 	USC. (hence this function SHOULD never be called)
 * 
 * Arguments:		info		pointer to device extension (instance data)
 * Return Value:	None
 */
static void mgsl_isr_null( struct mgsl_struct *info )
{

}	/* end of mgsl_isr_null() */

/* mgsl_isr_receive_dma()
 * 
 * 	Service a receive DMA channel interrupt.
 * 	For this driver there are two sources of receive DMA interrupts
 * 	as identified in the Receive DMA mode Register (RDMR):
 * 
 * 	BIT3	EOA/EOL		End of List, all receive buffers in receive
 * 				buffer list have been filled (no more free buffers
 * 				available). The DMA controller has shut down.
 * 
 * 	BIT2	EOB		End of Buffer. This interrupt occurs when a receive
 * 				DMA buffer is terminated in response to completion
 * 				of a good frame or a frame with errors. The status
 * 				of the frame is stored in the buffer entry in the
 * 				list of receive buffer entries.
 * 
 * Arguments:		info		pointer to device instance data
 * Return Value:	None
 */
static void mgsl_isr_receive_dma( struct mgsl_struct *info )
{
	u16 status;
	
	/* clear interrupt pending and IUS bit for Rx DMA IRQ */
	usc_OutDmaReg( info, CDIR, BIT9+BIT1 );

	/* Read the receive DMA status to identify interrupt type. */
	/* This also clears the status bits. */
	status = usc_InDmaReg( info, RDMR );

	if ( debug_level >= DEBUG_LEVEL_ISR )	
		printk("%s(%d):mgsl_isr_receive_dma(%s) status=%04X\n",
			__FILE__,__LINE__,info->device_name,status);
			
	info->pending_bh |= BH_RECEIVE;
	
	if ( status & BIT3 ) {
		info->rx_overflow = true;
		info->icount.buf_overrun++;
	}

}	/* end of mgsl_isr_receive_dma() */

/* mgsl_isr_transmit_dma()
 *
 *	This function services a transmit DMA channel interrupt.
 *
 *	For this driver there is one source of transmit DMA interrupts
 *	as identified in the Transmit DMA Mode Register (TDMR):
 *
 *     	BIT2  EOB       End of Buffer. This interrupt occurs when a
 *     			transmit DMA buffer has been emptied.
 *
 *     	The driver maintains enough transmit DMA buffers to hold at least
 *     	one max frame size transmit frame. When operating in a buffered
 *     	transmit mode, there may be enough transmit DMA buffers to hold at
 *     	least two or more max frame size frames. On an EOB condition,
 *     	determine if there are any queued transmit buffers and copy into
 *     	transmit DMA buffers if we have room.
 *
 * Arguments:		info		pointer to device instance data
 * Return Value:	None
 */
static void mgsl_isr_transmit_dma( struct mgsl_struct *info )
{
	u16 status;

	/* clear interrupt pending and IUS bit for Tx DMA IRQ */
	usc_OutDmaReg(info, CDIR, BIT8+BIT0 );

	/* Read the transmit DMA status to identify interrupt type. */
	/* This also clears the status bits. */

	status = usc_InDmaReg( info, TDMR );

	if ( debug_level >= DEBUG_LEVEL_ISR )
		printk("%s(%d):mgsl_isr_transmit_dma(%s) status=%04X\n",
			__FILE__,__LINE__,info->device_name,status);

	if ( status & BIT2 ) {
		--info->tx_dma_buffers_used;

		/* if there are transmit frames queued,
		 *  try to load the next one
		 */
		if ( load_next_tx_holding_buffer(info) ) {
			/* if call returns non-zero value, we have
			 * at least one free tx holding buffer
			 */
			info->pending_bh |= BH_TRANSMIT;
		}
	}

}	/* end of mgsl_isr_transmit_dma() */

/* mgsl_interrupt()
 * 
 * 	Interrupt service routine entry point.
 * 	
 * Arguments:
 * 
 * 	irq		interrupt number that caused interrupt
 * 	dev_id		device ID supplied during interrupt registration
 * 	
 * Return Value: None
 */
static irqreturn_t mgsl_interrupt(int dummy, void *dev_id)
{
	struct mgsl_struct *info = dev_id;
	u16 UscVector;
	u16 DmaVector;

	if ( debug_level >= DEBUG_LEVEL_ISR )	
		printk(KERN_DEBUG "%s(%d):mgsl_interrupt(%d)entry.\n",
			__FILE__, __LINE__, info->irq_level);

	spin_lock(&info->irq_spinlock);

	for(;;) {
		/* Read the interrupt vectors from hardware. */
		UscVector = usc_InReg(info, IVR) >> 9;
		DmaVector = usc_InDmaReg(info, DIVR);
		
		if ( debug_level >= DEBUG_LEVEL_ISR )	
			printk("%s(%d):%s UscVector=%08X DmaVector=%08X\n",
				__FILE__,__LINE__,info->device_name,UscVector,DmaVector);
			
		if ( !UscVector && !DmaVector )
			break;
			
		/* Dispatch interrupt vector */
		if ( UscVector )
			(*UscIsrTable[UscVector])(info);
		else if ( (DmaVector&(BIT10|BIT9)) == BIT10)
			mgsl_isr_transmit_dma(info);
		else
			mgsl_isr_receive_dma(info);

		if ( info->isr_overflow ) {
			printk(KERN_ERR "%s(%d):%s isr overflow irq=%d\n",
				__FILE__, __LINE__, info->device_name, info->irq_level);
			usc_DisableMasterIrqBit(info);
			usc_DisableDmaInterrupts(info,DICR_MASTER);
			break;
		}
	}
	
	/* Request bottom half processing if there's something 
	 * for it to do and the bh is not already running
	 */

	if ( info->pending_bh && !info->bh_running && !info->bh_requested ) {
		if ( debug_level >= DEBUG_LEVEL_ISR )	
			printk("%s(%d):%s queueing bh task.\n",
				__FILE__,__LINE__,info->device_name);
		schedule_work(&info->task);
		info->bh_requested = true;
	}

	spin_unlock(&info->irq_spinlock);
	
	if ( debug_level >= DEBUG_LEVEL_ISR )	
		printk(KERN_DEBUG "%s(%d):mgsl_interrupt(%d)exit.\n",
			__FILE__, __LINE__, info->irq_level);

	return IRQ_HANDLED;
}	/* end of mgsl_interrupt() */

/* startup()
 * 
 * 	Initialize and start device.
 * 	
 * Arguments:		info	pointer to device instance data
 * Return Value:	0 if success, otherwise error code
 */
static int startup(struct mgsl_struct * info)
{
	int retval = 0;
	
	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk("%s(%d):mgsl_startup(%s)\n",__FILE__,__LINE__,info->device_name);
		
	if (info->port.flags & ASYNC_INITIALIZED)
		return 0;
	
	if (!info->xmit_buf) {
		/* allocate a page of memory for a transmit buffer */
		info->xmit_buf = (unsigned char *)get_zeroed_page(GFP_KERNEL);
		if (!info->xmit_buf) {
			printk(KERN_ERR"%s(%d):%s can't allocate transmit buffer\n",
				__FILE__,__LINE__,info->device_name);
			return -ENOMEM;
		}
	}

	info->pending_bh = 0;
	
	memset(&info->icount, 0, sizeof(info->icount));

	setup_timer(&info->tx_timer, mgsl_tx_timeout, (unsigned long)info);
	
	/* Allocate and claim adapter resources */
	retval = mgsl_claim_resources(info);
	
	/* perform existence check and diagnostics */
	if ( !retval )
		retval = mgsl_adapter_test(info);
		
	if ( retval ) {
  		if (capable(CAP_SYS_ADMIN) && info->port.tty)
			set_bit(TTY_IO_ERROR, &info->port.tty->flags);
		mgsl_release_resources(info);
  		return retval;
  	}

	/* program hardware for current parameters */
	mgsl_change_params(info);
	
	if (info->port.tty)
		clear_bit(TTY_IO_ERROR, &info->port.tty->flags);

	info->port.flags |= ASYNC_INITIALIZED;
	
	return 0;
	
}	/* end of startup() */

/* shutdown()
 *
 * Called by mgsl_close() and mgsl_hangup() to shutdown hardware
 *
 * Arguments:		info	pointer to device instance data
 * Return Value:	None
 */
static void shutdown(struct mgsl_struct * info)
{
	unsigned long flags;
	
	if (!(info->port.flags & ASYNC_INITIALIZED))
		return;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_shutdown(%s)\n",
			 __FILE__,__LINE__, info->device_name );

	/* clear status wait queue because status changes */
	/* can't happen after shutting down the hardware */
	wake_up_interruptible(&info->status_event_wait_q);
	wake_up_interruptible(&info->event_wait_q);

	del_timer_sync(&info->tx_timer);

	if (info->xmit_buf) {
		free_page((unsigned long) info->xmit_buf);
		info->xmit_buf = NULL;
	}

	spin_lock_irqsave(&info->irq_spinlock,flags);
	usc_DisableMasterIrqBit(info);
	usc_stop_receiver(info);
	usc_stop_transmitter(info);
	usc_DisableInterrupts(info,RECEIVE_DATA + RECEIVE_STATUS +
		TRANSMIT_DATA + TRANSMIT_STATUS + IO_PIN + MISC );
	usc_DisableDmaInterrupts(info,DICR_MASTER + DICR_TRANSMIT + DICR_RECEIVE);
	
	/* Disable DMAEN (Port 7, Bit 14) */
	/* This disconnects the DMA request signal from the ISA bus */
	/* on the ISA adapter. This has no effect for the PCI adapter */
	usc_OutReg(info, PCR, (u16)((usc_InReg(info, PCR) | BIT15) | BIT14));
	
	/* Disable INTEN (Port 6, Bit12) */
	/* This disconnects the IRQ request signal to the ISA bus */
	/* on the ISA adapter. This has no effect for the PCI adapter */
	usc_OutReg(info, PCR, (u16)((usc_InReg(info, PCR) | BIT13) | BIT12));
	
 	if (!info->port.tty || info->port.tty->termios->c_cflag & HUPCL) {
 		info->serial_signals &= ~(SerialSignal_DTR + SerialSignal_RTS);
		usc_set_serial_signals(info);
	}
	
	spin_unlock_irqrestore(&info->irq_spinlock,flags);

	mgsl_release_resources(info);	
	
	if (info->port.tty)
		set_bit(TTY_IO_ERROR, &info->port.tty->flags);

	info->port.flags &= ~ASYNC_INITIALIZED;
	
}	/* end of shutdown() */

static void mgsl_program_hw(struct mgsl_struct *info)
{
	unsigned long flags;

	spin_lock_irqsave(&info->irq_spinlock,flags);
	
	usc_stop_receiver(info);
	usc_stop_transmitter(info);
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;
	
	if (info->params.mode == MGSL_MODE_HDLC ||
	    info->params.mode == MGSL_MODE_RAW ||
	    info->netcount)
		usc_set_sync_mode(info);
	else
		usc_set_async_mode(info);
		
	usc_set_serial_signals(info);
	
	info->dcd_chkcount = 0;
	info->cts_chkcount = 0;
	info->ri_chkcount = 0;
	info->dsr_chkcount = 0;

	usc_EnableStatusIrqs(info,SICR_CTS+SICR_DSR+SICR_DCD+SICR_RI);		
	usc_EnableInterrupts(info, IO_PIN);
	usc_get_serial_signals(info);
		
	if (info->netcount || info->port.tty->termios->c_cflag & CREAD)
		usc_start_receiver(info);
		
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
}

/* Reconfigure adapter based on new parameters
 */
static void mgsl_change_params(struct mgsl_struct *info)
{
	unsigned cflag;
	int bits_per_char;

	if (!info->port.tty || !info->port.tty->termios)
		return;
		
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_change_params(%s)\n",
			 __FILE__,__LINE__, info->device_name );
			 
	cflag = info->port.tty->termios->c_cflag;

	/* if B0 rate (hangup) specified then negate DTR and RTS */
	/* otherwise assert DTR and RTS */
 	if (cflag & CBAUD)
		info->serial_signals |= SerialSignal_RTS + SerialSignal_DTR;
	else
		info->serial_signals &= ~(SerialSignal_RTS + SerialSignal_DTR);
	
	/* byte size and parity */
	
	switch (cflag & CSIZE) {
	      case CS5: info->params.data_bits = 5; break;
	      case CS6: info->params.data_bits = 6; break;
	      case CS7: info->params.data_bits = 7; break;
	      case CS8: info->params.data_bits = 8; break;
	      /* Never happens, but GCC is too dumb to figure it out */
	      default:  info->params.data_bits = 7; break;
	      }
	      
	if (cflag & CSTOPB)
		info->params.stop_bits = 2;
	else
		info->params.stop_bits = 1;

	info->params.parity = ASYNC_PARITY_NONE;
	if (cflag & PARENB) {
		if (cflag & PARODD)
			info->params.parity = ASYNC_PARITY_ODD;
		else
			info->params.parity = ASYNC_PARITY_EVEN;
#ifdef CMSPAR
		if (cflag & CMSPAR)
			info->params.parity = ASYNC_PARITY_SPACE;
#endif
	}

	/* calculate number of jiffies to transmit a full
	 * FIFO (32 bytes) at specified data rate
	 */
	bits_per_char = info->params.data_bits + 
			info->params.stop_bits + 1;

	/* if port data rate is set to 460800 or less then
	 * allow tty settings to override, otherwise keep the
	 * current data rate.
	 */
	if (info->params.data_rate <= 460800)
		info->params.data_rate = tty_get_baud_rate(info->port.tty);
	
	if ( info->params.data_rate ) {
		info->timeout = (32*HZ*bits_per_char) / 
				info->params.data_rate;
	}
	info->timeout += HZ/50;		/* Add .02 seconds of slop */

	if (cflag & CRTSCTS)
		info->port.flags |= ASYNC_CTS_FLOW;
	else
		info->port.flags &= ~ASYNC_CTS_FLOW;
		
	if (cflag & CLOCAL)
		info->port.flags &= ~ASYNC_CHECK_CD;
	else
		info->port.flags |= ASYNC_CHECK_CD;

	/* process tty input control flags */
	
	info->read_status_mask = RXSTATUS_OVERRUN;
	if (I_INPCK(info->port.tty))
		info->read_status_mask |= RXSTATUS_PARITY_ERROR | RXSTATUS_FRAMING_ERROR;
 	if (I_BRKINT(info->port.tty) || I_PARMRK(info->port.tty))
 		info->read_status_mask |= RXSTATUS_BREAK_RECEIVED;
	
	if (I_IGNPAR(info->port.tty))
		info->ignore_status_mask |= RXSTATUS_PARITY_ERROR | RXSTATUS_FRAMING_ERROR;
	if (I_IGNBRK(info->port.tty)) {
		info->ignore_status_mask |= RXSTATUS_BREAK_RECEIVED;
		/* If ignoring parity and break indicators, ignore 
		 * overruns too.  (For real raw support).
		 */
		if (I_IGNPAR(info->port.tty))
			info->ignore_status_mask |= RXSTATUS_OVERRUN;
	}

	mgsl_program_hw(info);

}	/* end of mgsl_change_params() */

/* mgsl_put_char()
 * 
 * 	Add a character to the transmit buffer.
 * 	
 * Arguments:		tty	pointer to tty information structure
 * 			ch	character to add to transmit buffer
 * 		
 * Return Value:	None
 */
static int mgsl_put_char(struct tty_struct *tty, unsigned char ch)
{
	struct mgsl_struct *info = tty->driver_data;
	unsigned long flags;
	int ret = 0;

	if (debug_level >= DEBUG_LEVEL_INFO) {
		printk(KERN_DEBUG "%s(%d):mgsl_put_char(%d) on %s\n",
			__FILE__, __LINE__, ch, info->device_name);
	}		
	
	if (mgsl_paranoia_check(info, tty->name, "mgsl_put_char"))
		return 0;

	if (!tty || !info->xmit_buf)
		return 0;

	spin_lock_irqsave(&info->irq_spinlock, flags);
	
	if ((info->params.mode == MGSL_MODE_ASYNC ) || !info->tx_active) {
		if (info->xmit_cnt < SERIAL_XMIT_SIZE - 1) {
			info->xmit_buf[info->xmit_head++] = ch;
			info->xmit_head &= SERIAL_XMIT_SIZE-1;
			info->xmit_cnt++;
			ret = 1;
		}
	}
	spin_unlock_irqrestore(&info->irq_spinlock, flags);
	return ret;
	
}	/* end of mgsl_put_char() */

/* mgsl_flush_chars()
 * 
 * 	Enable transmitter so remaining characters in the
 * 	transmit buffer are sent.
 * 	
 * Arguments:		tty	pointer to tty information structure
 * Return Value:	None
 */
static void mgsl_flush_chars(struct tty_struct *tty)
{
	struct mgsl_struct *info = tty->driver_data;
	unsigned long flags;
				
	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk( "%s(%d):mgsl_flush_chars() entry on %s xmit_cnt=%d\n",
			__FILE__,__LINE__,info->device_name,info->xmit_cnt);
	
	if (mgsl_paranoia_check(info, tty->name, "mgsl_flush_chars"))
		return;

	if (info->xmit_cnt <= 0 || tty->stopped || tty->hw_stopped ||
	    !info->xmit_buf)
		return;

	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk( "%s(%d):mgsl_flush_chars() entry on %s starting transmitter\n",
			__FILE__,__LINE__,info->device_name );

	spin_lock_irqsave(&info->irq_spinlock,flags);
	
	if (!info->tx_active) {
		if ( (info->params.mode == MGSL_MODE_HDLC ||
			info->params.mode == MGSL_MODE_RAW) && info->xmit_cnt ) {
			/* operating in synchronous (frame oriented) mode */
			/* copy data from circular xmit_buf to */
			/* transmit DMA buffer. */
			mgsl_load_tx_dma_buffer(info,
				 info->xmit_buf,info->xmit_cnt);
		}
	 	usc_start_transmitter(info);
	}
	
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	
}	/* end of mgsl_flush_chars() */

/* mgsl_write()
 * 
 * 	Send a block of data
 * 	
 * Arguments:
 * 
 * 	tty		pointer to tty information structure
 * 	buf		pointer to buffer containing send data
 * 	count		size of send data in bytes
 * 	
 * Return Value:	number of characters written
 */
static int mgsl_write(struct tty_struct * tty,
		    const unsigned char *buf, int count)
{
	int	c, ret = 0;
	struct mgsl_struct *info = tty->driver_data;
	unsigned long flags;
	
	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk( "%s(%d):mgsl_write(%s) count=%d\n",
			__FILE__,__LINE__,info->device_name,count);
	
	if (mgsl_paranoia_check(info, tty->name, "mgsl_write"))
		goto cleanup;

	if (!tty || !info->xmit_buf)
		goto cleanup;

	if ( info->params.mode == MGSL_MODE_HDLC ||
			info->params.mode == MGSL_MODE_RAW ) {
		/* operating in synchronous (frame oriented) mode */
		/* operating in synchronous (frame oriented) mode */
		if (info->tx_active) {

			if ( info->params.mode == MGSL_MODE_HDLC ) {
				ret = 0;
				goto cleanup;
			}
			/* transmitter is actively sending data -
			 * if we have multiple transmit dma and
			 * holding buffers, attempt to queue this
			 * frame for transmission at a later time.
			 */
			if (info->tx_holding_count >= info->num_tx_holding_buffers ) {
				/* no tx holding buffers available */
				ret = 0;
				goto cleanup;
			}

			/* queue transmit frame request */
			ret = count;
			save_tx_buffer_request(info,buf,count);

			/* if we have sufficient tx dma buffers,
			 * load the next buffered tx request
			 */
			spin_lock_irqsave(&info->irq_spinlock,flags);
			load_next_tx_holding_buffer(info);
			spin_unlock_irqrestore(&info->irq_spinlock,flags);
			goto cleanup;
		}
	
		/* if operating in HDLC LoopMode and the adapter  */
		/* has yet to be inserted into the loop, we can't */
		/* transmit					  */

		if ( (info->params.flags & HDLC_FLAG_HDLC_LOOPMODE) &&
			!usc_loopmode_active(info) )
		{
			ret = 0;
			goto cleanup;
		}

		if ( info->xmit_cnt ) {
			/* Send accumulated from send_char() calls */
			/* as frame and wait before accepting more data. */
			ret = 0;
			
			/* copy data from circular xmit_buf to */
			/* transmit DMA buffer. */
			mgsl_load_tx_dma_buffer(info,
				info->xmit_buf,info->xmit_cnt);
			if ( debug_level >= DEBUG_LEVEL_INFO )
				printk( "%s(%d):mgsl_write(%s) sync xmit_cnt flushing\n",
					__FILE__,__LINE__,info->device_name);
		} else {
			if ( debug_level >= DEBUG_LEVEL_INFO )
				printk( "%s(%d):mgsl_write(%s) sync transmit accepted\n",
					__FILE__,__LINE__,info->device_name);
			ret = count;
			info->xmit_cnt = count;
			mgsl_load_tx_dma_buffer(info,buf,count);
		}
	} else {
		while (1) {
			spin_lock_irqsave(&info->irq_spinlock,flags);
			c = min_t(int, count,
				min(SERIAL_XMIT_SIZE - info->xmit_cnt - 1,
				    SERIAL_XMIT_SIZE - info->xmit_head));
			if (c <= 0) {
				spin_unlock_irqrestore(&info->irq_spinlock,flags);
				break;
			}
			memcpy(info->xmit_buf + info->xmit_head, buf, c);
			info->xmit_head = ((info->xmit_head + c) &
					   (SERIAL_XMIT_SIZE-1));
			info->xmit_cnt += c;
			spin_unlock_irqrestore(&info->irq_spinlock,flags);
			buf += c;
			count -= c;
			ret += c;
		}
	}	
	
 	if (info->xmit_cnt && !tty->stopped && !tty->hw_stopped) {
		spin_lock_irqsave(&info->irq_spinlock,flags);
		if (!info->tx_active)
		 	usc_start_transmitter(info);
		spin_unlock_irqrestore(&info->irq_spinlock,flags);
 	}
cleanup:	
	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk( "%s(%d):mgsl_write(%s) returning=%d\n",
			__FILE__,__LINE__,info->device_name,ret);
			
	return ret;
	
}	/* end of mgsl_write() */

/* mgsl_write_room()
 *
 *	Return the count of free bytes in transmit buffer
 * 	
 * Arguments:		tty	pointer to tty info structure
 * Return Value:	None
 */
static int mgsl_write_room(struct tty_struct *tty)
{
	struct mgsl_struct *info = tty->driver_data;
	int	ret;
				
	if (mgsl_paranoia_check(info, tty->name, "mgsl_write_room"))
		return 0;
	ret = SERIAL_XMIT_SIZE - info->xmit_cnt - 1;
	if (ret < 0)
		ret = 0;
		
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_write_room(%s)=%d\n",
			 __FILE__,__LINE__, info->device_name,ret );
			 
	if ( info->params.mode == MGSL_MODE_HDLC ||
		info->params.mode == MGSL_MODE_RAW ) {
		/* operating in synchronous (frame oriented) mode */
		if ( info->tx_active )
			return 0;
		else
			return HDLC_MAX_FRAME_SIZE;
	}
	
	return ret;
	
}	/* end of mgsl_write_room() */

/* mgsl_chars_in_buffer()
 *
 *	Return the count of bytes in transmit buffer
 * 	
 * Arguments:		tty	pointer to tty info structure
 * Return Value:	None
 */
static int mgsl_chars_in_buffer(struct tty_struct *tty)
{
	struct mgsl_struct *info = tty->driver_data;
			 
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_chars_in_buffer(%s)\n",
			 __FILE__,__LINE__, info->device_name );
			 
	if (mgsl_paranoia_check(info, tty->name, "mgsl_chars_in_buffer"))
		return 0;
		
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_chars_in_buffer(%s)=%d\n",
			 __FILE__,__LINE__, info->device_name,info->xmit_cnt );
			 
	if ( info->params.mode == MGSL_MODE_HDLC ||
		info->params.mode == MGSL_MODE_RAW ) {
		/* operating in synchronous (frame oriented) mode */
		if ( info->tx_active )
			return info->max_frame_size;
		else
			return 0;
	}
			 
	return info->xmit_cnt;
}	/* end of mgsl_chars_in_buffer() */

/* mgsl_flush_buffer()
 *
 *	Discard all data in the send buffer
 * 	
 * Arguments:		tty	pointer to tty info structure
 * Return Value:	None
 */
static void mgsl_flush_buffer(struct tty_struct *tty)
{
	struct mgsl_struct *info = tty->driver_data;
	unsigned long flags;
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_flush_buffer(%s) entry\n",
			 __FILE__,__LINE__, info->device_name );
	
	if (mgsl_paranoia_check(info, tty->name, "mgsl_flush_buffer"))
		return;
		
	spin_lock_irqsave(&info->irq_spinlock,flags); 
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;
	del_timer(&info->tx_timer);	
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	
	tty_wakeup(tty);
}

/* mgsl_send_xchar()
 *
 *	Send a high-priority XON/XOFF character
 * 	
 * Arguments:		tty	pointer to tty info structure
 *			ch	character to send
 * Return Value:	None
 */
static void mgsl_send_xchar(struct tty_struct *tty, char ch)
{
	struct mgsl_struct *info = tty->driver_data;
	unsigned long flags;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_send_xchar(%s,%d)\n",
			 __FILE__,__LINE__, info->device_name, ch );
			 
	if (mgsl_paranoia_check(info, tty->name, "mgsl_send_xchar"))
		return;

	info->x_char = ch;
	if (ch) {
		/* Make sure transmit interrupts are on */
		spin_lock_irqsave(&info->irq_spinlock,flags);
		if (!info->tx_enabled)
		 	usc_start_transmitter(info);
		spin_unlock_irqrestore(&info->irq_spinlock,flags);
	}
}	/* end of mgsl_send_xchar() */

/* mgsl_throttle()
 * 
 * 	Signal remote device to throttle send data (our receive data)
 * 	
 * Arguments:		tty	pointer to tty info structure
 * Return Value:	None
 */
static void mgsl_throttle(struct tty_struct * tty)
{
	struct mgsl_struct *info = tty->driver_data;
	unsigned long flags;
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_throttle(%s) entry\n",
			 __FILE__,__LINE__, info->device_name );

	if (mgsl_paranoia_check(info, tty->name, "mgsl_throttle"))
		return;
	
	if (I_IXOFF(tty))
		mgsl_send_xchar(tty, STOP_CHAR(tty));
 
 	if (tty->termios->c_cflag & CRTSCTS) {
		spin_lock_irqsave(&info->irq_spinlock,flags);
		info->serial_signals &= ~SerialSignal_RTS;
	 	usc_set_serial_signals(info);
		spin_unlock_irqrestore(&info->irq_spinlock,flags);
	}
}	/* end of mgsl_throttle() */

/* mgsl_unthrottle()
 * 
 * 	Signal remote device to stop throttling send data (our receive data)
 * 	
 * Arguments:		tty	pointer to tty info structure
 * Return Value:	None
 */
static void mgsl_unthrottle(struct tty_struct * tty)
{
	struct mgsl_struct *info = tty->driver_data;
	unsigned long flags;
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_unthrottle(%s) entry\n",
			 __FILE__,__LINE__, info->device_name );

	if (mgsl_paranoia_check(info, tty->name, "mgsl_unthrottle"))
		return;
	
	if (I_IXOFF(tty)) {
		if (info->x_char)
			info->x_char = 0;
		else
			mgsl_send_xchar(tty, START_CHAR(tty));
	}
	
 	if (tty->termios->c_cflag & CRTSCTS) {
		spin_lock_irqsave(&info->irq_spinlock,flags);
		info->serial_signals |= SerialSignal_RTS;
	 	usc_set_serial_signals(info);
		spin_unlock_irqrestore(&info->irq_spinlock,flags);
	}
	
}	/* end of mgsl_unthrottle() */

/* mgsl_get_stats()
 * 
 * 	get the current serial parameters information
 *
 * Arguments:	info		pointer to device instance data
 * 		user_icount	pointer to buffer to hold returned stats
 * 	
 * Return Value:	0 if success, otherwise error code
 */
static int mgsl_get_stats(struct mgsl_struct * info, struct mgsl_icount __user *user_icount)
{
	int err;
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_get_params(%s)\n",
			 __FILE__,__LINE__, info->device_name);
			
	if (!user_icount) {
		memset(&info->icount, 0, sizeof(info->icount));
	} else {
		COPY_TO_USER(err, user_icount, &info->icount, sizeof(struct mgsl_icount));
		if (err)
			return -EFAULT;
	}
	
	return 0;
	
}	/* end of mgsl_get_stats() */

/* mgsl_get_params()
 * 
 * 	get the current serial parameters information
 *
 * Arguments:	info		pointer to device instance data
 * 		user_params	pointer to buffer to hold returned params
 * 	
 * Return Value:	0 if success, otherwise error code
 */
static int mgsl_get_params(struct mgsl_struct * info, MGSL_PARAMS __user *user_params)
{
	int err;
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_get_params(%s)\n",
			 __FILE__,__LINE__, info->device_name);
			
	COPY_TO_USER(err,user_params, &info->params, sizeof(MGSL_PARAMS));
	if (err) {
		if ( debug_level >= DEBUG_LEVEL_INFO )
			printk( "%s(%d):mgsl_get_params(%s) user buffer copy failed\n",
				__FILE__,__LINE__,info->device_name);
		return -EFAULT;
	}
	
	return 0;
	
}	/* end of mgsl_get_params() */

/* mgsl_set_params()
 * 
 * 	set the serial parameters
 * 	
 * Arguments:
 * 
 * 	info		pointer to device instance data
 * 	new_params	user buffer containing new serial params
 *
 * Return Value:	0 if success, otherwise error code
 */
static int mgsl_set_params(struct mgsl_struct * info, MGSL_PARAMS __user *new_params)
{
 	unsigned long flags;
	MGSL_PARAMS tmp_params;
	int err;
 
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_set_params %s\n", __FILE__,__LINE__,
			info->device_name );
	COPY_FROM_USER(err,&tmp_params, new_params, sizeof(MGSL_PARAMS));
	if (err) {
		if ( debug_level >= DEBUG_LEVEL_INFO )
			printk( "%s(%d):mgsl_set_params(%s) user buffer copy failed\n",
				__FILE__,__LINE__,info->device_name);
		return -EFAULT;
	}
	
	spin_lock_irqsave(&info->irq_spinlock,flags);
	memcpy(&info->params,&tmp_params,sizeof(MGSL_PARAMS));
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	
 	mgsl_change_params(info);
	
	return 0;
	
}	/* end of mgsl_set_params() */

/* mgsl_get_txidle()
 * 
 * 	get the current transmit idle mode
 *
 * Arguments:	info		pointer to device instance data
 * 		idle_mode	pointer to buffer to hold returned idle mode
 * 	
 * Return Value:	0 if success, otherwise error code
 */
static int mgsl_get_txidle(struct mgsl_struct * info, int __user *idle_mode)
{
	int err;
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_get_txidle(%s)=%d\n",
			 __FILE__,__LINE__, info->device_name, info->idle_mode);
			
	COPY_TO_USER(err,idle_mode, &info->idle_mode, sizeof(int));
	if (err) {
		if ( debug_level >= DEBUG_LEVEL_INFO )
			printk( "%s(%d):mgsl_get_txidle(%s) user buffer copy failed\n",
				__FILE__,__LINE__,info->device_name);
		return -EFAULT;
	}
	
	return 0;
	
}	/* end of mgsl_get_txidle() */

/* mgsl_set_txidle()	service ioctl to set transmit idle mode
 * 	
 * Arguments:	 	info		pointer to device instance data
 * 			idle_mode	new idle mode
 *
 * Return Value:	0 if success, otherwise error code
 */
static int mgsl_set_txidle(struct mgsl_struct * info, int idle_mode)
{
 	unsigned long flags;
 
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_set_txidle(%s,%d)\n", __FILE__,__LINE__,
			info->device_name, idle_mode );
			
	spin_lock_irqsave(&info->irq_spinlock,flags);
	info->idle_mode = idle_mode;
	usc_set_txidle( info );
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	return 0;
	
}	/* end of mgsl_set_txidle() */

/* mgsl_txenable()
 * 
 * 	enable or disable the transmitter
 * 	
 * Arguments:
 * 
 * 	info		pointer to device instance data
 * 	enable		1 = enable, 0 = disable
 *
 * Return Value:	0 if success, otherwise error code
 */
static int mgsl_txenable(struct mgsl_struct * info, int enable)
{
 	unsigned long flags;
 
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_txenable(%s,%d)\n", __FILE__,__LINE__,
			info->device_name, enable);
			
	spin_lock_irqsave(&info->irq_spinlock,flags);
	if ( enable ) {
		if ( !info->tx_enabled ) {

			usc_start_transmitter(info);
			/*--------------------------------------------------
			 * if HDLC/SDLC Loop mode, attempt to insert the
			 * station in the 'loop' by setting CMR:13. Upon
			 * receipt of the next GoAhead (RxAbort) sequence,
			 * the OnLoop indicator (CCSR:7) should go active
			 * to indicate that we are on the loop
			 *--------------------------------------------------*/
			if ( info->params.flags & HDLC_FLAG_HDLC_LOOPMODE )
				usc_loopmode_insert_request( info );
		}
	} else {
		if ( info->tx_enabled )
			usc_stop_transmitter(info);
	}
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	return 0;
	
}	/* end of mgsl_txenable() */

/* mgsl_txabort()	abort send HDLC frame
 * 	
 * Arguments:	 	info		pointer to device instance data
 * Return Value:	0 if success, otherwise error code
 */
static int mgsl_txabort(struct mgsl_struct * info)
{
 	unsigned long flags;
 
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_txabort(%s)\n", __FILE__,__LINE__,
			info->device_name);
			
	spin_lock_irqsave(&info->irq_spinlock,flags);
	if ( info->tx_active && info->params.mode == MGSL_MODE_HDLC )
	{
		if ( info->params.flags & HDLC_FLAG_HDLC_LOOPMODE )
			usc_loopmode_cancel_transmit( info );
		else
			usc_TCmd(info,TCmd_SendAbort);
	}
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	return 0;
	
}	/* end of mgsl_txabort() */

/* mgsl_rxenable() 	enable or disable the receiver
 * 	
 * Arguments:	 	info		pointer to device instance data
 * 			enable		1 = enable, 0 = disable
 * Return Value:	0 if success, otherwise error code
 */
static int mgsl_rxenable(struct mgsl_struct * info, int enable)
{
 	unsigned long flags;
 
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_rxenable(%s,%d)\n", __FILE__,__LINE__,
			info->device_name, enable);
			
	spin_lock_irqsave(&info->irq_spinlock,flags);
	if ( enable ) {
		if ( !info->rx_enabled )
			usc_start_receiver(info);
	} else {
		if ( info->rx_enabled )
			usc_stop_receiver(info);
	}
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	return 0;
	
}	/* end of mgsl_rxenable() */

/* mgsl_wait_event() 	wait for specified event to occur
 * 	
 * Arguments:	 	info	pointer to device instance data
 * 			mask	pointer to bitmask of events to wait for
 * Return Value:	0 	if successful and bit mask updated with
 *				of events triggerred,
 * 			otherwise error code
 */
static int mgsl_wait_event(struct mgsl_struct * info, int __user * mask_ptr)
{
 	unsigned long flags;
	int s;
	int rc=0;
	struct mgsl_icount cprev, cnow;
	int events;
	int mask;
	struct	_input_signal_events oldsigs, newsigs;
	DECLARE_WAITQUEUE(wait, current);

	COPY_FROM_USER(rc,&mask, mask_ptr, sizeof(int));
	if (rc) {
		return  -EFAULT;
	}
		 
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_wait_event(%s,%d)\n", __FILE__,__LINE__,
			info->device_name, mask);

	spin_lock_irqsave(&info->irq_spinlock,flags);

	/* return immediately if state matches requested events */
	usc_get_serial_signals(info);
	s = info->serial_signals;
	events = mask &
		( ((s & SerialSignal_DSR) ? MgslEvent_DsrActive:MgslEvent_DsrInactive) +
 		  ((s & SerialSignal_DCD) ? MgslEvent_DcdActive:MgslEvent_DcdInactive) +
		  ((s & SerialSignal_CTS) ? MgslEvent_CtsActive:MgslEvent_CtsInactive) +
		  ((s & SerialSignal_RI)  ? MgslEvent_RiActive :MgslEvent_RiInactive) );
	if (events) {
		spin_unlock_irqrestore(&info->irq_spinlock,flags);
		goto exit;
	}

	/* save current irq counts */
	cprev = info->icount;
	oldsigs = info->input_signal_events;
	
	/* enable hunt and idle irqs if needed */
	if (mask & (MgslEvent_ExitHuntMode + MgslEvent_IdleReceived)) {
		u16 oldreg = usc_InReg(info,RICR);
		u16 newreg = oldreg +
			 (mask & MgslEvent_ExitHuntMode ? RXSTATUS_EXITED_HUNT:0) +
			 (mask & MgslEvent_IdleReceived ? RXSTATUS_IDLE_RECEIVED:0);
		if (oldreg != newreg)
			usc_OutReg(info, RICR, newreg);
	}
	
	set_current_state(TASK_INTERRUPTIBLE);
	add_wait_queue(&info->event_wait_q, &wait);
	
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	

	for(;;) {
		schedule();
		if (signal_pending(current)) {
			rc = -ERESTARTSYS;
			break;
		}
			
		/* get current irq counts */
		spin_lock_irqsave(&info->irq_spinlock,flags);
		cnow = info->icount;
		newsigs = info->input_signal_events;
		set_current_state(TASK_INTERRUPTIBLE);
		spin_unlock_irqrestore(&info->irq_spinlock,flags);

		/* if no change, wait aborted for some reason */
		if (newsigs.dsr_up   == oldsigs.dsr_up   &&
		    newsigs.dsr_down == oldsigs.dsr_down &&
		    newsigs.dcd_up   == oldsigs.dcd_up   &&
		    newsigs.dcd_down == oldsigs.dcd_down &&
		    newsigs.cts_up   == oldsigs.cts_up   &&
		    newsigs.cts_down == oldsigs.cts_down &&
		    newsigs.ri_up    == oldsigs.ri_up    &&
		    newsigs.ri_down  == oldsigs.ri_down  &&
		    cnow.exithunt    == cprev.exithunt   &&
		    cnow.rxidle      == cprev.rxidle) {
			rc = -EIO;
			break;
		}

		events = mask &
			( (newsigs.dsr_up   != oldsigs.dsr_up   ? MgslEvent_DsrActive:0)   +
			(newsigs.dsr_down != oldsigs.dsr_down ? MgslEvent_DsrInactive:0) +
			(newsigs.dcd_up   != oldsigs.dcd_up   ? MgslEvent_DcdActive:0)   +
			(newsigs.dcd_down != oldsigs.dcd_down ? MgslEvent_DcdInactive:0) +
			(newsigs.cts_up   != oldsigs.cts_up   ? MgslEvent_CtsActive:0)   +
			(newsigs.cts_down != oldsigs.cts_down ? MgslEvent_CtsInactive:0) +
			(newsigs.ri_up    != oldsigs.ri_up    ? MgslEvent_RiActive:0)    +
			(newsigs.ri_down  != oldsigs.ri_down  ? MgslEvent_RiInactive:0)  +
			(cnow.exithunt    != cprev.exithunt   ? MgslEvent_ExitHuntMode:0) +
			  (cnow.rxidle      != cprev.rxidle     ? MgslEvent_IdleReceived:0) );
		if (events)
			break;
		
		cprev = cnow;
		oldsigs = newsigs;
	}
	
	remove_wait_queue(&info->event_wait_q, &wait);
	set_current_state(TASK_RUNNING);

	if (mask & (MgslEvent_ExitHuntMode + MgslEvent_IdleReceived)) {
		spin_lock_irqsave(&info->irq_spinlock,flags);
		if (!waitqueue_active(&info->event_wait_q)) {
			/* disable enable exit hunt mode/idle rcvd IRQs */
			usc_OutReg(info, RICR, usc_InReg(info,RICR) &
				~(RXSTATUS_EXITED_HUNT + RXSTATUS_IDLE_RECEIVED));
		}
		spin_unlock_irqrestore(&info->irq_spinlock,flags);
	}
exit:
	if ( rc == 0 )
		PUT_USER(rc, events, mask_ptr);
		
	return rc;
	
}	/* end of mgsl_wait_event() */

static int modem_input_wait(struct mgsl_struct *info,int arg)
{
 	unsigned long flags;
	int rc;
	struct mgsl_icount cprev, cnow;
	DECLARE_WAITQUEUE(wait, current);

	/* save current irq counts */
	spin_lock_irqsave(&info->irq_spinlock,flags);
	cprev = info->icount;
	add_wait_queue(&info->status_event_wait_q, &wait);
	set_current_state(TASK_INTERRUPTIBLE);
	spin_unlock_irqrestore(&info->irq_spinlock,flags);

	for(;;) {
		schedule();
		if (signal_pending(current)) {
			rc = -ERESTARTSYS;
			break;
		}

		/* get new irq counts */
		spin_lock_irqsave(&info->irq_spinlock,flags);
		cnow = info->icount;
		set_current_state(TASK_INTERRUPTIBLE);
		spin_unlock_irqrestore(&info->irq_spinlock,flags);

		/* if no change, wait aborted for some reason */
		if (cnow.rng == cprev.rng && cnow.dsr == cprev.dsr &&
		    cnow.dcd == cprev.dcd && cnow.cts == cprev.cts) {
			rc = -EIO;
			break;
		}

		/* check for change in caller specified modem input */
		if ((arg & TIOCM_RNG && cnow.rng != cprev.rng) ||
		    (arg & TIOCM_DSR && cnow.dsr != cprev.dsr) ||
		    (arg & TIOCM_CD  && cnow.dcd != cprev.dcd) ||
		    (arg & TIOCM_CTS && cnow.cts != cprev.cts)) {
			rc = 0;
			break;
		}

		cprev = cnow;
	}
	remove_wait_queue(&info->status_event_wait_q, &wait);
	set_current_state(TASK_RUNNING);
	return rc;
}

/* return the state of the serial control and status signals
 */
static int tiocmget(struct tty_struct *tty, struct file *file)
{
	struct mgsl_struct *info = tty->driver_data;
	unsigned int result;
 	unsigned long flags;

	spin_lock_irqsave(&info->irq_spinlock,flags);
 	usc_get_serial_signals(info);
	spin_unlock_irqrestore(&info->irq_spinlock,flags);

	result = ((info->serial_signals & SerialSignal_RTS) ? TIOCM_RTS:0) +
		((info->serial_signals & SerialSignal_DTR) ? TIOCM_DTR:0) +
		((info->serial_signals & SerialSignal_DCD) ? TIOCM_CAR:0) +
		((info->serial_signals & SerialSignal_RI)  ? TIOCM_RNG:0) +
		((info->serial_signals & SerialSignal_DSR) ? TIOCM_DSR:0) +
		((info->serial_signals & SerialSignal_CTS) ? TIOCM_CTS:0);

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s tiocmget() value=%08X\n",
			 __FILE__,__LINE__, info->device_name, result );
	return result;
}

/* set modem control signals (DTR/RTS)
 */
static int tiocmset(struct tty_struct *tty, struct file *file,
		    unsigned int set, unsigned int clear)
{
	struct mgsl_struct *info = tty->driver_data;
 	unsigned long flags;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s tiocmset(%x,%x)\n",
			__FILE__,__LINE__,info->device_name, set, clear);

	if (set & TIOCM_RTS)
		info->serial_signals |= SerialSignal_RTS;
	if (set & TIOCM_DTR)
		info->serial_signals |= SerialSignal_DTR;
	if (clear & TIOCM_RTS)
		info->serial_signals &= ~SerialSignal_RTS;
	if (clear & TIOCM_DTR)
		info->serial_signals &= ~SerialSignal_DTR;

	spin_lock_irqsave(&info->irq_spinlock,flags);
 	usc_set_serial_signals(info);
	spin_unlock_irqrestore(&info->irq_spinlock,flags);

	return 0;
}

/* mgsl_break()		Set or clear transmit break condition
 *
 * Arguments:		tty		pointer to tty instance data
 *			break_state	-1=set break condition, 0=clear
 * Return Value:	error code
 */
static int mgsl_break(struct tty_struct *tty, int break_state)
{
	struct mgsl_struct * info = tty->driver_data;
	unsigned long flags;
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_break(%s,%d)\n",
			 __FILE__,__LINE__, info->device_name, break_state);
			 
	if (mgsl_paranoia_check(info, tty->name, "mgsl_break"))
		return -EINVAL;

	spin_lock_irqsave(&info->irq_spinlock,flags);
 	if (break_state == -1)
		usc_OutReg(info,IOCR,(u16)(usc_InReg(info,IOCR) | BIT7));
	else 
		usc_OutReg(info,IOCR,(u16)(usc_InReg(info,IOCR) & ~BIT7));
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	return 0;
	
}	/* end of mgsl_break() */

/* mgsl_ioctl()	Service an IOCTL request
 * 	
 * Arguments:
 * 
 * 	tty	pointer to tty instance data
 * 	file	pointer to associated file object for device
 * 	cmd	IOCTL command code
 * 	arg	command argument/context
 * 	
 * Return Value:	0 if success, otherwise error code
 */
static int mgsl_ioctl(struct tty_struct *tty, struct file * file,
		    unsigned int cmd, unsigned long arg)
{
	struct mgsl_struct * info = tty->driver_data;
	int ret;
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_ioctl %s cmd=%08X\n", __FILE__,__LINE__,
			info->device_name, cmd );
	
	if (mgsl_paranoia_check(info, tty->name, "mgsl_ioctl"))
		return -ENODEV;

	if ((cmd != TIOCGSERIAL) && (cmd != TIOCSSERIAL) &&
	    (cmd != TIOCMIWAIT) && (cmd != TIOCGICOUNT)) {
		if (tty->flags & (1 << TTY_IO_ERROR))
		    return -EIO;
	}

	lock_kernel();
	ret = mgsl_ioctl_common(info, cmd, arg);
	unlock_kernel();
	return ret;
}

static int mgsl_ioctl_common(struct mgsl_struct *info, unsigned int cmd, unsigned long arg)
{
	int error;
	struct mgsl_icount cnow;	/* kernel counter temps */
	void __user *argp = (void __user *)arg;
	struct serial_icounter_struct __user *p_cuser;	/* user space */
	unsigned long flags;
	
	switch (cmd) {
		case MGSL_IOCGPARAMS:
			return mgsl_get_params(info, argp);
		case MGSL_IOCSPARAMS:
			return mgsl_set_params(info, argp);
		case MGSL_IOCGTXIDLE:
			return mgsl_get_txidle(info, argp);
		case MGSL_IOCSTXIDLE:
			return mgsl_set_txidle(info,(int)arg);
		case MGSL_IOCTXENABLE:
			return mgsl_txenable(info,(int)arg);
		case MGSL_IOCRXENABLE:
			return mgsl_rxenable(info,(int)arg);
		case MGSL_IOCTXABORT:
			return mgsl_txabort(info);
		case MGSL_IOCGSTATS:
			return mgsl_get_stats(info, argp);
		case MGSL_IOCWAITEVENT:
			return mgsl_wait_event(info, argp);
		case MGSL_IOCLOOPTXDONE:
			return mgsl_loopmode_send_done(info);
		/* Wait for modem input (DCD,RI,DSR,CTS) change
		 * as specified by mask in arg (TIOCM_RNG/DSR/CD/CTS)
		 */
		case TIOCMIWAIT:
			return modem_input_wait(info,(int)arg);

		/* 
		 * Get counter of input serial line interrupts (DCD,RI,DSR,CTS)
		 * Return: write counters to the user passed counter struct
		 * NB: both 1->0 and 0->1 transitions are counted except for
		 *     RI where only 0->1 is counted.
		 */
		case TIOCGICOUNT:
			spin_lock_irqsave(&info->irq_spinlock,flags);
			cnow = info->icount;
			spin_unlock_irqrestore(&info->irq_spinlock,flags);
			p_cuser = argp;
			PUT_USER(error,cnow.cts, &p_cuser->cts);
			if (error) return error;
			PUT_USER(error,cnow.dsr, &p_cuser->dsr);
			if (error) return error;
			PUT_USER(error,cnow.rng, &p_cuser->rng);
			if (error) return error;
			PUT_USER(error,cnow.dcd, &p_cuser->dcd);
			if (error) return error;
			PUT_USER(error,cnow.rx, &p_cuser->rx);
			if (error) return error;
			PUT_USER(error,cnow.tx, &p_cuser->tx);
			if (error) return error;
			PUT_USER(error,cnow.frame, &p_cuser->frame);
			if (error) return error;
			PUT_USER(error,cnow.overrun, &p_cuser->overrun);
			if (error) return error;
			PUT_USER(error,cnow.parity, &p_cuser->parity);
			if (error) return error;
			PUT_USER(error,cnow.brk, &p_cuser->brk);
			if (error) return error;
			PUT_USER(error,cnow.buf_overrun, &p_cuser->buf_overrun);
			if (error) return error;
			return 0;
		default:
			return -ENOIOCTLCMD;
	}
	return 0;
}

/* mgsl_set_termios()
 * 
 * 	Set new termios settings
 * 	
 * Arguments:
 * 
 * 	tty		pointer to tty structure
 * 	termios		pointer to buffer to hold returned old termios
 * 	
 * Return Value:		None
 */
static void mgsl_set_termios(struct tty_struct *tty, struct ktermios *old_termios)
{
	struct mgsl_struct *info = tty->driver_data;
	unsigned long flags;
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_set_termios %s\n", __FILE__,__LINE__,
			tty->driver->name );
	
	mgsl_change_params(info);

	/* Handle transition to B0 status */
	if (old_termios->c_cflag & CBAUD &&
	    !(tty->termios->c_cflag & CBAUD)) {
		info->serial_signals &= ~(SerialSignal_RTS + SerialSignal_DTR);
		spin_lock_irqsave(&info->irq_spinlock,flags);
	 	usc_set_serial_signals(info);
		spin_unlock_irqrestore(&info->irq_spinlock,flags);
	}
	
	/* Handle transition away from B0 status */
	if (!(old_termios->c_cflag & CBAUD) &&
	    tty->termios->c_cflag & CBAUD) {
		info->serial_signals |= SerialSignal_DTR;
 		if (!(tty->termios->c_cflag & CRTSCTS) || 
 		    !test_bit(TTY_THROTTLED, &tty->flags)) {
			info->serial_signals |= SerialSignal_RTS;
 		}
		spin_lock_irqsave(&info->irq_spinlock,flags);
	 	usc_set_serial_signals(info);
		spin_unlock_irqrestore(&info->irq_spinlock,flags);
	}
	
	/* Handle turning off CRTSCTS */
	if (old_termios->c_cflag & CRTSCTS &&
	    !(tty->termios->c_cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
		mgsl_start(tty);
	}

}	/* end of mgsl_set_termios() */

/* mgsl_close()
 * 
 * 	Called when port is closed. Wait for remaining data to be
 * 	sent. Disable port and free resources.
 * 	
 * Arguments:
 * 
 * 	tty	pointer to open tty structure
 * 	filp	pointer to open file object
 * 	
 * Return Value:	None
 */
static void mgsl_close(struct tty_struct *tty, struct file * filp)
{
	struct mgsl_struct * info = tty->driver_data;

	if (mgsl_paranoia_check(info, tty->name, "mgsl_close"))
		return;
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_close(%s) entry, count=%d\n",
			 __FILE__,__LINE__, info->device_name, info->port.count);

	if (tty_port_close_start(&info->port, tty, filp) == 0)			 
		goto cleanup;
			
 	if (info->port.flags & ASYNC_INITIALIZED)
 		mgsl_wait_until_sent(tty, info->timeout);
	mgsl_flush_buffer(tty);
	tty_ldisc_flush(tty);
	shutdown(info);

	tty_port_close_end(&info->port, tty);	
	info->port.tty = NULL;
cleanup:			
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_close(%s) exit, count=%d\n", __FILE__,__LINE__,
			tty->driver->name, info->port.count);
			
}	/* end of mgsl_close() */

/* mgsl_wait_until_sent()
 *
 *	Wait until the transmitter is empty.
 *
 * Arguments:
 *
 *	tty		pointer to tty info structure
 *	timeout		time to wait for send completion
 *
 * Return Value:	None
 */
static void mgsl_wait_until_sent(struct tty_struct *tty, int timeout)
{
	struct mgsl_struct * info = tty->driver_data;
	unsigned long orig_jiffies, char_time;

	if (!info )
		return;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_wait_until_sent(%s) entry\n",
			 __FILE__,__LINE__, info->device_name );
      
	if (mgsl_paranoia_check(info, tty->name, "mgsl_wait_until_sent"))
		return;

	if (!(info->port.flags & ASYNC_INITIALIZED))
		goto exit;
	 
	orig_jiffies = jiffies;
      
	/* Set check interval to 1/5 of estimated time to
	 * send a character, and make it at least 1. The check
	 * interval should also be less than the timeout.
	 * Note: use tight timings here to satisfy the NIST-PCTS.
	 */ 

	lock_kernel();
	if ( info->params.data_rate ) {
	       	char_time = info->timeout/(32 * 5);
		if (!char_time)
			char_time++;
	} else
		char_time = 1;
		
	if (timeout)
		char_time = min_t(unsigned long, char_time, timeout);
		
	if ( info->params.mode == MGSL_MODE_HDLC ||
		info->params.mode == MGSL_MODE_RAW ) {
		while (info->tx_active) {
			msleep_interruptible(jiffies_to_msecs(char_time));
			if (signal_pending(current))
				break;
			if (timeout && time_after(jiffies, orig_jiffies + timeout))
				break;
		}
	} else {
		while (!(usc_InReg(info,TCSR) & TXSTATUS_ALL_SENT) &&
			info->tx_enabled) {
			msleep_interruptible(jiffies_to_msecs(char_time));
			if (signal_pending(current))
				break;
			if (timeout && time_after(jiffies, orig_jiffies + timeout))
				break;
		}
	}
	unlock_kernel();
      
exit:
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_wait_until_sent(%s) exit\n",
			 __FILE__,__LINE__, info->device_name );
			 
}	/* end of mgsl_wait_until_sent() */

/* mgsl_hangup()
 *
 *	Called by tty_hangup() when a hangup is signaled.
 *	This is the same as to closing all open files for the port.
 *
 * Arguments:		tty	pointer to associated tty object
 * Return Value:	None
 */
static void mgsl_hangup(struct tty_struct *tty)
{
	struct mgsl_struct * info = tty->driver_data;
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_hangup(%s)\n",
			 __FILE__,__LINE__, info->device_name );
			 
	if (mgsl_paranoia_check(info, tty->name, "mgsl_hangup"))
		return;

	mgsl_flush_buffer(tty);
	shutdown(info);
	
	info->port.count = 0;	
	info->port.flags &= ~ASYNC_NORMAL_ACTIVE;
	info->port.tty = NULL;

	wake_up_interruptible(&info->port.open_wait);
	
}	/* end of mgsl_hangup() */

/*
 * carrier_raised()
 *
 *	Return true if carrier is raised
 */

static int carrier_raised(struct tty_port *port)
{
	unsigned long flags;
	struct mgsl_struct *info = container_of(port, struct mgsl_struct, port);
	
	spin_lock_irqsave(&info->irq_spinlock, flags);
 	usc_get_serial_signals(info);
	spin_unlock_irqrestore(&info->irq_spinlock, flags);
	return (info->serial_signals & SerialSignal_DCD) ? 1 : 0;
}

static void dtr_rts(struct tty_port *port, int on)
{
	struct mgsl_struct *info = container_of(port, struct mgsl_struct, port);
	unsigned long flags;

	spin_lock_irqsave(&info->irq_spinlock,flags);
	if (on)
		info->serial_signals |= SerialSignal_RTS + SerialSignal_DTR;
	else
		info->serial_signals &= ~(SerialSignal_RTS + SerialSignal_DTR);
 	usc_set_serial_signals(info);
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
}


/* block_til_ready()
 * 
 * 	Block the current process until the specified port
 * 	is ready to be opened.
 * 	
 * Arguments:
 * 
 * 	tty		pointer to tty info structure
 * 	filp		pointer to open file object
 * 	info		pointer to device instance data
 * 	
 * Return Value:	0 if success, otherwise error code
 */
static int block_til_ready(struct tty_struct *tty, struct file * filp,
			   struct mgsl_struct *info)
{
	DECLARE_WAITQUEUE(wait, current);
	int		retval;
	bool		do_clocal = false;
	bool		extra_count = false;
	unsigned long	flags;
	int		dcd;
	struct tty_port *port = &info->port;
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):block_til_ready on %s\n",
			 __FILE__,__LINE__, tty->driver->name );

	if (filp->f_flags & O_NONBLOCK || tty->flags & (1 << TTY_IO_ERROR)){
		/* nonblock mode is set or port is not enabled */
		port->flags |= ASYNC_NORMAL_ACTIVE;
		return 0;
	}

	if (tty->termios->c_cflag & CLOCAL)
		do_clocal = true;

	/* Wait for carrier detect and the line to become
	 * free (i.e., not in use by the callout).  While we are in
	 * this loop, port->count is dropped by one, so that
	 * mgsl_close() knows when to free things.  We restore it upon
	 * exit, either normal or abnormal.
	 */
	 
	retval = 0;
	add_wait_queue(&port->open_wait, &wait);
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):block_til_ready before block on %s count=%d\n",
			 __FILE__,__LINE__, tty->driver->name, port->count );

	spin_lock_irqsave(&info->irq_spinlock, flags);
	if (!tty_hung_up_p(filp)) {
		extra_count = true;
		port->count--;
	}
	spin_unlock_irqrestore(&info->irq_spinlock, flags);
	port->blocked_open++;
	
	while (1) {
		if (tty->termios->c_cflag & CBAUD)
			tty_port_raise_dtr_rts(port);
		
		set_current_state(TASK_INTERRUPTIBLE);
		
		if (tty_hung_up_p(filp) || !(port->flags & ASYNC_INITIALIZED)){
			retval = (port->flags & ASYNC_HUP_NOTIFY) ?
					-EAGAIN : -ERESTARTSYS;
			break;
		}
		
		dcd = tty_port_carrier_raised(&info->port);
		
 		if (!(port->flags & ASYNC_CLOSING) && (do_clocal || dcd))
 			break;
			
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
		
		if (debug_level >= DEBUG_LEVEL_INFO)
			printk("%s(%d):block_til_ready blocking on %s count=%d\n",
				 __FILE__,__LINE__, tty->driver->name, port->count );
				 
		schedule();
	}
	
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&port->open_wait, &wait);
	
	/* FIXME: Racy on hangup during close wait */
	if (extra_count)
		port->count++;
	port->blocked_open--;
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):block_til_ready after blocking on %s count=%d\n",
			 __FILE__,__LINE__, tty->driver->name, port->count );
			 
	if (!retval)
		port->flags |= ASYNC_NORMAL_ACTIVE;
		
	return retval;
	
}	/* end of block_til_ready() */

/* mgsl_open()
 *
 *	Called when a port is opened.  Init and enable port.
 *	Perform serial-specific initialization for the tty structure.
 *
 * Arguments:		tty	pointer to tty info structure
 *			filp	associated file pointer
 *
 * Return Value:	0 if success, otherwise error code
 */
static int mgsl_open(struct tty_struct *tty, struct file * filp)
{
	struct mgsl_struct	*info;
	int 			retval, line;
	unsigned long flags;

	/* verify range of specified line number */	
	line = tty->index;
	if ((line < 0) || (line >= mgsl_device_count)) {
		printk("%s(%d):mgsl_open with invalid line #%d.\n",
			__FILE__,__LINE__,line);
		return -ENODEV;
	}

	/* find the info structure for the specified line */
	info = mgsl_device_list;
	while(info && info->line != line)
		info = info->next_device;
	if (mgsl_paranoia_check(info, tty->name, "mgsl_open"))
		return -ENODEV;
	
	tty->driver_data = info;
	info->port.tty = tty;
		
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_open(%s), old ref count = %d\n",
			 __FILE__,__LINE__,tty->driver->name, info->port.count);

	/* If port is closing, signal caller to try again */
	if (tty_hung_up_p(filp) || info->port.flags & ASYNC_CLOSING){
		if (info->port.flags & ASYNC_CLOSING)
			interruptible_sleep_on(&info->port.close_wait);
		retval = ((info->port.flags & ASYNC_HUP_NOTIFY) ?
			-EAGAIN : -ERESTARTSYS);
		goto cleanup;
	}
	
	info->port.tty->low_latency = (info->port.flags & ASYNC_LOW_LATENCY) ? 1 : 0;

	spin_lock_irqsave(&info->netlock, flags);
	if (info->netcount) {
		retval = -EBUSY;
		spin_unlock_irqrestore(&info->netlock, flags);
		goto cleanup;
	}
	info->port.count++;
	spin_unlock_irqrestore(&info->netlock, flags);

	if (info->port.count == 1) {
		/* 1st open on this device, init hardware */
		retval = startup(info);
		if (retval < 0)
			goto cleanup;
	}

	retval = block_til_ready(tty, filp, info);
	if (retval) {
		if (debug_level >= DEBUG_LEVEL_INFO)
			printk("%s(%d):block_til_ready(%s) returned %d\n",
				 __FILE__,__LINE__, info->device_name, retval);
		goto cleanup;
	}

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_open(%s) success\n",
			 __FILE__,__LINE__, info->device_name);
	retval = 0;
	
cleanup:			
	if (retval) {
		if (tty->count == 1)
			info->port.tty = NULL; /* tty layer will release tty struct */
		if(info->port.count)
			info->port.count--;
	}
	
	return retval;
	
}	/* end of mgsl_open() */

/*
 * /proc fs routines....
 */

static inline void line_info(struct seq_file *m, struct mgsl_struct *info)
{
	char	stat_buf[30];
	unsigned long flags;

	if (info->bus_type == MGSL_BUS_TYPE_PCI) {
		seq_printf(m, "%s:PCI io:%04X irq:%d mem:%08X lcr:%08X",
			info->device_name, info->io_base, info->irq_level,
			info->phys_memory_base, info->phys_lcr_base);
	} else {
		seq_printf(m, "%s:(E)ISA io:%04X irq:%d dma:%d",
			info->device_name, info->io_base, 
			info->irq_level, info->dma_level);
	}

	/* output current serial signal states */
	spin_lock_irqsave(&info->irq_spinlock,flags);
 	usc_get_serial_signals(info);
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	
	stat_buf[0] = 0;
	stat_buf[1] = 0;
	if (info->serial_signals & SerialSignal_RTS)
		strcat(stat_buf, "|RTS");
	if (info->serial_signals & SerialSignal_CTS)
		strcat(stat_buf, "|CTS");
	if (info->serial_signals & SerialSignal_DTR)
		strcat(stat_buf, "|DTR");
	if (info->serial_signals & SerialSignal_DSR)
		strcat(stat_buf, "|DSR");
	if (info->serial_signals & SerialSignal_DCD)
		strcat(stat_buf, "|CD");
	if (info->serial_signals & SerialSignal_RI)
		strcat(stat_buf, "|RI");

	if (info->params.mode == MGSL_MODE_HDLC ||
	    info->params.mode == MGSL_MODE_RAW ) {
		seq_printf(m, " HDLC txok:%d rxok:%d",
			      info->icount.txok, info->icount.rxok);
		if (info->icount.txunder)
			seq_printf(m, " txunder:%d", info->icount.txunder);
		if (info->icount.txabort)
			seq_printf(m, " txabort:%d", info->icount.txabort);
		if (info->icount.rxshort)
			seq_printf(m, " ux/driv:%d", info/*
 * linux/drive;
		if (clink.c,v 4.38 longers/char/synclink.c
 *
p $
d: synclink.c,v 4.38 p $
 1/07 16:30:34 paulkf Exoverers/char/synclink.c
 *
otocd: synclink.c,v 4.38 otoco1/07 16:30:34 paulkf Excrcers/char/synclink.c
 *
crcd: synclink.c,v 4.38  * p;
	} else {
/char/synclink.c
 ASYNC tx:%d rn
 *",rs/c from clink.c,v 4.3txynclink.c,v 4.38 r Microgate Corporatioframeers/char/synclink.c
 fed: synclink.c,v 4.3
 * Or1/07 16:30:34 paulkf parityers/char/synclink.c
 p99
 *
 * This code ineral P1/07 16:30:34 paulkf brkers/char/synclink.c
 brkd: synclink.c,v 4.3n sy1/07 16:30:34 paulkf otocruners/char/synclink.c
 o99
 *
 * This code ihen operre t
	
	/* Append serial signy ontatus to s ex*/
char/synclink.c
 %s\n",plete_buf+1te() Calling mgsl_puttxactive=%d bh_reqat will unat wns eing_bh=%xar
 
	serial.tx_ame th,clink.ill nouested is called.unningchars or ml
 * mgsl_te() 
	spin_lock_irqsave(&clink.crq_letefram,flagste(){	
	u16 Tcsr = usc_InReg(seria, TCSR te()ffer dm bypassed Dma(too small tDMhold largesic bypassed (too small tIChold largeRshe line discipline
 * Ro hold largeRt
 * frame and may fragmenR frames) andRthe line discipline
 * Receive entryIhe line discipline
 * eceive entryDche line discipline
 * DCceive entryT
 * frame a(too small tframes) and nterface for syncppp.c Triver (an alCca bypinwo smal->io_base + CCAhold lmbling an HDLC fcsr=%04X tdm a netwoic a netwrND a netwrrk devic\n"
from se device). The fun"r(if dosynthe l netwdcif dosyntk device d by whiccca* is set char		 is ,st
 , the, poi, dri,modi,Icr,inte,TSOFT'' A. Eacte()
 *lete uis, tes. restoraccomplish
 * this, the TTY fl}

/* CalledDLC synclseriarmation about devicesme. leteic TIESmgsl_proc_show(struct har/file *m, void *v)
{
	POSE AR PARTPOSE AR*clin assembling an HDLC synclink dri* wrihar
 *DIRECT_versioite()
	clin =HALL TLITY A_list;
	whileo smal )arks line_clinnk.cclinalso pARY, Oclink.nextNSEQUENe()
 *return 0, THETNESS FOR A PARTICULAopenRPOSE ARinodeHOR ode * wSE AR DISCL DIS NO ES OR
 *singleTA, OR DIS,A PARTICULAR PU, NULLO, THETNESS FconstS INTERRUPTITA, r MERCsA PARTICULAfops =arks.owner		= THIS_MODULE,EGLIpenE ORF USE, DATA, O
 * readE ORhar/THE 
 * llseekUSE OF TARE,  OF THleaseSE OED AND* OF TH,
};HE IM PARTallocate_dmal stfers()
 * __)
	A/

#if  and * OF M DMA ed(__i3 (ISA adaptocol  deorINT() asshared memory   int $3"PCI#else
#  .__)
#  dArguments:, PROC	pointerDLC LITY A instance datadefiR OR
 *Value:	0 if success, otherwise error
D FITNESS FOR A PART*/

#if defined(__i38VENT SHALL THE AUTHOR BE NO Eunne ced /driv Bd(__i3PerF * O;
MPLARY->last_meme <lin =* SE contCalculBREAthe number ofsm("   int $3neodularyDLC holdlude e. C/* largest */

wable 
 * O size. Note: Iflude max#include <l iND FIude not an even multiple/ttyude m("   int linux/then we neD WARRing.h>
roun.h>
#ie <linu * li per#includup one. <li
	h>
#include <li = (e <linux/timer):30:34 max_
 * O_e <l/DMABUFFERSIZEte() 16:EMENT Oclude <linux/s %ncluock.h>
#in ers/h>
#include <li++nux/i <linux/debus_type == MGSL_BUS_TYPE_e MALUDING/*
		 * The e MAX_ISA_D has 256KBytes/ttyNT() { }
#endito use.ctl.h>
#is/str64 PAGE_deviced(__i3
#inclctl.h>
#infirst pagx/struseOINT( pad* mg at te <atiludeoracectl.h>e <linuIAL  does
#incbegintopsoffset 0l.h>
#inPCIctl.h>else
# 'snclude <asm/syirq.h>
#include 2nddma.h>
#include <l.h>
#includIAL . A 4Kes.h>
#ctl.h>ncludcanflip.h128nclu_ock.h>S INTERurestops32 b.h>
ctl.h>eachirq.h>
#includis leaves 62IG_Hma.h/irq.h>
#include F SU N 1
#el aremapping.h>
ransmit#inclu(s). Wsm/typereserve enoughDLC 1
#e bframHDLC lip.h>
#i.
 *iredctl.h><linux/tty
#define dma   int $3"num_txdefined(__i3)ivedDULE))
/ttyMaxe <liSludee <lirq.h>
#inclOh>
#inremaieceiNERIC_H(62-N), determine how many(CONm/typesC 0
#ento receive full -EFAULT : 0
#inboport
 * OK_MODU/O, PROC mgsled(__i_e <linREMENT OF copy_from_user(d *.h>
#include <linuc,size) r -EFAULT : 0

#in62 -rs or mgslEFAULT : 0

re trademarks /pci.h>
#include <linux/ttyh>
#include <asm/#incllude <l,src,ux/ior,destKPOI
#define m("   int $e <lin* unsigned long mode */
	0,			.h>
#include <linux/tty_f/
	HDLC_lip.h7#inclcluderor,dest,user(DERRUon
#inclx/ptra#define GET_Ue <liHDLC_
#inFLAG_UND#include <linis bumped byck_spsoh>
#aED.  anar encodiElockf Lned(CondiMERCHifseriddr_filter; */
_HDLC 0
#enwce.hgned chauOSSI  * Iedfined(,	/* unsigned shoc,size) ? -EFAULT : 0

#include <asm/uaccess.h>

#define RCLRVALUE 0xffff

static MGSL_PARAMS de(x/seq_file.h>
#i* MAXRXFRAMES) + 6xfffed cha| (definine total Txh>
#inc & RONE		/* utoC_HDLC ARITY_t,src,(alancludfine BRAL, 

#defned /07 16:6:30:34 ? -EFAULT : 0

#+EMENT Otic MGSL_PARAMS) >efauers/cf

static MGSL_PARAMS default_params = {
	MGSL_MODE_()
 alloc.hdebug_level >= DEBUG_LEVEL_INFO#defisynclk("%s(%d):_MEM_ADDng %d TXDERRU%d RXE_PATTERN_NONned.
 		__FILE__,__LIN	volt_params = {
	MGSL_MOD is cale DMABUFFERSIZE EXEMPL
#declude <lin= {
	MGSIAL h>
#ory * (INCLU< 0 ||t,sr	/* charactee <lind */
	usmall tus field */
	voIAL 32 link;	/* 32-bitRSIZE 4ved;	/

#dedding required by 16C32 */
	u32 link;s = {
	MGS flat link ts = {
	MGSL_MODntry */
	char *virt_addr;ICES mediif drxEFAULT 16C32 */
	untry *;	/* padding requirer entry */
	tma_addr_t dma_addr;
} DMALUDINGf data buffer *Can'/seria BREAclude <linud */
	ned.t */
	volatile u1also S OR
 *-ENOMEMe()
 * c PARTue,at_ry_from_user(d * (INCL;
  IN_SHUTDOWN_py_from_user(duct	_inputODS OR
 * SE
}DLC_s exofAGE.
 */

#if defined(__i386 <lin/*defi/* character count field */
	u6__)
#  dfine BREAK commonEIVE  1
#defe <luse as <asmvalueAG_UNDERRUN_ABORT15f defined(CsVICES 10
#es.h>
#includis a incloftruct txentrieseambre,size)har yPREAtainsX_TXa EVICES 10
#an actualtruct txKPOInce data struude <NK_Gigned char yX_TX(plus some.h>
#i* (INCANTABI.h>
#incluEVICES 10

#inigned char *	bue <lafined(DLC buil_PARINT()ing ir>
#i#includsoX_TXthateambleude entirefined(/synbe	/* raPECIed youpletrt backtops.hAX_TXtermiecei           is funcMERCHA BH_RECs }
#endie <ljusint         char *	bVICES
#in * Is (EVICES 10
#		magruct )icounfiIED Win with* xonphysicalnstanddres

#inint			mag/*
 * de <asde <linuxCONFnaviginclude IAL X_TX */
	Hbus masinuxDMA.	int	EVICES ar pa timee
 */
 
stru_HDLC tatus_X_TXTABI#incrr;		/* xonxt_device;	/* devic	*xmit_bdVICES 10
#define MAX_PCI_DEVICES 10
#define MAX_TOTAL_DEVICES 20

#include <linux/module.h>
#include <linux/errno.h>
#include <liner count field */
	u1VENT SHALL THE AUTHOR BE nclude <linux/TIES
vmalloc.h>
#include <linux/init.h>
#include <linux/ionclude <linuxusenclude <linux/hdt,src,size) buffer */
	#include d */
	ce
 * reinterrupt.h>
#includxffff

star count fiel
	wa#include requested;
	
	int dcd_chupt.h>
#include+= ock.h>LIST
#inE_HDLC,			/* unsi);
#else
# bh_runnystem	/* Protectionchar adruct tx_hold	/* spinlock ffineng buffer	int			_charbothe_length;t_q;
rooduluct  mgsr_list	tx_tamodulstrur_sirialne MAENTAL,  char encodiinspect porTY, ORevent_w	int			AGES
;
	int	 rx_buffecountetile_length; pdst;		byt timer_list	 */
	HBus Mimer */
	st,	/* unsignemultiple */
	iefinigger cofer;nt(, WH,many IRQs if a , complisr count fielsigneddr, GFP_KERNEHETH07 16:30:34 multiple */
	=Y, WHEcounTUS   4

#define  dcd_chkcount;		/* check c(u32b.h>
#inuffers;		/* number ote()
 g.h>
We goint   ;
	int			xmad;
	int			xmityx_holdi <linux/Initial/ptrace }
#endifframstrucll zero buffermemset.h>
#incluffer */
	u30uffer;

	int num	ri_do/* Save vir
 */
_head_t	 mgsl_struct	*ne_HOLDING_BUFing.h>
FERS 5
struct tx_holdi (RHOLDING1st)strucsct mgsl_strwilld */
	
	_TO_USERENTRY *s */
	u32 trucmodul HDLC tra buffer link;	/* 32-bit flaunt linux/netENTRY *)tal allocated Tx ludeof data buffer */
	ex;  	/* next tx holding buffer for adapter to load */
	int pu+REMENT Otic MGSL_PARAMSinux/p
src,Buip.h>
#i		xmitlist;	/* list of transmit ux/mait_atunt;	twot			timeout;
	ted Rx 	ico. (T#define buffate_rxb).unt;unt;	ux/majmber of txDLC 
	wait_q   /* ner(de *ountchbool THE RENTRY *rx_buffe0
#der(value,int			ma rx_r list of trantem.h>
#iceive e <l( ie <li i <equest */
	int tx_holdi i++2 pending_c.h>
#inclbuffS, IN rx_overflow;
	boer_couRC_16ist of tran,src,size) 	/* 32-bit fla[i].
	wa_f tran=efine MAXRkcount;		/* check+ (i *; */
of  	/* next tx h)ri_do instance name */

	unsigned int bus_type;	/gned chaait_q;
	strint			imeout;
	iofchar *	bu,	/* unsigneA,PCI) */
	unsigne * INREMENT Okcount;		/* checunctiloc.h>

	char device_name[25]; - 1#define MAXRXFRAMES 7
ize;	/* size +=ro b+ 1)based) */
	unsigned char 
	DMABU16 tcsr_value;

	char ds = {
	MGSL_MODE	/* device instance name */

	unsigned int bus_type;	/* expansion bus type (ISA,EISta buffer */
	igned char bus;	f the I/O address range +coun(:30:34 tic MGSL_PARAMS + irq_level;		/* interrupt lefunction;		/* PCI device number */

	unsigned int io_base;		/* base I/O address of adapter */
	unsigned int	bool dma_requestesize of the I/O address ranged */

16 mbre_bit;
	u16 loop_level;		/* interrupt leve/
	bool io_addr_re? -EFAULT : 0

#e if I/O addressignal states */

	boolned int irq_level;		/* interrupt level */
wn;
	int	dsr_up;
	int	dsr_down;
	;
	int	cts_down;
};

dcd_dow FreCEIVE  1
#de_t bufist;		s*/
#define MAX_TX_HOLDING_BUFFERS 5
struct tx_holding_bWarecei:__)
#  de
#in_DEV	unsigt			y) */
	u3ssoc */
dnt_wait_q;f defined(CdefinMUSTrs;	frnclubefunsi	bootile_lcr_base;
	u32 itself becausAX_TX	ed;

	u32 misc_cDevice ited;

 * OF MERCH <linux/tty_f	booar flag_bindividdevice;	/* !nux/errno.h>ED.   PART	boo mask */
	u32 RecordedEvents;		/* pending events */equired */
 	int tx_dm&&h>
#include <lin!x/init.h>
#include rs_uefinpmode current_rx_buffer;

	int num_t the I/O address nt;
	spinlock_t netbuffers */
	DEMPLARYr entries */
	un, WHapter to tx_holding_index;tdev;
#endif
load */
	int put_tdev;
sr_up;
	int	dsr_dopmode_insert_requested;
	dcd_down;
	int	cts_up;ed by 16C32 *6__)
#  define BREAnt sincludm("   int $3/* number ofed iifiol l defined(COdefinEize)clude <linuldingbeck_spe
#endima.h>
AUSE<linde <asm/ <linux/tdefinue;
	chss of thCONFfragine ) error _charit mayO adimpossiblUFFER_Deviiguous 1
#elseCES 10
#define MA*/

#de_PCI_DDEVICES 10
#define MAX_TOTAL_DEVICES>
#inclDLC_DEVICES 10
# of adapigned char *	bCCAR SDPIN	 * li	/* truunsigned char *	bui address 	u32 lcrICES 20

#include <linux/module.h>
#includ

#defintMask;			/* event trigger ed by 16C32 *VENT SHALL THE AUTHOR BE, 	/* next tx holSDPIN		/* ,TIESfine DATAREequest_frame	u32 rx_ober oinux/pcfine BREAma.h>ied e offsettx holdingdr_filter; */
	isc_ce;
	u16 tcsr_value;

	fine DATAREquested;		/*loc.h>
#include <linux/init.h>
#include <linux/ing_bh;

	bool bh_running;		/* Pro* unsigned sh	h>
#inclDLC_igne    ber o
	int isr_overflow;
	bool bh_requested;
	
	int 	MR	0x02	/k counts to prevent */
	intt cts_chkcount;		/* too mlinux/netdeviDLC
trademarks t dsr_chkcount;		/* is floating */
	int rrol Register */
#define TMDsigned int current_rx_buflinux/netdevi, &ol Register */number of tx dma frames rnel Col Register */
#define TM_buffers_ussed;
	unsigned int  TMCR	0x0e	/* of to
#define IOCR	0x16	/* mes r}.h>
#inclister */TMCR	0x0e	/*MR	0x02	/* st_mem_alloc;
	unsigned char* memory_baIT_SIZE
#defindcd_down;
	int	cpmodeIT_SIZE
#define SERIAL_ (PCI_lcr_base;only) */
	u32 phydefinsize)ansion bus tyof ize) errs used in #define DCAR 0		/* DMA command/address register */
#define CCAR SDPIN		/* channel command/address register */
#define DATAREG DCPIN + SDPIN	/* serial data register */
#define MSBONLY 0xNonUFFEtx_done;

	bool loopmodeister address (ordinal number)
 * usedector Regiing address/value ax_frs to the USC.
 */

#def6 rcc;	ister */
#s to be16 tcsr_val  */
#define CCR	0smit* device gister */
#definr */
#define Tfine TMnel Control Register t_signal_events;

	/#defingeneric HDLC device parts ctor Register *ol Register */
#define  Interrupt ContrChain Controlguration Register */
#definetdev;
#ol Reg}nterrerial xmit buffer is 1	0x1e	/* status Interol loopmodeefined(__i386__)
#  de (PCI only) */
	defin 10
#define MAX_PCI_DEVICES 10
#define MAX_TOTAL_DEVICES 20

#include ceive count Limit Register */
#	int	ri_up;	
dEvents;		/* pending events */ Control Register */
#d small  link;	/* 32-bit flat link to next buffer eS OR efine DACR	0x08	/* DMA Array count Rta buffer */
	u32 phys_entry;	/* phys	/* Burst/Dwellr count field */
	u16 resee serial xmit buffer is 1dcd_up;
	int	dcd_ddown;
	int	cts_up;er entry */
	dma_addr_t dma_a6__)
#  define BREAK offsets linu/O addres,addr)
clude <linux/sUSC regDLC) ||DMA These mARRAasg clnly)embED Wincluduct	*ne, BU discip, BU */

#define DCAR 0		/* DMA command/address register */
#define CCAR#define MSBONLY 0x41
#define LSBONLY 0x40

/*
 * These macros define the reger entry */
	dma_addr_t dma_aclude <linux/sched.h>
#includclink.cr entry */
	dma_addr = km	*xmi.h>
#include <linux/sf tx dma fra | tx dDMAclude <linux/de Register (high) */
#define T#defiTUS   4

#definemem_alloc;
	unsigned char* memory_ba Register (high) */
#	/* status Interrupt Control R CDIR	0x1a	/* Clear DMA Interrupt Regiine TDIAR	0x1e	/* Transmit DMA Interrupt Arm Register */
#define TBCR	0x2a	/* Traceive count Limit Register */
# Address Register (low) */
#define TARU	0x2e	/* Transmit Ak	boo:30:34 pr entry */
	dma_addrcludedress Register (high) */
#deff the serial xmit buffer is 1t Address Register (high) */

#define RDMR	0x8*DMAPBUFFERENTRY;

/* The queue of  def#  define BREAU	0xaey */
	unsigned char *(s)define SDIR	0x1c	/* Set DMA Interrupt local gister (s>
#includommaoaRUN_ABORT15d long inuct	*nexe.h>
#inc,sizregister

#defceive D;		/* xorx/strsufficif I/spacONS F#define DCAR 0		/ DMA command/address register */
#define CCAe TBCR	0x2a	/* Transmit Byte count Register */
#define TARL	0x2c	/* Transmit Address Regi/* The queue of efine TARU	0x2e	/* Transmit Addframe_size;	r;	/* 32-bit flat physical address of data buf uffer  spinlocktile u1txing; timeou(__i3ata couns RegisSEQUENTname, t */
	volatile u16clude <asm/uax2800
#m_user(de */
uffer ter to loa0x3800
#define ,0,ed) */
xFifo		0x4800
#define RTe;

	M16 tcsr=lue;<rRxAndTxDma		0x3800
#define ; ++i ChanneFifo		0x4800
#define RTigne) */
#deLR	0ine NTBCR	0x3a	/* Next Transmit Byte coumes required */ine RTCmd_LoadTcc			0x7000
#de_bufferine TM16 tc--iue;
>value;--ter */
#
#define RARine RTCmd_LoadTcc			0x7000
#* Trans0
#define RTCmd_LoadTcc			0x7000
#deine TCLR	0x3a	sed;
	unsigned int /* Tra
#define NTARU	0x3e	/* Next Transmit Address Register (low) */
#d#define RDMR	0x82	/* Receive DMA mister (low) */
#define EMSTATUS_CTS 0x10
#define MODEMSTATUS_RI  0x04
#define MODEMSTATUS_DCD 0x01


/	/* Receive Byte count Register */
#define R RTCmd_Null			0x0000
#define RTCmd_ResetHighestIus		0xrgeRxAndTxFifo		0x5800
#define RTCmd_LoadRcc		ster */#define RTCmd_SerialDataLSBFirst	0xa000
#defi RTCmd_SerialDataMSBFirst	0xa800
#define RTC physruct gnts {
0x3800
#indexe <linDmaCmd_puortTxChannel		0x5000
#define DmtTxChannel	: 0

#int	dsr_up;
	int	dsr_doommand Codes
 */

#define DmaCmd_Nuter (shared)
#de_F SUB	0x4800
#define R#define Nattemptar pa
#defint			magic;
	sS
 *xfine ux/sDTR 0x80			x_xc,size) erro		0x0000
#define DmaCmd_ResetTxChannel		0x1000
#define DmaCmd_ResetRxChannel		0x12trueENGTx9000
#define DmaCmd_Star
#dest linine Rtruce.h>
#inc		0xa000
#def,ine Dmafademah>
#inclue count LimiboolsetHighestDmaIus	0x8000
#deficlude <linux/sched.h>
#includ TCmdreput_ TCmd Register
#define DmaCmd_InitTxC pending_bool tx_acifh>
#hfer;O addres	0xa000
#defin,src,g buffbuffBUFFls		0x9000tclude <_SIZE 0x4 INTERR	0x4800
#define R *ptxdefinecomplisine RTCmd_LoadTcc			aCmd_AbortTxChannel		0x50]int txt_USE DmaC =Cmd_Disabs {
	int	ri_up;	ITED TO, PRTCmd_Dloopbac= ptx I/O addrclude/	/* Clock mode Conrcc;	0
#define TCmd_Se <linux/netdevice.h>	++EofEom		0xalizationEofEom		0xe<eDleInsertfine TMRruct xmit_c

#in0
#define TCmd_SCLR	0 PARTetHigpy_from_user(*/
	u30
#define TFifostatus	0Cmd_Sializa	--
#define DmaCmd_InitTxControl Rcc		aCmd_AbortTxChannel		0x500>include <asm/ua0x3800
#define Refine RTCmdAbortTxChannel		0x50=linuxHDLC_FLus_mas0xff,				incl */
	HD	mod_ne REccomplistxUS		BI, jiff*	bu+ msecs_to__DATA		(5000e;

	MG Reg = ne Dxb000
#define RTCmret, THE I*/

To ae RC tx dma.
 * 
 ine DmaCmd_ResetctTi
	unsi0xff,				/* unaCmd_Stare <l
	
	sp0xff,		sIAL,		0x0000
#define DmaCmd_ResetTxChannel		0x1000
#define DmaCmd_ReR SDPIN	mand/address #include vice tileransmit D
#deS_CODE_VIOL: 0
	inux/snNCLINKeceis in RinInterruEMSTATUS_DCD 0x01


/1ENGTHh>
#Receive ,ludeull			0x0000
#definelearPIN			BIT1
#define MISC (ordinal number)
 * usedRACT,
charddress/v, 

	u32 max_frefine RXST NO EVENT SH0x8000
#define TCmd_Se_SelectTicrTxFifostatus	0x5000
#
/*
 * Bits for enabling and di#defineS OR
 * S from s  /*fer;	0
#definin
#def Condefi_Sendtx_dma_b000
#define TCmd_EnableDlaCmd_AbortRxChannel	defi0
#define TCmd_Se=_FRAMING_ER* Buemcpyine RCmd_Null Interru InterruG_ERR */
_SelectRLevel		0x6000
#defi RCmd_SelectRaCmd_AbortRxChannel		
/*
 * Bits for enabling and disablControl/status Register (Tl RegiS OR
 *1SERVICES; LOSS OF USEclaimUTDOourceinclude <linux/sched.h>
#includrcc;.
 * 
 _region:30:34 pnce
 *erRxAndiober oTransmECT,
 * I")	0x9000
#definf data  buffer *I/O   /* nexconflict on#define %s Add a n8ened.
 unt */
	volatile u16gerTxDma		0x3000
#dMARK_SPACE		0H_STATUS   4

#dDEVe()
 *0500
#define I.
 * 
 * #define TRTransmie IDLEMOirq:30:34 prq 32-bi, PARTU	0xaruptatus fid/ste TTYIUSCrs
 */
#define	IUSC_SL1served;define IDLEMODE_MARK		Cant in Tran	/* NeCSR)MODE_MASK			0xIRQ=%d * IUSC revision identifiers
 */
#define	IUSC_SL1660/status S OR 	goLC frrouDE_HD1660			0xr THI * Transmit status Bits >
#include <linux/init.h>
#include <linux/efine IDLEMO>
#iDE_ALT_MARK_SMR	0x_overflow;
,0x40000ODE_SPACE			0x0600
#definee IDLEMODE_MARK		mem   /*fine IDLEM_MASK			0x0700

/*
 * IUSC  revision identifiers
 */
#define	IUSC_SL166fine TXSTATUS_UN* Tran TXSTATUS_EOF_l RegelectRNT() {ALL_SEN * Transmit statune TXSTATUS_ALL_SENT		BIT2
#define lcrlow;
	bool bh_rcr_>
#inc,128BIT1
#define TXSTATUS_FIFO_EMPTY		BIT0
#deflcefine TXSTATUS_ALL			0x00fa
#define usc_UnlatchTxstatusBits(a,b) usc_OutReg( (a), TCSR, (u16)((a4
#define MISCSTATUS_TXC_LAue + ((b) & 0x00FF)) )
				

#dTUS_ISCSTATUS_RXC_LATCHED	
				

#d_overflow;
	= ioaluep_nocachfine RARfine TXSTATUS_UNDs Regi			ERRUN		mes requi!S_DCD			BIT6
#defS_FIFO_EMPTY		BIT0
#deffine mapnclude <asm/sysODE_MASK			0xMemefine usc_UnlatchTxstatusBits(a,b) usc_OutReg( (a), TCSR, (u16)((a)->tcsr_valdefine((b) & 0x00FF)) )
		

#defin! PART_overfltMISCddr;
}S_FIFO_EMPTY		BIT0
#defFaiED Wclude <asm/systATUSefine MISCSTATUS_BRG1_ZERO		BIT1
#define MISCSTATUS_BRG0_ZERO		BIT0

#define usc_UnlatchIostatusBits(a,b) usc_OutCD_LATCHE#define MISCSTATUS_CTS_LATCHED		BI4
#definMISCSTATUSh>
#incluBIT4
#define MIS4
#defin_UNDERRUN		BIT3
#define MISCSTATLCRL_NO_SYNC		BIT2
#define MISCSTATUS_BRG1_ZERO		BIT1
#define MISCSTATUS_BRG0_ZERO		BIT0

#de4
#define	BIT8
#define MISCSTATUS_DCD_LATCHEe
 * r counts tUS_TXC_LADLC
	sDLC,			/* unsiERO		latinchanneng_bufc_OutRege IDLEMOdmaTxFifo	efinatus RgerTxDma		0x3000
ntry )NDERRUN		BIT3
#define MISCTXSTATUSefine SICR_DODE_MASK			0xDMAne TXSTATUS revision identifiers
 */
#define	IUSC_SL166DCD_INACTCR_DSR_N_SHUTDOF TH	0x0200
#define DICRelectBigEndian_PRE_S )
				

#definIT7
#define MISCSTATU dsr_chkcount;		/* iout timer */
	 */ACTIVOWN_efinmodfine RARDCD_INACTIODULMODE_CASCADXC_INAenIT5
efine SICR_DCD_INACTte()
 * crcc;	/* charactif defined(__i38BH actions to be perfor3
#define MISC	*xmit_bdif

/*
 * Th5
#define SICR_CTS_INACTIVE	BIT4
#define SICR_CTS			(BIT5+BIT4)
#define SICR_RCC_UNDE TXSTATUS_EOF_S	tus S OR
 * SEATUS_E:PIN_SHUTDIT3
#define SICRITED TO,C		BIT2
#define ansmit count LimitERO			0x0200
#dene Dma_done;

	bool loone usc_EnableInterfine IDLEMODE_ONE			0x0300
#defin
#define RTCmd_TriggerChannelLoadDma	0x2000
BIT0
#defi0xc0 + (b)) )

#define%s)q;
	st * IUSC revision identifiers
 */
#define	IUCR_DSR_*/
#define NTA
#define TXST TDR	0xByte it Command/status RMITED TO, PROCIT4
#define TXSTAT TCmd_S_SENT <linux/deRO			BIT1
#def#defindisrIrqBit( struct mgsl_struct 00) + 00) )

#define usc_ClearRG1_ZERO			BIT1
#defin_OutReg( (aInterrupt Control RegiITED TO,fine DmaCmd_InitRxChanndma_addr_t dma_addr;
ut_s _signal_hannel		0x2000
#define DmaCmd_StarITED TO,*/
#define NTA4553

/*
 * TransUN		BIT1IT3
#def_ALT_MARK_SPACE		0x0500
#define IDLEMsableMasterI4553

/*
 * Transmi_OutReg( (a), ICR, (u1efine MISCSTATUS_RXC_PREAMBLE_SENT	LL_SENT		BIT2
#define TXSTATUS_UNDERRUN		sc_OutReg( efine MISCSTATUS_RXC_LA_OutReg( (a), ICR, (u1TCHED		BIT7
#defin_CRC_SENT	BIT3
#define TXSTATUS_ALL_4
#define MISCSTATUS_TXC_LATCHEsc_OutReg( TCHED		BIT7
#define _OutReg( (a), Ine MISCSTATUS_RCC_efineounmapDmaReg( (a), DICR, int tx_buf		BIT6
#define0
#definested;
	
	st2
#define(u16)(usc_InDmaReg4
#definelt_paramATUS_DSR			BIT8ICR_RXC			(BIT15+ableDmaIntb ) \
	usc_OutReg( (a), ICR, (u16)((usc_InReg((a),ICR) & 0xff00) + 0x80 + (b)) )

#defixitsc_EnableMasterIrqBit(a) \
	usc_OutReg( (a), ICR, (r_up;
	int	dsr_done usc_EnableInterne TCCR	0x3c	/addNSEQUENrrupt Registdels		0efine the define MAX_TOTAL_DEVdefined(COit DMA S_CODglobalHDLC_PREAMBLEofILITY ANDbuffincreof I/_listefine  * linine TC1R	0x3e	/* Time Constant 1 Register */


/*
 * MACRO DEFINITIONS FOR DMA REGISTERS
 */

#definet status Bi	bool loopmode_send_done_requestENT OF SUBSTITUTSL_MAGIC 0x5401
ode R OR CONSEQUENT00
#defissynclingerTxDma		0x3000
#"ttySLDeriefine usc_e u16 rcc;efine usc_E</* u_TOTAL_DEVICd chefine TXmaxd lonnableDl, BU]define MAXRlay.h>
#include=et Db) \
	usc_OutReg(alizationtxdmabufEnableDltReg( 0x3000
#defi<asm/uaccess.h>

#d= static u16  usc_InDmaControl Rruct mgsl_struct *info, u< 1sabling IRQsgsl_struct *info, u161e SICRb)) )

stlip.c u16  usc_InDmaReg( struct mgsl_stre RXSTATUS_OVERR16 P( struct mgsl_structusc_OutDmaReg( struct e RXSTATUS_OVERR*info, u16 Port, u16 Vstatic u16  usc_In void	adema 16:30:34 gsl_struct *info, u16 P>)
#defX_HOLDINGLE) && Seg( struct mgsl_struct *info, u16 Pormgsl_struct *info, u16e SICR_ uscbleTransmitter(a,x/vmnableSta(a),MISEQUENTIAL #defi u16 Cmd );

#def if dm;
6 Valup buEVENT SHALL THE AUTHOcurrent Cmd, OR CONSEQUENTIAL DAMMAGES
 *e + (b)))
#_AUTO_DCD     sablie + (b)))
#defg((a), RCSR, (b))

#defi) usg((a), RCSR, (b))

#defin_OutReg((_EnableSta RMR, (u16)((usc_InRe< 4096ne us RMR, (u16)((usc_InReg(verrg((a), Te <linux/delay.h>
#include> 65535un_sync( struct mgsl_struct  struBIT2, BIT0)
 */
lude <linux/init.h>
#include <linux/eg((a),ICSyncL* INstatv%d %s: IOterminefine ine 

/*
,
/*
 -EFAULT : 0
=%u * IUSC ABLE_AhwSPECIAL,nt i)
#define#define	IUSC_SL1660			0x4BORT_SENT		BIT5
#_stop_transfine TXSTATUS_UND11+BIT10)
#define SIst,sransmi RMR, (u16)((usc_InReare trademarks tatic void usc_sta);
#mitter( struct mgslR_CTS_ );
static void usc_stop_transruct *info );
static void usc_set_txidle( st
#define SICR_RCtxfifo( struct mgsl_struct *info );


#if oratLINK_GENERIC_HDLC
	hdlcdev_inib) & 0x;
#
 * fdsr_up;
	int	dsr_dow status Bit,ICR) & 0xffRACT,
 * STRItty_ rx_IABILITY, OR TORT et( stING NEGLcarridefiaiead_= atic void usc_
 * dtr_rtc_In mgsl_sSUCH DDAMAGE.
 */

#if detus Bits in Transine BREAKPOIc vos */
	iaus Register (TCSRefined(COine TC1R	0x3e	/* Timeneive cS 20

#include EVICES 10
#ALL THE AUTHinux/module.h>
#includ, WHe count LimiVENT SHALL THE AUTtic void usc_set_sdlc_mED.  NO EVENT SHALL THE AUTHOR BE LIABPROCUREkze NTBCed) */
VENT SHALL THE AUTest,srtx dma frames r nterruine M to be performE <li/* Bfine BH_RECEdefine MAX_TOTAL_DEV\n"are trademarks c_reset( c voicomplisct *| (b)) )

#ct *.nfo );&_struct *info (b)) )

#dagide <init.MAGIC) usINIT_WORKT5
#definaskHEORY Obh_handl	0x4000
#defiruct mgsl_struct *info ) info );
stacloset_slaue i5*HZ/1#defatic int mgsl_io00
#waisc_O30*HZ mgsl_itsignequeue_headct mgsl_lete H_e <lgned l_q| (b)) igned long arg);

#if SYNENERIC_HDLC
#defilete framestruct mgsl_h
 * this, t)
static void hdlcdev_tx_donett mgsl_st RXSTATt mgsl_sarams,&default_struct ed) */
init.PARAMSe;

leMasterIdleuct m = *inf_TXIDLE_FLAGS;ine SICR_RX, u16 Value );
static void u16 Cmd );

static u16  usc_In#def_EnabS OR
 *utReg(sr_up;
	int	dsr_down;
	int	dtus Bit*info );

static void usc_reABILITY, OR TORTUDING NEGLIpenEnableTr OUT OF l_iocEnableTrl_ioc
 * writwdd, Nwadrdd, 
 * aCmdROR		G IN ANY20 + \

 * flush + \
c_Int Inte((WrDly)  a, Nrdd, _roomNrad ) \
(0x0+ \
(RdDlyy)  _infine TCmd, Nwad,wad)   << 15)  + \
((WrD< 15) + \
((NxNrdd)   << 1
 * ioctl+ \
((Nx)

st
 * throttlwdd, Nwadace_bloc
 * unace_block(struct *info,con
 * send_x+ \
((WrHoldnt, int xm
 * break_
static voiagnosa, NrHDLCunti/*
 *sc_Od ) \
c bool mgsl_re countt_r(valo << 28) +ruct *info  countop);
statictop_irq_t_mas( struct arsl_sthangust( strucbool ml_traiocmg3
#deft mgsl_struct mgincltruct *_LAT4000(INCLUDING t usc_l(INCLUDISUCH DAMnfo per
	
	ittynfo );
stainfo );ADDREnux/errno.h>
#includene dettytruct mgsllearrcSTATactly _t_dma_a=k );
v
 * ment ro(
#definode_cmanagement roess Register (low) *ssembnagement ro->IGENC OR OTHERWISE)s as static void mNTAL, S (a),=RECT,
 * I"es(struct mgsl_strfo);
sta16)(( mgsl_add_device(stmajorelettystrucct *info);
static sinor);
statati4es(struct mgsl_str <linu TTY_DRIVER#incluSERIAv;
#struct mgsl_strsubnupulat*/
sta#incluNORMtatic void mgsl_free

/*
 *info );
/
ststdned int sl_struct *info, unsigned int .c_ce TTdefinB9600 | CS8( stREAD | HUPCunt CLOCmgsl_struct *info, unsigned int .c_ispnclu= rametatic bool mgsl_get_raw_rx_frame(ostruct mgsl_struct *info );
ste TTYlation functioREAL_RAW;
_requructABILITY, O(struct mgsl_s,uct mgsopY fli), I(rde </
stBIT7mer tatic in_resources(strSICR_Dto be performed */

#ouldl_stc int nuxactly oDIRECT * IUSC revision identifi int aCmd_
static in_resources(str
stattruct mgsl_sdefine RTCmATA		BIT	int_Sele
 	0x2000
#defin, mgslstruc#e TXSTATUuct *info);DENTAL, SPECIAL,MISC*info);
static strucs( a, b ) \ SERVIup;
	umILITC 0
#rntrol statu);
#else
# ine tx_done;

	bool loolloc_isaapter
 struct mgsl_struct *info );

static vo/

#defi* contCheckemory_ba formatting
 */
LITY AND FI	ntinueTi=0 ;(io_a
#deISAusc_Enablesigno[i]sl_strq[i]*/
#dc_OutD \
	usc_OutReg( (a), ICR, (u16)((usc_InRe performo);
staticntrol statuiotermi,irot b,dmaTS_INACTIVE	truct,nfo, DM,ODEMuct HDLC
	sPLARY, OR CONe PCI adapter
 * D		BIT15 ine M/* Port stae <limd_TriggerRxs Register (TCSR) */
 ContrRY *BufferList,int Buffercount);ERRORtus Regeg((a),ICgsl_struct * info );
static void usdataNO_SYN2 of nne TRANSMp_bits;Copymgsl_sine iguLITY, #defin0
#define MAX_TOTAL_DEVype (ISA,EISance
 * nclude <linuint)tructint  hdlcde		BIT5
#dct *info);
statico, DMAmgsl_alloc_intermediac_incanonit_qizfine RARU/status  int  hdlcde SICR_RCCct *info);
statiList,iint tx_buffede <linu/init.h>
#incluISAfine TXSTATUS_ABOR_InReg(1

static indefine TC/*
 * Dffer2
#define ENABLE_DPLL_NO_S}HER IN CONTED.  CT,
 * I_cleanupdevice and resourcEVENT SHALL THE AUTHOR BE LIVENT SHALL THE AUTHOtmpSTATf data bUn
#detile s:_char
 *SourcePtr, unsigned short cing tRY *_resources(streReceivero );
statiunc int num_free_tx_dma_buffers(sic void mgsl_ine RTCfhMiscstem.fo);
stat mgsl_rSize)errne TXSTATUSe devict */
	volatile u16k are fo, const char *Buffer, unsigned ne DmaCm, OR CONSEQUENTIAL DAMAGES
 ancel_tr( struct mgsl_struct *info  );
statiine id usc_set_seriERFLOW		BIT3
#define SICRITED TO, tmst( utReg(( PROCUREMENT OF SUBSTITUTE Gl	0x320tmpuct *info );
pciic int nuedss ofciinfo);
static void & char *Buecei unsignedRVICES; LOSS O_;

/*t char *Budlcdeevice and resource RY *agnostoe fradtructsignal_Abort SUBptrloc_  		BREAKPOINTloc_b}
_memory(char* Taruct work_struct *work);
static void _dma_buffers(struct mguct *info );
sic void mgsl struct mgsl_struct *info );
SICR_D be performed: *info);
stic void stat( structe <line TXSTt */
	volk are ademr_receioid mgsl_ine MISCSTATmgsl_strucfo );

/*
 * d mgsl_isr_ TXSTATUS/* Chwn;
	int	dse <li:
	 char *Buffer, uns( a, b ) \l_louct *info,const_gsl_it char *Busl_isruct mgsl_r_misc,
	mgsl_isr_i}

modulte cit(ansmit_status);ta,
	mgssl_isgsl_isr_transing );
staasseRTCmd#define NIssu
staUSCING_BUFF/[MAX_TX_Hdma_ERRUNsmit InteC SICR_DCt file/0700typeRic void (gistEVICEnfo ux/m 0x10
#dfo( 
#inct file is encopbaci/* xonmo
	inignificine 5 bits <15..11>ile,
		event_wgistevclud. Bint mg0..7>(struct mgsl_mt_hebof te,addrIT7
#  (exce* info6..0>nt
	__userdd,teMR	0	/* tx 		0x0000
#define DmaCmdom seria  ce data strudefine MA* OF MERCH_async_mode(    Cmdstruct file mask ( DCPsymbolic macrosdefine N 20

#includeL_PARAMS  ceive count Limit Regtatic int Control Register (shared), DLC).mdnts *//ink *puruct file *figisteDLE_int mgsl_getd */
	
	r *user_gsl_stru stru,gsl_strt mgsl_get_par/* td/statoutw();
stbool bh_roopk;
	_sl_sSC_SL1660			0x4 registers c intR;

	SYNC((Wrrams(ransmgiste
	int_stop_receiver( struct mgsl_struct *info sl_isvice instance
 * registers R, (u16)(usc_tatic int tdcd_down;
	asseDmaint tiocmget  t(struct efinet file *file)oopmo tiocmset(struct tty_strucD *tty, struc info, MGSL_PARAMS  __user *user_params);
static int mgsl_set_params(struct mgslopmode_send_* info, r *mask)_Xo );__user *new_params);
static int mgsmgsl_get_txidle(struct mgsl_*mask); * info, int __user *idle_mode);
static intrdd, N* Global linkto * segsl_ruct * info, int enmbretati int mgsl_txabort mgsl_struct * info);
static i with thexenable(struct mgsl_struct * info, int enable);
static int mgsl_ent(struct mgsl_stru*mask);
fo, int __user Outnd may 
static intWstatia 16-bit_strucctTic tty_eMasc int nutruct * info, MGSL_PARAMS  __user er *user_params);
static l_set_params(struReg0700   mgsl_stru_head_t	ine b(stre <lrdd, ied optionnclud ed major number. 
static iced as module pparams);
static int mgsl_get_tt_txidle(struct mgsl_o zero to  * info, int __user *idle_mode)ions fo
static iS];
static intux/maj
#in withNK_Glist;		/ BIT0)er_list_e
 * _head_t	ing.h>
ux/majt
	__ idle_modletetl.h>BIT8t mg with thhe
 * .tions forsection address and breakpoint on mor, int, 0ncludand breakpoint on module load.
 * This is useful for use with gdb and add-symbol-file command.
 */
static int break_on_load;

/*
 * Driver mo zero to gule_ s
 */
stati and may 
static inttructned major numbefro	int forced as module parameter.
 */
static int ttymjor;

/*
 * Array of user specified options for SA adapters.
 */
static inVICESa Traomr *new_params);
static int mgs
#ind major numbe
static iCES];
static inFITNESS Ffferrame and may frame[MAX_TOTAL_DEVICES];
static int tOTAL_DEVICES];
static int txholdbufs[MAX_TOTAL_DEVICES];
	
module_param(break_on_load, bool, 0);
module_param(ttymajor, int, 0);
module_param_array(io, int, NULL, 0);
s a BUS dic int break_on_load;

/*
 * Driver mbufs, int,dcd_down;
faults to z to get auto
 * assigned major number. May beactly oe SICR_Dtic void odule parameter.
 */
static int ttymajor;

/*
 * Array of user specified options for ISA adapters.
 */
static inAX_ISA_DEVIISA_DEVICES];
static int irq[MAX_ISA_DEVICES];
static int dma[MAX_ISA_DEVICES];
static int debug_level;
static  maxframe[MAX_TOTAL_DEVICES];
static int txdmabufs[MAX_TOTAL_Dr, int, 0);
module_parable);
static int mgsl_txabort(struct m
module_param_array(irq, int, Nt(struct mgsl_struct * info);
static int mgsl_rxenable(struct mgsl_struct * info, int enable);
static int mgsl_wait_event(struct mgsl_struULE_DEVIfo, int __user nchron, NULL, 0);
mod* vi_param_array(txholdbDULE_LICENSE("GPL");

statiodule parameter.
 */
static int ttyma "SyncLink serial dextenster RC options for Iersion = "$Revision: 4.38 $";

static int synclink_init_one (struct pev *dev,
				     const struct pcivice_id *ent);
statvoid synclink_remove_one (struct pci_dev *dev);
WAKEUP_CHARS 256


static void mgsl_change_params(struct mgsl_E, 0x0210, PCI_ANY_ID, PCIwait_event(struct mgsl_struakpointe TCCR	0d liructsdlcuct mg
static intSet#inc timer_list	e <lSDLbe forde_suic vTY, Ostruct * info, MGSl_isr_r = "SyncLink serial driX_TOTAL_DEVICES 20

#include 	NONEt_txidle(struct mgsl_;
	static consedEvents;		/* pending events */
abufs[MAX_T;x4000
#PreSL166_strg_count;	md_SelectTicre( sItty_oock_t icount;	is_loa-fo)
		. If rx_rnot, takine vantt_usevent_wUnderWgnedfead Trat	dsore rx_rct mrn chips wraTDMRu linrun occue Daort.hgistifer_sl_memo		x_c status Bi
	spidingev_i lists *gramm, unata pd_Rernceiveol mgr_list_dma_a/synincluReceerrame__buf the lin. O>
#inclueceive_buf,sizRXSTrolldefiay sl_s_bufcycleine cviouslySICR_CTSPY_FaCmdnl lcrmio  - pass tileong ae Dmabuffealue;
	_module() fidle_TMCR,0x1fgsl__param_a=assed (too(ld->opDRgsl_!info)
		ry(sfs[MAX_TO==}

/*_PRE_fo)
		ent( e with gdb astruct.c int &uct mgct * *inf_LOOPoid  )
 	{
 sl_s);
ssl_s*static intMt(st tty_structMR * 	s:		tents:		ttmgsl_g4>l_get0ct pcx Suber tos, Ss exF_getonin linrunReturn Valu3ne
 * mgs
stat flag[MAX_TX_HD & 0x7dDmaRinfo lycture
 *  <12ruct mgsl_struc1 = Consecue th IdstruNT() g buffer0gned long f1..8ruct m11/
st[MAX_TX_oid r to ruct m/ mgslLoopgned long 7..one
 *  UN		  Rc void mgsl_ser o/ctrl field end_dinggned long 3 mgslsl_sto	
	ifate_rxbevel >= DEBUG_LEVEture
 * Return Va1p(%s
	
	iop(%spinlo= 0x8e06ture
 */ture
 , count);

	spin_;
  * Argum-s);
	
}	/* end of mgsl_stop() */

/* mgsl_start()ture
  * ignunsigsl_sopTY, ORe <ln linRun Achar iplin	release prea2	/*s	releases);
	
}	/* end of mgsl_stop() */

/* mgsl_start()	_irqr}
 ch_fun ip buffers(SICR_Dit(sttty info structq.h>
#incllue:	None
(%s)atic voidct ms,ty_structments:	 tty->drtruct mgf (mgsl_patop(sP tty in lcr_mem WrHS_EXIlag tty->drlags;
	
f (mgsl_paranoia_check(info, tty->name, "mgtty->dr
		returpinloc ( debug_levit(struct m>tx_entty->dtk("mgsl_op(%s)\n",inflong flce_name);	
		
	spin_lock_tty->dinfo->irqpinlock,flags);	
	spin_lock_irqsave(usc_sttter(info)tter(info);
	s0606dest,src,stDmaReg( sl_stopit(strmgsl_strid uRAWReg( store(&info->ir0001;t *ttinfoate_rxbuit(struing rcomplync	unsign(ld) {
		if 000/02/1OCR, */

/* morm. DCDplinRxd us Det intInsl_s Contr	lude <linux/slab.ed lips->receiven Val) & ~(BIT13|mgsl2)) |odulct *Register mgsl* TxSubr to:long fl	CMR>dri>		0	Dol_stnt,  CR**
 *Txty_struct c = 0;
	
	spi4>		x	 thefinPY_FR= 0;
	
	spi3_locktop(so tty in_start"))
			retuext h & BH_RECEIVE2 {
		info-8majorext s, 1=ve(&id ussnux/mTxLengthh & BHlong flag	int rc = 0;
	
	spi1-8)	0100	Monod us
		info->pend	ctio rc = xxxxpendin 04xxh & BHContrfs[MAX_TO|uncti4l_strad_pcademark
ue access functi6spinannel Control sl_stop()		throttle (stopUNDERRUN_ABORT1uct mg		info->pendio)
{/*
 6 Value );c) {
		/* Mark BH routine as complete */
ct *>bh_running = false;
	oid o->bh_requested = false;
	}
	
	spin_unlock_irqrestonfo-nfo->irq_spinlock,flag +se;
		info usc_DmaCc) {
		/* Mark>pending_!ruct mgPREAMBLE_PATTERN_infog of work items queued3ch table void mgsl_bh_han/*
 * Bottom half *inf &&
		l_start() */

()		throttle (stopSHARE_ZERO)g of nning = false;
	2_SelectTicrTxFisl_stopine Ifiloid !->irffg of/* unsiinclup to be loa(structsl_bh_uffers */alf action to perfRSRstatic vo "%s(%d):mgsl_bh_hoc_bunning = false;
/*
  uscalf action to perfCMR,ufs[MAX_TOTdefine Dmcmr_ numbe=1;
	}
#elsegsl_strfo);
	spin_u tty_strucR *inf;

	boomgsl_gtrucp(%s) nt set	 	usif ( dt * inta;
	un FCStruct * lnfo-CCITT (xd by x12ion) by nfo,dler(o->irq_s1VE:
			mty->;

		suffer;	1sfo, MGnull mgs/*infe BH_RE9ruct mgs__LINEbh_reInclutk( "r,destly)  <in

		 BH_REreturn;			mgsl_bh_reUse Abort/PE
 * l/* v
			E];	
cator BH_RE7..6>irq_spmgsl_E <lineral  BH_RE5		mgsl_bh_transneral Regiriver BH_RE4..lags;
tem act		break;COR		MIT) { = 8gsl_s BH_RECt_transmiID */
->driveBH )
			pbh_handletter(in01 exit\np(%suncti500alue;
	u access functi5l_st
	switchvoid mgsl_bh_hanion=%d\nER		BIcabor) traENCOt *inNRZB:e device). The l_struct *info =
	 agnos;l_struct *info)
{
	bool I_MARK*get_rx_franning = false;
		i*info) =
		(info->params.mode == SPACE:sl_strucnning = false;
		by ISR.t *info) =
		(info->params.modBIPHASE MGSL_MODE_HDirq_spinlock,flagstk( "%s(%d):mgsl_bh_receive(%s)\n",
( debuILE__,__LINE__,info->de )
		printk( "%s(%d):mgsl_bh_receive(%s)\n",
sicalderrun) {
			unsigned long flagame : mgsl_get_raw_rx_frame);
DIFF->irq_spinlock,fork items queued by ISR.
 )
		printk( "%s(% phys_addl_start() */

crce <linrottle CRC_MASK	0x06h_transmi16_witch g_level >= DEBUG_LE9o );
static v));
}

static void mgsl_bh_transmit(struct mgsl_stru32 *info)
{
	struct tty_s(G_LEVEinfo)
{e( strucon modame);
	
	info->bh_rs work item */
	tx_bufcount)
		break;
rruptLYNC_P "%s(%d):mgCLRclinkler(ffbuffer enWce.han))
		return;
eceivnl mgsl		BIT6
s04	/ognr */
>
#include if ( debug_lacist	trrupt(RCC)nt txh#defnt_wait_q; numbeile_lenart CLRstructRCCe_rede BIT2, _memorysize)_HOLDIN
	bote. pci_ding.h>
 numbeofif ( !inS, INd afoid mt)
{fo, un and loop 4096
#ending.h>
 buffuested;
ude <linux/8
#d->nampuk fod/statame);
	
	info->bh_rCL worCLRVALUrent_txtatic int irectly.md_Selectmodie SICR_RCC_UNstart echoingIPREAMBLE_Car *fl (tty)
		ttyICl_bh_handler() w8>	?	RxFIFObufs,Rmd_StarL2-biknown w_lockExictivHruptIA (mgsl_bh_stArme BH_RE6_lock(inft echoind IAUnknown lockBgnos/;
			 = 0;
	ininfo0	Rx Boport= 0;
	in) {
1	Qldisc_lete HDrefEBUGs oldATUSYNCLINK_spiinfo BH_REVE;
	};
			brea= 0;
	in1s()
 Rx Oen openterrupt 0 {
		inEBUG TC0_array(tor	    k;
	bh_handl			__FILE__ILE__op_trunctionaalue;
	u/pci.rSYNCma_adhe coun= 0;
	ty)
>dcd_chkcount sl_sting.h>
I/O abortheytLevel*/
	uantryumbed liArm			pd */ck_ir->device_named ppp.c driver
 * for s & 0xc
statxenable(struct mgsl_struct * info, int enableame);
	
	info->bh_rICR, (u16)(0x030a |ock,flags)*/
		h_func)ntk("%s(%d):mgsl_isr_receive_sta14s status=%04X\n",
er toUnlavoider;	RxService sl_stty)
fferr&& 
		info-IRQ P
 * mgid mgsl_bh_RECEIVRxlete H* inebug_leveXSTATUS_Addreefinsc_CinseIrq
 		usc 	{
		++info-ECEIVE_icounton module[MAX_TX_Hintk( "%s(%d):mTsl_bh_h	andler() work mod	ion=%d\n",
				__FILE	00	__,action);

		switch (action) {
		
		case BH_RECEI()
 h_recruct nfo-/* v
			break;
		case BH_TRANSMIT:
		mgslh_reTxbort EerIrqn);
			8utReg(infains exe(info)
	int	d0xff,				/* unknown work  		u[MAX_TX_H08X!\n"			p0;
	info->ctus & (RXSTATUS_E->driver);
			breaknfo->o, Rg_level >= DEBUG_LEVEL_BH )
		 		u(%d):mgs ( debug_lebh_handle mode.
o->p		__FILE__,__LI4E__,info->device_name);
bh &=atic void mgsl_bh_receive(struct mgsl_struct *info)
{
	bool (*get_rx_frame)(struct mgsl_struct *info) =
		(info->params.mode == MGSL_MODE_HDLC ? mgsl_get_rx_frame : mgsl_get_raw_rx_frame);

	if ( debuODE_HDLC ? mgsl_get_rx_fra )
		printk( "%s(%d):mgsl_bh_receive(%s)\n",
			__FILE__,__LINE__,info->device_name);
	
	do
	{
		if (info->rx_rcc_underrun) {
			unsigned long flags;
			spin_lock_irqsave(&info->irq_spinlock,flags);
			usc_start_receiver(info);
			spin_unlock_irqrestore(&info->irq_spinlock,flags);
			return;
		}
	} while(get_rx_frame(info));
}

static void mgsl_bh_transmit(struct mgsl_struct *info)
{
	struct tty_strucby ISR8t *tty = info->port.tty;
	unsigned long flags;
	
	if ( debug_level >= DEBUG_LEVEL_BH )
		printk( "%s(%d):mgsl_bh_gsl_bdefigsl_bh_status(structT",
			__FILE__,__LAGIC) {
txine
efine DICR		
	usctruct small to >= DEBUG theEL_BH )
		printk( [MAX_TX_Hmgsl_bh_status() entry on %srece
			__FILE__,__LINE_[MAX_TX_Hinfo->dev);

	info->ri_chkP *us;
	info->dsr_chkcount = 0;
	info->dcd_Sng HDL0;
	info->1atus
 the TxFifo witinfo1	EOF/EOMthe TxFifo wit) {
		nfo-he TxFifo witVE;
 receie discull W Triggled;
	ved abe <lirrupt is indq_spinlock,the state of th
 *	RACT,ine oBIT2

	bed for HDLC mode.
 *
 * 11,flags);
	
036alue;
	u	if ( debug_level >= DEBUG_LEVEL_ISR )	
		printk("%s(%d):mgsl_irece,ncti736n",
			__SCSTATUSTXSTATUS_EOF_SENT )
		info->i14unt.txpmode_active(Tnfo) )
 	{
		++infoT>icount.rxabort;
	 	info->loopmode_insert_requeTRANSMITfalse;
 
 		/* 
	*s( info, st tiocmseSete HDatus & (TXSCSTUS_*>icounmgsl_gtus & 				md	info->1> 	0/1	n line di	info->0..08us & RXS(inf	info-ri_cxy, daen_tail =k itemfo( stx->dcd
	
	if ( in work it_rts_;
			
	
	if ( i"mgsl_stet_ser/
		/* so pu	info-truct mgslrts_ Also sh	info-lags;
	
	i_serillRTS ) {
			LE__>drop_rts_Txn lin	info-o->irq_sl_RTS;
	Emp("Un*s; **) exit\node.
 *
 * Argunction0
	
	int get_AND g_level > );

	if (!, data, fln_sync( st SYNCLINK_G|= to hmpleteWAITe_tx_bTATUS_EOF_SENT )
		Cunning = t SYNCLINK_GUS_ABORTCdma_bit(status() entry on %sCMATUS_UNDERRUN | T(inf00G DCPIer 1 S200
# =IVED)) {
		if (13mit_cXSTA		return;0		}
		info->pending_bh |=1..CEIV	11	BRG1 0 if nis TxC PiED_HUNT9__LINs() */0
/* mgsl_isr_io_pin()
 	if (st1	DPLL
/* mgsl_i */

Omgsl_Unknown..3>	XXX	TxCLKruct	stronstProm __,in <2nfo->irted bR bits in the MISR
 * 1for HDLC mode.11l );
 
 * Retunctif77_,info->device_name);
f4 );

	if ( debug

	if (!info)
		return;
	RXC_ The g_level >= DEBUGction3;ck,fl bitse MIS The S) {;
static void usct mgsl_struct *info )
{
 	stBR&info-	info->pending_004u16 status = uscrvice_irqrnfo, MISR );

	if ( debug_level >= DEBUG_LEVETXCPIN * 		
		printk("%s(%d)6u16 status = uscTXC 0 if nothih_func)
		printk("%s(%d)7u16 status = uscR
 *1 info, RTCmd_Purg

	if (!info)
		return;
	T	struct	mgsl_icount *icount;18u16 sy bits= usc_InReg( info, MISR );

	if ( debug_level >= DEBUG_    L_ISR )	
		printk("%s(%d20CHED | MISCSTATUstatus=%04X\n",
			__FILE__,__LINE__,status);
			
	    RlearIrqPendingBits( info, 3TCHED );
	usc_UnlatchIostatusBits( info, status );

	i3		if (status & MIR
 *tus=%!= 0) {
	
		/* Processnfo-		__FILE__,__er toHardwty->C void mgsl_fr tty_strucHop_transmitter(info);
		CTR0 Divisor:00=32,01=16,10=8,11=4g_bh |= Fifo. TR1DSel:0=if (Divendif
	rets hkcount",
				_Fifo. VOK:0=rect * set, violgsl_free biphafuncransmit_stant.exThe tatus & MISCSTATUS_DSR_LATCHED) {
	* 
 * XX+;
			ct m MIS, actioTATUNRZDSR_B_DSR);ATCHlse
				{
		/* finisif (stat *user_ic;
	info->cts*/

gnal_e=struct ous,1=OSSIBI
{
	strted frameXatus &,
 			 the Txtatus &down++;
		}
		iis itatus0& MISCSTATUS_DCD_LATCHED) {
			if ((inf of th_Disaount)++ >=info )
{
	u16 se = fasr_io_pin( struct mgsl_struct (*info )
{
 	struct	+ATCHED |
	         xaaa0))
ne CXtalStrucs);
ne CDplltatus &+;
#ifer iCR_BRG0_c_InRe int  			(.info)tus &ARRANovide_naUS_DCD_L a rrren

#ddma_bt io_basechann;
		.c_InReit(stin HCint tdens eng H
 * line(struct nclueceive buenable(struct mgsl_struct * info, int enable	dcd_down+truc10592h &= ~h_func)
		{
			if ((i4745sl_st	if (!rc) {
		/* Mark BH routine as com;
		_DIV16aaa0))
#YNCLINK_GENtruct *inevel >= DEBUG_LEV &= ~BH_STATUST)
				usc_DisablestatusIrqs(info,SICR_CTS);8		icount->cts++;
			iftus atus & MISCSTATUS_ void uchkcount)+->cts++;
			if3EL_BHn(infTde <(dcd_/down+)/
	iner_on(infor.hwstruct *alue,adux/tty);
		wake_up_is gred/stathmodetructer_on(inface.hioportdev)p giERICaferen->peccludinclu
 		usc_. Instd( ier_on(infof    (status &line.
en _rx_D tousc_1h>
#it_hedirqsavtk("%s CK_CD) && 
		ck_spnsignisid mgoid mg vents);
	
}	/* end of mgsl_stop() */

/* mgsl_start()		resrc,sjz:fo->netde		els, applr (%s) /timul mgsgsl_i
				was(a),f_carrstructine MASTATtl_res floa,de <li
				wathrror f_carusc_etif_IREC  constSIZE];sl_sRxDtty);				waIoid mgsl_i) tranh_runa 0x holdingif ( debug_,tty)
		info, ceup(tty);
ASYN )
		 info-
	u16 c	
	boe bytty)
		/* ttty)
	ure
 * Return Value:	None
 */
static void mgsl_start(stainer_of(work, struf_car_structrqPenFIFO_ait_q)ive_s);
		C_CHE/YNCLINK_GEN)/status & MISCSTATUS_CTS) Control R !((vel >= DEBUG_LEVEL_ISR ) %(status & MISCSTATUS_CTS)rq_l2sabliRXSTATUSinfo->port.tty->hw_stopped sablinTc--;
			ructt tty_stf ( deb-1ding_				_event_ assigd majorTncluranousc_Dhanntus &ice_name);
	
	info->bh_TC1R,waitunsigngsl_bh_action(inf(strucunt) & MISCSTAunt)c void mgsl_bh_receive(struct mgsl__struct *info)
{
	bool  rc struct *info)
{
	bool (*w_stopped = 1;
					usc_s= MGSL_p_transmitter(info);
				}
sc_Unlanning = false;
8vice_name)%d):mgsl_bh_receive(%s)\n",
			__ics set IRQ flag */
	if ( stat= BH_STATUS;
	
	/* for9diagnostics set IRQ flag */
	if ( statnlock,ics set IRQ flag */
	ire(&info->irq_spinlock,flags);
			ru16 statuiagnosticsd usc_Talf action to perfHsignal_events.ri_up++;atic int tr *fl/lete HD tty_struct false_handler()->dcdf ( info-ted flowService (ROe BH_RECo->dcdansmit_daNot nfo )
 * 	Service a tran	if ((l_parinsertnsmit_da(Wce a tranIT)
nput_siion c(RWe BH_RECis input_si2 Misead_rt.ttoopm 	Service a tranablenput_si1*/
static voi mgsl_isr_transmit* 
 * r++;
			Reext bmaCmio );
KPOINallock edrror * Return ri_cX	LEVEL_INF Osl_s 	Service a tra	infmit_data xmitop(snt=%d\n",
			__FIh dataByine T	returnr
 * 	TxClkHOLDIN, TRAmgsl_isr_tbreak;
	nfo->LastTATUS_ofde_send_done/synBUG_LE
 * Return Vuments:	SHUTDOWN_LIMIT)Cmd_ResetTxChannel_FILE__,__LI02__,info-> 0) {
	
		/* Proces>port0x1020s.ri_up	}
}

/* mgsl_stop()		throttle (stopAUTO_CTSnal_evenlf action to perfSinfo
					usug_levd->ops->receivefo->*inffo->xmit_INACTIVE\n",
	_Enaer to				pri list omgsl_bh_st,
 			
 * l(MIE(%s)  WAKE,
 			 list IrqBi to sta		info->icinfo->loopmode_insert_requested = false;
 +ested = fDATAd */

	abort++;
	else
	+xabort++;
ode.
  MISCUS_ABORTarminfo	 the 
 * 
_PREAMBLE_if (ld) {
		if (ld->nfo->pbh |= BH_TRANSMIT;

}	/* end oBIT3c inteceive datmgsl_bh_sinfo ),the receivction address /*
 * Duct * 0and breakpoint on 	ret+BIT8nser list o;	/*)
 * 
 /* setnts:		inftic int cter cot of SRDOWNAlly)
{
	ss )u16 s, actiox & Txefine SICR_/
stata( struct mgsl_ststatus uct * for and breakpoint on 
	u16 incl;
	int work = 0;
	unsigned cested;
	
	strude <linux/init.h>
#incluISA pending_)
 * 
 DMAEN (R
 * 7,atic 14ned chri_chks & onnceivee( strucTXSTATUSne compuct	*ne);
#out ice_name);
	
	inisr_rePevice inst int mgsl_bh_actiPrn Value:15tructver(ic intMABUFFEtruct us() entry on %sDop_transmitter(info);1 senioX!\n"it(struA_bh_nggerRxTx/Rx RxD upt.Rxrt.ttpf this , RICR+ RXSTLY, (u16)(usc_In).
 * 
 * A1	)
 * 
 of this bPre*
 * ux/mDCRer(info), RICR	(WARNING*/

	wit_statt
	__use00ne MODEM <asm/se BH_ifo. ho Nwdame t 0);e SICR_DFO */

	wit_sta/

/* mgsl_IT)
		LiblockEndi Bitor Array/DLC_
			icount->dsr++B& Tx*tty = inCONFit);ine RMRrt.fa ) {( debug (star_down++;
		}
		if (sta7 bits- Minimum workRe-TXSTATUSmgsl_va	info->* mgsl Val_bh_s D/C	
		pS/D pe in_status()
 fo, Rdock_spignedl, 0);uffer;	_stru
	str);
	ie of staeceiverobe /UASEVELeve	u16egister_ERROR >icoun11nsmit			 BIT2, k("%only affceiveLS24UG_LEVEL_HDLC mspinlock,f Argumn Value600bd( info, RTCmd_PurgeTxFifo );
	}
 
	if ( status & pending_bh;

	bool be <linux/inclueMasMING_ERROR ice_name);
	nd may fragmenDnfo->ia00bS OR IMP		__FILE__,__LED) ) {
			printk("rx8rr=%04Xintk( "%s(%d):eMasintk( "%s(%d):mgDsl_bh_handler() wotatu1	atus & RX= c_stedHDLC_Ph>
#ins & R& ~BIT3 ));

RSBinA/L =ceive s& 
		info-Buct *in0) | r(RDR+Ltruct tinfo->io_b1	ts:		i DCPIN + DLC_PE tranend_dofetchd\n",
				unt->dsr++t(structit(struIlatchRxs 	usc_OutR1	T(valu 0);ERROR +inpuxr_recusc_InReg(in;	/*Wid>= DEion); ( stat7icoun?	brk++;
	hw_strdd, Nas 0isab) & ~BIata
 *	}
	
	if ILE__,__Lf2E__,info->;					
			/* update eAK_Rinfo
			tics */
		nfo = tty-tus & RXSTATUS_BREt frECEIVED ) {
				status &= ~(RXSTATUS_FRAMING_ERROR + RXSTATUS_PARITY_TCROR);
				elsefo, R readba			} ee MISatus & RXSTATUS_PARITY_ERROR) 
				icount->parity++;
			else if (status & RXSTATUS_FRAMING_ERROR)
				icount->frame++;
			else if (statu
	int	dED));
 	}

OVERRUN) {
				/* must issue purge fifo cmd before tructOBitsunsi/
				/* 6C32 acce	ts more receive chars */
				usc_RTCmd(info,RTCmd_PurgeRxt fr);
				icount->ove		}
mgsl_bh_status() entry on %sDTATUS_UNDERRUN |rame+tty_insert_fliount)++ >= Insmit		RXST(%d):mgsIEOtatus USSTATUS:
 * Argumen_irqsas & MISCvector du->devIntAd for S_PARITY_mit(info);
	nt=%d\nin Vand d BH_RECEtatus down++;
	, MRICR) >>EVEL_BH * OverrunRx	tty_insert_fli,
 			(usc_In of th(info, 

	if ( debug_level >= DEtatus 0,
			__FILE__ILE__,__L9 falssc_RTCmd(info,RTCmd_PurgeRxte, infoerru		info->ic been tested with a sli	u16 statusl
 * mg04	/* Chatty_iRQe
 */
statrame and may fragment frameount->frame,icount-N_ABORT15,	/*}
			
	if(work)
			
			/* update eCDIfo->ic303nfo->postatusIUS	
		p
 		usc_ info,ANSMIT_ointer to ;
	}

}	/* endl_isr_io_pin(ly reflects the status nfo)32se {
	inue;
				
			sse = B16C32 			 * repin case TxtatuSWr++;
			o->pending_bh |=IT)
		tructe, "mgsl_mgsl_isr_transmit_staeg(ie, "mgsl_MIT) {
	( debug_levele, "mgsl_P/

static vents.dsic void mgsl_it->brk++;
			} lag Sstruct *i + RXSTA case Rtus = usc_InReg( info, MISR );4icounttransmitter(info);op_tr* ArgumentILE__,__L808__,info->device_name);x DM_up_interruptible(&info->eo tty in_lel >= mgsl_struct *instruct *wLENGTH_16BITSlock,flags);
			re0vice_name);
	
	do
	{esetRxChannel);
32usc_UnlatchRxstatusBit1(info, RXSTATUS_ALL);
		usc_ClearIrq64ndingBits(info, RECEIVEby ISR.s(info, RXS usco,DISABLE_UNCONDITIONAL);
		usd(info, DmaCmd_ResetRxChaork)
{
	ct *iSTATUS;
	
	/* for by ISR.2(info, RXSTATUS_ALL);
		usc_Cork)
{
	ONES:_,__LINE__,info->( info, MISderrun = true;
	}

	usc_Cle10latchg( info, SICR,
			(unsignederrun = true;
	}

	usc_Cle01fo, status );

}	/* eusBits( info, MIS) != 0) {
	
		/* Processignal_events.ri_up++unt;		/rst/Dweltance data)
 * Ret			__FILE__,__LIN0x20	Maxus ofUSER(error,destive Dux/mataByte = inb( urge finfo		pointer to devicef_carr{
	struc(instance data)
isr_rece		/* as set by device config */

	u32 pending_ info-SYNC_PataB dispancYNC		clude <linuxBREAK_RECEIVED) ) {
			priBntk("rx++;
	04X\n",status);					
			/* update eerrupt.
2rx,icount->brt mg_  - pass renfo );
stnterrupts_HOLDINentified i(struct mgsl_stru;
	static const%s) in %s\n"terIrqBable);
sst char *__,info-16C32nter U	0xacompable);
sING_Eil;
	int	y bitsNSMIT_DLKor RICR	bool genation.");
				if_Disaty	por listTx	BH aableus( ave uct	*neRhut buffer lyn %s\n";

	if (!info) {
DEVICES 10
#define MAX_TOTAL_DEVICEu16 unt) EL_IS				priable);
sXSTA=ort.tty;setRxChannel		0x1200
#define DmaCmd_Staof List, all receive* info, int __user *idle_mleartermin00
#definthe buf	/* unsiblank)	Return nTXDt mgsl_sc_RCmd( info, RCmd_Seorm.
d->ops->receiveon(str|  mgs7+BIT6c intct *ttyt.tty->hw_stopped) {
			usc_stop_tro = tty->driver_d;
			return;
	VED)) {
		_bh |= BH_TRANSMIT;
	}

}	/truct *info )
{
it_status() */

/* mgsl_isr_io_pinn()
 * 
 * 	Service an Input/Output ppin interrupt. The type of
 * 	interrupt  is indicatc = y bits in the MIS_Disto idenguments:	c = o	       pointer  This als(&info->irqata
 * RUS_IDLEd mgsl64_SIZE 0FILE__,__LINE__,statut_signif ( dlevel >		}
			} else {
				if (!(status & MItus=%ngth; *		if ( debug_lNGTHvail.dsr_.h>
#includ__FI8nter diagnosticusc_RCmmgsl_start() */

->hw_stopped ne TMR	0xp_receiver( struct mgsl_struct *infsablind( info, RCmd_SelTC0tRicrRxFifinfo->ct
						printk("CTS tx star-1c int hkcount)+ount.buf_overrun++;
	}

}	/* endHUTDOWNl_isr_receive_dma() */

/* mgsl_istrademom half action rrun++;
	}

}	/* s=%04Xp++;	
			else
				info->input_signal_evennts:		io, RE,tatus = u ING_ERROue;
US_DCD)__,io, R0DLC fr		printk0.rier_onTXC_LATCHED );
		info->crRxFifostatus ); );
		infofo, Ratus *info)
0ATUS_ABr entrput/terrupus readback ,o clears=gume, Deg(inRxC		ifnt_wai=%04X\n",
is driver there iorm.
t occurs when a
 *h_action(structty);8vices(%d):e;

	MGSL_inclmgsl_compportlist have beenype (ISA,EISable);
static

	if3h &= ~ *info ce.
 )
{
	int Fifocouregisters aDLC,			/* unsi2  EOB of receive buffer entries.
 * 
 * Arguments:		info		pointer to deviuct mgstance data
 * RetArgumend
 *     	transmit mode, there may be enough transmit Duffers to hx0500
#defeast two or more egister (RDMR):
 * ist, all receive OL		End of List, alauxd, Nive buffers,
 			(ct FIAUXic void mgsl_ss[MAX_Tefine the fTATUSncfer. This interruptx10
#define MODEMSTATUS_RI  0x0ing of dynam	_DEV_tion, CDI tion atic void mgsl_stsr_nseREAMer is Aonfig 0 );

	/0ort.tty;ect FIct *info struct *	of a good frame or a frame with errors. The stransmit_d synclink_remove_one (struct32f(ttyR, BInts */
ts.dcd_down++;
_HDLC
			iRY *BuEBUG_LEVELeReceiver	}
		if (status & MISCSTATUS_CTS_LATCHED)
		{
			if ((info->cts_chkcount)++ >= IO_PIN_SHUTDOWN_LI_DATA )it_q);
		wake_up_interruptibl(&info->event_wait_q);

		if ( (info->port.flags & ASYNC_CHECK_CD) &&
		     (status & MISCSTATUS_DCD_LATCHED) ) {
			if ( debug_level >= EBUG_LEVEL_ISR )
				printk("%s CD now %s...", info->device_name,
			       (status & MISCSTATU
f ( debug_levl >= DEBUGansmit_dmoc_buffer_l	if (info->p %ransmit_dmd = 0; /sl_interruptsabli>pendis(%d):mgsl_isr_receive_dma(%s) status=%04X\n",
S_CTS)) {
					if ( 	}

g_level >ioctl.h>	
			else
				info->input_signal_eventsl_is the Transmit DMA Register (TDMR):
ring i*
 *     	BIT2  EOB      ebug_level >= DEBUG_LEVEL_ISrrupt occurs when a
 *     			transmit DMA buffer has been emptied.
 *
 *     	The driver maintains enough transmit DMA buffers to hold at leto perform.
	one max frame size transmit frame. When operatinfo );

static vo/

/+BIT0 );
=ransde <R
 *offEL_ISR )	
		printk(KERN_DEBUGrrupt occurassed (too small 	transmit DM has b Transmit count d mgsl_isr_transmit_dmate list */
};
MODs */
	u_hum forun_ext st char 	R MODEed char 	s */
	u>port_HOLDINGeg(infoumbeTDOWNstatuit Int voiverrun);
	eceive Daned isu/bito Puine Rxmit_dade_send UscVuffer;owR	0x04	/* Ched;
	struct mLEVEL_Ius_mask-zero on successful for Tx DMA IRQ */
	usc_OutDmaReg(info	return 1;
	}
	if me or a frame with errorsusc_InDmaReg(info, DIVR_AUTO_CTS          3
#define ENABG_ERRt( stdexl_free_t, i= BIT10)
			e <linu9)) == BIT10) TCmdinfo);ofister *foport(a,b) \
	u TCmdt, i_receive_dma(info);

		if ( inreine disendiine TCmd_Selount Register */multiple */
	int isrs requested */efine CMR	0x02	/* Chd list of e;
 	struct ttyP
	chRxn (instaort;
	 	 ( debug_level >=Eturn 0;
e, th
			usc_D TRANSMIT_ST
			b___FILRxFis occurstrue + (b)igned ch "Synctruct	*ne1	__uif (stavent_wait_qfo, int N 4		/yinfo->devi* unsigned lonbuffer
	t_dma(info);
		el				iending_bh &&mgsl_isr_e if dma e + (b))tic MGSL_,__LINE__archtruct to tfinish*	Ser->devddress rea_addr_tmean/
static _char_LEVEL_ISRinclude!infer_dat_GENistennt;	/* cou_waiing.h>
D
	 */incltoISCST) b	int  TATUS_no r(valuistatuine TCmdlock,f(nt=%d\nk(&infonon-->tasoid mgsAGES
 *!ol dma_requemgsl_isr_].5000
#d) entry\nCrrupt
		
	spir */
	uTDOWNinfo->taumbeive
 
	int ri_chkODEM STATUS Be + (b)le I/O.h>
	}
#endif
		!sl_isr_receive_dma(inurinFIFO_sl_isr_receive_dma(infofine TRA( info->pending_bh &&mgsl_isr_tras te->isr_overflow ) {
			print
static voibug_level >= DEBUG_LEVEnt=%d\nd of mgsl/

/t=%d\n_DEBUG "%s(%d):sl_i(%d)exit.\n",
				__FILE>
#iA monfo-ng 
	 * fo_LEVEL_IS Transmvel);

	UFFERENwine UNT	ENER	/* Receive Dlist;	( de	BIT6
#tactatic int stMoISR n	wait_queIN 4		/*nt retval = 0;
sl_isr_receive_dma(info);

		ife.
 * 	
 * Arguments:		fine TRANSt mgse dawraptrucait_queansion bus tyinHDLC_PREAMBLE_irq |= gsl_isr_x/vmof mif
		ning && !inrequest */
	int tx_hold {
		retning && !infN_LIMIT)
		 !info->bh_ruunning && !ilue:	0 if su
#in/xoff character */
	usg_levrqsave(& (nok(KERspin_loaded */
n 0;
	gnalt=%d\nin_lstructITS,	/* unsigned cha,
				__FstatletePARIcrewcharmgsl_iH_8BITS,	/* unsigne!CEIVE_ST_SHUTDOWN_LIMIT 100

struct	_input
 * 
 * 	Initialize andument>device_name);
		
	if (info->port.N_ERR "%s(%d):%srtup()
 *nfo, MISCSTATUS_(GFP_KERNEL_receive_dma(in&& !
 * 	
 * Arguments:)	
		printTinfo->bh DEBUG_LEVEL_ISR )	
			pector=%08X DmaVector\n",
			/* viresort. for itVector,Dmeg(infovel);

	art esl_in= DEBUG_LEVEL_INF *ttG_LEVEL_I_irqrestor 0;
	ndRN_ERR "%sR	0x04	/* Cha_strucr *flags,Receioid))
		retvan receigsl_struct * S_ADMIN) && infovel);

	 !info->bh_runt_dma(info);
		elser* So of mgsl*(lude <linup $
 *)&u16 mbre_bit;
	u1reque !info->bh_++VEL_ISR))rted* Clock mode Cd charannedit_he	0x500t io[Map aioporvel); char(GFP_KERNEL);
		if (fo->xmit_buf = (unsigned char;
  	}

	/* proN_LIMI}ount oP_KERNEL);
		i! (!info->xmit currnfo);
	
	/* Allocate an phys_addN_ERR "%s(%d):)	
		prode_active(info) )
 	{
	isr_r->icount.rxa

	se
	 	info->loopmode_inser_requested = fode.|sted = false;
turn Valupointer to device instanchutdown(struct mgsl_struct * inforn Valu,
 			H )
			pvoid sDISAt *wUNCONDITIONAeturn _DATA );
	
eset*	bu	0x04	/* Chainfo-y)
	ck_i"%s(%dnfo	e);
	struCSTATUS_CTS)) {
					if fo( inh |= BH_TRANSMIT;

}	n() *info)
{3has been emset_bit(ive
 *e);
	gned int bus_type;	/se_rclude <linubus type (ITMCR	0x0e	/* Test (info);
	
	if (t_dma(info);
		elned char busurn Valu		
			/* update eNRARL
	/* clTMCR	0x0e	turn Valu(&info->event_wait_q)U
	/* cleTMCR	0x0e	>> 16has beenode_active(info) )
 	{
		++info->icount.rxabort;;
	 	info->loopmode_insert_requested = fed to tsted = false;
 
 	TIALIZED))
	 void mgsl_t_requested = false;
 
 		/fo->1.coune; */
	Hessing (EOB)		if ( statusmgsl_bh_st mgs2
 		uDIigned chfo->2.learIrqPDisableInterrupts(infso,REC1/tty.* end_level >= DEB,RTCmd_PurgeRxFiIAR,lue:	by ISR2c(&info->tx_timer);

	if (i,icound the interrnd may 
	if (dlear the WLINE__level);
			usc_DisableMasttrieBit(info);
					info->tx_active = false;
		
	if (info->xe:	BsabliALIZED))
		return;

	if ENbug_lthe ISA leMast	For this dhis has no effect for the Pevel >= DEBUG_LEVn",status/* perfor		printk("%s(%d):mgsl_shutdown(%s)\n",
			 __FILE__,__LINE__, info->device_name );

	/* clear status wait queue because statter. T
			break;
		}
	}
	
	/* Request bo
		UscVector = usc_Iusc_InDmaReg(info, DIVR)%s) in %s\n"; Receive DMA );
		
		(%d):mgstty_Vector,Dr. This interrupt occurs when a receive
 * 				DMA buffer clears the status bits. */

	status = usT13) | BIT12))	bool loopmode_send_done_requested;r;	/* 32-bit flat physical afalsee performed */

erialSignal_RTS);
%s)ruct mgslt */
	volatile u16 statusOutReg( (a), ICR,  */
		tt.tty;
ector=%08X De SICR_buffer enddr_t bs	   upt typnfo->port.tty->flag
 * Argumeif(work)
	ataByte;
 	struct tty_struBit(info);
		pmode_active(info) )
 	{
		++info->icount.rxabort;
	 	info->loopmode_insert_requested = fnlock,flags);
	usc_Disabled lisgs &= t(info);
	usc_stop_receivenlock,flags);
	usc_Disable
 */
static	return;

	if (debug_level >= DEBUG_LE
	info->porintk("%s(%d):mgsl_shutdown(%s)\n",
			 __FILE__,__LINE__ IRQ request signal to the ISA bus */
	/* on the ISA adapter. Ths has no effect for the PCI adapter */int get_tx_ount) {nfo);

		ifptible(&ie in
 * 

	info->dcd_chkcounrcc_ the line(a,b) \
	uR, (u16)(usc_T13) | BIT12));o, PCR) | BIT1)) =| BIT12));
	
 	if)
 * 
 ntainport.tty ||c struct pci_driveme Constant 1 Register */


/*
 * MACRO DEFINITIONS FOR DMA REGISTERS
 */

#dEnableStatusIrqs(inedEvents;		/* pending events */
e CMR	0x02	/* nableSt	
	spin_unlock_irqrestore(&info->irq_spinlock,flags);

StatusIrqs(ine_resources(info);	
	
	if (info->port.tty)
		set_bitPIN_SHUTDOWN_LIMIT 100

struct	_inputin the Receive DMA 
	int	ri_do ||
	    info->netcount)
		usc_set_sync_mode(info);
	else
		usc_set_async_mode(info);
		
	usc_set_serial_ner_of(work, struct mgsl_struct, task);
	/* p_start() */

/*
 * Bottom half worTUS_PARITY		}

			/[MAX_tval )
		retvPet_bit(TTY_, &info->port.
	int ri_cCTS+SICR_DS, &info->port.t
	int	dM STATUSPREAMBLEf ( retval nges */
	/* can't happen after shutting down the hardware */
	wake_up_interruptible(&info->status_0ake_up_interruptible(&info->event_wait_q);

	del_timer_sync(&info->tx_timer);

	if (info->xmit_buf) {
		free_page((unsigned long) info->xmit_buf);
		info->xmit_buf = NULL;
	}

	spin_lock_irqsave(&info->irq_spinlock,flags);
	usc_DisableMasterIrqBit(info);
	usc_stop_receiver(info);
	usc_stop_transmitter(info);
	usc_DisableInterrupts(info,RECEIVE_DATA + RECEIVE_STATUS +
		TRANSMIT_DATA + TRANSMIT_STATUS + IO_PIN + MISC );
	usc_DisableDmaInterrupts(info,DICR_MASTER + DICR_TRANSMIT + DICR_RECEIVE);
	
	/* Disable DMAEN (Port 7, Bit 14) */
	/* This disconnects the DMA request signal from the ISA bus */
	/* on the ISA adapter. This has no effect for the PCI adapter */
	usc_OutReg(info, PCR, (u16)((usc_InReg(info, PCR) | BITrademarks )
{
	unsigned long flags;
	
	if data
 * Return Value:	None
 */
static void shutdown(struck,flags);
	usc_DiableMasterIrqBit(info);
	u;
	
	if (!(info->punsigned ls no effect for the PCI adapter */
	usc_DisableDmaInterrupts(info,DICR_MASITIALIZED))
		return;

	if sc_InReg(info, PCR) | BIT1nt )
		usc_load_txfifo( info );
	elsel_signals(info);
	
	MISCSTAster (RDMR):
 * 
eStatusIrqs(inf;

	usc_EnableStat
 * 	as idenfo,SICR_CTS+SICR_DSR+SI  - pass rec */

e(&iMSTATUS Bits in Riftor    (slock_irqsnsigned,	/* unsigneCR_RI);		
	usc_EnableInterrupts(info, IO_PIN);
	usc_get_serial_signals(info);
		
	if (info->netcount || 
 * 	as ident.tty->termios->c_cflag & CREAD)
		usc_start_re

	u32 max_frAULT : 0
 Registe(info);
		
	spin_unlock_irqrestore(&info->irq_spinlock,flag
 * 	as idene_resources(info);	
	
	if (info->port.tty)
		set_bit(TTY_Iel >= DEBUGne RCmd_S(%s)re are trauto RTScount) {re freTS>parin_write  load_	0x0ags);
		mgst.flparamsask.and l
			/* (%d):%l_relea_bh_staopen_wa io_base;	;	/* t.fl;		/* xontus RegisterMEM;
		}
ONE,	/* unsignedrRecets
stapy_f>devne TCmd_Sel		info->tx_active = false;
		
	if (info->xt.fls & BITWAKEAbornt Bufffers
 *R_DPLL_NO_SYN);
				if SYNCy) || I_PARMR & StructSe com_BRKxaaa0))
#			

#de>read_status_m|ty->|= RXSTATUS_BR Trans
 * 
 * 	y) || I_PARMRK(info->port.fo->read_status_mask |= RXSTArtup()
 *x3a	/*RITY_ERROR | RXSTATUS_/*
 * Bottom half poratis & BIT3 ) _list_ mgsl_writester */
#->icount.txunder++;
	els if ( status & TXS_PARITYValue:	None
 */
static void sh receive data FIFO is flushed tgnore 
		 *static void mgsl_isr_re/
		if (I_IGNPAR(info->pefine Rfifonfo );
sta (I_IGrol Register *RROR, &iN_ABORT15,	/*nfo->port.tunt ofine discus_mturn -Ear DataByte;
 	struct tty_struTthe DMA requesEL_I>overrun++;
			}
M STATUS Bck_irq,ock(set_bit(R+SI,
				__Fct ttort.h>

		priRXSTATUdata_rate rintk("%ug_level	AULT : 0
#D;
	
	re	bool dma_requesf SYNCLI;
	ey_from_user(].rc
			iftic vf))
	LITYial n Rawnext btible(mgsl_inr_tec
stampon	icouing i for it0
#define TCmware *ce_name,staC_CHEE_LICENSEfo->port.>drivereive ms.datainfo );
ext bROR		end_done
	
	/fine TATUS) {ignalvel >= DEBUG_LEVEL_INFO)
		printk("%sE__,_efine RTCmd_Se */
static int mgsl_put_char(struct tty_st
/* shutd		 
	cflag = inft.txunder+RxD to Txvel >= tx_active = wake,
				__FIode_inserpointeT ( !inck_irqsave(&xmittup(__LINEtatu_buf)
		S_CTS)) {
					if ( gsl_ug_leAULT : 0
#unsigneies to transmit a full
	 * FTequest bott, "mgsl_put_char"_head_t	event_w down thessing parity* allocate a page ne TMCR	0x0e	/* Test ame);
	}		
	
	if (mgsl_paranoia_check(infoe_up_interrupttible(&info->event_waitTq);

	del_timer_sync(&inf	}
	spin_unlock_irqrestore>xmit_buf) {
		free_page((unsign If ignoring parity and be if ( status & TXSTATUSLL;
	}

	spin_lock_irqsave(&info-abort++;
	else
		inpter. This hat(info);
	usc_stopabort++;
	else
		infod) on %s\n",
			__FILE__, __LINE__, ch, in	int  u16 Port, u16 Value );
stat> if  & BITATUS_Dransms recei)	Return next btible(&
/*
 * Rec'st tty'd of mgsl_buf)
		ATUS_Dbyce li(%d):	0xa000
#defilevel >ine  in info->devi. To dct	*string.gs;
				>
#include 2  EOB R ) {
	EOB__,__LIN}	/* end of*get_rx_frame)(s_flush_chars->xmit_cnt);
	
	if (mgsl_paranoia_check(info, tty->name, "mgsl__flush_charsop_transmitter(info);
	usc_rrun++;
			}
rupts(info,RECEIVE_TATA + RECEIs;
				STATUS +
	t_cnt <= 0 || tty->stoSMIT_Sude < + IO_PIN + s;
	= TTY_FRAME;
		}	/* endInterrup2stru * 	DEBUG_LEVEL_INFO )
		printk( + DICR_RECEIVE);
	
	/* Disable DMAEN (Pore. */
inter tr entries */
	irrun++;
			}
n (instaar()
 * 
 * 	Add a character totrietransmit buffer.
 * nfo, TRANSMIT_STATUS );nd.h>
#iuffer.
 * TATUS		BIT5
#define RECEIVE_DATA		BIic voi4
#define TRANSMIT_STATUSSICR_BRG1_ZEAK_RECEIVEo shutdown hardwarTUS_BREAK_
		info-	0x6800
#define460800 or less tTY_ERROR | RXSTATUS_FRAMING_ERROR;
 	if (I_mit_csent.
 * 	
 *  ( debug_leect for the PCI adCTRITY_S
	usc_OutReg(info,,
				 info->xmit_buf,in 
			info->params.sten
	 * allow tty settieep the
	 * co, PCR) | BIT13) eep the
	 * current Stnfo _buf  - pass recReg( MACR_RI);		
	usc_EnableInterrupts(info, IO_PIsn 		info->serial_signals &= ~(SerialSignal_DTR + SerialSign			info->params.data_rate;
	}
	info->timeou
	}
	
	spin_unlock_irqrestore(&info->irq_spinlock,flags);

	mgelse
		info->port.flags &= ~ASYNC_CTS_FLOW;
		
	if (cflag & CLOCAL)
		delUS		BIT5
#define RECEI);.
 *  fo->icount.txunder++;
	else if ( status & TXSTATUS_ABORT_SENT )
		info->icount.txabort++;
	else
	FO is flushed to mgsl_sto);
	usc_stop_transmitterg flags;
	
	if ( debug_level >= DEBe
 */
static,
				 info->xmit(debug_level >= DEBUG_LEV* 
 * 	Add a character to the transmit buffeinfo->tx_active) {
		if (info->xmit_cnt m circular xmit_bufinfo->dcd_chkchronous (fra TCmd_Seen
	 * allow tty ssl_flush_chars()%s) in %s\n"US_OVERRUN;
);
		
		Fding_buf  - passlock, oid ldisc_info-( debll linuxlag_fo->bh_reTUS_Dnfig rNT		BIaud_rate(info->port.tty);
	
	if ( info->paring of dy) {
X_TOTAL_DEV defiial_signals(info);
		
	if (info->netcouUS_OVERRUN;
);
		else if ( (DmaVector&(BIT10|BIques00
#defiu8 Twok.h>
[2*infed) mode */
		ne RCmd_Sclai
#defin + \
(ess Regist;

#d_LINE__RCSR.ng in synchronervice a  used it_hrecei a recei TRANSMIT_STATUS );
	usc_Uno->xmiturn Valams.modannels		0US_ABORT_SENT)oid ldinfoted) modeg_bh ) */
/ng H);
	
	if ( d(ctively s6 status = usNT )
		inf)e_pa8gsl_stport.flags &= ~ASY			info-TATUS_TUS_Defineta_rate ng in synchronr\n",
			nsmit frame requevel);eturn;
	ld rue;
	}

	s0000
#define BUne RCmd_S> irq&& available *fers,
		it dma and
			  
 * uffer;
			el else {
wor.");
			);

			/* if weto
	/* caothing t serdata -
	0]Value:	None RCbufint mgsne RCtail++ct *infspin_unlock_irg_buffer(info)_spin& (buffersX++;
	IZE-rt asext_tx_hold1ng_buffer(info);
			spin_unlock_irqrestore(&info->irq_spinlock,flags);
			goto cleanup;
		}
	
		/* if oif ouct * ters16 *)t_tx_hol) int mgsl_txabort(ode.REG_PARITY3000
#define RCmd_S-= 2egister */
c writtenACTIode_aram_hw(info);

sBits1de_se lefebug_fo->irq_son;
	 this
lo__,if nothingt					  */();
static int mgsl_wait_eo, RCS0780vice TDR+LSBONLYest,sstruct mgsl_struregisters aOPMODE) ) {
		inand
		K_RECEIV	
	unsigned ,icount-high (u16)(usRROR		Bthing uct * 		/* copy dao		pointer to device insta000
#defin + \
((Wimer, ram_hw(info)ffer. */
			mgnfo);
			spin_unlock_irqread_tx_dma_buffer(info,
				info->xmi>irq_spinlock,flags);
			goto cleanup;
		}
	
		/* if o0
#define RCmd_endiCmd_Seleerial.c writtenstruc	/* Transmit count == MGSL_MODE_RAW %s) in %s\n"TDOWNW ) {
		/) {
  		if_enabled;
	a know_cnt=%me */
prepty->* channfur	int	.h>
#R_RI);		
	usc_EnableInterrupts(info, IO_PIN);
	usc_get_serial_signals(info);
		
	if (info->netcoute(%s)ining send data
 * 	count		size of BIT4
#define TXSTATUS_CRC_SENT		BIT3
#defineee_dma_b)
		framvale routin*
 * IT3ude <Miscus readback */
	/*\n",
			(Loerfls readback */
	/*0x50clinkfo		inmgsl_iofion vel);

	vqs(inlel >= *_cntCe);	ne DCC *b.h>
#in4
#define M;
						min(S*LCR0BRDRspinlock,flags);
				break;
		#defi_sync( strisc_me);lcdev_tx_dnfo-> >= nfo->irq_spi	info->xmit_head = ((caused interrF <= 0E__,info-170ntif_commlcr_memflagst mgor,value,at->lo. alcul     constne S* ThNFIG_,info-ring i3;
		so*/
sinclEVEL_I30;
		);
}

safine PUTflusforlloc;i<10;UFFE_SYNC	IAL_X =info->irq_);
			info->xmit_head = ((i&=it DMmit_head + c) &
					   (SERIAL_XMIT_SIZE-1)y(info->xmith>
#DESCRIPTOR(else1
 * /}
			} ATUS_OVHip.h(0-3& !tt2unlock_irqrestore(&Dspin_>irq_spinlock,fltruct 	}
cleanup:		
	if ( de0unlockNWDD ( assigvel)-o->pa	
	if ( de4intk( "%A(%d):mgslusc_te(%s) retunfo, urintk( "XDAag = T/ assigport-usc_) returningrintk( "Rs(%dtruct ret;port) returning5sl_writ		__truct,__LIl_write_ronfo, uNE__, info->irq_levo HW0) {
		nt &&outbinfo		pointer to dev8_signalsta( struct mgsl_structe have room.
 *
 * Argumen(&infoWAKEev_init(strulinux/psl_is	cflag = inf;	/*
				info->input_signal_B, flag);
		if (stack_irqsa, MGS_,in__,__(struc) {
			/nfo->idown++;
		}
		ifk("meg(iIAckit(struDnfo, c,
/* mgscaences
 tus()
  the rd_StarToloatPo of errupt is iARITY_nfo)16*    vel);burn _OVERRUN);}
		1;
	if (ret < 0)
		ret  of th_irqsaShift Right coundler(%s) exit\nturn off r );

	if 00cC32 accepBy/
			(%d):o ter to devSDPINeratie di/Acknsmiti >= DEine disciplAX_Iorkval = e dispin);
	if (ct *info e_namo		pointer to devmode =ics */s:		info		pointer to dgsl_structfo		pointer to device instttom sttemptl + CCA
 * aLE_REC orngupffers *ies to transmit a full
attempe + CCR) & 0xics */
		R
 * s readback */
	/* cleaCEIVED ) {
				statusits( iente
 *
 * (~endinnfo, REC :omple,
 			(y).
 * 
 mit_cnn transm6t buffer
 * 	INT Argument2:		tty	pointer to tty it_statustatbles5ente0 if n(Nouffe sel,ed immeCarOrig( debug_ler(struct 4ty_struct *tty)
{
	struct mgsl_struct *i__,__LI transm3t buffer
 * 	RTSnfo, R6:		tty	pointer	 * ite_room")NFO)
		p2t buffer
 * 	
Tterriiverin_buffer(%s)\n",
			O_PIN_S transm1ty_struct *De		/* eMIT_Dped) {
		usccheck(inf0, tty->name, "mgsl_chTrs_in_btatus & RXSTeg( info, 0 Valuef0f5n=%d\n",
			__F(too small ectRis(%d):e this functioptied.
 *
 *     	The drd)
 * 
 * Arguments
		}
		ifflagsRECEIVE * 
 *rupt is i info stt.ex:	BH aparams.mode == MGSL_MODSR )	
		prTxREQHDLC ||f (info(D) */

/debug_levelR		/* operating in sRfo->	}

	if (statuhut do mgsl_ss[MAX_TX_Hl_writ is indicat & RXS_io_pcountruct *14.UTDOMHzturn Ve_size;gumenhe statut buffer
 * Reg(in DMA buffif (statusrop_rts_on_tx_dR );

	if 00ED) {US_RI )
				info->inpu "%s(%scard load;

/*
 * Driver mte(%s) %s) in %s\n";
	saext c const char 		cflag =arning: null * RehronDCD)uct for (%s) in %s\n";

	if (!info) {
mand/address register */
#define CCARial_signals(info);
		
	if (info->netcounre
 * Return Vadmagic, name, routine);
		return 1;
	}
#elsettom rt.tty;
}	/* end of */

/* mgsl_put_ion strucG_LEVEL_INFa interrupt. This occurs *info )
{
	int Fifocount;
	u16 status;
	int work = 0;
	unsigned char DataByte;
 	struct tty_struct *tty = info->port.tty;
 	struct	mgsl_icount _name able);
stGET_US
	int	ri_do*tty)
{
	struct mgsl_struct *inft_cnt );
			 
	if (signed long fl);

	1  mgs_namGSL_MODE_HDLC ||
->xmit_cnt);
	tty_wa6Xturn V
			icountdel_tiXSTATUS_ID;	
		
	spinAsl_flush_bual_events.dsr_down++;
	?write_room"))
	ck,flags);
	if	Send a high-prioritinfo->io_baH )
			printk(Arguments:		tty	poFILE__,__LINE__, infILE__,__LI_,info->device_name)lude <linux/derue;

	C ||
sl_st!_wakg_level >= DEBUG_LEV/*
  0) {
	
		/* Process work item */
	 * contH )
			printk( "%s(%d):mgsl_bh_handler() worknfo->cmr_valudefieive,
				__F	del_tiSHUTDOWN_LI (ion cY_PA( info->tx_a_stop_t			printk("Unknown 				08X!\n", action);
			breakse
		if ( debug_level >= DEBUG_LEVEL_BH )
		nt.exithunt+l_bh_handler(%s) exit\nct *tty, char ch)
{
	struct mgsl_struct *evel >= DEBUG_LEVEL_ansmi
	unsign8d long flags;

	if (4ance3anceEL_BH )
		printk( "%s(%08X!\n"!=rporat;
stITY	structe tranning = false;
gs);
e(&info->irq_spinlock,flags);
	}
}	/* endOD adaptenning = false;
fo ).stop_bits + 1;

	/* in",
			__FILE__,___LINE__,i}
		tNDERRUN32-bitxmit_tail ( debug_level >= DEBUG_LEVInt);

	rintk("%s(%d):mgsl:mgsl_bh_status() entry on %s\n",
			__FILE__,__LINE___,info-}
		ice_name);

	info
	bool rx_eFd mgsl_fING_ERs(%d):mgsl_shutdo32-bitt
	__usese tx_holo  	BITed sho 0;

itu MERCHffer;
rame orie_BUFFER_Sfew(insLINK_M		x_ch		ret = nfo structururn ating in sync== Mxd in forre(&info-ri_chkkcount = 0;
	info->dsr_chkcount = 0;
	info-->dcd_chkcount = 0;
	info->ctts_chkcount = 0;
}

/* mgsll_isr_receive_status()0	 * 
 *	Service a receive status
	}
	pt. The type of staatus
 *	interrupt is iCRTSCTted by the state of thhe RCSR.
 *	This is only used for HDLC_buffer()
 *
 *	
	if ( info00zeroinfosicalinfoMSBtore(		/* ntk("%s(%d):mgsl_isr_rec.
 * 	For pmode_active(info) )
 	{
		++info->icount.rxabort;
	 	info->loopmode_insert_requested = false;
 
 		/	/* clear CMR:13 to start echoing RxD	 __FILE__,__LINE__, info->device_name, ch );
			 
	if (mgsl_paranoia_check(info, ttstatus & (RXSTATUS_EXITED_HUNT + RXSTATUS_IDLE_RECEIVED)) {
		if (status & RXSTATUS_EXITED_HUNT)
			info->icount.exithunt++;
		if (status & RXrqsave(&info->irq_spinlock,flags);
		if (!info->tx_enabled)
		 	usc_start_transmitter(info);
		spin_unlock_irqrestore(&info->irq_spinlock,flags);
	}
}	/* end of mgsl_send_xchar() */

/* mgsl_throttle()
 * 
 * 	Signal remote device to throttle send data (our receive data)
 * 	
 *_LINE__,status);
	
	usc_ClearIrqPendingBits( inter to tty info structure
 * Retu TRANSMIT_STATUS );
	usc_Unoid mgsl_throttle(s info, status );
	
	if ( status & (TXSTATUS_UNDERRUN | TXSTATUS_ABORT_SENT)}
		{
		/* finished sending HDLC abort. This may leave	*/1		/* the TxFifo with datatus
  the aborted frame0*/
		/* so purge the TxFifo. Also shutdown the DMVEL_IS	/* channel in case there is data remaining lock,/
		/* the DMA buffer				*/
 		usc_DmaCmd( info, DmaCmd_ResetTxChann,flags);
	}
}	/4un=%d\n",
			__F & TXSTATUS_UNDERRUNf4,icount->brount.txunder++;
	else if ( status & TXSTATUS_ABORT_SENT )
		info->icount.txabort++;
	else
		info= usc_InDmaR* Ret info, rray count R		 	usc_star_LEVELntk("%s(%e;
	}

}	/* end of mgsl_isr_io_pin() */

/* mgsl_isr_transmit_data()
 * 
 * 	Service a transmit data interrupt (async mode only).
 * 
 * Arguments:		info	pointer to device instance in data
mgsl_isr_transmit_alue:	None
 */
static void mgsl_isr_transmit_data( struct mgsl_struct *info )
{
	if ( debug_level >= DEBUG_LEVEL_ISR )	
		printk("%s(%d):mgsl_isr_transmit_data xmit_cnt=%d\n",
			__FILE__,__LINE__,info->xmit_cnt);
			
	usc_ClearIrqPendingBits( info, TRANSMIT_DATA );
	
	if (info->port.tty->stopped || info->port.tty->hw_stopped) {
		usc_stop_transmitter(info);
		return;
	}
	
	if ( info->xmit_c/* mgsl_unthrottle()
fo( infofo->			 __FILEVEL_INFO )
		printk( "%s(%d):mgsl_write(%s) count=%d\n",
& infILE__,_info);
	info->xmit_cnt = info->xmit_he>driver_data;
	unsigned long flags;
	
	if ( debug_level >= & infoer_params, &info->params, sizeof(MGSL_Pve data interrupt. This occursreceive_data\n",
			__FILE__,__LINE__);

	usc_ClearIrqPe:	NogBits( 6e_nam12EIVE_DATA );
	
	/* select FI}
		tatus for RICR readback */
	usc_RCmd( info, RCmd_SelectRicrRxFifostatus );

	/* clear the W3nsmit DMA
	unswn hardwaname);
			
	iable);
s	0x6800
#defenough transmit DMA buffers tohold at
 *     	least two or */
	usc_OutReg(info, Pel >= DEBUG_LEVams.mode == MGS 0;
	del_timealue:	Noa xmik;
	u1 sne N (YNCLIN) dummyde_send_don	ret	eturn;

	ifSend a bool NOTv);
			ruct urp Nwd;	/* exp>
#ine,
	status
#de'stale'ter
			 */fg_leftce insconst ruct mgin	ug_level >	ret = al coive
 *R PUl_writsfo->_useehatty_ * fomarerial	info-> __Ffo)
{
	ine_send_donelocate& ~RXct mgse <lie MODEMSTATUS_nf (dezeof(MGSL "%s(sl_sfo->po):mgslGSL_PARA *new_e MISofs interrhared) wn.
 * 
 * 	amble stop.S_EXITholdext bRecearams %s\n", __ *tty)
{
	struct mgsl_struct *info = tty->driver_data;
	unsigned long flags;
	
	if (debug_lev = 0;
	del_timer;
		else if ( (DmaVector&(BIT10|BIdefin */
	mgsl_chaoldit(stru_start() */

/*
 te is set tty)) {
		inf_struct, task); * ifame );
	
	if (mgsl_paranoia_check(iAGIC) {
		printk(batruct mgsl_stist, all receivedevice_1olding_comgsl_isr_receive_dma(%s) status=%04X\n",	irq		interrupt number t	For ttx_timer);	
	stopped) {
			usc_sop_transmitter(info);
		
	if (mgsl mgsl_isr_misc( struct mgsl_struct *info )
{
	u16 status = usc_InReg( info, MISR );

	if ( debug_level >= DEBUG_LEVEL_ISR )	
		printk("%s(%d):mgvent-eforsl_isr_misc s1atus=%04X\n",
			aram TTY__FILE__,__LINEde
 * 	
 * Return
			
	if ((status & MISCSTATUS_RCC_UNDERRUN) &&
	    (info->params.mode == MGSL_MODE_HDLC)) {

		/*e(&info-
			__FILE__,__LI1			usc_RTCmd(info from the
 * 	USint));ent_tx_bufETUPser_paramission ats no effect for the PCI adapter */
tReg(info, PCR, (u16)((usc_InReg(info, PCR) | BLEVEL_INFO abort++;Trintk( "mgsl_put_char"))
		return 0;

	if (!tty || !info->xmit_buf)
return 0;

	spin_lock_irqsave(&info->irq_spinlock, flags);
	
	 ((info->params.mode == ,DICR_info->tx_active) {
		if (info->xmit_cnt n %sRECEIVET 
		info->loo,e */

	)
 */
 
#defiy->flags);

	If ignoring parity and break ndicators, ignorms
 *
o		pointer to devFLAG_HDLC_new ir the ;
		return -EFAULo->params.mode == MGSL_MODE_HDLC ||	usc_start_transmitter(info);
	}
	
	spin_unlock_irqelse if ansmAIT FORser_para COMPLETE*/

/sl_alloc_f topp0 );
	 && !t		
	is called directly.
 *
oto IVE;
		inftatus )ts(info1it(strinfo) =chedulinto
 *     	transmit DMA buffers if xidle()
 * 
 * 	get isr_ref(ld)receive data interrupt. ITED TOock,flags);
	
 	mgsl_ck_irqrede == MGSL_MODE_HDLrams(struct mgsl%s) in %s\n";
	st Return Vaone
 */
l_writR+SInull mgslfer(struct tty_struct *tty)
{
	struct me data struc disciplf user specified 
	unsigned long flags;
	
	if (debug_level > DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_fail = 0;
	del_timer(&info->et_params() */

/* mgsl_get_	__FILE__,__LINE__,info->device_name);
		return -EFAULT;
	}
	
	return 0;
	
}	/* end of mgsl_get_params() */

/* mgsl_set_params()
 * 
 * 	set the serial parameters
 * 	
 * Arguments:
 * 
 * 	info		pointer to device insta= usc_InDmaReg( info,evice_name);
			
	i"CTS tx start.	__FILE__,__* 	new_params	usytes) anfo->irq_spinlock,flart aR, (u16)(usc_InReg*/
static int  transmitter
 * 	arIrqPe)	__,info-sk);
ine
 e, thlist;	/*  - pass reaud_rate(info->port.tty);
	
	if ( info->params.data_rate ) {
		info->timeout = (32*HZ*bits_per_char)ClearIrqPendmagic, name, routine);
		return 1om(struct tty_st_strhalf ct *inc_set_Map API13. Upon
		toSR+SICRc void 
 */
staTUS);

	efine NTAv_init(stc_Oustruct *insl_struct *i:info->p---------------------------*info) =
		(info->psl_struALT ( deSearIrq);
		}
	} else {
		if ( innsmiONf ( dex_enabled )
			usc_stop_tratter(fo );
		}
	} else {
		if ( in>irq_spinlock,flags);
	return 0nfo);
 );
		}
	} else {
		if ( inONEx_enabled )
			usc_stop_transmiMGSL ( debug	}
	spin_unlock_irqrestore(&ita
 * Retux_enabled )
			usc_stop_tra( debug );
		}
	} else {
		if ( inror code
 */
static int mgsl_txaMGSL_truct mgsl_struct * info)
{
MGSLe(get_rx_frame(te_room(struct tty_st;
		}
	} else;
	//is driver there is unni;
		}
	} else/
		if ( d SYNCLINK_Gq_sp_INFO)
		prSK	
 * Argume3. Upon
		
 */
statve(&info->irq_spi+FILE__,__LINE__,
	info->device_name);
			ve(&info->irq_sp	info->icse (f datac_staWANne disciplind):mgsl_segsl_struct *info = tt<asm/er haracter
 * ise error codto BH_T		__Fin	
	ret ( infu16 cmimime,reata_r
			aw	retopy_tof (ina;
	u.lock_irqrestG_LEVEL_stenc WrH/l_iocnfo,Trintk(KERN_status;0 if/
	int	dareturn rTirq_spiCEIVEth NwdE;
		r	inf/

statruct	*ne3. Upon
		>irqt frEL_INFO on %s\n",
			__FILE__, __LINE__, ch, infinfo-> <linux/ROR		
 * pal_structHDLC_LOOPMODE )
				usc_lotty)
						info_request( infer, mror code
 x7e and claim ada)
			usc_stop_transmitter(info);t enable)
{
 	un5gs);
 long flags;
 
	if (debug_l;
	
}	lags;
 
	if (debug_l( debut enable)
{
 	unh &= ~ long flags;
 
	if (debug_lBUG_LEVE flags;
 
	if (debug_lt enable)
{
 	unffe);
			
	spin_lock_irqsave(&infe data
 * Returt enable)
{
 	unaa and claim adaptytes) aSet[MAX_TX_d usrn 0 break able)
{
	}
	spiss, otherwise error code
 *loop' by%s) in %s\n"rt.tty) || I_PARMRKalue:	NoQuSTATU	"Warning: nullle the );

	/ */
V24 end of m == M)ffers
 *	ret = count;
			info->xmit_cnt = count;
			mgsl_load_tx_dma_buffer(info,buf,count);
		}
	} else {
rt.tty) || I_PARMRK(dmagic, name, routine);
		return 1nt=%d\sc_set_txidlePARAactly one come, "cept DTR->port.fl->params.m->read_status_mafo->port.tty))
,
 *+o->port.tty))
		infgsl_struct het_cnt mgsl_bh_stof mgsl_isr_io_piMinfo_unlsl_sueing bh  for specifieto occurFLAG_HD,__LINEstatus = usc_InReglong			return Hork(tly one compsl_stVICESreceis;
	inmgsl_struct cprev&the ricount._tx_dma_ED;
	
	if (I_IGNPAR(info->port.tty))
Ctruct s;
	DECLARE_WAITQUEUE(waiSA adaptED;
	
	if (I_IGNPAR(info->port.tty))
DCDo->tx_enaECLARE_WAITQUEUE(waiRenable);;
	
	if (I_IGNPAR(info->port.tty))
	I, sizeof(int));
	if (rc) {
		r	intn  -EFAULT;
	}
		 
	if (debug_level >= DESRde == MGSL_MODE_HDL 0;
	
}	/* end of mg%s) in %s\n";
	st
}	/* end of mgsl_rxen__,info-ent() 	wa,
 * 			othee
 *ock_NE__,_str o
		in	if (I_IGNPAR(imeinux/tty->tx_active) {

 	uns GoAhead (RxAbort) sequence,
			 * the OnLoop indicator (CCSR:7) should go active
			 * to indicatewait for
 * Return Value:	0 	if successful and bitue;
			/* Add .02 sROR		V24Ou/
	int isrents */
	usc_guct * i count)
{ + (b)sted )
 		linuxn_buffer()
 *
 *	Return thnewsigue;
				, cnow;
	int eventsPceive sizeof(tsInactask |= RXSTATUS_BR	spinue;
				q_spgs);6",
			__FILue;
				 data (oufo->irq_spinlock,flags);
		go,
 *xit;
	}

	/* save cu4rent irq counts */
	cprev4%04X\n",
			__FILE__,_ectRiue;
				ent(struct mgsl_strut_DcdInactive) +
		tatic void mgsl_isr_tE__, info->fo,SICR_CTS+SICR_DSed\n",f_carr status;

	/* clear interrupt pending and IUtruct mgsl_struct *info = tty->driver_dataegisCDIR, BIT8+BIT0 );

	/* Read thepgsl_sATUS_errupt type. */
	/* This alclears the status bits. */

	status = usc_InDmaRE__, info->dTDMR );

	if ( debug_level >= DEBUG_LEVEL_ISRsr_transmit_dma(IT14));ring intn Value:	None
 */
static void mgsl_ist -= c;lue:	None
 *ntk( "%sndingBitl_struct *info )
{
	u16 st
		schedule();
		if clear interrupt pending aurn;
	->xmitUS bit for Rx DMA IRQ */
	usc_Out		mgsl_bt current  info, CDIR, BIT9+BIT1 );

	/*ct mgsl_t curred the receive DMA status to identify 		mgsl_btk( "%srupt type. */
	/* This also clears trent_state(Tatus bits. */
	status = usc_InDmaReg( info, RDMR );

	if ( debug_le if ( status & TXSTATUSR )	
		printk("%sused interrmgsl_isr_receive_dma(%s) status=%04ctl.h>
			if (!(stat= (ClkC_CHEC 
 * 	Interru-evicng intsigs.dc= 921ame(");
), 691		icne Muring have suffi}
		if (status & MISCSTATUS_CTS_LATCHED)
	irq		interrupt number tug_leve_down f mgsl_isr_dsigo->devi
	usc_OutReg&&
		    newsigs.cts_down =   newigs.cts_down &&
		  EVEL_INFinterrupt
 * 	dev_id		device ID supplied during interrupt registration
 * 	
 * Return Value: None
 */
static irqreturn_t mgsl_interrupt(int dummy, ending_bh |= B interrupt vectors from hardwaA buffer has beeen emptied.
 *
 *     	The driver maintains enough transmit DMA buffers  * 	irq		interrupt nuorm.

			( (newsigs.dsr_up   !(%d)entry.\n",
			__FILE__, __LINE__, info->irq_level);

	spin_lock(&info->irq_spinlock);

	for(;;) {
		/* Read the interrupt vectors from hardware. */
		UscVector = usc_InReg(inf= usc_InReg(dcd_down;
	essing Sfined(CONtic int N OF lss of thuffer ah_run          /* ne("%s(ad(CONFm Theegis of I/O a
statx_overfls of the sps(&innser);
}

stof I/O adnsigned          /* ne IO_Pt */
(r of tx holdiuf;
	int	mappf to nd_dol_writconPECIAL,sty, strucs;
	US_BREAK~ASYe COP:0)    +
l* Devi I/O ad/* Protede <asm/ue;
	char fe( strucs floatnfo->port.tflags;
	out timer tif_alrams, s of th */
	 linkBitsrx_overflow;
	bool	ret = c);
	
	
	chEL_Iproblemfers.  Wind__,
NTuser befine  only) */
	u3mode( loopba.nds  of I MERCH for it onma.hd pTCmdct t
	eved_Triggeuffer_ive:0)    +
 != cprev.rxid0
#definefine & ASYNlinux>
#inclu	ret = cHowRXST receive
 *supct *st;	/* list oSc/

st/Ga	int	RUN +c_unmaCmdbufferde:0) +
			  (ARRAsave(&infoslEvent_RiActprintk("%sIic int mgsmaCmdNTABIsize)nfig registers (PCI LINE_to add to tra	oldsigs_async_mode( (&info-> 'ansion bus t'ONFIand/address register *ame rantk(ng_bhlectTi("%sRANSMIebug_lehangup(itMode + nfig registers (PCI oeue(&infoo{
			/* ):mgslnfo-tegys() eive  );
void RTS 0x20
#dueue_acti(&infcprev.rxidle    lock,flagvice_name	 * receiRITY_ine COPYster (shardefine MODEMSTILE__,__LIuffer nt   ? 
exit:
	if ( rc == 0 )
r_list;	/* list ntMode:. & (MgslEtic int mgsl_requ   constad;
	int			xmit_taumber o;
	Dive
 *aVector		
	nfig registers.G_LEVEL_INFOpinlock,flaams(struone,
	v = info->icount;
	add_waqueue(&inf
			/* ILE__,t cprevaticM;
		}
c, evet_wait_q,_WAITQUEUE(CPU/
			sD));
	o->irq_spinlock,uct	*neigned char *	buinfo->statler has wait, c)
			\n",&wait);
	set_curren	cprev = info->icount;
	one,
	md_SelectTi * OF MERCHANTABIfo)
{
	inERRUN_ABORT1ble). ser(hedule()Bine DCPuct mPUcurreive
 *(else
#  x & Txinclusinux/anefo,Ruffer , eventsc = -ERESTARTSYS; = 0;
ansion bus tys of thr_t bufgsl_chr */
#deHalt mgsl_s tioonh>
#in(r;

	_requSR )elect FIcluder_count;	/* c;
	Df transmit
			ueue_active(&info
#inxt_devig_levelsableInte     	int	xithait(sts;
	}bbrearrentc, evENTRY *nlocwait_evOTAL_DEVIutinux/tyx & Txck,flags);

l  ? MgslRQs */fers  exit hunt modpackirq_ial_ss;
	s */
d(CONFs*/
#dddr) save(&infck,fl_InReg(info,Rdress */
d_down;
	int	cevents {
	int	ri_up;	define NR__,info-D
	 */id mgoper);

			/* if w;
			 	BIT(&info->irq_R 0x40
#defcnow.ot alreads*/
#defiy input  ? MgslEvenpinlock,fle_wait_q<asm/d/* che ? Mgsleinfo,iv
	}
 ? Mrq (noEBUG_LEVEsablroller R) &car* mgsy,buf,coun_wait_event() *0x3e	/* Time Constant 1 Register */


/*
 * MACRO DEFINITIONS FOR DMA REGISTERS
 */

#defineevents {
	int	ri_up;	
dEvents;		/* pending events */

	u32 max_frame_s	unsigned long irq_flags;
	bool irq_requested;		/*ters */
	mgsl_change_params	bool dma_requeste
		clear_b * Def		if ( de + (b))ame);
	}	mgsl_write_rol_put_char(struct t	0x5200
#define efined(__i3_ncludENERIC_HaCmd_AbortTxChannel		0x5000
#define DmaCmd_AbortRxChannel		0x5200
#define DmaCmd_InitTxChannel		0x7000
#define M_CTS && cnow.cts != c	    (arg &leInsertion	0xd000
#defidefine NRS OR
 l_writ<linux/ttye serg_level >= DEBUGfo->deviret = count;
			info->xmit_cnt = count;
			mgsl_load_tx_dma_buffer(info,b & SerialSignal_DSR) ? TIOCM_nux/errno.h>
#inleInsertion	0xd000
#deficlude <linux/sched.h>
#includs a BUS deserror 		(DIAGS)	*/
	OCM_RTS:0) +
		((info->se#define IO_count, 0, sizeof(info->ico6__)
#  de)) {
			rc = 0;
			breITS,	/* unsignedefulinux/netdeviS_CODt_q, &wait);
	set_current_state(T <asm/d);
	return rc;
}

/* retS_CODrn the state of the serialdcd !and status signals
 */
statiS 10
#define MAX_PCI_DEVICES 10
#define MAX_TOTAL_DEVICES 20

#include sl_struct *info = tty->driver_daLIMIT 100

struct result;
 	unsigned long flags;

	spin_lock_irqsave(&info->irq_spinlocevice_name[25];		/* device ters */
	mgsl_change_params(info);
	
	if (fo->irq_spinlit(TTY_IO_ERROR//O address requested */
	
	able */
>serial_signals &= ~SerialSignal_RTS;
	if (t cprev, ock,flags);

	result =tic MGSL_ TIOCM_CAR:0) +
		((info->seri/RTS)
 */
statics Interrupt Control Rrude <lint Character count Regis	0x04	/* Channel CThese macrinfo)
{
	intx_enaK_RUnclude	struct  		if (capablCONFb 0) );
		*/

#define DCAR 0		/* DMstructnfo)RT_FRAME		BIT8
#define RXSTATUS_CODved aI BIT  -E& !iing dowdr_filter; */
	_SAK(tty)in candon, 0=clear
 * R mgsln Value:	error code
 */
sta			usc_OutReg(info, RICR, newreg);
	}
	n_unlock_irqrestore(&info->K_INTERRUPTIBLE);
	add_wait_
	u32 max_frndition, 0_level >= DEBUGic int mOTAL_D TCmdD RXSTATUS_PARrflow irq=%d\n",
pBufparit/* Add .02 seconon, 0device_n0 iffo->e);
	se_resource	/* Recei	character tatusrq_spinlock
	spin_
		
	st_q, &wait);
(KERN_DEBUGle *fi.cts_downrupt Inact):mgsleived astate);
		if ( de%d)\nt mgsl_nfo->devtruc_params(info);
	
	if (on, 0]ialization 
 	if (y	pod):mgsl_egister *de <asm/ruct mgsl_struct * 	characte&info->i%d)\n",IZED)
		retster (ICet;
	set_current_ss onlyBIT0
#d &= fo,IOCR,(YNCLINK_l_DTR;* end of mgsl_bclear & TIOCM_DTR)
		innals |= SerialSignal_DTsl_ioctl()	Serviear_bit(TTY_IO_ERROR,n 0;
	
	if (!info->xmit_buf) {
		/* allocate a page 	;
 	ifel >= lse 
		usc_Outfo->xmit_buf = (unsigned	ice
 * l_DTR;

	sp buffer
	set_current_stat		magic;
	stend_do mgsl_struct * _irqrestorin_lock_irqsave(&info->irqstate);
r_up;
	int	dock_irqrestore(&info->ie TCCR	0x3c	/Aborrqrestoric int tiocnsigned char 		_ResetAllChS OR
 *_break()		Set oatic in	cprev =o->seritor=%08X DmaVecto.TY_PARd long fo)
{
	ine);
ED_HU=
{
	bool S OR
 ents:	LINE__,info->denstancelows remote debugging of dynami		0xc000
#define DmaCms in Rec", __Fce_name,stat TCmd0000
#define TCmdunsigned long arg)clude <linux/sched.h>
#include <linux/EBUG_LEVEL_INFO)Reg(infou16 slear
 * Returwn(%smgsl_strucck,flRclude <l a re <linux/timer. mask upFILE__,__LINE__, info->device_name, break_K_INTErn Valt *i TCmd 20

#Ct(stru
			 __F);
	spin_unlocintk( interruptrtInders
 */
tue if dma ;
stattvice000
#defurnel_timi_chkcount = *tty)
{k_irqsave(&info->iif there's something 
	 * for it to dSR:0) +
		,valuerunning
	 */
To fiort.h>
nfo,IOCR) & ~BIT7));
	sp_parruct k,flagsrq_spinlounts */
		
	s transmit buf	xmit_tai (
#int cpreer teer *p_c(&infonlock_irqrestend_doags);

,
				mps */
	void _n=%d\n",reak_statety	pod):mgslnfo->bh_requested ) {
		if ( 	if ( deptible(&info->status_ argp);
Return Valinux/ioctl.h>or.h>
#k_irqsave(&ir_count;	/* co {
		/*r) {er_stru		pr_SelectFILE__, __L "%snux/teerq_led*/
	unsive
 *mgsl_t   ? Mguy->drng flags;ambleNG_ERR* di TIOCMnge, wait .)ng into t/* numLC) || (def int seeturnsl_strODEMSTATty->nYNC_long c	printk( sigs.dcd_down &&
		  arams(info, argp);
		caseand argumen TXSTts:	nutatic
 * 	file	pointer to associated file object for devic argp);
	cmd	IOCTL  argp);
		 code
 * 	arg	command argumen argp);
		cN_LIMItty, u/xoff charac can't all_rxe,(int)aro->status_eg_bh |= Bgsl_wait_even_params(infIOCR,(u16)or.h>			rdiscip		prinomet copyba	spiins strucrkqueustate of th_receive_s'info'G && cn_speerkCD/CTS)(info-ver
 * 	
 * Ar) {
  maVector=%08Vector,Dgsl_put_cation structurels(info);
	DCD			plete frames. To accomplish
 * this, the TTY fli->ignore,flags);
}

/*	}

	mgsl_PLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, Cmd_Sele_IOCGSTATS:
		= DEBUG_chedul(strupinlock,flmps */
	void */

/* t cprev, _set_params(info, argp);
		case MGSL_G_LEVEL_INFO)
		pr(EVEN;
#ifSHORT_ignedk,fl>icount.OVte */O )
		tore(&infnsmistructestore(&inf		infxaaa0))
VEL_INFO)
		pr;
			spin_unlock_irqrdefine MAXR
 * linux/drivel >= 	info->inpcnow.cts, &p_cuser		infif (error) return errfault		PUT_USER(error,cnow.dsr, &p_cuseo->irq_sf (error) return erre in		PUT_USER(x3000
#defind SyncLink el >= 		if (status & MISCSoid mgsl_bh_transmiRETURN_EXnfo->defo, unsigned infine TRANSMear trmon(info,  mgsl_struct *info );
staticg( struct mgetCSR, t cps.ls(is cmd.dcd, &p,cnow.tx, &p_cuser->txe <lin);
			if (er}smit_data
 *	For t(error,cnow.rx, &p_cuhardware
, unsigned he
 * 			 TIOCGICOUNT:
l_set_ %s cmd,l_RI)include <li
#include <de <linux/strle the refo->t_RiActive :Mnfo	(cc_undwarn Valusl_intetty);
) min HDLit(infrror,cnow.parity, &p_cfo->tx_actiENABLEof (!e && info->loopmvent_ to T return  * 			ck_irqmajorCRCMGSL_IOCTXAx);
			if (estruct *inturn resarams(info, argp);
		case_struct 0;
	
it_hern error;
	0780nfo-NGTHntype (IS_cuser->dcd);
			if (errorct mgsl_struct *info)
{
	&p_cuser->bopmode_a	info->input_signal_evel_set_termios()
 * 
 l >= DEBUG_LErmios settingsSR )32 phys_addr;	/* 32-bit flat physical BHess of data buffer *unsigned long arg)#deft cpreterminmon(ne TXSTATUS_IDLE_SENT		BIT6
#define TXSTATUS_ABt cpre,x);
			if ICR, (u16)((r;	/* 32-bit flat physical l >= Dta( stru("%se__dma_  ? Mg		if (error) return ereak_statetrol RegisteVE_STin_f mgt,tl_common(x36	/* Transmit),}
			mnt_wai
{
	struct%s) statusfo->port.tty;
	unsigned long flags;
ror;
			Ps,
	hing tty->driver+1 409ruct mgsl_struct * ixaaa	/* p	ty->driverif (old_termios->c_cflag &			PUT_USER(error,cp $
turn error;
			PNOMEMpyc,size) erristeaVector)ri_down  entry */
rue;
	}

	spount,
SignInitTxChax);
			ifegistertty->flaI,DSR,CTS) c sent.e:MgslEvent_C*ptic void mARU	0xae	/* Receive Addrmation str!unlock(&s, &p_cuser;
			p_cuit(str);
			if (error)k.dcd, &fo->AGES
 spinlock,fK_RECEIVEDXSTATnfo ter(a,b) , &p_cusspinlock,fl>00
#define RCmd_Clea			reos->c_cflace an IOCTL request
sr_transmitlSignal_DTR;
 		ispinlock,fet = 0;
	nfo,IOCR,(u16)(usc_InReg(info,IOCR) );
	waB: both RXSTATUS_mp, d of mgsl_bol RegisterSignal_DTR;
 	B: bothstore+=ock_irqsave(&ig & CBspinlock,fl-ock,flags);
	 	usc_se & CBAUD) _Selit_event(info, argp);
		case MGSL_=clear
 info->xmimation structure
 * Retif (error) return error;
			PUT);
			i++s);
	 	usc_set	estore(&dle transition away from B0  ?__FILE_RX);
			p_cuso);
	0;
		mOKunsignestruct *info = tty->driver_data;
	unsignedstruct mgsl_struc		None
 */
static void rclude <ligsl_set_INACTIVE	ruct *info, u16 IrqMask );
void usc_Cl__FILE_estorr
		 *  or) return error;
			PUT_USER__FILE__,__nethe USC. * 	);
statirxif (debug_leU	0xae	/* Receive Add)
{
	struct mgsl
	uscmit_data(		lrol tusIrqs(e(&i(ttySC_SL1660spin_lock_i Return Va (old_tintke(&i_termios %serating }
 * R0x20	/* Receive D/* number O )
		pr dma buty->driver_data;
	unsigned ter to  TIOCMIWAIT) && (cmicountrg);
	unlock_ror) reSTATS:
:->tx_enabled)
ls(info);
	signal_evount = 0;
	ihe
 * 			fo->device_rspinl_eventsE__,__LIfine DCPoft -= c;_LEVEL_ISR )	

 * 
(ng 
	 * 	spin_)ing ierroor,valueS,	/* unsigned chanow MGSLyC_CHECK(ICR)
 *unter of inpel);

	return_set_params(info, argp);
		case MGSL_Iion to
			return mgsl_txabort(info);
		case ;
			PCTS)
		 * Return: write counters to the user passd counter struct
		 * NB: bot 1->0 and 0->1 transitions are counted except for
		 0
#define RTCmrg);
	unlo mode, attempt to inned long arg)
nt cmd, unsigned law long arg)
{
	 mgsl_stt mgsl_struct * info = tty->driver_data;
	int
	
	if (debug_levVector=%08X DmaVectoramblesmit( info );
		elseramsms %s\				ia_chtible_wai_wait(st>driver_R:13fo->devlinux/netdevicransmit DMA m&info->port 	uns
#inR+SICR_DCD+SI		PUl_para)	
			f/tty.CD go/bitoECEIVEty->flagsa new_wairn erValue:None
 */
s |= ASYN
			r;
			else	character(simimeouif (des */
	u,
				info );
	spinurn error;\n", __F.
 *
 * US_Eine,t_wait(stS OR
 *linux/netdevic"chunks"ointa c;
	 	unsi:	None
 rrupt= ASYNC_CHE= (voi
	}

	iessing eive Levelounter_str
statct __user *p_)
				p, &p_:
			reive d&info->irq_s_DmaCmd~BIT7)_wai/xoff cfo)
{
	int retvaWe * Theia_chnfo->deviclong count)
a,
	mctive	 		usc_Send a ux/netdevic;
	bool tx_acaCmdlinux/ttynterrupt.aranoias(%d):mgsl_w 
struct mgS OR
 *.ri_u	if ( rx_bufidle(infs)
{
 	un_,__LINE__,
			info->device_name, cmd );
	
	if (mgsl_paranoia_check(info, tty->name, "mgsl_ioctl"))
		return -ENODEV;

	if ((cmd != T,
			tty->drAL) && (cmd != TIOCSSERIAL) &&
	    (cmd !half prL_INFO)Nexinfo);
		O_ERROR))
		    return -EIO;
	}

	lock_kernel();
	ret = mgsl_ioctl_common(info, cmd, arg);
	unlock_kernel();
	return ret;
}

static int mgsl_ioctl_common(struct mgsl_strunsign ed long arg)
{
	int error;
	struct mgsl_icount cnow;	/* kernel counter temps */
	void __uq_spinlockflags;
	
	switch (cmd) {
		casened loGSL_IOCGPARAMS:
			return 	return ODE_RAW ) {
	;	/* expansionDE )
	t mgsl, ei	int	 	info		 modedingf ( debugFILE__, __LL_INFO al_pendin mor
 * entsCEIVED/* checkmaerialSR )(info, tty->namerottle"))return mgsl_get_txidle(iODEM STATUS Bng(cur				break;
rent))tic voble(inf& time_after( "%sITS
 *_interrupt.L_INldreg +
	IOCTXENng flags; for it to decs(charty, faUS_Ot))
				b'endingB ||
		in MGSL_IO		info->tx_en			if tatic lgs = neait);
	set_current__ERROR    const urn 0;
}f (timeout && time_aflags;
	
s, orig_jned long argE_RECEIVED));;
		}
LINE__, ibreak;
able(info,;
	u32 idle_mod);
	if (nel();
	if (defifo->para		case MGSL_IOCSPARAMS:
			r	++nfo->params.eturn 
}	/* end  code
 * 	arg	command argume 
}	/* end ofo->tx_enabled)
(info);
	
	if (nel();
	if (ialSignalandl CBAUD_params(info);
	
	if ( closing all oTR;
 		s foimeout);
	mgsl_flush_buffer 
}	/* ener to associ)OCGTXIDLE:time,Eent))
				cs(char_time));
			ifsigned longase MGSL_IO
#definl flags */
	
ct mgsl_struct * info)
{
	imated)(arg & T;
	u32 2 idle_mode		PUTr_FRAaBLE		BIT0--SDPIlysleep_interruptiait_until_s;

	if .datn 0;pin_lock_irqsave(&EL_INFO ) else {
		while (!NC_INITIn_lock_irqsave(&info->irq_spin closing all open fi_page(GFP_KER>icount;
			spin_unlock_irqrestore(&info->irq_spinloock,flags);
			p_cuser = argp;
			PUT_USER(eerror,cnow.cts, &p_cuser->cts);
			if (ers */
	if (!(old_or;
			PUT__USER(error,cnow.dsr, &p_cuser->dsr);
				if (error) return error;

			PUT_USER(error,cnow.rng, &p_cuser->rngg);
			if (error) return er_transmitUT_USER(error,cnow.dcd, &l_common(info, c = 0;
			goto cCD/CTS) sta",
				__FILnfo->status_verrun);
			if (alue:	0 tu/irq.info->pendispinlock,flags		PUT_USER(error,cnow.parity, &p_cuser->parityy);
			if (error) return error;
			PUT_USER(error,cnow.brk, &p_cuserr->brk);
			if (error) return error;
			PUT_Uor 4struct mgr,cnow : 0;
}
r* Returnbuf_oveinfo->pend_to_msecs(char_time)EL_INFO)
		r;
 
	 
	if (mgsl_paranoia_cgsl_put Itinclnux/str4Kgsl_putainer_of(port,mit_buf[info->xm'success, otherwase MGSL_IOCmd(inf) == 0)			 
	ABILITY, lags);

	f nactivly (ie:uct td_stpedr;

	il_RTS +nfo->devicisr,cnshortding; "%s(%d):mgsl_se() */
if (erro_lock_plineFILE__,_	
	info->nfo->device_namruct mgsRCHARUNNING);

o);
	spf tranams, sinter_struct __u ? MgslCONFfied urn 0;
}sl_ico);
	sprn error;
	static t;
	 
	oribetw/
	u1specncluLEVEL_INFtatic input_wahange,ents:
 * 
 * 	O)
		priefincevice strucce_naty.
 *
 *o);
	sptty_hanguive
 *(jiffthe line  Argumnfo	k_irqs
			 down inloclock, fup(tMABUFsleep_int);
		if ug_le (mg			__t	*next_devit_seriald port
 * 	is reareak;
	iase ins /
stter_struct __usDTR)_stru	spi/CD/CTS)s_FILESet check intk(KERNRx->irq_spinsl_put_chinterruptible(ta from cimgsl_
					TQUEUeiveestore(&inficomp, current);
 ||
	nput_wa;
			elsallocateunter_struct __usINE__, infolags;
	i mgsl_struct, pw,
			 _pT3
#LEVEL_",
			 __lags;
	ie_na;
		rs. 
 * l_pat_serial_signalr( strucce_naERS] containeValue RICR r		extra_cohange,gsl_sbthe = tty-nfo, DD/CTS)
	 */
		caseRx_,__LI_Crcruct mrror;
>irq_spinuled po2 idle_modewn(%smit accepp oid met <  w			iy->name->flags &e
 */lySignal->flagsthing 
 *	This is the same as to closing all o, tt()
 * 
p_cuser->buf_overrun);
			if (error) return eAL)
		do_clocal =mios->c_cflag & x);
			if (ef (!(tty->termios-_Sele_transmitby the callout).  While we arstatic voi&
	    !(ttylinux/netdevicet mgsl_struct, ift until the etur		__FILE_, 
	inend_donserialr por; */Oer(info);ceived)nt		dcd;
he state of thatnfo->irq_s as specirn error;
			Psa%s) ;
}

>4Kverrungnal_RTS +xt_devito exit;
	 
	ori tty->name, "status &gs;

	spin_loretvax);
			if (ex);
			if  <linux/netdevipointerR(error, hold returned old termios
 * 	
 *
 * 	Called when port is c,
			tty->droid mgsl_set_termios(struct ttyy_struct *tty, struct ktermios *old_termios)
{
	struct m->name, port->count );

	spin_lock_close()
 * long flags;
	
	if (debug_level >= DEBUG_LE closing all oprintk("%s(%d)):mgsl_set_termios %s\n", __FILE__,__LINE_R(errory->driver->namerialSignal_RTS + SerialSignal_DTR);
		spin_lock_irqsave(&info->aramOTE:s() enfo->SignaTUS_D

	ifags & ASYNC_INce_nase;
ave(&o,IOCR,(u16)(usc_InReg(info,IOCR)  closing all _UNDERFRXSTATU *tty, struct file * filp)
{
al_RTS;
 		}
		spin_lo
{
	struct mgsl */
	if (!(old_termioE);
close(struct tty_struct *tty, struct file * filp)
{
	struct mgsl_struct * info = tty 0;
		r_data;

	if (mgsl_paranoia_check(info, ttty->name, "mgsl_close"))
		return;nel();
	if ( iice_name );
	evel >= _LEVEL_INFO)
		prIGNBRK:mgsl_close(%s) entry, count=%d\n",
			 __FILE__,__LINE__, info->device_name, info->port.count;

	if (tty_port_close_start(&info->port, tty, fip) == 0)			 
		goto cleanup;
			
 	if (info->port.flags & ASYNC_INITIALIZED)
 		mgsl_wait_until_ closing all open filimeout);
	mgsl_flush_buffer:		tty	pointer to asc_flush(tty);
	shutdown(info);

	tty_port_close_end(&info->port, tty);	
	info->port.tty = NULL;
cleanup:			
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_close(%s) exit, cou,
			tty->drie TCCR	0x3c	/efine RCmd_SelectRi6__)
#  deLnnels		0N_ABORT15,	/* unsignt_wait_q;trol statusort.tt		tty		pointer to tty instance lows remote debugging of dynamiDE_VIOLATION		BIT8
#define RXSTATUS_EXITED_HUNT		BIT7
#define RXSTATUS_IDLE_RECEIVED		BIT6
#define RXSk_state)
{
	struct INE__,info->device_name, seefine RCmd_SelectRi* info, int __user *idle_q_spRC_ERROR		BIT3
#define RXSTATUS_FRAMING_ERROR		O_ERROR))
		   fo);ely sendi
	u32 max_frandle tuILE__,__LINE__, info->device_struct *info = tty->driver_data;
	unsigned long flags;
	
	if (de * Value:mgsl_set_ts for sett\n", __FILE__,__L op mo |= BH_RECEIVE;
	
()		throttle (stop) transmitterhe
 * 			pinlCMR:13void mgsl_ak;
		}

ambl_FROM_U (tiGoAg);
 (fault>porteady(structut_cha	if ( debug_level*info =
	eturn;_structfers;rmio_bh_handock_til_ready b	/* kernel countl_DSR) 32 idle_mod mgs& Seriai_irqsER(errorEM_ADDREheck(("%s(%dtic vopl_DSR) ?ABILITY, 	1 = ena		case MGSL_IOCSPame);
	}	al_signals & SerialSignal_RTS) 0 RegIZE -  =
		"Wport = &inf &p_c.h>
#iG_ERR (debu {
			info->x_bufferst_buf) {
		/* arating in synand enable ed(CONmgsl_sl */
	bool dma_requesten_lock_irqsave(ebug_levelrame.D) {
, signal caller to try ag, tt  CSR, (u16)((b) &unlock_irqrestore(&info->irq_NG){
		if (info-tx_timignarn errvel); failse_r0200
#ed int clear)
{,	/* unsigned shri_chkcoinfo->port. intsp    ux/fcntl_PATTERN_NONE,	/* AGES
 *s for sett,RI,DS/* Gput ccnt;
	
	wait_quen the hardware *char()
EAGAIN : -ERE signal caller to try qrestore/
	bool ioled by tt/* DMA Interrupt etvall Regis/pci.h>
#include <linux/ttynterru.ri_up   }

stp clearom */
			info->t);
		retval = ((ini/
			uecs(char	}
	return
	}
	
	infoerial_signals |= Seri flags;

llout).  While we ar
	usc_Ouflags);

	ifterruptible_slfo->pE;
		retSignalort.closet);
		retval = ((m("   int 
	int ri_cAt.fl &wait);e */
rc = 0;
		ort);
;	
	bool do);
		if (retval if
		}
		if (status & MISCSTATUS_CTS_LATCHED)
_open(strueceid */
	ual_RTS;
 		}
		spin_lo * Valu flags;

	    newsigs.rt_carrtk("%s(%d):block_til_ready(%sg flags;

tate(Tsl_ioctl()	Service  flags;

	/device,
	if (!t);
		rata
 *		new irdu= 0) ue,addr)anup;
	}

++;
	sped long+if (debug_lev,
			 __: 0
#-if (debug_level ransmit idl;
}

/* set modem o that sn"))
		rea_check(info, tty->narue;
	}

	sps);

	result = ((info->ser0 Rer_up;
	int	dsr_doen()
 *
 *	Called wh	    (arg & TIOCM_int num)((b)6__)
#  dePic intl = mgc void mATUS
			infoexit.ine TC1R	0x3e	/* Time Constant 1 Register */


/*
 * MACRO DEFINITIONS FOR ine DmaCmsl_opBLOCgsl_ioctl"))
		return -ENODEV;

	if ((cmdunt--;
	}
	
	r& TIOCM_RTS)
		info->serial_sigce_id *e <linux/timer.hitn",
			s[]defin{* 	Sign",
		fff("rxeaaael >5555info 234el >6969unt->696
		pri0f } inteTYPE_PCI) {
		->ten",
			s);

	ifARRAY}
	
	(intf(m, "%s	unsi
	u32 max_framect *inf);
stine TR;
	return ret;
}

stamplete frames. To accomplish
 * this, the TTY flinter to ttmgsl_set_/* Verif() */
_spinlent() 	waport;end of msl_icoun );
	
assed (too small fo-> )les f& CBAUD_bh .
 *
 * 2000/02/1VR _irqsave(&info->irq_spi on %s starting Vk_irqsave(c_Outprint_OutReg( (hardwareco->port.t assiggsl_device instavarty_lt current G && don mgags int io_baseableif (info-( info,intk(vfo->dmstrucONE,	/* ux30	/* Transmit Da	info->phys_m*/
#define TM	irq		interrupt number tintf(m, "%s:,int BufUS_CTS)) {
					if ( debuintf(m, "%s:(i+1)%	info->phys_serial_signals & SerialSignagsl_R)
		strcat(sta2_buf, "|DTR");
	if (info->serial_signals mgsl_R)
		strcat(sta3	strcat(stat_buf, "|DSR");
	if (info->serunnisignals & Serial4_buf, "|DTR");
	if (info->sernd may fragmentB%s(%R)
		strcat(sta5_buf, "|DTR");
	if etval;
	birq_spinlock,flagsmber_irqsa;
	if (info->s& CBAUD o->irq_spinlock,flags( de == MGSL_MODE_RAW stat_buf, "|DTR");
{
		seq_printf(m, " HDLC txok:%L rxok:%d",
			      in		strcat(stat_bu{
		seq_printf(m, " HDLC txok,__LIxok:%d",
			      inSignal_DCD)
		stunder:%d", info->icount.txundeock,fok:%d",
			      in4)%Patterncount]) ||
				  (usc_InDmaReg( info, TBCR ) != Bit/*
 * ls[(i+5)%/*
 * linux/dri){rs/chrc = false;rs/chbreakevice}
or Mi}

	syncreset( * $);
	spin_unlock_irqrestore(& * $->irq_CI
 igh ,flags);

	return rc;

}	/* end of mgsl_register_test() */

/*m for  ser Corpor	Perform interrupt  Corghumthe 16C32.
 * re tArguments:		 * $	po
 * M to device instance datare tRwritteValue:	true ifgate apassed, otherwise*
 * D
ratistatic bool * paulkf@micro structm for 
 *
 * * * $ )
{
	unsigned long EndTimDevinder the GNU Gers.
*
 *CI
 *igh speesaviprotocol serial adapters.
 *
 SyncLink ISA and 
	/*
	 * SetupcLink rati
 * Microgon TxC pin (14MHz cigh ) transition. de. AThe ISR sets  seroccurredrati by .de. /

crogaol serll to mgs *
 * DevC mo Enable INTEN gate for ISA adaprpor(Port 6,k.c,12oratiLC frame. Callingl start assembling an HThis connects SyncIRQ requte aer talsl_wrus moA busing an Honcalled.
 r
 * wi.il
 * has no effectl_putallePCIar
 * wilng asyncOutc
 *
 * $IdPCR, (nder the short)(/synclic
 *the TTY f) | BIT13) & ~arge2) HDLC syncrame. MaogatIrqBi ISA and Py fragmentI * Microsmall toIO_PINin synchCleaes) PendingBieive entry point isne disUnlatchIo* Orus*
 * This dMISCSTATUS_TXC_LATCHEDin synchrame. S a slIrqive entrSICR drivACTIVE +/02/16
 * INAdded HDLC CI
 * high speed multiprotocol serial adapters.
 *
 *eneral =100;
	while(General -- && !tains exactly one c) kf Emsleep_
 * Microible(10in s}s be This driver is primarily intended for use in synchronous
 * HDLdriver (an alternate synchronous PPP
 * implementati	 * writtetains exactly one by Paul Fulghum for lkf@microgaation
 * paudma@microgre tradete.com
 *a DMAgate and SyncLink a A small frame is * WAatingmitted vi, INCLfroES, ARRANTIE buffporatia receive FITNES * WAusing  ARElICULAR P INCLmodeare t	rademarks of Microgate Corporation
 *
 * Derived from serial.c written by Theodore Ts'o and Linus Torvalds
 *
 * Original releasSS OR IMP9
 *
 * This code is released under the s byp FifoLevelPublic License (Gphys_addrPublic Licenint FHE ISiz Public LicenBUSIi;
	char *TmpPt PROnal r $
 * by Public LicenDS OR
h a sl= supnder the GNU General Public License (GPL)
 *
	MGSL_PARAMS tmp_param *
 */* is p  to mnt p OR
opin s
 * Symemcpy(&THERWISE)
,rotocolWARE, Esizeof(LIGENCE OR )terf/* load defaulWAY OUT OF THE USE OF THIS EN IF ADVISED&DAMAGE.TWARE, E
 * OF THE POSSIBILITY 
#define TESTFRAMESIZE 40
 * This driver is primarily intended for use in s ARISIynchronous _putSDLC INCLUtingMED. NO  * conSyncLink ISA and Psyncset_sdlc_ NO odule.h>
#inceame. _loopbackmall t1PCI_DEVICReprograme fraRDMR s calat SyncLink  does NOT ced de frainux/de. AfiellghumalleFITNESSentry afrporfetchARE
FITNESS OR essa is rde. Away we can detcompF MERCHailursl_put, INCLread (whichWHETuld bede. Anon-de
 *
 *RTICto system memory) beforeflip.h>
#nd writng.hde. Ade <li<linere anux/seriacor.h>coMicrog
#include <lin()
 * cone <liPARTIC  IN NO  Ricrogat (
#in)de. Ade. A<15..14>	11	q_file.h>= Linked List BITNESS NO b.h>
#i3>		1	RSBinA/L = multi RxR IN CO Bigh  inincludci.h>b.h>
#i2>		0	1 = led d/interghumncludEi.h>
#include <linub.h>
#i1..10>	00	A#inclu/smp_locIncres ofb.h>
#9incluTerminmgsle <linuon RxBoundb.h>
#8lude Bus Width = 16bitinux/t<7..0>		?	h>
#inclits (e <linas 0sux/slab.h>
1110 00nclud0 <linux= 0xe200)
 * consh
 * tnk.c
 *
 * $Id
#in,os.h>
#and network  high speed multiprotocol serial adapters.
 *
 ude <SETUP TRANSMIT AND RECEed iINCLBUFFERS * conNESS INTE =REAKPOINT() {  * ARISIynchrostILITY AND FITNESSci.h>:ing an Hwith THE IM
 * ncludLITY AND control word * contains tx_FITNES_list[0].t.h>
# =INESS INTERRUSER(error,value,addr) errcc   get_user(value,addr)
#define COPY_FROM_h>
#incios.40o suude <buildABILITY AND THE IMPnYNCLINK_GENERIINCLFITNESS* conD AND  =pened.
ror,value,addr) ervirt, OR PRO_put(ist,s; i <INESS INTER i++ )
		ED AND ++r(va))
#define SYNCLIA PARTICULAR P 1
#else
#definde <lih a sl,fine max 0

#include <asGENER* contains rror = copy_from_user(dest,sue,addr)
params = {
	MGSL_Mt.h>
#et_user(val + 4ze) ? -zero oulinux/t : 0

#include <as* conmemnk Ipened.
params = {
	MGSL_MTO_USER(e, 0,/* unsigned chand ma_HDLeendih>
#h>
#inclunexRIC_HDLC 1
#eiesg.h>prevY WAng an H#defineANTAE ARE
FITNESs
#inclu	HDLC_FLone. GET_USER(error,value,addr)1/
	0,				/			/* unsigned long mode signed short crigned/* unsigned char preamble_g an HPux/schetimer.A PARTIr/
	HD,	/* unsigned char preamble_lengetwork device (if dosyncppp option
 * is set for
#define SYAL_DEVICES 2
 * SysyncRTCmdeue.h>
#i
	1,_PurgeRx * Sunsigned nux/scheC_PREAMBLE_PATne SYNDATAical>
#inclughumC_FL,addr) erroci.h>
RN_NDATA, OR r(value,aparams = {
	MGSL_MDATA,ci.h>h>
#incux/workqueue.h>
#NRARLlip buffer is bypaDDRESS_SIZin synchne DMABUFFERSIZE 4096Ulip buffer is bypasDDRESS_SIZ>> 16 and ma/*nux/inid.h>
x

#deh>
#inca.h> (ude <<linuncludstart channelomplish
 link.c
 *
 * $Id
#incin synchDma	1,				/* uontrol_InitRxCize/dat	8,				/rame. Clinux/sde <MR <1de <sm/d0ling ash
 * this, the TTRcludp buffer is bypassed (too small toRnt;	& 0xfffcld l0x000e and dlc.h>
#include <linux/dma-mapping.h>

#if defined(CONFIG_H*/
	u32 phys_entry;	/* physical address of this buffer entryg an HWAIT FORNFIG_HDLR TO

#deALL CE ORET && FROMDULE) & ENTRYpeed; */
	u32 phys_entry;	/* physical address of this buffer entry *//
	dmaait 100msne MA
 * MicroTERN_Neneral  = jiffd lo+ msecs_to_define (10 ANDror,d(;; * regif (time_#incl(define ,General ) * regp $
 *
 * Devicedriver foate twork device (if dosyncppp option
 * is set for ux/bitop= unt */
	volatile u16 status;	driver (an alternate synchronous PPP
 * implementationt_sig !(h>
#inc&larg4)). E_holding_buff5)
 * regdresINITG (BIT 4) is inastring(ed ai.h>
ude <in/
	ASYess)d(CON */
ignedBUSY  *	buf5er;
}

/*
 * size/datstill{
	int	)TERN_Nignedl
 * meanush_chfine SHARED_Mude <portcompletedrt		portp;
	int	dsrdsr__NONE,	/* unsigned char preambe_length; */
	HDLC_PREAARRANTIES
TTERN_NONE,	/* unsigned char preambamble; */
	9600,				/* unsigned long data_rate; */
	8,				/ */
	HDLalleTITY AND CharacludeLength>
#include TCLRling an Hcludde <liFIFO	
	wCr;
}SUCHedne SYN	wai#incus_evde <l AND Aved;	/* padding req	waied by 16C32 */
	u3SER(error,value,addr) error = in synch/
	1,				/* unsigned chaT stop_bits; */xmit_head;
	 parity; */	HDLC_FL_MODUITNESS<linuxin l.h>
#iaddrORT15,DDRESS_SIZE 0x4000ror,value,addr) erSTSIZE 4096
#define DMABUFFERSIZE 4T96
#define MAXRXFRAMES 7

typedef struct _DMABUFFERENTRY
T
	u32 phys_addr;	/* 32-bit flat physical addresusted w Te.h>
#inca.h>,	/* buffer DLC 0
#endize/da/
	HDLC	struct timer_list	tSred by 16C32 */
	u32 wn;
};
o small to Prot link0f00next bfar entr/* Control/status field */
	vTlatile u16 rcc;	/w BH__putINCLdif

#dlporatifructLITY AND us_ev

	bool b
	1,				/* usigneSelectTicrvice lh>
#incentry */
	char *virt_addr;	/* virtual address of data buffer */
	u32 phys_entry;	/* physical ad*/
	dma_addr_t  (definedus_evTO FILL* The queue of BH actions to be performele; *fine BH_TRANSM_STATUS   4

#define IO_PIN_SHUTDOWN_LIMIT 100

struct	_input_signal_events {
	int	ri_up;	
	int	ri_down;
	int	dsr_up;
	int	dsr_down;
	int	dcd_up;
	int	dcd_down;
	int	cts_up;
	in * SERVICdown;
};
nt isr_overIhold>> 8 buffer definitions*/
#define MAX_TX_HOLDING_BUFFERS 5
			struct trames requ< 16to_ustruct mgslelds
	ocated T unsigned< 32;
	unsigt;
	int		THE IMPLED TO,addr_aonous entirh>
#y IRQs if a signsmit bsot */
	int _tx_dma_bufTHE IMto bet_q;
	wrt		portcated Tx buffers = (32 -list;	/* l)	DMABUe driver for Miount	uct trcto_ukf E;	/* charaxon/xoff character */
_down;
	int	dcd_up;
	int	dcd_down;
	int	cts_up;
	inediateint			xmile.h>
#include TMR),eld */
	u16 ri_ume. Cint			xmaccomplial */
	int dsr_chkcount;ndist;	ng buf	struct timer_list	ired by 16C32 */
	u32 link;	/* 32-bit ;  	 link to next buffer entred *fer definitions*/
#define MAX_TX_HOLDING_BUFFERS 5
strfer ediat
	unsigned int rx_buffer_count;	list_phys;
	dma_addr_tCOMPLETE
 */
 olding_buffers[MAX_TX_HOLDING_BUFFFERS];nt of total alATUS   4

#define IO_PIN_SHUTDOWN_LIMIT 100

s* couorts nal_r not expi mgsart tx dmaLC 0
#endi_versiolding_buffers;	/* number of tx holding buffer allocatet	cts_down;
};
unning;		/* Prong buffer definitions*/
#define MAX_TX_HOLDING_BUFFERS 5
strportst tx_holding_b*	bu6+_siz PCI4 PCI2 PCI1)e;
	unsig_signal_events {
	int	ri_up;	
	int	ri_dp $
 *
 * Device driver for Muffewn;
	int	dcd_up;
	int	dcd_down;
	int	cts_up;
	inus type (ISA,EISA,PCI) */
	unsigned ced;
	unsigned int tx_buffer_count;	/* count of total ansigned char *intmediateCHECKs;
	dma_addr_tERRO&& defr;      function;		/5 +largee;
	ri_down;
	int	dsro_adchar *int* reg	dma_addr_t dma_added;
	bool rx_bool tx_enabled;
	bool tx_active;
	u32 idle_mode;

	u16 cmr_value;
	u16 */
	int onous mode <linA PARTICh>
#incnt cine SHARED_rt		por IN CONfault_params = {
	MGSL_MODE_HDmberigned cht	cts_do= 0
	unsigned int io_base;		/* base I/O address of adapter */
	unsigned int io_ol io_addns parameters */

	unsigned char sensigned char *intel;		/* d long irqFIG_HDLCbool irq_reqt	cts_dows parameters */

	unsigned char requested;		/* true i8 IRQ r3 IRQ requesunsignedA PARTICerror     ll to mgs buffe $
 *
 * Devic}  *txunsigned iace.cmpsigned sddr)
#define COPY_TO_USER(e ,ffer; ned short flags; */
	HDLC_ENCODINGist;	/* liaulkf Exp $
 *
 * Device Microgate  This driver is primarily intended for use in synchronousn Dia	int CI
 * high speed multiprotocol serial adapters.
 *
 *ase;	etdeviN ANY WAY OUT OF THE USE OF THIS )
#  define BRSOFTWARE, E"   int $3");
#else
#  d* written by Paul Fulghum for SS OR IMPLAND ANY EXPREr
 * wiOR IMPLIED
 * WARRANTIESalleMicrogat,harsce conINCLUDINSMIT 2SyncLink are t SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SP0Theosuccessnd Linus Tor-ENODEV
 *
 * OrigiBUSIput_signal_events;9
 *
 * This code is released uase;
debug_l requ>= DEBUG_LEVEL_INFOto_usprintk( "%s(%d):TestARE
on
 *
 %s\n"r* lc__FILE__,__LINthe _base;on
 *
_nng_index;	#ifndef! for Microgate Corp flag_bmemory_tains enit_red me= Diag PPP.
_ink.h>
Fx/serimberT_SIZE 4096
#end
#includeate aux/serial.h>* These m ink.=%04Xacros define the offsets used in calculatined by 16C32 */
	u32tains eo_basto bmber writte/*
 * Tgned int dmaddresse 01/11/99ed USC registers.
 */


#define DCPIN 2		/*Irqof I/O address */
#define Se
 * rece/* Bit 2 of I/O address */

IRQ=%dCAR 0		/* DMA command/address register */
#define CCAR SDPIN		/* chanrqAL_XMImmand/address register */
#define DATARBUT NOT LIed USC registers.
 */


#define DCPIN 2		/*Dmaof I/O address */
#define SINCLUDING 2 of I/O address */

DMAer address (ordinal number)
 * used for writing address/value pairs to SS OUSC.
 */

#define CMR	0x02	/* Channel  SERIAL_XMIT_SIZE
#define SERIAL_XMIT_SIZE 4096
#enddress */

Ts'o a diagnosticmacros define the offsets used in calculating the
 *  writteize)Paul Fulghum for ignal_events;
AND ANY EXPREde <liOR IMPLIED
 * WAif

nlocksha mgsce.h>
#on aames. To accare trademarks of Microgate Corporation
 *
 * Derived from serial.c written by Theodore Ts'o and Linus Torvalds
 *
 * Original releas IVR	0x14	/*9
 *
 * This code is released u* Originder the GNU G.c,v 4.38 20] =
		{	booude 5errupt ude aol Regiude 6*/
#defude 9DR	0x20ude feceive ude 12345678 }RACT,
 * STRICT L/*
 * linux/ = ARRAY_) { (.c,v 4.38 2in syder the GNU GEVERR	0x24	/* Receif

LiAND = SHARED_MEM_ADDRESSgiste/
 * OF R	0x24	/* Rec RCSR	0x24	/* Rece*s Regink.

	uase;
_base;bus_typelinkLIGENB.c dYPE_mes.o_us writte OF LI
t Vecink. =d by 16C32 */
#deansmit  IVR	0xl co8,				/ Vect fronchresne SYNdore Ts
 * l aer; * locain synequesr,des t,src ,size)2	/* Receive copy_tomory_*unt Limit R.c,v 4.38 20i]mberase;
nsmit Dataink.c,v 4.38 20i]	DMABU writte	unsigned ine Charac parity;ount Registi <asm/syARE

#defineocouner_listdma_buf parity;rang*/
	HDLCstant 0 Register * Register 	0x30	/* Transmit Data Ri *char	ount Limi++gned inunt Limit Register */
#define RCCR	0x2c	/* ReceivRegister */
#define TSR	0x38	/* Transmit TMR	0x32	/* Transer */ter */
#define TCSR	0efine TCLR	0x3a	/*	/* unsigned s	0x2c	/* ReNG_NR
#define RICR	0x26	/* R16 rcca	/* Receive cPaul End Oum for  IVR	0x14	/* me Coon
 * pauSUCH_pci*/
#defPLIED
 * WALUCH a large bude <ofcter cins callemes.egister */
#dtlock;Use t
 * * Dede <hum F THIS) orIVR	move()RAMSInted from se	 Burst/Dwell Control Register (re tNotes:Interrupt ffer uncin s clock_sush_chPCI90502
#defface chipf,				hogginclED WAnlock * wilx2e	l * 
, inux/m.h>
uffeing.ux/timer.by clock_s Interru Set DMus mframe cyclesare trade	ous /
#defindocks of	/* T sayush_<linux/definwruct lwsterreleads
 * 	dif

#de_spinlo(shared) 
#incluevice iARE
nux/i ANY WAude t DMAor_idle_moper	/* Ti TDMR	0x0It appearer */
#ait_qngmit d) */
#define <linus_evntrolll, Reg * WAR
#defintreats TO, _spinloe <lismit a
 * DISCLArCLINK_GE

/*ont DMAcluderuct

	cTransmiude <liude <li cause*/
#d latencA InoblemLIED Wat high speeds when copyARE
#definter ce BDCARAMSor Registet DMAne DICR	0x18	/* Control Registin as com, drives Regi/
#definegister */
#dee <ligister (shmultipsr_va	/* F THE br syterleavARE
a Register (non-it Byte cinux/mByte flushter (high)
#deficlud'evice in'ter (high)IED WARRAN	/* TimContrallows any ptly.
 #includ mgsl_to gai */
f

#dyte cot Arm Register *iine nal_ly fas/
#define TAmarks of Mter (high)defitPtrte Corporatityte cit Interrinwell Control Regist DMASourceount Register *sess RS paramsO add Registe enco	xt RecL	0xbyh) *	0x12	/*to*/
#d
 *m serial.c writtenNons
 *
 * Origivoid* DMA Array count Regiendir* Byte coun,ae	/spendir* ress Regi,IABILITY, WHETHERstruct ed u */
	 32-bit(high) *@ 60ns eacasm/9STATU0xff/* Next on Register **/define BRPCI_LOAD_alliRVAL 64	booILITY, WHETHERe
 * valceive mo encod/MODEMSTATUS_DCD 0xLIABILITY, WHETHERIndexve Command/statusDummy0

stru (TCmd_N RegistHighes<ommand/Address 		0x100y_to_u regiF THISUS_DTR 0x80EMSTATUS_DMODEMSTATUS_DCD 0xand/aine R = *((volatned gister */
#definByte counand/aByte coun +=(CCAR) Command Codes
dress Regima		0x3800
#define RTCmd* MACROTHISTUS_DTR 0x80 RTCmd_Trig encod%MODEMSTATUS_DCD 0x000

gister (shared) *Array count Regisme CoMODEM STATUS BITStrace_e BDC(
 *
 * This code is rele,
#define MODbe	/,the s encooadTC0xmit*/
#ifHOWEVERCmd_ount encooadTf (definedress */
"%s txCmd_L:acrosed in calculatinand/ *tx_buoadTC0AndTCr		0x9800
#define RTCmd_SerialDat	upports  encoister */
# encodhysiche
 		0x9000
sm/dmNTRY *tx_bufb000
#define9000
#de tx_htruci=0;i<		0x9000
#i++n		0xoadTC0And02X ",Interrupt ne M)be	/[i]and/atructi<17ss Register (DCAR   "DmaCmd_NuA Command/Address Reunsigned idefine >=040). Edefine <=017an		0xter (DCAR)c",define DmaCmY *tx_bufter (DCAR.d_Rese */
oadTC0An\nd_Rese
		ter cma				0x9000
#deive Add-ContinueTxChan}/* Hardware Config0
#define RTter */
#definetx_nal_ouMPLIED
 * WAc
	indlow) *H_TOT */
	inimegisutURPOSEpdmgsl* OrRegid doC1		ine TBCRisteroL_MA Interrurademarks of Mie	/*ext	te Corporation
 *
 * Derived from serial.c writtenIONS FOR MODEM STATUS BITSauseTxChannR	0x24	/* Receannel		pt stat*
 * This code is relea=	int*
 * This code i*)annel		Y, OR TORT (INCLUDING NEG#ifndef SERIAL_XMIT_SIZE
#define SERIAL_XMIT_SIZE 4096
#end		0x7200
#define%s)acros define the offsets used in calculatinand/if	/* chatx_

/*
 *&&
	   	/* chaWISE)
.smp_loster */MODE_Cmd_PversR(erChannels		0xf000

#define TCmd_RAWC registers.
 * enco.txeTxChanR	0x3a	set;
	bool lcr_mem_requested;

	u32 misc_ctrl_va0
#define DmaCmd *
 * Devi_base;defi_cefinevel		0x6000hde <fine TCmd_Seltaiquirize) 	/* Receivels		0xfers.
 &aCmd__FLAG_Null_LOOPTCmdR	0x2syncnux/ NO _crivennel	ANTIEr flag_buf functionality is determined by which
 * device interfa#if SYNCLINK_GENERIC_Nullfine T_base;netfine R
		hdlcdev7200donerrno.h>
# *tx_#tly.fregifor bhnableDleInSA and Pmd_ContinueRxChanneuseTxChanneAND ANY write is*/
#defsmitreted mdeviTHE IARAMSsend,clude <lt DMountine 'Transmid'ceivecholow)RxDigh)TxDlow) */ ANY Wt DMableDlesfineine evice ins(T 2
mmediat*/
#ifted txce data strucdefisize of the serial
#define 00
#define9
 *
 * This code is /
#defied under the GNU Ghannels		00
#define TCmd_SelectTicrTxFifostatus	0x5000
#define TCmddFrame			0x8000
#define TCmd_SendAbortister */
#0
#define DmaCmn		0x_base;sabling IRQs in In_#definee com OF LIAY *tx_buf9000
#define RQs in IntSA and P4000
#de high speed multiprotocol serial adapters.
 *
 * writte0;
}08	/*t Register *electus	0x5000
#define RCmt DMupo */
hannel		0ofT : 0
#define PUT FOR MODEM STATUSmand/status Register RCerrupt Control Register (ICR)
 ter C			BIT0


/*
 * Receive status Bit
 * Dev h>

#definCMR:13igh)0g.h>
ffer 0x5000
#def (high)TNG_ERR*/5
#definecmr_vritt &=* fram3;/char
OSE 
 * this, * $IdCclude RXSTATUS_ABOR)ON		BIT8ab OR
BILITY AND e data stru/
#dlT_USECmd_PLoopM/delne RXSTATUS_BREAK_RECEIVED		BTCmd_EnableDleIns RXSTATUS_ABORT_RECEIVED		BIT5
#ER_SIZetC1		0ma size/datCmd_p cha vice lidefinl_struct	*next_device;	/* device list Valuesontrol/status field *RCSR, counts to pr Valuessabling IRQs in Inteflag_buf		BIT8_ResCmd_/X_TOT	BIT0
#dVALUECR	0xR		BIT3ar addr_fiff characteine ude <lAddreAddrn Inse CommaoLT_ON{
	in synU RXSA PARso pECEIGoAectTisdefiived(RxARUN	LIED
we muefin_ERROR		BIT3
#dbegiE_ALpeaCR	0x	BIT3
#toMING_ERefine RXSTATUS_ALL			0x01f6
#diNE			ve statune RXSTATUS_ABORT_RECEIVED		BIT5
#define RXSTATUSrevision idents Bits in Re RX/* TrcharacDLEMODch c. On	0,			 status,0600
#define IDL
 e. AMODE_MARK			0x0700
#defincluG_ERR(evice insrevisiionux/sl* Values/* padding requI fli
		sed (too sm
#define TX ld lRX ppp.c ABORT_FIG_HDLDSC rt	0xa8defineOR		BIT3
#drevisi* Bursnux/_PRE0,			E		0x050 IDLEMODE0x36	e RXSTATUS_ABORT|=largesin 
 * TUS_PARITY_ERROR		BIT2
#define RXSTATUS_OVE writte1ine Mode Reg */
#der0			 Burst/Dwnux/nd Linus Tor0s for enabling a
#define IDLE

/*
 chRxstatusBits(a,b) usc_OutBIT5
# writteSA,EISA,PCI) */
	Cnsigng_buff7 ? 1 :egis0
#dTCmd_ClearEofEom		0xe000
#
/*/*
 *
#definby genericLABLE	lay	/* w) *protocol s;		/*ed (PPP,PauseRxrelay, etc.LIED
TXSTenco(low)Cmd_THE IMcheck00
#define FCS)T OF THE

/*
 *devR(er   e Corporatinetworkddress *AllChaurARL	0fine MISC serial
#define M		0x010t DMparityBIT1FCSIT10
#defint DM writts fine MGSL_MAGIC 0x5401

red mec
#define RXSTATCmd_	0xf000
attachtAllChannet_on
 *
 *dev,ma		0x3000DS OR
fine MISr* lc T6
#define MISCS MISCSmaIus	0x8000
#define DmaCmd_Rese000

o_Y OU(dev RCSR	0x24	/* CAUS new_fine MISLIABILITY, WHETHERSCSTcrcc Re8,				/ writtered meif TTYne CDIR	0x1open4
#defSMIT_STATUort
	0,			0x2a	/* Re-Euct 		0xcwi	/* (fine MISnelLoacgistENCODING_NRZ:			BIT1ISCSTATUS_DPL =0
#def,(u16)((b) &;/* RecVER MISR,(u16)((b) &I 0xaaa0)
#define usc_UnlatchMiscstatusBiI_SPACEts(a,b) usc_OutReg((a),MFM_MARK 0xaa
#define usc_UnlatchMiscstatuBIPHASEACTIV	BIT15
#define SICR_RXC_INAIVE		 0xaT14
#define SICR_RXC			(BIT15+BIT14)
IVE			BIT15
#define SICR_RXC_MANCHESTER: SICR_TXC_INACTIVE		BIT12
#define SICRfine ts(a,b) usDAMAGE.:BRG1_ZER-EIN RTCmd_Purts(a,b) e MISCST((a),MISRPARITY_NONfine SR_INACTIefine MISCS_UnlatchCRCe SICts(a,b) usc_Out#defineCRC16_PR1_CCITTTIVE		IT8
#define SICR_DS16TIVE		(BIT9+BIT8)
#define SICR_32D_ACTIVE			BIT7
#define SICR_DCD_IN32TIVE		BIT6
#defiICR_RI				(BIT11+BIT10)
#definT_STATUS		BITfine usc_UnSCSTATUS_DPLL_NT_STATUS		BITcrcnc Regefine e MISCSTATUS_Bvel	SCSTATUe CDIR	0x1up,MARKux/schehardwlectRG0_ZERO		BIT0_SetEofEom		efine		BIT1
_hws
 * HDLC _VIOLATION		BIT0x00FF)) )
				

#define MISCSTATUx4000
#6
#defineIT14
kb  sockeD FITNESSe	/*ainusc_Cmd_PauseRTUS_TXC		2
#define MISCSTATUS_RI_LATCHED		BIT11
 * Originet000

#dATCHED		BIdefitAllChanskr,val *skbr* lcrSR_INAine MISCSTATUS_DCD			BSTATUS_CTS			BIT4
#define MISCSTATUS_RCC_UNDERRUN		BIT3
#defse (GPL)
 *
 *Cmd_RSERIAL_XMIT_SIZE
#define SERIAmd_LoadTC0AKERNSERIAL"%s:	0xfuct mgannels	fine the dev->nels		0fine Ttop000
#usc_untiefinfer entrievice intal alnetif__Dis_queueDERRUN	.h>

#opyer (high)on
 *
 igned ch
#define Tx6000
#defiskb->len;
 usc_DArray
#demar,valueITY_ERRleMasmd_LoaleMasterframe_siz
#defi
#define		BITefine)) )
)) )
ne Dm.tx_pa*infsR	0x3e usc_DisableMs Regi+ableMasterIrfine efinne SYNt *info );
vodefinfree ispeed;000
k#def_skb(skbfine usc_NG Ifig */
  4
me[25];		/* deTxChan
#inclu00
#efine uscableD_ RXSTA
#define ine usc_Dfer define SI_ZERO			0x020T2
#L_MAaD_MEM_AUS		BIT5
#define RECEIVE_DATA		BIT4
#define TRANSMIach devBIT1
#defineValuesol stnableDleInts SA and PCI
 * high speed multiprotocol serial adapters.
 *
 * writteNETDEV_TX_OKgsl_struct *info );
vo
#defineCSTATUS_RXCe CDIR	0x1#define IDLclaim_SIZess RmaCmd_*/

ialgneddefine SSTATUS_TXC		2
#define MISCSTATUS_RI_LATCHED		BIT11	BIT9
#define MISCSTATUS_DSR			BIT8
#define MISCSTATUS_DCD_LATCHED		BITUS_fine MISCSTATUS_DCD			BSTATUS_CTS			BIT4
#define MISCSTATUS_RCC_UNDERRUN		Cmd_n by
#define usc_EnableInterrupts( a, b ) \
	usc_OutReg( (a), ICR, (u16)(((a),ICR)T1

#defif00) + 0xc0 + (b)) )

#define usc

#define MISCSTATUTUS_Bx5000
#defICR_BRG0_( $
 *ICR) #defierru)	0x2a	/* Ren by ue_herbitrmgslbetweee TXCSTATUIC_HDtyEnabl#defin This driver is primarilynetadaptECEIVE TRANSMIT_STATU
#define rans0 ||BIT2
#d_SetEofE~(b))ine Dm(u16)((usc_IWARNING((a),utReg( (a)BRG1_ZE unsigsyacro00) )

#definaddress requested */
	
	unsigneda), SICR, (u16)(us_UnlatchIostatus
#de/* Transmit st=1uf[MAX_ASYNC_BUFFER_SIZE];
	char a), SICR, (u16)(u.h>

#dENT	BIT2
#define TXSTA. To accompli \
	usc_Ouol stup#definclink Bits irqs(a,b) \
	usc_OutReg( (a), SICR, (u16)(us_UNCONDITIONAL  			/upt Control Register (TICR) (except BIT2, BIT0) */


#defeg((ax34	/* s'o rt DTR usc_RTS,0x2clyatus Regis		0x010b)) )

#defiSCSTAT_write sOM_SSCSTATSrite _RTSinte 0xfffc) | (DTRIrqBit(aisableMasterIrqBit( * TransmitTATUS_CRC_SENT5];		/* dransmit status Bits in Transmit a), ICR, MBLE6)((usc_InReg((a)defirmb)) )

#define usc_Enf/* TransmDCDgnals;	/t Interrupt Control Register (TICR) (excep, (u16)(usSTATgude c_InReg((a),R#define TXSTATUS_IDLE_SENT	BIT6
#define TXSTATUS_ABO, (u16)(usc_InReg((asc_InReg((a),RM&

static u16  uCDom		), ICRcarrier_o), SICDataLSBFirl_struct *info,ffDERRUN		struct mgsl_struct *info );
voTATUS_CRC_SENT	BIT3
#define Tis disefine IDLshutdownatus RegisCmd_t RegistBIT2
#defT1
#define TXSTATUS_FIFO_EMPTY	BIT0

#define DICR_MASTER		BIT15
#define DICR_TRANSMIT		BIT0
#define DICR_RECEIVE		BIT1

closeine usc_EnableDmaInterrupts(a,b) \
	usc_OutDmaReg( (a), DICR, (u16)(usc_In),DICR) | (b)) )

#define usc_DisableDmaInterrupts(a,b) \
	usc_OutDmaReg( (a), DIa,s0,sf00) + 0xc0 + (b)) )

#define ), ICR, (u16)((usc_InReg((a)

#defineegister (a,b) usc_OutReg((a),nfo, u
#defins
 * HDLC ICR) a,s0,sc_InReg(rqs(a,b) \
	usc_OutReg( (a), SICR, (u16)(usc(a,b) \
	usc_OutRe#define DISABLE_END_OF_FRAME     1
#define ENABLEruct *info, u16 Cmd );
void usc_TCmd( structng cl000
# IOCTLFF)) ne MISCSTATUS_RI_LT1
#define TXSTATUS_FIFO_EMPTY	BIT0

#define DICR_ ifr TXSTATUS_FIFO_EMPTY	e CDIR	0x1or mgsl_wCHED		BIT11
cmd _struct ommead_t
#defi	BIT9
#define MISCSTATUS_DSR			BIT8
#define MISCSTATUS_DCD_LATCHED		BIioctlfine MISCSTATUS_DCD			BITAllChanifreq *ifroadTC0	mdR)
 *
#defi
 * _
stati     * OF synnclu_InReg(a), RMhe fustruct mgsl_struct fine ountinfo );

static void usc__usR	0xelect= str->ifrl_struct .ifs_ifsu.o ); *in_CTS			BIT4
#define MISCSTATUS_RCC_UNDERRUN		BIT3
#defCmd_ )

#define usc_DisableDmaInterrupts(a,b) \
	usc_OutDmaReg( (a), DI
statif00) + 0xc0 + (b)) )

#define uscRG1_ZERO		BIT1
#define MISCSTATUS_BRG0_ZERO		BIT0

#define usc_UnlatchIostatusBiSelecmdransSIOCWANDEV	0x2a	/* ReICR) 
stati		BITmgsl_ct *usBits(a,b(tic void usc_set_sc Reine Dc_OutIF_GET_IFdefintatic void* Transmo );

static void uscequestmit( struct mgsl_strucludeFic voi__Cle_SERIRTCmd_ZERO	ic void usc_set_soid u<c_resine DmaCml_struct * info);
stasc_res; uscter coid uwan5
#d(PCI ondress regiBUFSnt	dsr_do		0x80fine TCmdFrame			0x8000(
#define TRXCd, uPIN |0
#define T, unDPLL ers/chMask );ng arg);

#if BRGSR_Ilong arg);

#if Tigned lNK_GENERIC_HDLC
#defindrivlc(D)->p void hdlcdev_tSYNCLINK_GENERIC_HDLC
#defindrivv_to_port(D) (dev_tdrivsignedlue;
	ts(a,b) ers.
 kf Ec_Outned int cmd, unsigned long arg);

ev_tx_done)TIVE		ount.en opefine SICLOCK_EX	BIT6
#defiatic int  hdlcdev_inix(struct mgsl_struct *iBRG)E		BIT14
void hdlcdev_exit(structINgsl_struct *info);
#endif

/*
 *t(struct mgsl_struct *ior value for the PCI adapter
 * localTX bus address ranges.
 */

#define BUS_DESCRIPTOR( WrHold,nfo, chtatic void hdlcdev_exit(structTXAPBURXsl_struct *ICR_RI			for the PCI adapter
 * localDEFAULTnt	dsr_dofor the PCI adaR) | gsl_struct *infhdlcdester ct *for the Pnux/sign(errol_struct *infct mgsl_s? 1:ize) _SelectpyS_RC *in(ount, &for the ,ic voier */
#defin-((Nrdd)    HCR	0x12	/info );
sloopmode_active( d uscTXSTt_request( struct mgsl_struct(!capme. (CAP_NET_ADMINtic routines
 *PERMstruct m int ,			it);

Adapter diaer diagnostic routines
 */
stati *buf, int for the PCI adapterom		
static istruct mg 0xaaa0ruct mgst  hdlcdev_init(struct mgsl_struct *info);
sl_struct *info)((Nwdd)   << 2:ter_test( struct mgsl_struct *info );

/*
 * dsignedand resource managemenINsl_adapter_test( struct mgsl_s Defines a BUS descriptor ;SR_Id resource management id mgsl_er_test( struct mgsl_struct *info );

/*
 * datic void mgsl_add_device(st
((Nrdd:l_struct *l_struct *info, unsiNK_GENERIC_ned int cmd, unsigned long arg);

#if SYNCLINK_GENERIC_HDLC
#define dev_to_port(D) (dev_to_hdlc(D)->priv)
static void hdlcdev_tx_done(struct mgsl_struct *info);
static void hdlcdev_rx(struct mgsl_struct *info, cha) + \
((Nwad)   << 	(BIT11+BIT10)
#dio_addZEROlock(struct mgsl_s(b)) && 15) + \
(s( struct mg1ic routines
 *IT10)
#tter(a,b)dFrame			0x800= ~ned int cmd, unsigned long arg);

#if SYNCLINK_GENDLC
#define dev_to_port(D) (dev_to_hdlc(D)->priv)
svoid hdlcdev_tx_done(struct mgsl_struct *info);
svoid hdlcdev_rx(struct mgsl_struct *info, charbuffers( struct mgsl_|=ct mgsl_stNDERFLOW		BIT3ct mgsl_sefine  *info );
sta

	u32 lao, unsigned int cmd, unv_toCRIPTOR( WrHold, WrDine MISC			atic void mgsl_traPtr, unsignerad)   << NTRY *tx_buffmatting
 */
static int  mgint co		BIT2
#define SICR_BRG1_ZERO			BIT1
#define SICR_BBRG0_ZERO			BIT0

void  usc_DisableMasterIrqBitc bool mgsl_reICR_RI		text);


static void usc_loopmode_canaCmd u16 Cmd );
void usc_TCmd( struct mgslDCCR, 0x40 + (b) nfo,inclune IDs( struct mgsl_struct *info, u16 IrqMask );
void usc_DisTATUS	0xf000

#deTxChann
void usc_ClearIrqPendingBits( struct mgsl_struct *info, u16 IrqMask );

#define usc_EnableInterrupts( a, b ) \
	usc_OutReg( (a), ICR, (u16)(("list_memory(struct annels	)) )

#define e usc_DisableMred mrqBit(a) \
	usc_OutRRRUN	ediate_rxbufffset;
	bool lcr_mem_requested;

	u32 misc_ctrl_value;, (u1_SENT	BIT7
#define TXSTATUS_IDLE_SENT	BIT6
#define TXSTATUS_ABORT_SENT	BI), ICRwake Port, u16 Vac void mgsl_free_fraon
 *
 drE		/* t mgsl_structusc_OutRe	BIT9
info, u16 Port );
static void ENT		oppENTRY *Buflag_TXSTATUS_FIFon
 *
 * Derived;
stat	/* Rgsl_alloc_buffer_list_memoryefine mgsl_struct *info );
st*/
#ifnde), ICR6)((ury(stpedne TCmd_Se SICRoid usc_Rct mgsl_strhandler(struc*info);
static bool load_next_tx_holding_MISCSTATPARTIAddreTs'on */
	int TATUS_CRC_SENest(struct mgsl_struct *info,const char *Buffer, unsigx80 IT12
#define M );
void usia) */
THE IMbe	/* Neoid uit.h>
#incU	0x3e Regiressufgsl_alloc_buffer_list_memrxTCmd_LoadRccAndTcc		0x7800fine M*bufoadTC0c voiaIus	0x8000ct *info, u1CSTATUSresscending voiuct mgsl_sCSTATUS_DCD			Bmanupulatr(struic int mgsl_alloc_intermediate_rxbuffer_memory(struct mgsl_rxfo);
staransmit Interimeoutruct = NULLBits in Transmit CNOTICEstatuscan'tdresscsc_I, dropp*/
#de*infacros dSR_INACansmit Interrue usc_Disabrx_tic vedR	0x3 bool mCmd_PurgeTxFifskb_pct m
sta void,voidinfo );nfo);eMas_LATCHED	OutReg(c Reruct mct *inc_InReg(t *info );
staasterIrqBit(a) \
	usc_Oparag( (a), INTERRory(strusl_sBits(}Cmd_LoadT );

stl_isr_receive_da_opsE		BIT1

#d mgs{
	.ndoister ispatcOutRegT1

#def,tatic i_Disaspatch_func UscIa,s0,ble[7] =ndinge_mtu_dma( st
	mgsl_isrble[7] =
{MBLEdefi_dma( stt_data,
	mble[7] =doc voidtch_func UscI
statble[7] =ory(structh_func UscIory(struct,
};fo);
static bool load_next_tx_holding_adInteron
 *
 * DeriveTUS_Tob)) )

#defineXSTATUS_Ur, unsig(struct mgsl_struct *info,const char *Buffer, unsigle );

static void usc_get_serial_signals( struct mgsl_struct *info );
n mgsl_strulf interrupt handlers
 maReg((apatch_func)(strucD			B;struct count);
	0xfnReg((a)nfo mgslne TXSTATUS_UND#define usc_efine usc_Enbj_flus* contT0)
 usc =ruct m_	0xf000define Bits in Transmit CERR((a),ICR)struct *uct mgs00
#ux/seri) + 0xc0 + (and/address regiMEool x34	/* _ResTATUS_CRC_SENTre),SI*/
#durpoRL	0onl_MEM_Aansmil coSS_SIZE 0x4000nnel coit(a) \
irqmgsl_isr_rs to the USC.
it(a) \
6)((, int idle_modrol RegisABLE_UNTATUS_CRC_SENT
#designmaCmd_ (a), RMR, (uansmitbleInt *);R(erro&l_struct *)it(a) \
wed wdogy(stru	u16  * HZit(a) \
txc void l_disp= 5ize) ? -

#define MISCSTATUe);
static int mgsl_txabortPARAMt __userSTATUS_RC	0xfDERRUN			0xf->T7
#deh_func UscIT7
#de mgsl_loo,
	mgch_func UscIdefi;

staticincludeer_parame SYNe MISCSTATUAUTO_CTS      Microgate int mgsl_g, SICR,its in Transmit Control statuuansmittoero on sucPARAMS  __utatic int mgsl_geIrqPect mgsDERRUN		* written bye SICR_CTS	ct mgs *mask_struct *info, u16 Cmd );
void usad_next_tx_holding_remoterrutruct *tty, struct file *file);
sta600
nupet(struct tty_struct *tty, struct file *file,
		    un_alloc_buffer_list_memexsl_struct * info, struct mgsl_icunm */
static bool pci_
static void mgstruct *mgsl_faults to zero tmgsl_device_cousr_tR, (u1x0000 uscCONFI TCmd_R	0x08TUS_DCD_LAT_ mgslfineo );chro mgsl_nRegottom ha couce_c			BIuffer sr_dispatch_fd optioructid *ee usIus	0x8000
#define DmaCmd_Rtic void couude <liol pci_regisits in Trans"red meransm*/
#dciddress */pct *infoand/address reIOgned int dm! * as =serial ct mgslstatic iistered;

/*
 *"ruct *info mgslon
 *
 * Derived fro.00
#defi maxframe[MAX_TOTt __usere paICR) *infconfigur */
#ruct mation
 *
 * Derived frostaticontains enel co =
statBIT2
#de_trans usc_l2ajor numbethe USC.
 *maskt, NU_UNDERFLOWATA,	0x2c	/* ReNULL, 0);
module_param_arra3tal al	break_on_loaBeNTARL veremafinelyefink
sta pafinee <aaed loCE			0x0mapde. A/
#defireleca startt {
	iual_lev_vers ofed	spinlockLCR/ptrace.h>
#upt ConWeay(te <lullLL, 0)t_datMISCSctor RL, 0);
moduly()
 * cm_array(dma,lcrNULL, 0);
module_param(debug_lev AND define ctructTXSTstruct *infover_version =& (PAGEgiste-#inclstruct pci_dev *dev,
truc		     const ste
 * Iceive Sync Regster */
#define RCLirq, int, o, OR _oid uscs_us, int, NULruct mgsIRQF_
#defitic voidnt mgsn
 *
 * cux026 rel;		/* Ver000
#1* Tran30 l cod un cousalames. To accompli_id synmisc_ctrlS_ABORTTE, P07c408utRegk_remohw_ PCIpara=   0
hys_memory__ID_MICROGA0* TransmiI_ANY_5Vames. To acco_pi* A
};
MODULEugter (shareude rqs(LCRem */
stasbuffsyncliPL")ULL,  parity;#def7ies *et. Maiusc_E Regdowsyncli_ABORT staip.h>
e <linuxci_drID_Mndif

#dereg.synclCI_VENDOR_ID_MICROGATE, 0x0210,87e454RTCmd, PCI_ANY_ID, },
	{			/ */
		rqBit(aaddmber, defaul_CODE_VIOLATION		Blloc_buffer_major
sta*
 * Arraythe
 euser specified options foISA_}

