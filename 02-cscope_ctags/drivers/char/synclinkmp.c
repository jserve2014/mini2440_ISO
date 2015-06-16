/*
 * $Id: synclinkmp.c,v 4.38 2005/07/15 13:29:44 paulkf Exp $
 *
 * Device driver for Microgate SyncLink Multiport
 * high speed multiprotocol serial adapter.
 *
 * written by Paul Fulghum for Microgate Corporation
 * paulkf@microgate.com
 *
 * Microgate and SyncLink are trademarks of Microgate Corporation
 *
 * Derived from serial.c written by Theodore Ts'o and Linus Torvalds
 * This code is released under the GNU General Public License (GPL)
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

#define VERSION(ver,rel,seq) (((ver)<<16) | ((rel)<<8) | (seq))
#if defined(__i386__)
#  define BREAKPOINT() asm("   int $3");
#else
#  define BREAKPOINT() { }
#endif

#define MAX_DEVICES 12

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
#include <linux/netdevice.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/ioctl.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/dma.h>
#include <linux/bitops.h>
#include <asm/types.h>
#include <linux/termios.h>
#include <linux/workqueue.h>
#include <linux/hdlc.h>
#include <linux/synclink.h>

#if defined(CONFIG_HDLC) || (defined(CONFIG_HDLC_MODULE) && defined(CONFIG_SYNCLINKMP_MODULE))
#define SYNCLINK_GENERIC_HDLC 1
#else
#define SYNCLINK_GENERIC_HDLC 0
#endif

#define GET_USER(error,value,addr) error = get_user(value,addr)
#define COPY_FROM_USER(error,dest,src,size) error = copy_from_user(dest,src,size) ? -EFAULT : 0
#define PUT_USER(error,value,addr) error = put_user(value,addr)
#define COPY_TO_USER(error,dest,src,size) error = copy_to_user(dest,src,size) ? -EFAULT : 0

#include <asm/uaccess.h>

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

/* size in bytes of DMA data buffers */
#define SCABUFSIZE 	1024
#define SCA_MEM_SIZE	0x40000
#define SCA_BASE_SIZE   512
#define SCA_REG_SIZE    16
#define SCA_MAX_PORTS   4
#define SCAMAXDESC 	128

#define	BUFFERLISTSIZE	4096

/* SCA-I style DMA buffer descriptor */
typedef struct _SCADESC
{
	u16	next;		/* lower l6 bits of next descriptor addr */
	u16	buf_ptr;	/* lower 16 bits of buffer addr */
	u8	buf_base;	/* upper 8 bits of buffer addr */
	u8	pad1;
	u16	length;		/* length of buffer */
	u8	status;		/* status of buffer */
	u8	pad2;
} SCADESC, *PSCADESC;

typedef struct _SCADESC_EX
{
	/* device driver bookkeeping section */
	char 	*virt_addr;    	/* virtual address of data buffer */
	u16	phys_entry;	/* lower 16-bits of physical address of this descriptor */
} SCADESC_EX, *PSCADESC_EX;

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

/*
 * Device instance data structure
 */
typedef struct _synclinkmp_info {
	void *if_ptr;				/* General purpose pointer (used by SPPP) */
	int			magic;
	struct tty_port		port;
	int			line;
	unsigned short		close_delay;
	unsigned short		closing_wait;	/* time to wait before closing */

	struct mgsl_icount	icount;

	int			timeout;
	int			x_char;		/* xon/xoff character */
	u16			read_status_mask1;  /* break detection (SR1 indications) */
	u16			read_status_mask2;  /* parity/framing/overun (SR2 indications) */
	unsigned char 		ignore_status_mask1;  /* break detection (SR1 indications) */
	unsigned char		ignore_status_mask2;  /* parity/framing/overun (SR2 indications) */
	unsigned char 		*tx_buf;
	int			tx_put;
	int			tx_get;
	int			tx_count;

	wait_queue_head_t	status_event_wait_q;
	wait_queue_head_t	event_wait_q;
	struct timer_list	tx_timer;	/* HDLC transmit timeout timer */
	struct _synclinkmp_info	*next_device;	/* device list link */
	struct timer_list	status_timer;	/* input signal status check timer */

	spinlock_t lock;		/* spinlock for synchronizing with ISR */
	struct work_struct task;	 		/* task structure for scheduling bh */

	u32 max_frame_size;			/* as set by device config */

	u32 pending_bh;

	bool bh_running;				/* Protection from multiple */
	int isr_overflow;
	bool bh_requested;

	int dcd_chkcount;			/* check counts to prevent */
	int cts_chkcount;			/* too many IRQs if a signal */
	int dsr_chkcount;			/* is floating */
	int ri_chkcount;

	char *buffer_list;			/* virtual address of Rx & Tx buffer lists */
	unsigned long buffer_list_phys;

	unsigned int rx_buf_count;		/* count of total allocated Rx buffers */
	SCADESC *rx_buf_list;   		/* list of receive buffer entries */
	SCADESC_EX rx_buf_list_ex[SCAMAXDESC]; /* list of receive buffer entries */
	unsigned int current_rx_buf;

	unsigned int tx_buf_count;		/* count of total allocated Tx buffers */
	SCADESC *tx_buf_list;		/* list of transmit buffer entries */
	SCADESC_EX tx_buf_list_ex[SCAMAXDESC]; /* list of transmit buffer entries */
	unsigned int last_tx_buf;

	unsigned char *tmp_rx_buf;
	unsigned int tmp_rx_buf_count;

	bool rx_enabled;
	bool rx_overflow;

	bool tx_enabled;
	bool tx_active;
	u32 idle_mode;

	unsigned char ie0_value;
	unsigned char ie1_value;
	unsigned char ie2_value;
	unsigned char ctrlreg_value;
	unsigned char old_signals;

	char device_name[25];			/* device instance name */

	int port_count;
	int adapter_num;
	int port_num;

	struct _synclinkmp_info *port_array[SCA_MAX_PORTS];

	unsigned int bus_type;			/* expansion bus type (ISA,EISA,PCI) */

	unsigned int irq_level;			/* interrupt level */
	unsigned long irq_flags;
	bool irq_requested;			/* true if IRQ requested */

	MGSL_PARAMS params;			/* communications parameters */

	unsigned char serial_signals;		/* current serial signal states */

	bool irq_occurred;			/* for diagnostics use */
	unsigned int init_error;		/* Initialization startup error */

	u32 last_mem_alloc;
	unsigned char* memory_base;		/* shared memory address (PCI only) */
	u32 phys_memory_base;
    	int shared_mem_requested;

	unsigned char* sca_base;		/* HD64570 SCA Memory address */
	u32 phys_sca_base;
	u32 sca_offset;
	bool sca_base_requested;

	unsigned char* lcr_base;		/* local config registers (PCI only) */
	u32 phys_lcr_base;
	u32 lcr_offset;
	int lcr_mem_requested;

	unsigned char* statctrl_base;		/* status/control register memory */
	u32 phys_statctrl_base;
	u32 statctrl_offset;
	bool sca_statctrl_requested;

	u32 misc_ctrl_value;
	char flag_buf[MAX_ASYNC_BUFFER_SIZE];
	char char_buf[MAX_ASYNC_BUFFER_SIZE];
	bool drop_rts_on_tx_done;

	struct	_input_signal_events	input_signal_events;

	/* SPPP/Cisco HDLC device parts */
	int netcount;
	spinlock_t netlock;

#if SYNCLINK_GENERIC_HDLC
	struct net_device *netdev;
#endif

} SLMP_INFO;

#define MGSL_MAGIC 0x5401

/*
 * define serial signal status change macros
 */
#define	MISCSTATUS_DCD_LATCHED	(SerialSignal_DCD<<8)	/* indicates change in DCD */
#define MISCSTATUS_RI_LATCHED	(SerialSignal_RI<<8)	/* indicates change in RI */
#define MISCSTATUS_CTS_LATCHED	(SerialSignal_CTS<<8)	/* indicates change in CTS */
#define MISCSTATUS_DSR_LATCHED	(SerialSignal_DSR<<8)	/* change in DSR */

/* Common Register macros */
#define LPR	0x00
#define PABR0	0x02
#define PABR1	0x03
#define WCRL	0x04
#define WCRM	0x05
#define WCRH	0x06
#define DPCR	0x08
#define DMER	0x09
#define ISR0	0x10
#define ISR1	0x11
#define ISR2	0x12
#define IER0	0x14
#define IER1	0x15
#define IER2	0x16
#define ITCR	0x18
#define INTVR 	0x1a
#define IMVR	0x1c

/* MSCI Register macros */
#define TRB	0x20
#define TRBL	0x20
#define TRBH	0x21
#define SR0	0x22
#define SR1	0x23
#define SR2	0x24
#define SR3	0x25
#define FST	0x26
#define IE0	0x28
#define IE1	0x29
#define IE2	0x2a
#define FIE	0x2b
#define CMD	0x2c
#define MD0	0x2e
#define MD1	0x2f
#define MD2	0x30
#define CTL	0x31
#define SA0	0x32
#define SA1	0x33
#define IDL	0x34
#define TMC	0x35
#define RXS	0x36
#define TXS	0x37
#define TRC0	0x38
#define TRC1	0x39
#define RRC	0x3a
#define CST0	0x3c
#define CST1	0x3d

/* Timer Register Macros */
#define TCNT	0x60
#define TCNTL	0x60
#define TCNTH	0x61
#define TCONR	0x62
#define TCONRL	0x62
#define TCONRH	0x63
#define TMCS	0x64
#define TEPR	0x65

/* DMA Controller Register macros */
#define DARL	0x80
#define DARH	0x81
#define DARB	0x82
#define BAR	0x80
#define BARL	0x80
#define BARH	0x81
#define BARB	0x82
#define SAR	0x84
#define SARL	0x84
#define SARH	0x85
#define SARB	0x86
#define CPB	0x86
#define CDA	0x88
#define CDAL	0x88
#define CDAH	0x89
#define EDA	0x8a
#define EDAL	0x8a
#define EDAH	0x8b
#define BFL	0x8c
#define BFLL	0x8c
#define BFLH	0x8d
#define BCR	0x8e
#define BCRL	0x8e
#define BCRH	0x8f
#define DSR	0x90
#define DMR	0x91
#define FCT	0x93
#define DIR	0x94
#define DCMD	0x95

/* combine with timer or DMA register address */
#define TIMER0	0x00
#define TIMER1	0x08
#define TIMER2	0x10
#define TIMER3	0x18
#define RXDMA 	0x00
#define TXDMA 	0x20

/* SCA Command Codes */
#define NOOP		0x00
#define TXRESET		0x01
#define TXENABLE	0x02
#define TXDISABLE	0x03
#define TXCRCINIT	0x04
#define TXCRCEXCL	0x05
#define TXEOM		0x06
#define TXABORT		0x07
#define MPON		0x08
#define TXBUFCLR	0x09
#define RXRESET		0x11
#define RXENABLE	0x12
#define RXDISABLE	0x13
#define RXCRCINIT	0x14
#define RXREJECT	0x15
#define SEARCHMP	0x16
#define RXCRCEXCL	0x17
#define RXCRCCALC	0x18
#define CHRESET		0x21
#define HUNT		0x31

/* DMA command codes */
#define SWABORT		0x01
#define FEICLEAR	0x02

/* IE0 */
#define TXINTE 		BIT7
#define RXINTE 		BIT6
#define TXRDYE 		BIT1
#define RXRDYE 		BIT0

/* IE1 & SR1 */
#define UDRN   	BIT7
#define IDLE   	BIT6
#define SYNCD  	BIT4
#define FLGD   	BIT4
#define CCTS   	BIT3
#define CDCD   	BIT2
#define BRKD   	BIT1
#define ABTD   	BIT1
#define GAPD   	BIT1
#define BRKE   	BIT0
#define IDLD	BIT0

/* IE2 & SR2 */
#define EOM	BIT7
#define PMP	BIT6
#define SHRT	BIT6
#define PE	BIT5
#define ABT	BIT5
#define FRME	BIT4
#define RBIT	BIT4
#define OVRN	BIT3
#define CRCE	BIT2


/*
 * Global linked list of SyncLink devices
 */
static SLMP_INFO *synclinkmp_device_list = NULL;
static int synclinkmp_adapter_count = -1;
static int synclinkmp_device_count = 0;

/*
 * Set this param to non-zero to load eax with the
 * .text section address and breakpoint on module load.
 * This is useful for use with gdb and add-symbol-file command.
 */
static int break_on_load = 0;

/*
 * Driver major number, defaults to zero to get auto
 * assigned major number. May be forced as module parameter.
 */
static int ttymajor = 0;

/*
 * Array of user specified options for ISA adapters.
 */
static int debug_level = 0;
static int maxframe[MAX_DEVICES] = {0,};

module_param(break_on_load, bool, 0);
module_param(ttymajor, int, 0);
module_param(debug_level, int, 0);
module_param_array(maxframe, int, NULL, 0);

static char *driver_name = "SyncLink MultiPort driver";
static char *driver_version = "$Revision: 4.38 $";

static int synclinkmp_init_one(struct pci_dev *dev,const struct pci_device_id *ent);
static void synclinkmp_remove_one(struct pci_dev *dev);

static struct pci_device_id synclinkmp_pci_tbl[] = {
	{ PCI_VENDOR_ID_MICROGATE, PCI_DEVICE_ID_MICROGATE_SCA, PCI_ANY_ID, PCI_ANY_ID, },
	{ 0, }, /* terminate list */
};
MODULE_DEVICE_TABLE(pci, synclinkmp_pci_tbl);

MODULE_LICENSE("GPL");

static struct pci_driver synclinkmp_pci_driver = {
	.name		= "synclinkmp",
	.id_table	= synclinkmp_pci_tbl,
	.probe		= synclinkmp_init_one,
	.remove		= __devexit_p(synclinkmp_remove_one),
};


static struct tty_driver *serial_driver;

/* number of characters left in xmit buffer before we ask for more */
#define WAKEUP_CHARS 256


/* tty callbacks */

static int  open(struct tty_struct *tty, struct file * filp);
static void close(struct tty_struct *tty, struct file * filp);
static void hangup(struct tty_struct *tty);
static void set_termios(struct tty_struct *tty, struct ktermios *old_termios);

static int  write(struct tty_struct *tty, const unsigned char *buf, int count);
static int put_char(struct tty_struct *tty, unsigned char ch);
static void send_xchar(struct tty_struct *tty, char ch);
static void wait_until_sent(struct tty_struct *tty, int timeout);
static int  write_room(struct tty_struct *tty);
static void flush_chars(struct tty_struct *tty);
static void flush_buffer(struct tty_struct *tty);
static void tx_hold(struct tty_struct *tty);
static void tx_release(struct tty_struct *tty);

static int  ioctl(struct tty_struct *tty, struct file *file, unsigned int cmd, unsigned long arg);
static int  chars_in_buffer(struct tty_struct *tty);
static void throttle(struct tty_struct * tty);
static void unthrottle(struct tty_struct * tty);
static int set_break(struct tty_struct *tty, int break_state);

#if SYNCLINK_GENERIC_HDLC
#define dev_to_port(D) (dev_to_hdlc(D)->priv)
static void hdlcdev_tx_done(SLMP_INFO *info);
static void hdlcdev_rx(SLMP_INFO *info, char *buf, int size);
static int  hdlcdev_init(SLMP_INFO *info);
static void hdlcdev_exit(SLMP_INFO *info);
#endif

/* ioctl handlers */

static int  get_stats(SLMP_INFO *info, struct mgsl_icount __user *user_icount);
static int  get_params(SLMP_INFO *info, MGSL_PARAMS __user *params);
static int  set_params(SLMP_INFO *info, MGSL_PARAMS __user *params);
static int  get_txidle(SLMP_INFO *info, int __user *idle_mode);
static int  set_txidle(SLMP_INFO *info, int idle_mode);
static int  tx_enable(SLMP_INFO *info, int enable);
static int  tx_abort(SLMP_INFO *info);
static int  rx_enable(SLMP_INFO *info, int enable);
static int  modem_input_wait(SLMP_INFO *info,int arg);
static int  wait_mgsl_event(SLMP_INFO *info, int __user *mask_ptr);
static int  tiocmget(struct tty_struct *tty, struct file *file);
static int  tiocmset(struct tty_struct *tty, struct file *file,
		     unsigned int set, unsigned int clear);
static int  set_break(struct tty_struct *tty, int break_state);

static void add_device(SLMP_INFO *info);
static void device_init(int adapter_num, struct pci_dev *pdev);
static int  claim_resources(SLMP_INFO *info);
static void release_resources(SLMP_INFO *info);

static int  startup(SLMP_INFO *info);
static int  block_til_ready(struct tty_struct *tty, struct file * filp,SLMP_INFO *info);
static int carrier_raised(struct tty_port *port);
static void shutdown(SLMP_INFO *info);
static void program_hw(SLMP_INFO *info);
static void change_params(SLMP_INFO *info);

static bool init_adapter(SLMP_INFO *info);
static bool register_test(SLMP_INFO *info);
static bool irq_test(SLMP_INFO *info);
static bool loopback_test(SLMP_INFO *info);
static int  adapter_test(SLMP_INFO *info);
static bool memory_test(SLMP_INFO *info);

static void reset_adapter(SLMP_INFO *info);
static void reset_port(SLMP_INFO *info);
static void async_mode(SLMP_INFO *info);
static void hdlc_mode(SLMP_INFO *info);

static void rx_stop(SLMP_INFO *info);
static void rx_start(SLMP_INFO *info);
static void rx_reset_buffers(SLMP_INFO *info);
static void rx_free_frame_buffers(SLMP_INFO *info, unsigned int first, unsigned int last);
static bool rx_get_frame(SLMP_INFO *info);

static void tx_start(SLMP_INFO *info);
static void tx_stop(SLMP_INFO *info);
static void tx_load_fifo(SLMP_INFO *info);
static void tx_set_idle(SLMP_INFO *info);
static void tx_load_dma_buffer(SLMP_INFO *info, const char *buf, unsigned int count);

static void get_signals(SLMP_INFO *info);
static void set_signals(SLMP_INFO *info);
static void enable_loopback(SLMP_INFO *info, int enable);
static void set_rate(SLMP_INFO *info, u32 data_rate);

static int  bh_action(SLMP_INFO *info);
static void bh_handler(struct work_struct *work);
static void bh_receive(SLMP_INFO *info);
static void bh_transmit(SLMP_INFO *info);
static void bh_status(SLMP_INFO *info);
static void isr_timer(SLMP_INFO *info);
static void isr_rxint(SLMP_INFO *info);
static void isr_rxrdy(SLMP_INFO *info);
static void isr_txint(SLMP_INFO *info);
static void isr_txrdy(SLMP_INFO *info);
static void isr_rxdmaok(SLMP_INFO *info);
static void isr_rxdmaerror(SLMP_INFO *info);
static void isr_txdmaok(SLMP_INFO *info);
static void isr_txdmaerror(SLMP_INFO *info);
static void isr_io_pin(SLMP_INFO *info, u16 status);

static int  alloc_dma_bufs(SLMP_INFO *info);
static void free_dma_bufs(SLMP_INFO *info);
static int  alloc_buf_list(SLMP_INFO *info);
static int  alloc_frame_bufs(SLMP_INFO *info, SCADESC *list, SCADESC_EX *list_ex,int count);
static int  alloc_tmp_rx_buf(SLMP_INFO *info);
static void free_tmp_rx_buf(SLMP_INFO *info);

static void load_pci_memory(SLMP_INFO *info, char* dest, const char* src, unsigned short count);
static void trace_block(SLMP_INFO *info, const char* data, int count, int xmit);
static void tx_timeout(unsigned long context);
static void status_timeout(unsigned long context);

static unsigned char read_reg(SLMP_INFO *info, unsigned char addr);
static void write_reg(SLMP_INFO *info, unsigned char addr, unsigned char val);
static u16 read_reg16(SLMP_INFO *info, unsigned char addr);
static void write_reg16(SLMP_INFO *info, unsigned char addr, u16 val);
static unsigned char read_status_reg(SLMP_INFO * info);
static void write_control_reg(SLMP_INFO * info);


static unsigned char rx_active_fifo_level = 16;	// rx request FIFO activation level in bytes
static unsigned char tx_active_fifo_level = 16;	// tx request FIFO activation level in bytes
static unsigned char tx_negate_fifo_level = 32;	// tx request FIFO negation level in bytes

static u32 misc_ctrl_value = 0x007e4040;
static u32 lcr1_brdr_value = 0x00800028;

static u32 read_ahead_count = 8;

/* DPCR, DMA Priority Control
 *
 * 07..05  Not used, must be 0
 * 04      BRC, bus release condition: 0=all transfers complete
 *              1=release after 1 xfer on all channels
 * 03      CCC, channel change condition: 0=every cycle
 *              1=after each channel completes all xfers
 * 02..00  PR<2..0>, priority 100=round robin
 *
 * 00000100 = 0x00
 */
static unsigned char dma_priority = 0x04;

// Number of bytes that can be written to shared RAM
// in a single write operation
static u32 sca_pci_load_interval = 64;

/*
 * 1st function defined in .text section. Calling this function in
 * init_module() followed by a breakpoint allows a remote debugger
 * (gdb) to get the .text address for the add-symbol-file command.
 * This allows remote debugging of dynamically loadable modules.
 */
static void* synclinkmp_get_text_ptr(void);
static void* synclinkmp_get_text_ptr(void) {return synclinkmp_get_text_ptr;}

