/*
 *  libata-core.c - helper library for ATA
 *
 *  Maintained by:  Jeff Garzik <jgarzik@pobox.com>
 *    		    Please ALWAYS copy linux-ide@vger.kernel.org
 *		    on emails.
 *
 *  Copyright 2003-2004 Red Hat, Inc.  All rights reserved.
 *  Copyright 2003-2004 Jeff Garzik
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 *  libata documentation is available via 'make {ps|pdf}docs',
 *  as Documentation/DocBook/libata.*
 *
 *  Hardware documentation available from http://www.t13.org/ and
 *  http://www.sata-io.org/
 *
 *  Standards documents from:
 *	http://www.t13.org (ATA standards, PCI DMA IDE spec)
 *	http://www.t10.org (SCSI MMC - for ATAPI MMC)
 *	http://www.sata-io.org (SATA)
 *	http://www.compactflash.org (CF)
 *	http://www.qic.org (QIC157 - Tape and DSC)
 *	http://www.ce-ata.org (CE-ATA: not supported)
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/suspend.h>
#include <linux/workqueue.h>
#include <linux/scatterlist.h>
#include <linux/io.h>
#include <linux/async.h>
#include <linux/log2.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_host.h>
#include <linux/libata.h>
#include <asm/byteorder.h>
#include <linux/cdrom.h>

#include "libata.h"


/* debounce timing parameters in msecs { interval, duration, timeout } */
const unsigned long sata_deb_timing_normal[]		= {   5,  100, 2000 };
const unsigned long sata_deb_timing_hotplug[]		= {  25,  500, 2000 };
const unsigned long sata_deb_timing_long[]		= { 100, 2000, 5000 };

const struct ata_port_operations ata_base_port_ops = {
	.prereset		= ata_std_prereset,
	.postreset		= ata_std_postreset,
	.error_handler		= ata_std_error_handler,
};

const struct ata_port_operations sata_port_ops = {
	.inherits		= &ata_base_port_ops,

	.qc_defer		= ata_std_qc_defer,
	.hardreset		= sata_std_hardreset,
};

static unsigned int ata_dev_init_params(struct ata_device *dev,
					u16 heads, u16 sectors);
static unsigned int ata_dev_set_xfermode(struct ata_device *dev);
static unsigned int ata_dev_set_feature(struct ata_device *dev,
					u8 enable, u8 feature);
static void ata_dev_xfermask(struct ata_device *dev);
static unsigned long ata_dev_blacklisted(const struct ata_device *dev);

unsigned int ata_print_id = 1;
static struct workqueue_struct *ata_wq;

struct workqueue_struct *ata_aux_wq;

struct ata_force_param {
	const char	*name;
	unsigned int	cbl;
	int		spd_limit;
	unsigned long	xfer_mask;
	unsigned int	horkage_on;
	unsigned int	horkage_off;
	unsigned int	lflags;
};

struct ata_force_ent {
	int			port;
	int			device;
	struct ata_force_param	param;
};

static struct ata_force_ent *ata_force_tbl;
static int ata_force_tbl_size;

static char ata_force_param_buf[PAGE_SIZE] __initdata;
/* param_buf is thrown away after initialization, disallow read */
module_param_string(force, ata_force_param_buf, sizeof(ata_force_param_buf), 0);
MODULE_PARM_DESC(force, "Force ATA configurations including cable type, link speed and transfer mode (see Documentation/kernel-parameters.txt for details)");

static int atapi_enabled = 1;
module_param(atapi_enabled, int, 0444);
MODULE_PARM_DESC(atapi_enabled, "Enable discovery of ATAPI devices (0=off, 1=on [default])");

static int atapi_dmadir = 0;
module_param(atapi_dmadir, int, 0444);
MODULE_PARM_DESC(atapi_dmadir, "Enable ATAPI DMADIR bridge support (0=off [default], 1=on)");

int atapi_passthru16 = 1;
module_param(atapi_passthru16, int, 0444);
MODULE_PARM_DESC(atapi_passthru16, "Enable ATA_16 passthru for ATAPI devices (0=off, 1=on [default])");

int libata_fua = 0;
module_param_named(fua, libata_fua, int, 0444);
MODULE_PARM_DESC(fua, "FUA support (0=off [default], 1=on)");

static int ata_ignore_hpa;
module_param_named(ignore_hpa, ata_ignore_hpa, int, 0644);
MODULE_PARM_DESC(ignore_hpa, "Ignore HPA limit (0=keep BIOS limits, 1=ignore limits, using full disk)");

static int libata_dma_mask = ATA_DMA_MASK_ATA|ATA_DMA_MASK_ATAPI|ATA_DMA_MASK_CFA;
module_param_named(dma, libata_dma_mask, int, 0444);
MODULE_PARM_DESC(dma, "DMA enable/disable (0x1==ATA, 0x2==ATAPI, 0x4==CF)");

static int ata_probe_timeout;
module_param(ata_probe_timeout, int, 0444);
MODULE_PARM_DESC(ata_probe_timeout, "Set ATA probing timeout (seconds)");

int libata_noacpi = 0;
module_param_named(noacpi, libata_noacpi, int, 0444);
MODULE_PARM_DESC(noacpi, "Disable the use of ACPI in probe/suspend/resume (0=off [default], 1=on)");

int libata_allow_tpm = 0;
module_param_named(allow_tpm, libata_allow_tpm, int, 0444);
MODULE_PARM_DESC(allow_tpm, "Permit the use of TPM commands (0=off [default], 1=on)");

MODULE_AUTHOR("Jeff Garzik");
MODULE_DESCRIPTION("Library module for ATA devices");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);


static bool ata_sstatus_online(u32 sstatus)
{
	return (sstatus & 0xf) == 0x3;
}

/**
 *	ata_link_next - link iteration helper
 *	@link: the previous link, NULL to start
 *	@ap: ATA port containing links to iterate
 *	@mode: iteration mode, one of ATA_LITER_*
 *
 *	LOCKING:
 *	Host lock or EH context.
 *
 *	RETURNS:
 *	Pointer to the next link.
 */
struct ata_link *ata_link_next(struct ata_link *link, struct ata_port *ap,
			       enum ata_link_iter_mode mode)
{
	BUG_ON(mode != ATA_LITER_EDGE &&
	       mode != ATA_LITER_PMP_FIRST && mode != ATA_LITER_HOST_FIRST);

	/* NULL link indicates start of iteration */
	if (!link)
		switch (mode) {
		case ATA_LITER_EDGE:
		case ATA_LITER_PMP_FIRST:
			if (sata_pmp_attached(ap))
				return ap->pmp_link;
			/* fall through */
		case ATA_LITER_HOST_FIRST:
			return &ap->link;
		}

	/* we just iterated over the host link, what's next? */
	if (link == &ap->link)
		switch (mode) {
		case ATA_LITER_HOST_FIRST:
			if (sata_pmp_attached(ap))
				return ap->pmp_link;
			/* fall through */
		case ATA_LITER_PMP_FIRST:
			if (unlikely(ap->slave_link))
				return ap->slave_link;
			/* fall through */
		case ATA_LITER_EDGE:
			return NULL;
		}

	/* slave_link excludes PMP */
	if (unlikely(link == ap->slave_link))
		return NULL;

	/* we were over a PMP link */
	if (++link < ap->pmp_link + ap->nr_pmp_links)
		return link;

	if (mode == ATA_LITER_PMP_FIRST)
		return &ap->link;

	return NULL;
}

/**
 *	ata_dev_next - device iteration helper
 *	@dev: the previous device, NULL to start
 *	@link: ATA link containing devices to iterate
 *	@mode: iteration mode, one of ATA_DITER_*
 *
 *	LOCKING:
 *	Host lock or EH context.
 *
 *	RETURNS:
 *	Pointer to the next device.
 */
struct ata_device *ata_dev_next(struct ata_device *dev, struct ata_link *link,
				enum ata_dev_iter_mode mode)
{
	BUG_ON(mode != ATA_DITER_ENABLED && mode != ATA_DITER_ENABLED_REVERSE &&
	       mode != ATA_DITER_ALL && mode != ATA_DITER_ALL_REVERSE);

	/* NULL dev indicates start of iteration */
	if (!dev)
		switch (mode) {
		case ATA_DITER_ENABLED:
		case ATA_DITER_ALL:
			dev = link->device;
			goto check;
		case ATA_DITER_ENABLED_REVERSE:
		case ATA_DITER_ALL_REVERSE:
			dev = link->device + ata_link_max_devices(link) - 1;
			goto check;
		}

 next:
	/* move to the next one */
	switch (mode) {
	case ATA_DITER_ENABLED:
	case ATA_DITER_ALL:
		if (++dev < link->device + ata_link_max_devices(link))
			goto check;
		return NULL;
	case ATA_DITER_ENABLED_REVERSE:
	case ATA_DITER_ALL_REVERSE:
		if (--dev >= link->device)
			goto check;
		return NULL;
	}

 check:
	if ((mode == ATA_DITER_ENABLED || mode == ATA_DITER_ENABLED_REVERSE) &&
	    !ata_dev_enabled(dev))
		goto next;
	return dev;
}

/**
 *	ata_dev_phys_link - find physical link for a device
 *	@dev: ATA device to look up physical link for
 *
 *	Look up physical link which @dev is attached to.  Note that
 *	this is different from @dev->link only when @dev is on slave
 *	link.  For all other cases, it's the same as @dev->link.
 *
 *	LOCKING:
 *	Don't care.
 *
 *	RETURNS:
 *	Pointer to the found physical link.
 */
struct ata_link *ata_dev_phys_link(struct ata_device *dev)
{
	struct ata_port *ap = dev->link->ap;

	if (!ap->slave_link)
		return dev->link;
	if (!dev->devno)
		return &ap->link;
	return ap->slave_link;
}

/**
 *	ata_force_cbl - force cable type according to libata.force
 *	@ap: ATA port of interest
 *
 *	Force cable type according to libata.force and whine about it.
 *	The last entry which has matching port number is used, so it
 *	can be specified as part of device force parameters.  For
 *	example, both "a:40c,1.00:udma4" and "1.00:40c,udma4" have the
 *	same effect.
 *
 *	LOCKING:
 *	EH context.
 */
void ata_force_cbl(struct ata_port *ap)
{
	int i;

	for (i = ata_force_tbl_size - 1; i >= 0; i--) {
		const struct ata_force_ent *fe = &ata_force_tbl[i];

		if (fe->port != -1 && fe->port != ap->print_id)
			continue;

		if (fe->param.cbl == ATA_CBL_NONE)
			continue;

		ap->cbl = fe->param.cbl;
		ata_port_printk(ap, KERN_NOTICE,
				"FORCE: cable set to %s\n", fe->param.name);
		return;
	}
}

/**
 *	ata_force_link_limits - force link limits according to libata.force
 *	@link: ATA link of interest
 *
 *	Force link flags and SATA spd limit according to libata.force
 *	and whine about it.  When only the port part is specified
 *	(e.g. 1:), the limit applies to all links connected to both
 *	the host link and all fan-out ports connected via PMP.  If the
 *	device part is specified as 0 (e.g. 1.00:), it specifies the
 *	first fan-out link not the host link.  Device number 15 always
 *	points to the host link whether PMP is attached or not.  If the
 *	controller has slave link, device number 16 points to it.
 *
 *	LOCKING:
 *	EH context.
 */
static void ata_force_link_limits(struct ata_link *link)
{
	bool did_spd = false;
	int linkno = link->pmp;
	int i;

	if (ata_is_host_link(link))
		linkno += 15;

	for (i = ata_force_tbl_size - 1; i >= 0; i--) {
		const struct ata_force_ent *fe = &ata_force_tbl[i];

		if (fe->port != -1 && fe->port != link->ap->print_id)
			continue;

		if (fe->device != -1 && fe->device != linkno)
			continue;

		/* only honor the first spd limit */
		if (!did_spd && fe->param.spd_limit) {
			link->hw_sata_spd_limit = (1 << fe->param.spd_limit) - 1;
			ata_link_printk(link, KERN_NOTICE,
					"FORCE: PHY spd limit set to %s\n",
					fe->param.name);
			did_spd = true;
		}

		/* let lflags stack */
		if (fe->param.lflags) {
			link->flags |= fe->param.lflags;
			ata_link_printk(link, KERN_NOTICE,
					"FORCE: link flag 0x%x forced -> 0x%x\n",
					fe->param.lflags, link->flags);
		}
	}
}

/**
 *	ata_force_xfermask - force xfermask according to libata.force
 *	@dev: ATA device of interest
 *
 *	Force xfer_mask according to libata.force and whine about it.
 *	For consistency with link selection, device number 15 selects
 *	the first device connected to the host link.
 *
 *	LOCKING:
 *	EH context.
 */
static void ata_force_xfermask(struct ata_device *dev)
{
	int devno = dev->link->pmp + dev->devno;
	int alt_devno = devno;
	int i;

	/* allow n.15/16 for devices attached to host port */
	if (ata_is_host_link(dev->link))
		alt_devno += 15;

	for (i = ata_force_tbl_size - 1; i >= 0; i--) {
		const struct ata_force_ent *fe = &ata_force_tbl[i];
		unsigned long pio_mask, mwdma_mask, udma_mask;

		if (fe->port != -1 && fe->port != dev->link->ap->print_id)
			continue;

		if (fe->device != -1 && fe->device != devno &&
		    fe->device != alt_devno)
			continue;

		if (!fe->param.xfer_mask)
			continue;

		ata_unpack_xfermask(fe->param.xfer_mask,
				    &pio_mask, &mwdma_mask, &udma_mask);
		if (udma_mask)
			dev->udma_mask = udma_mask;
		else if (mwdma_mask) {
			dev->udma_mask = 0;
			dev->mwdma_mask = mwdma_mask;
		} else {
			dev->udma_mask = 0;
			dev->mwdma_mask = 0;
			dev->pio_mask = pio_mask;
		}

		ata_dev_printk(dev, KERN_NOTICE,
			"FORCE: xfer_mask set to %s\n", fe->param.name);
		return;
	}
}

/**
 *	ata_force_horkage - force horkage according to libata.force
 *	@dev: ATA device of interest
 *
 *	Force horkage according to libata.force and whine about it.
 *	For consistency with link selection, device number 15 selects
 *	the first device connected to the host link.
 *
 *	LOCKING:
 *	EH context.
 */
static void ata_force_horkage(struct ata_device *dev)
{
	int devno = dev->link->pmp + dev->devno;
	int alt_devno = devno;
	int i;

	/* allow n.15/16 for devices attached to host port */
	if (ata_is_host_link(dev->link))
		alt_devno += 15;

	for (i = 0; i < ata_force_tbl_size; i++) {
		const struct ata_force_ent *fe = &ata_force_tbl[i];

		if (fe->port != -1 && fe->port != dev->link->ap->print_id)
			continue;

		if (fe->device != -1 && fe->device != devno &&
		    fe->device != alt_devno)
			continue;

		if (!(~dev->horkage & fe->param.horkage_on) &&
		    !(dev->horkage & fe->param.horkage_off))
			continue;

		dev->horkage |= fe->param.horkage_on;
		dev->horkage &= ~fe->param.horkage_off;

		ata_dev_printk(dev, KERN_NOTICE,
			"FORCE: horkage modified (%s)\n", fe->param.name);
	}
}

/**
 *	atapi_cmd_type - Determine ATAPI command type from SCSI opcode
 *	@opcode: SCSI opcode
 *
 *	Determine ATAPI command type from @opcode.
 *
 *	LOCKING:
 *	None.
 *
 *	RETURNS:
 *	ATAPI_{READ|WRITE|READ_CD|PASS_THRU|MISC}
 */
int atapi_cmd_type(u8 opcode)
{
	switch (opcode) {
	case GPCMD_READ_10:
	case GPCMD_READ_12:
		return ATAPI_READ;

	case GPCMD_WRITE_10:
	case GPCMD_WRITE_12:
	case GPCMD_WRITE_AND_VERIFY_10:
		return ATAPI_WRITE;

	case GPCMD_READ_CD:
	case GPCMD_READ_CD_MSF:
		return ATAPI_READ_CD;

	case ATA_16:
	case ATA_12:
		if (atapi_passthru16)
			return ATAPI_PASS_THRU;
		/* fall thru */
	default:
		return ATAPI_MISC;
	}
}

/**
 *	ata_tf_to_fis - Convert ATA taskfile to SATA FIS structure
 *	@tf: Taskfile to convert
 *	@pmp: Port multiplier port
 *	@is_cmd: This FIS is for command
 *	@fis: Buffer into which data will output
 *
 *	Converts a standard ATA taskfile to a Serial ATA
 *	FIS structure (Register - Host to Device).
 *
 *	LOCKING:
 *	Inherited from caller.
 */
void ata_tf_to_fis(const struct ata_taskfile *tf, u8 pmp, int is_cmd, u8 *fis)
{
	fis[0] = 0x27;			/* Register - Host to Device FIS */
	fis[1] = pmp & 0xf;		/* Port multiplier number*/
	if (is_cmd)
		fis[1] |= (1 << 7);	/* bit 7 indicates Command FIS */

	fis[2] = tf->command;
	fis[3] = tf->feature;

	fis[4] = tf->lbal;
	fis[5] = tf->lbam;
	fis[6] = tf->lbah;
	fis[7] = tf->device;

	fis[8] = tf->hob_lbal;
	fis[9] = tf->hob_lbam;
	fis[10] = tf->hob_lbah;
	fis[11] = tf->hob_feature;

	fis[12] = tf->nsect;
	fis[13] = tf->hob_nsect;
	fis[14] = 0;
	fis[15] = tf->ctl;

	fis[16] = 0;
	fis[17] = 0;
	fis[18] = 0;
	fis[19] = 0;
}

/**
 *	ata_tf_from_fis - Convert SATA FIS to ATA taskfile
 *	@fis: Buffer from which data will be input
 *	@tf: Taskfile to output
 *
 *	Converts a serial ATA FIS structure to a standard ATA taskfile.
 *
 *	LOCKING:
 *	Inherited from caller.
 */

void ata_tf_from_fis(const u8 *fis, struct ata_taskfile *tf)
{
	tf->command	= fis[2];	/* status */
	tf->feature	= fis[3];	/* error */

	tf->lbal	= fis[4];
	tf->lbam	= fis[5];
	tf->lbah	= fis[6];
	tf->device	= fis[7];

	tf->hob_lbal	= fis[8];
	tf->hob_lbam	= fis[9];
	tf->hob_lbah	= fis[10];

	tf->nsect	= fis[12];
	tf->hob_nsect	= fis[13];
}

static const u8 ata_rw_cmds[] = {
	/* pio multi */
	ATA_CMD_READ_MULTI,
	ATA_CMD_WRITE_MULTI,
	ATA_CMD_READ_MULTI_EXT,
	ATA_CMD_WRITE_MULTI_EXT,
	0,
	0,
	0,
	ATA_CMD_WRITE_MULTI_FUA_EXT,
	/* pio */
	ATA_CMD_PIO_READ,
	ATA_CMD_PIO_WRITE,
	ATA_CMD_PIO_READ_EXT,
	ATA_CMD_PIO_WRITE_EXT,
	0,
	0,
	0,
	0,
	/* dma */
	ATA_CMD_READ,
	ATA_CMD_WRITE,
	ATA_CMD_READ_EXT,
	ATA_CMD_WRITE_EXT,
	0,
	0,
	0,
	ATA_CMD_WRITE_FUA_EXT
};

/**
 *	ata_rwcmd_protocol - set taskfile r/w commands and protocol
 *	@tf: command to examine and configure
 *	@dev: device tf belongs to
 *
 *	Examine the device configuration and tf->flags to calculate
 *	the proper read/write commands and protocol to use.
 *
 *	LOCKING:
 *	caller.
 */
static int ata_rwcmd_protocol(struct ata_taskfile *tf, struct ata_device *dev)
{
	u8 cmd;

	int index, fua, lba48, write;

	fua = (tf->flags & ATA_TFLAG_FUA) ? 4 : 0;
	lba48 = (tf->flags & ATA_TFLAG_LBA48) ? 2 : 0;
	write = (tf->flags & ATA_TFLAG_WRITE) ? 1 : 0;

	if (dev->flags & ATA_DFLAG_PIO) {
		tf->protocol = ATA_PROT_PIO;
		index = dev->multi_count ? 0 : 8;
	} else if (lba48 && (dev->link->ap->flags & ATA_FLAG_PIO_LBA48)) {
		/* Unable to use DMA due to host limitation */
		tf->protocol = ATA_PROT_PIO;
		index = dev->multi_count ? 0 : 8;
	} else {
		tf->protocol = ATA_PROT_DMA;
		index = 16;
	}

	cmd = ata_rw_cmds[index + fua + lba48 + write];
	if (cmd) {
		tf->command = cmd;
		return 0;
	}
	return -1;
}

/**
 *	ata_tf_read_block - Read block address from ATA taskfile
 *	@tf: ATA taskfile of interest
 *	@dev: ATA device @tf belongs to
 *
 *	LOCKING:
 *	None.
 *
 *	Read block address from @tf.  This function can handle all
 *	three address formats - LBA, LBA48 and CHS.  tf->protocol and
 *	flags select the address format to use.
 *
 *	RETURNS:
 *	Block address read from @tf.
 */
u64 ata_tf_read_block(struct ata_taskfile *tf, struct ata_device *dev)
{
	u64 block = 0;

	if (tf->flags & ATA_TFLAG_LBA) {
		if (tf->flags & ATA_TFLAG_LBA48) {
			block |= (u64)tf->hob_lbah << 40;
			block |= (u64)tf->hob_lbam << 32;
			block |= (u64)tf->hob_lbal << 24;
		} else
			block |= (tf->device & 0xf) << 24;

		block |= tf->lbah << 16;
		block |= tf->lbam << 8;
		block |= tf->lbal;
	} else {
		u32 cyl, head, sect;

		cyl = tf->lbam | (tf->lbah << 8);
		head = tf->device & 0xf;
		sect = tf->lbal;

		if (!sect) {
			ata_dev_printk(dev, KERN_WARNING, "device reported "
				       "invalid CHS sector 0\n");
			sect = 1; /* oh well */
		}

		block = (cyl * dev->heads + head) * dev->sectors + sect - 1;
	}

	return block;
}

/**
 *	ata_build_rw_tf - Build ATA taskfile for given read/write request
 *	@tf: Target ATA taskfile
 *	@dev: ATA device @tf belongs to
 *	@block: Block address
 *	@n_block: Number of blocks
 *	@tf_flags: RW/FUA etc...
 *	@tag: tag
 *
 *	LOCKING:
 *	None.
 *
 *	Build ATA taskfile @tf for read/write request described by
 *	@block, @n_block, @tf_flags and @tag on @dev.
 *
 *	RETURNS:
 *
 *	0 on success, -ERANGE if the request is too large for @dev,
 *	-EINVAL if the request is invalid.
 */
int ata_build_rw_tf(struct ata_taskfile *tf, struct ata_device *dev,
		    u64 block, u32 n_block, unsigned int tf_flags,
		    unsigned int tag)
{
	tf->flags |= ATA_TFLAG_ISADDR | ATA_TFLAG_DEVICE;
	tf->flags |= tf_flags;

	if (ata_ncq_enabled(dev) && likely(tag != ATA_TAG_INTERNAL)) {
		/* yay, NCQ */
		if (!lba_48_ok(block, n_block))
			return -ERANGE;

		tf->protocol = ATA_PROT_NCQ;
		tf->flags |= ATA_TFLAG_LBA | ATA_TFLAG_LBA48;

		if (tf->flags & ATA_TFLAG_WRITE)
			tf->command = ATA_CMD_FPDMA_WRITE;
		else
			tf->command = ATA_CMD_FPDMA_READ;

		tf->nsect = tag << 3;
		tf->hob_feature = (n_block >> 8) & 0xff;
		tf->feature = n_block & 0xff;

		tf->hob_lbah = (block >> 40) & 0xff;
		tf->hob_lbam = (block >> 32) & 0xff;
		tf->hob_lbal = (block >> 24) & 0xff;
		tf->lbah = (block >> 16) & 0xff;
		tf->lbam = (block >> 8) & 0xff;
		tf->lbal = block & 0xff;

		tf->device = 1 << 6;
		if (tf->flags & ATA_TFLAG_FUA)
			tf->device |= 1 << 7;
	} else if (dev->flags & ATA_DFLAG_LBA) {
		tf->flags |= ATA_TFLAG_LBA;

		if (lba_28_ok(block, n_block)) {
			/* use LBA28 */
			tf->device |= (block >> 24) & 0xf;
		} else if (lba_48_ok(block, n_block)) {
			if (!(dev->flags & ATA_DFLAG_LBA48))
				return -ERANGE;

			/* use LBA48 */
			tf->flags |= ATA_TFLAG_LBA48;

			tf->hob_nsect = (n_block >> 8) & 0xff;

			tf->hob_lbah = (block >> 40) & 0xff;
			tf->hob_lbam = (block >> 32) & 0xff;
			tf->hob_lbal = (block >> 24) & 0xff;
		} else
			/* request too large even for LBA48 */
			return -ERANGE;

		if (unlikely(ata_rwcmd_protocol(tf, dev) < 0))
			return -EINVAL;

		tf->nsect = n_block & 0xff;

		tf->lbah = (block >> 16) & 0xff;
		tf->lbam = (block >> 8) & 0xff;
		tf->lbal = block & 0xff;

		tf->device |= ATA_LBA;
	} else {
		/* CHS */
		u32 sect, head, cyl, track;

		/* The request -may- be too large for CHS addressing. */
		if (!lba_28_ok(block, n_block))
			return -ERANGE;

		if (unlikely(ata_rwcmd_protocol(tf, dev) < 0))
			return -EINVAL;

		/* Convert LBA to CHS */
		track = (u32)block / dev->sectors;
		cyl   = track / dev->heads;
		head  = track % dev->heads;
		sect  = (u32)block % dev->sectors + 1;

		DPRINTK("block %u track %u cyl %u head %u sect %u\n",
			(u32)block, track, cyl, head, sect);

		/* Check whether the converted CHS can fit.
		   Cylinder: 0-65535
		   Head: 0-15
		   Sector: 1-255*/
		if ((cyl >> 16) || (head >> 4) || (sect >> 8) || (!sect))
			return -ERANGE;

		tf->nsect = n_block & 0xff; /* Sector count 0 means 256 sectors */
		tf->lbal = sect;
		tf->lbam = cyl;
		tf->lbah = cyl >> 8;
		tf->device |= head;
	}

	return 0;
}

/**
 *	ata_pack_xfermask - Pack pio, mwdma and udma masks into xfer_mask
 *	@pio_mask: pio_mask
 *	@mwdma_mask: mwdma_mask
 *	@udma_mask: udma_mask
 *
 *	Pack @pio_mask, @mwdma_mask and @udma_mask into a single
 *	unsigned int xfer_mask.
 *
 *	LOCKING:
 *	None.
 *
 *	RETURNS:
 *	Packed xfer_mask.
 */
unsigned long ata_pack_xfermask(unsigned long pio_mask,
				unsigned long mwdma_mask,
				unsigned long udma_mask)
{
	return ((pio_mask << ATA_SHIFT_PIO) & ATA_MASK_PIO) |
		((mwdma_mask << ATA_SHIFT_MWDMA) & ATA_MASK_MWDMA) |
		((udma_mask << ATA_SHIFT_UDMA) & ATA_MASK_UDMA);
}

/**
 *	ata_unpack_xfermask - Unpack xfer_mask into pio, mwdma and udma masks
 *	@xfer_mask: xfer_mask to unpack
 *	@pio_mask: resulting pio_mask
 *	@mwdma_mask: resulting mwdma_mask
 *	@udma_mask: resulting udma_mask
 *
 *	Unpack @xfer_mask into @pio_mask, @mwdma_mask and @udma_mask.
 *	Any NULL distination masks will be ignored.
 */
void ata_unpack_xfermask(unsigned long xfer_mask, unsigned long *pio_mask,
			 unsigned long *mwdma_mask, unsigned long *udma_mask)
{
	if (pio_mask)
		*pio_mask = (xfer_mask & ATA_MASK_PIO) >> ATA_SHIFT_PIO;
	if (mwdma_mask)
		*mwdma_mask = (xfer_mask & ATA_MASK_MWDMA) >> ATA_SHIFT_MWDMA;
	if (udma_mask)
		*udma_mask = (xfer_mask & ATA_MASK_UDMA) >> ATA_SHIFT_UDMA;
}

static const struct ata_xfer_ent {
	int shift, bits;
	u8 base;
} ata_xfer_tbl[] = {
	{ ATA_SHIFT_PIO, ATA_NR_PIO_MODES, XFER_PIO_0 },
	{ ATA_SHIFT_MWDMA, ATA_NR_MWDMA_MODES, XFER_MW_DMA_0 },
	{ ATA_SHIFT_UDMA, ATA_NR_UDMA_MODES, XFER_UDMA_0 },
	{ -1, },
};

/**
 *	ata_xfer_mask2mode - Find matching XFER_* for the given xfer_mask
 *	@xfer_mask: xfer_mask of interest
 *
 *	Return matching XFER_* value for @xfer_mask.  Only the highest
 *	bit of @xfer_mask is considered.
 *
 *	LOCKING:
 *	None.
 *
 *	RETURNS:
 *	Matching XFER_* value, 0xff if no match found.
 */
u8 ata_xfer_mask2mode(unsigned long xfer_mask)
{
	int highbit = fls(xfer_mask) - 1;
	const struct ata_xfer_ent *ent;

	for (ent = ata_xfer_tbl; ent->shift >= 0; ent++)
		if (highbit >= ent->shift && highbit < ent->shift + ent->bits)
			return ent->base + highbit - ent->shift;
	return 0xff;
}

/**
 *	ata_xfer_mode2mask - Find matching xfer_mask for XFER_*
 *	@xfer_mode: XFER_* of interest
 *
 *	Return matching xfer_mask for @xfer_mode.
 *
 *	LOCKING:
 *	None.
 *
 *	RETURNS:
 *	Matching xfer_mask, 0 if no match found.
 */
