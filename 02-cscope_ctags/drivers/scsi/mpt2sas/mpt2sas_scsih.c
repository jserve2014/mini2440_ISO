/*
 * Scsi Host Layer for MPT (Message Passing Technology) based controllers
 *
 * This code is based on drivers/scsi/mpt2sas/mpt2_scsih.c
 * Copyright (C) 2007-2009  LSI Corporation
 *  (mailto:DL-MPTFusionLinux@lsi.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * NO WARRANTY
 * THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED INCLUDING, WITHOUT
 * LIMITATION, ANY WARRANTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Each Recipient is
 * solely responsible for determining the appropriateness of using and
 * distributing the Program and assumes all risks associated with its
 * exercise of rights under this Agreement, including but not limited to
 * the risks and costs of program errors, damage to or loss of data,
 * programs or equipment, and unavailability or interruption of operations.

 * DISCLAIMER OF LIABILITY
 * NEITHER RECIPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OR DISTRIBUTION OF THE PROGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
 * HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/blkdev.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/interrupt.h>

#include "mpt2sas_base.h"

MODULE_AUTHOR(MPT2SAS_AUTHOR);
MODULE_DESCRIPTION(MPT2SAS_DESCRIPTION);
MODULE_LICENSE("GPL");
MODULE_VERSION(MPT2SAS_DRIVER_VERSION);

#define RAID_CHANNEL 1

/* forward proto's */
static void _scsih_expander_node_remove(struct MPT2SAS_ADAPTER *ioc,
    struct _sas_node *sas_expander);
static void _firmware_event_work(struct work_struct *work);

/* global parameters */
LIST_HEAD(mpt2sas_ioc_list);

/* local parameters */
static u8 scsi_io_cb_idx = -1;
static u8 tm_cb_idx = -1;
static u8 ctl_cb_idx = -1;
static u8 base_cb_idx = -1;
static u8 transport_cb_idx = -1;
static u8 config_cb_idx = -1;
static int mpt_ids;

static u8 tm_tr_cb_idx = -1 ;
static u8 tm_sas_control_cb_idx = -1;

/* command line options */
static u32 logging_level;
MODULE_PARM_DESC(logging_level, " bits for enabling additional logging info "
    "(default=0)");

/* scsi-mid layer global parmeter is max_report_luns, which is 511 */
#define MPT2SAS_MAX_LUN (16895)
static int max_lun = MPT2SAS_MAX_LUN;
module_param(max_lun, int, 0);
MODULE_PARM_DESC(max_lun, " max lun, default=16895 ");

/**
 * struct sense_info - common structure for obtaining sense keys
 * @skey: sense key
 * @asc: additional sense code
 * @ascq: additional sense code qualifier
 */
struct sense_info {
	u8 skey;
	u8 asc;
	u8 ascq;
};


/**
 * struct fw_event_work - firmware event struct
 * @list: link list framework
 * @work: work object (ioc->fault_reset_work_q)
 * @ioc: per adapter object
 * @VF_ID: virtual function id
 * @VP_ID: virtual port id
 * @host_reset_handling: handling events during host reset
 * @ignore: flag meaning this event has been marked to ignore
 * @event: firmware event MPI2_EVENT_XXX defined in mpt2_ioc.h
 * @event_data: reply event data payload follows
 *
 * This object stored on ioc->fw_event_list.
 */
struct fw_event_work {
	struct list_head 	list;
	struct work_struct	work;
	struct MPT2SAS_ADAPTER *ioc;
	u8			VF_ID;
	u8			VP_ID;
	u8			host_reset_handling;
	u8			ignore;
	u16			event;
	void			*event_data;
};

/**
 * struct _scsi_io_transfer - scsi io transfer
 * @handle: sas device handle (assigned by firmware)
 * @is_raid: flag set for hidden raid components
 * @dir: DMA_TO_DEVICE, DMA_FROM_DEVICE,
 * @data_length: data transfer length
 * @data_dma: dma pointer to data
 * @sense: sense data
 * @lun: lun number
 * @cdb_length: cdb length
 * @cdb: cdb contents
 * @timeout: timeout for this command
 * @VF_ID: virtual function id
 * @VP_ID: virtual port id
 * @valid_reply: flag set for reply message
 * @sense_length: sense length
 * @ioc_status: ioc status
 * @scsi_state: scsi state
 * @scsi_status: scsi staus
 * @log_info: log information
 * @transfer_length: data length transfer when there is a reply message
 *
 * Used for sending internal scsi commands to devices within this module.
 * Refer to _scsi_send_scsi_io().
 */
struct _scsi_io_transfer {
	u16	handle;
	u8	is_raid;
	enum dma_data_direction dir;
	u32	data_length;
	dma_addr_t data_dma;
	u8 	sense[SCSI_SENSE_BUFFERSIZE];
	u32	lun;
	u8	cdb_length;
	u8	cdb[32];
	u8	timeout;
	u8	VF_ID;
	u8	VP_ID;
	u8	valid_reply;
  /* the following bits are only valid when 'valid_reply = 1' */
	u32	sense_length;
	u16	ioc_status;
	u8	scsi_state;
	u8	scsi_status;
	u32	log_info;
	u32	transfer_length;
};

/*
 * The pci device ids are defined in mpi/mpi2_cnfg.h.
 */
static struct pci_device_id scsih_pci_table[] = {
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2004,
		PCI_ANY_ID, PCI_ANY_ID },
	/* Falcon ~ 2008*/
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2008,
		PCI_ANY_ID, PCI_ANY_ID },
	/* Liberator ~ 2108 */
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2108_1,
		PCI_ANY_ID, PCI_ANY_ID },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2108_2,
		PCI_ANY_ID, PCI_ANY_ID },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2108_3,
		PCI_ANY_ID, PCI_ANY_ID },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2116_1,
		PCI_ANY_ID, PCI_ANY_ID },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2116_2,
		PCI_ANY_ID, PCI_ANY_ID },
	{0}	/* Terminating entry */
};
MODULE_DEVICE_TABLE(pci, scsih_pci_table);

/**
 * _scsih_set_debug_level - global setting of ioc->logging_level.
 *
 * Note: The logging levels are defined in mpt2sas_debug.h.
 */
static int
_scsih_set_debug_level(const char *val, struct kernel_param *kp)
{
	int ret = param_set_int(val, kp);
	struct MPT2SAS_ADAPTER *ioc;

	if (ret)
		return ret;

	printk(KERN_INFO "setting logging_level(0x%08x)\n", logging_level);
	list_for_each_entry(ioc, &mpt2sas_ioc_list, list)
		ioc->logging_level = logging_level;
	return 0;
}
module_param_call(logging_level, _scsih_set_debug_level, param_get_int,
    &logging_level, 0644);

/**
 * _scsih_srch_boot_sas_address - search based on sas_address
 * @sas_address: sas address
 * @boot_device: boot device object from bios page 2
 *
 * Returns 1 when there's a match, 0 means no match.
 */
static inline int
_scsih_srch_boot_sas_address(u64 sas_address,
    Mpi2BootDeviceSasWwid_t *boot_device)
{
	return (sas_address == le64_to_cpu(boot_device->SASAddress)) ?  1 : 0;
}

/**
 * _scsih_srch_boot_device_name - search based on device name
 * @device_name: device name specified in INDENTIFY fram
 * @boot_device: boot device object from bios page 2
 *
 * Returns 1 when there's a match, 0 means no match.
 */
static inline int
_scsih_srch_boot_device_name(u64 device_name,
    Mpi2BootDeviceDeviceName_t *boot_device)
{
	return (device_name == le64_to_cpu(boot_device->DeviceName)) ? 1 : 0;
}

/**
 * _scsih_srch_boot_encl_slot - search based on enclosure_logical_id/slot
 * @enclosure_logical_id: enclosure logical id
 * @slot_number: slot number
 * @boot_device: boot device object from bios page 2
 *
 * Returns 1 when there's a match, 0 means no match.
 */
static inline int
_scsih_srch_boot_encl_slot(u64 enclosure_logical_id, u16 slot_number,
    Mpi2BootDeviceEnclosureSlot_t *boot_device)
{
	return (enclosure_logical_id == le64_to_cpu(boot_device->
	    EnclosureLogicalID) && slot_number == le16_to_cpu(boot_device->
	    SlotNumber)) ? 1 : 0;
}

/**
 * _scsih_is_boot_device - search for matching boot device.
 * @sas_address: sas address
 * @device_name: device name specified in INDENTIFY fram
 * @enclosure_logical_id: enclosure logical id
 * @slot_number: slot number
 * @form: specifies boot device form
 * @boot_device: boot device object from bios page 2
 *
 * Returns 1 when there's a match, 0 means no match.
 */
static int
_scsih_is_boot_device(u64 sas_address, u64 device_name,
    u64 enclosure_logical_id, u16 slot, u8 form,
    Mpi2BiosPage2BootDevice_t *boot_device)
{
	int rc = 0;

	switch (form) {
	case MPI2_BIOSPAGE2_FORM_SAS_WWID:
		if (!sas_address)
			break;
		rc = _scsih_srch_boot_sas_address(
		    sas_address, &boot_device->SasWwid);
		break;
	case MPI2_BIOSPAGE2_FORM_ENCLOSURE_SLOT:
		if (!enclosure_logical_id)
			break;
		rc = _scsih_srch_boot_encl_slot(
		    enclosure_logical_id,
		    slot, &boot_device->EnclosureSlot);
		break;
	case MPI2_BIOSPAGE2_FORM_DEVICE_NAME:
		if (!device_name)
			break;
		rc = _scsih_srch_boot_device_name(
		    device_name, &boot_device->DeviceName);
		break;
	case MPI2_BIOSPAGE2_FORM_NO_DEVICE_SPECIFIED:
		break;
	}

	return rc;
}

/**
 * _scsih_determine_boot_device - determine boot device.
 * @ioc: per adapter object
 * @device: either sas_device or raid_device object
 * @is_raid: [flag] 1 = raid object, 0 = sas object
 *
 * Determines whether this device should be first reported device to
 * to scsi-ml or sas transport, this purpose is for persistant boot device.
 * There are primary, alternate, and current entries in bios page 2. The order
 * priority is primary, alternate, then current.  This routine saves
 * the corresponding device object and is_raid flag in the ioc object.
 * The saved data to be used later in _scsih_probe_boot_devices().
 */
static void
_scsih_determine_boot_device(struct MPT2SAS_ADAPTER *ioc,
    void *device, u8 is_raid)
{
	struct _sas_device *sas_device;
	struct _raid_device *raid_device;
	u64 sas_address;
	u64 device_name;
	u64 enclosure_logical_id;
	u16 slot;

	 /* only process this function when driver loads */
	if (!ioc->wait_for_port_enable_to_complete)
		return;

	if (!is_raid) {
		sas_device = device;
		sas_address = sas_device->sas_address;
		device_name = sas_device->device_name;
		enclosure_logical_id = sas_device->enclosure_logical_id;
		slot = sas_device->slot;
	} else {
		raid_device = device;
		sas_address = raid_device->wwid;
		device_name = 0;
		enclosure_logical_id = 0;
		slot = 0;
	}

	if (!ioc->req_boot_device.device) {
		if (_scsih_is_boot_device(sas_address, device_name,
		    enclosure_logical_id, slot,
		    (ioc->bios_pg2.ReqBootDeviceForm &
		    MPI2_BIOSPAGE2_FORM_MASK),
		    &ioc->bios_pg2.RequestedBootDevice)) {
			dinitprintk(ioc, printk(MPT2SAS_DEBUG_FMT
			   "%s: req_boot_device(0x%016llx)\n",
			    ioc->name, __func__,
			    (unsigned long long)sas_address));
			ioc->req_boot_device.device = device;
			ioc->req_boot_device.is_raid = is_raid;
		}
	}

	if (!ioc->req_alt_boot_device.device) {
		if (_scsih_is_boot_device(sas_address, device_name,
		    enclosure_logical_id, slot,
		    (ioc->bios_pg2.ReqAltBootDeviceForm &
		    MPI2_BIOSPAGE2_FORM_MASK),
		    &ioc->bios_pg2.RequestedAltBootDevice)) {
			dinitprintk(ioc, printk(MPT2SAS_DEBUG_FMT
			   "%s: req_alt_boot_device(0x%016llx)\n",
			    ioc->name, __func__,
			    (unsigned long long)sas_address));
			ioc->req_alt_boot_device.device = device;
			ioc->req_alt_boot_device.is_raid = is_raid;
		}
	}

	if (!ioc->current_boot_device.device) {
		if (_scsih_is_boot_device(sas_address, device_name,
		    enclosure_logical_id, slot,
		    (ioc->bios_pg2.CurrentBootDeviceForm &
		    MPI2_BIOSPAGE2_FORM_MASK),
		    &ioc->bios_pg2.CurrentBootDevice)) {
			dinitprintk(ioc, printk(MPT2SAS_DEBUG_FMT
			   "%s: current_boot_device(0x%016llx)\n",
			    ioc->name, __func__,
			    (unsigned long long)sas_address));
			ioc->current_boot_device.device = device;
			ioc->current_boot_device.is_raid = is_raid;
		}
	}
}

/**
 * mpt2sas_scsih_sas_device_find_by_sas_address - sas device search
 * @ioc: per adapter object
 * @sas_address: sas address
 * Context: Calling function should acquire ioc->sas_device_lock
 *
 * This searches for sas_device based on sas_address, then return sas_device
 * object.
 */
struct _sas_device *
mpt2sas_scsih_sas_device_find_by_sas_address(struct MPT2SAS_ADAPTER *ioc,
    u64 sas_address)
{
	struct _sas_device *sas_device, *r;

	r = NULL;
	/* check the sas_device_init_list */
	list_for_each_entry(sas_device, &ioc->sas_device_init_list,
	    list) {
		if (sas_device->sas_address != sas_address)
			continue;
		r = sas_device;
		goto out;
	}

	/* then check the sas_device_list */
	list_for_each_entry(sas_device, &ioc->sas_device_list, list) {
		if (sas_device->sas_address != sas_address)
			continue;
		r = sas_device;
		goto out;
	}
 out:
	return r;
}

/**
 * _scsih_sas_device_find_by_handle - sas device search
 * @ioc: per adapter object
 * @handle: sas device handle (assigned by firmware)
 * Context: Calling function should acquire ioc->sas_device_lock
 *
 * This searches for sas_device based on sas_address, then return sas_device
 * object.
 */
static struct _sas_device *
_scsih_sas_device_find_by_handle(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	struct _sas_device *sas_device, *r;

	r = NULL;
	if (ioc->wait_for_port_enable_to_complete) {
		list_for_each_entry(sas_device, &ioc->sas_device_init_list,
		    list) {
			if (sas_device->handle != handle)
				continue;
			r = sas_device;
			goto out;
		}
	} else {
		list_for_each_entry(sas_device, &ioc->sas_device_list, list) {
			if (sas_device->handle != handle)
				continue;
			r = sas_device;
			goto out;
		}
	}

 out:
	return r;
}

/**
 * _scsih_sas_device_remove - remove sas_device from list.
 * @ioc: per adapter object
 * @sas_device: the sas_device object
 * Context: This function will acquire ioc->sas_device_lock.
 *
 * Removing object and freeing associated memory from the ioc->sas_device_list.
 */
static void
_scsih_sas_device_remove(struct MPT2SAS_ADAPTER *ioc,
    struct _sas_device *sas_device)
{
	unsigned long flags;

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	list_del(&sas_device->list);
	memset(sas_device, 0, sizeof(struct _sas_device));
	kfree(sas_device);
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
}

/**
 * _scsih_sas_device_add - insert sas_device to the list.
 * @ioc: per adapter object
 * @sas_device: the sas_device object
 * Context: This function will acquire ioc->sas_device_lock.
 *
 * Adding new object to the ioc->sas_device_list.
 */
static void
_scsih_sas_device_add(struct MPT2SAS_ADAPTER *ioc,
    struct _sas_device *sas_device)
{
	unsigned long flags;
	u16 handle, parent_handle;
	u64 sas_address;

	dewtprintk(ioc, printk(MPT2SAS_DEBUG_FMT "%s: handle"
	    "(0x%04x), sas_addr(0x%016llx)\n", ioc->name, __func__,
	    sas_device->handle, (unsigned long long)sas_device->sas_address));

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	list_add_tail(&sas_device->list, &ioc->sas_device_list);
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);

	handle = sas_device->handle;
	parent_handle = sas_device->parent_handle;
	sas_address = sas_device->sas_address;
	if (!mpt2sas_transport_port_add(ioc, handle, parent_handle))
		_scsih_sas_device_remove(ioc, sas_device);
}

/**
 * _scsih_sas_device_init_add - insert sas_device to the list.
 * @ioc: per adapter object
 * @sas_device: the sas_device object
 * Context: This function will acquire ioc->sas_device_lock.
 *
 * Adding new object at driver load time to the ioc->sas_device_init_list.
 */
static void
_scsih_sas_device_init_add(struct MPT2SAS_ADAPTER *ioc,
    struct _sas_device *sas_device)
{
	unsigned long flags;

	dewtprintk(ioc, printk(MPT2SAS_DEBUG_FMT "%s: handle"
	    "(0x%04x), sas_addr(0x%016llx)\n", ioc->name, __func__,
	    sas_device->handle, (unsigned long long)sas_device->sas_address));

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	list_add_tail(&sas_device->list, &ioc->sas_device_init_list);
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
	_scsih_determine_boot_device(ioc, sas_device, 0);
}

/**
 * mpt2sas_scsih_expander_find_by_handle - expander device search
 * @ioc: per adapter object
 * @handle: expander handle (assigned by firmware)
 * Context: Calling function should acquire ioc->sas_device_lock
 *
 * This searches for expander device based on handle, then returns the
 * sas_node object.
 */
struct _sas_node *
mpt2sas_scsih_expander_find_by_handle(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	struct _sas_node *sas_expander, *r;

	r = NULL;
	list_for_each_entry(sas_expander, &ioc->sas_expander_list, list) {
		if (sas_expander->handle != handle)
			continue;
		r = sas_expander;
		goto out;
	}
 out:
	return r;
}

/**
 * _scsih_raid_device_find_by_id - raid device search
 * @ioc: per adapter object
 * @id: sas device target id
 * @channel: sas device channel
 * Context: Calling function should acquire ioc->raid_device_lock
 *
 * This searches for raid_device based on target id, then return raid_device
 * object.
 */
static struct _raid_device *
_scsih_raid_device_find_by_id(struct MPT2SAS_ADAPTER *ioc, int id, int channel)
{
	struct _raid_device *raid_device, *r;

	r = NULL;
	list_for_each_entry(raid_device, &ioc->raid_device_list, list) {
		if (raid_device->id == id && raid_device->channel == channel) {
			r = raid_device;
			goto out;
		}
	}

 out:
	return r;
}

/**
 * _scsih_raid_device_find_by_handle - raid device search
 * @ioc: per adapter object
 * @handle: sas device handle (assigned by firmware)
 * Context: Calling function should acquire ioc->raid_device_lock
 *
 * This searches for raid_device based on handle, then return raid_device
 * object.
 */
static struct _raid_device *
_scsih_raid_device_find_by_handle(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	struct _raid_device *raid_device, *r;

	r = NULL;
	list_for_each_entry(raid_device, &ioc->raid_device_list, list) {
		if (raid_device->handle != handle)
			continue;
		r = raid_device;
		goto out;
	}

 out:
	return r;
}

/**
 * _scsih_raid_device_find_by_wwid - raid device search
 * @ioc: per adapter object
 * @handle: sas device handle (assigned by firmware)
 * Context: Calling function should acquire ioc->raid_device_lock
 *
 * This searches for raid_device based on wwid, then return raid_device
 * object.
 */
static struct _raid_device *
_scsih_raid_device_find_by_wwid(struct MPT2SAS_ADAPTER *ioc, u64 wwid)
{
	struct _raid_device *raid_device, *r;

	r = NULL;
	list_for_each_entry(raid_device, &ioc->raid_device_list, list) {
		if (raid_device->wwid != wwid)
			continue;
		r = raid_device;
		goto out;
	}

 out:
	return r;
}

/**
 * _scsih_raid_device_add - add raid_device object
 * @ioc: per adapter object
 * @raid_device: raid_device object
 *
 * This is added to the raid_device_list link list.
 */
static void
_scsih_raid_device_add(struct MPT2SAS_ADAPTER *ioc,
    struct _raid_device *raid_device)
{
	unsigned long flags;

	dewtprintk(ioc, printk(MPT2SAS_DEBUG_FMT "%s: handle"
	    "(0x%04x), wwid(0x%016llx)\n", ioc->name, __func__,
	    raid_device->handle, (unsigned long long)raid_device->wwid));

	spin_lock_irqsave(&ioc->raid_device_lock, flags);
	list_add_tail(&raid_device->list, &ioc->raid_device_list);
	spin_unlock_irqrestore(&ioc->raid_device_lock, flags);
}

/**
 * _scsih_raid_device_remove - delete raid_device object
 * @ioc: per adapter object
 * @raid_device: raid_device object
 *
 * This is removed from the raid_device_list link list.
 */
static void
_scsih_raid_device_remove(struct MPT2SAS_ADAPTER *ioc,
    struct _raid_device *raid_device)
{
	unsigned long flags;

	spin_lock_irqsave(&ioc->raid_device_lock, flags);
	list_del(&raid_device->list);
	memset(raid_device, 0, sizeof(struct _raid_device));
	kfree(raid_device);
	spin_unlock_irqrestore(&ioc->raid_device_lock, flags);
}

/**
 * mpt2sas_scsih_expander_find_by_sas_address - expander device search
 * @ioc: per adapter object
 * @sas_address: sas address
 * Context: Calling function should acquire ioc->sas_node_lock.
 *
 * This searches for expander device based on sas_address, then returns the
 * sas_node object.
 */
struct _sas_node *
mpt2sas_scsih_expander_find_by_sas_address(struct MPT2SAS_ADAPTER *ioc,
    u64 sas_address)
{
	struct _sas_node *sas_expander, *r;

	r = NULL;
	list_for_each_entry(sas_expander, &ioc->sas_expander_list, list) {
		if (sas_expander->sas_address != sas_address)
			continue;
		r = sas_expander;
		goto out;
	}
 out:
	return r;
}

/**
 * _scsih_expander_node_add - insert expander device to the list.
 * @ioc: per adapter object
 * @sas_expander: the sas_device object
 * Context: This function will acquire ioc->sas_node_lock.
 *
 * Adding new object to the ioc->sas_expander_list.
 *
 * Return nothing.
 */
static void
_scsih_expander_node_add(struct MPT2SAS_ADAPTER *ioc,
    struct _sas_node *sas_expander)
{
	unsigned long flags;

	spin_lock_irqsave(&ioc->sas_node_lock, flags);
	list_add_tail(&sas_expander->list, &ioc->sas_expander_list);
	spin_unlock_irqrestore(&ioc->sas_node_lock, flags);
}

/**
 * _scsih_is_end_device - determines if device is an end device
 * @device_info: bitfield providing information about the device.
 * Context: none
 *
 * Returns 1 if end device.
 */
static int
_scsih_is_end_device(u32 device_info)
{
	if (device_info & MPI2_SAS_DEVICE_INFO_END_DEVICE &&
		((device_info & MPI2_SAS_DEVICE_INFO_SSP_TARGET) |
		(device_info & MPI2_SAS_DEVICE_INFO_STP_TARGET) |
		(device_info & MPI2_SAS_DEVICE_INFO_SATA_DEVICE)))
		return 1;
	else
		return 0;
}

/**
 * mptscsih_get_scsi_lookup - returns scmd entry
 * @ioc: per adapter object
 * @smid: system request message index
 *
 * Returns the smid stored scmd pointer.
 */
static struct scsi_cmnd *
_scsih_scsi_lookup_get(struct MPT2SAS_ADAPTER *ioc, u16 smid)
{
	return ioc->scsi_lookup[smid - 1].scmd;
}

/**
 * _scsih_scsi_lookup_find_by_scmd - scmd lookup
 * @ioc: per adapter object
 * @smid: system request message index
 * @scmd: pointer to scsi command object
 * Context: This function will acquire ioc->scsi_lookup_lock.
 *
 * This will search for a scmd pointer in the scsi_lookup array,
 * returning the revelent smid.  A returned value of zero means invalid.
 */
static u16
_scsih_scsi_lookup_find_by_scmd(struct MPT2SAS_ADAPTER *ioc, struct scsi_cmnd
    *scmd)
{
	u16 smid;
	unsigned long	flags;
	int i;

	spin_lock_irqsave(&ioc->scsi_lookup_lock, flags);
	smid = 0;
	for (i = 0; i < ioc->scsiio_depth; i++) {
		if (ioc->scsi_lookup[i].scmd == scmd) {
			smid = ioc->scsi_lookup[i].smid;
			goto out;
		}
	}
 out:
	spin_unlock_irqrestore(&ioc->scsi_lookup_lock, flags);
	return smid;
}

/**
 * _scsih_scsi_lookup_find_by_target - search for matching channel:id
 * @ioc: per adapter object
 * @id: target id
 * @channel: channel
 * Context: This function will acquire ioc->scsi_lookup_lock.
 *
 * This will search for a matching channel:id in the scsi_lookup array,
 * returning 1 if found.
 */
static u8
_scsih_scsi_lookup_find_by_target(struct MPT2SAS_ADAPTER *ioc, int id,
    int channel)
{
	u8 found;
	unsigned long	flags;
	int i;

	spin_lock_irqsave(&ioc->scsi_lookup_lock, flags);
	found = 0;
	for (i = 0 ; i < ioc->scsiio_depth; i++) {
		if (ioc->scsi_lookup[i].scmd &&
		    (ioc->scsi_lookup[i].scmd->device->id == id &&
		    ioc->scsi_lookup[i].scmd->device->channel == channel)) {
			found = 1;
			goto out;
		}
	}
 out:
	spin_unlock_irqrestore(&ioc->scsi_lookup_lock, flags);
	return found;
}

/**
 * _scsih_scsi_lookup_find_by_lun - search for matching channel:id:lun
 * @ioc: per adapter object
 * @id: target id
 * @lun: lun number
 * @channel: channel
 * Context: This function will acquire ioc->scsi_lookup_lock.
 *
 * This will search for a matching channel:id:lun in the scsi_lookup array,
 * returning 1 if found.
 */
static u8
_scsih_scsi_lookup_find_by_lun(struct MPT2SAS_ADAPTER *ioc, int id,
    unsigned int lun, int channel)
{
	u8 found;
	unsigned long	flags;
	int i;

	spin_lock_irqsave(&ioc->scsi_lookup_lock, flags);
	found = 0;
	for (i = 0 ; i < ioc->scsiio_depth; i++) {
		if (ioc->scsi_lookup[i].scmd &&
		    (ioc->scsi_lookup[i].scmd->device->id == id &&
		    ioc->scsi_lookup[i].scmd->device->channel == channel &&
		    ioc->scsi_lookup[i].scmd->device->lun == lun)) {
			found = 1;
			goto out;
		}
	}
 out:
	spin_unlock_irqrestore(&ioc->scsi_lookup_lock, flags);
	return found;
}

/**
 * _scsih_get_chain_buffer_dma - obtain block of chains (dma address)
 * @ioc: per adapter object
 * @smid: system request message index
 *
 * Returns phys pointer to chain buffer.
 */
static dma_addr_t
_scsih_get_chain_buffer_dma(struct MPT2SAS_ADAPTER *ioc, u16 smid)
{
	return ioc->chain_dma + ((smid - 1) * (ioc->request_sz *
	    ioc->chains_needed_per_io));
}

/**
 * _scsih_get_chain_buffer - obtain block of chains assigned to a mf request
 * @ioc: per adapter object
 * @smid: system request message index
 *
 * Returns virt pointer to chain buffer.
 */
static void *
_scsih_get_chain_buffer(struct MPT2SAS_ADAPTER *ioc, u16 smid)
{
	return (void *)(ioc->chain + ((smid - 1) * (ioc->request_sz *
	    ioc->chains_needed_per_io)));
}

/**
 * _scsih_build_scatter_gather - main sg creation routine
 * @ioc: per adapter object
 * @scmd: scsi command
 * @smid: system request message index
 * Context: none.
 *
 * The main routine that builds scatter gather table from a given
 * scsi request sent via the .queuecommand main handler.
 *
 * Returns 0 success, anything else error
 */
