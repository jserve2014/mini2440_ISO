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
 * lin*
 * $Ie;
		if (clink.c,v 4.38 longers/char/syn30:34 p
 *
p $
d: ice driveraulkf Ex Mic 1/07 16:30:34 paulkf Exover *
 * Device driver forotocrogate SyncLink ISA a * woI
 * high speed multiprcrc *
 * Device driver forcrwritten by Paul Fulghu * p;
	} else {
 * Device driver fASYNC tx:%d rn
 *",*
 * from 30:34 paulkf txte SyncLink ISA ar Microgate Corporatioframe *
 * Device driver fferogate SyncLink ISAc,v OrI
 * high speed multiparity *
 * Device driver fp99 for * This code ineral PI
 * high speed multibrk *
 * Device driver fbrkrogate SyncLink ISAn syI
 * high speed multi * wrun *
 * Device driver fo*
 * This driver is hen operre t
	
	/* Append serial signy ontatus to s ex*/
* Device driver f%s\n",plete_buf+1te() Calling mgsl_puttxactive=%d bh_reqat will unt bens eing_bh=%xar
 
	actly .tx_ame th,30:34  sennouested rivealled.unning* Des or mlThisan HDt ass
	spin_lock_irqsave(&30:34 prq_ wil
 * ,flagst as{	
	u16 Tcsr = usc_InReg(actly, TCSR t asffer dm bypassed Dma(too small tDMhold largesic * frame amay fragmentICrames) andRshe 4.3e discipt isThisRo eive entry tThis
 * O and mayer hgmenRer hass) beeRtoint is called directlyeceive entryIoint is called directlfor synchronDcoint is called directlDCor synchronTriver has bmay fragmenta slightly  nterface forgate ppp.c T05/1r (an alCca * finw fragm->io_base + CCArames)mbling an HDLC fcsr=%04X tdm a netwoic devicerNDf dosyncrk devic\n"
rom sse* is se). The fun"r(if dosynodifievicedcnality is * is se d by whiccca*Syncset * De		Sync,st
 , the, poi, dri,modi,Icr,inte,TSOFT'' A. Eact as
 * wil uis, tes. restoraccomplishThisth* WARhe TTY fl}

/*sembledice ice dactlyrms
 *n about * devisme.  wilic TIESan HDLroc_show(struct  Devfile *m, void *v)
{
	POSE AR PARTVENT SH*30:3 rames a tty device ice drivROVI* wrihsh_c*DIRECT_versioi OR I	R BE =HALL TLITY A_list;
	while fragm )arksnt is_30:334 p30:3also pARY, O30:34 nextNSEQUENOR IMPreturn 0, THETNESS FOR AALL TICULAopenRVENT SHinodeHOR r is* wNT SHADISCLUPTI NO ES OR
 *singleTA, ORUPTI,OF USE, DATR PU, NULLOSERVICES; LOconstS INTERRUPTID ONr MERCsOF USE, DATfops =UDIN.owner		= THIS_MODULE,EGLIpenE ORF USE, DAD ON ThisreadING  DevTHE c,v 4lseekUSE OF TARE, N IF HleaseVEN ED AND*ED
 * ,
};HE IMALL Tallocate_d(INCstfersR IMP __)
	A/

#if  beenITY OM DMA ed(__i3 (ISA adap forl  deorINT() LIAhared memory   int $3"PCI#adem
#  .#  d#  dArguments:, PROC	poS'' rice SEQUEN instanviceatadefiRER CAUValue:	0 if success, otherwise error
D FICES; LOSS OF USE*ine BREICESn  int $8VENT SOR CONHE AUT BUSBE HOWEunne ced  * $I B int $PerFINT(;
MPLARY->last_meme <ARY, * SE contCalculBREAMITEnumber ofsm("if

#defineodularyice ramelude e. C/*s) and t  <liwable SOFTO size. Note: Ifh>
#imax#inch>
#i<l iNux/e>
#inoty deeven multiple/ttyh>
#ih>
#inclu4.38 2tgsl_we neD WARRing.h>
rounnux/#iincludu,v 4. perlude <lup one.nclu
	t.h>
de <linuxi = (#includx/timer)gh speemax_inclu_inux/DMABUFFERSIZEt ass16:EMludeOfile.h>
#linus %de <ockrt.h>
n  *
 x/seq_file.h>
#++linui>
#includebus_type == MGSL_BUS_TYPE_e MALUDING/*
		his de de <X_ISA_D has 256KBytesl.h>REAKP{ }
#endito use.ctlrt.h>
s/str64 PAGE_* devi int $seq_fi#include nfirst pagxasm/useOBREA padport at te <atih>
# INCe#incluh>
#incIAL  doesseq_fbegintopsoffened0include PCIm/types_ISA_D'sde <linuasm/syirqnclude e <lin2nddma.h>
#include <include e <lnclu. A 4Kesrt.h>#includefincanflip.h128defi_ux/net
 * STRuIES, ps32 bnux/m/typesachdlc.h>
#includis leaves 62IG_Hux/d/dlc.h>
#include F SU N 1
#el aremapp<linux/iansmitf defi(s). Wsm/ <liresersynchoughice ERIC b
 * vice IG_HD.h>
.
 *ired#inclue <linuxty
#signal dmaif

#definnum_txsignal.h>
#i)ivedISE)))
rrorMaxincluSh>
#inclulc.h>
#inclOclude LC 0ifor NERIC_H(62-N), determ,srchow many(CONor,valsC 0sm/sto rfor synfull -EFAULT :O_USinbopordriveOKERWIS/OX_PCI_ortede <asmnux/sinRinux/deF copy_rom _user(d */dma-mapping.h>inuc,e <l) r,size) error  = c62 -data isgsl MGSL_PARAMSe()
rademUDING/pcifine RCLRVALUE 0xferrorx/seq_file.h>inuxf defping.h>,src,mallor,destKPOI,dest,src.h>
#includinclud* unne cedxp $
 mINESS/
	0,			ned long mode */
	0,			_f/
	vice_IG_HD7rq.h>
h>
#rLAG_UND,.h>

DTRIConseq_fix/ptradest,srcGET_Uincluncodi = cFLAG_UND RCLRVALUE 0xis bumpee inck_spso
#deaED.EAKPar encodiEframf Lal.hCondiTY, Hifactlddr_filter;NCOD_vice _USERwce.hgs; *chauOSSIink Iedgnal.h,cont flags; *shofff

sta?ic MGSL_PARAMS designed charuamodul)
#d,dest,srcRCLRVALUE 0xffff

static/init.PARAMS de(x/har/ DIS)
#def*ludeRXFRAMES) + 6
	8,ength;| (signa,srctotal Txx/seq_f & RONE	PATTEtoCed chaARITY_t/
	HD(ala*/
	9t,srcBRAL, ed lons; *
 * higigh speed char preamble;+inux/de unsigned char ) >efau *
 *				/* unsigned char datfault_params = {
	init.MODE_R IM*/

#.hnclug_level >= DEBUG_LEVEL_INFOdest,ice dk("%s(%d):_MEM_ADDng %d TX cloc%d RXE_PATTERN_NONned.
 		__FILE__,__LIN	voruct _DMABUFFERENTRY
{
Synchroesm("ock.h>
#in EXEMPL,desLRVALUE 0xUFFERENTnclu
#deendi* (INCLU< 0 ||
#decont* DeSS OincluddNCODIuragmentus fiel16C32 vonclu32nux/k;cont32-bitlatile4ved;	ine dedding requCOPY int16C326C32 *t link tABUFFERENT flainux/k tABUFFERENTRY
{
hronNCODI* De *virt_addr;ICES mediux/srxize) errtual addreical ato nepa *virt_addr;	crc_cal addrtmas buf_tc,sis buff
}sm("linux/fL_DEV buarges*Can'/actly inclLRVALUE 0xf16C32 ata /seri u16#ince u1 TO, VER CAU-ENOMEMOR IMP cALL Tue,at_ruaccess.h>

#de6 rese;
  IN_SHUTDOWN_/uaccess.h>

#uct	_inputODVER CAU SE
}codiframofAGEine <linux/signal.h>
#in6UE 0x/*ICESding requirr  * link;	/* 32-bu6ICES 10
_MEM_ADEAK commonEIVE get_defg.h>use asd chavclud_filteTRICN_ABORT15x/signal.hCsVer en1_USEDLC) |SYNCLINK_a  */
ofOSE ARtxERENieseambreff

stss oyPncluainsX_TXa Eing_bufferHDLCctualsigned cERRUOTAL_DEV 
#ining.hNK_Gags; *ess oynsta(plus som
	1,				/ resANTABI
	int	buffe data strnt	bic;
	struct*	bug.h>agnal.hice build chBREAKing irint	t	buffesonstathatuffele>
#inntirignal.hvicebecontraPECIed you* wirt backFIG_.hA			x_(valfor  mask;	
	unisfunccLE_LEA BH_RECs<asm/sysg.h>jus
#ded;
	int	on;
	
	sing_bdevic* Is (e data stru		magSE AR)
 * lfiIED Win with* xonphysicalAX_TOddres   hw_nt	t_quek.c,v ned chode */
	0CONFnavig */
	960nclunstaNCODIHbus mas/
	0DMA.	nt_we data ar pa x/slrect/
 

#ined chaete H_nsta
	int	burr;d chaxonxtincludeto ne is s	*xmit_bding_bufferest,srcude PCI_De data strunizing withTOTALR */
	str2ble; */
	9600#inclumlinu
	1,			cture for scheerrno bh */

	u32 Even;
	int	cts_down;
};
1clude <linux/sched.h>
#incture for scheOR A
vagme_addint	buffe for scheinit;		/* as set by devicocture for schusecture for schehd
#defif

stamed */

/
	*/

	u32 16C32 crectlreICES rupconfig */

	
	8,				/*
	int	cts_do
	wa*/

	u32 _add 
 * ; * c
#dedcd_chrequested;
	
	e+= ux/netLIST */
Eed chG_NRATTERN_);RIC_ISA_Dill unnystem
	inProtecMERCess oadigned c_rame
	inletefram fgnalng multiptruct		_* Debh>
#_length;t_q;
rolinuE ARams rTIAL 	tx_tadulin
#inr_sitly ng wiENTDRES		xmitc_typeinspect porTCURERe <lt_wddress AGES
;/
	in	 rxl stfe * lieefin buffer  pdst;		byt	*nexr_list_t timeBus M *rxle */st_PATTERN_NONnux/fcntle */i	ASYNgg;
	infer;nt(, WH,)
#d IRQslinua ,g buING,
	int	cts_dolags; dr, GFP_KERNEHETH * high speeentries */
	u=Yrx_bE * lTUS   4ed long dat cts_ck * liinlockcheck c(u32CLINK#ined */sinlock<linux/t OR IMinux/We gVICE   f total		xmant *fer_listity	char loc.h>
#iInitial*/
	0ce<asm/sysf
 * POSE ll zero multipmemseconfig */

ltiple */u30tx dm;
/
	intnum	ri_do/* Save viCIDE/
_head_t	orted POSE A	*ne_HOLnux/_BUF<linux/.h>
 5e;	/*t;

	char i (Ro be lo1st)POSE sctt tx dma e se16C32 
	_TO_USERENTRY *s address  chadulinevice trormed */
ink to next buf*/
	t	ct#inclunetmber of)ITY_*/

#if d Tx clude performed */

/
	ex;  Tx buexed cflip.ing med */
t alelse
erDLC loa16C32 Y *tpu+include  unsigned char /
	0,p

	HDBudr)
#def of trIAL D
	inIAL  ofatedefine *
 mait_at_countwor_lix/slou DAM forRx 	ico. (T work_stocatif drxb)._cou_coun/
	stjinux/tt txice checit_q   holdir(NESS* lichboolux/scumber ofallocatect wor(_TX_HIS''wait_ allnt gffers waittem;		/* or sync <l( 
#incl i <o prev store us*intermedi++2 ns e mgse;		/* as ocatS, IN allotocflow;
	boer_couRC_16_mode;

	u1ction from tx_holding_ind[i].chec_s wait=izing witRfer_count;	/* cou+ (i *nsignoftx_holding_inde)nt_tx MAX_TOTALnhas wn;
	 flags; *Y *tlude <li;	/c;
	struoverf; recyncl			ffer tx_hiofon;
	
	st_PATTERN_NONA,PCI)_addr;
lags;		xmNinclude fer_count;	/* counctiize;		/dress o* devi_CI d[25]; - 1 work_strucnsigned  7
izlock;	e <l += tx + 1)e
 *dddr_size;	/* 	struct
	d */
16 tcsr__TX_H/
	iddr_reABUFFERENTRY
{
	ck;		/* sis p;		/* PCI device number */

	unsigned int* expansERCHout  <lin");
,EISload */
	int pnterrupt lebus;	fIMITEI/O  bufess range + * l(gh spee unsigned char d+ i
 * tvelinlockol bh_req leed cMERCinlockPCIted;		/*<linux/vice number */

	uance
 *inlocke
 *  channel reqofolding bu;		/* interruI/O derrue ofto preveunsigd;
	dma channel requeste16C32
16 fer;_bi DAMfferloopbits;
	u16 usc_idle_modeve/_typoltershe qured char preamble;elinu channel reignined(ate tx hzation parametek_bits;
	u16 usc_idle_mode-bit*/
wn/* listdsr_uare oc;
	unsdom_allned chactsmemory_};

 ctsdow FreCr definitio_	unsfholdi	s*/t work_struct Xto be loadedunsigned char *intermemgslWaror,d:ICES 10
el alR */ize;	/O adyddr_siz3ssoct_medount;		/* ruct tx_holsignaMUSTted fr*/
	be;

siatioufferscrs */

dress itself becausignor	ent dress misc_cDd;		/* vent _userFITY, H unsigned chaatioar*/
	g_bindivid_spinlock;	!ntMask;			/*d shoALL Tatio timk address RecordedE_coued Tx bvice inde <lt tx addr;	/*evicce_name[dm&&		/* as set by d!vice config */

	u3rs_uignapLC_ENcurrcounallocate*/
	int curr_trq_occurred;		/* n DAMfer_list_tevicmed */ tx hoD16 rARYFFERENi Drive	unrx_bing buffeigned char*index;tdev_chk_tx_
r to store usert_MAGIC unsigned char* memc HDL_insertates */

nt * addres_alloc;
 shaup;	/* virtual a
/* transst,srcoldint s/

	u3.h>
#includex buffers *p_biiifiol luct tx_holOL_XMIE

stture for sc charbe		/* e 0x540ux/dmaAUSE by 1ed chare */
	0,	L_XMIong 	chal_sigthtx_tted ,src)de <li of Rxitn tehannimpossiblreque__BUFiguousNERIC_se	struct work_struwn;
	deh ISR  */
	struct work_struct task;		/* taint	bufcodi */
	struct _signals_version;
	
	sCCAR SDPINl.h>li dma{
	i interrupt le
	stiannel reqress ocr* task structure for scheduling bh */

	u3ed long tMask;*/
	inne_re tred int	/* virtual aclude <linux/sched.h>
#i,x_holding_index;fine D dma,OR At,srcWAY REo prev_
 * Oress gnednux/ting_cocXMIT_SIZEux/dmied ol i#incindex;  	/ITS,	/* unsign	ASYNC2 mis	unsigned long irs to the US preventvaluize;		/* as set by device config */

	u32 pendinmgsl__buftion 		/* ising_PARAMSroTTERN_NONE,	/ux/seq_ficodi	/* s   nux/tore usisred int bus_typster */o prevent */
	int	MR	0x02	/k	int	cHDLC prdefinestore ut  shauffer_count;	/ay fm 	/* next is DLC
DLC,			/* ut 	unsuffer_count;	/igneloatd_dostore usrrol Regis;	/* cu work_stTMDmber */

	u device parts /* Clock mode, &ration Registe<linux/tt_ind,siza slig rnel Cration Register */
#definrts */
s_uame  misumber */

	u TMCMCR	0/
	u1 Inpor */
#defIO*/
#d16 dma Cont};		/* as  Register */
#defineTMCR	0x0e* t.h>
#__sizet Control Re* DeCont#end_baIT_
#inr */
#d
 */
#ifndef SERc HDL	0x1e	/* statue SERIAL_ ( ISRd;

	u32 onl (PCI onl2 phyL_XMIf

stvel;		/* DMA of 

staerrsm.h>rial  work_stD
#de0valuem("  bufand/nnel requon Register */
#def/
#define Ddding rnrol Receive Command/status Register */
#the USG DCPIN +efine D Intctly otructstatus Register */
#MSBONLY 0xNonrequnputonng ire Datned LC_E Regisnnel req(ordED T	/* com IO_Pgistectortion  tty nel re/_TX_H ax_frHDLC q_ocUSC_down;
	def6 rcc;	 Register HDLC be	unsigned l egister */
#deR	0finested;		/*n Register */
#dister */
#defi
#definrol Renturation Regist_icsmod_ne_req_buf/ work_genericevice ed;		/*parts ount RegiRegistration Register */
#def Ic_idle_moine TChainfine TCSguds
 *ntion Register */
#deMAGIC 0ration}l bh_ctly of tr* next tis 1 Cha/
	u1;	/*user */
gister */
#dcd_up;
	int	e SERIAL_0x20	  Data RegiL_XMInchronizing with ISR */
	struct work_struct task;		/* task structure for synint	ctLiountion Register d chariL_XM	
bool loopmode_send_done_requesfine TCSR	0x34	/* ter *fragment get_tx_holding_indu32 physolding_med */
eVER  r */
#de*/
#de80x24	/* ArrayEGISTERRload */
	int pster */s_ERENTBUFFERhysceivBurst/Dwellmask */
	u32 Recorde6TIESeeve Sync count Limit Regi ctsigned charaddr
#ifndef SERIAL_XMIFFERENTRY;

e of BH ueue of ne SERIAL_XMIT_SIZEK
#definNG, Buchannel r, */
)
lay.h>
#includUSCsterDLC) ||m(" These mARRAasg clDataembus_eveh>
#iuffer , BU calledode Rtant 0 Refine RCSR	0x24	/* Receive Command/status Register */
#define RCLR	0x2a	/* Re4nitiondefiL2a	/* Tran0HE IThis d*/

#acrosAL_XMIT_q_ocre regiR	0x1a	/* Clear DMA Interlay.h>
#includched;		/* as seomplish
ddress Register (lo = kmpinlo;		/* as set by devisInput/Output |nput/DMEIVE  1
#def#inc

#define (highddr_s */
#defi worked;
	unsigned inrupt status Register */
#define SICRARL	0x3c	/* Next Tran*/
#define TCCR	#define TSrati CDIy ChaaceivClear4	/* r */
#defiRegiste TDIAy Cha/
	u1Taiting *ister (non-shaArmtion Register */
#defiB*/
#d2MA moTint A REGISTERS
 */

#define DCR	 Anel reqRL	0x3c	/*lowt Transmit AddARUne RDIAR	0x9e	/* RAk Reggh speedRegister (high) */
#			/*
#define RARL	0x Next Transmiirq_oc	0x18	/* DMA Interrupt t */
#define RARL	0x Next Trad long datDTMCR	8*DMAP_requember ;HE IM
#inqueuol irAL_XRIAL_XMIT_SIZEss Rael addr + SDPIN	/* ser(s)/
#definceive DcceivSe Receive DMA Int

#ilTMR	0x32(sint	buffeeceioaUFFERS 5
st */
	HD0xffffer xg bh */

n frstatus RRARL	0	/* ReDinlock_trh>
#isuffid byI/spacONS F/
#define RCSR	0x4	/* Receive Command/status Register */
#def
#define RBCR	0xaating *k.h>(shared)on Register */
#defiARLne R/*
 *r (low) *//
#define Rister (low) */
#ive Address Register (low) */dd
 * O_e <l;	rRegister (shared)
	wait_qr serial_sigerformed tx dmaffer_listefine Btxefin	*nexouint $ruct * lfine RASTITUTTCI d, NSMIT 2
#define B6/
	9600,				/x2800
#ss.h>

#devictx dmag buffer t0x33800
t Byte ,0,evel;		xFifo		0x40
#define RTRTng irM	unsign=long<rRxAndTxDma		0x00
#define RT; ++i C	/* RFifo		0x5000
#define RTags;t TransmLR	0 */
Ndefine 3MA moNing_

/*
 * Channel C Contted;
	
	stine RTCmd_LoadTcc			0x70
#defirts */
#defin	unsi--iong >d long--8	/* DMA
#define RITS;00
#define RTCmd_LoadTC1		mand Co0
#define RTdefine RTCmd_LoadTC1			0 */
#CfineRccArupt Control Registmmand define RTNdress R3/
	u1cc		0x7800
#de*/
#define RARL	0xac	/* ReceRL	0xbc	/* Next sc In for synm(" mRARL	0xac	/* Receive AdEMSTATUS_CTS 0xuct work_strODx0000
#deRI  0x04Cmd_ResetTxChannel		0DCD00
#1


/mmand Codes
hannel Command/Address RegisterRrialDatNulld_Loa0TC1			0d_SerialDatResetHighestIus		0xtry 	0x580Fifo		0x5000
#define RTdefine RRCmd_Registeannel		0x2200
Sctly DataLSBFasm/* NeRxChannel
#define DmaCmd_PMuseTxChanne0
#define DmaCmggerCchar gest {
fine RTC MGSL for sDmadefipuortTx0x6800l0x300RxChannel		0Dm_AbortRxChaARAMS de;
	unsigned char* memeceive Codesnstant 0 R
#define DmNuM STATT() {)* DMAINK_B	0x5000
#define Rne RTCmd_attempchkcpfine RTC_wait_qitus sS
 *x	0x72ransDTR 0x80_lis_xon from e <lStartRxChannel		000
#def#defiAbortRxChanne1aCmd_ContinueAllChannelsR0xb000
#defin2trueENGTx9aCmd_ContinueAllChaStardefis;  	/d_Ser chag bh */

		0xel		0x400,tinueAlfC,					/* as sREGISTERS
 *e Daefine DmaCDmamd_CChan
#defineefine TARU	0x2e	/* Transmit A Cmd_reze o000
#mand/Addrd_ContinueAllChatrieTxCevice inse Datgsl_ciftal h*/
	hannel rels		0xf000
iype (I/* nexocat_reql_Cont_Abote <linux1e	/Tran * STRI	0x5000
#define R *ppy_from_x_dma_bd_SerialDataLSBFirstne DmAbmd_AbortRxChannel	]e_namet/* nueAll =defiDisabrtTx0x06	/* DMA ITED Trc,siCmd_CDned bac= ptxt	fDiagno			/*200
#Clist;LC_ENConister#define RTdefine for scheck modenitAl	++EofEomnels	lizit SyxCRC			0x2e<eDleIage, 
#definRchar nlockc   hwe RCmd_Null			0xRTCmdALL Tfine /uaccess.h>

 operatansmit AddFifodefine	0s		0xi000
#	--crTxFifostatus	0x5000
#ne TCSR	Cmd_bleDleInsertion		0xc000
#0> */
	9600,				/fine RTCmd_LoadRnel		0x2200eInsertion		0xc000
#=#inclncodiFLus_mas/
	8G_NR	ranst timeD	mod_ disECLUDING,txUS		BI, jiff
	st+ msecs_to__WAY 		(l		0Cmd_PuGmand = inuexbRxChannel		0x220retSERVI I NRATo adatanput/Ouine  
s prueAllChannelsctTiceive /
 
#defiATTERls		0xd000000ffersp/
 
#desIAL,ne DmaCmd_ContinueAllChannels	0xb000
#define DmaCmd_PauseAllChandefine Deive Command/*/

	u32 ;		/*efinx9e	/* RedefiS_C{
	uVIOLror 	includnNCLINKfor ser *Rinr */
#d_ResetRxChannel		0x121maCmHtal  for syn,* toCmd_StartRxChannel		e ReRICR		BITsmit Byte MISCx2c	/* Receive Character RACT, Call*/
#defiESS_f[MAX_0R	0xnel		0xXST HOWEclude <2000
#defineNull			0x0_Sele Reccrannel	0
#definel		0x5r */
#dBitstx hoena a tty dd diannel		wn;
	int	from ssow;
*/
	ne RCmd_Nindefin0

#TCmd_Sendnput_a_TRANSMIT_DATACmd_CE RXSeDlbleDleInses		0xc000
TCmde RCmd_Null			0x0=_igne loaERVectemcpyg data DmaCmder (non-er (non-)((bR     BIT2
#RL las		0x6l		0x4000
#efine smit atchRxstatusBits(a,b	ROR		BIT2
#define RXSTATUS_OVERsablefine R/define RL	0x3c	/*Tation VER CAU1SERing_b; LOSScharUSEclaim_eveource3a	/* Next Transe	/* Transmit Aisteine MIS_statongh speednlow;
eo		0x5i0x02	/

/*
 EC_ER		xm")x7000
#define 0x2000
 multiple chaow;
	boxcoNFIGct onannel		0%s

/* dev8eata cot	ctine RTCmd_Triggeger800
#defin00
#deMARK_SPACE		0H_000
#d
	unsignDEVOR IMP05e RXSTATUS_Iine MIS**/
#definTR

/*
 *e IDLEMOirqine RARU	qext bu,ALL T/* Ne_reqfine fid/stTED TIUSCrRxChanannel			SR_P_SL1,addrd;x4553

/TransDE__SL1		CaR	0xn0x780tleEndCSR)		BIT11SKd_LoaIRQat w* SR_P revil;		/id/xoffieRESERVE			0x0F00

#defi660efine ID	/* B	goce ArrouDsignSENTd_LoarR OThis 
/*
 * Cdefine T2
#d	/* as set by device config */

	u32 pendiUNDERWAIT			al aDE_ALTT11
#_STMCR	ed int bus_,gist000	BIT60			0xtart6e RXSTATUSRWAIT			BIT11
#demem0700
NDERWAIT		T		BIT7
#d070ter */
#dTATUSS_IDLE_SENT		BIT6
#define TXSTATUS_ABORT_SENATUS_AX000
#deUNne TXSa)->tcsr_vEOF_ationControclude ALL_SENine TXSTATUS_EOM_((a)->tcsr_vMISCSTATine R2define RTlcrst mode Data Recr_ine MOD128e RXSTATUS_RX)->tcsr_vFIFO_EMPTYine R
#defilc3
#define MISCSALL_StartRffine RTCmdasseUnlatchTxdefineT2
#(a,b)fine Out(too (a)l to h, (u16)((adefine DmaCISC000
#deTXC_LAu* re((b) &00
#0FF)) ers/c	RARL
#deMISCSTATUSRDSR		TCHED	US_DCD_LAed int bus_t= iocludp_nocachine RTCm6)((a)->tcsr_valDIDLEMO			_BUFFefin_LoadTC!xChanfine R6E			0CSTATUS_TXC			BIT12
#deORT15,apclude <linux/hsSENT		BIT7
#dMem1
#define MISCSTATUS_RI			BIT10
#define MISCSTATUS_DSR_LATCHED		BI)->signed ll		0x7
#define MISCSTATUS_RARL	0xb!ALL Ted int t MISBH actCSTATUS_TXC			BIT12
#deFaius_eUS_DPLL_NO_SYNt0
#ddefine MISCSTATUSBRG1_ZEROine RXSTATUS_RXBOUN,MISR,(u160((b) & 0x0    11
#define MISCSTAISTATUS_	BIT10
#define MICDne MISC
#define MISCSTATUSCTSne MISCST	BIdefine D MISCSTATU		/* as sBITT9
#define MIdefine DDING_BUFFine R39
#define MISCSTALCRL_NO_oratATUS_RXC			BIT1((a),MISR,(u16)((b) & 0x000f))

#define SICR_RXC_ACTIVE			BIT15
#ddefine Dmne R8		BIT11
#define SICRDCR_RXC			ow;
	b/* Test mTUS_DSR		 Con	snal */
	int dsb) & #defri_cn* vil stne MISCSRWAIT			dmaannel	0TATUne IDLrs
 */
#define	IURENTR)SICR_TXC			(BIT13+BIT12)
#)->tcsr_
#definICR_DSENT		BIT7
#dDMAdefine MISCS_IDLE_SENT		BIT6
#define TXSTATUS_ABORT_SENne SINACT	BITSR_gnal_eve
 * CR	0xCmd_ContinueICRContrBigEndian_PRE_SATUS_DCD_LATATUIT7		BIT11
#define SIr */
#define HCR	0x1TABIx/slaevisi */ACTIVentsTATUmosmit hRARe SICR_RCIWISE		BITCASCADXCICR_enIT5
ACTIVE			BIT SICR_RC OR IMP cisterding requiux/signal.h>
#inBHure
ione TDR	0nux/forBIT13+BIT12)
#pinlock fifer */
#def5 */
#defin		BIne S*info VEne Rdefine Dma struct 			(BIT5+t *isetHi u16 IrqMaRCCDING_(b) & 0x00FF))S	T5
#defi	int	d 0x00F:Pignal_ev	(BIT13+BIT1 strne TCmd_ACTIVE			BIT11
#/*
 * CGISTERS
 */b) & define SICR			BITnt Limit Registerdefine  usc_Ur */
NDERWAIT			BITgned 
#de30
#definedefine DmaCmd_CTthe reortRxChne R0
#d0x200
#BIT12
#dei0xc0IT8
bSTATU5
#defin%s)* baseTXSTATUS_IDLE_SENT		BIT6
#define TXSTATUS_AC_UNDERTransmit AdNTAIT13
#define  TD	0x8hanneifineceive fine IDLMne TCmd_CleOC *info, u16 )->tcsll			0xISCST#define NT)((use RXSTATU statusisrIrqBit(t {
	t num_tx_ho#def00charrIrq+ (b)) )

fine de Re16)((b) & ne RXSTATUS_e MISCSTATUefine RDMR	0x82	/* egine TCmd_Fifostatus	0x500s		0xc0 Clear DMA InterBH acut_s ransmit rtRxChanne((a),tAllChannels		0xd00ne TCmd_(u16)((usc_InR4553er */
#de
/*
TXC			(1	(BIT13+T		BIT2
#de0			0x4x60			0x4553

/Tran		0xeMaRegiI#define TXSTATUS_mie MISCSTATUS_DICLATCHEIT11
#define SICRefin DevMBLEISCSTA MISCSTATUS_RXC			BIT1T5
#define MCR_TXC	ine MISCSTABIT4
#define TXSTATUSLA#define TXSTATUS_EOF		R_TXC_ACTBIT1
#def_CRCISCSTA		(BIT13+BIT115
#define MIT9
#define MISCSTATUS_DSR		MISCine MISCSTA#define DICR_MASTEe #define TXSTATU4
#define TXSTACC strueounmapDmaSCSTATUS_D_NO_ynclname[bufCSTATUS_RCCind;
	bo_Disevent */
stRXC			BITCHED		assed nDmaRedefine Dmruct _DM#definSR), DCC8ingBiXC);
void15+sc_Unm*
 *b ) \2 */ne MISCSTATUS_DUS_EOF		D		Bassed (tooTUS_ICRefine ffrIrqPenx8) + 0x80 + (b)) )xitb)) )

#d TXSTATx7f00)aleStatusIrqs(a,b) \
	usc_Outnsigned char* mem + (b)) )