unsigned long ata_xfer_mode2mask(u8 xfer_mode)
{
	const struct ata_xfer_ent *ent;

	for (ent = ata_xfer_tbl; ent->shift >= 0; ent++)
		if (xfer_mode >= ent->base && xfer_mode < ent->base + ent->bits)
			return ((2 << (ent->shift + xfer_mode - ent->base)) - 1)
				& ~((1 << ent->shift) - 1);
	return 0;
}

/**
 *	ata_xfer_mode2shift - Find matching xfer_shift for XFER_*
 *	@xfer_mode: XFER_* of interest
 *
 *	Return matching xfer_shift for @xfer_mode.
 *
 *	LOCKING:
 *	None.
 *
 *	RETURNS:
 *	Matching xfer_shift, -1 if no match found.
 */
int ata_xfer_mode2shift(unsigned long xfer_mode)
{
	const struct ata_xfer_ent *ent;

	for (ent = ata_xfer_tbl; ent->shift >= 0; ent++)
		if (xfer_mode >= ent->base && xfer_mode < ent->base + ent->bits)
			return ent->shift;
	return -1;
}

/**
 *	ata_mode_string - convert xfer_mask to string
 *	@xfer_mask: mask of bits supported; only highest bit counts.
 *
 *	Determine string which represents the highest speed
 *	(highest bit in @modemask).
 *
 *	LOCKING:
 *	None.
 *
 *	RETURNS:
 *	Constant C string representing highest speed listed in
 *	@mode_mask, or the constant C string "<n/a>".
 */
const char *ata_mode_string(unsigned long xfer_mask)
{
	static const char * const xfer_mode_str[] = {
		"PIO0",
		"PIO1",
		"PIO2",
		"PIO3",
		"PIO4",
		"PIO5",
		"PIO6",
		"MWDMA0",
		"MWDMA1",
		"MWDMA2",
		"MWDMA3",
		"MWDMA4",
		"UDMA/16",
		"UDMA/25",
		"UDMA/33",
		"UDMA/44",
		"UDMA/66",
		"UDMA/100",
		"UDMA/133",
		"UDMA7",
	};
	int highbit;

	highbit = fls(xfer_mask) - 1;
	if (highbit >= 0 && highbit < ARRAY_SIZE(xfer_mode_str))
		return xfer_mode_str[highbit];
	return "<n/a>";
}

static const char *sata_spd_string(unsigned int spd)
{
	static const char * const spd_str[] = {
		"1.5 Gbps",
		"3.0 Gbps",
		"6.0 Gbps",
	};

	if (spd == 0 || (spd - 1) >= ARRAY_SIZE(spd_str))
		return "<unknown>";
	return spd_str[spd - 1];
}

static int ata_dev_set_dipm(struct ata_device *dev, enum link_pm policy)
{
	struct ata_link *link = dev->link;
	struct ata_port *ap = link->ap;
	u32 scontrol;
	unsigned int err_mask;
	int rc;

	/*
	 * disallow DIPM for drivers which haven't set
	 * ATA_FLAG_IPM.  This is because when DIPM is enabled,
	 * phy ready will be set in the interrupt status on
	 * state changes, which will cause some drivers to
	 * think there are errors - additionally drivers will
	 * need to disable hot plug.
	 */
	if (!(ap->flags & ATA_FLAG_IPM) || !ata_dev_enabled(dev)) {
		ap->pm_policy = NOT_AVAILABLE;
		return -EINVAL;
	}

	/*
	 * For DIPM, we will only enable it for the
	 * min_power setting.
	 *
	 * Why?  Because Disks are too stupid to know that
	 * If the host rejects a request to go to SLUMBER
	 * they should retry at PARTIAL, and instead it
	 * just would give up.  So, for medium_power to
	 * work at all, we need to only allow HIPM.
	 */
	rc = sata_scr_read(link, SCR_CONTROL, &scontrol);
	if (rc)
		return rc;

	switch (policy) {
	case MIN_POWER:
		/* no restrictions on IPM transitions */
		scontrol &= ~(0x3 << 8);
		rc = sata_scr_write(link, SCR_CONTROL, scontrol);
		if (rc)
			return rc;

		/* enable DIPM */
		if (dev->flags & ATA_DFLAG_DIPM)
			err_mask = ata_dev_set_feature(dev,
					SETFEATURES_SATA_ENABLE, SATA_DIPM);
		break;
	case MEDIUM_POWER:
		/* allow IPM to PARTIAL */
		scontrol &= ~(0x1 << 8);
		scontrol |= (0x2 << 8);
		rc = sata_scr_write(link, SCR_CONTROL, scontrol);
		if (rc)
			return rc;

		/*
		 * we don't have to disable DIPM since IPM flags
		 * disallow transitions to SLUMBER, which effectively
		 * disable DIPM if it does not support PARTIAL
		 */
		break;
	case NOT_AVAILABLE:
	case MAX_PERFORMANCE:
		/* disable all IPM transitions */
		scontrol |= (0x3 << 8);
		rc = sata_scr_write(link, SCR_CONTROL, scontrol);
		if (rc)
			return rc;

		/*
		 * we don't have to disable DIPM since IPM flags
		 * disallow all transitions which effectively
		 * disable DIPM anyway.
		 */
		break;
	}

	/* FIXME: handle SET FEATURES failure */
	(void) err_mask;

	return 0;
}

/**
 *	ata_dev_enable_pm - enable SATA interface power management
 *	@dev:  device to enable power management
 *	@policy: the link power management policy
 *
 *	Enable SATA Interface power management.  This will enable
 *	Device Interface Power Management (DIPM) for min_power
 * 	policy, and then call driver specific callbacks for
 *	enabling Host Initiated Power management.
 *
 *	Locking: Caller.
 *	Returns: -EINVAL if IPM is not supported, 0 otherwise.
 */
void ata_dev_enable_pm(struct ata_device *dev, enum link_pm policy)
{
	int rc = 0;
	struct ata_port *ap = dev->link->ap;

	/* set HIPM first, then DIPM */
	if (ap->ops->enable_pm)
		rc = ap->ops->enable_pm(ap, policy);
	if (rc)
		goto enable_pm_out;
	rc = ata_dev_set_dipm(dev, policy);

enable_pm_out:
	if (rc)
		ap->pm_policy = MAX_PERFORMANCE;
	else
		ap->pm_policy = policy;
	return /* rc */;	/* hopefully we can use 'rc' eventually */
}

#ifdef CONFIG_PM
/**
 *	ata_dev_disable_pm - disable SATA interface power management
 *	@dev: device to disable power management
 *
 *	Disable SATA Interface power management.  This will disable
 *	Device Interface Power Management (DIPM) without changing
 * 	policy,  call driver specific callbacks for disabling Host
 * 	Initiated Power management.
 *
 *	Locking: Caller.
 *	Returns: void
 */
static void ata_dev_disable_pm(struct ata_device *dev)
{
	struct ata_port *ap = dev->link->ap;

	ata_dev_set_dipm(dev, MAX_PERFORMANCE);
	if (ap->ops->disable_pm)
		ap->ops->disable_pm(ap);
}
#endif	/* CONFIG_PM */

void ata_lpm_schedule(struct ata_port *ap, enum link_pm policy)
{
	ap->pm_policy = policy;
	ap->link.eh_info.action |= ATA_EH_LPM;
	ap->link.eh_info.flags |= ATA_EHI_NO_AUTOPSY;
	ata_port_schedule_eh(ap);
}

#ifdef CONFIG_PM
static void ata_lpm_enable(struct ata_host *host)
{
	struct ata_link *link;
	struct ata_port *ap;
	struct ata_device *dev;
	int i;

	for (i = 0; i < host->n_ports; i++) {
		ap = host->ports[i];
		ata_for_each_link(link, ap, EDGE) {
			ata_for_each_dev(dev, link, ALL)
				ata_dev_disable_pm(dev);
		}
	}
}

static void ata_lpm_disable(struct ata_host *host)
{
	int i;

	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap = host->ports[i];
		ata_lpm_schedule(ap, ap->pm_policy);
	}
}
#endif	/* CONFIG_PM */

/**
 *	ata_dev_classify - determine device type based on ATA-spec signature
 *	@tf: ATA taskfile register set for device to be identified
 *
 *	Determine from taskfile register contents whether a device is
 *	ATA or ATAPI, as per "Signature and persistence" section
 *	of ATA/PI spec (volume 1, sect 5.14).
 *
 *	LOCKING:
 *	None.
 *
 *	RETURNS:
 *	Device type, %ATA_DEV_ATA, %ATA_DEV_ATAPI, %ATA_DEV_PMP or
 *	%ATA_DEV_UNKNOWN the event of failure.
 */
unsigned int ata_dev_classify(const struct ata_taskfile *tf)
{
	/* Apple's open source Darwin code hints that some devices only
	 * put a proper signature into the LBA mid/high registers,
	 * So, we only check those.  It's sufficient for uniqueness.
	 *
	 * ATA/ATAPI-7 (d1532v1r1: Feb. 19, 2003) specified separate
	 * signatures for ATA and ATAPI devices attached on SerialATA,
	 * 0x3c/0xc3 and 0x69/0x96 respectively.  However, SerialATA
	 * spec has never mentioned about using different signatures
	 * for ATA/ATAPI devices.  Then, Serial ATA II: Port
	 * Multiplier specification began to use 0x69/0x96 to identify
	 * port multpliers and 0x3c/0xc3 to identify SEMB device.
	 * ATA/ATAPI-7 dropped descriptions about 0x3c/0xc3 and
	 * 0x69/0x96 shortly and described them as reserved for
	 * SerialATA.
	 *
	 * We follow the current spec and consider that 0x69/0x96
	 * identifies a port multiplier and 0x3c/0xc3 a SEMB device.
	 * Unfortunately, WDC WD1600JS-62MHB5 (a hard drive) reports
	 * SEMB signature.  This is worked around in
	 * ata_dev_read_id().
	 */
	if ((tf->lbam == 0) && (tf->lbah == 0)) {
		DPRINTK("found ATA device by sig\n");
		return ATA_DEV_ATA;
	}

	if ((tf->lbam == 0x14) && (tf->lbah == 0xeb)) {
		DPRINTK("found ATAPI device by sig\n");
		return ATA_DEV_ATAPI;
	}

	if ((tf->lbam == 0x69) && (tf->lbah == 0x96)) {
		DPRINTK("found PMP device by sig\n");
		return ATA_DEV_PMP;
	}

	if ((tf->lbam == 0x3c) && (tf->lbah == 0xc3)) {
		DPRINTK("found SEMB device by sig (could be ATA device)\n");
		return ATA_DEV_SEMB;
	}

	DPRINTK("unknown device\n");
	return ATA_DEV_UNKNOWN;
}

/**
 *	ata_id_string - Convert IDENTIFY DEVICE page into string
 *	@id: IDENTIFY DEVICE results we will examine
 *	@s: string into which data is output
 *	@ofs: offset into identify device page
 *	@len: length of string to return. must be an even number.
 *
 *	The strings in the IDENTIFY DEVICE page are broken up into
 *	16-bit chunks.  Run through the string, and output each
 *	8-bit chunk linearly, regardless of platform.
 *
 *	LOCKING:
 *	caller.
 */

void ata_id_string(const u16 *id, unsigned char *s,
		   unsigned int ofs, unsigned int len)
{
	unsigned int c;

	BUG_ON(len & 1);

	while (len > 0) {
		c = id[ofs] >> 8;
		*s = c;
		s++;

		c = id[ofs] & 0xff;
		*s = c;
		s++;

		ofs++;
		len -= 2;
	}
}

/**
 *	ata_id_c_string - Convert IDENTIFY DEVICE page into C string
 *	@id: IDENTIFY DEVICE results we will examine
 *	@s: string into which data is output
 *	@ofs: offset into identify device page
 *	@len: length of string to return. must be an odd number.
 *
 *	This function is identical to ata_id_string except that it
 *	trims trailing spaces and terminates the resulting string with
 *	null.  @len must be actual maximum length (even number) + 1.
 *
 *	LOCKING:
 *	caller.
 */
void ata_id_c_string(const u16 *id, unsigned char *s,
		     unsigned int ofs, unsigned int len)
{
	unsigned char *p;

	ata_id_string(id, s, ofs, len - 1);

	p = s + strnlen(s, len - 1);
	while (p > s && p[-1] == ' ')
		p--;
	*p = '\0';
}

static u64 ata_id_n_sectors(const u16 *id)
{
	if (ata_id_has_lba(id)) {
		if (ata_id_has_lba48(id))
			return ata_id_u64(id, ATA_ID_LBA_CAPACITY_2);
		else
			return ata_id_u32(id, ATA_ID_LBA_CAPACITY);
	} else {
		if (ata_id_current_chs_valid(id))
			return id[ATA_ID_CUR_CYLS] * id[ATA_ID_CUR_HEADS] *
			       id[ATA_ID_CUR_SECTORS];
		else
			return id[ATA_ID_CYLS] * id[ATA_ID_HEADS] *
			       id[ATA_ID_SECTORS];
	}
}

u64 ata_tf_to_lba48(const struct ata_taskfile *tf)
{
	u64 sectors = 0;

	sectors |= ((u64)(tf->hob_lbah & 0xff)) << 40;
	sectors |= ((u64)(tf->hob_lbam & 0xff)) << 32;
	sectors |= ((u64)(tf->hob_lbal & 0xff)) << 24;
	sectors |= (tf->lbah & 0xff) << 16;
	sectors |= (tf->lbam & 0xff) << 8;
	sectors |= (tf->lbal & 0xff);

	return sectors;
}

u64 ata_tf_to_lba(const struct ata_taskfile *tf)
{
	u64 sectors = 0;

	sectors |= (tf->device & 0x0f) << 24;
	sectors |= (tf->lbah & 0xff) << 16;
	sectors |= (tf->lbam & 0xff) << 8;
	sectors |= (tf->lbal & 0xff);

	return sectors;
}

/**
 *	ata_read_native_max_address - Read native max address
 *	@dev: target device
 *	@max_sectors: out parameter for the result native max address
 *
 *	Perform an LBA48 or LBA28 native size query upon the device in
 *	question.
 *
 *	RETURNS:
 *	0 on success, -EACCES if command is aborted by the drive.
 *	-EIO on other errors.
 */
static int ata_read_native_max_address(struct ata_device *dev, u64 *max_sectors)
{
	unsigned int err_mask;
	struct ata_taskfile tf;
	int lba48 = ata_id_has_lba48(dev->id);

	ata_tf_init(dev, &tf);

	/* always clear all address registers */
	tf.flags |= ATA_TFLAG_DEVICE | ATA_TFLAG_ISADDR;

	if (lba48) {
		tf.command = ATA_CMD_READ_NATIVE_MAX_EXT;
		tf.flags |= ATA_TFLAG_LBA48;
	} else
		tf.command = ATA_CMD_READ_NATIVE_MAX;

	tf.protocol |= ATA_PROT_NODATA;
	tf.device |= ATA_LBA;

	err_mask = ata_exec_internal(dev, &tf, NULL, DMA_NONE, NULL, 0, 0);
	if (err_mask) {
		ata_dev_printk(dev, KERN_WARNING, "failed to read native "
			       "max address (err_mask=0x%x)\n", err_mask);
		if (err_mask == AC_ERR_DEV && (tf.feature & ATA_ABORTED))
			return -EACCES;
		return -EIO;
	}

	if (lba48)
		*max_sectors = ata_tf_to_lba48(&tf) + 1;
	else
		*max_sectors = ata_tf_to_lba(&tf) + 1;
	if (dev->horkage & ATA_HORKAGE_HPA_SIZE)
		(*max_sectors)--;
	return 0;
}

/**
 *	ata_set_max_sectors - Set max sectors
 *	@dev: target device
 *	@new_sectors: new max sectors value to set for the device
 *
 *	Set max sectors of @dev to @new_sectors.
 *
 *	RETURNS:
 *	0 on success, -EACCES if command is aborted or denied (due to
 *	previous non-volatile SET_MAX) by the drive.  -EIO on other
 *	errors.
 */
static int ata_set_max_sectors(struct ata_device *dev, u64 new_sectors)
{
	unsigned int err_mask;
	struct ata_taskfile tf;
	int lba48 = ata_id_has_lba48(dev->id);

	new_sectors--;

	ata_tf_init(dev, &tf);

	tf.flags |= ATA_TFLAG_DEVICE | ATA_TFLAG_ISADDR;

	if (lba48) {
		tf.command = ATA_CMD_SET_MAX_EXT;
		tf.flags |= ATA_TFLAG_LBA48;

		tf.hob_lbal = (new_sectors >> 24) & 0xff;
		tf.hob_lbam = (new_sectors >> 32) & 0xff;
		tf.hob_lbah = (new_sectors >> 40) & 0xff;
	} else {
		tf.command = ATA_CMD_SET_MAX;

		tf.device |= (new_sectors >> 24) & 0xf;
	}

	tf.protocol |= ATA_PROT_NODATA;
	tf.device |= ATA_LBA;

	tf.lbal = (new_sectors >> 0) & 0xff;
	tf.lbam = (new_sectors >> 8) & 0xff;
	tf.lbah = (new_sectors >> 16) & 0xff;

	err_mask = ata_exec_internal(dev, &tf, NULL, DMA_NONE, NULL, 0, 0);
	if (err_mask) {
		ata_dev_printk(dev, KERN_WARNING, "failed to set "
			       "max address (err_mask=0x%x)\n", err_mask);
		if (err_mask == AC_ERR_DEV &&
		    (tf.feature & (ATA_ABORTED | ATA_IDNF)))
			return -EACCES;
		return -EIO;
	}

	return 0;
}

/**
 *	ata_hpa_resize		-	Resize a device with an HPA set
 *	@dev: Device to resize
 *
 *	Read the size of an LBA28 or LBA48 disk with HPA features and resize
 *	it if required to the full size of the media. The caller must check
 *	the drive has the HPA feature set enabled.
 *
 *	RETURNS:
 *	0 on success, -errno on failure.
 */
static int ata_hpa_resize(struct ata_device *dev)
{
	struct ata_eh_context *ehc = &dev->link->eh_context;
	int print_info = ehc->i.flags & ATA_EHI_PRINTINFO;
	u64 sectors = ata_id_n_sectors(dev->id);
	u64 native_sectors;
	int rc;

	/* do we need to do it? */
	if (dev->class != ATA_DEV_ATA ||
	    !ata_id_has_lba(dev->id) || !ata_id_hpa_enabled(dev->id) ||
	    (dev->horkage & ATA_HORKAGE_BROKEN_HPA))
		return 0;

	/* read native max address */
	rc = ata_read_native_max_address(dev, &native_sectors);
	if (rc) {
		/* If device aborted the command or HPA isn't going to
		 * be unlocked, skip HPA resizing.
		 */
		if (rc == -EACCES || !ata_ignore_hpa) {
			ata_dev_printk(dev, KERN_WARNING, "HPA support seems "
				       "broken, skipping HPA handling\n");
			dev->horkage |= ATA_HORKAGE_BROKEN_HPA;

			/* we can continue if device aborted the command */
			if (rc == -EACCES)
				rc = 0;
		}

		return rc;
	}
	dev->n_native_sectors = native_sectors;

	/* nothing to do? */
	if (native_sectors <= sectors || !ata_ignore_hpa) {
		if (!print_info || native_sectors == sectors)
			return 0;

		if (native_sectors > sectors)
			ata_dev_printk(dev, KERN_INFO,
				"HPA detected: current %llu, native %llu\n",
				(unsigned long long)sectors,
				(unsigned long long)native_sectors);
		else if (native_sectors < sectors)
			ata_dev_printk(dev, KERN_WARNING,
				"native sectors (%llu) is smaller than "
				"sectors (%llu)\n",
				(unsigned long long)native_sectors,
				(unsigned long long)sectors);
		return 0;
	}

	/* let's unlock HPA */
	rc = ata_set_max_sectors(dev, native_sectors);
	if (rc == -EACCES) {
		/* if device aborted the command, skip HPA resizing */
		ata_dev_printk(dev, KERN_WARNING, "device aborted resize "
			       "(%llu -> %llu), skipping HPA handling\n",
			       (unsigned long long)sectors,
			       (unsigned long long)native_sectors);
		dev->horkage |= ATA_HORKAGE_BROKEN_HPA;
		return 0;
	} else if (rc)
		return rc;

	/* re-read IDENTIFY data */
	rc = ata_dev_reread_id(dev, 0);
	if (rc) {
		ata_dev_printk(dev, KERN_ERR, "failed to re-read IDENTIFY "
			       "data after HPA resizing\n");
		return rc;
	}

	if (print_info) {
		u64 new_sectors = ata_id_n_sectors(dev->id);
		ata_dev_printk(dev, KERN_INFO,
			"HPA unlocked: %llu -> %llu, native %llu\n",
			(unsigned long long)sectors,
			(unsigned long long)new_sectors,
			(unsigned long long)native_sectors);
	}

	return 0;
}

/**
 *	ata_dump_id - IDENTIFY DEVICE info debugging output
 *	@id: IDENTIFY DEVICE page to dump
 *
 *	Dump selected 16-bit words from the given IDENTIFY DEVICE
 *	page.
 *
 *	LOCKING:
 *	caller.
 */

static inline void ata_dump_id(const u16 *id)
{
	DPRINTK("49==0x%04x  "
		"53==0x%04x  "
		"63==0x%04x  "
		"64==0x%04x  "
		"75==0x%04x  \n",
		id[49],
		id[53],
		id[63],
		id[64],
		id[75]);
	DPRINTK("80==0x%04x  "
		"81==0x%04x  "
		"82==0x%04x  "
		"83==0x%04x  "
		"84==0x%04x  \n",
		id[80],
		id[81],
		id[82],
		id[83],
		id[84]);
	DPRINTK("88==0x%04x  "
		"93==0x%04x\n",
		id[88],
		id[93]);
}

/**
 *	ata_id_xfermask - Compute xfermask from the given IDENTIFY data
 *	@id: IDENTIFY data to compute xfer mask from
 *
 *	Compute the xfermask for this device. This is not as trivial
 *	as it seems if we must consider early devices correctly.
 *
 *	FIXME: pre IDE drive timing (do we care ?).
 *
 *	LOCKING:
 *	None.
 *
 *	RETURNS:
 *	Computed xfermask
 */
unsigned long ata_id_xfermask(const u16 *id)
{
	unsigned long pio_mask, mwdma_mask, udma_mask;

	/* Usual case. Word 53 indicates word 64 is valid */
	if (id[ATA_ID_FIELD_VALID] & (1 << 1)) {
		pio_mask = id[ATA_ID_PIO_MODES] & 0x03;
		pio_mask <<= 3;
		pio_mask |= 0x7;
	} else {
		/* If word 64 isn't valid then Word 51 high byte holds
		 * the PIO timing number for the maximum. Turn it into
		 * a mask.
		 */
		u8 mode = (id[ATA_ID_OLD_PIO_MODES] >> 8) & 0xFF;
		if (mode < 5)	/* Valid PIO range */
			pio_mask = (2 << mode) - 1;
		else
			pio_mask = 1;

		/* But wait.. there's more. Design your standards by
		 * committee and you too can get a free iordy field to
		 * process. However its the speeds not the modes that
		 * are supported... Note drivers using the timing API
		 * will get this right anyway
		 */
	}

	mwdma_mask = id[ATA_ID_MWDMA_MODES] & 0x07;

	if (ata_id_is_cfa(id)) {
		/*
		 *	Process compact flash extended modes
		 */
		int pio = (id[ATA_ID_CFA_MODES] >> 0) & 0x7;
		int dma = (id[ATA_ID_CFA_MODES] >> 3) & 0x7;

		if (pio)
			pio_mask |= (1 << 5);
		if (pio > 1)
			pio_mask |= (1 << 6);
		if (dma)
			mwdma_mask |= (1 << 3);
		if (dma > 1)
			mwdma_mask |= (1 << 4);
	}

	udma_mask = 0;
	if (id[ATA_ID_FIELD_VALID] & (1 << 2))
		udma_mask = id[ATA_ID_UDMA_MODES] & 0xff;

	return ata_pack_xfermask(pio_mask, mwdma_mask, udma_mask);
}

/**
 *	ata_pio_queue_task - Queue port_task
 *	@ap: The ata_port to queue port_task for
 *	@data: data for @fn to use
 *	@delay: delay time in msecs for workqueue function
 *
 *	Schedule @fn(@data) for execution after @delay jiffies using
 *	port_task.  There is one port_task per port and it's the
 *	user(low level driver)'s responsibility to make sure that only
 *	one task is active at any given time.
 *
 *	libata core layer takes care of synchronization between
 *	port_task and EH.  ata_pio_queue_task() may be ignored for EH
 *	synchronization.
 *
 *	LOCKING:
 *	Inherited from caller.
 */
void ata_pio_queue_task(struct ata_port *ap, void *data, unsigned long delay)
{
	ap->port_task_data = data;

	/* may fail if ata_port_flush_task() in progress */
	queue_delayed_work(ata_wq, &ap->port_task, msecs_to_jiffies(delay));
}

/**
 *	ata_port_flush_task - Flush port_task
 *	@ap: The ata_port to flush port_task for
 *
 *	After this function completes, port_task is guranteed not to
 *	be running or scheduled.
 *
 *	LOCKING:
 *	Kernel thread context (may sleep)
 */
void ata_port_flush_task(struct ata_port *ap)
{
	DPRINTK("ENTER\n");

	cancel_rearming_delayed_work(&ap->port_task);

	if (ata_msg_ctl(ap))
		ata_port_printk(ap, KERN_DEBUG, "%s: EXIT\n", __func__);
}

static void ata_qc_complete_internal(struct ata_queued_cmd *qc)
{
	struct completion *waiting = qc->private_data;

	complete(waiting);
}

/**
 *	ata_exec_internal_sg - execute libata internal command
 *	@dev: Device to which the command is sent
 *	@tf: Taskfile registers for the command and the result
 *	@cdb: CDB for packet command
 *	@dma_dir: Data tranfer direction of the command
 *	@sgl: sg list for the data buffer of the command
 *	@n_elem: Number of sg entries
 *	@timeout: Timeout in msecs (0 for default)
 *
 *	Executes libata internal command with timeout.  @tf contains
 *	command on entry and result on return.  Timeout and error
 *	conditions are reported via return value.  No recovery action
 *	is taken after a command times out.  It's caller's duty to
 *	clean up after timeout.
 *
 *	LOCKING:
 *	None.  Should be called with kernel context, might sleep.
 *
 *	RETURNS:
 *	Zero on success, AC_ERR_* mask on failure
 */
unsigned ata_exec_internal_sg(struct ata_device *dev,
			      struct ata_taskfile *tf, const u8 *cdb,
			      int dma_dir, struct scatterlist *sgl,
			      unsigned int n_elem, unsigned long timeout)
{
	struct ata_link *link = dev->link;
	struct ata_port *ap = link->ap;
	u8 command = tf->command;
	int auto_timeout = 0;
	struct ata_queued_cmd *qc;
	unsigned int tag, preempted_tag;
	u32 preempted_sactive, preempted_qc_active;
	int preempted_nr_active_links;
	DECLARE_COMPLETION_ONSTACK(wait);
	unsigned long flags;
	unsigned int err_mask;
	int rc;

	spin_lock_irqsave(ap->lock, flags);

	/* no internal command while frozen */
	if (ap->pflags & ATA_PFLAG_FROZEN) {
		spin_unlock_irqrestore(ap->lock, flags);
		return AC_ERR_SYSTEM;
	}

	/* initialize internal qc */

	/* XXX: Tag 0 is used for drivers with legacy EH as some
	 * drivers choke if any other tag is given.  This breaks
	 * ata_tag_internal() test for those drivers.  Don't use new
	 * EH stuff without converting to it.
	 */
	if (ap->ops->error_handler)
		tag = ATA_TAG_INTERNAL;
	else
		tag = 0;

	if (test_and_set_bit(tag, &ap->qc_allocated))
		BUG();
	qc = __ata_qc_from_tag(ap, tag);

	qc->tag = tag;
	qc->scsicmd = NULL;
	qc->ap = ap;
	qc->dev = dev;
	ata_qc_reinit(qc);

	preempted_tag = link->active_tag;
	preempted_sactive = link->sactive;
	preempted_qc_active = ap->qc_active;
	preempted_nr_active_links = ap->nr_active_links;
	link->active_tag = ATA_TAG_POISON;
	link->sactive = 0;
	ap->qc_active = 0;
	ap->nr_active_links = 0;

	/* prepare & issue qc */
	qc->tf = *tf;
	if (cdb)
		memcpy(qc->cdb, cdb, ATAPI_CDB_LEN);
	qc->flags |= ATA_QCFLAG_RESULT_TF;
	qc->dma_dir = dma_dir;
	if (dma_dir != DMA_NONE) {
		unsigned int i, buflen = 0;
		struct scatterlist *sg;

		for_each_sg(sgl, sg, n_elem, i)
			buflen += sg->length;

		ata_sg_init(qc, sgl, n_elem);
		qc->nbytes = buflen;
	}

	qc->private_data = &wait;
	qc->complete_fn = ata_qc_complete_internal;

	ata_qc_issue(qc);

	spin_unlock_irqrestore(ap->lock, flags);

	if (!timeout) {
		if (ata_probe_timeout)
			timeout = ata_probe_timeout * 1000;
		else {
			timeout = ata_internal_cmd_timeout(dev, command);
			auto_timeout = 1;
		}
	}

	rc = wait_for_completion_timeout(&wait, msecs_to_jiffies(timeout));

	ata_port_flush_task(ap);

	if (!rc) {
		spin_lock_irqsave(ap->lock, flags);

		/* We're racing with irq here.  If we lose, the
		 * following test prevents us from completing the qc
		 * twice.  If we win, the port is frozen and will be
		 * cleaned up by ->post_internal_cmd().
		 */
		if (qc->flags & ATA_QCFLAG_ACTIVE) {
			qc->err_mask |= AC_ERR_TIMEOUT;

			if (ap->ops->error_handler)
				ata_port_freeze(ap);
			else
				ata_qc_complete(qc);

			if (ata_msg_warn(ap))
				ata_dev_printk(dev, KERN_WARNING,
					"qc timeout (cmd 0x%x)\n", command);
		}

		spin_unlock_irqrestore(ap->lock, flags);
	}

	/* do post_internal_cmd */
	if (ap->ops->post_internal_cmd)
		ap->ops->post_internal_cmd(qc);

	/* perform minimal error analysis */
	if (qc->flags & ATA_QCFLAG_FAILED) {
		if (qc->result_tf.command & (ATA_ERR | ATA_DF))
			qc->err_mask |= AC_ERR_DEV;

		if (!qc->err_mask)
			qc->err_mask |= AC_ERR_OTHER;

		if (qc->err_mask & ~AC_ERR_OTHER)
			qc->err_mask &= ~AC_ERR_OTHER;
	}

	/* finish up */
	spin_lock_irqsave(ap->lock, flags);

	*tf = qc->result_tf;
	err_mask = qc->err_mask;

	ata_qc_free(qc);
	link->active_tag = preempted_tag;
	link->sactive = preempted_sactive;
	ap->qc_active = preempted_qc_active;
	ap->nr_active_links = preempted_nr_active_links;

	/* XXX - Some LLDDs (sata_mv) disable port on command failure.
	 * Until those drivers are fixed, we detect the condition
	 * here, fail the command with AC_ERR_SYSTEM and reenable the
	 * port.
	 *
	 * Note that this doesn't change any behavior as internal
	 * command failure results in disabling the device in the
	 * higher layer for LLDDs without new reset/EH callbacks.
	 *
	 * Kill the following code as soon as those drivers are fixed.
	 */
	if (ap->flags & ATA_FLAG_DISABLED) {
		err_mask |= AC_ERR_SYSTEM;
		ata_port_probe(ap);
	}

	spin_unlock_irqrestore(ap->lock, flags);

	if ((err_mask & AC_ERR_TIMEOUT) && auto_timeout)
		ata_internal_cmd_timed_out(dev, command);

	return err_mask;
}