static inline int sanity_check(SLMP_INFO *info,
			       char *name, const char *routine)
{
#ifdef SANITY_CHECK
	static const char *badmagic =
		"Warning: bad magic number for synclinkmp_struct (%s) in %s\n";
	static const char *badinfo =
		"Warning: null synclinkmp_struct for (%s) in %s\n";

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

/* tty callbacks */

/* Called when a port is opened.  Init and enable port.
 */
static int open(struct tty_struct *tty, struct file *filp)
{
	SLMP_INFO *info;
	int retval, line;
	unsigned long flags;

	line = tty->index;
	if ((line < 0) || (line >= synclinkmp_device_count)) {
		printk("%s(%d): open with invalid line #%d.\n",
			__FILE__,__LINE__,line);
		return -ENODEV;
	}

	info = synclinkmp_device_list;
	while(info && info->line != line)
		info = info->next_device;
	if (sanity_check(info, tty->name, "open"))
		return -ENODEV;
	if ( info->init_error ) {
		printk("%s(%d):%s device is not allocated, init error=%d\n",
			__FILE__,__LINE__,info->device_name,info->init_error);
		return -ENODEV;
	}

	tty->driver_data = info;
	info->port.tty = tty;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s open(), old ref count = %d\n",
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
			printk("%s(%d):%s block_til_ready() returned %d\n",
				 __FILE__,__LINE__, info->device_name, retval);
		goto cleanup;
	}

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s open() success\n",
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
}

/* Called when port is closed. Wait for remaining data to be
 * sent. Disable port and free resources.
 */
static void close(struct tty_struct *tty, struct file *filp)
{
	SLMP_INFO * info = tty->driver_data;

	if (sanity_check(info, tty->name, "close"))
		return;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s close() entry, count=%d\n",
			 __FILE__,__LINE__, info->device_name, info->port.count);

	if (tty_port_close_start(&info->port, tty, filp) == 0)
		goto cleanup;
		
 	if (info->port.flags & ASYNC_INITIALIZED)
 		wait_until_sent(tty, info->timeout);

	flush_buffer(tty);
	tty_ldisc_flush(tty);
	shutdown(info);

	tty_port_close_end(&info->port, tty);
	info->port.tty = NULL;
cleanup:
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s close() exit, count=%d\n", __FILE__,__LINE__,
			tty->driver->name, info->port.count);
}

/* Called by tty_hangup() when a hangup is signaled.
 * This is the same as closing all open descriptors for the port.
 */
static void hangup(struct tty_struct *tty)
{
	SLMP_INFO *info = tty->driver_data;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s hangup()\n",
			 __FILE__,__LINE__, info->device_name );

	if (sanity_check(info, tty->name, "hangup"))
		return;

	flush_buffer(tty);
	shutdown(info);

	info->port.count = 0;
	info->port.flags &= ~ASYNC_NORMAL_ACTIVE;
	info->port.tty = NULL;

	wake_up_interruptible(&info->port.open_wait);
}

/* Set new termios settings
 */
static void set_termios(struct tty_struct *tty, struct ktermios *old_termios)
{
	SLMP_INFO *info = tty->driver_data;
	unsigned long flags;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s set_termios()\n", __FILE__,__LINE__,
			tty->driver->name );

	change_params(info);

	/* Handle transition to B0 status */
	if (old_termios->c_cflag & CBAUD &&
	    !(tty->termios->c_cflag & CBAUD)) {
		info->serial_signals &= ~(SerialSignal_RTS + SerialSignal_DTR);
		spin_lock_irqsave(&info->lock,flags);
	 	set_signals(info);
		spin_unlock_irqrestore(&info->lock,flags);
	}

	/* Handle transition away from B0 status */
	if (!(old_termios->c_cflag & CBAUD) &&
	    tty->termios->c_cflag & CBAUD) {
		info->serial_signals |= SerialSignal_DTR;
 		if (!(tty->termios->c_cflag & CRTSCTS) ||
 		    !test_bit(TTY_THROTTLED, &tty->flags)) {
			info->serial_signals |= SerialSignal_RTS;
 		}
		spin_lock_irqsave(&info->lock,flags);
	 	set_signals(info);
		spin_unlock_irqrestore(&info->lock,flags);
	}

	/* Handle turning off CRTSCTS */
	if (old_termios->c_cflag & CRTSCTS &&
	    !(tty->termios->c_cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
		tx_release(tty);
	}
}

/* Send a block of data
 *
 * Arguments:
 *
 * 	tty		pointer to tty information structure
 * 	buf		pointer to buffer containing send data
 * 	count		size of send data in bytes
 *
 * Return Value:	number of characters written
 */
static int write(struct tty_struct *tty,
		 const unsigned char *buf, int count)
{
	int	c, ret = 0;
	SLMP_INFO *info = tty->driver_data;
	unsigned long flags;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s write() count=%d\n",
		       __FILE__,__LINE__,info->device_name,count);

	if (sanity_check(info, tty->name, "write"))
		goto cleanup;

	if (!info->tx_buf)
		goto cleanup;

	if (info->params.mode == MGSL_MODE_HDLC) {
		if (count > info->max_frame_size) {
			ret = -EIO;
			goto cleanup;
		}
		if (info->tx_active)
			goto cleanup;
		if (info->tx_count) {
			/* send accumulated data from send_char() calls */
			/* as frame and wait before accepting more data. */
			tx_load_dma_buffer(info, info->tx_buf, info->tx_count);
			goto start;
		}
		ret = info->tx_count = count;
		tx_load_dma_buffer(info, buf, count);
		goto start;
	}

	for (;;) {
		c = min_t(int, count,
			min(info->max_frame_size - info->tx_count - 1,
			    info->max_frame_size - info->tx_put));
		if (c <= 0)
			break;
			
		memcpy(info->tx_buf + info->tx_put, buf, c);

		spin_lock_irqsave(&info->lock,flags);
		info->tx_put += c;
		if (info->tx_put >= info->max_frame_size)
			info->tx_put -= info->max_frame_size;
		info->tx_count += c;
		spin_unlock_irqrestore(&info->lock,flags);

		buf += c;
		count -= c;
		ret += c;
	}

	if (info->params.mode == MGSL_MODE_HDLC) {
		if (count) {
			ret = info->tx_count = 0;
			goto cleanup;
		}
		tx_load_dma_buffer(info, info->tx_buf, info->tx_count);
	}
start:
 	if (info->tx_count && !tty->stopped && !tty->hw_stopped) {
		spin_lock_irqsave(&info->lock,flags);
		if (!info->tx_active)
		 	tx_start(info);
		spin_unlock_irqrestore(&info->lock,flags);
 	}

cleanup:
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk( "%s(%d):%s write() returning=%d\n",
			__FILE__,__LINE__,info->device_name,ret);
	return ret;
}

/* Add a character to the transmit buffer.
 */
static int put_char(struct tty_struct *tty, unsigned char ch)
{
	SLMP_INFO *info = tty->driver_data;
	unsigned long flags;
	int ret = 0;

	if ( debug_level >= DEBUG_LEVEL_INFO ) {
		printk( "%s(%d):%s put_char(%d)\n",
			__FILE__,__LINE__,info->device_name,ch);
	}

	if (sanity_check(info, tty->name, "put_char"))
		return 0;

	if (!info->tx_buf)
		return 0;

	spin_lock_irqsave(&info->lock,flags);

	if ( (info->params.mode != MGSL_MODE_HDLC) ||
	     !info->tx_active ) {

		if (info->tx_count < info->max_frame_size - 1) {
			info->tx_buf[info->tx_put++] = ch;
			if (info->tx_put >= info->max_frame_size)
				info->tx_put -= info->max_frame_size;
			info->tx_count++;
			ret = 1;
		}
	}

	spin_unlock_irqrestore(&info->lock,flags);
	return ret;
}

/* Send a high-priority XON/XOFF character
 */
static void send_xchar(struct tty_struct *tty, char ch)
{
	SLMP_INFO *info = tty->driver_data;
	unsigned long flags;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s send_xchar(%d)\n",
			 __FILE__,__LINE__, info->device_name, ch );

	if (sanity_check(info, tty->name, "send_xchar"))
		return;

	info->x_char = ch;
	if (ch) {
		/* Make sure transmit interrupts are on */
		spin_lock_irqsave(&info->lock,flags);
		if (!info->tx_enabled)
		 	tx_start(info);
		spin_unlock_irqrestore(&info->lock,flags);
	}
}

/* Wait until the transmitter is empty.
 */
static void wait_until_sent(struct tty_struct *tty, int timeout)
{
	SLMP_INFO * info = tty->driver_data;
	unsigned long orig_jiffies, char_time;

	if (!info )
		return;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s wait_until_sent() entry\n",
			 __FILE__,__LINE__, info->device_name );

	if (sanity_check(info, tty->name, "wait_until_sent"))
		return;

	lock_kernel();

	if (!(info->port.flags & ASYNC_INITIALIZED))
		goto exit;

	orig_jiffies = jiffies;

	/* Set check interval to 1/5 of estimated time to
	 * send a character, and make it at least 1. The check
	 * interval should also be less than the timeout.
	 * Note: use tight timings here to satisfy the NIST-PCTS.
	 */

	if ( info->params.data_rate ) {
	       	char_time = info->timeout/(32 * 5);
		if (!char_time)
			char_time++;
	} else
		char_time = 1;

	if (timeout)
		char_time = min_t(unsigned long, char_time, timeout);

	if ( info->params.mode == MGSL_MODE_HDLC ) {
		while (info->tx_active) {
			msleep_interruptible(jiffies_to_msecs(char_time));
			if (signal_pending(current))
				break;
			if (timeout && time_after(jiffies, orig_jiffies + timeout))
				break;
		}
	} else {
		//TODO: determine if there is something similar to USC16C32
		// 	TXSTATUS_ALL_SENT status
		while ( info->tx_active && info->tx_enabled) {
			msleep_interruptible(jiffies_to_msecs(char_time));
			if (signal_pending(current))
				break;
			if (timeout && time_after(jiffies, orig_jiffies + timeout))
				break;
		}
	}

exit:
	unlock_kernel();
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s wait_until_sent() exit\n",
			 __FILE__,__LINE__, info->device_name );
}

/* Return the count of free bytes in transmit buffer
 */
static int write_room(struct tty_struct *tty)
{
	SLMP_INFO *info = tty->driver_data;
	int ret;

	if (sanity_check(info, tty->name, "write_room"))
		return 0;

	lock_kernel();
	if (info->params.mode == MGSL_MODE_HDLC) {
		ret = (info->tx_active) ? 0 : HDLC_MAX_FRAME_SIZE;
	} else {
		ret = info->max_frame_size - info->tx_count - 1;
		if (ret < 0)
			ret = 0;
	}
	unlock_kernel();

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s write_room()=%d\n",
		       __FILE__, __LINE__, info->device_name, ret);

	return ret;
}

/* enable transmitter and send remaining buffered characters
 */
static void flush_chars(struct tty_struct *tty)
{
	SLMP_INFO *info = tty->driver_data;
	unsigned long flags;

	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk( "%s(%d):%s flush_chars() entry tx_count=%d\n",
			__FILE__,__LINE__,info->device_name,info->tx_count);

	if (sanity_check(info, tty->name, "flush_chars"))
		return;

	if (info->tx_count <= 0 || tty->stopped || tty->hw_stopped ||
	    !info->tx_buf)
		return;

	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk( "%s(%d):%s flush_chars() entry, starting transmitter\n",
			__FILE__,__LINE__,info->device_name );

	spin_lock_irqsave(&info->lock,flags);

	if (!info->tx_active) {
		if ( (info->params.mode == MGSL_MODE_HDLC) &&
			info->tx_count ) {
			/* operating in synchronous (frame oriented) mode */
			/* copy data from circular tx_buf to */
			/* transmit DMA buffer. */
			tx_load_dma_buffer(info,
				 info->tx_buf,info->tx_count);
		}
	 	tx_start(info);
	}

	spin_unlock_irqrestore(&info->lock,flags);
}

/* Discard all data in the send buffer
 */
static void flush_buffer(struct tty_struct *tty)
{
	SLMP_INFO *info = tty->driver_data;
	unsigned long flags;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s flush_buffer() entry\n",
			 __FILE__,__LINE__, info->device_name );

	if (sanity_check(info, tty->name, "flush_buffer"))
		return;

	spin_lock_irqsave(&info->lock,flags);
	info->tx_count = info->tx_put = info->tx_get = 0;
	del_timer(&info->tx_timer);
	spin_unlock_irqrestore(&info->lock,flags);

	tty_wakeup(tty);
}

/* throttle (stop) transmitter
 */
static void tx_hold(struct tty_struct *tty)
{
	SLMP_INFO *info = tty->driver_data;
	unsigned long flags;

	if (sanity_check(info, tty->name, "tx_hold"))
		return;

	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk("%s(%d):%s tx_hold()\n",
			__FILE__,__LINE__,info->device_name);

	spin_lock_irqsave(&info->lock,flags);
	if (info->tx_enabled)
	 	tx_stop(info);
	spin_unlock_irqrestore(&info->lock,flags);
}

/* release (start) transmitter
 */
static void tx_release(struct tty_struct *tty)
{
	SLMP_INFO *info = tty->driver_data;
	unsigned long flags;

	if (sanity_check(info, tty->name, "tx_release"))
		return;

	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk("%s(%d):%s tx_release()\n",
			__FILE__,__LINE__,info->device_name);

	spin_lock_irqsave(&info->lock,flags);
	if (!info->tx_enabled)
	 	tx_start(info);
	spin_unlock_irqrestore(&info->lock,flags);
}

/* Service an IOCTL request
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
static int do_ioctl(struct tty_struct *tty, struct file *file,
		 unsigned int cmd, unsigned long arg)
{
	SLMP_INFO *info = tty->driver_data;
	int error;
	struct mgsl_icount cnow;	/* kernel counter temps */
	struct serial_icounter_struct __user *p_cuser;	/* user space */
	unsigned long flags;
	void __user *argp = (void __user *)arg;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s ioctl() cmd=%08X\n", __FILE__,__LINE__,
			info->device_name, cmd );

	if (sanity_check(info, tty->name, "ioctl"))
		return -ENODEV;

	if ((cmd != TIOCGSERIAL) && (cmd != TIOCSSERIAL) &&
	    (cmd != TIOCMIWAIT) && (cmd != TIOCGICOUNT)) {
		if (tty->flags & (1 << TTY_IO_ERROR))
		    return -EIO;
	}

	switch (cmd) {
	case MGSL_IOCGPARAMS:
		return get_params(info, argp);
	case MGSL_IOCSPARAMS:
		return set_params(info, argp);
	case MGSL_IOCGTXIDLE:
		return get_txidle(info, argp);
	case MGSL_IOCSTXIDLE:
		return set_txidle(info, (int)arg);
	case MGSL_IOCTXENABLE:
		return tx_enable(info, (int)arg);
	case MGSL_IOCRXENABLE:
		return rx_enable(info, (int)arg);
	case MGSL_IOCTXABORT:
		return tx_abort(info);
	case MGSL_IOCGSTATS:
		return get_stats(info, argp);
	case MGSL_IOCWAITEVENT:
		return wait_mgsl_event(info, argp);
	case MGSL_IOCLOOPTXDONE:
		return 0; // TODO: Not supported, need to document
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
		spin_lock_irqsave(&info->lock,flags);
		cnow = info->icount;
		spin_unlock_irqrestore(&info->lock,flags);
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

static int ioctl(struct tty_struct *tty, struct file *file,
		 unsigned int cmd, unsigned long arg)
{
	int ret;
	lock_kernel();
	ret = do_ioctl(tty, file, cmd, arg);
	unlock_kernel();
	return ret;
}

/*
 * /proc fs routines....
 */

static inline void line_info(struct seq_file *m, SLMP_INFO *info)
{
	char	stat_buf[30];
	unsigned long flags;

	seq_printf(m, "%s: SCABase=%08x Mem=%08X StatusControl=%08x LCR=%08X\n"
		       "\tIRQ=%d MaxFrameSize=%u\n",
		info->device_name,
		info->phys_sca_base,
		info->phys_memory_base,
		info->phys_statctrl_base,
		info->phys_lcr_base,
		info->irq_level,
		info->max_frame_size );

	/* output current serial signal states */
	spin_lock_irqsave(&info->lock,flags);
 	get_signals(info);
	spin_unlock_irqrestore(&info->lock,flags);

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

	if (info->params.mode == MGSL_MODE_HDLC) {
		seq_printf(m, "\tHDLC txok:%d rxok:%d",
			      info->icount.txok, info->icount.rxok);
		if (info->icount.txunder)
			seq_printf(m, " txunder:%d", info->icount.txunder);
		if (info->icount.txabort)
			seq_printf(m, " txabort:%d", info->icount.txabort);
		if (info->icount.rxshort)
			seq_printf(m, " rxshort:%d", info->icount.rxshort);
		if (info->icount.rxlong)
			seq_printf(m, " rxlong:%d", info->icount.rxlong);
		if (info->icount.rxover)
			seq_printf(m, " rxover:%d", info->icount.rxover);
		if (info->icount.rxcrc)
			seq_printf(m, " rxlong:%d", info->icount.rxcrc);
	} else {
		seq_printf(m, "\tASYNC tx:%d rx:%d",
			      info->icount.tx, info->icount.rx);
		if (info->icount.frame)
			seq_printf(m, " fe:%d", info->icount.frame);
		if (info->icount.parity)
			seq_printf(m, " pe:%d", info->icount.parity);
		if (info->icount.brk)
			seq_printf(m, " brk:%d", info->icount.brk);
		if (info->icount.overrun)
			seq_printf(m, " oe:%d", info->icount.overrun);
	}

	/* Append serial signal status to end */
	seq_printf(m, " %s\n", stat_buf+1);

	seq_printf(m, "\ttxactive=%d bh_req=%d bh_run=%d pending_bh=%x\n",
	 info->tx_active,info->bh_requested,info->bh_running,
	 info->pending_bh);
}

/* Called to print information about devices
 */
static int synclinkmp_proc_show(struct seq_file *m, void *v)
{
	SLMP_INFO *info;

	seq_printf(m, "synclinkmp driver:%s\n", driver_version);

	info = synclinkmp_device_list;
	while( info ) {
		line_info(m, info);
		info = info->next_device;
	}
	return 0;
}

static int synclinkmp_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, synclinkmp_proc_show, NULL);
}

static const struct file_operations synclinkmp_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= synclinkmp_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/* Return the count of bytes in transmit buffer
 */
static int chars_in_buffer(struct tty_struct *tty)
{
	SLMP_INFO *info = tty->driver_data;

	if (sanity_check(info, tty->name, "chars_in_buffer"))
		return 0;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s chars_in_buffer()=%d\n",
		       __FILE__, __LINE__, info->device_name, info->tx_count);

	return info->tx_count;
}

/* Signal remote device to throttle send data (our receive data)
 */
static void throttle(struct tty_struct * tty)
{
	SLMP_INFO *info = tty->driver_data;
	unsigned long flags;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s throttle() entry\n",
			 __FILE__,__LINE__, info->device_name );

	if (sanity_check(info, tty->name, "throttle"))
		return;

	if (I_IXOFF(tty))
		send_xchar(tty, STOP_CHAR(tty));

 	if (tty->termios->c_cflag & CRTSCTS) {
		spin_lock_irqsave(&info->lock,flags);
		info->serial_signals &= ~SerialSignal_RTS;
	 	set_signals(info);
		spin_unlock_irqrestore(&info->lock,flags);
	}
}

/* Signal remote device to stop throttling send data (our receive data)
 */
static void unthrottle(struct tty_struct * tty)
{
	SLMP_INFO *info = tty->driver_data;
	unsigned long flags;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s unthrottle() entry\n",
			 __FILE__,__LINE__, info->device_name );

	if (sanity_check(info, tty->name, "unthrottle"))
		return;

	if (I_IXOFF(tty)) {
		if (info->x_char)
			info->x_char = 0;
		else
			send_xchar(tty, START_CHAR(tty));
	}

 	if (tty->termios->c_cflag & CRTSCTS) {
		spin_lock_irqsave(&info->lock,flags);
		info->serial_signals |= SerialSignal_RTS;
	 	set_signals(info);
		spin_unlock_irqrestore(&info->lock,flags);
	}
}

/* set or clear transmit break condition
 * break_state	-1=set break condition, 0=clear
 */
static int set_break(struct tty_struct *tty, int break_state)
{
	unsigned char RegValue;
	SLMP_INFO * info = tty->driver_data;
	unsigned long flags;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s set_break(%d)\n",
			 __FILE__,__LINE__, info->device_name, break_state);

	if (sanity_check(info, tty->name, "set_break"))
		return -EINVAL;

	spin_lock_irqsave(&info->lock,flags);
	RegValue = read_reg(info, CTL);
 	if (break_state == -1)
		RegValue |= BIT3;
	else
		RegValue &= ~BIT3;
	write_reg(info, CTL, RegValue);
	spin_unlock_irqrestore(&info->lock,flags);
	return 0;
}

#if SYNCLINK_GENERIC_HDLC

/**
 * called by generic HDLC layer when protocol selected (PPP, frame relay, etc.)
 * set encoding and frame check sequence (FCS) options
 *
 * dev       pointer to network device structure
 * encoding  serial encoding setting
 * parity    FCS setting
 *
 * returns 0 if success, otherwise error code
 */
static int hdlcdev_attach(struct net_device *dev, unsigned short encoding,
			  unsigned short parity)
{
	SLMP_INFO *info = dev_to_port(dev);
	unsigned char  new_encoding;
	unsigned short new_crctype;

	/* return error if TTY interface open */
	if (info->port.count)
		return -EBUSY;

	switch (encoding)
	{
	case ENCODING_NRZ:        new_encoding = HDLC_ENCODING_NRZ; break;
	case ENCODING_NRZI:       new_encoding = HDLC_ENCODING_NRZI_SPACE; break;
	case ENCODING_FM_MARK:    new_encoding = HDLC_ENCODING_BIPHASE_MARK; break;
	case ENCODING_FM_SPACE:   new_encoding = HDLC_ENCODING_BIPHASE_SPACE; break;
	case ENCODING_MANCHESTER: new_encoding = HDLC_ENCODING_BIPHASE_LEVEL; break;
	default: return -EINVAL;
	}

	switch (parity)
	{
	case PARITY_NONE:            new_crctype = HDLC_CRC_NONE; break;
	case PARITY_CRC16_PR1_CCITT: new_crctype = HDLC_CRC_16_CCITT; break;
	case PARITY_CRC32_PR1_CCITT: new_crctype = HDLC_CRC_32_CCITT; break;
	default: return -EINVAL;
	}

	info->params.encoding = new_encoding;
	info->params.crc_type = new_crctype;

	/* if network interface up, reprogram hardware */
	if (info->netcount)
		program_hw(info);

	return 0;
}

