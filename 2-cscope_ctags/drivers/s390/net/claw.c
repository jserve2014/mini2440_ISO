/*
 *  drivers/s390/net/claw.c
 *    ESCON CLAW network driver
 *
 *  Linux for zSeries version
 *    Copyright IBM Corp. 2002, 2009
 *  Author(s) Original code written by:
 *		Kazuo Iimura <iimura@jp.ibm.com>
 *   	      Rewritten by
 *		Andy Richter <richtera@us.ibm.com>
 *		Marc Price <mwprice@us.ibm.com>
 *
 *    sysfs parms:
 *   group x.x.rrrr,x.x.wwww
 *   read_buffer nnnnnnn
 *   write_buffer nnnnnn
 *   host_name  aaaaaaaa
 *   adapter_name aaaaaaaa
 *   api_type    aaaaaaaa
 *
 *  eg.
 *   group  0.0.0200 0.0.0201
 *   read_buffer 25
 *   write_buffer 20
 *   host_name LINUX390
 *   adapter_name RS6K
 *   api_type     TCPIP
 *
 *  where
 *
 *   The device id is decided by the order entries
 *   are added to the group the first is claw0 the second claw1
 *   up to CLAW_MAX_DEV
 *
 *   rrrr     -	the first of 2 consecutive device addresses used for the
 *		CLAW protocol.
 *		The specified address is always used as the input (Read)
 *		channel and the next address is used as the output channel.
 *
 *   wwww     -	the second of 2 consecutive device addresses used for
 *		the CLAW protocol.
 *              The specified address is always used as the output
 *		channel and the previous address is used as the input channel.
 *
 *   read_buffer	-       specifies number of input buffers to allocate.
 *   write_buffer       -       specifies number of output buffers to allocate.
 *   host_name          -       host name
 *   adaptor_name       -       adaptor name
 *   api_type           -       API type TCPIP or API will be sent and expected
 *				as ws_name
 *
 *   Note the following requirements:
 *   1)  host_name must match the configured adapter_name on the remote side
 *   2)  adaptor_name must match the configured host name on the remote side
 *
 *  Change History
 *    1.00  Initial release shipped
 *    1.10  Changes for Buffer allocation
 *    1.15  Changed for 2.6 Kernel  No longer compiles on 2.4 or lower
 *    1.25  Added Packing support
 *    1.5
 */

#define KMSG_COMPONENT "claw"

#include <asm/ccwdev.h>
#include <asm/ccwgroup.h>
#include <asm/debug.h>
#include <asm/idals.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/if_arp.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ip.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/tcp.h>
#include <linux/timer.h>
#include <linux/types.h>

#include "cu3088.h"
#include "claw.h"

/*
   CLAW uses the s390dbf file system  see claw_trace and claw_setup
*/

static char version[] __initdata = "CLAW driver";
static char debug_buffer[255];
/**
 * Debug Facility Stuff
 */
static debug_info_t *claw_dbf_setup;
static debug_info_t *claw_dbf_trace;

/**
 *  CLAW Debug Facility functions
 */
static void
claw_unregister_debug_facility(void)
{
	if (claw_dbf_setup)
		debug_unregister(claw_dbf_setup);
	if (claw_dbf_trace)
		debug_unregister(claw_dbf_trace);
}

static int
claw_register_debug_facility(void)
{
	claw_dbf_setup = debug_register("claw_setup", 2, 1, 8);
	claw_dbf_trace = debug_register("claw_trace", 2, 2, 8);
	if (claw_dbf_setup == NULL || claw_dbf_trace == NULL) {
		claw_unregister_debug_facility();
		return -ENOMEM;
	}
	debug_register_view(claw_dbf_setup, &debug_hex_ascii_view);
	debug_set_level(claw_dbf_setup, 2);
	debug_register_view(claw_dbf_trace, &debug_hex_ascii_view);
	debug_set_level(claw_dbf_trace, 2);
	return 0;
}

static inline void
claw_set_busy(struct net_device *dev)
{
 ((struct claw_privbk *)dev->ml_priv)->tbusy = 1;
 eieio();
}

static inline void
claw_clear_busy(struct net_device *dev)
{
	clear_bit(0, &(((struct claw_privbk *) dev->ml_priv)->tbusy));
	netif_wake_queue(dev);
	eieio();
}

static inline int
claw_check_busy(struct net_device *dev)
{
	eieio();
	return ((struct claw_privbk *) dev->ml_priv)->tbusy;
}

static inline void
claw_setbit_busy(int nr,struct net_device *dev)
{
	netif_stop_queue(dev);
	set_bit(nr, (void *)&(((struct claw_privbk *)dev->ml_priv)->tbusy));
}

static inline void
claw_clearbit_busy(int nr,struct net_device *dev)
{
	clear_bit(nr, (void *)&(((struct claw_privbk *)dev->ml_priv)->tbusy));
	netif_wake_queue(dev);
}

static inline int
claw_test_and_setbit_busy(int nr,struct net_device *dev)
{
	netif_stop_queue(dev);
	return test_and_set_bit(nr,
		(void *)&(((struct claw_privbk *) dev->ml_priv)->tbusy));
}


/* Functions for the DEV methods */

static int claw_probe(struct ccwgroup_device *cgdev);
static void claw_remove_device(struct ccwgroup_device *cgdev);
static void claw_purge_skb_queue(struct sk_buff_head *q);
static int claw_new_device(struct ccwgroup_device *cgdev);
static int claw_shutdown_device(struct ccwgroup_device *cgdev);
static int claw_tx(struct sk_buff *skb, struct net_device *dev);
static int claw_change_mtu( struct net_device *dev, int new_mtu);
static int claw_open(struct net_device *dev);
static void claw_irq_handler(struct ccw_device *cdev,
	unsigned long intparm, struct irb *irb);
static void claw_irq_tasklet ( unsigned long data );
static int claw_release(struct net_device *dev);
static void claw_write_retry ( struct chbk * p_ch );
static void claw_write_next ( struct chbk * p_ch );
static void claw_timer ( struct chbk * p_ch );

/* Functions */
static int add_claw_reads(struct net_device *dev,
	struct ccwbk* p_first, struct ccwbk* p_last);
static void ccw_check_return_code (struct ccw_device *cdev, int return_code);
static void ccw_check_unit_check (struct chbk * p_ch, unsigned char sense );
static int find_link(struct net_device *dev, char *host_name, char *ws_name );
static int claw_hw_tx(struct sk_buff *skb, struct net_device *dev, long linkid);
static int init_ccw_bk(struct net_device *dev);
static void probe_error( struct ccwgroup_device *cgdev);
static struct net_device_stats *claw_stats(struct net_device *dev);
static int pages_to_order_of_mag(int num_of_pages);
static struct sk_buff *claw_pack_skb(struct claw_privbk *privptr);
/* sysfs Functions */
static ssize_t claw_hname_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t claw_hname_write(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count);
static ssize_t claw_adname_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t claw_adname_write(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count);
static ssize_t claw_apname_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t claw_apname_write(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count);
static ssize_t claw_wbuff_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t claw_wbuff_write(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count);
static ssize_t claw_rbuff_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t claw_rbuff_write(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count);
static int claw_add_files(struct device *dev);
static void claw_remove_files(struct device *dev);

/*   Functions for System Validate  */
static int claw_process_control( struct net_device *dev, struct ccwbk * p_ccw);
static int claw_send_control(struct net_device *dev, __u8 type, __u8 link,
       __u8 correlator, __u8 rc , char *local_name, char *remote_name);
static int claw_snd_conn_req(struct net_device *dev, __u8 link);
static int claw_snd_disc(struct net_device *dev, struct clawctl * p_ctl);
static int claw_snd_sys_validate_rsp(struct net_device *dev,
        struct clawctl * p_ctl, __u32 return_code);
static int claw_strt_conn_req(struct net_device *dev );
static void claw_strt_read(struct net_device *dev, int lock);
static void claw_strt_out_IO(struct net_device *dev);
static void claw_free_wrt_buf(struct net_device *dev);

/* Functions for unpack reads   */
static void unpack_read(struct net_device *dev);

static int claw_pm_prepare(struct ccwgroup_device *gdev)
{
	return -EPERM;
}

/* ccwgroup table  */

static struct ccwgroup_driver claw_group_driver = {
        .owner       = THIS_MODULE,
        .name        = "claw",
        .max_slaves  = 2,
        .driver_id   = 0xC3D3C1E6,
        .probe       = claw_probe,
        .remove      = claw_remove_device,
        .set_online  = claw_new_device,
        .set_offline = claw_shutdown_device,
	.prepare     = claw_pm_prepare,
};

/*
*       Key functions
*/

/*----------------------------------------------------------------*
 *   claw_probe                                                   *
 *      this function is called for each CLAW device.             *
 *----------------------------------------------------------------*/
static int
claw_probe(struct ccwgroup_device *cgdev)
{
	int  		rc;
	struct claw_privbk *privptr=NULL;

	CLAW_DBF_TEXT(2, setup, "probe");
	if (!get_device(&cgdev->dev))
		return -ENODEV;
	privptr = kzalloc(sizeof(struct claw_privbk), GFP_KERNEL);
	dev_set_drvdata(&cgdev->dev, privptr);
	if (privptr == NULL) {
		probe_error(cgdev);
		put_device(&cgdev->dev);
		CLAW_DBF_TEXT_(2, setup, "probex%d", -ENOMEM);
		return -ENOMEM;
	}
	privptr->p_mtc_envelope= kzalloc( MAX_ENVELOPE_SIZE, GFP_KERNEL);
	privptr->p_env = kzalloc(sizeof(struct claw_env), GFP_KERNEL);
        if ((privptr->p_mtc_envelope==NULL) || (privptr->p_env==NULL)) {
                probe_error(cgdev);
		put_device(&cgdev->dev);
		CLAW_DBF_TEXT_(2, setup, "probex%d", -ENOMEM);
                return -ENOMEM;
        }
	memcpy(privptr->p_env->adapter_name,WS_NAME_NOT_DEF,8);
	memcpy(privptr->p_env->host_name,WS_NAME_NOT_DEF,8);
	memcpy(privptr->p_env->api_type,WS_NAME_NOT_DEF,8);
	privptr->p_env->packing = 0;
	privptr->p_env->write_buffers = 5;
	privptr->p_env->read_buffers = 5;
	privptr->p_env->read_size = CLAW_FRAME_SIZE;
	privptr->p_env->write_size = CLAW_FRAME_SIZE;
	rc = claw_add_files(&cgdev->dev);
	if (rc) {
		probe_error(cgdev);
		put_device(&cgdev->dev);
		dev_err(&cgdev->dev, "Creating the /proc files for a new"
		" CLAW device failed\n");
		CLAW_DBF_TEXT_(2, setup, "probex%d", rc);
		return rc;
	}
	privptr->p_env->p_priv = privptr;
        cgdev->cdev[0]->handler = claw_irq_handler;
	cgdev->cdev[1]->handler = claw_irq_handler;
	CLAW_DBF_TEXT(2, setup, "prbext 0");

        return 0;
}  /*  end of claw_probe       */

/*-------------------------------------------------------------------*
 *   claw_tx                                                         *
 *-------------------------------------------------------------------*/

static int
claw_tx(struct sk_buff *skb, struct net_device *dev)
{
        int             rc;
	struct claw_privbk *privptr = dev->ml_priv;
	unsigned long saveflags;
        struct chbk *p_ch;

	CLAW_DBF_TEXT(4, trace, "claw_tx");
        p_ch=&privptr->channel[WRITE];
        spin_lock_irqsave(get_ccwdev_lock(p_ch->cdev), saveflags);
        rc=claw_hw_tx( skb, dev, 1 );
        spin_unlock_irqrestore(get_ccwdev_lock(p_ch->cdev), saveflags);
	CLAW_DBF_TEXT_(4, trace, "clawtx%d", rc);
	if (rc)
		rc = NETDEV_TX_BUSY;
	else
		rc = NETDEV_TX_OK;
        return rc;
}   /*  end of claw_tx */

/*------------------------------------------------------------------*
 *  pack the collect queue into an skb and return it                *
 *   If not packing just return the top skb from the queue          *
 *------------------------------------------------------------------*/

static struct sk_buff *
claw_pack_skb(struct claw_privbk *privptr)
{
	struct sk_buff *new_skb,*held_skb;
	struct chbk *p_ch = &privptr->channel[WRITE];
	struct claw_env  *p_env = privptr->p_env;
	int	pkt_cnt,pk_ind,so_far;

	new_skb = NULL;		/* assume no dice */
	pkt_cnt = 0;
	CLAW_DBF_TEXT(4, trace, "PackSKBe");
	if (!skb_queue_empty(&p_ch->collect_queue)) {
	/* some data */
		held_skb = skb_dequeue(&p_ch->collect_queue);
		if (held_skb)
			dev_kfree_skb_any(held_skb);
		else
			return NULL;
		if (p_env->packing != DO_PACKED)
			return held_skb;
		/* get a new SKB we will pack at least one */
		new_skb = dev_alloc_skb(p_env->write_size);
		if (new_skb == NULL) {
			atomic_inc(&held_skb->users);
			skb_queue_head(&p_ch->collect_queue,held_skb);
			return NULL;
		}
		/* we have packed packet and a place to put it  */
		pk_ind = 1;
		so_far = 0;
		new_skb->cb[1] = 'P'; /* every skb on queue has pack header */
		while ((pk_ind) && (held_skb != NULL)) {
			if (held_skb->len+so_far <= p_env->write_size-8) {
				memcpy(skb_put(new_skb,held_skb->len),
					held_skb->data,held_skb->len);
				privptr->stats.tx_packets++;
				so_far += held_skb->len;
				pkt_cnt++;
				dev_kfree_skb_any(held_skb);
				held_skb = skb_dequeue(&p_ch->collect_queue);
				if (held_skb)
					atomic_dec(&held_skb->users);
			} else {
				pk_ind = 0;
				atomic_inc(&held_skb->users);
				skb_queue_head(&p_ch->collect_queue,held_skb);
			}
		}
	}
	CLAW_DBF_TEXT(4, trace, "PackSKBx");
	return new_skb;
}

/*-------------------------------------------------------------------*
 *   claw_change_mtu                                                 *
 *                                                                   *
 *-------------------------------------------------------------------*/

static int
claw_change_mtu(struct net_device *dev, int new_mtu)
{
	struct claw_privbk *privptr = dev->ml_priv;
	int buff_size;
	CLAW_DBF_TEXT(4, trace, "setmtu");
	buff_size = privptr->p_env->write_size;
        if ((new_mtu < 60) || (new_mtu > buff_size)) {
                return -EINVAL;
        }
        dev->mtu = new_mtu;
        return 0;
}  /*   end of claw_change_mtu */


/*-------------------------------------------------------------------*
 *   claw_open                                                       *
 *                                                                   *
 *-------------------------------------------------------------------*/
