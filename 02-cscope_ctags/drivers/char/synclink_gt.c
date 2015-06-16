/*
 * Device driver for Microgate SyncLink GT serial adapters.
 *
 * written by Paul Fulghum for Microgate Corporation
 * paulkf@microgate.com
 *
 * Microgate and SyncLink are trademarks of Microgate Corporation
 *
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

/*
 * DEBUG OUTPUT DEFINITIONS
 *
 * uncomment lines below to enable specific types of debug output
 *
 * DBGINFO   information - most verbose output
 * DBGERR    serious errors
 * DBGBH     bottom half service routine debugging
 * DBGISR    interrupt service routine debugging
 * DBGDATA   output receive and transmit data
 * DBGTBUF   output transmit DMA buffers and registers
 * DBGRBUF   output receive DMA buffers and registers
 */

#define DBGINFO(fmt) if (debug_level >= DEBUG_LEVEL_INFO) printk fmt
#define DBGERR(fmt) if (debug_level >= DEBUG_LEVEL_ERROR) printk fmt
#define DBGBH(fmt) if (debug_level >= DEBUG_LEVEL_BH) printk fmt
#define DBGISR(fmt) if (debug_level >= DEBUG_LEVEL_ISR) printk fmt
#define DBGDATA(info, buf, size, label) if (debug_level >= DEBUG_LEVEL_DATA) trace_block((info), (buf), (size), (label))
//#define DBGTBUF(info) dump_tbufs(info)
//#define DBGRBUF(info) dump_rbufs(info)


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
#include <linux/termios.h>
#include <linux/bitops.h>
#include <linux/workqueue.h>
#include <linux/hdlc.h>
#include <linux/synclink.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/dma.h>
#include <asm/types.h>
#include <asm/uaccess.h>

#if defined(CONFIG_HDLC) || (defined(CONFIG_HDLC_MODULE) && defined(CONFIG_SYNCLINK_GT_MODULE))
#define SYNCLINK_GENERIC_HDLC 1
#else
#define SYNCLINK_GENERIC_HDLC 0
#endif

/*
 * module identification
 */
static char *driver_name     = "SyncLink GT";
static char *tty_driver_name = "synclink_gt";
static char *tty_dev_prefix  = "ttySLG";
MODULE_LICENSE("GPL");
#define MGSL_MAGIC 0x5401
#define MAX_DEVICES 32

static struct pci_device_id pci_table[] = {
	{PCI_VENDOR_ID_MICROGATE, SYNCLINK_GT_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID,},
	{PCI_VENDOR_ID_MICROGATE, SYNCLINK_GT2_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID,},
	{PCI_VENDOR_ID_MICROGATE, SYNCLINK_GT4_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID,},
	{PCI_VENDOR_ID_MICROGATE, SYNCLINK_AC_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID,},
	{0,}, /* terminate list */
};
MODULE_DEVICE_TABLE(pci, pci_table);

static int  init_one(struct pci_dev *dev,const struct pci_device_id *ent);
static void remove_one(struct pci_dev *dev);
static struct pci_driver pci_driver = {
	.name		= "synclink_gt",
	.id_table	= pci_table,
	.probe		= init_one,
	.remove		= __devexit_p(remove_one),
};

static bool pci_registered;

/*
 * module configuration and status
 */
static struct slgt_info *slgt_device_list;
static int slgt_device_count;

static int ttymajor;
static int debug_level;
static int maxframe[MAX_DEVICES];

module_param(ttymajor, int, 0);
module_param(debug_level, int, 0);
module_param_array(maxframe, int, NULL, 0);

MODULE_PARM_DESC(ttymajor, "TTY major device number override: 0=auto assigned");
MODULE_PARM_DESC(debug_level, "Debug syslog output: 0=disabled, 1 to 5=increasing detail");
MODULE_PARM_DESC(maxframe, "Maximum frame size used by device (4096 to 65535)");

/*
 * tty support and callbacks
 */
static struct tty_driver *serial_driver;

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

/*
 * generic HDLC support and callbacks
 */
#if SYNCLINK_GENERIC_HDLC
#define dev_to_port(D) (dev_to_hdlc(D)->priv)
static void hdlcdev_tx_done(struct slgt_info *info);
static void hdlcdev_rx(struct slgt_info *info, char *buf, int size);
static int  hdlcdev_init(struct slgt_info *info);
static void hdlcdev_exit(struct slgt_info *info);
#endif


/*
 * device specific structures, macros and functions
 */

#define SLGT_MAX_PORTS 4
#define SLGT_REG_SIZE  256

/*
 * conditional wait facility
 */
struct cond_wait {
	struct cond_wait *next;
	wait_queue_head_t q;
	wait_queue_t wait;
	unsigned int data;
};
static void init_cond_wait(struct cond_wait *w, unsigned int data);
static void add_cond_wait(struct cond_wait **head, struct cond_wait *w);
static void remove_cond_wait(struct cond_wait **head, struct cond_wait *w);
static void flush_cond_wait(struct cond_wait **head);

/*
 * DMA buffer descriptor and access macros
 */
struct slgt_desc
{
	__le16 count;
	__le16 status;
	__le32 pbuf;  /* physical address of data buffer */
	__le32 next;  /* physical address of next descriptor */

	/* driver book keeping */
	char *buf;          /* virtual  address of data buffer */
    	unsigned int pdesc; /* physical address of this descriptor */
	dma_addr_t buf_dma_addr;
	unsigned short buf_count;
};

#define set_desc_buffer(a,b) (a).pbuf = cpu_to_le32((unsigned int)(b))
#define set_desc_next(a,b) (a).next   = cpu_to_le32((unsigned int)(b))
#define set_desc_count(a,b)(a).count  = cpu_to_le16((unsigned short)(b))
#define set_desc_eof(a,b)  (a).status = cpu_to_le16((b) ? (le16_to_cpu((a).status) | BIT0) : (le16_to_cpu((a).status) & ~BIT0))
#define set_desc_status(a, b) (a).status = cpu_to_le16((unsigned short)(b))
#define desc_count(a)      (le16_to_cpu((a).count))
#define desc_status(a)     (le16_to_cpu((a).status))
#define desc_complete(a)   (le16_to_cpu((a).status) & BIT15)
#define desc_eof(a)        (le16_to_cpu((a).status) & BIT2)
#define desc_crc_error(a)  (le16_to_cpu((a).status) & BIT1)
#define desc_abort(a)      (le16_to_cpu((a).status) & BIT0)
#define desc_residue(a)    ((le16_to_cpu((a).status) & 0x38) >> 3)

struct _input_signal_events {
	int ri_up;
	int ri_down;
	int dsr_up;
	int dsr_down;
	int dcd_up;
	int dcd_down;
	int cts_up;
	int cts_down;
};

/*
 * device instance data structure
 */
struct slgt_info {
	void *if_ptr;		/* General purpose pointer (used by SPPP) */
	struct tty_port port;

	struct slgt_info *next_device;	/* device list link */

	int magic;

	char device_name[25];
	struct pci_dev *pdev;

	int port_count;  /* count of ports on adapter */
	int adapter_num; /* adapter instance number */
	int port_num;    /* port instance number */

	/* array of pointers to port contexts on this adapter */
	struct slgt_info *port_array[SLGT_MAX_PORTS];

	int			line;		/* tty line instance number */

	struct mgsl_icount	icount;

	int			timeout;
	int			x_char;		/* xon/xoff character */
	unsigned int		read_status_mask;
	unsigned int 		ignore_status_mask;

	wait_queue_head_t	status_event_wait_q;
	wait_queue_head_t	event_wait_q;
	struct timer_list	tx_timer;
	struct timer_list	rx_timer;

	unsigned int            gpio_present;
	struct cond_wait        *gpio_wait_q;

	spinlock_t lock;	/* spinlock for synchronizing with ISR */

	struct work_struct task;
	u32 pending_bh;
	bool bh_requested;
	bool bh_running;

	int isr_overflow;
	bool irq_requested;	/* true if IRQ requested */
	bool irq_occurred;	/* for diagnostics use */

	/* device configuration */

	unsigned int bus_type;
	unsigned int irq_level;
	unsigned long irq_flags;

	unsigned char __iomem * reg_addr;  /* memory mapped registers address */
	u32 phys_reg_addr;
	bool reg_addr_requested;

	MGSL_PARAMS params;       /* communications parameters */
	u32 idle_mode;
	u32 max_frame_size;       /* as set by device config */

	unsigned int rbuf_fill_level;
	unsigned int rx_pio;
	unsigned int if_mode;
	unsigned int base_clock;

	/* device status */

	bool rx_enabled;
	bool rx_restart;

	bool tx_enabled;
	bool tx_active;

	unsigned char signals;    /* serial signal states */
	int init_error;  /* initialization error */

	unsigned char *tx_buf;
	int tx_count;

	char flag_buf[MAX_ASYNC_BUFFER_SIZE];
	char char_buf[MAX_ASYNC_BUFFER_SIZE];
	bool drop_rts_on_tx_done;
	struct	_input_signal_events	input_signal_events;

	int dcd_chkcount;	/* check counts to prevent */
	int cts_chkcount;	/* too many IRQs if a signal */
	int dsr_chkcount;	/* is floating */
	int ri_chkcount;

	char *bufs;		/* virtual address of DMA buffer lists */
	dma_addr_t bufs_dma_addr; /* physical address of buffer descriptors */

	unsigned int rbuf_count;
	struct slgt_desc *rbufs;
	unsigned int rbuf_current;
	unsigned int rbuf_index;
	unsigned int rbuf_fill_index;
	unsigned short rbuf_fill_count;

	unsigned int tbuf_count;
	struct slgt_desc *tbufs;
	unsigned int tbuf_current;
	unsigned int tbuf_start;

	unsigned char *tmp_rbuf;
	unsigned int tmp_rbuf_count;

	/* SPPP/Cisco HDLC device parts */

	int netcount;
	spinlock_t netlock;
#if SYNCLINK_GENERIC_HDLC
	struct net_device *netdev;
#endif

};

static MGSL_PARAMS default_params = {
	.mode            = MGSL_MODE_HDLC,
	.loopback        = 0,
	.flags           = HDLC_FLAG_UNDERRUN_ABORT15,
	.encoding        = HDLC_ENCODING_NRZI_SPACE,
	.clock_speed     = 0,
	.addr_filter     = 0xff,
	.crc_type        = HDLC_CRC_16_CCITT,
	.preamble_length = HDLC_PREAMBLE_LENGTH_8BITS,
	.preamble        = HDLC_PREAMBLE_PATTERN_NONE,
	.data_rate       = 9600,
	.data_bits       = 8,
	.stop_bits       = 1,
	.parity          = ASYNC_PARITY_NONE
};


#define BH_RECEIVE  1
#define BH_TRANSMIT 2
#define BH_STATUS   4
#define IO_PIN_SHUTDOWN_LIMIT 100

#define DMABUFSIZE 256
#define DESC_LIST_SIZE 4096

#define MASK_PARITY  BIT1
#define MASK_FRAMING BIT0
#define MASK_BREAK   BIT14
#define MASK_OVERRUN BIT4

#define GSR   0x00 /* global status */
#define JCR   0x04 /* JTAG control */
#define IODR  0x08 /* GPIO direction */
#define IOER  0x0c /* GPIO interrupt enable */
#define IOVR  0x10 /* GPIO value */
#define IOSR  0x14 /* GPIO interrupt status */
#define TDR   0x80 /* tx data */
#define RDR   0x80 /* rx data */
#define TCR   0x82 /* tx control */
#define TIR   0x84 /* tx idle */
#define TPR   0x85 /* tx preamble */
#define RCR   0x86 /* rx control */
#define VCR   0x88 /* V.24 control */
#define CCR   0x89 /* clock control */
#define BDR   0x8a /* baud divisor */
#define SCR   0x8c /* serial control */
#define SSR   0x8e /* serial status */
#define RDCSR 0x90 /* rx DMA control/status */
#define TDCSR 0x94 /* tx DMA control/status */
#define RDDAR 0x98 /* rx DMA descriptor address */
#define TDDAR 0x9c /* tx DMA descriptor address */

#define RXIDLE      BIT14
#define RXBREAK     BIT14
#define IRQ_TXDATA  BIT13
#define IRQ_TXIDLE  BIT12
#define IRQ_TXUNDER BIT11 /* HDLC */
#define IRQ_RXDATA  BIT10
#define IRQ_RXIDLE  BIT9  /* HDLC */
#define IRQ_RXBREAK BIT9  /* async */
#define IRQ_RXOVER  BIT8
#define IRQ_DSR     BIT7
#define IRQ_CTS     BIT6
#define IRQ_DCD     BIT5
#define IRQ_RI      BIT4
#define IRQ_ALL     0x3ff0
#define IRQ_MASTER  BIT0

#define slgt_irq_on(info, mask) \
	wr_reg16((info), SCR, (unsigned short)(rd_reg16((info), SCR) | (mask)))
#define slgt_irq_off(info, mask) \
	wr_reg16((info), SCR, (unsigned short)(rd_reg16((info), SCR) & ~(mask)))

static __u8  rd_reg8(struct slgt_info *info, unsigned int addr);
static void  wr_reg8(struct slgt_info *info, unsigned int addr, __u8 value);
static __u16 rd_reg16(struct slgt_info *info, unsigned int addr);
static void  wr_reg16(struct slgt_info *info, unsigned int addr, __u16 value);
static __u32 rd_reg32(struct slgt_info *info, unsigned int addr);
static void  wr_reg32(struct slgt_info *info, unsigned int addr, __u32 value);

static void  msc_set_vcr(struct slgt_info *info);

static int  startup(struct slgt_info *info);
static int  block_til_ready(struct tty_struct *tty, struct file * filp,struct slgt_info *info);
static void shutdown(struct slgt_info *info);
static void program_hw(struct slgt_info *info);
static void change_params(struct slgt_info *info);

static int  register_test(struct slgt_info *info);
static int  irq_test(struct slgt_info *info);
static int  loopback_test(struct slgt_info *info);
static int  adapter_test(struct slgt_info *info);

static void reset_adapter(struct slgt_info *info);
static void reset_port(struct slgt_info *info);
static void async_mode(struct slgt_info *info);
static void sync_mode(struct slgt_info *info);

static void rx_stop(struct slgt_info *info);
static void rx_start(struct slgt_info *info);
static void reset_rbufs(struct slgt_info *info);
static void free_rbufs(struct slgt_info *info, unsigned int first, unsigned int last);
static void rdma_reset(struct slgt_info *info);
static bool rx_get_frame(struct slgt_info *info);
static bool rx_get_buf(struct slgt_info *info);

static void tx_start(struct slgt_info *info);
static void tx_stop(struct slgt_info *info);
static void tx_set_idle(struct slgt_info *info);
static unsigned int free_tbuf_count(struct slgt_info *info);
static unsigned int tbuf_bytes(struct slgt_info *info);
static void reset_tbufs(struct slgt_info *info);
static void tdma_reset(struct slgt_info *info);
static void tx_load(struct slgt_info *info, const char *buf, unsigned int count);

static void get_signals(struct slgt_info *info);
static void set_signals(struct slgt_info *info);
static void enable_loopback(struct slgt_info *info);
static void set_rate(struct slgt_info *info, u32 data_rate);

static int  bh_action(struct slgt_info *info);
static void bh_handler(struct work_struct *work);
static void bh_transmit(struct slgt_info *info);
static void isr_serial(struct slgt_info *info);
static void isr_rdma(struct slgt_info *info);
static void isr_txeom(struct slgt_info *info, unsigned short status);
static void isr_tdma(struct slgt_info *info);

static int  alloc_dma_bufs(struct slgt_info *info);
static void free_dma_bufs(struct slgt_info *info);
static int  alloc_desc(struct slgt_info *info);
static void free_desc(struct slgt_info *info);
static int  alloc_bufs(struct slgt_info *info, struct slgt_desc *bufs, int count);
static void free_bufs(struct slgt_info *info, struct slgt_desc *bufs, int count);

static int  alloc_tmp_rbuf(struct slgt_info *info);
static void free_tmp_rbuf(struct slgt_info *info);

static void tx_timeout(unsigned long context);
static void rx_timeout(unsigned long context);

/*
 * ioctl handlers
 */
static int  get_stats(struct slgt_info *info, struct mgsl_icount __user *user_icount);
static int  get_params(struct slgt_info *info, MGSL_PARAMS __user *params);
static int  set_params(struct slgt_info *info, MGSL_PARAMS __user *params);
static int  get_txidle(struct slgt_info *info, int __user *idle_mode);
static int  set_txidle(struct slgt_info *info, int idle_mode);
static int  tx_enable(struct slgt_info *info, int enable);
static int  tx_abort(struct slgt_info *info);
static int  rx_enable(struct slgt_info *info, int enable);
static int  modem_input_wait(struct slgt_info *info,int arg);
static int  wait_mgsl_event(struct slgt_info *info, int __user *mask_ptr);
static int  tiocmget(struct tty_struct *tty, struct file *file);
static int  tiocmset(struct tty_struct *tty, struct file *file,
		     unsigned int set, unsigned int clear);
static int set_break(struct tty_struct *tty, int break_state);
static int  get_interface(struct slgt_info *info, int __user *if_mode);
static int  set_interface(struct slgt_info *info, int if_mode);
static int  set_gpio(struct slgt_info *info, struct gpio_desc __user *gpio);
static int  get_gpio(struct slgt_info *info, struct gpio_desc __user *gpio);
static int  wait_gpio(struct slgt_info *info, struct gpio_desc __user *gpio);

/*
 * driver functions
 */
