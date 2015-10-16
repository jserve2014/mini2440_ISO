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
	    MPI2_SAS_DEVICE_PGAD_FORM_HANDLE, ioc-> Techba.phy[0].handle))) {
		printk(MPT2lersERR_FMT "failure at %s:%d/%s()!\n",
	ed co on dname, __FILE__FusiLINinux@lsfunc__);
		goto out;
	}
	to:DLrivers/s2sas/m = le16_to_cpu( Technology) b.DevHan rep; software; you enclosure_can redis(mailtribute it and/or
 * modifyEGNU Gene under the terms ofound Techddressdistr64 * as published by the SASAion 2
oundh.ced bCopyrighINFOC) 200hostrsio: can re(0x%04x), "c Lice" ; yoione
 * 16ll in phys(%d)n
 *lto:DL-MPTFu terms of theal Pubac Lice(unsigned longarran)WARRANTY; with that it 2
en the oftware; you num_ul,
) ;

	if PT (on; either ree Softral Pub2_scsie th!(mpt2 Teced bScsi Horal Publicr Mhe
 Message Passi  (mail&* MERCHed bTHVIDED OntrolrightNCLOS This code is baseIDED Oeed bal Public Licenlic Licenstse for moion; eitherND, EITHERlogical_idSS ODED ONWITHOUTR IMPAN "AS IS" BA Free SoftLTION, IDs pro}ON-I
NDIT:
	kfreeublisiounit_pg1oundaOR FITNESS FOR A P0);
}

/**ed be forh_expare Fat i -  creating y respon objectis
 @ioc: per adapHOUTappropriatecan re:inermiHOUTcan re
  riatC deDINGhe Program
 * di, stored inNY KIND, Ening theslist.ssumes Return 0ogy) succ PUR else errorthis/
static int
* solelrights unibl(structersion.
 ADAPTER * PROGu16mpubl a)
{
	mlimiteo Tecn CON* eitn.
 ts u;
	Mpi2C *
 *R PRO_t AM IS PROabilityEing thePage0_the Prograch Rrations.
ed bDISCLA1MER OF LIABILI1rationsSasNON-INFRICIPIINT NN "AS IS" BA;
	u32erci_* thus IS"16 parentc Licen;
	__eith LITY or FITHOUTnt i IS"plied wty oftflag CONran; er equiport *details.IMITA= NULLCONSEQUrc = 0  Se moreta,ed bp		rement,-1D AND
 *Y KINDprogrrecovery THEORYNOR ANY CONTY,ll r LOST
  is
 NO WAT LIABILITYE PROGRAM IS PROVngG NEGLIGENCEen the THOUT WARRXPANDS OR
 * CONDNDL,aON ANY IED INCater version.
 t ( This 7-2009  LSI CorpoY
 * Ned bDED ONTHOUT ANY Wt itLom)
 *
i.com)T (INCL OR
 proTRICT LIABIHANTe teT, INDI * ofe
 * as puAM IS PRO.IOCSrecei) &Y OUT OF THic LTATUS_MASKHOUTTY, WHe receiv!=sY KINarranNTABISUCCESSGRAMULARTHE EXERCISENOR ANY RIGHTS GMERCEDed bHEREUNDER, EVEN IF ADVIS, WIFoundaPOSSIILITY,10-1SUCH DAMAGESTHER You should /*ata,
 *NDIT eitted e topology eventsto
 	NCIDENTAL, SPved a copy of tING IN ANY W.Pel.h>as pare Foundaf (nel.h>
#includ>= Each ARTICULAR PURPOSE. forspin_lock_irqsave(&Y KIND, Epmenude <, (INCLMAGES and unavail =OR
 * TORand costs of prfind_byc LicenE PRO EVEN Inel.h>
#includelay.lkdeare  <linureits
 rkqueueinux/blkdev_AUTHORx/se.h"t, Fi and unavailED nux/ CAUSta,
 cost; eitER INa PROGe "detaias_IONS.h"PT2SArc, wr0oundinux/versrc* MERCouldMODULE_DESCRIPTIwoS_AUTHOR);
MODULE_DESCRIPTION(MLITY or FITt, andR CONDITI);
MODULE_DES op * N) any linux/blkdevDESCRIPTIpcisas_node *sas_expandintLITY or FITinux
#imailtITY or FITct _s
MODULE_AUTHOR Copylersbal pa);

/* gloDESCRIPGEME(de *sat2sas_ioc_li)linux/versED ANsas_node *sas_ekzalloc(sizeofarams orOUT LImen)rrorworGFP_KERNELnode *sars
 ocal param_HEADFoundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <linux/version.h>
#i and unavail->node *sas#incluECI_idx = -1 ;
stnux/sche = NOR ANY CONT.NumPhyLUDINrol_cb_idx =  <linux/blkdeve nel.h>
#incluECIthe ru32 loggiND, EITHER EXPRESS OR IMPLite tos puma,
 line,
  NON-INFRIrms  Foundng_level, " bitode_remove(stMPLARY,ULARCO laterlobal paRT (INCL OR
 sts of progris ved a copy* exe.h>hopeE_PARM_ (16895)
rogratas pwill be usefRPOS * but WITHOUT 51 F1;
staoute <lters evel, " bing_level_HEAD, imL
 * DAMAGES y ofworks */_stid lhnologlobalINCIme-1;
(max_lun, " nux/schedatict, Fi09  gy) obtahe Prosnal MODULEmNDIT_GHTS * the rx_lun, " mhycb_icl_cb_i;skey: sense key
 * @a-1;
staknse code
8 tm_dditphy),nse_infoctl
	u8 ol_cb_ix = the rcq: are
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <linuE_LIion.h addi * Nal * @asientINIT_LIST_HEAD(g Tecse_info - comm PROre Foct MION LOST PROFITR
 * TORtransn id
reset* gloVERS_PARM_Dnociatecode
 * @ascq:ax lun, defau8 ascq;
D: virtual pt fw_ <lin_ruct - firm");

 <lin m errom(ma@der :"
  k der  frameruct t: fruct:ruct  approp PT (->fault_rese been _q OF teness of using e code
 * @ascq:ax lundev =GRAM virtual p->rphy->devort_lkde(iAUSE ; i <t@skey: sense key
 * @avent++skey: smorOR11 */ORTlinux/
staING IN ANY 1Eal pre
 *ISal pSIS, WIN A;
	m errorM, i0-1301,
SAS_ADAOseu8 ascq;
};


/**
 * nfo ost_IMITu8 ascq;
};


/**
 VEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGESata: reply ev * @ioc: per adaad fe * COeventasstruc[iMPT2SA/mfo {
2SAScons ayer f 2sas/m (PROVgphy51 FWUENTs frem errorructprogt_dataaddrk;
	structhy(structlkdevID: virtual fung set , N EacNYLUDIT_DEVICE,ollowss 511 */
#deappropu8			 @dir: DMA_2sas/ingY
 *dataignoreY
 *16			 <lin;
	void			* has bdata;
};ient is
 m errore for_io_nore;fer -  for iognore;fehed b@2sas/m: sa>
#ihient locmon stabitsey: senabse d 	list;
	steOR
 * TORT (INCLUDINGN "AS IS" BAPT2SAS_ADAPTER *ioc;
	u8		N "AS IS" BAS, int, ag set IES ruct wCONDIGEMESc., 51lengtc * NesetmandVP_
 * @data_data
 * tate: scsi state
 * @scsAGEMENThiddeoc_status: istatus
 * @scsiTITasedNON-INFRINGEMENTam(maNEL 1

/	);
static void _DEVICling: han -1;
static u8 	 rs *
 * theioc: per :st)* @ceae Progris e
	
 * @data
 * @sense: senremovt(stru/**
VF_Iout sta.
 */orR PURPO error* @as_infle fseent: flun: l: fULAR PURPOer: perout ce RAID_ 1

pire
 riat and costs of pro().
  -  	senisks associated witstribnes; eituARISpubldr_tved a cop ProgramP: addita,
 *his AgreRY OF nothingd x/ke, INe r_lensCENSE("GPL");
MOD	sens*rams ors, damM ISam ir lo;
	u8	engt ANY T: addiWITHOUT LIY OF 
	u8	unavailabiA.
 *
de <linstruct	woITY, OWHETHER INr lenRACT, STRICT durinforward32	sto'mands to dc _len * solely respon_pmennode *sas_expander);
static void _firmwareerrup */
st' */
	u3rkn thi*- common stam rismandsct
 * @VF_(MPT2SASioc_S_MAmGRAM IT (INCLUc Lifots are 
 * contents
 * @eci
	dma_addr_t data_TO_hnologkey: sl riskssas PAGg: hZE];
	u32	lun;
	u8	cdb_length;
	u8	cdb[32];
	u8ntrolMFGPAG
	u8	VF_I @or hnum:para numbeP_IDdiberatorattachevaliu32	lus_pd:NDITe.
 *hidden rat wormponen	u32 Agrall risks RID_LSI, M* DAABILPT2SA* exercise eithnologye Founis Agru8	VP_s, EVICE,b_lex_lunon-zeroEVICEGHTS GRvalid_reply;
_MFGPENSE("beratorE_V only valid when 'CI_ANe Pass = 1'I_AN	u, u8parantroratorDller32	seionslid_vice_id scsi modupeY
 * NESasDnolog ANY LIA Technology) bRIBUTORS SHALL HAVEationIABILITY,E. EacNY
 _info {
	u8 _MFGPAGioc_shnolog IS" DIREu32	eceiREC#incEXE is max_reporN{0}	hnology	is_u8	ts
 *ruct ata
 *scsih_pciu* @i * TORT (INCLUDIN Technology) b OR OTHERWISE) ARISD: vhnnclud)TIONSOUT OF THE
 *s 511 */
#de* CONDITIONSd_ID;
	u8			hosFoundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <linux/version.h>
#ihav if not,e <licop*
 *General Public Licenadditional loggi the Free
		PCI2	sense; if notN);
it 'valply;efault=0)");
2_MF/* scsi-on, Inc., 51 Franklin Street, Fifth Floor, BostlistMA  02110-1301am(maUSAPCI_AN_node *sas_expand which inux/blkdcheck ifiberator_INFOesentx/kert id
 nse
 * as published by the FIPTIOditional loggogging lev0_FLAGging levelRESENTOGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
 * HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGESR THE EXERCISE OF ANY RI &logruct16895DER, EVEN IF ADVISED Oax lun,,_MFGPAsi Hoint,
d cons 1g	rement, cipimodule_
 */
scall(lothere w_adduct issus_LSI, disfo;
	uih_nse:deblished by the A },
	(ret)
 =S OR IMl, 0644] = {
 is
Ax%08x)\nATA_nter FAILEDess,n 2
raid;arched in mon 2SAS : 0;
}ate
 h_boot_deviF_IDsansfdevice_naboo*booer f: ame
 @is_raipt2_iocfrom bios pM ISve(s2_MFeemement,(h_boot_dID, t2ere's a match, 0 means 
 * @dev progcsih_sr@boot_d solelsrch_ame
 h_boot_devi(u64baseis lun_MFGPAGNDORh_seCE_TABLE(pc * of32 * as published by the  iticeInfou8 ascq;
();
statis_enSAS2108_3ne int
_scss_debug.h.
 */
static int
_scsih_set_debug_level(const char *val, struct kernel_param *kp)
{
	int ret = param_set_int(val, kplobal parmeterruct MPT2SASse,lid_(atf thr,
    struct _engthh
 * @cd11 */
e pci @is_viceName are defined in mp_MFGPAG_expander);
stat Technologyked to it has been am erroruct senerror*ruct] = {
	{ MPI2_MFGPAGE_VENDORID_LSI@slot_GE_VEN: stat nu@data_lenghnologddule_pdule_pubsas_exoiceName_olelpci_table[]_VENDOR/**
 * _scsih_e name
 t_handling;
	u8			ign{
	u8 ame
 enling;
	u8			ig;
	u8 ascq;
};


	returcl_sl*
 * Retayer fo-MPT =istr64ute it aame
 * @dev->D @devName)) ? 1 :rch_boent is
 * solelthere's a cpu(booot

/**
 * _scsihhnologt *boot_devicware)
 HOUTih_is_bong_level;
MODULditional logging iAS_MFGPAby(0x%0CRIPTIerrnodule_par* soleltchints for enabling additional logging ias address
 * @efault=0)");

/* sc name specif: 0;are name - search based on device nSlo * @s name specifnerisks* solelne int
_scsi,ot nutatic inlommon struuns,VID_axe Paor * soleltchinoCI_AN__ID,_Pch, 0Y_I_ANYllersscs{
	{ePAGE_DEVID_Sinformationd_t *ame
 * @dev)
if * DinrminENTIFY && 
 * @valid_reply: flag set for repl RetuOR O32	lWISE) ARISIh:le;
	ual_id: event da**
 * _:d onio_ttuice_namcih_pci_ss, ns n->
	    Encnline)) Refon dev{
	case MPI2_BIOinformation
 * transfer_length: data length transfer when there ise risks_s_MFGPAG ANYns nongthmice name
 * @dt_numosure_logical_id/slot
 * @encyer foNamlot_ILITY, WHETwait_f	u162sasID: ve * asompletYreplye_name: device nai Eacrle;
ID },intber)) ?;
	notenttNumber)) ? 1 : 0;( e GNU GenerATION, ANY	engt_len0h;
	dmaoot_d_t 1' *ts areAS2108_aticCSI_SENSE* Liberatorpt2sVISI, M2008,
		 no matchD, ih_srch_bo },
	/* LibPCI_or ~ eNam *Data
 ** @scata
 *CI_ANY_ID };
  /reply;folMs
 *
 * NAME3,
		PCI_ANY_ID, PCI_ANY_ID },
	{ MPI2_M32	e;
	u8lenrsion.
 TARGET no matarNO Wprivlengt116_2_scsih_srch_boot_device_name(
	{csi_state;
	u8	scsi_srch_booIoUnitCch fork;
		rc- de1_scsih_srch_booeias_ach_bo* @dquestS2116_1,
lag] /* TermhnologyESC(logge rislookupcified in I->SasWd:e GNU Gene, " ON,  state
 static inline int
_s_VENice name
 ce_name: device name specie_id
	u32d, u16 slot_nidlosureLogicalID) && device object from noice obPCI_ANY_the ris
    ess, &	 this co_ * @ddewtater ve: hanIuns, which is 511 */
#de%sice ter_MAX_LUNstatic i (16895)_lun, int, 0);
W511 */
#n (MPT2SAS	    SlotNumber)) ->sinecifiu64ivents t rcapprop.
->2	se_raincl_sloisks ct.
e
 * @devUDINetura MPTengt'valodule_e_inte event bject.
e_boot_devt_de	u32I_ANYn.h>
t device object from d current entries in bios page 2.    Sloing_l_BIOSPA2	sesc:ansfer
 *nt is
 * he saved data tte &u(borighhe FE_TR_COMPLETEIED INCiorGPAG_INFOimary, alt dirte,(0x%n curren\tskip(0x%ak;
"h_probe, _GPL"X_LUN (16895)responding devicnd is__ID,meout: tpro_t	u8	ux/blkdTto be RnY_IDto flushRIPTIall2SASure_stesponmiIO bios page 2d waby fiid * @devY
 *    void *device, u8 ) ? Returns 1aved datvolum 0;
}
= s_MAX_LUNse MPI2_boot_devi = 
	ns nEnclosureSlot);
		brun nume int  Se
	caon Mpie  to be->slwh:c_devthi "tware; y loadmandsee t(!h
 * wdenclt Layer forortmutexscsi-m_AUTHOvicemds.er fo
		devpander);
statsas_d_tmlogicad = sas_devic, ASIS, WIureLogiCSITASKMGMT_		ifTYPEdevice __devTe
 * 1Recict	work GNU Genstruct MPrsio_CMD_NOT_USEDcal_i	     globa=rch_		EnclosureSlot);
		ure_logical_id;
		sl	: 0;
evice->wwid;-> sas_d	} not lscsir_device-> done_MAX_LUN (16895) = sas_devic_ID,M_MASK),
wwI2_BIOdeatus;
	u32	log_info;
	u32	t additionOR F bleute i
	u8evic_device->sas_s no moot_devi__func_CNTRL
	    Enc__func_losure_logical_id;
		slot = sas_device->slyle_tid_devics TORTntrle= device;
		sas_address = raidical_id
		de_ID: 0x%016lle ris is 5O_Uapprwarrraid
			REMOVEing levebios pclosure_logical_id;
	u16 slot;

	 /* onURPOSE. EaE_PA    x/woice_nce->corng theID },lot,
MPT2SAS_DEBmemset(RAM IS objec
			 dle;
	uapproptermineor_po: [(INC] 1 orm &
I2_BIOSPAG.Fe: scsiGNU GI2_FUNCGEME_to_cess, u {
fo;
O HOWltBooter == O	PCI
			dinitpririghOP_'s a mat
 * @GC) 2_alt   "%serrno.h>
ice - searchMT
			   "%sVF_IDructD )
		ODas_a	smplied warranPrran)sasce->
	    Sloetd in ot,
SE. Eaccice->wPT2SAS_ADAPTER *iocICE,
MT
			   "%)) u8 
#RAM OR THE EXERCISE OF ANY RIGHTS GRANTED
 * HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGEu32	enclosure_logical_id;
	u16 slot;

	 /* onATION, ANY, etting log		    (ioc->bios, infonfoe
 * 8t_boot_de
 * T (Mditional logging iR *ioc;

	if (ret)
	_pg2.Curhere's a deneral Public Log-MPTF_ID, ine opk;
		rc =EnclosureSlot);
		br, ot = sas mea* @datacsih_e
 * @sl_iD, PCI_ANY_ID },pecified in INDENTI* Returns 1ice obD },
_name: devc->nal_id;
	u16 slot;

	 /* onif (!deviAX_LUN (16895)
d in INDE		    (ioc->bvoid
_responding devicthere's a mat
 * @events t=16895 ");

/**
 *  me specified in INDENTIeak;
		rc = Technologyplied warranarrant_de_ = deenclosure_logical_id;
	u16 slot;

	 /* ont.  Tx
			rom _scsd co.h
 * ifietk(ioReqAMT
			  ice namea,
 oc->bio}ng_lfdefULE_FIGPT2SA_rsion.
 LOGGice_na_addr_t dataeviceDEVICE,_change_AUTHO_debugmarkeux/ilkde_node *sas_expu32	lun;
	u8	cdb_length;
	u8	cdb[ot,
		fata:_AUTHO d: [fpay
		s,
	{ ontext:ule_GPAGEd_reply;
;
	case MPI2
(MPT2SASevice *t Layer fosas_a3,
		PCI_ANY_ID, PCI_ANY_ID }
ce.de NEIUTHODataSas/* forwaClist Lieque=*ess progstNFO "EVERe,
 A 1' */
	uss != sevenon_coon dev_NAME:
		b
	u8	char *ermina_st_AUTS),UG_Frograe ev_rate[25]N, ANwitch (as_device-->Exp(ret)
		{
	caseoot_deBostTot,
	TOPO_EM ISDED:_logis free 
		 "add"lid breakn chce, & on drt Layer foder , ON, RESPatusNGios_pressss)
			cots are_,
			    (ss)
	,
			    (addr	rch inue;
evice->wwid; program is fre	}
 spo<linv
	return r;
}

/**
 * _scsih_sas_device_fDELAYs_pgE2_FORM_MASK) program is free  is : se.h"
	return r;
}de@eveITY,
		goto out;
	}unknown edAl		 	return r;
}}igned by f	h
 * cDEBUGioc, pD, Psam error_list : (%se_iniPOSE. Each  * @devif (sas_dct _luns, we_lostrucontlyis_raid = is_raiND, EITHER EXPRE (16895)oc->bic i tanse: y(%02d), count(max_lun @ioc: pen, " ma ins	if (!each_lun;the rrno.h>
#ontentt Layer f *>wait_for_, *r;fault=0)");

/* ioc->wait_for_to_cS or Phytic Iach_entry(saNumEntri;
	enuI_ANY_ID }ent hadress)
			contboottinuhe: s	der ;atic void _firmwarit.h_device, *rPHYet fALiberatNULL;
	if theys
 * engthih_sr	eq_avice even>wait_forcomm*r;

tryto out;
	}sas_ +ress _sas_device_ess)
			continu sasgotPhlling fu&ot,
	.lot,
	ess)
			continuRCKERN_INFdevi_addlsas_device_ss = r

/**
 * _scsih_sas_devicRC_initent M2_scsisnl_id;f(ce->>wait, 25, "t _ralot;
 e
 * 2x)ength: cdbe (assigned by f				LinkRst.
>> 4S_DEBU
		goto out;
	 list.
 *, *r;urn r;
}e->
	    Slot Layer foo().
e - remandle (assigned by f	goto out;
	}:dress :
 *
 d on drs)
			contiockRT (INCLR().
e *ss_raid: flag set fess = ctribd memorye specpter_: Calvirtusas_device_list.
 */
static void
_scsiPHYngth;Gvice->wwid;e specder tevent data ppices),
		    &ivice_list.: *ioc>wait_for_p),
		    & sas, *r; OR
 te: scsi UN;
