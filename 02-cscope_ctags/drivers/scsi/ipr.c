/*
 * ipr.c -- driver for IBM Power Linux RAID adapters
 *
 * Written By: Brian King <brking@us.ibm.com>, IBM Corporation
 *
 * Copyright (C) 2003, 2004 IBM Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
 * Notes:
 *
 * This driver is used to control the following SCSI adapters:
 *
 * IBM iSeries: 5702, 5703, 2780, 5709, 570A, 570B
 *
 * IBM pSeries: PCI-X Dual Channel Ultra 320 SCSI RAID Adapter
 *              PCI-X Dual Channel Ultra 320 SCSI Adapter
 *              PCI-X Dual Channel Ultra 320 SCSI RAID Enablement Card
 *              Embedded SCSI adapter on p615 and p655 systems
 *
 * Supported Hardware Features:
 *	- Ultra 320 SCSI controller
 *	- PCI-X host interface
 *	- Embedded PowerPC RISC Processor and Hardware XOR DMA Engine
 *	- Non-Volatile Write Cache
 *	- Supports attachment of non-RAID disks, tape, and optical devices
 *	- RAID Levels 0, 5, 10
 *	- Hot spare
 *	- Background Parity Checking
 *	- Background Data Scrubbing
 *	- Ability to increase the capacity of an existing RAID 5 disk array
 *		by adding disks
 *
 * Driver Features:
 *	- Tagged command queuing
 *	- Adapter microcode download
 *	- PCI hot plug
 *	- SCSI device hot plug
 *
 */

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/blkdev.h>
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/libata.h>
#include <linux/hdreg.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/processor.h>
#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_cmnd.h>
#include "ipr.h"

/*
 *   Global Data
 */
static LIST_HEAD(ipr_ioa_head);
static unsigned int ipr_log_level = IPR_DEFAULT_LOG_LEVEL;
static unsigned int ipr_max_speed = 1;
static int ipr_testmode = 0;
static unsigned int ipr_fastfail = 0;
static unsigned int ipr_transop_timeout = 0;
static unsigned int ipr_enable_cache = 1;
static unsigned int ipr_debug = 0;
static unsigned int ipr_dual_ioa_raid = 1;
static DEFINE_SPINLOCK(ipr_driver_lock);

/* This table describes the differences between DMA controller chips */
static const struct ipr_chip_cfg_t ipr_chip_cfg[] = {
	{ /* Gemstone, Citrine, Obsidian, and Obsidian-E */
		.mailbox = 0x0042C,
		.cache_line_size = 0x20,
		{
			.set_interrupt_mask_reg = 0x0022C,
			.clr_interrupt_mask_reg = 0x00230,
			.sense_interrupt_mask_reg = 0x0022C,
			.clr_interrupt_reg = 0x00228,
			.sense_interrupt_reg = 0x00224,
			.ioarrin_reg = 0x00404,
			.sense_uproc_interrupt_reg = 0x00214,
			.set_uproc_interrupt_reg = 0x00214,
			.clr_uproc_interrupt_reg = 0x00218
		}
	},
	{ /* Snipe and Scamp */
		.mailbox = 0x0052C,
		.cache_line_size = 0x20,
		{
			.set_interrupt_mask_reg = 0x00288,
			.clr_interrupt_mask_reg = 0x0028C,
			.sense_interrupt_mask_reg = 0x00288,
			.clr_interrupt_reg = 0x00284,
			.sense_interrupt_reg = 0x00280,
			.ioarrin_reg = 0x00504,
			.sense_uproc_interrupt_reg = 0x00290,
			.set_uproc_interrupt_reg = 0x00290,
			.clr_uproc_interrupt_reg = 0x00294
		}
	},
};

static const struct ipr_chip_t ipr_chip[] = {
	{ PCI_VENDOR_ID_MYLEX, PCI_DEVICE_ID_IBM_GEMSTONE, IPR_USE_LSI, &ipr_chip_cfg[0] },
	{ PCI_VENDOR_ID_IBM, PCI_DEVICE_ID_IBM_CITRINE, IPR_USE_LSI, &ipr_chip_cfg[0] },
	{ PCI_VENDOR_ID_ADAPTEC2, PCI_DEVICE_ID_ADAPTEC2_OBSIDIAN, IPR_USE_LSI, &ipr_chip_cfg[0] },
	{ PCI_VENDOR_ID_IBM, PCI_DEVICE_ID_IBM_OBSIDIAN, IPR_USE_LSI, &ipr_chip_cfg[0] },
	{ PCI_VENDOR_ID_IBM, PCI_DEVICE_ID_IBM_OBSIDIAN_E, IPR_USE_MSI, &ipr_chip_cfg[0] },
	{ PCI_VENDOR_ID_IBM, PCI_DEVICE_ID_IBM_SNIPE, IPR_USE_LSI, &ipr_chip_cfg[1] },
	{ PCI_VENDOR_ID_ADAPTEC2, PCI_DEVICE_ID_ADAPTEC2_SCAMP, IPR_USE_LSI, &ipr_chip_cfg[1] }
};

static int ipr_max_bus_speeds [] = {
	IPR_80MBs_SCSI_RATE, IPR_U160_SCSI_RATE, IPR_U320_SCSI_RATE
};

MODULE_AUTHOR("Brian King <brking@us.ibm.com>");
MODULE_DESCRIPTION("IBM Power RAID SCSI Adapter Driver");
module_param_named(max_speed, ipr_max_speed, uint, 0);
MODULE_PARM_DESC(max_speed, "Maximum bus speed (0-2). Default: 1=U160. Speeds: 0=80 MB/s, 1=U160, 2=U320");
module_param_named(log_level, ipr_log_level, uint, 0);
MODULE_PARM_DESC(log_level, "Set to 0 - 4 for increasing verbosity of device driver");
module_param_named(testmode, ipr_testmode, int, 0);
MODULE_PARM_DESC(testmode, "DANGEROUS!!! Allows unsupported configurations");
module_param_named(fastfail, ipr_fastfail, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fastfail, "Reduce timeouts and retries");
module_param_named(transop_timeout, ipr_transop_timeout, int, 0);
MODULE_PARM_DESC(transop_timeout, "Time in seconds to wait for adapter to come operational (default: 300)");
module_param_named(enable_cache, ipr_enable_cache, int, 0);
MODULE_PARM_DESC(enable_cache, "Enable adapter's non-volatile write cache (default: 1)");
module_param_named(debug, ipr_debug, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Enable device driver debugging logging. Set to 1 to enable. (default: 0)");
module_param_named(dual_ioa_raid, ipr_dual_ioa_raid, int, 0);
MODULE_PARM_DESC(dual_ioa_raid, "Enable dual adapter RAID support. Set to 1 to enable. (default: 1)");
MODULE_LICENSE("GPL");
MODULE_VERSION(IPR_DRIVER_VERSION);

/*  A constant array of IOASCs/URCs/Error Messages */
static const
struct ipr_error_table_t ipr_error_table[] = {
	{0x00000000, 1, IPR_DEFAULT_LOG_LEVEL,
	"8155: An unknown error was received"},
	{0x00330000, 0, 0,
	"Soft underlength error"},
	{0x005A0000, 0, 0,
	"Command to be cancelled not found"},
	{0x00808000, 0, 0,
	"Qualified success"},
	{0x01080000, 1, IPR_DEFAULT_LOG_LEVEL,
	"FFFE: Soft device bus error recovered by the IOA"},
	{0x01088100, 0, IPR_DEFAULT_LOG_LEVEL,
	"4101: Soft device bus fabric error"},
	{0x01170600, 0, IPR_DEFAULT_LOG_LEVEL,
	"FFF9: Device sector reassign successful"},
	{0x01170900, 0, IPR_DEFAULT_LOG_LEVEL,
	"FFF7: Media error recovered by device rewrite procedures"},
	{0x01180200, 0, IPR_DEFAULT_LOG_LEVEL,
	"7001: IOA sector reassignment successful"},
	{0x01180500, 0, IPR_DEFAULT_LOG_LEVEL,
	"FFF9: Soft media error. Sector reassignment recommended"},
	{0x01180600, 0, IPR_DEFAULT_LOG_LEVEL,
	"FFF7: Media error recovered by IOA rewrite procedures"},
	{0x01418000, 0, IPR_DEFAULT_LOG_LEVEL,
	"FF3D: Soft PCI bus error recovered by the IOA"},
	{0x01440000, 1, IPR_DEFAULT_LOG_LEVEL,
	"FFF6: Device hardware error recovered by the IOA"},
	{0x01448100, 0, IPR_DEFAULT_LOG_LEVEL,
	"FFF6: Device hardware error recovered by the device"},
	{0x01448200, 1, IPR_DEFAULT_LOG_LEVEL,
	"FF3D: Soft IOA error recovered by the IOA"},
	{0x01448300, 0, IPR_DEFAULT_LOG_LEVEL,
	"FFFA: Undefined device response recovered by the IOA"},
	{0x014A0000, 1, IPR_DEFAULT_LOG_LEVEL,
	"FFF6: Device bus error, message or command phase"},
	{0x014A8000, 0, IPR_DEFAULT_LOG_LEVEL,
	"FFFE: Task Management Function failed"},
	{0x015D0000, 0, IPR_DEFAULT_LOG_LEVEL,
	"FFF6: Failure prediction threshold exceeded"},
	{0x015D9200, 0, IPR_DEFAULT_LOG_LEVEL,
	"8009: Impending cache battery pack failure"},
	{0x02040400, 0, 0,
	"34FF: Disk device format in progress"},
	{0x02048000, 0, IPR_DEFAULT_LOG_LEVEL,
	"9070: IOA requested reset"},
	{0x023F0000, 0, 0,
	"Synchronization required"},
	{0x024E0000, 0, 0,
	"No ready, IOA shutdown"},
	{0x025A0000, 0, 0,
	"Not ready, IOA has been shutdown"},
	{0x02670100, 0, IPR_DEFAULT_LOG_LEVEL,
	"3020: Storage subsystem configuration error"},
	{0x03110B00, 0, 0,
	"FFF5: Medium error, data unreadable, recommend reassign"},
	{0x03110C00, 0, 0,
	"7000: Medium error, data unreadable, do not reassign"},
	{0x03310000, 0, IPR_DEFAULT_LOG_LEVEL,
	"FFF3: Disk media format bad"},
	{0x04050000, 0, IPR_DEFAULT_LOG_LEVEL,
	"3002: Addressed device failed to respond to selection"},
	{0x04080000, 1, IPR_DEFAULT_LOG_LEVEL,
	"3100: Device bus error"},
	{0x04080100, 0, IPR_DEFAULT_LOG_LEVEL,
	"3109: IOA timed out a device command"},
	{0x04088000, 0, 0,
	"3120: SCSI bus is not operational"},
	{0x04088100, 0, IPR_DEFAULT_LOG_LEVEL,
	"4100: Hard device bus fabric error"},
	{0x04118000, 0, IPR_DEFAULT_LOG_LEVEL,
	"9000: IOA reserved area data check"},
	{0x04118100, 0, IPR_DEFAULT_LOG_LEVEL,
	"9001: IOA reserved area invalid data pattern"},
	{0x04118200, 0, IPR_DEFAULT_LOG_LEVEL,
	"9002: IOA reserved area LRC error"},
	{0x04320000, 0, IPR_DEFAULT_LOG_LEVEL,
	"102E: Out of alternate sectors for disk storage"},
	{0x04330000, 1, IPR_DEFAULT_LOG_LEVEL,
	"FFF4: Data transfer underlength error"},
	{0x04338000, 1, IPR_DEFAULT_LOG_LEVEL,
	"FFF4: Data transfer overlength error"},
	{0x043E0100, 0, IPR_DEFAULT_LOG_LEVEL,
	"3400: Logical unit failure"},
	{0x04408500, 0, IPR_DEFAULT_LOG_LEVEL,
	"FFF4: Device microcode is corrupt"},
	{0x04418000, 1, IPR_DEFAULT_LOG_LEVEL,
	"8150: PCI bus error"},
	{0x04430000, 1, 0,
	"Unsupported device bus message received"},
	{0x04440000, 1, IPR_DEFAULT_LOG_LEVEL,
	"FFF4: Disk device problem"},
	{0x04448200, 1, IPR_DEFAULT_LOG_LEVEL,
	"8150: Permanent IOA failure"},
	{0x04448300, 0, IPR_DEFAULT_LOG_LEVEL,
	"3010: Disk device returned wrong response to IOA"},
	{0x04448400, 0, IPR_DEFAULT_LOG_LEVEL,
	"8151: IOA microcode error"},
	{0x04448500, 0, 0,
	"Device bus status error"},
	{0x04448600, 0, IPR_DEFAULT_LOG_LEVEL,
	"8157: IOA error requiring IOA reset to recover"},
	{0x04448700, 0, 0,
	"ATA device status error"},
	{0x04490000, 0, 0,
	"Message reject received from the device"},
	{0x04449200, 0, IPR_DEFAULT_LOG_LEVEL,
	"8008: A permanent cache battery pack failure occurred"},
	{0x0444A000, 0, IPR_DEFAULT_LOG_LEVEL,
	"9090: Disk unit has been modified after the last known status"},
	{0x0444A200, 0, IPR_DEFAULT_LOG_LEVEL,
	"9081: IOA detected device error"},
	{0x0444A300, 0, IPR_DEFAULT_LOG_LEVEL,
	"9082: IOA detected device error"},
	{0x044A0000, 1, IPR_DEFAULT_LOG_LEVEL,
	"3110: Device bus error, message or command phase"},
	{0x044A8000, 1, IPR_DEFAULT_LOG_LEVEL,
	"3110: SAS Command / Task Management Function failed"},
	{0x04670400, 0, IPR_DEFAULT_LOG_LEVEL,
	"9091: Incorrect hardware configuration change has been detected"},
	{0x04678000, 0, IPR_DEFAULT_LOG_LEVEL,
	"9073: Invalid multi-adapter configuration"},
	{0x04678100, 0, IPR_DEFAULT_LOG_LEVEL,
	"4010: Incorrect connection between cascaded expanders"},
	{0x04678200, 0, IPR_DEFAULT_LOG_LEVEL,
	"4020: Connections exceed IOA design limits"},
	{0x04678300, 0, IPR_DEFAULT_LOG_LEVEL,
	"4030: Incorrect multipath connection"},
	{0x04679000, 0, IPR_DEFAULT_LOG_LEVEL,
	"4110: Unsupported enclosure function"},
	{0x046E0000, 0, IPR_DEFAULT_LOG_LEVEL,
	"FFF4: Command to logical unit failed"},
	{0x05240000, 1, 0,
	"Illegal request, invalid request type or request packet"},
	{0x05250000, 0, 0,
	"Illegal request, invalid resource handle"},
	{0x05258000, 0, 0,
	"Illegal request, commands not allowed to this device"},
	{0x05258100, 0, 0,
	"Illegal request, command not allowed to a secondary adapter"},
	{0x05260000, 0, 0,
	"Illegal request, invalid field in parameter list"},
	{0x05260100, 0, 0,
	"Illegal request, parameter not supported"},
	{0x05260200, 0, 0,
	"Illegal request, parameter value invalid"},
	{0x052C0000, 0, 0,
	"Illegal request, command sequence error"},
	{0x052C8000, 1, 0,
	"Illegal request, dual adapter support not enabled"},
	{0x06040500, 0, IPR_DEFAULT_LOG_LEVEL,
	"9031: Array protection temporarily suspended, protection resuming"},
	{0x06040600, 0, IPR_DEFAULT_LOG_LEVEL,
	"9040: Array protection temporarily suspended, protection resuming"},
	{0x06288000, 0, IPR_DEFAULT_LOG_LEVEL,
	"3140: Device bus not ready to ready transition"},
	{0x06290000, 0, IPR_DEFAULT_LOG_LEVEL,
	"FFFB: SCSI bus was reset"},
	{0x06290500, 0, 0,
	"FFFE: SCSI bus transition to single ended"},
	{0x06290600, 0, 0,
	"FFFE: SCSI bus transition to LVD"},
	{0x06298000, 0, IPR_DEFAULT_LOG_LEVEL,
	"FFFB: SCSI bus was reset by another initiator"},
	{0x063F0300, 0, IPR_DEFAULT_LOG_LEVEL,
	"3029: A device replacement has occurred"},
	{0x064C8000, 0, IPR_DEFAULT_LOG_LEVEL,
	"9051: IOA cache data exists for a missing or failed device"},
	{0x064C8100, 0, IPR_DEFAULT_LOG_LEVEL,
	"9055: Auxiliary cache IOA contains cache data needed by the primary IOA"},
	{0x06670100, 0, IPR_DEFAULT_LOG_LEVEL,
	"9025: Disk unit is not supported at its physical location"},
	{0x06670600, 0, IPR_DEFAULT_LOG_LEVEL,
	"3020: IOA detected a SCSI bus configuration error"},
	{0x06678000, 0, IPR_DEFAULT_LOG_LEVEL,
	"3150: SCSI bus configuration error"},
	{0x06678100, 0, IPR_DEFAULT_LOG_LEVEL,
	"9074: Asymmetric advanced function disk configuration"},
	{0x06678300, 0, IPR_DEFAULT_LOG_LEVEL,
	"4040: Incomplete multipath connection between IOA and enclosure"},
	{0x06678400, 0, IPR_DEFAULT_LOG_LEVEL,
	"4041: Incomplete multipath connection between enclosure and device"},
	{0x06678500, 0, IPR_DEFAULT_LOG_LEVEL,
	"9075: Incomplete multipath connection between IOA and remote IOA"},
	{0x06678600, 0, IPR_DEFAULT_LOG_LEVEL,
	"9076: Configuration error, missing remote IOA"},
	{0x06679100, 0, IPR_DEFAULT_LOG_LEVEL,
	"4050: Enclosure does not support a required multipath function"},
	{0x06690000, 0, IPR_DEFAULT_LOG_LEVEL,
	"4070: Logically bad block written on device"},
	{0x06690200, 0, IPR_DEFAULT_LOG_LEVEL,
	"9041: Array protection temporarily suspended"},
	{0x06698200, 0, IPR_DEFAULT_LOG_LEVEL,
	"9042: Corrupt array parity detected on specified device"},
	{0x066B0200, 0, IPR_DEFAULT_LOG_LEVEL,
	"9030: Array no longer protected due to missing or failed disk unit"},
	{0x066B8000, 0, IPR_DEFAULT_LOG_LEVEL,
	"9071: Link operational transition"},
	{0x066B8100, 0, IPR_DEFAULT_LOG_LEVEL,
	"9072: Link not operational transition"},
	{0x066B8200, 0, IPR_DEFAULT_LOG_LEVEL,
	"9032: Array exposed but still protected"},
	{0x066B8300, 0, IPR_DEFAULT_LOG_LEVEL + 1,
	"70DD: Device forced failed by disrupt device command"},
	{0x066B9100, 0, IPR_DEFAULT_LOG_LEVEL,
	"4061: Multipath redundancy level got better"},
	{0x066B9200, 0, IPR_DEFAULT_LOG_LEVEL,
	"4060: Multipath redundancy level got worse"},
	{0x07270000, 0, 0,
	"Failure due to other device"},
	{0x07278000, 0, IPR_DEFAULT_LOG_LEVEL,
	"9008: IOA does not support functions expected by devices"},
	{0x07278100, 0, IPR_DEFAULT_LOG_LEVEL,
	"9010: Cache data associated with attached devices cannot be found"},
	{0x07278200, 0, IPR_DEFAULT_LOG_LEVEL,
	"9011: Cache data belongs to devices other than those attached"},
	{0x07278400, 0, IPR_DEFAULT_LOG_LEVEL,
	"9020: Array missing 2 or more devices with only 1 device present"},
	{0x07278500, 0, IPR_DEFAULT_LOG_LEVEL,
	"9021: Array missing 2 or more devices with 2 or more devices present"},
	{0x07278600, 0, IPR_DEFAULT_LOG_LEVEL,
	"9022: Exposed array is missing a required device"},
	{0x07278700, 0, IPR_DEFAULT_LOG_LEVEL,
	"9023: Array member(s) not at required physical locations"},
	{0x07278800, 0, IPR_DEFAULT_LOG_LEVEL,
	"9024: Array not functional due to present hardware configuration"},
	{0x07278900, 0, IPR_DEFAULT_LOG_LEVEL,
	"9026: Array not functional due to present hardware configuration"},
	{0x07278A00, 0, IPR_DEFAULT_LOG_LEVEL,
	"9027: Array is missing a device and parity is out of sync"},
	{0x07278B00, 0, IPR_DEFAULT_LOG_LEVEL,
	"9028: Maximum number of arrays already exist"},
	{0x07278C00, 0, IPR_DEFAULT_LOG_LEVEL,
	"9050: Required cache data cannot be located for a disk unit"},
	{0x07278D00, 0, IPR_DEFAULT_LOG_LEVEL,
	"9052: Cache data exists for a device that has been modified"},
	{0x07278F00, 0, IPR_DEFAULT_LOG_LEVEL,
	"9054: IOA resources not available due to previous problems"},
	{0x07279100, 0, IPR_DEFAULT_LOG_LEVEL,
	"9092: Disk unit requires initialization before use"},
	{0x07279200, 0, IPR_DEFAULT_LOG_LEVEL,
	"9029: Incorrect hardware configuration change has been detected"},
	{0x07279600, 0, IPR_DEFAULT_LOG_LEVEL,
	"9060: One or more disk pairs are missing from an array"},
	{0x07279700, 0, IPR_DEFAULT_LOG_LEVEL,
	"9061: One or more disks are missing from an array"},
	{0x07279800, 0, IPR_DEFAULT_LOG_LEVEL,
	"9062: One or more disks are missing from an array"},
	{0x07279900, 0, IPR_DEFAULT_LOG_LEVEL,
	"9063: Maximum number of functional arrays has been exceeded"},
	{0x0B260000, 0, 0,
	"Aborted command, invalid descriptor"},
	{0x0B5A0000, 0, 0,
	"Command terminated by host"}
};

static const struct ipr_ses_table_entry ipr_ses_table[] = {
	{ "2104-DL1        ", "XXXXXXXXXXXXXXXX", 80 },
	{ "2104-TL1        ", "XXXXXXXXXXXXXXXX", 80 },
	{ "HSBP07M P U2SCSI", "XXXXXXXXXXXXXXXX", 80 }, /* Hidive 7 slot */
	{ "HSBP05M P U2SCSI", "XXXXXXXXXXXXXXXX", 80 }, /* Hidive 5 slot */
	{ "HSBP05M S U2SCSI", "XXXXXXXXXXXXXXXX", 80 }, /* Bowtie */
	{ "HSBP06E ASU2SCSI", "XXXXXXXXXXXXXXXX", 80 }, /* MartinFenning */
	{ "2104-DU3        ", "XXXXXXXXXXXXXXXX", 160 },
	{ "2104-TU3        ", "XXXXXXXXXXXXXXXX", 160 },
	{ "HSBP04C RSU2SCSI", "XXXXXXX*XXXXXXXX", 160 },
	{ "HSBP06E RSU2SCSI", "XXXXXXX*XXXXXXXX", 160 },
	{ "St  V1S2        ", "XXXXXXXXXXXXXXXX", 160 },
	{ "HSBPD4M  PU3SCSI", "XXXXXXX*XXXXXXXX", 160 },
	{ "VSBPD1H   U3SCSI", "XXXXXXX*XXXXXXXX", 160 }
};

/*
 *  Function Prototypes
 */
static int ipr_reset_alert(struct ipr_cmnd *);
static void ipr_process_ccn(struct ipr_cmnd *);
static void ipr_process_error(struct ipr_cmnd *);
static void ipr_reset_ioa_job(struct ipr_cmnd *);
static void ipr_initiate_ioa_reset(struct ipr_ioa_cfg *,
				   enum ipr_shutdown_type);

#ifdef CONFIG_SCSI_IPR_TRACE
/**
 * ipr_trc_hook - Add a trace entry to the driver trace
 * @ipr_cmd:	ipr command struct
 * @type:		trace type
 * @add_data:	additional data
 *
 * Return value:
 * 	none
 **/
static void ipr_trc_hook(struct ipr_cmnd *ipr_cmd,
			 u8 type, u32 add_data)
{
	struct ipr_trace_entry *trace_entry;
	struct ipr_ioa_cfg *ioa_cfg = ipr_cmd->ioa_cfg;

	trace_entry = &ioa_cfg->trace[ioa_cfg->trace_index++];
	trace_entry->time = jiffies;
	trace_entry->op_code = ipr_cmd->ioarcb.cmd_pkt.cdb[0];
	trace_entry->type = type;
	trace_entry->ata_op_code = ipr_cmd->ioarcb.add_data.u.regs.command;
	trace_entry->cmd_index = ipr_cmd->cmd_index & 0xff;
	trace_entry->res_handle = ipr_cmd->ioarcb.res_handle;
	trace_entry->u.add_data = add_data;
}
#else
#define ipr_trc_hook(ipr_cmd, type, add_data) do { } while(0)
#endif

/**
 * ipr_reinit_ipr_cmnd - Re-initialize an IPR Cmnd block for reuse
 * @ipr_cmd:	ipr command struct
 *
 * Return value:
 * 	none
 **/
static void ipr_reinit_ipr_cmnd(struct ipr_cmnd *ipr_cmd)
{
	struct ipr_ioarcb *ioarcb = &ipr_cmd->ioarcb;
	struct ipr_ioasa *ioasa = &ipr_cmd->ioasa;
	dma_addr_t dma_addr = be32_to_cpu(ioarcb->ioarcb_host_pci_addr);

	memset(&ioarcb->cmd_pkt, 0, sizeof(struct ipr_cmd_pkt));
	ioarcb->write_data_transfer_length = 0;
	ioarcb->read_data_transfer_length = 0;
	ioarcb->write_ioadl_len = 0;
	ioarcb->read_ioadl_len = 0;
	ioarcb->write_ioadl_addr =
		cpu_to_be32(dma_addr + offsetof(struct ipr_cmnd, ioadl));
	ioarcb->read_ioadl_addr = ioarcb->write_ioadl_addr;
	ioasa->ioasc = 0;
	ioasa->residual_data_len = 0;
	ioasa->u.gata.status = 0;

	ipr_cmd->scsi_cmd = NULL;
	ipr_cmd->qc = NULL;
	ipr_cmd->sense_buffer[0] = 0;
	ipr_cmd->dma_use_sg = 0;
}

/**
 * ipr_init_ipr_cmnd - Initialize an IPR Cmnd block
 * @ipr_cmd:	ipr command struct
 *
 * Return value:
 * 	none
 **/
static void ipr_init_ipr_cmnd(struct ipr_cmnd *ipr_cmd)
{
	ipr_reinit_ipr_cmnd(ipr_cmd);
	ipr_cmd->u.scratch = 0;
	ipr_cmd->sibling = NULL;
	init_timer(&ipr_cmd->timer);
}

/**
 * ipr_get_free_ipr_cmnd - Get a free IPR Cmnd block
 * @ioa_cfg:	ioa config struct
 *
 * Return value:
 * 	pointer to ipr command struct
 **/
static
struct ipr_cmnd *ipr_get_free_ipr_cmnd(struct ipr_ioa_cfg *ioa_cfg)
{
	struct ipr_cmnd *ipr_cmd;

	ipr_cmd = list_entry(ioa_cfg->free_q.next, struct ipr_cmnd, queue);
	list_del(&ipr_cmd->queue);
	ipr_init_ipr_cmnd(ipr_cmd);

	return ipr_cmd;
}

/**
 * ipr_mask_and_clear_interrupts - Mask all and clear specified interrupts
 * @ioa_cfg:	ioa config struct
 * @clr_ints:     interrupts to clear
 *
 * This function masks all interrupts on the adapter, then clears the
 * interrupts specified in the mask
 *
 * Return value:
 * 	none
 **/
static void ipr_mask_and_clear_interrupts(struct ipr_ioa_cfg *ioa_cfg,
					  u32 clr_ints)
{
	volatile u32 int_reg;

	/* Stop new interrupts */
	ioa_cfg->allow_interrupts = 0;

	/* Set interrupt mask to stop all new interrupts */
	writel(~0, ioa_cfg->regs.set_interrupt_mask_reg);

	/* Clear any pending interrupts */
	writel(clr_ints, ioa_cfg->regs.clr_interrupt_reg);
	int_reg = readl(ioa_cfg->regs.sense_interrupt_reg);
}

/**
 * ipr_save_pcix_cmd_reg - Save PCI-X command register
 * @ioa_cfg:	ioa config struct
 *
 * Return value:
 * 	0 on success / -EIO on failure
 **/
static int ipr_save_pcix_cmd_reg(struct ipr_ioa_cfg *ioa_cfg)
{
	int pcix_cmd_reg = pci_find_capability(ioa_cfg->pdev, PCI_CAP_ID_PCIX);

	if (pcix_cmd_reg == 0)
		return 0;

	if (pci_read_config_word(ioa_cfg->pdev, pcix_cmd_reg + PCI_X_CMD,
				 &ioa_cfg->saved_pcix_cmd_reg) != PCIBIOS_SUCCESSFUL) {
		dev_err(&ioa_cfg->pdev->dev, "Failed to save PCI-X command register\n");
		return -EIO;
	}

	ioa_cfg->saved_pcix_cmd_reg |= PCI_X_CMD_DPERR_E | PCI_X_CMD_ERO;
	return 0;
}

/**
 * ipr_set_pcix_cmd_reg - Setup PCI-X command register
 * @ioa_cfg:	ioa config struct
 *
 * Return value:
 * 	0 on success / -EIO on failure
 **/
static int ipr_set_pcix_cmd_reg(struct ipr_ioa_cfg *ioa_cfg)
{
	int pcix_cmd_reg = pci_find_capability(ioa_cfg->pdev, PCI_CAP_ID_PCIX);

	if (pcix_cmd_reg) {
		if (pci_write_config_word(ioa_cfg->pdev, pcix_cmd_reg + PCI_X_CMD,
					  ioa_cfg->saved_pcix_cmd_reg) != PCIBIOS_SUCCESSFUL) {
			dev_err(&ioa_cfg->pdev->dev, "Failed to setup PCI-X command register\n");
			return -EIO;
		}
	}

	return 0;
}

/**
 * ipr_sata_eh_done - done function for aborted SATA commands
 * @ipr_cmd:	ipr command struct
 *
 * This function is invoked for ops generated to SATA
 * devices which are being aborted.
 *
 * Return value:
 * 	none
 **/
static void ipr_sata_eh_done(struct ipr_cmnd *ipr_cmd)
{
	struct ipr_ioa_cfg *ioa_cfg = ipr_cmd->ioa_cfg;
	struct ata_queued_cmd *qc = ipr_cmd->qc;
	struct ipr_sata_port *sata_port = qc->ap->private_data;

	qc->err_mask |= AC_ERR_OTHER;
	sata_port->ioasa.status |= ATA_BUSY;
	list_add_tail(&ipr_cmd->queue, &ioa_cfg->free_q);
	ata_qc_complete(qc);
}

/**
 * ipr_scsi_eh_done - mid-layer done function for aborted ops
 * @ipr_cmd:	ipr command struct
 *
 * This function is invoked by the interrupt handler for
 * ops generated by the SCSI mid-layer which are being aborted.
 *
 * Return value:
 * 	none
 **/
static void ipr_scsi_eh_done(struct ipr_cmnd *ipr_cmd)
{
	struct ipr_ioa_cfg *ioa_cfg = ipr_cmd->ioa_cfg;
	struct scsi_cmnd *scsi_cmd = ipr_cmd->scsi_cmd;

	scsi_cmd->result |= (DID_ERROR << 16);

	scsi_dma_unmap(ipr_cmd->scsi_cmd);
	scsi_cmd->scsi_done(scsi_cmd);
	list_add_tail(&ipr_cmd->queue, &ioa_cfg->free_q);
}

/**
 * ipr_fail_all_ops - Fails all outstanding ops.
 * @ioa_cfg:	ioa config struct
 *
 * This function fails all outstanding ops.
 *
 * Return value:
 * 	none
 **/
static void ipr_fail_all_ops(struct ipr_ioa_cfg *ioa_cfg)
{
	struct ipr_cmnd *ipr_cmd, *temp;

	ENTER;
	list_for_each_entry_safe(ipr_cmd, temp, &ioa_cfg->pending_q, queue) {
		list_del(&ipr_cmd->queue);

		ipr_cmd->ioasa.ioasc = cpu_to_be32(IPR_IOASC_IOA_WAS_RESET);
		ipr_cmd->ioasa.ilid = cpu_to_be32(IPR_DRIVER_ILID);

		if (ipr_cmd->scsi_cmd)
			ipr_cmd->done = ipr_scsi_eh_done;
		else if (ipr_cmd->qc)
			ipr_cmd->done = ipr_sata_eh_done;

		ipr_trc_hook(ipr_cmd, IPR_TRACE_FINISH, IPR_IOASC_IOA_WAS_RESET);
		del_timer(&ipr_cmd->timer);
		ipr_cmd->done(ipr_cmd);
	}

	LEAVE;
}

/**
 * ipr_do_req -  Send driver initiated requests.
 * @ipr_cmd:		ipr command struct
 * @done:			done function
 * @timeout_func:	timeout function
 * @timeout:		timeout value
 *
 * This function sends the specified command to the adapter with the
 * timeout given. The done function is invoked on command completion.
 *
 * Return value:
 * 	none
 **/
static void ipr_do_req(struct ipr_cmnd *ipr_cmd,
		       void (*done) (struct ipr_cmnd *),
		       void (*timeout_func) (struct ipr_cmnd *), u32 timeout)
{
	struct ipr_ioa_cfg *ioa_cfg = ipr_cmd->ioa_cfg;

	list_add_tail(&ipr_cmd->queue, &ioa_cfg->pending_q);

	ipr_cmd->done = done;

	ipr_cmd->timer.data = (unsigned long) ipr_cmd;
	ipr_cmd->timer.expires = jiffies + timeout;
	ipr_cmd->timer.function = (void (*)(unsigned long))timeout_func;

	add_timer(&ipr_cmd->timer);

	ipr_trc_hook(ipr_cmd, IPR_TRACE_START, 0);

	mb();
	writel(be32_to_cpu(ipr_cmd->ioarcb.ioarcb_host_pci_addr),
	       ioa_cfg->regs.ioarrin_reg);
}

/**
 * ipr_internal_cmd_done - Op done function for an internally generated op.
 * @ipr_cmd:	ipr command struct
 *
 * This function is the op done function for an internally generated,
 * blocking op. It simply wakes the sleeping thread.
 *
 * Return value:
 * 	none
 **/
static void ipr_internal_cmd_done(struct ipr_cmnd *ipr_cmd)
{
	if (ipr_cmd->sibling)
		ipr_cmd->sibling = NULL;
	else
		complete(&ipr_cmd->completion);
}

/**
 * ipr_send_blocking_cmd - Send command and sleep on its completion.
 * @ipr_cmd:	ipr command struct
 * @timeout_func:	function to invoke if command times out
 * @timeout:	timeout
 *
 * Return value:
 * 	none
 **/
static void ipr_send_blocking_cmd(struct ipr_cmnd *ipr_cmd,
				  void (*timeout_func) (struct ipr_cmnd *ipr_cmd),
				  u32 timeout)
{
	struct ipr_ioa_cfg *ioa_cfg = ipr_cmd->ioa_cfg;

	init_completion(&ipr_cmd->completion);
	ipr_do_req(ipr_cmd, ipr_internal_cmd_done, timeout_func, timeout);

	spin_unlock_irq(ioa_cfg->host->host_lock);
	wait_for_completion(&ipr_cmd->completion);
	spin_lock_irq(ioa_cfg->host->host_lock);
}

/**
 * ipr_send_hcam - Send an HCAM to the adapter.
 * @ioa_cfg:	ioa config struct
 * @type:		HCAM type
 * @hostrcb:	hostrcb struct
 *
 * This function will send a Host Controlled Async command to the adapter.
 * If HCAMs are currently not allowed to be issued to the adapter, it will
 * place the hostrcb on the free queue.
 *
 * Return value:
 * 	none
 **/
static void ipr_send_hcam(struct ipr_ioa_cfg *ioa_cfg, u8 type,
			  struct ipr_hostrcb *hostrcb)
{
	struct ipr_cmnd *ipr_cmd;
	struct ipr_ioarcb *ioarcb;

	if (ioa_cfg->allow_cmds) {
		ipr_cmd = ipr_get_free_ipr_cmnd(ioa_cfg);
		list_add_tail(&ipr_cmd->queue, &ioa_cfg->pending_q);
		list_add_tail(&hostrcb->queue, &ioa_cfg->hostrcb_pending_q);

		ipr_cmd->u.hostrcb = hostrcb;
		ioarcb = &ipr_cmd->ioarcb;

		ioarcb->res_handle = cpu_to_be32(IPR_IOA_RES_HANDLE);
		ioarcb->cmd_pkt.request_type = IPR_RQTYPE_HCAM;
		ioarcb->cmd_pkt.cdb[0] = IPR_HOST_CONTROLLED_ASYNC;
		ioarcb->cmd_pkt.cdb[1] = type;
		ioarcb->cmd_pkt.cdb[7] = (sizeof(hostrcb->hcam) >> 8) & 0xff;
		ioarcb->cmd_pkt.cdb[8] = sizeof(hostrcb->hcam) & 0xff;

		ioarcb->read_data_transfer_length = cpu_to_be32(sizeof(hostrcb->hcam));
		ioarcb->read_ioadl_len = cpu_to_be32(sizeof(struct ipr_ioadl_desc));
		ipr_cmd->ioadl[0].flags_and_data_len =
			cpu_to_be32(IPR_IOADL_FLAGS_READ_LAST | sizeof(hostrcb->hcam));
		ipr_cmd->ioadl[0].address = cpu_to_be32(hostrcb->hostrcb_dma);

		if (type == IPR_HCAM_CDB_OP_CODE_CONFIG_CHANGE)
			ipr_cmd->done = ipr_process_ccn;
		else
			ipr_cmd->done = ipr_process_error;

		ipr_trc_hook(ipr_cmd, IPR_TRACE_START, IPR_IOA_RES_ADDR);

		mb();
		writel(be32_to_cpu(ipr_cmd->ioarcb.ioarcb_host_pci_addr),
		       ioa_cfg->regs.ioarrin_reg);
	} else {
		list_add_tail(&hostrcb->queue, &ioa_cfg->hostrcb_free_q);
	}
}

/**
 * ipr_init_res_entry - Initialize a resource entry struct.
 * @res:	resource entry struct
 *
 * Return value:
 * 	none
 **/
static void ipr_init_res_entry(struct ipr_resource_entry *res)
{
	res->needs_sync_complete = 0;
	res->in_erp = 0;
	res->add_to_ml = 0;
	res->del_from_ml = 0;
	res->resetting_device = 0;
	res->sdev = NULL;
	res->sata_port = NULL;
}

/**
 * ipr_handle_config_change - Handle a config change from the adapter
 * @ioa_cfg:	ioa config struct
 * @hostrcb:	hostrcb
 *
 * Return value:
 * 	none
 **/
static void ipr_handle_config_change(struct ipr_ioa_cfg *ioa_cfg,
			      struct ipr_hostrcb *hostrcb)
{
	struct ipr_resource_entry *res = NULL;
	struct ipr_config_table_entry *cfgte;
	u32 is_ndn = 1;

	cfgte = &hostrcb->hcam.u.ccn.cfgte;

	list_for_each_entry(res, &ioa_cfg->used_res_q, queue) {
		if (!memcmp(&res->cfgte.res_addr, &cfgte->res_addr,
			    sizeof(cfgte->res_addr))) {
			is_ndn = 0;
			break;
		}
	}

	if (is_ndn) {
		if (list_empty(&ioa_cfg->free_res_q)) {
			ipr_send_hcam(ioa_cfg,
				      IPR_HCAM_CDB_OP_CODE_CONFIG_CHANGE,
				      hostrcb);
			return;
		}

		res = list_entry(ioa_cfg->free_res_q.next,
				 struct ipr_resource_entry, queue);

		list_del(&res->queue);
		ipr_init_res_entry(res);
		list_add_tail(&res->queue, &ioa_cfg->used_res_q);
	}

	memcpy(&res->cfgte, cfgte, sizeof(struct ipr_config_table_entry));

	if (hostrcb->hcam.notify_type == IPR_HOST_RCB_NOTIF_TYPE_REM_ENTRY) {
		if (res->sdev) {
			res->del_from_ml = 1;
			res->cfgte.res_handle = IPR_INVALID_RES_HANDLE;
			if (ioa_cfg->allow_ml_add_del)
				schedule_work(&ioa_cfg->work_q);
		} else
			list_move_tail(&res->queue, &ioa_cfg->free_res_q);
	} else if (!res->sdev) {
		res->add_to_ml = 1;
		if (ioa_cfg->allow_ml_add_del)
			schedule_work(&ioa_cfg->work_q);
	}

	ipr_send_hcam(ioa_cfg, IPR_HCAM_CDB_OP_CODE_CONFIG_CHANGE, hostrcb);
}

/**
 * ipr_process_ccn - Op done function for a CCN.
 * @ipr_cmd:	ipr command struct
 *
 * This function is the op done function for a configuration
 * change notification host controlled async from the adapter.
 *
 * Return value:
 * 	none
 **/
static void ipr_process_ccn(struct ipr_cmnd *ipr_cmd)
{
	struct ipr_ioa_cfg *ioa_cfg = ipr_cmd->ioa_cfg;
	struct ipr_hostrcb *hostrcb = ipr_cmd->u.hostrcb;
	u32 ioasc = be32_to_cpu(ipr_cmd->ioasa.ioasc);

	list_del(&hostrcb->queue);
	list_add_tail(&ipr_cmd->queue, &ioa_cfg->free_q);

	if (ioasc) {
		if (ioasc != IPR_IOASC_IOA_WAS_RESET)
			dev_err(&ioa_cfg->pdev->dev,
				"Host RCB failed with IOASC: 0x%08X\n", ioasc);

		ipr_send_hcam(ioa_cfg, IPR_HCAM_CDB_OP_CODE_CONFIG_CHANGE, hostrcb);
	} else {
		ipr_handle_config_change(ioa_cfg, hostrcb);
	}
}

/**
 * strip_and_pad_whitespace - Strip and pad trailing whitespace.
 * @i:		index into buffer
 * @buf:		string to modify
 *
 * This function will strip all trailing whitespace, pad the end
 * of the string with a single space, and NULL terminate the string.
 *
 * Return value:
 * 	new length of string
 **/
static int strip_and_pad_whitespace(int i, char *buf)
{
	while (i && buf[i] == ' ')
		i--;
	buf[i+1] = ' ';
	buf[i+2] = '\0';
	return i + 2;
}

/**
 * ipr_log_vpd_compact - Log the passed extended VPD compactly.
 * @prefix:		string to print at start of printk
 * @hostrcb:	hostrcb pointer
 * @vpd:		vendor/product id/sn struct
 *
 * Return value:
 * 	none
 **/
static void ipr_log_vpd_compact(char *prefix, struct ipr_hostrcb *hostrcb,
				struct ipr_vpd *vpd)
{
	char buffer[IPR_VENDOR_ID_LEN + IPR_PROD_ID_LEN + IPR_SERIAL_NUM_LEN + 3];
	int i = 0;

	memcpy(buffer, vpd->vpids.vendor_id, IPR_VENDOR_ID_LEN);
	i = strip_and_pad_whitespace(IPR_VENDOR_ID_LEN - 1, buffer);

	memcpy(&buffer[i], vpd->vpids.product_id, IPR_PROD_ID_LEN);
	i = strip_and_pad_whitespace(i + IPR_PROD_ID_LEN - 1, buffer);

	memcpy(&buffer[i], vpd->sn, IPR_SERIAL_NUM_LEN);
	buffer[IPR_SERIAL_NUM_LEN + i] = '\0';

	ipr_hcam_err(hostrcb, "%s VPID/SN: %s\n", prefix, buffer);
}

/**
 * ipr_log_vpd - Log the passed VPD to the error log.
 * @vpd:		vendor/product id/sn struct
 *
 * Return value:
 * 	none
 **/
static void ipr_log_vpd(struct ipr_vpd *vpd)
{
	char buffer[IPR_VENDOR_ID_LEN + IPR_PROD_ID_LEN
		    + IPR_SERIAL_NUM_LEN];

	memcpy(buffer, vpd->vpids.vendor_id, IPR_VENDOR_ID_LEN);
	memcpy(buffer + IPR_VENDOR_ID_LEN, vpd->vpids.product_id,
	       IPR_PROD_ID_LEN);
	buffer[IPR_VENDOR_ID_LEN + IPR_PROD_ID_LEN] = '\0';
	ipr_err("Vendor/Product ID: %s\n", buffer);

	memcpy(buffer, vpd->sn, IPR_SERIAL_NUM_LEN);
	buffer[IPR_SERIAL_NUM_LEN] = '\0';
	ipr_err("    Serial Number: %s\n", buffer);
}

/**
 * ipr_log_ext_vpd_compact - Log the passed extended VPD compactly.
 * @prefix:		string to print at start of printk
 * @hostrcb:	hostrcb pointer
 * @vpd:		vendor/product id/sn/wwn struct
 *
 * Return value:
 * 	none
 **/
static void ipr_log_ext_vpd_compact(char *prefix, struct ipr_hostrcb *hostrcb,
				    struct ipr_ext_vpd *vpd)
{
	ipr_log_vpd_compact(prefix, hostrcb, &vpd->vpd);
	ipr_hcam_err(hostrcb, "%s WWN: %08X%08X\n", prefix,
		     be32_to_cpu(vpd->wwid[0]), be32_to_cpu(vpd->wwid[1]));
}

/**
 * ipr_log_ext_vpd - Log the passed extended VPD to the error log.
 * @vpd:		vendor/product id/sn/wwn struct
 *
 * Return value:
 * 	none
 **/
static void ipr_log_ext_vpd(struct ipr_ext_vpd *vpd)
{
	ipr_log_vpd(&vpd->vpd);
	ipr_err("    WWN: %08X%08X\n", be32_to_cpu(vpd->wwid[0]),
		be32_to_cpu(vpd->wwid[1]));
}

/**
 * ipr_log_enhanced_cache_error - Log a cache error.
 * @ioa_cfg:	ioa config struct
 * @hostrcb:	hostrcb struct
 *
 * Return value:
 * 	none
 **/
static void ipr_log_enhanced_cache_error(struct ipr_ioa_cfg *ioa_cfg,
					 struct ipr_hostrcb *hostrcb)
{
	struct ipr_hostrcb_type_12_error *error =
		&hostrcb->hcam.u.error.u.type_12_error;

	ipr_err("-----Current Configuration-----\n");
	ipr_err("Cache Directory Card Information:\n");
	ipr_log_ext_vpd(&error->ioa_vpd);
	ipr_err("Adapter Card Information:\n");
	ipr_log_ext_vpd(&error->cfc_vpd);

	ipr_err("-----Expected Configuration-----\n");
	ipr_err("Cache Directory Card Information:\n");
	ipr_log_ext_vpd(&error->ioa_last_attached_to_cfc_vpd);
	ipr_err("Adapter Card Information:\n");
	ipr_log_ext_vpd(&error->cfc_last_attached_to_ioa_vpd);

	ipr_err("Additional IOA Data: %08X %08X %08X\n",
		     be32_to_cpu(error->ioa_data[0]),
		     be32_to_cpu(error->ioa_data[1]),
		     be32_to_cpu(error->ioa_data[2]));
}

/**
 * ipr_log_cache_error - Log a cache error.
 * @ioa_cfg:	ioa config struct
 * @hostrcb:	hostrcb struct
 *
 * Return value:
 * 	none
 **/
static void ipr_log_cache_error(struct ipr_ioa_cfg *ioa_cfg,
				struct ipr_hostrcb *hostrcb)
{
	struct ipr_hostrcb_type_02_error *error =
		&hostrcb->hcam.u.error.u.type_02_error;

	ipr_err("-----Current Configuration-----\n");
	ipr_err("Cache Directory Card Information:\n");
	ipr_log_vpd(&error->ioa_vpd);
	ipr_err("Adapter Card Information:\n");
	ipr_log_vpd(&error->cfc_vpd);

	ipr_err("-----Expected Configuration-----\n");
	ipr_err("Cache Directory Card Information:\n");
	ipr_log_vpd(&error->ioa_last_attached_to_cfc_vpd);
	ipr_err("Adapter Card Information:\n");
	ipr_log_vpd(&error->cfc_last_attached_to_ioa_vpd);

	ipr_err("Additional IOA Data: %08X %08X %08X\n",
		     be32_to_cpu(error->ioa_data[0]),
		     be32_to_cpu(error->ioa_data[1]),
		     be32_to_cpu(error->ioa_data[2]));
}

/**
 * ipr_log_enhanced_config_error - Log a configuration error.
 * @ioa_cfg:	ioa config struct
 * @hostrcb:	hostrcb struct
 *
 * Return value:
 * 	none
 **/
static void ipr_log_enhanced_config_error(struct ipr_ioa_cfg *ioa_cfg,
					  struct ipr_hostrcb *hostrcb)
{
	int errors_logged, i;
	struct ipr_hostrcb_device_data_entry_enhanced *dev_entry;
	struct ipr_hostrcb_type_13_error *error;

	error = &hostrcb->hcam.u.error.u.type_13_error;
	errors_logged = be32_to_cpu(error->errors_logged);

	ipr_err("Device Errors Detected/Logged: %d/%d\n",
		be32_to_cpu(error->errors_detected), errors_logged);

	dev_entry = error->dev;

	for (i = 0; i < errors_logged; i++, dev_entry++) {
		ipr_err_separator;

		ipr_phys_res_err(ioa_cfg, dev_entry->dev_res_addr, "Device %d", i + 1);
		ipr_log_ext_vpd(&dev_entry->vpd);

		ipr_err("-----New Device Information-----\n");
		ipr_log_ext_vpd(&dev_entry->new_vpd);

		ipr_err("Cache Directory Card Information:\n");
		ipr_log_ext_vpd(&dev_entry->ioa_last_with_dev_vpd);

		ipr_err("Adapter Card Information:\n");
		ipr_log_ext_vpd(&dev_entry->cfc_last_with_dev_vpd);
	}
}

/**
 * ipr_log_config_error - Log a configuration error.
 * @ioa_cfg:	ioa config struct
 * @hostrcb:	hostrcb struct
 *
 * Return value:
 * 	none
 **/
static void ipr_log_config_error(struct ipr_ioa_cfg *ioa_cfg,
				 struct ipr_hostrcb *hostrcb)
{
	int errors_logged, i;
	struct ipr_hostrcb_device_data_entry *dev_entry;
	struct ipr_hostrcb_type_03_error *error;

	error = &hostrcb->hcam.u.error.u.type_03_error;
	errors_logged = be32_to_cpu(error->errors_logged);

	ipr_err("Device Errors Detected/Logged: %d/%d\n",
		be32_to_cpu(error->errors_detected), errors_logged);

	dev_entry = error->dev;

	for (i = 0; i < errors_logged; i++, dev_entry++) {
		ipr_err_separator;

		ipr_phys_res_err(ioa_cfg, dev_entry->dev_res_addr, "Device %d", i + 1);
		ipr_log_vpd(&dev_entry->vpd);

		ipr_err("-----New Device Information-----\n");
		ipr_log_vpd(&dev_entry->new_vpd);

		ipr_err("Cache Directory Card Information:\n");
		ipr_log_vpd(&dev_entry->ioa_last_with_dev_vpd);

		ipr_err("Adapter Card Information:\n");
		ipr_log_vpd(&dev_entry->cfc_last_with_dev_vpd);

		ipr_err("Additional IOA Data: %08X %08X %08X %08X %08X\n",
			be32_to_cpu(dev_entry->ioa_data[0]),
			be32_to_cpu(dev_entry->ioa_data[1]),
			be32_to_cpu(dev_entry->ioa_data[2]),
			be32_to_cpu(dev_entry->ioa_data[3]),
			be32_to_cpu(dev_entry->ioa_data[4]));
	}
}

/**
 * ipr_log_enhanced_array_error - Log an array configuration error.
 * @ioa_cfg:	ioa config struct
 * @hostrcb:	hostrcb struct
 *
 * Return value:
 * 	none
 **/
static void ipr_log_enhanced_array_error(struct ipr_ioa_cfg *ioa_cfg,
					 struct ipr_hostrcb *hostrcb)
{
	int i, num_entries;
	struct ipr_hostrcb_type_14_error *error;
	struct ipr_hostrcb_array_data_entry_enhanced *array_entry;
	const u8 zero_sn[IPR_SERIAL_NUM_LEN] = { [0 ... IPR_SERIAL_NUM_LEN-1] = '0' };

	error = &hostrcb->hcam.u.error.u.type_14_error;

	ipr_err_separator;

	ipr_err("RAID %s Array Configuration: %d:%d:%d:%d\n",
		error->protection_level,
		ioa_cfg->host->host_no,
		error->last_func_vset_res_addr.bus,
		error->last_func_vset_res_addr.target,
		error->last_func_vset_res_addr.lun);

	ipr_err_separator;

	array_entry = error->array_member;
	num_entries = min_t(u32, be32_to_cpu(error->num_entries),
			    sizeof(error->array_member));

	for (i = 0; i < num_entries; i++, array_entry++) {
		if (!memcmp(array_entry->vpd.vpd.sn, zero_sn, IPR_SERIAL_NUM_LEN))
			continue;

		if (be32_to_cpu(error->exposed_mode_adn) == i)
			ipr_err("Exposed Array Member %d:\n", i);
		else
			ipr_err("Array Member %d:\n", i);

		ipr_log_ext_vpd(&array_entry->vpd);
		ipr_phys_res_err(ioa_cfg, array_entry->dev_res_addr, "Current Location");
		ipr_phys_res_err(ioa_cfg, array_entry->expected_dev_res_addr,
				 "Expected Location");

		ipr_err_separator;
	}
}

/**
 * ipr_log_array_error - Log an array configuration error.
 * @ioa_cfg:	ioa config struct
 * @hostrcb:	hostrcb struct
 *
 * Return value:
 * 	none
 **/
static void ipr_log_array_error(struct ipr_ioa_cfg *ioa_cfg,
				struct ipr_hostrcb *hostrcb)
{
	int i;
	struct ipr_hostrcb_type_04_error *error;
	struct ipr_hostrcb_array_data_entry *array_entry;
	const u8 zero_sn[IPR_SERIAL_NUM_LEN] = { [0 ... IPR_SERIAL_NUM_LEN-1] = '0' };

	error = &hostrcb->hcam.u.error.u.type_04_error;

	ipr_err_separator;

	ipr_err("RAID %s Array Configuration: %d:%d:%d:%d\n",
		error->protection_level,
		ioa_cfg->host->host_no,
		error->last_func_vset_res_addr.bus,
		error->last_func_vset_res_addr.target,
		error->last_func_vset_res_addr.lun);

	ipr_err_separator;

	array_entry = error->array_member;

	for (i = 0; i < 18; i++) {
		if (!memcmp(array_entry->vpd.sn, zero_sn, IPR_SERIAL_NUM_LEN))
			continue;

		if (be32_to_cpu(error->exposed_mode_adn) == i)
			ipr_err("Exposed Array Member %d:\n", i);
		else
			ipr_err("Array Member %d:\n", i);

		ipr_log_vpd(&array_entry->vpd);

		ipr_phys_res_err(ioa_cfg, array_entry->dev_res_addr, "Current Location");
		ipr_phys_res_err(ioa_cfg, array_entry->expected_dev_res_addr,
				 "Expected Location");

		ipr_err_separator;

		if (i == 9)
			array_entry = error->array_member2;
		else
			array_entry++;
	}
}

/**
 * ipr_log_hex_data - Log additional hex IOA error data.
 * @ioa_cfg:	ioa config struct
 * @data:		IOA error data
 * @len:		data length
 *
 * Return value:
 * 	none
 **/
static void ipr_log_hex_data(struct ipr_ioa_cfg *ioa_cfg, u32 *data, int len)
{
	int i;

	if (len == 0)
		return;

	if (ioa_cfg->log_level <= IPR_DEFAULT_LOG_LEVEL)
		len = min_t(int, len, IPR_DEFAULT_MAX_ERROR_DUMP);

	for (i = 0; i < len / 4; i += 4) {
		ipr_err("%08X: %08X %08X %08X %08X\n", i*4,
			be32_to_cpu(data[i]),
			be32_to_cpu(data[i+1]),
			be32_to_cpu(data[i+2]),
			be32_to_cpu(data[i+3]));
	}
}

/**
 * ipr_log_enhanced_dual_ioa_error - Log an enhanced dual adapter error.
 * @ioa_cfg:	ioa config struct
 * @hostrcb:	hostrcb struct
 *
 * Return value:
 * 	none
 **/
static void ipr_log_enhanced_dual_ioa_error(struct ipr_ioa_cfg *ioa_cfg,
					    struct ipr_hostrcb *hostrcb)
{
	struct ipr_hostrcb_type_17_error *error;

	error = &hostrcb->hcam.u.error.u.type_17_error;
	error->failure_reason[sizeof(error->failure_reason) - 1] = '\0';
	strstrip(error->failure_reason);

	ipr_hcam_err(hostrcb, "%s [PRC: %08X]\n", error->failure_reason,
		     be32_to_cpu(hostrcb->hcam.u.error.prc));
	ipr_log_ext_vpd_compact("Remote IOA", hostrcb, &error->vpd);
	ipr_log_hex_data(ioa_cfg, error->data,
			 be32_to_cpu(hostrcb->hcam.length) -
			 (offsetof(struct ipr_hostrcb_error, u) +
			  offsetof(struct ipr_hostrcb_type_17_error, data)));
}

/**
 * ipr_log_dual_ioa_error - Log a dual adapter error.
 * @ioa_cfg:	ioa config struct
 * @hostrcb:	hostrcb struct
 *
 * Return value:
 * 	none
 **/
static void ipr_log_dual_ioa_error(struct ipr_ioa_cfg *ioa_cfg,
				   struct ipr_hostrcb *hostrcb)
{
	struct ipr_hostrcb_type_07_error *error;

	error = &hostrcb->hcam.u.error.u.type_07_error;
	error->failure_reason[sizeof(error->failure_reason) - 1] = '\0';
	strstrip(error->failure_reason);

	ipr_hcam_err(hostrcb, "%s [PRC: %08X]\n", error->failure_reason,
		     be32_to_cpu(hostrcb->hcam.u.error.prc));
	ipr_log_vpd_compact("Remote IOA", hostrcb, &error->vpd);
	ipr_log_hex_data(ioa_cfg, error->data,
			 be32_to_cpu(hostrcb->hcam.length) -
			 (offsetof(struct ipr_hostrcb_error, u) +
			  offsetof(struct ipr_hostrcb_type_07_error, data)));
}

static const struct {
	u8 active;
	char *desc;
} path_active_desc[] = {
	{ IPR_PATH_NO_INFO, "Path" },
	{ IPR_PATH_ACTIVE, "Active path" },
	{ IPR_PATH_NOT_ACTIVE, "Inactive path" }
};

static const struct {
	u8 state;
	char *desc;
} path_state_desc[] = {
	{ IPR_PATH_STATE_NO_INFO, "has no path state information available" },
	{ IPR_PATH_HEALTHY, "is healthy" },
	{ IPR_PATH_DEGRADED, "is degraded" },
	{ IPR_PATH_FAILED, "is failed" }
};

/**
 * ipr_log_fabric_path - Log a fabric path error
 * @hostrcb:	hostrcb struct
 * @fabric:		fabric descriptor
 *
 * Return value:
 * 	none
 **/
static void ipr_log_fabric_path(struct ipr_hostrcb *hostrcb,
				struct ipr_hostrcb_fabric_desc *fabric)
{
	int i, j;
	u8 path_state = fabric->path_state;
	u8 active = path_state & IPR_PATH_ACTIVE_MASK;
	u8 state = path_state & IPR_PATH_STATE_MASK;

	for (i = 0; i < ARRAY_SIZE(path_active_desc); i++) {
		if (path_active_desc[i].active != active)
			continue;

		for (j = 0; j < ARRAY_SIZE(path_state_desc); j++) {
			if (path_state_desc[j].state != state)
				continue;

			if (fabric->cascaded_expander == 0xff && fabric->phy == 0xff) {
				ipr_hcam_err(hostrcb, "%s %s: IOA Port=%d\n",
					     path_active_desc[i].desc, path_state_desc[j].desc,
					     fabric->ioa_port);
			} else if (fabric->cascaded_expander == 0xff) {
				ipr_hcam_err(hostrcb, "%s %s: IOA Port=%d, Phy=%d\n",
					     path_active_desc[i].desc, path_state_desc[j].desc,
					     fabric->ioa_port, fabric->phy);
			} else if (fabric->phy == 0xff) {
				ipr_hcam_err(hostrcb, "%s %s: IOA Port=%d, Cascade=%d\n",
					     path_active_desc[i].desc, path_state_desc[j].desc,
					     fabric->ioa_port, fabric->cascaded_expander);
			} else {
				ipr_hcam_err(hostrcb, "%s %s: IOA Port=%d, Cascade=%d, Phy=%d\n",
					     path_active_desc[i].desc, path_state_desc[j].desc,
					     fabric->ioa_port, fabric->cascaded_expander, fabric->phy);
			}
			return;
		}
	}

	ipr_err("Path state=%02X IOA Port=%d Cascade=%d Phy=%d\n", path_state,
		fabric->ioa_port, fabric->cascaded_expander, fabric->phy);
}

static const struct {
	u8 type;
	char *desc;
} path_type_desc[] = {
	{ IPR_PATH_CFG_IOA_PORT, "IOA port" },
	{ IPR_PATH_CFG_EXP_PORT, "Expander port" },
	{ IPR_PATH_CFG_DEVICE_PORT, "Device port" },
	{ IPR_PATH_CFG_DEVICE_LUN, "Device LUN" }
};

static const struct {
	u8 status;
	char *desc;
} path_status_desc[] = {
	{ IPR_PATH_CFG_NO_PROB, "Functional" },
	{ IPR_PATH_CFG_DEGRADED, "Degraded" },
	{ IPR_PATH_CFG_FAILED, "Failed" },
	{ IPR_PATH_CFG_SUSPECT, "Suspect" },
	{ IPR_PATH_NOT_DETECTED, "Missing" },
	{ IPR_PATH_INCORRECT_CONN, "Incorrectly connected" }
};

static const char *link_rate[] = {
	"unknown",
	"disabled",
	"phy reset problem",
	"spinup hold",
	"port selector",
	"unknown",
	"unknown",
	"unknown",
	"1.5Gbps",
	"3.0Gbps",
	"unknown",
	"unknown",
	"unknown",
	"unknown",
	"unknown",
	"unknown"
};

/**
 * ipr_log_path_elem - Log a fabric path element.
 * @hostrcb:	hostrcb struct
 * @cfg:		fabric path element struct
 *
 * Return value:
 * 	none
 **/
static void ipr_log_path_elem(struct ipr_hostrcb *hostrcb,
			      struct ipr_hostrcb_config_element *cfg)
{
	int i, j;
	u8 type = cfg->type_status & IPR_PATH_CFG_TYPE_MASK;
	u8 status = cfg->type_status & IPR_PATH_CFG_STATUS_MASK;

	if (type == IPR_PATH_CFG_NOT_EXIST)
		return;

	for (i = 0; i < ARRAY_SIZE(path_type_desc); i++) {
		if (path_type_desc[i].type != type)
			continue;

		for (j = 0; j < ARRAY_SIZE(path_status_desc); j++) {
			if (path_status_desc[j].status != status)
				continue;

			if (type == IPR_PATH_CFG_IOA_PORT) {
				ipr_hcam_err(hostrcb, "%s %s: Phy=%d, Link rate=%s, WWN=%08X%08X\n",
					     path_status_desc[j].desc, path_type_desc[i].desc,
					     cfg->phy, link_rate[cfg->link_rate & IPR_PHY_LINK_RATE_MASK],
					     be32_to_cpu(cfg->wwid[0]), be32_to_cpu(cfg->wwid[1]));
			} else {
				if (cfg->cascaded_expander == 0xff && cfg->phy == 0xff) {
					ipr_hcam_err(hostrcb, "%s %s: Link rate=%s, WWN=%08X%08X\n",
						     path_status_desc[j].desc, path_type_desc[i].desc,
						     link_rate[cfg->link_rate & IPR_PHY_LINK_RATE_MASK],
						     be32_to_cpu(cfg->wwid[0]), be32_to_cpu(cfg->wwid[1]));
				} else if (cfg->cascaded_expander == 0xff) {
					ipr_hcam_err(hostrcb, "%s %s: Phy=%d, Link rate=%s, "
						     "WWN=%08X%08X\n", path_status_desc[j].desc,
						     path_type_desc[i].desc, cfg->phy,
						     link_rate[cfg->link_rate & IPR_PHY_LINK_RATE_MASK],
						     be32_to_cpu(cfg->wwid[0]), be32_to_cpu(cfg->wwid[1]));
				} else if (cfg->phy == 0xff) {
					ipr_hcam_err(hostrcb, "%s %s: Cascade=%d, Link rate=%s, "
						     "WWN=%08X%08X\n", path_status_desc[j].desc,
						     path_type_desc[i].desc, cfg->cascaded_expander,
						     link_rate[cfg->link_rate & IPR_PHY_LINK_RATE_MASK],
						     be32_to_cpu(cfg->wwid[0]), be32_to_cpu(cfg->wwid[1]));
				} else {
					ipr_hcam_err(hostrcb, "%s %s: Cascade=%d, Phy=%d, Link rate=%s "
						     "WWN=%08X%08X\n", path_status_desc[j].desc,
						     path_type_desc[i].desc, cfg->cascaded_expander, cfg->phy,
						     link_rate[cfg->link_rate & IPR_PHY_LINK_RATE_MASK],
						     be32_to_cpu(cfg->wwid[0]), be32_to_cpu(cfg->wwid[1]));
				}
			}
			return;
		}
	}

	ipr_hcam_err(hostrcb, "Path element=%02X: Cascade=%d Phy=%d Link rate=%s "
		     "WWN=%08X%08X\n", cfg->type_status, cfg->cascaded_expander, cfg->phy,
		     link_rate[cfg->link_rate & IPR_PHY_LINK_RATE_MASK],
		     be32_to_cpu(cfg->wwid[0]), be32_to_cpu(cfg->wwid[1]));
}

/**
 * ipr_log_fabric_error - Log a fabric error.
 * @ioa_cfg:	ioa config struct
 * @hostrcb:	hostrcb struct
 *
 * Return value:
 * 	none
 **/
static void ipr_log_fabric_error(struct ipr_ioa_cfg *ioa_cfg,
				 struct ipr_hostrcb *hostrcb)
{
	struct ipr_hostrcb_type_20_error *error;
	struct ipr_hostrcb_fabric_desc *fabric;
	struct ipr_hostrcb_config_element *cfg;
	int i, add_len;

	error = &hostrcb->hcam.u.error.u.type_20_error;
	error->failure_reason[sizeof(error->failure_reason) - 1] = '\0';
	ipr_hcam_err(hostrcb, "%s\n", error->failure_reason);

	add_len = be32_to_cpu(hostrcb->hcam.length) -
		(offsetof(struct ipr_hostrcb_error, u) +
		 offsetof(struct ipr_hostrcb_type_20_error, desc));

	for (i = 0, fabric = error->desc; i < error->num_entries; i++) {
		ipr_log_fabric_path(hostrcb, fabric);
		for_each_fabric_cfg(fabric, cfg)
			ipr_log_path_elem(hostrcb, cfg);

		add_len -= be16_to_cpu(fabric->length);
		fabric = (struct ipr_hostrcb_fabric_desc *)
			((unsigned long)fabric + be16_to_cpu(fabric->length));
	}

	ipr_log_hex_data(ioa_cfg, (u32 *)fabric, add_len);
}

/**
 * ipr_log_generic_error - Log an adapter error.
 * @ioa_cfg:	ioa config struct
 * @hostrcb:	hostrcb struct
 *
 * Return value:
 * 	none
 **/
static void ipr_log_generic_error(struct ipr_ioa_cfg *ioa_cfg,
				  struct ipr_hostrcb *hostrcb)
{
	ipr_log_hex_data(ioa_cfg, hostrcb->hcam.u.raw.data,
			 be32_to_cpu(hostrcb->hcam.length));
}

/**
 * ipr_get_error - Find the specfied IOASC in the ipr_error_table.
 * @ioasc:	IOASC
 *
 * This function will return the index of into the ipr_error_table
 * for the specified IOASC. If the IOASC is not in the table,
 * 0 will be returned, which points to the entry used for unknown errors.
 *
 * Return value:
 * 	index into the ipr_error_table
 **/
static u32 ipr_get_error(u32 ioasc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ipr_error_table); i++)
		if (ipr_error_table[i].ioasc == (ioasc & IPR_IOASC_IOASC_MASK))
			return i;

	return 0;
}