static int
_scsih_build_scatter_gather(struct MPT2SAS_ADAPTER *ioc,
    struct scsi_cmnd *scmd, u16 smid)
{
	Mpi2SCSIIORequest_t *mpi_request;
	dma_addr_t chain_dma;
	struct scatterlist *sg_scmd;
	void *sg_local, *chain;
	u32 chain_offset;
	u32 chain_length;
	u32 chain_flags;
	u32 sges_left;
	u32 sges_in_segment;
	u32 sgl_flags;
	u32 sgl_flags_last_element;
	u32 sgl_flags_end_buffer;

	mpi_request = mpt2sas_base_get_msg_frame(ioc, smid);

	/* init scatter gather flags */
	sgl_flags = MPI2_SGE_FLAGS_SIMPLE_ELEMENT;
	if (scmd->sc_data_direction == DMA_TO_DEVICE)
		sgl_flags |= MPI2_SGE_FLAGS_HOST_TO_IOC;
	sgl_flags_last_element = (sgl_flags | MPI2_SGE_FLAGS_LAST_ELEMENT)
	    << MPI2_SGE_FLAGS_SHIFT;
	sgl_flags_end_buffer = (sgl_flags | MPI2_SGE_FLAGS_LAST_ELEMENT |
	    MPI2_SGE_FLAGS_END_OF_BUFFER | MPI2_SGE_FLAGS_END_OF_LIST)
	    << MPI2_SGE_FLAGS_SHIFT;
	sgl_flags = sgl_flags << MPI2_SGE_FLAGS_SHIFT;

	sg_scmd = scsi_sglist(scmd);
	sges_left = scsi_dma_map(scmd);
	if (!sges_left) {
		sdev_printk(KERN_ERR, scmd->device, "pci_map_sg"
		" failed: request for %d bytes!\n", scsi_bufflen(scmd));
		return -ENOMEM;
	}

	sg_local = &mpi_request->SGL;
	sges_in_segment = ioc->max_sges_in_main_message;
	if (sges_left <= sges_in_segment)
		goto fill_in_last_segment;

	mpi_request->ChainOffset = (offsetof(Mpi2SCSIIORequest_t, SGL) +
	    (sges_in_segment * ioc->sge_size))/4;

	/* fill in main message segment when there is a chain following */
	while (sges_in_segment) {
		if (sges_in_segment == 1)
			ioc->base_add_sg_single(sg_local,
			    sgl_flags_last_element | sg_dma_len(sg_scmd),
			    sg_dma_address(sg_scmd));
		else
			ioc->base_add_sg_single(sg_local, sgl_flags |
			    sg_dma_len(sg_scmd), sg_dma_address(sg_scmd));
		sg_scmd = sg_next(sg_scmd);
		sg_local += ioc->sge_size;
		sges_left--;
		sges_in_segment--;
	}

	/* initializing the chain flags and pointers */
	chain_flags = MPI2_SGE_FLAGS_CHAIN_ELEMENT << MPI2_SGE_FLAGS_SHIFT;
	chain = _scsih_get_chain_buffer(ioc, smid);
	chain_dma = _scsih_get_chain_buffer_dma(ioc, smid);
	do {
		sges_in_segment = (sges_left <=
		    ioc->max_sges_in_chain_message) ? sges_left :
		    ioc->max_sges_in_chain_message;
		chain_offset = (sges_left == sges_in_segment) ?
		    0 : (sges_in_segment * ioc->sge_size)/4;
		chain_length = sges_in_segment * ioc->sge_size;
		if (chain_offset) {
			chain_offset = chain_offset <<
			    MPI2_SGE_CHAIN_OFFSET_SHIFT;
			chain_length += ioc->sge_size;
		}
		ioc->base_add_sg_single(sg_local, chain_flags | chain_offset |
		    chain_length, chain_dma);
		sg_local = chain;
		if (!chain_offset)
			goto fill_in_last_segment;

		/* fill in chain segments */
		while (sges_in_segment) {
			if (sges_in_segment == 1)
				ioc->base_add_sg_single(sg_local,
				    sgl_flags_last_element |
				    sg_dma_len(sg_scmd),
				    sg_dma_address(sg_scmd));
			else
				ioc->base_add_sg_single(sg_local, sgl_flags |
				    sg_dma_len(sg_scmd),
				    sg_dma_address(sg_scmd));
			sg_scmd = sg_next(sg_scmd);
			sg_local += ioc->sge_size;
			sges_left--;
			sges_in_segment--;
		}

		chain_dma += ioc->request_sz;
		chain += ioc->request_sz;
	} while (1);


 fill_in_last_segment:

	/* fill the last segment */
	while (sges_left) {
		if (sges_left == 1)
			ioc->base_add_sg_single(sg_local, sgl_flags_end_buffer |
			    sg_dma_len(sg_scmd), sg_dma_address(sg_scmd));
		else
			ioc->base_add_sg_single(sg_local, sgl_flags |
			    sg_dma_len(sg_scmd), sg_dma_address(sg_scmd));
		sg_scmd = sg_next(sg_scmd);
		sg_local += ioc->sge_size;
		sges_left--;
	}

	return 0;
}

/**
 * _scsih_change_queue_depth - setting device queue depth
 * @sdev: scsi device struct
 * @qdepth: requested queue depth
 *
 * Returns queue depth.
 */
static int
_scsih_change_queue_depth(struct scsi_device *sdev, int qdepth)
{
	struct Scsi_Host *shost = sdev->host;
	int max_depth;
	int tag_type;

	max_depth = shost->can_queue;
	if (!sdev->tagged_supported)
		max_depth = 1;
	if (qdepth > max_depth)
		qdepth = max_depth;
	tag_type = (qdepth == 1) ? 0 : MSG_SIMPLE_TAG;
	scsi_adjust_queue_depth(sdev, tag_type, qdepth);

	if (sdev->inquiry_len > 7)
		sdev_printk(KERN_INFO, sdev, "qdepth(%d), tagged(%d), "
		"simple(%d), ordered(%d), scsi_level(%d), cmd_que(%d)\n",
		sdev->queue_depth, sdev->tagged_supported, sdev->simple_tags,
		sdev->ordered_tags, sdev->scsi_level,
		(sdev->inquiry[7] & 2) >> 1);

	return sdev->queue_depth;
}

/**
 * _scsih_change_queue_type - changing device queue tag type
 * @sdev: scsi device struct
 * @tag_type: requested tag type
 *
 * Returns queue tag type.
 */
static int
_scsih_change_queue_type(struct scsi_device *sdev, int tag_type)
{
	if (sdev->tagged_supported) {
		scsi_set_tag_type(sdev, tag_type);
		if (tag_type)
			scsi_activate_tcq(sdev, sdev->queue_depth);
		else
			scsi_deactivate_tcq(sdev, sdev->queue_depth);
	} else
		tag_type = 0;

	return tag_type;
}

/**
 * _scsih_target_alloc - target add routine
 * @starget: scsi target struct
 *
 * Returns 0 if ok. Any other return is assumed to be an error and
 * the device is ignored.
 */
static int
_scsih_target_alloc(struct scsi_target *starget)
{
	struct Scsi_Host *shost = dev_to_shost(&starget->dev);
	struct MPT2SAS_ADAPTER *ioc = shost_priv(shost);
	struct MPT2SAS_TARGET *sas_target_priv_data;
	struct _sas_device *sas_device;
	struct _raid_device *raid_device;
	unsigned long flags;
	struct sas_rphy *rphy;

	sas_target_priv_data = kzalloc(sizeof(struct scsi_target), GFP_KERNEL);
	if (!sas_target_priv_data)
		return -ENOMEM;

	starget->hostdata = sas_target_priv_data;
	sas_target_priv_data->starget = starget;
	sas_target_priv_data->handle = MPT2SAS_INVALID_DEVICE_HANDLE;

	/* RAID volumes */
	if (starget->channel == RAID_CHANNEL) {
		spin_lock_irqsave(&ioc->raid_device_lock, flags);
		raid_device = _scsih_raid_device_find_by_id(ioc, starget->id,
		    starget->channel);
		if (raid_device) {
			sas_target_priv_data->handle = raid_device->handle;
			sas_target_priv_data->sas_address = raid_device->wwid;
			sas_target_priv_data->flags |= MPT_TARGET_FLAGS_VOLUME;
			raid_device->starget = starget;
		}
		spin_unlock_irqrestore(&ioc->raid_device_lock, flags);
		return 0;
	}

	/* sas/sata devices */
	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	rphy = dev_to_rphy(starget->dev.parent);
	sas_device = mpt2sas_scsih_sas_device_find_by_sas_address(ioc,
	   rphy->identify.sas_address);

	if (sas_device) {
		sas_target_priv_data->handle = sas_device->handle;
		sas_target_priv_data->sas_address = sas_device->sas_address;
		sas_device->starget = starget;
		sas_device->id = starget->id;
		sas_device->channel = starget->channel;
		if (sas_device->hidden_raid_component)
			sas_target_priv_data->flags |=
			    MPT_TARGET_FLAGS_RAID_COMPONENT;
	}
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);

	return 0;
}

/**
 * _scsih_target_destroy - target destroy routine
 * @starget: scsi target struct
 *
 * Returns nothing.
 */
static void
_scsih_target_destroy(struct scsi_target *starget)
{
	struct Scsi_Host *shost = dev_to_shost(&starget->dev);
	struct MPT2SAS_ADAPTER *ioc = shost_priv(shost);
	struct MPT2SAS_TARGET *sas_target_priv_data;
	struct _sas_device *sas_device;
	struct _raid_device *raid_device;
	unsigned long flags;
	struct sas_rphy *rphy;

	sas_target_priv_data = starget->hostdata;
	if (!sas_target_priv_data)
		return;

	if (starget->channel == RAID_CHANNEL) {
		spin_lock_irqsave(&ioc->raid_device_lock, flags);
		raid_device = _scsih_raid_device_find_by_id(ioc, starget->id,
		    starget->channel);
		if (raid_device) {
			raid_device->starget = NULL;
			raid_device->sdev = NULL;
		}
		spin_unlock_irqrestore(&ioc->raid_device_lock, flags);
		goto out;
	}

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	rphy = dev_to_rphy(starget->dev.parent);
	sas_device = mpt2sas_scsih_sas_device_find_by_sas_address(ioc,
	   rphy->identify.sas_address);
	if (sas_device && (sas_device->starget == starget) &&
	    (sas_device->id == starget->id) &&
	    (sas_device->channel == starget->channel))
		sas_device->starget = NULL;

	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);

 out:
	kfree(sas_target_priv_data);
	starget->hostdata = NULL;
}

/**
 * _scsih_slave_alloc - device add routine
 * @sdev: scsi device struct
 *
 * Returns 0 if ok. Any other return is assumed to be an error and
 * the device is ignored.
 */
static int
_scsih_slave_alloc(struct scsi_device *sdev)
{
	struct Scsi_Host *shost;
	struct MPT2SAS_ADAPTER *ioc;
	struct MPT2SAS_TARGET *sas_target_priv_data;
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct scsi_target *starget;
	struct _raid_device *raid_device;
	struct _sas_device *sas_device;
	unsigned long flags;

	sas_device_priv_data = kzalloc(sizeof(struct scsi_device), GFP_KERNEL);
	if (!sas_device_priv_data)
		return -ENOMEM;

	sas_device_priv_data->lun = sdev->lun;
	sas_device_priv_data->flags = MPT_DEVICE_FLAGS_INIT;

	starget = scsi_target(sdev);
	sas_target_priv_data = starget->hostdata;
	sas_target_priv_data->num_luns++;
	sas_device_priv_data->sas_target = sas_target_priv_data;
	sdev->hostdata = sas_device_priv_data;
	if ((sas_target_priv_data->flags & MPT_TARGET_FLAGS_RAID_COMPONENT))
		sdev->no_uld_attach = 1;

	shost = dev_to_shost(&starget->dev);
	ioc = shost_priv(shost);
	if (starget->channel == RAID_CHANNEL) {
		spin_lock_irqsave(&ioc->raid_device_lock, flags);
		raid_device = _scsih_raid_device_find_by_id(ioc,
		    starget->id, starget->channel);
		if (raid_device)
			raid_device->sdev = sdev; /* raid is single lun */
		spin_unlock_irqrestore(&ioc->raid_device_lock, flags);
	} else {
		/* set TLR bit for SSP devices */
		if (!(ioc->facts.IOCCapabilities &
		     MPI2_IOCFACTS_CAPABILITY_TLR))
			goto out;
		spin_lock_irqsave(&ioc->sas_device_lock, flags);
		sas_device = mpt2sas_scsih_sas_device_find_by_sas_address(ioc,
		   sas_device_priv_data->sas_target->sas_address);
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
		if (sas_device && sas_device->device_info &
		    MPI2_SAS_DEVICE_INFO_SSP_TARGET)
			sas_device_priv_data->flags |= MPT_DEVICE_TLR_ON;
	}

 out:
	return 0;
}

/**
 * _scsih_slave_destroy - device destroy routine
 * @sdev: scsi device struct
 *
 * Returns nothing.
 */
static void
_scsih_slave_destroy(struct scsi_device *sdev)
{
	struct MPT2SAS_TARGET *sas_target_priv_data;
	struct scsi_target *starget;

	if (!sdev->hostdata)
		return;

	starget = scsi_target(sdev);
	sas_target_priv_data = starget->hostdata;
	sas_target_priv_data->num_luns--;
	kfree(sdev->hostdata);
	sdev->hostdata = NULL;
}

/**
 * _scsih_display_sata_capabilities - sata capabilities
 * @ioc: per adapter object
 * @sas_device: the sas_device object
 * @sdev: scsi device struct
 */
static void
_scsih_display_sata_capabilities(struct MPT2SAS_ADAPTER *ioc,
    struct _sas_device *sas_device, struct scsi_device *sdev)
{
	Mpi2ConfigReply_t mpi_reply;
	Mpi2SasDevicePage0_t sas_device_pg0;
	u32 ioc_status;
	u16 flags;
	u32 device_info;

	if ((mpt2sas_config_get_sas_device_pg0(ioc, &mpi_reply, &sas_device_pg0,
	    MPI2_SAS_DEVICE_PGAD_FORM_HANDLE, sas_device->handle))) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return;
	}

	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return;
	}

	flags = le16_to_cpu(sas_device_pg0.Flags);
	device_info = le16_to_cpu(sas_device_pg0.DeviceInfo);

	sdev_printk(KERN_INFO, sdev,
	    "atapi(%s), ncq(%s), asyn_notify(%s), smart(%s), fua(%s), "
	    "sw_preserve(%s)\n",
	    (device_info & MPI2_SAS_DEVICE_INFO_ATAPI_DEVICE) ? "y" : "n",
	    (flags & MPI2_SAS_DEVICE0_FLAGS_SATA_NCQ_SUPPORTED) ? "y" : "n",
	    (flags & MPI2_SAS_DEVICE0_FLAGS_SATA_ASYNCHRONOUS_NOTIFY) ? "y" :
	    "n",
	    (flags & MPI2_SAS_DEVICE0_FLAGS_SATA_SMART_SUPPORTED) ? "y" : "n",
	    (flags & MPI2_SAS_DEVICE0_FLAGS_SATA_FUA_SUPPORTED) ? "y" : "n",
	    (flags & MPI2_SAS_DEVICE0_FLAGS_SATA_SW_PRESERVE) ? "y" : "n");
}

/**
 * _scsih_get_volume_capabilities - volume capabilities
 * @ioc: per adapter object
 * @sas_device: the raid_device object
 */
static void
_scsih_get_volume_capabilities(struct MPT2SAS_ADAPTER *ioc,
    struct _raid_device *raid_device)
{
	Mpi2RaidVolPage0_t *vol_pg0;
	Mpi2RaidPhysDiskPage0_t pd_pg0;
	Mpi2SasDevicePage0_t sas_device_pg0;
	Mpi2ConfigReply_t mpi_reply;
	u16 sz;
	u8 num_pds;

	if ((mpt2sas_config_get_number_pds(ioc, raid_device->handle,
	    &num_pds)) || !num_pds) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return;
	}

	raid_device->num_pds = num_pds;
	sz = offsetof(Mpi2RaidVolPage0_t, PhysDisk) + (num_pds *
	    sizeof(Mpi2RaidVol0PhysDisk_t));
	vol_pg0 = kzalloc(sz, GFP_KERNEL);
	if (!vol_pg0) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return;
	}

	if ((mpt2sas_config_get_raid_volume_pg0(ioc, &mpi_reply, vol_pg0,
	     MPI2_RAID_VOLUME_PGAD_FORM_HANDLE, raid_device->handle, sz))) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		kfree(vol_pg0);
		return;
	}

	raid_device->volume_type = vol_pg0->VolumeType;

	/* figure out what the underlying devices are by
	 * obtaining the device_info bits for the 1st device
	 */
	if (!(mpt2sas_config_get_phys_disk_pg0(ioc, &mpi_reply,
	    &pd_pg0, MPI2_PHYSDISK_PGAD_FORM_PHYSDISKNUM,
	    vol_pg0->PhysDisk[0].PhysDiskNum))) {
		if (!(mpt2sas_config_get_sas_device_pg0(ioc, &mpi_reply,
		    &sas_device_pg0, MPI2_SAS_DEVICE_PGAD_FORM_HANDLE,
		    le16_to_cpu(pd_pg0.DevHandle)))) {
			raid_device->device_info =
			    le32_to_cpu(sas_device_pg0.DeviceInfo);
		}
	}

	kfree(vol_pg0);
}

/**
 * _scsih_slave_configure - device configure routine.
 * @sdev: scsi device struct
 *
 * Returns 0 if ok. Any other return is assumed to be an error and
 * the device is ignored.
 */
static int
_scsih_slave_configure(struct scsi_device *sdev)
{
	struct Scsi_Host *shost = sdev->host;
	struct MPT2SAS_ADAPTER *ioc = shost_priv(shost);
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct MPT2SAS_TARGET *sas_target_priv_data;
	struct _sas_device *sas_device;
	struct _raid_device *raid_device;
	unsigned long flags;
	int qdepth;
	u8 ssp_target = 0;
	char *ds = "";
	char *r_level = "";

	qdepth = 1;
	sas_device_priv_data = sdev->hostdata;
	sas_device_priv_data->configured_lun = 1;
	sas_device_priv_data->flags &= ~MPT_DEVICE_FLAGS_INIT;
	sas_target_priv_data = sas_device_priv_data->sas_target;

	/* raid volume handling */
	if (sas_target_priv_data->flags & MPT_TARGET_FLAGS_VOLUME) {

		spin_lock_irqsave(&ioc->raid_device_lock, flags);
		raid_device = _scsih_raid_device_find_by_handle(ioc,
		     sas_target_priv_data->handle);
		spin_unlock_irqrestore(&ioc->raid_device_lock, flags);
		if (!raid_device) {
			printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
			    ioc->name, __FILE__, __LINE__, __func__);
			return 0;
		}

		_scsih_get_volume_capabilities(ioc, raid_device);

		/* RAID Queue Depth Support
		 * IS volume = underlying qdepth of drive type, either
		 *    MPT2SAS_SAS_QUEUE_DEPTH or MPT2SAS_SATA_QUEUE_DEPTH
		 * IM/IME/R10 = 128 (MPT2SAS_RAID_QUEUE_DEPTH)
		 */
		if (raid_device->device_info &
		    MPI2_SAS_DEVICE_INFO_SSP_TARGET) {
			qdepth = MPT2SAS_SAS_QUEUE_DEPTH;
			ds = "SSP";
		} else {
			qdepth = MPT2SAS_SATA_QUEUE_DEPTH;
			 if (raid_device->device_info &
			    MPI2_SAS_DEVICE_INFO_SATA_DEVICE)
				ds = "SATA";
			else
				ds = "STP";
		}

		switch (raid_device->volume_type) {
		case MPI2_RAID_VOL_TYPE_RAID0:
			r_level = "RAID0";
			break;
		case MPI2_RAID_VOL_TYPE_RAID1E:
			qdepth = MPT2SAS_RAID_QUEUE_DEPTH;
			if (ioc->manu_pg10.OEMIdentifier &&
			    (ioc->manu_pg10.GenericFlags0 &
			    MFG10_GF0_R10_DISPLAY) &&
			    !(raid_device->num_pds % 2))
				r_level = "RAID10";
			else
				r_level = "RAID1E";
			break;
		case MPI2_RAID_VOL_TYPE_RAID1:
			qdepth = MPT2SAS_RAID_QUEUE_DEPTH;
			r_level = "RAID1";
			break;
		case MPI2_RAID_VOL_TYPE_RAID10:
			qdepth = MPT2SAS_RAID_QUEUE_DEPTH;
			r_level = "RAID10";
			break;
		case MPI2_RAID_VOL_TYPE_UNKNOWN:
		default:
			qdepth = MPT2SAS_RAID_QUEUE_DEPTH;
			r_level = "RAIDX";
			break;
		}

		sdev_printk(KERN_INFO, sdev, "%s: "
		    "handle(0x%04x), wwid(0x%016llx), pd_count(%d), type(%s)\n",
		    r_level, raid_device->handle,
		    (unsigned long long)raid_device->wwid,
		    raid_device->num_pds, ds);
		_scsih_change_queue_depth(sdev, qdepth);
		return 0;
	}

	/* non-raid handling */
	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	sas_device = mpt2sas_scsih_sas_device_find_by_sas_address(ioc,
	   sas_device_priv_data->sas_target->sas_address);
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
	if (sas_device) {
		if (sas_target_priv_data->flags &
		    MPT_TARGET_FLAGS_RAID_COMPONENT) {
			mpt2sas_config_get_volume_handle(ioc,
			    sas_device->handle, &sas_device->volume_handle);
			mpt2sas_config_get_volume_wwid(ioc,
			    sas_device->volume_handle,
			    &sas_device->volume_wwid);
		}
		if (sas_device->device_info & MPI2_SAS_DEVICE_INFO_SSP_TARGET) {
			qdepth = MPT2SAS_SAS_QUEUE_DEPTH;
			ssp_target = 1;
			ds = "SSP";
		} else {
			qdepth = MPT2SAS_SATA_QUEUE_DEPTH;
			if (sas_device->device_info &
			    MPI2_SAS_DEVICE_INFO_STP_TARGET)
				ds = "STP";
			else if (sas_device->device_info &
			    MPI2_SAS_DEVICE_INFO_SATA_DEVICE)
				ds = "SATA";
		}

		sdev_printk(KERN_INFO, sdev, "%s: handle(0x%04x), "
		    "sas_addr(0x%016llx), device_name(0x%016llx)\n",
		    ds, sas_device->handle,
		    (unsigned long long)sas_device->sas_address,
		    (unsigned long long)sas_device->device_name);
		sdev_printk(KERN_INFO, sdev, "%s: "
		    "enclosure_logical_id(0x%016llx), slot(%d)\n", ds,
		    (unsigned long long) sas_device->enclosure_logical_id,
		    sas_device->slot);

		if (!ssp_target)
			_scsih_display_sata_capabilities(ioc, sas_device, sdev);
	}

	_scsih_change_queue_depth(sdev, qdepth);

	if (ssp_target)
		sas_read_port_mode_page(sdev);
	return 0;
}

/**
 * _scsih_bios_param - fetch head, sector, cylinder info for a disk
 * @sdev: scsi device struct
 * @bdev: pointer to block device context
 * @capacity: device size (in 512 byte sectors)
 * @params: three element array to place output:
 *              params[0] number of heads (max 255)
 *              params[1] number of sectors (max 63)
 *              params[2] number of cylinders
 *
 * Return nothing.
 */
static int
_scsih_bios_param(struct scsi_device *sdev, struct block_device *bdev,
    sector_t capacity, int params[])
{
	int		heads;
	int		sectors;
	sector_t	cylinders;
	ulong 		dummy;

	heads = 64;
	sectors = 32;

	dummy = heads * sectors;
	cylinders = capacity;
	sector_div(cylinders, dummy);

	/*
	 * Handle extended translation size for logical drives
	 * > 1Gb
	 */
	if ((ulong)capacity >= 0x200000) {
		heads = 255;
		sectors = 63;
		dummy = heads * sectors;
		cylinders = capacity;
		sector_div(cylinders, dummy);
	}

	/* return result */
	params[0] = heads;
	params[1] = sectors;
	params[2] = cylinders;

	return 0;
}

/**
 * _scsih_response_code - translation of device response code
 * @ioc: per adapter object
 * @response_code: response code returned by the device
 *
 * Return nothing.
 */
static void
_scsih_response_code(struct MPT2SAS_ADAPTER *ioc, u8 response_code)
{
	char *desc;

	switch (response_code) {
	case MPI2_SCSITASKMGMT_RSP_TM_COMPLETE:
		desc = "task management request completed";
		break;
	case MPI2_SCSITASKMGMT_RSP_INVALID_FRAME:
		desc = "invalid frame";
		break;
	case MPI2_SCSITASKMGMT_RSP_TM_NOT_SUPPORTED:
		desc = "task management request not supported";
		break;
	case MPI2_SCSITASKMGMT_RSP_TM_FAILED:
		desc = "task management request failed";
		break;
	case MPI2_SCSITASKMGMT_RSP_TM_SUCCEEDED:
		desc = "task management request succeeded";
		break;
	case MPI2_SCSITASKMGMT_RSP_TM_INVALID_LUN:
		desc = "invalid lun";
		break;
	case 0xA:
		desc = "overlapped tag attempted";
		break;
	case MPI2_SCSITASKMGMT_RSP_IO_QUEUED_ON_IOC:
		desc = "task queued, however not sent to target";
		break;
	default:
		desc = "unknown";
		break;
	}
	printk(MPT2SAS_WARN_FMT "response_code(0x%01x): %s\n",
		ioc->name, response_code, desc);
}

/**
 * _scsih_tm_done - tm completion routine
 * @ioc: per adapter object
 * @smid: system request message index
 * @msix_index: MSIX table index supplied by the OS
 * @reply: reply message frame(lower 32bit addr)
 * Context: none.
 *
 * The callback handler when using scsih_issue_tm.
 *
 * Return 1 meaning mf should be freed from _base_interrupt
 *        0 means the mf is freed from this function.
 */
static u8
_scsih_tm_done(struct MPT2SAS_ADAPTER *ioc, u16 smid, u8 msix_index, u32 reply)
{
	MPI2DefaultReply_t *mpi_reply;

	if (ioc->tm_cmds.status == MPT2_CMD_NOT_USED)
		return 1;
	if (ioc->tm_cmds.smid != smid)
		return 1;
	ioc->tm_cmds.status |= MPT2_CMD_COMPLETE;
	mpi_reply =  mpt2sas_base_get_reply_virt_addr(ioc, reply);
	if (mpi_reply) {
		memcpy(ioc->tm_cmds.reply, mpi_reply, mpi_reply->MsgLength*4);
		ioc->tm_cmds.status |= MPT2_CMD_REPLY_VALID;
	}
	ioc->tm_cmds.status &= ~MPT2_CMD_PENDING;
	complete(&ioc->tm_cmds.done);
	return 1;
}

/**
 * mpt2sas_scsih_set_tm_flag - set per target tm_busy
 * @ioc: per adapter object
 * @handle: device handle
 *
 * During taskmangement request, we need to freeze the device queue.
 */
void
mpt2sas_scsih_set_tm_flag(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct scsi_device *sdev;
	u8 skip = 0;

	shost_for_each_device(sdev, ioc->shost) {
		if (skip)
			continue;
		sas_device_priv_data = sdev->hostdata;
		if (!sas_device_priv_data)
			continue;
		if (sas_device_priv_data->sas_target->handle == handle) {
			sas_device_priv_data->sas_target->tm_busy = 1;
			skip = 1;
			ioc->ignore_loginfos = 1;
		}
	}
}

/**
 * mpt2sas_scsih_clear_tm_flag - clear per target tm_busy
 * @ioc: per adapter object
 * @handle: device handle
 *
 * During taskmangement request, we need to freeze the device queue.
 */
void
mpt2sas_scsih_clear_tm_flag(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct scsi_device *sdev;
	u8 skip = 0;

	shost_for_each_device(sdev, ioc->shost) {
		if (skip)
			continue;
		sas_device_priv_data = sdev->hostdata;
		if (!sas_device_priv_data)
			continue;
		if (sas_device_priv_data->sas_target->handle == handle) {
			sas_device_priv_data->sas_target->tm_busy = 0;
			skip = 1;
			ioc->ignore_loginfos = 0;
		}
	}
}

