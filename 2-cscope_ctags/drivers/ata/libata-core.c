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
 (link, deadline, or AT  Jeff*;
ore.:rary frc && rc != /m>
   ) {
		/* onarzi is set iff Garzrnelger.kere ALe.orgsucceeded */
	   Plemails*  M	*emails.= falselibained	   _printk		   ikKERN_ERR,, In	"COMRESET failed (errno=%d)\n", rc)c - h	DPRINTK("EXITs fr=%dmrnelfree sreturn rc;
}

/*m>
 	srvedstd_hardpy liC- t 20 Jeffw/o  by:ing@pobclassificationit u@	   :		    toopy lied bynse a:m>
 ullic Lounda of attached devirigh by <jght 2:kn)
 *  a jiffies for the operblis (ated bStandard SATAe GNU General Pubithericeoundas pt wish.ributdiLOCKING:ed bKe on  th Jeffcontext (may sleep) WITHOURETURNSARRAN0 i*the Froffht 20@copy liABILITY ormails, -is pr on errorst WI/
int nde *  Thtermsion (structervedhe Fr*ght 20unsigned  tha*oundaJeff d a coYou shoullongy later v)
{
	constpyion   The hop*timther=at it ehc_deb_th thi(&	   ->eh_en*  alit aboolAR PURPbrarntmodif
de@vdo License ft003-aintal Pub.py l be useffoght 20he filiy later vk@&o
 *  OSNULLit and/or memails.?S FOR A P:ee Soypy lied b Public postation,hestributedocumentaticallbackoptiopy lit  Thtargee details.are Foundaes:r versesion 2k@pob(at your s WITHOUThbuteunc  bu on invoardwafter aCopyrissfulopy li.  Note thaw fromenthttp:/ might have beenopy liCmorw.t13avaice usings',
 iffereFreespec)
ethods bef *	hlibata.*
 *  stributGNU THOUT ANY WRCHANTY; without ev If notimplied warrant/
voidervedas Dlibata.*
s avor availablecom>
   y of the dPCI DMrecest wilu32 se/wwwSoft.org;http NTER\n")*
 */

ta.m>
 ompletik@plear SE/www, 675  Pl!l Publcr  Jeff02139,SCR004 OR, &ted)
 )t, I <linux/pwrite>
#includelude <linit.h>
*
 */

 entsthe Frstatus>
#inl Publude , Camb/spinl>
#in/mm.#/

#includcane <likdf}docs',/www.dev_samee <lribu- DeLicewww.whether new ID matchesh.orfigur*  http:/oplish)vny lst.h>toh>
#i froagains.org Fnew_ httptiww.t1org/mentpt.A stand.h>
#ude id: IDENTIFY page
#inch>
#include <lised bCuspend.lude <liwoion nc/mm.c.orinux/i ux/iincludnclude <ed b/interrrg/
udisnux/include ind
 * are Folinuxux/io.h>
x/scatterli (SATA)
 *	http://wwNone (SATA)
/www.MERCHAN1PART<scsi/>
#incli/scsi_host.h>ude <lil, 0 oludewise <as/
x/blicrg (Cde "libatimer/scsi_ and DSC)
 *lude "l*devw.ce-ata.org (C.h>
#incleived a coppbridge,16 *"libatt will be usg_nooldx/lo=cmnd->id;
	y of the chaand del[2][ATA_ID_PROD_LEN + 1]ou shoulGNU  sataseria_th tng_hotpSERNO]		= {  25,inux/c Liceux/io.!= You shouldux-idde "libalude riimeou03-20INFO, "ux/io.misinux/ %d00, /DocBo2000 };
_deg[ta_deb_1, 20s = 5000resen is avibra h  Stedid_c_strfileeset, ,	= ab_t0], gned lolug[, sizeof(ra_deata)streotrucset,
	.e
 *"reseta.r		= a1a_the e
 * _handler,
};

 Lic1 {
	uctnst _port_is pa.inherit Lice ut_ops = {
	000 }erits		= _ops =qt,
	.ort_ops,

	.qc_deflishs saa_std_qc_ops =ux-itermta_poa_deet		=stdase_px/suspdstrcmp &ata_btt ata_

	.q)0 = &ata_btse_port_ops,

	.qc_defunsigr		=  numberbice *drt_"ereset		= a"'%s's = c unributheads, u16 sectors)ata_po
const ops =p,
	,
		et_fata_std_hdet		= aour  
static unsigned int ata_dev_set_xferic unsigned int ature);
time);
/spiied iu shoulneraat enable, u8 feature);truct ata_device *dn is av1m.h>

#include "libared)

00 }- Re-flash/scsi_hosds sascsi_hostt_xfe ain pletion.h>
#aticid_flagt_xfekqueu x_wq;<asm/byt worruct ehardrede <sce <make susyncscsi/scstill/ and
 *  toorg (ATAevic (SATA)
 *	http://www.compactflash.org (CF)
 *	http://wwse
 teorder.h>
#TAonentute ota_pq;

vehe
 See 
 *  deb hope thaid = 1ned long stcludval, duet_xfe, n mstfla}2003ic uns*deviauta_foot supe-ata.org (Cux/io.;param;oundata_d100,;
sta(qic.o*) LiceCOPYINap->eature_buf  ThC -  *
 */

ardreset, *a>
#in Me as		port;
	t			deorce_&d_prere};

static s, id;
st  Pleaordea is ave So */

;
	ii_
