/************************************************************************
 * Linux driver for                                                     *  
 * ICP vortex GmbH:    GDT ISA/EISA/PCI Disk Array Controllers          *
 * Intel Corporation:  Storage RAID Controllers                         *
 *                                                                      *
 * gdth.c                                                               *
 * Copyright (C) 1995-06 ICP vortex GmbH, Achim Leubner                 *
 * Copyright (C) 2002-04 Intel Corporation                              *
 * Copyright (C) 2003-06 Adaptec Inc.                                   *
 * <achim_leubner@adaptec.com>                                          *
 *                                                                      *
 * Additions/Fixes:                                                     *
 * Boji Tony Kannanthanam <boji.t.kannanthanam@intel.com>               *
 * Johannes Dinner <johannes_dinner@adaptec.com>                        *
 *                                                                      *
 * This program is free software; you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published    *
 * by the Free Software Foundation; either version 2 of the License,    *
 * or (at your option) any later version.                               *
 *                                                                      *
 * This program is distributed in the hope that it will be useful,      *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the         *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with this kernel; if not, write to the Free Software           *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.            *
 *                                                                      *
 * Linux kernel 2.6.x supported						*
 *                                                                      *
 ************************************************************************/

/* All GDT Disk Array Controllers are fully supported by this driver.
 * This includes the PCI/EISA/ISA SCSI Disk Array Controllers and the
 * PCI Fibre Channel Disk Array Controllers. See gdth.h for a complete
 * list of all controller types.
 * 
 * If you have one or more GDT3000/3020 EISA controllers with 
 * controller BIOS disabled, you have to set the IRQ values with the 
 * command line option "gdth=irq1,irq2,...", where the irq1,irq2,... are
 * the IRQ values for the EISA controllers.
 * 
 * After the optional list of IRQ values, other possible 
 * command line options are:
 * disable:Y                    disable driver
 * disable:N                    enable driver
 * reserve_mode:0               reserve no drives for the raw service
 * reserve_mode:1               reserve all not init., removable drives
 * reserve_mode:2               reserve all not init. drives
 * reserve_list:h,b,t,l,h,b,t,l,...     reserve particular drive(s) with 
 *                              h- controller no., b- channel no., 
 *                              t- target ID, l- LUN
 * reverse_scan:Y               reverse scan order for PCI controllers         
 * reverse_scan:N               scan PCI controllers like BIOS
 * max_ids:x                    x - target ID count per channel (1..MAXID)
 * rescan:Y                     rescan all channels/IDs 
 * rescan:N                     use all devices found until now
 * hdr_channel:x                x - number of virtual bus for host drives
 * shared_access:Y              disable driver reserve/release protocol to 
 *                              access a shared resource from several nodes, 
 *                              appropriate controller firmware required
 * shared_access:N              enable driver reserve/release protocol
 * probe_eisa_isa:Y             scan for EISA/ISA controllers
 * probe_eisa_isa:N             do not scan for EISA/ISA controllers
 * force_dma32:Y                use only 32 bit DMA mode
 * force_dma32:N                use 64 bit DMA mode, if supported
 *
 * The default values are: "gdth=disable:N,reserve_mode:1,reverse_scan:N,
 *                          max_ids:127,rescan:N,hdr_channel:0,
 *                          shared_access:Y,probe_eisa_isa:N,force_dma32:N".
 * Here is another example: "gdth=reserve_list:0,1,2,0,0,1,3,0,rescan:Y".
 * 
 * When loading the gdth driver as a module, the same options are available. 
 * You can set the IRQs with "IRQ=...". However, the syntax to specify the
 * options changes slightly. You must replace all ',' between options 
 * with ' ' and all ':' with '=' and you must use 
 * '1' in place of 'Y' and '0' in place of 'N'.
 * 
 * Default: "modprobe gdth disable=0 reserve_mode=1 reverse_scan=0
 *           max_ids=127 rescan=0 hdr_channel=0 shared_access=0
 *           probe_eisa_isa=0 force_dma32=0"
 * The other example: "modprobe gdth reserve_list=0,1,2,0,0,1,3,0 rescan=1".
 */

/* The meaning of the Scsi_Pointer members in this driver is as follows:
 * ptr:                     Chaining
 * this_residual:           unused
 * buffer:                  unused
 * dma_handle:              unused
 * buffers_residual:        unused
 * Status:                  unused
 * Message:                 unused
 * have_data_in:            unused
 * sent_command:            unused
 * phase:                   unused
 */


/* interrupt coalescing */
/* #define INT_COAL */

/* statistics */
#define GDTH_STATISTICS

#include <linux/module.h>

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/proc_fs.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/dma-mapping.h>
#include <linux/list.h>
#include <linux/smp_lock.h>

#ifdef GDTH_RTC
#include <linux/mc146818rtc.h>
#endif
#include <linux/reboot.h>

#include <asm/dma.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/spinlock.h>
#include <linux/blkdev.h>
#include <linux/scatterlist.h>

#include "scsi.h"
#include <scsi/scsi_host.h>
#include "gdth.h"

static void gdth_delay(int milliseconds);
static void gdth_eval_mapping(ulong32 size, ulong32 *cyls, int *heads, int *secs);
static irqreturn_t gdth_interrupt(int irq, void *dev_id);
static irqreturn_t __gdth_interrupt(gdth_ha_str *ha,
                                    int gdth_from_wait, int* pIndex);
static int gdth_sync_event(gdth_ha_str *ha, int service, unchar index,
                                                               Scsi_Cmnd *scp);
static int gdth_async_event(gdth_ha_str *ha);
static void gdth_log_event(gdth_evt_data *dvr, char *buffer);

static void gdth_putq(gdth_ha_str *ha, Scsi_Cmnd *scp, unchar priority);
static void gdth_next(gdth_ha_str *ha);
static int gdth_fill_raw_cmd(gdth_ha_str *ha, Scsi_Cmnd *scp, unchar b);
static int gdth_special_cmd(gdth_ha_str *ha, Scsi_Cmnd *scp);
static gdth_evt_str *gdth_store_event(gdth_ha_str *ha, ushort source,
                                      ushort idx, gdth_evt_data *evt);
static int gdth_read_event(gdth_ha_str *ha, int handle, gdth_evt_str *estr);
static void gdth_readapp_event(gdth_ha_str *ha, unchar application, 
                               gdth_evt_str *estr);
static void gdth_clear_events(void);

static void gdth_copy_internal_data(gdth_ha_str *ha, Scsi_Cmnd *scp,
                                    char *buffer, ushort count);
static int gdth_internal_cache_cmd(gdth_ha_str *ha, Scsi_Cmnd *scp);
static int gdth_fill_cache_cmd(gdth_ha_str *ha, Scsi_Cmnd *scp, ushort hdrive);

static void gdth_enable_int(gdth_ha_str *ha);
static int gdth_test_busy(gdth_ha_str *ha);
static int gdth_get_cmd_index(gdth_ha_str *ha);
static void gdth_release_event(gdth_ha_str *ha);
static int gdth_wait(gdth_ha_str *ha, int index,ulong32 time);
static int gdth_internal_cmd(gdth_ha_str *ha, unchar service, ushort opcode,
                                             ulong32 p1, ulong64 p2,ulong64 p3);
static int gdth_search_drives(gdth_ha_str *ha);
static int gdth_analyse_hdrive(gdth_ha_str *ha, ushort hdrive);

static const char *gdth_ctr_name(gdth_ha_str *ha);

static int gdth_open(struct inode *inode, struct file *filep);
static int gdth_close(struct inode *inode, struct file *filep);
static int gdth_ioctl(struct inode *inode, struct file *filep,
                      unsigned int cmd, unsigned long arg);

static void gdth_flush(gdth_ha_str *ha);
static int gdth_queuecommand(Scsi_Cmnd *scp,void (*done)(Scsi_Cmnd *));
static int __gdth_queuecommand(gdth_ha_str *ha, struct scsi_cmnd *scp,
				struct gdth_cmndinfo *cmndinfo);
static void gdth_scsi_done(struct scsi_cmnd *scp);

#ifdef DEBUG_GDTH
static unchar   DebugState = DEBUG_GDTH;

#ifdef __SERIAL__
#define MAX_SERBUF 160
static void ser_init(void);
static void ser_puts(char *str);
static void ser_putc(char c);
static int  ser_printk(const char *fmt, ...);
static char strbuf[MAX_SERBUF+1];
#ifdef __COM2__
#define COM_BASE 0x2f8
#else
#define COM_BASE 0x3f8
#endif
static void ser_init()
{
    unsigned port=COM_BASE;

    outb(0x80,port+3);
    outb(0,port+1);
    /* 19200 Baud, if 9600: outb(12,port) */
    outb(6, port);
    outb(3,port+3);
    outb(0,port+1);
    /*
    ser_putc('I');
    ser_putc(' ');
    */
}

static void ser_puts(char *str)
{
    char *ptr;

    ser_init();
    for (ptr=str;*ptr;++ptr)
        ser_putc(*ptr);
}

static void ser_putc(char c)
{
    unsigned port=COM_BASE;

    while ((inb(port+5) & 0x20)==0);
    outb(c,port);
    if (c==0x0a)
    {
        while ((inb(port+5) & 0x20)==0);
        outb(0x0d,port);
    }
}

static int ser_printk(const char *fmt, ...)
{
    va_list args;
    int i;

    va_start(args,fmt);
    i = vsprintf(strbuf,fmt,args);
    ser_puts(strbuf);
    va_end(args);
    return i;
}

#define TRACE(a)    {if (DebugState==1) {ser_printk a;}}
#define TRACE2(a)   {if (DebugState==1 || DebugState==2) {ser_printk a;}}
#define TRACE3(a)   {if (DebugState!=0) {ser_printk a;}}

#else /* !__SERIAL__ */
#define TRACE(a)    {if (DebugState==1) {printk a;}}
#define TRACE2(a)   {if (DebugState==1 || DebugState==2) {printk a;}}
#define TRACE3(a)   {if (DebugState!=0) {printk a;}}
#endif

#else /* !DEBUG */
#define TRACE(a)
#define TRACE2(a)
#define TRACE3(a)
#endif

#ifdef GDTH_STATISTICS
static ulong32 max_rq=0, max_index=0, max_sg=0;
#ifdef INT_COAL
static ulong32 max_int_coal=0;
#endif
static ulong32 act_ints=0, act_ios=0, act_stats=0, act_rq=0;
static struct timer_list gdth_timer;
#endif

#define PTR2USHORT(a)   (ushort)(ulong)(a)
#define GDTOFFSOF(a,b)  (size_t)&(((a*)0)->b)
#define INDEX_OK(i,t)   ((i)<ARRAY_SIZE(t))

#define BUS_L2P(a,b)    ((b)>(a)->virt_bus ? (b-1):(b))

#ifdef CONFIG_ISA
static unchar   gdth_drq_tab[4] = {5,6,7,7};            /* DRQ table */
#endif
#if defined(CONFIG_EISA) || defined(CONFIG_ISA)
static unchar   gdth_irq_tab[6] = {0,10,11,12,14,0};    /* IRQ table */
#endif
static unchar   gdth_polling;                           /* polling if TRUE */
static int      gdth_ctr_count  = 0;                    /* controller count */
static LIST_HEAD(gdth_instances);                       /* controller list */
static unchar   gdth_write_through = FALSE;             /* write through */
static gdth_evt_str ebuffer[MAX_EVENTS];                /* event buffer */
static int elastidx;
static int eoldidx;
static int major;

#define DIN     1                               /* IN data direction */
#define DOU     2                               /* OUT data direction */
#define DNO     DIN                             /* no data transfer */
#define DUN     DIN                             /* unknown data direction */
static unchar gdth_direction_tab[0x100] = {
    DNO,DNO,DIN,DIN,DOU,DIN,DIN,DOU,DIN,DUN,DOU,DOU,DUN,DUN,DUN,DIN,
    DNO,DIN,DIN,DOU,DIN,DOU,DNO,DNO,DOU,DNO,DIN,DNO,DIN,DOU,DNO,DUN,
    DIN,DUN,DIN,DUN,DOU,DIN,DUN,DUN,DIN,DIN,DOU,DNO,DUN,DIN,DOU,DOU,
    DOU,DOU,DOU,DNO,DIN,DNO,DNO,DIN,DOU,DOU,DOU,DOU,DIN,DOU,DIN,DOU,
    DOU,DOU,DIN,DIN,DIN,DNO,DUN,DNO,DNO,DNO,DUN,DNO,DOU,DIN,DUN,DUN,
    DUN,DUN,DUN,DUN,DUN,DOU,DUN,DUN,DUN,DUN,DIN,DUN,DUN,DUN,DUN,DUN,
    DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,
    DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,
    DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DIN,DUN,DOU,DUN,DUN,DUN,DUN,DUN,
    DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DIN,DUN,
    DUN,DUN,DUN,DUN,DUN,DNO,DNO,DUN,DIN,DNO,DOU,DUN,DNO,DUN,DOU,DOU,
    DOU,DOU,DOU,DNO,DUN,DIN,DOU,DIN,DIN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,
    DUN,DUN,DOU,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,
    DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,
    DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DOU,DUN,DUN,DUN,DUN,DUN,
    DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN
};