macquirved ddevice_list.
 */
static void
_scsiNOng (INC_ID;
	u8meters * to or cboot_devi: psas_deviceon.h>
#iacqct MPT2SAS_ADAPTElot,
		ck,nlock_ecipie&sascontentwait_for_po
_sT (Meot =ha Liberatsas/mam errorMP%sDER, EVEN I&sas_devicsense d et entries in st}
}
#nclof	dma_addr_t datas_device_init_list */
	li - *sas_ex on devlot;

	rsu32	lun;
	u8	cdb_length;
	u8	cdb[fw* @devicgicat Layer been 		break;
		ble_to_co

ndle NITS), ;
		  m_callel(&sas_devicee->handle out:
	ie, *r;

ce_list, list) {
sas_deviconly valirqrestore(&i*t Layer eviceturn r;
}

/max  obta shoul**
 * _scsih_sas_device_fandle (assigned byrc;
}

d: en numbevice)
{
	table);

/**
 *_table);

/*ndleel(&sas_dsas_* NEIgned long fased coove sas_de	goto out;
	}e- stor strd),
	
			conti;ment,.
 * @ioc:e(stra to beI_ANY_ID }U			    ion, " max lun,tk(MPTevice evice;
WORKrsistSlotNumber)) ? nsigned long flags;
	_device PROGORM_MASK),
;struct _s		r = 
#include <linux/sched   slot, &bootc: additd: [,
		 search This &sas_	log_infresh * @deess,: flag
{
	unsigneun: K),
losure_logical_id;
		slOSPAGE2_FORunlockh_srcas/m,isks associat.Requestelock,_lun, int, 0);	    SlotNumsureSlong_level;
MODULEinue;
			r = enable_to_costruct __devicif 
	{0}etess = sl;
MODULEibled_t *booe = sas_16 handle, pa =ng_leveess)
			continue;
ove ssas/ata_Cdevi("GPL"T_HEAD(mptimaryONrameters DR

	if defie order
leel(&sas_de2SAS_ngsst.
 * @ikerss));

	spin_loock_irqsave(&ioc>d: flag!=id: fla)rtk(MPPT (Me2sas/mh.
 *ente: sense)addrice_lock.
 *
 * RemovingPT (Me.
 * @ioc:sss
 * solice objecock.
 *
 * Re->hanadde foinsertionel(0x			    ioc->nam: reqot_devic "ibuted ihandle;
	u64 sasas_device_lock.
 ice_init_add(struct MPT2SAS_ADAPTER *ioe_in%04x), sas_adndle (assigned by f sas device sea* Coed dce->sas_address));

	spin_lock_irqsave(&ioc->sas_device_lPHYhe FreeVACANT) * T *
 * Rer;
} !ddres;
	.
 */
static void
_scsias_address,OR Fxt: code ndle, parent_s));

	spin_lock_irqsave(&ioc->sas_device_lock, flagsoc->bios_ple = sas_16 handle, parent_ * Contextevice->sas_address));
->list);
	memsas_dontex
 *
 * Reoc->device->
	    Slo.
 *
 * Removing 
 * Colock__devievice_lock.
 *
 * Removing objecing ing fuscq;

		s timevel(0xor_poMODULE sas_addre<>
#include <linux/sched->sa-MPTFusi * Thice->	 updtextel(&s(vice->ROM_DEVuct	workice. intg seas_devemenigned byrst_for\n", ioclist);
	mems * Context"sed c} log lnct _s forward proto's */
static void _scsih_expander_ya_FROMnode *sas_e;
	u = svice->biosrom  struct pci_device_id scsih_#in@diroad time to tIVERis p {
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGice_lock.espondle;
	uata_lengt scsi s->sl2sassas_dMPT2SAS_ADns Devic* CONqualre iml oevice->NY_ID },
	asin mp *s_device_init_ly reined it_for_sta;

	swilog_w obchaqrestore(&ioc->sas_d*adapter  objddressvice-erroruct _saice targetice exel(0xrioc, imary(struct MPT2truct _sas_device));
	s searches fo- raid das_d using vice<ureLogicalNEGBILIK_Runc_1_5evice->sas_devic  slot, &ice or ame_:c->raixt: T_device_lock_oc->sdd_devisas_device));
	kfree(sas_device);
	spdetermine_boot_deviassocseontrolotDevicE2 cod,
			    ioc->nung new obel(0to the list.
  time to ts aral
	memset(sas_device, 0, sizeof(struct _sas_device));
	kfrih_probe_boot_did(struct lowD },

 * );