static int
claw_open(struct net_device *dev)
{

        int     rc;
        int     i;
        unsigned long       saveflags=0;
        unsigned long       parm;
        struct claw_privbk  *privptr;
	DECLARE_WAITQUEUE(wait, current);
        struct timer_list  timer;
        struct ccwbk *p_buf;

	CLAW_DBF_TEXT(4, trace, "open");
	privptr = (struct claw_privbk *)dev->ml_priv;
        /*   allocate and initialize CCW blocks */
	if (privptr->buffs_alloc == 0) {
	        rc=init_ccw_bk(dev);
        	if (rc) {
			CLAW_DBF_TEXT(2, trace, "openmem");
                	return -ENOMEM;
        	}
	}
        privptr->system_validate_comp=0;
        privptr->release_pend=0;
	if(strncmp(privptr->p_env->api_type,WS_APPL_NAME_PACKED,6) == 0) {
		privptr->p_env->read_size=DEF_PACK_BUFSIZE;
		privptr->p_env->write_size=DEF_PACK_BUFSIZE;
		privptr->p_env->packing=PACKING_ASK;
	} else {
		privptr->p_env->packing=0;
		privptr->p_env->read_size=CLAW_FRAME_SIZE;
		privptr->p_env->write_size=CLAW_FRAME_SIZE;
	}
        claw_set_busy(dev);
	tasklet_init(&privptr->channel[READ].tasklet, claw_irq_tasklet,
        	(unsigned long) &privptr->channel[READ]);
        for ( i = 0; i < 2;  i++) {
		CLAW_DBF_TEXT_(2, trace, "opn_ch%d", i);
                init_waitqueue_head(&privptr->channel[i].wait);
		/* skb_queue_head_init(&p_ch->io_queue); */
		if (i == WRITE)
			skb_queue_head_init(
				&privptr->channel[WRITE].collect_queue);
                privptr->channel[i].flag_a = 0;
                privptr->channel[i].IO_active = 0;
                privptr->channel[i].flag  &= ~CLAW_TIMER;
                init_timer(&timer);
                timer.function = (void *)claw_timer;
                timer.data = (unsigned long)(&privptr->channel[i]);
                timer.expires = jiffies + 15*HZ;
                add_timer(&timer);
                spin_lock_irqsave(get_ccwdev_lock(
			privptr->channel[i].cdev), saveflags);
                parm = (unsigned long) &privptr->channel[i];
                privptr->channel[i].claw_state = CLAW_START_HALT_IO;
		rc = 0;
		add_wait_queue(&privptr->channel[i].wait, &wait);
                rc = ccw_device_halt(
			(struct ccw_device *)privptr->channel[i].cdev,parm);
                set_current_state(TASK_INTERRUPTIBLE);
                spin_unlock_irqrestore(
			get_ccwdev_lock(privptr->channel[i].cdev), saveflags);
                schedule();
		set_current_state(TASK_RUNNING);
                remove_wait_queue(&privptr->channel[i].wait, &wait);
                if(rc != 0)
                        ccw_check_return_code(privptr->channel[i].cdev, rc);
                if((privptr->channel[i].flag & CLAW_TIMER) == 0x00)
                        del_timer(&timer);
        }
        if ((((privptr->channel[READ].last_dstat |
		privptr->channel[WRITE].last_dstat) &
           ~(DEV_STAT_CHN_END | DEV_STAT_DEV_END)) != 0x00) ||
           (((privptr->channel[READ].flag |
	   	privptr->channel[WRITE].flag) & CLAW_TIMER) != 0x00)) {
		dev_info(&privptr->channel[READ].cdev->dev,
			"%s: remote side is not ready\n", dev->name);
		CLAW_DBF_TEXT(2, trace, "notrdy");

                for ( i = 0; i < 2;  i++) {
                        spin_lock_irqsave(
				get_ccwdev_lock(privptr->channel[i].cdev),
				saveflags);
                        parm = (unsigned long) &privptr->channel[i];
                        privptr->channel[i].claw_state = CLAW_STOP;
                        rc = ccw_device_halt(
				(struct ccw_device *)&privptr->channel[i].cdev,
				parm);
                        spin_unlock_irqrestore(
				get_ccwdev_lock(privptr->channel[i].cdev),
				saveflags);
                        if (rc != 0) {
                                ccw_check_return_code(
					privptr->channel[i].cdev, rc);
                        }
                }
                free_pages((unsigned long)privptr->p_buff_ccw,
			(int)pages_to_order_of_mag(privptr->p_buff_ccw_num));
                if (privptr->p_env->read_size < PAGE_SIZE) {
                        free_pages((unsigned long)privptr->p_buff_read,
			       (int)pages_to_order_of_mag(
			       		privptr->p_buff_read_num));
                }
                else {
                        p_buf=privptr->p_read_active_first;
                        while (p_buf!=NULL) {
                                free_pages((unsigned long)p_buf->p_buffer,
				      (int)pages_to_order_of_mag(
				      	privptr->p_buff_pages_perread ));
                                p_buf=p_buf->next;
                        }
                }
                if (privptr->p_env->write_size < PAGE_SIZE ) {
                        free_pages((unsigned long)privptr->p_buff_write,
			     (int)pages_to_order_of_mag(
			     	privptr->p_buff_write_num));
                }
                else {
                        p_buf=privptr->p_write_active_first;
                        while (p_buf!=NULL) {
                                free_pages((unsigned long)p_buf->p_buffer,
				     (int)pages_to_order_of_mag(
				     	privptr->p_buff_pages_perwrite ));
                                p_buf=p_buf->next;
                        }
                }
		privptr->buffs_alloc = 0;
		privptr->channel[READ].flag= 0x00;
		privptr->channel[WRITE].flag = 0x00;
                privptr->p_buff_ccw=NULL;
                privptr->p_buff_read=NULL;
                privptr->p_buff_write=NULL;
                claw_clear_busy(dev);
		CLAW_DBF_TEXT(2, trace, "open EIO");
                return -EIO;
        }

        /*   Send SystemValidate command */

        claw_clear_busy(dev);
	CLAW_DBF_TEXT(4, trace, "openok");
        return 0;
}    /*     end of claw_open    */

/*-------------------------------------------------------------------*
*                                                                    *
*       claw_irq_handler                                             *
*                                                                    *
*--------------------------------------------------------------------*/
static void
claw_irq_handler(struct ccw_device *cdev,
	unsigned long intparm, struct irb *irb)
{
        struct chbk *p_ch = NULL;
        struct claw_privbk *privptr = NULL;
        struct net_device *dev = NULL;
        struct claw_env  *p_env;
        struct chbk *p_ch_r=NULL;

	CLAW_DBF_TEXT(4, trace, "clawirq");
        /* Bypass all 'unsolicited interrupts' */
	privptr = dev_get_drvdata(&cdev->dev);
	if (!privptr) {
		dev_warn(&cdev->dev, "An uninitialized CLAW device received an"
			" IRQ, c-%02x d-%02x\n",
			irb->scsw.cmd.cstat, irb->scsw.cmd.dstat);
		CLAW_DBF_TEXT(2, trace, "badirq");
                return;
        }

	/* Try to extract channel from driver data. */
	if (privptr->channel[READ].cdev == cdev)
		p_ch = &privptr->channel[READ];
	else if (privptr->channel[WRITE].cdev == cdev)
		p_ch = &privptr->channel[WRITE];
	else {
		dev_warn(&cdev->dev, "The device is not a CLAW device\n");
		CLAW_DBF_TEXT(2, trace, "badchan");
		return;
	}
	CLAW_DBF_TEXT_(4, trace, "IRQCH=%d", p_ch->flag);

	dev = (struct net_device *) (p_ch->ndev);
        p_env=privptr->p_env;

	/* Copy interruption response block. */
	memcpy(p_ch->irb, irb, sizeof(struct irb));

	/* Check for good subchannel return code, otherwise info message */
	if (irb->scsw.cmd.cstat && !(irb->scsw.cmd.cstat & SCHN_STAT_PCI)) {
		dev_info(&cdev->dev,
			"%s: subchannel check for device: %04x -"
			" Sch Stat %02x  Dev Stat %02x CPA - %04x\n",
                        dev->name, p_ch->devno,
			irb->scsw.cmd.cstat, irb->scsw.cmd.dstat,
			irb->scsw.cmd.cpa);
		CLAW_DBF_TEXT(2, trace, "chanchk");
                /* return; */
        }

        /* Check the reason-code of a unit check */
	if (irb->scsw.cmd.dstat & DEV_STAT_UNIT_CHECK)
                ccw_check_unit_check(p_ch, irb->ecw[0]);

        /* State machine to bring the connection up, down and to restart */
	p_ch->last_dstat = irb->scsw.cmd.dstat;

        switch (p_ch->claw_state) {
	case CLAW_STOP:/* HALT_IO by claw_release (halt sequence) */
		if (!((p_ch->irb->scsw.cmd.stctl & SCSW_STCTL_SEC_STATUS) ||
		(p_ch->irb->scsw.cmd.stctl == SCSW_STCTL_STATUS_PEND) ||
		(p_ch->irb->scsw.cmd.stctl ==
		(SCSW_STCTL_ALERT_STATUS | SCSW_STCTL_STATUS_PEND))))
			return;
		wake_up(&p_ch->wait);   /* wake up claw_release */
		CLAW_DBF_TEXT(4, trace, "stop");
		return;
	case CLAW_START_HALT_IO: /* HALT_IO issued by claw_open  */
		if (!((p_ch->irb->scsw.cmd.stctl & SCSW_STCTL_SEC_STATUS) ||
		(p_ch->irb->scsw.cmd.stctl == SCSW_STCTL_STATUS_PEND) ||
		(p_ch->irb->scsw.cmd.stctl ==
		(SCSW_STCTL_ALERT_STATUS | SCSW_STCTL_STATUS_PEND)))) {
			CLAW_DBF_TEXT(4, trace, "haltio");
			return;
		}
		if (p_ch->flag == CLAW_READ) {
			p_ch->claw_state = CLAW_START_READ;
			wake_up(&p_ch->wait); /* wake claw_open (READ)*/
		} else if (p_ch->flag == CLAW_WRITE) {
			p_ch->claw_state = CLAW_START_WRITE;
			/*      send SYSTEM_VALIDATE                    */
			claw_strt_read(dev, LOCK_NO);
			claw_send_control(dev,
				SYSTEM_VALIDATE_REQUEST,
				0, 0, 0,
				p_env->host_name,
				p_env->adapter_name);
		} else {
			dev_warn(&cdev->dev, "The CLAW device received"
				" an unexpected IRQ, "
				"c-%02x d-%02x\n",
				irb->scsw.cmd.cstat,
				irb->scsw.cmd.dstat);
			return;
			}
		CLAW_DBF_TEXT(4, trace, "haltio");
		return;
	case CLAW_START_READ:
		CLAW_DBF_TEXT(4, trace, "ReadIRQ");
		if (p_ch->irb->scsw.cmd.dstat & DEV_STAT_UNIT_CHECK) {
			clear_bit(0, (void *)&p_ch->IO_active);
			if ((p_ch->irb->ecw[0] & 0x41) == 0x41 ||
			    (p_ch->irb->ecw[0] & 0x40) == 0x40 ||
			    (p_ch->irb->ecw[0])        == 0) {
				privptr->stats.rx_errors++;
				dev_info(&cdev->dev,
					"%s: Restart is required after remote "
					"side recovers \n",
					dev->name);
			}
			CLAW_DBF_TEXT(4, trace, "notrdy");
			return;
		}
		if ((p_ch->irb->scsw.cmd.cstat & SCHN_STAT_PCI) &&
			(p_ch->irb->scsw.cmd.dstat == 0)) {
			if (test_and_set_bit(CLAW_BH_ACTIVE,
				(void *)&p_ch->flag_a) == 0)
				tasklet_schedule(&p_ch->tasklet);
			else
				CLAW_DBF_TEXT(4, trace, "PCINoBH");
			CLAW_DBF_TEXT(4, trace, "PCI_read");
			return;
		}
		if (!((p_ch->irb->scsw.cmd.stctl & SCSW_STCTL_SEC_STATUS) ||
		 (p_ch->irb->scsw.cmd.stctl == SCSW_STCTL_STATUS_PEND) ||
		 (p_ch->irb->scsw.cmd.stctl ==
		 (SCSW_STCTL_ALERT_STATUS | SCSW_STCTL_STATUS_PEND)))) {
			CLAW_DBF_TEXT(4, trace, "SPend_rd");
			return;
		}
		clear_bit(0, (void *)&p_ch->IO_active);
		claw_clearbit_busy(TB_RETRY, dev);
		if (test_and_set_bit(CLAW_BH_ACTIVE,
			(void *)&p_ch->flag_a) == 0)
			tasklet_schedule(&p_ch->tasklet);
		else
			CLAW_DBF_TEXT(4, trace, "RdBHAct");
		CLAW_DBF_TEXT(4, trace, "RdIRQXit");
		return;
	case CLAW_START_WRITE:
		if (p_ch->irb->scsw.cmd.dstat & DEV_STAT_UNIT_CHECK) {
			dev_info(&cdev->dev,
				"%s: Unit Check Occured in "
				"write channel\n", dev->name);
			clear_bit(0, (void *)&p_ch->IO_active);
			if (p_ch->irb->ecw[0] & 0x80) {
				dev_info(&cdev->dev,
					"%s: Resetting Event "
					"occurred:\n", dev->name);
				init_timer(&p_ch->timer);
				p_ch->timer.function =
					(void *)claw_write_retry;
				p_ch->timer.data = (unsigned long)p_ch;
				p_ch->timer.expires = jiffies + 10*HZ;
				add_timer(&p_ch->timer);
				dev_info(&cdev->dev,
					"%s: write connection "
					"restarting\n", dev->name);
			}
			CLAW_DBF_TEXT(4, trace, "rstrtwrt");
			return;
		}
		if (p_ch->irb->scsw.cmd.dstat & DEV_STAT_UNIT_EXCEP) {
			clear_bit(0, (void *)&p_ch->IO_active);
			dev_info(&cdev->dev,
				"%s: Unit Exception "
				"occurred in write channel\n",
				dev->name);
		}
		if (!((p_ch->irb->scsw.cmd.stctl & SCSW_STCTL_SEC_STATUS) ||
		(p_ch->irb->scsw.cmd.stctl == SCSW_STCTL_STATUS_PEND) ||
		(p_ch->irb->scsw.cmd.stctl ==
		(SCSW_STCTL_ALERT_STATUS | SCSW_STCTL_STATUS_PEND)))) {
			CLAW_DBF_TEXT(4, trace, "writeUE");
			return;
		}
		clear_bit(0, (void *)&p_ch->IO_active);
		if (claw_test_and_setbit_busy(TB_TX, dev) == 0) {
			claw_write_next(p_ch);
			claw_clearbit_busy(TB_TX, dev);
			claw_clear_busy(dev);
		}
		p_ch_r = (struct chbk *)&privptr->channel[READ];
		if (test_and_set_bit(CLAW_BH_ACTIVE,
			(void *)&p_ch_r->flag_a) == 0)
			tasklet_schedule(&p_ch_r->tasklet);
		CLAW_DBF_TEXT(4, trace, "StWtExit");
		return;
	default:
		dev_warn(&cdev->dev,
			"The CLAW device for %s received an unexpected IRQ\n",
			 dev->name);
		CLAW_DBF_TEXT(2, trace, "badIRQ");
		return;
        }

}       /*   end of claw_irq_handler    */


/*-------------------------------------------------------------------*
*       claw_irq_tasklet                                             *
*                                                                    *
*--------------------------------------------------------------------*/
static void
claw_irq_tasklet ( unsigned long data )
{
	struct chbk * p_ch;
        struct net_device  *dev;
        struct claw_privbk *       privptr;

	p_ch = (struct chbk *) data;
        dev = (struct net_device *)p_ch->ndev;
	CLAW_DBF_TEXT(4, trace, "IRQtask");
	privptr = (struct claw_privbk *)dev->ml_priv;
        unpack_read(dev);
        clear_bit(CLAW_BH_ACTIVE, (void *)&p_ch->flag_a);
	CLAW_DBF_TEXT(4, trace, "TskletXt");
        return;
}       /*    end of claw_irq_bh    */

