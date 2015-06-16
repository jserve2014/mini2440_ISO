/*
 * pmcraid.c -- driver for PMC Sierra MaxRAID controller adapters
 *
 * Written By: PMC Sierra Corporation
 *
 * Copyright (C) 2008, 2009 PMC Sierra Inc
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,
 * USA
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
#include <linux/hdreg.h>
#include <linux/version.h>
#include <linux/io.h>
#include <asm/irq.h>
#include <asm/processor.h>
#include <linux/libata.h>
#include <linux/mutex.h>
#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsicam.h>

#include "pmcraid.h"

/*
 *   Module configuration parameters
 */
static unsigned int pmcraid_debug_log;
static unsigned int pmcraid_disable_aen;
static unsigned int pmcraid_log_level = IOASC_LOG_LEVEL_MUST;

/*
 * Data structures to support multiple adapters by the LLD.
 * pmcraid_adapter_count - count of configured adapters
 */
static atomic_t pmcraid_adapter_count = ATOMIC_INIT(0);

/*
 * Supporting user-level control interface through IOCTL commands.
 * pmcraid_major - major number to use
 * pmcraid_minor - minor number(s) to use
 */
static unsigned int pmcraid_major;
static struct class *pmcraid_class;
DECLARE_BITMAP(pmcraid_minor, PMCRAID_MAX_ADAPTERS);

/*
 * Module parameters
 */