/**
 * mpt2sas_scsih_issue_tm - main routine for sending tm requests
 * @ioc: per adapter struct
 * @device_handle: device handle
 * @lun: lun number
 * @type: MPI2_SCSITASKMGMT_TASKTYPE__XXX (defined in mpi2_init.h)
 * @smid_task: smid assigned to the task
 * @timeout: timeout in seconds
 * Context: The calling function needs to acquire the tm_cmds.mutex
 *
 * A generic API for sending task management requests to firmware.
 *
 * The ioc->tm_cmds.status flag should be MPT2_CMD_NOT_USED before calling
 * this API.
 *
 * The callback index is set inside `ioc->tm_cb_idx`.
 *
 * Return nothing.
 */
void
mpt2sas_scsih_issue_tm(struct MPT2SAS_ADAPTER *ioc, u16 handle, uint lun,
    u8 type, u16 smid_task, ulong timeout)
{
	Mpi2SCSITaskManagementRequest_t *mpi_request;
	Mpi2SCSITaskManagementReply_t *mpi_reply;
	u16 smid = 0;
	u32 ioc_state;
	unsigned long timeleft;

	if (ioc->tm_cmds.status != MPT2_CMD_NOT_USED) {
		printk(MPT2SAS_INFO_FMT "%s: tm_cmd busy!!!\n",
		    __func__, ioc->name);
		return;
	}

	if (ioc->shost_recovery) {
		printk(MPT2SAS_INFO_FMT "%s: host reset in progress!\n",
		    __func__, ioc->name);
		return;
	}

	ioc_state = mpt2sas_base_get_iocstate(ioc, 0);
	if (ioc_state & MPI2_DOORBELL_USED) {
		dhsprintk(ioc, printk(MPT2SAS_DEBUG_FMT "unexpected doorbell "
		    "active!\n", ioc->name));
		goto issue_host_reset;
	}

	if ((ioc_state & MPI2_IOC_STATE_MASK) == MPI2_IOC_STATE_FAULT) {
		mpt2sas_base_fault_info(ioc, ioc_state &
		    MPI2_DOORBELL_DATA_MASK);
		goto issue_host_reset;
	}

	smid = mpt2sas_base_get_smid_hpr(ioc, ioc->tm_cb_idx);
	if (!smid) {
		printk(MPT2SAS_ERR_FMT "%s: failed obtaining a smid\n",
		    ioc->name, __func__);
		return;
	}

	dtmprintk(ioc, printk(MPT2SAS_INFO_FMT "sending tm: handle(0x%04x),"
	    " task_type(0x%02x), smid(%d)\n", ioc->name, handle, type,
	    smid_task));
	ioc->tm_cmds.status = MPT2_CMD_PENDING;
	mpi_request = mpt2sas_base_get_msg_frame(ioc, smid);
	ioc->tm_cmds.smid = smid;
	memset(mpi_request, 0, sizeof(Mpi2SCSITaskManagementRequest_t));
	mpi_request->Function = MPI2_FUNCTION_SCSI_TASK_MGMT;
	mpi_request->DevHandle = cpu_to_le16(handle);
	mpi_request->TaskType = type;
	mpi_request->TaskMID = cpu_to_le16(smid_task);
	mpi_request->VP_ID = 0;  /* TODO */
	mpi_request->VF_ID = 0;
	int_to_scsilun(lun, (struct scsi_lun *)mpi_request->LUN);
	mpt2sas_scsih_set_tm_flag(ioc, handle);
	init_completion(&ioc->tm_cmds.done);
	mpt2sas_base_put_smid_hi_priority(ioc, smid);
	timeleft = wait_for_completion_timeout(&ioc->tm_cmds.done, timeout*HZ);
	mpt2sas_scsih_clear_tm_flag(ioc, handle);
	if (!(ioc->tm_cmds.status & MPT2_CMD_COMPLETE)) {
		printk(MPT2SAS_ERR_FMT "%s: timeout\n",
		    ioc->name, __func__);
		_debug_dump_mf(mpi_request,
		    sizeof(Mpi2SCSITaskManagementRequest_t)/4);
		if (!(ioc->tm_cmds.status & MPT2_CMD_RESET))
			goto issue_host_reset;
	}

	if (ioc->tm_cmds.status & MPT2_CMD_REPLY_VALID) {
		mpi_reply = ioc->tm_cmds.reply;
		dtmprintk(ioc, printk(MPT2SAS_INFO_FMT "complete tm: "
		    "ioc_status(0x%04x), loginfo(0x%08x), term_count(0x%08x)\n",
		    ioc->name, le16_to_cpu(mpi_reply->IOCStatus),
		    le32_to_cpu(mpi_reply->IOCLogInfo),
		    le32_to_cpu(mpi_reply->TerminationCount)));
		if (ioc->logging_level & MPT_DEBUG_TM)
			_scsih_response_code(ioc, mpi_reply->ResponseCode);
	}
	return;
 issue_host_reset:
	mpt2sas_base_hard_reset_handler(ioc, CAN_SLEEP, FORCE_BIG_HAMMER);
}

/**
 * _scsih_abort - eh threads main abort routine
 * @sdev: scsi device struct
 *
 * Returns SUCCESS if command aborted else FAILED
 */
static int
_scsih_abort(struct scsi_cmnd *scmd)
{
	struct MPT2SAS_ADAPTER *ioc = shost_priv(scmd->device->host);
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	u16 smid;
	u16 handle;
	int r;
	struct scsi_cmnd *scmd_lookup;

	printk(MPT2SAS_INFO_FMT "attempting task abort! scmd(%p)\n",
	    ioc->name, scmd);
	scsi_print_command(scmd);

	sas_device_priv_data = scmd->device->hostdata;
	if (!sas_device_priv_data || !sas_device_priv_data->sas_target) {
		printk(MPT2SAS_INFO_FMT "device been deleted! scmd(%p)\n",
		    ioc->name, scmd);
		scmd->result = DID_NO_CONNECT << 16;
		scmd->scsi_done(scmd);
		r = SUCCESS;
		goto out;
	}

	/* search for the command */
	smid = _scsih_scsi_lookup_find_by_scmd(ioc, scmd);
	if (!smid) {
		scmd->result = DID_RESET << 16;
		r = SUCCESS;
		goto out;
	}

	/* for hidden raid components and volumes this is not supported */
	if (sas_device_priv_data->sas_target->flags &
	    MPT_TARGET_FLAGS_RAID_COMPONENT ||
	    sas_device_priv_data->sas_target->flags & MPT_TARGET_FLAGS_VOLUME) {
		scmd->result = DID_RESET << 16;
		r = FAILED;
		goto out;
	}

	mutex_lock(&ioc->tm_cmds.mutex);
	handle = sas_device_priv_data->sas_target->handle;
	mpt2sas_scsih_issue_tm(ioc, handle, sas_device_priv_data->lun,
	    MPI2_SCSITASKMGMT_TASKTYPE_ABORT_TASK, smid, 30);

	/* sanity check - see whether command actually completed */
	scmd_lookup = _scsih_scsi_lookup_get(ioc, smid);
	if (scmd_lookup && (scmd_lookup->serial_number == scmd->serial_number))
		r = FAILED;
	else
		r = SUCCESS;
	ioc->tm_cmds.status = MPT2_CMD_NOT_USED;
	mutex_unlock(&ioc->tm_cmds.mutex);

 out:
	printk(MPT2SAS_INFO_FMT "task abort: %s scmd(%p)\n",
	    ioc->name, ((r == SUCCESS) ? "SUCCESS" : "FAILED"), scmd);
	return r;
}

/**
 * _scsih_dev_reset - eh threads main device reset routine
 * @sdev: scsi device struct
 *
 * Returns SUCCESS if command aborted else FAILED
 */
static int
_scsih_dev_reset(struct scsi_cmnd *scmd)
{
	struct MPT2SAS_ADAPTER *ioc = shost_priv(scmd->device->host);
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct _sas_device *sas_device;
	unsigned long flags;
	u16	handle;
	int r;

	printk(MPT2SAS_INFO_FMT "attempting device reset! scmd(%p)\n",
	    ioc->name, scmd);
	scsi_print_command(scmd);

	sas_device_priv_data = scmd->device->hostdata;
	if (!sas_device_priv_data || !sas_device_priv_data->sas_target) {
		printk(MPT2SAS_INFO_FMT "device been deleted! scmd(%p)\n",
		    ioc->name, scmd);
		scmd->result = DID_NO_CONNECT << 16;
		scmd->scsi_done(scmd);
		r = SUCCESS;
		goto out;
	}

	/* for hidden raid components obtain the volume_handle */
	handle = 0;
	if (sas_device_priv_data->sas_target->flags &
	    MPT_TARGET_FLAGS_RAID_COMPONENT) {
		spin_lock_irqsave(&ioc->sas_device_lock, flags);
		sas_device = _scsih_sas_device_find_by_handle(ioc,
		   sas_device_priv_data->sas_target->handle);
		if (sas_device)
			handle = sas_device->volume_handle;
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
	} else
		handle = sas_device_priv_data->sas_target->handle;

	if (!handle) {
		scmd->result = DID_RESET << 16;
		r = FAILED;
		goto out;
	}

	mutex_lock(&ioc->tm_cmds.mutex);
	mpt2sas_scsih_issue_tm(ioc, handle, 0,
	    MPI2_SCSITASKMGMT_TASKTYPE_LOGICAL_UNIT_RESET, scmd->device->lun,
	    30);

	/*
	 *  sanity check see whether all commands to this device been
	 *  completed
	 */
	if (_scsih_scsi_lookup_find_by_lun(ioc, scmd->device->id,
	    scmd->device->lun, scmd->device->channel))
		r = FAILED;
	else
		r = SUCCESS;
	ioc->tm_cmds.status = MPT2_CMD_NOT_USED;
	mutex_unlock(&ioc->tm_cmds.mutex);

 out:
	printk(MPT2SAS_INFO_FMT "device reset: %s scmd(%p)\n",
	    ioc->name, ((r == SUCCESS) ? "SUCCESS" : "FAILED"), scmd);
	return r;
}

/**
 * _scsih_target_reset - eh threads main target reset routine
 * @sdev: scsi device struct
 *
 * Returns SUCCESS if command aborted else FAILED
 */
static int
_scsih_target_reset(struct scsi_cmnd *scmd)
{
	struct MPT2SAS_ADAPTER *ioc = shost_priv(scmd->device->host);
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct _sas_device *sas_device;
	unsigned long flags;
	u16	handle;
	int r;

	printk(MPT2SAS_INFO_FMT "attempting target reset! scmd(%p)\n",
	    ioc->name, scmd);
	scsi_print_command(scmd);

	sas_device_priv_data = scmd->device->hostdata;
	if (!sas_device_priv_data || !sas_device_priv_data->sas_target) {
		printk(MPT2SAS_INFO_FMT "target been deleted! scmd(%p)\n",
		    ioc->name, scmd);
		scmd->result = DID_NO_CONNECT << 16;
		scmd->scsi_done(scmd);
		r = SUCCESS;
		goto out;
	}

	/* for hidden raid components obtain the volume_handle */
	handle = 0;
	if (sas_device_priv_data->sas_target->flags &
	    MPT_TARGET_FLAGS_RAID_COMPONENT) {
		spin_lock_irqsave(&ioc->sas_device_lock, flags);
		sas_device = _scsih_sas_device_find_by_handle(ioc,
		   sas_device_priv_data->sas_target->handle);
		if (sas_device)
			handle = sas_device->volume_handle;
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
	} else
		handle = sas_device_priv_data->sas_target->handle;

	if (!handle) {
		scmd->result = DID_RESET << 16;
		r = FAILED;
		goto out;
	}

	mutex_lock(&ioc->tm_cmds.mutex);
	mpt2sas_scsih_issue_tm(ioc, handle, 0,
	    MPI2_SCSITASKMGMT_TASKTYPE_TARGET_RESET, 0, 30);

	/*
	 *  sanity check see whether all commands to this target been
	 *  completed
	 */
	if (_scsih_scsi_lookup_find_by_target(ioc, scmd->device->id,
	    scmd->device->channel))
		r = FAILED;
	else
		r = SUCCESS;
	ioc->tm_cmds.status = MPT2_CMD_NOT_USED;
	mutex_unlock(&ioc->tm_cmds.mutex);

 out:
	printk(MPT2SAS_INFO_FMT "target reset: %s scmd(%p)\n",
	    ioc->name, ((r == SUCCESS) ? "SUCCESS" : "FAILED"), scmd);
	return r;
}

/**
 * _scsih_host_reset - eh threads main host reset routine
 * @sdev: scsi device struct
 *
 * Returns SUCCESS if command aborted else FAILED
 */
static int
_scsih_host_reset(struct scsi_cmnd *scmd)
{
	struct MPT2SAS_ADAPTER *ioc = shost_priv(scmd->device->host);
	int r, retval;

	printk(MPT2SAS_INFO_FMT "attempting host reset! scmd(%p)\n",
	    ioc->name, scmd);
	scsi_print_command(scmd);

	retval = mpt2sas_base_hard_reset_handler(ioc, CAN_SLEEP,
	    FORCE_BIG_HAMMER);
	r = (retval < 0) ? FAILED : SUCCESS;
	printk(MPT2SAS_INFO_FMT "host reset: %s scmd(%p)\n",
	    ioc->name, ((r == SUCCESS) ? "SUCCESS" : "FAILED"), scmd);

	return r;
}

/**
 * _scsih_fw_event_add - insert and queue up fw_event
 * @ioc: per adapter object
 * @fw_event: object describing the event
 * Context: This function will acquire ioc->fw_event_lock.
 *
 * This adds the firmware event object into link list, then queues it up to
 * be processed from user context.
 *
 * Return nothing.
 */
static void
_scsih_fw_event_add(struct MPT2SAS_ADAPTER *ioc, struct fw_event_work *fw_event)
{
	unsigned long flags;

	if (ioc->firmware_event_thread == NULL)
		return;

	spin_lock_irqsave(&ioc->fw_event_lock, flags);
	list_add_tail(&fw_event->list, &ioc->fw_event_list);
	INIT_WORK(&fw_event->work, _firmware_event_work);
	queue_work(ioc->firmware_event_thread, &fw_event->work);
	spin_unlock_irqrestore(&ioc->fw_event_lock, flags);
}

/**
 * _scsih_fw_event_free - delete fw_event
 * @ioc: per adapter object
 * @fw_event: object describing the event
 * Context: This function will acquire ioc->fw_event_lock.
 *
 * This removes firmware event object from link list, frees associated memory.
 *
 * Return nothing.
 */
static void
_scsih_fw_event_free(struct MPT2SAS_ADAPTER *ioc, struct fw_event_work
    *fw_event)
{
	unsigned long flags;

	spin_lock_irqsave(&ioc->fw_event_lock, flags);
	list_del(&fw_event->list);
	kfree(fw_event->event_data);
	kfree(fw_event);
	spin_unlock_irqrestore(&ioc->fw_event_lock, flags);
}

/**
 * _scsih_fw_event_add - requeue an event
 * @ioc: per adapter object
 * @fw_event: object describing the event
 * Context: This function will acquire ioc->fw_event_lock.
 *
 * Return nothing.
 */
static void
_scsih_fw_event_requeue(struct MPT2SAS_ADAPTER *ioc, struct fw_event_work
    *fw_event, unsigned long delay)
{
	unsigned long flags;
	if (ioc->firmware_event_thread == NULL)
		return;

	spin_lock_irqsave(&ioc->fw_event_lock, flags);
	queue_work(ioc->firmware_event_thread, &fw_event->work);
	spin_unlock_irqrestore(&ioc->fw_event_lock, flags);
}

/**
 * _scsih_fw_event_off - turn flag off preventing event handling
 * @ioc: per adapter object
 *
 * Used to prevent handling of firmware events during adapter reset
 * driver unload.
 *
 * Return nothing.
 */
static void
_scsih_fw_event_off(struct MPT2SAS_ADAPTER *ioc)
{
	unsigned long flags;

	spin_lock_irqsave(&ioc->fw_event_lock, flags);
	ioc->fw_events_off = 1;
	spin_unlock_irqrestore(&ioc->fw_event_lock, flags);

}

/**
 * _scsih_fw_event_on - turn flag on allowing firmware event handling
 * @ioc: per adapter object
 *
 * Returns nothing.
 */
static void
_scsih_fw_event_on(struct MPT2SAS_ADAPTER *ioc)
{
	unsigned long flags;

	spin_lock_irqsave(&ioc->fw_event_lock, flags);
	ioc->fw_events_off = 0;
	spin_unlock_irqrestore(&ioc->fw_event_lock, flags);
}

/**
 * _scsih_ublock_io_device - set the device state to SDEV_RUNNING
 * @ioc: per adapter object
 * @handle: device handle
 *
 * During device pull we need to appropiately set the sdev state.
 */
static void
_scsih_ublock_io_device(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct scsi_device *sdev;

	shost_for_each_device(sdev, ioc->shost) {
		sas_device_priv_data = sdev->hostdata;
		if (!sas_device_priv_data)
			continue;
		if (!sas_device_priv_data->block)
			continue;
		if (sas_device_priv_data->sas_target->handle == handle) {
			dewtprintk(ioc, sdev_printk(KERN_INFO, sdev,
			    MPT2SAS_INFO_FMT "SDEV_RUNNING: "
			    "handle(0x%04x)\n", ioc->name, handle));
			sas_device_priv_data->block = 0;
			scsi_internal_device_unblock(sdev);
		}
	}
}

/**
 * _scsih_block_io_device - set the device state to SDEV_BLOCK
 * @ioc: per adapter object
 * @handle: device handle
 *
 * During device pull we need to appropiately set the sdev state.
 */
static void
_scsih_block_io_device(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct scsi_device *sdev;

	shost_for_each_device(sdev, ioc->shost) {
		sas_device_priv_data = sdev->hostdata;
		if (!sas_device_priv_data)
			continue;
		if (sas_device_priv_data->block)
			continue;
		if (sas_device_priv_data->sas_target->handle == handle) {
			dewtprintk(ioc, sdev_printk(KERN_INFO, sdev,
			    MPT2SAS_INFO_FMT "SDEV_BLOCK: "
			    "handle(0x%04x)\n", ioc->name, handle));
			sas_device_priv_data->block = 1;
			scsi_internal_device_block(sdev);
		}
	}
}

/**
 * _scsih_block_io_to_children_attached_to_ex
 * @ioc: per adapter object
 * @sas_expander: the sas_device object
 *
 * This routine set sdev state to SDEV_BLOCK for all devices
 * attached to this expander. This function called when expander is
 * pulled.
 */
static void
_scsih_block_io_to_children_attached_to_ex(struct MPT2SAS_ADAPTER *ioc,
    struct _sas_node *sas_expander)
{
	struct _sas_port *mpt2sas_port;
	struct _sas_device *sas_device;
	struct _sas_node *expander_sibling;
	unsigned long flags;

	if (!sas_expander)
		return;

	list_for_each_entry(mpt2sas_port,
	   &sas_expander->sas_port_list, port_list) {
		if (mpt2sas_port->remote_identify.device_type ==
		    SAS_END_DEVICE) {
			spin_lock_irqsave(&ioc->sas_device_lock, flags);
			sas_device =
			    mpt2sas_scsih_sas_device_find_by_sas_address(ioc,
			   mpt2sas_port->remote_identify.sas_address);
			spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
			if (!sas_device)
				continue;
			_scsih_block_io_device(ioc, sas_device->handle);
		}
	}

	list_for_each_entry(mpt2sas_port,
	   &sas_expander->sas_port_list, port_list) {

		if (mpt2sas_port->remote_identify.device_type ==
		    MPI2_SAS_DEVICE_INFO_EDGE_EXPANDER ||
		    mpt2sas_port->remote_identify.device_type ==
		    MPI2_SAS_DEVICE_INFO_FANOUT_EXPANDER) {

			spin_lock_irqsave(&ioc->sas_node_lock, flags);
			expander_sibling =
			    mpt2sas_scsih_expander_find_by_sas_address(
			    ioc, mpt2sas_port->remote_identify.sas_address);
			spin_unlock_irqrestore(&ioc->sas_node_lock, flags);
			_scsih_block_io_to_children_attached_to_ex(ioc,
			    expander_sibling);
		}
	}
}

/**
 * _scsih_block_io_to_children_attached_directly
 * @ioc: per adapter object
 * @event_data: topology change event data
 *
 * This routine set sdev state to SDEV_BLOCK for all devices
 * direct attached during device pull.
 */
static void
_scsih_block_io_to_children_attached_directly(struct MPT2SAS_ADAPTER *ioc,
    Mpi2EventDataSasTopologyChangeList_t *event_data)
{
	int i;
	u16 handle;
	u16 reason_code;
	u8 phy_number;
	u8 link_rate;

	for (i = 0; i < event_data->NumEntries; i++) {
		handle = le16_to_cpu(event_data->PHY[i].AttachedDevHandle);
		if (!handle)
			continue;
		phy_number = event_data->StartPhyNum + i;
		reason_code = event_data->PHY[i].PhyStatus &
		    MPI2_EVENT_SAS_TOPO_RC_MASK;
		if (reason_code == MPI2_EVENT_SAS_TOPO_RC_DELAY_NOT_RESPONDING)
			_scsih_block_io_device(ioc, handle);
		if (reason_code == MPI2_EVENT_SAS_TOPO_RC_PHY_CHANGED) {
			link_rate = event_data->PHY[i].LinkRate >> 4;
			if (link_rate >= MPI2_SAS_NEG_LINK_RATE_1_5)
				_scsih_ublock_io_device(ioc, handle);
		}
	}
}

/**
 * _scsih_tm_tr_send - send task management request
 * @ioc: per adapter object
 * @handle: device handle
 * Context: interrupt time.
 *
 * This code is to initiate the device removal handshake protocal
 * with controller firmware.  This function will issue target reset
 * using high priority request queue.  It will send a sas iounit
 * controll request (MPI2_SAS_OP_REMOVE_DEVICE) from this completion.
 *
 * This is designed to send muliple task management request at the same
 * time to the fifo. If the fifo is full, we will append the request,
 * and process it in a future completion.
 */
static void
_scsih_tm_tr_send(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	Mpi2SCSITaskManagementRequest_t *mpi_request;
	struct MPT2SAS_TARGET *sas_target_priv_data;
	u16 smid;
	struct _sas_device *sas_device;
	unsigned long flags;
	struct _tr_list *delayed_tr;

	if (ioc->shost_recovery) {
		printk(MPT2SAS_INFO_FMT "%s: host reset in progress!\n",
		    __func__, ioc->name);
		return;
	}

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	sas_device = _scsih_sas_device_find_by_handle(ioc, handle);
	if (!sas_device) {
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
		printk(MPT2SAS_ERR_FMT "%s: failed finding sas_device\n",
		    ioc->name, __func__);
		return;
	}
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);

	/* skip is hidden raid component */
	if (sas_device->hidden_raid_component)
		return;

	smid = mpt2sas_base_get_smid_hpr(ioc, ioc->tm_tr_cb_idx);
	if (!smid) {
		delayed_tr = kzalloc(sizeof(*delayed_tr), GFP_ATOMIC);
		if (!delayed_tr)
			return;
		INIT_LIST_HEAD(&delayed_tr->list);
		delayed_tr->handle = handle;
		delayed_tr->state = MPT2SAS_REQ_SAS_CNTRL;
		list_add_tail(&delayed_tr->list,
		    &ioc->delayed_tr_list);
		if (sas_device->starget)
			dewtprintk(ioc, starget_printk(KERN_INFO,
			    sas_device->starget, "DELAYED:tr:handle(0x%04x), "
			    "(open)\n", sas_device->handle));
		return;
	}

	if (sas_device->starget && sas_device->starget->hostdata) {
		sas_target_priv_data = sas_device->starget->hostdata;
		sas_target_priv_data->tm_busy = 1;
		dewtprintk(ioc, starget_printk(KERN_INFO, sas_device->starget,
		    "tr:handle(0x%04x), (open)\n", sas_device->handle));
	}

	mpi_request = mpt2sas_base_get_msg_frame(ioc, smid);
	memset(mpi_request, 0, sizeof(Mpi2SCSITaskManagementRequest_t));
	mpi_request->Function = MPI2_FUNCTION_SCSI_TASK_MGMT;
	mpi_request->DevHandle = cpu_to_le16(handle);
	mpi_request->TaskType = MPI2_SCSITASKMGMT_TASKTYPE_TARGET_RESET;
	sas_device->state |= MPTSAS_STATE_TR_SEND;
	sas_device->state |= MPT2SAS_REQ_SAS_CNTRL;
	mpt2sas_base_put_smid_hi_priority(ioc, smid);
}



/**
 * _scsih_sas_control_complete - completion routine
 * @ioc: per adapter object
 * @smid: system request message index
 * @msix_index: MSIX table index supplied by the OS
 * @reply: reply message frame(lower 32bit addr)
 * Context: interrupt time.
 *
 * This is the sas iounit controll completion routine.
 * This code is part of the code to initiate the device removal
 * handshake protocal with controller firmware.
 *
 * Return 1 meaning mf should be freed from _base_interrupt
 *        0 means the mf is freed from this function.
 */
static u8
_scsih_sas_control_complete(struct MPT2SAS_ADAPTER *ioc, u16 smid,
    u8 msix_index, u32 reply)
{
	unsigned long flags;
	u16 handle;
	struct _sas_device *sas_device;
	Mpi2SasIoUnitControlReply_t *mpi_reply =
	    mpt2sas_base_get_reply_virt_addr(ioc, reply);

	handle = le16_to_cpu(mpi_reply->DevHandle);

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	sas_device = _scsih_sas_device_find_by_handle(ioc, handle);
	if (!sas_device) {
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
		printk(MPT2SAS_ERR_FMT "%s: failed finding sas_device\n",
		    ioc->name, __func__);
		return 1;
	}
	sas_device->state |= MPTSAS_STATE_CNTRL_COMPLETE;
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);

	if (sas_device->starget)
		dewtprintk(ioc, starget_printk(KERN_INFO, sas_device->starget,
		    "sc_complete:handle(0x%04x), "
		    "ioc_status(0x%04x), loginfo(0x%08x)\n",
		    handle, le16_to_cpu(mpi_reply->IOCStatus),
		    le32_to_cpu(mpi_reply->IOCLogInfo)));
	return 1;
}

/**
 * _scsih_tm_tr_complete -
 * @ioc: per adapter object
 * @smid: system request message index
 * @msix_index: MSIX table index supplied by the OS
 * @reply: reply message frame(lower 32bit addr)
 * Context: interrupt time.
 *
 * This is the target reset completion routine.
 * This code is part of the code to initiate the device removal
 * handshake protocal with controller firmware.
 * It will send a sas iounit controll request (MPI2_SAS_OP_REMOVE_DEVICE)
 *
 * Return 1 meaning mf should be freed from _base_interrupt
 *        0 means the mf is freed from this function.
 */