/* LILO and modprobe/insmod parameters */
/* IRQ list for GDT3000/3020 EISA controllers */
static int irq[MAXHA] __initdata = 
{0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
/* disable driver flag */
static int disable __initdata = 0;
/* reserve flag */
static int reserve_mode = 1;                  
/* reserve list */
static int reserve_list[MAX_RES_ARGS] = 
{0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
/* scan order for PCI controllers */
static int reverse_scan = 0;
/* virtual channel for the host drives */
static int hdr_channel = 0;
/* max. IDs per channel */
static int max_ids = MAXID;
/* rescan all IDs */
static int rescan = 0;
/* shared access */
static int shared_access = 1;
/* enable support for EISA and ISA controllers */
static int probe_eisa_isa = 0;
/* 64 bit DMA mode, support for drives > 2 TB, if force_dma32 = 0 */
static int force_dma32 = 0;

/* parameters for modprobe/insmod */
module_param_array(irq, int, NULL, 0);
module_param(disable, int, 0);
module_param(reserve_mode, int, 0);
module_param_array(reserve_list, int, NULL, 0);
module_param(reverse_scan, int, 0);
module_param(hdr_channel, int, 0);
module_param(max_ids, int, 0);
module_param(rescan, int, 0);
module_param(shared_access, int, 0);
module_param(probe_eisa_isa, int, 0);
module_param(force_dma32, int, 0);
MODULE_AUTHOR("Achim Leubner");
MODULE_LICENSE("GPL");

/* ioctl interface */
static const struct file_operations gdth_fops = {
    .ioctl   = gdth_ioctl,
    .open    = gdth_open,
    .release = gdth_close,
};

#include "gdth_proc.h"
#include "gdth_proc.c"

static gdth_ha_str *gdth_find_ha(int hanum)
{
	gdth_ha_str *ha;

	list_for_each_entry(ha, &gdth_instances, list)
		if (hanum == ha->hanum)
			return ha;

	return NULL;
}

static struct gdth_cmndinfo *gdth_get_cmndinfo(gdth_ha_str *ha)
{
	struct gdth_cmndinfo *priv = NULL;
	ulong flags;
	int i;

	spin_lock_irqsave(&ha->smp_lock, flags);

	for (i=0; i<GDTH_MAXCMDS; ++i) {
		if (ha->cmndinfo[i].index == 0) {
			priv = &ha->cmndinfo[i];
			memset(priv, 0, sizeof(*priv));
			priv->index = i+1;
			break;
		}
	}

	spin_unlock_irqrestore(&ha->smp_lock, flags);

	return priv;
}

static void gdth_put_cmndinfo(struct gdth_cmndinfo *priv)
{
	BUG_ON(!priv);
	priv->index = 0;
}

static void gdth_delay(int milliseconds)
{
    if (milliseconds == 0) {
        udelay(1);
    } else {
        mdelay(milliseconds);
    }
}

static void gdth_scsi_done(struct scsi_cmnd *scp)
{
	struct gdth_cmndinfo *cmndinfo = gdth_cmnd_priv(scp);
	int internal_command = cmndinfo->internal_command;

	TRACE2(("gdth_scsi_done()\n"));

	gdth_put_cmndinfo(cmndinfo);
	scp->host_scribble = NULL;

	if (internal_command)
		complete((struct completion *)scp->request);
	else
		scp->scsi_done(scp);
}

int __gdth_execute(struct scsi_device *sdev, gdth_cmd_str *gdtcmd, char *cmnd,
                   int timeout, u32 *info)
{
    gdth_ha_str *ha = shost_priv(sdev->host);
    Scsi_Cmnd *scp;
    struct gdth_cmndinfo cmndinfo;
    DECLARE_COMPLETION_ONSTACK(wait);
    int rval;

    scp = kzalloc(sizeof(*scp), GFP_KERNEL);
    if (!scp)
        return -ENOMEM;

    scp->sense_buffer = kzalloc(SCSI_SENSE_BUFFERSIZE, GFP_KERNEL);
    if (!scp->sense_buffer) {
	kfree(scp);
	return -ENOMEM;
    }

    scp->device = sdev;
    memset(&cmndinfo, 0, sizeof(cmndinfo));

    /* use request field to save the ptr. to completion struct. */
    scp->request = (struct request *)&wait;
    scp->cmd_len = 12;
    scp->cmnd = cmnd;
    cmndinfo.priority = IOCTL_PRI;
    cmndinfo.internal_cmd_str = gdtcmd;
    cmndinfo.internal_command = 1;

    TRACE(("__gdth_execute() cmd 0x%x\n", scp->cmnd[0]));
    __gdth_queuecommand(ha, scp, &cmndinfo);

    wait_for_completion(&wait);

    rval = cmndinfo.status;
    if (info)
        *info = cmndinfo.info;
    kfree(scp->sense_buffer);
    kfree(scp);
    return rval;
}

int gdth_execute(struct Scsi_Host *shost, gdth_cmd_str *gdtcmd, char *cmnd,
                 int timeout, u32 *info)
{
    struct scsi_device *sdev = scsi_get_host_dev(shost);
    int rval = __gdth_execute(sdev, gdtcmd, cmnd, timeout, info);

    scsi_free_host_dev(sdev);
    return rval;
}

static void gdth_eval_mapping(ulong32 size, ulong32 *cyls, int *heads, int *secs)
{
    *cyls = size /HEADS/SECS;
    if (*cyls <= MAXCYLS) {
        *heads = HEADS;
        *secs = SECS;
    } else {                                        /* too high for 64*32 */
        *cyls = size /MEDHEADS/MEDSECS;
        if (*cyls <= MAXCYLS) {
            *heads = MEDHEADS;
            *secs = MEDSECS;
        } else {                                    /* too high for 127*63 */
            *cyls = size /BIGHEADS/BIGSECS;
            *heads = BIGHEADS;
            *secs = BIGSECS;
        }
    }
}

/* controller search and initialization functions */
#ifdef CONFIG_EISA
static int __init gdth_search_eisa(ushort eisa_adr)
{
    ulong32 id;
    
    TRACE(("gdth_search_eisa() adr. %x\n",eisa_adr));
    id = inl(eisa_adr+ID0REG);
    if (id == GDT3A_ID || id == GDT3B_ID) {     /* GDT3000A or GDT3000B */
        if ((inb(eisa_adr+EISAREG) & 8) == 0)   
            return 0;                           /* not EISA configured */
        return 1;
    }
    if (id == GDT3_ID)                          /* GDT3000 */
        return 1;

    return 0;                                   
}
#endif /* CONFIG_EISA */

#ifdef CONFIG_ISA
static int __init gdth_search_isa(ulong32 bios_adr)
{
    void __iomem *addr;
    ulong32 id;

    TRACE(("gdth_search_isa() bios adr. %x\n",bios_adr));
    if ((addr = ioremap(bios_adr+BIOS_ID_OFFS, sizeof(ulong32))) != NULL) {
        id = readl(addr);
        iounmap(addr);
        if (id == GDT2_ID)                          /* GDT2000 */
            return 1;
    }
    return 0;
}
#endif /* CONFIG_ISA */

#ifdef CONFIG_PCI

static bool gdth_search_vortex(ushort device)
{
	if (device <= PCI_DEVICE_ID_VORTEX_GDT6555)
		return true;
	if (device >= PCI_DEVICE_ID_VORTEX_GDT6x17RP &&
	    device <= PCI_DEVICE_ID_VORTEX_GDTMAXRP)
		return true;
	if (device == PCI_DEVICE_ID_VORTEX_GDTNEWRX ||
	    device == PCI_DEVICE_ID_VORTEX_GDTNEWRX2)
		return true;
	return false;
}

static int gdth_pci_probe_one(gdth_pci_str *pcistr, gdth_ha_str **ha_out);
static int gdth_pci_init_one(struct pci_dev *pdev,
			     const struct pci_device_id *ent);
static void gdth_pci_remove_one(struct pci_dev *pdev);
static void gdth_remove_one(gdth_ha_str *ha);

/* Vortex only makes RAID controllers.
 * We do not really want to specify all 550 ids here, so wildcard match.
 */
static const struct pci_device_id gdthtable[] = {
	{ PCI_VDEVICE(VORTEX, PCI_ANY_ID) },
	{ PCI_VDEVICE(INTEL, PCI_DEVICE_ID_INTEL_SRC) },
	{ PCI_VDEVICE(INTEL, PCI_DEVICE_ID_INTEL_SRC_XSCALE) },
	{ }	/* terminate list */
};
MODULE_DEVICE_TABLE(pci, gdthtable);

static struct pci_driver gdth_pci_driver = {
	.name		= "gdth",
	.id_table	= gdthtable,
	.probe		= gdth_pci_init_one,
	.remove		= gdth_pci_remove_one,
};

static void __devexit gdth_pci_remove_one(struct pci_dev *pdev)
{
	gdth_ha_str *ha = pci_get_drvdata(pdev);

	pci_set_drvdata(pdev, NULL);

	list_del(&ha->list);
	gdth_remove_one(ha);

	pci_disable_device(pdev);
}

static int __devinit gdth_pci_init_one(struct pci_dev *pdev,
				       const struct pci_device_id *ent)
{
	ushort vendor = pdev->vendor;
	ushort device = pdev->device;
	ulong base0, base1, base2;
	int rc;
	gdth_pci_str gdth_pcistr;
	gdth_ha_str *ha = NULL;
    
	TRACE(("gdth_search_dev() cnt %d vendor %x device %x\n",
	       gdth_ctr_count, vendor, device));

	memset(&gdth_pcistr, 0, sizeof(gdth_pcistr));

	if (vendor == PCI_VENDOR_ID_VORTEX && !gdth_search_vortex(device))
		return -ENODEV;

	rc = pci_enable_device(pdev);
	if (rc)
		return rc;

	if (gdth_ctr_count >= MAXHA)
		return -EBUSY;

        /* GDT PCI controller found, resources are already in pdev */
	gdth_pcistr.pdev = pdev;
        base0 = pci_resource_flags(pdev, 0);
        base1 = pci_resource_flags(pdev, 1);
        base2 = pci_resource_flags(pdev, 2);
        if (device <= PCI_DEVICE_ID_VORTEX_GDT6000B ||   /* GDT6000/B */
            device >= PCI_DEVICE_ID_VORTEX_GDT6x17RP) {  /* MPR */
            if (!(base0 & IORESOURCE_MEM)) 
		return -ENODEV;
	    gdth_pcistr.dpmem = pci_resource_start(pdev, 0);
        } else {                                  /* GDT6110, GDT6120, .. */
            if (!(base0 & IORESOURCE_MEM) ||
                !(base2 & IORESOURCE_MEM) ||
                !(base1 & IORESOURCE_IO)) 
		return -ENODEV;
	    gdth_pcistr.dpmem = pci_resource_start(pdev, 2);
	    gdth_pcistr.io    = pci_resource_start(pdev, 1);
        }
        TRACE2(("Controller found at %d/%d, irq %d, dpmem 0x%lx\n",
		gdth_pcistr.pdev->bus->number,
		PCI_SLOT(gdth_pcistr.pdev->devfn),
		gdth_pcistr.irq,
		gdth_pcistr.dpmem));

	rc = gdth_pci_probe_one(&gdth_pcistr, &ha);
	if (rc)
		return rc;

	return 0;
}
#endif /* CONFIG_PCI */

#ifdef CONFIG_EISA
static int __init gdth_init_eisa(ushort eisa_adr,gdth_ha_str *ha)
{
    ulong32 retries,id;
    unchar prot_ver,eisacf,i,irq_found;

    TRACE(("gdth_init_eisa() adr. %x\n",eisa_adr));
    
    /* disable board interrupts, deinitialize services */
    outb(0xff,eisa_adr+EDOORREG);
    outb(0x00,eisa_adr+EDENABREG);
    outb(0x00,eisa_adr+EINTENABREG);
    
    outb(0xff,eisa_adr+LDOORREG);
    retries = INIT_RETRIES;
    gdth_delay(20);
    while (inb(eisa_adr+EDOORREG) != 0xff) {
        if (--retries == 0) {
            printk("GDT-EISA: Initialization error (DEINIT failed)\n");
            return 0;
        }
        gdth_delay(1);
        TRACE2(("wait for DEINIT: retries=%d\n",retries));
    }
    prot_ver = inb(eisa_adr+MAILBOXREG);
    outb(0xff,eisa_adr+EDOORREG);
    if (prot_ver != PROTOCOL_VERSION) {
        printk("GDT-EISA: Illegal protocol version\n");
        return 0;
    }
    ha->bmic = eisa_adr;
    ha->brd_phys = (ulong32)eisa_adr >> 12;

    outl(0,eisa_adr+MAILBOXREG);
    outl(0,eisa_adr+MAILBOXREG+4);
    outl(0,eisa_adr+MAILBOXREG+8);
    outl(0,eisa_adr+MAILBOXREG+12);

    /* detect IRQ */ 
    if ((id = inl(eisa_adr+ID0REG)) == GDT3_ID) {
        ha->oem_id = OEM_ID_ICP;
        ha->type = GDT_EISA;
        ha->stype = id;
        outl(1,eisa_adr+MAILBOXREG+8);
        outb(0xfe,eisa_adr+LDOORREG);
        retries = INIT_RETRIES;
        gdth_delay(20);
        while (inb(eisa_adr+EDOORREG) != 0xfe) {
            if (--retries == 0) {
                printk("GDT-EISA: Initialization error (get IRQ failed)\n");
                return 0;
            }
            gdth_delay(1);
        }
        ha->irq = inb(eisa_adr+MAILBOXREG);
        outb(0xff,eisa_adr+EDOORREG);
        TRACE2(("GDT3000/3020: IRQ=%d\n",ha->irq));
        /* check the result */
        if (ha->irq == 0) {
                TRACE2(("Unknown IRQ, use IRQ table from cmd line !\n"));
                for (i = 0, irq_found = FALSE; 
                     i < MAXHA && irq[i] != 0xff; ++i) {
                if (irq[i]==10 || irq[i]==11 || irq[i]==12 || irq[i]==14) {
                    irq_found = TRUE;
                    break;
                }
                }
            if (irq_found) {
                ha->irq = irq[i];
                irq[i] = 0;
                printk("GDT-EISA: Can not detect controller IRQ,\n");
                printk("Use IRQ setting from command line (IRQ = %d)\n",
                       ha->irq);
            } else {
                printk("GDT-EISA: Initialization error (unknown IRQ), Enable\n");
                printk("the controller BIOS or use command line parameters\n");
                return 0;
            }
        }
    } else {
        eisacf = inb(eisa_adr+EISAREG) & 7;
        if (eisacf > 4)                         /* level triggered */
            eisacf -= 4;
        ha->irq = gdth_irq_tab[eisacf];
        ha->oem_id = OEM_ID_ICP;
        ha->type = GDT_EISA;
        ha->stype = id;
    }

    ha->dma64_support = 0;
    return 1;
}
#endif /* CONFIG_EISA */

#ifdef CONFIG_ISA
static int __init gdth_init_isa(ulong32 bios_adr,gdth_ha_str *ha)
{
    register gdt2_dpram_str __iomem *dp2_ptr;
    int i;
    unchar irq_drq,prot_ver;
    ulong32 retries;

    TRACE(("gdth_init_isa() bios adr. %x\n",bios_adr));

    ha->brd = ioremap(bios_adr, sizeof(gdt2_dpram_str));
    if (ha->brd == NULL) {
        printk("GDT-ISA: Initialization error (DPMEM remap error)\n");
        return 0;
    }
    dp2_ptr = ha->brd;
    writeb(1, &dp2_ptr->io.memlock); /* switch off write protection */
    /* reset interface area */
    memset_io(&dp2_ptr->u, 0, sizeof(dp2_ptr->u));
    if (readl(&dp2_ptr->u) != 0) {
        printk("GDT-ISA: Initialization error (DPMEM write error)\n");
        iounmap(ha->brd);
        return 0;
    }

    /* disable board interrupts, read DRQ and IRQ */
    writeb(0xff, &dp2_ptr->io.irqdel);
    writeb(0x00, &dp2_ptr->io.irqen);
    writeb(0x00, &dp2_ptr->u.ic.S_Status);
    writeb(0x00, &dp2_ptr->u.ic.Cmd_Index);

    irq_drq = readb(&dp2_ptr->io.rq);
    for (i=0; i<3; ++i) {
        if ((irq_drq & 1)==0)
            break;
        irq_drq >>= 1;
    }
    ha->drq = gdth_drq_tab[i];

    irq_drq = readb(&dp2_ptr->io.rq) >> 3;
    for (i=1; i<5; ++i) {
        if ((irq_drq & 1)==0)
            break;
        irq_drq >>= 1;
    }
    ha->irq = gdth_irq_tab[i];

    /* deinitialize services */
    writel(bios_adr, &dp2_ptr->u.ic.S_Info[0]);
    writeb(0xff, &dp2_ptr->u.ic.S_Cmd_Indx);
    writeb(0, &dp2_ptr->io.event);
    retries = INIT_RETRIES;
    gdth_delay(20);
    while (readb(&dp2_ptr->u.ic.S_Status) != 0xff) {
        if (--retries == 0) {
            printk("GDT-ISA: Initialization error (DEINIT failed)\n");
            iounmap(ha->brd);
            return 0;
        }
        gdth_delay(1);
    }
    prot_ver = (unchar)readl(&dp2_ptr->u.ic.S_Info[0]);
    writeb(0, &dp2_ptr->u.ic.Status);
    writeb(0xff, &dp2_ptr->io.irqdel);
    if (prot_ver != PROTOCOL_VERSION) {
        printk("GDT-ISA: Illegal protocol version\n");
        iounmap(ha->brd);
        return 0;
    }

    ha->oem_id = OEM_ID_ICP;
    ha->type = GDT_ISA;
    ha->ic_all_size = sizeof(dp2_ptr->u);
    ha->stype= GDT2_ID;
    ha->brd_phys = bios_adr >> 4;

    /* special request to controller BIOS */
    writel(0x00, &dp2_ptr->u.ic.S_Info[0]);
    writel(0x00, &dp2_ptr->u.ic.S_Info[1]);
    writel(0x01, &dp2_ptr->u.ic.S_Info[2]);
    writel(0x00, &dp2_ptr->u.ic.S_Info[3]);
    writeb(0xfe, &dp2_ptr->u.ic.S_Cmd_Indx);
    writeb(0, &dp2_ptr->io.event);
    retries = INIT_RETRIES;
    gdth_delay(20);
    while (readb(&dp2_ptr->u.ic.S_Status) != 0xfe) {
        if (--retries == 0) {
            printk("GDT-ISA: Initialization error\n");
            iounmap(ha->brd);
            return 0;
        }
        gdth_delay(1);
    }
    writeb(0, &dp2_ptr->u.ic.Status);
    writeb(0xff, &dp2_ptr->io.irqdel);

    ha->dma64_support = 0;
    return 1;
}
#endif /* CONFIG_ISA */

#ifdef CONFIG_PCI
static int __devinit gdth_init_pci(struct pci_dev *pdev, gdth_pci_str *pcistr,
				   gdth_ha_str *ha)
{
    register gdt6_dpram_str __iomem *dp6_ptr;
    register gdt6c_dpram_str __iomem *dp6c_ptr;
    register gdt6m_dpram_str __iomem *dp6m_ptr;
    ulong32 retries;
    unchar prot_ver;
    ushort command;
    int i, found = FALSE;

    TRACE(("gdth_init_pci()\n"));

    if (pdev->vendor == PCI_VENDOR_ID_INTEL)
        ha->oem_id = OEM_ID_INTEL;
    else
        ha->oem_id = OEM_ID_ICP;
    ha->brd_phys = (pdev->bus->number << 8) | (pdev->devfn & 0xf8);
    ha->stype = (ulong32)pdev->device;
    ha->irq = pdev->irq;
    ha->pdev = pdev;
    
    if (ha->pdev->device <= PCI_DEVICE_ID_VORTEX_GDT6000B) {  /* GDT6000/B */
        TRACE2(("init_pci() dpmem %lx irq %d\n",pcistr->dpmem,ha->irq));
        ha->brd = ioremap(pcistr->dpmem, sizeof(gdt6_dpram_str));
        if (ha->brd == NULL) {
            printk("GDT-PCI: Initialization error (DPMEM remap error)\n");
            return 0;
        }
        /* check and reset interface area */
        dp6_ptr = ha->brd;
        writel(DPMEM_MAGIC, &dp6_ptr->u);
        if (readl(&dp6_ptr->u) != DPMEM_MAGIC) {
            printk("GDT-PCI: Cannot access DPMEM at 0x%lx (shadowed?)\n", 
                   pcistr->dpmem);
            found = FALSE;
            for (i = 0xC8000; i < 0xE8000; i += 0x4000) {
                iounmap(ha->brd);
                ha->brd = ioremap(i, sizeof(ushort)); 
                if (ha->brd == NULL) {
                    printk("GDT-PCI: Initialization error (DPMEM remap error)\n");
                    return 0;
                }
                if (readw(ha->brd) != 0xffff) {
                    TRACE2(("init_pci_old() address 0x%x busy\n", i));
                    continue;
                }
                iounmap(ha->brd);
		pci_write_config_dword(pdev, PCI_BASE_ADDRESS_0, i);
                ha->brd = ioremap(i, sizeof(gdt6_dpram_str)); 
                if (ha->brd == NULL) {
                    printk("GDT-PCI: Initialization error (DPMEM remap error)\n");
                    return 0;
                }
                dp6_ptr = ha->brd;
                writel(DPMEM_MAGIC, &dp6_ptr->u);
                if (readl(&dp6_ptr->u) == DPMEM_MAGIC) {
                    printk("GDT-PCI: Use free address at 0x%x\n", i);
                    found = TRUE;
                    break;
                }
            }   
            if (!found) {
                printk("GDT-PCI: No free address found!\n");
                iounmap(ha->brd);
                return 0;
            }
        }
        memset_io(&dp6_ptr->u, 0, sizeof(dp6_ptr->u));
        if (readl(&dp6_ptr->u) != 0) {
            printk("GDT-PCI: Initialization error (DPMEM write error)\n");
            iounmap(ha->brd);
            return 0;
        }
        
        /* disable board interrupts, deinit services */
        writeb(0xff, &dp6_ptr->io.irqdel);
        writeb(0x00, &dp6_ptr->io.irqen);
        writeb(0x00, &dp6_ptr->u.ic.S_Status);
        writeb(0x00, &dp6_ptr->u.ic.Cmd_Index);

        writel(pcistr->dpmem, &dp6_ptr->u.ic.S_Info[0]);
        writeb(0xff, &dp6_ptr->u.ic.S_Cmd_Indx);
        writeb(0, &dp6_ptr->io.event);
        retries = INIT_RETRIES;
        gdth_delay(20);
        while (readb(&dp6_ptr->u.ic.S_Status) != 0xff) {
            if (--retries == 0) {
                printk("GDT-PCI: Initialization error (DEINIT failed)\n");
                iounmap(ha->brd);
                return 0;
            }
            gdth_delay(1);
        }
        prot_ver = (unchar)readl(&dp6_ptr->u.ic.S_Info[0]);
        writeb(0, &dp6_ptr->u.ic.S_Status);
        writeb(0xff, &dp6_ptr->io.irqdel);
        if (prot_ver != PROTOCOL_VERSION) {
            printk("GDT-PCI: Illegal protocol version\n");
            iounmap(ha->brd);
            return 0;
        }

        ha->type = GDT_PCI;
        ha->ic_all_size = sizeof(dp6_ptr->u);
        
        /* special command to controller BIOS */
        writel(0x00, &dp6_ptr->u.ic.S_Info[0]);
        writel(0x00, &dp6_ptr->u.ic.S_Info[1]);
        writel(0x00, &dp6_ptr->u.ic.S_Info[2]);
        writel(0x00, &dp6_ptr->u.ic.S_Info[3]);
        writeb(0xfe, &dp6_ptr->u.ic.S_Cmd_Indx);
        writeb(0, &dp6_ptr->io.event);
        retries = INIT_RETRIES;
        gdth_delay(20);
        while (readb(&dp6_ptr->u.ic.S_Status) != 0xfe) {
            if (--retries == 0) {
                printk("GDT-PCI: Initialization error\n");
                iounmap(ha->brd);
                return 0;
            }
            gdth_delay(1);
        }
        writeb(0, &dp6_ptr->u.ic.S_Status);
        writeb(0xff, &dp6_ptr->io.irqdel);

        ha->dma64_support = 0;

    } else if (ha->pdev->device <= PCI_DEVICE_ID_VORTEX_GDT6555) { /* GDT6110, ... */
        ha->plx = (gdt6c_plx_regs *)pcistr->io;
        TRACE2(("init_pci_new() dpmem %lx irq %d\n",
            pcistr->dpmem,ha->irq));
        ha->brd = ioremap(pcistr->dpmem, sizeof(gdt6c_dpram_str));
        if (ha->brd == NULL) {
            printk("GDT-PCI: Initialization error (DPMEM remap error)\n");
            iounmap(ha->brd);
            return 0;
        }
        /* check and reset interface area */
        dp6c_ptr = ha->brd;
        writel(DPMEM_MAGIC, &dp6c_ptr->u);
        if (readl(&dp6c_ptr->u) != DPMEM_MAGIC) {
            printk("GDT-PCI: Cannot access DPMEM at 0x%lx (shadowed?)\n", 
                   pcistr->dpmem);
            found = FALSE;
            for (i = 0xC8000; i < 0xE8000; i += 0x4000) {
                iounmap(ha->brd);
                ha->brd = ioremap(i, sizeof(ushort)); 
                if (ha->brd == NULL) {
                    printk("GDT-PCI: Initialization error (DPMEM remap error)\n");
                    return 0;
                }
                if (readw(ha->brd) != 0xffff) {
                    TRACE2(("init_pci_plx() address 0x%x busy\n", i));
                    continue;
                }
                iounmap(ha->brd);
		pci_write_config_dword(pdev, PCI_BASE_ADDRESS_2, i);
                ha->brd = ioremap(i, sizeof(gdt6c_dpram_str)); 
                if (ha->brd == NULL) {
                    printk("GDT-PCI: Initialization error (DPMEM remap error)\n");
                    return 0;
                }
                dp6c_ptr = ha->brd;
                writel(DPMEM_MAGIC, &dp6c_ptr->u);
                if (readl(&dp6c_ptr->u) == DPMEM_MAGIC) {
                    printk("GDT-PCI: Use free address at 0x%x\n", i);
                    found = TRUE;
                    break;
                }
            }   
            if (!found) {
                printk("GDT-PCI: No free address found!\n");
                iounmap(ha->brd);
                return 0;
            }
        }
        memset_io(&dp6c_ptr->u, 0, sizeof(dp6c_ptr->u));
        if (readl(&dp6c_ptr->u) != 0) {
            printk("GDT-PCI: Initialization error (DPMEM write error)\n");
            iounmap(ha->brd);
            return 0;
        }
        
        /* disable board interrupts, deinit services */
        outb(0x00,PTR2USHORT(&ha->plx->control1));
        outb(0xff,PTR2USHORT(&ha->plx->edoor_reg));
        
        writeb(0x00, &dp6c_ptr->u.ic.S_Status);
        writeb(0x00, &dp6c_ptr->u.ic.Cmd_Index);

        writel(pcistr->dpmem, &dp6c_ptr->u.ic.S_Info[0]);
        writeb(0xff, &dp6c_ptr->u.ic.S_Cmd_Indx);

        outb(1,PTR2USHORT(&ha->plx->ldoor_reg));

        retries = INIT_RETRIES;
        gdth_delay(20);
        while (readb(&dp6c_ptr->u.ic.S_Status) != 0xff) {
            if (--retries == 0) {
                printk("GDT-PCI: Initialization error (DEINIT failed)\n");
                iounmap(ha->brd);
                return 0;
            }
            gdth_delay(1);
        }
        prot_ver = (unchar)readl(&dp6c_ptr->u.ic.S_Info[0]);
        writeb(0, &dp6c_ptr->u.ic.Status);
        if (prot_ver != PROTOCOL_VERSION) {
            printk("GDT-PCI: Illegal protocol version\n");
            iounmap(ha->brd);
            return 0;
        }

        ha->type = GDT_PCINEW;
        ha->ic_all_size = sizeof(dp6c_ptr->u);

        /* special command to controller BIOS */
        writel(0x00, &dp6c_ptr->u.ic.S_Info[0]);
        writel(0x00, &dp6c_ptr->u.ic.S_Info[1]);
        writel(0x00, &dp6c_ptr->u.ic.S_Info[2]);
        writel(0x00, &dp6c_ptr->u.ic.S_Info[3]);
        writeb(0xfe, &dp6c_ptr->u.ic.S_Cmd_Indx);
        
        outb(1,PTR2USHORT(&ha->plx->ldoor_reg));

        retries = INIT_RETRIES;
        gdth_delay(20);
        while (readb(&dp6c_ptr->u.ic.S_Status) != 0xfe) {
            if (--retries == 0) {
                printk("GDT-PCI: Initialization error\n");
                iounmap(ha->brd);
                return 0;
            }
            gdth_delay(1);
        }
        writeb(0, &dp6c_ptr->u.ic.S_Status);

        ha->dma64_support = 0;

    } else {                                            /* MPR */
        TRACE2(("init_pci_mpr() dpmem %lx irq %d\n",pcistr->dpmem,ha->irq));
        ha->brd = ioremap(pcistr->dpmem, sizeof(gdt6m_dpram_str));
        if (ha->brd == NULL) {
            printk("GDT-PCI: Initialization error (DPMEM remap error)\n");
            return 0;
        }

        /* manipulate config. space to enable DPMEM, start RP controller */
	pci_read_config_word(pdev, PCI_COMMAND, &command);
        command |= 6;
	pci_write_config_word(pdev, PCI_COMMAND, command);
	if (pci_resource_start(pdev, 8) == 1UL)
	    pci_resource_start(pdev, 8) = 0UL;
        i = 0xFEFF0001UL;
	pci_write_config_dword(pdev, PCI_ROM_ADDRESS, i);
        gdth_delay(1);
	pci_write_config_dword(pdev, PCI_ROM_ADDRESS,
			       pci_resource_start(pdev, 8));
        
        dp6m_ptr = ha->brd;

        /* Ensure that it is safe to access the non HW portions of DPMEM.
         * Aditional check needed for Xscale based RAID controllers */
        while( ((int)readb(&dp6m_ptr->i960r.sema0_reg) ) & 3 )
            gdth_delay(1);
        
        /* check and reset interface area */
        writel(DPMEM_MAGIC, &dp6m_ptr->u);
        if (readl(&dp6m_ptr->u) != DPMEM_MAGIC) {
            printk("GDT-PCI: Cannot access DPMEM at 0x%lx (shadowed?)\n", 
                   pcistr->dpmem);
            found = FALSE;
            for (i = 0xC8000; i < 0xE8000; i += 0x4000) {
                iounmap(ha->brd);
                ha->brd = ioremap(i, sizeof(ushort)); 
                if (ha->brd == NULL) {
                    printk("GDT-PCI: Initialization error (DPMEM remap error)\n");
                    return 0;
                }
                if (readw(ha->brd) != 0xffff) {
                    TRACE2(("init_pci_mpr() address 0x%x busy\n", i));
                    continue;
                }
                iounmap(ha->brd);
		pci_write_config_dword(pdev, PCI_BASE_ADDRESS_0, i);
                ha->brd = ioremap(i, sizeof(gdt6m_dpram_str)); 
                if (ha->brd == NULL) {
                    printk("GDT-PCI: Initialization error (DPMEM remap error)\n");
                    return 0;
                }
                dp6m_ptr = ha->brd;
                writel(DPMEM_MAGIC, &dp6m_ptr->u);
                if (readl(&dp6m_ptr->u) == DPMEM_MAGIC) {
                    printk("GDT-PCI: Use free address at 0x%x\n", i);
                    found = TRUE;
                    break;
                }
            }   
            if (!found) {
                printk("GDT-PCI: No free address found!\n");
                iounmap(ha->brd);
                return 0;
            }
        }
        memset_io(&dp6m_ptr->u, 0, sizeof(dp6m_ptr->u));
        
        /* disable board interrupts, deinit services */
        writeb(readb(&dp6m_ptr->i960r.edoor_en_reg) | 4,
                    &dp6m_ptr->i960r.edoor_en_reg);
        writeb(0xff, &dp6m_ptr->i960r.edoor_reg);
        writeb(0x00, &dp6m_ptr->u.ic.S_Status);
        writeb(0x00, &dp6m_ptr->u.ic.Cmd_Index);

        writel(pcistr->dpmem, &dp6m_ptr->u.ic.S_Info[0]);
        writeb(0xff, &dp6m_ptr->u.ic.S_Cmd_Indx);
        writeb(1, &dp6m_ptr->i960r.ldoor_reg);
        retries = INIT_RETRIES;
        gdth_delay(20);
        while (readb(&dp6m_ptr->u.ic.S_Status) != 0xff) {
            if (--retries == 0) {
                printk("GDT-PCI: Initialization error (DEINIT failed)\n");
                iounmap(ha->brd);
                return 0;
            }
            gdth_delay(1);
        }
        prot_ver = (unchar)readl(&dp6m_ptr->u.ic.S_Info[0]);
        writeb(0, &dp6m_ptr->u.ic.S_Status);
        if (prot_ver != PROTOCOL_VERSION) {
            printk("GDT-PCI: Illegal protocol version\n");
            iounmap(ha->brd);
            return 0;
        }

        ha->type = GDT_PCIMPR;
        ha->ic_all_size = sizeof(dp6m_ptr->u);
        
        /* special command to controller BIOS */
        writel(0x00, &dp6m_ptr->u.ic.S_Info[0]);
        writel(0x00, &dp6m_ptr->u.ic.S_Info[1]);
        writel(0x00, &dp6m_ptr->u.ic.S_Info[2]);
        writel(0x00, &dp6m_ptr->u.ic.S_Info[3]);
        writeb(0xfe, &dp6m_ptr->u.ic.S_Cmd_Indx);
        writeb(1, &dp6m_ptr->i960r.ldoor_reg);
        retries = INIT_RETRIES;
        gdth_delay(20);
        while (readb(&dp6m_ptr->u.ic.S_Status) != 0xfe) {
            if (--retries == 0) {
                printk("GDT-PCI: Initialization error\n");
                iounmap(ha->brd);
                return 0;
            }
            gdth_delay(1);
        }
        writeb(0, &dp6m_ptr->u.ic.S_Status);

        /* read FW version to detect 64-bit DMA support */
        writeb(0xfd, &dp6m_ptr->u.ic.S_Cmd_Indx);
        writeb(1, &dp6m_ptr->i960r.ldoor_reg);
        retries = INIT_RETRIES;
        gdth_delay(20);
        while (readb(&dp6m_ptr->u.ic.S_Status) != 0xfd) {
            if (--retries == 0) {
                printk("GDT-PCI: Initialization error (DEINIT failed)\n");
                iounmap(ha->brd);
                return 0;
            }
            gdth_delay(1);
        }
        prot_ver = (unchar)(readl(&dp6m_ptr->u.ic.S_Info[0]) >> 16);
        writeb(0, &dp6m_ptr->u.ic.S_Status);
        if (prot_ver < 0x2b)      /* FW < x.43: no 64-bit DMA support */
            ha->dma64_support = 0;
        else 
            ha->dma64_support = 1;
    }

    return 1;
}
#endif /* CONFIG_PCI */

/* controller protocol functions */

static void __devinit gdth_enable_int(gdth_ha_str *ha)
{
    ulong flags;
    gdt2_dpram_str __iomem *dp2_ptr;
    gdt6_dpram_str __iomem *dp6_ptr;
    gdt6m_dpram_str __iomem *dp6m_ptr;

    TRACE(("gdth_enable_int() hanum %d\n",ha->hanum));
    spin_lock_irqsave(&ha->smp_lock, flags);

    if (ha->type == GDT_EISA) {
        outb(0xff, ha->bmic + EDOORREG);
        outb(0xff, ha->bmic + EDENABREG);
        outb(0x01, ha->bmic + EINTENABREG);
    } else if (ha->type == GDT_ISA) {
        dp2_ptr = ha->brd;
        writeb(1, &dp2_ptr->io.irqdel);
        writeb(0, &dp2_ptr->u.ic.Cmd_Index);
        writeb(1, &dp2_ptr->io.irqen);
    } else if (ha->type == GDT_PCI) {
        dp6_ptr = ha->brd;
        writeb(1, &dp6_ptr->io.irqdel);
        writeb(0, &dp6_ptr->u.ic.Cmd_Index);
        writeb(1, &dp6_ptr->io.irqen);
    } else if (ha->type == GDT_PCINEW) {
        outb(0xff, PTR2USHORT(&ha->plx->edoor_reg));
        outb(0x03, PTR2USHORT(&ha->plx->control1));
    } else if (ha->type == GDT_PCIMPR) {
        dp6m_ptr = ha->brd;
        writeb(0xff, &dp6m_ptr->i960r.edoor_reg);
        writeb(readb(&dp6m_ptr->i960r.edoor_en_reg) & ~4,
                    &dp6m_ptr->i960r.edoor_en_reg);
    }
    spin_unlock_irqrestore(&ha->smp_lock, flags);
}

/* return IStatus if interrupt was from this card else 0 */
static unchar gdth_get_status(gdth_ha_str *ha)
{
    unchar IStatus = 0;

    TRACE(("gdth_get_status() irq %d ctr_count %d\n", ha->irq, gdth_ctr_count));

        if (ha->type == GDT_EISA)
            IStatus = inb((ushort)ha->bmic + EDOORREG);
        else if (ha->type == GDT_ISA)
            IStatus =
                readb(&((gdt2_dpram_str __iomem *)ha->brd)->u.ic.Cmd_Index);
        else if (ha->type == GDT_PCI)
            IStatus =
                readb(&((gdt6_dpram_str __iomem *)ha->brd)->u.ic.Cmd_Index);
        else if (ha->type == GDT_PCINEW) 
            IStatus = inb(PTR2USHORT(&ha->plx->edoor_reg));
        else if (ha->type == GDT_PCIMPR)
            IStatus =
                readb(&((gdt6m_dpram_str __iomem *)ha->brd)->i960r.edoor_reg);

        return IStatus;
}

static int gdth_test_busy(gdth_ha_str *ha)
{
    register int gdtsema0 = 0;

    TRACE(("gdth_test_busy() hanum %d\n", ha->hanum));

    if (ha->type == GDT_EISA)
        gdtsema0 = (int)inb(ha->bmic + SEMA0REG);
    else if (ha->type == GDT_ISA)
        gdtsema0 = (int)readb(&((gdt2_dpram_str __iomem *)ha->brd)->u.ic.Sema0);
    else if (ha->type == GDT_PCI)
        gdtsema0 = (int)readb(&((gdt6_dpram_str __iomem *)ha->brd)->u.ic.Sema0);
    else if (ha->type == GDT_PCINEW) 
        gdtsema0 = (int)inb(PTR2USHORT(&ha->plx->sema0_reg));
    else if (ha->type == GDT_PCIMPR)
        gdtsema0 = 
            (int)readb(&((gdt6m_dpram_str __iomem *)ha->brd)->i960r.sema0_reg);

    return (gdtsema0 & 1);
}


static int gdth_get_cmd_index(gdth_ha_str *ha)
{
    int i;

    TRACE(("gdth_get_cmd_index() hanum %d\n", ha->hanum));

    for (i=0; i<GDTH_MAXCMDS; ++i) {
        if (ha->cmd_tab[i].cmnd == UNUSED_CMND) {
            ha->cmd_tab[i].cmnd = ha->pccb->RequestBuffer;
            ha->cmd_tab[i].service = ha->pccb->Service;
            ha->pccb->CommandIndex = (ulong32)i+2;
            return (i+2);
        }
    }
    return 0;
}


static void gdth_set_sema0(gdth_ha_str *ha)
{
    TRACE(("gdth_set_sema0() hanum %d\n", ha->hanum));

    if (ha->type == GDT_EISA) {
        outb(1, ha->bmic + SEMA0REG);
    } else if (ha->type == GDT_ISA) {
        writeb(1, &((gdt2_dpram_str __iomem *)ha->brd)->u.ic.Sema0);
    } else if (ha->type == GDT_PCI) {
        writeb(1, &((gdt6_dpram_str __iomem *)ha->brd)->u.ic.Sema0);
    } else if (ha->type == GDT_PCINEW) { 
        outb(1, PTR2USHORT(&ha->plx->sema0_reg));
    } else if (ha->type == GDT_PCIMPR) {
        writeb(1, &((gdt6m_dpram_str __iomem *)ha->brd)->i960r.sema0_reg);
    }
}


static void gdth_copy_command(gdth_ha_str *ha)
{
    register gdth_cmd_str *cmd_ptr;
    register gdt6m_dpram_str __iomem *dp6m_ptr;
    register gdt6c_dpram_str __iomem *dp6c_ptr;
    gdt6_dpram_str __iomem *dp6_ptr;
    gdt2_dpram_str __iomem *dp2_ptr;
    ushort cp_count,dp_offset,cmd_no;
    
    TRACE(("gdth_copy_command() hanum %d\n", ha->hanum));

    cp_count = ha->cmd_len;
    dp_offset= ha->cmd_offs_dpmem;
    cmd_no   = ha->cmd_cnt;
    cmd_ptr  = ha->pccb;

    ++ha->cmd_cnt;                                                      
    if (ha->type == GDT_EISA)
        return;                                 /* no DPMEM, no copy */

    /* set cpcount dword aligned */
    if (cp_count & 3)
        cp_count += (4 - (cp_count & 3));

    ha->cmd_offs_dpmem += cp_count;
    
    /* set offset and service, copy command to DPMEM */
    if (ha->type == GDT_ISA) {
        dp2_ptr = ha->brd;
        writew(dp_offset + DPMEM_COMMAND_OFFSET,
                    &dp2_ptr->u.ic.comm_queue[cmd_no].offset);
        writew((ushort)cmd_ptr->Service,
                    &dp2_ptr->u.ic.comm_queue[cmd_no].serv_id);
        memcpy_toio(&dp2_ptr->u.ic.gdt_dpr_cmd[dp_offset],cmd_ptr,cp_count);
    } else if (ha->type == GDT_PCI) {
        dp6_ptr = ha->brd;
        writew(dp_offset + DPMEM_COMMAND_OFFSET,
                    &dp6_ptr->u.ic.comm_queue[cmd_no].offset);
        writew((ushort)cmd_ptr->Service,
                    &dp6_ptr->u.ic.comm_queue[cmd_no].serv_id);
        memcpy_toio(&dp6_ptr->u.ic.gdt_dpr_cmd[dp_offset],cmd_ptr,cp_count);
    } else if (ha->type == GDT_PCINEW) {
        dp6c_ptr = ha->brd;
        writew(dp_offset + DPMEM_COMMAND_OFFSET,
                    &dp6c_ptr->u.ic.comm_queue[cmd_no].offset);
        writew((ushort)cmd_ptr->Service,
                    &dp6c_ptr->u.ic.comm_queue[cmd_no].serv_id);
        memcpy_toio(&dp6c_ptr->u.ic.gdt_dpr_cmd[dp_offset],cmd_ptr,cp_count);
    } else if (ha->type == GDT_PCIMPR) {
        dp6m_ptr = ha->brd;
        writew(dp_offset + DPMEM_COMMAND_OFFSET,
                    &dp6m_ptr->u.ic.comm_queue[cmd_no].offset);
        writew((ushort)cmd_ptr->Service,
                    &dp6m_ptr->u.ic.comm_queue[cmd_no].serv_id);
        memcpy_toio(&dp6m_ptr->u.ic.gdt_dpr_cmd[dp_offset],cmd_ptr,cp_count);
    }
}


static void gdth_release_event(gdth_ha_str *ha)
{
    TRACE(("gdth_release_event() hanum %d\n", ha->hanum));

#ifdef GDTH_STATISTICS
    {
        ulong32 i,j;
        for (i=0,j=0; j<GDTH_MAXCMDS; ++j) {
            if (ha->cmd_tab[j].cmnd != UNUSED_CMND)
                ++i;
        }
        if (max_index < i) {
            max_index = i;
            TRACE3(("GDT: max_index = %d\n",(ushort)i));
        }
    }
#endif

    if (ha->pccb->OpCode == GDT_INIT)
        ha->pccb->Service |= 0x80;

    if (ha->type == GDT_EISA) {
        if (ha->pccb->OpCode == GDT_INIT)               /* store DMA buffer */
            outl(ha->ccb_phys, ha->bmic + MAILBOXREG);
        outb(ha->pccb->Service, ha->bmic + LDOORREG);
    } else if (ha->type == GDT_ISA) {
        writeb(0, &((gdt2_dpram_str __iomem *)ha->brd)->io.event);
    } else if (ha->type == GDT_PCI) {
        writeb(0, &((gdt6_dpram_str __iomem *)ha->brd)->io.event);
    } else if (ha->type == GDT_PCINEW) { 
        outb(1, PTR2USHORT(&ha->plx->ldoor_reg));
    } else if (ha->type == GDT_PCIMPR) {
        writeb(1, &((gdt6m_dpram_str __iomem *)ha->brd)->i960r.ldoor_reg);
    }
}

static int gdth_wait(gdth_ha_str *ha, int index, ulong32 time)
{
    int answer_found = FALSE;
    int wait_index = 0;

    TRACE(("gdth_wait() hanum %d index %d time %d\n", ha->hanum, index, time));

    if (index == 0)
        return 1;                               /* no wait required */

    do {
	__gdth_interrupt(ha, true, &wait_index);
        if (wait_index == index) {
            answer_found = TRUE;
            break;
        }
        gdth_delay(1);
    } while (--time);

    while (gdth_test_busy(ha))
        gdth_delay(0);

    return (answer_found);
}


static int gdth_internal_cmd(gdth_ha_str *ha, unchar service, ushort opcode,
                                            ulong32 p1, ulong64 p2, ulong64 p3)
{
    register gdth_cmd_str *cmd_ptr;
    int retries,index;

    TRACE2(("gdth_internal_cmd() service %d opcode %d\n",service,opcode));

    cmd_ptr = ha->pccb;
    memset((char*)cmd_ptr,0,sizeof(gdth_cmd_str));

    /* make command  */
    for (retries = INIT_RETRIES;;) {
        cmd_ptr->Service          = service;
        cmd_ptr->RequestBuffer    = INTERNAL_CMND;
        if (!(index=gdth_get_cmd_index(ha))) {
            TRACE(("GDT: No free command index found\n"));
            return 0;
        }
        gdth_set_sema0(ha);
        cmd_ptr->OpCode           = opcode;
        cmd_ptr->BoardNode        = LOCALBOARD;
        if (service == CACHESERVICE) {
            if (opcode == GDT_IOCTL) {
                cmd_ptr->u.ioctl.subfunc = p1;
                cmd_ptr->u.ioctl.channel = (ulong32)p2;
                cmd_ptr->u.ioctl.param_size = (ushort)p3;
                cmd_ptr->u.ioctl.p_param = ha->scratch_phys;
            } else {
                if (ha->cache_feat & GDT_64BIT) {
                    cmd_ptr->u.cache64.DeviceNo = (ushort)p1;
                    cmd_ptr->u.cache64.BlockNo  = p2;
                } else {
                    cmd_ptr->u.cache.DeviceNo = (ushort)p1;
                    cmd_ptr->u.cache.BlockNo  = (ulong32)p2;
                }
            }
        } else if (service == SCSIRAWSERVICE) {
            if (ha->raw_feat & GDT_64BIT) {
                cmd_ptr->u.raw64.direction  = p1;
                cmd_ptr->u.raw64.bus        = (unchar)p2;
                cmd_ptr->u.raw64.target     = (unchar)p3;
                cmd_ptr->u.raw64.lun        = (unchar)(p3 >> 8);
            } else {
                cmd_ptr->u.raw.direction  = p1;
                cmd_ptr->u.raw.bus        = (unchar)p2;
                cmd_ptr->u.raw.target     = (unchar)p3;
                cmd_ptr->u.raw.lun        = (unchar)(p3 >> 8);
            }
        } else if (service == SCREENSERVICE) {
            if (opcode == GDT_REALTIME) {
                *(ulong32 *)&cmd_ptr->u.screen.su.data[0] = p1;
                *(ulong32 *)&cmd_ptr->u.screen.su.data[4] = (ulong32)p2;
                *(ulong32 *)&cmd_ptr->u.screen.su.data[8] = (ulong32)p3;
            }
        }
        ha->cmd_len          = sizeof(gdth_cmd_str);
        ha->cmd_offs_dpmem   = 0;
        ha->cmd_cnt          = 0;
        gdth_copy_command(ha);
        gdth_release_event(ha);
        gdth_delay(20);
        if (!gdth_wait(ha, index, INIT_TIMEOUT)) {
            printk("GDT: Initialization error (timeout service %d)\n",service);
            return 0;
        }
        if (ha->status != S_BSY || --retries == 0)
            break;
        gdth_delay(1);   
    }   
    
    return (ha->status != S_OK ? 0:1);
}
    

/* search for devices */

static int __devinit gdth_search_drives(gdth_ha_str *ha)
{
    ushort cdev_cnt, i;
    int ok;
    ulong32 bus_no, drv_cnt, drv_no, j;
    gdth_getch_str *chn;
    gdth_drlist_str *drl;
    gdth_iochan_str *ioc;
    gdth_raw_iochan_str *iocr;
    gdth_arcdl_str *alst;
    gdth_alist_str *alst2;
    gdth_oem_str_ioctl *oemstr;
#ifdef INT_COAL
    gdth_perf_modes *pmod;
#endif

#ifdef GDTH_RTC
    unchar rtc[12];
    ulong flags;
#endif     
   
    TRACE(("gdth_search_drives() hanum %d\n", ha->hanum));
    ok = 0;

    /* initialize controller services, at first: screen service */
    ha->screen_feat = 0;
    if (!force_dma32) {
        ok = gdth_internal_cmd(ha, SCREENSERVICE, GDT_X_INIT_SCR, 0, 0, 0);
        if (ok)
            ha->screen_feat = GDT_64BIT;
    }
    if (force_dma32 || (!ok && ha->status == (ushort)S_NOFUNC))
        ok = gdth_internal_cmd(ha, SCREENSERVICE, GDT_INIT, 0, 0, 0);
    if (!ok) {
        printk("GDT-HA %d: Initialization error screen service (code %d)\n",
               ha->hanum, ha->status);
        return 0;
    }
    TRACE2(("gdth_search_drives(): SCREENSERVICE initialized\n"));

#ifdef GDTH_RTC
    /* read realtime clock info, send to controller */
    /* 1. wait for the falling edge of update flag */
    spin_lock_irqsave(&rtc_lock, flags);
    for (j = 0; j < 1000000; ++j)
        if (CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP)
            break;
    for (j = 0; j < 1000000; ++j)
        if (!(CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP))
            break;
    /* 2. read info */
    do {
        for (j = 0; j < 12; ++j) 
            rtc[j] = CMOS_READ(j);
    } while (rtc[0] != CMOS_READ(0));
    spin_unlock_irqrestore(&rtc_lock, flags);
    TRACE2(("gdth_search_drives(): RTC: %x/%x/%x\n",*(ulong32 *)&rtc[0],
            *(ulong32 *)&rtc[4], *(ulong32 *)&rtc[8]));
    /* 3. send to controller firmware */
    gdth_internal_cmd(ha, SCREENSERVICE, GDT_REALTIME, *(ulong32 *)&rtc[0],
                      *(ulong32 *)&rtc[4], *(ulong32 *)&rtc[8]);
#endif  
 
    /* unfreeze all IOs */
    gdth_internal_cmd(ha, CACHESERVICE, GDT_UNFREEZE_IO, 0, 0, 0);
 
    /* initialize cache service */
    ha->cache_feat = 0;
    if (!force_dma32) {
        ok = gdth_internal_cmd(ha, CACHESERVICE, GDT_X_INIT_HOST, LINUX_OS,
                                                                         0, 0);
        if (ok)
            ha->cache_feat = GDT_64BIT;
    }
    if (force_dma32 || (!ok && ha->status == (ushort)S_NOFUNC))
        ok = gdth_internal_cmd(ha, CACHESERVICE, GDT_INIT, LINUX_OS, 0, 0);
    if (!ok) {
        printk("GDT-HA %d: Initialization error cache service (code %d)\n",
               ha->hanum, ha->status);
        return 0;
    }
    TRACE2(("gdth_search_drives(): CACHESERVICE initialized\n"));
    cdev_cnt = (ushort)ha->info;
    ha->fw_vers = ha->service;

#ifdef INT_COAL
    if (ha->type == GDT_PCIMPR) {
        /* set perf. modes */
        pmod = (gdth_perf_modes *)ha->pscratch;
        pmod->version          = 1;
        pmod->st_mode          = 1;    /* enable one status buffer */
        *((ulong64 *)&pmod->st_buff_addr1) = ha->coal_stat_phys;
        pmod->st_buff_indx1    = COALINDEX;
        pmod->st_buff_addr2    = 0;
        pmod->st_buff_u_addr2  = 0;
        pmod->st_buff_indx2    = 0;
        pmod->st_buff_size     = sizeof(gdth_coal_status) * MAXOFFSETS;
        pmod->cmd_mode         = 0;    // disable all cmd buffers
        pmod->cmd_buff_addr1   = 0;
        pmod->cmd_buff_u_addr1 = 0;
        pmod->cmd_buff_indx1   = 0;
        pmod->cmd_buff_addr2   = 0;
        pmod->cmd_buff_u_addr2 = 0;
        pmod->cmd_buff_indx2   = 0;
        pmod->cmd_buff_size    = 0;
        pmod->reserved1        = 0;            
        pmod->reserved2        = 0;            
        if (gdth_internal_cmd(ha, CACHESERVICE, GDT_IOCTL, SET_PERF_MODES,
                              INVALID_CHANNEL,sizeof(gdth_perf_modes))) {
            printk("GDT-HA %d: Interrupt coalescing activated\n", ha->hanum);
        }
    }
#endif

    /* detect number of buses - try new IOCTL */
    iocr = (gdth_raw_iochan_str *)ha->pscratch;
    iocr->hdr.version        = 0xffffffff;
    iocr->hdr.list_entries   = MAXBUS;
    iocr->hdr.first_chan     = 0;
    iocr->hdr.last_chan      = MAXBUS-1;
    iocr->hdr.list_offset    = GDTOFFSOF(gdth_raw_iochan_str, list[0]);
    if (gdth_internal_cmd(ha, CACHESERVICE, GDT_IOCTL, IOCHAN_RAW_DESC,
                          INVALID_CHANNEL,sizeof(gdth_raw_iochan_str))) {
        TRACE2(("IOCHAN_RAW_DESC supported!\n"));
        ha->bus_cnt = iocr->hdr.chan_count;
        for (bus_no = 0; bus_no < ha->bus_cnt; ++bus_no) {
            if (iocr->list[bus_no].proc_id < MAXID)
                ha->bus_id[bus_no] = iocr->list[bus_no].proc_id;
            else
                ha->bus_id[bus_no] = 0xff;
        }
    } else {
        /* old method */
        chn = (gdth_getch_str *)ha->pscratch;
        for (bus_no = 0; bus_no < MAXBUS; ++bus_no) {
            chn->channel_no = bus_no;
            if (!gdth_internal_cmd(ha, CACHESERVICE, GDT_IOCTL,
                                   SCSI_CHAN_CNT | L_CTRL_PATTERN,
                                   IO_CHANNEL | INVALID_CHANNEL,
                                   sizeof(gdth_getch_str))) {
                if (bus_no == 0) {
                    printk("GDT-HA %d: Error detecting channel count (0x%x)\n",
                           ha->hanum, ha->status);
                    return 0;
                }
                break;
            }
            if (chn->siop_id < MAXID)
                ha->bus_id[bus_no] = chn->siop_id;
            else
                ha->bus_id[bus_no] = 0xff;
        }       
        ha->bus_cnt = (unchar)bus_no;
    }
    TRACE2(("gdth_search_drives() %d channels\n",ha->bus_cnt));

    /* read cache configuration */
    if (!gdth_internal_cmd(ha, CACHESERVICE, GDT_IOCTL, CACHE_INFO,
                           INVALID_CHANNEL,sizeof(gdth_cinfo_str))) {
        printk("GDT-HA %d: Initialization error cache service (code %d)\n",
               ha->hanum, ha->status);
        return 0;
    }
    ha->cpar = ((gdth_cinfo_str *)ha->pscratch)->cpar;
    TRACE2(("gdth_search_drives() cinfo: vs %x sta %d str %d dw %d b %d\n",
            ha->cpar.version,ha->cpar.state,ha->cpar.strategy,
            ha->cpar.write_back,ha->cpar.block_size));

    /* read board info and features */
    ha->more_proc = FALSE;
    if (gdth_internal_cmd(ha, CACHESERVICE, GDT_IOCTL, BOARD_INFO,
                          INVALID_CHANNEL,sizeof(gdth_binfo_str))) {
        memcpy(&ha->binfo, (gdth_binfo_str *)ha->pscratch,
               sizeof(gdth_binfo_str));
        if (gdth_internal_cmd(ha, CACHESERVICE, GDT_IOCTL, BOARD_FEATURES,
                              INVALID_CHANNEL,sizeof(gdth_bfeat_str))) {
            TRACE2(("BOARD_INFO/BOARD_FEATURES supported\n"));
            ha->bfeat = *(gdth_bfeat_str *)ha->pscratch;
            ha->more_proc = TRUE;
        }
    } else {
        TRACE2(("BOARD_INFO requires firmware >= 1.10/2.08\n"));
        strcpy(ha->binfo.type_string, gdth_ctr_name(ha));
    }
    TRACE2(("Controller name: %s\n",ha->binfo.type_string));

    /* read more informations */
    if (ha->more_proc) {
        /* physical drives, channel addresses */
        ioc = (gdth_iochan_str *)ha->pscratch;
        ioc->hdr.version        = 0xffffffff;
        ioc->hdr.list_entries   = MAXBUS;
        ioc->hdr.first_chan     = 0;
        ioc->hdr.last_chan      = MAXBUS-1;
        ioc->hdr.list_offset    = GDTOFFSOF(gdth_iochan_str, list[0]);
        if (gdth_internal_cmd(ha, CACHESERVICE, GDT_IOCTL, IOCHAN_DESC,
                              INVALID_CHANNEL,sizeof(gdth_iochan_str))) {
            for (bus_no = 0; bus_no < ha->bus_cnt; ++bus_no) {
                ha->raw[bus_no].address = ioc->list[bus_no].address;
                ha->raw[bus_no].local_no = ioc->list[bus_no].local_no;
            }
        } else {
            for (bus_no = 0; bus_no < ha->bus_cnt; ++bus_no) {
                ha->raw[bus_no].address = IO_CHANNEL;
                ha->raw[bus_no].local_no = bus_no;
            }
        }
        for (bus_no = 0; bus_no < ha->bus_cnt; ++bus_no) {
            chn = (gdth_getch_str *)ha->pscratch;
            chn->channel_no = ha->raw[bus_no].local_no;
            if (gdth_internal_cmd(ha, CACHESERVICE, GDT_IOCTL,
                                  SCSI_CHAN_CNT | L_CTRL_PATTERN,
                                  ha->raw[bus_no].address | INVALID_CHANNEL,
                                  sizeof(gdth_getch_str))) {
                ha->raw[bus_no].pdev_cnt = chn->drive_cnt;
                TRACE2(("Channel %d: %d phys. drives\n",
                        bus_no,chn->drive_cnt));
            }
            if (ha->raw[bus_no].pdev_cnt > 0) {
                drl = (gdth_drlist_str *)ha->pscratch;
                drl->sc_no = ha->raw[bus_no].local_no;
                drl->sc_cnt = ha->raw[bus_no].pdev_cnt;
                if (gdth_internal_cmd(ha, CACHESERVICE, GDT_IOCTL,
                                      SCSI_DR_LIST | L_CTRL_PATTERN,
                                      ha->raw[bus_no].address | INVALID_CHANNEL,
                                      sizeof(gdth_drlist_str))) {
                    for (j = 0; j < ha->raw[bus_no].pdev_cnt; ++j) 
                        ha->raw[bus_no].id_list[j] = drl->sc_list[j];
                } else {
                    ha->raw[bus_no].pdev_cnt = 0;
                }
            }
        }

        /* logical drives */
        if (gdth_internal_cmd(ha, CACHESERVICE, GDT_IOCTL, CACHE_DRV_CNT,
                              INVALID_CHANNEL,sizeof(ulong32))) {
            drv_cnt = *(ulong32 *)ha->pscratch;
            if (gdth_internal_cmd(ha, CACHESERVICE, GDT_IOCTL, CACHE_DRV_LIST,
                                  INVALID_CHANNEL,drv_cnt * sizeof(ulong32))) {
                for (j = 0; j < drv_cnt; ++j) {
                    drv_no = ((ulong32 *)ha->pscratch)[j];
                    if (drv_no < MAX_LDRIVES) {
                        ha->hdr[drv_no].is_logdrv = TRUE;
                        TRACE2(("Drive %d is log. drive\n",drv_no));
                    }
                }
            }
            alst = (gdth_arcdl_str *)ha->pscratch;
            alst->entries_avail = MAX_LDRIVES;
            alst->first_entry = 0;
            alst->list_offset = GDTOFFSOF(gdth_arcdl_str, list[0]);
            if (gdth_internal_cmd(ha, CACHESERVICE, GDT_IOCTL,
                                  ARRAY_DRV_LIST2 | LA_CTRL_PATTERN, 
                                  INVALID_CHANNEL, sizeof(gdth_arcdl_str) +
                                  (alst->entries_avail-1) * sizeof(gdth_alist_str))) { 
                for (j = 0; j < alst->entries_init; ++j) {
                    ha->hdr[j].is_arraydrv = alst->list[j].is_arrayd;
                    ha->hdr[j].is_master = alst->list[j].is_master;
                    ha->hdr[j].is_parity = alst->list[j].is_parity;
                    ha->hdr[j].is_hotfix = alst->list[j].is_hotfix;
                    ha->hdr[j].master_no = alst->list[j].cd_handle;
                }
            } else if (gdth_internal_cmd(ha, CACHESERVICE, GDT_IOCTL,
                                         ARRAY_DRV_LIST | LA_CTRL_PATTERN,
                                         0, 35 * sizeof(gdth_alist_str))) {
                for (j = 0; j < 35; ++j) {
                    alst2 = &((gdth_alist_str *)ha->pscratch)[j];
                    ha->hdr[j].is_arraydrv = alst2->is_arrayd;
                    ha->hdr[j].is_master = alst2->is_master;
                    ha->hdr[j].is_parity = alst2->is_parity;
                    ha->hdr[j].is_hotfix = alst2->is_hotfix;
                    ha->hdr[j].master_no = alst2->cd_handle;
                }
            }
        }
    }       
                                  
    /* initialize raw service */
    ha->raw_feat = 0;
    if (!force_dma32) {
        ok = gdth_internal_cmd(ha, SCSIRAWSERVICE, GDT_X_INIT_RAW, 0, 0, 0);
        if (ok)
            ha->raw_feat = GDT_64BIT;
    }
    if (force_dma32 || (!ok && ha->status == (ushort)S_NOFUNC))
        ok = gdth_internal_cmd(ha, SCSIRAWSERVICE, GDT_INIT, 0, 0, 0);
    if (!ok) {
        printk("GDT-HA %d: Initialization error raw service (code %d)\n",
               ha->hanum, ha->status);
        return 0;
    }
    TRACE2(("gdth_search_drives(): RAWSERVICE initialized\n"));

    /* set/get features raw service (scatter/gather) */
    if (gdth_internal_cmd(ha, SCSIRAWSERVICE, GDT_SET_FEAT, SCATTER_GATHER,
                          0, 0)) {
        TRACE2(("gdth_search_drives(): set features RAWSERVICE OK\n"));
        if (gdth_internal_cmd(ha, SCSIRAWSERVICE, GDT_GET_FEAT, 0, 0, 0)) {
            TRACE2(("gdth_search_dr(): get feat RAWSERVICE %d\n",
                    ha->info));
            ha->raw_feat |= (ushort)ha->info;
        }
    } 

    /* set/get features cache service (equal to raw service) */
    if (gdth_internal_cmd(ha, CACHESERVICE, GDT_SET_FEAT, 0,
                          SCATTER_GATHER,0)) {
        TRACE2(("gdth_search_drives(): set features CACHESERVICE OK\n"));
        if (gdth_internal_cmd(ha, CACHESERVICE, GDT_GET_FEAT, 0, 0, 0)) {
            TRACE2(("gdth_search_dr(): get feat CACHESERV. %d\n",
                    ha->info));
            ha->cache_feat |= (ushort)ha->info;
        }
    }

    /* reserve drives for raw service */
    if (reserve_mode != 0) {
        gdth_internal_cmd(ha, SCSIRAWSERVICE, GDT_RESERVE_ALL,
                          reserve_mode == 1 ? 1 : 3, 0, 0);
        TRACE2(("gdth_search_drives(): RESERVE_ALL code %d\n", 
                ha->status));
    }
    for (i = 0; i < MAX_RES_ARGS; i += 4) {
        if (reserve_list[i] == ha->hanum && reserve_list[i+1] < ha->bus_cnt &&
            reserve_list[i+2] < ha->tid_cnt && reserve_list[i+3] < MAXLUN) {
            TRACE2(("gdth_search_drives(): reserve ha %d bus %d id %d lun %d\n",
                    reserve_list[i], reserve_list[i+1],
                    reserve_list[i+2], reserve_list[i+3]));
            if (!gdth_internal_cmd(ha, SCSIRAWSERVICE, GDT_RESERVE, 0,
                                   reserve_list[i+1], reserve_list[i+2] | 
                                   (reserve_list[i+3] << 8))) {
                printk("GDT-HA %d: Error raw service (RESERVE, code %d)\n",
                       ha->hanum, ha->status);
             }
        }
    }

    /* Determine OEM string using IOCTL */
    oemstr = (gdth_oem_str_ioctl *)ha->pscratch;
    oemstr->params.ctl_version = 0x01;
    oemstr->params.buffer_size = sizeof(oemstr->text);
    if (gdth_internal_cmd(ha, CACHESERVICE, GDT_IOCTL,
                          CACHE_READ_OEM_STRING_RECORD,INVALID_CHANNEL,
                          sizeof(gdth_oem_str_ioctl))) {
        TRACE2(("gdth_search_drives(): CACHE_READ_OEM_STRING_RECORD OK\n"));
        printk("GDT-HA %d: Vendor: %s Name: %s\n",
               ha->hanum, oemstr->text.oem_company_name, ha->binfo.type_string);
        /* Save the Host Drive inquiry data */
        strlcpy(ha->oem_name,oemstr->text.scsi_host_drive_inquiry_vendor_id,
                sizeof(ha->oem_name));
    } else {
        /* Old method, based on PCI ID */
        TRACE2(("gdth_search_drives(): CACHE_READ_OEM_STRING_RECORD failed\n"));
        printk("GDT-HA %d: Name: %s\n",
               ha->hanum, ha->binfo.type_string);
        if (ha->oem_id == OEM_ID_INTEL)
            strlcpy(ha->oem_name,"Intel  ", sizeof(ha->oem_name));
        else
            strlcpy(ha->oem_name,"ICP    ", sizeof(ha->oem_name));
    }

    /* scanning for host drives */
    for (i = 0; i < cdev_cnt; ++i) 
        gdth_analyse_hdrive(ha, i);
    
    TRACE(("gdth_search_drives() OK\n"));
    return 1;
}

static int gdth_analyse_hdrive(gdth_ha_str *ha, ushort hdrive)
{
    ulong32 drv_cyls;
    int drv_hds, drv_secs;

    TRACE(("gdth_analyse_hdrive() hanum %d drive %d\n", ha->hanum, hdrive));
    if (hdrive >= MAX_HDRIVES)
        return 0;

    if (!gdth_internal_cmd(ha, CACHESERVICE, GDT_INFO, hdrive, 0, 0))
        return 0;
    ha->hdr[hdrive].present = TRUE;
    ha->hdr[hdrive].size = ha->info;
   
    /* evaluate mapping (sectors per head, heads per cylinder) */
    ha->hdr[hdrive].size &= ~SECS32;
    if (ha->info2 == 0) {
        gdth_eval_mapping(ha->hdr[hdrive].size,&drv_cyls,&drv_hds,&drv_secs);
    } else {
        drv_hds = ha->info2 & 0xff;
        drv_secs = (ha->info2 >> 8) & 0xff;
        drv_cyls = (ulong32)ha->hdr[hdrive].size / drv_hds / drv_secs;
    }
    ha->hdr[hdrive].heads = (unchar)drv_hds;
    ha->hdr[hdrive].secs  = (unchar)drv_secs;
    /* round size */
    ha->hdr[hdrive].size  = drv_cyls * drv_hds * drv_secs;
    
    if (ha->cache_feat & GDT_64BIT) {
        if (gdth_internal_cmd(ha, CACHESERVICE, GDT_X_INFO, hdrive, 0, 0)
            && ha->info2 != 0) {
            ha->hdr[hdrive].size = ((ulong64)ha->info2 << 32) | ha->info;
        }
    }
    TRACE2(("gdth_search_dr() cdr. %d size %d hds %d scs %d\n",
            hdrive,ha->hdr[hdrive].size,drv_hds,drv_secs));

    /* get informations about device */
    if (gdth_internal_cmd(ha, CACHESERVICE, GDT_DEVTYPE, hdrive, 0, 0)) {
        TRACE2(("gdth_search_dr() cache drive %d devtype %d\n",
                hdrive,ha->info));
        ha->hdr[hdrive].devtype = (ushort)ha->info;
    }

    /* cluster info */
    if (gdth_internal_cmd(ha, CACHESERVICE, GDT_CLUST_INFO, hdrive, 0, 0)) {
        TRACE2(("gdth_search_dr() cache drive %d cluster info %d\n",
                hdrive,ha->info));
        if (!shared_access)
            ha->hdr[hdrive].cluster_type = (unchar)ha->info;
    }

    /* R/W attributes */
    if (gdth_internal_cmd(ha, CACHESERVICE, GDT_RW_ATTRIBS, hdrive, 0, 0)) {
        TRACE2(("gdth_search_dr() cache drive %d r/w attrib. %d\n",
                hdrive,ha->info));
        ha->hdr[hdrive].rw_attribs = (unchar)ha->info;
    }

    return 1;
}


/* command queueing/sending functions */

static void gdth_putq(gdth_ha_str *ha, Scsi_Cmnd *scp, unchar priority)
{
    struct gdth_cmndinfo *cmndinfo = gdth_cmnd_priv(scp);
    register Scsi_Cmnd *pscp;
    register Scsi_Cmnd *nscp;
    ulong flags;

    TRACE(("gdth_putq() priority %d\n",priority));
    spin_lock_irqsave(&ha->smp_lock, flags);

    if (!cmndinfo->internal_command)
        cmndinfo->priority = priority;

    if (ha->req_first==NULL) {
        ha->req_first = scp;                    /* queue was empty */
        scp->SCp.ptr = NULL;
    } else {                                    /* queue not empty */
        pscp = ha->req_first;
        nscp = (Scsi_Cmnd *)pscp->SCp.ptr;
        /* priority: 0-highest,..,0xff-lowest */
        while (nscp && gdth_cmnd_priv(nscp)->priority <= priority) {
            pscp = nscp;
            nscp = (Scsi_Cmnd *)pscp->SCp.ptr;
        }
        pscp->SCp.ptr = (char *)scp;
        scp->SCp.ptr  = (char *)nscp;
    }
    spin_unlock_irqrestore(&ha->smp_lock, flags);

#ifdef GDTH_STATISTICS
    flags = 0;
    for (nscp=ha->req_first; nscp; nscp=(Scsi_Cmnd*)nscp->SCp.ptr)
        ++flags;
    if (max_rq < flags) {
        max_rq = flags;
        TRACE3(("GDT: max_rq = %d\n",(ushort)max_rq));
    }
#endif
}

static void gdth_next(gdth_ha_str *ha)
{
    register Scsi_Cmnd *pscp;
    register Scsi_Cmnd *nscp;
    unchar b, t, l, firsttime;
    unchar this_cmd, next_cmd;
    ulong flags = 0;
    int cmd_index;

    TRACE(("gdth_next() hanum %d\n", ha->hanum));
    if (!gdth_polling) 
        spin_lock_irqsave(&ha->smp_lock, flags);

    ha->cmd_cnt = ha->cmd_offs_dpmem = 0;
    this_cmd = firsttime = TRUE;
    next_cmd = gdth_polling ? FALSE:TRUE;
    cmd_index = 0;

    for (nscp = pscp = ha->req_first; nscp; nscp = (Scsi_Cmnd *)nscp->SCp.ptr) {
        struct gdth_cmndinfo *nscp_cmndinfo = gdth_cmnd_priv(nscp);
        if (nscp != pscp && nscp != (Scsi_Cmnd *)pscp->SCp.ptr)
            pscp = (Scsi_Cmnd *)pscp->SCp.ptr;
        if (!nscp_cmndinfo->internal_command) {
            b = nscp->device->channel;
            t = nscp->device->id;
            l = nscp->device->lun;
            if (nscp_cmndinfo->priority >= DEFAULT_PRI) {
                if ((b != ha->virt_bus && ha->raw[BUS_L2P(ha,b)].lock) ||
                    (b == ha->virt_bus && t < MAX_HDRIVES && ha->hdr[t].lock))
                    continue;
            }
        } else
            b = t = l = 0;

        if (firsttime) {
            if (gdth_test_busy(ha)) {        /* controller busy ? */
                TRACE(("gdth_next() controller %d busy !\n", ha->hanum));
                if (!gdth_polling) {
                    spin_unlock_irqrestore(&ha->smp_lock, flags);
                    return;
                }
                while (gdth_test_busy(ha))
                    gdth_delay(1);
            }   
            firsttime = FALSE;
        }

        if (!nscp_cmndinfo->internal_command) {
        if (nscp_cmndinfo->phase == -1) {
            nscp_cmndinfo->phase = CACHESERVICE;           /* default: cache svc. */
            if (nscp->cmnd[0] == TEST_UNIT_READY) {
                TRACE2(("TEST_UNIT_READY Bus %d Id %d LUN %d\n", 
                        b, t, l));
                /* TEST_UNIT_READY -> set scan mode */
                if ((ha->scan_mode & 0x0f) == 0) {
                    if (b == 0 && t == 0 && l == 0) {
                        ha->scan_mode |= 1;
                        TRACE2(("Scan mode: 0x%x\n", ha->scan_mode));
                    }
                } else if ((ha->scan_mode & 0x0f) == 1) {
                    if (b == 0 && ((t == 0 && l == 1) ||
                         (t == 1 && l == 0))) {
                        nscp_cmndinfo->OpCode = GDT_SCAN_START;
                        nscp_cmndinfo->phase = ((ha->scan_mode & 0x10 ? 1:0) << 8)
                            | SCSIRAWSERVICE;
                        ha->scan_mode = 0x12;
                        TRACE2(("Scan mode: 0x%x (SCAN_START)\n", 
                                ha->scan_mode));
                    } else {
                        ha->scan_mode &= 0x10;
                        TRACE2(("Scan mode: 0x%x\n", ha->scan_mode));
                    }                   
                } else if (ha->scan_mode == 0x12) {
                    if (b == ha->bus_cnt && t == ha->tid_cnt-1) {
                        nscp_cmndinfo->phase = SCSIRAWSERVICE;
                        nscp_cmndinfo->OpCode = GDT_SCAN_END;
                        ha->scan_mode &= 0x10;
                        TRACE2(("Scan mode: 0x%x (SCAN_END)\n", 
                                ha->scan_mode));
                    }
                }
            }
            if (b == ha->virt_bus && nscp->cmnd[0] != INQUIRY &&
                nscp->cmnd[0] != READ_CAPACITY && nscp->cmnd[0] != MODE_SENSE &&
                (ha->hdr[t].cluster_type & CLUSTER_DRIVE)) {
                /* always GDT_CLUST_INFO! */
                nscp_cmndinfo->OpCode = GDT_CLUST_INFO;
            }
        }
        }

        if (nscp_cmndinfo->OpCode != -1) {
            if ((nscp_cmndinfo->phase & 0xff) == CACHESERVICE) {
                if (!(cmd_index=gdth_fill_cache_cmd(ha, nscp, t)))
                    this_cmd = FALSE;
                next_cmd = FALSE;
            } else if ((nscp_cmndinfo->phase & 0xff) == SCSIRAWSERVICE) {
                if (!(cmd_index=gdth_fill_raw_cmd(ha, nscp, BUS_L2P(ha, b))))
                    this_cmd = FALSE;
                next_cmd = FALSE;
            } else {
                memset((char*)nscp->sense_buffer,0,16);
                nscp->sense_buffer[0] = 0x70;
                nscp->sense_buffer[2] = NOT_READY;
                nscp->result = (DID_OK << 16) | (CHECK_CONDITION << 1);
                if (!nscp_cmndinfo->wait_for_completion)
                    nscp_cmndinfo->wait_for_completion++;
                else
                    gdth_scsi_done(nscp);
            }
        } else if (gdth_cmnd_priv(nscp)->internal_command) {
            if (!(cmd_index=gdth_special_cmd(ha, nscp)))
                this_cmd = FALSE;
            next_cmd = FALSE;
        } else if (b != ha->virt_bus) {
            if (ha->raw[BUS_L2P(ha,b)].io_cnt[t] >= GDTH_MAX_RAW ||
                !(cmd_index=gdth_fill_raw_cmd(ha, nscp, BUS_L2P(ha, b))))
                this_cmd = FALSE;
            else 
                ha->raw[BUS_L2P(ha,b)].io_cnt[t]++;
        } else if (t >= MAX_HDRIVES || !ha->hdr[t].present || l != 0) {
            TRACE2(("Command 0x%x to bus %d id %d lun %d -> IGNORE\n",
                    nscp->cmnd[0], b, t, l));
            nscp->result = DID_BAD_TARGET << 16;
            if (!nscp_cmndinfo->wait_for_completion)
                nscp_cmndinfo->wait_for_completion++;
            else
                gdth_scsi_done(nscp);
        } else {
            switch (nscp->cmnd[0]) {
              case TEST_UNIT_READY:
              case INQUIRY:
              case REQUEST_SENSE:
              case READ_CAPACITY:
              case VERIFY:
              case START_STOP:
              case MODE_SENSE:
              case SERVICE_ACTION_IN:
                TRACE(("cache cmd %x/%x/%x/%x/%x/%x\n",nscp->cmnd[0],
                       nscp->cmnd[1],nscp->cmnd[2],nscp->cmnd[3],
                       nscp->cmnd[4],nscp->cmnd[5]));
                if (ha->hdr[t].media_changed && nscp->cmnd[0] != INQUIRY) {
                    /* return UNIT_ATTENTION */
                    TRACE2(("cmd 0x%x target %d: UNIT_ATTENTION\n",
                             nscp->cmnd[0], t));
                    ha->hdr[t].media_changed = FALSE;
                    memset((char*)nscp->sense_buffer,0,16);
                    nscp->sense_buffer[0] = 0x70;
                    nscp->sense_buffer[2] = UNIT_ATTENTION;
                    nscp->result = (DID_OK << 16) | (CHECK_CONDITION << 1);
                    if (!nscp_cmndinfo->wait_for_completion)
                        nscp_cmndinfo->wait_for_completion++;
                    else
                        gdth_scsi_done(nscp);
                } else if (gdth_internal_cache_cmd(ha, nscp))
                    gdth_scsi_done(nscp);
                break;

              case ALLOW_MEDIUM_REMOVAL:
                TRACE(("cache cmd %x/%x/%x/%x/%x/%x\n",nscp->cmnd[0],
                       nscp->cmnd[1],nscp->cmnd[2],nscp->cmnd[3],
                       nscp->cmnd[4],nscp->cmnd[5]));
                if ( (nscp->cmnd[4]&1) && !(ha->hdr[t].devtype&1) ) {
                    TRACE(("Prevent r. nonremov. drive->do nothing\n"));
                    nscp->result = DID_OK << 16;
                    nscp->sense_buffer[0] = 0;
                    if (!nscp_cmndinfo->wait_for_completion)
                        nscp_cmndinfo->wait_for_completion++;
                    else
                        gdth_scsi_done(nscp);
                } else {
                    nscp->cmnd[3] = (ha->hdr[t].devtype&1) ? 1:0;
                    TRACE(("Prevent/allow r. %d rem. drive %d\n",
                           nscp->cmnd[4],nscp->cmnd[3]));
                    if (!(cmd_index=gdth_fill_cache_cmd(ha, nscp, t)))
                        this_cmd = FALSE;
                }
                break;
                
              case RESERVE:
              case RELEASE:
                TRACE2(("cache cmd %s\n",nscp->cmnd[0] == RESERVE ?
                        "RESERVE" : "RELEASE"));
                if (!(cmd_index=gdth_fill_cache_cmd(ha, nscp, t)))
                    this_cmd = FALSE;
                break;
                
              case READ_6:
              case WRITE_6:
              case READ_10:
              case WRITE_10:
              case READ_16:
              case WRITE_16:
                if (ha->hdr[t].media_changed) {
                    /* return UNIT_ATTENTION */
                    TRACE2(("cmd 0x%x target %d: UNIT_ATTENTION\n",
                             nscp->cmnd[0], t));
                    ha->hdr[t].media_changed = FALSE;
                    memset((char*)nscp->sense_buffer,0,16);
                    nscp->sense_buffer[0] = 0x70;
                    nscp->sense_buffer[2] = UNIT_ATTENTION;
                    nscp->result = (DID_OK << 16) | (CHECK_CONDITION << 1);
                    if (!nscp_cmndinfo->wait_for_completion)
                        nscp_cmndinfo->wait_for_completion++;
                    else
                        gdth_scsi_done(nscp);
                } else if (!(cmd_index=gdth_fill_cache_cmd(ha, nscp, t)))
                    this_cmd = FALSE;
                break;

              default:
                TRACE2(("cache cmd %x/%x/%x/%x/%x/%x unknown\n",nscp->cmnd[0],
                        nscp->cmnd[1],nscp->cmnd[2],nscp->cmnd[3],
                        nscp->cmnd[4],nscp->cmnd[5]));
                printk("GDT-HA %d: Unknown SCSI command 0x%x to cache service !\n",
                       ha->hanum, nscp->cmnd[0]);
                nscp->result = DID_ABORT << 16;
                if (!nscp_cmndinfo->wait_for_completion)
                    nscp_cmndinfo->wait_for_completion++;
                else
                    gdth_scsi_done(nscp);
                break;
            }
        }

        if (!this_cmd)
            break;
        if (nscp == ha->req_first)
            ha->req_first = pscp = (Scsi_Cmnd *)nscp->SCp.ptr;
        else
            pscp->SCp.ptr = nscp->SCp.ptr;
        if (!next_cmd)
            break;
    }

    if (ha->cmd_cnt > 0) {
        gdth_release_event(ha);
    }

    if (!gdth_polling) 
        spin_unlock_irqrestore(&ha->smp_lock, flags);

    if (gdth_polling && ha->cmd_cnt > 0) {
        if (!gdth_wait(ha, cmd_index, POLL_TIMEOUT))
            printk("GDT-HA %d: Command %d timed out !\n",
                   ha->hanum, cmd_index);
    }
}

/*
 * gdth_copy_internal_data() - copy to/from a buffer onto a scsi_cmnd's
 * buffers, kmap_atomic() as needed.
 */
static void gdth_copy_internal_data(gdth_ha_str *ha, Scsi_Cmnd *scp,
                                    char *buffer, ushort count)
{
    ushort cpcount,i, max_sg = scsi_sg_count(scp);
    ushort cpsum,cpnow;
    struct scatterlist *sl;
    char *address;

    cpcount = min_t(ushort, count, scsi_bufflen(scp));

    if (cpcount) {
        cpsum=0;
        scsi_for_each_sg(scp, sl, max_sg, i) {
            unsigned long flags;
            cpnow = (ushort)sl->length;
            TRACE(("copy_internal() now %d sum %d count %d %d\n",
                          cpnow, cpsum, cpcount, scsi_bufflen(scp)));
            if (cpsum+cpnow > cpcount) 
                cpnow = cpcount - cpsum;
            cpsum += cpnow;
            if (!sg_page(sl)) {
                printk("GDT-HA %d: invalid sc/gt element in gdth_copy_internal_data()\n",
                       ha->hanum);
                return;
            }
            local_irq_save(flags);
            address = kmap_atomic(sg_page(sl), KM_BIO_SRC_IRQ) + sl->offset;
            memcpy(address, buffer, cpnow);
            flush_dcache_page(sg_page(sl));
            kunmap_atomic(address, KM_BIO_SRC_IRQ);
            local_irq_restore(flags);
            if (cpsum == cpcount)
                break;
            buffer += cpnow;
        }
    } else if (count) {
        printk("GDT-HA %d: SCSI command with no buffers but data transfer expected!\n",
               ha->hanum);
        WARN_ON(1);
    }
}

static int gdth_internal_cache_cmd(gdth_ha_str *ha, Scsi_Cmnd *scp)
{
    unchar t;
    gdth_inq_data inq;
    gdth_rdcap_data rdc;
    gdth_sense_data sd;
    gdth_modep_data mpd;
    struct gdth_cmndinfo *cmndinfo = gdth_cmnd_priv(scp);

    t  = scp->device->id;
    TRACE(("gdth_internal_cache_cmd() cmd 0x%x hdrive %d\n",
           scp->cmnd[0],t));

    scp->result = DID_OK << 16;
    scp->sense_buffer[0] = 0;

    switch (scp->cmnd[0]) {
      case TEST_UNIT_READY:
      case VERIFY:
      case START_STOP:
        TRACE2(("Test/Verify/Start hdrive %d\n",t));
        break;

      case INQUIRY:
        TRACE2(("Inquiry hdrive %d devtype %d\n",
                t,ha->hdr[t].devtype));
        inq.type_qual = (ha->hdr[t].devtype&4) ? TYPE_ROM:TYPE_DISK;
        /* you can here set all disks to removable, if you want to do
           a flush using the ALLOW_MEDIUM_REMOVAL command */
        inq.modif_rmb = 0x00;
        if ((ha->hdr[t].devtype & 1) ||
            (ha->hdr[t].cluster_type & CLUSTER_DRIVE))
            inq.modif_rmb = 0x80;
        inq.version   = 2;
        inq.resp_aenc = 2;
        inq.add_length= 32;
        strcpy(inq.vendor,ha->oem_name);
        sprintf(inq.product,"Host Drive  #%02d",t);
        strcpy(inq.revision,"   ");
        gdth_copy_internal_data(ha, scp, (char*)&inq, sizeof(gdth_inq_data));
        break;

      case REQUEST_SENSE:
        TRACE2(("Request sense hdrive %d\n",t));
        sd.errorcode = 0x70;
        sd.segno     = 0x00;
        sd.key       = NO_SENSE;
        sd.info      = 0;
        sd.add_length= 0;
        gdth_copy_internal_data(ha, scp, (char*)&sd, sizeof(gdth_sense_data));
        break;

      case MODE_SENSE:
        TRACE2(("Mode sense hdrive %d\n",t));
        memset((char*)&mpd,0,sizeof(gdth_modep_data));
        mpd.hd.data_length = sizeof(gdth_modep_data);
        mpd.hd.dev_par     = (ha->hdr[t].devtype&2) ? 0x80:0;
        mpd.hd.bd_length   = sizeof(mpd.bd);
        mpd.bd.block_length[0] = (SECTOR_SIZE & 0x00ff0000) >> 16;
        mpd.bd.block_length[1] = (SECTOR_SIZE & 0x0000ff00) >> 8;
        mpd.bd.block_length[2] = (SECTOR_SIZE & 0x000000ff);
        gdth_copy_internal_data(ha, scp, (char*)&mpd, sizeof(gdth_modep_data));
        break;

      case READ_CAPACITY:
        TRACE2(("Read capacity hdrive %d\n",t));
        if (ha->hdr[t].size > (ulong64)0xffffffff)
            rdc.last_block_no = 0xffffffff;
        else
            rdc.last_block_no = cpu_to_be32(ha->hdr[t].size-1);
        rdc.block_length  = cpu_to_be32(SECTOR_SIZE);
        gdth_copy_internal_data(ha, scp, (char*)&rdc, sizeof(gdth_rdcap_data));
        break;

      case SERVICE_ACTION_IN:
        if ((scp->cmnd[1] & 0x1f) == SAI_READ_CAPACITY_16 &&
            (ha->cache_feat & GDT_64BIT)) {
            gdth_rdcap16_data rdc16;

            TRACE2(("Read capacity (16) hdrive %d\n",t));
            rdc16.last_block_no = cpu_to_be64(ha->hdr[t].size-1);
            rdc16.block_length  = cpu_to_be32(SECTOR_SIZE);
            gdth_copy_internal_data(ha, scp, (char*)&rdc16,
                                                 sizeof(gdth_rdcap16_data));
        } else { 
            scp->result = DID_ABORT << 16;
        }
        break;

      default:
        TRACE2(("Internal cache cmd 0x%x unknown\n",scp->cmnd[0]));
        break;
    }

    if (!cmndinfo->wait_for_completion)
        cmndinfo->wait_for_completion++;
    else 
        return 1;

    return 0;
}

static int gdth_fill_cache_cmd(gdth_ha_str *ha, Scsi_Cmnd *scp, ushort hdrive)
{
    register gdth_cmd_str *cmdp;
    struct gdth_cmndinfo *cmndinfo = gdth_cmnd_priv(scp);
    ulong32 cnt, blockcnt;
    ulong64 no, blockno;
    int i, cmd_index, read_write, sgcnt, mode64;

    cmdp = ha->pccb;
    TRACE(("gdth_fill_cache_cmd() cmd 0x%x cmdsize %d hdrive %d\n",
                 scp->cmnd[0],scp->cmd_len,hdrive));

    if (ha->type==GDT_EISA && ha->cmd_cnt>0) 
        return 0;

    mode64 = (ha->cache_feat & GDT_64BIT) ? TRUE : FALSE;
    /* test for READ_16, WRITE_16 if !mode64 ? ---
       not required, should not occur due to error return on 
       READ_CAPACITY_16 */

    cmdp->Service = CACHESERVICE;
    cmdp->RequestBuffer = scp;
    /* search free command index */
    if (!(cmd_index=gdth_get_cmd_index(ha))) {
        TRACE(("GDT: No free command index found\n"));
        return 0;
    }
    /* if it's the first command, set command semaphore */
    if (ha->cmd_cnt == 0)
        gdth_set_sema0(ha);

    /* fill command */
    read_write = 0;
    if (cmndinfo->OpCode != -1)
        cmdp->OpCode = cmndinfo->OpCode;   /* special cache cmd. */
    else if (scp->cmnd[0] == RESERVE) 
        cmdp->OpCode = GDT_RESERVE_DRV;
    else if (scp->cmnd[0] == RELEASE)
        cmdp->OpCode = GDT_RELEASE_DRV;
    else if (scp->cmnd[0] == ALLOW_MEDIUM_REMOVAL) {
        if (scp->cmnd[4] & 1)                   /* prevent ? */
            cmdp->OpCode = GDT_MOUNT;
        else if (scp->cmnd[3] & 1)              /* removable drive ? */
            cmdp->OpCode = GDT_UNMOUNT;
        else
            cmdp->OpCode = GDT_FLUSH;
    } else if (scp->cmnd[0] == WRITE_6 || scp->cmnd[0] == WRITE_10 ||
               scp->cmnd[0] == WRITE_12 || scp->cmnd[0] == WRITE_16
    ) {
        read_write = 1;
        if (gdth_write_through || ((ha->hdr[hdrive].rw_attribs & 1) && 
                                   (ha->cache_feat & GDT_WR_THROUGH)))
            cmdp->OpCode = GDT_WRITE_THR;
        else
            cmdp->OpCode = GDT_WRITE;
    } else {
        read_write = 2;
        cmdp->OpCode = GDT_READ;
    }

    cmdp->BoardNode = LOCALBOARD;
    if (mode64) {
        cmdp->u.cache64.DeviceNo = hdrive;
        cmdp->u.cache64.BlockNo  = 1;
        cmdp->u.cache64.sg_canz  = 0;
    } else {
        cmdp->u.cache.DeviceNo = hdrive;
        cmdp->u.cache.BlockNo  = 1;
        cmdp->u.cache.sg_canz  = 0;
    }

    if (read_write) {
        if (scp->cmd_len == 16) {
            memcpy(&no, &scp->cmnd[2], sizeof(ulong64));
            blockno = be64_to_cpu(no);
            memcpy(&cnt, &scp->cmnd[10], sizeof(ulong32));
            blockcnt = be32_to_cpu(cnt);
        } else if (scp->cmd_len == 10) {
            memcpy(&no, &scp->cmnd[2], sizeof(ulong32));
            blockno = be32_to_cpu(no);
            memcpy(&cnt, &scp->cmnd[7], sizeof(ushort));
            blockcnt = be16_to_cpu(cnt);
        } else {
            memcpy(&no, &scp->cmnd[0], sizeof(ulong32));
            blockno = be32_to_cpu(no) & 0x001fffffUL;
            blockcnt= scp->cmnd[4]==0 ? 0x100 : scp->cmnd[4];
        }
        if (mode64) {
            cmdp->u.cache64.BlockNo = blockno;
            cmdp->u.cache64.BlockCnt = blockcnt;
        } else {
            cmdp->u.cache.BlockNo = (ulong32)blockno;
            cmdp->u.cache.BlockCnt = blockcnt;
        }

        if (scsi_bufflen(scp)) {
            cmndinfo->dma_dir = (read_write == 1 ?
                PCI_DMA_TODEVICE : PCI_DMA_FROMDEVICE);   
            sgcnt = pci_map_sg(ha->pdev, scsi_sglist(scp), scsi_sg_count(scp),
                               cmndinfo->dma_dir);
            if (mode64) {
                struct scatterlist *sl;

                cmdp->u.cache64.DestAddr= (ulong64)-1;
                cmdp->u.cache64.sg_canz = sgcnt;
                scsi_for_each_sg(scp, sl, sgcnt, i) {
                    cmdp->u.cache64.sg_lst[i].sg_ptr = sg_dma_address(sl);
#ifdef GDTH_DMA_STATISTICS
                    if (cmdp->u.cache64.sg_lst[i].sg_ptr > (ulong64)0xffffffff)
                        ha->dma64_cnt++;
                    else
                        ha->dma32_cnt++;
#endif
                    cmdp->u.cache64.sg_lst[i].sg_len = sg_dma_len(sl);
                }
            } else {
                struct scatterlist *sl;

                cmdp->u.cache.DestAddr= 0xffffffff;
                cmdp->u.cache.sg_canz = sgcnt;
                scsi_for_each_sg(scp, sl, sgcnt, i) {
                    cmdp->u.cache.sg_lst[i].sg_ptr = sg_dma_address(sl);
#ifdef GDTH_DMA_STATISTICS
                    ha->dma32_cnt++;
#endif
                    cmdp->u.cache.sg_lst[i].sg_len = sg_dma_len(sl);
                }
            }

#ifdef GDTH_STATISTICS
            if (max_sg < (ulong32)sgcnt) {
                max_sg = (ulong32)sgcnt;
                TRACE3(("GDT: max_sg = %d\n",max_sg));
            }
#endif

        }
    }
    /* evaluate command size, check space */
    if (mode64) {
        TRACE(("cache cmd: addr. %x sganz %x sgptr0 %x sglen0 %x\n",
               cmdp->u.cache64.DestAddr,cmdp->u.cache64.sg_canz,
               cmdp->u.cache64.sg_lst[0].sg_ptr,
               cmdp->u.cache64.sg_lst[0].sg_len));
        TRACE(("cache cmd: cmd %d blockno. %d, blockcnt %d\n",
               cmdp->OpCode,cmdp->u.cache64.BlockNo,cmdp->u.cache64.BlockCnt));
        ha->cmd_len = GDTOFFSOF(gdth_cmd_str,u.cache64.sg_lst) +
            (ushort)cmdp->u.cache64.sg_canz * sizeof(gdth_sg64_str);
    } else {
        TRACE(("cache cmd: addr. %x sganz %x sgptr0 %x sglen0 %x\n",
               cmdp->u.cache.DestAddr,cmdp->u.cache.sg_canz,
               cmdp->u.cache.sg_lst[0].sg_ptr,
               cmdp->u.cache.sg_lst[0].sg_len));
        TRACE(("cache cmd: cmd %d blockno. %d, blockcnt %d\n",
               cmdp->OpCode,cmdp->u.cache.BlockNo,cmdp->u.cache.BlockCnt));
        ha->cmd_len = GDTOFFSOF(gdth_cmd_str,u.cache.sg_lst) +
            (ushort)cmdp->u.cache.sg_canz * sizeof(gdth_sg_str);
    }
    if (ha->cmd_len & 3)
        ha->cmd_len += (4 - (ha->cmd_len & 3));

    if (ha->cmd_cnt > 0) {
        if ((ha->cmd_offs_dpmem + ha->cmd_len + DPMEM_COMMAND_OFFSET) >
            ha->ic_all_size) {
            TRACE2(("gdth_fill_cache() DPMEM overflow\n"));
            ha->cmd_tab[cmd_index-2].cmnd = UNUSED_CMND;
            return 0;
        }
    }

    /* copy command */
    gdth_copy_command(ha);
    return cmd_index;
}

static int gdth_fill_raw_cmd(gdth_ha_str *ha, Scsi_Cmnd *scp, unchar b)
{
    register gdth_cmd_str *cmdp;
    ushort i;
    dma_addr_t sense_paddr;
    int cmd_index, sgcnt, mode64;
    unchar t,l;
    struct page *page;
    ulong offset;
    struct gdth_cmndinfo *cmndinfo;

    t = scp->device->id;
    l = scp->device->lun;
    cmdp = ha->pccb;
    TRACE(("gdth_fill_raw_cmd() cmd 0x%x bus %d ID %d LUN %d\n",
           scp->cmnd[0],b,t,l));

    if (ha->type==GDT_EISA && ha->cmd_cnt>0) 
        return 0;

    mode64 = (ha->raw_feat & GDT_64BIT) ? TRUE : FALSE;

    cmdp->Service = SCSIRAWSERVICE;
    cmdp->RequestBuffer = scp;
    /* search free command index */
    if (!(cmd_index=gdth_get_cmd_index(ha))) {
        TRACE(("GDT: No free command index found\n"));
        return 0;
    }
    /* if it's the first command, set command semaphore */
    if (ha->cmd_cnt == 0)
        gdth_set_sema0(ha);

    cmndinfo = gdth_cmnd_priv(scp);
    /* fill command */  
    if (cmndinfo->OpCode != -1) {
        cmdp->OpCode           = cmndinfo->OpCode; /* special raw cmd. */
        cmdp->BoardNode        = LOCALBOARD;
        if (mode64) {
            cmdp->u.raw64.direction = (cmndinfo->phase >> 8);
            TRACE2(("special raw cmd 0x%x param 0x%x\n", 
                    cmdp->OpCode, cmdp->u.raw64.direction));
            /* evaluate command size */
            ha->cmd_len = GDTOFFSOF(gdth_cmd_str,u.raw64.sg_lst);
        } else {
            cmdp->u.raw.direction  = (cmndinfo->phase >> 8);
            TRACE2(("special raw cmd 0x%x param 0x%x\n", 
                    cmdp->OpCode, cmdp->u.raw.direction));
            /* evaluate command size */
            ha->cmd_len = GDTOFFSOF(gdth_cmd_str,u.raw.sg_lst);
        }

    } else {
        page = virt_to_page(scp->sense_buffer);
        offset = (ulong)scp->sense_buffer & ~PAGE_MASK;
        sense_paddr = pci_map_page(ha->pdev,page,offset,
                                   16,PCI_DMA_FROMDEVICE);

	cmndinfo->sense_paddr  = sense_paddr;
        cmdp->OpCode           = GDT_WRITE;             /* always */
        cmdp->BoardNode        = LOCALBOARD;
        if (mode64) { 
            cmdp->u.raw64.reserved   = 0;
            cmdp->u.raw64.mdisc_time = 0;
            cmdp->u.raw64.mcon_time  = 0;
            cmdp->u.raw64.clen       = scp->cmd_len;
            cmdp->u.raw64.target     = t;
            cmdp->u.raw64.lun        = l;
            cmdp->u.raw64.bus        = b;
            cmdp->u.raw64.priority   = 0;
            cmdp->u.raw64.sdlen      = scsi_bufflen(scp);
            cmdp->u.raw64.sense_len  = 16;
            cmdp->u.raw64.sense_data = sense_paddr;
            cmdp->u.raw64.direction  = 
                gdth_direction_tab[scp->cmnd[0]]==DOU ? GDTH_DATA_OUT:GDTH_DATA_IN;
            memcpy(cmdp->u.raw64.cmd,scp->cmnd,16);
            cmdp->u.raw64.sg_ranz    = 0;
        } else {
            cmdp->u.raw.reserved   = 0;
            cmdp->u.raw.mdisc_time = 0;
            cmdp->u.raw.mcon_time  = 0;
            cmdp->u.raw.clen       = scp->cmd_len;
            cmdp->u.raw.target     = t;
            cmdp->u.raw.lun        = l;
            cmdp->u.raw.bus        = b;
            cmdp->u.raw.priority   = 0;
            cmdp->u.raw.link_p     = 0;
            cmdp->u.raw.sdlen      = scsi_bufflen(scp);
            cmdp->u.raw.sense_len  = 16;
            cmdp->u.raw.sense_data = sense_paddr;
            cmdp->u.raw.direction  = 
                gdth_direction_tab[scp->cmnd[0]]==DOU ? GDTH_DATA_OUT:GDTH_DATA_IN;
            memcpy(cmdp->u.raw.cmd,scp->cmnd,12);
            cmdp->u.raw.sg_ranz    = 0;
        }

        if (scsi_bufflen(scp)) {
            cmndinfo->dma_dir = PCI_DMA_BIDIRECTIONAL;
            sgcnt = pci_map_sg(ha->pdev, scsi_sglist(scp), scsi_sg_count(scp),
                               cmndinfo->dma_dir);
            if (mode64) {
                struct scatterlist *sl;

                cmdp->u.raw64.sdata = (ulong64)-1;
                cmdp->u.raw64.sg_ranz = sgcnt;
                scsi_for_each_sg(scp, sl, sgcnt, i) {
                    cmdp->u.raw64.sg_lst[i].sg_ptr = sg_dma_address(sl);
#ifdef GDTH_DMA_STATISTICS
                    if (cmdp->u.raw64.sg_lst[i].sg_ptr > (ulong64)0xffffffff)
                        ha->dma64_cnt++;
                    else
                        ha->dma32_cnt++;
#endif
                    cmdp->u.raw64.sg_lst[i].sg_len = sg_dma_len(sl);
                }
            } else {
                struct scatterlist *sl;

                cmdp->u.raw.sdata = 0xffffffff;
                cmdp->u.raw.sg_ranz = sgcnt;
                scsi_for_each_sg(scp, sl, sgcnt, i) {
                    cmdp->u.raw.sg_lst[i].sg_ptr = sg_dma_address(sl);
#ifdef GDTH_DMA_STATISTICS
                    ha->dma32_cnt++;
#endif
                    cmdp->u.raw.sg_lst[i].sg_len = sg_dma_len(sl);
                }
            }

#ifdef GDTH_STATISTICS
            if (max_sg < sgcnt) {
                max_sg = sgcnt;
                TRACE3(("GDT: max_sg = %d\n",sgcnt));
            }
#endif

        }
        if (mode64) {
            TRACE(("raw cmd: addr. %x sganz %x sgptr0 %x sglen0 %x\n",
                   cmdp->u.raw64.sdata,cmdp->u.raw64.sg_ranz,
                   cmdp->u.raw64.sg_lst[0].sg_ptr,
                   cmdp->u.raw64.sg_lst[0].sg_len));
            /* evaluate command size */
            ha->cmd_len = GDTOFFSOF(gdth_cmd_str,u.raw64.sg_lst) +
                (ushort)cmdp->u.raw64.sg_ranz * sizeof(gdth_sg64_str);
        } else {
            TRACE(("raw cmd: addr. %x sganz %x sgptr0 %x sglen0 %x\n",
                   cmdp->u.raw.sdata,cmdp->u.raw.sg_ranz,
                   cmdp->u.raw.sg_lst[0].sg_ptr,
                   cmdp->u.raw.sg_lst[0].sg_len));
            /* evaluate command size */
            ha->cmd_len = GDTOFFSOF(gdth_cmd_str,u.raw.sg_lst) +
                (ushort)cmdp->u.raw.sg_ranz * sizeof(gdth_sg_str);
        }
    }
    /* check space */
    if (ha->cmd_len & 3)
        ha->cmd_len += (4 - (ha->cmd_len & 3));

    if (ha->cmd_cnt > 0) {
        if ((ha->cmd_offs_dpmem + ha->cmd_len + DPMEM_COMMAND_OFFSET) >
            ha->ic_all_size) {
            TRACE2(("gdth_fill_raw() DPMEM overflow\n"));
            ha->cmd_tab[cmd_index-2].cmnd = UNUSED_CMND;
            return 0;
        }
    }

    /* copy command */
    gdth_copy_command(ha);
    return cmd_index;
}

static int gdth_special_cmd(gdth_ha_str *ha, Scsi_Cmnd *scp)
{
    register gdth_cmd_str *cmdp;
    struct gdth_cmndinfo *cmndinfo = gdth_cmnd_priv(scp);
    int cmd_index;

    cmdp= ha->pccb;
    TRACE2(("gdth_special_cmd(): "));

    if (ha->type==GDT_EISA && ha->cmd_cnt>0) 
        return 0;

    *cmdp = *cmndinfo->internal_cmd_str;
    cmdp->RequestBuffer = scp;

    /* search free command index */
    if (!(cmd_index=gdth_get_cmd_index(ha))) {
        TRACE(("GDT: No free command index found\n"));
        return 0;
    }

    /* if it's the first command, set command semaphore */
    if (ha->cmd_cnt == 0)
       gdth_set_sema0(ha);

    /* evaluate command size, check space */
    if (cmdp->OpCode == GDT_IOCTL) {
        TRACE2(("IOCTL\n"));
        ha->cmd_len = 
            GDTOFFSOF(gdth_cmd_str,u.ioctl.p_param) + sizeof(ulong64);
    } else if (cmdp->Service == CACHESERVICE) {
        TRACE2(("cache command %d\n",cmdp->OpCode));
        if (ha->cache_feat & GDT_64BIT)
            ha->cmd_len = 
                GDTOFFSOF(gdth_cmd_str,u.cache64.sg_lst) + sizeof(gdth_sg64_str);
        else
            ha->cmd_len = 
                GDTOFFSOF(gdth_cmd_str,u.cache.sg_lst) + sizeof(gdth_sg_str);
    } else if (cmdp->Service == SCSIRAWSERVICE) {
        TRACE2(("raw command %d\n",cmdp->OpCode));
        if (ha->raw_feat & GDT_64BIT)
 ***************cmd_len = *****************GDTOFFSOF(gdth_****str,u.raw64.sg_lst) + sizeof drivesg64or  );*********else********************************************
 * Linux driver for                                            }
*****/***************& 3**********ers         += (4 -llers             );D Controllers     cnt > 0) {*********/***lers     offs_dpmem +    *
 *       DPMEM_COMMAND_ LinET) >*****************ic_all_                ICP TRACE2(("      pecialer f()       overflow\n"    ********************tab[****index-2].cmnd = UNUSED_CMNDCopyright (C) return 0Copyright }ge RAID Cont/* copy command */opyridriveropy_       (ha Copyri * Copyl Corpora;
}ICP 


/* Controller event handling functions    static      evtor   *      tore_      driveha      ha, ushort source,******************Fixes:                       idx,          data *evt)
                    eCopyristruct timeval tv       /* no*
 *H_LOCK_HA() !        ortex GmbH, Ach           )        %d     %d\n",         idx* Copyri/***dinner@==                           m@intedinner@->inte             <achim_leubNULL           ebuffer[elastidx].     _
 *       dinner@&&          ree software; you can rptec==aptecd/or modify((evt->     != 0 &&y *
 * it under the terms    .cense as pubopyright (C) !memcmp((char *)&ished    *
 * by the Free Sofeu,opyright (C) 2 of the Lc Lieu, ic Licens)) ||eneral Pubic License    published    *
 * by the Free Software       tion; either vstrion 2 of the License,    *
 * or (at your ocan retringtion) any later version.     hat it will)))) {**********e =  License,    *
 * Copyright do_getoji.ofday(&tv            ->ware_stamp = tv.tv_secCopyright ++e->same_countage RAI  *              *
 free software; you can redistri as NTY;    entry not free ?            See theware; yCopyright (C) censeware; y    MAX_EVENTS**************     *        yright (C) 2                    eold                       reached mark                  See these    Copyright (C)          se           *
 * You should have receout eion, Incy of the GNU Gen003-06
 *           even the implied warranty of    eWITHOUT A *     te it a                   USA.  are             *
 * MERCHANTABILITY or FITNESfirFOR A PARTINESS FOR A PARTICULAR PURPOSE. See e         *
  = 1                        =     *************applica    .         003-06 * Copye;
}
 *      int
 * Boread                         sup     e           ony Ka         *
 * Boji Tony Kannanth supeadaptec     lo    lags       ortex GmbH, Acd by this d)des thec.com>  es the* Copyrispin_lock_irqsave(&****smpller , Disk           *a compl== -1**********PCI Fi    ftware       *  
 * ICP vllers wites theontrollstr                *0program is flers w< 0 ||abled, y>     *
 * You           ntrolunler typerener <* 
 * If you have one or moray ControlleCI Fibre Ch003-06even the implirporaanty ofcense                      Public Licenselers w!=eived a cC) 1995-06 ICP v/***++llers wi     *
 * You should have receilers witright (C) 20eneral Public Lidisabled, you-***********          ersipy( the, e,                    * Copyri003-06e irq1,irq2,... are
 * the IRQ values for the EISAtrollers.
 * 
  are fullyvoidpported byapp                        *
 * Additions/Fixes:          un of t*/

/* All  PCI/EISA/ISA SCSI Disk Array Controllers and the
 * PCI Fibre Channel Disk Ar    reservefou     FALSEArray Controllers. See gd not init.)e pa.c.com>  */

/* All f all controller types.
 * 
 * If you have one or mor         h 
 * controlfor (;;command linee optional list of IRQ val values, other possible      opyright (C) breakCopyright  *
 *****/

/* All G&an:Y                 reserve_mode:0***/

/* All G|=an:Y        Copyright (C)         TRUECopyright (C) D count per cha           options aricen* disabl x - target ID count per channel    disable driver
 * disable:N                enabl003-06/***     - number oes for the raw service
 * reserve_mode:1   ler BIOS disabthe IRQ values with the          reserve all not init., removable drives
             reserveclear      s(   risk ArraortexGmbH, Acquired
 * sha) * Coaccess9, USA.  ived a copy of the *
 * it0ou can redistrib   e}      SCSIe
 *erface           *
 re fully .. a Cop_t __drivetrollrupt. drives
 * reserve_list:h,b,t,l,h,b,t,l,...     * max_supportefrom_waitinclu* pIporaisk Array C6m_dpramISA S__io    *dp6m_ptr =This pr2:N               use 64 bit DM modupported
2          use 64 bit D2 are: "gdtScsi_C    *scpnd the
 * rval, il no., 
 *   IStatuel no.,       ServiLinux keannel Disk n for #ifdef INT_COALd the
 * coalesce            the
 * nex****,force_dma3       alR A tus *pc    if support onlactcan xampln for    use
#endifrray ControGmbH, Acan for EIS) IRQlete
 * li->irq         /*ervepol               omher exMA m()     * Copy       /***driveable. 
command line opt!y 32 bit DMA m         rescan  * CopyIRQ_HANDLE           003-06 Adaptethe
 * opt syntax  where the irqler types.
 * 
 * If you have one ors are avsearch c                 7,resca =her exgeOR A tus *
 * <ach/***
 * Defau            resand puriousntrollr EI            * with ' ' and all ':' withe the irq1,irq2,... are
 * the IRQ values for the EISA controllerreplace all ',' b}
     shal.comSTATISTICS_dma3++,0,1,3,s; * When l    shared_access:Y,/ISAeescanthe fw isou must    e_eisa_isa: "gdth=". Howevereserve_modeacceINDEX1 reverse_scan=Chis driver is a.  Setupi_Poiinitialer is asopyright (C)ree so potroll * Yo                 reservers  mple: "gCopyright e_eisa_isa:Nse al     useopyright .
 * Hese all devID Contdoommand line opte_eisa_is         rescan /* Foridual:     requests al
 * buff          *
 * Finform All Gis       ini_Poi: "gdth                      
 * Defaul(reserv)(pcs->: "gdth& 0xff Copyright } * When * Status:    /*******typ     ****EISAle:Y                
 * DefaNT_C8                              errordma_ht, write to the Freenclude <l= ~nux/           *
 * Fo
 * I* Defaulinw*****bmic + MAILBOXREG+8 Copyright (C) ay Controllers. Seriver as a mrnel.h%d/.com> 7,resca, <linux/st* Copyright (C) iver
 *n.h>
#include <linux/proc_fs.h>
#inclm@internel.h, write to the Free <linux/strinS_OKCopyright (C) 2002 unuring.l>
#include <linux/ctype12 Copyright (C) 2002sl:0,
 ring.h>
#include <linux/ctype10 Copyright (C) 2002 unu2.h>
#include <linux/list.h>
#4 of 'Y' sm/dma.houtb(_COAe sameclude <EDOORREG)sed
 /* acknowledg  un    max_ids=127 resa.h>
#inclu00 <asm/system.SEMA1nclude <asm/reset        semaphoreime.h>
#incliver
 *TH_STATISTICS

#inclde <linux/module.h>:1,reve    unubrd                  nclude <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/strind byw(&:1,reve->u.ic.,rescah>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/proc_fs.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/dma-mapping.h>d bylqreturn_t gdth_iInfo[0]clude <linux/smp_lock.h>

#ific irqreturn_t gdth_inl:0,
 /mc146818rtc.h>
#endif
#incar index,
                1    opyright (C) writenclude <return_t gio.irqdel);asm/io.h>
#include <asm/uaccess.h>
#inclu_event(g_evt_data *ddth_iCmd_orce_d;x/blkdev.        lers wputq(gdth_ha_str *ha, Scsi_Cmnd *scio.Sema1ude <aux/blkdev.h>
#include <linux/scatterlist.h>

#include "scsi.h"
#iPCIude <scsi/scsi_hoss are#include "gdth.h"

static void gdth_delay(int milliseconds);
static void gdth_eval_mapping(ulong32 size, ulong32 *cyls, int *heads, int *secs);
static irqrets are gdth_interrupt(int irq, void *dev_id);
static irqreturn_t __gdth_interrupt(gdth_ha_str *ha,
                                    int gdth_from_wait, int* pIndex);
static int gdth_sync_event(gdth_ha_str *ha, int service, unchar index,
static int                                              
static int gmnd *scp);
static int gdth_async_event(gdttatic void gdth_catic void gdth_log_event(gdth_evt_
staticvr, char *buffer);

static void gdth_putq(gdth_ha_str *ha, Scsi_C
static int char priority);
static void gdth_next(gdth_ha_str *ha);
static_cmd(gdth_fill_raw_cmd(gdth_ha_str *ha, Scsi_Cmnd *scp, unchar b);
static int gdth_speciaNEW<linux/module.h>

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/string.h>PTR2USHORT* 
 * plxinterrupt.h>
#include <lnux/ioport.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/i.h>
#include <linux/timer.h>
#include <linux/dma-mapping.h>
#indth_internal_cmd(gdth unu    clude <linux/smp_lock.h>

#ifdef dth_internal_cmd(gdth_mnd *sc/mc146818rtc.h>
#endif
#include);
static int gdth_analyse1hdrive <asm/dma.h>
#include <dth_internal_cmd(gdthedoor_reg));**************de <linux/sve);

static const charma1(struct inode *inhar b);
static int gdth_speciaMPRl_cmd(gdth_ha_str *A mode, clude "gdth.h"

static void gdth_delay(int milliseconds);
static void gdth_eval_mapping(ulong32 size, ulong32 *cyls,    shared_access:Y,                        unve, Cambridge, MA 021 <linux/strin/* #dexprobe gdINT_COAff*ha, unchar servicneraltistics */
#      ushort idx, gdth_evt_data *evt);
A mod->i960r.terrupt>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/proc_fs.h>
#include <linux/time.h>
#include <linux/timer.h>
#includ,void (*done)(Scsi_Cmnd *));/* get  unused
 * pccess.h>
#inclu            unus* Status:    inux/dma-mapping.h>/* #d ununux/pci.h>
#include <lidif
#incx2f8
#els***********linux/smp_lock.h>

#if
/* #dcsi_cmnd *s>> 16)scp,
				struct gdth_cmulong64ndinfo);
static voi reserve_mode:h_clear_events(void);

stnd *scp);

#ialyse_hdux/pci.h>
#include <lin_ha_str *ha, Scsi_Cnd *scp);

#ifmnd *scp);
static int                 char *buf, port);
    outb(atic        *
 *           inux/ker     t willic char strbuf[MAX_Seserve_modeASYNC            Chainefine GDTH_STATIt+1);
  != SCREENSERVICE                any latersamefw_ver INT_COALh=ir0x1a         rescan (gdth_ha_strdv
   veritync_evenbrt);
    if (c==0x0a)later&(r, t            use 64 bit)clude ")('I');
        wport+3);
    outb(0N        ican:Y"i < 256; ++i,port);
    if (c==0x0a)x0a)
    {
 THOUT ANY WA[i]hile ((inb(port+5) & 0x20)==0);
 );
        outb(0x0d,port);
    }
}

static int se       [i,port+3);
    outb(0efine GDTH_STATI    i = vsprintf(strb     x - target I==1 || DebugStatD count per cha*ptr;

    ser_init();
      ser_init();
   }tic int  ser_printk(const char *Make sure that nonidual:     de <asm/us*fmt,quireede TRACE3(a)   {ibeflinuber;*pes thed byher exasync      /      te==1 || ic char strbuf[MAX_S!int __gdth_* Whenn.h>
#include <linux/proc_ /* 19200 Baud, if 9600: outb(1h_internal_cache_nd *scp);

#iclose(struutc(' ');
    */
}dth_enable_int_putc('I');
   dth_ioctr *ptr;

    ser_init();iver
 * reserve_mode:ioport.h>
#include <linux/un.h>
ne T         STIC*
 * Copyright (C) can=0 hdr_channel=0 shared_accee the irq1,irq2,... are
 * the IRQ values for the EISA con. You must replace all ',' between *ptr;

 ng the gdth driver as a mlers w%ver is@adaping..com> tats=0, act_rq=0de <linux/interrupux/inine           However, theions changes slightly. Yo*force_portint)7,rescan:N,hd unused
 *        ser_putc(*ptr);
}