/**
 * ipr_handle_log_data - Log an adapter error.
 * @ioa_cfg:	ioa config struct
 * @hostrcb:	hostrcb struct
 *
 * This function logs an adapter error to the system.
 *
 * Return value:
 * 	none
 **/
static void ipr_handle_log_data(struct ipr_ioa_cfg *ioa_cfg,
				struct ipr_hostrcb *hostrcb)
{
	u32 ioasc;
	int error_index;

	if (hostrcb->hcam.notify_type != IPR_HOST_RCB_NOTIF_TYPE_ERROR_LOG_ENTRY)
		return;

	if (hostrcb->hcam.notifications_lost == IPR_HOST_RCB_NOTIFICATIONS_LOST)
		dev_err(&ioa_cfg->pdev->dev, "Error notifications lost\n");

	ioasc = be32_to_cpu(hostrcb->hcam.u.error.failing_dev_ioasc);

	if (ioasc == IPR_IOASC_BUS_WAS_RESET ||
	    ioasc == IPR_IOASC_BUS_WAS_RESET_BY_OTHER) {
		/* Tell the midlayer we had a bus reset so it will handle the UA properly */
		scsi_report_bus_reset(ioa_cfg->host,
				      hostrcb->hcam.u.error.failing_dev_res_addr.bus);
	}

	error_index = ipr_get_error(ioasc);

	if (!ipr_error_table[error_index].log_hcam)
		return;

	ipr_hcam_err(hostrcb, "%s\n", ipr_error_table[error_index].error);

	/* Set indication we have logged an error */
	ioa_cfg->errors_logged++;

	if (ioa_cfg->log_level < ipr_error_table[error_index].log_hcam)
		return;
	if (be32_to_cpu(hostrcb->hcam.length) > sizeof(hostrcb->hcam.u.raw))
		hostrcb->hcam.length = cpu_to_be32(sizeof(hostrcb->hcam.u.raw));

	switch (hostrcb->hcam.overlay_id) {
	case IPR_HOST_RCB_OVERLAY_ID_2:
		ipr_log_cache_error(ioa_cfg, hostrcb);
		break;
	case IPR_HOST_RCB_OVERLAY_ID_3:
		ipr_log_config_error(ioa_cfg, hostrcb);
		break;
	case IPR_HOST_RCB_OVERLAY_ID_4:
	case IPR_HOST_RCB_OVERLAY_ID_6:
		ipr_log_array_error(ioa_cfg, hostrcb);
		break;
	case IPR_HOST_RCB_OVERLAY_ID_7:
		ipr_log_dual_ioa_error(ioa_cfg, hostrcb);
		break;
	case IPR_HOST_RCB_OVERLAY_ID_12:
		ipr_log_enhanced_cache_error(ioa_cfg, hostrcb);
		break;
	case IPR_HOST_RCB_OVERLAY_ID_13:
		ipr_log_enhanced_config_error(ioa_cfg, hostrcb);
		break;
	case IPR_HOST_RCB_OVERLAY_ID_14:
	case IPR_HOST_RCB_OVERLAY_ID_16:
		ipr_log_enhanced_array_error(ioa_cfg, hostrcb);
		break;
	case IPR_HOST_RCB_OVERLAY_ID_17:
		ipr_log_enhanced_dual_ioa_error(ioa_cfg, hostrcb);
		break;
	case IPR_HOST_RCB_OVERLAY_ID_20:
		ipr_log_fabric_error(ioa_cfg, hostrcb);
		break;
	case IPR_HOST_RCB_OVERLAY_ID_1:
	case IPR_HOST_RCB_OVERLAY_ID_DEFAULT:
	default:
		ipr_log_generic_error(ioa_cfg, hostrcb);
		break;
	}
}

/**
 * ipr_process_error - Op done function for an adapter error log.
 * @ipr_cmd:	ipr command struct
 *
 * This function is the op done function for an error log host
 * controlled async from the adapter. It will log the error and
 * send the HCAM back to the adapter.
 *
 * Return value:
 * 	none
 **/
static void ipr_process_error(struct ipr_cmnd *ipr_cmd)
{
	struct ipr_ioa_cfg *ioa_cfg = ipr_cmd->ioa_cfg;
	struct ipr_hostrcb *hostrcb = ipr_cmd->u.hostrcb;
	u32 ioasc = be32_to_cpu(ipr_cmd->ioasa.ioasc);
	u32 fd_ioasc = be32_to_cpu(hostrcb->hcam.u.error.failing_dev_ioasc);

	list_del(&hostrcb->queue);
	list_add_tail(&ipr_cmd->queue, &ioa_cfg->free_q);

	if (!ioasc) {
		ipr_handle_log_data(ioa_cfg, hostrcb);
		if (fd_ioasc == IPR_IOASC_NR_IOA_RESET_REQUIRED)
			ipr_initiate_ioa_reset(ioa_cfg, IPR_SHUTDOWN_ABBREV);
	} else if (ioasc != IPR_IOASC_IOA_WAS_RESET) {
		dev_err(&ioa_cfg->pdev->dev,
			"Host RCB failed with IOASC: 0x%08X\n", ioasc);
	}

	ipr_send_hcam(ioa_cfg, IPR_HCAM_CDB_OP_CODE_LOG_DATA, hostrcb);
}

/**
 * ipr_timeout -  An internally generated op has timed out.
 * @ipr_cmd:	ipr command struct
 *
 * This function blocks host requests and initiates an
 * adapter reset.
 *
 * Return value:
 * 	none
 **/
static void ipr_timeout(struct ipr_cmnd *ipr_cmd)
{
	unsigned long lock_flags = 0;
	struct ipr_ioa_cfg *ioa_cfg = ipr_cmd->ioa_cfg;

	ENTER;
	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);

	ioa_cfg->errors_logged++;
	dev_err(&ioa_cfg->pdev->dev,
		"Adapter being reset due to command timeout.\n");

	if (WAIT_FOR_DUMP == ioa_cfg->sdt_state)
		ioa_cfg->sdt_state = GET_DUMP;

	if (!ioa_cfg->in_reset_reload || ioa_cfg->reset_cmd == ipr_cmd)
		ipr_initiate_ioa_reset(ioa_cfg, IPR_SHUTDOWN_NONE);

	spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
	LEAVE;
}

/**
 * ipr_oper_timeout -  Adapter timed out transitioning to operational
 * @ipr_cmd:	ipr command struct
 *
 * This function blocks host requests and initiates an
 * adapter reset.
 *
 * Return value:
 * 	none
 **/
static void ipr_oper_timeout(struct ipr_cmnd *ipr_cmd)
{
	unsigned long lock_flags = 0;
	struct ipr_ioa_cfg *ioa_cfg = ipr_cmd->ioa_cfg;

	ENTER;
	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);

	ioa_cfg->errors_logged++;
	dev_err(&ioa_cfg->pdev->dev,
		"Adapter timed out transitioning to operational.\n");

	if (WAIT_FOR_DUMP == ioa_cfg->sdt_state)
		ioa_cfg->sdt_state = GET_DUMP;

	if (!ioa_cfg->in_reset_reload || ioa_cfg->reset_cmd == ipr_cmd) {
		if (ipr_fastfail)
			ioa_cfg->reset_retries += IPR_NUM_RESET_RELOAD_RETRIES;
		ipr_initiate_ioa_reset(ioa_cfg, IPR_SHUTDOWN_NONE);
	}

	spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
	LEAVE;
}

/**
 * ipr_reset_reload - Reset/Reload the IOA
 * @ioa_cfg:		ioa config struct
 * @shutdown_type:	shutdown type
 *
 * This function resets the adapter and re-initializes it.
 * This function assumes that all new host commands have been stopped.
 * Return value:
 * 	SUCCESS / FAILED
 **/
static int ipr_reset_reload(struct ipr_ioa_cfg *ioa_cfg,
			    enum ipr_shutdown_type shutdown_type)
{
	if (!ioa_cfg->in_reset_reload)
		ipr_initiate_ioa_reset(ioa_cfg, shutdown_type);

	spin_unlock_irq(ioa_cfg->host->host_lock);
	wait_event(ioa_cfg->reset_wait_q, !ioa_cfg->in_reset_reload);
	spin_lock_irq(ioa_cfg->host->host_lock);

	/* If we got hit with a host reset while we were already resetting
	 the adapter for some reason, and the reset failed. */
	if (ioa_cfg->ioa_is_dead) {
		ipr_trace;
		return FAILED;
	}

	return SUCCESS;
}

/**
 * ipr_find_ses_entry - Find matching SES in SES table
 * @res:	resource entry struct of SES
 *
 * Return value:
 * 	pointer to SES table entry / NULL on failure
 **/
static const struct ipr_ses_table_entry *
ipr_find_ses_entry(struct ipr_resource_entry *res)
{
	int i, j, matches;
	const struct ipr_ses_table_entry *ste = ipr_ses_table;

	for (i = 0; i < ARRAY_SIZE(ipr_ses_table); i++, ste++) {
		for (j = 0, matches = 0; j < IPR_PROD_ID_LEN; j++) {
			if (ste->compare_product_id_byte[j] == 'X') {
				if (res->cfgte.std_inq_data.vpids.product_id[j] == ste->product_id[j])
					matches++;
				else
					break;
			} else
				matches++;
		}

		if (matches == IPR_PROD_ID_LEN)
			return ste;
	}

	return NULL;
}