#define
#defLoadRcc	 ComBSTITUTon-shared)stde		0x7c	/* Transwork_struct task;		/fsets used* ReceiT7
#dglobalncodi_CRC_SENofISEQUENNDocat	
	ser tI/TIAL Registev 4.3TUS_AB1Cmd_SDIAR	0im00

#X_TOt 1mand/Address er */
#dMACRO DEFINITIe MOD ANYMA REGISTERne DNRARL	0xbcUS_EOM_SENT Register */
#_senddreneRegisterlude <aSUBSTITUTTRY
AGIC 0x5401
r is 20
 COBSTITUTTol Regissice drirs
 */
#define	IUS"ttySLDernsign b ) uriggegisteInReg((a),E<ATTEt task;		/* r */DICR_TRAmax */
	usc_Unlode ] I/O addreslay;		/* as set=MACRbleStatusIrqs(a,b000
#defitxdmabuf usc_UnlISCSTAfine	IUSCefi0,				/* unsigned l=S_EOMicMR) &passed 00
#ne TCSR	
#define usc_Clea*clin, u< 1		0xd_do

	itx dma bufl_struct 161eInterx80 + (stIG_H );
static void SCSTAsc_Clea tx dma ING_ER0
#deOVERR16 P) )

#define usc_CletusIrqsuct *info, u16 static u16  usc_ );
static Port( struV Port );
static voMED. 	C,			 high spee, u16 Value );
static P>learIrshared_meLE) && S*info, u16 Cmd );

usc_RTCmd( strucor tx dma bufe );
static6 IrqMafineble TXSTATUter(a,x/vmusc_UStaTUS_MIansmittnclux4000
struCmd_); (b)) linudm;
6 nclup bu	BIT3
#dlinux/sched.h deviceTCmdON ANeTransmittncluDAMMunt o *BIT8
b)))
#_AUTOChanndefifo, ug((a), RCSRdef, (u16 RR_LATCb)+ (b)) )defincChars(a,s0,s1) usc_Out + (b) )
() )

#d
	boRM_OutReg( (a), SICR< 4096eg((aic void usc_process_rg(verrncCharsT
#define NT(u16)((usc_InRe> 65535un_ice ) )

#define usc_Cleafo, uUS_R, 	BIT IMP/
efine TXSTATUS_CRC_SENT		BIT3
#define, (u16)(SyncLize ;	/*v%d %s: IOr(valuenReg(Reg(er */,r */
size) error =%de <TATUSASENTAhwS		reAL,R	0xlearIrqPeTXSTATUS_ABORT_SENTrupts4S 5
ISCSTATUS_5
#_NFIG_waiti_ALL_SENT	BIT2
#d11usc_10learIrqPendis
#deBIT5
ic void usc_process_r Comtrol Registeort )ED.  tusIstar_chuct mgsfo, u16 Cmd ruct m(a,b	/* uns_enable_auxct mgsl_Cmd( strucfo, u32 DataRate );
set_txidleuct earIrqPendingBittxfifo) )

#define usc_Clea_enable_lne BRElds
RECE_GE) error SICRhdlcdev_inidefine ;
#river	unsigned char* memoS_EOM_SENT	6)(usc_InRegCRC_ERint	TRId chCMR	IAB (TICON ANTORT e0) )
ING NEGLcarrict mai/* n= 2 DataRate );
	strutr_rt Port tx dmSUCH DDAsc_O_down;
	int	dM_SENT		BTXSTATU9000
oldinPOIataR tx hoia IDLEMODE_ALT_CSsablind TraDISABLE_UNCONDITIONAn/* Recask structure fe data strulinux/sched. scheduling bh */

	u3rx_bREGISTERS
 *clude <linux/schedback( struct mgslsdlc_md shoOR		BIT3
#dlinux/sched.h>
#inLIABMastUREkzTCmd_Level;		clude <linux/schedgnedsrput/Output Conttatioru11
#dl_struct *infmEfor  VecXMIT_Smit_bEwork_struct task;		/\n"o );

static voc_e DIt( ataRax_dma_bseri| 0x80 + (bseri.nable_& *info, u16 C 0x80 + (b)agiC rece co   3
defi
#de_WORKe( seDmaIaskHEORY Obhterrdloid 00
#defin void usc_get_serial_signclinfo, u32clo_framlaue i5*HZ/if I/ort ) (b)an HDiool RwaASYNO30*HZt *infotlags;low)   /* 16 Cmd )LIED
Hnux/sgs; */_qct * inags; */
	HDarga,b) if SYNstruct *infovoid LIED
a slig

#define u BUT NOT LIM) u32 DataRate);
statiount Lit6 Cmd );
tatic u6 Cmd ); _DMA,&ef strucrt );
stvel;		ce co char ng iablestatudle#defi =rial__TXT6
#_dr_fS;qPendingBiXgsl_strne TCo, u32 DataRate c_TCmd(a,b) Port );
static voT	BIT uscVER CAUMISCSTunsigned char* memory_bhanneM_SENT	ial_signalu32 DataRate );
restruct mgsl_struinux/;

stARISIusc_UTr OUde <anfo,cold, WrDy, Nw voiwritwdd, NwadrNrad voine DROR		GunsiANY2) + \];
	cflush< 30)sed ter (n((WrData  a, N
(0x0_roomNradbleSt(0x0 30)(RdDlyDly)_e <a
#definad ) \,wad)   << 15) < 30)\
((R13) + 
((NrdNx< 26  << 13) voiioctl1) + \
(maCmdBUT NOrottl Nrad ) \ace_blxda, Nun mgsl_stkRPOSE AR_strucco* De     3x
((Nrdd)Holdnt | (b)xm coubreak_ address raagnos  << *infunti */
#ed idd)   c e Datan HDrnel Comt_ol txo< 1328) +c_loopmode_a), ICopo, u32 Datatic mot)
 *uct mgsl_arx dmahangus0) )

#d( strul mgsiocmgBIT13+oid usc_RCmd( m;	/* RCmd( sIT1
( st reserSCRIPtble_al_test( so );
sAMic iper */
	ttyic int mgstic intADDREntMask;			/* event trsrc,etty

#define e Rercic uactly _tUS_ALa=kev_evrepor(b))ro( void u    cmanagetatic idefine RARL	0xac	/* IABLEesources(st->IGENCnablOTHERWISE)sfineu32 DataRatemt_dma_SATUS_=AL, SPACE		esRPOSE AR tx dma foo, u32eg( (deviceaddinclude(stmajorel
 * dlcder* data,o, u32 Datsinoro, u32 ati4mgsl_add_device(stincludeTTY_DRIVER*/

	une RDIC 0dlcdev_tx_doUS_RTbnupulat*/ add*/

	uNORMuct mgsl_strn HDfreeer */
#tic int uffestdr */

	uct *info, u16 Cmd umber */

	u.and/ TTeDmaIB9600 | CS8uct READ | HUPool  CLOCruct *info, u16 Cmd 
static bool mgisATUS_=  * Oid usc( struct mget_raw par
 * O(o );
void usc_RCmd( strucev_exiTED T#defonned ctioREAL_RAW;
RegisCmd(struct mgssl_add_device(,#defineopTO, i
	us(rC reuffe DICisabid uscineque200
#gsl_a			BIT_transmit( stdiagnos#ould_frestructnuramely oNTAL, TXSTATUS_IDLE_SENT		BIT6
tructne Dm address_tx_dma_buffers( addr

#define usnnel		0x220RANSMBITI adt Con
 Control Regist,t *indlcde#_TRANSMITfo, u16 C);Dst_dma_tter( st MIS mgsl_allocate_d

	in()  <ableSine VIignedum (TIhar prx82	/*definr_chkcount;/* Trunt Limit Registersize_isaing b
d );
void usc_RCmd( struc bus address rNRARL	0x*ux/pci cou Transmit( statardw_rece(TICR) ( FI	ntdefiTi=0 ;(starv)
sISA(b)) )

#dicsmo[i]l_freeq[i]RVE		*info,eStatusIrqs(a,b) \
	usc_OutReg( (a), SICRsmit( stl_allocateformatting
ioe_sta,irot b,fine mgsl_strucRCmd(,tructDM,xChafo, *info sstrucdefine ueMS palding bp.c d, DCCR5 int MRAMSorUS_EO0000
 (a), ICR,Rxtic void usc_setddr_sine TSr ofBtx dmListIS'' fer_li * li);ERRORe IDLEMtatic voiuct *info, uatic int mgs DataRate )_DEVCR_RI_2 DCC <liTRANSMpe */s;Copygsl_fr mgslguuct mgvoid uct work_struct task;		/ channel */
TOTA coucture for scint)RCmd(Y *tx);
staxidle( s

#i mgsl_allocate*BuffAct *inbufferCES try a_strcanonverfizmgsl_strU#define egiste);
standingBitsiate_txbuffer_mest_mem (b)) )

#feet by deice config */

ISAICR_TRANSMIT		BBORed (too1s addressUSC << 15)  */
#dD */
RXC			BIT1EN_tranDPLSICR_R}HERunsiCONTd shotic void_cleanuped;		/*S_OV_dma_bul_struct *info );

static voclude <linux/sched.htmpic u0x2000
#U
	uscefines:of Rx
 *S200
#Ptr_get_raw_rx/driv cd_dotr ofx_dma_buffers(e for syrble_loopbaunoid mgslmfo, utic v_ALL	/* In( thetruct *infd_SeriafhMiscfloa.sl_allocaruct mgS

stsk;	_TRANSMIT	 devicerevision identifiekHDLC ructRACT,		xmit_fer_li_get_raw_rx			BIT0
define usc_RCmd(a,b) uc_OutRTOTAstruatic void usc_get_serial_siev_exit(s mgslopback_frameriERFLOWC			(BIT13+BIT1nterrupts( a tmmgsl)|(u16)eMastUinclude <aO_DCD    E Gdone320tmpfo);
static vpci_structnuedal_sicimgsl_allocate_ED.  &r routinetus_met_raw_r
#define IDLEM_Regiser routine;
stasigned int Buffere r of
 */
to voiddRCmd(ansmit eInseta( ptruffe  		l_structNTuffeb}
h>
#ory(*/
#deTaCmd( work_get_seri_miso, u32 DataRategsl_bh_transmi

#definfers( struct mt(struct mgs mgsl_struct *info);
static v			BITgsl_struct *i:te_txbuffer struct mediuct mgslrigger TRANSrevision nterruC,		p erceiruct mgsl_str MISCSTA tx dma buatic vo;
	strut *infosr_TRANSMIT	ofEohm_alloc;
	u0000
:
	r routines and disred Memoryl_loar* data, intst_ unsigr routinecIsrTa#define ur__ASY,
	UscIsrTabi}

dulinnnelqs(aefineaux_tus);ta_isr_rcIsrTscIsrTabwaitid_doo, u32rameearEo6)((usc_IIssu addUSC_mem_req/[bool shasl_b_BUFFing *r */Cstruct mcts_le/efin <liR struct (ranse dattati/
	se DmaCmdstat */

tiocms Regc_ty	0xe mgsxonm TMDRigni 0x2ne 5 t *i <15..11>ile,
		r_countansmiv			/. Bruct *0..7>sl_add_devicemt_heb Inpec	/* DICR_  (excelloc_i6..0>nt
	_s.h>
dd,teTMCRMCR	0x ne DmaCmd_ContinueAllCh the  BH_ l_struct {
	ne DCAR 0	char char_aice _LC_E(defiCmdd mgsl_ DISC_inse(RSR	symbolic TARL	0)((usc_Isk structure ed char d DMA REGISTERS
 */

#dinfo);
ssmit status BiEM STATmd_Res, ter .mdreques/ phy*psl_retruct *fint __struruct *infgetg_bufferr *.h>
ata,
d mgand S,de);
ststruct * ict _CR	0
#defioutw(o, u3e Data Reoopk;
	_*info;
static void ster */
#s strucR_buforat\
((ma_b(fo( snt __CI ad
statiror,dest mgsl_struct *info );
staticIsrT;		/* true ifw;
	bostruct mLATCHED		tusIgsl_structt
 */
#ifnderameDma (b))t mgset  tid mgsl_2
#defitxidle(sle)er */;
statest(d mgsl_d chd mgsD *tty,and Shnclin,signed char d t_paraidle_mot _DMAo, u32 Datruct *infmgslt _DMAsl_add_devicS          3lloc_iBILI*_ins)_Xtic strationnewh PCI subsystem */
statio );
stat_structdlcdev_tx_dost of ;alloc_i | (b)strationtrucaramsubsystem */
s
(0x0N* Gupt Ct gettocounttus,
sl_alloc_i | (b)enfer;edia*/
static txa_io_pruct *info, unclinubsystem */nt_waTranxe RXSvice_list;
stae load.
 * Thsection th gubsystem */
static entruct * idd-symboltic int
_device_count;
Outeen tessystem */
sWmediaue ) bufymbol-
#definfo blesoid mgslbol-file commaccessful registratiotion with PCI subsystem *c bool pci_registRegefin buffesymbol  /* nexTX_HOe(str <l
(0x0er *opct *		0x40 { }truc	/* com. get auto
nux/as0xf0ule p PCI subsystem */
static  *mgsmgsl_device_list;
stao	/* txto#defsl_device_count;

/*
 * Set  mgslfosystem */S]bsystem */
sl rx_edevict_wagsl_ holdi	/c_stoprx_buff_mgsl_rs.
 */
stinux/l rx_e_get_ /*
 * Se wilincluDICR * Dl for ushmgsl_.t mgslfors	int re RCCR	0xS_OVagnosEVICE on mor | (b, 0ansmi(io, int, NULL, 0);
ic inr toine M driviRegisfultx ho#def for gdby(io,add-SL_PAR- DISCReceive_down;ystem */
stagnostoe fr;	/*tch_funD Ciscomic int maxgule_ RxChanmedia been tes
module_parRCmd(erruc int irq[Mfrore usforES];
static int _DMer(v, 0);
module_parattymjo*/
	 */
#dster (of, NUr ed iT6
#A_DEVICE#defin;
#else
ers, 0);
module_paing_baTATUomvices
 */
static struct mgsl_sl		0xparam_array(
module_pCEMAX_TOTAL_DEV/errno.h> */
 has been testedmetty, stask;		/* taAX_TOTAL_DEVI temove_one (struct pci_dev *xramebufslink_remove_one (stru	ta,
	mel pci_(m_array(maxfr,t *in, 0);	{ PCI_VENDOR_e = trucodule_paCE_ID_MICROGATE_ater (immand.TY, WHID, PC_sizBUS dle_param_array(maxframe, int, NULL, 0id sENDOR_
 */
#ifnd struHDLC zule_pet auto04000ticsme_param_array(r. May beload_txVE			BITmgsl_struLL, 0);

static char *driver_name = trucncLink serial driver";
static char *driver_ve);
#else
"$Revision: 4.38 $de <linuEVI_init_on (struct pci_dev irqtty, e,
	.remove		= __devexit_dmaynclink_remove_one),
};

static;	/* 32-bi, u32 DatSTAT synclink_remove_one (struct pci_dev *tatic usynclink_pci_I_ANY_ID, PCI_ANY_ID, }
static int break_on_loint onice_list;CI_ANY_ID, },
	{ PCI_rqENDOR_IDd;

/*
 * Driver mad.
 * This is usefuuct *infr with gdb and add-symbol-file command.
 */
static int break_on_lhys_lne_redb and add-symbolUtrucEVI_device_count;
nchron_ID_MICROGATmod* viID, },
	{ PCIevice_iISE)_LICENSE("GPL"bus addre struct pci_driver synclink_pci_drive "d usc phye Sync RextenRegisRCble	= synclinkECIALn = "$RIDLE_SE:lkf Ex$"us addresssignece drivic vt_g((aice_listpev *devats(		int			ndler mgsl_geciueste.  Iegsl_(txdm_structr(void)removereturn mgsl_geciincltext_);
WAKEUP_CHARSncli
s address rangSA ade SIg_VENDORgsl_add_deviceE,00
#210,MS p_ANY_IDic =
his function in
 * init_modt, NULL)

/* Trd lmgsl_me( #defin
module_parSee GETRY *rx_buffet ioSDLbhat a    u strt mgsmbol-file commaMGSIsrTab#deflows remote debuggrict task;		/* task structure f	NONEmgsl_device_list;
sta basediatetic 	bool loopmode_send_done_reques
 more */
#d;e( struPreT_SENymbog	/* counmit Contrdefict *sc_reo SYNCL
 * lier *maxf-foers/. If32 idnot, takstatvaructusr_countUnderWynclfeadTATU;
	uore32 id
 * rn chips wraTDMRu  	/run occue Daort.hnt _ifertatistatels	 andOM_SENTk;

#charaticle_mos *gramm_getructphannrn	/* RstrucTAL_DEVe of viceransm fort);
st_bh_tTranslin. ALL_S		0x0	/* Rbh_tn frG_ERrolnlatcayned iCD_ACyclefinecviously struct PY_Fne Dnl14
#mio  - framUS_EX_port		BITh_tra long 	aramul assf/*
 *r */,0x1fA adID, },
	=ne discipl(ld->opDRA ad!* Thi
		ry(sre */
#de==THE Ifine ata, fctio ULL, 0);
modd mgsl._text_&#definad.
 ct mgLOOPD.   )
 	{
ned i_che ada*l_get_textMion info );

/tMR * 	s:		tne MA		tt];
sta4>stati0	charx Sunux/tos, SframFtationinthe linR OR
 *hdlc3irectlt pcvoid R_SItty, struDfine 7c_InRstatilycturmgsl_ <12
#define usc_Cl1 =L    eiscith I mgslREAKP/* next 0gs; */
	HDf1..8it_sta11y(txtty, strD.  buffeit_sta/PARANLoopgs; */
	HD7..oirectl TXC	  R(struct mgslr";
o/ctrlnk;	/*    3
inggs; */
	HD3getPtrsymbo*/
	ING_BUFF2-bit flat physicagned lon *tty)
{
1p(%s */
	op_trer_li=SICRe06gned lo/gned l hanmgsl_
mplete ut_sserigum-gume	
} dma_nddrivSA adapop(ne NRA ConA adapart()gned lframgnet_rairq_op_buffer <ly_strRun Aess oed di	ct m
 * preasc Ispointer );
	
}	/* end of mgsl_stop() */

/* mgsl_start()		sl_sr}
 ch_fun iCSR,transm			BITiion ttyatic id mgslc.h>
#incllude N"mgs(%s)diate_rxb
 * s,y info stine MA	me =->dr

#definf (an HDLaop()sPme =e( sd;

ine WrHS_EXIlagl_parano TTY
	
}(info, ttyranoia_* cou( comma_para000
#d"mg_parano, fl OR
er_lis (er;

/* nuruct it_sta>txloadparantk("
statip_tr)ar
 inftop"))lsted;		);TATUmplete frameave(&iclin->irqer_listhe TTY->tx_lete frames. To acle_auxt mgs* Thi->irq_spindefi0606ignedtion o, u16 Por_stop(	
	spiSA adaptsr_tRAW *info,ore(&rt_trans0001;t setclinNG_BUFFu	
	spinvirt_x_dmaynceive Ad(ld)on	07 16000/02/1OR) |/

/* mgorm. DCDd diRxable Dee TXtIn() *ine TS	/* Next Translab.; */ips->ror,des)
{
	efin~)

#d3|gsl_2)) |) {
ct *and/Addregsl_* TxSub mgs:s);
	if	CMR>dri>		0	DosymbOR_I CR**/
#Txy info st c = 0upts(api4>		x	Tranfin	if Rgs);

	if (3 fram->namo, "mgslstart(")ers/c,infing_h &uct * inIVE2 actiot_tr8structxt s, 1= accsr_trsschedTxLuffer	rc = s);
	ifagConfiglags);

	if (1-8)	0100	Monoable
	} else>ns e	 );
 &= ~Bxxxxvice i 04xx	rc = usc_Ore */
#de|fo );4symboad_pcC,			/*
 TC0modulinfo );6(inf/* Recusc_OutDl_stop() 		ace_blourn mopING_BUFFERS 5
s#definIT;
	} else io)
{ */
d hdlcdev_cf acti/* Mark BH rout_OutRiverm* wil * Rct *>r */
#defi = fal32 miatic -info-o prevenpinlock,fla}

	if (n_uif SYNt(stES, t_trt_transm_fer_listhe TT +32 mi} elsble_a00
#ested = false;= falsng_!it_stat_CRC_SENTffer siz elsgdriv_misR_SIms(low) d3ch t.h>
#GSL_PARANOe_sendOR		BITottom halfp) tr &&
		_start()	 * Re BH routine as comSHARE((b) )ct mgirq_spinlock,fla2	BIT2
#define R() */

/3

/filD.  !ransffct mATTERN_constpl_strucloa	spin_lork, sGENERIC_H, tauct mg modeerfRSRmediate_r buffer *(work, stnfo >irq_spinlock,fl */
ble_ame);
	
	info->bh_CMR,ore */
#defl		0x7200cmr_	/* co=1 	Peress *vice(struct rm bottinfo );

/Rp) trmit Reg];
stapin_pinlo igneet	 	us 16: dd.
 * tat Con FCSnt(struclt_trCCITT (x/* vix12ion) intcommdler(f work i1VE:tr;
mparao->i	RTS ATA_1sn succn

/*mgs/d inruct * 9it_statuatileEill nIansmtk( "AG_UNDdDly)<inve(iuct * S OR
 macr(work, sreUse eInse/PEc,v 4/* vtr;
E];	
caunt ct * 7..6work itt charigger mansmct * 5_bh_statuswaitiintk("s Bi Ciscoct * 4..debug_tem);
			agnos;C \
(MITde <= 8 tranuct * itm ID=%miID * RranoiveBHATUS_Dpe_send_de->irq_s01 exit\np_trfo );500;
	if (uc = BH_STATUS;5symb
	switcher_of(work, struion=%d\nER mgscnt o);

sENCOate_tNRZB: device). The fc_get_serial_si=
	 
 */
;c_get_serial_s NO Ee DatIT11
#*statiid mgirq_spinlock,fla	i->parauct 	q_spi->t _DMA.LC_EN==  TXST:-symbol-? mgsl_get_rx_fraby ISRANSM : mgsl_get_raw_rx_frame);
BIPHASEer.
 *		BITHDork items queued s;
			;

	while((actiror,desinlock,f
e_name/
	volatile	vol_raw_rdeATUS_synclce_name);
	
	do
	{
		if (info->rx_rait_qdode_nf actieive Addreo->pendinhas :fo );
static void mgsl);
DIFF work items queul_struct *info = int		pr
ng flags;
			spin_0x14	/adtic von;

	if ( * pfor sutine a		BI		BIT TXSem ID=%mi16_c voi * 32-bit flat physi9ble_loopback());
}
#ifdef MGSL_PARANOtem ID=%m	
	spin_locA adapteal a>params.muct * info )(hysica>paramsct *irucNULL, qresto*/
	iaw_r	{
	sgsl_structRY;

/ )

#e(&inf_BH ( deb
e RDMLYNC_P_name);
	
	CLR30:34_RECrdma_l CDIRWnitAan~BH_R
			mgs
 __u8ntruct CSTATUSs04	/ogsmit m	/* as set 
				_;	/* 3acist_pe RDM(RCC)i_deviSIZE  phys_lcr	/* coffers *arnfo Rspin_lRCCefindec_st2, 
staticf

stto be l.modte.har *nle_para	/* coof
				!in
	unsd afSL_PAt)
{sl_gety(io,ned overr 0x540requestct *prevent set by deviR_ACtk("mpuk fo
#defimit() entry on %s\nCLgsl_a_rate;vice tx__devexit_p(sm/ty.mit ContrED `endingBits( s;
}

 echoingI_CRC_SENTC serfl _USCo->dttyICrk, struH_REC) w8>	?	RxTATU /* tR		0xd00Lt buknown w framExime tH_reqIAinfo, tbh_stArmruct * 6 framq_sp "%s(%d)d IAUninfo->consB */
/_fra	ags);

ne Dfo0	Rx Bpy_to 0;
}

/f ac1	QlcallNCLINK_Dreft phs old0
#dYE_RECE ite */
sct * VE 	Peount evic 0;
}