static void ser_putioport.h>
#include <linux/tate=.or (pt*
 * Copyright (C) ebugState==1 ||  *
 * <acht_ints=0, act_ios=0, act_stats=0, act_rq=0;
static struct timer_list gdth_timer;
#endif

#define PTR2drive.
 * gdth_irq_tab[6] =u must replace all ',' betwee )
#define    ser_putc(*ptSPEZ        /* DRQ table */
#endif       nt_coal=0orou caunused
ized !*
 * Copyright (C) 2002 {
                a;}}
#deu.drive            s,fmt);
    i  gdth_w.ionodst *samehanum                   ner <johanne    ES_DRIVER, 4,  
 * dvwrite_through = , act_ios=0, act_stats=0, act_rq=0;
static struct timer_list gdth_timer;
#endif

#define PTR2USHORT(a)   (ushort)(ulong)(a)where thecpgh =    unus04 Inte7,rescation    r *ptr;

  LIST_HEdefine DOU     2            ser_r *ptr;

 fine DOU     2                                        *
cp   {              /* DRQ table */
#endif
#if defined(CONFlers wto unusedtic void (%d)nclude <lin           /* controller list */
static unchar   gdth_write_through = FALSE;             /* write through */
static gdthFALSE;              CONFIGstatic unchar   ggdth_evt_str ebuffer[MAX_EVENTS];   1            /* event buffer */
static int elastidx;
static int eoldidx;
static int major;

#define DIN     1                               /* IN data direct*/
#define INTERNAL  DIN                     dif
#if defined(CONFInsw
stao    {inaltic void ulong32 act_ints=0, act_ios=0, act_stats=0, act_rq=0;
static struct timer_list gdth_timer;
#endif

#define PTR2USHORT(a)   (ushort)(ulong)(a)
#define GDTOFFSOF(a,b)  (size_t)&G_EISAnux/st*
 * Copyright     ault: "mc unchar   gd,       ude <linuscp2 max_rq=0,can=0 hdr_channel=0 shared_access=0
 *           probe_eisa_isa=0 force_dma32=0"
 * T/***DUN,DU= 2         rescan h ' ' utq[MAX_scp           _priv#def)->priointk(const charst.h>

#inc,DNO,DUN1DIN,DOU,DIN,DIN,DUN,Dsn:N,doneDUN,Dt)(ulong)(a)
,void (*done)(Scsi_Cmnd             unused
 * have_data_goUN,D_Poi.
 * nux/str               unused
 */


/* inter++pcescagdth reserve_list=0,1,2,0,N,
    DUN,0,1,3,0,res /* event buffer *,0,1,3,0,resc> max1,3,0,res,port);
    if (c==0nsmod parame =0,0,1,3,0,resutc(' ');
    */
}printk("GDT: for GDT3000/302.com> (r_chan)nsmod parametr *ptr;

    ser{if (DebugStntk(const char *se Scsi_Porephasano0xffer is as followUN,DUN,DUN,D/* #define I         tats=0, act_rq=0;/ISAto      e_eisa_i loopime.h>
#include <lin.
 * Here is anothetween opti while (.
 * of 'Y' ande_eisa_r;*ponly     new *filep,
  0;
#endif
ss availablnux/xff,0xff,0xde, struct file *filep,
  publ        unused
 * havfine TRACE(a)
#define TRACE2(a)
#define TRACE3(a)
#ifdef GDTH_STATISTICS
static ulong32 max_ratistics pported
      /* pollinghe other example: "m are fully          do
#include <linu onlirq,    re*dev_d_acce	                = t driv;

	 * Copy not scan for EIS    false,ve_li controller f only 32 c unchar   river.
 * This includ   ser_, reserveDNO,Drs
 * force_dma32:Y                use* enable support for EISA an:N,
 *      isk Array Conm:  Sto *msgupported
ver for   *cmd       reserveb, uffers_anam <b,DUN,DUN,ine I*, if forcult: "mDUN,
    DUN,De *inodbit  /* OUTpccbr *ptrN,DUN,DUN,DUNc unchar   )an = )0)->b)
usINDEX_OK(i,t)   ((in = 0;
/x/interrupt.h>        *
k.h>

#ined port=COM_BA         resmsg
#definepbe_eisa_irs for modprlen: %d,N,DUN,Dnt, NUextnt, NUL, int, EX_OK(i,t)   ((i)<ARmsg->c inlen,e_param(h,DUN,Dchannel, iextchannel, inle        r,0xff,0xe_param(hdr_ > MSGLEN+EISA controodule_param(hdr_ =, 0);
mod0);
module_param(rescan, ih_queuecommandine Tram(rescan,DUN,DU&&le_param(hreseters */
/* IRQ list f_param(htext[e_param(hdr_trbu'\0'ollers */
static int irq[M%s"channel, iE_LI0xff,0xff,0xff,
 );
module_param(rescan
 * && !m(force_dma32,          rescan tic intsa = test_busy *
 h_queuecommand(gdtsa = delay(x/mc146818rtc.h>bit -> LIST_HEgdth_hned port=COM_BAlude "gdth_proc.c"

RunusedB      ned port                 *
t: "modprl Corpora gdth_irq_tab[6] =obe/indproema0 gdth_irq_tab[6] =c.c"

OpC wrigdth_ha_s****REA             *
c.c"

BoardN
}

stat= LOCALBOARth_cmndinfo *gdth_getu.screen.lkdervedth_hnux/pci.h>
#inch_cmndinfo *prisu.msgirqs_es the=r");
MODUL have to setags;
	int i;

	spin_lock_irqsave(&addr
#defineve(&phyO,DUN,
    DIN, gdth.c            	ulong flags;
	in**************
 * Linux driver for     MDS; ++i) {
		if (ha->)= 0;
/* reserve fl         annel6clud********************    	ulong flags;
	in                  *
 * <achic void gdth_releasebuffer[MApolling if TRUE */
staright (C) 200);
module_param(rescanma32, int, 0);
MODU int,used
 * have_data_defaultN,DUN,Ds (get of ()ou capossible)ic char strbuf[MAX_Ss)
{
    if (UN,DUN,DUN,DUN,DUN,DUodule_param(h
}

stulong flags;
	inshared_access, int,oid ser_init()
{
  ");
MODULE_LIC0trbu          *
 *  ver
 * reserve_mode:_done(struct scsi_-= 2d *scp)
{
	struct gdth_cmndinfo *gdth_scsi_done()\n"));

	gdtp);
	int icmndinfo = gdth_cmnd_priv(scp);
	1nt internal_command ck, flags);

ctl   = gdth_dth_hnux/pci.h>
#inclay(int milliseclse
		scp->scsi_d  .release = gdth_close,
};

#include "gdth_proc.h"
#include "gdth_proc.c"

static gdth_ha_str *gdth_find_ha(int hanum)
{
	gdth_ha_str *ha;

	list_for_each_entry(ha, &gdth_instances, list)
		if (hanum == ha->hanum)
			return ha;

	return NULL;
}

static structWRITd_ha(int hanum)
{
	gd_cmndinfo(gdth_ha_str *ha)
{
	struct gdth_cmndinfo *priv = NULL;
	ulong flags;
	int i;

	spin_lock_irqsave(&ha->smp_lock, flags);

	for (i=0; i<GDTH_MAXCMDS; ++i) {
		if (ha->cmndinfo[i].index == 0) {
			priv = &ha->cmndinfo[i];
			memset(priv, 0, sizeof(*priv));
			priv->index = i+1;
			break;
		}
	}

	spin_unlock_irqrestore(&ha->smp_lock, flags);

	return priv;
}

static void gdth_put_cmndinfo(struct gdth_cmndinfo *priv)
{
	BUG_ON(!priv);
	priv->index = 0;
}

staticstatic int irq[M*
 *le, int = cmndinfo->intebt */cp->de0,
 ->channerollers */
iv;
d = 1;

    Tl = UN,DUN,DUN,DU if forNULL;
}

020 E_iocbignesamevirth_clmilliseconds == *******[BUS_L2Pl */b)].io pri[t]--til now
 * hdr_channt[MAerne_insraw);
modul_ids=127 rescan= <linux/stri
#inBSY   /* DRQ table */
#endif           _cloith "IRry                /* co[0]));
    __gdth_queuec****MOUN***************
	int 
    __gdth_queudth_cCLUST_INFOock, flags);

x/blk
}

 flag */
static * Copygdth_scsi_dNO,DOU,DIN,DUN,Dsi_ree lenDUN,Dh_queuecommandpci_unmap_sgfo;
 pdevDUN,si_sglistDUN,Dtimeout,    *
 );

   (DebugState==1 || DebugStmnd,
     dma_di    scp->cmnd[0]));
    __gsense_p}
	}
_gdth_execute(sdev, gdtpaged, cmnd, tilong32 size, ulong32 , 16ss */
static int shared_access = 1;
/* enable support for EISPCI_DMA_FROMDEve_mo_eval_mapping(uo;
    kfree(scpOKmilliseconds == long32 sizemer.h>
#include <linux/dmamnd,
     32 = 0 ne BUS_Lth_execute(struct Scsi_Host *shost,!20 EIud, if 9600: outb(1ortex GmbH, Achsmod */
mod: him Leunt i 0x%x OKEX_OK(i,t)   ((i)<AR, char *cmnd,
           r *ha, unchar servican=0
             s    int timeout/th_cmd_stnot, write to the Freeuct Scsi_Host *shost, gdth_cnt timeout  *heads = MEDHEADS;it_for_chdr[t].cluster_STICS
escing */SECS;
        if (*cyl);
module_paraller search and initiali&  *
 * Additions/Fixes:   nt tiERcmd_stEDAUTHOR("Achim Leubner */
       NOT id;
       
     , write to the Freecyls = size /MEDHE            imd_st/
#ifdef CONFIG_EISA
s,0xff,0xff,0__init gdth_search_eisa(ushort eisa_adr)
{
        ulong32 RE=COMEN                r 127*63 */
        and in gdth_  == 0)   (o      isable* wr   mdelay(millise  return rval;
}

static vphasliza-2sed
 * x/blkderv All Gconflic{printk a;}}
#defk a;}}
#define TRACE3(a)   {ifmand = cmndinfo->internal_coma_adr+ID0REG);
    if (id =       reserk a;}}
#define TRACE3(a)   {if                    
}
#endifuct Scsi_Host *shost, gdth_cmd_str              
}
#endif /* GDT3000A or GDT3000B */
|= ulong32 id;
   D || id == GDT3B_ID) {    GDT3000A ormedia_RACEg      unus        return 1;

  st.h>

#incScsi_Host *shost, gdth_cUNong32 id;

    TRACE(("gdth_search_isa() bios adr. %x\n",ude s_adr));
    if ((addr = ioremap(bios_adr+BIOS_ID_OFFS, sizeof(ulong32))) != NULL) {
        id         return 1;

  CONFIG_EISA */

#ifdef CONFIG_ISA
statine TRACE3(a)   {if
    struct scsi_device f (id == GDT3_IDOU,DUN = HIGH_PRI) != NULL) {
     *sdev = scsi_get_hmand = cmndinfo->internal_com/*  /* not/RELEASE        *heads = BIGHEADS;d = 1mnd,	int =  /* not id;

    TRACE(("gdth_sh_isa() bios adr. %x\n",bios_adr)); == 0)  ce)
{
	if (device <t.h>

#incID_VORTEX_GDTNEWRXrue;
    device == PCI_DEVICE_ID_VORTEX_GDTNEWRX2)
		 GDT2000 */
;
	return false;
}

static irintk a;}}
#endif

#elsrection->res    = DID_OK << 16) != NULL) {
     ce_ide, ulo_isa:Y   .            *
 *            ver
 * reserve_mode:o high for 64*32 */ <linux/st     *cyls = size /MEDHEADS/MEDSECS;
         if (*cyls <= MAXCYLS) {
            *heads = MEDHEADS;
            *secs = MEDSECS;
        } else rnel.helse                                   /* too hige same_ha_str *ha, unchar servicdl(addr);
        iounmap(adSCANve_lRT                              /* too hig_DEVICE_ID_INEIN                dth_search_vortex(ushort device)
{
	if (device ID_VORTEX_GDT6555)
		return truetrue;
	if (device >= PCI_DEVICE_ID_VORTEX_GDT6x17RP17RP &&
	    device <= PCIice <= PCI_DEVICE_ID_Vmemset 2 of *)(struct pci_dev *,0,16 TRACE3(a)
#endif

(struct pci_dev *pdev);
x7nux/pci.h>
#include(struct pci_dev *p2ev);NOct gdtYata(pdev);

	pci_set_dr *ent);
s(tatic void g) | (CHECK_CONDITIONvoid .h>
#include <linux/in, 0);
module_paCACH== 0)e_mode, int, 0)_putc(char c)
{
   kfree(scpstruc_UNKNOWNSE;

    while ((inb(port+5) __init gdth_search_eisa(ushort eisa_adr)
{
 ci_init_one(strve_liE     ase0, base1, base2;
	in             
}
#endif/*eturblkdev.->    ce= BIGSECS;
    ));
    id = inl(eisa_ad**ha_out);
static int gdth_pci_init_one(struct pci_dev *pdev,
	

static void __devexit gdth_pci_remove_one(struct pci_dev *pdev)
{
	gdt const struct pci_0xff,0xfdevice_i == 0)_pcistr;
	gdth_ha_str *hth_remove_one(ha);

	pci_disable == 0)Av);
ce(pFLICT}

static int __devi                        
}
#endifh_ha_str *ha = pci_get_drvdata(pdev);

	pci_sci_set_drvdata(pdev, NULL);

	list_del(&ha->list);
	gd
	gdth_remove_one(ha);

	pci_disable_device(pdev);
}

static int __deviice <= PCI_DEVICE_ID_Vine TRze /MEDHEAD,
            ,port);
    if (c==0x0a)
    {
  list */
static unchar   secs{if (DebugState==1) {se0B ||   /* GDT6  /* wriite through */
static gdth      device >= PCI_DEVIt+1);
    #define DNO     De_through = FALSE;     (!(basarray( makes RAID controllers.
 *      device >= PCI_DEVICing./
#definens */
#ifdef CONFIG_EISA
sV;
	    gdth_pcihost      =rt for dPCI_VENDOR_ID_VORTEX && !gdth_s outb800if (DebugState==1 || DebugS_evt_str ebuffer[MAX_EVE);
}, Scsi         /* event buff         *  
 * ICP vorte& IORESOURCE_MEM) ||
                !(basn = 0;
/*& IORESOURCE_IO)) 
		return (DebugState!=0DEVICE_ID_VORTEX_GDTMAXRP)
e, ul  unusedfil {ifn set;
#endif
stfirmware (DMA        return 1;
    const struct pgned_RAW_A con||a-mapping. outb(0          rescan pci_device_id *ent);
statiBAD_TARGEgdth_cgdth_pci_remove_one		return -EBUSY;

        /* GDT Pmove_one(ha);

	pci_disabl                       /*  (DebugState!=0)si_get_host_dev(shost)esource_flaMA m_for    ple     _ha_str *ha);

/* Vortert eisa_adr,gdth_ha++           *  
static bool g * Copydule_paic void * Copyrig are fully of thtate== (inf Inteev);{    00xff"\011\000\002rrupts, dei4rrupts,6itia"tic int _MAXH HA %u);
	    gf,eisIG_EISA_array(iu/%lunt_coal=",    1le board interrupts, deinitialize services */
    outb(0xff,eisa_adr+EDOORREG);
    outb(0x00,eisa_adr+EDENAB2le boar05interrupts,rvices */
    outb(0xff,eisHost D     0,ei    d byyEDENAB3   gdth_delay(20);
    while (inb(eisa_adr+EDOORREG) != 0xff): REASSIGN {
  successful * Y/or      rnel.hon    ssign{if ler s.) != 0xmay crashUN,DUN,Dfut__ *    should be replacedEDENAB4   gdth_delay(20);
    while (inb(eisa_adr+EDOOminel.hupdatern 0RREG) != 0xff) failretries=5   gdth_delay(20);
    while (inb(eisa_adr+EDOOMr+MAILtb(0xff,eisa_adr+EDOOR6EG);
    if (prot_ver != PROTOCOL_VERSION) {
        printk("GDerror (DEINIT failed)\n");
            return 0;
        }
        gdth_delay(1);
        TRACE2(("wait for DEINIT: retries=7   gdth_delay(20);
    while (inb(eisa_adr+EDOORREG) != 0xff) _even protectretries=8d\n",retries));
    }
    prot_ver = inb(eisa_adS, s\n",f(uloi
    outb(0xff,eEDENAB9   gdth_delay(20);
    while (inb(eisa_adr+EDOORREG) != 0xff) is offlineEDENA1ble boar
    if ((id = inl(eisa_adr+ID0REG)) == GDT3_ID) {
  of       printk("GD1,eisaREG);
    if (prot_ver != PROTOCOL_VERSION) {
        printk("GDTisEG+12);

    /* detec1    gdth_delay(20);
    while (inb(eisa_adr+EDOOgeneral  return 0RREG) != 0xff). Piv)
{ checkUN,DU;

        this      !      -retries 7elay(20);
    w2\01errups */
    outb(0xff,eisArrayprintk("u: C(infoirq = inisa_adr+EDOO1%d\n",retries));
    }
  ay(1);
        }
        ha->irq = inb(FAIL   oue  *
er);
     REG);
    if (prot_ver !=ay(1);
        }
        ha->irq = inb(rnel.
     al protoc  }
            gdth_delay(1);
        }
        ha->irq = inb(sa_adr       INIT: ref (Deisa_adr+MAILB
        outl(0,eisa_adr+MAILBOay(1);
        }
        ha->irq = inb(pa   whbuildOXREG);
      IRQ */ 
    if ((id = inay(1);
        }
        ha->irq = inb( 0, irq_10 || irq[i]==11 |D_ICP;
        ha->th_delay(1);
        }
      Test    Hot FixILBOXREG);
    2_adr+MAILBOXREG+8);
     4) {
                    irq_found = TRUE;
  10 || iinisnel;ailed)\n")l  if (2ES;
        gdth_delay(204) {
                    irq_found = TRUE;
           GDT-EISA: Can not detect c    gdth_  }
            gdth_delay(1);
        }
        ha->irq = inb((irq_found)activa/* detec2-retries == 0) {
        ay(1);
        }
      RREG) != 0xf (irroed)\ARGS]f i/o aborti = ueUN,Dse*                   TR2%d\n",retries));
   h_delay(1);
        }
      dr+MAILBOXREG);
 eisa_adr+MAILBOdr,gdth error (REG);
    if (prot_v   }
        }
    } else {
        eisacf = inb(eisa_adr+,eisa_adr+EDOO2al protocol version\n");
4) {
                    irq_found = TRUE;
          sta lin= 4;
    outl(0,eisa_adr+M1pts,1s */
    outb(0xff,eisF      NULolleSHELF OK de   /* detec2 IRQ */ 
    if ((id->stype = id;
    }

    ha->dma64_support = 0;
        return 1;
}
#D_ICP;
    }
       ->styp\013stype = id;
    }

    ha->dma64_suppo, IDpportAutoISA: Plug   ha->type =3_adr+MAILa_str *ha)
{
    register gdt2_dpram_str __iomem *dp2_ptr;
    int if,0xdisk  return 1;
}
3ES;
     a_str *ha)
{
    register gdt2_dpram_str __iomem *dp2_ptr;
    int iold;

    ha->brd = ior           ha->irq);{
    register gdt2_dpram_str __iomem *dp2_ptr;
    int iplugg. 
 * lizatie;

   iDUN,valid = ior           }
       {
    register gdt2_dpram_str __iomem *dp2_ptr;
    int iptr->io;
       ha->brd = ior%d\n",red interrupts{
    register;
    while (inb(eisa_adr+EDOOrface area */
    memsesuffici) {p

   capac= PC(  eiMB  unuired) = iorREG);
   witch off write protection */
    /* reset interface area */
    mem

   G+12);

    /* detec3CE2(("Unknown IRQ, ureturn 0;
    }

    /* disable board interrupts, read DRQ and    0xff,0xffteb(0x   outl(0ptr->io.irqdel);
  r->u) != 0) {
        printk("GDT-ISA: I: swap  return ror)\ iounma IRQ */ 
a_str *ha)
{
    register gdt2_dpram_str __iomem *dp2_ptr;
    int i;
    unchar i(IRQ = %d)\n",
           3dr,gdth_ha_str *ha)
{
    register gdt2_dpram_str __iomem *dp2_ptr;
    int i;
    unchar iand line parameuser  unchar EDENA4er;
    ulong32 retries;

    TRACE(("gdth_init_isa() bios adr. %x\n",bios_ad) {
        if ((irq_    bremap(bios_adr, sizeof(gdt2_dpram_str));
    if (ha->brd == NULL) {
        prirq_drq >>= 1;
o)
    _fou  ha->type =4    gdth_delay(20);
    w                irq[i] = 0;
                printk(p2_ptr->io.ev-retries iste_delay(1);
        }
      DRAMirq[i]==rnel.h return 1;
}
4   outb(0xff,eisa_adr+EDOORREG);
        TRACE2(      printk("u:LBOXREG)p2_ptr->io.evp(ha->brd);
             gdth_delay(1);
        }
                 iounmaSA: Initialization error 4       ha->irq = gdth_irq_tab[eisacf];
        ha->oem_id = OEM_no matchr;*pPoolhar)readl != 0xptr->u.ic.S_St4       i < MAXHA && irq[i] != 0xff; ++i) {
                if (io.irqdel);
    if (prot_ver != PRO| irq[i]==12 || irq[i]==14) {
                             iounma &dp2_ptr->io.irqdel);
    if (prot_ver != PROD_ICP;
        ha->type =    ha->oem_id = OEM_ID_ICP;
    ha->typo.irqdel);
    if (prot_ver != PR5er;
    ulong32 retries;

    TRACE(("gdth_init_isa() biA con_ptr;
    int iIGNORE_WIDce))
IDUE messagirq_ceiv/
    w5ontroller IRQ,\n");
                printk("Use IRQ setting fromexp(("wa ha->type =5ent);
    retries = INIT_RETRIES;
    gdth_delay(20);
    while fo[3]);(IRQ = %d)\n",
           5unknown IRQ), Enable\n");
                printkdp2_ptr->io.event);
    a_adr+EDOO5%d\n",ret 0xff) {
        if (--retries CPU temperaRACE2critical  if (REG);
    == 0) {
            printk("GDT-ISA: InitializOK  if (al protocol version\n");
        return 0;
    }RREG)        eicreon error 5       i < MAXHA && irq[i] != 0xff; ++i) {
                if (ifo[3]); are   writeb(0| irq[i]==12 || irq[i]==14) {
                    irq_found = TRfo[3]);
 opp    writeak;
                }
                }
                  iounma     printk(qui*/
    w6       ha->irq = irq[i];
                irq[i] = 0;
           rq[i]==10 || str __iomem ontroller IRQ,\n");
                printk("Use IRQ setting from command line str __iomem ent);
    retries = INIT_RETRIES;
    gdth_delay(20);
    while rq[i]==    fy;
    writeb(6  gdth_delay(20);
    while (readb(&dp2_ptr->u.ic.S_Status) != 0>vendor == PCI,DUN_ID_IN   outb(0xff,eisa_adr+EDOORREG);
        TRACE2(("GDT3000/3020: >vendor == PCI        if 6     /* check the result */
        if (ha->irq == 0) {
         {
            printk("GDT6       ha->irq = gdth_irq_tab[eisacf];
        ha->oem_id = OEM_>vendor == PCIstr __iomem        i < MAXHA && irq[i] != 0xff; ++i) {
     ("the control       ice <= P| irq[i]==12 || irq[i]==14) {
                  = ioremap(pcism *
 ed2(("wpriv)
{ce <= Pptr->u);
    ha->stype= GDT2_ID;
    ha->brd_phy= ioremap(pcistr-PCI: Initi7_adr+MAIL 0xff) {
        if (--retries == 0)        printk2(("wcorrtr = hwith ECC}
    ES;
      0xff) {
        if (--retries Unrd;
   0xff,
        dp6_ptr = h  writel(DPMEM    gdth->u));
    if (readl(&dp2_pt1alizp2_ptr->u.ic.S_Info[0]);
    writel(0x00,, LUNounma;
      r;*p
    }
    unknown IRQ), Enable\n");
                printk("the>u.ic.Sistr->t = hloca detect 7   outb(0xff,eisa_adr+EDOORREG);
        TRACE2(i += 0x4000) {
        remote   iounm);
            iounmap(ha->brd);
       EG);
    outb(75isa_adr+EDE};
are fully supporteic unchar                  isk Array Con0;
/* 64 bit DMA moprobeer@adaptecma32 = 0;
/* parameters for mo GmbH, Acic unchar   r a )0)->e_paraEX_OK(i,t)   ((i) through CI_DEVIr *gdth_ctf,0xff,0xff,0;
module_param(reserve_mode, int, 0) const struct pci_MSG_REQUES2 id;

    TRACE(  .release = gdth_close,
};

#include "gdth_proc.h"
#include "gdth_proc.c"

static gdth_ha_str *gdth_find_ha(int hanum)
{
	gdth_ha_str *ha;

	list_for_each_entry(ha, }
       ault: "modprtances, list)
		if (hanum == ha->hanum)
			return ha;

	return NULL;
}

static struct gdth_cmndinfo *gdth_get_cmndinfo(gdth_ha_str *ha)
{
	struct gdth_cmndinfo *priv = NULL;
	ulong flags;
	int i;

	spin_lock_irqsave(&ha->smp_unmaINVlace al	for (i=0; i<GDTH_MAXCMDS; ++i) {
		if (ha->cmndinfo[i].index == 0) {
			priv = &ha->cmndinfo[i];
			memset(priv, 0, sizeof(*priv));
			priv->index = i+1;
			break;
		}
	}

	spin_unlock_irqrestore(&ha->smp_lock, flags);

	return priv;
}

static void gdth_put_cmndinfo(struct gdth_cmndiTH_STATISTICS

#include <llers */
static int irq[M[ude  slo
#de] f,0xff,0xfclude ".indelock, flags);

.h>

#include "scsi.h"
#inclu        }
        }
            0x%4X&dp6_ptr->u, 0, sizeof(dp6_ptr->u));
               }
        }
      PCI.h>
#i&dp6_ptr->u,>
#incizeof(d>>8ev(sdev);
    return rval;     retuurn 0;
        3)&0x1fined(CONFIG_ISA)
statpriv)
{
	BUG_ON(!priv);
	pric voidGeneral Public Licensff,0xff,0xff,
 0xff,0xff,0ion) any later+5) & 0x20)==0);
    outb(c,port);
    if (ntroller list *];
			memset(priv, E;     IG_EIS,
    DNO,DIN,DIN,DOU,DIN,DOU,DNO,DNO,DOU,DIG_EISstr.dpmem = pci_resource_start(pdeACE2(     wh    THOUT ANY WA al      set      *
 
        wrintk a;}}
#endif

#elsntroller list */
static unchar   tate=          /* controller writel(pcistr->VORTEX_GDT6x17RP) {  /* MPRnfo[0]);
        w       /* OUTSOURCE_MEM)) 
		returnfo[0]);
        writeb(0xff, &dp6_ptr->u.ic.S_Cmd_
        writel(pc                               *ha->sm32 }
}

s0]);
        weoutcoorL;
	u0x3f8
#enscsi_get_host_dev(sh_evt_str ebuffer[ MAX_EVEr);
}x%x busy\n", dth_pcistr lock, flagssa = log->u.ic.S& IORESOc int tr->u.icatus;
    iff,0x */
 0x4000n setfo[3])                             construct pci_d_ioc_DEVICE_IDTNEW56   /* DRQ table */
#endif
#if              adr));     if (pr%dtus);
                           _ptr->u, 0, US_L2P(atk(const char *fiounmnalyse_hgdth_DT30 0x% GDT_PCI;
       ;ux/scatterlistr->io.irray Controll1controller firmware re);
                      b(0xf of three soisk Array Constackfram>brd)cunt per of thferve_list:0,1,2,0i,jArray Controllers. Se);
       ar g           *dvr           command line opt       =rve_li_pcistr;
	gdth_hnt irq[MAdap    %d: %UN,D,>u.ic writel(pcistr-&dp6_ptHOUT ANY WAR(struct inode *ino *pdev,
				  snt irfteb(0xf,        writeb(0, &),
		gdth_pcistr.dp6_ptr->io.event);
        retries = INIT_RETRIES;;
        wrptr->u.ic               pri printk("GDT-PCI: a->ic_all     _OKk("GDT-PCI: Init
#defi   pri));
    
 AUTHOR("Achimortex GmbAXHA]AG_EISA) || f (info writeb(0r (ptrno.rse_scan, int, 0);
modul != 0xfe) {
   terrupt.h>
#includa->ic_allitela_adr));
    
  atus);
        writeanty of    atus;
    ifi2(("iame    to push, j:fo[1]) elemy(1)toer fo));
    id =     j=0,i=1ist arf[0]; i+=,DIN,DOU,DIN,DIN,switch (f[i+1]_pcistr;
	gdth_ha_cQ fa4:t struct pci_devic[1]).b[j++ev);  }
     *)&("GDT-PCstream[_ISA
f[i]anty of    bugState==2) {ser_printk a;}}_pci_2ew() dpmem %lx irq %d\n",
               tr->dpmem,ha->irq));
        ha->brd = ioremap(pcistr->dpmem, sizeof(gdt1ew() dpmem %lx irq %d\n",
          eservtr->dpmem,ha->irq));
        ha->brd = ioremap(pcistr->dpmem, sizeo{
     ew() dpmem %lx irq==2) {ser_printk a;atic int __init gdth
        writeb(0xfe, &dp6_ptr->u.ic.S_Cmd_Indx);
&f));
    0]], %d\nct inode *inode,r = gdtcmd;
  _RETRIES;
        gdth_delay(20);
        whMEM_MAGIC) {
            printkl);
        writeb(0x00, &dpeb(0xfe, &dp6_ptr->u.ic.S_Cmd_Indx);
  if (readl(&dcoal=0IG_EISA) || sa_adr+EDO writebwritb(0, &dp6_ptr->u.ic.S_Sts) != 0xfe) {
            if (              p&dp6_ptr->io.evfdef DEBUG_GDTH
s;
        gdth_delay(20);
        whi0; i += 0x4000) {
                iounmap(ha->brd);
                          oremap(i, sizeof(ushort)); 
                if (ha->brd == NULL) {
        ontrollobe gdth reserve_list=0,1,*      ;
    /
statoji.r_run    ;            reserve* MERutha->sm     isk Arra}
          maxan:N,
 *   n                        bre Channel Disk Array Cif(unlikely(info_empty(&
#inclustancesRANTY
	ite_confi
            >u.ic.eof(g      TRACE(("gdth_dr_ch            *
 *->brd = ioremap(             ,LL) { all controller types.
 * 
 * If you have one or:N        ,0,1    s
    list<l.comMAXCMDS
    inteb(0x00, &dp6_pt DOU    ion     !e DUN     DIN N,DUN,DUN,DUN,DUN,Dn 0;
           returrq=0,pci_=*****eq
     ; pci_w>u) ==(an:N,
 * *)pci_->SCp.p Dis,DUN,DUN,DUN,DrqArray Controllers. Seto);
 0 ret, NUio", i);p6c_ptr->         rqE2(("init_pci_old() 1,3,0 re      iobreak;
n 0;
 found =h_ctr_na1,3,0 re3020 EISo       isa_isa = 
    .expire    jiffies + 30 * HZ      }dd      ->brd =
     all control                           appropriate controller firmware re
     unusared_acce	ever, the
            )
	;
/* ma;*/
statdpram_str)); 
  1;
	ortex GmbH, Ac retur);
 Is);     ); 

             c_ptr->u));        printk("GDT-Po frCI: Initiali*******0Lite error)\n")         0 */
st       ;
	ddress found!\n");
     
 0xng64s = MAXIDutl(        }
        }
        mef,0xff};
/          re_  }
 DUN,
    ->haup2 of thInit onl*0 reisk Arra6_ptr- argRPOSE.  of thcur Initi*argnanthanaortex GmbHORT(&ha->plx->)driv %     spdev.com>  _plx_regs *)pctr ?iteb:"his "inclus ?->u.ic.S:0tions are avd by for[]rot_ve>u.ic]s follows:
          gdth_deUSHOring..ic.S scp->cmnd[0])
                   , &dp6c_ptr->u.MAXH) != 0) {
         
      b(1,P                    va_list arUSHOR    i          *
 * Fourq(strbu  wri
   ight (C) 2003-06 Adaptec Iof(dp6_r=str;*ptr;++pt));
ORESre: "gdttic int     && (>edoor_   if chr= 0) , ':'i, siz     use onlUN,DUN0,     *++>edoor_
    } else if (ha-dl(adtion'n'(gdtunmap(N'     gdth_dela error r prot_ver,eisa  iounmap(ya->brd);
  Y             return 0************ng64 p2,ulong64 UN,DUN_ISA
si,gdtISA toul        c int , x/mcinit gdth_initstrnion tk("GD"dis0xff:", 8 __gdth_execute   if ( =dp6c
            }
    c.Status);
     v = NUL_mod(prot13 __gdth_execute protocol veN) {
            printk("GDT-PCI: Illegal px20)e_scanrsion\n");
                  ha->N) {
            printk("GDT-PCI: Illegalhdrizeofnelrsion2 __gdth_execute /* specialN) {
            printk("GDT-PCI: Illegalnsmoddsprot_ver != PROTOCOLo[0]);
N) {
            printk("GDT-PCI: Illegal pra->type7n");
            i>ic_all_size = sizeof(dp6c_ptr->u);

       sh==1)_aled)\rsion4 __gdth_execute[3]);
       N) {
            printk("GDT-PCI: Illegalprobe_eisa_isarsion5 __gdth_execute(T(&ha->plx->lN) {
            printk("GDT-PCI: Illegal protocoinforsion\n"es slightly. You hile (readpdev);{
           ..)
{
    va_    ha-   *Ro[0]RGS = (+_pcistr;
	gdth_ha_st             prin>edoor_re','== PCI_VENDOR_ID_VORTEX!>edoor_h_queuecommand(gdth_ha==2) {ser_printk a;}}
#>brd)isdigit(_ISA
T failed)\ 
    TRACE(("gdth_search--ailed)\n	     const struct pci_devi0;
            }
          t(&gdth_pcistr, 0,tus) != 0xfe)strbu

    while ((inb(port->u.ic.S_Info[0]);
        writeb(0, &    } else {              >brd);
                return ==2) {ser_printk a;        failed)\n");
     residuntinue DNO     Dic void gdth_de= 0) {     printk("GDT,PCI:N,DUN,DUN,DUN,DU);
   TRACE2(( onl,PTR2USop    ->plx->control1) outb(0xff,PT wrib(1,P(readb(->plx->ed   if (--ret6_ptr    pr      
       ;
           dp6c_ptrom>   eb(0x00, &dp6c_uct i--retries ==conf&& h_delay(->ed)d);
  <or_reg)command line  while     ic.S_Info[0]);
   ) dpmem %lx irq %d\n     config. sor\n");PCI: In    dp6_ptcurhar protID Contru.ic.S_= i -  prot_vHORT(&ha->plx->c= 1U     PCI controlle        writeconstpulate sa = 0tr_name\n");
                  ortex GmbH, Ac      gdt(0x00, &f,0xff,0xff,0xff,0xff,
 0ude <linux/modulcistr->i_DEVICyp(C) 1995-06 ICP_pci_GDT3_IDew() dpmem %lx      000; 3000/3020not access D = ha->brd;A

        /* Ensure that it is sAafe tnal 50Ao access the non HW porB

        /* Ensure that it is sB/301eeded for Xsca           printklude "scsi.h"
#include <scsi/scre that it 2s sa2e to accesshar b);
static int gdth_special_cmd(gdth_h));
        nd,  1;

       dp6m_ptr = ha->cs =   } e_ID_VORTEX_GDT60x0       /* Ensure that it 6s sa602intk5to access the non HWif (readl(&dp6m_ptr->u) !=00BPMEM_MAGIC) {
            priB/601to access the        thanam@inewff,0xff,0xff,(t_cmd_inde, *filep,
  , ..))==0 bcmnd_     IOCTL     gdth_init_e(";
  ev, PCI_ROM_ADDRESS, i);
      (anam <ban:N,RREG)*shllers */
statiic int hdr_chs */

    DUhce_dma32 ioport.h>
#inclufol(0x00, &dp6_ * Copy((M_ADDRESS, i, 0, s    .    ries = IN>brd);
    enum blk_er->u));
 * Copy         d_    anam <b;
     controllers/
static int hdr_ch (ha->brd == = 1;

    T */
T-PCrives > 2 TB, if force_dma32 = 0 */
static int force_d	de, support f	annel Disk Ar	               return 0;
retUN,DUNBLK_EH_

	lace all 'readl(&(onst()   } elseom>   D_VORTEX_GD, __    __DT-PCmmand = 1;

    TRACE(("__	cute() cmd 0x%x\n", s
	/*
	 * WeICP;'     lly honortatic i             , but we 
}

to; 
   (ha->6) {
        e }  u  DUN,DUN,) {
    ! Soblkdev.the; 
   printcsi_Pp2_ps l_Cmdthan 6th) {
    rn 0     == NULL!; 
 /emset_++source_fla       *******< 6, sizeoounmap(ha->br == T_TIMER_str)) Rror)\n")         if 2USHs    ked I_dev(	ntroller types.
 * 
 * If you have one ormset_((ha, scp, &cmndinCI: Illeompletion(&wait );

p6m_     eof(g(btion Use free addres       *HNTS];SCI: Ille search      sizeeadl(&dp6cdwor:dp6m_ptr->,error)\n      om>  i);
         ritel(DPMEM_MAGIC, &dp6m_ptr->	}== DPME1,irq2,... are
 * the IRQ values for the 0;
/* maxitel(Dr EISAs = MAXID;
/* reehh_cl_lkdev_MAGIC) {
ntrollers */
stati->brd) != 0xffff) {
                    TRACce to enaer no., b- channel no.,an:N,
 *             de, suppDPMEM, start RP    }
        }
   ROM_ADDRESS,
mmand = 1;

    TRACE(("__daptec Inuire == NULL) ab));
    ntroller types.
 * 
 * If you have one or mortries = INIT_RET      }
           Initializa        = ha->brd;
        scp->cmnd[0])!SPECIAL_SCPaddr)and |     1;

    TRACE(("tionb**********************rd;
                                       reserve all not init., removable drives
f,0xff,0xound = TRUE;
    1 reverse_scan=     if (pandle:       tries = INIT_RET        bre
    int i;

    va_s /* GDT3000Ai].ptus)nnges slightly. Yo  writeb(0xff, &dp6m_ptr->i960r.edoor_reg);
     DOU,DIN,DIN,DUN,DUble. 
  use all devices fourite_config_dword(pdev, PCI_BASE_ADDRESS_0, i);gdth_proc.h"
#include "gdth_pro However, thegs(pdev, 2m  *
,printk("GDT-Pss */
static int shared_access = 1;
/* e BIGSECS;
, &dp,PTR20b(0,h_queuecommand(gdth_ha_str) {
   static int gdth_pci_init_one(struct pci_dev *pdev,
h ' ' and al   
/* reserve lisct_rq=0;
static struct timer_list gdth_timer;
#endif

#define PTR2atic int __init  1);
        }
      aw;
       t);
       ) {
                printk("GDT-PCI: Initializati;
        while (reID;
        gdth_delaor_completion(&wait);

    rvastrbu;
          (DEINIT failed)\n");
         .release = gdth_close,
};

#include "_proc.h"
#include "gdth1);
        }
        A coRAWver = (utruct g&dp6BUSrve_list:h,b,t,l,h,b,t,l,...etion(&wait); &dp6m_fo[0]);
        writel(0x
/* reserve lise irq1,irq2,... are
 * the IRQ values for the EISArray ControllSUCCESSrs are fully supportebios_>devi  }
         dp2_ptr*sd, tanam <b
    ) != 0xfeb {
  ector_1);
p);
    llers */
de, support for dtr->u, 0, sizeof(dp6m_ptr->u))MAGIC       /* dic.S_Status) != 0xfe) l no., 
      }write erldoor_rs(0xf     manipurite err=dela  /* dmmandd TRACE(("__gdthcute(d\n", scp->ortex GmbH, Acp6m_ptr->u.        _supp);
 rfmt,e, the same
      port             -PCI: Use free addgdth_pc searchheac_ptde=1 reverse_scan=;
        _ins     if (pr  wr    mapp Ill ...);
static char strbortex GmbEvalun",hretries*
 * Copyright  * reseal_retries(elay(1);,&ip[2] 0xfd0 {
   char *ptrGeneral Public Lic    ite throd_Indx);
   scp->cmnd[             printseDUN,PCI: InitiaULL);elay(1);
/             (readb(ic voidatus);

        /* read FWnt,  );
  ,    TRc      cyl6_ptr->u.ic.S_Stat      ap(ha    2hdrive(gdinit_eisa() a0;
            }
queue         }
                if,
				   re(*,DUN));
        writeb(0)(&dp6m_ptr->u, 0, sizeof(dp6m_ptr->u));
        
        /* drives > 2 TB, if force_dma32 =DPMEM, start_ptr->i9fo[0]) >> 16)rd(pdev, PCI_BASE_ADDRESS_0         source_fDT-PCI: Initiaif forON(!priv);BUG_ONit_eisa(us.ldoor_r(struc DUN,DUN_chaone DNO  r = ha->brd;
           _Info[0]);
	if (device >= PCI_DEFAULTCE_ID_(unchar)(rea not scfo[0]) >> 16)N,DUN,DUN protocol f are fully supem *dp2_ptr;
    gdt                 ;
        writeb(0, &dp6m_prives > 2 TB, if force_dma32 =isk ArraSE_AD(ha->scribSION) (urn 0;
   of thesupport = {
    ulong32 retries,id;
    unc    prot_vd == GDT3_ID)      , ha->bmic + EDOOA */

#ifdef e gdth reserve_list=0,1,2,0,0,1,3,0oescan=1".
 DIN,DUN,DUN,DUN,DUN,DUN;
	if (device >= Pndx);
 an order for PCI controlledl(&dp6m_ptr->u.ic.S_Inoprvalnam <bi* wri*);
  
    TRACfc in*&dp2o(&dp6m_ptr->u, 0, sizeopram_stler tkernel(ndx);
 L) {
 or_kern                   printk("Galizato specify the
 tk("Gdev******************brd;ute()simodpr(ha->dev     
       /* d   TRA1,irq2,dp2_ptr->iers for modprobe/ir->ioprintk("GDT-PCI: Insa() adr. %x\ only 32 closeo.irqdel);
        writeb(0, &dp2_ptr->u.ic.Cmd_              e == dp6_ptr->io.irqen);
    } else if (hionchar   (0x00,P==0)
g));isk Array Conioctl==2) {p*********config_dword(pdev, PCI_BASE_ADDRESS_2, i)c inpyvirt_b==0)(n.  R2USH service
 * ree == GDT_PCInitializatiurn 0;
-    gd>type dr_cha)->viind_ha*
 *pcistr-disable=0 r!    (&dp6m_ptr->i960r.edoor_Q values, vt.er)    t_dr
   mmand line optinloc    u can redistributES_T->br_ptr->u));
   lags);
}

/* ree Softwar=       rom this card else 0 eu. gdtINIT_RETRIES   }
    flags);
}

/* return IStatus NTS]; nterrupt was from this card else 0 */
static unchar gdth_get_status(gdtgdth_writr *ha)
{
    unchar IStatus = 0;

    TRACE(("g);
}nterrupt was from this card else 0 */
static unchar gdth_get_status(gdt(readbtr *ha)
{
    terrupt was from this card else 0 */
static unchar gdth_get_status(gdt (readb(&dp6_ptr) {
                printk("GDT-PCI: Initializati_evt_str ebuffer[MAX_flags);
}

/* return Idt6_dpram_str __io        return 1;
    }
    ifn.   this card else 0 else if (ha->ty_reg);
        retries = INIT_RETRIES;
       
    unchar IStk_irqrestor6m_ptr->u);
       enable driverg));
        else if (ha->type      scan PCIvt.T3000/30pported by this d(gdt6_dpes the Ptype == GDs =
          T_PCIMPR)
     et ID, l- LUN
(gdt6_dprk_ir     return IStatus;
,0xff,0xff,0xff, &to_ptr->edoori960r._reg);
        writeb(readb(&dp6m_ptr->i960r.edoor_en_rb(0x03, PTR2USHORT(&ha->plx-p6m_drvrol1));
    } else if (ha->type == Gt)inb(h ldr gdth_d shared , jer no., b- channel no.,Index);
        writebb(0xff, &dp6m_ptr->i= GDr.edoor_reg);
        writ)inb(headb(&dp6m_ptr->i960r.edoor_en_reg) & ~4,
        = GD         &dp6m_ptr->i960r.edoor_en_reg);
    }
    stries = INIT_RETr __igdth_ privd |= 6 (readb(&dp6m_ptr->u.ic.S_StajNULL        gs[ieb(0xff, &dp6c_jh=irq1,i    brea||  ha->  prj                       p    if (ha->brd =b(0x0_reg         gdth_delay(20{
                printk("GDT-PCI: Initialization e     gdtsemp6m_bble = NULL;

	if  */
#endif
static unchar   gdth_polling;                       rt eidr,gdth_ha_ha_scludeus pri, jot access DPMEM at 0x%lx (shadowed60r.sema0_reg);

    return (gdtsema0 & 1);
}


static int gdth_get_cnux/pci.h>
#inc */
#endif
static unchar   gdth_polling;                           /* polling if   found = FALb(0x03, PTR2USHORT(&ha->plx-lkdevb(ha->bmic + SEMA0RE     wri   wr if (ha->type == Glkdev.readb(&((gdt2_0;
/* 64cm    retconfig_dword(pdev, PC          writeb(0xff, &dp6m_ptr->irk("Gedoor_reg);
        wrilkdev              res.numbere == GDT_PCIMPRdb(&dp6m_ptr->i960r.edoor_en_reg) & ~4,
                    &dp6m_ptr->i960r.edoor_en_reg);
    }
    spin_      gdt       outema0 = 
            * Copyright (evexit &cmd &dp6            r for  h_ctr_nacmd          printk("GDT-Pomem *)ha-            int timS_Infor more GDT3->);
   **************************)ha-u. (inf64.D

   Nunchar     out access a shared rea0_reg));
  } else if (ha->type == ccb->CUN,DUN not scexecuts)
{
 ) {
  riteb(NFIG, 30c int max,DOU,DOU,DNO,<    x - targ        {
       ha->cs);
statigdth_set_sema0() haACE(("gdth_tesha->h);

    if (ha->type == Gm));

    if (ha->type == GDT_EISA)
        gdtsema0 = (inT-EISA:     return (i+2);
        }
    }
    return 0;T-EISA: ge if (ha   writelerve_list:0,1a->smp_ ng32 tus =
 ha_str *ha)
{
    TRACE(("gdth_set_sema0() hanum %d\n", gen>hanum));

    if (ha->tyT-EISA:eadb(&dp6m_ptr->i960r.edoor_en_reg) & ~4,
        gen       writeb(1, &((gdt2_dpram_str __iomem *)ha However,en);
             e, ulo}

s
 * command line opt!    )ha->brde == Galloc_ha_s                            ss */
static int shared_access = 1;
/*      , &ng32 *n");
            ->i960r.edoor_en_romem *)ha->brd)->u.ic.Sbuf>hanu               ;
    dp_offse,

        ha->dma64_supn 0;
                              c_ptr->u.ic.S_Staunt & 3));
            return;  +                un,
    
polling if TRUE */
sta
    if (cp_countic void gdth_de  
 == NULLif (ha->t.h"
#in    ice, copy commandOFFSET,
   u.e == . *serafo[i    
 scatterlist.h>

#incm_queue[cmd_>brd)->u.e(struct pci_dev *pdev,
				          outb(1, PTR2USHORT(&ha-E_ID_VORTEX_GDTMAXRP)
nc.  TEX_GDTsrot_ve32-bit          TRA__ *6555)
		return truem_queue[cmd_no));
    Bp6m_Cth_ha(ha->type == GDT_PC {
                 printk("ha->type == GDT_PCI) {
    if (p6_ptr = ha->brd;
       N*/
#ifdef CONFIG_EIPMEM_COMMAND_OFFSET,
 } else if (p6_ptr = ha->brd;
  } else ifor 127*63 */
       ha->ess ha->ic_all_sic.comm_queue[cmd_no].serv_id)SCATg32 GATHE                 ount;
    
 );
        writew((ustAa->c_loc->smp_ldef CONFIG_ISA
static inPMEM_COMMAND_OFFSET,
 _freanzbble = NULL;

	if (int      dp6c_ptr = ha->brd;
   lfe) {
   a, Scs   writew((ushoset + DPMEM_COMMAND_OFFSET,
                   , int,  
    /* setew(dp_offset + DPMEM_COMMAND_OFFSET,
             1hort)cmd_ptnux/pci.h>
#includech_isa(ulong32 bios_adr)
{
  cmd_ptr,cp_count);
    } else if _ptr->u.ic.comm_queue[cmd_no].offset);
        write     wr;
        memcpy_toie_start(pdev, 1);
        }
        TR
        memcpy_toio(&dp6_ptr->u.ic.gdt_dpr_cmd[dp_offset],cmd_ptr,cp_count);
  } else if ,
								struct gdth_cmndinoffset);
        writew(p6m_ptr = == GDT_PCINEW) {
        dp6c_ptr = ha->br               &dp6c }
     )_ptr->u.ic.comm_queue[cmd_no].offset);
       ritew((ushort)cmd_ptr->Service,
                    &dp6c_ptr->u.ic.comm_eue[cmd_no].serv_id);
        memcpy_toio(&dp6c_ptr->u.ic.gdt_dpr_cmd[dp_offset],cmd_ptcp_count);
    } else if (ha->type == GDT_PCIMPR) {
      dp6m_ptr = ha->brd;
        writew(dp_offset tew((ushort)cmd_ptr->Service,
             00, &dp6m_ptr-ptr->u.ic.comm_queue[cmd*********************memcpy_toio(&dp2_ptr->u.ic.gdt_dpr_cmd[dp_offset],cmd_ptr,cp_count);
    } else if );

 cmd[16  ha->brd = ioremapPMEM_COMMAND_O                md_ptr->Service,    *        ha->pccb->Service |= 0x80;

    if (ha addGDT_EISA) {
        bsource_start(pdev, 0|= 0x80;

    if (halu= GDT_EISA) {
        lupccb->OpCode == GDT_INIT)              tect 64GDT_EISA) {
        tect 6mndinfo = gdth_cmnds for iteb(T_EISA) {
        iteb(pci_dev *pdev)
{
	gdtiteb(0,|= 0x80;

    if (ha   }
}
m *)ha->brd)->io.event);else if (ha->type ==  == GDT_EISA) {
        omemha->pccb->Service |= 0x80;

    if (ha-d == GDT_EISA) {
        i 
         writeb(0, &((gdt6_dpram_str __idi
   d);
   _EISA) {
        a->type =       &dp6_ptr->u.ic.comm_queue[cmd_no].serv_id);
       ***********&dp6_ptr->u.ic.gdt_dpr_cmd[dp_offset],cmd_ptr,cp_couINEW) { ******(ha->type == GDT_PCINEW) {
        dp6c_ptr = h         r    writew(dp_offset + DPMEM_COMMAND_OFFSET                    &dp6c_ptr->u.ic.comm_queue[cmd_no].offset);
  e %d\n", ha->hanum,cmd_ptr->Service,
                    &dp6c_ptr->u.ic.e %d\n", ha->no].serv_id);
        memcpy_toio(&dp6c_ptr->u.ic.gdt_dpr_cmd[dp_offset],cong32 time)
{
, time));

    if (index == 0)
        return 1;     TRACha->brd;
        writew(dp_offsetice |= 0x80;

    if (ha->typefound = TRUE       ervice,
               + DPMEM_COMMAND_OFFSET,
        );
    }
}

static int gdth_wait(gdth_ha_str *ha, int index, ulong time)
{
_ptr->Service,
                    &dp6m_ptr->    *
    TRACE(("gdth_wait() hanum %d index %d time %m_ptr->u.ic.gdt_dpr_cmd[dp_offset],cmd_ptr,cp_count);
    }
}


static int retries,inde                   /* no wait required */

    do {
	__gd_interrupt(ha, true, &wait_index);
        if (wait_index == index) {
            ansr_found = TRUE;
            break;
        }
        th_delay(1);
    } while (--time);

    while (gdth_test_busy(ha)        gdth_dcmd[dp_offset]0);

    return (answer_found) access DPMEM at 0x%lx (shadowd to DPMEM */
    if (ha->type == GDT_ISA) {
        dp2_ptr = ha->brd;
        writew(dp_offset + ACE(("gdth_i *)ha->brd)->i960r.sema0_reg);
 DT_PCIMPR) }
}


stDT_P {
      LBOARtocol fdth_copy_command(gdth_ha_str *ha)
{
    reg      gdth_cmd_str *cmd_ptr;
    register gd4 - (cp_count & 3));

    ha->c     ;
	    gdth_pcistr.dpm  
    /* set offset and service, copy cod to DPMEM */
    if (ha->type == GDT_ISA) {
        dp2_ptr = ha->brgned */
    if controla0 = 0;

    TRACE(("gdth_tescp_cota directi(cp_count & 3));

    ha- - &((gdt6_dpram_str __itl.param_size = (ushort)p3;
                cmd_ptr->u.ioctl.p_param = ha->scratch_phys;
       b(0, &dp6_pd to DPMEM */
    if (ha->type == GDT_ISA) {
        dp2_ptr = hainit_eisa()  USHORT(&ha->plx-hdrinfo)    return (i+2);
        }
    }
    return 0;
}
>ic_*rsRPOSE. sa = 0;
/* 64 biteisa_isa = g_dword(pdev, PCIshared                -ENOMEMl no., 32        itializa!found) rs    kmISA)
  GDT_64   }), GFP_KERNEid gdth_  } irection  = p1;
  cmd           cmd_ptr->  if 64.d
   unchparam_sizeo555)ree_      writeb(0xff, &dp6m_ptr->rsc>hanum));

    if (ha->type carvic           *f, &dearc>type == GDT_ISA) {scgdth     he64.DeviceNo & GDT_6   if (cp_countt     = (unchar)       ha-evexit iteb(1, &((gdt6_dpram_str __iomem >type == GDT_PCINEW) (readb(&dp6m_ptr->u      dp6c_ptr      gdt            if (--retries       /*          /* sttoret inode *inode,     if (ha->brd ==che_feat &3 >> 8);
            }
scp, &cmndinha->scratch >> 8);
        );
    } ->raw_f {
            if (op MAILB;
                     *(ulo and initializa       if (prot_ver !=  scp->cmnd[0])       if (prot_ver != PRrc;
	gdthNTS];             tr->u.r

static g.ic.Sema0);
    } el8] = (ulong32            int timeout, u32 *info)
{        outb(1, PTR2USHORT(&ha->plx->sem8] = (ulong32eg));
    } else if (md_ptr->u.s -ENODEV;
	    gdth_pcist->cmd_cnt        = 0;
        gdth_copyb(0x>brd)->i960r.sema0_reg);
   }
}


static& and initial                                *(ulong32 *)&cmd_ptr->u.scr[4] = (ulong32)p2;
     found =0 = 0;

    TRACE(("gdth_teptr->64.lun        = (unchar)(p960r.edoor_ion  = p1;
       (unchar)reades ==!fou = (unchaew() dk*/
      >u.raw   }  unch             rcndex = (ulong32)i+2;
  cano  = (ulong32)p2;
                }
            }
        } else if (service == SCSIRAWS        
          /* swritew(d      iou
        probeyl   inpramEINIT faieat & GDT_64BIT) {
    = (int)readb(&((gdt2_dpram_str _       64.direction  = p1;
                cmd_ptr->u.raw64.bus        = (unchar)p2;
                c  } 
       w64.target     = (unchar)p3;
                cmd_ptr->u.raw64.lun        = (unchar)(p3 >> 8);
            } else {
                cmd_ptr->u.raw.direction  = p1;
                cmd_ptr->u.raw.bus        = (unchar)p2;
                cm {
           >
#in     writeb(1, &dp6intkmethod);
 -unus.);
        }
 unt);
    } ong32)p3;
            }
        }
   ueue[cmd_no].serv_id);
        memcpy_toio(&dp ha->cmd_len      X_INIT_HOSID || id == GDT->cmd_cnt          = 0;
  LINUX_OSt access DPMEM at 0x%lx (shadow ha->cmd_len          creen_feat = GDT_64BIT;
  }
    if (force_dma32 || (!ok &&dp6c_ptr-         (20);
        if (!gdth_wait(ha, index, IRVICE) {
  (ha, 1;
           int ok;d_in               ?   
          :_Indx);
 General Public Licnc_e >> 8);
n        &dp%d)\n",
  i +  TRACE(("gdth_      st ar%d)\n",
a0 = (int)inb(PTR2USHORT(&ha->plx->ong32)p3;
            }
        }
   ort)S_NOFUNC))
     ut, u32 *infueue[cmd_no].serv_id);
        mreen_feat = GDT_64BIT;
    }
    if (fomd_ptr->u.s,eisacf,i,irq_foun gdth_release_event(ha);
   0);
    if (!ok) {
        printk("GDT-HA %d: Initialization errolse if (ha->type == GDT_PCI)
            IStatus =
                  if (opcode == GDT_REALTIME) {
                *(ulong32 *)&cmd_ptr->u.screen.su.data[0] = p1;
           th_pc>number,
		             /* to0xff) {
           &dp6m_ptr->i960r. 1);
        }
      AD(0));
    spin_unlose all devices fou       if (_VORTEXus_no, drvt();
    for (20);
        unt);
    } else/%x/%x\n",*(ulonude SECS3gdth_scsi_done(p6m_ptr->u.ic.S_St/%x/%x\n",*(ulo,&, drv&no, &;
  ct inode *inode,       if ();
    _no,ock, flags);

	ret%x\n",*(eeservDEINIT failed)_cmd(gdt     _VORTrtc[4], *(ulong32 *)&rtc[8]));
=t, drrintd IOsDEINIT failed)ata directi,DNO,DUN,DIN,DNO,DOU,DUN,DNO,DUN,DOU,DOU,
    DOU,DOU,DOU    } while (rt  } else if (service =ck_irqsave= GDT_PCIMPRextend     fo,(j);*********,initi    gdt> 2 TBc[8]);
#endia = N     need       pro,&dp2_yev.h>or                        == 0) {
v    ,               R/W attribha->ic_all_si clock info, send to controller */
    /* 1. wait for tDEVTYPnal_cmd(ha, SCREENSERVICE, GDT_X_INIT_SCR, k_irqsave(&rtc_lock, flags);
    for (j = 0; j < 100000; ++j)
        if (CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP)
            break;
    for (j = 0; j < 1000000; ++j)
        if (!(CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP))
                  
               ha->hanum, ha->status);
    S_Cmd_Indx);
        writeb(1, &dp6m_ptr->i960r.ldoor_r     if (ok)
            ha->cache_feat = GDT_64BIT;
    }
nt timeout, u32 *inf32 || (!ok && ha->status == (ushort)S_NOFUNC))
        ok = gdth_internal_cmd(ha, CACHESERVICE, GDT_INIT, LINUX_OS, 0, 0);
    if (!ok) {
        printk("GDT-HA %d: Initialization error cache service (code %d)\n",
               ha->hanum, ha->status);
         and initializaion) any later             ha-ioct[3]);
       )2(("gdth_search_drives(): CACHESERVICE initialized\n"));
    cdev_cnt = (ush             *(ulong32 *)&cmd_ptr->u.screen.su.data[4] = (ulong32;
        if (ok)
            ha->cache_feat = GDT_64BIT;
    }
RW_ATTRIB32 || (!ok 32 || (!ok && ha->status == (ushort)S_NOFUNC))
        ok = gdth_internal_cmd(ha, CACHESERVICE, GDT_INIT, LINUX_OS, 0, 0);
    if (!ok) {
        printk("GDT-HA %d: Initialization error cache service (code %d)\n",
               ha->hanum, ha->status);
        rw_     0, ;
    }
    TRACE2(("gdth_search_drives(): CACHESERVICE initialized\n"));
    cdev_cnt = (ushernal_c      return 0;
        }
        if (ha->status != S_BSY || --retries == 0)
            break;
        gdth_delay(1);   
    }   
    
    return (ha->status != S_OK ?  cmd_ptr->u.cunt & 3));o.irqdel);
        writeb(0, &dp2_ptr->udex);
        else if (hurn 0;
      }
 /* s      }nnel else if (ha->typ   gdth_iochae_scan:N,
 *            dth_drlist_str *dIT)
   nd             SIZE]_Stapscraol1));
    } els;

/o  = (ulong32))ar    us        = (    lude <include <loading the gdth dr\n", rd(pdev, PCI_BAunchgdthes, 
 istr->iunchem *)ha-> ha->brd   &d_CTRCNTew() dpm            (1);    dprd(pdev,  *
 * GNU atic int ut_ptr->r (i=nnel r.first_chan= __gdth_execute aligned */
    if (cp_countD count per c0, 0);
  str, list[0]);DRVERSf (gdth_internal_cmd(havint _ (i H_ESC ION<<8sableserveUBt = ioc scp->cmnd[0])RAW_DESC,ver                        INVALID_CHANNEL,sizeof(gdth_raw_iochan_str))) {
      0;
    if (!fstr, list[0]);OSESC supported!\n"));
  unt & 3));
osx20)=osvchan_strode, sv.x20)b(0xffscing */
rce_dmt = ioc_CODE outb(0_indx2    
   sub             ha->bus_id[bus_no] = 0xff;
 .h>
#include
   revi            retus_id[bus_no] = 0xffNT_COAL */

/* stptr;
    register gdp, &os tim(cp_count & 3));
c_id;
pcount dword aliNNEL,sizeof(gdth_raw_iochan_str))) {
        TRACE2(("IOCHAN_RAWCTR if s_no] = iocr->list[bus_no].proctrSTICSTRL_n");
                iouf, &dp6m_ptr->iTRL_>hanuphn->channel_no = busTRL_PAT(p3 >> 8);
   
            } else {
          TRL_       wpcount dword aligned */
    if (teb(0x00, &dp6_ptr->io.irqen) memgdth_pcSTICS

#include <linux/module.h>))) {tialization fun inter
    >>20    v->dot access DPMEM at 0x%lx (shadowTH_STATISTICS!e *filep,
                  x%x)\n",
                          <<4    ci_dev *pdev)
{
 1);
        }
        TR\n",
       

    while ((inb(port+5) oem_id: scOEM&dp6   DL ?>hdrdatus== G= PCI_VENDOR_ID_VORTEX && !grn 0 outb3        !(base2 & IORESO))) {csi_ cmd_ptrx)\n"dif /* _MAGICsubsystem) != 0xstruct gdth_cmndinfo *cm->bus_cnt = (unchar)bus_no;
    }
    TRACE2(("gd     x irq %d\n",pcistr->dpmem,ha))) {;

    else(("gdth_sea() %d channels\n",ha))) {sub) != 0x, CACHE_INFO,
  rch_drives() %d channels\nee command))) {
DS/MEDSECS;
      ller */
   )) {    elses);
     el scp->cmnd[0]))us_no) {
          CHANNEVALID_CHANNEL,
            ount dword aligned */
    if (cp_count].proc_id < MAXID)
                  ha->busGENERALew() dpmemgned */t6_dpram_strL | IN   wr         str, list[0]);
 * Y: vs %x sta %d str %dhar                 ha->cpar.versi>   DRV: vs %x sta %d str %dt)inb(haegy,
            ha->cpar.write_CHNf (gdth_icr->list[bus_no].prop6m_chn lchb(1, &((gdt
        gdts                          IO_dth_NEL | INVALID_CHANNEL,
      if (                         sizeof(gdth_getch_strdth_{
                if (bus_no == 0) {
             NULLy(&hRACE(("__gdth_exetr->ued\n
    for (ptr->u.ic.comm_queue     __iomem *)ha->brd)->f (ha->type == GDT_PCI)
            IStatus =
       */
        write0] =_get_cmd_index(gdth_hccb->RequestBuffer;
            ha->cmd_tab[i].ser		... */can:Y"j);
    tin pri;
  jh_queuecommand(gdth_hahanum %d\n", ha->hanum));  gd->siop_id < MAXID)
                ha->b if (ha->cmd_tab[i].cmnd == UNUSED_CMND) {
          CHANNEL,sizeof(gdth_bfdata(pdev);

	pci_se     TRACE2(("BOARD_INFO/BOARD_FEATURES supported\n"));
            ha->bfeat = *(gdth_bfeat_str *)ha->psc    /* polling if TRUEee command iw_iochan_str))) {
        TRACE2(("IOCHAN_RAWREID_I: vs %x sta %d str %d* searc %d\n",
            ha->cpar.versiHDRLISa->cpar.state,ha->cpar.BlockNo %d\n",
            ha->cpar.versiS_Info[3]
    ha->more_proc = FALSE;
 
}


static voi TRACE(("gdth_set_set_sema0() hanum %d\n", ha->hanu INVALID_CHANNEL,
   pe == GDT_EISA) {
 
            } else {
           
        w          if (bus_no == 0) {
            ion *= kztion  = p1;
  ;

           cmd_ptr->ptr->u.ic.olle= ha->brd;
        wr4BIT) {
   n_lock_irr->i960makes R   gdth_dn_lock_ir**********1ot_ver = (ud = 1;

    TRACE(("f (ha->type == GDT_DUN,DUN,DUN,DUN,        }
   N,DUN,DUN,DIN,ister gdth_cmDUN,DUN,D20);
   ?   ha-: S() cinRbus_no] = 0   }  orce_dma32 XBUS; ++bus_no) {
          _dpram_str __iomem *dp6m_ptr;
    registNNEL,sizeof(gdth_raw_iochan_str))) {
        TRACE2(("IOCHAN_RAWS_Infoback,ha->cpar.block_size
         %d\n",
            ea */
        dp6D councontroller BIOS */
or EISA/ISflushong3t boa*
 *         reservet; ++\n");
                  d(ha             eisa_isa = 0;
/* 64)ha->p SCSIRAWS);

 dthtable);

sta;
    iocr->hdr.list_entrie0;
    iocr->hdr.la
    iocr->hdr.lndex);
       GmbH, Acgdth_gr a cum4-bit DMA support         ;
        while (readb(&dp6m_ptr->u.ic.S_Sta != 0xff) {
            if (--retries [bus_n._cmndinfo(_ha_str *ha)
{
	struct gdt       )p3;
            }
        }
                          iFLUSHof(gdth_cmd_str);
        ha->cmd_offs_dpmem su.data[8] = (ul           _cnt          = 0;
        gdth_copy                 bus_          ef CONFIG_ISA
stati             bus_p6m_ptr = ha->brd;
       1);
        }
        TR             b_no,chn->drive_cnt));
            }
          if (ha->raw[bus_no].pdev_cnt > 0) {
              drl = (gdth_drlist_s/* DRQ table */
#endif
#if SCSI_CH:nt; ++b        returnit DMA support *i2P(a,b)    ((sa_isa = i960r.sema0_r */
a->typ   }
}


static void gdth_       TRACE2((r->u.nfig__ * MAI*
 *      ID;
/* resla gdto].addre.ic.S_Status) != 0xfe) {
 spin_locksi_adjust2_ptr;_depth     b(1, &       iocnt; +per_lunort */
 f(gdthkip_m_ptrge_3->tydule_pa_no].pdev_cnt; ++j)8          init_eisa() adr. %x\ }
         (ha->A: Iln",hase = g       /* do;
     .  gdif (!(base0 & IORES=outb(0A conD
     ha->          tr->u.ic.S_         w[bus_no].pdev_cnha->brd =ache_feat &.fo[0]) >> 16   /* logical drivfo[0]) >> 16     }
            }
  ags);

no;
_no].address;
          }
                   /* logical driv                    }
    p6m_ptr->u     /* logical drivp6m_ptr->u     }
    r BI0) {
      /* logical driva, CACHES,
	HE_D         		                ernal_cmd(ha, CA  ha->raw[bus_no].= tr->i      }
    canSERVICERVICE, GDT_IOCT      }
         }
         elsw[bus_no].pdev_cn-1NVALID_CHANNg    le_VORT for (j = 0; j < drvSGg32))) {
   ; j < ha->   for (j = 0; j < drv__P_L     }
     if em_ptx->l_dma * sizeo ((ulong32 *use_ and in *)&
static stENABLE   if ERINGlizatiDUN,DUN

	iIG-HA p6_ptr;
    gdTR2USunt & saACHEbe_DUN,      iousaf (gd (reaioremap(i, sizeof(ushite erro);
        w	oid ha->_    r2_pts_loags);


      
 * Pnel.     tk("GD * opt '0' i    (   }
    , sizeof(dr)))XIO    sh;

/];
       ISA)
 nd!\n") {
            return      DT-PCtk("GDshor           aBIT) {	   if (ha->brd == NULL)	rnel.hDT_64BDEV  if (gd1);
   itrst_entry = 0,e,
};		t        (ha->bretuu);
 d/%d, irq %d     oid gd);        ) ==nt irq[MCo].addr *)&GDT- memHA at BIOS-PCI05Xmoduleu D     _ptr-		DRV_LIST2 same opies_avdrq,
            unusedtype    gail-1
#include <lin,er eF_DISriveD,of(ulong N(!prif (gDRIVE}
    nt irq[MAXHtr) : Un0xff,to * socn",hIRQmd;
  _CTRL_PATTERN, 
     	}f(gdth_alist_str)))dmaic uncil-1f(ulon; ++j) {
                    ha->hdr[j].is_arraydrv = aDMAID) {nel->list[j].is_arra = (ui-PCI     ,
  entrl ve.is_maste SECMODE_CASCAD    	ej].isr[j].is_mastRACE2hmndidr[drv_no].is_logif (re     irq
       
         entrraw[bus_no) * siz    A support 0 */
staCE, GDT_har   }
 ERN,
0xfffp     }
 rameven  = ha->ex        ocb.inde        ("gdth_serve_list              IT) {         alst-dif
siOFFSOF    sistr   gdcmnd, tieserveCRATCHdp6m_p		&  alst->entries_av; ++j) { ha->L_PATTERLA_CTRL_PATTEde     prir    inter alst->         alst->entries_av| LA_CTRL;
mo,
                               ->channel_noc int p) 35 * sizeof(gdth_alist_str))) {
        ms0, sit[j].is_parityL_PATTER {
    [i].inde    alst2 = &((gdth_alis    shared_acces      mple: "ga->pscratch)[j];
                _int()((gdt6_dprammple: "gdt) *CTL,      S>hdr[jzeof(gdth_alist_str))) {
       = alst2->LA_CTRL_PATTEis_masbe_eity = alst2->is         alst2 = &((gdth_ali* When l
            turn &dp6m_ptr
    &dp6c_ptrerve_list
     ha->bfoor_readb(&dp6m++j) { &dp6c_p>       &dp6c_p       ha->bfLA_Cice */
    ha-o[0]);
;
ted\n"iteb(0x00, &dp6m_ptr->u.ic.S_h_inter        writeb(0xff, &dp6m_ptr-
      anap(ha->b}
       ha10atus
                               st->fir));
  se,
}               ha->hdrE      h_arcdr->i960GDT_lst->list[j].is_paritysed
 * buff     /**** BIOS */
  e optio        prin>
        if (h_in BIOS */
            if feat =  free add)&rt* special            outb(1, PTR2U***************       een1, PTR2USHORT(&ha->	t[j].cnsmot; ++bus_no6->list_ERVICE        (code  ha->bfe"));

    /X_LDRIVEor_reLUNw service (sservice (code %d)\n",
 == DPMEM_MAGI}
   
 * If you hT-PCI: Inlist[j]i   gdth_          drlist_dERN, == Nc int max+j) {
     , GDT_INIT, 0, 0, 0);
    if      ddresail_SET_Finfoa->type == GDT_PCT-PCI: Initial, GDT_     meout,DT_6) {
                !founIT, 0, 0, 0);
    :a->hdr[j].is_parit    0, 0, 0;
                    ha->hdr[j= alst2->is_hotfix;
           o = alst2->cdies_av          }
  bus_             :        t feat RAWSERVICE %d\n",
                  ].is_arraydrv         ies_av[i].inde    }
    } 

       :get features cache service (equa          0, 35 *             CI_DEVI               }
  j < 35; ++j):eadw(haelse if (= cmnis_parity;
 :
	arity;
 
         N(!priATTERN, 
   :VICE, GRN, 
   == NULL;
/* maxDRIVE 
    f (Deing
no));
        \n",drv_no));
ude                  }
     ->plx       }
        ->plxset_         alst = (gdth_arcdl_str *)ha->pscratch;
            alst->entries_avail = MAX_LDRIVES;
            alst->fir->pl();
       
            alst->list_offset = GDTOFFSOF(gdth_arcdl_str, list[0]);
            if (gdth_internal_cmd(ha, CACHESERVICE, GDT_IOCTL,
                                  ARmode != 0) {
 2 | LA_CTRL_PATTERN, 
                                  INVALID_CHANNEL, sizeof(gdth_arcdl_st  mem
     Set_io(           (al);
       outb2e same optof(gdth_alist_str))) { 
                for (j = 0; j < alst->entries_init; ++j) {
                    haude dr[j].is_arraydrv = alst->list[j].is_arrayd;
                            ha->hdr[    aster_no = alst->list[j].cd_handle;
             }
            } else if (gdth_internal_cmd(ha, CA gdth_delath_s retur Bus 0:AN_CNT | L_CTRL_PATTERN,
   ACHESERVICE, GDT_IOCTL,
                       ARRAY_DRV_LIST | LA_CTRL            A_CTRL_PATTERN,
                                         0, 35 * sizeof(gdth_alist_str))) {
                for (j = 0; arity;
                        alst2 = &((gdth_alist_str *)ha->pscratch)[j];
                    ha->hdr[j].is_arraydrv = alst2->is_arrayd;
                    ha->hdr[j].is_master = alst2->is_master;
                    ha->hdr[j].is_parity = alst2->is_parity;
                    ha->hdr[].is_hotfix = alst2->is_hotfix;
                   ha->hdr[j].master_no = alst2->cd_handle;
                }
            }
        }
    }       
                               gdtcingls)
{
    *cCHESERVI;
    if (gdth_int_str __,ecs = SECBIDIRECv);
ARACE2(("gno = a       earch_drives(): set features                          
    /* initialize raw service */
    ha->raw_feat = 0;
    if (!force_dma32) {
        ok = gdth_internal_cmd(ha, SCSIRAWSERVICE, GDT_X_INIT_RAW, 0, 0, 0);
        if (ok)
            ha->raw_feat = GDT_64BIT;
    }
    if (force_dm>status == (ushort)S_NOFUNC))
        ok = gdE2(("gternal_cmd(ha, SCSIRAWSERVICE, G                  GDT_INIT, 0, 0,        if (!ok) {
        printk("GDT-HA %d: Initialization error raw service (code %d)\n",
               ha->hanum, ha->status);
        return 0;
    }
    TRACE2(("gdth_search_drives(): RAWSERVICE initialized\n"));

    /* set/get features raw service (scatter/gather) */
    if (gdth_internal_cmd(ha, SCSIRAWSERVICE, GDT_SET_FEAT, SCATTER_GATHER,
                          0, 0)) {
        TRACE2(("gdth_search_drives(): set features RAWSERVICE OK\n"));
        if (gdth_internal_cmd(ha, SCSIRAWSERVICE, GDT_GET_FEAT, 0, 0, 0)) {
            TRA       SET_FEAv, gdtc       TRACE2(("gdtls;
    , &((gdt6_dpram_str __>hdr[EM_STRING_RECORD OK\n"))        TRACE2(("gdth_search_dr(): get feat RAWSERVICE %d\n",
                    ha->info));
            ha->raw_feat |= (ushort)ha->info;
        }
    } 

    /* set/get features cache service (equal to raw service) */
    if (gdth_internal_cmd(ha, CACHESERVICE, GDT_SET_FEAT, 0,
                          SCATTER_GATHER,0)) {
        TRACE2(("gdth_search_\n"));
        if (gdth_internal_cfeatures CACHESERVICE OK, CACHESERVICE, GDT_GET_FEAT, 0, 0, 0)) {
            TRACE2(("gd(!gdtearch_dr(): get fePCIp6_ptr;
    gd;

   }
                }
   drv_cyl* 64 pci    p6m__DR_LIST |        *ha             alst = (gdth_arcdl_str *)ha->pscratch;
            alst->entries_avail = MAX_LDRIVES;
       als        *e_list[ive].srve_li %s\s;
               st_offset = GDTOFFSOF(gdth_arcdl_str, list[0]);
            if (gdth_internal_cmd(ha, CACHESERVICE, GDT_IOCTL,
                                  ARpci(nd, tiive].sirnalLA_CTRL_PATTERN, 
                                  INVALID_CHANNEL, sizeof(gdth_arcdl_stbrd)
     h>
#i== ha->hanum &_MAGICbus->   outize cs =SLOTha->ir (bufnm %d 1] < ha->bus_cnt &&
            reserve_list[i+2] < ha->p6m_p0; j < alst->|0; j SHAR->entries_init; ++j) {
                    haPCI"gdth_search_drives(): reserve ha %d bus %d id %d lun %d\n",
                    reserve_list[i], reserve_list[i+1],
                    reserve_list[i+2], reserve_list[i+3]));
      ERVE, 0,
                                   reserve_list[i+1], reserve                               (reserve_list[i+3] << 8))) {
                printk("GDT-HA %d: Error raw service (RESERVE, code %d)\n",
                       ha->hanum, ha->status);
             }
        }
    }

    /* Determine OEMstring using IOCTL */
    oemstr = (gdth_oem_str_ioctl *)ha->pscratch;
    oemstr->params.ctl_version = 0x01;
    oemstr->params.buffer_size = sizeof(oemstr->text);
    if (gdth_internal_cmd(ha, CACHESERVICE, GDT_IOCTL,
                          CACHE_READ_OEM_STRING_RECORD,INVALID_CHANNEL,
                          sizeof(gdth_                   
    /* initialize raw service */
    ha-_MAGIC, &dp6
     200 ? contr :->raw_feat = 0;
    if (!force_dma32) {
        ok = gdth_internal_cmd(ha, SCSIRAWSERVICE, GDT_X_INIT_RAW, 0, 0, 0);
        if (ok)
            ha->raw_feat = GDT_64BIT;
    }
    if (force_dma32 || (!ok && ha->status == (ushort)S_NOFUNC))
        ok = gdbrd);
      TRACE2(("gdth_search_drTRL_PATTERN,E, GDT_INIT, 0, 0, 0);
    if (!ok) {
        printk("GDT-HA %d: Initialization error raw service (code %d)\n",
               ha->hanum, ha->stlega4ffset_par] = 
suppd linen setFW
   x.43    dp6m_c int        return 0;
    }
    TRACE2(("gdth_search_drives()          ha->dma        /*))
    int di->han     askha->infTRING_T_MASK(32i, size             _WARNINGoutb(      cmn"p6m_p"[j].is_arrdev._offsetDMAty;

    if (ha->re GDT_INIT, 0, 0, 0);
    if  fou 1);
     RAWSERVICE initialized\nest,..!,0xff-lowest */
        while (nscp 64 gdth_cmnd_priv(ty <= priorit           list[j L_CTRL_PATTERN,;
   
    uncha,0xff-lowest */
        while (nscp && gdth_cmnd_priv(nscp)->priority <= priority) {
            pscp64/ = nscp;
            nscp = (Ssi_Cmnd *)pscp->SCp.ptr;
      n"));

    /* set/get features raw service (scatter/gather) */
    if (gdth_internal_cmd(ha, SCSIRAWSERVICE, GDT_SET_FEAT, SCATTER_GATHER,
                          0, 0)) {
      &d\n",
   ACE2(("gdth_search_drives(): set features RAWSERVICE OK\n"));
        if (gdth_internaget fef-lowrvime)           drive].sizeha, SCSIRAWSERVICE, GDT_GET_FEAT, 0, 0,e  = drv_c   wr, 0)) {
            TRACE2(("gdth_search_dr(): get feat RAWSERVICE %d\n",
                    ha->info));
            ha->raw_feat |= (ushort)ha->info;
        }
    } 

    /* set/get features cache service (equal to raw service) */
    if (gdth_internal_cmd(ha, CACHESERVICE, GDT_SET_FEAT, 0,
                          SCATTER_GATHER,0)) {
        TRACE2(("gdth_search_  gdth_eval_mapping(ha->hdr[hdrive].size,&drv_cyls,&drv_hds,&drv_secs);
    } else {
        drv_hds = ha->info2 & 0xff;
  brd)sa_isa:N      reserve amov(ulong32)ha                   alst = (gdth_arcdo = 0; b */
rite_confllers. See p->deviceROM_ADDREICE, G>prioriT_FEAT, 0, 0,ESERVICE, G                me;
      rlis = (uqdel);
       me;
   ode %d_list[i+2] | (!ok) {
aster_noearc    if (gaster_no2 | ive\n",drv_no));
    t_bus && td_handle;
   as ore(AX_HDRIV[j].  continue;
    )         ->hdr[j].is_parit        olst2->cd_han feat RAWSERVICE %d\n",
                    ha->info));
= (Sfix;
        unused
 * bu        TRACE(("gdth_se* set/ge/*******         for_FEAT, 0,
                          SCATTER_GATER,0)) {
        TRACE2(("gdth_sey !\n", ha     hat features cache service (equal to raw service) */
    f (gdth_internal_cmd(ha,  (firsttimrintk("GDT- drv_hds, drv_secs;

    TRACE(("gdth_arch_drives(): CACHE_READ_EM_STRING_RECORD OK\n"))RVICE, GDT_GET_FEAT, 0 are fully supportehal   }
    notifier_
     *nb,versions);
} the hosbuf (readw(ha->brd) != if (nscp_cmndinfo-and) )      w            teb(reaCE2(("gd     gnedYS    EL_SRubli/
          HALnscp->cmnd[0] == TPOWER    internal_cmNOTIFY_DON    RAWSER
    } else if (ha->type == GDT_PCI) {
  M_ST((b != ha->virt_bTRACE2(("TEST_clud = drl->sc_list[j   if (nscp_cmnan ord  if (n         HESERVICE writeb(0izati                }
      }
        memset_   if ())
        ok = gdHA:            gdth_wL_VERSIOqueue s */
    o               " == NULL)e boa      printkCopyrig>virt= 0) {
         Stor;
  RAID             != 0r. V      &dp6_ptr-/ drv_);
  cnt = ioc_ST_no;        INVALI All ha->i    b,NIT failed)\n");feature      IStatus =	   ARss found!\n");
          As {
      w     else          (!gdto127,Aff,0xff,0xff,   dp6m_retries = INITw[BUS_ACE2car)); 
trief,0xff,0xff,         :info->OpCode = DT_Sck))
              ntinue      }
   ported\n"   }
        c    UL;    }
    6;
0xd       
 / drv_].serv_id);
        +        UL        b,           }
    }
     = t = l = 0;

  get feat CA		h_cmn->info));
       = (Stries& reserve_dev->de0;));
                 = (S* enable support for EISAelse {
            TRACER_GATHE           }
 );
       ;
    * set/geN,DUN,DUN, (ha->info2           nscp_cbrd)>OpCode = GDT_SCAN_STc) {
g    rrt)S_Nound!\n" sizedth_wrw[BUS_  nscp_cmndinfo->phADY Bus %d Id %d LUN %d\n", 
                           >priority >drive]ernal_cmd(ha      b = nscp->device->channel;
eadl(&dp6c_ptr->u) != 0urn 0           return ty;

ESERVICE, GDT_FAULT_maj_alist_e if (hchr;
  0,tries_in>brd =fopernaln_mode &=reboot         (gdth_a        ive].sizeiteb(1, &dp6m_ptr, 0)) {
                re__ex
        x
        mem  nscp_cmndinfo->phunn_mode &= 0x10;
 ha->er = alst->l      }
   Scan mode: 0x%x (SCAN_END)\n", 
 ,DUN,DUN,DUN,DUN,DUN,DUN	del   if (secsund!\n");
     an=1".
 */

/*             (sdev,lse if (ha->scan_mode == 0x12) {
           DY Bus %d Id %d LUN %d\n", 
                        b,         nscp_cm}

modu     iu.ic.S_>OpC);mndinfo-                     n shaMODULE
_->plx->tr->i=",);
          LUSTER_DRI