static void add_device(struct slgt_info *info);
static void device_init(int adapter_num, struct pci_dev *pdev);
static int  claim_resources(struct slgt_info *info);
static void release_resources(struct slgt_info *info);

/*
 * DEBUG OUTPUT CODE
 */
#ifndef DBGINFO
#define DBGINFO(fmt)
#endif
#ifndef DBGERR
#define DBGERR(fmt)
#endif
#ifndef DBGBH
#define DBGBH(fmt)
#endif
#ifndef DBGISR
#define DBGISR(fmt)
#endif

#ifdef DBGDATA
static void trace_block(struct slgt_info *info, const char *data, int count, const char *label)
{
	int i;
	int linecount;
	printk("%s %s data:\n",info->device_name, label);
	while(count) {
		linecount = (count > 16) ? 16 : count;
		for(i=0; i < linecount; i++)
			printk("%02X ",(unsigned char)data[i]);
		for(;i<17;i++)
			printk("   ");
		for(i=0;i<linecount;i++) {
			if (data[i]>=040 && data[i]<=0176)
				printk("%c",data[i]);
			else
				printk(".");
		}
		printk("\n");
		data  += linecount;
		count -= linecount;
	}
}
#else
#define DBGDATA(info, buf, size, label)
#endif

#ifdef DBGTBUF
static void dump_tbufs(struct slgt_info *info)
{
	int i;
	printk("tbuf_current=%d\n", info->tbuf_current);
	for (i=0 ; i < info->tbuf_count ; i++) {
		printk("%d: count=%04X status=%04X\n",
			i, le16_to_cpu(info->tbufs[i].count), le16_to_cpu(info->tbufs[i].status));
	}
}
#else
#define DBGTBUF(info)
#endif

#ifdef DBGRBUF
static void dump_rbufs(struct slgt_info *info)
{
	int i;
	printk("rbuf_current=%d\n", info->rbuf_current);
	for (i=0 ; i < info->rbuf_count ; i++) {
		printk("%d: count=%04X status=%04X\n",
			i, le16_to_cpu(info->rbufs[i].count), le16_to_cpu(info->rbufs[i].status));
	}
}
#else
#define DBGRBUF(info)
#endif