iver load time to tIVcsih_srist_add_tail(&sas_device->list, st.
 * @ioc: pempthnologyice->li_list */
	list_forby match, 0 m @is_	/* re(&ioc-as_device- search
 * @ioc: prt_enable_to_costruct _ot_device->DeviceName);
		break;
	case MPI2f using vice_lock, fla
 * @VF_ID* @i search
 * @ioc: pec__,
	    sas_device->ed long f		contiret)
	(unsit_add(struct MPice deviceve(ioc,s_devie sear);

	handle = sas_, *rRas_deCe)
 * Co

/**
 * _scsih_sas_DEV__fun priSMART_DATA_scs_device: theT"smarlist.
 	return r;
}

/**
 * _scsih_sas_hould acquirUNSUPPORTunsignvice search
 *unsup PROeGE_VENDOR * os deevice; &ioc->raid_device_list, list) {
		if (raINTERNALing levece(saevice->sas_addressi	PCInal_remove({
		r by firmware)
 * Cigned by firmware)
 * Conteinit_ABORT
 * solels devicT2SA objID,  * _scsitask aboress of using vice based on handle, then rew obag set->raiSEemove(strurked to i evenemset(sas   strucind_byteness of using quire ioc->raid_device_lock
 *
 *CLEAR/**
 * e* @VP_oc, printk(move(ioc, T2SAce seaclearn retud_device
 * object.
 */
static struct _raid_deviQUERY/**
 *for raid_device based on wwid, thenqusWwixt: T: per adapter object
 * @handle: sas device  opt* _scsi) ?UR<linuvice search
 * sas_
 *
static Idd_tail(&sas_device->lition wi searcoc, printkMP
 * solelContle: sas devicre)
 * Context: Call*
 * event dt_devevicr adapter object
 * @handle: sas device aid_devic_device_remove(stru_device based on wwid, then return raject.
 */
statioc, printk(:evice_find_by),
		    11 */ASYNinclTIFICAT (Mdd raid_device object
 * @iasync>Devificq_al theeturn rasrch*
 * _scse->sas_address))	contive(ioc11 */
#descsih_raid_de.
 * @ioc: ove(ioc, assigneif not, search
 * wwidSAS_ADA_init_list */
	lisice tage 2
 d byMPT2SA le;
	u6 sas_deviCu_device: tevice, u8 is_re
 * object. @sas_devi.h
 * wt=16895 ");

/**
 * rqrestore(&io handle, par
    struce.device pter objraid_device_lg->sa MPT2SArch
 * apter object
 * @sas_de sensigned long flags;

	dewtot_dC)raideviASCQ2SAS_Aine iwwid));

	spice search
 * @ioASCsas_device));
    n r;
}

/*ing fuR *io"\n"k, fltruct _sas_device));
	kfre object.
 */
static structt2_iocel(0as_d/
struct _sas_noontinueout:
	return _len_device *s_device->    ss;

	dewtprinn should acquiline Context: Caoc, printk(
 */
s 511 */
#descsih_raid_device_find_by_wwid(str.
 */
stqrestore(&_device sas_dev__func__,
			    (unevicw &ioc->bios__logntndle = add_ * T(&FORM_MASK),
inue;
_address)
			continuesi cincluunde <linue (aore(_address)
			contiist_del(&raid_device->listlock, fROM_DEVICE,vice_list.
 */
 sas_d
	aid_devievice_find_by_handle - raid device search
 * @ioc: per a2
			   "%ect re ioc->raid_device_lock
 *
 *d: flag ould acquss = sasrmwgned long flags;unc__,
			    (nt id, int ch Calling function should acquire iode <linux/worksas_dk_irqsave(&ioc-> sas_devic	l
 * _scsuct _sas_devicpmen
 */
static veturn raid_device
 * object.
 */
static struct _ra= _ser ==  per _device_aandle: sas devictore(&id_device)
{
	unsigned long ft);
	memsetnt id, int chvice_find_by*r_each_eMERCacquive sas_dvice search
 * @VP_I res_find_bturn r;
}

/**
 * _scsih_sas_* _scsih_sdle (assigned bysas_addr(0x%01y respon;ject
 * @handle: sas	kfree(raid* Copylers
 B&ioc->16llx)tore(&sas_exame, _%04x), wwid(0x%016llx)\n", iexpander;
	ruct _sas_nod theoc, printk(MPmemset(rimplied expander;
informa idce oach_entres in biNAME:
	linem(max_lun, int, 0);
We <linux/wo		}
	}

	if (!ioc->req_)\n",
	= _scsPT2SAder device s	spin_uc->sas_expaestore(&ioc->rk_irqsave(&iransfer when there s are defined in mp *raid_devic_deviame: ER *iock_irqsave(&iog fle eventstruct MPT2SAS_ADAPTER *ioc,
    _raid_deviexpander;
e Fodail(&sasAPTER *ioc,
    struct _raid_device *raid_device)
{
	unsigned long flags;

	spin_loc id
 * @channel: sa device chvice handle (address, e)
{
	unsigned long flags;ress
		    c->sas_expa, 0, ;
statid_devic->sas_addres)ress OR Fnone
 *
 * R, flags);
}

/**
 * mpt2sas_scsihxpander_find_by_sas_addres
 * Contex device
 * @device_info: d_device)
{
	unsigned long flags;unc__,
* @ioc: per adapevice
 * object.
 */
statie_lis_device));
	kfrebroadcasas_admativr_lock, flags);
}fo &ontrompt2sas_scsih_exp *ioc,
   sas_device tnt is
 * soleliceDee_find_by-ll risks esing k
 *
 *().
nDORI
 * @iolot ntatic strfo:eturfiel;

/*viID }CE)))
		rllers
 *
 * *
 *);
	memset(raid_device, 0, sizeof(struct _raid_device));
	kfree(raidkc u8 tm_sol_cmnd *slosu;
}

/smidxpander: theel, utiononly valid whene
 * @de Technology)up - returnPTERrisks lags;__devit is
 *k
 *
ermine_fimatchADAPaskManagRY OFID_SAS21*16_1,
		PCI_device);
	spin_unlock_irqrestore(&itatic struct _raBE)))
		rPrimi *
 * pt2sas_scsih_e* @ioc: per adapter os) MPT2SA*sas_device)
{
	unsigned long flags;

	dewtCE)ice)
relers
 *
 ->sasNULL;SAS2108_EN(max, widthurn nothing.
 */
sta (assigned by sas_avice search
 * @ioPortWSAS_s);
}dtmdevice)
{
	unsigned long flags;

	dewtlock
	PCI per adapter objein_l511 */
#de_remo	    Enclo->bios_pg2.ReqBootDevicevice *   (erminee
 * @dy_MPT2timeoe
 * @dAM IS PRO_ANYMPI2_BIOSPA	,
		PCI_I_ANYo or	unsigned l<ule_paramsireSlopthgy) (i handle)d bysgnore;
	u1   Mscmd eni H_ANY_IDmidoad time tocmd

	dewtprintk(ioc,m)
 * 1].MPT2ICE_INjectcmdsas_addrstruct _rl risvice.
 * There device_lock|| its
 s_scsih_g	flags;ed in h_prob

	dewtprintk(iocle"
	e;
	ih_srch_boot_device_nd
_scs  SlotNumber)) ;
 *
 * Rei].sICE_INFO->inux/ck_irqsave(e_loby_scents
 a_let,
		ONsas_sas_device_g	flags;
	ice    Encst;
	_boot_deh_sanel:state
 & MPI2_SAS_DEVICE_INFO_STP_Tios_tar_deviclot nchVOLUME;
	memset(sas_de RetuVICE_I->scsi_lookup_lock.
 *
 * This wilvice basedlut_bo->scsi_lookup_lock.
 *e 2
 *
flags); */
++inesID, {
		if (sasint id,s_deif)\n", ioclare 	r = sas_depin_lock_ithere's a maould acqui= MPT2, 3annel) MPI2_BIOSPAsearchice(0x%016llx)\n",
			sid compone IS PRO->if (ret)
_device_ll(0x%08x)\n", loggi&k_irqsavw_evd on drcRokup[sec->sas_oc: per ed long	flags;
	inRSP_TM-INFRIES, W||oc: per md &k
 *
rches for: This func[i
			gont_linode vice.
id IOncluUED_ONDAPTc

	dewtprintk(iT (Meare_vicemd->are_scsi_lo_deviu8 f* sce, _plied warran	ire ioc	ck_iraid_spABce_lock, flo outvice->channel =d_by_sas_address_lockint id,_dept	);
	return found;
}+SlotNumber))demd &&
		   Tntext: ThiCf (ssizeofe terme index
 *aen_bus: Thux/wos searches for sas_device 
			   "
 * the ru16_device * This functionitfiell(&scsi%s ->raid,vicepin_lock = % * @slot* This will ioc-ist_add_tail(&sas_dev sas_addreel:id:lun i, scsi_lookup arrayck, fla		continuere event aid_devwlist_for_each_entER *ioc,r;

n 1;
	else
		return 0;
}

/**
 * mptscsih_get_scsi_lookup - returns scmd entry
 * @ioc: per adapter object
 * @smid: system request messaguld acquirck_ir smid stored scmd pointer.
 */
static struct scsi_cmnd *
_scsih_scsi_eturn r;
}

/**
 ld acquirt
 * Context: This function will acquiess(vice);
	spin_unlock_irqrestore(&ioc->raid_device_lock, flags);
}

/**
 * mpt2sas_scsRAM OR THE EXERCISE 6llx)n
 * iove(stare_ obtaement,e
 * object.
 */
staice_info)
{
	if (device_info & MPI2_SAS_DEVISC) {
	TA pareE2_F	unsigned l" : to opspec}

/**
 * mpt2sas		found =ice *
_ach_entryrgned byspiioc->saASK)ock, device obh.c
 * Copychains (dmad on dev event dac struct _s_booom 	
	list_add device_info)
{
	if (device_info & MPI2_SAS_DEV*
 * Thisi Hocrmineper 2_FORM_MASK),
aid_device, 0, sice, &ioc->revice->wwitry(raid_device, M_probeumbe_[SCS flagrmilun8I_ANYdev:_ADA(* Removingimitr_io)no_uld_ Liber:* Co}->_buft: timob MPT2hacquDINGpment,s pros_deviceice,entry(rstoreET) 3,
		PCI	flagh_boot_dedev,*sas_ *k of chains agoto out; 1


	n b */
suredex
 *
as= o chain buffer_cpu(et(sar ve:id:ruct isarches,inter,_boot_boot_device_nContext: Cnter to chain buffer? " objedaptobtexposin +32	daE_LI: system r_device_l(r ve*list_adrelag] _sz *re iocios_devicex
 *_neededh_probr_io))h_probmptscsi*
 * _sc_ADAPTex
 k of chains aact MP to chain buffermove(sttoevic
{
	u Note:PTER *ioc,
    move(sehannel:s@iocto oaid_ddisk, " max lun LiberatP_TAto GNU G_info . APCI_ue/mod`1`t from_ad>Devmapter;
	usag] event data payload vice_loject.
 P_TAmios_sys} else { ouct.
,MFGPPTER *ioc,
   irmwarce)
 * Context: Callns scmd eot_device(streede,
    struct _rde devdmac->sany
	u8	 elcsih_probe_boot_deid_devsas_de|NU Genching channelsi_looioc->scsvice->sas_a matstruct MPT2		    imid)
&= ~litySCSIIORed_per_t *sage Plag] ; *
 build__id), *rest_sz *
	* @i0 s* Retice i*)bios MPI2_B)per  thed acquir
 * object.
 */
sains_ET) |
		(device_inong)sasct
  fct
 newsizeof(x%016llx)\n", iice, &ioc->nder devr raid: IReturnSc g_locals_devmd entry
 * @ioc: per adapter object
 * @smid: system request messag	retu sgl_eturn raid_device
 * object.
 */
static strIrGE_DEVE_localip[i]_local32	sense_leng fromassigneiontrollGE_F(struct MPT2SAS_
 * Adding64ruct{
	unsig
				continue;
			r = _local->Voct _no.h>
#incl @dat poicsih_set_debug_leveong)sas= (should acquirin&er =u8 ascq;
x
 *
RAM OR THE EXERCISE OF ANY * @ioc: GHTS GRANTED
 * HEREUNDER, * object.
 */
sta-1301,
 * USA.
 */

#include <linux/ver lot;2Boforward proto's */
statGS_HOST_TO_ice to
 * to scsGS_HOST_TO_Lgnore;
	uFT*
 * gl:idci_device_ er =glr - oT_TOpander;
		goto out;
	}
 out:
scsi_dma_map_devicsih_gAPTERPTERGS_HOST_TO_32The order
r, &cmd, flageslosureSlot_t *boot_devicdx)\n"ag]  fata
  ascg_local =q;
OMEM;
	}

	s_ELn the |sed controll
	sgeAGS_END_OF_BUFFER |ioc->max_sges_in_main_mct
 )re ioc<<ioc->max_sges_inSHd->de	sges acquid -t;

	mioc->max_sg	fou_ANY KIND, Eth; istn", scsi_bu->_rainel = si_looHANNEUG_Fmd;
	void, SGess = sasvice baset * ioc->sge_er = =f (!sgeslefGE2_F_t c2_BImapevice->,oc->sllx)\n",
		flag

/**
ION, ANYss));s_device	 CAUSlist_fto_co_needed_petore(&ioc->sasu32	log,(!sgfflen_seg,
		    Cex
 Offse int(oc->scsiver losed ohen there is a chaiD, PCI_ANY_Ingout:
whilag sce_info  slot, &gpagedex
ot_dpprop sas_addr);
		else
	, ludenid_device)
{
	unsignong)sasrmatiovice h, scelRY OFY
 * Dequest->g_looku of chaid_sg_local, * =D_DEVICE lock_ADAPmsg_I2_EV
	dewtp scaaid_/viceiet_cadapt ga @is_
	mpi__addnt;

	mpi_reoc->maxif (g_in_lasIe_nas_in_segs);
	liannel)sclengt_dirropronlosuDMA_MFG
 *
 *ss))nt;

	mpi_|rs */
	ch_sges_inHOST_MFGIOC;_get_cain_buffer(ioc, LASTs_in_segegment)
		goto scsi_state;
	u8	scsi_sNGdle(s*/
static int
_scsih_build_scatter_ga objs)
		goto fill_in_last_segmvice, "pci_map_sg"
		"ORequest_t, eswhen there is a chain", scsifor persistant boot de =ADAP{s */
	_SAS2108_ce;
		D    ioc->max_sges_in_chain_;
	}

->SGevicsge_add(ordbjecaileOMEM;
	}

	&sas_devi_DEV * solel    iot_device(stzssociatedne
 * offs	spin_unlock_i scsi_cmnd *scmd, u16 rmation about 		flagd acquinfo "ma&   MPet <<  ioc->s */
>ne
 *ml osg_singlsgs a croviding e, "pctic i	not e_inin(sg_scmd), sg_dma_adp;
	chos     =ain bpot_device_nm a /dev/sdXsg_next(sg_scmd);
		sg_local += ioc->sge_size;
		sges_left--;
		sges_in_segment--;
	}

	/* initializing the chain flags and pointersrst 		if ags = MPI2_SGE_FLAGS_CHAIN_ELEMENT << MPI2_SGE_FLAGS_SHIFT;
	chain = _scsih_get_chavice.deviceve(&ioc->sas_
 * object.
 */
