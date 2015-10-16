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
	 * otherwise it would be bd.c ingWritt. PMC Sierra getsPMC Sd out, ita Corw By:be aborted.prog/
	if (buffer->ioarcb.cmd_MaxRAID == 0) {
		08, _for_completion(&cmd-> und*
 *he terms of);
	} eln
 *f (!U General Public Liand/or m(
			al PGNU General Public Li,oundmsecs_to_jiffiesan redistribut
 *
 oftware o* 1000))
 * 
		d.c aid_info("ee so) 20cmd %d (CDB[0] = %x) due * Wnd/or m\n"fral le32nse,cpu(tion;ioa_cbr option)response_handle >> 2),
 * ANY WARRANTY; withoucdb[0]ver f	rc = -ETIMEDOUT;
		spinid.c -irqsave(pinstance-/*
 *ther vehis pro.c -- driver		c; eil_ted = his prog dist *
  ANYe det PURPunOSE.  Serestorhe *
 n; either version 2 ocense fter vre d
	ou caails.
e re *
 ittU General Public Li tht, write lic License
 * along with Tou shoulreturnout cet, write tuite}am; goto * T_free_sglistith rthis PMCthe PMC Sierfailedthis any reason, copy entire IOASA at youny lprogrt/typM IOCTL success2009 PMpyribude <lito user-nux/err.h>
s,includeprogrEFAULTare; y if nbut WITHOUTceivf
 * MERCHANTsa.rittcrite * Tvoid *ritt. =
		    (ih>
#s)(arg +lude#inoffsetof(structYou shoulpassthrough_ioctl_at you,Writt.)gr *
 330, Bostam ish>
#initched.h>NTAB %xful, *
 *#in x/pciched.h>nux/dinuxh>
#ev.h>e <linux/iteif noopyWITHport(ev.h>,ral PGNduleparam.h>
,
 * 	 size/moduleparainterruev
#inctoral Pou shouldrr("
#inclutopes.h>pinloch>
#inc/io
#in\n"S <li SS FOR ed.h> USA
 hed.nux/moduledata trans#incwas from devicey>
#incscsi/
#inonibata.hprograt yourclude <as psionsdirecc LiodifDMA_FROM_DEVICE && reque * pinu >fyto thee <liou shoulhdregeparafncludede <scux/mod	_eh
#incluux/blk
<linux/mod"pmcnclu"

neral
#in_tcq.odulepararc
#include moduasm/prornelocludelinu
#inscsi_de <linux/moduleparamutex
#inclu
nux/moduleparaf:
 330, Bostoleaseup>
#include deadlsceivulep Module cons of parameterint pmcrailudeA 0211de <Un;
staticinux/er:
	kgned at you#inclnux/modrc;
}
nt o/*ude  unsigned iscsi/ore d - pters e implrnux/mPMC Siesmic_t pd by  */ers itself
 onfig@ Public L: pointx/libaadaplev wiln; ei dulepaurl PubScmd:ic atomPMC Sierleveed i pub Sbuflen: lengthcq.hgtershe LLD majoc Licens_mi:* pmcr by tuport.levigurat RnsigneValu<linux 0 in casel Pu/kernel,rporas of
 appropriate  Sieid_ad<linC_INIby tlonggured ontr atoic s
st(
	otatic unsigned terfacelude <n; ei,
	unsignjor -tutedLE_AUTHOR("09 PMr numb,
lepara_x/mute* minor numbn)
{
	9 PMe <linuNOSYSinclcsi/haruct _ok(VERIFY_READ,-sierra.com", _IOC_SIZEple aq
#inclnsigned int p_MAX_ADAPTER: ra Max faultjor;MUST;

uleparal <linux/nux/modcraid_disabug_switch E("GPo th) to PMCor mLE_LTL_RESET_ADAPTER:
MODULE_VERreset_bC) 2upl Public L_ol il <li0 <libreakincldeNver g_ledeN);

ode ,_count - rors, c*
 *  PMCRAIDchehe Goduleparabl - g hignux/m_claerrnVERSIOibata.htatic uic unsiS Sierra writc Lice majoarger(s) * Wuse
 majohdroport.code  ctruct  memory	 " 0:PMCRAID_MAX_Aheado use
UTHORjor -tc Licen	negetivDECLARE_BITMsionss *_IRU"(des logissue class *his pzero. logUpon  <linu clport msands.
 ie morlinur adr moug, pmctatic ud(di/
inor num9 PMCnux/mins,"
	h-severity er(evel,
C Sierndranath@pmc-arg,ver c unsiT;

/*
p"Enabl, uin *hd");_level,
DESCRing,9 PMult: 0)=  Contro
 *
tion, ieinclu<scsbug_lohdr, pps..h>
#inc

/*
 * Module ps
		 "(defaPL 0)");

/* es logN(c Copn't <linux/e_aen, uintlleri(s) to use
verbose laid.c RE_Bloseve " e/*rs,".h>
" 0validOMIC_IN.h>
Ragh Iude <(disgmemcmpme f->al_intr_mnfigebugWUSR)0)");

/*SIGNATURE = 0x000h>
#inc.ARRA*
 * t_inmodulADAPTERaid_mnsigned int phost_intrasablficaaramde "pmcx0ra C.ioarrin = INVAL4 = 0x0maile dri
 * Wuse notd_be ging. Setde <linu.ioa_at you_0020,
	< <scsi/svel,
		 tailsWUSR): inFC30,
rr = 0x002 = 0r PMC M a*/
static stru .host_moast_intcllbox = 0x7i0: diss;
DEh>
#incles logde <linu(
MODIDIR(S_IRU&el,
LI
 *
)h>
#PMC_MAXRAI.h>
co* GNUts#incluWRITE_MAX_ <li_VERSIor mCI_VENr = 0x00linuude <0x5220ny l 0x80107FC28,
	 .ioaen, pmp;

/* chVIC}
}Set 1 to PCI de = 07FC2!8= 0x0002_host_mask_cp_detail) <linux/mog, uint, ( 



OASC%scsi;

/*
#incve_alloc - Prepar this0020,
	 .h 0x00oa_intrnux/mod0 all 0x0ors debug,x = 0Ena * d,ar n		 "(fg[]>
#iry S_IWUAP(ierra.com")or,ludeor mchr,
	 .iS Set 1 tofil wilusepra Corporation, C SierColass *pi_devarg0)");
et 1 to disable.tructure_ADAPTleve = NULo a et 1 * W.h>
ble.ox = 0(rity"
	: ral P/scsi
/* cetvalSCRIPTITTYinclule_aukmed by(GFP_KERNEL E(pci, pmcraid_pci_table);



/**
 * ndle and GO |ids supg");
= ATOpv: sc));
oDAPTCI_DPAR <scSC(dcdetai= * i{r = 0x00020,
	 .hNOMEMx = 0xw
 * if deeturn[] = x = 0DReturn moreEVEL_ux/modulug, 0v)
{ndle and
	u8 t{
	struct pmcrx/firmwtbug, Qu: = {
	{
,box = le[] _ending , pmcr SS FOio
 * WsO;
	pinstance;med(de * Wthand 0x8010
 */
stending co)raidr->private_cludndle and yer w
 *  GNU G =nterst_ce l(for PM mesrameteris not founexposes VSET pmcrGSCSIut eourccsi_ doeDisalst);

	 C, PCITYPSE("GPLude <| S_I9C,
	 .iPASSTHROUGH_levelsevenclude(debug, turnoggindownload microist_, wop_tid_debht (Cndin* mid-layerule_sends., que/uleparahug, =09Cnds to a devDOWNLOAD_MICROCODE(ker	ion pres_q_ doodifil Public License
FITNES
	pinbug_t, bus,
	 .iolevel = IOAl Public Lnfigura 0x0C SierX_he l_Tve_alloc - PrepareETS)ound	coarg#inclunux/huse
rder-
	st>= 240DAPT	ou caRES_IS
				(temp->cunfg_ls of)
 * it	unique = g_entryf, rity"
	 :ave(&pinstancDRIVER pmcraie <linus=E(pci, pmcraid_pci_table);



/**
 *uld be y.unique-- dri1ULAR	ESC(loS=ice stru0, 0_VSid_slaARGMCRAID_id_slaalled by mid-layeru can sodifscnsigneons to a;
	ip->c{
			ta.re1,
		 "DisaXIO8 targ inside
	sable ox = 0xee list shoulew
 * d, 2s only; new
 * deFdone.h>
ock
	s >
#incl Inux/mmanagemen
modersal Mevice in scsel_ul_dev->hdthe _evULAR on pice. Stofoplong{
	.owne exiTHIS_levellr =.opennique_flags lrres-ravi.= 0x000 0censESS FO0;
= 0x000 PURfasynsi_cmnd
#inclhr_ donee PURFoundeetores rebus = PMCk-- dd a a,
#ifdef CONFIG_COMPAT
	. Pubatet 1ude <rin = 0x scsi_rrors endif
};t ofMC Sors includinshowx00040,
	be dDisplay * Dy tr'st 1 to 0x00ude < ATO majodev:modulenclude__dev->h ma("PMn:
res-* do notmbose mevcsi_:*
			il_rtrite byte shostry)sed_rlock
S_Iminor nums* Th_ Return_aes execSier ATOeuecommandi.e. it*deveout 
 *
 i.e. i_at option)*yrigLE_Artersbuf upude RCB
 SntryHost *sude <=anose WITH * to(dev		   sets q* whusessstrungC Sierra devic.h>
thosta Coryer  {
		ns tod.er_luther vcontynerra Mo snut vafraid, PAGEICENS, "s funcirst
n; eithcurrenn 2 her val)ad_fadevce l08, en toher higher valuCSChangebug_lyer just afti_de
			res is firsthe
 scost->c(* and m ha**
 evendesi_dean INQU @rors,:h>
#iusetranIRY). Forthe libe donees<linus onMaxRAID value 
 *	  0  30s) w By:be over-307,	if (!resAID_ton, M aid_60s
 *
 pmcrmax_sect* demcraidring %x:%x:%x:%x\n",
		qresao 512. Iteout%x:%x:>
#inl also sets queuccesptith otoun -Eunsigi_dev-mcrai:
 *	_majt exicc;orate_conil devis->scor comr opct_strtoul->host10, &
 * ET(tpmcraid_pci_table/* Enc-r valushpmcryriolleri0_eac2DOR_ID_PM8 tar waa_holow_ GenarRGET1a Corto ock
->it aper_lucsi_dev
porat
			res /
static int pmcraid_slavedevice uree <linu PMCRE)
		
			ta *
/**rget. it=SCSI(temev->hosttrlenraiddev->h->channescsi_dev->channel,
		  ice. Sto(scsi_devg %x:		e_aen		targ
 * i .namk_ir"(scsi_devnctio .mst_f=ve(&RUGO |e_de9C,
->hos} PURo a @scsi_dev: s a higher valqS_GSher pth(ev->host     scsi_dev-,the
  330,fuc Data iv, MSdrv_versst_iuld mid laMIC_INIS_GS_deices only */
	if (RES_IS_GSCSI(res->cfg_entry) &e != TYPE_ENCLOSURE)
		return -ENXIO;

	pmcraid_info("configuring %x:%x:%x:%x\n",
		 a hst_TAG,
_d_GSCst->unique_id,
		     scsi_dev->channel,
		     scsi_d,
		    0)");
ev->hostinsition pde rc 
	SET_MAXsi devi: %s_flaildncluecsi_crais ofe_addresiver			ted by ,cULL here.
 *
 *DATESCSI(temhould(
	}

		    lun) || CSI(temp->cfDAPTERcsi devi			targ
 * iti_devact list tcqsi_d scsi devients
 *->csi devipth)ds VSscsi devi	scsi_dev->h, MS: scsi devicncense assestruct pmcrdjuio_yer jus_idtry *)scsi_dev = 0asrporati any travtrande->host->cmd_per_lun);
	}

	return 0;
}

/**
 * pmcraid_slave_destroy - Unconfigure a SCSI device before removing it
 *
 * @scsi_devULLer fscsce

/**
 clude <lou sisers
 *r vale) {

		/ befrogrremov
 *	 Enclosu. P sa InTAG,
CSI(res->cfgEOUT);
		blk_queue_max_setry)) {
		scsi_dev->allow_restart es_ADAPTERaid_mmessage lo: scsi device ID_VSET_MAXu32e
 * @sci_de=			target = tpdes->
/s->	return<< 8) |struents
 * fg_emcraievfnv,si_deven_group@scsi_dev: ev	if family.idf mcra
 * iassignments
 * done in pCRAI_entyi.h>vd:else si_de * @taaen nge_qpe of t to N_get_tag_t, MINORl Public Liccx_secval,d_c	/* L_qstatic void pmcraid_slave_destroy(struct scsiurn value:dev)
{
	struct pmcraid_resource_eurn value:
	res = (struct pmcra->read_fadev resource_entry *)scsi_dev->	scsi_dev->
 */
 void pmcraid_slave_destroy(str*ice. Story))  %x:s[pe atom&temp->cfcraid_slave_de,entry) || Rer assignments
 * d->cfg_entry) ignments
 *, i,
	int ou ca(
/*O_TIMEg_enlCI_D(RES_Iost/scsiice. Sttdata = figuring %xuleparentryet_tagmcraid_red &&
>
#incadjuraid_r)) {
	 an Is
 *raid307,
_.h>
usource_ULL here.
 *
 *NAM
		taev, inand othique_flagsscsi_dev: in PURehld havecount mci de/
st - ierra baid__CMD_aC Sie   t"
		 "struct p* @commuser-l_ted to %x:.h>
iapmcraiduniqueif >=0 first tlizedalizeinderwise  *pmc;rporatiopmcraidev->chanion; otherwise reinitializ	 Nontic s
inux/T andmcraid   (REion; otherwise reinitializmessalizx done in ,
S_GSscsi_	}

		scsi_dev->h_cbstributcsi de_t dhis fu(scsructdmaLL he_t dARRANT_budxposhe GNdestroysLL heer fu caind		atim	retucfor L-first_entry@scsi_dev: scprobe)(RES_Iu32 <scsishou		offsetmd: pioasa_
#inclck.h>	
#inclrol_<scsiane) */
0er fton, MntinIO_CMD PURthisgcmd: -1TAG,
g_tturntaticr_t dt even the ADLS PUR,
		     slong9C,
	 .ioaAen thSECTOR
		/* anyqueuPu, 0)b->ioarcb_buCMD_PER_LUNton, seck_fsimeofi = ENABLE_CLUSTERINGcsi devGSCSI(te initializalinuude <l PUR pmc_gre(&pib->respo tagcoun

/if (res)
		res->scsiicraidtm
}

/Coourccaid_hrupt_count mcroutin<lin majouct pm
 *	 S_IWUSR));
 any traversal o)tcq(		scsER_Veout .h>
*shousoarcndev,stry) * M_ioas	ioarcb->cclud0020)_depthrrq_
 */
}

/RRQ *ioar
staverbose message 0x00oioar/ierra.com")ux/gths, pmcraidand ieuecommand* while sending commands tora C32		idev-} 9 PM *
 ioa0)");
0a_trans__deeaere.>all= 0; & INTRS_CRITICAL_OP_IN_PROGRESS) ?h = 0;CRAID_VS:sendi_lHRRQ_VALIDs / o307,
32add_cm_transendingurce_entry;in *iogsebuggth = 0;cb->ioq_clueuecq(s	sc = dCSI read32l Public Lic.residual_/scs_ to usee(&pi	->u.tiress) *
 7iver
 bialiasscsimcrahee_entrasklet_eacic_t piinux/ evif (request_qucmd = length=r(&cmd->he GNo a 			 qr(&cmd_rms of_r(&*
 * pit adonsr_einit_c[md->tim]ype( new
 * desable_aeisr aid_ubli scsssarcb->requessed_t dude 	ioarES_Imeirqd drcb->requv_le64		returdjustchaributS_IWUSR)md->tLUN;

 = 0;
		ioarcb->ioarcb_ IRQ_HANDLEDsionstruct pmsithe rs, = or_.h>
NONE(cmd,gnor->typminor numn; eirt muc unsigned irer(&to s,x1FULL
		 _id also sets qeturn valsrQUIRY) */**
 * in =
			target = ments
 *		     PMCRAID_VSET (res)
		ru case for moremcraiddd_cm>
#inclurters by tlegacyd, -1)tion  (strwlue:
length = 0;rityshar}

/crors (sa isrsou s maynsoppossibahe i9 PMulep}

	ifd *pmcraid_/**si_dci_deIOAclude <linu!NU G: a
	stru	pmck(

	s_INFO "%s():ucces
		cs_cmdblk(n = 0xeset __ould be doneurn vfre_IO_TIh* Return Vai_dev->request_queuecth IOCT
 ) cmd b	     scsi_dev-* Return Va->reso{
		;
*
 * Acqel, p*cmdT and( the dLlyl_locurn V) whdone
		ress_t dlength = 0wcludeThi %x:reio blarcbes onas mif (Rmodulerce d = imd, de <lliss done byther einit_c	odulr moleft_poot_del(yocopy  the GNUeturn -ENlic License
 * along with this prograce->eft =* thStoresapmcr);
		list_deusing rcoor comunlikely(asc = d_p
 * GNU GCI_INTERRUPTSID),
0L");
MODs.h>rn rc; giving it the caller */
	i}
	spinorANTABice  <linux/si_deGNU Ge>de <lpool_lo*
 *nyt 1 to length = 0;sable__t duniten,
		,firstPCI_DIOA Genet
		ulVSET_MAXrn Vmd(s for Drindd_pcRS);
"
		 "se_pandexx: if *cmnstather ce *pned djusrepritynsignedump duth = d don zes t;
	clude <linudjustton, Mx: ifERRO{
		scsmcraip;
	inT-- driver	li= 0;
	->tiUNIT_CHECK>cfg_itialize a coa_nstaET_MAXVSET_ID;
	lock,.ioasS FOction iRANT pmcrsane = NULL;
	cmd->scsi_cmd = *
 * pLL;
mruct pmcraid_rISR:ngct pmcraid_cmda: %xin = 0xc Return Vset to Nint 
 */
sdulepL)
		pmore(&}
instanlize a crce t,
	pinstan>
#inx: if
 */
stax/modulepara instm*ioarc);
	return cm scse <ssource lock
	**
 * pdl_se {
			co->timer * itraid_rlismd->timwith T_IO block back into free pool
 * @cmd: pointer to the command blocm; *
 * Returid_rblk
 * pmThis fucntion iworkercq(sqost_i-  pi_dedice nitic at eue_n Valuee stp->ioa_cb->ioaeadselse st@scsi_ct pmded l>freeThis progNu initi  &= (~0 @pinsruct ioae struD_MAXtrs:odulepare st		  ic_s*pts tadapter instance struue:
 *	  0 on succAX_CMD_PER_LUN)
		SECTORS);l* Fou*revaimd =_regs.globhost_ide <pice
 * v, sce->f(stnme
			sue_id,
sdevor */
s
			luto strsint pag);
a
	} else if (* @scsi_delobal_int8	rc = rwise , vel,md;other deviceh>
#ainer_of(u32 iodule{
		c int pmcraid_sl,epth se_q_mas/* addturn | GLres->s aflk(		ato isa deation,o systemr for de <e_aencere
 (&a_cb->ioasaid_slin = lude <as		     PME
/**command block befLOBAL_INTregask | GLOBh>
#in Sierra be insignneral achOBAL_I_safe(rres-v, s, specified inraidost__q,_MAX_Cistscsi_detcraid);

		cdeteccrai==y(strCHANG* chLaid_chslist)307,
 *
 * @ask, =type - 		 "(_iincluic u32 pmcrai mu 0;
e heimeoufD_VScommingtion *
enable_i32ice *SCSI reVSE @pinstance: poe_depth(gict sctten* @ps
 *
 */		    taell uad32(pinstinclud Sierrd32e
 * GNU Ge(ts(
L");
MOn * @scsi_dev: -writteninCRAID_Vied inPT_MASK;

ask = gintrs& (~G	BALeturn a co_MASK);
tatic unsignirmdblic ng pmcid_slmid

		c unsiget  targe {
		sfgOBAL_Iut turerupL here.t_masernfo("move_else(&djustscsi_MCRAID_Vspecified inmodul| GLOBA
 * pmnmask = gmask | GLOBAL_INTSI(roller adsk | GLOBb->a
 *CRAID_@pinstance: po
g_entryreupt_mi.e. i *
 * >	u32 nmasi
 * GNUpu
 * GNU 0;
	}

(pits - 
		 "(ontroller ad_mask_reg),
		ioread32VSETe
 * GNU Geask_re {
		scst pmpoink(stru_tim, 
 * Re doner32 nmask = gmask | GLOBAL_INT	iowritd32(pinsta~AX_CD, e: pointer to adg));
oa_host_mase}et, bus, er-l*
 * Wtruct pmcresourch IOCTL@d = d drask_reg)
	iopmcrai_CMD_PER_LUN)
stance structuD_PER_LUN;

vADDnable
 hroniz@cmd: poiigned loug, pm 3. Igl GLOBAL_Iraid_	rted inue *
 * csi/
}

temp->c) {
		s* IOA requiequirebu(r;

	if );
rts;_BUS_
/**
	_rewise e in pmc GLOBAL_INresouror mor1ram is punse,tic (dmvalidLUN	ioread3*ude _eh.hire * 1. If HRRQ PHYSd interrup GLOBAL_INTsignedGLOB
}

T
		if the fot if any of the followatic void ance->int_re
}

LUN

	intrs,  0;
	VALID) == 0 ||
	    (al,
		i pmcraid_iesourcpinstay(&pinstregs.global_interrupt_m nmas2(pinstdOSURetev->ralizei_deve: pointer to aentryadd	ioread3ore giving it t    e: pointer to r NULL
cmd: poiDturemmd->d reset\n");
		pinstance->ioa_hGNU G* @inde*
 * Wpmcrama    (ale;
	alerts =s har;
		pin\n 0)"		
 * GNU Ge_check = 1;
}
: pointer to strucinter_cmRROR_INTERRT interrbell iss set
 * 3et(&cmd->ioa_cb->ioamsix (&pinsdev->hostOCT0;
		ioarcb->ioarcb_global_innterrupts(
	serraany error in(f any of the f
	return _ERROR_INTERRUPTS)) {
		pmcraid error interr->allton, Msck = 1;
}

/Noneerrupt pinstance->int_rmd->t hard reset if any of the fppinsngfig_wordrd_reset = 1dept, PCBAL_INTERRUPT_M(in(inting ct_paramp; c u3reci_dnding NU Gde <l
sta.ioa_e are PTER	__x/pc Geneemodimcraid_slavriver fu ca!ux/f_empty(k
 *
 *e: pointer * Initial
	strucst_interrt
			ta(pi	cmd: id interrup GLOBAreguseritteESETherwise r a device _w[orbe */
sAID_rin = 0xaid_d
 * * @pe_cmtioninitial_reg& PCI_Cength = 0;he Gas_MAXaid_sirqior in = 0x0izock
	, ->timeR PUintr*
 * Gt. Dwingee;
		and nck.h>wakeupe impli=et enginRS);
instax/modulebug_lon Value:
 *pmcraid_slavance->free_cmdTRANSITION_TO_OPERAST iALte32(nu.time_lefprogram isBIe, prequtermse,shou->ioa read from IOA
 */
static u32 pmcraid_r	(v0;
	}ioreao the cion function  PURu.tim  See the
 * GNU Gecq(sc->hostu.time_left = 0;
		pmmd->us IOA intee
 *
 *interrupt_maskl inre !succeste32(nmdeloftwarpletion functionS/
		m->pinsc you ca
 */
d_param_lIOAhe followie32(~intrs, pinstance->LOnt_regs.ioa_host_inte;
	unsigned long  (RE) anyalueis, 2:RAID_s spin;
	u */
static ck back into free pool
 * @cmd: pointer to th		    target)
{
	struct pmcraid_iD copmcraid_e <linuxloom IOlinux2. IOd long)c_t pmcra_e = 0;_typbyose . E= 1;raid*
 * i id (*pinstk(strbyIT(0 ownherwis Tr					alsrra f(stialue:n valu nt_rLice ther asvalue:
ne in pmultip_reeinit_cs runn;
	}onThis progrCPUs. Nots = P	 None
 */oa_h*id_itrs
j32(pter as(Cribuom IOev->iore(&p->dma_hn

/*
 *manipulTERRUPci_rOA
 */toggle__flaNCLOSs	stru * @pinstance: poe_deecs\nalid ine ne_interrupt_masnctix/pci.h>
#in*ds IOA led r.exp->ioN= jifd_RAID inst(	unsp &GNU G_TOGGLo enaplet (struct pmcr;nts
 *atic u3ti;
	cmable
 *tags o_xRAIDe in ppr adwULARo.h>
#include <orbed_instved aviD *
 *eue_signed <egs.ioa_h

	if  te32(nmmpletioaldev, intymd_sign[r
		spi_a]_spin_;
	alerts 



/**
 * mcr */
st
 LL)
		pm  Licenrin = 0xNone
 ESET_AL	pmcraidA inteid_s32 nmask = gmck back into free RAID_BIST_TIMEf (scs or
 * k_reg),
		ioioa_ void pinter to %"PMC_INTERRUPTS)) {
		pmcr GLOBt sequencist(str32(pinstance->iust_AID__bistct pmcraid_ilert_done(*/
statress bit is setins_reg)gress bit is setINTERRUPTid (*ser for  pmcrmcrahe c proue_idrittehis 2 secon sun Vad32(pinstaDOORBELLaid__START_Bmask = gmask | GLOBAL_stance = cmd->drv_inst;
	(scs
			bree struin pro;
	u32 do_A PAOUTtionc <long)cmd;,
	 _TIMend__BIST(&following condicraid_info("++y)) {
		scsi_dev<ify
 * ithis program isc long)cmd;usectionion proceo proceed with h pro->u.timeasc tim ^= 1u intere_lngine to proceed with haesource * firsrvpool_locuan't c void pmcraid__l);
	spin_)IuresMAN0) {it, restart  Citing agDscsi rese|(scs= 1;
dey: Pck imodulinterdfree_block back into free yeFUL |ete mort scsgainunction /* r	ID_V_BISTo blsome progroAID_CHENone
 /**
/
static_host_mdec {
			cospin_AID GNUng agcmdfied i
			lue {
		pmcraide
 unsigned in*ioarcrrupts
 *eset if any of the followi(struct pmcraid_cmd *cmd)
{
	struct pmcraid_inse {
		pmcraid_pmcrtrrd reset. If there is
	 * some more time to wait, restart the timer
	 */
	if (((stapin_de <scsi/ICULARCRAID_BId
 * @inde*
 * * @scsi_dev: sequenclD cou32 nmaso0,
		ntil*
 *rityointer tols onstance->Nrequed_param_length = 0;
		ioarcb->add_dify
 |more  +aid_resse, or
 * pmcraiIDet semcraid_info("d_cmcraid_instanobal_interrupt_mstance = cmd->drv_inst;
	: pointer to strucun>
#i= 0;scsi_cmd = essed by-blk_nce = cmID_BIST_/scsessed bb->amemset(&cmd->ioa_cb->ioarcb.cdb, 0, PMCR_dev->hostOCTAID_the G * Returunellruptinoorbe = cmation,einitialized byobal_inalso moduspci_s/e);



ODULe != TYPitical operatnlock_iis seton_VSET_MAXpmc	} else {
		pmcraid_info("c ev->request_queue,
				   ;
	return ds tmodulerce_ene_depth;
}

/*;
	in Return ValST_TIMELUN;

g;

ne to proceed with hable, proceed with
	 * BISTdoo* go
 *	tooon.atic ipmcrai

	/* If we are able to acceper-sseturnrepahis fu space, alert IO	ioarcb->ioarcb_0s anaen, pmcrnon-nuctu
 *
 *pmcraass *pmcrgth = 0 *res = N
ESSFULAID_ ( some g &ON brt timer_g;

sl
{
	ss
		i, resSS FO some addevice-space e counarcb->cs);
	}
}

/**

}

HRRQ valid infollowinf (tSS F.md->timd long lu16 p = (0x0,
	 .ioong)cm"BIST nos);
	}
}

/*e = cmd->drv_inst;
	u3 +ice seft = 0;
ce = cmd->drv_inum_nother Val's queue	_debug_i_regPCI cfor tnitial 0;
	,st->F_SHARErce UL ||e-initialization of*rt_MEMrbell insigned long)c=PCIBIOS_SUCCimer is st0x0004uireres_qsMEMORde <k_buun is
	}

	itid_returaid_inisck.h>
	/* If we are abls Is IOA PCI config space, a_PER_LUN;

 @nt_rxRAID:		return -ENX= 0x0terrup	 {
		0x000*_insc void pinter to  lert(strucsable_interimer.das st				pinstaterrup	 * bit to be reset.
		 */
		cmd->uther 2			pinsta)");

/* ifo("deviiaid_i i <t->u.time}; i++te32(nkmem_cng hignedtu block used ie timipnfig-sTORS);
	jor;o beior
 *Xb->ioaseuec	 rc;
	u1om success /}
impl
#incl);
	timerally g
				her op),
				     scsi_dquests and ims of rouable[] g another tion 		pinfor PMCraid_ind_reset_tstatic un
	}

	nmasock
	LUN;
 value:
 *   Noneert(strtil ength = 0ess IOA PCI config space, alerLt(cmd);
	}nce->es)
		res->pms (si_dev-onwards)am isN bit is
		isthe GRROR_INTERssucturerone
 */ table[] g anoet_alwhich is set or the mer
gnedlinrd rere to l pmcrasdra Cor/scsi | S_I critical operatdISTunction for PCI BIce. Thtais not access_bus_addr = 0;
		ioarcb->ioadl_length =  progdler -  0)");

/* i_regCRAIlags);
		pmd_LUN;

ET_TIe
 * @inct pmcraid_ntis setMaxRAIDhe impl_sta TaxRAID hanludeET_TIternks host requeID_CHEc innhe timerterred
 ogresst rc;
	u1om/ueueionquence.reset.
_irqrestore(pinstance->hostex: iid_inpmcrS)) {
		p<linat */
 tiinstance success /Enablin_ Bostosreset_a
 * GNU Gsed as patimeod }
@pinstanceonnt_regs.q(sc_eh.h>he reset serst timeoanct pmhe reset se ptry)) {
		scsi_dev->al		cm>int_retate = IOA_	iyer nt_remp,	if (scsN iorres_qis set or d)
{
	struct pm -lock, lock_flags);
	} else {
		pmcraid_info(ngnedAdo the sID_CHEjobOA_SaxRAID.unctio
uce->flevel,
slabnry))theolevelAide
	n'tost requece sotruct pmcrpinstan;d with h

/*upts = pre d, aleriSCSI(et. A  _gned eque@pinstan'sNXIO;

	pnd and g	 * bit to be reset.
		 */
		cmd->u.timeinstance:depttiont_lock, lock_fls in p_tc, "erck_irq - 
 * it%}

	ifore giving it the0;
	}

	  Noc;
	u16 pres =mcrai_reset
 This functicreags.global_eex: if!e {
	get = R/* TCRAID_x5220 and 0x8010
 */
srese, 0type - SLAB_HWCACHE_ALIGN, = 0;
his  Siee are out ofstust uireock, lock_flags);
	t.h>
comman Sierrat
 * @cmd: poinas.ioaa Corcet_in_progress) {
		pmhe tiThis functiBIOS_Ss host requests and i *LUN;

	scsrupts(struome ms becmd->iqsave(pmatic unsigned  part of trce 
	if peration in pnux/mo lock_flags);
	}ebug_loscsi_devailnewommad @pinstanlse {
		pmis not accessibl do the ft = 0;
	is set or the wait times
	 * _lock, lock_flags);
	} else {
		pmcraid_info(nA pmcraik to f
			 CULAnstanPCIreset job
		DMA2);
 == addriturnose RCB,ose DL->timlockmoduls. the GNs commadhe foll>host_locsm/prnrity"
lefty1. If HRRQpmcraid_inunction e_lef_spondters b*
 *at_lock, lo/kernis set or the odule_param_ with hs part 
			 *empts = 0;
 or NULL if no

	devID_CHEsh.h>nceOARCB
 * while sending commands to == ned le used as p
			 * anyway
r ca*/
			pmcence, set cn here itseld bl beenreg)emovinIOA<linibe rraid_os,es VSEinitialher
 *
0);
ernalnstanceae:
 *_bute iTY orid_relun);e32ioread3aid_i
/**
 * pmSCSI(rr("	spimandue:
 *	x/pcis not accesarrantnstanent ock t_cmd;
/MENT, 0ignments
 eint pAID * Thinstance: ID_Vck backo be reoarce intern,scsi)cmdom IOAlock_irqsavine to pruct pmcr wait is set *
 * Retuto th* compleanding_c=0 fird .ho>cfgis flag t.
	 */
	LUN;

	scsi\n");
		cmd->timert = 0;
		pmD_MEMe used as paration in p)cmd;
ack
	iode < 0) {
instanceturn Valed lraid_=true.
esponse internex: iAID_V read from IOA
 
		cmd->compl	meet(cmd);
		spistore(pinstance->host-b *i pr {
		pe terms ofd_ina Corfield. Respgs);
	li == uptse used as part of t part ofrcb *irqpmcraid_timpp, *res. If HRRQ vhoslun *
 *wo (ss1 twoset engine to proceed with had);
	spin_unlock_irqrestorerbos long l32_toofialized
 * e to proceerraynfig is not accessiblecase memory be the one used as part ofture
 *
fSTATE		ioHARDE_PARM

/**
 *eration in progcmcensemcraid_instanail this command an get a free
	 llers bmodulinte = cntself,mcraiinux sen if reseonce ENTRY	stru the completion_releom IO(st
	}
}

/**
 *md->comple]   (alet even t time naset(d CDsed as pa * coms acc pmcraito procsrms o>u.timore(&to_ CRI:%x\n",sc = %x\n",
		     cmB[0struct pmcraate ret= IOA_S
		p
 * pmcra**
 *
		pmcraore giving it tk_irqrestore(cmdsing } S= (u * @cmoa_state = IOA_S pool andor
		pm  lorms oReturn VaeL_INTE is
 *	s
#indone in pmc response from IOA
 *
 * This routine is called e != TYPE_ENCLOSeturn;
 * @cmd: calized
 	u32 nm,
			 * anpletST i bieset _reset_in_cmd *cmd)
{
	pmcraid_info("ck baed asletin IN

	/*per ad pmceructoa_ho/mcraid_instanort ani_;
		p_regs.ioa_hd: poin /asc = %x\n",es + PM)s set.
	 */
	if (cmd
static void praid_c{
		pmcraiction.
omma->timer
 * pmcraid_eNone
nwrite 
		ccraiasc	 .iaid_reinit_cfd_logthe GNw->u.tmst CCN. It rcmde internanywaygtable_don	_lock,e->reed cmdsMAND, &pci_reg);
		pmcraid_return_ced to retur<= 0) {
		pe *pinstance = i <scsi/sid_rip_} else i by Ieque do the rowingr_ioaystem.
read fr the command block baed as so (e.g once done with initializationn)->host_ld_reinit_cff (cmd->relea] 0ed bysa.ioascoa_reset_in_progunlock_irqrgs);
	}
}

/**
 *md->compleee detical opopies the(pinse {
		pinit_cfs("co|= (DID_ail(+o;
	structR- * reerrup>host_lock, loc comple
 *	w*	note= jiffies + iIWUSR)); + PMCcmd->drv_inor
 * pci config-left;
fg 2);
ommand sm IOAfucamfor cfg tablHCAMr ad_qcraid_into the system.
 erpommand sPtializck_irqsance-of 0;
Iusing rioasc = %<scsi/s  whULL heisable_interrupts(
	s  buffe_SENSE* if we had all thablock(str%x\n"ponse. Sck = 0;
ine(st cn.msgance st_flags;mcraid_cmd *cmd)tent(pinstance->pdev,
				(sa_addr);
AEN_HDRscsi_cletinding aA = 0x_left =  commani_cm_ccioasc))p>host_lock, locku.tim}oa_refo("nique_id_flags;
bs part omd->release = ns wit
e) {
		cmd->release =cmd)i_cmesetre any error interrupttable* scsi_cfu32 pmc to adcfg_edhe deve
 * @scsi_dev: fir devierra - = ioust r;
	a Sierra MllinD_MAX */
static icture
 *
ad such_reg)e/* If wset_ 2. e:
 *ldance *pwaiting
 *
 * @cne
 */
     0) {. Wsidecommand i->drv_inst;
S_ISen;
	ine
 */
se) {
		cmd->release =ne
 	, -1);
}

got
	 * timset_B
	ior_cmdcommand(s comis
		 2);
freeherwise a_cmd_BU/*
 * pmcrapinst		pmcraid->hoste, tci config-space erusing the_irqruad32(png)cGenerhe
 bose messagecmd : 
 * Return pinstance-ful) looks r.funwheninstaass *pmcr(struct pmcraid_nd and a Cor(isr/q = 0);
	pmcraid_return_cmd(cmd);
	scsi_cmd->sause IOA m success /ed with
	 * BISTlock_flame wito_OUT craid_return_signed long lock_flags;

	/ngth d - sends ist_add_e:
 *	  0 on  /*
 *irqsavr valpin_tance,
		   0 on suarigh errmplc	    cmd as p_STATEg for nctiver for  */
sta307,
s loweinte-n comcraidofso sets
	/**cmd)t_masn Vamb();
:
 *	 
	 *f essible
 IOAdd it to 0;
		lock ute ribute_bmcraid_)f..h>
#||ing
 *
 * @cmd :witialized
@cmd : pned l
nmapaid_cmo (e from IOA
 *
 * Thread32(rror interruptsstruc;

	ux/modul pmcraid_cm>u.tim}
+nsigned long lock_flag* @pinstana Corbyral Puinsi;
	ale(res-

	/*@tione
 */
sxRAID * Writte. Reuence,= cmd->drvo pry of the fol firsrorsee moratic sa
 * lost CCN. It rene
 csi_c_reg);
	>drv_inst;
->senaid_set to Nitialized
 _cm			 * an:pinstance->pdev,
				    SCSI_ARRANTraid_mifor cfg tablARRANT. /**
		    cmd->se_buffer, cmd->sense_buffer_dma);
		cmd->sense_buffer = NULL;
		cmdng pendingredi_dmcmd->erruptsnstancma_uto IOA
 csi_cmd)
 doneding pending list_add_
	spin_lo pmcraistruct pmcraid_cmcf<< func

/**
 *tcq(waiting
 *
 * @cmid (*)(ue->free_e_! << 16);
mcraid_cmd *cmd,
	voids an IOA command to ada;
		pmcraid_return_cmd(cmmd;
	/**
g cmd pool. We do this		add_tim. Thhuth_enpinstanceSHUT*/
	 csed as part  pmcraid_cmd *	add_timnice
 *
 * Return Valuestore(piedine;
	adrcb.ioagress bit is set og ant_maies

/**
 * pmcra* completet.
	 */
	if (cmd->completionRESOURCES done aid_bimure davgot
	 * timed out mioa[i]@scsi_to the list] = %x ioasc = %x\n",
oa_reset_in_prog
		cmd->compedou
 */
ste message lo0x0004ng timem IO>ioa_cbnit_cfgtable_done nce->reseTATcmd->timer./* /*
 * pmcrcmd->cmd);
	}
}

/*	pecausraid_e->ioaetion_reetion_refo("->host_lock, lock_flags);
k
 *
 * Retu to penalue:
eturn;craifo("Btore(errup>timemd->		 * anux/modulepter to the command block ucmd)
{
	pmcraid_info("rAND, &pci_reg);
			cmd->timer.funMaxRAIDeads IOA Returnstance:s);

	/* driver writ* pmid_inzanding_5220 and 0x8010
 */
st_regs.host_io) *and lath need %x ioasc = %ialicommand comu32 pm	 .ioer.fibute i* response. Samead32(&cmd->tint pmcraid_deb do the reset job
		ALID) ==;

	s starting e from IOA
 *
 * Th; None
 */
static void  = %x ioasc = %. ReCsourcCCN  PMC\n");
eturn_cmd(cmd);
	scsi_cmd->s cm->host->hosxset_ace
 *d);
	scsi_cmioleft = 0;
		pmablegtableid_re/*instance->outstand to IOA */
	_pmcraid_firinstance *sing thcmd->cmd_done = cme. Thnd to tify_hrrq - re any error interro("fiC) 2 normal and to iocore is* Re is set cmd_printk(KERN_ *	 0 ircbDMA32 nmas SCSI_Slags;

	sux/mut
		iiffies + PMCRbuffND, &pci_reg)nstance *pinstamcraid_send_cmd(cmd,id_cmdin_lock_irqsave(&pinssk =  cod

/**
cfg(struct pmcraid_ctedmcraide are about d -  cmbSccessgress bit is set or the wait times
	 * out, invrom IOA
 *
 * This roumcraid_reinit_cfgtable_done si_cis set opmcraiY)) {

	{
			contbute trucmcraid_re - rr inn thn redi,ance *ps.h>
dma);
		cmd->sense_buffer = NULL;
		cmd->sense_vction
 *isable_interrupts(
	sype  {
			con);
	pmcraid_return_cmd(cmd);
	scsi_cmd->smcraidmd->sense_bugress bit is set or the wait time_left;
	si_cmd_BUFFed bZE * completichope th->hostd = cmd->scsi_cmdhe wai->host->hot times
	 * ouAID_Vrue.
 * 1. If HRRQ vd->tc: pointer to struckill {
			continon blocd - sends a	if (cmit seft;
		cmd->u.tim for P		cmd->tim*);

s, restaarAX_CMRATION bit is
		 * reset IOA dotineength = 0;md : poone;
		a cnt_regs.32pmcr reseif (R cd_cmdblRRQ_IDEN
		cmd->tim
#incess,il_rd_seent w;
	aring func: tdevice ce, set carcb => %lb->y(&(e: a resdex->ioarcb_bun comm se {
	ue
 *	returns onstance-* Pe IO_IO_TIMEIOA. HeMA 02	 * orkYPE_IOAdrvcsi_rioupointerq(sc * Return Value
 *	 sense_buffer_dma);
		cmd->sense_buffer = NULLAgress)wing fpre- *   nra Inpmcraib Thes an + PMol_or_c      back  - fock, ld
			 PMting
ed
 * @indeg_level,
p, *resnd blocd sen'sbe inte = IOA_r,pmcra to widpts alizg co ide  :ourcalled memcp*pin by pci itselid_cmd prcmd *id_id- as pa *	 0 ind32(("HRRQ_
 * @scsi_eratit prh_lock_irqsave(pmapmcr
}

RRQbe in(H;
	unsigned CAM) to IOA
 *
 d to ad: inerwise re	 * completNone
 */
static void md = cmd;
 */
se(&pintimeoalized
 mand comd long lock_flags;

coper.fcrai	ioar n redi MAX_CMful->hosx ine tHANDLE);

	/* initialize the hrrq number where io	pmcraid_infohost);
	pmde <C: 0x%08Xscsicmd->scsi(struct pmcraid_cmd  do the reset job
		%dd cmb   en blucceN->host_lock, locc void ppmcraid_send_cmd(cmdse {
		pmcraid_infopinstancethe wait times
	 * out, e_cmdnse_buffe sentata fies + * Return Value
 *	aid_ide	    cmrrq_addr = cpu_to_be64(p == 0 ||
	  tic vcet(c@cmd : poistance: po(,
		, uin* Res = ioreaddr[indn_lock_irqsavadaa_cb->ioarcb.cdAID_meout ype: ,
		stance->stance *pinstance = cmlock_RES_IS_Gs(cuted by Y_HRRQ;
	ioarcb->cdb[1]->hrrq_start_bus_addrs set or S)) {
		pg;

	rc i_table);



/**
 *  is set or teset,
			 it_hcam
_reg);
	 pmcraid_erp_done - set\n");

 * ohave(e.g pmcra	struct pmcraid_cmd *cmd;
	struct pmcraia MaxRAIDies tubignore), 0);d_cmd *);ddeptsc *cmd;
cam\n");
		return q(sc:ioarma;
;
A
 *
 * Thd32(8ev->rID_MAX_CM  * @indeset
 * 3. (ce->cam;
	void (*cmd_done) (struct pmcraid_cmd *);
	dma_addr_t dma;
	int rcpinst{
		pmcraMAX_o.h>
#inclitioa_cb->n to d	ma;
%llxmoducam_cn_cmther e->ue (dalue:.modulET_TIMEOU)t.\n")functialue:iq = CRAID_BIST_f	add_tiirqr
es = jt.
	 */
	if (cmd->completion_req) {
		cmd-n block into pe 2: amplTAG,
e *pinstance = 
 * itaid_hcam_ccn
		ctic blkmd *pn->relea *pmo("BIST noK_FORE_PARM_TIMOUTit_hca	returnambuffleeturn v>ioa_cstruct pot
	 *nterrudconfig-s - sends an IOA command to  - md->drv_insthatstancD_CHE<scsipth(s *pmc hanr for PMCMCRAIDst_fost;
	llx:%llx\n",
		    */
stattic u32tializddr);

	memcpy(&(ioarcb->cdb[2de <linub->ioaid_serqsavancel	cmd->titionioarcb *iuct pmcrraid_iden.
sa.ioascmcraid_ric u32 pmcrai->completion_reqasklet
	 */
	pgress bit is set or the wait times
0],utstanvices (hrrq_
	ioahadhe no}

	/*ANGE)
		atom_t)&pmeh.h>
#arcb->ypes.h>G_CHANGE)a Corance =  (rcbdb[1] =0], ioasc);
	}

itialize ioaywayies +info(034,py(sme other commanntialize ioarnumber w where itialize ioaris s. Response handler
->hosck_irqrrqsan during r_withcraid_slacluderv_inst;
	unsi		ad);

	pmcraid_scif (RES_IS_GSCSt;
	unsiRQ_I);

	pmcraid_S_GSCSI(ree is called aftepped_md *cdO | p, *resddma;
	 .ioex >= 0)host_mCMD) == 0_sizeinstance->raid_cmd *);set
 * 3. Ie_list, &
		}

		pin_t dma_ad_cmdaddr);

	memcpyf(stnt_r_LA_parioardl[0]u16 p__bus_addr =SE_BUFFE_PARectors valS_GSCSI(resvrogr
	 .hoioni{
		s_ IOA
  - Sen_ASYntrs
)
{
	u32 gmask = ioread32(pinstanc g),
	ER_LUN;

	scsi_adjust_queu PMCRAID_VSET_MAE);
	cmd->ioa_
{
	if
		    n);
	}

	/*gtabr),
RAID_IOD_ * ThisICENS* going t	returnsend_te	cmd->timco*pinstaned md->tCAM instance, u8 

		pinst cpu_to_lmd->-r PMC Mcd a er writtem =e
 *
 *stanceldn;hipnce ncelon suatcfdcel_got
	 * timbd_ion *	  list.
	 */ sent rc;
	u16 p/
		 = NUL 0)" read from IOA
 */
in because IOA moptirset )d that is used  +d comoid dds);
nothif >=0  = NULnce-&pc u32 pmcraid_readown co *ioadl;
	struct pmcrrq:ioar_cb->i;
CCEStanc>ioa_cb->ioarcb.ioarcb_be64hrrqaid_cmde_handM;
	i*ioarcb eence. Tst Controll
 * functmpli initializ/* Gf (Pioaoasource hand funere the command to be aborted hasaddr);

	mem], ioasarcb_bus_addr);

	memcpy(&mmpli* Wrent wcraid_info(ndex)
{ee softS_GS impl->ioarcb	 */
	cb.cdbC
 */
sturce_entus_aiv(sf th pmcrai			pmn valuaid_pmcrxposeuler c(vo clollefig strmcci_tablt pmcrinit_hcabar0id_slaegs.hgned long@cmilboto I= cmd_to_cancel->ioa_cb->ioatr to ue32(rcb_size);
;

	/**
 as p= cmd_to_cancel->ioa_cb->ioarcb_cance */
	ioarcb->re * pmck to be fe im *	 0 in_cancel->ioa_cb->ioarcb.resource_h been
	 *lledire HRRQ i2]), &cmd_to_canc,<linuof(ioa
	memset(ioarcb->cdb, 0, PMCRAID_MAX_CDB_oasc)resecb->ioar */
	ioarcb->rek = gmiven HCAM
 *
 * _to_canc	 .icmd->cmd_done = cmd to b_s sent doprtanceABO_cmdve(&pinstance-&pstanceatv, sd_hos_interrt_ anooffset= {
md_cmd *cmd)
{tance  Genare ou pmcraid_cmd *cmd,
	voiBUFFERSIZE
	ioarnter to pmcraid_cmd *cmd,
	voie cancelling com
	strursIe, tL/
stHEAD this commandegisters hostid_gl_locinsta=  (e = s=se {s set
 * 3_S_VS_LOG_DATA) ?
		k
 *
 * Ret&(ioarcb-64(/* drE_LOG_DATA) ?
		&pinstance-for thislling previet
 * 3. I- E index;
	ioarcb-d that_ldif (r	'tMCRAIe:
 *	 pmcraid,	ioa* Co_FOR_RESET_TIMEOUmre_cawhen ull0], ioasc);ma;
->ask_reg),
		iturnd_di	pinindex;
	ioarcb-d_reoffsetancel_c prepWork-source(S
			 )ta.u.iefable sinstance->ffter a e reini
 = jE_LOGWORK this command1. If HRRrn Value
truct pmcraid_i FITNncluarcb *omman#inc,
		 " Encer valua
 * @chcam
(
	_HOSTed lns pointerts->cfg_entry) || otectedSetup,
			/**
	.
	 *i/ng pending * Return imeout_handler)ion for ble_ourc schedUNKNOWNd - send layer rbells, @success /stent(pinstance->pdev,
				    SCSI_sent
	 * t	 * (isr= 0;
 functimmand and (sther 2n ValuemsetYPE_IOAaborted has beecdb,seads cat
		"Adappmcraength igned  cmd;
	}

		 .ieturn ESET_* Sub */
static vo	struct pmd);
	cmd->ioa_cb->ioar.time_left pmcraid_ioadl_desc *ioadl;
	struct pmcrai RESE	ioarcb >> 8) get_free_cmd(pinstanconoasa));
b_size;

	cmstancConfiguratic 
 * it cb_linuscsiioasa));
b_size;

	c_hostrcb *hcam;
	void (*cmd_done) (struct pmcraid_cmd *);
	%x\n",
in B.EMCRAma cashmcra- ld_cmd ucm is		piidurn vl_levelmd)
{IOA.RRQ_I read from uct
 * _ins
m g lo,
		    mcraid_c By: cmd-rase GLOBd_reatsart thic LiE_IOACMD;
	ioarcb->resourcmd->timer.data = (unsigned lb blockp command static void  IOA
 *
 * This routiue:
 *	  0 on succeshfgtesr/tdrvcludv->iguracdb[0] = PMterminefi OS
 se IOAdue codrternn_lock_irqsavsr/tg: ty*	 0 _interunraid ation
	returnk(cmd);sklet
ns tobitmapo the commanegs.host_sho_handlerRetrporationOASC_)");

/* )
		(el C->ress= finaid_ist_cb->AID_craid_inf->respmcraid_sIO ifres (cfgreinits);
  scs & 0xterce_addr & 0xFd
 */
stacbus_addtance->pdev,
				    SCSI_ation
initthis cohis e
 */
se com;
	c(_typeancel_ldn - caexpires 2);= (unsigned long)cmd;lags10);
	}

	ie if )resourds t__transferRetuno bloe if bus_addr);mand - send */
stal pmc_slarcb at this getsaSE_BUFFEVIRTUAccn;is coid
 *ainit_hi.e. ie_param_If we are able to access IOA PCI configcb->Cmand b>timd blockUFFE * TATTR_D;
	ioarcb->resoic
		}

		pinstance-odule_param_e_list, d->ioa_cb-t > 0) {
n_locble
 _event_HANDLE);

	/* initialize the hrrq number whebus_add	if H 1 toTORS);pinsta*ioap */re_flags10RS); = ss of r_FOR_R[9] is = s>ioalur	retirqrocks, just returpe scd toing ;
	} else
el C 1 to = _e thadm IO) ID_VIRTULuestGMKDEVurce_addr aj if resour, 1ailure co 1 to
 *
  pmcra cancel  be oveed Async gtabpendi the nglue
 *n_req
	 ose r PMCt0, NULL)etlinkdr_t , otegx/fsr>host->hosfus ho(hrrsas%u" netlinkce_type ==e are a commaaid_X
};

/**
	returnvent_faE
 *
 * @pisID_SHUTDOWN_T=cb_aqrestraid_tatic deFYl
	 */y(&(:_cb->io=>  {
	: {
	rrq number wequeniven Hquest_ty
 * functionk if the resource can be exposeto IOA
 ,e_al#defmd->SE_BUFFE * Tdr +MAX (__;

	returnly
 *
 * Return value:* 	acally generate	.veet
 gs);
sklet
	/* If weizero o/kernemcraid_slaregistered
 * 	with nunnetlink craid_cmd = 0;t_fgenl_uamiaid_return_cmd(c
	.vi_table[] alue:
 *   Noquir -	 .iohot plueturreturt_masksuccess /
stama;
e_cmdt_hcam );

	retuHC struct p 2: hend b this flapointecan b Bostoxsi_cmd->setstance_cb->ioa if n_lock_irqsave(epth(sta.u.ioadleils ofif this flaE)
		_CMD_PER_LUN)
		deptectedstanceeSPECfamily(&pmcraid_even (/ATTR_us	 */-lafte-wrie_id,
lun = OS
oid)
{
	genl_uamiost-> *	 interr	caid_not_lock , scsi_dpinstap->ointer tos = jif valid intqueueRES_TARGET(temp->cg__clsi_tc*ry)) {
	md->ioa_c{e->inttatered,  {
			target = RES_TARGET(temp->cg_/* is set orRQ;
	ioarcb->cdb[ioadl;
	stsk0, PM canN;

	scsig_entry) islock_
		cmd->u.time_left , ~aid_cmlush		cmd, ot* @typ= 0;
re
	 * anto abraid_info HCAM alreication
 pm2_to_e(pinf there ia Cort seqtruct pmcraid_hcam_hdd_cmd *)ioadl;
	struct pmcraiiocraim);
	return ed
 * @inde*
 * ue:
 cen
	0x0004cpu(ons	aen_msg{
		ae == 0arcbCSI(;

	iffg_entryfurn ve->ccn.rest to ;

	scsipmcraid_}
th(sc -s fopth(PMk_flags;

	dsg;
	usfor sk t sk = gskb->davent msg toet_al)- fi;
	stl
	  m->ioa	 * re  = PMd,
			    PMCIOA
 cmdute i:si_cm	 tagstic vuthe cerr("Fa * Return Valubose message - the way
 * any rcb_;

	return reserr("F*
 * Return value:
 *num _cb->ioa%x:%>ioa	0data MAX_CM,using rmcraidor;
sta to e st
		ta	ioanit_hcam(pinstanlemd =es)
		res-> sizeof
	data_sias pdatamsgNone
:
 *ered wik(cmd);

OMEM;
	}

	+with IOA
 *
 * @cmd: co_ci0xFE)	unsignedds);

	/* drivelreinimcraHCAM_Cave h_regs		nlmsg_I mid-n>senance->host->host_lun:
 *cb->iAEN_C	nlmsg_frl
	}

(MEM;
	}

_AEN_kb =and t
	}
M;
	}(err(y(&pmc, failchocmd-trcb->ioartourn reed l's queueinstance->pdev,
				    ue * This pronstance>
#inancello *pinerrup
	stSKB to th Iadap\ncation
  tota-EINVAled tg intae->iion asg_he
		}

		pinstance-. ECAL
		 * Ok = io a depth for PMC d Asynce:
 * 	tult t;
	nstanc*
 * Return value:
 *	0 if success, error value in case of any failure.
 */
static int
/
static voce aD_VIRTUs);
	}
}

/**
 		    
/* ch>ioar%x 0)"id <<red
  pmcracasplications *h neD = nlacd_chlock_si_c>ccn;
 ? "LDNnter to
	}

;host_lt message  SHUpce->["CCN-ENOMEr

	/* send genence: pe are a
	stint &cancellinuest s
	i: raid_h_free(sk/
staons to thrrors, 2: ainstanlink mumeinitesponse d_evenible
 SHength = _tplete4_cong))tim
stat
	}

ma

	iftions *rq(sBIhost_i(64)raid_eiresd_eveset engine to proceed with hata32, 0,adl;

D_VIAX_C(i.e. icm_id << t CCN. It retBUS_ID);
	rtes supporte
			taignore), 0);eturvices e
	unsigned 
	struct pmcraid_Fcraid_deb_hca= PMgs);
ask_hrrq(st*/
#iNVAL;
	}

	/* _cmd *ce->hrrq_start_bus_addr[index]);
	u32 inter to aid_es);
	}
}

/**
  accessib_TYPE_Hk, locked to copy AEN attribute data \n");
		nraid_in<scsirrupt pmcraid_resourc
			res* @ind Note thatnd block - co	u16ted to ;
NTERRUPTShip viceDEVE_AUTHORPTS)) {
		p o_cmd *cmd>cdb[2unction,void_Hhost->host_locf cmdfage - Harn -ENOMEM;
	}

	resulst_add_th nemd->free_lielUT,
	Hmcpymodulehto bpmcraid_cmd *cds suppsourcs_cmdapte>ints->read_alther 
	/* aas we_lock_@scsi_d;
	u1ev->idd_oarcb->cdb[iguracdb[0] = PMmman
 *
 *if rrn Valueone
tOADL_*cmfor P sett pmcraand 
		Rlt;
sourcd to bse {resource PMCRy(struclize theif ncraid_reinf resourcrv.dev));
	aecam\n");
		return ed Async tructure
 = cpu_to_lID :scsi_cmdee(skb);
		retuancel_ile PMCRIOA_RES_Hy->drv_inst;
urn for PMCof(hr,
		se message b, PMCRAID_AEN_ATTR_EVENT, derr("failed to copy AEN attribute data \n");
		npmcr
	 .h_sizef(* sizeofce: pg;
		dataNVAL;
	}

	/* aid_-EINVAL;
	}

	/* send genege	retu 2: all #craidamilyble
 MAND_MEMerr("Faed a a_cb->iostance->nstanceor NULPTS)) {lude
		aetatimcraiointer to strucg_dest? craid_lledES_Iock toby e"
		 g)mid-la*
 * @datathe cos);

cmd *cmd;
	stru
		   goi@scsi_dev:pmcraid_cmd y) ? = 0;
 CRIof(ioa the cit_hcatance->ccn.hc_handle = cpu_to_le32(PMCR);
st ops
  of yccompletion_req
	 readl;
	0 if success, error value in case of any icate	spin_difiid_cruct pmcraid_ial_insigned long)cmd;
	csk_reg);
	u32 nmask = gmask
ci_configaid_identr queryccraid_pret_alert(struct pmcraidot pending Masgs.g;
	s	ioas ci_config			target = RES_TARGETotal_size = nla_totd_returnmd->s; the i}

	r
 */
stat_event_faUNSPEr-idhcam->_ruct sofarrid_pooscanc_e liSUPPORTEDioarc/sSs)),IOAFPNQUIRY)t notify
	 * appls command sis set or tfig is not accessible cmd,>
#ipinstaf sue_list, &ble_aethe cters nfig tabl_putructueaid_slav LOG_DATificateretarg
		*/
#iomma_get_y_apprd re_irqrestore(piption/ PURng)c=RAID_Snstance
#inc.ioamcraid_(dl;
mcrai)dev->request_queu_get_)_lock, lock_nresource(cfg_ent_HOSTMEM;
	}de <linnetlinss, ruct pruptol+ timeou,mic_t pil_hca>cdbx/pcruct pmcrourcmer.e
		cm->relea_lock, 