MODULE_AUTHOR("PMC Sierra Corporation, anil_ravindranath@pmc-sierra.com");
MODULE_DESCRIPTION("PMC Sierra MaxRAID Controller Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(PMCRAID_DRIVER_VERSION);

module_param_named(log_level, pmcraid_log_level, uint, (S_IRUGO | S_IWUSR));
MODULE_PARM_DESC(log_level,
		 "Enables firmware error code logging, default :1 high-severity"
		 " errors, 2: all errors including high-severity errors,"
		 " 0: disables logging");

module_param_named(debug, pmcraid_debug_log, uint, (S_IRUGO | S_IWUSR));
MODULE_PARM_DESC(debug,
		 "Enable driver verbose message logging. Set 1 to enable."
		 "(default: 0)");

module_param_named(disable_aen, pmcraid_disable_aen, uint, (S_IRUGO | S_IWUSR));
MODULE_PARM_DESC(disable_aen,
		 "Disable driver aen notifications to apps. Set 1 to disable."
		 "(default: 0)");

/* chip specific constants for PMC MaxRAID controllers (same for
 * 0x5220 and 0x8010
 */
static struct pmcraid_chip_details pmcraid_chip_cfg[] = {
	{
	 .ioastatus = 0x0,
	 .ioarrin = 0x00040,
	 .mailbox = 0x7FC30,
	 .global_intr_mask = 0x00034,
	 .ioa_host_intr = 0x0009C,
	 .ioa_host_intr_clr = 0x000A0,
	 .ioa_host_mask = 0x7FC28,
	 .ioa_host_mask_clr = 0x7FC28,
	 .host_ioa_intr = 0x00020,
	 .host_ioa_intr_clr = 0x00020,
	 .transop_timeout = 300
	 }
};

/*
 * PCI device ids supported by pmcraid driver
 */
static struct pci_device_id pmcraid_pci_table[] __devinitdata = {
	{ PCI_DEVICE(PCI_VENDOR_ID_PMC, PCI_DEVICE_ID_PMC_MAXRAID),
	  0, 0, (kernel_ulong_t)&pmcraid_chip_cfg[0]
	},
	{}
};

MODULE_DEVICE_TABLE(pci, pmcraid_pci_table);



/**
 * pmcraid_slave_alloc - Prepare for commands to a device
 * @scsi_dev: scsi device struct
 *
 * This function is called by mid-layer prior to sending any command to the new
 * device. Stores resource entry details of the device in scsi_device struct.
 * Queuecommand uses the resource handle and other details to fill up IOARCB
 * while sending commands to the device.
 *
 * Return value:
 *	  0 on success / -ENXIO if device does not exist
 */
static int pmcraid_slave_alloc(struct scsi_device *scsi_dev)
{
	struct pmcraid_resource_entry *temp, *res = NULL;
	struct pmcraid_instance *pinstance;
	u8 target, bus, lun;
	unsigned long lock_flags;
	int rc = -ENXIO;
	pinstance = shost_priv(scsi_dev->host);

	/* Driver exposes VSET and GSCSI resources only; all other device types
	 * are not exposed. Resource list is synchronized using resource lock
	 * so any traversal or modifications to the list should be done inside
	 * this lock
	 */
	spin_lock_irqsave(&pinstance->resource_lock, lock_flags);
	list_for_each_entry(temp, &pinstance->used_res_q, queue) {

		/* do not expose VSETs with order-ids >= 240 */
		if (RES_IS_VSET(temp->cfg_entry)) {
			target = temp->cfg_entry.unique_flags1;
			if (target >= PMCRAID_MAX_VSET_TARGETS)
				continue;
			bus = PMCRAID_VSET_BUS_ID;
			lun = 0;
		} else if (RES_IS_GSCSI(temp->cfg_entry)) {
			target = RES_TARGET(temp->cfg_entry.resource_address);
			bus = PMCRAID_PHYS_BUS_ID;
			lun = RES_LUN(temp->cfg_entry.resource_address);
		} else {
			continue;
		}

		if (bus == scsi_dev->channel &&
		    target == scsi_dev->id &&
		    lun == scsi_dev->lun) {
			res = temp;
			break;
		}
	}

	if (res) {
		res->scsi_dev = scsi_dev;
		scsi_dev->hostdata = res;
		res->change_detected = 0;
		atomic_set(&res->read_failures, 0);
		atomic_set(&res->write_failures, 0);
		rc = 0;
	}
	spin_unlock_irqrestore(&pinstance->resource_lock, lock_flags);
	return rc;
}

/**
 * pmcraid_slave_configure - Configures a SCSI device
 * @scsi_dev: scsi device struct
 *
 * This fucntion is executed by SCSI mid layer just after a device is first
 * scanned (i.e. it has responded to an INQUIRY). For VSET resources, the
 * timeout value (default 30s) will be over-written to a higher value (60s)
 * and max_sectors value will be over-written to 512. It also sets queue depth
 * to host->cmd_per_lun value
 *
 * Return value:
 *	  0 on success
 */
static int pmcraid_slave_configure(struct scsi_device *scsi_dev)
{
	struct pmcraid_resource_entry *res = scsi_dev->hostdata;

	if (!res)
		return 0;

	/* LLD exposes VSETs and Enclosure devices only */
	if (RES_IS_GSCSI(res->cfg_entry) &&
	    scsi_dev->type != TYPE_ENCLOSURE)
		return -ENXIO;

	pmcraid_info("configuring %x:%x:%x:%x\n",
		     scsi_dev->host->unique_id,
		     scsi_dev->channel,
		     scsi_dev->id,
		     scsi_dev->lun);

	if (RES_IS_GSCSI(res->cfg_entry)) {
		scsi_dev->allow_restart = 1;
	} else if (RES_IS_VSET(res->cfg_entry)) {
		scsi_dev->allow_restart = 1;
		blk_queue_rq_timeout(scsi_dev->request_queue,
				     PMCRAID_VSET_IO_TIMEOUT);
		blk_queue_max_sectors(scsi_dev->request_queue,
				      PMCRAID_VSET_MAX_SECTORS);
	}

	if (scsi_dev->tagged_supported &&
	    (RES_IS_GSCSI(res->cfg_entry) || RES_IS_VSET(res->cfg_entry))) {
		scsi_activate_tcq(scsi_dev, scsi_dev->queue_depth);
		scsi_adjust_queue_depth(scsi_dev, MSG_SIMPLE_TAG,
					scsi_dev->host->cmd_per_lun);
	} else {
		scsi_adjust_queue_depth(scsi_dev, 0,
					scsi_dev->host->cmd_per_lun);
	}

	return 0;
}

/**
 * pmcraid_slave_destroy - Unconfigure a SCSI device before removing it
 *
 * @scsi_dev: scsi device struct
 *
 * This is called by mid-layer before removing a device. Pointer assignments
 * done in pmcraid_slave_alloc will be reset to NULL here.
 *
 * Return value
 *   none
 */
static void pmcraid_slave_destroy(struct scsi_device *scsi_dev)
{
	struct pmcraid_resource_entry *res;

	res = (struct pmcraid_resource_entry *)scsi_dev->hostdata;

	if (res)
		res->scsi_dev = NULL;

	scsi_dev->hostdata = NULL;
}

/**
 * pmcraid_change_queue_depth - Change the device's queue depth
 * @scsi_dev: scsi device struct
 * @depth: depth to set
 *
 * Return value
 * 	actual depth set
 */
static int pmcraid_change_queue_depth(struct scsi_device *scsi_dev, int depth)
{
	if (depth > PMCRAID_MAX_CMD_PER_LUN)
		depth = PMCRAID_MAX_CMD_PER_LUN;

	scsi_adjust_queue_depth(scsi_dev, scsi_get_tag_type(scsi_dev), depth);

	return scsi_dev->queue_depth;
}

/**
 * pmcraid_change_queue_type - Change the device's queue type
 * @scsi_dev: scsi device struct
 * @tag: type of tags to use
 *
 * Return value:
 * 	actual queue type set
 */
static int pmcraid_change_queue_type(struct scsi_device *scsi_dev, int tag)
{
	struct pmcraid_resource_entry *res;

	res = (struct pmcraid_resource_entry *)scsi_dev->hostdata;

	if ((res) && scsi_dev->tagged_supported &&
	    (RES_IS_GSCSI(res->cfg_entry) || RES_IS_VSET(res->cfg_entry))) {
		scsi_set_tag_type(scsi_dev, tag);

		if (tag)
			scsi_activate_tcq(scsi_dev, scsi_dev->queue_depth);
		else
			scsi_deactivate_tcq(scsi_dev, scsi_dev->queue_depth);
	} else
		tag = 0;

	return tag;
}


/**
 * pmcraid_init_cmdblk - initializes a command block
 *
 * @cmd: pointer to struct pmcraid_cmd to be initialized
 * @index: if >=0 first time initialization; otherwise reinitialization
 *
 * Return Value
 *	 None
 */
void pmcraid_init_cmdblk(struct pmcraid_cmd *cmd, int index)
{
	struct pmcraid_ioarcb *ioarcb = &(cmd->ioa_cb->ioarcb);
	dma_addr_t dma_addr = cmd->ioa_cb_bus_addr;

	if (index >= 0) {
		/* first time initialization (called from  probe) */
		u32 ioasa_offset =
			offsetof(struct pmcraid_control_block, ioasa);

		cmd->index = index;
		ioarcb->response_handle = cpu_to_le32(index << 2);
		ioarcb->ioarcb_bus_addr = cpu_to_le64(dma_addr);
		ioarcb->ioasa_bus_addr = cpu_to_le64(dma_addr + ioasa_offset);
		ioarcb->ioasa_len = cpu_to_le16(sizeof(struct pmcraid_ioasa));
	} else {
		/* re-initialization of various lengths, called once command is
		 * processed by IOA
		 */
		memset(&cmd->ioa_cb->ioarcb.cdb, 0, PMCRAID_MAX_CDB_LEN);
		ioarcb->request_flags0 = 0;
		ioarcb->request_flags1 = 0;
		ioarcb->cmd_timeout = 0;
		ioarcb->ioarcb_bus_addr &= (~0x1FULL);
		ioarcb->ioadl_bus_addr = 0;
		ioarcb->ioadl_length = 0;
		ioarcb->data_transfer_length = 0;
		ioarcb->add_cmd_param_length = 0;
		ioarcb->add_cmd_param_offset = 0;
		cmd->ioa_cb->ioasa.ioasc = 0;
		cmd->ioa_cb->ioasa.residual_data_length = 0;
		cmd->u.time_left = 0;
	}

	cmd->cmd_done = NULL;
	cmd->scsi_cmd = NULL;
	cmd->release = 0;
	cmd->completion_req = 0;
	cmd->dma_handle = 0;
	init_timer(&cmd->timer);
}

/**
 * pmcraid_reinit_cmdblk - reinitialize a command block
 *
 * @cmd: pointer to struct pmcraid_cmd to be reinitialized
 *
 * Return Value
 *	 None
 */
static void pmcraid_reinit_cmdblk(struct pmcraid_cmd *cmd)
{
	pmcraid_init_cmdblk(cmd, -1);
}

/**
 * pmcraid_get_free_cmd - get a free cmd block from command block pool
 * @pinstance: adapter instance structure
 *
 * Return Value:
 *	returns pointer to cmd block or NULL if no blocks are available
 */
static struct pmcraid_cmd *pmcraid_get_free_cmd(
	struct pmcraid_instance *pinstance
)
{
	struct pmcraid_cmd *cmd = NULL;
	unsigned long lock_flags;

	/* free cmd block list is protected by free_pool_lock */
	spin_lock_irqsave(&pinstance->free_pool_lock, lock_flags);

	if (!list_empty(&pinstance->free_cmd_pool)) {
		cmd = list_entry(pinstance->free_cmd_pool.next,
				 struct pmcraid_cmd, free_list);
		list_del(&cmd->free_list);
	}
	spin_unlock_irqrestore(&pinstance->free_pool_lock, lock_flags);

	/* Initialize the command block before giving it the caller */
	if (cmd != NULL)
		pmcraid_reinit_cmdblk(cmd);
	return cmd;
}

/**
 * pmcraid_return_cmd - return a completed command block back into free pool
 * @cmd: pointer to the command block
 *
 * Return Value:
 *	nothing
 */
void pmcraid_return_cmd(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	unsigned long lock_flags;

	spin_lock_irqsave(&pinstance->free_pool_lock, lock_flags);
	list_add_tail(&cmd->free_list, &pinstance->free_cmd_pool);
	spin_unlock_irqrestore(&pinstance->free_pool_lock, lock_flags);
}

/**
 * pmcraid_read_interrupts -  reads IOA interrupts
 *
 * @pinstance: pointer to adapter instance structure
 *
 * Return value
 *	 interrupts read from IOA
 */
static u32 pmcraid_read_interrupts(struct pmcraid_instance *pinstance)
{
	return ioread32(pinstance->int_regs.ioa_host_interrupt_reg);
}

/**
 * pmcraid_disable_interrupts - Masks and clears all specified interrupts
 *
 * @pinstance: pointer to per adapter instance structure
 * @intrs: interrupts to disable
 *
 * Return Value
 *	 None
 */
static void pmcraid_disable_interrupts(
	struct pmcraid_instance *pinstance,
	u32 intrs
)
{
	u32 gmask = ioread32(pinstance->int_regs.global_interrupt_mask_reg);
	u32 nmask = gmask | GLOBAL_INTERRUPT_MASK;

	iowrite32(nmask, pinstance->int_regs.global_interrupt_mask_reg);
	iowrite32(intrs, pinstance->int_regs.ioa_host_interrupt_clr_reg);
	iowrite32(intrs, pinstance->int_regs.ioa_host_interrupt_mask_reg);
	ioread32(pinstance->int_regs.ioa_host_interrupt_mask_reg);
}

/**
 * pmcraid_enable_interrupts - Enables specified interrupts
 *
 * @pinstance: pointer to per adapter instance structure
 * @intr: interrupts to enable
 *
 * Return Value
 *	 None
 */
static void pmcraid_enable_interrupts(
	struct pmcraid_instance *pinstance,
	u32 intrs
)
{
	u32 gmask = ioread32(pinstance->int_regs.global_interrupt_mask_reg);
	u32 nmask = gmask & (~GLOBAL_INTERRUPT_MASK);

	iowrite32(nmask, pinstance->int_regs.global_interrupt_mask_reg);
	iowrite32(~intrs, pinstance->int_regs.ioa_host_interrupt_mask_reg);
	ioread32(pinstance->int_regs.ioa_host_interrupt_mask_reg);

	pmcraid_info("enabled interrupts global mask = %x intr_mask = %x\n",
		ioread32(pinstance->int_regs.global_interrupt_mask_reg),
		ioread32(pinstance->int_regs.ioa_host_interrupt_mask_reg));
}

/**
 * pmcraid_reset_type - Determine the required reset type
 * @pinstance: pointer to adapter instance structure
 *
 * IOA requires hard reset if any of the following conditions is true.
 * 1. If HRRQ valid interrupt is not masked
 * 2. IOA reset alert doorbell is set
 * 3. If there are any error interrupts
 */
static void pmcraid_reset_type(struct pmcraid_instance *pinstance)
{
	u32 mask;
	u32 intrs;
	u32 alerts;

	mask = ioread32(pinstance->int_regs.ioa_host_interrupt_mask_reg);
	intrs = ioread32(pinstance->int_regs.ioa_host_interrupt_reg);
	alerts = ioread32(pinstance->int_regs.host_ioa_interrupt_reg);

	if ((mask & INTRS_HRRQ_VALID) == 0 ||
	    (alerts & DOORBELL_IOA_RESET_ALERT) ||
	    (intrs & PMCRAID_ERROR_INTERRUPTS)) {
		pmcraid_info("IOA requires hard reset\n");
		pinstance->ioa_hard_reset = 1;
	}

	/* If unit check is active, trigger the dump */
	if (intrs & INTRS_IOA_UNIT_CHECK)
		pinstance->ioa_unit_check = 1;
}

/**
 * pmcraid_bist_done - completion function for PCI BIST
 * @cmd: pointer to reset command
 * Return Value
 * 	none
 */

static void pmcraid_ioa_reset(struct pmcraid_cmd *);

static void pmcraid_bist_done(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	unsigned long lock_flags;
	int rc;
	u16 pci_reg;

	rc = pci_read_config_word(pinstance->pdev, PCI_COMMAND, &pci_reg);

	/* If PCI config space can't be accessed wait for another two secs */
	if ((rc != PCIBIOS_SUCCESSFUL || (!(pci_reg & PCI_COMMAND_MEMORY))) &&
	    cmd->u.time_left > 0) {
		pmcraid_info("BIST not complete, waiting another 2 secs\n");
		cmd->timer.expires = jiffies + cmd->u.time_left;
		cmd->u.time_left = 0;
		cmd->timer.data = (unsigned long)cmd;
		cmd->timer.function =
			(void (*)(unsigned long))pmcraid_bist_done;
		add_timer(&cmd->timer);
	} else {
		cmd->u.time_left = 0;
		pmcraid_info("BIST is complete, proceeding with reset\n");
		spin_lock_irqsave(pinstance->host->host_lock, lock_flags);
		pmcraid_ioa_reset(cmd);
		spin_unlock_irqrestore(pinstance->host->host_lock, lock_flags);
	}
}

/**
 * pmcraid_start_bist - starts BIST
 * @cmd: pointer to reset cmd
 * Return Value
 *   none
 */
static void pmcraid_start_bist(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	u32 doorbells, intrs;

	/* proceed with bist and wait for 2 seconds */
	iowrite32(DOORBELL_IOA_START_BIST,
		pinstance->int_regs.host_ioa_interrupt_reg);
	doorbells = ioread32(pinstance->int_regs.host_ioa_interrupt_reg);
	intrs = ioread32(pinstance->int_regs.ioa_host_interrupt_reg);
	pmcraid_info("doorbells after start bist: %x intrs: %x \n",
		      doorbells, intrs);

	cmd->u.time_left = msecs_to_jiffies(PMCRAID_BIST_TIMEOUT);
	cmd->timer.data = (unsigned long)cmd;
	cmd->timer.expires = jiffies + msecs_to_jiffies(PMCRAID_BIST_TIMEOUT);
	cmd->timer.function = (void (*)(unsigned long))pmcraid_bist_done;
	add_timer(&cmd->timer);
}

/**
 * pmcraid_reset_alert_done - completion routine for reset_alert
 * @cmd: pointer to command block used in reset sequence
 * Return value
 *  None
 */
static void pmcraid_reset_alert_done(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	u32 status = ioread32(pinstance->ioa_status);
	unsigned long lock_flags;

	/* if the critical operation in progress bit is set or the wait times
	 * out, invoke reset engine to proceed with hard reset. If there is
	 * some more time to wait, restart the timer
	 */
	if (((status & INTRS_CRITICAL_OP_IN_PROGRESS) == 0) ||
	    cmd->u.time_left <= 0) {
		pmcraid_info("critical op is reset proceeding with reset\n");
		spin_lock_irqsave(pinstance->host->host_lock, lock_flags);
		pmcraid_ioa_reset(cmd);
		spin_unlock_irqrestore(pinstance->host->host_lock, lock_flags);
	} else {
		pmcraid_info("critical op is not yet reset waiting again\n");
		/* restart timer if some more time is available to wait */
		cmd->u.time_left -= PMCRAID_CHECK_FOR_RESET_TIMEOUT;
		cmd->timer.data = (unsigned long)cmd;
		cmd->timer.expires = jiffies + PMCRAID_CHECK_FOR_RESET_TIMEOUT;
		cmd->timer.function =
			(void (*)(unsigned long))pmcraid_reset_alert_done;
		add_timer(&cmd->timer);
	}
}

/**
 * pmcraid_reset_alert - alerts IOA for a possible reset
 * @cmd : command block to be used for reset sequence.
 *
 * Return Value
 *	returns 0 if pci config-space is accessible and RESET_DOORBELL is
 *	successfully written to IOA. Returns non-zero in case pci_config_space
 *	is not accessible
 */
static void pmcraid_reset_alert(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	u32 doorbells;
	int rc;
	u16 pci_reg;

	/* If we are able to access IOA PCI config space, alert IOA that we are
	 * going to reset it soon. This enables IOA to preserv persistent error
	 * data if any. In case memory space is not accessible, proceed with
	 * BIST or slot_reset
	 */
	rc = pci_read_config_word(pinstance->pdev, PCI_COMMAND, &pci_reg);
	if ((rc == PCIBIOS_SUCCESSFUL) && (pci_reg & PCI_COMMAND_MEMORY)) {

		/* wait for IOA permission i.e until CRITICAL_OPERATION bit is
		 * reset IOA doesn't generate any interrupts when CRITICAL
		 * OPERATION bit is reset. A timer is started to wait for this
		 * bit to be reset.
		 */
		cmd->u.time_left = PMCRAID_RESET_TIMEOUT;
		cmd->timer.data = (unsigned long)cmd;
		cmd->timer.expires = jiffies + PMCRAID_CHECK_FOR_RESET_TIMEOUT;
		cmd->timer.function =
			(void (*)(unsigned long))pmcraid_reset_alert_done;
		add_timer(&cmd->timer);

		iowrite32(DOORBELL_IOA_RESET_ALERT,
			pinstance->int_regs.host_ioa_interrupt_reg);
		doorbells =
			ioread32(pinstance->int_regs.host_ioa_interrupt_reg);
		pmcraid_info("doorbells after reset alert: %x\n", doorbells);
	} else {
		pmcraid_info("PCI config is not accessible starting BIST\n");
		pinstance->ioa_state = IOA_STATE_IN_HARD_RESET;
		pmcraid_start_bist(cmd);
	}
}

/**
 * pmcraid_timeout_handler -  Timeout handler for internally generated ops
 *
 * @cmd : pointer to command structure, that got timedout
 *
 * This function blocks host requests and initiates an adapter reset.
 *
 * Return value:
 *   None
 */
static void pmcraid_timeout_handler(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	unsigned long lock_flags;

	dev_info(&pinstance->pdev->dev,
		"Adapter being reset due to command timeout.\n");

	/* Command timeouts result in hard reset sequence. The command that got
	 * timed out may be the one used as part of reset sequence. In this
	 * case restart reset sequence using the same command block even if
	 * reset is in progress. Otherwise fail this command and get a free
	 * command block to restart the reset sequence.
	 */
	spin_lock_irqsave(pinstance->host->host_lock, lock_flags);
	if (!pinstance->ioa_reset_in_progress) {
		pinstance->ioa_reset_attempts = 0;
		cmd = pmcraid_get_free_cmd(pinstance);

		/* If we are out of command blocks, just return here itself.
		 * Some other command's timeout handler can do the reset job
		 */
		if (cmd == NULL) {
			spin_unlock_irqrestore(pinstance->host->host_lock,
					       lock_flags);
			pmcraid_err("no free cmnd block for timeout handler\n");
			return;
		}

		pinstance->reset_cmd = cmd;
		pinstance->ioa_reset_in_progress = 1;
	} else {
		pmcraid_info("reset is already in progress\n");

		if (pinstance->reset_cmd != cmd) {
			/* This command should have been given to IOA, this
			 * command will be completed by fail_outstanding_cmds
			 * anyway
			 */
			pmcraid_err("cmd is pending but reset in progress\n");
		}

		/* If this command was being used as part of the reset
		 * sequence, set cmd_done pointer to pmcraid_ioa_reset. This
		 * causes fail_outstanding_commands not to return the command
		 * block back to free pool
		 */
		if (cmd == pinstance->reset_cmd)
			cmd->cmd_done = pmcraid_ioa_reset;

	}

	pinstance->ioa_state = IOA_STATE_IN_RESET_ALERT;
	scsi_block_requests(pinstance->host);
	pmcraid_reset_alert(cmd);
	spin_unlock_irqrestore(pinstance->host->host_lock, lock_flags);
}

/**
 * pmcraid_internal_done - completion routine for internally generated cmds
 *
 * @cmd: command that got response from IOA
 *
 * Return Value:
 *	 none
 */
static void pmcraid_internal_done(struct pmcraid_cmd *cmd)
{
	pmcraid_info("response internal cmd CDB[0] = %x ioasc = %x\n",
		     cmd->ioa_cb->ioarcb.cdb[0],
		     le32_to_cpu(cmd->ioa_cb->ioasa.ioasc));

	/* Some of the internal commands are sent with callers blocking for the
	 * response. Same will be indicated as part of cmd->completion_req
	 * field. Response path needs to wake up any waiters waiting for cmd
	 * completion if this flag is set.
	 */
	if (cmd->completion_req) {
		cmd->completion_req = 0;
		complete(&cmd->wait_for_completion);
	}

	/* most of the internal commands are completed by caller itself, so
	 * no need to return the command block back to free pool until we are
	 * required to do so (e.g once done with initialization).
	 */
	if (cmd->release) {
		cmd->release = 0;
		pmcraid_return_cmd(cmd);
	}
}

/**
 * pmcraid_reinit_cfgtable_done - done function for cfg table reinitialization
 *
 * @cmd: command that got response from IOA
 *
 * This routine is called after driver re-reads configuration table due to a
 * lost CCN. It returns the command block back to free pool and schedules
 * worker thread to add/delete devices into the system.
 *
 * Return Value:
 *	 none
 */
static void pmcraid_reinit_cfgtable_done(struct pmcraid_cmd *cmd)
{
	pmcraid_info("response internal cmd CDB[0] = %x ioasc = %x\n",
		     cmd->ioa_cb->ioarcb.cdb[0],
		     le32_to_cpu(cmd->ioa_cb->ioasa.ioasc));

	if (cmd->release) {
		cmd->release = 0;
		pmcraid_return_cmd(cmd);
	}
	pmcraid_info("scheduling worker for config table reinitialization\n");
	schedule_work(&cmd->drv_inst->worker_q);
}

/**
 * pmcraid_erp_done - Process completion of SCSI error response from device
 * @cmd: pmcraid_command
 *
 * This function copies the sense buffer into the scsi_cmd struct and completes
 * scsi_cmd by calling scsi_done function.
 *
 * Return value:
 *  none
 */
static void pmcraid_erp_done(struct pmcraid_cmd *cmd)
{
	struct scsi_cmnd *scsi_cmd = cmd->scsi_cmd;
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	u32 ioasc = le32_to_cpu(cmd->ioa_cb->ioasa.ioasc);

	if (PMCRAID_IOASC_SENSE_KEY(ioasc) > 0) {
		scsi_cmd->result |= (DID_ERROR << 16);
		scmd_printk(KERN_INFO, scsi_cmd,
			    "command CDB[0] = %x failed with IOASC: 0x%08X\n",
			    cmd->ioa_cb->ioarcb.cdb[0], ioasc);
	}

	/* if we had allocated sense buffers for request sense, copy the sense
	 * release the buffers
	 */
	if (cmd->sense_buffer != NULL) {
		memcpy(scsi_cmd->sense_buffer,
		       cmd->sense_buffer,
		       SCSI_SENSE_BUFFERSIZE);
		pci_free_consistent(pinstance->pdev,
				    SCSI_SENSE_BUFFERSIZE,
				    cmd->sense_buffer, cmd->sense_buffer_dma);
		cmd->sense_buffer = NULL;
		cmd->sense_buffer_dma = 0;
	}

	scsi_dma_unmap(scsi_cmd);
	pmcraid_return_cmd(cmd);
	scsi_cmd->scsi_done(scsi_cmd);
}

/**
 * pmcraid_fire_command - sends an IOA command to adapter
 *
 * This function adds the given block into pending command list
 * and returns without waiting
 *
 * @cmd : command to be sent to the device
 *
 * Return Value
 *	None
 */
static void _pmcraid_fire_command(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	unsigned long lock_flags;

	/* Add this command block to pending cmd pool. We do this prior to
	 * writting IOARCB to ioarrin because IOA might complete the command
	 * by the time we are about to add it to the list. Response handler
	 * (isr/tasklet) looks for cmb block in the pending pending list.
	 */
	spin_lock_irqsave(&pinstance->pending_pool_lock, lock_flags);
	list_add_tail(&cmd->free_list, &pinstance->pending_cmd_pool);
	spin_unlock_irqrestore(&pinstance->pending_pool_lock, lock_flags);
	atomic_inc(&pinstance->outstanding_cmds);

	/* driver writes lower 32-bit value of IOARCB address only */
	mb();
	iowrite32(le32_to_cpu(cmd->ioa_cb->ioarcb.ioarcb_bus_addr),
		  pinstance->ioarrin);
}

/**
 * pmcraid_send_cmd - fires a command to IOA
 *
 * This function also sets up timeout function, and command completion
 * function
 *
 * @cmd: pointer to the command block to be fired to IOA
 * @cmd_done: command completion function, called once IOA responds
 * @timeout: timeout to wait for this command completion
 * @timeout_func: timeout handler
 *
 * Return value
 *   none
 */
static void pmcraid_send_cmd(
	struct pmcraid_cmd *cmd,
	void (*cmd_done) (struct pmcraid_cmd *),
	unsigned long timeout,
	void (*timeout_func) (struct pmcraid_cmd *)
)
{
	/* initialize done function */
	cmd->cmd_done = cmd_done;

	if (timeout_func) {
		/* setup timeout handler */
		cmd->timer.data = (unsigned long)cmd;
		cmd->timer.expires = jiffies + timeout;
		cmd->timer.function = (void (*)(unsigned long))timeout_func;
		add_timer(&cmd->timer);
	}

	/* fire the command to IOA */
	_pmcraid_fire_command(cmd);
}

/**
 * pmcraid_ioa_shutdown - sends SHUTDOWN command to ioa
 *
 * @cmd: pointer to the command block used as part of reset sequence
 *
 * Return Value
 *  None
 */
static void pmcraid_ioa_shutdown(struct pmcraid_cmd *cmd)
{
	pmcraid_info("response for Cancel CCN CDB[0] = %x ioasc = %x\n",
		     cmd->ioa_cb->ioarcb.cdb[0],
		     le32_to_cpu(cmd->ioa_cb->ioasa.ioasc));

	/* Note that commands sent during reset require next command to be sent
	 * to IOA. Hence reinit the done function as well as timeout function
	 */
	pmcraid_reinit_cmdblk(cmd);
	cmd->ioa_cb->ioarcb.request_type = REQ_TYPE_IOACMD;
	cmd->ioa_cb->ioarcb.resource_handle =
		cpu_to_le32(PMCRAID_IOA_RES_HANDLE);
	cmd->ioa_cb->ioarcb.cdb[0] = PMCRAID_IOA_SHUTDOWN;
	cmd->ioa_cb->ioarcb.cdb[1] = PMCRAID_SHUTDOWN_NORMAL;

	/* fire shutdown command to hardware. */
	pmcraid_info("firing normal shutdown command (%d) to IOA\n",
		     le32_to_cpu(cmd->ioa_cb->ioarcb.response_handle));

	pmcraid_send_cmd(cmd, pmcraid_ioa_reset,
			 PMCRAID_SHUTDOWN_TIMEOUT,
			 pmcraid_timeout_handler);
}

/**
 * pmcraid_identify_hrrq - registers host rrq buffers with IOA
 * @cmd: pointer to command block to be used for identify hrrq
 *
 * Return Value
 *	 0 in case of success, otherwise non-zero failure code
 */

static void pmcraid_querycfg(struct pmcraid_cmd *);

static void pmcraid_identify_hrrq(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	struct pmcraid_ioarcb *ioarcb = &cmd->ioa_cb->ioarcb;
	int index = 0;
	__be64 hrrq_addr = cpu_to_be64(pinstance->hrrq_start_bus_addr[index]);
	u32 hrrq_size = cpu_to_be32(sizeof(u32) * PMCRAID_MAX_CMD);

	pmcraid_reinit_cmdblk(cmd);

	/* Initialize ioarcb */
	ioarcb->request_type = REQ_TYPE_IOACMD;
	ioarcb->resource_handle = cpu_to_le32(PMCRAID_IOA_RES_HANDLE);

	/* initialize the hrrq number where IOA will respond to this command */
	ioarcb->hrrq_id = index;
	ioarcb->cdb[0] = PMCRAID_IDENTIFY_HRRQ;
	ioarcb->cdb[1] = index;

	/* IOA expects 64-bit pci address to be written in B.E format
	 * (i.e cdb[2]=MSByte..cdb[9]=LSB.
	 */
	pmcraid_info("HRRQ_IDENTIFY with hrrq:ioarcb => %llx:%llx\n",
		     hrrq_addr, ioarcb->ioarcb_bus_addr);

	memcpy(&(ioarcb->cdb[2]), &hrrq_addr, sizeof(hrrq_addr));
	memcpy(&(ioarcb->cdb[10]), &hrrq_size, sizeof(hrrq_size));

	/* Subsequent commands require HRRQ identification to be successful.
	 * Note that this gets called even during reset from SCSI mid-layer
	 * or tasklet
	 */
	pmcraid_send_cmd(cmd, pmcraid_querycfg,
			 PMCRAID_INTERNAL_TIMEOUT,
			 pmcraid_timeout_handler);
}

static void pmcraid_process_ccn(struct pmcraid_cmd *cmd);
static void pmcraid_process_ldn(struct pmcraid_cmd *cmd);

/**
 * pmcraid_send_hcam_cmd - send an initialized command block(HCAM) to IOA
 *
 * @cmd: initialized command block pointer
 *
 * Return Value
 *   none
 */
static void pmcraid_send_hcam_cmd(struct pmcraid_cmd *cmd)
{
	if (cmd->ioa_cb->ioarcb.cdb[1] == PMCRAID_HCAM_CODE_CONFIG_CHANGE)
		atomic_set(&(cmd->drv_inst->ccn.ignore), 0);
	else
		atomic_set(&(cmd->drv_inst->ldn.ignore), 0);

	pmcraid_send_cmd(cmd, cmd->cmd_done, 0, NULL);
}

/**
 * pmcraid_init_hcam - send an initialized command block(HCAM) to IOA
 *
 * @pinstance: pointer to adapter instance structure
 * @type: HCAM type
 *
 * Return Value
 *   pointer to initialized pmcraid_cmd structure or NULL
 */
static struct pmcraid_cmd *pmcraid_init_hcam
(
	struct pmcraid_instance *pinstance,
	u8 type
)
{
	struct pmcraid_cmd *cmd;
	struct pmcraid_ioarcb *ioarcb;
	struct pmcraid_ioadl_desc *ioadl;
	struct pmcraid_hostrcb *hcam;
	void (*cmd_done) (struct pmcraid_cmd *);
	dma_addr_t dma;
	int rcb_size;

	cmd = pmcraid_get_free_cmd(pinstance);

	if (!cmd) {
		pmcraid_err("no free command blocks for hcam\n");
		return cmd;
	}

	if (type == PMCRAID_HCAM_CODE_CONFIG_CHANGE) {
		rcb_size = sizeof(struct pmcraid_hcam_ccn);
		cmd_done = pmcraid_process_ccn;
		dma = pinstance->ccn.baddr + PMCRAID_AEN_HDR_SIZE;
		hcam = &pinstance->ccn;
	} else {
		rcb_size = sizeof(struct pmcraid_hcam_ldn);
		cmd_done = pmcraid_process_ldn;
		dma = pinstance->ldn.baddr + PMCRAID_AEN_HDR_SIZE;
		hcam = &pinstance->ldn;
	}

	/* initialize command pointer used for HCAM registration */
	hcam->cmd = cmd;

	ioarcb = &cmd->ioa_cb->ioarcb;
	ioarcb->ioadl_bus_addr = cpu_to_le64((cmd->ioa_cb_bus_addr) +
					offsetof(struct pmcraid_ioarcb,
						add_data.u.ioadl[0]));
	ioarcb->ioadl_length = cpu_to_le32(sizeof(struct pmcraid_ioadl_desc));
	ioadl = ioarcb->add_data.u.ioadl;

	/* Initialize ioarcb */
	ioarcb->request_type = REQ_TYPE_HCAM;
	ioarcb->resource_handle = cpu_to_le32(PMCRAID_IOA_RES_HANDLE);
	ioarcb->cdb[0] = PMCRAID_HOST_CONTROLLED_ASYNC;
	ioarcb->cdb[1] = type;
	ioarcb->cdb[7] = (rcb_size >> 8) & 0xFF;
	ioarcb->cdb[8] = (rcb_size) & 0xFF;

	ioarcb->data_transfer_length = cpu_to_le32(rcb_size);

	ioadl[0].flags |= IOADL_FLAGS_READ_LAST;
	ioadl[0].data_len = cpu_to_le32(rcb_size);
	ioadl[0].address = cpu_to_le32(dma);

	cmd->cmd_done = cmd_done;
	return cmd;
}

/**
 * pmcraid_send_hcam - Send an HCAM to IOA
 * @pinstance: ioa config struct
 * @type: HCAM type
 *
 * This function will send a Host Controlled Async command to IOA.
 *
 * Return value:
 * 	none
 */
static void pmcraid_send_hcam(struct pmcraid_instance *pinstance, u8 type)
{
	struct pmcraid_cmd *cmd = pmcraid_init_hcam(pinstance, type);
	pmcraid_send_hcam_cmd(cmd);
}


/**
 * pmcraid_prepare_cancel_cmd - prepares a command block to abort another
 *
 * @cmd: pointer to cmd that is used as cancelling command
 * @cmd_to_cancel: pointer to the command that needs to be cancelled
 */
static void pmcraid_prepare_cancel_cmd(
	struct pmcraid_cmd *cmd,
	struct pmcraid_cmd *cmd_to_cancel
)
{
	struct pmcraid_ioarcb *ioarcb = &cmd->ioa_cb->ioarcb;
	__be64 ioarcb_addr = cmd_to_cancel->ioa_cb->ioarcb.ioarcb_bus_addr;

	/* Get the resource handle to where the command to be aborted has been
	 * sent.
	 */
	ioarcb->resource_handle = cmd_to_cancel->ioa_cb->ioarcb.resource_handle;
	ioarcb->request_type = REQ_TYPE_IOACMD;
	memset(ioarcb->cdb, 0, PMCRAID_MAX_CDB_LEN);
	ioarcb->cdb[0] = PMCRAID_ABORT_CMD;

	/* IOARCB address of the command to be cancelled is given in
	 * cdb[2]..cdb[9] is Big-Endian format. Note that length bits in
	 * IOARCB address are not masked.
	 */
	ioarcb_addr = cpu_to_be64(ioarcb_addr);
	memcpy(&(ioarcb->cdb[2]), &ioarcb_addr, sizeof(ioarcb_addr));
}

/**
 * pmcraid_cancel_hcam - sends ABORT task to abort a given HCAM
 *
 * @cmd: command to be used as cancelling command
 * @type: HCAM type
 * @cmd_done: op done function for the cancelling command
 */
static void pmcraid_cancel_hcam(
	struct pmcraid_cmd *cmd,
	u8 type,
	void (*cmd_done) (struct pmcraid_cmd *)
)
{
	struct pmcraid_instance *pinstance;
	struct pmcraid_hostrcb  *hcam;

	pinstance = cmd->drv_inst;
	hcam =  (type == PMCRAID_HCAM_CODE_LOG_DATA) ?
		&pinstance->ldn : &pinstance->ccn;

	/* prepare for cancelling previous hcam command. If the HCAM is
	 * currently not pending with IOA, we would have hcam->cmd as non-null
	 */
	if (hcam->cmd == NULL)
		return;

	pmcraid_prepare_cancel_cmd(cmd, hcam->cmd);

	/* writing to IOARRIN must be protected by host_lock, as mid-layer
	 * schedule queuecommand while we are doing this
	 */
	pmcraid_send_cmd(cmd, cmd_done,
			 PMCRAID_INTERNAL_TIMEOUT,
			 pmcraid_timeout_handler);
}

/**
 * pmcraid_cancel_ccn - cancel CCN HCAM already registered with IOA
 *
 * @cmd: command block to be used for cancelling the HCAM
 */
static void pmcraid_cancel_ccn(struct pmcraid_cmd *cmd)
{
	pmcraid_info("response for Cancel LDN CDB[0] = %x ioasc = %x\n",
		     cmd->ioa_cb->ioarcb.cdb[0],
		     le32_to_cpu(cmd->ioa_cb->ioasa.ioasc));

	pmcraid_reinit_cmdblk(cmd);

	pmcraid_cancel_hcam(cmd,
			    PMCRAID_HCAM_CODE_CONFIG_CHANGE,
			    pmcraid_ioa_shutdown);
}

/**
 * pmcraid_cancel_ldn - cancel LDN HCAM already registered with IOA
 *
 * @cmd: command block to be used for cancelling the HCAM
 */
static void pmcraid_cancel_ldn(struct pmcraid_cmd *cmd)
{
	pmcraid_cancel_hcam(cmd,
			    PMCRAID_HCAM_CODE_LOG_DATA,
			    pmcraid_cancel_ccn);
}

/**
 * pmcraid_expose_resource - check if the resource can be exposed to OS
 *
 * @cfgte: pointer to configuration table entry of the resource
 *
 * Return value:
 * 	true if resource can be added to midlayer, false(0) otherwise
 */
static int pmcraid_expose_resource(struct pmcraid_config_table_entry *cfgte)
{
	int retval = 0;

	if (cfgte->resource_type == RES_TYPE_VSET)
		retval = ((cfgte->unique_flags1 & 0xFF) < 0xFE);
	else if (cfgte->resource_type == RES_TYPE_GSCSI)
		retval = (RES_BUS(cfgte->resource_address) !=
				PMCRAID_VIRTUAL_ENCL_BUS_ID);
	return retval;
}

/* attributes supported by pmcraid_event_family */
enum {
	PMCRAID_AEN_ATTR_UNSPEC,
	PMCRAID_AEN_ATTR_EVENT,
	__PMCRAID_AEN_ATTR_MAX,
};
#define PMCRAID_AEN_ATTR_MAX (__PMCRAID_AEN_ATTR_MAX - 1)

/* commands supported by pmcraid_event_family */
enum {
	PMCRAID_AEN_CMD_UNSPEC,
	PMCRAID_AEN_CMD_EVENT,
	__PMCRAID_AEN_CMD_MAX,
};
#define PMCRAID_AEN_CMD_MAX (__PMCRAID_AEN_CMD_MAX - 1)

static struct genl_family pmcraid_event_family = {
	.id = GENL_ID_GENERATE,
	.name = "pmcraid",
	.version = 1,
	.maxattr = PMCRAID_AEN_ATTR_MAX
};

/**
 * pmcraid_netlink_init - registers pmcraid_event_family
 *
 * Return value:
 * 	0 if the pmcraid_event_family is successfully registered
 * 	with netlink generic, non-zero otherwise
 */
static int pmcraid_netlink_init(void)
{
	int result;

	result = genl_register_family(&pmcraid_event_family);

	if (result)
		return result;

	pmcraid_info("registered NETLINK GENERIC group: %d\n",
		     pmcraid_event_family.id);

	return result;
}

/**
 * pmcraid_netlink_release - unregisters pmcraid_event_family
 *
 * Return value:
 * 	none
 */
static void pmcraid_netlink_release(void)
{
	genl_unregister_family(&pmcraid_event_family);
}

/**
 * pmcraid_notify_aen - sends event msg to user space application
 * @pinstance: pointer to adapter instance structure
 * @type: HCAM type
 *
 * Return value:
 *	0 if success, error value in case of any failure.
 */
static int pmcraid_notify_aen(struct pmcraid_instance *pinstance, u8 type)
{
	struct sk_buff *skb;
	struct pmcraid_aen_msg *aen_msg;
	void *msg_header;
	int data_size, total_size;
	int result;


	if (type == PMCRAID_HCAM_CODE_LOG_DATA) {
		aen_msg = pinstance->ldn.msg;
		data_size = pinstance->ldn.hcam->data_len;
	} else {
		aen_msg = pinstance->ccn.msg;
		data_size = pinstance->ccn.hcam->data_len;
	}

	data_size += sizeof(struct pmcraid_hcam_hdr);
	aen_msg->hostno = (pinstance->host->unique_id << 16 |
			   MINOR(pinstance->cdev.dev));
	aen_msg->length = data_size;
	data_size += sizeof(*aen_msg);

	total_size = nla_total_size(data_size);
	skb = genlmsg_new(total_size, GFP_ATOMIC);


	if (!skb) {
		pmcraid_err("Failed to allocate aen data SKB of size: %x\n",
			     total_size);
		return -ENOMEM;
	}

	/* add the genetlink message header */
	msg_header = genlmsg_put(skb, 0, 0,
				 &pmcraid_event_family, 0,
				 PMCRAID_AEN_CMD_EVENT);
	if (!msg_header) {
		pmcraid_err("failed to copy command details\n");
		nlmsg_free(skb);
		return -ENOMEM;
	}

	result = nla_put(skb, PMCRAID_AEN_ATTR_EVENT, data_size, aen_msg);

	if (result) {
		pmcraid_err("failed to copy AEN attribute data \n");
		nlmsg_free(skb);
		return -EINVAL;
	}

	/* send genetlink multicast message to notify appplications */
	result = genlmsg_end(skb, msg_header);

	if (result < 0) {
		pmcraid_err("genlmsg_end failed\n");
		nlmsg_free(skb);
		return result;
	}

	result =
		genlmsg_multicast(skb, 0, pmcraid_event_family.id, GFP_ATOMIC);

	/* If there are no listeners, genlmsg_multicast may return non-zero
	 * value.
	 */
	if (result)
		pmcraid_info("failed to send %s event message %x!\n",
			type == PMCRAID_HCAM_CODE_LOG_DATA ? "LDN" : "CCN",
			result);
	return result;
}

/**
 * pmcraid_handle_config_change - Handle a config change from the adapter
 * @pinstance: pointer to per adapter instance structure
 *
 * Return value:
 *  none
 */
static void pmcraid_handle_config_change(struct pmcraid_instance *pinstance)
{
	struct pmcraid_config_table_entry *cfg_entry;
	struct pmcraid_hcam_ccn *ccn_hcam;
	struct pmcraid_cmd *cmd;
	struct pmcraid_cmd *cfgcmd;
	struct pmcraid_resource_entry *res = NULL;
	u32 new_entry = 1;
	unsigned long lock_flags;
	unsigned long host_lock_flags;
	int rc;

	ccn_hcam = (struct pmcraid_hcam_ccn *)pinstance->ccn.hcam;
	cfg_entry = &ccn_hcam->cfg_entry;

	pmcraid_info
		("CCN(%x): %x type: %x lost: %x flags: %x res: %x:%x:%x:%x\n",
		 pinstance->ccn.hcam->ilid,
		 pinstance->ccn.hcam->op_code,
		 pinstance->ccn.hcam->notification_type,
		 pinstance->ccn.hcam->notification_lost,
		 pinstance->ccn.hcam->flags,
		 pinstance->host->unique_id,
		 RES_IS_VSET(*cfg_entry) ? PMCRAID_VSET_BUS_ID :
		 (RES_IS_GSCSI(*cfg_entry) ? PMCRAID_PHYS_BUS_ID :
			RES_BUS(cfg_entry->resource_address)),
		 RES_IS_VSET(*cfg_entry) ? cfg_entry->unique_flags1 :
			RES_TARGET(cfg_entry->resource_address),
		 RES_LUN(cfg_entry->resource_address));


	/* If this HCAM indicates a lost notification, read the config table */
	if (pinstance->ccn.hcam->notification_lost) {
		cfgcmd = pmcraid_get_free_cmd(pinstance);
		if (cfgcmd) {
			pmcraid_info("lost CCN, reading config table\b");
			pinstance->reinit_cfg_table = 1;
			pmcraid_querycfg(cfgcmd);
		} else {
			pmcraid_err("lost CCN, no free cmd for querycfg\n");
		}
		goto out_notify_apps;
	}

	/* If this resource is not going to be added to mid-layer, just notify
	 * applications and return
	 */
	if (!pmcraid_expose_resource(cfg_entry))
		goto out_notify_apps;

	spin_lock_irqsave(&pinstance->resource_lock, lock_flags);
	list_for_each_entry(res, &pinstance->used_res_q, queue) {
		rc = memcmp(&res->cfg_entry.resource_address,
			    &cfg_entry->resource_address,
			    sizeof(cfg_entry->resource_address));
		if (!rc) {
			new_entry = 0;
			break;
		}
	}

	if (new_entry) {

		/* If there are more number of resources than what driver can
		 * manage, do not notify the applications about the CCN. Just
		 * ignore this notifications and re-register the same HCAM
		 */
		if (list_empty(&pinstance->free_res_q)) {
			spin_unlock_irqrestore(&pinstance->resource_lock,
						lock_flags);
			pmcraid_err("too many resources attached\n");
			spin_lock_irqsave(pinstance->host->host_lock,
					  host_lock_flags);
			pmcraid_send_hcam(pinstance,
					  PMCRAID_HCAM_CODE_CONFIG_CHANGE);
			spin_unlock_irqrestore(pinstance->host->host_lock,
					       host_lock_flags);
			return;
		}

		res = list_entry(pinstance->free_res_q.next,
				 struct pmcraid_resource_entry, queue);

		list_del(&res->queue);
		res->scsi_dev = NULL;
		res->reset_progress = 0;
		list_add_tail(&res->queue, &pinstance->used_res_q);
	}

	memcpy(&res->cfg_entry, cfg_entry,
		sizeof(struct pmcraid_config_table_entry));

	if (pinstance->ccn.hcam->notification_type ==
	    NOTIFICATION_TYPE_ENTRY_DELETED) {
		if (res->scsi_dev) {
			res->change_detected = RES_CHANGE_DEL;
			res->cfg_entry.resource_handle =
				PMCRAID_INVALID_RES_HANDLE;
			schedule_work(&pinstance->worker_q);
		} else {
			/* This may be one of the non-exposed resources */
			list_move_tail(&res->queue, &pinstance->free_res_q);
		}
	} else if (!res->scsi_dev) {
		res->change_detected = RES_CHANGE_ADD;
		schedule_work(&pinstance->worker_q);
	}
	spin_unlock_irqrestore(&pinstance->resource_lock, lock_flags);

out_notify_apps:

	/* Notify configuration changes to registered applications.*/
	if (!pmcraid_disable_aen)
		pmcraid_notify_aen(pinstance, PMCRAID_HCAM_CODE_CONFIG_CHANGE);

	cmd = pmcraid_init_hcam(pinstance, PMCRAID_HCAM_CODE_CONFIG_CHANGE);
	if (cmd)
		pmcraid_send_hcam_cmd(cmd);
}

/**
 * pmcraid_get_error_info - return error string for an ioasc
 * @ioasc: ioasc code
 * Return Value
 *	 none
 */
static struct pmcraid_ioasc_error *pmcraid_get_error_info(u32 ioasc)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(pmcraid_ioasc_error_table); i++) {
		if (pmcraid_ioasc_error_table[i].ioasc_code == ioasc)
			return &pmcraid_ioasc_error_table[i];
	}
	return NULL;
}