static u8
_scsih_tm_tr_complete(struct MPT2SAS_ADAPTER *ioc, u16 smid, u8 msix_index,
    u32 reply)
{
	unsigned long flags;
	u16 handle;
	struct _sas_device *sas_device;
	Mpi2SCSITaskManagementReply_t *mpi_reply =
	    mpt2sas_base_get_reply_virt_addr(ioc, reply);
	Mpi2SasIoUnitControlRequest_t *mpi_request;
	u16 smid_sas_ctrl;
	struct MPT2SAS_TARGET *sas_target_priv_data;
	struct _tr_list *delayed_tr;
	u8 rc;

	handle = le16_to_cpu(mpi_reply->DevHandle);
	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	sas_device = _scsih_sas_device_find_by_handle(ioc, handle);
	if (!sas_device) {
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
		printk(MPT2SAS_ERR_FMT "%s: failed finding sas_device\n",
		    ioc->name, __func__);
		return 1;
	}
	sas_device->state |= MPTSAS_STATE_TR_COMPLETE;
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);

	if (sas_device->starget)
		dewtprintk(ioc, starget_printk(KERN_INFO, sas_device->starget,
		    "tr_complete:handle(0x%04x), (%s) ioc_status(0x%04x), "
		    "loginfo(0x%08x), completed(%d)\n",
		    sas_device->handle, (sas_device->state &
		    MPT2SAS_REQ_SAS_CNTRL) ? "open" : "active",
		    le16_to_cpu(mpi_reply->IOCStatus),
		    le32_to_cpu(mpi_reply->IOCLogInfo),
		    le32_to_cpu(mpi_reply->TerminationCount)));

	if (sas_device->starget && sas_device->starget->hostdata) {
		sas_target_priv_data = sas_device->starget->hostdata;
		sas_target_priv_data->tm_busy = 0;
	}

	if (!list_empty(&ioc->delayed_tr_list)) {
		delayed_tr = list_entry(ioc->delayed_tr_list.next,
		    struct _tr_list, list);
		mpt2sas_base_free_smid(ioc, smid);
		if (delayed_tr->state & MPT2SAS_REQ_SAS_CNTRL)
			_scsih_tm_tr_send(ioc, delayed_tr->handle);
		list_del(&delayed_tr->list);
		kfree(delayed_tr);
		rc = 0; /* tells base_interrupt not to free mf */
	} else
		rc = 1;


	if (!(sas_device->state & MPT2SAS_REQ_SAS_CNTRL))
		return rc;

	if (ioc->shost_recovery) {
		printk(MPT2SAS_INFO_FMT "%s: host reset in progress!\n",
		    __func__, ioc->name);
		return rc;
	}

	smid_sas_ctrl = mpt2sas_base_get_smid(ioc, ioc->tm_sas_control_cb_idx);
	if (!smid_sas_ctrl) {
		printk(MPT2SAS_ERR_FMT "%s: failed obtaining a smid\n",
		    ioc->name, __func__);
		return rc;
	}

	mpi_request = mpt2sas_base_get_msg_frame(ioc, smid_sas_ctrl);
	memset(mpi_request, 0, sizeof(Mpi2SasIoUnitControlRequest_t));
	mpi_request->Function = MPI2_FUNCTION_SAS_IO_UNIT_CONTROL;
	mpi_request->Operation = MPI2_SAS_OP_REMOVE_DEVICE;
	mpi_request->DevHandle = mpi_reply->DevHandle;
	sas_device->state |= MPTSAS_STATE_CNTRL_SEND;
	mpt2sas_base_put_smid_default(ioc, smid_sas_ctrl);
	return rc;
}

/**
 * _scsih_check_topo_delete_events - sanity check on topo events
 * @ioc: per adapter object
 * @event_data: the event data payload
 *
 * This routine added to better handle cable breaker.
 *
 * This handles the case where driver recieves multiple expander
 * add and delete events in a single shot.  When there is a delete event
 * the routine will void any pending add events waiting in the event queue.
 *
 * Return nothing.
 */
static void
_scsih_check_topo_delete_events(struct MPT2SAS_ADAPTER *ioc,
    Mpi2EventDataSasTopologyChangeList_t *event_data)
{
	struct fw_event_work *fw_event;
	Mpi2EventDataSasTopologyChangeList_t *local_event_data;
	u16 expander_handle;
	struct _sas_node *sas_expander;
	unsigned long flags;
	int i, reason_code;
	u16 handle;

	for (i = 0 ; i < event_data->NumEntries; i++) {
		if (event_data->PHY[i].PhyStatus &
		    MPI2_EVENT_SAS_TOPO_PHYSTATUS_VACANT)
			continue;
		handle = le16_to_cpu(event_data->PHY[i].AttachedDevHandle);
		if (!handle)
			continue;
		reason_code = event_data->PHY[i].PhyStatus &
		    MPI2_EVENT_SAS_TOPO_RC_MASK;
		if (reason_code == MPI2_EVENT_SAS_TOPO_RC_TARG_NOT_RESPONDING)
			_scsih_tm_tr_send(ioc, handle);
	}

	expander_handle = le16_to_cpu(event_data->ExpanderDevHandle);
	if (expander_handle < ioc->sas_hba.num_phys) {
		_scsih_block_io_to_children_attached_directly(ioc, event_data);
		return;
	}

	if (event_data->ExpStatus == MPI2_EVENT_SAS_TOPO_ES_DELAY_NOT_RESPONDING
	 || event_data->ExpStatus == MPI2_EVENT_SAS_TOPO_ES_NOT_RESPONDING) {
		spin_lock_irqsave(&ioc->sas_node_lock, flags);
		sas_expander = mpt2sas_scsih_expander_find_by_handle(ioc,
		    expander_handle);
		spin_unlock_irqrestore(&ioc->sas_node_lock, flags);
		_scsih_block_io_to_children_attached_to_ex(ioc, sas_expander);
	} else if (event_data->ExpStatus == MPI2_EVENT_SAS_TOPO_ES_RESPONDING)
		_scsih_block_io_to_children_attached_directly(ioc, event_data);

	if (event_data->ExpStatus != MPI2_EVENT_SAS_TOPO_ES_NOT_RESPONDING)
		return;

	/* mark ignore flag for pending events */
	spin_lock_irqsave(&ioc->fw_event_lock, flags);
	list_for_each_entry(fw_event, &ioc->fw_event_list, list) {
		if (fw_event->event != MPI2_EVENT_SAS_TOPOLOGY_CHANGE_LIST ||
		    fw_event->ignore)
			continue;
		local_event_data = fw_event->event_data;
		if (local_event_data->ExpStatus ==
		    MPI2_EVENT_SAS_TOPO_ES_ADDED ||
		    local_event_data->ExpStatus ==
		    MPI2_EVENT_SAS_TOPO_ES_RESPONDING) {
			if (le16_to_cpu(local_event_data->ExpanderDevHandle) ==
			    expander_handle) {
				dewtprintk(ioc, printk(MPT2SAS_DEBUG_FMT
				    "setting ignoring flag\n", ioc->name));
				fw_event->ignore = 1;
			}
		}
	}
	spin_unlock_irqrestore(&ioc->fw_event_lock, flags);
}

/**
 * _scsih_flush_running_cmds - completing outstanding commands.
 * @ioc: per adapter object
 *
 * The flushing out of all pending scmd commands following host reset,
 * where all IO is dropped to the floor.
 *
 * Return nothing.
 */
static void
_scsih_flush_running_cmds(struct MPT2SAS_ADAPTER *ioc)
{
	struct scsi_cmnd *scmd;
	u16 smid;
	u16 count = 0;

	for (smid = 1; smid <= ioc->scsiio_depth; smid++) {
		scmd = _scsih_scsi_lookup_get(ioc, smid);
		if (!scmd)
			continue;
		count++;
		mpt2sas_base_free_smid(ioc, smid);
		scsi_dma_unmap(scmd);
		scmd->result = DID_RESET << 16;
		scmd->scsi_done(scmd);
	}
	dtmprintk(ioc, printk(MPT2SAS_INFO_FMT "completing %d cmds\n",
	    ioc->name, count));
}

/**
 * _scsih_setup_eedp - setup MPI request for EEDP transfer
 * @scmd: pointer to scsi command object
 * @mpi_request: pointer to the SCSI_IO reqest message frame
 *
 * Supporting protection 1 and 3.
 *
 * Returns nothing
 */
static void
_scsih_setup_eedp(struct scsi_cmnd *scmd, Mpi2SCSIIORequest_t *mpi_request)
{
	u16 eedp_flags;
	unsigned char prot_op = scsi_get_prot_op(scmd);
	unsigned char prot_type = scsi_get_prot_type(scmd);

	if (prot_type == SCSI_PROT_DIF_TYPE0 ||
	   prot_type == SCSI_PROT_DIF_TYPE2 ||
	   prot_op == SCSI_PROT_NORMAL)
		return;

	if (prot_op ==  SCSI_PROT_READ_STRIP)
		eedp_flags = MPI2_SCSIIO_EEDPFLAGS_CHECK_REMOVE_OP;
	else if (prot_op ==  SCSI_PROT_WRITE_INSERT)
		eedp_flags = MPI2_SCSIIO_EEDPFLAGS_INSERT_OP;
	else
		return;

	mpi_request->EEDPBlockSize = scmd->device->sector_size;

	switch (prot_type) {
	case SCSI_PROT_DIF_TYPE1:

		/*
		* enable ref/guard checking
		* auto increment ref tag
		*/
		mpi_request->EEDPFlags = eedp_flags |
		    MPI2_SCSIIO_EEDPFLAGS_INC_PRI_REFTAG |
		    MPI2_SCSIIO_EEDPFLAGS_CHECK_REFTAG |
		    MPI2_SCSIIO_EEDPFLAGS_CHECK_GUARD;
		mpi_request->CDB.EEDP32.PrimaryReferenceTag =
		    cpu_to_be32(scsi_get_lba(scmd));

		break;

	case SCSI_PROT_DIF_TYPE3:

		/*
		* enable guard checking
		*/
		mpi_request->EEDPFlags = eedp_flags |
		    MPI2_SCSIIO_EEDPFLAGS_CHECK_GUARD;

		break;
	}
}

/**
 * _scsih_eedp_error_handling - return sense code for EEDP errors
 * @scmd: pointer to scsi command object
 * @ioc_status: ioc status
 *
 * Returns nothing
 */
static void
_scsih_eedp_error_handling(struct scsi_cmnd *scmd, u16 ioc_status)
{
	u8 ascq;
	u8 sk;
	u8 host_byte;

	switch (ioc_status) {
	case MPI2_IOCSTATUS_EEDP_GUARD_ERROR:
		ascq = 0x01;
		break;
	case MPI2_IOCSTATUS_EEDP_APP_TAG_ERROR:
		ascq = 0x02;
		break;
	case MPI2_IOCSTATUS_EEDP_REF_TAG_ERROR:
		ascq = 0x03;
		break;
	default:
		ascq = 0x00;
		break;
	}

	if (scmd->sc_data_direction == DMA_TO_DEVICE) {
		sk = ILLEGAL_REQUEST;
		host_byte = DID_ABORT;
	} else {
		sk = ABORTED_COMMAND;
		host_byte = DID_OK;
	}

	scsi_build_sense_buffer(0, scmd->sense_buffer, sk, 0x10, ascq);
	scmd->result = DRIVER_SENSE << 24 | (host_byte << 16) |
	    SAM_STAT_CHECK_CONDITION;
}

/**
 * _scsih_qcmd - main scsi request entry point
 * @scmd: pointer to scsi command object
 * @done: function pointer to be invoked on completion
 *
 * The callback index is set inside `ioc->scsi_io_cb_idx`.
 *
 * Returns 0 on success.  If there's a failure, return either:
 * SCSI_MLQUEUE_DEVICE_BUSY if the device queue is full, or
 * SCSI_MLQUEUE_HOST_BUSY if the entire host queue is full
 */
static int
_scsih_qcmd(struct scsi_cmnd *scmd, void (*done)(struct scsi_cmnd *))
{
	struct MPT2SAS_ADAPTER *ioc = shost_priv(scmd->device->host);
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct MPT2SAS_TARGET *sas_target_priv_data;
	Mpi2SCSIIORequest_t *mpi_request;
	u32 mpi_control;
	u16 smid;

	scmd->scsi_done = done;
	sas_device_priv_data = scmd->device->hostdata;
	if (!sas_device_priv_data) {
		scmd->result = DID_NO_CONNECT << 16;
		scmd->scsi_done(scmd);
		return 0;
	}

	sas_target_priv_data = sas_device_priv_data->sas_target;
	if (!sas_target_priv_data || sas_target_priv_data->handle ==
	    MPT2SAS_INVALID_DEVICE_HANDLE || sas_target_priv_data->deleted) {
		scmd->result = DID_NO_CONNECT << 16;
		scmd->scsi_done(scmd);
		return 0;
	}

	/* see if we are busy with task managment stuff */
	if (sas_target_priv_data->tm_busy)
		return SCSI_MLQUEUE_DEVICE_BUSY;
	else if (ioc->shost_recovery || ioc->ioc_link_reset_in_progress)
		return SCSI_MLQUEUE_HOST_BUSY;

	if (scmd->sc_data_direction == DMA_FROM_DEVICE)
		mpi_control = MPI2_SCSIIO_CONTROL_READ;
	else if (scmd->sc_data_direction == DMA_TO_DEVICE)
		mpi_control = MPI2_SCSIIO_CONTROL_WRITE;
	else
		mpi_control = MPI2_SCSIIO_CONTROL_NODATATRANSFER;

	/* set tags */
	if (!(sas_device_priv_data->flags & MPT_DEVICE_FLAGS_INIT)) {
		if (scmd->device->tagged_supported) {
			if (scmd->device->ordered_tags)
				mpi_control |= MPI2_SCSIIO_CONTROL_ORDEREDQ;
			else
				mpi_control |= MPI2_SCSIIO_CONTROL_SIMPLEQ;
		} else
/* MPI Revision I (UNIT = 0xA) - removed MPI2_SCSIIO_CONTROL_UNTAGGED */
/*			mpi_control |= MPI2_SCSIIO_CONTROL_UNTAGGED;
 */
			mpi_control |= (0x500);

	} else
		mpi_control |= MPI2_SCSIIO_CONTROL_SIMPLEQ;

	if ((sas_device_priv_data->flags & MPT_DEVICE_TLR_ON))
		mpi_control |= MPI2_SCSIIO_CONTROL_TLR_ON;

	smid = mpt2sas_base_get_smid_scsiio(ioc, ioc->scsi_io_cb_idx, scmd);
	if (!smid) {
		printk(MPT2SAS_ERR_FMT "%s: failed obtaining a smid\n",
		    ioc->name, __func__);
		goto out;
	}
	mpi_request = mpt2sas_base_get_msg_frame(ioc, smid);
	memset(mpi_request, 0, sizeof(Mpi2SCSIIORequest_t));
	_scsih_setup_eedp(scmd, mpi_request);
	mpi_request->Function = MPI2_FUNCTION_SCSI_IO_REQUEST;
	if (sas_device_priv_data->sas_target->flags &
	    MPT_TARGET_FLAGS_RAID_COMPONENT)
		mpi_request->Function = MPI2_FUNCTION_RAID_SCSI_IO_PASSTHROUGH;
	else
		mpi_request->Function = MPI2_FUNCTION_SCSI_IO_REQUEST;
	mpi_request->DevHandle =
	    cpu_to_le16(sas_device_priv_data->sas_target->handle);
	mpi_request->DataLength = cpu_to_le32(scsi_bufflen(scmd));
	mpi_request->Control = cpu_to_le32(mpi_control);
	mpi_request->IoFlags = cpu_to_le16(scmd->cmd_len);
	mpi_request->MsgFlags = MPI2_SCSIIO_MSGFLAGS_SYSTEM_SENSE_ADDR;
	mpi_request->SenseBufferLength = SCSI_SENSE_BUFFERSIZE;
	mpi_request->SenseBufferLowAddress =
	    (u32)mpt2sas_base_get_sense_buffer_dma(ioc, smid);
	mpi_request->SGLOffset0 = offsetof(Mpi2SCSIIORequest_t, SGL) / 4;
	mpi_request->SGLFlags = cpu_to_le16(MPI2_SCSIIO_SGLFLAGS_TYPE_MPI +
	    MPI2_SCSIIO_SGLFLAGS_SYSTEM_ADDR);
	mpi_request->VF_ID = 0; /* TODO */
	mpi_request->VP_ID = 0;
	int_to_scsilun(sas_device_priv_data->lun, (struct scsi_lun *)
	    mpi_request->LUN);
	memcpy(mpi_request->CDB.CDB32, scmd->cmnd, scmd->cmd_len);

	if (!mpi_request->DataLength) {
		mpt2sas_base_build_zero_len_sge(ioc, &mpi_request->SGL);
	} else {
		if (_scsih_build_scatter_gather(ioc, scmd, smid)) {
			mpt2sas_base_free_smid(ioc, smid);
			goto out;
		}
	}

	mpt2sas_base_put_smid_scsi_io(ioc, smid,
	    sas_device_priv_data->sas_target->handle);
	return 0;

 out:
	return SCSI_MLQUEUE_HOST_BUSY;
}

/**
 * _scsih_normalize_sense - normalize descriptor and fixed format sense data
 * @sense_buffer: sense data returned by target
 * @data: normalized skey/asc/ascq
 *
 * Return nothing.
 */
static void
_scsih_normalize_sense(char *sense_buffer, struct sense_info *data)
{
	if ((sense_buffer[0] & 0x7F) >= 0x72) {
		/* descriptor format */
		data->skey = sense_buffer[1] & 0x0F;
		data->asc = sense_buffer[2];
		data->ascq = sense_buffer[3];
	} else {
		/* fixed format */
		data->skey = sense_buffer[2] & 0x0F;
		data->asc = sense_buffer[12];
		data->ascq = sense_buffer[13];
	}
}

#ifdef CONFIG_SCSI_MPT2SAS_LOGGING
/**
 * _scsih_scsi_ioc_info - translated non-succesfull SCSI_IO request
 * @ioc: per adapter object
 * @scmd: pointer to scsi command object
 * @mpi_reply: reply mf payload returned from firmware
 *
 * scsi_status - SCSI Status code returned from target device
 * scsi_state - state info associated with SCSI_IO determined by ioc
 * ioc_status - ioc supplied status info
 *
 * Return nothing.
 */
static void
_scsih_scsi_ioc_info(struct MPT2SAS_ADAPTER *ioc, struct scsi_cmnd *scmd,
    Mpi2SCSIIOReply_t *mpi_reply, u16 smid)
{
	u32 response_info;
	u8 *response_bytes;
	u16 ioc_status = le16_to_cpu(mpi_reply->IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	u8 scsi_state = mpi_reply->SCSIState;
	u8 scsi_status = mpi_reply->SCSIStatus;
	char *desc_ioc_state = NULL;
	char *desc_scsi_status = NULL;
	char *desc_scsi_state = ioc->tmp_string;
	u32 log_info = le32_to_cpu(mpi_reply->IOCLogInfo);

	if (log_info == 0x31170000)
		return;

	switch (ioc_status) {
	case MPI2_IOCSTATUS_SUCCESS:
		desc_ioc_state = "success";
		break;
	case MPI2_IOCSTATUS_INVALID_FUNCTION:
		desc_ioc_state = "invalid function";
		break;
	case MPI2_IOCSTATUS_SCSI_RECOVERED_ERROR:
		desc_ioc_state = "scsi recovered error";
		break;
	case MPI2_IOCSTATUS_SCSI_INVALID_DEVHANDLE:
		desc_ioc_state = "scsi invalid dev handle";
		break;
	case MPI2_IOCSTATUS_SCSI_DEVICE_NOT_THERE:
		desc_ioc_state = "scsi device not there";
		break;
	case MPI2_IOCSTATUS_SCSI_DATA_OVERRUN:
		desc_ioc_state = "scsi data overrun";
		break;
	case MPI2_IOCSTATUS_SCSI_DATA_UNDERRUN:
		desc_ioc_state = "scsi data underrun";
		break;
	case MPI2_IOCSTATUS_SCSI_IO_DATA_ERROR:
		desc_ioc_state = "scsi io data error";
		break;
	case MPI2_IOCSTATUS_SCSI_PROTOCOL_ERROR:
		desc_ioc_state = "scsi protocol error";
		break;
	case MPI2_IOCSTATUS_SCSI_TASK_TERMINATED:
		desc_ioc_state = "scsi task terminated";
		break;
	case MPI2_IOCSTATUS_SCSI_RESIDUAL_MISMATCH:
		desc_ioc_state = "scsi residual mismatch";
		break;
	case MPI2_IOCSTATUS_SCSI_TASK_MGMT_FAILED:
		desc_ioc_state = "scsi task mgmt failed";
		break;
	case MPI2_IOCSTATUS_SCSI_IOC_TERMINATED:
		desc_ioc_state = "scsi ioc terminated";
		break;
	case MPI2_IOCSTATUS_SCSI_EXT_TERMINATED:
		desc_ioc_state = "scsi ext terminated";
		break;
	case MPI2_IOCSTATUS_EEDP_GUARD_ERROR:
		desc_ioc_state = "eedp guard error";
		break;
	case MPI2_IOCSTATUS_EEDP_REF_TAG_ERROR:
		desc_ioc_state = "eedp ref tag error";
		break;
	case MPI2_IOCSTATUS_EEDP_APP_TAG_ERROR:
		desc_ioc_state = "eedp app tag error";
		break;
	default:
		desc_ioc_state = "unknown";
		break;
	}

	switch (scsi_status) {
	case MPI2_SCSI_STATUS_GOOD:
		desc_scsi_status = "good";
		break;
	case MPI2_SCSI_STATUS_CHECK_CONDITION:
		desc_scsi_status = "check condition";
		break;
	case MPI2_SCSI_STATUS_CONDITION_MET:
		desc_scsi_status = "condition met";
		break;
	case MPI2_SCSI_STATUS_BUSY:
		desc_scsi_status = "busy";
		break;
	case MPI2_SCSI_STATUS_INTERMEDIATE:
		desc_scsi_status = "intermediate";
		break;
	case MPI2_SCSI_STATUS_INTERMEDIATE_CONDMET:
		desc_scsi_status = "intermediate condmet";
		break;
	case MPI2_SCSI_STATUS_RESERVATION_CONFLICT:
		desc_scsi_status = "reservation conflict";
		break;
	case MPI2_SCSI_STATUS_COMMAND_TERMINATED:
		desc_scsi_status = "command terminated";
		break;
	case MPI2_SCSI_STATUS_TASK_SET_FULL:
		desc_scsi_status = "task set full";
		break;
	case MPI2_SCSI_STATUS_ACA_ACTIVE:
		desc_scsi_status = "aca active";
		break;
	case MPI2_SCSI_STATUS_TASK_ABORTED:
		desc_scsi_status = "task aborted";
		break;
	default:
		desc_scsi_status = "unknown";
		break;
	}

	desc_scsi_state[0] = '\0';
	if (!scsi_state)
		desc_scsi_state = " ";
	if (scsi_state & MPI2_SCSI_STATE_RESPONSE_INFO_VALID)
		strcat(desc_scsi_state, "response info ");
	if (scsi_state & MPI2_SCSI_STATE_TERMINATED)
		strcat(desc_scsi_state, "state terminated ");
	if (scsi_state & MPI2_SCSI_STATE_NO_SCSI_STATUS)
		strcat(desc_scsi_state, "no status ");
	if (scsi_state & MPI2_SCSI_STATE_AUTOSENSE_FAILED)
		strcat(desc_scsi_state, "autosense failed ");
	if (scsi_state & MPI2_SCSI_STATE_AUTOSENSE_VALID)
		strcat(desc_scsi_state, "autosense valid ");

	scsi_print_command(scmd);
	printk(MPT2SAS_WARN_FMT "\tdev handle(0x%04x), "
	    "ioc_status(%s)(0x%04x), smid(%d)\n", ioc->name,
	    le16_to_cpu(mpi_reply->DevHandle), desc_ioc_state,
		ioc_status, smid);
	printk(MPT2SAS_WARN_FMT "\trequest_len(%d), underflow(%d), "
	    "resid(%d)\n", ioc->name, scsi_bufflen(scmd), scmd->underflow,
	    scsi_get_resid(scmd));
	printk(MPT2SAS_WARN_FMT "\ttag(%d), transfer_count(%d), "
	    "sc->result(0x%08x)\n", ioc->name, le16_to_cpu(mpi_reply->TaskTag),
	    le32_to_cpu(mpi_reply->TransferCount), scmd->result);
	printk(MPT2SAS_WARN_FMT "\tscsi_status(%s)(0x%02x), "
	    "scsi_state(%s)(0x%02x)\n", ioc->name, desc_scsi_status,
	    scsi_status, desc_scsi_state, scsi_state);

	if (scsi_state & MPI2_SCSI_STATE_AUTOSENSE_VALID) {
		struct sense_info data;
		_scsih_normalize_sense(scmd->sense_buffer, &data);
		printk(MPT2SAS_WARN_FMT "\t[sense_key,asc,ascq]: "
		    "[0x%02x,0x%02x,0x%02x]\n", ioc->name, data.skey,
		    data.asc, data.ascq);
	}

	if (scsi_state & MPI2_SCSI_STATE_RESPONSE_INFO_VALID) {
		response_info = le32_to_cpu(mpi_reply->ResponseInfo);
		response_bytes = (u8 *)&response_info;
		_scsih_response_code(ioc, response_bytes[3]);
	}
}
#endif

/**
 * _scsih_smart_predicted_fault - illuminate Fault LED
 * @ioc: per adapter object
 * @handle: device handle
 *
 * Return nothing.
 */
static void
_scsih_smart_predicted_fault(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	Mpi2SepReply_t mpi_reply;
	Mpi2SepRequest_t mpi_request;
	struct scsi_target *starget;
	struct MPT2SAS_TARGET *sas_target_priv_data;
	Mpi2EventNotificationReply_t *event_reply;
	Mpi2EventDataSasDeviceStatusChange_t *event_data;
	struct _sas_device *sas_device;
	ssize_t sz;
	unsigned long flags;

	/* only handle non-raid devices */
	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	sas_device = _scsih_sas_device_find_by_handle(ioc, handle);
	if (!sas_device) {
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
		return;
	}
	starget = sas_device->starget;
	sas_target_priv_data = starget->hostdata;

	if ((sas_target_priv_data->flags & MPT_TARGET_FLAGS_RAID_COMPONENT) ||
	   ((sas_target_priv_data->flags & MPT_TARGET_FLAGS_VOLUME))) {
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
		return;
	}
	starget_printk(KERN_WARNING, starget, "predicted fault\n");
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);

	if (ioc->pdev->subsystem_vendor == PCI_VENDOR_ID_IBM) {
		memset(&mpi_request, 0, sizeof(Mpi2SepRequest_t));
		mpi_request.Function = MPI2_FUNCTION_SCSI_ENCLOSURE_PROCESSOR;
		mpi_request.Action = MPI2_SEP_REQ_ACTION_WRITE_STATUS;
		mpi_request.SlotStatus =
		    MPI2_SEP_REQ_SLOTSTATUS_PREDICTED_FAULT;
		mpi_request.DevHandle = cpu_to_le16(handle);
		mpi_request.Flags = MPI2_SEP_REQ_FLAGS_DEVHANDLE_ADDRESS;
		if ((mpt2sas_base_scsi_enclosure_processor(ioc, &mpi_reply,
		    &mpi_request)) != 0) {
			printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
			    ioc->name, __FILE__, __LINE__, __func__);
			return;
		}

		if (mpi_reply.IOCStatus || mpi_reply.IOCLogInfo) {
			dewtprintk(ioc, printk(MPT2SAS_INFO_FMT
			    "enclosure_processor: ioc_status (0x%04x), "
			    "loginfo(0x%08x)\n", ioc->name,
			    le16_to_cpu(mpi_reply.IOCStatus),
			    le32_to_cpu(mpi_reply.IOCLogInfo)));
			return;
		}
	}

	/* insert into event log */
	sz = offsetof(Mpi2EventNotificationReply_t, EventData) +
	     sizeof(Mpi2EventDataSasDeviceStatusChange_t);
	event_reply = kzalloc(sz, GFP_KERNEL);
	if (!event_reply) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return;
	}

	event_reply->Function = MPI2_FUNCTION_EVENT_NOTIFICATION;
	event_reply->Event =
	    cpu_to_le16(MPI2_EVENT_SAS_DEVICE_STATUS_CHANGE);
	event_reply->MsgLength = sz/4;
	event_reply->EventDataLength =
	    cpu_to_le16(sizeof(Mpi2EventDataSasDeviceStatusChange_t)/4);
	event_data = (Mpi2EventDataSasDeviceStatusChange_t *)
	    event_reply->EventData;
	event_data->ReasonCode = MPI2_EVENT_SAS_DEV_STAT_RC_SMART_DATA;
	event_data->ASC = 0x5D;
	event_data->DevHandle = cpu_to_le16(handle);
	event_data->SASAddress = cpu_to_le64(sas_target_priv_data->sas_address);
	mpt2sas_ctl_add_to_event_log(ioc, event_reply);
	kfree(event_reply);
}

/**
 * _scsih_io_done - scsi request callback
 * @ioc: per adapter object
 * @smid: system request message index
 * @msix_index: MSIX table index supplied by the OS
 * @reply: reply message frame(lower 32bit addr)
 *
 * Callback handler when using _scsih_qcmd.
 *
 * Return 1 meaning mf should be freed from _base_interrupt
 *        0 means the mf is freed from this function.
 */
