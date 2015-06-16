/*
 *    Disk Array driver for HP Smart Array controllers.
 *    (C) Copyright 2000, 2007 Hewlett-Packard Development Company, L.P.
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; version 2 of the License.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *    General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *    02111-1307, USA.
 *
 *    Questions/Comments/Bugfixes to iss_storagedev@hp.com
 *
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/delay.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <linux/bio.h>
#include <linux/blkpg.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/hdreg.h>
#include <linux/spinlock.h>
#include <linux/compat.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include <linux/dma-mapping.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>
#include <linux/completion.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>
#include <scsi/scsi_ioctl.h>
#include <linux/cdrom.h>
#include <linux/scatterlist.h>
#include <linux/kthread.h>

#define CCISS_DRIVER_VERSION(maj,min,submin) ((maj<<16)|(min<<8)|(submin))
#define DRIVER_NAME "HP CISS Driver (v 3.6.20)"
#define DRIVER_VERSION CCISS_DRIVER_VERSION(3, 6, 20)

/* Embedded module documentation macros - see modules.h */
MODULE_AUTHOR("Hewlett-Packard Company");
MODULE_DESCRIPTION("Driver for HP Smart Array Controllers");
MODULE_SUPPORTED_DEVICE("HP SA5i SA5i+ SA532 SA5300 SA5312 SA641 SA642 SA6400"
			" SA6i P600 P800 P400 P400i E200 E200i E500 P700m"
			" Smart Array G2 Series SAS/SATA Controllers");
MODULE_VERSION("3.6.20");
MODULE_LICENSE("GPL");

static int cciss_allow_hpsa;
module_param(cciss_allow_hpsa, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(cciss_allow_hpsa,
	"Prevent cciss driver from accessing hardware known to be "
	" supported by the hpsa driver");

#include "cciss_cmd.h"
#include "cciss.h"
#include <linux/cciss_ioctl.h>

/* define the PCI info for the cards we can control */
static const struct pci_device_id cciss_pci_device_id[] = {
	{PCI_VENDOR_ID_COMPAQ, PCI_DEVICE_ID_COMPAQ_CISS,  0x0E11, 0x4070},
	{PCI_VENDOR_ID_COMPAQ, PCI_DEVICE_ID_COMPAQ_CISSB, 0x0E11, 0x4080},
	{PCI_VENDOR_ID_COMPAQ, PCI_DEVICE_ID_COMPAQ_CISSB, 0x0E11, 0x4082},
	{PCI_VENDOR_ID_COMPAQ, PCI_DEVICE_ID_COMPAQ_CISSB, 0x0E11, 0x4083},
	{PCI_VENDOR_ID_COMPAQ, PCI_DEVICE_ID_COMPAQ_CISSC, 0x0E11, 0x4091},
	{PCI_VENDOR_ID_COMPAQ, PCI_DEVICE_ID_COMPAQ_CISSC, 0x0E11, 0x409A},
	{PCI_VENDOR_ID_COMPAQ, PCI_DEVICE_ID_COMPAQ_CISSC, 0x0E11, 0x409B},
	{PCI_VENDOR_ID_COMPAQ, PCI_DEVICE_ID_COMPAQ_CISSC, 0x0E11, 0x409C},
	{PCI_VENDOR_ID_COMPAQ, PCI_DEVICE_ID_COMPAQ_CISSC, 0x0E11, 0x409D},
	{PCI_VENDOR_ID_HP,     PCI_DEVICE_ID_HP_CISSA,     0x103C, 0x3225},
	{PCI_VENDOR_ID_HP,     PCI_DEVICE_ID_HP_CISSC,     0x103C, 0x3223},
	{PCI_VENDOR_ID_HP,     PCI_DEVICE_ID_HP_CISSC,     0x103C, 0x3234},
	{PCI_VENDOR_ID_HP,     PCI_DEVICE_ID_HP_CISSC,     0x103C, 0x3235},
	{PCI_VENDOR_ID_HP,     PCI_DEVICE_ID_HP_CISSD,     0x103C, 0x3211},
	{PCI_VENDOR_ID_HP,     PCI_DEVICE_ID_HP_CISSD,     0x103C, 0x3212},
	{PCI_VENDOR_ID_HP,     PCI_DEVICE_ID_HP_CISSD,     0x103C, 0x3213},
	{PCI_VENDOR_ID_HP,     PCI_DEVICE_ID_HP_CISSD,     0x103C, 0x3214},
	{PCI_VENDOR_ID_HP,     PCI_DEVICE_ID_HP_CISSD,     0x103C, 0x3215},
	{PCI_VENDOR_ID_HP,     PCI_DEVICE_ID_HP_CISSC,     0x103C, 0x3237},
	{PCI_VENDOR_ID_HP,     PCI_DEVICE_ID_HP_CISSC,     0x103C, 0x323D},
	{PCI_VENDOR_ID_HP,     PCI_DEVICE_ID_HP_CISSE,     0x103C, 0x3241},
	{PCI_VENDOR_ID_HP,     PCI_DEVICE_ID_HP_CISSE,     0x103C, 0x3243},
	{PCI_VENDOR_ID_HP,     PCI_DEVICE_ID_HP_CISSE,     0x103C, 0x3245},
	{PCI_VENDOR_ID_HP,     PCI_DEVICE_ID_HP_CISSE,     0x103C, 0x3247},
	{PCI_VENDOR_ID_HP,     PCI_DEVICE_ID_HP_CISSE,     0x103C, 0x3249},
	{PCI_VENDOR_ID_HP,     PCI_DEVICE_ID_HP_CISSE,     0x103C, 0x324A},
	{PCI_VENDOR_ID_HP,     PCI_DEVICE_ID_HP_CISSE,     0x103C, 0x324B},
	{0,}
};

MODULE_DEVICE_TABLE(pci, cciss_pci_device_id);

/*  board_id = Subsystem Device ID & Vendor ID
 *  product = Marketing Name for the board
 *  access = Address of the struct of function pointers
 */
static struct board_type products[] = {
	{0x40700E11, "Smart Array 5300", &SA5_access},
	{0x40800E11, "Smart Array 5i", &SA5B_access},
	{0x40820E11, "Smart Array 532", &SA5B_access},
	{0x40830E11, "Smart Array 5312", &SA5B_access},
	{0x409A0E11, "Smart Array 641", &SA5_access},
	{0x409B0E11, "Smart Array 642", &SA5_access},
	{0x409C0E11, "Smart Array 6400", &SA5_access},
	{0x409D0E11, "Smart Array 6400 EM", &SA5_access},
	{0x40910E11, "Smart Array 6i", &SA5_access},
	{0x3225103C, "Smart Array P600", &SA5_access},
	{0x3235103C, "Smart Array P400i", &SA5_access},
	{0x3211103C, "Smart Array E200i", &SA5_access},
	{0x3212103C, "Smart Array E200", &SA5_access},
	{0x3213103C, "Smart Array E200i", &SA5_access},
	{0x3214103C, "Smart Array E200i", &SA5_access},
	{0x3215103C, "Smart Array E200i", &SA5_access},
	{0x3237103C, "Smart Array E500", &SA5_access},
/* controllers below this line are also supported by the hpsa driver. */
#define HPSA_BOUNDARY 0x3223103C
	{0x3223103C, "Smart Array P800", &SA5_access},
	{0x3234103C, "Smart Array P400", &SA5_access},
	{0x323D103C, "Smart Array P700m", &SA5_access},
	{0x3241103C, "Smart Array P212", &SA5_access},
	{0x3243103C, "Smart Array P410", &SA5_access},
	{0x3245103C, "Smart Array P410i", &SA5_access},
	{0x3247103C, "Smart Array P411", &SA5_access},
	{0x3249103C, "Smart Array P812", &SA5_access},
	{0x324A103C, "Smart Array P712m", &SA5_access},
	{0x324B103C, "Smart Array P711m", &SA5_access},
};

/* How long to wait (in milliseconds) for board to go into simple mode */
#define MAX_CONFIG_WAIT 30000
#define MAX_IOCTL_CONFIG_WAIT 1000

/*define how many times we will try a command because of bus resets */
#define MAX_CMD_RETRIES 3

#define MAX_CTLR	32

/* Originally cciss driver only supports 8 major numbers */
#define MAX_CTLR_ORIG 	8

static ctlr_info_t *hba[MAX_CTLR];

static struct task_struct *cciss_scan_thread;
static DEFINE_MUTEX(scan_mutex);
static LIST_HEAD(scan_q);

static void do_cciss_request(struct request_queue *q);
static irqreturn_t do_cciss_intr(int irq, void *dev_id);
static int cciss_open(struct block_device *bdev, fmode_t mode);
static int cciss_release(struct gendisk *disk, fmode_t mode);
static int cciss_ioctl(struct block_device *bdev, fmode_t mode,
		       unsigned int cmd, unsigned long arg);
static int cciss_getgeo(struct block_device *bdev, struct hd_geometry *geo);

static int cciss_revalidate(struct gendisk *disk);
static int rebuild_lun_table(ctlr_info_t *h, int first_time, int via_ioctl);
static int deregister_disk(ctlr_info_t *h, int drv_index,
			   int clear_all, int via_ioctl);

static void cciss_read_capacity(int ctlr, int logvol, int withirq,
			sector_t *total_size, unsigned int *block_size);
static void cciss_read_capacity_16(int ctlr, int logvol, int withirq,
			sector_t *total_size, unsigned int *block_size);
static void cciss_geometry_inquiry(int ctlr, int logvol,
			int withirq, sector_t total_size,
			unsigned int block_size, InquiryData_struct *inq_buff,
				   drive_info_struct *drv);
static void __devinit cciss_interrupt_mode(ctlr_info_t *, struct pci_dev *,
					   __u32);
static void start_io(ctlr_info_t *h);
static int sendcmd(__u8 cmd, int ctlr, void *buff, size_t size,
		   __u8 page_code, unsigned char *scsi3addr, int cmd_type);
static int sendcmd_withirq(__u8 cmd, int ctlr, void *buff, size_t size,
			__u8 page_code, unsigned char scsi3addr[],
			int cmd_type);
static int sendcmd_withirq_core(ctlr_info_t *h, CommandList_struct *c,
	int attempt_retry);
static int process_sendcmd_error(ctlr_info_t *h, CommandList_struct *c);

static void fail_all_cmds(unsigned long ctlr);
static int add_to_scan_list(struct ctlr_info *h);
static int scan_thread(void *data);
static int check_for_unit_attention(ctlr_info_t *h, CommandList_struct *c);
static void cciss_hba_release(struct device *dev);
static void cciss_device_release(struct device *dev);
static void cciss_free_gendisk(ctlr_info_t *h, int drv_index);
static void cciss_free_drive_info(ctlr_info_t *h, int drv_index);

#ifdef CONFIG_PROC_FS
static void cciss_procinit(int i);
#else
static void cciss_procinit(int i)
{
}
#endif				/* CONFIG_PROC_FS */

#ifdef CONFIG_COMPAT
static int cciss_compat_ioctl(struct block_device *, fmode_t,
			      unsigned, unsigned long);
#endif

static const struct block_device_operations cciss_fops = {
	.owner = THIS_MODULE,
	.open = cciss_open,
	.release = cciss_release,
	.locked_ioctl = cciss_ioctl,
	.getgeo = cciss_getgeo,
#ifdef CONFIG_COMPAT
	.compat_ioctl = cciss_compat_ioctl,
#endif
	.revalidate_disk = cciss_revalidate,
};

/*
 * Enqueuing and dequeuing functions for cmdlists.
 */
static inline void addQ(struct hlist_head *list, CommandList_struct *c)
{
	hlist_add_head(&c->list, list);
}

static inline void removeQ(CommandList_struct *c)
{
	/*
	 * After kexec/dump some commands might still
	 * be in flight, which the firmware will try
	 * to complete. Resetting the firmware doesn't work
	 * with old fw revisions, so we have to mark
	 * them off as 'stale' to prevent the driver from
	 * falling over.
	 */
	if (WARN_ON(hlist_unhashed(&c->list))) {
		c->cmd_type = CMD_MSG_STALE;
		return;
	}

	hlist_del_init(&c->list);
}

#include "cciss_scsi.c"		/* For SCSI tape support */

static const char *raid_label[] = { "0", "4", "1(1+0)", "5", "5+1", "ADG",
	"UNKNOWN"
};
#define RAID_UNKNOWN (sizeof(raid_label) / sizeof(raid_label[0])-1)

#ifdef CONFIG_PROC_FS

/*
 * Report information about this controller.
 */
#define ENG_GIG 1000000000
#define ENG_GIG_FACTOR (ENG_GIG/512)
#define ENGAGE_SCSI	"engage scsi"

static struct proc_dir_entry *proc_cciss;

static void cciss_seq_show_header(struct seq_file *seq)
{
	ctlr_info_t *h = seq->private;

	seq_printf(seq, "%s: HP %s Controller\n"
		"Board ID: 0x%08lx\n"
		"Firmware Version: %c%c%c%c\n"
		"IRQ: %d\n"
		"Logical drives: %d\n"
		"Current Q depth: %d\n"
		"Current # commands on controller: %d\n"
		"Max Q depth since init: %d\n"
		"Max # commands on controller since init: %d\n"
		"Max SG entries since init: %d\n",
		h->devname,
		h->product_name,
		(unsigned long)h->board_id,
		h->firm_ver[0], h->firm_ver[1], h->firm_ver[2],
		h->firm_ver[3], (unsigned int)h->intr[SIMPLE_MODE_INT],
		h->num_luns,
		h->Qdepth, h->commands_outstanding,
		h->maxQsinceinit, h->max_outstanding, h->maxSG);

#ifdef CONFIG_CISS_SCSI_TAPE
	cciss_seq_tape_report(seq, h->ctlr);
#endif /* CONFIG_CISS_SCSI_TAPE */
}

static void *cciss_seq_start(struct seq_file *seq, loff_t *pos)
{
	ctlr_info_t *h = seq->private;
	unsigned ctlr = h->ctlr;
	unsigned long flags;

	/* prevent displaying bogus info during configuration
	 * or deconfiguration of a logical volume
	 */
	spin_lock_irqsave(CCISS_LOCK(ctlr), flags);
	if (h->busy_configuring) {
		spin_unlock_irqrestore(CCISS_LOCK(ctlr), flags);
		return ERR_PTR(-EBUSY);
	}
	h->busy_configuring = 1;
	spin_unlock_irqrestore(CCISS_LOCK(ctlr), flags);

	if (*pos == 0)
		cciss_seq_show_header(seq);

	return pos;
}

static int cciss_seq_show(struct seq_file *seq, void *v)
{
	sector_t vol_sz, vol_sz_frac;
	ctlr_info_t *h = seq->private;
	unsigned ctlr = h->ctlr;
	loff_t *pos = v;
	drive_info_struct *drv = h->drv[*pos];

	if (*pos > h->highest_lun)
		return 0;

	if (drv->heads == 0)
		return 0;

	vol_sz = drv->nr_blocks;
	vol_sz_frac = sector_div(vol_sz, ENG_GIG_FACTOR);
	vol_sz_frac *= 100;
	sector_div(vol_sz_frac, ENG_GIG_FACTOR);

	if (drv->raid_level < 0 || drv->raid_level > RAID_UNKNOWN)
		drv->raid_level = RAID_UNKNOWN;
	seq_printf(seq, "cciss/c%dd%d:"
			"\t%4u.%02uGB\tRAID %s\n",
			ctlr, (int) *pos, (int)vol_sz, (int)vol_sz_frac,
			raid_label[drv->raid_level]);
	return 0;
}

static void *cciss_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	ctlr_info_t *h = seq->private;

	if (*pos > h->highest_lun)
		return NULL;
	*pos += 1;

	return pos;
}

static void cciss_seq_stop(struct seq_file *seq, void *v)
{
	ctlr_info_t *h = seq->private;

	/* Only reset h->busy_configuring if we succeeded in setting
	 * it during cciss_seq_start. */
	if (v == ERR_PTR(-EBUSY))
		return;

	h->busy_configuring = 0;
}

static const struct seq_operations cciss_seq_ops = {
	.start = cciss_seq_start,
	.show  = cciss_seq_show,
	.next  = cciss_seq_next,
	.stop  = cciss_seq_stop,
};

static int cciss_seq_open(struct inode *inode, struct file *file)
{
	int ret = seq_open(file, &cciss_seq_ops);
	struct seq_file *seq = file->private_data;

	if (!ret)
		seq->private = PDE(inode)->data;

	return ret;
}