/*-------------------------------------------------------------------*
*       claw_release                                                 *
*                                                                    *
*--------------------------------------------------------------------*/
static int
claw_release(struct net_device *dev)
{
        int                rc;
        int                i;
        unsigned long      saveflags;
        unsigned long      parm;
        struct claw_privbk *privptr;
        DECLARE_WAITQUEUE(wait, current);
        struct ccwbk*             p_this_ccw;
        struct ccwbk*             p_buf;

	if (!dev)
                return 0;
	privptr = (struct claw_privbk *)dev->ml_priv;
        if (!privptr)
                return 0;
	CLAW_DBF_TEXT(4, trace, "release");
        privptr->release_pend=1;
        claw_setbit_busy(TB_STOP,dev);
        for ( i = 1; i >=0 ;  i--) {
                spin_lock_irqsave(
			get_ccwdev_lock(privptr->channel[i].cdev), saveflags);
             /*   del_timer(&privptr->channel[READ].timer);  */
 		privptr->channel[i].claw_state = CLAW_STOP;
                privptr->channel[i].IO_active = 0;
                parm = (unsigned long) &privptr->channel[i];
		if (i == WRITE)
			claw_purge_skb_queue(
				&privptr->channel[WRITE].collect_queue);
                rc = ccw_device_halt (privptr->channel[i].cdev, parm);
	        if (privptr->system_validate_comp==0x00)  /* never opened? */
                   init_waitqueue_head(&privptr->channel[i].wait);
                add_wait_queue(&privptr->channel[i].wait, &wait);
                set_current_state(TASK_INTERRUPTIBLE);
	        spin_unlock_irqrestore(
			get_ccwdev_lock(privptr->channel[i].cdev), saveflags);
	        schedule();
		set_current_state(TASK_RUNNING);
	        remove_wait_queue(&privptr->channel[i].wait, &wait);
	        if (rc != 0) {
                        ccw_check_return_code(privptr->channel[i].cdev, rc);
                }
        }
	if (privptr->pk_skb != NULL) {
		dev_kfree_skb_any(privptr->pk_skb);
		privptr->pk_skb = NULL;
	}
	if(privptr->buffs_alloc != 1) {
		CLAW_DBF_TEXT(4, trace, "none2fre");
		return 0;
	}
	CLAW_DBF_TEXT(4, trace, "freebufs");
	if (privptr->p_buff_ccw != NULL) {
        	free_pages((unsigned long)privptr->p_buff_ccw,
	        	(int)pages_to_order_of_mag(privptr->p_buff_ccw_num));
	}
	CLAW_DBF_TEXT(4, trace, "freeread");
        if (privptr->p_env->read_size < PAGE_SIZE) {
	    if (privptr->p_buff_read != NULL) {
                free_pages((unsigned long)privptr->p_buff_read,
		      (int)pages_to_order_of_mag(privptr->p_buff_read_num));
		}
        }
        else {
                p_buf=privptr->p_read_active_first;
                while (p_buf!=NULL) {
                        free_pages((unsigned long)p_buf->p_buffer,
			     (int)pages_to_order_of_mag(
			     	privptr->p_buff_pages_perread ));
                        p_buf=p_buf->next;
                }
        }
	 CLAW_DBF_TEXT(4, trace, "freewrit");
        if (privptr->p_env->write_size < PAGE_SIZE ) {
                free_pages((unsigned long)privptr->p_buff_write,
		      (int)pages_to_order_of_mag(privptr->p_buff_write_num));
        }
        else {
                p_buf=privptr->p_write_active_first;
                while (p_buf!=NULL) {
                        free_pages((unsigned long)p_buf->p_buffer,
			      (int)pages_to_order_of_mag(
			      privptr->p_buff_pages_perwrite ));
                        p_buf=p_buf->next;
                }
        }
	 CLAW_DBF_TEXT(4, trace, "clearptr");
	privptr->buffs_alloc = 0;
        privptr->p_buff_ccw=NULL;
        privptr->p_buff_read=NULL;
        privptr->p_buff_write=NULL;
        privptr->system_validate_comp=0;
        privptr->release_pend=0;
        /*      Remove any writes that were pending and reset all reads   */
        p_this_ccw=privptr->p_read_active_first;
        while (p_this_ccw!=NULL) {
                p_this_ccw->header.length=0xffff;
                p_this_ccw->header.opcode=0xff;
                p_this_ccw->header.flag=0x00;
                p_this_ccw=p_this_ccw->next;
        }

        while (privptr->p_write_active_first!=NULL) {
                p_this_ccw=privptr->p_write_active_first;
                p_this_ccw->header.flag=CLAW_PENDING;
                privptr->p_write_active_first=p_this_ccw->next;
                p_this_ccw->next=privptr->p_write_free_chain;
                privptr->p_write_free_chain=p_this_ccw;
                ++privptr->write_free_count;
        }
        privptr->p_write_active_last=NULL;
        privptr->mtc_logical_link = -1;
        privptr->mtc_skipping = 1;
        privptr->mtc_offset=0;

        if (((privptr->channel[READ].last_dstat |
		privptr->channel[WRITE].last_dstat) &
		~(DEV_STAT_CHN_END | DEV_STAT_DEV_END)) != 0x00) {
		dev_warn(&privptr->channel[READ].cdev->dev,
			"Deactivating %s completed with incorrect"
			" subchannel status "
			"(read %02x, write %02x)\n",
                dev->name,
		privptr->channel[READ].last_dstat,
		privptr->channel[WRITE].last_dstat);
		 CLAW_DBF_TEXT(2, trace, "badclose");
        }
	CLAW_DBF_TEXT(4, trace, "rlsexit");
        return 0;
}      /* end of claw_release     */

/*-------------------------------------------------------------------*
*       claw_write_retry                                             *
*                                                                    *
*--------------------------------------------------------------------*/

static void
claw_write_retry ( struct chbk *p_ch )
{

        struct net_device  *dev=p_ch->ndev;

	CLAW_DBF_TEXT(4, trace, "w_retry");
        if (p_ch->claw_state == CLAW_STOP) {
        	return;
        }
	claw_strt_out_IO( dev );
	CLAW_DBF_TEXT(4, trace, "rtry_xit");
        return;
}      /* end of claw_write_retry      */


/*-------------------------------------------------------------------*
*       claw_write_next                                              *
*                                                                    *
*--------------------------------------------------------------------*/

static void
claw_write_next ( struct chbk * p_ch )
{

        struct net_device  *dev;
        struct claw_privbk *privptr=NULL;
	struct sk_buff *pk_skb;
	int	rc;

	CLAW_DBF_TEXT(4, trace, "claw_wrt");
        if (p_ch->claw_state == CLAW_STOP)
                return;
        dev = (struct net_device *) p_ch->ndev;
	privptr = (struct claw_privbk *) dev->ml_priv;
        claw_free_wrt_buf( dev );
	if ((privptr->write_free_count > 0) &&
	    !skb_queue_empty(&p_ch->collect_queue)) {
	  	pk_skb = claw_pack_skb(privptr);
		while (pk_skb != NULL) {
			rc = claw_hw_tx( pk_skb, dev,1);
			if (privptr->write_free_count > 0) {
	   			pk_skb = claw_pack_skb(privptr);
			} else
				pk_skb = NULL;
		}
	}
        if (privptr->p_write_active_first!=NULL) {
                claw_strt_out_IO(dev);
        }
        return;
}      /* end of claw_write_next      */

/*-------------------------------------------------------------------*
*                                                                    *
*       claw_timer                                                   *
*--------------------------------------------------------------------*/

static void
claw_timer ( struct chbk * p_ch )
{
	CLAW_DBF_TEXT(4, trace, "timer");
        p_ch->flag |= CLAW_TIMER;
        wake_up(&p_ch->wait);
        return;
}      /* end of claw_timer  */

/*
*
*       functions
*/


/*-------------------------------------------------------------------*
*                                                                    *
*     pages_to_order_of_mag                                          *
*                                                                    *
*    takes a number of pages from 1 to 512 and returns the           *
*    log(num_pages)/log(2) get_free_pages() needs a base 2 order     *
*    of magnitude get_free_pages() has an upper order of 9           *
*--------------------------------------------------------------------*/

static int
pages_to_order_of_mag(int num_of_pages)
{
	int	order_of_mag=1;		/* assume 2 pages */
	int	nump;

	CLAW_DBF_TEXT_(5, trace, "pages%d", num_of_pages);
	if (num_of_pages == 1)   {return 0; }  /* magnitude of 0 = 1 page */
	/* 512 pages = 2Meg on 4k page systems */
	if (num_of_pages >= 512) {return 9; }
	/* we have two or more pages order is at least 1 */
	for (nump=2 ;nump <= 512;nump*=2) {
	  if (num_of_pages <= nump)
		  break;
	  order_of_mag +=1;
	}
	if (order_of_mag > 9) { order_of_mag = 9; }  /* I know it's paranoid */
	CLAW_DBF_TEXT_(5, trace, "mag%d", order_of_mag);
	return order_of_mag;
}

/*-------------------------------------------------------------------*
*                                                                    *
*     add_claw_reads                                                 *
*                                                                    *
*--------------------------------------------------------------------*/
static int
add_claw_reads(struct net_device *dev, struct ccwbk* p_first,
	struct ccwbk* p_last)
{
        struct claw_privbk *privptr;
        struct ccw1  temp_ccw;
        struct endccw * p_end;
	CLAW_DBF_TEXT(4, trace, "addreads");
	privptr = dev->ml_priv;
        p_end = privptr->p_end_ccw;

        /* first CCW and last CCW contains a new set of read channel programs
        *       to apend the running channel programs
        */
        if ( p_first==NULL) {
		CLAW_DBF_TEXT(4, trace, "addexit");
                return 0;
        }

        /* set up ending CCW sequence for this segment */
        if (p_end->read1) {
                p_end->read1=0x00;    /*  second ending CCW is now active */
                /*      reset ending CCWs and setup TIC CCWs              */
                p_end->read2_nop2.cmd_code = CCW_CLAW_CMD_READFF;
                p_end->read2_nop2.flags  = CCW_FLAG_SLI | CCW_FLAG_SKIP;
                p_last->r_TIC_1.cda =(__u32)__pa(&p_end->read2_nop1);
                p_last->r_TIC_2.cda =(__u32)__pa(&p_end->read2_nop1);
                p_end->read2_nop2.cda=0;
                p_end->read2_nop2.count=1;
        }
        else {
                p_end->read1=0x01;  /* first ending CCW is now active */
                /*      reset ending CCWs and setup TIC CCWs          */
                p_end->read1_nop2.cmd_code = CCW_CLAW_CMD_READFF;
                p_end->read1_nop2.flags  = CCW_FLAG_SLI | CCW_FLAG_SKIP;
                p_last->r_TIC_1.cda = (__u32)__pa(&p_end->read1_nop1);
                p_last->r_TIC_2.cda = (__u32)__pa(&p_end->read1_nop1);
                p_end->read1_nop2.cda=0;
                p_end->read1_nop2.count=1;
        }

        if ( privptr-> p_read_active_first ==NULL ) {
		privptr->p_read_active_first = p_first;  /*  set new first */
		privptr->p_read_active_last  = p_last;   /*  set new last  */
        }
        else {

                /* set up TIC ccw  */
                temp_ccw.cda= (__u32)__pa(&p_first->read);
                temp_ccw.count=0;
                temp_ccw.flags=0;
                temp_ccw.cmd_code = CCW_CLAW_CMD_TIC;


                if (p_end->read1) {

               /* first set of CCW's is chained to the new read              */
               /* chain, so the second set is chained to the active chain.   */
               /* Therefore modify the second set to point to the new        */
               /* read chain set up TIC CCWs                                 */
               /* make sure we update the CCW so channel doesn't fetch it    */
               /* when it's only half done                                   */
                        memcpy( &p_end->read2_nop2, &temp_ccw ,
				sizeof(struct ccw1));
                        privptr->p_read_active_last->r_TIC_1.cda=
				(__u32)__pa(&p_first->read);
                        privptr->p_read_active_last->r_TIC_2.cda=
				(__u32)__pa(&p_first->read);
                }
                else {
                        /* make sure we update the CCW so channel doesn't   */
			/* fetch it when it is only half done               */
                        memcpy( &p_end->read1_nop2, &temp_ccw ,
				sizeof(struct ccw1));
                        privptr->p_read_active_last->r_TIC_1.cda=
				(__u32)__pa(&p_first->read);
                        privptr->p_read_active_last->r_TIC_2.cda=
				(__u32)__pa(&p_first->read);
                }
		/*      chain in new set of blocks                         */
                privptr->p_read_active_last->next = p_first;
                privptr->p_read_active_last=p_last;
        } /* end of if ( privptr-> p_read_active_first ==NULL)  */
	CLAW_DBF_TEXT(4, trace, "addexit");
        return 0;
}    /*     end of add_claw_reads   */

/*-------------------------------------------------------------------*
 *   ccw_check_return_code                                           *
 *                                                                   *
 *-------------------------------------------------------------------*/

static void
ccw_check_return_code(struct ccw_device *cdev, int return_code)
{
	CLAW_DBF_TEXT(4, trace, "ccwret");
        if (return_code != 0) {
                switch (return_code) {
		case -EBUSY: /* BUSY is a transient state no action needed */
			break;
		case -ENODEV:
			dev_err(&cdev->dev, "The remote channel adapter is not"
				" available\n");
			break;
		case -EINVAL:
			dev_err(&cdev->dev,
				"The status of the remote channel adapter"
				" is not valid\n");
			break;
		default:
			dev_err(&cdev->dev, "The common device layer"
				" returned error code %d\n",
				  return_code);
		}
	}
	CLAW_DBF_TEXT(4, trace, "ccwret");
}    /*    end of ccw_check_return_code   */

/*-------------------------------------------------------------------*
*       ccw_check_unit_check                                         *
*--------------------------------------------------------------------*/

static void
ccw_check_unit_check(struct chbk * p_ch, unsigned char sense )
{
	struct net_device *ndev = p_ch->ndev;
	struct device *dev = &p_ch->cdev->dev;

	CLAW_DBF_TEXT(4, trace, "unitchek");
	dev_warn(dev, "The communication peer of %s disconnected\n",
		ndev->name);

	if (sense & 0x40) {
		if (sense & 0x01) {
			dev_warn(dev, "The remote channel adapter for"
				" %s has been reset\n",
				ndev->name);
		}
	} else if (sense & 0x20) {
		if (sense & 0x04) {
			dev_warn(dev, "A data streaming timeout occurred"
				" for %s\n",
				ndev->name);
		} else if (sense & 0x10) {
			dev_warn(dev, "The remote channel adapter for %s"
				" is faulty\n",
				ndev->name);
		} else {
			dev_warn(dev, "A data transfer parity error occurred"
				" for %s\n",
				ndev->name);
		}
	} else if (sense & 0x10) {
		dev_warn(dev, "A read data parity error occurred"
			" for %s\n",
			ndev->name);
	}

}   /*    end of ccw_check_unit_check    */

/*-------------------------------------------------------------------*
*               find_link                                            *
*--------------------------------------------------------------------*/
static int
find_link(struct net_device *dev, char *host_name, char *ws_name )
{
	struct claw_privbk *privptr;
	struct claw_env *p_env;
	int    rc=0;

	CLAW_DBF_TEXT(2, setup, "findlink");
	privptr = dev->ml_priv;
        p_env=privptr->p_env;
	switch (p_env->packing)
	{
		case  PACKING_ASK:
			if ((memcmp(WS_APPL_NAME_PACKED, host_name, 8)!=0) ||
			    (memcmp(WS_APPL_NAME_PACKED, ws_name, 8)!=0 ))
        	             rc = EINVAL;
			break;
		case  DO_PACKED:
		case  PACK_SEND:
			if ((memcmp(WS_APPL_NAME_IP_NAME, host_name, 8)!=0) ||
			    (memcmp(WS_APPL_NAME_IP_NAME, ws_name, 8)!=0 ))
        	        	rc = EINVAL;
			break;
		default:
	       		if ((memcmp(HOST_APPL_NAME, host_name, 8)!=0) ||
		    	    (memcmp(p_env->api_type , ws_name, 8)!=0))
        	        	rc = EINVAL;
			break;
	}

	return rc;
}    /*    end of find_link    */

/*-------------------------------------------------------------------*
 *   claw_hw_tx                                                      *
 *                                                                   *
 *                                                                   *
 *-------------------------------------------------------------------*/