/**
 * called by generic HDLC layer to send frame
 *
 * skb  socket buffer containing HDLC frame
 * dev  pointer to network device structure
 */
static netdev_tx_t hdlcdev_xmit(struct sk_buff *skb,
				      struct net_device *dev)
{
	SLMP_INFO *info = dev_to_port(dev);
	unsigned long flags;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk(KERN_INFO "%s:hdlc_xmit(%s)\n",__FILE__,dev->name);

	/* stop sending until this frame completes */
	netif_stop_queue(dev);

	/* copy data to device buffers */
	info->tx_count = skb->len;
	tx_load_dma_buffer(info, skb->data, skb->len);

	/* update network statistics */
	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;

	/* done with socket buffer, so free it */
	dev_kfree_skb(skb);

	/* save start time for transmit timeout detection */
	dev->trans_start = jiffies;

	/* start hardware transmitter if necessary */
	spin_lock_irqsave(&info->lock,flags);
	if (!info->tx_active)
	 	tx_start(info);
	spin_unlock_irqrestore(&info->lock,flags);

	return NETDEV_TX_OK;
}

/**
 * called by network layer when interface enabled
 * claim resources and initialize hardware
 *
 * dev  pointer to network device structure
 *
 * returns 0 if success, otherwise error code
 */
static int hdlcdev_open(struct net_device *dev)
{
	SLMP_INFO *info = dev_to_port(dev);
	int rc;
	unsigned long flags;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s:hdlcdev_open(%s)\n",__FILE__,dev->name);

	/* generic HDLC layer open processing */
	if ((rc = hdlc_open(dev)))
		return rc;

	/* arbitrate between network and tty opens */
	spin_lock_irqsave(&info->netlock, flags);
	if (info->port.count != 0 || info->netcount != 0) {
		printk(KERN_WARNING "%s: hdlc_open returning busy\n", dev->name);
		spin_unlock_irqrestore(&info->netlock, flags);
		return -EBUSY;
	}
	info->netcount=1;
	spin_unlock_irqrestore(&info->netlock, flags);

	/* claim resources and init adapter */
	if ((rc = startup(info)) != 0) {
		spin_lock_irqsave(&info->netlock, flags);
		info->netcount=0;
		spin_unlock_irqrestore(&info->netlock, flags);
		return rc;
	}

	/* assert DTR and RTS, apply hardware settings */
	info->serial_signals |= SerialSignal_RTS + SerialSignal_DTR;
	program_hw(info);

	/* enable network layer transmit */
	dev->trans_start = jiffies;
	netif_start_queue(dev);

	/* inform generic HDLC layer of current DCD status */
	spin_lock_irqsave(&info->lock, flags);
	get_signals(info);
	spin_unlock_irqrestore(&info->lock, flags);
	if (info->serial_signals & SerialSignal_DCD)
		netif_carrier_on(dev);
	else
		netif_carrier_off(dev);
	return 0;
}

/**
 * called by network layer when interface is disabled
 * shutdown hardware and release resources
 *
 * dev  pointer to network device structure
 *
 * returns 0 if success, otherwise error code
 */
static int hdlcdev_close(struct net_device *dev)
{
	SLMP_INFO *info = dev_to_port(dev);
	unsigned long flags;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s:hdlcdev_close(%s)\n",__FILE__,dev->name);

	netif_stop_queue(dev);

	/* shutdown adapter and release resources */
	shutdown(info);

	hdlc_close(dev);

	spin_lock_irqsave(&info->netlock, flags);
	info->netcount=0;
	spin_unlock_irqrestore(&info->netlock, flags);

	return 0;
}

/**
 * called by network layer to process IOCTL call to network device
 *
 * dev  pointer to network device structure
 * ifr  pointer to network interface request structure
 * cmd  IOCTL command code
 *
 * returns 0 if success, otherwise error code
 */
static int hdlcdev_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	const size_t size = sizeof(sync_serial_settings);
	sync_serial_settings new_line;
	sync_serial_settings __user *line = ifr->ifr_settings.ifs_ifsu.sync;
	SLMP_INFO *info = dev_to_port(dev);
	unsigned int flags;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s:hdlcdev_ioctl(%s)\n",__FILE__,dev->name);

	/* return error if TTY interface open */
	if (info->port.count)
		return -EBUSY;

	if (cmd != SIOCWANDEV)
		return hdlc_ioctl(dev, ifr, cmd);

	switch(ifr->ifr_settings.type) {
	case IF_GET_IFACE: /* return current sync_serial_settings */

		ifr->ifr_settings.type = IF_IFACE_SYNC_SERIAL;
		if (ifr->ifr_settings.size < size) {
			ifr->ifr_settings.size = size; /* data size wanted */
			return -ENOBUFS;
		}

		flags = info->params.flags & (HDLC_FLAG_RXC_RXCPIN | HDLC_FLAG_RXC_DPLL |
					      HDLC_FLAG_RXC_BRG    | HDLC_FLAG_RXC_TXCPIN |
					      HDLC_FLAG_TXC_TXCPIN | HDLC_FLAG_TXC_DPLL |
					      HDLC_FLAG_TXC_BRG    | HDLC_FLAG_TXC_RXCPIN);

		switch (flags){
		case (HDLC_FLAG_RXC_RXCPIN | HDLC_FLAG_TXC_TXCPIN): new_line.clock_type = CLOCK_EXT; break;
		case (HDLC_FLAG_RXC_BRG    | HDLC_FLAG_TXC_BRG):    new_line.clock_type = CLOCK_INT; break;
		case (HDLC_FLAG_RXC_RXCPIN | HDLC_FLAG_TXC_BRG):    new_line.clock_type = CLOCK_TXINT; break;
		case (HDLC_FLAG_RXC_RXCPIN | HDLC_FLAG_TXC_RXCPIN): new_line.clock_type = CLOCK_TXFROMRX; break;
		default: new_line.clock_type = CLOCK_DEFAULT;
		}

		new_line.clock_rate = info->params.clock_speed;
		new_line.loopback   = info->params.loopback ? 1:0;

		if (copy_to_user(line, &new_line, size))
			return -EFAULT;
		return 0;

	case IF_IFACE_SYNC_SERIAL: /* set sync_serial_settings */

		if(!capable(CAP_NET_ADMIN))
			return -EPERM;
		if (copy_from_user(&new_line, line, size))
			return -EFAULT;

		switch (new_line.clock_type)
		{
		case CLOCK_EXT:      flags = HDLC_FLAG_RXC_RXCPIN | HDLC_FLAG_TXC_TXCPIN; break;
		case CLOCK_TXFROMRX: flags = HDLC_FLAG_RXC_RXCPIN | HDLC_FLAG_TXC_RXCPIN; break;
		case CLOCK_INT:      flags = HDLC_FLAG_RXC_BRG    | HDLC_FLAG_TXC_BRG;    break;
		case CLOCK_TXINT:    flags = HDLC_FLAG_RXC_RXCPIN | HDLC_FLAG_TXC_BRG;    break;
		case CLOCK_DEFAULT:  flags = info->params.flags &
					     (HDLC_FLAG_RXC_RXCPIN | HDLC_FLAG_RXC_DPLL |
					      HDLC_FLAG_RXC_BRG    | HDLC_FLAG_RXC_TXCPIN |
					      HDLC_FLAG_TXC_TXCPIN | HDLC_FLAG_TXC_DPLL |
					      HDLC_FLAG_TXC_BRG    | HDLC_FLAG_TXC_RXCPIN); break;
		default: return -EINVAL;
		}

		if (new_line.loopback != 0 && new_line.loopback != 1)
			return -EINVAL;

		info->params.flags &= ~(HDLC_FLAG_RXC_RXCPIN | HDLC_FLAG_RXC_DPLL |
					HDLC_FLAG_RXC_BRG    | HDLC_FLAG_RXC_TXCPIN |
					HDLC_FLAG_TXC_TXCPIN | HDLC_FLAG_TXC_DPLL |
					HDLC_FLAG_TXC_BRG    | HDLC_FLAG_TXC_RXCPIN);
		info->params.flags |= flags;

		info->params.loopback = new_line.loopback;

		if (flags & (HDLC_FLAG_RXC_BRG | HDLC_FLAG_TXC_BRG))
			info->params.clock_speed = new_line.clock_rate;
		else
			info->params.clock_speed = 0;

		/* if network interface up, reprogram hardware */
		if (info->netcount)
			program_hw(info);
		return 0;

	default:
		return hdlc_ioctl(dev, ifr, cmd);
	}
}

/**
 * called by network layer when transmit timeout is detected
 *
 * dev  pointer to network device structure
 */
static void hdlcdev_tx_timeout(struct net_device *dev)
{
	SLMP_INFO *info = dev_to_port(dev);
	unsigned long flags;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("hdlcdev_tx_timeout(%s)\n",dev->name);

	dev->stats.tx_errors++;
	dev->stats.tx_aborted_errors++;

	spin_lock_irqsave(&info->lock,flags);
	tx_stop(info);
	spin_unlock_irqrestore(&info->lock,flags);

	netif_wake_queue(dev);
}

/**
 * called by device driver when transmit completes
 * reenable network layer transmit if stopped
 *
 * info  pointer to device instance information
 */
static void hdlcdev_tx_done(SLMP_INFO *info)
{
	if (netif_queue_stopped(info->netdev))
		netif_wake_queue(info->netdev);
}

/**
 * called by device driver when frame received
 * pass frame to network layer
 *
 * info  pointer to device instance information
 * buf   pointer to buffer contianing frame data
 * size  count of data bytes in buf
 */
static void hdlcdev_rx(SLMP_INFO *info, char *buf, int size)
{
	struct sk_buff *skb = dev_alloc_skb(size);
	struct net_device *dev = info->netdev;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("hdlcdev_rx(%s)\n",dev->name);

	if (skb == NULL) {
		printk(KERN_NOTICE "%s: can't alloc skb, dropping packet\n",
		       dev->name);
		dev->stats.rx_dropped++;
		return;
	}

	memcpy(skb_put(skb, size), buf, size);

	skb->protocol = hdlc_type_trans(skb, dev);

	dev->stats.rx_packets++;
	dev->stats.rx_bytes += size;

	netif_rx(skb);
}

static const struct net_device_ops hdlcdev_ops = {
	.ndo_open       = hdlcdev_open,
	.ndo_stop       = hdlcdev_close,
	.ndo_change_mtu = hdlc_change_mtu,
	.ndo_start_xmit = hdlc_start_xmit,
	.ndo_do_ioctl   = hdlcdev_ioctl,
	.ndo_tx_timeout = hdlcdev_tx_timeout,
};

/**
 * called by device driver when adding device instance
 * do generic HDLC initialization
 *
 * info  pointer to device instance information
 *
 * returns 0 if success, otherwise error code
 */
static int hdlcdev_init(SLMP_INFO *info)
{
	int rc;
	struct net_device *dev;
	hdlc_device *hdlc;

	/* allocate and initialize network and HDLC layer objects */

	if (!(dev = alloc_hdlcdev(info))) {
		printk(KERN_ERR "%s:hdlc device allocation failure\n",__FILE__);
		return -ENOMEM;
	}

	/* for network layer reporting purposes only */
	dev->mem_start = info->phys_sca_base;
	dev->mem_end   = info->phys_sca_base + SCA_BASE_SIZE - 1;
	dev->irq       = info->irq_level;

	/* network layer callbacks and settings */
	dev->netdev_ops	    = &hdlcdev_ops;
	dev->watchdog_timeo = 10 * HZ;
	dev->tx_queue_len   = 50;

	/* generic HDLC layer callbacks and settings */
	hdlc         = dev_to_hdlc(dev);
	hdlc->attach = hdlcdev_attach;
	hdlc->xmit   = hdlcdev_xmit;

	/* register objects with HDLC layer */
	if ((rc = register_hdlc_device(dev))) {
		printk(KERN_WARNING "%s:unable to register hdlc device\n",__FILE__);
		free_netdev(dev);
		return rc;
	}

	info->netdev = dev;
	return 0;
}

/**
 * called by device driver when removing device instance
 * do generic HDLC cleanup
 *
 * info  pointer to device instance information
 */
static void hdlcdev_exit(SLMP_INFO *info)
{
	unregister_hdlc_device(info->netdev);
	free_netdev(info->netdev);
	info->netdev = NULL;
}

#endif /* CONFIG_HDLC */


/* Return next bottom half action to perform.
 * Return Value:	BH action code or 0 if nothing to do.
 */
static int bh_action(SLMP_INFO *info)
{
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&info->lock,flags);

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

	spin_unlock_irqrestore(&info->lock,flags);

	return rc;
}

/* Perform bottom half processing of work items queued by ISR.
 */
static void bh_handler(struct work_struct *work)
{
	SLMP_INFO *info = container_of(work, SLMP_INFO, task);
	int action;

	if (!info)
		return;

	if ( debug_level >= DEBUG_LEVEL_BH )
		printk( "%s(%d):%s bh_handler() entry\n",
			__FILE__,__LINE__,info->device_name);

	info->bh_running = true;

	while((action = bh_action(info)) != 0) {

		/* Process work item */
		if ( debug_level >= DEBUG_LEVEL_BH )
			printk( "%s(%d):%s bh_handler() work item action=%d\n",
				__FILE__,__LINE__,info->device_name, action);

		switch (action) {

		case BH_RECEIVE:
			bh_receive(info);
			break;
		case BH_TRANSMIT:
			bh_transmit(info);
			break;
		case BH_STATUS:
			bh_status(info);
			break;
		default:
			/* unknown work item ID */
			printk("%s(%d):%s Unknown work item ID=%08X!\n",
				__FILE__,__LINE__,info->device_name,action);
			break;
		}
	}

	if ( debug_level >= DEBUG_LEVEL_BH )
		printk( "%s(%d):%s bh_handler() exit\n",
			__FILE__,__LINE__,info->device_name);
}

static void bh_receive(SLMP_INFO *info)
{
	if ( debug_level >= DEBUG_LEVEL_BH )
		printk( "%s(%d):%s bh_receive()\n",
			__FILE__,__LINE__,info->device_name);

	while( rx_get_frame(info) );
}

static void bh_transmit(SLMP_INFO *info)
{
	struct tty_struct *tty = info->port.tty;

	if ( debug_level >= DEBUG_LEVEL_BH )
		printk( "%s(%d):%s bh_transmit() entry\n",
			__FILE__,__LINE__,info->device_name);

	if (tty)
		tty_wakeup(tty);
}

static void bh_status(SLMP_INFO *info)
{
	if ( debug_level >= DEBUG_LEVEL_BH )
		printk( "%s(%d):%s bh_status() entry\n",
			__FILE__,__LINE__,info->device_name);

	info->ri_chkcount = 0;
	info->dsr_chkcount = 0;
	info->dcd_chkcount = 0;
	info->cts_chkcount = 0;
}

static void isr_timer(SLMP_INFO * info)
{
	unsigned char timer = (info->port_num & 1) ? TIMER2 : TIMER0;

	/* IER2<7..4> = timer<3..0> interrupt enables (0=disabled) */
	write_reg(info, IER2, 0);

	/* TMCS, Timer Control/Status Register
	 *
	 * 07      CMF, Compare match flag (read only) 1=match
	 * 06      ECMI, CMF Interrupt Enable: 0=disabled
	 * 05      Reserved, must be 0
	 * 04      TME, Timer Enable
	 * 03..00  Reserved, must be 0
	 *
	 * 0000 0000
	 */
	write_reg(info, (unsigned char)(timer + TMCS), 0);

	info->irq_occurred = true;

	if ( debug_level >= DEBUG_LEVEL_ISR )
		printk("%s(%d):%s isr_timer()\n",
			__FILE__,__LINE__,info->device_name);
}

static void isr_rxint(SLMP_INFO * info)
{
 	struct tty_struct *tty = info->port.tty;
 	struct	mgsl_icount *icount = &info->icount;
	unsigned char status = read_reg(info, SR1) & info->ie1_value & (FLGD + IDLD + CDCD + BRKD);
	unsigned char status2 = read_reg(info, SR2) & info->ie2_value & OVRN;

	/* clear status bits */
	if (status)
		write_reg(info, SR1, status);

	if (status2)
		write_reg(info, SR2, status2);
	
	if ( debug_level >= DEBUG_LEVEL_ISR )
		printk("%s(%d):%s isr_rxint status=%02X %02x\n",
			__FILE__,__LINE__,info->device_name,status,status2);

	if (info->params.mode == MGSL_MODE_ASYNC) {
		if (status & BRKD) {
			icount->brk++;

			/* process break detection if tty control
			 * is not set to ignore it
			 */
			if ( tty ) {
				if (!(status & info->ignore_status_mask1)) {
					if (info->read_status_mask1 & BRKD) {
						tty_insert_flip_char(tty, 0, TTY_BREAK);
						if (info->port.flags & ASYNC_SAK)
							do_SAK(tty);
					}
				}
			}
		}
	}
	else {
		if (status & (FLGD|IDLD)) {
			if (status & FLGD)
				info->icount.exithunt++;
			else if (status & IDLD)
				info->icount.rxidle++;
			wake_up_interruptible(&info->event_wait_q);
		}
	}

	if (status & CDCD) {
		/* simulate a common modem status change interrupt
		 * for our handler
		 */
		get_signals( info );
		isr_io_pin(info,
			MISCSTATUS_DCD_LATCHED|(info->serial_signals&SerialSignal_DCD));
	}
}

/*
 * handle async rx data interrupts
 */
static void isr_rxrdy(SLMP_INFO * info)
{
	u16 status;
	unsigned char DataByte;
 	struct tty_struct *tty = info->port.tty;
 	struct	mgsl_icount *icount = &info->icount;

	if ( debug_level >= DEBUG_LEVEL_ISR )
		printk("%s(%d):%s isr_rxrdy\n",
			__FILE__,__LINE__,info->device_name);

	while((status = read_reg(info,CST0)) & BIT0)
	{
		int flag = 0;
		bool over = false;
		DataByte = read_reg(info,TRB);

		icount->rx++;

		if ( status & (PE + FRME + OVRN) ) {
			printk("%s(%d):%s rxerr=%04X\n",
				__FILE__,__LINE__,info->device_name,status);

			/* update error statistics */
			if (status & PE)
				icount->parity++;
			else if (status & FRME)
				icount->frame++;
			else if (status & OVRN)
				icount->overrun++;

			/* discard char if tty control flags say so */
			if (status & info->ignore_status_mask2)
				continue;

			status &= info->read_status_mask2;

			if ( tty ) {
				if (status & PE)
					flag = TTY_PARITY;
				else if (status & FRME)
					flag = TTY_FRAME;
				if (status & OVRN) {
					/* Overrun is special, since it's
					 * reported immediately, and doesn't
					 * affect the current character
					 */
					over = true;
				}
			}
		}	/* end of if (error) */

		if ( tty ) {
			tty_insert_flip_char(tty, DataByte, flag);
			if (over)
				tty_insert_flip_char(tty, 0, TTY_OVERRUN);
		}
	}

	if ( debug_level >= DEBUG_LEVEL_ISR ) {
		printk("%s(%d):%s rx=%d brk=%d parity=%d frame=%d overrun=%d\n",
			__FILE__,__LINE__,info->device_name,
			icount->rx,icount->brk,icount->parity,
			icount->frame,icount->overrun);
	}

	if ( tty )
		tty_flip_buffer_push(tty);
}

static void isr_txeom(SLMP_INFO * info, unsigned char status)
{
	if ( debug_level >= DEBUG_LEVEL_ISR )
		printk("%s(%d):%s isr_txeom status=%02x\n",
			__FILE__,__LINE__,info->device_name,status);

	write_reg(info, TXDMA + DIR, 0x00); /* disable Tx DMA IRQs */
	write_reg(info, TXDMA + DSR, 0xc0); /* clear IRQs and disable DMA */
	write_reg(info, TXDMA + DCMD, SWABORT);	/* reset/init DMA channel */

	if (status & UDRN) {
		write_reg(info, CMD, TXRESET);
		write_reg(info, CMD, TXENABLE);
	} else
		write_reg(info, CMD, TXBUFCLR);

	/* disable and clear tx interrupts */
	info->ie0_value &= ~TXRDYE;
	info->ie1_value &= ~(IDLE + UDRN);
	write_reg16(info, IE0, (unsigned short)((info->ie1_value << 8) + info->ie0_value));
	write_reg(info, SR1, (unsigned char)(UDRN + IDLE));

	if ( info->tx_active ) {
		if (info->params.mode != MGSL_MODE_ASYNC) {
			if (status & UDRN)
				info->icount.txunder++;
			else if (status & IDLE)
				info->icount.txok++;
		}

		info->tx_active = false;
		info->tx_count = info->tx_put = info->tx_get = 0;

		del_timer(&info->tx_timer);

		if (info->params.mode != MGSL_MODE_ASYNC && info->drop_rts_on_tx_done ) {
			info->serial_signals &= ~SerialSignal_RTS;
			info->drop_rts_on_tx_done = false;
			set_signals(info);
		}

#if SYNCLINK_GENERIC_HDLC
		if (info->netcount)
			hdlcdev_tx_done(info);
		else
#endif
		{
			if (info->port.tty && (info->port.tty->stopped || info->port.tty->hw_stopped)) {
				tx_stop(info);
				return;
			}
			info->pending_bh |= BH_TRANSMIT;
		}
	}
}


/*
 * handle tx status interrupts
 */
static void isr_txint(SLMP_INFO * info)
{
	unsigned char status = read_reg(info, SR1) & info->ie1_value & (UDRN + IDLE + CCTS);

	/* clear status bits */
	write_reg(info, SR1, status);

	if ( debug_level >= DEBUG_LEVEL_ISR )
		printk("%s(%d):%s isr_txint status=%02x\n",
			__FILE__,__LINE__,info->device_name,status);

	if (status & (UDRN + IDLE))
		isr_txeom(info, status);

	if (status & CCTS) {
		/* simulate a common modem status change interrupt
		 * for our handler
		 */
		get_signals( info );
		isr_io_pin(info,
			MISCSTATUS_CTS_LATCHED|(info->serial_signals&SerialSignal_CTS));

	}
}