static ssize_t
cciss_proc_write(struct file *file, const char __user *buf,
		 size_t length, loff_t *ppos)
{
	int err;
	char *buffer;

#ifndef CONFIG_CISS_SCSI_TAPE
	return -EINVAL;
#endif

	if (!buf || length > PAGE_SIZE - 1)
		return -EINVAL;

	buffer = (char *)__get_free_page(GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	err = -EFAULT;
	if (copy_from_user(buffer, buf, length))
		goto out;
	buffer[length] = '\0';

#ifdef CONFIG_CISS_SCSI_TAPE
	if (strncmp(ENGAGE_SCSI, buffer, sizeof ENGAGE_SCSI - 1) == 0) {
		struct seq_file *seq = file->private_data;
		ctlr_info_t *h = seq->private;
		int rc;

		rc = cciss_engage_scsi(h->ctlr);
		if (rc != 0)
			err = -rc;
		else
			err = length;
	} else
#endif /* CONFIG_CISS_SCSI_TAPE */
		err = -EINVAL;
	/* might be nice to have "disengage" too, but it's not
	   safely possible. (only 1 module use count, lock issues.) */

out:
	free_page((unsigned long)buffer);
	return err;
}

static const struct file_operations cciss_proc_fops = {
	.owner	 = THIS_MODULE,
	.open    = cciss_seq_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release,
	.write	 = cciss_proc_write,
};

static void __devinit cciss_procinit(int i)
{
	struct proc_dir_entry *pde;

	if (proc_cciss == NULL)
		proc_cciss = proc_mkdir("driver/cciss", NULL);
	if (!proc_cciss)
		return;
	pde = proc_create_data(hba[i]->devname, S_IWUSR | S_IRUSR | S_IRGRP |
					S_IROTH, proc_cciss,
					&cciss_proc_fops, hba[i]);
}
#endif				/* CONFIG_PROC_FS */

#define MAX_PRODUCT_NAME_LEN 19

#define to_hba(n) container_of(n, struct ctlr_info, dev)
#define to_drv(n) container_of(n, drive_info_struct, dev)

static ssize_t host_store_rescan(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct ctlr_info *h = to_hba(dev);

	add_to_scan_list(h);
	wake_up_process(cciss_scan_thread);
	wait_for_completion_interruptible(&h->scan_wait);

	return count;
}
static DEVICE_ATTR(rescan, S_IWUSR, NULL, host_store_rescan);

static ssize_t dev_show_unique_id(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	drive_info_struct *drv = to_drv(dev);
	struct ctlr_info *h = to_hba(drv->dev.parent);
	__u8 sn[16];
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(CCISS_LOCK(h->ctlr), flags);
	if (h->busy_configuring)
		ret = -EBUSY;
	else
		memcpy(sn, drv->serial_no, sizeof(sn));
	spin_unlock_irqrestore(CCISS_LOCK(h->ctlr), flags);

	if (ret)
		return ret;
	else
		return snprintf(buf, 16 * 2 + 2,
				"%02X%02X%02X%02X%02X%02X%02X%02X"
				"%02X%02X%02X%02X%02X%02X%02X%02X\n",
				sn[0], sn[1], sn[2], sn[3],
				sn[4], sn[5], sn[6], sn[7],
				sn[8], sn[9], sn[10], sn[11],
				sn[12], sn[13], sn[14], sn[15]);
}
static DEVICE_ATTR(unique_id, S_IRUGO, dev_show_unique_id, NULL);

static ssize_t dev_show_vendor(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	drive_info_struct *drv = to_drv(dev);
	struct ctlr_info *h = to_hba(drv->dev.parent);
	char vendor[VENDOR_LEN + 1];
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(CCISS_LOCK(h->ctlr), flags);
	if (h->busy_configuring)
		ret = -EBUSY;
	else
		memcpy(vendor, drv->vendor, VENDOR_LEN + 1);
	spin_unlock_irqrestore(CCISS_LOCK(h->ctlr), flags);

	if (ret)
		return ret;
	else
		return snprintf(buf, sizeof(vendor) + 1, "%s\n", drv->vendor);
}
static DEVICE_ATTR(vendor, S_IRUGO, dev_show_vendor, NULL);

static ssize_t dev_show_model(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	drive_info_struct *drv = to_drv(dev);
	struct ctlr_info *h = to_hba(drv->dev.parent);
	char model[MODEL_LEN + 1];
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(CCISS_LOCK(h->ctlr), flags);
	if (h->busy_configuring)
		ret = -EBUSY;
	else
		memcpy(model, drv->model, MODEL_LEN + 1);
	spin_unlock_irqrestore(CCISS_LOCK(h->ctlr), flags);

	if (ret)
		return ret;
	else
		return snprintf(buf, sizeof(model) + 1, "%s\n", drv->model);
}
static DEVICE_ATTR(model, S_IRUGO, dev_show_model, NULL);

static ssize_t dev_show_rev(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	drive_info_struct *drv = to_drv(dev);
	struct ctlr_info *h = to_hba(drv->dev.parent);
	char rev[REV_LEN + 1];
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(CCISS_LOCK(h->ctlr), flags);
	if (h->busy_configuring)
		ret = -EBUSY;
	else
		memcpy(rev, drv->rev, REV_LEN + 1);
	spin_unlock_irqrestore(CCISS_LOCK(h->ctlr), flags);

	if (ret)
		return ret;
	else
		return snprintf(buf, sizeof(rev) + 1, "%s\n", drv->rev);
}
static DEVICE_ATTR(rev, S_IRUGO, dev_show_rev, NULL);

static ssize_t cciss_show_lunid(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	drive_info_struct *drv = to_drv(dev);
	struct ctlr_info *h = to_hba(drv->dev.parent);
	unsigned long flags;
	unsigned char lunid[8];

	spin_lock_irqsave(CCISS_LOCK(h->ctlr), flags);
	if (h->busy_configuring) {
		spin_unlock_irqrestore(CCISS_LOCK(h->ctlr), flags);
		return -EBUSY;
	}
	if (!drv->heads) {
		spin_unlock_irqrestore(CCISS_LOCK(h->ctlr), flags);
		return -ENOTTY;
	}
	memcpy(lunid, drv->LunID, sizeof(lunid));
	spin_unlock_irqrestore(CCISS_LOCK(h->ctlr), flags);
	return snprintf(buf, 20, "0x%02x%02x%02x%02x%02x%02x%02x%02x\n",
		lunid[0], lunid[1], lunid[2], lunid[3],
		lunid[4], lunid[5], lunid[6], lunid[7]);
}
static DEVICE_ATTR(lunid, S_IRUGO, cciss_show_lunid, NULL);

static ssize_t cciss_show_raid_level(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	drive_info_struct *drv = to_drv(dev);
	struct ctlr_info *h = to_hba(drv->dev.parent);
	int raid;
	unsigned long flags;

	spin_lock_irqsave(CCISS_LOCK(h->ctlr), flags);
	if (h->busy_configuring) {
		spin_unlock_irqrestore(CCISS_LOCK(h->ctlr), flags);
		return -EBUSY;
	}
	raid = drv->raid_level;
	spin_unlock_irqrestore(CCISS_LOCK(h->ctlr), flags);
	if (raid < 0 || raid > RAID_UNKNOWN)
		raid = RAID_UNKNOWN;

	return snprintf(buf, strlen(raid_label[raid]) + 7, "RAID %s\n",
			raid_label[raid]);
}
static DEVICE_ATTR(raid_level, S_IRUGO, cciss_show_raid_level, NULL);

static ssize_t cciss_show_usage_count(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	drive_info_struct *drv = to_drv(dev);
	struct ctlr_info *h = to_hba(drv->dev.parent);
	unsigned long flags;
	int count;

	spin_lock_irqsave(CCISS_LOCK(h->ctlr), flags);
	if (h->busy_configuring) {
		spin_unlock_irqrestore(CCISS_LOCK(h->ctlr), flags);
		return -EBUSY;
	}
	count = drv->usage_count;
	spin_unlock_irqrestore(CCISS_LOCK(h->ctlr), flags);
	return snprintf(buf, 20, "%d\n", count);
}
static DEVICE_ATTR(usage_count, S_IRUGO, cciss_show_usage_count, NULL);

static struct attribute *cciss_host_attrs[] = {
	&dev_attr_rescan.attr,
	NULL
};

static struct attribute_group cciss_host_attr_group = {
	.attrs = cciss_host_attrs,
};

static const struct attribute_group *cciss_host_attr_groups[] = {
	&cciss_host_attr_group,
	NULL
};

static struct device_type cciss_host_type = {
	.name		= "cciss_host",
	.groups		= cciss_host_attr_groups,
	.release	= cciss_hba_release,
};

static struct attribute *cciss_dev_attrs[] = {
	&dev_attr_unique_id.attr,
	&dev_attr_model.attr,
	&dev_attr_vendor.attr,
	&dev_attr_rev.attr,
	&dev_attr_lunid.attr,
	&dev_attr_raid_level.attr,
	&dev_attr_usage_count.attr,
	NULL
};

static struct attribute_group cciss_dev_attr_group = {
	.attrs = cciss_dev_attrs,
};

static const struct attribute_group *cciss_dev_attr_groups[] = {
	&cciss_dev_attr_group,
	NULL
};

static struct device_type cciss_dev_type = {
	.name		= "cciss_device",
	.groups		= cciss_dev_attr_groups,
	.release	= cciss_device_release,
};

static struct bus_type cciss_bus_type = {
	.name		= "cciss",
};

/*
 * cciss_hba_release is called when the reference count
 * of h->dev goes to zero.
 */
static void cciss_hba_release(struct device *dev)
{
	/*
	 * nothing to do, but need this to avoid a warning
	 * about not having a release handler from lib/kref.c.
	 */
}

/*
 * Initialize sysfs entry for each controller.  This sets up and registers
 * the 'cciss#' directory for each individual controller under
 * /sys/bus/pci/devices/<dev>/.
 */
static int cciss_create_hba_sysfs_entry(struct ctlr_info *h)
{
	device_initialize(&h->dev);
	h->dev.type = &cciss_host_type;
	h->dev.bus = &cciss_bus_type;
	dev_set_name(&h->dev, "%s", h->devname);
	h->dev.parent = &h->pdev->dev;

	return device_add(&h->dev);
}

/*
 * Remove sysfs entries for an hba.
 */
static void cciss_destroy_hba_sysfs_entry(struct ctlr_info *h)
{
	device_del(&h->dev);
	put_device(&h->dev); /* final put. */
}

/* cciss_device_release is called when the reference count
 * of h->drv[x]dev goes to zero.
 */
static void cciss_device_release(struct device *dev)
{
	drive_info_struct *drv = to_drv(dev);
	kfree(drv);
}

/*
 * Initialize sysfs for each logical drive.  This sets up and registers
 * the 'c#d#' directory for each individual logical drive under
 * /sys/bus/pci/devices/<dev/ccis#/. We also create a link from
 * /sys/block/cciss!c#d# to this entry.
 */
static long cciss_create_ld_sysfs_entry(struct ctlr_info *h,
				       int drv_index)
{
	struct device *dev;

	if (h->drv[drv_index]->device_initialized)
		return 0;

	dev = &h->drv[drv_index]->dev;
	device_initialize(dev);
	dev->type = &cciss_dev_type;
	dev->bus = &cciss_bus_type;
	dev_set_name(dev, "c%dd%d", h->ctlr, drv_index);
	dev->parent = &h->dev;
	h->drv[drv_index]->device_initialized = 1;
	return device_add(dev);
}

/*
 * Remove sysfs entries for a logical drive.
 */
static void cciss_destroy_ld_sysfs_entry(struct ctlr_info *h, int drv_index,
	int ctlr_exiting)
{
	struct device *dev = &h->drv[drv_index]->dev;

	/* special case for c*d0, we only destroy it on controller exit */
	if (drv_index == 0 && !ctlr_exiting)
		return;

	device_del(dev);
	put_device(dev); /* the "final" put. */
	h->drv[drv_index] = NULL;
}

/*
 * For operations that cannot sleep, a command block is allocated at init,
 * and managed by cmd_alloc() and cmd_free() using a simple bitmap to track
 * which ones are free or in use.  For operations that can wait for kmalloc
 * to possible sleep, this routine can be called with get_from_pool set to 0.
 * cmd_free() MUST be called with a got_from_pool set to 0 if cmd_alloc was.
 */
static CommandList_struct *cmd_alloc(ctlr_info_t *h, int get_from_pool)
{
	CommandList_struct *c;
	int i;
	u64bit temp64;
	dma_addr_t cmd_dma_handle, err_dma_handle;

	if (!get_from_pool) {
		c = (CommandList_struct *) pci_alloc_consistent(h->pdev,
			sizeof(CommandList_struct), &cmd_dma_handle);
		if (c == NULL)
			return NULL;
		memset(c, 0, sizeof(CommandList_struct));

		c->cmdindex = -1;

		c->err_info = (ErrorInfo_struct *)
		    pci_alloc_consistent(h->pdev, sizeof(ErrorInfo_struct),
			    &err_dma_handle);

		if (c->err_info == NULL) {
			pci_free_consistent(h->pdev,
				sizeof(CommandList_struct), c, cmd_dma_handle);
			return NULL;
		}
		memset(c->err_info, 0, sizeof(ErrorInfo_struct));
	} else {		/* get it out of the controllers pool */

		do {
			i = find_first_zero_bit(h->cmd_pool_bits, h->nr_cmds);
			if (i == h->nr_cmds)
				return NULL;
		} while (test_and_set_bit
			 (i & (BITS_PER_LONG - 1),
			  h->cmd_pool_bits + (i / BITS_PER_LONG)) != 0);
#ifdef CCISS_DEBUG
		printk(KERN_DEBUG "cciss: using command buffer %d\n", i);
#endif
		c = h->cmd_pool + i;
		memset(c, 0, sizeof(CommandList_struct));
		cmd_dma_handle = h->cmd_pool_dhandle
		    + i * sizeof(CommandList_struct);
		c->err_info = h->errinfo_pool + i;
		memset(c->err_info, 0, sizeof(ErrorInfo_struct));
		err_dma_handle = h->errinfo_pool_dhandle
		    + i * sizeof(ErrorInfo_struct);
		h->nr_allocs++;

		c->cmdindex = i;
	}

	INIT_HLIST_NODE(&c->list);
	c->busaddr = (__u32) cmd_dma_handle;
	temp64.val = (__u64) err_dma_handle;
	c->ErrDesc.Addr.lower = temp64.val32.lower;
	c->ErrDesc.Addr.upper = temp64.val32.upper;
	c->ErrDesc.Len = sizeof(ErrorInfo_struct);

	c->ctlr = h->ctlr;
	return c;
}

/*
 * Frees a command block that was previously allocated with cmd_alloc().
 */
static void cmd_free(ctlr_info_t *h, CommandList_struct *c, int got_from_pool)
{
	int i;
	u64bit temp64;

	if (!got_from_pool) {
		temp64.val32.lower = c->ErrDesc.Addr.lower;
		temp64.val32.upper = c->ErrDesc.Addr.upper;
		pci_free_consistent(h->pdev, sizeof(ErrorInfo_struct),
				    c->err_info, (dma_addr_t) temp64.val);
		pci_free_consistent(h->pdev, sizeof(CommandList_struct),
				    c, (dma_addr_t) c->busaddr);
	} else {
		i = c - h->cmd_pool;
		clear_bit(i & (BITS_PER_LONG - 1),
			  h->cmd_pool_bits + (i / BITS_PER_LONG));
		h->nr_frees++;
	}
}

static inline ctlr_info_t *get_host(struct gendisk *disk)
{
	return disk->queue->queuedata;
}

static inline drive_info_struct *get_drv(struct gendisk *disk)
{
	return disk->private_data;
}

/*
 * Open.  Make sure the device is really there.
 */
static int cciss_open(struct block_device *bdev, fmode_t mode)
{
	ctlr_info_t *host = get_host(bdev->bd_disk);
	drive_info_struct *drv = get_drv(bdev->bd_disk);

#ifdef CCISS_DEBUG
	printk(KERN_DEBUG "cciss_open %s\n", bdev->bd_disk->disk_name);
#endif				/* CCISS_DEBUG */

	if (drv->busy_configuring)
		return -EBUSY;
	/*
	 * Root is allowed to open raw volume zero even if it's not configured
	 * so array config can still work. Root is also allowed to open any
	 * volume that has a LUN ID, so it can issue IOCTL to reread the
	 * disk information.  I don't think I really like this
	 * but I'm already using way to many device nodes to claim another one
	 * for "raw controller".
	 */
	if (drv->heads == 0) {
		if (MINOR(bdev->bd_dev) != 0) {	/* not node 0? */
			/* if not node 0 make sure it is a partition = 0 */
			if (MINOR(bdev->bd_dev) & 0x0f) {
				return -ENXIO;
				/* if it is, make sure we have a LUN ID */
			} else if (memcmp(drv->LunID, CTLR_LUNID,
				sizeof(drv->LunID))) {
				return -ENXIO;
			}
		}
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
	}
	drv->usage_count++;
	host->usage_count++;
	return 0;
}

/*
 * Close.  Sync first.
 */
static int cciss_release(struct gendisk *disk, fmode_t mode)
{
	ctlr_info_t *host = get_host(disk);
	drive_info_struct *drv = get_drv(disk);

#ifdef CCISS_DEBUG
	printk(KERN_DEBUG "cciss_release %s\n", disk->disk_name);
#endif				/* CCISS_DEBUG */

	drv->usage_count--;
	host->usage_count--;
	return 0;
}

#ifdef CONFIG_COMPAT

static int do_ioctl(struct block_device *bdev, fmode_t mode,
		    unsigned cmd, unsigned long arg)
{
	int ret;
	lock_kernel();
	ret = cciss_ioctl(bdev, mode, cmd, arg);
	unlock_kernel();
	return ret;
}

static int cciss_ioctl32_passthru(struct block_device *bdev, fmode_t mode,
				  unsigned cmd, unsigned long arg);
static int cciss_ioctl32_big_passthru(struct block_device *bdev, fmode_t mode,
				      unsigned cmd, unsigned long arg);

static int cciss_compat_ioctl(struct block_device *bdev, fmode_t mode,
			      unsigned cmd, unsigned long arg)
{
	switch (cmd) {
	case CCISS_GETPCIINFO:
	case CCISS_GETINTINFO:
	case CCISS_SETINTINFO:
	case CCISS_GETNODENAME:
	case CCISS_SETNODENAME:
	case CCISS_GETHEARTBEAT:
	case CCISS_GETBUSTYPES:
	case CCISS_GETFIRMVER:
	case CCISS_GETDRIVVER:
	case CCISS_REVALIDVOLS:
	case CCISS_DEREGDISK:
	case CCISS_REGNEWDISK:
	case CCISS_REGNEWD:
	case CCISS_RESCANDISK:
	case CCISS_GETLUNINFO:
		return do_ioctl(bdev, mode, cmd, arg);

	case CCISS_PASSTHRU32:
		return cciss_ioctl32_passthru(bdev, mode, cmd, arg);
	case CCISS_BIG_PASSTHRU32:
		return cciss_ioctl32_big_passthru(bdev, mode, cmd, arg);

	default:
		return -ENOIOCTLCMD;
	}
}

static int cciss_ioctl32_passthru(struct block_device *bdev, fmode_t mode,
				  unsigned cmd, unsigned long arg)
{
	IOCTL32_Command_struct __user *arg32 =
	    (IOCTL32_Command_struct __user *) arg;
	IOCTL_Command_struct arg64;
	IOCTL_Command_struct __user *p = compat_alloc_user_space(sizeof(arg64));
	int err;
	u32 cp;

	err = 0;
	err |=
	    copy_from_user(&arg64.LUN_info, &arg32->LUN_info,
			   sizeof(arg64.LUN_info));
	err |=
	    copy_from_user(&arg64.Request, &arg32->Request,
			   sizeof(arg64.Request));
	err |=
	    copy_from_user(&arg64.error_info, &arg32->error_info,
			   sizeof(arg64.error_info));
	err |= get_user(arg64.buf_size, &arg32->buf_size);
	err |= get_user(cp, &arg32->buf);
	arg64.buf = compat_ptr(cp);
	err |= copy_to_user(p, &arg64, sizeof(arg64));

	if (err)
		return -EFAULT;

	err = do_ioctl(bdev, mode, CCISS_PASSTHRU, (unsigned long)p);
	if (err)
		return err;
	err |=
	    copy_in_user(&arg32->error_info, &p->error_info,
			 sizeof(arg32->error_info));
	if (err)
		return -EFAULT;
	return err;
}

static int cciss_ioctl32_big_passthru(struct block_device *bdev, fmode_t mode,
				      unsigned cmd, unsigned long arg)
{
	BIG_IOCTL32_Command_struct __user *arg32 =
	    (BIG_IOCTL32_Command_struct __user *) arg;
	BIG_IOCTL_Command_struct arg64;
	BIG_IOCTL_Command_struct __user *p =
	    compat_alloc_user_space(sizeof(arg64));
	int err;
	u32 cp;

	err = 0;
	err |=
	    copy_from_user(&arg64.LUN_info, &arg32->LUN_info,
			   sizeof(arg64.LUN_info));
	err |=
	    copy_from_user(&arg64.Request, &arg32->Request,
			   sizeof(arg64.Request));
	err |=
	    copy_from_user(&arg64.error_info, &arg32->error_info,
			   sizeof(arg64.error_info));
	err |= get_user(arg64.buf_size, &arg32->buf_size);
	err |= get_user(arg64.malloc_size, &arg32->malloc_size);
	err |= get_user(cp, &arg32->buf);
	arg64.buf = compat_ptr(cp);
	err |= copy_to_user(p, &arg64, sizeof(arg64));

	if (err)
		return -EFAULT;

	err = do_ioctl(bdev, mode, CCISS_BIG_PASSTHRU, (unsigned long)p);
	if (err)
		return err;
	err |=
	    copy_in_user(&arg32->error_info, &p->error_info,
			 sizeof(arg32->error_info));
	if (err)
		return -EFAULT;
	return err;
}
#endif

static int cciss_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	drive_info_struct *drv = get_drv(bdev->bd_disk);

	if (!drv->cylinders)
		return -ENXIO;

	geo->heads = drv->heads;
	geo->sectors = drv->sectors;
	geo->cylinders = drv->cylinders;
	return 0;
}

static void check_ioctl_unit_attention(ctlr_info_t *host, CommandList_struct *c)
{
	if (c->err_info->CommandStatus == CMD_TARGET_STATUS &&
			c->err_info->ScsiStatus != SAM_STAT_CHECK_CONDITION)
		(void)check_for_unit_attention(host, c);
}
/*
 * ioctl
 */