static int
claw_hw_tx(struct sk_buff *skb, struct net_device *dev, long linkid)
{
        int                             rc=0;
        struct claw_privbk 		*privptr;
        struct ccwbk           *p_this_ccw;
        struct ccwbk           *p_first_ccw;
        struct ccwbk           *p_last_ccw;
        __u32                           numBuffers;
        signed long                     len_of_data;
        unsigned long                   bytesInThisBuffer;
        unsigned char                   *pDataAddress;
        struct endccw                   *pEnd;
        struct ccw1                     tempCCW;
        struct chbk                     *p_ch;
	struct claw_env			*p_env;
        int                             lock;
	struct clawph			*pk_head;
	struct chbk			*ch;

	CLAW_DBF_TEXT(4, trace, "hw_tx");
	privptr = (struct claw_privbk *)(dev->ml_priv);
        p_ch=(struct chbk *)&privptr->channel[WRITE];
	p_env =privptr->p_env;
	claw_free_wrt_buf(dev);	/* Clean up free chain if posible */
        /*  scan the write queue to free any completed write packets   */
        p_first_ccw=NULL;
        p_last_ccw=NULL;
	if ((p_env->packing >= PACK_SEND) &&
       	    (skb->cb[1] != 'P')) {
		skb_push(skb,sizeof(struct clawph));
		pk_head=(struct clawph *)skb->data;
		pk_head->len=skb->len-sizeof(struct clawph);
		if (pk_head->len%4)  {
			pk_head->len+= 4-(pk_head->len%4);
			skb_pad(skb,4-(pk_head->len%4));
			skb_put(skb,4-(pk_head->len%4));
		}
		if (p_env->packing == DO_PACKED)
			pk_head->link_num = linkid;
		else
			pk_head->link_num = 0;
		pk_head->flag = 0x00;
		skb_pad(skb,4);
		skb->cb[1] = 'P';
	}
        if (linkid == 0) {
        	if (claw_check_busy(dev)) {
                	if (privptr->write_free_count!=0) {
                                claw_clear_busy(dev);
                        }
                        else {
                                claw_strt_out_IO(dev );
                                claw_free_wrt_buf( dev );
                                if (privptr->write_free_count==0) {
					ch = &privptr->channel[WRITE];
					atomic_inc(&skb->users);
					skb_queue_tail(&ch->collect_queue, skb);
                                	goto Done;
                                }
                                else {
                                	claw_clear_busy(dev);
                                }
                        }
                }
                /*  tx lock  */
                if (claw_test_and_setbit_busy(TB_TX,dev)) { /* set to busy */
			ch = &privptr->channel[WRITE];
			atomic_inc(&skb->users);
			skb_queue_tail(&ch->collect_queue, skb);
                        claw_strt_out_IO(dev );
                        rc=-EBUSY;
                        goto Done2;
                }
        }
        /*      See how many write buffers are required to hold this data */
	numBuffers = DIV_ROUND_UP(skb->len, privptr->p_env->write_size);

        /*      If that number of buffers isn't available, give up for now */
        if (privptr->write_free_count < numBuffers ||
            privptr->p_write_free_chain == NULL ) {

                claw_setbit_busy(TB_NOBUFFER,dev);
		ch = &privptr->channel[WRITE];
		atomic_inc(&skb->users);
		skb_queue_tail(&ch->collect_queue, skb);
		CLAW_DBF_TEXT(2, trace, "clawbusy");
                goto Done2;
        }
        pDataAddress=skb->data;
        len_of_data=skb->len;

        while (len_of_data > 0) {
                p_this_ccw=privptr->p_write_free_chain;  /* get a block */
		if (p_this_ccw == NULL) { /* lost the race */
			ch = &privptr->channel[WRITE];
			atomic_inc(&skb->users);
			skb_queue_tail(&ch->collect_queue, skb);
			goto Done2;
		}
                privptr->p_write_free_chain=p_this_ccw->next;
                p_this_ccw->next=NULL;
                --privptr->write_free_count; /* -1 */
		if (len_of_data >= privptr->p_env->write_size)
			bytesInThisBuffer = privptr->p_env->write_size;
		else
			bytesInThisBuffer = len_of_data;
                memcpy( p_this_ccw->p_buffer,pDataAddress, bytesInThisBuffer);
                len_of_data-=bytesInThisBuffer;
                pDataAddress+=(unsigned long)bytesInThisBuffer;
                /*      setup write CCW         */
                p_this_ccw->write.cmd_code = (linkid * 8) +1;
                if (len_of_data>0) {
                        p_this_ccw->write.cmd_code+=MORE_to_COME_FLAG;
                }
                p_this_ccw->write.count=bytesInThisBuffer;
                /*      now add to end of this chain    */
                if (p_first_ccw==NULL)    {
                        p_first_ccw=p_this_ccw;
                }
                if (p_last_ccw!=NULL) {
                        p_last_ccw->next=p_this_ccw;
                        /*      set up TIC ccws         */
                        p_last_ccw->w_TIC_1.cda=
				(__u32)__pa(&p_this_ccw->write);
                }
                p_last_ccw=p_this_ccw;      /* save new last block */
        }

        /*      FirstCCW and LastCCW now contain a new set of write channel
        *       programs to append to the running channel program
        */

        if (p_first_ccw!=NULL) {
		/*      setup ending ccw sequence for this segment           */
                pEnd=privptr->p_end_ccw;
                if (pEnd->write1) {
                        pEnd->write1=0x00;   /* second end ccw is now active */
                        /*      set up Tic CCWs         */
                        p_last_ccw->w_TIC_1.cda=
				(__u32)__pa(&pEnd->write2_nop1);
                        pEnd->write2_nop2.cmd_code = CCW_CLAW_CMD_READFF;
                        pEnd->write2_nop2.flags    =
				CCW_FLAG_SLI | CCW_FLAG_SKIP;
                        pEnd->write2_nop2.cda=0;
                        pEnd->write2_nop2.count=1;
                }
                else {  /*  end of if (pEnd->write1)*/
                        pEnd->write1=0x01;   /* first end ccw is now active */
                        /*      set up Tic CCWs         */
                        p_last_ccw->w_TIC_1.cda=
				(__u32)__pa(&pEnd->write1_nop1);
                        pEnd->write1_nop2.cmd_code = CCW_CLAW_CMD_READFF;
                        pEnd->write1_nop2.flags    =
				CCW_FLAG_SLI | CCW_FLAG_SKIP;
                        pEnd->write1_nop2.cda=0;
                        pEnd->write1_nop2.count=1;
                }  /* end if if (pEnd->write1) */

                if (privptr->p_write_active_first==NULL ) {
                        privptr->p_write_active_first=p_first_ccw;
                        privptr->p_write_active_last=p_last_ccw;
                }
                else {
                        /*      set up Tic CCWs         */

                        tempCCW.cda=(__u32)__pa(&p_first_ccw->write);
                        tempCCW.count=0;
                        tempCCW.flags=0;
                        tempCCW.cmd_code=CCW_CLAW_CMD_TIC;

                        if (pEnd->write1) {

                 /*
                 * first set of ending CCW's is chained to the new write
                 * chain, so the second set is chained to the active chain
                 * Therefore modify the second set to point the new write chain.
                 * make sure we update the CCW atomically
                 * so channel does not fetch it when it's only half done
                 */
                                memcpy( &pEnd->write2_nop2, &tempCCW ,
					sizeof(struct ccw1));
                                privptr->p_write_active_last->w_TIC_1.cda=
					(__u32)__pa(&p_first_ccw->write);
                        }
                        else {

                        /*make sure we update the CCW atomically
                         *so channel does not fetch it when it's only half done
                         */
                                memcpy(&pEnd->write1_nop2, &tempCCW ,
					sizeof(struct ccw1));
                                privptr->p_write_active_last->w_TIC_1.cda=
				        (__u32)__pa(&p_first_ccw->write);

                        } /* end if if (pEnd->write1) */

                        privptr->p_write_active_last->next=p_first_ccw;
                        privptr->p_write_active_last=p_last_ccw;
                }

        } /* endif (p_first_ccw!=NULL)  */
        dev_kfree_skb_any(skb);
	if (linkid==0) {
        	lock=LOCK_NO;
        }
        else  {
                lock=LOCK_YES;
        }
        claw_strt_out_IO(dev );
        /*      if write free count is zero , set NOBUFFER       */
	if (privptr->write_free_count==0) {
		claw_setbit_busy(TB_NOBUFFER,dev);
        }
Done2:
	claw_clearbit_busy(TB_TX,dev);
Done:
	return(rc);
}    /*    end of claw_hw_tx    */

/*-------------------------------------------------------------------*
*                                                                    *
*     init_ccw_bk                                                    *
*                                                                    *
*--------------------------------------------------------------------*/