/**
 * pmcraid_ioasc_logger - log IOASC information based user-settings
 * @ioasc: ioasc code
 * @cmd: pointer to command that resulted in 'ioasc'
 */
void pmcraid_ioasc_logger(u32 ioasc, struct pmcraid_cmd *cmd)
{
	struct pmcraid_ioasc_error *error_info = pmcraid_get_error_info(ioasc);

	if (error_info == NULL ||
		cmd->drv_inst->current_log_level < error_info->log_level)
		return;

	/* log the error string */
	pmcraid_err("cmd [%d] for resource %x failed with %x(%s)\n",
		cmd->ioa_cb->ioarcb.cdb[0],
		cmd->ioa_cb->ioarcb.resource_handle,
		le32_to_cpu(ioasc), error_info->error_string);
}

/**
 * pmcraid_handle_error_log - Handle a config change (error log) from the IOA
 *
 * @pinstance: pointer to per adapter instance structure
 *
 * Return value:
 *  none
 */
static void pmcraid_handle_error_log(struct pmcraid_instance *pinstance)
{
	struct pmcraid_hcam_ldn *hcam_ldn;
	u32 ioasc;

	hcam_ldn = (struct pmcraid_hcam_ldn *)pinstance->ldn.hcam;

	pmcraid_info
		("LDN(%x): %x type: %x lost: %x flags: %x overlay id: %x\n",
		 pinstance->ldn.hcam->ilid,
		 pinstance->ldn.hcam->op_code,
		 pinstance->ldn.hcam->notification_type,
		 pinstance->ldn.hcam->notification_lost,
		 pinstance->ldn.hcam->flags,
		 pinstance->ldn.hcam->overlay_id);

	/* log only the errors, no need to log informational log entries */
	if (pinstance->ldn.hcam->notification_type !=
	    NOTIFICATION_TYPE_ERROR_LOG)
		return;

	if (pinstance->ldn.hcam->notification_lost ==
	    HOSTRCB_NOTIFICATIONS_LOST)
		dev_info(&pinstance->pdev->dev, "Error notifications lost\n");

	ioasc = le32_to_cpu(hcam_ldn->error_log.fd_ioasc);

	if (ioasc == PMCRAID_IOASC_UA_BUS_WAS_RESET ||
		ioasc == PMCRAID_IOASC_UA_BUS_WAS_RESET_BY_OTHER) {
		dev_info(&pinstance->pdev->dev,
			"UnitAttention due to IOA Bus Reset\n");
		scsi_report_bus_reset(
			pinstance->host,
			RES_BUS(hcam_ldn->error_log.fd_ra));
	}

	return;
}

/**
 * pmcraid_process_ccn - Op done function for a CCN.
 * @cmd: pointer to command struct
 *
 * This function is the op done function for a configuration
 * change notification
 *
 * Return value:
 * none
 */
static void pmcraid_process_ccn(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	u32 ioasc = le32_to_cpu(cmd->ioa_cb->ioasa.ioasc);
	unsigned long lock_flags;

	pinstance->ccn.cmd = NULL;
	pmcraid_return_cmd(cmd);

	/* If driver initiated IOA reset happened while this hcam was pending
	 * with IOA, or IOA bringdown sequence is in progress, no need to
	 * re-register the hcam
	 */
	if (ioasc == PMCRAID_IOASC_IOA_WAS_RESET ||
	    atomic_read(&pinstance->ccn.ignore) == 1) {
		return;
	} else if (ioasc) {
		dev_info(&pinstance->pdev->dev,
			"Host RCB (CCN) failed with IOASC: 0x%08X\n", ioasc);
		spin_lock_irqsave(pinstance->host->host_lock, lock_flags);
		pmcraid_send_hcam(pinstance, PMCRAID_HCAM_CODE_CONFIG_CHANGE);
		spin_unlock_irqrestore(pinstance->host->host_lock, lock_flags);
	} else {
		pmcraid_handle_config_change(pinstance);
	}
}

/**
 * pmcraid_process_ldn - op done function for an LDN
 * @cmd: pointer to command block
 *
 * Return value
 *   none
 */
static void pmcraid_initiate_reset(struct pmcraid_instance *);

static void pmcraid_process_ldn(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	struct pmcraid_hcam_ldn *ldn_hcam =
			(struct pmcraid_hcam_ldn *)pinstance->ldn.hcam;
	u32 ioasc = le32_to_cpu(cmd->ioa_cb->ioasa.ioasc);
	u32 fd_ioasc = le32_to_cpu(ldn_hcam->error_log.fd_ioasc);
	unsigned long lock_flags;

	/* return the command block back to freepool */
	pinstance->ldn.cmd = NULL;
	pmcraid_return_cmd(cmd);

	/* If driver initiated IOA reset happened while this hcam was pending
	 * with IOA, no need to re-register the hcam as reset engine will do it
	 * once reset sequence is complete
	 */
	if (ioasc == PMCRAID_IOASC_IOA_WAS_RESET ||
	    atomic_read(&pinstance->ccn.ignore) == 1) {
		return;
	} else if (!ioasc) {
		pmcraid_handle_error_log(pinstance);
		if (fd_ioasc == PMCRAID_IOASC_NR_IOA_RESET_REQUIRED) {
			spin_lock_irqsave(pinstance->host->host_lock,
					  lock_flags);
			pmcraid_initiate_reset(pinstance);
			spin_unlock_irqrestore(pinstance->host->host_lock,
					       lock_flags);
			return;
		}
	} else {
		dev_info(&pinstance->pdev->dev,
			"Host RCB(LDN) failed with IOASC: 0x%08X\n", ioasc);
	}
	/* send netlink message for HCAM notification if enabled */
	if (!pmcraid_disable_aen)
		pmcraid_notify_aen(pinstance, PMCRAID_HCAM_CODE_LOG_DATA);

	cmd = pmcraid_init_hcam(pinstance, PMCRAID_HCAM_CODE_LOG_DATA);
	if (cmd)
		pmcraid_send_hcam_cmd(cmd);
}

/**
 * pmcraid_register_hcams - register HCAMs for CCN and LDN
 *
 * @pinstance: pointer per adapter instance structure
 *
 * Return Value
 *   none
 */
static void pmcraid_register_hcams(struct pmcraid_instance *pinstance)
{
	pmcraid_send_hcam(pinstance, PMCRAID_HCAM_CODE_CONFIG_CHANGE);
	pmcraid_send_hcam(pinstance, PMCRAID_HCAM_CODE_LOG_DATA);
}

/**
 * pmcraid_unregister_hcams - cancel HCAMs registered already
 * @cmd: pointer to command used as part of reset sequence
 */
static void pmcraid_unregister_hcams(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;

	/* During IOA bringdown, HCAM gets fired and tasklet proceeds with
	 * handling hcam response though it is not necessary. In order to
	 * prevent this, set 'ignore', so that bring-down sequence doesn't
	 * re-send any more hcams
	 */
	atomic_set(&pinstance->ccn.ignore, 1);
	atomic_set(&pinstance->ldn.ignore, 1);

	/* If adapter reset was forced as part of runtime reset sequence,
	 * start the reset sequence.
	 */
	if (pinstance->force_ioa_reset && !pinstance->ioa_bringdown) {
		pinstance->force_ioa_reset = 0;
		pinstance->ioa_state = IOA_STATE_IN_RESET_ALERT;
		pmcraid_reset_alert(cmd);
		return;
	}

	/* Driver tries to cancel HCAMs by sending ABORT TASK for each HCAM
	 * one after the other. So CCN cancellation will be triggered by
	 * pmcraid_cancel_ldn itself.
	 */
	pmcraid_cancel_ldn(cmd);
}

/**
 * pmcraid_reset_enable_ioa - re-enable IOA after a hard reset
 * @pinstance: pointer to adapter instance structure
 * Return Value
 *  1 if TRANSITION_TO_OPERATIONAL is active, otherwise 0
 */
static void pmcraid_reinit_buffers(struct pmcraid_instance *);

static int pmcraid_reset_enable_ioa(struct pmcraid_instance *pinstance)
{
	u32 intrs;

	pmcraid_reinit_buffers(pinstance);
	intrs = pmcraid_read_interrupts(pinstance);

	pmcraid_enable_interrupts(pinstance, PMCRAID_PCI_INTERRUPTS);

	if (intrs & INTRS_TRANSITION_TO_OPERATIONAL) {
		iowrite32(INTRS_TRANSITION_TO_OPERATIONAL,
			pinstance->int_regs.ioa_host_interrupt_mask_reg);
		iowrite32(INTRS_TRANSITION_TO_OPERATIONAL,
			pinstance->int_regs.ioa_host_interrupt_clr_reg);
		return 1;
	} else {
		return 0;
	}
}

/**
 * pmcraid_soft_reset - performs a soft reset and makes IOA become ready
 * @cmd : pointer to reset command block
 *
 * Return Value
 *	none
 */
static void pmcraid_soft_reset(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	u32 int_reg;
	u32 doorbell;

	/* There will be an interrupt when Transition to Operational bit is
	 * set so tasklet would execute next reset task. The timeout handler
	 * would re-initiate a reset
	 */
	cmd->cmd_done = pmcraid_ioa_reset;
	cmd->timer.data = (unsigned long)cmd;
	cmd->timer.expires = jiffies +
			     msecs_to_jiffies(PMCRAID_TRANSOP_TIMEOUT);
	cmd->timer.function = (void (*)(unsigned long))pmcraid_timeout_handler;

	if (!timer_pending(&cmd->timer))
		add_timer(&cmd->timer);

	/* Enable destructive diagnostics on IOA if it is not yet in
	 * operational state
	 */
	doorbell = DOORBELL_RUNTIME_RESET |
		   DOORBELL_ENABLE_DESTRUCTIVE_DIAGS;

	iowrite32(doorbell, pinstance->int_regs.host_ioa_interrupt_reg);
	int_reg = ioread32(pinstance->int_regs.ioa_host_interrupt_reg);
	pmcraid_info("Waiting for IOA to become operational %x:%x\n",
		     ioread32(pinstance->int_regs.host_ioa_interrupt_reg),
		     int_reg);
}

/**
 * pmcraid_get_dump - retrieves IOA dump in case of Unit Check interrupt
 *
 * @pinstance: pointer to adapter instance structure
 *
 * Return Value
 *	none
 */
static void pmcraid_get_dump(struct pmcraid_instance *pinstance)
{
	pmcraid_info("%s is not yet implemented\n", __func__);
}

/**
 * pmcraid_fail_outstanding_cmds - Fails all outstanding ops.
 * @pinstance: pointer to adapter instance structure
 *
 * This function fails all outstanding ops. If they are submitted to IOA
 * already, it sends cancel all messages if IOA is still accepting IOARCBs,
 * otherwise just completes the commands and returns the cmd blocks to free
 * pool.
 *
 * Return value:
 *	 none
 */
static void pmcraid_fail_outstanding_cmds(struct pmcraid_instance *pinstance)
{
	struct pmcraid_cmd *cmd, *temp;
	unsigned long lock_flags;

	/* pending command list is protected by pending_pool_lock. Its
	 * traversal must be done as within this lock
	 */
	spin_lock_irqsave(&pinstance->pending_pool_lock, lock_flags);
	list_for_each_entry_safe(cmd, temp, &pinstance->pending_cmd_pool,
				 free_list) {
		list_del(&cmd->free_list);
		spin_unlock_irqrestore(&pinstance->pending_pool_lock,
					lock_flags);
		cmd->ioa_cb->ioasa.ioasc =
			cpu_to_le32(PMCRAID_IOASC_IOA_WAS_RESET);
		cmd->ioa_cb->ioasa.ilid =
			cpu_to_be32(PMCRAID_DRIVER_ILID);

		/* In case the command timer is still running */
		del_timer(&cmd->timer);

		/* If this is an IO command, complete it by invoking scsi_done
		 * function. If this is one of the internal commands other
		 * than pmcraid_ioa_reset and HCAM commands invoke cmd_done to
		 * complete it
		 */
		if (cmd->scsi_cmd) {

			struct scsi_cmnd *scsi_cmd = cmd->scsi_cmd;
			__le32 resp = cmd->ioa_cb->ioarcb.response_handle;

			scsi_cmd->result |= DID_ERROR << 16;

			scsi_dma_unmap(scsi_cmd);
			pmcraid_return_cmd(cmd);

			pmcraid_info("failing(%d) CDB[0] = %x result: %x\n",
				     le32_to_cpu(resp) >> 2,
				     cmd->ioa_cb->ioarcb.cdb[0],
				     scsi_cmd->result);
			scsi_cmd->scsi_done(scsi_cmd);
		} else if (cmd->cmd_done == pmcraid_internal_done ||
			   cmd->cmd_done == pmcraid_erp_done) {
			cmd->cmd_done(cmd);
		} else if (cmd->cmd_done != pmcraid_ioa_reset) {
			pmcraid_return_cmd(cmd);
		}

		atomic_dec(&pinstance->outstanding_cmds);
		spin_lock_irqsave(&pinstance->pending_pool_lock, lock_flags);
	}

	spin_unlock_irqrestore(&pinstance->pending_pool_lock, lock_flags);
}

/**
 * pmcraid_ioa_reset - Implementation of IOA reset logic
 *
 * @cmd: pointer to the cmd block to be used for entire reset process
 *
 * This function executes most of the steps required for IOA reset. This gets
 * called by user threads (modprobe/insmod/rmmod) timer, tasklet and midlayer's
 * 'eh_' thread. Access to variables used for controling the reset sequence is
 * synchronized using host lock. Various functions called during reset process
 * would make use of a single command block, pointer to which is also stored in
 * adapter instance structure.
 *
 * Return Value
 *	 None
 */
static void pmcraid_ioa_reset(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	u8 reset_complete = 0;

	pinstance->ioa_reset_in_progress = 1;

	if (pinstance->reset_cmd != cmd) {
		pmcraid_err("reset is called with different command block\n");
		pinstance->reset_cmd = cmd;
	}

	pmcraid_info("reset_engine: state = %d, command = %p\n",
		      pinstance->ioa_state, cmd);

	switch (pinstance->ioa_state) {

	case IOA_STATE_DEAD:
		/* If IOA is offline, whatever may be the reset reason, just
		 * return. callers might be waiting on the reset wait_q, wake
		 * up them
		 */
		pmcraid_err("IOA is offline no reset is possible\n");
		reset_complete = 1;
		break;

	case IOA_STATE_IN_BRINGDOWN:
		/* we enter here, once ioa shutdown command is processed by IOA
		 * Alert IOA for a possible reset. If reset alert fails, IOA
		 * goes through hard-reset
		 */
		pmcraid_disable_interrupts(pinstance, ~0);
		pinstance->ioa_state = IOA_STATE_IN_RESET_ALERT;
		pmcraid_reset_alert(cmd);
		break;

	case IOA_STATE_UNKNOWN:
		/* We may be called during probe or resume. Some pre-processing
		 * is required for prior to reset
		 */
		scsi_block_requests(pinstance->host);

		/* If asked to reset while IOA was processing responses or
		 * there are any error responses then IOA may require
		 * hard-reset.
		 */
		if (pinstance->ioa_hard_reset == 0) {
			if (ioread32(pinstance->ioa_status) &
			    INTRS_TRANSITION_TO_OPERATIONAL) {
				pmcraid_info("sticky bit set, bring-up\n");
				pinstance->ioa_state = IOA_STATE_IN_BRINGUP;
				pmcraid_reinit_cmdblk(cmd);
				pmcraid_identify_hrrq(cmd);
			} else {
				pinstance->ioa_state = IOA_STATE_IN_SOFT_RESET;
				pmcraid_soft_reset(cmd);
			}
		} else {
			/* Alert IOA of a possible reset and wait for critical
			 * operation in progress bit to reset
			 */
			pinstance->ioa_state = IOA_STATE_IN_RESET_ALERT;
			pmcraid_reset_alert(cmd);
		}
		break;

	case IOA_STATE_IN_RESET_ALERT:
		/* If critical operation in progress bit is reset or wait gets
		 * timed out, reset proceeds with starting BIST on the IOA.
		 * pmcraid_ioa_hard_reset keeps a count of reset attempts. If
		 * they are 3 or more, reset engine marks IOA dead and returns
		 */
		pinstance->ioa_state = IOA_STATE_IN_HARD_RESET;
		pmcraid_start_bist(cmd);
		break;

	case IOA_STATE_IN_HARD_RESET:
		pinstance->ioa_reset_attempts++;

		/* retry reset if we haven't reached maximum allowed limit */
		if (pinstance->ioa_reset_attempts > PMCRAID_RESET_ATTEMPTS) {
			pinstance->ioa_reset_attempts = 0;
			pmcraid_err("IOA didn't respond marking it as dead\n");
			pinstance->ioa_state = IOA_STATE_DEAD;
			reset_complete = 1;
			break;
		}

		/* Once either bist or pci reset is done, restore PCI config
		 * space. If this fails, proceed with hard reset again
		 */

		if (pci_restore_state(pinstance->pdev)) {
			pmcraid_info("config-space error resetting again\n");
			pinstance->ioa_state = IOA_STATE_IN_RESET_ALERT;
			pmcraid_reset_alert(cmd);
			break;
		}

		/* fail all pending commands */
		pmcraid_fail_outstanding_cmds(pinstance);

		/* check if unit check is active, if so extract dump */
		if (pinstance->ioa_unit_check) {
			pmcraid_info("unit check is active\n");
			pinstance->ioa_unit_check = 0;
			pmcraid_get_dump(pinstance);
			pinstance->ioa_reset_attempts--;
			pinstance->ioa_state = IOA_STATE_IN_RESET_ALERT;
			pmcraid_reset_alert(cmd);
			break;
		}

		/* if the reset reason is to bring-down the ioa, we might be
		 * done with the reset restore pci_config_space and complete
		 * the reset
		 */
		if (pinstance->ioa_bringdown) {
			pmcraid_info("bringing down the adapter\n");
			pinstance->ioa_shutdown_type = SHUTDOWN_NONE;
			pinstance->ioa_bringdown = 0;
			pinstance->ioa_state = IOA_STATE_UNKNOWN;
			reset_complete = 1;
		} else {
			/* bring-up IOA, so proceed with soft reset
			 * Reinitialize hrrq_buffers and their indices also
			 * enable interrupts after a pci_restore_state
			 */
			if (pmcraid_reset_enable_ioa(pinstance)) {
				pinstance->ioa_state = IOA_STATE_IN_BRINGUP;
				pmcraid_info("bringing up the adapter\n");
				pmcraid_reinit_cmdblk(cmd);
				pmcraid_identify_hrrq(cmd);
			} else {
				pinstance->ioa_state = IOA_STATE_IN_SOFT_RESET;
				pmcraid_soft_reset(cmd);
			}
		}
		break;

	case IOA_STATE_IN_SOFT_RESET:
		/* TRANSITION TO OPERATIONAL is on so start initialization
		 * sequence
		 */
		pmcraid_info("In softreset proceeding with bring-up\n");
		pinstance->ioa_state = IOA_STATE_IN_BRINGUP;

		/* Initialization commands start with HRRQ identification. From
		 * now on tasklet completes most of the commands as IOA is up
		 * and intrs are enabled
		 */
		pmcraid_identify_hrrq(cmd);
		break;

	case IOA_STATE_IN_BRINGUP:
		/* we are done with bringing up of IOA, change the ioa_state to
		 * operational and wake up any waiters
		 */
		pinstance->ioa_state = IOA_STATE_OPERATIONAL;
		reset_complete = 1;
		break;

	case IOA_STATE_OPERATIONAL:
	default:
		/* When IOA is operational and a reset is requested, check for
		 * the reset reason. If reset is to bring down IOA, unregister
		 * HCAMs and initiate shutdown; if adapter reset is forced then
		 * restart reset sequence again
		 */
		if (pinstance->ioa_shutdown_type == SHUTDOWN_NONE &&
		    pinstance->force_ioa_reset == 0) {
			reset_complete = 1;
		} else {
			if (pinstance->ioa_shutdown_type != SHUTDOWN_NONE)
				pinstance->ioa_state = IOA_STATE_IN_BRINGDOWN;
			pmcraid_reinit_cmdblk(cmd);
			pmcraid_unregister_hcams(cmd);
		}
		break;
	}

	/* reset will be completed if ioa_state is either DEAD or UNKNOWN or
	 * OPERATIONAL. Reset all control variables used during reset, wake up
	 * any waiting threads and let the SCSI mid-layer send commands. Note
	 * that host_lock must be held before invoking scsi_report_bus_reset.
	 */
	if (reset_complete) {
		pinstance->ioa_reset_in_progress = 0;
		pinstance->ioa_reset_attempts = 0;
		pinstance->reset_cmd = NULL;
		pinstance->ioa_shutdown_type = SHUTDOWN_NONE;
		pinstance->ioa_bringdown = 0;
		pmcraid_return_cmd(cmd);

		/* If target state is to bring up the adapter, proceed with
		 * hcam registration and resource exposure to mid-layer.
		 */
		if (pinstance->ioa_state == IOA_STATE_OPERATIONAL)
			pmcraid_register_hcams(pinstance);

		wake_up_all(&pinstance->reset_wait_q);
	}

	return;
}

/**
 * pmcraid_initiate_reset - initiates reset sequence. This is called from
 * ISR/tasklet during error interrupts including IOA unit check. If reset
 * is already in progress, it just returns, otherwise initiates IOA reset
 * to bring IOA up to operational state.
 *
 * @pinstance: pointer to adapter instance structure
 *
 * Return value
 *	 none
 */
static void pmcraid_initiate_reset(struct pmcraid_instance *pinstance)
{
	struct pmcraid_cmd *cmd;

	/* If the reset is already in progress, just return, otherwise start
	 * reset sequence and return
	 */
	if (!pinstance->ioa_reset_in_progress) {
		scsi_block_requests(pinstance->host);
		cmd = pmcraid_get_free_cmd(pinstance);

		if (cmd == NULL) {
			pmcraid_err("no cmnd blocks for initiate_reset\n");
			return;
		}

		pinstance->ioa_shutdown_type = SHUTDOWN_NONE;
		pinstance->reset_cmd = cmd;
		pinstance->force_ioa_reset = 1;
		pmcraid_ioa_reset(cmd);
	}
}

/**
 * pmcraid_reset_reload - utility routine for doing IOA reset either to bringup
 *			  or bringdown IOA
 * @pinstance: pointer adapter instance structure
 * @shutdown_type: shutdown type to be used NONE, NORMAL or ABRREV
 * @target_state: expected target state after reset
 *
 * Note: This command initiates reset and waits for its completion. Hence this
 * should not be called from isr/timer/tasklet functions (timeout handlers,
 * error response handlers and interrupt handlers).
 *
 * Return Value
 *	 1 in case ioa_state is not target_state, 0 otherwise.
 */