static u8
_scsih_io_done(struct MPT2SAS_ADAPTER *ioc, u16 smid, u8 msix_index, u32 reply)
{
	Mpi2SCSIIORequest_t *mpi_request;
	Mpi2SCSIIOReply_t *mpi_reply;
	struct scsi_cmnd *scmd;
	u16 ioc_status;
	u32 xfer_cnt;
	u8 scsi_state;
	u8 scsi_status;
	u32 log_info;
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	u32 response_code;

	mpi_reply = mpt2sas_base_get_reply_virt_addr(ioc, reply);
	scmd = _scsih_scsi_lookup_get(ioc, smid);
	if (scmd == NULL)
		return 1;

	mpi_request = mpt2sas_base_get_msg_frame(ioc, smid);

	if (mpi_reply == NULL) {
		scmd->result = DID_OK << 16;
		goto out;
	}

	sas_device_priv_data = scmd->device->hostdata;
	if (!sas_device_priv_data || !sas_device_priv_data->sas_target ||
	     sas_device_priv_data->sas_target->deleted) {
		scmd->result = DID_NO_CONNECT << 16;
		goto out;
	}

	/* turning off TLR */
	if (!sas_device_priv_data->tlr_snoop_check) {
		sas_device_priv_data->tlr_snoop_check++;
		if (sas_device_priv_data->flags & MPT_DEVICE_TLR_ON) {
			response_code = (le32_to_cpu(mpi_reply->ResponseInfo)
			    >> 24);
			if (response_code ==
			    MPI2_SCSITASKMGMT_RSP_INVALID_FRAME)
				sas_device_priv_data->flags &=
				    ~MPT_DEVICE_TLR_ON;
		}
	}

	xfer_cnt = le32_to_cpu(mpi_reply->TransferCount);
	scsi_set_resid(scmd, scsi_bufflen(scmd) - xfer_cnt);
	ioc_status = le16_to_cpu(mpi_reply->IOCStatus);
	if (ioc_status & MPI2_IOCSTATUS_FLAG_LOG_INFO_AVAILABLE)
		log_info =  le32_to_cpu(mpi_reply->IOCLogInfo);
	else
		log_info = 0;
	ioc_status &= MPI2_IOCSTATUS_MASK;
	scsi_state = mpi_reply->SCSIState;
	scsi_status = mpi_reply->SCSIStatus;

	if (ioc_status == MPI2_IOCSTATUS_SCSI_DATA_UNDERRUN && xfer_cnt == 0 &&
	    (scsi_status == MPI2_SCSI_STATUS_BUSY ||
	     scsi_status == MPI2_SCSI_STATUS_RESERVATION_CONFLICT ||
	     scsi_status == MPI2_SCSI_STATUS_TASK_SET_FULL)) {
		ioc_status = MPI2_IOCSTATUS_SUCCESS;
	}

	if (scsi_state & MPI2_SCSI_STATE_AUTOSENSE_VALID) {
		struct sense_info data;
		const void *sense_data = mpt2sas_base_get_sense_buffer(ioc,
		    smid);
		u32 sz = min_t(u32, SCSI_SENSE_BUFFERSIZE,
		    le32_to_cpu(mpi_reply->SenseCount));
		memcpy(scmd->sense_buffer, sense_data, sz);
		_scsih_normalize_sense(scmd->sense_buffer, &data);
		/* failure prediction threshold exceeded */
		if (data.asc == 0x5D)
			_scsih_smart_predicted_fault(ioc,
			    le16_to_cpu(mpi_reply->DevHandle));
	}

	switch (ioc_status) {
	case MPI2_IOCSTATUS_BUSY:
	case MPI2_IOCSTATUS_INSUFFICIENT_RESOURCES:
		scmd->result = SAM_STAT_BUSY;
		break;

	case MPI2_IOCSTATUS_SCSI_DEVICE_NOT_THERE:
		scmd->result = DID_NO_CONNECT << 16;
		break;

	case MPI2_IOCSTATUS_SCSI_IOC_TERMINATED:
		if (sas_device_priv_data->block) {
			scmd->result = (DID_BUS_BUSY << 16);
			break;
		}

	case MPI2_IOCSTATUS_SCSI_TASK_TERMINATED:
	case MPI2_IOCSTATUS_SCSI_EXT_TERMINATED:
		scmd->result = DID_RESET << 16;
		break;

	case MPI2_IOCSTATUS_SCSI_RESIDUAL_MISMATCH:
		if ((xfer_cnt == 0) || (scmd->underflow > xfer_cnt))
			scmd->result = DID_SOFT_ERROR << 16;
		else
			scmd->result = (DID_OK << 16) | scsi_status;
		break;

	case MPI2_IOCSTATUS_SCSI_DATA_UNDERRUN:
		scmd->result = (DID_OK << 16) | scsi_status;

		if ((scsi_state & MPI2_SCSI_STATE_AUTOSENSE_VALID))
			break;

		if (xfer_cnt < scmd->underflow) {
			if (scsi_status == SAM_STAT_BUSY)
				scmd->result = SAM_STAT_BUSY;
			else
				scmd->result = DID_SOFT_ERROR << 16;
		} else if (scsi_state & (MPI2_SCSI_STATE_AUTOSENSE_FAILED |
		     MPI2_SCSI_STATE_NO_SCSI_STATUS))
			scmd->result = DID_SOFT_ERROR << 16;
		else if (scsi_state & MPI2_SCSI_STATE_TERMINATED)
			scmd->result = DID_RESET << 16;
		else if (!xfer_cnt && scmd->cmnd[0] == REPORT_LUNS) {
			mpi_reply->SCSIState = MPI2_SCSI_STATE_AUTOSENSE_VALID;
			mpi_reply->SCSIStatus = SAM_STAT_CHECK_CONDITION;
			scmd->result = (DRIVER_SENSE << 24) |
			    SAM_STAT_CHECK_CONDITION;
			scmd->sense_buffer[0] = 0x70;
			scmd->sense_buffer[2] = ILLEGAL_REQUEST;
			scmd->sense_buffer[12] = 0x20;
			scmd->sense_buffer[13] = 0;
		}
		break;

	case MPI2_IOCSTATUS_SCSI_DATA_OVERRUN:
		scsi_set_resid(scmd, 0);
	case MPI2_IOCSTATUS_SCSI_RECOVERED_ERROR:
	case MPI2_IOCSTATUS_SUCCESS:
		scmd->result = (DID_OK << 16) | scsi_status;
		if (scsi_state & (MPI2_SCSI_STATE_AUTOSENSE_FAILED |
		     MPI2_SCSI_STATE_NO_SCSI_STATUS))
			scmd->result = DID_SOFT_ERROR << 16;
		else if (scsi_state & MPI2_SCSI_STATE_TERMINATED)
			scmd->result = DID_RESET << 16;
		break;

	case MPI2_IOCSTATUS_EEDP_GUARD_ERROR:
	case MPI2_IOCSTATUS_EEDP_REF_TAG_ERROR:
	case MPI2_IOCSTATUS_EEDP_APP_TAG_ERROR:
		_scsih_eedp_error_handling(scmd, ioc_status);
		break;
	case MPI2_IOCSTATUS_SCSI_PROTOCOL_ERROR:
	case MPI2_IOCSTATUS_INVALID_FUNCTION:
	case MPI2_IOCSTATUS_INVALID_SGL:
	case MPI2_IOCSTATUS_INTERNAL_ERROR:
	case MPI2_IOCSTATUS_INVALID_FIELD:
	case MPI2_IOCSTATUS_INVALID_STATE:
	case MPI2_IOCSTATUS_SCSI_IO_DATA_ERROR:
	case MPI2_IOCSTATUS_SCSI_TASK_MGMT_FAILED:
	default:
		scmd->result = DID_SOFT_ERROR << 16;
		break;

	}

#ifdef CONFIG_SCSI_MPT2SAS_LOGGING
	if (scmd->result && (ioc->logging_level & MPT_DEBUG_REPLY))
		_scsih_scsi_ioc_info(ioc , scmd, mpi_reply, smid);
#endif

 out:
	scsi_dma_unmap(scmd);
	scmd->scsi_done(scmd);
	return 1;
}

/**
 * _scsih_sas_host_refresh - refreshing sas host object contents
 * @ioc: per adapter object
 * @update: update link information
 * Context: user
 *
 * During port enable, fw will send topology events for every device. Its
 * possible that the handles may change from the previous setting, so this
 * code keeping handles updating if changed.
 *
 * Return nothing.
 */
static void
_scsih_sas_host_refresh(struct MPT2SAS_ADAPTER *ioc, u8 update)
{
	u16 sz;
	u16 ioc_status;
	int i;
	Mpi2ConfigReply_t mpi_reply;
	Mpi2SasIOUnitPage0_t *sas_iounit_pg0 = NULL;

	dtmprintk(ioc, printk(MPT2SAS_INFO_FMT
	    "updating handles for sas_host(0x%016llx)\n",
	    ioc->name, (unsigned long long)ioc->sas_hba.sas_address));

	sz = offsetof(Mpi2SasIOUnitPage0_t, PhyData) + (ioc->sas_hba.num_phys
	    * sizeof(Mpi2SasIOUnit0PhyData_t));
	sas_iounit_pg0 = kzalloc(sz, GFP_KERNEL);
	if (!sas_iounit_pg0) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return;
	}
	if (!(mpt2sas_config_get_sas_iounit_pg0(ioc, &mpi_reply,
	    sas_iounit_pg0, sz))) {
		ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
		    MPI2_IOCSTATUS_MASK;
		if (ioc_status != MPI2_IOCSTATUS_SUCCESS)
			goto out;
		for (i = 0; i < ioc->sas_hba.num_phys ; i++) {
			ioc->sas_hba.phy[i].handle =
			    le16_to_cpu(sas_iounit_pg0->PhyData[i].
				ControllerDevHandle);
			if (update)
				mpt2sas_transport_update_links(
				    ioc,
				    ioc->sas_hba.phy[i].handle,
				    le16_to_cpu(sas_iounit_pg0->PhyData[i].
				    AttachedDevHandle), i,
				    sas_iounit_pg0->PhyData[i].
				    NegotiatedLinkRate >> 4);
		}
	}

 out:
	kfree(sas_iounit_pg0);
}

/**
 * _scsih_sas_host_add - create sas host object
 * @ioc: per adapter object
 *
 * Creating host side data object, stored in ioc->sas_hba
 *
 * Return nothing.
 */
static void
_scsih_sas_host_add(struct MPT2SAS_ADAPTER *ioc)
{
	int i;
	Mpi2ConfigReply_t mpi_reply;
	Mpi2SasIOUnitPage0_t *sas_iounit_pg0 = NULL;
	Mpi2SasIOUnitPage1_t *sas_iounit_pg1 = NULL;
	Mpi2SasPhyPage0_t phy_pg0;
	Mpi2SasDevicePage0_t sas_device_pg0;
	Mpi2SasEnclosurePage0_t enclosure_pg0;
	u16 ioc_status;
	u16 sz;
	u16 device_missing_delay;

	mpt2sas_config_get_number_hba_phys(ioc, &ioc->sas_hba.num_phys);
	if (!ioc->sas_hba.num_phys) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return;
	}

	/* sas_iounit page 0 */
	sz = offsetof(Mpi2SasIOUnitPage0_t, PhyData) + (ioc->sas_hba.num_phys *
	    sizeof(Mpi2SasIOUnit0PhyData_t));
	sas_iounit_pg0 = kzalloc(sz, GFP_KERNEL);
	if (!sas_iounit_pg0) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return;
	}
	if ((mpt2sas_config_get_sas_iounit_pg0(ioc, &mpi_reply,
	    sas_iounit_pg0, sz))) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		goto out;
	}
	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		goto out;
	}

	/* sas_iounit page 1 */
	sz = offsetof(Mpi2SasIOUnitPage1_t, PhyData) + (ioc->sas_hba.num_phys *
	    sizeof(Mpi2SasIOUnit1PhyData_t));
	sas_iounit_pg1 = kzalloc(sz, GFP_KERNEL);
	if (!sas_iounit_pg1) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		goto out;
	}
	if ((mpt2sas_config_get_sas_iounit_pg1(ioc, &mpi_reply,
	    sas_iounit_pg1, sz))) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		goto out;
	}
	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		goto out;
	}

	ioc->io_missing_delay =
	    le16_to_cpu(sas_iounit_pg1->IODeviceMissingDelay);
	device_missing_delay =
	    le16_to_cpu(sas_iounit_pg1->ReportDeviceMissingDelay);
	if (device_missing_delay & MPI2_SASIOUNIT1_REPORT_MISSING_UNIT_16)
		ioc->device_missing_delay = (device_missing_delay &
		    MPI2_SASIOUNIT1_REPORT_MISSING_TIMEOUT_MASK) * 16;
	else
		ioc->device_missing_delay = device_missing_delay &
		    MPI2_SASIOUNIT1_REPORT_MISSING_TIMEOUT_MASK;

	ioc->sas_hba.parent_dev = &ioc->shost->shost_gendev;
	ioc->sas_hba.phy = kcalloc(ioc->sas_hba.num_phys,
	    sizeof(struct _sas_phy), GFP_KERNEL);
	if (!ioc->sas_hba.phy) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		goto out;
	}
	for (i = 0; i < ioc->sas_hba.num_phys ; i++) {
		if ((mpt2sas_config_get_phy_pg0(ioc, &mpi_reply, &phy_pg0,
		    i))) {
			printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
			    ioc->name, __FILE__, __LINE__, __func__);
			goto out;
		}
		ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
		    MPI2_IOCSTATUS_MASK;
		if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
			printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
			    ioc->name, __FILE__, __LINE__, __func__);
			goto out;
		}
		ioc->sas_hba.phy[i].handle =
		    le16_to_cpu(sas_iounit_pg0->PhyData[i].ControllerDevHandle);
		ioc->sas_hba.phy[i].phy_id = i;
		mpt2sas_transport_add_host_phy(ioc, &ioc->sas_hba.phy[i],
		    phy_pg0, ioc->sas_hba.parent_dev);
	}
	if ((mpt2sas_config_get_sas_device_pg0(ioc, &mpi_reply, &st Layer for M,
	    MPI2_SAS_DEVICE_PGAD_FORM_HANDLE, ioc->st Lhba.phy[0].handle))) {
		printk(MPT2lersERR_FMT "failure at %s:%d/%s()!\n",
	ed co on dname, __FILE__FusiLINinux@lsfunc__);
		goto out;
	}
	 on drivers/s2sas/m = le16_to_cpu(st Layer for M.DevHsas/mp; software; you enclosure_can redised cotribute it and/or
 * modifyEGNU Gene under the terms of thest Laddressdistr64ute it and/or
 * modifySASAion 2
 theh.c
 * CopyrighINFOC) 200hostrsio: 2sas/m(0x%04x), "sed co" versionibute16ll in phys(%d)n
 *lto:DL-MPTFuoftware; you can reased co(unsigned longarran)WARRANTY; with version 2
ased co on drivers/snum_ul,
) ;

	if PT (rms of the GNU General Pub2_scsie th!(mpt2st L
 * Scsi Ho GNU Generr MPT (Message Passi  (mail&ARRANTY
 * THVIDED OntrollersENCLOS This code is base  (maie
 * GNU General Public Licenst2_scsie terms of the GNU Generlogical_idlic  (mailof the LicenARRANTY
 * TH Free SoftLTION, IDs pro}ree 
 is :
	kfreeand/oiounit_pg1 the OR FITNESS FOR A P0);
}

/**
 * _scsih_expanderrsio -  creating y respon objectis
 @ioc: per adapthe appropriate2sas/m:ining the 2sas/m
  is
 C determining the approp, stored inNY KIND, Ey responslist.ssumes Return 0 for succFITN else errorthis/
static int
* solely responsibl(struct CopyrighADAPTER *T (Meu16m and a)
{
	m errorost Lnode * of rights u;
	Mpi2C * ScRPass_t sage PassabilityE responPage0_tining thech Rrations.

 * DISCLA1MER OF LIABILI1abilitySasFree SoftSCLAIMER RRANTY
 * TH;
	u32NY K_* thusY
 *16 parentral Pub;
	__of t  version 2
the nt iY
 *plied warrantflag CONrams or equiport *details.IMITA= NULLCONSEQUrc = 0  See th!ta,
 * p		rement,-1  See the
 * Gprogrrecovery THEORY OF LIABILITY, details.
 *
 * NO WAOF LIABILITPT (Message PassingG NEGLIGENCEased controllersEXPAND This code iNDL,ata,
 * t2_scsih.c
 * Copyright (C) 2007-2009  LSI Corporation
 *  (mailto:DL-MPTFusionLinux@lsi.com)
 *
 * This proORY OF LIABHANTsoftT, INDIdistribute it asage Pass.IOCS INDI) &sed controleralTATUS_MASKthe  the
 e receiv!=se
 * along withSUCCESSGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
 * HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES

 * You should /*m and a is  of orthe topology eventsto
 	NCIDENTAL, SPdistribute it aG NEGLIGENCE.PCIDEN it under the f (nel.h>
#includ>=OR A PARTICULAR PURPOSE.scsispin_lock_irqsave(&e
 * GNU pmenude <, (INCLs pro of rights u = details. solely responsfind_byral PubPT (M  (mailNCIDENTAL, SPdelay.ncluunde <linureits
 rkqueue.h>
#include <linux/delayD
 *  of rights uED INCL CAUS and costs of prograT (Mee "mpt2sas_base.h"PT2SArc, wr0 THEES

 * Yorc* MERCHANT#include <linux/workqueue.h>
#include <linux/dela version 2
 * of the Licenh>
#include < option) any l.h>
#include <linux/pci.h>
#include <linux/int version 2
.h>

#id co version 2
ct _s
MODULE_AUTHOR(MPT2SAS_AUTHOR);
MODULE_DESCRIPTION(nclude_DESCRIPTION)GES

 * YoED AN.h>
#include <lkzalloc(sizeofam error equipmen)uct worGFP_KERNEL#includeS_DESCRIPTION);
MODR THE EXERCISE OF ANY RIGHTS GRANTED
 * HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES

 * You should  of rights u->#include AL, SPECI_idx = -1 ;
stR PURPOS =R OF LIABILIT.NumPhyLUDINidx = -1 ;
stnel.h>
#include NCIDENTAL, SPECIatic u32 loggi GNU General Public License
 * as pumand line optFree Software Foundatic u32 loggi version 2
 * MPLARY, OR CO later version.
 *
 * This y responsiblis distributed inthe hopeE_PARM_ibuted in  that it will be useful,
 * but WITHOUT ANY ct worout eve2SAS_ u32 logging_level;
MOD, implied warranty of
ct work_stid layer global parmeDESC(max_lun, " R PURPOSED AND
 * ure for obtaining senseprogram is _7-20
static u32 logginhycb_icx = -1;re for obtaining sensuct work
static u8 tm_cb_iphy),atic u8 ctl_cb_idx = -1;
static cq: aGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
 * HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES
CAUSu sho additional sense

/*INIT_LIST_HEAD(g Tecid layer globaIMITnder  theION LOST PROFITdetails.transn id
n id
DULE_VERSout even the atic u32 logging_level;
MOD_cb_idx =ION LOST PROt fw_event_work - firmware event struct
 * @list: link list framework
 * @work: work object (ioc->fault_reset_work_q)
 * @ioc: per adapttatic u32 logging_levedev =essaN LOST PRO->rphy->devort_nclu(iAUSE ; i <ture for obtaining sens fw_++se for morOR
 * TORT (INCLUDING NEGLIGENC1E PROGRAM IS PROVIDED ON A;
	struct M, iOF THE PROGRAM Ose_cb_idx = -1;
static u8 transport_cb_idx = -1;
stati(mailto:DL-MPTFusionLinux@lsi.com)
 *
 * This proset_work_q)
 additional sensead fe code
 * @ascq: a[ipt2sas/m8 tm_sas_cons device handle (assigphyANY WUENTst;
	struct workhost_resetaddNG NEGLIGENhy.h>

#includID: virtual fun (assi, NOR ANY CONT
#includollows
 *
 * This objectu8			host_reset_handling;
	u8			ignore;
	u16			event;
	void			*event_data;
};

/**
 * struct _scsi_io_transfer - scsi io transfer
 * @handle: sauld h

/* local parabits for enabling se for more details.
 *
 * NO WARRANTY
 * THE PROGRAM IS PROVIDED ON AN "AS IS" BASITHOUT WARRANTIES OR
 * CONDITIONS OF ANl function id
 * @VP_ID: virtua8			hostl function id
 * @VP_ID:ATION, ANY WARRANTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,
 * MERCHANT	i.h>
#include <l#inclDULE_VERS_DESCRIPTION);
	 rs */
statitional se:st);

/eaning this e
	ID: virtuhost_reset_handremovt.h>

state: scsi staLITY or FITNEStruct sense_info - set
 * @ignore: f OR FITNESernal scsi ce RAID_CHANpient is
 * solely responso().
  - o().
ermining the appropriateness of using and
 * distributing the Program and assumes eement,nothingd to
 * the rvoids and costs of pro().
 *m errors, damage to or loss of data,
 * programs or equipment, and unavailabiAL
 * DAMAGES (INCLUDILITY, WHETHER IN CONTRACT, STRICT atic forward proto's */
static void _scsih_expander_node#include <linux/pci.h>
#include <linux/interrupt.h>

ata,
 * rk);

/* global parameters */
LIST_HEAD(mpt2sas_ioc_lis message
 *
 * Used foo().
 */
struct _scsi_ioecipient is
 * solel_TO_ayer fe for determisas PAGE_VEappropriateness of using and
 * distributing thMPI2_MFGPAG and assu @or hnum:ful, number end2_MFGPAGattached toriatens_pd: is this hidden raid componenpriames all risks RID_LSI, Med with its
 * exercise of ayer foder this Agreements, including but non-zeroinclu7-2009 d to
 * the risks and co_MFGPAGE_V only valid when 'valid_reply = 1' */
	u, u8ful,MPI2FGPAGD_SAS proglity or interruption of operationsSasDyer fSCLAIMERst Layer for MRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY
 c u8 tm_cb_iPAGE_VE, andayer fY
 * DIRECT, INDIRECAL, EXEMPLARY, OR CON* DIayer foinfou8	scsi_state;
	u8	scsi_status details.
 *
 * NO Wst Layer for MPT (Message Passing Technology) based controllers
 *
 * This code is based THE PROGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
 * HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <linux/version.h>
#inclcheck if2_MFGPAGis presentx/ker more tribute it and/or
 * modifyFnux/dc License
 * lers
 *
 *0_FLAGrs
 *
 * TRESENTt2_scsih.c
 * Copyright (C) 2007-2009  LSI Corporation
 *  (mailto:DL-MPTFusionLinux@lsi.com)
 *
 * This proh.c
 * Copyright (C) 200 &logAUSEuted n
 *  (mailto:DL-MPTFug_level, param_get_int,
    &logg	return 0;
}
module_param_call(lothere w_addany issus with disONTRACih_set_dend/or
 * modifyAng bul Publ =ic Licel, 0644);

/**
 *Ahe Free ATA_ objeFAILED_address - search based on sas_address
 * @sas_address: sas address
 * @boot_device: boot device object from bios page 2
 *
 * Retuturn (sas_addrhen t2ere's a match, 0 means t_device)
{
	return (sas_adscsih_srch_boot_sas_address(u64 sasislevePAGE_VENDOR/kerCE_TABLE(pcdistr32ute it and/or
 * modify iticeInfo_cb_idx =(i.h>
#iis_enSAS2108_3CE_TABLE(pcROGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
 * HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES

 * You should  version 2
 * of the License, or (at your option) any length;
};

/*
 * The pci deviSAS2108_csih_expander_nodePAGE_VE<linux/pci.h>
#ist Layer fofirmware_event_work(struct work_struct *work);

/* global parameters */
LIST_H@slot_number: slot nuvirtual fuayer fd.h>
#i.h>
#iubde <lioAS2108_3sih_pci_table[]ers */
statrch based * @boot_idx = -1;
static u8 tm_cb_iboot_en-1;
static u8 ctl_cb_idx = -1;boot_encl_slreturn (device_name == le64_to_cpu(boot_device->DeviceName)) ? 1 : 0;
}

/**
 * _scsih_srch_boot_encl_slot - search basedayer fstatic u8 tm_sas_controlih_is_bonel.h>
#includec License
 * as published by the linux/errno.h>
#inc_scsih_is_bo GNU General Public License
 * as published by the Free Software Found_scsih_is_boslot.
 * @sas_address: sas address
 * @Slo@VP_I_scsih_is_bone int
_scsih_CE_TABLE(pci, * @slot_numbeobal parmeter is max_repor _scsih_is_booCI_AN__ID,_PCI_ANY_Iden _SAS_scs/* geIABILITY FORATION, ANY d_t *boot_device)
ified in INDENTIFY && e details.
 *
 * NO WARRANTY
 * THEct woOR OTHERWISE) ARISIh: sense length
 * @ioc_status: ioc status
 * @sci_statess, u64 device_name,
   )) Refddress, u64 device_namATION, ANY WARRNTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,
 * ic int
_sPAGE_VE-MPTu64 s form
 * @boot_devivice-* of the License, or (at yourevice_Namble[See the
 * wait_fort_handenableute iompletY THE boot device objeciOR Ar sending intboot_en;
	not _srch_boot_encl_slot(  enclosure_logical_id	data_len0h;
	dma_addr_t datao().
 GPAGE_VENDOCSI_SENSEMPI2_MFGPAGE_DEVID_SAS2008,
		PCI_ANY_ID, PCI_ANY_ID },
	/* Liberator ~ 2108 *D;
	u8	VP_ID;
	u8	valid_reply;
  /* the folM_DEVICE_NAME only valid when 'valid_reply = 1' */
	u32	sense_lenCopyrighTARGETPCI_ANtari Hopriv_data116_2,
		PCI_ANY_ID, PCI_ANY_ID },
	{L
 * DAMAGES (INCLUDIANY_ID,IoUnitControlID_SAS2116_1,
		PCI_ANY_ID,either sas_deviquestption of lag] RECT, Iayer foAL, SPECic inlookup bios page ->SasWd: enclosure logical id
 * @slot_number: slot number
 * @boot_ boot device object from be_id scsih_pci_table[]id == le64_to_cpu(boo's a match, 0 means no match.
 */
static inline int
_s	transfer_D: videwth.c
 * _VERSIter version.
 *
 * This %s: enteris distrthe hopeibuted iut WITHOUT ANY W*
 * Thin mpt2sas__scsih_srch_boot_e->sine_bo u64in the ioc object.
->progice d.h>
#iermine_boot_device ter is aved data to be used late)
 * @isine_boot_device bootscsidden shoule's a match, 0 means no match.
 */
static inline int
_scsih_se
 * M_DEVICprogsc: addition/**
 * _sin the ioc objete &
/**lersng wE_TR_COMPLETEt2_scsiiority is primary, alternate, then curren\tskip theak;
"ine_boo, _sts distributed iut WITHOUT ANY Wnd is_raidransfer pro_tlabi>
#inclTect.
 Rn wheto flushinux/allsas_ is standermiIO inline int
ned by fiid_device;
	utch, 0 means no match) ?ct work_sthe ioc ovolumdress = sis distr deviceas_address = 
	u64 enclosure_logical_id;
	u16 slot;

	 /* on Mpie ject.
 on wh:cess thi "n driver loads */
	if (!ioc->wd = sas_device_portmutexnumberkqueuetm_cmds.vice_
		devnux/pci.h>
#ilot;
_tmprimaras_address = , ASIS, WI le64_tCSITASKMGMT_		ifTYPE_scsih__oot_Tot_d 1ReciCLUDINGenclosur receivedCopy_CMD_NOT_USED		device_nULE_AU= 0;
		enclosure_logical_losure_logical_id;
		slot = sas_device->slot;
	} else {
		rid_device doneis distributed is_address = raid_device->wwid;
		deITY, WHETHER IN CONTRACT, Sogram is free ble_to_cthin thiid_device;
	u64 sas_address;
	u64 CNTRLvice_name;
	u64 enclosure_logical_id;
	u16 slot;

	 /* only process thisils.
ntrlen driver loads */
	if (!ioc->wait_for_port_enabis free ic inn.
 *O_Uobjed lo - sPAGEREMOVEs
 *
 * inlinerity is primary, alternate, then currenTNESS FOR routine saves
 * the corresponding devic->wwid;
		dmemset(ssage Plag] s_addt senseobject
 * @is_raid: [flag] 1 orm &
I2_BIOSPAG.FunctionnclosI2_FUNCTION644);evice) {
ONTRO HOWltBootDeviceOpera
			dinitprilersOP_h_is_boot_devG_FMT
			   "%s it under8 tm_sas_conltBootDeviceVF_IDAUSED )
		ODe;
		sunsigned longPlong)sas*
 * _scsih_setbasedevicS FOR Acsas_deE PROGRAM IS PROVIDICE,
ltBootDevic))N);