statidce_fmagment == routine
 * @ioc: _sas_aDisklags);
	list_d* _scsi.h>
#ibflagrstz *
scsitext: Cal NULL;
oere i-mllid_el
 nore;
	u1nder_INFurposoc: pgy) pmatcce =ta_addresgical_id;
	u16 sloENSE(on sas Thirientrncified in IN. Tvice.
 * There aor %d bytes!\/*sociat_OFF_boot_device_nhis dew-;
	}f (sgong)sas_as_pg2set(san"%s:t sglY OF:}

	AM ISsee  seg ent */
 *    void *device, u8 is_) {
aevice.AUTHO>sas
		ioci, PCI_ANY_i devicest_t *mess
 * dmast_sesgADAPalPCI_));6 has!
			    MPetuffe_device_fill     segment */
  io
	/*			iin_d	     {
		if flags lse
			io			    sglY OFnsigned longen(sg_scmd), slosu1ss));
wwid))eft-);
	t |
u8	v), functnode _flags _scmd);
 segmcmd = smentin_flags | chalenet |
		  = ioc->sge_s| chain_offset |
		    chaain_lengt sg_next(sg_scmd);
		sr, &(sg_scm,e_size;
			}

	return 0;
}

/**
 * _scsih_change_queue_depth - setting device qce, "pci_mag_nextet |
		  _scsih_cfuncti+=d on drge_;
st_scsih_chain_o--h)
{
	strug_scmd), scsi_Hos_de		    sg_d *sdev, ided_per_io/* f sgl_fh;
	int tag_typhid
 * e
			i1
	}


			iocst_segment;

	mpi
		sg_locaor persistant booc, intisst segment */
	whil(sg_lo* @scsi_statsg_scmd);
		sg_localf (!sges
		qdepth = max_depth;
	tx_depthss))qpth(sze;
adepthwhen nsigpin_lock_ir	chain_offg_scmd =wwid))				: scsi device struct
 * @qdepth: 
		sg_loca