/**
 * ipr_get_max_scsi_speed - Determine max SCSI speed for a given bus
 * @ioa_cfg:	ioa config struct
 * @bus:		SCSI bus
 * @bus_width:	bus width
 *
 * Return value:
 *	SCSI bus speed in units of 100KHz, 1600 is 160 MHz
 *	For a 2-byte wide SCSI bus, the maximum transfer speed is
 *	twice the maximum transfer rate (e.g. for a wide enabled bus,
 *	max 160MHz = max 320MB/sec).
 **/
static u32 ipr_get_max_scsi_speed(struct ipr_ioa_cfg *ioa_cfg, u8 bus, u8 bus_width)
{
	struct ipr_resource_entry *res;
	const struct ipr_ses_table_entry *ste;
	u32 max_xfer_rate = IPR_MAX_SCSI_RATE(bus_width);

	/* Loop through each config table entry in the config table buffer */
	list_for_each_entry(res, &ioa_cfg->used_res_q, queue) {
		if (!(IPR_IS_SES_DEVICE(res->cfgte.std_inq_data)))
			continue;

		if (bus != res->cfgte.res_addr.bus)
			continue;

		if (!(ste = ipr_find_ses_entry(res)))
			continue;

		max_xfer_rate = (ste->max_bus_speed_limit * 10) / (bus_width / 8);
	}

	return max_xfer_rate;
}

/**
 * ipr_wait_iodbg_ack - Wait for an IODEBUG ACK from the IOA
 * @ioa_cfg:		ioa config struct
 * @max_delay:		max delay in micro-seconds to wait
 *
 * Waits for an IODEBUG ACK from the IOA, doing busy looping.
 *
 * Return value:
 * 	0 on success / other on failure
 **/
static int ipr_wait_iodbg_ack(struct ipr_ioa_cfg *ioa_cfg, int max_delay)
{
	volatile u32 pcii_reg;
	int delay = 1;

	/* Read interrupt reg until IOA signals IO Debug Acknowledge */
	while (delay < max_delay) {
		pcii_reg = readl(ioa_cfg->regs.sense_interrupt_reg);

		if (pcii_reg & IPR_PCII_IO_DEBUG_ACKNOWLEDGE)
			return 0;

		/* udelay cannot be used if delay is more than a few milliseconds */
		if ((delay / 1000) > MAX_UDELAY_MS)
			mdelay(delay / 1000);
		else
			udelay(delay);

		delay += delay;
	}
	return -EIO;
}

/**
 * ipr_get_ldump_data_section - Dump IOA memory
 * @ioa_cfg:			ioa config struct
 * @start_addr:			adapter address to dump
 * @dest:				destination kernel buffer
 * @length_in_words:	length to dump in 4 byte words
 *
 * Return value:
 * 	0 on success / -EIO on failure
 **/
static int ipr_get_ldump_data_section(struct ipr_ioa_cfg *ioa_cfg,
				      u32 start_addr,
				      __be32 *dest, u32 length_in_words)
{
	volatile u32 temp_pcii_reg;
	int i, delay = 0;

	/* Write IOA interrupt reg starting LDUMP state  */
	writel((IPR_UPROCI_RESET_ALERT | IPR_UPROCI_IO_DEBUG_ALERT),
	       ioa_cfg->regs.set_uproc_interrupt_reg);

	/* Wait for IO debug acknowledge */
	if (ipr_wait_iodbg_ack(ioa_cfg,
			       IPR_LDUMP_MAX_LONG_ACK_DELAY_IN_USEC)) {
		dev_err(&ioa_cfg->pdev->dev,
			"IOA dump long data transfer timeout\n");
		return -EIO;
	}

	/* Signal LDUMP interlocked - clear IO debug ack */
	writel(IPR_PCII_IO_DEBUG_ACKNOWLEDGE,
	       ioa_cfg->regs.clr_interrupt_reg);

	/* Write Mailbox with starting address */
	writel(start_addr, ioa_cfg->ioa_mailbox);

	/* Signal address valid - clear IOA Reset alert */
	writel(IPR_UPROCI_RESET_ALERT,
	       ioa_cfg->regs.clr_uproc_interrupt_reg);

	for (i = 0; i < length_in_words; i++) {
		/* Wait for IO debug acknowledge */
		if (ipr_wait_iodbg_ack(ioa_cfg,
				       IPR_LDUMP_MAX_SHORT_ACK_DELAY_IN_USEC)) {
			dev_err(&ioa_cfg->pdev->dev,
				"IOA dump short data transfer timeout\n");
			return -EIO;
		}

		/* Read data from mailbox and increment destination pointer */
		*dest = cpu_to_be32(readl(ioa_cfg->ioa_mailbox));
		dest++;

		/* For all but the last word of data, signal data received */
		if (i < (length_in_words - 1)) {
			/* Signal dump data received - Clear IO debug Ack */
			writel(IPR_PCII_IO_DEBUG_ACKNOWLEDGE,
			       ioa_cfg->regs.clr_interrupt_reg);
		}
	}

	/* Signal end of block transfer. Set reset alert then clear IO debug ack */
	writel(IPR_UPROCI_RESET_ALERT,
	       ioa_cfg->regs.set_uproc_interrupt_reg);

	writel(IPR_UPROCI_IO_DEBUG_ALERT,
	       ioa_cfg->regs.clr_uproc_interrupt_reg);

	/* Signal dump data received - Clear IO debug Ack */
	writel(IPR_PCII_IO_DEBUG_ACKNOWLEDGE,
	       ioa_cfg->regs.clr_interrupt_reg);

	/* Wait for IOA to signal LDUMP exit - IOA reset alert will be cleared */
	while (delay < IPR_LDUMP_MAX_SHORT_ACK_DELAY_IN_USEC) {
		temp_pcii_reg =
		    readl(ioa_cfg->regs.sense_uproc_interrupt_reg);

		if (!(temp_pcii_reg & IPR_UPROCI_RESET_ALERT))
			return 0;

		udelay(10);
		delay += 10;
	}

	return 0;
}

#ifdef CONFIG_SCSI_IPR_DUMP
/**
 * ipr_sdt_copy - Copy Smart Dump Table to kernel buffer
 * @ioa_cfg:		ioa config struct
 * @pci_address:	adapter address
 * @length:			length of data to copy
 *
 * Copy data from PCI adapter to kernel buffer.
 * Note: length MUST be a 4 byte multiple
 * Return value:
 * 	0 on success / other on failure
 **/
static int ipr_sdt_copy(struct ipr_ioa_cfg *ioa_cfg,
			unsigned long pci_address, u32 length)
{
	int bytes_copied = 0;
	int cur_len, rc, rem_len, rem_page_len;
	__be32 *page;
	unsigned long lock_flags = 0;
	struct ipr_ioa_dump *ioa_dump = &ioa_cfg->dump->ioa_dump;

	while (bytes_copied < length &&
	       (ioa_dump->hdr.len + bytes_copied) < IPR_MAX_IOA_DUMP_SIZE) {
		if (ioa_dump->page_offset >= PAGE_SIZE ||
		    ioa_dump->page_offset == 0) {
			page = (__be32 *)__get_free_page(GFP_ATOMIC);

			if (!page) {
				ipr_trace;
				return bytes_copied;
			}

			ioa_dump->page_offset = 0;
			ioa_dump->ioa_data[ioa_dump->next_page_index] = page;
			ioa_dump->next_page_index++;
		} else
			page = ioa_dump->ioa_data[ioa_dump->next_page_index - 1];

		rem_len = length - bytes_copied;
		rem_page_len = PAGE_SIZE - ioa_dump->page_offset;
		cur_len = min(rem_len, rem_page_len);

		spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
		if (ioa_cfg->sdt_state == ABORT_DUMP) {
			rc = -EIO;
		} else {
			rc = ipr_get_ldump_data_section(ioa_cfg,
							pci_address + bytes_copied,
							&page[ioa_dump->page_offset / 4],
							(cur_len / sizeof(u32)));
		}
		spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);

		if (!rc) {
			ioa_dump->page_offset += cur_len;
			bytes_copied += cur_len;
		} else {
			ipr_trace;
			break;
		}
		schedule();
	}

	return bytes_copied;
}

/**
 * ipr_init_dump_entry_hdr - Initialize a dump entry header.
 * @hdr:	dump entry header struct
 *
 * Return value:
 * 	nothing
 **/
static void ipr_init_dump_entry_hdr(struct ipr_dump_entry_header *hdr)
{
	hdr->eye_catcher = IPR_DUMP_EYE_CATCHER;
	hdr->num_elems = 1;
	hdr->offset = sizeof(*hdr);
	hdr->status = IPR_DUMP_STATUS_SUCCESS;
}

/**
 * ipr_dump_ioa_type_data - Fill in the adapter type in the dump.
 * @ioa_cfg:	ioa config struct
 * @driver_dump:	driver dump struct
 *
 * Return value:
 * 	nothing
 **/
static void ipr_dump_ioa_type_data(struct ipr_ioa_cfg *ioa_cfg,
				   struct ipr_driver_dump *driver_dump)
{
	struct ipr_inquiry_page3 *ucode_vpd = &ioa_cfg->vpd_cbs->page3_data;

	ipr_init_dump_entry_hdr(&driver_dump->ioa_type_entry.hdr);
	driver_dump->ioa_type_entry.hdr.len =
		sizeof(struct ipr_dump_ioa_type_entry) -
		sizeof(struct ipr_dump_entry_header);
	driver_dump->ioa_type_entry.hdr.data_type = IPR_DUMP_DATA_TYPE_BINARY;
	driver_dump->ioa_type_entry.hdr.id = IPR_DUMP_DRIVER_TYPE_ID;
	driver_dump->ioa_type_entry.type = ioa_cfg->type;
	driver_dump->ioa_type_entry.fw_version = (ucode_vpd->major_release << 24) |
		(ucode_vpd->card_type << 16) | (ucode_vpd->minor_release[0] << 8) |
		ucode_vpd->minor_release[1];
	driver_dump->hdr.num_entries++;
}

/**
 * ipr_dump_version_data - Fill in the driver version in the dump.
 * @ioa_cfg:	ioa config struct
 * @driver_dump:	driver dump struct
 *
 * Return value:
 * 	nothing
 **/
static void ipr_dump_version_data(struct ipr_ioa_cfg *ioa_cfg,
				  struct ipr_driver_dump *driver_dump)
{
	ipr_init_dump_entry_hdr(&driver_dump->version_entry.hdr);
	driver_dump->version_entry.hdr.len =
		sizeof(struct ipr_dump_version_entry) -
		sizeof(struct ipr_dump_entry_header);
	driver_dump->version_entry.hdr.data_type = IPR_DUMP_DATA_TYPE_ASCII;
	driver_dump->version_entry.hdr.id = IPR_DUMP_DRIVER_VERSION_ID;
	strcpy(driver_dump->version_entry.version, IPR_DRIVER_VERSION);
	driver_dump->hdr.num_entries++;
}

/**
 * ipr_dump_trace_data - Fill in the IOA trace in the dump.
 * @ioa_cfg:	ioa config struct
 * @driver_dump:	driver dump struct
 *
 * Return value:
 * 	nothing
 **/
static void ipr_dump_trace_data(struct ipr_ioa_cfg *ioa_cfg,
				   struct ipr_driver_dump *driver_dump)
{
	ipr_init_dump_entry_hdr(&driver_dump->trace_entry.hdr);
	driver_dump->trace_entry.hdr.len =
		sizeof(struct ipr_dump_trace_entry) -
		sizeof(struct ipr_dump_entry_header);
	driver_dump->trace_entry.hdr.data_type = IPR_DUMP_DATA_TYPE_BINARY;
	driver_dump->trace_entry.hdr.id = IPR_DUMP_TRACE_ID;
	memcpy(driver_dump->trace_entry.trace, ioa_cfg->trace, IPR_TRACE_SIZE);
	driver_dump->hdr.num_entries++;
}

/**
 * ipr_dump_location_data - Fill in the IOA location in the dump.
 * @ioa_cfg:	ioa config struct
 * @driver_dump:	driver dump struct
 *
 * Return value:
 * 	nothing
 **/
static void ipr_dump_location_data(struct ipr_ioa_cfg *ioa_cfg,
				   struct ipr_driver_dump *driver_dump)
{
	ipr_init_dump_entry_hdr(&driver_dump->location_entry.hdr);
	driver_dump->location_entry.hdr.len =
		sizeof(struct ipr_dump_location_entry) -
		sizeof(struct ipr_dump_entry_header);
	driver_dump->location_entry.hdr.data_type = IPR_DUMP_DATA_TYPE_ASCII;
	driver_dump->location_entry.hdr.id = IPR_DUMP_LOCATION_ID;
	strcpy(driver_dump->location_entry.location, dev_name(&ioa_cfg->pdev->dev));
	driver_dump->hdr.num_entries++;
}

/**
 * ipr_get_ioa_dump - Perform a dump of the driver and adapter.
 * @ioa_cfg:	ioa config struct
 * @dump:		dump struct
 *
 * Return value:
 * 	nothing
 **/
static void ipr_get_ioa_dump(struct ipr_ioa_cfg *ioa_cfg, struct ipr_dump *dump)
{
	unsigned long start_addr, sdt_word;
	unsigned long lock_flags = 0;
	struct ipr_driver_dump *driver_dump = &dump->driver_dump;
	struct ipr_ioa_dump *ioa_dump = &dump->ioa_dump;
	u32 num_entries, start_off, end_off;
	u32 bytes_to_copy, bytes_copied, rc;
	struct ipr_sdt *sdt;
	int i;

	ENTER;

	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);

	if (ioa_cfg->sdt_state != GET_DUMP) {
		spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
		return;
	}

	start_addr = readl(ioa_cfg->ioa_mailbox);

	if (!ipr_sdt_is_fmt2(start_addr)) {
		dev_err(&ioa_cfg->pdev->dev,
			"Invalid dump table format: %lx\n", start_addr);
		spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
		return;
	}

	dev_err(&ioa_cfg->pdev->dev, "Dump of IOA initiated\n");

	driver_dump->hdr.eye_catcher = IPR_DUMP_EYE_CATCHER;

	/* Initialize the overall dump header */
	driver_dump->hdr.len = sizeof(struct ipr_driver_dump);
	driver_dump->hdr.num_entries = 1;
	driver_dump->hdr.first_entry_offset = sizeof(struct ipr_dump_header);
	driver_dump->hdr.status = IPR_DUMP_STATUS_SUCCESS;
	driver_dump->hdr.os = IPR_DUMP_OS_LINUX;
	driver_dump->hdr.driver_name = IPR_DUMP_DRIVER_NAME;

	ipr_dump_version_data(ioa_cfg, driver_dump);
	ipr_dump_location_data(ioa_cfg, driver_dump);
	ipr_dump_ioa_type_data(ioa_cfg, driver_dump);
	ipr_dump_trace_data(ioa_cfg, driver_dump);

	/* Update dump_header */
	driver_dump->hdr.len += sizeof(struct ipr_dump_entry_header);

	/* IOA Dump entry */
	ipr_init_dump_entry_hdr(&ioa_dump->hdr);
	ioa_dump->format = IPR_SDT_FMT2;
	ioa_dump->hdr.len = 0;
	ioa_dump->hdr.data_type = IPR_DUMP_DATA_TYPE_BINARY;
	ioa_dump->hdr.id = IPR_DUMP_IOA_DUMP_ID;

	/* First entries in sdt are actually a list of dump addresses and
	 lengths to gather the real dump data.  sdt represents the pointer
	 to the ioa generated dump table.  Dump data will be extracted based
	 on entries in this table */
	sdt = &ioa_dump->sdt;

	rc = ipr_get_ldump_data_section(ioa_cfg, start_addr, (__be32 *)sdt,
					sizeof(struct ipr_sdt) / sizeof(__be32));

	/* Smart Dump table is ready to use and the first entry is valid */
	if (rc || (be32_to_cpu(sdt->hdr.state) != IPR_FMT2_SDT_READY_TO_USE)) {
		dev_err(&ioa_cfg->pdev->dev,
			"Dump of IOA failed. Dump table not valid: %d, %X.\n",
			rc, be32_to_cpu(sdt->hdr.state));
		driver_dump->hdr.status = IPR_DUMP_STATUS_FAILED;
		ioa_cfg->sdt_state = DUMP_OBTAINED;
		spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
		return;
	}

	num_entries = be32_to_cpu(sdt->hdr.num_entries_used);

	if (num_entries > IPR_NUM_SDT_ENTRIES)
		num_entries = IPR_NUM_SDT_ENTRIES;

	spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);

	for (i = 0; i < num_entries; i++) {
		if (ioa_dump->hdr.len > IPR_MAX_IOA_DUMP_SIZE) {
			driver_dump->hdr.status = IPR_DUMP_STATUS_QUAL_SUCCESS;
			break;
		}

		if (sdt->entry[i].flags & IPR_SDT_VALID_ENTRY) {
			sdt_word = be32_to_cpu(sdt->entry[i].bar_str_offset);
			start_off = sdt_word & IPR_FMT2_MBX_ADDR_MASK;
			end_off = be32_to_cpu(sdt->entry[i].end_offset);

			if (ipr_sdt_is_fmt2(sdt_word) && sdt_word) {
				bytes_to_copy = end_off - start_off;
				if (bytes_to_copy > IPR_MAX_IOA_DUMP_SIZE) {
					sdt->entry[i].flags &= ~IPR_SDT_VALID_ENTRY;
					continue;
				}

				/* Copy data from adapter to driver buffers */
				bytes_copied = ipr_sdt_copy(ioa_cfg, sdt_word,
							    bytes_to_copy);

				ioa_dump->hdr.len += bytes_copied;

				if (bytes_copied != bytes_to_copy) {
					driver_dump->hdr.status = IPR_DUMP_STATUS_QUAL_SUCCESS;
					break;
				}
			}
		}
	}

	dev_err(&ioa_cfg->pdev->dev, "Dump of IOA completed.\n");

	/* Update dump_header */
	driver_dump->hdr.len += ioa_dump->hdr.len;
	wmb();
	ioa_cfg->sdt_state = DUMP_OBTAINED;
	LEAVE;
}

#else
#define ipr_get_ioa_dump(ioa_cfg, dump) do { } while(0)
#endif

/**
 * ipr_release_dump - Free adapter dump memory
 * @kref:	kref struct
 *
 * Return value:
 *	nothing
 **/
static void ipr_release_dump(struct kref *kref)
{
	struct ipr_dump *dump = container_of(kref,struct ipr_dump,kref);
	struct ipr_ioa_cfg *ioa_cfg = dump->ioa_cfg;
	unsigned long lock_flags = 0;
	int i;

	ENTER;
	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
	ioa_cfg->dump = NULL;
	ioa_cfg->sdt_state = INACTIVE;
	spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);

	for (i = 0; i < dump->ioa_dump.next_page_index; i++)
		free_page((unsigned long) dump->ioa_dump.ioa_data[i]);

	kfree(dump);
	LEAVE;
}

/**
 * ipr_worker_thread - Worker thread
 * @work:		ioa config struct
 *
 * Called at task level from a work thread. This function takes care
 * of adding and removing device from the mid-layer as configuration
 * changes are detected by the adapter.
 *
 * Return value:
 * 	nothing
 **/
static void ipr_worker_thread(struct work_struct *work)
{
	unsigned long lock_flags;
	struct ipr_resource_entry *res;
	struct scsi_device *sdev;
	struct ipr_dump *dump;
	struct ipr_ioa_cfg *ioa_cfg =
		container_of(work, struct ipr_ioa_cfg, work_q);
	u8 bus, target, lun;
	int did_work;

	ENTER;
	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);

	if (ioa_cfg->sdt_state == GET_DUMP) {
		dump = ioa_cfg->dump;
		if (!dump) {
			spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
			return;
		}
		kref_get(&dump->kref);
		spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
		ipr_get_ioa_dump(ioa_cfg, dump);
		kref_put(&dump->kref, ipr_release_dump);

		spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
		if (ioa_cfg->sdt_state == DUMP_OBTAINED)
			ipr_initiate_ioa_reset(ioa_cfg, IPR_SHUTDOWN_NONE);
		spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
		return;
	}

restart:
	do {
		did_work = 0;
		if (!ioa_cfg->allow_cmds || !ioa_cfg->allow_ml_add_del) {
			spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
			return;
		}

		list_for_each_entry(res, &ioa_cfg->used_res_q, queue) {
			if (res->del_from_ml && res->sdev) {
				did_work = 1;
				sdev = res->sdev;
				if (!scsi_device_get(sdev)) {
					list_move_tail(&res->queue, &ioa_cfg->free_res_q);
					spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
					scsi_remove_device(sdev);
					scsi_device_put(sdev);
					spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
				}
				break;
			}
		}
	} while(did_work);

	list_for_each_entry(res, &ioa_cfg->used_res_q, queue) {
		if (res->add_to_ml) {
			bus = res->cfgte.res_addr.bus;
			target = res->cfgte.res_addr.target;
			lun = res->cfgte.res_addr.lun;
			res->add_to_ml = 0;
			spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
			scsi_add_device(ioa_cfg->host, bus, target, lun);
			spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
			goto restart;
		}
	}

	spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
	kobject_uevent(&ioa_cfg->host->shost_dev.kobj, KOBJ_CHANGE);
	LEAVE;
}

#ifdef CONFIG_SCSI_IPR_TRACE
/**
 * ipr_read_trace - Dump the adapter trace
 * @kobj:		kobject struct
 * @bin_attr:		bin_attribute struct
 * @buf:		buffer
 * @off:		offset
 * @count:		buffer size
 *
 * Return value:
 *	number of bytes printed to buffer
 **/
static ssize_t ipr_read_trace(struct kobject *kobj,
			      struct bin_attribute *bin_attr,
			      char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ipr_ioa_cfg *ioa_cfg = (struct ipr_ioa_cfg *)shost->hostdata;
	unsigned long lock_flags = 0;
	ssize_t ret;

	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
	ret = memory_read_from_buffer(buf, count, &off, ioa_cfg->trace,
				IPR_TRACE_SIZE);
	spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);

	return ret;
}

static struct bin_attribute ipr_trace_attr = {
	.attr =	{
		.name = "trace",
		.mode = S_IRUGO,
	},
	.size = 0,
	.read = ipr_read_trace,
};
#endif

static const struct {
	enum ipr_cache_state state;
	char *name;
} cache_state [] = {
	{ CACHE_NONE, "none" },
	{ CACHE_DISABLED, "disabled" },
	{ CACHE_ENABLED, "enabled" }
};

/**
 * ipr_show_write_caching - Show the write caching attribute
 * @dev:	device struct
 * @buf:	buffer
 *
 * Return value:
 *	number of bytes printed to buffer
 **/
static ssize_t ipr_show_write_caching(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ipr_ioa_cfg *ioa_cfg = (struct ipr_ioa_cfg *)shost->hostdata;
	unsigned long lock_flags = 0;
	int i, len = 0;

	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
	for (i = 0; i < ARRAY_SIZE(cache_state); i++) {
		if (cache_state[i].state == ioa_cfg->cache_state) {
			len = snprintf(buf, PAGE_SIZE, "%s\n", cache_state[i].name);
			break;
		}
	}
	spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
	return len;
}


/**
 * ipr_store_write_caching - Enable/disable adapter write cache
 * @dev:	device struct
 * @buf:	buffer
 * @count:	buffer size
 *
 * This function will enable/disable adapter write cache.
 *
 * Return value:
 * 	count on success / other on failure
 **/
static ssize_t ipr_store_write_caching(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ipr_ioa_cfg *ioa_cfg = (struct ipr_ioa_cfg *)shost->hostdata;
	unsigned long lock_flags = 0;
	enum ipr_cache_state new_state = CACHE_INVALID;
	int i;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;
	if (ioa_cfg->cache_state == CACHE_NONE)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(cache_state); i++) {
		if (!strncmp(cache_state[i].name, buf, strlen(cache_state[i].name))) {
			new_state = cache_state[i].state;
			break;
		}
	}

	if (new_state != CACHE_DISABLED && new_state != CACHE_ENABLED)
		return -EINVAL;

	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
	if (ioa_cfg->cache_state == new_state) {
		spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
		return count;
	}

	ioa_cfg->cache_state = new_state;
	dev_info(&ioa_cfg->pdev->dev, "%s adapter write cache.\n",
		 new_state == CACHE_ENABLED ? "Enabling" : "Disabling");
	if (!ioa_cfg->in_reset_reload)
		ipr_initiate_ioa_reset(ioa_cfg, IPR_SHUTDOWN_NORMAL);
	spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
	wait_event(ioa_cfg->reset_wait_q, !ioa_cfg->in_reset_reload);

	return count;
}

static struct device_attribute ipr_ioa_cache_attr = {
	.attr = {
		.name =		"write_cache",
		.mode =		S_IRUGO | S_IWUSR,
	},
	.show = ipr_show_write_caching,
	.store = ipr_store_write_caching
};

/**
 * ipr_show_fw_version - Show the firmware version
 * @dev:	class device struct
 * @buf:	buffer
 *
 * Return value:
 *	number of bytes printed to buffer
 **/
static ssize_t ipr_show_fw_version(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ipr_ioa_cfg *ioa_cfg = (struct ipr_ioa_cfg *)shost->hostdata;
	struct ipr_inquiry_page3 *ucode_vpd = &ioa_cfg->vpd_cbs->page3_data;
	unsigned long lock_flags = 0;
	int len;

	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
	len = snprintf(buf, PAGE_SIZE, "%02X%02X%02X%02X\n",
		       ucode_vpd->major_release, ucode_vpd->card_type,
		       ucode_vpd->minor_release[0],
		       ucode_vpd->minor_release[1]);
	spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
	return len;
}

static struct device_attribute ipr_fw_version_attr = {
	.attr = {
		.name =		"fw_version",
		.mode =		S_IRUGO,
	},
	.show = ipr_show_fw_version,
};

/**
 * ipr_show_log_level - Show the adapter's error logging level
 * @dev:	class device struct
 * @buf:	buffer
 *
 * Return value:
 * 	number of bytes printed to buffer
 **/
static ssize_t ipr_show_log_level(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ipr_ioa_cfg *ioa_cfg = (struct ipr_ioa_cfg *)shost->hostdata;
	unsigned long lock_flags = 0;
	int len;

	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
	len = snprintf(buf, PAGE_SIZE, "%d\n", ioa_cfg->log_level);
	spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
	return len;
}

/**
 * ipr_store_log_level - Change the adapter's error logging level
 * @dev:	class device struct
 * @buf:	buffer
 *
 * Return value:
 * 	number of bytes printed to buffer
 **/
static ssize_t ipr_store_log_level(struct device *dev,
			           struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ipr_ioa_cfg *ioa_cfg = (struct ipr_ioa_cfg *)shost->hostdata;
	unsigned long lock_flags = 0;

	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
	ioa_cfg->log_level = simple_strtoul(buf, NULL, 10);
	spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
	return strlen(buf);
}

static struct device_attribute ipr_log_level_attr = {
	.attr = {
		.name =		"log_level",
		.mode =		S_IRUGO | S_IWUSR,
	},
	.show = ipr_show_log_level,
	.store = ipr_store_log_level
};

/**
 * ipr_store_diagnostics - IOA Diagnostics interface
 * @dev:	device struct
 * @buf:	buffer
 * @count:	buffer size
 *
 * This function will reset the adapter and wait a reasonable
 * amount of time for any errors that the adapter might log.
 *
 * Return value:
 * 	count on success / other on failure
 **/
static ssize_t ipr_store_diagnostics(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ipr_ioa_cfg *ioa_cfg = (struct ipr_ioa_cfg *)shost->hostdata;
	unsigned long lock_flags = 0;
	int rc = count;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
	while(ioa_cfg->in_reset_reload) {
		spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
		wait_event(ioa_cfg->reset_wait_q, !ioa_cfg->in_reset_reload);
		spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
	}

	ioa_cfg->errors_logged = 0;
	ipr_initiate_ioa_reset(ioa_cfg, IPR_SHUTDOWN_NORMAL);

	if (ioa_cfg->in_reset_reload) {
		spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
		wait_event(ioa_cfg->reset_wait_q, !ioa_cfg->in_reset_reload);

		/* Wait for a second for any errors to be logged */
		msleep(1000);
	} else {
		spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
		return -EIO;
	}

	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
	if (ioa_cfg->in_reset_reload || ioa_cfg->errors_logged)
		rc = -EIO;
	spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);

	return rc;
}

static struct device_attribute ipr_diagnostics_attr = {
	.attr = {
		.name =		"run_diagnostics",
		.mode =		S_IWUSR,
	},
	.store = ipr_store_diagnostics
};

/**
 * ipr_show_adapter_state - Show the adapter's state
 * @class_dev:	device struct
 * @buf:	buffer
 *
 * Return value:
 * 	number of bytes printed to buffer
 **/
static ssize_t ipr_show_adapter_state(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ipr_ioa_cfg *ioa_cfg = (struct ipr_ioa_cfg *)shost->hostdata;
	unsigned long lock_flags = 0;
	int len;

	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
	if (ioa_cfg->ioa_is_dead)
		len = snprintf(buf, PAGE_SIZE, "offline\n");
	else
		len = snprintf(buf, PAGE_SIZE, "online\n");
	spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
	return len;
}

/**
 * ipr_store_adapter_state - Change adapter state
 * @dev:	device struct
 * @buf:	buffer
 * @count:	buffer size
 *
 * This function will change the adapter's state.
 *
 * Return value:
 * 	count on success / other on failure
 **/
static ssize_t ipr_store_adapter_state(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ipr_ioa_cfg *ioa_cfg = (struct ipr_ioa_cfg *)shost->hostdata;
	unsigned long lock_flags;
	int result = count;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
	if (ioa_cfg->ioa_is_dead && !strncmp(buf, "online", 6)) {
		ioa_cfg->ioa_is_dead = 0;
		ioa_cfg->reset_retries = 0;
		ioa_cfg->in_ioa_bringdown = 0;
		ipr_initiate_ioa_reset(ioa_cfg, IPR_SHUTDOWN_NONE);
	}
	spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
	wait_event(ioa_cfg->reset_wait_q, !ioa_cfg->in_reset_reload);

	return result;
}

static struct device_attribute ipr_ioa_state_attr = {
	.attr = {
		.name =		"online_state",
		.mode =		S_IRUGO | S_IWUSR,
	},
	.show = ipr_show_adapter_state,
	.store = ipr_store_adapter_state
};

/**
 * ipr_store_reset_adapter - Reset the adapter
 * @dev:	device struct
 * @buf:	buffer
 * @count:	buffer size
 *
 * This function will reset the adapter.
 *
 * Return value:
 * 	count on success / other on failure
 **/
static ssize_t ipr_store_reset_adapter(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ipr_ioa_cfg *ioa_cfg = (struct ipr_ioa_cfg *)shost->hostdata;
	unsigned long lock_flags;
	int result = count;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
	if (!ioa_cfg->in_reset_reload)
		ipr_initiate_ioa_reset(ioa_cfg, IPR_SHUTDOWN_NORMAL);
	spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
	wait_event(ioa_cfg->reset_wait_q, !ioa_cfg->in_reset_reload);

	return result;
}

static struct device_attribute ipr_ioa_reset_attr = {
	.attr = {
		.name =		"reset_host",
		.mode =		S_IWUSR,
	},
	.store = ipr_store_reset_adapter
};

/**
 * ipr_alloc_ucode_buffer - Allocates a microcode download buffer
 * @buf_len:		buffer length
 *
 * Allocates a DMA'able buffer in chunks and assembles a scatter/gather
 * list to use for microcode download
 *
 * Return value:
 * 	pointer to sglist / NULL on failure
 **/
static struct ipr_sglist *ipr_alloc_ucode_buffer(int buf_len)
{
	int sg_size, order, bsize_elem, num_elem, i, j;
	struct ipr_sglist *sglist;
	struct scatterlist *scatterlist;
	struct page *page;

	/* Get the minimum size per scatter/gather element */
	sg_size = buf_len / (IPR_MAX_SGLIST - 1);

	/* Get the actual size per element */
	order = get_order(sg_size);

	/* Determine the actual number of bytes per element */
	bsize_elem = PAGE_SIZE * (1 << order);

	/* Determine the actual number of sg entries needed */
	if (buf_len % bsize_elem)
		num_elem = (buf_len / bsize_elem) + 1;
	else
		num_elem = buf_len / bsize_elem;

	/* Allocate a scatter/gather list for the DMA */
	sglist = kzalloc(sizeof(struct ipr_sglist) +
			 (sizeof(struct scatterlist) * (num_elem - 1)),
			 GFP_KERNEL);

	if (sglist == NULL) {
		ipr_trace;
		return NULL;
	}

	scatterlist = sglist->scatterlist;
	sg_init_table(scatterlist, num_elem);

	sglist->order = order;
	sglist->num_sg = num_elem;

	/* Allocate a bunch of sg elements */
	for (i = 0; i < num_elem; i++) {
		page = alloc_pages(GFP_KERNEL, order);
		if (!page) {
			ipr_trace;

			/* Free up what we already allocated */
			for (j = i - 1; j >= 0; j--)
				__free_pages(sg_page(&scatterlist[j]), order);
			kfree(sglist);
			return NULL;
		}

		sg_set_page(&scatterlist[i], page, 0, 0);
	}

	return sglist;
}

/**
 * ipr_free_ucode_buffer - Frees a microcode download buffer
 * @p_dnld:		scatter/gather list pointer
 *
 * Free a DMA'able ucode download buffer previously allocated with
 * ipr_alloc_ucode_buffer
 *
 * Return value:
 * 	nothing
 **/
static void ipr_free_ucode_buffer(struct ipr_sglist *sglist)
{
	int i;

	for (i = 0; i < sglist->num_sg; i++)
		__free_pages(sg_page(&sglist->scatterlist[i]), sglist->order);

	kfree(sglist);
}

/**
 * ipr_copy_ucode_buffer - Copy user buffer to kernel buffer
 * @sglist:		scatter/gather list pointer
 * @buffer:		buffer pointer
 * @len:		buffer length
 *
 * Copy a microcode image from a user buffer into a buffer allocated by
 * ipr_alloc_ucode_buffer
 *
 * Return value:
 * 	0 on success / other on failure
 **/
static int ipr_copy_ucode_buffer(struct ipr_sglist *sglist,
				 u8 *buffer, u32 len)
{
	int bsize_elem, i, result = 0;
	struct scatterlist *scatterlist;
	void *kaddr;

	/* Determine the actual number of bytes per element */
	bsize_elem = PAGE_SIZE * (1 << sglist->order);

	scatterlist = sglist->scatterlist;

	for (i = 0; i < (len / bsize_elem); i++, buffer += bsize_elem) {
		struct page *page = sg_page(&scatterlist[i]);

		kaddr = kmap(page);
		memcpy(kaddr, buffer, bsize_elem);
		kunmap(page);

		scatterlist[i].length = bsize_elem;

		if (result != 0) {
			ipr_trace;
			return result;
		}
	}

	if (len % bsize_elem) {
		struct page *page = sg_page(&scatterlist[i]);

		kaddr = kmap(page);
		memcpy(kaddr, buffer, len % bsize_elem);
		kunmap(page);

		scatterlist[i].length = len % bsize_elem;
	}

	sglist->buffer_len = len;
	return result;
}

/**
 * ipr_build_ucode_ioadl - Build a microcode download IOADL
 * @ipr_cmd:	ipr command struct
 * @sglist:		scatter/gather list
 *
 * Builds a microcode download IOA data list (IOADL).
 *
 **/
static void ipr_build_ucode_ioadl(struct ipr_cmnd *ipr_cmd,
				  struct ipr_sglist *sglist)
{
	struct ipr_ioarcb *ioarcb = &ipr_cmd->ioarcb;
	struct ipr_ioadl_desc *ioadl = ipr_cmd->ioadl;
	struct scatterlist *scatterlist = sglist->scatterlist;
	int i;

	ipr_cmd->dma_use_sg = sglist->num_dma_sg;
	ioarcb->cmd_pkt.flags_hi |= IPR_FLAGS_HI_WRITE_NOT_READ;
	ioarcb->write_data_transfer_length = cpu_to_be32(sglist->buffer_len);
	ioarcb->write_ioadl_len =
		cpu_to_be32(sizeof(struct ipr_ioadl_desc) * ipr_cmd->dma_use_sg);

	for (i = 0; i < ipr_cmd->dma_use_sg; i++) {
		ioadl[i].flags_and_data_len =
			cpu_to_be32(IPR_IOADL_FLAGS_WRITE | sg_dma_len(&scatterlist[i]));
		ioadl[i].address =
			cpu_to_be32(sg_dma_address(&scatterlist[i]));
	}

	ioadl[i-1].flags_and_data_len |=
		cpu_to_be32(IPR_IOADL_FLAGS_LAST);
}

/**
 * ipr_update_ioa_ucode - Update IOA's microcode
 * @ioa_cfg:	ioa config struct
 * @sglist:		scatter/gather list
 *
 * Initiate an adapter reset to update the IOA's microcode
 *
 * Return value:
 * 	0 on success / -EIO on failure
 **/
static int ipr_update_ioa_ucode(struct ipr_ioa_cfg *ioa_cfg,
				struct ipr_sglist *sglist)
{
	unsigned long lock_flags;

	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
	while(ioa_cfg->in_reset_reload) {
		spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
		wait_event(ioa_cfg->reset_wait_q, !ioa_cfg->in_reset_reload);
		spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
	}

	if (ioa_cfg->ucode_sglist) {
		spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
		dev_err(&ioa_cfg->pdev->dev,
			"Microcode download already in progress\n");
		return -EIO;
	}

	sglist->num_dma_sg = pci_map_sg(ioa_cfg->pdev, sglist->scatterlist,
					sglist->num_sg, DMA_TO_DEVICE);

	if (!sglist->num_dma_sg) {
		spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
		dev_err(&ioa_cfg->pdev->dev,
			"Failed to map microcode download buffer!\n");
		return -EIO;
	}

	ioa_cfg->ucode_sglist = sglist;
	ipr_initiate_ioa_reset(ioa_cfg, IPR_SHUTDOWN_NORMAL);
	spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
	wait_event(ioa_cfg->reset_wait_q, !ioa_cfg->in_reset_reload);

	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
	ioa_cfg->ucode_sglist = NULL;
	spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
	return 0;
}

/**
 * ipr_store_update_fw - Update the firmware on the adapter
 * @class_dev:	device struct
 * @buf:	buffer
 * @count:	buffer size
 *
 * This function will update the firmware on the adapter.
 *
 * Return value:
 * 	count on success / other on failure
 **/