static int cciss_ioctl(struct block_device *bdev, fmode_t mode,
		       unsigned int cmd, unsigned long arg)
{
	struct gendisk *disk = bdev->bd_disk;
	ctlr_info_t *host = get_host(disk);
	drive_info_struct *drv = get_drv(disk);
	int ctlr = host->ctlr;
	void __user *argp = (void __user *)arg;

#ifdef CCISS_DEBUG
	printk(KERN_DEBUG "cciss_ioctl: Called with cmd=%x %lx\n", cmd, arg);
#endif				/* CCISS_DEBUG */

	switch (cmd) {
	case CCISS_GETPCIINFO:
		{
			cciss_pci_info_struct pciinfo;

			if (!arg)
				return -EINVAL;
			pciinfo.domain = pci_domain_nr(host->pdev->bus);
			pciinfo.bus = host->pdev->bus->number;
			pciinfo.dev_fn = host->pdev->devfn;
			pciinfo.board_id = host->board_id;
			if (copy_to_user
			    (argp, &pciinfo, sizeof(cciss_pci_info_struct)))
				return -EFAULT;
			return 0;
		}
	case CCISS_GETINTINFO:
		{
			cciss_coalint_struct intinfo;
			if (!arg)
				return -EINVAL;
			intinfo.delay =
			    readl(&host->cfgtable->HostWrite.CoalIntDelay);
			intinfo.count =
			    readl(&host->cfgtable->HostWrite.CoalIntCount);
			if (copy_to_user
			    (argp, &intinfo, sizeof(cciss_coalint_struct)))
				return -EFAULT;
			return 0;
		}
	case CCISS_SETINTINFO:
		{
			cciss_coalint_struct intinfo;
			unsigned long flags;
			int i;

			if (!arg)
				return -EINVAL;
			if (!capable(CAP_SYS_ADMIN))
				return -EPERM;
			if (copy_from_user
			    (&intinfo, argp, sizeof(cciss_coalint_struct)))
				return -EFAULT;
			if ((intinfo.delay == 0) && (intinfo.count == 0))
			{
//                      printk("cciss_ioctl: delay and count cannot be 0\n");
				return -EINVAL;
			}
			spin_lock_irqsave(CCISS_LOCK(ctlr), flags);
			/* Update the field, and then ring the doorbell */
			writel(intinfo.delay,
			       &(host->cfgtable->HostWrite.CoalIntDelay));
			writel(intinfo.count,
			       &(host->cfgtable->HostWrite.CoalIntCount));
			writel(CFGTBL_ChangeReq, host->vaddr + SA5_DOORBELL);

			for (i = 0; i < MAX_IOCTL_CONFIG_WAIT; i++) {
				if (!(readl(host->vaddr + SA5_DOORBELL)
				      & CFGTBL_ChangeReq))
					break;
				/* delay and try again */
				udelay(1000);
			}
			spin_unlock_irqrestore(CCISS_LOCK(ctlr), flags);
			if (i >= MAX_IOCTL_CONFIG_WAIT)
				return -EAGAIN;
			return 0;
		}
	case CCISS_GETNODENAME:
		{
			NodeName_type NodeName;
			int i;

			if (!arg)
				return -EINVAL;
			for (i = 0; i < 16; i++)
				NodeName[i] =
				    readb(&host->cfgtable->ServerName[i]);
			if (copy_to_user(argp, NodeName, sizeof(NodeName_type)))
				return -EFAULT;
			return 0;
		}
	case CCISS_SETNODENAME:
		{
			NodeName_type NodeName;
			unsigned long flags;
			int i;

			if (!arg)
				return -EINVAL;
			if (!capable(CAP_SYS_ADMIN))
				return -EPERM;

			if (copy_from_user
			    (NodeName, argp, sizeof(NodeName_type)))
				return -EFAULT;

			spin_lock_irqsave(CCISS_LOCK(ctlr), flags);

			/* Update the field, and then ring the doorbell */
			for (i = 0; i < 16; i++)
				writeb(NodeName[i],
				       &host->cfgtable->ServerName[i]);

			writel(CFGTBL_ChangeReq, host->vaddr + SA5_DOORBELL);

			for (i = 0; i < MAX_IOCTL_CONFIG_WAIT; i++) {
				if (!(readl(host->vaddr + SA5_DOORBELL)
				      & CFGTBL_ChangeReq))
					break;
				/* delay and try again */
				udelay(1000);
			}
			spin_unlock_irqrestore(CCISS_LOCK(ctlr), flags);
			if (i >= MAX_IOCTL_CONFIG_WAIT)
				return -EAGAIN;
			return 0;
		}

	case CCISS_GETHEARTBEAT:
		{
			Heartbeat_type heartbeat;

			if (!arg)
				return -EINVAL;
			heartbeat = readl(&host->cfgtable->HeartBeat);
			if (copy_to_user
			    (argp, &heartbeat, sizeof(Heartbeat_type)))
				return -EFAULT;
			return 0;
		}
	case CCISS_GETBUSTYPES:
		{
			BusTypes_type BusTypes;

			if (!arg)
				return -EINVAL;
			BusTypes = readl(&host->cfgtable->BusTypes);
			if (copy_to_user
			    (argp, &BusTypes, sizeof(BusTypes_type)))
				return -EFAULT;
			return 0;
		}
	case CCISS_GETFIRMVER:
		{
			FirmwareVer_type firmware;

			if (!arg)
				return -EINVAL;
			memcpy(firmware, host->firm_ver, 4);

			if (copy_to_user
			    (argp, firmware, sizeof(FirmwareVer_type)))
				return -EFAULT;
			return 0;
		}
	case CCISS_GETDRIVVER:
		{
			DriverVer_type DriverVer = DRIVER_VERSION;

			if (!arg)
				return -EINVAL;

			if (copy_to_user
			    (argp, &DriverVer, sizeof(DriverVer_type)))
				return -EFAULT;
			return 0;
		}

	case CCISS_DEREGDISK:
	case CCISS_REGNEWD:
	case CCISS_REVALIDVOLS:
		return rebuild_lun_table(host, 0, 1);

	case CCISS_GETLUNINFO:{
			LogvolInfo_struct luninfo;

			memcpy(&luninfo.LunID, drv->LunID,
				sizeof(luninfo.LunID));
			luninfo.num_opens = drv->usage_count;
			luninfo.num_parts = 0;
			if (copy_to_user(argp, &luninfo,
					 sizeof(LogvolInfo_struct)))
				return -EFAULT;
			return 0;
		}
	case CCISS_PASSTHRU:
		{
			IOCTL_Command_struct iocommand;
			CommandList_struct *c;
			char *buff = NULL;
			u64bit temp64;
			unsigned long flags;
			DECLARE_COMPLETION_ONSTACK(wait);

			if (!arg)
				return -EINVAL;

			if (!capable(CAP_SYS_RAWIO))
				return -EPERM;

			if (copy_from_user
			    (&iocommand, argp, sizeof(IOCTL_Command_struct)))
				return -EFAULT;
			if ((iocommand.buf_size < 1) &&
			    (iocommand.Request.Type.Direction != XFER_NONE)) {
				return -EINVAL;
			}
#if 0				/* 'buf_size' member is 16-bits, and always smaller than kmalloc limit */
			/* Check kmalloc limits */
			if (iocommand.buf_size > 128000)
				return -EINVAL;
#endif
			if (iocommand.buf_size > 0) {
				buff = kmalloc(iocommand.buf_size, GFP_KERNEL);
				if (buff == NULL)
					return -EFAULT;
			}
			if (iocommand.Request.Type.Direction == XFER_WRITE) {
				/* Copy the data into the buffer we created */
				if (copy_from_user
				    (buff, iocommand.buf, iocommand.buf_size)) {
					kfree(buff);
					return -EFAULT;
				}
			} else {
				memset(buff, 0, iocommand.buf_size);
			}
			if ((c = cmd_alloc(host, 0)) == NULL) {
				kfree(buff);
				return -ENOMEM;
			}
			// Fill in the command type
			c->cmd_type = CMD_IOCTL_PEND;
			// Fill in Command Header
			c->Header.ReplyQueue = 0;	// unused in simple mode
			if (iocommand.buf_size > 0)	// buffer to fill
			{
				c->Header.SGList = 1;
				c->Header.SGTotal = 1;
			} else	// no buffers to fill
			{
				c->Header.SGList = 0;
				c->Header.SGTotal = 0;
			}
			c->Header.LUN = iocommand.LUN_info;
			c->Header.Tag.lower = c->busaddr;	// use the kernel address the cmd block for tag

			// Fill in Request block
			c->Request = iocommand.Request;

			// Fill in the scatter gather information
			if (iocommand.buf_size > 0) {
				temp64.val = pci_map_single(host->pdev, buff,
					iocommand.buf_size,
					PCI_DMA_BIDIRECTIONAL);
				c->SG[0].Addr.lower = temp64.val32.lower;
				c->SG[0].Addr.upper = temp64.val32.upper;
				c->SG[0].Len = iocommand.buf_size;
				c->SG[0].Ext = 0;	// we are not chaining
			}
			c->waiting = &wait;

			/* Put the request on the tail of the request queue */
			spin_lock_irqsave(CCISS_LOCK(ctlr), flags);
			addQ(&host->reqQ, c);
			host->Qdepth++;
			start_io(host);
			spin_unlock_irqrestore(CCISS_LOCK(ctlr), flags);

			wait_for_completion(&wait);

			/* unlock the buffers from DMA */
			temp64.val32.lower = c->SG[0].Addr.lower;
			temp64.val32.upper = c->SG[0].Addr.upper;
			pci_unmap_single(host->pdev, (dma_addr_t) temp64.val,
					 iocommand.buf_size,
					 PCI_DMA_BIDIRECTIONAL);

			check_ioctl_unit_attention(host, c);

			/* Copy the error information out */
			iocommand.error_info = *(c->err_info);
			if (copy_to_user
			    (argp, &iocommand, sizeof(IOCTL_Command_struct))) {
				kfree(buff);
				cmd_free(host, c, 0);
				return -EFAULT;
			}

			if (iocommand.Request.Type.Direction == XFER_READ) {
				/* Copy the data out of the buffer we created */
				if (copy_to_user
				    (iocommand.buf, buff, iocommand.buf_size)) {
					kfree(buff);
					cmd_free(host, c, 0);
					return -EFAULT;
				}
			}
			kfree(buff);
			cmd_free(host, c, 0);
			return 0;
		}
	case CCISS_BIG_PASSTHRU:{
			BIG_IOCTL_Command_struct *ioc;
			CommandList_struct *c;
			unsigned char **buff = NULL;
			int *buff_size = NULL;
			u64bit temp64;
			unsigned long flags;
			BYTE sg_used = 0;
			int status = 0;
			int i;
			DECLARE_COMPLETION_ONSTACK(wait);
			__u32 left;
			__u32 sz;
			BYTE __user *data_ptr;

			if (!arg)
				return -EINVAL;
			if (!capable(CAP_SYS_RAWIO))
				return -EPERM;
			ioc = (BIG_IOCTL_Command_struct *)
			    kmalloc(sizeof(*ioc), GFP_KERNEL);
			if (!ioc) {
				status = -ENOMEM;
				goto cleanup1;
			}
			if (copy_from_user(ioc, argp, sizeof(*ioc))) {
				status = -EFAULT;
				goto cleanup1;
			}
			if ((ioc->buf_size < 1) &&
			    (ioc->Request.Type.Direction != XFER_NONE)) {
				status = -EINVAL;
				goto cleanup1;
			}
			/* Check kmalloc limits  using all SGs */
			if (ioc->malloc_size > MAX_KMALLOC_SIZE) {
				status = -EINVAL;
				goto cleanup1;
			}
			if (ioc->buf_size > ioc->malloc_size * MAXSGENTRIES) {
				status = -EINVAL;
				goto cleanup1;
			}
			buff =
			    kzalloc(MAXSGENTRIES * sizeof(char *), GFP_KERNEL);
			if (!buff) {
				status = -ENOMEM;
				goto cleanup1;
			}
			buff_size = kmalloc(MAXSGENTRIES * sizeof(int),
						   GFP_KERNEL);
			if (!buff_size) {
				status = -ENOMEM;
				goto cleanup1;
			}
			left = ioc->buf_size;
			data_ptr = ioc->buf;
			while (left) {
				sz = (left >
				      ioc->malloc_size) ? ioc->
				    malloc_size : left;
				buff_size[sg_used] = sz;
				buff[sg_used] = kmalloc(sz, GFP_KERNEL);
				if (buff[sg_used] == NULL) {
					status = -ENOMEM;
					goto cleanup1;
				}
				if (ioc->Request.Type.Direction == XFER_WRITE) {
					if (copy_from_user
					    (buff[sg_used], data_ptr, sz)) {
						status = -EFAULT;
						goto cleanup1;
					}
				} else {
					memset(buff[sg_used], 0, sz);
				}
				left -= sz;
				data_ptr += sz;
				sg_used++;
			}
			if ((c = cmd_alloc(host, 0)) == NULL) {
				status = -ENOMEM;
				goto cleanup1;
			}
			c->cmd_type = CMD_IOCTL_PEND;
			c->Header.ReplyQueue = 0;

			if (ioc->buf_size > 0) {
				c->Header.SGList = sg_used;
				c->Header.SGTotal = sg_used;
			} else {
				c->Header.SGList = 0;
				c->Header.SGTotal = 0;
			}
			c->Header.LUN = ioc->LUN_info;
			c->Header.Tag.lower = c->busaddr;

			c->Request = ioc->Request;
			if (ioc->buf_size > 0) {
				int i;
				for (i = 0; i < sg_used; i++) {
					temp64.val =
					    pci_map_single(host->pdev, buff[i],
						    buff_size[i],
						    PCI_DMA_BIDIRECTIONAL);
					c->SG[i].Addr.lower =
					    temp64.val32.lower;
					c->SG[i].Addr.upper =
					    temp64.val32.upper;
					c->SG[i].Len = buff_size[i];
					c->SG[i].Ext = 0;	/* we are not chaining */
				}
			}
			c->waiting = &wait;
			/* Put the request on the tail of the request queue */
			spin_lock_irqsave(CCISS_LOCK(ctlr), flags);
			addQ(&host->reqQ, c);
			host->Qdepth++;
			start_io(host);
			spin_unlock_irqrestore(CCISS_LOCK(ctlr), flags);
			wait_for_completion(&wait);
			/* unlock the buffers from DMA */
			for (i = 0; i < sg_used; i++) {
				temp64.val32.lower = c->SG[i].Addr.lower;
				temp64.val32.upper = c->SG[i].Addr.upper;
				pci_unmap_single(host->pdev,
					(dma_addr_t) temp64.val, buff_size[i],
					PCI_DMA_BIDIRECTIONAL);
			}
			check_ioctl_unit_attention(host, c);
			/* Copy the error information out */
			ioc->error_info = *(c->err_info);
			if (copy_to_user(argp, ioc, sizeof(*ioc))) {
				cmd_free(host, c, 0);
				status = -EFAULT;
				goto cleanup1;
			}
			if (ioc->Request.Type.Direction == XFER_READ) {
				/* Copy the data out of the buffer we created */
				BYTE __user *ptr = ioc->buf;
				for (i = 0; i < sg_used; i++) {
					if (copy_to_user
					    (ptr, buff[i], buff_size[i])) {
						cmd_free(host, c, 0);
						status = -EFAULT;
						goto cleanup1;
					}
					ptr += buff_size[i];
				}
			}
			cmd_free(host, c, 0);
			status = 0;
		      cleanup1:
			if (buff) {
				for (i = 0; i < sg_used; i++)
					kfree(buff[i]);
				kfree(buff);
			}
			kfree(buff_size);
			kfree(ioc);
			return status;
		}

	/* scsi_cmd_ioctl handles these, below, though some are not */
	/* very meaningful for cciss.  SG_IO is the main one people want. */

	case SG_GET_VERSION_NUM:
	case SG_SET_TIMEOUT:
	case SG_GET_TIMEOUT:
	case SG_GET_RESERVED_SIZE:
	case SG_SET_RESERVED_SIZE:
	case SG_EMULATED_HOST:
	case SG_IO:
	case SCSI_IOCTL_SEND_COMMAND:
		return scsi_cmd_ioctl(disk->queue, disk, mode, cmd, argp);

	/* scsi_cmd_ioctl would normally handle these, below, but */
	/* they aren't a good fit for cciss, as CD-ROMs are */
	/* not supported, and we don't have any bus/target/lun */
	/* which we present to the kernel. */

	case CDROM_SEND_PACKET:
	case CDROMCLOSETRAY:
	case CDROMEJECT:
	case SCSI_IOCTL_GET_IDLUN:
	case SCSI_IOCTL_GET_BUS_NUMBER:
	default:
		return -ENOTTY;
	}
}

static void cciss_check_queues(ctlr_info_t *h)
{
	int start_queue = h->next_to_run;
	int i;

	/* check to see if we have maxed out the number of commands that can
	 * be placed on the queue.  If so then exit.  We do this check here
	 * in case the interrupt we serviced was from an ioctl and did not
	 * free any new commands.
	 */
	if ((find_first_zero_bit(h->cmd_pool_bits, h->nr_cmds)) == h->nr_cmds)
		return;

	/* We have room on the queue for more commands.  Now we need to queue
	 * them up.  We will also keep track of the next queue to run so
	 * that every queue gets a chance to be started first.
	 */
	for (i = 0; i < h->highest_lun + 1; i++) {
		int curr_queue = (start_queue + i) % (h->highest_lun + 1);
		/* make sure the disk has been added and the drive is real
		 * because this can be called from the middle of init_one.
		 */
		if (!h->drv[curr_queue])
			continue;
		if (!(h->drv[curr_queue]->queue) ||
			!(h->drv[curr_queue]->heads))
			continue;
		blk_start_queue(h->gendisk[curr_queue]->queue);

		/* check to see if we have maxed out the number of commands
		 * that can be placed on the queue.
		 */
		if ((find_first_zero_bit(h->cmd_pool_bits, h->nr_cmds)) == h->nr_cmds) {
			if (curr_queue == start_queue) {
				h->next_to_run =
				    (start_queue + 1) % (h->highest_lun + 1);
				break;
			} else {
				h->next_to_run = curr_queue;
				break;
			}
		}
	}
}

static void cciss_softirq_done(struct request *rq)
{
	CommandList_struct *cmd = rq->completion_data;
	ctlr_info_t *h = hba[cmd->ctlr];
	unsigned long flags;
	u64bit temp64;
	int i, ddir;

	if (cmd->Request.Type.Direction == XFER_READ)
		ddir = PCI_DMA_FROMDEVICE;
	else
		ddir = PCI_DMA_TODEVICE;

	/* command did not need to be retried */
	/* unmap the DMA mapping for all the scatter gather elements */
	for (i = 0; i < cmd->Header.SGList; i++) {
		temp64.val32.lower = cmd->SG[i].Addr.lower;
		temp64.val32.upper = cmd->SG[i].Addr.upper;
		pci_unmap_page(h->pdev, temp64.val, cmd->SG[i].Len, ddir);
	}

#ifdef CCISS_DEBUG
	printk("Done with %p\n", rq);
#endif				/* CCISS_DEBUG */

	/* set the residual count for pc requests */
	if (blk_pc_request(rq))
		rq->resid_len = cmd->err_info->ResidualCnt;

	blk_end_request_all(rq, (rq->errors == 0) ? 0 : -EIO);

	spin_lock_irqsave(&h->lock, flags);
	cmd_free(h, cmd, 1);
	cciss_check_queues(h);
	spin_unlock_irqrestore(&h->lock, flags);
}

static inline void log_unit_to_scsi3addr(ctlr_info_t *h,
	unsigned char scsi3addr[], uint32_t log_unit)
{
	memcpy(scsi3addr, h->drv[log_unit]->LunID,
		sizeof(h->drv[log_unit]->LunID));
}

/* This function gets the SCSI vendor, model, and revision of a logical drive
 * via the inquiry page 0.  Model, vendor, and rev are set to empty strings if
 * they cannot be read.
 */
static void cciss_get_device_descr(int ctlr, int logvol, int withirq,
				   char *vendor, char *model, char *rev)
{
	int rc;
	InquiryData_struct *inq_buf;
	unsigned char scsi3addr[8];

	*vendor = '\0';
	*model = '\0';
	*rev = '\0';

	inq_buf = kzalloc(sizeof(InquiryData_struct), GFP_KERNEL);
	if (!inq_buf)
		return;

	log_unit_to_scsi3addr(hba[ctlr], scsi3addr, logvol);
	if (withirq)
		rc = sendcmd_withirq(CISS_INQUIRY, ctlr, inq_buf,
			     sizeof(InquiryData_struct), 0,
				scsi3addr, TYPE_CMD);
	else
		rc = sendcmd(CISS_INQUIRY, ctlr, inq_buf,
			     sizeof(InquiryData_struct), 0,
				scsi3addr, TYPE_CMD);
	if (rc == IO_OK) {
		memcpy(vendor, &inq_buf->data_byte[8], VENDOR_LEN);
		vendor[VENDOR_LEN] = '\0';
		memcpy(model, &inq_buf->data_byte[16], MODEL_LEN);
		model[MODEL_LEN] = '\0';
		memcpy(rev, &inq_buf->data_byte[32], REV_LEN);
		rev[REV_LEN] = '\0';
	}

	kfree(inq_buf);
	return;
}

/* This function gets the serial number of a logical drive via
 * inquiry page 0x83.  Serial no. is 16 bytes.  If the serial
 * number cannot be had, for whatever reason, 16 bytes of 0xff
 * are returned instead.
 */
static void cciss_get_serial_no(int ctlr, int logvol, int withirq,
				unsigned char *serial_no, int buflen)
{
#define PAGE_83_INQ_BYTES 64
	int rc;
	unsigned char *buf;
	unsigned char scsi3addr[8];

	if (buflen > 16)
		buflen = 16;
	memset(serial_no, 0xff, buflen);
	buf = kzalloc(PAGE_83_INQ_BYTES, GFP_KERNEL);
	if (!buf)
		return;
	memset(serial_no, 0, buflen);
	log_unit_to_scsi3addr(hba[ctlr], scsi3addr, logvol);
	if (withirq)
		rc = sendcmd_withirq(CISS_INQUIRY, ctlr, buf,
			PAGE_83_INQ_BYTES, 0x83, scsi3addr, TYPE_CMD);
	else
		rc = sendcmd(CISS_INQUIRY, ctlr, buf,
			PAGE_83_INQ_BYTES, 0x83, scsi3addr, TYPE_CMD);
	if (rc == IO_OK)
		memcpy(serial_no, &buf[8], buflen);
	kfree(buf);
	return;
}

/*
 * cciss_add_disk sets up the block device queue for a logical drive
 */
static int cciss_add_disk(ctlr_info_t *h, struct gendisk *disk,
				int drv_index)
{
	disk->queue = blk_init_queue(do_cciss_request, &h->lock);
	if (!disk->queue)
		goto init_queue_failure;
	sprintf(disk->disk_name, "cciss/c%dd%d", h->ctlr, drv_index);
	disk->major = h->major;
	disk->first_minor = drv_index << NWD_SHIFT;
	disk->fops = &cciss_fops;
	if (cciss_create_ld_sysfs_entry(h, drv_index))
		goto cleanup_queue;
	disk->private_data = h->drv[drv_index];
	disk->driverfs_dev = &h->drv[drv_index]->dev;

	/* Set up queue information */
	blk_queue_bounce_limit(disk->queue, h->pdev->dma_mask);

	/* This is a hardware imposed limit. */
	blk_queue_max_hw_segments(disk->queue, MAXSGENTRIES);

	/* This is a limit in the driver and could be eliminated. */
	blk_queue_max_phys_segments(disk->queue, MAXSGENTRIES);

	blk_queue_max_sectors(disk->queue, h->cciss_max_sectors);

	blk_queue_softirq_done(disk->queue, cciss_softirq_done);

	disk->queue->queuedata = h;

	blk_queue_logical_block_size(disk->queue,
				     h->drv[drv_index]->block_size);

	/* Make sure all queue data is written out before */
	/* setting h->drv[drv_index]->queue, as setting this */
	/* allows the interrupt handler to start the queue */
	wmb();
	h->drv[drv_index]->queue = disk->queue;
	add_disk(disk);
	return 0;

cleanup_queue:
	blk_cleanup_queue(disk->queue);
	disk->queue = NULL;
init_queue_failure:
	return -1;
}

/* This function will check the usage_count of the drive to be updated/added.
 * If the usage_count is zero and it is a heretofore unknown drive, or,
 * the drive's capacity, geometry, or serial number has changed,
 * then the drive information will be updated and the disk will be
 * re-registered with the kernel.  If these conditions don't hold,
 * then it will be left alone for the next reboot.  The exception to this
 * is disk 0 which will always be left registered with the kernel since it
 * is also the controller node.  Any changes to disk 0 will show up on
 * the next reboot.
 */
