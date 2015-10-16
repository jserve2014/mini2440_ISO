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
 ***************cmd_len =******************GDTOFFSOF(gdth_****str,u.raw64.sg_lst) + sizeof drivesg64or  );*********else*****************mbH:    GDT ISA/EISA/PCI Di
 * Linux      r f            *
 * Intel Corporation:  Storage R}
mbH: /mbH:    GDT ISA& 3mbH:    GDers        *+= (4 -ll   *
 *         );D Contro          cnt > 0) {mbH:    Groll         offs_dpmem +    sk Ar      DPMEM_COMMAND_rrayET) >mbH:    GDT ISA/Eic_all_        *
 * IntICP TRACE2(("      pecialller()       overflow\nbH, AmbH:    GDT ISA/EISAtab[
 * Cndex-2].cmnd = UNUSED_CMNDCopyright (C) return 0          }ge RAI      /* copy command */     ntrollopyght (C) (ha        * * <al Corpora;
}CP v


/m_le         event handling function *
 *static      evts           toreght (C)     ha      ha, ushort source,mbH:    GDT ISA/EIFixes:        *
 * Intel Corpidx,        *
data *evt)
        *
 * Intel Ce      struct timeval tv       /* nosk AH_LOCK_HA() !        ortex GmbH, Ach                    %dnner@ad\n"          idxm_leubrirolldinner@==        *
 * Intel Corporatm@inte
 *    ->            *
 * <achim_leubNULLBoji Tony Kabuffer[elastidx].     _          
 *    &&        *
ree software; you can rptec==a of d/or modify((evt->     != 0 &&y      it under the term *
 *.cense as pub           *
!memcmp((char *)&ishedaptesk Arbyby thF*
 *Sofeu,           *
2 ofby thLc Lieu, i    twar)) ||eneral Pub              publLicense,    *
 * or (at your  unde           ; either vstrioner version.           sk Aror (at ther otermsetring    ) any lat    ersionu can hat    will)))            *e = am is distributed           do_getoji.ofday(&nnanthanar FIT->unde_stamp = tv.tv_sec          ++e->same_counta03-06          ublic Li*
 f*
 * it under the termsedhanai FouNTY;ny Kantry notse for?        *
 *Seeby tunder t            *
twareunder tubliMAX_EVENTSmbH:    GDT ISc Liceublic Lin) any later    *
 * Boji Tony Kaoldaptec
 * along with thireaccensmark
 * along with thi                         *
 * Founda      lic Licens* You should have receout eion, Incy versionGNU Gen003-06                enby thimplied warrant     y KaWITHOUT Aived a te           ernel 2.6.x sUSA.            c., 675 MasMERCHANTABILITY in FITNESfirFOR A PARTINESS            CULAR PURPOSE. Free          *
  = 1        *
 * Intel Corpo      mbH:    GDT Iapplic     u can isk       rranty e;
}         int MasBorea    *
 * along with this  sup      ernel 2.6.xony Kinux kerne5 MasBoji TSA SCSnnanthcludeade GNUs inclo Chanag *
 *   * Johannes Dind
 * oris d)desby tc.com>  a compranty ofspin_lock_irqsave(&r formp     , Dis not, write *a    pl== -1mbH:    GDPCI Fi inc             *   MasCP vv      wita comp       st          *
 * In*0program is fled, y< 0 ||abled, yicense5 Mass Av", where t     un    typer     <* BIOS f the ridgeone in moray          lers bre Ch                    adapt                er possible 
 * coP     *       led, y!=eived a cC) 1995y Co disaroll++bled, yo., 675 Mass Ave, Cambridge, MAiled, you) any later0         *d linedison "gdthou-hould have recei      rsipy(by t, istrib IRQ values with e      ter the irq1,irq2,...ed		 Masy thIRQ valuesers  y thEISA        . Mas
ted			fullyvoidpp* Joe gdapdes thervice
 * reserve_mo MasAddi     /ixes:           un vers*/     All  PCI/ * r/ISA SCSIve oneArA controllersrs     thnot illers 
 * Afannel        this kservef..", whFALSE                 ******gd     init.)e pa.lete
 *  particularf all c          ,...mode:2   IRQ values for the EISAit. drivehfor P reversrs  (;;        linee op    al list ver, removemovabl, o     possible PCI/E           *
break          ve_li     articularG&an:Y* along with this k *   _mode:01..MAXID)
 * r|=scan:Y                 *
 * FoundaTRUE            *
D   *
  per c              x      ar    * ode:0  x - target Ind until now
 * b- cbute e:0  ontrollor Pes
 * s:N*
 * Boji Tony Kanabl      roll FITNE number oble drives
raw  *  icnot i      rescan ****    BIOSss:Y  nit., removablewithives
               r     , l- LUN
, remov * shared_s     *
 * Boji      rclea  
 * rs(irmw        * Johnnes Dinquiredor Psha)mode:access9,upporte * disabc.  versionshed   0ls.            by Ka}o the FCSInot erface PCI/EISA/I*
           allN   _t __          rupt.riate con          rs l:h,b,t,l,ce_dma32e alrollermax_sueservefrom_waitinclu* pIdapt           6m_dpramwith __inel D*dp6m_ptr =T.h fpr2          disableuse 64 bit DMeralnly 32 d
U General Pefault value2ed		: "gdtScsi_C4 bitscp     h- corval, i    .,for P  IStatue   max_o the Frrviray Coke, b- channeners  #ifdef INT_COAL    h- cocoaless
 * probe_ei   h- conex (1.,force_dma3r_channal    tus *pre Chifcludserv onlacttermxampl      ,reser
#endif           nnes Dina      EIS) BIOletnot ili->irqs, 
 *   /**   po driv PCI/EISA/ISm    exMA m       ode:1 ons are a**     on ". 
     scan PCel:x!y 32 valuesA mller firmwardriv"IRQ=..IRQ_HANDLE
 * You canrray CoAI Fib  h- coopt syntax  wher         e scan order for PCI controllers        e avsearch         of 'N'.
 *7,tly.  =t the ge     gdth=ontr    everontrDefa.", where thmwar    purious       as  * reserve_modral n' '         ':'an=0 with '='  reserve all not init., removable drives
 * r  reverse sreplrs
 el=0 ,' b}     *shalete
STATISTICSanoth++,0,1,3,s; * When  drivshared_rve/re:Y, witely. Yd_acfw isou must     _eisa_isarse_sch=". Howeve      rescanrve/INDEX1 rs:
 se_y. Y=C.h fotrolleis a.e Frtupi_Poi LUNLeubneiduas           **
 * i po     Mass :Y                     r  *
mplerse_          this driverNre Fle. 
 use         ode:2Hoftw    dev Adaptedoto specify the
this drivges slightly. Yo/* Foridual      requests al  *
 uf     isk Array CFinform)
 * r unused
 in     r is asinit. drives
 * reserveserve_mol(      )(pcs->r is as& 0xffN          }can=1".
* ,resc:     rollers tydes th (1. * rlean:Y              eserve_med_a8        *
 * Intel Corporationerrordma_ht, write to or (at yde
 de <l= ~nux/s
 * reserve_lisFoor PCpt coaleinw (1..bmic + MAILBOXREG+8N                         t- targtrolleas a mrnel.h%d/ete
 *
 * Def, <lay C/stranty of      *
ed_accen.h>
#ode
 inclunterrproc_fsh>
#inclu      delay.clude <linux/types.x/interruprinS_OK            *
2002 unuwill.l
#include <linux/pc,...12N              -mapsl:0,
 .h>
#>
#include <linux/p.h>
#i0clude <linux/smp_loping2ef GDTH_RTC
#includers lef GD4obe_'Y' sm/dma.houtb(_acce     lude <lEDOORREG)sble /* acknowledg  res    ax_ids=127e_scaef GDTH_RT00 <asm/system.SEMA1clude <lpinl    s in ndatiomaphoreimeef GDTH_Red_acceTH_e_list=0,1
#inclue <linux/pare:l/sca:1,        reubr    *
 * along withclude <linux/p *
 * bu>
#include <linux/pkinux/ti
#include <linux/pan ord>
#include <linux/prciize, ulong32 *cyls, .h>
#e gdw(&ost.h>
->u.ic. * Defh_eval_mapping(ulonioservize, ulong32 *cyls, delayize, ulong32 *cyls,  <linr EISh_interrupt(gdth_ha_sh>
#include <linux/proc_fs.h>
#incluapping(ulongux/scatterlint* pIndex);
str;
static irqreturn_t ma-mappifdef e gdlq * Cop_t driveiInfo[0], int *secs);
mpller 
   
#ific    ex,
           nk.h>

/mc146818rtc
     When#inclar rpora,     *
 * Boji To*****           *
ude <nt gdth_ex,
      io.irqdel);pinlio
             pinlurve/re
         _     (g_evt_      d     Cmd_ is an;x/blkdevGDT Disk led, yputq driveha_the *    an:N,
    *scio.Sema1 void uty);
stat*heads, int *secs);
cattert.h>

#i#include <"scsi.h"
#iPCIde <lt gd/t gd_hos 'Y' 
static ins asth_s
*         r       __gd(int milliseconds);tatic gdth_evt_str.t.k_vice, u(ulong32     ,         *cyls,y su *head        secth_ha_str *       'Y' a Scsi_Ctr *ha,h_stoirq,dth_ev*dev_id_evt_data *evt);
     __c int gdth_readdth_ha_str *ha);     *
 * Boji Tony K                 n       bit DMA m     * fopora_evt_data *plicatiosyncr *ha, Sth_ha_str *ha);
   g       , un of tnt(gdth          ggdth_copy_internal_data(gdth_ha_str *ha, Scsitatic void gg int gdp             gdth_eavt_str *estr);tic gdth_evt_strcic gdth_evt_strloScsi*estr);
ssi_Ctatic vvr,w
 *r *ree so);static gdth_evt_strext(gdth_ha_str *ha);
statictatic void ga_strpriorityh_ha_str *ha, ushortnexstr);
static voidh_ha_str _cmdtr);
sfill_****(gdth_ha_a_str *ha);
static int gdpvents(voib             gdth_evim LeNEWude <scsi/scsi_h);
static i_delay(int milliseconds);
static void gdth_eval_mapping(ulong32 size, ulong32 *cyls, int *heads, int *secs);
statdef PTR2USHORT the Iplx gdth_rea_str *ha, Scsi_d *dev_id);
static irqreturn_t __gdth_interrupt(gdth_ha_str *ha,
                    atic int gdth_sync_event(gdth_ha_str *ha, int service, unch#inc int gdthnal;
static clud                                  sha);
static int gdth_an_ int gdcp);
static int gdth_async_ gdt                    nalyse1hhis_rid gdtma.h>
#include <l);
static int gdth_anedoor_reg)           inodent *secs);
vtatitatic vocons*ha, Sma1(anam <binode *inha);
static int gdth_get_cmd_iMPRc const chaa_str *hAerale,drive(g *scp);
static gdth_evt_str *gdth_store_event(gdth_ha_str *ha, ushort source,
                                 /

/* The meaning ogdth_copy_internal_data(unve, Cambridge, MA 021 *secs);
stat/* #dexprobtic red_accffha);
nts(voi           tisticsan:Y#e:N,rese          nal_cache          ;
 unsi->i960r.tr *ha,t(int irq, void *dev_id);
static irqreturn_t __gdth_interrupt(gdth_ha_str *ha,
                                    int gdth_from_wait, int* pIndex);
static int gdth_sync_event(gdth_ha_str,th_ev(*done)(static int ));/* of vcludde <a* pputq(gdth_ha_sttic int __gdthus*/
#define GD, int service, unchuct s];
#ls, int *heads, int *seh_async_x2f8
#elsmbH:    GDT                          #dha_s     *s>> 16)ha_s
				anam <bt counm     64nd unuh_ha_str *ha,             ah_quireh_intes(   r*filep          
#iruct _hds, int *heads, int *secint gdth_test_busy(, port);
    f                      */
}

static voia_str *ha, serv);     
#inc                       M2__
#defke  
 * ANY WA
stadth_ctrbuf[   *S     rescanASYN
 *  rescan:N haineffy tGD#includet+1*str)!= SCREENSERVICll ',' betwe exampful,        fw_ollered_acceh=ir0x1* hdr_chantly. Yotr);
static dv    vemnd _str *ebr *str)
{/***c==0x0a),    &(r,  */
}

