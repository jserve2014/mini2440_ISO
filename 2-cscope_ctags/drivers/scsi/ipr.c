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
			spin_lock_irqsave(ioa_cfg->er ften ByD ada,  Kinpflagsor IB	
 * ipr.c-> * Writ = rporatiibm.com>, IBM Coap = apopyright (C) 200res = res;
pyrires->om>, IBM  = om>, IBM opyriap->private_dataoftware; you can stargey: Bria it and/or modify
 *} else Linuxkfree(om>, IBM .opyrireturn -ENOMEM Gene
nd/o RAIDun<brkiterrestor*om>,rati: Briar the n King <brking@us.ib
 b Gen t0;
}

/*rsionipr_t unde_destroy - Ds progra SCSI *
 * Trsio@i*
 * T:	scsid in th struche he hoIf the device wastribATA ANY WA, this function icensHOUT libatae howithIBM ,ral Puit does nothing.* but */
static voidon.
 rsionTthe  dis(ful,
   wil
 * GNU *e that )
{
	Public the
he iree so*should havftw in theHOUT termoptiM Poe as p *
 ublice that  the GNU Gs oNULLibm.sionsasree soSoft or Fht (C) 2003,.ibm.Larranublic Liceshe}ater vhe hothe
find_sdev - Findhout evRbased on bus/*
 * T/lunTICU @30,termwillMA  021PublicICULARWRn) anyvalue: * Nistrource entry poinmpleif found /distri*
OR A *(C) LAR PURPOSE.twar is You lowor
 S_SI Kin*, Suite 3tes: 
 *
 * undatMA  021*tes:ailshe
 * GN570Brporatio*        n
l Channe is l Ultra 3) tes:f the LicensNU to  Adapter
 *  570A, C) 2pSeriNU Genelist_for_eachannel (res, &ion 2 of usedX Du_q, queueatio
 al Pnt C->cfgte.   Eaddr.bus ==ibutedAchannel) &&
		l Uler07  p615ms o p655 sse f thes 570A, ided Hard/or  Features:
 *	- Ultralunutedcontrolun))hedion) anyNU Ge	}pCI-X) anyistrm;mple Place, Suslave GeneralramUnconfigureTY; utedout eve folt inter* GNU Gendriv:
 *s      todded PowHOUT folR A PAR, 2780, 5709, SeeHOUT
 Non-Volatilel Cupportce
 *	 320ibutedound An; eitnnel Dua *	- Background Png
 *	- Backgy to increasPCI;
	unsigned lor
 * (at your =y la
dwarePCI-    und Data Scrubbingarity Ch *	- Ability to ie F IBM on; eitI contro 2 of the License, or
 * (at your opm of no*		by adding Dual Channel Ul
 * Driver Fy to inal Presbutedon; ee Wr should haC RISs free sodisable>
#include <linare is Foplug of no/

#atile Wr; i
#incl0, Binux/types.h>
uxreceived a includeCI-X IBMPCI-X; eithple Place, 2 oTHOUT undatio, o Back(atodifr opMA Enginnclu	-	- Hote Cac
 *	*- C/spinlock	- Suppr.cs attachmente <lnon-eckindisks, tape, aTOUT implied wx/spinlocnty ofspecifnux/undax/sched
 * , 27nd optical devices007  success, 2780, 5709,iner
 * sclude / RAIloc *	- Background Parity Checking
 *	- Backg to increase thy
 *		by adding disks
 *
 * Driver Features:
 *	m/io.h>
#include <asm/irq.h>
#inche
 * GNhould have3, 20includeapaciinclu an existor
 eckin5 diTagged comm *	-ddedinnux/	-iver Fea microcode downloadscsi_cPCI <linux/errno.hl.h>
#include /fs.h is f s_af_dasdund Pares.h>ux/in* Drivtype = TYPE_oundibm.al P * Whead);URPOSE. apacity o|| Backgsinclu
 *
 * Icity oublic   Drivundatlevel = 4opyrig Drivno_uld_d.h>
# = 1 Sand/oLT_LOG_LEVELvsNU Genapacity oublic blk_dler
_rq_timeout(acity requesion
0;
,hed b- PCI-IPR_VSET_RW_TIMEOUTished ouenable_smax_sectorsity ointon.
 ende <_catic unsigMAX_SECTORS.ibm.static unsigned innt ipr_dtranst ipr_dtatikgrouniskriver_lock);

loallowed =taed a 
sta0atic unsigngataes.h>
&&essoatic LIST_HEADiniclud_HEADg_leipr_istatypes.ude <linux/delay.h>
#include <linux/pci.h>
#include <linux/watiLT_LOwerint iprarraadjudual_ioa_depthity o, 0_raid it tCMD_PER_ATA_LUNished e <lt, wtatic LIST_HEADli.set_nclude the Frche_line_sizevel0x20,
		Linux= 0x0int* Driver Feacmd_per_werPypes.hdware lastatic LIST_HEADdelay] = {
	{ic LIST_HEADpci224,
			.ioarrin_regwaHardware lemple Place, Sui.clr<linuxtwec - PrepPCI-0 SC#includs toTY; NTABbug ev224,
			.ioarrin_regers:
rupt224,
			.ioarrin_regbinitializes anANTABILIT so that futncludask_002	.iosncluthroughe= 0;
line_si*
 *  work
static LIST_HEADmodule224,
			.ioarrin_regclr_inparam] = {4g = 22C,
	upcal 	- Background Parity Checking
 *	- Backshost.h>
#i received a rt.h>
#iam.rc =OUT XIOsi_cENTER			.ioar_testm0, Bf non-ux/i received a ce_si50e_interrunData
 */
static Lpt_reg = 0x00ne_si	.clr_inrPCI-campFld h= 0;
sincludeipr_icux/i0scsi_cHot srrup
 * Chesi_cLEAVEproc			.errcwai
	},
	{ /* Snipe ant_maship_t ipupt_regline_siz1e_intarnterruptterrupt_reg = 0x00218
		}
	},
	{ /* Snipe and SI coNTY;mmand:
 t52C,er_c PARibutedadlace,n			.ioirmpt_r.h>, 570A, 5Tty ofMA  021inclus. Whip_tcaSI, &nape,e use I_DEVICE*npport2C,
			.EX, PChenstathandlTRINnewinterruptg = 0x00288,
			.clr_interrupt_mask_re /028eg = 0 PMA  021SS FOR AID_ADAg = 0x0028C,
			.sens			.iot_maskI_DEVICE_ID_88_interclr_MYLEX, PCI_DEVICpes.asm/io224,
			.ioarrg[0] rq,
	{ PCI_VENDOR_IDR_IDstrur224,
			.ioarr wil/ wil IPR_USE_LSI, &ip/scsi_tcq.h>
#include <sCI_DEVICE_ID__USE_L	.iST_HEADerrn},
	{ PCI_VEsi_eh IPR_USE_LSI, &ipr_chip_cmnd224,
			.ioar" IBMh"er v*
 *   Gloes:array
 *		bynst sttic LIST_HEAD(ST_HEADkerneU160
#includeadd_to_mURPOnse_U320_Sin_erclude, orAMP, IPR_USE_LSI,I, &ipne_si!able denaca_modelriver_lockul,
tneeds_sync_complet 0;
ollerne_sie, or chipsR PURPOSE. con.cache_loundCI_DEVICE_ID_2e_interioarrinI_DEVICE_ID4 = 0x00290,set_masISC Procelt: 1=MYLEX, PCIICE_I160_SCSnd/or nclude <linux/delay.h>
#include <linux/pci.h>
#include <linux/wa HardwarepM_CITp[] = Lin{ Peh_censereset - Rreasi_IBMn Byd commanstatiUarracmd
 *
 * _LSI, &iisks, tape, and optical devicesSUCCESS / FAILEDg = 0x0028C,
			.__0 - 4t_regincverbl Channel Ultipr_line);
clr_SE_MDEVICn.
 l, "Writ[0] }, 0 - CI_DEVICEMaximum bus sk array
 *		by adding disks
 *
 *ons");
-> ipr_disks,X host interface_inerr(/*
 includp Driv= 0x
		"AY WARd befailws uniA, PCresullude error	.seovery.\n"tati8al PWAIT_FOR_DUMPuteduing
 *	-sdt_, 57e4
		}n seco00214, _chi = GETt, "TC(tr0e_siude <lielows"

/*
 ng
 *nterruSHUTDOWN_ABBREVeful,
 ipr_dnamede_cache, i0028C,
			.sens!!! Atwee ipr_nux/sch>
#iCache
afor clr_istfail, be eckinon; eit(rite"Reduce ic unsi.maih>
#iOR_IDe_siGEROUSnable_cache, "n-vo wilPARM_DESC(tati: 1)"n-volatie_,
			_named(debugDULElog_= 0;
, "Seto coROUS ipr_dows unTRINverbosityDOR_ID_MYLE(defaul:r adblkdev.(x0050ode, @t inDdiskTRINE, IPR_a8for 
	il, ip/* Snipe
 *	-ScssuM, PCMA  021 enabl_IDsityaffecte, MA  02,statI PCI_VENDOR_Ii, PClinux/sched, a LUNble duaU160,besk_reid, l adaptThis driirst.ing. 1aNDULE_cfg[E_ID_);
M 320 SCESC(dMODUNSE("GPLSC(dER_VERSo coual_io. (de.clr_uprocena PHYLE_LICEs/URMODUs/E);
MO = 0x00288,
			.clr_intIBM_OBSIDIAN. Se<linzero#inc wriurhip_ 0x0028C,
			.sensVERSes */
st*		by adding disks
 *nable de60. 1;
s/scsi_cBackgr IBM Data ScrubbiRAID Adapter
 * er's noo be di/scsi_cmbili_tctrcb,
	"Srcbot found"},
	{CI_DEkt *dclude ot * IBM"il, ie_si8080 MBregse
 *CI_DEu32 be site ct, S_IRUGinux/slt: 300GNU Gcenebug,acheparam_nambutePR_DE8= &s 0);
MCorpo 0, 0,"},
	{0sPR_D unsi PCI_DE IPR1tic uterru ANY WAUTHterm.u.sign,
10EVEL, -X D*d(fastoperFeatures:
 *	-  IPR_	r rea"4101nt ipr_duabuggingPOSERQ = IPIOACMEFAUgnclude <cdb[0]01170900Eipr_DEVICachetaticpeed, u deviionsMODUtatic unsign,
2"FFF7: Mask_PHYMedia TION(, 1, IPR117CI_DEarms_len = cpuHOR(be32(upt_of(sign-> your 022AN_Eessful"}, |011802EFAUFLAG_STATUS_ON_GOOD_COMPLEFAULr
 *	-HEVICEend_b intingign c unscmd, the
 unsignnterru);
MODIPR_DEeer_lock);

E: So = ssig reacpuDULEmmen_LOG_sa.EL,
	_IWUibute0117Oail(_DFAULtatil_ * Wrule_param_by thq88100_RAspeed, uint, 0);60_SCable_cache, i60_Sssign !FF7: MIOASCA"},_WASIPR_DEux/imemcpy(&_chip_cfg[] = {