static int
init_ccw_bk(struct net_device *dev)
{

        __u32   ccw_blocks_required;
        __u32   ccw_blocks_perpage;
        __u32   ccw_pages_required;
        __u32   claw_reads_perpage=1;
        __u32   claw_read_pages;
        __u32   claw_writes_perpage=1;
        __u32   claw_write_pages;
        void    *p_buff=NULL;
        struct ccwbk*p_free_chain;
	struct ccwbk*p_buf;
	struct ccwbk*p_last_CCWB;
	struct ccwbk*p_first_CCWB;
        struct endccw *p_endccw=NULL;
	addr_t  real_address;
	struct claw_privbk *privptr = dev->ml_priv;
        struct clawh *pClawH=NULL;
        addr_t   real_TIC_address;
        int i,j;
	CLAW_DBF_TEXT(4, trace, "init_ccw");

        /*  initialize  statistics field */
        privptr->active_link_ID=0;
        /*  initialize  ccwbk pointers  */
        privptr->p_write_free_chain=NULL;   /* pointer to free ccw chain*/
        privptr->p_write_active_first=NULL; /* pointer to the first write ccw*/
        privptr->p_write_active_last=NULL;  /* pointer to the last write ccw*/
        privptr->p_read_active_first=NULL;  /* pointer to the first read ccw*/
        privptr->p_read_active_last=NULL;   /* pointer to the last read ccw */
        privptr->p_end_ccw=NULL;            /* pointer to ending ccw        */
        privptr->p_claw_signal_blk=NULL;    /* pointer to signal block      */
	privptr->buffs_alloc = 0;
        memset(&privptr->end_ccw, 0x00, sizeof(struct endccw));
        memset(&privptr->ctl_bk, 0x00, sizeof(struct clawctl));
        /*  initialize  free write ccwbk counter  */
        privptr->write_free_count=0;  /* number of free bufs on write chain */
        p_last_CCWB = NULL;
        p_first_CCWB= NULL;
        /*
        *  We need 1 CCW block for each read buffer, 1 for each
        *  write buffer, plus 1 for ClawSignalBlock
        */
        ccw_blocks_required =
		privptr->p_env->read_buffers+privptr->p_env->write_buffers+1;
        /*
        * compute number of CCW blocks that will fit in a page
        */
        ccw_blocks_perpage= PAGE_SIZE /  CCWBK_SIZE;
        ccw_pages_required=
		DIV_ROUND_UP(ccw_blocks_required, ccw_blocks_perpage);

        /*
         *  read and write sizes are set by 2 constants in claw.h
	 *  4k and 32k.  Unpacked values other than 4k are not going to
	 * provide good performance. With packing buffers support 32k
	 * buffers are used.
         */
	if (privptr->p_env->read_size < PAGE_SIZE) {
		claw_reads_perpage = PAGE_SIZE / privptr->p_env->read_size;
		claw_read_pages = DIV_ROUND_UP(privptr->p_env->read_buffers,
						claw_reads_perpage);
         }
         else {       /* > or equal  */
		privptr->p_buff_pages_perread =
			DIV_ROUND_UP(privptr->p_env->read_size, PAGE_SIZE);
		claw_read_pages = privptr->p_env->read_buffers *
					privptr->p_buff_pages_perread;
         }
        if (privptr->p_env->write_size < PAGE_SIZE) {
		claw_writes_perpage =
			PAGE_SIZE / privptr->p_env->write_size;
		claw_write_pages = DIV_ROUND_UP(privptr->p_env->write_buffers,
						claw_writes_perpage);

        }
        else {      /* >  or equal  */
		privptr->p_buff_pages_perwrite =
			DIV_ROUND_UP(privptr->p_env->read_size, PAGE_SIZE);
		claw_write_pages = privptr->p_env->write_buffers *
					privptr->p_buff_pages_perwrite;
        }
        /*
        *               allocate ccw_pages_required
        */
        if (privptr->p_buff_ccw==NULL) {
                privptr->p_buff_ccw=
			(void *)__get_free_pages(__GFP_DMA,
		        (int)pages_to_order_of_mag(ccw_pages_required ));
                if (privptr->p_buff_ccw==NULL) {
                        return -ENOMEM;
                }
                privptr->p_buff_ccw_num=ccw_pages_required;
        }
        memset(privptr->p_buff_ccw, 0x00,
		privptr->p_buff_ccw_num * PAGE_SIZE);

        /*
        *               obtain ending ccw block address
        *
        */
        privptr->p_end_ccw = (struct endccw *)&privptr->end_ccw;
        real_address  = (__u32)__pa(privptr->p_end_ccw);
        /*                              Initialize ending CCW block       */
        p_endccw=privptr->p_end_ccw;
        p_endccw->real=real_address;
        p_endccw->write1=0x00;
        p_endccw->read1=0x00;

        /*      write1_nop1                                     */
        p_endccw->write1_nop1.cmd_code = CCW_CLAW_CMD_NOP;
        p_endccw->write1_nop1.flags       = CCW_FLAG_SLI | CCW_FLAG_CC;
        p_endccw->write1_nop1.count       = 1;
        p_endccw->write1_nop1.cda         = 0;

        /*      write1_nop2                                     */
        p_endccw->write1_nop2.cmd_code = CCW_CLAW_CMD_READFF;
        p_endccw->write1_nop2.flags        = CCW_FLAG_SLI | CCW_FLAG_SKIP;
        p_endccw->write1_nop2.count      = 1;
        p_endccw->write1_nop2.cda        = 0;

        /*      write2_nop1                                     */
        p_endccw->write2_nop1.cmd_code = CCW_CLAW_CMD_NOP;
        p_endccw->write2_nop1.flags        = CCW_FLAG_SLI | CCW_FLAG_CC;
        p_endccw->write2_nop1.count        = 1;
        p_endccw->write2_nop1.cda          = 0;

        /*      write2_nop2                                     */
        p_endccw->write2_nop2.cmd_code = CCW_CLAW_CMD_READFF;
        p_endccw->write2_nop2.flags        = CCW_FLAG_SLI | CCW_FLAG_SKIP;
        p_endccw->write2_nop2.count        = 1;
        p_endccw->write2_nop2.cda          = 0;

        /*      read1_nop1                                      */
        p_endccw->read1_nop1.cmd_code = CCW_CLAW_CMD_NOP;
        p_endccw->read1_nop1.flags        = CCW_FLAG_SLI | CCW_FLAG_CC;
        p_endccw->read1_nop1.count        = 1;
        p_endccw->read1_nop1.cda          = 0;

        /*      read1_nop2                                      */
        p_endccw->read1_nop2.cmd_code = CCW_CLAW_CMD_READFF;
        p_endccw->read1_nop2.flags        = CCW_FLAG_SLI | CCW_FLAG_SKIP;
        p_endccw->read1_nop2.count        = 1;
        p_endccw->read1_nop2.cda          = 0;

        /*      read2_nop1                                      */
        p_endccw->read2_nop1.cmd_code = CCW_CLAW_CMD_NOP;
        p_endccw->read2_nop1.flags        = CCW_FLAG_SLI | CCW_FLAG_CC;
        p_endccw->read2_nop1.count        = 1;
        p_endccw->read2_nop1.cda          = 0;

        /*      read2_nop2                                      */
        p_endccw->read2_nop2.cmd_code = CCW_CLAW_CMD_READFF;
        p_endccw->read2_nop2.flags        = CCW_FLAG_SLI | CCW_FLAG_SKIP;
        p_endccw->read2_nop2.count        = 1;
        p_endccw->read2_nop2.cda          = 0;

        /*
        *                               Build a chain of CCWs
        *
        */
        p_buff=privptr->p_buff_ccw;

        p_free_chain=NULL;
        for (i=0 ; i < ccw_pages_required; i++ ) {
                real_address  = (__u32)__pa(p_buff);
                p_buf=p_buff;
                for (j=0 ; j < ccw_blocks_perpage ; j++) {
                        p_buf->next  = p_free_chain;
                        p_free_chain = p_buf;
                        p_buf->real=(__u32)__pa(p_buf);
                        ++p_buf;
                }
                p_buff+=PAGE_SIZE;
        }
        /*
        *                               Initialize ClawSignalBlock
        *
        */
        if (privptr->p_claw_signal_blk==NULL) {
                privptr->p_claw_signal_blk=p_free_chain;
                p_free_chain=p_free_chain->next;
                pClawH=(struct clawh *)privptr->p_claw_signal_blk;
                pClawH->length=0xffff;
                pClawH->opcode=0xff;
                pClawH->flag=CLAW_BUSY;
        }

        /*
        *               allocate write_pages_required and add to free chain
        */
        if (privptr->p_buff_write==NULL) {
            if (privptr->p_env->write_size < PAGE_SIZE) {
                privptr->p_buff_write=
			(void *)__get_free_pages(__GFP_DMA,
			(int)pages_to_order_of_mag(claw_write_pages ));
                if (privptr->p_buff_write==NULL) {
                        privptr->p_buff_ccw=NULL;
                        return -ENOMEM;
                }
                /*
                *                               Build CLAW write free chain
                *
                */

                memset(privptr->p_buff_write, 0x00,
			ccw_pages_required * PAGE_SIZE);
                privptr->p_write_free_chain=NULL;

                p_buff=privptr->p_buff_write;

                for (i=0 ; i< privptr->p_env->write_buffers ; i++) {
                        p_buf        = p_free_chain;      /*  get a CCW */
                        p_free_chain = p_buf->next;
                        p_buf->next  =privptr->p_write_free_chain;
                        privptr->p_write_free_chain = p_buf;
                        p_buf-> p_buffer	= (struct clawbuf *)p_buff;
                        p_buf-> write.cda       = (__u32)__pa(p_buff);
                        p_buf-> write.flags     = CCW_FLAG_SLI | CCW_FLAG_CC;
                        p_buf-> w_read_FF.cmd_code = CCW_CLAW_CMD_READFF;
                        p_buf-> w_read_FF.flags   = CCW_FLAG_SLI | CCW_FLAG_CC;
                        p_buf-> w_read_FF.count   = 1;
                        p_buf-> w_read_FF.cda     =
				(__u32)__pa(&p_buf-> header.flag);
                        p_buf-> w_TIC_1.cmd_code = CCW_CLAW_CMD_TIC;
                        p_buf-> w_TIC_1.flags      = 0;
                        p_buf-> w_TIC_1.count      = 0;

			if (((unsigned long)p_buff +
					    privptr->p_env->write_size) >=
			   ((unsigned long)(p_buff+2*
			    (privptr->p_env->write_size) - 1) & PAGE_MASK)) {
				p_buff = p_buff+privptr->p_env->write_size;
                        }
                }
           }
           else      /*  Buffers are => PAGE_SIZE. 1 buff per get_free_pages */
           {
               privptr->p_write_free_chain=NULL;
               for (i = 0; i< privptr->p_env->write_buffers ; i++) {
                   p_buff=(void *)__get_free_pages(__GFP_DMA,
		        (int)pages_to_order_of_mag(
			privptr->p_buff_pages_perwrite) );
                   if (p_buff==NULL) {
                        free_pages((unsigned long)privptr->p_buff_ccw,
			      (int)pages_to_order_of_mag(
			      		privptr->p_buff_ccw_num));
                        privptr->p_buff_ccw=NULL;
			p_buf=privptr->p_buff_write;
                        while (p_buf!=NULL) {
                                free_pages((unsigned long)
					p_buf->p_buffer,
					(int)pages_to_order_of_mag(
					privptr->p_buff_pages_perwrite));
                                p_buf=p_buf->next;
                        }
                        return -ENOMEM;
                   }  /* Error on get_pages   */
                   memset(p_buff, 0x00, privptr->p_env->write_size );
                   p_buf         = p_free_chain;
                   p_free_chain  = p_buf->next;
                   p_buf->next   = privptr->p_write_free_chain;
                   privptr->p_write_free_chain = p_buf;
                   privptr->p_buff_write = p_buf;
                   p_buf->p_buffer=(struct clawbuf *)p_buff;
                   p_buf-> write.cda     = (__u32)__pa(p_buff);
                   p_buf-> write.flags   = CCW_FLAG_SLI | CCW_FLAG_CC;
                   p_buf-> w_read_FF.cmd_code = CCW_CLAW_CMD_READFF;
                   p_buf-> w_read_FF.flags    = CCW_FLAG_SLI | CCW_FLAG_CC;
                   p_buf-> w_read_FF.count    = 1;
                   p_buf-> w_read_FF.cda      =
			(__u32)__pa(&p_buf-> header.flag);
                   p_buf-> w_TIC_1.cmd_code = CCW_CLAW_CMD_TIC;
                   p_buf-> w_TIC_1.flags   = 0;
                   p_buf-> w_TIC_1.count   = 0;
               }  /* for all write_buffers   */

           }    /* else buffers are PAGE_SIZE or bigger */

        }
        privptr->p_buff_write_num=claw_write_pages;
        privptr->write_free_count=privptr->p_env->write_buffers;


        /*
        *               allocate read_pages_required and chain to free chain
        */
        if (privptr->p_buff_read==NULL) {
            if (privptr->p_env->read_size < PAGE_SIZE)  {
                privptr->p_buff_read=
			(void *)__get_free_pages(__GFP_DMA,
			(int)pages_to_order_of_mag(claw_read_pages) );
                if (privptr->p_buff_read==NULL) {
                        free_pages((unsigned long)privptr->p_buff_ccw,
				(int)pages_to_order_of_mag(
					privptr->p_buff_ccw_num));
			/* free the write pages size is < page size  */
                        free_pages((unsigned long)privptr->p_buff_write,
				(int)pages_to_order_of_mag(
				privptr->p_buff_write_num));
                        privptr->p_buff_ccw=NULL;
                        privptr->p_buff_write=NULL;
                        return -ENOMEM;
                }
                memset(privptr->p_buff_read, 0x00, claw_read_pages * PAGE_SIZE);
                privptr->p_buff_read_num=claw_read_pages;
                /*
                *                               Build CLAW read free chain
                *
                */
                p_buff=privptr->p_buff_read;
                for (i=0 ; i< privptr->p_env->read_buffers ; i++) {
                        p_buf        = p_free_chain;
                        p_free_chain = p_buf->next;

                        if (p_last_CCWB==NULL) {
                                p_buf->next=NULL;
                                real_TIC_address=0;
                                p_last_CCWB=p_buf;
                        }
                        else {
                                p_buf->next=p_first_CCWB;
                                real_TIC_address=
				(__u32)__pa(&p_first_CCWB -> read );
                        }

                        p_first_CCWB=p_buf;

                        p_buf->p_buffer=(struct clawbuf *)p_buff;
                        /*  initialize read command */
                        p_buf-> read.cmd_code = CCW_CLAW_CMD_READ;
                        p_buf-> read.cda = (__u32)__pa(p_buff);
                        p_buf-> read.flags = CCW_FLAG_SLI | CCW_FLAG_CC;
                        p_buf-> read.count       = privptr->p_env->read_size;

                        /*  initialize read_h command */
                        p_buf-> read_h.cmd_code = CCW_CLAW_CMD_READHEADER;
                        p_buf-> read_h.cda =
				(__u32)__pa(&(p_buf->header));
                        p_buf-> read_h.flags = CCW_FLAG_SLI | CCW_FLAG_CC;
                        p_buf-> read_h.count      = sizeof(struct clawh);

                        /*  initialize Signal command */
                        p_buf-> signal.cmd_code = CCW_CLAW_CMD_SIGNAL_SMOD;
                        p_buf-> signal.cda =
				(__u32)__pa(&(pClawH->flag));
                        p_buf-> signal.flags = CCW_FLAG_SLI | CCW_FLAG_CC;
                        p_buf-> signal.count     = 1;

                        /*  initialize r_TIC_1 command */
                        p_buf-> r_TIC_1.cmd_code = CCW_CLAW_CMD_TIC;
                        p_buf-> r_TIC_1.cda = (__u32)real_TIC_address;
                        p_buf-> r_TIC_1.flags = 0;
                        p_buf-> r_TIC_1.count      = 0;

                        /*  initialize r_read_FF command */
                        p_buf-> r_read_FF.cmd_code = CCW_CLAW_CMD_READFF;
                        p_buf-> r_read_FF.cda =
				(__u32)__pa(&(pClawH->flag));
                        p_buf-> r_read_FF.flags =
				CCW_FLAG_SLI | CCW_FLAG_CC | CCW_FLAG_PCI;
                        p_buf-> r_read_FF.count    = 1;

                        /*    initialize r_TIC_2          */
                        memcpy(&p_buf->r_TIC_2,
				&p_buf->r_TIC_1, sizeof(struct ccw1));

                        /*     initialize Header     */
                        p_buf->header.length=0xffff;
                        p_buf->header.opcode=0xff;
                        p_buf->header.flag=CLAW_PENDING;

			if (((unsigned long)p_buff+privptr->p_env->read_size) >=
			  ((unsigned long)(p_buff+2*(privptr->p_env->read_size)
				 -1)
			   & PAGE_MASK)) {
                                p_buff= p_buff+privptr->p_env->read_size;
                        }
                        else {
                                p_buff=
				(void *)((unsigned long)
					(p_buff+2*(privptr->p_env->read_size)-1)
					 & PAGE_MASK) ;
                        }
                }   /* for read_buffers   */
          }         /* read_size < PAGE_SIZE  */
          else {  /* read Size >= PAGE_SIZE  */
                for (i=0 ; i< privptr->p_env->read_buffers ; i++) {
                        p_buff = (void *)__get_free_pages(__GFP_DMA,
				(int)pages_to_order_of_mag(
					privptr->p_buff_pages_perread));
                        if (p_buff==NULL) {
                                free_pages((unsigned long)privptr->p_buff_ccw,
					(int)pages_to_order_of_mag(privptr->
					p_buff_ccw_num));
				/* free the write pages  */
	                        p_buf=privptr->p_buff_write;
                                while (p_buf!=NULL) {
					free_pages(
					    (unsigned long)p_buf->p_buffer,
					    (int)pages_to_order_of_mag(
					    privptr->p_buff_pages_perwrite));
                                        p_buf=p_buf->next;
                                }
				/* free any read pages already alloc  */
	                        p_buf=privptr->p_buff_read;
                                while (p_buf!=NULL) {
					free_pages(
					    (unsigned long)p_buf->p_buffer,
					    (int)pages_to_order_of_mag(
					     privptr->p_buff_pages_perread));
                                        p_buf=p_buf->next;
                                }
                                privptr->p_buff_ccw=NULL;
                                privptr->p_buff_write=NULL;
                                return -ENOMEM;
                        }
                        memset(p_buff, 0x00, privptr->p_env->read_size);
                        p_buf        = p_free_chain;
                        privptr->p_buff_read = p_buf;
                        p_free_chain = p_buf->next;

                        if (p_last_CCWB==NULL) {
                                p_buf->next=NULL;
                                real_TIC_address=0;
                                p_last_CCWB=p_buf;
                        }
                        else {
                                p_buf->next=p_first_CCWB;
                                real_TIC_address=
					(addr_t)__pa(
						&p_first_CCWB -> read );
                        }

                        p_first_CCWB=p_buf;
				/* save buff address */
                        p_buf->p_buffer=(struct clawbuf *)p_buff;
                        /*  initialize read command */
                        p_buf-> read.cmd_code = CCW_CLAW_CMD_READ;
                        p_buf-> read.cda = (__u32)__pa(p_buff);
                        p_buf-> read.flags = CCW_FLAG_SLI | CCW_FLAG_CC;
                        p_buf-> read.count       = privptr->p_env->read_size;

                        /*  initialize read_h command */
                        p_buf-> read_h.cmd_code = CCW_CLAW_CMD_READHEADER;
                        p_buf-> read_h.cda =
				(__u32)__pa(&(p_buf->header));
                        p_buf-> read_h.flags = CCW_FLAG_SLI | CCW_FLAG_CC;
                        p_buf-> read_h.count      = sizeof(struct clawh);

                        /*  initialize Signal command */
                        p_buf-> signal.cmd_code = CCW_CLAW_CMD_SIGNAL_SMOD;
                        p_buf-> signal.cda =
				(__u32)__pa(&(pClawH->flag));
                        p_buf-> signal.flags = CCW_FLAG_SLI | CCW_FLAG_CC;
                        p_buf-> signal.count     = 1;

                        /*  initialize r_TIC_1 command */
                        p_buf-> r_TIC_1.cmd_code = CCW_CLAW_CMD_TIC;
                        p_buf-> r_TIC_1.cda = (__u32)real_TIC_address;
                        p_buf-> r_TIC_1.flags = 0;
                        p_buf-> r_TIC_1.count      = 0;

                        /*  initialize r_read_FF command */
                        p_buf-> r_read_FF.cmd_code = CCW_CLAW_CMD_READFF;
                        p_buf-> r_read_FF.cda =
				(__u32)__pa(&(pClawH->flag));
                        p_buf-> r_read_FF.flags =
				CCW_FLAG_SLI | CCW_FLAG_CC | CCW_FLAG_PCI;
                        p_buf-> r_read_FF.count    = 1;

                        /*    initialize r_TIC_2          */
                        memcpy(&p_buf->r_TIC_2, &p_buf->r_TIC_1,
				sizeof(struct ccw1));

                        /*     initialize Header     */
                        p_buf->header.length=0xffff;
                        p_buf->header.opcode=0xff;
                        p_buf->header.flag=CLAW_PENDING;

                }    /* For read_buffers   */
          }     /*  read_size >= PAGE_SIZE   */
        }       /*  pBuffread = NULL */
        add_claw_reads( dev  ,p_first_CCWB , p_last_CCWB);
	privptr->buffs_alloc = 1;

        return 0;
}    /*    end of init_ccw_bk */

/*-------------------------------------------------------------------*
*                                                                    *
*       probe_error                                                  *
*                                                                    *
*--------------------------------------------------------------------*/

static void
probe_error( struct ccwgroup_device *cgdev)
{
	struct claw_privbk *privptr;

	CLAW_DBF_TEXT(4, trace, "proberr");
	privptr = dev_get_drvdata(&cgdev->dev);
	if (privptr != NULL) {
		dev_set_drvdata(&cgdev->dev, NULL);
		kfree(privptr->p_env);
		kfree(privptr->p_mtc_envelope);
		kfree(privptr);
	}
}    /*    probe_error    */

/*-------------------------------------------------------------------*
*    claw_process_control                                            *
*                                                                    *
*                                                                    *
*--------------------------------------------------------------------*/