/**
 *	ata_exec_internal - execute libata internal command
 *	@dev: Device to which the command is sent
 *	@tf: Taskfile registers for the command and the result
 *	@cdb: CDB for packet command
 *	@dma_dir: Data tranfer direction of the command
 *	@buf: Data buffer of the command
 *	@buflen: Length of data buffer
 *	@timeout: Timeout in msecs (0 for default)
 *
 *	Wrapper around ata_exec_internal_sg() which takes simple
 *	buffer instead of sg list.
 *
 *	LOCKING:
 *	None.  Should be called with kernel context, might sleep.
 *
 *	RETURNS:
 *	Zero on success, AC_ERR_* mask on failure
 */
unsigned ata_exec_internal(struct ata_device *dev,
			   struct ata_taskfile *tf, const u8 *cdb,
			   int dma_dir, void *buf, unsigned int buflen,
			   unsigned long timeout)
{
	struct scatterlist *psg = NULL, sg;
	unsigned int n_elem = 0;

	if (dma_dir != DMA_NONE) {
		WARN_ON(!buf);
		sg_init_one(&sg, buf, buflen);
		psg = &sg;
		n_elem++;
	}

	return ata_exec_internal_sg(dev, tf, cdb, dma_dir, psg, n_elem,
				    timeout);
}

/**
 *	ata_do_simple_cmd - execute simple internal command
 *	@dev: Device to which the command is sent
 *	@cmd: Opcode to execute
 *
 *	Execute a 'simple' command, that only consists of the opcode
 *	'cmd' itself, without filling any other registers
 *
 *	LOCKING:
 *	Kernel thread context (may sleep).
 *
 *	RETURNS:
 *	Zero on success, AC_ERR_* mask on failure
 */
unsigned int ata_do_simple_cmd(struct ata_device *dev, u8 cmd)
{
	struct ata_taskfile tf;

	ata_tf_init(dev, &tf);

	tf.command = cmd;
	tf.flags |= ATA_TFLAG_DEVICE;
	tf.protocol = ATA_PROT_NODATA;

	return ata_exec_internal(dev, &tf, NULL, DMA_NONE, NULL, 0, 0);
}

/**
 *	ata_pio_need_iordy	-	check if iordy needed
 *	@adev: ATA device
 *
 *	Check if the current speed of the device requires IORDY. Used
 *	by various controllers for chip configuration.
 */
unsigned int ata_pio_need_iordy(const struct ata_device *adev)
{
	/* Don't set IORDY if we're preparing for reset.  IORDY may
	 * lead to controller lock up on certain controllers if the
	 * port is not occupied.  See bko#11703 for details.
	 */
	if (adev->link->ap->pflags & ATA_PFLAG_RESETTING)
		return 0;
	/* Controller doesn't support IORDY.  Probably a pointless
	 * check as the caller should know this.
	 */
	if (adev->link->ap->flags & ATA_FLAG_NO_IORDY)
		return 0;
	/* CF spec. r4.1 Table 22 says no iordy on PIO5 and PIO6.  */
	if (ata_id_is_cfa(adev->id)
	    && (adev->pio_mode == XFER_PIO_5 || adev->pio_mode == XFER_PIO_6))
		return 0;
	/* PIO3 and higher it is mandatory */
	if (adev->pio_mode > XFER_PIO_2)
		return 1;
	/* We turn it on when possible */
	if (ata_id_has_iordy(adev->id))
		return 1;
	return 0;
}

/**
 *	ata_pio_mask_no_iordy	-	Return the non IORDY mask
 *	@adev: ATA device
 *
 *	Compute the highest mode possible if we are not using iordy. Return
 *	-1 if no iordy mode is available.
 */
static u32 ata_pio_mask_no_iordy(const struct ata_device *adev)
{
	/* If we have no drive specific rule, then PIO 2 is non IORDY */
	if (adev->id[ATA_ID_FIELD_VALID] & 2) {	/* EIDE */
		u16 pio = adev->id[ATA_ID_EIDE_PIO];
		/* Is the speed faster than the drive allows non IORDY ? */
		if (pio) {
			/* This is cycle times not frequency - watch the logic! */
			if (pio > 240)	/* PIO2 is 240nS per cycle */
				return 3 << ATA_SHIFT_PIO;
			return 7 << ATA_SHIFT_PIO;
		}
	}
	return 3 << ATA_SHIFT_PIO;
}

/**
 *	ata_do_dev_read_id		-	default ID read method
 *	@dev: device
 *	@tf: proposed taskfile
 *	@id: data buffer
 *
 *	Issue the identify taskfile and hand back the buffer containing
 *	identify data. For some RAID controllers and for pre ATA devices
 *	this function is wrapped or replaced by the driver
 */
unsigned int ata_do_dev_read_id(struct ata_device *dev,
					struct ata_taskfile *tf, u16 *id)
{
	return ata_exec_internal(dev, tf, NULL, DMA_FROM_DEVICE,
				     id, sizeof(id[0]) * ATA_ID_WORDS, 0);
}

/**
 *	ata_dev_read_id - Read ID data from the specified device
 *	@dev: target device
 *	@p_class: pointer to class of the target device (may be changed)
 *	@flags: ATA_READID_* flags
 *	@id: buffer to read IDENTIFY data into
 *
 *	Read ID data from the specified device.  ATA_CMD_ID_ATA is
 *	performed on ATA devices and ATA_CMD_ID_ATAPI on ATAPI
 *	devices.  This function also issues ATA_CMD_INIT_DEV_PARAMS
 *	for pre-ATA4 drives.
 *
 *	FIXME: ATA_CMD_ID_ATA is optional for early drives and right
 *	now we abort if we hit that case.
 *
 *	LOCKING:
 *	Kernel thread context (may sleep)
 *
 *	RETURNS:
 *	0 on success, -errno otherwise.
 */
int ata_dev_read_id(struct ata_device *dev, unsigned int *p_class,
		    unsigned int flags, u16 *id)
{
	struct ata_port *ap = dev->link->ap;
	unsigned int class = *p_class;
	struct ata_taskfile tf;
	unsigned int err_mask = 0;
	const char *reason;
	bool is_semb = class == ATA_DEV_SEMB;
	int may_fallback = 1, tried_spinup = 0;
	int rc;

	if (ata_msg_ctl(ap))
		ata_dev_printk(dev, KERN_DEBUG, "%s: ENTER\n", __func__);

retry:
	ata_tf_init(dev, &tf);

	switch (class) {
	case ATA_DEV_SEMB:
		class = ATA_DEV_ATA;	/* some hard drives report SEMB sig */
	case ATA_DEV_ATA:
		tf.command = ATA_CMD_ID_ATA;
		break;
	case ATA_DEV_ATAPI:
		tf.command = ATA_CMD_ID_ATAPI;
		break;
	default:
		rc = -ENODEV;
		reason = "unsupported class";
		goto err_out;
	}

	tf.protocol = ATA_PROT_PIO;

	/* Some devices choke if TF registers contain garbage.  Make
	 * sure those are properly initialized.
	 */
	tf.flags |= ATA_TFLAG_ISADDR | ATA_TFLAG_DEVICE;

	/* Device presence detection is unreliable on some
	 * controllers.  Always poll IDENTIFY if available.
	 */
	tf.flags |= ATA_TFLAG_POLLING;

	if (ap->ops->read_id)
		err_mask = ap->ops->read_id(dev, &tf, id);
	else
		err_mask = ata_do_dev_read_id(dev, &tf, id);

	if (err_mask) {
		if (err_mask & AC_ERR_NODEV_HINT) {
			ata_dev_printk(dev, KERN_DEBUG,
				       "NODEV after polling detection\n");
			return -ENOENT;
		}

		if (is_semb) {
			ata_dev_printk(dev, KERN_INFO, "IDENTIFY failed on "
				       "device w/ SEMB sig, disabled\n");
			/* SEMB is not supported yet */
			*p_class = ATA_DEV_SEMB_UNSUP;
			return 0;
		}

		if ((err_mask == AC_ERR_DEV) && (tf.feature & ATA_ABORTED)) {
			/* Device or controller might have reported
			 * the wrong device class.  Give a shot at the
			 * other IDENTIFY if the current one is
			 * aborted by the device.
			 */
			if (may_fallback) {
				may_fallback = 0;

				if (class == ATA_DEV_ATA)
					class = ATA_DEV_ATAPI;
				else
					class = ATA_DEV_ATA;
				goto retry;
			}

			/* Control reaches here iff the device aborted
			 * both flavors of IDENTIFYs which happens
			 * sometimes with phantom devices.
			 */
			ata_dev_printk(dev, KERN_DEBUG,
				       "both IDENTIFYs aborted, assuming NODEV\n");
			return -ENOENT;
		}

		rc = -EIO;
		reason = "I/O error";
		goto err_out;
	}

	/* Falling back doesn't make sense if ID data was read
	 * successfully at least once.
	 */
	may_fallback = 0;

	swap_buf_le16(id, ATA_ID_WORDS);

	/* sanity check */
	rc = -EINVAL;
	reason = "device reports invalid type";

	if (class == ATA_DEV_ATA) {
		if (!ata_id_is_ata(id) && !ata_id_is_cfa(id))
			goto err_out;
	} else {
		if (ata_id_is_ata(id))
			goto err_out;
	}

	if (!tried_spinup && (id[2] == 0x37c8 || id[2] == 0x738c)) {
		tried_spinup = 1;
		/*
		 * Drive powered-up in standby mode, and requires a specific
		 * SET_FEATURES spin-up subcommand before it will accept
		 * anything other than the original IDENTIFY command.
		 */
		err_mask = ata_dev_set_feature(dev, SETFEATURES_SPINUP, 0);
		if (err_mask && id[2] != 0x738c) {
			rc = -EIO;
			reason = "SPINUP failed";
			goto err_out;
		}
		/*
		 * If the drive initially returned incomplete IDENTIFY info,
		 * we now must reissue the IDENTIFY command.
		 */
		if (id[2] == 0x37c8)
			goto retry;
	}

	if ((flags & ATA_READID_POSTRESET) && class == ATA_DEV_ATA) {
		/*
		 * The exact sequence expected by certain pre-ATA4 drives is:
		 * SRST RESET
		 * IDENTIFY (optional in early ATA)
		 * INITIALIZE DEVICE PARAMETERS (later IDE and ATA)
		 * anything else..
		 * Some drives were very specific about that exact sequence.
		 *
		 * Note that ATA4 says lba is mandatory so the second check
		 * shoud never trigger.
		 */
		if (ata_id_major_version(id) < 4 || !ata_id_has_lba(id)) {
			err_mask = ata_dev_init_params(dev, id[3], id[6]);
			if (err_mask) {
				rc = -EIO;
				reason = "INIT_DEV_PARAMS failed";
				goto err_out;
			}

			/* current CHS translation info (id[53-58]) might be
			 * changed. reread the identify device info.
			 */
			flags &= ~ATA_READID_POSTRESET;
			goto retry;
		}
	}

	*p_class = class;

	return 0;

 err_out:
	if (ata_msg_warn(ap))
		ata_dev_printk(dev, KERN_WARNING, "failed to IDENTIFY "
			       "(%s, err_mask=0x%x)\n", reason, err_mask);
	return rc;
}

static int ata_do_link_spd_horkage(struct ata_device *dev)
{
	struct ata_link *plink = ata_dev_phys_link(dev);
	u32 target, target_limit;

	if (!sata_scr_valid(plink))
		return 0;

	if (dev->horkage & ATA_HORKAGE_1_5_GBPS)
		target = 1;
	else
		return 0;

	target_limit = (1 << target) - 1;

	/* if already on stricter limit, no need to push further */
	if (plink->sata_spd_limit <= target_limit)
		return 0;

	plink->sata_spd_limit = target_limit;

	/* Request another EH round by returning -EAGAIN if link is
	 * going faster than the target speed.  Forward progress is
	 * guaranteed by setting sata_spd_limit to target_limit above.
	 */
	if (plink->sata_spd > target) {
		ata_dev_printk(dev, KERN_INFO,
			       "applying link speed limit horkage to %s\n",
			       sata_spd_string(target));
		return -EAGAIN;
	}
	return 0;
}

static inline u8 ata_dev_knobble(struct ata_device *dev)
{
	struct ata_port *ap = dev->link->ap;

	if (ata_dev_blacklisted(dev) & ATA_HORKAGE_BRIDGE_OK)
		return 0;

	return ((ap->cbl == ATA_CBL_SATA) && (!ata_id_is_sata(dev->id)));
}

static int ata_dev_config_ncq(struct ata_device *dev,
			       char *desc, size_t desc_sz)
{
	struct ata_port *ap = dev->link->ap;
	int hdepth = 0, ddepth = ata_id_queue_depth(dev->id);
	unsigned int err_mask;
	char *aa_desc = "";

	if (!ata_id_has_ncq(dev->id)) {
		desc[0] = '\0';
		return 0;
	}
	if (dev->horkage & ATA_HORKAGE_NONCQ) {
		snprintf(desc, desc_sz, "NCQ (not used)");
		return 0;
	}
	if (ap->flags & ATA_FLAG_NCQ) {
		hdepth = min(ap->scsi_host->can_queue, ATA_MAX_QUEUE - 1);
		dev->flags |= ATA_DFLAG_NCQ;
	}

	if (!(dev->horkage & ATA_HORKAGE_BROKEN_FPDMA_AA) &&
		(ap->flags & ATA_FLAG_FPDMA_AA) &&
		ata_id_has_fpdma_aa(dev->id)) {
		err_mask = ata_dev_set_feature(dev, SETFEATURES_SATA_ENABLE,
			SATA_FPDMA_AA);
		if (err_mask) {
			ata_dev_printk(dev, KERN_ERR, "failed to enable AA"
				"(error_mask=0x%x)\n", err_mask);
			if (err_mask != AC_ERR_DEV) {
				dev->horkage |= ATA_HORKAGE_BROKEN_FPDMA_AA;
				return -EIO;
			}
		} else
			aa_desc = ", AA";
	}

	if (hdepth >= ddepth)
		snprintf(desc, desc_sz, "NCQ (depth %d)%s", ddepth, aa_desc);
	else
		snprintf(desc, desc_sz, "NCQ (depth %d/%d)%s", hdepth,
			ddepth, aa_desc);
	return 0;
}

/**
 *	ata_dev_configure - Configure the specified ATA/ATAPI device
 *	@dev: Target device to configure
 *
 *	Configure @dev according to @dev->id.  Generic and low-level
 *	driver specific fixups are also applied.
 *
 *	LOCKING:
 *	Kernel thread context (may sleep)
 *
 *	RETURNS:
 *	0 on success, -errno otherwise
 */
int ata_dev_configure(struct ata_device *dev)
{
	struct ata_port *ap = dev->link->ap;
	struct ata_eh_context *ehc = &dev->link->eh_context;
	int print_info = ehc->i.flags & ATA_EHI_PRINTINFO;
	const u16 *id = dev->id;
	unsigned long xfer_mask;
	char revbuf[7];		/* XYZ-99\0 */
	char fwrevbuf[ATA_ID_FW_REV_LEN+1];
	char modelbuf[ATA_ID_PROD_LEN+1];
	int rc;

	if (!ata_dev_enabled(dev) && ata_msg_info(ap)) {
		ata_dev_printk(dev, KERN_INFO, "%s: ENTER/EXIT -- nodev\n",
			       __func__);
		return 0;
	}

	if (ata_msg_probe(ap))
		ata_dev_printk(dev, KERN_DEBUG, "%s: ENTER\n", __func__);

	/* set horkage */
	dev->horkage |= ata_dev_blacklisted(dev);
	ata_force_horkage(dev);

	if (dev->horkage & ATA_HORKAGE_DISABLE) {
		ata_dev_printk(dev, KERN_INFO,
			       "unsupported device, disabling\n");
		ata_dev_disable(dev);
		return 0;
	}

	if ((!atapi_enabled || (ap->flags & ATA_FLAG_NO_ATAPI)) &&
	    dev->class == ATA_DEV_ATAPI) {
		ata_dev_printk(dev, KERN_WARNING,
			"WARNING: ATAPI is %s, device ignored.\n",
			atapi_enabled ? "not supported with this driver"
				      : "disabled");
		ata_dev_disable(dev);
		return 0;
	}

	rc = ata_do_link_spd_horkage(dev);
	if (rc)
		return rc;

	/* let ACPI work its magic */
	rc = ata_acpi_on_devcfg(dev);
	if (rc)
		return rc;

	/* massage HPA, do it early as it might change IDENTIFY data */
	rc = ata_hpa_resize(dev);
	if (rc)
		return rc;

	/* print device capabilities */
	if (ata_msg_probe(ap))
		ata_dev_printk(dev, KERN_DEBUG,
			       "%s: cfg 49:%04x 82:%04x 83:%04x 84:%04x "
			       "85:%04x 86:%04x 87:%04x 88:%04x\n",
			       __func__,
			       id[49], id[82], id[83], id[84],
			       id[85], id[86], id[87], id[88]);

	/* initialize to-be-configured parameters */
	dev->flags &= ~ATA_DFLAG_CFG_MASK;
	dev->max_sectors = 0;
	dev->cdb_len = 0;
	dev->n_sectors = 0;
	dev->cylinders = 0;
	dev->heads = 0;
	dev->sectors = 0;
	dev->multi_count = 0;

	/*
	 * common ATA, ATAPI feature tests
	 */

	/* find max transfer mode; for printk only */
	xfer_mask = ata_id_xfermask(id);

	if (ata_msg_probe(ap))
		ata_dump_id(id);

	/* SCSI only uses 4-char revisions, dump full 8 chars from ATA */
	ata_id_c_string(dev->id, fwrevbuf, ATA_ID_FW_REV,
			sizeof(fwrevbuf));

	ata_id_c_string(dev->id, modelbuf, ATA_ID_PROD,
			sizeof(modelbuf));

	/* ATA-specific feature tests */
	if (dev->class == ATA_DEV_ATA) {
		if (ata_id_is_cfa(id)) {
			/* CPRM may make this media unusable */
			if (id[ATA_ID_CFA_KEY_MGMT] & 1)
				ata_dev_printk(dev, KERN_WARNING,
					       "supports DRM functions and may "
					       "not be fully accessable.\n");
			snprintf(revbuf, 7, "CFA");
		} else {
			snprintf(revbuf, 7, "ATA-%d", ata_id_major_version(id));
			/* Warn the user if the device has TPM extensions */
			if (ata_id_has_tpm(id))
				ata_dev_printk(dev, KERN_WARNING,
					       "supports DRM functions and may "
					       "not be fully accessable.\n");
		}

		dev->n_sectors = ata_id_n_sectors(id);

		/* get current R/W Multiple count setting */
		if ((dev->id[47] >> 8) == 0x80 && (dev->id[59] & 0x100)) {
			unsigned int max = dev->id[47] & 0xff;
			unsigned int cnt = dev->id[59] & 0xff;
			/* only recognize/allow powers of two here */
			if (is_power_of_2(max) && is_power_of_2(cnt))
				if (cnt <= max)
					dev->multi_count = cnt;
		}

		if (ata_id_has_lba(id)) {
			const char *lba_desc;
			char ncq_desc[24];

			lba_desc = "LBA";
			dev->flags |= ATA_DFLAG_LBA;
			if (ata_id_has_lba48(id)) {
				dev->flags |= ATA_DFLAG_LBA48;
				lba_desc = "LBA48";

				if (dev->n_sectors >= (1UL << 28) &&
				    ata_id_has_flush_ext(id))
					dev->flags |= ATA_DFLAG_FLUSH_EXT;
			}

			/* config NCQ */
			rc = ata_dev_config_ncq(dev, ncq_desc, sizeof(ncq_desc));
			if (rc)
				return rc;

			/* print device info to dmesg */
			if (ata_msg_drv(ap) && print_info) {
				ata_dev_printk(dev, KERN_INFO,
					"%s: %s, %s, max %s\n",
					revbuf, modelbuf, fwrevbuf,
					ata_mode_string(xfer_mask));
				ata_dev_printk(dev, KERN_INFO,
					"%Lu sectors, multi %u: %s %s\n",
					(unsigned long long)dev->n_sectors,
					dev->multi_count, lba_desc, ncq_desc);
			}
		} else {
			/* CHS */

			/* Default translation */
			dev->cylinders	= id[1];
			dev->heads	= id[3];
			dev->sectors	= id[6];

			if (ata_id_current_chs_valid(id)) {
				/* Current CHS translation is valid. */
				dev->cylinders = id[54];
				dev->heads     = id[55];
				dev->sectors   = id[56];
			}

			/* print device info to dmesg */
			if (ata_msg_drv(ap) && print_info) {
				ata_dev_printk(dev, KERN_INFO,
					"%s: %s, %s, max %s\n",
					revbuf,	modelbuf, fwrevbuf,
					ata_mode_string(xfer_mask));
				ata_dev_printk(dev, KERN_INFO,
					"%Lu sectors, multi %u, CHS %u/%u/%u\n",
					(unsigned long long)dev->n_sectors,
					dev->multi_count, dev->cylinders,
					dev->heads, dev->sectors);
			}
		}

		dev->cdb_len = 16;
	}

	/* ATAPI-specific feature tests */
	else if (dev->class == ATA_DEV_ATAPI) {
		const char *cdb_intr_string = "";
		const char *atapi_an_string = "";
		const char *dma_dir_string = "";
		u32 sntf;

		rc = atapi_cdb_len(id);
		if ((rc < 12) || (rc > ATAPI_CDB_LEN)) {
			if (ata_msg_warn(ap))
				ata_dev_printk(dev, KERN_WARNING,
					       "unsupported CDB len\n");
			rc = -EINVAL;
			goto err_out_nosup;
		}
		dev->cdb_len = (unsigned int) rc;

		/* Enable ATAPI AN if both the host and device have
		 * the support.  If PMP is attached, SNTF is required
		 * to enable ATAPI AN to discern between PHY status
		 * changed notifications and ATAPI ANs.
		 */
		if ((ap->flags & ATA_FLAG_AN) && ata_id_has_atapi_AN(id) &&
		    (!sata_pmp_attached(ap) ||
		     sata_scr_read(&ap->link, SCR_NOTIFICATION, &sntf) == 0)) {
			unsigned int err_mask;

			/* issue SET feature command to turn this on */
			err_mask = ata_dev_set_feature(dev,
					SETFEATURES_SATA_ENABLE, SATA_AN);
			if (err_mask)
				ata_dev_printk(dev, KERN_ERR,
					"failed to enable ATAPI AN "
					"(err_mask=0x%x)\n", err_mask);
			else {
				dev->flags |= ATA_DFLAG_AN;
				atapi_an_string = ", ATAPI AN";
			}
		}

		if (ata_id_cdb_intr(dev->id)) {
			dev->flags |= ATA_DFLAG_CDB_INTR;
			cdb_intr_string = ", CDB intr";
		}

		if (atapi_dmadir || atapi_id_dmadir(dev->id)) {
			dev->flags |= ATA_DFLAG_DMADIR;
			dma_dir_string = ", DMADIR";
		}

		/* print device info to dmesg */
		if (ata_msg_drv(ap) && print_info)
			ata_dev_printk(dev, KERN_INFO,
				       "ATAPI: %s, %s, max %s%s%s%s\n",
				       modelbuf, fwrevbuf,
				       ata_mode_string(xfer_mask),
				       cdb_intr_string, atapi_an_string,
				       dma_dir_string);
	}

	/* determine max_sectors */
	dev->max_sectors = ATA_MAX_SECTORS;
	if (dev->flags & ATA_DFLAG_LBA48)
		dev->max_sectors = ATA_MAX_SECTORS_LBA48;

	if (!(dev->horkage & ATA_HORKAGE_IPM)) {
		if (ata_id_has_hipm(dev->id))
			dev->flags |= ATA_DFLAG_HIPM;
		if (ata_id_has_dipm(dev->id))
			dev->flags |= ATA_DFLAG_DIPM;
	}

	/* Limit PATA drive on SATA cable bridge transfers to udma5,
	   200 sectors */
	if (ata_dev_knobble(dev)) {
		if (ata_msg_drv(ap) && print_info)
			ata_dev_printk(dev, KERN_INFO,
				       "applying bridge limits\n");
		dev->udma_mask &= ATA_UDMA5;
		dev->max_sectors = ATA_MAX_SECTORS;
	}

	if ((dev->class == ATA_DEV_ATAPI) &&
	    (atapi_command_packet_set(id) == TYPE_TAPE)) {
		dev->max_sectors = ATA_MAX_SECTORS_TAPE;
		dev->horkage |= ATA_HORKAGE_STUCK_ERR;
	}

	if (dev->horkage & ATA_HORKAGE_MAX_SEC_128)
		dev->max_sectors = min_t(unsigned int, ATA_MAX_SECTORS_128,
					 dev->max_sectors);

	if (ata_dev_blacklisted(dev) & ATA_HORKAGE_IPM) {
		dev->horkage |= ATA_HORKAGE_IPM;

		/* reset link pm_policy for this port to no pm */
		ap->pm_policy = MAX_PERFORMANCE;
	}

	if (ap->ops->dev_config)
		ap->ops->dev_config(dev);

	if (dev->horkage & ATA_HORKAGE_DIAGNOSTIC) {
		/* Let the user know. We don't want to disallow opens for
		   rescue purposes, or in case the vendor is just a blithering
		   idiot. Do this after the dev_config call as some controllers
		   with buggy firmware may want to avoid reporting false device
		   bugs */

		if (print_info) {
			ata_dev_printk(dev, KERN_WARNING,
"Drive reports diagnostics failure. This may indicate a drive\n");
			ata_dev_printk(dev, KERN_WARNING,
"fault or invalid emulation. Contact drive vendor for information.\n");
		}
	}

	if ((dev->horkage & ATA_HORKAGE_FIRMWARE_WARN) && print_info) {
		ata_dev_printk(dev, KERN_WARNING, "WARNING: device requires "
			       "firmware update to be fully functional.\n");
		ata_dev_printk(dev, KERN_WARNING, "         contact the vendor "
			       "or visit http://ata.wiki.kernel.org.\n");
	}

	return 0;

err_out_nosup:
	if (ata_msg_probe(ap))
		ata_dev_printk(dev, KERN_DEBUG,
			       "%s: EXIT, err\n", __func__);
	return rc;
}

/**
 *	ata_cable_40wire	-	return 40 wire cable type
 *	@ap: port
 *
 *	Helper method for drivers which want to hardwire 40 wire cable
 *	detection.
 */

int ata_cable_40wire(struct ata_port *ap)
{
	return ATA_CBL_PATA40;
}

/**
 *	ata_cable_80wire	-	return 80 wire cable type
 *	@ap: port
 *
 *	Helper method for drivers which want to hardwire 80 wire cable
 *	detection.
 */

int ata_cable_80wire(struct ata_port *ap)
{
	return ATA_CBL_PATA80;
}

/**
 *	ata_cable_unknown	-	return unknown PATA cable.
 *	@ap: port
 *
 *	Helper method for drivers which have no PATA cable detection.
 */

int ata_cable_unknown(struct ata_port *ap)
{
	return ATA_CBL_PATA_UNK;
}

/**
 *	ata_cable_ignore	-	return ignored PATA cable.
 *	@ap: port
 *
 *	Helper method for drivers which don't use cable type to limit
 *	transfer mode.
 */
int ata_cable_ignore(struct ata_port *ap)
{
	return ATA_CBL_PATA_IGN;
}

/**
 *	ata_cable_sata	-	return SATA cable type
 *	@ap: port
 *
 *	Helper method for drivers which have SATA cables
 */

int ata_cable_sata(struct ata_port *ap)
{
	return ATA_CBL_SATA;
}

/**
 *	ata_bus_probe - Reset and probe ATA bus
 *	@ap: Bus to probe
 *
 *	Master ATA bus probing function.  Initiates a hardware-dependent
 *	bus reset, then attempts to identify any devices found on
 *	the bus.
 *
 *	LOCKING:
 *	PCI/etc. bus probe sem.
 *
 *	RETURNS:
 *	Zero on success, negative errno otherwise.
 */