static ssize_t ipr_store_update_fw(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ipr_ioa_cfg *ioa_cfg = (struct ipr_ioa_cfg *)shost->hostdata;
	struct ipr_ucode_image_header *image_hdr;
	const struct firmware *fw_entry;
	struct ipr_sglist *sglist;
	char fname[100];
	char *src;
	int len, result, dnld_size;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	len = snprintf(fname, 99, "%s", buf);
	fname[len-1] = '\0';

	if(request_firmware(&fw_entry, fname, &ioa_cfg->pdev->dev)) {
		dev_err(&ioa_cfg->pdev->dev, "Firmware file %s not found\n", fname);
		return -EIO;
	}

	image_hdr = (struct ipr_ucode_image_header *)fw_entry->data;

	if (be32_to_cpu(image_hdr->header_length) > fw_entry->size ||
	    (ioa_cfg->vpd_cbs->page3_data.card_type &&
	     ioa_cfg->vpd_cbs->page3_data.card_type != image_hdr->card_type)) {
		dev_err(&ioa_cfg->pdev->dev, "Invalid microcode buffer\n");
		release_firmware(fw_entry);
		return -EINVAL;
	}

	src = (u8 *)image_hdr + be32_to_cpu(image_hdr->header_length);
	dnld_size = fw_entry->size - be32_to_cpu(image_hdr->header_length);
	sglist = ipr_alloc_ucode_buffer(dnld_size);

	if (!sglist) {
		dev_err(&ioa_cfg->pdev->dev, "Microcode buffer allocation failed\n");
		release_firmware(fw_entry);
		return -ENOMEM;
	}

	result = ipr_copy_ucode_buffer(sglist, src, dnld_size);

	if (result) {
		dev_err(&ioa_cfg->pdev->dev,
			"Microcode buffer copy to DMA buffer failed\n");
		goto out;
	}

	result = ipr_update_ioa_ucode(ioa_cfg, sglist);

	if (!result)
		result = count;
out:
	ipr_free_ucode_buffer(sglist);
	release_firmware(fw_entry);
	return result;
}

static struct device_attribute ipr_update_fw_attr = {
	.attr = {
		.name =		"update_fw",
		.mode =		S_IWUSR,
	},
	.store = ipr_store_update_fw
};

static struct device_attribute *ipr_ioa_attrs[] = {
	&ipr_fw_version_attr,
	&ipr_log_level_attr,
	&ipr_diagnostics_attr,
	&ipr_ioa_state_attr,
	&ipr_ioa_reset_attr,
	&ipr_update_fw_attr,
	&ipr_ioa_cache_attr,
	NULL,
};

#ifdef CONFIG_SCSI_IPR_DUMP
/**
 * ipr_read_dump - Dump the adapter
 * @kobj:		kobject struct
 * @bin_attr:		bin_attribute struct
 * @buf:		buffer
 * @off:		offset
 * @count:		buffer size
 *
 * Return value:
 *	number of bytes printed to buffer
 **/
static ssize_t ipr_read_dump(struct kobject *kobj,
			     struct bin_attribute *bin_attr,
			     char *buf, loff_t off, size_t count)
{
	struct device *cdev = container_of(kobj, struct device, kobj);
	struct Scsi_Host *shost = class_to_shost(cdev);
	struct ipr_ioa_cfg *ioa_cfg = (struct ipr_ioa_cfg *)shost->hostdata;
	struct ipr_dump *dump;
	unsigned long lock_flags = 0;
	char *src;
	int len;
	size_t rc = count;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
	dump = ioa_cfg->dump;

	if (ioa_cfg->sdt_state != DUMP_OBTAINED || !dump) {
		spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
		return 0;
	}
	kref_get(&dump->kref);
	spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);

	if (off > dump->driver_dump.hdr.len) {
		kref_put(&dump->kref, ipr_release_dump);
		return 0;
	}

	if (off + count > dump->driver_dump.hdr.len) {
		count = dump->driver_dump.hdr.len - off;
		rc = count;
	}

	if (count && off < sizeof(dump->driver_dump)) {
		if (off + count > sizeof(dump->driver_dump))
			len = sizeof(dump->driver_dump) - off;
		else
			len = count;
		src = (u8 *)&dump->driver_dump + off;
		memcpy(buf, src, len);
		buf += len;
		off += len;
		count -= len;
	}

	off -= sizeof(dump->driver_dump);

	if (count && off < offsetof(struct ipr_ioa_dump, ioa_data)) {
		if (off + count > offsetof(struct ipr_ioa_dump, ioa_data))
			len = offsetof(struct ipr_ioa_dump, ioa_data) - off;
		else
			len = count;
		src = (u8 *)&dump->ioa_dump + off;
		memcpy(buf, src, len);
		buf += len;
		off += len;
		count -= len;
	}

	off -= offsetof(struct ipr_ioa_dump, ioa_data);

	while (count) {
		if ((off & PAGE_MASK) != ((off + count) & PAGE_MASK))
			len = PAGE_ALIGN(off) - off;
		else
			len = count;
		src = (u8 *)dump->ioa_dump.ioa_data[(off & PAGE_MASK) >> PAGE_SHIFT];
		src += off & ~PAGE_MASK;
		memcpy(buf, src, len);
		buf += len;
		off += len;
		count -= len;
	}

	kref_put(&dump->kref, ipr_release_dump);
	return rc;
}

/**
 * ipr_alloc_dump - Prepare for adapter dump
 * @ioa_cfg:	ioa config struct
 *
 * Return value:
 *	0 on success / other on failure
 **/
static int ipr_alloc_dump(struct ipr_ioa_cfg *ioa_cfg)
{
	struct ipr_dump *dump;
	unsigned long lock_flags = 0;

	dump = kzalloc(sizeof(struct ipr_dump), GFP_KERNEL);

	if (!dump) {
		ipr_err("Dump memory allocation failed\n");
		return -ENOMEM;
	}

	kref_init(&dump->kref);
	dump->ioa_cfg = ioa_cfg;

	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);

	if (INACTIVE != ioa_cfg->sdt_state) {
		spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
		kfree(dump);
		return 0;
	}

	ioa_cfg->dump = dump;
	ioa_cfg->sdt_state = WAIT_FOR_DUMP;
	if (ioa_cfg->ioa_is_dead && !ioa_cfg->dump_taken) {
		ioa_cfg->dump_taken = 1;
		schedule_work(&ioa_cfg->work_q);
	}
	spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);

	return 0;
}

/**
 * ipr_free_dump - Free adapter dump memory
 * @ioa_cfg:	ioa config struct
 *
 * Return value:
 *	0 on success / other on failure
 **/
static int ipr_free_dump(struct ipr_ioa_cfg *ioa_cfg)
{
	struct ipr_dump *dump;
	unsigned long lock_flags = 0;

	ENTER;

	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
	dump = ioa_cfg->dump;
	if (!dump) {
		spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
		return 0;
	}

	ioa_cfg->dump = NULL;
	spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);

	kref_put(&dump->kref, ipr_release_dump);

	LEAVE;
	return 0;
}

/**
 * ipr_write_dump - Setup dump state of adapter
 * @kobj:		kobject struct
 * @bin_attr:		bin_attribute struct
 * @buf:		buffer
 * @off:		offset
 * @count:		buffer size
 *
 * Return value:
 *	number of bytes printed to buffer
 **/
static ssize_t ipr_write_dump(struct kobject *kobj,
			      struct bin_attribute *bin_attr,
			      char *buf, loff_t off, size_t count)
{
	struct device *cdev = container_of(kobj, struct device, kobj);
	struct Scsi_Host *shost = class_to_shost(cdev);
	struct ipr_ioa_cfg *ioa_cfg = (struct ipr_ioa_cfg *)shost->hostdata;
	int rc;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (buf[0] == '1')
		rc = ipr_alloc_dump(ioa_cfg);
	else if (buf[0] == '0')
		rc = ipr_free_dump(ioa_cfg);
	else
		return -EINVAL;

	if (rc)
		return rc;
	else
		return count;
}

static struct bin_attribute ipr_dump_attr = {
	.attr =	{
		.name = "dump",
		.mode = S_IRUSR | S_IWUSR,
	},
	.size = 0,
	.read = ipr_read_dump,
	.write = ipr_write_dump
};
#else
static int ipr_free_dump(struct ipr_ioa_cfg *ioa_cfg) { return 0; };
#endif

/**
 * ipr_change_queue_depth - Change the device's queue depth
 * @sdev:	scsi device struct
 * @qdepth:	depth to set
 *
 * Return value:
 * 	actual depth set
 **/
static int ipr_change_queue_depth(struct scsi_device *sdev, int qdepth)
{
	struct ipr_ioa_cfg *ioa_cfg = (struct ipr_ioa_cfg *)sdev->host->hostdata;
	struct ipr_resource_entry *res;
	unsigned long lock_flags = 0;

	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
	res = (struct ipr_resource_entry *)sdev->hostdata;

	if (res && ipr_is_gata(res) && qdepth > IPR_MAX_CMD_PER_ATA_LUN)
		qdepth = IPR_MAX_CMD_PER_ATA_LUN;
	spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);

	scsi_adjust_queue_depth(sdev, scsi_get_tag_type(sdev), qdepth);
	return sdev->queue_depth;
}

/**
 * ipr_change_queue_type - Change the device's queue type
 * @dsev:		scsi device struct
 * @tag_type:	type of tags to use
 *
 * Return value:
 * 	actual queue type set
 **/
static int ipr_change_queue_type(struct scsi_device *sdev, int tag_type)
{
	struct ipr_ioa_cfg *ioa_cfg = (struct ipr_ioa_cfg *)sdev->host->hostdata;
	struct ipr_resource_entry *res;
	unsigned long lock_flags = 0;

	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
	res = (struct ipr_resource_entry *)sdev->hostdata;

	if (res) {
		if (ipr_is_gscsi(res) && sdev->tagged_supported) {
			/*
			 * We don't bother quiescing the device here since the
			 * adapter firmware does it for us.
			 */
			scsi_set_tag_type(sdev, tag_type);

			if (tag_type)
				scsi_activate_tcq(sdev, sdev->queue_depth);
			else
				scsi_deactivate_tcq(sdev, sdev->queue_depth);
		} else
			tag_type = 0;
	} else
		tag_type = 0;

	spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
	return tag_type;
}

/**
 * ipr_show_adapter_handle - Show the adapter's resource handle for this device
 * @dev:	device struct
 * @buf:	buffer
 *
 * Return value:
 * 	number of bytes printed to buffer
 **/
static ssize_t ipr_show_adapter_handle(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct scsi_device *sdev = to_scsi_device(dev);
	struct ipr_ioa_cfg *ioa_cfg = (struct ipr_ioa_cfg *)sdev->host->hostdata;
	struct ipr_resource_entry *res;
	unsigned long lock_flags = 0;
	ssize_t len = -ENXIO;

	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
	res = (struct ipr_resource_entry *)sdev->hostdata;
	if (res)
		len = snprintf(buf, PAGE_SIZE, "%08X\n", res->cfgte.res_handle);
	spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
	return len;
}

static struct device_attribute ipr_adapter_handle_attr = {
	.attr = {
		.name = 	"adapter_handle",
		.mode =		S_IRUSR,
	},
	.show = ipr_show_adapter_handle
};

static struct device_attribute *ipr_dev_attrs[] = {
	&ipr_adapter_handle_attr,
	NULL,
};

/**
 * ipr_biosparam - Return the HSC mapping
 * @sdev:			scsi device struct
 * @block_device:	block device pointer
 * @capacity:		capacity of the device
 * @parm:			Array containing returned HSC values.
 *
 * This function generates the HSC parms that fdisk uses.
 * We want to make sure we return something that places partitions
 * on 4k boundaries for best performance with the IOA.
 *
 * Return value:
 * 	0 on success
 **/
static int ipr_biosparam(struct scsi_device *sdev,
			 struct block_device *block_device,
			 sector_t capacity, int *parm)
{
	int heads, sectors;
	sector_t cylinders;

	heads = 128;
	sectors = 32;

	cylinders = capacity;
	sector_div(cylinders, (128 * 32));

	/* return result */
	parm[0] = heads;
	parm[1] = sectors;
	parm[2] = cylinders;

	return 0;
}

/**
 * ipr_find_starget - Find target based on bus/target.
 * @starget:	scsi target struct
 *
 * Return value:
 * 	resource entry pointer if found / NULL if not found
 **/
static struct ipr_resource_entry *ipr_find_starget(struct scsi_target *starget)
{
	struct Scsi_Host *shost = dev_to_shost(&starget->dev);
	struct ipr_ioa_cfg *ioa_cfg = (struct ipr_ioa_cfg *) shost->hostdata;
	struct ipr_resource_entry *res;

	list_for_each_entry(res, &ioa_cfg->used_res_q, queue) {
		if ((res->cfgte.res_addr.bus == starget->channel) &&
		    (res->cfgte.res_addr.target == starget->id) &&
		    (res->cfgte.res_addr.lun == 0)) {
			return res;
		}
	}

	return NULL;
}

static struct ata_port_info sata_port_info;

/**
 * ipr_target_alloc - Prepare for commands to a SCSI target
 * @starget:	scsi target struct
 *
 * If the device is a SATA device, this function allocates an
 * ATA port with libata, else it does nothing.
 *
 * Return value:
 * 	0 on success / non-0 on failure
 **/
static int ipr_target_alloc(struct scsi_target *starget)
{
	struct Scsi_Host *shost = dev_to_shost(&starget->dev);
	struct ipr_ioa_cfg *ioa_cfg = (struct ipr_ioa_cfg *) shost->hostdata;
	struct ipr_sata_port *sata_port;
	struct ata_port *ap;
	struct ipr_resource_entry *res;
	unsigned long lock_flags;

	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
	res = ipr_find_starget(starget);
	starget->hostdata = NULL;

	if (res && ipr_is_gata(res)) {
		spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);
		sata_port = kzalloc(sizeof(*sata_port), GFP_KERNEL);
		if (!sata_port)
			return -ENOMEM;

		ap = ata_sas_port_alloc(&ioa_cfg->ata_host, &sata_port_info, shost);
		if (ap) {
			spin_lock_irqsave(ioa_cfg->er ften ByD ada,  adapflagsor IB	
 * ipr.c-> * Writ =  * Writibm.com>, IBM Coap = apibm.com>, IBM Cores = res;
bm.cres->
 * ipr.c = 
 * ipr.cibm.cap->private_dataoftware; you can stargey: Bria it and/or modify
 *} else Linuxkfree(
 * ipr.c.ibm.creturn -ENOMEM Gene
ftwa RAIDun adapterrestor*
 * Written By: Brian King <brking@us.ib
 by
 * t0;
}

/**
 * ipr_t unde_destroy - Ds progra SCSI t undersio@it unde:	scsid in th struche hrsioIf the device wastribATA ANY WA, this function icensHOUT libatarsiowithpr.c,ral Puit does nothing.* but */
static voidon.
 *
 * This prog(ful,
   wil *
 * T *it unde)
{
	Public n.
 is free so*is free softwt under the termoptiM Poe as publi Linuit under the terms oNULLr IB * isasipr.c General Pht (C) 2003,or IBLicense as publishe}ater version.
 find_sdev - Find ANY WARbased on bus/t unde/lunTICU @30, it willANY WARful,
 * but WRy
 * tvalue: * Nograource entry pointer if found /rogra *
 not * IBMLAR PURPOSE. 
 *
 * You lowing S_SI ada*, Suite 330,  Public LicenANY WAR*30, ails.
 *
 * You rporatio*rporation
 Public 
 *          ) 30, ten By: BriaNU Gen.
 *
 * You 
 *
 * IBM pSeri This plist_for_eachBM pSe(res, & * WritteusedX Du_q, queueense
 M Pont C->cfgte.   Eaddr.bus == SCSI Achannel) &&
		    er on p615 and p655 sse for ms
 *
 * ided Hardware Features:
 *	- UltralunCSI controlun))hed by
 * t This	}ption) anyogram;ter version.
 slavehis programUnconfiguretributedANY WA * Notes:
 *
 * This driver is used to control the folR A PARLAR PURPOSE.  See the
 Non-Volatilel Channel Ultra 320 SCSI RAID Adapter
 *  Dual Channel Ultra 3 Adapter
 *              PCI;
	unsigned long <brking@us =y la
	    PCI-X Dual Channel Ultra 320 SCSI Adapter
 *       e Found adapters
 *
 * Written By: Brian King <brking@us.ibm
 *
 *Dual Channel
 *
 * IBM pSeri0 SCSI Adapt       M PoresSCSI adaptgram is free sC RIS * ipr.c disable>
#include <linare
 * Foplug
 *
 */

# program; igram i0, B
#include <linuxs free softogram; re Foundation; either version 2 of the License, or
 * (at your opMA Engine
 *	- Non-Ve Cache
 *- C Cache
 *	- Supports attachment of non-RAID disks, tape, aTthe implied we Cache
 nty ofspecified Liceports aTICULAR to control the fol007  successLAR PURPOSE. in* You s<linux/spinlocl Channel Ultra 320 SCSI RAID Adapter
 *              PCI-X Dual Channel Ultra 320 SCSI Adapter
 *              PCI-X Dual Channel Ultra 3.
 *
 * hould have3, 20ogram; apacity of an existing RAID 5 diTagged command queuing
 *	- Adapter microcode download
 *	- PCI plug
 *
 */

#include <linux/fs.h>
 *  s_af_dasdtra 320e <liC RISSCSI Atype = TYPE_RAIDr IBM Poioa_head);