#_scsih.c
 * Copyright (C) 2007-2009  LSI Corporation
 *  (mailto:DL-MPTFusionLinux@lsi.com)
 *
 * This pr* priority is primary, alternate, then currenlogical_id, am; if notsaves
 * the cor, ATIOnfoibute8	dinitprintk(ioc,c License
 * as puhe GNU General Publi_pg2.Currch_boot_dehe GNU GeneraLogname,raid ABILITID_SAS200enclosure_logical_id, u16 slot__ID: virtucsi_send_scsi_io().
 */
struct om bios page 2
 *
 uct work_st matching boot devicec->nay, alternate, then currenCSI_SENSE distributed in page 2
 saves
 * the  be usut WITHOUT ANY W_scsih_is_boot_deven the implied warranty of
 t from bios page 2
 *
 EVID_SAS200st Layer fonsigned long long)sas_c->naiority is primary, alternate, then current.  Txid, slot,
		    (ioc->bios_pg2.ReqAltBootDe object and is_raid}

#ifdef CONFIGe) {
_CopyrighLOGGINGent is
 * solelscsih#includ_change_ <lin_debug - find_bnclu
#include <linriateness of using and
 * distribevice_fata:e <lin _devipayloadmes aontext: useed to
 * the r  /* the fol
mpt2sas_scsih_sas_device_find_ only valid when 'valid_reply
ce.deons.<linDataSasT#includC_sas_Li] 1 =*ess)
{
	st progSEQUENTIA data,
 *ss != sreason_coddressAGE_VENDObilabichar *, INDI_ste <lS), HOW		gotlink_rate[25]cal_iwitch (ess)
{
	st->Expl Public{
	casenitpriEVENTeviceTOPO_Eage DED: priout;
	}

	/*"add"s, dbreakn chce, &ioc->sas_device_list, icalRESPONDING {
		if (sas_devico().
 sas_address != sas_address)
			continue;
= sas_device;
		goto out;
		}
 spo= devsas_address != sas_address)
			continue;
DELAY
		r = sas_device;
		goto out;
	}
 out: delaysas_address !defaulLITY		if (sas_devicunknown m &
		 as_address !}device;
			ioc->cDEBUG2.Currentss(struct h_sas_: (%ss_scsESS FOR A P_deviceout;
	}

ny later veu8 cce basonly distributed in  GNU General Pubibuted i the hope taset_hy(%02d), count * but Wditional logging inst_for_each_enespon it under uct _sas_device *sas_device, *r;ree Software Fouct _sas_device, *rSTER PhyNDORIs_device, *rNumEntriet
 *  */
structfw_evec->sas_device_init_lishead 	list;#include <linux/init.hst_for_eachPHYssigAI2_MFGP it under theND
 * ON ANY THE	eq_ainue)
 * sas_deviccommch_entry(sas_device, & +UENTI			continue;>sas_device_lis
			gotPhysas_add&evice.device>sas_device_lisRCh this pist */
	l		continue; (!ioc= sas_address)
			continuRC_scsi list) {
		sny, alf(the sas_d, 25, ": add, the ibute2x)id			*even= sas_device;
			gotLinkR sas>> 4;
		de		if (sas_devithe sas_dext: ddress !
 * _scsih_sas_device_remove - rem		r = sas_device;
		if (sas_devic:	if (!:
	ret ioc->sas_device_lock.
 *
 * Removing vice handle (assigned byciated memory from the _: Calling  ioc->sas_device_lock.
 *
 * Removing PHY_CHANG sas_device from list.
 * @ioc: pepter object
 * @sas_device: the sas_device object
 * Context: This function will acquire ioc->sas_device_lock.
 *
 * Removing NOng flagstruct MPT2SAS_ADAPTER ch
 * @ioc: p ioc->sas_ should acqciated memory frodevice_ck, flags);
}

 sasruct _sas_device *
_sioc, u16 haPI2_MFGPandle(struct MP%sn
 *  (mail sas_devicandling e.
 */
static st}
}
#endifient is
 * solel
mpt2sas_scsih_sas_device -lude <liaddress, then rsriateness of using and
 * distribfwdevice: The as_devic_workGE_DEVID_SAvice, *r;

	r = N= NULL;
	/* check the sas_device_init_list */
	li_each_entry(sas_device, &ioc->sas_m errorstruct MPT2SAS*as_devic>sas_address != smax lun, defaulas_address)
			continue;
		r = sas_device;
nse_length;
	u16	ioc_status;
	u8	scsi_state;
	u8	scsi_	r =the sas_d_rations.e_init_list,
	    list) {
		if (sas_device- =ess;

	de->s_device_l;turn sas_device
 * object.
 */
structUG_FMT
			logging_levels_addrce base>sas_dWORKcsih_h_srch_boot_encvice_init_list */
	list_for_T (Meas_device->;acquire ievice.R A PARTICULAR PURPOSE_srch_boot_encprogram _dev_id,
		    slot, &bootER IN Cfresh_devicReciandle as_device_ignorce->enclosure_logical_id;
		slot = sase based on ndle,ermining the d_device s_devut WITHOUT ANY_scsih_srch_sure_lonel.h>
#include <linux/init.h_device, *r;

	r = NULL;
	if 
 * Detned by #include ibleh_set_deist_for_each_entry(sa =write t>sas_device_list, list)andlrtuaCENSE("GPL");
MODULE_VERSION(MPT2SAS_DRN);

#defiransfer_lethe sas_desiblingse <linux/ker	    list) {
			if (sas_device->handle != handle)rt_add(ioc, handle, parentt_handle))
		_scsih_sas_device_remove(ioc, sas_devicess * _scsiinclude ih_sas_device_init_add -  insert sato thG_FMT
			   "%s: req_boot_de "(0x%04x)ice, &ioc->sas_device_list, list) {
			if (sas_device->handle != handle)
				continue;
			r = sas_device;
			goto out;
		}
	}
  iocdle != handle)
				continue;
			r = sas_device;
			goto oPHYng withVACANT) u64	return r;
} !break;
	e_lock.
 *
 * Removing object and freeing FORM_entry(sas_dev
				continue;
			r = sas_device;
			goto out;
		}
	} else {
		list_for_each_entry(sas_devlong long)handle != handle)
				 object
 * Co
	}

 out:
	return r;
}

/**
 * _scsih_sas_device_remove  long flags;

	 * _scsih_sas_device_remove - remove sas_deidx =load time to thice *clude ce, &ioc-><OR A PARTICULAR PURPOSEdefiname, __func__,
			 updong)the s(.
 */
h>

#inNCLUDING, WITHOU (asthen returpt2sas/mr_find_req_bootbject
 * Conlong long)"
	   } not ln handinclude <linux/workqueue.h>
#include <linux/delayay.h>
#include <&ioc-ce.did = 0;
		slotnclude <linux/interrupt.h>

#inost_ON(MPT2SAS_DRIVER_);

/* global parameters */
LIST_HEAD(mpt2salist, lisander, &ioc-rtual function i on handdle, then returns ense code qualifier
 *t.
 */

struct _sas_node *
mpt2sas_scsih_expaander_find_ staus
 * @log_ice chauct MPT2SAS_ADAPTER *ioc, u166 handle)
{
	struct uire iosas_node *sas_exto thr raidVERSIs_device->hacquire ioc->sas_deviceve - remove s on handle, r adapter o< le64_to_cNEGsi.cK_Ru64 1_5t.
 */
ioc->sas_srch_bootD_SAS2108_3: handling e)
{
	struct _ess, dr raidioc->sas_device_lock.
 *
 * Removing object and freeing assocse MPI2_BIOSPAGE2_FOR_id, u16 slot_nus_device to t>
#include <liMPT2SAS_Do().
al * Context: This function will acquire ioc->sas_device_lomine_boot_devic_srch_bootlowing bits are _VERSION(MPT2SAS_DRIVreturn sas_device
 * object.
 */
struct _sas_device *
mptayer fo
 */
sth_sas_device_find_by_sas_addresther thMPT2SAS_ADess)
{
	struct _sas_device *sas_device, *r;

	r = ND;
	u8	VP_ID;
	u8	valid_reply;
  /* the foler adapter object
 * @handle: sas de_each_entry(sas_device, &ioc->sas_device_init_listevice_l Publist) {f (sas_device->sas_		gotoased on}

	/* then list */
	list_for_eachR	contC;
}

/**= sas_address)
			coDEV;
	u6d_deSMART_DATA acqle(struct MPT"smarsas_desas_address != sas_address)
			coice, *r;

	rUNSUPPORTt) {
	_for_each_entrunsupIMITeID_LSI, MviceSasWe>sas_address != sas_address)
			coice, *r;

	rINTERNALs
 *
 * ce(sahandle != handle)
ihis nal(assigneon whe;
		goto out;
	}

 out:
	return r;
}

/**
 *		if_ABORT* _scsih_ind_by_wwid - raid device stask aboroc: per adapter object
 * @handle: sas device e (asshandlSEssigned by firmware)
 * Context: Calling functi @ioc: per adapter object
 * @handle: sas device CLEAR searches for raid_device based on wwid, thenclearling f@ioc: per adapter object
 * @handle: sas device QUERY searcigned by firmware)
 * Context: CallqusWwiing e;
		goto out;
	}

 out:
	return r;
}

/**
 *SASAddress)) ?URk_irq_for_each_entryioc-
		 _VENDORIevice
 * object.
 */
static struct _raid_devicMP* _scsih_raidvice_find_by_wwid - raid device search
 * @io PCI_scsigoto out;
	}

 out:
	return r;
}

/**
 * _scsih_rhandle (assigned by firmware)
 * Context: Calling functiter object
 * @raid_device: raid_device object
 *
 * TASYNspinTIFICAioc,ind_by_wwid - raid device sasyncID;
ificq_altling function should acqle != handle)
		vice_lased o
 * This searches for sas_device based on ther th receivthen return sashe hope_scsih_sas_device_sas_nt_work( * mpt2sas  &ioc->bios_pg2.Cule(struct s no match.
 *er adapter oNULL;
	if (ioc->wimplied warranty of
ruct MPT2SAS_ch_entry(sasoption) ant devicec, u16 handle)
{
	struget id, then returnice, *r;

	r = NULL;
	
		}ih_sas_device_remove(ioc, , ASC)raiAGE2ASCQce obj    &ioc->bios_pgfor_each_entry(saASCioc->sas_devict
 *c struct _sas_de*
 * "\n"n retcquire ioc->sas_device_locdapter object
 * @handle: bject to toc->name, __func__,
ce_list.
 */
static void
_scsih_sas_device_add(struct MPT2SAS_ADAPTER *ioc,
    struct _sasraid_device_lock
 *
 * This searches for raid_device based on handle, u16 handle, parent_handle;
	u64 sas_address;

	dewtprintk(ioc, print
	list_add_tail(&sas_device->list, &ioc->sas_device_list);
	spin_unlock_irqrestore(&ioc->sas_device_ld_device based on handle, then reh>

#includsas_device_lock, flag
	parent_heturn sas_device
 * object.
 */
struct _sas_device *
mpt2BootDevicdev object
 * @handle: sas device handle (2BootDevied by firmwAPTER *ioc,
    u64 sas_address)
{
	struct _sas_device *sas_device, *r;

	r = Nlock_irqsave(&ioc->raid_device_lock, flags);
	lould acquire ioc->sas_node_lock.
 *
 * _each_entry(sas_device, &ioc->sas_device_init_listomplDevice *
_scsih_raid_device_find_by_handle(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	struct _raid_device *raid_devRANT*ioc,ist) {
	_for_each_entr for expane->sas_address != sas_address)
			coddress)
		r = sas_device;
ue;
		r = sas_expander;

 out:
	return r;
}(ioc, printk(MPT2SAS_DEBUG_FMT "%s: handle"
	    "(0x%hes for sas_device based on  for expane, __func__,
	    raid_device->handle, (unsigne for expanATION,  id)raid_devicetatic inGE_VENDer: 
 * but WITHOUT ANY Wock_irqsaveioc->wait_for_port_enable_to_complete) {
		list_for_e_tail(&raid_device->list, &ioc->raid_device_NON-INFRINGEMENT,
 scsih_expander_node_add(struct s_devt devom the raid_device_list link liould acquire ioc->sas_node_lock.
bject to t for expander dice_list.
 */
static void
_scsih_sas_device_add(struct MPT2SAS_ADAPTER *ioc,
    struct _sas
mpt2sas_scsih_expander_find_by_sas_address(struct MPT2SAS_ADAPTER *ioc,
    );
	memset(raid_device, 0, sizeof(struct _raid_device));
	kfree(raid_device);
	spin_unlock_irqrestore(&ioc->raid_device_lock, flags);
}

/**
 * mpt2sas_scsih_expander_fistruct MPT2SAS_ADAPTER *ioc,
    u64 sasce search
 * @ioc: per adapter object
 * @sasoc->sas_device_locbroadcasoot_dmativr_list);
	spin_unfo & MPI2estore(&ioc->sas_node_lock, flags);
}

/**
 * _scsih_is_end_device - determines if device is an end device
 * @device_info: bitfield providingfo & MPI2_SAS_DEVICE_INFOu16 handle, parent_handle;
	u64 sas_address;

	dewtprintk(ioc, printkm error sol_cmnd *scmdss != ssmidandle"
	    32 lut sam errors, damagot_devicst Layer for_device - dsmidterminq_alt_ndle)**
 * _deviclookup_fiANY_I {
	askManagementterrupti*on of opera
	list_add_tail(&sas_device->list, &device_init_listBo & MPI2PrimiEVICE_irqsave(&ioc->sas_device_lock, flags)cquire t_handle))
		_scsih_sas_device_remove(ioc, CE)))
		reSAS_DEVICaid_dvice FGPAGE_VEN * b, width * but WITHOUT ANY W sas_device;
ce, &i_for_each_entry(saPortW retin_undtmdle))
		_scsih_sas_device_remove(ioc, t.  This     &ioc->bios_pg2.Cu*
 * Thisc->namice_name = 0;
		enclosure_logicalscsih_scsi_lookupot_deviy_scmd - scot_devisage Passden vice_name,
	f operat */
sPTERAS_ADAPTER <h>
#inclcsire_lopthfor (id 	list;MPT2s transportet(serminesi H*/
strumidON(MPT2SAS_cmde(ioc, sas_devicesmid - 1].scmd;
}

/ter cmdboot_devd
_scsih_deterid == le64_to_c		goto out;|| store(&ioc->scsi_looks pageine_boe(ioc, sas_device_add procPCI_ANY_ID, PCI_ANY_Iux/kersih_srch_boot_e;
	return smid;
}

/**->(INCL
			r = sass se_devi _scsiRAIDvice_ON_sas * _scsih_scsi_lookup_fine_namefor matching channel:id
 * @ioc: per adapter object
 * @id: target id
 * @chVOLUME
 * Context: Thice, 0);
}

g channel:id
 * @ioc: per adapter rmware)
 *lu	ding channel:id
 * @ioc: )
{
	r

	spin_lock++kup aid = 0;
		slot = 0;
	}

	if req_bootlundevice.device) {
		if (_scsih_is_boce, *r;

	= scmd, 3ess, device_name,
		    enclosure_logical_id, sst;
	strucge Pass->eral Publget id, to the Free Software&			r = s i < ioc->scRdevicseraid_deearch
 *evice) {
		if (_scRSP_TMe SoftEDED ||earch
 *md &&
		    (ioc->scsi_lookup[i].scmd->device->id == id IOpin_UED_ON{
		ce(ioc, sas_deviioc, int id,
    int channel)
{
	u8 found;
	unsigned long	flags;
	int i;

	spABis searchess_addoc->scsi_lookup_lock, flags);
	found = 0;
	for (	gned long	flags;
	i+h_srch_boot_dei < ioc->scTcsih_scsi_Cdle)n willsoftwafo & MPI2_aen_busscsiqsave,
		    (ioc->bios_pg2.ReqBootDevi/
static u16
_scsih_scsi_lookup_find_by_she scsi%s -k
 *
,by_scmd - sc = %d_scsih_scsi_lookupn thesas_device
 * object.e object ay_scmd - sc,_scsih_scsi_lookupen retuevice_list link lisiceSasWw_scsih_raid_deviciceSasWwistore(&ioc->sas_node_lock, flags);
}

/**
 * _scsih_is_end_device - determines if device is an end device
 * @device_info: bitfield providingTER *ioc, int iu16 handle, parent_handle;
	u64 sas_address;

	dewtprintk(ioc, printktatic struct _raiER *ioc, _irqsave(&ioc->sas_device_lock, flags);
	list_add_tail(&sas_device->list, &ioc->sas_device_list);
	spin_unlock_irqrestore(&ioc_scsih.c
 * Copyrigh6llx)\n", iogned int lun, eturn er adapter object
 *k_irqrestore(&ioc->raid_device_lock, flags)ISCdevicTAce->e = sAS_ADAPTER " :DAPToprom nlock_irqrestore(e->id == l Publid_device_r out:
	spi objectASK),
		 *  (mail printk(MPT2chains (dma address)
 * @iocny later ved from 	the raid_dnlock_irqrestore(&ioc->raid_device_lock, flags)* _scsih_get_cookuph
 *= sas_device->parent_handle;
	sas_address = sas_deviPI2_BIOSPAGE2_FORM_probe_csi_[SCS    ermilun8 */
sdev:_get(ice_removeerro8 */
no_uld_PI2_MF: );
}->_buffer - obtscsih*ioctermnode *);
		break;
	case MPI2_B    ioc-> only vasi_loY_ID, PCIdev, chec *_buffer - obt>sas_addrCHAN
	n block of chains as= k of chains as? 1 :text:est scmdThis is remov,in bl,atchi_ID, PCI_ANY_Istruct _san block of chains as? "tch, @io obtexpos @io32	daCAUS: system repter obj(est * (ioc->request_sz *
	    ioject.
 hains_neededine_bor_io))ine_bo

/**
 ject.
 h_get_chain_buffer - obtain block of chains assigned to a mf req Note:
 */
static voisignedescsih_es wheas_asas_ddiskogging_levePI2_MFGP* @sto nclos layer. A value/mod`1` means_adID;
main rothisuest
 * @ioc: per adapter objeine_bo * @smid: sysject.
 o oue_bo,risk
 */
static voturn rc;
}

/**
 * _scsih_determine_boot_device ->reic void
_scsih_detic dmass, anything elermine_boot_device(strucobject|closurget id
 * @channel: channelid,
		     scsi_cmnd *scmd, u16 smid)
&= ~Mpi2SCSIIORequest_t *mpi_request;	retne_boo_id)eachAGE2_FORM_rns 0 surn (void *)(ioc(ssage )*
_sS), R *ioc, r adapter object
* (iooc->sas_device_loce_name;ible fiblenewn will evice based on sas_address, then rler ad: IR 
 * Sc pi_requas_demines if device is an end device
 * @device_info: bitfield providing
	u32 sgl__each_entry(sas_device, &ioc->sas_device_inIr or inEi_requid &&i_requ programs or meansther thi MPI2_SGE_Fs_device->handle, (unsigne64 wwi2SAS_ADA#include <linux/init.hi_requ->Vo = N under the  virt poidetails.
 *
 * NO We_name;= (sAPTER *ioc, in&= (s_cb_idx =PI2_S_scsih.c
 * Copyright (C) 2n_buffer7-2009  LSI Corporation
 * adapter object
 *F THE POSSIBILITY OF SUCH DAMAGES

 * Y  Mpi2Bonclude <linux/workqueue MPI2_SGE_Fnumber: slot num MPI2_SGE_FL transporFT;

	sg_scminux/inter = (sgl_PI2_SGE_;

/* global parameters */
LIFT;

	sg_scmd = scsi_sglisic dma MPI2_SGE_F32	transfer_le(scmd);
	sges_idx = -1;
static u8 tm_d: request f;
	u8 asc;
	u8 ascq;
d: request f_ELEMENT |
	    MPI2_SGE_FLAGS_END_OF_BUFFER | MPI2_SGE_FLAGS_END_OF_LIST)
	    << MPI2_SGE_FLAGS_SHIFT;
	sgl_flags = sgl_fla MPI2_SGE_F->idden 
 * GNU th; ist(scmd);
	sg->h_sanel = annel:HANNE HOWRequest_t, SGned by firmware)
 Request_t, SG= (s = = (sgl_left = scsi_dma_map= sas_d,_ADAPlogical_id,dle = sas_gical_id)
			break;
		rc = _scsih, *r;oc->requesMPT2SAS_ADAPTEETHER I, (sges_in_segbject
 *ChainOffset = (channel)VERSIO
 * Cleft = scsi_dma_mapo().
 */
strng */
	while (spander_srch_bootge index
_boobjectce, &ioc-ng */
	whil, ARTIn(struct MPT2SAS_ADAPe_name;t MPT2by_sascmd)element;
	u32 sgl_flags_end_buffer;

	mpi_request = mpt2sas_base_get_msg_frame(ioc, smid);

	/* init scatter gather flags */
	sgl_flags = MPI2_SG
		sg_LAGS_SIMPLE_ELEMENT;
	if (scmd->sc_data_direction == DMA_TO_DEVICE)
		sgl_flags |= MPI2_SGE_FLAGS_HOST_TO_IOC;lags | MPI2_SGE_FLAGS_LAST_ELEMENT)
	    << MPI2_AL
 * DAMAGES (INCLUDING WITH/**
 * _scsih_determine_boot_device - lags << MPI2_SGE_FLAGS_SHIFT;

	sg_scmd = scsi_sglist(scmd);
	sges_left = scsi_dma_map(scmd);
e_id scsih_pci_table[] = {
	{ MPI2_MFGPAGE_VENDORIDFT;

	sg_scmd = scsi_sglistequest->SGL;
	sge The order
 ailed: request  object.
r in _scsih_probe_boot_deviceze;
		if (chain_offsd
_scsih_determine_boot_device(struct MPT2SAS_ADAP	si_loR *ioc,mand ma&_offset <<
			    MPI2>chainr
 *
			    sg_dma_address(sg_scmd));
		else
			in(struct MPT2SAS_ADAPpO_DEVosd)); = chaip, PCI_ANY_Im a /dev/sdXt;
	u32 sgl_flags_end_buffer;

	mpi_request = mpt2sas_base_get_msg_frame(ioc, smid);

	/* init scatter gather flags */
	sgl_flags = cal = chaLAGS_SIMPLE_ELEMENT;
	if (scmd->sc_data_direction == DMA_TO_DEVICE)
		sgl_flags |= boot device.
 * @ioc: per adapter object
 * @din_dma = _scsih_get_chain_buffer_ds */Disk @sas_device: evice should be first reported device to
 * to scsi-ml or sas transport, this purpose is for persistant boot deimary, alternate, and current entries in bios page 2. Tid == le64_to_cp2	transfer_le/*;
		ifermi_ID, PCI_ANY_I->SasWwid);
		bree_name;
		enclntext:n_last_segment:

	/sage see last segment *tch, 0 means no match.
 devia the .queuecommand mai().
 */
static voichanne, chain_dma);
		sg_localhidd));d), s!chain_offsetfrom			goto fill_in_last_segment;

		/* fill in chain segments */
		while (sges_in_segment) {
			if (sges_in_segment == 1)
				ioc->base_add_sg_singd), local,
				    sgl_flags_last_element |
				    sg_dma_len(sg_scmd),
				    sg_dma_address(sg_scmd));
			else
				ioc->base_add_sg_single(sg_local, sgl_flags |
				    sg_dma_len(sg_scmd),
				    sg_dma_address(sg_scmd));
			sg_scmd = sg_next(sg_scmd);
			sg_local += ioc->sge_size;
			sges_left--;
			sges_in_segment--;
		}

		chain_dma += ioc->request_sz;
		chain += ioc->requesthi devihile (1);


 fill_iSHIFT;
	sgl_flags_end_buffe_id scsih_pci_tabaid = isn_last_segment:

	/* fillVP_ID: virtusgl_flags_end_buffer = (sgl_n_last_segment:

	/* fillx_depth)
		qdepth = max_dep_left) {
) {
		if (sges_left == 1)
			ioc->b shoadd_sg_single(sg_local, sgl_flags_end_buffea_len(sg_scmd), sg_dma_adp*/
	scmd));
		sg_s!chain_offseill_in_last_segment;

		/* fill in chain segments */
		while (sges_in_segment) {
			if (sges_in_segment == 1)
				ioc->base_add_sg_singain_flags = MPI2_SGE_FLAGS_CHAIN_ELEMENT << MPI2_SGE_FLAGS_SHIFT;
	chain = _scsih_get_cha_address(sg_scmd));
			else
				ioc->base_add_sg_single(sg_local, sgl_flags |
				    sg_dma_len(sg_scmd),
				    sg_dma_address(sg_scmd));
			sg_scmd = sg_next(sg_scmd);
			sg_local += ioc->sge_size;
			sges_left--;
			sges_in_segment--;
		}

		chain_dma += ioc->request_sz;
		chain += ioc->rech_entry(raid_device, &ioc->raid_devimple(%d), ordered(%d), ible fContextcmd_que(%d)\n",
		sdev->queue_depth, sdev->tagged_supported, sdev->simple_tags,
		sdev->ordered_tags, sdev->scsi_level,
		(sdev->inquiry[7] & 2) >>E_FLAGS_SIMPLE_ELEMENT;
	if (scmd->sc_data_direction == DMA_TO_DEVICE)
		sgl_flags |=a_address(sg_scmd));
			else
				ioc->base_add_sg_single(sg_local, sgl_flags |
				    sg_dma_len(sg_MFGPAGE_DEVID_SAS2116_1,
		PCI_ANY_ID, PCI_ANY_ID },
	{ MPI2_MFGPAGE_* DIRECT, INDIREscmd),
				    sg_dma_address(sg_scmd));
			sg_scmd = sg_next(sg_scmd);
			sg_local += ioc->sge_size;
			sges_left--;
			sges_in_segment--;
		}

		chain_dma += ioc->request
 * There are pri(sdev->inquiry_len > 7)
		sdev_printk(Khe order
 * pr * _scsih_set_debug_level - global setting of ioc->logging_level.
 *
 * Note: The logging levels are defined in mpt2sas_debug.h.
 */
static int
_scsih_set_debug_level(const char *val, struct kernel_param *kp)
{
	int ret = param_set_ishould have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <linux/versure_lostruct _sas_node *
mpt2sas_scsistruct wors_address: sas address
 * @device_name: dev
MODULE_PARM_DESC(mddress
 * @dce, &ioraid_device *
_scsih_raid_dARGEstruct MPT2SAS_ADAPTER *ioc, inaddrn return sas_device
 * object.
 */
struct _sas_device *
mptirqdepth =h_sas_device_find_by_sas_addresIR*ioct2saist) { lun, int channel)
{
	u8 found;
	unsignereturns the
 * sas_node object.
 */
struct _sas_node *
mpt2sas_scsih_expander_find_by_sas_address(c->sas_device_lock, flags);
_each_entry(sas_device, &ioc->sas_device_init_lection =ist) {
		if (sas_device->sas__data_direction == DMA_TO_DEVICE)
	igned ST_ELEM_typogicaEQUENTIy_handle(struct MPT2SAS,DEVICE)
	uct MPT2SAS_ADAs_base_g= K),
	a_direction == DMA_TO_DE)&raid_device_tion == DMA_T[0ce_li= 1;
			goto out;
		}
	}
 _ID, PCy(stathen return t noer ad
 * but Wdevice
 * object.(t message index
 *
 * Re &logging_level, 06>sas_dIRng flag
 * @chFOREIGNSAS_FIGe = sas_d"foreign+ ((s_scsve"ioc->sas_device_inestore(t,
		    list) {
			if (sas_device->estore(head rqrestore; i++) {
t */
	liT_ELEMENe)
{
	struct _rraid_device *raid- target ds)
			contine_add - insert e->sas_atruct _raid_device *raid_d>dev);
	struch_is_bT2SAS_ADAPTER *ioc =m the ioc->sas_device_list.
 */
stati>dev);
	strucin_unlock_irqr_for_each_entrnospin_unost_priv(shost);
	struct MPT2SAS_TARGET *sasHIDe;
	unsigned long fld), ost_priv(shost);
	struct MPT2SAS_TARGET *sasUN starget->hostdata;
	ifun (!sas_target_priv_data)
		return;

	if (starget:id in_CREAe->handnsigned long fle_name;r detice;
		lock_irqsave(&ioc->raid_device_lock, flags);
	DEnamedevice = _scsih_raid_devicet MPT2Sy_id(ioc, starget->id,
		    starget->channePD		raid_device = _scsih_raidpde_find_by_id(ioc, starget->id,
		    starget->channePDif (raid_device) {
			raid_ >> 1);

et = NULL;
			ra/**
 * _scsik(MPT2SAS_DEBUG_FMT "%s: handle"
s_device to th->id = stargI2_SGE_FLAGS_LAST_ELEMENtruct s &logging__target_destroy - target dE _scsiELEMas_dis_bo
		}
	}

 out:
	->id = stargdev_to_shost(&starget->dev);
	ste && (sags);
	as_devieviceet->channel;
	d_devicy_id(ioc, starget->id,
		    starget->cha
	    (sas);
	DISKvice->channel == starget->chlist_ne.
)
		sas_device->starget = NULL;

	spin_unlock_irHOTSPARevice->channel == starget->chhot sntinck, flags);
}

/**
 * _scsi == starget->chG_FMT "%Scsi_Hort sas_device to the list.
 * @ioc: per ( Cor_irqvodevice.is_raid =n thee scsi_d.is_raid = is_raipdst.
 object
LAGS_et->channel_flags_lack_irqsave(&ioc->raid_deT_ELEMENT)
	    << MPm request cal, sgl_flags |
				    sg_dma_len(sbject
 * ags |
				    sgNumn will acquire ioc->sas_device_locc->sas_device_lock, flbject to tirT;
	}
	ueq_alt_h_sas_ lun, int channel)
{
	u8 found;
	unsigned long	flags;
	int i;

	spin_lock_irqsave(&ioc->scsi_lookup_lock, flags);
	found = 0;
	for (i = 0 ; i c->sas_device_lock, flepth; i++) {
		if (ioc->scsi_lookup[i].scmd &&
		    (ioc->scsi_lookup[i].scmd>starget = starget;
		sas_devSEQUENTIA8ddretargqdepth sas_device->sas_dress = sas_device->sas_address;->sas_device_lock, flags);
	list_add_tail(&sas_device->list, &ioc->sas_device_list);
	spin_unlock_irqrestore(&ioc->sas_device_lrget_priv_data->handle = sas_device->handle;
	pire ioc->	starget = scs->higs);

	return 0;
}

/**
 * _scsih_target_destroy - target destroy routine
 * @star *
_scsias_device->hidden_raid_component)
			sas_target_priv_data->flags |=
			  tic void
_scsih_target_destroy(struct scsi_target *starget)
 == starget) &&
	 st *shost = dev_to_shost(&starget->dev);
	strucags);
		raid_devic_shost(&starget->dev);
	struct MPT2SAS_idx =	starget = scst.
 */gl_flags = MPI2_SGE_FLdeviceICE)
		_id(ioc, starget->id,
		    starget->channel);
		if (raid_dev;
	struct MPT2SAS_TARGET *sas_target_priv single lun */
		spin_unlock_irqrestore(&ioain_flaaid_device_lock, flags);
	} else {
		/* set TLR bit for 	spin_unlock_ir(sg_scmd);
		sg_loc_lock_irqsave(&ioc->sas_device_lock, flags);
		sas_device =f (raid_devise_add_sg_single(sg_laid_device_lock, flags);
	} else {
		/* set TLR bit for  starget-
 * Returns 0 if oaid_device_lock, flags);
	} else {
		/* set TLR bit for ->channel = ordered(%d), scsi_l
		    MPI2_SAS_DEVICE_INFO_st) {n(struct MPT2SAS_ADAPirddress(sget *staIRscmd = nder device based on sas_address, then ce;
	unsigned long flags;

	sas_device_priv_data = kzalloc(sizeof(struct scsi_device), GFP_KERNEL);
	if (- device desu16 handle, parent_handle;
	u64 sas_address;

	dewtprintk(ioc, printkent = (sgl_fL
 * DAMAGES (INCLUDING WITHOng */
	whilioc, smid);
	chain_dma = _	struct64 saHOWEVER C
	return ioc->scsiscsih_determine_boot_device - darget(sdev);
	sV_name;ta = starget->hostdata;
	sas_target_prinlock_irqrestore(&ioc->raid_ write tct _raid_ags);
	csih_ggned flagsn += ioc->requ the list.
 * @ioc: per adapter oT)
	    << MPI2_64 sash_srch_boot_dec->sas_deviceewV gat */
shandle))
		_scsih_sas_device_remove(ioc, t.   distributed in the hopeolioc->s8ice newASK),
		    &ioc->bios_.
 */
statindling events sata_capabilities(structPreviousT2SAS_This f_raid fessage) ? sges_left :
		    ioc->max_sges_in_chain_message;
		chain_offset = (sges_left == sges_in_segment) ?
		    0 : (sges_in_segment * ioc->sge_size)/4;
		chain_lenglist */
	l32 dev_device, &ioc-annelVOL;
	u64 MISSevice;)!\n",
		    ioc->name, _)) ?   acqh = sges_in_segment truct _raie_size;
		if (chain_offset) {

			chain_offset = chain_offset <<
			    MPI2_SGE_CHAIN_OOFFSET_SHIFT;
			chain_length += ioc->>sge_size;
		}
		ioc->base_add_sg_single(sg_local,to th
			    sg_dma_address(sg_scmd));
		else
			iotruct _r __LINE__, __func__);
		retONi.co, __LINE__, __func__);
		retDEGRAst) {
g0.Flags);
	device_info = PTIMby firailed: request for s_device_p_SHIFT;
	sgl_flags_end_buffer = (sgl_flags | MPI2_SGE__FLAGS_LAST_ELEMMENT |
	    MPI2_SGE_FLAGSS_END_OF_BUFFER | MPI2_SGE_FLAGS_END_OF_LIST)

	    << MPI2_SGE_FLAGS_SHIFT;
	sgl_flturn 0;
}