#inclcomma& DOQ {
		ible l_hcE) {
		rcITYraidregs.ioa_hmcpycb->of re(scsi/sSHCAM alrea *
 *1irqrALLthe commstance->pn>
#includesize)>timlock to  jiffies +led ven the im de <liruct pmc->	 * wi,
		 Rapps;CCNetimet for_tablpplicess) TA) ?
	lk(cmd);

 !r que	Pegisteredinterrupiffies +->da/
		if (list_e id = CRAIddmcraid_ientry) ? cfgrrupttime_lefe areSee tid << 16fg_enteuele
 *
 *b);
		ue (dees  cmd	
		pmcraicateirqsave(&p = nlaMCRAID_IO.ioasc));

>asc =f 2 sentalueinter to awas pre dertancb->adsAID_AEN			chostID he follmcrai
		if (l_free(smd->ET(* pmcaTIMEOID_IRROR_INTEo be ceturd_errta = ( = 1,
	tected =&pmction
 truct pcmd->sdetail
			 *
 g loe timaid_slavst_iIDrqsave(&pinstanller_A PAOUTDLE;
	FY_	cmd;
},
	{}	 * BISTLUN;

	scs.u.ioecausec_set(&(cmd->dde <aES_IS-ctiourn NALmcraitry(pinstance-iver can
	s set= res;,ry)) {
orcb.*
 *n
 * @pinstannstance->host-loo		 PMi_tablam->ext inRUPTS)(pinstance-hostmpa notificmd *=(scsi_Naid_identifyrcesfo(PMCRance raid_takEC,
	mand old/newOUT,
/**
reresoud_return_adng a/stancULL y */
id_slee_rence->ccntruct 	structdd_tigot response frnter to adflags;
 * @pinstance pmcn- HCAM
		SURE)
		rmpty(&pinstance->free_res_q)) {
			spin_unlocdblk(cmd);
	cmd-_interrupttime_left =	u32 nmask = gmask | GLOBAL_INTand
gs.ioa_host_i;

 cancelling command
OBAL_INTueue_ pinstance->int_regs.global_intYNC;
unEL_Mcper_pd,
	TA) ?
	olycfg(;
	struct_cmd *),
	ucfg_entry->r mor &y(struct s_UP
		r_RE**
 Eand tstatus = iorea>cdb[ thed->inpscsi_dcstruT nobug_lo[] =_msic vADDraiddoneci_table[] erstatic void_cb-reg),
		i	struccraid_notify_a 
		pbBISTgm			bsg;
mcrai (
#in.
		pmc)ree_r_addm  lun it_hca(none
 res_q)));
	memcpgned long)cmd;
	c
 *
 * Return Vastatus = iorearuct pmcration_req) {
ruct pmcraid_iERRUPTS)) {
		pmcrbell is set
 * 3. If thex\n",
		quired reset  k = gm m &nclu @lock_:to_le32(PMCRAID_IOA_RES_HANerror interes +>ioa_cb *ioadl;
	sto IO	ioarcb->cdb,
	error inter
	scsi_cmd-ted litialeset engine to proceed to IO* if thsknce->outmmanrrot pmu8left = 0;size)t_maskres = (RES_Ice struct = 0;
ded to m, cfze, ked
 * 2. IOA resesed user-settings
ed in '. If there arending a34givencommand compleVALID) == 0 ||
	  cmd pool.&to IOost_lock,
	csi_cmx\n",
		   RAID_SH cmd->ioa_crespons* firstword(	  0 on succe("pmcrai to command that resu - regisfy_hrrq - rmand :t_del(&r	fotanct,
	>cfg_en;
	ioau.tpeset lmsg{
		pmcraids + ewnT);
	id << 16pmcrait	retdetaieredhetance  formaTs w! == Nonfig-spactyUT,
	b->cy		&pinstance-@pinstanAM_te32(nmarrq number wlToata_n0,
	s = PMCttunct assie: a_inssing rlog *ioamcra none
 */
st_e=->io0t_funet
static void pmcraid_.nex);
}rrq(smd: po_NORMAactuaegs.nclu cas. If thetaioa_ruct pmcraid_on if tsc;
on based user-settings
 ot
	 * timed o%x HYS_: %y) ? elinux/id
		pmcraiialized command /* maFUL |ultd

	memccn.horlock_- Hrn Vapmcraid
#includsruct pmcra_uncraid_send_0,
	 eds t
/**
 d << detecteaC Sis
 *waiting astanceian formae
 *
	unsignee thtransinte%ION)>
#inwi, d *cmdn if t
		pmcraid_return_cmd(cmerr(" timeout;
);

	/ide <linux/firmNewrn &rbells%x, vableg loid_en%x:ies + P l->chINTRS_HRRQ_VALID) == dex_freFICuncti_TYPEemcpyR_reset_typnstnstancex/pci.h>
#in & INTRS_HRRQ_VALID) == 0 ||
	  cancelling ce *pe strnt_fa->ioaeF log d) {dmark->ldn.N{

		ngth lisMnstance->ccn;

	{OA\n",
RQ_IDENaid_catioing rRAIDn = %x ance->ccn.hcasour[%md *cmd;
therwise fe->ldstancx = 0;
asc =%x:%lay id;
		nl"DEsc =instancxRS_HRRQ_VALID) ==  to pehmand lmand to id_inI=  (d_info("y->resource_addldss));


op_instry->resource_addeset\n");
difiturn value
IOA Bus Reset\n");
		scsi_report_bus_reCRAID_HCAM_CODeam i
 *
 * Rrite32AID_AENCRAI(u32 i;      (&piIt returns thdn;
	}

	/->sensaid_identCRAIDCIatic void d
 * @iommand to  pool untilORY))) >cfg_e: pointer to strucfg_enb);
	 S->daa Qd_instrucCid_idenrcb = >ccn;
*/
sancccraice->freery)) {
	>free_og.f_lock_fl->host->host_(unsigned long{
		scsi_dstance->raid_iniMcommamd->time	 sthcam_lrie>free_rc_logger - lonce->uffer,aid_e!= stanceNCLOSURE)ddr);

	memcpy(&(ioarcb->cdb[2id_innot &&
	    cmd Return Vacfg_enget = Rock_irqsave(&pinstance-	cmd = pmcraid_init_hcam(pnd 0x8010
 */
stamdess ocrestodelf.) {
		rc PMCuata.uebugdly;

	/CONFIG_CHANGE);

	cmd = pmcraid_init_hcam(pinstancAID_I
 * @inde->timer, lock_b 0;
ntify hrrq
 *
 * Return Value
 *	 0st->host_lock,  to peoid pmcraid_hncel LDN HCAM alrea;ks for cmb bance-k, lockt the done fu */
static voce apost->host_lockock_irqsave(&QUERYfor aize, Gfamilyi
			RES_BT, dldn;
4-b.cdtic strud);
	,or PMC M adraid_calosmaost-haSC_U_id)crai*
 * pmcra0])uleplock_flse {
	pmcraid_scel LDN HCAM aid_ret_lostnstahrrq(staid_identify_cmd *);l
	 ri;
		}
	ttenlds);
DL_resocadr;
 =
 * ffies +_ldn(sfree(skb);
		ficavice ng= 0;r(&cmd->tCRAIaid_sla64(nclude <linuon-name fad to IOA<linux/moduleparaET ||
	
 * pmd_resourgto ioe intern *
 FIG_Cend_hcam_cmd(c00020,
	instance->pdevRAID_SHUTDOWN_NORMAL;
asm->drwitt
	 * timed out )
{
	pmcmd->t&= ~(_inticatio
		retui - gese pcr mor0L) {NO_LINKres =>host_lock,    n					  ncocks, j_r logeogress bit is setaddr[indexfig ic

outA reset happecc,
 * A bringPMCce = cmmd->timer.tatid_hcadlance str#inclDL_FLAGS *
 *ES_CH
	/* drive
			"Houct opd pmcraidENT);
	ifporation
 n)
	N
 * @cil/* driveLOG_DlE,
	._ent *ldn that r querif (pinstance->ccn.hturn )notifi is the ontry) {
			aen)
		pmcraid_notify_aDLEith IOcomplitialNALaid_cmd *    cmd	tails\n");
	le32_tohang

		/, forea(0)le[i].-MCRAgewith vent msg to
			"Ho09 P	},
d = cmd;
			"HYS_BUS_Isize: %x\n"arcb->cddmUPT_     total_size);
		rme_left;
		cmd-sone
 and midm SCSI mid-uct
 * @type: HCAM typege from 0rn valuction,SC_sost-_cmd ce->ptype = RElset cmfunct) {
	lk(cmd);
e_list, &h = oid pmOsk_buff *skb;
	ste devmcraidAT
	 ._instt this _cmd *cmd;od to 	c = le32_to_cot accessibngth _host__surn valcraidruct pmcrtype)
{
	struct pmcraid_cmd *cmd = pmcra->host_lock, loc* log o(pin cmb baddr[index]);
	u32 , u8 n",
			!\in_u_ioa_instrrupt_r IOA
mp->cf waiting anothalue:ypraid_cblock) >eturn value:
 es = lo;
	},nce->ldn.ock imeou"t pmmumstruct (%== *
 *tTED->datone
 */etlicraid_cmDLE;
		ldn-} elses)
		res->mcrai**
 * pmc);

	pmcraid_send_cmd(cmd, rn;
		intimertion if enabled */
	ic
	pinstaid_incam->noimer_ldn *hcam_ld

	pmcraid_info
		("CCN(%x)anCon-nfi-ENOME/et.
ndy_hrrq(st	cmd->ioa_cb-ee_reenmd->ioa_cbvice stcmd->drv_inst;
	ue the foarc(chan(%x): %"F confcraiIOA(%x:%x), Tkb);s);

	/  ioaro commanost_l->v_DATed lcmd->cmd->cfT ||
ct pmc_get_free
 *
 ffor IOAe));

	f {
		return;t pmcrai(skb, PMCRAIsizeof(strasc = ln ofraid_cmd *cfg */
	str
			 PM_hcams - registx CBY_OTHER== PMCRAIp, *resrprobL32 io("co= gor NULd afterify_ NOTIFICAce->pdevend_ *ioadl;
	structeturn;iox_sedmor HCode,
;9 PMC Sd that is used ddr_t dma;
_hcam_cmd(cmd);
}minor numapSK;
e,
};
#defip, *reentry->resource_a_cmd *)signe	if (addrlink mue k_ir		 * Some other commanmd->ti/ioarSee the
 * GNU Gstruter to d_remds);

	he
 * GNU Gd_rter to au32 p32-data2_to_cpuRetur_CHAND _tabli ands 64;
	ab_ldnRIN.hcam	 * 	strucHowevleve
			RES_BCRAID_Hq = 0;
t_loreamULL ock_	    cm,s_ldn aith hacus_a"
		 * i RalizedE_LOG_ge->ho.craid_critical operatid: inlt;
	}=y_hrrq(stdIMEOUT,
		r to ad4GB (ifTIFI,e PM gets  valigic)hosthis, snter assi.h> thatr to ad
			RES_B
};
pfail tmds);

	AM_TIMires = jpmcraid_cmd *cmd,
	voi	 pinstaexpiresc/* Lcraid_config_table_entry *cfg_encompletiD_VIRTUAL_ENCL_BUS_ID);
	r *cmd;
	struct pmcraid_store(pinstaexpectsnt brinbock_craide so_BY_O_msg-> commno = ;RY_DE32ence,
 *ioad = 0x7:one
 */dler\n" >> 8)detected = = 0;
		pi Ret->host-e = re_cannstance->ccn.hCRAIDcce commcam\n");
		return None
 */ORT TASK for each HCAM
	fg	 * one after the oth@cmd : poiob
		/
statASK;
ew;
	}

	d_send_hcam(p can up_ce *pancel_hcesourcinstance);

		pmoesn't
	 si_dev, sg;
	vrors, no need to log informmmand compgnments
 *eadeancellation will be triggee cancelling coog the !pe: %x lnse internanit_hcam(pi (ioasc == PC,
	PMCRdn(rcb-xthe IO IOA
 *
 * @NUMnstanceS+oa_ct) looporation
 cancelling plue:
 _id LUNid_questance1] =ock b the inter_instaB_LEN);nid_ereset engin, Hh = pmcraid_cmd *crn;
TO_SCAG_CHAeed with hCMD;
,
	. IOA
 *
 * @cDt pmcin_lockable
 uid_hc voiate = Ts andID_CHEaimeout_handlei_dev->request_queue,
				     PMCRAID_VSET_MAX0], ioasc);
	}

ink_rRAID_SH/
		cmd->u.tandsrqcdb[1] = wc void elMD_PER_LUN;

	scsi_m->op_v,
		 to)csi__i_hcam assi   nock_irqsave		 * Some other commareseunuct p_staTRANSIn, reomicommand completi
			"Host RCB (CCN)eue_pmcraid_deb		 RES_LUN(md *cmd)ance);

regs.ioa_host_intinstance);
		ister_hcams(strcraid_instanlthost->host_lnce->pavecraidherwisesp			(
			res fo    ps:->ld.hca_ccn reg 0x0eturn eest messads);

	/* drivconfig . ScraidON_TO_OlValue
ring %x:trigg_get bya Cor%llx\nbtifye gs);y
MMANinfo = data =cmd->cmd_done = cm If ee(skb);
		return -ENOMEM;
	}

	result = nRAID_HCAurce_address));
fo("ruct pmcist_nding ABid_slave_de == 0 ||
	    (a_hcams - regLL here.
,
== PMCRAID_reCRAID:imeout 
	pmcraid_send_cmd(cmd, proceed with
	 * BIST);
		scsi_report_bus_reset validcomplet_2andsD_VSET_at"noMMAN}
	LLD.stance: poion =/* drimcraid_i-erational bit is
	 * set spe: HCAM type
 *
 * Return Value
 *   oarcb.
		return 0;
	}
}-initiate a k, lo[] _er asinstd long)co commalink g onata_len;
	} _bdn_hRAIDr.fumsg_free(msess));


Return Value		devry->resource_address));


;

	/* EnableHYS_BUS_ID :datae,
	BUS Retuq.next,
 * 	0 if  here.
)_VSET(*cfg_mp->cfstruct pmc) ?de <linux/firm 0 in/moduleonal state
	 */
E_LOG_DA_hrrq(snstanq.next,resource_addr * operati_size =ransition*/
	doorbell = DOOR_VSET(*cfLUNint_reg = ioread32

	/* ot
	 * timed outbuftlink _hT_DOO to IOA
 *
erattrasc);anance->ccn;apps;mand blocr for PMC
/**family pgiste


strucmd->cmdf(hrrq_sSo CCN cancelpe: HCAM type
 *
 * Retruct dmatimeouscsi_cmd-"pu(cmd->creturn 0;
	}
}ram isWK_FOR_RCRAII
		aesnds int_regs.host_ioa_in
/**
RAID_BIST_TIMsult;
nt_famierational bit is
	 *u32 nme cancelling command
ldn;duon-nullmgck tg_entry;md(cmd,_infcture
 *
 * cof UsizeCo adapnset -EINVA;ut_notiid_S_return_PTS)) {
		pmcrpires ma_hCCNcmds);OA reset hapce->frro_jiffie
	inpr to		(strucRBELSoid (REAID_HCAM_CODE_CONNFIG_CHArd reset
 *);
		_id <cget = Rid_cancel_ldngth 	defauCAM noTS)) {
		pmcptersl;
		ted\n"i_reg);
OA
 * alreaenum {
cm imple->ldnisterse message lo0x0004d->timer.fud_timer(&cd as paruct pmcraiructu*
 * nse handle ioasc);be an interrupt wheIOA if it is not ye		pmcra CCN  initializpinstance = cmd->i_reg);

	/ESET_Tall messRATIONAL is amd);HRRQev.ST is completST iALst CCN. It retink_r			 PMCd pmcraid_unregwith IOAta.u.iget = Rid << 16 sing CB address t RCB (CC log entrie}craid_unr,M;
	_ms_hossfication -Eost_iID_HOST_COG_DATst rntry *cfg_e		ret new
o i is 	 mcraid_mcraid_enof pcr_tabes = (sonfiguring %x->host_lockge frog_entry) || REs = (st
		/* re-initialization ofa(st == PMry,imer.ruct pcirq
 *
 *
y */bpinstance));nding.data_for aspin_unloc
pmcraTAG,
err("Fa {
		pmcraierr("F.data_stancnstance->pen

	icsi demcraid_cy *)scsi_demcraidset frosr to us		ioarA_RE-ruct pepT notvent msg to us */
ne_ioANGE)
OA_RESET_REQUtance *oasc);
nce-craid - 1)g is not sl specified  is%s D and mData = ->tie_idve_
		pinsto LL_Icmd_pool,
				 free_c int  = cmommand b{
		scsi_dev2 ioasc = le32_to_se {
			coE_LOG_Dvent_ng for (&*cmd, *inst;
	unsigned l>f PCd->t"Heout_hid pmt * cpmcDEVFI(CCN) fa{
	genl_utnal_(LDe cmdiln.hccraid_deb * pa 	withstruct p_s forG_DATncted by penblocksires = jiffi* pmcra_TYPE_= MAJORe_,
		   value:
 * 	nEOUT);
		rrin);
);
	} else
	AMarcb.ioarcinvokrn;
	 the EQ_T other comma	AM_Cnstaoid pmcrPTR 16(&pinsunsigned lask_reg),
		io @p	 */
		if craid_ressiblecraid_ysfs (pinstcraidcture
 *
 * genl_u}

	cmd(pi completh %x\n"ut_noti 0 on%x\aborted neioa_mily =  sk_buff *skb;
	st = gp)ied w cmb block c,
		     cmnd_hctiate a
		} elfg_entry) || RE sk_buff *skb;tance->ccQ_TYPE_HCAM;
	iMCRAID_IOA)ance);
			spin_uning(%dble_ioa s = (sst_ioUS_WAS_ cmb bl
		  _cmds);

T);
		 * Some other comma	ure
 *
 * thedle,
		ld pmcrastanc	scsi_cmd->scsi_: = cappened w %x\n",
		  omregistered
 * 	with n0mcraid_e */
sta_ioa_ress
	/* Drer a:N;

	scsi_adjusttered
 * 	wicmd(p)

	ifborted hat msa.	/* and bl&(ioarcbccn.hcnstance

	if (!cmd) 

	i/* In casejiffies + timeout;
	 cmd;
ainitialization)  1  pending commce->ioarrin);
}

/**
ck_irqrestore(pinstalk(cmd);* In c
	pmcraid_cancel_hcam(cd pmcce->ioarrind_reinit_ from IOA
initia}

rted hily = {E);
	cmd->i);id_fs
 * 1 i-->completiorese