/*
 * handle async tx data interrupts
 */
static void isr_txrdy(SLMP_INFO * info)
{
	if ( debug_level >= DEBUG_LEVEL_ISR )
		printk("%s(%d):%s isr_txrdy() tx_count=%d\n",
			__FILE__,__LINE__,info->device_name,info->tx_count);

	if (info->params.mode != MGSL_MODE_ASYNC) {
		/* disable TXRDY IRQ, enable IDLE IRQ */
		info->ie0_value &= ~TXRDYE;
		info->ie1_value |= IDLE;
		write_reg16(info, IE0, (unsigned short)((info->ie1_value << 8) + info->ie0_value));
		return;
	}

	if (info->port.tty && (info->port.tty->stopped || info->port.tty->hw_stopped)) {
		tx_stop(info);
		return;
	}

	if ( info->tx_count )
		tx_load_fifo( info );
	else {
		info->tx_active = false;
		info->ie0_value &= ~TXRDYE;
		write_reg(info, IE0, info->ie0_value);
	}

	if (info->tx_count < WAKEUP_CHARS)
		info->pending_bh |= BH_TRANSMIT;
}

static void isr_rxdmaok(SLMP_INFO * info)
{
	/* BIT7 = EOT (end of transfer)
	 * BIT6 = EOM (end of message/frame)
	 */
	unsigned char status = read_reg(info,RXDMA + DSR) & 0xc0;

	/* clear IRQ (BIT0 must be 1 to prevent clearing DE bit) */
	write_reg(info, RXDMA + DSR, (unsigned char)(status | 1));

	if ( debug_level >= DEBUG_LEVEL_ISR )
		printk("%s(%d):%s isr_rxdmaok(), status=%02x\n",
			__FILE__,__LINE__,info->device_name,status);

	info->pending_bh |= BH_RECEIVE;
}

static void isr_rxdmaerror(SLMP_INFO * info)
{
	/* BIT5 = BOF (buffer overflow)
	 * BIT4 = COF (counter overflow)
	 */
	unsigned char status = read_reg(info,RXDMA + DSR) & 0x30;

	/* clear IRQ (BIT0 must be 1 to prevent clearing DE bit) */
	write_reg(info, RXDMA + DSR, (unsigned char)(status | 1));

	if ( debug_level >= DEBUG_LEVEL_ISR )
		printk("%s(%d):%s isr_rxdmaerror(), status=%02x\n",
			__FILE__,__LINE__,info->device_name,status);

	info->rx_overflow = true;
	info->pending_bh |= BH_RECEIVE;
}

static void isr_txdmaok(SLMP_INFO * info)
{
	unsigned char status_reg1 = read_reg(info, SR1);

	write_reg(info, TXDMA + DIR, 0x00);	/* disable Tx DMA IRQs */
	write_reg(info, TXDMA + DSR, 0xc0); /* clear IRQs and disable DMA */
	write_reg(info, TXDMA + DCMD, SWABORT);	/* reset/init DMA channel */

	if ( debug_level >= DEBUG_LEVEL_ISR )
		printk("%s(%d):%s isr_txdmaok(), status=%02x\n",
			__FILE__,__LINE__,info->device_name,status_reg1);

	/* program TXRDY as FIFO empty flag, enable TXRDY IRQ */
	write_reg16(info, TRC0, 0);
	info->ie0_value |= TXRDYE;
	write_reg(info, IE0, info->ie0_value);
}

static void isr_txdmaerror(SLMP_INFO * info)
{
	/* BIT5 = BOF (buffer overflow)
	 * BIT4 = COF (counter overflow)
	 */
	unsigned char status = read_reg(info,TXDMA + DSR) & 0x30;

	/* clear IRQ (BIT0 must be 1 to prevent clearing DE bit) */
	write_reg(info, TXDMA + DSR, (unsigned char)(status | 1));

	if ( debug_level >= DEBUG_LEVEL_ISR )
		printk("%s(%d):%s isr_txdmaerror(), status=%02x\n",
			__FILE__,__LINE__,info->device_name,status);
}

/* handle input serial signal changes
 */
static void isr_io_pin( SLMP_INFO *info, u16 status )
{
 	struct	mgsl_icount *icount;

	if ( debug_level >= DEBUG_LEVEL_ISR )
		printk("%s(%d):isr_io_pin status=%04X\n",
			__FILE__,__LINE__,status);

	if (status & (MISCSTATUS_CTS_LATCHED | MISCSTATUS_DCD_LATCHED |
	              MISCSTATUS_DSR_LATCHED | MISCSTATUS_RI_LATCHED) ) {
		icount = &info->icount;
		/* update input line counters */
		if (status & MISCSTATUS_RI_LATCHED) {
			icount->rng++;
			if ( status & SerialSignal_RI )
				info->input_signal_events.ri_up++;
			else
				info->input_signal_events.ri_down++;
		}
		if (status & MISCSTATUS_DSR_LATCHED) {
			icount->dsr++;
			if ( status & SerialSignal_DSR )
				info->input_signal_events.dsr_up++;
			else
				info->input_signal_events.dsr_down++;
		}
		if (status & MISCSTATUS_DCD_LATCHED) {
			if ((info->dcd_chkcount)++ >= IO_PIN_SHUTDOWN_LIMIT) {
				info->ie1_value &= ~CDCD;
				write_reg(info, IE1, info->ie1_value);
			}
			icount->dcd++;
			if (status & SerialSignal_DCD) {
				info->input_signal_events.dcd_up++;
			} else
				info->input_signal_events.dcd_down++;
#if SYNCLINK_GENERIC_HDLC
			if (info->netcount) {
				if (status & SerialSignal_DCD)
					netif_carrier_on(info->netdev);
				else
					netif_carrier_off(info->netdev);
			}
#endif
		}
		if (status & MISCSTATUS_CTS_LATCHED)
		{
			if ((info->cts_chkcount)++ >= IO_PIN_SHUTDOWN_LIMIT) {
				info->ie1_value &= ~CCTS;
				write_reg(info, IE1, info->ie1_value);
			}
			icount->cts++;
			if ( status & SerialSignal_CTS )
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
				       (status & SerialSignal_DCD) ? "on" : "off");
			if (status & SerialSignal_DCD)
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
			if ( info->port.tty ) {
				if (info->port.tty->hw_stopped) {
					if (status & SerialSignal_CTS) {
						if ( debug_level >= DEBUG_LEVEL_ISR )
							printk("CTS tx start...");
			 			info->port.tty->hw_stopped = 0;
						tx_start(info);
						info->pending_bh |= BH_TRANSMIT;
						return;
					}
				} else {
					if (!(status & SerialSignal_CTS)) {
						if ( debug_level >= DEBUG_LEVEL_ISR )
							printk("CTS tx stop...");
			 			info->port.tty->hw_stopped = 1;
						tx_stop(info);
					}
				}
			}
		}
	}

	info->pending_bh |= BH_STATUS;
}

/* Interrupt service routine entry point.
 *
 * Arguments:
 * 	irq		interrupt number that caused interrupt
 * 	dev_id		device ID supplied during interrupt registration
 * 	regs		interrupted processor context
 */
static irqreturn_t synclinkmp_interrupt(int dummy, void *dev_id)
{
	SLMP_INFO *info = dev_id;
	unsigned char status, status0, status1=0;
	unsigned char dmastatus, dmastatus0, dmastatus1=0;
	unsigned char timerstatus0, timerstatus1=0;
	unsigned char shift;
	unsigned int i;
	unsigned short tmp;

	if ( debug_level >= DEBUG_LEVEL_ISR )
		printk(KERN_DEBUG "%s(%d): synclinkmp_interrupt(%d)entry.\n",
			__FILE__, __LINE__, info->irq_level);

	spin_lock(&info->lock);

	for(;;) {

		/* get status for SCA0 (ports 0-1) */
		tmp = read_reg16(info, ISR0);	/* get ISR0 and ISR1 in one read */
		status0 = (unsigned char)tmp;
		dmastatus0 = (unsigned char)(tmp>>8);
		timerstatus0 = read_reg(info, ISR2);

		if ( debug_level >= DEBUG_LEVEL_ISR )
			printk(KERN_DEBUG "%s(%d):%s status0=%02x, dmastatus0=%02x, timerstatus0=%02x\n",
				__FILE__, __LINE__, info->device_name,
				status0, dmastatus0, timerstatus0);

		if (info->port_count == 4) {
			/* get status for SCA1 (ports 2-3) */
			tmp = read_reg16(info->port_array[2], ISR0);
			status1 = (unsigned char)tmp;
			dmastatus1 = (unsigned char)(tmp>>8);
			timerstatus1 = read_reg(info->port_array[2], ISR2);

			if ( debug_level >= DEBUG_LEVEL_ISR )
				printk("%s(%d):%s status1=%02x, dmastatus1=%02x, timerstatus1=%02x\n",
					__FILE__,__LINE__,info->device_name,
					status1,dmastatus1,timerstatus1);
		}

		if (!status0 && !dmastatus0 && !timerstatus0 &&
			 !status1 && !dmastatus1 && !timerstatus1)
			break;

		for(i=0; i < info->port_count ; i++) {
			if (info->port_array[i] == NULL)
				continue;
			if (i < 2) {
				status = status0;
				dmastatus = dmastatus0;
			} else {
				status = status1;
				dmastatus = dmastatus1;
			}

			shift = i & 1 ? 4 :0;

			if (status & BIT0 << shift)
				isr_rxrdy(info->port_array[i]);
			if (status & BIT1 << shift)
				isr_txrdy(info->port_array[i]);
			if (status & BIT2 << shift)
				isr_rxint(info->port_array[i]);
			if (status & BIT3 << shift)
				isr_txint(info->port_array[i]);

			if (dmastatus & BIT0 << shift)
				isr_rxdmaerror(info->port_array[i]);
			if (dmastatus & BIT1 << shift)
				isr_rxdmaok(info->port_array[i]);
			if (dmastatus & BIT2 << shift)
				isr_txdmaerror(info->port_array[i]);
			if (dmastatus & BIT3 << shift)
				isr_txdmaok(info->port_array[i]);
		}

		if (timerstatus0 & (BIT5 | BIT4))
			isr_timer(info->port_array[0]);
		if (timerstatus0 & (BIT7 | BIT6))
			isr_timer(info->port_array[1]);
		if (timerstatus1 & (BIT5 | BIT4))
			isr_timer(info->port_array[2]);
		if (timerstatus1 & (BIT7 | BIT6))
			isr_timer(info->port_array[3]);
	}

	for(i=0; i < info->port_count ; i++) {
		SLMP_INFO * port = info->port_array[i];

		/* Request bottom half processing if there's something
		 * for it to do and the bh is not already running.
		 *
		 * Note: startup adapter diags require interrupts.
		 * do not request bottom half processing if the
		 * device is not open in a normal mode.
		 */
		if ( port && (port->port.count || port->netcount) &&
		     port->pending_bh && !port->bh_running &&
		     !port->bh_requested ) {
			if ( debug_level >= DEBUG_LEVEL_ISR )
				printk("%s(%d):%s queueing bh task.\n",
					__FILE__,__LINE__,port->device_name);
			schedule_work(&port->task);
			port->bh_requested = true;
		}
	}

	spin_unlock(&info->lock);

	if ( debug_level >= DEBUG_LEVEL_ISR )
		printk(KERN_DEBUG "%s(%d):synclinkmp_interrupt(%d)exit.\n",
			__FILE__, __LINE__, info->irq_level);
	return IRQ_HANDLED;
}

/* Initialize and start device.
 */
static int startup(SLMP_INFO * info)
{
	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk("%s(%d):%s tx_releaseup()\n",__FILE__,__LINE__,info->device_name);

	if (info->port.flags & ASYNC_INITIALIZED)
		return 0;

	if (!info->tx_buf) {
		info->tx_buf = kmalloc(info->max_frame_size, GFP_KERNEL);
		if (!info->tx_buf) {
			printk(KERN_ERR"%s(%d):%s can't allocate transmit buffer\n",
				__FILE__,__LINE__,info->device_name);
			return -ENOMEM;
		}
	}

	info->pending_bh = 0;

	memset(&info->icount, 0, sizeof(info->icount));

	/* program hardware for current parameters */
	reset_port(info);

	change_params(info);

	mod_timer(&info->status_timer, jiffies + msecs_to_jiffies(10));

	if (info->port.tty)
		clear_bit(TTY_IO_ERROR, &info->port.tty->flags);

	info->port.flags |= ASYNC_INITIALIZED;

	return 0;
}

/* Called by close() and hangup() to shutdown hardware
 */
static void shutdown(SLMP_INFO * info)
{
	unsigned long flags;

	if (!(info->port.flags & ASYNC_INITIALIZED))
		return;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s synclinkmp_shutdown()\n",
			 __FILE__,__LINE__, info->device_name );

	/* clear status wait queue because status changes */
	/* can't happen after shutting down the hardware */
	wake_up_interruptible(&info->status_event_wait_q);
	wake_up_interruptible(&info->event_wait_q);

	del_timer(&info->tx_timer);
	del_timer(&info->status_timer);

	kfree(info->tx_buf);
	info->tx_buf = NULL;

	spin_lock_irqsave(&info->lock,flags);

	reset_port(info);

 	if (!info->port.tty || info->port.tty->termios->c_cflag & HUPCL) {
 		info->serial_signals &= ~(SerialSignal_DTR + SerialSignal_RTS);
		set_signals(info);
	}

	spin_unlock_irqrestore(&info->lock,flags);

	if (info->port.tty)
		set_bit(TTY_IO_ERROR, &info->port.tty->flags);

	info->port.flags &= ~ASYNC_INITIALIZED;
}

static void program_hw(SLMP_INFO *info)
{
	unsigned long flags;

	spin_lock_irqsave(&info->lock,flags);

	rx_stop(info);
	tx_stop(info);

	info->tx_count = info->tx_put = info->tx_get = 0;

	if (info->params.mode == MGSL_MODE_HDLC || info->netcount)
		hdlc_mode(info);
	else
		async_mode(info);

	set_signals(info);

	info->dcd_chkcount = 0;
	info->cts_chkcount = 0;
	info->ri_chkcount = 0;
	info->dsr_chkcount = 0;

	info->ie1_value |= (CDCD|CCTS);
	write_reg(info, IE1, info->ie1_value);

	get_signals(info);

	if (info->netcount || (info->port.tty && info->port.tty->termios->c_cflag & CREAD) )
		rx_start(info);

	spin_unlock_irqrestore(&info->lock,flags);
}

/* Reconfigure adapter based on new parameters
 */
static void change_params(SLMP_INFO *info)
{
	unsigned cflag;
	int bits_per_char;

	if (!info->port.tty || !info->port.tty->termios)
		return;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s change_params()\n",
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
	if (info->params.data_rate <= 460800) {
		info->params.data_rate = tty_get_baud_rate(info->port.tty);
	}

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

	info->read_status_mask2 = OVRN;
	if (I_INPCK(info->port.tty))
		info->read_status_mask2 |= PE | FRME;
 	if (I_BRKINT(info->port.tty) || I_PARMRK(info->port.tty))
 		info->read_status_mask1 |= BRKD;
	if (I_IGNPAR(info->port.tty))
		info->ignore_status_mask2 |= PE | FRME;
	if (I_IGNBRK(info->port.tty)) {
		info->ignore_status_mask1 |= BRKD;
		/* If ignoring parity and break indicators, ignore
		 * overruns too.  (For real raw support).
		 */
		if (I_IGNPAR(info->port.tty))
			info->ignore_status_mask2 |= OVRN;
	}

	program_hw(info);
}

static int get_stats(SLMP_INFO * info, struct mgsl_icount __user *user_icount)
{
	int err;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s get_params()\n",
			 __FILE__,__LINE__, info->device_name);

	if (!user_icount) {
		memset(&info->icount, 0, sizeof(info->icount));
	} else {
		COPY_TO_USER(err, user_icount, &info->icount, sizeof(struct mgsl_icount));
		if (err)
			return -EFAULT;
	}

	return 0;
}

static int get_params(SLMP_INFO * info, MGSL_PARAMS __user *user_params)
{
	int err;
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s get_params()\n",
			 __FILE__,__LINE__, info->device_name);

	COPY_TO_USER(err,user_params, &info->params, sizeof(MGSL_PARAMS));
	if (err) {
		if ( debug_level >= DEBUG_LEVEL_INFO )
			printk( "%s(%d):%s get_params() user buffer copy failed\n",
				__FILE__,__LINE__,info->device_name);
		return -EFAULT;
	}

	return 0;
}

static int set_params(SLMP_INFO * info, MGSL_PARAMS __user *new_params)
{
 	unsigned long flags;
	MGSL_PARAMS tmp_params;
	int err;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s set_params\n",
			__FILE__,__LINE__,info->device_name );
	COPY_FROM_USER(err,&tmp_params, new_params, sizeof(MGSL_PARAMS));
	if (err) {
		if ( debug_level >= DEBUG_LEVEL_INFO )
			printk( "%s(%d):%s set_params() user buffer copy failed\n",
				__FILE__,__LINE__,info->device_name);
		return -EFAULT;
	}

	spin_lock_irqsave(&info->lock,flags);
	memcpy(&info->params,&tmp_params,sizeof(MGSL_PARAMS));
	spin_unlock_irqrestore(&info->lock,flags);

 	change_params(info);

	return 0;
}

static int get_txidle(SLMP_INFO * info, int __user *idle_mode)
{
	int err;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s get_txidle()=%d\n",
			 __FILE__,__LINE__, info->device_name, info->idle_mode);

	COPY_TO_USER(err,idle_mode, &info->idle_mode, sizeof(int));
	if (err) {
		if ( debug_level >= DEBUG_LEVEL_INFO )
			printk( "%s(%d):%s get_txidle() user buffer copy failed\n",
				__FILE__,__LINE__,info->device_name);
		return -EFAULT;
	}

	return 0;
}

static int set_txidle(SLMP_INFO * info, int idle_mode)
{
 	unsigned long flags;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s set_txidle(%d)\n",
			__FILE__,__LINE__,info->device_name, idle_mode );

	spin_lock_irqsave(&info->lock,flags);
	info->idle_mode = idle_mode;
	tx_set_idle( info );
	spin_unlock_irqrestore(&info->lock,flags);
	return 0;
}

static int tx_enable(SLMP_INFO * info, int enable)
{
 	unsigned long flags;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s tx_enable(%d)\n",
			__FILE__,__LINE__,info->device_name, enable);

	spin_lock_irqsave(&info->lock,flags);
	if ( enable ) {
		if ( !info->tx_enabled ) {
			tx_start(info);
		}
	} else {
		if ( info->tx_enabled )
			tx_stop(info);
	}
	spin_unlock_irqrestore(&info->lock,flags);
	return 0;
}

/* abort send HDLC frame
 */
static int tx_abort(SLMP_INFO * info)
{
 	unsigned long flags;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s tx_abort()\n",
			__FILE__,__LINE__,info->device_name);

	spin_lock_irqsave(&info->lock,flags);
	if ( info->tx_active && info->params.mode == MGSL_MODE_HDLC ) {
		info->ie1_value &= ~UDRN;
		info->ie1_value |= IDLE;
		write_reg(info, IE1, info->ie1_value);	/* disable tx status interrupts */
		write_reg(info, SR1, (unsigned char)(IDLE + UDRN));	/* clear pending */

		write_reg(info, TXDMA + DSR, 0);		/* disable DMA channel */
		write_reg(info, TXDMA + DCMD, SWABORT);	/* reset/init DMA channel */

   		write_reg(info, CMD, TXABORT);
	}
	spin_unlock_irqrestore(&info->lock,flags);
	return 0;
}

static int rx_enable(SLMP_INFO * info, int enable)
{
 	unsigned long flags;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s rx_enable(%d)\n",
			__FILE__,__LINE__,info->device_name,enable);

	spin_lock_irqsave(&info->lock,flags);
	if ( enable ) {
		if ( !info->rx_enabled )
			rx_start(info);
	} else {
		if ( info->rx_enabled )
			rx_stop(info);
	}
	spin_unlock_irqrestore(&info->lock,flags);
	return 0;
}

/* wait for specified event to occur
 */
static int wait_mgsl_event(SLMP_INFO * info, int __user *mask_ptr)
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
		printk("%s(%d):%s wait_mgsl_event(%d)\n",
			__FILE__,__LINE__,info->device_name,mask);

	spin_lock_irqsave(&info->lock,flags);

	/* return immediately if state matches requested events */
	get_signals(info);
	s = info->serial_signals;

	events = mask &
		( ((s & SerialSignal_DSR) ? MgslEvent_DsrActive:MgslEvent_DsrInactive) +
 		  ((s & SerialSignal_DCD) ? MgslEvent_DcdActive:MgslEvent_DcdInactive) +
		  ((s & SerialSignal_CTS) ? MgslEvent_CtsActive:MgslEvent_CtsInactive) +
		  ((s & SerialSignal_RI)  ? MgslEvent_RiActive :MgslEvent_RiInactive) );
	if (events) {
		spin_unlock_irqrestore(&info->lock,flags);
		goto exit;
	}

	/* save current irq counts */
	cprev = info->icount;
	oldsigs = info->input_signal_events;

	/* enable hunt and idle irqs if needed */
	if (mask & (MgslEvent_ExitHuntMode+MgslEvent_IdleReceived)) {
		unsigned char oldval = info->ie1_value;
		unsigned char newval = oldval +
			 (mask & MgslEvent_ExitHuntMode ? FLGD:0) +
			 (mask & MgslEvent_IdleReceived ? IDLD:0);
		if ( oldval != newval ) {
			info->ie1_value = newval;
			write_reg(info, IE1, info->ie1_value);
		}
	}

	set_current_state(TASK_INTERRUPTIBLE);
	add_wait_queue(&info->event_wait_q, &wait);

	spin_unlock_irqrestore(&info->lock,flags);

	for(;;) {
		schedule();
		if (signal_pending(current)) {
			rc = -ERESTARTSYS;
			break;
		}

		/* get current irq counts */
		spin_lock_irqsave(&info->lock,flags);
		cnow = info->icount;
		newsigs = info->input_signal_events;
		set_current_state(TASK_INTERRUPTIBLE);
		spin_unlock_irqrestore(&info->lock,flags);

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
		spin_lock_irqsave(&info->lock,flags);
		if (!waitqueue_active(&info->event_wait_q)) {
			/* disable enable exit hunt mode/idle rcvd IRQs */
			info->ie1_value &= ~(FLGD|IDLD);
			write_reg(info, IE1, info->ie1_value);
		}
		spin_unlock_irqrestore(&info->lock,flags);
	}