ered EFAU
MODULE_PARISCu.POSE * Trleng  inclusu		by adding disaRPOSE1, Iable_cache, int, ( IO{0x0
	{Sble__KEYg = 0c) ? -EIO : 0upro
	},
	{ /* Sni
	{0x0nable. (default:ft P
 you d, ilink:	R_DEFnsigULE_cq.h"},
	@classr_duODULEdevin; ei_fastD "Enablete prE_R);
MODULEdua1418000aidRs"},
hyLE_LICEldevice dreckins	"FF3D: tde <Set to 0 e_si	{0x01, 1. Sets"},
	{0x0dures"},
		"8155: An unknown 0);
MOwas0, 1, IPR1FFF6: D0troloverS*nsig,i/scsi_tcqeg =*ODULE_Pcpinli/scsi_tcq.h>
#deastfaeRAID Adapter
 * 0x00284,
			.sense_intensigare
diNTY; utetermot found"},
	{0x0ollows un 10
and/or modif_LOG_    _USE_Lle_param_named(fas1fail, iprCI_VENDORl adADAPTEC2raidIy deipEVICE_ID_ADAPTEC2_SCmum bus ache (defaultd queuing
 *	- Adapter microcode download
 *	whilrsion 2 of in0ESC(debug, "SCSI a*R);
MODULE_PARd by de"Maximum USA d by  (0-2). Defau rec1=E
};.ed b_ct itsion 2 of ESC(deynchrq, !A"},
2048014A0000, 1, IPR_PARGemstd command queuing
 *	- Adapter microcode download
 *	ror.ar Globold exceLOG_LLOG_LEVTE00, 1,U30, 0,300	.sense_iFF6:nable dev	"Noyrighwitcay.hatures:
 protoPCI-proaseF7: MPROTO_ft P:3110BEFAULT_eg =0x01S_STP Med	0, 1, IP =		.sror,FAULpr_debreak0");ium error, data u5:FAULPIableium 0);
Me IOta unreadata unread,ODULE recd verbacit"},PIOA"},
MediC00,d0000, , 0, IPDEFAULT31x014A00,UNKNOWa erAULT_LOG_LE adahutdULT_LOG_LEVE9070:A er t ipr_ee3100set: Disk me23F format r, datle_cache, int, 0);
MODSet to 0 - 4 _LEV */
static co000, 0,0, 0, IPR_SC(debug, "Enable devicioa_raid, iprR_DEFAULT_LOG_LEVEL,
	", "Eal_ioined device responsenux/sc. SetA, 0, IP_table_tCs/Error, IPssagAULT, 1, RIV MessagION);
IOASC*  ASoft Pant arratcq.hICs/URC_error_td by the IOA"},
	{0x014Arite prIPR_DEFAULT_ioa_raid, "DANdebug, inIPR_DEFAU"Enable adapter's noionsSC(debug, "Enable devy
 *celled5702,080000, 1, IPR
	{0 write pr_faE_LSI, &ipr_chip_cfg[1] },
	{ PCIic int er f] = {
	{g AULT_Lbatt/scsievice busO |ice WUSRwrite prIPR_DEFAULT_EVEL,
	"90);
module_param_nam3100trpr.h: Dirror"},
	{0x04320000,      the Fr!pond ISC Proce);
MOD1, I/*
	gging.we , PCcurrently gofail,  0x002ssage/bug, "bsyI co tVEL,ed.},
	{0U160,foINE,the,
	"Fmid-layICE_IDcallDULEual_ioa_pinlgn"}whichh 0);
V IPRgoervesleep X, PCai2, 5r,
	"F4338imed out nable de,
	"/rdware 000, 0, 0,
	"No ready, Ist_regterr r veageG_LEVE3400: LogoatatiFE: rre"},
	{0x04408500"}ity CheckinOA timmen
	{0x033eg, "Enable endgnmemb	.set_inux/fs.h> IPR_DEFAULT_LOdaptt
MODUautedF9: DNY WARr"},
	{0x0optic uVEL,
0I_RAT b area datahe = WARh*	- don	"FF0);
Mby tFAULon00, NY WAR resmessaq9 for Disk me444x014A0000, 1trolR_DEFAULT_LOG_LEVEFFF4: Dis60_S!LT_LOG_LEVEL5EFAULT_&10000QCFAULT);
MODA reservice probleneLSI, 160, statAC_ERRR_DEFAUL unsi, 1, IPR_DEFAU
	"No realure: Disk me4448 unsistatdebug,  bus ssagetEFAUMA  021d	"9070CI_DErintk(KERN0: D,ures"},
	, " (defsigns is no0x04118* ThoEL,
	"FF3D: Soft PCI bus error rec_ublice_cache, in(faset to 0 -A shutdoone, Citri(error"},
	{0x04320000, 0, IPvice dri,, IPR_td-volor,
	{0x0r
		.OA IPR_DLT_LOG_	{ging. DULE_PAA"},
	{0x04447o respond 
	clude "iis coX, PCA"},
	{0x041 0,
	"000, 1, IPR_DEFAULT_LOG_LEVEL,
	"FFF4: DuDEFAULTA"},
	{0x043x014A0000, datU "Enable addevFAULT_MODUL		.stLT_LOG_LE	"No readault: 1=AULT_LOG_LEVE3020: S408500 sua trtem cR_DEFAULT_LOG_LEVEL,
1: /scsiPR_DEFAULT_LOG_Lrc ?x044085 : rite prred byerved area LRCual_FAULT_LOG_LEV_DEF	"3002servedn-v spare
		.clr_upLT_LVEL,
	"310DESC(debug, "Enable device dri9001: 0, 1, IPnLT_LOG_LEV	"9002: IOA reserved area LRC0, 1, IIOA timedNY WAR disks,e drigTRINlo000, 
	{0x
	"F0, 0, Ix014A- Op x014A <linux/baor USA	"No r00, 1,
	{0x04:0, 0	{0x0080DEFAULT_, 1, IPR_DEFAULT_LOG_L
	"FFo: SAS Cinclude/ Taskak Manageme.h>
#inndM_DEScal phase"s
 n"FFF6: Device  5, 10
 *t has been mod or commx01404118100, 0, module_param_named(fastfail, iprCInction f- PCIexceed0: Sto_LEVEL5D9FAULT400, 0, IPR_DEFAULT_LOGrocode is corrupt"},t Ca0, 0, IPR_D"},
	{0xELT_LOG_LEVEL,
	"81!1, Impprocedu1, 0,
	"Unsuppor 1, 0,
04678100,tery pack failong lengthe IOAccccurred"},
	{0x0444AA reserveby treee sot has beesion 2 of the last tures:
 *	- UltraystPR_DEFA05 format IPR_D},
	{0x033abDULEha
/8810nable dendednd WARopsity_CIele adYice FI},
	{43E0100,nux/s's"9091: Incorrec comek	{0x06ngth 	"Deehc uneaDEFAULT_LOG_L"FFF4: Dgibtfailegal002: for 
	}"Iluest, invpr_eT_LOvYLEX, PCI_ult: 1=, datest type or r,
	{050000, 0, 0,
	"ImDAPT.clr_BM_SNdst i_LOG_LEVEL,PR_DEFA400, 0, IPR_DEFAULle_cachCI_DEVICE_ID_IBux/sEFAULT_L - A "Enablet},
	ed eFAULd ouEFAULTlied w wriPR_DEFAULT_467044670400, 0, IPR_DEFAULOG_LEV wricoveilEFAULT_5258FAULs dat Messagi},
	{happensre o41800*	- PCI-apte sinWARR excv46E00TRINE, 0,inclu upnal"},mustr_eR_DEFt, iforde <* GNUt ha adaptmidAULT__DEF *	- phase"} resfabri detRAID command not  0,
	, and  the deEFAULT_LOG_LEVE9073: Invalid multi-on; eier's n_LEVEL,00, 0, IPR_DEFAULT_LOG_L300, 01:value invali04670400, 0, IPR_DEFA
	"4101:
	{0x052580/scsi_tcq.h>
#include <scsi/scsi"8155OA"},
	{0x2-2).o respond to34FFLEVELk phase"}format be  distvice bus messanable dce, x014A ipr000, 0, 0,
	"No ready, I0, 1, 0,
	"Illegal EL,
	"3002: Addressed device failed to respond toSnable 00, 0, Iay pde "iA design lim50000, 0,u.line_s"AOA"},
	red"},
.. (def
led"bus
MODULEwn sr commahardware erFAULe
	"FFF6: DEVEL08adapt ipr_eng_levor_LOG9quest, du5629LT_L, d device fai00, 0, IPR, data uE:  battery pack failust, dual adapA design limitssign succL,
	toist"glSCSIIPR_DEl request,T_LOG_LEV500, 0,OG_LEVEL,: MeEL,
	"4010: Incorrect conAULT_LOG1802R_DEFA);
MODULEndle"},
	{0x052580_DEF1, IPR_0, ISELECT |B: SCBUx014A001, IPher o_reqCSI "3140: 0, 1,t has been modIPR_DEFAULT_L1806CSI bus was reset by anothey protection tempor3002: Addrese, ad, proteest, 0,
	"Mspon 0, Iselectmple Place, Su118100_opk.h>xiliac LIST_HEADopegal requA design limit	{0x04670400, 0, IPR_DEFAULT_LOGue IOA,
	{ntains: IO0, 0,
	"Illegal request, cneeded by the pr,
	"CSI bus was 	"FFPR_DEFry 00, 0, IPR_DEFAULT_L areadable,checkrted at its p04670400, 0, IPR_DEFAULT_LOG_Legal req 0, IPR_DEFAULT_LOGiest, coable,paEVEL_LOG_erro reer value in604050ngle e= 0; IOA op_ Channay protection temporared d0, IPR_DEFAULT_LOGLRCeeded by the pr32 to respo, 1, IPR_DEFAULT_LOG_LEVE102E: Oulude alLEVEa/: Unsx014A0000, 1, IPR_DEFAULT_LOG_LEVELOG_LEVble,tus eferalong bus trt faiA design limiR_DEFAULT_LOG_LEVEL,
	"8008: A pth coB: SCnection between E_PA	{0x0ure"},
	{0x0667043EI bus was reset by another iniPR_DEFAULnge hunitC8100procedures"},
plete multipath connecNY WARmat,
	"Uns	{0x2C,
		.cg"403river_locipath connectionclude "i},
	{0x04449200, 0, IPR_DEFAULT_LOG_LEVEL,
	"8008: A pice bus messanother imbeddedea dat: A p and enblem"},
	{0x044, IPR_DEFAULT_LOus conl adaoller protection tempos"},
support 1: Array prrite prt has ocIPR_DEFAULT_LOG_LEVEL,
	"404B:LEVEL, res3150: s06298000bus con29"905000, 1, 0,
	"Unsuppor0, 0,8300,Enclosure does not SI bus was reset by another initiaad block written on tOG_LaR A :
 *amp *tor"CANCEL_ALL_REQUESTS written on uFOR An"},
	{0x06678300, che, nclude "ion between encl448"FFFEady tcommaine_si: %02X\n"rdware enother iniachestfa writteAULT_LOG_LEVascad,
	{0x03398000,	{0x052C0000nterruion
, PC aret by another initiator"},
	{0x063F0300ULT_LOG_L	"3002gn su10: Unsn; ei6 to respond t, datA desi31ied aparameter l,ed aU160,LEVE"},
	{0,losufotwee;

/* Tpon rec device ux/s	{0x06678500, 
	"4VEL,
	"FFFB:eplalem"},
	{procedulsumiameter valuSYNCEFAULIREDE, IPR_L,
	"40 tablehe
 raB: SIPR_Durce handle"},
	{0x05258000, 0, 0,
	"Illegal requesunsi2C,
		.ceckin* Driver Fea DisksSC(debug, "Enable devicmaxDEFAULT_LOG_LEVEL,
	"4046 connecti04678100, IPRn erro8"},
	{0arameterx014A0000, 1, on err-,
	"90oaist"glemotdisksbus error"},
	{0x04080100, 0, IPR_DE0x0050, IPR_ce rewrite pr"},
	{0x04118000, 0, IPR_DsOA"},
	{0x7	"9000: IOA reserved area data ch/scsi_tcq.h>
# Impe_rror E_In	{0xect coArray protection tem 0,
	"Message reject received fro,"},
	{ 1, IPR_Dio3: Disk me667 IPR_DEFstre"},
a misble. orC8100,ux/bithed.h>
#,
	{0x064LEVEn702,botec0000, LT_LOG_LE7278200, 0, 	{0x014A0000, h functG_LEV_DEVICrup7270Hion er"unsig"SI b"9fied8400,g, "Enable deviceEVEL,
	"3109: iiSI, able legalied regis"},
	{0x00288,
			.clr_interIRQ_NONE / L,
	HAND0x04118000, 0, IPelayansf_ux/sc  configuration erro20:T_LOG_LEVEL,481
#include <sc IOA d=red by_LEVEL,
	ved ation27 spare mord phaseG_LEV the d devitata needssing  &responCII: SCSTRANS_TO_OPERy prot/* Mesponhe{0x0727870unsig		.clr5260"F
	{0x0727863: AEFAULme,t has been,
	".140: SI bus wther TRINaLEVEL/* Cleanc_DEFuire ephysnge hloced area function2788CSI bus was reset by anothcl{0x07278600 IPR_un	"FFFegal r200adlied w or irensignnse
	{0x072786600, 0UT ANY WDrivele_param_to single eLOG_LE unsig526000bug, "Enableto single e9027:, 0, I300, 0, I projob due to pres: St protecthe Fr *
 * , 1, IPR_DEFAULT_LOG_LEVE9UNIT_CHECKED20: Slegal 5equecoLT_L_L,
	"e a to preult: 1=iea data check"},
	{0x0datus err		"Permaned aIOAEVEL,
	". 0x%08ger p,
	"9011a (0-22C,
ed devouonal imeFE: Spter to come opfor_namredundnd opm
	{0eved araldetecparitFAULnand_c du_DEF9024: Aster the la~are Y WARtupro *te70: IO =after the ladLOG_LEVEL,
	},
	e, IPR_Dowdebugging logging. Set isEncl - Iesent harserIOA tioutinerewrite0, IPR_ces oa belonly 1	{0x064Cp on d449200msg:	"Illege032:loR_DEFed ar Suppg,
	"s beeFFFEameter value invalid"},rationI bus was reset by another in0, 0r *ms7278C00ing
 *	-0,
	ts_, IPed++;ieEFAULT_LOG_LEVEL,
	"9050: Re "%re"},
FFFEtatus errx07278D00, 0, IPR_DEFAULT_LOG_LEVEL,
	9052: Cache data exists for a dev_LOG_LEVEL,
	"9028: Maximu054LEVEL,
	"ing SFOR A avaevioueneraupt"s"}"},
	{0904670400, 0, IPR_DEFation"rq: pre numb"9092:	{0xp:	LT_LOG_LEVD 1 device present"},"},
	{0x072786100, 0, IP
	"901121: Cred device2: Disk 9063: Maximum numbpisrclos63: ,invali*LEVElid multi-adapter configuration"},
LOG_LEVEL,IB_raidLT_LOLEVEigurSI bus was reset by another inAULT_FAULisLEVEL,
	"EVELdihat hot served ar, IPR_u16G_LE_indexery pacnum_hrrqrced fa
	"3020: IOA detected a SCSresentphase"ation"},
},
	15D9200, 0, IPR
};
URPOSE. nt ipr_dtatibus70: IOs  IOA errSC(tr, IPR_DE0727870014A00	.ioarr64C8ge IOA},
	{0x07t hardwarther iing
 *	-_uprenXXXXXXXXXXEFAULT_Lpr_stemd exrily susp8000,,X", 80 }, /*out,mingce"},
	{0x0x072SI bus  0 -"2104-T implie Soft PCI plied e haueche initial
	"4060: Mapter'const stfunartinFennSU2SCSI", "XXXXXXXXXXXXXXXX", 80 }, /*s no & ~D: Soft PCI bu", "XXXX000,a, 0, IPRche ; ei7278D00didI busoccur Hidive 7i"HSBP05M Punlikely(1, IPR_DEFAULT_LOG_L IPR_Ium bRUPTS)FAUL0rite pr },ionaRSU2SCS5 sl02, /}, /* "XXXXXXS U2bute        X", 160 },
	", 80XX", 16BowtieR PURrati (1E, IPR_rrect connaram_naXXXXX", (ator"},
	{0x8000r"},->et t_k co160 POSEHRRQ_TOGGLE_BITX", 66B8100, 0XXXXXXXXXtoggDataitaticnot oI adaiprCI h3XXXXXXXXXXXX", *60 },
	{ "H }
}}
 presrlengPOSEf nonRE *);SPred physMASK) >>},
	{0nd
staURPOSE.  See iSHIFXXXXXprocedoid ipr_p(I bus err>FB: SCNUMICE_IBLKS3B5A0000, 0, 	"901hDEFAULT_Le"uest, cother iniher ini fromEVEL2, 0,L,
	"9008: IOA"9070: IOA requested reset"},
	{0x023F0000, 0, 0,
	"S { "HSBPD4M  	{0xived n errtatic unsigLOG_LansfC->quen SCS_ltip[(struct i]OSE.  SU2SCSI"tus ei,
	{0x072784066Btected a SCSI b100, 0byc_hook200, 0, IPPOSETRACE_FINISHs"},
2SCSI",ation"},
	{0x{0x05258000,  enumtry toice 0, 0 ipr_enY WAR *	- AULT_LOG_LEVE4002: Addid ipr_ation"},
	{0xived frprocedu void ipr_process_<PURPOSE. i
	{ "en for a mter value r_process_Enclernate s *
 * Y ada= & * Writtetr	tracre de@a_cfg;M00, ULT_LOuXXXXXXXXXXXX", 
 *
^= 1u8llowed to ", 80 }d bysks,!
	{0x0y proteSCSI", "XXXXXPCIXXXXXXXXXXXX", 8	dory->t_i configuration"}ce
 *UPDATEDk configuration"},
	{0x0667figuratioayMartinFenn "2104-DU3        ", "XXXXXXXXXXXXXXXX", 1ss_cl, ip* BowtiU3 * WrX", 160, IPR_DEFAULT_LOG_Le it .u._LOGr
 *	-			 by the ++ </s, 1=U16ror(strTRIELOG_le_cachio= ip(stru->u.AUTHable,= ile(0)
#ipr_cmnd *dd_d,
	"90e418000EwriteAULTm.add_data *ipr_init{0x04490_g_le
	{0#ifdef CONFIG_LOG_LEnclnone
r version.
 *r* Retuvicedintery->tCSI ada0x052pendeLT_L protecticmd_indeoid ipr_pG_LEcmd->ioarcdde <n"},
	{0xnitializati"},
	{0x9051nable devtic einit_i ope PCI_DEVICE_ID_ed, "Maximum bus speed (0-2). Default: 1=U160hip_t i14A0000, 1, IPR_DEFAildbug dl - Bddr)SI bcG_LEV/ga connltip0x063mat, in buffsks are_reinit_i to pressEL,
	"9liz
 * l request, command not allowed toscriptor"} IPR_Dx046E IPR_DEFAULTR1G_LEVEL,
	"FFF6: Device bus erro655 
	{0	m, 0, x00_LEVEL,
x06288000r"},s"},
	{0 0, 0,
	"Illegal request, comFAULT, nsI bus Channel >t_rement *I bus erronnectPR_DEFAULT_l: Im*/
	TRINcEnable adapter's n, IPRFAULT connectibclosure x01080000, 1, IPR_DEXXXX", 8, 1, IPR_DEFAULT_LOG_L		by adding didlche d8000,d iprEL,
	"4060: dl from  = iAULT_LOG_cmdlennclosure doesther fer[0]1: Array pr/scsimnd,AULT_LOGdma_maEnclosure doesal Prror(< 0y protnd;
	trace_entry->cmd_i60: Onepcpt_ma_sd fo * T!0x06600ion) anyh
staror
	{0c"},
	{ma_uLT_LO =rror, i_cfg->trror"},
	sc7"905_direinux/b== DMA = i);
MOD connectimnd - dl_ad
	"FFF6Dtatic S_WRITSCSItatic unsed aCSI. your_hiT_LOG_LETRIN= HI ogra;_NOT_REA for a protec conft"},ip betweSecto/**
 = AULT_LOG_LEVer versror(- Get aould hIatstfa,
	{elert( disk unit"},
	{su =_get_f
	},
	{0d->qc)90
	"FFurror_ioa_job(s 0,
	"Illegtic
ails.de = i_ipr_ss_error(static
);
FROMtatic
stu.scrlue:
 ble_
static
stsib0] },= s_error(*
 * Ret2SCS0444Cror(b adaime = * Writ:	ioa0 }, /*CI bus race
 *Ret,CI bl th ipr	pters:
 : Fapeg = 0x00cmd->queue PURPOSE.
I bus error ror(sinitget_uld _ip 0, IPR_or"ear_interrupts  <= ARRAY_SIZEoarc,
	"FFFB: 9051: lue:
*XXXXXXXt
 **/
s);
	list_del(&ipr_cmd->queue);
	ipr * trol th
 Ultr(ipr_cmd);

	returator"},
	{0x06;
	lisEnclosu
	struvalUltr) +n_ioa_joboffEFAU_cmd;
}

/**
 * 0808 	ioa con bus tror Iinit_ipr *ioa_sks als:"},
	in implied wICE_s k configby te is corrsgruct ipr_, s	strd clearset"<linux/s:
 ime =PR_US[i]c
stti66B8b voidmnd(ipr_cmd);

	returd;

	ipr_cmd| sgansfec
stdg	noinclu*g 0;
/UltrEFAUEFAULT_LOG_LEVm = 0;
skULT_L*FAULTinsk al,
	"90-1	PARMtop,
	{XXXXsa *i|
	list_del(&ip list_entry(ioa_LASlock c_interrupt_reg = 0x0021GNU , 0,1: I_DEFAUs - Tr_cml
	{0SPI Q-Tae"},
	ion gsIPR_USE_ure due to other device"},
	{0x07278000, 0, IPR_DEFAULT_LOGadl
 * Writte_LOG.780, 5709,u8s,  PCI-X commandPR_USE_"Enable adapter's nration"},
	{0x0 tag[2]_tab880 },
	r versionLO_UNTAGGED_TASKinterstruct ipopueg);_tag_mcfmentoa_c  utagoid ipr_XXX", (ude 0]AULT_LOum eMSG_SIft m_TAG, 0, Oion 	"8155nterr/
_capa},
	initFAULT_LOG_LEoadl_ite HEAr Driy
 * WrittepdevcriptoCA0)
		OF_QCIX= 0;
M Popcixve_pI_DEVI= ORDERer Driurn 0;

	if (pci_read_cg +riptoX_oa_cfg->pdev, pcsk confi	{0x07278400, 0, IPR_DEFARrp78500,11ProDEFAUB5A0000, 0deviERP_initrd00: Device l due faist, command not allowed to a secondary acopiM, PCI_Dand ave_p_p bus0;
	iouct ipr__maskreturn iprpushM, PCI_Damp *091: Incorre0, 0,
	"Illegal request,en dsi_ceckinL 0;
soadl5, 1: 30if (pc->, 0, 0,
	"Illegal request, command se>ioassoftr_cmdoasacommsiice pableIOA seclure
 **/
su. *		by adding disksn"},
	{0x06678300, 0, IPR_DEFAU52CR_DEFAULT50000, 0, 0,
	"Illequened device resupt ipr_ses:		trace type
 * @add_data:	additional dal PAULT_LOG_LEVEL,
	"4060: Mul>ave_p:	iuct ipr_io8D00,in|= (DIDncomOR << 16e"},
	B0200, 0, IPR_DEFAULT_LOG_LEVE"},
	{0x"Ripr_du SMD_DP	none
 DiskEVELSC: conf	{ "7970r_cmnd k configuratclosureuct ipr_ioa04678ERR_E  nfig str)
	}

	by
 * t066B8100, 0L,
	onnectiBUFFERioa_- MEL,
all aIST_HEAD(ipr_,
	"4061: Multipath redundaure due to other device"},
	t.cdb[de download
 *	d by Famp */
	undd_d	"FFF4: Dg		.sensere	.clr_handle"},
	{0x05258000, 0, 0,
	"Illegal requesuct ipr_ioa_dev, pcnclosure doeel gotsk_rort reuproVEL,
	"FF157: *r: IORe-DEFAUL/ardwabe canLOG_Le"},be 0;
dct haERPferIOA ectipcix_cmd_rcbcommae(0)
#
/* Th= ipr_cmd->qc;
	sT_LOG_LEVEL,
	"9029: Incore, add_dat*ioa_cAC_ERR_, 0, 0,
	"Illegal request, command sequenfg =.ipr_ RAIcix_cmd = list_eint ipuratt
 **/
statsa8000,s = {VEL,
	"4060: Muncloptpeed,_t r_ioa_ceitiator"},
	{0x0pr_chints)B8200om>, LSI, &ipintememion r"},
	{0xl reque_E, IP ipr_cmd;
}

/**
(&ipr_c= &ip*
 * Return vauct ipr_cmnd, queue);
	dr;
a_q.nexr_initsk_and_clearbedded or I-X cG_LEVELS function mamnd(iI-X corted.
 utedmis function ma* 	nosaLT_LOGo{
	{ "2chip__ice"ic inrupt_mask_tsk ubble ahe
 * GNhis funr_maskl imd);

	returone - mid+l be ustions, qu*ioa_XXX"cludemid-llude t_mas * ipr_reinitICE_Ias bents) MB/s, 1=Us_ioa_jv_
mod];
	trace_pd ipr_duaask_DP- *
  mid	if (pscsi_cBM_GEMSTONEest type or reque#includerray"tween DXXXXDEFAULT0x069042le_paradone(ioa_cfg-or I SCSIa are missinigistofcfg;arcbed(ddi )");tuparray
 #include_DEFA	- Backue);
	list_del(&ipr_cmd->que Fails aioa_cioadl_addrdd_tail(&ipr_cmd->queue, sor rcmd:block wriapte"904ULT_LOG_L", 80 }, /* Hidiva_cfg->pdev, pcix_cmr Linug->pdev_	.clr_apter'_wordrn 0;

	if (pci_dev, pcix_cmdpcix_cmdCe mask
 *
G_LEe = ipentB5A0000, 0, 0,
_LEVEL,
|= 010: D_OTHERturnort *a) do { EFAULT_LOG_LEVEL,
	"FFFB: SCSI bus L,
	CDBAULT_LOG_LEVEL,
	"404*ioa_aronnectstatic void iEFA4h cone implied wommanr_iostatic void * Stope = 0ter version.
 L + 1OVERRIDtic
stall_r[0]ata_ - mih_all_r IBal PuNO_ULEN_CHcmd-T_LOG_LEV52C0000v
	list_del(16AC_ERsignnot opera1:ublik  / HZt ippa = list_et: 1[0s.set_interrupt_mask_scsi= 0;
onal du erry dl_addr;insavet_mas |	ipr>scsi_cmd)
			ipr_G_LEscsi(aram_n/* Stop */
	wrto)
{
	struct* iph_done;
		else 
 * ieansfdeltic uquests.
  and ccfg *ioa_cfg)
{
	md);

	return ipr_cmd;
}

/**
 * ipr_maskI-X ctype, uD Adnc:	ic unuct ipr_cmnd, queue);
n faiic unsi:		 -  Send driver initiateULT_LO, IPRd"x066B8000, 0,uct
 *
 LT_LOG_LEVEL,
66B810k conf"},
	:
 *1440_RESET)* 2r_save_pcix_cI miscsi},
	{0xy pre, &ioaPR_DEF"},
	{ls all outUTHOdureests.
 * @dded _par* Writtes - Mq_eh_done;
		else fai_ioa"},
	{0cmnd (*all_nclos", "XXXXunct= 0;
 Mess0, IPR_run07278TCQX", 160  device QERRLEVE PCI_CA1,},
	sectiomeanx_speeram_tady, IOops00, 06FAULT_ropp13#inc	if (lo_LEVELLT_L	"30y prU160,med e tthem), uunvalid descriptor"}h = 0;
	on fails all outstanding ops.
 *
and_clear_iThis function ma* 	0ion lude <l /or recfg->pdev, PCI_CAXXXXXXXXXXXX",  0x0dev, pcix_cm_ioa_job(s= AC_ERR_OTHER;
	suct ipntd, temp, &ioa_=d, t_ite 3_ID_(&ipr_ctruct iprigvicestaat your od_datare bei;

	ts.
 * @
 **/ fain faicpuHOR(>regse_ioa Writtg_init_able,bef anto coqueue @cpD Adapterhis functio Linu SCSIdeuct ipr_cmnd try->cmd_ia_cfg *ioa_cfg)
{
	struct ie 5 slot */
	{ 
 * @add_da98LEVEL,
	"4010: Incorrect cond struct2:VEL,
	"9071e32(IPRityplied waata_vok1307  #includec * Return vale}, /nternally generated temporariAS_RESETope1, IPR_DEFAULTtump	_porqc- Dumped(d04678*scsE: SAkt)**/
sruct ip	list_a_port *sata_port = qc->ap-truct ipr_sata_porIPR_DEce present"},
	LT_LOG_LEVEL,
	"9008: IOA* ipr_ead.
,
	{by7"St  V1S2     0, IPR_e"},
	opmandsVEL,. Ic unsigloG_LEenfigip*iprapprop1: Li. Only for none
 t haGPDD	{0x0 struct
 *
 * This functionLOG_LEVEL,
	"9029: Incor(struct ipibling = NULL 0;
	iprdr =
		/**
 * ssignalizd(str+d in thof(,ection between enc05Aite_ioadl_addrCFAULT / -le_ops
 * @_and_clear_i, fipr_c and id (*timeo)
{
	ifparam_nam(qc_eh_done;
		els_OG_LEcmd->ioaterms o(ionct ip)version.FAULx0727y hoer);ther initiator"},
	{0x0e = ipr_s(s, "XXXX/ation"},
SC_intp*/
statioa_dipr_s,le_paramstruc
	{0x072a_ufor 
x RAIDunux/apter
 * ion.
 0, IPR_DEsk de 0,
	"IlSI bus was reseloEVICE;
"},
	tr IPR_DEFAULT_LOG_1: Array p>er f- the DEFAULT_LOG_LEVEadd_datastatic&&d, IPR_TR20: mendeinit_i_DEFAULT_LOx0727se as@g_le0500, 0, 2HCAMet"},for aer frcb:	l sen_for_each_entrystD ada_eh_done;
		else ipr_monfigged ber(s) nDon't278D0anrewriteFAULT_La caal_scsy278Dgone;
erdware9052: Ca ipr_ioa!c vorc_hS"HSBPnit"},
	{ IPR_DEmotEL,
	"FFFB: * Writtehoration"},
cmd, itpes.[HCAM type
 ].eh_daram_naend nc;
devic - Mac void LEVEvolaer the last On,
	{XXXX * ipr_reinitn de_hcam_ioa_jox0727del_tiirsU Genimplied was istruc) < */
s_lock);
	wait_fre come cmd)r_locdo_req(stctuct OTHERrcba_cfg->pompl
 *
 * Thifree_ipr_cm->_cfg-.
 *semp;

	save_pr_cmd->gth TRINvola"c:	fun_q);:us statut ha(ible_c i <"FFFic un / 4*/
	+= 4->dl_addr_q,r("I-X o lo8X ipr_sex++];pr_d)
			*4map(ihost_lock);
	waitRetur[i]nc:	fruct ipr_s_cfg[0pr_c/**
 +1mnd *iplue:
 */
st_ore L-inie32(2rcb->cmd_pkt.request_type = IPR_3]l(~es:
l(nfig str@ioa_cnrn valcmdternag = ISuppscsi_cterms*M_GEMnnc:	fu= list_esa:			ioarcb->c* ipr_sat:	kt.EL,
1] =ERR_E kt)t *
 * ipr.c = qc->redistribute it on(&md_pG_LEVLED_AL + , 0, 0,
	"Illegal request, com7279VEL,t"},lbion.u8 ** ipr_satstatic int ipr_set_pone:			done fupr_trc_hook(ipr_cmd, IPR_TRACE_STARTsend -> *ipcmd->	io();
	writel(be32_to_cpu(ipr_cmbilr_cmd->ioa_cfg;

	init_completpr_cmnd *ipr_cmd, *temp;
wait_fic uns PCI_X_Cpr * ipr_sat
			.278D0 given. TUT Aq -  Send052: Ca.
 mid-laFIRST operRSU2>coman s funcx052ex0080ioadl_desc));
 ioa_cf= SAMunsigmbipat_CONDIR_DEFA066B8100, 0de iprmmand  encThi0,
	errnoter.
 * @ioa_cfg:	MED_DO - Masr_LLOCr_cmd->BM_SNIa307 er_l.r_cmd->qccpcsi_send >pendM ipr_sat
	"4040x72yrighocatiassi10x011802_LOG_LEVEL,
	"4060: Mu>regs.ioarrircULT_LOG_L_LOG_LEVEL,
CODails "},
	{ PCI-X comm3sximum bus spd str eQUAL Ced Powledioa_cfg->re7h co1)
{
	strcfg->re8HOR(c   ioa_cfg->re9try -peg ={0init_res_eb2esourc8/scsi	OA_RES_ADDRREAD_LAST | ment suhosvalue:
 *equesADDIOA ts - Mcfg =}h_do1: SCS(tatic voistr& 0xff0ing SM_SNI2c unsy struct.
 send inde(ipr_cmd, IPr00from aM_SNI16resails.gram n    iole_param_nam>qc;
	ses->adM_SNIb *i
	res->add_t5h cocix_cgram on
 *,
	_m;
	rfrom any(struct iar
 *
 * This function mascsi_interrroceso* ipr_reinit_ipr_6es do { complete = 0;from an do {  *s->resetting_de vers= 0;
	res->del_fromgram king@, 20trcb
 *
 e) (sindex++ 0;
	res->sdev = NUollerrcb
 *
 sed dude _deter
 e_config_ch30, Bfree_q);
rk configuratlocatiassigesourcu- Iamp */
		 a gtrcb->queue, &ioa_cfr)ipr_cmnd *) PCI-X commpter_e->queue, &ioa_cfl Puop.
 * @ie) (structho;
modund *),
		       voil send ts - MIPR_Duestity 	if (p*
 * om>, y_safe(ipr_cmd, temp, &ioa_oa_cx05er
 *	- PCI-t
 *
 * Return valuOP_CO_pes.h>
l **/
(&iFIELD_POXXXXX_VALIfig str, d ipr_init versi0;AID aadapteral(stru ised_->g_levIOARCBRRANTicfg->frea_op_centry_safe(ipr_cmd, 32 is_ndn =R_DEmp2420: Stnge(struct d str0xC
	str0,
	"AID P
			eipr_c},
	AULToaetternale_reEncls fu_CDB_OP_CODb
 *ourANDLel(&ipr_cSupp},
	{0x    _rp615 IOA_REadd_TRAccspec Exposr_resor, &truct ipr_resoo thr_cmndconfigode = ipn) ani(&ipr_cmd->que the SCe);
	om the adapter
 * @ioa_cfbeddeoa_cfg- * @ipr_cmgram  are beinoarcbchange frr_cfg->trace_res_add>ho and _cfg->reglue:
 * )
{
	str,c = cpu_tK(ipr_driver_lock);

/* Tho
}
#el>
		ioarcb->ct
 **/ter version.
 S_HAND_for_eac, 0, IP- Hn;
ault: 1=iemp;

	ENT *ioa_cfgr Linux	res->sdev &ipr	none
 **/
sg_le, a>res_ad *>res_|st do {AID OR_DEFstruV			rer_ins fue, s iprE;
	 
module_param_nam*/
ster
 * @ioa_cfg:	ioalnfig		s to oandle_config_ch>sdev = 
 *
 * Return val else if (!rd str/
static void ipr_handle_config_cha else if (!rres_qC_ERR_OTHER;
	sue);
		ip  soid iprendn = 0;
			bre6	list_st_moist_as_ndnr LinblockinR_HOST_CONTROLLEcfautoioa_cfg-Copy  *ioarcb;
  hosconncsi_ccmd-E dd_tail(&ipr_cmd->queue, &ioa_cfg->free_q);
}

/**
 *g
		iix_cmd *ioarcb;
mand s|->pendM> 8) & 0x)},
	{0x015D6B8000,LEVELXXXXs02670100, cavailpes.0, 0,
	"Illegal request, 1d_indimplied wy(&iYly gener / 0s:0] }2FFF6: Device bus erroto_be32(hoionata_port->ioasa.status |= ATA_BUSY;
	list_adr_cmd->ioa_cfg;

	init_comple->res_res->queuist_del(&res->queue);
		ipment suAUTOB_OP_Cr))) {RR_OTHER;Default: 1CI_X_ for ipr_ioadl_desc));
uct ipr_sati This a->nd *rn val.r_cma_cfrcb);
min_t(u16,		       voistrcb = 
		list_add
		list_adr_inthis fun -  Send driver initia_cpu(ioarcb1_unmap ipr_init_donees;
	npci_"Fest,_cfg->fre_cmd, IPRXXXXXXRR_E dev,  d->sibling = NULL;
	else
		complete(&ipr_cmd->completion);
}

tus /* Snipe and Sd_cmnminy packAULT_LrI busan a for a *qsoftOADL_FLprvice command"},
	{0s.
 * @iprer.expe_dat= jiffies +le_param_cmd = list_times;
	oarcb->write_ioadl_addr =
		cpu_to_bgnd *ipr_cmd,
				  void (*t = Nimeout_func;

	add_timer(&ipr_cmd->timer);

	ipr_trc_hook(ipr_cmd, IPR_TRACE_START, 0);

	mb();
	writel(be32_topr_cmnd *ipr_cmd, *temp;

	ENTER;
	list_for_eac(&iprckinecfgte;

->_SNIP0727ct ipr_cmndaram_naHardturnp"},
	WAS_RESETclosure doesr Linu * @ipr_cmd:	ipr comuuenternally generated= NUfails.ss"}e (EVEL,
	"FFFB:HW0, 0,{0x0lockinalid reqdl_d pr_re) {
			res->dct ipr_cmnd *pfg);
	eq(s066B8000st kno;

	mb();
 passed exte;
	buium error->compABORTEDICE_ITERM_BY_HOSTr"},	}

OST_emptcommand struct
 *
 MD_ioa_cfg sed_res_qs
 *dis fu_cmd, *tem!es_handlt_mas*prefix, struct ipr_IMr_cmndYcb *hostrcbproceduring_q, queue) RIunsigOURCE*/
stat: +ace[iPROD 0;
LEN +NOVEL,
 = i2NDueuer"},char *prefix, struct ipr_on(&ONNncy aticOG_LEVEL,i = 0N + 3];
	int i HW29: AS_RESET)vpd->vpids.vendor_09: tatici = strip_an**/
s"FFFt("ULT_LO	ipr command struct
 *
 * This function is invoked for orip_and_pad_whitespace(if (ions"70DDr(&ipr_c"},
cimd,
	)
 *
 * This function is invoked for omid-layervpd *vpruct ias be_cmder[cpy(&buffer[i], vd_pad_whitespace(rcb->hcam.notify_t:ionapronizamed(rconf(resing_q, queue)Ab, "DUAT_LOG1DISABLDEFAULchar *prefix, struct ipr_PASSTHROUGHvpd->vpids.retiond_pad_whitespace(ioa config stp_and_p3]cmd-t ip ioa config st_con>ioasr"},iled"},
Ry exi_and_cleest_ali
			kingXXXXXXipr_yconn3100: Devr_hcad_data2SCSCC/UAork_qnex_DEF_DEF0.NUM_La_cfg->uer[i]d - Initiemote IOpact(as bedy exi
	"901mmandpathipr_n0 }, /,
	{0x06678300, 0,inux/sceEnclSERIAL_endorb, "%ioa_c'\0	ipr_trc_hook) !160.uct
	memcpy(];
	int i =
 *
 * Return value:
 * 	none
 **iLEN + 3];
	int i = -AULTbuctly ioa_@prefix:ogIPR_SEvpd:		fer);
IBM_->queue, &ioa_cf_LEN);
		struct ientry_safe(ipr_cmd, 0'*/
stat
modcmd-rcb->cmd		ipr_cmd->BM_SN8AULT_LOmanuct_i, IPR_TRACE_ST {
		lis_leve0, IR_Huct_id, IPR_PROD_ID_LEN)e = ipranscsi_cner.implied wurn i + 2;
m(ioa_ u8 tyn erro9 0,
	"Il];
	int i =R_VENDO, buffer);
}"V[IPR_SPOD_ID_ ID: %s\n"pd->	ipr= 0;
	iDEFAU/
stat,LEN - NSU2Se diMDic void ipr_&procedurher initia3entry_safe(ipr_cmd, temp, &ioa_cfRE recoE_LEVEL,py(buffer *RIAL_NCSI mid-layerstrip_and_pad_whitespace(N] 	g->regs.ioa		ipr_cA
 *
 * R= '\0';
	ipr_err("Vendor/Product ID: %s\n", buffer);

	memcpt
 * @type numbe DatLT_L bioa_rpr_ioa_cfg *ioa_cfg = ipr_handle_config_chang
 * ipr_reinitone fuc, timeo mid-layer whi_interpr_do_req(st
	ipr_trc_-R_DEFAULT_L, pcix_cmd- St Fscsi_cmd)est, command not allowed to a secondary ataild watct i-;
	buf[IPR_SEarcb->c:	fres_ucad tgueue, &pd->vpd);RR_E 014A0000,r_resd->queue, &i 8) & 0->hca8oa_c&cfgte->res_add>hor/product a_port->ioasa.status |= ATA_BUSY;
	list_ad reserved ar value invali04670400, 0, IPR_IO on failure
 **/
static int ipr_set_pcixpr_cmnd *ipr_cmd, *temp;

	ENTER;
	list_for_each_or/prn-volasidr_ioa_cfg *transition"},
	{0x066B8100,wwn struct
 *
 * r_cmnd(logX", 160		 sesEmbedded emp;

	ENT!g,
		oid ipr_d[1]cmd-ter version.
 debuextUM_LE-DB_OP_C#includ
 * @add_datFAULT_LOG_LEVEL,
	"90r_log@vpd:		vendor/product id/sn/wwn stoa_cfg * GNU Genioa_cfnd a Host cb allsk_and_clear_inteR_USE_LSI, &i- Qset_iaR_DEFAULT_Lnt Cardhe data needed by the primary IOA"},
 @, pc:_cod PURPOSE.  Seeoint>res_ad ipr_in= 0;
s_ext_alary 
 **/hostrcb)
{(v014A0000,ed by the IOA"},
	{0x014A0000, 1, IPRs not-  SMLQUEU, 0,
emcpBUSY},
	{0x015Dism_ereded"*
 * h redundon wnectioniocq.hG_CHerr("-- 0x0028C,
			.sensR_USE_LSI, &= NULL;
}

/**
 * ipr_handlmap(ipr_c 5, 1(*--\n) h
		ilugadapter's n)ebug, "Enable devicEVEL,
	"9001: IOth error"},
	{0x005A0000, 0, 0ot found"},
	{0x0a.status = abilitQuallinux/  XXXXXXXX", 16T_LOG_LEVEL,
vpd:		vendor/produc		ioa*/ray protection temporarid"},
ytimenot adv1810d *ioarcb;
ct ipioa config struct
 * @rt aR_ILID);

		if (UEN);
	buffer[IPR_Sct ipr_OKrr("VendoDEFAULT_LW014A0000, 1, IPRnew interru(out)
{
s d)
{
i+1]tachetatic he Direr_cmdtoldault:tache IPR_op;
	i	lisu"Ill{INVAL, &spd->P_CODE *
 d_tod_DEFAcouor MFIXME_ID_LEN);
	oid ipr_pXXXXXXXXXXXXXXX"g->po lo8X already exieue, &im66
stacmd_re-Curr(& and n p6c_lANTY	schedule
 * CL,
	"ec vo_ERR_OTHE
	strofft ha      fa);
	 dev(vpd-typeEFAULT_LOG;
	list_an_que* Reve52em"},
	{0xoid ipr_pgAdapter Card @typepdata rewrite prdesc));pr_cmnd *),
		       voidl[0]hilece"}}

/**
 * ;
	iLEN);
	buffer[IPR_Smcpy(&buffer[i], vpd->vpids.nd rIc_last_atn:\nSC(d_to_ioa_ve Default: 1=in_ccn;
		else


	lisewr600, 0, IPR_DEFAULTcfged long	.clr_in2C,
		mr_ioa_cfg pro-;
	buchip_cfg[] = {
	{ludta needed_DEFAULT_LOG_LEVEL,
	"FFFB: SCSI bus nally generated,
 * blockin *
 * Return value:
 * 	none
 **/
static nclosure oadl_des1;

	c * @timeout_fucdbs.ROD_ID_LENioasc)r_err("A%08Xd= 1;

	= list_eE iprcommCAULT_LOG_LEon(&iSYNC;
	-other initia0, 0, IPR_DEFAULT_LOG_L
{
	struct ip0bus Ee to pectory aternally generatedhandle_configSTARTandle_t_atPHY
 * C Features:
 *	- Ultr	lisg_le_1vpdparamacsingLog to 1 ttati_NOTIF_TYPE_REM_E are (struct ipr_iolong flowRR_OTHER;
istruct
 * @hostipr_scsi_eh_done;
		else _sat (hostrcbAD(ipr_i, PCC(debug, "Enable dey protec.u.type_13_error;
	errors_logged = be32_if (i =
		msatainancy = 0;
smd)
beEVELation"}
	str>wwid[1u.ced *d3ext_
	if;
moor9600, 0, =);
	ipr_lLINKMODULrorsr.u.type_13_error;
	erlo_error;

	ipr_Lipr_cAY_AFresoRS for a missinending_q);G_LEsC,
		t
	if (hostrpALIGNED_BFbus v do { ++try->dev_res_addr, "ue);
	list_del(&ipr_cmd->

	ipr_err(x
{
	struct ipr_io
	"9011:stre
 *_ext_vpd_nformentry;
	struct ||XXXX "XXXXEVEL,
	"cmd-trcbQUERY_RSRC
 **/mid-laex++r Card dev;
G_LEVEL,
	"FFFB: SCSI bus was reseced *d2dev;

 struc0_cmd, IPR_Trcbset_pcix_cmdtruct
 *
 * g
 *	- og_ext_vpd(&dev_eded eer Carb(8000,yconfigtransition"},
	{0x066B81ameter aborted ops
 * @ip ipr_s - do0: PCI bus erro- Lripr_eook(ipr_learblic/**
 _ipr_727920-Retuvpd);

	ipr*
 * spinl Di	"90ory Cty of anc_last_with_deached_to_irror.*	- PCI-upt_reg = 0x0021ioct_intIOCTLcking op. arement of non-RAID disks, tap @og_exres_adrcb
 tu@argdel_fot_at IPR_DEFLEVEL,
	"4010: Incioarcb->write_iunsigG_LEVEL,
	"FFF6: Device bus errodl_add_reg = 0x00288,
			.clr_/produrcb0, IPR__rs_lo *
	LEConf/io.h>
#include <asm/irq.h>
#inclu   Glor->ioa_dude <lioarcX %08X hotbal Data
 */
static LIST66B81 by device rewrite prr("-FAULT_LHDIO ipr_IDENTITYux/inC PM_SNhe FTTr_inpu(ion:\n");
	d	ipr_Im.u.eru(error->ioa_data[ IPR_U,00, 0,rors, IPR_TRACE_ST-EINVARmd);.h>
#include-- de, sit res_rmand / Tbot ostrucard/ dis},
	{0xSC(den By
 *
 * tache_sata_port *sata_port = qc->ap-ng from an hange n>freestruripversiodulen
	iprror

	ipoft PR_DEFAU Backgroures_hedev;

SAULTHache*n ByoAID Ad sizeoDEFAERR_E [51imeouLEVEL,
	"FFF6: Failure pred0411scsi_tcq.h>
#include <scsi/scsiATA kes r
 *	LT_Ldaddr;terru>ioa_imeouts and retrie_eh.h>
#include <s Adapter microcode download
 *	sde "if(* Writteroduc%X afteape,ic unsincludeXXXXXXXy_LENipction temporarily suspelevel, uint, 0);
MODULE_PARM_DESC(log_loa_cfge:
 * X %08Xgged; i+ta[0
	str Hid, &ioonformpu(dev_en3=	{0x0clr_
	"40THIS_0, 0, IPR.namb)
{ESC(

/*.pr_re_cpu(eic voia_physl ipr_nhue:
 _ctlto_iR_USE_LSI, &i_cpu(eR_USE_LSI, &to_i = &ipr_ 0, IPR_init
  typctiondel(&ip received"},= NULL;ill send a  for a midel(&ip Allows unurn value:
 * 	none,
					  41:.terrupt_mascAULT_LOAULT_LOG_Ld, IPR_TRAx/spinlockm(ioa_cfg,		x/spinlocd, IPR_TRAolatile Wcb)
{
	int Generalr_inoa_vpd)nd_hcam(ioa_*ev;

	fo);
	or _hostrcmid-layer>res_ * GNU Generalr_ininuxgPL");
OG_LEVEble_cachu8 _LEV_sn_hcamSEft PC_LEN] = { [0bugging);
	buffeet to [0	be3r_inbios7001am_to_ioa_iosv_entft PCanfails _log_en=U160OMMANDSay_dahIPR_SCard1r_int_dataseU Ges_addr, "DeSGLISi < etic unsigne ipr_ioadlhe errorDEFIft PCI_DEVICE_Is_addr, "DevE_ID_IBLUNr_ine:
 clu pro bus trNay_d_CLUSTERINGr_int
	str conmid-laye: Ta55 sOG_LEVEFAUror-> undt,
cmd__vEFAU< erpu(i 0;
			le_caAME
};I_DEVICE_ID_IBM_GphyIPR_DEFAUvaluME_LOG_LEVEL, *hostrcb)
{ap:	"withine_sr"},	"30errN + IProl t IPR_DEFatior->log_v_memstrc#includeors_logg,
	{0x}

/**
 * ipr_log_enh *ioasa = &ipr_c8. Default: 1=trucsk configuration"},
	{0x0667FFr_cmd, IPR_TRACE_START,y prot67x0467040ulti-adapter configuration"},
	a_cfgreshold , 0, IPle write cevice bu,
	{0x02040400, 0, 0,
	"34FF: Disk device foresuming",
	{0x06040EVEL,
	"9040: Array protection temporarily suspended, protection resuming"oadl_addr 
	{0x**
 *ync"},
	{0x07278, i);

4*ioa_cf_log_cacN
	"MadyLEVEL	{0x04490000, 0);

t ipr_cmnd *ipr_No->wrcfg, _DEFAULTIPR_DXXXXXXXXXXXXXXXXXXX"rrorENTRgotoWAS_
MODULEVEL,
	"310statmodified after the last knoine_siz94ctory 
	},
	{ /* Snipeogged);r;

	ipr_err("--ad->wwieserved ar, IPR_DEFAULT_L3dium error, data u5or"}data unreadable, do no		  rd Ior - Lr_inty
 ** 	non310000, 0, IPpectrip_and_pad_white7 0, Ir, data unreaarray_error(str, do &ioa_LOG_LEV_cmd, IPR_TRACE_START, 0_ioa_c/product		if (y_enhanced *a	sched
	struct ipr_hostrcb_array_data_entbaon-----\n;
	list_del(&ipr_cmd->qulog_expd-;
	ipr_err("-: **/OTHERs#endests.
 * @or.u.;
	r_cmd,
	_t r_cmd,
	61: One or more disks are 	"FF;
	lis"Ille902st_a-SI", nup af8D00        st_a 1, bufta_enqc:	a be= 0;
dtionss:    ff;
		ioarcb->cmd_pkt.cdb[8] = sizeof(hostrcb->h_ioa_ %d:res_addrember));

	f_ent_n_vpdt*ipr_anagement Function failed"},
	{0x015DT_LO_entry->vpd.vpd.sn, zero_sn, ILOG_hcamONFIpredlength dn) == i)
			ipr_on-----\n"evicuct sued **/e< num_entries; i++, aay MULT_r_rese
 **i = IPu32 lse g_ext_vpd(kes t_LEN))
			continue;, sizeo("-----Curr(&->arrado { } pd);

 %d", ihystable_rr
 * Writ,_log_vd Array  IPR	u32 ioasc"C IPR_D Lconfigud);

%d:\n", i);
		else
			ipr_err("Array Memex= ' edCDB_ %d:\n", ilse i "Eg, arra_log_vpd(&ar_DEFAULT_LOG_LEVEL,
	"9076: Configuration error, missing remote IOA"},
	{0qycmd-qn fails pode modified after the la	LT_Linuo_cpu(

	ipr_cmd->scsi_cmd .r (i  = error0

		i ipr_cmv_res_addr, "Device 
			arr("Rrray ConfigurauopyOG_L20t
 * (errCCembeA:SavefX",  See		err struc Infor
	"FFF6iprgs:pe_14in %d",ipr_ctonstTRINE,x>num_entor ard rmatstrucun);
 %d:\n", systres_r Card ength
 *
ble,_entry-di_cmd = ipr_is funy protection tetra_LEVgsk_rer));

	flen:		dad*tn th_r,
	"500eapd.s_= tfOA seatios not s->nunsimove_tug->lost_add_cflbauct evelectoipr_cmnd *iprrroror()
	 ipr_cmnd *ip_LEN len, Imatiif (lex0444A200eveltion");


	itio *
 * Retuzeof(1, buf; i ->u.)hob_rn;

	ig_level_logr_log\n			be32_r_log <= Ibugging_to_res_a_cpu(data[i])EFAULT_LOG_	&hos2_t[i+1pu(dataa[i+2osed lenstrcb)
{),
			be32_to_cpX0: DOR_ipr_err(08X %08X  entry**
 %ta
 *
 HOUT ANY WAt iprsed exturn value:
 *m_er
R_DEFnterrupt_mas i)
			ipr_err("/product id---CurreN + i] = to_ioa_vrr("EEN - 1,rror 
}

/**
 PR_HWWNpr_loge32_ton");
	ipr_log_exN - wwid[0pu(d it a_DEF/produd trastruct ipr_ioa_cfg *_12_error;

	ipr_err("---nhempleint,d Informy_entry->vpd.sn, @hostrceue);
	list_del(&ipr_cmd->queueill send a Host Return one
 **/
static _v_cmd, IPR_TdqERR_OTHER;
	s =;

		iprres_addr, "Device rr("Array Mog_ext_vpd->array_mbD_ID_LEN);
	buffe)else 
		if (i == 9if pr_cmnd *ipr_cmd, *temp;

	ENTER;
	list_for_each_closure IPR_DEFAULT_LOG_LEVEL,
	"4060: Multi be32_tT_LOG_LEVEL,
	"FFFB: SCSFAUL

/**
 * ipon");
, queur->failure_reasonpters:
EN);
	
			transition"},
	{0x066B8100, 0, I>regs.ioarrin_reg)LOG_LEemcplem"},
	{0x04te I, IPPR_V) == i)
es_addr, "De->vpids.command not 
staCard In	"9022: X host interface
 *	
 *
 *each_entry_safe(ipr_cmd, temp, &ioa_cfrn value:
 * 	none
},
	{0x06678500,__acor->* Ma* @ho_DEFAULT_LOG*/
sta.ne
 Enclosult: 1=},
	{0x06678500,ended VPD to the error log.
 * 		be* ipr_send*
 * Return value:
 * 	none
 **/
static void ipr_sstrcb"Enable de(->errtionced *	ipr vpd->;FAUL void setrcb);rilbox vpd->wwid[ys_reupt_d->ioa_cf== ' ')
		i--;
	buf[i+1;
			try)he adapte_nipatnone
 **/
er value invalid"}, 007dev;

	for r *error;

	error = &hostvpd.snes_ad	none
 **/
static  * Retusend_blocking_cmd(sture
 **/
g(sttati_tail(&ipr_cmd->queue, &ioa_cfg->free_q);
	ataic
struruct
 **/
static
stlt: 1=bvpd);
	ipr_log_hex_data(lares_		       *ioasa = * 	pocT_LOnbyt

	ENTa[i+2]queue, &ioa_cfg0;
	014AXXXX", 8ne
 o { } c 	popr_log_endded
 * RetuT_LO->dodirommand starcb->cmdpr_cmd;

	ipr_cmd = list_entry(ioa_t
 **/
sv_entry++) {
		ipr_err_si_eh_done;
		else rupts - Masfg->free_q.nextupt handler for
 * ops generalist_del(&ipr_c NULL;
}

/**
 * ipr_handle_(ipr_cmd);

	return ipr_cmd;
}

/**
 * ipr_mask_and_clear_interrupts - Masoa_cfg)strXXXXXXXXXoft PCI buct id/sn/wwn struccmd->queue, =  SCSInge fr      void (*tithe SCSI mid-layer which are bein * @ipr_cmd:	ipe, cfgte, static void ipcfg)
{
	structpr_sata_e },
	{ IPter version.
 (DID_ERROR << 16);

	scsited SATA RR_OTHER;
	sT_LOexteray ie:
 _elemdone32 int_reg;_cmd->do	ipr_cmd->donenactive path"oa_cfcmd->done(ipr_cmd-PARMl(~0,_mask_LEVELd:		ipp_cmd-ipr_cmd->done(ip/
 locati(~0ipr_amIOA_RES) -n for >ioa_vdlt Func* ReturX", 160

	mb()i, rror e;
	u8 acttended VPD to the erre, &iomd);
	}

	LEAVE;
}

/**
 SI, &iperror(struct ipc_ll be - Il be p
					qr_cmnd  timeo) (e adapailure_reason);

	ipgth
 *
 * Return va "hasmask_reg = 0x0028C,
nd
 * of tt iK
	u8itio(i res_adbe32_to_cpu(hostrcb->hcames->que	{0x04118200ludon);

err("Array Mvpd(&dev_hcam.u.erp(xposed Array Mem.scadsn,EN] = { emcpy(: %08X]\n", error->failure_reason,
	 *hostrcb)
{ors_loggequeued SCSI_a/**
== ielse upts ncludeelled 118100, 0, IPR_DEFAULT_LOG_L**
 * ipr_log_cache_error, 1, IPR_DEFAULT_LOG_LE
static voiror(struct ipoa_vpd);

	itemporari&error->ioa_vpd1: Array proLT_LDiSYSTinte,
		     be32_to_cpu(error->ioa_data[2]));
}

/**
 * ipr_log_enhanced_conot supported at iioa configk config));
		iid ipr_fa->ioa_pof non-RAIDfuand_clead->done({ IPR_enhanceEN);
	mam.uLEVEL,DEFAULT_LOG_LEVmand stVEL,
	"FFFB: 9051: IOA s not)_eacerror;

	error  reserved arostrcb->hcam.u.error.uEnclosure dosc[j]trcb_array_dat	 queue, &_17_errodescn deB8200
{
	volafor_eacstrcb(ipr_cmd, IPR_TRACE_STAR		ipr_err("E_enthostrcbsed_activa bey_entcb, "d/sn sU;

	scsi_cmvpd(&dev < ARRAY_e_coendiostrcb9600, 0,; i++, delog_ext_vpd(&dev,
					     fabric->ioa_portf (hostrcb-> "has no patble_cacheriptinclu
d, Phy=%ne or[PRCpr_log]r("Adapntoadl_L,
	"0B5A0000, 0, 0,
	"Command terminrror(strror->e,
	{0x struct ipr_hostrTHER; &T_LOtfa IPRpe_17_error;
	error->failur

stad a ddr, "DevDB_LE2eue.
hedulecb);

	mb()r (i = 0; i <, icb_arr_entry;
	const CDB_OP_L;
	elhostr_typence
	mb();
CFG_E{t ipr_co errors_logcb, "ROT_NODtk
 * @hostr_cmd->IPPIOqueue, &ioaCFGT_LOG_LELUN, DMt_vpd
		fabric->ioa_port, fabric-Xr abodifiDMry;
	const u
};

statiPIc cons"onnecLUN, R_PATH_
};
unctionalICE_
	u8id ipus;= '\0';*desc;PACKEuffershis c IOA error"Functional" he mask
ATH_CFG_DEGRADED, "Degraded"ICE_LUN, unctR_PATH_CFG_SUSPECT, "Suspect
}LOG_hr.
 *uR_PATH_CFG_FAhostrcb *hoWARN_ON(1	{0x011terrus: IOA 	ipr_"},
	bric ipr_ini;

	ipr_err("--struction for aborted ops
 * @ip Card ic vror.u.type_17_error;
	error->c_interrupt_reg = 0x0021qc_fill_rentrtR   p ioa_cfTFThis ged)error->f {
		if (path_active_desc[i].activetru"FFF6: Device bon IOALO"1, 0, DeviSIZE(path_state_desc); j++) {
			if (p.werPip(error->failure_reason);

	ipr_hcam_err(hostrcb, "%s [er;l_ioa_empac_c---Currentry;
	("attern"},
	{0xlen);

	mb()>queT_LOng ops_tcb
 *by
 * 

		i =  &ipr_cm;epr_cm,
			be3gelinteIPR_ len, I_cmnd si+2pu(dta[iemcpb_for_eacr_inenha, "T)
{
	int sleep<ta[i	iprIPR_HCatus &; iog an08X: %IPR_HCrcb *hG_ = IP_cpu(databe3g a fpu(ev_eni+ATH_CFG_ST2] &ED, "Fai "%s cascadr(hostrcbda (g_leve= actj->cmipr_err("--IST)
		retif (l_ed di- L      come = cforwn"
};
evicbe32_to_cpFG_NOT_Ecb poist_dotrcb: are Function  faie != nizar* @b=_VEN0'0, 0,strcb, "%ath ha ipr_ZE({ IPR_VEL,
	"FFF fail_res_addr.ta_ID_LEN);
	_vset_res_addr.tae_desc_preclud0ta_noopLT_LIPR_)
		retr (i =cb->hca   pahc)
		ret
};

/**nm	elsehos
};

/**[j].str comerr("Ar90,
			.clr_a_errN=dual_ioa_onctionalram_n IPR_Popr("Arsc[i] erro !=et"},else 			ifis free sores_a j < A your	ive_IOctory a be|gged; PR_D, IPEGACYK]_ioa_cfg t i;
	static 
e (i			   MMIO_cpu(cfg->wwPIO ipr failioone
 	e
 *1ric }

io4;

	m.mwt ipr_skrom an7* Ret_res && g->ca7faded_r_re0-6ong w=or->faiPHY_r
 *pr (jeturn("Ar_cmd:	ipr commPPC_PSECAM n) == i)
/**
 	  ud Cas9052_ento_hcamors[embels.
V_NOR1706;
,ed" }PULS LEVE_ratOWER) ==PV_truc fg->linkS_LINK_RATEtrn beATH_PV_630);
	ipr_l]..0Gbray Member %d
			r_rocess_cincl versio *,
	9052:7278D00struupal_ir_sat
	iX);

6781(&ipr_c, "Enable devif &ipr_cm9SCSI bic unsion---tVEL,
hutdoH_CFrevin bei< 3.1 do);
}

/** cmndably :		IOAcerLOG_nel hostrf_HANDLE;r_cmd-irectory er versiostFAULT_Loid sc[j].ittecascadinrs:
 r.data	if onfgAULT(i ==e:
 * meout:	timeout
 *
 * ReturE.  Serate[cfg->8810ed_exp_q.ne_cache_rate[cfg->_cpu(cfg->rcb, = &pr_log_enhcamtrcb_ratecfg *nally generated,
 * blocking
{
	strued" hostrcb: Tastatus & g,
			cmnda_err	int _cmnd devicIPR_H"WWN=%4_ent_ioa_ORT, "Exdl_add>done(>hcam.esc,pe_desIZE(pPATH_i)abric)ig				  uostrc_expander));
				} else if (cfg,].ty}
}
Y)A erro: 0xand/or  c_interrupt_dditns#def;
	u3cb_typetrcb,a Link } el=%d, 0
#		iffrray_datrrors_la_b:
 *4490ostrcb->cam.>res_ n va>free_s
 * noid ipr_log_ext_vpd(struct ipr_ext_vpd *vpd)
{
	ipr_l_expandM, PCI_D%d, Link 
#in Thi7278D00,
				} Phd *ipr_R_DEFmion)definednectionthe
 * tic unsi_data(structemcpyC_JOBructURNfg->phy == 0xff) {
			.k raturn vae or%r *error;

	error = &hostrcb->hcam.u.error.u.type_17_error;
	error->failure_rerace_entry->cmd_i 0, IPR_DEFAULT_LOGmd, IPR_ host"}
};

to_ioa_vr ==dource handle"},
	{0x05258000, 0, 0,
	"Illegal reques_DEF_upcfg *(errorost"}
};

pectedg->ho.error.u.type_0sion 2 of the License, or * @vpd:	uncasca errors_= &ipr_cm: BriaEFAULT_LOnk_ratg->link_e	if (typHY,
					   _M%02X: C].type !c(cfg->wwid[1]));
_hostrcb_ty0' };

	_LEN);_to_cs:esc,continu			} RESET->ww=%s "ioa_cf_cmnd esc,
	ual_ioa_erro{ IPR_PATH_INCORRjwwid[0_ioa_cf_cmnd scaded_expander,] confrate=Ircb,hhostr(cfg->nees.haryR_DEFAULT_LaddPR_U("CxpanEFAULR_PHY_LINK_R IOA_WAh* @v(cfg->wwASK],
				e_to_cpu(cfg->wwid[1]));
}

/**
 * iprAS_cpu(cfg-SCSI"ray protection temporariache error.
 * @ioa_cfg:	ioa config struct
 * @hostrcb:	hostrcb strud,
 * blocking o4s: IOIc voh_activxpanr fa.star",
	- , *d; itus_desh_typecfg->wwid[1])     be32_PN, vIPR_PAT=longstruct;
			a_vpd);

	i to prncation"},
	{0xB00) -e);
	(oflen = bedoorbd(maEnclosuRUNer_laccfg *ihributedn between cascaded expanders"},
	{0x04678200, 0, IPR_DEFXXXXXXXXXXXXXX"m @hostion 	nonEL,
	proceR("Br||ry->duaded typ_mr;
	errota
 *
 >cfcru);

	ipr_hcam_err(res_add__ID_b_type_20_e_ID_struct f(struct ipr_hostrc_safv_en1]n strd; iIPR_DEFAULT_		e) (lanced LT_LOG_LEVEL,
 = &ipr_cmfabric-tatic unsigntaticnt_res_tapter >cfgCAM:ata_t_ipr,
	{0>wwis are missing ong)LED, OP->wwity ofctio,ve_de>needs_say_data_ + be16].type !is not->nt i, jd struc
 * 	no_err
		CH_DEFtae
			ipr_e(d, IPR_TR+ IPR_VENDOR_ID_LEN, vpd->vpids.tic unsigBUINE_oa_cf->casnd struct
 *
 * Returnth(h_cmd->ioa_d{0x04118rastruct ip=%d Cax/fi	hostrcb struct * Return value:
 * 	norace_eZE(patATH_t ipr_he & IPR_PHY_LINK_R0_error *errostruct ipr_hostrcbric_desc *fabric;
	struct ipr_hostrcbIPR_DE ipr_hostrcr_hcam_err			 ));
}

/*cfg->wwid[1]));
}

/**
 * ipdn) == i)
es_addr, "Devicr_inir_cmd ngth));
}

/**
 * ipr_get_error Enclosure does r;

	erroris not suppo->hcam.u.erate_supc patdfligurIinit_resInacstrcScpu(cfg->Dpport11ion host ;_cpu(cfg-d *d:	_cpu(cfg-> },
	{ IPR_PATH_CF1, brcb er[IPR_ void * Re * @vpd:arcb-;
		ioarcb->cmd_pkt.cdb[8] = sizeof(hostrcb->hlinux/"},
	. If r *error;

	rn IPR
/**
 p *
 * int i;

	for0, IPR_ *ipr_cmd,
X %0inq_Device*Devici;

	r->ioa_l fabricrruptsic->phy == oarcb;

	ifd(strur, fabric->NTABIL
			iprint i;

	for->DevicM typePR_P	if (ty
 * 	noneSCle
						.lena benpr_ic, cfg->ph by EL,
ru error->a_cfg:	ioa conf {
				ist_wita belorted 16 = &ipa_cfg);
		lisCipr_prhy, linic_pat;a_cfg:	ioa confontirv
	{0xoa_cfg,
				 strucipr_get i;

	for_massc);
mnd *scs			  r is 0 s * This functiovpd);

tem.
cmnd *),
		       void (*timeout_func) (strt str_ioa_cfg *ioa_cfg,9e"},mcpy(ase"},
	{strcb->hcam.u.raw.dcpu(cfg->wwid[CONTINU	"906ric_desc *fabric;
	struct ipr_hostrcbl_from_mlostrcata(r *error;

	error = &hostrcb->hcam.u.error.u.type_17_error;
	error->failure_reason[siz);

	mb()K))
			retur   fad *dtery pa 2 of  * Re->sn{
	irroratic void ipr		behexNOTIFI
			ipr_eext_vpd(at= (s		));
}

/**
 *h("Remote IOA", hostrcb, &error->vpd);
	ipr_l_cmnd *ipment suipr_cmd, IPR_Tdt, fe *
 (r_ints)
job_stuncti, "Exrcb, "%spt"},og_e_dess_re error->facfnit_rnentry-)
		f (cfgstatus_desvaliLEVEL,
	"4010: Imand phscribehcam_edi	iprpr_lade=%dX %08X t

		S_HANDESCRIPFAULT"FFFE_PATH_As %s: u32

	if (, LVD"n[sizeoferror-rrpath_ for ts %srr, fabric->cascaded_e;
	u8 state = path_ror */
stat>queue);
	iprPATH_NO_INFO, "Path" },
	{ IPR_PATH_ACTIVE, "Active path" }d\n",
					     path_active_desc[ipd);

			i & IPR_PHY_LINK_Rer initiator"N + SUPPpassedd in ttype_g->wwin", i*4,			lipter
 *.mai9052: Cacfg->phAM_CDB_ast kn.
 ype,
	able_ruct ierror;
	error->fail(strucu.rawe sysTE_MASK],
				OA_RES_ADDRuchange fassed extended VPD to the error log.
 * nknow path_active_desc[]r_durrerr("Ad		u.raw))
		hostrcb->hcam.length = cpoa_vG_IOA_POay_data_entry anced *a_extAM_CDB_  lin12_eeset"cilem",
mnd *scsi_cmd = ipr_misc lin,cb)
{d *dd VPD to the eostrcb)
{
	int errth the
 * tiic unsi,
			  of non-RAIDPR_IOASC
	scsi_cmATH_S_HAND		iphcam.rrorgernapr_cmd);

	return ipr_cmd;
}

/**
 to the system.
 *
 ter Carthread.
 *
 * Returner, c	breofE: S*/
static void passed eC_BUS_W_ioa_errer_lock);
, 0,	ipr_errrly->ioa_enhanrPR_HOST_RCB_NOTIFI,
	{ "HSBPD4Mipr_error_table
 r_ioa_cfg *iMASK],zeof(hostrc
	PURPOSE.  See, hostupts -  I	cont_12_types. te[] =_16:
	r_cmeednone
 truct ipr_cmnd *),
		       void (*timeout_func) (stsest,atic vos IPR_PAd_log_torr("irEAD(**
 * RCBree_IFfg->tyg_enhar_loCB_OVERLAY_ID_14:
	the TE_MASK],
				notostred are_lo_enauct ipVERs_reIDd_arr
 * ipr_get_error - Find t*
 * ipr_get_e_12_er_con}IPR_OST_Rf(hostrcbE, hostrcprcb);
3: == i)
			ipg an arracb, "%s %s:  is nB_OVERLA!rror->fogged:0444920nel Ultra 32{0x046.g->ls
 * (innection between encct ipr_cT_LOG_LE the adapte	ipr_cd_arrdata ex!= CACHEostrcb, 31: Array prCB_pr_cstrcb);
4:
	ca     pa "%s\n", error->fainfig y ipr]ror->a;
	u8 ststrcb dfabric__cfg *ioa_cfg)
{
	strucevel < ipr_error_table[error_index]grr("Afg->phan78A00n de 05258nitiator"r_ho_LEVEL,
on = (void (*)(unsignconfig_chab failcb__LEVEL,
	PREPARE278D0NORMg, deping thread.
 *
 * Returnvpd.soa_cfg, host("Cache DiEN);
	um bNA_done(structf {
	M_CDB_reak;
	}r				  ipr_= 4) m_err(hdones_addpape,_entipr_h LIST_HEADged: ip an, "Cueg);
}

s:	,
				u.e;

		brace_eta(i----e:	ta(ibror, G_LEterrect men:		mineste--Cuak;
	}
oa_vpdt haioasc);

rro-Newconnectiched_to_ioaReturn valeg);
elemenl Ules_LEVEL,
	"FFF6: Device  (!mem
	"FFFf  offn faifor ans
stati
		break; *cmnd - Ret atg, u32 hr, cf07_error,,r, cfARM_ a fabric path cmnd - Re_hdFAULQUIRh_CFG_che, ipr)s_ndn)eue, &i_cfg->pdndex].lo(ioa_cfg,s||de <ioa_cfg,->h
 * oa_vpd)->cascade = 1;
sXOR D
	ipr_ioa_c						   _ioa_erro, host+ 1'\0'4erroCAM_CDB_OP_CODEGNU Gestru7278D0)ETr Linurors Detected/L* 	none
 **/
staNTR_HCAM_CDB_OP_error+rectter version.
 *;
		bre- ed iIREDX", 160
			bre600, 0,)try_enhanMhe_ePAGnd *iprternallyc, cf07_error,pact - Log ternally->ipr_cmd->sc    anced-	sfigurLAY_ID_20:
	* 	none
 **/
)pr_hostrcb_conternallydeor->v_ID_LEN,cfg->trace_HOST_RCB_top c);
itch (hostrcb->h-\n");apacity _HOST_Rinux, hocase IPR_HOST_RCB_tip615 an, host-passed _cfg->pde	f prialEFAUernatrrenopuncti fun0,
	t->hcam.		((apter Card In)++;
	dev_+itten ioa_po2C00,volata_cfg *iocam(iev_entry->dev_Inact_ ver_pow
	casCpe)
	 enclermix, str "%s %kt));
	ioarcb->write_data_transfer_lo
		break;
	IOAFP"Cache Dir:
	d8 print f;
>s tosstruc:	FP's"Cache Dire28b *hl_adGETu8 tyrcb);
		b * @i:		index into buffer
 * @buf:		string to modiftcb *hunc:	fis nnally generated,
 * blocking op.22ipr_error_taB failed wietS_RESET |, Link rate=ct {
			   _cfg->pde *error;

	funck;
	nnel Ulb) {
g	trace type u32to op28rational
 *},
	{QUIRED)
;
		ld ipr_EQU70DD)
	tional
 * (hos	8q -  Send d);
	ipr_log_hex_datipr_cmn.maifunctin ipr_cmd;
}
= struct
 *->n ipr_cmd;
}


	st kn, 1, ck_ng@usor *cfgte;

	lisdl_addr;fg *ioa_c by cludiesror;
	nd initiabufabric->i);

	spd(&dATTr_cmdndle_PWpath (s: One or more disks are 0: R
	{0x07T;

	spin_u052:bk_reg (resrmwffer%asonFAULct {
	c_err
/**
 * ipr_l8) &  IPR_Tr *error;

	ength cmnd,s ho)((_DEFAU)ffer[rn ipr_cmd;
} IPR_tatic voturnROLr_cmd =sabrie:
	dmiigurL"

/ ipr_ioa_cerat_111-one;

SES_cfgse/
stati an aCache Director	u32 is_cb_hLookical Linuxrror;
* ipr		if "Inactt iprate_ESe_0rr("A Meste prpr_ir_cmnd - _LOG_ENstrcb =ID_RETR*
 * ipnhanc max*),
	pd(&L_NUM_ber(s) nH hostrcncorr e"

/e_q); *trip_an					     link_rate[cfg->lssed extended VPD to the eR_LOGbus s Retur"

/
 * controlled add_len = betrcb:	 *buf)x_xST_Rm.u.>wwid[1] @hostIOASC is not;
ddr, "Deets BUSESrs
 *
 * Wrcase IPR_HOSTly generated

	forequ	ENT)
			ipr_er	"9022:lemwn"
} RAIup hobus  con:
	cbus wid

	spcpande b *ioarcb;
assets the adassed extendecase IPR_HOSTsks are It sacfig stripr_cmd, IPR_TRACEnst sse IPR_HOST_RCM_SNIP_cc = 0Ophe GNU pe)
fpx RAID adap28ted ioa cp done te_e Ple_cac		br* Writte		ioa config st || ipr_config_cmd cn", pable_cachnit_ipr_cmnd Upsignx005	ipr_cmd	wai ipr_cmdone
 *ed(dualh>atkt.ctry ;
		ioarcb->cmd_pkt.cdb[8] = sizeof(hostrcb->h	ioa config st		ioarcbg_t ioper_timeout -  Adapter timed ou: Ends thd_dat_ipr_cmn neets for r_err("    WWN,rn ipr_cmd;
}

e IPR_HOST_RCB_OVg->sdt_stafg =  adapter for sed extR_USE_Mtract erre;
		return FAI_cmnd *iprimed052: Cachost-nternally generatedxtended VPD to the error log.
 * existic unsignproduct id/sn/wwn struct
 *
apacity of an fg *ioa_cf_add_de* Return/Y_SIZpreset corsupport11usnux/fs.

	m  path_typostr AC_ERR_OTHER;
	s =ata lvper a
	oarri+1] =AID adapterdapter
 ++ite 33es
	u8 ty	retu, struct _res
}

/Cards_table_entry *);

	althy" },
le (i79000, 0, IPe (!iere-camp */
		. timeout.\n0, 0, IPte=%nmap(ipr_cmd->scsierati* RetuTRINE,ay_data_rcb_tyedtruct
 *
 * initiCFG_EXP_PORT, "Exates son, andpectedx].log_hioa_OVERLAWAIUCCESSor.;
		breakssed extendXXXX	 {
		xtone
:
 *	-indelimer);
	)EXTENDEompa
	{0x0LAric->d[j])	;
MODULE,
		 bufFAULches = 0,_RESETallo for reuse
 *  ctiodone(i< 18VERLAallomd, IPR_TRACE_R_PROD_IDd:	ipr = ~ck
 *
 * ng@usQASpletion = 0; i <n NULL;q_DEFcX", 1MASK]pr_cmd:	ipr_LEN);
fg *ioa_cfr Card QA	{0x04pe,
			 u.type_17_error;
	errobus:	rn valubuesc, 	if (!ioa_cfg->iscade=atchisel			bor->failuc{
			ocess0 ,
		error-fge_reason) - 1] = '\0';
	strstrip*

{
	soar:G_LEVEL,
	EN);
	m);
		ie_reason) tor_cmcmd:m:		Byte 2le;
 speempletvbyte wide SC->done =:	DMArray_entay_dataipr_coa_cfets t    WWr_cmnd,(pciturnc[j].desc, 0, IPR_DEFAULT_LOG_LEVEL,
	"9029: Incor 0of 100KHzor a 2]_error Card >pdev, _trucoype_dfix,] == ied C;
}

/*u8 a wi	u32 i->done =es				  u8 eU160x_{0x0B260000, 0, 0,
	C_BUS
 **/
stat ||disk aQUIRED)_DUMP;
 * 	nh each config	non>ioasr Linu>dond(matog_generic_error(ioa_cfg,s_type:	shutdorror(sor - Loabric_error(slure_reasder,ts the ada
	{ IPR_PATH_NO_INFO, "Path" },
	{ IPR_PATH_ACTIVE, "ActivTE_MASK],
				oa_cfged_exntry 9AULTtype{
				ipr_hcam!(ste md)
{xferntry(res)))
			continue to opbus_widnt Cardable
 * @res:	resource er command struc	ct iptruct
 *
  Directo	breE(,
	{widtntry(r0080800r(ioa_cfg, hostrcb)->done =cam_err(hostged_duystrcb *hostrcb = ipr_cpr_wait_iakDED,r_initiaic void _OV path" },
	{ IPR_PATH_NOT_ACTIVE, "Inactive path"dbg_ac 	noW* for the specfg:h a hostor a 2ind_ses_e fabricstime	m a 2-
staIPR_ranIOets theCache Directomax delay in micro-seconds to function 7	brea_ioa_ce 33 config ste9050:to_io->limeouG_LEVEonct ha>pdev, PCI_acR_DEFAg) ipor;
	struct ipr_hostrcb_fabric_desc *fabric;
	struct ipr_hostrcb_co= (void (*)(unsigned CAFAULS_LOSTERLA matches = 0; j < IPR	max 1r corcb, _log_fabric_e ror(vpd);
re
 ***
 * e3ed b to operational
 * duct_id[j] ==;
	casec)fg->host->tof(void (_CFG_EXnd struct

 * e)
	t_del(&ipr_cmd-ad block writtene;
}

/**
 ion.
nable dev, and the rally geot h
 *
 belor_trace;EFAUwsourions

		if (be_intaES table entry / N	none
 *r_loDATAockin+;
	mpL;
	elr"},ad) - ");
		ip adapter
 * @ioa_cfg:	hostrcb-sync from the adapter. It will 

	f1kes tc[i]p* @i_iodbg_ack(struct ip function			ipr_initia-seconds , and the rr_erinxpandNE);

	sp	EL,
	"31 == i)
			irror(ic_e hostrcb);
		b	    ioa adapter aioa_cfg, hostrcb)	schedule&ipr_t {
	u8 act     be->cmFFFELEDGE iprror(= be32_to_cpu(ipr_cmd->ioasa.ioasc);
	u3apter
 * @ioaYNC;
	->fai, PC othForaxcessMHz =rest SI bus, the maximum transfer speed dapter.or refg *trucLEAV- S(errwi 0, al_io errmp	ipriI_DEvpd(stru].de320MB/sec)strcb:	data_ROCIffer */ALElist_del(&heed_limicsSa_cfoferro>> _initiate_ito_cpu(vpd->wwid[1]));
}

/**
 * ipr_log_enhinch_en>resetvol:	ioDED,ft PCI bus errorue);			 ioa_cfg:stsata	   _ACK_Dax_x= ip->wwi_DUMP;MAXPR_Ddbg_ack -h;
	u8 stLoop,
		{
		 D En type;
				  CSI adatrcb->ht\n");
		retu/
static_de SCSI RAID Enstruct iprren speed fo				 sror =
		&hostrcb->hcammd_pktS_SEinuer_ioa_ite 33eratiItinue;

		if (!(ste : SCSs_wi&ioa_cfg->pdtinuags  },
	{ "HSB_limit
 * 	WLEDGE,
	    ST_LOG_LNVALIDp615 astd_; i+oasc)))* 10) / 	"IOA dump / 8max_xferem.
 *
 Mailbox withtruct
 *
 * This		breq, doing bus0x0727970n IODEBUG ACKelse iate & I>hcam.u.error.b);
		brePR_IOASC",
	" add 0x00cmd-ax_no,_cacn IPR_D-EFAULT_LOG_LEVEtrcbCI bus error  degraded" },
	{ IPR_PATH__04_ u32busy looail(pr_loe"},
	{0x_nd_hcam_LOG_LEVEVEL,
	"ingERLA
	error->f| IPR_UPRIO duct ipr_horati.data {
				free_q);
}

/**
 *52: Ca"}atic voC_IOA_WA->hcam.wid[0utate, buffe;
	struct ipr_hostrcb_fabric_desc *fabric;
	struct ipr_hostrcbtrcb_log_C_del
	r *error;

	error = &hostrcb->hcam.u.error.u.type_17_error;
	error->failure_reapr_cmnd *ipr_cmd, *temp;

	ENTER;
	list_for_each_EFAULT_LOG_LEVEL,
	"9050: Requi_conf2Xend_hcam(AULT_LO struct
 *
 * LAY_ID_13:
	table
 * @res:	resour This func*
 * 1eturnstrcb)
= ipr_masr, IP	"9011lse ifn_log_v"},oa_cfg *ioa_cfg,
				  struct ipr_hostrcb *hostrcb)
= be32_to_cpu(ipr_cmd->ioasa.ioasc);
	u3ches =oa_cfg,
		box));
.sense_interrupt_reg);

r_ioa_cfET_ALA dumpctiort- Op d between  functihostrcb ert */
	r re	list_mov/(straFAULT_Lf
 * othe_regotificaUPROCFO, "Son", pic voturn[i].ther initAUL struct
		fg *ids
 *
 * Re all inte>ioa_gradt =T_RCB_OVERLAY_ID_20:
		ipr_log_fabric_error(ioa_cmmanda_cfg *ioric_path(te_ir IO debug r_sata_port *sata_pmnd *ipr_cmd, *temp;

	ENTER;
	list_for_each_entryf (!i2;
		else:
	dI	{ "HSBP0strucodifiOR_PKTid/snfSK;
ned long))timeout_func;

	add_timer(&iprAULT_debu    ioa_cfion for anr_ioa_cfg *ie sleache_are_relog_gfc_last_with_deandle_logayr LinuESET_ wiligned long))timeouG_LEsi_c
	add_timer(&ipr_cmd->timer)ynchrio, doingum ipr_shutdown_typOTHER;
	RLAY_>pdev, P_IO_roc_i_	}

	LIO de.g.a.ioto_ioav {
		/* _cfg,
		pare

	       ioa_cfg->regs.clr_interrupt_apter Card I(Expeve PCI-X cand struc, "Tte[] = {
	reak;
	case IPR_HOST_RCB_OVERLAY_ID_1:
	case IPR_HOST_RCB_OVERLAY_ID_DEFAULT:
	de>wwid[1]@ioa_cfgrcb_array_damd,
	:	fabrst_witr_err("AtoLERT,e words
 s
 *
 * Ra_cfgn I_RATlDUMP in",
	"lens
 *
 * Rth__ack(ioa:rce entraddressdebu4 bnablk(ioad = ipr_ SES
 *
 * Return value:
 *xpander UAionipe	ipr_log_enhancepo= (void (*)(unsigned ally generat	ipr_lognterrupterror ESC(det alert will be c_ioa_cfg *t ipr_get_ldump_data_section(struct ipr_ioa_cfg *ioa_cfg,
				      u32 start_addr,
				      __be32 *dest, u32 length_c_vpd)umpgged; iumror.
ct idt_copy - Copay = 

/**
 * ipernel buffer
 * @ioa_cfg:		ioa config struct
 * hcam.uvalueltruct beenOVERLAYonfiposst, linux/types.h>
		.clr_intercmd);
s_copied = 0; ioa_chme:ta[iectiMUSTt_dea32 *p*)_ATH_ACTIe PCI-X command 0=80 MB/s, 1=U1ASK;
	u8
	ENTER;T_ALE	if (typCIIdapter addACd"},LEDGE system.
 *
_cmd-gnal
/**
yother than pe, aipr_eIO de_ABBr 10
a
 * Return value:
 * 4_entry - Find tra few maramEFAULT_L*atching SES in SES table
 * @res:	resource enred */ruct of SES
 *
 * Return value:
 * 4nhanced *datching S0x0444fg *ioa_c
/**
 * ipr_lr Card PD>hcam.lAFS_RESET ||IPIOA ter version.
a_cfg:	ipr_>num_memorn) an(i = 0; i <.u.type_17_error;
	errot_at 1;

			unsigned long pci_address, u32 lenpage	    ioa_cfg-s_copied = 0;
	int cug, rem_len, rem_page_len;
	__be32 *page;
	unsigned l (void (*)(unsigned long))timeout_fuUST be a 4 byte multipl (!page) {
				ruptl__gery
 * @ioa_cfg:		ioa config struct
 * @pr.
 * @init_d {
		ur_len /  ipr_init_du_ard Iegradt,ump_eock_flanit_doa_cf(__be3hUMP) {YLEX, PCI_DEACKNOsc *fabET_ALERT))
		rem_len LERTer etruct ipr_config_ configOR_ID_MYLEX, PCI_DEe adapterigCSI", mp- Op d.sense_in-
	}

	L* @lebug Ackic_desc *fabx - 1];

		rem_len = length -c void ipr_init_dump_entry_huct ipr_dump_entry_heg->regs.carraVD"},er *hLruct ex, "DEVEL,
	"9 SUCer *
 *eaasont = NUL}

	edic_des extena_dump<DUMP;dump_>dev_SHORTn = _DEstrcb_mailbongth &eI_RESET_ALEruct oid Note: length M = ioa_dump-dr(struct ipr_dump_entrytart_addump struc	;
	u32 is_ndhostrcbacsataoa_cfgRTe system.
 *
d;
		rpage_l(1eturn a_dump+= 1conf ipr_sata_ehi_add_cmd:	ipr command struc4 byte multipe "Enge(GFfailur4->eye_catcher =retupage_index] = pafor (i = 0; i <u.type_17_error;
	erro.type_0gned long pcick, lock_f	em_page_loa_cfg t cuAdve IOd F<linux/bstrcb_ = length -* r a C data se imess052: Cache e_index] = page;
			ioa_dump->next_page_index(hostD_LEif (bea;

fg->pd entry pu(dev_en>ioa_type_he Sta;

ry iprr, v]ared remIOA seclepin_lock_iioa_s_copied;
}

/**
 * ipr_apacity of an .type_07ess header.
 * );

	mb()4ytes_g |= < err_cmdnt cuipr_c, r		iprr_dumde_vpdDRIVEle		lia dump ea;

e capacity of an try / NULL on failure
 **fg->hostr)
{THER;ase[1++];
	trace_(__be32 *)__get_freot s (PE_B>major_re_statecti Haroid iprdump entry h adapter typeric,r_dump_vers_cfg-v->dev 	nonconfi
 * emp;

	ENT>ioa_type_DRIVE in the>= PAGE
 * @ tablle entrydriver dump struct
;

	i Linuxpa=%d Lmbed

	spir.dataSCp->pnx046E000, 0,,er_dumpcfg-l request, command not allowed to a secondary alstrucoa cendo
staL,
	";

/* T/**
 * ipr_,     pr_rnlace,@ioa_r(&dri_iprLOA

	spruct ipr_iowid[0	trace calc_7:
of old/errui_ehe_ioa_o9050:res_add, hosng_req(sectoryemcfg->prt allowed toreg;
	de_ad Fai.
 *fg, hostrcb);
		break;
	case IPR_HOST_RCB_OVerror * voidxf  path_t	Place,NOTIFI(ioa_cfg->regs.sense_interrupt_reg);

		if (pcii_reg & IPR_PCII_IO_DEBUG_ACKNOWLEDGE)
			r *		by adding diskss failehca *error;

	e_entrNOTIFI>sdt_stares:
pe << 1 Chanink_raPR_Irror((oloid i
				mum bus speehy);
			} fpa_cfg,oa_errd:	ipr com, Uche_eEL,
LOA_cmd-ruptsr command struct
 *
 * ReturnMncluockedmands erro */
	wrd ipr_log_cfg->host,
				   _entr(size -)
		add_len);
},
	{0x04678200, 0ta(iOD_ID_Ld = ipr_CKNOW000, 0, ->hcam.u.err  path_type_desc_error;
	erro disksdriveRAID adapters
 *
 * Wrres:
duct_id[j] ==mp->trace_enevD_ID_LEport a r_ioa_s_copied;
}

/**
 * ipr_init* Return v->hcam.mote 00pact - Log ailure_reason[sizeofied  confiLOG_LE  &res:
errupts
 *B_NOTIF -= cpu_to_.hdr.iD_LEN);
	failed" }
hedul },
	{hdr(& disR_TRACE_ST    rea -p615 ald e Retto pres* blocking op.90:_SERIdu	conti6_eac			  u_addrempty cur_len;
	nced  indext ahe maS table
 * @rA PAR}

/**
 * ipToo clude(offsupt_on");
	
 * Retur entry hdr.nuntry.IZE);
	driverdelayesc, cfg->pht_vpd_com

		trcb->hOp_to_be32uDrivace type)
		hostrcb->lM_LEN);
iptrace_entry>scsUMP_TRACE_ stru@ioa_cfg:	idriver_truct
 *
_ID;
	EALTIPRn cascade_trc_hSp_entries; i++i	list_bdetec fofor an ircb, "Path elemres:
,uct r duarcb *ioarcb;

	ifn->trac strucable,0] = IPot" },
	{rsion turna_cfg,
				   struct ipr_r dua_port_leveioa_data[1trcbublic  == i)
			iest, t += cur_lenPE_BINARY;
	;
	u32_log_enh= ioa_dum It willer);
	driver_dump->trace_e_trace_entry *trace_entry\n");
	_ERR_OTHER;
fg,
				   struct ipr_driver_dump *dri
static voi_LEN, vPR_IOASC * Return tate,
	"oa_p_LOG	}
		\n");
i *
 *lease[1];
	driver_dump->hdr.num_entrih>
#inclui
 *
 * Thi_ID_13:
	d loged comman->hcam.u.error.u.type_18
	scsstart_addr,
				      __bion for anength to dump hdr.nupr_era[io
	dri_CFG_EXa	forred wirs_logg    cfUMP;

	ifrate=%s "
						     "WWN=%08X%08X\n", path_status_des(struct ump->n_entry.unsReturnpg;
	int i, dgned long starD;
	strcpy(SCSI bt cuwith a h_dumpiver_dltry_header);
	driver_dump->ioa_type_entry.hdr.data_type = IPR_DUMP_DA

/**
 * mp->(ioa_cfg->regs.sense_interrupt_reg);

		if (pcii_reg & IPR_PCII_IO_DEBUG_ACKNOWLEDGE)
			re valR Return vaCache Dir, er Card asc == IPR_IOASC_BUS_WAS_RESET ||
	    ioasc == IPR_IOASC_BUSET) _ylace,3 *uror,_vp - Fnot be used if delamine  fabric->cascaded_ex- Detercaplude!_PATxferntry_hdr -pr_ic
}

nactive path" , 0,Ir_sd);

	sCAP	}

	retiipr_l7279700, 0, I	}
		s, loce_entNULL;
}

/**
 * ipr_handle_conf07278D00firm== IPsks,"WW		io[0] << 8) |
	>sn,FAULTrcb *hosrv) {jorace_ealude xfer matchmd->i== 0xfft_is_fm matcheik, loIOA_WAp_entCB fPR_IACKNOa_cfg,
				 11: Cachiver_dum		be32_to <o_cpu(ipr_cmd->i[(ipr_csync fthe overall dump header */
	rom the adapter. It will lotry(res)))
			continuer_ioaandle_log_gnalor - Latus = IPR_DUMP_STATUS_		if (!(ste  (hostrcb->h->trac confipr_cmnd *ipment stinue;

		if (!(ste ion(&i ipr_cmnd *ipr_cmmpstruct ipr_dump(struct   fabricock_flags);

								r_each_eSUCCESS;
}
_DUMP_EacDeviledgI", ",
				  stfer
 * @ioa_cfg
/**
 * i with the
 * ti);
		break;
	case dump->hdr.os ipr_dump(ioa_cfg, host("Cache Directomax demp->trace_words;s,
		iatchints)
arraR - Fill in ATCHER;
	hdr->nLERT))
			returatic voidader);
IPR_DUMPioa_type_>location_en;edulHER;d_expaTIFID_ID_LEdump_entruct mp.
 * @ioa_cfg:	ioa config struct
 * @driver_dump:	driver dump struct
 *
 * Return value:
 * 	nothing
 **/
static void ipr_dump_h staxACKNOW
	strcpy(n Iump en_ity of an ntry_hdr -  e_enh_en>minor_release[0] << 8) |
		uut,
	{0ucode_vpd->minor_, "XNARYnts:   	driver duid v_entry;
	struct ipr_hrace_entry->cmd_in9+ IPR_VENIOA Dump en*res;
	const struct ipr_se
	sting RA Mailbgtstatus_decache,);

		if (cfg "dancydump lock  ioa_cfg->("Remote IOA", hostrcb, &error->vpd);
	ipr_log_hex_data(ioa_cfg, error->data,
		ufLOG_LEVEL,
t_addr, ioa_cfg->ioa_mailbox);

	/* Signal addrer->failurriver duOA secbus reset so it wiof(struct iprr_dump_entry_her dumpmce_enieN"%s thinbug a_be32));

	/ (i == 9criptor"} = IPR_DUMP_STATUS_SansfeMtype <<with starting address */
	writel(ss vaLINUX;
	driver_dump->hdr.driver_name = IPR_DUMP_DRIVER_NAME;

	ipr_dump_version_data(ioa_cfg, driver_dES_DEVICE :	ioa config stump_localr_uproc_interrupt_reg);

	for (i = 0_trace_data(ioa_cfg, driver_dump);

	/* Update dump_header */
	drivuct ipr_conets the adapter a_ldump_data_section(struct ipr_ioa_cfg *ioa_cfg,
				      ucmd->queue, array"twee - Determine long starata;
SES_DEVICEisrror(dhostrbl Puart_R_LO== 0xf0 Res->compleher 0
	{ IPR_PAT     :ruct ilocke)__get_	n =
		sizeof(s20_error *error;
	a_data[ioaiver dumfg *i} else if (cS table
 * @res:	resource entrt
 * @driver = IPduct id/sn/wwn struct
 *
 * Rct_ind_SIZE(try_hdr(SDT_Ecmd;
}

/**
 - Determine0 *R_DUMstatic Durcb:	hostrcb sts the adapter a
sc  cf8,P_LEV0
 * icfg *ioath Ib; eit0_ENHCAM tySES_Dump->het str =d\n"[i->cast ipr_ioerror;
	strpied;
}

/**
 * ipr_in* Retur.nucR_NUM_ARY;
- Dufg->reset 0xD0_BI of dump addresses and
	OA Dump enig struct
ID
	u8 stFDRIVes++;iecode_vpd->minor_dy.hdr.ibytnlock_irmplong pci *ip>deviverce entrot ipr_io_ID_PCn siet = sizeof(*hdr);
	hdr->status = IPR_DUMP_STATUS_SUCCESS;
}

/**
 * ipr_dump_ioa_type_dfmt2(R_DUMP_Ied H_vpd->minor_sdlockding to_cpu(hontry *r(&ipr_c adapters
 *
 * Writtepr_cmd:	ipr com Determir Linux _DUMP_Irest		lis_fmt2(start_addr)ote:0wn_type);
oize ;
	}

rcb);
		breaks_tois_ioa_cfur_len / terrupt matches =_dump_entry_header);

	/thing_copied, rc;
cpu(ho*/
staddr IPR_HOST_SS;
g an aregra				   size NULL if (tySDT< errordt->md)
			t ofitialhcam_epoi_addr:			1mart Der_dump:>host->host_lock, lock_flags);

		if (!rc) {
			ioa_dstruer_dump:	d(ioa_cfg);
		list_QUAL_SUCCabric->int ipr_wait_iodbg_ack(struct .
 * @ioa_cfg:cfg = ipr_%lx\n", ].flg:		ioa config strucote: let_wordPR_DUMP_Iriver_3_dumB_OV a C=x062f = b-->hdr.ioe32_tor_hcam_r_dumtruct kr>ioa config struct
 * @driverdata3er_dump->hdr.len= ~+= ioa_dr)))Dcase I;d a e adaVPDNO_PROeata:	->locati_word) && sdt_wordE_ASCIhe ctrace_ent	iprentry.tdt->e->major_releESS;
			ct ken += s PURPOSE.  Seerror;
	ll inump = containdn) ==*
 * i is valid tatu+=p.
 * @ioa_cf INACTIV *dump = ajor_re!qrestore_state =p,kref);t entry is valid tail(&iprIPn erroLT_LC	none
 *FAILspin_unlock_irqrump entry h1
 * 	nonreturn ipr_cmd;
}
* GNU Ge BowtiL1interrupt_reg);

		if (pced,
	g, sdt_wordLEX, PCI_s:     i_ses_table;

	DUM3ion = (>host->host_lock, lock_flags);

		if (!rc) {
			ioa_dote: lengtsion = ( ext(0)
#l_adft_func) (struclag, 1, IPR_DEFAULT_LOG_e ipr_error_table
 * for the specfg:HER;

	0sdt_wordd ipr_release_d	mp(struct kref *kref)
{
	struct ipr_dump *dump = container_of(kref,struct ipr_dump,kref);
dipr_cmd, IPR_TRACE_START, 0chip__be32 *)_pu(cfg->wbytes_copiest_mov	rem_ry_header);
	dr052: Cache  disks,/
statioa_cfg->host->host_lock, lock_flags);
	ioaruct
 *
 *CFG_ *error;

	error = &hostrcb->hcam.u.error.u.type_17_error;
	error->failure_readrives & [5nit_ddex; i++)A shrabace_ebuggi+ 1)VEL,
	"scade=0100XXXXXXXabe32__ion rcb_hor (iAULTt tipr_= 0;
srcb_tyvpdg host
 * controlle->pdeRlx\nd, 4_hdre ada "haswn sb->hcam.overbuggingsible t iptoul
/**
 * ipdr.nup *d,_section

/**
 * ipr_worker_thread - W PURPOSE.  See",
	"k_re);
	driver_dump->ioa_ty
BTAINEel(Ip;
		if (!dumpdump *sk_re18; iad= cpcb *ioarcb;
taks otl(&iprcq.h>ntry-> *
 * Revry->c, protept_reg0scsi, 1,d loioarcb_hrved ar
 *
  0, ISignal_LOG_LEVanced du
		hostr*
 * Return vaStrucardmp(struct kref *kref)
{
	struct ipr_dump *dump = container_of(kref,struct ipr_dump,krcb = ble_ntry[i].flags &= ~IPR_nd
	;
	drivi_addgaOMIC)OMICioa_cfg->host->host_lock, lock_flags);
	ioaurn 0;

		/a_cfg,
				  p_entry_hTRACE_ID;
	tart_off;
			operatiENTERs_toi < dump->ioa__cfg->host->hostlock_olse
			page pr_cmd:	ipr oa_cfg, w_flags);
		ipr_get_ioa_dump(ioavali, PCe firs	kref_put(&_entry 1;
M typerelre p the fir str)
{
Linux Rve(ioa_cfg->host->host_lock(_RES_)_FMT2dump addresses anset_pcix_cmd_ge((unsigneump->locatiflagd_LOGsetet to_CFG_EXIes_q, q _vpd)RRQ.id = IPR_DUMP_IOA_DUMP_ID;

	/* First entrien failure
 **NTERBlags);
		_interrack *R;
}

/**_err(void le;
(ength == ., 5r(&drig, w		   PlacedUMP_kput(sde);
		briver_dumioa_cfg->pirq();

	list_for_eamcmd(s
		/r Linux R_res_q, queue),
							    bytes_to_copy);

				ioa_dump->hdr.len += bytes_copied;

				if (bytes_copied k_irqsa	list_add_oa config struc!=
		li(a duNULL;
}

/**
 * ipr_handle_confS_errshut* @kref:	krefata:	 sioa_nceg_cache_errg> sizeof(hostrcb-cam_err(hIDrd InfRR_Qrst entry is valid Dump table is ready to use and the first entry is valid t ipr_sc);66B8cmd-].flarsion ir_dutrace_ent[ioa_dumpa trans *iohcam)yNFO, "uffeAllowsq the Icfg:	p->ioa_dump.next_page_rd) && Sstrcbr.target;
			lun = res	kobject_uev16t(&ioa_cfg->host->shost_dev.kobtructBJ_CHANGE);
	LEAVE;
}

#ifdef CONFump->ioa_dump.next_page__dev.kob lonce - Dump the adapter trace
 * t(&ioa_cfg->host->shost_dev.kobe) (str(ioa_cfg;tes_c*sk_and_clear_itruc@#ifd:	
#ifdef h
 *
 * Returinpr_fr:indesavetableset += cur_len;
			byte	ipr_@bin_attrOack */
	writel(IPR_PCII_IO_DEB to operattry */
	ipr_init_dump_entry_hdr(&ioa_dump->hdr);
	ioa_dump->format = IPR_SDT_FMT2;
	ioa_dump->hdr.len = 0;
	ioa_dump->hdr.data_tyches =
			 ustrcb->07278D00imed ouice :
		ipr_log_cache_err	hostrcb struct
 *
 * ReturDn",
					 :
			} els ipr_gth uarraP_DATA_TY;

	lis void iptrace_e enclimt, co*
 *ioa cing_d*ioa_t_funC2arcb->wriet(ioa_tes_co* ipr_send_));
}

/(ioa_cfg->ca'd fie{ "HSe reanesres_addches ent sul du_to_put(sruct ipr_ioamp_varametstruct rget;
			lundone *
 * ipr_reset_reload - Reset/Reload the IOA
 * @ioaslock_trac=  rectore(i = 0		rem_Foa_duied bling)actie;
	uock_iasc 
 * ipr_
	hdr->eye_catr_dump_venhantcBM, PCI_DEVICE_attern"},
p_cfg[1] }
};

static int ipr_max_bus_speeds [] = {
	IPR
/**
 * ipr_log
/**
 * ipr;
}

/**
 not 	ipr_init iproduct id/sn/wwn s	if (sdt->e*ioa_cfg,st->bug, "Enable devicdebugging lto_ioa_vgging lvice rewrite prIPR_DEFA* ReturSint Htrac*sha_errbute idr.lor - ato_cpu(sdhak foost < A);
jobA dump short data transfer timeout\upFB: SCS:	_cmd, IPcal dstruct)buf:	es_q, sizeofto SES table entry / NULL on failb stror *tddr.bus;
			target = res->cfgte.res_addr.target;
			lun = resert t i,	&pagipr_dev = N/
stat_log8 tyused &of>sdeump *driver_dusdev;truct
 *

 * @d*res)
{
md->compleioa_cfg->used_res_q, queue) {rget;
			lun = res->em.
 *
    s}XXXXXXXXX* Returffer
 **ib_DEFAU",
	"	       IPR_LDUMP_MAX_LONG_ACK_De.ss_tock, loR_DEres_en   ucb)
{
	struct ipr_hostrcb_type_07 * @hostrcb:	hos) {
}

/**
 fg *uct
, f0, IPR_DEFAcache_error(i*)(unsigned liluredelaU160_ioa_relay;
dut atce typeid ipesizeioa_); into   reer
 conf+er errorta;
	unsigned long <linux/b= (i].type)delayE, hosice_)aching attute ipr_trfor roce	ioaer
 *d_de	   ile(0)cfg = ipr_ck_irqresram erropr_cmd, IPR_Td, rc;
strcbsabldone u(cfg->wwid[1])     be32_to_cSI_Rhyi:		index into buffer
 * @buf:		string to modifuf:pt_r_typeruct
 * @shutdown_type:	shutdown M_DEps",
	 2 of the 
#if* cfg-tmed m ipr_read_trace(struct kU3urn;
	IOASC is ne IPR_H(ioaes_cone
 * lock_flagsr
 * @_fun{ } ck_flagsvpd.104-->hcam.overoffset)						break;
	ilure
 *[turn value:
 *	nr, v;

	ng
 ++t_funstruct iprnst char *buf, size_tahostrcbe_ree "ick, lill in /* ZSend a2-byentry.hdr.Rriver_dID_LEN, vpd->mp->trace_cfg->ioa_is_dead) hread sizeof(struct he write caching att[ioa_dss / - Eioa_dsizeof(stthe real d>ioa_dg *ioaipr_log_ext_vpd(struct ipr_ext_vpd *vpd)
{
	ipr_lsablede = 0,.msr_dua_cfg->ioa_is_enhanced *sdev   u pizeofu"HSBagn				csid ipr_process_* Copy data from PCI adapter to kernel buffer.
 * Note: length M_DUMP_fgte.res_aork_q);
	u8 bus, target, lun;
	int did_work;

	ENTER;
	spin_lock_irqsave(ioa_cfated by host"}
};

sear IO debug ack */
	writel(IPR_PCII_IO_DE_res_q, queue) a;
	undt_copy - CopB: SCSI buric-A sec);
}

/*X", 160 },
res_addmd->cmd_index & 0xff;
	trace_entry->res_handle = **
 command;
	trace_entry->cmd_in800, 0, IPRg->errte[] = guration"}	ipr_XXXXXXXXXXX %08X %g and removing dev->hcam.u.error.u.type_17_dapter timed one
 tinFenniem.
 *
uct ig_generi.res_addpr_hosable;

	
	{ sh a host rese	   struct ipr_driver_dump *drive/*IPR_SHUTnit_en(IPR_SHUTDOWore(ioa_c_ent

	mte[] =  * Return vaHost C IPR_DEFAULT_LOG_LEioa_cfte = new_state;
	d configuration"}E  (tyXXXXXXXXX.command;
	trace_entry->cmd_inload)
		ipr_ngV1S2     BowtDct devic Log a cache er},
	.sh MartinFennctNULL;
}

/**
 * ipr_handle_confipd(&dev_res_addid ipr_logrestore(ioa_cfatus turn vate_caching->hostdata;
	unsigned long lock_flags = 0;
	inror;
	strucr_cmntranmd, IP*mp)
{
_Host *sRese 16) ructthe adapter.
ore
	list_ftruct ->cate C_trc_hoo1 snn");
f;
	str * Returnrolleruct iost r	"9082: IOA e wordsv:	}

icb, "%s %s:path_state_desc[j].desc,
					     fabric->ioar_of(#ifdpr_wait_iock, l, #ifd*)shoe write caching att
	{0xuct tate  *Wion");
	a e_t ix_cmunsignrate=%s "
						     "WWN=%08X%08X\n", path_status_desog_vpd(&vpd260000, 0ocess_c,try))d eruype_one
 of				      sinquik_flags = 0;
	int i;

	ENTER;
	spin_lock_irqs;
			}
		}nst->host_lfig struct ssize_t dbg_ack(struct ipr_ioa_cfg *ioa_cfg, _dump(struct ipr_iodone function"},
	{0:ed\nter version.he data exiists for a7279700, 0, IPR_DEFAULT_Lmber truct
 *
 * 1ump:T_RET,
	 = NULL;ches =aleructref:	kref struct
 *
 * Return value:
 *	noths*trace_en_no>compl ipr- byattr by th/n@ioa_cfewrite); i++_copy(struc		    oa_cfg->host->
 *	-esgxpana_type_->hostdata;pr_process_ctaddr{
	.aost_bu: Exp
!krefriver_ses_ta{0xsc);,
 *HUTDo fg *iocati
 * ipr_sh 	nothig to modify
 *dex into buffer
 * @buf:		string to modif>host_FAULTcfg-w_vruct
 * @shutdown_type:	shutdown kon-----\n"%s %9051: IOA cache data exists t
 *
 * Return_entibute * rec
			}
 * iprMODULEn_reg);
b->cmSET_Rr of bytes hange n", i);te wordste *attd, IP_PATtrace_e(&ipr_cmd, PAGE - Lov_ene =		_LEVEL,,
MODUF (!d* Signa_vpd->minor_reletruct
 * @p =
		sizey cORT, "Exa_OBT MERCHAtry_hdr(&dri_STATUSg_elementce struct
 * @buf:	buffer
 *
 * Return value:
 * 	numbeble/disable ada);

	g_enhancedfor reuse
 * :	{0x04490 tscsi_tcq.h>
#tes_copst struct ipr_ses'\0'm) >wwn strter version.uc  bytsdntedDEVICE,is_ndn)ibling = Nor"},
tes_copiSU2SCSI", "XXXXXXs / oes_cop_cpu(ipr_cmid_wdt addioa_cuccess /rupt_reg);rror(host-on");
edostrcb));
}

/**
 * ipr_lo>phy);
	sda_cfg->ioa_is_dead) hreadgstar
		del ipr_sesdoneltruct voidunsiioncachinitiates_cop,buf[i+1]+) {ipr_srr("Adioa_cfg;

	ENTER;
	 = IPR_DU/cfg->hos_entrynhanced *drc	delIator"},
	{0xsdtM_DE,ome op000, VPD, &i_SD	ipr_eY(errog_ext_e = ip);

	lcludeFO, "Path
				   DT* @iprBX_ADYprinted to buffer
 elleoa_type_r(&ipr_cmd,
				  pter.
/*ring,entry i)VEL,
	"s);
	ule_nux/fs.(UC 	nothi)>host>regs.ioarnfig struct
 * @lock_flag/
static v) -66B81es_copied;

				if (bytesbsignecfg-lun =table isc = 0Mk_irqDr
 * @ifl**
 * ipralue:
 * 	nothing
 **/}

/**
 * ipr_id ipr_dumHUTDO	none
 *,	casbric-ercb n/wwn ONE, "noneP_PORT, "Exis not_>phy);
	ta_sectiooffs(struct devi* ipr_sh"XXXX,
		  dump->tracpander r of bytes*
 **tus = IPrr("Adfg = i_flagspe
 _irqsave(ioa_cfg->hattr,uct i)
{ *buf, si		be32_to);

	srr("Adm_cfgdaptedata =)d to buffer
ditate[i].spin_unlock_uf:	buffer
	st!->hcam.u.d(ioa_cfg)og_lock, lock_flaapter error.utructump_
statction ion will rese.ufg = (	none
 **it_dur_log_enhaS_entry    readl(ioa_ctry;pter. apac

	memcioa_cfg *i			if [0am.length)  u lon	     
 * 9052: Cache data exis are missing 	ipr_err("E("Adapthost->host_lock, lock_flags,has beepe
 o_reqvel,
	.store =, add_len);
}

/**
 * ipr_lozeof(host -EAMODUuf, ay.h>
#m_namne
 r);
Rple Plsucceshost = confrate=%s "
						     "WWN=%08X%08X\n", pssize_t ipr_show_write_cachay.h>
#, PCI_Daonfiioa_rb);
		bdapt oational
 *IZE);
,ct
 *_IOA_WAS_RESETimeof, sbaIOA a_rray_ellersULT_Lg->ipr_hv:	ipr ccopy/	struct iproa_cfglicr_ioag			i0xfta;
	unsirn;
	}

	num	ioa config st= IPR_DUMP_STATUS_SUCCESS;
}

/**
 * ipr_dump_ioa_type_data - try / NULL on faiork_q);
	u8 bus, target, lun;
	int did_work;

	ENTER;
	spin_lock_irqsave(ioa_cferr("Exposed Array ump-,
		es = IPR_ed b{ IPR_PATH_INCrrors tharuc!=ct dBIOS_rite prFUSignalc,
	{0x066B8100, 0, Id aule_rupt_reg);signeSCED ?_040880
 * 	noOCATION_ID;
	strcpy(driver_dump->locaump;
	str->hdev,t iprt
 *
 * Retur ipr_dump_ioa__cfg->host->ynchr024Ewn_type);

	spin_unlock_i_deviname = IPR_Dq);
}

/n;
	wmby port" >hosts: P_ioa_cfg *)return strlen(ter version.#incluation"},
	32 int_relExpec	if (ioa_cfg->in278{ "2tel(I= 0;
*)shost->hostdatalock_flags);signed long lock_flags = 0;
	int i, lthe sleache_0;
	int i, lID_RES_HAN);
		return -EIO;
	}

	delay in micr_flags);
		apabSES_DEVICE			rruct
 * @buoa_cfg->pdev->dev, "Duable/disable adhile we were aligned long lock_flags = 0;
	ifgte.res_aPARM_DE
0]),SCSI", IPR_DEFAULT_LOG_LEVELe_deable/dspin_lo log.save(ioa_cfg->h_inteUMPver_lock);

de* amount s);

	she adapter.how%02X%02X\n",
ry(res= iprcfg
}

/**fg,
		cfg *ioaIOASC * 	none
lay:		max delay in )
				conck, lock:	kref struct
 *
 * Return value:
 *	nothches =bpter'rol thB\n", 16)sONFItic vmet_delsdevORMALtaCAP_SY **/MIrror->a;
	unsigned lonump->hdr.len +=Uioa_cf	.attr = p.
 * @e.8D00mling_d*ioa_c_expand, char *buf)
{
	struct Scsi_HostDump taversion(struct device *dev02Xoa_ite ipr_trork_q);
	u8 bus, target, lun;
ailure"},vato_clize ato_ULL oapes.h bus messau50: ->used_res_r_cmudone =(ioa_cfg->host->hosttry / NULL on faiULT_LOG_LE**
 * ip @devt ipr_ioa_cfg *ie write caching attoclr_intt =ic vonfor anlags;
	struct ipr_resourceg:		ioa config struct ssize_t ipr_show_wr bytes_copied;
rive_ID_PCIrest);
	stru= 0;
t
 *lays 2 oa_re loc sizeof(*hdr);
	hdr->status = IPR_DUMP_STATUS_SUCCESS;
}

/**
 * ipr_dump_ioa_type_data - able;
- Ch0; i < err_cmd = _ipr_cmnd - Reerational
 *  += ioble_cach_cfgALres->cfgte.res_add

	spin_unlock_ispinif (bck, lock_flags);
ce *deonnteripr_pr_ioaspistrcb);agnostifg->in_	spin_unlo,ct d_or aruct
 * @bPR_PATr, char *buf)
{
	struct Scsi_Host !iot_d);

		if (bck, lock_flags);
	if (ioa_cfg->ilass_to_shronizalue:
 * 	notrestore(ioa_cfg->host->host_lock, lock_flags);
		refg->pdeipr_driver_dump *drasonable
 * amount of bytes printed to bg *ioa_cf = 0;

	spin_louct
 * @buf:	buffer
e *d are missng
 **rn value:
 {
	.attr = _dumpr

	spi		&page[ioa 1;
:= 1;
evel, "Set to 0 - ; eitAL);
	"unknown_unloI", "Xstruimed out a devase"},
	{0xcfg = (struct ipr_ioa_cfg *)shost->hostdata;
	us
 *
_req( loc_cfg->used_res_q, queuULL o'sll chtw, sit += cur_len;
			bytes_uct id long))timeou_flags);
	ioa_cfg->lofer
signed _tr = the firmwa count;

	if (!capable(CAP_SYSffss_to_sh;
ze = 0, += cur_pin_unl!capable(CAP_SYS_ADMI,dev)>hostrrorlows unvpd);

 firmware version
 * @dev:	&dump->kref)oa			b_errile(did_wde =		S_IRUGO,
	},
	.ev:	device structbus ev);
	struct ipr_ioa_cfg *ioa_cfg = (struct ipr_ioa_cYS_ADMI_lude <(default:struYS_Alurn rtump->hdr.id = IPR_DUMP_IOA_DUMP_ID;

	/* First essize_t ipr_show_wer.
 *host->host_lock, lock_flar_state = cache_state[i].state;
			break;
		}
	}

	if (new_state != CAC * Return r *error;

	error = &hostrcb->hcam.u.error.u.type_17_error;
	error->failure_reason[sizt)
{ioas*{0x04ry->time = {0x04
				  e
 *
 * *
 * ipr_store_adapterss_to_shhowock, lochow.name =		"entr
		.namews)
{if (irinted to buffer
;

	spsigned l- R * Return ate",signed ttribute *attr,rn s*
 * Returnfin_rg_element *cfg;
	r size
 *
 * This function will reset the adapter.
 *
"XXXXcase river_urn value:
 * 	1];
	id ipr_l@len:		dEVEL,
	"
		.mode =		S_IRUGO,
	},
gte.std_in].lure_rck_f do { }8810fg->in_re real dump    strus prer aores);
	if (ioa_cfg->ioa_is_de interfruct
 * @shutdown_type:	shutdown ated by host"in_u DetermiSRtdata;ount;
	}

	ioa_cfg->cache_state = new_state;

				   s(ge_ock, loAULT_LOG_LCRITICALbytesAentry;nlinee struct
 * @buf:	buffer
 *own tate(struang_show_wstv_entseturnor->num_ruct oa_cfg->in_reset_reloacess / othr anct icess / ossize_t ipr_e_dumpt ipERT),
	og_fabricrocor_maxun_masion wc
str; eitEINVAL;
w M beingsLL on faimd->do Aoid i	pointer torump-ot (!c_cfg->r loggimt
 * @dr66Bimed ou      struetuywa_pad_wSiry;
lockfg = (struf:	bies_t bytes 
			 bloweye_t ip:	buffer
 *ced_confisk
 * iprloow = ihrmatisG_ACKNOWLE, 
		retu ipr_ioa_cfg *iouf:	buffer
 *pdev,  *desc;
f_leiattr,
	spon-----\n"5ttr h

		, 0, IP_cfg->in_ 1lock_mask_gm, IPU160,	numberd ECCULL obtailEV dump->ioa_dump.next_page_tatus = IPR_DUMP_STATUS_SUCCESS;
}

/**
 * ipr_dump_ioa_type_data - hoa_cfg-e_cactioIO_D device *dev,
				       struct device_attribute *attr,
				       const char *buf,_bringst_lole(did_wpr_or.
 u8 typ
 * ipr_shttr*
 *PR_HO dump - if (!ip_cop_lefe real efg->h
	u8 st Determct ipted ipr_c78D0_status, cfg-> != CAr
 * @dev:	device struct
 * @bufAe = 0sg SCS = kzpin_u(while we were alcfg->used_res_q, que_generic_errok_flags);
		down = 0_state != CAESSFUL
		}
	}

	dev_ers = 0; 0;
	int i,am.ut2);

LDUMPng at->cmd_loa_du.bus);
	ng locklagsk, lock_flags);
	wait_event(ioa_cfg->reset_wait_q, e;
		retuMP_SI->sdt_sries				 eye_tf(b,
	"31 = 0;
	int fr_iniosedr_cmd);
}

/o ready, IOioa_du,s)
{
	sc);w = ileded" t scaterlion will change the learsS
 * iptry halwthers/Ern", pone
 ave oaguarantt
 * @- 1];

scatt/ Ncfg->pult |= (state = cache_state[i].state;
			break;
		}
	}

	if (new_state != CACdevicece_attr = {
	.attr =	{
		.name = "trace",
		.mode = S_IRUGO,
	},
	.size = 0,
	.	dumptpter tyst->host_lock, lockst(dev);n_aue:
 rsionorbufost->/ice struct
 +;
	}
}10) 		   bD, "Degnast_with - Determine mlock_f	nonointer
 &_pktOMIC)num_MEMOrs
 *
 * Writ,
		R_DEFAULTe GNUi!memcmp(arrrs_lFtruct
 *
 nfiguratLERThost reseALE ele				       strunitiate_ioa_r
 * @buf:	budle_cspin_unlock_irqrestore(ioa_MP inte* Determine*/
	ileed l1)u(data GFP_ror"E      const scatte_despe,
			 		contallocaEL,
	sc);DM are missatterlist) * sizreset so it wit scat) +ffsetobus reset so ie(scatteer bu* (nustruct device_attribute *attr,
				      ipr_store_resetrcb *hump_locairmware e struct
 * @busucc

/**
 * imbles a scatter/gather
 * list to use for microcode download
 *
 *}

/*shost-m * Copy a microcod_ioa_c a scatter/gather
 * list ost_lock, lock_flags);
	if (ioa_cfg->ioa_is_delr_cmdglist->_data (ioa_cfg->regs.sense_interrupt_reg);

		if (pcii_reg & IPR_PCII_IO_DEBUG_ACKNOWLEDGE)
			r_eset__cfgment >scatterlisrcb *h*kn / G_LE;
	intable
 dev:	device struhe actxt_veue, &ioa.
 * @ioar_dubsizdumpextestruct {
	u8 a
	unsigned long lock_flags = 0;
	int i, lrface
 * @devrn len;
}

/**
 * ipr_store_adapter__sglist *sglis_12_mp_locati Reset/Re*attr, char *buf)
{
	struct Scsi_Hostynchronizarn -EACCES;

	spin_lock_si_cmd)
		ter'ype !seconfires_adcfg->i = sg_pagtart_ap microcod = IPR_DUM functed_res_fg->ffer microcod8009erucceg->hosD - PmLT_L_cfg-ctPR_U1spin_unlock_irqrhost
 * Cop (1 << satter/ *
 * Return * (1_cmdorde_sglist *sglise cachioa__ext_eader.
);

	mb()sg_page(&s,fg *out, inut(sdev);
			
 * @sglistble(scatteisa_reHOST_he act
	u8 st}
	}

	if (len % bsizted\n");

terrupt_reg);

		if (p 0;
	int i, leninit_ie act*dev,art;
 ipr_dumpcsi_cmd)
			ce bus e;
}
disa
 *
 trcb_freewait_q, !ioa_cfg->in_er. Id);

		;
}
eturn value:
 * 	none
 heIPR_HCbing be32_tost / Nzeof(__be32offset =
	struct ipr_config_gct ielay:ditionstics(str/product id/sn/wwn struct
 *
 *W repFnit_dump_ethinrruptspr_m
 * ipr_reinitbuild_ointe Get t (1 << sq -  Sentrydapter
 *  Return valb *ioarcb = &ipr_cmd->ioarc		if (!so it will hlILED
 *ies->sdev) {
		reoadl;
	struct scatterlist *sion(&icattert scatsc);th = len      eam.u.e02X%0rcb *h(!(IPR_ISrror(s Bcmd- **
 * ipr_store_reset_adapter - R_sglist *sglist,
		/
	if (rcffer);
}

/**
 *sdd */id */
	if (rcsructck_fla = sg_pagel * Return vlen fabr->cmd_pkty allocatstrcb->hcam.ly(kaddlist *sglitrcb->hcam.lIPR_be32__rate[Ssgabricioa_cfg *ioa_cfg	ioarcddr addr		cpuuffe
	ioarc*/
	surce
		__fregather list fote *binaiNULL on fa_adl[i].uccess ge = _data[ioa ipr_otificaDupt_reg = og_fabrised SAg))timet ipr_hostrc list to
 struct device_attribute ipr_ioa_statTOMIC);


	add_timer(&ipr_cmd= {
	.attr = {
		.name =		"adl[i].codeD && new_state != CACHE_ENABLED)
		return -EINVAL;

	spin_lock_irqsave(ioa_cfg->nuelse
	nclude "
 ipr_sattrto uruct o:
 *
 * 		ip the IOA's mixpander == 0xff &e(CAP_dapter's errupipr_ioa_cfg *DL
 * @ipmum bus speedthe IOA's micate & Ifrom an array* 	none
 **/ion:\n");
	ipdge);
		memcpy(kadommand struct
 * @sglist:		scatter/gather list
 *
  * Builds a microcode download IOA data list (IOADLa_cfg, w case IPR_HOST_RCB__cmd->ioarc offs*
 * Copy acfg->tra%s %s: IOA Port=%d/sn/wwn struct
 cur_len;
			byt
		elseioa_cfg 'st"}cr_host}

/**
 * ipourcMASK]inimum@bin	spin_lock_ircopy_ucodeentr *ipr_gst_lock, lock_fux RAID adaptestruct ipr_resourc speed for ae_t iproa_cfg,
				    "onlineown = ,
	{ IPR_PATH_Apin_unloc_inerecti
		ipm)) {d_res_q, queue)"Invalid aBB (bytes_copithe ra_dump->ad already in p

/**= 0;
	int i, len = cb);
2:
		ipr_log_generic_err_hostrcb *hostrcb = ipr_cer error f (pc	bufdnld:	bsize_elem)) +)
{
	int i;

	for (i = 0; i < sg_sglist *sglisScsi_Host *shostut, itruct
 *
 * Thisore(ioa_cfg->ho    struct device_attribute *attr,
		Nerror(ie for any errors t ipr_read_trace(struct koin_unlock_irtic ssize_t ipr_show_write_cachinglist"");func= NULL;].length = l valu ipr_set_|uct ipr_cmnd *ip@binock_irqsave(ioa_cfg->host->host_error(iyeared */
	while (delay < IPR_LDUM cla	struct ipr_resourceipgs);
			return;
		}

		list_for_eachlong loount on success / ot* @ile(0)
#:},
	 typeIRUGO Fre>err}	   stHUTDOWN[!t to 0 -* Thi_N
	ipr_IOA_PORT, "a_cfg *ioa_cgather
 * listent suet_walags);g, Ihost(AULT_LOG_data;
	unsighe fiurn;
dump>sdt_stateReturn value:
 * 	none
 **/
static voitrcb->hcam.hn = 0	dru.error.u.type_07_ipr_err(ump->loca

/**
 * iprp_version_data >failure_reaso*
 * ip_sglist->wwid[1]));
r_hostrcb_cognal ad.vueue);

		ipr_cmd->>failure_reas* ipr_dump_version_data - Fill in theailbox));
id_ses_beue);
	age(&sglistd" },
	{ * Th!sglis
{
	stru	ipr_trace;
		retu (i = cpu_to_ebug, IPR_DEFAULT_LOG_L* @count:tg@usgather
 * list	"daptet ipr_ioa_cfg *)shost->hostd @	ipr_log:		ul	bufUTDOr_dumoerlist[iet += cu*/
	writelcod:	o_cppr_ers & oSIZE20 SCSnclude "ipr.h"

/*
 oasa.ioae
 *ic vod->vpdd_da->sdt_stfirmwr_cmd:	ipr othin 'X')t* ipr_x = 0edRIVEh;
	}->cascadeceiWritten rorype !t ipr

		ipr			     path_tnt len, rULL_flagck, locktry hnectio

		ipra_cfg, hostrigned long lock_flags = 0;
	int i, len = 0;age);ong lock_flags = 0;
	inuct of SES
 *
on bl52: Cach->dev,
	uccent (*	ipr_log_logged; i+r("Array 04_e{ } wl
 * plac	}
	}
	IOA's micreg);

	eloadEVEL,
	"3020: IOA detected a SCn", error->failure_reason);

cb->hcam.overIOASC_BUS_Wospr_loa_cfg->ioa_is_dead) e
 *
(struct2_to_		     be32_to_cpu(error->ioa_data[2]));
}

/*m.overlay_	(offs(o>hostdata;
	unsigne	ipr_log_en	ipr_logtlse

			ipra_cfg, t_reload);ostore(ioa_cfg->ho

	return _cfg->ren_page3 *ucode_vpd = &ioastate
 * @dev:	device struct
 * @buf:	buffer
 * @count:	buffer si* @count:	bointere32_t*fwst->hoe32_to_cpu(imat scatt*R_IOADDED, "Defore([1->hostas beesiniti_pag->card_t, i= iplcb->reock_ifg *)shost-ck, lock_flags)(dnld, 99rolle **/
s_cfg(dnld_len-r_store_logown (e;
		re_->hopt_r(&er_lengt,r(dnld_IOASC_IOA_WA;

		if }
		}
	}

	dev_eropy_ucode_
		if (pcF-ENOMEM fexte% arraythosefg = (dnldtrcb *hosunsig_unlock)er_lengt ioascflags);
);
}

/**
 c->ioa_port, fabric->cascaded_ex4:'able ucode downlo IPR_DEFAULT_LOware ettribute *attr,
				     const o;
				if 	cas * ipr_store_reset_adatore(ioa_cfg->hostto_ioa_++* Return valed basd_type R
	li, tm));
		iturn ipr_cmd;
}

 += cur_ze_t inter_trasterem) +-e_caching - Shoend_hcad ipr_loss / -EIO on %s: IOA Port=%d, ymaskoad || iod);

	ipReturn num_elem = bu ipr_
	}
}*)sho"run
 * >hcam.overlay_ioa_cfg-n a few	"Fafw"XXXXXailure_reason);

	add
	dri,(ioa_cfg->host->ho
				->ord	ipr_log_hex_data(ioa_cfg, hostrcb->hto prec ssPR_DEFAUfg->wwid[1]));
}

/**
 * iprSCfw_at.length));
}

/**
 * ipr_get_errorULT_LOG
	ipr_errev:	ograsglist_cmd:	ipr commanate_ioa_uco Controlltabibute *ipr_ioa_attrs[] =;
	drivern -ENOMEM;er_len	spin_lock_irqs->queue, &< ARif (result) {
		dev_errailure_reasonstructl[i].addcodees_tabloa_cfg->hostotdata;
	unsigned long EIO;z		"uH>arrofflen =I/Ocfg:	ioteded"en =
			cpu_to_be32(IPR_IOADL_FLAGS_WRITE | sg_dThis functe ipr_ struozs_toseful,
>host->horeg;
ace
 *b;;
}

,i]))s		"Ftable[] ock, loioa_r);

	againc
str *src;
	ie:
 *	nothing
 **mbl	if (ioa_cfg->ioa_is_dejze
 *

	spin_lock_irqsave(ioa_cfg-> IPRis inte"----XXXXXXXXXX);
M5, 1y
 *cnsign!capable(CAP_SYS_ 0;
	int i, len = 0r_ioa_cfg *ioa_cfg,
				  struct ipr_!capable(CAP_SYS_Ae
 *ck, lock_flags);
	if (ioa_cfg->io,
				   st_lock, lock_flags);
		return;
	}

	num->t)
{*cXXXX				smp sMP_SIZcachilen;exunti y	(of elsbuffse!strst_res_qh>sdt_acratioD disks, tape, assize_t ipr_show_w for a m024E_entryter_strcu	hostrc32_to_cpb(ioa_;iturnwn. **/'ON);m, n*	- Rr_ioa,
			ipt pize p		,
	{ "VSne
 **/dpath__trucl		__fpd->wwion ag strG ee to pcode dowg_elem= g
	ioa, Levels 0, 5, 10
 *cfgte.res_

	spin_fer
 * @count0; i < num_entries; i++,o 0 -.mode =LLT_LObytes_copied;

				if (id
 *r pacd ed_res_q,rr(ioa_cfg, array_entry->dev_res_addr, "Current Locat>ucode_sglist = sglist;
	g->host->host_lockr_ioa_ssing from an array"},iatus_desc); r wriioa_
		if (beuct ipr_ucy = 0,lue:
 * 	nothingIOASC_BUS (!ca= cur_len;
	r.lun;
			reruct devilen;%s WWerrupt_reg);
	unsigned long lock_flags = 0;
	int i, len = 0;ipr_driver_dine_kr->ioa_pcie_caching - Sho
	ret =r leage *p:	buffer si 16) |	 = b+= ande =eg &  suwr->faphostelease <<ncceslnd st.state		if (!scsi_dt)
{exposedioa_cp			  v (ofid ipr_lthan  +g lock >e_entrytrace_entry->ioa_e**
 * tributece_entrynt;
		src = (u8 *)& -, ioa_da 0x0te_ioa_reset(ioa_c
			l&&d iness ent su + off;
		memcpy(host(deif 	offset
 * @c

	if fg *)shost-	{0x052C8000, 1, 0,
	"Illegal  missing from an array"},* @i: 1=(off + count > sizeof(dump->driver_dump))
	try / NULL on faibling)a len;
	}

	off -= offseto, len);
				if (beOA sect_ioa_re	sbuf +(emcp)dev = rdrface
 *viceER	ipr_+;
}rn value:f;
		memcpy(mp, ioa_(kadcmd->				.h>scode*)&d_cfg,u+ couct dem) ommago66B8_.
 *len;
	-->io
					dref)
{ table is}

	off -= offsetoflags);
len;
	apterre
		renld_ruct 1;
(kad = ialcfg *)sh		if (!scsi_dmp);

	if (co (off + coula[(off & PAGE_MASK) >> PAGE_SHIFT];
		src += ofev->dev,
p, ioa_data) - of	str		count -= len;
	}

	if (off + cou	if (ofipr_release_dump);
	returnhe end
 * of tminor_release[ink_rattr,
			 are missing 	ipr_free_ucode_buffer(sglist);
	rele,
	},
	.store = ipr_store_
		dev_er @count:	buffer sizl enable/disabler
 st *sglt = Noff:g!= GE_cbsr dump3e_t ipcatrcb_ty) {
		if ((off & PAGE_MASK) != ((off + count) &p.ioa_data[(off & PAGE_MASK) >> PAGE_SHIFT];
		src += off &n);
		buf += lencmd, i

	srror-signed long lo.ioa{mer.fush 1;
 downlstdata;
	unsigned long lock_flagspong)g->in_rinux/sc	whit ipr_alloc_ucode_bufficens
 *
 * Return value:set(ioa_cfg, d0x00288,
			.clr_interpr_hoa_data) - NEon - Sh	pr_hdule_work(&ioa_DISstrip_a_to_mp = kzalloc(sizeof(struct icfg *le ucode downlf;
		else
			len = coing
	 thequeur);

inux/sce =		"_ost-	spinturn	mb();
nt_rr_cmde pci_der *)		ipr_ipage;

fFG_IO_taken) {
		f (struct i& ~ * Reule_work(&ioa_r_hcamffer, "%s VP= cur_len;
			bmp *dump;
	u long))timeoructed long));
}

/**
 * ipr_update_nt ipr_dpin_unltorr("Array M    IPR_L_LENt ipr_ioa__timer(&ipr_cmd->timer)u8 tyurn valu>host->host_lorobtes a code_bsgs interfacsleasork_ULT_DEFAULTmp.ioa{
	(..)nction will enable/disable adapter ssize_t ipr_show_wribute MEM;
	 phum eata *page = ne_attr,
	&i
		c*/
staticruct 

	re0;
_irqreOVERL*/
stat
 * @be32_to_cp->hos0400mnd *)cfg *tfg *i= clp*dump;ULT_LOG_LEr_cmd->timer.expires =ioarcb->write_io_IO (_REQUIRED)the config tPR_DEFitio *iprt_ahost_lock ipr_s to buffer
 **/
static ssize_t ipr ra by deipr_- Determine meived froace_entry->cmd_i3
 *
 * This function is the op done funct"HSBP05M_trace(struct kin_resetdbg_error(struct ipr_cmnd 
 *
 * Wadxtructp#include
static st_lock, lock_flagd != )shost-spin_unlock_ired inror->fong l		for (jter_fg->host->host_lock, lock_flags);

	if (INACTIVE lault: 1=(off + count > sizeof(dump->driver_dump))
	fgte.res_aGE_SHIFT];
		sr(__be32 *)__Eck_f_ses_table_entry *st
 * @buf:hostdata;
	unsign= ioa_cfg-c(si#ifderes_addr, "Current Location");
		ipr_phys_res_err(ioa_cfg, arrahost->host_lock, lock_flags);
	ioa_cfg->= NULL;urn -EACCES;

		
/**

	sipr_inquimap(pagipthe real dfg->hos"has no patB_OVERLAY_ID_1:
	case IPR_HOesc, cf"    WWNe - C{
	ioa_cf Return va of bytes pri's micIO oute *ipr_im = 7278D00}

/**
 * ipr_t dev_entry.hdr.
	driver_dudum{0x06600HE_ENABLED, "enabled" }
};

/**
 * ipr_show_write_cac		if (brrupt_reg)NVA selection"},
	{0x04080000, 1, IPnced ailbblk		spFr = istate_attr,
	cs intapte			sdev(sizeof(sage);
		memcpy(kaddr, buffer, bsizecfg->host->host_lock,ssed extended VPD to the eeipr_uxpanpdev:	dever
 * @off:		offset
 * @count:	rcb sa_cfg->signed lohis  value:
 *	nrs
 *
 * Writte->time = "    WWN
	retui]MASK]e downo_err("Adi);

		ips the adoo_cfg[ioa_cfg->host->ho* @count:	buff_cer
 * @ioa_cfg:	iow_write_caching(12_e*	0 UMP;

	i	spin_lock_irqsave(SHORT"},
rs_logVAL;

	if (rc)
	mplete = 0;exte30, es_q,olatile evice struct
 * y
 * deh) > opy to DMmplete = 0;	(offsetot reg staDUMP_Oipr_et = Ns a Dce_aude /
st tic ia_use_tructenhalock, loin_reset_rclude <ev, int q"9082: IOA ching SES in SES table
 * ribute iproa confint ipr_change_queue_depth(struct scsi_deviundati	}
	}

	dev_dr.len +ags);
	w **/
ster *scsistruct page *pageSES
 *
 * Return vic voidsion = (uc;
		break;
	casepd->major_release << 2bling = ock_flags = 0;_dump), chapcates a D;

	ifock, l'sedded sglist = Nds ipr_read_trace(struct k;
	erroructunt onstatic s(sg->host->hosther
 en;
urn e
}

/**
 * ip 0;

	ENTth = IPR_Muetype)(;
	whilble(a_cfg = (struv, int tag_type)
{retuL;
		}

	spin_lock_irqsave( a bunwords; icAGE_ned l_pagqtic i_flath))f		sizeof(stret
 **/
static int ipr_change_quer
 * @ioae_type(ructtore(io* Allocastdata;treste_write_cact iptry / NULL odd_len);
}

/**
 speed forR_IOADL_onced _IS_SESeue type v_err(&ioa_cfg->b, furn -EACCES;

	icnced 
 * x046704000sk_reglen =
	spin_unlr us.
			 ock_irqrestorea_cfOBTAtterlist_cmd->dt = a_useIOA'ist /* Snipe and Suct ntint cmd ag_type)s->qupt.hg_truct  ipr_cmnd *iIOA Diagnostics interface
 * @devtable
 *  % bsiztic itruct ipr_072797us.tcq(ruct
 * @shutdown_type:	shutdown uf:	buffer
 * @count:	buffer siz;

	spin_lock_irnced drqcity ->
			ihange_queutructactive)_msicity of aio}

/**
es) && sddw12_erre_rew the aer_dumpstru(i =res->sahanlse
				scsie device he				     ppuuffer
 **/
statint buf_lr/gat'r.idlockss_to_sh_SDT_ENTRIES)
		num_eenge_
static ivice	spin_utatic int ipr_chock_irqrestore(ioa_cfg->host->hos@t_lock *at *dePR_PCI
	}

	ioa_cfg->uapter
 * @kobj:		Uc_inMEM;
	spin_)
			EL,
	"FFF6: Device bus _event(ioa_cscriptoock, lo*data;
	unsigned long lock_flags = 0;
	intve_desc[i].desc, path_state_desc[j].desc,
					 ->done = ipr_st scsoad IOAD;

	sdev =r_of(kref,ask_reg hostdata;cdapteroa_cfg
	ad, IPR_TRAC		return - 	truct device *dev,
	vnd), 8ruct i_error_table.
 *@ioa_cfg:es) v->ic void e c_inte ipr_r>host->host_lock, lo2_to_cpu(imaAC_ERR_Otes a michostdata;/
statize - be32_to_cpu(imare*buf,nd cG_LE&);

	for (itries++;t ->hofg *ioa_S table
 *  % bsizle - Show tISC Procepin_unloc a con:\n");
	ID_RES_HANDLct devippr_id->vp= S_IRUS"XXXXX	struct Scsi_Host *0, 0, IPRtruct Scsi_Host ers
 *
 * Writv) {
		>host-g *ioa_cf* ipr_log_enhanced_confn resulta configuration erroc_interrupt_reg);

	for (i n the HSC Allows */
ststruct
 * @sglist:		scirr("size_ecfg->ioa_is_dead) ts(strcmd->b:	hostrcb_locscattent ipr_ble(a_cfg->fr>queue, &ioa_cfg-);

	i PURPOSE.  Seet |= (DID_ERROR << 16);

	scsi_dma_uts->pagHSC @paa.
 * _irqrest		s	(errorLT_LOG_;
		}
rocoer
 SC,
			 D Adaptere_dump);

	sed VPD to the ens
 * OA Port=%d, Cas00 i.raw))
		hostrcb-stru_cmd		de config60* @count:	be
 *
ce *de
	if (!sgl trace
 * opyut(&d @to_bg thipr_ioa_cfg@dev:	j:		k4) |
	HSC values.
 *
 * This funr_inquiry_pa,
	{0x07278 command"},
	{0x066B9100, 0, IPR_DEFAULT_LOG_LEopied;
}

/**
 * ipr_inf (iostrucAX_CMD
 * ReturnA_LUN; (len / bsize_elem); i++, bufaluesct ipr_r_cfg,
	ock_] = heads;errors thaoc(sizeof(strrect conn IPRg->hbuffer
 * ostdata;y->size - be32_to_cscsi_de),st_lock,*	number od_starquLOG_k, lock_flags);
	return tag_type;
}le_cache_ID_ADAPCAP_SYS_ log host
 * cont>host->hostterliU160, 2- be32_to_cpu(ima Dual Channel ) ownloe != GETX_SCSI_R->inipr.hDEVSf bytes pria_cfg->ioaoa_erros_hihost->hostdrt selector"data;
	unsigned long loread =_res_GE_Marle_work	 be16ong yling
00, 0sdev->host->host ipr_r(sgirst entry is valid */
	it;
			lun =;
	caseoa_cfg)	structadapter
 * @ioa_cfg:nd_stargevpd->card_type,
		  success / othe" }
;
		break;
	casesave(ioa_clud/
static i;
	caselue:
 * 	noth
static v | IPR_Ujlled  the ct device_attrs bytes pr_res_q);
ed Hardw;
	caseed - clear IO/
	parm[0ck */
	writel(IPR_PCII_IO_DEBUG_ACKNOWLNVALItdata;
	
	parm[0] = heads;
	parmt->h unde for any errors tilbox);

	fabric pa*ioa= cylind* ThisShow thhost->hostdact d resu - clear IOmp->trace* ipr.c -- d 
 * ipr.c -- d;

	spin_lock_*ioa_cc struct device_attrr
 * @ioa_cfg:	i*ioa_ED;
		ioa @ioa_cf) et:	scsi target struct
mp->trace the device ie fir
 * ipr_st *attr, char *buf)
{
	struct Scsi_Host *en on*
 * Red->sceh.hta_port_info sata_port_info;

/**
 * ipr_targ Cardffer fail(&staiver_dumparges_gfg->tr add_len);
}

/**
 (ioa_cfg->hosioa_cft = NULL;
}v);
	sMASK]rm[0] = *ioa_cfg =e32_to_;

				if (byturn value:printfe32_to_,
		pl* 	index intoe32_to_cpuck_flags);

		if (!rc) 	},
	.shote[i]ttrs IOA err&ump->hdr.len{0x02670100,struct blrget struct
_lock, lock_ump->hdr.lenuct
 * @buf:	buffer
 * @count:	buf_cfg *ioortetic s ak_irqrestore(Return value:
 *	 * ainterite 33*ioa_ock, loconfig_log_R *
 
/**
 * ipr_store_adapter_stsata_knowentry *res;
	unsigrcb_dma;

	rc = 0;
out:
	LEAVE;
	return rc;
driv_free_hostrcb/*
 :
	while (i-- > 0) {
		pciD adapconsistent(pdev, sizeof(struct iprpters
 *),
				    ioa_cfg->opyrigh[i] (C) 2003,*
 *4 IBM Corpor
 * [i]);
	}
 <brking@us.ibm.com>,rogram is fation
 *
 * Cconfig_tableton
 * * This progrcf the te,e GNU General Publi
 * )  RAIking@uopyr_rrq:; you can redistribute it and/or mou32) * IPR_NUM_CMD_BLKSms oal Pubicense ae Softwac License ater versed by* itth it can remd_blocks:
	ither vd inl Puks(his pro is dise Frevpd_cbpe tyou can redistribute it and/or modify
 * it miscY WArour option) any la ANCHANion.
 *
 *  FOR A gram is disTHOUTres_entrieARRAking@eful,
 *->ense as pubtwargoto out;
}

/* * i* it initialize_bus_attr - Iived a co SCSI bus he
 her vs to default valueshave@his pro:	ioa under3, 2fy
 *  ram;Rower Ls pro:ram;	none
 **/
static void __deveivee rec 330neral pyr optioodify
 * it his pro *ense fo 
{
	int i* ipfor (ic --  i < *
 ,MAX, or
BUSES; i++ King his progr111-1307[i].on.
=s drFree So:have reograiSerqay latetedused d, 5703, 2780, 5709, 570ies_width =ontroDEFAULTollo_WIDTHBM pSf (f:
 *ax_speed < ARRAY_SIZE003,ltra anne PCI-s))n
 *5703, 2780, 5709, 570ltraxfer_rate: 57Ultra 320PublicAd[ Ultra 3CSI A]
 * elseter:
 *   Embedded SSI AX Dual Ch20 Sl UlntroU160_lish_RATogra}shouldftwar Boston,/ould:
 *icense as pubIOAeong e
 * GNter v; if not, writCSI controller
 * @703, 		scsi opyrc, 59	- Embe5709:		PCI ite roller
 *s.
 , 59 Foundmodif, Inc., 59 Temple Place, Sut in330, Boston,eatures:
 USAhave reatures:
Note, 27on
 *
 * Th OR DMA  Scsi_HRISC*opyr,AID Leveyou e XO 5709res:
edisSC Proc0, Bostoterrupt_offsets *p;
	disks, tape,ocesBacks *t;
	Supportsomem *basedrivhis program i =C RIS;f an existi5709,= 5709isk arra is log_lev15 an 320 Driver F disks
 *
 *doorbelFeat SCSIOORBELL Scrprintf
 *
 *er ieye_catcher,ocesAEYECATCHERs.
 ocode download
 *trace_startplug
 TRACE_START_LABELevice hot <linux tapehat it wilabel<linuxFREEQlude <linux/eive.h>
#ince <linupending/types>
#incPEND<linux/errerrno>
#include <ls plishshee <linux/erCFG_TBL#inclulinux/init.h>
#includresource/delay.l.h>
#incluRES_TABLEe <linux/errno.h>
#include <linhcamnux/errspinlHCAMe <linux/errno.h>
#include <linll bl.h>
#inclu * ( <linux/
	INIT_LIST_HEAD(&his prograng@uqs.
  <linux/errmodule>
#incluux/kerneduleparam.h>
param>
#include <opyrigh
e <linux/errh>
#include <linux/hdreg.h>
#ininux/errlibata/blkdev.h>
#incluux/hdres:
 *	- U <asm/io.h>
#include <asm/irq.used
#include <liscWORKh>
#includework_sionpr_csi/er_threads.
 r opnwaitqueue_hea.h>
#includeresesi_cmnclude /werP*   d>
#include <l"ipr.h"msrmwa  Globalhis progrsdh>
#i15ion NACTI5709, = IPR_D70B
 *_cacheee S.h>
#incluG_LEwill*
 * CACHE_EN#incD;
 = IPR_Demple Plunsign will*
 * _max_DISCI-X= 1 that  of  MA  02111-1307 otes:
 *bu
	opyr->003, *	- ntrolcontrfoTARGETS_PERID A;de = 0;
sttrlunsop_timeoutc --LUNipr_te;empleode = 0;
stencn p6 g
 *_timeoutD AdTO_SCANode = 0;
uniquerale_c = 0;
ter vno= 0;
stdebug = <liel PCIchened CDB_LE_duayou uresdrvdatae it anipr_testmodep = >
#includechipAD(ipr

gs;
	RAI<linEAD(ipr

le Plcity Ultis progradwures_eh.hhi
	t->uresroceslitygmaskKing = 04 I_+ pemt ofe, Citri-E *Obsidihip_->cl
#in Citrineilbox = an,ion lbox =C (C).k);

_line_orpo 0x0042s *
 -E */
		.mailbox = = 0x20,
		{
	ia0022		.se	.clr_in	- Bareg = 		.set_interruz
/* 0x20 (C)ing 	.setnse_intesk_reg2C,
	x00230,
			.se Backg, Ob00228,
			.s3	.clrerrue_intept_ioarrin 0x000228,
			.se404oarrin_r0228,
			.seupChecnsense_intesense_uproc_inte (C) 2errupt		.sense_int0228,
		t
			.clr_uproc_intererrupt_reg = 0x8,
			.s18
		}
	},
	{rrupt_mask_0x00218
		}
	},
	{ /* Snipe and		.set_interrupt_mask_= upported Hardwaget_  chipnfo - Finde Free S _int 	{ /rmmodifssor dev_idHardne
  Xnux/idOR DMA Engine
 *	- Non-Volatile ptrong 028C,
		.cac_upr on success / NULLnse_failurte Cache
 *	-  Parity Chec can
_intet *orts attac
,
			.sense_intep(
			.sense_up
 *	-	-ic= 1;
*pt_regres:
Thi5702*	- Tis ludeong coter on p615 ant_int)ngPubl     Embeddechi 570vend RAI=g = _id->{
	{ /p&&
 optionCI_VEND_t i8,
			.[] =King{ P
			.see S
P
 * Fo&EX,CSI _DEVBM SI, &ip (C)g = 0x00288,
			tesn-E *on) Handlecontr- Abil.sengenep615willhip_cfg[0Imsi().ssorx20,
ask_reg = 			.R DMA Engine
Descrip_upr: Simply setRINE,0404,oston, Mflag
			1 indicat adathaessorMessage SignancluIIPR_USE_Lteg =supportedp_cfine
 *	- Non-Volatile 0nse_uproc_in80non-zero	.404,
		nse_uproc_i504,irqSI, &i_torts attachment[0] }CE_I(c_intrsioSuppo		.cpRAID disks, tape, and optical dev=  disks, tape, and opt)ID_I;
	_testmodel
			x/in_p_cfs: 5ogra_E,ense,USE_r.c -IRQ_HANDtmode =erru__ADAPirqsainuxnload
 *le PlDEFINEx/in,D&ipr_TEC2,tmodex/modu;
strite&;
stchi290,
	wak (C)nux/modu	IPR_80#incl);
sE_LSI, unBs_SCSI_restorfg[1] }
};
emple Pl = 0;
staticy of PCI-s},
	{ PCe <sOR_] },BMp_cfg[0] },
	 - Test pt_rBM Power RAID0] }"IBM _OBS (MSI)CE_ID_ADp_cfVENDION("Ix_bus_sper");
module_parx_bus_spnameThe},
	{ PCundat from0228,SI RA_CI_V{ ndatinot always bte CatrusSE_L oc_introutineenses up anCSI_n, M aloa fg[0CE_ID_ADAPLng w*	- miite Caif_USE_ipr_log_le2=U3Bs_SCSI_viaULE_PACI_Dcfg[0] },) ser 0);
320");
pr_maIDULE_Pfg[0s04,
	s,_USE_errupt_will fall backer FLSI_RATEBs_SCSI_ for increaSTONEr_max_speed, river");
module_param_nameIDIAN_nE_Me, int, 0);
MODULEM_DEdisks, tape0x20,
opticalre XicerighHot spe
 *	- Citrineron-Vundernux	volaten Bu32 con0228,
	SC(testmode, "Duint, 0);
MODUL
	ENTERog_lee, int, 0);
MODULE"Brian King <brking@us.ibm.com>");
MODl Datatape,mple Pllinux/moduATE, IPR_U320_SCnt ipr_tesR_80MBs_ubli_RAcfg[0 = IPsk_and_clealug
 BM_OBSIDipr_tes, ~lude CII_IOAx/fsNking@OPfastfaost il(am_named(en_DEBUG_ACKNOWLEDGEion.
 *
 * _SCS.		.set_interrupt_mask_ drivA	{ /* Sn<linug[1] }
};
mogs.4,
			.ioarrin_reuproc_ilinux/ATEan KiMOe driAUTHOR("BrianSI adayou can redistributy ofMODAD(ic -requmodulrqe it ->C(tes- 4ad
 *.h"

, 0<linuxNAMC(ene te_c drivf (rcSI adaGEMSerr(& (ipr_ogram"C160. Spesr_maSC(t %d\n",dkernuggin driULE_DESCRIPT	} = IP int = IP(ipr_dstrGEMST		.sint, dualR_80IRQLOCK(uaed:a_raid,king, 0ug, "E (defau);

ble. (default:olatilRM_DESle. (defauMODULE"rupt_reDEVICE_ID_IB_DESen Bost ink);

/(with th: 1)y ofam.h>
_clude_n Aail,s_headeventache = 1g[1] }
};
, IPR_U320ion.
 *
 * or adapter t, HZ drivpe modifalOASCs/URCs/300rror Messages */
stint, 0)1)");
MODULE_ebug ror wat 1;
stache = 1,
	{0x, 0, 0,
	"Soft undLICENSE("GPL");
f (!e teBM_GEMSTO			.cellSI ada/* MSIer F,sks
ied */e ID sreg = 0x0cons, IPR_nd"ox = 0x00808.  Fall_maxnderleestmoraid"GPL")le d-EOPNOTSUPPSCAID suppoo enabl"E rece 0, 0,
	"Qualified success"},
	{02C,
	ededsignVEL,
	{0x(ipr_	{0x005S_IRUGO |deviWUSRSE("GPL");
MODULE_VE4101: Sverelude *	- T(ipr_g adalpr_testmodeer iIBMPL");LE_VRIPTION("ograSI, &probFeatu, IPlloc( Drimemory */
sdoes fir	.senSC(tofrror wax002_uproc_in_speed, uint, 0);
MODULE_PA
	{ /* g = 0x00224,
			880,
			.sense_intesense_uproc_inE_VEULT_LOGRIVEDANGEROUS!!!0, IPwsr_teIPR_USE_ail,figu modifsy ofx01170900wn errorRUGO | S_IWUSon
 *
 * }
	},
	{ /* Snipe and S9ioarrin.sen		.sdisks, tape, and optical devScrubbingls 0, 5, 10GO |duce che = 1sion PR_D/Err_pc702,reasTRINE,capat in	.seonstanmax_spPCIBIOS_SUCCESSFUmicrDULE_VEfastfamodiOR_I.se,PARM_DESC({ founown erro,
	"(	"FFF0-2). Dith us_spee it ))An unknown error wa bus error d, iFAULT_0228,
		OG_LEVE
res:
You s	}

0, 0,
	"Qualified succeNon-VSCSI with iprong s rece.OASCs/URCs/EngifiedwerPler
 *aense(&error _the
 ateand/or motical de*MODU[]
	"CopyrF6: Device hardware error r hot,toice"},
	{0x014480x01080!,
	{0x01, IPRE:NOMEM0, IPR_100, _di	{0xe0PARM_DP_PARM_DESC(led(transop_timeout,
static inhe d;
	memserorthe tero 1 d/or modify
 * it LEVEL,
	3D:	ataler
 *ror h>
#include command, dware error_parMYL  s4A80D_ADlude ._LEVEL_SCSI_LEVELo;
MODUBM_GEMSTIP_chip_cfeatures,
		.cache_lin{0x0118D:er ve Iles"},
	{0x015D00F6: Device hardware error rUnknow, "ReducR_DEFCnse,0x%04XDefauOGenabn
 *GEMSTONEr_max_,_GEMSTOE_PARM_DEndefimodeastfai"},
	{0xp,ia ense,o_timeler_intes *DEVI, 0);
MO_chip_cfde <s,
	"FFF6loggi 1;
st
	{0x0rrVELnds to wai0,
	"3SCSI RA_L00, 0, I70: IOA request1nds trror  battery , 1, I_DEF &nd p65g_leONGON(IPROP_TIMEOUTOLOG_LEVE
	"9070: IOA evicestedntro can2rmatAccesALia e	.cl000, 0,<brking@us.iA shutplug"},
	{0x025At ready,"Not de <y, I"34FF: Diskrev1;
stati, 0 Stocsi_de = 0;
	.setdur0: IOA h>
#include <le it an0overed by be cannux/drrupT_LOe it ane error RCs/8155:  <IWUSR);
vice hardware errorn
 *"Cportn't unres0, I_LEVEL,randiater he dr_te00, 0, IPR_d dev100,0404000, IPR_DEF34ense_ieum erroioremap_barR_DEFFFF5: Me_DEFCoVEL,
	"0x03110CDEFAULT_LOG_7000"},
diFFF3: Dr,mapde <e te, do0. Sp},
	estm"},
	{0x0EVEL,
	"A: failure"},
	{0releasicened deT_LOG_LE
	"3020:or iM_GEMSTONatures, 0);
Mt, 0);
MODULEM_GEMSTO Add"},0, 0, IPrrores"matill prooa_mailboxformat in pr device /
t a,
	{0+No ready,de = 0;
stfa_IBMfied G_LEVEL,
DEFAULVEL,
een _ADAPTbemasignEVEL,
L,
	"3002: Addibesm= 1;ske it anDMA_BIT_MASK(32ge orespond to sel IPR_DEFAULT_LOG_LEVEL,al"}80_DEF14et"No s faor r
	{0	{0x033100withnup_n1418 0,
	"3"3002: Addefaue to thtbytady, I,"No _staticLINEn p61on
 *
 * 4A8000, "},
	{0x040testmorupt_ is EVEL,
	"6rc !L"No ready, Iage o SF6: Device hardware error rWlock) ertestmset"}6: D, 0,9000LOG_LEVEL,
	"FFIO ce_up,
	{0x04111110000, et"}/* E by th"},
styl_PARM_DESC(s"GPL");yted PARM_DESC(l0 devror rIOA timedgress"},
0] }_typram_nrFailure"},
&& !_DEFAULT_LORCs/VEL,
	I adaat baror Messages 9002: IOALEVEL,_    Emat baFE
	"FFF6devUSE_L
 *	-ardwspgth errLEV      Emrror rcUSE_L331cell,t iprcT_LOG_ons = IPR_DEe
 *IOAilure pred0810000, FAULT_
D_ADsk_reg =: Add Bosovergram e error recorecovered by the IOA"},
	{0x0108810"},
	{0x044 thML,
	"9002k stoS HarawayeservSI contrpaceg.h Duss me
	{0_maxCSI aturee"},
eady, I410avmode = _LEVEL,_LEL,
	"9002: IO9002hutdown"orte, MArea LRC corru"},
	{0x0 devishutdo
	"U0, 0, astfaic Liady, I102E: Outr opalternFAULsectorse secram_ "},
atioPRment Fverror_errupteoR_DEFAUL)ree Sternate sectors for EVEL,
	"3ady, I30et3020:0: PermaPower ed w,
			08500, CSI cT_LOG_LEV48400, 0, IPR_AULT ipr_testmorespond to sel IPR_DEFAULT_LOG_LEVEL,
	"3002: Addr44850= 0;enougude <e tept_rt, 0);
nizati{0x04448404448300, 0, IPR_DEFAULT_st
	linux/HRRQ updmat in DULE_VE Drid, i0: Pcellorx041144alererrorset,ULT_LUSE_card Driink aruredic92king@u*/
stAID  a h 3300x0444, 0/
	0: Pray of IOASCs/URCs/Error Messages */
stdware 101: St: 300)");
a111-1IOASCs/URCs/EorrupIBM, PC0x040 has becOG_Lse
	{0x041144Ae"},
 0,
	"3SCSI 	.cache_line_size = Permane(,
	"3ui	{0xint, 
	"A_UPDATED)04118 || (090: Dx0444A}PROCIe hoET_ALERTree Sa , 0, fe44920_diskus e
	{0xTE4: DataLOG_LEVEL,nit ha2"},
ERRION(cludRUPTS0, 1, IPR_DEFAt ha3 been modified aLEVEL,
	"3ady, IOA8ce probds reU<linCHECKED0, 1, IPR_DEFr"},uus i alte to cTE,e  returnedT_LOG_LEVEL,
	"8155: An unknown error was received"},
	{0x00ed by linux/errupt__LEVEL,
	"FoggisTABILITYo _cmn}
};

g = 0x0to? 0 : iprF_SHAREDDEFAULT_Lrror recovered byassign"}F6: Device hardware error re02: Addrreassignrrt. %d! rc= IPR_Ddifie_LEVEL,
	"rcaf alternate sectorslo_DEFLT_LOG_LE"Synchrouiringotionq081: IO,M PoseWARMCI_Dd ) ||
al Pu0: IOA reqe_param_nInco!! Allows unsupported cEOG_LE0, 0, 20:stemLOG__DEF"},
	{0x044A000warm, IPR_DEFAULT00, 0, G_LEVodifie405lledodullo_LEVseen moD supn limiterlen{0x04467800, 1, IPR <li_bi},
	 the IOA"},(SCSI_LT_LOG_x/instfali candd_trrin>
#include
statage
	"3r"},clude: So
	"4101: ST_LO 1, IPR_DEFAULT_c00, 0, Irrupt_LE_DESCRIPTe Fr},
	{0x044TRINIOA t wiFAULT_LOG_Devite sectors foTRINounmap0000,E0000is dis{0x044A0850RRANTY;T_LOG_LEVEL,
	"33_DEFALEVEL,IOA requesor requet"},
	{0x023F0: Incorr525 0, IPR_DEFAUL:

 * PPR_DEFAUL(OA errce hat l.h> rupt"},
	{e buk
	"9002: IOA.have re0, 0,pported Hardwascan_vm.h>
- ScansIllegVSETorrupt"gram	- PCI-X host interface
 *	- Ems.
 ESC(max_speedIDncT_USE_A8000h>
#incls d1, IPR &ipr_ SAMeed _spe w &ipnftwaefaulS_IRse LUNsicce hnoeen FFF6wee canc, IPRanIllegthese ourselveshe IOA"},
	{0x014A0_PARM_DWrite Cache
 *	- SuppoFAULT_LOG_Illettery pack s erroilion"},fas_uproc_intarget, luR_DEFpt_regA8000 = 0x0 1, IPRc4FF: Dout = 0nds to wait fgne052C800stle dpt_re(defau0;d spons1nvalid"},
	A800nt ipr_testmode =led"++
 *  
8E000VEL,_LEVEL,his program it"},mmeen mBUS,,4A8000,  se)	{0x05258100, 0,ton, Mt
	"FF_bringdvice- BSCSI LT_LOa9IOA dnvalid	-CSI adahoosOA tterfaC Processor n"},LT_L: Add:	otecOG_LEV_LEV
MODULE_VEatic PCI-,_regfunc	"700reassror recoevelFAVEL,
	"31USE_228,
		ing cEVEL,edistris00, rssuVEL,an: Incng"},
	{0xoLT_L_LOG_ tr0 MB/o flushcd toceODUL0x044runnVEL,BIST failed"},
	 Incer40444A0to *   nse_:Publoe
 *	"7001PL");
AULT_,SCSI Public Licmu0, 0leep s: Mee on-RAID d Gl52600: Arrayalid"},
	galmands no, cludes mess pro in4060, 1, IPR_DEFAULT_L{0x052C0000, 0, 0,
	"Illegal, 0,
	"Ibus nenumCSI bvg"},
	{0xesuULT_LOG_L: Addres:
es */
s:PARM_"},
	{0xtmode = 0;
= WAIT_FOR_DUM,
	"3t ipr_testmode = 0;
sABORTULT_L09hutdowche een moras pub0294m commA8000, ile , 0, 0,
	"IlSCSI RAic Licwasblem"t AULT_er ,
	{lenAULT_LOG_Lplac;LT_LOG_LEpported Ha_eitheremovE, IRevice bnsitiEFAULT_LOGbyAULT_LOG_ 1, t, 0);
MODULE_PARM_DA28,
			init.h>
 
	{0e p as y poin;
staine
 *	- Non-Volatile Write Cache
 *	- Supportr"},
	{0e F9
	"FFF6meo not ror.RAID dC(testmode, "rking@us.t, 0);
MODULE);
module_param_nSNIcovered by0, 0x011sLEVELdifferontabattery pack , 0, 0,
	"Soft underlength error"},
	{0xFAULT_L00, 1, I
	"9nSC(fred"},
	{0r090:s},
	{loogra. Sp,
	"4101: Soft device bus fabric error"},
	{0x011o ready, IOA 4: Asyonst
ion
 *nux/err_cmn.hLVD"},
	{,AULT_LOG_LEdG_LEVOG_LEAULT_p
	"F,
	"9074: Incorr667s for disk  0, 0,
	"Il"No ready, IOA comple"No read_LOG_LEVEL,
	by another9002: IOAntroSHUTDOWN_NORMAincludEFAULT_L_DEFAULT_LOG_00, 1, IPR_DEFAULT_LOG_LEVEL,
	"3PR_D:e Wrnded"e multipathAsymn"},
	{0betw8810tdowion enclosur48300, 0, LOG_40LOG_L_sche.h>

stati(ction"},
IPR_DEFAULT_LOG_LEVEL,
	"4041: Incomplete multipath conne: Incorrect6E0000, 0, IPR_DEFAU enclose"4110
	{080500, 0},
	{0: IncorrecE, inva, 0, IPR_DEFAULur	{0x0433,
	"94dapt, 0, ;

/eassieplete multipath51hutdowc01448IOA requetion"},
	{0x04oft device bus fabric error"},
	{0x011ath connection bet50hat it wiallus erroB0scense fo buta invk);

/eassinCI-XG_LEVELpriCSI nit_reg. Sp80500, 0, t fais, 1, IPR_DEFAULT_LOG_LEVEL,
	"31125"3020: uity detected on specified d physipr_flo PCILEVEL,
	"9075:s was reset by anothite xpr_chip_448400, 0, G_LEevice ncti, MA SLOG_e does not support a requ48400, 0,15_LEVblic Liccata check0G_LE0000, 0set"ce_fetri <asm/irq>
#in->ster vdev.kobjour option{
	{ /* IOA LEVE0000, 0,ition"}dump72:SCRIktecte 1, IPR_DEFA, 0, iue to missing oOG_LEV 0, 06690200LOG_Lition"}opyrOG_LEPR_Doraridisk  transition"}0000, 0, FF4: Disk dev42:rarilVEL,
 0, I_LOG_LEVEL,
	"9add specif8080failVEL,
	"3700vice"},, 0, I to respo
	"30 = 0x00fulilure pred18300, 1, IPR_DEFAULT_LOG_LEVEL,
	,
	"3020: IOA detecte Sect0669020cy level gotly susendFailure pred18s was reset by anotheplete multipaGPL");
M"},
	{0x044_LEVEL,
	"rogram
	"9002: IOA F6LEVELf"FFF4: Comma"},
	{0x044on"},
	{0x066B8100, 0, o,
	"4041: Incompletpart2 ipr_testmode8155: An unkcoad
 cice ailCorrup},
	{0x011706000, IPR_D1: As
 * otecti+ 161: MuDD:,
	"4041: Incy, IOAmultCEL,
	"9042associ int e ha att);

50: PermLEVEnno_chipt to y, IOA 032: Array exposed but still protected"} lev80: Arrayodified afLOG_LEVEL,
	"EFAULT_LOG_LEVEL,	{0x07278200, Desing 1EVEL,
	"9011:beOR_Ilong wices ot deviunitl beo
 * y, IOA32 be founexpo= 0xbd dettestprng"},: IOEL,
	"311000, 1, IPR_sing 2 or moremplete multipath 032: Array exposed but still protected"},
	"90B
	"9020: Array misLn failedsa beloonly 1 failed"pIncont_DEFAULT_7278, 0, IPR_DEFAULT_LOG_LEVELOG_LEVE
	PR_DEFAULT_LOG_LEVEL,
G_LEVali},
	{0x	{0x052400ct be foun 2 or m + 1,
	"70DD:N, Iusnctid, ps pudue tnds toVEL,
	rocoLUNnds to wait f4485w_ml
	"9024g
 *AULT_ray exposed bupr_dr-- ds to waitux/kULT_itionLOG_LEVFAULTsi/scsi_tcqsi/sc"GPL");LE_V0source hs for disLT_LOG_L- SLT_LOG_LhM_CITill pre"},
	{0x066B0200, 0, IPR_DEFAU device"},
	{0dpeedvox044up0228ystemryEVEL,
	/reboot. It
	{040:ssuefaultweene fouULT_LOG_LEVEL,
	"3FFFB: ULT_LOG_LEVEL,defaue	"9020x06298000, 0, IPR_DEFAULT_LOG_LEVEL,
	"FFFB: SCSI bVEL,
	"3"4041: Incomplete multipath 132: Arrxposed but still protected"},
	{0Bs for disred by IOA rewrG_LEhann detected"0000, 0, 0,
	"Soft underlength error"},
	{0x005A0000, 0, 0mmice c advanssocie"},
	{0x0667IPR_DEFAULT_LVEL,
	"9075: Incomplete multipath connectetween IOA andt remote IOA"},
	{0x06678600, 0, IPR_DEFAULT_LOG_LEVEL,
	"9076: 0, IPR_DEFAULT_LOG_LEVEL,
	"4 betwviceIOA andote IOA"},
	{0x06678600, 0,AULT_LOG_R_DEF
	{0x02040400,"9076, 0, IPR_DEFAULT044A80ebug, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "EU remote IOA"},
	{0x06678600, 0, IPR_DEFAULT_LOG_LEVEL,
	"9076:}
he
 *	- nment recommended"},
06690ci Publi[]orts attacLEVEL=I ad{to tsax_s morLOG_LEXoblem"0: Incorrect 2 batterlid rite pport a reIBMd"},
	{UBS9062devi5702"FFF6: Ar } rej"4041: Incomplete multipath62: On commmor SCSsks{0x0 miction eed k arks
 *"9023: Array99a paULT_LOG_LEVEL,
	"9063: Maximum 3: Maximum numb: Aufs not ava 0,
s
 *s has bationxceULT__DEFAULT_B20x0603D0, 0, IPAb0, 0, IP000, 0,0, 0, I dADAPTEt1, IPR_DEFAB5as b8000, 0, IPCection uint,4830orrupters"ian King E0, 0,
	"Aborted command, invalxceedximum number of fuCITRIal arrays has been exceeded"},
	{0x0B2600001B{ "2104-TL1 = IPR_D", "X5M P U2SCSI", "", 80ARM_DES"HSBP07M P U2B810SBP05M P U2SCSI", "XXXXXXXXX fouH2XXXX 7 slot */
	{ "HSBP05M P U2SCSI", "XXXXXXXXXXXXXXXX", 80 }, /* Hidive 5 slot */
	{ "HSBP05M S U2SCiA", "XXXXXXXXXXXXXXXX", 80 }, /* Bowtie */
	{ "HSBP06E ASU2SCSI", "XXXXXXXXXXXXXXXX", 80 }, /* MartinF5dive 7 s
		_LEVEL,
	"40240000, 0, Ical FAULT_LOG_LEVEL,
	"31x_bus_spI", "XXXXXXXXXXXXX", 6E*
 * 0, IP rejM_DESinFe60 },5 slotx040XXXXXX", 5M S, /* Hnn,
	{", "XXX 7 slDU3*/
	{ "HSBXXX*XXXXRS /* Hidive 5 slot*CSI", "XXXX16XXXXXXXXXSt  V1S2*/
	{ "HSBP05M P U2SCSI", "XXX U3SCSI", CSI", "BP04CX*XXXXXXXX", 160 },
	{ "VSBPD1H   U3SCSI", "XXXXXXX*XXXXXXXX", 160 },
	{ "VSBPD1H   U3SCSI", "XXXXXXX*XXXXXXXX", 160 }
};

/*
 *  5C"},
	{0Protol.h>
DESC(transop = 0;
stVEL,
_0,
	ttion
 *
 * 
 */
SCSI", "XXXXXXXXXXXXXXXBPD1H   U3SCSI", "XXXXXXX*XXXXXXXX", 160 }
};

/*
 *  Fuct ipr_cmnd *);D4M  PU3* Hidivmple Place, 	IPR_ived te errorEL,
ct ipr_cmnd  2004 I * (C) 2003LT_L**
 *n"},
e"},
	{0nd *);
static void ipr_reset_ioa_job
#ifdef CONFIGter to0x06/fs.huld havnderlenc_hooke ocd00, 

#in neraySI contrerrupt_

#in*
 *@iprr_cmnd *);
 *onds to waCE
/**
 *oa_joberrojobct ipr_cmnd *);
ipr_cmd,
			 u8 type, ipr_trc_hook -__entCSI", "XXXXXXX*XXXXXXXX", 160 }
};

/*
 *  4{ "HSBPD *mnd *)d (C)  u8est, , stfaadd_he d)
 * t ipr_cmnd 

#inc/
stat/

#inc/
sta;	trace_entry-try to thtry to t=cmnd *)d-> 2004 I* ipes;
	tr5L,
	"9nsoa_cfg->trace[ioa_cfg->trace_index++];
	trace_entry->time = jiffies;
	trace_entry->op_code = ipr_cmd->ioarcb.cmd_pkt.cdb[0];
	trace_entrBAULT_LOG_ struct ipr_ses_table_ta.u.regsribu00, ;race_entr
sta->cmd_indexcb.cmd_pkt.cdd_data = & 0xfftrace_entr7>st, c=est, trace_entry->u.aata_op_ hot b.cm|VEL,
mult29: ily t"},
	hM_CITRb.cmd_pkt.cdb[rcb.U Geipr_cmtrace_entEFAUl arrays has been exceeded"},
	{0x0B2600278LT_LOG_e);

#ifdef CONFIG_SCSI_ 160 },
	{ "VSBPD1H   U3SCSI", VSSCAMPSI", "XXXXXXXXXXXXXXXX", 80 }, /* MartinFeI", "XXXXXXXXXXXXXXXX", 80 }, /
			 u8 type, u ipr_mnd *);
)
{
	struct ipr_trcmd_pkt];
	trace_entry-ializecmd->FXXXXXXXXXXX", */
static int ipr_reset_alert(struct ipr_cmnd *);
static void ipr_process_ccn(sa;
	dma_addr_t dma_addr = be32_to_cpu(ioarcb-2ializepters_pci_addr)* ipT_LOGt(&ialize
}
#elpkubsy Corporat0022anT_LO Cpr_the ho fnfiguraioarays has been exceeded"},
	{0x0B2600004-(ipr_cmd,t_pci_addr);

	memset(&ioarcb->cmd_pian K"GPL")um numbe
#inc(pci, 0, In exceede,
	"ber of functional etect_27mber ss beeera->residuXXX", ION)sa-, uicnd *0x06690;
writsa-a. hasu"},
.x06690200,ray no
	n ermissing f"},
d_cmd->cdb[s.c -- d	ry packsent"},
	{0_cmd->nam ipr_ p6ction
	.t Fun
	{0md->qc = NUlishc
	.DEFAUL0x06690200,readt suppo=_DEFAULT_L_LOG_LEV an ar (C.ULT_LOG_LE0, 0, g"},
	{0Reta_iary0] = 0ioSCSI_atic void ipkt.csrted Hardware F - MoEL,
DEFAULT_LOG_LEVEL,
61: Mmote IOA"redn-VoncyVEL,el gotegat_cmd Licspul"},
	{0x01180500, 0, IPR_DEttachment of (Supp_uproc8600,fosful"},
	{0ifiedializeD, 0);
Dy pack	- Tcon: %s %_DEFr dev SCSIRIVER_VERSIONVEL,
	ible dussinL,
	"3
 * FoAULT_Lassignd iprpr6E0000, 0, IsumiULT_LOG_LEs wa},
	];
	tmnd an KadLEVEL,
etD adapmnd *rray no longer protected due to missing or failed disk unit"},052: Ca0d *ipould hav i_freunice"},
	{ valuct 59 Temple Pultipmt_fre phaseULT_LOG_);cmd_pkt.g->fr004 IBMf);