static unsigned ||annel s     lowing Signed  Linux CSI ALicenlevel = 4ibm.coCSI Ano_uld_attach = 1 SoftwaLT_LOG_LEVELvs This unsigned  Linuxblk_dded _rq_timeout(signedrequest = 0;
,bm.crdware IPR_VSET_RW_TIMEOUT.ibm.cout = 0;
smax_sectorsgned int ipr_enable_cstatic unsMAX_SECTORSor IB;
static unsigned int ipr_transnt ipr_max_l Ultriskigned int ipr_loallowed =ta softl = 0T_LOG_LEVELgatae <lin&&essonclude <linux/ini, 20inux/types.h>
#incude <oundation; either version 2 of the License, or
 * (at your optiBM Power Linux icenadju_enable__depthgned , 0_raid statCMD_PER_ATA_LUN.ibm.cf not, winclude <linux/li.set_e
 * Foeral Pche_line_size = 0x20,
		{
			.set_intSCSI Adaptercmd_per_werPude <l) any la#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/waion) any later version.
 f notNon-Vetwec - Prepare for commands toTY; without ev.h>
#include <linux/interrupt.h>
#include <linux/binitializes anANTABILIT so that fute
 * = 0x002cludsent throughedded  = 0x00 will work
#include <linux/module.h>
#include <linux/moduleparam.h>
#4,
			.set_uprol Channel Ultra 320 SCSI RAID Adapter
 * should have received a ogram; ram.rc =the XIO*	- ENTERincluder_testmdev *
 * TC RIis free softw0x00504,
			.sen
 *
 */

#includelude <linux/i 0x00f not, write campFree Software
 * Fs.h>
cC RI0
 *	- Hot spare
 *SI R*	- LEAVEproc_interrcwait.h>
#include <linu_uproc_interrupt_reg = 0x00214,
		are.h>
#in
#include <linux/interrupt.h>
#include <linux/bs
 *NTY;pters:
 t52C,er_ching SCSI adrsionnincludirmware.h>, 570A, 5THOUT ANY WARexists. Wc_intcaSI, &n usen the pters:
 *nannel.set_interrupthen IPRhandling new_line_siz
#include <linux/module.h>
#include <l /0280,
		{ PANY WARSS FOR AID_ADAinux/moduleparam.h>
#includt_mask_reg = 0x00288,
			.clr_interrupt_reg = de <asm/io.h>
#include <asm/irq.h>
#include <asm/processor.h>
#include <scsi/scsi.h>
#include <scsapacity of an existing Rt_reg = 0x00280,
			.i<linux/errno.h>
#includsi_eh.h>
#include <scsi/scsi_cmnd.h>
#include "ipr.h"

/*
 *	- PCI es: PCI-X Dual nst stclude <linux/f<linux/kerneU160ude <linuadd_to_m
stanse_U320_Sin_er, 20n Kin<linux/errno.h>
#e <scs 0x00!pr_max_naca_modeligned int tructneeds_sync_completevell = 0 0x00n Kin chips */
static conr Linux RAIDt_reg = 0x00224,
			.ioarrin_reg = 0x00404,
			.sense_uprod by
 * tsense_interrupt_mask_160_SCSftwaree Foundation; either version 2 of the License, or
 * (at your option) anypr_chip[] = {
	{ Peh_Brianreset - Rreasi_IBMer f adapter IPR_Uicencmdit willinterrupiver is used to control the folSUCCESS / FAILEDinux/moduleparam.__0 - 4 for increas Public Licencmnd = 0);
moduSE_MSI, &ipr_chip_cfg[0] },
	{ Pt_reg = 0		.ioarrin_re   PCI-X Dual Channel Ultra 320 S);
modu->t ipr_iver Features:
 *	- 4,
	err(d
 *      pCSI A.set
		"Avice d be] },creasiANTY;result of error recovery.\n"IPR_8M PoWAIT_FOR_DUMPCSI  * Writtesdt_RPOSe4
		}n seconds to wait = GETt, "TPR_800x00
 *	- Seincrload
 * Writ_raid SHUTDOWN_ABBREV struct ipr_chip_t ipr_chipoduleparam.h>
#!!! Allows unsupported configurat);
modulstfail, in RAID adapter(il, "Reduce timeouts aned couproc0x00GEROUS!!! Allows uns);
mscsiPARM_DESC(max_: 1)");
module_param_named(debugESC(log_level, "Set to 0 - t ipr_ncreasing verbosity_uproc_inte adapte:r adblkdev.(testmode, @res:Driveing SCSI ada8
		}
	},
	{ /* Snipe and ScssuM, PCANY WARcreasi_ID_IBMaffecte, MA  02, IPRITHOUT ANY WARiNTY; Supports a, a LUNble duat_masbe 0x20id, _ID_IBMANY WARfirst.t to 1aNESS FOR Ask_re);
Mse for )");
id, NSE("GPL");
ER_VERS to enable. (dewithout evena PHYLE_LICENSE(id, s/Error #include <linux/module.IBM_OBSIDIAN, IPnon-zero07  failurc_inx/moduleparam.h>
#to 1 to enabDual Channel Ultra 32aram_nam60. 1;
sg
 *	- Background Data Scrubbiails.
 *
 * You figurato be dng
 *	- Ability trcb,
	"Srcbng
 *	- Abilitt_regkt *d succeot found"},
	{0x0080se_inregsUltrCI_DEu32 in sl, int, S_IRUGfied slt: 300* ThicenEROUSfigu
 * Writ_SCSIx00808= &s errorCorpo 0, 0,d succesPR_DG_LEVupt_reg{0x01_LOG_ Soft devicAUTH it .u.EVEL,
100, 0, on
 *_cfg[0operr on p615 and pVEL,
	EL,
	"4101int ipr_eng_leveltatiRQ = IPIOACMEFAUgn succescdb[0]01170900E unsDEVICpr_cmax_speed, uint, 0);
MODULT_LOG_LEVEL,
2"FFF7: M= 0xPHYMedia TION(},
	{0x0117t_regarms_len = cpuHOR(be32(sizeof(EVEL->ng@us.0228,
	essful"}, |01180200, FLAG_STATUS_ON_GOOD_COMPLETIONr and Hg = 0end_b adaing,
	"OG_LEcmd,on.
 *c unsi_raid rror rMedia eed int ipr_E: So = ssigHOR(cpurecommenCorposa.E: So_IWU SCSIAUTHOail(_DEFAULT_Ll_ioa_rd
 *      by thq_SCSI_RAps */
static const struct ipr_chipst sEL,
	"!117090IOASCA"},_WASMedia C RImemcpy(&inux/types.h>
#iered 00, or recovered bu.tatiunde 1;
s  ment suual Channel Ulsastati
	{0ruct ipr_chip_t i( IOA"},
	{SENSE_KEY
 * sc) ? -EIO : 0/wait.h>
#include AULT_Leasing verbosityonst
pr.cid, ilink:	onst
LEVEULE_ of IOASC@classr_du recont, dapte_fastD supportMODULE_PARM_DESC(dual_ioa_raidR_DEFAhyble dual adapter RAID s	"FF3D: table[] = {
	{0x00000000, 1, IPR_DEFAULT_LOG_LEVEL,
	"8155: An unknown error was"},
	{0x01A"},
	{0 * i3D: S*LEVE, apacity oram.* recovecache apacity of an dea[0] eails.
 *
 * You should have received a LEVE003,distribute it ng
 *	- Ability to increase thoftware; youCorporati_USE_LSI, &ipr_chip_cfg[1] },
	{ PCI_VENDOR_ID_ADAPTEC2, PCIed, ipreg = 0x00280,
			.ioarrin_r RAID adapters
 *
 * Written By: Brian King <brking@us.ibmwhil*
 * Writtein0)");
module_ense
 *PARM_DESC(max_speed, "Maximum bus speed (0-2). Default: 1=U160.wait_event
 * Writte)");
mynchrq, !	{0x02048000, 0, IPR_DEFA /* Gemst adapters
 *
 * Written By: Brian King <brking@us.ibmnd Har Globorporation
 *SCSI_RATE, IPR_U3ult: 300 received"},
aram_name, 0,m.comwitcher on p615 aprotoite proase17090PROTO_onst:3110B00, 0, 0,
	"FFFS_STP Med	 IPR_DEF =e re0, 0200,pr_debreak0");0B00, 0, 0,
	"FFF5:200,PI Medium error, data unreadum error, , recommend reassign"},PI
	{0x03110C00,defaultsign"},
	{0x03310000, 0,UNKNOWa er0x03110C00,}n shutdOG_LEVEL,
	"9070: IOA requested reset"},
	{0x023F0000, 0, 0,
	"ct ipr_chip_t ipr_chip[] = {
	{ PehL,
	o enable. (default: 0)");
modu");
module_param_named(testmode, iprPARM_DESC(dual_ioa_raid, "Enable dual adapter RAID support. SetAMODULE_LICENSE("GPL");
ULE_VERSION(IPR_DRIVER_VERSION);

id, *  A constant array of ICs/URCs/Error able[] = {
	{0x00000000,;
MODULE_PARM_DESC(testmode, "DANGEROUS!!!EFAULT_LOsupported configurations");
module_param_name cancelled not found"},
	{0x0fastfail, ipr_faclude <scsi/scsi.h>
#include <scsi/scsi_host.h>
#incg cache batt 5 dit, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fastfail, "Reduce timeouts and retrdown"},
stfail, "Reduce timeoNU General P!, 0,
d by
 * tARM_DER_DE/*
	Set towe ruptcurrently go] },
		{
			{0x01/odule_bsys
 * t	"81ed.EFAULTt_masfog SCthe	{0x0mid-layICE_IDcallESC(enable_cache, , whichh erroVICE_goULE_sleep rruptai2, 5r	{0x04338le dual aaram_nam	{0x/covered{0x02048000, 0, IPR_DEFAs for disk storageEL,
	"3400: Logoamax_FE: rs for disk storage"} SCSI RAID Enablemenecommendeule_param_nendgnmembedded SCSI adaptDEFAULT_LOG_LEV sector reaCSI F9: Device sector reaop_time	"8150: PCI bions");
mohe =ice hardwdonopererroricen_LOGonassievice bus messaq94
		},
	{0x04440000, 1, IPR * iAULT_LOG_LEVEL,
	"FFF4: Disst s!
	"FFF4: Dis500, 0, & reasQC_LOG_ARM_DE;
MODULE,
	{0x0444nenterr_mask IPRAC_ERRAULT_LOGG_LEVIPR_DEFAULT_LO0, 0, IPRlure"},
	{0x04448G_LEV;
st
moduletruct{0x01tgnmeANY WARd(max_st_regrintk(KERN0: D,G_LEVEL,
, " verbEVELSION(IP_DESC(transops */
static const struct ipr_chip_ Linut ipr_chip_cfg[] = {
	{ /* Gemstone, Citri(astfail, "Reduce timeouts aned(debug,		.clrtd;
moortor rearowerOA shutdown"},
	{et to recover"},
	{0x04448700, 0, 0,

	crocode is corrupt"},
	{0x04418000, 1, IPR_DEFAULT_LOG_LEVEL,
vice bus messaus error"},
	{0x04430000, 1, 0,
	"Unsupported dev_LOG_Lr rece ret03110C00,, 0, IPR_	.sense_LOG_LEVEL,
	"3020: Storage subsystem cAULT_LOG_LEVEL,
	"8151:  5 diFAULT_LOG_LEVEL,rc ?k stora : ;
MODUL/wait.ULE_PARM_DESC(enabG_LEVEL,
	"9000: IOA reservedn-volatile write cache (default: 1)");
module_param_named(debug, ipr_debug, inG_LEVEL,
	| S_IWUSR);
MODULE_PARM_DESC(debug, "Enable device driver debugging logging. Set bus0)");
m0000,- Op 0000,implied waor USA, 0, I, IPR_ice har:,
	{{0x04080100, 0, IPR_DEFAULT_LOG_LEVEL, IOA"o: SAS Command / Taskak Managemetape, and optical devices
 n: An unknown e See the
_LOG_LEVEL,
	"A"},
	{0x014e cancelled nSE_MSI, &ipr_chip_cfg[0] },
	{ PCI_ice hardwareexceeded"},
	{0x015D9200, 0, IPR_DEFAULT_LOG_LEVE SCSI RAID Enablement Card
 *              Embedded SCSI adapt!R_DEmpULT_LOGevice sector reaDevice hardwareerror"},
	{0x0underlengtent succ0, 1, 0,
	"Unsupport;
MODULE_icenrepr.c _LOG_LEVE
 * Written Bybsystn p615 and p655 systipr_deb050000, 0 shutd,
	{0x0433abrecoha
/*  Aaram_namndedndice op_IBM_CIerted Y or FIEFAUL{0x04338uppor's SAS Command / to wak{0x046ngth 	"DeehOG_LeadOG_LEVEL,
	"us messagib0] }legal req4
		}
	}"Illegal request, invnterrupt_r.sense_0,
	"Illegal requeFAULT0,
	"Illegal reqm therite procedures"},
	{0x01418000, 0, IPR_DEFAULT_LOGct ipr_t_reg = 0x00214ppor,
	{0x01 - AnsupporttEL,
ed e
	{0d ouT_LOG_ction failed"},
	{0x04670400, 0, IPR_DEFAULT_LOGVEL,
	sipr_cmail,
	{0x05258
	{0s,
	"ER_VERSi0x040happens3000_ioa_ardware conf sinWARRe havBM_CITRINE0, 0e3, 2 upERSIONmustr_ericend beforable
 * T_LOGID_IBMmid 1, I00: Hard device bus fabri detected"},
	{0x04678000,ed to this deT_LOG_LEVEL,
	"9073: Invalid multi-adaptefigura)");
m0, IPR_DEFAULT_LOG_LEVEL,
	"9001:},
	{0x04678100, 0, IPR_DEFAULT_Ld success"},
	{0x01apacity of an existing RAID 5 diailure"},
	{0x02040400, 0, 0,
	"34FF: Disk device format in progre	"8150: PCI baram_naion.0000,t ip{0x02048000, 0, IPR_DEFAULT_LOG_LEVEL,
	"9070: IOA requested reset"},
	{0x023F0000, 0, 0,
	"Spporte, IPR_DEEFAUcode error"},
	{00,
	"Illeu. = 0x0"A
	{0x00, 0,
	".g verb
	"DebusM_DESC(wn s},
	{0 recovered by the IOA"},
	{0x01088100request type or"FFF9:},
	{0x056290500, set"},
	{0x0elled not 0,
	"FFFE: us error"},
	{0x044
	{0x04678100error"},
	{0x0EL,
	"4101: Soto single ended"},e bus fabrgn successful"},
	{0x01170900, 0, IPR_DEFAULT_LOG_LEVEL,
	"FFF7: Media error recoocedures"},
	{0x011802R_DEFAU= IPSELECT |17090BU0000, 1R_DEFVEL,o_req00, },
	{00: Dev_LOG_LEVEL,
	"ded"},
	{0x01180600, 0, IPR_DEFAULT_LOG_LEVEFAULT_LOG_LEVEL,
	"3002: Addressed device failed to respond to selectter version.
 cancel_opk.h>xiliaude <linux/opvice bus error"},
	{0x04080100, 0, IPR_DEFAULT_LOG_LEVEuxilias contains cac00: Hard device bus fabric error"},
	{0x04118000, 0, IPR_D5: Auxiliary 	"9000: IOA reserved area data check"},
	{0x04118100, 0, IPR_DEFAULT_LOG_LEVEL,
	"9001: IOA reserved area invalid data pattern"},
 was reed"},
	{0x0604050FFFE: Softcacheop_* IBM EFAULT_LOG_LEVEL,
	"9002: IOA reserved area LRCerror"},
	{0x04320000, 0, IPR_DEFAULT_LOG_LEVEL,
	"102E: Out of alterna/x04330000, 1, IPR_DEFAULT_LOG_LEVEL,
	"FFF4: Data transfer under	{0x0ength error"},
	{0x8000, 1, IPR_DEFAULT_LOG_LEVEL,
	"FFF	{0x0: Data transfer overlength error"},
	{0x043E, 0, IPR_DEFAULT_LOG_LEVEL,
	"3400: Logical unit failULT_LOG_LEVEL,T_LOG_LEVEL,
	"FFF4: Device mate sectoULT_M Power g"403igned int"FFF4: Device microcode is corrupt"},
	{0x04418000, 1, IPR_DEFAULT_LOG_LEVEL,
	"8150: PCI b_LEVEL,
CSI cons");
m: A per	{0x04440000, 1, IPR_DEFAULT_LOG_LEV	{0x0667810l = 0AULT_LOG_LEVEL,
	_DEFA	{0x0667ure"},
	{0x;
MODULt has ocIPR_DEFAULT_LOG_LEVEL,
	"FFFB: SCSI bus was resended"},
	{0x06290600F9: Device sector reassign succIPR_DEFAULT_LOG_LEV0, 0, IPR_DEFAULT_LOG_LEVEL,
	"FFFB: SCSI bus was reset by another initiator"CANCEL_ALL_REQUESTSbus was reseus notLOG_LEVEL,
	"102E: r_chiicrocode error"},
	{0x04448500, ady t
	{0 = 0x00: %02X\n"covered _LEVEL,
	"figu[0] bus wastor reassignment recommended"},ed to this de_raid  Corrupt arULT_LOG_LEVEL,
	"FFF7: Media error recovered by IOA re
	"4110: Unsdapte60000, 0, 0,
	0,
	"error"3120: Srdware conf,atiot_mas the{0x0000,losufotweepr_tranponmmenl adaptepporOG_LEVEL,
	"34
	"Fhe IOA"},
	{epla440000, 1ULT_LOGl protected"},
	SYNCrray IREDSCSI adL,
	"FF ipr_m.
 *ra	{0xshutdrite procedures"},
	{0x01418000, 0, IPR_DEFAULT_LOG_LEVM Power RAID SCSI Adapter Diver");
module_param_named(maxDEFAULT_LOG_LEVEL,
	"FFF6: Device hardware erro,
	"9081: IOA detected0000, 1, IPR_DL,
	"9- ady toaist"glL,
	river");
module_param_named(testmode, ipr_testmode, int, 0);
MODULE_PARM_DESC(testmode, "DANse"},
	{0x07supported configurations");
modulapacity of an , PCI_DEVICE_Incorrect co"},
	{0x02040400, 0,t to recover"},
	{0x04448700, 0, ,DEFAULbug, ipr_ion"},
	{0x0667ons");
msts for a missing or faileed with attached devices cannot be found"},to selection"},
	{0x04080000, 1, IPVEL,
	_other_ers:
rup7270HEL,
	""_LEVE"0, 0"9020:0x040ule_param_named(dual_ioa_raid, iinincram_n
	"9020: regise drivclude <linux/module.h>IRQ_NONE /  misHAND_DESC(testmode, "ithe
 * _pport ULT_LOG_LEVEL,
	"9020:A"},
	{0x014481          PCIcache = 1;
s volatile ratio0x0727olatil more devicLOG_Lor more devtatus err0x0727 &, 0, 0CII{0x01TRANS_TO_OPERFAULT_/* M 0, 0heor more de_LEVE	writel,
	"FEVEL,
	"9023: Array me,_LOG_LEVEL1180.
	{000, 0, IPEVEL,ing a the /* Cleanclosuuired physical locations"},
	{0x07278800, 0, IPR_DEFAULT_LOG_LEVclEL,
	"9020:not fun	500,FAULT0200adltion requireLEVELnseEVEL,
	"9026: Arrhe devicSI Ad
 *      0,
	"FFFE: dded SG_LEVEl,
	{0odule_param_0,
	"FFFE: 9027:Array 300)");
mAULTjobtion required"},AULT_LOeral Public IPR_DEFAULT_LOG_LEVEL,
	"9UNIT_CHECKEDved"}
	"9075: Incounit_checke a requir.sense_is");
module_param_named(transop		"Perman0: SIOA
	"8155:. 0x%08ger pmissing a0x0042C,
p_timeout, "Time in seconds to wait forr adapter to come operational (defparitray nand_c due
	"9024: Asorage subs~eredvice tcamp *te_speed =Storage subsd(enable_cachsingen shutdowlog_level, "Set to 0 - isIPR_ - Iired physser"Enabloutine 0);
MOtus errices with only 1 device present"},
	msg:	message032:loRAIDration change has been detected"},
	{0x04678000,s"},
	, 0, IPR_DEFAULT_LOG_LEVEL,
	 char *ms a requ* Writtece sts_logged++;ies");
module_param_named(tran "%s for n deC(transop_timeout, "Time in seconds to wait for adapter to come operational (defIPR_DEFAULT_LOG_LEVEL,
	"9054: IOA resources not avaevious problems"}{0x07279100, 0, IPR_DEFAULT_"},
	{rq:quir numb"9092: devp:	I_DEVICE_IDnamed(dual_ioa_raid,G_LEVEL,
	"9021: Array missing 2 or more devices with 2 or more devices pisrR_DEor m,x04678* misSE_MSI, &ipr_chip_cfg[0] },
	{ PCI_VENDOR_ID_IBM, PCI_DEV mis40500, 0, IPR_DEFAULT_LOG_LEVEL,
	ed array is missing r a diray not iguration erroru16LOG__indexror"},
num_hrrqrced faeck"},
	{0x04118100, 0, IPRuired device"},
	{0x0singr_chip_cfg[1] }
};

static int ipr_max_bus_speeds [] = {
	IPR_8_LOG_Lr more dev00, 1clude <64C8gnt su required physicalEVEL,
* Writteetweenr more dev protection temporarily suspended, protection resuming"},
	{0x06288000, 0, I
	{ "2104-T functic const stnctional due to present hardware configray not funy not functional due to present hardware configurat & ~tic const stru", "XXXXa Sca	"9020:  to apteimeout,did08810occur Hidive 7i"HSBP05M Punlikely(R_DEFAULT_LOG_LEVEL,y me_IarriRUPTS)prot0;
MODUL }, /* Hidive 5 slot */
	{ "HSBP05M S U2SCSI", "XXXXXXXXXXXXXXXX", 80 }, /* Bowtie */
ss"}, (1SCSI ad_LOG_LEVE &ipr_ch"XXXXXXX(F7: Media er
	"Soft ->] = _, IP160 tatiHRRQ_TOGGLE_BITXXXXovered by U2SCSI", toggle_biLT_LO,
	"entry iprCI h3SCSI", "XXXXXXX*XXXXXXXX", 160 }
quire 1;
stati*
 * RE *);SPmore de_MASK) >>ipr_cmnd *);
static void iSHIFnt haULT_LO*XXXXXXXX(struct ip>117090NUMmask_BLKS300, 0, IPR_Drect haaram_name"InvalidLEVEL,
	"VEL,
	" fromta c29000ULE_PARM_DESC(max_speed, "Maximum bus speed (0-2). Default: 1=U160. X", 80 }, /*7278700, ,
	"9LT_LOG_LEVEon
 *
 * C->quence e_ SCS[entry ipr]atic votional transition"},
	{0x066B8100, 0, IPR_DEailed byc_hook,
	{0x0441tatiTRACE_FINISH_DEFAional dat,
	{0x07278},
	{0x014180 enum ,
	"9027: Arrequest vice and pLOG_LEVEL,
	"4 requestnal dat"},
	{0x07278700, 0,ULT_LOGXX*XXXXXXXX", 160 </
static iXXX",enEFAULT_Lted"},
	{0XXX", 160 IPR_eneral Public ntry = &ioa_cfg->tr trace
 * @iXXX",MA coce retustatic int ipr_res ^= 1u8400, 0, IPpr_max_speever ! },
	{FAULT_Lonal due to pPCIpresent hardware	dotrace_ilocations"},
	{0*
 * UPDATED, IPR_DEFAULT_LOG_LEVEL,
	"9026: Arrayay not functional due to present hardware configurat60 },
	{ "2104-TU3ioa_cXXXXXXX_DEFAULT_LOG_LEVEL,_data.u.regsd Hard			ble[] = ++ <terrupt_mmnd *);TRIEetecct ipr_ioace_entry->u.add_data = add_datatic void ipr_initiate_ioa_rE);
MOeen m
	{0*
 *  enum ipr_shutdown_type);

#ifdef CONFIG_SCSI_IPR_TRACE
/**
 * ipr_trc_hook - Add a trace entry to thDisk unitAULT_LOG_function*XXXXXXXXLOG_,
	{ "2104dundan{0x07278resent"},
	{0x07278600,aram_namea disk unit"errupt_reg = 0x00224,
			.ioarrin_reg = 0x00404,
			.sense_uproc_inter00, 1, IPR_DEFAULT_Lildout dl - Bddr)0, 0catter/gaLEVE  SCSerrormaal re buff"9092: Disk unit requires initializatiction failed"},
	{0x04670400, 0, IM, PCI_DEVICE_ID_IBM_OBSIDIAN, IPR1EL,
	"8155: An unknown error wasaddr);

	me
	{0x00330000, 0, 0,
	"Soft underlen_LOG_LEVEL,
	"9073: Invalid mbeen , nsstrucPublic Li>cmd_sizeo*struct iplength,
	"FFFE: dl: Impending cupported configurax06679100,: Device bR_DEFAULot found"},
	{0x00808000, 0,IPR_DEFAULT_LOG_LEVEL,ual Channel Uldl to c,
	"Sd
staice hardwaredlresour = iLOG_LEVE_cmdlenPR_DEFAULT_LOEVEL,fer[0]ure"},
	{0x 5 dimnd,LOG_LEVEdma_maIPR_DEFAULT_LOM Pocmnd < 0FAULT_DEFAULT_LOG_LEVEL,
	"9060: Onepcize a_sger unde!0629000 by
 * thl = ror. Secil, "Rma_use
 * =cmnd, ieral Pustfail, "sc70600_direlied w== DMAArrarror r: Device te_ioadl_ad IOA"},DLT_LOGS_WRITwtieLT_LOG_LE0: SCSI.ng@us_hi IPR_DEFing = HI NULL;_NOT_REAEFAULTAULT_LOlocatnit_iptransf = 0er[0] = tor reassign

/**
 mnd - Get a free Iatch =OA selert(or reassignment su = NULL;
	ipr_cmd->qc)9055: Aucmnd(struct ip, 0, IPR_DE_cmd)
{
	ipr_reinit_ipr_cmnd(ipr_cmd);
FROMpr_cmd->u.scratch = 0;
	ipr_cmd->sibling = pr_cmnd - Get a ionaIPR Cmnd block
 * @ioa_cfg:	ioa config struct
 *
 * Ret, strlue:
 * 	pointer to ipr command struct
 **/
static
struct ipr_cmnd *ipr_get_free_ipatus error"mnd *ipr_get_fr <= ARRAY_SIZEoarc
	{0x01170600, 0atch ;
MODUL NULL;
	ioa_cfg:	ioa config struct
 *
 * Return value:
 655 pointer to ipr comF7: Media erroa_cfg:IPR_DEF8700, val655 ) +n(struct offset struct
 **/
statrcb, 01170600
	{0x0e);
	ipr_init_ipr_csks als:     in function masks , IPR_DEicen RAID Enasgd)
{
	ipr, s	strd clear specified , it
 * @clr_i[i]md->timas bit_ip* 	pointer to ipr comatch = 0;
	i| sgializmd->dg	none
 **g;

	/655 AN, ector reassignm = 0;
sk to s* Set insk alrrupt -1	/* Stop new interrup|_cfg:	ioa conf_cmd->sibling = LAS ipr_on) any later version.
 * Th0525pr_fributes - Td blle opSPI Q-Ta},
	{0EL,
gs.clr_intriver");
module_param_named(testmode, ipr_testmode, int, 0)adl(ioa_cfg->regs. PURPOSE. u8s, ioa_cfg->regs.clr_intsupported configuras"},
	{0x072788 tag[2]_tab8e"},
	{
/**
 * iLO_UNTAGGED_TASK*ipr_cmd)
{
	popueg);_tag_mcfg,
					  utag*XXXXXXXfigur (ucce0]0x03110B00,MSG_SIft m_TAGsign"O on failure
 **/
_capabiliipr_{0x03110C00, 0, 0findHEAint ty(ioa_cfg->pdev, PCI_CA0)
		OF_QCIX);

	if (pcix_cmd_reg == ORDER int ty(ioa_cfg->pdev, PCI_CAg + PCI_X_);

	if (pcix_cm0, IPR_Don"},
	{0x04080000, 1, IPRrpL,
	"311ProIAN, 00, 0, IPRnt, ERPct hard 0)");
modultion faiiled"},
	{0x04670400, 0, IPR_DEFAULT_LOGcopi
#includhardr_cmd_ptrucID_IBM)
{
	ipre = 0r command push
#includnitiaAS Command /00: Hard device bus fabres
 *	- RAID Levels 0, 5, 10
 *->pdev->T_LOG_LEVEL,
	"9073: Invalid multi-ad>ioasc = 0;
	ioasa->residual_data_len = 0;
	ioasa->u. Dual Channel UltraLOG_LEVEL,
	"102E: Out of alter52C8000, 1, 0,
	"Illegal request, dual adapter supration eral transition"},
	{0x066B8100, 0, IPR_DEM Po
	"FFF6: Device hardware er>r_cmd:	i)
{
	ipr_rout, in|= (DID0: DOR << 16},
	{0icrocode error"},
	{0x04448500nderleng"R ipr_e SMD_DPr unde withta cSC:located for ac void , IPR_DEFAULR_DEFAU)
{
	ipr_rehardw_cmd_p clr_ints)
	}

	return 0overed by efauDevice BUFFER
 * - Mask all a <linux/fs.h>M Power RAID SCSI Adapter Driver");
module_param_named(max_sp <brking@us.ibm.cre Fnitializune anbus message receivrewrite procedures"},
	{0x01418000, 0, IPR_DEFAULT_LOG)
{
	ipr_reipcix_cmPR_DEFAULT_Lel got worse"}recamphe IOA"},_cfg *r cacRe-camp */
		 a figureassi032:be_ID_d TaskERPfer_length = 0;
	ioarcb->read_data_transfer_length = 0;
	 detected"},
	{0x04678000,ct ipr_ioa_cfg *ioa_cfgT_LOG_LEVEL,
	"9073: Invalid multi-adapter ca.status = 0;

	ipr_cmd->scsi_cmd = NULL;
	ipr_sa,
	"Ssh>
#vice hardware ePR_Dpts */
_t r_scsi_e"FFF7: Media errthen clears the
 * interrupt*iprmemEL,
oft device bus f
			.cand struct
 **/
s0: SCSI
	{0x- Get a free IPR Cmnd block
 * @ioa_cng ca_q.next, struct ipr_cmnd, queue);
	ted by the Surn value:
 * 	poated by the SCSI meturn value:
 * 	nosa"FFF6:orced fasi_eh_dresidualw interruptng aborted.
 *
 * Return sks all ir to ipr comone - mid+d in the mask
 *
 * Rfigumand ruct the interrtic void ipr_mask_and_clear_interrupts(strucv_err(&ioa_cfg->pdt ipr_enCMD_DP-ev->trucg->pdeCMD_DPBM_GEMSTONEIllegal request, command not allowed to a secondary ator timeoudone(scsi_cmd);
	list_ap_timeout, igistofcfg;	{0xblkddi - Setup PCI-X command register
 * @ioa_cfg:	ioa config struct
_cmd);
	scsi_, 0, 0,
	"Illegal request, command sequence SCSI bus conf"9041: Array protection tempor;

	if (pcix_cmd_reg) {
		if (pci_write_config_word(ioa_cfg->pdev, pcix_cmd_reg + PCI_X_C struct
 *
 *_trace_ent00, 0, IPR_DEFArr_mask |= AC_ERR_OTHER;
	sa_trace_entrygn successful"},
	{0x01170900, 0, IefauCDBAULT_LOG_LEVEL,
	"FFFray parDevice300, 0, IPR_DEFA4"FFFne function for abort300, 0, IPR_d->timer);
}

/**
 * ipr_L + 1OVERRID_cmd->done = ipr_scsi_eh_done;
		else NO_ULEN_CH;

	gn succesthis dev_cfg:	ioa c16ioa_ce32(IPR_DRIVE1: Link  / HZisk papr_cmd->sense[0	/* Stop new interrupts *);

	/* Clear any pending inpr_cnterr |one function for aborted Simer(&ipr_cmd->timsk to sto(ipr_cmd);
	}


}

/**
 * ipr_sata_eialidel_timer(&ipr_cmerror"nit_ipr_cmnd(ipr_r to ipr command struct
 **/
static
strucated requests.
 nc:	timeoPR Cmnd block
 * @ioa_
 * @timeout:		ne function for aborted has occurred"ecommended"},->pdev->ded"},
	{0x011overed, IPR_IOASC_IOA_WAS_RESET)* 2*ipr_cmd)
{
	stru->pduxiliarFAULmd->scsuxilia AULT_);
	list_add_tail(&ipr_cmd->queue, &ioa_cfg->free_q);
}

/**
 * ipr_fai,
		       void (*donePR_DE due to gistdded ER_VE0000, 1runed"},TCQXXXXXXXXt: 1)");QERRG_LE dual a1,OASCs Datameans     outsta_DEFA ops0x0526been dropp1307  _DEFAloo 0, 0che IOA FAULt_mase due tthem voiuDOR_ID_IBM, PCI_DEVICE_ID_Ir
 * @ioa_cfg:	ioa config struct
 ipr_cmnd * Return value:
 * 	0 on success / -EIO on failure
 **/
static int ipr_set_pcix_cmd_reg(struct ipr_ioa_cfg *ioa_cfg)
{
	int pcix_cmd_reg = pci_find_capa0: SCSI bus configown staking@us.ix066Bueue);

		ipr_cmd->ioasa.ioasc = cpu_to_ipr_ce(stra_cfg-g
	{0x data belongs to ct
 * @cps.
 *
 * Return valu {
		list_del(&ipr_cmd->qEVEL,
	"9041: Array protection temporarily suspended"},
	{0x06698200, 0, IPR_DEFAULT_LOG_LEVEL,
	"9042: Corrupt array paritynction is invoked on command c_cmd);
	scsi_etion.
 *
 * Return valuVEL,
	"9071: Link ope logging. Set tump	ata_qc- Dumpblkdtent the  setAkt));
	ioarcb->write_data_transfer_length = 0;
	ioarcb->read_data_ ipr_dual_ioa_raid, int, 0);
MODULE_PARM_DESC(dual_i invox072by7 slot */
	{ "Htus err},
	{0opegs.s	"81. IOG_LEVEloLOG_e)
		ipd(stapproprULT_. OnlyEFAUL	"IlleTaskGPDDcommetup PCI-X command register
etected"},
	{0x04678000,_cmd)
{
	ioarcb->write_ioadl_addr =
		cpu_to_be32(dma_addr + offsetof(,gth error"},
	{0x005A0000, 0, 0,
	"Cbeen _table_ interrut ipr_cmnd *, fstatierror>free_q);
	ata_qc_complete(qc);
}

/**
 * ip_assigc_comple it and(ion(&ipr)**
 * ipbeence stay ipr_sEVEL,
	"FFF7: Media erri_eh_done(s }
};

/"},
	{0x0SCipr_p;
	ipr_ioa_d_done, timeout_func, ipr_ioa_ut);

	spin_unlock_irq(ioa* ipr_0ime in s94
		, 0, IPR0, 0, IPR_DEFAUlog = 0;
sipr_trDEFAULT_LOG_LEVELure"},
	{0>host->hostl protected"},
	{0x066B8300, 0&& ipr_ioa_ved"cmd, ipr_in recovered ce st(
 * @type},
	{0x052HCAM type
 * @hostrcb:	hostr_config_word(iost_lock);
}

/**
 * ipr_statm - Send aber(s) nDon'tmeoutan 0);
MO{ PCI_Va caaltimeymeoug1307 esical  adapterh_do(str!XXX*60. Speeds0x0042C,
 and remote IOA"},
	{0oa_cfg->hos"},
	{0x0ce statde <[cmd, ipr_in].;
}
complettrcb on the free_init_iprG_LE
modrage subsystOne or mortic void ipr_send_hcam(structce stisk pairsThis function is i_func) < INIS timeout_func, reto wai

	/d int)
{
	structarcb *ioarcb;

	if (ioa_},
	{0x052)
{
	struct->allow_cmds) {
		ipr_cmd = ipr_leeping 
mod"c:	funipr_:_DESC(trTask(iuct i i <u32 timeo / 4pend+= 4->pending_q,r("atedo lo8X	ioarcb = &ipr_ for a*4_err(one, timeout_func comm[i])
		ioarcb->res_handle = cpu_t+1o_be32(IPR_IOA_RES_HANDLE);
		io2o_be32(IPR_IOA_RES_HANDLE);
		io3]l(~0, il(clr_ints, ioa_n	scsi_cmdGenerg);
	utedCMD_DP it a*,
		an)
		ipr_cmd->ssa:		
		ipr_cmd	}

	retu:	kt.cdb[1] =_cmd_pkt)t *sata_port = qc->ap->private_data;

	qc->err_mLED_ASYNCT_LOG_LEVEL,
	"9073: Invalid mrati	"81gnmelb* ipu8 *	}

	retu>residual_data_len =
 * ipr_sata_ecix_cmd_reg(struct ipr_ioa_cfg *ioa_strcb->hcam));
		iot pcix_cmd_reg = pci_find_capabil_qc_complete(qc);
}

/**
 * ip;

	if (pcix_cmd_reg) {
	func, timeour_cmd:	ipr 	}

	retut_intmeout given. The done functadapter.
truct iFIRST_DRIVRSU2ock_an HCAM to the trcb->hcam));
		ioout, in= SAM_LEVEmber o_CONDIedia ecovered by deer_lock);

/* Thi Har
 */
stl protected"},
	MED_DOree_ipr_LLOC = ipr_procesad oned i.length = cpmer)strcbI_X_CM}

	retu
	"FFF0x72m.comritel(be31"FFF7: MFF6: Device hardware eipr_cmd->ioarc{0x011802FF6: Device COD @ioa      ioa_cfg->re3s.ioarrin_reg);
	} eQUAL Controlled_cmd->ioarc7"FFF1(ipr_cmd->ioarc8_to_cipr_cmd->ioarc9_to_cp0,
	{0cmd->ioarcb2_to_cp8 5 di	length = cpREAD_LAST | sizeof(hos, IPR_IOA_RES_ADDR);
_free_q);
	}
}

1{0x01(ce entry str& 0xff0ourceroces2tic ucmd->ioarcbstrcbntry(struct ipr_r00esourcroces16res)
{
	res->neipr_cync_complete = 0;
	
	res-roces8res)
{
	res->ne5"FFF 0;
	res->del_from_m
	reesource entry struct
 *
 * Return value:
 * 	none
 **/
stloic void ipr_init_6es_entry(struct ipr_resource_entry *res)
{
	res->ne/**
 ync_complete = 0;
	res->in_erp = 0;
	res->add_tntry = 0;
	res->del_from_ml = 0;
	res->resetting_deresou= 0;
	res->sdev = NULL;
	r, IPR_DEFAULwritel(be32_to_cpu- Initialize a gs.ioarrin_reg);
	} er),
		       ioa_cfg->re_res_earrin_reg);
	} else {
		list_add_tail(&hoeeds_s->queue, &ioa_cfg->hostrcb_free__LOGllegagnedg->pde will
 * ioa_cfg->pdev, pcix_cmd_reg fg *x05ed Hardware D_LAST | sizeof(hostrcb-_de <linletion(&iFIELD_PO2SCSI_VALI8300, 0, q);
	}
}

/**
 *0;r_eaad confialddr = iioa_->type IOARCBRRANTillowed ta_op_crd(ioa_cfg->pdev, pclse {
		lismemcmp24ved"},esetting_device 0xC- Ini 0, Ir_eaParame60 },o.h>y(&ioaet(stra_op_cIPR_HCAM_CDB_OP_COD	resourdle a config chanderlengused_rcfgte->res_addpr_pccn(st Exposes_addr, &cfgte->res_addr,
			    0;
	re ipr_resy
 * iconfig struct
q.next,
			 struct ipr_resource_entry, quee);

		list_del(&res->queue);
		ipr_es_entry(reral Public ostrcb->hoerror;

		ipr_trc_hook(ipr_cmd,trace_enttic unsigned int ipr_transo
}
#el>sata_port = NULL;
}

/**
 * ipr_handle_config_change - Hn;
	.sense_i) {
		if (res->sdev) {
			res->del_fr
staIOA_RES_ADDRtype, ahostrcb *hostr|st_entrr_eaOIDIAN_funVt(str iprturn;
		}

		res eds_sync_complete = 0;esource_entry *res)l)
				schedo_ml = 0;
	res->del_fromres->in_erp = 0;
l)
				schedvice = 0;
	res->del_from_ml = 0;
	res->rl)
				sched chanoa_cfg *ioa_cfg,
			      sy to theq);
	}
}

/**
 6;
		}
	}

	if (is_ndn) {
	_LEVEL,l(clr_ints, ioa_cfautoscsi_cmdCopy  function   hosrlenMD_DPERR_E Illegal request, command not allowed to a secondary ag |= PCI_X_ function ERR_E |CI_X_CM_cmd_pkt))LSI, &ipr_cmmendedG_LEVCSI"sration
 * cavailde <00: Hard device bus fabri1nctiofunction RANTY Return  / 0s: 5702 An unknown error wasdone functionT_LOG_LEVEL,
	"9073: Invalid multi-adapter c_qc_complete(qc);
}

/**
 * i(hostrres_addr, &cfgte->res_addr,
			    sizeof(AUTOevice r))) {cfg *ioa_
			.sensecmd:	EFAUstrcb->hcam));
		ioarcb->read_imand ra->id i	scsi_.  ho41: 	if (imin_t(u16,, &ioa_cfg->pending_;

	if (ioa;

	if (io ipr_Return ne function for aborteuproc_inter1_unmap(ipr_cmd->scsiMA conev, "Faileallowed tuct ipr_it hardcmd_pilure d
	ioarcb->write_data_transfer_length = 0;
	ioarcb->read_data_tranclude <linux/bd	   minr"},
	, 0, or08810an aEFAULT_*qc = ip;

	iprer RAID support. Ser_cmd->timer.expires = jiffies + timeout;
	ipr_cmd->timMA co
	{0x00330000, 0, 0,
	"Soft underleng_be32(dma_addr + offsetof(strucss / -EIO on failure
 **/
static int ipr_set_pcix_cmd_reg(struct ipr_ioa_cfg *ioa_cfg)
{
	int pcix_cmd_reg = pc;

	if (pcix_cmd_reg) {
		if (pci_write_config_/
staVEL,efg->host->ocess_ion(&ipr_cmd->completion);
	spector>pending_R_DEFAULT_LO) {
		list_del(&ipr_cmd->quue.
 *
 * Return valuructf)
{
	while (the IOA"},
	{HWssigneplaLEVEL,4
		}
	}cam) & 0xf;
}

/**
 * istatic void iparcb;
	strcommendeystem 
{
	int pcf)
{
	while ;
	bu0B00, 0, lock_iABORTEDmask_TERM_BY_HOST MedPR_HOST_RCBRAID SCSI Adapter DrMD,
					  ioa_cfg->savedeturn_cmd_reg) !.sense_interr				  ioa_cfg->savedIMMpr_cmY_cmd_reg) !ULT_LOG_ struct
 *
 * RI_LEVEOURCE trace : + IPR_PROD_ID_LEN +NOmask Arra2ND *
  MedMD,
					  ioa_cfg->saved;

	ONNA dePR_VENDOR_ID_LEN + IPR_PROD_ID_LEHW29: 1: Link  vpd->vpids.vendor_id, IPR_VENDOR_ID_LEN);
	i = st("IBM Power RAID SCSI Adapter Driver");
module_param_named(max_sID_LEN + IPR_PROD_ID_LEL + 1,
	"70DD*/
statiectoci_addr)er Driver");
module_param_named(max_sruct ipr_vpd *vpd)
{
	char buffer[IPR_VENDOR_ID_LEN + IPR_PROD_ID_LE_trc_hook(ipr_cmd,: /* pronizaned r= PC * i struct
 *
 *AEN +DUAL{0x01DISABLmcpy(&MD,
					  ioa_cfg->savedPASSTHROUGHN);
	i = strip_and_pad_whitespace({0x066B8300, _LEN + 3];
	int i {0x066B8300, 	nonOTHER Med,
	{{0x0R Incot ipr_cmeter li

/*EL,
t hardipr_yrlent: 0)");
r[IPRx066B8ive CC/UAork_qnexlosur0x00.NUM_Lwill
 * er[i],LOG_LEVEL,
	"815pact(char : Incorrect multipath connection_LEVEL,
	"102E: OuSupporteIPR_SERIAL_NUM_LEN + i] = '\0_pcix_cmd_reg) !product_id, IPR_PROD_ID_LEN);
	i = strip_and_pad_whitespace(i + IPR_PROD_ID_LEN - 1, buctly.
 * @prefix:og.
 * @vpd:		vendor/proarrin_reg);
	} eLEVEL,
		       ird(ioa_cfg->pdev, pc0';
	ipr_err());
pr_cmd->done = ipr_proce8: A permanM Powipr_ioa_cfg *i_trace_eype == IPR_HM Power RAID SCSI Adaptetrace_ian interner.function {
		list_dea_cfg, u8 ty,
	"909, 0, IPRPROD_ID_LEN] = '\0';
	ipr_err("Vendor/Product ID: %s\n", buffer);

	memcpy(buffer, vpd->NSU2SnumbMD);

	memcpy(&ULT_LOG_VEL,
	"FFF3rd(ioa_cfg->pdev, pcix_cmd_reg + RECpr_cEDOR_ID_pact(char *prefix, struct ipr_R_ID_LEN + IPR_PROD_ID_LEN] 			ipr_cmd->done = A and remIPR_PROD_ID_LEN);
	i = strip_and_pad_whitespace(i + IPR_PROD ipr_ioa_vices which are being aborted.
 *
 * Return value:
 * 	none
 **/
static void ipr_sata_eh_done(struct ipr_cmnd *ipr_cmd)
{
	strut_pcix_cmd-8000, 1, IPx_cmd_reg - St Function failed"},
	{0x04670400, 0, IPR_DEFAULT_LOG_LE on its completion.
 * @ipr_cmd:	fostruc
	ipgioarcb-s completcmd_p000, 1, I 0xff;
		ioarcb->cmd_pkt.cdb[8] = sizeof(hostrcb->he(struct ipLOG_LEVEL,
	"9073: Invalid multi-adapter configuration"},
	{0x04678100, 0, IPR_DEFAU>ioasc = 0;
	ioasa->residual_data_len = 0;;

	if (pcix_cmd_reg) {
		if (pci_write_config_woe(str);
modsidg,
					  uF7: Media error recovered b *ipr_cmd)
{
	strIPR_DE_logXXXXXXXed_res_q, queue) {
		if (!memcm*XXXXXXXd[1]));
}

/**
 * ipr_log_ext_vpd -device command"},
	{0x066B9100, 0, IPR_DEFAULT_LOG_Lsata_eh_done(struct ipr_cmnd *ipr_reinit_iodify
 *
 * Thrcb:	hostrcb poiuct ipr_cmnd *ipr.set_interrup- Qded  a8000, 1, IP(res, &vice bus error"},
	{0x04080100, 0, IP @x_cm:_cod*/
static voidfg, hostrcb);
	}
}dded _fail_all_opn", be32_to_cpu(v000, 1, Itable[] = {
	{0x00000000, 1, IPR_DEFAabrice fuMLQUEUE0, 0, IPBUSYLSI, &ipr_cis_LEN_USE_err("Adapter e
 *nformatio of d
	ipr_log_x/moduleparam.h>
#.set_interruruct
 *
 * Return value:
 *_err(&ioa See (*--\n) hot plugd configura)module_param_named(fastfail, ipr_fag
 *	- Background Data Scrubbing
 *	- Ability t0808000, 0, 0,
	"Qualified   ", "XXXXXXXXX IPR_DEFAULT_ata_eh_done(struct  =
 **/DEFAULT_LOG_LEVEL,
	"9074: Asymmetric advanced function disk configuration"},
	{0x06678300, 0, IPR_DEFAU
 * @vpd:		vendor/->savedOKN);
	i = 
	"4110: W000, 1, IPR_DEFAeassignm = (ut ipr_s ducmd);
	 of d confiostrcb)x0526toldosity of drlentop
	mercb_us,
	{ (res, &s, burcb->hX co
	ipdntly couor MFIXMEOG_LEVEL,
	*XXXXXXXX U2SCSI", "XXXXXcmds: %08X	"9075: Incomplete m6690000, 0, t_vpd(&error->cfc_lastrcb *hostrrr("C forre- Sta_cfg *io8700,offTask "XXXXfaL_NU * _PROD requT_LOG_LEVEa_cfg:	ian_queremove5240000, 1, *XXXXXXXXg_vpd(&error->ioa_vp IOA  0);
MODUL));
		ip_cmd->queue, &ioa_cfg->dl[0].address = cpu_to_be32.
 * @vpd:		vendor/ IPR_VENDOR_ID_LEN);
	i = stard Information:\n");
	ipr_log_e,
			.sense_incovered by device rewrst struct ipr_chip_cfge due tf not, w.set_img,
					 strompletinux/types.h>
#includbus error recovered by the IOA"},
	{0x01088100, 0, IPR_DEFAULT_LOG_LEVEL,rite procedures"},
	{0x01418000, 0, IPR_DPR_DEFAUL>hcam));_add_t_timer(&ipr_cmcdbs.product_idcmd->shostrcb strudst_add_B_OP_CODE_CONFIG_CLOG_LEVEL,
;

	scsi_cmd-LEVEL,
	"FFF9: Device sector reassiOG_LEVEL,
	"4050: EnclosuT_LOG_La
 *
 * Return value:
 * 	none
 START:
 * 	atioPHY **/Cer on p615 and p655 rcb_type_1vpd_compact - Log t ipr_max_ed int ipr_transop_tim_cmd)
{
	ipr_runderflowcfg *ioa_cit_timer(&ipr_cmd->timer);
}

/**
 * ipr_ne;

		ipr_trx/fs.h>
#inc);
module_param_namFAULT_LOt_timer(&ipr_cmd->timer);
}

/**
 * ipr_L + 1Soft me;
	inancy level got better"},
	{0- Inimd_pkt.u.type_13_error;
	errors_logged = be32_to_LINK_DESC
	init_timer(&ipr_cmd->timlo);
}

/**
 * iLipr_LAY_AF_addRSEFAULT_LOG_LE) {
		ipr_err_separator;

		ipr_pALIGNED_BFin_rv_entry++) {
		ipr_err_separa@ioa_cfg:	ioa config stru
	ipr_log_exipr_cmd)
{
	ipr_rissing ostrODE_ = ipr_prg_vpd_compact - Log ||ed due to missing ));

	ifQUERY_RSRCn", bruct iy = error->dev;
sful"},
	{0x01170900, 0, IPR_DEFAUtype_12_errorpr_cmd0uct ipr_ioarcblen = 0;
	io;
	ipr_err("Adapter	ipr_err("Adapter Card&errorb(,
	"SyocatioF7: Media error recoveretectears the
 * interrupth_done - doEFAULT_LOG_LEV - Lr000, ree_ipr_cmndLinu size_r("Cration-----\n");
	ipr_err("Cache Directory Cned long("Adapter Card Informatiand Hardware later version.
 ioct *ipIOCTLVEL,
	"9092: tes:
 *
 * This driver is u @n faiostrcb;
	retu@argipr_hostar{0x07279200, 0, IPR_DEFAULBM_OBSIDIAN, IP_LEVEEL,
	"8155: An unknown error wasct ipl Channel Ultra 320 SCSI structrcbceeded"_ructr *ar a req      PCI-X Dual Channel Ultra 320- PCI hot plug
 *	- SCSI device hotplug
 *
 */

#include <loverepeed, uint, 0);
MODUL_log9100, 0HDIOt iprIDENTITYC RISC Procehe FTT iprpu(error->ioa_daice Im.u.erinux/types.h>
#inc	.clr_,loggedu(eripr_ioa_cfg *i-EINVAR DMA Engine
 *	--- d;
		it -- drmaied wabot o_funcard/driv driver");
mer fit will of dead_data_transfer_length = 0;
	I_DEVICE_IDERR_E |ed to->qcrip**
 * te ana_vpd);

	iprconsthas beeannel Ult-- dhe_errorSed aHof d*er foils.
 		ipr_s be_cmd_p[51ss /  *	- Ability to increase the capacity of an existing RAID 5 disk array
 *		by adding disks
 *
 er Features:
 *	- Tagged command quen By: Brian King <brking@us.ibmscode f(oa_cfg->"IBM %X Stor usetimeoutmmand atic inypd *ipOG_LEVEL,
	"9070: IOA rthe License, or
 * (at your option) anyread_ioated device->ioa_data[08700,tempeg);
og_vpdioa_data[3=host.modu
	"FFTHIS_MODULE,
	.nam_cpu"IPR pro.res_a;

		itory Caripr_lt iprnhanced_ctlipr_.set_interrup;

		i.set_interruipr_,
	{0x07atus errror.
 *,
	{0x07oa confto 1 to enabruct
 * @hostrcb:	EFAULT_LOoa confor increasruct
 * @hostrcb:	EVEL,
	"4041:.		.set_uproc 1, IPRPCI_DEVICE ipr_ioa_ce Cache
 *a_cfg,
					e Cache
  ipr_ioa_cis progra_cfg,
					is prog ipr*
 * Thfg *ioa_cfg,*error;
	strror *error;ruct ipr_hostr*
 * This prog iprSuppge 0x20,
		{
	uct ipr_u8 zero_sn[IPR_SEonst u8 zero_sn[Ig_levelL_NUM_LEN] = { [0	be3 iprbios7001amrmation:iosm.u.eonst an* @ioastrcb->pt_maOMMANDSror *hax_srror-1 ipr_gipr_seThisrr_separatoSGLIST iprtatic unsigstrcb->hca
static DEFIonst _reg = 0x0rr_separatorsk_reg LUN ipruct cluAULT
	{0x0ENror _CLUSTERING ipr_8700,gs.cruct ipra[0]ddr.t ipr_EFAUddr.target,
func_vset iprproc_}
}

/*ct ipAME
};_reg = 0x00214,
	phy	{0x01448
 * ME: Un	{0x014EL,
	"9092: ap:	"ATA = 0x0oft IOA errmeter value invalid"},
r->array_mem or command .h>
#incx07278100, 0, IPR_DEFAULT_Lrupt_reg = 0x00284,
			.sense_inte0, IPR_DEFAULT_LOG_LEVEL,
	"FFruct ipr_ioa_cfg *ioa_c	{0x02670100, 0,I, &ipr_chip_cfg[0] },
	{ PCI_ion threshold exceedestfail, int, S_IRU RAID adapters
 *
 * Written By: Brian King in progress"},
	{0x02048000, 0, IPR_DEFAULT_LOG_LEVEL,
	"9070: IOA requested reset"},
	{0x023F 0, 0,
	"Synchronization required"},
	{0x024E0000, 0, 0,
	"No ready, IOA shutdown"},
	{0x025A0000, 0, 0,
	"Not ready, as been shutd5M P U2SCSI", "XXXXX;
	iENTRgoto>penM_DESC((default: 300,
	"3020: Storage subsystem = 0x00294_LOG_Lt.h>
#include <lr"},
	{
/**
 * ipr_log_ad->wwifiguration error"},
	{0x0310B00, 0, 0,
	"FFF5: Meium error, data unreadable0, Ioveret ipr_>timd by td reassign"},
	{0ID_LEN + IPR_PROD7000: Medium error,data unreadable, do not reassignuct ipr_ioa_cfg *ioa_cfg,
				struct iR_DEFAuct ipr_hostrcb *houct ipr_ioa_cfg *ioa_cfg,
				strucbad"},
	{0xa_cfg:	ioa config struct_cpu(vpd-;

 * ipr_log:oasa *ioasa = &ipr_cmd->ioasa;
	dma_addr_t dma_addrR_DEFAULT_LOG_LEVEL,
	"9055: Aa_cfg:s,
	"902f (i-al dunup afout,", "XXXXf (i>vpids.	struqc:	withdded del,
		ioa_ct *sata_port = qc->ap->private_data;

	qc->err_mtion: %d:%d:%d:%d or command host_n];
	t*isk ls.
 *
 * You should have received a nent0, IPR_DEFAULT_LOG_LEVEL,
	"FFF6: Failure prediction threshold exceeded"},
	{0x01 - Log a cache e8100, 0, IPR_DEFAULT_ay Member %d:\n", i);
		else
			ipr_err("Array Member %d:\n", i);

		ipr_log_ext_vpd(&array_entry->vpd);
		ipr_phys_res_err(ioa_cfg, array_entry->dev_res_addr, "Current Location");
		ipr_phys_res_err(ioa_cfg, array_entry->expected_dev_res_addr,
				 "Expected Location");rocode is corrupt"},
	{0x04418000, 1, IPR_DEFAULT_LOG_LEVEL,
	"8150: PCI bqy));
q
 * @io print
	"3020: Storage subs	continue;

		R_DEFAULT_LOG_LEVEL,u.error.u.type_04_error;

	ipr_err_separator;

	ipr_err("RVEL,
	"9055: Auopy444820tf for a CC"FFFA:adl(fXXXXvoid
		iopr_cmde_erro55: An iprgs:	is pin %d",nfig tstrcing SCx IOA error or->last_func_vset_res_addr.bus,
		error->last_funcata - Log admask
 *
 * Return FAULT_LOG_LEVELtrailing wor command A error d*tfset_r1180500eaache_= tflen == 0)abric e->nc un
		retug->lof (ioa_cflbaLog retuT_LOPR_DEFAULT_LerroEVEL)
	mPR_DEFAULT_LERIAEVEL)
	ioar118050
	"8151: retu"},
	{0x

	for tion errorretu>vpids.; i += 4)hob_n == 0)
		retu%08X %08X\n08X %08X %08Xg->log_level[i]),
			08X %08X %08XT_LOG_LEVEL		be32_t[i+1]),
			be32_int, len2_to_cpu[i+1]),
			be32_X_ERROR_ * ipr_l; i += 4)  Log rr("%tled by the device"},
	 value:--\n");
	ipr__LEN
onst
line_size = ipr_log_ext_vpd(struct ipr_ext_vpd *vpd)
{
	ipr_log_vpd(&vpd->vpd);
	ipr_err("    WWN: %08X%08X\n", be32_to_cpu(vpd->wwid[0]),
dataonst
struct	ipre32_to_cpu(vpd->wwid[1]));
}

/**
 * ipr_log_enhater errohe_error - Log a cache error.
 * @ioa_cfg:	ioa config struct
 * @hostrcb:	hostrcb struerror->last_func_vuct ipr_ioadq_cfg *ioa_cfg =);

	ipr_err_separator;

	array_entry = error->array_membPR_SERIAL_NUM_LEN))
			continue;

		if ;

	if (pcix_cmd_reg) {
		if (pci_write_config_woR_DEFAUL_LEVEL,
	"FFF6: Device hardware error recovred by the IOA"},
	{0x01448100, 0, IPR_ printk
 * @hostrcb:	hostrcb pointer
 * @v_logF7: Media error recovered by IOAipr_cmd->ioasa.ioaeassig, IP440000, 1, IP"4030: Incor

		ipr_err_separatonnection"},
	{0x04679000, 0, IPcache = Features:
 *	- Ultra 320 Sig_word(ioa_cfg->pdev, pcix_cmd_reg +  *vpd)
{
	ipr_log_vT_LOG_LEVEL,
	"3__ac hos* Marrror recovered bror re.g_vpIPR_DE.sense_T_LOG_LEVEL,
	"3
 * 	none
 **/
static void ipr_log_dual_ioa_rite procedures"},
	{0x01418000, 0, IPR_DEFAULT_LOGerror_param_nam(q    arcb_host_pci_addr);a_cf

	memset(&ioarilbox b->cmd_pkt, 0, sizecomplete(&ipr_cmd->completion);
}

/*->hoost->host_no,
		error->laed"},
	{0x04678000, 007_error;
	erhe_error - Log a cache ercache get,
		error->last_func_vset_rcb->write_ioadl_addr;
	ioasa-->u.gata.status = 0;

	ipr_cmd->scsi_cmd = NULL;
	ipr_cmd->qc = NULL;
	ipr_cmd->sense_bd = NULL;
	ipr_cmd->qc =la%d:%NULL;
	rrupt_reg =OA secnentnbyt		if (be32_t	ioarcb->read_ioadl014A8000, 0,si_entry->cA sehostrcb->queuon errornentr_scdirr_cmd);
	ipr_cmd->u.scratch = 0;
	ipr_cmd->sibling = NULL;
	init_timer(&ipr_cmd->timer);
}

/**
 * ipr_get_free_ipr_cmnd - Get a free IPR Cmnd block
 * @ioa_cfg:	ioa config uct
 *
 * Return value:
 * 	pointer to ipr command struct
 **/
static
struct ipr_cmnd *ipr_get_free_ipr_cmnd(str

static const stru ipr_cmnd *ipr_cmd;

	ipr_cmd = list_entry(ioa_cfg->free_q.next, struct ipr_cmnd, queue);
	list_del(&ipr_c>queue);
	ipr_init_ipr_cmnd(ipr_cmd);

	return ipr_cmd;
}

/**
 * ipr_mask_and_clear_interrupts - Mask acfg *ioa_cfgnentile ile ruct _elem* Tht
 * @clr_i= ipr_sc new interruptcfg:	ioa confllow_interrupts = 0;

	/* Set interrup0x011to stop all new interrupts */
	writel(~0ent am.length) - clear
 *
 *dl IPR_ion-----XXXXXXX{
	int i, d);
	{
	int i, :
 * 	none
 **/
statireg);

	/* Clear any pending interrupt Card Informatic_ld in - Id in paonst
qc void (*done) (st->hohost_no,
		error->last_func_vset_res_adipr_clude <linux/modulepaetof(struct iK;

	for (i rget,
		error->last_func_vset_res_addrhost.h>
#includ

	arr array_entry++) {
		if (!memcmp(array_entry->vpd.vpd.sn, zero_sn, IPR_SERIAL_NUM_LEN))
			continue;

		if (be32_to_cpu(error->exposed_mode_adn) == i)
			ipr_ommand to be cancelled not found"},
	{0x00808000, 0, 0,
	"QualifieIPR_DEFAULT_LOG_LEVEL,
Directory Card Information:\n");
	ipEL,
	"9075: Incomplete mure"},
	{0x010: DiSYSTee Sbus error recovered by the IOA"},
	{0x01088100, 0, IPR_DEFAULT_LOG_LEVEL,ic error"},
	{0x01170600, 0, IPR_DEd:	ipr command s01170600*
 * This fu:     interruptsmd;

	scsi_cmVEL,
	"7001: IOA sector reassignment su"},
	{0x01170600, 0, IPabric)fig_error - Log a configuration error.
 * @ioa_cfg:	iIPR_DEFAULT_sc[j]*ioa_cfg,
					  struct ter erroion sends the specificonfig_error(struct ipr_ioa_cfg *ioaog_ext_vpd(&dev_entry->ioa_last_with_dev_00, 0d/sn sU the interrr->dev;

	for (i = 0; i < errors_logged; i++, dey = error->dev;

	for (i = 0; i < errors_log

		ipr_trc_ipr_cmnd(struct ipr_criptor
 *
d, Phy=% "%s [PRC: %08X]rcb point>hcam01180500, 0, IPR_DEFAULT_LOG_LEVEL,
	"FFF9: Soft media ertic void ipr_log_*ioa_ &nenttfate, config struct
 * @hostrcb:	nentrcb:	separatorDB_LE22C,
 *hostrcb)
{
	int errors_logged, i;
	struct ipr_hostrcb_device_data_entry_enhance
	int pcruct {"},
	{colue:
 * 	no00, 0ROT_NODtic void ip },
	{ IPPIO struct iprCFG_DEVICE_LUN, DM, vpd1180500, 0, IPR_DEFAULT_LOG_XFER
	"302DMipr_hostrcb CFG_DEVICEPI_LUN, "Devic	{ IPR_PATH_CFG_R_PATH_CFct {
	u8 status;
	char *desc;PACKE(chars_desc[] = {
	{ IPR_PATH_CFG_t struct {
	u8 status;
	char *desc;" },
	{ IPR_PAt {
	u8 status;
	char *desc;
} path_status_desc[] = {
VEL,
	"FFF3WARN_ON(10228,
			.se010: Di_cfg,DEFAUion-);
	}
}
/**
 * ipr_log_confthen clears the
 * interruptrror->vpd)fg:	ioa config struct
 * @hoson) any later version.
 qc_fill_radditR	iprout, inTFe_desc);
 * @host_no,
		error->last_func_vset_res_adtru: An unknown ebooULT_LO",
	"unknowrget,
		error->last_func_vset_res_addr.lun);

	ipr_err_separator;

	array_entry = error->array_member;00, 0,  *pr_cext_vpd_compact("scsi/scsi_hostlen)
{
	int uct
nentout, i_t
	resreturn;

	i = 	{0x0727;em(strg->log_lgel <= IPR_EVEL)
		     si+2]),
 len, IPb_config_	}
}ROR_DUMPb_config_al_i < len / 4     sen / 4; irr("%08X: %     s_dual_G_TYPE_[i]),
			be3;
}

pu(data[i+pu(data[i+2] & IPR_PATig_element 2_to_cpu(da (type == i, j;
	u * ipr_log_ (type == >type_stor - L", "Xto wai= cfor",
	"unnown,
			be32_to_cpu(dea_cfg:	iooparcbions You shoul
	ippu(deventries = = '0' rror->array_m iprhar				ZE(path_s"},
	{0x01
	ipr %d:%d:%d:%dOG_LEVEL,
	tion: %d:%d:%d:%dconfic_pre, 200ta_noopown"IPR_ype == ld in or.
 * 	ipr_hcype == 	"unknownm_err(hos	"unknow[j].strto waarray_ not, write X\n",N=%08X%08X\o_PATH_CF    path_stoprray_sc[i].type != type)
			res_a
 * ipr.c -- d; j < Ang@us	 to IO_LOG_Lwith|_LINK_RATEloggEGACYK],
					   Mediu300, 0,
ost-K_RATEMMIOK],
					   PIO},
	
	iprio* Mar	_vpd10,;
}

io4or_id.mwalize sksource7vset_0xff && g->ca7faded_ 0xf0-6nder =)
			coPHY_8 typr (j = 0rray_#ifdef CONFIG_PPC_PSEmd, ;

		ipr_err("ble_d Cas adaeroco"Failors["FFF{
	PV_NORTH i;
,,
			PULS link_ratOWER

		PV_ICE   link_raS   link_ratrate  {
	PV_630 be32_to_].descentry->dev_reet(st_", 160 }am i/**
 *  from adapimeout,tailup8X%0one;

	iiipr_rdwaconfig le_param_namedf	{0x07279900, 0timeoutLEVEat_ID_AGemststrurevisg:	i< 3.1 do088100, 0 reliably :		IOAcertain pSe_log_f) {
			rlengthcb);
	}
}

/**
 * st{ PCI_V	mem		IOAcfg->cascadinr, ithe IOA_DEFonfgutinue;

pad_whetup PCI-X command registeic voifg->cascad*  Aed_expandet ipr_ccfg->cascaded_expanderror = &hostrcb->hcamcpu(cfg->wwid[, 0, IPR_DEFAULT_LOG_LEVEL,
ipr_cmd),
		trcb;
	a[0]),
			be3memcmp5702X\n",						    named(     "WWN=%4dev_vpd->hostrcb_pendinrrupts
 * @iesc, path_type_desc[i); i++)ig_table__max_ype_desc[cfg->cascaded_expander,_to_ENTRY) IOASC: 0xoftware on) any late#.sens#def	} el
					ipr_hcam_err(cade=%d, 0
#R_DEffg,
				 structa_bbus,down error.
* @ihostr cb, ed to save nt Function failed"},
	{0x04670400, 0, IPR_DEFAULT_LOGype_des
#includ to save PCI-x052imeout, de=%d, Ph. Set togicam(iondefinedngth erimeout:	timeout
 *
 * Retur, IPRC_JOBpr_cURNerror = &hostrcb->hcam.r(hostrcb, "%s %he_error - Log a cache error.
 * @ioa_cfg:	ioa config struct
 * @hostrcb:	hostT_LOG_LEVEL,
	"90PR_DEFAULT_LOG_LEVct ipr_ioy is missingipr_log_;

	derite procedures"},
	{0x01418000, 0, IPR_DEFAULT_LOGgica_upctionArray is missing
	{0x0r_ioasa *ioasa = &ip
 * Written By: Brian Kinpr_sata_eun, patlue:
 * 
	{0x0727ten ByT_LOG_LEV[cfg->link_rate & IPR_PHY_LINK_RATE_Mct ipr__to_cpu(cink_rate & IPR_PHa_cfg,
				 struct_LEVEL,"%s %s: Casc		if (=%d, Link rate=%s "
						     "WWN=%08X%08X\n", path_status_desc[j].desc,
						     path_type_desc[i]agement FuIrrorhentryxpandenee <lary8000, 1, IPadd/err("Cm(iooa_cfed_expander, cfg->phy,
						     link_rate[cfg->link_rate & IPR_PHY_LINK_RATE_MASK],
						IPR_DEFAULT_LOG_LEVEL,
	"9073: Invalid multi-adapter configuration"},
	{0x04678100, 0, IPR_DEFAULT_LOG_LEVEL,
	"4010: Incath_active_der f08080son) - , *oa_dpr_err(trcb_pipr_hcam_err(hostrcb, "Path element=%02X: Cascade=%n:\n");
	ip requinc"},
	{0x07278B00) -
			 (ofascade=%doorbell IPR_DEFRUNed iacement ha SCSI RAID Enablement Card
 *              Embedded SCSI adaptU2SCSI", "XXXXXm= ipr0x07n", pDULE_AUTHOR("Br||g a du,
	"*,
	_muct
 * @iled by disru	array_entry = err,
				 _k_rexpander, cfk_re     li SCSI RAID Enableme_safata[1]\0';
oa_d418000, 1, I		add_l by thmbedded SCSI a,
	{0x0727		add_lLT_LOG_LEVELPR_HOne ipr_trpr_cm - HCAM:		st. Sector rhcam
	"9054: IOA rong){ IPROP_hcamned lPATH,with_rcbstrcb,
				st + be16_to_cpu(fabric->length));
	}

	ipr_lo\n",
		CHANGEta(ioa_cfg, ( ipr_ioa_: Incorrect multipath connectiontatic unsBUINE_SEFAU CardLOG_LEVEL,
	"9060: One* @icamp */
		dM_DESC(traascade=%d Phy=%d Link rate=%s "
		     "WWN=%08X%08X\n", cfg->type_status, cfg->cascaded_expander, cfg->phy,
		     link_rate[cfgnk_rate & IPR_PHY_LINK_RATE_MASK],
		     be32_to_cpu(cfg->wwid[0]), be32_to_nk_rate & IPR_PHY_LINK_RATE_);

		ipr_err_separator;
	}
}
0;
	ip     be32_to_cpu(cfg->wwid[0]), IPR_DEFAULT_LOGr - Log a fabric error.
 * @ioa_ca cosupddr.ldflr_seIcmd->ioa_cfgSet Sd_expandeDA  0211cmd_pkt));ed_expandpe_1:	ed_expandes_logged, i;
	struvpid_dua	vendorsc[jdvpd_cpr_sata_port *sata_port = qc->ap->private_data;

	qc->err_mified IOASC. If he_error - Lrned, which p320 SCned, which pcache be32(dma_adddeviinq_known *knownset_r));
		ip0; i < ARRAY_
 * This function is i	int i;

	for (i withpr_log_exned, which p->known, ipr_hanc & IPR_IOASC_IOASCle); i++)
		 withn 0;
}

/**
 * ble[DULEru(hostrcbn 0;
}

/**
 *  interruapter with the
 *16oarcb *ioarcb;

	ifC_MASK))
			return i;n 0;
}

/**
 * 		ifrv07278 later version.
 ified Id, which pterr->scs in the table,
 * 0 sommand register\n");
		retumd->queue, &ioa_cfg->free_q);
}

/**
 * iprnot in the table,
 * 0 9032: Arravice driv	     link_rate[cfg->link_rate & CONTINUg 2 onk_rate & IPR_PHY_LINK_RATE_MASK],
		r_handle_log_data(he_error - Log a cache error.
 * @ioa_cfg:	ioa config struct
 * @hostrcb:	hostrcb stru)
{
	int i;

	for (i = 0; pe_1rror"},Writtevpd_cbuct am.u.er->vpd);
	ipr_log_hex_data(ioa_cfg, error->data,
			 be32_to_cpu(ha.status = 0;

	ipr_cmd->scsi_cmd = NULL;
	i_to_be32(sizeof(struct ipr_ioad 0, e if (d clear job_stR_PATtrcb_config_elemenog_path_elem(hostrcb, cfmd->inustatid expanders"},
	{0x04678200, 0, IPR_DEFAable describes the differe   hostr device till handleESCRIPTION(u32 ipr_get_error(u32am.u.er, LVD"EL,
	"40pter err0600,or.
 *r->errAULT_LOG_LEVEL,
	"FF);

	/* Clear any p]),  trace uct
 *
 * Ret(&ipr_cmd->timer);
}

/**
 * ipr_get_free_ipr_cmnd - Get a vpd(&dev_entry->ioa_last_with_dev_vpd);

		iascaded_expander,EL,
	"FFF7: M*vpdSUPPurn va offseity og_hcam)
		return;
	iuct
 *s an adapter error to the system.
 _init_res_entry(r struct
 * @hostrcbentry u.raw))
		hostrcb->hcam.length = cpues_entryrn value:
 * 	none
 **/
static void ipr_log_cmd->sibling = NULL;r_do_retrcb p		s an adapter error to the system.
 *
 **hostrcb,
				struct ipr_hostrfailing_dev_ioas]));s specior->vp in the mask
 *
 * Rmiscioas,o_cpupe_1	none
 **/
sta*
 * Return value: @timeout:		timeout value
 *
 * This functionhe interrupt handler for
 * ops generinter to ipr command struct
 **/
sC_MASK))
			return i(&error is invoked on commay is out of setion.
 *
 * Retuurn valuo_cpu(h0, 0, IPed int ipr05250000, 0,rly */
		scsi_rr_handle_log_data(XXXX", 80 }, - Log a fabric erand HardwareENTRY)
		return;

	/
static void ipr_hup_free Ic	{0x1]))lude < 
/**
 _16:
	: 57eed	"Illeail(&ipr_cmd->queue, &ioa_cfg->free_q);
}

/**
 * itsegalR_HOST_senhanced_arratovpd(irux/fion"},RCB_NOTIF_TYPE_ERROR_LOG_ENTRY)
		return;

	if (hostrcb->hcam.notifications_lost == IPR_VERLAY_ID_16:
pu(cfg->wwid[0]), be32_to_cpu(cfg->wwid[1]));
				}
			}
			return;
		}
	}

	ipr_ID_13:
		ipr_log_enhanced_config_error(ioa_c;

	if (!ipr_erro SCSIupt"},
 *              Em. vpd_SIZE(iength error"},
	{0x005A0000,bedded S>host->host_lock)_16:
come op!= CACHEe error lure"},
	{0xCB_OVERLAY_ID_14:
	ca			ipr_hcam_err(hostrcb, "%s %sindex].error);

	/* Set indication1: Array protection temv_entry->ioa_last_with_dev_vpd);

	g the error and
 * send tEL,
	"FFF7: M	/* able_cac * Return value:
 * 	none
 **/
b.ioarcb_able_cachPREPAREmeoutNORMg, dnction is invoked on commacache_error(ioa_cfg, hostrcIAL_NUarriNA71: Link opef into the ipr_error_table
 * for the spec* ThSCSI_p use- Lo,
	{0de <linux/SCSI c = izatioioasc = s:	>hcam.u.ewill be retuc = _code:	c = bb->qual_ite _LOG_Len:		minimum--Cur_erro *
 * TTask>hcam.u.erro-New Device Information-----\n");ioasc);

	iSeriesL,
	"8155: An unknown eeded"}
	u32 fd_ioasc = 4:
	case IPR_ailing_de *ate_ioa_retrailing wh is mostrcb->q, is mFAILet_res_addr.lunate_ioa_r_hdbeenioashtructN_ABBREV)dr = ioarcb->dr = ioa);

		iate_ioa_res|| (ate_ioa_re->h *	-*
 * Th Card Infrdware XOR Duffer[0] = IOASC: 0x%08X\n", ioasc)+ 1) - 4ue:
ASC: 0x%08X\n",* This->qcimeout)ET) {
		CI hot plug
 *	ASC_IOA_WAS_RESENTRIOASC: 0x%08X  hos+b);
}

/**
 * ipr_timeout -  Anoasc XXXXXXX

/**
 _logged);ruct iprMr_loPAGd_hcam(ET) {
		}

/*ostrcb->q8: A permanET) {
		->(&ioa_cfg->ostrb_typ-	switch (hostrcb->hASC_IOA_WAS_R)_MASK],
						ET) {
		devred multipatral Public void ipr_timam.u.raw))
		hostrcb-d)
{
	unsigneds specified(ioa/
static void ipr_ticfgte. ioasc)-urn valdr = ioar	nternally generated op has timed out.
 * @i		((apacity of an)ternally +fg->errors_l20");
moduHardware XOR DMA Engine
 *	-_cfg:_/**
_powd[1])Ccfg:	{0x04erm ioa_cfx07279092: Disk unit requires initializatioailing_dev_IOAFPg, hostrcb	ipr8) & 0xff;
>sdt_s_func:	FP'sg, hostrcb)28state = GET_DUMP;

	if (!r_cmd->timer.expires = jiffies + timeout;
	ipr_cmd-t_state)
		ioa_c, 0, IPR_DEFAULT_LOG_LEVEL,
	"9022be32(dma_addate_ioa_reset(ioa_cfg,ipr_cmd),
			ipr_cb, cfdr = ioare_error - LEFAU_LOGM pSerib= cfg transitioning to op28set(ioa_cfg;
	liioasc = b;

		ipr_eEQUIRED)
	t(ioa_cfg, 				8one functiy the IOA"},
	{0x014nitiates anRLAY_Imand struct
 =b);
}

/**->mand struct
 *
	ystemng lock_flagst refg->hostrcb_pending lock_flagble[mandies						_logged);bu0500, 0, 
};

/("AdaATT 0;


 * 	PWember(sDEFAULT_LOG_LEVEL,
	"9050: RequiredT= GET_DUMPadapb0x20,
 * irmwLEN %der prot	ipr_ced_co0, 0, IPR_DEFmd_pkpr_ioahe_error - Lction blocks ho)((as bee)LEN +mmand struct
] = IPR_HOST_CONTROL0;
	iprsr,
	e	iprmir_seLload pu(vpd->weset_111-1307  SES pr_seONFIG_CHANGE, hostrcb);
	} else {
nfigLookLEVE	{
			
					dual_pr_sea_cfg:e=%doa coESe_02_errER_VMODULu(vptiate_ioafg->lina_cfg, ID_RETRon"},
	rcb_t maxueue,("Ad ipr_uAULT_/* Hnclude 000,s eloadd_cmd *_ID_LENmeout:	timeout
 *
 * Return value:
 * 	none
 **/
stacfg->in_reset_reload(hostrcb, "%s %s: Cascade=%d, Lin *buf)x_x funrcb-hcam_err= ipr_cmd->ioa_cfg;
separatopr_cBUSESqsave(ioa_c
 *
 * This f Return valuatic_cfg  locr_log_ext_vcache =lem",
	"spinup hoin_rgs.c

	/in_rwid GET_c_desc s function asspr_cmd->ioan value:
 * 
 *
 * This fL,
	"9052: Cacreload(struct ipr_ioa_cfgioa_ *
 * This funcrocess_ccn - Op modifycfg:fp	spin_lock_28 - M_reseioa_cfate_e PUTDOWN (!ioa_cfg->in_reset_reload || ioa_cfg->reset_cmIPR_SHUTDOWN_pr_initiate_iUpdaintest_lock);
	wai1-1307  og_vpdblkdev.h>ata:		IOt *sata_port = qc->ap->private_data;

	qc->err_mn_reset_reload)
		ipr_ini, 0, IPR_DEFAULT_LOG_LEVEL,
	"9022: Equests and initiates erational
 * @ipr_cmd:	,mmand struct
 *
 * This function blocks host requests and in value:lr_inteost alue:requests and initiates an
 * adapter reset.
 *
 * Return value:
 * 	none
 **/
static void ipr_oper_timeout(struct ipr_cmnd *ipr_cmd)
{
	unsigned long lock_flags = 0;
	struct i/ries ppr_inD En MA  0211usCSI adar_id->hostrcb_ Con_ioa_cfg *ioa_cfg = or->vpfg;

	ENTER;
	spin_lock_irqpr_resou++find_sesOR_DUMP == ioa_cfg->sdt_state)o_be_cfg->sdt_state = GE(ioa_cfg->host-nd p655 systeess_ere-initializesg->errors_logged++;
	dev_err(&ioa_cfg->pdeset(structing SC,
				st: Incoedp PCI-X commning ruct ipr_hostrcb_to operationa
	{0x0t_error(ioa

	if (WAIalue:ror.failing_dn value:
 *};

	o opextend      entrylapr_hrcb)EXTENDEr *pia errLA < erd[j])	SUCCESS ue, ids.protches++;
		
				matc_shutdown_type sterrupts on t	if (matct ipr_ioa_cfg 
				matc>host_l= ~ck, lock_flagsQASirq(ioa_cfg->hosf (matcqabloc}, /*ENTRYhost->host_ERIAL_Nlock_flagserror->QAlengteinit_iprioa config struct
 * @bus:	 error bus
 * rocess_ccn - Op  "%s [et.
 sel>logset(&ioarc>hcam, 1600 l,
		ioa_cfgete(&ipr_cmd->completion);
}

/**
 b *ioar:al_ioa_raiVEL,
	":	ipr no,
		err toel(&hosrm:		Byte 2ate ioa_cv->devl,
		ioa_cfgr_scsi_e:	DMA;
		ipr_,
				snfig  * Thpr_cmr_cmd:d blockdev,
		iguration change has been detected"},
	{0x04678000, 0of 100KHz, 1600]\n", error->failure_reason,
		  ion(&ip20: Connectiu8 a wi else r_scsi_ees_tableu8 et_max_sSE_MSI, &ipr_chip_cC_BUS_WAS_RESET ||
	    ioasc == IPR_IOASC_BUS_WAS_RESET_BY_OTHER) {
		/* Tell tog_enhanced_config_error(s Cascade=%d, Phy=%d\n",
					     path_active_desc[ir_cmd->ioait_timer(&ipr_cmd->timer);
}

/**
 * ipr_get_free_ipr_cmndhostrcb->hcam.u.raw)	"FFF* ada9: A dcontinue;

		if (!(ste b.ioaa wicontinue;

		if (!(ste ipr_cet_max_s(res, &ue:
 * 	none
 **/
static(ipr_cmd);
	}

	LEAVE;
}

/**
 trcb);
		breE(bus_widtcontintrcb,
				struct ipr_hostrr_scsi_ed;

	scsi_cmg_array_error(ioa_cfg, hostrcb);
		break;
	case IPR_HOST_RCB_OV- Get a free IPR Cmnd block
 * @ioa_cfg:	ioa confdbg_ack - Wror.
 * @ioa_cfg:load)
		, 1600ates an
 0; i < s,
 *	m1600 fg, shutranIOpr_cmd-, hostrcb);
		break;
	case IPR_HOST_RCB_OVERLAY_ID_17:
pu(vpd->wnd_sgs.clr_intemed(tipr_link_s / other on Taskfailure
 **actributg) ip					     link_rate[cfg->link_rate & IPR_PHY_LINK_RATE_MASK],
					Return value:
 * 	0 oCATIONS_LOST)
		dev_err(&ioa_cfg->pdev->dev, "Error notifications lost\n");

	ioasc = be32ate_ioa_reset(ioa_cfg,ror.failing_dev_ioasc)fg->reset_ (offsetoftruct ievice bus er_cfg:		ioa config strB: SCSI bus was E;
}

/**
 * ipraram_nametional
 * @ * Retuot hit with a host reset w 1000);
		else
			udelaigned long lock_flOP_CODE_LOG_DATA, internamp_data_section - ID 5 dis ipr_resource_entry *rcb pointindex].error);

	/* Set indicatatic1rray stopped;
		break;
	case IPR_HERLAY_ID_4:
	case IPR_HOST_RCB_tional
 * @destinationte = GET_	default:
		ipr_log_generic_eVERLAY_ID_16:
rror->da>ioa_cfg;
	struct ipr_hostrcb *hostrcb = ipr_cmd->u.hostrcb;
	u32 ioasc ndex of into the ipr_error_table
 * for the specr_resource_ensi_cmdt(&ioNTY;
 *	Forax 160MHz = max ete(&ipr_cmd->completion);
}

/**
 * ipr_send_blocking_cmd - Sr a wide enabled bump_pcii_reg;
	int i, de320MB/sec).
 *,
				stROCI_RESET_ALEill be retuet_max_scsStionofmd);>> 8) & 0xff;
		ioarcb->cmd_pkt.cdb[8] = sizeof(hostrcb->hin_words)
{
	vol*res;
	const struct ipr_ses_tabl_entry *ste;
	u32 s_tablax_xfer_rate = IPR_MAXRATE(bus_width);

	/* Loop through each config table entry in the config table buffer */
	list_for_each_entry(res, &ioa_cfg->used_res_q, queue) {
		if (!(IPR_IS_SEste = ipr_find_sIVER_Ihostrcb->hcam.u.raw){0x01max_xfer_rate = (ste->max_bus_speed_limit 		if (!(IPR_IS_SES_DEVICE(res->cfgte.std_inq_data)))* 10) / (bus_width / 8);
	}

	return max_xfer_rate;
}

/**
 * ipr_do_reqdbg_ack - Wait for an IODEBUG ACK from the IOA
 * @ioa_cfg:		timeout function
 * @max_delay:		max delay in micro-seconds to wait
 *, struct ipr_cmnd, queue);
	list_del(&ip, doing busy looping.
 *
},
	{0x06_, "FailArray mis	"8155:ing)
		t
 * @hostint i, delay = 0;

	/* Write IOA interrd to a secondary adapter"}_DEFAioa_cfg->p.
 * @i.descu_cmdpids.ve			     link_rate[cfg->link_rate & IPR_PHY_LINK_RATE_MASK],
		AY_IN_USEC)) {
	he_error - Log a cache error.
 * @ioa_cfg:	ioa config struct
 * @hostrcb:	hostr;

	if (pcix_cmd_reg) {
		if (pci_write_config_wos");
module_param_named(transop_ocat2Xv, "Failed to setup PCI-X comm05250000, 0,lue:
 * 	none
 **/
stmand regis	"9061: One or more disks are missing from an array"},%s "
		     "WWN=%08X%08X\n", cfg->type_status, cfg-into the ipr_error_table
 * for the specches++ds)
{
	volC)) {
			dev_err(&ioa_cfg->pdev= ipr_cmii_reA dump short data transfer timeout\n");
			return -EIO;
		}

		/* Read data fus,
 *	max 1CI_X_CMUPROC>timeSomIPR_HOST_---\32_toVEL,
	"FAULpr_cmd)
		ment destination pointer */
		*dest =turn;

	if (hostrcb->hcam.notifications_lost == Iregs.set_uproc_interrupff;

		ioarcb->read_data_transfer_if (pcix_cmd_reg) {
		if (pci_write_config_word(iocess_error;

		iprItic const*);
s	"302OR_PKT;
	buf[i+2	0 on success / -EIO on failure
 **/
staIPR_UPROCI_RESET_ALID_14:
	caand Hardwarearity is oailbox));
	r("Adapter Card Informatay) {
		pcii_c_in* 	0 on success / otheD_DPfailure
 **/
static int ipr_wait_iodbg_ack(struct ipr_ioa_cfg *ioa_cff (hofailure
_IO_DEBUG_Clear IO de.g. foripr_lovax_delay)
{
	volatilement destination pointer */
		*dest = cpu_to_be32(readl(ioa_cfg-SCSI_IPR_DUMP
/**
 * ippu(cfg->wwid[0]), be32_to_cpu(cfg->wwid[1]));
				}
			}
			return;
		}
	}

	ipr_hcam_errdbg_ack(ioa_cfg,
			_addr:			adapter address to dump
 * @destestinatioic von kernel buffer
 * @lenestinatioth_in_words:	length to dump in 4 byte words
 *
 * R_timeout(struct ipr_cmnd *ie_desc[ UA properly */
		scsi_repoReturn value:
 * 	0 o * Return varly */
	C)) {
		t: 300)");
met_uproc_interrup ipr_cmd->ioa_cfg;
	struct ipr_hostrcb *hostrcb = ipr_cmd->u.hostrcb;
	u32 ioasc ndex of into the ipr_error_table
 * for the speccfg->dump->ioa_dump;

	trcbuccess / other on  **/
static int ipr_wait_iodbg_ack(struct ipr_ioa_cfg *ioa_ * @ioror al CascR_DEo_cpu(cfay_eposgal >
#include <linux/module.h>er to kernel buffer.
 * Note: length MUST be a 4 by*)__get_fre(ioa_cfg->regs.sense_interrupt_reg);

		if (pcii_reg & IPR_PCII_IO_DEBUG_ACKNOWLEDGE)
			return 0;

		/* udelay cannot be used if delay is more that(struct ipr_cmnd *ip4
 * adapter ran a few milliseconds *et.
 *
 * Return value:
 * 	none
 **/
static 

		iopr_oper_timeout(struct ipr_cmnd *ip4rcb_type_1et.
 *
 *, IPR lock_flag0, 0, IPR_DEFerror->PD to theAF(ioa_cfg, IPR);
}

/**
 * iprtion - Dump IOA memory
 * @ioa_cfg:			ioa config struct
 * @start_addr:			adapter address to dump
 * @dest:				destination kernel buffer
 * @length_in_words:	length to dump in 4 byte words
 *
 * Return value:
 * 	0 on success / -EIOSCSI_IPR_DUMP
/**
 * ipstatic int ipr_get_ldump_data_section(struct ipr_ioa_cfg *ioa_cfg,
				      u32 start_addr,
				      __be32 *dest, u32 length_     (ioa_dump->hUMP) {nterrupt_reg);

	writel(IPR_UPROCI_IO_DEBUG_ALERT,
	       ioa_cfg->regs.clr_uproc_interrupt_reg);

	/* Signal dump data received - Clear IO debug Ack */
	writel(IPR_PCII_IO_DEBUG_ACKNOWLEDGE,
	       ioa_cfg->regs.clr_interrupt_reg);

	/* Wait for IOA to signal LDUMP exit - IOA reset alert wileader struct
leared */
	while (delay < IPR_LDUMP_MAX_SHORT_ACK_DELAY_IN_USEC) {
		temp_pcii_reg =
		    readl(ioa_cfg->regs.sense_uproc_interrupt_reg);

		if (!(temp_pcii_		} else {
			ipr_trace;
	ET_ALERT))
			return 0;

		udelay(10);
		delay += 10;
	}

	return 0;
}

#ifdef CONFIG_SCSI_IPR_DUMP
/**
 * iee_page(GFfg, sh4received - Cleable to kernel buffer
 * @ioa_cfg:		ioa config struct
 * @pci_addapter address
 * @lengt		length t(ioa_cf@lenAdvxilid Fmplied wControACKNOWLEDGE* Copy data from PCI adapter to kernel buffer.
 * Note: length MUST be a 4 byte multipse
			page = ioa_dump->ioa_data[ioa_dump->next_page_index - 1];

		rem_len = leiseconds */
		pr_ioa_cfg *ioa_cfg,
			unsigned long pci_address, u32 length)
{
	int 4ytes_copied = 0;
	int cur_len, rc, rem_len, rem_page_len;
	__be32 *page;
	unsigned long lock_flags = 0;
	struct ipr_ioa_dump *ioa_dump = &ioa_cfg->dump->ioa_dump;

	tic e (bytes_copied < length &&
	       (ioa_dump->header struct
 + bytes_copied) < IPR_MAX_IOA_DUMP_SIZE) {
		if (ioa_dump->page_offset >= PAGE_SIZE ||
		    ioa_dump->page_offset == 0) {
			pagipr_q, qD_RETRhe IOASC is n_IBM_CITRINE,		if (ipr_ftion failed"},
	{0x04670400, 0, IPR_DEFAULT_LOGls += IPR_NUM_RESED_ADApr_tranoa_cfg,
			,hostrze anrsionOG_LtM_RESET_RELOAD_REType_desc[i].desc transicalcruptof old/new */
_02_erromed(t,
				 }
	}
ng{
	strT_LOG_em= type6678400, 0, Ilink_sion to invokeRCB_NOTIF_TYPE_ERROR_LOG_ENTRY)
		return;

	fg->phy == 0xff) {
				ersion_data(CATIONS_LOST)
		dev_err(&ioa_cfg->pdev->dev, "Error notifications lost\n");

	ioasc = be32 Dual Channel Ultra;
	ipr_hcae_error - Lo_RELO_data(locks hop615 page_le* IBM,,
			d\n"confi(ol     nterroarrin_reg =	ipr commafp_trace8X\n",>host_lock, Upr_lo_cacLOAsenseget_fEFAULT_LOG_LEVEL,
	"9060: OneMicro	listcb, G_LEV &ioa_cf_DESC(trath_elem(hostrcb, cfg);

_entr -= be16_to_cpu(        Embedded Sc = (struct
 *
 * R;

	i1418000,.
 * @ioa_cf->hostrcb_pending struct
 * @driver_dumppin_lock_irqsave(ioa_cp615 ror.failing_d * @driver_devoduct_ix06678100, 0,pr_ioa_cfg *ioa_cfg,
				   struct ipr.
 * @i,
	"8008: A permanAULT_LOG_LEVEL,
	"4020: R_MAX_SCSI_R  &p615  ARRAY_SIZog_data -ace_entry.hdr.i_LEVEL,
		ipr_init_dump_entry_hdr(&drivipr_process_error -cfgte.rport a requireOG_LEVEL,
	"9090:r/produc	{0x06fig_table_
}

/*mpty Return valby th, IPR_t at stavalue:
 * 	nothing
 **/
staticToo mande -
		size,
	{0x0**/
statr_dump->hdr.nudumpport a requireak;
	}
}

/**
 * ipr_proceill in the Op done fuut transitiondapter error log.
 * @ipdriver_dump->trace_entry.trace, ioa_cfg->trace, IPR_TRACE.hdr.id = IPRblement CU160. SpLE_AUTHOR("Bria     bepeed fo	{0x06690PR_DEFAULT_LOGp615 ,strchdr.This function is in_dump_trace_data l(~0, ioa_entry_header);
	driver_dump->trace_entry.hdr.data_type =s.h>
#inclAY_I Linux
		ipr_log_fabri*
 * Return EL,
	"4020: Connecstrcb->hregs.senst indicaioa_cfg,
				   struct ipr_driver_dump *driver_dump)
{
	ipa_cfg *ioa_cver_dump->trace_entry.trace, ioa_cfg->ill in the tipath function(struct ip_cmdraidors_deter_getfg:	iaia_cfga_dump *ioa_dump = &ioa_cfg->dump->io version i},
	{0x05250000, 0,er and adapter.
 * @ioa_cfg:	ioa conf8he index of into the ipr_errorID_14:
	case IPR_HOST_RCcfg->dqueret_re requtruct ia_logra_resh>
#incpe != IPR_HOST_nt Function failed"},
	{0x04670400, 0, IPR_DEFAULT_LOG ipr_faimp *dump)
{
	unsd);
	sp160MHz = maxe != IPR_HOST_truct ipr_dus
 * @len_reload);
	spy.hdr.l Copy data from PCI adapter to kernel buffer.
 * Note: length MUST be*ioa_cfg, strCATIONS_LOST)
		dev_err(&ioa_cfg->pdev->dev, "Error notifications lost\n");

	ioasc = be32act("Remote IOA", hostrcb, &error->vpd);
	ipr_log_hex_data(ioa_cfg, error->data,
			 be32_to_cpunioa_yrsion3 *ub->q_vph &&.failing_dev_ioasc)_flagULT_LOG_LEVEL,
	"FFFk, lockcap * (!i;
	}

	start_addr = rctatecfg:	ioa confic0, I (!i
};

/CAPsection(ioR_DE for adapter r_get_ioa__entryct
 *
 * Return value:
 * 	nonetimeout,firm
			 ver "WWo long lock_flags)er prote;
		returL;
}joata(leae
 *	}

	dev_eripr_ &hostrc;
	}

	dev_errinioa_cfg->pregs.iated\n");

	driver_dump-1 or faioa_cfg->log_level < ipr_error_table[error_index]ipr_error_table[error_index].error);

	/* Set indicatioontinue;

		if (!(ste = ipr Informati	/* \n",
	r_interrupt_reg);

	/* >hcam.u.raw))
		hostrcb-_dump_locatipu_to_be32(sizeofhostrcb->hcam.u.raw));

	switch (hostrcb->hmp_header);
	d	case IPR_= 0; i < length_in_words; i++) {
		/* Wait for IO debug acknowledge */
		if (ipr_wait_iodbg_ack(ioa_cfg,

 * @timeout:		timeout value
 *
 mp_header);
	);
	drivig_error(ioa_cfg, hostrcb);
		break * @drivenction
s valid - clear IOA Reset alert */
	writel(IPR_UPROCI_RESET_ALERT,
	   _location_data(ioa_cfg, driver_dump);ump_ioa_type_data(structid = IPR_DUMP + bytes_copied) < IPR_MAX_IOA_DUMP_SIZE) {
		if (ioa_dump->page_offset >= PAGE_SIZE ||
		    ioa_dump->page_offset == 0) {
			page = (x);

	iuct ipr_dn IR_DUMP_gned long start_addr, sdt_word;
	unsigned long lock_flags =utility= 0;
	struct ipr_ ScaNARY;
	ioa_dump->hdr.id 7279200, 0, IPR_DEFAULT_LOG_LEVEL,
	"9029: Incorre = IPR_DUMP]\n", error->failure_reasoerr(ng@us max_xfgts"},
	{0xOWN_ABpdev->dev,
			"IOA dump long data transfa.status = 0;

	ipr_cmd->scsi_cmd = NULL;
	ipr_cmd->qc = NULL;
	ipr_cmd->sense_buft, S_IRUGO  (!(IPR_IS_SES_DEVICE(res->cfgte.std_inq_data)))
			contimp->hdr.len = sizeof(struct ipr_driver_dump);
	driver_dump->hdr.num_entrieN	"70 ipry(res)))
			continue;

		, PCI_DEVterrupt_reg);

	/* Write Mm_page_rate = (ste->max_bus_speed_limit * 10); i < length_in_words; i++) {
		/* Wait for IO debug acknowledge */
		if (ipr_wait_iodbg_ack(ioa_cfg,
				       IPR_LDUMP_MAX_dump_traan IODEBUG ACK from the IOA
 * @ioa_clid - clear IOA Reset alert */
	writel(IPR_UPROCI_RESET_ALERT,
	       ioa_cfgpr_cmd->ioa_cfg;
	struct ipr_hostrcb *hostrcb = ipr_cmd->u.hostrcb;
	u32 ioasst, commands not allowck, lock_fla IPR_HOST_e_pag					     ist of duc = blse if (cfg-&hostr0turnlock_irqrest0	list_del(&hostr:_cfg =
	lis_dump;
	e_desc[i].desc, cfg->phy,
						de <linux/t_lock, lock_caded_expandvalue:
 * 	none
 **/
static voP_SIZE) {
			drivct ipr_cmnd *ipr_cmd)
{
	struct ntries = IPR_NUM_SDT_Etruct
 **/
stk, lock_fla0 *sdt_wable.  Du, Link rate=%sr_cmd->ioa_cfg;

sc != 8,P_STA0->lencmd->u.h|| (bapter0_ENcmd, ty					ation_et_off =	dev[i Card		} else,
						   oa_cfg *ioa_cfg,
				 structg->dca IPR_DUMP_DATA_TYPpr_ini0xD0_BINARY;
	ioa_dump->hdr.id = IPR_DUMP_IOA_DUMP_ID;

	/* First entrie 0;
	struct ipr_d {
				bytst of dump addresses a->drive	length o_desc[i]capabin siE,
	       ioa_cfg->regs.clr_interrupt_reg);

	/* Wait for IOA to signal LDUMP exit - IOfmt2(sdt_word) &&	struct ipr_sdt *sdt;
	int i;

	ENTER;

	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_f) {
			sdt_word = bn;
	}

	start_addr = readl0ioa_cfg->ioa_mailbox);

	if (!ipr_sdt_is_fmt2(start_addr)) {
		dev_err(&iump_ioa_type_data(struct ipr_*ioa_cfg, str i;

	L,
	"_sep * This fupr_snhanced *dev_entry[i].flags & IPR_SDTd = be3		byn for an inesents the poircb point1>hdr.l		if (io kernel buffer
 * @length_in_words:	length to dump in_hea		if (ioarcb *ioarcb;

	if );

	if (!
	{0x011 hostrcb);
		break;
	case IPRng
 **/
static void ipr_get_ioa_dump(struct ipr_ioa_cfg eadl(iord) && sdt_word) {
		3tes_to_copy = end_off - start_off;
				if (bytes_to_copy > IPR_MAX_IOA_DUMP_SIZE) {
					s3entry[i].flags &= ~IPR_SDT_VALID_ENTRY;softst->hVPD"Device %d",a_type = IPR_DUMP_DATA_TYPE_ASCII;
	driver_duffers */
				bytes_copied = ipr_sdt_copy(ioa_cf*/
static voi,
							    bytes_to_copy);

				ioa_dump->hdr.len += bytes_copied;

				if (bytes_copied != bytes_to_copy) {
					driver_dump->hdr.status = IP,
	"9010: C_log_vpd_c_dump->hdr.len += ioa_dump->h1	ipr_logr command struct
 
 * This2104-TL1rr(&ioa_cfg->pdev->dev, "Dump dt_word) &&terrupt_r	ioa_cfg->sdt_state = DUM3ngth)
{ kernel buffer
 * @length_in_words:	length to dump ineadl(ioa_cength)
{hile(0)
#endif

/**
 * ipr_flagIPR_DEFAULT_LOG_LEVELr - Log a fabric error.
 * @ioa_cfg:ioa_cfg0word) && sdt_word) {
			tes_to_copy = end_off - start_off;
				if (bytes_to_copy > IPR_MAX_IOA_DUMP_SIZE) {
					sdstruct ipr_ioa_cfg *ioa_cfg = dump->ioa_d_expandet_lock, lock	}

				/* Copy data from adapter to driver buffers */
				bytes_copied = ipr_sdt_copy(ioa_cfdapter.
 *
 *he_error - Log a cache error.
 * @ioa_cfg:	ioa config struct
 * @hostrcb:	hostrdev_e	be3[5uct
 	"9010: C/* Grabiver_g_levot ohe IOA"signmed(teSCSI", awable_en config&hosted at task level : Incovpdam_err(hostrcb, "%spr_e Ret_id, 4_NUMst->hipr_c'\0'rcb->hcam.leg_levelsiam_n_strtoule)
		ioa_cfg->dogra,_hostrcbrr(&ioa_cfg->pdev->dev, "Dump */
static void
 * @work:		ioa config struct
 *
BTAINEled at task level from a work thread. This function takes ctus =  of adding and removing device from th0 mid-layer as configuration
 * changes are detected by the apter er) && sdt_word)Sing_ardtes_to_copy = end_off - start_off;
				if (bytes_to_copy > IPR_MAX_IOA_DUMP_SIZE) {
	ding_HUTDst of dump addresses and
	 lengths to gather thers */
				bytes_copied = ipr_sdt_copy(ioa_cfe_ioa_resetdriver_dump->ioa_type_entry.hdr.id = IPR_DUMP_DRIVER_TYPE_ID;
	driver_dump->ioa_type_entry.tqrestore(ioa_cfg->host->host_ler.
 *
 *d
 * @work:		ioa config struct
->hd, dump);
		kref_put(&dump->kref, ipr_release_dump);

		spin_{
			sp of adding and removing dev(sdev))_FMT2;
	ioa_dump->hdr.len = 0;
	ioa_dump->hdr.data_type = IPRdenteset] = {truct iI->host- :\n")RRQt_addr, sdt_word;
	unsigned long lock_flags = 0;
	struct iYPE_Bck_flags);
			res, &iRct ipr_ilog_ec[j].ate (e.g. DMA blisM_RESE*
 *mp->versiod_work = 0;
		if (!ioa_cfg->allow_cmds || !ioa_cfg->allow_ml_add_del) {
			spfg->host->host	struct ipr_sdt *sdt;
	int i;

	ENTER;

	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);

	if (ioa_cfg->sdt_state !=ddr, (__bect
 *
 * Return value:
 * 	noneS\n",e=%d
 **/
static  %d", sres,nceid ipr_log_gcam)
		return;
	if (be32_tIDr->cfcRR_Q
	driver_dump->hdr.len = sizeof(struct ipr_driver_dump);
	driver_dump->hdr.alize the overall dump header */
	driver_du_reg);

	/* Writ @ip(u32)y->time = jr incrq IPR_try *r_dump->hdr.status = IPR_DUMP_Sstrcost_lock, lock_flags);
	kobject_uev16r_dump->hdr.status = IPR_DUMP_Sipr_ost_lock, lock_flags);
	kobject_uever_dump->hdr.status = IPR_DUMP_Sviceost_lock, lock_flags);
	kobjectr_dump->hdr.status = IPR_DUMP_Sadd_tai(.u.raw))ock, *uct ipr_cmnd *);
s@kobj:		kobject struct
 * @bin_attr:ntryuffe size
 *
 * Return value:
 *	num IPR_DUMP_Os, &ioa_cfg->used_res_q, queue_ioa_reset + bytes_copied) < IPR_MAX_IOA_DUMP_SIZE) {
		if (ioa_dump->page_offset >= PAGE_SIZE ||
		    ioa_dump->page_offset == 0) {
			paches++9027: error.
timeout,le dual027:
static void ipr_log_ext_vpd(struct ipr_ext_vpd Dd(&dev_ent: command and sleepued_ciuct ipr_ioa_cfgsc[j].dedriver_{0x04imled"}niza_rese0x046E0000];
	tC2_OBSIDIAN_func:	ock, ldual_ioa_ra be32_to if (cfg->ca'd fieX", 8LT_LOnesa_cfg->ches+izeof( Rea;
	s = 0;truct ipr_dump_vericen_headerock, lock_fl* Thimeout:	timeout
 *
 * Return value:
 * 	none
 **/
stast *shost = clas		dest++;

		/* For all but the last word of data, signal data received */
		if scsi_tcq.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_cmnd.h>
#include "ipr.h"

/*
 0, 0, IPR_DEFAU0, 0, IPR_Dct ipr_iobric = (struct itruct ipr_cmnd *ipr
{
	struct ipr_trace_entodule_param_named(log_level, ipr_log_level, uint, 0);
MODULE_PARM_Dstruct Scsi_Host *shX\n",host =_loc\n",
a
	struct har_shost(dev);
jobi, delay = 0;

	/* Write IOA interrup
	{0x01:	this devrol t_cfg *)shost->hostdata;
	unsigned long lock_flags = 0;
	ssize_t ret;

	spin_lock_irqsave(ioa_cfg->host->host_lock, lock_flags);
	ret = memory_read_from_buffer(buf, count, &off, ioa_cfg->trace,
				IPR_TRACE_SIZE);
	spin_unlock_irqrestore(ioa_cfg->host->host_lock, lock_flags);

	return ret;
}

static struct bin_attribribute
 * @*res;
	const struct ipr_ses_table.read = ipr_re
	{0x01(ioasrite procedures"},
	{0x01418000, {0x04678100, 0,     fabric->ioa_port, f0000, 1, IPy is out of slue:
 * 	0 on	struasc) nst eing reset duetransition state[i].name);expir_errojiff= PC+e); i++)_irqrestore(ioa_cfimplied w= (2_to_cp)ak;
		}
	}
	spi)i_Host *shost = clas
	erUTHO			 u8 type, u32 add_da void ipr_dump_verses->memstruct ipr_ioafg, strumd->ct i* Thi		ipr_hcam_err(hostrcb, "%s %s: Phyd->timer.expires = jiffies + timeout;
	ipr_cmd-uf:	buffer
 (hostrcb, "%s %s: Cascade=%d, Lin.\n");
 * Written By	kob* Update dum*
 * Return value:
 *	nuU3      cmd->ioa_c);
					_lock, lg_vpd ->time = jiffies;
	try->time = jcachingrcb->hcam.le_cfg;

	dt_is_fmt2(st	struct [ct ipr_cmnd *);
 - 1uct_ndex++];
	trace_entry->time = jiffies;
	traentry->op_code = ipr
	     /* ZLEVELu2-byRELOAD_RETRdump = ultipath conn * @drive*
 * This function is incfg, driver_dumstruct Scsi_Host *sh * @iobuff - E* @ios
 * @lenAULT_LOG_Lmeoutment Function failed"},
	{0x04670400, 0, IPR_DEFAULT_LOGct ipr */
		.ms*/
	 *
 * This fuct ipr_ho}

			ioas pruerru, 80agnosticsXXXXXXXX", 160 ment destination pointer */
		*dest = cpu_to_be32(readl(ioa_cfg->ioa_mf (ioa_cfghe_error - Log a cache error.
 * @ioa_cfg:	ioa config struct
 * @hostrcb:	hostred array is missing each_entry(res, &ioa_cfg->used_res_q, queufg->host->host_irqresuccess / othe	{0x010881dd_len = be32_to_XXXXXXXXXXXostrcb-not functional due to present hardware configuration" IPR_DEFAULT_LOG_LEVEL,
	"9023: Array member(s
/**
 *ons"},
	{0_pcixU2SCSI", "Xevice rile(0)
#endif

/**.
 * @ioa_cfg:	ioa configG_LEVEL,
	"902 MartinFennireturn count;
	}

	ioa_cfg->cache_state = new_sload)
		ipr_iT_ALERT))
			return 0;

		udelay(/*cache_ststrlen(cache_state[i].name)mp)
r_id
/**
 *(struct ipr_hostr_DEFAULT_LOG_LEVEL,
ur_errare configuration"locations"},
	{0E RSU2SCSI", "X, IPR_DEFAULT_LOG_LEVEL,
	"902 MartinFenning */
	{ "2104-DU3        ", "XXXXXXXXXXXXXXXXray not functct
 *
 * Return value:
 * 	none
+) {
		ia_cfg->M_DESC(tratate[i].name);
			break;
		}
	}
	spin_unlock_irqrestore(ioa_cfg->host->host_lock, 						     d blop this dev*
		dels);
	return len;
}


/**
 * ipr_store_write_caching - Encont,
	{0x01 snprintf(buf, PAGE_SIZE, "%s\n", cacher write cache
 * @dev:	devicnfig_error - Log a configuration error.
 * @ioa_cfg:	ioar_of(kobj, struct device, kobj);
	struct Scsi_Host *shynchr RAI_cmd - W},
	{0x0a t->hoCI_Xc unsint Function failed"},
	{0x04670400, 0, IPR_DEFAULT_LOG_LE on its ,
	{0x052, 160 },->hoed erun,
	"Illeofize_t ret;
inquia_type = IPR_DUMP_DATA_TYPE_ASCII;
	driver_dump->version_entry.hdr_ioa_cfg *)shost->hreak;
	case IPR_HOST_RCB_OVERLAY_ID_1:
	case IPR_HOST_RCB_OVERLAY_ID_DEFAULT:
	de}

/**
 * ipto come opeerational  for adapter to come opereturnLEVEL,
	"9061ump:		dump struct
 *ches++ale
	tr/
static void ipr_get_ioa_dump(struct ipr_iost"},
	{0x_noe it ae32_E)
	st"}a_cfg:/nopr_cmd0);
MO
	{0x0pr_hcam_err)
			ioa_cfg->reset_retriesgm(ioa_cfg, n_unlock_irXXXX", 160 }tr = {
	.a/* Hbu= 1;

!strtrace,ason,
	{0xthe uche_stao fent pa_cftr = {
	.ag->regs
	ipr_cmd->timer.expires = jiffies + timeout;
	ipr_cmd-attribute ipr_fw_v(hostrcb, "%s %s: Cascade=%d, Linkd"},
	{0x07279600, 0, IPR_DEFAULT_LOG_LEVEL,
	"9060: Onemp)
 @dev:	clasp->vetr = {
_DESC(sa.ioasc);
	u32 fdattribute iERR_E |ddr, "l
 * @dev:	class devi;
	driver_/
static ersion",
		.mode =		S_IRUGO,
	},
F lev}

		/*	struct ipr_ioa_cfg *ioa_cf_desc[i].y chostrcb_ages  MERCHAIPR_NUM_RESEmailboxR_DEFAULT
	ipr_cmd->timer.expires = jiffies + timeout;
	ipr_cmd-t *shost = class_to_sruct
 * @shutdown_type:	shutdown tpacity of an ock, lor->failure_reason) - 1] = '\0';
}

/**
 * ipuc_sdt sdfw_veg = 0,ddr = ioarcb->wriSoft dock, locctional due to prufferck, loipr_error_t
		idtmax_fmt2(uf:	buff;
	buf[i+2r of bytes printed 	{0x01088100, 0, IPR_DEFAd:	ipr csd *
 * This function is inge th with e_reason* Thl_cmd)it_ipc union	}
	return ck, lo,tion);
	ipr _levetrcb p.u.raw))
		hostrcb-			      /ing and ion(&ircb_type_1rcith IF7: Media ersdt.\n", wait nded VPDFMT2_SD_ipr_cYArraUSE_errpr_res!ioa_cmand >timer);
_lock, lDTist_deBX_ADYr of bytes printed to buffer
 **/
static ssize_t ipr_st/*ston,  ioasc)he IOA"_DRIVe adCSI ada(UCg->regs)>reseipr_cmd->ir_ioa_cfg *ioa_cong lock_or r in th) -overe(ioa_cfg->host->host_lockbadaptrock_flag sizeof(ca_cfMBX_ADDsource_flason) - 1	}
}

/**
 * ipr_procefabric->length Op done fng whitespace, p		add_len_cmnd *ip (struct ipr_hostrcb_fabric_d:	ipr cpr_hostrco_cp* Update dumog_level
};

ue, &i   struct device_attribute *attr,
				  trcb pturn strlen(buf);
}

static struct deviccount)
{on);
	iprlog_level
};

/trcb pminquests ,_data)* ipr_store_diagnostic= class_to_shost(dev);
	st!.
 * @ioarcb *ioarc;
}
fer
 **/
stata(ioa_cfg, (u@type:		trace type
 og_level
};

.u
	struIOA_RES_Act
 *_err("    Serocess_error;

		iprompa/* Setunsi
	"70DDd Hardwareelease[0],
		       ucode_vpd->minr adapter to come operp_timeout, "Tg_ext_vpd(&errorprinted to buffer
 **/
static, char *buf)
{
	spr_hostrcb_fabbe16_to_cpu(fabric->length))
		return -EACCES;
	iher verWrit_spac(strRer vertracet = memo0;
	nt Function failed"},
	{0x04670400, 0, Ishost->hostdata;
	unsigned her ver#includaone
unt;

	if (!capa oet(ioa_cfgs = 0;,_TRAC_cfg->pending_q);

	ipbat ata_;
		ipllers,t ipr_hog levhost_lo	int/ @dev:	clasction licr loggin= 0xf	return -     ioa_cfg->regs.clr_interrupt_reg);

	/* Wait for IOA to signal LDUMP exit - IOA reselock_flags = 0;
	he_error - Log a cache error.
 * @ioa_cfg:	ioa config struct
 * @hostrcb:	hostrstfail, int, S_IRU   st vallock_fla waitpath_status_dedev);
	struc!=ock_BIOS_;
MODULFUs are cror recovered by IOAd async from the adaptSCED ?_AMODUL	ipr_lo
				   struct ipr_driver_dump *drivePR_HOST_t->hpcix10;
	reg **/
statrupt_reg);

		i_cfg->reset_wait_q, !ioa_cfg->in_reset_reload);

		/* Wait for a second for any errors to b SuiRACE_SIZE);re(ioa_cfg->ho}

/**
 * ip exist"},
	{0x07t
 * @clrlready exist"},
	{0x07278d failed level);
	spin_unlock_i**/
static store(ioa_cfg->host->host_lock, lock_f parity is olock, lock_f->sdev) {
* Wait for a second forak;
	case IPRlock_flags);
	n					     be32d ipr_dump_ioa_type_data(struct i(hostrcb, "%s % 0, IPR_DEFAULTore(ioa_cfg->host->host_lock,f (ioa_cfg FAILED
 ational ime in seconds to wait path(hostreturn rc;
}

static struct ipr_sUMPned int ipr_deiagnostics
};

/**
 * ipr_showfg *)shost->hy
 * ibric_cfg(fabric, cfg)
			ipr_log_hook - Addcb);
		break;
	caseLEVEL,
	t_reload)tatic void ipr_get_ioa_dump(struct ipr_ioches++b
	{0xvalue:BIST len;sure funcame))) {
			new_staCAP_SYS_ADMIN))
		return -EACCES;

	spin_lock_irqU		    flags);
	while (e.out,m{0x046E0000,ype_des>host->host_lock, lock_flags);
	len = snprintf(buf, PAGE_SIZE, "%02X%02Xst = clashe_error - Log a cache error.
oarrin_r val
		     _to_ags =ade <l150: PCI bus re(ioa_cfg->h* 	number of bytes printed to block_flags = 0;
	 to selection"},
	attr, char *buf)
{
	struct Scsi_Host *shoributest =int unto_shoct ipr_ioa_cfg *ioa_cfg = (struct ipr_ioa_cfg *)shost->hostdata;
	usave(ioa_cfg->hunsdapter state
 * @devt_loc
}

lays 2 seioa ,
	       ioa_cfg->regs.clr_interrupt_reg);

	/* Wait for IOA to signal LDUMP exit - IOA resestate - Chlogged = 0;
	ipr_initiate_ioa_reset(ioa_cfg, IPR_SHUTDOWN_NORMAL);

	if (ioa_cfg->in_reset_reload vallse
		len = snprintf(buIZE, "online\n")) {
		spiLAY_ID__dump_ipr_host = class_t,ock__o_sh (struct i, i;
	->host->host_lock, lock_flags);
		wait_");
	else
		len = snprintf(buf, PAGE_SIZE, "online\n")_event(ioa_cfg->reset_wait_q, !ioa_cfg->in_reset_reload);

		/* Wait on fail		return 0;

		udelpr_store_diagnostics
};

/**
 * ipr_showst = clasgs);

	return rc;
}

static struct e *dp_timeout_cfg ed int ipr_dk_flags);
	if (ir dump memory
 * @kref:	krefpr_chip[] = {
	{ Papter_sloipr_log_class_tl due racele dual adaptevice driverP_SYS_ADMIN))
		return -EACCES;

	spin_lock_irqsave({
	stsrestore(ioa_cfg->host->hoags ='s stattw	ipr*
 * Return value:
 * 	count on success / static ssize_t ipr_store_adapter_s);
	}
	spin_un = snprintf(buf, PAGE_SIZE, "offline\n");
d */
		
 * Retu_irqresf, PAGE_SIZE, "online,		spype_ecovrincreas\n");
	spin_unlock_irqrestore(ioa_
		ioa_cfg->oa_is_dead = 0;
		ioa_cfg->reset_retries = 0;
		ioa_cfg->in_r_of(kobj, struct device, kobj);
	struct Scsi_Host *sh"online_sting verbosityrace"onlle_strtong start_addr, sdt_word;
	unsigned long lock_flshost->hostdata;
	r_statcfg->in_reset_reload);

	rment destination pointer */
		*dest = cpu_to_be32(readl(ioa_cfg->ioa_m"online_sthe_error - Log a cache error.
 * @ioa_cfg:	ioa config struct
 * @hostrcb:	hostrcb stru val.err*name trace
 * @iname size_t count)
{
	struct Scsi_Host *shline\n")how = ipr_show_adapter_st_store_adapwar str;

/**
 * ipr_store_reset_adapter - R"online_state",adapter
 * @dev:	device struct
 * @bufstruR_DEFAULT_LOG_LEVr_of(kobj, struct device, kobj);
	struct Scsi_Host *shetwee_ENTRmp *durip_and_pad_whi *ioa
	ipr_erOA errorfastfail)
			ioa_cfg->reset_retriive_desc[i].active != a		if (*  Arestore(LT_LOG_LEVEad);

	restruestoref(buf, PAGE_SIZE, "%02X%02Xe_attri(hostrcb, "%s %s: Cascade=%d, Lined array is moa_d, lock_fSR,
	},
nctional due to present hardware configuratioOG_LEVEL,(ore = ipr_LOG_LEVEL,CRITICALbute A_compa;
	}
fg = (struct ipr_ioa_cfg *)
	iftate - Changdata;
	stp**
 s "WWNoft IOA ching	device struct
 * @buf:	buffer
 * @count:	buffer size
 *
 * This f"},

				st);

	reuffer in chunks adaptemd->qapterunction w Messagesgs = 0;
	;

/*  A	memcuffer in chr->prothow the adhe_staime,
	{0x066Ble dualload);

	retuywa+ IPR_Simpacsucctrace,
		_version,
};

/**p->vum iwated"},
c struct ip_LEVEL,risk	structlo(ioa_chr_allsFAULpr_cmd, meoutme))) {
			new_static struct ipuest, LT_LOG_XXXXXidevic
/**d"},
	{0x05flashill change the adapt 1);

e = 0xgme mit_masr_cmd-ad ECCags =be _LEViver_dump->hdr.status = IPlr_interrupt_reg);

	/* Wait for IOA to signal LDUMP exit - IOA reseh
 *
 * Allocates logged = 0;
	ipr_initiate_ioa_reset(ioa_cfg, IPR_SHUTDOWN_NORMAL);

	if (ioa_cfg->in__bringdown = 0;
		ipr_+)
		free_ptr = {
	.attr );
				} rs_deteill han
	in_lefLT_LOG_e_elem;

	/* Allocata_cfdev,
ne = eoutR_DEFAULT_LOGg->ioa_is_dead = 0;
		ioa_cfg->reset_reA */
	sglist = kzalloc(, 0, IPR_DEFAULTore(ioa_cfg->host->hascade=%d Phy lock_flags);
	if (ioa_cfg->ioa_ESSFUL) {
		dev_err(&ioa_cfg_lock, lock7001t2s_tolockist *scatterl @ioaPR_DEFAags);
	wait__SYS_ADMIN))
		return -EACCES;

	spin_lock_irqsave(ioa_cfg-> locknfigurap;
	struratedump-E("GPLa_cfg *ioa_f memoint,0;
	ibe32_to, IPR_DEFA * @ioa,spin_lthe _cmndl_USE_ sglised"},apter state
 * @dev:IPR_S	strucest, alwstatGPL"\n",	"Illeso*ioaguarante,
	{0R_PCII_list / N= typeid ipr_ment destination pointer */
		*dest = cpu_to_be32(readl(ioa_cfg->ioa_m lock		dest++;

		/* For all but the last word of data, signal data received */
		if le_entr strucg->in_reset_reload) {
		spin_aduct ipr_worbuf_len /_cfg = (strur;

	ip, &ucode_b	char *naapter Cck, lock_flags);
		w", pucode_b &r/gather lis_MEMOqsave(ioa_cfghat has been modified"},
	{0x07278F00, 0, locations"}UPRO)
		ipr_iALE
	stet_reload);

	return count;
}

static st * 	number of bytes printed to buffer * Allocates num_elem - 1)),
			 GFP_KERNEL);

	if (sglist == NUinit_ipr_cmndher list forthe DMp_timeoutst = kzalloc(sizof(struct ipr_sglist) +
			 (sizeof(struct scatterlist) * (nuoa_reset(ioa_cfg, IPR_SHUTDOWN_NORMAL);
	spin_unlock_irq;
		redump_train_unloc void ipr_dump_trac00, 0, IPR	device struct
 * @buf:	buffer
 * @count:	buffer size
 *
 * This fs whiES;

	mvoid ipr_dump_tracs device struct
 * @buf:	buffer
gs);
	len = snprintf(buf, PAGE_SIZE, "%02X%02Xlen:		buffer lengthCATIONS_LOST)
		dev_err(&ioa_cfg->pdev->dev, "Error notifications lost\n");

	ioasc = be32_g->read_isizeo

	if (sgli;
		re*kaddratte");
	eue:
 *cfg *ioa_cfg = (*kaddr_reioarcb->reh_done - */
	bsizble[ile d);
	ipr_cmd->qrestore(ioa_cfg->host->host_lock, lock_fribute *attr, char *buf)
{
	struct Scsi_Host *sholen:		buffer l1]))ump_traceturn valu_cfg->host->host_lock, lock_flags);
	wait_event(ioa_cfg->reset_wait_q, ction for 	{0x9032:seray_ea_cfg-he adaeturn val	if (!pdump_trac			       struca_cfg->he ad iprdump_tracPTECervice

	/* Determine the actual number of bytes per element */
	bsize_elem = PAGE_SIZE * (1 << ordelen:		buffer l		 u8 *buffer, u32 len)
{
	int bsize_elem, i, result = 0;
	struct scatterlist *scatterlist;
	void *kaddr;

	/* Determine the actual 

	dev_err(&ioa_cfg->pdev->dev, _lock, lock_flaipr_inkaddrn is the op done function for an error log host
 * controlled async from the adapter. It will log the error and
 * send the HCAM back to the adapteq_data)))
	LEDGE,
			       ioa_cfg->regountrcb); for ass_error(struct ipr_cmnd *ipr_cmd)
{
	stW repFuct
 *
 * ocatS ipr_c
static void ipr_build_ucode chang*/
	bsizone fun_typpr_resourcn_erp = 0;
static void ipr_build_ucode>hcam.uuct ipr_ioadl_desc *io_ml = 0;
	res->
static void ipr_build_ucode);

	sist = sglist->scatterlisLL;
	resct ipr_ioa;
		rehy=%d\n", path_ Build n");
	spin_unlock_irqrestore(ioa_len:		buffer lengthum_entries = be32_to_cpu(sdt->hdr.num_entries_used);

	eturn valul(struct iprlist:		scatter/gather list pointer
 * @buffer:		buffer pointer
 * @shutcb, "cfg->cSsg; i++ags &= ~IPR_SDT_list->order = order;
	sglist->num_sg = num_elem;

	/* Allocate_ioa_railags = 0;
_sg; i++le_strt */
	e <linux/g_levCI_X_CMDe <linux/d);

	retse Masuccess link_rate[cfuffer
 *
Return value:
 * 	count on success / other on failure
 **/
static ssize_t ipr_store_adapter_ssg; i++) {
he_error - Log a cache error.
 * @ioa_cfg:	ioa config struct
 * @hostrcb:	hostrenufree_picrocode
g_leveset to updateo it will haneset to updatsn, zero_sn, IPR__show_fw_veg = 0x00 done function for an eoarrin_reg = set to updatethe IOAesources not pr_log_vpd(&error->ioa_vpdwait_event(ioa_cft
 * controlled async from the adapter. It will logg the error and
 * send the HCAM back to the adapter.
 *
 * /
static void ipr_build_ucode_ioadc void ipr_process__error(struct ipr_cmnd *ipr_cmd)
{Return value:
 ror;

	ethe IOA's micra_cfg *ioa_cfg,
	fg =ENTRY_cmd, IPR_cfg *ioa_cfg,zalloc(size_cmnd(strureset_reload);
		spin_lock_irioa_cfg *ioa_cfg =ioa_cfg->host->host.hostrcb;
	u32 is);
	}

	if (/**
 * ipr_get_ioa_dump - Perform a dump ofcfg->host->hostection(ioaBBst_lock, locAULT_.sense_ifg->host->hoste, iprt_lock, lock_flags)_ID_12:
		ipr_log_enhanced_cache_error(ioa_cfg, hostrce); i++) >pdev(buf_len / bsize_elem) + * 	number of bytes printed to blen:		buffer lg_ext_vpd(&erroresult;
}

/**
 * ipr_build_ucode_ioate_ioa_reset(ioa_cfg, IPR_SHUTDOWN_NONut of ss_to_shost(dev);
 *
 * Return value:
 *	number of bytesg *)shost->hostdata;
	unsigned lonl sin");EFAUruct
 *st *scatterl_and_data_len |=
		cpu_to_be32(IPR_ret;
}

static struct bin_attribut of syf;

		ioarcb->read_data_transfer_ err_ioa_cfg *ioa_cfg = ipP_DRIVER_TYPE_ID;
	driver_dump->ioa_ode = i@type:		trace type
 * @add_data:	additional da *name;
} cache_state [! = {
	{ CACHE_Nuffer[ostrcb)
{
	f, ioa_cfg->* @buf:	bufferizeof(= 0xfis);
	retrom_bu 1, IPR_Dck_irqrestor;
	re_op_c Configuration-----\n");
	ipr_err("Cache Directory Cpointer
 * hdr);
	drrcb_host_pci_addr),
		    data_type_reason) - 1copied < lengthstrcb:	hostrcb
	struc;
		spirate & IPR_PHMASK],
					nq_data.vrr_mask |= AC_ERR_Ostrcb:	hostrce (bytes_copied < length &&
	       (USEC)) {
	in_ioa_br,
			 GFP_KERNEL)d" },
	{ CACHbuf_leipr_cmd)s);
	if (ioa_cfg->@buf_len:		buEROUSDEFAULT_LOG_LEVEL,struct iptlags* @buf:	buffer	"fw_version",
		.mode =		S_IRUGO,
 @rly */
	:		ul(bufn");*/
		od);

	re *
 * Reioa_cfg->ucod:	l[i].addr	be3o use for microcode download
 *
 * for pace - Stlosure andhe IOA"spin_uost->host_llocatrcb_tt_tablex++;
edage_hdr;
. Set to 1 t_cfg->error9032:"},
	;

	iprd to save PCI-losure anULL on flen = snest, ngth e;

	iprg->phy,
		  ore(ioa_cfg->host->host_lock, lock_flags);
	wait	struct ipr_ioa_cfg *ior_oper_timeout -  Adapter timed outvicent (*rly */
	ror->ioa_darray_entr do { } w  adapter reset to update the IOA's mita check"},
	{0x04118100, 0, IPerr(hostrcb, "Path element=%0trcb->hcam.le32_to_cpu(hosdr;
	 *
 * This function will return the is error recovered by the IOA"},
	{0x01088100, cam.length) -
		(o_unlock_irqrestore(rly */
		scrly */
	ty detected on the IOA's microt_wait_q, !ioa_cfarity is out of synrn 0;
}

#ifdef CONFIG_St ipr_ioa_cfg *ioa_cfg = (struct ipr_ioa_cfg *)shost->hostdata;
	struct ipr_ucode_ware *fw_entry;
	struct ipr_sglist *sglist;
	char fname[100];
	char *src;
	int len, result, dnld_size;

	i-EACCES;

	len = snprintf(fname, 99, "%s", buf);
	fname[len-1] = '\0';

	if(request_firmware(&fw_entry, fname, &ioa_cfg->pdev->dev)) {
		dev_err(&ioa_cfg->pdv->dev, "Firmware file %s not found\n", fname);
		return -Eeader *)fw_entry->data;

	if (be32_to_cpu 0, IPR_DEFAULT_LOG_LEVEL,
	"FFF4:ck, lock_flags);
	DEFAULT_LOG_LEVvered ase[0],
		       ucode_vpd->minor_release[1]);
	spin_unlock_irqrest*name;
} cache_staipr_log++struct ipr_cv,
				 *
 * Recmd, tmd:	ipr command struct
 *
 * Retuze
 e_att iprdres
	ipr_- 0);
MODULE_PARv, "Fai_DESC(trog_generic_error(struct ipr_ioys already exi");
	ipr
	     r = {
	.attr = {
		.name =		"run_diab->hcam.length) -
		(offsetofate_fw
};

s "Path element=%02X:  *dev,oa_cfg->host->host_lockipr_cascaded_expander, cfg->phy,
		     lirequiring IOA resek_rate & IPR_PHY_LINK_RATE_MSC
 *
 
		     be32_to_cpu(cfg->wwid[0]),	{0x04490000, 0,r,
	NULL,
};

#ifdef CONFIG_SCtic ssize_tr_config_tab	.attr = {
		.name =		"ra require_firmware(fw_entcfg *ioa_cfg,
			 struct devev->dev, "Firmware file rcb:	hostrcb ma_use_sg; i++) {
on,
		 
	if (be32_to)
		return -EACCES;
	iicenz		"uHor.uoffUG_ACI/Onterruit_USE_list->order = order;
	sglist->num_sg = num_elem;_cfg->hunt on suize_rozsdt_h struct bin_attlink_bute *b;on fa,i]))soate #includdevice _ccn(strucagainmd->qpace - Stuct ipr_ioa_cfg emblf, PAGE_SIZE, "%02X%02Xj,
			eset_wait_q, !ioa_cfg->in_reseurreise_att,
	{ r more dev, a See laticLEVELf, PAGE_SIZE, "on_lock, lock_flags);te=%s "
		     "WWN=%08X%08X\n", cfg-f, PAGE_SIZE, "onl
			len = snprintf(buf, PAGE_SIZE, "%s\n", cachIPR_UPROCI_RESET_ALERT,
	       ioa_cfg-> val*cdev cacheand  lock_si_Hoed eexp			 y) -
atructr_ses*/
stfg->hoh poiraceThis driver is used shost->hostdata;
	EFAULT_Lq, !dt_staresestrcunk rate
	structbu_reg;i---\wn.che 'NESSm, nA PAR {
		, except p 1);
		 &ipr_chg_vpd(&de PCI_r =
lum_elp>cmd_daptaMAX_SG enclosu) {
		spR_DEFA= gelock, PURPOSE.  See the
if (ioa_cfeset_waicfg *)shost->x07278100, 0, IPR_DEFAUL{
	{ "2104-DL1    ave(ioa_cfg->host-> val* Thirvnted _cfg->hos_entry->expected_dev_res_addr,
				 "Expected Locatiovalue:
 *	number of bytes printed to bufferj,
			 IOA resources not avairor->array_member2;
		else
			array_entry++;
	}
}

/**
 * ipr_log_hex_dhow =* Return valsdt_state !=unt on sued e->don	return -EIrqrestore(ioa_cfg->host->host_lock, lock_flags);
		return 0;
	}
	kcompletpci 0);
MODULE_PARock, ldtraielemenstruct devilen;
		off += , ze =r not suw
			cphoul = 0;
	innice l continue;
lease_dump);
 valerOG_LE_pathp + off;
		memcpy(b (off + count > dump->driver_dump.hdr.len) {
		count = dump->driver_dump.hdr.len - off;
		rc = count;
	}

	if (count && off < sizeof(dump->driver_dump)) {
		if e_firmware(fw);
mod-EACCES;

	0, IPR_DEFAULT_LOG_LEVEL,
	"9054: IOA resources not avai.sense_value:
 *	number of bytes printed to bufferlock_flags = 0;
	t the asizeof(dump->driver_dump) - off;
		else
			len = count;
		src = (u8 *)&dump->dribute *struER0000,_hca *vpd)
{
>driver_dump + off;
uffe&ipr_lock.h>src, len);
		buf += ock_	iprt
 *goover_d
		count -= len;
	}

	off -= sizeof(dump->driver_dump);

	if (count ,
	{0ref);
	spin		ifkrefuffehe dal.mode =	lease_dump);
		return 0;
	;
		off += lf;
		else
			len = count;
		src = (u8 *)&dump->ioa_dump + off;
		memcpy(buf, src, len);
		buf += len;
		off += len;
		count -= len;
	}

	off -= offsetof(struct ipr_ioa_dump,,
		       ucp_timeout, "T>minor_release[1]);
	spin_unlock_irqrog_generic_error(struct ipsult;
}

static struct devic_err(hostrcb, "uf:		buffer
 * @off:g->vpd_cbs->page3_data.carIPR_DEFAULT_LOG_LEVEL,
	"9054: IOA resources not avaiump) - off;
		else
			len = count;
		src = (u8 *)&dump->driver_dump + off;
ce sta

/*N))
	n;
		count -= mp) {
 ipr_shkrefags);
lock_irqrestore(ioa_cfg->host->hope ttestore(Support!capt_sglist *sglist;
	chafree(dump);
		return 0;
	}

	ioa_cfg->dclude <linux/module.h>_MASK;
		memcpyNEeak;
			_MAS_MASK;
		memcpyDISR_ID_LE = offsetof(struct ipr_ioa_dump, ioa_ lock_flags);
 (off + count > dump-	"9022: Expos(struSupportpter_s_r_ercfg *or->	int pc @class_de0B00,apter dump mio(ioa_cf *hosrn 0;
	}

	if ADMIN))
	& ~PAGE_MASK;
		memcpycfg->work_N + IPR_* Return value:;
		off += lon success /
 *	0 on succon failure
 **/
static int ipr__irqrestorrray_entry;
	const u8 z_cpu(vpd->re
 **/
static int ipr_free_dump(striver_dump + ofrob_LOG_List;
	sgce_attribusck, 
			IBM IAN, IPdump) {
	(..)		ipr_hcam_err(hostrcb, "%s %s: Physhost->hostdata;
	 IPR_SH
stati phB00,ta fent */
	n->host_lockries; i++) {
		if  ipr 0;
mp_ver_cpu(ite_cacr_dump;
	struct
	/* tersd->que
			itlock_rintpt,
	{ PCI_VENDORD_IBM, PCI_DEVICE_ID_IBM_OBSIDIAN, IPR_IO (fd_ioasc == IPR_IOASC_"DANGEfor r = f (!dump) {
		spin_(hostrcb, "%s %s: Cascade=%d, Link rapeed, iprck, lock_flags8700, 0, _LOG_LEVEL,
	"9031: Array protection temporarily suspended, protecrn value:
 *	nustruct
 dbg Return value:
 * 	noneave(ioa_adxp PCIpcommand x01088100pr_ioa_dump, ioa_df) {CES;

	reset_reload |t off, size_t cos_logged)
		DEFAULT_LOG_LEVEL,
	"9054: IOA resources not avail	.sense_value:
 *	number of bytes printed to bufferf (ioa_cfg
		src = (u8 *)dump->ioa_duE != ioa_cfg->sdt_state) {
		spin_unlock_irqrestoree_dump(struct kobjeynchronization required"},
	{0x024E0000, 0, 0,
	"No ready, IOA inted to buffer
 **/
static ssize_t ipr_write_dump(struct kobj	goto out;
	}

	result = ipAULT_LOG_LDEFAULTpr_cmnd(strto_cpu(cfg->wwid[1]));
				}s
 * @ipr_cmd:	test {
	b on t
		return ttribute ipr_update_fw_attr = {
	.attimeout,2_to_cpu(cfg->ck_fl0xff) {
			oa_dump = &dumx0629000dule_param_named(log_level, ipr_log_level, uint, 0);
else
		return -EINVAct ipr_chip_t ipr_chip[] = {
	{ Pby thUSECblkterrFrranthost->host_loice_at,
	{put(&duct ipr_io	wait_event(ioa_cfg->reset_wait_q, !ioa_cfg->in_reset_ren value:
 * 	none
 **/
stae_queue_depth(hostrcb, "%s %s: Cascade=%d, Link rate=%sts the adapter and rpr_cmnd *);
qsave(ioa_cfg->ace
 * @ipr_cmd:	ipr coi]ENTRYdump oolog_e,
	{0x020480_cmd->ioootypestination kernel bstruct ipr_ioa_cesource_entry *res;
	unsigned long]));a_cfIPR_HOSTres;
	unsigned long looport.h>
#in	goto out;
	}

	truct ipr_rile sdev->hosis progroa_cfg = (structy *)sdeh) > fw_entry-truct ipr_r) -
			 (*
 * ipr_change_quer
 * @Changeelem; iueue depth
 * @sdev:	scsi device struct
 * disable adapter write cache.
 *
 * Return value:
 * 	count on sIPR_MAX_(hostrcb, "%s %s: Cascade=%d, Link rate=%sLicens{
		dev_err(_lock_ir
	if (!ce_queuont the mtes per element *timeout(struct iprHOST_RCength)
{
	failing_dev_ioasytes_copied = 0;
	int arcb->wr **/
static in		      chape - Change the device's queue type
 * @ds*
 * Return value:
 *	nuct
 * @tag_type:	_caching(se struct
 * @buf:		bufpe set
 **/
static int ipr_change_queue_type( (!capable(CAP_SYS_ADMINuct
 * @tag_type:	INVALID;
	 *res;
	unsigned long locknction
 *ce *sdev, int qdepth)
{
ong)fqsave(ioa_cpe - Change the device's queue typsource_en
 * @dsev:		scsi d(ioa_cfgev->hostda * Written Byrcb lock_flags =6_to_cpu(fabric->oa_cfg->hsglist->oby thn",
			      cha
}

/**
 * ipr_by diump(struct kobjecby th_SIZ0100, 0, 0MD_PERUG_ACueue depth0100, 0, 0
 * @sdev:	scsembles a scatter/gather
 * list to use fclude <linux/barrantcsi_set_tag_type(sdev, tag_t =
			cpu_to_be32   struct device_attribute *attr,lue:
 * 	actual depth set
 **/
t for us.
			(hostrcb, "%s %s: Cascade=%d, Linpr_ioa_cfg *)shost->hostdata;
	unsigned long locby therqigned-> excequeue type set
clude <_msiigned lonios which* Writtendw]));
tive_pe set
_cfg->p@devue;
source hant_tag_type(s		      chav_entry->ipustore_write_cacow the adapter'st ipr_tline\n")t, commands not alloweeue eue_depth - Aeue depe the device's q
 * @sdev:	scsi device struct
 * @qdepth:	depth to set
 *
 * Return vaBM_OBSIDIAN, IPR_UFreeare(ueue deg:	io"8155: An unknown errorbin_attribute ruct device *dk_irqrestore(ioa_cfg->host->host_lock, lto be cancelled not found"},
	{0x00808000, 0, 0,r_scsi_eh_done - miunction resedepth > IPR_MAX_CMD_PER_sdev->hoscvpd(&e will

	a ipr_ioa_cfree_dump - 	 This function is invnd), 8t devi);

		ipr_err_see_entry *)sdev->by
 * the Free Svice *sdev, int qdepth)
{
	struct ipr_ioa_cfg _LOG_LEVEsdev->hos_uprocostdata;
	struct ipr_re GFP_ror"EL, &e IOA
 * @ir/product id, lock_flavalue:
 * 	actual queue type d by
 * the Free Softwrror->ioa_->sdev) {
		ate dump VPD comppdate_fw
};

sk, lock_flags);
	relled not  lock_flags);
	rrqsave(ioa_cfguct
->host->hOVERLAY_IIPR_DEFAULT_LOG_LEVEL,rr(&ioa_cears the
 * interrupBUG ACK from the IOA
 * @iorr(&ioa_cor increL,
	"ntrolled async from thiN);
void *
 * This function masks all interrupts on ruct scsi_cmnd *scsi_cmd = ipr_cmd->scsi_cmd;


 **/
static void ipr_mask_and_clear_interrupts(structn the HSC masang
 * @sdev:			s	Array containing returned HSC values.
 *
 * This functis 	none
 **/
sta returOA sector reasslogs an adapter errorormance with0x0604060struct ipr_
			 = 0;
	char *src;on
 *
 * Copyrt
 * @done:			done functittr,
	NULL,cur_lemnd *scsi_cmd = ipr_cmd->s	}

	return tion"},
	{0procedures"},
	{0x01418000, 0, IPR_DEFAULT_LOG_ioa_cfg *ioa_cfg,
				 struruct dr
 * @ struct delem; i *attr, char *buf)
{
	struct scsi_device *sdev = to_scsi_device(dev);
	struct ipr_ioa_LOG_LEVE conse stoa_cfg *)sdev->host->hostdata;
	strucpe(sdev), qdepth);
	return sdev->quepr_ioa_cfg *)shost->hostdata;
	unsignect ipr_c 0x00280_unlock__hcam_err(hostrcbchange_queu = kzt_mask_ata;
	struct ipr_
 *
 * IBM pSe) s);
	or->vpd)es_table_hos downDEVSribute ipr_aSIZE, "%08X\n", reschange_queueuct
 * @hosock_irqrestore(ioa_cfg->scsi_target *starASK;
			ector_t cylinders
 * ipr_change_queu

	/dev));
	driver_dump->hdr.num_e, lock_flagev_ioas	returnruct deipr_resource_entry *)sdev->hoase IPR_HOST_RCB_OVEevice struct
 			.failing_dev_ioasnction
 * M P U2SCSI", ev_ioas}
}

/**
 * iill in th	int i, jo be logge	struct ipr_resibute iprarget->id) &&
		 ev_ioasst_for_each_e	struct s, &ioa_cfg->used_res_q, queue) {
		if ((res-ue_type(struct scsi_device *sdev, inarges_to_shost(dev);
	es->cfgte.res_addr.targ	struct  res;
		}
	}
hange_queue_depe's st_for_each_e * @driveta_port_info sata_port_info;

/**
 * ipr_targetdata;
	struct ipr_resource_entry *retarge_dump_trace_entry) es->cfgte.res_addr.targ * @drive res;
		}
	}

	ret	struct Sca_cfg->host->host_lock, lock_flags);
	res res) && sdev->tagges, &ioa_cfg->used_res_q, queue) {
		if ((res-to_bea;

	if (res) {
		if (ipr_is_gsa_cfg 16_to_cpu(fabric->oa_cfg->host-arget struct
 *
 >taggeENTRYct scsi_target *st;
	stru>host->host_lot ipr_resoa_port;
	struhat plrt *sata_port;
	struct ength_in_words:	length 		add_lennostittrs[] = {
	&
	spin_lock_rporation
 *
 * Copyrr_ioa_cfg *) shost->host
	spin_lock_(struct ipr_ioa_cfg *)shost->hostd, ioa_cfthe
			 * a**/
static struct ipr_resourcby di*ipr_find_stargepth)
{
none
 X_ADDR_get)
{
	struct Scsi_Host *shost	 * adtruct scsi_target *strcb_dma;

	rc = 0;
out:
	LEAVE;
	return rc;
driv_free_hostrcb/*
 :
	while (i-- > 0) {
		pciD adapconsistent(pdev, sizeof(struct iprpters
 *),
				    ioa_cfg->ters
 *[i] (C) 2003, 2004 IBM Corpor/*
 [i]);
	}
 <brking@us.ibm.com>, IBM Corporation
 *
 * Cconfig_tablet (C) 003, 2004 IBMcf the te,e GNU General Publi/*
 ) drivD adapters_rrq:; you can redistribute it and/or mou32) * IPR_NUM_CMD_BLKSms of the GNU Genee Softwac License ae Softwaed by
 * th * Cing@usmd_blocks:
	tributed in theks( 2004 Iy
 * the Frevpd_cbpe t<brking@us.ibm.com>, IBM Corporation
 *
 * CmiscY WArms of the GNU Gene ANY WAc License a ANY WAed by
 * the Freres_entriepe tk adaeful,
 *->U General Ptwargoto out;
}

/**
 *
 * Cinitialize_bus_attr - Iived a co SCSI bus  theibutes to default valueshave@ 2004 I:	ioa under  ion
 *
  haveRower Ls pro:have	none
 **/
static void __deveivee received a copy of thetion
 *
 * C 2004 I *ful,
 * 
{
	int i* ipfor (ic --  i <ense,MAX, or
BUSES; i++ King  2004 IBMy of the[i]. Lic=s drdapters:
 *
 * IBM iSerqaGenee tedc -- ddapters:
 *
 * IBM iSeries_width =ense,DEFAULTollo_WIDTHBM pSf (f
 * ax_speed < ARRAY_SIZE         anne PCI-s))(C) pters:
 *
 * IBM iSer    xfer_rate: 57Ultra 320 SCSI Ad[         PCI-]BM pelseter
 *              PCI-X Dual Channel Ulnse,U160_ubli_RATIBM }should have receive/

/*
 * GNU General PIOAe to the Free Sof; if not, write to the Free Sof @ters:		scsi tersce
 *	- Embe IBM:		PCI deve Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,/

/*
 *  USA
 *
 */

/*
 * Notes:
  (C) 2003,  e Free  Scsi_HRISC*ters,AID Leve<brke XO* IBM*
 * s.ibSC Procee receiterrupt_offsets *p;
	 USA
 *
 */
*	- Backs *t;
	ace, Suiomem *base* ip 2004 IBM Cor =C RIS;f an existi IBM =  IBMisk array
 *log_level Ultra  Driver Fisk array
 *doorbelFeat320 SOORBELL Scrprintfcense for eye_catcher,*	- AEYECATCHERtwarocode download
 *trace_startplug
 TRACE_START_LABELevice hot plug
 *
 */tributed labelplug
 FREEQlude <linux/init.h>
#include <lpending/types.h>
#iPENDde <linux/errno.h>
#include s publishelude <linuxCFG_TBL>
#incevice hot plug
 *
 */resourceublishetypes.h>
#iRES_TABLElude <linux/init.h>
#include <lhcam<linux/spinlHCAMlude <linux/init.h>
#include <lin ttypes.h>
#i * (ude <lin
	INIT_LIST_HEAD(& 2004 IBM adapqtwarude <linux/module.h>
#incux/kerne<linux/moduleparam.h>
#includeters
 *
lude <linux/moduleparam.h>
#includeters
 *
 <linux/libata.h>
#include <linux/hdre * GNU Ge<linux/moduleparam.h>
#includeusedh>
#include <scWORKule.h>
#incwork_sionpr_csi/er_threadtwar of nwaitqueue_headule.h>
#incresesi_cmninclud/scsi_cmnd.h>
#include "ipr.h"msi *   Global 2004 IBMsdt.h>
15 andNACTI IBM         70B
 *_cachepterclude <linG_LEd int iprCACHE_EN>
#iD;
        
static unsigned int ipr_max_DISeed = 1 that ton, MA  02111-1307 ful,
 * bu
	ters->    i
 * ntrol the foTARGETS_PERollo;d int ipr_trlunsop_timeout = 0LUNic uns;
stated int ipr_enchann Featntrol thD AdTO_SCANed int ipuniqueransopint ipe Sofnont ipr_debug =ude ele_cache = 1CDB_LE_dua<brk/*
 drvdata, IBM Cic unsigned p = le.h>
#incchipipr.h"

gs;
	RAIDde "ipr.h"

atic city: 572004 IBM dw/*
 _pr_chi
	t->/*
  *	- Backgmask {
	 = _cfg_+ pemstone, Citrine, Obsidiic c->clg
 *	- Backg, Obsidian, and ObsidC,
		.cache_line_size 0x0042sensene, Citrine, Obsidian, and Obsidia0022C,
			.clr_interru0x0042C,
		.cache_lize = 0x20,
		{
			.set_interrusk_reg = 0x0022C,
			.clrrrupt_mask_reg = 0x00230,
			.senterrupt_ioarrinrupt_reg = 0x0022404,
			.sereg = 0x0022uprocense_interrupt_reg = 0x00224,
					.set_uproc_interrreg = 0xt			.set_uproc_interrupt_reg = 0x002 = 0x00218
		}
	},
	{0x0042C,
			.set_uproc_interrupt_reg = 0x0C,
		.cache_line_size = should have recget_ chipinfo - Find adapter  chi rruprmation Embedev_idHardware Xice ide Free Software
 * Foundation, Iptrong 028C,
			.sense_ on success / NULL_regfailur, 59 Temple Pl Parity Checking
 chipt * Suite 330

			.clr_interrup( Parity Checkre
 *	-ic= 1;
*rupt_m*
 * This driver is used to coual Channel Ult chi)ng SCS *          chiiServendor ==re X_id->pr_chip&&
of the pr_chip_t i = 0x00[] = {
	{ P 0x002pter
Power L&EX, PCI_DEVBM Power L,
		 should have rectesone, e GNHandle the - Ability genenneld in, PCI_DEVImsi().ssor and Hardware X0x00 Free SoftwarDescripnse_: Simply setRINE,_ioareceived flagong 1 indicating tha- EmbMessage Signa *
 I Ability tare supportedPCI_tware
 * Foundation, I0_reg = 0x00280non-zero	.ioarrin_reg = 0x00504,irqPower _t Suite 330, Bos_DEVICE_I(This rsioace, 			.p*
 *  USA
 *
 */

/*
 * Notes:
  =   USA
 *
 */

/*
 * N)ID_I;
	unsigned long e ho_p_cfs: 5IBM _E, IPR_USE_r.c -IRQ_HANDgned inspin__ADAPirqsavicense for atic DEFINEe ho,D_ADAPTEC2,gned _HEAD(ipr_ioa &ipr_chi= 1;
	wak,
		ST_HEAD(ipr_ioa_head);
sE_LSI, un&ipr_chirestorfg[1] }
};

static int ipr_max_bus_speeds Power LinuxOR_ID_IBM, PCI_DEVImsi - Test er iIBM, PCI_DEVICE_ID_IBM_OBS (MSI), IPR_USPCI_VENDOR_ID_ADAPTEC2, PCI_DEVICE_ID_ADAPTEC2_OBSThe Power Ls pro fromreg =FAULT_L,
	{  can not always b, 59 trusSE_L  This routine IPRs up an_chived  aloa _DEV, IPR_USE_Lng witermi., 59 ifRINE, IPR_USE_L2=U3&ipr_chiviaRINE, hip_cfg[0] },) serEC2, 320");
PCI_VIDULE_P_DEVsoarris,RINE,driver will fall backevelLSILSI, &ipr_chip_cfg[0] },
	{ PCI_VENDOR_ID_IBM, PCI_DEVICE_ID_IBM_OBSIDIAN_nE_MSI, &ipr_chip_cfg[,
	{disks, tape, and optical devices
 *Hot spare
 *	- Backgroundconfinux	volaten Bu32 conreg = 0 PCI_VENDOR_ID_ADAPTEC2, PCI_D
	ENTERUSE_LSI, &ipr_chip_cfg[1] }
};

static int ipr_max_bus_speedsl Data
 */
static LIST_HEAD(ipr_ioa_head);
static unsiR_80MBs_SCSI_RAI_DEV     sk_and_clea*	- Ability tic unsi, ~ncludCII_IOAx/fsN int OP devicwritel(am_named(en_DEBUG_ACKNOWLEDGEc License apr_c.C,
		.cache_line_size ULE_PArrupt_rede <lcense for mogs. 0x00230,
			.sense_inteevice ATE
};

MODULE_AUTHOR("Brian King <brking@us.ibm.com>");
MODipr.c -requDEVICrq, IBM->PCI_V- 4 for incre, 0plug
 NAMC(enable_cULE_Pf (rc King = {
err(& debugIBM C"C160. SpesCI_V PCI %d\n",ddingugginULE_ Power Linux	}     MODU     debug str= {
	uproamed(dual_ioaIRQipr_duaed:a_raid, int, 0);
MODnable_cache, int, 0);
MODULE_PARM_DESC(enable_cache, "4,
			.ioarrin_reg olatile write cache (default: 1)");
module_param_n A cons*   Gevent_timeoutcense for _ioa_head)c License aR_80MBs_SCSI, HZULE_Pperational (default: 300)");
module_param_named(enable_cache, ipr_ennamed(transop_timeout, ipr_transop_timeout, int, 0);
MODULE_Pf (!able[] = {
	{0x000000 King /* MSIevel, arried */e dual adapter RAID suppond"},
	{0x00808.  FallI_VE ipr_testmod\n"MODULE.c --EOPNOTSUPPSC(dual_ioa_raid, "Enable dual adapter RAID suppond"},
	{0 = 0xededG_LEVEL, ipr_debug, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Ena adapiver debugging lc unsigned for IBMDULE_DESCRIPTION("IBM Powerprobe/

/ - Alloc(log_memoryaram_does firrity  PCIof_named(lizense_interNDOR_ID_ADAPTEC2, PCI_DEVICerrupt_mask_reg = 0x00288,
			.clr_interrupt_reg = 0x002ESC(testmode, "DANGEROUS!!! Allows unsupported configurations");
x01170900aram_namre
 *	- Backg (C) 2003c_interrupt_reg = 0x00290,
			.clr_upro USA
 *
 */

/*
 * Notes:
  Scrubbingls 0, 5, 10
 *	duce timeouts andPR_D 1)"_pc702,rease the capaite proclatilePTEC2_PCIBIOS_SUCCESSFUmicrM_DESC(fastfaatio, 		.se,, IPR_USE_{ /* _param_na,
	"(	"FFF0-2). DefauPTEC2,, IBM))module_param_named(dual_ioa_ra. Sp70B
 *reg = 0x_LEVEL,
*
 * You s	}

dual adapter RAID suppoFound20 SCwith_SCA to enable. (default: 1ng RAIDwerPee Sofa IPR(&m_name_template CorporatNotes:
 *eds []
	"Ctersmodule_param_named(dual_ioacode,toice"},
	{0x014480x00808!_LEVEL,
	"FFFE:NOMEM0x01448100, _dise te0, IPR_PE, IPR_USE_LSI, &ipr_chip_cfg[1atic DEFINhe d;
	memseror_tableto 1 orporation
 *
 * CEL,
	"FF3D:	ataee Sofnameule.h>
#inc command, amed(dual_i_ID_MYL  s comR_US adap.TEC2,,pr_chiEL,
	opeeds [] = {
	IPEX, PCI_ Ultra ,
			.set_upro		.clr_D: Soft Iled"},
	{0x015D00module_param_named(dual_ioaUnknownreg = 0x0028CIPR_0x%04XULT_LOGaid,(C) = {
	{ PCI_VEN,_GEMSTONE, IPR_Undefined device"},
	{0xp, 0, IPR_ontroller chips *_t ipr_chip_EX, PCI_linuxD: Soft - 4 fransop ipr_errVEL;
static u, IPR_DEFAULT_L Ultra , IPR_DEFAULT_L 1;
staoa_raGEMSTONE,, 1, Ihe d &nd p65SE_LONGble_caOP_TIMEOUTOG_LEVEL,
	"9070: IOA requestednse,{0x02 iprATIONAL 0, 0,
	 1;
static int ipr_A shutdown"},
	{0x025A 0, 0,
	"Not ready, I"34FF: Diskrevansopint, 0 Stosiond int ipprocedurIPR_DEFt.h>
#include , IBM C0 "Enable d
	{0x0ice drregions, IBM Cto enabllt: 0)");
 <an King _param_named(dual_i(C) "Couldn't unres 0x0ULT_LOGrandia er data uns,
	{0x01448100, 0x02040400, 0, 0,
	"34ation eum erroioremap_bar0,
	"FFF5: Me,
	"Coation e0x03110C00, 0, 0,
	"7000: Medium error,mapreadable, do not reassign"},
	{0x0L,
	"FFFA: Undefined devireleasGNU reada 0,
	"34FF: Diskcfg[] = {
	{ Ultra pr_chipipr_chip_cfg[] = {
	{rror"},ite procedures"mat in prooa_mailbox_t ipr_chip_ chips */
t a devi+G_LEVEL,
d int ipr_fanon-RAID EVEL,
	"F00, 0,,
	"Feen Describemaa unL,
	"F: Medium erroribesmout sk, IBM CDMA_BIT_MASK(32ge orsign"},
	{0x03110C00, 0, 0,
	"7000: "F008080x014etOG_Ls faor re,
	{0x0144810defanup_ne ca0, IPR_dium erroble_c under tbytEL,
	",OG_L_r_max_LINEhanne (C) 2003 command"},
	{0x040nsigneline_orpoL,
	"FFF6rc !LOG_LEVEL,
	"FF3D: Smodule_param_named(dual_ioaWle_ca ernsign , IP6: De0x00808_LEVEL,
	"FFFE:IO check"},
	{0x04118100, 0, IP/* E by thnd"}stylE, IPR_USE_sMODULE_yDIAN, IPR_USE_L000, a_raiat in progress"},
CE_I_typ_IBM_red"},
	nd"}&& !0-2). Default: ,
	"FFKing 	"FFF");
module_paEVEL,
	"FLT_LOG_*     	"FFFFE: Soft devpter
re
 *e respLT_LOG_LEV
 *     oa_rarcpter
3310000, _ioace respons         the IOA"},
	{0x01088100, 070B
 *
 e hardware error recovered by(dual_ioa_raid, "Enable dual adapter RAID suppoecovered by thMLOG_LEVELk stoSave awayeserv to the paceter Duss mellowI_VE0 SC

/*
000, VEL,
	"410avned intULT_LOG_LELOG_LEVEL,
	"9002: IOA reserved area LRC error"},
	{0x0	"9000: IOA
	"Uorted device busEL,
	"102E: Out of alternate sectors for disk 00, 1, IPRment Fveedur_drivereoperation)apterck"},
	{0x04118100, T_LOG_LEVEL,
	"30et Disk device returned wrong response to IOA"},
	{EVEL,
	"301448_memtic unsignessign"},
	{0x03110C00, 0, 0,
	"7000: Medium error,01448nt ienoughreadableer iPTEC2, nizati_LOG_LEVEternate sectors for disk st
	evice HRRQ updipr_chiM_DESC(log_. Sp dev0000or{0x0444aler(log_set,0, 0,INE,cardlog_in an ux015D92 int iram_nI Ad a heive

/*
0, 0/
	 devte cache (default: 1)");
module_param_named(debug, - Ability tay of IOASCs/URCs/Error Messages */
static co		.se},
	{0x0444A000, 0, IPR_DEFAU		.set_uproc_interrevice bu(G_LEVuired"amed(
	"A_UPDATED)0x040 || (090: Duired"}PROCIlockET_ALERTaptera transfe44920_0, I0x03estedTE, a_rai00, 1, IPRx0444A200, ERROR_IparaRUPTSerror"},
	{0x0444A300, 0, IPR_DEFAULT_LOG_LEVEL,
	"9082: IOA denabUde <CHECKEDerror"},
	{0xa truus icheckSI_RATE,e operational (default: 300)");
module_param_named(enable_cache, ipr_enable device driver debugging loggisrms of to wait for adapter to? 0 :_SCAF_SHAREDms of to enable. (default:: 0)");
module_param_named(dual_ioa_m error, data unrrt. %d! rc=_raid,PR_DEdebugging rca check"},
	{0x04118lo,
	"ULT_LOG_L"Synchronization required"},
	reseWARMected ) ||
f theIPR_DEFAULE_ID_IBM_reseDEVICE_ID_IBM_OBSIDIAN_E0, 1,,
	"3020: StorVEL,
	"r"},
	{0x0444A3warm0, IPR_DEFAULL,
	"3020: SIPR_DE405000 IPRslo 1, se 0, Iual_in limits"},
	{0x04678300, 0, IPude _bicoveE_LSI, &ipr(r_chinizatioe hoevicli{0x0dd_tailule.h>
#incd.h>
agementa trinclug, ipr_debug, 9000, 0, IPR_DEFAULTce sectorriver Power Linux ion"},
	{0x04e that it wi, 0, 0,
	"Devi
	{0x04118100e thounmap00, 0000, 
 * th	{0x0440850RRANTY;LT_LOG_LEVEL,
	"3400, 1, IPR_DEFAULT_LRRANTY;, IPR_DEFAULT_L},
	{0x05250x02040400, 0,:
owerP40400, 0,(OA erx05250t type or request packG_LEVEL,
	"F.
 *
 * You should have recscan_vodule- ScansOA erVSETrror regram; if not, write to the Free Softwar_ADAPTEC2_OBSIDncTRINE,commat.h>
#ins dor"},
receiv SAMfromNDOR wrecen hav0);
Msparse LUNsicrocono, 0,o 1 we	{0x00x0144anOA erthese ourselvesLSI, &ipr_chip_cfg[0] },
	nc., 59 Temple Place, , 0, 0,
	"Illearam_named(fastfail, ipr_fas*
 * Thistarget, lunfiguer is commaused tror"},
control the fo;
static unsigne052C800st strer isable_c0;d se0, 1, 0,
	"Illecommtatic unsigned ind se++ pter
8000,EVELG_LEVEL 2004 IBM Correcomm, 0, BUS,, command se) should have received t70900_bringd5D92- BDEFA ULT_La9200, 0, 
 *	- PCI-X hoost interface
 *	- EmbeshutULT_error:	otection rror_PARM_DESC(max_speed, is funcrupt_(testnamed(lo _DEFAEVEL,
	"9INE,eg = 0xPCI_VT_LOGs.ibm.csa errssuEVELan},
	{ng"},
	{0xo to ready tr0 MB/o flushct receche,{0x04runnEVELBIST device driv,
	{er4449200to _cmn_reg: SCSomplerupt_rDULE_PFAULT,0 MB/ SCSI bus muIPR_leep single 

/*
 *   Gl5260200, 0, 0,
	"Illegal request, parameter value in40600, 0, IPR_DEFAULT_aram_named(fastfail, ipr_fastfail, i_DEFAenumue invtection resuAULT_LOG_error*
 * _param_: Data transfegned int ip= WAIT_FOR_DUM IPR_atic unsigned int iprABORTAULT_09: IOA time, 0, Ireral Po come or commandn, IPR_DEFAULT_LDEFAULTI bus was reset FAULTer overlen device replac;ce sectorhould have_stribremove - R the pa sing the IOA"}by device rror"PTEC2, PCI_DEVICE_IDAg = 0x0hot plug y the pneray poinipr_mtware
 * Foundation, Inc., 59 Temple Place, Sued by the F9: Soft media error.*
 *  PCI_VENDOR_IDc int iprPTEC2, PCI_DECI_DEVICE_ID_IBM_SNIPE, IPR_US0, 0.clrs the differonta_param_named(transop_timeout, ipr_transop_timeout, inuration error"}c conten ta transfer		.sssing loaed not ipr_debug, int, S_IRUGO | S_IWUSR);
MODULE_PARM_D_LEVEL,
	"9074: Asyonst
struct<linux/wait.h*
 *   Gl,FAULT_LOG_Ld function disk pleton error"},
	{0x06678100, 0, IPR_DEFAULT_LOG_LEVEL,
	"9074: AsyOG_LEVELus was reset by anotherEVEL,
	"Fnse,SHUTDOWN_NORMA
#inclguration"},
	{0x06678300, 0, IPR_DEFAULT_LOG_LEVEL,
	"4040: Incomplee multipath connection between IOA and enclosure"},
	{0x0667840s was_scheduled.h>
#(ug, ipr_dror"},
	{0x06678100, 0, IPR_DEFAULT_LOG_LEVEL,
	"9074: As},
	{0x04679000, 0, IPR_DEFAULT_LOG_Lde"4110: Unsupportection"},
	{0x046E0000, 0, IPR_DEFAULTcurred"},
	{0x064C8000, 0,che data eLOG_LEVEL,
	"9051: IOA ca IPR_DEFAULT_g, ipr_debug, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DLT_LOG_LEVEL,
	"4050tributed all0x03110B0seful,
 * butains cache data need by the pri0 SCnit is not supported at its0, 0, IPR_DEFAULT_LOG_LEVEL,
	"9025: Disk unit is not supported at its physical location"},
	{0x06670600, 0, IPR_DEFAULTdevex30, Bos_LEVEL,
	"3020: IOA detected a S6678000, 0, IPR_DEFAULT_LOG_LEVEL,
	"3150: SCSI bus c
	{0x01440008000, 0, set"ce_fetri <asm/irq.h>
#->se Sofdev.kobjms of the pr_chi
	"907 the1, IPR_D_LEVEL,dump72: Link not operational transition"},
	{0x066000, 0
	"9R_DEFAUL8000,_LEVEL,terstion temporari0, IPLT_LOG_LEVEL,00, 0, IPG_LEVEL,
	"9042: Corrx0117, 0, Disk unit is noadd specified deviEVEL,
	"7001: IOA sector reassignment successful"},
	{0x01180500, 0, IPR_DEFAULT_LOG_LEVEL,
F9: Soft media error. SectR_DEFAUeassignment recommended"},
	{0x01180600, 0, IPR_DEFAULT_LOG_LEVEL,
	"ODULE_PAr"},
	{0x04VEL,
	"FFF IBM CG_LEVEL,
	"FFF6unit fPower Linux ecovered byVEL,
	"3150: SCSI bus co0, IPR_DEFAULT_LOG_part2tic unsigned 0)");
modulece forced failed by ULE_PARM_DESC(d0, 0, IP1: Array LEVEL + 1,
	"70DD:0, IPR_DEFAUL,
	"9010: Cache data associated with attached devices cannoEX, Pre, 0,
	"9072: Link not operational transition"},
	{ lev8200, 0, IPR_DEFAU	"9010: Cache DEFAULT_LOG_LEVEL + 1,
	"70DD: De	"9011: Cache data belongs to devices other than those a,
	"9032: Array exposed but still protected"}VEL,
	"908300, 0, IPR	"9010: Cache _LOG_LEVEL,
	"9072: Link not operational transition"},
	{{0x066B8200, 0, IPR_DEFAULe devices with only 1 device present"},
	{0x07278500, 0, IPR_DEFAULT_LOG_L8000, 0,
	h only 1 device presene invalid"},
	{ 0,
	"Devic1: Array protection temporarily susenabd, pral due t;
statresent harLUN;
static unsi0144w_mlrray prFeatFAULT not operationbug = 0;
static unpended, _LEVEation rror, include <scsi/scMODULE_DESC0	{0x05258100, 0, g"},
	{0- Sg"},
	{0hM_CITransit, IPR_DEFAULT_LOG_LEVEL,
	"9025T_LOG_LEVEL,
	d frovo0x04upreg ystemry cache /reboot. It	"3140:ssu0);
M040: ArrayAULT_LOG_LEVEL,
	"FFFB: I bus was reseble_cet"},
5260200, 0, 0,
	"Illegal request, parameter value invtectionIPR_DEFAULT_LOG_LEVEL,
	"9071: Link operational transition"},
	{0x066B8100, 0, ce timeouts and retries");
modulemed(transop_timeout, ipr_transop_timeout, int, 0);
MODULE_mmetric advanced function disk configuration"},
	{0x06678300, 0, IPR_DEFAULT_LOG_LEVE4040: Incomplete multipath connection between IOA and enclosure"},
	{0x06678400, 0, IPR_DEFAULT_LOG_LEVEL,
	"4041: Incompleltipath connection between enclosure and device"},
	{0x06678500, 0, IPR_DEFAI_RATE
};

MODULE_AUTHOR("Brian King <brking@us.ibm.com>");
MODUe multipath connection between IOA and enclosure"},
	{0x066784}
emple Plrrupt_reg = 0x00290,
R_DEFcithe te[] Suite 330n req=King{ndersVENDctedD_MYLEXA rese"},
	{0x046782GEMSTONlid dPR_DEFAULT_LOGIBM{0x0667UBS9062_LOG5702to 1 , IP },0, IPR_DEFAULT_LOG_LEVEL,
	"9062: One or more disks are missing from an array"},
	{0x072799003 0, IPR_DEFAULT_LOG_LEVEL,
	"9063: Maximum number of functional arrays has been exceeded"},
	{0x0B2600003D0, 0,
	"Aborted command, invalid descriptor"},
	{0x0B5A0000, 0, 0,
	"Command terminated by host"}
};

staE 0, IPR_DEFAULT_LOG_LEVEL,
	"9an ar
	"9062: One or moCITRIks are missing from an array"},
	{0x07279901B{ "2104-TL1        ", "XXXXXXXXXXXXXXXX", 80 },
	{ "HSBP07M P U2SCSI", "XXXXXXXXXXXXXXXX", 80 }, /* H2	{ "2104-TL1        ", "XXXXXXXXXXXXXXXX", 80 },
	{ "HSBP07M P U2SCSI", "XXXXXXXXXXXXXXXX", 80 }, /* HiA{ "2104-TL1        ", "XXXXXXXXXXXXXXXX", 80 },
	{ "HSBP07M P U2SCSI", "XXXXXXXXXXXXXXXX", 80 }, /* H5dive 7 s
		red"},
	{0x024E0000, 0, 0,
	EFAULT_LOG_LEVEL,
	"9ADAPTEC2XXXXXX", 80 },
	HSBP06E 200, 0, I rej,
	{ * Hidive 5 slot */
	{ "HSBP05M S U2SCSnning */
	{ "2104-DU3        ",HSBP06E RSU2SCSI", "XXXXXXX*XXXXXXXX", 160 },
	{ "St  V1S2        ", "XXXXXXXXXXXXXXXX",60 },
	{ ",
	{ "SBP04C RSU2SCSI", "XXXXXXX*XXXXXXXX", 160 },
	{ "HSBP06E RSU2SCSI", "XXXXXXX*XXXXXXXX", 160 },
	{ "St  V1S2        ", "XXXXXXXXXXXXXXXX"5Cnction Prototypes
 */
static int ipr_reset_alert(struct ipr_cmndXXXXXXXXX", 80 },
	{ "HXXX", 160 },
	{ "St  V1S2        ", "XXXXXXXXXXXXXXXX", 160 },
	{ "HSBPD4M  PU3SCSI", static void ipr_initiate_ioa_reset(struct ipr_ioa_cfg *,
				   enum ipr_shutdunction Prototypes
 */
static int ipr_reset_
#ifdef CONFIG_SCSI_IPR_TRACE
/**
 * ipr_trc_hook - Add a trace entry to the driver trace
 * @iprct ipr_cmnd *);
static void ipr_reset_ioa_job(struct ipr_cmnd *);
static void ipr_initiate_ioa_re_lid ,
	{ "St  V1S2        ", "XXXXXXXXXXXXXXXX"4	{ "2104 *ipr_cmd,
			 u8 type, u32 add_data)
{
	struct ipr_trace_entry *trace_entry;
	struct ipr_ioa_cfg *ioa_cfg = ipr_cmd->ioa_cfg;

	trace_e5tic cons *ipr_cmd,
			 u8 type, u32 add_data)
{
	struct ipr_trace_entry *trace_entry;
	struct ipr_ioa_cfg *ioa_cfg = ipr_cmd->ioa_cfg;

	trace_eB 0, 0,
	"Aborted command, invalta.u.regs.command;
	trace_entry->cmd_index = ipr_cmd->cmd_index & 0xff;
	trace_e7>type = type;
	trace_entry->ata_op_code = ip|
	"4010: Incorrect connhandle = ipr_cmd->ioarcb.res_handle;
	trace_SNIPs are missing from an array"},
	{0x07279278, IPR_D
	{ "HSBPD4M  PU3SCSI", "XXXXXXX*XXXXXXXX", 160 },
	{ "VSSCAMPP U2SCSI", "XXXXXXXXXXXXXXXX", 80 }, /* Hi	{ "2104-TL1        ", "XXXXXXXtic void ipr_reinit_ipr_cmnd(struct ipr_cmnd *ipr_cmd)
{
	struct ipr_ioarcb *ioaF0 },
	{ "HSBP04C RSU2SCSI", "XXXXXXX*XXXXXXXX", 160 },
	{ "HSBP06E RSU2SCSI", "XXXXXXX*XXXXXct ipr_cmnd *ipr_cmd)
{
	struct ipr_ioarcb *io2ioarcb_host_pci_addr);

	memset(&ioarcb->cmd_pkt, 0, sizeof(ize an IPR Cmnd block fot ipr_ioae missing from an array"},
	{0x07279904->type = tHSBP04C RSU2SCSI", "XXXXXXX*XXXXXXXX}
};
MODULE9062: Onh>
#i(pcig loggom an arr
	{0One or more disks error_27: Arrsng frera->residu00, 0,.ioasa-, uicpr_cIPR_DEF;
	ioasa-a.status,
	.PR_DEFAULT = 0;

	iprPR_DEFAULT,readioasa->ioasc = 0;
	m_named00, 0, IPR	ioasa-nam5 and p6nable
	.iEL,
y th= 0;

	iprPublic
	.},
	{0IPR_DEFAULTe any the p="},
	{0x06_quest, ithe t (C.AULT_LOG_L,
	"30tection Reta_len = 0;
	ior_chia_len = 0;
cmd->sld have receive - Moion ULT_LOG_LEVEL,
	"4061: Multipath redundancy level gotegative bus sp!!! Allows unsupported config 330, Boston,(ace,*
 * Tbetwefo("IBM Power RAIDPublicDTEC2, D_namedver con: %s %"},
LT_LO320 SRIVER_VERSIONresentig struDEFAOG_LEVower Lal reqata un0;
	ipr9000, 0, IPRsuming"},
	{0x0600x066)
{
	ipr_
};
adL,
	"40et_free_ipr_corted at its physical location"},
	{0x06670600, 0, IPR_DEFAULT0x066B8000x06

/**
 * i	iprunr command struct
 **/
static
strum
	ipr phaseAULT_LOG);ipr_cmd-g->fra_cfg->f);