exit:
	if ( rc == 0 )
		PUT_USER(rc, events, mask_ptr);

	return rc;
}

static int modem_input_wait(SLMP_INFO *info,int arg)
{
 	unsigned long flags;
	int rc;
	struct mgsl_icount cprev, cnow;
	DECLARE_WAITQUEUE(wait, current);

	/* save current irq counts */
	spin_lock_irqsave(&info->lock,flags);
	cprev = info->icount;
	add_wait_queue(&info->status_event_wait_q, &wait);
	set_current_state(TASK_INTERRUPTIBLE);
	spin_unlock_irqrestore(&info->lock,flags);

	for(;;) {
		schedule();
		if (signal_pending(current)) {
			rc = -ERESTARTSYS;
			break;
		}

		/* get new irq counts */
		spin_lock_irqsave(&info->lock,flags);
		cnow = info->icount;
		set_current_state(TASK_INTERRUPTIBLE);
		spin_unlock_irqrestore(&info->lock,flags);

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
	SLMP_INFO *info = tty->driver_data;
	unsigned int result;
 	unsigned long flags;

	spin_lock_irqsave(&info->lock,flags);
 	get_signals(info);
	spin_unlock_irqrestore(&info->lock,flags);

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
	SLMP_INFO *info = tty->driver_data;
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

	spin_lock_irqsave(&info->lock,flags);
 	set_signals(info);
	spin_unlock_irqrestore(&info->lock,flags);

	return 0;
}

static int carrier_raised(struct tty_port *port)
{
	SLMP_INFO *info = container_of(port, SLMP_INFO, port);
	unsigned long flags;

	spin_lock_irqsave(&info->lock,flags);
 	get_signals(info);
	spin_unlock_irqrestore(&info->lock,flags);

	return (info->serial_signals & SerialSignal_DCD) ? 1 : 0;
}

static void dtr_rts(struct tty_port *port, int on)
{
	SLMP_INFO *info = container_of(port, SLMP_INFO, port);
	unsigned long flags;

	spin_lock_irqsave(&info->lock,flags);
	if (on)
		info->serial_signals |= SerialSignal_RTS + SerialSignal_DTR;
	else
		info->serial_signals &= ~(SerialSignal_RTS + SerialSignal_DTR);
 	set_signals(info);
	spin_unlock_irqrestore(&info->lock,flags);
}

/* Block the current process until the specified port is ready to open.
 */
static int block_til_ready(struct tty_struct *tty, struct file *filp,
			   SLMP_INFO *info)
{
	DECLARE_WAITQUEUE(wait, current);
	int		retval;
	bool		do_clocal = false;
	bool		extra_count = false;
	unsigned long	flags;
	int		cd;
	struct tty_port *port = &info->port;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s block_til_ready()\n",
			 __FILE__,__LINE__, tty->driver->name );

	if (filp->f_flags & O_NONBLOCK || tty->flags & (1 << TTY_IO_ERROR)){
		/* nonblock mode is set or port is not enabled */
		/* just verify that callout device is not active */
		port->flags |= ASYNC_NORMAL_ACTIVE;
		return 0;
	}

	if (tty->termios->c_cflag & CLOCAL)
		do_clocal = true;

	/* Wait for carrier detect and the line to become
	 * free (i.e., not in use by the callout).  While we are in
	 * this loop, port->count is dropped by one, so that
	 * close() knows when to free things.  We restore it upon
	 * exit, either normal or abnormal.
	 */

	retval = 0;
	add_wait_queue(&port->open_wait, &wait);

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s block_til_ready() before block, count=%d\n",
			 __FILE__,__LINE__, tty->driver->name, port->count );

	spin_lock_irqsave(&info->lock, flags);
	if (!tty_hung_up_p(filp)) {
		extra_count = true;
		port->count--;
	}
	spin_unlock_irqrestore(&info->lock, flags);
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

		cd = tty_port_carrier_raised(port);

 		if (!(port->flags & ASYNC_CLOSING) && (do_clocal || cd))
 			break;

		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}

		if (debug_level >= DEBUG_LEVEL_INFO)
			printk("%s(%d):%s block_til_ready() count=%d\n",
				 __FILE__,__LINE__, tty->driver->name, port->count );

		schedule();
	}

	set_current_state(TASK_RUNNING);
	remove_wait_queue(&port->open_wait, &wait);

	if (extra_count)
		port->count++;
	port->blocked_open--;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s block_til_ready() after, count=%d\n",
			 __FILE__,__LINE__, tty->driver->name, port->count );

	if (!retval)
		port->flags |= ASYNC_NORMAL_ACTIVE;

	return retval;
}

static int alloc_dma_bufs(SLMP_INFO *info)
{
	unsigned short BuffersPerFrame;
	unsigned short BufferCount;

	// Force allocation to start at 64K boundary for each port.
	// This is necessary because *all* buffer descriptors for a port
	// *must* be in the same 64K block. All descriptors on a port
	// share a common 'base' address (upper 8 bits of 24 bits) programmed
	// into the CBP register.
	info->port_array[0]->last_mem_alloc = (SCA_MEM_SIZE/4) * info->port_num;

	/* Calculate the number of DMA buffers necessary to hold the */
	/* largest allowable frame size. Note: If the max frame size is */
	/* not an even multiple of the DMA buffer size then we need to */
	/* round the buffer count per frame up one. */

	BuffersPerFrame = (unsigned short)(info->max_frame_size/SCABUFSIZE);
	if ( info->max_frame_size % SCABUFSIZE )
		BuffersPerFrame++;

	/* calculate total number of data buffers (SCABUFSIZE) possible
	 * in one ports memory (SCA_MEM_SIZE/4) after allocating memory
	 * for the descriptor list (BUFFERLISTSIZE).
	 */
	BufferCount = (SCA_MEM_SIZE/4 - BUFFERLISTSIZE)/SCABUFSIZE;

	/* limit number of buffers to maximum amount of descriptors */
	if (BufferCount > BUFFERLISTSIZE/sizeof(SCADESC))
		BufferCount = BUFFERLISTSIZE/sizeof(SCADESC);

	/* use enough buffers to transmit one max size frame */
	info->tx_buf_count = BuffersPerFrame + 1;

	/* never use more than half the available buffers for transmit */
	if (info->tx_buf_count > (BufferCount/2))
		info->tx_buf_count = BufferCount/2;

	if (info->tx_buf_count > SCAMAXDESC)
		info->tx_buf_count = SCAMAXDESC;

	/* use remaining buffers for receive */
	info->rx_buf_count = BufferCount - info->tx_buf_count;

	if (info->rx_buf_count > SCAMAXDESC)
		info->rx_buf_count = SCAMAXDESC;

	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk("%s(%d):%s Allocating %d TX and %d RX DMA buffers.\n",
			__FILE__,__LINE__, info->device_name,
			info->tx_buf_count,info->rx_buf_count);

	if ( alloc_buf_list( info ) < 0 ||
		alloc_frame_bufs(info,
		  			info->rx_buf_list,
		  			info->rx_buf_list_ex,
					info->rx_buf_count) < 0 ||
		alloc_frame_bufs(info,
					info->tx_buf_list,
					info->tx_buf_list_ex,
					info->tx_buf_count) < 0 ||
		alloc_tmp_rx_buf(info) < 0 ) {
		printk("%s(%d):%s Can't allocate DMA buffer memory\n",
			__FILE__,__LINE__, info->device_name);
		return -ENOMEM;
	}

	rx_reset_buffers( info );

	return 0;
}

/* Allocate DMA buffers for the transmit and receive descriptor lists.
 */
static int alloc_buf_list(SLMP_INFO *info)
{
	unsigned int i;

	/* build list in adapter shared memory */
	info->buffer_list = info->memory_base + info->port_array[0]->last_mem_alloc;
	info->buffer_list_phys = info->port_array[0]->last_mem_alloc;
	info->port_array[0]->last_mem_alloc += BUFFERLISTSIZE;

	memset(info->buffer_list, 0, BUFFERLISTSIZE);

	/* Save virtual address pointers to the receive and */
	/* transmit buffer lists. (Receive 1st). These pointers will */
	/* be used by the processor to access the lists. */
	info->rx_buf_list = (SCADESC *)info->buffer_list;

	info->tx_buf_list = (SCADESC *)info->buffer_list;
	info->tx_buf_list += info->rx_buf_count;

	/* Build links for circular buffer entry lists (tx and rx)
	 *
	 * Note: links are physical addresses read by the SCA device
	 * to determine the next buffer entry to use.
	 */

	for ( i = 0; i < info->rx_buf_count; i++ ) {
		/* calculate and store physical address of this buffer entry */
		info->rx_buf_list_ex[i].phys_entry =
			info->buffer_list_phys + (i * sizeof(SCABUFSIZE));

		/* calculate and store physical address of */
		/* next entry in cirular list of entries */
		info->rx_buf_list[i].next = info->buffer_list_phys;
		if ( i < info->rx_buf_count - 1 )
			info->rx_buf_list[i].next += (i + 1) * sizeof(SCADESC);

		info->rx_buf_list[i].length = SCABUFSIZE;
	}

	for ( i = 0; i < info->tx_buf_count; i++ ) {
		/* calculate and store physical address of this buffer entry */
		info->tx_buf_list_ex[i].phys_entry = info->buffer_list_phys +
			((info->rx_buf_count + i) * sizeof(SCADESC));

		/* calculate and store physical address of */
		/* next entry in cirular list of entries */

		info->tx_buf_list[i].next = info->buffer_list_phys +
			info->rx_buf_count * sizeof(SCADESC);

		if ( i < info->tx_buf_count - 1 )
			info->tx_buf_list[i].next += (i + 1) * sizeof(SCADESC);
	}

	return 0;
}

/* Allocate the frame DMA buffers used by the specified buffer list.
 */
static int alloc_frame_bufs(SLMP_INFO *info, SCADESC *buf_list,SCADESC_EX *buf_list_ex,int count)
{
	int i;
	unsigned long phys_addr;

	for ( i = 0; i < count; i++ ) {
		buf_list_ex[i].virt_addr = info->memory_base + info->port_array[0]->last_mem_alloc;
		phys_addr = info->port_array[0]->last_mem_alloc;
		info->port_array[0]->last_mem_alloc += SCABUFSIZE;

		buf_list[i].buf_ptr  = (unsigned short)phys_addr;
		buf_list[i].buf_base = (unsigned char)(phys_addr >> 16);
	}

	return 0;
}

static void free_dma_bufs(SLMP_INFO *info)
{
	info->buffer_list = NULL;
	info->rx_buf_list = NULL;
	info->tx_buf_list = NULL;
}

/* allocate buffer large enough to hold max_frame_size.
 * This buffer is used to pass an assembled frame to the line discipline.
 */
static int alloc_tmp_rx_buf(SLMP_INFO *info)
{
	info->tmp_rx_buf = kmalloc(info->max_frame_size, GFP_KERNEL);
	if (info->tmp_rx_buf == NULL)
		return -ENOMEM;
	return 0;
}

static void free_tmp_rx_buf(SLMP_INFO *info)
{
	kfree(info->tmp_rx_buf);
	info->tmp_rx_buf = NULL;
}

static int claim_resources(SLMP_INFO *info)
{
	if (request_mem_region(info->phys_memory_base,SCA_MEM_SIZE,"synclinkmp") == NULL) {
		printk( "%s(%d):%s mem addr conflict, Addr=%08X\n",
			__FILE__,__LINE__,info->device_name, info->phys_memory_base);
		info->init_error = DiagStatus_AddressConflict;
		goto errout;
	}
	else
		info->shared_mem_requested = true;

	if (request_mem_region(info->phys_lcr_base + info->lcr_offset,128,"synclinkmp") == NULL) {
		printk( "%s(%d):%s lcr mem addr conflict, Addr=%08X\n",
			__FILE__,__LINE__,info->device_name, info->phys_lcr_base);
		info->init_error = DiagStatus_AddressConflict;
		goto errout;
	}
	else
		info->lcr_mem_requested = true;

	if (request_mem_region(info->phys_sca_base + info->sca_offset,SCA_BASE_SIZE,"synclinkmp") == NULL) {
		printk( "%s(%d):%s sca mem addr conflict, Addr=%08X\n",
			__FILE__,__LINE__,info->device_name, info->phys_sca_base);
		info->init_error = DiagStatus_AddressConflict;
		goto errout;
	}
	else
		info->sca_base_requested = true;

	if (request_mem_region(info->phys_statctrl_base + info->statctrl_offset,SCA_REG_SIZE,"synclinkmp") == NULL) {
		printk( "%s(%d):%s stat/ctrl mem addr conflict, Addr=%08X\n",
			__FILE__,__LINE__,info->device_name, info->phys_statctrl_base);
		info->init_error = DiagStatus_AddressConflict;
		goto errout;
	}
	else
		info->sca_statctrl_requested = true;

	info->memory_base = ioremap_nocache(info->phys_memory_base,
								SCA_MEM_SIZE);
	if (!info->memory_base) {
		printk( "%s(%d):%s Cant map shared memory, MemAddr=%08X\n",
			__FILE__,__LINE__,info->device_name, info->phys_memory_base );
		info->init_error = DiagStatus_CantAssignPciResources;
		goto errout;
	}

	info->lcr_base = ioremap_nocache(info->phys_lcr_base, PAGE_SIZE);
	if (!info->lcr_base) {
		printk( "%s(%d):%s Cant map LCR memory, MemAddr=%08X\n",
			__FILE__,__LINE__,info->device_name, info->phys_lcr_base );
		info->init_error = DiagStatus_CantAssignPciResources;
		goto errout;
	}
	info->lcr_base += info->lcr_offset;

	info->sca_base = ioremap_nocache(info->phys_sca_base, PAGE_SIZE);
	if (!info->sca_base) {
		printk( "%s(%d):%s Cant map SCA memory, MemAddr=%08X\n",
			__FILE__,__LINE__,info->device_name, info->phys_sca_base );
		info->init_error = DiagStatus_CantAssignPciResources;
		goto errout;
	}
	info->sca_base += info->sca_offset;

	info->statctrl_base = ioremap_nocache(info->phys_statctrl_base,
								PAGE_SIZE);
	if (!info->statctrl_base) {
		printk( "%s(%d):%s Cant map SCA Status/Control memory, MemAddr=%08X\n",
			__FILE__,__LINE__,info->device_name, info->phys_statctrl_base );
		info->init_error = DiagStatus_CantAssignPciResources;
		goto errout;
	}
	info->statctrl_base += info->statctrl_offset;

	if ( !memory_test(info) ) {
		printk( "%s(%d):Shared Memory Test failed for device %s MemAddr=%08X\n",
			__FILE__,__LINE__,info->device_name, info->phys_memory_base );
		info->init_error = DiagStatus_MemoryError;
		goto errout;
	}

	return 0;

errout:
	release_resources( info );
	return -ENODEV;
}

static void release_resources(SLMP_INFO *info)
{
	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk( "%s(%d):%s release_resources() entry\n",
			__FILE__,__LINE__,info->device_name );

	if ( info->irq_requested ) {
		free_irq(info->irq_level, info);
		info->irq_requested = false;
	}

	if ( info->shared_mem_requested ) {
		release_mem_region(info->phys_memory_base,SCA_MEM_SIZE);
		info->shared_mem_requested = false;
	}
	if ( info->lcr_mem_requested ) {
		release_mem_region(info->phys_lcr_base + info->lcr_offset,128);
		info->lcr_mem_requested = false;
	}
	if ( info->sca_base_requested ) {
		release_mem_region(info->phys_sca_base + info->sca_offset,SCA_BASE_SIZE);
		info->sca_base_requested = false;
	}
	if ( info->sca_statctrl_requested ) {
		release_mem_region(info->phys_statctrl_base + info->statctrl_offset,SCA_REG_SIZE);
		info->sca_statctrl_requested = false;
	}

	if (info->memory_base){
		iounmap(info->memory_base);
		info->memory_base = NULL;
	}

	if (info->sca_base) {
		iounmap(info->sca_base - info->sca_offset);
		info->sca_base=NULL;
	}

	if (info->statctrl_base) {
		iounmap(info->statctrl_base - info->statctrl_offset);
		info->statctrl_base=NULL;
	}

	if (info->lcr_base){
		iounmap(info->lcr_base - info->lcr_offset);
		info->lcr_base = NULL;
	}

	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk( "%s(%d):%s release_resources() exit\n",
			__FILE__,__LINE__,info->device_name );
}

/* Add the specified device instance data structure to the
 * global linked list of devices and increment the device count.
 */
static void add_device(SLMP_INFO *info)
{
	info->next_device = NULL;
	info->line = synclinkmp_device_count;
	sprintf(info->device_name,"ttySLM%dp%d",info->adapter_num,info->port_num);

	if (info->line < MAX_DEVICES) {
		if (maxframe[info->line])
			info->max_frame_size = maxframe[info->line];
	}

	synclinkmp_device_count++;

	if ( !synclinkmp_device_list )
		synclinkmp_device_list = info;
	else {
		SLMP_INFO *current_dev = synclinkmp_device_list;
		while( current_dev->next_device )
			current_dev = current_dev->next_device;
		current_dev->next_device = info;
	}

	if ( info->max_frame_size < 4096 )
		info->max_frame_size = 4096;
	else if ( info->max_frame_size > 65535 )
		info->max_frame_size = 65535;

	printk( "SyncLink MultiPort %s: "
		"Mem=(%08x %08X %08x %08X) IRQ=%d MaxFrameSize=%u\n",
		info->device_name,
		info->phys_sca_base,
		info->phys_memory_base,
		info->phys_statctrl_base,
		info->phys_lcr_base,
		info->irq_level,
		info->max_frame_size );

#if SYNCLINK_GENERIC_HDLC
	hdlcdev_init(info);
#endif
}

static const struct tty_port_operations port_ops = {
	.carrier_raised = carrier_raised,
	.dtr_rts = dtr_rts,
};

/* Allocate and initialize a device instance structure
 *
 * Return Value:	pointer to SLMP_INFO if success, otherwise NULL
 */
static SLMP_INFO *alloc_dev(int adapter_num, int port_num, struct pci_dev *pdev)
{
	SLMP_INFO *info;

	info = kzalloc(sizeof(SLMP_INFO),
		 GFP_KERNEL);

	if (!info) {
		printk("%s(%d) Error can't allocate device instance data for adapter %d, port %d\n",
			__FILE__,__LINE__, adapter_num, port_num);
	} else {
		tty_port_init(&info->port);
		info->port.ops = &port_ops;
		info->magic = MGSL_MAGIC;
		INIT_WORK(&info->task, bh_handler);
		info->max_frame_size = 4096;
		info->port.close_delay = 5*HZ/10;
		info->port.closing_wait = 30*HZ;
		init_waitqueue_head(&info->status_event_wait_q);
		init_waitqueue_head(&info->event_wait_q);
		spin_lock_init(&info->netlock);
		memcpy(&info->params,&default_ synclisizeof(MGSL_PARAMS));
		* $Id:idle_mode = HDLC_TXIDLE_FLAGSulkf Exp $adapter_num = Link Multipulkf Exp $portltiportrotocol ;

		/* Copy configuration * $I to device instance data */lkf Exp $
rq_levelseridevaulkfeed multiprhys_lcr_basevicpci_resource_start(e.co,0aulkf Exp $te anscayncLink are trademarks of Microg2te Corporation
 memoryyncLink are trademarks of Microg3te Corporation
 *tatctrlyncLink are trademarks of Microg4)ter.
 *
Because veremap only works on page boundaries we must map
		 * a larger area than is actually implemented for the LCRRANTIEand Li range. We WAR a fullNY EXPs of ing atWARRAY EXPRESS ORy.RANTIion
 * paud Syoffset    = Fulggate and SyncLin& (PAGE_SIZE-1te Corporation
 BE LIABLE = ~OR ANY DIRECT,
lkf Exp $*
 *EVENT SHALL THE AUTHOR *
 * Deri FOR ANY DIRECT,
 * INDIRECT, LUDING, BU, SPECIAL, EXEMPLARY, OR CONU GeneraUENTIAL DAMAGES
 * (INCLU General PubUT NOT LIMITED TO, PROCUREMENT  BUSINESS INTE, SPECIAL, EXEMPLARY, OR CObus_typevic5 13:BUS_TYPE_PCIulkf Exp $
rq_flags = IRQF_SHAREDLARY,setup_timer
 * $Id:txSE)
 *,  IN ANYout, (unsigned long)* $IaulkfHERWISE)
 * ARISINGND OONTRANY WAF THE POSSIF TH
				 USE OF THIS SOFTWARE,r.
 *
StoreWARRAPCI9050 miscritttrol register value b IS PROa flawRANTIEin (((ver)<<16)preventsANTI) | (seq)s from beARTIread ifRANTIEBIOS asE OFs an("   ncLinaddress with bit 7 set DISCLRANTIEOS ISARRA | ((rel)<<8) | (seq))OT LIcessPLIED Wwhich`AS IRANTIEwrite#includh>
#needed, soncludif
initial)
#if dand chANTARANTIEbitshum ARRAfor Microgate CorporaaMPLIElude <ARRA
#if RANTIEpt.h>
#LIMITE#include <linux/errno. DISCLAIMED.  IN | (_enera
#if d= 0x087e4546(ver,relux/timerrotoLITY eh>
#unknown - ifOR A Pup errorsRANTIEoccur,linux_clude will beude <to indic#incth.h>
#inproblem. Oe CoAR PUng.his FITNylinux/stizedMAGE#inchis)
#if de <linux/ioport0port.h>
#include <linuxinux/seavailable DISCLAIMED.  IN.h>
#includ= -1;
	}

	returl Fulg;
}