static int
claw_process_control( struct net_device *dev, struct ccwbk * p_ccw)
{

        struct clawbuf *p_buf;
        struct clawctl  ctlbk;
        struct clawctl *p_ctlbk;
        char    temp_host_name[8];
        char    temp_ws_name[8];
        struct claw_privbk *privptr;
        struct claw_env *p_env;
        struct sysval *p_sysval;
        struct conncmd *p_connect=NULL;
        int rc;
        struct chbk *p_ch = NULL;
	struct device *tdev;
	CLAW_DBF_TEXT(2, setup, "clw_cntl");
        udelay(1000);  /* Wait a ms for the control packets to
			*catch up to each other */
	privptr = dev->ml_priv;
        p_env=privptr->p_env;
	tdev = &privptr->channel[READ].cdev->dev;
	memcpy( &temp_host_name, p_env->host_name, 8);
        memcpy( &temp_ws_name, p_env->adapter_name , 8);
	dev_info(tdev, "%s: CLAW device %.8s: "
		"Received Control Packet\n",
		dev->name, temp_ws_name);
        if (privptr->release_pend==1) {
                return 0;
        }
        p_buf=p_ccw->p_buffer;
        p_ctlbk=&ctlbk;
	if (p_env->packing == DO_PACKED) { /* packing in progress?*/
		memcpy(p_ctlbk, &p_buf->buffer[4], sizeof(struct clawctl));
	} else {
		memcpy(p_ctlbk, p_buf, sizeof(struct clawctl));
	}
        switch (p_ctlbk->command)
        {
	case SYSTEM_VALIDATE_REQUEST:
		if (p_ctlbk->version != CLAW_VERSION_ID) {
			claw_snd_sys_validate_rsp(dev, p_ctlbk,
				CLAW_RC_WRONG_VERSION);
			dev_warn(tdev, "The communication peer of %s"
				" uses an incorrect API version %d\n",
				dev->name, p_ctlbk->version);
		}
		p_sysval = (struct sysval *)&(p_ctlbk->data);
		dev_info(tdev, "%s: Recv Sys Validate Request: "
			"Vers=%d,link_id=%d,Corr=%d,WS name=%.8s,"
			"Host name=%.8s\n",
			dev->name, p_ctlbk->version,
			p_ctlbk->linkid,
			p_ctlbk->correlator,
			p_sysval->WS_name,
			p_sysval->host_name);
		if (memcmp(temp_host_name, p_sysval->host_name, 8)) {
			claw_snd_sys_validate_rsp(dev, p_ctlbk,
				CLAW_RC_NAME_MISMATCH);
			CLAW_DBF_TEXT(2, setup, "HSTBAD");
			CLAW_DBF_TEXT_(2, setup, "%s", p_sysval->host_name);
			CLAW_DBF_TEXT_(2, setup, "%s", temp_host_name);
			dev_warn(tdev,
				"Host name %s for %s does not match the"
				" remote adapter name %s\n",
				p_sysval->host_name,
				dev->name,
				temp_host_name);
		}
		if (memcmp(temp_ws_name, p_sysval->WS_name, 8)) {
			claw_snd_sys_validate_rsp(dev, p_ctlbk,
				CLAW_RC_NAME_MISMATCH);
			CLAW_DBF_TEXT(2, setup, "WSNBAD");
			CLAW_DBF_TEXT_(2, setup, "%s", p_sysval->WS_name);
			CLAW_DBF_TEXT_(2, setup, "%s", temp_ws_name);
			dev_warn(tdev, "Adapter name %s for %s does not match"
				" the remote host name %s\n",
				p_sysval->WS_name,
				dev->name,
				temp_ws_name);
		}
		if ((p_sysval->write_frame_size < p_env->write_size) &&
		    (p_env->packing == 0)) {
			claw_snd_sys_validate_rsp(dev, p_ctlbk,
				CLAW_RC_HOST_RCV_TOO_SMALL);
			dev_warn(tdev,
				"The local write buffer is smaller than the"
				" remote read buffer\n");
			CLAW_DBF_TEXT(2, setup, "wrtszbad");
		}
		if ((p_sysval->read_frame_size < p_env->read_size) &&
		    (p_env->packing == 0)) {
			claw_snd_sys_validate_rsp(dev, p_ctlbk,
				CLAW_RC_HOST_RCV_TOO_SMALL);
			dev_warn(tdev,
				"The local read buffer is smaller than the"
				" remote write buffer\n");
			CLAW_DBF_TEXT(2, setup, "rdsizbad");
		}
		claw_snd_sys_validate_rsp(dev, p_ctlbk, 0);
		dev_info(tdev,
			"CLAW device %.8s: System validate"
			" completed.\n", temp_ws_name);
		dev_info(tdev,
			"%s: sys Validate Rsize:%d Wsize:%d\n",
			dev->name, p_sysval->read_frame_size,
			p_sysval->write_frame_size);
		privptr->system_validate_comp = 1;
		if (strncmp(p_env->api_type, WS_APPL_NAME_PACKED, 6) == 0)
			p_env->packing = PACKING_ASK;
		claw_strt_conn_req(dev);
		break;
	case SYSTEM_VALIDATE_RESPONSE:
		p_sysval = (struct sysval *)&(p_ctlbk->data);
		dev_info(tdev,
			"Settings for %s validated (version=%d, "
			"remote device=%d, rc=%d, adapter name=%.8s, "
			"host name=%.8s)\n",
			dev->name,
			p_ctlbk->version,
			p_ctlbk->correlator,
			p_ctlbk->rc,
			p_sysval->WS_name,
			p_sysval->host_name);
		switch (p_ctlbk->rc) {
		case 0:
			dev_info(tdev, "%s: CLAW device "
				"%.8s: System validate completed.\n",
				dev->name, temp_ws_name);
			if (privptr->system_validate_comp == 0)
				claw_strt_conn_req(dev);
			privptr->system_validate_comp = 1;
			break;
		case CLAW_RC_NAME_MISMATCH:
			dev_warn(tdev, "Validating %s failed because of"
				" a host or adapter name mismatch\n",
				dev->name);
			break;
		case CLAW_RC_WRONG_VERSION:
			dev_warn(tdev, "Validating %s failed because of a"
				" version conflict\n",
				dev->name);
			break;
		case CLAW_RC_HOST_RCV_TOO_SMALL:
			dev_warn(tdev, "Validating %s failed because of a"
				" frame size conflict\n",
				dev->name);
			break;
		default:
			dev_warn(tdev, "The communication peer of %s rejected"
				" the connection\n",
				 dev->name);
			break;
		}
		break;

	case CONNECTION_REQUEST:
		p_connect = (struct conncmd *)&(p_ctlbk->data);
		dev_info(tdev, "%s: Recv Conn Req: Vers=%d,link_id=%d,"
			"Corr=%d,HOST appl=%.8s,WS appl=%.8s\n",
			dev->name,
			p_ctlbk->version,
			p_ctlbk->linkid,
			p_ctlbk->correlator,
			p_connect->host_name,
			p_connect->WS_name);
		if (privptr->active_link_ID != 0) {
			claw_snd_disc(dev, p_ctlbk);
			dev_info(tdev, "%s rejected a connection request"
				" because it is already active\n",
				dev->name);
		}
		if (p_ctlbk->linkid != 1) {
			claw_snd_disc(dev, p_ctlbk);
			dev_info(tdev, "%s rejected a request to open multiple"
				" connections\n",
				dev->name);
		}
		rc = find_link(dev, p_connect->host_name, p_connect->WS_name);
		if (rc != 0) {
			claw_snd_disc(dev, p_ctlbk);
			dev_info(tdev, "%s rejected a connection request"
				" because of a type mismatch\n",
				dev->name);
		}
		claw_send_control(dev,
			CONNECTION_CONFIRM, p_ctlbk->linkid,
			p_ctlbk->correlator,
			0, p_connect->host_name,
			p_connect->WS_name);
		if (p_env->packing == PACKING_ASK) {
			p_env->packing = PACK_SEND;
			claw_snd_conn_req(dev, 0);
		}
		dev_info(tdev, "%s: CLAW device %.8s: Connection "
			"completed link_id=%d.\n",
			dev->name, temp_ws_name,
			p_ctlbk->linkid);
			privptr->active_link_ID = p_ctlbk->linkid;
			p_ch = &privptr->channel[WRITE];
			wake_up(&p_ch->wait);  /* wake up claw_open ( WRITE) */
		break;
	case CONNECTION_RESPONSE:
		p_connect = (struct conncmd *)&(p_ctlbk->data);
		dev_info(tdev, "%s: Recv Conn Resp: Vers=%d,link_id=%d,"
			"Corr=%d,RC=%d,Host appl=%.8s, WS appl=%.8s\n",
			dev->name,
			p_ctlbk->version,
			p_ctlbk->linkid,
			p_ctlbk->correlator,
			p_ctlbk->rc,
			p_connect->host_name,
			p_connect->WS_name);

		if (p_ctlbk->rc != 0) {
			dev_warn(tdev, "The communication peer of %s rejected"
				" a connection request\n",
				dev->name);
			return 1;
		}
		rc = find_link(dev,
			p_connect->host_name, p_connect->WS_name);
		if (rc != 0) {
			claw_snd_disc(dev, p_ctlbk);
			dev_warn(tdev, "The communication peer of %s"
				" rejected a connection "
				"request because of a type mismatch\n",
				 dev->name);
		}
		/* should be until CONNECTION_CONFIRM */
		privptr->active_link_ID = -(p_ctlbk->linkid);
		break;
	case CONNECTION_CONFIRM:
		p_connect = (struct conncmd *)&(p_ctlbk->data);
		dev_info(tdev,
			"%s: Recv Conn Confirm:Vers=%d,link_id=%d,"
			"Corr=%d,Host appl=%.8s,WS appl=%.8s\n",
			dev->name,
			p_ctlbk->version,
			p_ctlbk->linkid,
			p_ctlbk->correlator,
			p_connect->host_name,
			p_connect->WS_name);
		if (p_ctlbk->linkid == -(privptr->active_link_ID)) {
			privptr->active_link_ID = p_ctlbk->linkid;
			if (p_env->packing > PACKING_ASK) {
				dev_info(tdev,
				"%s: Confirmed Now packing\n", dev->name);
				p_env->packing = DO_PACKED;
			}
			p_ch = &privptr->channel[WRITE];
			wake_up(&p_ch->wait);
		} else {
			dev_warn(tdev, "Activating %s failed because of"
				" an incorrect link ID=%d\n",
				dev->name, p_ctlbk->linkid);
			claw_snd_disc(dev, p_ctlbk);
		}
		break;
	case DISCONNECT:
		dev_info(tdev, "%s: Disconnect: "
			"Vers=%d,link_id=%d,Corr=%d\n",
			dev->name, p_ctlbk->version,
			p_ctlbk->linkid, p_ctlbk->correlator);
		if ((p_ctlbk->linkid == 2) &&
		    (p_env->packing == PACK_SEND)) {
			privptr->active_link_ID = 1;
			p_env->packing = DO_PACKED;
		} else
			privptr->active_link_ID = 0;
		break;
	case CLAW_ERROR:
		dev_warn(tdev, "The communication peer of %s failed\n",
			dev->name);
		break;
	default:
		dev_warn(tdev, "The communication peer of %s sent"
			" an unknown command code\n",
			dev->name);
		break;
        }

        return 0;
}   /*    end of claw_process_control    */


/*-------------------------------------------------------------------*
*               claw_send_control                                    *
*                                                                    *
*--------------------------------------------------------------------*/

static int
claw_send_control(struct net_device *dev, __u8 type, __u8 link,
	 __u8 correlator, __u8 rc, char *local_name, char *remote_name)
{
        struct claw_privbk 		*privptr;
        struct clawctl                  *p_ctl;
        struct sysval                   *p_sysval;
        struct conncmd                  *p_connect;
        struct sk_buff 			*skb;

	CLAW_DBF_TEXT(2, setup, "sndcntl");
	privptr = dev->ml_priv;
        p_ctl=(struct clawctl *)&privptr->ctl_bk;

        p_ctl->command=type;
        p_ctl->version=CLAW_VERSION_ID;
        p_ctl->linkid=link;
        p_ctl->correlator=correlator;
        p_ctl->rc=rc;

        p_sysval=(struct sysval *)&p_ctl->data;
        p_connect=(struct conncmd *)&p_ctl->data;

        switch (p_ctl->command) {
                case SYSTEM_VALIDATE_REQUEST:
                case SYSTEM_VALIDATE_RESPONSE:
                        memcpy(&p_sysval->host_name, local_name, 8);
                        memcpy(&p_sysval->WS_name, remote_name, 8);
			if (privptr->p_env->packing > 0) {
				p_sysval->read_frame_size = DEF_PACK_BUFSIZE;
				p_sysval->write_frame_size = DEF_PACK_BUFSIZE;
			} else {
				/* how big is the biggest group of packets */
			   p_sysval->read_frame_size =
				privptr->p_env->read_size;
			   p_sysval->write_frame_size =
				privptr->p_env->write_size;
			}
                        memset(&p_sysval->reserved, 0x00, 4);
                        break;
                case CONNECTION_REQUEST:
                case CONNECTION_RESPONSE:
                case CONNECTION_CONFIRM:
                case DISCONNECT:
                        memcpy(&p_sysval->host_name, local_name, 8);
                        memcpy(&p_sysval->WS_name, remote_name, 8);
			if (privptr->p_env->packing > 0) {
			/* How big is the biggest packet */
				p_connect->reserved1[0]=CLAW_FRAME_SIZE;
                        	p_connect->reserved1[1]=CLAW_FRAME_SIZE;
			} else {
	                        memset(&p_connect->reserved1, 0x00, 4);
        	                memset(&p_connect->reserved2, 0x00, 4);
			}
                        break;
                default:
                        break;
        }

        /*      write Control Record to the device                   */


        skb = dev_alloc_skb(sizeof(struct clawctl));
        if (!skb) {
                return -ENOMEM;
        }
	memcpy(skb_put(skb, sizeof(struct clawctl)),
		p_ctl, sizeof(struct clawctl));
	if (privptr->p_env->packing >= PACK_SEND)
		claw_hw_tx(skb, dev, 1);
	else
        	claw_hw_tx(skb, dev, 0);
        return 0;
}  /*   end of claw_send_control  */

/*-------------------------------------------------------------------*
*               claw_snd_conn_req                                    *
*                                                                    *
*--------------------------------------------------------------------*/
static int
claw_snd_conn_req(struct net_device *dev, __u8 link)
{
        int                rc;
	struct claw_privbk *privptr = dev->ml_priv;
        struct clawctl 	   *p_ctl;

	CLAW_DBF_TEXT(2, setup, "snd_conn");
	rc = 1;
        p_ctl=(struct clawctl *)&privptr->ctl_bk;
	p_ctl->linkid = link;
        if ( privptr->system_validate_comp==0x00 ) {
                return rc;
        }
	if (privptr->p_env->packing == PACKING_ASK )
		rc=claw_send_control(dev, CONNECTION_REQUEST,0,0,0,
        		WS_APPL_NAME_PACKED, WS_APPL_NAME_PACKED);
	if (privptr->p_env->packing == PACK_SEND)  {
		rc=claw_send_control(dev, CONNECTION_REQUEST,0,0,0,
        		WS_APPL_NAME_IP_NAME, WS_APPL_NAME_IP_NAME);
	}
	if (privptr->p_env->packing == 0)
        	rc=claw_send_control(dev, CONNECTION_REQUEST,0,0,0,
       			HOST_APPL_NAME, privptr->p_env->api_type);
        return rc;

}  /*  end of claw_snd_conn_req */


/*-------------------------------------------------------------------*
*               claw_snd_disc                                        *
*                                                                    *
*--------------------------------------------------------------------*/

static int
claw_snd_disc(struct net_device *dev, struct clawctl * p_ctl)
{
        int rc;
        struct conncmd *  p_connect;

	CLAW_DBF_TEXT(2, setup, "snd_dsc");
        p_connect=(struct conncmd *)&p_ctl->data;

        rc=claw_send_control(dev, DISCONNECT, p_ctl->linkid,
		p_ctl->correlator, 0,
                p_connect->host_name, p_connect->WS_name);
        return rc;
}     /*   end of claw_snd_disc    */


/*-------------------------------------------------------------------*
*               claw_snd_sys_validate_rsp                            *
*                                                                    *
*--------------------------------------------------------------------*/

static int
claw_snd_sys_validate_rsp(struct net_device *dev,
	struct clawctl *p_ctl, __u32 return_code)
{
        struct claw_env *  p_env;
        struct claw_privbk *privptr;
        int    rc;

	CLAW_DBF_TEXT(2, setup, "chkresp");
	privptr = dev->ml_priv;
        p_env=privptr->p_env;
        rc=claw_send_control(dev, SYSTEM_VALIDATE_RESPONSE,
		p_ctl->linkid,
		p_ctl->correlator,
                return_code,
		p_env->host_name,
		p_env->adapter_name  );
        return rc;
}     /*    end of claw_snd_sys_validate_rsp    */

/*-------------------------------------------------------------------*
*               claw_strt_conn_req                                   *
*                                                                    *
*--------------------------------------------------------------------*/