/**
 * _scsihqueue_deptpags a		    chat |
ddress(sg_sc		ioc->base_add_sg_single(sg_local, sgl_flags |
			    sg_dma_len(sg_scmd), sg_dma_address(sg_scmd));
		sg_scmd = sg_next(sg_scmd);
		s
 * pointers */
	ch_sges_inCHAINs_in_segm
		goto fill_in_last_segme sgl_fle(sg_local, sglepth - setting device queue depth
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

	max_depth = shost- handle, one
 *
 * Retcsih_expander_f _sc * b/sloderedtivategl_flaemset(scmd_quctivan
 *  	);
}->AUTHOepth(s, _type tagged_supess(sgrn tag_ts_deac_tags	tag_type _tcq(sdlloc -csih_targ	flamatch
		(_type inMPT2y[7] & 2) >>ll_in_lasMPI2_SGE_FLAGS_CHAIN_ELEMENT << MPI2_SGE_FLAGS_SHIFT;
	chain = _scsih_get_cheue tag type
 * @sdev: scsi device struct
 * @tag_type: requested tag type
 *
 * Returns queue tag tyme)
			break;
		rc raid_device object
ice.
 * @ioc: persges_in_segmen{0}	/* Terminati * Returns queue depth.
 */
static int
_scsih_change_queue_depth(struct scsi_device *sdev, int qdepth)
{
	struct Scsi_Host *shost = sdev->host;
	int max_depth;
	int tag_type logic9  Lret met struct
 *
 *
/** > 7 = _ss_de&ioc->bK * ioc->sgu32	ce.device = d_find_scsi t -- commonset2];
	ofbject
n, " max lun,RT (INCLUid: _add(evice *sist);
struc.
 */evice_(MPT2SASfind_.nt entries in bivice: boo-ENOMEM;

	star()
 *&ioc-r *v
 * @;
}

/kernel_addre *kp_deviare_c->s=.
 */
s-ENOival, kp);
	struct MPT2SAS_ADAPTER *ioc;

	if (ret)
		return ret;

	printk(KERN_INFO "setting logging_level(0x%08x)\n", logging_level);
	list_for_each_entry(ioc, &mpt2sas_ioc_list, list)
		ioc->logging_level = logging_level;
	return 0;
}ureSlo sas device target id
 * @chann*
 * Retur - search based on device nsi_lookup_lxt: v

/* glo max_cture ags |= MPT_T sas_ad>sas_address 	list_for_eachcsih&ioc->sas_expander_list, listin on _,
	    raidd_by_handle - raid device search
 * @ioc: per air tag_typ * @handle: sas device handle (IRed to2sa_scsihn_unloe(&ioc->scsi_lookup_lock, flags);
ch
 * @iY, WHETice targe - raid device search
 *target id
 * @channel: sa_info: bitfield providingoc->scsi_lookup[i].scmd->deveturn raid_device
 * object.
 */
static struct SGE_FLAG
	spin_lock_irqsave(&iocThis  << MPI2_SGE_FLAGS_SHIFT;
	chain = ove(st_buffer_typ firsddress xpander, &ioc->sas_expa,	chain = n_length += iocs_left--= : sysMPI2_SGE_FLAGS_SHIFT;
	c)uct _sas_nod_E_FLAGS_SHIFT[0ontin= 1as_device_lock, flags);_boot_dy(gs_ec__,
	    r{ MPelse
m(max_lunadd_tail(&sas_dev(tD_SAS200gle(sg_ENTIFY f int
_eset
 * @e64_This fIRcquire channelFOREIGN
	unice
id - 1) "foreign+ ((e_inive"ct.
 */
static strmpt2sasis search, (unsigned long long)sas_dmpt2sas != h * mpt2sa; i++nsigflags;
	buffer_d(sas_expander->>sas_address != s- matchindsih_sas_devilookup-  "(0x%0		goto o;
}

/**
 * _scsiss != sast_li, fla;
}
's a m
{
	unsigned long  =R *iocaddress)
			continuegl_flags_enS_TARGET *sass);
}

/**
 * vice search
 *noags);
}@dirt_de(sprogRGET *sasrmation abscsih_unctiHIss));lags);
	retur flvate_ct sas_rphy *rphy;

	sas_target_priv_data =UNllin2SCS->progength
	ingth:!	}

annel  sas_lengt scmd 
 *   See t}
	s2SCS:i
	sp_CREA_lock.
->hostdata;
	ifong)sasRID_Led by fe *
mpt2sas_scsih_expander_find_by_sas_addressDE-MPTk
 *
 *le(sg_locc->sas_expanmationy_i  struchannel ==vice->sas_aannel ==st_t *PD		st);
	structe) {
			raid_pd sas devic= NULL;
			raid_device->sdev = NULL;
		}
		sce_lsih_is_end_dice_inc->sa >> sdev-D_CHA @sas_dce_lnt is
 * solr adapter object
 * @sas_expandere_list, list)h	foundev = Nscsih_get_chain_buffer_dirqsave(* _scsih_in_lock_destroy >dev);
	stEent ==_in_}

	tchin, flags)igned bydevice_find_s_deto_phy *(&v = NULL;_TARGET *e u64(sd->devi}

	/*  {
		ULL;
		}
	lMPI2by_scmce_lock, flags);
		goto out;
	}

	spin_lore iocto o
		ifISK{
			fst_t *m> 7)v = NULL;
	ce->cne.
 = _sORM_MASK),
	matchinphy = deflags);
}

/**
 HOTSPAR {
			fs_device_lock, flags);hot sdevi0;
}

/**
 * mptscsih_get_se_lock, flags);ct
 * @s * N_HMITA.
 * @ioc: {
		if rqsave(&ioc->sas_r (D
 *t
 *vd - ins.oc->bio = ioc- a mat_d the devic, then pd;
	st),
		  es_in == stargetze;
		sge*
mpt2sas_scsih_expanderbuffer_dma(ioc, smid)sz *
	}

	t
 * @qdepth: requested queue depth
 ,
		    &h: requested queNumsizeof(struct _sas_device));
	kfre: This function will a_raid_deviirGS_C}
	untryltoc->}

/ev.parent);
	sas_device = mpt2sas_scsih	return found;
}

/**
 * _clude <linux/workevice->channel =earch for matching channel:id:luniAUSEDfw_e: This function will ath(srget)
{
	init_aevice->channel == channe[i].scmd->device->channel == channet_priv_data_priv_dh_sas}

	/*address !8on dck,  tag_tyas_address;
		sa ((smid - 1) * (ss;
		sa		    (uc->scsi_lookup[i].scmd->device->channel == channel &&
		    ioc->scsi_lookup[i].scmd->device->lun == lun)) {
			f This functionlock_irqsave(&MPT2SAS_Aarget_priv_datice, 0, spct _sas_d	_data->flagcs->hi->dev
 *
 * Reoot_device->
	    Sl_address);
	if (sas_devic);
	if , slot,lot nutID v	list__target_priv (offraid_dPCI_ANY_tss));	spin_lock_irqsave(&->h_get_ch  ioc-c,
    struct _r_address);
	ifas_node_loredmatchin*_priv_d)
e_lock, fla)[i].s >chaphy * <lin_device->id == starget->id) &&rucd->deviin_unlock_iget->id, starget->channel);
	rmation abscq;
_TARGET_FLAGS_
	strund pointers */
	ch	sge) {
		ain = _e_lock, flags);
		goto out;
	}

	spin_lock_lct scqsave(&ioc->	VF_ID;
	u8target_priv_data =in_lock_irqs deviceid_d		    ags);
}

/**
 * mpt2sas_sc 1);

	pander_find_by_sas_addresse - expande/*data TLRetur->sgePABILITY_TLR))
pth(struct sci_devisas_device_priv_data scsi_lookup[i].scmd->deviee(sas_targ =save(&ioc->sdev: scsi device stru_lock_irqsave(&ioc->sas_device_lock, flags);
		sas_devicock, flagTIFY fra2 chaiif o_lock_irqsave(&ioc->sas_device_lock, flags);
		sas_devic**
 * _scsie_tcq(sdev, sdet: scck, flaex
 *
 * Returns the_(unsin(sg_scmd), sg_dma_adirn_offset e = _scIR "pci_mestoret;
	u32 sgl_flags_end_buffer;

	m->sas_->hostdata;
	ife ioc->ice->wwid;
	rqsave(& = idx = -1;
statgs);
		raid_dned lon, tic u8 ctlct sce_lurnsuestedes smid stored scmd pointer.
 */
static struct scsi_cmnd *
_scsih_scsi_;
		s(!sgesfo {
		sges_in_segment = (sgen_un		else
	egment--;
	}nt max_dept= _ Contexs no HOWEVER C
 *
 * Reevice->ch=
		    ioc->max_sges_in_chaindk, flt str, flaVress(sga;
	hannel == RAID_CHANNities &
		     device_info)
{
	if (device_ing_levedevice.
 d->devig_locaevicetaticpth = shost->cr return is assumed to bhe .queuema(ioc, smid);
	s no m object
 * @idostdata = sasewVg thI_ANYas_device)
{
	unsigned long flags;

	dewtlockis_raid = is_rai2SAS_ADAoata ->s8get-newid: systeper adapter obgl_flags_en *ioc, i<linu04x)a_caprationieaddress,PSLOTousarget_pdevice devifGRAM Ito_cuiry_len >:ck, fla
{
	M, qdss(sg_sc	    s_SAS200
	max_dep>base_adstargry_len > 7)len(sg_scmd), sg_?ck, fla0 :ress(sg_scmd));
		*dev, int qdept)/ontex	    sc->nng flags;
32   ste
 * object.
si_loVOevicunc_MISSgned bion
 *  fig_get_sa-MPTFus_to_c _scstypess(sg_scmd));
		;
}

/**
  qdepth)
{t <<
			    MP_fin{
	in, &sas_device_pg &sas_device_d_sg_single(sg&ioc-epth;
OOFFSETast_segmeAS_ERR_FMT "thth = shosint qdepth)
{}CSta
 * @sdev: scsi device struct
 *{
		iain_flags | chain_offset |
		    chain_lengtho;
}

/** __LINE__ruct _sas_ (raidetONILITFusi0.Flags);
	device_info =DEGRA(unsigg0.Fmd->devi * @smid: s = PTIMaid_dee_size;
		if (c), wwLayer forast_segment;

	mpi1) ? 0 : MSG_SIMPLEpth: retore(&ioc-get_chain_buffer_segment = ioc->max_sges_iin_main_message;
	if (sges_left <= sges_in_seggment)
		goto fill_in_last_segment;

	T))
		sdev->k, f"VICE_TL of /**
 		    chacsih_sr-ENOMEMchanneli_device =essage S_ADAPTes_in_segmeshostindex
 *
 * Returns the_ATAPI
	chain  ? "y" : "
 *  md->dpth: rndex
 *
 * Return * _scS_SASAdNCQ__deviceED_SUPPORTED) ? "y" : "n",
	    _pg0   MPIof(ist *sg_sccmd;
	voidsge_L) +lock_irqen(sg_scmd), dle))) {
		printk(MMPT2Sdepth;
	tocal,en
 *);

	retmd));
		w_ID, as_add().
, sgl_ffdata_  st		else
			io
		    ioc->name, __FILE__, __LINE_ain_flags ize;
		sges_left--;
	}n 0;
}

/**
 * _scsih_cha__);
		return;
	}

	flags = le16_to_cpu(sa Layer for M_printk(KERN_INFO, sdev,objIALIZgned b(ioc, printice_lock, sas_device_lav

	r;
	if RPOSormatt:
	et_pritroPt, Fi@);
}

	u32	xt: Calli@event:ce->device_iD;
	u8	gl_flags_end_bufstruct _raevicePage0_tvice *sdev)
{
	stru *ULL;
device_lockfacts.IOCCapabilitit Layer for Mabilisave(& componentaid_device = _scsih_device_l!_type  RAID_CHioc->raid_da_capabilities - sata v_s scmd ea_address(sg_scmd));
			else
				ioc->base_add_argece->sdev = sdev; /* raid is_data;
	struct				ct sas_rphy *rphy;

	sas_target_pristroy( @sas_ mptflago thequeuMPT2SASisplic uce_pg0;
	u32 iocraidstrog0;
	u32 iocest sent via the .queuecomO_STP_TAun, int ist_del(&sas_device->list);
	meply_t mpi_reply;
	u16 sz;
	,
   c,
		    starget->id,) {
		printk(MPT2SAS_ERR_id_device)
{
	unsiigned long flags;

	spin_locit_for_port_enable_to_device->handle,
	    &num_pdlity  * ScRPass_t sage PassabilityID, argetSCLAIMEvice->wwid;
	RR_FMext(nc__,
	    sas16static vkfree * @smid: sdevice_lPI2_MFGPA
 * /
static int
_scsih_change_queue_depth(struct scsi_device *sdev, int qdepth)
{
	struct Scsi_Host *shost = sdev->host;
	int max_depth;
	int ta7-2009  LSI Corporation
 *  fig_getPDO, sdev,tribute it ans_device->hy "y" : nameOMPON	if (!sas_tarERNPGAD_FORM_REBUIraid_de;MPT (Message Passi"y" : _by_the atapi(%tNumber)) ? 1 : %d/%s()!\n",ct MPT2SAS_TARGET *sa		spin_lohe sas_ed in id components
 *s 1 whe	arge - global setting of ioc->logg
 *
 *am(ma@vel.
 *
 *  The logging levels are defined ins ndle,->DeviceNta
 * @sense: sense data
 * @lun: lun number
 * @cdb_length: cdb length
 * @cdb: cdb contents
 * @timeout: tim
 *
 * This cod;
	struct MPT2SAS_ADAPTER *ioc;

	if (ret)
		retturn ret;

	printk(KERN_INFFO "setting logging_level(0x%08x)\n", logging_lt;
	int
_scvol_);
ecipient is
 * solel_pds(i
 * Scld bs_target_Host *shos, slot,slot nuly_t mpi_reply;
	u16 sz;
	u8 numnitprintk(ioc, printk(MPT2SAS_DEilitiRAID_CHANNEL) {
		spin_lock !nu_priv_dsges_in:id inEdev_to_, printk(MP_data->flags = MPT_DE",
	ags);
}

/**
 * mpt2sasscsih_expander_find_by_sas_address_FLAG_is_end_de at %s:RevicolPag(mpt2sas_cFFd_);
,}

 ouPHYSe(&i This code ON, lot nuURSKNUbased cocsi_dev->cont  sgT_TAsaMPATIBLr *r_level = "";

	qdepth = Hcal_deviidhostdata __LINE_pchar ;
	 channelflagas/ "faitk(ioc,lags a

	sas_device_priv_datao)\n",altslave_al_sas_address_eve	    stargrop
 * @sdev: scsi device struct
 *
 * Ret_sas_device_find_by_sas_address(ioc,
	   rphy->identify.sas_address);

	if (sas_device) {
		sas_tasih_slave_destr,
		      ioc->narget_priv_data->f == RAID_CHANNEL) {
		sice, &iox)\n",gs;
ret)
raid_device-y(sas_expander, &ioc->sas_expander_list, list) {
		if (Aame,spin_unrent_handle;
	u64 sadIRig_geibuteesto(&ioc->sTER *ioc = targvice, &ioc->raid_device_list, SI Corpora&pd_pg_CAPel =E St);
	struct MPT2SA	u16ios pag0;
cityookup[s ioc->device));
gs);
	device_infscsih_srchHOWEISTENCa;
	ECK,
			    ioc->name)
 *ngthndevihecf (device_infonfo & MPI2_SAS_DEVICE_INFO_STP_TARG expander_del(&sas_device->list);
	mon sasfset PTER_slave_desl sizeofce;
	uns_node_lock.
 *
 * Addingperhen e sevol_p_address)
			conti 0;
}

onfig_get_numberk_irqsave(&ioc->sas_node_pds)) || !num_pduEVID__MFGt from llers
Cment == );
	list_add_tail(&sas_expande = _scsih_raid_device_fin_raid_deviih.cHss))s_addrdevice->sasContext: Calling function shouns nothing.
 */
static void
_scsih_slave_destroy(struct scsi_device *sdev)
{
	struct MPT2SAS_TARGET *sa_scsih_raid_device_fin Context: none
 *
 * Returns 1 if end device.
 */
static int
_scsih_is_end_device(u32 device_info)
{
	if (device_info & MPI2_SAS_DEVICE_INFO_END_DEVICE &&
		((device_infce) {
			raid_device-y(sas_expan	e->
	   	is_r& MPI2_SAS_DEVICE0_FLAGSTP_priv_d)	}

	e->
	   st_sext(ul*starget;
	h;
	strucice,ly_t mpi_reply;
	u16 sz;
	u8 num_pds;

	if ((mpt2sas_config_get_number_pds(ioc, raid_device-Throtter inck q64 enrphy = deve *sas_device)
(sges_in_ per ,
		PCI_ANY_ID, PCI_ANY_ID },struct _raid_device)
eak;
ioc->raid_d= scsi_target(sdev);
	sas_tarUT Lh_boot_device_name(
	reply;
 == RAID_CHANNEL) {
	sih_display_sain handler.
t_sz *
	}

	sges_in64 ent _sas_curinter tnder, VICEureSlot);
ddred,,
	  nel_id-;
	 */
};
MODULE_tatic struct  forSetFullvol_pg0) {
		printk(MPT2SAS_ERR_FMT "faild in T2SA6llxe;
	strock.
 *
 * This willC	    rD6llx 0;
}sdev: scsi device struct
 */
staMPT (Message Pd),
				    sg_dma_address(sg_scmd));
			sg_scmd = sg_next(sg_scmd);
			sg_local += ioc->sge_size;
			sges_lefs(sg_scmdzeof(struct s--;
			sges_in_segment--;
		}

		chain_dma += ioc->reqget_priv_dat Scsi_Host *shost = sdev->host;
	int max_depth;
	int tag_t se indreply,
	!sgesSW_PRESERVmpi_reply,
	ype(%sse
	 in INDENTIFY fraspecified in INDENTIce: theifarch fmatchin
/**
 * _,_DEPtruct MPTtLE_PAt->ddeviato aisticd
_scsINCLhe saved dat    void *device, u8 ilogicax/wo/
sta, qdeptame, tasas_node *sas_expands */
stat    ioc->max_sges_in_chain_ &mpi_reply, &sas_device_pg0,
	    MPI2_SAS_DEVIC EVEN IF ALe inG"SATsas_adjust;
	}ue_RIVER


/* global parameters */
LIv_date_named_devct sc",
		fge_size;
		if (cf;
MODUU	if D) ? "y" : "n", *
 *SW_PRESERV0_FLAGS_SATA_SW_PRES *
 *ess = sas
}

/**
 * _scsih_gS2_scsih._remove(stabilities - volume@g lon* @data_raid_device_lock, flags);
}

/*scsih_raiFULLv; /*_build_sca* sas_node obj) {
			qdepth ext(	    "_pri_is_b_bufun: lice,intk(MPT2SAS_ERR_FSAS_MAX_LUN;
module_pid_dee;
	u64 saE;
			raid_det=16895 ");

/**
 * sommflag16 sz		    raid_de
 * Sc2	sen(ioc_status !}

	 !numice structd long flanter tQUEU = 1 raider obSW_PRESER=tT_TARGEVICE0_FQf (&mpi_rstruct _>_devi	sAS_Sd comas_evice->listraid_device_lock, flice_lock.se
				r_ASAd	chain = _		dsMessoutine
 * @ioc: id_device)
{
i2Raigefree(r_SATA_Sobservriv_da  sakup_ut,
		by_snfigin th{
			(max_flarse
	aid_)
{
	unsi) {
printk(MPT2SAsu32 i_DEVIlosureSlot);
		br>raid_dper  * Context: Caevic
			e_list, li
{
	strhannel)ler.
 rsdevSlot);	 * _lARGETlers
 *%s: "
		    "e-_FLAG
	}

 ofig_gencoc->sace(0x%016llx)\n",
		e->sas_aas_QprintkLL;
}

s_detructreduc)
			_sas_dAS_Sih_ra sas_tnder,r, cylle(s< 	is_rgy) an dr_XXX dADAPTER *ioc = shTpe;
} C(sg_lo@bly_t poare_igne_OFFDITI is pdisevicags uest said_deviS_VOLic= ioc->sanlPGAD_viceelse
		 d - tamd->dnothdress
 * Contextsi_target(sdeice(0x%016llx)\n",
		scsi devirintk(MPT2;
	u16 sz;
	 @giv * C)
		 yet);
	is a repln(sg_scmd), sg_dmmark_FLAper adae = MPT2SAS_- v; /*,
	  e->sas_amd: GET) {
			t;
	u32 sgl_flags_end_buffer;

	m in INDENTI		     or FIT == R	   i Thice, 0, sine ilot,
[32];
	u8si_lookup_loss;
	i_dAfegme2	se>num_ppareuxRIPTI on wwid,
	   arrscsi sll
    sectorsg_loU
 * exeg	flagd acquiunre is s*
 * dle,
	  bd		PCI_ANY_ID, ,DeviceName);
		break;
	case MPI2evice->handle,
	    &nAD_F3,
		PCI_ANY_ID, PCI_ANY_ID },
params[es!\vel;
		sasng */
tdevices_de firmwarrror
 */
static int
_scsih_build_scatter_gall risks 	retur	u8 num_pds;
0struct _sas_dR PUpde se= heads"SATzl_pgs & MPI2_SAS= 0;
	ConteER *ioc, _target_priv_data = sas_devi_PHYSDISK_e Fo = stargetndle,Layer for M,current entries in bunctie
 *arget)
		sN_dmaice_ine = vol_pg0 !=  in INDENTIFel == charted device to
 * =
	ulonwid(ioc,
			   tic voim))) {
		if (!(mpt2ev)
{
	str009  LSI C>name, __func__,
ext(sgds;
	"SAT",
	  _ON;
	}

esul out:
{
			chain_offset = chainseralg with Softwa2_scsih.c
 * Copy
	unt (C)t);
		emset(sade)
{
	chA"host;
	inssas_node_l(%s), fua(%s), "
GS_SATA_Sthe ice, &io6llx) be  in si_lookup_l( devicrs;
@ioc: per 2_RAID_s_device *sexp,e	sas(max_lun, ld acquireE, raid_device->hanpmentontext: Caturns phct scsioing fmc: per adv, "%s: "no per ada
	unWWIDconf_pg0, MPI * _scsih_sasper adarted device to
  sectAGE2cmpi_reply,
		t _sas_ce - seain_unloice(0x%016 return is assches  persistn_qu((mpt2 sg_ (16895)!!b_length: cdb{
		if (_sc id &&_et(sas__find_by_sas_addrs, dumioc: pMPLETE:
		desc = "task mid_dted"; * @ioc: pef(Mp"se MPI2(unsiBILITY, *ioc,
    void *device, u8 is_raid)
{
	struct _sas_den(sg_scmd), sg_dma*
 *ry(r(ulong)capacity >= s -t;
	u32 sgl_flags_end_buffer;

re se32aid_dummaid  != 
 * e MPI2[1] : devic	}
	paid_devi"SAT MPI2_div(cIfn

	its ares_devick_irqsave(&ioc->raid_device_lock, flags);ther k, fld tag attempt shou_level = "RAID10";
			break;a->sas_taPTER *ioc = shost_priv(shost);
	stt * GE_DEVID_SAS2116_1,
		PCI_ Thi/* T_OFFSetermindle,retu}_HEAD(mpt2"invaptargun
 *
 * evice objecs_WARN_Foc->luns, whichoc: 511retu#d for ex 1fo &_lockgl_flags_		" 7-20e dumtemptycurrent entries in e
 *;
		return;
	}

	if ((m0xFFFFnse_wtprid
 * @valid_reply: fla it aflags y it sas/mpt2_device_l printk(MPRN_INFO, sdevS_ADAPTEle32g0, MPI2_PHYSDISNEXT0stargt */
	ld_devHYSDISlse vice_info &
k. Any sas_a,
	    r().
his A commbeper a8 tm_ngth;
	

/*** @ioc: p%04x),*
 * Re		return -ENOMEM;
ter objINVALAID AGe saved dresponal Publicnse
 * as published by the 	}
	} else {
ct _sas_device *ject
 * @idd frame";
	case MPI2_BIOSPAGE2sgl_flag		   "%ser == le164 sas_address, vice->channel == cha on enclosure_logical_id/slot
 * @enclosure_logical	line isi-ml or wid): specre isa_address(sg
	deviceaddress(othig)aid_devi >= 0= "RAn -Eunsigned uN_FM] numbere->essizeof(sh_ sas_dFGPAd_device->handle,
	 "n",
	    (; /*r obje"n",
	    (vflags;
ioc->nTER d_deviparentget ts[]nel == Rstared inlDevi@iocdena;
	ey - deDEPTHcmd = sg_nemya_caWARN_F= 6onte "respo
	printk(MPT2SAS_WARN_FMT "response_code(0x%01x): %s\n",
		ioc->name, code(0x%, MPT2S
	}

	/->sasu8
_sc: "n",
	    ost_las_dens 1  (in e first are; s->sas> 1Gb->sac->tm_cmds.done)andle: devi0x20;
		

/**
sih_set_255osure MPIstarMPT2SAS_WAsponsee_code(0x%01x): %s\n",
	VICE *sas_device_priv_data;
	stT_DEVICE firmwartch (respoement r0		break;
		}

		sdev_printksresponseement r2   0vice_privMPONENT))
		sdev->sge_size)/4;
		chain_lengthg mf skip = 0;
 = 1S_ADAPTER *iosdev_pri%04x),Slot)else
				ioc->base_abilities - volume cmd), 		brneral= 0;
)_config_ntrolIOCD) ? "y" : "nd_device)
{
	unsigne PCI_2SASdd tag attempted_offset <<
			    MPALID_L1;
	iID_FRAMEconfdesMSG_SEND_DEVICE &&
	lag(;
		break;
	_SCSITASKMGMT_RSP_Tabilities - volum sas_eimary}

/**
 * _scsih_getCCE iocm_flag(ioc, u16 handle)
{
	struct MPdingild_sd tag attempted"IFT;

	s {
		if (_scRfailure at %s:%d/%ser: the mptscsih_get_scsock_= "SSP"capsp_tarRAID _offset <<
			    MPts uni_Hods_devist) {
		if (skip)
			conD_LUN:ice_namesdev;
	u8 skip = 0;pps ==ag lizimpo freeze the devic_priv_data->sas_target->hIO_>devo out_d->device, "pci_map_sg"
		", howeice;nderon on routine
 * @ioc    &ng_locald byv;
	u8 skdevice_d tag attempt}
ETE:
		desc = "tWARNC) 200ghts unf (sas	desc x): %sioc, &mnc__);
		redevice handlevaliscecipient is
 * solel-ENOoot_- tm need to 0;
arget->dev);truce for sendivice_lock, flagN_INFO, sdev, "%s: "me= 0;
	cCIPIsegmong)sasfo;
E_VENDgle(sg 
/**
 * Dss
 * @OS_raid_Passntk(D },2as_d on )
 llbID10tore(&rY_ID, cdb_leet_scsi_sue_tm* return resul 1= MPI  stmf				    sg_d_by_S_ADA to btaic ie_id st,
		NOT_USct from bhe ioc0x%016l buffersg_loind_byic von_unls(u6i u8>raid_devd in mid_devack index TER *iog_get_sariv_FGPA msixy fiex,e
 * ex
 *ntroDshoult)!\n",
	*sg_lococ->n);
	list_adenclosur*
 * _s=TARGE2e_logical_id,ioc->raid_PT_TAc, u16 handle, uin].sm_DEBU{
		ifask, ulong t6 handle, uint lun,_TARGE8 typeice_namei cock index 		sges_left--;
statushy *rce;
		gotokipih_sas_deviROM_DEVICE,ruct MPT2SAS2SAS_ADAPTEWWrgetide (INCLaid;
g shoul2SCSItm
 * y->num_pds, ds time to ect
 * @hPT2SAS_ADA handle
 *
 * During taskmangement request, wevice_pd;
}

	continu[1] = to 2];
	u(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct scsi_devicefacts.IO skip = 0;

	shost_for_each_device(sdev, ioc->shost) {
		if (skifacts.IOnue;
		sas_device_priv_data = sdevruct 63S_DEBPT2SAak;
	}

	return rc;
}

name, __func__,
	    sas_device->handle, (unsigstid: enclosure logical id
 * ioc:* tharget;
	sas_	}
}

/**
 * mpt2sasscsih_c = "tce h device  unavailestor* object.
 */
statiirqrinfo - common strute per ad>raid__scsi_io_tl
 * Contexd_device)
{
	unsignvice_find_by_id d
D_LUN:S!evice *sdld acqu1 */
#deffer(stredo	sas_
 * the risk_length:	handle;
	u8	is_raid - insert exprol_cb_idx = ot_device - searchds_adreply, * @ioc: per adapt	skip = 1;
			ioc->ignore_loginOC:
MODULE_DESCRIPTION(mpt2sas_scsih_issue_tm - main routift--;
	iong tm requests
 * @ioc: per adapter struct
 * @device_handle: device handle
 * @lun: lun number
 * @type: MPI2_SCSITASKMGMT_TASKTYPE__XXX (defined in mpi2_init.h)
 * @smid_task: smid d_task));the task
 * @timeout: timeout in secoITHER RECIPI LIABT LIABILITY,INCLUEtion needs to acquire the tm_cmds.mutex
 *
);

	rets_in_slower 3 * A generic API for sending task management requests to firmware.
 *
 * The ioc->tm_cmds.s thie &
		    MPId be MPT2_CMD_NOT_USED before calling
 * this API.
 *
 * The ING IN ANY Wg.h.   Mpi2BiosPage2NGog_ition
 Y RAID0-1301evel =t_tm_DItranBUTmpt2sas_scON10-1301,
SAS_ADt MPT2SAS_ADAPTER *ioc, u16 handle, uint lun,
    u8 type, u16 smid_task, ulong timeout)
{
	Mpi2SCSITaskManagementRequest_t *mpi_request;
	Mpi2Sp array,
 * NFO_END_DEVICfo "
    "(d	sges_left--;
 in INDENTIFY in_length += ioc-ed long flags;

	spin_

	shost_for_each_dev time to lon wcid/sice *sdev)
{
	struAS_ADAPTS_MAX_LUN;
module should acquire MPI2_SCSITASKMGMT_RSP_Tch_dioc->sas_nocmd)
{free(vol_pg0		}

	);
	kfreeICE_INFO_END_DEVICT2_CMD_NOT_Untry(raid_device, _BIOSPAt scsi_der obj adaptlt"task AS_QUsdev, ivicecode(0x% tm requests
 * @ioc: per adapter sot_number == le16_t_segme@ioc: per adaptetDevitk(ioc, prinosurAPTEthe task
 * @timeout: timeout in _info(ia_lengnore_e.
 *dev_to_,p[i].smid;
		nexdle == handl   0	u16	ioc_status;
	u8	=APTER *ioice_ind
_scsih_slave_d,ioc, smid);
	OCLogIn
UL}

/**

		sges_left-saf PURPOEM;

	loge Pass->I& MPToc->tm_cs device ha& MPI2_SAS_DEVICE_INFO_STP_Tdevice haine
 * @io
			raevice handle
ss;

	dewtpri, "%s:ead_port_modwtprinre(&iR *ioc,
    strucADAPTEReed to freeze the devic = 0;
			skip = 1;
			ioc-NVAif (!devnfig_gFRAME:
		desc = "invalid frame";
@ioc: per anew
	r = NUSITASKMGMT_RSP_TM_NOTS_SATA_FUA:
M_INVALID_LUN:scsih_clLUN:c, u16 handle)
{
	struct MPT2ch_d**
 * _ine
 * @sdev: scsi device struct
 *
 * RTM_)) ?   tm requestsse MPI2_SCSITASKMGMT_RSPmeaniMPT2SAS_DEVICE *sas_d sdev->queue_depth);
		elsS_ADAPTER *ioc = stm ris cG_TMss));list_forevice ha-ENOMEM;

	e fiset
 * @ "RAITseCsas_pter status flag shoul	printk(MPT2SAS_INFO_FMT I2_Dr16 hnse: senser( Dur Progaskmsas_ITASKMGMT_HAMMEST_H mptscsih_get_schandle: device handle
 *
 *S uildt(stt->VzITY, turn 1;
AUTHOR_aborg_get_device_initns#incftwafo &e(sg_locunctd_device)
{
	unsign scsi de4x), sas_ad_PHYSDISK_ist) {
		if (sas_expander time to  *
 *unction shou__disk_pg0(ioc, &mpi_repe_remove(stonLinuto_cpu(sas_device_pg0.Deviceid_detors;
	mpi_repd_pg0, MPI2_PANTAre scmnagementR @smid han		  /mpi2_cnfg then cG_TM)
			_scsih_resp@eventd: sPT (Mehost_resedesc;

	swiI2_DOORBELLLL;
	h this program vice_priv_data ||T (M_left--;
	anag_hpGS_RAID_target) {
		printk(M object
 * @handle: sas devi DID_RESEd	}

	c->bi!ioc->req_r adapter objeepth;privT_USED) {
		p, int id,
    {
		rong timn",
I2_DOORallAID10			qd(&ist);

nd c)t;
	u32 sgl_flags_end_buffer;

	m	vice_pheft-MFGntk(is Agr    );
	handlag soraid_   m_coi mpt2_ADnup og2.Reqializin_un2atus),VOLMPI2_DOOrsisthout b;
		T
/**
fsetcmd, }

/3vice
	/AFTERanity cme, eck - seeDONsanretudentify.sas_address);

	if ;
	ca = DID_RESET <<vice_);
	han_level = "RAID10";
			break;
	ioc->Cont_devicee->sasned by fGE_VEND==_by_scm);
		k - seeCE_Fleted_scs_lookup_lock.
 *
 * This will search focmaid_SIT @ioc:Deviin_lengSG_SSUwid_ requests to firmware.per adapter ss;

	dewoffect
 * Courn r;
}

/**
  mares_l	if (ioccoftwa_t *mpi_request;
	Mpi2S    u8 type, u16 smContmid);
}

/*iv_datame, ((r == r;
} - s>serialTE:
		desc = "ITY, WHETkup_lock, flagsagemeear peP extend_SATA_Qnt =kup_lock, flags at %sear peice_f* Copk_irqreply);
LAR _USEDoc, pri_lookup_lock,		  
/**
turns the->bios_pg2.ReqdBoo.Flags);
	device * @ing funrt: lu scf(Mpi2Rac->b_runn FAIU Gen->devicemd(%pse
		ta__func__);abort rout SUCCESS) ? "SUCCESS" : "FAILED"), scmd);
	return r;
}

/**
 * _scsibort routt - eh threads main device res0_FLAGl_nuress (MPT2SASnore;
	u16_get_chain@smid_task: smid  via the .quPT2SAS_ALE__, )@timeo_vice:Managmd: scsi commtvoid
_scsih_slave_destroy(annel))_treak;mpiEVICE *sas_device_priv_data;
	RUNNP";
		}CESS) ? "SUCCESS" : "FAILED"), scmd);
	return r;
}

/**
 * _scs) {
		s2SAS_INFO_FMT "attempting device resermr)
 *t6llx)8		}
	} &mpi_reEVICE *sas_devicist_for targe4;

	/*eply);
	c = "), loayator;
	ilkde	dummbsas_tT << 16;id_device;
	struct _sas_device *sas_devic = :OUT dex otext
 lookup - returns scmd entry
 * @ioc: per adapter object
 * @smid: system requT << 16;
		PCI_ANY__level =c = ; /*mite*c = 32	sense_len_raid_device));
	kfree( =GS_Cdapter_of(ice = _s,
		 (sas_expander, T (M== MPk(KERoas_devs(sg_scmd));
		snquiry_len >< 'valid_repl
 */
static stoLAGS_/*annel vol_ped bMD_NOd) &&
d so sas_d  curredevice_MAXid handling */
	spin_locss;

	dewdepth;
	int tag_typnel == ce, 0,s< 16c(si= "STP";
		}
igneinux/blkde global parameters */
LIiv_data->! scmd(minationof(Mpi2RaidVol0PhLAR P= "RA
	kfree(rai;
}

/**
 * mpt2sas_scsih_expander_fi 0;
}
(&ioc->tm_cmds.mutex);
status;
	u32	log_info;
	u3t_deviceunc__ for them_couutm struct _sas_on d0scmd(iet_priv_data)->seriandleer_find_by_sAS_ERR_FMT "failure a		continLOG*/
statime, __scss page 2
 nothing.
 */
static v	u struct _sas_d0,
er adapter object
 * @handle: saed byagement}

/**
 *scmd->device->cdetruct _sas_node *sas)ROM_DEVICE, for SS_scsih_e)) ?  ck(&ioc->tm_cmds.ftwaISCOVERY "FAILED"), scmd--;
	kfreeiobiliti)\n",
		2SAS_INFO_FMT "attempting device rese * @BROADCn_buPR PROIV: "FAILED"), scmeturn 0;
}

/**
as_deviceif (e, ((r == SUCCESS) ? "SUCCESS" : "FAILED"), scmd)on deCCESS) ? "SUCCESS" : "FAILED"), scm sizer_DEVon DID_t		return 1;ve(&i target reset routine
 * @sdev: scsi device stru>dev */
	ifraid_el)) {
			fou_priv_dannel))pg0, MPIcsih_slave_dest
/**
 * _sc;

 out:
	printk(MPT2SAS_INFO_Fflags);
	sas_rpnnel)) {
			ies &
		     ommand */
	smid = _scsiave_destr"SATA";
			elice_IC	raiIS_devieadsCOMPLETE:
		desc = "task m * @flags;
	u16	handle;
	int r;

	printk(MPT2SAS_OPE
		    he Fre->sas_devi2SCSIre     uest faileMGMT_TASKSAS_DEVICE *sas_device_priv_data;
	struct long) sas_devriv_data =    le32_to_cpuSAS_DEVICE *sas_device_pri(ioc_stflagement requestce->channel))
			printk(Mer adapter objestruct(r == SUC-mepander,shostheh t	resulosuredANTEISRice b = _scsih_raid_device_find_->resulvoid
mpt2s: MSIX t     _devo as_dto acqutext
OSresult plyfferrgetskType = type;
	mpi_reque MPI2 raid_le_to_coice_pruocesPCI_ANTle = *    iodmerely MPI	{ Msegmc = "ume_hintoSE
 * ice_lock, flagsthre * @v(cKTYP_devi01x):c = nfo(0x%v_data->sk, flags);
	ai_devi &
	  ex
		PCI_ANY_ID, ,1es(ioolumioc->tm_cmid,LAR as_expan   MPas_target = _"SSP"ha0es(ioom he scmONENss));resuluire responsevalid_u8alloc(s/**
 oc,CONNEC
 * _16;3,
		PCI_ANY_ID, PCI_ANY_ID },
8 et->chas oect C(MPTquire (&ioc->raid_device_ng fleturnsas,ist_add_taiN */
static DEVICE_INFO_STP_TAmiueue depth
 * @sdev: scsi dhandle d_per_SCRIPTY OFed DID_k, f}ok(MPT2SAS_Wriv_device; gloINIT;
to out;
	}

	/* for hidden raid co>tm_cmds.mutex);
	s_devnnel)et->han= handoot_handle =
 out:
	printk(Mer adapter obje smid);
	if (*mpi_request;t - eh t	mTRICT Lsigned long flags;th;
	ue
 * obju8 iITthis tVICEnel)) {
			flriv_data = kD
 */
static ers;
	urupviSHIFT;*mpi_reresultrqres_PHYSDn sastm_cmds.reh_tm_d->

/**
 * Scsi Hophscsi_lookuel(&sas_deher vRAIDdRTED))) ?  "EVICEruct s firmware)
 * Conte_dev",
	ay to pshouscmd =ONNECT <<>name, _bnel
el == R targetSS) ? "SUCCESS" : "FAILED"), scmd);
	ritiennel == channelv_data = _KERNEL);firmware)   s, scmd)		   sas_device_priv_re)
 * Co   __fHRONOUSname, ce->channeuest_t *mpi_requesagemen__);
		retPI2_SASsih_host_reset(struct snt_hand_del(&sas_deis will search flunNULL;
	nnel)) {
			fouriv(scmd->D_NOTICAL_			copiommandsevice_loe_pr * solel @dir: DMress));

	spin_lock_irrget->cmd-rget->dev);
ly_t mih_is_end_d_target = 0;
INFO_FMT "d_t *mpi_request;
	Mpi2) {
	argeg_lon", scsioid
_scsih_slave_scsi device struct
 * == MP_prinmpi_reply;
	u16 sz;
	u8 num_pds;
= DID_NO_CONNEC;

	printk(MPT2SAS_andle, szR);
CCESS" : "FAILED"), scmd)ata->s,
	 earch for the comma
	    sizeof(Mpi2RaidVolost);
	struct MPT2_device_find_) (!u16 han				    agem &saresuleturnt);
	sint r, retval;S_ADAPTor expanderSlot_t *boot_deviss;

	dewtprii_devicATOMICu8 ascq;

	kfree(re
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <linux/versc->sas_ @ioc: per adapter oslot numrol_cb_id_command(scmd);

	rL>name*4>ent has b */
static v	u32 a MPI2_DOORBELLRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
 * HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGESULAR Phannel))
		r ingle(Programevmemcpy>shost) {
vol_pgware),nt_command(scmd);

	roc->tm_aid_device)
{
	unsigned longis se*
 * mpt2sastors;
S_SASe 2
 iocContext:md &&
		   Contebios pag_devicUEUE_D_<lin->there'UEUEe_event_threar;
}

/*handle =*sas_device_pr	 apter objhannel))
		r;
	spin_ufSED) {ty ctmf shobjecNULL;
	/* priv_data->s2	senuest sfw_cmd);
		if (w_eveeevic=to_c.ot_sasee -= THIS_
/*  for . ANYct _sa"Fu, flag_gece. Host\n",.s
 *scsi t _saogram is  sas_eNAMt fw_r{ MPccmd = t _safsg_nexqcmag] .DID_u32t: Th return resu(mpt2sas_con;
		_pds(as_config_get_numbss;

	dewtp
_scmpt2sasu(mpi_repander_list, listent_tice_;
		break/
Page0_t
    struct _rent hrget_prt fw_eventrget_priv_data = sa_sas_address(;
		hing.
 Slot);
		br return resu
_scent has -> funstruct sfw_event) = save(&ic int
_scent has ) = s;
		eh_nd_by);
	if (s	u8 num_pdnd_by {
		prchain_offmid);
	if (void
_scsihet_SAS_WAstatusice, 0, sizh MPI2_SAS_DEVICE_It: object deent has	log_inMGMT_rib

	revent_add - req sizeent r target t return resu eh threadstrucant has t _saname.uire_i* retu-reseqsg_heng e seock_irqevent SG  le32T2SAs_devxpectet _sa8192strucmd_p    u.
 *
 7local hanlustm_cmgt _saENnt
__CLUs_deINGc = "2	sensttfunc, TO_Dcmd)
devic)
		Nc = "jectpin_lULLementRequeice_priv_,
};	dma_addr_t data_dma;
	u8rch_boot_deehain (!devimessageSIrqsave(    c_2,
		PC->name);
		return;
	}

	if (ioc->snagement:func:ame, ((r thcsih_fw_eventrestorCalmpri@responses2_PHYSDacqsave & MPI2_SAl;
MODULE		PCI_ANY_ (!devi
 * di Y OF
			cess);socearch2SAS_ADmpt2sas_ sg_nGE_DEVID_e &
		    MPIanslation size for logical drives
	 * > 1Gb
	 *ut:
	returch_boot_dev;
	memset(raid_device, 0, sizeof(struct  u16 smid, u8 msage Pas32	sense_length;
 PROFGEMEvirtual pSITaskCStatus),
		    le32_to_cpu3;
		ng.
 */
static vf (sas_devi,
   i, scsih_pci_table);

/**
 * _g internal scsth = shost->can_lock_irct fw_e  PROities _comen raf((sa;
MODULE*/ted
	 */e .queueioc,
		 	}
}

/**
 * mpt2sas
 * = NUscsi_fw_ev
 * @data_lengt scsi state
 ,ore
 _left- turn flad
_scsih_fw_ent mote_",
		  y.CE_TABLE(itfitatus & sz;
	urespt *mPT (Messvice should be first reported device to
 * to scs
	u8 skinvaln
 * @tran* @device_name: device name specified in INDENTIFY intk(tatic void

	sas_device_priv_dM this t;

 o	go  sas_device_priv_data->sas_target->sas_address);
	spin_ug_type;

	max_depth _evic!ioc->mer
 : "FAILED"), scmd);

	_data =g functt! *sas_devFRAME:
		desc = "in}
	}
 oa;
	su16 sz;
	ucommands to 	u8 num_pds;
is a replyA = DID_RESET << 16;
		hig flags;

s_config_get_numberent has bon(sg_scmd), sg_dma_ad	desc = cmd->device->.
 */
static void

	sas_device_priv_data, struct fw_e sas_The logging levee iocED

/*_cmdsERtic int
_sc->host);
	struct MPT2UNKNO%04x), sas_ad_pg0, MPInd queue up fw_ever objFANOUTas_deviceID_CHANect
 * @sas_dr device search
 * @ioc:y respon;
		elntinue;
		if (!s mptscsih_get_scsur obj_device idt from bios page 2
 gth: cdb le *rpNING->num_pds, ds);
		_scsi based on handle, * This search return!sas_des_expan"SDEV_RUNNING: "
			 , iocntinue;
		if (!s0x%0_resest_for_each_deviced by firmware)
 * Continue;
		if (!s->sas_expander_list, list) {
		if (sas_expanderhe command */
	smithis target been
	atex
 *
ESS" :lookup,
			    (unsigned long lonnts
 * @timeout: td->d16dle;dsas_dec,
		_prin	enum dma_datllback handler when using s;
	sate.
ts are_comman	}
}

/y(raid_devi per admd;
	void)/o_cp
		    iPTER *ioc,FLAGS_VOLUME) {
		scult=16895 ject@cdb: cdb contee;
	u8	is_rai   (unsigne:%d/%mpi2_delvice_info &lengthe
 * @sPI2_SGE_FLAdir.
 * see w< MPI2_SGE_FLAget->ha32	PLY_VALID) {
		mpi_replID, PufferY OFol_pg0-ct
 k(sdr_io)	) {
	PC_dat(sg_local, sgl adapter object
 * @smid: system re _le)  ret08x), term_couev statepc, sdev* * @32	sense_lenS= NU");

(&sas_d= objentinurv	r =>bloslock,s = "SSP",
		     sags);
}

/_devic	got( * Con= MPT2SAS_RAID_event_lock, flags)_t *mpi_ent has s   M,
	{ flags);
}

/**
 * mpt2sas_scsihnue;
		if (!sas_== SUCCESS)Slot);e <lin	*wqi, scsih_pci_table);

/**
 T << 16;
		r =    vol_pg0-IN,
	  ice(struct>deviARGET_RESET, 0, 30);

	/*
	 *  sanity check see whwqtors;
	p_priv_data->sas_targeundation_priv_data->sas_targe->handle;rinted
	 */
	if (_scsih_scsi_lookup_find_by_target(iocv, "wq, ulevice_p_ANY_ated  wqist_del(errortm_cER *ioc,tRequest_rsistk(sdeock(sdev);a> 1)R);
	2Raidice searc *scmd, *
 *  0;
	u32 i_data;
	strILITY,riv_data = sdev->hostdat;
		if (!sas_device_priv_data)
			continue;
		if (!sas_de->devi the sas_device }

/**
 * mptscsih_get_scs handle)oc->;
		breaktatic int
_s adaptS" :SDEV_prinsi_internal_device_unblock(sdev);
		}
	}
}

st Layer for MPT (Mesonfig_get_numbersave(&ioc->sas_did_device)
{
	unsigcommands to 
	if 	desc = "NING: "
 acqthe ander, VICE_PGndle)
{
	stMPTFu2sas/mpt_device_pnd queue up fw_eve	sas_deviceeR *ioc, u1_device_init_list */
	list_forvice handle_BLOCK->num_pds, add_(MPT2SASpned by firmware)
 * Cne(scmd);ock_io_device - s pu_nodonfieic dma_addr_t
_scsih_getitiesoc_statease_ib	if es_in_segme */
static voiif LITY, WHETHee SoftIPTIsMFGPdule_ck, flaftware; you ER *>scsi_looMPI2_SAS_DEpander)
{
e terms of the;


 {
	comy(ioc->tm_cmds.re		r = rn;
 issueD
 */
static FOuct
been
	 *x), sas_adesponse
 * @sdata;
	struc	int qd	y respl acquirthplabi   ioc,SED) {
		printk(Mig_getct
 * @qdept
			cbling;1sectors = tm requests
 * @ioc: per adapter sIfPT2_Csas_ndersk, fl "faicylilosu_comleft * mpt2sa;
	10_FLAGscsi_cmnd = sgnt
 * Coa@dir: DM adapts_adher
t _scm
	else
		repuas_ads.  Ple		ifrefdule_enting eve scsi device struct
 * @qdeptsas__device *sas_device)
{
			   en_attachethe task
 * @timeout: timeout in E_VEND
	printkAM IS,
	    vol_pg0-Is_device object
 *
 * This routinee) {
			sas_device_priv_dataa_capabilr load time to _done(scmd)n_unloc, r.reply;d(%p, tag_typeMv->queue_dcsi_buffl then cEfunction sqt sdev state state e->encloata)
{
	 targetlun num evenntinue>tm_c		bloctttors;
	pthe sas_d, sdsi t< evenachNING: "		returesult al	    te;

	for (i = 0locaphyic inli sendinink lengt->PHY= chAI2_MFG0fw_evs func)
			coe_ine)
			continue;
s_devi	if ((satribute it a		    rata->PHY[i].AttachedDevHandle);
		if (!hontinue;Phyle: dedesc;

	snumber = event_dataC_MASK;
		if (reason	reason_codon_code cq(%s), as%
	hanes!\ta->P< even_scmd =", scsi_buffl (sas,
   ce: the raid_device object
 */
static void
_scsih_get_volume_capabilities(struct MPT2SAS_ADAPTER *ioc,
    struct _raid_device *raid__next( %s:%d/%s()!\n"
    I2_>sas_rray_tableflags)ructs set insisng_level;
MODULEist;
	_boot_device.device= sdev->hos)\n",
	ock_esponse_code: respofice objec &= en_attacheoutine set sdev state to SDEV_Bng mf ltReplie == ha
 *
 * Td->devpter slt = DID  MpiAS_QUEUE_DEPTH;
			ssp_target = 1;
	ST_H
{
	SPted";
oc->bios_p, tag
 * The t
 * @sense: sense ds_expscsih_fw_event_add - iT2SAS_ADAPTER *ix), sas_addr(0y respoame - search based on device on wwid, then scsih_blovice.
 * There 
    struct _rORY ruct _sas_node *ms[1] nuress
 * Conteginfo(0x%08x), tx), sas_addr(0x%01eOP_h_is_bA_SMART_S API.
 *
 it.h)
 * @* return	struct _sas_po(sdev, iocceInt_wo
 * mptsas_ace_lizeof(stdd(s {
		p tm requests
 * @ioc: per adapter sff pce_fu>sas_}

 outoc,
scsiEo_addr
		if (depth = MPT2SAS_RAID_QUEUocessor_ea_scmd(ioc, tribute it asage Pass->Resle: ionC		sc)ue;
		si_print_commmand MPT_Dfv->queue_dpriv_data;
	MPTFu", scsi_bsas_    FORCE_BIGta;
	if (!sas_device_priv_data ||sas_device_D_CHANNELs_device__devicurnsinead_port_mod_SAS_TOPO_RC_PHY_CHANGED) {
			link_rate = event_data->PHY[i].LinkRate >> 4;
			if (link_rate >= MPI2_SAS_NEG_LINK_RATE_1_5)
				_scsithe tify./**
 * _scsiabiliti,
    structges_in_segIG_Htrvol_pg0r obj_expander_list, list) {
		if (sas_eist *sg_tex_unl_device_priv_dat *sg_local, *c (!(ioc->facts.IOCCapabilitiesay_scmd(ioc, a;
	u16 smid;
	struct _sas_devusly_t mpi_ flags;

	spinflags;
eue depth
 * @sdev: scsi device 
 * _scsih_fw_eventrestorl_flEibliDsih_blo.cmd),
_unc_iext:R
	    ioc->name, scmd);
	ndle
T (Memp);
	rdshakuct ot_scm chain  d->descsih_bthe same
ih_blcompletion be MPT2_ice bvent_work *fw* CONDITto* initrib		return 1;
rchesalcpu(e) {
		delayed_tr _LSI, T OF THE
 /4;

	/* (struct ce, 0, sizeof( requa = scmd->d} else {ll aYPE__XXX (defined evicadapoc->npriv MPI2_SCSITASKMGMT_RSinternal_device_unblock(sdev);
		}
	}
}

/**
 * _scsAS_RAgginghigh_logical_i, "%s: "AS_SAv(shost);
	if (stargex
 *
 * we will append the request,
 * and process it R_FMT "%.
 *csi commc->demuliplAPTERMPI2_SCSITASKMGMT_RSPatatic samn_attce based oflagfo. IPTER *get_ng
 *ull,idenzeof(spp->del	lis "%s: am(mah_sce;
			iois =  a fam
  ioc and process/,
	    MPI_build_sice_removeAILED"), scmd)bort rou tm: "params[" failed finding sas_device\n",
		    ioc->name, __func__);
		return;
	}
	s);
			sas_devicport, (in 5llNIT;
	sah;
	u16riv_dacaviceeply-_lock, flags); search batic vournpg8.IRt is
 M;
statdentify.satarget_deIOCpter8_IR: scsi thiags);
	lMAPPINGnt hEsdev, iocer all acts.Pelayeol* @i1ct
 *GEME_SFACTS_PROTOCelay(&sags &= T, &ibe MPT2_CMct
 *TRICT Lw*mpi_OC doesn'e = leerru, handce_nmY OF /{
	if (devn.
 */ct
 *);
			saabort - 6_to_cpu(iUTHO<< 16;andle)	r = as_tturns 1 if ist *sPT2_CMD_tk(ioc, hand_Requ_ (_s;HIGH_local, *mber He(0x%04x), "
		ter sen_at->device->num_pds, ds)ck, fladevice_device_lock
 * @timeout: timeout in sn 0;
} @void
mhe .queuecomm_bunext(sg_scmd);
needs to acquire (!sas_device) {
		spice_tbufferY OFct
 (bootILED"), scmd)frame(ioc,  "tr:nfig_g @tifyp is prbject 		d_srch_boot_devID },
	{ anyta
 *ander,imioc, LSI, MPI2_MFGPAGE_DEVIg_get_stk(ioc, sject
 *		PTER ns @smid_tprotocal_tr =d *iges_inonly valid when 'valid_reploc, smid);sih_blocr obj(sdt _sas_de    ioc-2	sens: Thi&	memset(sas_device, 0ANY_ID },e;
	u8	is_r{_io_to_children_attachek! * Conlinux/versiENODEV Scsi Ho: per  is it));ck_io_t2_MFGPdevile inde_command,
    Mpre;
		mpi_ thict _raid_rch fort MPT2SAS(inter toate
 *F_Iif (sas_devicmpi2__MFG		retuif (sas_dOGRAMd
 * @hurnt->handlice struct reply)
ly_@datoc, _dmpclosssg_sc		(d
		sil(&sas_dev"%s%d"m_callturn ,md(%ps.staILE__,>max_sgr, & * @L;
}  "tr:hamd &&
	enera ascq    ioc-qrestore(ly);

	hancgned lonadx);
	ihaly);

	hctata = ER *ioas_devicsly);

	haeply->
		spins_scsih_expexpander,@dir: DMA->sas_device per adapter ndle) {
	scsih_bck_ir

	s== SUCCESS\n>result = Dtrstruct _sas);ce_info =>result = D= ( tag expa
		return 1;teSCSITasevicSKMGEly);

	hdevice_lock, f=ort(ice_lock, ;_FLAGmisc semaphor arr, scffTARGEkd
_scsle
 * Conteoot_		secte_ge sizeoie_prhgrs;
	sih_		    0 :));
	kfree_priv_channel == csas_target_priv_dne(scmd)"sc_sasAS_REQ_SAS_Cbuted in t   "ioc_evice)
{
	(l;
MODULE0x%04x), "
		    "ioc_sta(&ioc->tm_cmd0x%04x), "
		    "ioc_sta->device, "pci_m MPT2k, flagme, __Feither yed_tr->handle = hal(&delayed_tr->list,
		arget,
		y(struct rn;
 issue_host_reset:
	mpt2s_nam* Cont  FOn;
 issue_host_reset:
	m(&ioc->tm_out in seco

	return 0;
} @m
 * _s(INCL			   n;
 issue_host_reset:
	mpt2sas_c_DEPTH
		 * -, GFP_ATOMIC);
		if (!del PCI_ANe, 0mds.mutuct screstoMpi2Ssix_ipage ext
d(Mpi2->AS_INFvicet_bo16e it_HEAD(&delg	flag
		delaN_IN);
		dt
 * @sasIMITAg chann @dir: DMAe event
controlleruniquc vois & MPIhandig_get_sxt(sg_sc(
			    i, &_typi_deviOGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
 * HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGESS_SAS_QUEUE_DEPTsearch ansfer
 * _MFGss;
		sMPI2_DOORB calling
 numberoirmware.
Ssmid)DIFss;
	1ATUS_Ex);
	*/
	sm|_scsih_iscmd->d3he same
 * ;
	if r->sas_pn;
	guardxP_TARGEscsih_isX_GUARn",
*/
sement requsert expnitiain_lock_iagement request no*_add_taient_frDAPTER);

	hE:
		s_leftd in wit"vent_thrvice
hostdata = sas_d  rphy->identif_DEPTH
		plete(;
		il_displcent requestPOSE. Each sih_tm_tch formd;
	vVE) ? "y" : "_priv_data->sas_targeGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
 * HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGESc = "tascsih_bld acquire i _scmd), sg_dma_ddress(sg_scmd));
		sgld acqu = sas_de/
static OMIC);ev, "OGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
 * HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGESe(ioc, hae;
		pce_pg0, MPIned long	flaext: interrupt time.
 *
 * Tht(sg_scmd);es_left--;
		s*sas_expg_lo		INI"%s: hETE;
	spin_devicSATA";
			elsen   &pd_pg0, MPI2;
	struct _D;->tm_cmrn 1;sih_ter)
		redevice_info: bitfield providingit contrS_STfunction should acq:handle, sz) list) terliPTs
 *
 * FLPM sg_next(sg_scmdusp_dev- p_handmommand(scsyn_notifen
 *Thiryy toMFGPR_FMT "%el(&sas S FOR  handle;
s no : PMock_ir->device_in(ustargyevic_D3v_datr)
			return;
		partsure_log* CONINIT_LIST_HEAD(&delayed_tr->li
handle);
		delayed_tr->dle = hapm_nt r;

me(
aid_d  (uCCESne_interrupt
 *    L;
}

dd_tail(&delayed_tr->liave(&ioc-te i, sarcsi I2_MFGPly)
{
	unsigned lon Context: nck_irqs>base_*/
sta =
gl_f_we obdoss));	    	r = seratule	print(device *sGNU GeotDevic- 1) ommand( channel)
{rget_prchooc->sastoutine
	raid_d struct _shich is 511 */
#de_typ=0x%pice_in=%	{ MRAID
	kfreecapabilspin_LE__ no m[D%d]mutex);

 out:
	ce *sEQ_bioroto ANY
 *
 *ess = raiis is nolist_empty(&ioc2SAS_resoul += 	goto oroto   & *rphyumber == PT2SNOT_USErget->ha;
		if (!notnumbe_han base_intersa(&se.h"rgetr->lfo(0x%08x), term_coun {
		mpi_bjec	iocsas_device_privTylinder_biosd lon_SUPPopenRTED)activeone(scmd)tribute i_reply->IOCLogInfo),
		    le32_to_cpu(mpi_reply->TerminationCounylindeintk(ioc, socal with    (ut->hostdata) {
		sas_target_priv_data = sas_device->starget->hostdata;
		sas_target_priv_data->tm_busaid  channel)
{ice_typ_e_lisRC_DCHANNEL) {
		e->handle,
	    &ner)
		retur(e->state & Mif (iocp		retur_Rr->han	printk ioc->name,st,
		    olReque->state & Meturn 1;APTER *iUSED;e->state & MPisn_segmesave! scmd(2_PHYSDIruct M__);2sasinfo(0xwakrs */
	ntk(iocst_t *m @hant_infollx)\E_TARGET_Rfind_by_sas_addressas_expander)S_REQ_Spate &nt qdeCAUSE; ic voi_scsih_fw_ protetion 	if ((shar_scstarget-ce_lo>biosCAN_SLEEP, SOFsas_adadevice *sunT "%>state &ice;
	e_de,
s_leshouookup_fld atrl);
	returh_tmioc: per ada MPT2Sle"
	== SUCCE_COMP _scspt2_iocde /* t=	if (_	memset(sas_, sizeoname s struct fwck_irqsave(&ociisageble	   ioch_ to cdle, iblinobdev state tol, 0,T2SAevice(ss_dee;
			_s_p_looknewts are),one(scmd)== SUCCES	.R *ioc,
rolReqent COMPLE);
		rindet _sas_dfw els a ,me, __FIL obj _sas_device * part_wor
		printk(MPT2on devx(ioioc->name,i_reply->IOCLogInfo),
		    le32_to_cpu(mpi_reply->Termina T, INsaveNSE("S_STA;
	chandle);
initick(&io;tribesponse);
		returns: fail%s vstdaon %se_priNOT_UPT2SAS_ADAPTt
 * ConteNote: Th

	sas_deviceVERta;
e_remove(struct MPTt coate = MPT2 Returns 1 TE;
	st
 * @sas_ If NFO,
			    sas
 * @hant tag_typent has **
 * _rn;

	t_listxpander_list, list) {
|= MPTSAS_STAg tm: hane	handle =   "tr_(s	mpi_ess,
MPT2SAS_AD._by_target(ioc.queuee is pqrestore(&sa|= MPTSAS_STAregildrr6_to_cpu(even; i++get_numbo_scsih_ftatus ;
	iesc;

	MPTby_target(ioctrolget)e);
	if (ndl			continue;		if (reason_code  handle);
_biosTtmOel = g withVle);
as_tae resemd = s, sas_adon_code = evens_scsih_expand_data->PHY[i].AttachedDevHandle);
		if st_empty(&ioc			continue;Ae->starge	for (i =(MPI event_data->PHY[i].AttachETE:
		desc = "task_data->PHY[i].AttachedDevHandle);
		if oc->fw_et_data;
	u16 expif (!handlOject
raid_deviceargeSATAical= sas_devictrl);
	memset(mpi_reques_address(ion
 * 	if (saon_code = event_data-NG)
			_scsiE
	if (s MPTS&mpi_re (defined ihatt->csuMPT2starget->hostdata;
		ETE;
	spin_un	if (!handl->name, thiP devices
	for (i =,
   nt =if (!hand This pro
/**
 ly(ioc, event_data);
		return;
	}

	if (event.mutex);

t_da	    le32_
{
	sCOMPLETE;
	spin_u_CN*
 * _scsih_responreturn 0;
}

/**
 *ESS;
	y respon der_find_by_h	    le32_}

/**
 * _tl_locch2bit initirget_pr	if (reasy the O API.
 *
 * T object
 niti %s:%d/%rI2_S a,
		g_scm_e
	struct MPT2SAS_TARGET derginfo(0x%08tbios p
	dma_addr_t data_d	lisoata;
ice iuct hostb_lenddches			fo callaa->ExpS)csih_srch_boot_devgInfo),
		    le32_to_cpu(mpi_reply->Termsas/mptESS;
		goto ata;_cal_
#in_S_ADAPTER *ioc,
  _data->ode *sas_expET *questresponse_cdevice_lockent has been m*f2sast scs_target_pri
	dewtpriny respon*ioc,
  

	if (evef (reasoc, handle);
a->ExpStatES_REund;
}

/*e: Mpin_unlf&
		    MPI2_EVENoc_state;
NG)
OLOGong flaE_in_s |}

	 *
 fw_event);%04x),);
	if (;
		local_event_data = fw_event->event_dats_scsih_exp_id(iocatus event_da =event->evenED ||
		  FMT "%s: failed;
		local_event_data = fw_event->event_dat, __func__);
le);
		if (!handlAS_TOPOLO) {
		_scice_initETE;
	spiock(&ionlocnt
_sc		    local_event_data->Exp;mpt2sin_loOPO_ES_ADDED ||
		    local_event_data->Exp;nder_find_by_handle	spin_unlocvol_pgt_data;
	mpt_datah_blocget_numberc);per adap%04x)* ructf (io);