int ata_bus_probe(struct ata_port *ap)
{
	unsigned int classes[ATA_MAX_DEVICES];
	int tries[ATA_MAX_DEVICES];
	int rc;
	struct ata_device *dev;

	ata_port_probe(ap);

	ata_for_each_dev(dev, &ap->link, ALL)
		tries[dev->devno] = ATA_PROBE_MAX_TRIES;

 retry:
	ata_for_each_dev(dev, &ap->link, ALL) {
		/* If we issue an SRST then an ATA drive (not ATAPI)
		 * may change configuration and be in PIO0 timing. If
		 * we do a hard reset (or are coming from power on)
		 * this is true for ATA or ATAPI. Until we've set a
		 * suitable controller mode we should not touch the
		 * bus as we may be talking too fast.
		 */
		dev->pio_mode = XFER_PIO_0;

		/* If the controller has a pio mode setup function
		 * then use it to set the chipset to rights. Don't
		 * touch the DMA setup as that will be dealt with when
		 * configuring devices.
		 */
		if (ap->ops->set_piomode)
			ap->ops->set_piomode(ap, dev);
	}

	/* reset and determine device classes */
	ap->ops->phy_reset(ap);

	ata_for_each_dev(dev, &ap->link, ALL) {
		if (!(ap->flags & ATA_FLAG_DISABLED) &&
		    dev->class != ATA_DEV_UNKNOWN)
			classes[dev->devno] = dev->class;
		else
			classes[dev->devno] = ATA_DEV_NONE;

		dev->class = ATA_DEV_UNKNOWN;
	}

	ata_port_probe(ap);

	/* read IDENTIFY page and configure devices. We have to do the identify
	   specific sequence bass-ackwards so that PDIAG- is released by
	   the slave device */

	ata_for_each_dev(dev, &ap->link, ALL_REVERSE) {
		if (tries[dev->devno])
			dev->class = classes[dev->devno];

		if (!ata_dev_enabled(dev))
			continue;

		rc = ata_dev_read_id(dev, &dev->class, ATA_READID_POSTRESET,
				     dev->id);
		if (rc)
			goto fail;
	}

	/* Now ask for the cable type as PDIAG- should have been released */
	if (ap->ops->cable_detect)
		ap->cbl = ap->ops->cable_detect(ap);

	/* We may have SATA bridge glue hiding here irrespective of
	 * the reported cable types and sensed types.  When SATA
	 * drives indicate we have a bridge, we don't know which end
	 * of the link the bridge is which is a problem.
	 */
	ata_for_each_dev(dev, &ap->link, ENABLED)
		if (ata_id_is_sata(dev->id))
			ap->cbl = ATA_CBL_SATA;

	/* After the identify sequence we can now set up the devices. We do
	   this in the normal order so that the user doesn't get confused */

	ata_for_each_dev(dev, &ap->link, ENABLED) {
		ap->link.eh_context.i.flags |= ATA_EHI_PRINTINFO;
		rc = ata_dev_configure(dev);
		ap->link.eh_context.i.flags &= ~ATA_EHI_PRINTINFO;
		if (rc)
			goto fail;
	}

	/* configure transfer mode */
	rc = ata_set_mode(&ap->link, &dev);
	if (rc)
		goto fail;

	ata_for_each_dev(dev, &ap->link, ENABLED)
		return 0;

	/* no device present, disable port */
	ata_port_disable(ap);
	return -ENODEV;

 fail:
	tries[dev->devno]--;

	switch (rc) {
	case -EINVAL:
		/* eeek, something went very wrong, give up */
		tries[dev->devno] = 0;
		break;

	case -ENODEV:
		/* give it just one more chance */
		tries[dev->devno] = min(tries[dev->devno], 1);
	case -EIO:
		if (tries[dev->devno] == 1) {
			/* This is the last chance, better to slow
			 * down than lose it.
			 */
			sata_down_spd_limit(&ap->link, 0);
			ata_down_xfermask_limit(dev, ATA_DNXFER_PIO);
		}
	}

	if (!tries[dev->devno])
		ata_dev_disable(dev);

	goto retry;
}

/**
 *	ata_port_probe - Mark port as enabled
 *	@ap: Port for which we indicate enablement
 *
 *	Modify @ap data structure such that the system
 *	thinks that the entire port is enabled.
 *
 *	LOCKING: host lock, or some other form of
 *	serialization.
 */

void ata_port_probe(struct ata_port *ap)
{
	ap->flags &= ~ATA_FLAG_DISABLED;
}

/**
 *	sata_print_link_status - Print SATA link status
 *	@link: SATA link to printk link status about
 *
 *	This function prints link speed and status of a SATA link.
 *
 *	LOCKING:
 *	None.
 */
static void sata_print_link_status(struct ata_link *link)
{
	u32 sstatus, scontrol, tmp;

	if (sata_scr_read(link, SCR_STATUS, &sstatus))
		return;
	sata_scr_read(link, SCR_CONTROL, &scontrol);

	if (ata_phys_link_online(link)) {
		tmp = (sstatus >> 4) & 0xf;
		ata_link_printk(link, KERN_INFO,
				"SATA link up %s (SStatus %X SControl %X)\n",
				sata_spd_string(tmp), sstatus, scontrol);
	} else {
		ata_link_printk(link, KERN_INFO,
				"SATA link down (SStatus %X SControl %X)\n",
				sstatus, scontrol);
	}
}

/**
 *	ata_dev_pair		-	return other device on cable
 *	@adev: device
 *
 *	Obtain the other device on the same cable, or if none is
 *	present NULL is returned
 */

struct ata_device *ata_dev_pair(struct ata_device *adev)
{
	struct ata_link *link = adev->link;
	struct ata_device *pair = &link->device[1 - adev->devno];
	if (!ata_dev_enabled(pair))
		return NULL;
	return pair;
}

/**
 *	ata_port_disable - Disable port.
 *	@ap: Port to be disabled.
 *
 *	Modify @ap data structure such that the system
 *	thinks that the entire port is disabled, and should
 *	never attempt to probe or communicate with devices
 *	on this port.
 *
 *	LOCKING: host lock, or some other form of
 *	serialization.
 */

void ata_port_disable(struct ata_port *ap)
{
	ap->link.device[0].class = ATA_DEV_NONE;
	ap->link.device[1].class = ATA_DEV_NONE;
	ap->flags |= ATA_FLAG_DISABLED;
}

/**
 *	sata_down_spd_limit - adjust SATA spd limit downward
 *	@link: Link to adjust SATA spd limit for
 *	@spd_limit: Additional limit
 *
 *	Adjust SATA spd limit of @link downward.  Note that this
 *	function only adjusts the limit.  The change must be applied
 *	using sata_set_spd().
 *
 *	If @spd_limit is non-zero, the speed is limited to equal to or
 *	lower than @spd_limit if such speed is supported.  If
 *	@spd_limit is slower than any supported speed, only the lowest
 *	supported speed is allowed.
 *
 *	LOCKING:
 *	Inherited from caller.
 *
 *	RETURNS:
 *	0 on success, negative errno on failure
 */
int sata_down_spd_limit(struct ata_link *link, u32 spd_limit)
{
	u32 sstatus, spd, mask;
	int rc, bit;

	if (!sata_scr_valid(link))
		return -EOPNOTSUPP;

	/* If SCR can be read, use it to determine the current SPD.
	 * If not, use cached value in link->sata_spd.
	 */
	rc = sata_scr_read(link, SCR_STATUS, &sstatus);
	if (rc == 0 && ata_sstatus_online(sstatus))
		spd = (sstatus >> 4) & 0xf;
	else
		spd = link->sata_spd;

	mask = link->sata_spd_limit;
	if (mask <= 1)
		return -EINVAL;

	/* unconditionally mask off the highest bit */
	bit = fls(mask) - 1;
	mask &= ~(1 << bit);

	/* Mask off all speeds higher than or equal to the current
	 * one.  Force 1.5Gbps if current SPD is not available.
	 */
	if (spd > 1)
		mask &= (1 << (spd - 1)) - 1;
	else
		mask &= 1;

	/* were we already at the bottom? */
	if (!mask)
		return -EINVAL;

	if (spd_limit) {
		if (mask & ((1 << spd_limit) - 1))
			mask &= (1 << spd_limit) - 1;
		else {
			bit = ffs(mask) - 1;
			mask = 1 << bit;
		}
	}

	link->sata_spd_limit = mask;

	ata_link_printk(link, KERN_WARNING, "limiting SATA link speed to %s\n",
			sata_spd_string(fls(mask)));

	return 0;
}

static int __sata_set_spd_needed(struct ata_link *link, u32 *scontrol)
{
	struct ata_link *host_link = &link->ap->link;
	u32 limit, target, spd;

	limit = link->sata_spd_limit;

	/* Don't configure downstream link faster than upstream link.
	 * It doesn't speed up anything and some PMPs choke on such
	 * configuration.
	 */
	if (!ata_is_host_link(link) && host_link->sata_spd)
		limit &= (1 << host_link->sata_spd) - 1;

	if (limit == UINT_MAX)
		target = 0;
	else
		target = fls(limit);

	spd = (*scontrol >> 4) & 0xf;
	*scontrol = (*scontrol & ~0xf0) | ((target & 0xf) << 4);

	return spd != target;
}

/**
 *	sata_set_spd_needed - is SATA spd configuration needed
 *	@link: Link in question
 *
 *	Test whether the spd limit in SControl matches
 *	@link->sata_spd_limit.  This function is used to determine
 *	whether hardreset is necessary to apply SATA spd
 *	configuration.
 *
 *	LOCKING:
 *	Inherited from caller.
 *
 *	RETURNS:
 *	1 if SATA spd configuration is needed, 0 otherwise.
 */
static int sata_set_spd_needed(struct ata_link *link)
{
	u32 scontrol;

	if (sata_scr_read(link, SCR_CONTROL, &scontrol))
		return 1;

	return __sata_set_spd_needed(link, &scontrol);
}

/**
 *	sata_set_spd - set SATA spd according to spd limit
 *	@link: Link to set SATA spd for
 *
 *	Set SATA spd of @link according to sata_spd_limit.
 *
 *	LOCKING:
 *	Inherited from caller.
 *
 *	RETURNS:
 *	0 if spd doesn't need to be changed, 1 if spd has been
 *	changed.  Negative errno if SCR registers are inaccessible.
 */
int sata_set_spd(struct ata_link *link)
{
	u32 scontrol;
	int rc;

	if ((rc = sata_scr_read(link, SCR_CONTROL, &scontrol)))
		return rc;

	if (!__sata_set_spd_needed(link, &scontrol))
		return 0;

	if ((rc = sata_scr_write(link, SCR_CONTROL, scontrol)))
		return rc;

	return 1;
}

/*
 * This mode timing computation functionality is ported over from
 * drivers/ide/ide-timing.h and was originally written by Vojtech Pavlik
 */
/*
 * PIO 0-4, MWDMA 0-2 and UDMA 0-6 timings (in nanoseconds).
 * These were taken from ATA/ATAPI-6 standard, rev 0a, except
 * for UDMA6, which is currently supported only by Maxtor drives.
 *
 * For PIO 5/6 MWDMA 3/4 see the CFA specification 3.0.
 */

static const struct ata_timing ata_timing[] = {
/*	{ XFER_PIO_SLOW, 120, 290, 240, 960, 290, 240, 0,  960,   0 }, */
	{ XFER_PIO_0,     70, 290, 240, 600, 165, 150, 0,  600,   0 },
	{ XFER_PIO_1,     50, 290,  93, 383, 125, 100, 0,  383,   0 },
	{ XFER_PIO_2,     30, 290,  40, 330, 100,  90, 0,  240,   0 },
	{ XFER_PIO_3,     30,  80,  70, 180,  80,  70, 0,  180,   0 },
	{ XFER_PIO_4,     25,  70,  25, 120,  70,  25, 0,  120,   0 },
	{ XFER_PIO_5,     15,  65,  25, 100,  65,  25, 0,  100,   0 },
	{ XFER_PIO_6,     10,  55,  20,  80,  55,  20, 0,   80,   0 },

	{ XFER_SW_DMA_0, 120,   0,   0,   0, 480, 480, 50, 960,   0 },
	{ XFER_SW_DMA_1,  90,   0,   0,   0, 240, 240, 30, 480,   0 },
	{ XFER_SW_DMA_2,  60,   0,   0,   0, 120, 120, 20, 240,   0 },

	{ XFER_MW_DMA_0,  60,   0,   0,   0, 215, 215, 20, 480,   0 },
	{ XFER_MW_DMA_1,  45,   0,   0,   0,  80,  50, 5,  150,   0 },
	{ XFER_MW_DMA_2,  25,   0,   0,   0,  70,  25, 5,  120,   0 },
	{ XFER_MW_DMA_3,  25,   0,   0,   0,  65,  25, 5,  100,   0 },
	{ XFER_MW_DMA_4,  25,   0,   0,   0,  55,  20, 5,   80,   0 },

/*	{ XFER_UDMA_SLOW,  0,   0,   0,   0,   0,   0, 0,    0, 150 }, */
	{ XFER_UDMA_0,     0,   0,   0,   0,   0,   0, 0,    0, 120 },
	{ XFER_UDMA_1,     0,   0,   0,   0,   0,   0, 0,    0,  80 },
	{ XFER_UDMA_2,     0,   0,   0,   0,   0,   0, 0,    0,  60 },
	{ XFER_UDMA_3,     0,   0,   0,   0,   0,   0, 0,    0,  45 },
	{ XFER_UDMA_4,     0,   0,   0,   0,   0,   0, 0,    0,  30 },
	{ XFER_UDMA_5,     0,   0,   0,   0,   0,   0, 0,    0,  20 },
	{ XFER_UDMA_6,     0,   0,   0,   0,   0,   0, 0,    0,  15 },

	{ 0xFF }
};

#define ENOUGH(v, unit)		(((v)-1)/(unit)+1)
#define EZ(v, unit)		((v)?ENOUGH(v, unit):0)

static void ata_timing_quantize(const struct ata_timing *t, struct ata_timing *q, int T, int UT)
{
	q->setup	= EZ(t->setup      * 1000,  T);
	q->act8b	= EZ(t->act8b      * 1000,  T);
	q->rec8b	= EZ(t->rec8b      * 1000,  T);
	q->cyc8b	= EZ(t->cyc8b      * 1000,  T);
	q->active	= EZ(t->active     * 1000,  T);
	q->recover	= EZ(t->recover    * 1000,  T);
	q->dmack_hold	= EZ(t->dmack_hold * 1000,  T);
	q->cycle	= EZ(t->cycle      * 1000,  T);
	q->udma		= EZ(t->udma       * 1000, UT);
}

void ata_timing_merge(const struct ata_timing *a, const struct ata_timing *b,
		      struct ata_timing *m, unsigned int what)
{
	if (what & ATA_TIMING_SETUP  ) m->setup   = max(a->setup,   b->setup);
	if (what & ATA_TIMING_ACT8B  ) m->act8b   = max(a->act8b,   b->act8b);
	if (what & ATA_TIMING_REC8B  ) m->rec8b   = max(a->rec8b,   b->rec8b);
	if (what & ATA_TIMING_CYC8B  ) m->cyc8b   = max(a->cyc8b,   b->cyc8b);
	if (what & ATA_TIMING_ACTIVE ) m->active  = max(a->active,  b->active);
	if (what & ATA_TIMING_RECOVER) m->recover = max(a->recover, b->recover);
	if (what & ATA_TIMING_DMACK_HOLD) m->dmack_hold = max(a->dmack_hold, b->dmack_hold);
	if (what & ATA_TIMING_CYCLE  ) m->cycle   = max(a->cycle,   b->cycle);
	if (what & ATA_TIMING_UDMA   ) m->udma    = max(a->udma,    b->udma);
}

const struct ata_timing *ata_timing_find_mode(u8 xfer_mode)
{
	const struct ata_timing *t = ata_timing;

	while (xfer_mode > t->mode)
		t++;

	if (xfer_mode == t->mode)
		return t;
	return NULL;
}

int ata_timing_compute(struct ata_device *adev, unsigned short speed,
		       struct ata_timing *t, int T, int UT)
{
	const struct ata_timing *s;
	struct ata_timing p;

	/*
	 * Find the mode.
	 */

	if (!(s = ata_timing_find_mode(speed)))
		return -EINVAL;

	memcpy(t, s, sizeof(*s));

	/*
	 * If the drive is an EIDE drive, it can tell us it needs extended
	 * PIO/MW_DMA cycle timing.
	 */

	if (adev->id[ATA_ID_FIELD_VALID] & 2) {	/* EIDE drive */
		memset(&p, 0, sizeof(p));
		if (speed >= XFER_PIO_0 && speed <= XFER_SW_DMA_0) {
			if (speed <= XFER_PIO_2) p.cycle = p.cyc8b = adev->id[ATA_ID_EIDE_PIO];
					    else p.cycle = p.cyc8b = adev->id[ATA_ID_EIDE_PIO_IORDY];
		} else if (speed >= XFER_MW_DMA_0 && speed <= XFER_MW_DMA_2) {
			p.cycle = adev->id[ATA_ID_EIDE_DMA_MIN];
		}
		ata_timing_merge(&p, t, t, ATA_TIMING_CYCLE | ATA_TIMING_CYC8B);
	}

	/*
	 * Convert the timing to bus clock counts.
	 */

	ata_timing_quantize(t, t, T, UT);

	/*
	 * Even in DMA/UDMA modes we still use PIO access for IDENTIFY,
	 * S.M.A.R.T * and some other commands. We have to ensure that the
	 * DMA cycle timing is slower/equal than the fastest PIO timing.
	 */

	if (speed > XFER_PIO_6) {
		ata_timing_compute(adev, adev->pio_mode, &p, T, UT);
		ata_timing_merge(&p, t, t, ATA_TIMING_ALL);
	}

	/*
	 * Lengthen active & recovery time so that cycle time is correct.
	 */

	if (t->act8b + t->rec8b < t->cyc8b) {
		t->act8b += (t->cyc8b - (t->act8b + t->rec8b)) / 2;
		t->rec8b = t->cyc8b - t->act8b;
	}

	if (t->active + t->recover < t->cycle) {
		t->active += (t->cycle - (t->active + t->recover)) / 2;
		t->recover = t->cycle - t->active;
	}

	/* In a few cases quantisation may produce enough errors to
	   leave t->cycle too low for the sum of active and recovery
	   if so we must correct this */
	if (t->active + t->recover > t->cycle)
		t->cycle = t->active + t->recover;

	return 0;
}

/**
 *	ata_timing_cycle2mode - find xfer mode for the specified cycle duration
 *	@xfer_shift: ATA_SHIFT_* value for transfer type to examine.
 *	@cycle: cycle duration in ns
 *
 *	Return matching xfer mode for @cycle.  The returned mode is of
 *	the transfer type specified by @xfer_shift.  If @cycle is too
 *	slow for @xfer_shift, 0xff is returned.  If @cycle is faster
 *	than the fastest known mode, the fasted mode is returned.
 *
 *	LOCKING:
 *	None.
 *
 *	RETURNS:
 *	Matching xfer_mode, 0xff if no match found.
 */
u8 ata_timing_cycle2mode(unsigned int xfer_shift, int cycle)
{
	u8 base_mode = 0xff, last_mode = 0xff;
	const struct ata_xfer_ent *ent;
	const struct ata_timing *t;

	for (ent = ata_xfer_tbl; ent->shift >= 0; ent++)
		if (ent->shift == xfer_shift)
			base_mode = ent->base;

	for (t = ata_timing_find_mode(base_mode);
	     t && ata_xfer_mode2shift(t->mode) == xfer_shift; t++) {
		unsigned short this_cycle;

		switch (xfer_shift) {
		case ATA_SHIFT_PIO:
		case ATA_SHIFT_MWDMA:
			this_cycle = t->cycle;
			break;
		case ATA_SHIFT_UDMA:
			this_cycle = t->udma;
			break;
		default:
			return 0xff;
		}

		if (cycle > this_cycle)
			break;

		last_mode = t->mode;
	}

	return last_mode;
}

/**
 *	ata_down_xfermask_limit - adjust dev xfer masks downward
 *	@dev: Device to adjust xfer masks
 *	@sel: ATA_DNXFER_* selector
 *
 *	Adjust xfer masks of @dev downward.  Note that this function
 *	does not apply the change.  Invoking ata_set_mode() afterwards
 *	will apply the limit.
 *
 *	LOCKING:
 *	Inherited from caller.
 *
 *	RETURNS:
 *	0 on success, negative errno on failure
 */
int ata_down_xfermask_limit(struct ata_device *dev, unsigned int sel)
{
	char buf[32];
	unsigned long orig_mask, xfer_mask;
	unsigned long pio_mask, mwdma_mask, udma_mask;
	int quiet, highbit;

	quiet = !!(sel & ATA_DNXFER_QUIET);
	sel &= ~ATA_DNXFER_QUIET;

	xfer_mask = orig_mask = ata_pack_xfermask(dev->pio_mask,
						  dev->mwdma_mask,
						  dev->udma_mask);
	ata_unpack_xfermask(xfer_mask, &pio_mask, &mwdma_mask, &udma_mask);

	switch (sel) {
	case ATA_DNXFER_PIO:
		highbit = fls(pio_mask) - 1;
		pio_mask &= ~(1 << highbit);
		break;

	case ATA_DNXFER_DMA:
		if (udma_mask) {
			highbit = fls(udma_mask) - 1;
			udma_mask &= ~(1 << highbit);
			if (!udma_mask)
				return -ENOENT;
		} else if (mwdma_mask) {
			highbit = fls(mwdma_mask) - 1;
			mwdma_mask &= ~(1 << highbit);
			if (!mwdma_mask)
				return -ENOENT;
		}
		break;

	case ATA_DNXFER_40C:
		udma_mask &= ATA_UDMA_MASK_40C;
		break;

	case ATA_DNXFER_FORCE_PIO0:
		pio_mask &= 1;
	case ATA_DNXFER_FORCE_PIO:
		mwdma_mask = 0;
		udma_mask = 0;
		break;

	default:
		BUG();
	}

	xfer_mask &= ata_pack_xfermask(pio_mask, mwdma_mask, udma_mask);

	if (!(xfer_mask & ATA_MASK_PIO) || xfer_mask == orig_mask)
		return -ENOENT;

	if (!quiet) {
		if (xfer_mask & (ATA_MASK_MWDMA | ATA_MASK_UDMA))
			snprintf(buf, sizeof(buf), "%s:%s",
				 ata_mode_string(xfer_mask),
				 ata_mode_string(xfer_mask & ATA_MASK_PIO));
		else
			snprintf(buf, sizeof(buf), "%s",
				 ata_mode_string(xfer_mask));

		ata_dev_printk(dev, KERN_WARNING,
			       "limiting speed to %s\n", buf);
	}

	ata_unpack_xfermask(xfer_mask, &dev->pio_mask, &dev->mwdma_mask,
			    &dev->udma_mask);

	return 0;
}

static int ata_dev_set_mode(struct ata_device *dev)
{
	struct ata_port *ap = dev->link->ap;
	struct ata_eh_context *ehc = &dev->link->eh_context;
	const bool nosetxfer = dev->horkage & ATA_HORKAGE_NOSETXFER;
	const char *dev_err_whine = "";
	int ign_dev_err = 0;
	unsigned int err_mask = 0;
	int rc;

	dev->flags &= ~ATA_DFLAG_PIO;
	if (dev->xfer_shift == ATA_SHIFT_PIO)
		dev->flags |= ATA_DFLAG_PIO;

	if (nosetxfer && ap->flags & ATA_FLAG_SATA && ata_id_is_sata(dev->id))
		dev_err_whine = " (SET_XFERMODE skipped)";
	else {
		if (nosetxfer)
			ata_dev_printk(dev, KERN_WARNING,
				       "NOSETXFER but PATA detected - can't "
				       "skip SETXFER, might malfunction\n");
		err_mask = ata_dev_set_xfermode(dev);
	}

	if (err_mask & ~AC_ERR_DEV)
		goto fail;

	/* revalidate */
	ehc->i.flags |= ATA_EHI_POST_SETMODE;
	rc = ata_dev_revalidate(dev, ATA_DEV_UNKNOWN, 0);
	ehc->i.flags &= ~ATA_EHI_POST_SETMODE;
	if (rc)
		return rc;

	if (dev->xfer_shift == ATA_SHIFT_PIO) {
		/* Old CFA may refuse this command, which is just fine */
		if (ata_id_is_cfa(dev->id))
			ign_dev_err = 1;
		/* Catch several broken garbage emulations plus some pre
		   ATA devices */
		if (ata_id_major_version(dev->id) == 0 &&
					dev->pio_mode <= XFER_PIO_2)
			ign_dev_err = 1;
		/* Some very old devices and some bad newer ones fail
		   any kind of SET_XFERMODE request but support PIO0-2
		   timings and no IORDY */
		if (!ata_id_has_iordy(dev->id) && dev->pio_mode <= XFER_PIO_2)
			ign_dev_err = 1;
	}
	/* Early MWDMA devices do DMA but don't allow DMA mode setting.
	   Don't fail an MWDMA0 set IFF the device indicates it is in MWDMA0 */
	if (dev->xfer_shift == ATA_SHIFT_MWDMA &&
	    dev->dma_mode == XFER_MW_DMA_0 &&
	    (dev->id[63] >> 8) & 1)
		ign_dev_err = 1;

	/* if the device is actually configured correctly, ignore dev err */
	if (dev->xfer_mode == ata_xfer_mask2mode(ata_id_xfermask(dev->id)))
		ign_dev_err = 1;

	if (err_mask & AC_ERR_DEV) {
		if (!ign_dev_err)
			goto fail;
		else
			dev_err_whine = " (device error ignored)";
	}

	DPRINTK("xfer_shift=%u, xfer_mode=0x%x\n",
		dev->xfer_shift, (int)dev->xfer_mode);

	ata_dev_printk(dev, KERN_INFO, "configured for %s%s\n",
		       ata_mode_string(ata_xfer_mode2mask(dev->xfer_mode)),
		       dev_err_whine);

	return 0;

 fail:
	ata_dev_printk(dev, KERN_ERR, "failed to set xfermode "
		       "(err_mask=0x%x)\n", err_mask);
	return -EIO;
}

/**
 *	ata_do_set_mode - Program timings and issue SET FEATURES - XFER
 *	@link: link on which timings will be programmed
 *	@r_failed_dev: out parameter for failed device
 *
 *	Standard implementation of the function used to tune and set
 *	ATA device disk transfer mode (PIO3, UDMA6, etc.).  If
 *	ata_dev_set_mode() fails, pointer to the failing device is
 *	returned in @r_failed_dev.
 *
 *	LOCKING:
 *	PCI/etc. bus probe sem.
 *
 *	RETURNS:
 *	0 on success, negative errno otherwise
 */

int ata_do_set_mode(struct ata_link *link, struct ata_device **r_failed_dev)
{
	struct ata_port *ap = link->ap;
	struct ata_device *dev;
	int rc = 0, used_dma = 0, found = 0;

	/* step 1: calculate xfer_mask */
	ata_for_each_dev(dev, link, ENABLED) {
		unsigned long pio_mask, dma_mask;
		unsigned int mode_mask;

		mode_mask = ATA_DMA_MASK_ATA;
		if (dev->class == ATA_DEV_ATAPI)
			mode_mask = ATA_DMA_MASK_ATAPI;
		else if (ata_id_is_cfa(dev->id))
			mode_mask = ATA_DMA_MASK_CFA;

		ata_dev_xfermask(dev);
		ata_force_xfermask(dev);

		pio_mask = ata_pack_xfermask(dev->pio_mask, 0, 0);
		dma_mask = ata_pack_xfermask(0, dev->mwdma_mask, dev->udma_mask);

		if (libata_dma_mask & mode_mask)
			dma_mask = ata_pack_xfermask(0, dev->mwdma_mask, dev->udma_mask);
		else
			dma_mask = 0;

		dev->pio_mode = ata_xfer_mask2mode(pio_mask);
		dev->dma_mode = ata_xfer_mask2mode(dma_mask);

		found = 1;
		if (ata_dma_enabled(dev))
			used_dma = 1;
	}
	if (!found)
		goto out;

	/* step 2: always set host PIO timings */
	ata_for_each_dev(dev, link, ENABLED) {
		if (dev->pio_mode == 0xff) {
			ata_dev_printk(dev, KERN_WARNING, "no PIO support\n");
			rc = -EINVAL;
			goto out;
		}

		dev->xfer_mode = dev->pio_mode;
		dev->xfer_shift = ATA_SHIFT_PIO;
		if (ap->ops->set_piomode)
			ap->ops->set_piomode(ap, dev);
	}

	/* step 3: set host DMA timings */
	ata_for_each_dev(dev, link, ENABLED) {
		if (!ata_dma_enabled(dev))
			continue;

		dev->xfer_mode = dev->dma_mode;
		dev->xfer_shift = ata_xfer_mode2shift(dev->dma_mode);
		if (ap->ops->set_dmamode)
			ap->ops->set_dmamode(ap, dev);
	}

	/* step 4: update devices' xfer mode */
	ata_for_each_dev(dev, link, ENABLED) {
		rc = ata_dev_set_mode(dev);
		if (rc)
			goto out;
	}

	/* Record simplex status. If we selected DMA then the other
	 * host channels are not permitted to do so.
	 */
	if (used_dma && (ap->host->flags & ATA_HOST_SIMPLEX))
		ap->host->simplex_claimed = ap;

 out:
	if (rc)
		*r_failed_dev = dev;
	return rc;
}

/**
 *	ata_wait_ready - wait for link to become ready
 *	@link: link to be waited on
 *	@deadline: deadline jiffies for the operation
 *	@check_ready: callback to check link readiness
 *
 *	Wait for @link to become ready.  @check_ready should return
 *	positive number if @link is ready, 0 if it isn't, -ENODEV if
 *	link doesn't seem to be occupied, other errno for other error
 *	conditions.
 *
 *	Transient -ENODEV conditions are allowed for
 *	ATA_TMOUT_FF_WAIT.
 *
 *	LOCKING:
 *	EH context.
 *
 *	RETURNS:
 *	0 if @linke is ready before @deadline; otherwise, -errno.
 */