static void cciss_update_drive_info(int ctlr, int drv_index, int first_time,
	int via_ioctl)
{
	ctlr_info_t *h = hba[ctlr];
	struct gendisk *disk;
	InquiryData_struct *inq_buff = NULL;
	unsigned int block_size;
	sector_t total_size;
	unsigned long flags = 0;
	int ret = 0;
	drive_info_struct *drvinfo;

	/* Get information about the disk and modify the driver structure */
	inq_buff = kmalloc(sizeof(InquiryData_struct), GFP_KERNEL);
	drvinfo = kzalloc(sizeof(*drvinfo), GFP_KERNEL);
	if (inq_buff == NULL || drvinfo == NULL)
		goto mem_msg;

	/* testing to see if 16-byte CDBs are already being used */
	if (h->cciss_read == CCISS_READ_16) {
		cciss_read_capacity_16(h->ctlr, drv_index, 1,
			&total_size, &block_size);

	} else {
		cciss_read_capacity(ctlr, drv_index, 1,
				    &total_size, &block_size);

		/* if read_capacity returns all F's this volume is >2TB */
		/* in size so we switch to 16-byte CDB's for all */
		/* read/write ops */
		if (total_size == 0xFFFFFFFFULL) {
			cciss_read_capacity_16(ctlr, drv_index, 1,
			&total_size, &block_size);
			h->cciss_read = CCISS_READ_16;
			h->cciss_write = CCISS_WRITE_16;
		} else {
			h->cciss_read = CCISS_READ_10;
			h->cciss_write = CCISS_WRITE_10;
		}
	}

	cciss_geometry_inquiry(ctlr, drv_index, 1, total_size, block_size,
			       inq_buff, drvinfo);
	drvinfo->block_size = block_size;
	drvinfo->nr_blocks = total_size + 1;

	cciss_get_device_descr(ctlr, drv_index, 1, drvinfo->vendor,
				drvinfo->model, drvinfo->rev);
	cciss_get_serial_no(ctlr, drv_index, 1, drvinfo->serial_no,
			sizeof(drvinfo->serial_no));
	/* Save the lunid in case we deregister the disk, below. */
	memcpy(drvinfo->LunID, h->drv[drv_index]->LunID,
		sizeof(drvinfo->LunID));

	/* Is it the same disk we already know, and nothing's changed? */
	if (h->drv[drv_index]->raid_level != -1 &&
		((memcmp(drvinfo->serial_no,
				h->drv[drv_index]->serial_no, 16) == 0) &&
		drvinfo->block_size == h->drv[drv_index]->block_size &&
		drvinfo->nr_blocks == h->drv[drv_index]->nr_blocks &&
		drvinfo->heads == h->drv[drv_index]->heads &&
		drvinfo->sectors == h->drv[drv_index]->sectors &&
		drvinfo->cylinders == h->drv[drv_index]->cylinders))
			/* The disk is unchanged, nothing to update */
			goto freeret;

	/* If we get here it's not the same disk, or something's changed,
	 * so we need to * deregister it, and re-register it, if it's not
	 * in use.
	 * If the disk already exists then deregister it before proceeding
	 * (unless it's the first disk (for the controller node).
	 */
	if (h->drv[drv_index]->raid_level != -1 && drv_index != 0) {
		printk(KERN_WARNING "disk %d has changed.\n", drv_index);
		spin_lock_irqsave(CCISS_LOCK(h->ctlr), flags);
		h->drv[drv_index]->busy_configuring = 1;
		spin_unlock_irqrestore(CCISS_LOCK(h->ctlr), flags);

		/* deregister_disk sets h->drv[drv_index]->queue = NULL
		 * which keeps the interrupt handler from starting
		 * the queue.
		 */
		ret = deregister_disk(h, drv_index, 0, via_ioctl);
	}

	/* If the disk is in use return */
	if (ret)
		goto freeret;

	/* Save the new information from cciss_geometry_inquiry
	 * and serial number inquiry.  If the disk was deregistered
	 * above, then h->drv[drv_index] will be NULL.
	 */
	if (h->drv[drv_index] == NULL) {
		drvinfo->device_initialized = 0;
		h->drv[drv_index] = drvinfo;
		drvinfo = NULL; /* so it won't be freed below. */
	} else {
		/* special case for cxd0 */
		h->drv[drv_index]->block_size = drvinfo->block_size;
		h->drv[drv_index]->nr_blocks = drvinfo->nr_blocks;
		h->drv[drv_index]->heads = drvinfo->heads;
		h->drv[drv_index]->sectors = drvinfo->sectors;
		h->drv[drv_index]->cylinders = drvinfo->cylinders;
		h->drv[drv_index]->raid_level = drvinfo->raid_level;
		memcpy(h->drv[drv_index]->serial_no, drvinfo->serial_no, 16);
		memcpy(h->drv[drv_index]->vendor, drvinfo->vendor,
			VENDOR_LEN + 1);
		memcpy(h->drv[drv_index]->model, drvinfo->model, MODEL_LEN + 1);
		memcpy(h->drv[drv_index]->rev, drvinfo->rev, REV_LEN + 1);
	}

	++h->num_luns;
	disk = h->gendisk[drv_index];
	set_capacity(disk, h->drv[drv_index]->nr_blocks);

	/* If it's not disk 0 (drv_index != 0)
	 * or if it was disk 0, but there was previously
	 * no actual corresponding configured logical drive
	 * (raid_leve == -1) then we want to update the
	 * logical drive's information.
	 */
	if (drv_index || first_time) {
		if (cciss_add_disk(h, disk, drv_index) != 0) {
			cciss_free_gendisk(h, drv_index);
			cciss_free_drive_info(h, drv_index);
			printk(KERN_WARNING "cciss:%d could not update "
				"disk %d\n", h->ctlr, drv_index);
			--h->num_luns;
		}
	}

freeret:
	kfree(inq_buff);
	kfree(drvinfo);
	return;
mem_msg:
	printk(KERN_ERR "cciss: out of memory\n");
	goto freeret;
}

/* This function will find the first index of the controllers drive array
 * that has a null drv pointer and allocate the drive info struct and
 * will return that index   This is where new drives will be added.
 * If the index to be returned is greater than the highest_lun index for
 * the controller then highest_lun is set * to this new index.
 * If there are no available indexes or if tha allocation fails, then -1
 * is returned.  * "controller_node" is used to know if this is a real
 * logical drive, or just the controller node, which determines if this
 * counts towards highest_lun.
 */
static int cciss_alloc_drive_info(ctlr_info_t *h, int controller_node)
{
	int i;
	drive_info_struct *drv;

	/* Search for an empty slot for our drive info */
	for (i = 0; i < CISS_MAX_LUN; i++) {

		/* if not cxd0 case, and it's occupied, skip it. */
		if (h->drv[i] && i != 0)
			continue;
		/*
		 * If it's cxd0 case, and drv is alloc'ed already, and a
		 * disk is configured there, skip it.
		 */
		if (i == 0 && h->drv[i] && h->drv[i]->raid_level != -1)
			continue;

		/*
		 * We've found an empty slot.  Update highest_lun
		 * provided this isn't just the fake cxd0 controller node.
		 */
		if (i > h->highest_lun && !controller_node)
			h->highest_lun = i;

		/* If adding a real disk at cxd0, and it's already alloc'ed */
		if (i == 0 && h->drv[i] != NULL)
			return i;

		/*
		 * Found an empty slot, not already alloc'ed.  Allocate it.
		 * Mark it with raid_level == -1, so we know it's new later on.
		 */
		drv = kzalloc(sizeof(*drv), GFP_KERNEL);
		if (!drv)
			return -1;
		drv->raid_level = -1; /* so we know it's new */
		h->drv[i] = drv;
		return i;
	}
	return -1;
}

static void cciss_free_drive_info(ctlr_info_t *h, int drv_index)
{
	kfree(h->drv[drv_index]);
	h->drv[drv_index] = NULL;
}

static void cciss_free_gendisk(ctlr_info_t *h, int drv_index)
{
	put_disk(h->gendisk[drv_index]);
	h->gendisk[drv_index] = NULL;
}

/* cciss_add_gendisk finds a free hba[]->drv structure
 * and allocates a gendisk if needed, and sets the lunid
 * in the drvinfo structure.   It returns the index into
 * the ->drv[] array, or -1 if none are free.
 * is_controller_node indicates whether highest_lun should
 * count this disk, or if it's only being added to provide
 * a means to talk to the controller in case no logical
 * drives have yet been configured.
 */
static int cciss_add_gendisk(ctlr_info_t *h, unsigned char lunid[],
	int controller_node)
{
	int drv_index;

	drv_index = cciss_alloc_drive_info(h, controller_node);
	if (drv_index == -1)
		return -1;

	/*Check if the gendisk needs to be allocated */
	if (!h->gendisk[drv_index]) {
		h->gendisk[drv_index] =
			alloc_disk(1 << NWD_SHIFT);
		if (!h->gendisk[drv_index]) {
			printk(KERN_ERR "cciss%d: could not "
				"allocate a new disk %d\n",
				h->ctlr, drv_index);
			goto err_free_drive_info;
		}
	}
	memcpy(h->drv[drv_index]->LunID, lunid,
		sizeof(h->drv[drv_index]->LunID));
	if (cciss_create_ld_sysfs_entry(h, drv_index))
		goto err_free_disk;
	/* Don't need to mark this busy because nobody */
	/* else knows about this disk yet to contend */
	/* for access to it. */
	h->drv[drv_index]->busy_configuring = 0;
	wmb();
	return drv_index;

err_free_disk:
	cciss_free_gendisk(h, drv_index);
err_free_drive_info:
	cciss_free_drive_info(h, drv_index);
	return -1;
}

/* This is for the special case of a controller which
 * has no logical drives.  In this case, we still need
 * to register a disk so the controller can be accessed
 * by the Array Config Utility.
 */
static void cciss_add_controller_node(ctlr_info_t *h)
{
	struct gendisk *disk;
	int drv_index;

	if (h->gendisk[0] != NULL) /* already did this? Then bail. */
		return;

	drv_index = cciss_add_gendisk(h, CTLR_LUNID, 1);
	if (drv_index == -1)
		goto error;
	h->drv[drv_index]->block_size = 512;
	h->drv[drv_index]->nr_blocks = 0;
	h->drv[drv_index]->heads = 0;
	h->drv[drv_index]->sectors = 0;
	h->drv[drv_index]->cylinders = 0;
	h->drv[drv_index]->raid_level = -1;
	memset(h->drv[drv_index]->serial_no, 0, 16);
	disk = h->gendisk[drv_index];
	if (cciss_add_disk(h, disk, drv_index) == 0)
		return;
	cciss_free_gendisk(h, drv_index);
	cciss_free_drive_info(h, drv_index);
error:
	printk(KERN_WARNING "cciss%d: could not "
		"add disk 0.\n", h->ctlr);
	return;
}

/* This function will add and remove logical drives from the Logical
 * drive array of the controller and maintain persistency of ordering
 * so that mount points are preserved until the next reboot.  This allows
 * for the removal of logical drives in the middle of the drive array
 * without a re-ordering of those drives.
 * INPUT
 * h		= The controller to perform the operations on
 */
static int rebuild_lun_table(ctlr_info_t *h, int first_time,
	int via_ioctl)
{
	int ctlr = h->ctlr;
	int num_luns;
	ReportLunData_struct *ld_buff = NULL;
	int return_code;
	int listlength = 0;
	int i;
	int drv_found;
	int drv_index = 0;
	unsigned char lunid[8] = CTLR_LUNID;
	unsigned long flags;

	if (!capable(CAP_SYS_RAWIO))
		return -EPERM;

	/* Set busy_configuring flag for this operation */
	spin_lock_irqsave(CCISS_LOCK(h->ctlr), flags);
	if (h->busy_configuring) {
		spin_unlock_irqrestore(CCISS_LOCK(h->ctlr), flags);
		return -EBUSY;
	}
	h->busy_configuring = 1;
	spin_unlock_irqrestore(CCISS_LOCK(h->ctlr), flags);

	ld_buff = kzalloc(sizeof(ReportLunData_struct), GFP_KERNEL);
	if (ld_buff == NULL)
		goto mem_msg;

	return_code = sendcmd_withirq(CISS_REPORT_LOG, ctlr, ld_buff,
				      sizeof(ReportLunData_struct),
				      0, CTLR_LUNID, TYPE_CMD);

	if (return_code == IO_OK)
		listlength = be32_to_cpu(*(__be32 *) ld_buff->LUNListLength);
	else {	/* reading number of logical volumes failed */
		printk(KERN_WARNING "cciss: report logical volume"
		       " command failed\n");
		listlength = 0;
		goto freeret;
	}

	num_luns = listlength / 8;	/* 8 bytes per entry */
	if (num_luns > CISS_MAX_LUN) {
		num_luns = CISS_MAX_LUN;
		printk(KERN_WARNING "cciss: more luns configured"
		       " on controller than can be handled by"
		       " this driver.\n");
	}

	if (num_luns == 0)
		cciss_add_controller_node(h);

	/* Compare controller drive array to driver's drive array
	 * to see if any drives are missing on the controller due
	 * to action of Array Config Utility (user deletes drive)
	 * and deregister logical drives which have disappeared.
	 */
	for (i = 0; i <= h->highest_lun; i++) {
		int j;
		drv_found = 0;

		/* skip holes in the array from already deleted drives */
		if (h->drv[i] == NULL)
			continue;

		for (j = 0; j < num_luns; j++) {
			memcpy(lunid, &ld_buff->LUN[j][0], sizeof(lunid));
			if (memcmp(h->drv[i]->LunID, lunid,
				sizeof(lunid)) == 0) {
				drv_found = 1;
				break;
			}
		}
		if (!drv_found) {
			/* Deregister it from the OS, it's gone. */
			spin_lock_irqsave(CCISS_LOCK(h->ctlr), flags);
			h->drv[i]->busy_configuring = 1;
			spin_unlock_irqrestore(CCISS_LOCK(h->ctlr), flags);
			return_code = deregister_disk(h, i, 1, via_ioctl);
			if (h->drv[i] != NULL)
				h->drv[i]->busy_configuring = 0;
		}
	}

	/* Compare controller drive array to driver's drive array.
	 * Check for updates in the drive information and any new drives
	 * on the controller due to ACU adding logical drives, or changing
	 * a logical drive's size, etc.  Reregister any new/changed drives
	 */
	for (i = 0; i < num_luns; i++) {
		int j;

		drv_found = 0;

		memcpy(lunid, &ld_buff->LUN[i][0], sizeof(lunid));
		/* Find if the LUN is already in the drive array
		 * of the driver.  If so then update its info
		 * if not in use.  If it does not exist then find
		 * the first free index and add it.
		 */
		for (j = 0; j <= h->highest_lun; j++) {
			if (h->drv[j] != NULL &&
				memcmp(h->drv[j]->LunID, lunid,
					sizeof(h->drv[j]->LunID)) == 0) {
				drv_index = j;
				drv_found = 1;
				break;
			}
		}

		/* check if the drive was found already in the array */
		if (!drv_found) {
			drv_index = cciss_add_gendisk(h, lunid, 0);
			if (drv_index == -1)
				goto freeret;
		}
		cciss_update_drive_info(ctlr, drv_index, first_time,
			via_ioctl);
	}		/* end for */

freeret:
	kfree(ld_buff);
	h->busy_configuring = 0;
	/* We return -1 here to tell the ACU that we have registered/updated
	 * all of the drives that we can and to keep it from calling us
	 * additional times.
	 */
	return -1;
mem_msg:
	printk(KERN_ERR "cciss: out of memory\n");
	h->busy_configuring = 0;
	goto freeret;
}

static void cciss_clear_drive_info(drive_info_struct *drive_info)
{
	/* zero out the disk size info */
	drive_info->nr_blocks = 0;
	drive_info->block_size = 0;
	drive_info->heads = 0;
	drive_info->sectors = 0;
	drive_info->cylinders = 0;
	drive_info->raid_level = -1;
	memset(drive_info->serial_no, 0, sizeof(drive_info->serial_no));
	memset(drive_info->model, 0, sizeof(drive_info->model));
	memset(drive_info->rev, 0, sizeof(drive_info->rev));
	memset(drive_info->vendor, 0, sizeof(drive_info->vendor));
	/*
	 * don't clear the LUNID though, we need to remember which
	 * one this one is.
	 */
}

/* This function will deregister the disk and it's queue from the
 * kernel.  It must be called with the controller lock held and the
 * drv structures busy_configuring flag set.  It's parameters are:
 *
 * disk = This is the disk to be deregistered
 * drv  = This is the drive_info_struct associated with the disk to be
 *        deregistered.  It contains information about the disk used
 *        by the driver.
 * clear_all = This flag determines whether or not the disk information
 *             is going to be completely cleared out and the highest_lun
 *             reset.  Sometimes we want to clear out information about
 *             the disk in preparation for re-adding it.  In this case
 *             the highest_lun should be left unchanged and the LunID
 *             should not be cleared.
 * via_ioctl
 *    This indicates whether we've reached this path via ioctl.
 *    This affects the maximum usage count allowed for c0d0 to be messed with.
 *    If this path is reached via ioctl(), then the max_usage_count will
 *    be 1, as the process calling ioctl() has got to have the device open.
 *    If we get here via sysfs, then the max usage count will be zero.
*/
static int deregister_disk(ctlr_info_t *h, int drv_index,
			   int clear_all, int via_ioctl)
{
	int i;
	struct gendisk *disk;
	drive_info_struct *drv;
	int recalculate_highest_lun;

	if (!capable(CAP_SYS_RAWIO))
		return -EPERM;

	drv = h->drv[drv_index];
	disk = h->gendisk[drv_index];

	/* make sure logical volume is NOT is use */
	if (clear_all || (h->gendisk[0] == disk)) {
		if (drv->usage_count > via_ioctl)
			return -EBUSY;
	} else if (drv->usage_count > 0)
		return -EBUSY;

	recalculate_highest_lun = (drv == h->drv[h->highest_lun]);

	/* invalidate the devices and deregister the disk.  If it is disk
	 * zero do not deregister it but just zero out it's values.  This
	 * allows us to delete disk zero but keep the controller registered.
	 */
	if (h->gendisk[0] != disk) {
		struct request_queue *q = disk->queue;
		if (disk->flags & GENHD_FL_UP) {
			cciss_destroy_ld_sysfs_entry(h, drv_index, 0);
			del_gendisk(disk);
		}
		if (q)
			blk_cleanup_queue(q);
		/* If clear_all is set then we are deleting the logical
		 * drive, not just refreshing its info.  For drives
		 * other than disk 0 we will call put_disk.  We do not
		 * do this for disk 0 as we need it to be able to
		 * configure the controller.
		 */
		if (clear_all){
			/* This isn't pretty, but we need to find the
			 * disk in our array and NULL our the pointer.
			 * This is so that we will call alloc_disk if
			 * this index is used again later.
			 */
			for (i=0; i < CISS_MAX_LUN; i++){
				if (h->gendisk[i] == disk) {
					h->gendisk[i] = NULL;
					break;
				}
			}
			put_disk(disk);
		}
	} else {
		set_capacity(disk, 0);
		cciss_clear_drive_info(drv);
	}

	--h->num_luns;

	/* if it was the last disk, find the new hightest lun */
	if (clear_all && recalculate_highest_lun) {
		int i, newhighest = -1;
		for (i = 0; i <= h->highest_lun; i++) {
			/* if the disk has size > 0, it is available */
			if (h->drv[i] && h->drv[i]->heads)
				newhighest = i;
		}
		h->highest_lun = newhighest;
	}
	return 0;
}

