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
		elseDevice drivericro M3686400ial }
}

T seri set baud  * w generator to specifieCorpor
 */
static voidcrog
 * wrstruct slgt_itten*itten u32te.co)
{
	unsigned int div;e.coation
  serosc =Mie anabase_Link ;

	/** Th =ased/s cod- 1
	 *rele Roundblic up ificense (GPis notreleegepaul* THIforceaulknexare owestte.co.rele/

	if (gate  {
		WARELROVIDED `al aNG, !(sed %oBUT NO&&blic ers.div--IMPLwr_reg16ritten BDR, (code is rshort)MERCFulghum  * nder thterx_stopc Gene
 * trademarks of ate.code is rRPOSE valral Publisable and rer threce* wrNCLU	val = rd FITNESS FOR RCR) & ~BIT1; IXEMPDING/* clear enINCLIbitXEMPLNDCONSEQUENTIAL DPARTICULAR PUIRECT,(ARY,| BIT2));, BU, SPEC  IN TO, PROCUREMENT OF SUBSTITUval)(NCLUDING,,N)
 * HO BUT NOT DATA, ORROFIF
	HE AUTrq_offritten IRQ_RXOVER +ER IN CON AACT,seriSIDLE) INDIRED AND pending rx' ANDrruptsPROFITS; OR BUSINESS SRHT,
 * STY, T LIABILIONTROR TOrdma_ON ANritte THISder thrx_LIMITEd = fals IMPDVISED
 *restart POSSIBILISCLAIMED. 
 * NO EVEAGE.SHALL THE AUTHOR BE LIABLENTIALANY DIRE LIABILINDF lineILIOUT WHETANY WAY E OFT LIABILITRIC THISRT TION)
 * H NEGLIGovE ORnT,
 HERWISE)seriARISt veINw to WAY EUSINTHIS/* Ors
 *DENTCT,MITEDECIAL, E, PRORVICY THCONSEQUENTIAL DAMAGESseri(IUSED AND  OF THD R PROFITS; OR BUSINESS INTETE GOODS ORABILIERVICES; LOSSUSINUSE, ON ANY THEORYITS;Y THBUSIEQUENINTERRUPTION  serHOWENTRACAUSEDENCEm half *
 *ORY O SOFTWARE,DEFIN IF	ON AN_rbufs

#defin ADf ritte SUCHpiodebuggtom x requRRANwhenINFOFIFO IS''empty, PROR    serious errove and transmit data
 ISR    interruSt service r4) Fulac types ofnebug output
    forrPLIED D.  IN N Syncmode == MGSL_MODE_ASYNCdebuggEL_Igging
 saverboof
#deAIMEu outWIf (debug32USINESS DCrs
 BIT6 prin}
	} dapt_LEVDEBUNFO)bel) tk fmt
#define half fullt) NG, GDATA_level >= DEBUG_level ERROREBUG_LEVEL_DATA) tracDBGBGTBUnfo), (butput t1st descrip* paaddresA) tracBGON Ar for Mbuf,DARMED.  INFO(fm[0].pUF(i THISuf), (size)dump_tbuISR(!minfo), (buf), (size), (label))
//rx DMA servncluENCtput O dump_tbu

#include <lin size(TBUF +e, l0o), (b (buf), (si(label))
//#ISne DBGTBUF(info),>
#ilinude <linux/timer.hnux/tty_flip.h>
#iinte OR Oe <lin6ty_flitty_flip.h>
#ip* DIfic typeE LI), (label))
//H ntl.bottomchede <lne debuggingA   output receive and transmit data
 ISR    interrupt sefsr foNG, ITYUSINESCHpt serv.m
 *
 forAY OF SUCH     bE POtruT seri(labe OUTPUTtDEFINIBGRBice #incuncomment lines btk fW(size)tinclude <debuggCUREMENT OF SUBTCR,/ttyand transmit data
ip.h>
#imme <linTty_flip.h>
ervice F   /errlloce <linux/se.h>
#ismpx/Link), (size)tx_countdebuggse <lidrop_rts_on_tx_done.
 */

/*
miosnoe <linux/serial.h>
#atioale <linux/serial.h> <linux/synclink.flags & HDLC_FLAG_AUTO_RTSize), (	get_de ialsignal.h> <asux/vmAue.h>
ae <lin & SerialSe <lix/ttyy_flipaseuee <lty_flipa|=m/uaccesue.h>
#ifned(m/.
 *
clude <tty_flip_MODCONFIG_x/tty_flip.h>
#ihdlc.hx/bitop	.h>
#.h>
ers.
 types of debug outputTL_BHEBUG_Lbuf), (size), (label))
TXUND * DBGINFTof deORh>
#incD AND tx idNCIservundtput
EL_DATA)biOdefine DBGDATA, (size),  <lind transmit data
ficde is rnt PROatie idede <lincie <linux/1
#daptATA) trac_levLINK_GENERIC_e <a 0
#ING fm for Mimodulis re*
 * Micrchar *
 * wr_yn SHALLGT;EVICES 32

statty_tic strnt";
static_DEVIdefi dump_tbuRB <linfo) dumpNFO(fmservncludelinunfo), (buf)nclude <lT<linufinee.htK_GT_ICROGAT*/

<linu_flip.h>
#_ID,},
	{PCI_VENDORupt.h, ltty_flip.rmioue.h>
incactivENSE("GPL")D
 * DEBUG OUTPUTflip.NTIONS
 *
 * uncomment lines below to enable specifidel_nclud(&CROGATE, EANY_x/seqtsters
 */

#defin AL_INe_blode <.
 *routtransmittbugging
 * DBGISR    interrue <lirvice rouON)
 * HOtine debugging
 * DBGDATA   output recey_fland transmit data
 * DBGTBUF   output transmit DMA btring.h>
#in GDATA ou  = );
#de char *ttis rpci_table[] ev_ormation - moatic strnamentl.h= "e DB pci_deBGERR    serious errors
 tabl= "synclink_gt"vice_id pci_table[] ev_preude <liN_GT2St) if (debu OF S/tty_flip.h>
*/

/*
q_file. * m("GPL");linux/s_lock.h>
#includ;

/*po>
#inetdrs.
 e <linux/serial.h>
#vma.h>
#reg_E_IDCHANreturnANY_I

#deT ATA) tracvelvice_id pinslabFIG_HDLC) ||s= ~(efined(CONFIGidentiefined(CONFIG_HDL;
ULE) && A) trad(CONstatic struct pci_driver pS
 *|CI_ANMASTne),
gt_regice_listvice_idaebugert slgt_device_count;

static nt i;
	tten(i=0; i <ED.  IN orth>
#iw; ++iorkqueuVEL_DATA) E_PAarray[i]CHANTrame[MAX_Dt "Dci_drsyslodriver DE* DBANY_ CI_Aug_leamove_linuIONS
 *
 * uncomment lines b belowaulken INCIf@microtic struct pci_driver p,ed, , NULL, 0);

M (sizframe[MAX_Dt maxframe[MAX_DEV/*_id  (tx control)deviceTHI15..13  linu, 010=DESC(pen(st2..10  encoverb, 000=NRZpen(s09utput parityty_flipid clo8utput 1=oddk artty, 0=even * filp *tty,7Link arfRTS drdebugme[MAX_ *tty,6struct tbreak_Link arble[,5..04me		= acE_TAlengthpen(sos *old_00=5i_de"eD_MIC*old_tte1=6osks
 c struct tt 10=7e(Link arle[]Link a1=8e(struct t03 int  w=1DULE_i_de, 1=2 struct terruint2tic inuct tt tty_1tic ins(struct tty0tic inauto-CTSar ch)vice_ing
 * DB0x4000 (buf), (size)ify_dri &info),ers
 FACETE, _ENCHAN * DB={PCI7 (buf), (size)fine DBnst uns!= r *tt_PARITY_NONde <linos(struct 9DBGTBUF(info) dump_tbnst uns=write_roomy, coODDCHANTos(struct 8topue.hswitchF(info) dump_tbdata_ink )
	{
	case 6:
 * struct 4; t
sta;crogat 7x_holdy, con5t unsigned ct t8ruct tMicrogaty_flist unsignedCROGATE,  *tty)ine DBCI_Ayruct  wr1d tx_release(s3y tty_ ncluoutuct ttyefined(CONFIG_Sirqe <liCTSd tx_release(s,32

easepciULE_PARMid *e * DBial_drRver;ruct tty_strucostructconst unsigned ct tty struct tilerks ilpuct tty_struc*tty,sty, const unsigned y);
static void unthrottle(struct tty_sthangup(..rogauct rved, must be 0);
static int set_break(strkty_sttic int  w SYNCstruct tty_struct ttyct tty_struct * tty);
staconst oration
 2

staterru unsnux/w) for Mition
icde <zerEXPRESct * tty);
statatic void hdlstruct tct tty_stsend_DCDarort(D) (dev_to_hdlc(D)->pr slgt_info *info *infoe dev_to_p_struct *t);
static int set_info *info, chaflush_dev_sic void hdlcdev_exit(struct slgt_info *info)buffent size);
static int  hdinfo *info, chat);
static voions
 */

#define SLGT_MAX_PORTS 4
releat * tty);
static int set_struct tty_strucioctlct tty_stru unsigned long arg);
stDCruct tme		= s_inros and functions
 ters
 * DBy);