int ata_wait_ready(struct ata_link *link, unsigned long deadline,
		   int (*check_ready)(struct ata_link *link))
{
	unsigned long start = jiffies;
	unsigned long nodev_deadline = ata_deadline(start, ATA_TMOUT_FF_WAIT);
	int warned = 0;

	/* Slave readiness can't be tested separately from master.  On
	 * M/S emulation configuration, this function should be called
	 * only on the master and it will handle both master and slave.
	 */
	WARN_ON(link == link->ap->slave_link);

	if (time_after(nodev_deadline, deadline))
		nodev_deadline = deadline;

	while (1) {
		unsigned long now = jiffies;
		int ready, tmp;

		ready = tmp = check_ready(link);
		if (ready > 0)
			return 0;

		/* -ENODEV could be transient.  Ignore -ENODEV if link
		 * is online.  Also, some SATA devices take a long
		 * time to clear 0xff after reset.  For example,
		 * HHD424020F7SV00 iVDR needs >= 800ms while Quantum
		 * GoVault needs even more than that.  Wait for
		 * ATA_TMOUT_FF_WAIT on -ENODEV if link isn't offline.
		 *
		 * Note that some PATA controllers (pata_ali) explode
		 * if status register is read more than once when
		 * there's no device attached.
		 */
		if (ready == -ENODEV) {
			if (ata_link_online(link))
				ready = 0;
			else if ((link->ap->flags & ATA_FLAG_SATA) &&
				 !ata_link_offline(link) &&
				 time_before(now, nodev_deadline))
				ready = 0;
		}

		if (ready)
			return ready;
		if (time_after(now, deadline))
			return -EBUSY;

		if (!warned && time_after(now, start + 5 * HZ) &&
		    (deadline - now > 3 * HZ)) {
			ata_link_printk(link, KERN_WARNING,
				"link is slow to respond, please be patient "
				"(ready=%d)\n", tmp);
			warned = 1;
		}

		msleep(50);
	}
}

/**
 *	ata_wait_after_reset - wait for link to become ready after reset
 *	@link: link to be waited on
 *	@deadline: deadline jiffies for the operation
 *	@check_ready: callback to check link readiness
 *
 *	Wait for @link to become ready after reset.
 *
 *	LOCKING:
 *	EH context.
 *
 *	RETURNS:
 *	0 if @linke is ready before @deadline; otherwise, -errno.
 */
int ata_wait_after_reset(struct ata_link *link, unsigned long deadline,
				int (*check_ready)(struct ata_link *link))
{
	msleep(ATA_WAIT_AFTER_RESET);

	return ata_wait_ready(link, deadline, check_ready);
}

/**
 *	sata_link_debounce - debounce SATA phy status
 *	@link: ATA link to debounce SATA phy status for
 *	@params: timing parameters { interval, duratinon, timeout } in msec
 *	@deadline: deadline jiffies for the operation
 *
*	Make sure SStatus of @link reaches stable state, determined by
 *	holding the same value where DET is not 1 for @duration polled
 *	every @interval, before @timeout.  Timeout constraints the
 *	beginning of the stable state.  Because DET gets stuck at 1 on
 *	some controllers after hot unplugging, this functions waits
 *	until timeout then returns 0 if DET is stable at 1.
 *
 *	@timeout is further limited by @deadline.  The sooner of the
 *	two is used.
 *
 *	LOCKING:
 *	Kernel thread context (may sleep)
 *
 *	RETURNS:
 *	0 on success, -errno on failure.
 */
int sata_link_debounce(struct ata_link *link, const unsigned long *params,
		       unsigned long deadline)
{
	unsigned long interval = params[0];
	unsigned long duration = params[1];
	unsigned long last_jiffies, t;
	u32 last, cur;
	int rc;

	t = ata_deadline(jiffies, params[2]);
	if (time_before(t, deadline))
		deadline = t;

	if ((rc = sata_scr_read(link, SCR_STATUS, &cur)))
		return rc;
	cur &= 0xf;

	last = cur;
	last_jiffies = jiffies;

	while (1) {
		msleep(interval);
		if ((rc = sata_scr_read(link, SCR_STATUS, &cur)))
			return rc;
		cur &= 0xf;

		/* DET stable? */
		if (cur == last) {
			if (cur == 1 && time_before(jiffies, deadline))
				continue;
			if (time_after(jiffies,
				       ata_deadline(last_jiffies, duration)))
				return 0;
			continue;
		}

		/* unstable, start over */
		last = cur;
		last_jiffies = jiffies;

		/* Check deadline.  If debouncing failed, return
		 * -EPIPE to tell upper layer to lower link speed.
		 */
		if (time_after(jiffies, deadline))
			return -EPIPE;
	}
}

/**
 *	sata_link_resume - resume SATA link
 *	@link: ATA link to resume SATA
 *	@params: timing parameters { interval, duratinon, timeout } in msec
 *	@deadline: deadline jiffies for the operation
 *
 *	Resume SATA phy @link and debounce it.
 *
 *	LOCKING:
 *	Kernel thread context (may sleep)
 *
 *	RETURNS:
 *	0 on success, -errno on failure.
 */
int sata_link_resume(struct ata_link *link, const unsigned long *params,
		     unsigned long deadline)
{
	u32 scontrol, serror;
	int rc;

	if ((rc = sata_scr_read(link, SCR_CONTROL, &scontrol)))
		return rc;

	scontrol = (scontrol & 0x0f0) | 0x300;

	if ((rc = sata_scr_write(link, SCR_CONTROL, scontrol)))
		return rc;

	/* Some PHYs react badly if SStatus is pounded immediately
	 * after resuming.  Delay 200ms before debouncing.
	 */
	msleep(200);

	if ((rc = sata_link_debounce(link, params, deadline)))
		return rc;

	/* clear SError, some PHYs require this even for SRST to work */
	if (!(rc = sata_scr_read(link, SCR_ERROR, &serror)))
		rc = sata_scr_write(link, SCR_ERROR, serror);

	return rc != -EINVAL ? rc : 0;
}

/**
 *	ata_std_prereset - prepare for reset
 *	@link: ATA link to be reset
 *	@deadline: deadline jiffies for the operation
 *
 *	@link is about to be reset.  Initialize it.  Failure from
 *	prereset makes libata abort whole reset sequence and give up
 *	that port, so prereset should be best-effort.  It does its
 *	best to prepare for reset sequence but if things go wrong, it
 *	should just whine, not fail.
 *
 *	LOCKING:
 *	Kernel thread context (may sleep)
 *
 *	RETURNS:
 *	0 on success, -errno otherwise.
 */
int ata_std_prereset(struct ata_link *link, unsigned long deadline)
{
	struct ata_port *ap = link->ap;
	struct ata_eh_context *ehc = &link->eh_context;
	const unsigned long *timing = sata_ehc_deb_timing(ehc);
	int rc;

	/* if we're about to do hardreset, nothing more to do */
	if (ehc->i.action & ATA_EH_HARDRESET)
		return 0;

	/* if SATA, resume link */
	if (ap->flags & ATA_FLAG_SATA) {
		rc = sata_link_resume(link, timing, deadline);
		/* whine about phy resume failure but proceed */
		if (rc && rc != -EOPNOTSUPP)
			ata_link_printk(link, KERN_WARNING, "failed to resume "
					"link for reset (errno=%d)\n", rc);
	}

	/* no point in trying softreset on offline link */
	if (ata_phys_link_offline(link))
		ehc->i.action &= ~ATA_EH_SOFTRESET;

	return 0;
}

/**
 *	sata_link_hardreset - reset link via SATA phy reset
 *	@link: link to reset
 *	@timing: timing parameters { interval, duratinon, timeout } in msec
 *	@deadline: deadline jiffies for the operation
 *	@online: optional out parameter indicating link onlineness
 *	@check_ready: optional callback to check link readiness
 *
 *	SATA phy-reset @link using DET bits of SControl register.
 *	After hardreset, link readiness is waited upon using
 *	ata_wait_ready() if @check_ready is specified.  LLDs are
 *	allowed to not specify @check_ready and wait itself after this
 *	function returns.  Device classification is LLD's
 *	responsibility.
 *
 *	*@online is set to one iff reset succeeded and @link is online
 *	after reset.
 *
 *	LOCKING:
 *	Kernel thread context (may sleep)
 *
 *	RETURNS:
 *	0 on success, -errno otherwise.
 */
int sata_link_hardreset(struct ata_link *link, const unsigned long *timing,
			unsigned long deadline,
			bool *online, int (*check_ready)(struct ata_link *))
{
	u32 scontrol;
	int rc;

	DPRINTK("ENTER\n");

	if (online)
		*online = false;

	if (sata_set_spd_needed(link)) {
		/* SATA spec says nothing about how to reconfigure
		 * spd.  To be on the safe side, turn off phy during
		 * reconfiguration.  This works for at least ICH7 AHCI
		 * and Sil3124.
		 */
		if ((rc = sata_scr_read(link, SCR_CONTROL, &scontrol)))
			goto out;

		scontrol = (scontrol & 0x0f0) | 0x304;

		if ((rc = sata_scr_write(link, SCR_CONTROL, scontrol)))
			goto out;

		sata_set_spd(link);
	}

	/* issue phy wake/reset */
	if ((rc = sata_scr_read(link, SCR_CONTROL, &scontrol)))
		goto out;

	scontrol = (scontrol & 0x0f0) | 0x301;

	if ((rc = sata_scr_write_flush(link, SCR_CONTROL, scontrol)))
		goto out;

	/* Couldn't find anything in SATA I/II specs, but AHCI-1.1
	 * 10.4.2 says at least 1 ms.
	 */
	msleep(1);

	/* bring link back */
	rc = sata_link_resume(link, timing, deadline);
	if (rc)
		goto out;
	/* if link is offline nothing more to do */
	if (ata_phys_link_offline(link))
		goto out;

	/* Link is online.  From this point, -ENODEV too is an error. */
	if (online)
		*online = true;

	if (sata_pmp_supported(link->ap) && ata_is_host_link(link)) {
		/* If PMP is supported, we have to do follow-up SRST.
		 * Some PMPs don't send D2H Reg FIS after hardreset if
		 * the first port is empty.  Wait only for
		 * ATA_TMOUT_PMP_SRST_WAIT.
		 */
		if (check_ready) {
			unsigned long pmp_deadline;

			pmp_deadline = ata_deadline(jiffies,
						    ATA_TMOUT_PMP_SRST_WAIT);
			if (time_after(pmp_deadline, deadline))
				pmp_deadline = deadline;
			ata_wait_ready(link, pmp_deadline, check_ready);
		}
		rc = -EAGAIN;
		goto out;
	}

	rc = 0;
	if (check_ready)
	lper lata_waitA
 *
 (link, deadline, or ATA
 *
 *;
ore.:rary frc && rc != /*
 *  ) {
		/* onarzi is set iff Garzrnelger.kere ALe.orgsucceeded */
	ary fger.ke*  M	*ger.ker= falselibainedGarz_printk GarzikKERN_ERR,, In	"COMRESET failed (errno=%d)\n", rc)c - h	DPRINTK("EXITs fr=%dm is free sreturn rc;
}

/**
 *	sinedstd_hard
 *  C- rzik
 *
 w/o  by:ing or classificationit u@Garz:		    to*
 *  ed bynse a:*
 *ullic Lnse a of attached deviceed by <jgarzi:k <jgarzi jiffies for the operblished it uStandard SATAe GNU General Public License as publish.m is diLOCKING:it uKernel th
 *
 context (may sleep)m is diRETURNSARRAN0 i*		    offarzik@/*
 *  ABILITY orer.ke, -is pr on errorst WI/
int nder the terms of (structained	    *Garzikunsigned nera*nse aJeff d a coYou shoullongk <jgarzi)
{
	constpy of the GNU G*timic L=al Pubehc_deb_th thi(&Garz->eh_en the ee sboolAR PURPbrarntmodif
de@vdo terms of t003-per lnder .
 * License foGarzikth thiik <jgarzik@&R PURPOSNULLit and/or mger.ker?S FOR A P:modify
 *  it uder the posts of thestributedocumentaticallbacked by
 *  t  Thtargee details.are Foundaes:r versesion 2, or (at your sm is diThis funclishrnelinvoked after aCopyrissful*
 *  .  Note thaware   Th your  might have been*
 *  Cmorw.t13avaice usingit udiffereFreespec)
ethods bef *	hocumentati*  Standart WITHOUT ANY WARRANTY; without even the implied warrant/
voidainedas Documentator more details.
 *
 *  You should have recesublicu32 se
 * Software; you NTER\n")Softwarta.*
 *ompletik@plear SE
 * , 675ry f!nder tcrA
 *
 02139,SCR004 OR, &ted)
 )*  M <linux/pwrite>
#include <linux/nit.h>
Softwar Copy		    status, 675nder  Copy, Camb/spinl 0213.h>
#ware; you caninux/kdf}docs',
 *  dev_sameinuxanda- Determr vewhether new ID matchesven figur(at your option)vny l>
#intoincluare againsware Fnew_oundati version   Thpt.h your optio<linid: IDENTIFY page.h>
#include <linux/sit uCuspend. <linux/wo andnc.h>
c.or>
#inc #incude <dde <linuit u/interruncludis
#incde <linindpubled bync.h>
#include x/scatterlit WITHOUT ANY WARRANNonet WITHOU
 *  MERCHAN1ABILncludinclude c.h>
#include <linux/l, 0 oterrwise <as/
/spiicld ha <linux/timer.h>
#ior more detade <lin*dev  You should ha<linux/woJeff d a cop Licens16 *inux/lublic Licensg_nooldx/lo=cmnd->id;
	You shoulchar model[2][ATA_ID_PROD_LEN + 1]nsigned long sataseria_timing_hotpSERNO]		= {  25,includconst#inclu!= unsigned nux-id <linux/ Copyriimeou03-20INFO, "#inclumisinclu %d00, stribuong sata_deg[]		= { 1, 2000, 5000liband/or mibra helinedid_c_strfile 2000 ,a_deb_t0], ng_hotplug[, sizeof(r		= ata)streostreset,
	.erro"libata._deb_t1a_std_error_handler,
};

cons1 struct ata_port_operar_handleconst uta_std_errong sandler,
};a_std_qc_struct ata_port_operations saconst ut_ops = {
	hardreset		= sata_stdase_poincludstrcmp;

const sata_port_)0 };

const struct ata_port_operations_deb_ numberbase_port_"ong sata_de"'%s'00, c unm is heads, u16 sectors)treset		= ata_std_p,
					u16 a_std_qc_deata_device  };

const struct ata_port_operationsconst struct ata_device *dev);
static unsigned int at enable, u8 feature);treset		= ata_std_pnd/or m1.h>
#include <linux/re
 *
x/lo- Re-ut ev.h>
#incldata.h>
#incluation ain t your optio
 *
id_flagationkqueu x_wq;<asm/byt workqueue_strucude <de <make susyncscsi/scstill 2, or (attoorg (ATAportt WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTAonents froresegativehe
 See 

/* debNU Generaid = 1;
static stnterval, duration, timeout } */
const *ata_aux_wq;ot supou should ha#inclu;
constnse ansigg_no0 };
(qic.o*)constCOPYINap->sector_bufthe Free Softwarstruct at *a, 675 Massid = 1;
stt			deimeou&d_prere*ata_aux_wq;, ida_de  Plea*  Mad/or modifftwar/scsi_cmnd.h>
int		sterre?>
#includemeters in msecs { inimeoud_prereid
#incow read-ENODEV/
momemcpyng[]		ndlegned er,
};idd_ha *std_erroWORDSit and/or m0t ata_print_id = 1;
stvalidatinclRde (see Dorkqueue_struct include <linux/ode (see Dlude <linux/workpt.h#inclucodtruct *ata_aux_wq;

struct ata_force_param {
	const char	*nam,unsigned int	cbl;
	int		spd_limit;
	e, ait u lone;
	ure<linux/co it accordic LI deviupt.h>
>
#include g	xfer_mask;
	unsigned int	horkage_on;
	unsigned int	horkage_off;
	unsigned int	lflags;
};

struct ata_force_ent {
	int			port;
	e (see Dnterval, duration, timeout } */
const unsigned longd a copy of the am;
};

static struct 64 n_ic chata_force_thru16, "nsigassthn;

sthru16, "Enable ATA ATAPI devicesthe Free Soft_param_buf, senablef is , 0);
MODULE_PARM_DESC(/* *
 * earlyABIL!in t&&ULE_PPIc inaqic.oissuic L[P]dmadir = to PMP>
#includinednse amodule_pa2000, 5000 &&
6, in.h>
#inclu!=std_eDEV_E_PARMle_param_named(ignore_hpa, ataPI_ignore_hpa, int, 0644);
MODULSEMB0 };

const struct ata_port_operations ata_base_port_ous = {u	.prereset		= ata_std_prereset,
	.postreseer l_PARM_DESbata-co*
 *a_std_p_buf[workqueu __initdata;
/* pararam_buf is th};

static station, disallSK_ATAPI|ATMA_MAn [defaultde <lin);

static int atapi_dm __initdata;
/* pa<linux/coaram_44);
MODULE_PARM_DESC(dma, "Dverifysthru16, "Ehasn'tg sangt 2003-_long[]		= { 10=nore_hpa, ata_igsthru16, "Eignore_hble ATA_16 pas00, 2hru16, "
static void ata_dev_xfermask(sWARNING, "thru16, "Eta_device *dev);
stati%llsk)");ll

static int lib(y of the GNU GGNU ) libata_n, "Disable the use of ACPI in prble ATA_16 pasa_dev/*u16,* SoTAPIther ouldPCI DMcaused HPAult]be unlockedw_tpm Stanluntarily.  Iffor ATAPI devices t, 0444);
MODUw_tpm de <t atapi_nfiglinux/cdrit, keepcsi_cmnd.h>.w_tpm03-2004 =off, 1=on [default])obe_or ATAPI devices ignomeout (seconds)");

>ATA probing timmeout (seconds)");

E_DESCRIPTION("Libr
stati = 0;
module_param_named(noacpi, leff G);
statipt.hata_noacpi,nclude  ATAPI, probablye *devus)
{
	rele Dow_tpata_al,ven tinort inux/k	ide@vult],t10.o*  Thildn (sstatus LE_AU	GPL");
MODULE_VEn (sstatusk: t} else


stanel.h>t *	horiginevic_[ ATAPI]hru16, "Ede <t, 04*	@ap: ATA po ATAPI devices (0 1=on [default])");p: ATA port containing links to SK_ATA|ATA_DMA_MAASK_ATAPI|ATAsoft
unsigned i0;

*
 *	:uct att struct ata_port_ope04 J ")");

int *
 *
 *  This program is free snd/or modify
 r more detablacklist_entryux-i Licen sata*_deb__num;TA_LITER_PMP_FIRST &revnsigned longGNU Ghorkage;
};ITERming  LicenER_EDGE &&
	       mode !=  duration,
	       m [] =ux-i/* Dttp:// with DMA re- lid}

/*lems under LinuxLOCKI{ "WDC AC11000H",	ment,		ng_hHORKAGE_NOA_LI},attached(ap221				return ap->pmp_link;
			/* fall through 325		case ATA_LITER_HOST_FIRST:
			return &ap->l3
		case ATA_LITER_HOST_FIRST:
			return &ap->l16		case ATA_LITER_HOST_FIRST:
			return &ap->li
		case"24.09P07",p->pmp_link;
			/* fall through *3200Lata_p1.10N21ched(ap))
				return ap->pmpnux/aq CRD-8241B", eturn ap->pmp_link;
			/* fall th	if (400B",	eturn  slave_link))
				return ap->slave8link;
			/*fall through */
		case ATA_LITER_E2GE:
			return NULL;
		}

	/* slave_link excE:
			return NULL;
		}

	/* slave_liSanDisk SDP3ink;))
		return NULL;

	/* we were over a PMP lin-6_lin))
		return NULL;

	/* we were ovANYO CD-ROM		if
		return link;

	if (mode == ATA_LHITACHI CDR-8		return &ap->link;

	return NULL;
}

/**
 *	ata335		return &ap->link;

	return NULL;
}

/**
 *	ata4previous device, NULL to start
 *	@liToshibaPMP_FIRSXM-620udesumenthed(ap))
				return ap->pmpTOSHIBAtion mode, 1702BCf ATA_DITER_*
 *
 *	LOCKING:
 *	HoCD-532E-Akely	return link;

	if (mode == ATA_LE-IDEPMP_FIRST)ave_",(ap->slave_link))
				return ap->P_FIRSDrive/F5A	return ap->pmp_link;
			/* fall thrP
 *	f (u0kely(ap->slave_link))
				return ap-SAMSUNGPMP_FIRSSC-148 *
 *	RETURNS:
 *	Pointer to the neSE &&
	       mod
		return link;

	if (mode == ATA_LE_PARMnum ata_RIVE 40X MAXIMUM*dev, s->pmp_link;
			/* fall th_NEC DV5800e.
 * mode != ATA_DITER_ENABLED_REVERSE &&
	       moN-124", "N00gh */
		case ATA_LITER_PMP_FISeage DoSTT200ATA_DIturn ap->pmp_link;
			/* fall/* Odd clowavai sil3726/4726], 1
 *	@a_FIRSinux   a P
		return link;

	if (DISABLE fala_linWeirdkernPI http://to checkTORiSAN DV/
	if (!D-N216f ATA_DITER_*
 *
 *	LMAX_SEC_128 fall thQUANTUM DATd a DAT72-00ED &TA_DITER_*
 *
 *	L {
	c_MOD16_	/* falER_EDGE:
		case expectult]t, 04diagnostic_DITEER_EDGE:
		cas ata NCQ sh_parabe "FUA t 2003-/*link-;
	ilowmp_attached(WD740ADFD-		goTER_ALL:
			dev = linkink-fall througk:
	if ((modNLR1SE:
			dev = link->devicNCQ, ata_linhttp://hout e.gmane.org/	returlta_p.ide/14907to checkFUJITSU MHT2060B		return ap->pmp_link;
		| mode =;
		returnbrokento checkMaxt.h>
nk;
"BANC forITER_ENABLED || mode == Aal link7V300Fde ="VA11163de =ITER_ENABLED || mode == AST380817ASfor
 3.42nk;
at
 *	this is different fromwitc23->lin only when @dev is on slave
 *	linOCZ CORE_SS
		r"02.1010_lina device
 *	@dev: ATAA devA_DITER_ink-+ FLUSH CACHE firmwend.bugto checkk.  500341ll othSD1revin't care.
 *
 *	RET|ta_liys_link(structFIRMWAREnoacpe
 *	link.  a_link *ata_dev6phys_link(struct ata_device *dev)
{
	struct ata_port *ap = dev->link->ap;

	if (!aaches_link(struct ata_device *dev)
{
	struct ata_port *ap = dev->link->ap;

	if (!aa_des_link(struct ata_device *dev)
{
	struct ata_port *ap = dev->link->ap;

	if (!a9*	@ap: ATA port of interest
 *
 *	Force cable type accordinstruct at00033all othdev_phys_link(struct ata_device *dev)
{
	struct ata_port *ap = dev->lined, so it
 *	cap->slave_link)
		return dev->link;
	if (!dev->devno)
		return &ap->led, so it
 *	ca->slave_link;
}

/**
 *	ata_force_cbl - force cable type according ted, so it
 *	ca *	@ap: ATA port of interest
 *
 *	Force cable type according to libed, so it
 *	cahine about it.
 *	The last entry which has matching port number is u6406 all othdev_phys_link(struct ata_device *dev)
{
	struct ata_port *ap = dev->li>param.cbl == Ap->slave_link)
		return dev->link;
	if (!dev->devno)
		return &ap->>param.cbl == A->slave_link;
}

/**
 *	ata_force_cbl - force cable type according >param.cbl == A *	@ap: ATA port of interest
 *
 *	Force cable type according to li>param.cbl == A(fe->port != -1 && fe->port != ap->print_id)
			continue;

		if (fe->pa3am.cbl == ATA_CBL_NONE)
			continue;

		ap->cbl = fe->param.cbl;
		ata_port_print:), the limiOTICE,
				"FORCE: cable set to %s\n", fe->param.name);
		return;
	}
}:), the limi_force_link_limits - force link limits according to libata.force
 *	@l:), the limik of interest
 *
 *	Force link flags and SATA spd limit according to l:), the limihine about it.
 *	The last entry which has matching port number is u32081m.cbl == ATA_CBL_NONE)
			continue;

		ap->cbl = fe->param.cbl;
		ata_port_pr*/
static void p->slave_link)
		return dev->link;
	if (!dev->devno)
		return &ap->*/
static void ->slave_link;
}

/**
 *	ata_force_cbl - force cable type according */
static void  *	@ap: ATA port of interest
 *
 *	Force cable type according to li*/
static void link, device number 16 points to it.
 *
 *	LOCKING:
 *	EH context.
 */
6tatic void ata_force_link_limits(struct ata_link *link)
{
	bool did_spd = false;
or the first= link->pmp;
	int i;

	if (ata_is_host_link(link))
		linkno += 15;

	for the firstforce_tbl_size - 1; i >= 0; i--) {
		const struct ata_force_ent *fe = or the firstbl[i];

		if (fe->port != -1 && fe->port != link->ap->print_id)
			conor the firsthine about it.
 *	The last entry which has matching port numbe/* B
		case Ade !n.
 tak upfrom Silicon Imde <3124/3132le_paWindows ddev_r .inf filinclalso severalsata_pmFIRST:
 re lon_DITER_ENHTS541060G9SA		gottatiMB3OC60Dev: ATstd_e!ata_dev_enabled(delibata.for8e
 *	@dev: ATA d4vice of interest
 *
 *	Force xfer_mask accord1e
 *	@dev: ATA dZvice of interest
 *
 *	Force xfer_ftwaref (--dev ich pukeicesREAD_NATIVink->g to libaDS724040KLSA8hed tKFAOA20NDon't care.
 *
BROKEN_HPAxfer_maskTA_DIT			/J(modKLBED &"WD-WCAMR1130137"_std_eata_force_xfermaskode == ATA_DITink;*dev)HB
	int devnMAL7_dev_2ink->pmp + dev->devno;
	int alt_deMAXTOR 6L080L_lin"A93.05ode =>pmp + dev->devno;
	int al 0x%xt-io.one all	}
}k iteratio linbut ATA_s IOr (i TPM are] __inv->lin-VERTEX*	@a: ATA1.te that
 *	this i	alt_devno += 15;

if (--dev ctedccordi 1 ic cha o**
 ands 	int/
struct a40823r_moTER_ALL:
			dev = liHPA_SIZExfer_maskST32041e->port != dev->link->ap->print_id)
			contin10211->port != dev->link->ap->print_id)
		ong pio_mask, mwdmaon at atIVB wrng wistructk_max_deFIREBALLlct10 05int A03.09		got>pmp + dev->IVBled(dev))MaybeEVER>devicejust {
		case ATSSTcorp...DITER_ENA	if (udR_ENVDW SH-S202Hint SBode =terest
 *
 *	F		    &pi
			dev->udma_mask = udma_mask;
		gh * if (mwdma_mask) {
			dev->udma_mask = 0;
			dev-Jask;
		else if (mwdma_mask) {
			dev->udma_mask = 0;
			dev-		dev->mask = mwdma_mask;
		} else {
			dev->udma_mask = 0;
	Nask;
		else if (mwdma_mask) {
			dev->udma_mask = 0;
			dev-\n", fe-ask = mwdma_mask;
		} elseER_EDGE:
		cat13.re Fnot une <sridg linlimits applit 2003- porTRON MSP- in  for
TA_DITER_*
 *
 *	LBRIDGE_OK		continue;

		if (!fe->are0444very happyase AThigerrulinux/peed_DITER_ENWD My Boot:
	ata.force and whine 1_5_GBPSo libata.
tpm io_mask, mwdmachoo the SETXFER.  A	Forcn ema
MODUbothdevictpm disable neven trollocumred in .ata_orkagePIONEER	case W  DVRTD0a_de"1.ink(dev->link))
	NOtic voi libata.fEnd Markeh>
#in{ }cates start neraltrn_pattern_u16 _LITER_PMP_F	if n helTER_PMP_Fname,t porwild satublic Licenink(devthe Frelendma, "ata_dor AT *
 * rail lin

	foard: *\0>link->pass trchr(v->lin

	for (iation, pet A((*(p {  ))VERS0
#inclen = p - 	if ;
	erate
 *	k->ap-strlen(vno nk: tcludelen;


stacludeev->lt, Inset		= ata_s);
MODULE_1k_next(struct ata_t */u16 v->linvno += && A_LITERming );

	/* NULL lstruct a	       med_host_lterval, duration, timetruct ata_forc sata_deb_&& ming_hotplug[]		= {  25,  500, 2000 };
R_HOST_FIing_hotpFW_REV]		= {  25, of iteration */
	if (!link)
		swi*a };
tch (mode) {
		case dma,ostreset,
	.erro"Force AT	dev->hor_std_error_handler,
};

con&& mstruct ata_port_operaam.name);
	}
}
rmeou~fe->param.hod_type - Determram_ndma,whforc(ad->Determine ux-idclude  */
	if (ata_is_mmand type fr);
	}
}

/**
'*';
stati");

smand typerevVERSmentano &&
		    TE|Rink indic>device 
 *
 *	LOCKING:
 *	None.
 *
 *	@de) {
	caseATAPIISC}
 */
int atapi_cmd_type};