static int fill_cmd(CommandList_struct *c, __u8 cmd, int ctlr, void *buff,
		size_t size, __u8 page_code, unsigned char *scsi3addr,
		int cmd_type)
{
	ctlr_info_t *h = hba[ctlr];
	u64bit buff_dma_handle;
	int status = IO_OK;

	c->cmd_type = CMD_IOCTL_PEND;
	c->Header.ReplyQueue = 0;
	if (buff != NULL) {
		c->Header.SGList = 1;
		c->Header.SGTotal = 1;
	} else {
		c->Header.SGList = 0;
		c->Header.SGTotal = 0;
	}
	c->Header.Tag.lower = c->busaddr;
	memcpy(c->Header.LUN.LunAddrBytes, scsi3addr, 8);

	c->Request.Type.Type = cmd_type;
	if (cmd_type == TYPE_CMD) {
		switch (cmd) {
		case CISS_INQUIRY:
			/* are we trying to read a vital product page */
			if (page_code != 0) {
				c->Request.CDB[1] = 0x01;
				c->Request.CDB[2] = page_code;
			}
			c->Request.CDBLen = 6;
			c->Request.Type.Attribute = ATTR_SIMPLE;
			c->Request.Type.Direction = XFER_READ;
			c->Request.Timeout = 0;
			c->Request.CDB[0] = CISS_INQUIRY;
			c->Request.CDB[4] = size & 0xFF;
			break;
		case CISS_REPORT_LOG:
		case CISS_REPORT_PHYS:
			/* Talking to controller so It's a physical command
			   mode = 00 target = 0.  Nothing to write.
			 */
			c->Request.CDBLen = 12;
			c->Request.Type.Attribute = ATTR_SIMPLE;
			c->Request.Type.Direction = XFER_READ;
			c->Request.Timeout = 0;
			c->Request.CDB[0] = cmd;
			c->Request.CDB[6] = (size >> 24) & 0xFF;	//MSB
			c->Request.CDB[7] = (size >> 16) & 0xFF;
			c->Request.CDB[8] = (size >> 8) & 0xFF;
			c->Request.CDB[9] = size & 0xFF;
			break;

		case CCISS_READ_CAPACITY:
			c->Request.CDBLen = 10;
			c->Request.Type.Attribute = ATTR_SIMPLE;
			c->Request.Type.Direction = XFER_READ;
			c->Request.Timeout = 0;
			c->Request.CDB[0] = cmd;
			break;
		case CCISS_READ_CAPACITY_16:
			c->Request.CDBLen = 16;
			c->Request.Type.Attribute = ATTR_SIMPLE;
			c->Request.Type.Direction = XFER_READ;
			c->Request.Timeout = 0;
			c->Request.CDB[0] = cmd;
			c->Request.CDB[1] = 0x10;
			c->Request.CDB[10] = (size >> 24) & 0xFF;
			c->Request.CDB[11] = (size >> 16) & 0xFF;
			c->Request.CDB[12] = (size >> 8) & 0xFF;
			c->Request.CDB[13] = size & 0xFF;
			c->Request.Timeout = 0;
			c->Request.CDB[0] = cmd;
			break;
		case CCISS_CACHE_FLUSH:
			c->Request.CDBLen = 12;
			c->Request.Type.Attribute = ATTR_SIMPLE;
			c->Request.Type.Direction = XFER_WRITE;
			c->Request.Timeout = 0;
			c->Request.CDB[0] = BMIC_WRITE;
			c->Request.CDB[6] = BMIC_CACHE_FLUSH;
			break;
		case TEST_UNIT_READY:
			c->Request.CDBLen = 6;
			c->Request.Type.Attribute = ATTR_SIMPLE;
			c->Request.Type.Direction = XFER_NONE;
			c->Request.Timeout = 0;
			break;
		default:
			printk(KERN_WARNING
			       "cciss%d:  Unknown Command 0x%c\n", ctlr, cmd);
			return IO_ERROR;
		}
	} else if (cmd_type == TYPE_MSG) {
		switch (cmd) {
		case 0:	/* ABORT message */
			c->Request.CDBLen = 12;
			c->Request.Type.Attribute = ATTR_SIMPLE;
			c->Request.Type.Direction = XFER_WRITE;
			c->Request.Timeout = 0;
			c->Request.CDB[0] = cmd;	/* abort */
			c->Request.CDB[1] = 0;	/* abort a command */
			/* buff contains the tag of the command to abort */
			memcpy(&c->Request.CDB[4], buff, 8);
			break;
		case 1:	/* RESET message */
			c->Request.CDBLen = 16;
			c->Request.Type.Attribute = ATTR_SIMPLE;
			c->Request.Type.Direction = XFER_NONE;
			c->Request.Timeout = 0;
			memset(&c->Request.CDB[0], 0, sizeof(c->Request.CDB));
			c->Request.CDB[0] = cmd;	/* reset */
			c->Request.CDB[1] = 0x03;	/* reset a target */
			break;
		case 3:	/* No-Op message */
			c->Request.CDBLen = 1;
			c->Request.Type.Attribute = ATTR_SIMPLE;
			c->Request.Type.Direction = XFER_WRITE;
			c->Request.Timeout = 0;
			c->Request.CDB[0] = cmd;
			break;
		default:
			printk(KERN_WARNING
			       "cciss%d: unknown message type %d\n", ctlr, cmd);
			return IO_ERROR;
		}
	} else {
		printk(KERN_WARNING
		       "cciss%d: unknown command type %d\n", ctlr, cmd_type);
		return IO_ERROR;
	}
	/* Fill in the scatter gather information */
	if (size > 0) {
		buff_dma_handle.val = (__u64) pci_map_single(h->pdev,
							     buff, size,
							     PCI_DMA_BIDIRECTIONAL);
		c->SG[0].Addr.lower = buff_dma_handle.val32.lower;
		c->SG[0].Addr.upper = buff_dma_handle.val32.upper;
		c->SG[0].Len = size;
		c->SG[0].Ext = 0;	/* we are not chaining */
	}
	return status;
}

static int check_target_status(ctlr_info_t *h, CommandList_struct *c)
{
	switch (c->err_info->ScsiStatus) {
	case SAM_STAT_GOOD:
		return IO_OK;
	case SAM_STAT_CHECK_CONDITION:
		switch (0xf & c->err_info->SenseInfo[2]) {
		case 0: return IO_OK; /* no sense */
		case 1: return IO_OK; /* recovered error */
		default:
			printk(KERN_WARNING "cciss%d: cmd 0x%02x "
				"check condition, sense key = 0x%02x\n",
				h->ctlr, c->Request.CDB[0],
				c->err_info->SenseInfo[2]);
		}
		break;
	default:
		printk(KERN_WARNING "cciss%d: cmd 0x%02x"
			"scsi status = 0x%02x\n", h->ctlr,
			c->Request.CDB[0], c->err_info->ScsiStatus);
		break;
	}
	return IO_ERROR;
}

static int process_sendcmd_error(ctlr_info_t *h, CommandList_struct *c)
{
	int return_status = IO_OK;

	if (c->err_info->CommandStatus == CMD_SUCCESS)
		return IO_OK;

	switch (c->err_info->CommandStatus) {
	case CMD_TARGET_STATUS:
		return_status = check_target_status(h, c);
		break;
	case CMD_DATA_UNDERRUN:
	case CMD_DATA_OVERRUN:
		/* expected for inquiry and report lun commands */
		break;
	case CMD_INVALID:
		printk(KERN_WARNING "cciss: cmd 0x%02x is "
		       "reported invalid\n", c->Request.CDB[0]);
		return_status = IO_ERROR;
		break;
	case CMD_PROTOCOL_ERR:
		printk(KERN_WARNING "cciss: cmd 0x%02x has "
		       "protocol error \n", c->Request.CDB[0]);
		return_status = IO_ERROR;
		break;
	case CMD_HARDWARE_ERR:
		printk(KERN_WARNING "cciss: cmd 0x%02x had "
		       " hardware error\n", c->Request.CDB[0]);
		return_status = IO_ERROR;
		break;
	case CMD_CONNECTION_LOST:
		printk(KERN_WARNING "cciss: cmd 0x%02x had "
		       "connection lost\n", c->Request.CDB[0]);
		return_status = IO_ERROR;
		break;
	case CMD_ABORTED:
		printk(KERN_WARNING "cciss: cmd 0x%02x was "
		       "aborted\n", c->Request.CDB[0]);
		return_status = IO_ERROR;
		break;
	case CMD_ABORT_FAILED:
		printk(KERN_WARNING "cciss: cmd 0x%02x reports "
		       "abort failed\n", c->Request.CDB[0]);
		return_status = IO_ERROR;
		break;
	case CMD_UNSOLICITED_ABORT:
		printk(KERN_WARNING
		       "cciss%d: unsolicited abort 0x%02x\n", h->ctlr,
			c->Request.CDB[0]);
		return_status = IO_NEEDS_RETRY;
		break;
	default:
		printk(KERN_WARNING "cciss: cmd 0x%02x returned "
		       "unknown status %x\n", c->Request.CDB[0],
		       c->err_info->CommandStatus);
		return_status = IO_ERROR;
	}
	return return_status;
}

static int sendcmd_withirq_core(ctlr_info_t *h, CommandList_struct *c,
	int attempt_retry)
{
	DECLARE_COMPLETION_ONSTACK(wait);
	u64bit buff_dma_handle;
	unsigned long flags;
	int return_status = IO_OK;

resend_cmd2:
	c->waiting = &wait;
	/* Put the request on the tail of the queue and send it */
	spin_lock_irqsave(CCISS_LOCK(h->ctlr), flags);
	addQ(&h->reqQ, c);
	h->Qdepth++;
	start_io(h);
	spin_unlock_irqrestore(CCISS_LOCK(h->ctlr), flags);

	wait_for_completion(&wait);

	if (c->err_info->CommandStatus == 0 || !attempt_retry)
		goto command_done;

	return_status = process_sendcmd_error(h, c);

	if (return_status == IO_NEEDS_RETRY &&
		c->retry_count < MAX_CMD_RETRIES) {
		printk(KERN_WARNING "cciss%d: retrying 0x%02x\n", h->ctlr,
			c->Request.CDB[0]);
		c->retry_count++;
		/* erase the old error information */
		memset(c->err_info, 0, sizeof(ErrorInfo_struct));
		return_status = IO_OK;
		INIT_COMPLETION(wait);
		goto resend_cmd2;
	}

command_done:
	/* unlock the buffers from DMA */
	buff_dma_handle.val32.lower = c->SG[0].Addr.lower;
	buff_dma_handle.val32.upper = c->SG[0].Addr.upper;
	pci_unmap_single(h->pdev, (dma_addr_t) buff_dma_handle.val,
			 c->SG[0].Len, PCI_DMA_BIDIRECTIONAL);
	return return_status;
}

static int sendcmd_withirq(__u8 cmd, int ctlr, void *buff, size_t size,
			   __u8 page_code, unsigned char scsi3addr[],
			int cmd_type)
{
	ctlr_info_t *h = hba[ctlr];
	CommandList_struct *c;
	int return_status;

	c = cmd_alloc(h, 0);
	if (!c)
		return -ENOMEM;
	return_status = fill_cmd(c, cmd, ctlr, buff, size, page_code,
		scsi3addr, cmd_type);
	if (return_status == IO_OK)
		return_status = sendcmd_withirq_core(h, c, 1);

	cmd_free(h, c, 0);
	return return_status;
}

static void cciss_geometry_inquiry(int ctlr, int logvol,
				   int withirq, sector_t total_size,
				   unsigned int block_size,
				   InquiryData_struct *inq_buff,
				   drive_info_struct *drv)
{
	int return_code;
	unsigned long t;
	unsigned char scsi3addr[8];

	memset(inq_buff, 0, sizeof(InquiryData_struct));
	log_unit_to_scsi3addr(hba[ctlr], scsi3addr, logvol);
	if (withirq)
		return_code = sendcmd_withirq(CISS_INQUIRY, ctlr,
					      inq_buff, sizeof(*inq_buff),
					      0xC1, scsi3addr, TYPE_CMD);
	else
		return_code = sendcmd(CISS_INQUIRY, ctlr, inq_buff,
				      sizeof(*inq_buff), 0xC1, scsi3addr,
				      TYPE_CMD);
	if (return_code == IO_OK) {
		if (inq_buff->data_byte[8] == 0xFF) {
			printk(KERN_WARNING
			       "cciss: reading geometry failed, volume "
			       "does not support reading geometry\n");
			drv->heads = 255;
			drv->sectors = 32;	// Sectors per track
			drv->cylinders = total_size + 1;
			drv->raid_level = RAID_UNKNOWN;
		} else {
			drv->heads = inq_buff->data_byte[6];
			drv->sectors = inq_buff->data_byte[7];
			drv->cylinders = (inq_buff->data_byte[4] & 0xff) << 8;
			drv->cylinders += inq_buff->data_byte[5];
			drv->raid_level = inq_buff->data_byte[8];
		}
		drv->block_size = block_size;
		drv->nr_blocks = total_size + 1;
		t = drv->heads * drv->sectors;
		if (t > 1) {
			sector_t real_size = total_size + 1;
			unsigned long rem = sector_div(real_size, t);
			if (rem)
				real_size++;
			drv->cylinders = real_size;
		}
	} else {		/* Get geometry failed */
		printk(KERN_WARNING "cciss: reading geometry failed\n");
	}
}

static void
cciss_read_capacity(int ctlr, int logvol, int withirq, sector_t *total_size,
		    unsigned int *block_size)
{
	ReadCapdata_struct *buf;
	int return_code;
	unsigned char scsi3addr[8];

	buf = kzalloc(sizeof(ReadCapdata_struct), GFP_KERNEL);
	if (!buf) {
		printk(KERN_WARNING "cciss: out of memory\n");
		return;
	}

	log_unit_to_scsi3addr(hba[ctlr], scsi3addr, logvol);
	if (withirq)
		return_code = sendcmd_withirq(CCISS_READ_CAPACITY,
				ctlr, buf, sizeof(ReadCapdata_struct),
					0, scsi3addr, TYPE_CMD);
	else
		return_code = sendcmd(CCISS_READ_CAPACITY,
				ctlr, buf, sizeof(ReadCapdata_struct),
					0, scsi3addr, TYPE_CMD);
	if (return_code == IO_OK) {
		*total_size = be32_to_cpu(*(__be32 *) buf->total_size);
		*block_size = be32_to_cpu(*(__be32 *) buf->block_size);
	} else {		/* read capacity command failed */
		printk(KERN_WARNING "cciss: read capacity failed\n");
		*total_size = 0;
		*block_size = BLOCK_SIZE;
	}
	kfree(buf);
}

static void
cciss_read_capacity_16(int ctlr, int logvol, int withirq, sector_t *total_size, 				unsigned int *block_size)
{
	ReadCapdata_struct_16 *buf;
	int return_code;
	unsigned char scsi3addr[8];

	buf = kzalloc(sizeof(ReadCapdata_struct_16), GFP_KERNEL);
	if (!buf) {
		printk(KERN_WARNING "cciss: out of memory\n");
		return;
	}

	log_unit_to_scsi3addr(hba[ctlr], scsi3addr, logvol);
	if (withirq) {
		return_code = sendcmd_withirq(CCISS_READ_CAPACITY_16,
			ctlr, buf, sizeof(ReadCapdata_struct_16),
				0, scsi3addr, TYPE_CMD);
	}
	else {
		return_code = sendcmd(CCISS_READ_CAPACITY_16,
			ctlr, buf, sizeof(ReadCapdata_struct_16),
				0, scsi3addr, TYPE_CMD);
	}
	if (return_code == IO_OK) {
		*total_size = be64_to_cpu(*(__be64 *) buf->total_size);
		*block_size = be32_to_cpu(*(__be32 *) buf->block_size);
	} else {		/* read capacity command failed */
		printk(KERN_WARNING "cciss: read capacity failed\n");
		*total_size = 0;
		*block_size = BLOCK_SIZE;
	}
	printk(KERN_INFO "      blocks= %llu block_size= %d\n",
	       (unsigned long long)*total_size+1, *block_size);
	kfree(buf);
}

static int cciss_revalidate(struct gendisk *disk)
{
	ctlr_info_t *h = get_host(disk);
	drive_info_struct *drv = get_drv(disk);
	int logvol;
	int FOUND = 0;
	unsigned int block_size;
	sector_t total_size;
	InquiryData_struct *inq_buff = NULL;

	for (logvol = 0; logvol < CISS_MAX_LUN; logvol++) {
		if (memcmp(h->drv[logvol]->LunID, drv->LunID,
			sizeof(drv->LunID)) == 0) {
			FOUND = 1;
			break;
		}
	}

	if (!FOUND)
		return 1;

	inq_buff = kmalloc(sizeof(InquiryData_struct), GFP_KERNEL);
	if (inq_buff == NULL) {
		printk(KERN_WARNING "cciss: out of memory\n");
		return 1;
	}
	if (h->cciss_read == CCISS_READ_10) {
		cciss_read_capacity(h->ctlr, logvol, 1,
					&total_size, &block_size);
	} else {
		cciss_read_capacity_16(h->ctlr, logvol, 1,
					&total_size, &block_size);
	}
	cciss_geometry_inquiry(h->ctlr, logvol, 1, total_size, block_size,
			       inq_buff, drv);

	blk_queue_logical_block_size(drv->queue, drv->block_size);
	set_capacity(disk, drv->nr_blocks);

	kfree(inq_buff);
	return 0;
}

/*
 *   Wait polling for a command to complete.
 *   The memory mapped FIFO is polled for the completion.
 *   Used only at init time, interrupts from the HBA are disabled.
 */
static unsigned long pollcomplete(int ctlr)
{
	unsigned long done;
	int i;

	/* Wait (up to 20 seconds) for a command to complete */

	for (i = 20 * HZ; i > 0; i--) {
		done = hba[ctlr]->access.command_completed(hba[ctlr]);
		if (done == FIFO_EMPTY)
			schedule_timeout_uninterruptible(1);
		else
			return done;
	}
	/* Invalid address to tell caller we ran out of time */
	return 1;
}

/* Send command c to controller h and poll for it to complete.
 * Turns interrupts off on the board.  Used at driver init time
 * and during SCSI error recovery.
 */
static int sendcmd_core(ctlr_info_t *h, CommandList_struct *c)
{
	int i;
	unsigned long complete;
	int status = IO_ERROR;
	u64bit buff_dma_handle;

resend_cmd1:

	/* Disable interrupt on the board. */
	h->access.set_intr_mask(h, CCISS_INTR_OFF);

	/* Make sure there is room in the command FIFO */
	/* Actually it should be completely empty at this time */
	/* unless we are in here doing error handling for the scsi */
	/* tape side of the driver. */
	for (i = 200000; i > 0; i--) {
		/* if fifo isn't full go */
		if (!(h->access.fifo_full(h)))
			break;
		udelay(10);
		printk(KERN_WARNING "cciss cciss%d: SendCmd FIFO full,"
		       " waiting!\n", h->ctlr);
	}
	h->access.submit_command(h, c); /* Send the cmd */
	do {
		complete = pollcomplete(h->ctlr);

#ifdef CCISS_DEBUG
		printk(KERN_DEBUG "cciss: command completed\n");
#endif				/* CCISS_DEBUG */

		if (complete == 1) {
			printk(KERN_WARNING
			       "cciss cciss%d: SendCmd Timeout out, "
			       "No command list address returned!\n", h->ctlr);
			status = IO_ERROR;
			break;
		}

		/* Make sure it's the command we're expecting. */
		if ((complete & ~CISS_ERROR_BIT) != c->busaddr) {
			printk(KERN_WARNING "cciss%d: Unexpected command "
				"completion.\n", h->ctlr);
			continue;
		}

		/* It is our command.  If no error, we're done. */
		if (!(complete & CISS_ERROR_BIT)) {
			status = IO_OK;
			break;
		}

		/* There is an error... */

		/* if data overrun or underun on Report command ignore it */
		if (((c->Request.CDB[0] == CISS_REPORT_LOG) ||
		     (c->Request.CDB[0] == CISS_REPORT_PHYS) ||
		     (c->Request.CDB[0] == CISS_INQUIRY)) &&
			((c->err_info->CommandStatus == CMD_DATA_OVERRUN) ||
			 (c->err_info->CommandStatus == CMD_DATA_UNDERRUN))) {
			complete = c->busaddr;
			status = IO_OK;
			break;
		}

		if (c->err_info->CommandStatus == CMD_UNSOLICITED_ABORT) {
			printk(KERN_WARNING "cciss%d: unsolicited abort %p\n",
				h->ctlr, c);
			if (c->retry_count < MAX_CMD_RETRIES) {
				printk(KERN_WARNING "cciss%d: retrying %p\n",
				   h->ctlr, c);
				c->retry_count++;
				/* erase the old error information */
				memset(c->err_info, 0, sizeof(c->err_info));
				goto resend_cmd1;
			}
			printk(KERN_WARNING "cciss%d: retried %p too many "
				"times\n", h->ctlr, c);
			status = IO_ERROR;
			break;
		}

		if (c->err_info->CommandStatus == CMD_UNABORTABLE) {
			printk(KERN_WARNING "cciss%d: command could not be "
				"aborted.\n", h->ctlr);
			status = IO_ERROR;
			break;
		}

		if (c->err_info->CommandStatus == CMD_TARGET_STATUS) {
			status = check_target_status(h, c);
			break;
		}

		printk(KERN_WARNING "cciss%d: sendcmd error\n", h->ctlr);
		printk(KERN_WARNING "cmd = 0x%02x, CommandStatus = 0x%02x\n",
			c->Request.CDB[0], c->err_info->CommandStatus);
		status = IO_ERROR;
		break;

	} while (1);

	/* unlock the data buffer from DMA */
	buff_dma_handle.val32.lower = c->SG[0].Addr.lower;
	buff_dma_handle.val32.upper = c->SG[0].Addr.upper;
	pci_unmap_single(h->pdev, (dma_addr_t) buff_dma_handle.val,
			 c->SG[0].Len, PCI_DMA_BIDIRECTIONAL);
	return status;
}

/*
 * Send a command to the controller, and wait for it to complete.
 * Used at init time, and during SCSI error recovery.
 */
static int sendcmd(__u8 cmd, int ctlr, void *buff, size_t size,
	__u8 page_code, unsigned char *scsi3addr, int cmd_type)
{
	CommandList_struct *c;
	int status;

	c = cmd_alloc(hba[ctlr], 1);
	if (!c) {
		printk(KERN_WARNING "cciss: unable to get memory");
		return IO_ERROR;
	}
	status = fill_cmd(c, cmd, ctlr, buff, size, page_code,
		scsi3addr, cmd_type);
	if (status == IO_OK)
		status = sendcmd_core(hba[ctlr], c);
	cmd_free(hba[ctlr], c, 1);
	return status;
}

/*
 * Map (physical) PCI mem into (virtual) kernel space
 */
static void __iomem *remap_pci_mem(ulong base, ulong size)
{
	ulong page_base = ((ulong) base) & PAGE_MASK;
	ulong page_offs = ((ulong) base) - page_base;
	void __iomem *page_remapped = ioremap(page_base, page_offs + size);

	return page_remapped ? (page_remapped + page_offs) : NULL;
}