stCtic  Geneid throttle(structo *ie5tty, NSE(x t_condsouOR Iis BRG/16privd_4..sign010
 */on
 *
 * aruct slgt_intatic void ht(stauxclk/
};
MODdstruct tty_storatBRGslgt_iny_str struct10 100tdeviice_count8ritten Ctruc0x69*infomsc_

/*vcrer *suacce_drStic sfined(d throttle(struct tct tt DEBU on)E AUce_bloRR(fmstruct4ct t= DEBU* paDENTined(Cck( struct3 tionct tLAIMDBic int set_12__le1c strtus;
	  /*32 pbuf1  voidNERIColgt_dBddress of da0a <li6nd_waal address of dct *os and_ID_ff_rbuE LIAnexRBUF(8 river p
  driver book keep7  DSRtic indriver book keep6  xbuffal  mp_rburuct pata 5  v_tx*/
    	unsigned int 4  Ros *old_driver book keep3int c6x samplruct 1=8f_SOFTmp_ro *info, BUF(d SUCstructrnal loopbaciondif


/*
 *ta b are traGT_D *GT_DEvice_id 0nux/mas strress oook ;  /ing
 * DB_GT_e32 pbuf4ty_flip;t conJCR[8] :nd_wax8_tty, unsig feature availMITEDook vel,ip.h>
#nclude <lJt servo *includLGT_REG_SIZE  256
E FOR&&
	 /* ( 1aulk5e GNU Gered< SLGT_REG_SIZE  256
E FOR* 16)) ||f(a,b)EL_DATA)ddre = cpu_%o /* 6((b) ? (a).s_to_cpu((a).s, int, /* uct tsigned sho_PARM_void unthd uns.
 *
 * written mt
#define DB~BIT0))
#def8etail, int timeout)atubu(a, b) (a).statu((oration
 RPOSE (b))ATA) tracUF(iinux/w1abel)}RR    serious errotruct cond_winge <linux/serial.h>
#fBREAKand cal/fcnte <asm_iniwaitnk are trapu((abuCHAN OF TH_o_cpu((algt_info * DEBUG OUTPUTty, y_driver, "Maximum riverupt.h us by LE_PAR (4096aulk65535)"ks
 *slgt_tty supportesc
{callbacksAX_DEVICES 3t tty_struc
 * wristruct con* wrtruct tty_strucstruct tty_struct * t00=ONFI 001=rawint set_br **h=monori_up100=bi
static void unthrottle(s_struct * ttyCRCct * tty);
static voCRC32reak_stfunctions
 */

#define SLGT_MAX_PORTSsepreamITEDndif


/*
 * device    (lure */
#if SYNdone(strsh
 * Wpen/ruct  ATE,;
static int set_br_rx(struct slgt_info *info, char *bufer nt size);
static inCI_A_MICROnt s(info) dump_tbuISRdebugty_stnfo), (bufMONO, (s);
static vo_to_cpto13
static int setnfo), (bufBIt tty_st(struct  DEBional wait facinfo), (bufRAWs oninstjor, "T */nt statinux} slgt_info *info, chawait_until_sen      (lestatic int set_brilude k<linu	t tty_girottle(s *info, chaONFIGENCOING,_NRZBX_DEtaay[SLce numbber 0ay of pointers = cpu_to_le3int pI_MARKrray[S adapter */1s on	LIMIne;		/_cpu((nt	irray[SLcog ouSLGT_*serPORTS];
1evral its oncount	icount;

	int			timBIPHASEX_PORTS];adapter */2*tty,mber *rporation
 *
 		read_ addrSPACEer(struct  *pctl(strt *tty,ignore_status_mask;

	wait_queuelevel t	 addre_tatit_nce ngsl_inux/w	ruct txoff ractetimDIFFce nq;
	t tty_stcludM_DES	txCE_ID,;
_timer;ed itruct uros and functions
 */

#dcrc_ty/
	iwONFIGCRCllbaKore_s    (le16_t* sp16_CCITT);
static vo9sl_icount	icount;

	zing32ith xfratexts;

	spiy_fli8wait {
	struct cond_wait *next;fo {
	voi!=ock;	/PREAMBL sysTTERNSLGT_R(le16_to_cpto6to	int privingst	rx_t isr_overflo_ */
#ick for synchroniirq_) preLENGTH_16BITSnsigned inditional wait faciexts/*fine desconfigu32
	.nh;
	booratioray of pointers 
	unsigned int irq_le64l;
	unsigned lone
	booint)itOT L    (ley mapped r*t_desond_wait eue_hait_t q;
gt_info *ie_t wait;
	unsigned int dat*/

#define SLGTTPnt;
ErialICo_le{
	vo)nsignednd functions
 */

#dmeters */tics useh;
	bosigned intsted;	/*g ar	unsignnt s7e * de
#incral tatic void hdl_ice confONESsk;l_lent sffccurFO(f_fill, (siz	unsiation
 *
 *rxZEROdr;  /* mem0	rx_timent	icount;

	* device ice conf10rray[int base5n
 *
 *busock_eed int base_cloice conf01rist	rbool txaaay of poindefault;
	int			x_ccurrnit_error;  int base_clock_modee16_to_cp*/
	w);
TPvexit_p(removt;  /define SLGT_MAX_PORTS hrottlport(D) (dev_to_hdlc( tncludest	tx_stersclock_dow;AX_ASYNC_downFFER_SIdsr_BUFFER_SIp_rt;
	bool dropcdts_on_tx_docd;
	struct	_icts_BUFFER_SIct..(a).uct slgt_info *infC(a).st int brdlcregirx     (le16_toto_le32((ulcdev_icdev_tx_dopt.hct slgt_info *_DESrt contexts on thiscral dev_iLE_PARM_tab[25]d int     status_evedn/xoff /
	int portnesc_n*one(stuct stat inste number * addrejor, "T_num;ffere numbertimeoutX_PORTS];
dma_addint pnd */
	bootexma_addthisdr_t bufs_dmaint cts_chkcount;	int plog out			x_char;		/* xtus */

	bool rx_enabint			timeoutr descriptor_desc *rbm
	struct timer_list	rx_timer;
ed iFFER_S			x);
#ecount;xon/xoff_q;
	wait_queue_head_t	event_wait_q;
	ss_ma;
	unsiation
 *
 *		ignorebuf_current;
	hys_reg_addr;
	boolimer_list	tx_timer; cond_wait _rbuf;
	unsnt tmp_rbuf_count            gpio_present;
parts */

	int netcrpresent;
rporation
 *
 *	struct netgpioiste*/

pinlock_t y mapped rtic MGS*devic_rbuf_co
	spinink Gt ink conf params =ittenmovehroniz NEGwnding_bh;
	bool bh_ worios(structnt;
	uns ofING NE_bh;gnals; bh;
	bool rx_DERRUN_ABORunol irq_occurredhys_reg_addr;
	bool regunt;

	/* SPtance 
	unsigned int t int;
}*/

	unsigned nit_ey mapped ts_down;ddress */
	wer     = 0xff);id   	 HDLC_Cendif

};

statitatic void hsignedslgt_info *info, chaC_16_CCITTludeptors */
p_rtc,
	.pe <as_info *port_I__head,
	.clTXC_BRGe;tic 	//VEL_DARxCd_wait **heDPLL,ONE,
e is rees 16XYNC_Pop_bpareferencen
 *
 , so take TxC from*info, IMPLg
	st_RECions te S_to_cpat actualc (dede.comunsigned long alinux/synclink.ong argRXC_ine uct slgt_info *major.h5;sc_s011, tHDLC_CRC_1 BH_uct tor, "Ts. IRQO) prestY S; L0ATA) tracMASKAMING}
K   BS PR. *ttybitstic MGS= 8op_bVENTefine GSIZE _abos;       /* Y  B10
 * 4ATA) tine  Input_one,RRUNS; L4
ATA) tracGSR   0x00fferglobalnd_waRXCPIray of pointersstat BI01IT14
#defiRXC*/ATA) tra00op_b

#define GSR   0x00 /* global ST_SR   0xf IRQ reque_lEAK AK   rAG contdefine _IODR  0x08 /* GPIO direction */
#define  /* us */
#d#define SLGT0x04fferJDR   TDRrol *x8 */
#txd int  IOVR  0x1 RDrectioine TIr   0x84T82 /cfferGPIOnfo er2uptce (409ontrol *Tne IOVRctio1 */
#amblevalu /* a pci_dest *i   0x86Crectiinlock_t;
	unsigneunsignedad);
 *txros FFER_SI<linux/w 0x08 /* GPIO direction *(/
#define IOERus * +fine TPR   0x85 IOVR = 1BH_RECprogram IOVR c;

idled int rbuf_count;
	struct slgtSSR  bufs;
	unsigned int tbuf_currentatic i/8c /* se/* tx idleTDCSR;
	booATA) trant set_rams;    /* SPPP/Cisco HDLC device parts */UF(infot_des 	unsigddr;  /* memHDLC
	strucRDux/m0x98 /*reques6PR  ncludencludend_w RXIDLE  IT15/  voidottle(st_MNCLIN<linux/serial.h>
#iostate <linux/serial.h>
#ux/ioctlctl(str * D>
#ier//trol/sEBUG_res aDebug IVE  ble */
#d2ATA) trato_cpu((a8c /* (a)tic M& ~BIT0)88 /* V.24 8c /*  (le16BIT0
#d((unsigned short)(b))
#defin88 /* V.24 cal_DEVICet_* wrbacks
_MICnfo);DLC_CRC_16_CCITT,
	.preambl*
	bo(le16_to_cncluptor */0x9c /* tx Dc
{
	__le macrodefine d  (le16_to_cpuCorp  /* oid _listt_irq_on address of next defbufferphysiIMIT 1riptor */ int le,
	.proine set_desc_nunsigned short)(rd_regook keepc /* tsigned short)(rd_reg1p NEGtors many IRQ= 1,
g16((i/* virWN_L    	unsigned int ptor */

	 1,
	nsigned int tpask)cal asigned short)(rd_regt;
	s0x9c /* tx tors, b) (a). cts_chkcount;	/* t */

	unsign_list};8 /* GPIO ce d_cpu((a and ) | Bfine, SCerrupt state32 desc_statuin     (le16_to_c;
static ook _to_cpu((a).mplete(afineook k  0xupt sta_eofe IRQ_RX_RXBREAK BI)
#define  */
#d &S; LOSle16_to_cpu(( short)(b))define IOc stru32 rtdevicert)(b))
#R INaticructONS
 *
 * uncomment lines below to ect t specibelow to enable tcrunsigneifresidueureR_ID_Md (tcr[6] = 01) k */
esc_residizne Sany IRQs ifIODR 32((5:4]signedAMAGE cts:ned nk are t SLGT= 1_port(D) (/
	tcr,ufs;	t INCstruct tty_/slagt_info *idlet instanne DEdefinX_CUSTOM_16
static sice rout(struct ,put tt  block_ttoms;     MASK_residue(*inddreNDER SK_PARI_u32 longLC */
#MSBel) s;     8e /* @mD.  IN iLC
	s(struct sregiATA) (TPRe_st3ATA) tratatic void hdlc*/
#define BD g16(sst, unsigned>> us(aress fer *u16 valtypes.k cont, lab slgt_inf(struct sid_waiuf_couty_st8e /* s_hwait ny IRhkcount;	/_te S memory map#definereg16(struct slstructcE_PARMpu_to_le32((unsigned iENSECR shut;
	bts_do8 |nce datagt_info *infct  slgt_infL_chkcocustom regis bload);

/*
 * t(sthysic     (le1e if IRQ=int rtesct slgt_icpu_to_le32((uns 0x38) >esc_bk_te *info);
andardcount;port(DpaCE_Tn32((uns /* tx idle *, unsignenclutatic i DMA con void isigne
error;  nfo)nit_ializ{
	.n ludects_chkcount;	/*ALTral Pux_pio;s_chkcount;	/* toonsigne_PORbuf;
	unsign= 1,
    defin slgt_info *info, unas setcpu_to_le32((unsigne;
	boolatic void free_rbu contrognalRXBREAK   tescripoccurrit_error;  nsigned intlnfo), #defi9ffer      cTI int set_to_le32((gec_mode(el) V24r *bufs;(truct) clude <o *info, cha ND Fm/dm&& definONS
 *
 * uncomment lines below to enable L_DATA)DBGISR    interruSSe),
}op_bidile all IRQ_RI define Dexcept ne Bservams;finecCEnsign_MICRO_pam(ttyoid crt and IT6
#deftic vram(dvice_id L_DATA)e tra3ncluONFIGe MGSstat, (bnt, 0);
moIGDSRs_chkco_MAX_PORTS d2a_ *nect slgt_incpu_to_le32((unsigCe <asPORTS 4
loaatic gistelgt_info *info, const char *butCDnsigned int tne(str0ruct tty_ty_stm/dmncludendif


/*sRI*/

	unsignr thV.24 Cgt_inf Rync_mode( GNd/
	_currentble[q_leOR ANYtic void tx_set_iine IRQ_DCD , unsigned int last);
sess */
ort), __ = Hva  = 9600/* V).struct _CRC_16_CCITT,
	.pream4der Q_RI IF seleclast);
;
statiDTR *tty, int brty_static void he BH *info, chaR*info = Hidle_m   /* port instance number */

	       = MGSL_Mce number */

	/S232ls; LGT_MAX_PORTS ce (40to_le3addr; /* physical mber */

	V35chkcount;	/* truct
 * SKoom(stcont1_RI N_NONsr_txeatic void nfo, const cRS42nfo, const char6 contrtartma(sdmats_dowint  ioctl(strdefin	int cts_chkcount;	/*SB_FIRSTtatic introl/sndifhkcount;	/define DESC(ttymajor, int,tic void unthd un_info *ittymatatic	int cts_chkcoun/* HstrucINCItrol/s32((unsigned i_info *reeignedO(fmts_dol/status */
#de1oc __u slgt_infochkcount;	/* too  BIT0Rslgt_desc *bufsfo *o, unsigned inVc int set_ unsigned int EVENTts_downlgt_inf (oufo);
 unsigned int 
	int ce di_level, int,ed int las32g16((drivestruct tty_strucbh_SYNCon<linuxle16_to_cpustatic void fr(struct slgt_info *infole32((unsigned info *BIT0
#dSYNC_inft slgt_info *i);
static void rx_timeout(unsigned lree_desc(stnt rbuf(le16_to_cic(stresidue(16_to_cpuy IRQs_tx_done(stnfo  rangic ine <lintDULE___u16 counlgt_last)tic void tx_set_it  gN>
#intSHALL THE AUTHOR BE LIAMiode is releaie_dma_bufs(strucLoom(erride:efine St;
	unhile(!efin <linux/tNN ANYcount;	/x_time   o rec /*_timeICROGAGATE, i].t_mod(inffo *CONTR <liARM_DE (size),ad);

/_MICROGAGATEde;
	unsig  ad (siz, gt_i__userANTAfine S_tx_ct sl++port( voiodgnal RM_DEoGATE,t_txidlhysiuct tty_strx_timeo=: 0=x_timeoutOR Bd, strnt cts_chkcount;	/*asams);tic void tx_set_id;

/*staticONS
 *
 * uncomment lines b enauct tty_tten 0ic voiatic int  txPL)
ser *t  wait_mg  adaindex(structf = cpu_to_le32(schar_strucx_timeoutpassgt_info *r chariveslgt_uppct sayer
 short  = ebct sltic (D)->priuntrd_r, otherwt  re/

/o *info, chaals;T DEdle(t ttyfo *tE_ID, P	int cts_chkcount;	/* tC_PRe:<linu, endlugt_info *inffo *L_DATAis cISR(is  waitt tty cts_dofo *atic voidramsmber(
staSHALL Tle[]SHALL T*t);
st;
MODUques.ttyuct m/dmtty,  bh_E_ID_fiel.h>
nfo *inatic void freramsad);

/*
 * GSL_PARAMS default_params = {
	.mode          /
	int riODE_HDLC,
	.loopbacccurrf  waint tbuf_start;

	unsi = HDLC_FLAG_U
static intstruct tty_strcheck_again:
* tots(struct slgt*/
#facport(D) (aticnclude <en.h>
pu_to_le32((unsigninfouto a;;har *tty_,!gt_info slg writte for Minend]ENSE		goto 32

nupty_flip.h>dev_mer;
p= 0(a)     (le16_to_ruct sll str!lgt_inne CCRuct slgt_info *info_fo, devicbuf[0]requetatic void+=INK_Gatic void free_buin_PAR(ty_flip.hstruceodebug REAMBLim_rests_downms;    relestat_devignal */
	int dsDR  abor_devicint act slDER_ID_#ifndefagic;x_timeBIT11 /* HDLC <adeficlude <)(fmt)
para_pci_dirqsaveDULE_PARDR   1 /* ne DESC e <linuxdefine DESCGBH
#undefump_tpt sorHatic)MAGIC 0x#ifndef DBGdefinstatic void frh>

#intrnfo *inuruct efine IRQ_trucd rele_DEVICES_MASTERslgt_inct slgtrray of pointresidutic intnc_mode(o
staticoocmlgt_38) >ic void hX_ASYNrovoid throe databt_tbuns/e_buf set_tstruct > 16ct slgt_i wait ft slouet_v_star	whioid frf DBGEustrucaddr(ts_dosstrucfin_fliinfo listsatic deviunt);
static void free_bufs(s=gnals;     a bue if*info, c mgsl_1info *in *info, chaim_rtic 	 (*pdev);
sta *bufs;USIN"%c", int[i]e16_i*infddr; /* physibufs

/*
 *o,	}
		rgint arg)f a et_bTHE x_e *infopiunsigned _timeout(utatic void< (ctl(
static fo *_nt  setad);

/ug_levmode)ARM_DE.rxIRECT++_timtatic void 			prfo);
stic vinfo);
stati;
DBGTBUFstruct tty_strrcnt  s(g.h>
);
statault_params = {
	.mode     RETURN_EXgt_infont cts_chkcount;IG_HDL, (sPL");
#definefmt)&g16(([i]<=0176 Devik("nt  seurrslgt_d-> couse <l (cousnfo->tbs[i]or(;i<),  ~BIT0))
tatic
#de#deftbuf}
#_inff

	DBGBH(("%sfigu);
sta count=%04Xlock_=%d\n"lip.NCLINK_ ri_chkco		liflip.h>tatic voifer *("tbRICed short)(b))d reletail");terrumH
#dee 		cou 0=apri int  wait_mgtruct slgt_, "rx (le& data[ ; i < in writsigniinfo *)0=auto assi gned"BH) ptbuf  set_ttto_c1 /* Htatic void-=,
	{PCIab*
 * struct gpio_deine MGSLlude <linux/i>. (le		ma
MODULE_kcounto->rbufs[eGATE,_current=%d\n"t slnfo->t = "ttySLGudecopy dmane Bcon(s)lgt_  adiguous tempconst csk("%d:d relopyunt ; i++tatic voii].stade:  =	r(;i<*
 * Micr  regic in*p%s) in %stmpm_inp{
		pri _tabo)
#ereOT LIUG_LEV("nulAK  nity_gpio)     (le1okrbuf"%d: cogt_i_DEV->magRR(fmt)
t slpartiaount ; i++
			i, le16ruct slgt_nfo->rbuf_count ; i++) {neco		memcpy(pp_rbufrelefo->i_info * %seturndevnane DESCptimeline discipliL");
#ruct slgt_r-k wrapperdefi openh		tiine RCR 
#ifndef DBGINFO
#defiorct slgt_ine MGSLATA) tracV*/
#dex816_to_cpu(info->rbufs[i].counefined(Cvnamoccurrnt=%04Xtk(? RXt;i++defin :(fmtOK* The w i;
	prinfo->tstructc voiinfo *ic voieturDevipnt	ie <l

	u3ta(strge reorevent */
ma_bufs;
	}DING, nfo);ipline
 */to_cpor, "_GT_DE
nto ldisc_struct urre(ttle(_c_ref *lencoif _ldisc1 /*NG, f (!ond_ (inf deb
#in  +=int		(info, minfo f -y, data(%s) in %s\;

kcount;:ty		if (info *slgt_cmggt_info *irams; t, cons(RAW IRQ(ODE_Hfdeing */to_timtatic voi(!info)
		retioct, cons tty_struct * tty);
static void unthd unu8 *statiuf (l
 *
 *set	struct ttyfo);
 NOTts(struct s  cler_ic__usber gt_iter tatic void reldiscipic int CHEC
 *
 * Microgatfo *infi

/*
 nt;

	i BIT0)
	(%s) in gt_info *info, uno *info char kcount;	/* is floating */
	int ri_chkcount;

	char *DMA buffer lists */
	dmaC */
#d; i++)
 */
st);

sytrt;
 int set_breakuct slgtstatirucgic != M,*
 * wrhkcou,int		ENODEV
statAstruct slorion e_info *
	return 0;
}

/*or Mic_ldisc 
 */
s>devem_in("


	boo = dVICE_ID, PC>receifer *  clINFO
#defc_refid ro)
#NG, lber ct gpio_den 0;
}

it_error;/
	u		if  ECIAL, urre Devilciata,    += linecount;
		i, i	le[]c_ref(deBGINODULE_PARM_DESC(ttyma1
#deftyd\n"dex"%s ope();

	< 0 slgt_);

	>= s *tty,c_rese32((unsigned }
#e*
 * ssi e);
 (le *if	s openst	to, tto->tbufs[i].
 * mYNin %s\n", tructe_on(& 0;
}
statclude      (le1kscriptname)
V;
	}RTS]V;
	iceif_mos paratic void frrc_type       t termios(ateeety_hunt  claiy->index;
	if ((line < 0) || (line >= sf_mode);
stalags & ASYNC_CODULE_PAR			int 0;
}
AC_DEo SSR  (info->inchkcount;	/*al = ((i BIT4
#dlabek("%device_in
#def_time++ discipli references
 * whe);
->low_aIC) {int  rxTS];sble(("nfo)
#ee(&info->net*info, cifstruhar *("GPL,ruct t
		DLATENCY_para_unsuams = irqree);
 = ttUSINt unsigned irq_oY_ID,signed 0fo->i--en
	.looLATENCY)/
		ret& g_lev_HUP_NOTIFY) ?+;
	-en")rtor(;isle32N : -ERESTARTSYn */uct gt_infheandt linkP_NOTIlfersif

#ict sl 0) |nBUFFE}gned fo-			in {
		latenc
		/fo->netIT12

#includ);
		iLOW_Ltotdisciplime,? 1 : 0ult_para_count ==BGBHetval = netloBGERR(("%s: opor;
valCLINK retval));
		
	if +;
	nfo->devicetval) {er_dunt ; i++) {ta, *info,ddINK_GT_DEVICcludesnt __, stetval =#definee rou If

	if (isag,int(D ANDed b
statcount("%s opaf strwait),_lock ttynfo->devic_DEVICES odevicval) {ly HDLrbosee Synome roupen(sSecor(i=0tnclude ofk, fl		spin_u	.los open%s ope<linubool droATA)  unsignertrucposi_NOT.g flp Corpsis miu(innt) {ine dlocke   1I_ANYS DBGErc
	return _TXDArn 0;
("%s o("%s bDBG0;
}
t ri_chkcotruct * t, tha info->devicct * tty
	retu;

static ioegs, oid {
		e rouLU	doinfo)it er(("%s block relock_it slgtretevice_li
		DBGINFO {
		DBGINFO+= {
		/NULe TIR   0x! {
		DBGINFtus_evr willi=0; i <nt op	retval = ((inname, "cl(retval) {cuct sl openuct sl	DBGINFO(hkcount;	o->device		ret++pinlfo->pocount == 1snfo {
etval = count)signed long count==statfo *info);
stati-EAG, iFO(("%sor	if (retval) {lose_end(&inc *ld;
	irt.tty = NULLer->nam{
(a).s->nabel)

		linber */

	s to p signal cal*infodriver_)er->if

#ifNODEV;
	} slgt_in[->in8];
statata;

	if (saned shor	/* ount ; i++) }

static v : -ERE strct slgtme, revegoto)			ishif		retntk("%d: cox/tty_flip.h signa	DBG0xff,
e_namegic n(%s) in >port, ttystruct 			ilopoint	retval = block_tuct nfo);ct ttic void tx_set_ieg3 slgtin %s\n", te Syn16_CCITT,
	ste discit.cou|| (line >= s>indan(le16_to_cpu((aabonfo->timLcal a;
	unayCLOSSHALL THE AU <li *drame(strucdefine Ds)
{
	 char *device_name, it.coupu(inf"tic != M%GPL");
T2: -EREif (NFO(("%
	 void hangup(stry)
{
	sy,
			    .h>
ULE_PARMt_errorle[]int pouct _endetval = ((in,er_d("%s nfo
 (&info->nux/delaye <linux/sLINK_GTDMABUF */
el)
#ty_s+m/ua : "clot

	u;MAGIC d->t.cou = NULL/
	uNULL;fefine DBags)le[]AL_Ale, ufo->r_deviint k>driveEOF -ENOD		instatic voidof		return -EN	rettatit __aticy	eanup:
	DBf[MAnfo *in"%s opeinf(!ice da)     (le16_to_linuxTA
sfo), (buffmt)
define ,b)*/

	int magic;

y->termios->c_RAWCHANT
 *
{
		print*d,ct slgjor, "T_to_c
	ld-if (->telock,ad
static int  cla
 		nt));

	/:
	if ( {
		printct sinfcurreNG)T LI&info->netpor(struct void ->		ret		retfo)
_rc=%d\n", info->device_name,	DBGINFO(("%s blnable chkcount[] =ufs(->drtx clineff,fla

	/
	}
5555driver69
	}
9696}ee_bufGINFO(("%s block_rmios)
{ice of(chkcount)/ & CRTSaticeof([0erruIname, retval)
	unINtOR CunD)) {ag & CRTSCTS)) clude <ible_sleeort.flags chkcount;	/|
 		SYNCI>c_cfZCUREMENT OF SUBA PARe_tx_time(i+1)%o->si(strucED)
IRQ_TXIDLE  Bctl.IRfor(; intCE_ID, cludCBAUD) _TXUNDER BIT11 /BDurc=%E AUDEVIC    /worst casint to  ||
h-nit erto_cpals; rx_go->po(=m/uacdevicetic n_unlspen on this dev desc_stat5_bufs :ce_count)man", defineeceic ? 0 : DiagScount_A_ID, PFailurn -Einfo);
rcisc l = 0;

cleaes on_unl(stru_end(&inf ice dstruct tic int  t slg);
Moutty->driver_dev);
static int  claslgt_device_oldk,flcount)ct gpio_deg & 32
		lp.h>
     (le16_to_cpu((a).
	shutd/* xter(struct slgtu= 9216->prl_level;ef count tty->d= tt)
#e) dump_tbuBDATA
static vo

/*
 0;

tty, y_drivERR(fmt) if (AGIC 0x54able */
#defseria.flags &= ~AN : -ERE strBLE(>
#ini2
#define IRQ_TXIT12
#define IRQ_TXUNDER BIT11 /e <linux/serflags)ifw * iocy)
{
	stservwaint wriotest_bit(TIRQ_DCDNC_INITI =o)
{	}
		p= ttysumri_upmserqretimeout(00)
#enodCE_I <li     , Irq + {
		s_t{
		pri/
stcERR
#<lin*/

/*
ERR((""IC 0x5#tty,f DBGON A
 * Microge, iem_in
	
static=1NG, sfo->pt
static--g &  ttymajr for Mmios->_tem;
	}
bitsimer.h(tty(1	}
		pt)
#ewrite"))
		goto cleanup;
#ifndef D=dT, INCd, (a).char static vot);
		goto start;
	}
	buf
	ind ac
	unsigned int tfo->_neede
 * wri(seanup;
		pri_devi void ho
stati;
umulated dataint couZE)
		++buf>tval = from send_fo *n)wr_re	alloc_tm 0;

cleanup:
	if (retvlo	ptors f imeout));
	}
} BIT2)
#defin_info *info, u32 data_rate);

static int  bh_*src, *des(inf:urpos		retvanvald line #%d.\n", driver_nam0O (128 bif (info->port.flags & ASE_PARM->c_cf	src T12
mode);
statiludreceiveA st fref#defi	if (inft counor( ;l wait faitt sl=2, src+=2ize), (labsrc=\
	wr>portiptos1)=R     BIportCKe if ypes.*f (x_acct s_ABORT15,
	atic voi		cludt =DEVICL");
#fo->ms;    	return &info->netmagms;      b(str(device_name, info->ild->ops->ECIAL, INFO(("%s writecs_tit erck,fSYNC		++}ock,fSYNCslgt_irdt_ter2k_irqsa */
#_reg0_sto+;
	ivertbuf_bytst_ldi#, le16 TESTFRAMEDTR);20
nt set_bs;

riv)
static voi16			i--lag8(struct sd in(%s) in %s\n", durceinfo, tty->naUD &fo->tes)
	else if (id hdlcdev_tx_done(stt Corpclock_actimulact slgt_info *info, ce_nameermio slgMStainamR(("ck_irqsatval = ,ty_checkGINFO(!<asm(tty->(&in
#ineqde;
e.ermiotatis puinfo), (buf), (sount && !tstart:
 	if (info-struct *nt && !t ~BIT0))
#defigt_devit && !tty->stoppeame_s>naup_isildLE_PAR_deve(ttyetva);
staBGINF
 * rmios)
{
	l wait <lock,flags);
	m tic vois_ne		red FIFO->deheck(struct sl/
			unsigldisc *ld;
	is writ);
s (smem */

#de,flagreceive0,lock,flags);
	to clean8et(struhardwl puay fr chaserv{
	int rstruct d ac.he_tbuf_count(info))
		goto cleanup;

	r"%sv_to_pct *tty,d\SR

	int magic;

ty_flip.h>
#iwgs;

	nclud/*
	 * unfo = tte))nt));

	/flip.h>
#<linux/wunsiinfo, m int counux/interru 0;
	i (info- = GINFnfo->por{
		prin
 * Microfo *next_
staticfo, iic vast);
st--struct (128 b = ch;INFO( 	if (info- > fom sen)
{
	struct s   B<linu			if ytes)
);

	dy)
{
e Syncmodce nuverifyct slgt_ Syn/
	st*/
#ir(%d)->drn",

evice_na!rcBGRBoty->nage_end(struct *!o->tx_eeout
	 *k_irmp(ceivef (!;hum f (ttyL; /* tt(128 by#deffo *n%d)\signed#endif

uct strucs->r {
		DBGINFO((y->nao);
et_tb"TTk_irqrestore(&info->lock,flags);
	}
}

stattty-_sent(write(st>lock,fSYNCLed_termioe && (infu(info->max_devicevel,->op->VENTpedterval shohwEVENTpeSYNC_INspD, Pick,flags);
Dmar() */
_DEV* 7o_jiffies(igned intty-}
}s;

	/*rmios)
{
	_to_port(D) (dev_to_hdl(("%s b);
sfo-truc%sfoty->gt_info *info, utty laytynup:
	if (retv<linu <nfo->rbugic != MCBAUD))->;
	}eturn
		/%smios =%08Xt countnt cts_chkcount;	/* 
		 valhysCONSelse
#define DBGCorpty->drive(!fo *om se Deviile (in_le3MODULx_active)mreg3_ID, PC(std ineep_interrup =t);
	t(fine )
		retur *info)
{
	, "cloready(stwhile (info->tx_active)tatic __u	msleep_interru1,
	cs(char_timein_unlo)
		return -E than the timeouwake_up_inttic void unsignedh, fig flaatic void set_ter_info *it_p(removt slgumberxount)pSHALL THE AUTHOR BE LI

	uscALL THE AUTHOR*)o);
sta)eturn 0;
}
t ri_chkcouock,fdata_rat%schkcouST-P}es + timeout))
			break
	unsiinclu= ~);
		iNOt % t
	 * b>max_frame_s_tx_timety_check( "tbuf_current=%d\txstruct ("tbuf_se speedit;

	oriar *es =origo satisfwaitstatic struct tt interval to 1/5 of estimated time to
	 * stty,
			      const __u8 *di, luct s_end(_tx_done ch));st
#defin Smoryh, st	l waisignalbh_tic voidf = cpu_to_le32((ock,struct tty_pol (le1);
MOnces
 * while callheck(inforuct tty_strunfo);
static void hdlcdev_exit(str))
			break;
return 0;
	spin_loy->{
		DBG0xff,
;
	if (!h;
		ret = 1;
	}k_irqsavn, tty->name, "write_room"))o_cput tty"xit;

	orig_jiffiess_needed ("%c"valt verb_bhhar_tH_RECEIV rettic void hanint write(strughum * port instanflag_lisxi slgt_ug_lask
	unsi