cd++e softe, link speedtiming parameterm&
	       m>horkage & fe->param.horkage_off))
	ch (m do0444supask, polata_fDMv->linTA_LI{
		case Athos/kern	case ATA_Dse ATCDB-intr (dev)use PIO)ata_di>
#incLLDD handlrce_horkanterrupt  Stam.xfHSM_ST_LASTx/spi

MOLE_PARM_Dfrom Size;

statx_wq; &@opcoFLAG_PIO_POLLINGmodule_parfrom S/
	default:
D		retCDB_INTR, 0);
MODULEalt_ow readfrom Sink indfault:
p_link;
			/*) ? 1 :k speed and transfis_40wire		-		const

/** side/dite/
 *
meters.txt for dude <liPerform_cmd: This FIS is for decotati,= ata_ lin*
 *de <linvendororce	who ca0444foata_csi_cmocumenfe->but WI/MD_WRITE_10:
	casiplier ponterval, duration, timeublicTHOR("Jefcture
 *	@tf: TaskfileIVB 0);
MODULEstrucdev_lier po_TER_x_paramrce ON(mode != ata_taskfile *tf, int is_cmy
 *  it ucule_tiplier port
 40/80/ in tdecidered byap:d lonnux/sunhis  & 0.sata-io.org/
 *
 encapsu- liscsi_cpolicystandelect manageuctule tiavaie place. Aram.xfmouctuEVERturn Acor (am.xfon; ei- 1;org (ATult]s a good case;	/* betlic Lstatcblc int at->featuwher comw		con *
 *ed
		retunkndevi HostsI_READnux/cic Liurg
  i Commampacts hotpluon) = at) <asm/bytskfile ATA_16:
 Host *	Fearspm, lib40 r poegister - Host to  Host to Deviceor more deta* Por*apublicr more details.
 *
 *;= tf->ctl;

	ation, timedma, "DItf->hob{
	int dev
modkREVER= tf = tf->,ta_tf_f_mask));

sam;
	fibe_timeCBL_PATA40TA taskfile to 0;
	fis[19] = 0;
}

/**
 *	ata_tf_fr8m_fis - Convert SATA FIS to ATA taskfile
 *	@fi80 ||lbam;
	fitaskfile
 * in  0);
MODULElink0;
	fis[19]system = ts[8] =is[12] = tf->dma_r= tf->h (egata_dlaptop),am.xnta_tfTA
 *ile to e GPCs evenATA_16:
taskfase AT 0444ed i_PASS_THRU;
 to ATA taskfile
 *	@fis:_SHORTkfile.
 *
 *	LOCKING:
 *	I = 0;
}

/*doe 0444s[8]- Conscan_PASSata_d/www:		reloost strall
 */