/*
 * Takes jobs of the Q and sends them to the hardware, then puts it on
 * the Q to wait for completion.
 */
static void start_io(ctlr_info_t *h)
{
	CommandList_struct *c;

	while (!hlist_empty(&h->reqQ)) {
		c = hlist_entry(h->reqQ.first, CommandList_struct, list);
		/* can't do anything if fifo is full */
		if ((h->access.fifo_full(h))) {
			printk(KERN_WARNING "cciss: fifo full\n");
			break;
		}

		/* Get the first entry from the Request Q */
		removeQ(c);
		h->Qdepth--;

		/* Tell the controller execute command */
		h->access.submit_command(h, c);

		/* Put job onto the completed Q */
		addQ(&h->cmpQ, c);
	}
}

/* Assumes that CCISS_LOCK(h->ctlr) is held. */
/* Zeros out the error record and then resends the command back */
/* to the controller */
static inline void resend_cciss_cmd(ctlr_info_t *h, CommandList_struct *c)
{
	/* erase the old error information */
	memset(c->err_info, 0, sizeof(ErrorInfo_struct));

	/* add it to software queue and then send it to the controller */
	addQ(&h->reqQ, c);
	h->Qdepth++;
	if (h->Qdepth > h->maxQsinceinit)
		h->maxQsinceinit = h->Qdepth;

	start_io(h);
}

static inline unsigned int make_status_bytes(unsigned int scsi_status_byte,
	unsigned int msg_byte, unsigned int host_byte,
	unsigned int driver_byte)
{
	/* inverse of macros in scsi.h */
	return (scsi_status_byte & 0xff) |
		((msg_byte & 0xff) << 8) |
		((host_byte & 0xff) << 16) |
		((driver_byte & 0xff) << 24);
}

static inline int evaluate_target_status(ctlr_info_t *h,
			CommandList_struct *cmd, int *retry_cmd)
{
	unsigned char sense_key;
	unsigned char status_byte, msg_byte, host_byte, driver_byte;
	int error_value;

	*retry_cmd = 0;
	/* If we get in here, it means we got "target status", that is, scsi status */
	status_byte = cmd->err_info->ScsiStatus;
	driver_byte = DRIVER_OK;
	msg_byte = cmd->err_info->CommandStatus; /* correct?  seems too device specific */

	if (blk_pc_request(cmd->rq))
		host_byte = DID_PASSTHROUGH;
	else
		host_byte = DID_OK;

	error_value = make_status_bytes(status_byte, msg_byte,
		host_byte, driver_byte);

	if (cmd->err_info->ScsiStatus != SAM_STAT_CHECK_CONDITION) {
		if (!blk_pc_request(cmd->rq))
			printk(KERN_WARNING "cciss: cmd %p "
			       "has SCSI Status 0x%x\n",
			       cmd, cmd->err_info->ScsiStatus);
		return error_value;
	}

	/* check the sense key */
	sense_key = 0xf & cmd->err_info->SenseInfo[2];
	/* no status or recovered error */
	if (((sense_key == 0x0) || (sense_key == 0x1)) && !blk_pc_request(cmd->rq))
		error_value = 0;

	if (check_for_unit_attention(h, cmd)) {
		*retry_cmd = !blk_pc_request(cmd->rq);
		return 0;
	}

	if (!blk_pc_request(cmd->rq)) { /* Not SG_IO or similar? */
		if (error_value != 0)
			printk(KERN_WARNING "cciss: cmd %p has CHECK CONDITION"
			       " sense key = 0x%x\n", cmd, sense_key);
		return error_value;
	}

	/* SG_IO or similar, copy sense data back */
	if (cmd->rq->sense) {
		if (cmd->rq->sense_len > cmd->err_info->SenseLen)
			cmd->rq->sense_len = cmd->err_info->SenseLen;
		memcpy(cmd->rq->sense, cmd->err_info->SenseInfo,
			cmd->rq->sense_len);
	} else
		cmd->rq->sense_len = 0;

	return error_value;
}

/* checks the status of the job and calls complete buffers to mark all
 * buffers for the completed job. Note that this function does not need
 * to hold the hba/queue lock.
 */
static inline void complete_command(ctlr_info_t *h, CommandList_struct *cmd,
				    int timeout)
{
	int retry_cmd = 0;
	struct request *rq = cmd->rq;

	rq->errors = 0;

	if (timeout)
		rq->errors = make_status_bytes(0, 0, 0, DRIVER_TIMEOUT);

	if (cmd->err_info->CommandStatus == 0)	/* no error has occurred */
		goto after_error_processing;

	switch (cmd->err_info->CommandStatus) {
	case CMD_TARGET_STATUS:
		rq->errors = evaluate_target_status(h, cmd, &retry_cmd);
		break;
	case CMD_DATA_UNDERRUN:
		if (blk_fs_request(cmd->rq)) {
			printk(KERN_WARNING "cciss: cmd %p has"
			       " completed with data underrun "
			       "reported\n", cmd);
			cmd->rq->resid_len = cmd->err_info->ResidualCnt;
		}
		break;
	case CMD_DATA_OVERRUN:
		if (blk_fs_request(cmd->rq))
			printk(KERN_WARNING "cciss: cmd %p has"
			       " completed with data overrun "
			       "reported\n", cmd);
		break;
	case CMD_INVALID:
		printk(KERN_WARNING "cciss: cmd %p is "
		       "reported invalid\n", cmd);
		rq->errors = make_status_bytes(SAM_STAT_GOOD,
			cmd->err_info->CommandStatus, DRIVER_OK,
			blk_pc_request(cmd->rq) ? DID_PASSTHROUGH : DID_ERROR);
		break;
	case CMD_PROTOCOL_ERR:
		printk(KERN_WARNING "cciss: cmd %p has "
		       "protocol error \n", cmd);
		rq->errors = make_status_bytes(SAM_STAT_GOOD,
			cmd->err_info->CommandStatus, DRIVER_OK,
			blk_pc_request(cmd->rq) ? DID_PASSTHROUGH : DID_ERROR);
		break;
	case CMD_HARDWARE_ERR:
		printk(KERN_WARNING "cciss: cmd %p had "
		       " hardware error\n", cmd);
		rq->errors = make_status_bytes(SAM_STAT_GOOD,
			cmd->err_info->CommandStatus, DRIVER_OK,
			blk_pc_request(cmd->rq) ? DID_PASSTHROUGH : DID_ERROR);
		break;
	case CMD_CONNECTION_LOST:
		printk(KERN_WARNING "cciss: cmd %p had "
		       "connection lost\n", cmd);
		rq->errors = make_status_bytes(SAM_STAT_GOOD,
			cmd->err_info->CommandStatus, DRIVER_OK,
			blk_pc_request(cmd->rq) ? DID_PASSTHROUGH : DID_ERROR);
		break;
	case CMD_ABORTED:
		printk(KERN_WARNING "cciss: cmd %p was "
		       "aborted\n", cmd);
		rq->errors = make_status_bytes(SAM_STAT_GOOD,
			cmd->err_info->CommandStatus, DRIVER_OK,
			blk_pc_request(cmd->rq) ? DID_PASSTHROUGH : DID_ABORT);
		break;
	case CMD_ABORT_FAILED:
		printk(KERN_WARNING "cciss: cmd %p reports "
		       "abort failed\n", cmd);
		rq->errors = make_status_bytes(SAM_STAT_GOOD,
			cmd->err_info->CommandStatus, DRIVER_OK,
			blk_pc_request(cmd->rq) ? DID_PASSTHROUGH : DID_ERROR);
		break;
	case CMD_UNSOLICITED_ABORT:
		printk(KERN_WARNING "cciss%d: unsolicited "
		       "abort %p\n", h->ctlr, cmd);
		if (cmd->retry_count < MAX_CMD_RETRIES) {
			retry_cmd = 1;
			printk(KERN_WARNING
			       "cciss%d: retrying %p\n", h->ctlr, cmd);
			cmd->retry_count++;
		} else
			printk(KERN_WARNING
			       "cciss%d: %p retried too "
			       "many times\n", h->ctlr, cmd);
		rq->errors = make_status_bytes(SAM_STAT_GOOD,
			cmd->err_info->CommandStatus, DRIVER_OK,
			blk_pc_request(cmd->rq) ? DID_PASSTHROUGH : DID_ABORT);
		break;
	case CMD_TIMEOUT:
		printk(KERN_WARNING "cciss: cmd %p timedout\n", cmd);
		rq->errors = make_status_bytes(SAM_STAT_GOOD,
			cmd->err_info->CommandStatus, DRIVER_OK,
			blk_pc_request(cmd->rq) ? DID_PASSTHROUGH : DID_ERROR);
		break;
	default:
		printk(KERN_WARNING "cciss: cmd %p returned "
		       "unknown status %x\n", cmd,
		       cmd->err_info->CommandStatus);
		rq->errors = make_status_bytes(SAM_STAT_GOOD,
			cmd->err_info->CommandStatus, DRIVER_OK,
			blk_pc_request(cmd->rq) ? DID_PASSTHROUGH : DID_ERROR);
	}

after_error_processing:

	/* We need to return this command */
	if (retry_cmd) {
		resend_cciss_cmd(h, cmd);
		return;
	}
	cmd->rq->completion_data = cmd;
	blk_complete_request(cmd->rq);
}

/*
 * Get a request and submit it to the controller.
 */
static void do_cciss_request(struct request_queue *q)
{
	ctlr_info_t *h = q->queuedata;
	CommandList_struct *c;
	sector_t start_blk;
	int seg;
	struct request *creq;
	u64bit temp64;
	struct scatterlist tmp_sg[MAXSGENTRIES];
	drive_info_struct *drv;
	int i, dir;

	/* We call start_io here in case there is a command waiting on the
	 * queue that has not been sent.
	 */
	if (blk_queue_plugged(q))
		goto startio;

      queue:
	creq = blk_peek_request(q);
	if (!creq)
		goto startio;

	BUG_ON(creq->nr_phys_segments > MAXSGENTRIES);

	if ((c = cmd_alloc(h, 1)) == NULL)
		goto full;

	blk_start_request(creq);

	spin_unlock_irq(q->queue_lock);

	c->cmd_type = CMD_RWREQ;
	c->rq = creq;

	/* fill in the request */
	drv = creq->rq_disk->private_data;
	c->Header.ReplyQueue = 0;	// unused in simple mode
	/* got command from pool, so use the command block index instead */
	/* for direct lookups. */
	/* The first 2 bits are reserved for controller error reporting. */
	c->Header.Tag.lower = (c->cmdindex << 3);
	c->Header.Tag.lower |= 0x04;	/* flag for direct lookup. */
	memcpy(&c->Header.LUN, drv->LunID, sizeof(drv->LunID));
	c->Request.CDBLen = 10;	// 12 byte commands not in FW yet;
	c->Request.Type.Type = TYPE_CMD;	// It is a command.
	c->Request.Type.Attribute = ATTR_SIMPLE;
	c->Request.Type.Direction =
	    (rq_data_dir(creq) == READ) ? XFER_READ : XFER_WRITE;
	c->Request.Timeout = 0;	// Don't time out
	c->Request.CDB[0] =
	    (rq_data_dir(creq) == READ) ? h->cciss_read : h->cciss_write;
	start_blk = blk_rq_pos(creq);
#ifdef CCISS_DEBUG
	printk(KERN_DEBUG "ciss: sector =%d nr_sectors=%d\n",
	       (int)blk_rq_pos(creq), (int)blk_rq_sectors(creq));
#endif				/* CCISS_DEBUG */

	sg_init_table(tmp_sg, MAXSGENTRIES);
	seg = blk_rq_map_sg(q, creq, tmp_sg);

	/* get the DMA records for the setup */
	if (c->Request.Type.Direction == XFER_READ)
		dir = PCI_DMA_FROMDEVICE;
	else
		dir = PCI_DMA_TODEVICE;

	for (i = 0; i < seg; i++) {
		c->SG[i].Len = tmp_sg[i].length;
		temp64.val = (__u64) pci_map_page(h->pdev, sg_page(&tmp_sg[i]),
						  tmp_sg[i].offset,
						  tmp_sg[i].length, dir);
		c->SG[i].Addr.lower = temp64.val32.lower;
		c->SG[i].Addr.upper = temp64.val32.upper;
		c->SG[i].Ext = 0;	// we are not chaining
	}
	/* track how many SG entries we are using */
	if (seg > h->maxSG)
		h->maxSG = seg;

#ifdef CCISS_DEBUG
	printk(KERN_DEBUG "cciss: Submitting %u sectors in %d segments\n",
	       blk_rq_sectors(creq), seg);
#endif				/* CCISS_DEBUG */

	c->Header.SGList = c->Header.SGTotal = seg;
	if (likely(blk_fs_request(creq))) {
		if(h->cciss_read == CCISS_READ_10) {
			c->Request.CDB[1] = 0;
			c->Request.CDB[2] = (start_blk >> 24) & 0xff;	//MSB
			c->Request.CDB[3] = (start_blk >> 16) & 0xff;
			c->Request.CDB[4] = (start_blk >> 8) & 0xff;
			c->Request.CDB[5] = start_blk & 0xff;
			c->Request.CDB[6] = 0;	// (sect >> 24) & 0xff; MSB
			c->Request.CDB[7] = (blk_rq_sectors(creq) >> 8) & 0xff;
			c->Request.CDB[8] = blk_rq_sectors(creq) & 0xff;
			c->Request.CDB[9] = c->Request.CDB[11] = c->Request.CDB[12] = 0;
		} else {
			u32 upper32 = upper_32_bits(start_blk);

			c->Request.CDBLen = 16;
			c->Request.CDB[1]= 0;
			c->Request.CDB[2]= (upper32 >> 24) & 0xff;	//MSB
			c->Request.CDB[3]= (upper32 >> 16) & 0xff;
			c->Request.CDB[4]= (upper32 >>  8) & 0xff;
			c->Request.CDB[5]= upper32 & 0xff;
			c->Request.CDB[6]= (start_blk >> 24) & 0xff;
			c->Request.CDB[7]= (start_blk >> 16) & 0xff;
			c->Request.CDB[8]= (start_blk >>  8) & 0xff;
			c->Request.CDB[9]= start_blk & 0xff;
			c->Request.CDB[10]= (blk_rq_sectors(creq) >> 24) & 0xff;
			c->Request.CDB[11]= (blk_rq_sectors(creq) >> 16) & 0xff;
			c->Request.CDB[12]= (blk_rq_sectors(creq) >>  8) & 0xff;
			c->Request.CDB[13]= blk_rq_sectors(creq) & 0xff;
			c->Request.CDB[14] = c->Request.CDB[15] = 0;
		}
	} else if (blk_pc_request(creq)) {
		c->Request.CDBLen = creq->cmd_len;
		memcpy(c->Request.CDB, creq->cmd, BLK_MAX_CDB);
	} else {
		printk(KERN_WARNING "cciss%d: bad request type %d\n", h->ctlr, creq->cmd_type);
		BUG();
	}

	spin_lock_irq(q->queue_lock);

	addQ(&h->reqQ, c);
	h->Qdepth++;
	if (h->Qdepth > h->maxQsinceinit)
		h->maxQsinceinit = h->Qdepth;

	goto queue;
full:
	blk_stop_queue(q);
startio:
	/* We will already have the driver lock here so not need
	 * to lock it.
	 */
	start_io(h);
}

static inline unsigned long get_next_completion(ctlr_info_t *h)
{
	return h->access.command_completed(h);
}

static inline int interrupt_pending(ctlr_info_t *h)
{
	return h->access.intr_pending(h);
}

static inline long interrupt_not_for_us(ctlr_info_t *h)
{
	return (((h->access.intr_pending(h) == 0) ||
		 (h->interrupts_enabled == 0)));
}

static irqreturn_t do_cciss_intr(int irq, void *dev_id)
{
	ctlr_info_t *h = dev_id;
	CommandList_struct *c;
	unsigned long flags;
	__u32 a, a1, a2;

	if (interrupt_not_for_us(h))
		return IRQ_NONE;
	/*
	 * If there are completed commands in the completion queue,
	 * we had better do something about it.
	 */
	spin_lock_irqsave(CCISS_LOCK(h->ctlr), flags);
	while (interrupt_pending(h)) {
		while ((a = get_next_completion(h)) != FIFO_EMPTY) {
			a1 = a;
			if ((a & 0x04)) {
				a2 = (a >> 3);
				if (a2 >= h->nr_cmds) {
					printk(KERN_WARNING
					       "cciss: controller cciss%d failed, stopping.\n",
					       h->ctlr);
					fail_all_cmds(h->ctlr);
					return IRQ_HANDLED;
				}

				c = h->cmd_pool + a2;
				a = c->busaddr;

			} else {
				struct hlist_node *tmp;

				a &= ~3;
				c = NULL;
				hlist_for_each_entry(c, tmp, &h->cmpQ, list) {
					if (c->busaddr == a)
						break;
				}
			}
			/*
			 * If we've found the command, take it off the
			 * completion Q and free it
			 */
			if (c && c->busaddr == a) {
				removeQ(c);
				if (c->cmd_type == CMD_RWREQ) {
					complete_command(h, c, 0);
				} else if (c->cmd_type == CMD_IOCTL_PEND) {
					complete(c->waiting);
				}
#				ifdef CONFIG_CISS_SCSI_TAPE
				else if (c->cmd_type == CMD_SCSI)
					complete_scsi_command(c, 0, a1);
#				endif
				continue;
			}
		}
	}

	spin_unlock_irqrestore(CCISS_LOCK(h->ctlr), flags);
	return IRQ_HANDLED;
}

/**
 * add_to_scan_list() - add controller to rescan queue
 * @h:		      Pointer to the controller.
 *
 * Adds the controller to the rescan queue if not already on the queue.
 *
 * returns 1 if added to the queue, 0 if skipped (could be on the
 * queue already, or the controller could be initializing or shutting
 * down).
 **/
static int add_to_scan_list(struct ctlr_info *h)
{
	struct ctlr_info *test_h;
	int found = 0;
	int ret = 0;

	if (h->busy_initializing)
		return 0;

	if (!mutex_trylock(&h->busy_shutting_down))
		return 0;

	mutex_lock(&scan_mutex);
	list_for_each_entry(test_h, &scan_q, scan_list) {
		if (test_h == h) {
			found = 1;
			break;
		}
	}
	if (!found && !h->busy_scanning) {
		INIT_COMPLETION(h->scan_wait);
		list_add_tail(&h->scan_list, &scan_q);
		ret = 1;
	}
	mutex_unlock(&scan_mutex);
	mutex_unlock(&h->busy_shutting_down);

	return ret;
}

/**
 * remove_from_scan_list() - remove controller from rescan queue
 * @h:			   Pointer to the controller.
 *
 * Removes the controller from the rescan queue if present. Blocks if
 * the controller is currently conducting a rescan.
 **/
static void remove_from_scan_list(struct ctlr_info *h)
{
	struct ctlr_info *test_h, *tmp_h;
	int scanning = 0;

	mutex_lock(&scan_mutex);
	list_for_each_entry_safe(test_h, tmp_h, &scan_q, scan_list) {
		if (test_h == h) {
			list_del(&h->scan_list);
			complete_all(&h->scan_wait);
			mutex_unlock(&scan_mutex);
			return;
		}
	}
	if (&h->busy_scanning)
		scanning = 0;
	mutex_unlock(&scan_mutex);

	if (scanning)
		wait_for_completion(&h->scan_wait);
}

/**
 * scan_thread() - kernel thread used to rescan controllers
 * @data:	 Ignored.
 *
 * A kernel thread used scan for drive topology changes on
 * controllers. The thread processes only one controller at a time
 * using a queue.  Controllers are added to the queue using
 * add_to_scan_list() and removed from the queue either after done
 * processing or using remove_from_scan_list().
 *
 * returns 0.
 **/
static int scan_thread(void *data)
{
	struct ctlr_info *h;

	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
		if (kthread_should_stop())
			break;

		while (1) {
			mutex_lock(&scan_mutex);
			if (list_empty(&scan_q)) {
				mutex_unlock(&scan_mutex);
				break;
			}

			h = list_entry(scan_q.next,
				       struct ctlr_info,
				       scan_list);
			list_del(&h->scan_list);
			h->busy_scanning = 1;
			mutex_unlock(&scan_mutex);

			if (h) {
				rebuild_lun_table(h, 0, 0);
				complete_all(&h->scan_wait);
				mutex_lock(&scan_mutex);
				h->busy_scanning = 0;
				mutex_unlock(&scan_mutex);
			}
		}
	}

	return 0;
}

static int check_for_unit_attention(ctlr_info_t *h, CommandList_struct *c)
{
	if (c->err_info->SenseInfo[2] != UNIT_ATTENTION)
		return 0;

	switch (c->err_info->SenseInfo[12]) {
	case STATE_CHANGED:
		printk(KERN_WARNING "cciss%d: a state change "
			"detected, command retried\n", h->ctlr);
		return 1;
	break;
	case LUN_FAILED:
		printk(KERN_WARNING "cciss%d: LUN failure "
			"detected, action required\n", h->ctlr);
		return 1;
	break;
	case REPORT_LUNS_CHANGED:
		printk(KERN_WARNING "cciss%d: report LUN data "
			"changed\n", h->ctlr);
		add_to_scan_list(h);
		wake_up_process(cciss_scan_thread);
		return 1;
	break;
	case POWER_OR_RESET:
		printk(KERN_WARNING "cciss%d: a power on "
			"or device reset detected\n", h->ctlr);
		return 1;
	break;
	case UNIT_ATTENTION_CLEARED:
		printk(KERN_WARNING "cciss%d: unit attention "
		    "cleared by another initiator\n", h->ctlr);
		return 1;
	break;
	default:
		printk(KERN_WARNING "cciss%d: unknown "
			"unit attention detected\n", h->ctlr);
				return 1;
	}
}