static int pmcraid_reset_reload(
	struct pmcraid_instance *pinstance,
	u8 shutdown_type,
	u8 target_state
)
{
	struct pmcraid_cmd *reset_cmd = NULL;
	unsigned long lock_flags;
	int reset = 1;

	spin_lock_irqsave(pinstance->host->host_lock, lock_flags);

	if (pinstance->ioa_reset_in_progress) {
		pmcraid_info("reset_reload: reset is already in progress\n");

		spin_unlock_irqrestore(pinstance->host->host_lock, lock_flags);

		wait_event(pinstance->reset_wait_q,
			   !pinstance->ioa_reset_in_progress);

		spin_lock_irqsave(pinstance->host->host_lock, lock_flags);

		if (pinstance->ioa_state == IOA_STATE_DEAD) {
			spin_unlock_irqrestore(pinstance->host->host_lock,
					       lock_flags);
			pmcraid_info("reset_reload: IOA is dead\n");
			return reset;
		} else if (pinstance->ioa_state == target_state) {
			reset = 0;
		}
	}

	if (reset) {
		pmcraid_info("reset_reload: proceeding with reset\n");
		scsi_block_requests(pinstance->host);
		reset_cmd = pmcraid_get_free_cmd(pinstance);

		if (reset_cmd == NULL) {
			pmcraid_err("no free cmnd for reset_reload\n");
			spin_unlock_irqrestore(pinstance->host->host_lock,
					       lock_flags);
			return reset;
		}

		if (shutdown_type == SHUTDOWN_NORMAL)
			pinstance->ioa_bringdown = 1;

		pinstance->ioa_shutdown_type = shutdown_type;
		pinstance->reset_cmd = reset_cmd;
		pinstance->force_ioa_reset = reset;
		pmcraid_info("reset_reload: initiating reset\n");
		pmcraid_ioa_reset(reset_cmd);
		spin_unlock_irqrestore(pinstance->host->host_lock, lock_flags);
		pmcraid_info("reset_reload: waiting for reset to complete\n");
		wait_event(pinstance->reset_wait_q,
			   !pinstance->ioa_reset_in_progress);

		pmcraid_info("reset_reload: reset is complete !! \n");
		scsi_unblock_requests(pinstance->host);
		if (pinstance->ioa_state == target_state)
			reset = 0;
	}

	return reset;
}

/**
 * pmcraid_reset_bringdown - wrapper over pmcraid_reset_reload to bringdown IOA
 *
 * @pinstance: pointer to adapter instance structure
 *
 * Return Value
 *	 whatever is returned from pmcraid_reset_reload
 */
static int pmcraid_reset_bringdown(struct pmcraid_instance *pinstance)
{
	return pmcraid_reset_reload(pinstance,
				    SHUTDOWN_NORMAL,
				    IOA_STATE_UNKNOWN);
}

/**
 * pmcraid_reset_bringup - wrapper over pmcraid_reset_reload to bring up IOA
 *
 * @pinstance: pointer to adapter instance structure
 *
 * Return Value
 *	 whatever is returned from pmcraid_reset_reload
 */
static int pmcraid_reset_bringup(struct pmcraid_instance *pinstance)
{
	return pmcraid_reset_reload(pinstance,
				    SHUTDOWN_NONE,
				    IOA_STATE_OPERATIONAL);
}

/**
 * pmcraid_request_sense - Send request sense to a device
 * @cmd: pmcraid command struct
 *
 * This function sends a request sense to a device as a result of a check
 * condition. This method re-uses the same command block that failed earlier.
 */
static void pmcraid_request_sense(struct pmcraid_cmd *cmd)
{
	struct pmcraid_ioarcb *ioarcb = &cmd->ioa_cb->ioarcb;
	struct pmcraid_ioadl_desc *ioadl = ioarcb->add_data.u.ioadl;

	/* allocate DMAable memory for sense buffers */
	cmd->sense_buffer = pci_alloc_consistent(cmd->drv_inst->pdev,
						 SCSI_SENSE_BUFFERSIZE,
						 &cmd->sense_buffer_dma);

	if (cmd->sense_buffer == NULL) {
		pmcraid_err
			("couldn't allocate sense buffer for request sense\n");
		pmcraid_erp_done(cmd);
		return;
	}

	/* re-use the command block */
	memset(&cmd->ioa_cb->ioasa, 0, sizeof(struct pmcraid_ioasa));
	memset(ioarcb->cdb, 0, PMCRAID_MAX_CDB_LEN);
	ioarcb->request_flags0 = (SYNC_COMPLETE |
				  NO_LINK_DESCS |
				  INHIBIT_UL_CHECK);
	ioarcb->request_type = REQ_TYPE_SCSI;
	ioarcb->cdb[0] = REQUEST_SENSE;
	ioarcb->cdb[4] = SCSI_SENSE_BUFFERSIZE;

	ioarcb->ioadl_bus_addr = cpu_to_le64((cmd->ioa_cb_bus_addr) +
					offsetof(struct pmcraid_ioarcb,
						add_data.u.ioadl[0]));
	ioarcb->ioadl_length = cpu_to_le32(sizeof(struct pmcraid_ioadl_desc));

	ioarcb->data_transfer_length = cpu_to_le32(SCSI_SENSE_BUFFERSIZE);

	ioadl->address = cpu_to_le64(cmd->sense_buffer_dma);
	ioadl->data_len = cpu_to_le32(SCSI_SENSE_BUFFERSIZE);
	ioadl->flags = IOADL_FLAGS_LAST_DESC;

	/* request sense might be called as part of error response processing
	 * which runs in tasklets context. It is possible that mid-layer might
	 * schedule queuecommand during this time, hence, writting to IOARRIN
	 * must be protect by host_lock
	 */
	pmcraid_send_cmd(cmd, pmcraid_erp_done,
			 PMCRAID_REQUEST_SENSE_TIMEOUT,
			 pmcraid_timeout_handler);
}

/**
 * pmcraid_cancel_all - cancel all outstanding IOARCBs as part of error recovery
 * @cmd: command that failed
 * @sense: true if request_sense is required after cancel all
 *
 * This function sends a cancel all to a device to clear the queue.
 */
static void pmcraid_cancel_all(struct pmcraid_cmd *cmd, u32 sense)
{
	struct scsi_cmnd *scsi_cmd = cmd->scsi_cmd;
	struct pmcraid_ioarcb *ioarcb = &cmd->ioa_cb->ioarcb;
	struct pmcraid_resource_entry *res = scsi_cmd->device->hostdata;
	void (*cmd_done) (struct pmcraid_cmd *) = sense ? pmcraid_erp_done
							: pmcraid_request_sense;

	memset(ioarcb->cdb, 0, PMCRAID_MAX_CDB_LEN);
	ioarcb->request_flags0 = SYNC_OVERRIDE;
	ioarcb->request_type = REQ_TYPE_IOACMD;
	ioarcb->cdb[0] = PMCRAID_CANCEL_ALL_REQUESTS;

	if (RES_IS_GSCSI(res->cfg_entry))
		ioarcb->cdb[1] = PMCRAID_SYNC_COMPLETE_AFTER_CANCEL;

	ioarcb->ioadl_bus_addr = 0;
	ioarcb->ioadl_length = 0;
	ioarcb->data_transfer_length = 0;
	ioarcb->ioarcb_bus_addr &= (~0x1FULL);

	/* writing to IOARRIN must be protected by host_lock, as mid-layer
	 * schedule queuecommand while we are doing this
	 */
	pmcraid_send_cmd(cmd, cmd_done,
			 PMCRAID_REQUEST_SENSE_TIMEOUT,
			 pmcraid_timeout_handler);
}

/**
 * pmcraid_frame_auto_sense: frame fixed format sense information
 *
 * @cmd: pointer to failing command block
 *
 * Return value
 *  none
 */
static void pmcraid_frame_auto_sense(struct pmcraid_cmd *cmd)
{
	u8 *sense_buf = cmd->scsi_cmd->sense_buffer;
	struct pmcraid_resource_entry *res = cmd->scsi_cmd->device->hostdata;
	struct pmcraid_ioasa *ioasa = &cmd->ioa_cb->ioasa;
	u32 ioasc = le32_to_cpu(ioasa->ioasc);
	u32 failing_lba = 0;

	memset(sense_buf, 0, SCSI_SENSE_BUFFERSIZE);
	cmd->scsi_cmd->result = SAM_STAT_CHECK_CONDITION;

	if (RES_IS_VSET(res->cfg_entry) &&
	    ioasc == PMCRAID_IOASC_ME_READ_ERROR_NO_REALLOC &&
	    ioasa->u.vset.failing_lba_hi != 0) {

		sense_buf[0] = 0x72;
		sense_buf[1] = PMCRAID_IOASC_SENSE_KEY(ioasc);
		sense_buf[2] = PMCRAID_IOASC_SENSE_CODE(ioasc);
		sense_buf[3] = PMCRAID_IOASC_SENSE_QUAL(ioasc);

		sense_buf[7] = 12;
		sense_buf[8] = 0;
		sense_buf[9] = 0x0A;
		sense_buf[10] = 0x80;

		failing_lba = le32_to_cpu(ioasa->u.vset.failing_lba_hi);

		sense_buf[12] = (failing_lba & 0xff000000) >> 24;
		sense_buf[13] = (failing_lba & 0x00ff0000) >> 16;
		sense_buf[14] = (failing_lba & 0x0000ff00) >> 8;
		sense_buf[15] = failing_lba & 0x000000ff;

		failing_lba = le32_to_cpu(ioasa->u.vset.failing_lba_lo);

		sense_buf[16] = (failing_lba & 0xff000000) >> 24;
		sense_buf[17] = (failing_lba & 0x00ff0000) >> 16;
		sense_buf[18] = (failing_lba & 0x0000ff00) >> 8;
		sense_buf[19] = failing_lba & 0x000000ff;
	} else {
		sense_buf[0] = 0x70;
		sense_buf[2] = PMCRAID_IOASC_SENSE_KEY(ioasc);
		sense_buf[12] = PMCRAID_IOASC_SENSE_CODE(ioasc);
		sense_buf[13] = PMCRAID_IOASC_SENSE_QUAL(ioasc);

		if (ioasc == PMCRAID_IOASC_ME_READ_ERROR_NO_REALLOC) {
			if (RES_IS_VSET(res->cfg_entry))
				failing_lba =
					le32_to_cpu(ioasa->u.
						 vset.failing_lba_lo);
			sense_buf[0] |= 0x80;
			sense_buf[3] = (failing_lba >> 24) & 0xff;
			sense_buf[4] = (failing_lba >> 16) & 0xff;
			sense_buf[5] = (failing_lba >> 8) & 0xff;
			sense_buf[6] = failing_lba & 0xff;
		}

		sense_buf[7] = 6; /* additional length */
	}
}

/**
 * pmcraid_error_handler - Error response handlers for a SCSI op
 * @cmd: pointer to pmcraid_cmd that has failed
 *
 * This function determines whether or not to initiate ERP on the affected
 * device. This is called from a tasklet, which doesn't hold any locks.
 *
 * Return value:
 *	 0 it caller can complete the request, otherwise 1 where in error
 *	 handler itself completes the request and returns the command block
 *	 back to free-pool
 */
static int pmcraid_error_handler(struct pmcraid_cmd *cmd)
{
	struct scsi_cmnd *scsi_cmd = cmd->scsi_cmd;
	struct pmcraid_resource_entry *res = scsi_cmd->device->hostdata;
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	struct pmcraid_ioasa *ioasa = &cmd->ioa_cb->ioasa;
	u32 ioasc = le32_to_cpu(ioasa->ioasc);
	u32 masked_ioasc = ioasc & PMCRAID_IOASC_SENSE_MASK;
	u32 sense_copied = 0;

	if (!res) {
		pmcraid_info("resource pointer is NULL\n");
		return 0;
	}

	/* If this was a SCSI read/write command keep count of errors */
	if (SCSI_CMD_TYPE(scsi_cmd->cmnd[0]) == SCSI_READ_CMD)
		atomic_inc(&res->read_failures);
	else if (SCSI_CMD_TYPE(scsi_cmd->cmnd[0]) == SCSI_WRITE_CMD)
		atomic_inc(&res->write_failures);

	if (!RES_IS_GSCSI(res->cfg_entry) &&
		masked_ioasc != PMCRAID_IOASC_HW_DEVICE_BUS_STATUS_ERROR) {
		pmcraid_frame_auto_sense(cmd);
	}

	/* Log IOASC/IOASA information based on user settings */
	pmcraid_ioasc_logger(ioasc, cmd);

	switch (masked_ioasc) {

	case PMCRAID_IOASC_AC_TERMINATED_BY_HOST:
		scsi_cmd->result |= (DID_ABORT << 16);
		break;

	case PMCRAID_IOASC_IR_INVALID_RESOURCE_HANDLE:
	case PMCRAID_IOASC_HW_CANNOT_COMMUNICATE:
		scsi_cmd->result |= (DID_NO_CONNECT << 16);
		break;

	case PMCRAID_IOASC_NR_SYNC_REQUIRED:
		res->sync_reqd = 1;
		scsi_cmd->result |= (DID_IMM_RETRY << 16);
		break;

	case PMCRAID_IOASC_ME_READ_ERROR_NO_REALLOC:
		scsi_cmd->result |= (DID_PASSTHROUGH << 16);
		break;

	case PMCRAID_IOASC_UA_BUS_WAS_RESET:
	case PMCRAID_IOASC_UA_BUS_WAS_RESET_BY_OTHER:
		if (!res->reset_progress)
			scsi_report_bus_reset(pinstance->host,
					      scsi_cmd->device->channel);
		scsi_cmd->result |= (DID_ERROR << 16);
		break;

	case PMCRAID_IOASC_HW_DEVICE_BUS_STATUS_ERROR:
		scsi_cmd->result |= PMCRAID_IOASC_SENSE_STATUS(ioasc);
		res->sync_reqd = 1;

		/* if check_condition is not active return with error otherwise
		 * get/frame the sense buffer
		 */
		if (PMCRAID_IOASC_SENSE_STATUS(ioasc) !=
		    SAM_STAT_CHECK_CONDITION &&
		    PMCRAID_IOASC_SENSE_STATUS(ioasc) != SAM_STAT_ACA_ACTIVE)
			return 0;

		/* If we have auto sense data as part of IOASA pass it to
		 * mid-layer
		 */
		if (ioasa->auto_sense_length != 0) {
			short sense_len = ioasa->auto_sense_length;
			int data_size = min_t(u16, le16_to_cpu(sense_len),
					      SCSI_SENSE_BUFFERSIZE);

			memcpy(scsi_cmd->sense_buffer,
			       ioasa->sense_data,
			       data_size);
			sense_copied = 1;
		}

		if (RES_IS_GSCSI(res->cfg_entry)) {
			pmcraid_cancel_all(cmd, sense_copied);
		} else if (sense_copied) {
			pmcraid_erp_done(cmd);
			return 0;
		} else  {
			pmcraid_request_sense(cmd);
		}

		return 1;

	case PMCRAID_IOASC_NR_INIT_CMD_REQUIRED:
		break;

	default:
		if (PMCRAID_IOASC_SENSE_KEY(ioasc) > RECOVERED_ERROR)
			scsi_cmd->result |= (DID_ERROR << 16);
		break;
	}
	return 0;
}

/**
 * pmcraid_reset_device - device reset handler functions
 *
 * @scsi_cmd: scsi command struct
 * @modifier: reset modifier indicating the reset sequence to be performed
 *
 * This function issues a device reset to the affected device.
 * A LUN reset will be sent to the device first. If that does
 * not work, a target reset will be sent.
 *
 * Return value:
 *	SUCCESS / FAILED
 */
static int pmcraid_reset_device(
	struct scsi_cmnd *scsi_cmd,
	unsigned long timeout,
	u8 modifier
)
{
	struct pmcraid_cmd *cmd;
	struct pmcraid_instance *pinstance;
	struct pmcraid_resource_entry *res;
	struct pmcraid_ioarcb *ioarcb;
	unsigned long lock_flags;
	u32 ioasc;

	pinstance =
		(struct pmcraid_instance *)scsi_cmd->device->host->hostdata;
	res = scsi_cmd->device->hostdata;

	if (!res) {
		sdev_printk(KERN_ERR, scsi_cmd->device,
			    "reset_device: NULL resource pointer\n");
		return FAILED;
	}

	/* If adapter is currently going through reset/reload, return failed.
	 * This will force the mid-layer to call _eh_bus/host reset, which
	 * will then go to sleep and wait for the reset to complete
	 */
	spin_lock_irqsave(pinstance->host->host_lock, lock_flags);
	if (pinstance->ioa_reset_in_progress ||
	    pinstance->ioa_state == IOA_STATE_DEAD) {
		spin_unlock_irqrestore(pinstance->host->host_lock, lock_flags);
		return FAILED;
	}

	res->reset_progress = 1;
	pmcraid_info("Resetting %s resource with addr %x\n",
		     ((modifier & RESET_DEVICE_LUN) ? "LUN" :
		     ((modifier & RESET_DEVICE_TARGET) ? "TARGET" : "BUS")),
		     le32_to_cpu(res->cfg_entry.resource_address));

	/* get a free cmd block */
	cmd = pmcraid_get_free_cmd(pinstance);

	if (cmd == NULL) {
		spin_unlock_irqrestore(pinstance->host->host_lock, lock_flags);
		pmcraid_err("%s: no cmd blocks are available\n", __func__);
		return FAILED;
	}

	ioarcb = &cmd->ioa_cb->ioarcb;
	ioarcb->resource_handle = res->cfg_entry.resource_handle;
	ioarcb->request_type = REQ_TYPE_IOACMD;
	ioarcb->cdb[0] = PMCRAID_RESET_DEVICE;

	/* Initialize reset modifier bits */
	if (modifier)
		modifier = ENABLE_RESET_MODIFIER | modifier;

	ioarcb->cdb[1] = modifier;

	init_completion(&cmd->wait_for_completion);
	cmd->completion_req = 1;

	pmcraid_info("cmd(CDB[0] = %x) for %x with index = %d\n",
		     cmd->ioa_cb->ioarcb.cdb[0],
		     le32_to_cpu(cmd->ioa_cb->ioarcb.resource_handle),
		     le32_to_cpu(cmd->ioa_cb->ioarcb.response_handle) >> 2);

	pmcraid_send_cmd(cmd,
			 pmcraid_internal_done,
			 timeout,
			 pmcraid_timeout_handler);

	spin_unlock_irqrestore(pinstance->host->host_lock, lock_flags);

	/* RESET_DEVICE command completes after all pending IOARCBs are
	 * completed. Once this command is completed, pmcraind_internal_done
	 * will wake up the 'completion' queue.
	 */
	wait_for_completion(&cmd->wait_for_completion);

	/* complete the command here itself and return the command block
	 * to free list
	 */
	pmcraid_return_cmd(cmd);
	res->reset_progress = 0;
	ioasc = le32_to_cpu(cmd->ioa_cb->ioasa.ioasc);

	/* set the return value based on the returned ioasc */
	return PMCRAID_IOASC_SENSE_KEY(ioasc) ? FAILED : SUCCESS;
}

/**
 * _pmcraid_io_done - helper for pmcraid_io_done function
 *
 * @cmd: pointer to pmcraid command struct
 * @reslen: residual data length to be set in the ioasa
 * @ioasc: ioasc either returned by IOA or set by driver itself.
 *
 * This function is invoked by pmcraid_io_done to complete mid-layer
 * scsi ops.
 *
 * Return value:
 *	  0 if caller is required to return it to free_pool. Returns 1 if
 *	  caller need not worry about freeing command block as error handler
 *	  will take care of that.
 */

static int _pmcraid_io_done(struct pmcraid_cmd *cmd, int reslen, int ioasc)
{
	struct scsi_cmnd *scsi_cmd = cmd->scsi_cmd;
	int rc = 0;

	scsi_set_resid(scsi_cmd, reslen);

	pmcraid_info("response(%d) CDB[0] = %x ioasc:result: %x:%x\n",
		le32_to_cpu(cmd->ioa_cb->ioarcb.response_handle) >> 2,
		cmd->ioa_cb->ioarcb.cdb[0],
		ioasc, scsi_cmd->result);

	if (PMCRAID_IOASC_SENSE_KEY(ioasc) != 0)
		rc = pmcraid_error_handler(cmd);

	if (rc == 0) {
		scsi_dma_unmap(scsi_cmd);
		scsi_cmd->scsi_done(scsi_cmd);
	}

	return rc;
}

/**
 * pmcraid_io_done - SCSI completion function
 *
 * @cmd: pointer to pmcraid command struct
 *
 * This function is invoked by tasklet/mid-layer error handler to completing
 * the SCSI ops sent from mid-layer.
 *
 * Return value
 *	  none
 */

static void pmcraid_io_done(struct pmcraid_cmd *cmd)
{
	u32 ioasc = le32_to_cpu(cmd->ioa_cb->ioasa.ioasc);
	u32 reslen = le32_to_cpu(cmd->ioa_cb->ioasa.residual_data_length);

	if (_pmcraid_io_done(cmd, reslen, ioasc) == 0)
		pmcraid_return_cmd(cmd);
}

/**
 * pmcraid_abort_cmd - Aborts a single IOARCB already submitted to IOA
 *
 * @cmd: command block of the command to be aborted
 *
 * Return Value:
 *	 returns pointer to command structure used as cancelling cmd
 */
static struct pmcraid_cmd *pmcraid_abort_cmd(struct pmcraid_cmd *cmd)
{
	struct pmcraid_cmd *cancel_cmd;
	struct pmcraid_instance *pinstance;
	struct pmcraid_resource_entry *res;

	pinstance = (struct pmcraid_instance *)cmd->drv_inst;
	res = cmd->scsi_cmd->device->hostdata;

	cancel_cmd = pmcraid_get_free_cmd(pinstance);

	if (cancel_cmd == NULL) {
		pmcraid_err("%s: no cmd blocks are available\n", __func__);
		return NULL;
	}

	pmcraid_prepare_cancel_cmd(cancel_cmd, cmd);

	pmcraid_info("aborting command CDB[0]= %x with index = %d\n",
		cmd->ioa_cb->ioarcb.cdb[0],
		cmd->ioa_cb->ioarcb.response_handle >> 2);

	init_completion(&cancel_cmd->wait_for_completion);
	cancel_cmd->completion_req = 1;

	pmcraid_info("command (%d) CDB[0] = %x for %x\n",
		le32_to_cpu(cancel_cmd->ioa_cb->ioarcb.response_handle) >> 2,
		cmd->ioa_cb->ioarcb.cdb[0],
		le32_to_cpu(cancel_cmd->ioa_cb->ioarcb.resource_handle));

	pmcraid_send_cmd(cancel_cmd,
			 pmcraid_internal_done,
			 PMCRAID_INTERNAL_TIMEOUT,
			 pmcraid_timeout_handler);
	return cancel_cmd;
}

/**
 * pmcraid_abort_complete - Waits for ABORT TASK completion
 *
 * @cancel_cmd: command block use as cancelling command
 *
 * Return Value:
 *	 returns SUCCESS if ABORT TASK has good completion
 *	 otherwise FAILED
 */
static int pmcraid_abort_complete(struct pmcraid_cmd *cancel_cmd)
{
	struct pmcraid_resource_entry *res;
	u32 ioasc;

	wait_for_completion(&cancel_cmd->wait_for_completion);
	res = cancel_cmd->u.res;
	cancel_cmd->u.res = NULL;
	ioasc = le32_to_cpu(cancel_cmd->ioa_cb->ioasa.ioasc);

	/* If the abort task is not timed out we will get a Good completion
	 * as sense_key, otherwise we may get one the following responses
	 * due to subsquent bus reset or device reset. In case IOASC is
	 * NR_SYNC_REQUIRED, set sync_reqd flag for the corresponding resource
	 */
	if (ioasc == PMCRAID_IOASC_UA_BUS_WAS_RESET ||
	    ioasc == PMCRAID_IOASC_NR_SYNC_REQUIRED) {
		if (ioasc == PMCRAID_IOASC_NR_SYNC_REQUIRED)
			res->sync_reqd = 1;
		ioasc = 0;
	}

	/* complete the command here itself */
	pmcraid_return_cmd(cancel_cmd);
	return PMCRAID_IOASC_SENSE_KEY(ioasc) ? FAILED : SUCCESS;
}

/**
 * pmcraid_eh_abort_handler - entry point for aborting a single task on errors
 *
 * @scsi_cmd:   scsi command struct given by mid-layer. When this is called
 *		mid-layer ensures that no other commands are queued. This
 *		never gets called under interrupt, but a separate eh thread.
 *
 * Return value:
 *	 SUCCESS / FAILED
 */
static int pmcraid_eh_abort_handler(struct scsi_cmnd *scsi_cmd)
{
	struct pmcraid_instance *pinstance;
	struct pmcraid_cmd *cmd;
	struct pmcraid_resource_entry *res;
	unsigned long host_lock_flags;
	unsigned long pending_lock_flags;
	struct pmcraid_cmd *cancel_cmd = NULL;
	int cmd_found = 0;
	int rc = FAILED;

	pinstance =
		(struct pmcraid_instance *)scsi_cmd->device->host->hostdata;

	scmd_printk(KERN_INFO, scsi_cmd,
		    "I/O command timed out, aborting it.\n");

	res = scsi_cmd->device->hostdata;

	if (res == NULL)
		return rc;

	/* If we are currently going through reset/reload, return failed.
	 * This will force the mid-layer to eventually call
	 * pmcraid_eh_host_reset which will then go to sleep and wait for the
	 * reset to complete
	 */
	spin_lock_irqsave(pinstance->host->host_lock, host_lock_flags);

	if (pinstance->ioa_reset_in_progress ||
	    pinstance->ioa_state == IOA_STATE_DEAD) {
		spin_unlock_irqrestore(pinstance->host->host_lock,
				       host_lock_flags);
		return rc;
	}

	/* loop over pending cmd list to find cmd corresponding to this
	 * scsi_cmd. Note that this command might not have been completed
	 * already. locking: all pending commands are protected with
	 * pending_pool_lock.
	 */
	spin_lock_irqsave(&pinstance->pending_pool_lock, pending_lock_flags);
	list_for_each_entry(cmd, &pinstance->pending_cmd_pool, free_list) {

		if (cmd->scsi_cmd == scsi_cmd) {
			cmd_found = 1;
			break;
		}
	}

	spin_unlock_irqrestore(&pinstance->pending_pool_lock,
				pending_lock_flags);

	/* If the command to be aborted was given to IOA and still pending with
	 * it, send ABORT_TASK to abort this and wait for its completion
	 */
	if (cmd_found)
		cancel_cmd = pmcraid_abort_cmd(cmd);

	spin_unlock_irqrestore(pinstance->host->host_lock,
			       host_lock_flags);

	if (cancel_cmd) {
		cancel_cmd->u.res = cmd->scsi_cmd->device->hostdata;
		rc = pmcraid_abort_complete(cancel_cmd);
	}

	return cmd_found ? rc : SUCCESS;
}