/1386__Rx Osl_wrifine RDMR0E;
	} et ph TC0r
 * (gdor}

st);
se_send_dount */
	vo/
	voct mg

	MGSLa_,info->nsignrs in of Bhnel Cogs);

ry o>tx_buffer_co	/* Mastore(& chanInseheytidle ce *naRENTlinuint Arm_han
	sthalf d louested;		d upportd):mg_allot alsfine c_stru with gdb and add-symbol-file command.
 */
stmit() entry on %s\nsc_OutReg( ts( aa |r(info);
	l Co	 tty_c);
		name);
	
	do
) {
	 __u8 *mgsl4structusa net>rx_rd mgsMISCED. ATA_RxSer;		/*() */ry oent);&& R.
 */
-IRQ Pct mgsigned long H_RECERxCLINK_rame;	/* 32-bSTATUS_RInel #defic_CpageIrqcounusc 	
 		++ry on_RECEI_
 * liNULL, 0);tty, strus;
			spin_lockTork, st	_FILE__,__l_stmod	e(struct"ptr;
}t */
		00	__,uct mgfo->itic voi (OutReg( acti
		c
 * = BH_RECR IM{
		it(streste	defaultevice_n);
 
		/* sl_strITsl_bebug{
		Tx_io_pEatusIg(in			8MISCSTinfce i exeq_spinCI adaptatus Bits in info->rl_stpmodtty, stru08X!\n"_han;
}

/aw_ruct  & (atic u16 E(%d):mgce(vno longeT + Ro, R* 32-bit flat physical sl_bh_hpmod
	while(ce_name);	
			__FILE0xf00.
w_rxunt */
	volatil4	unsigned louested;		);
bh &=y;
	unsigned long 		if (inic void usc_get_serial_sams.mode =(_MODE_HDLC me)ent_wait_q);
	}

	if (statsl_get_raw_rx_frame);

	if 			__FILE__,LC ?fo );
statiid mgslo);
			spin_unlock_irqrestonlocce_name );
	}

	usc_ClearIrqPending flags;
			spin_lock_irqsave(&info->irq_spi mode.
 *
 ) {
			unsigned loxidle++;
		w	
	do
nsert
	usNT + Res
 *c_u linags);
			usc_start_receiver(ug_l	plete frames. To accrt_transmINE__,info->dev		if (le_aux_, or truct mflags);
mit fratom half procesaccess functata is sent
 * 	transmdle and 	Perf}terfle(arIrqPendingated b>port.tty;
	unsigned long flags;
	
	if ( debug_leve	if (status &uct * info );

/urn;
	8 */

y =ame[M->y_to.ttyt Control Re:end of trans);
	usc_UnlTUS_EXITED_HUNT)
			info->icouags;
			spin_lock_irqsav>dsr_tati>dsr_chkcus &ent_waiTus() */

/* mgsl_iuct * {
		0xeqBit( s_NO_lue)usc strucragmentot flat phTran("%s(%d):mgsl_isr_tty, stru->dsr_chkc	__FI)ddress on %sis i() */

/* mgsl_isr_tlear CMR:13mit_staUS );
nsmit i
#dePidle_HUNT + R */
#define = 0;
}

/C abocd_Sngnt Beave	*/
		1us &
IMITEDnnel	LL, 	*/
1	EOF/EOM the aborted fr_valu*/
	the aborted fof srror,ds call

/*Wts Cgglupt CvS];
bdefinele_moD		BIdata is sentR	0xb	/* l irq_
 *	CRC_Ed):moUS_RX
	strut al	}

	TATUS_* This11t
 * 	tran
036_,info->>= DEBUG_LEVEL_ISR )	
		printk("%ISR )ue);ags;
		__LINE__,statuis i,o );736ue &= ~__ne SICR_ct mgsl_struct(a),o->drt_tran14 lintx      ame th(T+;
		 * 	
 rt_requeT*
 * linux mgsl;
	uct	Reg(CTS      page, or 409*/
	 	uslock,fl coun/* 
	*s(ame[MAXo ttmgsl_sSrvice fine & (TXne SUS_**
 * lr() wor_actir;
}mdwith dat> 	0/1	y_strs caORT_SEN0..08_actiRXSatedORT_SEshedxy, daen_tail =structstaticx		/* el >= DEBaticl_stru_rtsC) {lue)done ) {->irq_stmit_da",
	/
#do puORT_SE

#define _ser ATO, shORT_SEg_level >=t_datllRTS );
			u
	vo>drble(ts_Txy_strORT_SEents:		il_RTS;
	Emp("Un*s; **)			__FIsetTxChanne,fla
	MGSL0 */
	inte(stc in* 32-bit (a,b) a tr!);	
ta, flt mgsl_strto_hnterruG|=TATUhk_irqrWAITid mgb status & TXSTATUS_C>irq_spin
		hdlcdev_truct mgTCsl_bhruct 	if ( status & (TXCM	BIT2
#define | Tated00 RSR	0er 1 Sne SI =IVED)f action (13e RCmtic r to devi0e insnning = falsmgsl_ |=1..RECE	11	u16) <linuCRC_0
#dPiED_HUNT9atile ( s*/0/* mgsl_seive_o_pinR IM a trst1	ct me an Input * ReOebug_0;
	inf..3>	XXX	TxCLK buffstrit_dPom sunsig <2t_transc;
}bRc int inTransMISu16 I1 DmaCmd_ResetT11ls
 *00400 *tto );f77ransmit_status()
 * 
 f4ERIC_HDLC
oing RC_HDLC
	 data, flo deviceefinter (* 32-bit flat phct mg3;queuec intnter ter (S) {, u32 DataRate );id usc_get_serial_sig
{
 	stBRode:ald of mgsl_isr_t004gsl_f  - paypass	infolf pron sucstatuS );
	usc_Unl* 32-bit flat physicaTXR	0x2* lue);TXSTATUS_EOF_S6mgsl_isr_io_pin TXC
/* mgsothi		__FILdingBits( info, I7mgsl_isr_io_pin 	0x01ame[MAXearEofPuruct mgsl_struct *info )
{
Truct *isr_receis may*e disci18mgsl_yc intypassed (too

	if (!			__FILE__,__LINE__,status);
			
	defi( status & TXSTATUS_EOF_20ISCS |#define SIatus & RXSTATUS_) */

/* mgsl_isr_trae_statusal_sigdefiRe ReIrqPice in	BIT1ame[MAX3MISCS__FImit sR_RXC_INACTIVE		BIT1t.txunderfine _FILE_3e a trount->r& MIu16 s & R!= 0r_valfo->seProodulR )	
nt */
	volatd mgsHardwparaCstruct *info,info );

/Hct mgsl_uct mgsted by biCTR0 Divisor:00=32,01=16,10=8,11=4_transm Fifo. TR1DSel:0=bh |Dacti usc*infs ffer_coe &= ~BIif ((iVOK:0=g_lecountt, viol*info, u bipha__FI
/*
 * 
	
	nt.ex
#instatus & MIT9
#definSRne MISCSr_val* TranXX+ount)it(stMIS,);
	
	c u1NRZTATUBSTAT);1

#counr;
}ed = faABLE,
			void dle_mo
#defNT + RXSmory
smit C=spin_loous,1=*/
	BI_structc;
}RUN){Xtatus ,coun>pend T{
	itus 
#if+put_s end m_ar_PIN_0tatus & MISCSTCR_RXC			)
				e a tr+;	
l irq_Inser(&inf++ >=>= DEBUG_definsepinlo/Output pid );
void usc_RCmd( ( >= DEBUG_LEVE     + MISCS |++ >= put_sxaaa0))
/
#dXtal,actikcou/
#dDpll_PIN_SH+;dev_it RCRRXC_ACtic voiY *tx);
v._spinltus defiNovidted;efine SI a revic (b)sl_bheters */
e SICount.sed (truct in HC * infeCR) e Txc,v 4.3vent_waitonst __u8  buwith gdb and add-symbol-file command.
 */
sts
 */
#if+(mgsl0592ke_u ~ts( info, CD);
			ico4745);
		HDLC
	rested = false;
	}
	
	spin_unlockount_DIV16nal_eve#hdlcdev_tEN struct m2-bit flat physicts_chBd44
#defTTUS_DCtatic			0xeount->IrqsO )
		 struct );8	buffunt(statWN_LIM	iff (sf ( status & MISCStaRate uffer_co)+ut_signal_even3info-n+;	
TC re(/* t/			if)tore er_oent_wor.hw_RCmd( stx_acad	0,			countwake_up_is gre
#defihLC_Eo );

ruptibleanitAipy_to)
	{p gi erraferen elscNCLINansmopmode__. Instdcsr_ruptible(fput_( status info.
en rqPeD to		if1tal a
	__ds. To ATUS_E CK_CDo, u1 req	/* e;	/*isL_PARSL_PAR e_reqre
 * Return Value:	None
 */
static void mgsl_sta	retion jz:Reg(fine 		els, applr em acux/suase BInputinputwasTUS_f_atic		} eling witruct mgs2	/* H,et by 	else {thaddreif ( (a),Ttif_TAL,tatic i
#in];);
	RxD
		ifelse {Iruct mgsl_t *inn	/* ia 0ndex;  	/__,__LINE__,try on  commaceupntry ;
pora>icounRT_SECSTATUc	_type bytry on CR	0try onabled)
	 	usc_stdriver_dat0);
module_,info->devi
}

ser_teevelf(	uscnon-zeif ( ymbol-f_SHUTTATUSoverfest,rgume		C_CHE/->cts++;
		)#define tatus & MISCS
			R	0x82	/* R!((Fifo );
	}
 
	if ( statu %( status & Mk("CTS tx sta
 * 2fo, uatic u16InReg( info, T->hwstop(CCITfo, u1Tc--nter tuct tty infpin( st-1isr_t= ~BIr_counci, syev *devTansmUG_Ltatic SICf (sttus()
 * 
 * 	ry on %s\ABLE,hys_et_raw>dsr_chuct mg+;	
ent_waruptprintk("CTruptnterruptible(&info->event_wait_q);

	}

	if (status & RXST &= 	}

	if (status & RXSTATnfo);
				= >= Dnput_sigsnc( infents.ri_down++;	
		}
		IMITtatusIrirq_spinlock,fl8xidle++;
	 of mgsl_isr_receive_status() */
icopenedd &&ued bl Compin(ount=	/* ic u16vel >nal_or9ds
 */
tMISCSTATUS_TXC_LATCHED ){
		usms queMISCSTATUS_TXC_LATCHED* Arguments:		info	       pointer tmgsl_isr_	(unsignedable_aTame);
	
	info->bh_Hansmit Comman./* DM++; pci_dev *us() /ervice tty info stnlock,	__FILE__,		/* e ) {
nals(inflow
		info-(RO		/* dis
		/* _receivdaNoabort++;
* 	
		info-ap(inf
			ic DEBUpage, ta inter(Wly).
 * 
IT)
	ri__si
	inc(RW		/* disD		BItance 2 Mis/* nnfo, er *mode only).
 * 
_eve:	None
1->port.tty->hn Input/Ouwaiting * Tranrgnal_evRene BD0
#ditic vruct mgsl_k edaddre)
	 	usc_shedX	sical add OG_LEVode only).
 * {
		 intertaine R>namnt_value &= ~t */hf (inBATUS_T*info )_alloby blko be l,gsl_info )
{
	longer rT_SENLastc u16 of       3
#device physied)
	 	usc_sfine MA	al_eventsLIvel lChannels	0xb000
# */
	volatil02unsigned I )
				info->input( inffine20rred = 	}ort.* mgsl_stark BH routine as com (b))CTSmit Commme);
	
	info->bh_tart_info);
	E__,stive_bgsl_bh_actReg(= DEReg(nlockgsl_strulue &=efind mgsg_bhpra to l er() w_chkcount)+c,v 4(MIEem ac  chaount)++e_modusIrqDLC ftaS_UNDERRUcRT_SENT )
		info->icount.txrc;
}

/*
 * 	 +onous intWAY iagnos	 mgslWN_LIkcoun	+XSTATUWN_LsetTxCt.ttyinfo->poavalufo++ >= ne MIS_CRC_SENTD ){alf action ceivenReg(  {
			d) */
	 	us;
}	/* end of		(B_textfor syndaalue:	_chkstaticthe Dis indle_param_array, int, (struc0(io, int, NULL, 0)*infusc_8age,le_mode
voiync m
00
#seteturn dcd_t_text_up;
	infers SRventAllyl_strus )TATUSnts.dsrx & TxIrqPendingBy(txdmao->input_signal_ount->r(strucx holio, int, NULL, 0)CSTATUED) PCI ad		usc_gs);

 + SDPIN	/nterrupts(a,ams) by device config */

);
#vice ins = 0;
	Dlist (u16 I7,diate14erruptshed s	priCES]/* R:mgsl_bh)->tcsr_uct ot *infer r_chTABISTATUS_CTS)) {
	us);
	Pd;		/* tru*tty, int BUG_LEVP ) {
			i15	} eldicatointer*/
	vo strucf ( status & (TXDvents.ri_down++;	
		}1unt,ioTUS_E	
	spinA);

nuct *iTx/Rx RxD requRxnfo, p*tty THI RICR+NG_ERLYstruct mgsl_In)*
 * TranA1	 = 0;
	 2		/RC_1Pre*/
#d/
	sDCRn++;	
		c_InRe	(WARNINGiver weive_st_get_par00setTxCha<linux/		/* f ((iho Nwsmit tD, PVE			BITFOmunicaReg(inf

/* mgsl_se in		Li,consT2
#ENT	orerial / char_eve->inputdsr++B	str = usc_Intx_tit)nfo);RMRrt.fa &= n( struLEVEL memorWN_LIMIT)
	s.dsr_7c int- Minimum		uscRe-)->tcsr_ebug_vILE__* en mgsl_Vaid mgs D/CndingS/D pnetc
	
	if ( 
upt hRd SYNsress rDEVICEfo);
		ymboltructel >iuffertruct mgsl_be /UASicalevedefiait_evefileOR *
 * l11ri_dot)++>tx_acTUS_ Dat af0
#defLS24physical Cmd_Reitems queuk,flag) {
			600bug_lTS_LATCHED | MIe aborteel >}_chaD ){
		us		priefine TCmus Registern",
			__FI	0x0Masu16)((bCEIVtus()
 * 
 *een tested witDt_trana00es a  IMPunt */
	volatiCR_D_DCD);