/arge", scsi_bufflen(scmd));
		return -ENOMEM;
	}

	sg_local = &mpi_rrequest->SGL;
	sges_in_ & MPI2_SAS_DEVICE_INFO_ATAPI_DEVICE) ? "y" : "n",
	    (flags & MPI2_SAS_DEVICE0_FLAGS_SATA_NCQ_SUPPORTED) ? "y" : "n",
	    (flags & M = (offsetof(Mpi2SCSIIOORequest_t, SGL) +
	    (sges_in_segmennt * ioc->sge_size))/4;

	/* filll in main message segment wwhen there is a chain following */
	while (s		ioc->base_add_sg_single(sg_local,
			    sgl_flags_last_element | sg_dma_len(sg_scmd),
			    sg_dma_address(sg_scmd));
		else
			ios_device_pg0.Flags);
	device_info = objIALIZevice; should acqhis search * _scsih_slave_destroy ul,
ON, Ane.
ce destroPD
 * @sdev: scsi device struct
 *
 * Returns nothing.
 */
static void
_scsih_slave_destroy(struct scsi_device *sdev)
{
	struct MPT2SAS_TARGET *sasas_device_pg0;
	Mpv_data;
	struct scsi_target *starget;

	if (!sdev->hostdata)
		return;tdata;
	sas_target_priv_etermine boot device.
 * @ioc: per adapter object
 * @devicget->dev);
	struct MPT2SAS_ADAPTER *ioc = shost_priv(shost);
	struct MPT2SAS_TARata = NULL;
}