/**
 * pmcraid_eh_xxxx_reset_handler - bus/target/device reset handler callbacks
 *
 * @scmd: pointer to scsi_cmd that was sent to the resource to be reset.
 *
 * All these routines invokve pmcraid_reset_device with appropriate parameters.
 * Since these are called from mid-layer EH thread, no other IO will be queued
 * to the resource being reset. However, control path (IOCTL) may be active so
 * it is necessary to synchronize IOARRIN writes which pmcraid_reset_device
 * takes care by locking/unlocking host_lock.
 *
 * Return value
 * 	SUCCESS or FAILED
 */
static int pmcraid_eh_device_reset_handler(struct scsi_cmnd *scmd)
{
	scmd_printk(KERN_INFO, scmd,
		    "resetting device due to an I/O command timeout.\n");
	return pmcraid_reset_device(scmd,
				    PMCRAID_INTERNAL_TIMEOUT,
				    RESET_DEVICE_LUN);
}

static int pmcraid_eh_bus_reset_handler(struct scsi_cmnd *scmd)
{
	scmd_printk(KERN_INFO, scmd,
		    "Doing bus reset due to an I/O command timeout.\n");
	return pmcraid_reset_device(scmd,
				    PMCRAID_RESET_BUS_TIMEOUT,
				    RESET_DEVICE_BUS);
}

static int pmcraid_eh_target_reset_handler(struct scsi_cmnd *scmd)
{
	scmd_printk(KERN_INFO, scmd,
		    "Doing target reset due to an I/O command timeout.\n");
	return pmcraid_reset_device(scmd,
				    PMCRAID_INTERNAL_TIMEOUT,
				    RESET_DEVICE_TARGET);
}

/**
 * pmcraid_eh_host_reset_handler - adapter reset handler callback
 *
 * @scmd: pointer to scsi_cmd that was sent to a resource of adapter
 *
 * Initiates adapter reset to bring it up to operational state
 *
 * Return value
 * 	SUCCESS or FAILED
 */
static int pmcraid_eh_host_reset_handler(struct scsi_cmnd *scmd)
{
	unsigned long interval = 10000; /* 10 seconds interval */
	int waits = jiffies_to_msecs(PMCRAID_RESET_HOST_TIMEOUT) / interval;
	struct pmcraid_instance *pinstance =
		(struct pmcraid_instance *)(scmd->device->host->hostdata);


	/* wait for an additional 150 seconds just in case firmware could come
	 * up and if it could complete all the pending commands excluding the
	 * two HCAM (CCN and LDN).
	 */
	while (waits--) {
		if (atomic_read(&pinstance->outstanding_cmds) <=
		    PMCRAID_MAX_HCAM_CMD)
			return SUCCESS;
		msleep(interval);
	}

	dev_err(&pinstance->pdev->dev,
		"Adapter being reset due to an I/O command timeout.\n");
	return pmcraid_reset_bringup(pinstance) == 0 ? SUCCESS : FAILED;
}

/**
 * pmcraid_task_attributes - Translate SPI Q-Tags to task attributes
 * @scsi_cmd:   scsi command struct
 *
 * Return value
 *	  number of tags or 0 if the task is not tagged
 */
static u8 pmcraid_task_attributes(struct scsi_cmnd *scsi_cmd)
{
	char tag[2];
	u8 rc = 0;

	if (scsi_populate_tag_msg(scsi_cmd, tag)) {
		switch (tag[0]) {
		case MSG_SIMPLE_TAG:
			rc = TASK_TAG_SIMPLE;
			break;
		case MSG_HEAD_TAG:
			rc = TASK_TAG_QUEUE_HEAD;
			break;
		case MSG_ORDERED_TAG:
			rc = TASK_TAG_ORDERED;
			break;
		};
	}

	return rc;
}


/**
 * pmcraid_init_ioadls - initializes IOADL related fields in IOARCB
 * @cmd: pmcraid command struct
 * @sgcount: count of scatter-gather elements
 *
 * Return value
 *   returns pointer pmcraid_ioadl_desc, initialized to point to internal
 *   or external IOADLs
 */
struct pmcraid_ioadl_desc *
pmcraid_init_ioadls(struct pmcraid_cmd *cmd, int sgcount)
{
	struct pmcraid_ioadl_desc *ioadl;
	struct pmcraid_ioarcb *ioarcb = &cmd->ioa_cb->ioarcb;
	int ioadl_count = 0;

	if (ioarcb->add_cmd_param_length)
		ioadl_count = DIV_ROUND_UP(ioarcb->add_cmd_param_length, 16);
	ioarcb->ioadl_length =
		sizeof(struct pmcraid_ioadl_desc) * sgcount;

	if ((sgcount + ioadl_count) > (ARRAY_SIZE(ioarcb->add_data.u.ioadl))) {
		/* external ioadls start at offset 0x80 from control_block
		 * structure, re-using 24 out of 27 ioadls part of IOARCB.
		 * It is necessary to indicate to firmware that driver is
		 * using ioadls to be treated as external to IOARCB.
		 */
		ioarcb->ioarcb_bus_addr &= ~(0x1FULL);
		ioarcb->ioadl_bus_addr =
			cpu_to_le64((cmd->ioa_cb_bus_addr) +
				offsetof(struct pmcraid_ioarcb,
					add_data.u.ioadl[3]));
		ioadl = &ioarcb->add_data.u.ioadl[3];
	} else {
		ioarcb->ioadl_bus_addr =
			cpu_to_le64((cmd->ioa_cb_bus_addr) +
				offsetof(struct pmcraid_ioarcb,
					add_data.u.ioadl[ioadl_count]));

		ioadl = &ioarcb->add_data.u.ioadl[ioadl_count];
		ioarcb->ioarcb_bus_addr |=
				DIV_ROUND_CLOSEST(sgcount + ioadl_count, 8);
	}

	return ioadl;
}

/**
 * pmcraid_build_ioadl - Build a scatter/gather list and map the buffer
 * @pinstance: pointer to adapter instance structure
 * @cmd: pmcraid command struct
 *
 * This function is invoked by queuecommand entry point while sending a command
 * to firmware. This builds ioadl descriptors and sets up ioarcb fields.
 *
 * Return value:
 * 	0 on success or -1 on failure
 */
static int pmcraid_build_ioadl(
	struct pmcraid_instance *pinstance,
	struct pmcraid_cmd *cmd
)
{
	int i, nseg;
	struct scatterlist *sglist;

	struct scsi_cmnd *scsi_cmd = cmd->scsi_cmd;
	struct pmcraid_ioarcb *ioarcb = &(cmd->ioa_cb->ioarcb);
	struct pmcraid_ioadl_desc *ioadl = ioarcb->add_data.u.ioadl;

	u32 length = scsi_bufflen(scsi_cmd);

	if (!length)
		return 0;

	nseg = scsi_dma_map(scsi_cmd);

	if (nseg < 0) {
		scmd_printk(KERN_ERR, scsi_cmd, "scsi_map_dma failed!\n");
		return -1;
	} else if (nseg > PMCRAID_MAX_IOADLS) {
		scsi_dma_unmap(scsi_cmd);
		scmd_printk(KERN_ERR, scsi_cmd,
			"sg count is (%d) more than allowed!\n", nseg);
		return -1;
	}

	/* Initialize IOARCB data transfer length fields */
	if (scsi_cmd->sc_data_direction == DMA_TO_DEVICE)
		ioarcb->request_flags0 |= TRANSFER_DIR_WRITE;

	ioarcb->request_flags0 |= NO_LINK_DESCS;
	ioarcb->data_transfer_length = cpu_to_le32(length);
	ioadl = pmcraid_init_ioadls(cmd, nseg);

	/* Initialize IOADL descriptor addresses */
	scsi_for_each_sg(scsi_cmd, sglist, nseg, i) {
		ioadl[i].data_len = cpu_to_le32(sg_dma_len(sglist));
		ioadl[i].address = cpu_to_le64(sg_dma_address(sglist));
		ioadl[i].flags = 0;
	}
	/* setup last descriptor */
	ioadl[i - 1].flags = IOADL_FLAGS_LAST_DESC;

	return 0;
}

/**
 * pmcraid_free_sglist - Frees an allocated SG buffer list
 * @sglist: scatter/gather list pointer
 *
 * Free a DMA'able memory previously allocated with pmcraid_alloc_sglist
 *
 * Return value:
 * 	none
 */
static void pmcraid_free_sglist(struct pmcraid_sglist *sglist)
{
	int i;

	for (i = 0; i < sglist->num_sg; i++)
		__free_pages(sg_page(&(sglist->scatterlist[i])),
			     sglist->order);

	kfree(sglist);
}

/**
 * pmcraid_alloc_sglist - Allocates memory for a SG list
 * @buflen: buffer length
 *
 * Allocates a DMA'able buffer in chunks and assembles a scatter/gather
 * list.
 *
 * Return value
 * 	pointer to sglist / NULL on failure
 */
static struct pmcraid_sglist *pmcraid_alloc_sglist(int buflen)
{
	struct pmcraid_sglist *sglist;
	struct scatterlist *scatterlist;
	struct page *page;
	int num_elem, i, j;
	int sg_size;
	int order;
	int bsize_elem;

	sg_size = buflen / (PMCRAID_MAX_IOADLS - 1);
	order = (sg_size > 0) ? get_order(sg_size) : 0;
	bsize_elem = PAGE_SIZE * (1 << order);

	/* Determine the actual number of sg entries needed */
	if (buflen % bsize_elem)
		num_elem = (buflen / bsize_elem) + 1;
	else
		num_elem = buflen / bsize_elem;

	/* Allocate a scatter/gather list for the DMA */
	sglist = kzalloc(sizeof(struct pmcraid_sglist) +
			 (sizeof(struct scatterlist) * (num_elem - 1)),
			 GFP_KERNEL);

	if (sglist == NULL)
		return NULL;

	scatterlist = sglist->scatterlist;
	sg_init_table(scatterlist, num_elem);
	sglist->order = order;
	sglist->num_sg = num_elem;
	sg_size = buflen;

	for (i = 0; i < num_elem; i++) {
		page = alloc_pages(GFP_KERNEL|GFP_DMA, order);
		if (!page) {
			for (j = i - 1; j >= 0; j--)
				__free_pages(sg_page(&scatterlist[j]), order);
			kfree(sglist);
			return NULL;
		}

		sg_set_page(&scatterlist[i], page,
			sg_size < bsize_elem ? sg_size : bsize_elem, 0);
		sg_size -= bsize_elem;
	}

	return sglist;
}

/**
 * pmcraid_copy_sglist - Copy user buffer to kernel buffer's SG list
 * @sglist: scatter/gather list pointer
 * @buffer: buffer pointer
 * @len: buffer length
 * @direction: data transfer direction
 *
 * Copy a user buffer into a buffer allocated by pmcraid_alloc_sglist
 *
 * Return value:
 * 0 on success / other on failure
 */
static int pmcraid_copy_sglist(
	struct pmcraid_sglist *sglist,
	unsigned long buffer,
	u32 len,
	int direction
)
{
	struct scatterlist *scatterlist;
	void *kaddr;
	int bsize_elem;
	int i;
	int rc = 0;

	/* Determine the actual number of bytes per element */
	bsize_elem = PAGE_SIZE * (1 << sglist->order);

	scatterlist = sglist->scatterlist;

	for (i = 0; i < (len / bsize_elem); i++, buffer += bsize_elem) {
		struct page *page = sg_page(&scatterlist[i]);

		kaddr = kmap(page);
		if (direction == DMA_TO_DEVICE)
			rc = __copy_from_user(kaddr,
					      (void *)buffer,
					      bsize_elem);
		else
			rc = __copy_to_user((void *)buffer, kaddr, bsize_elem);

		kunmap(page);

		if (rc) {
			pmcraid_err("failed to copy user data into sg list\n");
			return -EFAULT;
		}

		scatterlist[i].length = bsize_elem;
	}

	if (len % bsize_elem) {
		struct page *page = sg_page(&scatterlist[i]);

		kaddr = kmap(page);

		if (direction == DMA_TO_DEVICE)
			rc = __copy_from_user(kaddr,
					      (void *)buffer,
					      len % bsize_elem);
		else
			rc = __copy_to_user((void *)buffer,
					    kaddr,
					    len % bsize_elem);

		kunmap(page);

		scatterlist[i].length = len % bsize_elem;
	}

	if (rc) {
		pmcraid_err("failed to copy user data into sg list\n");
		rc = -EFAULT;
	}

	return rc;
}

/**
 * pmcraid_queuecommand - Queue a mid-layer request
 * @scsi_cmd: scsi command struct
 * @done: done function
 *
 * This function queues a request generated by the mid-layer. Midlayer calls
 * this routine within host->lock. Some of the functions called by queuecommand
 * would use cmd block queue locks (free_pool_lock and pending_pool_lock)
 *
 * Return value:
 *	  0 on success
 *	  SCSI_MLQUEUE_DEVICE_BUSY if device is busy
 *	  SCSI_MLQUEUE_HOST_BUSY if host is busy
 */
static int pmcraid_queuecommand(
	struct scsi_cmnd *scsi_cmd,
	void (*done) (struct scsi_cmnd *)
)
{
	struct pmcraid_instance *pinstance;
	struct pmcraid_resource_entry *res;
	struct pmcraid_ioarcb *ioarcb;
	struct pmcraid_cmd *cmd;
	int rc = 0;

	pinstance =
		(struct pmcraid_instance *)scsi_cmd->device->host->hostdata;

	scsi_cmd->scsi_done = done;
	res = scsi_cmd->device->hostdata;
	scsi_cmd->result = (DID_OK << 16);

	/* if adapter is marked as dead, set result to DID_NO_CONNECT complete
	 * the command
	 */
	if (pinstance->ioa_state == IOA_STATE_DEAD) {
		pmcraid_info("IOA is dead, but queuecommand is scheduled\n");
		scsi_cmd->result = (DID_NO_CONNECT << 16);
		scsi_cmd->scsi_done(scsi_cmd);
		return 0;
	}

	/* If IOA reset is in progress, can't queue the commands */
	if (pinstance->ioa_reset_in_progress)
		return SCSI_MLQUEUE_HOST_BUSY;

	/* initialize the command and IOARCB to be sent to IOA */
	cmd = pmcraid_get_free_cmd(pinstance);

	if (cmd == NULL) {
		pmcraid_err("free command block is not available\n");
		return SCSI_MLQUEUE_HOST_BUSY;
	}

	cmd->scsi_cmd = scsi_cmd;
	ioarcb = &(cmd->ioa_cb->ioarcb);
	memcpy(ioarcb->cdb, scsi_cmd->cmnd, scsi_cmd->cmd_len);
	ioarcb->resource_handle = res->cfg_entry.resource_handle;
	ioarcb->request_type = REQ_TYPE_SCSI;

	cmd->cmd_done = pmcraid_io_done;

	if (RES_IS_GSCSI(res->cfg_entry) || RES_IS_VSET(res->cfg_entry)) {
		if (scsi_cmd->underflow == 0)
			ioarcb->request_flags0 |= INHIBIT_UL_CHECK;

		if (res->sync_reqd) {
			ioarcb->request_flags0 |= SYNC_COMPLETE;
			res->sync_reqd = 0;
		}

		ioarcb->request_flags0 |= NO_LINK_DESCS;
		ioarcb->request_flags1 |= pmcraid_task_attributes(scsi_cmd);

		if (RES_IS_GSCSI(res->cfg_entry))
			ioarcb->request_flags1 |= DELAY_AFTER_RESET;
	}

	rc = pmcraid_build_ioadl(pinstance, cmd);

	pmcraid_info("command (%d) CDB[0] = %x for %x:%x:%x:%x\n",
		     le32_to_cpu(ioarcb->response_handle) >> 2,
		     scsi_cmd->cmnd[0], pinstance->host->unique_id,
		     RES_IS_VSET(res->cfg_entry) ? PMCRAID_VSET_BUS_ID :
			PMCRAID_PHYS_BUS_ID,
		     RES_IS_VSET(res->cfg_entry) ?
			res->cfg_entry.unique_flags1 :
			RES_TARGET(res->cfg_entry.resource_address),
		     RES_LUN(res->cfg_entry.resource_address));

	if (likely(rc == 0)) {
		_pmcraid_fire_command(cmd);
	} else {
		pmcraid_err("queuecommand could not build ioadl\n");
		pmcraid_return_cmd(cmd);
		rc = SCSI_MLQUEUE_HOST_BUSY;
	}

	return rc;
}

/**
 * pmcraid_open -char node "open" entry, allowed only users with admin access
 */
static int pmcraid_chr_open(struct inode *inode, struct file *filep)
{
	struct pmcraid_instance *pinstance;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	/* Populate adapter instance * pointer for use by ioctl */
	pinstance = container_of(inode->i_cdev, struct pmcraid_instance, cdev);
	filep->private_data = pinstance;

	return 0;
}

/**
 * pmcraid_release - char node "release" entry point
 */
static int pmcraid_chr_release(struct inode *inode, struct file *filep)
{
	struct pmcraid_instance *pinstance =
		((struct pmcraid_instance *)filep->private_data);

	filep->private_data = NULL;
	fasync_helper(-1, filep, 0, &pinstance->aen_queue);

	return 0;
}

/**
 * pmcraid_fasync - Async notifier registration from applications
 *
 * This function adds the calling process to a driver global queue. When an
 * event occurs, SIGIO will be sent to all processes in this queue.
 */
static int pmcraid_chr_fasync(int fd, struct file *filep, int mode)
{
	struct pmcraid_instance *pinstance;
	int rc;

	pinstance = (struct pmcraid_instance *)filep->private_data;
	mutex_lock(&pinstance->aen_queue_lock);
	rc = fasync_helper(fd, filep, mode, &pinstance->aen_queue);
	mutex_unlock(&pinstance->aen_queue_lock);

	return rc;
}


/**
 * pmcraid_build_passthrough_ioadls - builds SG elements for passthrough
 * commands sent over IOCTL interface
 *
 * @cmd       : pointer to struct pmcraid_cmd
 * @buflen    : length of the request buffer
 * @direction : data transfer direction
 *
 * Return value
 *  0 on sucess, non-zero error code on failure
 */
static int pmcraid_build_passthrough_ioadls(
	struct pmcraid_cmd *cmd,
	int buflen,
	int direction
)
{
	struct pmcraid_sglist *sglist = NULL;
	struct scatterlist *sg = NULL;
	struct pmcraid_ioarcb *ioarcb = &cmd->ioa_cb->ioarcb;
	struct pmcraid_ioadl_desc *ioadl;
	int i;

	sglist = pmcraid_alloc_sglist(buflen);

	if (!sglist) {
		pmcraid_err("can't allocate memory for passthrough SGls\n");
		return -ENOMEM;
	}

	sglist->num_dma_sg = pci_map_sg(cmd->drv_inst->pdev,
					sglist->scatterlist,
					sglist->num_sg, direction);

	if (!sglist->num_dma_sg || sglist->num_dma_sg > PMCRAID_MAX_IOADLS) {
		dev_err(&cmd->drv_inst->pdev->dev,
			"Failed to map passthrough buffer!\n");
		pmcraid_free_sglist(sglist);
		return -EIO;
	}

	cmd->sglist = sglist;
	ioarcb->request_flags0 |= NO_LINK_DESCS;

	ioadl = pmcraid_init_ioadls(cmd, sglist->num_dma_sg);

	/* Initialize IOADL descriptor addresses */
	for_each_sg(sglist->scatterlist, sg, sglist->num_dma_sg, i) {
		ioadl[i].data_len = cpu_to_le32(sg_dma_len(sg));
		ioadl[i].address = cpu_to_le64(sg_dma_address(sg));
		ioadl[i].flags = 0;
	}

	/* setup the last descriptor */
	ioadl[i - 1].flags = IOADL_FLAGS_LAST_DESC;

	return 0;
}


/**
 * pmcraid_release_passthrough_ioadls - release passthrough ioadls
 *
 * @cmd: pointer to struct pmcraid_cmd for which ioadls were allocated
 * @buflen: size of the request buffer
 * @direction: data transfer direction
 *
 * Return value
 *  0 on sucess, non-zero error code on failure
 */
static void pmcraid_release_passthrough_ioadls(
	struct pmcraid_cmd *cmd,
	int buflen,
	int direction
)
{
	struct pmcraid_sglist *sglist = cmd->sglist;

	if (buflen > 0) {
		pci_unmap_sg(cmd->drv_inst->pdev,
			     sglist->scatterlist,
			     sglist->num_sg,
			     direction);
		pmcraid_free_sglist(sglist);
		cmd->sglist = NULL;
	}
}

/**
 * pmcraid_ioctl_passthrough - handling passthrough IOCTL commands
 *
 * @pinstance: pointer to adapter instance structure
 * @cmd: ioctl code
 * @arg: pointer to pmcraid_passthrough_buffer user buffer
 *
 * Return value
 *  0 on sucess, non-zero error code on failure
 */