static int
claw_strt_conn_req(struct net_device *dev )
{
        int rc;

	CLAW_DBF_TEXT(2, setup, "conn_req");
        rc=claw_snd_conn_req(dev, 1);
        return rc;
}    /*   end of claw_strt_conn_req   */



/*-------------------------------------------------------------------*
 *   claw_stats                                                      *
 *-------------------------------------------------------------------*/

static struct
net_device_stats *claw_stats(struct net_device *dev)
{
        struct claw_privbk *privptr;

	CLAW_DBF_TEXT(4, trace, "stats");
	privptr = dev->ml_priv;
        return &privptr->stats;
}     /*   end of claw_stats   */


/*-------------------------------------------------------------------*
*       unpack_read                                                  *
*                                                                    *
*--------------------------------------------------------------------*/
static void
unpack_read(struct net_device *dev )
{
        struct sk_buff *skb;
        struct claw_privbk *privptr;
	struct claw_env    *p_env;
        struct ccwbk 	*p_this_ccw;
        struct ccwbk 	*p_first_ccw;
        struct ccwbk 	*p_last_ccw;
	struct clawph 	*p_packh;
	void		*p_packd;
	struct clawctl 	*p_ctlrec=NULL;
	struct device	*p_dev;

        __u32	len_of_data;
	__u32	pack_off;
        __u8	link_num;
        __u8 	mtc_this_frm=0;
        __u32	bytes_to_mov;
        int	i=0;
	int     p=0;

	CLAW_DBF_TEXT(4, trace, "unpkread");
        p_first_ccw=NULL;
        p_last_ccw=NULL;
	p_packh=NULL;
	p_packd=NULL;
	privptr = dev->ml_priv;

	p_dev = &privptr->channel[READ].cdev->dev;
	p_env = privptr->p_env;
        p_this_ccw=privptr->p_read_active_first;
	while (p_this_ccw!=NULL && p_this_ccw->header.flag!=CLAW_PENDING) {
		pack_off = 0;
		p = 0;
		p_this_ccw->header.flag=CLAW_PENDING;
		privptr->p_read_active_first=p_this_ccw->next;
                p_this_ccw->next=NULL;
		p_packh = (struct clawph *)p_this_ccw->p_buffer;
		if ((p_env->packing == PACK_SEND) &&
		    (p_packh->len == 32)           &&
		    (p_packh->link_num == 0)) {   /* is it a packed ctl rec? */
			p_packh++;  /* peek past pack header */
			p_ctlrec = (struct clawctl *)p_packh;
			p_packh--;  /* un peek */
			if ((p_ctlrec->command == CONNECTION_RESPONSE) ||
		            (p_ctlrec->command == CONNECTION_CONFIRM))
				p_env->packing = DO_PACKED;
		}
		if (p_env->packing == DO_PACKED)
			link_num=p_packh->link_num;
		else
	                link_num=p_this_ccw->header.opcode / 8;
                if ((p_this_ccw->header.opcode & MORE_to_COME_FLAG)!=0) {
                        mtc_this_frm=1;
                        if (p_this_ccw->header.length!=
				privptr->p_env->read_size ) {
				dev_warn(p_dev,
					"The communication peer of %s"
					" sent a faulty"
					" frame of length %02x\n",
                                        dev->name, p_this_ccw->header.length);
                        }
                }

                if (privptr->mtc_skipping) {
                        /*
                        *   We're in the mode of skipping past a
			*   multi-frame message
                        *   that we can't process for some reason or other.
                        *   The first frame without the More-To-Come flag is
			*   the last frame of the skipped message.
                        */
                        /*  in case of More-To-Come not set in this frame */
                        if (mtc_this_frm==0) {
                                privptr->mtc_skipping=0; /* Ok, the end */
                                privptr->mtc_logical_link=-1;
                        }
                        goto NextFrame;
                }

                if (link_num==0) {
                        claw_process_control(dev, p_this_ccw);
			CLAW_DBF_TEXT(4, trace, "UnpkCntl");
                        goto NextFrame;
                }
unpack_next:
		if (p_env->packing == DO_PACKED) {
			if (pack_off > p_env->read_size)
				goto NextFrame;
			p_packd = p_this_ccw->p_buffer+pack_off;
			p_packh = (struct clawph *) p_packd;
			if ((p_packh->len == 0) || /* done with this frame? */
			    (p_packh->flag != 0))
				goto NextFrame;
			bytes_to_mov = p_packh->len;
			pack_off += bytes_to_mov+sizeof(struct clawph);
			p++;
		} else {
                	bytes_to_mov=p_this_ccw->header.length;
		}
                if (privptr->mtc_logical_link<0) {

                /*
                *  if More-To-Come is set in this frame then we don't know
                *  length of entire message, and hence have to allocate
		*  large buffer   */

                /*      We are starting a new envelope  */
                privptr->mtc_offset=0;
                        privptr->mtc_logical_link=link_num;
                }

                if (bytes_to_mov > (MAX_ENVELOPE_SIZE- privptr->mtc_offset) ) {
                        /*      error     */
                        privptr->stats.rx_frame_errors++;
                        goto NextFrame;
                }
		if (p_env->packing == DO_PACKED) {
			memcpy( privptr->p_mtc_envelope+ privptr->mtc_offset,
				p_packd+sizeof(struct clawph), bytes_to_mov);

		} else	{
                	memcpy( privptr->p_mtc_envelope+ privptr->mtc_offset,
                        	p_this_ccw->p_buffer, bytes_to_mov);
		}
                if (mtc_this_frm==0) {
                        len_of_data=privptr->mtc_offset+bytes_to_mov;
                        skb=dev_alloc_skb(len_of_data);
                        if (skb) {
                                memcpy(skb_put(skb,len_of_data),
					privptr->p_mtc_envelope,
					len_of_data);
                                skb->dev=dev;
				skb_reset_mac_header(skb);
                                skb->protocol=htons(ETH_P_IP);
                                skb->ip_summed=CHECKSUM_UNNECESSARY;
                                privptr->stats.rx_packets++;
				privptr->stats.rx_bytes+=len_of_data;
                                netif_rx(skb);
                        }
                        else {
				dev_info(p_dev, "Allocating a buffer for"
					" incoming data failed\n");
                                privptr->stats.rx_dropped++;
                        }
                        privptr->mtc_offset=0;
                        privptr->mtc_logical_link=-1;
                }
                else {
                        privptr->mtc_offset+=bytes_to_mov;
                }
		if (p_env->packing == DO_PACKED)
			goto unpack_next;
NextFrame:
                /*
                *   Remove ThisCCWblock from active read queue, and add it
                *   to queue of free blocks to be reused.
                */
                i++;
                p_this_ccw->header.length=0xffff;
                p_this_ccw->header.opcode=0xff;
                /*
                *       add this one to the free queue for later reuse
                */
                if (p_first_ccw==NULL) {
                        p_first_ccw = p_this_ccw;
                }
                else {
                        p_last_ccw->next = p_this_ccw;
                }
                p_last_ccw = p_this_ccw;
                /*
                *       chain to next block on active read queue
                */
                p_this_ccw = privptr->p_read_active_first;
		CLAW_DBF_TEXT_(4, trace, "rxpkt %d", p);
        } /* end of while */

        /*      check validity                  */

	CLAW_DBF_TEXT_(4, trace, "rxfrm %d", i);
        add_claw_reads(dev, p_first_ccw, p_last_ccw);
        claw_strt_read(dev, LOCK_YES);
        return;
}     /*  end of unpack_read   */

/*-------------------------------------------------------------------*
*       claw_strt_read                                               *
*                                                                    *
*--------------------------------------------------------------------*/
static void
claw_strt_read (struct net_device *dev, int lock )
{
        int        rc = 0;
        __u32      parm;
        unsigned long  saveflags = 0;
	struct claw_privbk *privptr = dev->ml_priv;
        struct ccwbk*p_ccwbk;
        struct chbk *p_ch;
        struct clawh *p_clawh;
        p_ch=&privptr->channel[READ];

	CLAW_DBF_TEXT(4, trace, "StRdNter");
        p_clawh=(struct clawh *)privptr->p_claw_signal_blk;
        p_clawh->flag=CLAW_IDLE;    /* 0x00 */

        if ((privptr->p_write_active_first!=NULL &&
             privptr->p_write_active_first->header.flag!=CLAW_PENDING) ||
            (privptr->p_read_active_first!=NULL &&
             privptr->p_read_active_first->header.flag!=CLAW_PENDING )) {
                p_clawh->flag=CLAW_BUSY;    /* 0xff */
        }
        if (lock==LOCK_YES) {
                spin_lock_irqsave(get_ccwdev_lock(p_ch->cdev), saveflags);
        }
        if (test_and_set_bit(0, (void *)&p_ch->IO_active) == 0) {
		CLAW_DBF_TEXT(4, trace, "HotRead");
                p_ccwbk=privptr->p_read_active_first;
                parm = (unsigned long) p_ch;
                rc = ccw_device_start (p_ch->cdev, &p_ccwbk->read, parm,
				       0xff, 0);
                if (rc != 0) {
                        ccw_check_return_code(p_ch->cdev, rc);
                }
        }
	else {
		CLAW_DBF_TEXT(2, trace, "ReadAct");
	}

        if (lock==LOCK_YES) {
                spin_unlock_irqrestore(get_ccwdev_lock(p_ch->cdev), saveflags);
        }
	CLAW_DBF_TEXT(4, trace, "StRdExit");
        return;
}       /*    end of claw_strt_read    */

/*-------------------------------------------------------------------*
*       claw_strt_out_IO                                             *
*                                                                    *
*--------------------------------------------------------------------*/

static void
claw_strt_out_IO( struct net_device *dev )
{
        int             	rc = 0;
        unsigned long   	parm;
        struct claw_privbk 	*privptr;
        struct chbk     	*p_ch;
        struct ccwbk   	*p_first_ccw;

	if (!dev) {
		return;
	}
	privptr = (struct claw_privbk *)dev->ml_priv;
        p_ch=&privptr->channel[WRITE];

	CLAW_DBF_TEXT(4, trace, "strt_io");
        p_first_ccw=privptr->p_write_active_first;

        if (p_ch->claw_state == CLAW_STOP)
                return;
        if (p_first_ccw == NULL) {
                return;
        }
        if (test_and_set_bit(0, (void *)&p_ch->IO_active) == 0) {
                parm = (unsigned long) p_ch;
		CLAW_DBF_TEXT(2, trace, "StWrtIO");
		rc = ccw_device_start(p_ch->cdev, &p_first_ccw->write, parm,
				      0xff, 0);
                if (rc != 0) {
                        ccw_check_return_code(p_ch->cdev, rc);
                }
        }
        dev->trans_start = jiffies;
        return;
}       /*    end of claw_strt_out_IO    */

/*-------------------------------------------------------------------*
*       Free write buffers                                           *
*                                                                    *
*--------------------------------------------------------------------*/

static void
claw_free_wrt_buf( struct net_device *dev )
{

	struct claw_privbk *privptr = (struct claw_privbk *)dev->ml_priv;
        struct ccwbk*p_first_ccw;
	struct ccwbk*p_last_ccw;
	struct ccwbk*p_this_ccw;
	struct ccwbk*p_next_ccw;

	CLAW_DBF_TEXT(4, trace, "freewrtb");
        /*  scan the write queue to free any completed write packets   */
        p_first_ccw=NULL;
        p_last_ccw=NULL;
        p_this_ccw=privptr->p_write_active_first;
        while ( (p_this_ccw!=NULL) && (p_this_ccw->header.flag!=CLAW_PENDING))
        {
                p_next_ccw = p_this_ccw->next;
                if (((p_next_ccw!=NULL) &&
		     (p_next_ccw->header.flag!=CLAW_PENDING)) ||
                    ((p_this_ccw == privptr->p_write_active_last) &&
                     (p_this_ccw->header.flag!=CLAW_PENDING))) {
                        /* The next CCW is OK or this is  */
			/* the last CCW...free it   @A1A  */
                        privptr->p_write_active_first=p_this_ccw->next;
			p_this_ccw->header.flag=CLAW_PENDING;
                        p_this_ccw->next=privptr->p_write_free_chain;
			privptr->p_write_free_chain=p_this_ccw;
                        ++privptr->write_free_count;
			privptr->stats.tx_bytes+= p_this_ccw->write.count;
			p_this_ccw=privptr->p_write_active_first;
                        privptr->stats.tx_packets++;
                }
                else {
			break;
                }
        }
        if (privptr->write_free_count!=0) {
                claw_clearbit_busy(TB_NOBUFFER,dev);
        }
        /*   whole chain removed?   */
        if (privptr->p_write_active_first==NULL) {
                privptr->p_write_active_last=NULL;
        }
	CLAW_DBF_TEXT_(4, trace, "FWC=%d", privptr->write_free_count);
        return;
}

