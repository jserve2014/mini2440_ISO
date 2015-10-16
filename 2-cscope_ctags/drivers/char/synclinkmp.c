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
		* $Id:idle_mode = HDLC_TXIDLE_FLAGSulkf Exp $adapter_num = Link Multipogate Syncporth sportrotocol ;

		/* Copy configuration  Exp to device instance data */gate Sync
rq_levelseridevaogateed mgh sprhys_lcr_basor Mpci_resource_start(e.co,0m
 *
e Syncte anscaync
 * hare trademarks of Microg2te Corpoy Paul
 memory* Derived from serial.c written 3y Theodore Ts'o *tatctrl* Derived from serial.c written 4)ter.
 *
Because veremap only woal.c n page boundaries we must map
		 * a largered fa than is actually implemented for the LCRRANTIEand Li range. We WAR a fullNY EXP.c wring atWARRASS FORESS ORy.ES
 *the GN paud Syoffset    = Fulggaion
  NO  Deri& (PAGE_SIZE-1y Theodore Ts'o BE LIABLE = ~OR ANY DIRECT,
 Corporat*
 *EVENT SHALL THE AUTHOR NSEQ Deri FPECIAL, EXEMPLA * IN EXEMPL LUDING, BU, SPECIAL, EXEMPLARY, OR CONU GeneraUENTIAL DAMAGESO, P(INCLOSS OF Ul PubUT NOT LIMITED TO, PROCUREMNTIA BUSIN AREINTETUTE GOODS OR
 * SERVICES; bus_typor M5 13:BUS_TYPE_PCIogate Synclkf@flags = IRQF_SHARED SERVsetup_timerO, Pxp $txSE)
 *,  INCIALout, (unsigned long)RISIm
 *
HERWIN ANY  ARISINGND OONTRIAL,WAFAGES
POSSIITY 
				 USE OITY IS SOFTWARE,OFTWARStoreLAR PPCI9050 miscritttrol register value b ISVER a flawES
 * in (((ver)<<16)preventsS
 *) | (seq)s from beARTIread ifES
 * BIOS as

#ds an("    Deriaddress with bit 7 NT SDISCLES
 * OS ISAR Pint (rel)<<8 int $3"))PTIONcessPLIED Wwhich`AS IES
 * write#includh>
#needed, soinux/if
initial)
#if dR BEchANTAES
 * bitshum AR PED Witten UTHOheodoreaMe <llude <AR P.h>
#ES
 * pt.scheON)
 *<linux/e <linux/errno.de <liAIMED.AY OU| (_ OF U.h>
#i= 0x087e4546e BR,relux/E)
 *ial LITY escheunknown - ifPECI Pup trinrsES
 * occur,>
#in__flip.will belip.hto indic<linthude <inproblem. Oux/pAR PUng.his FITNy>
#incstizedOR P<linhisr.h>
#ip.h>
#incio ser0 sere <linu_flip.h>
#in.h>
#ieavailablelinux/serial.h>
ock.h>
#inc= -1;
	}

	returl THE ;
}

F THic voidh>
#inc_inux(inttDVIShigh spILITructed frdev *icro)
{
	SLMP_INFO *ial aarray[SCA_MAX_PORTS];
	>
#i
#ine BR/* alloh>
#iinclude <linux/s<linuup seroock.h>
#incldmaude sx/seria W(>
#inc= 0;>
#inc<psock.h>
#incl; ++
#inc) {
		flip.hasm/i
#in]ort
.h>

#in(lip.h>
#incclude,h>
#iogatef
#inclck.h>
#include= NULL/'' Aquck.h>
#--qude >
#inc>h>
#iCONFIG )AGE.
kfree(b.h>
#ifined(CONirq.kh>
#incogat}flip.h/* give c* wrof
SYNCLINKMP seraTNESortndifd MAXupt.includelist peos.h>
#includ.h>
#inctermioe GET_USER(error,valu || (dmemcpy_SYNCLINKMP_MODULipial a>
#inh>
#iror,des05/07/1t,src,05/0pm
 *
add
#inice>lse
#define_MODULFRE, EVpin_lockyude <&EFAU COPY_FROM_USEUT_Uirq.K_GENERA.h>
#inest,sdlaimlude <lirom seriayine Gif ( !NKMP_rom seriasefine COPY_F0])alue,ser(ux/hdlma_bufcopy_to_u,sizdesclude <lLC 1
om seria THE rm Paul #rogafirsirNFIGitopsth");
/serik.h>
#include1value,addr) error = get_user(value,adueueock.h>
#inclulude =DAMA 0

#include /
	HDL))
#dchar loopback; */ARY,@mtten L DAMAAG_UNDERRUN_ABOgs; */
	H,
 *
USE OF THshorti386OF MERnus To_ENCOF SU_NRZI_SPAC
	0,				/* igned char encoding; */LUDING, B drisigned long clock_LUDING, 0xff,GE.
ned char enccha BUSINESS INTlic/
	e driCRC_16_ BUSINESS INigned char encoding; */BE LIABLEilter; */
	HDLC_CRC_16BE LIABL,	/* ze) ? -EFAULT : 0lse

#incROM_UE,	/* e <s= coprequest_irq: 0

#include ACEigned chOR P.
	.38 20nkmp_interrup OR P.
d char encodigned shC OF ned short crc_tr stop_x/delayname	ASYNC_PARITY_NONE		) < 0short crcrintk( "%s(%d):%s CantIS Sorpo /
	1unsign NEG=%d\n"/* uns__FILE__,__LINMEM_* unsPARITY_NONEigned short crc_tr paet fla	8unsigned short c0* unf}
		rogaE   512
#dened shigned sh/
#defied = trued char * high estmble; */
	96.h>
#ne COP_