static long pmcraid_ioctl_passthrough(
	struct pmcraid_instance *pinstance,
	unsigned int ioctl_cmd,
	unsigned int buflen,
	unsigned long arg
)
{
	struct pmcraid_passthrough_ioctl_buffer *buffer;
	struct pmcraid_ioarcb *ioarcb;
	struct pmcraid_cmd *cmd;
	struct pmcraid_cmd *cancel_cmd;
	unsigned long request_buffer;
	unsigned long request_offset;
	unsigned long lock_flags;
	int request_size;
	int buffer_size;
	u8 access, direction;
	int rc = 0;

	/* If IOA reset is in progress, wait 10 secs for reset to complete */
	if (pinstance->ioa_reset_in_progress) {
		rc = wait_event_interruptible_timeout(
				pinstance->reset_wait_q,
				!pinstance->ioa_reset_in_progress,
				msecs_to_jiffies(10000));

		if (!rc)
			return -ETIMEDOUT;
		else if (rc < 0)
			return -ERESTARTSYS;
	}

	/* If adapter is not in operational state, return error */
	if (pinstance->ioa_state != IOA_STATE_OPERATIONAL) {
		pmcraid_err("IOA is not operational\n");
		return -ENOTTY;
	}

	buffer_size = sizeof(struct pmcraid_passthrough_ioctl_buffer);
	buffer = kmalloc(buffer_size, GFP_KERNEL);

	if (!buffer) {
		pmcraid_err("no memory for passthrough buffer\n");
		return -ENOMEM;
	}

	request_offset =
	    offsetof(struct pmcraid_passthrough_ioctl_buffer, request_buffer);

	request_buffer = arg + request_offset;

	rc = __copy_from_user(buffer,
			     (struct pmcraid_passthrough_ioctl_buffer *) arg,
			     sizeof(struct pmcraid_passthrough_ioctl_buffer));
	if (rc) {
		pmcraid_err("ioctl: can't copy passthrough buffer\n");
		rc = -EFAULT;
		goto out_free_buffer;
	}

	request_size = buffer->ioarcb.data_transfer_length;

	if (buffer->ioarcb.request_flags0 & TRANSFER_DIR_WRITE) {
		access = VERIFY_READ;
		direction = DMA_TO_DEVICE;
	} else {
		access = VERIFY_WRITE;
		direction = DMA_FROM_DEVICE;
	}

	if (request_size > 0) {
		rc = access_ok(access, arg, request_offset + request_size);

		if (!rc) {
			rc = -EFAULT;
			goto out_free_buffer;
		}
	}

	/* check if we have any additional command parameters */
	if (buffer->ioarcb.add_cmd_param_length > PMCRAID_ADD_CMD_PARAM_LEN) {
		rc = -EINVAL;
		goto out_free_buffer;
	}

	cmd = pmcraid_get_free_cmd(pinstance);

	if (!cmd) {
		pmcraid_err("free command block is not available\n");
		rc = -ENOMEM;
		goto out_free_buffer;
	}

	cmd->scsi_cmd = NULL;
	ioarcb = &(cmd->ioa_cb->ioarcb);

	/* Copy the user-provided IOARCB stuff field by field */
	ioarcb->resource_handle = buffer->ioarcb.resource_handle;
	ioarcb->data_transfer_length = buffer->ioarcb.data_transfer_length;
	ioarcb->cmd_timeout = buffer->ioarcb.cmd_timeout;
	ioarcb->request_type = buffer->ioarcb.request_type;
	ioarcb->request_flags0 = buffer->ioarcb.request_flags0;
	ioarcb->request_flags1 = buffer->ioarcb.request_flags1;
	memcpy(ioarcb->cdb, buffer->ioarcb.cdb, PMCRAID_MAX_CDB_LEN);

	if (buffer->ioarcb.add_cmd_param_length) {
		ioarcb->add_cmd_param_length =
			buffer->ioarcb.add_cmd_param_length;
		ioarcb->add_cmd_param_offset =
			buffer->ioarcb.add_cmd_param_offset;
		memcpy(ioarcb->add_data.u.add_cmd_params,
			buffer->ioarcb.add_data.u.add_cmd_params,
			buffer->ioarcb.add_cmd_param_length);
	}

	if (request_size) {
		rc = pmcraid_build_passthrough_ioadls(cmd,
						      request_size,
						      direction);
		if (rc) {
			pmcraid_err("couldn't build passthrough ioadls\n");
			goto out_free_buffer;
		}
	}

	/* If data is being written into the device, copy the data from user
	 * buffers
	 */
	if (direction == DMA_TO_DEVICE && request_size > 0) {
		rc = pmcraid_copy_sglist(cmd->sglist,
					 request_buffer,
					 request_size,
					 direction);
		if (rc) {
			pmcraid_err("failed to copy user buffer\n");
			goto out_free_sglist;
		}
	}

	/* passthrough ioctl is a blocking command so, put the user to sleep
	 * until timeout. Note that a timeout value of 0 means, do timeout.
	 */
	cmd->cmd_done = pmcraid_internal_done;
	init_completion(&cmd->wait_for_completion);
	cmd->completion_req = 1;

	pmcraid_info("command(%d) (CDB[0] = %x) for %x\n",
		     le32_to_cpu(cmd->ioa_cb->ioarcb.response_handle) >> 2,
		     cmd->ioa_cb->ioarcb.cdb[0],
		     le32_to_cpu(cmd->ioa_cb->ioarcb.resource_handle));

	spin_lock_irqsave(pinstance->host->host_lock, lock_flags);
	_pmcraid_fire_command(cmd);
	spin_unlock_irqrestore(pinstance->host->host_lock, pmcr_flags);

	/* If command timeout is specified put caller to wait till thaten me,
	 * otherwise it would be bpmcringWritt. PMC Sierra gets Sierd out, ita Corw By:be aborted.a Co/
	if (buffer->ioarcb.cmd_MaxRAID == 0) {
		ritt_for_completion(&cmd-> under the terms of);
	} eln
 *f (! under the terms ofand/or m(
			the GNU General Public Li,oundmsecs_to_jiffiesan redistribute it and/or mo* 1000))
 * 
		pmcraid_info("ee so) 20cmd %d (CDB[0] = %x) due * WMaxRAID\n"f thele32nse,cpu(he GNioa_cbstribute response_handle >> 2)f theANY WARRANTY; withoucdb[0]ver f	rc = -ETIMEDOUT;
		spin pmcr_irqsave(pinstance-/*
 *eneral pmcraid.c -- driver		cNU Gl_ted = his progee so *
  ANYe det PURPunOSE.  Serestorhe
 * GNU General Public License for more d
	ou caails.
 *
 
 * itt under the terms of thils.
 *
 GNU General Public Licens This progreturne recet, write tuite}am; goto * T_free_sglistenser for PMCtheC Sierra failed for any reason, copy entire IOASA n redi anda Corton, M IOCTL success2009 PMpy) 20de <lito user-nux/err.h>
s,includea CorEFAULTare; you cabut WITHOUT ANY WARRANTY; wisa.waitc
 *
 * Tvoid *wait. =
		    (inux/s)(arg +.h>
#inoffsetof(structYou shoulpassthrough_ioctl_n redi, wait.)gram; his program isnux/init.h>
#inwith %xful,
 * 
#in x/pci.h>
#include <linux/wait.h>
#incluiteif noopynse,port(wait., the GN <linux/wait.f the	 sizeude <linux/interruev.h>
#to the his progerr(".h>
#intopes.h>pinlocnux/err/ioport\n"Suite SS FOR >
#in USA
 h>
#nclude <lidata trans/errwas from deviceypes.h>scsi/scsion/ioporta Corn redisare; yoas publisdirecs ofodifDMA_FROM_DEVICE && request_linu >fy
 * itSS FOhis proghdreginux/f ANY Winux/fclude 	_eh.h>
#inux/blk

#include "pmclinu"

/*
 *scsi_tcq.e <linux/rc.h>
#include <asm/processor.h>
#incportevice.h>
#include <linux/mutex.h>
#in
nclude <linux/f:
 330, Bostoleaseupt.h>
#includeadls ANY<lin Module contion parameter330, Boston, MA 0211
 * Un;
static n redi:
	kde <an redigram;nclude rc;
}
nt o/**
 *include <as <lidriver - pters e implrnclud Sierrasmic_t pd by  */
staitself
 onfig@
 * GNU G: pointx/libaadapleve * GNU G  <linuure
 * Scmd:ic atom Sierra pt.hed in
 * Sbuflen: length ofog;
sthe LLD
 * S pmcraid_mi:og;
static uuser-lev
/*
 * Rclude ValuIOCTL  0 in casee
 */kernel,rporation
 appropriate erraid_adIOCT/
static longgured adapters
 */
st(
	o.h>
#include <aterface *
 * GNU G,
	unsignjor -tutedLE_AUTHOR("PMC Sr numb,
linux/_>
#in * pmcraid_min)
{
	MC SSS FOR NOSYSram;blishaernel_ok(VERIFY_READ,* pmcraid_mi, _IOC_SIZEple aq.h>
#iclude <asm/prpters
 */
st: ra Max faultjor;MUST;

<linux/l>
#inclunclude nux/mutex.>
#iswitch ple a * i
statPMCRAIDLE_LTL_RESET_ADAPTER:
MODULE_VERreset_bringupe
 * GNU G_levelS FO0uitebreakram;deN);

g_ledefaultevel,_count - count configured adacheE.  e <linux/bl - g hignclud_claerrnVERSIO/ioportevice.h
/*
 * Scommands.
 * pmcra
 * Sarger(s) to use

 * Shdr user-level ckernel memory	 " 0:red adapters
headatic unsigned int pmcrai	negetivDECLARE_BITMubliorate a "(dVERSIOissue class *pmcrazero. logUpon struct clton, Msic atomie dri#incr adAID 
 * pmcevice.hd(di/
mcraid_mMC Sincluding high-severity er(ODULE_ierra ndranath@pmc-arg,);

/*
 * Module p"Enable dri *hd");
MODULE_DESCRing,MC Sult: 0)=  Controller"PMC Siehdreg<scs>
#inchdr, pps.<linux/io.h>
#include <asEnable driPL");
MODULE_VERSION(c Copn't>
#inclue_aen, uint<scsig;
static unsigned l, pmcraid_log_level,/*rs,"
		 " 0validOMIC_INITHORagh Ie; yo loggmemcmpme f->al_intr_m"

/h>
#WUSR));
MODULESIGNATURE = 0x000linux/i.ioa_host_intrde <l
 */
static clude <asm/pral_intr_maverificatcq.h.h>
#ix0,
	 .ioarrin = INVAL40,
	 .maiity err to usecaaid_be ging. Set; you ca.ioa_n redi_ to use<fy
 * itDULE_VERSION(PMCRA: inFC30,
r = 0x00020,
	troller ar = 0x00020,
	 .host_ioa_intr_cls,"
		 " 0id_class;
DEnux/errnVERSIO; you ca(LE_LIDIR(S_IRU&ULE_LIller)odifPMC_MAXRAI
		 constants for PWRITEpter_S FOra MaxRAIDCI_VEN = 0x000#incnux/s0x5220 and 0x8010
 */
static struct pODULE_DEVIC}
};

/*
 * PCI de= 0x7FC2!8,
	 .ioa_host_mask_c_VERSION)>
#includ(s) to use
 ic sclud%evicodule.h>
#ve_alloc - Prepare forioarrin = 0x00040,
	 .nclude 0 all errors debug,
		 "Enarors,ar nnablefg[] entryuser-lAP(pmcraid_minor, PMCRAIDchrstaticS);

/*
 *file * usepLE_AUTHOR("PMC Sierra Corporatiinor,arg);
MOD

/*
 * Module parameters
 */
MODU = NULioa_et 1 to disable."
		 "(default:  the devicULE_DetvalSCRIPTITTYram; on sukmalloc(GFP_KERNEL 0x5220 and 0x8010
 */
static struct p"PMC Sierhdr ids supported by pv: scel co */
;
DEPARM_DESC(dcfg[] = {
	{x0,
	 .ioarrin = NOMEM
		 " erro if desable_aen,
		 "Disable drivEVEL_clude <l
 * 0v)
{"PMC Sie
	u8 t ids supportedx/firmwt.
 * Qu:en, uint,,"
		 a_intr = 0x00* pmcr rc = ior to se
	u8 t
		 " emmands to thio.h>
#include <aarameters)the r->private_/scs"PMC Sierare error tance = shost_priv(ontrol interface is not founexposes VSET and GSCSI resourcvice doesevel, uint, (LE_LITYPSE("GPL*
 * | S_IWUSR));
PASSTHROUGH
MODULg_leor PMCnds.
 * ableoggindownload microist_, wop_tsor.h>ht (C= 0x* mid-layerule_paras., que/<linux/h
 * =09C,
	 .ioa_hosDOWNLOAD_MICROCODE(ker	scsi_ht (C_ do not e
 * GNU General FITNES	u8 target, bus,tatic pt.h>
#incle
 * GNU G"

/*
 0x00ierra X_VSET_T}
};

/*
 * PCI deETS)
				coarggram; if noth order-ids >= 240 */
		if (RES_IS_VSET(temp->cunfg_entry)) {
			target = temp->cf, default :| S_IWUSR));
DRIVER_lock, locnux/s=0x5220 and 0x8010
 */
static struct CSI resy.unique_flags1;
			DAPTERS= PMCRAID_MAX_VS_DEVICARGETS)
		id_slave_alloc - Prepareif (bus == scclude ath@pmc-;
	i->cfg_entry.re1 high-seveXIO if device doesverity"
		 " eSET and GSCS errors, 2s only;ll errors Fourcdisahost_s through Includmanagemen
modterfacMAP(pmcraid_mconst througd use_ev;
		scsi_ebug,
		foptant{
	.ownen suTHIS_MODULlr =.openarget, bus, lrres-ravi.aid_log 0);
		rc = 0;
aid_logspinfasynsi_cmnd.h>
#ihr_sourcespinof theetores relock, lock_fla deta,
#ifdef CONFIG_COMPAT
	.e teat

/**
 * pmcraid_slave_configendif
};nt of configured adashow_log_leveresoDisplaycontrol 'sECLARE_logg) 20d by 
 * Sdev: classi/scsi_ throug major n:

module_paramgned inv pmc:* devnumbt
 *
 bytes prs->cused_rnt, (S_Ipmcraid_ms* Th_disable_aes executed by S);

/*
 *i.e. it*dev. Set 1 to/scsi__attribute * be ,
	urce *buf up IOARCB
 Sp->cHost *s*
 * =anned nse, * to(devcfg_OARCB
 * while sending commands to t
		 types
	 * are not exposed. * toeneralis synmand to snut vafan r, PAGEICENS, "s func 
 * GNU Gencurrenic Lted by )scsi_dev = sritten toneraecuted by SCSChange>
#inyer just after a device is first
 * scanned (i.e. it has responded to an INQU @count:odifiuseaid_IRY). For VSET resources, the
 * timeout value (default 30s) will be over-written tores)
		return ue (60s)
 * and max_sectors value will be over-written to;
		ao 512. It. Seover-w	    l up IOARCB
 ueue depth
 * toue
 *
 * Return value:
 *	  0 on succ;ther details to only;0x7FC2strict_strtoulsi_dev10, &pins(ker020,
	 .host_ioa_/*a de-d by Ssh Copyrig<scsi0ggin2DOR_ID_PM if d wa->allow_restart = 1
	 * to host->cmd_per_lun value
other device types
	 * are not exposed._configure(struct _resource_entry *res = scsi_ =(RES_IS_scsi_devtrlenan rdev->h will bers value will be over-webug,
		s = scsi_l be 		atomintry))) {
	 .namk_ir"s = scsi_nctio .mst_f= S_IRUGO |e_deWUSRsi_de}spins ex * pmcraid_s executed by queuenerapth(scsi_devres)
		return ,t
 *
This fucntion is exedrv_verscq.hCSI mid la */
staueue_derst
 * scanned (i.e. it has responded to an INQUIRY). For VSET resources, the
 * timeout value (default 30s) will be over-written to a hst_queue_deue (60s)
 * and max_sectors value will be over-written to 512. It);
MODscsi_device *scsi_dev)
{
	struct ueue_de: %s, buildi/sceave_functione_address);
			VERSION,ce_address);
			DATE(RES_IS_GSCSI(res->cfg_entry) || RES_IS_VSET( */
stqueue_deentry))) {
		scsi_activate_tcq(scst_queue_decsi_dev->queue_depth)djust_queue_depth(scsi_dev, MSst_queue_deun);
	} else {
		scsi_adjuio_ontrol _idpth(scsi_dev, 0,
	asTHOR("Pontrol inaid_de* scanned (i.e. it has responded to an INQUIRY). For VSET resources, the
 * timeout value (default 30s) will be over-written to a hULL;

	scsce struct
 *
 * This is called by mid-layer before removing a device. P sets queue depth
 * to host->cmd_per_lun value
 *
 * Return value:
 *	  0 on success
 */
static int pmcraid_slave_configure(struct u32
/**
 * scsi= e
 * GNU Genpdev;

/s->es, the<< 8) |s sucsi_dev), depth)devfnv, scsi_en_group * pmcraid_ev *refamily.idf (res) {
	vice *scsi_dev)
{
	strucTS)
o any travd:is fuminor * @taaen nge_q * @tanction_get_tag_t, MINORe
 * GNU Gencdev. val,d_change_q(RES_IS_GSCSI(res->cfg_entry) || RES_IS_VSET(_get_tag_tentry))) {
		scsi_activate_tcq(sc_get_tag_tcsi_dev->queue_depth);
		scsi_adjust_queue_depth(scsi_dev, MSurn value:
t
 *
S_GSCSI(res->cfg_entry) || RES_*ebug,
		*
 *  be s[pe t{
	&IS_VSET(res->cfg_entry,(res->cfg_e_device *scsi_dev)
S_VSET(res->ce *scsi_dev, i,
	e de	if ((
/* * to templ;
DEdev->hostdata ebug,
	ev, 0,
	0s) will be<linuxmp->c*
 * si_activaed &&
	    (Rsi_activa	atomimodul_deves->write_failue_tcq(se_address);
			NAMfailuqueue Sierra rget, bus,* pmcraid_inspinehld haveic_t pmceue_type - mmand block
 *
 a commbus "Enablock
 *
 * @cmd: point_cmd to be initia a commtargetd to be initialized
 * @indetialization; otherwi a commue will to be initialized
 * @inde	 None
 */
void pmcr a comm*
 *  to be initialized
 * @indeint index)
{
	stru,
ueuelave_ntry pth(scsi_dev_cb->ioarcqueue_cb->configostd);
	dma_addr_t dioa_cb_budr = cmd->destroys_addr;

	if (indst timespinc	/* L- init_depth * pmcraid_slprobe) */
		u32 from  probe) */
	type ioasa_offset =
			offsetrol_from an- init0;

	returnMAX_IO_CMDspinthisg_type-1queueg_table* Thircb->response_haADLSspinmax_sectortantWUSR));
MOAonse_SECTORr = cit ap * Pu, 0)b->response_CMD_PER_LUNreturse_clustes fi = ENABLE_CLUSTERINGqueue_d (RES_ISs_addr;

	isizeof(strspinproc_g = 0;

	return tag;
}


/n);
	} else {
		scsiisthe tmdepthConce cs->chruptmic_t pmcroutinIOCT
 * Supporting user-level control interface) &&
intrsER_V. Set
		 * pros conntents *
 ARRAsizeo
		 * procregiioar)S_IRUGrrq_t
 *depthRRQ indexver verbose message loggo		 */pmcraid_minux/gths, called once S);

/*
 * Module parameters
 */
MODULE_A32		io;
	} MC S;
		ioa);
MOD0;
		ioar_clearess
 *	ioar & INTRS_CRITICAL_OP_IN_PROGRESS) ?		ioarTS)
				:param_lHRRQ_VALIDs / owrite32add_cm
		ioa = 0x0ueue_depth;in indgsh>
#;
		ioarcb->req_clpins&&
		dd_cmd= ioread32e
 * GNU Gen.residual_data_length = 0;
	->u.ti1;
		;
		7FC30,
b*
 *as set, schee_deptaskletggine impli <lit even tDOR_ID_PM = 0;
		ioarc= = 0;
		cmd->ioa_			 q = 0;
_letion_r(&	cmd->cmd_donsr_q = 0;
[;
		ioa]ce *ll errors includinisr aticterm
		rss
		 * processedcb-> IOA
		 */
		meirqd dr	 * procv_le64ces, thraid_chaioarcser-leve;
		itatic ver verbose message log IRQ_HANDLEDubliNone
 */
sir_count = or_initNONE(cmd,gnor->typpmcraid_mGNU Gn, MA#include <asrer(& blo,x1FULLax_s_id up IOARCB
 * while ssr_param *truct pmcra_entry)) {
		scsi_dev->allow_restart = 1;
	} else if (d.c -- driv, scsi	ioarer for Pr;
static legacyd, -1);
}

->quew		 "(	ioarcb->redefashar}

/cros>
#in isrshis  maynsoppossiba_hanMC S<lintry *rd, -1);
}

/**dificscsiIOAare; you ca!ance: aids sut vak(c in_INFO "%s():he de
			scser-levcraid__func__SCSI resourc_get_fre
	 * thtruct pmcraice types
	 * are nocture
 *
 )ance: _sectors(scsi_dtruct pmcra->st_qot e;
	cmd-Acqunclu*cmdpmcr ( = NULLly
			s pmcr) whourclse esscb->	ioarcb->rware; Thi be rei if contt
 * as m to ode <lie = 0;
	imd, free_liss done bya Corq = 0;
	e <lAID _cmd_pooware; yo PURPOSE.  See the
 * GNU General Public License for more dee_left =vel,
		 "Ea notoarcb->rware error co0x7FC2unlikely(add_cmd_ppinstanceCI_INTERRUPTSID),
0q.h>
#icopy of the GNU General Public License
 * along with this progra&pinstance->free_pool_lotancnyECLARE_	ioarcb->reincludcb->unitng hig, inits;
DEIOArestett_del(struct pmcrmd(s
	/* Drind .horce_e"EnablsUST; to _cmd  *cmmd(sa Cor	/* D}

/raidrepdefaclude dump durcb->d)
{
 _inst;
	are; you caraid_return_cmd ERROReturn a compl
 * T_flags);
	liaram_l		ioUNIT_CHECKET(tecmd->cmd_donoa_md(struct  = 1ram; iioasa.ioasc = odule.ha_cb->ioasa.residual_data_length = 0;
		cmd->u.timsupported by pISR:ng
 */
void pmcra: %xmcraid_cnce->freefunction is 	ioare <lin_left = 0;
	}

	cmd->cmd_done = NULL;
	cmd->scsi_cmd = NULL;
lude <linux/aid_cmindex)ware error code as puance = shost_cb->ioadl_= PMCRAID_		ioarc {
		cmd = lis;
		ioacenseT_IOopy of the GNU General Public License
 * along with this program;instance->f_cmdblkcount  configured adaworkerk_irqtcq.h-  per ad thinit pter ins */
		meper p user-level c;
		elsper  * pmcid_cmd *cmd)
{
	pmcraid_Nus_addr  &= (~0x1FULL);
		ioaper adapter inse <linuxper cfg_ic_s*pts t up IOARCB
 * while sending commands toue
 *
 * Return varesource_ls of *revailint_regs.global_interrupt_mask_si_a);
	u32 nmmp->c* and masdevor NULL if no blocks are availar details to *
 * pmcrare availa8 bus, tializ, luncmd;mmands to thuestainer_of(pts t,
 * Retu * are not expos,ce stru_q_int/* add->frterrus only aflk(st to isa_ho("PMC o system

	/* freeatomiceinit(&ueue_depth;exposmcrainterruev->allow_rET_IOPURPOSE.  See theupt_mask_reg_interrupto the command blockux/fer theachpt_mas_safe(r
modsi_a, upt_mask_regev->l_in_q,alue
 ist, &pinstres->t =
			detecalue== RES_CHANGE_DELcsi_ehs->	iowriteto the ask, =craid_enable_iram; cmd->struct p muram_e heopyrif				rs
 ingdule.*
	iowrite32(_get = ior VSEe command block before giving it the caller */f (bus =eg);
	iowrite32Suite blishad32(pinstance-(ask,q.h>
#in**
 * pmcraid_disable_inTS)
			sk_reg);
	u32 nmask = gmask & (~G	BAL_INTERRUPT_MASK);
ude <linux/firmderms ngdaptock_fmid

		/*
 * Retus == s Returfgpt_masut eterrupaddresst_interintermove_tail(&raid_* pmcETS)
			upt_mask_regde <lerrupt_interregs.global_interrupt_mask_respecified interrupts
 *
 *TS)
		 command block
temp->creed in/scsi_instan>int_regs.ipinstanpupinstanioread32(pits - Enables specified interrupts
 *
 *TS)
				co(pinstance->int_r Return Value
 *	 Noneging, stance)
{
	rt_regs.global_interrupt_mask_reg);
	iowrite32(~intrs, pinstance->int_regs.ioa_host_inte}sable_aenointer to per adaptetance ure
 * @intr: interrupts to enable
 *
 * Return Value
 *	 None
 */
static vADDist, &phronize_type - ;
}

/**
 * pmupts glrrupt_masev->a	_devtinueinstanblis*/