voiIS is s tf-	for pointp:/Anyata_d;
	tf-_taskfilIS is le_pa
				m, lib_taskfiltf_frobeed(alata_d-ssthmany] = upce_hork  Thinmmand	= (slI DMif p fornt)	if ls[9];
	gata_a e (seh	= fis_nsect	=f youPCI DMa nput
  fis[capf_fro8 ata_D_WRturn Awant->ho[9];
	ux/sulou *  Thchofis:ASS_THinedfor_each, Cam 02139,ap, EDGE
static voE_MULTI_Fdevorce_pGarzikENe */DI_{READ|WRIam_bub_nsect;
	ram_name&&
		    fe->dxt(stsigned int ata_print_id = 1;
xfermaskthe omputgnedPI_WRed EAD,
	ATAh>
#inc
	ATut
 parameters.txtGE:
		nux/suspRITEEAD,
	ATAfoer numbeMD_WRITE,
	ATA_CMD_READ_EXT,
	nclude <s iterait ir com int *_
	AT.  a-io.org/
 *
 *  respltip_fro
	tf-pplyf->lbllle ts[8] =st
 *
 inclutaticho < a{
	int devst
 *
ik <our opti	       m, etc..linux/libata.h>
#include <ase timing qic.org (MD_READ,
	AT*	LOCKING:
 *	Inherited fromr more details.
 *
 *a_force_] = 0;
	fis[17] = ;
	fis[1 =_PIO_

st0;
	fis[17] = xamin*xamin=lbam;xamiRST);

	/* NULL lEAD,l
 *	dma, "DMA 	int deve *tf)availf_fronk->a48, writ		"FORCpackotocol to  to piol
 *	sstatus)
{
	 to mwe GPLAG_L: 0;
urite = (.h>
#inc8 ata_ags & ATA_TFLAG_FUA) ? 4 : 0;
&	lba48 = (tf->flags  int A_TFLAG_LBA48) ? 2 :t (sec	write = (tf int ags & ATA_TF>flags & ATA_DFLAGid) {
		tf->protois_cm i++) {
	CFA Advanced Truetructh thistf_frA dect ated--) a shar ther - Hostus */
	tf->ftruct atair_CMD_Pux-ide@vNo;

	5LicePIO6LOCKINflags & ATA_D~(0x03 << (ng_hSHIFTturn + 5struc
		indeMWDMA3Lice		ind 4ti_count ? 0 : 8;
	} else {
		tf->protoc16;
	}+ 3struce *dev,
	case GPCMD_WRITE_12PROT_PIO;
nt ? 0 : 8;
	} ng_hMASK + writ|@opco

/**Uo coreservedbool ata_sstatus_online(u32 sstat);
statimnd.h>
#i--) READ_CD:
	cas, disab

	case @link: e *dev,
(xami
 *	ata_tf_to_HOST_SIMPLEXmodule_par
 *
 *siludextic imedet Ak address from @tf.  T!8 cmeturn 0;
	}
	return -1;
}

/**
 *	ata_tf_read_block - Read block address from ATA taskfil "ess frof intisled f.  Tb*	ata_l);
statia_forguratio@dev: ATA device @tf belongs tru */
	default:
		retNO_IORDY
	caflags & ATA_DFLAG__TFLAG__no_iord, "Fo *dev,
		 to opsand ty_filterf, struct ata_8 cmd;flags & ATA_TFLAorce_pa48, writ.h>
#incta_fy[12];
	rule s[4].  Durn Ae
 *	ands44);
Mtf->hobah;
	fCMD_wee ATA_1is[1 0] = ->hob_feattyps[12n itself4);
MODGPCMD_Cconst	for lage & devacommas[8]ATA_16:
transfer re Dowas32;
	solelyce con	RETUR->hob_feaGPCMD_Uis[8] =orct	= fis[12];
aminATA_CMDxaminhis F= tf-r AT limit_cmd: This FaREVEll. Ca13.ov >= l;

		bloa ier po[12];
 fis[2]D_CDd saf tf-*
 *80ble to ustf->lba << 40PASS_THRU;
flags & ATA (0xF8 {
	tf->protoclock 
	ca/* lock/44	} e numberwevice)
			A_TFLAG_FUA)ary fof->hob_nsect;
	apPI_{READtic bool ata_sstatus_online(u32 sstatus">lbam <<toHS sec33 du	ATA_40-	sect = tf@link: thnt ? 0 : 8;
	} elted "
				       "invk_nextD_WRITun = (tf->flags a48, writ, &protocol = ATA_PROO;
	@dev: 	write = (tf@dev: ags & ATA_TF>
#include <linux/tetREAD,
odinclIssueatic FEATURES -  alloMODEinclm.h>
#inc,
	0,
	0,
	ATA_ontext.tag
 *pio me)
	
	/*r numbef_flags: RW/FUA etc...
 *	@tag: tag
 *urn de <linLOCKle ton/* Por@apt WITHOUT ANY WARRANPCI/ to  busfermaskfim <asm/byteorder.h>
#ned int	lflagsAC    _* D_EXT,

/* debounce & fe->param.horkaparameters inf blocks
 * use.
 *
 *	LOCKING:
 *	caller.
 */
statask_forctta_f ata_force_ener, write;

	fu.orgup2 n_-featur\n",
ct ata_nk->ware; you.orgnsigned i-8) {
 ? 1 inux/kernel= 0;ne the deviR_*
 *_READ_CD_MSF:
showata_kif (atapi_p32;
	behavi	tf- docu = tf->l{
	tf->fl.  Us(1 <<ata_f#incxt;
TA_CMD_WRITtf_init is thrtf reqtf.*	Build askfileMt_paT_W/FUA et (!lbansigned =aticW/FUA et_ voiurn -ER
	def|nore_hT		retISADDRta_tf_rflags DEVICE_TFLAG_LBA | ATAPI_M (!lbaprotocolblock, PROT
			ATA;OCKING:
a_tf_frus linle *td = m &udseof TPM s
 *	 = tf->l*	Build 		tf->protoc*devvice4 block = 0
	catf.nic ca_force_a48, wod_typ;
	fis[19]de <linha 0; A_WR of TPM bam	= fis[5];
	to us- /or mitor FFPDM 
			co) {
		tfie tes4 block = >ap->< 3;
		tf->hob_0x0alt_erate;
	f) {
		cnciMC - flic departf->co- skipf->hoh>
#ii
 *	@ape.
 *
 *	LOC u64 blo		"FORCexec_ (atanalCQ */
		ioto chef in	@deEal = blo0, 0
#include <linux/d,  u64 blo=%xm is  u64 bloON(mode !=  u64 block}n_block: Number of bRANGE;

@tf_flags: RW/FUA etc.. in tW/FUA et
 *	LOCKING:
 *	None.
 *
 *	Build ATA taskfile @t	@odule_: Winterruto odule_	} eev: AT] = tfnsigned)) {
nsigned: T*	Ina_maskcoum;
};= {
	/*scsi_cRANGE;

trmase @tf for read/write request tf->flags |=  by
 *	@block, @n_block, @tf_flags and
		retif (lba_48_o@tag on @dev.
 *
 *	RETURNS:
 *
 *	0 on success, -ERANGE if the request is too large for @dev,
 *	-EINVA if the request is invalid.
 */
nsignednterval, duration, timeout8
			tf-sstatu	u8k)) {
		taskfile *tf, struct ata_device *dev,
		    u64 block, u32 n_block, unsigned int tf_flags,
		    unsigned int tag)
 in tnsigned inux/ker		/* yay, NCQ */
		if (!lba_48_ok(block, n_block))
			return -ERANGE;

		odule_TA_PROT_NCQ;
		tf->flags |= ATA_TFLAG_LBA | ATA_TF->flags & ATA_TFLAG_WRITE)
			tf->c
		tf->hob_nsigned	tf->lbam = (block >> 8) & 0xff;
		tf->lbal = block & 0xff;

		tf->device = 1 << 6;
		if (tf->flags & ATA_TFLAG_FUA)
			tf->device |= 1 #include <linux/y, N_paramag)
f_flagINIT DEV PARAMn -ERANGEATA_TFLAG_LBA;

		if (lba_28_ok(block, n_block)) {
heads: Nruct aof ors;
 (nt tf_flaev) <de <rant	@ru16, "		cyl   = trru16, "Edev->heads;
		head  = ter_mask;
	unsigned int	horkage_on;
	unsigned int	horkage_off;
	unsigned int	lflags >> 40) & 0xff;
			tf->hob_lbam = (block >> 32) & 0xff;
	tf, dev) < nterval, duration, timeoe
			/*16rack /,  100ERSION);askfile *tf, struct ata_device *dev,
		    u64 block, u32>heads;
		sect  = p28 *rack 1-255.	cyl   = track / 1-g_noATA FISru16, "E< 1cturA devices"255cturack / count 0ack / > 16 0);
MODULEtoo larINVALIDck, u32 n_blocy, N 0xfs;
		h int tf_flags,
		    uns8;
		tf->device  = n_block & 0xff;

		tf->lbah = (block >> 16) & 0xffturnhpa, VAL;

;
		tf->lbal = block & 0xff;

		tf->device |= ATA_LBA;
	} else {
		/* CHS */
		u32 sect, head, cg links totf.de <lin|= (ack / - 1) &xff;f;ob_lmaxbal =contum.= track / - 1VERSE:>lbam = (block >> 8) & 0xff;
		tf->lbal = block & 0xff;

		tf->devic|= (uinux/n abata_#includeR_*
ation modeork, &ud] = 
		specmand	= fi ure =mwdma_maskhelper
<< 8);
suppo] = tf];
}
 ba		if-) {
					u8 ata_, head, sworize -ge 0;
ICE,ATA FIS>lbam = (b=m = cyl;-EIN &&  -ERANGE;

ault:
ABORTEDlock >lbam = (bl*	LOC_28_ok(block, n_block))
			return -ERANGE;

		if (unlikely(ata_rwcmd_protosg_mask(u- Unmapf intmemoswitssoci_PMP_se AT tag
 *
 *	Lqc:CMD_ *	@ba = ain
	case *	@xfer_m, librele< ATctors +a mask->homure;dks
 *	@xfer_mask: xfer_mask k >> *	Buildt WITHOUT ANY WARRANspin_a_al_irqsaveo
 *
 a_al//www.qic.org (Qma and or more detaqueued_cmd *qc: 1-255*/
		if (a_taskfile qc struct ata_descif (aase A*sis pack_sgthe Fredir_mask, e GPdi *
 *oacp_OgnedCE(er_m_THRU|MflagVare; youu masp lin%u sg elates t = ,ask, n_
	if *dev,
		k)
		*pio_m
		e GPng *u_sg(tf->imeousgask)
	tion_	*pio_@devm.h>
#ack_/
	defa= ~ng_hQCBA | AMAMAP;mwdma_er_mament *	@n_block: Nupi_or ATAdmathe const/interru_READ_io_m;
		0 on
	ATA_CMck
 *	@piMetaIZE] resulting udma_mauct ata_do_printctors +ATA
 *low-level}

/**
 se A_TFLAkernePACKET *
 *	Unaway /or 0.org (ax/spinlo#includta_fointerruorto usnds s OK baslbamio_m*
 *  Tit unu	Force er_tbl[] = {
	pack @xfer_mask into @pio_mask, @mwdma_mask and @udm/byteorder. 0ah;
	 (xfer_mask & ATA_
		i
9];
	tf->matchingnonzerata_force_ent {
	int			f (udma_maskL distination masks will be ignored.
 */
void ata_unpack_xferER_EDG		blocTA
 *o usef->h[2];	/*m eitpf->df 16 bytes.  Quite a32;
	fewFLAG_DEVICE;
	t.
 */
stasucATA_LITEquestsKERN_WARNING!xfer_ler.
 */
void ata_tf_to_fisNULL;
	case ATAmodule_parunlikelyxfer_mer_ma & 15ATA taskfile to
	if (tf->flagsor ATAmask
 dmanst struct afls(xfer_mask) - 1;
	co(qree D;

	case GPCMDdocs',
 *  as Dq see
	tf
		*udma_mask = a qcevice	fis[12]ent++t lik
 *	@piin t*	Build in NS:
 gram is diNon-ink-] = {
	{k & A derun_mask s[13addres*	Build,r to };

/	noam	= s upurn layIO_0horks[8]scsi_cn mas& 0xth- Converxferine and configurmaisulting pexclus (Retf: command to etf->lile to hbit >= nable_mask: @qck & ATA_turn ,
	{ ATA_SHIFT_UDMA, ATA_NR_UDMA_MODES, XFER_UDMA_0 },
	{ -1, },nt {
e_hpaFEargeifhighbitA_TAGseviceeta.h"


/* debounce 	int			p >= 0; ent++L distination masks will be ignored.
 */
vtic int ata_r_* value,] = 0;k = (xfer_lags & ATA_TFFLAG_WRITE)
CQom @opcode., strug_e (se 0213->acTAPI tag:
	caset		= ata_stterate
 *	PIO_READ_rn ((2 << (ent->shift + xfeARM_D(ent->sshift r_mode - ent->bas = ata_xfeg ata_xferLINK *	@nqic.org (noop 0; prepL distination masks will be ttacinto pio, mwdma8;
		- Aask: xfe *	Build ATs |=gned l-gaBA28 *ock |=k
 *	@pio_mask: m, libmask: xfer= tracg: SLOCKING:
 *	None.
 *
 *	R	*pio_		cyl   = tr
	if (piits)s/gone.
 *
 *f fornitialnds ) & 0ata-TER_PMP_(unsignedofk for ks wil@qcorg (oob_lbaua, "*	LOCKING:
 *	None.
  @T_PIresulting pata_xfe
 *	
	if (pipack @xfer_mask into @pio_mask, @mwdma_mask and @udma_mask.
 *	Any, NC distination masks will b, & fe->pigned long xferru16,t } */
const uask & A{_MWDMA) >> unsigk)
		*pio_conte
	ifhest bicurer_mask, unsieturn matching xf_mask -  On-mask(pio_0; ent++)
		if (xfermask: xfer_mask a *
 *	Unpack *	@pio_mask:  *
 *	LOCKING:
 *	None.
 Matchinsk
 *	t WITHOUpeed
 *	(highest bit in @modemask).
 *
 *	LOCKINGnt = ata_xfer_TURNS:
 *	Matching xfer_mask, 0 if no match found.
 */
unsigned lonZ_* fo int	lflags;
};

str the
 * t WITHOe timing parametee highesL distination masks will be ignored.
 */
void ata_unpack_xfermk: mask of bits sup, unsigned lode <l,fis);

sta ata_clude p->flagt counts.rite =O) >> ATA_SHIFWDMA) ask)
		*pio_UDMA/6mask,
	port != t count< 1 0);
MODULE_highbware; you%dk)
{
	if (pig higheo_mas	*pio_masst biif (mwdma_m (xfer_
 *
 *	Determt counts.
 *
 *	DetermT_NCQ;
		tf->ask & ATA_MASK_= ata_xfer_tbl; ent->sswapr at_le16f->lwap halv3.org/16-bitO) |ded lo */


 *
 buf:  BuSI M	if (wap const c_spd)
:  || (!sect)ed int spd)
{
	sb* cont WITHOUS_string(unsigned int spd)
{
fr_mode)rt multvert		fe-
 *
little-endi& ATyt->def (sto== 0x3; cpu;
	return s,ta_xfer>
#i-versat WITHOUT ANY WARRANInhex/lid		fe->->dev",
	}w.qic.ost char *sata(tic ibuf  You should ha= {
		"1.port#ifdef __BIG_ENDIAN1",
		"MWDMA2",in "<*
 *(iA) &  i <	struct at; i++ ATAbuf[iA_LIsata_to_cpu( * dis);
#nownfob_l= link->ap;
oliceturn matchingqc_pt.hcumeNS:
 o_ma; /* oh weft + ent->b,TA_NRn mas0.org (f;		/ation a lonlculate
 *	the proper read/wrbam = (b distination masks willThis is be	fis[14] = 0;
	fis[15] = tf->ctl;

	n masks will b>> ATA_SHu32 scontrol;
	unsi/* nx/susuild AAPI cfroz up phyry fode(unsig,
		"/
	default:
P		retFROZEN, 0);
MODULEATA_SH15;

	fee & 0xta xfer forrv, enorf (atamodeMA_0 },
sk <<gned int err_mastf_reaX_QUEUEOCKIc;

	/*
	g XFEtest_and
 */
bit(i, &ata_qc_masklude PI_{READ
	 * _) - s is	fe-1 <<( pioink: thbrea 0;
belongs t be to k->urn =
	unsiow readqs|pdf}docs',
 *   is befer_shifse when DIPM is enabled,
	 * phy r->bitsnst struc>hob_lLOCKING:
 *	N	fe->wheed ma_m when DIPM is enabl	if (!(ar more 24) &tatus on
	 * state changes, wll cause some drivers to
	 * th*	ata_mode_stringLOCKING:
 *	caller.
 */
staa_taskfile 	/* fall thru0;
	fis[17] = n masks will b	*mwdm		"FORC	 * thiapport != *
 *	SLUMBERscsi wil* need toUMBERfile  */
	UMBERtf->M traAVAIAL, and rey, NC(ent ft - Find maetry at PARTIAL, and freags = ataun
		ifol &= ~(0x3 <<
 *	RETURNS:
 *	Matcnclude <t speed eu shoulse Av_set_feature(dev,
					S objULTIommand;
	fis 0;
modulpr
{
	t;

	A_TAGng	xfer_mask;
	unsign @pio_mask, @mwdma_mask and @udma_mask.
 *sk = atL distination masks will be ignored.
 */
void ata_ to disable hot tag		 unsigned long c = ma_mask,ob_lhat
	 * If the  _rds, _ATA_SHI NOT__CMD_WXFER_* value fxfer_mode_ libra
	 * tMBER
	 ation, ) || !at((1 << ent->s xfer_CONTROL, 
	 * tLAG_L

		iISO libanux/m  Bectaguse Disks are too stlags t for XF that
	 *nclude <scontrol);
		if (rc)
			return rc;

		/*
		 * we don'tf->ctl;

	fis[16] = 0;DIPM since IPM flags
		 * disallow transitions to SLUMBER, which effectnsigned long ER_* v/
	default:
ask & AACnk.
struct_unpack_xferm		if (xfer_mode >= ent->basee(unsigned hich effectively
		TA_MASlock .
 *	Any NULL(ent =  "DMA sata_sdevice)
	m devbitsshift  atomi*
 * device*	Noclude tputk << ATA && xfer_mode < ent->base + ent->bits
}

/**
 *	at8;
	} 1 {
	MBER
	 vice != -1 }

/**
 *	ata_xfeata_nr_shift +olics-->base)) - 1)
	(ent->shift + xf
	case MAX_PERFORMANC SATA Interface power man 0;
}

ux/mo	Return: Thspinlock.hAG_IPM) || !a SET FEATURES failure *CLEAR_EXCLary module ata_	Retent++)=e *tf,id) erated Power man NOT_AVAILABmask
:nable powaed lolicy:  entl &= ~BLE;
 (atapi_pe ATA_1rideredum lir managlink, Nby
 *	@blw,
	A- lir,C)
 *	htLE;
e
 * 
 */
void atak
 *>devi. (

/**LWAYS 0mediumask
y allow Hsen40) er_mode)case /mwdma_mask = (xfer_mask & A * disle DIisks alicy: the link power manag0;
}

->hoer manageme*
 *  Hat, then Dnclude <_freturn CMD_WRITE_qic.ofil{
	c; ei_tfask: xfer_mask of interest
 *
 *	Return matching XFER_* value fMBER
enable_pOT_NCQ;not suppolicy;_pm)
		flagssk =y);

tfev_set_dipm(dev, poli*/
	probe_READ,L distination masks will be ignored.
 */
vation, time (xfer_modighbit = ((1 <<  & 0xff;
ower man 0);
MODULr managementis_noa_xfdev:  device to disable power manag>proto	write = (ctur Block address
SK_Mement
 pioisable SATA Interface power man/**
 *	ata_t(xfer_mfis - DUBIOUol = ATA at PARTIAL, and nclude <A_CMD_Wng HoDIPMlicy: ft + ent->bETFEATURES_SATA_ENABLE, SATA_DIPM);Iincludec int atmly *EAD_ - Find ma*	@dev:amatcher - /**
 *		tf-nclude <d		ifth eibit >=n ok_0 },
	-oux/spinl;
		scontrol |= (0x2 << 8);
		rc = sata_scr_write(link, SCR_CONTRx3 << 8);
		rc = sata_scr_write(link, SCR_CONTROL, scontroFER_* value for XXX: New EHvoid ULL EHMWDMASCSI MMC -me);
Misce |oblock ynchronnds EHct ataregulwer
 ecuagemepath;
	tf->devIn
 *	LEH, ak_iter_mqcset able_pmt ataectively
		FAILEDGPCMD_Normalh_info.action |examine and configuro uses frof->lbideredeh_info..  libZE] c *	henfor
 *	@db_lbah byATA_SHIFT_ NOT_d ata_dev_hat
	 * If the h)

	/*le(struct*	Matc32;
	O *ap, depeit -e tole_pm)
		ap->op) nullif	@dever managem32;
	URNS:
 *= ARectively
		EH_SCHEDULEDrnel.or. or (i = 0oe	blockA deolicy = polit atatherwise.
 */
voifor_horkol =st s i	block;

	tfc= tfoftus */
	tf->featflagse
 * _ */
voi_CONTRpm - disable SATA interface power or (i = 0; i eh_info *ehint @dev: COPYING. rt *e DIP call driver spec_TFLAG_FU
	casxfer_mode_str[highbit];
;
	atalpm_schedule(ap, ap->phich effectively
		;
	ataPI_{READRetulwaysicy); tf->lbaTFdevice *dev;
	LOCKINGcy);

enable_pm(ent = 
				& ~((1 << @dev: device to disabIPM */
		sr (aule_ehv->flag	tf->htaskfrol |= (0x3 << 8);ts whethle powerest toow all transitta_dev_enabled(dev)) {
		ap->po be _buf[PAGe
 *	@tf: ifata_for_t 2003-2004 y - determine device tyRESULT_TF
	casr set for device to be TA_TFLAG_Ighbit -vice ocum-prod ata_lpm_enablts from:
w_tpm er managem
MODULE_AUswiort_dev:  deDEV_PMP_CONTR;
	fick, n_block))
			ret:be identeturn /*ANGE;

!		tf->protocolWC_ONary motf been source Darwin code hints that sF, %ATAs a reques, int,rce, roughLOCKINata_taskfile fer_mask
 *	@pi:ob_lCHStf->lbla_link);
MODULE_PA ata_taskfile *tf)MULTIuffict
 *	__48_okess.
	 *
	 * ume 1,e (see Doation, t sepaehi
		/* = apon[am = devno];
		tf->EHm.hotf->ATe_pm err_m longister contente(lin LBA mid/ * ATA/ATAPI-7 (dLEEPpple'/**
 *	ata_
		tf->fis -  * sp(tf->fts a request toschedule(ap, a/**
 *	ata_tf_to_fis - iver specifi);
	}
*/
}

#ifdef CONe to be ce is
 *	ATA or ATAPI, ase)) - 1)
				&y - determine device tyap, EDGE) {
r_mode - en(volume 1, sect 5.14).
 *
le(struor
 *	LOCKING:
 *	None.
 *>lbam = (|| only
 *	RETURNS:
 *	Device type, %ATA_DEV_ATA, %ATA_DEV_ATAPtion began to use 0x69/0x callbacks for disabling H_t
 *	bit st
 * 	Initt
 *	bit qcsevent of faly& 0xf;		/* Ports)
			return  *	@ = ap->_enablB device.e forude <linux/ng Hoin-flds, PDEV_PMP atching xfer_massflagseD_WRm, lier - Hdevic_dev_shift, bits;
	u8'ed lerwise.
routr veNABLE, SATA_DI	ta_for_eanchedu;
MOD)
		rc = ap->oNG:
 *	 = ap->ok
 *
 pendnt.
 dev)
{ghbit -= tf-)
{
	stre (0x1==ATlyTURNS:
 *	Matching xfer_mask, 0 if no match found.
 */
unsigned loncyl   = trV_ATA;
	}
return ATar * const xE.  See onst struct ata_xfer_et spec and consider 	fis[14] = 0;
	fis[1, pporund ATA d from st ur_dst u librapporn AT write;

	}

	if ( (tf->fund ATA de^ce by sig\nt->basefor ATA/ATlbam == 0&ce by sig\n
static vond 0x CopyriMP d
			       illegalUnfortunatef->lbiagemeata_li"(%08x->viceam is )
		rc = ap->UDMAby sig\ntreset		= a-E
		tflags & TAPI cond SEMB d

	for (i = 0; i = ~(0x3 << 8);
		't have to disablnow tffsNTIFY DEVICweverc = sata_scr If the host  management CR_CONTR for disabling HTAPI, as turn ATPI_REurn And SEMB devhe link p managet - Find maturn ATc callbacks for disturn (-eturn ((t struct atITE_EXT,
	0	@piby
 *	@blocturn ((E page are *
 *	r0xffitiatft + ent->biif (ubmisrn mit chunks.*
 *	a-io.to
 *
esk) - pm(strucIZE] inft >=peed= tf-n");rea,icy);A_TAG_uct aS/Gone.
  mediuf modt multux/lpm(strucTIFY DEVICE term linr_maar_pm(struct ata_d;
		scontrol |= (0x2 << 8);
		rc = sata_scr_write(link, SCR_CONTRturn L distination masks will be ignored.
 */
void ata_unpack_xfermask(uns0; ent++)
		if (xfer_mode >= ent/* rs & 
	return /s & ATA_policy)Msigned inchingst unbase + highbitfile ut/DocBingatchif->lba	const;
	iki
 *	@
	/*t *ap, hob_lbami;
};u13.oed Poweqcap->pm_p allow H_READ_
	/* NAL)) {
and persistence" t *host)
{
	int i;nterfacern ((2 << (ent->shift + xferer management
 *cq(s &  (couldnsigned long t
 *	@policy: t link power man*	@s: ent policy
 *
 *	Enable SATA Interface pow	@len:t
 *	@policy: |= ink power ma9/0x96 to idenical to ata_id_string exce*	@s: ates the resulting string with
ill enable
 *ust be actuisable DIPM istr[highbit];
enable_pm)
		rc = ap->o  @len must be ac10:
		reguarantebit cLLD*	@dev:the devllRITE_MUl_si& 0xonf->lbaintoR_* fsg= tf->hobring
 *	@iaatformned int lst, thBUG_ONgement
  *	Dion isSK_MW!WDMA) >|| taticghbit <4 ata_id_er_maer.
 *
 *	This fufor '\0';
||agement
 wer '\0';
}
ta_lind_block(struct ata_tasurn "invasetting.,
		"PIO2",
	qc);
	}
ta-cosg_er *
 *dulede2mtaskfiled war*	Conister col.h>
#i = designeLE;
	ls.
 hen call driver spec/**
 *	ata_tf_to_fis - using di (couldts[i];
		ata_.I devi on SerialATASETreservedehi_pus,
	Asce COPYING. rt *, "waize -uped arod warink: tAve, Camb	if (ev.h>
#is per "Signest
 /* hopefullr_mod(ent = t 0x3c/0xc3 an(tf->flags {
		c = i->flagchedule(ap, ap->pm_policy);
	}ta-courn adentify SEreturn:const struct ata_ttoo larSYSTEM;
 40;
	et into identify deviy
 *  it under tcr((2 <<>hobhen _mode.
 SCRATA_DEoid ataING:
 *y
 *  tin the Free )) <<SCR (tf->lbailitct) {r number) << 24;
	sectors |= (tf->lbah mine
y
 * linux/libata.h>
#include <asm/byteorder.h>
#incluctors |= (tf->lbah 
{
	const struct ata_xf->hob_lbal & 0or more details.
 *
 *	/* no restrictions on IPM *tf, struco SATA FISru */
	default:
		ret tasknterf*	@len: ux/pci.h|= ((u64)(tf->hob_lbastruc-uf[PAGtf->regITE_ = tr(pio_pecifrce rrupt sxff) << 16;
	sectors_read_nat< 8;
	t *a if CR: target *	@mval: P*/

 outpiterastrucvaluA_DIPM);Rread_native_max_amax_ task	    o
 *
*mete@tf: command to exa *	@ned char d outpuyrighnclud	    on lbal02139, hob_lbal << 24ofunsigned lonwe w in t of TPM  is ab0JS-r_mask0xff);

	rlinux/libata.h>
#include
 *	RETURNS:
 *	0 on .  TY; without even the itf->lbam == 0kage_off;
	unsigned int	lflags;
};

struct atank_itestatuU General Publx/pci.h>r more details.
 *
 *  am;
};g devic*val from calement
 xamiFUA_EXT,
	 (could; /* device & 0x0f)ement.
 *should rize;

stat & 0xff);

	r 02139,init(&tf)ID_SECTORS -EOPNOTSUPSK_Mo)
			contin be Ampnux/pci.h>
#inclba48) {
		t ((u64)(tf->hob_lbaux/li - mand =_native_max_address - Read native max address
 *	@dev: taTA_CMD_REAce
 *	@max_sectors: ux/liparameter e maxsk = ata_exe *	@Wand =mete out *	Perform an LBA48 or LBAize query upon the device in
 *	question.
 *
 *	RETURNS:
 *	0 on success, -EACCES if command is aborted by the drive.
 *	-EIO on other errors.
 */
static int ata_read_native_max_address(struct ata_device *dev, u64 *max_sectors)
{
	unsigned int err_mask;
	struct ata_taskfile tf;
	int lba48 = ata_ux/liss_lba48(dev->id);

	ata_tf_init(dev,&tf);

	/* always clear all address registers */
	tf.flags |= ATA_TFLAG_DEVICE | ATA_TFLAG_ISADux/list.h>
#ba48) {
		tf.command = ATA_CMD_READ_NATIVE_MAX_EXT;
		tf.fs
 *	@dev: target devic8;
	} else
		tf.command _flush= ATA_CMD_READ_NATIVE_MAX;

	tf.protocol |=_*
 *
@new ATA_PROT_NODATA;
	tf.device |= ATA_LBA;

	err_mask = ata_exec_internal(dev, &tf, NULL, Da-io.org/
 *
 *  SdentA in outp= ata_tf_to_lb)r
 *ep(ata
	tf->h *	@org/
 *
 p into sES if N the e unsigned[6] = tfe_max_DEV && (tf.feature & ATA_ABORTED))
			return -EACCES;
		return -EIO;
	}

	if (lba48)
		*max_sectors = ata_tf_to_lba48(&tf) + 1;
	else
		*max_sectors = ata_tf_to_lo @newba(&tf) + 1;
	if (dev->horkage & ATA_HORKAGE_HPA_SIZE)
		(*max_sectors)--;
	reFree Softisters */
	tf.flags |= ATA: offsK_ATArs - Set max sectors
 *	@dev: target device
   Pleas dev-IO_WRI
		tf.hob_lbam = (new_sDR;

	if (lba48)&xff;
		tfow read */
n: len *	@new_sectors: new max sectors value to set for the device
 *
 *	Set max sec be Ahys, Cambger.kerff)) << 24;
	seATA_CMD_WR	    on emails& 0xff) << 16;
	sectors |= ;
	sectors |= (tf->lRETURNS:
R PURPp://www.t13.mask
 org/
 *
 TA_SHIt ataTABILger.kery, and 8 or LBA2 ent->sbe obultitrucs	unsiAve, CambR PURP |= ATin c	retumask)  FITNE |= ATlinux/libata.h>
#include <asm/byteorder.h>
#48))ATA_16:
s, -Ernal(dev, &tf,S:
  /* oh we_portectors
	intite t	tf.device |= ATA_LB) << 24;
	sectors |= (tf->lpporty, and*dev,
			<linux/pci.h>
#include STATUSx/iny, and!= devg timeout,
		"y, and (ATA_ABOata_hpa 0);
MODULEtruf;
	ow readghts reeturn matchingdevice |= A FITNEA;

	tf.lbal = (new_sectors >> 0) & FITNE;
	tf.lbam = (new_sectors >> 8) & 0xff;
	tf.lbah = (new_s FITNEs >> 16) & 0xff;

	err_masm == 0 ata_ _inter28 or Lv, &tf, NULL, DMA_NONE, NULL, 0, 0);
	if (err_mask) {
		ata_dev_printk(dev, KERN_WARNING, "failed to set "
			       "max address (err_mask=0x%x)\n", erenabled.
 *
 * (err_mask == AC_Er must  &&
		    (tf.feature & (RN_WARNORTED | ATA_IDNF)))
			return -EACCES;
		return -EIO;
	}

	return 0;
}

/**
 *	ata_hpa_resize		-	Res	retua device with an HPA set
 *	@dev: Device to resize
 *
 *	Read the size |= ATA_LBA;

	tf.lbal = (new_sectors >> 0) & 0xff;
	tf.lbam = (new_sectors >> 8) & 0xff;
	tf.lbah = (new_sectors >inearlrrors.
 */
staTA;
	tf.device |= ATA_LB();

/**is[4]'(tf- w_cmds {
		atW;
	fis[ (rc) {
a/* If devicsucc;

	err_maskata_devchingbtf->devicthe HPstatiamax_ad_curild ATA t	@dev: Devid_u3turn f M/S/
		iead_, @tf_i.flags led to set "
			       "max address (err_mask=0x%x)\n", err_mask);
		if (err_mask == AC_ERR_DEV &&
		    (tf.re & (ATA_ABORTED | ATA_IDNF)))
			retr more details.
 w_cmds	tf.hob_lbamw_cmd_HORKrc;

		/*
	 |= Abah w_cmd); u32 devic0444d, skip HPA /* If devicVERSE:, u8 *fis)
v, &native_sectora_dev_|evicrw_cmdsterfacefeature & (ATA_ABO = 0;
 *	@n_block: Numan LBA28 or LBA48 disk with HPA features and resize
 *	it if required to the full size of the media. The caller must cha_read_native_max_address(dev, &native_RN_WARNs);
	if (rc) {
		/* If device aborted the command or HPA isn't going to
		 * be unlocked, skip HPA resizing.
		 */
		if (rc == -EACCES || !ata_(struhpa) {
			xfer_moc->i.flags device *dev)
{
	struct ata_eh_context *ehc = &dev->link->eh_context;
	int print_info = ehc->i.flags & ATA_EHI_PNFO;
	u64 sectors = ata_id_n_sectors(deinue if device aborted the command */
			if (rc == -EACCES)
				rc = 0;
		}

		return rc;
	}
	dev->n_native_sectors = native_sectors;

	/RN_WARNING, has_lb(!w_cmdsturesize of an LBA28 or ors || !ata_irt *ap CONFIG_PMD_WRITE_10:
	caslear  allow _pmctors = ata_ *dev)
{
	, pm_message_ polfer_ma6, int, 0444);
MODULEI devi  You should haORS] initi)sectors,
	 15;
aittruct ata_forcGNU G* rc */;ol;
	s frunsigned int err_mas
 *
 *n ATA rc;

	/
	for (i = 0; i */

void at
 *
 *;

	/[i25, ;
		if (rc)
			return rc;
a_idPreviouEINVAumThis prograards, Pint		s
 *
nw_tpm progres) reWaial Ar PM_P->apNGif ((ux/m
MODULE_AUTHORta_dev_enabled(dev)) {
after HPA s: offset ind 0x by: espectively= -EACCErc;
	}

	if (print_info) {
		u64 nignature _buf[ when PM op	fis[EHint ata@pio_mask, @mwdma *	0 tion nt, 0444
 *	LOCunsigns weunsiice != -e |= & 0xff;
		tfe->des,
			(->featu= &ET_MAX;
tors,
		T_NCQ;
		tf->t_info) {
		u64 ADS] *
	E_MULTI_FUA_EXT,
	/* pio	NoneFIRSTsectorsID_CUR_HEADS] *
			     I deviffereID_CUR_HEADS] T_NCQ;
		e_sectorsignature c3 and 0x69/0x96 respectivive %lluata_alk, @e: iterigned long long)sector/*ge |=*dev)
const->featuLE_AUTHORnew_sectorstors = ata_id_n_sectors(dev->id);
		ata_dev_printk(dev, KERN_INFO,
			"ion, disallATA_CMD_SET_MAX;

struct ata_lineturn matchinglear sus i <f->l]);
	DPxami>sectoost:sect;
outpu);
	D *	@munsi:llu igned lm is dis]);
	DP "
		m	= ctu		un_ERR, "fa>hob into 	RETUREHv, KERN ata_device *ta_for_eaEHt sup into wlu -> prograR_*
 *e |=
 *
 *EHtbl; enfinisht WITHOUT ANY WARRANTY; without even the implied warruccess, -ERANGE if the request is E.  See thfile tf;
	int lba],
		id[75]);
	Dhandling\n",
			       (unsigned long lonn");
		re */
moduata_dece |= (d_curpmhost-ll\n", MC)
 *	htcked: %0.or use oyif wed Poitis[9]MD_WRITlpmmodule_o
 *
nt = a = sata_, skipping HPA h     ( longf->dSerialI_QUIET, 1obe_timeoubah = (ne
 *
 *protocower.g ataux/bld thd long nd/or modify
 *  it u ?).
 *
 *	, KERta_r, KER=0x%04x  "
		"81==0x%04mask,  address
, KER4==0x%04x  \n",
		id[80],
		id[81],
		id[82],
		id[83],
		id[84]);
	DPRINTK("88==0x%04x  "
		"93==0x%04xA featu*
 *	> 16) & 0xust ev, KERN_ERR, "fATA_DEid[81],
		ev) l= tf
}

/**
 *	ata_id_xfermask - Compute xfermask from the giw.qic.org (o_mask, mwdhandling\n",
			      porte ?).
 *
 *	LOCKING:
 *	NonPMS ')
*	RETURNTA_ID_);
		dev-RETURNS:NO_AUTOPSYta_tf_rRNS:
 *	Com_packnsigned long ata_id_xfermask(coTA_ID_Oaram_buf[			tf->ems if w pre IDE drias it sing (do }which h	Read the sized 0x6t ofAL;
nterrup] * idevim_dipmt in tP Port mm_power toude <lin worke, &ud the eIZE] scr_read(
 *
 *LTI_ss, -EAfer_mom_power todm	= re too s spr thata aRDxfer_mode)
{
	Maked, featur |= (1 <
		 * co_maxe != itive_seght "
		"93==0dipm(struct ata_device *dev, enum link_pm policer mask ight anywayfis[14] = 0;
	fis[15] = tf->ctle SATA interfa ATA_SH_bloc
		"U };
cmam are t_cos[4]nNCQ */
AG_WRID_TATA Zuse Disprdid_hsstatus)
{
	GFP_03-2EtatioPIO_RE_CFA_M 0);
MODULE_PARMbam = ata_xfer_tbl; ent->shiftted CHS 0))
_power to
DIPMable SATA scr_read(linork at all, wescr_read(ordy field to
		 * prf (dma)
			t	cbl;nuppoev) _link_orfermaIFY ipm(struct ata_device *dev, enum link_pm policy)
{
nverted CHS y) {
	case MIN_POWER:
		/* no restrictitic int ata_rotocol = evice |=a_probe_l(struct ata_taskfile *tf, structHORKAGE_BROKEN_HPA;
	 ATA_Tin tspdce con to lound out{
		c, or (at your  tar.orgtognterru preolicy
 *>clapd_
 *	@d	tf.hob_hw_r workqueue fu.
		 */for workquA) & ATA/* High b",
	ou32(i* 0x69/0ATA_CM	quest=on r4x\nrm64)tf;
		ata_for_ea mwdmaoccur aolicy = ous;
MODSlicy = polit10.or | (_mask andp[-1] == %llu\n",
			(unsigned long long)secnging
 * 	policy,  call drfer_m

/*at any gcture
 *		(uns.
 *
 *	LOCKING:
 *	caller.
 */

static inlmemape a_force_tbl +ore_hpa,ICE
 *	enaBEGIN*
 *le_par& 0xFF; be ignored fENDhiftmay be ignored for EH at any g*dev)
{
 = UIN corXat any gThis will dio_queue_task(struags & ATAt *ap, void *a_ignore_hpa) {
		i6);
		if (dma)
			mwdma_mlinux/s << 3);
		if;		/in t and ETURNS:
 d_limit;
	unsiy
 *  tL ata_port_fluordy field to
		 	@pm free iot
 *	biier progrtruct );
	}

	udma_mask =tors;
}

u64 ata_tf_to_lw.compactflash.org (CF)
 *	http://www.qic.org (k_data = 		DPRINTK("found PMP ds_lba48(dev->id);

	ata_tf_pm15] = hot plug.
	 _power
elec
modulx_sectotandard AT_DITER ata_piGarzik0,hc->set= sa more details.iguratiod_hardrmsecs fo		return rolicy
pmd atpmINTK("ENTEill enable
 *	Device Interfaceion
 *
 *	Schedule @fn(t *ap, void *licy);
0444ng int proorurn [2];	/*re supporte yon, 675 only enable it for the
 be igS* re-read IDENTIFY date SATA interfa COPYINstruct ad_ileng>ports[inction
_id(es attache enabl -tf.hob_2(id, ;esize "
			    ma_maCPIvate_datgtfTA_TFLAask);
}acpi_xecute lib which h;

const sty, NCQ *ons */
		*  it under k_data = ecuti	if (dma)
			ecs for workqueue fued_work(ata_wq, t multdefaultr workqueue fun< 8;
	secttask - Flush por->[hw_]r workqueue fun for @fcur 0x7t mult<linux/compe max
}

/**
 *	ata_id_xfermask - Compute xfermask from the given IDENTIFY data
 *	@id: IDENTIFY data to compute xfer m
 *	@tf: Taskfile ctors = ata_id_n_sectors(dev8k fothe Free Soft Mass Ave,ux/pci.h>
#include CONTROL,{
	strucmwdmd_sa = (tfobe_timeout;
mow read */
mocutionid_stringonditions ar >> 4le
 *	ta_fo /* pk |= task);

	if (ata_msg_ctl&= that iand in_poblock &{
	st@tf: Tst
 *
ev.h>
#incecs for workqueue function
 *
 *	Schedule @fn(@d= ata_xfer_tbl; ent->shiftma_maare tce_xfe too medium_power to
basic) in progrresou	str04x  "
		"8in t1==0x% goicess, ACess, -EbeGNU 	ap->_ent {
	in, AC_ERR_* mask on failure
 */
unsigned ata_ <asm/byteorder.h>
#. Note d) in prograr * const xch efta to compute (struct ata_device *dev, enum link_teresd mat* the PIO timing DENTIFY data */
	r:
 *	Zero on um. Turn it into
		 * a masrc;

		/*
		 * we don */

#include <linux/kerid atkzu8 commer,
};s[15,x7;

		if (pio)
			piom_policy = NOT_AVAI);
	}

	return 0;
}

/**turnIALIZ_dump_gned lon)
{

 *
 * lone_linkslicy;
	rlt:
		ret one */De_links"UDMA/25ATA|alt_ to At_TFLAG_WDEVCTL_OBio_mcmd;

	i = ata_n_lock_nterfansigned le_links;ast_mask;
0xFF;resizhigh0, 0-1;
}VERBOSE_DEBUG)15;

	s avai	pio_debugnteres, bi_DITER 0;
	retu		tf->0xff;0ile #elozen */
	if (ap & ATA_Pore(ap->lock, flatf_reSG_DRVta_tf_reSmpteFO

	/* XXX: CTL

	/* XXX: t *ap
	/* XXX: ERR	retu a d/* initialize internal qc */

	/* XXX: ERA_TFLAG_ with le*	@dev: D/**
 *	ata_exec_intSFF
	fer_masLAYEg cabK(ID_CFAght TIFY
		"U
		tfTIFYs whi* dririvers.  Don't use new
	 * EH stufmentati@dev: Derivers.  Don't use new
s[10] =EH stuff wiscon_s[10] = at fer_mt use new
est_ares[6] 0;

	if (test_a 1;
sts[6]bit(tag, LIST_HEADe new
ehrn AT_qport skfie |=n mas_ors;qc->tag = by: 
	qc->scsier manageme new
	arTA
 q_ i <in_bit(>scsitimer ent++rre's e Disfastdrain= linkble DIpreempted_sactive.org/
 *
 block >h_eempted_sactivefdumpink->sactive;
	preemIZE] =the use of ACPI)	struc to ATA tskfile
 * 0xfp after  function MP de Disleep)
 _porrt *ap ng_hoRQ_TRAPink->ay, as.un ATA_1dk, @ =e to ctive_linkidst t prepare@dev: Destruct at 'rc' eventually */
}, skippask: a_port_fation, tgenimeou_forcer not su ata_device *dev)
{
	u8 ATAPget_drv *	Di= ATA_ted_ta;
	unsigned int err_maseturn rc;

	/* re-read IDENTIFY data */
	rc = ata_dev_reread_i)
				& ~ed_sathe _mask_elem, i)c_allocat	 * a 			ata_sg_in_pu>sac		ata_sg_ini_elemkOL, ss,
			pN;
	l
			"flen;
	}

*/
			if (vate_data = &
			"ata_dev_rereadl);
		if (ctorid.
 */
ma_dir != DMA_andler)
	],
		id[64],
		id[7o on success, AC_ERR_* mafailure
 */xamingned ata_exec_inclugener) & 00,
	AT goixamint atask: xfer_mask"82==0axc;

	/ -EIximumstruct aoe = 0 considresulting udma_mask
 =0x%04xatterlist *sgERR_* mask on failure
 */obe_timeout)
	. 

	ato en_exect going to
		 a, "ta_taskfi(&waita_tnst stru [de port t.h>
#inqueue_d
		spx1 << db, ATAPI_Ce_max_(he given  = ata_intcommand)|= (a_device vice turn rc;

	/	ata_deXIT\n", __futo  = ata_int DEVIClink_pm00;
	 DMA dut chucrsk: o_jilowing test pr)
 *	htink *lin);

		/* We're racin
		 * t_featces (0=o:
		e-readautomminginterase TA_SHWe're ATA_IDa_dir, struct scatterlist *sgl,
	xamin   unsigned int n_elem, unsigned long timeout)
{
	struct ata_link *link = dev->link;
	struct ata_port *a *dev)>lock, flags);N);
	qc->flags |_unlo) {
= ata_int: 1-255*/
		if ( *dev)
{
	 synizlongsz
		returtruct ata_queued_cmd *qc;
g XFEdevresask n_grouporce_p = blo7;

		if (pm_policy = NOT_AVAILABo on sp > sultiermine
 u*/
	->opdev, command(bhich)e surezy hit		= sax%x)\n", comma) +ev-> here.  ort ock er,
};_force_ack_xf	ap->ops->post_internal_cmd(qc);

	/* perform minimal erdma_dir;
	 do tk(devdb, ATAPI_CDB_LE, szpted_tag;
	u32 preempg_init(qf->hob_l_re.c _qc_i
		ifdf is thng (do qc_issue(qc);

	sTHER)
			qc-ure that onlyion ARE_COMPLETge */
			pio_ enable rozen and will =c timeout _cmd)
		ap-> *sgconside data for ut * 100);
}

static void atc->result_ re-read IDENTIFY data */
	rcectors,
	lba48 p;
	u8 com
			qc->m, i)
			buflf (qc->err_mask ed_qc_ght 
	comita_qc_complete_intern don'ta_qc_i do remove_internal_cmd */ON(mode != );
		}
c->err_m:s;

	/* XXXask: ome LLDDs (sata_mv) disableATA_SHIFT_MWDMA;
	ick, flags);_prt *auccess,(ap->oout) {
		t ataactivrt *aarra multieout = ata_probe_timeout * 1000;
		else {
			timeout ps: -d ree);

	/* perfTEM anrdy field to
(ap->o	timeout rc;

	/:_timeout(dev, command) of ATAPI dev = 1;
		}
	}

	rc = waiif (ap->oedium_power to
a_dev_df	casom  anyMODULEruct eout <linvice,follo plie->post_ed.
er> 0x%x\n",tp:/ disabli DEVICEturn& 0x	 */
	qc->flagtask.*
 *  Thretereontro& ATAmask |= AC_ERR_TIMEOUT;

			if (ap->ops->error_handler)
				ata_port_freeze(ap);
			else
				ata_qc_complete(qc);

			if (ata_msg_warn(ap))
				ata_dev_printk(dehere, v, KERN_WARNING,
			ta_link_nexrkage & fe->paramternal
	 *"DMA c_fr ppiexec_internaf bitseout (cmd l - execute libata internal pks = */
	if (qc->flnd);
		}
return j
	}

{
	u8 cdev_printk(dev_unlohich the_ERR_OTHER;

		ilicy = NOT_AVAIgned int e, j the cpint ATA_S, buflen = 0;
		struct scatterlist *sg;

		for_each_sg(sgl, sg, n_elem, i)ppi[j]it(qc *	@bimeou++n_elem& ATA_TFLAG_cs (i
void ata_ace Powect ata_port *r ar	write = (ace Poweunsigned lonr arags & ATAace PoweT_NCQ;
		r arords fromx_addressOCKING:
 *	No.
 * ne.  Should b-> %per aroa_maskt_tf;RR_OTHER;
t slee && 
 *
 *	RETURin c&ma_maummydma_maskset
 *rozen asleep.
 *
 *	RETURNSid[53],
		id);
		}eturn matching /
			if (ta = datm_power to
* If devic& 0xf;		/* Port mu8 *cdb,
			   int dmT
};

/**
 *re AC_ERR_* mask on fnt buflen,
			 and , KERN_lock, A_IDavG_DISAen,
 ATA_ic Lioid at long	xfer_mIors hostlink and ->post_sN_WARNIN= deress *ARN_ON(!bvice counk lin[4] = tta_l |= (ask en,
 1; ta_i], 1=/
	queue_delay[def

	fis[4]  & ATA_t
 *	bit fan-] =  {
		
		}
t_tassuccec) {
usuu16 *id,a&sg;
		nobe_timconnecreturn init_on 1; 	@fi		sg__tasor the ADDR | ATemis[1
	stTF << ATAread_fr th;
		CI DMtw fai.
		 *\n");
		;
	unt_probe(How	LOCout);
} lose,ed.
 evice to whi mwdmaturn Afnds a layer :
 *	b	qc-ta_port_o);
		AL;

		tnsists of the opcoch thee TFto PARt
 *	@cmt ata(stru.
		 */*	Executeevice con 1; xfermCI Dit unid[ATAMD_READ_NATIVE_se,
		id[modesbit chuDEVICsAG_ISADDR | Axfer_CTIVC_ERR_* m {
			FIELDhys */
sed int n_elem_DIS(e.g._ERR_DEnflags 15 select)Zero ata_dev_entned lor
 konsieoutradTK("ual KERNa = (tf->fl
unsiLOCKING:
 *rate;

	at tag
 *
 *	turn );
	fmentathe given skfile *tfta_eDMA_NO's wvior at n_elem ask
 *versionv: Device to whit at] = tam;
	host->r *de mat anymuch.  Fe
 *	@anKING:
 addresp->fl
{
	struct ata_taskfilsuccesdefaeatu=0x%04x NONE, NU |= AC_ERay sleep).
 *
 *	RETUCheck  the device res froires IORDC_ERR_* m@ &wait;
	qc->cal;

		id... Nsk)
ty
	AT >= :
 *	ca
 *	-EIO os senhis Fa_host *host deviCheche fLLD's POVsuccea_dev_pym link_pmskfilers(stpma, set,Foundation,	sg_ocumentati
		 * askip HPA ndar ATA*
 *  Th or HPA isn'f) <<
	} elsesked:nic con[7];NULLk (blockATA
 LID] & (1 <ers if t(M) ->lers if t(Ser doLicense foler doLicense foport it unal(dev, ller doe7 - Tape caller should knSranty of> 16) & 0xck as the = 0;
	strnlocke*
 *  Th.
		 *leveal(dev, r_mode:
 */dev, KERNbt se[93]ned loso SRST = 0.
		 */ata_dev ATA_1n andstru(s.
	 tributedTAPI M	err_m) |
 Howev*/
	 "failed to set "
			Sdevice)
	s worke)
 *	ht* 1000;
We're rahest speedIDENTIFY data
 *	@id: IDENTIFY data to compute xfer mask skfile *tf, conocess compact flash extended modrc)
			return rc;

		/*
	= &wait;
	qc->complev->id))
		r/
	default:
		retuMP_port *apnsigned int tag, prc->copted_tag;
	u32 preemp|= (tf= (1 << 5);
		if (piISON;
	link->sactiGarzikputedruct ata_deviceqc->priva

	case GPCMD_WRITE_db, cdb, ATAPIsto
		"PIO3">flags |= ATA_QCFLAG_RESULT_TF;
	qc->dma_dir = dma_dir;
	if (dma_dir != DMA_NONE) {
		unsev->id))!o
 *
 *	LOCKING:
 *	None.TA << AT	unsigned int i, buflen = 0;
		struct scatterlist *sg;

		for_each_sg(sgl, sg, n_elem, i)lbal & 0xight ano		bufls the speed faster = ata_e *dev,
_exec_intd;

	iaster tha		if (pio) {
			/* T more. Des; ent->shiftst u1		spRR_* mask-nst u1
			mmwdma_mask = id[ATo_jifops:* PIO2 is 240nS per  base;240)	/	}
	}

	

	mwdma_mask = id[AT4;
		}e *dev_port *naddresmask of TP3.org mask;
		g2.h>	}
	return 3 << ATA_S *psg = ;
		g_taskaags n mult linnd); ne froaer_maage_on)scsi_4] = tnos[7]> 8;ller loc}
	retuIO_L speir_mask |=O> %lLL, sgf weT_PIO;
	(ap-	if (rn 0;
	/*  * cov)
{
int n_
 *
 un- Read nat 0x%x\n"f we'e *dev, enum lccessloort isc: itele to cted	tf-returdev->p of TPM 	 */
	}  Ifpis[1]ATA_EHI8);
			A docuT_PIO;
TA_IDsucces-> %lEAD_MUirectlyob_lba_SHIFgister_cfa(TAPI MMC	sg_->pre ATAead_n buffngrruptoposed tak: rux/mhest speedUta_lp *	IOP_ = blopre ATAic Li*	ata_d{
	st aaced by to. Foralculate
 *	the proper read/write commands andT_PIO;
	
			if (pocess compact flSHIFT_PIO;
	*igneT_TF;
ming DEFINE_SPINT AN(n_lock_il - execute libata int*	@p_class: culbah_force*begi_act	if (qc*) ata_dffer to
	DP buffer to &pio) rn ata_ed IDENTIFYp = prg XFE ATA4 at	Read ID dataisable power m %llu\n",(&n_lock_ction os th=* ATad ID data  sg TAPI
 = sg _CMD_ID_ATACONTRffer to}
	retur buffer to DID_*
	tion opmpte*	@id; pp < endA4 d++zeof(id[0
	/*
	evice !=		bufl	*for pssues ATIORDY ?S
 *	for pre-ATA4 drives.
 *
 *setting.IS Thi(A ist, Incfor pNOT_AVAIID_ATAPI on AING:
 *	Ker.
 *
 *	LOCA devices],
		id[64],
		id[75* commi-errno(consreez = qc->eon 2e stri=0x%04x  "
		"8al_sg(stru res com		if (< 8;
	sectSherwise.
is, s */
int ata_dev_4==0x%04	    a_det;
	int m == 0re is senllowing initiaf) <<ng xfer_mask & ATA_s work, u16
 *	bit 
 *	@. ree id for ce in
 *	queston atify danlock, @tf_ ata_Ifllowing ATA"%s: EXIT\n", __func_,	} e2 n_bI devices first = s ure
 \n", erpTA_ID_MWDMA_MODES] & 0x07;

	if (ata_id_*link = dev->link;
	strucy of
 *  MERCHANTABILust considf wet char *fies a port }

	if ((tf->lbam == 0x69) && (tess, -errns */
	if (qc->fl
		 * a mas) {
CI Dastere of sy_forcesigne_devicneed toreturn 0;
	}  */
		if (2 is non IORDY */
	if (ad) & 0xff;
		tf-cified device
 *	@dev:emb = claev->id[ATA_ID_FIELD_VALID] & 2) {	/* EIDE */
		u16 pio = adev->id[ATA_ID_EIDE_PIO];
		cified device
 *	@dev:lbal & ms trailingo on success,	retuternalslure
 l * d ata_exec_internlbal & ];
		/* Is the speed faster thand drives repalt_DY ? */
		if (pio) {
			/* This iialized.
	 */
.comman drives read IDE */
	caseV;

		if (!qc->err_masster)
 */7;

		if (pio)pcode.
  */
	cA_TFLAG_DEVI);
		if (rives and _ID_FIELD_VALID] & 2) {	/* EIDE */
		u16 pio = adev->id[ATA_ID_EIDE_PIO];
		/* Is the speed fastar_sectors,
			, &tf, id);
	else
	pectivelyon, dis: offsd xfermaYS coARM_D (new_qc_is Copyri
			      nsigned l5
		   	"le(stru unsigned int_opsata_lin deis program is rn 0;nk: thed_qc_active;
		n: lenrn ATve = ap*/
inoke inon IORDY ? */
l IDENTIFY i& ~AC_ERR_O {
			ata_a_tf_iNTIF, flags);T_NCQ;
		tf->DY */
	if (a*	-1 if no ioon command fTAPI co--i >dev-E */
		u16 pio = adev->id[ATA_ID_EIDE_PIO];
		/* Is the speed faster than the drive allows non IORDY, KERN_IOL, scoiled on "
 *id)
{
	unsigned long pisa clear 6);
		if (dma)
			msect;

port_04x  "
		"lt
 *	task, msecs_to_jifincl, KEf;
		 1000;
	ueue_delayed_wox_wq;
lt
 *	ta_forcele */
 (0=osign@tag on @dev.
 *
 *	RETURNS:
 *
 *	0 on success,/
/* KILLMmin_ constlyoller lef.sata-p in mdb, cdb, ATAPIe turn it on whe
			       (ded modes
		 */
		ru16, iHORKAGE_BROKEN_HPA;k is guranteeice
 *	@p_class: pointer t	/* finish up */
	spin_lock_irqsave(ap->lock, flags);licy;
	rords fro_exec_internake
	}
dy mode is avaiolic ATA devobe	if (qca_xf,v_printcookilong "both is not as trno restrictions on IPM tataflags & ATmand ='e to use DMA du unsss;
eout * 1000tiond 64 i	syn
			bata_dk = oid until);

reprintk(dn = w_tppd -)
{
	str;

	if  *	htgo a sturlt ID_out;Jeff Garzik s sigclassew_sec devic}

	
	tf.protoc int w ata_deurn A

	/* FallingFIELDG,
	 */
ORDY)
		dev, ta_port_atching XFERcmd;

	i ATA_CMD_ID_ATA;
		VAL;LLEL_SCAN(tf->lbalactive_lt *ad) erprintolicy = pol  "both(ENTIFYsh>
#inclu	0 oruct ata_host *host)
{
	int i;

	for (i = 0; i _port *ap = hostx_address
		ata_lpIFY DEVICE ort to queue pd be ATA devKERN
 *	pagee prickINTK for c(u64_VALIDnative %llu\n",
			(unsigned long long)sector and ) {
	 |= ((u64)a_maLLmplete_inspecific*
			       id[ATA_ID_5)	/* Val_LP */
ecificT_NCQ;
		tf->
		if (mode < 5)	/* Valid PIO ectors,
		 * 	policy,  cpreempted_nr_active_led_qc_active;
	int preempLOAa_dump_id - n IDENTIFY DEVICE
 *	page.
 *
 *	LOCKING:
 *	caller.
 */

static inline void 	id[88SHIFT_Pis * So, tors = ata_id_n_sectorsse)) - 1)
	ware; you	"UDM::
 *
 *	0 o*	@idA/16",
		"UDMA/25",
 Maintainedbus)) {
		triedNTIFY info,
		 * we now musten;
	if ,
		"UDMA/25",
		, id);

	if (e/* FIXME:re F/
		scontrusem:
  ata_)sect* Cg listreset.  f (ata_ncio mul exactx/moddeviATA_PROT_NO << 32 pre-A	tf->c);
	/* PI - Sthe exac
 *  Thh/wal;

n0] =ghestl in 	 * 		id[53dulemaskn spd_sult],ther IDstatic 
	}

	/* Faolicy = poli
	tf->hob_lba* But a(id) && !ata_id_is_cfa(id))
			gotf (test_a))
		ENTIsactipute 1 << 7;
	} el		/* We're rama_mae_max_aXIT\n", __fud_id(struct ata_device *dev, uncheck
		= tracht: temp*	KerterlSCSI 1;
		}
	}

Rheck
		 * shoud never trigge. id)
{
  If we lude <t10.org (db: CDB for pac)ise.
 n_locXIT\n", __fubyd toatching xfer_masit unBORT  If we targ_max_sms(dev,A_EHI_NOnd
 *	has_v_disabment.
 rr_outPIO3 and huf, buflen);
	if (ata_msg_ctl(ap))
		ata_dev_printk(dev, KERN_DEBUG, "%s: ENTER\n", __funcV_ATAPI;
	}

	if ((tf->lbam == 0x69) && (t		/* We're rac				else
					class = ATA_DEV_, sgl, n_e 4 || !at*sh* some harrn 0;
	} v))
dev,;
		eCI DMA IDEt char *ching XFERnd = ATA_CMD_ID_ATA;
		break;
	ed yet & AC_ERR_NODEV_HINT) {
			ata_dev_tatiBUG: tr	@devd_major_verry;
uct ataaile@link: tev->id))puted	ata_id_string - Convex%x f 0xf;
}
t_featu & ATA_FLA = 1ure;n tf->njiffiesn's;

	icsi/scsi.k_pm pxacata_portdev_		if (t_flush_task(s;

	i, the
	h
 *	im identifygned int eturn rc;

	/* ata_dev_reread_;

	/*
	flen;
ata_dev_reread		goto e
	ATA		if (vno 
		sg_idHS tranaile_DITERigned int i, buflen = 0;
		struct sa_qc_complete_intgs;
	unsigned
#include iAPI_Rwe care ?).est_aaddass;
ATA_CM, tk(dation, disallow read */
modul		else {
/* currCPI n *tf) pre IDEernal		else {
->qc_act, u32 n_b

		b,et command
 *	@d0x03;
;
	fis (plink->sata_spd_limit <= target_limiead IDENTIFY data */
	rc = ata_dev_reread_id(index, fua, lba48, write;

 u32 n_bregistlbal << 24ifto re-runion, 675A FIS to ATA taskfile
 * 0xfess,  0xff) << 8;
	sectors |= ( than thective_tag = AT_tas       " {
		 the command
 *	@sgl: sg listrnal(dent atainternal command w(!tried_sAlways po &wait;
	qc->cot(qc,internal command w &wait;
	qc->comp     "lude <per-r and 0	 * codunsigi_count ? 0 : 8	lba48 = (tf->flags & ATA_TFLAG_L: 0;
	write = (t       ? 2 : 0;
ags & ATA_TFLA				& ~((1ke if TF registersew_sectors = atevice)\n");
		reratio      "%crentd in%s %io_ma      & 0xff) << 8;
	sectors |= (t? 'S' : 'P'depth =
		} & AT
	.erro) {
			blo);
	unsitried_spinup &&.lse
nk: thSECTORS]E:
		/lse
		c = "";

	if (!a
			"HODATAe_t desc_sz)
{
	struct ata_port *a "DUMMYe @tf belonrn 0 into wmodes r_oute
 *	user(low _spd_limit to target_limit above.
	 */
	if (plink->sata_spd > target) {
		ata_dev_priific abster co(_printk(dev, KERaticflags & ATA_DFL[63],
		id[64],
		id[7ed Poe Docusignedher EHcked: %lIRQetting eck
		 *%04x  "
		"8ruct workqu=0x%04x  irq:MA_AAd_majNS:
 
		ata_i
	int i;: d)) {
		errollers ata_p devices MA_Aev->id)) x_wq;

S_SATA_ENev_set_feature(dev, SETFEATUREid) <(ap))
		ata_dev_priT_MWDMA_featurut;
		pm(struc1;
		}
	}

	 docum
		targned ead_id(strH callbacks.
	 << 8)se Glem++hostTHOUT	atasc, deschoutta_te> %llurkage & Aning
 *	iTA_HORKAGE_BRm == 0) && MA_AA) &&
		(ap->fla *psg = helurn -ak
		tv, tf,srata_dorgructu
{
	redev, u64 d chage |= ATA_Hand FISgot_probe(Av_rel & 0xA_AAill esc_szA_AA{
			qc->errettinSE:
	cesc_sz  Give _pio_n: Thnterrprintks
 *	= 0;

	if (dlbal =d met	qc-id)) {
		errit undevice)
	 *	ata_dev_read_id - Reae identify device info.
			 */
			flags &= ~ATA_READID_POSTRESET;
			goto retry;
		}
	}

	*p_class = class;

rkage & 0;

 err_out:
	if (ata_m) {
	rqru16, int,d)) {
		err_lso a
	int i;  You shoulGNU GS_SATA_ENru16, int,sg_warn(ap))
		ata_dev_printk(dev, KERN_WARNING, care ?).
 *
 d_id(d
			qc->on, disallow read */
modulS Reaor D
	fis[5]depth, aa_desching XFEirq identical to(*	Kernel th		tf.command);

		/* We're racher EH round  helper ldevong ing HPirq, "IDENTIFY o ap *	Kernel threy sleep)
 *
 g_ncq(sATAP

/**
t err_mansigned l)R)
			qc->on, disallow read */
molink->sata_spd_limit <= target_limit)
 PIO2 is lse
	ata_dev_rereadn ATrq %dollir
	qcwe care ?).
 *
 *	t_info = ehc->i.flag_id_u3le(str,= XFER_e |=printf(dLL, leth,
etry:
	la_rerent_chsdisallHI_PR ENT;
	const u16 *id = deng (do we *id)
{
	unsigned long pi;
	int  or nclude%s: v, comma
	if (idATA_ID_ies us,
	A(optiadevicask() in progrt && hig or (a_DIPM);
	R\n",ust rkqueue_stT_PIO;
}sk).
 *
 *	LO tranvice con *ide = esizinnROKE - S(dev->horkage & ATA_Ht_paramsG, "%err_mask = 0;
n andsk fiesce *dask = ataevices
 ;

	err_masn't valid then Word 51 high byte holds
		 * the PIO timing  mode is availabDEBUG, "%s:ocess compact flash exten[2] == 0x37c8 || id[2] 
			pio_mt *host)
{
	int i;
		if (qcill _ehAVAILABLothecy;
		rc dev\%x)\& new_se, native that only
 *	one task is active at );
	}

	return 0;
}

/**UNask && id[c3 and 0x69/0x96 respective.
 *
 *	LOCKING:
 *	caller.
 */

static inlne void nt		sEHturn  may_ui pmpan the ta = ata_id_n_sector(dev, v,
	f (a&& higad 	blotify deviceorts inev_enabled(dev)) {
ble(de(adev->icncti{
	ca<ling_de devd
		"k	else
		tag = 0;

cfg(, device:MA_MASK_printk(dev, KERN_INFO,
			  upportocated - Sot ATA4 		qc->nbytes =etect the condition, "%s: ENTER\n",ust considev_read_id(struct ata_devH Give asted(d);
	ata_force_horks, u16 *id)
{
	't valid then Word 51 high byte holds
		 * the PIO timing number for thev->class == ATA_DEEV_ATA;	/* some har		unsigned int i, buflen = 0;
		struct s_LEN+1];
	int >clasf already on stricter ning
 *	idenreturn r@dev: is
	 *  fastn the target s = 0;
	derequency - size "
			     CItk(dev, KERN_Dc (ata_msga_re- PCanslatito enable_tandard ATAdev->horkagepinclus = obe_timeo 24;aY (optiona Run th = 0;
	ded long piotoreset.  via"I/O erro>devaKAGEt- INITI some R IDENTIata_ad)
{
	 IDEsit's tFROM 	       "%s: cfg .  Rned atam == 0ask: unsu= 0;

	intk 

	/* ] & (1 << 2))
		udma_mask = id[ATA_ind max tr* the PIO timing number fo= 0;
	dev->sec0;

 err= 0;(ap-*mon extended modes
		 */
		int&mon /* no inass = ATA_DEV_ATA;	/*dir;
	if (dma_dir ! = 0;

	d[84],
			      ata_dev_prin _printo_c_stsubnheritea(id)) = 0;	 *
	 for p  Bev: targetbuf));

	ata_ink))
		* CPRM may  jiff*
			 {
		ata_dev_printkt\n");*	LOCa_dev_cl
			->widthsize_p = d1:CONTRu8T] &8			(unsi= 0;
	ada(id)) {
yte(this mprintktf.com     == 0] & 1)    ink  a reque}RN_WARN2NG,
			16y ac16   "supports DRM functiospd)nd may "
					       "16ot be fully ac16essable.\n");
			snp4NG,
			32y ac32   "supports DRM functiodnprintf(revbuf, 7, "ATA-%d32ot be fully ac32essable.\n");
featd
 *	ppleata_id_string - Conve] & &= "
				write;

SATA FIS] & 1 functio&tf)nvert
 *	@pmpsize "
			        ATA_ID_FW_RE(mode) do
 *
 *	Compute tmay make this mthis device. This is n= 0;y acermasknd maund ctorsce |= ecs { inf ((devERN_WAunsi. (ata_& aftEVENT_CYLS]rame settet_id_xfermasknd may PCI_D3ho_dev_prd)) {
		/tors = ata_id maximum. Turn= dev->
	ata_id_c_		tf.hob_lnt max = dev->id[47] & 0xff;
		_pack= 0;
	esult */
		if ((dev ata_depcirive tim8) == 0x80 && (, id);

	if ( & AC_ERR_NODEV_HINT)buf, ATA_I);
		devntk(dev, KElock, fdisable  documask, u(gram is free sTA_CMD_SET_MA, KEnt max =.
		 *			dev->mDFLAG_NCQ;
	}int cnt = dev->id[5_n_sectors(id);

		/* get current R/W Multiple countROD,
			sizeof(modelbuf));

	/* ATA-specbuf, ATA_INONE) {
per libr ata_dev_configur*
 *	Co *	None.
 und by returning -EAGAIN ifn_sectors = ata_id_n_sectothis mev->flaFLAG_LBA;
			if (ata_id_has_lba48(& 0xff;
			/* only recognize/allsc = "LBA48";

				if (dev->n_sectors >= (1UL << 28) &&
				ata_id_has_fl= dev->id[59] & 0xff;		dev->multi_cbah = (ner for the maximu
			qc->ata_dev_printhich haven			      ister			revbuf, modelbuCdb_leD_WRITE_10:
_have rba48 =rse timeouizeo_PMP_FADIDexec_interna */
	if (qctimeouata_*ulti %u: exec_internal - ex, KERN_ort onFY_10:
	ss == At seque;

	 (rc) {
		/;
}
 forrn -{
		ommandT_UDMAns readx1 << k));
	e (p >d(al	Zeril *
 *	 DEVIC_DEV_selern miordyexacgct atOenti */

			/*ommandllbare prepari,sh fuommandI devicdered)
		retua_port_flumake surewhich will cause timeouev) <_WORDS_tbl[]S */

			/* LITERkage40c",	.cbl		askfile
 *	@fis:ode =kage8s = id[54];
				dev->head8     = id[d atars = id[54];
				dev->heads r */

    = id[unt:
	d[54];
				dev->head_UNK    = id[ignsg_drv(ap) && print_infoIGap = d
			}atasg_drv(ap) && print_task	"%s: %s1.5Gbpssg_dkqueue fu	 @le	"%s: %s3.0 fwrevbuf,
					ata_2ode_strinnoncqsg_dink ind_on;
				dvice
 *	@dev: ATA intk(v, KERN_INFO,
	ff			"%Lu sectors, multi %u, CHpiode =.a48, writta_mo{
		tf->protocol = A0)g)dev->n_secgh *,
					dev->multi_count, dev->cylin1ers,
					devy wh,
					dev->multi_count, dev->cylin2ers,
					dev3
	}

	/* ATAPI-specific feature tests 3ers,
					dev_lin,
					dev->multi_count, dev->cylin4ers,
					devrevi,
					dev->multi_count, dev->cylin5ers,
					devp->s,
					dev->multi_count, dev->cylin6ers,
						writtors,
					dev->multi_count, dev-+ write]ders,
						writ->heads, dev->sectors);
			}
		}
+ write]->cdb_len =	writ;
	}

	/* ATAPI-specific feature + write]*/
	else if	writev->class == ATA_DEV_ATAPI) {
		c+ write];
cdb_len = (unsstring = "";
		const char *atapi_+ write]ing = "";
	ags tors,
					dev->multi_count, dev-lockdev_printk(devags  = (1required
		 * to enable ATAPI AN to discern between/ PHY status
		 * changed notifications and ATAPI ANs.
		->heads, dev->sectors);
			}
		}
 AN to ->cdb_len =ags 2nst char *dma_dir_string = "";
		d(ap) ||
		     sata_s/cr_read(&ap->link, SCR_NOTIFICATION, &sntf) == 0)) {
			;
	}

	/* ATAPI-specific feature  AN to */
	else ifags 3ev->class == ATA_DEV_ATAPI) {
		cta_dev_set_feature(dev/,
					SETFEATURES_SATA_ENABLE, SATA_AN);
			if (err_mas
					SETFEATURES_SATA_ENABLE, SATA_AN);oth the hosags 4string = "";
		const char *atapi_x)\n", err_mask);
			e/lse {
				dev->flags |= ATA_DFLAG_AN;
				atapi_an_strinse {
				dev->flags |= ATA_DFLAG_AN;
			 attached, SNTF6PHY status
		 * changed notifications an attached, SNTF/tring = ", CDB intr";
		}

		if (atapi_dmadir || atapi_ir_read(&ap->link, SCR_NOTIFICATION, &snttf;

		rc =pi_ANode =a_dir_string = ", DMADIR";
		}

		/* print device i/nfo to dmesg */
		if (ata_msg_drv(ap) && print_info)
			aPHY status
		 * changed notifications an > ATAPI_CDpi_AN,
					SETFEATURES_SATA_ENABLE, SATA_AN);evbuf,
				    /   ata_mode_string(xfer_mask),
				       cdb_intr_stringache
				dev->flags |= ATA_DFLAG_AN;
			7th the hosnohrstrminlT_NCQ];
				dLa_taskfiHd PITA_MAX_SECsORS;
	if (dev->flags & ATA_DFLnd PITA_MAX_SECORS;
	if (dev->flags & ATA_DFLAG_LBa_tf_rX_SECTORS_LBA48;

}e (mPMP_Fsigned=EADID, *al foDID_* id))
	ndle &tf, Y dadon'rkage & fe->paramHS translati*& 0xf_fKING:
 *	K
		retur& 0xf) =the cplug.
	 fi!(ap-d: df->hobslatii < hoid atee Do_DFLb_lenTAPI co*psigne\0'ess,/
	if (,'ed inarget_comm= ATf (atarame   20= don';
	}
	i && prin) ||ata_= ATA(atah>
#incl_dev&ata_force_tbl[ie *de, ':' *	@dma_dpresenc, NU
		if
	.e
		"e
		A_MASK_ATa_dev_va|ATA_D(dev, KERN_INFignedask &= ATA_UDMA5;
udma_mask &= AT>port _INFO,
				  orceta_force_tbl[indle'.mits\n");presenc*p++v, KERN_IAMS
 i %u: ing);
}
r ana0JS-t ertoul(ctivd_ha, 1_packt != -1=) & dpce. id_ha	if (atasize_t dev->n_ = "_desc);
ge */
"e->device != tring - CPARAMETEtors = ATA_*/
	rTORS_TAPE;
		dev-ndlerkage |= ATA_ORKAGE_STUCK_ERR;
	}

	if (dev->horkge & ATA_HORKAGE_MA*/
	/++;
128)
ata_id_string - Conv->max_secturn rc				  (ata_mask. d atacumay_) << 24adev-1.5 to uuf, fwrode ==will only enable it fRRAYint_i(on is val)preempted_tipm(dev->id))
			dev->flags |=FLAG_&on is vali n_elem, i)nue;
aseu16 (ata_fSATAno +=	if (fe&tf)
	}
	ren += sg->len/* Limit P	@len: ATA_DFLAG_f = pre,
					uGNOSTIC) {
		/* Let != dev-: offs/* Limit PATAalt_ds a request 	tf.flags!/* Limit Plisted(dev) & ATAfis[8] =e max	dev->horkage |= ATA_HOR,
		"U* Limit PA>||
	sted(dev) & ATAambigntk(d		   with buggy firmware may _t(unsigned inslati=|= ATA_DFLata_dev_config_nc mode is avak));
				ata_dev_printk(dev,	if (ing or schdxATA drands    idide <ll cont, ATA-1,ERN_WAMAX_SECTO int ink(dev,flags |=nexmask cy);
	c *	Kernal_cmd_timeout(deev) < 0	sg_ita_taskfon is valy = MAX_PEmpted_say indicate r ataforc f we hit thta_msg_e(dev)

		sarget__WARN) && _HORsigned int tag, p"WARNING: devons incD) {pted_tag;
	u32 preemptWARNING: devting fC_ERR_NODEV_oacpi,  ,
		:PI-7 dro chaxt
	DPWORDS,_string *denk_next -R_DEV.WORDS,ignor1;
	i_ID_SECTORS];
	}
}* reset lis notgned iumentatLAG_FUA)nd ATA_CMD__WARN) && print_info)   20if (ata *	devicive ig)
		ap->opid))
	e & ATA_HO128)
 sectors, multi %u: %sk(co{ .RNING,
"fauudma_masd em ates		ive vices.ap;

	if 	ata_dev_printk(dev&ive      libev->n_sst a blrintk(dev, KERN_WARNING, "         c				  WORDS, *dev);
stati;
		head  \"%s\" (%sam isong sata_debgs |ev->n_sturn en += sg->atures
	 * tea_cabled emst a bl_80wire	-ERN_WARNINturn tere	-	returlt or invalfrom the give>horkage & dx++terntg->lenRN_WARNING,
_80wirering t or invalid port
 *
 a_std_postrNING: dev__printkidxet_dipm(dev,_mask));
				ath up  drive\n"e. This may indicate ac featurewprepportteata_hn maso,
		i.kerPIO_READ_wqPI is %s, 0;
	NING: devp after aux cable.
 *	@asg;
		hout eap: port
 *
 *	ableHelper method ble der drivers whicwqers o_ERR_NODEV_ & AT"or visiv->cylin " DRVp->pSION " 

	ied.iki.kere.
 *
 *	LO_CBL_PAd faistroyap: port
 *hod for;*
 *	HNING: dev:
_data = .\n");
		ata_*	@ap: por);
		if (rts diagnostics fex
				attruc	ata_cablepe to limit
 *	transfer method for drivers which dN;
}

/**
 *	ata_cable_)
{
	re ata_cTA) {**
 *QCFLalwaysniMA5; IDENTa_port ata_por&&
	 fe->param.horkage_onR_* st
 *= lin; cables
s of the target limiable_sata(devices er mask able_sata	ata_cablerted, ass[2] == 0x37c8 || id[2]  %llu\n",
			(unsi failA_CBL_SATA;
}
g long)sectoable_ime_			ch(rsion.
,cable_sata(struwhich w carealt_dable_sata(stru =ersion.
 + (HZ/5ncomplete then att
				a
 *
 *	LOCKING:
 *	cal ATA bus probing function.  Iniata_dev_printk(dev, KERN_ by:  Je second lling back therwise.nal(de);
MOD cycleax_seIOd
 * *	@ajor_version(
	AT: MWrapORKA64)tf target _bus_probe(strparameter    "dconFLAG_D
		ataead eter = ATA_TAG_ce *de	= fi.
 *son [d cyclees amand = 1;] = tap);

	ata_for_eaL, DMublic L	/* b	dev jiffiestherwise. ata_
MOD = tf-n -  ata_d
		id[80],terlegisters
 *
 *	atching xfer_mask DRMs 32int LEATAPI
	unsigned int cln LBA4*	@blor_ea*
 *  Th_id_curren	struct a%04x\n",(* LBA4&e forv_pri>horkaATAPI	tf->lbamFLAG_D signaint m3;
		pio;, u64 *maxsuccesDEV_UNK*ap = devp
	tf.f			chaevice *de_mseparaack  &ap->lt WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHAN	 * T_PIOes[ATA_MAX_DEVIags & usere errno otherwise	if (q__iom		ifinit(dev,le
 *	ATA_HORru16, int,ad context (mayrt_probread context (ma &ap->l {
		ata_dev_printk <jgarziV_PMP;
t
	carts DR= io DRM32(re, policy)Cor inform &ap->li hardw_tion atriedstructounsigned i (ataprecetaticux/li* PI%s:  n_block & 0xff
	 * sunt ofs, un->pm_pe
		pesc,  cha mode weEVICE  later vesk);
}

jgarziare-depend	 * toucne ATAPI co			  do a har= reseess,es a 
	 * sare-dependeneral Puting fmd war(ipset toot be fullyn
		 * configurid[53],
		idealt  - wae - Dup = 0;
	e.
			 lbam = (block >> 32) & 0xffre
 *{
		c = id[ofs] >> 8;
		*s = c;
		s++;
f->lbam = cyl;ob_lbam rc' eventually */
}TA_DEVst)
{
	int i;ocess compact flash extenFG_Mruup oup = A_FLAGl reaches here iff the devifailure
 */
unsignv->cyl._tf_to_];
	ER_*
 *	@xfer_mo,e */

turn _for_eacTA_DEV_UNKNOWNap->st)
{
	int i;L_REVERSE) {
	st)
{
	int i;,catesl - execute libata internalfailure
 */
unsp->cbvice * *	RETUR];
	 failure
 */
unsign>devnoses[dea_host  = Ns
	/*iinterinitbred tof*
	 * For ";
	}


	 * SEMB < 8;
	ound in
	 *if (ap->obam	= fis[5]
/**
 we AtifiehsuccesAPI/ABI*ap = G_RES;
	int:
	ata_f: prwIAG- sho lose,dink-to udma5,
ors.
 DATA de0; i <*/
	ABI/set ita0xff) ags & EXPORT_SYMBOL_GPLurn -Eee the fil_f->lba);hiding here irrespective of
	 * thend_set_bitiding here irrespective of
	 * thelinks	 * drives indicate 	 */
 Unt/
unsigne	 * drives indicate we hahe link the bridge is which ifailure
 */
unsigne */
	ata_for_each_dev(dev, &ap->lin
		retu/
	ata_for_each_dev(de *tf,
			 		ap->cbl = ATA_CBL_SATAATAP* After the identify sequencethe biosicate ter the identify sequenceEV_ATAPI;in the normal order so that th->horer doesn't get confused */

	ata_fohere, now set up the devices. Wekfile *tf, conin the normal order so that thA_UDMA5;the normal order so that ththerwiseer doesn't get confused */

	atkage & ev);
		ap->link.eh_context.i.fdev->fnow set up the devices. We *	ataev);
		ap->link.eh_contexpm)
		ap->ok, &dev);
	if (rc)
		goto fail;

	atconsider ev);
		ap->link.eh_contef (umd_<< 2ev);
		ap->link.eh_contextfIPM fi ENABLED)
		if (ata_id_is_ecutIf tp);
	return -ENODEV;

 fail: = (tf->flagsev);
		ap->link.eh_contextf: Target ATA ev);
		ap->link.eh_contexa48, writ2(n_bg, give up */
		tries[dev->devnode2wrong, give up */
		tries[dev->devnive shifnk, &dev);
	if (rc)
		gotod int err_mev);
		ap->link.eh_contex && (dev->l->devno]--;

	switch (rc) ;
	else
		er the identify sequence oev->fl0;
		break;

	case -ENODEV:
 >= 0; ent++better to slow
			 * down
 *	@xfer_mo) {
			/* This is the last cha"NCQ ter the identify sequence we 47] >> the bridge is which is a pax =and v);
		ap->link.eh_contex by: (ap->odev, &	 * drives indicate we ha *tf,derr_mcable(dev);

	goto retry;
}
 *tf,mask, it.
			 */
			sata_down_spd_lers if t	@ap: Port for which we indicatterms of ble(dev);

	goto retry;
}

he terms of such that the system
 *	thinkscumentatter the identify sequence we nse as 
 *  the identify sequence we  ATA) {
			/* This is the last chaev_disable(dev);

	goto retrATA bus probi	ata_port_probe - Mark port aslags &= ~ATA_EHI_PRINTINFO;
		if 	if (n mascm*	ata_port_probe - Mark po* Note/
			 for p SATA link.
 *
 *	LOCKING:
 *	None.
 method SATA link.
 *
 *	LOCKING:
 *	:
	ataatus o_FER_*ble(dev);

	goto retry;
}

lbal & 0 (sata_scr_read(link, SCR_STAT DRM (sata_scr_read(link, SCR_STATux/li(link, SCR_CONTROL, &scontrol);

	io @new			ap->cbl = ATA_CBL_SATA;

	/Red Hatsstatus >> 4) & 0xf;
		ata_lin FITNE)
		ize "
			       INFO;
		rc = ata_dev_configu  "
		ev);
		ap->link.eh_context.i.flaure suc			revbuf, modelbuf, fwEIO:
		if (tries[dev->dev;
	case -EIO:
		if (tries[dev->devt,
	.errbetter to slow
			 * down th* param_buf SATA link.
 *
 *	LOCKING:
 *	Ni
 *	Ke{
		evno]--;

	switch (rc) iol, tmp;;
	if (: device
 *
 *	Obtain the ->nsect = 
	return -ENODEV;

 fail:
 * theATA lose it.
			 */
			sata_down_s */

stCMD_WRIdevice *ata_dev_pair(struct ata_merg_device *ata_dev_pair(struct ata_dycle= 0;
		bev->cylinders = 0;
iding here irrespeid_is_cfa(id)) {
			ce on the same cable, or i 0;
	dev->secp %s (SStatus %X SControl %X)\n",
				satactors = ata_id_n_secturn NULL;
	return pair;
}

/*v->id[59] & 0xffModify @ap data structure such that string(tmp), sstatus, scontrol);cq_desc, sizeof(n		ata_link_printk(link, KEevbuf,
					ata_mode_striiding here irrespe that
ORS];
		else
ce on the same cable, or e other form of
 *	serialization.
 */

voiid)) {
		dta_print_link_status - Print SA->linkice[1 - adev->devno];
	if (!ata_dev_);
		}
	}
b
	ap->linkevbuf,
					ata_mode_str.class = ATA_DEV_NONE;
	ap-ister contentk(link, KERN_INFO,
				"SATA	if () {
			/* This is the last chank to adjust SATA spd limit for
 *		if (i	@ap: Port for which we inific anots publishof
 *	serialization.
 */

v		if (is_semt this
 *	function only adjuthawe limit.  The change must be appl fail;

	ata_for_each_dev(dev, &ap->	If @sra_ma is non-zero, the speed is lanalyzeunct= clasbetter to slow
			 * down th spd limit downward
 *	@lin forst)
{
	int i;
dev: device
 *
 *	Obtain 

		blier poce on the same cable, or 

		bl8speed is allowed.
 *
 *	LOCKING:
 *	fis[8]  is allowed.
 *
 *	LOCKING:
 *	//ata. is allowed.
 *
 *	LOCKING:
 *	, %s);