/*-------------------------------------------------------------------*
*       claw free netdevice                                          *
*                                                                    *
*--------------------------------------------------------------------*/
static void
claw_free_netdevice(struct net_device * dev, int free_dev)
{
	struct claw_privbk *et/cptr;

	CLAW_DBF_TEXT(2, setup, "free/*
 ");
	if (!*
 * 		return;SCON ON C netwo_rk driver
 *%s", dev->namex focies    =009
 *mlnc
 * for zSal coflags & IFF_RUNNINGs ve390/nrelease:
 *thor by:(s) Ori) {
	r(s) Ori->channel[READ].ndev = NULL;  /* say it's s) Or*/
	}
	al code writ.ibm.com
#ifdef MODULE    Res) OrLin by
 ries righ@us.  	Orig}
#endif*    Copyright BM Corp. 200x.x.wokux f}

/*nnnnnClaw init www
 *iceme  Initialize everything of the.x.wnal ice exceptpeOrigame andeg.
er_nchter < rp. s/s.
mwprstatic const.0201
 .x.w Linice_opss.ibm.*   ad20
 *=rrrr.ndo_open		=*   ho ada,9 LIN  stoppter_  Aucom>
 *Ks) Oriaget_d_busype     Tnnnnn
 *  whepiart_xmitype     Ttx
 *  whe200 ge_mtuype     T  are adde,
};
a*
 *ffevoid
.ibm.aaa
ost    ice( Oriom>
es clawr  *nal *  drnnnn
 *   write_buffer nnnd clLin   ho   Copyright IBM Corp. 2002,r thl co  Authorus.ibmtu =    CopEFAULT_MTU_SIZEocol.
 *hard_header_len = 0always uadd input (Read)
 *	type = ARPHRD_SLIPpe   nextx_queuennel an130d the nexKazuo =imuraPOINTOecond |the sNOAR aspe   law1
 *e LINUX&   haddresses u009
aa
 addressefirst_typ2er 25   aa hersion
 *addresses*   g a new0200 0.0.ineg.
 		Andy Ri200 0.0ri]   rs us@param c@us. The ccwMAX_DEV
to bp the dviouscol.
d addr0 on success, !	-   errorvioue is clawint
	chagrounelup to CLe   nnel ac*s us,int i,0201
 *.ibm.c
 *law.(s) Ori   rr    spechmber _ch; buffers toite_b_id		CL_id E*    Copyright s used for the
 *		CL_peci(&    -P
 * toco		Andy groupe   prev      = i+1; P
 *	d th is 1 WCLAWpe T2 reaiocatsed*    is decid_t add r na_ch->    e=ie
 *
.
 nprintf(ws1
 * d, speciIDspe T, "cl- the grname es n.
 * daptor nwrite_buffere
 id(t matc for  tor nam    devnoginal _id.peci2     Relowapi_rrb = kzalloc(sizeof e.
 *   irb),GFP_KERNEL)) =*
 *  .rrrr,_MAX_DE-ENOMEM is ctorrrr, 0
 *  dressess usSrp.  anX_DEerface *   read_se sisg
 *
dD_buffehter <buffe) Ori input bufsries nngeriremefies nfailuber Bunnel aMAX_DEsfor
 *	gured adays u to CLallopAW_MAX_DEV addput buffers tifies number of out ESG_COMPONENT "nvred enNote 
 *   AW_ input c   e <asm- ret support
 *		the Csm/idals.es nh thenfo(&defiddre *   	cha    %s\n",
		t_name must /io.hme
 *rand tch the connnnn
 *   write_buffer nned Pac*   ho		Andy )  adaapterdrvdataasm <linu
#italways_stive/if_arpincludncx/ct adclude <l,	15  Chae <linit.hnitclude <lludde <linuxWRITErrup.h>
#include <l by:e_typoutpustial rel   1DEVte siclud=>
#inclu alude <lafigurnt a thee     de <linux/kinternpe   rem <assienclude <oype.h>)tch the cipped
 uher protoh>
#include <linux/kpernel.sh>
#include <linux/kschedh>
#ininclude <linux/sigret =ays  ured ateinux/skbuff.0],0,ude <linux/kmodlude = 0nclude e <linux/ktcnclude <lude <l1],1x/timere "cu3088.h"
!<linrrrr,tivewarnp.h>
#includnclCreatted
a compsm/id1aclaw_
"
			"n 2.4ed withnumber.ibm.c%d/bit/debpile	goto oubug.me aaah>
#erdevice.hx/iponlicatenux/skbuff.roc_fw.h"st_nncluCopyruseevice ONENdbf file systup
*/Setes390asm/r typsubthe outper[255_dbf_s

d_bufferhar 201
ion[] __/ip.f_ar = "Copyr buffe";;

/**
 *  CLdebug_MAX_DEw_db];t_name  Debh>
#incility Stuff
mwprd_buffenregisinfo_t *390/ndbf_riveric void
wP or nregister(claw_db f_trace

/**
 *  CLAW Debug Facility functions
 */
static  useibdevicw*   ad 0,"NENT%d" drif 2 cumbe1upport      Reeries)
		debug_unregister(claw_dbf_sActivep;
statictracelaw_se ster(c\ive de90/nregis
#innregi*
 *   P
 *cu3088.h"
#linux/ip.h>
#include <li
#include <linux<linux/de "cu3088.h"
#<linuxkroc_fh>
#include <linuxug_facility();
		return -ENOMh>
#in>
#include <linux/ *		ysfs magicowerdriver
 SET_NETDEVput    	, cii_view);
	debug88.h(claw_y(void)
 2.6f
 */
statiNENT x.x.y(void)api_
#inv1, 8);nnnn
 *   write_ber("c, "reg
sta2 2);
for zSclaw_dbf_setu     -I=~he s <ithe sm/idReom>
ten-> clas_devicr_viewrrrrriver
 2ret=p = d*  Cbk   read_ity Stuff
 /
stati	un &nregishex_ascii_vi inlg_set_levelusy(structer(", 2) ESCrsion
 0;
}e;

/**
 inliccwmem
claw_ons
 */
stattic } esse expected
 *		and thabug_falude <(dev);
	eieio();
h>
#inev->ml_prnite_t
390/nchr_view( net_device u3088.h"
#dbf file system%s:bug_ost =%dbusy(stv)->tbuw_dbf"bug_k *)eric iclaw_dev)
{
setbug_=0x%04xtbit_brs/ses nbitousy = 1;opput ch	CLAW pro,l_priv)xbug__v)->_dbfr_view(390/nid *)&_bit(nr, (0 theumbes	netif_pi_tput ch(ivers/s390/neistatic _devicev
#inclcii_v&((inline v#include <lier_view(390/net/claw.)009
 *ml
 *	ls.h>:%.8s,ce.h>struc   gdevicet/clcted
  ad: t/cl  drsy));
}

static i*
 *;
	v)
{*)dev->rs/s390/nuct net_dev*)&(((strnet/rdevicesy;eue(devpilesice.dm>
 out:ignascii_view clawffbug_facility(void1
	clic inlinelaw_pritest_andev)
{*)dev->0		(vo;
/**
 * Dewww
 }tpe Tumbe0ic debecopurge_skb  1.15e.
 *   sk)
{
	devic *q   r
static ce;

/**
   -  *skb-       host namer4dev->ml_prhe Dut crrnostatic wht *c((skconf mww
 hods *q))tati}

static ikb_quatomic_dec(&skb->usersers/s3cwgr}

static in_kupportmetanyce * *q);
stati}llocat alwaShutdown0  Changes <linBAX_DEVux/tcp. addreinux/.15int clhut buff    2.6 Kh>
#ilude longrsionmpiles-   2.4aw_dldbf_ ccwgroup25  Add *cgbuffnux/api_support ccwgroup5setup
#m/io.e KMSG_COMPONENT  1)aw"
systelaw_priccwroup.h>
#inctic law_prraw_csm/idaluct ce(dev);
	eiude <linux/sname must "cu3088.h"rrn#incllude <linux/kh>
#include <l];
/**
inclule.hnclude <linux/kwww
 *tic inlc
 * and the pr

static )ic voit_devtup",/  aaose (claw_MAX_DE/atic deit(nr, (void *)&(((strestattic vbuffe
{
	nps( unsAW protocoic voirel	w     -Ithe s
{
 ((sa@jpb *irb) *
 *   adapt_wrieoid _ CLAW_e ou ( **
 *  dvicetruar_deviinline vn)
{
aw_e "cl ( strp == NULL ccwgr   APruct c>
#i, notet_de's15  int will dev)
{
	clear_bit(0, tic  inlructedx(str y funnt clawdeviruct net d;
st_check_rsions */
staticdeq);
st/*
 aa
 ieio()&(((stru);
stat}tic inline(struct claw_privbk *) dev-
		l_priivbkct net_devit(nr, (void *)&(((strnet/ctatic>
 * s* Functions for tinclvp x.x)
{
	eclawaddelaw_return_code void * p,vice newaddeurn_code (snt
	BUG_ON(!evice * protocol.
 *  e
 *
e <linthev);		CLstatic void*c *sk
	unsignccw_checintparm,hw_tx(stirb * rem;
_checrace voicleFunc)dev->ml_pritatic i willer <ressesd., 
claw by:v);
sta*
 *er_viCCWGROUP_ONLINE * p *
 *oup_device 390/nkidnt clar *owing e )_t *sruct irb *irb);
eaticclaw_id cmtc_vieelo *  drd_link(struct et/cpt=);
stativers/s390/netende (s__of_paghnaister(clasost how(stnd the p0].#ince_show(sr,st voi_attrib sk_buff *skdrivers/schar *b1f);
sute *uf);, *  CL*bt inlidevice *dev,
	strude <linux/ip.h>
#include <licode  int e <linux/ip.h>
#include <linux/kEM;
	}
	debuounturn_code (ev,
	structw_aduct e *aex_ascii_viewice *devpu_MAX_DEVruct irb *irb);
	netif_alloc
/ct ne
	cleauf);
sutesct net_devicsd *)&*dev);
hce *e *ate.
 *   cii_view);
,statiup_AX_DEV2law_hname *_tx(, thev,
	ufff *skb, struct net_device *devof_pagopdevcii_viet_devic clawprobe_error( struct cc  read_ct cistati = deb_irq_tasklet (
#include <lr_view(cetsy));
}she 8);lbuf(((st
{
	ev)
{
 claw_privic voi(struct device
	r 25
 *  Cce_atf, v,
	struice *dev,
	struct device_attpibute *aw(s
	t cl5
 aw_hname_, device *ice c ssize_t claw_hname_writruct dev device *dev,
	ufattribute *attr, char *buf);
s CLAWinline vtatic ssize_t claw_hname_wriice_attribute *auct dhar *rs/s >  inpNAME_LEN+1nclude <linuxINVAstatmemset(;
static ssize_te 0x20_write(str  CLnt cstrncpyufftatic ssize_t cribute
	conse <linux/rs/s390/n[uct d-1<lin voict sk_b str extra 0x0aclaw bect device_attridevice *dev,ute *act nennnn
 *   write_buffer nnHstnSetv
static int init_ccw_bk(struct net_dev,
	struct device device *uct d}
st_n FunctDEVICE_ATTR( ssize_t cla644,ce *device *	conribute *atsence_atllaw_ruff_show(struct deadttr,
	consstruct device_attribute *attr, char *buf);
static ize_t claw_rbuff_show(struct device *dev,
	struct ttr,
	const char *buf, size_t count);
static ssize_t claw_rbuff_show(struct device *dev,
	struct devicevice *k,
       __u8 correruct sk_bnt nr,strucof_pagw cla_name, char *re_u8v);
ce *dev,
	struct device_attribute *attr,
	const char *buf, size_t cout_devic int claw_snd_disc(struct net_device *dev, struct clawctl * p_ctl);
static int claw_snd_dev,
	struct device_atr_device *dev,
     mote_name);
static int claw_snd_conn_req(struct net_device *dev, __u8 link)(strucom>
date_rsp, ch *dev);
static void claw_free_wrt_buf(s2.6 K5  Agist_ctl);
static int cla reads   */
e_attribute *atadd_buff, char *remote_nament claw_r inline vocaw_wrmove_ev)
{
	return -EPERM;
}

/* *de ev, stAd_shutdSystem Vali rea);

ste_attribute *atbug e reads   */
uctw_tx(struct sk_buff *skbp table  wid
cce *dev);

ste_attribute    struk,nclu= 0xC3D3C1E6ruct sk_buff *skb_   st adbe,
pe *dev, int lock);
static void claw_strt_out_IO*local_name, char *remote_name);
static int claw_snd_conn_req(struct net_device *dev, __u8 link);
static int claw_snd_disc(struct net_device *dev, struct clawctl * p_ctl);
static int claw_snd_opsct net_devicevERM;
}

ch )ruct sk_buff *skncluevicove e *d*dev,
	struct device_attribute *attr,
	const char *buf,t_conn_reqnt return_codek_buff *sturn_code (ce *dev,
	t cc_ inpevice.             *
 *kb, stlock-------------------------out_IOevice.             *
 *--------------------
 * _wrviceft ccwgroup_device *cgdev)
 *dev, stion_shutdunpack eue(dev------------, "pro-----------------------ux for  THIS_MODULE,
 maw_hpaeue(devcwgroup_device *gdc stwe gew_bklo    EPERMbk *)/*;
staorivbk **/

static struct ccwgrife.
 ncmplaw_pri   1DEV;
	WS_APPLq(struPACKED,6ccwgC/
statict net_device_st=DEF voi(_BUF *cgdevinline void
clt/cl driver
 *probex_dbf_   1.00"proing=up, ING_ASKructclaw_privbk *) deorp. 200penfigutruct}
	else MAXCON CLAWc_envelod thCON CLAW netwo_(2,   CoFRstru
	privptr->p_mtv = krsion
de
 *
;         i.h>
p_ buffeof_pagic ssi bufpi = {
   }ct ccw_.ownDEV
r(cgd= THIS_arms:
        aw_probe  ue(dev);          .max_slavesdev)2    NODEV;
	 (id       .p       return -ENOMEM;
      = claw_probe,
        .rwibutek,
       __u8 correlator, __u8 rc , char *local_name, char *remote_name);
static int claw_snd_conn_req(struct net_evice *dev, __u8 link);
static int claw_snd_disc(struct net_device *dev, struct clawctl * p_ctl);
static int clawcility
 *   cpriv)->t_es(s   return -ENOMEM;
  AME_Sv->ada return -ENOMEM;
  ruct ccwemotis fTEXT(2,/* Fuallbk(strueachf
 */
.h>
#in->p_env->write_si-		probe_error(cgdev);
		put_device(&cgdev->dev);
		dew_prinnn,ma;
	rev, __u8 link);
static int claw_snd_disc(struct net_device *dev, struct clawctl * p_scantruct devi", &nnnstruct dwured host na(ease(smax = 64vptr->pizeoonfiv->p_p512onger ct ma%d",> v->p) ||IZE;cd< 2)nst char *buf, size_t 5;
	privptr->p_env- =(&cgwgroup_driver claw_group_driWbufs= {
        .owner       = THIS_MWB=_dbf= 5;
	ions *>p_priv ->re kzalloc(nv), GFP_K -ENOMEM;
	}
	privivptr->p_envptr-_FRAME_SIZE;
	p>
#i		probe_error     = claw_probe,
        .rr---------    T,WStr, chNOT_DEF,aw_sememcpy(d of claw_probe 
 *		the                    *
 *----------------------ted
 *	p-----------------------d of claw_probe c_envel     *skb, struct net_d CLAW_MAX_DEs /*  end of claw_probe    is claw
	struct clice *dev, charead_size = CLAW_FRAME_SIuct chbk *p_ch;

	CLAW_DBe_size = CLAW_FRAME_SIZE;
	rc = claw_add_files(&cgdev->dev);
	if (rc) {
		probe_error(cgdev);
		put_device(&cgdev->de = kaw_nerrlaw_09
 *-----"  seedbf_the ebug o_t *_shutded astup
"iles(&cgdev-  2.4ed\nux fo   */

/*---------------------------, rctr->p_mtc_e rc;rice@kb, struct net_de	retu = d of cl----------s);
   tati[0]->handler =dev,
	stru	rc = N;
	TX_BUSY;
	elice *dev, chDEV_TX_OK;
        rON CLAW networR------------bt ch0ux f---------rsion
Rvbk >
 *	 endear_d probe_erroccwdev_lock(p_ch->cdev), saveflags);
ice *dev, c-------------ot packing jIftruct"proruct sk_buff *skb201
 *online  = c    .pttr[<lin{
	h>
#i		pr_ice *dev, cct netss		probe_erivptr->p_env		probe_error(cgde reads   */
---ace;

/**
      
		CLAW		probe_error(cgders/s390/n		probe_ *        * Funct_tx(ut chot pack_  apicc) {
		pr *p
	eiUX390
		prf clc) {
		prw_skb,*held_se *dev);
_vies cla struct device_attr----rrror(cg------            >p_priv mpilesetif_      _c seencti_ch(_tx(->kobj_vie &d of clawch->read_size = struct nstruct skizeo_setnt	pkt_cnt,pk_ind,so_fa  ESCructgdevibm.com		/remssume no dt_queuruct sk----------------4,ter("c, "nux/SKBNODEV;/*-y(held		heet_ccp_enup
*_TEXT_(m.com
	MEM);t net_device *d!= DO_ice(*
rivptrsetup =  allop
{
	nupE,
 e DEV meput *clawurn t at m>
 t one;

s*
n(held_skb);
		else
			return NULL;
		if (p_env->packing != DO_PACKEE-*/---------- ch __exi*dev);

		/* g(ions   rrunctions */
u3088_discipbug_fter("c( strucbufff, sier("cunctions */nregisfaf (cla(tribut------"Drn NU unload net
claice *cgdev)ys uclaw_sataticutdow This APIcalode just aftef (cla */
		 API<mwprifor 2.6 Kernel No longer compiles on versiofte_buffMAX_DEV_up =  for t_ch KB we ue_hp.h>deb and tnd a plb);
Lo----i_w_sprivAW Deb
	CLAchturn_code ( we haveurn tedd_skb-tgrourb *irb)rrrr,		vefla"Rup
*		hpy(sstruc;
		S/390 ld_sk featur0, &(((eAW_DBuct neCopyrDregi Fd packeFRAME_SIv);
s/
nregi>dev);
	}
	veflor            The cmod no dice = d(&	CLA->collectput ch,eld_skb);
		ereturn NULL;
bufflenic_dec(ar;

	new_skb = NULL;		/&p_chbaf (hel
		i});
							held_skb->data,held_s	e, "clawtxer_is.tx_ata,hes++;f (hel device_urn NUe;

/**
_dbf_ inl *  CLAW Debug Facility functime aaa aw_tev)
}ta */
		n+someeturn hel);		probe__buf------);
			s
	/*arms:
_AUTHOR("annel andkb o<rnt clatic inltic votruc------*DESCRIPTION("utive _vie       zter("cto put\n" \dbf_se addresse2000,2008es used fortruct	pk_in       LICENSE("GPLtruc