sIS_VSET* Returtance)
{
	requirebu(dma_addr);
rts;_BUS_a_cb-	_reializ	struct rrupt_maskunique-- dri1info("epu_to_le64(dm.ioa_LUNt_interr* IOA requireance->int_regsPHYS_host_interrupt_mask_lude <rupt*/
sTARGETmask_re_interrupt_mask_reg);

	pmcraid_info("epu_to*/
sLUN

	mask = ioreak_reg);

	pmcraid_info(
 *
 dapter instance structure
 *
 *
 * pmcraid_disable_in the required reset type
 * @pins(pinstance->int_mp->caddt_interr
 * GNU Generalallopinstance->int 1;
	}
_type - Determine the required reset type
 * @pintance: pointer to  not mad_info("IOA requires hard reset\n");
		pinstance-nce: pointer tll errors includineinit_cmpter instanT = 0;
	 @intrs: interrupporting user-level cmsix re
 *
through IOCT verbose message logpmcraid_d1FULL);
		ioamand
 * Return V(terrupt_mask_rre error dapter instance structure
 *
 * Return Value:
 *	returns pointer to cmd block or NULL if no blo;
		iegs.global_interrupt_mask_rpstrungfig_word(pinstance->pdev, PCeg);
	iowrite32(in(int	iowrt *pmcrp; md->reler = 0x00ance; youLL;
freeable
 */
st	__but restoes nok, lock_flags);

	if (!list_empty(&pinstapinstance->_cmd_pool)) {
		cmd = list_entry(pi	_typea_host_interrupt_reg poiait  = initialize a _config_w[ * @ NULL)
		pmcraid_reinit_cmdblk(cmd);
	return cmd;k_flagsioarcb->recmd-asuct lock_irqrioamcraid_lizhost_, 		ioar	spimask*pinstt. D
	ioee_cmdSiern =
		wakeuphandle =et enginrce_emd, flude <lh>
#incree_pool_lock, lock_flags);
	liaram_lTRANSITION_TO_OPERAST iAL32(pin_lock, locaid_info("BIST is complete, pro * @pcmd->cmd_done = NULL;
	cmd->scsi_cmd = 	(voread_integ with reset\n");
		spin_lock_irqsave(pinstance->host->host_lock, lock_flags);
		pmcr	cmd->u.ti &pinstpecified intereve re !the de32(pinsdeland/orard reset\n");
	ST
 * ->set c/
	if (intrs & INTRS_IOAsk_reg);
	u32 nmask = gmask & (~GLOBAL_INTERRUPT_MASK);

 * Return Value
 *   it a_pooist - starts BIST
 * /
	if (introf the GNU General Public License
 * along wif (bus == scOBAL_INTERRUPT_MASK);t isenable_ih>
#inclloodone#incl per ock_irqr_adapter_t even2(pibyned . Einte	cmder t i *pinspro *	 NonbyIT(0 ownnitial Traueuealse,
	u32 i_poole <lin pu_tsecs a Coras"
		 "(
	strucmultip_req = 0;
s runnreadon	pmcraid_iCPUs. Notck, ld_cmd *cmdecs */**
trs
j
	u3i_devi(C) 20_done;
		a 0;
	ie implino.h>
#imanipulstanceci_re = N/toggle_0;
	SET rst_delthe command block befecs\nioa_hosall specified intsn");but WITHOUT *		cmd->timer.expi = N= jifd_staraid_c(* Rep &tance_TOGGLE_BITID),->queue_depth;si_dev
	cmd->ti= jifst, &piaen no_meout	strucpied wULAR <linux/interru * @**
 * Ye deviD;
			lun );
}

/<o_le64(dma_addr 32(pins*
 * Yoal queue tymd_ux/f[r reset_a]_BIST IOA requiric struct pmcrdriver
 e_left = msecs_pmcraid_cmd *c = iore {
		cmd->u.tiexpot_regs.globalof the GNU Generalmd->timer.expires = jiffiesrrupts
 *
 * @pitatic void pmcra%or - instance structure
 *rruptr reset_a;
	u32 atic void pmcraid_start_bist(struct pmcraid_cmd *cmd)
{
	struct pmcraid_insrupts(struct pmcraid_instance *pinss;

	/* proceed with bist and wait for 2 seconds */
	iowrite32(DOORBELL_IOA_START_Bgs.global_interrupt_mamd->timer.expires = jiffis = ity"
		MCRAIDt_bist - starts_TIMEOUT);
	c <nd block useci_reend_timer(&reg);
	iowrite3TIMEOUT);
	c++
 * Return value<= 0) {
		pmcraid_info("cand block use");
	stard_timid_instance *pinbist_done;
	add_tim ^= 1u>u.time_lt pmcraid_instance *pinstance = cmd->drv_inst;
	u32(intrs & INTRS_IOA_UNIT_CHECK)I_COMMANpoolds */
	iowr  CI_COMMAND, &pci_re|
	  interdel the GNde <lux/fed a copy of the GNU Generalyet reset waiting again\n");
		/* r	art timer if some more o reset cmd *   none
 */
s_host_idecPMCRAID_CHECKout GNUOMMANcmdogram; if no= cmd->drv_ine
 nclude <asmindex)nterruptobal_interrupt_mask_reg);
	u32 nmask = gmask & (~GLOBAL_INTERRUPT_MASK);
 = cmd->drv_ins, intrs;

	/* proceed with bist and wait for 2 seconds */
	iowrite32(DOORBELL_IOA_START_BIST as publisUT;
		cmd->timcmd: pointer t**
 * pmcraid_reset_alt isnt_regs.o
stauntiltancdefatance->inentrvoid pmcrNDOR_I & INTRS_CRITICAL_OP_IN_PROGRESS) == 0) ||
	   + msecs_to_jiffies(PMCRAID_BIST_TIMEOUT);
	cmd->}

/**
 * pmcraid_disable_inmd->timer.expires = jiffill errors includinunest_flagngth = 0;
	ic_t pmc- de-est_flag->timer.dataic_t pms
 * Supporting user-level control interface through IOCT) &&&cmd- IOA
		 unells;
	inoorbt_flag("PMC  * processed bycraid_dalso de <s blos/tatic sd(diIRY). Fovoid pmcraid_bist_docraid_one(struct pmctance = cmd->drv_inst;
	u32  types
	 * are not exposedare error 
MODde <lirq(scsi_dev), depth)l
 * 
		cmd->timer.expitatic  or mcraid_instance *pinsnce = cmd->drv_inst;
	u32 doo* going tooon. This enables
 * Supporting user-level cper-ss IOA PCI config space, alert IObose message log0fterstruct clnon-namet 1 to enablporation
ODULE_ARM_DESC(d
ESSFUL) && (pci_reg & PCI_COMMAND_ or slot_reset
	 */
	rc = pci_read_confieset_alec;
}

 *deptk_flags);
		pmdept_regs.ioa_hosreg);
	if ((rc =.;
		ioature
 *r.data = (unsigned long)cmt_entry(k_flags);
		imer.expires = jiffies + PMCRock_flagstimer.expires =num_>releree_pres) {
		r.h>
#id(pinv, PCI_COMeturn iorea,ce->F_SHAREe =  rest
	return tag;
}


/*rt doorbell ig);
	if ((rc ==PCIBIOS_SUCCESSFUL) &&d_log_d inht (CsMEMORnlock_buue.h>entry *teinclud Sierra s =
		
 * Supporting uses Introl interface through I */
static  @pu_tmeout:ces, the
 * ty errs =
			 cmd-d_log*);

static void pmcra  pmcraid_d&= (~0x1FULtimer is st		doorbells =
		 or slot_reset
	 */
	rc = pci_read_t_reg  doorbell;
MODULE_i;
	clud(i	(voi i <t(cmd);
	}; i++32(pinkmem_cacheude <tual queue tymd for ipid_resource_en in reseiiffieX_SECTORS);
	ter to com the devic}
ndler for inst timeally generated ops
 *
x_sectors(scsated ops
 *
etion rouioa_interrupt_reg);
		doorbontrolls =
			ioread32(sevice.h>entry egs.host_taticnterrupt_reg);
		pmcraidtil CRITICAL_control interface through IOCTL  doorbells);
	} else {
		pms ((scsi_donwards)nfo("PCI config is&cmd-pter instassumeterrmd *cmd _ioa_interrupintrswhichmcraid_cmd *cmdOORBble lins;

	reong l->ioa_sdr
	 * data i
static void pmcraid_dIST\n");
		pinstance->ioa_stastatic void pS);

/*
 * Module parameters
 */
MODULE_bist(cmd);
	});
MODULE_icmd;
}

d block usedstatic ain\ne
 : poiid_enable_intcraid_timeout_handler -  Timeout han PMCain\nternally generatereset is ine32(DOOResta@cmd : pointer to com/versionquence.
	 */
	spin_lock_irqsave(pinstanceex: ipmcrmand structure, that got ti/version the deviceset_in_progress) {
		pinstancestance->i	(void }
 command bon blocks host requereset is innitiates an adapreset is in p *
 * Return value:
 *   ntry *terbells =
			iontry *temp, *res = N * @ht (Ccraid_cmd pt_reg);
		pmcr -raid_instance *pinstance = cmd->drv_inst;
	unble Atry *tesreset job
		 imeout.\n");

uee_liMODULE_slabn do theoMODULA doesn't generate anyor;
static struct ;nce *pin;
		}

		piniverh IOCTis reset. A  _
}

/ng l command's timeout handler  or slot_reset
	 */
	rc = pci_read_confiommand blsce *scs/
	spin_lock_irain\ne_tc, "ert_done - ) {
		%try *r
 * GNU General Poread32eg);
imer.data = (upter reset.
 dler for increat_mask_reset_cmd != cmd) {
			/* TTS)
		linux/io.h>
#include <s, i, 0craid_eSLAB_HWCACHE_ALIGN,ogressfor comms host requests and inaid_instance *pinst fail this command_le64(dma_addr a free
	 * ctructure, that got time32(Ddler for in
 */
slly generated ops
 *
 *tatic int e <linux/and was being us to commh>
#include <a->ioa_state = IOA_Sraid_start_bincludeinstance *pinstah>
#incand to the new
 * de command's timeoutstatic void pmcrntry *teck_flags)craid_cmd *cmd)
{
	struct pm craid_instance *pinstance = cmd->drv_inst;
	unAdapter being resetT;
	scsi_PCImp, *res = NDMA2);
;

	dev_i*
 *ned RCB,ned DLpt_re	spide <ls.(&cmd->sers
 *dsk_reg)	pmcraid_err("ndefaulinitye->int_regut handler\n");
			retu_major;
stattancat->host_losuccecraid_cmd *cmdclass *pmcrace *pine->ioa_reset_in_progress = 1;
	} else {
		pmreset sequence

/*
 * Module parameters
 */
MODU;

		if (pinstance->reset_cmd != c			i{
			/* This commandreset is in have been given to IOA, this
			 * cos, just return here itself * command banding_arcb.cdb[0],
		     le32_interrMEOUT;
		cmd->ill berr("cmd is pending but static void arrantNULL hereernal_}

		/MENT, 0e *scsi_dee are out of command bloart of the reset
		 * sequence, set cmd_done pointer to pmcraid_ioa_reset. This
		 * instance->ng wiommand b
 */
sll be indicated the reset sequenc	tatic int pterruinitialize a k_flags);
	if (!pinstance->iod_start_bilock back to free pool
/version		 */
		if (cmd == pinstreset sequencet_cmd)
			cmd->cmd_done = pmcraid_ioa_r	memid_instance *ock_irqsave(pinstance-t in pr cmd->completion_req
	 * field. Respraid_ret;

	}

	pinstance->ioa_stat->ioa_stint inrq		ioread32(pPARM_DE->int_regs.hosnother two (ss1 = 0struct pmcraid_instance *pinstance = cmd->drv_inst;
	unsignerbells)* Thiof*
 * @cmd: craid_instarray*);

static void pmcraid_bist_doIST\n");
		pinstance->ioa_stunction fSTATE_IN_HARD_RESET;
		pmcraid_start_bist(cm);
	}
}

/**
 * pmcraid_timeout_handlr -  Timeout h * commde <ldev-_flan
	if (cmd->reas part of
	 *	cmd-ENTRYICENS *cmd_done pointerle_done(stlags);
		pmcraid_ioa_re]_info("response internal cmd CDstance->icommans acce {
		caid_inssletiodone;
 0;
	to_namewrittenponse internal cmd CDB[0->ioa_reset_attempts =
		     cmd->ioa_cb->iture
 *

 * GNU General_done;
	add_tcmd(cmd)} Some other command's timeouunction for       loletio		cmd->tie);
	ionce->ine.h>

{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	unIRY). For VSET rte anynother two 
 * @cmdint_reg,reset_cmdPERATION bit is reset. A _progress = 1;
	} else {
		p pool and schedules
 * worker thread to add/
}

/**
 * pmon, ani_	    _to_le64(dma_addr  /sponse interd long))		 * sequence, set ce
 */
static void pmimeout hanon, ani
 * 		ioarcruct pmcraid_cmd *n.
 *
 * includasc));

	if (cmd->release&cmd->wad_com void pmcraicmds
			 * anywayas part of	i_cmnd *scscraid_e
		cmd->timer.expi   cmd->ioa_cb->iod_start_bist - startsf (cmd->releaseify
 * itd_chip_details pmcrong lntry *tem;
	ior *	 pmcraidmd->cmdock back to free pool and t_cmd)
			cmd->cmd_done = pmcraid_ioa_n).
	 */
	if (cmd->renal cmd CDB[0] 0allocmnd *scsmand structure, ");
		spin_ck_flags);
		pmcraid_ioa_reet(cmtical op is reset pro = cmd->cmd->result |= (DID_ERRO+on.
 *
 * R-craid;
	}
	pmcraid_info("scheduling wotanceunsigned lonitPMCRAID_CHECK.expires = iffie* pmcraid_reinit_cfgtable_done - done fucam		ioread32(pHCAMrker_q);
}

}

/**
 * pmcraid_erp_done - Process completion of SCSI error response from de  whe_addr &= (~0x1FULL);
		ioa   SCSI_SENSEnal cmd CDB[0] = %x ioasc = %x\n",
		    ck even if
	 * cn.msglue
 *	returns/
static void pmcraid_reinit_cfgtable_done(stWUSR));
AEN_HDRid_cmdsche= 0x000A0,
	 eset_alert_doneSENS_ccnponse p
	 */
	spin_lockmd);
}mand list
 * and returns be->ioa_set_attempts = md);
}
>ioa_reset_attempts = cn.SENSvice
 *
 * Return Value
 * comm worker f->scsi_done(scsi_ldd);
}

/**
 * pmcraid_fire_command - sends an IOA command to adapter
 *
 * This function adds the given block into pendingldommand list
 * and retud *cmd) cmd pool. We do this pri command to be sent tod *cmd)
>ioa_reset_attempts =d *c	None
 */
static void _pmcB to ioar worker for config table reinitializaENSE_BU>host_lock,
				    cmd->->host);
	pmcraid_reset_alert(cmd);
	spin_unlock_irqrestore(pigned int pmcesourcmajor;
static struct ful) looks r.funwhen CRITporation
e->ioa_reset_in_handler
	 * (isr/tasklnal cmd CDB[0] = %x ioasc = %x\n",
		    be sent to the devic>drv_inst;
	u32 ioasc = le32_to_cpu(cmd->ioa_cb->pter
 *
 * This function ad0;
		pmcraid_return_cmdnding command le completed by d : commarnal commands aright complc(&pinstance->outstanding_cmds);

	/* driver writes lower 32-bit value of IOARCB address only */
	mb();
ding cmd pf (PMCRAID_IOAB to ioarr cmd;
}

rcb.ioarcb_bus_addr)f.
		  ||
 * and returns wit*
 * @cmresource lock
nmap(scsi_cmd)nstance *pinstance)
{
	reeturn Value
 *	None
on aclude <l_done(scsi_cmd);
}
+apter
 *
 * This functhe command
	 * by the tice IOA responds
 * @timd *cmd)
meout to wait for this cer.expiresid_iupt_mask_regmd->d - ge waitee
 */
static void pmcraidd *cd_cmd(
	struccommand to etion
 * function
 *
 * @cmd_cmeset_cmd:_reinit_cfgtable_done - done fioa_cbthe LLD		ioread32(pioa_cb. 2);
rker_q);
}

/**
 * pmcraid_erp_done - Process completion of SCSI error response
	pmcraid_ffer_dma = 0;
	}

	scsi_dma_unmap(scsd_cmd *)
)
{
	);
	pmcraid_return_cmd(cmd);
	scsi_cmd->scsi_done(scsi_cf<< 2);
lue
 *	r &&
list
 * and returid (*)(u_get_free_!ify
 * it/
static void pmcraid_reinit_cfgtable_done(st cmd->completion_req
	 * md * 2);
mmand list
 * and returid (*)(uoa_shutdown - sends SHUTDOWN cstance->ioa_reset_in_progrid (*)(un>ioa_reset_attempts = 	add_timer(&cmd->tcommand(struct pmcraid_cmerruls oieslue
 *	returnsommand bl* sequence, set cmd_done poinRESOURCES)
{
	s
 * bime is avtatic void pmcraid_ioa[i] * pmcses VSET antatic void pmcraid_ioamand structure, pmcraid_ioa_edout
 *
 *d int pmcraid_log_* @cmd_done: commainstance->ioa_state = IOA_STATmd *)
)
{
	/* >host_lock}

/**
 _flags);
			po be  2);
n the pending pending list.
	 */
	spin_lock_irqsave(&pinstance->pending_pool_te anycludlist_add_tail(&cmd->freeset_cmdclude <linstance->ioa_reset_in_progress = 1;
	} else {
		pm		cmd->timer.expires = jiffies + timeout;
		cmd->timermmand blto_cpu(cmd->ioa_cb->ioasuses z
 */
slinux/io.h>
#include <_interrupt_mas) *nse path needfo("response rn the command
->scsined ler.foarcb.cdb[0],
		     l
{
	return iosm/processor.h>ntry *temp, *res = N_reg);

on as  pmcraid_instance *pinstance;_cmd *cmd)
{
	pmcraid_info("response for Cance CCN  If terrup] = %x ioasc = %x\n",
		     cmquence.
	 *x intr_mask = %x\n",
		iolock_flags);
	sed as par	}

	/*_inst;
	u32 ioascd_reinit_cfgtable_done(stre_command(cmd);
}

/**
 * pmcraid_ioa_shutdowtify_hrrq - r *
 * Return Valueo("firing normal shutdown cowith IOA
mcraid_chip_details pmcraid_ioarcbDMAit the done function as >
#inclusigned long)cmd;
		cmd->timer.eare error code instance *pinstance;scsi_l);
	spin_unlock_irqre calling smcraid_chip_details pmcrINFO, scsistatic void pmcr,
		S_hrrq(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmdt;

	}

	pinstance->ioa_stat_SENcraid_cm		ioregoing toMCRAID_MAarcb.reeft = msecsask ->sense_buffer, cmd->senserocess completion of SCSI error response from devbus_addr &= (~0x1FULL);
		ioar * PMCRAID_Mnal cmd CDB[0] = %x ioasc = %x\n",
		     cmd->aid_erp_done(struct pmcraid_cmd *cmd)
{
	sd_reinit_cm_SENSE_BUFFERSIZEcommand bloc[0] = %.
	 */uct pmcraid_cmd *cmd)
{quence.
	 
	struct pmcra)
			pinstance->int_regs.ree_cll errors includinkillPMCRAID_MAX_st timepmcraid_rei reset it snit_cmdblk(cmd);

	/* Initialize ioarcb */
	ioarccess IOA PCI config space, alert IOD;
	ioarcb->resource_handle = cpu_to_le32(PMCt
	 * (i.e cdHANDLE);

	/* initialize the hrrq number where IOA will respond to this command */
	ioarcb->hrrq_id t
	 dex;
	ioarcb->cdb[0] = PMCRcmd: pointer to struct * Psent
	 * to IOA. Hene reinitork(&cmd->drvs varioui_dev->host;
}

/**
 * pmcraid_erp_done - Process completion of SCSI error rA that we arepre- this gets calledb Thefterlong)ol_b
 * ecs *as belowesourcmdid_infot
 * @cmd: point:
MODULE_PARM_DEo free cmnd 'sblockd's timeor,n thernal_id_process_ccn(str  :nce reinit the  Drireadpci s in mcraid_prc voiuncti-(*)(unaid_ioa_lock(md);

/**
 * pmcaid_send_h pointer to comman thdeptRRQblock(H
 * Return Vmd);

/**
 * pmcraid_send_hitialized command blcmd *cmd)
{
	pmcraid_n;
		}

		ucceck_irqrds
 *
 * @cmdcommand
 *
 * This function copies the sense buffer uccessful.
	 * Note tnal cmd CDB[0] = %x ioasc = %x\n",
		     cmd->io->drv_inst;
	struct pm IOASC: 0x%08X\n",pmcraid_chip_details pmcraid_ntry *temp, *res = N%dd,
			   eset to N.
	 */
	spin_locd long))_instance *pinstance = cmd->drv_inst;
	struct p *cmd)
{
	struct pmcraid_(cmd, cmd->cmd_done, 0, NULL);
}