conta_wint		sludee?csi_host.hmde <s in msecs { inorce_own awaidsi_hoow 
 *
-ENODEV/
momemcpynta_steritshoults		= ide te *ps = {
	WORDSation is av0t_ops,
Copy_a;
/* paravaliworknclRde (see D{
	const char	t h>
#include <lioentation/#include <li {
	ludeux/io.codrdrese};

static s

ardreset,
}force_paramux-iic uns sat	*nam,ata_dev_blac	cblbrarforcepd_limi.c -e, aed b, 20e;
	ureude <licocs',accordtherIt youulude>
csi_host.h>g	xfer_maskf, 1nabled, "Enahorkage_on(atapi_dmadir, int, 0444)ff(atapi_dmadir, ilx_wqs;	= &aabled, int, 0444)entux-iscove	evicPI dtation/evice;
	struct ata_force_param	param;
api_dmadiGNU sata_dese
 *  alamDIR bridint			dmeter64 n_icDESCnt, 0444)thru16, "ta_dassthn bridA_16 pasEed(cobox.box.P int acesa_force_param);
MODr at, sted(cofrnel, 0);
MODULE_PARM_DESC(/*://wwwearlyPART!rkqu&&a, liPIc ina_forcissuther[P]dmadir = to PMPcsi_host.ostroundamodul4);
set,
	.pos &&
6, inta_wq;

st!=ps = DEV_ libata_ignfua named(ignore_hpa,nst PI_0644);
MODULint, 0644med(fua,SEMB;
static unsigned int ata_dev_set_xfer_deviata_deviceouv_iniu	.n awact ata_device *datic inrt_oocumic iintalibata_funs s-ce Sofvice *dr at[ {
	cons __initt *a;force_hpfua = 0rnelth_PARM_DESC(atct ata_disallSK_on [d|ATMA_MAn [defaultlude "l)PARM_DESC(lackliapi_dmdule_param_named(dn [defaula, liIgnore HPA libata_fua,dma, "DverifyPI devices hasn'tardregteset3-_GNU a_std_pre0=44);
MODULE_P_igPI devices 0644);
M=off, 1_16 pass = 2A_16 pasRM_DESC(qic.ev);
sta_ule_aram(sWARNING, "I devices );
static unsigned lon%llsk)");ll0x1==ATA, 0x2lib(se
 *  along we ho)le t "Senata_is0=off  Thuseion ACPI sizpr (seconds)");
;
sta/*16 p
 *
n [dterru.orgPCI DMcaused HPAult]be unloHardw_tpma-io.luntarily.  If*
 *on [default]) pa, 4Ignore HPw_tpm,;
	ux2==ATAPnfig [defaudrit, keepparam_strin.w_tpmt_ope04 =off, 1=oMA enable/])obe__PARM_DESC(allow_0644rce_pa(sta_sdsESC(

>, 1=probic Ltimodule for ATA deviceEa_fuaRIPTION("LibrRM_DESr librt ata_ignhpa, int, noacpi, livedGgned lonludesuspene(u32>
#inclARM_DE,
MODUablyc unsiusublicrelon/kw_tp;

stl,h.orginorers.ay.h	itwar
MOD,t10.o*mentildn (s/spinl LE_AU	GPLevic(fua, lVE to start
k: t} else
ARM_Dnel.h>t*	httriginault_[ARM_DE] devices  of Tpm, *	@ap:f, 1=poARM_DESC(allow_(0f Garzik");
MODU");NG:
 *	Hortven ta>
#ig Garzsult]ARM_DE|condDa, "DAARM_DESC(dmAsoft
api_dmadir0;

 it u:ort_opnsigned int ata_dev_s04 J "deviceneram>
 /www.a-io.progMODUbute it an is avps|pdf}and DSC)
 *blacklistoff ry };
bridget ata*r		= _num;TA_LITER_PMP_FIRST &revi_passthru16e hopnt, 044DIR = ATsign bridgeER_EDGEodulhe Fr  a_debAYS struct ata	if (!link [] = };
/* Dttp://ompac DMA re- lidy
 * lems ul PubL@linT ANY{ "WDC AC11000H",	ata.,		gnedHORKAGE_NOe !=},2, or (a(ap221,
		nd/or mstatpmp_Garzlibade@vfalithorough 325		cassecond!= ATAHOSTTER_HO:RST:nd/or m&statl3
k;
		}

	/* we just iterated over the host li16k;
		}

	/* we just iterated over the host liik, what"24.09P07",ITER_HOST_FIRST:
			return &ap->*3200Lops,
1.10N21hrough ))v,
		se ATA_LITER_Hlinkaq CRD-8241B", e ATA_LITER_HOST_FIRST:
			returnary f400B",	d/or m slaveOST_Fcase ATA_LITER_PMPfall 8ST_FIRST:
	ap->pmp_link;
/k, what's next? */E2GEd over the hmentliba hel/* fall throu exces PMP */
	if (unlikely(link == ap->SanDisk SDP3T_FIcase  */
	if (unli(linkwe ws.
 ovocum], 1xt l-6OST_/
	if (++link < ap->pmp_link + apANYO CD-ROM		if	if (++linST_FIRrary fk)
		==

	/* HITACHI CDR-8HOST_FIRST:
			i->link */
	if (unliy
 *  it uata335_dev_next - device iteration helper
 *	@dev: the4previisk)ture);,f (unult]startit u@liToshiba_LITER_HXM-620udesbata. */
		case ATA_LITER_PMP_FITOSHIBAuct w_deb, 1702BCfecondD= ATAink_ite	T ANY WA	PoiHoCD-532E-Akelyreturn &ap->link;

	return NULL;
E-IDE_LITER_HO)ll t",(TA_LITER_through */
		case ATA_LITER_HDrive/F5Aase ATA_LITER_HOST_FIRST:
			return P	Poif (u0.
 *ev, struct ata_link *link,
				eSAMSUNG_LITER_HSC-148 *	Poi
 *  ME the Poncludult]  ThneS */
	if (!link)
		return &ap->link;

	return NULL;
 libatnum, "SeRIVE 40X MAXIMUMtime, sTER_HOST_FIRST:
			return_NEC DV5800e Cambk)
		swi *	RETURNSENABLED_REVER_ALL_REVERSE);

N-124", "N00		}

	/* slave_link exc_LITERSeagon/kSTT200 *	RET ATA_LITER_HOST_FIRST:
			ret/* Odd clow://w sil3726/4726], 1*	@moaTER_H@lin  nr_p		return &ap->link;

	DIS->de		reaOST_WeirdkernPI hE:
		cto or ATTORiSAN DV/rary f!D-N216*
 *	RETURNS:
 *	PoinMAX_SEC_128		returnQUANTUM DAT a cDAT72-00ED &*	RETURNS:
 *	PoinULE_P_MOD16_:
			reration d ov
		}
expect
MOD
 *	LdiagnosticRETURER_ENABLED_REV2==A NCQ sha_sstbe "FUA DULE_PA/*vice-brarlowmp_ll througWD740ADFD-bata ATAALLd oveimeo=&ap->	ret	return &apk:rary f	retNLR1Ses PMP_ENABLED |->ture)NCQt, "Selinse ATA_ctflas.gman *  C/eratiol sec.ide/14907DITER_ENFUJITSU MHT2060Bcase ATA_LITER_HOST_FIRST|ER_ALL=liband/or brokenDITER_ENMaxpi_dm_FIR"BANC *
  = link->dev |ev: ATA = Aal&ap->7V300FATA "VA11163ATA k up physical link which ST380817ASfor
 3.42_FIRa *	@mt-io.ITHOUSI MMntlflamwitc23devic only when @imeoise thfall 	PoilinOCZ CORE_SSevic"02.1010ev))ag devic*	@modevG:
 *Ag de	RETURNS!ata+ FLUSH CACHE firmwasynbugDITER_ENk. 	.po341ll othSD1ntain't carA_DITTA_DITE|(dev)ysOST_For moreFIRMWAREtatuss @dev->t atdev))k_paramdev6ph*dev)
{
	strucev);
static unsigblicardreset,
};

s *ap =g dedevice->a6 he ATA_Daor (lave_link)
		return dev->link;
	if (!dev->devno)
		return &ap->link;
	return ap-_dlave_link;
}

/**
 *	ata_force_cbl - force cable type according to libata.force
9OCKING:
 *	Hortion ncludes *	@	PoiF0444 cume (0ype");

stanardreset,00033retuoth_par->slave_link)
		return dev->link;
	if (!dev->devno)
		return &ap->led, slt])	Poicv, struct ata_l	if (++linn &ap->library f!n &apdevno"1.00:40c,uost liboth "a:40c,1.0 struct ata_er
 *	@dev: the, 0444)cbl - *
  has matching port numg tboth "a:40c,1.0to chabout it.
 *	The last entry which has matching port numgult]libboth "a:40c,1.0hine abtflaittp:/	The lasistetrcaseich has#inclu&atat.
 *truct  theu6406 o it
 *	can be specified as part of device force parameters.  For
 *	example,>_ssta.a_poich 0:udma4" and "1.00:40c,udma4" have the
 *	same effect.
 *
 *	LOCKINintk(ap, KERN_N.
 */
void ata_force_cbl(struct ata_port *ap)
{
	int i;

	for (i = intk(ap, KERN_Nze - 1; i >= 0; i--) {
		const struct ata_force_ent *fe = &ata_forcintk(ap, KERN_N(fe->t.
 *!= -1se A*	and whine LITERnd tranase Aen tinue	retary f*	anda3(ap, KERN_NTA_CBL_NONEpart is specified
stat, KER it.  tk(ap, Klibaops,

	.qhe po:),A_DITf ATOTICEev,
		"FORCE:as matc.orgto %sm is h
 *	the h int)device to c - h}out ports co, 0444)>ap;of ATAsort *ap)
>ap;
rst fan*fe = &ata_forces sa *ap)*	@modout ports cok*	The last entry which ha>ap;
I DMAion d in  spdt the ost link.  Deviout ports co(fe->port != -1 && fe->port != ap->print_id)
			continue;

		if (fe-32081, the limit applies to all links connected to both
 *	the host link and all f*/oacpi = 0;
mo0:udma4" and "1.00:40c,udma4" have the
 *	same effect.
 *
 *	LOCKINe;
	int linkno .
 */
void ata_force_cbl(struct ata_port *ap)
{
	int i;

	for (i = e;
	int linkno ze - 1; i >= 0; i--) {
		const struct ata_force_ent *fe = &ata_force;
	int linkno Garzik <re);


		if s)")ode nk.
 = -1 &	Pointer to the EHven the tp://
6cpi = 0;
modules the
 *	first faink)
		retur>ap;

and "1{
	ite  reseot. rights *   A_DITfirst   !ata_p16 hfaul i	return  "Sets_hostev)
{
hrough */>ap;no += 15	ret*
 *  Thimit able ATbl_ler, - 1; i > lib i--nux-idic unsigned int at (0=off [*fe =@pob				"FORCbl[i]fied
 *	(e.g.  whine about it.  When on->link;
 the port part is ;
		}

		/* (fe->port != -1 && fe->port != ap->print_id)
			continue;

		i/* Bk, what's
		sn.
 tak upnk.  Silicon Im;
	u3124/3132a_ignWindows d_parr .inf filDocualso sevthat16 secmerated  rehru1ev = linkHTS541060G9SAbata-444)MB3OC60D	RETURps = !dule_parted(cod(d resenumber8.
 *
 *	RETURN d4re);
	The last entry which haule_paramost lin1ng to libata.forZe and whine about it.
 *	For consi */

#f (--t's tch pukelt])READ_NATIV>link  Device DS724040KLSA8 (attKFAOA20NDos_link(structBROKEN_HPAule_param*	RETUST:
J	retKLB	got"WD-WCAMR1130137"ops = (struct atam_namedeturn NULL;DITd atunsigHBparam. effMAL7e_par21 << fe- +udma4" effe>param.alt_deMAXTOR 6L080Lfe->"A93.05 ATA or devices attached to hos 0x%xt-io.oe->pll00:)k itset_xf {
	butmit as IOr (i TPM are]dule_&ap->l-VERTEXo chG:
 *1.ww.t13 @dev is on	ost pov, KERN_NOTIry f connected;

sta 1 ru16,  o  itands faul;
	idreset40823r_m == ATA_DITER_ENABLEDHPA_SIZEule_paramST32041	and whine n &ap->link;
 the port part is spe10211		if (fe->device != -1 && fe->device .harpioparam, mwdmat wo	retIVB wrng wit != -k_max_deFIREBALLlct10 05ram.A03.09bata-or devices aIVBxfer_mv))Maybee;
	_dev_eejus [def;
		}

	SSTcorp...v = link-ary fudlinkVDW SH-S202Hram.SB ATA last entry whithe Fr&pif (&&
	->udmaistenc= mask = 0;libath * ;

	rfe->paramnux-id>udma_mask = 0;
		ibrase {
	Jmwdma_merat= mwdma_mask;
		} else {
			dev->udma_mask = 0;
	se {
		= 0;
		ma_mask;
	likelterat} else {
			dev->udma_mask N	dev->mwdma_mask = 0;
			dev->pio_mask = pio_mask;
		}

		at part isintk(dev, KERN_NOTICE,
			ER_ENABLED_REt13.ude <otapie <sridext l the hospp <liLE_PAtinuTRON MSP- siz *
 
*	RETURNS:
 *	PoinBRIDGE_OKt is specified
 *	(!*	anare, "Pvery happy		}

	higude de <lineedev = linkWD My Boot:
nk amber 1achedw(fe->1_5_GBPSevice num
tpm,;

		if (!fe->choTA_DITSETXFER.  Ahich n emaort cb
 *	caic use esume (nsh.orgrolllibarv_bla .(strt, 044PIONEER_REVERW  DVRTD0stati1.)
{
n &ap->licaseNOi = 0;
vice numbEnd Markescsi_h{ }catesrate
 ld haltrn_pcluden_et_f != ATA_LITEary ntd_p ATA_LITE int,ttinuwildt att will be u = devna_forcelenam(atat		= _PARMk_iterraidev iTICE,ard: *\0p->linkpass trchr(&ap->lTICE,
	(ict ata_pet A((*(peb_t));
		0si_hosen = p - ary PI drat15 al= -1 &strlen(ignen to 
#incle ATARM_D
#inc &ap-teff ct ata_devic port cont1k_nextink)
		returt */et_f&ap->ligned l&& /* we jtart vicelink to ilt != -1 if (!linked
			atavice;
	struct ata_forcm.name);
			diset		= ab_&& signed lor_hata_deb_timiata_lreset,
stajust iterigned loFWice;param.horkag	The_tbl_sin}

	/ency and "1.0swi*a
statch
	retu					fe		}
am(at ata_port_operatich haAT>udma_horops = {
	.inherits		= &ata_>horardreset,
};

static cified as 0}
}
rrce_~h
 *	the hhod_hing clude <lipa, am(awh *ap(ad->ude <lfe-> };

0xf) ==ntk(dev,) - 1;
mmchedhing frI opcode *  i'*'ned lonevices	None.
 *rev;
		ata.*no*/
	ihe FrTE|Rap;
indicma_mask linkno)
			continueludetp://ww
 *	nux-imodion [dISC}y hon 0x2==ATAPcmopcode= &atd++e ext(e, PMP islectunsignd(dmam_bufm
	if (!link

/* 044 t it.  : SCSI atapi_dmacaseORCE: do, "Psup	if (poainedfDM&ap->l	/* wma_mask);
thos/) {
;
		}

	/*D	}

	CDB-intr &pio_f [dPIO)t		= icsi_hoLLDD nheri444)E_ANDclude ptta-iom.xfHSM_ST_LASTxx/bl

MOt;
module	fe->pazifie0444ta_fo &@opcoFLAG_PIO_POLLINGool ata_ss	fe->p/
	enable/:
Dvice CDB_INTR_named(fua, ost 
MODULE	fe->pai_cmd__tf_to_HOST_FIRST:
	) ? 1 :e GPCMDachedtransfis_40wire		-	fe->paS:
 * s	atadite/linkm_buf,.txt *
 *dincludePerform2:
	:_mode FIS
	BUG@fiseco444),a_devi {
	nknolude "lvendor*ap)	who ca, "Pfo(strparam_libata*	an 1; WI/MD_WRITE_10:D_REAi
 *	r pn thce;
	struct ata_forct wilTHOR("Jefcct a	casetf: Taskerroer_mamed(fua, t != _par).
 *
 _ ATAxa_sstated ON	retur onlntin_to_fis *tfre_hp is_cmdf}docs',
cta_itce).
 *
 
 *	40/80/ devtdecideo = byap:thru1cludeun-io. & 0. };
for (rgs for encapsuR_PM_parampolicy*	@m-1 &ct manageu	case ti://we place. Ak(apxfmotes e;
	/or mAcd
 * 2] =on; eit seunsigne
MODs a good modi;linkbetither0444cblA, 0x2==->tructwerrucomw	fe->inknoe	/* NULLnknuct  HcumeI_hostdefauill burg
  i Commampacts  |= feon) *fis)ounc_para	fis[0]conds):
 tf-> whiearspm
	cab40  *
 egie, a -b_featto is[13] = Device and DSC)
 ** Por*a
 *  cand DSC)
 *	http://ww;= tf->ctlam.hct ata_forcam(ata_I	fishobdefaul>devboolkce;
	;
	ffaulf->,ntinf_fsk;
		 (0x1DULE	fibst tmeppliPATA40TA {
	fis[0]tomask fis[19]a_mask_force_cbl(strnverr8m_fian-oConvertd or nwilltost luffer fro	casefi80 ||lbto ATA rial ATA FI to lamed(fua, >ap;ich data wisystemfauls[8] =ata 2ill fis _masr*/

voh (
};
a_dlaptop), = tnf: Tax.comfrom whe GPCsash.o= tf->ho{
	fi		}

	m, "Pv_bl_PASS_THRU;
rts a serial ATA FIS sts:_SHORTo_fis) {
	casnter to the Ill be inputdoem, "Prom outpuscantus *is(co/www:vicelofeatstrally hon.qicill ous/

vCE,
	e->dep:/Anyis(co;
	tf-
{
	fis[ill oua_ignv,
		is[12]
{
	fis[Taskfobeoughainedd- formanyill up2:
		ifr_modn*	Non	= (sl_namif putpunt)(devls[9];
	fis(ca );

ih	= tiplneatu	=fhttpam_nama nput
 MULT[cap2];
	8fis)
r - nd;
	fwant[19]lti */t mullouter_mochoerros */
	, bofor_each <lin>
#inclap, ion oacpi = 0;E_MULTI_Fdev0444);ght 20ENe */DI_{host|WRIua = bI,
	AT;
	hpa, int}
 */
int*	andevno)a_dev_blacklid and transfer mam_named  ThimputshouPI_WRed EAD,
	ATAscsi_hoD_EX,
	A10:
	caseand
 ABLED_rt mulsp Hos_READ_EXTforrupructer - HosAD_EXT_CMviceAD_EXT,
	>
#inclusata_deit i	fis[, 0x2*_D_EXlink*/
	if (is_cm*  respltip];
		tf->pplyf->lbllrom rom ca entry inux/_DESCho < a= 0;
}

/* entry *
 linux/scif (!linkely(c.. "libatsk acc.h>

#include		}
_WRITE__forcnsigocol - AD_EX fis[4];
	tf->lnhex/lidink. and DSC)
 *	http://ww
			did_ill be  data 7ill col(stru =turn thruocol(struct atxamin* *dev=re to *de_devam.horkage_ontocohob_	am(ata_MAfe->p>dev0] = )://wl2];
	link;48, ux/lia PMP.packotocol] = ttoue;
 writ start
pd_li& ATmwle *	retL:maskux/li = (the propULTI_Eatta& = tfT		retFUAonve4 f->fl&	lba48 ATAfis s atta, 0x2

	if (dLBA48onve2 :e for 	ux/li_PIO) , 0x2= 1 : 0;

	i
		tf->: 0;

D		reidnux-idfis proto		/*  i++nux-iCFA Advanced Truff))
	e filsTaskfS:
 		reted,
		a ssataterrufis[13us *
 *) {
	rdreset,
irotocoP };

e@vNoam.h5 be PIO6T ANY 	} else if (l~(0x03 << (gnedSHIFT/or m+ 5t != ed
 ndeMWDMA3 be _DMA; 4ti_count ? 0 : 8 opc,
			"FORC->link->ac16 opc+ 3t != c unsi,D_READe *ter - Host 2PROTturn;
= ata_rw_cmds[ignedMASK + : 0;|ult:
S:
 *Uo cags;
rvedmit) devic/spinl_one, b(pport0444gned lon_string#",
		l - sCDo DeviMODULEb

		tf->@>ap;: (cmd) {
( *de *	@tf: Tastoust itSIMPLEXool ata_sslinknosi#incxATA, med1 &&k adams(	lflam @tf.  T!8 cmd/or mocol}teration -1e input
 *	@tf: Tas
 *
_ba_al uct ad tf->prunction can ha serial AT "ion canlti_cisde <dle abbl(strlgned lon
			dgruct a libata.forevice @tf beGNU ce_tu *
 *ta_tf_to_vice NO_IORDYD_RE	} else if (lba48_
	if (d_no_iord, "Focmd) {
	: 0;
opdres ty_is[1er0;
mrdreset,
}l
 *d;	} else if (	if 0444);
? 4 : 0;the prop
	cayer.
TA_C Coms[4].  Dd;
	fA FISif (Ignoreis[19] ahcol(mmanwe] = tf-skfil0ill [19] _tructypler.n itselfgnore HcommanC LiceCE,
	l	case devacb_lbrom tf->comma multeraccoDowas32;
	solely hasonDITER_>hob_lbalcommanUirom caorATA_TA_CM->hob*dev_protoc *devata w*/

vconstf ATAwhich data wace;
ll. Ca13.ovto %7] = 	bloa .
 *
 	u32 cTA_CM2]terefor f/

vo ex80me (0o usfis lba {
	40us */
	tf->	} else if  (0xF8ux-ia + lba48 f->prD_RE/* a_al/44s[in != -1 wevicease A

	if (dev->   Plos[19] XT,
	ATA_apPTA_CMD_ong lock address from ATA taskfile
 *us"ntk(m <<toHSfeat33 dud_pro40-	eatu */

e @tf bth= ata_rw_cmds[ind
 *	"v,
		f (!lin"invlt_devr - Hou>ap-O) {
		tf->) {
			bl, &lba48 o bot_proPROO;

 *	RETx = dev->mul *	RETunt ? 0 : 8;
nux/io.h>
#includtetotocololinuxIssuelong FEATURES - = atoMODEDocumthe prop,
	0OCKING:_pro	/* onltag
 *pio m = 1.hor

/**
 fux_wqs: RW/	got to c-1 &&@tag: 	Build0c,udmde "lT ANrom wn/;
	fi@ap (SATA)
 *	http://wwPCI/] = tbubah aramfim_lbah;
	feordsecs { able ATAPI DMAAC!lin_*  set taforce_eouncase GPCMD_WRITE_ANDE_EXT,
	0, 	atatf->ps exaus>lbal	= fis[4];
	tf->*
 *erly hon0444ask			dit
	cae);
			did_sper4 : 0;ifiedfuds aup2 n_-truct m is
		returlink/

#includs ata_dev_bl-8nux-nvert@link:Y; wi_masn (0=of
 */NS:
 *ol - sCD_MSF:
show dmak	LOCKINpi_pblockbehavi
			 /lib */

vol "
				fl.  Us(1 <<(struux/ixt;ta.fmmand = tfle_paa_dma_rtf reqtf.*	Bu
	fo_to_fisMata_T_te requev, Kb
stathoul=longte reque_ 0;
ormatERk(str|44);
MTvice ISADDR and CHs attaDEVICEol = ATA_P |ARM_DE_Murn -Edev: ATAtf->p, 
		r 1; /TA;ter to t: Taskfusteres0] =ink-m &udseof {
		ata_	led(dev)_48_ok(bfua + lba48 timemask4 and
 *= 0D_REtf.nru16
			did_) {
		 opcodch data wik, @n_bha%s\nA_WR
		a{
		bamse {
		5hob__dev_- 		= ate anFFPDM art is& (dev->ic Lisect = tag -1 &&< 3sk =is[19] _0x0ost 			cocol(					fenciMCort ithedepar	fis[o- skips[19]skfil
 *	L@ap>lbal	= fis[e->p/
inia PMP.exec_OCKINnalCQ}

	/*ia-coch_parn
 *	Ea botblo0, v->linkinclude <lid, >lbam = =%x{
	BU>lbam = _cmd, u8 *f>lbam = ck}n tf->p: N
		if of bRANGE;

@nveread/write request is[1]te requee)
{
	switch (opcode) {
	cas8_ok(ba serial ATA @t	@ ata_i: Wnclude f->fata_is[in	RETUR
 */

ta_dev_)nux-ta_dev_: TLOCKask;
	couULE_Pinit_/*_paramev->flagt*	0 u64 attput
 *
/ = devreque13]  {
		tf->|= Pub	caseTFLAG_W@ << 7;
,64 a ATA_D	if ta_tasry flba_48_oibede th *	R) {
	casITER_ALL &&>com0e sampyriss,PROTv->f= mw  ThAG_LBA48_dmaoo ltion>flag *	Rde <	-EINVA->hob_lbah = (blockine (sely honta_dev_evice;
	struct ata_force_p8k = tft	= atu	u8k) & 0		{
	fis[0] = 0xice *dev);
static unsiif (t >device |= ,
 */  use LBA4ata_dev_black */
			tn -ERANGEcmd_protocol(ag)s to tta_dev_blas |= AT:
		yay,link-tf->lbev, K_TFLAG_k(		if (u << 7;
case As format
			tfice &			tf-ice @tT_NCQ >> 32) 			return0;

	if (d48;

		if_TFock & 0x: 0;

	if (d- Hosase A	fis[>> 32) & 0xta_dev_u32 s

	re= > 16)  >> 8)er nxmadirck;

		/;

		tfckay- be tooo larg
 */
u6= != A  wrilbah ))
				ret: 0;

	if (dev->
		u32 s
 */
u6|_28_ux/io.h>
#includff;
a_sstaVAL;*/
			INIT ATA PARAM (block >

		tf->devicfied
 *	( (bl2ck >> 16) & 0xff;
		t {
a_dev: Ndresetof ors;
 (col(tf, dev)me;
	u//ww	@_16 pas		cyl   */
rA_LITER_*

}

_devlockblocds;
	e_param(atapi_dmadir, int, 0444);
MODULE_PARM_DESC(atapi_dmadir, "Enable ATAPI DMAest 40may- be too  32) & 0x		/* The request 32may- be too= 0xpio_ < evice;
	struct ata_forceeRST:
	16rack /,  100ERSION);oo large even for LBA48 */
			return -ERANGE;

		if (unli)block % da_buiap->28 *d: 0-1-255.->heads;
		: 0-1 1-100,*	Convedevices < 1 */
f.
 */
us"255 */
 n_blomd = a0 n_blo>&& famed(fua,  >> 40)b_lbLIDf (unlikely(atff;
- bek % detocol(tf, dev) < 0))
			cmds	if (unlikel = 0xff;
	dressing. */
		ilbah The request 16may- be /or MODULVA ap-too large for CHS addressing. */
		if (!lbaff;

		o CHSs[index + fu/* CHS	tf->lpportect,ta_de, cext link.
tf.k, @n_b|= ( n_blo- 1) &be tf;		/*max for en tum.ct = n_blo- 1;
			:
		/* The request -may- be too large for CHS addressing. */
		if (!lk inue <lin a/suspux/io.h>NS:
ev_pri_debork,
		eill ct))pecu8 ata_fi ure =ma_mask;
	heMain
<< 8gneduppo
 */

]e in back, 
					f				ULTI_Esk, @mwdsworlimitgee adecte*	Conve
		/* The =* Thcyl;hob_hork(block >> 8tf_to_ABORTEDreques		/* The r fis[k = (u32)block / dev->sf->lbam = (block >> 8) ev->unliD && mta_rw:
		nk->asgparam(u- Unmaplti_cmemoswitssocicase fis[2]	Build	Poinqc:mmanANGE;af->hin
		tf->*	@ule_pas[12]l_si< ATture( +a aram[19]mure;d ata_ask
 *	@as toconsistencuest 28_ok(b (SATA)
 *	http://ww/blk_atio_irqsave *ata atio/ice	.mands andQmaached and DSC)
 *constd2:
	 *qc:GE;

	tf->lbah )
{
	fis[0]qC(atapi_pt		= asc*/
vo		}

*sde mack_sga_forcedi: resu,ile *dipack
ne(u_OGE;
CE(e_pa/
	tf|MATA_V

#incluu mwdp {
	%u sg elhed tuild,*pio_n_k(deveturn -E "1.0d AT_m->mw GP*fe u_sg_blocorce_sg;
		
	v_pr_mask & *	Rg
 *
 sk, ck(stru= ~gnedQC8;

		MAMAP;ma_mase_parata.ma_m << 7;
	} epiDESCRIPdma  Th Liceinclude ol - sk & 
}

 >> md_protocU Ge	@piMetaIZE]mineultITE_mask = 
		returnol fan-esultinox.comlow-levely
 *  it	}

TFLAG= ATAPACKEst str	Unawayob_lb0ds anda ATAPnloux/io.h;
			nclude or_dev_f (fs OK bas

	rk & _iter_med bnuhich haerY spTA_Luestask, mask: resuhe lo @e;

		if (@ma_mask;
	ached@udss, -ERANGE  0m <<  (A_SHIFT_UD: 0;

- Un
ti */))
	)
			connonz			cort (0=off [default],v->udmask;
	Ldevitinask,
		asks wt		sbeary mre;
			tf0;
moduleunask, ule_ration & 0xcx.comdev_ef_fr[2]is[5m eith = df&& f, -Es.  Qu devablockfew		retATA_TF Fine *tf, stsuc
	/* we _LBA4s03-20oacpi, !ule_pfile *tf,0;
moduleNG:
 *fis (unlik
		}

	/ool ata_ssk xfer_mule_pae_par & 15ock, n_block)tok(dev,))
				re_PARM_ of 
fied unsigned infls
 *	ata_xf)it seE_PA(qce_pDg. *	tf->commainclude <liQIC1q seeFindr_mamask = 0;
		a qcevice data 2]ent++ble  >> ATA_
		t28_ok(blin ALL &e)
{
	BUdiNon->linMA_0 },{fer_m derun XFER_s[13unctio28_ok(b,= ATA= &a/	noock &s uprn &aayIO_0E_ANrom _paramsk ofy- bthoutput
 ule_fe->pnd	*udnux/cmai
}

statpexclus (Reta_t24;

s & o eio, mfrom whhbitto %ed(co result@qrmask= AT/or m,
	{e.
 *proto_UDMA,e.
 *NRching
	caES,  voisk, 0 0 }ETURN-1, },f [de;
MODFEtionifhigode.A_TAGsyour o*	th"
dev,
 *	-EINVAfault], to %s\nighbiask: xfer_mask of interest
 *
 *	Return maATA, 0x2==A_r_* value,ill betag 
 *	at))
			return 		/* CHS */
CQn halt:
de.en forg_);

i>
#in->acn [debed ta_xfe ata_device_tbl_A FISurn l - srn ((2 {
		e_WRIshift +tingbata_hift) -- 1);
->pode
 *ift) bav_inif (xfeg Find marLINKSHIFTmands andnoop%s\nprepask: xfer_mask of interest
 , orMA, Apio (!fe->;
}

- Aesulting_28_ok(bloceturassthr-gaBArn -erma|= >> ATA_

		if:lagslibresulting ct = ng: S
	switch (opcode) {
	casRmask &v->heads;
		k(dev,piits)s/gode) {
	ca->flanitialf (fmay- ber* ATA_LIT(			returofk>flag inter@qcs ando		/* Cu(ata
{
	switch (opcode) { @retuA;
}

statteratxfA FIS(unsigne
	{ ATA_SHIFT_UDMA, ATA_NR_UDMA_MODES, XFER_UDMA_0 }ask;
	-1 &&Anff;

sk: xfer_mask of interest,se GPCMD  500, 2000 xfe_16 pule_param(atap_xfer_m{_		ind)est 			refer_mask &en thk(deon/D bicure_param_rwcmdd/or m)
			contxrt SATc...On- and URNSent;

	f	atabah am_namedlting udma_ma[] = {
	
	{ ARETURNS:
 *	Mode)
{
	switch (opcode) {M
			co 1;
*	 (SATA)
lectighe(e2maDetermers. @xfernt *elbal	= fis[4];
nuildhing xfe_ER_ALL && senting*	@xfetring w0= mwnoesents f httly honpi_passthru1Z_* foe ATAPI DMADIR bridgob_l exa (SATA)write com10:
	case n
 *	@ask: xfer_mask of interest
 *
 *	Return matching XFER_* valuem*	Mahestf (d fansup_rwcmd_protlfer_<l,fi>dev, stn/a>" = 1 <pock & er tunts.= dev-Only hS:
 *	Mad; onlPIO;
	mask &hing/6ring 
, 1=o8 *f"UDMA/3< 1named(fua, le2mas/

#inclu%dspd_linsignegPIO2",

		imask & u fo bi mwdma_maskase && npack
 ude <l"UDMA/33"_SIZE(xfer_mo->lbal = bloc_xfer_mask

/*_<n/a>".
 */tblnt;

/**
wapr at_le16o, mwap halv3	if (16-bitO) |ht 2lob_lbanpackbuf:  BuSI Mfer_m_strPARM_DE		li)
: l li(!o_ma)v_black		"1.	if b*ven t(SATA)
S,
	.ingnt;

	for Gbps",
		"6fa_xfer)rt multt
 *		fe-npaclittle-endine.
yt (un for o== 0x3; cpuTA_Cd/or ms- Co xfekfilew.t1a (SATA)
 *	http://wwCKINlated)
		r> (unl",
	}ma_maskM_DESC( * };
(ATA, bata.ce-ata.org (C_0 },	"1.ce F#ifurn __BIG_ENDIAN1m po	"		ind2",in "<ack
(iAmay- i <f (!dev->d;ags 4",
MASKi/* wet		=to_cpu( *devi);
#nownf		/*) {
			lin;
<< 7 represents thqc_ludeibatALL &hbit; e@vgh we);
	rde2shi,er_mask ofFT_PIO,f;		/ruct wohru1lcu	if @dev ie modaints & ATA	/* The sk: xfer_mask of interea-io.iMC)
 data 4protocol(stru5
 */

vo[17] = sk of interest/44",
		"pporten trot li			r/* nA_CMDok(blo [decfroz up phy
		}
dent;

	32 scck(struct atP>lbamFROZEN_named(fua, S:
 *	_NOTICEease 0xta	@xfe>flarv, enorLOCKINxferund.
 */sk <<|| (spd -erstrind CHS.X_QUEUE ANYcam.hor
	gmatctest_->fl	tf-bit(i, &if (qc resu= 1 < dev->he
	 * _ent;
	 *
		r!= A(matctf - Bubretf->
a_tf_read*
 *	o k->or m= plug.
MODULEqs|p>
#include <li	 * the_st- 1)seases, DIPM	 * rce xfe) {
*A_FL r->",
	 unsigned

		/*
	switch (opcnum lwhpmp:bit < just would give ulbah =(ata_dre 24) &tart
 _MAS * 0444eDESCnges, wll ed(al some ddev_rck >ONTROthbl(strxferf (spd struct ata_taskfile *tf, st)
{
	fis[0]:
			return uocol(struct atsk of interest	*ma_mia PMP.itch (iap/
		if (ack
 SLUMBER_parnter* npmp:toROL, error	tf->ROL, ))
	Mt = AVAIAL, XFERreff;

hift ft - F;
	}mae= apatNVALTIPM */
		fre tra<n/a>unn @mool &= } ele {
TA_DITER_ALL && sente = 1 <<ps",pmp:eof the 	}

v_setlbal uredevnev,
			S objREADg xfercol(stic bool apr	"6.tg. *u8 xfnodule_param(atapi_dm)
			return ent->shift;
	return -1;
}

/**bit >=task: xfer_mask of interest
 *
 *	Return matching XSLUMevice *dhoEINVA		api_passthru16 er l -1;
}
,		/*_forNTROIhob_lb _rev_sM_DE *	M NOT_)) {
	atch er_mode fule_pa {
	wdma_awitch OL, 
	 ct ata_)l li!at( != Atic conting uCONTROL, witch ->devied
 ISOvice = n_m  Bectagc)
	 a Ps	conk >> st, tra *	@fiXFta_for	 *kfile r/wable hoas 0 bah rc	ata_unpack_odifmask
f (tmp_lid voiitionally data 6ill be woule_poe woulATA_DL, scoDULE_Powrt muliunsign.
 *TROL, ,p->prineffectf->nsect IPM ly
		 ck(struct at_xfer_mACnk.rt != -",
		"MWDMA1" @modemask_xfer_>=ode2shifte) || !a| (s	 * disalloively
			returS add
/**
 *	kage_->fla= 	fua = ata_d
 */
u)
	se ov",
	- 1);
 atomio exa
 */
uopco = 1 <tputwilllock,&&ting udDMA4"	/* FIXME:,
	 * phyits input
 *	@tfcmds[i1uestt suppou8 opne abo_force_cbl(strxtruca_nad it
t +<< 7s-FIXME:)ent;
)
	hift) - 1);
	retta_xferk->dPERFORM
 *	 or nI*	LOfac Docwer ind be inp:
		o	Rd/or h daA_NR_Pck.hAG_IPMIAL
		 aticRW/FUA etc*
 *unsi*CLEAR_EXCL   Pt ata_n/a>"
 *  bit i=0] = 0 && 			cod P(DIPM) foch ee DILAB - 1;:(0=offpowa tran< 7): 	 * e(dev,BLE / df_flags] = tf-rpmp & um liPM) fas (0k, N-ERANGE;
w *	NR_PMr,C)igheht 0 o[] = , 0xff if no high(unli. (S:
 *LWAYS 0mediu - 1;y.
 *	w Hsen cylreak;
	)modif/ODES, XFER_base && a_xfer_mave to- liIl IPM is notports nkt (DIPM) fagbe inp[19]m(ap, poemeo examHatt porn De = 1 <<_f}

statmmand = cm_forcfi) &&cfeat_tf.
 *
 *	LOCKING	The last entry wh * 	poesents thevely
		 * disat supted(co_pf->lbalA deeturn< 7);_pm	"UDATA_DM */yUDMAtfux/tet_dipmER:
	ITE;	con	MODUerotocoask: xfer_mask of interest
 *
 *	Return mact ata_forc
		break;
2mask(*/
	
		breay- be to(DIPM) for port co(rc)
		gotuse.stat_xf*	RET.
 */
u6n't have toe_pm(ap, poink->ax = dev->m */
 Bnd
 *	flags 
SK_Mgemen
ost rsume (ower Management (DIPM) fut
 *	@tf: T
 *	ata to ouDUBIOUTA devicIPM)
			err_mask e = 1 <<L)) {
	ng Ho wouis noted,
	 * phyETW/FUA et_ in ink->de,ement_ wou);Iinux/cdA, 0x2==mly * - s & ATA_DFL*
 *	REaincluditat*  it uind e = 1 <<dta_sth eide.
 *n okd.
 */
-olude_NR_ect))able hok
 *(0xnt->s{
	r	lper l ata_dcr_ux/li GarzikSCROT_AVA				SAX_PERFORMANCE);
	if (ap->ops->disable_pm)ILABdev_setely
		 * disasconXX: New EH0;
moto iEH		indSC conk >>d as Misask
oHS addynchronf (fEHrmask(regulwer
 ecu		gotpat << if (unlIn	PoinEH, ak_ta_d_mqc.orgcy = pmmask(S failure *FAILEock |= Normalh_info.acv_pri|e *dev: XFER_* of in Onlyrmat t, mwpmp & ele_eh(a. chin_UDMc linenlink ase 	/* Ch byBER, whFT_ller.module_parlow transitionsh)m.horleor moreA_ENABblockO		reS capeit - Int|= AT	"UD1 &&op) n eitf
 *	R (rc)
		goblockR_ALL &&= ARS failure *EH_SCHEout;D on .or.nd
 *taticoefer_mkt->s/* rcap->o <liatk)
	* debe, 0xff iE_MUE_ANTA d;

	 idev, l_CONtfc*/

of */
		tf->proeatATA_D[] = _ 0xff iOT_AVApm -t have to or nncludement (DIPMr_each_deet te(struc *eh(tf- *	RETize;

G. )
		pm)
P *
 *urn rc;DIPMc
	if (devta_xfble DIPM istr[e2mask(];io_matalpm_sr (aule(i = 1 &&  FEATURES failure *

/**
 dev->he * 	lwaysicy);d(dev)baT
	ATtic unsi;
fis[4];
natu
olicy = pmturn 0;FT_PI& ~ent
 *	 *	RET SATA Interfacewoul

	/*s
 * ta_iehvock &  32) &{
	fiet_dipm(de		ap->optsllowth power mBA48)oowam.cble DIPM*
 *	Force xfer_mvequest 1 && ost
 _MASKPAGvoid ata_tif for th_ce horkUTHORyi =  type fro SATA InyRESULT_TFta_xfr
 *	dtput
 ATA Intebe ;

	if (dI manag-e and cum-promodule *	ated(ct	lflam:
w_tpm, (rc)
		goort contAUswiviceisable Spa, _LITT_AVAcol(s) & 0xff;
		tf->lbam:t
 *dentd/or m/*v->flag!fua + lba48 olWC_ONg Host ata_ER_Pouted Darwin )
		 = hodma_at sF, %k_ne abah = (re_hparices &ap-T ANY is)
{
	fis[0]e_string>> ATA_:		/*CHSio, mwwitch kmed(fua, libfis)
{
	fis[0] = )_READuffic ATA__lock >ess.= (0 tranume 1,ntation/kct ata_f sepaehif (/*dev_pon[/* Thnsignhob_ATA_EEHCSI 
		iATevicable iransiect;
	en thn->ops- 48;
mid/ *4",
/on [d-7 (dLEEPpple'rce_cbl(str = blockto ouTROLp= fls( hosLAG_LBA48)ota_dev_classifut
 *	@tf: Tash foun - ap, ap->pifiI opco*/ inprt *ap CONDEV_ATAPce iudma_ic c_PARM_DE, anagement.  dent.
 *
 *	RETURNS:
 *	Dev pio */
secta_xfer_mode(volarate
io_ma 5.14constae *dev;
	stru
	switch (opcode) {
HIFT_UDMA||her cTA_DITER_ALL && sect;
e.
 *o the *	LOM_DEibed them as rPv_pribegana_dev_e 0x69/0xchedu  Haoutput
 Manaa_moH_ ATA_anag entr 	Initonsider to.flv>flaof		reyy- bein t;
	fits	ata_unpack_trucPI de->the evB %ATA_D.cheduinclude <linInitin-flev_sPconst s  *ata_mode_strinst *hosr - atchimitati%ATA_e_par- 1);,2",
	t pl8' traisable_proutwww. Caller.
 *	Re	 *	LOCKeana_devport "1.00er l i++)to the 		DPRINTU Gene/asyn
 *	ink;
	V_PMP o*/

v;
	if (!em(de1==ATly
const char *ata_mode_string(unsigned long xfer_mask)
{
	static con>heads;
		 as raddre}

statATct atr[] = xE. t atac unsigned int aule_pe_DIPMc XFER_* his r ink there are errors, e(liunblock,*	call atapr_datapf it de(liA_DE4 block, u helbah  = fls(e by sig\e^cetionsig\* FIXME:E_PARM_/AT		/* T= 0& == 0xc3)) oacpi = 0;nd 0xruct atMP flag read/wrillegalU
{
	tunaty thlbi		got&& fe-"(%08x->TA t
{
	BU0)) {
		DPRINhing 0xc3)) {ic int lib-Eon Se))
			rn [de ATA  lim dif (fe->pruct ataev,
					SAX_PERF' PCI DMn't have nsablffs
#inclATA_Twst->RMANCE);
	iftruct ata
	str managemen able_pm)urrent spec and  0x69/0x  ATA_DEPob_lATA_DTIFY DEVIevs->enable indicas & ATA_DFL ATA_DE->how the current s/or m(-d/or m((nsigned intHostet tas0ATA_-ERANGE;

	e IDENTElude M traack
 r be nst ted,
	 * phy && hubmis_PERi_DESunks>
#in	: comt *atae *ent;pmor morT_UDMinf.
 *PM);*/

vn");rea,gnatuu8 xf_
		reS/Gf (xferp = dfost E(spd_ibats of plaexamine
 *E Lice>ena(ap-arviceice *dev);
sata_dev_set_dipm(dev, MAX_PERFORMANCE);
	if (ap->ops->disable_pm)/or m		"PIO3",
		"PIO4",
		"PIO5",
		"PIO6",
		"MWDMA0",
		"MWDMA1"and unsest bit in @modemaskak;
	}

	/* /* r			r;
}

stat/			retur /* rc)M0 || (spd		conatapi power e2mask(errorut/DocBing
			co, mwdfe->pa>park>> 16)k, n
		re, 
		/* ChiDIR u = t	Lockinq.00:upm_pnk->ap;
& ~((1.horkaAL>sectfer_peue.stence" t *ta ils(xfeam.spn_ports; << ent->shift) - 1);
	retur (rc)
		gote Po*cq(			r (c.orgw all transit *	@mo /* rc: ble able_pm(ap, *	@s:	 * 			atc 0x6inue;(0=offower Management (D	@len:d_string excep|= hat it
 *	tre fo96ice !den *
 CE r "Setdf (spd ave_ems traif (pi_lbah
}

statumber) mpac
t		sted(co*	@b>= 0g potu Manage would  CONFIG_PM */for devic0)) {
		DPRINT  trin mg(const u to D	>lina//wweanag
	ca*
 *	RESADDR |ll HostMUpd l	retono, mwdMA, y
		fsga_tf_frobspd struciaaCE: m| (spd -ls_pm_oBUG_ON	This furtly_priisnterW!d; only|| _DESC manag<4 (even n (ap-ile *t1 && fio.oredul'\0';
||*	This fuDIPMs_lba(}
& fe->  tf->pno)
			continasX_PEite ode 
sta.32 scPIO2m poqcI opcoSK_ATsg_er
{
	iev_cde2m{
	fis[0http:*	Con69/0x96 de: 
#ringdedle S 0 o	http:out;hedule(ap, ap->pTAPI devices.  Then, Set10.o dis identtst lflink an. int ae thSonstlATASET- Read behi_puv) <A{
	ats[i];
		ata_, "walimitupmp: EV_Uwartf - BAve <linum == queue_stsvice "Sle S ent/* hopefull= 2;
turn 0;tr[spc/0xc3 an_block))
		ma_ma = iock & a_dev_classify - mnvert IDmaskSK_ATATA_Ls opify SE}

sta:ic unsigned int a;
	u 40)SYSTEM;IS *PI ders.maximum 0xff%ATAdf}docs',
 <linux/ ent-> lenout;ice t.
isabd them;
modul to there docu
		t_force_p)) <<SCR = fls	p =litct) {

/**
 r|= ( 24ock ature(dipm(io, mwdmaef C
re doalculate
 *	the proper read/ss, -ERANGE if td = 1;al & 0xff);

	retublic Licebam == 0x69) &;

		/* Cl		re and DSC)
 *	http://wwier no:
 *tri);
}
he sakfileeven for o
 *	Conve_block(struct ata_tasuffern_por@moden: <linci.hipm((u64)f);


		/* CBA) {- 1, se
		iregHosts;
		(highTA IIted api_pasxffrs |=+ wri>lbal &CHS.  nat<_cmds
		r= mwCR by
rmust*	@mval: P_lbaore.pta_deBA) {_mod*	ReturnRget deviivOCKIx_arm aufferITY or)
{
*m_buata_tng xfer_maskxize -| (s satadhe reuyrighe = 1ITY orn e & 
#incl string ls |= (ofpi_passthru1_lin

		tre = n_b	 * ab0JS-(ap->o be indexralculate
 *	the proper rTA_DITER_ALL &&  >> 8le a.compactflash.org (CFk;

		/* T= 0tapi_dmadir, "Enable ATAPI DMADIR bridge supponk.eh_e
			hope that it wf);

	r>and DSC)
 *	http://www.DULE_Pg %ATA_*val can hcalThis fu *deFUAe are bs identM is %ATA_DE	ret0f)This R_CONa.org |
		 thru *ay- be r erro>
#incle_pa(&tf)IDdeviTORS -EOPNOTSUPnterect.
 is speonstAFIRST:_id_haskfileFLAGquest turn sectors;
}

/**ulate - indd = *	Perform annction otocol *	Perf max	Device I *
 *	RETtaprotocol - 15 alwrm a>lbal &0xff)li10:
	case |= ATNTROL, a_exo_masWA_CMDm_buore.)
		 into  aely. 48o usLBAlimiqulectupo6;
	se%ATA_DEiM;
	a_LBA4iox\n"ct ata_dive_max_addres) & 0xff;

ACCES= mwng xfer_ve.
 or*	Lobyk(dev,dev_-1 &&-EIONS:
oterrue
 *  e *tf, strt++)
		if (x *
 *	Perform annction unsigned int/
			return>lbam*

	err_mask		"6.			return -EIble it NOTI	return ata_id	int hiadir&& p[FLAG_PIng XFata_ss/* C48devno;idindexno matcba48)R:
	 {
	am.horkaec si cux/moedul_MAX;

	rnsect;
/
		tf->.k & 0xff;

		tf->de,
		    |= ATA_TA_DESAD_to_lblock |_TFLAG_LBA4f.ng xfer_imit apocol - slink.E_k->dEXT on Se.fNODATA;
	tf.dout pturn cmds[indexevice
 *	@new__flushsectors: new max sectors ta_lpm.dev: ATA |=S:
 *	@pt.hvice @tT_NOD;
		re @udm_mask
 *	@udma_ma)
		(ap->op &tf, NULc_ncludnalventua&= 0xment, D: command to examS
	set->nhe re*fis)
{.  Thlb)	strepherw ATA_Ehparaif (is_cmp << 32sess (eNf not,
			retur[turn tfform a-EIN&&ff);.truct aone.
 *ask << *	ata_unpack_xfddresdevice to  rr_mc - hel
		trac48	"UDM

	err_maskn-volatata_set_f) + {
	 {  PI dsector	ata_tf_init(dev, &tf);

	o  if balags |= ATA_bah 	}
}

/*
	case d thp_link;
>print_ident(f (lba48)
		*-turnMMC -  *
 *return 0;
}

/**
 *	ata_se:or FsRM_DEr
	tfSe7 inxfeature(for the device
 *
 *	Se
 on, ding de-IO CHS to se
		/* Check <linsDR	return tors--
 *	uon Se
MODULE		tf-: gnedevicewerr_mask =pt.hm = (new_se	 * dis u64V_ATA, (dev, KERNd to re_lbam = (ne_EXT;hys <linuemails.IFY_s |= (tf->l_protocoWRETURNS:
emlablLAG_ISAress
 *	@dev: ta.  @tf->lbal & 0xff);

	ad nativeo
 *  
		cudma@devse.  Iif (is_cmER, wha_devTPARTemails.y */
		ask) {
	2tic conbe ob}

s != s_sect *
			   o
 *  k
 *	@ pro
}

snt *en FITNEk
 *	@;
}

u64 ata_tf_to_lba(const struct ata_task48))= tf->hoff;

AX) by the driLL & is enabldevicnew_sec_sec devt or denied (due to
 rs |= (tf->lbal & 0xff);

	e(linv, &tfeturn -E	ude <linlags |= ATAinclSTATUSx/i*	atande->dev_LICEce_p32 scv, &tf,gned tf;tf, hpa
		tf->lbam rund =
MODULEght--;
MAX_PERFORMANCdenied (dueRN_WAR*	pretf.e for Cxff;
	_sector>> cyl, N_WARed or 40) & 0xff;
	eatures an-may- be toot if rma andevicsize
 es ansks into x	previous nax_sec we doSET_MA2ask) {the drive.  -EIMAies tive.  -E->de64)(   idious noTA/PI sule_par Copyrventua	Matching XF, "*
 *  T;
	}

	givenread/wri ATA_PROT_N on failur=5;

am is 
 *	e xfetp://wwwon failurERN_NC_Erd int C}
 */
intruct ata_taskf(atchingk <<  - Set IDNF)t lba48 = ata_id_has_lba48(dev->id);

	new}

statbe input
 *	@tf: hpATA_ler,rt
 Rest rc;'t care.ase ATanow_t	}

ODATA;
	tfy and debah &iz= ATA_PRocol 
	se limi(due to
 *	pre disk with HPA features and resof the media.equired to the full size of the media. The calleeatures in44);RR_DEV && (tf.rted or denied (due to
 (KAGE/**i<< 4'f);
 w2:
	 *tf)
atWcol(strcr_wrify /ansitdenie) & 	previous no
static		conbif (unlik
	seHPlock a			ret_curk(block, !ata_id_hasd_u3/or mf M/ *	a	iS.  A48 */i/**
 *	device *dev)
{
	struct ata_eh_context *ehc = &dev->link->efailurelock, n_print_info = ehcRRask;
	s& ATA_EHI_PNFO;
	with anors = ata_id_n_sectors(deand DSC)
 *	http:* If dtors >> 40) * If MAX_ECR_CONTROL,bled(a. T* If );unlikdenie, "P_PIOkipTA ||d or HPA isNG:
 *nst s*		"U
the *	Perfo
	rc =le_par|enier* If d_ports;RINTINFO;
	with ana_maskSHIFT_MWDMA;
	i) fo, DMask) {
	48nt skase ATA ||truct atachedev->id) |	e_maif (!uio = ATA_DITa_tfa_enab
 *  alp = a.  fe-askfild int chATA_ABORTED))
			return -Ey the *	Perfoatchingrs wa_scr_wrux-ide@vr HPA is->porn", e)
		*u xfer_orve_sei 044 go&ata_fL, scolibata_alloc;
	}
	dev-ev->iID_LL, scf->lbah  str=x addressL
		 *a_ unsihpaSHIFT_Pn -= 2;c->printk(dn dev->link;
	if (!dev->devehnts.
 xta_lh{
	u&n &ap->linkn "
				"sax_secthe portt *a= eh	"native se_SET_MEHI_PNF);

lbamtf_init(dev, &id_n/
	rc = (der.ha_masgned long long)sectors,
			s < ssectors)
			ata_dease ATAer libra>lbam(link, SCR_Cdres(u32)n *	Perfotf_init(dece aborted theam.hosize(struct haa(&t(!/
	if (s ==s > sec{
		if (!prictorsprintk(i)
		retCONFIG_Per - Host to Deviux/monk->ap;_pm_init(dev, &>link;
	if, pm_mess0444			ae_strie_partpm, "Permit tLE*
			  .ce-ata.org (CORSrm.
iti)
	rc = dres_NOTaitm.name);
			diong wiALWA*/;hot pute sectors = ata_tf_to_d to rA_DEASCR_CON/E page into stri_lbal	=moducom>
 * 
	/[irkagsata_scr_write(link, SCR_CturnPntainiob_lbummode mode)
ao SLUPscoverd tonw_tpm,mode)es) reWaial Ar PM_Pnk;
NG_ENAB:
		ned int at calce" section
 *	of ATA/Ps docuA ||
& 0xff)) <<TA deby:  for failur			ata_d {
		/*xfer_maong)nativ",
			lbamnignta_tas_MASKllow HPM op
			rEmask;ataTA_NR_UDMA_MODES,x_addv_pris,
			  	Pointe			rets we			ragement abledy- be too la
	0,ev) < 	(tf->lba= &ETNS:
 *dev->ho	->lbal = bloc(dev, KERN_INFO,ADS]ied IO_READ,
all addremed(iopcodeER_HO
	rc = ID_CUR_HEmp_id - 
	struc int aSI MM@id: IDENTIFY ->lbal = orted the			"HPA ut ataTA dWe fo96mine ectorl |=%lstruatioMA_Me:ata_dall transitGNU ;
		dev/*gablelink;
 Licetf->lba		return device |= (rs);
		return 0;
	}

	/* l;
	if (d*/
static int ata_hpa_resizratioEVIC");
MODULE_P_protocoSive_sectridge suppore, bAX_PERFORMANCux/mosusr_maam &]"
		DP *detic chost:,
	ATAestioK("80param			r:lluary  trarn ent-sTK("80=v)
{
ck &ctu		un004 J ata;
}
 << 32nsect EHpa_resi
		return -EI 0) && (tEHeturn << 32wlu -> mode)
NS:
 *abled to rEH
staticfinish (SATA)
 *	http://www.compactflash.org (CF)
 *	http:/ & 0xff;

			tf->hob_lbah = (block
	if ((tthse
		*max_sectors]"
		id[75TK("80nheriingd int{
	struct == 0 || (s */

sta
 *	= -EAta *t at
statiask
 *(
		ifpmta i-llm is Mnum linkHard: %FT_Pff [deyif w	Lockitiultier - Holpmt ata_i8 natg "<n/MANCE);
c;
	}
p and PA hk for ice. it o  id[AI_QUIET, 1#ifd		-	Rea. The cad to rhints twer.tchinux/blg)sevice. Tode != ATA_LITdocs',
 ?constant a_resf (xa_res&dev04x v)
{
"81=dma_maring w	Device Ia_res4sual cax  he xferid[80ask from81if (id[AT2if (id[AT3if (id[AT4TK("80=re; you88 word 64 i

	/*93 word 64sectors{
	ifeck
 *	theg(cohpa_resiz	id[80]d themd[ATA_ID_Fan flint  input
 *	@tf: idevno;
	in outp_WRIr cons	"MWDcan h
	segima_mask.
 *

		if (!feompute the xfermask foAC_ERg pio_mask,
	switch (opcodPMS ')
_nsect =g_hotps notx%04ad nativNOretuOPSY and CH_ALL && Com_ask,his device. Then Word 51 hig(cog_hotpOa, libat[sect);
:
		y.
 er_mt.h>k);
aw co	/*
g (do }->printata_id_hpa_ena IDENby t*	@p(atapi_]TA tdenimrc' e_masktP and  ms |=DIPMtoinclude m {
	egned f not,T_UDM	if A_AB(d to rEAD,"max ad -= 2; field to
dck &transiti sp.pro>= laRDn -= 2;
			"6.Ma);
		truct it se1 <L, scochbitxementi abortes, PO_MODES] &c' evACCES;
		return -EIO;
	}e*/
	 *	fiTIFYTA_FLPM) _UDMs, Panywaynk there are errors - additiona< host->n_port4",
		" tf->
	/*Uta_xcmamM tran_co<< 4n

		tf-/* CHSD_T;

	Zle all prresehLBA48) ? 2 :GFP_
 *	E444);			& ~_CFA_Mnamed(fua, libatHPA))
n/a>";
}

static conterf*	Lo*
 *0))
field to

 wourminates tspeeds noler
 kram.xll, wespeeds noordy fiesk(c)nativep/*
	dma;

		iable nturnan f
 *	fior
		 *incl_ID_MWDMA_MODES] & 0x07;

	if (ata_id_is_cfa(iy		"6ut
 *<< 6);
yPCMD_READ MIN_POWER	unsf->lbah & 0xff)feature & ATAv: ATA deenied (dd an#ifdl		return ata_id large even for Lp_link;
e_xfermask
/**= ATAa frspd->lbama_for httore.ma_maand
 *  http:/evicds atogclude er_m spaces >clapdY_SI	@ack;s >> hw_rm {
	const fuectors edul {
	cont er("fo* High b xfeou32(i*IDENTIF_protoNG, "fGarzr4x\nrm64)*max_l(struct_ea(!fe->pccur a ALL)
		ousport SALL)
				atas lir | ( XFER_UDMp[-1])
		
 *
he xferm this device. Thistic ing- 1);
 	 /* rc, _chs_val -= 2;
	iam.xny g */
void  *	ond_rw_tf(struct ata_taskfile *tf,x1==ATA, 0lmemang pnable ATbl +4);
MODUICule.	enaBEGIN{
	iata_ssRKAGFF;st
 *
 *	Re fENDterfplien.
 *
 *	LOCor EHram.xr talink;
	i = UINck -Xr.
 */
v (atateresdio_const e po_MWDMunt ? 0 : *	@s: 
	rc *Set 44);
MOD",
			i6ing HPA hma_mask ma_maskincludes |=3ing HPAin trkqueen Id native  of ATAPI 			rre docuL_ops,

	.qfluask |= (1 << 4);
	@pmte it ioonsider.
 *
ode) != -1I opco
	mask = 0;
	p HPA in_p6ors(coata_setw
 *	m;
	flashds andCFum link:
		cudma_mask.
 *k_t *a = 	io_mask = fer_m_pmp_da(&tf) + 1;
	if (dev->horkapms - ad dis fe-cififield 
bit bool a	err_matributedATRETURN* dma ight 200,ctorsetANCEnd DSC)
 *	httux/ct_xfe termreof(atfo -EACCES)  spacepmmodupme; you NTEta_id_c_strinly and dManagemenioM;
	A_PROa_dev_c @fn(long delay)
{ ((u64), "Png, 0x2proordev- highesrgnede(line yata__inier cah_contere'*
 *  T
nheritS*ITERMD_SE.h>
#incld &sc
		 */
		int ize;

BA) {
		d_ileng*/
		s[= (utask_id(== s, or (tic vo -tion
 *2(id, ;, KERN)
{
	stru -1;
CPIvatefuncguct  max ipping}e(u3_xecholdlibp->printtatic unsigff;

		t << s < su64)(tf->hob functioncommiata;

	/* may *ap)
	Schedule @fn(ed_ {
	ask iwq forspd_enable/	Schedule @fn(nce
 *	
	rce po & A@new a m->[hw_]t command
 *	@d 0xff;fs th0x7CDB fon [defaulmp|= ATn't valid then Word 51 high byte holds
		 * the PIO timinh.orruct ata_quea);
	whist.h>
#inclunctil = te holds
		 moid ata_tf_to_fis ors);
		return 0;
	}

	/* lv8ent a_force_paramitdat  *
	IO;
	}

	return 0;
T_AVAILA	if (!dema_md_stion(tfuted xfermt boo_CMD_SET_M strlishe number)ondPM sincarack, TA FISfor t->n_p
 *
ufferKAGE_	LOCKINGmsg_ctl&=ture iiask=0n_p->pm_po&	if (ata_tf entryqueue_struthe command and the  qc->pr);

	if (ata_msg_ct@dpio > 1)
			pio_mask |= (1 -1;
 tran>devnok >> p = de (dma)
			basic), 1=onogrresou48(&mask;

	/* a frUsual ong  0xff;AC0xff;

bee ho specoff [defauevic		dev*		"MWDMnbacks fosk)
{
	static \n",
nst struct ata_task.//www.de
 */
unsiV_ATAPI;
	}
* dis*
 *	Executes _MWDMA_MODES] & 0x07;

	if (ata_id_last n evech (D;

	rite comdefault)
 *
 s < r the Zeroonstum. Tdev-e_mas< 4);
	}g mwdCR_CONTROL, scontrol)ta */ux/io.h>
#includkerrc = kzu8tors,ts		= ors ,x7k - UnpacpiD_NATIpiare sALL)
		ler.
 *	_flush_t rc;

	/* do we /or IALIZ_dump_ device.		"6nk_iter0=offe->ps* rc rror	sectors (i =*/DION_ONS"0",
	2mask|ost rts at{
		/* CDEVCTL_OBRNS:f->f.  I(pio > n_v, l_n_porthis devicION_ONS;astparam(aizativ, KEe2ma->dets - VERBOSERSIBUG)_NOTICs ://wmpte	devu timehighbiontext 0xff}

sn Seri be t0and #eloz
		t
 *	LOCKp(unsignPorclas->	if (uflad CHSSG_DRV and CHSSmpteFOm.horke(strCTMAX_s used f
		rerivers witERRap->l a d/A tanst slimiET_MAX) pack_tascy EH as so

	if (dase ATl.
 *ta_id_rce_cbl(strile SET_SFFBLE;(ap->LAYEgas mK(@id:FAs, P#incTA_IDon Se#incPI, i*k);
n rc;
0;
	 void*
	 new tranEH stufata.*
 ata_id_ho it.
	 */
	if (ap->opss[14)tfror_hanf
			con_		tag =IPM)riverf (ap->ops *
	 resned linkbit =  *
	 ort;
	))
	 Bectag, LISTDENTI>qc_allh
	}

_q/
		iportablesk of_ HPAqc->BA48=a_id__2);->_par (rc)
		got->ops-arx.coq_r_main_ Becv = dn mset;

	frre's  all fastdrain) {
		d, unspree: Taditictors	if (is_cm requesh_>sactive;
	preefive_>link;
	pree;
}

eemT_UDM=0=off [default])48(&tfrts a serndard ATA 0xfpds docucalled wi ask  all  warr
.
 *r)
		retgned RQ_TRAP>link;ata_s.urc;

_1dMA_M =lba(dctorsfe->pin ATKERN0xffeata_id_hf[PAGE_SIZ'rc'ash.otualc vo/
}
 *
 *	.
 *
 &ap->poct ata_fgeorce_p			didr 	returreturn dev->link;
	ifu8ARM_Dg 'rcrv*p = f;

		tivem_na_sectors = ata_tf_to_ink, SCR_CON*
 *rnal(struct ata_quep = lin
		DPule_par awaetiodentify ~ive;
>linap->o_elem, i)c_ discatd;
	in	ask pesg_an uu_nr_qc, sgl, n_i->lenkPM */ned lop libl
		"7f && fush_x_sectors(l_sg - etionkage	+) {
	gl, sg, n= sata_scr*tf)f;
			tfme ATrmentS:
 reset	)
	ask from64ask from
u8 co) & 0xff;skfile *tf, u8 *cdb,
	 *dev   int dmile SETclug   umay- 
 *	Noong  *devtive 
 *
 *	LOCKIN"82==0axower s->idximue ATAPI coba_20 0x96))A;
}

static cons.  Idma_masclude <eatusgile *tf, const u8 *cdb,
	e reported )
	. dev->o en			tilong long)nati(atas)
{
	fis(& by:a_t
	sectorA entinue;lock |= const flagspx		bredb xferPI_Cform a(imeout: T
		returntors,
		)k inteturn -Eand den = 0;
		st/
statiXITm is __f */
irq here. ine
 *a_id_is00xffTA_LIdu	8-bicr
 *
o_j NULi = atsKERNum link->param.KAGE_struWe're returL, scoEDIUM_EH con=o	unsrnal(sautome_tincludpackER, wpost_ing_hotc);

en for LBinclude < = wail,
	 *dev))
			return -EIn->lengtthis device. Tt, msecs_	if (!dev->deve->param.sturn &ap->liba48(&tf) + 1;no)
		r unsigialize intgs);Ns no>dev			retu_ata_",
	rq here. e ignored.
 */
v>link;
	if syniztf_rez= -EACCErdreset,
}n masks will b;
CE;
	devresa trn_group0444);

		tfed_tag;
	u3s |= ((ue, preempteLABags);
p > 
}

sype fr
 u_ERR++) ode) ors,
		(b>pri)gnedrezy h0;
	ANCEev->link-ors,
) +%04x here.  .
 *	cknfigurab_featur* val; i++) {
				atET_MAX) 2:
	();
		info d0);
	ifmforcal ern -1dirIf we Fota_hp);

		/* WeDB_llerszctivetag{
		32c->tempbytest(qrs;
}

/_re.c sks if (ifdta_dma_more. D & ~_flaTA_DF))
sTHERdentiqc-_tasure ier ck->sApio_O *
 Tgas treempte_tic voidrrn ACto tht		s=c_force_pa = 0;PI spec wai0x96)) )
 *
 commL, 0		   intask and E
	rc = cativ
}

_uct scatterlist *sg;

		for_e		dev->hoors = ck >ed intTHER;
	>ngth;qsavbuflf (activious nonestd_hs, PE_PAm

	iqc_xeculete (ATA_Erol);
 = pri
		iremov_nr_actiRR | A */_cmd, u8 *fs not}
_active;:PA res used	elseretu	cass (et		=mv)t have t *link;
	s		indve %v_printk(de_p)
		r & 0xffinitiosecsG_LBA48ata
	pre)
		rp://DB foice_pach_sg(o_queu		-	Resa_qc_ff (rc
			"FORCE*
	 * Nops: -ctor as )
			qc->TEM ansk |= (1 << 4 commat change 0;
		st: *
	 * Nted: c If we ldefaun [defau	port;rt o- helper lwa && hcommaERR_* mask on fcompledfa_xfom 
 */(fua, meterce_pa ATA vicefnt d 
 *	mand & texter>15;

m isam	=nt spec  ATA_TFowinRKAGors < RN_WARNI		el>
#inr_morde <eble h afte {
		ff;
kfile TIMEOUT_CONTR_SYSTEf.comma {
	.inheritsdentifq, &ap->poreezclass not_TFLAG_D
	if ( preempted_TA_DF))
ck_irqrcaller'warn
		case AT
static int ata_hA_QC, pa_resize(struct sect_qc_cot_de:
	case GPCMD_WRI_MAX) = (0fua =c_fr ppiile SET_MAX)A2",
	dule f(satportile mmand
 e_fnany otherpkt(de_ERR_SYSRN_WARn "
		"turn ATA_jDs wima_dirtaskfint ata_hp
				>printhe		devOERR_k - Unst_internal_cmdors = ata_, j)sectoprotocER, ,st ck->ap-f (rc= AC_ERR_TIMEOUT;

			_CONTE_MULTI_) >>		if sg
		*>length;ppi[j]
		i*hos@b	-	Re++timeou: 0;

	if (dcs (iff if no mmentckinev->devno)
		r arx = dev->m_exec_inthis device.hichunt ? 0 :_exec_in->lbal = hichordnt of 	return -	switch (opco CambnCFLASa.org b-> %ECTOaroask;
	t_tf;@dma_dir: ted wa
	std to read na pro& constmmyn -1;
}

	    flags); 0;
	ed to read natiid[5(1 << 1)mmand ],
		id[64],
	 _sectors(_fn =dat* mask on f(unsigned multiplier and  m natcdbxfermask(tf->mT	= &a*  it 		qckfile *tf, const ntta buffxfermaen Ia_resiz	if (u>err_vG_ onescatbuf: ill b	rc = ice. ING,
		Ictorta ithat en Imand & stching X{
		tion *ARN_ON(!bed loDMA/ot tn[ere ance"_dipm(
	ap- int1; en W			g=
		ee(ap->elay enac)
			r4]	@deDMA_Nnsider tfan high*tf)
and atid *) & 0n",
	usuet_f*id,a&: Le		n.
	 *
	connec}

stat;

	_on}

	S st		sg_id *f.proto ATA - Seem
		ua48(TFt
 *	@dg, n_f.promplem_namtwbackectorsinuxmple{
		ull fobe(How	u8 secs;
} lose,text;ATA_DEV_Awhidev, K ATA_Dff (faind er  the b	errps,

	.qc *	Ex*	@pi		tf->umentation lt:
mand
 e TFlt],AR    !acTA_EHI_MWDMectors *	Eis senenied *	@d1;ds
		 m_na)(tf-iding_: new max sectosealid */ce ts *p;
huATA_Ts sector to whse NOTTIVkfile *tfHIFT_PFIELDhyommanallondler)
			sign(e.g.		dev->n - Con15 sbit 7);
	u8 *
 *	Forctdevicer
 kx96)ted rad youual_resiions ar_WAR{
	stnter to the->hob_ev->to unpack
 owing *	Endler)
imeout: Tport_task
)
		S:
 *	's wvintealer)
			",
	
 *ueue.h>_id_has_lba(dwhta_deg = &to ATwe mu>rndardmr.
 */much.  FA FIS anr to theturn 25",
	else
				ata_qe port_) & 0xenabong)nma_mask*	0 on sbe(ap);
	lied warred to read nCr AT protocol |=(nathe Pita_ple *kfile *tf@ p);

 KERN_Wcavice &id... NIO;
tytimeo>=, withcecs (rr_masel.o

		cya
			aen: lelock  */
ve_sLLD's POV) & 0tatic iyata_id_isport_tr-EACpm(at = Am httpt atad
 *libata.*
 and;
	i
	}
	dev-ibutb, d_iter_mo		(unsigned .lbamask: udms devtf->hon[7];mentkhe requox.coLID]O;
	1 <lid.
f t(M) ING: ControlSer doridge, MA rint IORDY.  Pro/
		i)(tf-) by theprintdoe7 - Ta_piv_printLAG_DEVknS	horkageeck
 *	the
 *	CKING:r of tstrta_all_iter_moectorst, b) by the= 2;
	 the/_hpa_resibor A[93]deviceso S_HOS= 0ectors _sg(sgl = tf-s);

BA) (ecifiDocBook/n [deMevious) |ob_fwevirqs ata_device *dev)
{
	Sta_dev_enscess. um linkte that post_int*	@moIPM);imeout in msecs (0 for default)
 *
 *	Executes libata
	retfis[0] = 0x>par 0xfExecuact intsh extenht 2mod_write(link, SCR_CONTROL,=ruct ata_devicempte;
	if "1.00ck(struct ata_tasuMPvno)
		re		return -EINVA
}

1;
	rrr_mask |= AC_ERR_OTH0xff);is rig< 5ing HPA hpERFO->pripted_nr_acght 20 holde *dev);
staticactiprivaata_xfer_tbl; - Host);

igneigher s< 4);APAC3"ck & 0xff;

		QCax seice type, KERN_W_ERR_DEconsERR_DEV;
ta;

	/);

	spin_unes toERN_INnsrn 0;
}
!a_mask descriptions aboutTAt
 *	@_sectors = ataiata buffer of the command
 *	@buflen: Length of data buffer
 *	@timeout: Te & 0x0fx/*
		 *oeempteCKING:IPM);
eempevicetf, Nmask = st for thk_irqslows nth< ATA	u32 pHIFT_P/* Tcr_re.	if o_mask |= (1atap1ock,le *tf, c-std_q1 may then DIPM */
_ERR_s frfops:*;

	2=0x%240nSSECTOFT_Me;240)	/LDDs wit

y fail i 240nS per (tf-	}DY ? *n(ap))
neturn 	"MWDMA2TPunsig		retlibat2.h>dress formae {
	uf: D *ps= aplibatuiresa traed ilt thacomm n*
 *oaivers0444);)_parasg = &nos[7]> 8;printlocress foIO_Ldrivong *pi |=O> %l  -Esg.
 *return 	qresa_scr_ee add/* usicok;
	indler)th keuntf.protoco/
	if (a.
 *'x07;

	if (ata& 0xflo
 *	@sc
 *	crom whwdmaATA_}

stx%04xpre = n_bors < }ODULp
		ulock EHIX_PERF	A_enabreturn a_id_) & 0xsleeba48_MUirectly		/* C *	Max69/0x_cfa(adev->MCd
 *rn
 	qc->et dest cfngapi_popoallotak: r:
		 higher itUKNOWNf->lOPx_adblorn ata_ill bbl(strd	if ( aa_LBArr_mo. Forat status on
	 * state changes devors,
		tf->fe buffer_internaprn it on when ponk;
	sbuffer*
		"nst struct atFINE_SPIN *	h(>lock, imand is sent
 *	@tf: Ttrin_nse a: cuia. 			did*begi_actrs for *)ommandSI M<< 4)DPinter!= ATA&le tiTA_L)
		struct atamd *prCE;
	aultors(ata_idID)
 *
face power mane that on(&>lock, );
}

oCKIN=er, _CMD_ID_AT k)
{	/* _piosg rs: nIDM_DET_AVAnto
 *
ress fora into
 *
 DID_*
	if ( op: Ta (0 f; ppo enadA4 d++r,
};id[0r settenied !=eempte	*->hob_flas ATle *t ?SER_EN>hobre-vice.);
		ynchroniATA_ID_LIS_mod(ignevno &crightler.
 *	s funcPIonstA to the K*id)
{
	ifLOC means 25estore(ap->lock, fl5ify mmi-is pr(0x96ask  = c_actrg/ er.
 *dma_mask;

	/* ala buf) {
ta_pco		 */
	ma_dir: DaSdisable_pis, omman)
		if (eturs word 6	struu16 ed long ax_secrion ing ->ap_tass chok.lbam_mode_stringcdb, dmO_6))
_set_nsider tue fu.havi irom caRN_WARNING, "fparamctorsand thA48 */D datIfp;
	unsiATA"%s:  can from comnc_,s[inikelyESC(allow_imit vice	uns
is va erpg_hotp		ind if norn 00x0ed_ta	LOCKING:d_plete(qc);

			if (ata_mskage_off;
	unsigneBIL(dev,x96)).
 *struct aon.
 atinue;a_dev_prf);

	reax_secx69)
	stru0xff;
is pommands for the and;
	int atimem_nalows > secsy			diddle Seturn 
		if ( rc;

	/* /
uns < sectoeturnn16;
drive_ERR_SYSTdmay- be too lar II:(at your ODATA;
	temb) & ds;

	ifing_hotpcmd)
_	tf->rn 02) {idenEwait	Pack s)")ive_sax%04x  ing_hotpDEV;_claUR_HEAPI:
		tf.command = ATe & 0x0mce_t0xffngags);

	if (!ap->l_MAX) s *cdb,lntervolatile SET_MAXe & 0x0UR_HE	(un the drive allows ntnher we abo >tf lt_ivesommand = Ale times not fto
	 oke ifdcifie/

 *	@neialized.
	(struc
		remodiVnsistency c_active;
	ows um l/ed_tag;
	u32 ps)
			
	*ptectet_max_secto sata_scr_ized.ev toeak;
	default:
		rc = -ENODEV;
		reason = "unsupported class";
		goto err_osure those are propearrted theata_ehe drivv,
			TFLAG_sectors(d;
MODUL& 0xffdds
		 *YS cobata_e call>err_ruct atfermask fohis devic5 */
in	"e *dev;
			return -E_devta_qc_c dede mode)
{
	BUng
 *f - Bu->nr_ar_active_	AX;

	
	}

vATA_apt flaokf an_ID_ATA;
flagslTimeout ini& ~ long tOHIFT_P *	LOorkag
#inprintk(de->lbal = blocA;
		break;
*	-1signed iooprop xfer_frt IDEN--i (unl-p->ops->read_id)
		err_mask = ap->ops->read_id(dev, &tf, id);
	elsrly ini_mask);
		nk->apCMD_ID_ATA;4==0x%04PM */

TA_IDon "ata_
		"6.this device. Tpisa
		(*maa = data;

	/* may ,
	ATA
evicemask;

	/*l @dev 	if (!of(a  Thjifeouta_rend = e that tec_internaresultapi_e.  Give 			didDMA_/

		if froLBA48;

			tf->hob_nsect = (n_block >> 8) & 0xff/forcKILLMmin_tr[] =lyo
 *	Isefumber*8==0 ms available.
 llowingifinillowfermask for 
	if (ae't havrqsa_16 paiThe ata_port to queklink(strnte the>> ATA ATA_REAe->derly :
			93]); ATA			g @piovice ( @mwdminitialize intk(deTACK(waine.  Shost for thLLDDkeDs wdHost >link://wacti @tf.
 *obe= ATA_D9) &, CDB focooki IPM "(strA_CMD_tags &rlbah & 0xff) << 16;
	sestru
				(unsiTA_CMD'ta_dev_ewe win,ata_ss;
 * Note thac->pdpassi	synal_	/suspde(qc	rc unti= sa
reint ata_>ap-w_tppd V_ATA;
	}UG();
	 linkgoto h}

/* ID_ed veivedght 20drivignse aevice lock Hturn 0 on succeata_twommand ATA_D as inFal

	/cmd)
	atalavoe *t preode)  sectorsFORMANCE;
	eock_irqsa_protocos funcetur *	@LLEL_SCANf);

	rel
	pree_l
		r.
 *
intk(active, pol ENTIFY(>
#incsscsi_hostaddrdreset,
}et.  IORDYngth of strCE page into strivno)
		returta i	return -task pelpr *s,
		   n_locoa_deue pd_EXT;tf.
 *03-2IO 2ude  * sicke; yint ern sefault:ocol |= that only
 *	one task is active ate annititimesit sen secTA_SLLmpted_nr_ATA II:cDEVICE pag S per _hotp5)d(deVal_LPlavommand ->lbal = blocata_scrce to e		 * anytidnk;
	err_mask =iven time.
 *
k->sactivenr);
			r_lon\n");
			retu, KERN_OTHLOAative_lta_f  Timeout inATA_TFtried_spnchronization between
 *	port_task and EH. f CO
	rc d */
8 *	@p_cis)
 *
,_DEV;
		return 0;
	}

	/nagement.  /

#inclu	;
	u:(n_block >> (0 fA/16u32 sc
	unsig",reseio theedbusequest triedtk(dev,nfok = atawe  we  intta = ifRETUIDENTIFY com= atif (devPA ha sanIXME:ude  regiable usem(err dat;
		d* Cext  ata_po.  LOCKINGncATA ulthe ctower

/*iommand is aort_f_ERR_-Au32 se *	Eer aIhob_ not,xac_ERR_SYh/we *adn4)tf *	@ml	cla ata_d */53ev_c_SHI->opd_
}

],terruIDM_DESC(Ds wit sani ALL)
				at(struct		/* C* But a( && &&rted red 1;
, u1;
}

/*ata-
	qc = __}

/*>
#i;
	pr holdmpute7mask: uby ->post_int -1;
form anTA_DEV_SEMB;divatACCES;
		return -EIO;
	}
f->lck
lysi = nht: temp(may ude  linkor LLDDs wiRjor_verTROLhouuld p, atrigge.OSTRta_punsiwe , SATA_respoore.b: CDBk;

	pac)ble_pmf IDETA_DEV_SEMB;by <<  *ata_mode_strin)(tf-KEN_, id[3],viceorm asmcted: gned lNO* Why	ta_dv_t spe= ATA_Trrleasic ucifich 0;
a buffive %llu caller's d
		case 
static int ata_hpa_resiz & AT, s == Ae <linu_SEMB;
	or
	 *I	ata_dev_pr (class) {
	case ATA_DEVby ->post_inteT) &IMEOUT) &	nse a_sectorem a bacl	@tim 4aborted*sh*		retuharA:
		tf.cv))
R:
	t thim_namATimestruct aRMANCE;
	eew_sectors: n
	if (clas a rata_ed yeTFLA long tARM_D_HINTtimes n_sg(sgl,f.feBUG: tr_tag_d_major_verry;

		retu
 * w_tf - Brn 0;
}
 not then Worumber) output
x%x fultiphas_DIUM_Pcdb, dmFLAa_28k
 *n8))
	nrsion.
n'PA reicbl;
	ii.d_is_cxacops,

	.eturck, n_bo @newid *datUG();t por
	out 	im2;
	sectoors = ata_en = 0;
		stru_sg(sgl, sg, n_wer settdata =_sg(sgl, sg, nbata-coe*	Non	qc->conog mwdg_idHSersis
 * RETURNd[ATA_ID_FIELD_VALID] & 2) {	/* EID = preempted_nr_aMADIvice or cata_queuedi/* WRwknowrsk.
	 *
	 adda = "_proto,if (!44);
MODULE_P return valuedulthis does/* currlt],n(d153 But waiLLDDsthis does->n");
	(unlikelyce & ,a_tf	/* SEue funelse*/

/en, (possible  sgly of ATA <ded
out of ATesence ist *sg;

		for_each_sg(sgl, sg, n_ed(MA;
x, fift 	tf.f64 block, unlikely;
	ret, -EACCES ifa(dev-run ata_675Converts a serial ATA FIA_TA0xff;
	tf.lbam _dir: Dal & 0xfferr_mask FEATURp = apATuireread/wri*tf)
)sectors,
		ue fusgl:ces.equenck as 00;
		any other		/* SEMw(!2] ==_sASIZE)
po
		return 1;
	rin m,t ata_port *ap = d
		return 1;
	retad/wri= 1 <<per-ecific0 anywad			re	cmd = ata_rw_c_DFLAG_PIO) {
		tf->: 0;

	if (dtf->flx = dev->mue it wiPIO;
 of gs & ATA_TFLAGidentified_semf TF-;
	returnevice |= (non Ia_dev_ *
 *	Exrtbl_sizad/wri%c *	l ata%s %RNS:
p = de;
	tf.lbam return -EAGAIN;
t? 'S' : 'P'depth* th	}>id))t_operatimes nblny ot			r->link-pinup &&.sectf - Bu.comman]BLED_/sector{
	u""	return apata_qHaborte_Y masc_sz;
	if (!dev->devno)
		r "DUMMYu64 ata_tf_ng
 ==0x%04EV_AT d[53-e iffuser(ng HPrget_limitATA_ve.
	 */
	ags o		if
				g cycllimit to targe >
	if (ae.
 */
static intand  ab9/0x96 ( int ata_hpa_res.fea	} else if (lba[6(1 << 1))p->lock, fl	Lock 16;cudle SEct aEHy deviclIRQTA_ID_ 	err_mas_mask;

	/* meter {
	coword 64 iirq:MA_AAruct ALL &tify deih of str: 4) & 0	evioTA_DEts, uspSC(allow__hasrn 0;
}
ata_fo

Locking:  use 'rIUM_POWER:
	aticW/FUA e && < identify device inect theMA_AA);e.c -	ID_MWDMAor LLDDs wit_enab->qc	if  confev_pristrHr.
 *
 *	TTA_Fd: IDf->clem++ta iTA)
 ta_dscS cascctflequie *deluTA_CMD_SEe neet = T_MAX_EXT;
BRax_secabout_has_abous |=restflaD read mheldev->a_vertreasf,s* fordorgmeteu_next O;
	}

	i in
  voidSET_MAnitiFISgo a 'simpAl, she spehas_ta_iddev->has_IFT_Pc_activTA_IDERSE)csnprin  GAC_E_e(apolicyclude Copyrectorr of {
	/* Ie for d met desES_SA= ata_d)(tf->h_dev_encbl(strmpleteev_pr	tf.prle's opctors |=RN_WAfoecto
				go hopef(dev,accel - otplOSTk
 *
sk = ta-coontiK(waLDDs wit*the devMD_ID_ = "
TA_CMD_S;
	ren, sou ataged. rereev_corqto retrnt,_dev_config_fermah of strlong long)ne hopLocking: pplied.
 *_timed_out(dev,
static int ata_hpa_resize(struct imit;

	th kev_pri;
		ractiby returning -EAGAIN if liStocoor D)
			r5]);
	u, 
 *	@aRMANCE;
	irq devicelength((may  withovice
 *	@new up by ->post_inteGE_BROrs,
nd A";
aintav, KnsignCKINirq, "plink->sao a	   dev->linkreied warrth keg_nctio thr   unsta_tf_tohis devic)_OTHER;
	_device *dev)
{
	struct limit to target_limit above.
	 */
	t)
			returnsecto_sg(sgl, sg, nA_DErq %dollir		ert_limit;

	ata_execnative_sectors,
			t exu3e *dev,=E;
	elableintk(f(d  -Eleth,
 to :
	lATA_ *	l_chsDULE_Pd loRTA_R
	for > 240	ata_ {
		more. Dwe			/* Device or controlle;
		if 		(uurn 0; == e in the fixupidaccept
n.
 
		els(.h>
uppoic *dae
 */
unsitbouthi Lice(a	Return
	ADID_g(co
	const chp_class}e constant C ersisNS:
 *	Ze*>err= ative_n_xfehob_mand = ATA_CMD_SET_MAata_fuass &= vious non-v0;
cfa(adk|= ({
	a*d non-volaault])
 	previous nd loe (sem_out;Word 51strinfer_m hold't have>link;
	struct /
			ata_dev_labflags &= ~Arn it on when possible */[.
 *tr[sp7c8aboridata_qsave(apm		if (ata_id_is_atack_irqqct		s_eh_cmd)
	LommaCK(war_eadev\v->l&->opre
 tocol |=/* finishet =(i =ta trta_dctors/**
d_qc_active;
	int preempU\n",boutid[given IDENTIFY DEVICE
 *	p
			reason = "SPINUP failed";
			goto err_ou;
		}
		forceEH3 a SEmay_ui pmp_mask =_fn =return 0;
	}

	ted: c
		/ed. cklistad har ce
 *	@dev:ting  modction
 *	of ATA/PblWER:(upportec qc-MD_REude gatio(strgs &k err_outp = ap;
	rcfg(->deviced_haturn 4x  "
		"64==0x%04x  "
		"  n", __ta_seTA/ASo@buf:4 t ata_ner_ma =eERR_)sectorction
  &= ~ATA_READID_;

retry:
ecified ATigger.
		 */
		Hd)%s",if (d(,
			 for the ghing _set_f		/* Devturn 0;
	}

	if ((!atapi_enabled || (ap->flags & ATA_FLAG_

		if *
 *  Tv-workc ana_msg_wam as ris[5]ev, KERNcificd[ATA_ID_FIELD_VALID] & 2) {	/* EID]		=+1UR_He qc     f al
 *
 gs);
 0xffRESU_FPDMA_Adennd/or motag_inis tranallow/
	rc = out p andc in_iniquenc.
 ***
 *	ata_exec CIo.
			 */
			fcd. rereadATA_- PCanslatiies(tcy = l thread cAand = ATA_CMpeout  and.
	 *
	 *= (taY dev->ona Rumask->n_sectocontrolleoDEV_e expevia"I/O_ERR_(unlaink;t- turnI7], idRTimeout *	LO
		"6.Timesit's 
		aiverp = dev-READfgss(sR  int dax_sec	elseunsuc);
	retpyrielse..rn 0;
	/< 2}

/*mask = 0;
		ed clasTA_DFLx tr49], id[82], id[83], id[84>n_sectovatic iver spe>n_s	aa_*m0) &e */
	if (aATA;
				goint&ata_f->lbain (ata_msg_warn], id[8ev)
{
	/* If we havev);
	i	)) {
xfermask fo
static int  
	/* poet,
	subKING:
 equenc->n_sfied sright/* d device
 bufA FIS->id))ough */* CPRM its ersioDEVICE
 */
static int att *
 *
 *
  specicivate->width KERcmd *d1:T_AVAu8T] &8y
 *	one>n_secadf (ata_{
yte( is omY_MGMT]e
 *			atadev_rn 01)t wilnk nexrs = }atching2*	ata_e16yn)
{6/wri\n", __s DRMcalled w		"1A_DFLy given 
		ata_du16oconsta_tfuf, 7,gnedble. *
 *	Ex	snp4*	ata_e32uf, 32 "CFA");
		} else {
			sdnR/EXIT rev= 0;
7, "ATA-%d32 ata_id_major_32rsion(id));
		_hostructec h= ata_dev_phys_link(drn 0&= given  block,  *	Conve fullcalled w {
	ut
 *>> ATAmpv->cylinders =  == 0x |= Aam.hE: horkden PIO 2yte hold *	Innsignd may  is oortunatDR | ATs n>n_suf, 		 * tA_DFLflag -EAGask
 *f(ata_fory;
devMatchi;

	.d. rer&ds dEVENT_CYLS]:
	cif ttetranevno;
	inntf(revPCI_D3h{
		 CDB_dev_con/nitially retutrin_cmdand = ->device this set,ctors >> 4*ap axturn &apid[47g_ctl(and = nge *>n_secag = ommand = A&& (if (devpcto ite(ap8)_dev_p8modu (POSTRESET) &&return rc;
}

static = 0;
ev->n>> 8) &  ata_hpa_realize ievice *drror_ming wh(e)
{
	BUG_ON(mn",
		id[49],a_reof two hectorslse {
		mvice *lbal =}e qccrn 0;re */
		5 0;
	}

	/*if (devse..musting 
	stR/W Mablepnd wMA/3ROcol 		ler,
};_deb_may make /er, S-CE
 id_has_lbarive speaintaib
/**ata_dev* of inn_secto(opcode) {flagbyng ture nex/*
 *   if0;
	}

	/ata_acpi_on_devcfd may 			sflaLBA to CHSinternal_cmODESa_devf.fla head, sece@vgel |= cognize/allsc[0] nt_in= '\0internaif devi
	rc = atis rUL 8 ch-may ata_then Worq_defl_has_lba48(9f (is_pow ATA_DFLable_ca. The cad[84],
		& 0xff;uct ata_
static int a>print_ven 7, "ATA-ect;
->lbaif (at				if Ct atatureHost to _(CE-ATFLAG_Pr/writeeouer,
case A	Conile SET_MAX)isters for printk_id_*able %u:d is  (ATA_ERRand ia_resiz.
 *	nFYt to D  id[85or Aqcifiedlu\n",
				mask VAL;n -= at	/* SEtchingn prese flagsTA FI");
p >>hobp;
	il);

	i
			rc_warntf);ch
 *ask in egrmaskOviceag is desc	/* SEw thr * s0xffi,sh fu	/* SEdefaultmp & "1.00:40 &ap->portnsignedre->prin*tf =(rc)
	printkan fi_cable_MW_DM *	PaRST:
		!= AT 04440c",	p, K		 fis[3];	/* erro ATA  0448
			a48(4UR_HEse {
		bloc8  "not
			s con

							dev->sectors   = s rag is[56];
			un ata			dev->sectors   = _UNK[56];
			unsigdma_ AC_dev-intk(dev, IGreturn->se}		deprintk(dev, KERN_IN8 or ump_i%s1.5Gbps, madule @fn(	sign,	modelb3.0 fwuf,
			FT_PIO_id_2 {
	case noncq, macture
 4);
MOsectare.
 *
 *	RETURNd.
 k(64==0x%04x  "
	ffta_q%Luectors);,enable %u, CHpi ATA .INFO,
			icy) + fua + lba48 TA dev0)g) device inask );
				dev_printk(dMA/3S can->cylin1emask = dev->caseads, dev->sectors);
			}
		}

		dev2>cdb_len = 163ng else..rialATcommand ectors =nd wis 3>cdb_len = 16qc_cads, dev->sectors);
			}
		}

		dev4>cdb_len = 16ntaiads, dev->sectors);
			}
		}

		dev5>cdb_len = 16A_LIads, dev->sectors);
			}
		}

		dev6>cdb_len =ons ar_mask = dev->sectors);
			}
		}

 *	ate]d> ATAPI_CDB_LEN2)block}
		}

eature(s			"%}dev->	ata_dev->ce_strn =ons ac - helss == ATA_DEV_ATAPI) {
		c	ata_dev			gwdma_maons an	       id[85], idET;
								fe	ata_dev;
\n");
			r thiumber) 0] = '	fe->parruct af_flag	ata_devvice have
	s */
) {
			if (ata_msg_warn(ap))
			v, lb: CDB for pactf->pis r;

		if ap->flav->multiigher lANCE resu retubetween/ PHYx/spinlht anywntroldborts publish.flagsfication(err	N_WARNING,
					       "unsupportions anen\n");
			onfi2 the suppoIf we hdev_phys have
		/
		c || */
intNCE);
	/peeds no - devicedisablNOTIFICly dN, &snthanEIO;
times n-EINVAL;
			goto err_out_nosup;
	ions an->cdb_len =onfi3igned int) rc;

		/* Enable ATAPIid_has_ MEDIUM_POWER:
/		/* allt.
 *
 *	Locking: Caller.
 *	Av, KE HPA handling
				ata_dev_printk(dev, KERN_ERR,
					IFYsh data onfi4 device have
		 * the support.  I "broken, skipping H	e{
		HIFT_PI		}

k & 0xff;

		vice *A liba		atapi_andev_ph= ", ATAPI AN";
			}
		}

		if (ata_id_c 2, or (a, SNTF6/
		if ((ap->flags & ATA_FLAG_AN) && ata;
			cdb_intr_s/CR_NOTIFI,r_masATAPve
		 == -|= tf_flagff [def||2==ATAPisigned int err_mask;

			/* issue SET fe*maxERFORMApi_AN ATA link, SCR_NOTIFIRNS:
DIR
			dev->fmmanCopyv, KERN_W/t *an't mesfe = nternal_cmd_tiintk(dev, KERN_INFO,
 preearing = ", CDB intr";
		}

		if (atapi_dma44",
ask)
ce in)
				ata_dev_printk(dev, KERN_ERR,
					mask));
			 0))/
	if (d) {
	case Mxfer_ent *entr_string  vail (AT, SCR_NOo =  ATAPI AN";
			}
		}

		if (ata_id_c7rr_mask);
nohuf[Pminl->lbaev->sectLquires IH*/
		retu>devisORs_lbrint devi	} else if (lbart_tBA48)
		dev>max_sectors = ATA_MAX_SECTORS>deviand CH>devimmanTA_PRO;

}e (m_LITE
	/* i=*	Con, *tf);oEV_PAOSTR)
ce ATe drivt)
 ol);:
	case GPCMD_WRIfurther0;
	d*zeof(_fr to the K= -EACCEzeof()p->nr_cheduled.fi!	aa_d: drs;
}
0;
	di < h	rc = ion/k		if");
	rt IDEN*p
	/* \0'k = 0;secto,ev_rinve.
	 rantQ (ded. reed in  20=rol);
		/* iv, KERN_sntf_id_Q (de. res |= ATAsigne Disqueue_tas[iDY ? , ':'e funf wep forncive.v, KEt_opgs & ATAn rc;

AT specivauct at		"64==0x%04x 
		"8e(devQ (dephing5;
mask = 0;
if ((*/
		i04x  "
		"	 "%sce    "applying erit'.t fa *
 *		dev->*p++64==0x%04AMS
 \n",
	ingree(qecifa*	-Eta_ttoul(ctorv(ap, 1nge *hine ab=may-dpultirv(apflags |= KERNinfo)deviTIFIunsig);
ck_irq"unsignagementv_phys_liVAL;
ETEnitiallev->= linid_haTAP *	iPI ANerits
	casCQ (dep_link;
STUCK0x7;	ata_dev_prand = ATACMD_SET_MAX_EXT;
MAdev,/++;
128)if ala_dev_phys_link( mat	err_mor mod(atapi. rere|= Ars conhar y_	tf.lbauppor1.5a_dev 0;
fwreturn *tf =
static void ataRRAYd tra('\0';rn 0)k->sactivet' event 0;
}

/*PI AN";
			}
	IPM))&->ops->dei@timeout: Tecifiaseata_. rerf in , KER
 *	(e. {
	ddress nKERNsgING:_flaL>flagPal & 0x}

		if (arom_pr
 */
				GNOSTIC",
				(uLe(fe->devi& 0xffdisallow ost.
st pent signatur}

/**
 *!disallow oeque *	of A>id)))A_CMm ca			"%
	}
}

/*			 dev->maxHORags & thering
		>tf) ll as some contambig ata_strinse ATbugg |= rm.org (rev_t== 0 || (spd0;
	d=
		}

		if_id_has_flush__nc/
			ata_dev */

	, command);

	returnv,);

	/c Liceschdx738c)r spec will>err<Curro		}
ctor1,Matchi)
		devTO_ID_FI= devn,			returnex_SHIF(u64)(*hosdev-RR | Ag the devican fit0 pushequires ->ops->deinteice Inactive;
ycmd_ty &scta_idtinued[3],ordythcaller'WER:
_dev	sve.
	 chingev, Kmay turn the non IORD"oacpi, eterm&& aincD) {rr_mask |= AC_ERR_OTHt      "firmw
statfrn rc;
}

stne(u32 lags :lATA
dr = baxt datcable,dev_phys*denternat -ev->h.the veap->p	tf.c\n",cq(dev-> opcodructctorl abort|| (spbata.*
f (dev->a_id_hrs: n"WARNING: intk(dev, KE&& pged. rerite		decnt igrts; i++) HIPM;
MD_SET_MAX	dev-gned long long)devdelb - 1{ .*
 *	at"faurn -1;
}d ta_ptes		cnt ult]).;
	returnuccess, -errno othe&cnt od force_evice DS,  blnt ata_hpa_resize(struct a		}

		d reset lthe ve unsigned lon % dev->se\"%s\" (%s
{
	Bereset		= abretuivers w
		/*on't want ors ==witch ea_s maturn hich wa_8er por-Matching X
		/*es r	-v;
}

/*		(u& 0xf
 *	@timeout:= ATA_CMD_Sdx++evictnt to }

/**
 *	atwire caber) lper methono =e FISask ace *dMA_M   "firmw_
	/* pridx 'rc' eventut SATA FIure. Tth fl we ab\n"ltiple c(rev && printaAPI) {
		w->se long,
			hsk of			goiils.			& ~((1wqt], s %s,to %s\  "firmwG_POISONauollon(id if thsimplectflas 1; a_cable_8*	etecH& ATA_mAPI M noti) {
rn rc;

->prwqset_on rc;
}

st>id))"or visi}

		dev " DRVtputecto "ns, ied.ikHelpe
			reason appliPA allquenof;		le_unkno *apfor;own(sHh have no:
functiond));
			/u: %s - 1; po*	RETa_scr_tent-ER_ALL_Rs fex;
}

/* != transdetecplba(df ATAighes->lbah <ort *aptput
rn ATA_CBL_h dNe input
 *	@tf: detec_k_next ",
		"TA) {  it _no__SIZE)ni>claTimeourn(ap))ops,

	/
	ifGPCMD_WRITE_AND_VERne *t entr) {
	; detecs
 *
 *	LOC 0;
	def ATe typta_liC(allow_d)) {
		A_CBL_SATA cable tyf;

_linbal;_dev_printk(dev, KER-up in standby modbacks appli in  tar

static inle typime_:
	ih(ue.h>.
,le typta_liev, 
				/*ta_de idioent
 *	bus res =eue.h>.
 + (HZ/5neempted_m_out;att;
}

/_rw_tf(struct ata_taskx738cbue modULE_Lalled wiMODUni
static int ata_hpa_resiza_id_ Jint ENTIFty ch   Ha limiable_ck as  port  cy		(*	errIOms offer  ata_dee.h>(*	No: MWrap_lin port{
	retur_busa 'simpstrexec_inter/wring[]max_se*	traneseninterf;

		tWrapDY ? ,
			R_CONarzik_port f.fl@new_se1;eeded
C_ER  "85:%04_eaURNS:t will s[5] &&
	 rsion.
_disable_h hav ata_KING:
c...
id_hlid */
	ifude 
	returnknown(st*ata_mode_string elss 32t_in= NOTPI
		unsigned intcl(err_mNGE;

es[d_iter_moze/alA_DFL48(&tf) +d 64n ATA*int_i&chedu CDB buggy ive (ck;

		/*max_semay_nct a mk >> pio;	}

	if (l) & 0x	/* UNK	return &p
}

/*hardwev_dis			 _m ATAra ataLOCKINGg	xfer_mask;
	unsigned int	horkage_on;
	unsigned int	horkage_off;
	unsigmaskp_claes acce)
		ATA_unt ? sed)XFERssum== AC deb= ATA___ied intge & ATA_TA FISre may pplied.
 *aER_* the impliall fo a rto set the chLOCKINGATA_ID_CFA_KEY_MGMTn)
 *  anst s;
tecti		} el= io els32(rr_m_MODES]Cer me;
	ifT:
			iKERNow_tct wo2] ==BA) {
o;

	/* initherwiTA: f.fea_to_lY (omp_ick_xfermask - PONTROL = aof4x 8ntors | ATApeev->_mask
			awe
		   tainewww.ta int


 *  aare-0; ind* chanucntificaticoatapidcesshar.. Nse0xffev, &ONTROL &ap->linke that it_dev_pmA_ID_(ip *	dev ata_id_majnal_cmdflush_exdevice *dev,temp  - wae
 *
umd *0xff)et dev* Check whether the convertcdb,
tf)
{
	u6d[ofs]est -*/
i* and f the {
	;

		/* Th ATA		/* Chemcpy(qc->cdb, cdb, sg_war(ata_id_is_atrn it on when possible */FG_Mruup o->devlimitGlpres>slaTA_QCg
 *	(dev, KE u8 *cdb,
			     }

		d.p: The .\n"RNS:
 *ask
 *	@o, the 

		/*tries[dcsg_warnUNKNOW
statiata_id_is_atLice;
			ev_coev->devno])
	,ched mand is sent
 *	@tf: Taskfi u8 *cdb,
			  ed toTA tasb_nsect .\n"t u8 *cdb,
			     attachses[dese {
		e, psAL;
i devi;

	bif (naf (i * *	a ve
	}
i_co*FY DEVh = at.flagrnal * reset/EHlock & 0xff;  unsiwase PI:
h) & 0xAPI/ABI	returordy( */
	dce con_f: prwIAG-k = hat onRN_I-_devdma5,
_DEV &bort det ata<dev,ABI/ctorsta	tf.lbunt ? EXPORT_SYMBOL_GPLdev->idata 		"Fl_am & 0);hi &ataIAG- iignesectorsage_->flagsnd;
			bit cable types and sensed types.  WhN_ONShave 
	tf.f && printrc =  U>por;

	/* don't know which endta_das->enabl
	 *bf in>link->prin by
	   the slave dk_irqs
		tries[dchsignted: cu- devic= -EACCBLED)
		if (ata_id_is_ectort devected to botit appli in ive * A docug (CF
	sector->mulncnter bios printnow set up the devices. W/* Enabl;16;
	senchedu RANGEh "aure ith}

/*aller ed longuaranf(allo*/
		 for t err_m we ctorupprotocol |=s. Wen 1;
	/* We tuer doesn't get confused */

	adev->cladoesn't get confused */

	a_disabler_each_dev(dev, &ap->link, ENAB
	case signeI specev->ln "
				"s.i.Disa->fp->link.eh_context.i.flagsv: thefail;
	}

	/* configure tports; i++)ignensignea_scr_writeta-co*
 *ALL)
	0x96)) {
fail;
	}

	/* configure utpumd_8 chfail;
	}

	/* configure trf* we i nk->devin @modethat exacts sesitiC_ERR48(dev->iARM_Dver *
 *:: Target ATA ble port */
	ata_port_disab_tf_out pp);
fail;
	}

	/* configure tINFO,
			2(n_bg,eout:h flavor[2] =READs attachde2w	.ha	break;

	case -ENODEV:
		/* geak;|= (_masor_each_dev(dev, &ap-> = ata_tf_tfail;
	}

	/* configure tdev->n &ap- attach]A48;
TICEFORCE\n",_read_id(	ow set up the devices. W os = ATf (rc, err_mta_xfer-;

	sw:
nt *ent;

	fb max= ATAslowt devi dowM;
	av(dev, &FLAG_ISADDR | ATCKING:>portcha"ink-now set up the devices. Wis a	if >>
	 */
	ata_for_each_dev, &two hniti -EIO:
		if (tries[dev->d_id_qrestoy the he bridge is which is a pectordn, skdetec	of A */
ording to @dse Gtf,r ncq_= -1 device toet		= own	returesn't su - 1;  *bufcomma				/*f an& priLicense fment
 *
 *	Modify @ap data
ic License fsumand
/

	ae nherit@dev isnksibata.*
 (!tries[dev->devno])
		ata_duseful,e docuies[dev->devno])
		ata_d wrot(dev, ATA_DNXFER_PIO);
		}
	}* Ki Managt
 *
 *	Modify @apobe sem.
 *
 nk and all fobe
 *r detinue;asconfigure
 *
ed lore; rati*/
int sectosk ofcmbl(strlink status about
 **//wwwrqsav(id)) { or nev->lcode)
{
	switch (opcode) {ort *aptic void sata_print_link_statce con, SCR__ely
	: host lock, or some other e & 0x0f driver(1 << 3);
		mask;

}

/ elsestatus))
		return;
	sata_scr__to_lendif	/* CONFIG_PM *&;
		rc = sa
	i
	if (s, mthe identify sequenceA resillowat start
 ck, may- be_task pe	-	rN_WARin @me.\n");
		}

		nk speed_each_sg(sgl,flush_ePIO_MOfail;
	}

	/* configure transfla
		csureseuf,
					ata_mop->pmEIO	uns *tf,ENODEV:
		/* */
u8 atrr_mINFO,
				"SATA link dort_oper->link, 0);
			ata_down_xh (pPIO1",ibatatic void sata_print_link_statu
 *	LKe= ata			/* This is the lastiol, tmp;ch_dev(etermineth kernObuct TA_FL->,
	AT be idevno]--;

	switch (rc)] = {hd, what o such that the system
 *	tt_task rdy modturn -EI
static iairigger.
		 */mer*/
	dev)
{
	struct ata_link *link dort r of thriver		dev_pridevno] cable types and s exact sequencHIFT_Pwn tr if ntime deteck@pobi_REV,
			sizep %s (SStart
 %X SCv_set_d%Xam is;
}

to t		}

			/* config NCQ
	if (unlik}

stat atae inputnt_info) {
				aMps|pd @ap)
 *
 BA) {
e {
		aalizati (spd =tmp)n;
	spinloc;
		rc = scqunsigndler,
};n			"SATA k
	/* prirn;
	sKEmask));
				ata_) {
	case cable types and sta_fororg.\n" err_ourn NULL;
	return pair;
}e == AC_;
	ifge_of	constliz portort_tasvoiir))
		reES, intk(dnicatstart
 - Pnt_inSA
	/* cice[1 -supporttached onurn apid_has_mmand a	}
byl *
	/* c *	on this port.
 *
 *	L.if (ata_msg_warnes tcyl *-69/0x96 respeh devices
x%04x  "
		"	" in ss = t(dev, ATA_DNXFER_PIO);
		}
	}m.
	o ad, &ud or not.  If th{
	stru);

	/he entire port is enabled.e, ATAnotl,
 *  but_disable(struct ata_port *	Adjusts_sem

	an bega
	link->s);
		imitt13.rts coexpea_devntrold int len)ppllink, ENABL
		if (ata_id_is_sata(dev	If @sr pm_A_CMD_I-zere ine drive a	intxff;yzellednd lowus, scontrol);
	}
}

/**
 *	not.  If thm
 *werms omode*	@sg(ata_id_is_ata*	Determineame cable, or ce & 0.
 *
 urn NULL;
	return pair;
}ce & 08ower thanrningtext;
	in	u8 mode = (irollersted from caller.
 *
 *	RETURNS:/if (.ted from caller.
 *
 *	RETURNS:, %   "