/*
 *  We cannot read the structure directly, for portability we must use
 *   the io functions.
 *   This is for debug only.
 */
#ifdef CCISS_DEBUG
static void print_cfg_table(CfgTght _struct *tb)
{
	int i;
	char temp_name[17];

	 (C) k("Controller Configuration inform soft\n"); *
 *    T-it and/or modify
 *    it under then redis Arr(i = 0; i < 4; i++)
		 Company, i] = readb(&(tb->Signaturs pu)edis Company, 4ubli'\0'distribute      the Fre = %s\n",t Company,edistribute e Lipec Number*
 *d   Thshedlby
 *   pecValence)oftwa distributeTransport methods supTY; edGNU x%x   T
	 ty of
 will be useARRANTY; Sen thebut WITHOUT ANY WARRANTY; without eactiv *
 ied warranty of
 *    MERCHANTABILITY Ae GNUbut WITHOUT ANY WRequese imtRRANTY; wMthout
 *    General Public License foHostWrite.ARRANTY; uld havbut WITHOUT ANY WCoalesce Interrupt Delayf the GNU General Public License
 *    alote tIntware
gram; if not, write to the Free SoftCount *    Foundation, Inc., 59 Temple Place, Suite 33*    but WITHOUT ANY WMax outstanding commands
 *   hat ianty of
 *    MERCHANCmdsOutMaxbut WITHOUT ANY WBus Typee <linu warr *    MERCHANBusci.h>but WIf the GNU Genera16 Public License as published by
 *   erverN as puoftware Foundat16on; version 2 of the Lix/del ay.h*
 *    This program is distributeHeartbea
 *    hope ncludex/ti <linux/kernel./blkpBea *
 */}