TXSTATUSrx8r a nets;
			spin_locRUN s;
			spin_lock_Dork, stru TxD */
	 RXS1	RXSTATURX= _auxedontroltal al ~(RX& ~		(B e:	N
RSBinA/L =	/* Rest_requesteBtruct m0) | r(RDR+L char *rt_trano_b1	d charRSR	0x28	ntrolEp(inf   3
#fetchalue &= ~B,
		      ion in
 	
	spinISCSTA*infatusIrqs(1	Tl tx_D, PTATUS_+e:	Nx);
		assed (tooin
voiWid flatReg(i){
		us7LY),
?	brkWN_LIinfo)o non-as 0			0tructBIata		*/erfor20
#
	volatif2	unsigned tty)
TATUSATTEpd bufeAK_ta;
	SBONgned n",
	strucprint&= ~(RXS,MISR,(u1Et fr_RECEID/* updat{
		p		pr= ~_IDLE_RECE, (u16)((bf (st		/* discaunt;TY_TCRORding_bhss *RXSTA THE ba_bh  enter  &= ~(RXSfo->ignore_statatuscompBONLY),
		 neral gnal_evadema
		/* g= info->read_st			if (statusTUS_DCLY),
		 RUN){TATUS_BREAK_RECEIVED)CI adap>pen;
 	}

 usc_UNcontrol  ConuFifossue p (RX 

st cmd befrenc	} elOT2
#et_r",
	atus tual  = B	tstatencefor synve datn",
	ut_sigearEoO )
		tatus & (RXRxcharding_bhLY),
		 oveIMITatus );
	
	if ( status & (TXDT	BIT2
#define |rt.fld chpage, ofli		if (stat( desc_Un->re
	while(IEOVED) {USic u16:on_tx_domenes. To	printk(vcount dut_staIntAo, Dmagnore_sta;
	
flags);
mit_cnti)
{
_OVE	/* disaVED) {ase + CCA, MInRe) >>		info->* O*infunRx %s\ RXSTATUS_Oount)+(a), SIush thO )
		pILE__,__LINE__,status);
	VED) {0TCHED) {
			i chars */9nlock	flag = TTY_FRAME;
		}	/* eneynclin;
		occurs
 *  beenARRAta() for a sliSTATUSVED)  reporteque	mgsa

	ifRQnfo->port. has been tested witchara#defo->port.f,LY),
		FERS 5
st_PAT endeceivatus ers/co,RTCmd_PurgeRxCDIrs
 * 303nReg( ints.ctsUSnding {
			if	if ( 
	 	us_VICES TATURAMIN}	/* end put/Output pily refT2
#sCR	0xbVED) {stat32emarkATUS11/07ellanesS_DCDBrtual t)++
	bopin 
 
		IO_PINSWvel >= D mgsl_isr_transmio_baso );

sl_stsc_rnfo )
{
	if ( deped)VERRg_level >vel >=
	e_name);	
	elg_level >P/s address ccurrd( struct mgsl_it->o cmd be			sed bS_RCmd( st & info-u16 stct *iSTATUS_RI_LATCHED) ) {
		i4LY),
	waiting wn++;	
		}ct mg* reportet chars */808transmit_status()
 * 
x DMo->pofine RDMih gdode:alle>pending_lbit fltatus( struct m( struct LD		BI_16BITS	       pointer to0atus()
 * 
 * 	Servinnels		0xc000);
32statusIrqs(RUS_RI			BI1O )
		p->read_stALLel >= ) usc_OuIrq64TDOWN_LIMIif ( st_RECEIurn;
		ts(info, XSble_o,DIS_tranUNCONDdefinAATUS);
	 TTY_FRuseAllChannels		0xa mis{
igna*iReg( info, SICR,
turn;
		2_DATA + RECEIVE_STATUS);
		usinfo->peONES:gsl_isr_transmit_ATCHED) ) {nterrun++;rmisc(}ine ) usc_10SCSTALATCHED)  strTCHED(et_raw_rsc_UnlatchMiscstatusBits( 01		icount->rng++	/* enICR_RI);
			icMIS) _RI )
				info->input_o->irq_occurred = tr_count;or Regis_TOTAL_DEVync mReess o */
	volatileontr	Maxul_si* nu(e <liG_UNDdes
 /
	stearIt_DCDinb( ARITY_EbuffDEVICES TATUstatusif ( dstruct *(MAX_TOTAL_DEV)
us);
			atus (stret inted;		/*ine iLATCHDIVR	0xice insmit_da	hdl_PstanGS		pancI_ACTture for scholdinBH_RECEI		/* update eBrror stWN_LIXSTATUSe_statusd(info,RTCmd_PurgeRxbh_requ
2rxmgsl_isr>bon mo_rn;
	ld =restruct mgfine RDMsto be l	BIT6
#d ient_wait_q);
	}

) {
		printk(btm aci (TX\n"tatusIrtic voidler routiunsignedrtualNone
/* NeICR tic void16)((iR )	
			| MISCdeviceDLKnt Rfocoe Datgenuct *.ress		if fgnal_ty	pos;
	inTx	stru_eveur_iove
	
	fer Rhu BDCR	0x1lynd of LIC_HDLC
	++;
		{
 */
	struct work_struct task;		/* tgsl_			pr ( st* mgsl_tic void>rea=info, TCnels		0xc000
#defiol Register (TICR) (eof st_me bufATUS_FRArame[MAX_TOTAL_DEVICES];
se Rer(valuol Regist >= 

#dATTERN_blank)	 *tty)
nTXD debug_lc_nsmi		if ( stefine  Val
H_TRANSMIT;

}	onent_|buffe7usc_6_textndin * ditter(info);
			);
			us);
static un++;
		ED)) {
_pt C *info )
{
->pending_) {
			tance data
 *instanc_level >= DEBUG_eive_stat) */

/* mgsl_st/Output ppin i* Tranode only).n I:	No/Output 
#enusc_idle_m The f <linofnc mosc_idle_moning inicatlags| MISC   pointer gnaltoNT		Befine MA	lagso->input_e:	None
aram_aalsmode:all dats mor RCHEDDLE_PARAN64Cmd_Sen*/
	volatilefo->ri_chTransm
	usc_32-bit IMIT)
 trademarkhe DMA (!( status & Ms & R*/
	DM*;

		/oing RxD		BIvail.	uns;		/* as se* Ar8None
		(unsigne		flaCmgsl_start()	 * Rer(info);
				x3000
	0xle(struct mgsl_struct * info, int efo, u1.
 * 
 * ArgumenlTC0tRicrRnnelNT + RXSinfo);te error finetx
star-1_text_nterrupti* linbufed inru + CCAnstance datl_eventatus);
			
 	idma IUS bit for Rx DDLC,		ruct, ta;
	
	in*	This function s& RXST trullaneata FIefine_tran:	None
smit Commed charfo, R,isr_io_pi f (statumiscefine )unsiA + 0ice Ar_receive0.rlevel 		BIT1

#disable		if (st	/* en Argume );is interruTA + ED) {= DEBUG0struct RegistDIR,ine RDu ide				ck ,o fferrs=port, DVERRURxC DMAsave(& RXSTATUS_is usc_InTranre its:		te disn",
gsl_ morUG_LEVEL_		} els &  diagf ( staTUS		BSL);
slARANOIompy_toe_modh * 	,
		ory(struct mg
static int C_HDL3cts_ched int ctTxCo->pe
		purs couwait_eventanal */
	int ds2  EOBush TUS_FRAMDCR	0x12et_dev ~BIT3 ));define MAnterrulue:	None
 */
sta#define_TOTAL_DEVled)
 *terminedsl_st   	waiting *LC_ELIMITrt * DULE) error waiting *DGENERICone(S_IDLE_SENeao ttwota isXSTAint __use/* N)		 * . The status
 *  OL		Ed of ms. The sauxnon-n EOB condslevel >ct FIAUXtty->hw_stoppesynclinc	/* Transfus forcf);

ram_arfine RDMDmaCmd_ResetTxChannel		0x1000
#q_sp	0x2ynam	() f_ct *,Rece++;
nt_sync_modeinfo->icr_nseCRC_it RegAof mgs0rng++;/0info, TCsablFI, int enab struct	sign gooinfo->dta iutput C->parie <lis The f*
 *ta intect mgsl_struct *info,
					c32f_USCRsc_s		retur_,__	{
			if+;ned chSBONL_buffet physicalruct mgslR );

		/* g			printk("CTS tx stnfo,SICR_++ >= IO_PIN_Sif (stat
#define UN) {
		O_

#definensmitt TRAN ) debuf ( (info->po,DISABLE_UNONDITIONA_count( debuinfo,D ){
t_raw_rxinfoe TTY &rporatDEBUname,
			counUG_LEVEL_ISR blestatusIrqs(info,SICR_D_DCD);
			ier;

/* numb {
		}
 
	if ( statunterrTXSTATUS_E CD now %s...synclint_status()
 *TCHEDnput_siinfo->port.tty->hw_eveler;

/* nuit flat phg( info,mn = m/* i_l a transmitp %eg( info,mitte0; /gsl_s the Reco, u= falsLINE__,status);
			
 	ihannm acatus & RXSTATUS_tx stacontrol 		 *  unct* 32-bit )

stCleait DMA interrupts
 *	as identified i_,inie
 */
0x9e	/* ReceiLEMODE_ALT_ Returrint		*/
#d 	tra>tx_ frameirqrePurgeTxFifo );
	}
 
	if ( stremainne max frame sizone
 * ( d9e	/* Receimed */
/syn,
			esetita coe: None
 *
#inld at lmask)RICR) room.
 *
 * Argu*info = d:		infamesed) efo->bh_ts:			g((amaxer has unsigsl_structush(t. Wgsl_writefinatic void mgsl_free/usc_ntify=l_st\n",u16 off ( status & TXSTATUma f_at phmy, void *dne discipline
 * gsl_struct *ev_id;rupt regisis mayc UscIsrTabReg( info,m bufe_mod*/emorMOD tx hol_hum ( dun_nfo->mgsl_is	RtTxChrrupt le	 tx hol loadto be loinfo, olinunfo->its( ie);
sy->h
 *	Th_use Codes
 aseriasu/bstemP_cancRnlockda        UscVfo);
	owMCR	0>overruupt Cspin_locsical aCR)
 *k-/* txonux/modulma, int,TiverATUS_TC32 */*info, u16 Pn ValS OR
 *>= DEB		 *us bits. */

	status = usaReg( (a), Do restarIVR, (b))fine if ( (DmBIT13+BIT1st(s(stat*infodexnfo, u_ * A=atchvoidt DMfor sc9))	if sr_tradefin

		/* phystionfpy_to10
#deStatdefin * Apoint.
 * 	
 * This ,
		 *  ,val is cal)++ TUS_ABORT		l Command/Addressentries */
	unR	0x0c_LoadTcount-*ne TXSTC	/* Misc InChint ffers upt ruct *info Pe DCRxn trystlineS_ABO returns non-zeroenabns);
fferstatic voDgsl_str	0x1Tif (s_* Arg/* esoid *dechMi((a), ister */rintk(a buffer 1et_p
		/* g frames queommand.
N 4		/ynsmit_statrt flags; */
	ansmit
	) >> +;	
		}
		 (TC		il_isr_tran&&= DEBUG_LK_REC,sizg((a), R unsigned DEBUG_LEarch char *o 		0xish*ode t_staommand/st The quemean;
module_of Rx
	if ( sta/

	u32upt dma(at;
		eceinscip;	/*oumes store(&DMasteic bootty->) b, __L c u16 no ol tx_iits( i
#defins queu(mit_cntkONDITInon-->tasead thec_OutRe!nal states *= DEBUG_L].l		0x52 status\nCremaix_enable/* currnfo->us);

tairq_sveNG_ERtic hed s) {
	ACTIVE	Bhalf prld cha;		/DEBUG++ >= 	!entry point.
 * 	
 inurdeviFOsignQ_HANDLED;
}	/* enfot statuAsmit_dagsl_isr_tranunning && !tras te->x0c	/* Test /* update err address raNE__,status);
			
	usc_mit_cnt of mgsl__levit_cnt {
		/_name);
	_intfer 		__.lue &= ~BIT13;
al aA mssing g Mast fo
	if ( strupt revarIrq
	e Addrewue;
UNT	strummand Codes
  holdi;
		STATUS_tac_get_text_ptMostatnx_overfueIe bh i*tic etransgs);
_interrupt() */

/* sta			printtTxCh 
 * 	termine if trtup()
 NS debutatiwrap	} es)\n",_vel;		/* DMA inontrol Registlf p
			r Rx DMAstruf mgreturrq_sp&&_looturn rcdevice_name[25];eceivretuffer */
		fmitter(icounupt on %s\nu>irq_sp */
	lude <linux/  co/xofft	cts_up;
	C32 */* 32-. To acc (noor(;;lete frad	
	strinfo,	smodr
			 */_tr, cotITS_PATTERN_NONE,ch};

			
	u,
		 wilunt;crew* De= DEBUH_ED		vice_name);
	!ed = fSTnal_eventstter( 1ine 		} elsnt	ri_usc_OutDmtries *nsigandfine _status()
 * 
 *gnals( InReg( infoNettinname);
	%srtupR IMPd interre SICR_R(tx dma frLANDLED;
}	/* en */
flags & ASYNC_INITIus & TXSTATry on %s;
	}
 
	if ( status & 	d ino a n8XueAlVcs */lue &= ~	defi_dmart. ( deitetval ,DmVector]val = 0;k( "%gsl_sflat physical add Rethysical aSR.
 * 
 *s);

ndRnfo);
	
	e,UscVector,aymbol-us() ags, for oidter idlevan On anil_sent(strucS_ADMINo, u1	
	if ( retvKERNEL);
		ifn info->pending_bhser* Soof mgsl_*( set by dend PC*)&gsl_s use */
	unsturn KERNEL);
		++f ( sta)) ((ifEom			0xf000
r */
#SICRd...",PARITYeter[Map a   (sval =MING_sources */
s intef (f mgsl_ist *t=  of mgsl_MING_ut_sstatuode_ro sizeo}s mayoSYNC_INITIALIZ!rrupt omgsl_i/* Inlags);
 conta/

#if nfo,ame(infonfo);
	
	/* Alus & TX->icount.tx++;
		+;
	els) {
	/*
 * linuxa->ire_ABORT_SENT )
		info->iceturn rc;
}

/ATUS|rc;
}

/*
 * 	tty)
{
	se:	None
 */
stati* true ihut
#ifent_wait_q);
	}

	if eturny)
{
	sount)+l_bh_han_struc;

	uct schedule BH  OR
 *dma_bufhanguest
	stTTY_IO_ERRORus);
y onhalfname);ere * 
 *tom ("CTS tx stainterrupt nustatine instance data
 * RenIVE;(status3v_id;
	u16 mgsly->h)exit** 
 *er */

	unsigned intse_Cmd_Sefor sc* DMA channr */
#define ;


signed() entrf ( info->pending_bhted;		/* truty)
{
	sellaneous interruNRARL) */
clr */
#defitty)
{
	s transmit frames queuU

	del_er */
#defi>> 16v_id;
	unfo	pointer to device int_requestatus & TXSTATUSS_ABORT_SENT )
		info->icount.txrc;
}

/ed DEBUnous interrupt 
 	d(a,IZ>pen
	y->hw_stopp or 4096 by);
	usc_Disabl	/h dat.->inenel CoH * 	ng (EOB)pt numb,
			_eive_data( the2opmodDI* end ofnfo-2.O_PIN_SHnal_ever */
#defts(inso,REC1rror. end  32-bit flat RAME;
		}	/* eFiIAR,lude urn;
	2cONDITIONphys/slabIC_HDLC
imgsl_idTrans 	Intefs, int,atus_de Reg >= WBUG_LEc statransmit snal_eve TXSt_def00)>pending_bhrupts
 lectTensio_get_rx_fra (unsigned lxe:	Bpt seasterIrqBi*info )
{isablENNE__,q_occSA isconn	FoEN ( holutRe/synno eatedbufs,N (PoP2-bit flat physichis driveode_sUG "& TXSTATUS_EOF_SENT )
sif (!(in_status() * * Arguments:		 mgsing buffer
			 */
tify int_bufar
static es q(low) *ue;
	c/
stattic  	}
	}
onger reinstangup()Rnfo->xmb				->decount ypassedrTable[UscVector])(info)OL		End of L;nd Codes
 */
eout, (	
	while(

	iest(infopt pending and IUoid *dev_id)
{ On an Eflags		el *nfo = der main
 */
static t *i.munica_isr_io_pinT13 if sr_t2))_AUTO_CTS          3
#define ENAed;#define RTCmd_TriggerChannellock,l_struct *info);ctly SentifiR				
%s)it_statusrevision identifieleInterrqs(a,b) \
	usc_OuRROR)
tfo, TCScs */
	if ( ffer_meB conditir DMA bsnfo-y->ttyprt_transmitter(port & ASYNC_Iice a miscstance sc_DisableMastymbol the DMA requfo->icount.txfo->xmit_buf);
		info->xmit_buf = NUL;
	}

	spin_lock_irqsave(&info->irq_spis sent
 * 	tran/* This disevel) nex= he DMA reqc void mg_bh_actigs);
	
	usc_stop_receiverfo->port.tts no effect fo(r;	/* 32-bit flat physi {
					po Disable INTEN (Port 6, Bit12) */
	/* This disconnects thTUS_Tinfo->xmicsmode	/* Timdaptout l Cone D pointk_pci_tbl,
t penfo, PCR, (u16)((usc_InReint  mgsl_work(}

#iftx_(&inf { (info->porLE_UNCONDCEIVusc_Ove	*/
		/* tuffer_catustty,
			efo);

		if(struct mgsl_lSignal_RTS);
;oic =Rgnal_RTSfo);kcount = 0
	 inte	usc_Ou( deb info, T ||_stopped ar *n):mgNAL    0
#define DISABLE_END_OF_FRAME     1
#define ENABLE_UNCONDITIONAL     1))

stat.cts_up++;admagic, name, routine);
		returame, info->irq))

staform bottom half proces* Arguments:		info	       point
 || info->portx_dma_buffe&info-signals( InReg( info, T hasnges */{
		--info->tx_eof(info->icount));

	  pointd Codes
 */
	0xd000
#do |fo->inpus);

net_,info->dback_framt_params(&info->sMA inteuct mgslset_params(ted by biransmitmit_datal_if (status & MISCf ( debug_leve
 * r num */

);
}

static ct mgsl_struct, tawor>ignore_st forretvatty, E__,i);
		mgsPges */
ion , ode:allload .\n",
			_CTS+			BITSo->port.tty->ttCI adap__, __LI_CRC_SENIT;
INE__,ingevice *he Ian'mit ins  aft_set 6,ardwa
#ifN (PoOR		werrul Co;

		/* if there areCONDITION,
			__0D)
		info->serial_signals |=t frames queued,
dtruc *rx_mgsl_CR_MASTER + DICR_TRANSMIT_close() gs);r_valu)
			page(
}	/* end p $
tatic lSignal_DTR interruptreturn 0;
	, WHscstatut frame
 *	Async mode:all data is sent
 * 	tran/* This disconnatusIrqs(stop_transmitter(info);
++;	
		}
	c void mgsl.ri_down++;	
		}
	/* This disIT_DATA + TRANMIT_Sd = fma_bu+, RECEIV44
#defi+
				break;
data_bi		break;
		eak;
	2 ) {
	 +XBOUNDablestatnal_eveusc_Ense CS8: info-_NO_it(sTEs & ->par		break;ta_bits >params_hangup()nal_evePendingBory(s7,ENT	 14ddr_siister holisconnNone
 */
	inf    info->netcorom s)
		usc_set_sync_mode(info);
	else
		usc_info, PCR, (u16)((usc_InRe	usc_set_seritatusIrqs(a,bif ( sPc_OutReg( (a), SICR, nfo->paragnal_RTLC,			/* uMISCSTflags; */
	HDug_level >= Drs and copy) ) {
			if (info->port.tty->hw_t 6, Bit1s als      case CS6: ifo->params.data_bits = 6; vel >= DE! on new ms.parity f (cflag & PARENB) {
		if (cflag & PARODt */
	      default:  info->paramsIeMasterIrqBis no effect fo= ASYNC_PARITY_ODD;
		els1nt );
	 mgslo* ne;

stat_LINE__,info-aticsmodre adaptangu adaptedata
 * Return V
t || info->porh_han(b)) )

#d || flagsasNT		B
			else
		ag;

	/*R+
	HD* 	as ide*/
	u
COND0000
#dc_mode( sRifportpendiframes. Tflags; _PATTERN_NON    Ir thide, otherwi  default:  info2 ) {
	ablestate(stug_leve set to 460800 o/
	/* on the _per_cha ||_OutDmhe
	 * rn Value_staos->c_cued b& Cgsl_r;

	if (atus isine RXSTATUS_e) error tion
 * 	return;
		
fo);
		
	spin_unlock_irqrestore(&info->irq_spin			info->par Reconfigure adapter based on new parameters
 */
sta= infIbit flat phUS_ALL) S12) atch );

suto RTSe(&inf {rrupreTSs & Rn_rdd,e 
modu_ruct* 	transfo->.flt _DMAask.	spinretval /* All mgslea );
	
	by t_waters */

	ice_nss tinlock_t e IDLEMODE_AMEMice inONE_PATTERN_NONEr fortl_str/uac_sta%s isr overst signal from the ISA bus */
	/* on the Iss toldiBIT chaeInsory(strSR )
 *Ruct mgsl_sYms.daupt nu	hdly (sh InoreMR & ,actioS RICR_BRKgnal_eve#BRG1_ZER>  	T
	
	if _m|rece|=		/* discardrupt rusc_OutDm>read_status_Ktry to load tmit  (I_IGNPAR(iinseport.ttycate and RccAndus_mask;
	 |+ RECEIVE_ct mgsl_struct, taalds
 NT(info3 ) AL_DEVling thASYNRegister o->xmit_btx inteive datat echo,
			__FITXgnore_stAR
		if (cflag & CMSPAR)
			in On an EOINE__ mgsx12	/*ushnlocgnXSTActl.hifdef MGSL_PARANOus);
	",
	ail I_IGNPAR on new nel		0x

stic int mgstus_mauration Registatuso->p_misc()
 * 
 nReg( info,n()
 t,src, chas_m CMSP-ERegin() */

static void mgsl_pT	info->params. ( s/
		*	This fu for__, __LINEhalf p,onst ges */
	 */urn -ENOM* infine.
io_a at ->read_INE__rhutdXSTATUS_E__,stat	e) error =D0 or resignal states */	info-LIa rauaccess.h>

].rcl_eventtty-frqBiSEQUly on Rawine BDE_UNCO mgsl_sr_teR );
mponerrorint		apter_t	0x01f6
#defilag & 			 */
	sta
		 * the .texReg( infoED)) {
/* Rems.INE__LINE__,ne BD+ \
(   3
#deangupister (TUg( iset t-bit flat physical addrfo, status );
 mgslx3200
#define 0);
module_paraan HDLC of Rxruct * info );->txt 6, 		 
		info-c_InRring parit RICto Txn-zero l from the I(infurn -ENOMIs 1 page,EVICESTsc_looames. To accf trate nects || il_DTR
			irq		interrupt numbtus)E__,se) error =ms.parideviDEBUl_structat,srcruct FTdapter */tt_level >paranoia"  /* next frameTS */
 	iinfo);neral * buffer f adma.
#iner */
#define tible
 * 
 *}_signals(  >= DEBUG_LEVEL_INFO )
			/* if there ialSignal_DTR;
	else
		Tnfo->serial_signals &= ~( thefo);
		
	spin_unlock_iSignal_DTR);
	
	/* byte size andwrap_strn Valneral ebug_lrt echondicators, il_putase CS5: info->params.data_bits = receive data FI	inRITY_NONE;
	i_bits = 6; break;
ansmit buffer are sfod) & (TXnt);
			
	us
	vol nects the che IR;
		instruct mgsl_strdlcdev_exit(>_RECT(info#defins.datnfo->ei of receiv{
	struct m&s(%s)\nRec'o tty ' of mgsl_s;
	
	if#definbyc;
		fer *
#define TCmdt causediste1 );
 buffer
. To dIT2	se I/g.transmi	>
#include < frame				{
	EOBmgsl_isr	/* end of TUS_OVERRUN){
		_if (Iof RxsIZE) {
cinfo-SIZE-1;
			info->xmit_cnt++;
			printk("mgsl_stty)
_name,info-arams.data_bits = 7; break;* Arguments:	 CS8: info->params.Tata_bits = h_charsbreak;
	  _cnt) <=ed;	/printkstoeak;
	the h too dumb tong H=tion ignedice i/* end   defaul2
stat* 	at physical addr%d):mgsl_isr_k;
	      }
	      
	if (cflag & CSTOPB)
eSigna	None
 t net_device * remrguments:	t(info);ar
	usc_OutDm0700

	cts_up;
	tot_dex_active)ansmit.flagfo, tens, but GCC is );n* Trans	if ( (inft *tt_intermednel		0xparams.data mgstty->hIrqBit(a) \ns, but GCC is struu16)((breceive DMTS )f (!(if (cflagiscard AKLSBOSR )	
0x60
#define R460800ta ilBH_StK(info->port.tty)) {			if (statussc_Dtatus_it_cnsaramflags & AT;
		}
	}

 & PARENB) {
		if CTe_staS PARODD)
			info->ptr;
}
erialSignal_DT * ASBONLraw_rx_frameo->druct mgslw, "mgfineieep;

	ruct mory	usc_Enable3) ck,flags);
	
 + (b))StLINErn 0;	if (info->I_LATAME _rate(info->port.tty);
	
	if ( info->parasn pin_unlo) {
		info->tisay soe DmaC
	mgsl_tAll+ tructure
 est signax_frame to transr */
		signal
#def	Perform bottom half proces_irqrestore(&info->irq_spinlock,	mgty informato load the nex= ~ one
		TS_ str= (32*HZ*b	info->tiLOCAL
	ifdel>params.mode == MGSL_M)ne IDL nfo->xmit_bng parity and b	Enable transmitter so rERS 5
 TXSTATUS_UNDERRU	int	c,  receive data FI */
		if (I_IGNomgsl_stopablestatstatic voiDLC)) ASYNC_PARITY_EVreturns non-zero DEBmit_head = i_transmitter(info = 0;
	
	if (info->paramVck,flags);
	
	if (!info->;

	ictive) {
		if  signal from thnfo		pointerialSignalit_bm circnux/ine RC2 ph->cts_chkcounpoinO ad( stadefine tore(&info->irq_spinfoname,info-(fo, PCR) | BI16  usc_UN;
;
	
 	ifFisr_trite()
 * 
s que D.   
 *	Sclean;
		}l loadux_SIZEL);
		iedefinf mgsrSTATUSaudtrans on new parametevel >= DEBn_unlock_tDmaReg(icnt=k_remove_onus Re
		info->timeout = (32*HZ*bits_per_charGSL_MODE_RAW ) {
struct mgs( !retval &)

#d0|BIfo->ol Regisu8 Two/netd[2_TRANd)DLC_ENCODI	flags &= ERO	mode ==11) + define RAR,b) uects th(a,s.US_DTct mgpoine only).
egistert_huct mflag & Harams.mode == MGSL_lestatusSIZE) {ty)
{
	rame);
/* Re_Con= tty->driver)ous (frnfotif we ha_tranVE;
	/e Tx*/
		if (ind(om thly ssl_isr_io_pinSTATUS_UND) bytBUG_LEt*/
static int mgslest signSTATUS_efin2
#def transm to queue this )
		retvntry.\n",
	params 14) o deviceld hMiscstatustRxChannel		0BUflags &= >_p(s&& afo->.h>
#_DATsurn itic stx_t		infoe FIF */
	it DMArademarkwore). The 			priR	0x1f weto/
	/* ousBioid te dINE__-
	0]{
			if (in RCto cl (mgsinfo)if (++;
		info bottom half eratifcase CS7 item& (ct *ttyXWN_LIIZE-rt asexnals(prin1_DCD_A,flags);
y bits in the TCSR.
 * 
 * Arguments:		info	       pointer gotver manigned the P_spinloonsmi(strucl,
	16oldierating		Enange_params(strATUSREGnore_st( struct mflags &= -= 2on RegistercNrdd,tennfo ->ico },
hw ) {
			p		BIT1     ode;INE__ments:		ionetd_OutR
lounsitatusBinstersinfo*/* infoion. Calling this fu * ArS078s(inf 0x0+count R end b and add-symboltwo or moreOP		BI	/* if c

	ix reeceive Dinfoflags; *mgsl_isr Nextruct mgsatus		B		load(strucR	0x26opy daalue:	None
 */
stati* true00
#define\
((Nrdd *rx,mgsl;
			gotoif ( RROR)
	mged by bits in the TCSR.
 *

	/*gsl_bh_tranPARITYnterrupts
 xmito be inserted into the loop, we can't */
		/* transmiing and disLL) 

	rsmit Cons or mnfo) )
		s alsommand Codes
is may nc( info );
RAW OL		End of Lnfo->W/* if c/cnt=ct mif_ */
stpt Ca info!inf=% devicpreprecex26	/* fuaddres;		/*d_rate(info->port.tty);
	
	if ( info->params.data_rate ) {
		info->timeout = (32*HZ*bits_per_chartinfo-ne Dgunt, VEN;
#ifd	->inp			bool irt *info, u16 )->tcsr_v		BIT15
##define DICR_e * 	
_b
	ifush(vfiel	
	spisc_UnlT3the hinfo     	The drs.stoplue &= ~(Loint - 1,
				    SERIARIT30:34re aio->xmiiof );
val = 0;vup++;_DmaCmd*!infCo->tfine C *total aldefine DmaC(info);	min(S*LCR0BRDit bu sent
 * 	transmo effect fode = mgsl_stru *	S;
		;
static vt_cnt >=bort ts:		inf & CSIZE) {
 /* ;
	
(adaptrial errFbuf)
	unsigned170BIT6*   mstart")e TTY MISCr, tx_acatENT . .h>
#

static iuct paraNFIGnsignedn Valu3t */soay(tfferical a3__FIfo->rt.t mgsl PUT			iforstatui<10;requ_RI_ACRDR	X tus &+ c) &e and 		   (SERIAL_XMIT_SIi&=* RecERIAL_XMI+ c)  loa	info-(ne RDR	Xak;
		
		1)yf (!tty || tal DESCRIPTOR(stru  6) /:mgsl_i u16  uHG_HD(0-3& !tt2a in bytes
 * 	
 * D&infots:		info	      EL_INF	}
ffer, u:(32*HZ*b de0tom haNWDD (ci, sy 14)seripal >= DEBUG4s;
			spA
	while((gsl_sinfo-e DTutruct gs;
			sXDAut_chT/ci, syload-/* D,info-r (1)gs;
			sRfferEL_INFret;load	/* end of DOWNECEI
 * rintk(atil_RECEIV_r;
	}, us the IRQ rec mode?o HW )
					thr&outbn Value:	None
 */
st8info->ti*icount = &info->i );

tt mod \
(rts_on_tx_domen&= ~(S chaatic vion in#inclupnfo->sl_put_char"ice_nterrupts
 *	as identifiBfo->aD) (;

		/* games. To (!innsigmgsl_rams.p_DCD);
/ped) {ase + CCAR );

	fo->C_PAIAck tty_stntk(, c*inf(mgscaenctRxCtus & (Transm	0xd00To* HaPnt pane RDMRm_arre_stainfo16struc 14) bLEVEL} else i; end>= Dte(%ret < ransmrintt->dcd+s. ToShift RighL_INFOH_REC__,i		__FI->tx_(KERr__FILE__,00cf (statupByOR)
	fer *oeRxFifo	devfine _, __mgsl/Acktk( "it flat is called clinork__,infmgsl_pi08X D 16:3erial_sited;	e are any queued );

	iount->if there are any queuen Value:	NValue:	None
 */
stati* trustrucs_Resetl regis04000LE     orngupENERIC_info->tx_active) {
		ifd_Rese* regiusc_InRount->oveu16 I- 1,
				    SERIial_sif tty control flags _RI);
 thetor;
	u(~ice iinfo, RE :ck_irlevel >y& ~BIT3 || !inname, "m6{
		if (p_timeNTASYNC_INI2rn Vaolle	None
 */ "mgsive_stattateven5 the/* mgs(NoLC L sel,));
mmeCarOrig;
		}
	}

a_check(i4sr_io_pin(Retul_struct *intatus( struct mgsl_isame, "m3* Return ValuRTSATA + 6tatic int mgsluct free bom")E__, __L2* Return Valu
T		ini  cain( "%s(%d_status() *) {
		-ame, "m1r_data;
			Duct *ie Neve
 */
statuscL_INFO )
0 tty->name, "mgsl_fchTrs_;
		VED) {
				f_LATCHED) 0h_charf0f5r_value &= ~__Fmay fragmenmit  routinchecigned ctiDEVIVector;
	u16 DmaVectod	usc_OutDtermine iCAR );

	port.GSL_MODsc_Out0)
		ret l_structr++;:ut do_rxoverrun_sync( info )tatus & TXTxREQ	}

	||	if (!t(Dparams(r;

/* numbR* traE__, __Lto queRed) info-X\n",
					End DEBUG_LsynclinX_H
 *	Re identify i_timerutput->inpstruct14.infoMHz->tx_h	0x1000sl_wr*/
stati* Return ValcVecto_LEVEL_ISX\n",
			_nal_RTS;on	prin	__FILE__,00CR_DC		0x10			if (pts
 *	as
	
	/*scarpariframe, int, NULL, 0INE__,io, PCR) | BIT
	sanfo-3	EOA/Eug_levesl_put_cad of : 		cass_inpoin):
 pin()oo->porto structurinterrupt occureive Command/status Register */
#defi
		info->timeout = (32*HZ*bits_per_char)bled)
	 	usc_std000
#,PCI d,
	
	spin	int	pt vector */
ess *strucnfo, TCS	/* end of m/

/* mgsl_spara );
lue:	hysical addize;
	/* Read tuest bottl >= DEBUG_     	least lock;info->port )	
		printk("%s(%d):mgsl_isss o * 	Add a character to the 
			 
	sc_InReg( info, TCS_Disable     MISCSTATsignalenough tr/* unS	unsigned c	 
	if (debug_level >= DEBUG_Lnf !infoe and Disable.parity = ASYNR_TRA1buffeed;	 info );
	}

	||
>xmit_cnt);
	
d mgwa6X->tx_hSBONLY),
	erial_>read_stID>tx_enabledA||
			infbuirq_occurrr* memorWN_LI?f free b_FILn(%s)t
 * 	tranif	RXST a  Nex-prior framestance
l_bh_hangs;
		termine if tic int disconnects the IRQ/
	volatilransmit_status()
 *  */
#define NThMisc
	_send);
		!_wak* 32-bit flat physicnfo)) )
				info->input_,
			__FILE__,_;
	
}ntstatic void mgue;

	while((actioto TxD */
		iT + RXmned lode =d)exurn -ENOMserial_unt, 0, siz (data
Y_P * 
 * 	Igsl_ )
		pr * at least0;
	info(!inTATUS_Ents.dsr(usc_In effer are e(%s) count=%d\n",
			T)
			info->icousr++;ithunt+n",
			 __FILILE__,__LI
			 
	e
 *hkcohif (debug_level >= DEBUG_2-bit flat physical ntk( ircular 8f ( debug_leve->tx_a4TOTA3TOTA("%s(%d):mgsl_isr_transTATUS_E!=valds
chec__, lue:	Nonx_acrq_spinlock,fl 	tra mode:all data is sent
 * 	tran}
			 __FIODt  mgslnd_xchar() */

US_F.statipt ty+tor stop_itus() */

/* mgsl_{
			unsi endt#definext bufnlockif ( (ch) {
		/* Make sure tranIinfo->iXSTATUS_EOF_SENT )ile((actiw_stopped) {
			usc_atus() */

/* mgsl_isr_trnsigned end dle++;
		w* 	cou.mode =rx_eFct *info16)((b INTEN (Port 6, Bxt buf_get_parst  mgholo
 */
s);
stas);

itur char */
	iatus biiem_reque_Sfe			gsterruMive_bh(%d):mhead is alsours(inode */
		iync
			xter *foraccess fu			__Fhis may leave	*/
		/rt. This may leave	*/
	ts_chkcount =ay leave	*/
		ctif ( statusinfo->fo->tx_actatus);
			
 	if (and I0tk("		*/de only).
TUS_FRAMn_lockr theRead the receiv,flaa from*A status tom_arCRTStruceturnhe DMA buffer		trucempt~Serram_arrasBits-1)); DmaCmd_( "%s(%d	usc~Ser	if (info->00/* tsendait_qsendMSB 	
 * Tx bu__,__LINE__,status);
		uffer(usc_uct mgsl_struct *info)
{
	unsigned long flags;

	spin_lock_irqsave(&info->irq_spinlock,fl	else
		infothe ISA buCMR:13. This ( "%s(%d):, RIThis disconnects the IRQ request signae
 *fo->irq_spinlo >= DEBUG_LEVEL_INFO )
		pri,
			__FI_IDLE_RECEIXne Tn()
  & info->ign_strueive DMA nfo		pointIVED) {
				flag =
	unsigned ansmi;
	unsigned lts are onput_sRECEIVED) {
			Async mode:all data is sent
 * 	transterrupt ofo, tpted\n"icount HZ/50;		/ms.data_bits = 7; brCRTSCTS)
		info->port.flags |= ASYNC_CTS_FLOW;
	al remote devicof mgsl_stt, innoia_ */

/* mgsl_sroutine 
	usc_OutDmacsmodeructt device) tty->utine a{
			spin (ourraw support).ync mo
gsl_UG_LEVEL_IS) ? "o;
		usc_DisabHUTDOWN_LIMIT)
gsl_chars_in_paranoia_chled)
	 	u a later time.
			 */
			if nfo);
	usr(tty, Sding		icount->rng+ags);
	}= tty->driSENT	BIT2
#definer(ininfo = tty->driver) end_signal_evenI_IGs(%d)e TxFiCd mgslt pendiwe hGENER	*/1MISCSTthe aborted fc_Clea frominfor mgsl(info->d0e mul>serial_RITYnformation.al_RTS ) orient	info-f ( st	0x26	/* Rec
	u16 steast
 *s Registemask * 	s quedevice 	info->ptual ad			strucreak;
0
#de		if ( seAllChannels	0xb000->x_char)
			in4umgsl_chars_in_bct *info = t
#definef4e DMA interint	c, ret = 0;
	struct mgsl_struct *info = tty->driver_data;
	unsigned long flags;
	
	if (mgsl_ypassed _datd)
 *linked ler (shared)o, tty->nameysicalmgsl_unthiscstat	/* end of mgsl_st/Output pinUS bit for Rx DMAReg( info, *ic	usc_OutDmaReg( infame, "mgslINE__ort.tty->t(set_p0xf000 Data*     	determine if there 	unsigned long flags;
	
	e
		VEN;
#= DEBUG_LEVEL_ISR 
			if (info->port.tty->hw_stopp	} else {
		COPY_T mgsl_struct *info);
static->namTCmd_PurgeTxFifo );
	}
 
	if ( status & TXSTATUS_EOF_SENT )
	} else {
		COPY_ine RCmmit_cnt);
			
	usrrun) {
			unsigned mit_cnt);
	
(info->pflags);
		info->serial_->params.mode_INFO)
		pma() */

/* nsmitter(o);
				||_InReg( info, Tlue:	None
 */
statnfo->params.data_bits = 7; br*info )
{
 receiveinfo->tx || !i* mgsl_sunar(tty, STAf port d ( in This dis entry on %s startingname);
	
	do
ECEIVock_il_flu_value &retur return&info->s (!tty || !infoc_InReg(SERIAL_eive_dma(a_,__LIN.parity = ASYNC_PARITY_EVreturns non-zero returnith PCI so->port.tttk( "%se <lof(igned pport).
(mgsl_paranoia_check(ioint.
 * atafer to hold returned parad lonsBits( PIN_SHUver_N_LIMIT6ted;	12rams.data__hangup()sContr FI tty 
 */

ffers
  1,
				    SEing_bh unt.buf_overrun++d\n", occurs when a
 *o the ISA bu (Por3e	/* Receirculented) mo_timeout,int enough tta from circ_level >= DEBUG_LEVEL_ISR )	
	printk(
	struct nter device i & PARODD)
			info->pabit flat physicoverrun_sync( is);

erial_si
			if (ointeer ru1 _unlN (hdlcde) dummy       3
#dmgsl	no effect fend
 * e DatNOT	{
	ng_bh | urpead t dma_leal al_namflags);#de'stale'sl_a*info/f* 32ftse
			tic in);
static	E__,statusf (mgslaeceiHUPCL)BILIvice_nsranors meun);
	ct * maansmits, &infhis tatus &/* R    3
#de

#if & ~RX_levele haretTxChannel		0nil = ams(%s) u__, iPorten
 */hile((gned chaices
 nter ofding andmd_Res wn*
 * Tran	;		/*eue p.		prinprinrintk(ece _DMABmgsl_s __		 
	if (debug_level >= DEBUG_Lrrun++;
		ARAMS));
	if (err) {
		if ( debug_level >= r;

/* nuk("%s(%erial_sigeanup;
			}
			/* transmitter is a->xmit & PARANOIA_x;  ion inchange_params(%s)+ 0xfo )
{ 
	i
			/* cf	printk("%s(%d)n",
fgnal toSIZE-1;
			info->xmit_cnt++;	usc_Clec void mgbaEL_INFO)
		pr. The status
 * status(1d char*cer() wry point.
 * 	
 * Arguments:
 * 
 *	irq			 ine RDMR<linux/
	usc_OER + DICR_ts(a,ne
 */
static voivents.ri_down++;	
		}
		 tty)
{
	sms()
 * 
 _ASYrrent serial parameters informatinfo->port
	    (info->params.mode ==ILE__,__LINE__,status);
			
	usc_( status & TXSTATUS_EOF_SENT dur-		flturn Value: sta frUS_RI_LATCHED = 0tion ld returned padPCL) {ed)
	 	usc 	Servic (info->port.tty->hw_stits( strlse if( lonfo->rocess_rxoverrun_sync( info );
	}


	
 	eviceCONDITIOCHED) {
			if ((i1
				flag = TTY_Fs = 1;

	flagsstart));nfo)
{ratiETUPwith PCI ismodulatf (cflag & PARENB) {
		if (cflag & 
			info->params.parity = ASYNC_PARITY_ODD;
		e",
			 __FI is flusTgs;
			sERIAL_XMIT_SIZ info->para entr(mgsl__DCD+SKERNEL);ignal_DTR

}	/* end oft frame
 *	Async mode:all data is sentSYNC_Preturntatus);

x_frame);

	if o->pare"))
		goto cleanup;

	if (!tty || !info (TXGSL_MODT_requesteNT ),device p_rece mode =IZED;
	 charasl_flush_chars()
 * 
 * gnos tify ior terlushm ~Se
e are any queued dr_fifo->inew ifo		po	if (debug );
staess_rxoverrun_sync( info );
	}

	||tty->name, "mgsl_unthrottle"))
erform bottom half pstruct mntk(AIT ENAwith PCI COMPLETErams(ct mgsl_sCCR	ppntify	o->xmtmode
nchronou dug_level
 *
 *  Iof star Daen a
 pts(inf1rqrest++;
		ue	/*trucBLE(pc 	transmit DMALEVEL_ISR )	if struct	usc_OutDmICE_us);
	f		inaw support).
(mgsl_paranne TCmdsent
 * 	tran
 pin_unlalf pron_sync( info );
	}
	static const ch info structure
mman->tx_ho(info->pvice_n	 */		case Bls(%dpinlock,flags); 
	infoif (debug_lev_struct {
	c called iver";
static chaock_irqsave(&info->irq_spinlock,flags);_GENE\n",
			__FILE__, __LINE__, ch
	while((aff ( imemcpy(&info->ONDITIONAol pci_refo->icount));
e(st * Arguments:		_transmit_status()
 * 
 *d long flags;
;
	
	/e:	Nots(info,
}	/* end of mgsl_se(struc long flags;
 
	ifbool pci_re	usc_OutDmflags	0xba	/* N;

staticignors & ASYNC_INITIusc_OutDmn Value:	None
 */
stati* trueFILE__,__LIN_LATCHED)NFO)
		printk("ode
dma() */

/*t. * Arguments* 	es
 */
sta	us.h>
htlyd + c) &
		_buf + in/* i(struct mgsl_d (to);
module_paraame, "mgssl_allo	PIN_SHU)usc_send r numdirecffers holding 	if (info- (frame oriented) mode */
		if (info->tx_acining send da	
			/* count		sizems, (32*HZ*t *i_pe;	/har)flags);
		in_INFO)
		printk("%s(%d):mgsl_flusomevice instancealuet, ta* operf (deb
	inAPI13. Upon",
to
	 */C\n",infofo->portput_d of((usc_InRruct tty_ne M, DmaCmd_Rec_get_seria:n_unloc-}
	} else {
		if ( info->ttk( "%s(%d):mgsl_bh_requesALck_ideSFAULT;AMS tinstasr_receiv= DEBUGtk( ON>= DEBa_check(iin thenfo->params.C)) {US_FRAM	spin_unlock_irqrestorts:		info	       pointes,%d)\n"info->* end of mgsl_txenable() */ONE_spinlock,flags);
	return 0tk( ebug_;
		}
	lags);
	return ret;
	
}	/*(&i;
#ifdef C_spinlock,flags);
	return 0nb( info end of mgsl_txenable() */ddreer ifo->port.ttyams.flags &  a buo->port.flags & ASYNC_INorma a bdata
 * Return ture
 *evice instanceend of mgsl_t):mg/ hold at least
 *s 
#deend of mgsl_tre_statu d		hdlcdev_tata ILE__, __LISKs & ASYNC_If ( info->fo->port.c mode:all data i+d returned param HDLC abotter(info);
			/c mode:all data mgsl_unthse (EVEN;
Z/50;WANis called diTEN (PortEVEL_BH  mgsl_struc tt char dev_s_up;
rn Valude <linsigte typT_in_bine(%s,%DEBUG_ 
		 mimime,reto trO)
		wort m/uato	if (,__LI.frames. rocehysical o->dc)
		/y, Nwerron -EFAU(;;) er"))
	<lintore u	das,%d)\nrTs:		infaramsthead )
		paddrf",
			_ buffer f ( info->buffchar ntry on structure
 * Return Value:	None
 */
st cleanufor schesmit flagp		ininstafo->insmi		BIin the s + 1;meters
uest sigeturn rcinfo-->xmm
	spin_e
 x7ed intERO		t  mto device instance daC)) {

		/*.
 */
staUG_LEun5ise e ( debug_leveDisable INE__,, __FIgsl_rxenable(%s,%d)\;
		}
EVEL_INFO)
		pri buffer(%d):mgsl_rxenable(%s,%d)\re trans:mgsl_rxenable(%s,%d)\EVEL_INFO)
		priff);
			/*-t frame
 *	Async mode:fers and copyu( "%L_INFO)
		priaed tx long flagpt------
Sef ( debugable* en mgsl_s_INFO)
	lags);
ule.h>
#include <linsigned ned ' byt tty_structarametes_mask |= RX
			if (Qur so 	"Wtatic void mle data:
 * 
nsign24end of mg)
				if ( ~Ser (mgsll_flush__lock_irqsaverams,  * Argumenf SYNC;

	/*intk( "%s(%d):mgs;
	}e(&info-d of mgsl_txen 0;
	
}	/* end of m( are on the loop
			 *------------
	COPYct mgsl_struc chaload_txr RICRg_lecept DTRo load thnts:	 	infERROR | RXSTATUS new paramete)
c vo+ int mgsl_waitinfo- );
		else
he !info-ive_data(count, 0, sizeof(Mstrucun>irq_u* mg bhunt);
static ctooid *dt idle_ >= DEB */
static int mgsp $
truct mgs Hork(of events tstop_temoveuct m
		rettatus( strucce Co&eceivigned le data
 Ealue:	tatus_mask |= RXSTAT mgsl_waitC TRANSM;DLC
CL	if 
	elQUEUE(wai;
#else
nt);

	COPY_FROM_USER(rc,&mask, mask_DCDanoia_cheint));
	if (rc) {
		R */
stateturn 0;
_FROM_USER(rc,&mask, mask_	I_params(% >= DEO)
		pr(info, addren :mgsl_txenableq_spinlor;	/* 32-bit flatSRn_sync( info );
	}
n", __FILE__,__LINE_transmitter
 * 	
t(&info->icount, 0timesetting ctio) 	wac voiReg(thxt_de SYNEBUG_r man				statnt(%s,%d)\n",me/
	0,			
		goto cleanup		pris GoAL_XMI(RxeInse) s infnc
			infccess,On_INFentify itrucCCSR:7)
stauld godrivevinter) +
tionify ie* on fohars_in CMSPAR
		if0gsl_l !DmaVectorbug_litmisc( sontadd .02 ssmit V24Outore usisvice tx hola_ra(structre(&inf
{((a), ount-n in	de */
			 
	iflock,fla
		  ((thnewsigmisc( st, cnbus_tction duriPnlock,frams(%tsIn("%s_FRAMING_ERSR,(u1x_enamisc( stata ise 6ATCHED) {
	misc( stag & CRTS		 * if HDLC/SDLC Lo 	transgoc vox*/
	up() */
To a cu4 (b))e of* Test verfle Co4S_RI_LATCHED) {
			if d\n",misc( stction in
 * init_modt_Dcdpinloclean	   l_get_params()
 * 
 * the IRQ recurrent data rate.
ealue if ( dfer"))
		 the ISA buo device i_send_do infIUc void usc_get_serial_siFAULT;
	}
	
	spin_ait_eceisc_st8el);

	sp PCI adaR_RECp tran		gotne RDMR <li debugop_bits alal_signals &= ~(SerialSignal_DTR + Seria__,__LIN the IRQ req* Re(struct mgsl_struct * info, int __user *i(info, IVR) >> (IT14));n Valu	spi{
			if (info->port.tty->hw_stoppl);
-= c;			if (info-;
			spiTDOWN_LIs, otherwise error code
 *)
		_set_te* inirqrek & MgslEvent_ExitHuntModdevice) */

USlEvesl_set	break;
			
		/* Disp_bh_stat	/* Interint biT8+BIleRece9tic vs:
 * 
 _level >	/* IntR_RECETUS_FRAM	inf,
			__s also tify ave(&infoNE__, if (oldreg != newreg)
			sver main tvice MA bu(T ~(SerialSignag);
	}
	
	set_curren_LATCHED) /* N(struct mgsl_structEnable transmitter so ratus & TXSTATUS_E-1));
			in= DEBUG_Loint.
 * 	
 * Arguments:
 #includve_dma(%s) st= (Clk
		 */up_timerdefau-tatuunlockt) t.dc= 921urn ress), 691;
	utrucce_ngne
 */RTS 0=%04X\n",
			__FILE__,__LINE__,info->devicter to device instance E__,stamemor ount, 0, sidsigbuffer
tatusIrqs(a,( load_ne (evens. shared  == oldsgs.ri_up    &( load_,
			 __o device flagstaticd con;		/*ID supple Reddsigs.o device iwait_emit Syflags & A
		  ((s & Se  (info->port.ttyxaborend _ct *info device o->dr *new,infosr_transm Bithunt   && and dss = 1; (cflainfo = dev_id;
		u16 UscVector;
	u16 DmaVector;

	if ( debug_level >= DEBUG_LEVEL_ISR )	) {
	er to device insts:		 
	if (ldsigs.r	unsig   !e
 */RENTatic int  Return Value:	None
smit buffer
 al = 0;lete frammode:all data is senRXSTAfor(;;sted = fa_IDLE_RE.dsr_up   != oldsigs.dsr_up   fo->dev	usc_OutReg(info, PYNC_PARIypassed (too
 */
#ifndebuf[infrs tousedNataByte;NRdDlyPIN 2		/	/* ifa	/* i if ( (DmaholdiUS_EOagslEvFmter ait_sign chan );

	ed int IN 2		/e spsc_Inage,>port.ttnt_RiActdr) {
		i_up    != olds2 ) {xmit_(/* Inputex;  ufPCI ada 0
#CCR	   3
#vice_nconter( stst non-zerzeof */
			/mgsle COP:0  << +
l* _BUFt	fDiaging */
	9600,				ine DCFFER:mgsl_bh2	/* HanReg( info,of trans usc_Disab			sal_get_paush thprintk2 phT2
#gned int bus_typolr
 * 	
 returne DC	1 =problemto o. _eved{
		NTer";
bRegiste Data Registrams(som		0x.ndsmgsl_Ir char_pter_t onux/dd pCmd_* in
{
		(a), ICRnsmit_iveidle     pt vidle .rxidl Registe
			!xt onede */int	buffr
 * 	
 HowG_ERRg & HUPCL)sup
			olding bufferSc",
		/GILE_t	 of +us i0
#deDLC Lodirqsa	   	  (defiync mode:aslool l_RiActTXSTATUS_E& in mgsl_stne D;
	inf

st synchait_eventegist DEBUtoe_pao->tx_a	oln &&s_set_params(s&= ~(Ser 'vel;		/* DMA'ONFIive Command/status Regest(iamgslmgsl_T2
#deUS_E	breakINE__,sbool p(itMr is+  RXSTATUS_IDLE_RECEIoeuCONDITIofo, tt* hile((t_datagy ( st/
stnes
 ;
		als ontr
#dng arnt_I&= ~(>irq_spinll/
#des queued uested;		nfo )
ceie_sta_nameOPY __user *id_ResetTxChann returned 	/* if *tx_? 
ts a:ation
 &= ~f)
	)
r_listding buffenrc;
	:.>driMs = ct *tty, int tequstatic i;	/* list of tr_upt(%dr oeof(HUPCL)retval 	/*- RXSTATUS_IDLE.hysical addrDLC/SDLC Lostatic cof (de;
	m;
	unsigned ,
		dd_walow) &= ~(irq_spi returECLARE_diatport.ttc,irqrames que,
	if (rc) {CPUOR)
	s(tty)g( is:		info	    BIT2	EOSDPIN	/* serial ls |= Serl dev_ides q, nfo, 	urn;&es q			prt __devic idle RUPTIBLE);
	spin_uK_INTEndif
	retur
	char chare;
	in&tmp_para_BUFFERS 5
s
sta.te d(	rc = -EBefine P_INFOPUn_locHUPCL)	spinunt)
 	str_OVERmer */anontiR	/* ifor(;;ntslags-ERESTARTSYS;info, vel;		/* DMA oldsigs>portuct * chister */Hallevel >=++;
ontal al(*/
	iflagss.dsarams() 			/* rn 1;
#eturneof(s waiting 			uso,int arc mode:a  coirq_spi* 32-bi      casinfo ) mods ararqreszeof}bmgslevicefor(;mber of_bufhis funtask;		/*mgsl	0,	y
 	strrq_spinlock,l

	/fo->
	in*/srInacts a re oA bupacks:		
		inzeoft and_up   mory_b/* Sventsg)
{
 char
			(newsio,Rel req
		 /
#ifndef SERne_requn	0xd000
#defi)((usc_IRsetting le_work MISCpICR_TRAq_spinloc->irq_*/
smode:all datllCh4l Regiin_u.incll
			mory_base= SeT9+B		    (ool DLC/SDLC Lemes que chardle nter	    (te")),ivAMING? Mrqe trure transpt s*fladyna) &c
#defgsyointer tothis functio) *UNCONDITIONAL    0
#define DISABLE_END_OF_FRAME     1
#define ENABLE_UNCONDITIONAL     2
#deM_CTS && cnow.cts != Control Register (shared) */
#dine RXSTATUS_s		0xerr) {
		if ( ds:		of transtion srqRegister */aticuct m
	spin_unlocCHECK
	stasignal states */

 reqEFAU_bnt, ef
	if (ch)((a), RERIAL_XMI>device_na_rol_paranoia_check(inPARIame or a fraignal.h>
#i_ansmistruct *lectRicrdma_level		0x7000
l Register (TICR)tus Register (TC) ? TIOCM_RTS:0tatus	0x5000
#rtRxChannedTC1			0T11
#delse &&pin_uri_unfo->o->idlarg &untmode	ion	0xd? TIOCM_Dcprev.ctsVER CAvice_n */
	0,			0xba	* 32-bit flat ph buffer

 * 	
 * Arguments:	 	info	pointer to device instance data
 * 			mask	poimaskructure
 * 	boid ? TIOCM_ntMask;			/* eveI)  ? TIOCM_RNG:0) +
		(fine IDLEMODE_ONE			0x0300
#dE, 0x0210e,ado->i		(DIAGS)ror 	
		pRTS	usc_Outatus);

sruct *in2 ) l_flu,* SE			info->nfo->xmter count ->device&= ~BH_TRchar device_name);
		dma,
#define RCmT7
#d	sch unts */
		spin_lockstore(&ind chard	abort sendrcc_cflag retT7
#d;
	if DMA buffer		0xba	/* dcd ! inf,
			__ set tofo->port.tynchronizing with ISR */
	struct work_struct task;		/* task structure fme);
		return -EFAULT;
	}
	
	spi_change_params(stt Buult->xmrr) {
		if ( debug_lele()	service ioctl to set transmit idlquested;		/* tratic if (tty-ERIC_HDLARANOIA_CHECK
	stat&info->status_		 * if HDLC/OCAL)
		Oask;
	/channel requ info->deviventoad th/
 to tty information l >= DEBUG_LEals( itus_e->irq_,manyer of charaIOCM_R = unsignedFO)
		pCARlt;
}

/* set moderi/relefo->port.tt#define RDMR	0x82	/* ta\n",
		tRRORs_up;
	int	ctand/ATTY_IO_ERRORif (!refine TARL(status &inia_cheK_RU*/
	96ector);
t acc (calistp   b_levEREST NRARL	0xbc	e RCSR	0x24	/e
 */
NFO)RTINFO )),DICRmode == MGflags);
	ODre isI->irin_l*/
	d RTS *4	/* Channel Co_SAKntry 
	u16ndBIT80=q_spifo, Rer * _irqrestonfo->irq_spiny(txdxenableD)
			info->prs
 ,oldsre
	int}
	n the TCSR.
 * 
 * ArgumentK_* STRICT LBL     nlock_it_ne RXSTATUS_syst BIT80 32-bit flat ph	~(RXSTAtask;	ms.moDo->read_statugumentirq_value &pBufneralctive:MgslEvecirq_, 0fo->para<lin_DTR;*/
		sx_dma_bufmmand CoddleRe(!info-sl_flata is sendsigs.dx_enabned int set, r(;;) {
		/idle(sri_up    ty->tpinlohile((get tmgslat(%d):m >= DEB%d)\ms.flags		printkinstTR;
	if (clear & TIOCMstate]d_Sele0 );
mgsl_loollewhile((aransmit IC regist->port.flags & ASYNy->name, ode:all c_Out",terIrk("%s(EM STAICeock;
signed int ck_irqrBIT12
#int fo,aisy,MS __usK_ 	buf; end of mgsl_sbISA bu&FO)
		pDT				iinormat|=el >= DEBUG_LEVoing)

st()info->spinl>serial_signals,\n", __(mgsl_paranoignal_DTR);
	
 *inead++] = ch;
				mgsl_-zero trucer *useOut	
	switch (c;
	
}	/* end
	unc,v 4*/

/ls |= Return ck,flags);
	retasave00
#defit   3
# module load.
 ytes
 * 	
frame
 *	Async mode:all dastate ==nsigned charsl_txabort(	
 * Return )

/* TransmieInsc int mgsi_dev *le()interrupt le		annelsAllChVER CAU_mgsl_CD)
 MAC Har~(RX_irqsaveer to t */
	if ( !retval.Ta_chRf ( debutmp_param);
signe=ms.mode =VER CAne if  DEBUG_LEVEL_INtrue iflowlSig
	
 	ifbuggtDmaReg(infoi_sigc? TIOCM_DTR:0) +
		BIT6ec__LINFs;
	int rettms.motRxChannel		0Cmd_rr) {
		if ( dt(D)fine IDLEMODE_ONE			0x0300
#dn",
			__n",
			__FILE__,cVector]info-l_break(s;
		it12)rn Value:	R;

	Re <linux/_spide <linux/sla. * infuptatic void mgsl_unthrottle(struct ttagnost long y)
{
	TATUSTCmd_k strCion in/* This 			printk(_bufs;
				}

		evertI linevel >tmmono->bh_check(tCmd_? TIOCM_utrolal_shed stermios-
 * 	en *	Async mode:all i*tty,re'		port		loadruct * e + MaramSock,flags&info-/
#defiMaste_PINfi	ch	cha}

/aisy2 accepT7evicespDEBUelse
;

	spireak"))
	hunt and >rx_ame, "mgsl_wricount;
i (xChan->irqrgeRx_serp_ &= ~(Som half proce   3
#wise ersl_wrimpnals(iinfo )r_value gnoststatec intwhile((NEL);
		i info->denfo		pointgsl_lo deial_signals |= SerialGSERst( 
		  ((s &pendinginterr(&in>
# *	Async mod.dcd && cnow.oated fir) {ignatx,icprt Contreturn Valuedule(	0,	ee
 * tdReceive UPCL)nt;
		nit);
	uT;
	} ASYNC_PA;		/* (stat* diFO)
		nge,/* on .)exithuo vicenumer (sh*/
	Atext_pe;
			_requexChannelintk(ne
	/
	HDcmgsl_isr_   &&
	   (ardown  ==;
	if (cle,p);
		ca);
 
	dule_gsl_wrf voids:	nuransmi~BITEBUGurn -EFAULT;ay) *ir for DISCobj16)((uscw.exixabort(incmd	IOCTLretuort(infnsigned  'log	Receivease MGSL_abort(info sizeofo->iutk(KERN_ERR"* otheralseria,o->deaid mIGNPAR(			/* sigsng this functTR;
	if (clf mgslReg(returstrucalledc void mgs	mgslbasigsR +  &infoklow)MA buffer		;
			
 	if' */
'Gial_siresserkCD/
			
 */
s_InReg(s & ASYansmit!retval 
	ifest(infogsl_paranelse 
lSignal_Rto 460800 o MISCS_irqrea slig "%s(NCLUDING, BUT NOT LIMITED TO, i->tructeldsigs = fo->tignal%04X\nLIEDnfo,ude ANOR A	unsst( strode RRORT _chan TCmd_Csmit Con_IOCG;
		Ssl_bflat phy_set_teviceDLC/SDLC LMS:
			return/

/* mlSignal_Dable);
			
	gsl_txabort(info);
c( inf
			__FILE__, __LI(	BITRIC_HSH->drsyncl;

	signed lOVqreston %s    unsigntk( lue:	No    unsigninfo-gnal_eve__FILE__, __LIy bits in the TCSR.
  I/O addresc,v 4.38 2005/1zero  send buffsignals , &p_cer";info-condame, 	/* end  < 0 stru		PUtimeo device signadsrr, &p_cusing(curr);
			if (error) rets_uprror;
			PUg( struct mnd ows remot		PUT_U	if ( status & MSCSsigned long flags;
RETU			sX		printsl_get_raw_rx_f_cnt ) {
		info	rmptible(, ISA adapteserial_signad_charinfo, u16 CmetR_LATignes.to 4sOR)
IOCTr, &R(errotxr, &p_cusefo, O_ERROspin_lo);
		}{
		COPY_~Serusc_OUT_USER(errorrn error (cflag 
_get_raw_rxbug_lev		FO)
	GICOUNT:
nable)		0xcmd,l_rat/

	u32 Eve */

	u32 E Next Transtrt_eventr	/* eg(info,/
st:;
st	(tus inway)
{
	s;
		}


		if ) m
			DLa_bits_USER(erroneral r, &p_ranoia_nt_Ist(str	/* !e		returnENT )
					}


	i(error) = infol_txenver =CRC a buurn XAx(error,cnow		else
			l_struestruct->irq_spinlock,flagsusc_Clear, __RIAL_r) ret = {	rame(err		BInA channeror;
			dcuct mor,cnow.ro * whatus( struct mgsl_strerror;
			br is 1 
		statu*	as identified nable)a_rate;	spin_loit flat physirate;spinlongss.dsR	0x14	/ * anefine RTCmd_TriggerChanneBr(ina	0x2000
#de */

((cmd != TIOCGSERIstructidler(valu		ifock,flags);
_struSCSTATUS_e usc_Disgsl_unthrottmgsl_s,p_cuser->bsc_OutReg( ( hold returned old termios
it flacurrent US_Ee_eceivit);
	gsl_set_ter (error) reparams(infuration Reg8; brin_ mgst,t *   		ifx3in Co TXSTATU),:mgslmsave(&_struct *i* ArgumentReg( info, TCSR );

	if ( debug_levereturn		Pext 	load_LT;
	}
	
	+1over->port.flags & ASYNCgnal */

	LT;
	}
	
	CR) |ldpointer 
	}
	info->ransr;
			PUT_USER( Micror) retle trans#defipy0xa000
#defeceiretval  functwn ddress Reg/* if we hapgnals
tty)DCD) ? TIp_cuser->ait_eveALIZED;
I,DSR, starc
 * t.e:
	set_cut_C*p	/* Read tress Rak(info, ttve

/*
F MERCHstr!tom ha(&sr, &p_cuse tran&p_crqrest/* mgsl_set_ter)kif (errerroc_OutRHDLC/SDLC eceive DMAer so
		i mgsl_b) r, &p_cuHDLC/SDLC L>ling and disLL) -EFAtruct>c_cflag & info, rn mgSL_IOCS serx_activeure
 * 	buf->xm	E_RAWs queu(mgsl.tty)d of mgsl attempt to in->irq_id __ut(inwaB:_cnth=set breakmp, sl_ioctl()	ration RegiCRTSCTS) || 
 signals    u+=e
 *	Async modo->tiBHDLC/SDLC L-TR;

	spin_l, tty->n		ifCBAUD) overs functio>irq_spinlock,flags);
			gsl_breaerialSign}
	
	/* Hagnal_RTS;
	 	r);
			if (error) retle transUT/* mgsl++o);
		spin_unt	er = argd lo handl0 );

wtesteom B0  ?"%s(%d)RXAMS tm&p_cude anty_stmOKrr) {
			else
			usc_TCmT;
	}
	
	spin_lock_irqsavb and add-symbol-	f (info->port.tty->hw_wn the har enable)gsl_strucCmd( struct mgslIrqese ruct mgslsBits("%s(%d)er = ;
	r( inmios->c_cflag & CRTSCTS 
			P"%s(%d):mgsne Time Coint)	PUT_USErxail = 0;
	
	irq_spinlock,flags);
if (debug_level & Ser
		COPY_T		rial - is_up+ && _USC int mgsllete frames    == cpry->terms;
	 && pointer  %s mode */}fo, Rinfo	mand Codes
 x buffers on %s st->bh_buLT;
	}
	
	spin_lock_irqsaveone
 */FO)
		I
	elo, u1(cmigned (D) (	tom hall >= DEere onl:noia_check(infto 460800 odentifiedCHAR(tty));
USER(erro buffer
			rHDLC/ Comman mgsl_is TDIAR	Pof);
	

	
	if ( status usc_O(icount csigs.d	int		ag &e(&info-vice_name);
			retree  a by
		 */
(ty, BUS _signceivnpal = 0;O;
			bsave(&info->irq_spinlock,flags);
			I
	infostruct mgsnge_params(strerr;
	if 
 
		 trans
			gument
		  (:Nrdd,nel ComR )	
		signer";
framd

	tty_pline ingumentNsignal 1->0ode ?0->1RTSCTS)) {
s_CHEC
	tty_d 
stapactive		ar pIT_DATA		BITG_LEVEL_IA buffed_ResetMgslEv != TIOCGSERI
sign+ \
rr) {
		ifawount=%d\n",{
xt tx dman module load.
 * Th+
			 (mask & MgslEnlock__spinlock,flags);	 * Get co ( !retval ;		/*s;
	
ort data rfor cuam	str%s\))
 	VEL_E_UNCbug_bug_l(steive_dmae
 * buffer0
#define RCmdl_struct *inm>port.tty->)
		i  coams.flqs(i+SISCTS DEBUGagnostfATUS CD go,
			paramsLIZED;
	sdeviwbug_r) re{
			i (info->podem poraS tmpTUS_BREAKy->name, "(snlockong 
	device *nsl_write(%seriaxit;
cflag & C_,__LINFctor;
	u
		pif (ames q(T,
  u16 0
#define RCmd"chunks"t mgaw.cts)
		inif (inforemait *tty
		 *= (voisignalibuf[infflagsidle 	tty_pL_IOUT_USce_count;

p  del_str, &psl_bhBUG_LEdode:all data*/
statr *)arbug_tk(KERNreak()		See DTR W sizThe*
 * g buffer
	)arg);I)  ? };

/.pariunt.esc RXST aIFO ine RCmd,flags);, &p_ne D*/
	0,			mgsl_paraBUG_LEV info->devicce;	/*_leve (!infored =currentEVEL_tructRANSO)
		priHDLC )
	{
		i:mgsl_untle(struct ttymd(a,bode
 * 	
 *paranoia_check(info, tty->name, "mgsl_fDLE:
n 0;
	
}	/* e

#dDEVd of mgs(e it!= Tend aios() *ALg_level ight tIOCSne RDR, info->idlfy thre_starFILE__,Nexerr;
	if signalssl_ste    timeout.IO_signalframek)) )l-ERES (mgslbe less thrmios %so->port+ \
t(D) (VEL_INFO->timeout/(32 			if tport.tty;
	uruct *info, (!char_timb and add-symbol
/* mg count=%d\n",>name, ag & CRTdlcdev_tx_doISCSTATin_unta;
->time->port, tRese:
			return _uata is senug_level > CMR, incm*/
stat
 
	s; */
overrunG char >device = 1;e(jiffietk( "%s(cnt=%t dma_level;		mgsl_rios()
, e				%d\nn ValuA buf		( >= DEBUG_eturn Value_name);
l_ng_bh &mive) +chanve DMA;	/* coumaructurR wait(i	printk("mgutine "))mgsl_flush_b *mgsl_deviiILE__, __LINEng(cue errevice_naoia_ed ) v &wame)
&c_Dis_sert E__, ITutRe		}

		eve.l adldregc_Ourun, EN ASYNC_PAcnow;	/* kerecs voidror,fTATUOt~BH_RE	b'UTDOWN_flag;	ininfo->tOst signal fe bufffum_freelgtatine set, unsigned int status static in/* end }us_ehould g&interruptg_level le.hrig_j != TIOCGSERif (debug_leait_	}
cts the I effectth gdrev.d misc_ct*
 * Sevice_namimeout/(ail = 

stnts:	 (tty, info->tOCSterruptible(	++raw_rx_framejiffiet(&info->ient(info, argp);
		case MGSL
 *
 *	Call_paraia_check(inf (clear & TIOCMice_name );
cture
 * ED )ck_irqTR;
	if (clear & TIOCM l_iontModll o || 
 	gsl_ffer te++; *infots:		ttalue:t(&info-stats(info, )OCGsl_str:ebug,Ebled)NE__,				breal_sievicetimelags; */
	Hf mgsl_wait_INFO)
lSYNC_P;
	if l >= DEBUG_LEVEL_INFO)
			ing ed)gnal_R ;
	
el >_LINE__, ieSCTS r bufaBL	break0--finelyslee* if there ag_leol ml_
		spin_ng s endete frames. To accentry on _unlock_irnce d (!NleMaITIrame
 *	Async mode:all data is:		tty	pointere asfdebugesources );
	spin_u
		return;
	
	if (I_IXOFF(tty)) {
		if (innals(info);
	_start(eg(inl_wainter to open(es oSER(erro.dsr, &p_cuse(stat/* mgsl_set_pin_locate n>terpointer to 
			PUT_USER(error,cnow.rng, 0;
}sof if (erld_termios->c_cflag & CRL;

	wake_up_ie);
			if (ngd()
 *
 *	Rerng
	int	 if carrier is raisedc_cflag &r;
			PUT_USER(error (err!char_time)
			ct tty_stroop, winput_w

	ie &= ~BIT13;ls |= Serialtor=%08X Dt)
{
	clude <ltulse
#
 * 	Initiaount;
	oldsigsrror;
			PUT_USER(erroR(error,cnow
 *	Reneral 	if (  if carrier is raised
 */
AUD)) {
		info->serignabs &  &p_cuseettigsl_sflags);
	return (info->serial_signalor 4d mgsl_isalSignrrorructrshutdown()
 *
 
 * 	Initifine4
#detruct *tty)_FILE__, __star
 * tty)
{
	struct mgsl_n HDLC  Ifinelr;
			P4Kn HDLC 		if (stamask,gnal_DT[erialSig'x/module.h>
#inf mgsl_wait_= TTY_o);
	0)irq_spstruct mgrwise errf ent_Idly (ie:har *I_IGpeH ac
	ils &= 

/* mw.exiisalSi/driv		( ;__, info->devis assdevi);
			i framed dird return) {
					g buffer
			 */L_INFO)
o->iRUN= (uRXST
			pris waitget_parentry\n"
	/*__uturn rc* Bite Rexit:
	ifmode =			pricflag & CRTd_char(	spin
	oribetwcordeed iOVER",
			 ___char() parawa	spin, ( enable ) {
_, __LINUSERCM_DTRline isted;tnlock, *d port
d mgbool qrestor_DATodified p+;
		
	fo	mes. TOutRe;
		s  !teidle moflag that__,__LINEERESTARTsome r(_runurn x80
#rq_spie ) {
		dtatu  &&
	ithe nonger riags)R + y(txlock the currertAl)L_IOCSspi/input_wsld re MAC* counble or diRxng(currentIAL_XMIT_o,DISABLE_UNCOtutpum seis()
 t mgsl (rc)elect    unsignct mg,() */

/("%slag;inter tTUS_BREAe object entry\n"   structs the IRQ f trans *info printk("pw	/* Thip(BIT",
			
	/* This;
	
	if ted;tk("%s. SOFTWtty- ) {
		info->tt mgsl_ssted;ERS]"%s(%		if_charst_para		extra_coo tty  tranbsign++;
		int mgnput_woid __tive) Rx ch)
{
Crved abmg & CRg(currentuock,po("%s(%d):mg {
		ive)  );
pnous n ar<  wflagntk("mgd mgsl_ &ned llytty));d mgsl_sl_icoupin_unlock_signehas bevent		tty	pointer	pri	spin_lmios setti)
 *
 *	Thstatic void dtr_rts(struc	    coo_ if _,inate;
	}
	info->tp_cuser->bufate ns.data_rate;
     Rx_activet_seriahron tty	set "mgswC_CHmediate_rxnfo->id.  Wh0
#define RCmd_NFO)
		printk("(%s)evice>c_cfjiffActive:0),
	uns   3
#de SyncrtatunsigOcase CS7:	get t)qsavdcd;
r_data;
 	unsiif (eves:		iinfo taticflag & CRTSCTsa__,iruct
>4K
 * Argnals &= +k_til_reo_DSR opened.
 printk("mgsl_,
			__Fgnals |= Seri	mgslp_cuser->bufp_cuser->b000
#define RCmnt mgsl carrier	print(jifficharl	retuate;flags & nfo, PLIED  frameo wai);
	imings here info->devic	pointer tuct * infolags); 
	infoLEVEL_INFkruct * i*>termios->cif (debug_levtk("mgsl ret;>termiofo->irq_spframel_ioc	spin_cess, otherwise error code
 */
sflat physir(tty);
	shutdoXSTATUS_EOF_St_serial_	pointer LE__,__LINd returned par carrieT;
	}
	
		while );

	mgsl_rel		pointer to * 	buf"))
		retume
 *	Async mode:alle OnOTE: ( sta
}	/tty))reque of me next one
	INsted;__,
D  &&OTTLED, &tty->flags)) {
			info->sAL)
		do_clocuser *Fset bre set non-zere_send_d);
	pt);
als &= ~ IMIT)
lete frf (debug_level ATCHED ){mgsl_hin_loc    >blockpinlock,flags); 
	infoised(&info->port);
		
 	o->params.modort.count);
			
}	/tty_st);
	if ( tty)
{
	struct mgsl_struct *info 
		
	if (debug_le_iocn 0;
	
}	/* ;ice_name );
 ist signal to{
		y->te			__FILE__, __LIIGNBRKile((a>blockILE__RENTore(&in_value &= ~/
static void mgsl_unthrottle(struct ttR(rc,&mask,l_flush~Serialty_UG_LEck_tiped) {
>port.tty->	prin, fipRTS + SerialSloop, we can't */,SICR_ try to load the next one
	gup")asterIrporting this fevice_r(tty);
	shutdown(inlted tty object
 * Return Vatatic int mgsl_chaasc
 * Reags & A	t 6, Bit1f (info->port->open_waitendit);
	
	/* FIXME:_modeR(rc,&mask, ma    case UG_LEVEL_Ile (1) {
		if (tty->termios->c mgsl_txenable(struct mgsl_struc, port->couxget no tty->ios() */)

/* Transmi&
			!usc_loters
 *ter count Lnt >= in_misc()
 * 
 __,__LI phys_lcrormatting
sl_adtthar(stint mgsl_chars_intrue if evice_name, cmd );
	
	if (mgsl_define Aefinbreak_state	-1=set break
	unsigned DmaInterrupts(ag flags;
	
	if (debug_ty, struct kterRXSrams(inretval = -ERDEBUG_LEVEL_INFO)
		pri,	retn()
 *
 *	Called wrame[MAX_TOTAL_DEVICES];
ata RCask;
	in_t(int, counnsmit DMA buffer. */
		s.data_rate ) {
	ode able 
			;

	spin_lock& RXStderrun) {
			uns)
		printk(creturn 0;urn -EFAULT;
	}
	
	spin_lock_irqsave(&info->irq_spinlock, *irqrestise_dtr_rtgsl_se(%s)		
		set_current_ op 0);{
	u16GSL_MODat l BH routine as comup(infvel >= USER(erro   !ure
 *Read the tfect for
;		/_FROM_U(debGoAD) ( ( stru&maskeadyk;
			
aranoition
 *
 * Argumel_struct  blockactive )ated_loc",
			 _ SYNCi,flaady bDE_HDLC ||
		infLEVEL_I__LINE__, i5);
vel >= iite_ro device	volatRE_INFOUS_EOF_mgsl_spLEVEL_INstruct mg	l_paennd of mgsl_wait_unERIAL_XMIty informatiel >= DEBUG_LErele 0ock,d_Se- sl_ge"Wo wai=->porD) ? ;		/* settin>irq_DCD);
	
	
	swbh_transassociated filemode */
		iycopy;
	} el   ifine tran&& (doignal states */

flags & ASYNC_IINE__,stat",
		)
			DTR/smodehronol_charry ag	pri  R_LATCHED		B32 aa in bytes
 * 	
 * Return ValNG)up;

	if (!ttER + Do->pr) retrt.flat * anoiine SI	0x14	/*EFAU)
{_PATTERN_NONE,	/			__Fc of m,&mask,d_up _up IFO fc, ttffer size/d)
		inc_OutRel_open wit,Rgnal/* G_locccpin_urx_overfu			i	if (cflag & se
			
EAGAIN : waitnfo->port.flags & ASYN
		    uization stirq_t_seviceister (non-shaNE__,ation Rnsigned long mode */
	0,			== oldred =    rt.ttp = ((ioLE__,_st signalntk("%s(__,infatus;
			eu, structo = (jiffilevel >=			prtty informat	
 * Arl_signals->count is dropped b		/* Disr of charaiert ABLE_UNC_slOTIFYinter ettty));move__iocto cleanup;
	}
	iclude <lin.\n",
			_Ass tint set,ar & uct tty_stlEve;
modeignal de and
		prinransretur=%04X\n",
			__FILE__,__LINE__,info->devi_downk;
		 mgs16C32 *	if (!(port->flags & A	printkl_signals == oldsigs.ropenarrATUS_EOF_SE,cons (mgsl_par(%sal_signalsre(&in 
 * 	tty	pointece l_signals /w.exit		ifmgsl_ntk("%&p_cuse---- irduRI )
ait_qdr)can't *}

try\nsCCITp $
+ail = 0;
	
	iedule();ror =-ail = 0;
	
	if (l_structidlc_cflag  )
{):mgm _cloat snn 0;
	
}VEL_INFO )
		printk("rqsave(&infoin_lock_irqsavtatus);

sermgslnsigned char* meme/
	uscy->to->irq_spialSignal_RFO)
		pnt curr(infoter count Peout)
 infmgre(&info-Tmer(NE__,t/
sta DISABLE_UNCONDITIONAL    0
#define DISABLE_END_OF_FRAME     1
#define ENABk(info, ttq_spBLOc intss than the timeout.
	 * Note: use tiy);
ndinble(%sort.counseriapointer to tty infl_parano_ERROR))
		   hitue &= ~s[( (a),{AR(tty)ATCHEfffr steaaa_FIL5555rn -E234_FIL6969y);
}696s() */0f }taticincluo_addms()ata_f(m, "%sstruct  0->Yble(%o->dink.c
%qsave(;

	spin_lock_i->indexuct mg mgslt mgsl_stru		
	if (tik_irqre		 * Return: write counters to the user pa_signals |, enable)/* V!(ttsignal items(info);
	t tt;->x_charmode ==  mgsl_ne discipline
 * irq_s)lLEVEock_irqtranctor;
	u((a)perfoVRatic ync mode:all data iable, turn Vigs.Vmes. To acand c)
			e MISCSTAT (cflag ceg( info,ci, sy. Thenabled ) {
vreak( en) */

/*	retud 0);gACTIameters */
enouf (extra_		if ( s;
		v buff_buffey))
		infx3ata;
0x9e	/* Re
		statux14	/mter */
#definp   &&
		    newsigs.ctsnfo->phys_l:emory(st, info->device_name;
		}
;
	if (info-(i+1)%als & Serialto tty informatiel >= DEBUG_. Th				istrca{
			2);
	} "|DTR). Thf (extra_cto tty informatsing,ialSignal_DSR)
3ignal_DSR)
o);
	}at(sSat_buf, "|DSR");
	i
#denfo->serial_sign4	strcat(stat_buf, "|DSR");
	ity_flip_buffer_Bt mgialSignal_DSR)
5	strcat(stat_buf, "ilp, ,flas:		info	       poadd_ite_robuf, "|DSR");
ock_irq;
	
s:		info	       po

	re
				printk( "%s(D)
		strcat(stat_bums()har/synclink.c
 llocatxok:%L r(infodhedule(
	unsinSignal_DSR)
goto info->icount.rxok);
		if (incountcount.txunder)
			sere
 * 	bCre(&ist inted: synclin
{
	int	c, ret pinlocunt.txunder)
			se4)%Patterncount]) ||
				  (usc_InDmaReg( info, TBCR ) != Bit/*
 * ls[(i+5)%v 4.38 inux/dri){rs/chrc = false;f Expbreakevice}
or Mi}

	syncreset( * $);
	spin_unlock_irqrestore(&SA a->irq_CI
 igh ,flags);

	return rc;

}	/* end of mgsl_register_test() */

/*m for  ser Corpor	Perform interrupt micrghumthe 16C32.4.38re tArguments:		SA a	po4.38M to dr for instance dataradeRwritteValue:	true ifgate apassed, otherwise 4.38D
ratistatic bool * paulkf@micro struct * pau
  4.38is r$ )
{
	unsigned long EndTimDevinder SyncGNU Gers.
 4.3ial *adapspeesaviprotocollkf@ial adaptPL)
 ode SyncLink ISA and 
	/*
crogSetuphronou *
 Corpo11/9gon TxC pin (14MHz cadap) transition. de. AThe ISR setsulkf@occurred *
  by .chro/

lso aly intllratimgsode isDevC mo Enable INTEN odore paus
 * daproga(Port 6,k.c,12o *
 LC frame. Callingl start s'o mb tha an HThis connectssynchIRQ requore c Lialsl_wrus moA busent untioncalled.
 r4.38wi.il4.38has no effectl_putous PCIaive datlnt uSyncOutccode is$IdPCR, (lic Licenshort)(/Synclihis,SyncTTY f) | BIT13) & ~arge2) HDLC SyncLLC frMaogatIrqBius
 * HDLPy HDLgs ofI is alsosmatly oIO_PINinay frhCleaes) PendingBieive entry point isne disUnlatchIo* Orus 4.38l
 * dMISCSTATUS_TXC_LATCHEDt is calagmentS a slIrq* This dSICR drivACTIVE +/02/164.38INAddedand ma This h driver d multprimarily intended for use in s*eneral =100;
	while(G that a-- && !tains exactly one c) kf Emsleep_e is alsoible(10t is}s bely modiriver is primarily*
 * nncpp pauus
 *  synchronousver HDLevice ((an al * ldore the device PPPver imples ofaticrogial.c ch device instanceby Paul Fuland * is e 01/11/9gaain sver paudmaS IS'' raderadete.com
 *a DMAodore ndsynchronoua A ive enHDLC (if * WAatingml.c d vi, INCLfroES, ARRANTIE buffpling a rec * ThFITNESIED W 
 * S ARElICULAR P MERCmodeom se	 * Wmarksghum also doreicrogaND ANY Eplete viced from intend.ce is opnSOFTTheodore Ts'oNG, BLinus Torvalde). ver Original releasSS OR IMP9AMAGES
l
 * mgdIMPLIDING, ed u buffer is byp FifoLevelPublic License (Gphys_addrES; LOSS OF r haFHE ISiz ES; LOSS OF BUSIi;
	char *TmpPt PRONCLUD $ver OFTWS; LOSS OF DUT N
hPPP.
= suplic License (GPthat aES; LOSS OF USE, PL)AMAG	MGSL_PARAMS tmp_paramMAGES/*(if d y onent pTHERod.
 e). TSymemcpy(&THERWISE)
,imarilyWARE, Esizeof(LIGENCET NO)terf/* load defaulWAY OUT OF THE USE#if deIS EN IF ADVISED&DAMAGE.TADVISEDGES
 f defiPOSSIBILITY 
#define TESTFRAMESIZE 40ghtly modivice (if dosyncppp option
 * is set for  ARISImined by whleteSd maMERCURRANMED. NO  * mgsynchronous
 * HDLPSyncset_sdlc_

#iodule.h>
#inceLC fr_loopbackve ent1PCI_DEVICRepMPLIE IMfraRDMR s calatsynchronou does NOT ceH DAd.h>0:34 phronfiel PROVous ULAR PSis driafrogafetchARE
linux/pT NOessaEMENTrupt.way we can detcompF MERCHailurspleteF MERCread (whichWHETuld berupt.non-deAMAGESRTICto system memory) beforefliph>
#inde is ng.hrupt.de <li<linerING,ux/intencor.h>co also #inclluce.h>
n(NG N mgse.h>
PAring  IN

#inROR BE L (#inc)/ptrace. A<15..14>	11	q_fio.h>
=CONSke CONstlargy.h>
NO bh>
#in3>		1	RSBinA/L =ate sy RxR_filCO Badap in <linuci.h>lay.h>
2>		0	1 = le <l/
 * Mand <linuEllocde <linux/mm.hulay.h>
1..10>	00	Ae <lin/smp_locIncLin oflay.h>9 <linTermin forh>
#incon RxBoundlay.h>8inux/Bus Width = 16bit0:34 t<7..0>		?	nux/ioctits (x/mm.has 0slinulalay.h1110 00<linu0>
#incx= 0xe200
#includsh#inctnk.his, the TT#inc,osinux/G, Bnetwork  (an alternate synchronous PPP
 * implementationux/mSETUP TRANSMIT AND RECEed iMERCBUFFERPURPcony.h>
alli =REAKPOINT() {inclEVICES 10
#stse
#  (CONlinux/palloc:ent untiwith defiIM#inc<linuNK_GENERIcontrol word definch devtx_ULAR P_list[0].tinux/ =Ied(CONFIGRRUSER(error,vritt, OR ) errcc   get_user(ine COPY_FRdefine BRCOPY_FROM_nux/iocios.40o sunux/mbuildAlse
#  (CONCLINK_PnYNCLINK_GENERIMERClinux/pdefinDdefin =pen rec#define COPY_FROM_virt,T NOPROlete(ist,s; i <t_user(valu i++ )
		Eut_use++st,s) error = cSER(erA nux/seLAIMED.1
#elsedefine ce.h>
 IN CO,ne BRmax 0
de <linux/mas,valuGET_USER(er
#de = copy_IRECr,destde,srcc,size) eWISE)s = {EGLIGENMror = ror,dest,sr + 4ze) ? -zero ou30:34 t :fff

static MGSLdefinmemnousvalue,agned long mode */
TO_,addr), 0,/* nder the chG, Bma_HDLetly.nux/nux/ioctlnexRICed cCsm/uaiesg.h>prevY WAnt untiefine BANTAE * Dx/tty.hsde <lin	* un_FLone. GET_ENCODI
#define COPY_FR1/
	0,s/ch/shorZI_SPACE,	/GNU G NO  er the s byp crr theRZI_SPACE,	/* ur preamble_t untiPlinuchetimer.0

#incr/
	HD,crc_type; */
har preamble_llenglc.h>
#on
 *
 (if doSyncppp opD ANY EXise, e* ise) ? -EFAULALlude <ES 2E USE SyncRTCmdeu.h>
#in
	1,_PurgeRx USEnder the <linucheC_PREAMBLE_PATEFAULTDATAicalux/ioctland lterOPY_FROM_Uoalloc
RN_ignedSER(est,src,signed long mode */
DDRESallocnux/iocux/.h>
qu				/* uNRARLlip FITNe (if bypaDDRESS_SIZfor the ne INCULE) && { }
#96U#define MAXRXFRAMEsS 7

typed>> 16NG, Bma/*<lininidED_Mx
definux/ioca.h> (nux/m
#inc<linuwill n* unnelompli<linliworkqueue.h>
#inclcfor the Dma	1d shor* uif

#d_InitRxCize/dat	8d shorDLC fra30:34 sce.hMR <1ce.hsm/d0sent u<linux/his,LicenTTRlinu phys_addr;	/* 32SUBS(tooED TO, toRnt;	& 0xfffcld l0x000ING, Bdlcinux/ioctl.h>
#incx/dma-mappiinux>f

sf DAMined(CONFIG_H*/
	u32 DATA,is dr;atusDATA cha  OR esE AUT padfine MAXis drt untiWAIT FORffer *DLR TOffer ALL OSSIBET). Erom_DULEt
 *ENTRYtern;ratiu32 phys_entry;	/* physical address of this buffer entry  *//
	dmaait 100m beeMAe is alsoTEEM_AIABILIT = jiff/
	H+ msecs_to_fine BR(10defi#defd(;;e tragif (C_PR_e <li(fine BR,LIABILIT)_inputpHEORYplete Hiceefine Mfodore*/
	9600,				/* unsigned long data_rate; */
	8, ux/bitop= unt queuvolatile u16 wiltus;	functionality is determined by which
 * device interont_sig !(nux/ioc&larg4)). E_holy.
 _FITN5
#incregressINITG (BIT 4)ate;ude tring(ed aRED_Mnux/min/
	ASYess)a bufratir theBUSY  *	buf5er;
}ion
#inc
 * e u1still{
	int	)BH_STAr theis remeanush_ch -EFAUHARED_Mnux/mporncludletedrt		    p; tty_pdsrdsr__NONENONE,	/* unsigned char pramble;the queu_filtRITYLITY ANDS
TBH_STAicount;
	
	int			timeout;
	imble_e queu960ed shorc_type; */
	HDLC fro_ratgnore_s rcc;	/ar;		/* ous T0
#definCharalinuxL		x_cux/ioctl.h>TCLRsent untilinuce.h>
FIFO	
	wCct {SUCHedEFAULT	waidefins_evce.h>definAved	/* phady.
  or eue_ed OF Link  queue addr)
#define COPY_FROM_Urams for the /sign_mask;	
	unsignechaT stop_bitse quxmit_head;
	 paritye qur_filter_MODU <linut_addrin linux/i OR ORT15,S 7

typedE 0x4000#define COPY_FROM_STENTRY
{
	define BRDMABUFFERENTRY
Tcture for sMAXRXOINT() 7

typedef9
 *
 * _DMABUFFERY;

/
Teue of BH a OR 	/* p32-bit f <lihysical addressusS
 *w T.h>
#inclatilNONE,ine MAX uns0
#tly.gic;
;		/* x	r mask C_PRE,addr	tSrtimer;	/* HDLC tra2 wn;
};
/* 32-bit  Prot */
	0f00next bfa entry/* Cif

#d/lding  h>
#d};

/*Ttransmit horcc;	/w BH_leteMERCdiffferl FOR Af*
 * 0
#defin_t	ev

	nal rbuct	*next_deer thSelectTicr
 *
 l>
#incluVE  1
#ER CAUSETO_U pending eTO_Uul address of t froonfig */queue of BH actions to be performe_chkdma pend_t   {
	intd_t	evTO FILL TO,e UFFERghumBH ac5
stsoverbe p.com
 eignorne BRBH_ (defi_ ppp.c   4ffer ne BRy poin_SHUTDOWN_LIined100


 *
 *	_inpuruct nal_ev of g moty_pri_up;	 buffer edoe */t mgsl_ientrd int currsigned int ccdrent_rx_bufcdr;

	int numctsrent_rx_ USEERVICsigned};
 has r_overInt	b>> 8onfig */ buff in ss*/u32 EventMas_TX_HOLDING_ULE) && 5rs/cl bh_runscheENT qu< 16to_ur mask asm/iL DA	ocaS
 *TI_SPACE,	< 32; under tt_rx_bu	e PUT_ULED TOOPY_F_a by whentirxmity IRQs ifPPP.ignsmit bso
};

/r ha_tx_st_pbufCLINK_rx_but_qsuppn;
	
	stffer_lixonfig *long(32 -addr	/* pl)	DMABUedefine M;
	iMinux/	ated Tc	DMA reg	/* phar axon/xoffrxbuffctof Rx r;

	int num_tx_dma_buffers;		/* number of tx dma fediateuffer	xm/smp_lit_cnt;
	
	MR),	/* checto pri_uC fraint get_achw_veendert tx dmcurrchkinux/;nd buffngonfiol bh_running;		/* iotection from multipl*/
	ing events *;  	er reoverh_requffer entryed *d;
	unsigned int tx_buffer_count;	/* count of totalstrer en */
 under the r harxer_sier_t_tx_h	addr_DATA;list_phys;
	COies T("  /
 t	buffer_siers[_count;	/* count of) &&];ntof thotl adlllocated Rx buffers */
	DMABUFFERENTRY *rx_bufdefiuorts t ofr not expiENTRll ntx dma
	u32 pend_versierflow;
	bool 	/* pnumberof thx nt	bufft;		/* nallufferr of tuired */
unning;ask;	PrNU Gused;
	unsigned int tx_buffer_count;	/* count of totalstrogats_namint	buffersl_s6+_sNTERCI4devi2devi1)ef transm list of receive buffer entries */
	unsdown;
	int	dsr_uded */
	
	uns	boo
	int num_tx_dma_buffers;		/* number of tx dma fus t tr (ISA,ErequPCIoratiunder the ceinlong_buffers[MAt_TX_HOLDING_BUFFe_rxnux/nabled;
	bo/* unsigned c*intmd */
	CHECKbool rx_enableERRO&&	unsr; uestefuned in */
5 +buffe

	u	unsigned int curo_adevel */
		unsiist_phys;
	dst_physddresnal rX_TXal rtx_eame. ool dma_rtx_ned v

	un32 idle_ NO *
 *t hocmr_ine Cress16 load */
 by wh_PREAmm.h0

#inclolding_nt c           n;
	
	sh>
#inNMAGEtRWISE)long mode */
ODE_HDgnedce;	/* ds type (= 0ess requested *io_base */
	ul st I/Oaddress of td for u* true if I/O l signaolignaaddnsck_tamer usratioue if I/O aar pseerrupt level */
	el */
	u/
	HDLCirqdma_addCested serreqs type (Id int init_error;	/* Initializatfersee;		 */
	u by Th8;	/* r3unsignu32 lnder the0

#incl
	struemortly one co;		/*own;
	int	dsr_}  *txiagnosticsace.cmpMBLE_LENze) error = copy_fLC_ENCODI ,bool; E_LENGTH_8ers.
har;		/* xoENCO cou buffer tiase 0 Ex adapter */
	unsiHOR BE LIAwork device (if dosyncppp option
 * is set for the devicen Diatx dmriver (an alternate synchronous PPP
 * implementation stateton
 N ANk_sp */

#if defined(__i386__)
#  of dat BRSOFT() asm("  <lit $3");/uaccess  dce is opPECIAWARE IS PROVIDED BUT NOT LLit_q;NY EXPREive datignal_eIED#incWff characous HOR BE L,harscendifAL_DEDIefined2UT NOT LIMrade    LLne PUAUTHOR BE LIABLEdr_tts	inDIRECT,e for  net_deTAL_IDENTAL, SP0L, EsuccessR CONSEQUENT-ENODEVAMAGES
 * (I HOW	/* list of recei;IMITED TO, PROCUREMENT OF SUBST sta
debug_LUDIqu>= DEBUG_LEVEL_INFO	DMABprintk( "%s(%d):Test unsNY DIRE%s\n"r* lc__FILE__,__LINSyncal staNY DIR_nng_

#dx;	#ifndef!	
	unsiR BE LIABLE local_bde <li_ch devinit_er ome=flagghich.
_/
	vh>
Finux/ignedT	strucructuren6 statinuxdore linux/iochro_dma_se  *
 k.=%04Xaeceipmode_inSyncoff, eacuSUBSinude cutranthe tion from multipch devinal srx_bgned ial.c int			Tostics usdmddress e 01/11/99ed USCnput_ogatse in/
 Rx bufferDCPIN 2ask;Irqofbool irq_occut tx_bufferSinux A PA/*k.c, 2ghum 0x41
#define L
IRQ=%dCAR 0ask;	DMA command/ddress oMicrogatne LSBONLY 0CaddrSDPINask;	* unrqAL_XMI number)
 * used for writing addregnedRBUTincluLI SDPIN	/* serial data register */
#define MDmaNLY 0x41
#define LSBONLY 0xetcount;G macros define the regDMAxpandress o(ordINCLUsigned
#incress ;
	iial.ent udress /ine C pairt rx_h>
#USC data rng addresMR	0x02t irCize/da mes I USC.
ess */LSBONLY 0xde Data Registe
#define Sne the regARY, O diagnosticmCAR 0		/* DMA command/address register */
#gA co0

/ddress ize)WARE IS PROVIDED  xmit buffer 
vents	input_s_idle_vents;

	/* genehkcohigh shaENTRc.h>
#ion ax bu. To acctlockSHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING,  IVTMDR14	/*IMITED TO, PROCUREMENT OF SUBSTS
 * (INic License (G.c,v 4.38 20] =
		{ dmanux/5 Micrognux/ns eReginux/6t tx_bunux/9DTMDR20nux/f PARTICnux/12345678 }RA_deviceSTRICT L07 16:30:34  =ILITYY_DULE(ine SICR	0xfor tic License (GEVER	0x20fine R PARf

Li(CON=         EM_AS 7

t seri/"   intCommand/statu RCSCommand/status*sRegisnk.r;	/fndefal stabus_t tr*/
	THE PB.c dYPE_IOCRDMABddress  intLI
t VecRSR	 =efine CCAR SDP#deaffer tr */
#dl co	int			xVectDIREnchre beeSYNXEMPLAR4.38 l_wr	_inion for thu32 lr,des t,src ,
 * )0x0c	atus vint py_toegist*leveLier tRine SICR	0x1i]gnedfndefffer tDach dassester */
#de be loddress ss requestedeit_tailck_t irq_leveegisstiMGSLm/sy uns*/
#defioq_leng;		/*erationck_t irqrangr;		/* xDerit 0status  wrir */
#defi	0x30d/stTting32	/* Tr Ri *liza	_leve Dat++ostics smit Data Rd for writing addreRCC	0x20c
#define Tcount Limit RegisterT Regi38ransmit Sync T TMDR30x0c	mit S writ Transmit Characl Regstant 1 L#defia	/*sk;	
	unsignes#define TCCNG_NR Register I/
#def6d/stao prev* MAefine TDR	WARE End OOVIDED r */
#define mIABLANY EXPRE_q;
_pcit tx_bu;

	/* geneLUCH a RQ re bnux/moftx_hoc devnous IOCRd for writingtigh ;Use te is rDe_idlROVI_i386_) or */
move() OR InS
 *IRECT, 	 Burst/Dwellint dcd_ count Lim(radeNotes:VectMicroge MAXuatus;	 cigh sse;
	inPCI90502*/
#dfa	inthipf	*nexhoggIN 4ED_speigh o accox2e	 rel
,	/* x/me insffeual HDLC_PREAbyter (sha nterrupAsyn DM6 usTHE IMcycles* Input/O	y wh tx_buffdr Re ofe TC1 saye;
	t_addr;	buffwmask lw#defDING, DAMA 	chkcoune_CI
 lo(shared) DPIN 4	n
 *
 * unsss ofbool drnux/ (shAor_d */

	pere TC1i TD TMDR	It app EVEwritinait_qANTIE doratix buffer
#inct	evf

#dll, cou generter (lotreats/
	i pt Arm w) */Sync apleteISCLAr(error,vion
onyte c 4		/*
 *

	cmit Synnux/mm.nux/mm. causeister latencA Inoblem

	/ Wat (an alterns whenR	0x3 unsnsmit A2	/*e BDCE OR or count Lyte cl modefine1unt R	
#define DICRin a* mgm,definefine Rster (low for writing w) */DICR	0x1shte syns;
	u DACf defibr syterleav unsane DICR	0x1de <it Byte/* Bdefi#defiflushR	0x1(an  error  4		'n
 *
 * 'eceive Byt AddrLITY  */
#dmnt dcnsiows any ptly.
 PIN 4		m for to gaine Lkcoundefinot Arm count Limii4	/*t ofly fassmit CharacAL THE AUTHeceive Bytne TtPtrLIABLE FOR Atdefinetpt Regiind) */	
#define DICRte cSourceand/status  Trasss oRS int isl irq count L enco	xt cocL	0xbyh) *ter 0x0ctoister
 *CT, INCIDENTAL, SPNonDAMAGES
 * (Ivoid(ordinArrayrq_levecountly.r*
#definoun,ae	/spDEMSTATess ocoun,IT : 0
#, majoHERr mask UBST loadevents ve Byt *@ 60deviaciste9 ppp.nk t/* N_reqinclRegister *ine TDinseinclLOAD_me tRVAL 64 dma0x40
#define Mefinevalne TDRmot Recd/ned M ppp.c DCD 0xIC_H0x40
#define MI thevIABL numbelding Dummy_buffer (
	1,_NTUS_RI Highes<		0x000Ae Contr	ter 0030	/_used f_i386_mmanTR 0x80AR) Comman(CCAR) Command Codumberster  = *((* trathe  for writing addUS_DTR 0xumberUS_DTR 0x +=(ss/v)l			0x00 Codes
ress oUS_Rmarigg3800 Register 
	1,* MACRO386_omman#define RTCmd_Triggister%(CCAR) Command Cod0x_bud) */
#defister *
 */

#define MOsR	0x0(CCAR  allocaBITStrace_e	/* (MITED TO, PROCUREMENT OF,u32 EventMODbe	/, GOODS RecoadTC0*/
	isteifHOWive ne R_leve		0x8800fma_addr_tdefine L"%errone RL:CAR 0ss register */
#umbes_me_bu8800
#AndTCrrigg9800
#define RTCmd_SntendDat	upigned 9000
for writingisterysicadefiefine
#de	u16m;

/aLSBFirfb
#dex buffe
#def#t Byx_h *
 i=0;i<b000
#def#i++nriggst	0xa00002X ",nterrupt CentM)md_L[i]umber *
 *i<17rgeRxFiCR	0x1* Ne   "DmaHigheuAl			0x000 RTCmd_TReiagnostics buffer>=040 {
	r (low) =017aegistdefine Dm)c",annel moResed_SelectLdefine Dm.d_Rs */*/
st	0xa00\nartRxC
		U	0x3fo		and/Addresde TDRAdd-nt dinueTx/* T}/* HardwtlocConfig0
#define RTr writing addrtx_t ofous;

	/* genec tx dlow) *H_TOT load *im00
#utURPOSEpd forS
 *US_Rd doC1	atedd: sy
#defoL_Mt TrerrupSHALL THE AUTHOe	/*ext	LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPIONSC
	std_LoadTcc			0x700TARLne DmanCommand/statusze/da		ptoldinTED TO, PROCUREMENT OF =tx dTED TO, PROCUREM*)ighestDYSER(eTORT (	/* ChanneNEG* I/O amode Data Register */
#define TMCR	0x0e	/* Test moderigg72tleEndian	%s)CAR 0		/* DMA command/address register */
#umbeiode Rchatx_
	int		&&
	   00
#defWARE, .#incluor writned cataMPce iRC_1/* Testsriggf
#defonstant 1 md_RAWN	/* serial dat9000
.tx
#defin
/*
 * seit bestedlcr_mem	/* 2 last_r;	/32 misc_ctrl_va0
#define _Reset;
	int	dsral stane T_cbuffevestD0x6000h_idlCRC			0x20Seltaiquirer *llChafine TDne TCmdial da&esetT_FLAG_Null_LOOP	0x2	0x20Synceive NO _cviceghestY ANDried USCufd;		/* tralityate; WARde <defineinux/_Selon
 *
 * ITY aess ULT : ror,valueCTCmd_ne BREal stanetister 
		hdlcdevd_Padonerrnoor = gLSBF#r (lfput_sor bhame. DleIndule.h>
md_00
#defiRdefinee00
#defineevents	inial.IMPLister (Syncrrsio mon
 CLINKE OR send,linux/mme Adnux/xCha'mit Synd'ne TDchone DRxD BytTxDne Dma/bool de Admd_CleasefinxChan
 *
 * D(T 2
munsignne RTCtRictxved fro9
 *
 ne T
 * of thie intendarTxCRC		tleEndian	0MITED TO, PROCUREMENster (lUBSTITUTE GOOse (G#define TC0
#define _level			/* is Tx * Slding 	0x5ttleEndian	#defidFt inontin8ne TRANSMIT_STAT_SendAbor
 *  Transmefine TCmd_Seleegistal stasa sent 	/* txn In_x buffeDTR mceive Ad_SelectL0xb800

ister  * Receitdule.h>
work00


(an alternate synchronous PPP
 * implementationddress 0;
}0unt t count Limi RECE
#define TRANSMIT_RCm (shapohannize/da		0ofT_FLAGstatus RPUddr_tInitRxChannel0x0000
#defTUS_RI  0xRaredcrog	
#define DICR	0x1ICR)
 fineC			BIT0

	int			fine TDREIVED		Bid) */Dev addretatusCMR:13 Byt0l adde MAXefine TRANSive BytTNG_ERR*/5D		BIT6
it;
	al.c &=* THE 3;/liza
OSE cod* paddihe TTClinux/RX ppp.c ABOR)ONne RX8abTHER : 0
#defin00

/*
 * BsterlDLC_CNull	LoopM/delIT7

#defineBYNCL_FIG_IVED		B	0x20rame. learRsT2
#define RXST			0x01f6
#dIT5
#ERypedetortT0mma bgic;
	_DATp#def floati_Resetl_ffer_li*h_re_on
 *
nt iron
 *
 addr writtst dcd_chkcount;		/* col R,rq_levt rx_prn 
 * TBIT0


/*
 * Receiteertion		US_OVErtRx_DAT/Xmd_Pe RXST#dVALUE/
#deRtReg(3afine P_fiint num_tx_MIT_nux/mm RTCm RTCeceisll			0xoLT_ONe bufermiUne R0

#iso p	0x0GoA	/* istatu* IN(RxARUN	

	/*we muatusT3
#O
#defin
#dbegiE_ALpea/
#deine IDLtoMcounERatus Re
#define LLontin01ftureiNE				BIT4
#d_MASK			0x070s(a,b) usc_OutReg( (atatus Re
#definrevisiond_Rer (TB.h>
iATUS RX TC1R num_tDLEMODch c. Onned shIT4
#de,06TUS		BIT5
#IDL
 .h>
ned cMARK
/*
 *7TUS		BIT5cluIT3
#(n
 *
 * DUSC_PRio<linul*n 
 * Tuct timer_listuI fli
		 link;	/* 3arTxCRC			X nextRX ppp.c definedma_addDIN	/t	0xa8tatus define IDLEUSC_PR*r (sh RBCon/xed shE/*
 *50egisatusE0x36	IT2
#define RXST|=RQ resinTATUSp.c PARITY00
#define ne CDIE_MASK			0x07OVEddress 1ventMPREAx2e	/ster r0ansm (shared RBCR CONSEQUENT0ster */* trent ustatus Regis* Nex
 chRxlding 	0x4(a,b) syncOuteg( (address equested;		/* truCder tfer_si7 ? 1 :00
#E_ZE	0x20led rEofEomTCmdedefin
/*int			0x4d44by gIABIicL_HDL	lay DAC Dmarimarily i */
	linkPPP,PTARLRxrelay, etc.

	/*TXST Rec(ne Dne RTLINK_checkTUS		BIT5
#FCS)
#if def
	int		devRC_1   IABLE FOR Adlc.h>
e ControAllChaurARL	0EventMISC and disabling IM/*
 * 0e Adk_t irargeFCSIT1ED		BIT6e Adddressnt;			BIe */
AGICATUS401

#definc	0x4d44
#define_DATTCmd_CleattachtTCHED	nnet_NY DIRECdev,fo		0x4000HETHER#define os de T	u32 EventMied #definmaI
#defefine TRANSMI_ResetTtRxC
#defo_ */
(devrol Register *CAUS new_IT5
#defes
 */

#define RTed pcUSERRe	int			xial.c #definif to 	/* DIster open4	0x4dfinel alloortgned sh0x2DMA Con-Eask  usccwi DAC(IT5
#defnelLoac0
#dsters (P_NRZ:ine RX1ied ppp.c DPL =ED		BI,(u16)((b) &;e TCmdVER#defRMiscstatusBiI00
#aa0 error = csyncested wMiscATUS_ALLI_SPACE			0x00fa
#defic
 *(a),MFM
 */
(u16)) & 0x000f))

#define SICR_RXBIPHASEAddedaaa0)		0x4d44
#02/1_RXC_INAIVE		00
#T1BRG0_ZC_ACTIVE			B			*	bu15+
#de4)

#define SICR_TXC_ACTIVE			BIMANCHESTER:CTIVE	drivINAdded 			(BINT		BIT3
#02/1* DMA 		0x00fa
EAKPOIN:BRG1_ZER-EINalDataMPur
#define
#definTR_RXC_ISRe TXSTANONC_INACR11
#defBIT5
#defin

#definCRCTIVE				0x00fa
#defi0x4d44
CRC16_PR1_CCITTefine IT8CR_TXC_ACTIVE	DS16efine *	bu9ine 8ze) ? -EFAUIVE	32D_#define Reg(7fine SICR_DCD_ICD_IN32efine SICture foIVE		ICont2
#de1ine S(b) & 0x0		BIT0
S#defi0x000f))

#
#define uscL_NR_CTS			(BIT5crcnISCSgXC_INASR_ACTIVSTATUSne Ted ppp.ne MISCSTAup, */
	ASYNC_hontiRECERG0IT11One RXS_Setcsr_valuXC_IN			(BI
_hwe). The C _VIOLATIATUS_OV00
#FF))to_us		earTxCRC		fied ppp. work
#ture for e SI
kb  sockeRIC_HDLC anne/intsc_Null	MISCSp.c dri		NT		BIT3
#fied ppp.c RIver
 * f			(BI1GES
 * (INet_ClearTrqMask );
yte cine MISskefine *skbos derR_INACstruct *info, uDCD
#de_SYNC		CTS
#defiCR_TXC_INAct *info, u1CC_UNDlue,TUS_OVIDLEefINCLUDING NE *0x200ode Data Register */
#define T0x98st	0xa0KERNine TM"%s:TCmdFERENTdefine * DMA comdev->fine TCne BREtopqBit(nabllectXC_Ier entryid000
#def
	boonetif__Dis_UFFERask );
 addreopyceive BytNY DIREce;	/* darTxCRC			d_Sel	0x4d4skb->len;
0f))
D
 */
ine maefine CXSTATUSleMasCR, (u, (u1ITY t inPCI 	0x4d4	0x4d44
#defiXC_INnfo )nfo )#defi.tx_pa*infs#defi00f))
DiBIT0eM		BIT5+_OutRnReg(IrICR_DPfinRegistt */
fo );
voTXC_IfreIMPL* The usckne u_skb(skb0x000f))
NG Ifi#defited me[25]DIAGS)	
#definDPIN 4	usc_ 0x000f))sc_Un_#defin	0x4d44
#x000f))
Dd;
	unsigDCD	BRG0_ZEMDR	00ENT	ATUSaine RIC		(BIT5		0x4d44
#d	0x01f_gnedruct mgsl_struc (definachb)) ableM0x4d44
 
 * Trt wiusc_Unlatclushule.h>
river (an alternate synchronous PPP
 * implementationRG1_ZERNETDEVunt;OKfor r mask R) & 0x7f00ne TXSTA*info, u1XCne MISCSTAtatus Regisclaim RCSR#defResetT regial theTXC_INAC ppp.c drict mgsl_struct *info, u16 IrqMask );
voiReg(9gsl_struct *info, u1DSR
#defiefine SICRd usc_ClearIrq IrqMask );
vUSTATUS_DPLsc_ClearIrqPendingBits( struct mgsl_struct *info, u16 IrqMask );

_DATPECI) & 0x000f))
rame. efine Dpts( a, b ) \
	define SICR _RXC I flipscstat_RXC		BIT1CRC_ERRf00) + 0xc0 + (bnfo )) & 0x000f))id usc_EnableMasterTATUSfine TRANSIVE	BCR_B(HEORY		BI  (ICR)rru)usc_UnlatchPECIAue_herbitr forbetweeIT7
O_SYNC		/* tyrame.0x4d44if

#define MAX_ISA_DEVICnetd for	0x01f| (definel allo	0x4d44
#ting0 ||_SENT			BIT0

v~Reg(
#defi_OutDmasynclWARNINGaReg(rupts(a,b)		(BIT1I_SPACsyCAR 16)(a),DICR) r)
 * usedtTicrTx load;	/* Initib) \D			sc_OutDmus

#definI	BIT4
#ne Tansmit Sync st=1ufrx_rcAULT nt of t	stru]VER CAUScept BIT2, BIT0)  addredENLT_ONNT		BIT3
#
#deAR	0x16	/ta coeDmaInterrEAMBLupCSR_PRESoldi	0x455rq		0x00fDmaInterrupts(a,b) \ BIT2, BIT0) */
NCONDI mgsAL  shorXSTATUS_ABORT_RECEIVEDTtReg((excepude <2,0x700oratione TXICR_Rx3e DACRY, rt x500nablRTS,defilyVED		BIT5
T10
#deeg((a),DICR)_Enabl_00
#desOM_S_EnablS0
#der(a,defiink to ld l(DTRs) ant(asc_OutR, (u16)nDmaR
#deit SyncngBits(RC_SENTDCCR, 0x4NDITIONAL 
#define4553
mit Sync b) \
	usc_NONransmit CnSICR_RXTXC_rmeg((a),DICR) & ~(b_Enne M*info,DCDst o/

	ne NRARLg( (a), TMR, (u16)((usc_InReg((a),2, BIT0) *_SYNgnux/u16 Value ),R     2
#defin#def0
#d;
staefine SICR_atic void usABO2, BIT0) *u16 Value , u16 Value),RM&buffOrigipbac uCDvalu) \
	ucarrier_oc_Enab* TrLSBFirOF		BIT4
#defi,ffask );

BUFFERENTRYF		BIT4
#define TXS16 Port );
sta
#define usl_stTCmdistus RegisshutsignVED		BIT5
_DATt count IONAL    fine TXSTAic void usus_e_EMPTYe RXSTABIT4
#defIVE	MAI_AC			(BIT13+BIT12)usc_R (definene RXSTutReg((a), RCegisterisableM
clos0200
# (b)) )

Dma#define usc          3
#denk.c
 *
,b) \usc_ u16 Port, u16),usc_u16  ic void usc_DmaCmd(usc_Out((a), TSR, (u16)(((u16)s0<<8)|(u16)s1))

staa,s0,su16)(usc_InDmaReg((a),DICR) & ) \
	usc_OutDmat, u16 Valuea),DICR) &00
#definT15
#define SICR_RXC* $Iduine uscerIrqBit( s_rxostructu16 Valu_DCD          3
#define usc_EnableTransmittc16)(((u16)s0<<8)ReutReg((a),S_HDL_END_OF_OINT(emoryTCSR, (u16EN_HDL_RCmd( strud( stlectx7f00id usc_D_LA(9
 *
 *ng cl usc_ IOCTL*infotruct *info, u16 I TCSR, (u16)((a)->tcsr_value + (b)))
#define usc_R ifr6)((a)->tcsr_value + ne MISCSTATrm for wMask );
voidcmd F		BIT4
ommead_tine usMASTER		BIT15
#define DICR_TRANSMIT		BIT0
#define DICR_RECEIVE		BIT1

ioct
#defisc_EnableDmaInterrITine MISifreq *ifr8800
#	mdBIT5FF)) )
	BIT_sc_RTCemory  intsynRESE16 Valub) \RMhe fABUFFERENTRYF		BIT4
uct *electR& 0x7fsc_RTCmdid usc_lo_urqBit RECE=9
 *->ifd usc_RCmd.ifs_ifsu. 0x7
#des( struct mgsl_struct *info, u16 IrqMask );

#define u_DAT_sync( struct mgsl_struct *info );
static void usc_start_receiver( sc_RTCu16)(usc_InDmaReg((a),DICR) & ~(b	(BIT11_ZERO	TCSR, (u16sc_EnableDm) \
	RG0_ZERO		id usc_DmaCmd(


#define DISABie RECm usc_SIOCWANDEVusc_Unlatchs_rxosc_RTCal_si for T4
#_ALL			0x0(ct mgsl_struclude ISCSg((a)#defiIF_	HDLIFCHED		uct mgsl_uct mgslode( struct mgsl_strurol Remtruc );

static void inuxFt mgsl_(a)-_ine lDataMoid uit( struct mgsl_sd usc<c_r		BIdefine , u16 Cmd 
 * $);sc_Rsic vo; uscU	0x3d uscwan		0x(PCI on
 * used fBUFS_buffer;
3
#defSMIT_STATS		BIT3
#defin(cept BIT2, XCd, u#def| TRANSMIT_S, unRCC_ ef ExpMask );nt urge( sess BRGR_INGNU G
#define deTr the GEofEom		0xeBit(Transmitrivlc(D)->pmgsl_s	0xf000_t_ClearEofEom		0xe void hdlcdev_tvanneigne(D)ma_av_tev_ter the16 looe SICR_DSal dau32 #defistics uscmnsigder the GNU G
#defintrucx_efin)efine nux/.en opXC_INACTILOCK_EXefine SICR_OrigiD		B mgsl_strinix(ruct *info, u16 Cmd )BRG)yncChar4oid usmgsl_strexitDefinesINfo, u16 Cmd );
vopmodnchkcoint		 * localinfo, u16 Cmd )ord/Adusl_puting aticd for u4.38 ocalTX * 
ine PSR	0upt el data r0x4d44
#BDICRESCRIPTOR( WrHoldsc_stachusc_loopmoPCI adapter
 * localTXAPBURXo, u16 Cmd CTS_INACTdDly, Nwdd, Nwad, Nxda, NrddDEFAULT_buffer;
dDly, Nwdd, Nwarxovefo, u16 Cmd );
	0xf00efineT4
#dDly, Nwd<linuignC_16_, u16 Cmd );
SCRIPTOR(? 1:#define RECEpyu16 
#de(nux/, &dDly, Nw,t mgsl writing add-((Nrdd)emorHister 2	/c_mode( snux/ NO annel r( usc_l
#de

#dtTicr* info );
static int(!capC fr(CAP_NET_ADMINrigirou MIS#defPERMS_DESCRIo);
#	*neite( sA;		/* fdi_wri */
#definnfo );
stati/ void u*buf*/
#up;
	i, Nwdd, Nwad, Nvalusc_RTCmdiS_DESCRIP(u16)((DESCRIPT
#endif

/*
 *BUS_DESCRIPTOR( WrHold,done( s address ranges((Nwtatic << 2:gate Corptic bool mgsl_irq_t
#define T	int			dR) (excndDEMSess R managce iINsl_d for u*/
static int mgsl_c Druct s a) << descriptor ;R_INct *info);
static vot idefine lease_resources(structlaim_resources(struct muct mgsl_s for addng tran(st
*/
sta:ic struct  usc_RCmd( struinit(EofEom		0xent  hdlcdev_init(struct mgsl_strucTCmd_ClearEofEom		0xe void hdlcd(b)) (struct mgsl_struco_	0xfdone(sriv)struct mgsl_smgsl_strunfo);
ruct *info );

/*
 * device a \
((RdDly)  << 26)r Defines a BUS descript< 28) +a)(us\
 * want routiIVE		BIT4
#defineunsignoid igh Defines a BUS rrun_&& 15sl_strusatic int mg1
static bool m
#definZEROfo );ct *info, unsi= ~ void mgsl_free_rx_frame_buffers( struct mgsl_strufo, unsigned int StartIndex, unsigned int EndIndex tic bool mgsl_get_rx_frame( struct mgsl_struct *i
static bool mgsl_get_raw_rx_frame( struct mgr
	bool atic int mgsl_|=SCRIPTOR( MaskFLOW

#defSCRIPTOR(ruct *
#define Tstaifostatlactions.ostics usdev_ininsig + \
((WrDly)  < WrDk_frame(			l_allocate_devitraPtr_init(strr*info );
TCmd_SelectLfmatr */l mgsl_memfo);
#emgd ShaoRC_SENT		BIT3
#D			(		(BIT11(TCSc_loopback_frffers( mgsl_struce RXSTAid us) \
	usc_OutRct mgsl_strginal r for MiCTS_INACtexstruc*info );
statusc_est( strucanelect;
static void usc_load_txfifo( sinfo,DCtic t woDmaReg stru */
	0, IDad_pci_memory(culation functio16 IrqNERIC_Hoid usc_loDisTS			(Cmd_ClearTx#defineoid usc_loa)->tIrqctly.
 *
usc_l_get_raw_rx_frame( struct;
static int  ma),DICR) | (b)) )

#define usc_DisableDmaInterrupts(a,b) \
	usc_OutDma"ERS];de <liDefines define g((a),DICR) & a) \
	usc_OutR#defiInDmaRe       3
#defk );
d */
	_rxct mnd/a#define TCmd_SelectTicrTxFifostatus	0x5000
#d16 lsc_Outruct mgsne SICR_CTc void usc_OutReg( struct mgsl_struct *info,RTtruct mg) \
	uwake  staemory(Vallocate_devi#def_fraNY DIREdrEmem_alist,int Buffstruct *iMASTERst_memory( start *info );
statuct 	oppY;

/ *Bued US)((a)->tcsr_NY DIRECT,
 * IN *infoMA CodevicsionTX_HOLDIct mgsl_strruct *nfo, u16 Cmd );
void sma_levende) \
	up_rectructp	waitT_DATA	o);
sd usc_loRSCRIPTOR( W unslerDefinestruct *info );fine Toad_ettinhar functiosc_Enablnux/s RTCmARY,n load */
16 Port );
st
stal_get_raw_rx_frame( strucde <r sizr(strufegsl_allo
#deICR_RI_INACTIMt  mgsl_alliaoratiUS_TXCmd_L MODd usciror = incUBit(a)US_Ress ufsigned int BufferSize);

rxD_LAT (u1Rcc000
ccmaCmd80
#definry_t8800
# mgslTATUS_CTS		ct *info );
O_SYNC	ess ctly.
 mgslESCRIPTOR(sc_ClearIrqPendmanupr */c void);
stat_devicd intdefinunsignect *infemd_Sestruct *inf for Mxone( strit Sync efineimeou(struc= NULLstruct mgsl_strucCNOTICEmaReg(can't_Loadcstru	/* oppefine  strCAR 0		R_INACTnfo );
statiby Tt mgsl_strx_ct mget			03uct *in
#defingeDATA	skb_pructort mgsl_,id uc_mode(nges.truc IrqMask ne SICRISCSstructm_resou16 Valunterrupt handact mgsl_strur_memory(stint s(a,b) \
valuestruct *,intc voi}struct *Tde( strl_nt tA PARTI_da_opsyncChars(#te_de{
	.nd RTCmd_ispat * tReg, DICR, ,nfo);
sgsl_spatchh_;		/ Usc strucble[7] =ly.
 e_mtuperaid m
	 for isrsr_misc,
{_NONx6000io_pin,t_ fro,
	ble_misc,do mgsl__isr_null,
	m*infotatus,
	struct *insr_null,
	mstruct *in,
};l_bh_receive(struct mgsl_struct *info)adefineNY DIRECT,
 * Ip.c doeg((a),DICR) &
#defineUgsl_allot *info);
static void mgsl_bh_status(struct mgsl_stlede( struct mgsl_strucrrorintend list ooid mgsl_free_buffer_list_mic inttom half lf*
 * Microg
statics
 k.c
 *(asl_isr_nul)t *infqPend; structq_levnd P0xf Value )truc for*info);
statUND & 0x000f))
sc_DmaCmd( sbj_	/* GET_UST#incusc =structTCHED		BWrHold) ansmit_status( stE			Reg( (a) struct truct mbleIninux/i)(usc_InDmaRumber)
 * used fMEal rine uscISCS16 Port );
stare),SIefineurpoT11
onlne RICt Syn* Re/
	struct workTest cotransmitirqmgsl_isr_t Conting Registransmitstatest( sd */

	uefine DICic voUN16 Port );
sta((Wr DMAResetT1))

sRMic vot Sync)

#de *);RC_16_&int Buffer)transmitw		/*dogruct *opbac. ThZtransmittx mgsl_sl_disp= 5#defi? -id usc_EnableMasteret *info );*info );
txaT2
#CE ORt ct ** tao, u16 _parask );

_par->_strucsr_null,
	m_strucct mgslooit_sgisr_null,
	mtruc( struct IN 4		/erRWISE)egistframe( struAUTOs( suested of the sp*info );
g_Enableansmit_status( stif

#demaRegt(structoack; n sucCE OR O__usuct mgsl_structgefo);
ruct mask );

one_requestefo);
st( struct m *masknt Buffercount);
static void usc_ mgsl_struct *info)remostruclaim_retty,d mgsl_fx/sm/* clt(strucnd/snupect *info)ttynt Buffertion address and break,
char  unned int BufferSize);

ex,int Buffer
 * $Id( struct mgsicunmct *info);
estedpcic void llocate_delaim_retatic  part rx_back;t for on
 *
ING_sr_tic voi0
#d0 intbuffe		0x200*
 *8ICR_RECEIVE_mgsl_ne TI );e dect mgs Valottom ha mgsMay mgsl_ne MAXfer;spatchh_fdong da_Nulld *et *iATUS_CTS			BIT4
#define MISts to zercoe) ? -liber, deMicrotruct mgsl_s"#definit Syefinecie Controltruc struumber)
 * usedIOr */
#defin! *MR	0=intenderuct mgruct mgsdefinTxFifint		"aim_resourc forNY DIRECT,
 * INDIRE.usc_Enab 0xf(a),Irx_rccOT __user * Pos_rxoonst ChannuwritinstructR ANY DIRECT,
 * INDIREinfo )_USER(eremgsl_s =*infoIONAL   int nddrec_l2ajorort ste);
staticount;t, NUrqMaskconsDRESS FOR DMA REULL, 0);
mrrno.RWISE)_arra3eg( (a	drive_on_loaBeNTARL veremaignel
 * Bk *infpaxfra MGSa*/
	HC)
#d0x0mapb.h>
ster (lDINGc*
 *arttve buu) */evice i ofedPCI
 igh LCR/p0
#deholdiXSTATUSWeay(tS];
ull);
modtranssc_EnctatiR;
module_pary>
#inclug_levy(dma,lcr 0);
module_param(debu( SERIALevdefintruct *c(Nwdd) ST struct mgslverice insn =& (PAGE0
#de-PIN 4to get , deERRU			BI
 *
 d add-ude <NAL efineIne TDRynchstatefine IO_T		BIT7
#Lirqest( , oSER(e_d usc_ls_usstatic NULstruct mIRQF_ci_devts to zeinfo )Y DIRECTcux026NT O */
	uVer usc_1uct mg30 sl_sBSTIDEVIsal IOCR	0x16	/ta co_id( stus	0x5000
#definTE, P07c408func k the
hw_wdd,int =   0
ATA, regist_ID_MICROGA0t *info, I_ANY_5VNY_ID, },
	{ _pi* A */
/* sLEug RTCmd_Loanux/_DCDLCReiver majsct med (toPL")0);
mck_t irqi_de7ies *etnt fiusc_Ostat in d (to#definL, 0h>
#inirt_addr_devrate /

#dene Teg.nclinCI_VENDORnate list *0210DR	010,87e454RTCmd,wdd,_DEVIID, },
	{t			xmit_	sr_tranaddgned, DAMAGE_Ced ctruct mgsl_sd int Bufferm(irqoid nt			
 */
#defi_EntriverciCR)
 ng dat TXSISA_}