/* SCA-Iconos.h>lude tty_opeyPARAMs opINGorkq.open = 	u16,
	.clo
 * hlowergnedSCA-I =SCA-I gnedput_chascheddr SCA_gnedflush SCA_ADES6 bits 16 bc wrnext _roinux/ SCA_u8	bu of nfer _in preff def812

.c wrbuffe; of nf buf	pad1r a16	length;signedioct<linf_ptr;	/*throttl des/* statpper unf buffers o	/* pad2;
gnedsend_xse;	/16ESC;

RACTgnedbreak_e;	/* set_SC_EXaddr ait_until_senrror bookkeepARTIseSCADESt_e,addrADESMAX_;SHAL	/irt_atop.c wx_holA-I ..c wrporaburelea*/
	u8rrorurpor buffes;		/iocmge	physcalvirtressi of A-I DEVIof sx/sedeproc_firt_ad&ata_r */; *X, *Pirt_,
};T_USER(error,vaEX;

/* Thecleanup(h>
#m/sys*/
	rc;ystemock.h>
#nilockdefine BH_TRtmpe BRe
} SCD"Unload  de%s %s4
#d drivigh crc_OWN_ON)
version>
#in= cogateal_
struc || (d= co(rc flaty_unint $3"){asm/t	() asmorkq
	inr)FAULTS   4

#orpora faileLCble;n;
	idsr_down

struc
#in102HUTDNKMPMBLE_L  o.h>
EMY DIR	0x40rc styl	bufown;p<asm/p;
	int	ct<asm/ddr ncludvreA-I _signaor,desTHE h=performed */ unsign * D;
	while(RSION|| (d*if_p_REAM/ts_upogate SyAMAGES
 * */
eq))(usoorkq<lin *iry;	/tr bufgnedUSINESS purpose po/
	1,;
	ued by SPPP) SCA_t _s		h>
#fine COPuser(vIC_e dr
		hdlcdev_ex<linuypedp#endBREAK& defhar preamt t;

	oreout;tmp_rxh ofx */
se_dnal_ecicrogatal adapse=izicro byt= coICES; LSEQ DerFAULT ase;	/*egdetec, LPR, 1); /*(errorow pf bufING,v/seri	ort		clnclude = co_char;		/o.h>tmrporANSMIT unsignedruct _s		liny SPUSE  && de_tmpfnsignedd frts_up;
	inutruct __EX;

/* The  */
; */
fo }ed chDinkmp_ock.h>
#iPARAMSentryt		clo.
/fra>
#inclu*/
	_SER(eurpose pointude <e BH_RECEIVE  1
#gnal_ece dr
o PUTadtus_morpor

/* Thegrtualxt_ptr();
  				timPOINT_t	sd ch s) */
	un	dN_SHUTD

structT 100

stl.h>	_nfo {
E OFnt		t	ri_dd from Paulsme to SIZE   512
#d		ignorrks o* siz || (detruct _sy:kmp_inft	cx/errno.hPCI

struc,
#incl
/*
 * ance datae
i_dowclude <ut;
dA-I 	dsr_down;
	ih>
#inchdef ruct t _128t_q;= co!	dsr_down;
	inis) *r;	/*-ENOMEMADESgotlude <eo {
	void Ick.h>
#inCA-I */

	spinltouf_pturyndictx_tpad2;
} che->ownheckefineFROM_UE;chedulARTIber 16
nt_wait_q;
 = "ount;

	wa" bufferasncludby dtten  */ttySLMding_bhter.bool bh_majude <ttyh splProtecPaul #elsem* as' AND fALL 64ing_bh;

	bool bh_RAD TO,TTY_DRIVERCIDENISERIALing_bh;

	bool bh_sub			/* cT() ass to pNORMn	statuint cts_chkcs.h>
#al addr* vnkmpstdkcount;	 t	tx_t signal *
	innt dsing *.c_c; */ =
		B9600 | CS8rtuaREAD | HUPCLaddrLOC a/
	int ri_chkcount;

	char *buffeispe8

#d* vi 
#dest timeout time*/
#dth;		/_lost_resster.USE OF THal *rxAULT; */
NG 2 maxhar *sREAL_RAW;
	*NOT et */

	spinlt	dsr_down;
	i, &opst UT_U;	er;	/*nkmpications) */
	unsynclinkmp_inft_bits; */
fo	* */
_rporabCouldn' flaerrno.h	dsr_d

struc
 * Deviance data structui_downpe */

	spinloata_bits; */
ctioheduling bh */ckce drES 1ISR SCA_
	spinlwbookq cha_head_,t	ctsint i#/*
 * Devnt_wait_q;
	struct timer_li0000overflow;
	bool nt i>
#include <0;

/
	st:
count;

	waihys;NKMPt lor;
	u1nfo {T_USER(error,va_nt	icurpose point_bufo;
	int			l allocated Rx last_tx}

modulaSER(er512
#de		*tuffe);d;on froint tl allocated Rx >
#irel,linurror,val>
#ie SCAnt rned shorarityt.h>har		TxCLKr	/* Rr ie1

#dals/
	SCgen

	se <lrogaode;BRG1_va OF signTxD/nete0_ved #if ;clude <R
#ifS   4
#dnsignic
#include <lenar l_
	un#if (size) BH_POSSSMce.ht] buffent			tf ( ser_co|| (d/* MD2 (Mity/Rated Rx 2FAUL * 01..00  CNCT<_bit> Channel ConnecARAMS11=Lo of Levice iinux/seriransmit time16		MD2HE */

#defiddr )(EFAU/* expannput b int BIT1 + BIT0)m_usltip;
deg */
ex12
#deiTxC6 bick 
uffeic/serial.h>
struBLE_LENGTHlude regh>
#inc|=t irq0 << 
	u16tistatus_ma* 2m_usert timede <linux expans;

	u	1,	RXS/TXS (Rx/TxF THIS S req_t of tot7MBLE_LReserv; */DDVISbe 0 of tot6..04  C_counSseria,q;
	=BRG of tot3bits; *_counDivisor, AMAX=1t CONTRACT buffer

	unsigRXS, 0x4inux/pe;			/* expansioTaliz Paul s
	}_HDLncludem list  t adanterrist;		/* f total allocated *t ada<asm/ioch>
#inclu, 0=n13:2l int init_error;		/* Initnint  t;		 (ISA,Ed;

PCIicatil allocated & ~erialmicro buffer metered inimeout timer */ vents {t	tx_tphysi* curr manvents /
	int ruffeedress * fromrial/ptra OF RxC/E OFPiu8	pED Wdiagnostics  PRO timeout time chaux/t_ABORTca_offIx/time las0_mem of #includeress */32 rx_ENT ;
ng */

A-I 
 * S of tiftfor Mi.h>,{
	MGSwiclosep.c,vter 2Mbp,size)= coRISING4.38 216	lck_t of uct _lisvicexpansioe anuffeeneryncLi1;  32 s;
>
#inca_statcEVENT _ena36864l_vals_mask*
 * De;bIN Nvicey) */
	u32clude <insirenco of TWAR *	odor_ctrlER_SIFF	booofF THIS in
	1,			l */ucoar ctus_Aeodoredrop_rts0 dis stasrlregAUXF THISar for Mi_ed c[25]isc_ctrl_t;
	e Coed cm_requesteodor_ctrl _RECMicroga	;
	sTMCVh>
#	stak;lse
#uested;

	un BRsing */fore s (PCI =r en OF f crc= fCLK/(TMCAMS ^BRa_of/
	u32 ppinUT_U_t !=its; */
s (PCI d ch4745600//*
 * def expansi!s (PCI FAULTbase_requesemoryclosing  =v;
int	iemoryfor Mic.h>
#expansients Sign!= 1 && closing t la2E OF THION( devateinfo provides 50/50 duty cycle *al.h* whelocaf toISCSTATUSis 1 or 2.S DCD */
#al1s_sc9 alwaysRIlinu	/*size) Mr devestedchANTAde <ux/serit.h>
#esteddlc.hED	(Serial>>_LATCHEUSE O/(PCIileMISCSTATUS_CTtoo bigSE OF cha) */
	u3, dindicude <nty 21_valincrHE IM BR		/*oneR2 s chaISCSfor(;e in DSR *> 256 cha DCD */
< 10;nt laste<lined(	ED	(SerialSal_D;
	int lcr_mem_requesteCHEDequested;

	unsiigned char* sca_TXS_err0xf0mory aCD */ syncl buffebase;
	u32 lcr0x090x05
#dex_bu0	0x1efine 5
#defi1	Rx1put x16
#defi2e IE2R	0x18
#deEine IE
 * DTMC_requested;

	unsclosing t lo
	>
#includeint lcr_mem_requesteset;
	int lcr_mem_requ lcrset;
	int lcr_mem_requex1cl allocag_buf[2;
	u32 recetrucPPP/Cisco HDLC derx_of dogate Cot netcouar *bSCSTdebug */
	HD>= DEBUG_LEVEL_ISAGICex[SCAMAXDEcd_d	padST	0x26
ADVISDor Mance data structur fromscd short crcefin
BD	0xR2	0x16
#dTRCMDput RESET;
	u32 
#incl0h>
#incRY ORXRDY_err1a
#define IMVR	IE0D1D	0xf
ne SA35
#);	1,		D	0x4
#Rxeodoree SCABUFSLFROMDL	0x3
 * Devie TMRXDMA + DSR0x23
6ine TRC1	0XS	efine/serne CTL	0x31
35
#3R1	0x15
R	0x1SWABORMD	03a
gic;
e;		/eCSTne I3cR	0x18
#dCST0x3d
ded chaimeI RRCacros #define TCNne I CTL	0x31
T	0x681
#defrx_ buffehys;fal*/
	uNR8
#d62
overfk2; TCONRH	0xg_buf[ buffe_even0x18
#dSR3D	0x51
#defineFCMD	0 wriR 	0x1a
#dne I2ONRL	0u32 e_status IE35
#dR1	0x15
#deEne I261
#defineFIruct2b1
#define MD	0 wri1
#defineMD	0x80e BARH	0x81
35
#defBARH	0x81
ne I3R2	0x16
#dCT8
#deCR	0x18
#dSAT	0x62
nsio	fine_s_statcefine DVISSTRMODEuct talue,ad/* CorddressabeZE 	D ANrxunder the GN  INlne RXS	033R	0x18
#deDL8
#define TRC1	0xTH	0xegister maR CSTx3d Livirtic;
te CO TCONR	;		/* /

	stprogram rx dmre Ts'odefine CST1	0x3d

/* TimeeTCNTH	00x61
#define TCONR	60
##define TCNTL	0x60
#definer R| (seq))Macroed inne TRC1	0CNMD	060
DE_e dr,i.h>
#ii <1	0x2f
f12
#d_har *b i++ ADESC_EE)
 *_LATDMAx94
[i]xpantu
	SC	/* 	0x8c	//	u8	pad1rs_sc4 sha[MAXOF MERCSCA-IsICULa SUCHs_scNT() asR35
#08hogg  de
	u3lem_re(
	ch latencyONRL	0>
#ix94
ESC 	128 SPP)x16
#	0x18(i % 42b
#de	gned ITY OF ctransm paricatioR	0x94set;
	bf or DMCTS */0x8ONRL	 <XDISABL/1sptr; ((rptrucoc;
DEVI9R2	0x16
#dDMR16T1	0x3d

/* TimCDAa
#deR	0x94
#definIMER_ex[0].ion
  TMCu8
#d80
#dCINInew rx_ex16
#ine CRCEXC8
#d0egister maTXEOM#def0x3a
#define ABERTfine 7ne SAR	0x8PONfine 8
 */
#define T) | ( - 1BLE	0x12
BUFCLRine R1	0x1;		/* cgth of (ne IN0
by BFLLtance Cnal_evx16
#d)ABLE	0x12
#define RXDISABLE	0x13BFL,ios.BUF, EXCLR	0x4
#define D#defiine CTL	0x3CNTx6TH	0x610x65

/64
#dedefine	0x18
(EOM/BOFe SA0#def2CR	0x1 BCRL	0x8e
#define BCxf
#defa
8e
#red chIE0IRQs,A16
#d2#define /serlloc1;  /* gneaountYEatusITe TXCRCIARL	0xDAfine8R1	0x15
#dEDA	0|=efinine SYNCCLR	061
#defineE  	BIT6e BARL	0x8BFL	0Rlinue SARL	0x84
#define SARENABLco	/* x8a
#definTMC BFL6fine TRC1O3
#define	0x01
#def4
#de	g_buf[E	0x02
* DMtransmitist 
	st strx18
IDLD	BI fr84
#if ctrln */

ist tctr4
#def0x09
4
#defSPPP/Cisco HDLC detDIR	0x94
#defiDAcodes R2	0x16x8CR	0x18
#dDAR
#de8 	BIT1
#deBE 		Bfine FRMefine CLR	0egister m)E#def | (
/LUDING,vic
#define BARB	0x82
#define SAR	0x84
,D
 * O INx1egisst	tx_tim!_INFO *sye SHRT	B Multie SARL	0x84
#define STRH	0x85
#	boota_bits; *Link MulcouKD addBITne RXCRC_HDLC;g irqe SHRT		BCINISARH	0x RBI*atic int copy_frk_sf auto CA-Ie SHRT	Bvice TS*/

inactive,_even { }erthis pavirtTSdefine20x18_
#de char_b  dethCULde <task

	shSCA_adFTWA Thnpt levis i0x94
#defi */
#dsfo { co, THtes.D	0x95
#defineents	invalutx_dMVR	0
#defineexpansiero tox8x3a
#defin!Pigned majorAe clICES 1ESET		zero toed majos */
	&#defiilude_AUTO_c inE OF TH	t ofSE OF Te SARBefine definpa!d */

	odsr_doSE OF TH& Sdsr_dSefiner engned ld chale.h Link MusFTWA/
st|=tic
	u32 IE1	6457d opti_statfers r specifieodule_or number,LINK.c,vshum zeroodule p	mmanSCA_fine L	0x2
#define RXDISAefin/* unsiess e SHRoding)(((tx_MAX_at_fifo */
	H-1<linux+
 * k		cloHDLC, 0)y_baMemory
#define CTL	0x31BLe
#define BCRH ter.ine SAR	0am_aA-I sy[Se in D of Pf_baWN_Ler" Set tic T	0x93
#define DIR	0x94
#defi= "$Rtic ion: 4.0x09RH	0fulTXer M (T	0x04
#defTCR	0x18
#dRXEN		BIT7
( {0,};

mel,acros 0LE	0x13r Maine  <lin#def
T	0x14
#defiABLE	0x12
MP	0x1 are P/Ciscoid Eent)RXRES <lintatic int syremove_one(
	spinl = {
	{ *dev "SysnclinkmpTE_SCA, PCI_ PCI_VEsynal.h>
#agnedtobufbits; *are tbl[] =orkq{R	0x02
undCABUer RegID_MICnclinkmp1e EDA	0x8a
r fo_DSRsize_LICENSE("GPL|= UDRNtic sAL	0x8a
#define ED1H	0x8b
#dei_drive, bool1a
#define IMVR	SR1_requested;

	unsi
sta +38 20m_use* high s

static int synclink SHRT	fine(pci,tatic iTefine R	0x94
#definXIE_ID_MICROGATE_SCtic int synclink2
#deTfineE  	BIT71
#one),
}efine  	BIT7
 left iE_SCA, PmodF ADVISED
 * OY OUT O WAjiff IMP+0 Set msecs

#iCHARS 2(5000m_usero.h>
#include txgnal _nameS_DSansmit
#defnclice_coun RXCRCINItIDLE   	BITTx3a
#definfineDe_coTT
 * Devix80
LGice_counfine TRC1	CCTup;
counx8a
#dd char r, io lodrivertx/se sync tg_buf[0x26aak_on_load IT0#definDMAruct tty_stP
 * Tegistopned int chanetcouc inlRM *oldfine TRC1	RBITruct tty_strucOVRN(struct ttr,valRC *old2x20
#define 1
#define BARB	0x82
#define SAR	0x84
#defindel more */
#define WAKEUdefine SARL	0x84
#defiver;

/* numb*dc in_	_inputdev,const struct  $"levelclink chaata_bits; */
itMICROGATE_SCA, PCI_ANY_I,scrit

	spinLE  t *tp(str 	BIT1
#dear * 
#inc3 	BIT1
#di_driverGSL_(/irq + 
sta_TAB = {
ic inLE(pci.ed c		= "atic int s",x3a
#define txh>
#iOFPD   	BIT1
#defid_te.h>	=ruct *tty, iEVICE_T	/* x/mmer(static int scharacterspatusng
#defe GAPD ine SYNCD GD  ,

	spinlAL	0x8a
#define EDAH	0x8b
#define BFL	ct *tty);
sta	0x9 RXCRCINITefine TCOuct pci_de* Set thi#define6x8a
#dy) synclinkvarg);
staEPR	F.h>
ct tty_struc FIFOirt_iROGATEl.h>
/seq_fi oolleOGATr */

no meq) odore omiosMIuct ktermios *old_tey_
	spinl*efine DARL	0x80
#defu8 TwoBytes[2]
} tem.do nothevelk sttwf_count;a;		/* staX_DEVno XON/XOFFttt *truct tty_ copyh>
#incatic int&&CROGAThdx(PCI oET		0ty_struSYNCLtati_even__char;uct tty_char)l.h>OGATunthrXCRCCodore 	*vcask stime tjor, ity);ad eax&& igned char* scaSR0_errrialgned l,si/tlROGATE_SCP_INFspclinkMOGATE_SCSLMP_INFO *
	stmand.
 *
#incated)_DCDong argR2R	0x94
#;		/* cr major = 0;asefinlcdev_tx_> 1)_dICROtem.h>
#incin {
 l.h>
fSCA-I 16-	1,		sROGATquest)->p0.h>
 },
	{ 0, },inUTHOltxst o++
	u3C_EXed */

	et_parbuf,a */
t#ax_T4
#d_ errFAULT :t_stat, 5 13-29:44  _
	voi * syncl)nkmp_pS __user1 *info,  synclink chanclu MGSamsc int  get_part  get_t:29:44 LMP_INFO *info, 
static int  sgeenab
 *
SLMP_INFO *info, iinramne(struct pci_dev *d)B, *((u16 *)P_INFO i DeriveMoid hdlcdev_tx_-= 2linkGATE_SCAGloba.tx +e);
staSR1 */
#defm.h>al.hS1 bree le		/*meinfo, stror 1ct ttyslotinfo, r majoP_INFO *inf>
#incruct pstt  s handler(D)->pri<linuprioriTS *net_L_PARAent ioctl handlers
#in enable);
sta bool,  int  get_paTS */
#* Demmer_liswMP_INFO *ittyy_struct fige itatic int  set_txidle(ruct ttyNFO *info, int idle_mode);
static int  tx_enaable(SLMP_INFO *info, int enable);
statt _ne.h>c int  get--	spinlatic veid ad tx_ena++*/
tEOM	A buIT4
#definidle(LE(pa.h>
#inhar_bller Register macri */
ruct efine DARL	0x80
#defiT		0overflow(SR1 iit(SLMunt tx_eENTA		BITCMD	0x2cRSIONcludetic int maxframe[MAX_aLMP_ICES] = {0,};DTR +VICES] = {0,};

me#defiENm(SC_EX
oo);

statintrytic strlosing * tbufferOGATE_SC 	BIT1
#dee EDA	0TS */
#dhcter sFO *info)raisATUSfo);
sth>
#incmajR	0x80
* filp);
static void hangup(struct ttatic  void flush_buffer(struct tty_stru
	.AL	0x8a
#define ED2H	0x8b
#deCROGATEBIT1
	0x94
#defiister nclinCHc void f
r;    	faulBIT3
#LASYNs */rupt.TE_SCA, PCI_AIT6
#define PE	BIT5  claFERLISTe ABT	BIT5
#define FRMefine H	k.h>
#i=nude thr) error = get_useor synstat	unsigned cong dai]ne WCg  claim_re hant_stats(Suct ttyon *8) | (sPx8 zeros */
	u32ile *hronousci_dmunES 1gonBtatic int set_breaile * efinrademar(SLMP_INFO *i_INFCist;		/* (strRegsing */
e trademar(SLMP_I get_stats(S*infogned 0,ar* memory_base0IC SLMP_ 07..05  PRCTL<2ress,id rl adae <li* d Syile *>
#inc4
	stHD flu, Auto-R	0x02
(RTS/CTS/DCDGINFO *3a_offset;
	bool sca_base_reO *i2c voidCRCCC,>
#inCalculde <l phy file *defines_bits; STOPddress Sstru	1,		(OF T,10=ca_ofh>
#inc000statsLMP_x54INFO *inhutdorrx_b/
	u32

#ded majoO *ir;  ru*infp(syndP_INsta|=_icoustruct file *file,
ct ttINFO *inNFO *info)1de <lin bufE		/(1MP_INFO *info,6  BRATEddress,12

#viceINFO=1/1 01d_dm6 1ad_d32de <1/64igned 5ress *TXCHR *info);txestruc err_SLMP8 bufg,01=7me(S6,11=5NFO *in..(SLMRbuf,d char erFO *inar *)t;

	bool rx_PMPM *info);Paal adefin;

stDULE_10=XDMAde <odefiney) */
	O *info);tatic voNFO *irt(S4of uswitchh;

E OF THrt(S/*
 *d tx Multc	clo7:c int  gestats(S44570 S2; ce dr;uct tty6t  sbh_kpoionc in54570 S3_INFO *info);
v5idbh_hhandler;
staticwor.h>
#in3INFO *info,O *inf} *info);
static vtip;
stat!= orced:29:ine SCA_ Multi bh_handler(strl_DSRbh_h IDLD	BIc int  get_p=ansmit(SLMPe <lODnfo,2;
}c int  get_parx_b});
static voiSLMP_fi1oSLMP_INFO *info,);
2tatic void tx_se2MP_INFO *info, int void NFIG_frameAULTned , u3nd LivirtuDEVI(PCI`AS Iication32 phe an isrde <9
#de	device isr_rxrdy(SLMP_IFO *info);ic void set_r isr_rxrdy(SLMP_I(SLMPdevice inLMP_INFO *info)* HD64570 SCAe);
static voistatic 2oid isr_rxrdy(SLMP lcr_R_test(d char serialint  get_pa);
static INFO *info);
static vdress *RXCSatic voi char serialINFO *baof blet;
d buf, 110=DPLLr_txdmaok(0loc_BR<>
#info)addrnetcou allocated  isr_rxdmaerr=id rstruct file *file,
tatic visr_rxrdy(SLMPestedrxr_txdmaid rel_io_pir(str_INFO *info, ifo);ad2;
})->priv)
stat  sde <c_dEFAULT(SLMP_INFO *info,;
stca_offl_rxdO *inf(SLMPe);
statgisteMP_INFO *info);
staticloc_tmp_rx_bubufor usSLMP_INFO *info);
staticloc_tmp_rx_bufo);ested(SLMP_INFO *infoCe <linumory_bast  get_pa6,4,2,s; *LKSELuct tty_0	SCAc;
	u *in voiAuxclk outatic vo0r_txdmaok(SLMP_It free_;		/rue if NEG ong datpoint
s/conidle_modexidle buffercom
staTXENABLE	0xamegistt_TWARfo);
static vRRCO *info);Ready;
static LMP_INFO *info);
sfo, char* dest, const chv4ic int RC<ic i>#deftest(Srig; */
uct ttr_txR_LA>
#in*list_e1a
#define IMVR	fE0 *2s */
	*info, u0rt_adC *lint counIZE   512
#deMAX_ *tty);
sta_rxdlude _regr_txdmaok(SLMP_ITtic void Trite_reg1atic void write1val);6>
#in*inffo);ARB	0reg16r_txdm= "SmB	0x8tatic void 1rite_reg16(SLMP_INFO *int_*info, int enist_ex,int counc void write_reg1ount);tatus*info);
_fifo_leveeadriverB	0x8art3est FIs (q_fi-p(SLh>
#inc te_reg(SLMP_INFo1info,resourcatiTL, MSCIude <linux/errno.atic unsigned O **list_ex,ih>
#incs,escri
/* IHD/irqC,);
static de <lint fiab *li1=CRC+NFO *idefi/BSCtatic vde <linIsion = *
ong dattructrial 1=vice ntatiel /* b(SLMP_INFBRK,tatic t fiof*/
#=ic vile *tatic vr_txdmarcedoD,urposint co sl_iatic v(Ss

s 1=e SHRT	_INFO *io_levGOP, goort crc_UT OFll (LOOP_rxdeCLUDIN*info);
Not0a_offseTcodiTSit);putue = 0x007e4 lcr1_b1= level i_INFO *info,1		/*o);
static void set_r1list_ex,orc int  startup(SLMP_INFblock_til_ready(sr_txdmaok(SLMP_I0x0void isr_rxdmaestati 0x00(SLMP_INFO *infoR	0x02
static voiFO *info);
stl handlers */
uct fiINT(strR100 =struct file *file,
		 e,
	ve_fifo_levc_tmp02linkmpPR<2ce dri
	u16list_ex,int0=rESS  robunt;

	f_basBRKDSLMP_INFO *in lowestatic  ioctl handlers ;

// Number rx BTD are  cansignludetennit(shaCROGATEhutOVbits;e(SLMP_INFO *info,tic void isnsignet tiadr Micpaefaultsunsignsca sca_s/*
 * defi*us_rCLUDMP_I[s(SLet_tASYNSCABIT1
r,val_INFO *info);fs(SLet_txor(SLMP_Isl_iite_reg(SLMP_INF foraaticd            1=aftuest FIlistv;pll_DCD indica//ned 'tumbeO_INFINKMP_MO_nit(o, THtsMA CBTD d;

_counoned c	0x94
#//info);efineselected. T/slained(scmp_getl_va enARAMSCES statir(SLMP_I.(struU0x32
#dThiswdescmr seri lcr1c_buf_hardwter  0
#define CS;}c void isatictESC_bits; *_INFOer eptr isr_rxdmdition: 0=eveLMP_n handrt(STXC_anity+c_tmagic =
R	"Warniatic loatatic void wriority 100=rouobin
 *
 * 0000) _{
	{et_pap(s.
 * This al SCAtxdmT		0x01
#definH	s function iic void tx_senfo, char* dest, const chic void txor(SLMP_INFd frtest(m(badinfo, name, the atatic v (!info) {
		printk(badinfo, name,O *info);
static vX *list_ex,int counority = 0x04se condition: 0ers d, C = 16t  g SANIT 0x04fiCRC-16,c vol-CCITT-16struc0=all trC writecounnux/strude <ar *, e COt_idleclude1is funclS */
nelsse co3SHAL 8requFO *info);
staticuct tty_stragic =
 flu0CTS (!info) {
		printkasm/iipmaskFTWAR * ldisc_defiiv
stat  - pant  nfo,vCorporait(Sest(ic vpline
e_parauct ttrcT			/*  MicdrplinTH_8 wrap %s\n";
	static con24570 Seach channel completULL, (!info) {
		printk(badinfo, naNFO etatic unsigned O *ADDRSest FIFOA	0x05
#es BUTinfo, naoc;
h */is functt, char fo);
static NFO *int_sigic void isr (!info) {
		printk(badinfo, na) {
sigcludes_maskts chate debugging of dynaminfo, na);
sta_l h>
#inCalling this function ir_rxdmaerror(SLMP_INenable port.
 */
static SCArxrdy (!info) {
		printk(badinfo, na: nulused, NRZFMt fiNRZturnFMt char* sr5  CODuest FI Encoic vtructNRZlist_ex,in3  Dg of dynamim/sy#s (PCI only=8return 1;
	}
	unsigned long flags;

	line = ttyline;
	unsigned long flags;

	oid isrrytty_struct *tty, struct file *filp)
{
	SL*list_O *info, u32 de }, /*cic void isar *fsigned long c:	 SCAic void bh_recle port.
 */
st/
	SC(nfo, ittBIPHASE_MARK:e, "	u16"))
 SARt74570 Surn izinD2;
}aka FM1test(SLMP,src* $Id: phys_lcr_ ) ock_Eid bh_handler(strfor Mic6sK_GEp_rx_b128
, i0itnclude1024
#,AGE. SCA_MEM_SIZEefine* $Id:P/Cisco HDL,nit_error
#incka Manch	ck_ttructfor0sanity_ ( info->init_y->Bt.ttIC_HDLP_INFOt_ersupstrue
 */
sVEL_INFO)
		printk("%srams		pri> void -en(), ols(SLfref(ld = ;
	}

	tty  SCA_MEM_SIDIFFh);
srpora=n",
	name, info->port.count);tatus_ti forh_hao);
 SOFTWAon: 0=every cagic =
_INF_DIVCE_I sca_b(SLMPe.h>
2;
}__hw(ntk("%s(%d):%s 3*lisR1 */
filp) ||n",
			mp_loCLUDIN& Afine_CLO * O){
8=%d\n(&info->port.clo8	1,			tibl(info->port.flags 3;
stat tty_struct *tty,
nd enable port.
 */
slude (!info) {
d long flags;

	line = ttDESC_EX *list_ex,int count);
static int  alloc_tmp_rx_buf(SLMP_INFO *info);
static vsingle write_rx_buf(SLetlock, flags);
	if (info->n_memory(SLMP_INFO *info, char* dest,  and enable port.
 */t.flags & ASYNC_CL);
mBRG %s\n";
	static cone_wainfo)nt(SLM}stat&info->poef(ld++claipinbe
stacka_raresseq)
 * $or Micr*info);
src, unsign
statn 1;
	}
#else
	if (irt_adaddr)stup(info); dri		if _ex,_deref(ld);
t  alloc_tmp_rx_bu/xoff retv (!info) {
		printk(badinfo, nant) {l_ready(tty, filp, info);
	if(info->netcoustatiask fce_liSLMP_INFO *info, iter *us ott betonetlock, flags);

	if (info->port.c		"W_u
 * de		/* 1st open oo->riteock,i386gic inr=%d\);

	if (infnameber 1workqu/* 1st 	u16	onr(stSC_E}, /statit 
			 aencoding;up;
	}

	
	}

	if (debug_level >= DEBUG_LEVEt.coun= 		prisc_ctrl_value;by a breakpoitatcue = 0x0*ly->port.flaed_INFO32) | (_= 1)
ice i;tty->count == 1)
			infoed;L_INFGPDATA (Gie2_vS INrp bufI/O Dtatimory_baslist_ex,inl_ready() ret_roomturned ataatic veanupatic vxmcomp
	retvalf port is cloLINMEM_n",
			nfo->port.tt retvfined itxSE)
 out USE OF THIS Sled SANI

/* Called whend2;
}static void  /* ttyes.
 */
static void close(struct ttount)ruct *tty, struct file *filp)
{
close(struct tty_stru
	retvsigned char addr, uatic void write_reg16(SLMP_INFO *info, unsigned char addr, uve_fifo_level = 16;	// IZE   512
#detxhandlveturn;64570ops->ar cic ino HDLdev,Synatic void write_reg16(SLMP_INFO *info, unsig;
staticd\n",
			 __FILE__,__LINE__, inforx request FIFO activations_screname, info->port.count)l =0WAY Ored RAM
_ol re, routi * $Id:igned char addtive_fifINFO)
			it_until_sent(tfo->timeout);

	flush 16;	//efine
 *         kpoin Paul 2 lcr1_brded by32port_close_e_name, info->port.count)o->porequested;

	unsie recros _name = "Sy*infncLink SR2MR,writer* memory_basrite_reg16(SLMP_INFO *info, unsigned char adriverTMOuct  */
tic dd-s: 1=chane W-bthis functimagic, name, routine);
		return 1;
	}
#e, N
moduefin reaEs

/*t of -T4
#d<lin<linuxn 0;EORY */
#u En zerofC | (n;
}gned : ult_tnfo->device* The wa u16 status);

static  1 xn alln"%s( */
staclaim;

/* d when po *bad->na0x1ne Vapter(SLMP_INFO *tic st1
#sable port tty_p*
 *WARRAc voi2er_test((upaim_ */
stefin24c voioc;
		BIT7INFO)
ng: null(struct ttCPB82
#equested;

	unsiERY OUCOaticx14
#dion
 >> 16("%stty_p>ree re"NFO *ps(%d):%s urn_leve operation
sr);
st	n .trx_b>= DEne discfo-Y_ID, PCser uct ps);

	if (inrt.clos= ~_wait)s ifAL_ACTIVEttings
 */Number ..0>,t  tiocty 10.us/con ced lignal s/ist	tx_t flagASYNude vidintaER(ABORT>
#iARRse tw_loc
 * 1st clefins.lags;

	line =e GNse co
		p0
 */ajor0->port.flagso->device_namef(SLktermios ("%s(4 pci/ ;
cleanup:
	if (debug_level >= te unsignedo->device_name, inelinud):%s close() 32t.ttyt= NULL;
cleanupon to
	change_params(inffo);

	/* tty layer will rek("%s(%7e404


/* mios->c_lcr1_brdrBAUD)) {
		in800028_INFO)
			->c_ARB	0ansmitrt.countialSefinPCR,e_iniPtermios Con)<<8L_INFO)

#endif
	return 0D
 * ignase co4e lin BRC,em_rentry;sdescrdiCBAU*
 * The wansned hecksynclinkm */
	if (!(ol1=
	}

	/*afosin("%s(%d):% Thealling into the lin O *i
	    ttS */
#de Handle transever changetus */
	if (!(old_tc_cflaeachag & CBAUD B0 stas) &&
%s(%into tstrupreamrotocfo->port.coun!_() aswait_until_sent(ttaruct fisl_i * De (!info) {
		print layerde <li
	}

	/*.  I
 *
 * 0ptr;if>= DEB,__LINE__,line);
		return -EN bufferfor Microvoid MPLA debugoc;
_wai*buf[TY_CHoid ill refinuct ktermios *old_terhort crcnmand.
 *OT Lllow);
	motinclbuggARTI0x26ynne MPON		01gnedap API* Hvoid  tif da.h_ldisc_der int idleated aller ->L_INFO)Def (LEVEL_INFO)
id finclude :

	line = tt_cfl7es not alEVror=%d\n",ck of dALT_ZEROS_ONES:_rxdmaerror(SLaa * 	tty				closintolock_infPaul FDVISErguE IMs:
 00nter to buffer containing setestua
 * 	count		siffnter to buffer containing sorma_,tt. Disaiur Seri	buf buffer contath;		/*
	if ifo); snfo, u(aruct eanup		05/0} SCAendorporaarams(infdata_,tty)
{
V#if :	;
modu  !(sentkpoiDLE  y_struct *tty)IDg & CRTSCTS) |i
#incQuerar loolue,aTO_ned lonAk.h>
#ofk("%sV24struct k(nfo {)o->devicSPPP/Cisco HDLC deay  !(reak_od_termios->c_cflag & 16struct k=_testtruct *tty);


	/dVEL_gpINFO)
		printkBLE	0x12
#D   SLMP_VEL_strubik.h>
#inacterse COset;
	bose"))
	except FILE(SLMPanditten to st;
	int  (!info) {		spin_unlock_irqreb * defi)
		ady(intk("%ice_: syncl.* Dev
	1,			t.refline MISRiv)
	retva!(0ied opt_icohe lin=every cmaxfo);
o->lo05
#CESBLE(p0,};C

#defABLEforeanup;txck_i2tworkquSCADESC; accumuCA C ret = #else	c, DCICESO
				 _ty->HD64tic void status_timeout; // P *li0..3

staHD(info-<1,3,5,7>ce_n=%d\	,
				 _&,
				 _rame and wait before accepting more data. RIng_ucompletf(SLMPtatic void status_timeout*/
			/* as nt)DSR		info->sta0,2,4,6d):%s release(t count,
 indi
#defi		goto start;
	ion
snfo, ifo);
,
		DSRo cleanup;

	iffta buffwrO95

/* work Derdal_si{
#ifdramestatt maxframe[MAX_metval) {
	ncludex_bufxefindiciscoce drn_unlocnfo->port.ttef(ld);
	et = sanCRTSCTS &&
	    !(ttyamic16uct *#dBtlock, * 	count		E__,__LINEee reCTlockP_INFO *infoge condition: 0=every cycle
 *    : -ERESTART__LIn*info, /* ttyt retval, lilags);
y->termios->c_cflag & CRTSCTS) ||
 ait . Wait
		Tmin(i OF DTR);ar	    SCA_ze%d):ine _frame_size - innfo->tx_c*by Th_strub Sereanup:+=  */
count, retval);
	DTchars(wn(info);
#de
	info->pignal 	if (info-{
			ret foystem.h>
#incinfo->tx_count);
	}
start:
 	i|k1; o->tx_couse"))
		return;

	if (debu DEBU (!nfo->tx_ckpoint/e SR2MA Bame_siCitfor sf(!info->tx_active))
	2 
 *
 * De;nclinkeort.RCCA */
sta4
#defin
		ttymmncludstatters fu<= 0)
nction inme_siclude <c,v pa set_tr(is a)ehar *nvelystatmakER3eck tter_tes& deE2	0xic vardNERIy& !tty->stSHRtty, ct ktermios *oldr_e* indicatic vout += c;
		if (info-even eeNFO *inD	BIk( "void sAH	0x8b
#EJECMD	0lags);fi efine IEFucce_waitter_tesurn - RX_char()p:ueningru_BUFF THE h  NULL;

	 0
#endif
 <linux/pci.hdebuwrite(nfo-exD */
e SA	unsigf (infdeak;_count;f ID_MI2a
#buf,) {%d)\n" of DMA data buffddr */
ra;
	unsig contaistructin %s\n";
	sta;

static int  nt;/

#defined write(r;

	wake_up_ #def_RECE(tty-ajor, ioP_IN .teeriald!ajor;
		gok,(debu shorine ,tty->of DMA da>
#irestat/serial.h>
ine MPON		00write%s(%define MPON		01,flags))_strwrite(==buf)
	 <= 0)
debug
{
	- 1)count(SLMP_IN;_)
{
	 1)  debug_lesynclinkmp		BIT7
#did synclinkmp_r>tx_bu INTVR 	0x1a
RT	0x04
SLMP1x8a
#ynfo->device_n14
#definfo->txULE_DEVICE_TAB>tx_cputCINI
			ret writedn_unltx_count -< ine RXREJECT	0x15
#			 __Frestopen_wntil_sent* (info->params.m%d)us_mf DMA daafs(%d		if info->device_na	ret = etur		info->tx_0x8a
#writeefine IERlude <a*/
#define_counk1;  /* gnnt. DisaSLMP_I

/* Add 
	boe._coun SANIose"))ext_out
#inclu;
	un#definY_CH (debu *list_sing :	mit >tx_T4
#dOinfo->patus/t tt<<8)#defit +=h>por}

)
{
	r_txidNFO *ildol regis->c_came_ & CRTShange parmit I			_,_count	reMP_IN
			__FIL,ttyusedon: 0=evevorame_RxESC;

ty_chstati(PCI o(mastruct nfo-(!info->tx_b0ne(stymborvalu})
{
	 *list of dyn%s\n";
	*/

#define Vock_u

staem_ret;		/t);
}
	*uffeMAGES
 * p_lottyx_);
stad)
	strucoc;
_fielr(stMPON		r->n(info);
	tx_pIT 2mfuncr dri/* lm_ex;

Csignegain:*/
	96ssumst_b/* If port is cloEL_Ijor,fine R17ne GEav
		if (c <= 0infoWaituf, i unte fo, name, tty_struct *ttngup"tive)S __,tty data buffock,o_hdl;		/* staname,nt. Disastopp. Touct _icounch) {
		/* whenbreak			 _looklock,ame,a non-tm/syx_activdata;
fer */
k( "%s( TMCiic i(l_sex_actitry\nt. Disasefine ar loo16C32

	i->po);
	}
  deCefine IE2	0x2a
name, inf (!info- coune disc/* 1st op tty_struct *t*list_ex,i;;el >= DLMP_l < dapter(SLMP_     !nfo->porn 1;
FO *inf_CHAR IMP=  to 1/5;

defi/buffer/
	Sretval)LMP_
#defiE ask1;xffigned buf_C] = chMP_INF(info->netc st.h>
inic i,P_INFO *iicall/* stafileeturn _stopped && k1; &&spin_lock_irqsiver_dals(%dne s it at liver_data;
	uFO *inf->vreleaddr[0t	c, rstopped &&c int->->	SLMP_clude <l,CI_ANYNIST-meansport.flagsd when  transmit ch);
	}

	i,) {
_sign of dyn
		nfo->porffer(info,nfo->port.;
		if (c <= 0)
debug_l	rnfo->port.cTRC1	0arSE)
  = min_void cloYNC_INITIALIZEDl_DSR_devt write_timeha( "%een ';
	}' buMGSL_>
#irk_dma>max_ine  ge 2 & ofl fo0x2a
#mit bu   	BIT3
#H	->termios

	tROM__sleep_on(  	BIT1
#de)t __usrmiosor =SErqsav Serixp $rl_r,; */

	retva_active )ct file *f&&ne Rune_c_cfl((SLMeq)ies, origl to 1/5 +ne RXic ineast 1.har		cSLMP_INng */

t is A_MEM_SIofefine IE2	0x2a
() as*lis			 _ruct filsops->rO: ded_until_->tx_c	SLMP->par"%s7 EOM_struecs(msg) Wait char_time)
			char_tourcl; Ssed fNFO * Wait tendin (infodefi5 Aon t Wait {
			mson tition: 4e));idtty,
			MP_INFocmsetn(stE)
 try\n3 Od;

	28
#d,ted;

	

/ptrack,fdur->te* If poratusiomp_i}
2rs m,		ret:
	uunt = /
	st->recition:name, ix_active else
		char_intk("%fineq)intk( staift

/*uy->te	 __(port CA-Idefe sa		BIT7/*n: 0e:s mdatatttf est	if (1
#define );
static ol sca_statct8 * is cl_,__LIlags);

bh (re->tx_cong, ch *PStatus byters (tight ||	retuiver_data;
NIST-PCne R/* Send a blo(ings he froo sinfofysignS SOemory_me,rcoun0atic vstoppedon init_em{0,};rnel( someSUCHrity/frabrIDEDoout))
idlne(st rouermiosABR1ce_name,in %s\n";
	static coif (!info->tx_e discinfo- similkeeping sset_tNK_Gn-stopped &&<nfo->tget_paro cleanuptx_timv->tx_couVEL_6+Micrel >3 DEB2 * dekerneSLMP_INtx_counthas
#inclurror,vaupddefiGloba/

	st>seri->tx_acymboashedulineort.c} = 0;
			gcou6t charo);
static vrxts arn_unloC_HUPe_slnfo->tx_coun5->tx_b->lock&infoenaafterIDLD	BI
	ifR BE	c, rDED ini3g buffered charactersTMC
static vo buffered charactercrcn_un{
	 */
	if	chd);
	EBUG8;

/sgnedoid sinfo->tmgy(SLMP	0x94necleamf (ins.  	BSCA-In_unlo	0x2a
#		__F(info->paramFO *in data bufffl}y_hung_up_p() c The#define DARB	0x82
#define BHit);
0
#define BARL	0x80
#(info->ity)active)=%04Xt. Di
/*
 * Deviance data structurMP_INon: 0=evevoid h(%d)_LEVELtk("egister madevice_name,info->tx_count)leanu		
 trace_e;

	e;		/* 1/5* lowstimceptiINFOf (!info-].s.countraa
#dearam; */
,(jifflush(de =el >= DE ||
	 (ze -< 0			rertest(SLMP_stopped &&>e_mode);
static int  tx_enfered characterne V
static voiruct aitess.h	0x1x_coun(ug_levde <igu(str	if (medidefion: 0=everyarticlinpinus_tx_donE)
 static voest(Sstatic intk( "%s( (!in			ret 	SLMinf*po->device_->/xk_t ruct te operchadata from ck_irqsame and waope\n";
	static const 
kart:	unr(jiff(>tx_count - 1 __usm/irq			bre
			/* trmin		 _AGE.
 ion awatic vo(countd1_valene Sgned shodataRCINI charfine TRC1n",
			__FILular txtx_cou>tx_count de <spi_irq	+=(info-> count,(count>tx_count --gle write operatibool, call<linr *rot(unsigned long, chaEFAULT 	o = ttyhinfo->t
agotal =%d\ne(struct pc	SLMP_tlock, fl	}net, char_timgeout.
	 rx	SLMP_INF_LEdata from _counhw tx_sp	
 	if (inung_upm		ll();_UG_LEVEame,cttytry

	/* If port i


/* 	2a
#* wr: HDSCA_M, starting */
tce_inifrethingo->tx_count);
	}
sta* Dev ( infon thLEVEL_INFO)
		printk("cflasigno->max_frame_size -nostiear_tievel !__,info	
lar to : (ret 

	spin  	BIT1
#detime_roome ABTD   	BIock_kernehangup(	if (e SHRT	,msleepeedestore(*inforor=%dULT of torxfo) entr	0x1 BFLand.
   	BIT3
harsmpt firiorityconst chard)\nP/Cisco H 1/5 of estimated freo
	 * tx_activek(infoT the s {
			{
		//T to 1/5mine if there is someactiv_delSC_EXrintk	} rogaprintk/TOD>tx_ee,addn conttm"))
isDLC)osing *clude <);
	spin_uefine IEqsave(ev_lags);
	}fine SHRT	SANITY	SLMP_Ity_struc !=
		i;
statihar prea( ( */
	int netcount TXCRCstructe_up__buffetval);
, char_g & CRTSCTS 
				b>tx_count ock,flags);
est(Sbug_levfo);
	}ic voiptyo->port.flagsO *infaitd ||unsigninfo->tx_ainfo->ake_up_intevel >= DEBUG_LEVEL_INFe_up_rs()/overu,e, routiit);
}
	}B	0x8ning datpy serialf DMA data */
	oparlt_plose(strucfioctY P
		reDEBUk("%s(%dwrite(lags);
	} ch;
=%d\nO)
	l();uct o->l;)
e IE2>tx_count -)  tty->t)rt(info);
	}, "tx_release"))
	



	ne if there i= {
	{ PCI_V[i
	SCAory avalif (UG_LE 0, },addr
			__lush_cMP_IN	strOF MER *
 * 0$Id: syncl:%s fluslease
	 	tx_staic void txrelease(e_re	SLMP_INFO *ier conx_active )c voi *tt+ogate Corpor
stan";
	statfile object f0x94
#d, char_tim= 1	if (inuffer(info,iidle_mode) {
	{ P, char_timFO *info) (in		gotx_release"))
stx_active ) 81MP_INEL_Ier a/* IEOTactive) itten to shty->na};
M =k_irqsave(&sent(chcurrrt(info)stru4efine DARL	0x80
#def
#incluriented) mode struval[.h>
{05/0
modumandfa= 0;f55test(9mgsl96}eleasinfo->lock,fla
		spin_lo, iR PUL, EX(	}
starinfo- */
s
	}

	/(long, ch_EX rxt, 0);,flags);
	 , otherwis *tty)
{	if (inf>= DEBUG_LEVEL_INF IE2	0x memory_test(SLynfo if (debug_kmp_elock_s.h>
#incvm_rx_b	if (Diag/(32 *_		ld->oF2	0x2a
2 lcr1WCA-I sd_dpact *n{0,};vare comc_tm$3GSL_
stadong irut_SEN  !(frele * fn modHRT	BI relusednamefyx;			.oid fmajme tcombin o);
st_list;seq))MAX_DE161
#defineIMVE0 *1c
	}
startf (deb
		prindebu	if (infdeb	}
start(i+1)%GlobaOCMIWAIT) && (cmd != TSAunsiOUNT)= ch;2d ||_counrt.close(1esteheckIO_E1RORNFO )s->cr3turn -EIO;
.
	 * N_u_active ) efine0MC)writ< TTY_IOIOCt	if (	 t_txidle(Snfo, iaIDL"))
c
	/*tatu__,__LIN_counCS29:44 :info->tx_bet_txiSA0rn set_pargp"))
->tx_b-EIOIOCGver foLE:
		retur_INFO 1dle(info, argp);GCGTXIDLE:=%d\no->txr;	/*ruct tty* 	arg	co(info->n the;

	if (debug_los(%d):%s tx_hold()\n",
			__FILE__,__LINE__	unsital allocatedsent()
{
	TS; NFO )
		priargm/system.h> */
	unsigned SUCH DAac (PCI only) */
IT) && (co->tx_count);
	ADVIS= debugging ofol s&);

?ies;

2 :nfo = oO *is;gned shMP_INFO , ar = ning=);
	case )argd != TIOCGIrgp = (void __user *)_chars( entry tx_cA data buff */
l() IrqX	}

 porOR T*ent)IN_,info-
	pin_lock_ir] = o_/bitANY  eachSCAsmp}

/
 * 1st fun'' A_sER2<7..4>uct fmeAMS _icRNG/DSR/CD/ystem.h (se condit.open_wait);
}

/* Set(&inbadide == MV;
	}

_IOCags UENTILE:
		retu:%s o:;
			ncLink1a
#define IMVR	ounter of input  , arg+d thr)CNTH	0x61heevicexpnal_0xescaatic v(debug_level, int, t		inumetettymaignereak_pCONvoid1are co_irqsa TXCRddreCTS)f(struMCp(in, argNFO *in/octl() if (info->p)d characar		wrMF, Comp_coumaharslcr1_btest(al.hdataar_ti=every 		ret ECMIf(stF Iurn modemze)
		

/*nfo);
		spin_t.countit;	/bug_= NULL()*/
#n aINFO *p isEANTI/
	cze)
		*list_ex,int cIE2	0x2a
#to document
		/* W1AUD) &rror,cno

	/* If port isons are counted except foMCScasex5*filehorttatusIOCinfo->maLE:
		returrock,flagn set_L_IOCGS=1systemsl_icoL_IOCGS--static int S)S */
#dMGSLlue,add
sta);
		CABUFSer a(, unc/
	into, argp);
	case MGSL_IOCLOOPTXDONE:
		return 0; // TODO:o, argp)ser->rng);
		if (error) return error;
 >= t)a->dcd;
	spin_uABORefine IEock.h>
#innt);
	}
star_couock_irq(2ing */) and free red intata
ransmitter
 rg)
{
	SLMP_INed byiin
 ue = 0x0lct pc	me )void m(info-ARAMS(OP		estolv tx_icount%s\n";
	static const _FRAcr_bLMP_INor,cnn		PUT_USER MGS,E ARE
stas not &p_creakd: sye, "		 *16;	unsig)feretx_b_lcr_b
		me_afte
	uns",
	O)
		#define IMVR	WCRL	PUT_USER(ererror) return	0x6H <liABORT,cnow.P_IN TMCrun,cont(	if (error) return err	if (buf_overrun);
		 erroror) reHetti_param(LE:
		retur-ENk_irqTLCMD))
	tty_poPC_frame_P tcal adNFO *inte debugging of dNco HDR(error,cnow.cts, &p_cuserBT_USdefig/overuotimerror;y_base(SLalemandSignal* Dr(SerialSignalic inonst strerror,tl		PUTle,
	, ev nee*/
#dreturn 		
 	ifRnfo, nam{
		al adretur AREcountlocebugging ofnfo);_cflaalledreturn error;
		rethar dmhar info(strBUG_LEVE __FIaLEVELze)
		,ffer.aulkf.brk,PP) *onst strity, &p_cuser->parityDMEt = 08ow.rngr (%08X Sd2;
}urn 0; // TODO: Nn errgaulkfirqreRANTIEGen 0;
}sLMP_INTxRDY,RinfohTxINT,Rry_bALL cos 0-1rn ername,
		info->phys_sport.se 	txinDMIB,DMIlockE__, i=%0-3_statctrl_base,
		info->pt smit bbase,f TXC excep_base, the)
		 *Tr dmd != TIOCGI	if (debug_level >=, startinIPtatcurn modeminfo(strostiLMP_->vel || nclue	SLMIAK
statnsigch>
#ledX, *ANTAint opeo-tic voi
	retvalp_cuserVOS, VecdefiOB0 stostiunmodoad,

ste;
	(error) return error;
		PUT_USEReturatctrl_base,
		info-listpow.rngclude <efinRKEstruc.fo);
, &p_O)
		pri* If    urn error;
		PUT_	if (e port.
 */
s(tty-ned shor_IZE	 (!infnfo->pot couchecs */
	uruct filraturn _rxdmaeetmic v->sevo	caselintl()MiscC	mem= (, "|Dtic voidBE LIABLE+turn errfore testk_ird all dattty_flip.h>
#inc			ret =owut))GSL_case M_unl != Mnfo-CDl_sigose(strucorFl chaat_mg	if 170nops-lay befUT_UacteringLEVEL_Ice_nait. Each, "i1 */
#d
#detM_SIZE"|RI");intk("(infoo 10w.dcdNFO )
30ase Mo
#incafFO)
	nfo-g flags;i<10;(cmd);
}
nsigs =|rial_signtrca & SerialSignal_RI)
	intk("%sorcatbase;
	u32 staticx06
#defineRIcount	seq	if (ffer(in * $IS) cEc inL_SEf,d2;
}Clksel=

	tt 			 evel >= ned long conty_strucose"))
		return;

	if (debug_o->t& SerialSignaDLCR1BRDRl >= DEBUicount.txunder);
		if2s_timer= ~(Serirt:
 	if (ime,  ldis (!info) {}

stao->lock>initahinit, char_tine diioct16:}

sqE__,ntf(m, "efinbhoid ldis (!info) {info->tx_b_DSR		seq8 " rxover:%d" TMC:%d"t = 0;
	t.
	 *;
	spin_um, " rxlon4t.rxcrc)
			seq_printf(m, " rxlong16(S, info->icount.r0t.rxcrc)
			seq_printf(m, " );
		if (info-na
		unt.rxay(max(m, " rxover:%d_DSRrial_signalsf (info->icount.t/
	int
	if (ines.
 */
static voidUT_US
		st, info->icount.rnt.p2h>
#in is nt.txabo & Seri_ldisc_d<linuxe .td fluinfo)fincall;
		if (inf	u32 so->ict file  fu
		 di be
 * sent. D)
{
	device i	if LE:
		returx_acbd-syi#igneco TESTFRAME, EX 2idle(info, ar &p_cuser-GSTATSo);
list;		e:%d
		retu);long, char_tis op

	/* */
	seq_pri
	u3ebuf;

EVELbrk)
	k,flags);
gned long fllease"))
a_buffer(infoldl);
		goto cleanup;

	if32lock,f/* 1st ope_statctrl_requesttrcat(statanup:
	if ->tx_cou-=(inf,__LRI,DSR,CTve,info-(info);
	 Not supported, need to document
		/* Wait for modem iDmat (DCD,RIA_MEMbuitypeessE_mseinfo, str
			 _ne GET_USus to en0;vent) &<{
	 listf(m, " ++, char_tio);

 MGSLnfo->uested;

	unsmd	IOCTL mert_aget_statty->name, ",buf+1);

	seqeep_
 t.txreset_anfo->se_lockigne.usede SHRT	Br(SLMP_Iing steturn;

	if ( debug_level >= DEBUG_LEVe,
		ransm
		PUT_Udma_buffer(info, in 1st o	returpL_INFO )
		printk(oid hdlcdev_tx_ Ser
		/* Mtx_see error cbuff( info->inter to c void )
		printk(, &p_cuser->rng);
		if (error) return error;
uf_overr Dev!EVICEvoiL_INFO)info->in*
 *1yncliL_IN#defiaigned );
	case _delaller k.h>
# ( dele_=t errial signa --ytes in rcat(stat_buf, "|RTS");
	if (infr=%d\n"o->max_frame_
#defds(%d):, "\tk_onace>icount.rx);
SER(e/*ct tty_snfo->lock,flags	SLMP_I/delay	SLM+ i tty_strrc &&nclu	}e(&in",
(	SLMtoe(&inf fre!e,
	_opCSPAR		ret =r)
mp(r) recatedoverd pending* 	ttic int  g"\ttxkpointialSigx|CTS"rk, &p_rx		PUT_USER(ererror) return eg of dynS");
	if (info-n ert, info->devict_name, info->tx_cou Protecck_irqsavCall)
		on_t(int,pialSignal su = {vi 	ttfo) return ername,
c &&
err,cnonfig regisN
#de;
		if (info->icount>
#include <ignedc int  alloc_tmphar *b=%d bh_req=%d bh_run=ity->driver_data;
	unsigned long har_b	0x80
#defiMAorporabTesgned ock_irqd_t	evlevel >= DEBUk_irqresto= 0ep_o_counE		/_timer
			test(in Dev*turn; unsigned char dmax06
#define Tty_check(id charac#defin & CRTSCTSr Micted e statet	c, ret =
static void cing_berial_d:%stx_count cmdnfo->devicrintf(m, " pe:%d"arer en	er)
ignal sticount.txunder);= ~S1]->devi	unsit = 0;
			gef(ld);, startingo->	infog, char_tiTS;
	 	set_signals(info);
	2DCD */
#o->icTS		 * devi
	u32 s synclink3fo, argtval);
		goto cleanup; long, chark_irqreort driver";S   4

__LINE__,mory_basef(m, E2	0x2a
>
#i/Cisco HDus_md=%08lX	void __usesanity_check(info, tty->name, "tps type (ISAne VEbase;
	uED ANOF SUBSm_userclude <lzinDEV
		if (ret z!;

stat:
_signals(info);
		if buffer rxlong:%d"INFO)
		printk(1x_frame_ debugging ofus to e= 4staticnthrottle"))
		return;

	2mp_pI_I_hdlnfo->= ch;
DEBUG_LEVEacter  Called toAR(tty)s setti	3]turn 0; *info = tty->dri = tty->dri	return get_stats(i%u\n",
E 	
/*
 * Devie IE2	0x2a
#to document
		/* Wait forf, ing & Cts arx of m_user */
	H)me_si
Disable port and free  );!q_pr%d\nsion);,
			 __FILE__,__LINEt.tty me tNT Sorinfo)rrt(info);ESC_EX 0;
			send_xchar(tty, START_CHAR(tty
 * break_state	-1=set break corogaset or transmitct fio->lobacksunsi
 * break_state	-1=set break cofo, tty->name, "sen= ch;
st.brrqsave(&info->lock,flags); 	set_signals(inf|=
		if (info->icta (our rent. Disable port and free ffersata buffgnals(inty->name

	/* If port is clot. Disak,flags);
pned t netco->lockLINE_"unthrottle"))
		return;

	 condition/* status.
 */
static void cintk("%s(%d):
statiCBAUD:%d"P_IN Hantty-> =O * info)UG_L_sleep_on(&tty, START> 2k_irqrcin
 *
 * 000001c_tmak_state == -1)
		Ruefo);
stru SPPrit3-1)
		Reoe_siL,	0x9V#include <l0n_lock_irinf=LE__,_ IER2	0x16
#o Sert linfo);
sntf(m, " pe:%d TMC
	0,			ORT:
		return tx_abort(inok(SLMP_INtx_cou &p_cu}
start:drivesizmlush_id fl * RetCADESC%u\n"0x6 (FC;
		now;nP_INFO and S */
	ifesto2345678addrk=%d bh_req=%d us to encbool scaut.
	 *erP_INFO *MP_INgned lcus=%d bh_req=%d l chec=l_signEML, EX/ error */

#define VE;
	uns   FCS breto->pPUTisco	if  SPP || ttERSIOk_stic e SCA_0x "set_d chntil_flagMPLxt_pf(m, ->tx_coC" oeve;
	u32DEBUG"unthrcaseROGATE signevent& (1 << RegValu*ck_irqrase MGSL_I*/
#dein ose"))info, argp)_IEFAULTclude <_MEM_ anity_
{
	S
}
lated dar retco ProktermiosACMIWA/* Handsystinfo->inUG_LreCL	0x05
#errorBIsystem.h>
#inco->lockdeingd) {rt(Y_ID,  allocated  1/5nclude stem.hnal rk_irqrsl_icounattach"))
		re(strrmios-> writ)
	{info, asigned long : */
	if new_ Pronew_encodivice w_crcnit_eck(infor) rsigned long ;a hi(&inr);
		if	0,				/*  0000ncodingns 0 me_sials &= ~Serir->brk	ifadlong arg)oe dr&infata
ore closing */	/* Waitase BREAKPOIg/overut);
 <linuxlist umaok(S0x2_usereeping sts(%d):%to document
o tty ited (PP*/

	spin	/* WaitW*/
#dine BREAKPOISCA-I  enco awadriver-EL_IN*MANCHESTEtNG,  portthHDLC_Eies;

3	s atx_counturn t_NONol_signalsdo) errns do_ioctnable(srning=% {or) rflagCAlockmax/mine error  *
 * 0_ASYNsBUG_LE*/
	c struNCLtore */rl_re;

	if (;ESC_EX(info, asigned lvoid seq_pri	u8	yfineSCA-I d loBest(Sp16_PR1_CCing seb
	SLMu32 sleav
}

stata_dater(SL = 0);

	ene SCA_SPACEused'sent"))ri'ice_nasadapter_t
 *
crc_rning=%gnedw_tim	bref (deb	0x94
#defiet ong s open *vel;7
e drisigned loBld a	= sin, STafo {uct tty_struct *
	ttinfo,rr Mican  retval);
		goto char,			"untstruct tty* srcebug_lcharaic vo
 *sen_unlrr(vair(jifu32 sbuf+ */
6ASYNesto>
#i4 32  gefies;

ler k(info,136ns reak,sizSLend imumine CRC32of 542t _Ser */
NCODING_B.ruct  0 || tty->s		brFO *info) Serial/	PUT_	strvice
 "))
		rerror code
  TODO: y geneice drisigned loevel >= D       ding:%n_unloceS)
		cture= SerialSignal_RKERNtion opeCR	0x18
#dnfo->tx_buf)2 thied.  completes */
	netif_INF	= 0;rmios->tatic v;

stattxactigned lo
	unsigi;
#eam_buf)
	%{
	rec_cflag_ver voiftionT_USER(error,va/ TODO: Not  */
	int netcounnt = skblock,r
indica otherw = ttxmitx_t hd *fo->dnlocko, ttdef raegVaspin_o

/* Send a bget_sta:HUTD data buffd)
	 	tx_s%
	case ev_ONFIG_skf(m,b "SyncLijifft;
	in{
			%d Mt;
}

/*\n",
	txactree ut.
	 *);or) re( "%s(, sclose_wai
	case Etrucdr *clud info ) {c(infok newer if necoBUG_LEVn tratic v02X ",hdlcdev_atP) */tx_cL_IOflaguct *i<17ock_irqrestore( o->s"		/* 1st char_tiin_unlotxactiADESC_EX d retval>=04ive)
 retval<=017or) retive)
	 	tc", retval);
		rialS;
}
tive)
	 	.r) retcase buffer \nr) r is tatied. 	if (!info-l conmios-
ce structure}
} tty_sffies/ TODO: Not _requeacti
#deeBUG_LEpe:%dbrk MGSL_I// tx ()V;
	}

	, " rARK:    LEVEL; breaCIALroc.h>
= Mive=%d bh_rened lonytes inesg_le_,__wise, starti_RECE2
#define BH bufnfo-x_packet*) startiTATS:
		return get_stats(it. Disable port and free reSC_EX
MS __userunthrottle"))
	 countkb(skb);] = ch;
	|= SerialSignal_RTS;
	 	set_t time for i	}

	iic const ut &&akeup(t"))
		rf, inter. May be f intree _stao);
tatic voiytes ind long le;

	wake_up_intendition, 0=clear
	SLMPndatic const  (info->netc	 __FILE_le *file,N_WAo);
Gyncl"le_pvcondiMAX_DEnelease, a,cnow.tx, &p_cuser->tx);
		if (error) rgs;

	if ( debug_level >= Dturn rc;

	/* arbitrate serialtymajoreturnes inport and freebhinfo->loc/* Signal r6	phbreak;lock,rint ofe,
	TXSTATr RegiR/RITS)) m
/**
 * ount.tong:%d"PP/Cock,flags);
	 void L_IOCGS&& (cmd != TIOCGIEBUG_LEVELVEL_INFO)
		pest(Sine IE2	0x2a
#to document
		/* :sl_icoun	u16(%ssr);
{
	SLMP_e(info, argp);cdeltalow_le"))
		return;

	if (I_IXOFF(tty))
		sefo->device_nme_sizepara	u16ase(struct tty	0x8cd RTSparaoto ase(seq_r) != 0t_adanitialt of by* /pro->ma  socking_bh=%xoldrame[MAX_^	spin_u
		tx_load_dmaffereable_ID, PncLink		spin_ueCHEClags)_RTS;
ROGATE_Sroutfo->tx_count = 0is para}ew_e

	ie in DSR */signal_DSRD|	printk   info->max_f&, char_ti_INFINFOinfo = tty->drf (debug_l>seriRIe data)
 */
s_buffer(infoRIrottle(struct tty_stSignal_DCDDEBUG_LEVEicouRItxunder);
		if (info->iDCDtk(Kf"))
		 synclinkmp_pr retval)CD		netif_carrier_off(dev);
	return 0;
}

/**
aCD called by network layer when #defiivce is disabled
 * shuCTS		netif_carrier_off(dev);
	return 0;
}

/**
#defange
		!= TIOC;
	un.flags & , *PSo        tranor m;

	L commanup;
	} pe:%UPbacks */56d.  Inillshor upda1	if (}

#inc aerrno.hA tx_neRoutace ot neiAentre cmd );
& (cmdefinemappait)B/m, " rxloCA* cuEGe
 *() \CRTStte_roo

sta2	0x2 (dek;
	case ENCOcontang_bh=%xLUDING, B+f (de)TIOCMeanup;
		} tatus_mastruc\fer"= oid fd lo56uf_lk(to 1/
#inc0-1 (TIO, 2-3}

	led
ial_if ( SARB	0x8t filma

sta{l);
sree oid f>:PACEal);
sock,flags);
_r rx_" rxover tx_nebufferic void flbug_levre1init_oid f<0x80
Eor if TSE_SPAC	if (2it;	/nanup:rkirqsaveta)
__uso->ics are co tint cospin_lock Signal RTS) = 0;
riented) mode truct_t hE		/*of tr    ACeprognew*startinfine
#include <l,cnow.buf_fTATSue = 0x0))
		r(struct< TT fo ) dlcdev_ioctl(strected ransmitterdl_valerro contsucc =y_strurol-finfo-icize spin_loc16r */
static int hdlcdev_ioctl(struct transmitterfr, int cmdDING_FM_ng of dystartinot size = sizedowndebug_level,>port.flags & Asl_icoun modemNRZI:     16y_struct ifreq dev		infoctl()MP_INFO c int=buf)
05/0__LEVEL =LEVELfer contan this  i(dev);

	/* */
static int hx_t h&info->lotk(KERk Muid fl
	}

	/*tradema_port(dev8BITSigned char ettings _{
	con flaags;

	if (lcr_mem __Fg contextCA_MEM_pin_unloen proencodingeturn en -ETTY_unloc */
s	u16	

staetval = 0;

cleanup)
h(ifr->ifr_->lock,flags);
	RegValose(struct tfine Iransmit timeo= co;
	case MGS;

	boolre st(ct */
t

/* SCA-ebug


/*  e TXCRCEXC);
	; */
	9sign.d *e_tx_t hree iign buffer= copy_>
#ieceive buffer "%s(%dignal$Id: cnd codructpt	even_count Disable pIOm/syst unsign	if ( ++ount;

	wainfo);
sSskb->le
#deENCODING_FM *tty)ase MGS*/xoff e(&i (int allocatedty_cve&p_cuser-ifr->ifrd) { shued i}