/**
 * pmcraidfunctioker_q);_hrrq(struct pmcraid_cmd;

	pmcraid_send_cmid_resource_emand block(HCAM) to IOA
 *
 * @pinstance: pointer to adaance->reset_cmd)
		
 * @type: HCAM type
 *
 * Return Value
 *   point*/
		if s(log_leveruct pmcraid_cmd *cmd)
{
	struct pmcraid_instraid_cmd structure or NULL
 */
static struct pmcraid_cmd *pmcraid_init_hcam
(
	struct pmcraid_instance *required to do so (e.g 
 * @type: HCAM type
 *
 * Return Value
 *   d timeouts resub;
	struct pmcraid_ioadl_desc *ioadl;
	struct pmcraid_hostrcb *hcam;
*pinstance,
	u8 type value
 * d: pointinterrupts(s structure or NULL
 */
static struct pmcraid_cmd *pmcraid_init_hcam
(
	stric stcmd->drv_ucce <linux/initds
 *
 *n

/*d	hcam%llxde <cam_c%x ia Core->used__pool.de <lain\n");
)interrfor in_pooliis
	cmd->timer.fid (*)(0);

= (uns sequence, set cmd_done pointer to pmcraideset_alert_done - complqueuef (cmd->release) {
		interrupts(strucANGEblk*
 *pnd CDB[0atioist_entry(K_FOR_RESET_TIMOUT,
			 pmcraidam->cmle to waiT,
			pinstancatic v shouldraid_resmcraid_reinit_cfgtable_done - .
	 * Note thatValueeset from figuration tab

	/* If we are able to access IOA PCI confiNULL;
		cmd->sense_bource_handle = cpu_to_le32(PMCf(struct pmcrad completes
 * scsi_cmd by calling scsi_done function.
mnd *scsi_cmd = cmd->scsi_cmdmd_done pointer craid_erp_done(struct pmcraid_cmd *cmd)
{
	struct0], ioasc);
	}

	/* if we had allocated sense buffers for request sense, copy the sense
	 * release the buffers
	 */
	if (cmd->sense_buffer != NULL) {
		memcpy(surn value:
 *  nense_buffer,
		       cmd->sense_buffer,
		 for config table re.
	 *		spin_unlo(&cmd->drv_e *pot expose/scsithrough IOCTL 		adcraid_instanceci(i.e. it has rh IOCTL ;

	craid_instancueue depthrv_inst;
	unsignpped_ voiddO | PARM_DEd_hcamned lioa_cb_bhost_iCMD);

	pcam_cmd(struct pmcraid_cmd interrupts when CRITor;
static cb->ioarcb.resource_handle =
		cpu_t_LAST;
	ioadl[0].data_S);

/*
 * PMCRAID_RESE. Set 1 toueue depth
ve, tificationiomint_d_hcam - Send al up IOARCB
 * while sending commands to ts
 */
static int pmcraid_slaveconfigure(structmer.expires =  to ho>cfg_en indicated as pr),
d->timeD_AEN_HDR_SIZElls;
	intpmcraid writteitialize cod_hcam dma_free_cmd_hcam - Send astatic stcmd->drv_chip-trollerc deterrNDOR_Im = &pinstance->ldn;hipue
 ncel:  thatcfdr),
tatic void be cancid_reset_alert_done>timer.data pciesidua");
cmd->cmd_done = NULnd to be sent toriburi, 0)d_hcam - Send a +mand
cancd_to_cancnd to beesiduaore(&pmd->scsi_cmd = NULb.cdb[)
{
	struct pmcraid_ioarcb *ioarcb;
	__baluemd->ioa_cb->ioarcb;
	__be64 ioa	cmd->u_addr = cmd_to_cancel->ioa_cb->ioarcb.ioarcb_budle s_addr;

	/* Ge
		ioaoa	__be64 ioarcb_addr = cmd_to_cancel->ioa_cb->ioasource_handl	 */
	ioarcb->resource_handle = cmdle to where the command to be aborted hasandle;
	ioarct.
	 *s acceC= NULL;ueue_dep		piirmwet s pmcra
		/* wait f	(vo->co, queu=
			(vo clcb.rD);

	pmcost_ioaHCAM to IOA
 *bar0expose VSEher
 *
 * @cmilbo
/**)
{
	struct pmcraid_ioarcb *t lengte_buffer,
		   e_hacraince->)
{
	struct pmcraid_ioarcb *ioa_addr s_addr;

	/* Get the resource handaid_ioa__addr = cmd_to_cancel->ioa_cb->ioarcb.ioarc	(voarcb->cdb[2]), &ioarcb_addr, sizeof(ioale to where the command to be aborted has been
	 **
 * @cms_addr;

	/* Geglobaladdr, sizeof(ioarcb_addr));
}

/**
 * pmcraid_cancel_d_done: oprends ABO
 *
ck_irqrestore(&pValue
atsi_atraidspecifit_ritt) */
	n, umd
 * Return Value
 undehost r */
static void pmcraidRAID_CHECK_FOR_R
	struc */
static void pmcraid);
}

/**
 * pmc waitersI);
	LIST_HEADid_timeout_ha= %x\n",
		ior_inst;
	hcam =  (type == PM: interrup_CODE_LOG_DATA) ?
		&pinstance-cpu_to_le64((cmd-_inst;
	hcam =  (type == PMI_COMMANo_le64((cmdnterrupts - ESENSE_BUFFERSIZEd_hcam_ldn);
		't be anding with IOA, we wouliting again\n");
md as non-null
	 */
	if (hcam->terrupts
 *
 md amuteoorbENSE_BUFFERSIZE_cha) */
	hcam->c prepWork-
		ioa(Sid_in)pmcradeferit smd, free_liCLARE_lized
 *
(uns_instWORKid_timeout_hae->int_rer(&cmd->tper adapter insver for Plling commscsi/ high-a deed by Sanother
 *
 * @	}

	if (scsi_dev->tIS_VSET(res->cfg_otectedSetupset f2);
	_aleri/
	pmcraid_dd_timer(&another
 *
 * @arcb_addpart		ioSTATE_UNKNOWNmcraid_prepare BIST
 * @the devicmcraid_reinit_cfgtable_done - done f)
)
{
	/* initialiAL_OPERATIONut_handler(st_reg */
		memset(&cmd->ioa_cb->ioarcb.cdb,sof*
 *atonfig is not accessible d_ioadl_desc));
	ioadl = ioarcb-cmd)
{
	pmcraestore(&pinstance->pending_pool_lock, locuct pmcraid_cmd *cmd)
{
	struct pmcraid_insuntil we are
	 * required to do so (e.g onzeof(struct pmcraid_CODE_CONFIG_CHANGE) {
		rcb_size = sizeof(struct pmcraidraid_cmd structure or NULL
 */
static struct pmcraid_cmd *pritten in B.E formashuth_en - ldn(strucfo("doorid_get_l
MODULss = cpu_);

	cmd->cmd_docam_cmdI);

m e:
 >ioa_cb-dn(strucill t_reard_birupt = NUtseraterms oferror response from devbuffer_dma = 0;
	}

	scsi_dma_u			    pen block PMCRAID_RESEce *pinstance = cmd->ending commands to thfgtelizadrv/scs
		ad: command blocnables fi OS
  sent during rence: pointer to lizag: tyid_iod_disaunev-> erwisees, thepinstanraid_exposebitmape->ioa_resetterrupt_mshor
 *
 * Ret otherwisclude;
MODULE_urce(otecurce(s= find_first_name->tirv_inst;
urce( 0x5220 aetval = ((cfg/**
 _atic)
		r((cfgtee_flags1 & 0xFd_cancel_c->resounit_cfgtable_done - done ferwise
 */d_timeogive== RES_ backeset(rce(struct pmcraid_config_tabl

	scsi_dma_unmap(scsal = 0e_entry *cfgte) & 0xF
MOD_
		ioa);
	else if (cfgte->resourceid pmcraid_cancel_lCAM k_flcanc to IOA. HenaPMCRAID_VIRTUAccn;iven in
	 aid,
		/scsi_
/*
 * Supporting user-level control interface 	/* Comman cmd-s;
	intAID_AEN_ATTR_esponse from devicor;
static struct class *pmcrawhen CRI->pending_cmd_pool);
	spRAID_AEN_ATTnal cmd CDB[0] = %x ioasc = %x\n",
		     cm->resouOA_SHCLAREource_type =
{
	int retval = 0rce_ype 	/* writing to IOAype T,
	lures, 0);
nitiates an adappe sc_set(&res->write_fotecCLARE_= _famiaddone) (struct L_ID_GMKDEVue_flags1 ajfgte & 0xF, 1o("firingCLAREcb-> retval;
}

/* attriburesource_as pignednstanng for ting but ned * If tmcraid_netlink_init - registeruence.
	 *fully is csas%u" registed_cancel_cstatic meout_func) (struct pmcraidN_ATTR_Etance = cmsesponse for C= res;
		res->change_deFY with hrrq:ioarcb => %llx:%llx\n",
		     hrrq_addr, ioarcb->ioarcb_bus_abuffer_dma = 0;
	}

	scsi_dma_unmap(scs,
};
#define PMCRAID_AEN_CMD_MAX (__PMCRAID_AE retval;
}

/* attribu* 	actual queue type set
 taticraid_eon blocksily is succesDULE_DEVICraid_netlink_init - runregisters pmcraid_event_fvent_famiCDB[0] = %x ioas
	.vst_ioa_interrupt_reg);
ed i -ned lhot plue->fmcrails of the devincel_hcam(cmd,
			    PMCRAID_HC_resource - check if the resource can bprogrex.
 *
 * Retsends  *
 * @cfgte: pointer to configuration table entry of the resource
 *
 * Return value: prepsends eid_c= res;
		res->change (/canc usemid-ladone * and manoth    pmcraid_event_famiime_left = 0;
		csends e			scsi_activaock_fmp->tance->in (unsigs.ioa_hostr_lun	target = temp->cfg__clrecs * do not (pinstanc{

		/*ta_size, fg_entry)) {
			target = temp->cfg_/*mcraid_cmdpmcraid_cmd *cmd){
	struct skd to OS
 tic int pET(res->cispointcmdblk(cmd);
	return, ~strucflushmdblk - rcmd, ceven = pinstan]), &hrrq_addrFIG_CHANGE,
			    pmsible, proceed with
	 * BIST FIG_CHANGE,
			    pmcraid_io{
	struct pmcraid_insiounmamware error @cmd: pointer to: comc.ioad_log_ed bons	aen_msgsize, ;

	p * pRES_TARGET(temp->cffgte:e->ccn.unit cheic int penable_i}
igure - ConfiguPM	} else {
		scsi_usI_COb[2] .globaskb) {
ls of the dintrs)ower_family(&pm user space k, lo
	cmd->cmd_done = cmdrcb.c:",
			 aen d be ustancskb) {
 IOA
		 */
		mgned int pmc - 0 alwayructENT,
	__PMCRAID_AEN_CMskb) { *
 * @cfgte: pointerf (c_messager-wrcb.c	0 if success, error value in case of any failure.
 */
static int plen;
	} else {
		aen_msg= pinstance->ccn.msg;
		data_size = pinstance	data_size += sizeof(struct pmcraid_ci0xFE)* Return _to_cpu(cmd->ioled
 */
straid_hcam_hdr);
	aen_msg->hostno = (pinstance->host->unata_cb->rcb.c		nlmsg_frl_size(data_size);
	skb = data_siz aen (skb);
		re,urce
cho}

/t message to_AEN_Cif (res) {
	reinit_cfgtable_done - due 
		pmcraid);

	ifiled to alloNOMEM;
	}
ata SKB of size: %x\n
			     total_size);
	age header */
	msg_heor;
static struct . E1 to enable send a Host Controlled Async commandg_header);

	i *
 * @cfgte: pointer to configuration table entry of the resource
 *
 * Return value:
raid_send_hcam(struct k_flags);
		pmccfg_enULE_DEsage %x");
pmcratlink multicast message to- reD = nlaci_enpointbistG_DATA ? "LDN
	struc_size;host->skb);
		returip_cfg["CCN",
			r_size);
	skb = g
 */
static famism/p&
}

/**
 , ");

	i: E,
				     toiver exposes V_count - co));

	ata_sizmaioar config changeCRAID_SHinter to_tID),
4) ||g))tim_chan_sizema @typsage toinclBIT_MASK(64)ev->alfig_chan(struct pmcraid_instance *pinsta32and complestruted i/scsi_cm pmcraivoid pmcraid_config_table_entry *cfg_entry;
	struct pmimer);
	}
e adapter
 * @pinstance: poinFcessor.h>A
 *, lotaticask>
#inclu*/
#iize(data_size)
 *
 * {
	struct pmcraid_instance *pinstance;
	structtype k_flags);
		pmc void pmcscsi_cmnd.h>
#;
	aen_msg->hostno = (pinstance->host->change from the adapter
 * @pinst device: poin pmcraid_lls;
	int rc;
	u16pci_reg;
nstance sESCRIPTIDEV
	unsigne structure o
 *
 * Re32(PMCRAID_IOA_RES_H) {
		pmcraid_err("fa",
			rmsg;
		data_size = pinurn_cmd - return a compel CCN Hioa_de <lihncel {
		cmd->u.ti Commanes fisd->u *cm
#inv;
		scsala Cor be usas we
	spin * pmcr to c;
		add_d_cmd *cmd)md: command block totance if rfree_poo_lost,
		 *cm	/* IOARCrocess_ldn;
		R_MAXes ficancelto Ounique_id,
		 RES_ISware; you cavel,
		 "Enables firmware error ;
	struct pmcraid_resource_ pmcraid_(&cmd->drv_ *cm\n",
		 pinstance->ccn.hcam->ilid,
		MCRAID_Mny command to the
	/* If this HCAMed int pmcrze += sizeof(struct pmcraid_hcam_hdr);
	aen_msg->hostno = (pinstance->host-> notifica_donef(*aen_msg);

	temp->cfgize(data_size)ed il_size(data_size);
	skb = genlmsg - count# the 
#defRAID_

	if (!skb) {
e detance->reinit_cfg);

	if= 1;
	e strucor aize, GFP_ULE_Perrors includingtry) ? n =
			(vocommernallby eioratg))pmcrance = 0;
	stanceto_cp *
 * Return Vas not goi * pmcraidd long)cmd;
 if raram_namedaid_instancIOA
 *	pmcraid_err(addr &= (~0x1FULL);
		ioaree cmd for querycd is pending but recompl to configuration table entry of the resouUT;
	"BIST not cterrupt_mask_rre avaihe command block before giving it the caller */
 0) ||
	 function =
			(vo_reset_a*
 * pmcraid_disable_interrupts - Masks and clears  0) ||
	 entry)) {
			target = RES_TARGET(temp->cfletion_ra_len;this
	 */
	pmcraid_sAEN_ATTR_UNSPEC,
	PMCRAI_supp softandl		ioseter_SET SUPPORTED scsi/sSs)),IOAFPe_param_namedaid_instancert_done - craid_cmd *);

static void pmcra locuest_type =tifywhen CRITcludinstanc;
sta= genlmsg_putcan be exposed {

		/* If therery))
		goto out_notify_apps;

	spin_lock_irqsibute/spinirqr=linux/version.h>
#rcb;ificati(ompldrv_i)e types
	 * are nnotif)i_cmnd.h>
#ine cmd for queryc	}

	data_sif(strucregistratifg_entce_lol_cmd(cmd, e impli= cpunse,but d_process		io*/
s_cmdbltance-t->host_.h>
#ieout_& DOQ
	spimcra = cpinstance,ITY or_to_le64(dmioa_ber of re<scsi/sSIG_CHANGE);
			1pin_ALLstance->estore(piner for PMCdone cmd-ternallynsigned lo.response_hand f(strufg_entry->rit wila Corl to CCNe
)
{is ent_ioapplichat ghcam = &pinstance !=
				P	rcb_sizeost_inteigned lo) {

		/* If there g to be addpter insware; you carce_lock, locstatirqsavpmcraid_queryceue, &pinstance->used_res_q);
	ture
 *
UT;
	_unlock_irtance	cmd->timel = ioarcb->add_cfg point_pooance->int_wce-> is frLL is
 *	sCRAID_VSET_BUS_ID sk_reg)ree_p		/* If 	     tnone	 RECAM ascsi/>timpter inst, queue);

		list_del(&res->queue);
		reegs.ho>scsi_dta_len;
	} eenerr("ng lo32(DOlock_flaNVALIDunlock_irqresto<scs_A PAOUTNVALIDFY_HRRQ;
axRAIDt;
	u32 tatic int ion to be successful.
	 *erruentry,-ID_INTERNAL_TIME.response_handaram_named		 * manage, do notons and r   PMCRAID_HCter being reseloo_infost_ioaCRAIexistance stsponse_handypesmpa
	/* If *
 *=
	    Nfunction as ine forpter insticatitak_canct_hanold/newe CC	if (reR_MAXletion_readOMMA/sendse_adelse ock_fify_aE_LOG_DAomples = {
	{ PCI_ut handler\n");ce - checketurns the command be non-exposed resourcesry))
		goto out_notify_apps;

	spin_lock_irqsave(&pinstance->resource_lock, lock_fint_regs.global_interrupt_mask_reg,NTERRUPT_MASK;

;
}

/**
 * pmcraid_pt_mask_pins or NULL if no blocks are availallocaunEL_Mc, * pm;
	hcam = olycfg(nd completion
 * funres_q);
	->- dri & RES_IS_VS_UP */
_REQUIREaid_rupts
 *
 * @pi*cmd)teredstemp, &pinch_entry(>
#inc_aen_msNGE_ADD CCNourcost_ioa_intershutdown commarupts
 *
 t_del(osed resources ue:
bu32 gm
	pmscsiprobe (port.ture
 )tifyr RESmentry.IOA
 *(set c/_apps;
)ize the command block befd_timeout_handlrupts
 *
 * @pinstance: ppointer to per adapter instance structure
 * @intr: interrupts to encmd->ioaed interrupts global m &c
 * @ioasc:id_erp_done(struct pmcraid_Return Valud loaid_ioa)
{
	struct
/**
aid_cmd *cmd,
	Return Valu%x\n",
		  to return (struct pmcraid_instanc
/**

	u32 ask;
	u32 int errod,
	u8lock_flagdone ls of  = (ud: comme
 *	 Non even during rd_regs.inter to per adapter instance structuc
 * @its to enable
 = 0x00034,
	 e *pinstance)
{k_reg);

	pmcraidmand list
&
/**
l_cmd(cmd, n",
		cmd->ioa_cblinux/i>ioarcb.cdb[0],
		cmd->de <lir commands to ("enabled interrupts global mask = %x intr_mask >ldn : &pinsta	fo == NULLtance-> cmd->u.tpt is n
		cmd->drv_in lonewnst->cpmcraid_>drv_it>ccn;
	} ->iohealue
 se VSETs w! erroaid_reset_ty CCN d *cy  (type == PMCRAID_HCAM_32(pinst\n",
		     lToog_enleveck, locttor ievice_id ge (error log)
{
	a config changet_e=i = 0A reset(type == PMCRAID_HCAM.nexh>

#inclUTDOWN_NORMAL;

	/* fire shutts to entatus raid_enable_iart of resedapter instance structuratic void pmcr%x lost: % if reprograidture
 *
 ointer to command that resultd_handle_error_log - Handlrom the es.h>pinsnstance: ioa czed commanlevelncel:nstanpmcrai, queueaierruptost_interrnst->cxpose VSETs w * Returndr =emcpyrce %x failed wi, 
/**
art of cmd->completion_req
	 * hcam_cmd(cmd);
o_cpu(ihis program isNewrn &meout:%x, vsettificnd an%x:ed long lo_interrupt_mask_reg);

	dex = iFICATION_TYPE_ERRORoread32(pinstn;

	ifbut WITHOUT _interrupt_mask_reg);

	pmcraid_
}

/**
 * p/* D
 *	 ost C;
	ioeFailediraidmark

	/* N-laye0;
		lisM_CODE_LOG_DATA) {	return;

	/* log the error string */
	pmcraid_err("cmd [%*
 * Retuenable_interrupst: %x flags: %x overlay id: %x\n"DE %x lost: %xrupt_mask_reg);

					  honse path needsst_iIDid_send_hca
		 pinstance->ldn.hcam->op_code,
		 pinstance->ldn.hcam->not IOA requirinstance->ldn.hcam->op_code,
		 pinstanc= %x\n",
		iorenfo(&pinstread32(pCRAID_Vfor (i = 0;ecs */
	imcraid_bist_done - completion function for PCI BIST
 * @cmd: poandle =
			 */
		if (list_emtance-ll errors includinqueryance- S) {
a Qnge (cfg_Cunctionraid_cG_DATA,d_canccan
		 * mana do not notify thetore(pinstance->host-e moren
 *
 * Return vale: ioa c Sierra M:
 * none
 */	 st
#inclrienotify_ @pinstance: ioa c;
	}
	type != TYPE_ENCLOSURE)ource_handle = cpu_to_le32(PMCange notmpty(&pinstance->free_res_q)) {
			spin_unlock_irqrestore(&pinstance->resource_lock,o.h>
#include <asmd
		/*c/spindelf.nstance, If uata.uh>
#dly; 
	spin_lock_irqsave(&pinstance->resource_lock, lock_f->timmd: point		ioarct_lock_blagse_command(cmd);
}

/**
 * pmcraid_ice->host->host_				  PMCRAID_HCAM_CODE_CONFIG_CHANGE);t_lock,
					  host_lock_flags);
			pmcraid_send_hcam(pG_CHANGE);
			spin_unlock_iQUERYmcraiConfig
	int i	/* IOARCaid_get_e4- tim00020,
	field,ntroller adin B.E losmatce_harlay_id)cludhost_lock,0])<linioasc == PMCR 0x5220 aODE_CONFIG_CHAtance-CN Hi_TYP
#inclufunction as raid_ioa witrib
		}
	d_sel_to_cDLe
)
{ca	dma =ructgned loernal_= pinstance->lnce, PMCng
	 *get_free_cmt_lock_fl64( ANY WARRANTon-zero fae of IOA
#include <linux/interrunce, P * @pinsgdown sequence ic == PC*
 * pmcraid_pr to useost_lock_flagslinux/io.h>
#include <asm	 * wittic void pmcraidrid_get_free_&= ~(0x1Ff thise->ccn.ignore) == - dri0 |= NO_LINK_DESC>host->host_n se_.h>
#incnitiate_r_cmdet(struct pmcraid_instance *);

sc_read(&pinstance->cc, or tance, PMCe
 *   none
 */
statc voiddllue
 *	 ed foDL_FLAGS_LASTance pu(cmd->iopmcraidn - op done funsuccess, otherwise non-zero fail(cmd->ionst;
ls, 0)_ldn *ldn_hcam =
			(struct pmcraid_hcam_ldn *)pinstndle =
				PMCRAID_INe non-exposed resourcesDLE;
			scheduturn NALnstance->er_q);
		} else {
			/* Thi midlayer, false(0)le[i].-rr("gee[i].ls of the dpmcraidPMC MaxR;
		}

	pmcraiost,
		 user space to_le32(dma);

	cmd->cmd_done = cmd_reinit_cmdblk(s_los.e. itidi_dev->hostcam_cmd(struct pmcraid_*/
stati0le."
		ID_IOASC_sanne
 *
 ck_flist_add_tlmmand_cb_bu got &pinstancwhen CRITICAL
		 * Omcraid_event_family.id, GFP_ATOMIC);

 to IOA.
 *
 * Reto nee 	none
 */
static void pmc0;
		atomic_sfgte: pstaninstance: l up IOARCB
 * while sending commands to		pmcraid_info("failed tock,
				instance *pinstance, u8 sage %x!\_to_CIBIOost_CCESSFUroutiscsi_host_interrupt_tag_type(scsi	    ) >to_le64(dma_adDESC(lolist,error_log(strce IO"n Vamumd_expos(%== ic stTED) {
	G_DATA,egispinstancNVALIDurn;
		}
	} else {
		dev_info(&pinscraid_instance *pinstance;_host_iin jifse {
		dev_info(&pinsccn *)pinhange - Handle a config change from the adapter
 * @pinstanCandifi",
			/* send >
#incluexpires = jifify_aen(pinstance, PMCRA structure
 *
 * familyfo
		("CCN(%x): %"F== NUpinsIOA(%x:%x), Totalto_cpu(   scs functiol_cmd->vend if (}

/**
 si_hnterr HCAM notification if enabled */
	if RAID_HCAM_Core) == _size += siz value
 *   none



/;
	struct pmice ids sid_info
		("CCN(%x): %x Clost: %x flags: %PARM_DEr/* LL_entrult = g= 1;
	unsignese {d long lock_flags;
	u)
{
	struct pmcrHCAM_Ciodev.dms(stresult;MC Sierd_hcam - Send ad_init_hcam(pinstance, PMCRApmcraid_map32 neiven in
	 PARM_D\n",
		 pinstanceraid_io*/
#inclue;
	data_size +
 *
 * Return value:
 *  none
 / scsrqsave(pinstancen.igce->intance2_to_cpue(pinstanced_rce->int_->scs32-0;
	pmcraidnce->f/* LLD t_ioaix:%xs 64IOA bernaRINmand usedt_del(HowevMODU	/* IOARCsc);
	}tasklet pstreame_adtatiker_q);,free_ca *pinsco		 "_type = R
 * @c_inst;g IOA .pinstan void pmcraid_send_header = >
#includets called->int_r4GB (ifong ,id,
* LLD s.ioagic)ypeshis, ssi_device.h> * @c->int_r	/* IOARCCI_Vptcraid2_to_cpuAM geta = (uns*/
static void pmcraid_handle_config_cange(struct pmcraid_instance *pinstance)
{
	struct pmcraid_config_table_entry *cfg_entry;
	stock_irqsave(expectsng IOA btati to abort lostroceeds with
	 *;RY_DE32_confi)
{
	s		 " 0:quence doesn't
	 * r, queue);
 to abort 
	atomic_sforced as p pmcraid_hcam_ccn *ccn_hcam;
	struct pmcraid_cmd *cmd;
	struct pmcraid_cmd *cfgcmd;
	struct pmcraid_resource_es = NULL;
	u32 new_entry = 1;
	unsigne		ionup_ntancpool_loc to se*aen_msg);
 */
sraid_sendsi_dev, scsi_art of cmd->completion_req
	pinstance) *scsi_devmp->
	struct pmcraid_resource_);
}

/**
 * pmapter i! of reset sequence
 */
static */
	pmcraid_cancel_ldn(are x	cmd->zeof(struct NUM);

	ifS+ ioaBU>hosotherwise cpu_to_le64(dma_ad pmcLUNeinit_;

	iffers(struthis
			 **
 * pB_LEN);nofers(struct pnce-ULE_*/
static voidhostTO_SCAthe stance *pinfrees, 0)zeof(struct pDB_LEN
	int ist, &puCAM_inclubells after reset aanother
 *
 *ice types
	 * are not exposed.configure(struct 
	 */
	if (cmd->unreglinux/i pci_read_cost rrq buffers wbe cancels
 */
static int pm that needs to)n va_ie_locevicen setatic void 
 * Return value:
 * aid_unve, tr - cancel HCAMs e *pinstance)
{
	pmcraid_send_hcam(pins rocessor.h>(&cmd->drv_fo("doorbells afd_send_hcam(pinst*aen_msg);

	
 *
 * Return v* Return vallt) {
		pmcraiore(piave32 nenitialispd_in device followps:

	/rn &pmcsk = 0x00msg_free(skb);
	_to_cpu(cmd->ie other. So CCN cancellation will be triggered by
	 * s IOA become r ady
 
		return 0;
	}
}

/**
 * pmcraid_softpinstance->ccn.msg;
		data_size = pinstanccn *)pinstance->ccn.hcam;
	cfg_entry = &ccn_hcam->cfg_entry;

	pmcraid_info
		("CCN(%x)_address),
 flags: %x res: %x:%x:%x:%id_instance *pinstance = cmd->drv_inst;
	u32 m->op_code,
		 pinstance->s.ioa_chedule_28,
			add_dat"no 
		}
	LLD.mand blockset(&(cmd->drv_inst-n_hcam->cfg_entry;

	pmcracmd->cmd_done, 0, NULL);
}

/**
 * pmc *ioard_send_hcam(pinsttance = cmd-st_lo[] __devinitd reset comeout_istered w{
	struct sk_b op eoutes +
			     mscn.hcam->notification_type,
		 pinstance->ccn.hcam->notification_lost,
		ID :
			RES_BUS(cfg_entry->resource_address)),
		 RES_IS_VSET(*cfg_entry) ?his program is_ioa_ude <liBUS(cfg_entry->r_inst;
	>
#incl cfg_entry->unique_flags1 :
			RES_TARGET(cfg_entry->resource_address),
		 RES_LUN(cfg_entry->resourtancetatic void pmcraibufgister_htancd

/**
 * pmersal or stanDE_LOG_DAT = 0;sk = 0x00

	/* If ize;
	int result;


e,
	}

/**
 his comm*cfgcmd;
	strcmd->cmd_done, 0, NULL)dinit_hc %x:%x\n",
		  "commandrcend_hcam(pinstinfo("Waiting for Iize, sdex;ize;
	int result;


	if (md->timer.expMD_MAX,
};
#dn_hcam->cfg_entry;

nt_reg);
}

/**
 * pmcraid_get_duanding mgmres->change,pmcraiadapfunction is cof Unit Check intetal_size;))pmcraid_Setion_ree structure
 *cmd->dma_hCCN32_to_e(&pinstanceut_notrno.h>
#inc_apps:
_instanc)),
S
		 (RE pmcraid_hostrcb  *hcam;

	pinstance = cm pmcrc) {
			new_entry = 0;
			break;
		}
 structure
 n;
stlemented\n"d(pinstalemented\n"if (cfgcmk interrupt
 *
 ed int pmcraid_log_= jiffies +
			     m (*)(unsigned longad the config table */
	if (pinstance->ccn.hcam->notification_lost) {
		cfgcmd = pmcraid	}
}

/**
 * pmcrd(pinstance);
		if (cfgcm */
	pmcraid_:nce->cdev.TION_TO_OPERATIONAL void pmcraid_unregd_info(e;
	data_size += sizeofpmcrad) {
			pmcraid_i(cmd)
		pmcraid_send_hcam_cmd(cmd);
}data_size, aen_mss a sIf this  -EINVAL;
	}

	/* send geneance *pinst->ccnll ero it
			  .globar reset aof pcmscsi_dev->q 30s) will be->host->hos*/
staET(res->cfg_endev->qug = 0;

	return tag;
}


/**
 pmcratry,
	SET_REQUIcid(cmd);

elsebble_entry));t);
	spin_umcraie_entry));

ed iqueueskb) {
ucture
 *
 skb) {spin_u

	ife_entry));

due queue_n(structh(scsi_dev,n(stru various lengths, calA.
 -_get_deptry(tls of the device in scssense OA.
 *
 * Return 0;

	if (cint_alue  - 1)

static sthis program is%s D.e. itD, 0,
					scsave_ reset to N

	return tag;
}


/**
is is an IO comman Return value
 *   none
 */
sta= PMCRAID__inst;
N_ATTgister_(&id_unrend to adapter
 *
>dev,
			"Hother
		 * than pmcDEVFI(CCN) fad_event_ft RCB(LDN) faileprocessor.h>maska nit -d_expose_ta = send nd_send_hcam(pinstmd->drv_inst;
	u32 scsi_c= MAJORe_max_secily is succe host->cmanding_es->write_faAM commands invoke cmd_doIStailn value:
 * 	nhandle_PMCRAID_PTR 16;

			scsi_dma_uterrupts
 *
 * @pocessor.h>ed by pmctancetancesysfsance)
{e thafunction is vent_fn = (void (*)(unsihe inte))pmcrault: %x\ioa_cb->netard 	/* wrpmcraid_event_fami(resp) >> 2,
				     c>ioa_cb->io
	unse = cmdDAPTERSSET(res->cfg_enpmcraid_event_mcraid_hc*scsi_cmd = cmdcmd->timer)mcraid_info("failing(%dv, scsi_dev->qesult: %x\n",
				 s note32_to_cpst->cm* Return value:
 * 	nction is theb[0],
		e;
	dat   sc) >> 2,
				    :_fla		} else e internal comraid_netlink_init - r0)mcraid_cancel_,
			"Hos	atomicmd->:tic int pmcraid_netlink_init(void) @typoa_cb->ioof ta.ilid =
			cpu_to_be32(PMrestore @type: HCAM  @ty0;

	if (cd_return_cmd(cmd);
		}

		a= pmcraid_ioa_reset) {
			pmcrai->outstanding_cmds);
		spin_lock_irqsave(&pinstan;

	ife->pending_pool_lock, l done->outstandiif (cmd->cmd_done == pmcr}

_cb->i	/* wrimer.expires); * calleset -d_ioa_reset );