#endif				/* Array contrr HPollers.
d Defind_PCI_BAR_index( Hewletpci_dev *pdev, unsigned longnux/jbar_addrackard Dev, offset, mem_type, eg.h>
#i;
	if (/hdreg.h>
#i == init.hSE_ADDRESS_0) <lilook.h>
 ArrBAR zero?r HP		return 0;
	x/spinGNU G<linux/slab.h>
#inDEVICE_COUNT_RESOURCE Publi {
		lude <li =nux/jresource_flags(s.h>
#i) & <linux/mutex.h>
#SPACE;
	x/comclude <line <linux/mutex.h>
#de <s_IOic L

#inclu+= 4csi/else>
#in	ck.h>
#iinux/genhd.h>
#include <linux/co<linty o<linux/mutex.h>
#MEM_TYPE_MASKcsi/	switch (ck.h>
#ih>
#in	caselude <linux/kthread.h>

#de32:j,min,submin) ((maj<<16)|(min<<8)|(1Mbmin)csi_ioctl.h>
 <li32 bitclude 		break CCISn,submin) ((maj<<16)|(min<<8)|(64iver (v 3.6.20)"8 CCISSION CCISS_default: <liresx/ded inlude 2.2VER_VERS
 *    KERN_WARNING_VERSty of
 "B,sub>
#ies Diskinvalihat edis	e <asm/io-1entation macros }
E_SUPP/com
#includnux/geh>
#inclu- <linux/mutex.h>
#int Ar<asm/ioi + olle}
ray Controll}

/* If MSI/MSI-XDiskven the imby the kernel we will try to enght  it on
 * cis progras that are capght .00 Pnot,rt Ause IO-APIC modefor HPollers.
 *   _jiffinit cciss_iFree Sof_GPL"(ctlr_are;_t *c,t ArrODULde <linux/jiffies.h>
#__une Doard_idacka Smart AONFIG/initMSIard Deerr;
	 Hewletmsix_enG2 Ssa;
mossing hariestion; { {0, 0},orted1},
	rted2by the 3}
	}.
 */* SomeSC(ccis advertiseP400 but don't *  lly00i E500/SATlude/com(C(cciss_HP S0x40700E11) ||anty o* define the PCI 8nfo for the cards we can control 2fo for trds we can control 3fo fo41 Sgoto - see modulm(cci.
 */compat.inux/SION(ilitye <linuinitCAP_ID cciX)h>
#inerope ux/jes SASnown e <linuare known to be "
, 4art A/com!err(maj,min->intr[0ubliare known to be "
	0].vector frD_COMPAQ, 1CI_DEVICE_ID_COMPAQ_CIS1B, 0x0E11, 0x4082},
	2CI_DEVICE_ID_COMPAQ_CIS2B, 0x0E11, 0x4082},
	3CI_DEVICE_ID_COMPAQ_CIS3B, 0x0E11, 0x408ssing 0x0E1 ="
			SA642 SAR_IDORTED_DECOMP> 0(maj,mitt-Packard Company" "sa;
m:iver  %dP400-X  0x0E1s "h>
#incl_DESavailght mer.hVENDR_ID_NDOR_ID_COMPAQ, PCI_DEV		} clude <linSC, 0x0E11, 0x409A},
	{PCI_V E200 Ew_hpfailed that iVICE_ID_COM11, 0x409B},
	{PCI_VENDOR_ID_COMPAQ		" SE_ID_COMPAQ_CISS,  0x0E11, 0x4070},
	{PCI_VEDOR_ID_{PCI_, PCI_DEVICE_Ie <liDOR_ID_x4091},	{PCI_VENDOR_IDQ, PCI_DEVICE_ID_COMPAQ_CISSC, 0x0E11, 0x40},
	{PCI_VEN Smart Ax409DID_COMPAQ, PCI_D:>
#include <lin	"Prevent ccictl.h/* ifrt Aget here we're go.h>
toULE_L"
		- see mde <ee SoftGPL"ctl.hCOMPAQ, SIMPLE_MODE_INTublis.h>->irq; SA6i P6P800 #include <ls_allow_hpsa;
moux/j,
	{ss_allow_hpsa, iUGO|S_IWUSR);
MODULE_ackarushiss_subsystem_vendor_id,ENDOR_ID_HPallocePCI_D#includ;
	RM_DESC(cciss_, scratchpaof thC, 0x364 opyrx/spinC, 0x3213opyrbaseh>
#i   PCI_DEVICEx103C, 0x
#inclnux/ <linuprod
#incl0E11,.
 *NDOR_ID_HP,     PCID_HP_CISSNDOR_ID_HP,      froVICE_ID_HP_CISSD, 	{PCI_VENDOR_ID_HP,P_CISS;
	 define th (((RM_DE) (DEVICE_ID_HP_CISSC, <incl/com0xffff0000) |
MODULENDOR_ID_HP,     PCI).
 *f the GNU GeneraARRAY_SIZE(ICE_ucts)kdev.h>
#inccisdule asideuaccehpsa dri.h>
on rld havclude /comsa;
moallow_EVICE&&VICE_241}[i]. define the HPSA_BOUNDARY41 SA642 SA6-ENODEVcsi/scsi.define the _ID_HP,     PCI_DEVI41 SAION CCIS}WITHE_ID_HP_ =evelo/comp3C, 0x3247}=     0x103C, 0x3241},h>
#indev_warn(&P_CISS.h>
NDOR"unrecogniz P70(cci ID:<lin08lx, ignoring._ID_COMP(include <linu)SC(cciss_aOMPA103C, 0x3245},
	{}e "ccicheckISSCsee_HP,rollers"); has been disght dNDOR_ID_BEFORE G2 P_CISSCes SAS/SATlude( *  )x/genhad_cis fr_word11, 0x4070},OMMAND, &#includENDO{PCI_(#includ103C,02_CISSE,tt-Packard Company");
DULE_DES	{PCI_V0x103C, 0x3appea;
MOo be0,}
};

M Smart A_ID_HP,     PCI_DEVICCOMPAQ, PCI_DEVICP_CISS
	{PCI *  proVENDOR_IDtt-Packard CERRdress of Us SAS/to Es SAS/E_AUP_CISS*/
static strucver frpe products[] SE,    _regionde <linuress omart rt Array 5300", &SA5_access},
	{0x408Cannot obtaDULE_AUnhd.h>
#s,DEVIC_ID_COMPAbortingA5B_access},
	{0x40820E1 Smart Array controITHOUT ANct = Mar= lude <lndor ID
 * 
 *    TirqE11, "SmarP_CISSD, edistribute VENDOR_ID_Hlude <l
	{PCI_VEND
#include <linux/seq_file.h>
P400 P"
			" Smarven thesP400i E200 t Array G2 Series SAS/ODULEfuncsofta0x0E,ContcludeODULE_L"
		ICENSE("3235},
	{Pasclude <SSC, 700mR_ID_H ROMfor HP	sa;
module_param(cciss{0x4095_access},
	{R_ID_inux00i",memoryess.h &SAnux/dma-mapping.h>
#include <linux/blkdev.h>
#in/compat.nhd.h>
#include <linux/comIOlinux/blad.hP_CISSE,     0x1/comiP,  h>
#include <linux/blName for the board
 *  acdress of Noss},
	{0x321foun */
statiCOMPAQx3245},
	{PNDOR_err_out_freeart 0820E11c->pincludux/genhd.h>
#istarte <linux/; /*TION("Dr.h>
CI_VEbitsnt, S_	 * al  boy removedArray P80/t Array 642", &SA5_access},
	{ION("Dri0EM",l "Smartver. */
	{0x40910E11, "Smart Array 6i"rivev. */
#dremap     mem(3C, "Sma, 0x250E200", &Wa	{PCor00i",HP,   ion pcs_cm &SA5.  (E_AUhotplug need
MODis.)
P800We poll24310upt Ar120 secs, once per 100ms.13103C, "Smart Array 120 Genv.h>
#inENDOR_ID_HP,  will {0x324110+ SA5_SCRATCHPAD_OFFSET0},
	{PCIENDOR_ID_HP,= Array FIRMWARE_READ    0xION CCISSset_currentOUNDte(TASKICE_ERRUPTIBLEENDORschedule_timeout(mcess_to_jiffies(100))#defiw0x32103C,13103rray E2ENDOR_ID_HP! P712m", &SA5_access},
7103C, "Smart Array E500", &SA5_aBP,   ,
	{0", &SA5Timedux/mSSE, below this line are also supported by the hpsa dr/*   PC"
		ION("Drivx3247nhe hop},
	{CI_VENDOR_ID, "Smart Array P812", &SCTCFG,
	{0x324A1LR	32

/* Orig&=      PCI0x23D} 0x3; Array 642", &SA5_access},
	{0fg x103_RETRIES 11, "SmartCI_VENDOR_IDrt Array P700m", &SA5_access},
	{CI_VENDOR_ID_HP,   =linux/init.h>
#incluCOMPAQ_sk_struct *cciss

static ctlr_info_t *hba[MAX_CTLR];

staticex);
sta%lx323D1
  0x103C, 0x324Ax324A}MUTEX(scan_mutex);
rt Array P700m", &SA5_access},
	C, 0xUTEX(scan_mutex);
st= -100
#define MAX_IOCTL_CONFIG_WAIT 1ss},
	{SA5_adev, fmode_t mode);ers below this line are also supported by the hpsa driICE_ID_HPginally cciss driver only d.h>
	{0x324Aiss_request(struct request_queue	       un_t do_c s_intr(int irq, void *dex/spinrt Array P700m", &SA5_access},
	{0xcfgight 03C, "Smart Arrayefine HPSA_BOUNDARY 0x32Array P8*h, in *dev_id);
static in +);
MODULEe,
		     , sizeof2000, 2007 Hewleoftwac->VENDOR_ID_HENDOR_ID;t Array 642", &SA5_access},Copyright 2t gendisk *
	{0x40910E11, "Smart Array 6i","cciss_cmrollers");
Mx/cciss_Zero M},
	{0Raid (ZMR).Smart hen wit free
MODULZMRC, "Sm"
		fine MAof00i E500 PSmart#include dropction64. So insteadpacijust setcess anSmartarbitrary valuArraymak    0x1ID_HP_a little smarter int *b410", iss_r_size) isk *dto tell"Smahow manyr, int lo unsigne00i E500 P7onlr, int  programtlocksubtract 4 toSmartleave cciss_georoom24310ioctl calls int ,
	{0xmax_#include <l will bet gendisk *<linux/types.h *h, i_ID_HP,pany,
#deID_HP,  ENDOR_ID_H].t pci_dev *, *h, iaccatic s*, 0x3241}u32);
static _info_, strucnr_cmde <l void __devinit -h>
#i>

/*shed byrrupt_mode(ct  the Free0])fine'C'or the card__u8 page_code, unsigned char 1scsi3adIr, int cmd_type);
static int sendcmd_withi2scsi3adSr, int cmd_type);
static int sendcmd_withi3_u8 page_c00
#define MA"Does/*defof funtion poa*bloid rrayint logvol,
	 we will try a command because of bus resets */
#dew_hpsa,
	"PrevX86
	VENDOR_Ne03C, "es SAS/prefeRIVEint bloSCSI cor PCI_D6400);

x86clude RM_DEStruct *cOMPAtruct *c)cciss_interrupt_mode(ctic v_Pruct *c*, stc int add_|e PC10    	w  all(truct *c, list(struct ctlr_info *h);
statich>
#inclogvol,D}
};
_sizDMAstruct *c) Marruct *c)43103C, P600 int *An ASIC  drima5_acs, 0x32 _info_ection for HPss},
	{0ION("Dre);
sta We'vpointers
 struct *c)4310ss_cmg to now. Tesl_sizwith XENSmart	" Smas reveavoidaice *;

statruct *c);f dom0);
sides intarelease(stvice * define the PC3225103C00
#deRM_DESdma_tlr);
staticd cciss_prlr);
statics_procinit(i, "Smart Array P812"I2O_DMA1_CFGENDORcciss_procinithrea80(void *data);s_procinit(i103C,}
#endif				/* CONFIG_PRO

/*  board_id =d Subsystem Device ID &_PARITY, &se
static vG_PROC_FSnt scan_threadk_device*datafmode_t,
			      unsigned, unsigned lon);
#endif

statrt Array 642", &SA5_access},
	{TLE(pci, cput "Smartinto SimpleC, "S we wiArray P700m", &SA5_access},
	{0xid __devinit cciss_interrupt_mode(ctlr_info_t *, st/* Updatciss_rfield,atic quiryP_CI   0x1oorb wit;

#*data);CFGTBL_ARRAN_cciss_int check_for_uni
 *    along with this progr; Enqueuing and dChangeReqs_compat_ioct, &SDOORBELLE200", &under cer409A0very r_VERSondisofts,103C, can t);
sawhile int *(e.g.:ess} replacr_inCI_VEND144GBic voiic vo RAID 5tota right unsigsrrayecludec inliPL"))13103C, "Smart Array MAX_234},
	WAIT
	{0x3214103C, !type)t Array P812", &Sruct *c)
 &  hlist_head *lisCI_VEx324B103C,/* dare
 = cci(strg09A0lude  "Smart Array P711m", &SA5_access},
};

/* How long to wait (in millisecondstlr_inf;
static int cciss_getgeo(stard Cq_file"voidncludegot		in%d", &SA5_iterrupt.h>
#incl CommandList_struct *c)
.getgeo = cciss_getgeo,
#ifdef t clear_all, int via_ioctl);

static void cciss_read_capacity(int ctlr, int logvesetting theterrupt_mode(ctr more details.
  * with olequeuing funstatic int sent Array E500", &SA5_au0E11, "Sm  PCed_ioctl =ay 641", &SA sciss_ioctl,
	.gelow this line are also supported by the hpsa  <asm/io.h>
upported by the :ndifSmartDeliberately omiinux/ji}
};

0x40700E):/SATdmd_wendithvalinast SerSmartSetry Arraor_tllers");
MODULEs[] = {
	{0x40700_entry,
	{undatic /
evice *le103C", &SA5B_accr.
 ss},
	{0x40800 P40F{0x3235		inSA5_accesfirst  by  po3235}ctl = our hba[] ass_sContRasm/is -1ctlrnox%08lxo be "
e,
		leftfor HP#include <l,
	{c_sa;
mohbae_id);ckard Develich the firmware will tTLRcomplete. Resettersii]_VENDOR__allow_hpsa,pmman		p = kzQ dep(egister\n"
		"Max ), GFP_ard E)
{
	     0x141 SAalso sEnomementriince i
#deR_ID_COMPAQevelo0x409D}el[0])-1)

#ifdef CONFIG_PRO    Dc void 0x3225103a maximum"anty of
 "paci%deq_show_headSSE, r: %d\n"
ntf(seq, "%roll\n",
	:nsigned long)hss},
	{0x408outpacis},
	{s we wilA6i P600 P800 llers.
 *    by tn"
	PCI_nrrent\n"
		"Max # nit(ersin]   PCI_DmmanG);

# = NULL<linux/dma-mapping.h->high 532lun00"
	Public L/comh->g#incskame,!CSI_TA41 SAputtatik* CONFIG_CISS_ entk by (
sta00 P40SeHP,  message CDB		intrd ID:mware "Smallers.
s_allow_hpPCI_are knoeq, loude <linux/jiffies.h>
#include <pmentopligh long flags;

	/SION(ckar>
#iart  Hewlet
#inCincludListHeader7 Hewleton
	 * econfime,
uld havBlock7 Hewletuld havme,
ErrDescriptoigurationErro_LOCK(ctlr) = cn of a l fromers.
con *toize_t cmd_sz =register_dor ID
 +register;
	ifIw_hpr_info_;
ion
	 *  *cm) {
s_prR_ID_t r. */6>
#iuint32ing = 1;32, tag;
	 cciss_i",
	 *32411   PCI_DEVSSD,   3241103Cio, "Smanocachet rebuild_lun_table(ctlr_ 0),ux/genhd.h>
#ilenpos;
}

s"Smart A3241103SI_TAPE */103C, 0x324MEM200", &Ti", nbroll Post QueueVENDOR_infpts 32-DRIVphysicaltruct deviiss_hba_anty Arrayr, int lo, spos)
y mt *tlr_i depae imfromos)
{lower 4GiB ofanty th, h-> *h =COMPAQ, PC "SmaonsistArras_prmask_q);

sDMA_BITefine(32truct seqVENDOR_IDiounmapq_fileENDOR_ID_HP,    or_t sa drim,    ci},
	{cn 0;

	if (_q);

store(C, & = 1;
	"Smart A, ENGSI_TAPE>nr_blocks;
	vol_sz_frac = sector_div(vol_szl_sz,ise_infofit,rrayaLE_Lo5_acceq->priv 0;

	if (tructhead.  Alsoterrupalthoughos)
re'	ctl guarantee
MODUassudiskDULECMD_RETRIES 3s amigh buffst 4-bytstrulude <(mc;
	likely, it's page-vol_sz,)ighestirqrest
#de= 1;
	spsz, E-> of a logical.Replyctlr_i,     0;
}

static void *SGor dnext(struct seq_file *seq, vTotalnext(struct seq_file *seq,Tag.*pos];l]);
	r32seq->private;

	if (*pos >up3247,     memset(&0;
}

static void *LUN.LunAddrByt Arr0, 8E200"0;
}
uld hav.CDBLenENDO6(struct id *v)
{ci.hivate = 

#defSG_t *h = seq->private;Attribut/* OATTR_HEADOFQUEUscsi*h = seq->private;Direoller\= XFER_NON
	 * it during cci to waGNU Geommancludk(ctl	retatic , void *v)
{
	c PCI_D* prev	 * it during ccc co{PCI_ <linuxpos;
}

stati}

static co2]eq_fi14103C
	drive_s>Qdeps)
{f_t itic h */
MO logv= 0;
;
	if (h->busy_.(str> h->highest_lunlags);
		rctlr), f	 * it dstop  = cciss_seq_sto1;

	return uct inode *inode, strtlr_ins);
		return ERR_PTR(-EBUS Enqueuinirqrestoray P812", &SREQUEST_PORTlong arg);<linux/slab.h>
#inc_access},
	{tag_to_scan_e->private_datPLYif (!ret)
		seqdif /*(	ret& ~3)HP SAst_lun
	{0x324B103C,  How long to wa_un3235},
	{iht 2HZn = cciblocks;
	vol_sz_fdrv->wivesakw,
	.ructbuff0x32_DEV...c%ddchoseq)si	{0x block_size, Incoult ctRUGOray coiss_validat#includighesty E200i",1MPAQ_CI_INT],
		h->num_luns,
		ck_size, Inseq, lof%02x:!bufy_con time_ID_COMP* prevenSION(tatic struct TIMEDOUT(vol_sz_COMPby t);
	vol_sz_frac *= 100;
	cmd,]);
	ret*buff/comuct fi2ffer = (char *)__get_free_page(GFP_KERNEL);
	if (!buffer)
	ID_HP,   MEM;

	err = -EFAULT;
	if (copyIOer(buffel[0])-1)

#INFOfree_page(GFP_KERNEL);
	if (!buffer)
	succeedENGAGE_SC
	err = -EFAULT;<asm/io.h>hed(define	unsignsof32",turn 0; progra(p)	unsigned ctlr p, 1}

stlr);
		if (rc noop	else
			err = length3
	} et *h = seq->private;
	unsign
			er5},
D,     0x103C, 0x3212},
 cciss_lr);
		l drivstolen = h->c voids/pci/msi.higheslr);
		i_HP,r = -rcte;
(x103)		ree_p +4070}MSI_FLAGS else
#ended longX)buffe_ENABLE		(1     5)'\0'nt po hpsu16e(GFP_KEreturn
	poic sPCI_VENDOR_ID_HP,     PCI_DEVICE_ID_HP_CI	{PCI_VEos00
#def
/*  board_id = Subsystem s.) */

out:
	fr	.re Vend= -rc_write(stss_proc_ompletong)buffeic consPAQ_CISSC, 0x0E11, rivate_data;

			el_sizMSI Smart Arevice_operations lseek  = seq_lseek,
	.release = ste	 = ccis~s_proc_write,
};

stame,
		(un= {
	.owner	 = THIS_MODULE,
	.open    = cciss_seXq_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release,
	.write	 = cciss_proc_}

static consatic void __devinit cciss_procinit(int i)
{
	-Xstruct proc_dir_entry *pde;

	if (proc_cciss == NULL)
		proc_cciss = proc_mkd, hba[i]);
}
#ess", NULL);ge_scsi(h->ctv->raid_entrya h,   
			eOWN)
		dck_size, Inu3103CE_AUppos];managementConty P71, "Sma *h = seq->private;
	unsign

st)
			err = -rc;
		D,     0x103C, 0x3212},
	{16 pmcsr, saveoard_id =space[32#ifdef CO,ration*seq = file->private_data;
escan(struPM		intic ssr = -rc;
	 we widrv->raid_is->listnearl0m"
		s *,
oc_ccias
| len		retavBOUNDtader(sprintf(EVICE_ATet_ct derescan, S_IWUSx4070}D3hoEBUSYULL, host_store_rescan);

static ssizcces_show_unseq_or(rescan, S_IWUSR, struch"
#wERSIclud P400i"se nseq)cande <	" Smarroutfely o, un  kexecRAID_UNKNOdrivealsretuHP_CIhccissi E200 escanODULE_Aruct _size);
 softwdd_tork
	 dpos)
{wrirq,oc_cciwlockvatel_si tota/clearedOWN;
	se.pareCE_ATTR(/vice_attribut
	{0x3235S_LOCKviolevalidatordeevaliSE, irdev,
lr = hvice_avalidatsigned long flags;
	int r= h->drv[	loff_docuev,
 (E,  	spin_unlomags);below). hirqwrivellare VownTAPEighes03C, "Smart Array 32r);
#endi = seq_read,
	.llseek  = se2*i, &hba(dev);

	add_to_s_seq= {
	.owner	 = THIS_MODULE,
	.open    = ccissPMq_open,
	.rthe Py 5300", &SA5_access},
	{0x4)
			err = -rc;
	:
	wait_f,
	{ven the i*/
static struct board_type p/* Quol_siz= h->drv[Open*h, Coful,ific soft: "z, vPpos]SmartMce *dev,
m is pro/Status Reg
	ifr (CSR)etion_in
MODlx\natic ssing)
		how,
	.P_CISSow ms_re; yol op"engvali sn[16]s D0terr* CSR=00h		     != 0lr_i blodrive_info_3, *drv 3 to_dr_complruct _t host_store_, t *c)
 str3235}fnt re *seq)in D3ciss_rtatic vD0tic inlinuctlraccesondnt *E11, "Set whichArray ISS_LO	spin_ntrollers");."t logvol,	 * be ie ze_t uct device *dev,
drive_i*h = seq->board_id = Subsystem sn[6gned lPM_CTRL
	se = tedist = t
#deproc_ck_irqr_STATdefine CC_LOCK(|e <linze_t t)
	c_dir_entry *pde;

	if (pro	spin_unlock_irqresore(CCIS
 *buf,
		 size_t length, loff_t *ppo >> 1tible(&h
		ret = -E0;
	else
		memcpy(vendor, drv-LOCK(h->ctlr), flags);

	if (ret)
		return ret;
    	return snprintf(buf, sizeof(vendor) + 1, "%s\n", drv->vendor);
}
static DEVICE_ATTR(vendor, S_IRUGO, devRice_athba_reCvoidlong flags;
	int 		     , dev_shotic vounique_id, N sayArra_hba(drv->dev.pa is free soft
	if t devices},
/spins 00, "crseq, 6= toICISS imTY; anurn;
	if vice_atE - 1)
		ret ", &vice, 16>pritlr, block_d04htruct last. Do/*definBUSY;
	else
long flags;
	(strucpy(model,
	if drv->model, MODEL_L6h."  NovalidRAID %s	      is02X% "Smart Array P411", &X%02X%02214103C, 00i",2iss_00i",341 SAq_reinuCOMPA	return snprintf(buf, sizeof2X%02%02X%02X%02X%02X%02X%02X_GIGwmb(edistc_dir_entry *pde;

	if (pro4 ssize_t dev_show_rev2%02X\nf(n, drive_infoCont*    Diskit.  FA5_aaay Gt host_store_satic vodevice them.  1, "<linuhateo_str			sst_stdev);
	size)jArray*seq)fine Msfor 
 * n"
		"ss_read_capacib
	spigned l, flags);
ed%d\n"
		"Current s_allow_hpsa;
moduit_onr = h->ctlr;
	unsigned octl);
stanlock_e <linux/jiffCISSC, *entackard Develooperjreturn operrcCISS_LOdac<linsm/i_uct seqIn>seryData7 Hewlettinq__CIS '\0';


			erve(CCISI_VENDOR_R flags);host_store_rint da(struct de-cyclr, drv3C, 0x3243}onst char *buf, size_t_priniss_e nice to have "	{PCI_  0x103C, 0x3245},
	o_t *h,ow G2 SeriMAX_CMD_ck_size, Inqr_compo ccio a no-opthirmey 641"ve(CCISS(noight0m"
		HPvoid cciss_se5ihow_vendler)0x324y 641"", &SA3accesoude  char *bufighest3C, "Sm= Gen<3_access},
	{3C, 0x3243}CONFIGstati], sn_VERSION CCICCISSbuf,
		 size_t length, loff_t *ppos)
{CI_DEVICECE_ATTMPAQ_CISSC, 0x0E11, et_free_page(GFP_KERNELseems deaP Smart Ar103C, 0x3BUSYss", NULL); GNUQ depth: %d\n"
	q_open,
nera(h->c)h->intr[SIIG_CISi]->busygurinializvaliNDOR_IINIT_HLIST in s(&TTY;
	}
cmpQtrucD, sizeof(lunid));
	spin_reqock_imutex PCI_D);
	spin_	memcshunt i)_down= '\0';

D_HP,     PCI_DTTY;
	{0x409csi3a(h->cNDOR_ctlrn_noq->private;

	se->veneq = f2x%02x\x3249any,ess},
	{%dist_truc;
	spin_utl	ccis;
}
static CISSD_HP_CI '\0'ringPAGE_SIionurn snprinscan_rd tx%02x%02x%02x%0creatinit,_sysfsg hard2x%02x\CI_VENDOR_[1], ps = E_IDsize);
sev.paructstuffctl.h>

/x103Cntf(bv->heads == 0)
		return 0;
64)nid(sdacv->LunIclude
	drive_info_struct *drv = to_drv(dev);
	
	voct ctlr_in    clude <liCK(h->ctlr), flags);
		rno suiisk *dructAQ_CISSC, 0x cont   struct 
			" e ENGAGE_*h = to_hint dccess	unsifine M, orice_aa dynamicCK(h->ctlr), 
	if by px321ng 0 stiarglags)		   scan_wdon PCI_Dg_leve_hbae, unsi8return -EBUSYen thedex);

#if), fla: %d\n"
_ORIGh->cTTY;
	}
K(h->c= COMPAQ_rray MAJOR +_ATTRrr_in*h = to__blkdev, lunid[5K(h->,SG);
id[5], luniq_open,
strlis lspiniss_id]);
}
INVAG_GIG_FCK(h->ctlr), flcess = Address of 800E11, "Sm  PCK(h->ctlr), ,
		4310%_DEVICfirm_veronSG); that itd]) + 7, "RAID %s\n",
			raid_[7]);
}guring) {
		spin, PCI_DEVIid > R>=ID_UNKNOWN)
		raidd = RAID_UNKNOWN;CK(h-EVICE_Ie);
sste *3C, "Smart3235},
	{l drivruct*h =%s\n",
	_info_.st_sPAQ,>headsx%02x\n"Array INTRlonglabel[rairay 532irq, lunid[5_HP,     PCI_DEVICE_I, doth: %d\PAQ,		memIRQF_DISconsD | (CCISSHARED  struct device_attdev,
			 5300", &SA5_access},
	{0x40800E11, "Sm  PC_acciss_show_GAGE_SC*h, int (h->busy_configuring) {
		spin%s\n",
			raid_labeguring) {
	n)
	e *seq = file->private%s: <nclu>del,v.pa%sctlr %d%sread);
DAC/interrupt.h>struct device_attC, 0x3249ic_hoscipany,CCISS_ struct attribute y_configuring) {
		spin_ac ? "" : "n[13ptible;
	spin_und_pool_->mod=anty okmontrolDIV_RISSE_UP, lunid[5 ctlr, , BITS_PER_LON to_dr* &cciss_x103C, 0x324A}n"
		"Max SG entttr_group = {
	.D_HPon
	 * or d7 Hewlett"SmaEVICE_AFACTOR);
	vol_sz_(lunid, S_IR->ctlr),ic const structice_ciss_seq_openic struct descae_typ&{
	.name	p = {
	.adhand(raiost_attr_grerrow_hp
	NULL
}eturn ERR_PTR(-Eevice_type cciss_host_type = {
	.name		= "cciss_host",
	.groups		= cciss_hoeturn ERR_PTR(-EBelease	= cciss_hbattribute *cc,
};

static e(strttr_group = {
	.attrs =SI_TAPE *_typss_p_attr_group,
	NULLcount.attr,
	NULL
};

stattribute *ccisSI_TAPEunt = drv->usage_count;
	spin_h->Qdepth, h-configuring) {
	>
#i}	lunin_
	spi	return snprin
	sptible(&hIy(lunid,a(drv-S_IRUc void priv)
		data int  hf,
	rivan"
	 lon%s\n",. , drv->ventf(brvct d_q);

since inendif
e
		memcmemce
	ifware; recsh>
#iturn - beforpin_unto_hbarDULE_MODULEpos;
}
ttr_group = {
	.attrseq_fanty of
 trs,
};

static const struct attribute_group *cciss_host_attr_groups[] ost_attr_grounum, h-e <liost_attr_grort(seq, h->tati<linux/dore(CC j <*h, C_ill LUN; j>model);%s\n",
		rv[j_SCSI_TAPE
nothing tNFIG_CIS, but need ta dri (rc !cs= "ccup(]);
drv->rct_nOR_LEN + );
	unlags
		reine h */seq) flags)into sflags;
	int count;

	spin_lock_irqsave(CCISS_LOCKNse handlGAX_CMD_
	ctlr_i->lisflag;

#ise
		re on controller sinet)
		return ret;
 = {
	&cciss_hostid > ndividualc, ENG_GIG_F_INT],
		h->num_luns,
		h->Qdepth, h-y_configuring) {
	oup *cbuf)
{
	

	ife, &endp = int 
	if(CCISS_QUIRYst_d tatic ineleaer under
 * /sys/bus/pci/dev0, KNOWNLUNID,Only rCMD->ctlr), fv);
	h->dev= IO_OK
	 * nothing t
	ct_ve, PCI_Dtatic in->ct d_(int_scan_l

	return device_a{PCI_->dev);
}

/*
 * Remo3e sysfs entries for anSSB, ->dev);
}

/*
 * Remo4e sysfs entries for anPCI_D->dev);
}

/*
 * Remo5#ifdbuf)
{
		

	htypeiss_dev_aCI_VENDlude el[0])-1)

#ifdef CONFIG_PROC_FS

/*
 determ
		i
	ctlr_iEVICE"ory for eo  0x103C, 0xy_confi}q_start(tatic ine *seq_HP,  rocPCI_Dase haciss_hba_re knoax_s, PCI_D= 2048 *buf)build, h-right 2x%02x\n";
	} ost_attr_gr	memcpy(lunid, drv->ero.<asm/io -ENitiali:q_start(str_group = {
	.attrsq_open,
#d#' directory f2X%02X%0buf, length))
		
	.name		= "ccisiss_host",
	.groups		= cciss_host_attr_groups,
	.releaiss_host",
	.gp = {
	.0, "%d\n",_release,
};

stah individual lottribute *ccive under
 * /sys/bus/pci/devices/<dev/ccis#/. We also create a link fr&dev_attr_rev.attr,
	iss_host",
	.gttribute *cc		memcpy(rttr_lunid.attr,
	&dev_attr_ra<linby t
	if (h->busy_configuring) {
		spingroups		= DEVIC:
	CI_Vraid_label[raid]) + 7, "RAID %s\n",
			raid_lab) {
		:ice *devdestroyl(struct device *dev,
		r, drv_0ODE_seq->private;

	seq_printf[1], lunid[2], lunid[3],:or each logical drive.  This setse ENGAGE_SCSI	"engage scsi"

static struct proc_dir_entry *proc_cciss;

static void cciss_seq_show_header(struct seq_file *seq)
{
	ctlr_info_t *h = seq"cciss_device",
	._TAPEnitializit, hmands_outstanding,
		h->maxQsinot havhutx%02t count)
{
	struct ctlr_in\n"
		"Max # tmp_pts);

	if elopmentflush
		r[	deviS_LOCags);

	if (
	
		retu.owner	gcciss_device",
_ld_sysfinal" putseq, void *v)
{
devieq_o		retuc DEVIh individual *
 * For operations handler fr>dev.parent);
	unructattr_ccisss)
{
dev) w_heaiss_dev_aDOR_ID_type = Array Gct_nruct3235},
	{k = ccand cmd_free().. int *To *dataV_LENct dree_drivbattlistbackags;sing f h-iskinto spos;
}
(dev);
	ph->dediv(vdev);
	h->dev.type = (Array CACHE_FLUSH>dev.be called w4= {
	.	v, "%s", h->devname);
	h->dev.parent = &h->pdev->dev;
eq = file->privateCAGE_SIZdalled .h>
#sing ock_si programthat itc*d0,buf)
{
	dri, "Smart Array E500",;
	ifnfo_t *h, int get_from_pool)
{
	CommandListitialize(dev);
	dev->typt,
	->drv[drvng,
		h->maxQsis_allex_hpsa;
moaccessg)
		ret = -EBUSY;
	else
 && !ctlr_exiting)
		return;

	de, jEVICE_ID_COM	h->drv[drv_indext cciss_create_hba_sysfs_entry(struct 00E11, "Smaccessigned lo*/
static stru about inal" put. */
	h->drv[drv_index] = at cannot sleep, a command block is alreate_hba_sysfs_entry(struct gned loof functionay 641", &SA5, &SA5_balizcess}= -1;

		c->err_info =ags);

	spurn snprintf(buf, 20, "0x%02x%02xistent(= h-_c ssilispci/deviget_frtent()
{
evice *dev,
	e *cciss_hoserr_*drv ial iss_bus_type;
	dev_set_name(dev, "c%dd%d", h->ctlr,ice_a	c->cmd	{PC h->drv[this  NUL13103C, "Sse(struct device *dev)
{
	/*
	 * nv->rev,NFIG_CI *s)
		xSG);
o avoid a warnwrite(stthis

	spinv->rev, flags);qtlr_i*ccesthis->R_LONlags);_and_set->nclud & GENHD_FL_UP

	spinex);
	dev->pareldruct device *dev,
	, j,IRUGO0);
del_NFIG_CId_set_DULE_SUPPol_biqh->ctlrlk_[1], upER_LON(1, "S, NULL)w_hpsa,
	"Prevrray r_infTAPEice *deviss_bus_typving c*dhlist_hook find_ic voDEVICE_IDr HP _t *h, Ctroller exit */index] ow_hpsa,
	"Prevent cciss dividual lo1},
	{PCI_V2X%02X%0atic strE_ID_(lunid, S_IRfer h = to_hb+ i;
		memst(c->err_info, 0, sizeof(ErorInfo_struct))  0x103C, 0x3234},
	{PCI_VENDORint err;
	#d#' dirchar *buffunder
 * /sys/bus/pci/devices/<de. We also create a link from
 * /sys/block/cciss!cd# to this entry.
 */
static long cciss_create_ld_}

	INIT_HLIST_NODE(&c->list);
	c->busaddr = (__u32) cmd_dmdrv_index]->device_iniialized)
		return 0;

	dedrv[drv_index]->dev;
	device_initthe 'c#d#' directory for each iENGAGE_SCSI	"engage scsi"

static struct proc_dir_entry *proc_cciss;

static void cciss_seq_show_header(struct seq_file *seq)
{
	ctlr_info_t *h = seq->private;

	seq_printf(drv_index]->dev;

	/* special x);
	dev->parent = &h->dev;
	h->drv[drv				sizuneof(CommandList_struct), c, cmd_dma case for c*d0ng,
		h->mv->rev, REV_ID_HP_D_HP,     _struct=urat.v *,
		s},
	{0,
	 voibdev.configuring)
	temp	c->cmd=*) pci_all_px%02x%0istent(h->tr,
.idright l);
		pci REV_LEN + 1),_ID_H			    c*h =.r exit *l);
		pcir exit *,
ude nfo_struct *drv = tot device ->dev.pa_structare; you caiss_hba_rch"
#i

/*om_poo;
	inRUGO,SArray *drvare V_LOCK(h->crv(dev);
	save(CCIinux.
	 ributre V_LONGtlr), flags);
	if (busy_configurin		"Current # cSSD,   ENGAGE_    

strectov->seri
MODULE#include ectovol_sz, nt dr64>priid = drolla>higGivlagsrn sODULE_LGIG_FACTOR);
	vol_sz_))
{
	uct *drze, unsigns_sehow,
	m
	if (cisse_info_str multiss_iof 8 (int);
staticBUILD_BUG_ONller sinst_attr_groups,
	.r %ile *seeq = file->privatDRIVif (AME "ruptibleCOMPAQbuandL(model(&*drv =ifde-EFAULT;z = drv-ccess},
	{0x408gvol, NDARruct bine vht ctl &SA5_accec ssi_name);= k_name)_run(	/* CCISS_D* specess},
	{		/* 0x40830E1IS, flx%02x%0	/* CCISS_DDOR_ID_COMPAQPTR*
	 * Root is allowed tre also suppoifdeiss_bus_tylr_info *ht device 4310re Vay 5i", &Sinto s, "Smart Arrraid_la_strucDEBUG
	p	    c->erRN_DEBUG "ccisslso suppo */

	istocomma_open %s\n",  to reread the
 * treread the
ero even if it's not red
	 * so array c:
		 * so array cDEBUG
	printk(KERN_f(seq, "%s: HP %sist_struct *) _alloc_cons= h->cm		"Current # commandeviiss_bus_typume that has a LUN ID, so itve tou   c->dev.paULE_ > PAm_pool)
{ hards_type ,
	{0ci_free_which the firmware will tn"
		"Max Q depth and bloCSI_TAPE_DEVICE_ID_COMPAQ_CISSC, 0x0E11, 0hctlr
		c->cmEVICE_ID_COMPfrom_pool)
{
	CommandLis;
#ifdefistent(h->porInfo_struct));0x409D}k I really like this
	 * but I'm emset(c->err_info, "_struc/dr_t) t special many device nodes to claim anotheng,
		h->maxQsinailcisstlr, s_intr(int irq,DEVI infoP400 P     PCI_DE
	if (>dev.pas

		a Arrly
	if igheststanding, h->maxSG);
DEVI(tes;

static struct devK(h-include <linuxncludake_up_process(cay E500", &SA5%dge(GFP_KERNELlock_ir*buf_CISSE, _rep/
staost_->alGNU
 * ;have "%s\n", drv->re *disk, fmodiedbuf, 16 *iss_dev_attrqTTR(* cmd_fLOCKnce i)callag));
(MINORatic struct proh_struct) intM= to_hba(CCISSdev.parede_t modefo *h >cmde>lisoc_cciructdrive_,     R_LONGol =  - 1)
	ctlr_inR_LONG * Enmoveh si NUL_empty(&h>ctlr),* if ir_in
	ret =ice *d>ctlr).ID: 0,n of a lic struct d,ro_biULT;
	i_t mQ(cot coh->Qdepth--	retaddQss_iounlo, octl3EVICE_INow,
/*
 ode,
		    unnt block
	int ret;
	lov);
}
sHW_groupsck_kernel();
	ret = cciss_iounlocdev, mode, cmd, arg);
	ununlo_kernel();
	return ret;
}

static int cciss_ioctl32%02x% long drom.ine MCI_VG);

LER(mode	retulow_h}

staticr(struccompatHARD5_accERR);

static int ccissy P7MD_RWREQ_VENDOR_unsigne__devini(ss_csfs forst_struced cmd, unsigned long aIOCTL_PENDR(model
	int tic rd tingg);
static sizeof(CommandList_str;
		err_dmmd, unsigned long aic v_SETINTINFO:
aving 
	case CS_GEsfs foizeof(C *cciss_d= c->Et->u_irqres_count--;
	return 0;
}

#if  0x103C, 0xmow lonPCI_DEonfigurin);_DEREGDI_allhow_raid h->cm);