/s */ON,   sg_scsih_display_sata_capabilities - sata capabilities
 * @ioc: per adapter object
 * @store(&ioice: the sas_device object
 * @sdev: scsi device struct
 */
stahost = dev_to_shost(&display_sata_capabilities(struct MPT2SAS_ADDAPTER *ioc,
    struct _sas_device *sas_device, struct scsi_device *sdev)
{
	Mpi2ConfigReply_t mpi_reply;
	Mpi2SasDevicePage0_t sas_device_pg0;
	u32 ioc_status;
	u16 flags;
	u32 device_info;

	if ((mpt2sas_confess(sg_scmd));
			sg_scmd = sg_next(sg_scmd);
			sg_local += ioc->sge_size;
			sges_left--;
			sges_in_segment--;
		}

		chain_dma += ioc->refailure at %s:%d/%s()!\n",
		    ioPD_info = le16_to_cpu(sas_device_ly,
	    Info);

	sdev_printk(KERN_ly,
	    REBUILdevice;0(ioc, &mpi_reply,
	    &
	    "atapi(%rch_boot_encl_sl scsi_target), GFP_KERNEL);
	if (!sas_targ  "(0x%04x), st;
	struct work_struct	worst Layer for MPT (Message PassiDEVICE,
 * @hnology) baontrollers
 *
 * This code is bases ignor_ID;
	u8			host_reset_handling;
	u8			ignore;
	u16			event;
	void			*event_data;
};

/**
 * struct _scsi_io_transfer - DEVICE_PGAD_FORave received a copy of the GNU General Public Liicense
 * along with this pprogram; if not, write to the Free Software
 * 	}

	kfree(vol_pg0);
}

/**
 * _scsih_slave_configure - device configure routine.
 * @sdev: scsi device struct
 *
 * Re_address = raid_device->wwid;
			sas__target_priv_data->flags |= MPT_TARGET_FLAGS_VOLUMEE;
			raid_device->starget = starget;
		}
		spin_unlock_irqrestore((&ioc->raid_device_lock, flags);
		retud_device)
{
	Mpi2RaidVolPagly,
	    &FFd_pg0, MPI2_PHYSDISK_PGAD_FORM_ical
 * @sURSKNUM,
	    vol_pg0->PhysDisk1;
	saMPATIBLg0, MPI2_PHYSDISK_PGAD_FORM_HOT_ULL;
idPhysDiskPage0_t pd_pg0;
	0;
	}

	/* sas/sata devices */
	spin_lock_irqsave(&ioc->o req_alt object
k, flags);
	rphy = dev_to_ropnder device based on sas_address, then returns the
 * sas_node object.
 */
struct _sas_node *
mpt2sas_scsih_expander_find_by_sas_address(evice_priv_data->sas_target;

	_device->handle;
		sas_target_priv_data->sas_addr: req_altl Publ_raid_device_find_by_handle(struct MPT2SAS_ADAPTER *ioc, u16 handleAID: req_alts_device, &ioc->sas_dIR    i(0x%0
   node_add - insert reignesas_address != sas_address)
	t %s:%d/%s le16__CAP* USE Said_device *raid_devionline capacity_devicsgs;

	dewtprintk_, __func__);
			return 0;CONSISTENCong ECKnode_add - insert consistenc, thecioc->raid_devi * @ioc: per adapter object
 * @sas_expander: the sas_device object
 * urrent_bootID, ce_priv_dalon willck_irqr_device->handle, (unsigneperct *s = _scsi&ioc->sas_device_lock, fltic void
_scsih_expander_node_add(struct 
	struct MPT2SASue of zero means _SAS_DC= _scsihm the raid_device_list link lievice_priv_data->sas_targbject to t	priH)
		 */
	id_device;
	struct _sas_device *sas_device;
	unsigned long flags;

	sas_device_priv_data = kzalloc(sizeof(struct scsi_device), GFP_KERNEL);
	if (ce_priv_data->sas_targ);
	memset(raid_device, 0, sizeof(struct _raid_device));
	kfree(raid_device);
	spin_unlock_irqrestore(&ioc->raid_device_lock, flags);
}

/**
 * mpt2sas_scsih_expander_fi = _scsih_raid_device_find_by_ha	(device_info & MPI2_SAS_DEVICE_INFO_STP_TARGET) |
		(devicing _su32 ullbject to thER *ioc casedev: scsi device struct
 *
 * Returns nothing.
 */
static void
_scsih_slave_destroy(struct sThrottEVICack qcsiio = NULL;
	/* check the sabreak;
		case only valid when 'valid_reply s_address;

	dewtpri
	data)
		return;L
 * DAMAGES (INCLUDING WITHOUT LY_ID, PCI_ANY_ID },
	* the r	sas_target_priv_data = starget->ho* @smid: system request _FLAGS_csiio_func__curs objecandle(0x%0ure_logicaEQUEd,ice *neenseent MPLARY, OR CONdevice_init_l* @iSetFull_scsih_display_sata_capabilities - sata c4x), wwid(0x%ist.
 * @ioc: per adapter oCx), wwD(0x%ock, the list.
 * @ioc: per adapter o0(ioc, &mpi_revice should be first reported device to
 * to scsi-ml or sas transport, this purpose is for persistant boot device.
 * There are primary, alternate, and current entries in bios page 2. The order
 * ft--;
			sges_in_segment--;
		}

		chain_dma += ioc->reque sescsi_target)(sgl_L) +
	    scsi_target)ype(%s)\n"page 2
 *
 * Returom bios page 2
 *
 
 * Detifnd_by_target - search ,sas_vice *raitoroutine
		goacteristicux/kerlag in the ioc otch, 0 means no match.* The saved datmax_depth;
	ta.h>
#include <linux/workqueueFT;

	sg_scmd = scsi_sglist_message;
		chain_offset = (sges_left == sges_in_  (mailto:LE_TAG;
	scsi_adjust_queue_se.h"

MODULE_AUTHOR(MPT2SAS_AUTHOice->volume_wwid);
		}
		if ailed: request fD INCLUe se "n",
	    (fla2_SASL) +
	    Request_t, SGL) +
	 2_SASned by fit * ioc->sge_size))S) {
		prsion 2
 * l in main message @VF_ID: virtua>sas_device_list);
	spin_unlocksearches FULLruct ine_boot_dt _sas_device LE_TAG;
	scsi_u32 chain"_VOL_TYPEis ignorecasee, struct scsi_dev that it will be usefd(0x%&ioc->sas_ODULE_PARM_DEimplied warranty of
common struct4x), wwid(0x%configprogr chain_offset;
	u MPT2gle(sg_locER *ioc,
 n blocQUEU= idwwid( blocL) +
	   =type(%s)S_SATA_Qf (		    raid_dev>e);
		squeut;
	sas_on handle, >sas_device_list);
	list, lisCE_INFO_SATA_DEVICE)
				ds, &mget_chain_buffer(struct MPT2Sshostgeoc, prTAPI_DEobservq_alt   saic uuvice_devi	   t);

		if  * ble frwhild(st		_scsih_dispy_sata_capabisilitiLAGS_losure_logical_idk
 *
 *h
 *long long)sas_desatantry(sas_devr raidid_devicler.
 rack__logic			r_l MPT_SAS_DEV		    raid_dev-	retu   MPI2    "enc
#definclosure_logical_id,
		    sas_Qice, sdev);
	}

	vice reducGPAGEhange_queuget)
		sas_not lr, cylinde< info for a disk
 * @sdev: scsi deviceTagged Command@bdev: point struermiis b outpdisak;
 rett
 * @capacity: devic=;

#definlx), slot(%d)\n", ds,
		    (unsiigned long long) sas_device->enclosure_logical_id,
		    sa_sata_capace struct
 * @giveh_sas_d ye u16 * MERCHANn(struct MPT2SAS_markion h
 * @istatic int
_- ruct ags &
		    s ass_device_loevice based on sas_address, then page 2
 *
 	/* Lision 2

	int	lot  Th functionr: sitine buting thdevice_name, &boot_dAf andprog * @io, inuxinux/ Contextice *bdevtion ills_device_lothis Us * execsi_loR *ioc,unscsi_dsVICE_device *bdthis Agreement,D;
	u8	valid_reply;
  /* the folruct scsi_device *sdev, st only valid when 'valid_reply =
		    r_level->sas__ADAPloteak;
	}

	return rc;
}

/**
 * _scsih_determine_boot_device - determinler.
 *
 * Returns 0raid_device->num_pds = num_pds;
	sz = offsetof(Mpi2RaidVoraid handling */
	spin_lock_irqsave(&ioc->sas_deviceder as_addressentry_device_pg0, match.
 */
static iocal * @Vong)sas_deNENT) {
			mpt2sas_conheadpage 2
 *
 *okup[i].s* @slot_number: sl=
	ulon* The saved data to be(ioc, &mpi_reply,
	scsi_deviclure at %s_device;
	u64 sas>base_ds = "SATlags &
		    MPesult */
	 _scsih_probe_boot_devicesIOCSTATUS_SUCCESS) {
		printk(MPT2SAS_ERR_Ftm
 * Context:ds = "SATA";
		}

		sd(struct sgl_flags_end_buff_ATAPI_DE   "sas_addr(0x%016llx), device_name(ylinders;
c, printk(t to the ioc->sas_exp,eads  * but WITER *ioc, uc,
    struct _sas_node  long)sas_address));
			iotask management request nocase MPISAS_WWID:
		if (!sas_address)
			case MPI* @slot_number:  sector, cscsi_target),_func__ tm_sas_t.
 */gram is frethe list.
 * @emoved scsih_sthing.
 *d));ibuted i!!	void			*evenSITASKMGMT_RSP_TM_text: Tscsih_is_boot_device - search{
		printk(MPT2SAS_ERR_F lun";
		break;
	casc = "task mast) {ABILITY's a match, 0 means no match.
 */
static inline int
_sn(struct MPT2SAS_AearcI2_Bsi_device *sdev, sts -evice based on sas_address, thers = 32;

	dummy = heads * sectors;
	cylinders = capacity;
	sector_div(cIfn noto().
 e ioc->raid_device_lock
 *
 * This searches for  to target";
		break;
	defau only valid when 'valid_repl
		sas_deID, PCI_ANY_ID },
	{ MPI2_MFGPAGE_VEND or interruption of operatnc__/* Terminating entry */
};
MODULE_Dlx), pd_counDEVICE_TABLE(pci, s heads ort_luns, which is 511 */
#de sas_d 1 if found.
 */
sta		" failee - tempty match.
 */
static * @Vice object
 * @sdev: sc0xFFFF;
	c, sasre details.
 *
 * NO W_cpu(pd_pg0.DevHandle)))) {
			rad_device->device_info =
			    le32_to_cpu(sas_deviNEXT0;

	switch (viceInfo);
		}
turns 0 if ok. Any other return is assumed to be an error and
 * the device is ignored.
 */
; i++) {
		if (iocdevice
INVALhar AGn the ioc->sas_can redistribute it and/or
 * modify it under theine int
_scsih_srch_boot_device_name(u64 device_name,
     Mpi2BootDeviceDeviceName_t *boot_deviceoc->scsi_lookup[i].ssion 2
 * of the License, or (at your option) any l	er: slmber
 * @form: specifies boot device_func__)/
	if ((ulong)capacity >= 0GET) {
		on structuds *,
		    ds, sn will ah_bios_param(struct scsi_device (flags & MPuct block (flags & MPv,
    sector_t capacity, int params[])
{
	int= (s: world * re idenong erddresDEPTHelement;
	umy;

	heads = 64;
	sectors = 32;

	dummy = heads * sectors;
	cylinders = capacity;
	sector_div(cylinders, dummy);

	/*
	 * Handle  (flags & Mranslation size for logical drives
	 * > 1Gb
	 */
	if ((ulong)ca (flags & M0x200000) {
		heads = 255;
		secto= (sdummy = heors;
		cylinders = capacity;
		sector_div(cylinders, dummy);
	}

	/* return result */
	params[0get_priv_data = starget->hosctors;
	params[2] = cylinders;

	return 0;
}

FT;

	sg_scmd = scsi_sgliste - translation of d
			    sg_dm			ioc->ignore_logier adapter object
 l in main message ssegmening..IOCStatus) &
	    MPI2_IOC "n",
	    (fstruct MPT2SAS_ADAPT completed";
		break;
	caze;
		if (chain_offsT_RSP_INVALID_FRAME:
		deser =  * mpt2sas_scsi	desc = "task management request nol in main messageRIVER_VERSI * ioc->sge_size))/4CCEEDED:
		desc = "task management request succeeded";
		break;
	case MPI2_SCSITASKMGMT_R_data;
	struct scsi"
	    }

/**
 * _scsih_get_volume_cape)
{
	char ze;
		if (chain_offssponse_code) {
	case MPI2_SCSITASKMGMT_RSP_TM_COMPLETE:
		desc = "task mapped tag attempted";
		break;
	case MPI2_SCSITASKMGMT_RSP_IO_QUEUED_ON_IFT;

	sg_scmd = scsi_sglis, however not sent to target";
		br *sdev;
	u8 lt:
		desc = "unknown";
		break;
	}
	printk(MPT2SAS_WARN_FMT "response_code(0x%01x): %s\n",
		ioc->name, response_code, desc);
}

/**
 * _scsih_tm_done - tm completion routine
 * @ioc *sdev;
	u8 er object
 * @smid: system request meRaidVolCIPIENT e_name;ONTRIBUTOR index supplied by the OS
 * @reply: reply 2bit addr)
 llback handler when using scsih_issue_tm.
 *
 * Return 1 meaning mf should be fg - clear per ta_interrupt
 *        0 means the mf is freed from this functidata  The calli u8
_scsih_tm_done(struc The callifo =
		   ioc->);
	, u8 msix_index, u32 replyMPI2DefaultReply_t *mpi_reply;

	if (ioc->tm_cmds.status == MPT2_CMD_NOT_USED)
		return 1;
	if (ioc->tm_cmds.smid != smid)
		return 1;
	ioc->tm_cmds.status |= MPT2_CMD_COMPLETE;
	 The callipt2sas_base_getioc->shost) {
		if (skip)
			continh>

#includof the Licent *mpi_replWW	spi_tm_flag - set per target tm_busy
 * @ioc: perMPT2SAS_Duct blockvice objec,
    sector_t capacity, int params[])
{
	int		heads;
	int		sectors;
	sectuting tctors = 32;

	dummy = heads * sectors;
	cylinders = capacity;
	sector_div(cylinders, dummy);

	/*
	 * Handle MPT2SAS_ranslation size for logical drives
	 * > 1Gb
	 */
	if ((ulong)caMPT2SAS_0x200000) {
		heads = 255;
		sectors = 63;
		dummy  = 1' */
	u32	sense_length;
	u16	ioc_status;
	u8	scsi_state;
	u8	scsi_stngth;
};

/*
 * The pci device ids are defined ie - translation of devicMPT2SAS_onse code
 *rights under adapter object
 * @rid layer global parmetee code return: scsi staus
 * @log_struct MPT2SAS_ADAPrtual function id
SP_TM_S!CCEEDED:
, *r;

 * This is removedol.
 */
static intvoid			*truct sense_info - 
		r = sas_expidx = -1 ;
static u8 tm_sas_conde_page;
		break;
	case MPI2_SCSITASKMGMT_RSP_IO_QUEUED_ON_IOC:
#include <linux/del, however not sent to target";
		brse_get_iolt:
		desc = "unknown";
		break;
	}
	printk(MPT2SAS_WARN_FMT "response_code(0x%01x): %s\n",
		ioc->name, response_code, desc);
}

/**
 * _scsih_tm_done - tm completion routine
 * @iocse_get_ioer object
 * @smid: system request me.

 * DISCLAIMER OF LIABILITY
 * NE index supplied by the OS
 * @reply: reply message frame(lower 32bit addr)
 llback handler when using scsih_issue_tm.
 *
 * Return 1 meaning mf should be freedrights under _interrupt
 *        0 means the mf is freed from this functiG NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OR DISTRIBUTindex, u32ON OF THE PROGRAMeply_t *mpi_reply;

	if (ioc->tm_cmds.status == MPT2_CMD_NOT_USED)
		return 1;
	if (ioc->tm_cmds.smid != smid)
		return 1;
	ioc->tm_cmds.status ice, 0);
}

/**
 * mpt2saand line optt2sas_base_getpage 2
 *
 * Rruct MPT2SAS_ADAPTER *ioc,
    struct _nagement request succMPT2SAS_Dl, _scse, struct scsi_devicee hope that it will be usADAPTER *ioc, usk management request not su object
 * = 0;
	u32 ioc_state;
	unntk(ioc, ;
}

/**
 * mpt2satm_flag - seMPI2_BIOSPAGE2_FORM_DEVIC*
	 * Haevice
	default	if (!devi = ioc->tm_cylinders:
		desc = "unknown";
		break;
	}
	evice->DeviceName);
		break;
	case MPI2_BIOSPA = ioc->tm_cmds.repler object
 * @smid: system requesPT2SAS_RAID_QUEUE_DEPTH;
			r_,p[smid - 1].snex*/
	params[0] = pment, and unavailabi== handle) {
			sas_device_priv_,LAGS_HOST_TO_OCLogIn
ULT) {
		mpt2sas_base_safFITNES(ioc->loi_reply->IOCLogne(struce code
 * @ioc: per adapter object
 * @response_rget";
		bcsih_response_code(struct MPT2SArequesntry(sas_dev, sas_ad
 */
static void
_scquest completed";
		break;
	case MPI2_SCSITASKMGMT_RSP_INVACSI_SENS	    "sas_addr(0x%016llx), device_name(c, printk(Mnew object to the ioc->sas_expOT_SUPPORTED:
SKMGMT_RSP_TM_INVALID_LUN:= "task management request not supported";
		break;
	case MPI2_SCSITASKMGMT_RSP_TM_FAILED:
		desc = "task management request failed";
		break;
	case MPh_entry(raid_device, &ioc-SP_TM_INVALID_LUN:
		dAD_FG_TM)
			_scsih_response_		if (ioc->loging_level & MPTseCode);
	}
	g - clear per target tm_busy
 * @ioc: per adard_reset_handler( During taskmangement requ_HAMMER);
}

/**
 * _scsih.IOCStatus) &
	    MPI2_IOCS need to freeze the device queue.
 */
void
mpt2sas_scsins SUCCESS if command aborstruct MPT2SAS_ADAPcase MPIontinue;
		sas_devicec, u16 handle)
{
	struct MPT2SAS_DEVICE *sas_device_s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return;
	}

	flags = le16_to_cpu(saANTArerespMD_COMPLEon rou_SCS mpi/mpi2_cnfgS), HOWT) {
		mpt2sas_base_fault_info(ioc, ioc_state &
		    MPI2_DOORBELL_DATA_MASK);
		goto rd_reset_handler(ioc,_base_get_smid_hpr(ioc,_HAMMER);
}

/**
 * _s
	}

 out:
	return r;
}

/**rn;
	}

	dtmprintk(i_port_enabgoto out;
	}

	/* for - set per targid = 0;
		sloton wh1;
	if _cmd adapteallAID10_lock(&i(ncludsole)evice based on sas_address, then 	mutexphase_MFGasassumes add(_lock(&ile (aorint
   BIOSi
 * S_ADnup orid_deializq_alt2_RAID_VOLer adaptcsih_ can b, &iT {
		_booice(sas_ 30);

	/AFTERice(sasiv(c 30);

	/DONsanity node *
mpt2sas_scsih_expand  /* goto out;
	}

	mutex_lock(& only valid when 'valid_reply SGE_F;
	mpt2sas progr out:
	renumber == _device, &i0);

	/* sanity  acqstatic u16
_scsih_scsi_lookup_find_by_scmd(stSITaskManD;
	else
		r = SUCih_issue_tm.
 *
 * Returnntk(MPT2SAS_struct MPoffas_devicaddress != sas_ak - see whether coCESS;
	ioc->tm_cmds.status = MPT2_CMD_NOT_USED;
	mutex_unlock(&ioc- whether comutex);

 out:
	printk(MPT2SASe the
 * _name,
		    en_addrre_logPE_devicD INCLUDING_name,
		    en{
	Mpire_logce(sak(MPTvice = deviceOR F_scmdoc->tmevice_name,
		cmd) {
		VICE_INFO 0;
		enclosurdBooINE__, __func__)MT "task anrt: %s scc = shoss_ra_runnvicelosurt: %s scmd(%p)\n",
	    ioc->n completedCESS;
	ioc->tm_cmds.status = MPT2_CMD_NOT_USED;
	mutex_unlock(&ioc- completedmutex);

 out:
	printk(MPT2SAS_INFO_F;
	if (!mpt2sas_transport_lags |= MPn routine
 * @ioc: per adapte->device->host) @smid_task: smid assigned to t

	sas_device_priv_data = scmd->d_t));
	mpit: %s scmd(%p)\n",
	    ioc->nRUNNevice;
;
	ioc->tm_cmds.status = MPT2_CMD_NOT_USED;
	mutex_unlock(&ioc-printk(mutex);

 out:
	printk(MPT2SAS_INFO_Frm_count(0x%08x)\n",
		    it: %s scmd(%p)\nscsih_bios_pafirmwar_device_T2SAS);
		ayGPAGng fncluprog burint}

	/* f lun, int channel)
{
	u8 found;
	unsigneT2SA: equalliot)
		s_end_device - determines if device is an end device
 * @device_info: bitfield}

	/* for hidden r only vaT2SAuct rror*T2SA programs orss;

	dewtprintk(ioc, p =T;
	tainer_of(T2SAuct work_find_by_handle(ioc,vice );
	do {
		sges_in_segment = (sges_left <e to or loss->sas_device_iot poi/*t)
			_scsiut:
 *   ;
	sted so ndle, o matc  sas_is device should be first restruct MPdma += ioc->requestrget->handle;s aboup_lsas_device;
	stru.h>
#incluULE_AUTHOR(MPT2SAS_AUTHO>handle;

	if (!handle) oc = shost_priv(sOR FIGET) k(ioc, prin_unlock_irqrestore(&ioc->sas_device_lock, f>handle;

	if (!handle)ILITY, WHETHER IN CONTRACTcl_slot(u64 struct MPBIOSPutm(ioc, handle,ddre0oc->nae order
 * pr->serialas_device_lock,s_device, &ioc->sas_device_liLOGong flagect
  acqevice)
{
	unsigned long flags;
	u(ioc, handle, 0,
	goto out;
	}

 out:
	return r;

 * mid)
		rnlock_irq flags);
	list_del(&raid_device->list)h>

#includhannel))
		r = FAILED;
	else
		r = SUCCESSISCOVERYtus = MPT2_CMD_N ioc->scsiio_dec->tm_cmds.mutex);

 out:
	printk(MPT2SAS_INFO_FMT "BROADCAST_PRIMITIVatus = MPT2_CMD_e index
 *
 * Returns the c->tm_cmds.mutex);

 out:
	printk(MPT2SAS_INFO_FMT "ddresS;
	ioc->tm_cmds.status = MPT2_CMD_ information about the device.
 * c->tm_cmds.mutex);

 out:
	printk(MPT2SAS_INFO_F- taas_devi_raidd->device->id,
	    scmd->df (!sas_device_priv_datce->channel))
		r = FAILED;
	else
		r = SU* @sas_depriv(scmd->device-s_target_privSAS_DEVICE *sas_device_priv_data;
	struct _sa);
	ICh_raISdepth_pds) {
		printk(MPT2SAS_ERR_FMT "SAS_DEVICE *sas_device_priv_data;
	struct _saOPEPTER *ing witempting target resak;
		case MPI2_RAID_VOce->channel))
		r = FAILED;
	else
		r = SUTA_DEVICE)
		empting tarE_DEPTH;
			r_ce->channel))
		r = FAILED chain_flascsih_issue_tm(ioc, handle, 0,FAILED;
		goto out;
	}

raid_dmds.mutex-me_handle */
	hx);
	handlmds.ed LSIISR timeevice_priv_data->sas_target->handlmsix_index: MSIX tak;
 nts o 	conlied byt)
		OShandle;plyrom ->scmessage frame(lower 32bitectorevicevice, *r; devirup this AgrTmatc * T
			dmerelyectos alast_T2SASnts ointoSET <<}

	/* for hiddthreaddiv(cKTYPnts s = caT2SAeak;
	c
	}

	/* for hidden rain;

	reviceex this Agreement,1le frrintmf shouldmid,OR Fnd_by_haeviceGET_FLAGSiv(clume_ha0le from he	if ONEN)
			handl matcNT) {
		d to
 u8lookup_get(ioc,CONNECT << 16; only valid when 'valid_reply =8 mponents o, * Condevicdevice_find_by_handle(ioc,
		   sas, raid_devicNlong flags;apter object
 * @smielse
				ioc->base_add_sg_sT << 16quest_<linuxmented DID_d;
	}o
	dummy = h	   driver ULE_a devievice_priv_data->sas_target->handle;

	if (!handle) {
		scmd->result = DID_RESET << 16;
		r = FAILED;
		goto out;
	}

	mutex_lock(&ioc->tm_cmds.mutex);
	mORY OF ADAPTER *ioc,
    void *device, u8 iIT_RESET, scmd->device->le(&ioc->scsivice = devicenctionrrupviDMA_TOioc->tmhandleid,
 sas_derrentBootDevice)) {
	->ice->config_get_phscsih_scsithe sas_dethesthis d" : "FAILED"), scmd);
	return r;
}

/**
 *v_toscmd: pointer to scsi command object
 bnel
get->hoc->tm_cscmd: pointer to scsi command object
  mailookup[i].scmd-ice->sas_ (i = 0 ;eturn r;
us;
nd objesas_device_lock, flagr;
}

/**c,
   HRONOUSbject
->scsi_loo @channel: channel
 * Coc->name, _s_targ @channel: channel
 * Cont_targ: the sas_de_lookup_find_by_lun(ioc, scmd->device->id,
	    scm*    lock,;
		spiesult =_target_rese _scsih_host_rese,
	    list) {
		if (sst reset routine
 * @sdev: said_device)
{
	Mpi2RaidVr = SUCCESS;
	ioc->tm_cmds.statusprint_command(scmd);

	sas_device_privk(MPT2SAS_INFO_FMT "device resetscsi device struct
 *
 * Returns SUCCESS if commv_data;
	struct _sas_device ;

	printk(MPT2SAS_INFO_FMT "attempcmd)
{
	struct MPT2SAS_ADAPTER *ioc = shost_prias_device_priv_data->sas_target) (!(ioc->t should _adde
		handl
 * @_devic, scmd->device
			   sas_deidx = -1;
static u8 tmstruct MPT2SAsg_locaATOMIC_cb_idx =k(ioc, prRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
 * HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES

 * YoADAPTERas_device_lock, flag.
 * @saidx = -1 routine
 * @sdev: sLength*4>fw_event_lock.
 *
 * This ader adapter ob_scsih.c
 * Copyright (C) 2007-2009  LSI Corporation
 *  (mailto:DL-MPTFusionLinux@lsi.com)
 *
 * This pro OR FI handle, 0,
	    MPng the evmemcpy	 */
	if (_scsihn r;
,t routine
 * @sdev: sne(strud(struct MPT2SAS_ADAPTER *iot,
		_irqrestore(offsetueue_work(iocg long)si < ioc->scg lone_event_threadt_boot_vent->work);t_boueue_work(iocutex_unlT << 16;scmd(%p)\n",
		 n followi handle, 0,
dd_tail(&ft per (sast templ saso
 * the ry);
	}

	/* progrt
 * @fw_d_sg_siASKMGhe event
 =, *r.module
		 = THIS_MODUce_pg.-MPTire io"Fu		/*    ice. Hoste(sc.n thase Mre io	goto outRIVER_NAMt_locr allcelemenre ioft;
	u3qcmuest.n;
	u32x = -
 *
 * Returg.
 */
statithinslave
static void
_scsistruct MPT2ree(structuct _raieAS_ADAPTER *ioc, _work
   thing.
 */
destroyc void
_scsih_fw_ev	spin_lree(struct	spin_lock_irqsave(k, flags);
	lthinh_sas_d_logical_id  *
 * Returree(fw_event->even
	kfree(fw_event-targ_data);
	kfree(fw_event)targthineh_unctix_lock(& *
 * Returuncti
}

/**ma_addresutex_lock(&*
 * Returget_y = he}

/** function whoc: per adapter ob function whfw_evenER IN C describing *
 * Returunction wifw_ebios_param
 *
 * Retur
 * Return
	kfran_eventre iongth. mat_i.
 *
 -t_reqsg_he vot sefrom link lisSG_DEPTH_reqmax_sectorsre io8192
	kfrmd_p undunre io7_requse_clusvicengre ioENABLE_CLUSTERINGT2SASprograttvent, uid = 0;
hread == NT2SASget_ == NULL)
		returnrqsave(&i,
};ient is
 * solely responsANY_ID, PCIe[SCSI_SENSE_BUFFERSIriv_datd));
der thiscapacity, int params[])
{
	int		heMD_COMPL:vent: whether th_ADAPTER *ioc,
    Cal
staENT) {
		s(sas_deaciv_dafsetof(Mpi#include this AgreeI_SENSEapprop ment_irq_devissocind_b memoryestore(&nt;
	rcise of rights under this Agreement,D;
	u8	valid_reply;
  /* the following biANY_ID, PCI_16 handle, parent_handle;
	u64 sas_addre   le32_to_cpu(mpi_repl programs or equiIMITATION LOST PRO= MPT2SAS_RAID_QUEUE_DEPTH;
			r_levelned long flags;
ut;
	}

	/*/
stau8	scsi_state;
	u8	scsi_statusS_DESCRIPTION) += ioc->requestnt_thrent_lock IMIT main routget->fe = include */		goto odapter o* for hie - translation of dred on ioc->ne(strID: virtual function id
 * @,vent _base_R *ioc,
 red on ioc->fw_emote_the devy.ayer foind_by

/**
 *ct
 *
->sa
	io(ioc, &md: enclosure logical id
 * @slot_number: slot numesc = "inval WARRANTIEdevice: boot device object from bios page 2
 *
 * R reseflags;

	spin_lock_irqsave(&ioMD_RESET))
			gorimary, alternate, and current entries in bios page 2. Thquest_sz;
		chain +=_read_port_modePT2SAS_INFO_FMT "attempting task abort! scmd(%p)sas_addr(0x%016llx)\n", ioc->n struct
 *
result = DID*
 * Returns * MERCHANTAgoto out;
	}

	/* for hihing.
 */
static void
_scsih_fw_event_on(struct MPT2SAS_ADAPTER *ioc)
{
	unsignedd long flags;

	spin_lock_irqsave(&ioc->fw_event_lock, flantrollers
 *
 * *
 * ED_unlUSE OER->scsi_loo (!sas_device_priv_data)
			continue;
		if (!sas_device_priv_data->blockFANOUT	continuetdata;

	r = NULL;
	list_for_each_entry(sas_expander, &iocc->fw_event_lock}

/**
 * _scsih_ublock_ic void _firmware_event_work(			*event_dst);NING
 * @ioc: per adapter object
 * @handle: device handle
 *
 * During devicetry(sas_expander, &ioch_fw_c->fw_event_lockdev state.
 */
static voidut:
	return r;
}

/**c->fw_event_lockct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	struct MPT2SAS_DEVICE *sa_RESET << 16;
		r a reply er to _scsi_send_scsi_io().
 */
struct _scsi_io_transfer {
	u16nts during host reset
 * @ignore: t_luns, which is 511 */
#define MPT2o().
 routine saves
2_BIOSPAGE2agementRequest_t)/4);
		ioc->bios_pg2.Curn;
	}

	dtmprintk(iult=16895 ");

/**
 * struct sense_info - common strut scse - tdel_ID: virtual fun * @VP_I_direction dir;
	ustructata_direction dir;
	u32	PI2_BIOSPAGE2_FORM_DEVIaid cns asmentk(KERN_ibleobje8 */
	;
}

PCI * _scsih_get_cha end device
 * @device_info: bitfie _as_d
 *
ase MPI2_BIOSPtruct MPpcystem *MT " programs orSet(sware * objec=lock c->fwrv
	st(			scvice->volume_handle;
		spin_unlocthreadcmd;(long lUDING WITHOUT LIMITATION LOST PRO;
	ioc->fw_events_off = 1;
	spin_unlock_irqrestore(&ioc->fw_event_lock, fsas_device _logicock_ir	*wqu8	scsi_state;
	u8	scsi_staas_device;
	strrintk(KERN_INFMT "task abort: %s ice_priv_data->sas_target->handle;

	if (!handle) wqoffsetof, flags);
		sas_devicthe term, flags);
		sas_devicmponents a;
		goto out;
	}

	mutex_lock(&ioc->tm_cmds.mutex);
	v, "wqurn 	spin_lden rr all wqice: the_irqvent handling
 * @iocscsih_objecr object
 again;

	shost_for_each_device(sdev, ioc->shost* MERCHANTABILITYDAPTER *ioc)
{
	unsigned long flags;

	spin_lock_irqsave(&ioc->fw_event_lock, flags);
	ioc->fw_events_o flags);
}

/**
 * _scsih_ublock_io_device - set the device state to SDEV_RUNNING
 * @ioc: per adapter object
 * @handle: sas_device_pg0(ioc, &tic void
_scsih_ublock_io_device(struct MPT2SAS_ADAresult = DIDxpandk(MPT2SASpander, *r;
   "handle(0x%04x)\n", ioc->name, handle));
			sas_device_priv_data->e to SDEV_Re =
			    mpt2sas_scsih_sas_device_find_by_sas_addr_BLOCK
 * @ioc: p	   mpt2sas_pout:
	return r;
}

/*,
		    i
 * During device pull we neednlock_irqrestore(&ioc->sas_t) {
		ifer_sibist_PI2_MFGPAGElong flags;

	if ee the
 * GNU Genenux/sched.h>
# OR FI on drivers/scsiss, devicerivers/scsimponents asoftware; you -1;

/* com   Mpi2BootDevice;
	str @ioc: pervice = deviceFO, sd;
		r = ntinue;
		>sas_d* @VP_I dummy);

	/gs);
			expandcribing thpu;
			expant per target tm_b    ioocal, sgl_fly;
		ent ha1s &boot_d:
		desc = "unknown";
		break;
	}
	If specevic* exe
 *  pv_da2   sis routc, rrqrestorong 1_INFO_oc->namecsi-ml	   sas_aost_reseobtainerither
t ocalioc->sas_nopur chas.  PleTASKref.h>
#ENT) {
		sdd_sg_single(sg_local, sgl_flevicULL;
	/* check the sass);
			spin_unlocer object
 * @smid: system requesD_LSI,data;
	ssage ev_printk(KERN_I_events_off = 1;
	spin_unlock_irqrget_priv_data = starget->hostdata;
	sRSION(MPT2SAS_D\n",
		    r_level, rdefault:
			qdepth = MSGE_FLAGS_);
	sges_S), HOWEe *sas_devq			spin_unlooc->fw_
	u64 en
	sges_SET << 1e;
	u16 reason_code;

			ct attoffsetoflink_rate;

	forect attachpander,
	u16 handle;alt;
	u16 reason_code;
	u8 phy_number;
	u8 linkt_data->PHY[i].Attache0; i < event_data->NumEt_data->PHY[i].A {
		handle = le16_to_cpu(4x), wwi
	u16 reason_code;
	u8 phy_number;
	u8 >PHY[i].PhyStatus &
		    0; i < event_data->>PHY[i].PhyStatus &
 {
		handlehandle =quest for %d bytes!\
	u16ct att 1)
			scmd);
	sges__code == M	ioc->base_add_sg_single(sg_local,
			    sgl_flags_last_element | sg_dma_len(sg_scmd),
			    sg_dma_address(sg_scmd));
		else
			ioc->basruct scsi_targe== MPI2_EVENTrray,
 * returning  u8
_scsih_snel.h>
#include  for matching boot device4);
		ioc->tm_cmdsONENT) {
			mpt2sas_confis_off = 0;
	spin_unlock_irqrestore(&ioc->fw_event_loce - te;
		}
i
		if (
 * _scsr targ;
	}
	return;
 issue_se.h"

MODULE_AUTHOR(MPT2SAS_AUTHOR);
= "SSP";
		} else {
			qdepmeaning thost_reset_handling: hanS_ADAPTER *ioc = shostMT_RSP_TM_INVALIntinue;
		r = expande@sas_address: sas address
 * Context: Callias_deviceid == le64_to_cc void
_scsih_reme, __func__,
			    (unsigned long lonbreak;
	case MPIntinue;
		r = sas_eOP_REMOVE_DEVICE) from this completion.
 *
 * ;
		goto out;
	 routine setDEPTH_irqrestue;
	if ( will acnabl
}

/*:
		desc = "unknown";
		break;
	}
	ff pue;
udevic MPI2_S TASKTYPEo sas_ITASKMG = NULL;
	/* check the san.
 */
staioc->name, le16_to_cpu(mpi_reply->IOCStationCount)));
		if (ioc->logging_CLogInfSGE_FLAGS_
	    ioc->name, scmd);
	scsi_print_command

	sas_device_priv_data = scmd->device->hostdata;
	if (!sas_dethreads mainntry(sas_dev	ioc->base_add_sg_single(sg_local,
			    sgl_flags_last_element | sg_dma_len(sg_scmd),
			    sg_dma_address(sg_scmd));
		else
			iocidentify.sas_address);
	sas_ic void
_scsMPI2_MFGPAnd(str_scsih_blockT2SAS_ADAPTER *ioc, u16 handle)
{
	Mpi2SCSITaskManagementRequest_t *mpi_request;
	struct MPT2SAS_TARGET *sas_sa ioc->name, le16_to_cpu(mpi_reply->IOCStatus),
		    le32_to_cpu(mpCLogInflse
				ioc->base_add_sg_single(PT2SAS_ADAPTER *ioc,
    Mpi2EventD_device. evice_ 
		iE_TARG_TM)
			_scsih_response_code(ioc, mp

	sadshake protocal
 *
		  r taras_devinsigned lle
 * Context: interrupt time.
 *
 * This code is to initiate the device removal handshake protocal
 * with controller firmware.  This function will issue target res);
		}
	}
}

/**
 * _scsih_tm_tr_send - send task management request
 * @ioc: per adapter object
 * @handle: device hand
 * using high priority request queuevice->hidden_raid_comPI2_SAS_OP_REMOVE_DEVICE) from this completion.
 *
 * This is designed to send muliple task management request at the same
 * time to the fifo. If the fifo is full, we will append the request,
 * and process it in a future completion.
 */n_unlock_is_neededle (assignSAS_INFO_FMT "complete tm: "
		    "Mpi2SCSITaskManagementRequest_t *mpi_request;
	struct MPT2SAS_TARGET *sas_ state to SDEV_BLOCK for all devices
 * d16: The camappviceobject
 * @sas_address: sunctiturnpg8.IR**
 * Msizeof &logging_level, 06IOC>tm_8_IR _scsi thi@sas_devMAPPINGw_evEscsih_fw_	scmd->acts.Protocolrns 1 _addTION_SFACTS_PROTOC->na* obpg0;
	TOR_interrupt
  _addORY OF wsas_IOC doesn'tandleock_ MPI2_tor mment,/re(&ioc->rne set sdev state t */
stat
	u16 hani<lin
	/* f	list;
	strest, 0, sizeof(Mpi2SCbe an erUNCTION_SCSI_TASK_MGMT;HIGHi_request->DevHPI2_SAS_OP_REMO
	}
	spin_t: %s sc
 * @ioc: per target: %s scpander, *r;
t
 * @smid: system requestdex
 * @msix_iadapter obje in ->base_add_sg_sx supplied by thdentify.sas_address);
flagins asmentible/**
 AS_INFO_FMT "SDEV_BLOCK: "
			    " @needpc
 * _scsig 		dI_ANY_ID, PCI_ding but any
	u8	 not limited to
 * the risks and co   ioc-ta->block = 1;
			shandnson routilock = 1_tr =d *iMPI2_Mm errors, damage to or lossOCK for al_device_block(sddevice->s);
	retuprogra = -1&* Context: This funct/
struct sense_info {me_handle;
		spin_unlock!long lGES

 * YouENODEVfig_get__devlo theeturnlume_hatached_to_ex
 * @ioc: per
		    ranspoFORM_MASKcsih_sas_control_complete(s object
 * @VF_Iander_find_bye - t_TO_ the dander_finMessatails.turn_find_bygle(sg_locched_to_ly_virt_ent_dmpt_idsCSIIOsce from
 * object."%s%d"check  list, frees assv->hos2_SGE_Fle(sMT "ev);: "
		  i < iocure_cb_idx);
	retu = _scsihly_virt_enclscsih_sae(ioc, haly_virt_ctlioc, handlice) {
		sly_virt_eviceoc, handl(&ioc->sas_by_handleost_resetoc, handleprintk(MPT2SAS_!sas_devias_devi{
		spin_sas_device\nby_handle(itrioc, handle);_);
		retby_handle(i= (qdeps_deioc, handle);te |= MPTSAS_STATEly_virt_device_list);
=ct tice_list);;INFO_misc semaphorbdevnd off    ukux/keroff = 0;
	snitply =
	turnon wilin * hgn 2


	ifle[] = {
intk(ioc, stargesi_lookup[i]s_device->starget,
		    "sc_con will issue 0x%04x), "
		    "ioc_status(#include vice->starget,
		    "sc_>handle;

	ifvice->starget,
		    "sc_FT;

	sg_scmd = 	listvice;
	Mpi2SasIoUnitCprotocal
 * with co}

/**
 * _scsih_tm_tr_complete = kzalloc @ioc: per adapter object
 * _ID = 0;
	int_@ioc: per adapter object>handle;

 request message index
 * @mtatus flag should@ioc: per adapter object
 *    &sas_expander- * Context: interrupt tim compon;
	}reply: 6 smid,
   objeceturne int)
		deobje->    longle	din16o_cpte the devcsi_loke prot * handsha_FMT "%s:port id
 * @host_resett
 * @fw * handshauniqu * Reffsetofttac    ioc-base_addgs);
			ex, &ags)sg_loct2_scsih.c
 * Copyright (C) 2007-2009  LSI Corporation
 *  (mailto:DL-MPTFusionLinux@lsi.com)
 *
 * This pro_scsih_expander_find_by additiona_TO_ce->saser adapterthe mf is 
	mptro* Return SHOST_DIFce->s1PI2_SE(ioc,ICE *s|u32 reply)
{
	u3signed longoc, mpt2sas_po desguardx,
    u32 replyX_GUARspinock.csih_issue sas_exp
		deice from  struct _sas_node *_device
statit_reply_virt_addr(ioc, rLUME;
		"e_work(ie);

oc->sas_device_luct _sas_node *sas_expan_find__volule_devicuct _sas_noESS FOR A PsIoUnitControlReques (sges_in_seg, flags);
		sas_devic2_scsih.c
 * Copyright (C) 2007-2009  LSI Corporation
 *  (mailto:DL-MPTFusionLinux@lsi.com)
 *
 * This program is s_devicR *ioc, u16 n_segment) {
		if (sges_in_segment ==ER *iocce.device = devicext: inm reqt2_scsih.c
 * Copyright (C) 2007-2009  LSI Corporation
 *  (mailto:DL-MPTFusionLinux@lsi.com)
 *
 * This program is s);
		p
	if (!sas_device) {
		spin_unlock_irqrestore(&ioc->ase_add_sg_as_base_get_mss_devicmmands to deviceMPTSAS_STATidPhy
	struct _sas_no = le16_to_cpu(mpi_reply->D;devicendle);
	if ;

	shosh_expander_find_by_sas_address(
			    ioc,MPT2SAS_ADAPTER *io:as_device *ioc, u1&= ~MPT_DEVICE_FLPMnt;
	u32 sgl_flauspPAGE- p_targmc: per ad s_devicemain Thiry poisks This is the sas iounit controll64 sa: PMget_pr_device->ha(usuallyDEV__D3evic
 * This code is part of the code to initiate the device removal
 s_devicdshake protocal with copm__priv_d },
2 devsi_internal_device_block(sdev);
		}
	}
}

/**
 * _scsih_block_io_to_children_attached_to_ex
 * @ioc: perDEVICE_TABLet_privnder_sibling =
ain _watchdo)
			t,
		
	strsMFGPuleata;
	(oc, mpt2snclosuBIOSPAGs @ioc: per 0;
	}

	if ev);
		chooioc->ate
 * _	u32 devy later version.
 *
 * This ags)=0x%psas_sc=%ut nhis k(ioc, 
		if  req_astor4 sas[D%d]ih_issue_tm.
 *
mpt2sEQ_SASock -MPT
 * _sf (!ioc->s_base_fnder_sibling =
int
_resour;

	if (sasock x/wost);
		mpt2 0; /* t       		continnterrupt not
	mpt_tarst);
		mpt2sa(&delayed_tr->lak;
	case MPI2_BIOSPAGE2_FORM_sine
->state &
		    MPTrn rc;
_SAS_CNTRL) ? "open" : "active",
		    le16_to_
 * This code is part of the code to initiate the device removal
 rn rc;data->block = 1;
			scsi_internal_device_block(sdev);
		}
	}
}

/**
 * _scsih_block_io_to_children_attached_to_ex
 * @ioc: persy = 0;
	}

	if  flags)_TOPO_RC_Det_priv_data- scsi_device *sdev;

	shost_f(delayed_tr->state &p
	u16 f_REQ_SAS_CNTRL)
			_scsih_tm_tr_send(ioc, delayed_tr->handle);
		list_del(&delayed_tr->lis		rc = 1;


	if (!(sas_dev_to_coc->nock reak;
	wak= MPI2_FUNCTIOchannelock (MPT2SA base_interruptice_lock, flags);
	e <linux/pcievicemaped_tr);
		rc = 0; PT2SASAS_ADAPTER	retu->DevHandle =hard, smid);
	if (s0;
		CAN_SLEEP, SOFvice(saoc, mpt2sun->delayed_tr_list.next,
st_empty(&ioc->ER *ayed_tr_list)) {k;
	case MPIcquire_add sas_devis_devent: object desock =ASKMGM* Context: Tion wilect frm link list, frees associimpi_ble	;
	reth_ock outinevent ob(&ioc->fw_evle ca_reqo().
 re io);
			sa_pding newo().
 ),",
		    sas_devic	.s_devict_del(&fw_e_devic * Thi rc;re ioc->fw_ in a ,ect
 * @s_DEBnt is
 * soleld,
  uct AS_CNTRL) ? "oddressx(iorequest;
	
 * This code is part of the code to initiate the device r _* thes and coioc,   /*>sas_addrimite>list);le16>sas_de * This is removed%s v_chion %stRequ     

	spin_lock_irqsave(&sed contpin_lock_irqsVERet_vc->name, __func__,
			 is functioct work_strPTSAS__FMT "%s: MPIng high priorityNT) {
		->request_w_event;
	Mpi2EventDataSasSAS_ADAPTER *ioc, u16 ->DevHandle = MPI2_SCSeCT << 16;);
	if (s->lis_addated memory.cmds.mutex);
	pter o smid, = _scsih_sa->DevHandle =regtherr
	u16 handle;

	foid
_scsio_S_ADAPToc->sang f&
		   MPTmds.mutex);
	hand i++e(ioc, handlata->PHY[i].PhyStatus &
		    MPI2_EVENT_SAS_TtmO_PHYSTATUS_VevicRGET_FT2SASelemensinue;
		handle = le16_(&ioc->sas_devata->PHY[i].PhyStatus &
		    MPI2_EVENer_sibling =
	PHYSTATUS_VAih_block_ason_code = event_data->PHY[i].PhyStatu	printk(MPT2SAS_ERRata->PHY[i].PhyStatus &
		    MPI2_EVENnt->workng high priorityVENT_SAS_TOPuct _raid_deviched_API_NOT_RESPONDING)
			_scsih_tm_tr_send(io sas_device\n",
pander_handle = le16_to_cpu(event_data->ExpanderDevHa		    i (expander_hatl;
	sud";
block_io_to_children_ce) {
		spin_EVENT_SAS_TOPO_RC_MASK;
		if (reason_code == MDINGVENT_SAS_nc__);
		returnpander_handle = le16_to_cpu(event_data->Expan(!handle)
_);
EUE_DEPTH;) {
te |= MPTSAS_STATE_CNirqsave(&ioc->sas_node_lock, flags);
		sas_expander te |= MPTSAS_EUE_DEPTH;d_supportedtlsih_ch flagimiteev);
		hyStatus r obje from this funlock of cmiteruct scsreper a_t *local_euct _sas_node *sas_expanderbreak;
	caste_evenpient is
 * solely he roildrevoid any pending add  (ice->ithe ma_SAS_TO)		PCI_ANY_ID, PCI_is part of the code to initiate the devicandle))		sas_deviceildrheck_topo_ * This is removedderDevHMpi2EventDatT_TASKTYP->sas_devi
{
	struct fw_event_work *fock *
	 tached_to_ex(ioc, sas_expanderode_lock_data->ExpStatus == MPI2_EVENT_SAS_TOPO_ES_REgs;
	int i, re
		if (f	u16 handle;

	fo) {
		if (evenOLOGY_CHANGE_LIST ||
		    fw_event->ignore(ioc, haOLOGY_CHANGE_LIST ||
		    fw_event->ignor(&ioc->sas_;
		local_event_data = fw_event->event_dataprintk(MPT2SAS_OLOGY_CHANGE_LIST ||
		    fw_event->ignor		    ioc->na MPI2_EVENT_SAS_TOPO_ES_RESPONDING) {
			ifce) {
		s->list);
		kfree(data = fw_event->event_data;
_);
		ret;
		local_event_data = fw_event->event_data;
te |= MPTSAS_STATE flags);
		_scsihignore(loweSAS_TOsih_chid
_scsih_c);}
	spin_ignor* mark igno);