staticefault val)d int c)('I'*str)
{prinwserv+3*str)
{
    0         icscan"i < 256; ++i,har *str)
{) & 0x20)==0)==0     {
        NY WA[i]hile ((inb((cons5) NT_C20)==0*strt ser_printfmt, .x0dint i;

    }
}ilep);
stdth_clr_print[ int ist char *fmt, ._putc(char c)
{
    v = vsprintf_ioc for  number of vi==1 || Debug,resnd until now
 **ptr;
     ser_ LUN(t ser_priine TRACE3(a)  });
    */ine TRACEk(atic int g *Make su withat non           void gdths*fmt, enabedevortex3(a
 * {ibefinuxber;*pa compe gdt the      ns are orce_dme=1 || Dptr;++ptr)
        s!   gt_str *an=1".                int gdth_fasm/19200 Baud, va_9600:a_end(1
static int erne_, port);
    close_ioctutc(' nt ser_p*/
}al_cariveet gdic icc int ser_     ocr *h a;}}
#define TRACE3(ed_acces      rescan ev_id);
static irqreturn_tu     ne Te to the FTICwarranty of         an=0 hdr_r host =0
/* The meanaccess=0
 *           probe_eisa_isa=0 force_dma32=0"
 * T.ss Avbers r example: "modpretwecs * a;}}
#ngives
h_anahis_reslinux/led, y%_resid@CI F intete
 *tats=0, act_rq=0pt(gdth_ha_str *ha_ha_sfy tL2P(a,b)  lows:
 ,   h     r hoges sl    lyPTR2*e is aservint)
 * Defn:N,h
#els);
stath>
#inclurATISTI#def)rs atatic gdth_ev = {5,6ev_id);
static irqreturn_ttate=.    pt ulong32 act_ints=0bugStatete==2) {th disablett gd,t)   ((iio,t)   ((i*   ,t)   ((i)<AR_ha_str *anam <boji.r * fo  outbic un; gdth_as
#d_putc(dth_his_rode:2      rq_Inte6] =USHORT(a)   (ushort)(ulong)(a )         [4] = {5,6,7,7}SPEZions are a DRQ t * sh;
st Whenhere the _e_ei=0orhe te  gdth_ized ! ulong32 act_ints=0-mapp{     *
 * Boji Toa;}}    u.ode *i * This incl,fmi;

    v /* polw.ionodsx, gamehanunges sligh
static voie
 *jo hostc gdES_DRIVER, 4,#includvude <_through =  {0,10,11,12,14,0};    /* IRQ table */
#endif
static unchar   gdth_polling;                  intern a;}}
(oid gd)      )(a)':' with cp buff+1];
#i04 Inte
 * Def    erse_max_rq=0, LIST_HE       DOUc gdtU General Pub = {           OUT data direction */
#de      /* no data transfer */*
cdes {      /* no daroller count */
static;
         d(CONFled, yto   gdthc gdth_ev(%d)nt gdth_syn     DIN       reverse scrs li*/tatic vonts(voi          /* event buff             DIN      ude <livent buNO,DIN,DINr);
OU,DUN,DUN,DUN,DIN, n daIGDIN,DIN,DOU,DIN,Dnal_cachethe free sof   *
 * Yo]    *************/*       ree so,DOU,DIN,DO   gtware; y            gnse ,DIN,DNO,DNO,DINmajo;}}
        DI      *************************U,DNO,DUN,IN      direce pa        INTERNAL    DOU,DON,DUN,DOU,DUN,DU       /* unknown daInswtatidma_h{in*cmngdth_ev        0,10,[6] = {0,10,11,12,14,0};    /* IRQ table */
#endif
static unchar   gdth_polling;                                       /* IN datUN,DUN,
 
 * Linux a,b)  (    _t)&G_ * rterruptarranty of    atic ult: "mIN,DOU,DIN,DICmnd *))de <linuxscp2sm/ua)<AR,, act_ios=0, act_stats=0, act_rss=0   ser_init();i_cmnhis drive=0ers s anoth2=0"statTeverDUN,DU=rection */
tly. Yo hdr_cutqDIN,Dsefine,DUN,DUNk a;v    )->si_C;}}

#else /* !har b);
sta,DNO,DUN1DIN,DOU,N,DUN,DUNDNO,sunchser_N,
  DUN,DUN,DUN,Dic int  ser_printk(constAX_SERBUF+1];
#ible drridgtruct_goDNO,    ode:2cs);
ststatic int __gdth,
    D/       gdth++pcly. h_anaollers
 * fo=,3,02,0,NeadappN,
 ,3,0 r0 * DDUN,DIN,DOU,DOU,
 UN,DUN,DUN,Dc>sm/uDUN,DUN,Dint i;

    va_startnsmod parame =0UN,DUN,DUN,DTRACE3(a)
#endif

 a;}}

"GDT:ers  GDT3000/302ete
 *(s=0, a)for GDT3000/2 max_rq=0, max_i{/***ebugSta}}

#else /* !__se
statiPorephasano_COA * buffe followDNO,DDUN,
  uct scUN,
  N,force_dm    /* IRQ table wittdma_hanDUN,DNO, loop
static int gdth_syn       re buffnx   g)(a)
l:x  wrbuf,fode:2ude <asandDUN,DNOCE2(onl..". Hnew *filepeada0ing;      *havailabervicxff,_COA,0xigneanam <bf,0xxff,0xff,0x    OU,DUN,DUN,DUN,DUUN,Dputc(ortexUN,DUN,DUN,Dortex Gf,0xff,0xff,0xffk a;ort hdrichar c)
{
 "scsiDIN,DIN,       ,DUN,amndinfo) "gdth=d,DNO,DUN,pol    he:x     e0,resersem          OU,DUN,DUN,o#include <linux,2,0nt(gdU,DIN_str e mean	******************t,b)  ;

	"IRQ=...    y. Yoer as awith alse,s
 * 00] = {
    f,2,0* optIN,DOU,DIN,trollode:2 if sa);

s#define     *   N,DUNrcontrOU,DOU,
   an:Y              use* driveIAL_t:0,1,er as aA  unchare fully             m:  Sto *msg: "gdth=dollers    ****er firmware reb, ee sos(strm <bstatic inN,
  * /* !OU,DUN,DUN,tatiUN,DUN,DUDt inoodvaluDUN,OUTpccb      DUN,
  atic iIN,DOU,DIN,)a****)0)->b)
us     _OK(i,t     (i****0;
/ha_str *ha,
  nd:               nedchar =COM_BADIN,DOU,DIN,msgUN,DUN,
p,DUN,DNO,rle drimodprlen: %d,odprobent, NUext;
moduL gdth, q, int, NULL, 0))<ARmsg->DOU,len,e_T3000(hdprober host , iext;
module_pn          r0xff,0xfhannel, idr_ > MSGLEN+=0"
 * The i/scsm(rescan, in=, (strmod0);
modared_accestly. Y, ih_queue       0xff,be_eisa_isaprobe/&&red_accesslkdev unu*/    , rers lifannel, itext[ed_access, i)
  '\0'           DOU,DOU,DOirq[M%s";
module_pE_LIxff,0xff,0xff,0rbuf)param(probe_eisa_is0xff&& !m(OU,DOU,
   Cmnd *));
stly. YoU,DOU,Dsa_chaest_buslisheint, 0);
modultr);ase = *gdthxcp);
static intvalu->        r);
stparam(reserve_m int cmd, _roc_.c"

RUN,DUNBh_evt_stram(redata transfer */
#d,DUN,list,er@adapt/* polling if TRUEobe/indproema0/* polling if TRUE
{
	gdOpC   Dr);
statiinodREmode, int, */
#de{
	gdBoardN        = LOCALBOARutb(0);
    *r);
sgetu.sces.h.;
strveth_ha_s, int *heads,
{
	struct gprisu.msgypes_a comp=r");
MODULues foto setags;
	tatic= 0;ntroller types.
 * addrUN,DUN,

 * phyDUN,Dc int fINcsi_do. place of 'N'	0xff, fisk ; i<GA/EISA/PCI Disk Array Controllers      MDS
    ) {
		/*******)module=0;
#ifde f0xff,0xff host 6;

smbH:    GDT ISA/EISAinfo[i];
			memset(pr. drives
 * reserve_lis     gdth_evt_strreleasUN,DOU,DIN PCI co va_se aerface      /* contrs = {
    .ioctl   = gpen   erse_c vo, flecond,
    DUN,DUN,DUN,dcoaletDUN,
    (of vof ()he te        )ptr;++ptr)
        ss)
ler li/***atic intstatic void garam(probe_eh      i];
			memset(pr/* The meaninsecond DRQ tabTRACE3;
   lock, flae_opC0)
  , Inc., 675 Mas    ontrollers
 scan _ser__ioctl(s_ha_s-= 2       
{
+3);
    outb(0struct gdth_c_ha_scomma)\n");
  	gdt    i<GDTH	gdth_put="));

	gdt
    (      	1aticatic int        ck,		mems;
  ct0xff

	if (L;
	ulong flags;
	ingdth_store_eventlse
		scp->nfo(cm  .priv)
{;

	if (i
#de,
}
    si_Cmnd *scpnum)
{h_spemd_str *gdtcmd, ch
	gd,DIN,DOU,DN         h_ha_stnd_hah_stoough si_dor);
static voidscp-rs l_for_kern_ *
 *(    & Scsi_Cstancids:rs l)eak;
		}
gh *== ****ha_str 			 * Copy(sdev- * Copyhis            /anam <WRIT    gdth_ha_str *ha =
	gdth_putatic void gdth_ei_done()\n"));

	gdth_put_     =ait);
 [i];
			memset(prDTH_MAXCMDS; ++i) {
		if (****        pletion *)s	     i=0; i<char MAXC i+1;
			break;
		}
	}	gdth_pu[i].rporanfo;    RE_CEM;

  IZE,  -ENOMEM;
 ;RE_Cmemset(    , 0,       (MEM;

	scdevice     }

   i+1   /*D cou   /}
	}MAXCMDS;unler typeres    SIZE, GFP_KERNEL);
    if  * Copy               /* DRQstatic icp), GFP_K)
        return -ENOMEM;
(!scpBUG_ON(!cmndi;
* use request f0           ce */
static conN,DUle gdth_= 	gdth_pu      b,DNO_exedeh>

->r host        */
 *)&    1}}
#defTl      void gdth_dma32 =it);
    020 Eong3bigne thrvirce *sre_event(gdtnfo;*inode,[BUS_L2Pl */b)].ioest [t]--t    mw0xff,os=0, act[MAinux   sraws = {
  access.h>
#i, ac *secs);
sta *cmBSn:Y roller count */
static LIST_HDUN,
cloal n"IRrse_scan = 0_tab[0x100][0]
	scDUN,
_str *t, 0);inodMOUNSA/EISA/PCI Disk= kzasi_Host *shost,  counCLUST_INFOERNEL);
    ifty);
    		memerface */
sode:1 cmndinfo(cm,DUN,DUN,DUN,
   i_linulenst);
int, 0);
modulpci_unmap_sgfo;
 pdevst);si_sgrs l,DUN,DimA 02stribute;
      ebugStatete==2) {ebugStamndeadapp_h>
#dle drh_exe	gdtuct Scsi_Host s    _pptr. _str *execute(sdevcsi_dpag"gdt	gdt, ti                     , 16sterface */
stati/* The meanin) cmd UN,Dable support for EISAPCI_DMA_FROMDEintert source,
     , cm   ke focommOKfo);

    wait_f           nt(gdth_ha_str *ha, int se

static v32mndi ne etions, int *headnam <ban:N,Horite host,!ecomIse /* !DEBUG */
#de* Johannes Dinnor GD*/= {
: him Lentili 0x%x OKscan, int, 0);
modul_ha_str 
         *
 * Bo *ha);
t gdth_cmndin act     *
 * Bojiunuse kza   scsi/utb(0d_stnonclude <linux/types.s <= MAXCYLS) {
       outb(GHEADS/BIG   ushor = MEDHEADS;ist);
 chdr[t].cluster_ "scsiescv);
*/SECS ser_print/***    s = {
    .ioc      '0' in    unused
i&rve_list:h,b,t,l,h,b,t,l,.GHEADER;
    EDAUTHOR("A   *    bstr */     *
 NOT id ser_prin orderclude <linux/types.     =      /
}

/             
    /ort hdriO,DIN,,DUN,
sions gdth_f0_o = ggdth_ev'0' ihis d        is dradr);
    }#defi       REservE         disable r 127*63arch_eisa()gdth_se>cmd_l     scp  (dma_hans
 * s         *gdthre_even_ope Copy               /*;
/*liza-2
    DUty);
strv)
 * rconflic{ a;}}
c unchar f      returf,
 0xff,0xffUN,
 f     info.internal_coete((strnb(ei+ID0nclu

    va_si    r firmware n 1;

    return 0;           th_ha_str *ha, Scsi_C}
staticADS;
            *secs = BIG
    UN,DUN,DOU,DUN,adr)
{
  se_bnitdataA in nitdataBarch|=REG) & 8). %x\n"D2) {pdnfo;nitdB_ID)e DUN h_isa() biomedia_rtexg,DUN,DUN,D= gdth_ope Copymd 0x%har b);
sta
            *secs = BIGUNadr));
   0x%x\nf,0xf(*gdtcm000B */    ) bios adr. %xom> de <sb(eisIG_EISA */
(ha->t fiorude (    dif /shar_ID_ Linf(cmndinfEG) & 8RANT!    sccp->           = gdth_ope{
        i= GDT3B_ID)DUN,D|| id == GDT3BID) {    turn 0;           
#defid;

	TRACE2de     */

#if iorem_I_dev(UN = HIGH_PRI */

#ifdef CONFIG*ds, r+IDha_sget_h                        
}
#e/*nam@intt/RELEAS  if (id   }
    }BIG
/* co() cm    = kza=
		retur      if (id == GDT2_ID)                      /* }
    r)); /* not ce(!scp/***return <ar b);
staID_VORTEX_GDTNEWRXrue      return ==ontr_DEM_BA 0;
probe_one(gdth_2ARE_r+BI2000arch;uct requestati           /i/
        re;       els,DINion->N,DU>requD0;
}K <<tb(0*/

#ifdef CONFIGce_iigneulorivern:Y GDT Disk A;

    ser_init(); = cmndinfo->internalo highfor E64*    /x/interrupic voia_adr+ID0REG);
   ADS);
 ctions */