static inline int sanity_check(struct slgt_info *info, char *devname, const char *name)
{
#ifdef SANITY_CHECK
	if (!info) {
		printk("null struct slgt_info for (%s) in %s\n", devname, name);
		return 1;
	}
	if (info->magic != MGSL_MAGIC) {
		printk("bad magic number struct slgt_info (%s) in %s\n", devname, name);
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

static int open(struct tty_struct *tty, struct file *filp)
{
	struct slgt_info *info;
	int retval, line;
	unsigned long flags;

	line = tty->index;
	if ((line < 0) || (line >= slgt_device_count)) {
		DBGERR(("%s: open with invalid line #%d.\n", driver_name, line));
		return -ENODEV;
	}

	info = slgt_device_list;
	while(info && info->line != line)
		info = info->next_device;
	if (sanity_check(info, tty->name, "open"))
		return -ENODEV;
	if (info->init_error) {
		DBGERR(("%s init error=%d\n", info->device_name, info->init_error));
		return -ENODEV;
	}

	tty->driver_data = info;
	info->port.tty = tty;

	DBGINFO(("%s open, old ref count = %d\n", info->device_name, info->port.count));

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
		DBGINFO(("%s block_til_ready rc=%d\n", info->device_name, retval));
		goto cleanup;
	}

	retval = 0;

cleanup:
	if (retval) {
		if (tty->count == 1)
			info->port.tty = NULL; /* tty layer will release tty struct */
		if(info->port.count)
			info->port.count--;
	}

	DBGINFO(("%s open rc=%d\n", info->device_name, retval));
	return retval;
}

static void close(struct tty_struct *tty, struct file *filp)
{
	struct slgt_info *info = tty->driver_data;

	if (sanity_check(info, tty->name, "close"))
		return;
	DBGINFO(("%s close entry, count=%d\n", info->device_name, info->port.count));

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
	DBGINFO(("%s close exit, count=%d\n", tty->driver->name, info->port.count));
}

static void hangup(struct tty_struct *tty)
{
	struct slgt_info *info = tty->driver_data;

	if (sanity_check(info, tty->name, "hangup"))
		return;
	DBGINFO(("%s hangup\n", info->device_name));

	flush_buffer(tty);
	shutdown(info);

	info->port.count = 0;
	info->port.flags &= ~ASYNC_NORMAL_ACTIVE;
	info->port.tty = NULL;

	wake_up_interruptible(&info->port.open_wait);
}

static void set_termios(struct tty_struct *tty, struct ktermios *old_termios)
{
	struct slgt_info *info = tty->driver_data;
	unsigned long flags;

	DBGINFO(("%s set_termios\n", tty->driver->name));

	change_params(info);

	/* Handle transition to B0 status */
	if (old_termios->c_cflag & CBAUD &&
	    !(tty->termios->c_cflag & CBAUD)) {
		info->signals &= ~(SerialSignal_RTS + SerialSignal_DTR);
		spin_lock_irqsave(&info->lock,flags);
		set_signals(info);
		spin_unlock_irqrestore(&info->lock,flags);
	}

	/* Handle transition away from B0 status */
	if (!(old_termios->c_cflag & CBAUD) &&
	    tty->termios->c_cflag & CBAUD) {
		info->signals |= SerialSignal_DTR;
 		if (!(tty->termios->c_cflag & CRTSCTS) ||
 		    !test_bit(TTY_THROTTLED, &tty->flags)) {
			info->signals |= SerialSignal_RTS;
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

static void update_tx_timer(struct slgt_info *info)
{
	/*
	 * use worst case speed of 1200bps to calculate transmit timeout
	 * based on data in buffers (tbuf_bytes) and FIFO (128 bytes)
	 */
	if (info->params.mode == MGSL_MODE_HDLC) {
		int timeout  = (tbuf_bytes(info) * 7) + 1000;
		mod_timer(&info->tx_timer, jiffies + msecs_to_jiffies(timeout));
	}
}

static int write(struct tty_struct *tty,
		 const unsigned char *buf, int count)
{
	int ret = 0;
	struct slgt_info *info = tty->driver_data;
	unsigned long flags;
	unsigned int bufs_needed;

	if (sanity_check(info, tty->name, "write"))
		goto cleanup;
	DBGINFO(("%s write count=%d\n", info->device_name, count));

	if (!info->tx_buf)
		goto cleanup;

	if (count > info->max_frame_size) {
		ret = -EIO;
		goto cleanup;
	}

	if (!count)
		goto cleanup;

	if (!info->tx_active && info->tx_count) {
		/* send accumulated data from send_char() */
		tx_load(info, info->tx_buf, info->tx_count);
		goto start;
	}
	bufs_needed = (count/DMABUFSIZE);
	if (count % DMABUFSIZE)
		++bufs_needed;
	if (bufs_needed > free_tbuf_count(info))
		goto cleanup;

	ret = info->tx_count = count;
	tx_load(info, buf, count);
	goto start;

start:
 	if (info->tx_count && !tty->stopped && !tty->hw_stopped) {
		spin_lock_irqsave(&info->lock,flags);
		if (!info->tx_active)
		 	tx_start(info);
		else if (!(rd_reg32(info, TDCSR) & BIT0)) {
			/* transmit still active but transmit DMA stopped */
			unsigned int i = info->tbuf_current;
			if (!i)
				i = info->tbuf_count;
			i--;
			/* if DMA buf unsent must try later after tx idle */
			if (desc_count(info->tbufs[i]))
				ret = 0;
		}
		if (ret > 0)
			update_tx_timer(info);
		spin_unlock_irqrestore(&info->lock,flags);
 	}

cleanup:
	DBGINFO(("%s write rc=%d\n", info->device_name, ret));
	return ret;
}

static int put_char(struct tty_struct *tty, unsigned char ch)
{
	struct slgt_info *info = tty->driver_data;
	unsigned long flags;
	int ret = 0;

	if (sanity_check(info, tty->name, "put_char"))
		return 0;
	DBGINFO(("%s put_char(%d)\n", info->device_name, ch));
	if (!info->tx_buf)
		return 0;
	spin_lock_irqsave(&info->lock,flags);
	if (!info->tx_active && (info->tx_count < info->max_frame_size)) {
		info->tx_buf[info->tx_count++] = ch;
		ret = 1;
	}
	spin_unlock_irqrestore(&info->lock,flags);
	return ret;
}

static void send_xchar(struct tty_struct *tty, char ch)
{
	struct slgt_info *info = tty->driver_data;
	unsigned long flags;

	if (sanity_check(info, tty->name, "send_xchar"))
		return;
	DBGINFO(("%s send_xchar(%d)\n", info->device_name, ch));
	info->x_char = ch;
	if (ch) {
		spin_lock_irqsave(&info->lock,flags);
		if (!info->tx_enabled)
		 	tx_start(info);
		spin_unlock_irqrestore(&info->lock,flags);
	}
}

static void wait_until_sent(struct tty_struct *tty, int timeout)
{
	struct slgt_info *info = tty->driver_data;
	unsigned long orig_jiffies, char_time;

	if (!info )
		return;
	if (sanity_check(info, tty->name, "wait_until_sent"))
		return;
	DBGINFO(("%s wait_until_sent entry\n", info->device_name));
	if (!(info->port.flags & ASYNC_INITIALIZED))
		goto exit;

	orig_jiffies = jiffies;

	/* Set check interval to 1/5 of estimated time to
	 * send a character, and make it at least 1. The check
	 * interval should also be less than the timeout.
	 * Note: use tight timings here to satisfy the NIST-PCTS.
	 */

	lock_kernel();

	if (info->params.data_rate) {
	       	char_time = info->timeout/(32 * 5);
		if (!char_time)
			char_time++;
	} else
		char_time = 1;

	if (timeout)
		char_time = min_t(unsigned long, char_time, timeout);

	while (info->tx_active) {
		msleep_interruptible(jiffies_to_msecs(char_time));
		if (signal_pending(current))
			break;
		if (timeout && time_after(jiffies, orig_jiffies + timeout))
			break;
	}
	unlock_kernel();

exit:
	DBGINFO(("%s wait_until_sent exit\n", info->device_name));
}

static int write_room(struct tty_struct *tty)
{
	struct slgt_info *info = tty->driver_data;
	int ret;

	if (sanity_check(info, tty->name, "write_room"))
		return 0;
	ret = (info->tx_active) ? 0 : HDLC_MAX_FRAME_SIZE;
	DBGINFO(("%s write_room=%d\n", info->device_name, ret));
	return ret;
}

static void flush_chars(struct tty_struct *tty)
{
	struct slgt_info *info = tty->driver_data;
	unsigned long flags;

	if (sanity_check(info, tty->name, "flush_chars"))
		return;
	DBGINFO(("%s flush_chars entry tx_count=%d\n", info->device_name, info->tx_count));

	if (info->tx_count <= 0 || tty->stopped ||
	    tty->hw_stopped || !info->tx_buf)
		return;

	DBGINFO(("%s flush_chars start transmit\n", info->device_name));

	spin_lock_irqsave(&info->lock,flags);
	if (!info->tx_active && info->tx_count) {
		tx_load(info, info->tx_buf,info->tx_count);
	 	tx_start(info);
	}
	spin_unlock_irqrestore(&info->lock,flags);
}

static void flush_buffer(struct tty_struct *tty)
{
	struct slgt_info *info = tty->driver_data;
	unsigned long flags;

	if (sanity_check(info, tty->name, "flush_buffer"))
		return;
	DBGINFO(("%s flush_buffer\n", info->device_name));

	spin_lock_irqsave(&info->lock,flags);
	if (!info->tx_active)
		info->tx_count = 0;
	spin_unlock_irqrestore(&info->lock,flags);

	tty_wakeup(tty);
}

/*
 * throttle (stop) transmitter
 */
static void tx_hold(struct tty_struct *tty)
{
	struct slgt_info *info = tty->driver_data;
	unsigned long flags;

	if (sanity_check(info, tty->name, "tx_hold"))
		return;
	DBGINFO(("%s tx_hold\n", info->device_name));
	spin_lock_irqsave(&info->lock,flags);
	if (info->tx_enabled && info->params.mode == MGSL_MODE_ASYNC)
	 	tx_stop(info);
	spin_unlock_irqrestore(&info->lock,flags);
}

/*
 * release (start) transmitter
 */
static void tx_release(struct tty_struct *tty)
{
	struct slgt_info *info = tty->driver_data;
	unsigned long flags;

	if (sanity_check(info, tty->name, "tx_release"))
		return;
	DBGINFO(("%s tx_release\n", info->device_name));
	spin_lock_irqsave(&info->lock,flags);
	if (!info->tx_active && info->tx_count) {
		tx_load(info, info->tx_buf, info->tx_count);
	 	tx_start(info);
	}
	spin_unlock_irqrestore(&info->lock,flags);
}

/*
 * Service an IOCTL request
 *
 * Arguments
 *
 * 	tty	pointer to tty instance data
 * 	file	pointer to associated file object for device
 * 	cmd	IOCTL command code
 * 	arg	command argument/context
 *
 * Return 0 if success, otherwise error code
 */
static int ioctl(struct tty_struct *tty, struct file *file,
		 unsigned int cmd, unsigned long arg)
{
	struct slgt_info *info = tty->driver_data;
	struct mgsl_icount cnow;	/* kernel counter temps */
	struct serial_icounter_struct __user *p_cuser;	/* user space */
	unsigned long flags;
	void __user *argp = (void __user *)arg;
	int ret;

	if (sanity_check(info, tty->name, "ioctl"))
		return -ENODEV;
	DBGINFO(("%s ioctl() cmd=%08X\n", info->device_name, cmd));

	if ((cmd != TIOCGSERIAL) && (cmd != TIOCSSERIAL) &&
	    (cmd != TIOCMIWAIT) && (cmd != TIOCGICOUNT)) {
		if (tty->flags & (1 << TTY_IO_ERROR))
		    return -EIO;
	}

	lock_kernel();

	switch (cmd) {
	case MGSL_IOCGPARAMS:
		ret = get_params(info, argp);
		break;
	case MGSL_IOCSPARAMS:
		ret = set_params(info, argp);
		break;
	case MGSL_IOCGTXIDLE:
		ret = get_txidle(info, argp);
		break;
	case MGSL_IOCSTXIDLE:
		ret = set_txidle(info, (int)arg);
		break;
	case MGSL_IOCTXENABLE:
		ret = tx_enable(info, (int)arg);
		break;
	case MGSL_IOCRXENABLE:
		ret = rx_enable(info, (int)arg);
		break;
	case MGSL_IOCTXABORT:
		ret = tx_abort(info);
		break;
	case MGSL_IOCGSTATS:
		ret = get_stats(info, argp);
		break;
	case MGSL_IOCWAITEVENT:
		ret = wait_mgsl_event(info, argp);
		break;
	case TIOCMIWAIT:
		ret = modem_input_wait(info,(int)arg);
		break;
	case MGSL_IOCGIF:
		ret = get_interface(info, argp);
		break;
	case MGSL_IOCSIF:
		ret = set_interface(info,(int)arg);
		break;
	case MGSL_IOCSGPIO:
		ret = set_gpio(info, argp);
		break;
	case MGSL_IOCGGPIO:
		ret = get_gpio(info, argp);
		break;
	case MGSL_IOCWAITGPIO:
		ret = wait_gpio(info, argp);
		break;
	case TIOCGICOUNT:
		spin_lock_irqsave(&info->lock,flags);
		cnow = info->icount;
		spin_unlock_irqrestore(&info->lock,flags);
		p_cuser = argp;
		if (put_user(cnow.cts, &p_cuser->cts) ||
		    put_user(cnow.dsr, &p_cuser->dsr) ||
		    put_user(cnow.rng, &p_cuser->rng) ||
		    put_user(cnow.dcd, &p_cuser->dcd) ||
		    put_user(cnow.rx, &p_cuser->rx) ||
		    put_user(cnow.tx, &p_cuser->tx) ||
		    put_user(cnow.frame, &p_cuser->frame) ||
		    put_user(cnow.overrun, &p_cuser->overrun) ||
		    put_user(cnow.parity, &p_cuser->parity) ||
		    put_user(cnow.brk, &p_cuser->brk) ||
		    put_user(cnow.buf_overrun, &p_cuser->buf_overrun))
			ret = -EFAULT;
		ret = 0;
		break;
	default:
		ret = -ENOIOCTLCMD;
	}
	unlock_kernel();
	return ret;
}

/*
 * support for 32 bit ioctl calls on 64 bit systems
 */
#ifdef CONFIG_COMPAT
static long get_params32(struct slgt_info *info, struct MGSL_PARAMS32 __user *user_params)
{
	struct MGSL_PARAMS32 tmp_params;

	DBGINFO(("%s get_params32\n", info->device_name));
	tmp_params.mode            = (compat_ulong_t)info->params.mode;
	tmp_params.loopback        = info->params.loopback;
	tmp_params.flags           = info->params.flags;
	tmp_params.encoding        = info->params.encoding;
	tmp_params.clock_speed     = (compat_ulong_t)info->params.clock_speed;
	tmp_params.addr_filter     = info->params.addr_filter;
	tmp_params.crc_type        = info->params.crc_type;
	tmp_params.preamble_length = info->params.preamble_length;
	tmp_params.preamble        = info->params.preamble;
	tmp_params.data_rate       = (compat_ulong_t)info->params.data_rate;
	tmp_params.data_bits       = info->params.data_bits;
	tmp_params.stop_bits       = info->params.stop_bits;
	tmp_params.parity          = info->params.parity;
	if (copy_to_user(user_params, &tmp_params, sizeof(struct MGSL_PARAMS32)))
		return -EFAULT;
	return 0;
}

static long set_params32(struct slgt_info *info, struct MGSL_PARAMS32 __user *new_params)
{
	struct MGSL_PARAMS32 tmp_params;

	DBGINFO(("%s set_params32\n", info->device_name));
	if (copy_from_user(&tmp_params, new_params, sizeof(struct MGSL_PARAMS32)))
		return -EFAULT;

	spin_lock(&info->lock);
	if (tmp_params.mode == MGSL_MODE_BASE_CLOCK) {
		info->base_clock = tmp_params.clock_speed;
	} else {
		info->params.mode            = tmp_params.mode;
		info->params.loopback        = tmp_params.loopback;
		info->params.flags           = tmp_params.flags;
		info->params.encoding        = tmp_params.encoding;
		info->params.clock_speed     = tmp_params.clock_speed;
		info->params.addr_filter     = tmp_params.addr_filter;
		info->params.crc_type        = tmp_params.crc_type;
		info->params.preamble_length = tmp_params.preamble_length;
		info->params.preamble        = tmp_params.preamble;
		info->params.data_rate       = tmp_params.data_rate;
		info->params.data_bits       = tmp_params.data_bits;
		info->params.stop_bits       = tmp_params.stop_bits;
		info->params.parity          = tmp_params.parity;
	}
	spin_unlock(&info->lock);

	program_hw(info);

	return 0;
}

static long slgt_compat_ioctl(struct tty_struct *tty, struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	struct slgt_info *info = tty->driver_data;
	int rc = -ENOIOCTLCMD;

	if (sanity_check(info, tty->name, "compat_ioctl"))
		return -ENODEV;
	DBGINFO(("%s compat_ioctl() cmd=%08X\n", info->device_name, cmd));

	switch (cmd) {

	case MGSL_IOCSPARAMS32:
		rc = set_params32(info, compat_ptr(arg));
		break;

	case MGSL_IOCGPARAMS32:
		rc = get_params32(info, compat_ptr(arg));
		break;

	case MGSL_IOCGPARAMS:
	case MGSL_IOCSPARAMS:
	case MGSL_IOCGTXIDLE:
	case MGSL_IOCGSTATS:
	case MGSL_IOCWAITEVENT:
	case MGSL_IOCGIF:
	case MGSL_IOCSGPIO:
	case MGSL_IOCGGPIO:
	case MGSL_IOCWAITGPIO:
	case TIOCGICOUNT:
		rc = ioctl(tty, file, cmd, (unsigned long)(compat_ptr(arg)));
		break;

	case MGSL_IOCSTXIDLE:
	case MGSL_IOCTXENABLE:
	case MGSL_IOCRXENABLE:
	case MGSL_IOCTXABORT:
	case TIOCMIWAIT:
	case MGSL_IOCSIF:
		rc = ioctl(tty, file, cmd, arg);
		break;
	}

	DBGINFO(("%s compat_ioctl() cmd=%08X rc=%d\n", info->device_name, cmd, rc));
	return rc;
}
#else
#define slgt_compat_ioctl NULL
#endif /* ifdef CONFIG_COMPAT */

/*
 * proc fs support
 */
static inline void line_info(struct seq_file *m, struct slgt_info *info)
{
	char stat_buf[30];
	unsigned long flags;

	seq_printf(m, "%s: IO=%08X IRQ=%d MaxFrameSize=%u\n",
		      info->device_name, info->phys_reg_addr,
		      info->irq_level, info->max_frame_size);

	/* output current serial signal states */
	spin_lock_irqsave(&info->lock,flags);
	get_signals(info);
	spin_unlock_irqrestore(&info->lock,flags);

	stat_buf[0] = 0;
	stat_buf[1] = 0;
	if (info->signals & SerialSignal_RTS)
		strcat(stat_buf, "|RTS");
	if (info->signals & SerialSignal_CTS)
		strcat(stat_buf, "|CTS");
	if (info->signals & SerialSignal_DTR)
		strcat(stat_buf, "|DTR");
	if (info->signals & SerialSignal_DSR)
		strcat(stat_buf, "|DSR");
	if (info->signals & SerialSignal_DCD)
		strcat(stat_buf, "|CD");
	if (info->signals & SerialSignal_RI)
		strcat(stat_buf, "|RI");

	if (info->params.mode != MGSL_MODE_ASYNC) {
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
			seq_printf(m, " rxcrc:%d", info->icount.rxcrc);
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
static int synclink_gt_proc_show(struct seq_file *m, void *v)
{
	struct slgt_info *info;

	seq_puts(m, "synclink_gt driver\n");

	info = slgt_device_list;
	while( info ) {
		line_info(m, info);
		info = info->next_device;
	}
	return 0;
}

static int synclink_gt_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, synclink_gt_proc_show, NULL);
}

static const struct file_operations synclink_gt_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= synclink_gt_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/*
 * return count of bytes in transmit buffer
 */
static int chars_in_buffer(struct tty_struct *tty)
{
	struct slgt_info *info = tty->driver_data;
	int count;
	if (sanity_check(info, tty->name, "chars_in_buffer"))
		return 0;
	count = tbuf_bytes(info);
	DBGINFO(("%s chars_in_buffer()=%d\n", info->device_name, count));
	return count;
}

/*
 * signal remote device to throttle send data (our receive data)
 */
static void throttle(struct tty_struct * tty)
{
	struct slgt_info *info = tty->driver_data;
	unsigned long flags;

	if (sanity_check(info, tty->name, "throttle"))
		return;
	DBGINFO(("%s throttle\n", info->device_name));
	if (I_IXOFF(tty))
		send_xchar(tty, STOP_CHAR(tty));
 	if (tty->termios->c_cflag & CRTSCTS) {
		spin_lock_irqsave(&info->lock,flags);
		info->signals &= ~SerialSignal_RTS;
	 	set_signals(info);
		spin_unlock_irqrestore(&info->lock,flags);
	}
}

/*
 * signal remote device to stop throttling send data (our receive data)
 */
static void unthrottle(struct tty_struct * tty)
{
	struct slgt_info *info = tty->driver_data;
	unsigned long flags;

	if (sanity_check(info, tty->name, "unthrottle"))
		return;
	DBGINFO(("%s unthrottle\n", info->device_name));
	if (I_IXOFF(tty)) {
		if (info->x_char)
			info->x_char = 0;
		else
			send_xchar(tty, START_CHAR(tty));
	}
 	if (tty->termios->c_cflag & CRTSCTS) {
		spin_lock_irqsave(&info->lock,flags);
		info->signals |= SerialSignal_RTS;
	 	set_signals(info);
		spin_unlock_irqrestore(&info->lock,flags);
	}
}

/*
 * set or clear transmit break condition
 * break_state	-1=set break condition, 0=clear
 */
static int set_break(struct tty_struct *tty, int break_state)
{
	struct slgt_info *info = tty->driver_data;
	unsigned short value;
	unsigned long flags;

	if (sanity_check(info, tty->name, "set_break"))
		return -EINVAL;
	DBGINFO(("%s set_break(%d)\n", info->device_name, break_state));

	spin_lock_irqsave(&info->lock,flags);
	value = rd_reg16(info, TCR);
 	if (break_state == -1)
		value |= BIT6;
	else
		value &= ~BIT6;
	wr_reg16(info, TCR, value);
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
	struct slgt_info *info = dev_to_port(dev);
	unsigned char  new_encoding;
	unsigned short new_crctype;

	/* return error if TTY interface open */
	if (info->port.count)
		return -EBUSY;

	DBGINFO(("%s hdlcdev_attach\n", info->device_name));

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
	struct slgt_info *info = dev_to_port(dev);
	unsigned long flags;

	DBGINFO(("%s hdlc_xmit\n", dev->name));

	/* stop sending until this frame completes */
	netif_stop_queue(dev);

	/* copy data to device buffers */
	info->tx_count = skb->len;
	tx_load(info, skb->data, skb->len);

	/* update network statistics */
	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;

	/* done with socket buffer, so free it */
	dev_kfree_skb(skb);

	/* save start time for transmit timeout detection */
	dev->trans_start = jiffies;

	spin_lock_irqsave(&info->lock,flags);
	tx_start(info);
	update_tx_timer(info);
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
	struct slgt_info *info = dev_to_port(dev);
	int rc;
	unsigned long flags;

	if (!try_module_get(THIS_MODULE))
		return -EBUSY;

	DBGINFO(("%s hdlcdev_open\n", dev->name));

	/* generic HDLC layer open processing */
	if ((rc = hdlc_open(dev)))
		return rc;

	/* arbitrate between network and tty opens */
	spin_lock_irqsave(&info->netlock, flags);
	if (info->port.count != 0 || info->netcount != 0) {
		DBGINFO(("%s hdlc_open busy\n", dev->name));
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
	info->signals |= SerialSignal_RTS + SerialSignal_DTR;
	program_hw(info);

	/* enable network layer transmit */
	dev->trans_start = jiffies;
	netif_start_queue(dev);

	/* inform generic HDLC layer of current DCD status */
	spin_lock_irqsave(&info->lock, flags);
	get_signals(info);
	spin_unlock_irqrestore(&info->lock, flags);
	if (info->signals & SerialSignal_DCD)
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
	struct slgt_info *info = dev_to_port(dev);
	unsigned long flags;

	DBGINFO(("%s hdlcdev_close\n", dev->name));

	netif_stop_queue(dev);

	/* shutdown adapter and release resources */
	shutdown(info);

	hdlc_close(dev);

	spin_lock_irqsave(&info->netlock, flags);
	info->netcount=0;
	spin_unlock_irqrestore(&info->netlock, flags);

	module_put(THIS_MODULE);
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
	struct slgt_info *info = dev_to_port(dev);
	unsigned int flags;

	DBGINFO(("%s hdlcdev_ioctl\n", dev->name));

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
	struct slgt_info *info = dev_to_port(dev);
	unsigned long flags;

	DBGINFO(("%s hdlcdev_tx_timeout\n", dev->name));

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
static void hdlcdev_tx_done(struct slgt_info *info)
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
static void hdlcdev_rx(struct slgt_info *info, char *buf, int size)
{
	struct sk_buff *skb = dev_alloc_skb(size);
	struct net_device *dev = info->netdev;

	DBGINFO(("%s hdlcdev_rx\n", dev->name));

	if (skb == NULL) {
		DBGERR(("%s: can't alloc skb, drop packet\n", dev->name));
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
static int hdlcdev_init(struct slgt_info *info)
{
	int rc;
	struct net_device *dev;
	hdlc_device *hdlc;

	/* allocate and initialize network and HDLC layer objects */

	if (!(dev = alloc_hdlcdev(info))) {
		printk(KERN_ERR "%s hdlc device alloc failure\n", info->device_name);
		return -ENOMEM;
	}

	/* for network layer reporting purposes only */
	dev->mem_start = info->phys_reg_addr;
	dev->mem_end   = info->phys_reg_addr + SLGT_REG_SIZE - 1;
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
static void hdlcdev_exit(struct slgt_info *info)
{
	unregister_hdlc_device(info->netdev);
	free_netdev(info->netdev);
	info->netdev = NULL;
}

#endif /* ifdef CONFIG_HDLC */

/*
 * get async data from rx DMA buffers
 */
static void rx_async(struct slgt_info *info)
{
 	struct tty_struct *tty = info->port.tty;
 	struct mgsl_icount *icount = &info->icount;
	unsigned int start, end;
	unsigned char *p;
	unsigned char status;
	struct slgt_desc *bufs = info->rbufs;
	int i, count;
	int chars = 0;
	int stat;
	unsigned char ch;

	start = end = info->rbuf_current;

	while(desc_complete(bufs[end])) {
		count = desc_count(bufs[end]) - info->rbuf_index;
		p     = bufs[end].buf + info->rbuf_index;

		DBGISR(("%s rx_async count=%d\n", info->device_name, count));
		DBGDATA(info, p, count, "rx");

		for(i=0 ; i < count; i+=2, p+=2) {
			ch = *p;
			icount->rx++;

			stat = 0;

			if ((status = *(p+1) & (BIT1 + BIT0))) {
				if (status & BIT1)
					icount->parity++;
				else if (status & BIT0)
					icount->frame++;
				/* discard char if tty control flags say so */
				if (status & info->ignore_status_mask)
					continue;
				if (status & BIT1)
					stat = TTY_PARITY;
				else if (status & BIT0)
					stat = TTY_FRAME;
			}
			if (tty) {
				tty_insert_flip_char(tty, ch, stat);
				chars++;
			}
		}

		if (i < count) {
			/* receive buffer not completed */
			info->rbuf_index += i;
			mod_timer(&info->rx_timer, jiffies + 1);
			break;
		}

		info->rbuf_index = 0;
		free_rbufs(info, end, end);

		if (++end == info->rbuf_count)
			end = 0;

		/* if entire list searched then no frame available */
		if (end == start)
			break;
	}

	if (tty && chars)
		tty_flip_buffer_push(tty);
}

/*
 * return next bottom half action to perform
 */
static int bh_action(struct slgt_info *info)
{
	unsigned long flags;
	int rc;

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
	} else {
		/* Mark BH routine as complete */
		info->bh_running = false;
		info->bh_requested = false;
		rc = 0;
	}

	spin_unlock_irqrestore(&info->lock,flags);

	return rc;
}

/*
 * perform bottom half processing
 */
static void bh_handler(struct work_struct *work)
{
	struct slgt_info *info = container_of(work, struct slgt_info, task);
	int action;

	if (!info)
		return;
	info->bh_running = true;

	while((action = bh_action(info))) {
		switch (action) {
		case BH_RECEIVE:
			DBGBH(("%s bh receive\n", info->device_name));
			switch(info->params.mode) {
			case MGSL_MODE_ASYNC:
				rx_async(info);
				break;
			case MGSL_MODE_HDLC:
				while(rx_get_frame(info));
				break;
			case MGSL_MODE_RAW:
			case MGSL_MODE_MONOSYNC:
			case MGSL_MODE_BISYNC:
				while(rx_get_buf(info));
				break;
			}
			/* restart receiver if rx DMA buffers exhausted */
			if (info->rx_restart)
				rx_start(info);
			break;
		case BH_TRANSMIT:
			bh_transmit(info);
			break;
		case BH_STATUS:
			DBGBH(("%s bh status\n", info->device_name));
			info->ri_chkcount = 0;
			info->dsr_chkcount = 0;
			info->dcd_chkcount = 0;
			info->cts_chkcount = 0;
			break;
		default:
			DBGBH(("%s unknown action\n", info->device_name));
			break;
		}
	}
	DBGBH(("%s bh_handler exit\n", info->device_name));
}

static void bh_transmit(struct slgt_info *info)
{
	struct tty_struct *tty = info->port.tty;

	DBGBH(("%s bh_transmit\n", info->device_name));
	if (tty)
		tty_wakeup(tty);
}

static void dsr_change(struct slgt_info *info, unsigned short status)
{
	if (status & BIT3) {
		info->signals |= SerialSignal_DSR;
		info->input_signal_events.dsr_up++;
	} else {
		info->signals &= ~SerialSignal_DSR;
		info->input_signal_events.dsr_down++;
	}
	DBGISR(("dsr_change %s signals=%04X\n", info->device_name, info->signals));
	if ((info->dsr_chkcount)++ == IO_PIN_SHUTDOWN_LIMIT) {
		slgt_irq_off(info, IRQ_DSR);
		return;
	}
	info->icount.dsr++;
	wake_up_interruptible(&info->status_event_wait_q);
	wake_up_interruptible(&info->event_wait_q);
	info->pending_bh |= BH_STATUS;
}

static void cts_change(struct slgt_info *info, unsigned short status)
{
	if (status & BIT2) {
		info->signals |= SerialSignal_CTS;
		info->input_signal_events.cts_up++;
	} else {
		info->signals &= ~SerialSignal_CTS;
		info->input_signal_events.cts_down++;
	}
	DBGISR(("cts_change %s signals=%04X\n", info->device_name, info->signals));
	if ((info->cts_chkcount)++ == IO_PIN_SHUTDOWN_LIMIT) {
		slgt_irq_off(info, IRQ_CTS);
		return;
	}
	info->icount.cts++;
	wake_up_interruptible(&info->status_event_wait_q);
	wake_up_interruptible(&info->event_wait_q);
	info->pending_bh |= BH_STATUS;

	if (info->port.flags & ASYNC_CTS_FLOW) {
		if (info->port.tty) {
			if (info->port.tty->hw_stopped) {
				if (info->signals & SerialSignal_CTS) {
		 			info->port.tty->hw_stopped = 0;
					info->pending_bh |= BH_TRANSMIT;
					return;
				}
			} else {
				if (!(info->signals & SerialSignal_CTS))
		 			info->port.tty->hw_stopped = 1;
			}
		}
	}
}

static void dcd_change(struct slgt_info *info, unsigned short status)
{
	if (status & BIT1) {
		info->signals |= SerialSignal_DCD;
		info->input_signal_events.dcd_up++;
	} else {
		info->signals &= ~SerialSignal_DCD;
		info->input_signal_events.dcd_down++;
	}
	DBGISR(("dcd_change %s signals=%04X\n", info->device_name, info->signals));
	if ((info->dcd_chkcount)++ == IO_PIN_SHUTDOWN_LIMIT) {
		slgt_irq_off(info, IRQ_DCD);
		return;
	}
	info->icount.dcd++;
#if SYNCLINK_GENERIC_HDLC
	if (info->netcount) {
		if (info->signals & SerialSignal_DCD)
			netif_carrier_on(info->netdev);
		else
			netif_carrier_off(info->netdev);
	}
#endif
	wake_up_interruptible(&info->status_event_wait_q);
	wake_up_interruptible(&info->event_wait_q);
	info->pending_bh |= BH_STATUS;

	if (info->port.flags & ASYNC_CHECK_CD) {
		if (info->signals & SerialSignal_DCD)
			wake_up_interruptible(&info->port.open_wait);
		else {
			if (info->port.tty)
				tty_hangup(info->port.tty);
		}
	}
}

static void ri_change(struct slgt_info *info, unsigned short status)
{
	if (status & BIT0) {
		info->signals |= SerialSignal_RI;
		info->input_signal_events.ri_up++;
	} else {
		info->signals &= ~SerialSignal_RI;
		info->input_signal_events.ri_down++;
	}
	DBGISR(("ri_change %s signals=%04X\n", info->device_name, info->signals));
	if ((info->ri_chkcount)++ == IO_PIN_SHUTDOWN_LIMIT) {
		slgt_irq_off(info, IRQ_RI);
		return;
	}
	info->icount.rng++;
	wake_up_interruptible(&info->status_event_wait_q);
	wake_up_interruptible(&info->event_wait_q);
	info->pending_bh |= BH_STATUS;
}

static void isr_rxdata(struct slgt_info *info)
{
	unsigned int count = info->rbuf_fill_count;
	unsigned int i = info->rbuf_fill_index;
	unsigned short reg;

	while (rd_reg16(info, SSR) & IRQ_RXDATA) {
		reg = rd_reg16(info, RDR);
		DBGISR(("isr_rxdata %s RDR=%04X\n", info->device_name, reg));
		if (desc_complete(info->rbufs[i])) {
			/* all buffers full */
			rx_stop(info);
			info->rx_restart = 1;
			continue;
		}
		info->rbufs[i].buf[count++] = (unsigned char)reg;
		/* async mode saves status byte to buffer for each data byte */
		if (info->params.mode == MGSL_MODE_ASYNC)
			info->rbufs[i].buf[count++] = (unsigned char)(reg >> 8);
		if (count == info->rbuf_fill_level || (reg & BIT10)) {
			/* buffer full or end of frame */
			set_desc_count(info->rbufs[i], count);
			set_desc_status(info->rbufs[i], BIT15 | (reg >> 8));
			info->rbuf_fill_count = count = 0;
			if (++i == info->rbuf_count)
				i = 0;
			info->pending_bh |= BH_RECEIVE;
		}
	}

	info->rbuf_fill_index = i;
	info->rbuf_fill_count = count;
}

static void isr_serial(struct slgt_info *info)
{
	unsigned short status = rd_reg16(info, SSR);

	DBGISR(("%s isr_serial status=%04X\n", info->device_name, status));

	wr_reg16(info, SSR, status); /* clear pending */

	info->irq_occurred = true;

	if (info->params.mode == MGSL_MODE_ASYNC) {
		if (status & IRQ_TXIDLE) {
			if (info->tx_count)
				isr_txeom(info, status);
		}
		if (info->rx_pio && (status & IRQ_RXDATA))
			isr_rxdata(info);
		if ((status & IRQ_RXBREAK) && (status & RXBREAK)) {
			info->icount.brk++;
			/* process break detection if tty control allows */
			if (info->port.tty) {
				if (!(status & info->ignore_status_mask)) {
					if (info->read_status_mask & MASK_BREAK) {
						tty_insert_flip_char(info->port.tty, 0, TTY_BREAK);
						if (info->port.flags & ASYNC_SAK)
							do_SAK(info->port.tty);
					}
				}
			}
		}
	} else {
		if (status & (IRQ_TXIDLE + IRQ_TXUNDER))
			isr_txeom(info, status);
		if (info->rx_pio && (status & IRQ_RXDATA))
			isr_rxdata(info);
		if (status & IRQ_RXIDLE) {
			if (status & RXIDLE)
				info->icount.rxidle++;
			else
				info->icount.exithunt++;
			wake_up_interruptible(&info->event_wait_q);
		}

		if (status & IRQ_RXOVER)
			rx_start(info);
	}

	if (status & IRQ_DSR)
		dsr_change(info, status);
	if (status & IRQ_CTS)
		cts_change(info, status);
	if (status & IRQ_DCD)
		dcd_change(info, status);
	if (status & IRQ_RI)
		ri_change(info, status);
}

static void isr_rdma(struct slgt_info *info)
{
	unsigned int status = rd_reg32(info, RDCSR);

	DBGISR(("%s isr_rdma status=%08x\n", info->device_name, status));

	/* RDCSR (rx DMA control/status)
	 *
	 * 31..07  reserved
	 * 06      save status byte to DMA buffer
	 * 05      error
	 * 04      eol (end of list)
	 * 03      eob (end of buffer)
	 * 02      IRQ enable
	 * 01      reset
	 * 00      enable
	 */
	wr_reg32(info, RDCSR, status);	/* clear pending */

	if (status & (BIT5 + BIT4)) {
		DBGISR(("%s isr_rdma rx_restart=1\n", info->device_name));
		info->rx_restart = true;
	}
	info->pending_bh |= BH_RECEIVE;
}

static void isr_tdma(struct slgt_info *info)
{
	unsigned int status = rd_reg32(info, TDCSR);

	DBGISR(("%s isr_tdma status=%08x\n", info->device_name, status));

	/* TDCSR (tx DMA control/status)
	 *
	 * 31..06  reserved
	 * 05      error
	 * 04      eol (end of list)
	 * 03      eob (end of buffer)
	 * 02      IRQ enable
	 * 01      reset
	 * 00      enable
	 */
	wr_reg32(info, TDCSR, status);	/* clear pending */

	if (status & (BIT5 + BIT4 + BIT3)) {
		// another transmit buffer has completed
		// run bottom half to get more send data from user
		info->pending_bh |= BH_TRANSMIT;
	}
}

static void isr_txeom(struct slgt_info *info, unsigned short status)
{
	DBGISR(("%s txeom status=%04x\n", info->device_name, status));

	slgt_irq_off(info, IRQ_TXDATA + IRQ_TXIDLE + IRQ_TXUNDER);
	tdma_reset(info);
	reset_tbufs(info);
	if (status & IRQ_TXUNDER) {
		unsigned short val = rd_reg16(info, TCR);
		wr_reg16(info, TCR, (unsigned short)(val | BIT2)); /* set reset bit */
		wr_reg16(info, TCR, val); /* clear reset bit */
	}

	if (info->tx_active) {
		if (info->params.mode != MGSL_MODE_ASYNC) {
			if (status & IRQ_TXUNDER)
				info->icount.txunder++;
			else if (status & IRQ_TXIDLE)
				info->icount.txok++;
		}

		info->tx_active = false;
		info->tx_count = 0;

		del_timer(&info->tx_timer);

		if (info->params.mode != MGSL_MODE_ASYNC && info->drop_rts_on_tx_done) {
			info->signals &= ~SerialSignal_RTS;
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

static void isr_gpio(struct slgt_info *info, unsigned int changed, unsigned int state)
{
	struct cond_wait *w, *prev;

	/* wake processes waiting for specific transitions */
	for (w = info->gpio_wait_q, prev = NULL ; w != NULL ; w = w->next) {
		if (w->data & changed) {
			w->data = state;
			wake_up_interruptible(&w->q);
			if (prev != NULL)
				prev->next = w->next;
			else
				info->gpio_wait_q = w->next;
		} else
			prev = w;
	}
}

/* interrupt service routine
 *
 * 	irq	interrupt number
 * 	dev_id	device ID supplied during interrupt registration
 */
static irqreturn_t slgt_interrupt(int dummy, void *dev_id)
{
	struct slgt_info *info = dev_id;
	unsigned int gsr;
	unsigned int i;

	DBGISR(("slgt_interrupt irq=%d entry\n", info->irq_level));

	spin_lock(&info->lock);

	while((gsr = rd_reg32(info, GSR) & 0xffffff00)) {
		DBGISR(("%s gsr=%08x\n", info->device_name, gsr));
		info->irq_occurred = true;
		for(i=0; i < info->port_count ; i++) {
			if (info->port_array[i] == NULL)
				continue;
			if (gsr & (BIT8 << i))
				isr_serial(info->port_array[i]);
			if (gsr & (BIT16 << (i*2)))
				isr_rdma(info->port_array[i]);
			if (gsr & (BIT17 << (i*2)))
				isr_tdma(info->port_array[i]);
		}
	}

	if (info->gpio_present) {
		unsigned int state;
		unsigned int changed;
		while ((changed = rd_reg32(info, IOSR)) != 0) {
			DBGISR(("%s iosr=%08x\n", info->device_name, changed));
			/* read latched state of GPIO signals */
			state = rd_reg32(info, IOVR);
			/* clear pending GPIO interrupt bits */
			wr_reg32(info, IOSR, changed);
			for (i=0 ; i < info->port_count ; i++) {
				if (info->port_array[i] != NULL)
					isr_gpio(info->port_array[i], changed, state);
			}
		}
	}

	for(i=0; i < info->port_count ; i++) {
		struct slgt_info *port = info->port_array[i];

		if (port && (port->port.count || port->netcount) &&
		    port->pending_bh && !port->bh_running &&
		    !port->bh_requested) {
			DBGISR(("%s bh queued\n", port->device_name));
			schedule_work(&port->task);
			port->bh_requested = true;
		}
	}

	spin_unlock(&info->lock);

	DBGISR(("slgt_interrupt irq=%d exit\n", info->irq_level));
	return IRQ_HANDLED;
}

static int startup(struct slgt_info *info)
{
	DBGINFO(("%s startup\n", info->device_name));

	if (info->port.flags & ASYNC_INITIALIZED)
		return 0;

	if (!info->tx_buf) {
		info->tx_buf = kmalloc(info->max_frame_size, GFP_KERNEL);
		if (!info->tx_buf) {
			DBGERR(("%s can't allocate tx buffer\n", info->device_name));
			return -ENOMEM;
		}
	}

	info->pending_bh = 0;

	memset(&info->icount, 0, sizeof(info->icount));

	/* program hardware for current parameters */
	change_params(info);

	if (info->port.tty)
		clear_bit(TTY_IO_ERROR, &info->port.tty->flags);

	info->port.flags |= ASYNC_INITIALIZED;

	return 0;
}

/*
 *  called by close() and hangup() to shutdown hardware
 */
static void shutdown(struct slgt_info *info)
{
	unsigned long flags;

	if (!(info->port.flags & ASYNC_INITIALIZED))
		return;

	DBGINFO(("%s shutdown\n", info->device_name));

	/* clear status wait queue because status changes */
	/* can't happen after shutting down the hardware */
	wake_up_interruptible(&info->status_event_wait_q);
	wake_up_interruptible(&info->event_wait_q);

	del_timer_sync(&info->tx_timer);
	del_timer_sync(&info->rx_timer);

	kfree(info->tx_buf);
	info->tx_buf = NULL;

	spin_lock_irqsave(&info->lock,flags);

	tx_stop(info);
	rx_stop(info);

	slgt_irq_off(info, IRQ_ALL | IRQ_MASTER);

 	if (!info->port.tty || info->port.tty->termios->c_cflag & HUPCL) {
 		info->signals &= ~(SerialSignal_DTR + SerialSignal_RTS);
		set_signals(info);
	}

	flush_cond_wait(&info->gpio_wait_q);

	spin_unlock_irqrestore(&info->lock,flags);

	if (info->port.tty)
		set_bit(TTY_IO_ERROR, &info->port.tty->flags);

	info->port.flags &= ~ASYNC_INITIALIZED;
}

static void program_hw(struct slgt_info *info)
{
	unsigned long flags;

	spin_lock_irqsave(&info->lock,flags);

	rx_stop(info);
	tx_stop(info);

	if (info->params.mode != MGSL_MODE_ASYNC ||
	    info->netcount)
		sync_mode(info);
	else
		async_mode(info);

	set_signals(info);

	info->dcd_chkcount = 0;
	info->cts_chkcount = 0;
	info->ri_chkcount = 0;
	info->dsr_chkcount = 0;

	slgt_irq_on(info, IRQ_DCD | IRQ_CTS | IRQ_DSR | IRQ_RI);
	get_signals(info);

	if (info->netcount ||
	    (info->port.tty && info->port.tty->termios->c_cflag & CREAD))
		rx_start(info);

	spin_unlock_irqrestore(&info->lock,flags);
}

/*
 * reconfigure adapter based on new parameters
 */
static void change_params(struct slgt_info *info)
{
	unsigned cflag;
	int bits_per_char;

	if (!info->port.tty || !info->port.tty->termios)
		return;
	DBGINFO(("%s change_params\n", info->device_name));

	cflag = info->port.tty->termios->c_cflag;

	/* if B0 rate (hangup) specified then negate DTR and RTS */
	/* otherwise assert DTR and RTS */
 	if (cflag & CBAUD)
		info->signals |= SerialSignal_RTS + SerialSignal_DTR;
	else
		info->signals &= ~(SerialSignal_RTS + SerialSignal_DTR);

	/* byte size and parity */

	switch (cflag & CSIZE) {
	case CS5: info->params.data_bits = 5; break;
	case CS6: info->params.data_bits = 6; break;
	case CS7: info->params.data_bits = 7; break;
	case CS8: info->params.data_bits = 8; break;
	default:  info->params.data_bits = 7; break;
	}

	info->params.stop_bits = (cflag & CSTOPB) ? 2 : 1;

	if (cflag & PARENB)
		info->params.parity = (cflag & PARODD) ? ASYNC_PARITY_ODD : ASYNC_PARITY_EVEN;
	else
		info->params.parity = ASYNC_PARITY_NONE;

	/* calculate number of jiffies to transmit a full
	 * FIFO (32 bytes) at specified data rate
	 */
	bits_per_char = info->params.data_bits +
			info->params.stop_bits + 1;

	info->params.data_rate = tty_get_baud_rate(info->port.tty);

	if (info->params.data_rate) {
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

	info->read_status_mask = IRQ_RXOVER;
	if (I_INPCK(info->port.tty))
		info->read_status_mask |= MASK_PARITY | MASK_FRAMING;
 	if (I_BRKINT(info->port.tty) || I_PARMRK(info->port.tty))
 		info->read_status_mask |= MASK_BREAK;
	if (I_IGNPAR(info->port.tty))
		info->ignore_status_mask |= MASK_PARITY | MASK_FRAMING;
	if (I_IGNBRK(info->port.tty)) {
		info->ignore_status_mask |= MASK_BREAK;
		/* If ignoring parity and break indicators, ignore
		 * overruns too.  (For real raw support).
		 */
		if (I_IGNPAR(info->port.tty))
			info->ignore_status_mask |= MASK_OVERRUN;
	}

	program_hw(info);
}

static int get_stats(struct slgt_info *info, struct mgsl_icount __user *user_icount)
{
	DBGINFO(("%s get_stats\n",  info->device_name));
	if (!user_icount) {
		memset(&info->icount, 0, sizeof(info->icount));
	} else {
		if (copy_to_user(user_icount, &info->icount, sizeof(struct mgsl_icount)))
			return -EFAULT;
	}
	return 0;
}

static int get_params(struct slgt_info *info, MGSL_PARAMS __user *user_params)
{
	DBGINFO(("%s get_params\n", info->device_name));
	if (copy_to_user(user_params, &info->params, sizeof(MGSL_PARAMS)))
		return -EFAULT;
	return 0;
}

static int set_params(struct slgt_info *info, MGSL_PARAMS __user *new_params)
{
 	unsigned long flags;
	MGSL_PARAMS tmp_params;

	DBGINFO(("%s set_params\n", info->device_name));
	if (copy_from_user(&tmp_params, new_params, sizeof(MGSL_PARAMS)))
		return -EFAULT;

	spin_lock_irqsave(&info->lock, flags);
	if (tmp_params.mode == MGSL_MODE_BASE_CLOCK)
		info->base_clock = tmp_params.clock_speed;
	else
		memcpy(&info->params, &tmp_params, sizeof(MGSL_PARAMS));
	spin_unlock_irqrestore(&info->lock, flags);

	program_hw(info);

	return 0;
}

static int get_txidle(struct slgt_info *info, int __user *idle_mode)
{
	DBGINFO(("%s get_txidle=%d\n", info->device_name, info->idle_mode));
	if (put_user(info->idle_mode, idle_mode))
		return -EFAULT;
	return 0;
}

static int set_txidle(struct slgt_info *info, int idle_mode)
{
 	unsigned long flags;
	DBGINFO(("%s set_txidle(%d)\n", info->device_name, idle_mode));
	spin_lock_irqsave(&info->lock,flags);
	info->idle_mode = idle_mode;
	if (info->params.mode != MGSL_MODE_ASYNC)
		tx_set_idle(info);
	spin_unlock_irqrestore(&info->lock,flags);
	return 0;
}

static int tx_enable(struct slgt_info *info, int enable)
{
 	unsigned long flags;
	DBGINFO(("%s tx_enable(%d)\n", info->device_name, enable));
	spin_lock_irqsave(&info->lock,flags);
	if (enable) {
		if (!info->tx_enabled)
			tx_start(info);
	} else {
		if (info->tx_enabled)
			tx_stop(info);
	}
	spin_unlock_irqrestore(&info->lock,flags);
	return 0;
}

/*
 * abort transmit HDLC frame
 */
static int tx_abort(struct slgt_info *info)
{
 	unsigned long flags;
	DBGINFO(("%s tx_abort\n", info->device_name));
	spin_lock_irqsave(&info->lock,flags);
	tdma_reset(info);
	spin_unlock_irqrestore(&info->lock,flags);
	return 0;
}

static int rx_enable(struct slgt_info *info, int enable)
{
 	unsigned long flags;
	unsigned int rbuf_fill_level;
	DBGINFO(("%s rx_enable(%08x)\n", info->device_name, enable));
	spin_lock_irqsave(&info->lock,flags);
	/*
	 * enable[31..16] = receive DMA buffer fill level
	 * 0 = noop (leave fill level unchanged)
	 * fill level must be multiple of 4 and <= buffer size
	 */
	rbuf_fill_level = ((unsigned int)enable) >> 16;
	if (rbuf_fill_level) {
		if ((rbuf_fill_level > DMABUFSIZE) || (rbuf_fill_level % 4)) {
			spin_unlock_irqrestore(&info->lock, flags);
			return -EINVAL;
		}
		info->rbuf_fill_level = rbuf_fill_level;
		if (rbuf_fill_level < 128)
			info->rx_pio = 1; /* PIO mode */
		else
			info->rx_pio = 0; /* DMA mode */
		rx_stop(info); /* restart receiver to use new fill level */
	}

	/*
	 * enable[1..0] = receiver enable command
	 * 0 = disable
	 * 1 = enable
	 * 2 = enable or force hunt mode if already enabled
	 */
	enable &= 3;
	if (enable) {
		if (!info->rx_enabled)
			rx_start(info);
		else if (enable == 2) {
			/* force hunt mode (write 1 to RCR[3]) */
			wr_reg16(info, RCR, rd_reg16(info, RCR) | BIT3);
		}
	} else {
		if (info->rx_enabled)
			rx_stop(info);
	}
	spin_unlock_irqrestore(&info->lock,flags);
	return 0;
}

/*
 *  wait for specified event to occur
 */
static int wait_mgsl_event(struct slgt_info *info, int __user *mask_ptr)
{
 	unsigned long flags;
	int s;
	int rc=0;
	struct mgsl_icount cprev, cnow;
	int events;
	int mask;
	struct	_input_signal_events oldsigs, newsigs;
	DECLARE_WAITQUEUE(wait, current);

	if (get_user(mask, mask_ptr))
		return -EFAULT;

	DBGINFO(("%s wait_mgsl_event(%d)\n", info->device_name, mask));

	spin_lock_irqsave(&info->lock,flags);

	/* return immediately if state matches requested events */
	get_signals(info);
	s = info->signals;

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
		unsigned short val = rd_reg16(info, SCR);
		if (!(val & IRQ_RXIDLE))
			wr_reg16(info, SCR, (unsigned short)(val | IRQ_RXIDLE));
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
			wr_reg16(info, SCR,
				(unsigned short)(rd_reg16(info, SCR) & ~IRQ_RXIDLE));
		}
		spin_unlock_irqrestore(&info->lock,flags);
	}
exit:
	if (rc == 0)
		rc = put_user(events, mask_ptr);
	return rc;
}

static int get_interface(struct slgt_info *info, int __user *if_mode)
{
	DBGINFO(("%s get_interface=%x\n", info->device_name, info->if_mode));
	if (put_user(info->if_mode, if_mode))
		return -EFAULT;
	return 0;
}

static int set_interface(struct slgt_info *info, int if_mode)
{
 	unsigned long flags;
	unsigned short val;

	DBGINFO(("%s set_interface=%x)\n", info->device_name, if_mode));
	spin_lock_irqsave(&info->lock,flags);
	info->if_mode = if_mode;

	msc_set_vcr(info);

	/* TCR (tx control) 07  1=RTS driver control */
	val = rd_reg16(info, TCR);
	if (info->if_mode & MGSL_INTERFACE_RTS_EN)
		val |= BIT7;
	else
		val &= ~BIT7;
	wr_reg16(info, TCR, val);

	spin_unlock_irqrestore(&info->lock,flags);
	return 0;
}

/*
 * set general purpose IO pin state and direction
 *
 * user_gpio fields:
 * state   each bit indicates a pin state
 * smask   set bit indicates pin state to set
 * dir     each bit indicates a pin direction (0=input, 1=output)
 * dmask   set bit indicates pin direction to set
 */
static int set_gpio(struct slgt_info *info, struct gpio_desc __user *user_gpio)
{
 	unsigned long flags;
	struct gpio_desc gpio;
	__u32 data;

	if (!info->gpio_present)
		return -EINVAL;
	if (copy_from_user(&gpio, user_gpio, sizeof(gpio)))
		return -EFAULT;
	DBGINFO(("%s set_gpio state=%08x smask=%08x dir=%08x dmask=%08x\n",
		 info->device_name, gpio.state, gpio.smask,
		 gpio.dir, gpio.dmask));

	spin_lock_irqsave(&info->lock,flags);
	if (gpio.dmask) {
		data = rd_reg32(info, IODR);
		data |= gpio.dmask & gpio.dir;
		data &= ~(gpio.dmask & ~gpio.dir);
		wr_reg32(info, IODR, data);
	}
	if (gpio.smask) {
		data = rd_reg32(info, IOVR);
		data |= gpio.smask & gpio.state;
		data &= ~(gpio.smask & ~gpio.state);
		wr_reg32(info, IOVR, data);
	}
	spin_unlock_irqrestore(&info->lock,flags);

	return 0;
}

/*
 * get general purpose IO pin state and direction
 */
static int get_gpio(struct slgt_info *info, struct gpio_desc __user *user_gpio)
{
	struct gpio_desc gpio;
	if (!info->gpio_present)
		return -EINVAL;
	gpio.state = rd_reg32(info, IOVR);
	gpio.smask = 0xffffffff;
	gpio.dir   = rd_reg32(info, IODR);
	gpio.dmask = 0xffffffff;
	if (copy_to_user(user_gpio, &gpio, sizeof(gpio)))
		return -EFAULT;
	DBGINFO(("%s get_gpio state=%08x dir=%08x\n",
		 info->device_name, gpio.state, gpio.dir));
	return 0;
}

/*
 * conditional wait facility
 */
static void init_cond_wait(struct cond_wait *w, unsigned int data)
{
	init_waitqueue_head(&w->q);
	init_waitqueue_entry(&w->wait, current);
	w->data = data;
}

static void add_cond_wait(struct cond_wait **head, struct cond_wait *w)
{
	set_current_state(TASK_INTERRUPTIBLE);
	add_wait_queue(&w->q, &w->wait);
	w->next = *head;
	*head = w;
}

static void remove_cond_wait(struct cond_wait **head, struct cond_wait *cw)
{
	struct cond_wait *w, *prev;
	remove_wait_queue(&cw->q, &cw->wait);
	set_current_state(TASK_RUNNING);
	for (w = *head, prev = NULL ; w != NULL ; prev = w, w = w->next) {
		if (w == cw) {
			if (prev != NULL)
				prev->next = w->next;
			else
				*head = w->next;
			break;
		}
	}
}

static void flush_cond_wait(struct cond_wait **head)
{
	while (*head != NULL) {
		wake_up_interruptible(&(*head)->q);
		*head = (*head)->next;
	}
}

/*
 * wait for general purpose I/O pin(s) to enter specified state
 *
 * user_gpio fields:
 * state - bit indicates target pin state
 * smask - set bit indicates watched pin
 *
 * The wait ends when at least one watched pin enters the specified
 * state. When 0 (no error) is returned, user_gpio->state is set to the
 * state of all GPIO pins when the wait ends.
 *
 * Note: Each pin may be a dedicated input, dedicated output, or
 * configurable input/output. The number and configuration of pins
 * varies with the specific adapter model. Only input pins (dedicated
 * or configured) can be monitored with this function.
 */
static int wait_gpio(struct slgt_info *info, struct gpio_desc __user *user_gpio)
{
 	unsigned long flags;
	int rc = 0;
	struct gpio_desc gpio;
	struct cond_wait wait;
	u32 state;

	if (!info->gpio_present)
		return -EINVAL;
	if (copy_from_user(&gpio, user_gpio, sizeof(gpio)))
		return -EFAULT;
	DBGINFO(("%s wait_gpio() state=%08x smask=%08x\n",
		 info->device_name, gpio.state, gpio.smask));
	/* ignore output pins identified by set IODR bit */
	if ((gpio.smask &= ~rd_reg32(info, IODR)) == 0)
		return -EINVAL;
	init_cond_wait(&wait, gpio.smask);

	spin_lock_irqsave(&info->lock, flags);
	/* enable interrupts for watched pins */
	wr_reg32(info, IOER, rd_reg32(info, IOER) | gpio.smask);
	/* get current pin states */
	state = rd_reg32(info, IOVR);

	if (gpio.smask & ~(state ^ gpio.state)) {
		/* already in target state */
		gpio.state = state;
	} else {
		/* wait for target state */
		add_cond_wait(&info->gpio_wait_q, &wait);
		spin_unlock_irqrestore(&info->lock, flags);
		schedule();
		if (signal_pending(current))
			rc = -ERESTARTSYS;
		else
			gpio.state = wait.data;
		spin_lock_irqsave(&info->lock, flags);
		remove_cond_wait(&info->gpio_wait_q, &wait);
	}

	/* disable all GPIO interrupts if no waiting processes */
	if (info->gpio_wait_q == NULL)
		wr_reg32(info, IOER, 0);
	spin_unlock_irqrestore(&info->lock,flags);

	if ((rc == 0) && copy_to_user(user_gpio, &gpio, sizeof(gpio)))
		rc = -EFAULT;
	return rc;
}

static int modem_input_wait(struct slgt_info *info,int arg)
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

/*
 *  return state of serial control and status signals
 */
static int tiocmget(struct tty_struct *tty, struct file *file)
{
	struct slgt_info *info = tty->driver_data;
	unsigned int result;
 	unsigned long flags;

	spin_lock_irqsave(&info->lock,flags);
 	get_signals(info);
	spin_unlock_irqrestore(&info->lock,flags);

	result = ((info->signals & SerialSignal_RTS) ? TIOCM_RTS:0) +
		((info->signals & SerialSignal_DTR) ? TIOCM_DTR:0) +
		((info->signals & SerialSignal_DCD) ? TIOCM_CAR:0) +
		((info->signals & SerialSignal_RI)  ? TIOCM_RNG:0) +
		((info->signals & SerialSignal_DSR) ? TIOCM_DSR:0) +
		((info->signals & SerialSignal_CTS) ? TIOCM_CTS:0);

	DBGINFO(("%s tiocmget value=%08X\n", info->device_name, result));
	return result;
}

/*
 * set modem control signals (DTR/RTS)
 *
 * 	cmd	signal command: TIOCMBIS = set bit TIOCMBIC = clear bit
 *		TIOCMSET = set/clear signal values
 * 	value	bit mask for command
 */
static int tiocmset(struct tty_struct *tty, struct file *file,
		    unsigned int set, unsigned int clear)
{
	struct slgt_info *info = tty->driver_data;
 	unsigned long flags;

	DBGINFO(("%s tiocmset(%x,%x)\n", info->device_name, set, clear));

	if (set & TIOCM_RTS)
		info->signals |= SerialSignal_RTS;
	if (set & TIOCM_DTR)
		info->signals |= SerialSignal_DTR;
	if (clear & TIOCM_RTS)
		info->signals &= ~SerialSignal_RTS;
	if (clear & TIOCM_DTR)
		info->signals &= ~SerialSignal_DTR;

	spin_lock_irqsave(&info->lock,flags);
 	set_signals(info);
	spin_unlock_irqrestore(&info->lock,flags);
	return 0;
}

static int carrier_raised(struct tty_port *port)
{
	unsigned long flags;
	struct slgt_info *info = container_of(port, struct slgt_info, port);

	spin_lock_irqsave(&info->lock,flags);
 	get_signals(info);
	spin_unlock_irqrestore(&info->lock,flags);
	return (info->signals & SerialSignal_DCD) ? 1 : 0;
}

static void dtr_rts(struct tty_port *port, int on)
{
	unsigned long flags;
	struct slgt_info *info = container_of(port, struct slgt_info, port);

	spin_lock_irqsave(&info->lock,flags);
	if (on)
		info->signals |= SerialSignal_RTS + SerialSignal_DTR;
	else
		info->signals &= ~(SerialSignal_RTS + SerialSignal_DTR);
 	set_signals(info);
	spin_unlock_irqrestore(&info->lock,flags);
}


/*
 *  block current process until the device is ready to open
 */
static int block_til_ready(struct tty_struct *tty, struct file *filp,
			   struct slgt_info *info)
{
	DECLARE_WAITQUEUE(wait, current);
	int		retval;
	bool		do_clocal = false;
	bool		extra_count = false;
	unsigned long	flags;
	int		cd;
	struct tty_port *port = &info->port;

	DBGINFO(("%s block_til_ready\n", tty->driver->name));

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
	 * close() knows when to free things.  We restore it upon
	 * exit, either normal or abnormal.
	 */

	retval = 0;
	add_wait_queue(&port->open_wait, &wait);

	spin_lock_irqsave(&info->lock, flags);
	if (!tty_hung_up_p(filp)) {
		extra_count = true;
		port->count--;
	}
	spin_unlock_irqrestore(&info->lock, flags);
	port->blocked_open++;

	while (1) {
		if ((tty->termios->c_cflag & CBAUD))
			tty_port_raise_dtr_rts(port);

		set_current_state(TASK_INTERRUPTIBLE);

		if (tty_hung_up_p(filp) || !(port->flags & ASYNC_INITIALIZED)){
			retval = (port->flags & ASYNC_HUP_NOTIFY) ?
					-EAGAIN : -ERESTARTSYS;
			break;
		}

		cd = tty_port_carrier_raised(port);

 		if (!(port->flags & ASYNC_CLOSING) && (do_clocal || cd ))
 			break;

		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}

		DBGINFO(("%s block_til_ready wait\n", tty->driver->name));
		schedule();
	}

	set_current_state(TASK_RUNNING);
	remove_wait_queue(&port->open_wait, &wait);

	if (extra_count)
		port->count++;
	port->blocked_open--;

	if (!retval)
		port->flags |= ASYNC_NORMAL_ACTIVE;

	DBGINFO(("%s block_til_ready ready, rc=%d\n", tty->driver->name, retval));
	return retval;
}

static int alloc_tmp_rbuf(struct slgt_info *info)
{
	info->tmp_rbuf = kmalloc(info->max_frame_size + 5, GFP_KERNEL);
	if (info->tmp_rbuf == NULL)
		return -ENOMEM;
	return 0;
}

static void free_tmp_rbuf(struct slgt_info *info)
{
	kfree(info->tmp_rbuf);
	info->tmp_rbuf = NULL;
}

/*
 * allocate DMA descriptor lists.
 */
static int alloc_desc(struct slgt_info *info)
{
	unsigned int i;
	unsigned int pbufs;

	/* allocate memory to hold descriptor lists */
	info->bufs = pci_alloc_consistent(info->pdev, DESC_LIST_SIZE, &info->bufs_dma_addr);
	if (info->bufs == NULL)
		return -ENOMEM;

	memset(info->bufs, 0, DESC_LIST_SIZE);

	info->rbufs = (struct slgt_desc*)info->bufs;
	info->tbufs = ((struct slgt_desc*)info->bufs) + info->rbuf_count;

	pbufs = (unsigned int)info->bufs_dma_addr;

	/*
	 * Build circular lists of descriptors
	 */

	for (i=0; i < info->rbuf_count; i++) {
		/* physical address of this descriptor */
		info->rbufs[i].pdesc = pbufs + (i * sizeof(struct slgt_desc));

		/* physical address of next descriptor */
		if (i == info->rbuf_count - 1)
			info->rbufs[i].next = cpu_to_le32(pbufs);
		else
			info->rbufs[i].next = cpu_to_le32(pbufs + ((i+1) * sizeof(struct slgt_desc)));
		set_desc_count(info->rbufs[i], DMABUFSIZE);
	}

	for (i=0; i < info->tbuf_count; i++) {
		/* physical address of this descriptor */
		info->tbufs[i].pdesc = pbufs + ((info->rbuf_count + i) * sizeof(struct slgt_desc));

		/* physical address of next descriptor */
		if (i == info->tbuf_count - 1)
			info->tbufs[i].next = cpu_to_le32(pbufs + info->rbuf_count * sizeof(struct slgt_desc));
		else
			info->tbufs[i].next = cpu_to_le32(pbufs + ((info->rbuf_count + i + 1) * sizeof(struct slgt_desc)));
	}

	return 0;
}

static void free_desc(struct slgt_info *info)
{
	if (info->bufs != NULL) {
		pci_free_consistent(info->pdev, DESC_LIST_SIZE, info->bufs, info->bufs_dma_addr);
		info->bufs  = NULL;
		info->rbufs = NULL;
		info->tbufs = NULL;
	}
}

static int alloc_bufs(struct slgt_info *info, struct slgt_desc *bufs, int count)
{
	int i;
	for (i=0; i < count; i++) {
		if ((bufs[i].buf = pci_alloc_consistent(info->pdev, DMABUFSIZE, &bufs[i].buf_dma_addr)) == NULL)
			return -ENOMEM;
		bufs[i].pbuf  = cpu_to_le32((unsigned int)bufs[i].buf_dma_addr);
	}
	return 0;
}

static void free_bufs(struct slgt_info *info, struct slgt_desc *bufs, int count)
{
	int i;
	for (i=0; i < count; i++) {
		if (bufs[i].buf == NULL)
			continue;
		pci_free_consistent(info->pdev, DMABUFSIZE, bufs[i].buf, bufs[i].buf_dma_addr);
		bufs[i].buf = NULL;
	}
}

static int alloc_dma_bufs(struct slgt_info *info)
{
	info->rbuf_count = 32;
	info->tbuf_count = 32;

	if (alloc_desc(info) < 0 ||
	    alloc_bufs(info, info->rbufs, info->rbuf_count) < 0 ||
	    alloc_bufs(info, info->tbufs, info->tbuf_count) < 0 ||
	    alloc_tmp_rbuf(info) < 0) {
		DBGERR(("%s DMA buffer alloc fail\n", info->device_name));
		return -ENOMEM;
	}
	reset_rbufs(info);
	return 0;
}

static void free_dma_bufs(struct slgt_info *info)
{
	if (info->bufs) {
		free_bufs(info, info->rbufs, info->rbuf_count);
		free_bufs(info, info->tbufs, info->tbuf_count);
		free_desc(info);
	}
	free_tmp_rbuf(info);
}

static int claim_resources(struct slgt_info *info)
{
	if (request_mem_region(info->phys_reg_addr, SLGT_REG_SIZE, "synclink_gt") == NULL) {
		DBGERR(("%s reg addr conflict, addr=%08X\n",
			info->device_name, info->phys_reg_addr));
		info->init_error = DiagStatus_AddressConflict;
		goto errout;
	}
	else
		info->reg_addr_requested = true;

	info->reg_addr = ioremap_nocache(info->phys_reg_addr, SLGT_REG_SIZE);
	if (!info->reg_addr) {
		DBGERR(("%s cant map device registers, addr=%08X\n",
			info->device_name, info->phys_reg_addr));
		info->init_error = DiagStatus_CantAssignPciResources;
		goto errout;
	}
	return 0;

errout:
	release_resources(info);
	return -ENODEV;
}

static void release_resources(struct slgt_info *info)
{
	if (info->irq_requested) {
		free_irq(info->irq_level, info);
		info->irq_requested = false;
	}

	if (info->reg_addr_requested) {
		release_mem_region(info->phys_reg_addr, SLGT_REG_SIZE);
		info->reg_addr_requested = false;
	}

	if (info->reg_addr) {
		iounmap(info->reg_addr);
		info->reg_addr = NULL;
	}
}

/* Add the specified device instance data structure to the
 * global linked list of devices and increment the device count.
 */
static void add_device(struct slgt_info *info)
{
	char *devstr;

	info->next_device = NULL;
	info->line = slgt_device_count;
	sprintf(info->device_name, "%s%d", tty_dev_prefix, info->line);

	if (info->line < MAX_DEVICES) {
		if (maxframe[info->line])
			info->max_frame_size = maxframe[info->line];
	}

	slgt_device_count++;

	if (!slgt_device_list)
		slgt_device_list = info;
	else {
		struct slgt_info *current_dev = slgt_device_list;
		while(current_dev->next_device)
			current_dev = current_dev->next_device;
		current_dev->next_device = info;
	}

	if (info->max_frame_size < 4096)
		info->max_frame_size = 4096;
	else if (info->max_frame_size > 65535)
		info->max_frame_size = 65535;

	switch(info->pdev->device) {
	case SYNCLINK_GT_DEVICE_ID:
		devstr = "GT";
		break;
	case SYNCLINK_GT2_DEVICE_ID:
		devstr = "GT2";
		break;
	case SYNCLINK_GT4_DEVICE_ID:
		devstr = "GT4";
		break;
	case SYNCLINK_AC_DEVICE_ID:
		devstr = "AC";
		info->params.mode = MGSL_MODE_ASYNC;
		break;
	default:
		devstr = "(unknown model)";
	}
	printk("SyncLink %s %s IO=%08x IRQ=%d MaxFrameSize=%u\n",
		devstr, info->device_name, info->phys_reg_addr,
		info->irq_level, info->max_frame_size);

#if SYNCLINK_GENERIC_HDLC
	hdlcdev_init(info);
#endif
}

static const struct tty_port_operations slgt_port_ops = {
	.carrier_raised = carrier_raised,
	.dtr_rts = dtr_rts,
};

/*
 *  allocate device instance structure, return NULL on failure
 */
static struct slgt_info *alloc_dev(int adapter_num, int port_num, struct pci_dev *pdev)
{
	struct slgt_info *info;

	info = kzalloc(sizeof(struct slgt_info), GFP_KERNEL);

	if (!info) {
		DBGERR(("%s device alloc failed adapter=%d port=%d\n",
			driver_name, adapter_num, port_num));
	} else {
		tty_port_init(&info->port);
		info->port.ops = &slgt_port_ops;
		info->magic = MGSL_MAGIC;
		INIT_WORK(&info->task, bh_handler);
		info->max_frame_size = 4096;
		info->base_clock = 14745600;
		info->rbuf_fill_level = DMABUFSIZE;
		info->port.close_delay = 5*HZ/10;
		info->port.closing_wait = 30*HZ;
		init_waitqueue_head(&info->status_event_wait_q);
		init_waitqueue_head(&info->event_wait_q);
		spin_lock_init(&info->netlock);
		memcpy(&info->params,&default_params,sizeof(MGSL_PARAMS));
		info->idle_mode = HDLC_TXIDLE_FLAGS;
		info->adapter_num = adapter_num;
		info->port_num = port_num;

		setup_timer(&info->tx_timer, tx_timeout, (unsigned long)info);
		setup_timer(&info->rx_timer, rx_timeout, (unsigned long)info);

		/* Copy configuration info to device instance data */
		info->pdev = pdev;
		info->irq_level = pdev->irq;
		info->phys_reg_addr = pci_resource_start(pdev,0);

		info->bus_type = MGSL_BUS_TYPE_PCI;
		info->irq_flags = IRQF_SHARED;

		info->init_error = -1; /* assume error, set to 0 on successful init */
	}

	return info;
}

static void device_init(int adapter_num, struct pci_dev *pdev)
{
	struct slgt_info *port_array[SLGT_MAX_PORTS];
	int i;
	int port_count = 1;

	if (pdev->device == SYNCLINK_GT2_DEVICE_ID)
		port_count = 2;
	else if (pdev->device == SYNCLINK_GT4_DEVICE_ID)
		port_count = 4;

	/* allocate device instances for all ports */
	for (i=0; i < port_count; ++i) {
		port_array[i] = alloc_dev(adapter_num, i, pdev);
		if (port_array[i] == NULL) {
			for (--i; i >= 0; --i)
				kfree(port_array[i]);
			return;
		}
	}

	/* give copy of port_array to all ports and add to device list  */
	for (i=0; i < port_count; ++i) {
		memcpy(port_array[i]->port_array, port_array, sizeof(port_array));
		add_device(port_array[i]);
		port_array[i]->port_count = port_count;
		spin_lock_init(&port_array[i]->lock);
	}

	/* Allocate and claim adapter resources */
	if (!claim_resources(port_array[0])) {

		alloc_dma_bufs(port_array[0]);

		/* copy resource information from first port to others */
		for (i = 1; i < port_count; ++i) {
			port_array[i]->lock      = port_array[0]->lock;
			port_array[i]->irq_level = port_array[0]->irq_level;
			port_array[i]->reg_addr  = port_array[0]->reg_addr;
			alloc_dma_bufs(port_array[i]);
		}

		if (request_irq(port_array[0]->irq_level,
					slgt_interrupt,
					port_array[0]->irq_flags,
					port_array[0]->device_name,
					port_array[0]) < 0) {
			DBGERR(("%s request_irq failed IRQ=%d\n",
				port_array[0]->device_name,
				port_array[0]->irq_level));
		} else {
			port_array[0]->irq_requested = true;
			adapter_test(port_array[0]);
			for (i=1 ; i < port_count ; i++) {
				port_array[i]->init_error = port_array[0]->init_error;
				port_array[i]->gpio_present = port_array[0]->gpio_present;
			}
		}
	}

	for (i=0; i < port_count; ++i)
		tty_register_device(serial_driver, port_array[i]->line, &(port_array[i]->pdev->dev));
}

static int __devinit init_one(struct pci_dev *dev,
			      const struct pci_device_id *ent)
{
	if (pci_enable_device(dev)) {
		printk("error enabling pci device %p\n", dev);
		return -EIO;
	}
	pci_set_master(dev);
	device_init(slgt_device_count, dev);
	return 0;
}

static void __devexit remove_one(struct pci_dev *dev)
{
}

static const struct tty_operations ops = {
	.open = open,
	.close = close,
	.write = write,
	.put_char = put_char,
	.flush_chars = flush_chars,
	.write_room = write_room,
	.chars_in_buffer = chars_in_buffer,
	.flush_buffer = flush_buffer,
	.ioctl = ioctl,
	.compat_ioctl = slgt_compat_ioctl,
	.throttle = throttle,
	.unthrottle = unthrottle,
	.send_xchar = send_xchar,
	.break_ctl = set_break,
	.wait_until_sent = wait_until_sent,
	.set_termios = set_termios,
	.stop = tx_hold,
	.start = tx_release,
	.hangup = hangup,
	.tiocmget = tiocmget,
	.tiocmset = tiocmset,
	.proc_fops = &synclink_gt_proc_fops,
};

static void slgt_cleanup(void)
{
	int rc;
	struct slgt_info *info;
	struct slgt_info *tmp;

	printk(KERN_INFO "unload %s\n", driver_name);

	if (serial_driver) {
		for (info=slgt_device_list ; info != NULL ; info=info->next_device)
			tty_unregister_device(serial_driver, info->line);
		if ((rc = tty_unregister_driver(serial_driver)))
			DBGERR(("tty_unregister_driver error=%d\n", rc));
		put_tty_driver(serial_driver);
	}

	/* reset devices */
	info = slgt_device_list;
	while(info) {
		reset_port(info);
		info = info->next_device;
	}

	/* release devices */
	info = slgt_device_list;
	while(info) {
#if SYNCLINK_GENERIC_HDLC
		hdlcdev_exit(info);
#endif
		free_dma_bufs(info);
		free_tmp_rbuf(info);
		if (info->port_num == 0)
			release_resources(info);
		tmp = info;
		info = info->next_device;
		kfree(tmp);
	}

	if (pci_registered)
		pci_unregister_driver(&pci_driver);
}

/*
 *  Driver initialization entry point.
 */
static int __init slgt_init(void)
{
	int rc;

	printk(KERN_INFO "%s\n", driver_name);

	serial_driver = alloc_tty_driver(MAX_DEVICES);
	if (!serial_driver) {
		printk("%s can't allocate tty driver\n", driver_name);
		return -ENOMEM;
	}

	/* Initialize the tty_driver structure */

	serial_driver->owner = THIS_MODULE;
	serial_driver->driver_name = tty_driver_name;
	serial_driver->name = tty_dev_prefix;
	serial_driver->major = ttymajor;
	serial_driver->minor_start = 64;
	serial_driver->type = TTY_DRIVER_TYPE_SERIAL;
	serial_driver->subtype = SERIAL_TYPE_NORMAL;
	serial_driver->init_termios = tty_std_termios;
	serial_driver->init_termios.c_cflag =
		B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	serial_driver->init_termios.c_ispeed = 9600;
	serial_driver->init_termios.c_ospeed = 9600;
	serial_driver->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
	tty_set_operations(serial_driver, &ops);
	if ((rc = tty_register_driver(serial_driver)) < 0) {
		DBGERR(("%s can't register serial driver\n", driver_name));
		put_tty_driver(serial_driver);
		serial_driver = NULL;
		goto error;
	}

	printk(KERN_INFO "%s, tty major#%d\n",
	       driver_name, serial_driver->major);

	slgt_device_count = 0;
	if ((rc = pci_register_driver(&pci_driver)) < 0) {
		printk("%s pci_register_driver error=%d\n", driver_name, rc);
		goto error;
	}
	pci_registered = true;

	if (!slgt_device_list)
		printk("%s no devices found\n",driver_name);

	return 0;

error:
	slgt_cleanup();
	return rc;
}

static void __exit slgt_exit(void)
{
	slgt_cleanup();
}

module_init(slgt_init);
module_exit(slgt_exit);

/*
 * register access routines
 */

#define CALC_REGADDR() \
	unsigned long reg_addr = ((unsigned long)info->reg_addr) + addr; \
	if (addr >= 0x80) \
		reg_addr += (info->port_num) * 32;

static __u8 rd_reg8(struct slgt_info *info, unsigned int addr)
{
	CALC_REGADDR();
	return readb((void __iomem *)reg_addr);
}

static void wr_reg8(struct slgt_info *info, unsigned int addr, __u8 value)
{
	CALC_REGADDR();
	writeb(value, (void __iomem *)reg_addr);
}

static __u16 rd_reg16(struct slgt_info *info, unsigned int addr)
{
	CALC_REGADDR();
	return readw((void __iomem *)reg_addr);
}

static void wr_reg16(struct slgt_info *info, unsigned int addr, __u16 value)
{
	CALC_REGADDR();
	writew(value, (void __iomem *)reg_addr);
}

static __u32 rd_reg32(struct slgt_info *info, unsigned int addr)
{
	CALC_REGADDR();
	return readl((void __iomem *)reg_addr);
}

static void wr_reg32(struct slgt_info *info, unsigned int addr, __u32 value)
{
	CALC_REGADDR();
	writel(value, (void __iomem *)reg_addr);
}

static void rdma_reset(struct slgt_info *info)
{
	unsigned int i;

	/* set reset bit */
	wr_reg32(info, RDCSR, BIT1);

	/* wait for enable bit cleared */
	for(i=0 ; i < 1000 ; i++)
		if (!(rd_reg32(info, RDCSR) & BIT0))
			break;
}

static void tdma_reset(struct slgt_info *info)
{
	unsigned int i;

	/* set reset bit */
	wr_reg32(info, TDCSR, BIT1);

	/* wait for enable bit cleared */
	for(i=0 ; i < 1000 ; i++)
		if (!(rd_reg32(info, TDCSR) & BIT0))
			break;
}

/*
 * enable internal loopback
 * TxCLK and RxCLK are generated from BRG
 * and TxD is looped back to RxD internally.
 */
static void enable_loopback(struct slgt_info *info)
{
	/* SCR (serial control) BIT2=looopback enable */
	wr_reg16(info, SCR, (unsigned short)(rd_reg16(info, SCR) | BIT2));

	if (info->params.mode != MGSL_MODE_ASYNC) {
		/* CCR (clock control)
		 * 07..05  tx clock source (010 = BRG)
		 * 04..02  rx clock source (010 = BRG)
		 * 01      auxclk enable   (0 = disable)
		 * 00      BRG enable      (1 = enable)
		 *
		 * 0100 1001
		 */
		wr_reg8(info, CCR, 0x49);

		/* set speed if available, otherwise use default */
		if (info->params.clock_speed)
			set_rate(info,  for->params.clock_/*
 * ;
		elseDevice driver for M3686400ial }
}

/*
 *  set baud rive generator to specifieCorpor
 */
static voidcrogdriverstruct slgt_ for * for Mu32orpor)
{
	unsigned int div;rporation
 *
 *osc =Microgabase_Link ;

	/** Th =ased/rpora- 1
	 * *
  Roundblic up ificense (GPis not *
 egepaul* THIforceaulknexare owestorpor. *
 /

	if (gate  {
		lic License (Gal aNG, !(sed %ogate  &&* Th Devidiv--al awr_reg16r for MBDR, (oration
 short)MERCFulghum  * Microgaterx_stopcLink are trademarks of Corporation
 RPOSE valral Publisable and rerogareceiverNCLU	val = rd FITNESS FOR RCR) & ~BIT1; INCLUDING/* clear en INCIbitXEMPLND FITNESS FOR RCPARTICULAR PURPOSE (ARY,| BIT2));, BU, SPECrogatTO, PROCUREMENT OF SUBSTITUval)(INCLUDING,CLUDING, BUT NOT DATA, OR PROF
	e tradrq_offr for MIRQ_RXOVER +ER IN CDATAACT,
 * SIDLE)ral PubT NOT pending rx' ANDrrupts, PROCUREMENT OF SUBSSRHER IN CTY, ACT,
 * SONTROR TOrdma_DATA,r forOR TOicrogarx_LIMITEd = fals IMPDVISED
 *restart POSSIBILISCLAIMED.  IN NO EVEart SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDF LIABILITY, WHETHER IN CONTRACT,
 * STRICOR TORT (INCLUDING NEGLIGovE ORnER IHERWISE)
 * ARISING IN ANY WAY E OF THIS/* ON ANYDENTCT, INCIECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (I BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY O SOFTWARE, EVEN IF	DATA,_rbufs EVEN IF ADf r forED
 *pioNOT LItom x requRRANwhenINFOFIFO IS''emptyXEMPLISE)
 * ARISING ITITUTE GOODS OR
 * SER CONSEQUENTIALSDAMAGES
 * 4)ial aF LIABILITn WHETHER IN C  inforPLIED icrogate Syncmode == MGSL_MODE_ASYNCNOT LIEL_ILIMITEDsav NEGofINFO * MuTHERWIf (debug32 OF SUBSDCN ANBIT6 prin}
	} dapt_LEVEL_INFO) printk fmt
#define half fullt) if (debug_level >= DEBUG_LEVEL_ERROR) printk fmt
#define DBGES; L) if (deF USE, 1st descrip* paaddresdefine BGDATA(info, buf,DAR MicrogaFO(fm[0].pUF(iOR TObug_level >fine DBGISR(!mt) if (debug_level >= DEBUG_LEVELrx DMA servncluENCE OR Oefine DBGDATA(info, buf, size(; LO +e, l0if (de(debug_leveDEBUG_LEVEL_ISR) printk fmt
#d,>
#include <linux/timer.h>
#include <linux/interrupt.h>
#i6clude nclude <linux/pulghfic types of>= DEBUG_LEVELH     bottomched.h>
CIAL, EXEMPLCUREMENT OF SUBSTITUTE GOODS OR
 * SER CONSEQUENTIAL DAMAfs(infif (ITY OF SUCH DAMAGE.
 */

/*
ADVISED
 * OF THE POtru/*
 * DEBUG OUTPUTtDEFINITIONS
 *
 * uncomment lines bIED Wevel >t * OF THENOT LIND FITNESS FOR TCR,inclUTE GOODS OR
 * SE <linux/mm.h>
#iTclude <linuAGES
 *OSS /errlloc.h>
#includlinux/smpx/strug_level >tx_countNOT LIs.h>
#drop_rts_on_tx_done POSSIBILI/errno.h>
#include <linux/signal.h>
#include <linurno.h>
#include <flags & HDLC_FLAG_AUTO_RTSel >= D	get_atioalmt) if (d <asIED WAs.h>
#a.h>
#i & SerialS.h>
##incllude <aseue.h>
clude <a|=m/uaccess.h>

#if <asm/ice da.h>
#include <asm/eue.h>
#include <linux/hdlc.hx/bitop	linuxlinuDevic LIABILITY, WHETHER INTL_BH) prinebug_level >= DEBUG_LEVTXUNDTRACT,
 *TITY, OR ncludeT NOT tx idNCIDENTundtput
 fmt
#debiOTHERWIf (debug_level >= pt.h>E GOODS OR
 * SEfication
 ntificatie idelinux/pci.h>
#incl1
#else
#define SYNCLINK_GENERIC_HDLC 0
#endif

/*
 * modulon
 */
static char *driver_yncLink GT;
static char *tty_driver_nfication
 */
stNERIefine DBGRBUF(info) dump_rbufsDENTAMAGE.nclu) if (debug(info, buTnux/module.ht
#incICROGATE, EFINITude <linux_ID,},
	{PCI_VENDOR size, lnclude <lrmios.h>
#incactivne SYNCLINKDISCLAIMED.  IN Nde <lNT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDdel_timer(&s.h>
#incE_ID,x/seqtSOFTWARE, EVEN IF Atom half service routtransmitt EXEMPLARY, OR CONSEQUENTIAL.h>
#GES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBludeUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFfic types of debug outputK_GENEntification
  char *tty_dev_R TORT (INCLU*driver_name     = "SyncLink GTHERWISE)
 * ARISING IN AName = "synclink_gt";
static char *tty_dev_prene DBGINTE, S EVEN IF ADVISEDinclude <linuSSIBILITY OF SE, SYNCLINK_*/

/*
 * DEBUG OUTPUT DBGINponux/netdevice.h>
#include <linux/vmalloc.reg_mp_r DevreturnANY_I EVENT 
#define vel;
static inslab.h>
clude <as= ~(/uaccess.h>

DTRACT/uaccess.h>

#if ;
ULE) && defined(CONFfic types of debug outputALL |PCI_VMASTnk_gtgt_device_list;
statiadapterx/netdevice.h>
#include <linunt i;
	for (i=0; i <Microgatortinux/w; ++iorkqueuk fmt
#defE_PAarray[i] Devi
static int "Debug syslog outpuDEVICE_ID, PCI_ANY_Iasync_GISR SHALL THE AUTHOR BE LIABLE  below to enable specific types of debug output, int, NULL, 0);

Mlevel;
static int maxframe[MAX_DEV/*_id  (tx control) *
 * THI15..13  GISR, 010=DESC(pen(st2..10  encoG NE, 000=NRZ* THI09OWEVERparityclude <id clo8OWEVER1=odduct tty, 0=evenuct ttyid clo7struct fRTS drL, EXtatic iid clo6struct fbreak_struct *tty,5..04  characE_TAlength* THIos *old_00=5k GT"ermios *old_te1=6os);

static int  10=7e(struct tty_struct1=8os);

stat03*old_te=1 PCI_k GT, 1=2tatic intuf, int2os *ol
statf, int1os *olstruct *tty,0os *olauto-CTSar ch);
staMPLARY, O0x4000(debug_level >ifmaxfr &t) if INTERFACE#inc_EN DevRVICE=e, l7(debug_level >te Syncct tty_!= g_lev_PARITY_NONENOT LI_struct *t9printk fmt
#define DBct tty_=write_room(struODD Devi_struct *t8tops.h>switch fmt
#define DBdata_truc)
	{
	case 6:ERRUruct *t4; termi;c void 7x_hold(struc5 tty_struct *tt8);
static voiclude t tty_strucs.h>
#include te SyncPCI_y);
s  wr1 tty_struct *t3y, int timeout);
stat
#include <asm/irq.h>
#CTS tty_struct *t, chauct pci_device_id *eRRUPTial_drRver;r
static int  open(struct tty_struct *tty, struct file * filp);
static void close(struct tty_struct *tty, struct file * filp);
static void hangup(.. voi
starved, must be 0ct tty_struct *tty, struct ktermios *old_termios);

static int  write(struct tty_struct *tty, const unsigned char *buf, int count)
/*
 * generic HDLzerEXPRES_struct *tty, unsigned char ch);
static void send_DCDar(struct tty_struct *tty, char ch);
sta);
static int  write_room(struct tty_struct *tty);
static void flush_chars(struct tty_struct *tty);
static void flush_buffer(struct tty_struct *tty);
static void tx_hold(struct tty_struct *tty);
static void tx_release(struct tty_struct *tty);

static int  ioctl(struct tty
#include <asm/irq.h>
#DC);
stt  chars_in_buffer(struct tty_ INTERRUPTial_drCver;Link static int  open(sstate5  wr1e SYx t_condsouOR Iis BRG/16 cond_4.._str010, ORned int data);
static unsigned chat(stauxclkvice roud
static void unsiBRGar ch);oid r unsign10 100
 *
 e.h>
#inc8r for MCd *e0x69atic msc_BGINvcrer *serial_drSver;suaccestatic int  open(strct ftFO) p on) trace_bloRR(fmpen(st4ct fNFO) ptor and accessck((pen(st3 igne *tt  * DBstruct *tty12__le1iver_tus;
	__le32 pbuf1 tructermioo
 * DB	__le32 pbuf0a buf6 status;
	__le32 pbuose(buffer */
ffdress of next des8  output
 dress of next des7  DSRos *olress of next des6  xchaual  address of data 5  f, iual  address of data 4  RIos *oldress of next des3t);
s6x sampl);
st1=8f_dma_addrstatic vo desdED
 
 *
 ernal loopbacios(struct ttyta bct slgt_info *info);
static0counmasE_TA_le32 next;  /MPLARY, O(infct *tty14clude <;al_drJCR[8] :nd_wax8_DESC(ty_st feature avail INCInextvel, <linux(info, buJDAMAGEd fl* OF ct tty_struct *ttyrpora&&
	 /* ( 1 to 5e GNU Gene <ruct tty_struct *ttyrpora* 16)) ||f(a,b) fmt
#deatus = cpu_%o_le16((b) ? (le16_to_cpu((a).s define/* u(strf_dma_addr_ANY_Ict file *filevice driver for Microgate Synce16_to_cpu((8etaildebug_level >tatubuf_dma_addr_ANY_I((unsigned short)(b))
#define desc_count1abel)}WISE)
 * ARISING Itruct *tty);ing.h>
#include <linux/fBREAKnt, NUL/fcntl.h>
#_init(struct slgt_desc_bu DevLIMITE__desc_butatic inSCLAIMED.  IN NESC(maxframe, "Maximum frame size us by device (4096 to 65535)");

/*
 * tty support and callbacks
 */
static struct tty_driver *serial_driver;

static int  open(struct tty_struc00=e <a 001=rawuct *tty,  **h=monori_up100=bity, struct file * filp);
id close(struCRC_struct *tty, structCRC32hangup(struct tty_struct *tty);
static void sepreamINCIs(struct tty_structstructureuct ktermiot count)share open/close 
#int tty_struct *tty, unsigned char ch);
static void send_xchar(struct tty_structPCI_moduler(stfmt
#define DBGISRNOT Lvoid ) if (debuMONO_levx_hold(struc= cpu_to13 tty_struct *tt) if (debuBIstruct t pci_dev *pid tx_release(st) if (debuRAWs on on adapter */nt port_cou}har ch);
static void wait_until_sent(struct tty_struct *tty, it link */

	int magi filp);
tatic void e <asENCODING_NRZB instanstance number *0 tty_struct *tt slgt_info *port_I_MARK insta pci_dev *p1	int			line;		/* tty line instancrray[SLGT_MAX_PORTS];
1ev;

	i
	int			line;		/* tty line insBIPHASEe number pci_dev *p2haracter */
	unsigned int		read_statuSPACEct pci_dev *pnclude  character */
	unsigned int		read_statuLEVEL_t	status_event_wait_gsl_icount	icount;

	int			timDIFFait_q;
	struct timer_list	tx_timer;
t_wait_q;
	wait_qu_buffer(struct tty_structcrc_typid we <asCRCLL, K */
	struct slgt* sp16_CCITTx_hold(struc9	int			line;		/* tty* sp32ith ISR */

	structclude 8;

static int  ioctl(struct ttystructure!=ock;	/PREAMBLE_PATTERNuct ttatus = cpu_to6to port coning;

	int isr_overflo_uct kt */
	struct slgtirq_requeLENGTH_16BITS);
static void tx_release(st/

	/* device configu32tion */

	unsignt tty_struct *tt/

	/* device configu64tion */

	unsignet cond_wait {
	struct cond_wait *next;
	wait_queue_head_t q;
atic int  chars_in_buffer(struct tty_struct *tty);
stTPer;
E_DEVICnfo {
	vo));
stater(struct tty_structmeters */tics use */

	/* device sted;	/*m/irn */

	ct *7e irq_flags;

	unsigned char _sted;	/*ONESsk;
	unct *ffint rbuf_fill_level;
	unsigned int rxZERO/

	unsigne0
	int			line;		/* ttyirq_requested;	/*10 instunsigned5ed int bus_type;
	unsigned intsted;	/*01rt;

	bool txaa tty_strucdefaultrray[SLGT_Mint init_error; 	unsigned int rbuf_f#define dit *w);
TPvexit_p(remov *tt)t *tty);
static void throttle(struct tty_struct * tsignal_events {
	int ri_up;
	int ri_down;
	int dsr_up;
	int dsr_down;
	int dcd_up;
	int dcd_down;
	int cts_up;
	int ct..ddr_
/*
 * generic HDLC suppo void hdlcdev_rx(struct slgt_info *info, char *buf, int size);
static int list link */

	int magic;

	char device_name[25];
	struct pci_dev *pdev;

	int port_count;  /* count of ports on adapter */
	int adapter_num; /* adapter instance number */
	int port_n port contexts on this adapter */
	struct slgt_info *port_array[SLGT_MAX_PORTS];

	int			line;		/* tty line instance number */

	struct mgsl_icount	icount;

	int			timeout;
	int			x_char;		/* xon/xoff character */
	unsigned int		read_status_mask;
	unsigned int 		ignore_status_mask;

	wait_queue_head_t	status_event_wait_q;
	wait_queue_head_t	event_wait_q;
	struct timer_list	tx_timer;
	struct timer_list	rx_timer;

	unsigned int            gpio_present;
	struct cond_wait        *gpio_wait_q;

	spinlock_t lock;	/* spinlock for synchronizing with ISR */

	struct work_struct task;
	u32 pending_bh;
	bool bh_requested;
	bool bh_running;

	int isr
	wait_queue_head_t q;
	wait_queue_t wait;
	unsigned int data;
};
static void init_cond_wait(struct cond_wait *wgned int data);id add_cond_wruct cond_wait  unsigned cha *w);
sr ch);
static void it(struct gnal */
	int dsr_c   = HDLC_ENCODING_NRZI_SPACE,
	.clTXC_BRGe;    	//k fmt
RxC data);
staDPLL,ONE,
tion
 *es 16XYNC_P,
	.pareferenceed int, so take TxC fromatic voaulkgy, u	.paions para= cpu_at actualcal ade.com#include <asm/io.h>
#include <asm/irqRXC_NC_P;
static void fmajor.h5;sc_s011, tw);
swait( BH_
statdapters. IRQ requestY  BIT0
#define MASKAMING}
 BIT0S PR.data_bits       = 8,
	.stop_bits   IZE 4096_struct *ttysc_s10BIT14
#defiNC_P Input_one,RRUN BIT4

#define GSR   0x00 /* global statRXCPIt tty_struct *tITY  BI01
#define MRXC*/
#define00,
	.data_bits       = 8,
	.stop_bitST_S    = us = cpu_to_lEAK   BITr4
#define MASK_RRUN BIT4

#define GSR   0x00 /* global ST_SIZE 4096hold(struct 0x04 /* Jfine TDRrol *x80 /* tx data */
#define RDR   0x80 /* rx data T 0x0c /* GPIO inter2upt enablefine TDRTne IOVR  0x10 /* GPIO value */
Link GT seriefine TCR   0
	strucn_buffer(stit *w);
stati *tx_buf;
	int tx_countBIT4

#define GSR   0x00 (/* global status * +x80 /* rx data */
#d = 1,
	.paprogram*/
#deGISRine rt contexts on this adapter */ 1,
	/
	unsigned int		read_status_masontrol/status */
#define TDCSR_head_
#definect *ttytty_strucueue_head_t	event_wait_q;
	struct descriptor address */

	unsigned int      RDDAR 0x98 /*clude 6 rx DMA designal staAR 0x98 /BREA/ NRZ* filp);
s_MICROG>
#include <linux/ioport.h>
#include <linux/mm.h>
#include RRUPnux/er//*/
#de) prires a
#defEIVE  1
#define2
#defineine desc_status(a)     (le16_toLink GT serstatus))
#dedaptersce driver for Microgate SyncLink GT serial*/
staet_iver, 0);
moduush_cond_wait(struct cond_wait **head);

/*
 * DMA buffer descriptor and access macros
 */
struct slgt_desc
{
	__le16 count;
	__le16 status;
	__le32 pbuf;  /* physical address of data      = "S_le32 next;  /* physical address of next descriptophysical address of dping */
	char *buf;          /* virtual  address of data buffer */
    	unsigned int pdesc; /* physical address of this descriptor */
	dma_addr_uct slgt_info *info);
static vount;
};

#define set_desc_buffer(a,b) (a).pbuf = cpu_to_le32((unsigned int)(b))
#define set_desc_nextfine desc_complete(a(a).next   = cpu_to__eof(a)        (le16_to_cpu((a).status) & BIT2)
#define descfor Microgaions paraiver_statu
 *
 * MicrogateIRQ_CTS     SHALL THE AUTHOR BE LIABLE FOR ANY D *tt,
 * I FOR ANY DIRECT,tcrtic voiif structure
 */
sd (tcr[6]uct 1) tfmt
atic struizlc.har *buf, inRRUN *inf5:4]nsignestartup(s: intruct slgruct = 1te(struct /
	tcr, pci_table);

static /sla ch);
statdlec void we <asne MAX_CUSTOM_16et_desc_sCT, INCIstructur,USE, startup(sttoy_structAMING struct *inint nux/major.h5.sta long desc_sMSBprin_struc progra@microgatiint  structureregi
#def(TPR/
	u3
#defineunsigned char *tx_buf;
	int   (a).stnfo);
stat>>  shoned itermi    (leIED WA);
sta, labet_desc_sstructureisstatic vo void program_hw(star *bugt_info *i_para memory maps))
#define desc_complid *etcI_ANY_gt_info *info);
staticne SCR shutdown(stru8 |c void shutdown(struct et_desc_sLslgt_icustomnt  startustatic int  regiver_t(struct status = = irq_test(struct lgt_info *info);c int  oopback_te_desc_sstandard_info e(strupaCE_Tn*info);*/
#define RDnfo);
staDMA control/status shutdoig */

_error;  /* initialization erruct slgt_info *inALT;

	/*x_pio; slgt_info *info, unsign nume_head_t	sta;    /* serial sruct slgt_info *in

	/* lgt_info *info);
sta_head_t_error;  /* initiatus */

	bosignal states */
	int init_error; unsigned inl) if (  0x89 /* clock cTIruct *ttyinfo *infgetruct eprinV24ruct pci(i
#de) a.h>
#istatic void  wr_rm/dma.h>
#inSHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,fmt
#de OR CONSEQUENTIALSSk_gt",
	.id_taball t **heaclude <aexcept nt, DENTty_s(a).cCES];

module_pam(ttymajor, int, 0);
module_param(d;
staticfmt
#deslgt_3DMA ONFIG_HDLC) || (defined(CONFIGDSR slgt_iatic void td2a_reset(struct slgt_info *info);
sCHDLC_void tx_load(str struset(struct slgt_info *info);
stCDunsigned int count)0

static void get_signals(struct sRI;
static vorogaV.24 Catic i R(struct e GNd/
	_current*ttyfigurationstatic void  wr_rush_cond_waislgt_info *info, unsigned int addr, __u32 vant dsr_c/* Vver;oid e_wait(struct cond_wait4icro**heaIF selec, unsig count)DTRstatic void hRTS unsigned chae BHtatic void Rt slgu32 idle_mh);
static void wait_until_sent(inlock for syncait_until_sent(sS232ool 
static void t enab0info *nt adapter_num; /*ntil_sent(V35lgt_info *info    BIT1SK_PARIT
sta1*heaoid isr_txeom(struct slgt_info *iRS42slgt_info *info6;
stat10 isr_tdma(strucs.h>
#include erial(struct slgt_info *inSB_FIRSTcontrol */
#des(stgt_info *iclude <asm/uaccess.h>

DTRstruct file *fileic int  alloc_desc(struct slgt_infinclpreamble */
#de*info);
static void free_dma_bufs(str
#define TCR   01oc_bufs(struct slgt_info *info, structR#define TCR   0_le3info);
static Vtruct *ttyfo);
static vx_stop(structtatic i (out *info);
static void tx_set_iLE) && defino *info, u32 data_rate);

static int  bh_actionR CONSt slgt_desct slgt_info *ioc_desc(struct slgt_info *info);
static voiddaptersacti_paric void free_desc(struct slgt_info *info);
static int  alloc_context);

/*
 * iloc_ struct slgt_desc *bufs, int countfree rangop(stinux/pt PCI_buffer_infw(stlast)static void  wr_rt  gNFO(fmtLink are trademarks of Miration
 *
 *it slgt_info *inSL_PARerride:/hdlc.hn_buffhile(!/hdlG_LEVEL_INATA, _info *iinfo *SS O retatunfo *dule.h>
#inci].tbuf_coun_le3XOVERUF(iinux/wlevel >=static module.h>
#i_fill_levetatilevel, uct SL_PARANTAhdlc.h, inic in++e(strle_mode);
snux/wo>
#int_txidl_num
static intinfo *i=: 0=info *infmarkic unstruct slgt_info *inasams);static void  wr_r DBGINFO(fmtSHALL THE AUTHOR BE LIABLE ms);
static for M0idle_mode);
snux/wPL)
t slge_mode);
staticindext_txidlct slgt_info *insl_eve_txidinfo *infpassuct slgt_nal_eframhw(stuppstruayer
 or Mint debSYNCLest(*tty, count(a,b, otherwiinteSIBIstatic void boolNO Em/dm*tty,ree_tmp_rbuf(struct slgt_info *info);ide:FINIT, endlue);

static voidfmt
#dis code is relea*tty,up(stru_le3slgt_infolongter (ty, Link artty_Link ar*ttruc);
MODULE_P.ttynt  get_inte__u32mp_r_fiellinu void tslgt_info *ininlostatic int     *gpio_wait_q;

	spinlock_t lock;	/* spinlocchar devihronizing with ISR int if_modeint 		ignore_status_mu32 pending_bhint if_mode);

static intcheck_again:
nfo,);
static int nterface(struct slgtAMAGE.
 enlinugt_info *info);
st;
stuto a;;bug_level,!uct slgmplever forinfo, inend]ne S		gotoc chanupclude <lint  wait_gp= 0hort)(b))
#definenterfaclE_TA!ruct sstrucnterface(struct slgt_indevice.buf[0]cludent  wait_g+=BUF(islgt_info *info, invice(clude <liinfo)eo, WHETd add_device(structy_struo, int en gpioe);
static int  tx_abor gpio_);
stic inDE
 */
#ifndef DBGIinfo *m.h>
#include <as
 * OF THE)de <asspin_ink Girqsave PCI_ANYfine 
#ince <asm/ DEFINITIclude <asm/GBH
#undefine D DAMorH(fmt)
#endif
#ifndef DBGNERICt slgt_info *ilinux/str void tu

staMA buffer counfo, in*/
statios
 */
ate);

/*
 * gct tty_structresidueamble struct eot)
#endoocmset(nt  oigned cha	int rror
static void sbor, uns/fo, sf_couninfo)t > 16tic void release_resouet_vgnore	whil(struf IS''us)
#d	int((strusinfo)finude (a).count  set_gpio(struct slgt_info *info, stru=
	bool * sp true ifatic voi*
 * i1;
statictatic void deviatus	 (nterface(stuct pci OF "%c",data[i]);init(int adapter_num, ste(struco,int arg);
statint set_bre tx_et slgtpio);
statinfo *info)nt  wait_g< (ncluint if_mstat_tbuf_costatic rkqueue.h>
inux/w.rxRPOSE++nfo nt  wait_gpio(stback_test(sd int count);
DBGTBUF
static void crctbufs(types.h>
#inq;

	spinlock_t lock;	/* spRETURN_EX(strucstruct slgt_info 

#if _levLINK_GENERIC_e <a& data[i]<=0176)
			k("tbuf_currnetdev->t > s.
 * (cousnfo->tbs[i].count), le16_to_cnt  wcpu(info->tb}
#ING f

	DBGBH(("%sGTH_*tty, t > 16=%04Xtup(s=%d\n"e <lue.h>
#evice_namc vode <linnt  wait_termiDBGTRICr for Microgafo, inDEVICE_buf, min_ine tint i;
	priidle_mode);
static int  , "rx");
statict i;
	prinver functiurrent);
	for (i=0 ; i < info->tbuf_count ; i
#inclnt  wait_g-=size, lab/
staint if_mode);
IC_HDLC DBGDATA(info,>.");
		ma#else
#dt_infosize, labe>
#inF
static void rfactbufs(i.h>
#includecopy dmant, con(s)w(sttatiiguous tempnt, cons
statifo, iopystatic innt  wait_/
staide:  =	count/
static int  set_i*patic int tmpNFO(fstruct  name);
		re {
		printk("null stnity_check(struct soktbuftatict  genfo) {
		plude <aside:partia
static in);
	for (i=nfo) {
		pidle_mode);
static int  tx_e		memcpy(p=%d\n", info->icurrent %s\n", devnae <asm/pfo * %s\n", devnaINK_GEnfo) {
		pr-k wrappers
 *
 * The int enable);
static int  tx_aborort(structIC_HDLC 
#define VCR   0x8i=0 ; i < info->tbuf_count ; ude <asevnam	int i;
	printk(? RXt;i++ERROR :tic OKINK_GEnt  wait_tbufs( count=%04X status=%04X\n",
			pline.
 *
 *netalling intohdlcdev_rx;
stat1;
	}
	if (infint i;
	prinine ddapte(info)
nto ldisc_inux/pt_buf(tlp);_ldisc *ld;
	if 1;
	}

#inif (f (!tty)
		returnux/s  += linecount;
		count -= linectatic int  ;

t_info :ty_ldisc_*/

/*
 * cmget(struct tty_stnfo, in(RAWa)  (hronifdec;

	ctonfo ile *filestatic int  tiocnfo, intruct tty_struct *tty, struct file *file,
		     uf (ld int set, unsigned int clear);
static igt_idesc __user *gpio);
t_info *info, i *
 * inux/vmons
 */
static void add_deie(stru/* tty callbac	tatic inuct slgt_info *info, int ebug_ link */

	int magic;

	char device_name[25];
	strucount;  /* count of portsdesc_st; i++)printk(nt  byte_struct *tty, str*info);ces(strucprintk(", driver_name, lineENODEVLITY Aation errorrbuf_current=%d\n", info->
/**
 *  1;
	}
	printk>devINFO(("

	line = d dump_rbufs		retutermigt_it  tx_aboc_ref(tty);
	if (ler *if_mode);
 info->init_error));
_ldis receive_buf)
			lcipline co,int arg);
statii, i	tty_ldisc_deref(gt_device_list;
stati
 * moty->index;
	if ((line < 0) || (line >= sl
static st *info);
stati}
#euto assi gned");
MODU		if (l_eve; i++k("tbuf_currTE, SYN int  set_txidle_on(&info->portterrup(struct sks */

sttatic inumber>deviceDULE_DEVICslgt_info *istatic void int break_stateeety_huslgt_ind int set, unsigned int clear);
static itatic int   || (line >= slgt_device		if (info->AC_DEo= 1,
	ces(struclgt_info *innfo->por
/*
 * DEBUG
stanfo, inincpu((nfo *++
 *
 * Thint enable);
stat.tty->low_anity_=info *ber sble((".");
				if (info->atic voiifnt  ncludYNCLI,ruct fo);
LATENCY	spin_unsunlock_irqre.tty = tt OF t  = cpu_to_le16D,},
nsigned0(stru--en with itatic ien with& ASYNC_HUP_NOTIFY) ?
			-en")rt.counso *iN : -ERESTARTSYS);
		gouct stheandler(static ilANY define	goto cleanup;
	}

	info-		if goto latency = (info->port.flags & ASYNC_LOW_Ltot", devname,? 1 : 0;

	spin_lock_irqsave(&info->netlot_info *info, or;
valbitop& ASYNC_LOW_LATENC
			info->port.t>netloe = static int  ecou slgtAddBUF(info) duterrusnt __c un(&info-info *i, INC IfLATENCYisags);
(T NOTed byinfo-info);
	if afE_TAread), slgttty,info->port*/
stati o>port>netloly be NEGLearamsom, INC* THISecord retnterrupofk, flanfo, inwithags);

	if (FINITn;
	int 
#deffo);
star)
#dpositic .*filp)
{
	sis mi(count) {evnaminfo	/* 1DENTAS IS''rc=%d\n", i filp, info);
	if
	}

	DBGnfo->device_nam close(st, tha (info->portose(stru slgtt tty_strucsoe
		ct fioto , INCLU	dotaticNODEV;
	}

	info = slgt_d) {
		retfo, int to cleanupgoto cleanup+=tty = NUL /* tx dat!goto cleanu_dev *r will release tty_on(&info->portfilp)
{
	nfo->netlock, flags);
		goto cleanup;gt_info *fo->port.count++;
	spin_unlock_irqreststruc(&info-t > 16)ct slgt_info unt == 1) void tx_ this device, iirqrestore(&info->netlock, flags);

	if (info->port.count == 1) {
river->namstatic voitil_sent(tty, i	tty_ldisc_flushtic voi) == define tatic inunt == 1)[truc8]tty, sil_sent(tty, i *info = tt
static int irqrestore(E_DEVICE_TASYNCLIN) ==hdlcen"))t __shifcountslgt_info 
#include <li	tty_lver_data;

	if agic ntatic inoto cleanuinfo);
		iflolose : -ERESTARTSYS);
har  *inf *ttstatic void  wr_reg3erruc int  set_params(struct const\n", de
			lar);
static int san)
#define desc_aboty = NULL; /* tty layCLOSLink are traUF(i *dnfo);
sta_cpu(infoc int debug_lrbuf_current=%
			l < inf"trintk("%CLINK_GT2_DEVIC(tty(tty);

	shutnfo->netlockfo->por=%04X statlinuPCI_ANY_it_errotty_port_close_end(&info->port, tty);
	info
 	if (infoux/delay.h>
#incluRBUF(inDMABUFSIZEstat_RTS + Ser :
{
	sttime;
#endid->
			lt.count));

			ifpu(info-;

	tty_r_day, info->tidesc_intkatic vEOF	print __ct slgt_infooftruct *tty, ountPRESS O every		spin_unlos {
status ;
	if (inf(!	set_ort)(b))
#defineGISR(fmt) if (debue <astatus(a,b)mt
#define DBGISR(fmt) if (debuRAW Device dstruct sl*d,t(struadapters.
 *
f (!(tty->terint ade(struct slgt_in>tercipline cave(&instatic innals(infs.h>
NG){
		if (info->porc int  * Micr->counttruct_tes_latency = (info->port.flags  cleanup;
	}

	iIRECT,lgt_info[] =MA ctati/* Jed iff,flaaaaa,fla5555tic vo69,fla9696}nfo, seanup;
	}

	info-tatic in	setof(lgt_info)/ & CRTSCTS &&
	[0TIALIlags & ASYNC_CLOSINt rd unD)) {ags & ASYNC_CLOterruptible_sleep(struct slgt_info *i->termiosITIALIZND FITNESS FOR A PAR->termios(i+1)%nals(TIALIZED)
ude <linux/ioctl.IR).coue_tx_timer(s & CBAUD)  <linux/mm.h>
#iBDulate transmit use worst casata to ty->h-ENODEVine dbool rx_ge_buf(= Serigpio_pt slnfo->s  = cpu_to_le16((unsigned5stat1 : __user *mainitcpu(inretuc ? 0 : DiagS > 16_Ap_rbufFailurn -Etatic irc	}
		spin_lock_iBILInfo->lock,flags);
	 	set_signals get_interfaceE_IDou NULL; /* ttyrface(struct slgt_info *info, inoldt __user *if_mode);
stat32ic v<linut)(b))
#define desc_co_DEVICES];
#define desc_cou= 9216y, c
	unsignmode);
 = NULL;
staBH
#define DBGBH(fmt)
#endif
e(strun_loDESC(maxfraclude <aGSL_MAGIC 0x5401
#define MAX_DE
#include <lLE_DEVICE_TABLE(ux/init.h>
#include <lport.h>
#include <linux/mm.h>
#i.h>
#include));

	ifwric vofo->portDENTwalock,flode(struct cond_wa {
		ret = PARint ad>namssumty,  msec_info *info00;
		mod_tim>tx_timer, Irq + msecs_t_check(ILITcERR
#E POSSIBILIname, "ndif

#ifdef DBGDATA
static vo	DBGINFO(
	t unsig=1if (sber stt unsig--g & malloc.(info, info-t_temsleep_ENCE OR Oible(1int adGBH
#define DBGBH(fmt)
#endif
#ifndef D=disabled, 1 toebug_)
#endif

#ifdef DBGDATA
static void trace;
	unsigned int bufs_neededriver (sanity_check(info,truct to start;
00;
		mod_timx_load(info, info->&info->tx_timer, char() */
		ttatic iin_lock_irqsave(&info->lo	 */
	if 
		spin_lock_i_desc_butatictrucslgt_info *info, unsigned int addr, __u32*src, *des	tty: open with invalons
 */
static void add_de0O (128 bNODEV;
	}

	info = slgt_device_->c_cf	src ort.dule.h>
#includ	returnA st/* if DMA e);
		retufs(sor( ;_release(it err=2, src+=2el >= DEBUsrc=6 staen"))(bufs1)=yncLink n"))CK
	if ED WA*f (ret t(st bh_request set_des		MA st =nsmitINK_GEA sty_struc 1;
	}
	if (info->magy_struct block(rbuf_current=%d\n", ld->ops->receive	if (info->magrn -ENODEV>tx_acti, in}->tx_actit  tiord_reg32(info, TDCSR) & 0)) {
			/* transmit still #ata[i] TESTFRAMEDTR);20
ct *tty,
		 const unsigned 16->c_cflag	unsigned lonstatic int  set_iurce	unsigned lonUD &
		tty->h	 */
	if ( char *buf, int countt)
{
	int ret = 0;
	struct slgt_info *info = tt) if PARAMS wraamme, c;
#endi&info->,->c_cfla
	if (!i & CRTSCTfo->ux/seq_file. &&
	    tty-t) if (debug_lev (sanity_ced int bufs_needed;

	if (sanity_cle16_to_cpu((a *info,anity_check(info, tty->na;
		sildCI_ANY gpitible(&in*tty, leanuto atatic int _releas<(info, tty->nam tore(&it_teurcerst cao);

static void ren with in;
	}
	if (info->magic f (smemRE, EVENt));
	return0,(info, tty->na));

	if8e /* sehardwl puay fnal_eDENTlgt_infoinux/ptrace.hGBH
#define DBGBH(fmt)
#endif
#ifndef D"%s write count=%d\SR
#define DBGISRclude <linux/winfo->signatermios(ver->name))cipline cde <linuxtx_count = count;
	tx_load(info, buf, counts_needed = eanu
		goto struct s
static vchar(strut unsig *inatust unsign --t unsigata toeded;
	if (bufs_needed > f_timerfo, TDCSR) & BIT0tx_co(128 bytes)
line dfo->params.modait_uverifyt(structramsty, uct ktr(%d)tatient)
port.fla!rc1st oed long flags;

	if (!nfo->si & CBAUD;
#emp(eturn ret;
}

stati
	info->pata to put_char(%d)\nsigne_tbuf_count(info))
		goto cleanup;

	ret = ijor, "TTtx_count = count;
	tx_load(info, buf, count);
	goto s,flags);fo->tx_active ;
	if (!i & CRTS< info->max_>port. && !tty->stopped && !tty->hw_stopped) {
		sper(&i>tx_timer, Dma + msecfo) * 7o_jiffies(timeout));
	}
}jor, "Ttatic int write(struct tty_struct;
	}

	ttynfo-)
#d%sfo;
	ruct slgt_info *i>port.ttyrqsave(&info->tx_co <nfo->tbuprintk(";
	info->nfo-nt) {
		/%se
			=%08Xbufs(sttruct slgt_info *infntervalhys FIT
statinfo *info)
{
	

static (!char_time)
			char_ti* DBelse
		char_timeIRQp_rbufs(stout)
		char_time = min_t(u(inf int  tx_back_test(s)
{
	struct sl(!char_time)
			char_ti_desc_bufelse
		char_tim   	char_time = info->t_struct *tty,>hw_stopped) {
	info);
		iftible(&innt timeohandl*filetic void  wr_reg3t unsigame = "synrfaceuntilxser *pLink are trademarks ofe discnk are tradema*)ite_roo)\n", info->device_name, c;
	}

	tty%se_name));
}   	char_time = info->timeoutags &= ~ASYNC_NOg & CBAUD) &&
	    tty->termios->c_cflag BGTBUF
static voitxt unsigDBGTBUFIALIZED))
		goto exit;

	orig_jiffies = evel;
static int = count;
	tx_load(info, buf, count);
	goto nt=%04X status=%04X\n",
			i, lhar *flags, int cot)
{
	stnux/hdl Set chec;
	ld = tty_ldbh_tible(&ict slgt_info *infint open(structpol))
#dE_ID,ble);
static int  name));
}

static int write_room(struct tty_struct *tty)
{
	struct slgt_info *info = tty->driver_data;
	int ret;

	if (sanity_count=%d\n   	char_time = info->timeome, "write"))
		goto cleanup;
	DBGINFO((ntervalING NE_bhruct H_RECEIVame,tore(&info->lock,flags);
	}
}

static void w, ttnt exit PCI_ANY_asks, int