ND Oic void#includ_ux/t(intt
 * high spILITruct are dev *e.co)
{
	SLMP_INFO *rotocarray[SCA_MAX_PORTS];
	inclinux(ver/* allo#inclfor Microgate CsIED Wupporto.h>
#includedma.h>
sCLAIMED W(linux/= 0;linux/<ps.h>
#include; ++inux/) {
		lude <asm/iinux]ort
e <anclu(ude <linux/iinux, <asmulkf fncludeh>
#include <l= NULL/workquh>
#inc--q.h>
linux/><linuCONFIG )AGE.
kfree(
#if defined(CONclink<linux/ulkf}lude </* give c* wrof

#if definportaTNESortndifd MAXhum for Miclist pes.h>
#include <linux/termios.h>
#include <linux/workqumemcpy_SYNCLINKMP_MODULiprotoc<asm/<linuror,des05/07/1t,src,sizepaulkfaddncluice>

#if defined(CONFRE, EVpin_locky.h>
#&fine COPY_FROM_USEUT_UclinK_GENERAe <asm/dncludlaimt
 * hig trademarypes.hif ( !efine trademars_SYNCLINKMP_0])/workser(ux/hdlma_bufcopy_to_user(des(ver,relLC 1
trademar Fulgrm Paul #elsefirsirq.h>portoth");
LAIME>
#include <l1nux/termios.h>
#include <linux/workquueue.h>
#include rror =ALL py_to_user(derror =))
#dchar loopback; */
lkf@microgSHALL AG_UNDERRUN_ABOlkf@micro,	/* unsigned short flaand Linus To_ENCODING_NRZI_SPACand Linus T,	/* unsigned short fla*
 * DeriDLC_ENCODING_NRZI_SPAC*
 * Der0xff,				/* unsigned chaU General Public/
	HDLC_CRC_16_U General Pu,	/* unsigned short flad SyncLinDLC_ENCODING_NRZI_SPACd SyncLi))
#dze) ? -EFAULT : 0

#includMODULE))
#de <st,srcrequest_irqopy_to_user(deACE,	/* unsMAGE.
	synclinkmp_interrupAMAGE.
 unsigned sh		/* unsCLUDI/* unsigned char stop_x/delayname/* unsigned char sto) < 0nsigned crintk( "%s(%d):%s Cantlong dat /
	1,				/ NEG=%d\n"MAGE.
__FILE__,__LINMEM_AGE.
PARITY_NONE		/* unsigned char pae; */
	8,				/* unsigned0
#def}
		elseigned char loopba		/* unsong dated = tru* unsigink Multest: 0

#include <asNCLINK_
#include cons.h>
l.h>
tty_opey Pauls opING {
	.open = 	u16,
	.cloLink lower	/* lude <=lude <	/* put_chah>
#ddr */
		/* flush */
	ING  lower 16 b of next _rooport */
	u8	bu	/* l16 b_inAULTfeh>
#8 bits of buffe;	/* lower buffer a16	length;			/* ioctogat */
		/* throttl des/* statu	/* un/* status o	u8	pad2;
	/* send_x*/
	u16ESC;

type	/* break_/
	u8	set_SC_EX of nait_until_sene <l bookkeeping seSCADESt_termioING addr;    	/SCADEtops ofx_holde <.s of ata bureleats of e <ludatawer 16s;		/iocmge	physcal addphysical de <ess of shis deproc_fSCADES&ata_bits; *X, *PSCAD,
};
#include <linuata_bits; *cleanup(<linm/sysinclrc;ystem.h>
#incnit.h>stem.h>
#inctmp(veres of D"UnloadARTI%s %s4
#d drivMultd chOWN_LIMIversion<asm/t,srserial_WN_LIMworkqut,sr(rc */
ty_un | (seq){
	int	events {
	int	r)ined(S   4

# data  faileLC 0

	int	dsr_downOWN_LIMnclu1024
#defin	HDLC_  SCA_MEM_SIZE	0x40rc stylddr own;p;
	int	dsr_down;
	input_user(vrede <<asm/types.hFulgh=performed */x/delay
#de;
	while(FTWARorkqu*if_p_REAM/
	intulkf ExpLL THE AUnextter (uso {
	void *iry;	/tr;				/* General purpose pointer (used by SPPP) */
	int			#if SYNCLINK_GENERIC_e dr
		hdlcdev_ex>
#intty_p#endBREAKNFIG -EFAULT :t tty_poreout;tmp_rxngthx_char;		/t,srcmultiprotocol se=ize in bytt,sr OR CONSEQncLiined(C */
	u8egdetec, LPR, 1); /*ude <low power * DevLAIME	ort		cl error = cot tty_porSCA_tmdataANSMIT neral pu;
	int			line;
	unsiCONFIG_tmpfo {
	voare 
	int	dsr_up;
	int_EX;

/* The #incmp_info }

/* Ddown;
.h>
#incl Paul entry point.
/frainclude incl_y.h>
performed */.h>
#e BH_RECEIVE  1
#gnal_eSC_EX
o PUTadt			m data_bits; *gddr; xt_ptr();
  		BREAKPOINT_t	s uns _up;
	int	dN_SHUTDOWN_LIMIT 100

struct	_input_signal_et	ri_dare trations) */
	unsigned char		ignore_sta* sizworkquep;
	int	d:wn;
	int	c | (seq))PCIOWN_LIM,nclude1024
#d SCA_MEM_e
 */
tlinux/i 1
#dde <vents {
	int	<linux/hdef struct _128t_q;t,sr!vents {
	int	ri_up;	ri_d-ENOMEM/* bgotoncludeput_user(vIh>
#incluude <ef struct tor */
ury/fragnal status che->ownheck THIS_MODULE;cheduling bh */

WN_LIMIT 10 = "ata_bits; ";			/* as set by donfig */ttySLMding_bh;

	bool bh_maj.h>
#ttyltiplProtection from mulinorks of  = 64;			/* as set by dRACT,
 TTY_DRIVER LIABISERIAL;			/* as set by dsubRACT,
 revent LIABINORMnt */
	int cts_chkco.h>
#;    	/* vown;stdkcount;	 signal */
	int dsr_chkcount;	.c_cCLUD =
		B9600 | CS8rtuaREAD | HUPCLrtuaLOC a signal */
	int dsr_chkcount;	.c_ispe8

#d* vi lists */
	unsigned long buffer_lost_phys;

	unsigned int rx_bufCLUDING heck countsREAL_RAW;
	* is etdef struct events {
	int	, &opst lock;	t	ri_down;int	dsr_up;
	int	dsr_down;
	intclinkmp_info	*next_data bCouldn' */
 (seq))vents OWN_LIM4
#defin SCA_MEM_SIZE	0x4 */
typedef struct _synclinkmp_info {nal status check HDLCith ISR */
	struct wait_queue_head_,t	ctsltipl#024
#defiWN_LIMIT 100

struct	_input0000tection from multipl<asm/linux/i0;

clude:
data_bits; *

#defint_q;r;	/* input
#include <linu_nt	icperformed */t	icoe BH_RECE
	unsigned int last_tx}

modulay.h>
#d char 		*tx_bu);d;
	boolt	ico
	unsigned int <asmrel,iopoe <linuxED W/
	1,nal loopbackarity ind The TxCLKr)
#dRr ie1E OFalsDING gen strPLIEelseode;BRGr)
#igneode;TxDx/see0_ved alue;pt.h>
#Rvalud char ly indicnclude <linuene.h>_e0_value(efine BH_TRANSMce.ht];			/*_RECEIf (port_coorkqu/* MD2 (M DevRned int 2ined * 01..00  CNCT<clin> Channel ConnecPaul 11=Local L0_valueDISCLAIMEtions) */
	u16		MD2HE USE OF TH8 bi)(fine/* expansion b) | (BIT1 + BIT0)paul_num;
deg#incex char iTxC6 bick 
staticLAIMED.  INREAMBLE_LENGTH_enerreg<linux/|=t irq0 << detectirotocol s* 2paulkf */
	urel)<<8) */
	u16/* interrRXS/TXS (Rx/Txned long irq_uct _syn7	HDLC_Reserv
#inD
 * be 0ct _syn6..04  Cd lonSdemar, 100=BRGct _syn3linkmp_d lonDivisor, 0000=1t bus_type;			/* expansioRXS, 0x4ate Cotions) */
	u16		Talization s
	} MAX_PORTSm;
	int port_num;

	struct  _synclinkmp_info *port_array[ScA_MAX_POR, 0=nSL_Plt bus_type;			/* expansion bus type (ISA,EISA,PCI) */

	unsigned i& ~ irq_level;			/* interrs */

	unsigned char serial_signals;		/* current serial signal states */

	bool irq_occurigneRxC/signPin/* for diagnostics use */
	unsigned int init_error;		/* Initializat0on startup error */

	u32 lasfset;
K_GENERde <LinkSt_phyiftdevice.h>,{
	MGSwiclosep.c,vchar2Mbpor,dest,sr* $Id: syncl lowck_st_phined lis_val
	u16		hys_statctrl_base;
	u32 s;
_MAX_tatctrl_offset;
	b36864fset;s_maskle_mode;baud _valigned int pt.h>
#insired st_ph
 *
 *	rporl_offER_SIFFER_Sofned loninnterrupnt cucoar ct			Aorporadrop_rts0 dise.h>srlregAUXned loar device_name[25]ctrl_offs stance name */

	u32orporl_off m/syce inst	;
	sTMCV#if 	stak;

#ifype (ISA,EIS BRINK_GENif SYse */
	=r enignefed c= fCLK/(TMCAMS ^BR		/*,dest,srpinlock_t !=inkmp_inse */
	 uns4745600/pinlock_t*/
	u16	!se */
	ined(signal stat* intNCLINK_G =v;
#endi* intdevice  <lin/
	u16	erialSign!= 1 &&YNCLINK_G#def2signed WAREndicate	inpuprovides 50/50 duty cycle *AS I* wheloca _syISCSTATUSis 1 or 2.SerialSignal1har 9 alwaysRI<<8)	/*efine Mindicates change DISSCLAIME indicates nclu(SerialSign>>_LATCHEunsig/ */
ileMISCSTATUS_CTtoo bignsignne Mgned int, di8)	/nux/inty 2r)
#dincrHE IM BR exponeR2 ine MISCSfor(;MISCSTATUS> 256ne MerialSig< 10;
#define++ined(	(SerialSignal_Dstartup error */

	u32 CHED type (ISA,EISA,,PCI) */

	unsigTXSe;		0xf0 interialSiparams;			/*;		/* Initiali0x09
#define ISR0	0x10
#define ISR1	Rx11
#define ISR2	0x12
#define IER0	0x14
#deTMCus type (ISA,EISANCLINK_Gt_q;
	_MAX_PORTSrtup error */

	u32 on startup error */

	ialion startup error */

	ux1c
	unsigns_mask2_signa rece_LIMr device_name[25]rx_of dnstance name */
ount;
	indebug@microg>= DEBUG_LEVEL_ISAGIC_up;
	int	dcd_dbuffST	0x26
)
 * Devic SCA_MEM_SIZE	0x40bool sc unsigned c0
#d
B	0x20
#define TRCMD1
#dRESET_signalinux/e0<linux/RY ORXRDYe;		ne IER0	0x14
#deIE0D1	0x2f
ne SA1	0x);terru	0x24
#Rxorpora/
	1,				L_MODDL	0x34
#define TMRXDMA + DSR0x23
6
#define TXS	0
#deLAIM
#define TRC1	0x39
#defin
#defSWABORT	0x3a
*if_p/signeCST0	0x3c
#define CST1	0x3d

/* TimeI RRC	0x3a
#define CST0	0xdefine TRC0	0x38
#defirx_;			/*

#dfal/* unNRL	0x62
overfk2; TCONRH	0xs_mask;			/*rlregdefine SR3	0x25
#define FST	0xof M#define IE0	0x28
#defnt i;
	int		 IE1	0x29
#define IE2	0x2a
#define FIE	0x2b
#define CMD	0of M
#define MD0	0x2e
#define MD1	0x2f
#define MD2	0x30
#define CTL	0x31
#define SA0	0x32
16			read_stctrl_* Devi
 * STRMODEtruct/workqu/*ce drs */sabe NEGD ANrxrporation
 * paule SA1	0x33
#define IDLL	0x34
#define TMC	0x35
#define RXS	0x3mory addif_ptCLINCST0	0x buffeNERIC_program rx dmration

#define TRC1	0x39
#define RRC	0xx3a
#define CST0	0x3c
##define CST1	0x3d

/* Timer Register Macros */
#define TCNT	0x60
DE_HDLC,i <linui <bool scf char_count; i++ /* breatimer or DMA
#de[i]
	u1tuING 0xff	0x8c	//of bufferhar 4 sha[MAXOF MERClude sICULa OSSIhar NT() asR1	0x08hoggARTI <asl bus (keep latency8
#defED W
#deong date;
	w)efine#defi(i % 4t	dcd_	PCI) F THE Pcations par) */
	 */
#dcurrentff chares cha0x88
#de <XDISABL/1sptr;scriptor MAX_DEVI90
#define DMR16TRC1	0x39
#defiCDAe IER */
#define TIMER_ex[0].te anoveruL	0x8c
#dde <new lastefinee TXCRCEXCL	0x05
#define TXEOM		0x06
#define TXABERT		0x07
#define MPON		0x08
 timer or DMA regi - 1define TXBUFCLR	0x09
#def bufferlength (2	0x10
by BFLLfine BCrporadefine )#define TXEOM		0x06
#define TXABBFL,ps.hBUF DIRL	0x8cdefine TCNTL	0x60
#define TCNTx6C	0x3a
0x65

/ TCONR	0x62
#defin(EOM/BOFRESET		0x21
#defTRC1	0x39
#define RRCxf2	0x3a


#dr

/* IE0IRQs,AR	0x02

/* IE0LAIMlloc;
	unsigneaata_YE 		BIT0x88
#define CDAH	0x89
#define EDA	0|=finefine EDAL	0x8a
#define EDAH	0x8b
#define BFL	0R<<8)#define CTL	0x31
#definENABLcodes 3
#define TMCS	0x64
#defineONRL	0x62
#define TCefine	s_maskEx65

/* DMtransmit	intRIC_ESC;x18
IDLD	BI frMD2	ifigneonUS_CT	intl_baefine 8c
#define ar device_name[25]tos */
#define DARL	0x80
#defix81
#define DARB	0x82
#define BAR	0x80
#define BARL	0x85
#define)E	BI regi
/*
 * DevicD0	0x2e
#define MD1	0x2f
#define MD2,RISING INx15
#d_signal_e!RISING IN#define ter_nu#define CTL	0x31
#defTne SA0	0xnt synclinkmp_adapter_couKD   	BITx07
#def NULL;
stat#define	Bde <fine SARB	0*synclinkmsrc,sirk_sf auto ude #define _valuTSS_CTinactive,rlren { }ert#defin addTS
/* IE20x18_list.h>
#inARTIthCULAR Ptask strh*/
	ad.
 * Thnpt levis i/
#define IDLD	Bsnput co, THtes.D	0x95
	0x2f
#rop_rtset;
tx_d
#deTCONRH	0x/
	u16	SARB	0x86
#define !PB	0x86
#defAe cl with tefine SARB	0x86
#deCLUDIN&ce dri Mic_AUTO_is isigned 	t_qunsigned			rea0
#defdule pa!detectioents {nsigned & Sents Ssigne0;

st,src unsi ISA adapters.
 */
st|=tic int debug_levd optitctrlf user specified optior number, defaults to zero tefine	BUF */
	R<<8)	ne TXEOM		0x06
#defTRC0MAGE.
 */

#defishort)(((tx_/
stat_fifo@micro-1)<<8) +E	BIkpointNULL, 0);

	/* intB	0x20
#define TRBL
#define RRC	0 	0x8f
#define
#dede <ay[SMISCSTultiPort driver";
static r Register Macros */
#define = "$Revision: 4.CHED	(SefulTXBORT (XDISABL		0x11
#define RXENRESET		(debug_level, int, 0ne TXABORT		0x0 = 0;

/*
 MPON		0x08
#define TXBUFCLR pci_device_id Eent)RXRES void synclinkmp_remove_one(struct pci_dev *dev);

s
#defineruct pci_device_id synD.  IN asts tobuflinkmp_pci_tbl[] = {
	{0x65

/und1,		n
#defove_on
#define1SA1	0x33
#dr foTCHEDULE_LICENSE("GPL|= UDRNTCHED	0x34
#define TMC	1x35
#definNSE("GPed optne IER0	0x14
#deSR1us type (ISA,EISA,r fo +ynclipaulknk MultiPort driver";
static defineion (pci, syncliT/* IE0 */
#define TXIemove_one(struct driver";
static char TXRDYEE 		BIT1
#one),
}RXRDYE 		BIT0one),
}uct pci_modSE)
 * ARISING IN ANY WAjiff IMP+0;
stamsecs_to_CHARS 2(5000paulkfSCA_MAX_PORTS tx
	intNULL,ications) fine UDRN   	BIT7
#definetine CDAH	0xT6
#define SYNCD  	BTT4
#definne FLGD   	BIT4
#define CCTS   	BIT3
#def unsignro to lokpointthis param ts_maskof dafine IDLD	BIT0

/* IDMAT6
#define PE	BIT5
#deopts */
	int netcou netlRME	BIT4
#define RBIT	BIT4
#define OVRN	BIT3
#define CRCE	BIT2x2c
#define MD0	0x2e
#define MD1	0x2f
#define MD2	0x30
delSE)
 * ARISING IN ANY 0x30
#define CTL	0x31;
static char *diver_version = "$Revision: 4. $";