#ifddef CONFIs <=    CYLSef CONFIG_PC(device == PC
}

/* co     *
 * Boj gdth
statictions */
#ifd}  *   delay.{ PCIORTEX, PCI_ANY_ID) },
	{ PCI_VDEVI/* to

/*  <asm/int gdth_testt gdth_cmndindl   ret ser_printiov, gd(adSCANs
 *R
#endif
st_ID) },
	{ PCI_VDEVICE(INTEL, Pha_str **haINEDUN,DUN,DUN,DOU,DU2_ID)      v* Joh     if returnfalse;
}

static*ha_out);
sta6555ARE_ool gdttrued_ta_scr
}

static>gdth_ha_str **ha_out);
sta6x17RP	.re &&
/
sta
static gdth__remove_onha_str **ha_info, er ver*)*cyls <=(sde&
	 O and6 0xff,0xff,0;      i_remove_one(strund, );
x7e
		scp->scsi_doud(*cyls <= *ha = pci2get_NO    ouYata(i_get_d
	(sdeset_dr *en *sts(tic gdth_evt) | (CHECK_CONDITIONth_ev
                     , 0);
modared_aCACH   scnternaseconds)
ATISTI;++ptc);
             _GDT6_UNKNOWNSE   if (tic intmt,args);
   000A or GDT3000B */
        if ((inb(eisa_adcio = g_ommand;s
 *   if (ase0, base1gdth_p2void gdth_put_cmndadr)
{
  /*ol g);
statLicenscePCI_Dctions */
*/
          inl(((inb(e**ha_ou_one(nd *scp,
  dtcmdase0, base1, ba_str *ha = pci_ge,
	        /* DRQ_e(stex or GDT3tr_c   ape, vendor, device));

	memr *ha = tatic idor, deviceons gdthreturn_i    sch_pcsa;}}a = shost_priv(o *prr));

	ifh_enat);
	ges
 * sh   scAet_dce(pFLICTi;
}

#define  0, sle drih_isa(ulong32 bios_adr)
{
   int gdth_te =ata(pce <drv    &ha->list);
	gd
	gdth_re resourcesram(rLpdev)>hostdelSIZE, th_cmENODENODEV;
nable_device(pdev);
	if (rc)
	return&ha->lisdth_ctr_count >= MAXe,
};

static void __d <= PCWe do not readapp_event(gint i;

    va_start(argt);
     DNO,DNO,DIN,DIN,DOU,DIN,gdth0xff,0xff,0x   ret) {se0Bar   earch_i6N,
    DDNO,DIN,DIN,DOU,DIN,DOU,DN, gdthtble,
	.probe		= g    unsi isable __DNO      IN,DUN,DOU,DOU,DUN,DUN,(!(bas    y( makes-06 Ad reverse smode:2*/
            if (!(baC intDUN,DUN,
no);
st| id == GDT3B_ID) {V; gdth_dth_ctr_    *******t for EdstatVENDOR**ha_out);_ioctT2_ID)a_end80000/B */
         || DebugStDUN,DIN,DUN,DOU,DIN,DUN,ci_r;
statDOU,DNO,DUN,DIN,DOU,DOreserve_mod BIOS disa* Jo& IORESOURCE_MEM         *
 * Boji To_pcis;
module*cistr.dpmem =IO)) gdth",
	.isdev);
    !=0a_str **ha_out);
staMAXRP)
ct pc, ...);
fil    nor (f,0xff,0xftfirm     ( chatatic bool gdth_s	TRACEX && !gdth_sgned_RAW_
 * T||ervice, una_end(as, 
 *       driv_one(stevicedmove_one(    BAD_TARGgdth_hc(gdth_pcistr));

	idth",
	.i-EBUSY   if (EVICE(IGDT Pble_device(pdev);
	if (rc)LE) },
	{ }	/* terminate lce_start(pdev, )evice <=o= pdev(
    )e      _flachant);
DNO,D        void gdth_enaheadV* Joif ((inb(ei,r);
sta+     ontroller B,
	    bool g*sdev =it_one(th_cmndiranty of int reverseversi       (infU    get_ DUN 0ons "\011\000\002 *ha,s, dei4ts, dei6used"r_count >MAXH HA %ubase GDT6f,eisT3B_ID)_tr.dpmiu/%luEAD(gdth>     1le bcmndcomples, deinitiearch_eze_clear_eh_exe  va_end(arfDOORRndif /h>
#incluchar *fmt, .x00f,eisa_adr+LENAB2EG);
  05 gdth_reas,REG);
    
    outb(0xff,eiYLS) Disk Ar,e& IORe gdyRIES;
her t_str *gdth2(strbhort vendot,ar,eisa_adr+LDOORREGe as xff): REASSIGNf CONsu,DNO,fud
 * Gene0);
moelay.      ssignearcnt __.alizatimay crashcp->cmndfut__dev, 0e, Cambbge, exampdRIES;
4retries == 0) {
            printk("GDT-EISA: Imielay.upd    n 0nitialization  failit wies=5retries == 0) {
            printk("GDT-EISA: IMr+linuutb(0xff,eisa_adr+LDOO6NFIG_EISA */
prot0x20)!= PROTOCOL_VERSIONef CONFIG_PCnt irq[MAXrnel. (DEINITisa_aedfo);
st struct pci_ * Copyrable[] = {
CONFIG_PCt_str *gdth0 & IOREif (id == GmbMA mfor Er;
   : it wies=7retries == 0) {
            printk("GDT-EISA: Initialization  h_int,DOUtectEG+4);
 8com> EG+4);
*/
       outlrsion\n")d vetk("GDT-Eendiom>  CONFi 
    outb(0xff,RIES;
9  outl(0,eisa_adr+MAILBOXREG+8);
    outl(0,eisa_adr+MAILBOXREis    n PCRIES;1le s;
            t %d vendor %x d /* CONFI)
	if (devicef CON        ha->bmic = 1f,eisONFIG_EISA */
rsion\n");
        return 0;
    }
    ha->bmic = TisEG+12st_dev(CE(Idetec*****ries == 0) {
            printk("GDT-EISA: Ig       eisa_adr nitialization . Pndinf checkcp->c);
	if (rc)h.h f(pdev,r FITNEEG+4);
 7= 0) {
        2\01b(0x0
    
    outb(0xff,ei     nt irq[Mu: C;
  o optd veeisa_adr+LDO1.com> / 
    if ((id = inMAILBOXREG);
    outl(0,e****dr+MAILBb(FAI prooue/
#d Scsi  retES;
        gdth_delay(20ORREG);
        TRACE2(("GDT3000/3020:  retu);
   al(eisaoc   TRACE2(("(0,eisa_adr+MAILBOXREG);
   TRACE2(("GDT3000/3020: isa_ad95-06 ICLBOXREGff,0xeisa_adrlinux);
    va_enl( = INIT_RETlinux/from cmd line !\n"));
                fp    whbuild/ctypys = (ulo, re*/sa_adrEG+8);
     from cmd line !\n"));
                feof(ling1optiic coi]==11 |D_ICPst struct ****table from cmd line !\n"));
 Te/
    Hot Fixnux/ctypq));
  2HA && irq[ictype.q));
   4so wildcard matcINTEL, PCrq_fou     se a));
       LBOXnel;a->brd_phyl2 || i2Enst struct ries == 0) {
                irq[i] = 0;
                pand line GDT-ISA : Can. IDs     tn plac       wn IRQ, use IRQ table from cmd line !\n"));
                f(0;
      )activa        2            scp-> while ((rom cmd line !\n"));
 nitializatio (irrobrd_ARGS]f i/o abIG_I = ue,
   eeral Public Lic);
    2   outb(0xff,eisa_adable from cmd line !\n"));
      ha->irq q));LSE; 
        Od;
    ernel. ({
          gdth_dele !\n"));
     outl
	{ PCInable\n");LSE;cf+ID0REG)) == Gr+f,eisa_adr+LDO2CE2(("Unkol  *
 * b_phys            printk("Use IRQ setting from command line staan P= 4str)
{
   i < MAXHA && 1);
 1
    
    outb(0xff,eiFeisa() UL    SHELF OK              2| irq[i]==12 || irq[->s,...MAIL %x\n",truc       dma64 only 32mndinfx\n",
		gdth_pcist}
#eak;
        outl(0, */

#\013/

#ifdef CONFIG_ISA
static int __init, IDt:0,1Auto %d)\Plu(ulo     
#ifd3HA && irq;
    if (!scthe hogid in    2      g32 ise 6    *dp2 mod if (ei   },0xff one long32 bios_3ontrollerlong32 retries;

    TRACE(("gdth_init_isa() bios adr. %x\n",bios_adol    if (****e "gurn 1mand line (GDT3000);ies;

    TRACE(("gdth_init_isa() bios adr. %x\n",bios_adplugg.inclu    tie   if ist);valt %d vlization erro  outl(0,remap error)\n");
        return 0;
    }
    dp2_ptr = hatr->i       DT-ISA: Initiali   outb(  outb(0x00,ies;

    TRAC           printk("GDT-EISA: Ilers
 areaarch_eisinfo,suffic		brpISA
scapacgdth(trigMB, ...nabl)itiali{
       witch       DNO,

    /       
   n_unloc	comple"GDT-ISA: InitializaISA
s 0xfe) {
           3utl(0,Un.h>
n| ir, uisa_adr >> 12;_ISA
st    s
 * sh;
    outb(0x00,eid by ler     isabns gdth_tetrieGDT_EISA;set_io(, char *b
  r->ualizat(inb(eisa_adr+EDOORREG)- %d)\I: swap
    }
   or)\PCI_DEV| irq[i]=long32 retries;

    TRACE(("gdth_init_isa() bios adr. %x\n",bios_adinit_ints(void(, re= %rd_ph   /* too hig3d;
    unc3; ++i) {
        if ((irq_drq & 1)==0)
            break;
        irq_drq >>= 1;specify tT3000/usor (nts(voiRIES;42_ptr->uEG) & 8)EG+4);
   if (id == GDT2_ID0, baD_VORTEX_GDTNEWRX2)
		return ef CONFIG_PCIG+8);rqght (b1;
    }
    rf(cmndinf(("gdth_init_i*/
         ISA: Init

#ifdef CONFIG_PCpr */
drq >> cmd oDEVICE    rq_drq,prot_%d\n(--retries == 0) {
          irq[i] = 0;
[i]dth_init_isa(u }
    ha->bmicdr. %xiteb(ev          TRAble from cmd line !\n"));
 DRAM         retur ha->brd = io%d\n outb(0xff,eisa_adr+LDOORREG);
   ;
    outl(  retries = INu:  eisacfptr->u.ic.S_Spriteb(0xys = (ulong32)eRQ table from cmd line !\n"));
   irq[i] = 0I_DEV= reaadr+EINT       7;
  vent)(("GDT3000/30* polling if iggere]              oem	gdt= OEM_no matchCE2(Poolhar)d byllizati->u.idth_iS_St_Info[0]t arutb(A_ioch_delayization1;
			bre                 olleb(0x00, &dp2_col version\n");
               2                             irq[i] = rot_ver = (unch &adr. %xiteb(0x00, &dp2_col version\n");
    eak;
               
#ifdes);
    writeb(0xff,E_DE;
      ->stype
    ha->ic_all_size = sizeof(dp25eak;
        irq_drq >>= 1;
    }
    ha->irq = gdth_irq
 * T. %x\n",bios_adIGNORE_WIDce))
IDUE messasave_ceiv  
   w5          io.i_phys = (ulong32)eretries = INUst., resett     romexp(0,eiq_drq,prot_5ve_one32)eisaown IRrq_fo_RETRIontrolleries == 0) {
            pfo[3]);
    }
    ha->drq = gdth_5u2_ptr->io.), Eable c.S_Info[2]);
    writel(0x0= GDT_ISA;
 _inte, &dp2_sa_adr+LDO5   outb(0ILBOXREialize service-         CPU temperartex criticA: Ivers{
        RQ), Enable\n");x);

    irq_drq = reaadr+EINTOKror\n"       ha->irq = gdth_irqx\n",
		gdth_pc   writenitialevel trigcrer->u.ic.SREG)OL_VERSION) {
        printk("GDT-ISA: Illegal protocol versit);
   ed					OU,DIb(0        return 0;
    }

    ha->oem_id = OEM_ID_I0;
           t);
   
 onot inOU,DIe thion:  Storage RAIDgdth_pci_str *pcistr,
				   gdth_ ha->typeretries = Iqui   
   w6ACE2(("GDT3000/302_dela, gdth_pci_str *pch_delay(20);
    while              t_isa() bios &dp2_ptr->u.ic.S_Info[2]);
    writel(0x00, &dp2_ptr->u.ic.S_In         ify tt_isa() bios 0xfe) {
 ptr->u.ic.S_Cmd_Indx);
    writeb(0, &dp2_ptr->io.even       with y       if /* 6 (--retries == 0) {
         d byb( = GDT_ISAver != PRdefializat>vendor, gdth_>= PLE_DEVISA: Initialization error (DEINIT failed)\n");
 [MAXHdata = 
0: _id = OEM_ID_Io specify a *dp6_x100led) *    esul,DNO,o specify alGDT3000/3    iounmap(ha-> Illegal protont irq[MAXH *dp6_ptr;
    regteb(0, &dp2_ptr->u.ic.Status);
    writeb(0xff,_id = OEM_ID_It_isa() bioseb(0xff, &dp2_ptr->io.irqdel);

    ha->dma64_su("d_ac revers)
{
    vemove_        return 0;
    }

    ha->oem_id = OEM_IDurn 1;
   turnmth dedl(0,ecmndinfem, siz    el;
    }ZE, Gype= iore2_IDn error (Dbrd_phyL) {
         tr-PCIreturn 7HA && irq == 0) {
            printk("GD/* not E gdt6_dpraml(0,ecorrode, hral nECCistr,
ontroller == 0) {
            printk("GDUnr %x\n"dth_fopi, gdthtp6 mode, hNDOR_IDl(      }
     atioIG_EISA */
);
  NTEL;
  1EINTL;
    else
          ;
    }       ies =, LUNI_DEV, gdth_pCE2(h_ha_str *h  gdth_delay(20);
    while (readb(&dp2_ptr->u.i= iorelse
  ;
   >gdthhloca           o: Initialization error (DEINIT failed)\n");
 i    0x400.Cmd_Index);
nabl      (uncys = (ulong32)eCI_DEVIC  return 0;
     REG);
    retr75INIT_RETRIE};
nt reverseonly 32 DIN,DOU,DIN,6c_dpram_str __i            spin_ult values unsi_cmner#defitec
   	    gdtf,0xff,0 unuerve_lannes DinDIN,DOU,DIN,r aparam_ed_accscan, int, 0);
mo,DIN,DIN,tatic v2 *info)ct,0xff,0xfGDT3 = {
    .ioctl      rescanseconds)
dev->bus->numbeci_MSG_REQUES       if (id == truct scsi_device *sdev, gdth_cmd_str *gdtcmd, char *cmnd,
                   int timeout, u32 *info)
{
    gdth_ha_str *ha = shost_priv(sdev->host);
    Scsi_Cmnd *sistr,
			DUN,DUN,listtruct gdth_cmndinfo cmndinfo;
    DECLARE_COMPLETION_ONSTACK(wait);
    int rval;

  "));

	gdth_put_cmndice <p), GFP_KERNEL);
    if (!scp)
        return -ENOMEM;

    scp->sense_buffer = kzalloc(SCSI_SENSE_BUFFERSIZE, GFP_v, gINVxample: (!scp->sense_buffer) {
	kfree(scp);
	return -ENOMEM;
    }

    scp->device = sdev;
    memset(&cmndinfo, 0, sizeof(cmndinfo));

    /* use request field to save the ptr. to completion struct. */
    scp->request = (struct request *)&wait;
    scp->cmd_len = 12;
    scp->cmnd = cmnd;#include "scsi.h"
#ide <li interface */
static con[de < sloUN,D] 
         d int c   }
KERNEL);
    ifr b);
static int gdth_speuf[MAX_SERBU                ,' betweex%4X&AGIC) {->uzeof(cmndinfnitializat Cannot ck); /* switch off w
        PCIruct fInitializati
#inclror (DP>>8 gdt_get_d32)eisa_adr  if g32)eisa__adr >> 12;

  3)&0x1IN,DUN,DUN,if (d)acf,icmndinfo.priority = IOCTL_PRgdth_eGver
 * reserve_mtwarf,0xff,0xfh_fop    /* GDT3be useful,    
    ser_puts(strbap(ha->bcint i;

    va_s = {
    DNO,DN(&cmndinfo, 0, sizeUN,DUN,T3B_IDc int f,DUN,DUN,DUN,DUN,DUN,DUN,DUNost_dev(s3B_IDstr.      oller rt_eisa(ux = t&ha-");
      wR */
i = vsprintfnused
 *tr->tic void ser_printk	     const struct pci_ = {
    DNO,DNO,DIN,DIN,DOU,DIN,     tion_tab[0x100] = {
            0;
   >i_init_one,
	.reios_a/* MPR
            INIT_RETRIES
/* padpmem = pci pci_resour 0) {
           if /* C.irqe = tializater != char (DEINIT faile= 0x        /* no data transfer */
#ZE, GF32rn i;
} {
           A 02coorcp->s0x3taticn device <=_init gdthDUN,DIN,DUN,DOU,D    *
 *;    lse _cloom>  gdth_pc32 iKERNEL);
  ase =log        cistr.dpDOU,DO        defiic_all_s,0xf

   eof(us at %t);
  ORTEX, PCI_ANY_ID) },
	{ PCI_ev->br, device)ong3ha_str **h(gdt5 *dp                          /ist */
static  true;_all_size %d  ha, gdth_pci_str *pcUN,DUN,DUN,
 lization etion(&(a}

#else /* !__f (unctruct _hdth_cn & -PCI*****PCI, gdth_pc;nd *scp, unchaISA;
   A controller1ax_ids = MAX, dpmem rtati)
		return -EBUSY;

  d)\n"n",eis*
 * i          retstackframeturncntil nowversifers
 * for,DUN,DUi,j              t- targp6_ptr->u.ar (ulong3 matchdvization erro to specify the
 == NULLase2;
eturn -ENODEV;

atic conons ner@ad: %cp->,     tus) != 0xff) {        = vsprintfR_ioctl(struct inoNOMEmemset			  satic c.S_StafCmnd *));failed)\, &)h_dedth_ctr_writebtializas) != 0xfe) {
 = FALSE;

    TRACE(("gdth_r (DEINIT fa         \n");
         pri

    irq_drq     DT30opyri   retOKor\n");
    turnUN,DUNlizati Cannot 
  
    TRACE((* JohanneN) {]A3B_ID)     trueuct failed)\) || rno. Chaininseconds)
{
init_rintk("e) {
   tr *ha,
                   ;
  nb(eisreturn 0;    har (DEINIT faile           >io.irqdel);i>devi00/3      push, j:fo[1]) elemAILBtollersdev() cnt %d ... j=0,i=1ar  arf[0]; i+= &dp6_ptr->u.iIN,s);
   (f[i+1]eturn -ENODEV;

	rcQ fa4:&& !gdth_searirq,
D_VO.b[j++get_unmap(ha-*)&r\n");
 stream[f (def[i]           /
        2   detk a;}}
c unch_pci2ew()       %lx_iomc.com> _ptr->u.ic.S_Info co     ,r (DPMEMa64_supportISA: Initialieturn 0;
   {
      &dp2_ptr->1c_dpram_str));
        if (ha->brd =;
   L) {
            printk("GDT-PCI: Initialization error (DPMEM remap Illegac_dpram_str));
   (pcistr->dpmem, siztr_count >=0A or GDT (DEINIT failed)\n"e;
                iounmInd    &fprintk("0]],c.comETRIES;
     de,{
  gdtcm %x\nd_Indx);
    went);
    retries = INIT          MAGIC iounmap(ha->brd);
   ->ic_allNIT failed)\n0_ptrdp        if (readl(&dp6c_ptr->u) != DPt access DPMEM(gdth_T3B_ID)lay(1isa_adr+LD    }
 failp6_ptr               iStha->oem);
       protocol versPCI: Initializa          s) != hdriDEpriochar
.irqdelt 0x%lx (shadowed?)\n", 
     isens sizeof(ushort)); 
   brd == NULL) {
                   return 0;
        lizatioindif /* CO      );      retrv->irq;
    ha->(0xff, &dp2_ptr->u.ic.S       cmnd *,DUN,DUN,DUN,DUN,DUNev, 0);6_ptr-O,DIN,* MEr_rde <asN,DUN,DUN,DINe;
         utZE, GF             istr,
				  maxnd ISA cont gdth_put_cmndinfo([0]);
 r no., b- channel A conif(unlikelth_sfo_empty(&tr->u) struct RANTY
	 /* GDT3     hm_str));      2_ptrailed)\n"); GDT2_IDos=0,es
 * reserve_liI: Initializatio return 0;
  ,fdef       reverse scan order for PCI controllers              UN,D->io._ptr-rs l<eserver) {
	n",bios_ = FALSE;
  IC)  data di        !eUN,Dries =IN c void gdth_scsi 0);p6c_ptr;
    regol g,DIN,  io=hout eq_ptr->;   iowr->u==(nd ISA co*)  io->SCp.pve o void gdth_scsrq              t- targt  /* 0    moduio", i);p6c GDT_Pr;
    regi->dev0, ba  ioold() DUN,D_isa   regisave thptr->u       i));r_na       b8);
as adma_hands drive =      .expi breakjiff    + 30      /* co}d

statifff) {
   TRACE   reversORTEX, PCI_ANY_ID) },
	{ PCappropriatremap(pciwritel(0x00, &d_ptr->UN,D The mean	b)>(a)->v_ptr->u.ic.S_)
	*headma;NO,DIN,ic.S_Info[0]  hald t* Johannes Din       0x%Is           _ptr->u.ic.S_I       e er i < 0xE8000; i n");
o fr    }
   ali*inode,0Le <lrnel.fo);
ntk("GDT-Perface /* spec
	ddr         !_phys = (ul  ifrt+1ce_idAXIDEISA!= 0) {
            printk("me,0xff,};
nux/pci.h>
     }
UN,DU_ptr-   Dupr versiturn,2,0*   b        tializ arg*******    wrcureturn *argd theanN     annes    IZE, plx->)his_ %
#incl gdtete
 * _plx(strs
    tr ?f /*:".h f"d = io ?        :0x        ande gdtfor[]sion\n     ]r flag *s:_ptr->u.ic.t_str *intec inter !=eval_mapping(u_ptr->u.ic.S_Info[0]      CI: Init.utb(>u.ic.Cmd_Index);
      reb(1,P return 0;
         vachar  arinterE;     pci.h>
#includeurq(a)  ueadb(, &dp   /* contr options 
 c I (DPMEMr=n -E#defi++p   i
tr.dverse_sc);
    */
}
&& (>close(all_sichric.Cm,0 sh}
    0d,port);onlcp->cm0Cmnd **++                  //******I_VDE   }'n'tr->_DEVICN'        printk>u.ic.SScsiion\n"f,eis NULL) {
 y return 0;
n:Y            * CopyrmbH:    GDT rt+1 p2,,port+1scp->cmf (devi
   x(usto"Con"GDT-PCios_a, nclu0A or GDT3_adrstrn    rq[MAX"dis\n"):", 8ost *shoint *heort)); 
=
   _ptr->u.ic.S_istr,
c.     ha6_ptr->

    sterndth_d13ver != PROTOCOL_    ha->irq;
    }
    hion error (DPMEM     }llegCE2(r_puhainindelay(1);
    }
   ation error (D->brd);
            return 0;
        }

node    nel
 * b2ver != PROTOCOL_/* _cmd_il->brd);
            return 0;
        }

for Gdsrsion\n");
        r       ->brd);
            return 0;
        }

  r>stype=7phys = (ulong32)ei       _D0REG+ID0RE (DPMECI: Initi);
	if (rcsh    _a>brd_
 * b4ver != PROTOCOL(structeisa() >brd);
            return 0;
        }

DOU,DUN,DNO,DU
 * b5ver != PROTOCOL(iteb(0x00, &        writel(0x00, &dp6c_ptr->u.ic.S_Inf  iounma unus= gdth_(b-1):(b))

#ifu  = OEM_ID_i_get_ Illegal prot..);
    }vaght (ha-INITRructRGS = (+eturn -ENODEV;

	rc CI: Initializatin       re',' gdth_hase0 & IORESOURC!       };

#include "gdthout,(pcistr->dpmem, sizeo
#eturnisdigit(f (de  ha->brd_      id == GDT2_ID)     --a->brd_p/
stat               ioirq,0);
    while (istr,
				  t(cp;
  tatus)zeof  ha->oem);
 r)
  	ushort vendor = pdev-wed?)\n", 
            (readb(&dp6_ptr          /* return 0;
   eturn 0;
        }
   }
      (pcistr->dpmem, siz
       ha->brd_phys = (ulresiduntinu_MEM)) 
		r gdth_evt_str *ic.Cmd        return 0,
   c void gdth_scsi Illegortex Gmtion,dth_ino    w0x00, & revers1)(ha->brd);
PT    _reg)M_ID_IN0x00, &ense,    printIC) {
lizata->plx->ldoorbusy\n"DPMEM_MAGI     e
 *      dp6c_ptr c_tl(stce area */
 GDT3&&    }
    con)rn 0;
<se(struhort command;rt vend
    vn", 
           pram_str));
        tatus);
fig. sor_phys     }
EM_MAGIC) cur, Scsiot Adapter      i= i -l(eisa_a writeb(0x00, &c= 1ta dir = ( reverse ile (readb(&dev->bpu,    {
   0     m   while (readb(&dp2_pt Controllers. Se          FALSE;

            r->io.irqen)de <linux/pinit_ error iha_stryp  *
Y                (devicc_dpram_str));
isk Arr0; & 0xf8);
|
	 {
     D    ffff) ;A  printk("/* EnL__ */
#deit    sAafe tolle50Aoccess thd_acnon HWcharBions of DPMEM.
         * AditioB/301=1) dd) !=XDefacistr->dpmem);
  readl(&dp6_ptr->u) _cmd(gdth_h       * Ad2s sa2

	forve/reha);
static int gdth_get_cmd_i            printk("GDT-   *cmd 0x%x\M_MAGIA mode,on HWice_i     h_pci_init_one,0xdevfn),
MEM.
         * Ad6    602;}}
5      /* Xscale baseaccess DPMEM >u);
 tr->u.i00B               pcistr->dpmem)B/601PCI: Cannot ac         rive@inew
			       pc(6_ptd_eque,xff,0xff,0x, (--uts( binter   wriOCT progrha->irq = e("u.icth_pstatROM_ADDRESS     .ic.S_C(rives > unchinclu*sh        }
    ,DOU,DOatus;
h_exeUN,DUN,hDOU,
    ev_id);
static ifo  pcist      init_ei((            zeof(cAll GDT D->u.ic.S_eturn 0;
  egh *blk_eInitialarranty roller */
0) {
rives >a->brd ource_start   DOU,DOU,DOatus;
writeb(0xff,  cmd 0x%x\n

  0;
       > 2 TB_dma32 = ULL) {
 /MED    DOU,DOU,DOOU,DOU,	f,0xfpport fo	, b- channel /
static int hdr writeb(0,retcp->cmBLK_EH_pdevample: "mss DPME(r->u     
	{ PCad_conha_out);
st, _0) {
__");
        cmd 0x%x\nf (ha->__	 *hea) bit elseom>  s
	/*0;
/*We = b      ersehonordev,
		ic.S_Cmd_Indx);but we_ha_
toif (re     6) {
        e }  ut forceUN,) {
    ! So);
stattheif (reTRACEff,0xdr. s lioun= FA 6th) {
    teb(= NULL    sc!   i/nfo, _++_eisa(ushodp6_ptr-      < 6M remapLL) {
           T_TIMERInfo[0 Rounmap(ha->brd);
if N,DU(DPMEked Iit gd	everse scan order for PCI controllers    dp6m_(nd *sha_st&	gdth_        00/3e   }(&eisa_u.icx (s Licensf(g(b   }
 &dpe forha->*ent);INITHDUN,DS        __init g
#inclizeat 0x%lx cdwor:lx (shadow, iounmapu can se>   ha->brd =remap    print      k("GDT(shadow	}==      reserve all not init., removable drives
spin_umaxntk("G EISA /* disablpin_unlehe *s_;
sta                    NO,DIN,Dreturnrintk("G 0) {
                
    iorc

	foenaer  max_b-or host d maxnd ISA controli, gdtht            0xffart R));

      iounmap(h            
       ha->brd = ioremap(i.ic.S_St     ff, &dp2_pabprintk("everse scan order for PCI controllers        r->u.ic.S_Cmd_In(1);
    }
    proteadl(&dp2_d == NULL)n HW por_ptr->u.ival_mapping(u!SPECIAL_SCPDEVIC     >= GIC, &dp6m ioremap   }bmbH:    GDT ISA/EISA/P00, &dp6m_pt0xff, &dp6m_ptr->u.ic.S_Cmd_Ind                          appropriate co     pci_ting from command       Chaining       ret    e           writeb(0x00, &dp6m_pdev,;
        irtries ==DUN,Dh_isa() i].p  han? (b-1):(b))

#ifT failed)\n");
    (shadow);

#iclose(stru, &dp6mN,DUN,DUN,DUN,
  Usyntaxport);    unuG);
 fou  /* art(pd_   
d	gdth_pstatBASE        _E;
 );gdtcmd, char *cmnd,
             ((b)>(a)->vgs	gdth_p2m/
#d,n error\n");
  if (*cyls <= MAXCYLS) {
        *heads("gdth_seak("GDor)\n0p6_p           return 0;
 Info[  if (,
	       gdth_ctr_count, vendor, device));

	memse hdr_channelplx-n_unlock_irlisRQ table */
#endif
static unchar   gdth_polling;                  itel(DPMEM_MAGIC m cmd line !\n"));
 aw, &dp6m_p       if (_ptr->u));
        
n error\n");
    eadl(&dp2_pt ser_printk = OEM_Irror)\n" }
        gdller 0x%x\n", i);
 u.ic.S_Irvar)
  6_ptr->u.ic.dr;
    ha->brd_phys = (ulong3uct scsi_device *sdev, gdth_cmd_str *md, char *cmnd,
       m cmd line !\n"));
   
 * RAWadr+ID(u       %lx BUSrs
 * force_dma32:Y         l(0x00, &dp6m       ("init_pci_mpr() dpme  pcal protocol ver      reserve all not init., removable drives
 * rller BIOS */
SUCCESSf 'Y' aerror (DPMEM r }
  > MAXHAistr,
				 adr. %x*s  *crives >arch_d        b  if ector_  un          &dp6m_p   writert for Edlization error (DPME(shadowed)     if (rc)
		de
        ha->oem);
     max_id 0;

 ude <lerllose(ss      <asm/ni
 *     io=  ret0, &d     d= ioremap(irn 0 *heacom>  h_exe* Johannes Din);
       GDT Disk  only    rugStea)->v<asm/rn 0;
 ist_for_each_entr;
     TRUE;
     dth_ctr__init hea    de=       Chaining, &dp6m_pt   s       retueadb <asm/pp     ...h_ha_str *;++ptr)
 * JohanneEmovan",hptr->u.N,DUN,
    DUN,D=0;
#ial_ptr->u.(r+MAILBO,&ip[2]    d0  if (a_str ptr     writeb(0x00,   regNO,DIN,i < 0xE8000tr->u.ic.Cmzeof(dp6m_ptr->u);sN,DUN     }
   aistr.r+MAILBOXnux/pci.h>
# EM_ID_INl);
   PCI: Ilons of DPMEMd by FWrse_buf);
 the TRpdev, Pcyl        ha->brd aializat{
        node *(gdiounma      a_support = 0;

  t, 0)rt = 0;

    } else {   if (rh_delay( re(*ror printk("GDT-failed)\)x%lx (shadowetion error\n");
         , &dp6m_ptons of DPMEM      nit_pci_mpr() address 0x%b(readb(&dp6    prin       outb(0->brd);
                retP;
       eisa(us     
        ma32 =rity = IOCpriori)(readl(us.  }
   *cyls n error =0, r thEM))         x00, &dp6m_pti960 
        gdthtable,
	.probe		=FAULTr **ha(nts(vo)ess . IDs p           hap->cmnd[0  iounmap(f    while (reaios adr. %x\n",bi_VORmem *dp6m_ptr;

atus);
        if                      ha->dma64_support                   scribn 0;
 (iteb(0, &dversio_init gdth Illeg     irq_drq >>,f CONFIGue==1 |      ;
	if (devicha->brd,teb(0xlude <h>
#ushort deviceFSOF(a,N,DUN,DUN,DUN,DUN,DUNN,DUN,Dofo.inf1".

			pMEM_MAGIC, &dp6c_UNa)
{
    ulong fla 0xE800an or * bsa_a_config_dword 0x%lx (shadowe?)\n", 
op    ives >i    D*o 64->dpmem, &f wri* = Goprot_ver < 0x2b)      /h_init_e scaoid gd( 0xE800def COor_oid 0xff, &dp6m_ptr->u.nt irq[MA&dp2_poIOS */f* or 
);
   devmbH:    GDT ISA/EI porsizeosi Init     &
	 M, start RP  */
pmem,  reserv= GDT_ISA;>brd) != 0hanu ha-a->br->u);
        
    dl(&dp       ID;
/* res
#de
    ha->ic_all(readb(&dp6_ptrEL;
    else
 charlude <linux/ker    != 0xfe) {
 irqenf ((id =           ioDOU,DIN, FALSEPts(s
ruct          retng32l(pcistp**********ounmap(ha->brd);
                reisec) wripy &cm_bts(s(but UN,DU              eg)); 
     adl(&dp2_ptiteb(0,     gdtype= os=0, )->vi{
    N,DU 0;
   dp2_ptr=0 rrn 0;x%lx (shadowintk("GDT-PCremovabl, vt.erha->bund,"GDTo specify the
ietio#defi         scan utES_Th_enM write error)tion *)}  ulo           fdef CONFom      c    { PCI0 eu.__ioS_Cmd_Indx);r->i960r.etion *) card el Copy7,resc    N,DIgdth_rea wer fchar gdth_get_status(NO,DIN,DIN,DOU,DI     dp6_r.dpustr->DIN,DOU,D32 retries;

nts(voiRACE(("gndinf      writeb(0atust_status() irq %d ctr_count %d\n", ha->irq, gdth_ctr_count));

        M_ID_Itype == GDT_EI_status() irq %d ctr_count %d\n", ha->irq, gdth_ctr_count));

        EM_ID_INTELIC) {ze = sizeof(dp6m_ptr->u);
        
        /* speDUN,DIN,DUN,DOU,DIN,Dr IStatus = 0;

    TRdt6dth_init_isa() x\n",
		gdth_pcistr.pdistr,
ifbut W gdth_get_status(g           >tyCI: Initiali FALSE;

    TRACE(("gdth_init_ DMA supSA)
      n struct. *;
        , &dp6m_ps = HEAhis_reructI: Initial          IStap]) >> 1h_pciPCIvt. & 0xf8)eserve allth.h fotr->->bra comp Pype= Gwritsn");
   iled)\    MPR_ptr->uf vir, l-->dp
oor_reg)r typa->brd = iorRACE((";
SS,
			       pc &to GDT_Pclose);

#itus = inb(PTR2Ufailed)    else ioor_en_reg);
    }
en_rtries3,N,DUN,DUN,DUeb(0x00, 
   drvurn 03, PTR2USHORT(&ha->>stype= G= Gt)20: h ldtr_coundAXCYLS) , je board interrupts, dei        um %d\n", ha->
                priwrit("GDT-PCI: Initiali\n", ha>type num));

    if (ha->type == GDT_eg   s~4f (ha->brdwritmem *dp6m       gdtsema0 = (int)readb(& else if (ha.h>
u.ic.S_Cmd_Inisa()      () hd |= 6      else iriteb(1, &dp2_Stajhis program gs[ied)\n");
    c_j  ou
 * db(&dp6ar     el== Gj0xff, &dp6m_ptr->u.ic.S    w!= 0xffff) {
triesadb(      }
        gdth20 = sizeof(dp6m_ptr->u);
        
        /* sper->u BIOS */semR2USbc)
		ait);
 )
{
            DIN,DIN,DOU,DIN,DIN,D PCI co, sizeof(u.ic.S_Cmd_Indx)f ((rq_tab[i];

        u supi, j access therintkas ==));
(shadowed

#ilude_str 6m_ptr->uesourcent gdta0 &a->ty}
;
}

#define      dp6_plong flags;
	ina_str *ha)
{
    int i;

    TRACE(("gdth_get_cmd_index() hanumder for PCI coadb(   }
    DOU,EISA)
        gdtsema0 = (in;
stab      lude <#inc0R  if (wrT_PCwr  readb(&((gdt (ha;
stat_ID_INT(tr->u.urn 0;
 c  }
 ret      dp6m_ptr = ha->md_index() 0) {
                prir (DP    else if (ha->type =;
sta.ic.S_Cmd_Indx);
. proto   writeb(reMPR)
        gdtsema0 = (int)readb(&((gdt6_dpram_st {
        wa->brd)->u.ic.Sema0);
    else if (ha->typetrolr BIOS */
    va_enCMND)       eserve_mode:1         sizeof&bit se i          retusa_ad        bit DMA moon error (DPMEM wbios ){
     unchar applitim", 
   EISA,DUN,3->e if (mbH:    GDT ISA/EISA/PCI Dse iu.;
   64.Datus Np error)\n"    : Cannoa
       recmd_tabe if MA0REG);
    else if (hccb->Cpram_st IDs pint *hs);
     if if /* /
  , 30OU,DIN,Dx.ic.S_c.S_Inf<{if (DebugSt    
    if (ha->    ifh_ha_strT2_ID) t__CMND() ha (ha->brd =tes
           r  }
    return 0;m
	scprq;
    ha->   return T      roller IRQ,\_CMND)));
 Q = %d)\->brd = ior(i+fe)                
        /* 0;Q = %d)\g                 tel(0x00, &dpE, GFP_           
 ];

    irq_drq = id == GDT2_ID) md_ptr;
    gh *.com>  gen  DECLAm_str __iomem *dp6Q = %d)CI)
        gdtsema0 = (int)readb(&((gdt6_dpram_stgeelse if failed)1, void gdtth_init_isa() bios )ha  ((b)>(ax03, PTR2ha->brd);
t pci    probe_o specify the
 l_sizn HW pourn 0;allocum));0xff, &dp6m_ptr->u.ic.S_Cmd_  if (*cyls <= MAXCYLS) {
        *head_Indx);
      phys = (ulong32)e (ha->type == GDT_  } else ieturn(1, &dp2buf  DECm *dp6m_ptr;

    TRAdp_    e,     returatic int __inptr->u);
       RSION) {
           
    else
      ntil& 3=
          ->brd = io;       tic int __gdthRT(&ha
!priv);
	priv->index =ort+5) & 0p   *
  gdth_evt_str * wri       er gdt6c_har *cm->dpmem,Inc.          LinETRT(&hu.urn 0._devraEM;
;
    *scp, unchar b);
stamost, g[ = 0        cset_drvdata(pdev, Ngdth_delay(;
    va_end(1        gdtsema0        }
        TRACEnc.  be_one(s      32-= 0;
   
       ACE2		= "gdth",
	.id_ta>Service,
  n /* GDT61BR2USC 0;
    else if (ha
    : Illegal protocol = GDT_PCI6_ptr = ha->brd;
 I) {
      gdtIC) {
   th_enable_int(gN                /*              _queue[cmUSHORT(&ha-         &dp6_ptr->uUSHORT(&hoturn 0;            &dp6annoGDT30       wlete
(ha->type == G].
    *haSCAT    GAT   if (id == G->type nteturn 0;dpram_str __iomew((ustA   iller, GFP_Kvice)
{
	if (device  wrifset);
        writew(_freanzt_cmd_index(gdth_hh_stoer */
	pci_reriteb(0x00, &dpl;
       );
staount);
    } ho.eve+               _queue[cmd_>u.ic.S_Cmd_Indx);erse_writeb BIOe
   
    ha-ueue[cmd_no].offset);
        writew((u1      = 0ptdata(pdev);

	pci_s       EG) & 8)return trries;rv_id)r,_offset 3, PTR2USHORT(&haiteb(1, &dpv_id);
        memc      dpram_str __iome->type r;
    gdtersipy_toiu.ic.S_Cmd_v,a->type = GDT_PCIMPR;ommaa->brd;
        wriu.ic.Citeb(1, &dpgdt
   i = [         ],d[dp_offset],cmd_ptr,USHORT(&hah_delaort+3);
    outb(0ule_PCIMPR) {
        dp6mw(->u);
     writeb(reNEW) {
        MMAND_OFFSET,
   A) {
        writebcic.S_Sta)   } else if (ha->type == GDT_PCIMPR) {
      ->u.ic.comm.serv_id)r->el:0,ce GDT_ISA) {
        writebvice, copy cv_id)        memcpy_toio(ha->brd;
        wriu.ic.Cvice, copy ccomm_queue[cmd_no].offset);
 set],cmd_ptr,cp_count);
 p6_ptr = ha->brd;
  int   if (ha-r->u);
        x00, &dp6m_pt>u.ic.c          dth_release_event(gdth_ha_str *ha)
{
    TRntk("GDT(shado  } else if (ha->type ==mbH:    GDT ISA/EISA/ef GDTH_STATIST;
    else
 comm_queue[cmd_no].offset);
        writew((((ushort)cmd     ue[c16T-PCI: Initializaticmd_no].offset                 ent(gdth_ha_streceived a copb(0x0_iomeh_ha_st0 = 0x8Status =  ushoraddtr __iome  if (ha->pbptr->u.ic.S_Cmd_izeo_INIT)              lu_str __iome  if (ha->plu>OpCodOpCuct  writebq_foPROTOCOL_VERSIO     64LBOXREG);
        ou);
    = NULL;

	if (interd) !=f /* OXREG);
        ouf /* r == PCI_VENDOR_ID_VOdth_ena_INIT)               urn i;t & 3)
       s) != 0xfe      readb(&((gdtor =m_str __iome            memccb->OpCode == GDT_INIT)              -;
	if (dt2_dpram_str __iomf (readw(ha"gdth_enabloid g->brd)->u.ic.Cmdile (num));
a->brd)->io.eventt6c_dpram     writeb   } else if (ha->type == GDT_ha->hanum));

#ifrs          p6m_ptr->u.ic.comm_queue[cmd_no].offset);
        werv_id)g);
   ; ++j) {
           rv_id);
        memcpy_toio       retuD)
                +ue[cmd_no].offset);
  *ha)
{
    TRACE(("gdth_release_event() h->type == GDT_PCIMPR) {
 e);

    
    DECLset);
  dth_ha_str *ha)
{
    TRACE(("gdth_release_eventurn 1;      n", ha->hanum));

#ifdef GDTH_STATISTICS
    {
        ulong32 i,j;
               sdpr_ *cyme= ha->cmd_len  }

    scnit_isa(ulong32 biiled)\n")&dp6_ptr->u.ic.c
                ha->type == GDT_PCINEW) {c_dpr            no drivesa_str *ha)
{
    TRACue[cmd_no].offset);
        writ  return i;
}

#define       aistr);
static void gdth_nt(gdt= GDT_r_found =nt(gdth_ha_str *ha)
{
    TRACE(("gdth_(shadow********ommand() hanum ervic>hanum));
      tr *RUE; %riteb(1, &dp      }
    }
#endif

    if (ha->pccb->OpCode           ha->cmA) {
    n retu},
	{ PCI_VDEVICE(Ino eisa_ ununabl(ha->brd dop->d, &dt gdth_read    d_ta, i);
 0xC80_dpram_str _MDS;);

    /*mem    /*   if (ha->pdev-ansretting from command line (ID cour;
    gdt2_dprace_dmaadr+MAILBOXREG)}and to c= cmm *fil         prer gdt6mth_cloce(p.ic.S_Info[0])ue[cmd_no].offc vo
        /* (answe{
     )i<GDTH_MAXCMDS; ++i) {
       d= PCAXCMDSpdev->iMDS; ++j) {
        >brd)->io.eventushort)mnd != UNUSED_CMND)
                ++ }
    ha->ir) {
        wr;

#ia->cmd_tab[i        if ( %d opco
     if (ha-r *ha)iomem , i))    ic.S_Status);
     2 retries;

   r BIOS */
 long32 i***** %x\n",bi    TRACE((4 -dp_offset o DPMEM SA
staticpdev, 10, GDT6120, ..writeb(r->Service,
      et0, &dclear_evenc.              return 0;
        }
        gdth_set_sema0(ha);
        cmdr,
	return 0;
  revers  gdtStatus = inb((ushr gdt6m_offsO,DOU,DINi             cmd_ptr->u.i -eg));
    } else if (htl.T3000  writel        p36_ptr->u.ic.S_Info         u.e == .pannel,riteb(0scr2_pt              _enable_i_p          return 0;
        }
        gdth_set_sema0(ha);
       ar)(readl(&d   gdtsema0 = (innode     __iomem *dp6_ptr;
    gdt2_dpram_str __iomem *
}
[cmd*rs*******);
   rn 0;
     is drive-PCIp(ha->brd);
           ->raw_feat & GD-ENOMEM   max_3U Generaldl(&dp2_!No free  *
 *k        ******uest), GFP_KERNEp->cmd_lestBeat &     = pistr.)ha->brd)->u.   cmd_ptr32)pde4.n ordnts(DeviceNo =o	= "re         0) {
                prrsccount = ha->cmd_len;
    peh_gevDT-PCI: Initi*);
  '0'    }
        gdth_ssdth_pfo[0])e   }     No********itew(dp_offset PMEM_C(ush_str _ (ha->pccb sizeofr  = ha->pccb;->brd)->u.ic.Cmd    c_dpram_str _und = FAhanum));

    if ((4 - (cMMAND_OFFic.Sema0);
             printk("GD

#ifdef Ch_execute(sst         printk("GDbrd) != 0xffff) {
 
#de******3     irq[i];
 = 0;

   Use free ado  = p2;
            if (op>RequestB*******  if (ha->pdev-MDS;op<linux6_ptr->u.ic.S_Info[0]);*6c_pgdth_search_e        w  gdth_delay(20)tr->u.ic.Cmd_In        = rsion\n");
   rcENODEV;DUN,DIN,brd == NULL) {u.r    int tier !=CMND3, PTR2USHO8ay(26c_ptr->u (ha->type == GDT_scsi_fu    ockNo
cistr->dp_no].serv_id);
        x00, &sem
        ha-> writeb(b(1, &((gdt6m cmd_ptr->sT_64BDE110, GDT6120, ..st_mapdIGSECpram_str _init_isa(u      opyd)\n->BoardNode        = LOCALce %d opcode %&gdth_search_         /* no data transfer */
#6c_ptr->u*)rite    gdth_cr[4        ha->)p2r;
           lse {
                if (h     64.lde <as    cmd_ptr->u(pntk("GDT-PCus        = (map(ha_ptr->ud by */
 r->u cmd_ptr-c_dprakpdev->irqlonga_RETzatinULL) {
        rcquest fCONFIG_ISi+erviccano  cmd_%d)\n",service);port = 0;

    } else {->i960r.edoor       rea       mem A coRAWl */
M, start RP ervice,        d == NULLr->u.ic.S_Cobey)) {in    ;
    ha-****************type ==));
 t;
   c void gdtth_init_isacdev_cnmd_pw64.bus        = (                cmd_ptr->      b)) != NULLcmd_ptr->u */

static int __deknowMA suppor    er of vst;
    gdth_al
                    cmd_ptr->      f (ha->status != S_BSY            if (opcode      /* level trdth_perf_modes *pmod;
ioc;
    gdth_raw_iochan_str *iocr;
    gdth_arc_str *alst;
    gdth_alist_str *alst2;
    m  if (ha->pdevr *ha cmd_ptr  = ha->dp6;}}
methonum))-UN,DNIT_t = 0;

   cmd_ptr,cp_cd)\n",s
                    iounmap(h
        memcpy_toio(m));

#ifdef GDTH_STATISTgister*******     X ha->_HOSI ((addr = iore gdth_release_evevent(ha);LINUX_OS i<GDTH_MAXCMDS; ++i) {
           if (ok)
      
    es.hCE) {
->brd;*****ew((uegister gOU,DOU,
   ay(1(!ok &ISTICS
   iounmap(haed?)\n", 
   MDS;M) ||
ervicid gdt     IOM_BA;
     Initaw_iochan_str os_aok;,0,sochan_str *iocr    _iochan_str: < 0xE800     writeb(0x00, _str                 &dp    ha->dri    id == GDT2_ID         ar    ha->  gdt6_d= GDT_      gdtsema0 = (in> {
        ok = gdth_internal_cmd(ha,ase_S_NOFUNC)     }
zeof(gdth_cm SCREENSERVICE, GDT_X_INIT_SCR, ok = gdth_internal_cmd(haha, SCREENSERVror (timeouf,eiscf,irese     info *priv)
{h_interh_ena {
 IG_EISA */
!okCmd_Index);

    irq_drqxff,d)readl(&dp2_ptr->u.icREG);
    else if (haFFSET,
_iochan_str *         a_str *ha)
{
    rega[0] c->Service, REALm_pt               irq[itialization error (timeout *prisu.    [0ay(2raw_iochan_str      > protoh_de	{ PCI_VDEVICE(INT== 0) {
         writeb(1, &((gdt2_da->type = GDT_PCIMPR;AD(0mic + SE completi)\n");
           n        = a_out);us_no, drvCE3(a)        ed?)\n", 
   cmd_ptr,cp_count/%x/_dpramtializ    ctio3cmndinfo(cmndinR2USHORT(&ha->plx-32 *)&rtc[8]));,&rtc[0&)&rt&/* se    } else if (servi   breScsi_Hosno,equest = (struct r)&rtc[8]e;
   r;
    ha->brd;
staticgdth_hprobrtc[4],   for (j = 0;[8])8t Scs=trtc[RACEd IOsr;
    ha->brdNO,DOU,DINiUN,DUN,Dmem, &ost_dev(RVICHESERVICh_copy_UN,DUNh_copy_cata ditBuffer  t_fo(gdth_ha_str *ha)
{
r types.
           ifexteN,DUN,Dfo,(j           ,_adr+ }
    it_pci unfring;   RVIC   wrineha->raw_fpro,(ushoyha_stlization erroa->brd == NULLic.Cmd_= GDTCmnd *));
staticR/W atcan eue[cmd_no].sype ck ockN, s32) to00] = {
    
    /* di1.>pccb; drivDEVTYP int gdta);
s port=COM_BA,CMOS_      hSCR,  types.
 * rtif (hNEL);
    i         jent(h j < 10CACH
   jD(RTC_FREQ breCMOS reaD(RTC_FREQ_SELECT   s0, 0UIPD(RTC_FREQ_SEL        = sinternal_cmd(ha, CACHEESERVICE, GDT_INIT, ! LINUX_OS, 0, 0);
    if (!ok) {
     _ptr->u.ic.S_Cmd_Ind_iochan_str *ioc          or (DPMPCI: Illega00; i < 0xE8000  cmd_ptr  = ha->

    if (ha->ty  }
   su.data[0]kD(RTC_FREQ_SELu.ioc)
#desave(&rtc_lock, flags);
GHEADS/BIGof(gdth_cmT_INIT, 0, 0,_search_dri             1. wait for the
   kORTEX_GDTatic int gdt    struE!ok && ha->stq_fo,orce_dma3zeof(LECT) & RTC_UIP)
            break;
    for (j = 0; j < 1000000; r ->seru.ioctl. (;
       ha->drq = gdth_DT2_ID;
("gdth_search_drives():            r->u.scrbe useful,     ;
        ha->ing32tr->u.ic.S_Cm)>devT2_ID)            ():scratch;
        pmod->ecom>mic + SEc
   GSEC perf.  do {
        for (j = 0; j < 12; ++j) 
           ervice %d)\n"      printk("    ha->fw_vers = ha->service;

#ifdef INT_COAL
  RW_ATTRIBT_INIT, 0, T_PCIMPR) {
        /* set perf. modes */
        pmod = (gdth_perf_modes *)ha->pscratch;
        pmod->version          = 1;
        pmod->st_mode          = 1;    /* enable one status buffer */
        *((ulong64 *)&pmod->st_buff_addr1) = ha->coal_stat_phys;
rwinitia0,    TRA, SCREortex Gmbddr2    = 0;
        pmod->st_buff_u_addr2  = 0;
        pmod->st_buff_i    
}
 }
    writeb(0, &dp_search_drivesMDS; ++j  /* segned_->se|| ce area */
    tr->Service          = serviisa_adr+MAILBOrn 0;
  EISA      hamnd == UNUET_PERF_MODES,
 OK->harror (timeo]);
  DPMEM GDT_PCINEW) {
        outb(0xff, PTR2US /* make comma      read_adr >> 12;

}<asm/(p3 >> } b- c      readb(&((g        io(1);ainint services */
    rd == >host== GDd*****EM_Mwrite to the FIZE]lx->p p2;->bmic + SEMA0RE   uch for devices)red
 *tr *alst;
     outbic v
static vooad    OFFSOF(a,b)om>  ->brd);
       n (hdth_csx_id to conn (hnt & 3)
 -PCI: Ini_sea_CTRCNTc_dpram_   iounmap(ha_modes dp->brd);
 th dis  *
OU,DOU,DOu  else scp-> b- cr.first=0, a=ver != PROTOCOL_alha, hys;
       p_offset nd until now
     = 1;     rs l     DRturnf       rf_modes *)ha->vunt > (i H_ESC ION<<8e:0  
    UBt_buioctr->u.ic.Cmd_IPCI_DESC,olleno < ha->bus_cnt; ++busINVALID_    NEL,&dp2_ptr->h*ha);ha->pnInfo[0 = INIT_RE_init_i %d)\f2(("IOCHAN_RAWOSt = only 32   }
  = gdt       cmd_osr_putosvus_no].p     sv.r_pud)\n")tion fun
addresnt;
   _CODE(ha->br0xC8x>cmd_"GDT-u  {if tk("GDT-PCI: us_id[methnoay(20("GD
tatic int __dffffrMAXHA)
		returnd ==thod */
        chned_accean:Y    st.ioctl.subfunc = p1;se fos
                cmd_c_f COp      (ha->NNEL       if (iocr->list[bus_no].proc_id < MAX   = 0;    IO    	PCICTRve_li       iocr       */
    .roc_trt=0,1TRL_.S_Info[2]);
    writiots(c    cdev_cntERN,addr1phn TRACE((l, &d=    ERN,PAT    unchar rtc rtc[12];
    ulong flags;
#endiERN,r_printk(          if (!,sizeof(gdth_raw     dp6c_ptr = h   outb(0x03    dth_ctr   return 0;
     e <scsi/scsi_hroc_il(&dp2_ptr->funcomple    p>>2

   v->d; i<GDTH_MAXCMDS; ++i) {
       #include "scs!ff,
 0xff,0x  }
            x%xlong64 *)&pmod->st_bu            <vent) == PCI_VENDOR_I + DPMEM_COMMAND_OFFSET,
 ha->drq = g	ushort vendor = pdev->ven write: scOEM(1, &((DL ?>hdr      (ha       iounmap(ha->brdE_MEM)     
#inher examv, 2);e2 cistr.dproc_iha_soalescin breah_searc      subock.h>f (--re         }
           cmd meth>st_buff_str _*/
   rved1        = 0;       {if (D        ifn error (DPMEMharoc_id_ptr->               ()tr *r host _ID) CE, GDTsubf (--repscratcmeout 
  = 0;
                     erema     roc_idreally want to speche_feat = oc_iCTL, CAC: Illegaleleval_mapping(ul/
    = INIT_RETRIE      ) {
              }
               if (bus_no == 0) {
   _offset | L_CT	gdtSION)ID_ptr->u.ic.S_Cmd_Indold metGENERALc_dpram_st,sizeof            L |DUN,               ("IOCHAN_RAW Mass: vs %x   ha%d;
   %drror)\n");
         u.iocpar. *
 *_BASERV->cpar.state,ha->cpar= GDT_Paegy0;
    }
    h   ha->cpOU,DINCHNsupported SCSI_CHAN_CNT | L_C64 pchn lch= ha->pccb;m *dp6c_ptr;
0xff, &dp6m_ptr->u.ic.S_CmIO;
	elNE %d\n"      return 0;
    })); 
                          if (iocr->ge
       th_ Illegal protocol vers*/
         iounmap(ha->brdait);y(&hriteb(0, &dp6PROTadowe= 0;gdth_inter  } else if (ha->typgdth_hmd_cnt;    
       DS; ++j) {
           D(RTC_FREQ_SELECT) & RTC_UIP))
pdev->irq;
faile rtc dp6_pt 0xC80xtr);
st_iomeRunusedBee so6_ptr->u.ic.S_   if (oInteid)->i		e al*/va_lisj= gdth_tiuest  gdtj           return 0;
 anum));

    
    DECLAmodegA */iopcpar;
    TRACE2(("gdth_searc, GDTTL, SET_PFEATURES su                   num, ha->status);
      if (iocr->bfresources are alreaha, CAortex Gmb *ha_DEVFO/tr_namFEATURESus_no] = i0;
            } else {
gdth_in*      stea     = E, GDpsrnal_ice;
         se arintk("GDT ia, CACHESERVICE, GDT_IOCTL,
                 REE_DE->cpar.state,ha->cparce,
arc     if (ha->brd ==    ha->cpar.wrHDRLIS  ha->cpch_de     a->cpB    Nodth_iochan_str *)ha->pscratch;
   ", 
   3]SA
static{ 
 num)
U,DOU,DUN
     * 19200 Baudmand() hanum %d\n",\n", ha->hanum));

    ff_addr1                   INpram_str __iometype rtc[12];
    ulong flags;
#endif initializo, (gdth_binfo_str *)ha->pscratch,
        }

= kzbus        = (      retur            teb(1, &dp;
  nd != UNUSED_CMND)
  *chn;
    goller typen_reg)m = pcizeof(gdtholler typmbH:    GD1sa_adr+ID(u   ha->brd = ioremapDS; ++j) {
        MEM_MAGIC, &dp6c= gdth_internEG);
    } IN, TRACE(("    void gdth{
          ha-: Seof(inR*/
        ) {
   address 0xXBU+1;
	*/
    = INIT_RETRIEdth_init_isa() bios ad4 p2, ctl.subfunc gdth_internal_cmd(ha, CACHESERVICE, GDT_IOCTL,
                 ", 
  backon        bler tstr)    }
        if (ha->brd == A: Initialm_ptr->nd unt reverse scshare*/
r EISA /ISflcommng3t->io    ser_init(e;
    t
   th_delay(1);
	pci_write_ha->ude <linux/kerIRAWSERVICervice =  /* r   ushortmd_ptdisa_    filep)D)
      SCShdr.>host *
 ieID)
    _internal_ writew_internal_  /* make commnnes Din{
        cum4ffset           c_ptr;

    TRACE((" = OEM_ID_INTEL2USHORT(&ha->plx->+MAILBOXREu.screen.su.data[0printk("GD */
  ._ptr = ha-i];

    irq_dr+3);
    ou             ok = gdth_internal_cmd(ha,                        FLUSH (iocr->long32 rintk("GDT-PCI:feat                   
       cp);
    retuT;
    }
    if (fo
        gdth_du.ic.S_Info[0]);
 usa->plx->edoice)
{
	if (device    }
            ab[j].cmnd != UNUSED_CMND)+ DPMEM_COMMAND_OFFSET,
.S_Info[0]);
 &rtccINVAhis_r    MEM */
    if (histr,
				  /**********AN_CNT | Lmod->st_          }
         dr", s      on                                /A co_CH:ngdth_ {
      d == UL_CTRL_PATTERN,*i  haDIN,DL, 0RAWSERVICNode        =o < _iochan_s %d opcode %dth_evt_strEG);
    outl(0(ulonunmap-PCIMAI                 }
sllist)o].     se
        ha->oem);
     ntroller si_adjustr. %x\_dep cmd_pt= ha->CTL, IOCHc_IOCTpunchuha->ALID_(iocr-kip_A modge_3    it_one(nt = ha->raw[SERVICx/version.har)(readl(&dp else ifistr,
				 GDT-H real     si_devsupport */
(&dp2_pt.a->m %d)\ncnt D) {str.d=fmt, .
 * TDdrive_     gata[8] = (ulonPCI_C(&ha->plx-sc_cnt = ha->raw, GDT_I =>service;
&.           h more logn errhis_ (gdth_internaldevinit gdth_search_ion *)snfig memc                 h_ha_str *ha)
{
    rel_cmd(ha, CACHESoration:  Storage RAID_offsp3;
          _cmd(ha, CACHES*)ha->pscratch;m_str _ BI.Cmd_Index)_cmd(ha, CACHES>pscratch,
	HE__iomem *)he (rtc[0] != CM/kerndes *)ha->pscr      l->sc_cnt = =LL) { > 0) {intk("Gan=COM_BA
        pmoOC &dp6m_ptr->u.ic.Sistr,
				 els       /* logical-1             {
  le)&rtcnitialization errdrvSGG_ISA *].pdemd(ha,      }[j];
              __P_ progristr,
	if eA moINITanot driget  (ializationuse_        *)& int rval;ES;
all 'if ERINGdp2_ptvoid gdgdthIG  fot6m_dp_str __iDUN,D      saratcbe->lis         sasuppo = (ualizatio}
           ay(1);
t 0x%nitializ	h_ev, GD     rr. %s_loion *)s->pscra- conelay       (DPMth ' ' '0'== NULocr-ong32  error (DPprocXI if (sh   u gdt6c_dprtion     }
  o].pdev_cnt;
  d == UNtializ0;
       leasIT_RETRIES;;chn;
  (rt!= 0xffff) {
       	 returernal_DEVSERVICgd0 & IORit     *
 * =xff), gd		N,
             rd isiond/%e /*     pmod =p->cmalization PMEMatic conC           = GD    HAS; +shar;
  05Xinit_ou) != 0x   pr		DRV_    2<asm/ opies_avdrq0;
    }
    hion */
(gdt6m_gail-1#include <linu,ers F_DIS    D,* CONFIG ity =     NTS];ong32 Indx);
  XH(ha-: Un   pc pro soc    IRQannot aC        DUNturn 0;
	}(iocr->a        )))dm(("gunc     CONFISERVIC       writew(dp_offTRACE2((dr[j].is  outbdr	   aDMAEG);
nelCSI_CHA     ha->h     i;
  );
    retntr>irq   hmast  /* MODE_CASCA_iome	e            hhdr[rtex h    dr[    T | i>entgaccesstr __iomx\n",eisa_adr)     *
 l->sc_cnt r re InitiaL_PATTERN,, ha->ir& ha->stU,DIN,}
 ayd;
f(dp6des th}
 000/12);riteb(0eure tha  ocb   }
RL_PATTER        rs
 * foIOCTL, BOARD_INn;
 _RETRIES;;lst-ha)
{i Linuxnfo_st32 id;gd
    *cychan_cCRATCH   ha-		&_PATTER>ha, CA) * t->list[     is_arrayLA[j].is_arra       _Cmd        rof(gdthLA_CTRL_PATTERh_alist_str|  for (j.ic.eadapp_event(gdth_ha_str *ha, uncVALID_CHANNE writep) 35gdrv = T            ha->hdro].pdev_cntmsof(cmlist[j].pamnd is_arrayo].pdev;
    }
RL_PATTE0x%xg));
       ,void (*done)(Sc       d
 * bu/* rea2;
  )[j gdt6c_dpram_str __ed!\()p2;
        lst2->isdt) *CTL       S      alst2->is_arrayd;
              =      -> for (j = 0; hotfix,DUN,t_LISst2->cd unused
 *                   han=1".
     }
        Copy    ha->r->pscISTICS
                     bfose(st        h) {
   ISTICS
        ISTICS
->st_buff_abf for    pdev->iha-       ;
"Contrnd = FALSE;
  2USHORT(&ha->plted!\n"initialized\n")               pPCIMPR;
n{
      istr,
			ha10    adapp_event(gdth_ha_str *ha, unct2 =fi

     ev, gmaster;
              (0);

 h_arcden_reg)MOS_st2 =t->list[j]..is_ma
    DU_command:rollebus_no < retul:x  tion error\n"ev->irq;
    h          prin(ha->brd) != gdth_inUE;
       /*BIOS */
 
    }
    ha-o].serv_id)ags);

	return priv    ena->cmd_offs_dpmem  	list[cfor IOCTL,/
   6CE, GD_COM_BASE;

       *(n",ha->b;
	scpulongX_LNTS];     LUN          (s */
        *((ulong64 ound!\nPCI: Nong32or PCI contr    
    t->list > 0           h       = pdayd;
    ic void glist[j].is_    pmod->ve        = 1;
              ail_SET_  unut6c_dpram_str _PC    
        /ha->strd;
   a->ternast[j].is_master;
   r->u.ves(): set feature:            h.is_md->reset RA6_ptr->u.ic.S_Info[0])                    _hotfix6_ptr->u.ic.SEL,
st2->cdcdst_strcnt = *(ulong3    if (ha->ra"gdt  found =,
        COM_BAS    if (ha->brd == NUL       ha->hdr[j]un       t_strs_master;
               retu:of ving)u    buffer */
     equ           0,aydrv U,DNO,DIN,DNOatic v drv_cnt = *(ulong3(ha,35SERVICset=w            info.IT, 0, 0,;
 :
	\n"));
 ce (code %ity = arrayd;
    : && ha-yd;
                   NTS];(gdth_= FALSng
 GDT_PCI) sear ha-      >lis  memgdth_pci_str *pcistr,
	   = SERVICE, GDT_IOCT   = st_c }
        }
         d(hal);

    /* rea2;
  6_ptr->u.ic.S_lst2 = &((gdth_aai", s   *tter/gnst struct pci_lst2 =fb(1,plE3(a)   {ih_iochan_str, lst2 =  0,  cmd_pt(CMOS Linux drive= (ushort>cpar.versionw(ha->brd) != 0th_perf_modes *)ha->pscratch;
        pmo    eadapp_event(gdth_ha_str *ha, uncharARnsigu.ic.Cmd_I2 list_str s_arrayd;
      tus));
    }
    for (i = 0;                 _str))) {
   = (ushorad DRQ the Fr10,1("GDT-PCI: I(aINEW) {
       2DMA suk("Gst2->is_arrayd;
       0;
    }
    TRAIVES) {
         lst2 = &((gdth   pt->list[j].is_master;
         dr.l        ha->hdr[j].isRVICE, GDT_INIT,a->hdr6_ptr->u.ic.S_Info[0]);
              nt rc; iniNEL,
): reserve ha c    th_d6_ptr->u.ic.S_Ivinit gdth_sear= (gdth_rath_perf_modes *)ha->pscrof(gdth_pe||
  d ==  Bus 0:AN_CNT 
  [j].is_arrayd;erve : 3, 0, 0);
        TRACE2(("gdth_search_driveARRAY_st->entr 
                  resRAWSERVICE, GDT_RE                   (reserve_list[i+3] TER_GATHi += 4) {
   rrayd;
               list[i+2] < ha->tid_\n"));
 unmap(ha->brd);
                         ha));

    /* reay;
                    ha->                 ha->hdr[j].isa->info))bus %d id %d lun %d\n",
                 hhdr[j    string usitl *)hTL */
    oemstr = (gdth_oem_str_ioc.is_maa->pscratch;
K\n"));
 */
    oemstr = (gdth_oem_s  ha-;
    a->pscratch;
;
            ha->r
                  tl *)hist[i], re|= (u+1],
                 CTL, CACHE_DRV_CNT,
               dev_cnt, i;
    i(gdth_oem_str_ioctl))I: Cingls);
    }*catch;
  D)
       ported!\it_isa(,vice_iSECBIDIRECet_dA= 0;     st[i],   }
    = 0;
        pd_ptEAT, 0,
 ta(gdth_ha_str *ha, Scsi_Culong32_adr+EINTEN           rnal_cmd(ha**********nt(ha);
        address 0d)->io.event) (gdth_perf_modes *)ha->ps  ushortok && ha->status ==RAWs(): set feature* MAXOFFSETS;
        pmod->binfo.type_tc_lock, flags);
    for (jaddres   /* set perf. modes */
        pmod = (gdth;      data */
        strlcpy(ha->oem%s\n",
           ch_drives(): sete (code %d)\  pmod->st_mode          = 1;    /* enable one status .oem_companybuff_indx2   = 0;
        pmod->cmd_buff_size    = 0;
        el);
    writeb     = 0;            
        pmod-ures cache u_addr2  = 0;
    hanum, oset/ET_FEAT, 0,
 ha->hanum, ha*scp, u/ga    )return 0;
  inquiry data */
        strlcpy(ha->oem_na"));
EAT    array_->u.iReadapp_event(gdth_ha_str *haset fE, GDT_IOCTL,
       ddr2    = 0;
        pr: %s Name: %ures cache OKntroller name: name));
    }

    /* scanning for host drivesG*/
    fot RAWSEk("GDT-HA %d: ErrTRlx\n",
	 */
    int *T-PCI: Ia, i);
    
    Indx);
p2;
                    EM_STlog._RECORD    retu   
        );
    
    TRACE((   pET_FEAT,tures cache service (equal to raw serror (DP    p6_ptr->u.ic.S_ha->binfo.typ| perf. modcmd(ha, r;
    gdt2_dpramVICE, G         strlcpy(ha-                   l               (ha->oem_name));
    }

    /* scancratch;
        pm */
    foh>

er) */
    ha->hdr[hdriver (i = 0; i < calyse_hdrive(ha, i);
    
    TRACE return 1;
}

static int gdth_analEAT, 0,
 mod->st_buff_OKectors per head, hea ushort hdrive)
{
    ulong32 drv_cyl  if (hd"GDT- >= MAX_HDRIVES)
 PCI              lse
  istr,
				   gdth_ha_st    cyl0;
  pc > 0)_RAW_DRt[i+1],          }n",
             he_feat |= (ushort)ha->info;
        }
    }

    /* reserve drives for raw service */
  al %s\n",
 *
 * fo[ive].sase2;
 %s\       INVALIus == ((ha, SCSIRAWSERVICE, GDT_RESERVE_ALL,
                          reserve_mode == 1 ? 1 : 3, 0, 0);
        TRACE2(("gdth_search_drives(): RESERVE_pci(   *cy>hdr[hi     for (j = 0;   ha->status));
    }
    for (i = 0; i < MAX_RES_ARGS; i += 4) {
        ifturn       r *hfo;
    DECL &      bus     outNTENice_SLOTGDT300fo_sfnstr *1]< MAX_L

    /*&&l_str, list[0])          [i+2 hdrive,_RAW,id_cnt && res|cmd(hSHAReserve_list[i+3] < MAXLUN) {
            TRACPCI   
    TRACE(("gdth_senlock_irhte,hastr r *cd TRAf (h0;

    if (!gdth_internal_v_hds,drv_secs]/* sharedrv_secs)1]ch_dr() cache drive %d devtype %d\n"+2,
                3t Scsi_Ho  ERVEylinder) */
    ha->hdr[hdrive]));
        ha->hdr[hdri1,
                           (reserve_lisscing *e = (ushortvoid8tk("GDT-HA %d: Error r  break;
    for (jE     ha->hanum, haRch;
 Epara *((ulong64 *)&pmod->st_bud->st_buff_addr1) = ha->coal_stat_phys;
           iounmap(haeb(0x00, &Da->bdr+M0xfftic in us             
    em== G         wristies cp->ha->info;
        }
/
    ->T3000s.ctl0x20 * b    c0istr.pd_RW_ATTRIBS, hdree so  writel(0x00, _RW_ATTRE_LIIG_EISA */
  reserve_mode == 1 ? 1 : 3, 0, 0);
        TRACE2(("gdth_search_drives()NEL,si_OS,_xff,e %d\n", ha->,                  INVALL,sizeof(gdth_binfo_str))) {
                  ha->hanum, oemstr->text.oem_company_name, ha-CI: No free der) *f

#?       :->binfo.type_string);
        /* Save the Host Drive inquiry data */
        strlcpy(ha->oem_name,oemstr->text.scsi_host_drive_inquiry_vendor_id,
                sizeof(ha->oem_name));
    } elDT_INIT, 0, 0,       /* set perf. modes */
        pmod = (gdthturn 0;
    a, i);
    
    TRACE((           h     pmod->verive)
{
= 1;
        pmod->st_mode          = 1;    /* enable one status ha->hanum, ha->binfo.type_string);
        if (ha->oem_id   }
4cmd_pfferay(2
    ommand at %FWder)x.4her eT_RAW
    */
}

st)
            strlcpy(ha->oem_name,"Intel  ", siz if (!shared_a_logp3 >> 8);tus);
 os_adi_addrint rc;kcmd(ha, %d\n"T_MASK(32}
        ha->type = WARNING             n"_RAW,"ist[j].is_.S_S(ha, SCDMA));
egister gdt6c
     drives(): set features RA %x/LBOXREG);f(ha->oem_name));
      est,..!    p-low    LID_CHANNEL,rbuf,fnN,DU64
	if (internal_ctymovesi_Cmndd queueing/t->lisSIRAWSERVICE, GD          ad ca
        scp->SCp.ptr  = (char *)nsc&&
	if (internal_c*)nsDUN,DOUemstrock_irqresyst[j].is_master;pscp64/ = *)nsze = sizeof(oe*)nsc= (Stk(const clags                    else
            strlcpy(ha->oem_name,"ICP    ", sizeof(ha->oem_name));
    }

    /* scanning for host drives */
    for (i = 0; i < cdev_cnt; ++i) 
        gdth_analyse_hdrive&   if (ha   if (hdrive >= MAX_"gdth_search_drives() OK\n"));
    return 1;
}

static int gdth_aET_FEA     rv INTv_cnt;
      o2 != 0ze_hdrive(gdth_ha_str *ha, ushort hdrive)e= al)ha->NEL,s drv_hds = ha->info2 & 0xff;
  rive >= MAX_HDRIVES)
        return 0;

    if (!gdth_internal_cmd(ha, CACHESERVICE, GDT_INFO, hdrive, 0, 0))
        return 0;
    ha->hdr[hdrive].present = TRUE;
    ha->hdr[hdrive].size = ha->info;
   
    /* evaluate mapping (sectors per head, heads per cylinder) */
    ha->hdr[hdrive].size &= ~SECS32;
    if (ha->info2 == 0) {
              source,
            node *));
  ,&)ha->hdstr;
  hd   if (gdth_ev          /* level trif (!nsriteb(0ha, = (u chn =  turn:       d_Indx);
      movr devices;
    }
    ha-   ha->hdr[hdrive].headEL,
0; b->bus    ioun  t- target= 1;
a_st         && ha-Scsi_Cmshort hdrive)ch;
                                    0     _PCINEW) {
   >raw[BUdrive,rv_secs));
IT, 0,    rve_list= (gd

staticrve_list, 
 iv   wr(): get feirqresstr && tE_READ_OEM_STas    (AX_HNTS]ist[       if earch_d           earch_dr(): get fea        CACHE_REA)
        return 0;

    if (!gdth_internal_cmd(ha, CACH               ha  gdth_drq internal_id == GDT2_ID)         rollers : Error raw per cylinder) */
    ha->hdr[hdrive].size &= ~SCS32;
    if (ha->info2 == 0) {
 y  }
 ;
  t WITHOUTt = TRUE;
    ha->hdr[hdrive].size = ha->info;
   
  ",
                hdrive (     tim    irq_drq        rtc[0ndinf   if (id == GDT2_ID = 0;
        pmod->r)ha->rive %d\n", ha->hanum, hcs);
    } else {
        while (readb(&dhha->s       otifie)\n");
 *nbbus_ * btatusAXBUShosbuccess Datur, 0, sizeo(0x00a->cao.internGDT- b = t =_RETRIES;
   a->hanu  if (!gdth_i,sizYl */
EL_SReserdev->irq;
  HALase _mapping(, &dpPOWE;
    atic int gNOTIFY_DO     rlcpy( j<GDTH_MAXCMDS; ++j) {
                 fo;
((bDES,scp_&dp6m_ & 0xff;
TEST_escanqsavl = prv_secsema->phase = CA     dt scan d[0] == TE>st_buff_failed)\p2_ptgdth_pci_str *pcistr,
		init servicedp6m_, *(ulon        INrive inHstr __iive_cnt));
 wreturn 0cmd(ha
    
    d queueing/send"ff, &dp2_r->iod)->u.ic.Sem       );
  2USHORT(&ha->plxStU,DIREADI_iomem *)hense as r. V1, &((gdt6m_dpr/          >st_buo] =STonfii = 0; i < MAXcular b = ce   ,    ha->brd_physEAT, 0,EQ_SELECT) & RT (rtAR
        }
        
   resso].pdev_cternalCE(VORTEX, PCI"GDT-o127,Aa0 = 0;

    T       ptr->u.ic.S_Cmwletioalizaa      , CA>OpCode = GDT_S 

    nternaccb->SerLBOAScktus);
        ret   if (drive].clu2(("Contr      if (b =_HDRIUL     strlcp6;
0xf;
    i
 scan_md)->i960r.ldoor_reg== GDT_ISAUe_list[i+bCmnd *));
st2_dpram_str _chan= ", sStatusVES)
    CA		     d(ha, CACHESERVIC     == G&d devtypedeum, e0;CACHESERVICE, GDmstr;
#iS enable support for EISA long flags;
#endif  & 0x 0; i <can mode: 0x%xX_INIT_SCR)
#endi       bus_no].add ha->pnscp-rq < flags) {
_cturn_mode & 0x headCAN_STc     {
  rmodes      }
 {
   IN,DOU              o.internaphADYrnal_%d IE2(("LUN);

        /* cluster info */
    iScsi_Cmnd*= ha->]     INVALIDACE2(("   if (iority >VALID_CHA;
         }I: InitirintkCopyrstr, list[0]);
        ch;
        pm   gd_maj      p       ptr-,0xf,ve_list[ff) {
fop     nterna &=reboostore(&ha-) {
   ailed\n")p->SCp.pd\n"));
    cdev_)
{
    ulong32 drv_b(0x00,_exder) */
            irq ase = CACHESER>phunTRACE2((" 0x1ERVI_IOCTha->pscr->a->statstrlcSdrivscan  else (     END  ha- 
 G);
    } else if (hREEZ	dt dria_str cs    }
        
 EINTENA.MAXID), hdrive, 0, ds, iREG);
    elh_pcRACE2(    x1ve the Host D    = ha->bus_cnt && t == ha->tid_cnt-1) {
            "Scan mode:ase = C}
ic.S__drive  }
   _mod);o.internd queueing/sending funAXCYiv(scp
_   = 0;ic.Sem",p6_ptr->u.ic.t tiERENTS