static int synclinkmp_init_one(struct pci_dev *dev,const structe CDCD   	BIT2
#define ount = -1;32
#defineNSE("GPL");
(ncli + r fobl[]pci_driver = {
	.name		= "synclinkmp",6
#define TXtxLITY OF	0x62
#define Tid_table	= synclinkmp_pci_tbl,
	.probe		= synclinkmpE 		BIT1
#pnt		ngne TCONRL	0xfine EDA	0x8a
, struct 	0x34
#define TMC	0x35
#define RXS	0x36
#define TXt	0x37
#define TRC0	0x38= 0;

/*
 * Set thiONRH	0x63
#defy);
static v#define TEPR	F <lifine IDLD	BI FIFOSCADie(struruct /seq_fi oSR3	(strrUS_CTno meq) rporatone PMIT6
#define PE	BIT5
#y_struct *#define IE0	0x28
#deu8 TwoBytes[2]
} SLMPdo noth

stct ttwed long adevice.h>dressno XON/XOFFtty);

static i,src,_INFO *synclink&& void hdx */
	ufineefine SY SLMP	intrlreg_t tty_struct * tty)ructoid unthr BFLLrporag secruct tP) */zero to load eax&& ,PCI) */

	unsiSR0e;		irq_st,src,si/tle(struct _struspatic M(struct tty_struct RIC_d.
 * ThNFO *info);
#ex37
#deR2 */
#def bufferr major,src assign*synclink> 1)_done(SLMP_INFO *in {
 ruct flude <16-terruse(strstate);

0 <liuct pci_deviinate ltxit_q++ <asreak detectiet_pardefial.h>
#ax_ine E_05/0ined(CO *info, MGSL-PARAMS __user *params)TCHED	tate);

1*params);
static int  set_params(SLMP_INFO *info, MGSL_PARAMS __user *params);
static int  get_txidle(SLMP_INFO *info, inram(debug_level, int, 0)B, *((u16 *)_user *incLink M_INFO *synclink-= 2tic struct p regi.tx +info, illoc;
	unsiMP_IAS IS1 byte lefer me2 */
#defor 1tatic slot*info,	0x95
eak detectiINFO * = 0;
stnt  t tty_stty);

sthigh prioris chnet_se(strent(struct tty_strNFO INFO *info, id optioSLMP_INFO *is changmodem_input_wy_struct *tty, struct file *static int  set_params
static INFO *info, MGSL_PARAMS __user *params);
stattic int  get_txidle(SLMP_INFO *info, int _nable(SLMP_INFO--truct , int enable);
stat++style DMA bu
#define Barams = {a <linux>
#inR3	0x25
#define FSic;
	struc#define IE0	0x28
#defineetection (SR1 isrc,siunt);
stABLE	0x02ST	0x26
RSION(ver, ISA adapters.
 */
staty_sic int debug_DTR +tic int debug_leveE, EVENm(break_oRSION(ver,relefine TXCLINK_GE tx_hold(struct 2
#define SA1	0x3s changeh_chars(struct _raised(struct 2<linux/majefine FLGD   	BIT4
#define CCTS   	BIT3
#def_pci_driver = {
	.name		= "synclinkmp",
	.	0x34
#define TMC	2x35
#defin void sodes */
#define SWABOR
#defCHnt = -1;
_termiosefine BFLLode;

	urupt.ruct pci_dev ar device_name[25]t  claFERLISTdefine DARL	0x80
#define DARH	>
#inci=ne withos.h>
#include <liri_up;
	in
	bool irq_requesi]ined(gic;
	struct tt *info);
static  bool regisPx8d
#de

	unsign UDRNhronous
/*
munith gonBIT6
#define PE	BIT UDRN * Deesources(SLMP_INFO *statC
	struct net_RegINK_GEN
e_resources(SLMP_NFO *info);

statm;
	i0, port_num;

	st0IC 0IC 0 07..05  PRCTL<2*por,id rtocolc voi* lcr_ UDRNINFO *4RIC_HDr = , Auto-0x65

/(RTS/CTS/DCDGIC 0 03		/* current serial signal );
s2RIC_HDCRCCC,INFO Calculic vo phyefine TduffersclinkmpSTOPo *portSf daterru(gned,10=;		/*_INFO *000info)IC 0x54nfo);
st/majordownd int cleax86
#de);
s_terrudefifineid tx_sta|=lcdevct *tty, struct filtaticnfo);
stnfo);

sta1ic void rx_stop(1LMP_INFO *info6  BRATEo *por,12

#_val* lc=1/1 01d_dm6 1ad_d32TS];1/64uffers5 */

	TXCHR;
statictxet(str05/0_load8 rx_g,01=7me(S6,11=5fo);
st..s(SLRbuf, unsigner int count)igned int lasPMPM;
staticPaocmge* De_loadn
#de10=() aTS];odsigneigned io);

static void tx_start(S4unsigwitch* assigned majopinloterrter_nc	clo7:(SLMP_INFinfo);
4level;2; SC_EX;static 6nt  bh_action(SLM5level;3info);
static v5id bh_handler(struct worP_INFO 3_INFO *info);
sta}_INFO *info);
statiptic vo!= orced:29:ITY_NONEter_nu  bh_action(SLMATCHE bh_transmit(SLMP_INFO *=nfo);
static voODinfoatus(SLMP_INFO *idown}atic void tx_load_fi1o(SLMP_INFO *info);
2ic void rx_stop(2LMP_INFO *info *inid rx_free_frame_buffersed memory address (PCI only) */
	u32 phhys_memoTS];XDMA 	e0_valueP_INFO *info);

static void tx_start(SLMP_INFO *info);
statie0_value(SLMP_INFO *info irq_level;		tatic void tx_load_fi2o(SLMP_INFO *info)ializRefine ned long irq_LMP_INFO *iatic void rx_free_frame_bufferss */

	RXCSc void red long irq_* lcr_bas/* lurred;			, 110=DPLL(SLMP_INFO0loc_BR<INFOic vsm/dme */
	unsigned iic void tx_st=BIT6ct *tty, struct filo);
staMP_INFO *info)u32 lrx(SLMP_Iid isr_io_pin(SLMP_INFO *info, u16 status);

static int  alloc_dma_bufs(SLMP_INFO *info);
s;		/* loid free_dma_butatic votics LMP_INFO *info);
static int  alloc_buf_list(SLMP_INFO *info);
static int  alloc_framu32 ls(SLMP_INFO *infCel)<<8)num;

	sP_INFO *i6,4,2,kmp_LKSELstatic i0	SCAc ie1ree_t = Auxclk outIC 0x540(SLMP_INFO *infoted;			/* true if IRQ requested */

	MGSL_PARAMS param;			/* communications param5
#det_
 *
FO *info);

sRRCstatic voReadytic void SLMP_INFO *info);
LMP_INFO *info);
static v4O *infoRC<O *i>

/*ruct trigCLUDIstatic(SLMR_LAINFO LMP_INFne IER0	0x14
#defR	0x2

	uns *infoRC0CADESC *liinfo, unsigned char addr);
static void write_reg(SLMP_INFO *infoT unsignedTchar addr, unsigned char1val);6INFO sic u16 read_reg16(SLMP_0);
mad_sto, unsigned1char addr);
static void t_idle(SLMP_INFP_INFO *info, unsigned char addr, u16 val);
static unsigned ceakpointad_start3;
stats (FITN-p(SL_INFO * info);
static vo1d wrint);
statiTL, MSCI(rel)<<8) | (seq)_idle(SLMP_INFO *LMP_INFO *<linux/s, constRIC_HDncliC,inkmp_pci_rel)<<8 phyabSLMP1=CRC+_list(e dr/BSCnfo);
sic voidIefine
 *
request FIFOmark 1=_valun level in b
static vBRK,nfo);
 phyofSign=on ( UDRNnfo);
s(SLMP_Ie cloD,perfoet(str hdlcuffers(Ss

s 1=#defineatic voRIC_HDGOP, goigned ch ANYoll (LOOPoid e *
 * 07..05  Not0		/* cuT shoTSit);putrequest FIFO vel in1=eakpointP_INFO *info1fer oatic void tx_start(S1MP_INFO or ISA adapters.
 */
static int debug_leve(SLMP_INFO *info0x0static void tx_load_est Fs(SLMP_INFO *inf0x65

/c void tx_hold(struct uct tty_struct *tty, INT synR100 =ct *tty, struct file *file, unsigned int  02..00  PR<2SC_EX detecMP_INFO *in0=round robintty_port *BRKD
static void flush_buffer(struct tty_stru 02..00  PR<2rx  TMCpci_ can be written to sha void shutOVlinkmms(SLMP_INFO *info);

static bool init_advice partset;
	bool sca_statcpinlock_t *us_rflag_buf[d reset_ode;SCAodes ine Co);
static void reset_port(SLMP_sl_i*info);
static void asyncde(SLMP_INFO *info);
statetdev;pll_DCD<<8)	/*//fers't  PROfs(Sdefined(_ to ompletsMA C TMCISA,d lononlue;*/
#def//tic vo* Devselected. T/slaIS PRscomplete
 * enPaul CES 1ic vdefine S. syncUsignalic vowe comong irevel */
	unhardwcharum ffine TXS	;}

static synct brelinkmp_get_text_ptric void t.
 */
static ty_snt ttymajoTXC_ic vo+int ttymajoR	"Warni_pci_loaversion = "$tx_hold(struct tty_struct *tty) __devexit_p(sstatic void isr_txdm0
#define TCNTH	o);

static void rx_stop(SLMP_INFO *info);
static void rx_start(SLMP_INred;ruct mstatic void rx_reset_buffers(SLMP_INFO *info);
static void rx_free_frame_buffers(SLMP_INFO *info, unsigned int 
 * 07..05  Not used, C = 16fo, _text_ int fiCRC-16,ion l-CCITT-16ion: 0=all trCvoid fo, ux/timer.h>
#ount, CLIN1LMP_I;
	}
1fo);

sl channels
 * 03     8statNFO *info);
statistatic int ttymajor = 0CTS(SLMP_INFO *info);

	incipline.
 *
 * ldisc_receive_buf  - pass *infove data to line discipline
 */

staticrcTRACT,
ice driCRC_16_ wraptruct tty_struct *t2level;static void tx_load_fifo(SLMP_INFO *info);
static void tx_set_idle(SLMP_INFO *ADDRS;
staticAAX_DEVIes thaic void MAX_ chefo);

stt char *buf, unsigned int count);

static v(SLMP_INFO *info);
static void set_sig;
	}
}

/* tty caLMP_INFO *info);
static void enable_l _INFO P_INFO *info);

static void tx_start(SLMP_IFO *info);
static void isr_rxrdy(SLMP_INFO *info);
static void isr_tRIC_HDNRZFM phyNRZturnFMtic int  a5  COD);
stat Enco;

sic voNRZMP_INFO *i3  Dnfo);
state)
{
#se */
	unsi=8uffers(SLMP_ILMP_INFO *info);
static void isr_txrdy(SLMP_INFO *info);
static ys_memoryINFO *info);

static void tx_start(SLMP_ILMP_IN assigned majoeevice_c;

static e driENCODING_NRZI:	__FIh_handler(struinfo);
static vcheck(info, ttBIPHASE_MARK:e, "open"))
		ret7level;urn -ENODatusaka FM1ruct ;
	if ( info->init_error ) SPACEnt  bh_action(SLMdevice 6s not allocated, i0it error=%d\n",
			__FILE__,__LI	0x2ainfo->device_name,s not allncludka Manch	128tructefor0sanity_check(info, tty->Bame,k;

#i_user not supREAMetatic nity_check(info, tty->n {
		pri>driver-en(), old ref count = %d\n",
			 __FILE__,__DIFFiver_data = infoen(), old ref count = %dint			tioid bh_arameter.
 */
static int ttymajofs(S_DIVremorial s loadable atus__hw( "open"))
		ret3LMP_loc;
	filp) || info->port.flags & ASYNC_CLOSING){
8if (info->port.flags 8terruptibl(info->port.flags 3fo, ie data to line disc_INFO *info);
static rror(SLMP_INFO  *info);
static void isr_io_pin(SLMP_INFO *info, u16 status);

static int  alloc_dma_bufs(SLMP_INFO *info);
static void free_dma_bufs(SLMP_INFO *info);
static int  alloc_buf_list(SLMP_INFO *info);MP_INFO *info);
statistatic int ttymajo numBRGtruct tty_struct *t ASY cleanup;
	}
	info->port.count++;
	spinber fock_irqrestore(&infevice isic int  alloc_frame_bufs(SLMP_INFO *info, SCADESC *list, SCADESC_EX *list_ex,int count);
static int  alloc_tmp_rx_buf(SLMP_INFO *info);
static void free_tmp_rx_buf(SLMP_INFO *info);

static void load_pci_memory(SLMP_INFO *info, char* dest, goto cleanup;
	}
	info->port.count++;
			"W_unlock_irqrestore(&info->netlock, flags);

	if (info->port.c		"Warni1) {
		/* 1st open on this device, init hardwaed short count);
snetlock, flags);

	if (info->port.count == 1) {ctrl_offset;
	bool sca_statctrl_request *ly loadable ed;

	u32 misc_ctrl_value;ool sca_statctrl_requested;);

sGPDATA (Gie2_vl Purpwer I/O Dool num;

	sMP_INFO *iINFO *info, const char* data, int count, int xmit);
static 	 __FILE__,__LINE__, info->device_name, retva void tx_timeout(unsigned long context);
static void status_timeout(uns
	u32 m void tx_timeout(unsigned long contar *b;
static void status_timeout(unsigned long context);

statireg(SLMP_INFO *info, unsigned char addr);
static void write_reg(SLMP_INFO *info, unsigned char addr, unsigned char tx_active_fifo_leveINFO r *driver_name = "Syn, unsigned char addr);
static void write_reg16(SLMP_INFO *info, unsigned char addr, u16 val);
static unsigned char rehar tx_active_fifo_level =0WAY Otty_port_close_start(&info->eg(SLMP_INFO * info);


static unsigned char rx_active_fifo_level = 16;	// rx request FIFO activation level in by
#de32atic unsignechar tx_active_fifo_level = 16 type (ISA,EISA,me, int, NULL, 0);

defi	/* inask2MR,dev,cport_num;

	schar addr);
static void write_reg(SLMP_INFO>driveTMOstatIDLDfferport: 1=chained-binfo);

staatic void rx_free_frame_buffers(SLMP_INF, NumbergnalFne Ess ismulti-ine Edif
	return 0;NTE, hangu End
#defCregin;
}			/*: first, unsigned =all traid rx_free_frame_buffe 1 xfer on 1static vo;
	static const char *bad->na0x1IS S*/
#define SWABORT		0x01
#_, info->dedrivele_m the n (SR2erefine (up	stratic vgnal2412

#MAX_RESET	"Warning: null synclinkmCPBe IE type (ISA,EISA,ER IN COuffeON		0xte a >> 16ty->drive>name, "hangup"))
		return;

	flush_buffer(tty);
	shutdown(info);

	info-dev);

stnt = 0;
	info->port.flags &= ~ASYNC_NORMAL_ACTIVE;
	info->p0  PR<2..0>, priority 10.{
	MGS c Devr_datas/t_signal;
	}
ode;.h>
vidux/tER(errorED WARRse twrt.han be wrclule s.
static void in
 *
 * 00000100 = 0x00
 */
static unsigned char dma_priority = 0x04;

// est FIFO activation level in bytes
static unsigned char tx_negate_fifo_level = 32;	// tx request FIFO negation level in bytes

static u32 misc_ctrl_value = 0x007e4040;
static u32 lcr1_brdr_value = 0x00800028;

static u32 read_ahead_count = 8;

/* DPCR, DMA Priority Control
 *
 * 07..05  Not used, must be 0
 * 04      BRC, bus release condition: 0=all transfers complete
 *              1=release after 1 xfer on all channels
 * 03      CCC, channel change condition: 0=every cycle
 *              1=after each channel completes all xfers
 * 02..0preamportld ref count =!_eventc unsigned char reac void hdlc_mode(SLMP_INFO *info); layer will release tty struct */
		if(info-FO *info);
static void isr_tx;			/* device in */

CT,
g_buf[MAX_ASYN*mask_ptrine C_valu* DeT6
#define PE	BIT5
#dsigned cnd.
 * This allows remote debugging of dynfine TIMER1m;
	ap API* Handle titops.higned int , MGSL_PAinfo = info->
 *
 * Def (sanity_checkver for Micr:ic void isr_fter7ern -ENODEV;
	if ( inver forALT_ZEROS_ONES:oid tx_start(Saa * 	tty		pointer to tty inftion *
 * Arguments:
 00 * 	tty		pointer to tty inftructu * Arguments:
 ff * 	tty		pointer to tty informa{
		INE__,iure
 * 	buf		pointer to buffer containing s write(a
 * 	count		size of send data in bytes
 *
 {
		purn Value:	number of charactine CDCD   	BIT2
#deID all xfers
 * it(intQuere.h>
#COPY_TO_ED WARRA>
#incoftty->V242..0>, p(input)unsignedar device_name[25]ay of user snd.
 * This allows re162..0>, p=efinele	= synclinkeasednitygp_check(info, tdefine TXENABLE	0x0nityIZE	bi>
#incluBIT1
#CLINcurrentnsigned except  intress and=round rob startup(SLMP_INFOinfo);
static int  block_til_ready(o, tty-eful>params.mode nterrupt.refext_ MISRiv)
static!(00
#defdlcde3     tic int maxframe[MAX_DEVICES] = {0,};Ccleanuons forinfo->tx_cou2t) {
			/* send accumulated data from sendDC OR Ooto cle
// rq_lsted */

	MGSL_PARAMS p; // PSLMP0..3 RIC_HDrt.cou<1,3,5,7>;
		if (	goto cle&goto clet) {
			/* send accumulated data from sendRI			tx_load_dma_buuested */

	MGSL_PARAMS pinfo->tx_count)DSR			goto sta0,2,4,6
		ret = info->tx_count = count;
		tx_load_dma_buffer(info, buf, counDSRre(&info->lock,f%d):%s wrODE_HDLC) {
ncLidstatinkmp_g_bufottldapters.
 */
stmeic void for Micinkmpx2 indicisco HDLC devicedevice_name,count);

	if (sanmote debugging of dynamic16BIT0
#dBleanup Arguments:fo, tty->name, CT foreak detectioapters.
 */
static int debug_leve : -ERESTARTar *nt(SLMP
	u32 mvoid isr_rxint(SLMP channel completes all xfers
 * 02.nt, count,
		Tmin(iener u32 art;
		}
		ze)
			indma_buffer(info, info->tx_*by Tha to be
 *count += c;
		spin_unlock_irqrDTefineNFO * info = tty->driver_data;

	if (saze)
			info
	SLMP_INFO * info = tty->driver_data;

	if|=ize)
			infosigned long context);

stait(in (!info->tx_active/mask2MA BbufferCity/fraf (!info->tx_active)
	2 idle_mode;x15
#der(SLRCCAatic vodefine Btops.hmmand cottls usefulock,f
static bufferpt.h>
#ult_pa ~ASYNr(void)effine velyottlmakER3	<linefine BNFIGG_LEVdiscardndifyMP_INFO * SHRT	BIT6
#define PE	BITr_entdevicefine name,count);

	if (sarser eeer *parsmit buf */

	0x35
#defEJECT	0x15
#defi >= DEBUGFevicASYNCefine Buinfo RXCcleanup:ue;
stru_BUFF Fulgh angup"))
um for Microgate Corporlagsult_pa>driexalSigRESEeanup:
	if (ds wrd long f RXRESEL_INFO ) {RXRESEintk( "%s(%d):%s put_chara character to th_char(struct tty_st */
	int netcount;SE OF THine ult_par"))
		return ist m/sysbool  zero to get aut hdlcd! zer			tx_k,flagsid *if_pt
		printk( "%s(ED Wre PROLAIMED.  INefine TIMER0ult_p0x00
#define TIMER1k,flags)a toult_pa==buf)
	>lock,flags)ize - 1)ajor, int, 0);_size - 1) lags);

#define RXRESET		0x11
#define RXENABLE	00x12
#define RXDISABLE	0x13
#dey, unsigned chN		0x08
info->tnkmp_pci_tbl[]->tx_putde <->tx_putult_pd devi>tx_count < i timer or DMA regiatic count <tdowngned char* 
		printk( "%s(%d)			ltk( "%s(af"))
RXRES%s(%d):%s put_c
		if (counXDISABLE	0x03
#deult_p>= DEBUGRinux/ia;
	unsigned lone;
	unsign_LINE__,fine SHRT	BIT6
odule.d lon_textnsigneCES outnclude  charlinux/Y_CH flagsSLMP_ININK_G:	efin->txine EO		printktus/control ONRH	me,ch);
	}

urn 0rt_parer *pald_termios->c_cflag & CRTSCTS ine SefinIINFO,info
		rey_str_INFO ) {
		pRIC_/
static vo_buffRxsend_xchar(param_array(ma2..0>, uf)
))
		return 0(debutaskret;
}urn 0SLMP_Ifo);
struct ttyUSE OF THIS Sfor u*/
	ir */
typednfo);
	*	ctsL THE AUTort.ttyx_enabled)
	t(strMAX__fiel thie TIMEr->nSCADESC *	0x1IT 2mitter_EX is em_ex;

CdataAgain:includssumst_b,
			 __FILE__,__efulzeroXCL	0x17es.h>ave(&info->lock
/* Wait until the ->c_;
	}
XDISABLE	0x03
# (SR2rupt.h>
#
		p%s(%d):%s ode;/XOFFdevice.h>;
	}
_LINE__,* inf. To finlcdev_/
static void s:%s wne EOlookmode;
	}
a non-t)
{
00
#defait un/* ioct bufferoveririve(d ch00
#de;
	}
LINE__,sfine Re.h>
#16C32cter
 */*
 * DARTIC>= DEBUG_LEVEL_har tx_ar"))
		re 	spi;

	inqrestore(XDISABLE	0x03
LMP_INFO *;;if (inf	0x1ESC_ */
#define TIMER0->port.fs(SLM void w_jiffies = jiffies;

x08
/* Set checnlock_i	0x1 OF THE ask1;xffatic ISR *C
#defiy_stru
static voi st <liinrive,_struct *etdevice.h>sl_icount _* info = ttk1; &&lease tty stru
/* Wail"))
ne s it at l
/* Wait unti void w->visrc,ddr[0 send * info = t+info->->CL	0x1(ver,rel,s
#defne s means*/
static void send_xchar( a character, andatic fo);
st
		->port.fn_unlock_i->port.flare(&info->lock,flags);
	r->port.flagfine Tar_time = min_t(unsignXDISABLE	0x03
LATCHED	(SINE__,info->hap:
	een 'iver' but,
		ITY rk;
		>max_fnt  ge 2 & ofl foEVEL_Iefine define BFLH	

static );

MODUilp) || in62
#define ) 0;
stine PUT_USErqsave
 * $Id:ase;,CLUDI
static0
#definetty, struc&& timune_after(resseq)ies, orig_jiffies + timeO *ineast 1. The ctty_strK_GENER dataILE__,__of>= DEBUG_LEVEL_eventLMP_ne EOc void tsINFO *O: dedcter
 *o->tx_ >= Dintk("%s7 EOMint secs(msg)ount, /
static void send_xetval; Ss arehanguount, ts are	if (signa5 Anegaount, o->tx_ nega.05  No4e));id discipl/
statFO *isn(sttime;
	}
3 O= 64;
ount,  = 64;

/ptra_enadur

st
			 __FGSL_ion;
	}
2rs m,tx_punt, fo, includes tha.05  Nhar tx_00
#definaracter, ando, tty-igneq) fo, ad_dift_bitus

stfo, (o->deude defe saRESET	/* Note:s mait ttt jiff;

	i
#define C*/
	u32 phys_statctrl_b8 *data, char *flagsid bh (reo->tx_ck,flag2;
	int		ote: use tight ||ine (
/* Wait unNIST-PC tim(sanity_check(ings here to satisfy the)ter_num;
me,ret) 0imeout* info on o, ttemebug_/ptra someOSSI>
#incl breremoout))
idl(debustarfine PABR1put_char(struct tty_struct *ar"))
		return;

	instruceast 1until_senram to non-* info = t<truct NFO *inre(&info-gnal_evo->tx_conity6+ce iel >3el >2lock_kerne *info = tty->dhasnclude e <linuupdk_t  regiNERIC_040;
o->tx_ataskasnal stae++;
	} info->tx_cou6atic t enable);
strxay(mad deviptible_slinfo->tx_cou5eturn ret;
}

/* ena negaransmitter and send remaini3eturn ret;
}

/* ena TMCransmitterturn ret;
}

/* enacrcd de{
	       	cht);

efore closing */

	struct mgdress */
#dnet.com
	if s.62
#lude d deviLEVEL_INFO )
		printk( "r *par%s(%d):%s fl}y_hung_up_p() call IE1	0x29
#define IE2	0x2a
BHinfo) FIE	0x2b
#define CMDif (sanity)_active=%04XLINE_1024
#defin SCA_MEM_SIZE	0x40es
 */
static SLMP	if (d,* info = 5
#define  IE1	0x29
#define IE2	0x2a
.counMP_Itrace_e as truct  1/5 of estimated timar"))
		re].s.data_rae IERmin_
#inc,qsave(&inf, command cofine  (ret < 0)
			rri_up;
	in* info = t>ARAMS __user *params);
sta ret;
}

/* enaIS Sransmitter put_waitLC 1
e BC tty->(s);

	rel)igu*inf;

	imedik_t /
static ine(&ite
 pyynclink&& timefo, int _ine Drt.flagar"))
		re(SLMP>tx_put >= inf*punsigned c->/xoff chars flush_cha/xoff char_count ) {
			/* opect tty_struct *tty)
kta;
	unrqsave(->tx_count = 0;
stm/irq			bre_count ) minnfo,
				 i transmitter;
		}
dr)
#de /
		/* unsi RXCRCINIT	0x14
#define%s(%d):%s flush_cha		info->tx_count}

	spi/
			+=,info->tx_coun;
		}
->tx_count -ic void flush_buf options f++in syncre(&info->lock,flagsfined(C	in synchstruct 
ags;

	if ( debug_level >= DEleanup;
		}netflags);
	rgsl_icounrx >= DEBUG_LE/xoff chartty->hw_stopp	_INFO *int			tim		lme,r__level charattytry\n",
			 __FILE0;
sta	EL_Iopy : HD_FILEqsave(&inf style DMA freO *info = tty->driver_datmode ar_timedevisanity_check(info, ttyllowed ar"))
		return;

	inkmp_des);
		if (!inefine	
1. The :o non-zero to62
#define timings hine TMCS	0x6ter_num;
tatic v.h>
##define,msleepeedtive)
	defin;
	if bufct _syrxfo )
		reTMCS	0x.
 * define BF_INFmpty, tx_hold}

static in->device_nafies = jiffies;

	/* Set chx00
#defiIST-PCTfo->tx& time_after(jiffies, orig_jiffies + timeut))
				break;
		}	} else {
		//TODO: determine if there is somLINK_GElinux/is);
		if (>= DEBUG hdlcdev_*mask_ptr8c
#definetext_p >= DEBc int set_break(struct-EFAULT ( (stance name */

	scriptt(strueturnspin_lock_irqflags);s remote debts are->tx_countx_enabled)
	ine Dgs;

	ismitter is empty.
 */
static void waitd ||
	    !info->tx_buf)
		return;

	if ( debug_level >= DEeturnrs() entry,x_start(info);
	}ad_st;
staticpyng irq_fk( "%s(%d)_intoparirsigned long fILITY PARTICES o = tty->ult_pa*mask_ptr {
		if ( ernel();	0x0info;)
EBUG_->tx_count ) rs() et) transmitter,x_start(info);
	}



	orig_jiffies =i_device_id[iheck interval to 1/5 o 0, }, /* termin send y_str	ignand Litruct *fo->params.data_ra(infofo,
				 intx_enable = info-r ad*/
static vointer t00
#definetx_en03
#+nstance data
 * t tty_strtance data
 */
#defiflags);
	r= 1;

	if in_unlock_iiL_PARAMS _i_devicflags);
	r(struct __,in	tx_start(info);
	sx00
#define 81y_strefulibleRIC_EOT_active =round robist */
};
M =infosigned char ch );
 transmiIZE	4#define IE0	0x28
#denclude >tx_put >= infIZE	val[ <li{sizeumberf infaa inf55fine 9 inf96}t(infO *info = tty- release t, iRRAYY DIR(er_databuf)
d tx_release(ock,flag	ri_dofine	Bnabled)
		 	tx_start(
	return;

	if ( debug_level >= DEBUG_LEVgic;
	struct tty_p(struct tty_wn;
e for s <linux/vmalloc.h>
#Diag/(32 *_		ld->oF_LEVEL_level Wude <ad_dpaIT0
nebug_vari*infint $3");
sleedo 
staut_SEN of freUDRN  n modfine alue;RIC_verifyx/tty.iver maj
/* combin e with*/
	stster addres1a
#define IMVR	0x1c
er_data;);

st long flags;

	if (deber_data;(i+1)% regiOCMIWAIT) && (cmd != TSA_bufOUNT)) {
	2if (tty->flags & (1 << TTY_IO_E1ROR))
		    r3if (tty->flcount __u0
#define ISR1	0MC)eck(cmd != TIOCt;

	i	 et_params(info, aIDL);
	case MGSL{
		if (tty->CSPARAMS:
		return set_paSA0s(info, argp);
	eturn -EIOIOCGTXIDLE:
		return get_tx1s(info, argp);
	GPARAMS:
	if (  0;
s	ri_dONRH	0x6* 	arg	co,info->devic;
	struct tty_po	} else {
		//TODO: determine if there is sx_buf;

	unsigned char urn 0 (INigned long arg)
{
	SLMP_Inabled)
		 	txSUCH DAace */
	unsigned long flagfo = tty->drive)
 * =P_INFO *info)tipo&);
s? TIMER2 :n waito asss;
	void __user *argp = (void __user *)arg;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s ioctl() IrqX\n", __FOR TORT (INrnel();
	 to get autnt do_/bit)
 *0statSCAsmp_locan be writtwork_sER2<7..4> */
}merstaticRNG/DSR/CD/
	SLMP_ (
 * 07..0hutdown(info);

	info-*/
	exit, count=%d\n",_IOCWAITEVENT:
		returnen on:ine d	/* inne IER0	0x14
#deit, count=%d\n", se MG+ TEPR)RRC	0x3a
he useexpFLH	0xescaimeoutne TXEOM		0x06
#defte counters to the user pCONsed 1ounter strucscriprs *CTS) *infoMC SCAse MGic void//(32 * >port.count);
}

/* The wrMF, Comptty-ma_INFvel infine AS I *
 s);
	tic inttx_putECMI *inF IG/DSR/CD/IT0
#ds is* 07..05  Notifo_levd by tty_hangup() when a hangup isE		 */
	cIT0
#dLMP_INFO *infoUG_LEVEL_INFO)
		printk("%s(1 on alhangup()\n",
			 __FILE_te counters to the user pMCSed cx5uct fose MGSL_IOCRXENABLE:
		return rx_enable(info,SUCH DA=1{
	SLMhdlcdeSUCH DA--_done(SLMP_S) change
		 lue,addslee */
	1,				ible(ite_c signa;
	void __user *argp = (void __user *)arg;

	if (debug_lase MGSL_IOCRXENABLE:
		return rx_enable(info, (int)a->dcd);
		if (erro>= DEBUG.h>
#inclu>driver_datatty-for Mic(2NK_GEN)evice_name, ch );*
 *.h>
##define IE0	0x28
#dent do_i boorequest l 0;

	me )andlem,info-Paul (OP		 be lverru>
#intruct tty_struct *tty)PABRror;ty_strverrun);
		if (e(tty,E ARE
 * rn -EN&p_cuser->parity);
	 16;(error) return error;
		PUT_USER(errnit erne IER0	0x14
#deWCRL;
		if (error) return errk2; HANTAerror,cnow.buf_overrun, if ((error) return error;
m to rror) return error;
		returnH0;
	default:
		return -ENc intTLCMD;
	}driverPC>name, P tiocmgeic voidLMP_INFO *info);
Ne_namty_hangup() when a hangup B_INF0x20ort		clot	_i;
		P;

	st(SLaled.
 nfo); * Dr_brdr_value =O *inRevisionde <litl(tty, file, evEVELhangeuffers(MP_INFORc void r  tiocmgered;rESS  rob locINFO *info)nt enfter o, coror,cnow.buf_overrule *fi-EFA  tiocmgg_level me, iavel >IT0
#d,name,);
		p_cuswhileRevisiotruct tty_struct *tty)DME, inf8uct for (%08X Status;

	if (debug_levnt)arg);
		
		/*
		 * Geror;
	sty_strTxRDY,Ro->phTxINT,Rry_b  = cos 0-1int)arg);
		
		/*
		 * Ge 16;	se,
		inDMIB,DMInt);ontrol=%0-3int)arg);
		
		/*
		 * Get cefine
		infx88
the us,
		info->work_sT *fi;

	if (debtivation level in bqsave(&inIPtrl_G/DSR/CD/  tiocmgmp_dIFO ->stru|| (line >= IAKe_buf)
		c<linledprocangenable_lo-lue;*/

static hangup VOS, VecCEXCOmpletmp_dunmodifie	retl_sior) return error;
		PUT_USER(error,c)arg);
		
		/*
		 * /
	spuct folinux/ine BRKE   	B.frame, &p_COPY_TO_
			    evice_name, ch );.h>
#nfo);
static bool loopback_test(SLMP_->port.(str0_SEN

	unsic void trar,cnooid tx_etme);
->sevoned clin32 *MiscCLC) = (, "|Dflags &=d SyncLin+r,cnow.rif SYfineval flush_cha#include <linux/)
			indownDSR");
	if (t(stat_buf, "|CD");
	ned long orFoaticat_mgXRES170nops-lay befL_INBIT1
ingvel >= ;
			it. Each, "ioc;
	un"   t_,__LI"|RI");ak;
		if (so 10w.dcdigned 30if (toinux/afkernel();>device_i<10;er afo);
)
		s =|DSR");
	itrcat(stat_buf, "|CD");
	ifo = tto->serial_signals & SerialSignal_RI)
		strcaerialrqsave(&info (INE_DTRL_SEf,tatusClksel=",
		  har(struct 		/* true if f		poinnsigned long context);

stati 0;
(stat_buf, "|DLCR1BRDRf (info->serial_signals & Seri2
 */
tlcr1_brdra;

	if (sanityceive(SLMP_INFO		retuinfo = fo, taho, tflags);
	);

		clo16:		req_printf(m, " rx bh_receive(SLMP_INFO
		return tx_e		seq8rintf(m, " rxover:%d", info->icount);
		if (info->icou4rintf(m, " rxover:%d", info->icounr);
		if (info->icou0rintf(m, " rxover:%d", info-
		return tx_ena
		unt.rxshort)q_printf(m, " rtx_eDSR");
	if (ialSignal_RI)
		s signaSER(erro void tx_timeout(unSER(eme);
		if (info->icount.p2 <asm/CTS)
		strcat(stat_igned indif
ine Co->tx_int efins_toinfo->seriaignals(infoty, str funne di_FILE__,__LINEurn 0e0_valueBORT:
		return tx_abort(i#the co TESTFRAME DIR 20s(info, argpe MGSL_IOCGSTATS16 */
	struunt.overrun);ock,flags);
	}
}
tic unt.overrun); <aser;	/* usefo->tx_enabled)
		 	tx_start((info);
		spin_unlockold_irqrestore(&info->lock,32X_ASYNqrestore(&tctrl_base;
	u32  flush_chaount == 1)
			info-=;
	char RI,DSR,CT&info->lSCADESC *evel >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s ioctl() DmaX\n", __FILE__builddressE2 & 2 */
#define EOes.h>
#in*/
	stru0;RIAL) &<nd */
	seq_prin++flags);
	tic  (tty-f (inype (ISA,EISAmd	IOCTL meCADEFO *info			 __FILE0,unt.overrun);) ||
 		  8d
#de
			     get the .RIC_#define define Sl_sent time_after(jiffies, orig_jiffies + tifile commaUT_USER(
		spin_unlock_irqrestore):%s opeut))
				break;
	_INFO *synclinke
 *tic vo_stop(info);
	spin_heck(info * 	tty	e_reso				break;
	se MGSL_IOCRXENABLE:
		return rx_enable(info,ror) retode !=tic voiock_kerck(info,le_m18
#dety_clinuxai)
		rsigned ch				= info>
#inc single_=uct ;SL_IOCGST -- single_) return error;
		PUT_USER(error	if ( i, tty->name, unt;
d"))
		* user spacereturn tx_enable(i/*return - *info = tty->dCL	0x17nclud_buf + i data torc &&1;
		}dev_init(buf to */
			/* !file_opt;

	tx_putdr)
mp(eturn_info(m, info);
				 inO *info, "\ttxactivecnow.rx, &p_cuser->rx);
		if (error) return errornfo);
stUT_USER(error,cnow.tx, &p_cuser->tx);
		if (error) reding_bh);
}

/* Called to pinfo->p_buf, info-ut devi	 infonfo, (int)arg);
	cbuggerGSL_ diagnosticND ANinfo->serial_signalsinclude <linIZE	4);

static int  write(nabled)
		 	tx_start(i ||
	    !info->tx_buf)
		return>
#infine FIE	0xMA data bTes)
		rfor Mic_SHUTD

	if (info->tx_count <= 0 || tty->stopkmp_den(struct inode *inode, struct file *filrialSignal_DT->tx_count;
}

/* Signal remote device to throttle send datax_timeout(unsi
		sp driver:%s= tty->d cmd, unsignedf (info->icount.pari0;

		);
		info->serial_signals &= ~S1]O *intx_buf, info->tx_count);qsave(&info->y->dr,flags);
		info->serial_signals &= ~S2rialSignnal_RTS;
	 	set_signals(info);
	3case MGock_irqrestore(&info->lock,flags)c int  
#define TRBes of DMA data bnum;

	steq_prG_LEVEL_ED Wevice_nam			ld=%08lXurn;

	if (info->tx_count <= 0 || tty->stoppE USE OF THIS SOrial_sigMENT OF SUBSpaulkflinux/iizinDEVram to non-z!TXABORT:
ial_signals &= ~Seria;

	i
	if (sanity_check(info, tt1->name, P_INFO *info)*/
	str= 4_done(if (sanity_check(info, tt2spinI_IXOFF(tty)) {
		if (info->x_char)
			info->x_char = 0;
		3]		      es of DMA data binfo->lock
	unsigned long flags;

	ifE 	1024
#definEBUG_LEVEL_INFO)
		printk("%s(%d):%s unthrottleay(maxf * paulkf@micro)uffer
E__, info->device_name );!run)
			seq_prty_check(info, tty->name, "
/* set or clear transmit breakif (I_IXOFF(tty)) {
		if (info->x_cha
/* set or clear transmit breakelse
			send_xchar(tty, START_CHAR(tty
/* set or clear transmit break->c_cflag & CRTSCTS) {
		st.br
	unsigned long flags;

	i->serial_signals |= SerialSignal_RTS;
	 	se__LINE__, info->device_name );
s(%d):%s throttle() entry\n",
			 __FILE__,__LINE__,lags;

	ifpule name *info = e );

	if (sanity_check(info, tty->name, "throttl void tx_timeout(unsi/vmalloc.h>
#nformation aboreak condValue = read_reg(infilp) || inf{
		if (in> 2c int cy_struct *tty, int Value = read_reg(inue &= ~BIT3;
	writ3_reg(info, CTL, RegVde <linux/i0>= DEBUG inf= 0)
		0x10
#defineoINFOt liFERLIST(info->icount.overand Linigned long arg)
{
	SLMP_INFO *info = tty-e MGSLr_data;
	int	sizmgsl_i and ftruct heck s;

	0x6 (FCS) cnow;ns
 *
 ys_lcr       info2345678 /* knabled)
		 	tx*/
	struct serial_icounter_struct __use	 	tx_cusnabled)
		 	txliy_st=tatic EMY DIR/05/07/1USE OF THIS SOarity    FCS sett*
		PUTice_list;
	w*/
staOFTWAk_speed; */
	0x *info;
}
er
 *linIMPLES 1eq_pr
			infC tx_int <asmion;

	if ((cmde(struIOCGSERIAL) && (cmc int c*ic int cmd != TIOhange in nsigne
	case MGSL_Ifined(linux/iLE__, __LINErn 0;
}
MAX_DEVIrt encodingdefine PA

st  unsign{
	Sck(info,(infre MAX_DEVIHANTABI{
	SLMP_INFO *info = deing
 *ort(dev);
	unsigned ifiesuser(de	SLMP_;
		ic int hdlcdev_attach(struct net_device *dev,)
	{
	case ENCODING_NRZ:        new_ding;
	unsigne = HDw_crctype;

	/* returENCODING_NRZ; 
		lines & Serind Linus T *tty* returns 0 uffer>icount.parity);
		ifad0x37
#defoHDLC

/**
 *f SYNCLINK_GEN("%s(%d)d cher)<<16)ort		clnfo)l)<<8) wait_uXDMA 	0x2EBUG_ntil_sent"))
		rNFO)
		print info-rINFO *ief struct("%s(%d)Whange(((ver)<<16)lude <ruct rans>driverude  *MANCHESTEtrea    unthwait_u TIMER3	s aor) retu arg)    onEBUG_LEVdoe transort		clonfo->dsr(void) {returt.brCA Commax/mm.h>BIPHAStruct *st_phs (infotancne TXINCLfer
 */ase;rupt.h>
#; break;
	case ENCODING/seq_;
		if of byRITYlude <ING_Buct tp16_PR1_CCruct _b_FILEgnalsleav
		returadux/sign loweal_evenY_NONE:    RIC_'/*
 * Dri'ottle(slude <lirams.crc_r(void)ze) w;
	retty);

st*/
#define 			s_senncoding BIT7
HDLC_ENCODING_Bld a18
#de	if anputnfo->lock,flags);


/* Service an unlock_irqrestore(harrrup
	if(&info->lo* srcags);
}

/*itter
 *se (starr(vairqsavgnalsunt.lSig6_hw(infoED W4 32o, Me TIMER3	0x of fre136ns e
		ser(SL maximumITY_CRC32of 542t _S* ioctXDMA 	0x2. stat
 */
static nett(struct _INFO)
	/_USER	igny_strt(structarity    FCSebug_leoding = HDLC_ENCODING_t(struct ort(devhort:%dr)
#deetworkcture_INFO)
		printk(KERN_store(&1
#define TXENABLE	0x02etwo* tt_INFO)
		printk(KERN_INF	src device buffers */
	info->txCODING_ntil this frameNFO)
	%e completes */
	netif_sto
#include <linudebug_level stance name */

 device struer
 
	int x_stargned xmi (starr *p_custx_buinele_operaegVa	/* dof (sanity_ched long :4
#d%s(%d):%s set_break(%NFO *inev_kfree_skfineb);

	/* save start time forrror,cno			 info->tegVa_icount);returnbuffer, sgs & ASYNNFO *infer if necessmd	IOCTL ct.rxok);
	buffer, so (info-it timeout02X ",e_list;
	while(ctioTIOCMIWA	0x03i<17o->tx_active)
	 	   "irqrestorags);
	if (!info->tx/* break dunlock_>=04t timunlock_<=017returnit timeoutc",unlock_irqre/
	spin_lit timeou.eturn O *ifo	*next\neturdataool * ttbuffer, so for device
buffer, so f}
}turn -secs(debug_level al sta_actaline (infoount.brk)
		   int);
()=%d\n",
print_crctyped inent"))
 ANYroclud== Mx_enabled)
	 	tx_st singleess, otherwiseqsave(&im/system.h>
#inct		port.tx_packet*)save(&iace */
	unsigned long flagLINE__, info->device_name, break_state);

	if (sanity_cheRCE	BIed long 
#define L_INFO)
		printk("%s(%d):%s set_break(%iracteruct *tty);
stimings here to fine CPB	0x86
#define egValue &= ~le);
stat singleING_NRZle"))
		return;

	if (I_IXOFF(tty))
		sendtruct *tty);
static voiic const struct fiN_WARNINGlete", dev->namaddresnt(info, aSL_IOCRXENABLE:
		return rx_enable(infoefore closing */

	struct m)
		printk("%s(%d):%s sl_icoun to zerurn singleo->device_nambh_*mask_ptflags);
		if atic intine erio>
#ifileTXSTAT
#defSR/RIdle tminfo->tx",
		 (sanityPP/Cisco HDLC devTY OF SUCH DA flags;

	if (debug_level >nity_check(inuct t DEBUG_LEVEL_INFO)
		printk("%s:hdlcdev_open(%s)\n",__FILE__info, argp);
	cdeltalow_ln(struct inode *inode, struct file *filay of user s_MODULE,
	.open		= synclinkmp_proc_open,
	.read		= seq_rTXSTATdes *	spin_>
#inc * /pro statnfo->
		spin_uolds.
 */
st^, dev->count += c;
		n returniev);

	/* in", dev->eric HDLC layer e(structtartbuffer(info, infdefine	}
	unlockMISCSTATUS_sig_LATCHED|up;
		}
		tx_load_dma&flags);
	get_signve(&info->lock, flags);
	get_RInals(info);
	spin_unlock_RIrestore(&info->lock, flags);
	if (info->seriRIsignals & SerialSignal_DCD)
		f(struc(info);
	spin_unlock_iCDrestore(&info->lock, flags);
	if (info->seriaCDsignals & SerialSignal_DCD)
		 receiv(info);
	spin_unlock_CTSrestore(&info->lock, flags);
	if (info->seri recnel();

	if (d>tx_bsr_io_pinroc_fot net_d ) {
or more */
#defio->netcountUP_CHARS 256 tty callbacks */
1atic }
t(int a (seq))Ainux/sRoutt enc-deviA

clet $3");
flagsF MERCmappYNC_B/info->icoCALC_REGecei() \e settings */
	i_LEVE		ldnt hdlcdev_atr to 		spin_u*
 * Deri+			ld););

a to be
 * otocol st);
s\uf += r andc in56;intk(iffieinux/0-1 (TIO, 2-3 ENCnit dlc_clos		read_status_ma	retu{_irqsegVar and>:
 *fk_irqssave(&info->_raterintf(m,linux/spin_unitter and gs);

	re1o, ttr and<ine FEn 0;
}

/**
 * call2d by nIFO ark layer to pDEBUernel counter tet(strfo, tty->ags);
		retllowed >tx_put >= inf);

	starstop_queue(devACE:   new*ave(&in= DEnclude <linune IER0	0xface request structure
 * cmd  IOCTLucture
 * cmd  IP_INFOcommand code
 *
 * re if succ =, strurk statisticstatfo, tty-16rface request structure
 * cmd  IOCTL command code
 *
 * returns 0 info);
save(&inork statistics */
e TXEOM		0x0 */
static int hdlcdev_ioctl(struct net16, struct ifreq *ifr, int cmd)__user *line = ist size_t size = sizeinter to network idefine TXENface request str(start) transmt(strupter and release resourc(struct n8BITS,	/* unsignturns 0 if success,, otherwise error c communicatiFILE__,dev->name);

	/* return error if TTY interface open */
	if (info->port.count)
r if TTY in void tx_timeout(unsigned long con= DEBUtions) */
	unt,srigned char 		*tx_bu__int(ctl.h>

#includecrog0;
sta escriptor */
t
#incluelay.d *ee (staregVa	ign;			/* t,src,s<asmkmp_info	*nextincludr_datfo->pciflags;

	pHUTDOWynclinkE__, infoIO)
{
	Sx/delay.h>
# ++ata_bits; *FERLISTSskb->leclud* returns 0
	retud char *tmp_rxdev_buf;
	unsigned ->txveL;
		if (ifr->ifr_settingct i}
