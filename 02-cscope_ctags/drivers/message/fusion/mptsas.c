/*
 *  linux/drivers/message/fusion/mptsas.c
 *      For use with LSI PCI chip/adapter(s)
 *      running LSI Fusion MPT (Message Passing Technology) firmware.
 *
 *  Copyright (c) 1999-2008 LSI Corporation
 *  (mailto:DL-MPTFusionLinux@lsi.com)
 */
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    NO WARRANTY
    THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED INCLUDING, WITHOUT
    LIMITATION, ANY WARRANTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,
    MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Each Recipient is
    solely responsible for determining the appropriateness of using and
    distributing the Program and assumes all risks associated with its
    exercise of rights under this Agreement, including but not limited to
    the risks and costs of program errors, damage to or loss of data,
    programs or equipment, and unavailability or interruption of operations.

    DISCLAIMER OF LIABILITY
    NEITHER RECIPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
    TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
    USE OR DISTRIBUTION OF THE PROGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
    HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>	/* for mdelay */

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport_sas.h>
#include <scsi/scsi_dbg.h>

#include "mptbase.h"
#include "mptscsih.h"
#include "mptsas.h"


#define my_NAME		"Fusion MPT SAS Host driver"
#define my_VERSION	MPT_LINUX_VERSION_COMMON
#define MYNAM		"mptsas"

/*
 * Reserved channel for integrated raid
 */
#define MPTSAS_RAID_CHANNEL	1

#define SAS_CONFIG_PAGE_TIMEOUT		30
MODULE_AUTHOR(MODULEAUTHOR);
MODULE_DESCRIPTION(my_NAME);
MODULE_LICENSE("GPL");
MODULE_VERSION(my_VERSION);

static int mpt_pt_clear;
module_param(mpt_pt_clear, int, 0);
MODULE_PARM_DESC(mpt_pt_clear,
		" Clear persistency table: enable=1  "
		"(default=MPTSCSIH_PT_CLEAR=0)");

/* scsi-mid layer global parmeter is max_report_luns, which is 511 */
#define MPTSAS_MAX_LUN (16895)
static int max_lun = MPTSAS_MAX_LUN;
module_param(max_lun, int, 0);
MODULE_PARM_DESC(max_lun, " max lun, default=16895 ");

static u8	mptsasDoneCtx = MPT_MAX_PROTOCOL_DRIVERS;
static u8	mptsasTaskCtx = MPT_MAX_PROTOCOL_DRIVERS;
static u8	mptsasInternalCtx = MPT_MAX_PROTOCOL_DRIVERS; /* Used only for internal commands */
static u8	mptsasMgmtCtx = MPT_MAX_PROTOCOL_DRIVERS;
static u8	mptsasDeviceResetCtx = MPT_MAX_PROTOCOL_DRIVERS;

static void mptsas_firmware_event_work(struct work_struct *work);
static void mptsas_send_sas_event(struct fw_event_work *fw_event);
static void mptsas_send_raid_event(struct fw_event_work *fw_event);
static void mptsas_send_ir2_event(struct fw_event_work *fw_event);
static void mptsas_parse_device_info(struct sas_identify *identify,
		struct mptsas_devinfo *device_info);
static inline void mptsas_set_rphy(MPT_ADAPTER *ioc,
		struct mptsas_phyinfo *phy_info, struct sas_rphy *rphy);
static struct mptsas_phyinfo	*mptsas_find_phyinfo_by_sas_address
		(MPT_ADAPTER *ioc, u64 sas_address);
static int mptsas_sas_device_pg0(MPT_ADAPTER *ioc,
	struct mptsas_devinfo *device_info, u32 form, u32 form_specific);
static int mptsas_sas_enclosure_pg0(MPT_ADAPTER *ioc,
	struct mptsas_enclosure *enclosure, u32 form, u32 form_specific);
static int mptsas_add_end_device(MPT_ADAPTER *ioc,
	struct mptsas_phyinfo *phy_info);
static void mptsas_del_end_device(MPT_ADAPTER *ioc,
	struct mptsas_phyinfo *phy_info);
static void mptsas_send_link_status_event(struct fw_event_work *fw_event);
static struct mptsas_portinfo	*mptsas_find_portinfo_by_sas_address
		(MPT_ADAPTER *ioc, u64 sas_address);
static void mptsas_expander_delete(MPT_ADAPTER *ioc,
		struct mptsas_portinfo *port_info, u8 force);
static void mptsas_send_expander_event(struct fw_event_work *fw_event);
static void mptsas_not_responding_devices(MPT_ADAPTER *ioc);
static void mptsas_scan_sas_topology(MPT_ADAPTER *ioc);
static void mptsas_broadcast_primative_work(struct fw_event_work *fw_event);
static void mptsas_handle_queue_full_event(struct fw_event_work *fw_event);
static void mptsas_volume_delete(MPT_ADAPTER *ioc, u8 id);

static void mptsas_print_phy_data(MPT_ADAPTER *ioc,
					MPI_SAS_IO_UNIT0_PHY_DATA *phy_data)
{
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT
	    "---- IO UNIT PAGE 0 ------------\n", ioc->name));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Handle=0x%X\n",
	    ioc->name, le16_to_cpu(phy_data->AttachedDeviceHandle)));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Controller Handle=0x%X\n",
	    ioc->name, le16_to_cpu(phy_data->ControllerDevHandle)));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Port=0x%X\n",
	    ioc->name, phy_data->Port));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Port Flags=0x%X\n",
	    ioc->name, phy_data->PortFlags));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "PHY Flags=0x%X\n",
	    ioc->name, phy_data->PhyFlags));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Negotiated Link Rate=0x%X\n",
	    ioc->name, phy_data->NegotiatedLinkRate));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT
	    "Controller PHY Device Info=0x%X\n", ioc->name,
	    le32_to_cpu(phy_data->ControllerPhyDeviceInfo)));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "DiscoveryStatus=0x%X\n\n",
	    ioc->name, le32_to_cpu(phy_data->DiscoveryStatus)));
}

static void mptsas_print_phy_pg0(MPT_ADAPTER *ioc, SasPhyPage0_t *pg0)
{
	__le64 sas_address;

	memcpy(&sas_address, &pg0->SASAddress, sizeof(__le64));

	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT
	    "---- SAS PHY PAGE 0 ------------\n", ioc->name));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT
	    "Attached Device Handle=0x%X\n", ioc->name,
	    le16_to_cpu(pg0->AttachedDevHandle)));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "SAS Address=0x%llX\n",
	    ioc->name, (unsigned long long)le64_to_cpu(sas_address)));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT
	    "Attached PHY Identifier=0x%X\n", ioc->name,
	    pg0->AttachedPhyIdentifier));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Attached Device Info=0x%X\n",
	    ioc->name, le32_to_cpu(pg0->AttachedDeviceInfo)));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Programmed Link Rate=0x%X\n",
	    ioc->name,  pg0->ProgrammedLinkRate));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Change Count=0x%X\n",
	    ioc->name, pg0->ChangeCount));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "PHY Info=0x%X\n\n",
	    ioc->name, le32_to_cpu(pg0->PhyInfo)));
}

static void mptsas_print_phy_pg1(MPT_ADAPTER *ioc, SasPhyPage1_t *pg1)
{
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT
	    "---- SAS PHY PAGE 1 ------------\n", ioc->name));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Invalid Dword Count=0x%x\n",
	    ioc->name,  pg1->InvalidDwordCount));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT
	    "Running Disparity Error Count=0x%x\n", ioc->name,
	    pg1->RunningDisparityErrorCount));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT
	    "Loss Dword Synch Count=0x%x\n", ioc->name,
	    pg1->LossDwordSynchCount));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT
	    "PHY Reset Problem Count=0x%x\n\n", ioc->name,
	    pg1->PhyResetProblemCount));
}

static void mptsas_print_device_pg0(MPT_ADAPTER *ioc, SasDevicePage0_t *pg0)
{
	__le64 sas_address;

	memcpy(&sas_address, &pg0->SASAddress, sizeof(__le64));

	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT
	    "---- SAS DEVICE PAGE 0 ---------\n", ioc->name));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Handle=0x%X\n",
	    ioc->name, le16_to_cpu(pg0->DevHandle)));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Parent Handle=0x%X\n",
	    ioc->name, le16_to_cpu(pg0->ParentDevHandle)));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Enclosure Handle=0x%X\n",
	    ioc->name, le16_to_cpu(pg0->EnclosureHandle)));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Slot=0x%X\n",
	    ioc->name, le16_to_cpu(pg0->Slot)));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "SAS Address=0x%llX\n",
	    ioc->name, (unsigned long long)le64_to_cpu(sas_address)));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Target ID=0x%X\n",
	    ioc->name, pg0->TargetID));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Bus=0x%X\n",
	    ioc->name, pg0->Bus));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Parent Phy Num=0x%X\n",
	    ioc->name, pg0->PhyNum));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Access Status=0x%X\n",
	    ioc->name, le16_to_cpu(pg0->AccessStatus)));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Device Info=0x%X\n",
	    ioc->name, le32_to_cpu(pg0->DeviceInfo)));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Flags=0x%X\n",
	    ioc->name, le16_to_cpu(pg0->Flags)));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Physical Port=0x%X\n\n",
	    ioc->name, pg0->PhysicalPort));
}

static void mptsas_print_expander_pg1(MPT_ADAPTER *ioc, SasExpanderPage1_t *pg1)
{
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT
	    "---- SAS EXPANDER PAGE 1 ------------\n", ioc->name));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Physical Port=0x%X\n",
	    ioc->name, pg1->PhysicalPort));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "PHY Identifier=0x%X\n",
	    ioc->name, pg1->PhyIdentifier));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Negotiated Link Rate=0x%X\n",
	    ioc->name, pg1->NegotiatedLinkRate));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Programmed Link Rate=0x%X\n",
	    ioc->name, pg1->ProgrammedLinkRate));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Hardware Link Rate=0x%X\n",
	    ioc->name, pg1->HwLinkRate));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Owner Device Handle=0x%X\n",
	    ioc->name, le16_to_cpu(pg1->OwnerDevHandle)));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT
	    "Attached Device Handle=0x%X\n\n", ioc->name,
	    le16_to_cpu(pg1->AttachedDevHandle)));
}

/* inhibit sas firmware event handling */
static void
mptsas_fw_event_off(MPT_ADAPTER *ioc)
{
	unsigned long flags;

	spin_lock_irqsave(&ioc->fw_event_lock, flags);
	ioc->fw_events_off = 1;
	ioc->sas_discovery_quiesce_io = 0;
	spin_unlock_irqrestore(&ioc->fw_event_lock, flags);

}

/* enable sas firmware event handling */
static void
mptsas_fw_event_on(MPT_ADAPTER *ioc)
{
	unsigned long flags;

	spin_lock_irqsave(&ioc->fw_event_lock, flags);
	ioc->fw_events_off = 0;
	spin_unlock_irqrestore(&ioc->fw_event_lock, flags);
}

/* queue a sas firmware event */
static void
mptsas_add_fw_event(MPT_ADAPTER *ioc, struct fw_event_work *fw_event,
    unsigned long delay)
{
	unsigned long flags;

	spin_lock_irqsave(&ioc->fw_event_lock, flags);
	list_add_tail(&fw_event->list, &ioc->fw_event_list);
	INIT_DELAYED_WORK(&fw_event->work, mptsas_firmware_event_work);
	devtprintk(ioc, printk(MYIOC_s_DEBUG_FMT "%s: add (fw_event=0x%p)\n",
	    ioc->name, __func__, fw_event));
	queue_delayed_work(ioc->fw_event_q, &fw_event->work,
	    delay);
	spin_unlock_irqrestore(&ioc->fw_event_lock, flags);
}

/* requeue a sas firmware event */
static void
mptsas_requeue_fw_event(MPT_ADAPTER *ioc, struct fw_event_work *fw_event,
    unsigned long delay)
{
	unsigned long flags;
	spin_lock_irqsave(&ioc->fw_event_lock, flags);
	devtprintk(ioc, printk(MYIOC_s_DEBUG_FMT "%s: reschedule task "
	    "(fw_event=0x%p)\n", ioc->name, __func__, fw_event));
	fw_event->retries++;
	queue_delayed_work(ioc->fw_event_q, &fw_event->work,
	    msecs_to_jiffies(delay));
	spin_unlock_irqrestore(&ioc->fw_event_lock, flags);
}

/* free memory assoicated to a sas firmware event */
static void
mptsas_free_fw_event(MPT_ADAPTER *ioc, struct fw_event_work *fw_event)
{
	unsigned long flags;

	spin_lock_irqsave(&ioc->fw_event_lock, flags);
	devtprintk(ioc, printk(MYIOC_s_DEBUG_FMT "%s: kfree (fw_event=0x%p)\n",
	    ioc->name, __func__, fw_event));
	list_del(&fw_event->list);
	kfree(fw_event);
	spin_unlock_irqrestore(&ioc->fw_event_lock, flags);
}

/* walk the firmware event queue, and either stop or wait for
 * outstanding events to complete */
static void
mptsas_cleanup_fw_event_q(MPT_ADAPTER *ioc)
{
	struct fw_event_work *fw_event, *next;
	struct mptsas_target_reset_event *target_reset_list, *n;
	MPT_SCSI_HOST	*hd = shost_priv(ioc->sh);

	/* flush the target_reset_list */
	if (!list_empty(&hd->target_reset_list)) {
		list_for_each_entry_safe(target_reset_list, n,
		    &hd->target_reset_list, list) {
			dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT
			    "%s: removing target reset for id=%d\n",
			    ioc->name, __func__,
			   target_reset_list->sas_event_data.TargetID));
			list_del(&target_reset_list->list);
			kfree(target_reset_list);
		}
	}

	if (list_empty(&ioc->fw_event_list) ||
	     !ioc->fw_event_q || in_interrupt())
		return;

	list_for_each_entry_safe(fw_event, next, &ioc->fw_event_list, list) {
		if (cancel_delayed_work(&fw_event->work))
			mptsas_free_fw_event(ioc, fw_event);
	}
}


static inline MPT_ADAPTER *phy_to_ioc(struct sas_phy *phy)
{
	struct Scsi_Host *shost = dev_to_shost(phy->dev.parent);
	return ((MPT_SCSI_HOST *)shost->hostdata)->ioc;
}

static inline MPT_ADAPTER *rphy_to_ioc(struct sas_rphy *rphy)
{
	struct Scsi_Host *shost = dev_to_shost(rphy->dev.parent->parent);
	return ((MPT_SCSI_HOST *)shost->hostdata)->ioc;
}

/*
 * mptsas_find_portinfo_by_handle
 *
 * This function should be called with the sas_topology_mutex already held
 */
static struct mptsas_portinfo *
mptsas_find_portinfo_by_handle(MPT_ADAPTER *ioc, u16 handle)
{
	struct mptsas_portinfo *port_info, *rc=NULL;
	int i;

	list_for_each_entry(port_info, &ioc->sas_topology, list)
		for (i = 0; i < port_info->num_phys; i++)
			if (port_info->phy_info[i].identify.handle == handle) {
				rc = port_info;
				goto out;
			}
 out:
	return rc;
}

/**
 *	mptsas_find_portinfo_by_sas_address -
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@handle:
 *
 *	This function should be called with the sas_topology_mutex already held
 *
 **/
static struct mptsas_portinfo *
mptsas_find_portinfo_by_sas_address(MPT_ADAPTER *ioc, u64 sas_address)
{
	struct mptsas_portinfo *port_info, *rc = NULL;
	int i;

	if (sas_address >= ioc->hba_port_sas_addr &&
	    sas_address < (ioc->hba_port_sas_addr +
	    ioc->hba_port_num_phy))
		return ioc->hba_port_info;

	mutex_lock(&ioc->sas_topology_mutex);
	list_for_each_entry(port_info, &ioc->sas_topology, list)
		for (i = 0; i < port_info->num_phys; i++)
			if (port_info->phy_info[i].identify.sas_address ==
			    sas_address) {
				rc = port_info;
				goto out;
			}
 out:
	mutex_unlock(&ioc->sas_topology_mutex);
	return rc;
}

/*
 * Returns true if there is a scsi end device
 */
static inline int
mptsas_is_end_device(struct mptsas_devinfo * attached)
{
	if ((attached->sas_address) &&
	    (attached->device_info &
	    MPI_SAS_DEVICE_INFO_END_DEVICE) &&
	    ((attached->device_info &
	    MPI_SAS_DEVICE_INFO_SSP_TARGET) |
	    (attached->device_info &
	    MPI_SAS_DEVICE_INFO_STP_TARGET) |
	    (attached->device_info &
	    MPI_SAS_DEVICE_INFO_SATA_DEVICE)))
		return 1;
	else
		return 0;
}

/* no mutex */
static void
mptsas_port_delete(MPT_ADAPTER *ioc, struct mptsas_portinfo_details * port_details)
{
	struct mptsas_portinfo *port_info;
	struct mptsas_phyinfo *phy_info;
	u8	i;

	if (!port_details)
		return;

	port_info = port_details->port_info;
	phy_info = port_info->phy_info;

	dsaswideprintk(ioc, printk(MYIOC_s_DEBUG_FMT "%s: [%p]: num_phys=%02d "
	    "bitmask=0x%016llX\n", ioc->name, __func__, port_details,
	    port_details->num_phys, (unsigned long long)
	    port_details->phy_bitmask));

	for (i = 0; i < port_info->num_phys; i++, phy_info++) {
		if(phy_info->port_details != port_details)
			continue;
		memset(&phy_info->attached, 0, sizeof(struct mptsas_devinfo));
		mptsas_set_rphy(ioc, phy_info, NULL);
		phy_info->port_details = NULL;
	}
	kfree(port_details);
}

static inline struct sas_rphy *
mptsas_get_rphy(struct mptsas_phyinfo *phy_info)
{
	if (phy_info->port_details)
		return phy_info->port_details->rphy;
	else
		return NULL;
}

static inline void
mptsas_set_rphy(MPT_ADAPTER *ioc, struct mptsas_phyinfo *phy_info, struct sas_rphy *rphy)
{
	if (phy_info->port_details) {
		phy_info->port_details->rphy = rphy;
		dsaswideprintk(ioc, printk(MYIOC_s_DEBUG_FMT "sas_rphy_add: rphy=%p\n",
		    ioc->name, rphy));
	}

	if (rphy) {
		dsaswideprintk(ioc, dev_printk(KERN_DEBUG,
		    &rphy->dev, MYIOC_s_FMT "add:", ioc->name));
		dsaswideprintk(ioc, printk(MYIOC_s_DEBUG_FMT "rphy=%p release=%p\n",
		    ioc->name, rphy, rphy->dev.release));
	}
}

static inline struct sas_port *
mptsas_get_port(struct mptsas_phyinfo *phy_info)
{
	if (phy_info->port_details)
		return phy_info->port_details->port;
	else
		return NULL;
}

static inline void
mptsas_set_port(MPT_ADAPTER *ioc, struct mptsas_phyinfo *phy_info, struct sas_port *port)
{
	if (phy_info->port_details)
		phy_info->port_details->port = port;

	if (port) {
		dsaswideprintk(ioc, dev_printk(KERN_DEBUG,
		    &port->dev, MYIOC_s_FMT "add:", ioc->name));
		dsaswideprintk(ioc, printk(MYIOC_s_DEBUG_FMT "port=%p release=%p\n",
		    ioc->name, port, port->dev.release));
	}
}

static inline struct scsi_target *
mptsas_get_starget(struct mptsas_phyinfo *phy_info)
{
	if (phy_info->port_details)
		return phy_info->port_details->starget;
	else
		return NULL;
}

static inline void
mptsas_set_starget(struct mptsas_phyinfo *phy_info, struct scsi_target *
starget)
{
	if (phy_info->port_details)
		phy_info->port_details->starget = starget;
}

/**
 *	mptsas_add_device_component -
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@channel: fw mapped id's
 *	@id:
 *	@sas_address:
 *	@device_info:
 *
 **/
static void
mptsas_add_device_component(MPT_ADAPTER *ioc, u8 channel, u8 id,
	u64 sas_address, u32 device_info, u16 slot, u64 enclosure_logical_id)
{
	struct mptsas_device_info	*sas_info, *next;
	struct scsi_device	*sdev;
	struct scsi_target	*starget;
	struct sas_rphy	*rphy;

	/*
	 * Delete all matching devices out of the list
	 */
	mutex_lock(&ioc->sas_device_info_mutex);
	list_for_each_entry_safe(sas_info, next, &ioc->sas_device_info_list,
	    list) {
		if (!sas_info->is_logical_volume &&
		    (sas_info->sas_address == sas_address ||
		    (sas_info->fw.channel == channel &&
		     sas_info->fw.id == id))) {
			list_del(&sas_info->list);
			kfree(sas_info);
		}
	}

	sas_info = kzalloc(sizeof(struct mptsas_device_info), GFP_KERNEL);
	if (!sas_info)
		goto out;

	/*
	 * Set Firmware mapping
	 */
	sas_info->fw.id = id;
	sas_info->fw.channel = channel;

	sas_info->sas_address = sas_address;
	sas_info->device_info = device_info;
	sas_info->slot = slot;
	sas_info->enclosure_logical_id = enclosure_logical_id;
	INIT_LIST_HEAD(&sas_info->list);
	list_add_tail(&sas_info->list, &ioc->sas_device_info_list);

	/*
	 * Set OS mapping
	 */
	shost_for_each_device(sdev, ioc->sh) {
		starget = scsi_target(sdev);
		rphy = dev_to_rphy(starget->dev.parent);
		if (rphy->identify.sas_address == sas_address) {
			sas_info->os.id = starget->id;
			sas_info->os.channel = starget->channel;
		}
	}

 out:
	mutex_unlock(&ioc->sas_device_info_mutex);
	return;
}

/**
 *	mptsas_add_device_component_by_fw -
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@channel:  fw mapped id's
 *	@id:
 *
 **/
static void
mptsas_add_device_component_by_fw(MPT_ADAPTER *ioc, u8 channel, u8 id)
{
	struct mptsas_devinfo sas_device;
	struct mptsas_enclosure enclosure_info;
	int rc;

	rc = mptsas_sas_device_pg0(ioc, &sas_device,
	    (MPI_SAS_DEVICE_PGAD_FORM_BUS_TARGET_ID <<
	     MPI_SAS_DEVICE_PGAD_FORM_SHIFT),
	    (channel << 8) + id);
	if (rc)
		return;

	memset(&enclosure_info, 0, sizeof(struct mptsas_enclosure));
	mptsas_sas_enclosure_pg0(ioc, &enclosure_info,
	    (MPI_SAS_ENCLOS_PGAD_FORM_HANDLE <<
	     MPI_SAS_ENCLOS_PGAD_FORM_SHIFT),
	     sas_device.handle_enclosure);

	mptsas_add_device_component(ioc, sas_device.channel,
	    sas_device.id, sas_device.sas_address, sas_device.device_info,
	    sas_device.slot, enclosure_info.enclosure_logical_id);
}

/**
 *	mptsas_add_device_component_starget_ir - Handle Integrated RAID, adding each individual device to list
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@channel: fw mapped id's
 *	@id:
 *
 **/
static void
mptsas_add_device_component_starget_ir(MPT_ADAPTER *ioc,
		struct scsi_target *starget)
{
	CONFIGPARMS			cfg;
	ConfigPageHeader_t		hdr;
	dma_addr_t			dma_handle;
	pRaidVolumePage0_t		buffer = NULL;
	int				i;
	RaidPhysDiskPage0_t 		phys_disk;
	struct mptsas_device_info	*sas_info, *next;

	memset(&cfg, 0 , sizeof(CONFIGPARMS));
	memset(&hdr, 0 , sizeof(ConfigPageHeader_t));
	hdr.PageType = MPI_CONFIG_PAGETYPE_RAID_VOLUME;
	/* assumption that all volumes on channel = 0 */
	cfg.pageAddr = starget->id;
	cfg.cfghdr.hdr = &hdr;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;
	cfg.timeout = SAS_CONFIG_PAGE_TIMEOUT;

	if (mpt_config(ioc, &cfg) != 0)
		goto out;

	if (!hdr.PageLength)
		goto out;

	buffer = pci_alloc_consistent(ioc->pcidev, hdr.PageLength * 4,
	    &dma_handle);

	if (!buffer)
		goto out;

	cfg.physAddr = dma_handle;
	cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;

	if (mpt_config(ioc, &cfg) != 0)
		goto out;

	if (!buffer->NumPhysDisks)
		goto out;

	/*
	 * Adding entry for hidden components
	 */
	for (i = 0; i < buffer->NumPhysDisks; i++) {

		if (mpt_raid_phys_disk_pg0(ioc,
		    buffer->PhysDisk[i].PhysDiskNum, &phys_disk) != 0)
			continue;

		mptsas_add_device_component_by_fw(ioc, phys_disk.PhysDiskBus,
		    phys_disk.PhysDiskID);

		mutex_lock(&ioc->sas_device_info_mutex);
		list_for_each_entry(sas_info, &ioc->sas_device_info_list,
		    list) {
			if (!sas_info->is_logical_volume &&
			    (sas_info->fw.channel == phys_disk.PhysDiskBus &&
			    sas_info->fw.id == phys_disk.PhysDiskID)) {
				sas_info->is_hidden_raid_component = 1;
				sas_info->volume_id = starget->id;
			}
		}
		mutex_unlock(&ioc->sas_device_info_mutex);

	}

	/*
	 * Delete all matching devices out of the list
	 */
	mutex_lock(&ioc->sas_device_info_mutex);
	list_for_each_entry_safe(sas_info, next, &ioc->sas_device_info_list,
	    list) {
		if (sas_info->is_logical_volume && sas_info->fw.id ==
		    starget->id) {
			list_del(&sas_info->list);
			kfree(sas_info);
		}
	}

	sas_info = kzalloc(sizeof(struct mptsas_device_info), GFP_KERNEL);
	if (sas_info) {
		sas_info->fw.id = starget->id;
		sas_info->os.id = starget->id;
		sas_info->os.channel = starget->channel;
		sas_info->is_logical_volume = 1;
		INIT_LIST_HEAD(&sas_info->list);
		list_add_tail(&sas_info->list, &ioc->sas_device_info_list);
	}
	mutex_unlock(&ioc->sas_device_info_mutex);

 out:
	if (buffer)
		pci_free_consistent(ioc->pcidev, hdr.PageLength * 4, buffer,
		    dma_handle);
}

/**
 *	mptsas_add_device_component_starget -
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@starget:
 *
 **/
static void
mptsas_add_device_component_starget(MPT_ADAPTER *ioc,
	struct scsi_target *starget)
{
	VirtTarget	*vtarget;
	struct sas_rphy	*rphy;
	struct mptsas_phyinfo	*phy_info = NULL;
	struct mptsas_enclosure	enclosure_info;

	rphy = dev_to_rphy(starget->dev.parent);
	vtarget = starget->hostdata;
	phy_info = mptsas_find_phyinfo_by_sas_address(ioc,
			rphy->identify.sas_address);
	if (!phy_info)
		return;

	memset(&enclosure_info, 0, sizeof(struct mptsas_enclosure));
	mptsas_sas_enclosure_pg0(ioc, &enclosure_info,
		(MPI_SAS_ENCLOS_PGAD_FORM_HANDLE <<
		MPI_SAS_ENCLOS_PGAD_FORM_SHIFT),
		phy_info->attached.handle_enclosure);

	mptsas_add_device_component(ioc, phy_info->attached.channel,
		phy_info->attached.id, phy_info->attached.sas_address,
		phy_info->attached.device_info,
		phy_info->attached.slot, enclosure_info.enclosure_logical_id);
}

/**
 *	mptsas_del_device_component_by_os - Once a device has been removed, we mark the entry in the list as being cached
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@channel: os mapped id's
 *	@id:
 *
 **/
static void
mptsas_del_device_component_by_os(MPT_ADAPTER *ioc, u8 channel, u8 id)
{
	struct mptsas_device_info	*sas_info, *next;

	/*
	 * Set is_cached flag
	 */
	list_for_each_entry_safe(sas_info, next, &ioc->sas_device_info_list,
		list) {
		if (sas_info->os.channel == channel && sas_info->os.id == id)
			sas_info->is_cached = 1;
	}
}

/**
 *	mptsas_del_device_components - Cleaning the list
 *	@ioc: Pointer to MPT_ADAPTER structure
 *
 **/
static void
mptsas_del_device_components(MPT_ADAPTER *ioc)
{
	struct mptsas_device_info	*sas_info, *next;

	mutex_lock(&ioc->sas_device_info_mutex);
	list_for_each_entry_safe(sas_info, next, &ioc->sas_device_info_list,
		list) {
		list_del(&sas_info->list);
		kfree(sas_info);
	}
	mutex_unlock(&ioc->sas_device_info_mutex);
}


/*
 * mptsas_setup_wide_ports
 *
 * Updates for new and existing narrow/wide port configuration
 * in the sas_topology
 */
static void
mptsas_setup_wide_ports(MPT_ADAPTER *ioc, struct mptsas_portinfo *port_info)
{
	struct mptsas_portinfo_details * port_details;
	struct mptsas_phyinfo *phy_info, *phy_info_cmp;
	u64	sas_address;
	int	i, j;

	mutex_lock(&ioc->sas_topology_mutex);

	phy_info = port_info->phy_info;
	for (i = 0 ; i < port_info->num_phys ; i++, phy_info++) {
		if (phy_info->attached.handle)
			continue;
		port_details = phy_info->port_details;
		if (!port_details)
			continue;
		if (port_details->num_phys < 2)
			continue;
		/*
		 * Removing a phy from a port, letting the last
		 * phy be removed by firmware events.
		 */
		dsaswideprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "%s: [%p]: deleting phy = %d\n",
		    ioc->name, __func__, port_details, i));
		port_details->num_phys--;
		port_details->phy_bitmask &= ~ (1 << phy_info->phy_id);
		memset(&phy_info->attached, 0, sizeof(struct mptsas_devinfo));
		if (phy_info->phy) {
			devtprintk(ioc, dev_printk(KERN_DEBUG,
				&phy_info->phy->dev, MYIOC_s_FMT
				"delete phy %d, phy-obj (0x%p)\n", ioc->name,
				phy_info->phy_id, phy_info->phy));
			sas_port_delete_phy(port_details->port, phy_info->phy);
		}
		phy_info->port_details = NULL;
	}

	/*
	 * Populate and refresh the tree
	 */
	phy_info = port_info->phy_info;
	for (i = 0 ; i < port_info->num_phys ; i++, phy_info++) {
		sas_address = phy_info->attached.sas_address;
		dsaswideprintk(ioc, printk(MYIOC_s_DEBUG_FMT "phy_id=%d sas_address=0x%018llX\n",
		    ioc->name, i, (unsigned long long)sas_address));
		if (!sas_address)
			continue;
		port_details = phy_info->port_details;
		/*
		 * Forming a port
		 */
		if (!port_details) {
			port_details = kzalloc(sizeof(struct
				mptsas_portinfo_details), GFP_KERNEL);
			if (!port_details)
				goto out;
			port_details->num_phys = 1;
			port_details->port_info = port_info;
			if (phy_info->phy_id < 64 )
				port_details->phy_bitmask |=
				    (1 << phy_info->phy_id);
			phy_info->sas_port_add_phy=1;
			dsaswideprintk(ioc, printk(MYIOC_s_DEBUG_FMT "\t\tForming port\n\t\t"
			    "phy_id=%d sas_address=0x%018llX\n",
			    ioc->name, i, (unsigned long long)sas_address));
			phy_info->port_details = port_details;
		}

		if (i == port_info->num_phys - 1)
			continue;
		phy_info_cmp = &port_info->phy_info[i + 1];
		for (j = i + 1 ; j < port_info->num_phys ; j++,
		    phy_info_cmp++) {
			if (!phy_info_cmp->attached.sas_address)
				continue;
			if (sas_address != phy_info_cmp->attached.sas_address)
				continue;
			if (phy_info_cmp->port_details == port_details )
				continue;
			dsaswideprintk(ioc, printk(MYIOC_s_DEBUG_FMT
			    "\t\tphy_id=%d sas_address=0x%018llX\n",
			    ioc->name, j, (unsigned long long)
			    phy_info_cmp->attached.sas_address));
			if (phy_info_cmp->port_details) {
				port_details->rphy =
				    mptsas_get_rphy(phy_info_cmp);
				port_details->port =
				    mptsas_get_port(phy_info_cmp);
				port_details->starget =
				    mptsas_get_starget(phy_info_cmp);
				port_details->num_phys =
					phy_info_cmp->port_details->num_phys;
				if (!phy_info_cmp->port_details->num_phys)
					kfree(phy_info_cmp->port_details);
			} else
				phy_info_cmp->sas_port_add_phy=1;
			/*
			 * Adding a phy to a port
			 */
			phy_info_cmp->port_details = port_details;
			if (phy_info_cmp->phy_id < 64 )
				port_details->phy_bitmask |=
				(1 << phy_info_cmp->phy_id);
			port_details->num_phys++;
		}
	}

 out:

	for (i = 0; i < port_info->num_phys; i++) {
		port_details = port_info->phy_info[i].port_details;
		if (!port_details)
			continue;
		dsaswideprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "%s: [%p]: phy_id=%02d num_phys=%02d "
		    "bitmask=0x%016llX\n", ioc->name, __func__,
		    port_details, i, port_details->num_phys,
		    (unsigned long long)port_details->phy_bitmask));
		dsaswideprintk(ioc, printk(MYIOC_s_DEBUG_FMT "\t\tport = %p rphy=%p\n",
		    ioc->name, port_details->port, port_details->rphy));
	}
	dsaswideprintk(ioc, printk("\n"));
	mutex_unlock(&ioc->sas_topology_mutex);
}

/**
 * csmisas_find_vtarget
 *
 * @ioc
 * @volume_id
 * @volume_bus
 *
 **/
static VirtTarget *
mptsas_find_vtarget(MPT_ADAPTER *ioc, u8 channel, u8 id)
{
	struct scsi_device 		*sdev;
	VirtDevice			*vdevice;
	VirtTarget 			*vtarget = NULL;

	shost_for_each_device(sdev, ioc->sh) {
		vdevice = sdev->hostdata;
		if ((vdevice == NULL) ||
			(vdevice->vtarget == NULL))
			continue;
		if ((vdevice->vtarget->tflags &
		    MPT_TARGET_FLAGS_RAID_COMPONENT ||
		    vdevice->vtarget->raidVolume))
			continue;
		if (vdevice->vtarget->id == id &&
			vdevice->vtarget->channel == channel)
			vtarget = vdevice->vtarget;
	}
	return vtarget;
}

static void
mptsas_queue_device_delete(MPT_ADAPTER *ioc,
	MpiEventDataSasDeviceStatusChange_t *sas_event_data)
{
	struct fw_event_work *fw_event;
	int sz;

	sz = offsetof(struct fw_event_work, event_data) +
	    sizeof(MpiEventDataSasDeviceStatusChange_t);
	fw_event = kzalloc(sz, GFP_ATOMIC);
	if (!fw_event) {
		printk(MYIOC_s_WARN_FMT "%s: failed at (line=%d)\n",
		    ioc->name, __func__, __LINE__);
		return;
	}
	memcpy(fw_event->event_data, sas_event_data,
	    sizeof(MpiEventDataSasDeviceStatusChange_t));
	fw_event->event = MPI_EVENT_SAS_DEVICE_STATUS_CHANGE;
	fw_event->ioc = ioc;
	mptsas_add_fw_event(ioc, fw_event, msecs_to_jiffies(1));
}

static void
mptsas_queue_rescan(MPT_ADAPTER *ioc)
{
	struct fw_event_work *fw_event;
	int sz;

	sz = offsetof(struct fw_event_work, event_data);
	fw_event = kzalloc(sz, GFP_ATOMIC);
	if (!fw_event) {
		printk(MYIOC_s_WARN_FMT "%s: failed at (line=%d)\n",
		    ioc->name, __func__, __LINE__);
		return;
	}
	fw_event->event = -1;
	fw_event->ioc = ioc;
	mptsas_add_fw_event(ioc, fw_event, msecs_to_jiffies(1));
}


/**
 * mptsas_target_reset
 *
 * Issues TARGET_RESET to end device using handshaking method
 *
 * @ioc
 * @channel
 * @id
 *
 * Returns (1) success
 *         (0) failure
 *
 **/
static int
mptsas_target_reset(MPT_ADAPTER *ioc, u8 channel, u8 id)
{
	MPT_FRAME_HDR	*mf;
	SCSITaskMgmt_t	*pScsiTm;
	if (mpt_set_taskmgmt_in_progress_flag(ioc) != 0)
		return 0;


	mf = mpt_get_msg_frame(mptsasDeviceResetCtx, ioc);
	if (mf == NULL) {
		dfailprintk(ioc, printk(MYIOC_s_WARN_FMT
			"%s, no msg frames @%d!!\n", ioc->name,
			__func__, __LINE__));
		goto out_fail;
	}

	dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT "TaskMgmt request (mf=%p)\n",
		ioc->name, mf));

	/* Format the Request
	 */
	pScsiTm = (SCSITaskMgmt_t *) mf;
	memset (pScsiTm, 0, sizeof(SCSITaskMgmt_t));
	pScsiTm->TargetID = id;
	pScsiTm->Bus = channel;
	pScsiTm->Function = MPI_FUNCTION_SCSI_TASK_MGMT;
	pScsiTm->TaskType = MPI_SCSITASKMGMT_TASKTYPE_TARGET_RESET;
	pScsiTm->MsgFlags = MPI_SCSITASKMGMT_MSGFLAGS_LIPRESET_RESET_OPTION;

	DBG_DUMP_TM_REQUEST_FRAME(ioc, (u32 *)mf);

	dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT
	   "TaskMgmt type=%d (sas device delete) fw_channel = %d fw_id = %d)\n",
	   ioc->name, MPI_SCSITASKMGMT_TASKTYPE_TARGET_RESET, channel, id));

	mpt_put_msg_frame_hi_pri(mptsasDeviceResetCtx, ioc, mf);

	return 1;

 out_fail:

	mpt_clear_taskmgmt_in_progress_flag(ioc);
	return 0;
}

/**
 * mptsas_target_reset_queue
 *
 * Receive request for TARGET_RESET after recieving an firmware
 * event NOT_RESPONDING_EVENT, then put command in link list
 * and queue if task_queue already in use.
 *
 * @ioc
 * @sas_event_data
 *
 **/
static void
mptsas_target_reset_queue(MPT_ADAPTER *ioc,
    EVENT_DATA_SAS_DEVICE_STATUS_CHANGE *sas_event_data)
{
	MPT_SCSI_HOST	*hd = shost_priv(ioc->sh);
	VirtTarget *vtarget = NULL;
	struct mptsas_target_reset_event *target_reset_list;
	u8		id, channel;

	id = sas_event_data->TargetID;
	channel = sas_event_data->Bus;

	if (!(vtarget = mptsas_find_vtarget(ioc, channel, id)))
		return;

	vtarget->deleted = 1; /* block IO */

	target_reset_list = kzalloc(sizeof(struct mptsas_target_reset_event),
	    GFP_ATOMIC);
	if (!target_reset_list) {
		dfailprintk(ioc, printk(MYIOC_s_WARN_FMT
			"%s, failed to allocate mem @%d..!!\n",
			ioc->name, __func__, __LINE__));
		return;
	}

	memcpy(&target_reset_list->sas_event_data, sas_event_data,
		sizeof(*sas_event_data));
	list_add_tail(&target_reset_list->list, &hd->target_reset_list);

	target_reset_list->time_count = jiffies;

	if (mptsas_target_reset(ioc, channel, id)) {
		target_reset_list->target_reset_issued = 1;
	}
}

/**
 *	mptsas_taskmgmt_complete - complete SAS task management function
 *	@ioc: Pointer to MPT_ADAPTER structure
 *
 *	Completion for TARGET_RESET after NOT_RESPONDING_EVENT, enable work
 *	queue to finish off removing device from upper layers. then send next
 *	TARGET_RESET in the queue.
 **/
static int
mptsas_taskmgmt_complete(MPT_ADAPTER *ioc, MPT_FRAME_HDR *mf, MPT_FRAME_HDR *mr)
{
	MPT_SCSI_HOST	*hd = shost_priv(ioc->sh);
        struct list_head *head = &hd->target_reset_list;
	u8		id, channel;
	struct mptsas_target_reset_event	*target_reset_list;
	SCSITaskMgmtReply_t *pScsiTmReply;

	dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT "TaskMgmt completed: "
	    "(mf = %p, mr = %p)\n", ioc->name, mf, mr));

	pScsiTmReply = (SCSITaskMgmtReply_t *)mr;
	if (pScsiTmReply) {
		dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "\tTaskMgmt completed: fw_channel = %d, fw_id = %d,\n"
		    "\ttask_type = 0x%02X, iocstatus = 0x%04X "
		    "loginfo = 0x%08X,\n\tresponse_code = 0x%02X, "
		    "term_cmnds = %d\n", ioc->name,
		    pScsiTmReply->Bus, pScsiTmReply->TargetID,
		    pScsiTmReply->TaskType,
		    le16_to_cpu(pScsiTmReply->IOCStatus),
		    le32_to_cpu(pScsiTmReply->IOCLogInfo),
		    pScsiTmReply->ResponseCode,
		    le32_to_cpu(pScsiTmReply->TerminationCount)));

		if (pScsiTmReply->ResponseCode)
			mptscsih_taskmgmt_response_code(ioc,
			pScsiTmReply->ResponseCode);
	}

	if (pScsiTmReply && (pScsiTmReply->TaskType ==
	    MPI_SCSITASKMGMT_TASKTYPE_QUERY_TASK || pScsiTmReply->TaskType ==
	     MPI_SCSITASKMGMT_TASKTYPE_ABRT_TASK_SET)) {
		ioc->taskmgmt_cmds.status |= MPT_MGMT_STATUS_COMMAND_GOOD;
		ioc->taskmgmt_cmds.status |= MPT_MGMT_STATUS_RF_VALID;
		memcpy(ioc->taskmgmt_cmds.reply, mr,
		    min(MPT_DEFAULT_FRAME_SIZE, 4 * mr->u.reply.MsgLength));
		if (ioc->taskmgmt_cmds.status & MPT_MGMT_STATUS_PENDING) {
			ioc->taskmgmt_cmds.status &= ~MPT_MGMT_STATUS_PENDING;
			complete(&ioc->taskmgmt_cmds.done);
			return 1;
		}
		return 0;
	}

	mpt_clear_taskmgmt_in_progress_flag(ioc);

	if (list_empty(head))
		return 1;

	target_reset_list = list_entry(head->next,
	    struct mptsas_target_reset_event, list);

	dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT
	    "TaskMgmt: completed (%d seconds)\n",
	    ioc->name, jiffies_to_msecs(jiffies -
	    target_reset_list->time_count)/1000));

	id = pScsiTmReply->TargetID;
	channel = pScsiTmReply->Bus;
	target_reset_list->time_count = jiffies;

	/*
	 * retry target reset
	 */
	if (!target_reset_list->target_reset_issued) {
		if (mptsas_target_reset(ioc, channel, id))
			target_reset_list->target_reset_issued = 1;
		return 1;
	}

	/*
	 * enable work queue to remove device from upper layers
	 */
	list_del(&target_reset_list->list);
	if ((mptsas_find_vtarget(ioc, channel, id)) && !ioc->fw_events_off)
		mptsas_queue_device_delete(ioc,
			&target_reset_list->sas_event_data);


	/*
	 * issue target reset to next device in the queue
	 */

	head = &hd->target_reset_list;
	if (list_empty(head))
		return 1;

	target_reset_list = list_entry(head->next, struct mptsas_target_reset_event,
	    list);

	id = target_reset_list->sas_event_data.TargetID;
	channel = target_reset_list->sas_event_data.Bus;
	target_reset_list->time_count = jiffies;

	if (mptsas_target_reset(ioc, channel, id))
		target_reset_list->target_reset_issued = 1;

	return 1;
}

/**
 * mptscsih_ioc_reset
 *
 * @ioc
 * @reset_phase
 *
 **/
static int
mptsas_ioc_reset(MPT_ADAPTER *ioc, int reset_phase)
{
	MPT_SCSI_HOST	*hd;
	int rc;

	rc = mptscsih_ioc_reset(ioc, reset_phase);
	if ((ioc->bus_type != SAS) || (!rc))
		return rc;

	hd = shost_priv(ioc->sh);
	if (!hd->ioc)
		goto out;

	switch (reset_phase) {
	case MPT_IOC_SETUP_RESET:
		dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "%s: MPT_IOC_SETUP_RESET\n", ioc->name, __func__));
		mptsas_fw_event_off(ioc);
		break;
	case MPT_IOC_PRE_RESET:
		dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "%s: MPT_IOC_PRE_RESET\n", ioc->name, __func__));
		break;
	case MPT_IOC_POST_RESET:
		dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "%s: MPT_IOC_POST_RESET\n", ioc->name, __func__));
		if (ioc->sas_mgmt.status & MPT_MGMT_STATUS_PENDING) {
			ioc->sas_mgmt.status |= MPT_MGMT_STATUS_DID_IOCRESET;
			complete(&ioc->sas_mgmt.done);
		}
		mptsas_cleanup_fw_event_q(ioc);
		mptsas_queue_rescan(ioc);
		break;
	default:
		break;
	}

 out:
	return rc;
}


/**
 * enum device_state -
 * @DEVICE_RETRY: need to retry the TUR
 * @DEVICE_ERROR: TUR return error, don't add device
 * @DEVICE_READY: device can be added
 *
 */
enum device_state{
	DEVICE_RETRY,
	DEVICE_ERROR,
	DEVICE_READY,
};

static int
mptsas_sas_enclosure_pg0(MPT_ADAPTER *ioc, struct mptsas_enclosure *enclosure,
		u32 form, u32 form_specific)
{
	ConfigExtendedPageHeader_t hdr;
	CONFIGPARMS cfg;
	SasEnclosurePage0_t *buffer;
	dma_addr_t dma_handle;
	int error;
	__le64 le_identifier;

	memset(&hdr, 0, sizeof(hdr));
	hdr.PageVersion = MPI_SASENCLOSURE0_PAGEVERSION;
	hdr.PageNumber = 0;
	hdr.PageType = MPI_CONFIG_PAGETYPE_EXTENDED;
	hdr.ExtPageType = MPI_CONFIG_EXTPAGETYPE_ENCLOSURE;

	cfg.cfghdr.ehdr = &hdr;
	cfg.physAddr = -1;
	cfg.pageAddr = form + form_specific;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;
	cfg.dir = 0;	/* read */
	cfg.timeout = SAS_CONFIG_PAGE_TIMEOUT;

	error = mpt_config(ioc, &cfg);
	if (error)
		goto out;
	if (!hdr.ExtPageLength) {
		error = -ENXIO;
		goto out;
	}

	buffer = pci_alloc_consistent(ioc->pcidev, hdr.ExtPageLength * 4,
			&dma_handle);
	if (!buffer) {
		error = -ENOMEM;
		goto out;
	}

	cfg.physAddr = dma_handle;
	cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;

	error = mpt_config(ioc, &cfg);
	if (error)
		goto out_free_consistent;

	/* save config data */
	memcpy(&le_identifier, &buffer->EnclosureLogicalID, sizeof(__le64));
	enclosure->enclosure_logical_id = le64_to_cpu(le_identifier);
	enclosure->enclosure_handle = le16_to_cpu(buffer->EnclosureHandle);
	enclosure->flags = le16_to_cpu(buffer->Flags);
	enclosure->num_slot = le16_to_cpu(buffer->NumSlots);
	enclosure->start_slot = le16_to_cpu(buffer->StartSlot);
	enclosure->start_id = buffer->StartTargetID;
	enclosure->start_channel = buffer->StartBus;
	enclosure->sep_id = buffer->SEPTargetID;
	enclosure->sep_channel = buffer->SEPBus;

 out_free_consistent:
	pci_free_consistent(ioc->pcidev, hdr.ExtPageLength * 4,
			    buffer, dma_handle);
 out:
	return error;
}

/**
 *	mptsas_add_end_device - report a new end device to sas transport layer
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@phy_info: decribes attached device
 *
 *	return (0) success (1) failure
 *
 **/
static int
mptsas_add_end_device(MPT_ADAPTER *ioc, struct mptsas_phyinfo *phy_info)
{
	struct sas_rphy *rphy;
	struct sas_port *port;
	struct sas_identify identify;
	char *ds = NULL;
	u8 fw_id;

	if (!phy_info) {
		dfailprintk(ioc, printk(MYIOC_s_ERR_FMT
			"%s: exit at line=%d\n", ioc->name,
			 __func__, __LINE__));
		return 1;
	}

	fw_id = phy_info->attached.id;

	if (mptsas_get_rphy(phy_info)) {
		dfailprintk(ioc, printk(MYIOC_s_ERR_FMT
			"%s: fw_id=%d exit at line=%d\n", ioc->name,
			 __func__, fw_id, __LINE__));
		return 2;
	}

	port = mptsas_get_port(phy_info);
	if (!port) {
		dfailprintk(ioc, printk(MYIOC_s_ERR_FMT
			"%s: fw_id=%d exit at line=%d\n", ioc->name,
			 __func__, fw_id, __LINE__));
		return 3;
	}

	if (phy_info->attached.device_info &
	    MPI_SAS_DEVICE_INFO_SSP_TARGET)
		ds = "ssp";
	if (phy_info->attached.device_info &
	    MPI_SAS_DEVICE_INFO_STP_TARGET)
		ds = "stp";
	if (phy_info->attached.device_info &
	    MPI_SAS_DEVICE_INFO_SATA_DEVICE)
		ds = "sata";

	printk(MYIOC_s_INFO_FMT "attaching %s device: fw_channel %d, fw_id %d,"
	    " phy %d, sas_addr 0x%llx\n", ioc->name, ds,
	    phy_info->attached.channel, phy_info->attached.id,
	    phy_info->attached.phy_id, (unsigned long long)
	    phy_info->attached.sas_address);

	mptsas_parse_device_info(&identify, &phy_info->attached);
	rphy = sas_end_device_alloc(port);
	if (!rphy) {
		dfailprintk(ioc, printk(MYIOC_s_ERR_FMT
			"%s: fw_id=%d exit at line=%d\n", ioc->name,
			 __func__, fw_id, __LINE__));
		return 5; /* non-fatal: an rphy can be added later */
	}

	rphy->identify = identify;
	if (sas_rphy_add(rphy)) {
		dfailprintk(ioc, printk(MYIOC_s_ERR_FMT
			"%s: fw_id=%d exit at line=%d\n", ioc->name,
			 __func__, fw_id, __LINE__));
		sas_rphy_free(rphy);
		return 6;
	}
	mptsas_set_rphy(ioc, phy_info, rphy);
	return 0;
}

/**
 *	mptsas_del_end_device - report a deleted end device to sas transport layer
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@phy_info: decribes attached device
 *
 **/
static void
mptsas_del_end_device(MPT_ADAPTER *ioc, struct mptsas_phyinfo *phy_info)
{
	struct sas_rphy *rphy;
	struct sas_port *port;
	struct mptsas_portinfo *port_info;
	struct mptsas_phyinfo *phy_info_parent;
	int i;
	char *ds = NULL;
	u8 fw_id;
	u64 sas_address;

	if (!phy_info)
		return;

	fw_id = phy_info->attached.id;
	sas_address = phy_info->attached.sas_address;

	if (!phy_info->port_details) {
		dfailprintk(ioc, printk(MYIOC_s_ERR_FMT
			"%s: fw_id=%d exit at line=%d\n", ioc->name,
			 __func__, fw_id, __LINE__));
		return;
	}
	rphy = mptsas_get_rphy(phy_info);
	if (!rphy) {
		dfailprintk(ioc, printk(MYIOC_s_ERR_FMT
			"%s: fw_id=%d exit at line=%d\n", ioc->name,
			 __func__, fw_id, __LINE__));
		return;
	}

	if (phy_info->attached.device_info & MPI_SAS_DEVICE_INFO_SSP_INITIATOR
		|| phy_info->attached.device_info
			& MPI_SAS_DEVICE_INFO_SMP_INITIATOR
		|| phy_info->attached.device_info
			& MPI_SAS_DEVICE_INFO_STP_INITIATOR)
		ds = "initiator";
	if (phy_info->attached.device_info &
	    MPI_SAS_DEVICE_INFO_SSP_TARGET)
		ds = "ssp";
	if (phy_info->attached.device_info &
	    MPI_SAS_DEVICE_INFO_STP_TARGET)
		ds = "stp";
	if (phy_info->attached.device_info &
	    MPI_SAS_DEVICE_INFO_SATA_DEVICE)
		ds = "sata";

	dev_printk(KERN_DEBUG, &rphy->dev, MYIOC_s_FMT
	    "removing %s device: fw_channel %d, fw_id %d, phy %d,"
	    "sas_addr 0x%llx\n", ioc->name, ds, phy_info->attached.channel,
	    phy_info->attached.id, phy_info->attached.phy_id,
	    (unsigned long long) sas_address);

	port = mptsas_get_port(phy_info);
	if (!port) {
		dfailprintk(ioc, printk(MYIOC_s_ERR_FMT
			"%s: fw_id=%d exit at line=%d\n", ioc->name,
			 __func__, fw_id, __LINE__));
		return;
	}
	port_info = phy_info->portinfo;
	phy_info_parent = port_info->phy_info;
	for (i = 0; i < port_info->num_phys; i++, phy_info_parent++) {
		if (!phy_info_parent->phy)
			continue;
		if (phy_info_parent->attached.sas_address !=
		    sas_address)
			continue;
		dev_printk(KERN_DEBUG, &phy_info_parent->phy->dev,
		    MYIOC_s_FMT "delete phy %d, phy-obj (0x%p)\n",
		    ioc->name, phy_info_parent->phy_id,
		    phy_info_parent->phy);
		sas_port_delete_phy(port, phy_info_parent->phy);
	}

	dev_printk(KERN_DEBUG, &port->dev, MYIOC_s_FMT
	    "delete port %d, sas_addr (0x%llx)\n", ioc->name,
	     port->port_identifier, (unsigned long long)sas_address);
	sas_port_delete(port);
	mptsas_set_port(ioc, phy_info, NULL);
	mptsas_port_delete(ioc, phy_info->port_details);
}

struct mptsas_phyinfo *
mptsas_refreshing_device_handles(MPT_ADAPTER *ioc,
	struct mptsas_devinfo *sas_device)
{
	struct mptsas_phyinfo *phy_info;
	struct mptsas_portinfo *port_info;
	int i;

	phy_info = mptsas_find_phyinfo_by_sas_address(ioc,
	    sas_device->sas_address);
	if (!phy_info)
		goto out;
	port_info = phy_info->portinfo;
	if (!port_info)
		goto out;
	mutex_lock(&ioc->sas_topology_mutex);
	for (i = 0; i < port_info->num_phys; i++) {
		if (port_info->phy_info[i].attached.sas_address !=
			sas_device->sas_address)
			continue;
		port_info->phy_info[i].attached.channel = sas_device->channel;
		port_info->phy_info[i].attached.id = sas_device->id;
		port_info->phy_info[i].attached.sas_address =
		    sas_device->sas_address;
		port_info->phy_info[i].attached.handle = sas_device->handle;
		port_info->phy_info[i].attached.handle_parent =
		    sas_device->handle_parent;
		port_info->phy_info[i].attached.handle_enclosure =
		    sas_device->handle_enclosure;
	}
	mutex_unlock(&ioc->sas_topology_mutex);
 out:
	return phy_info;
}

/**
 * mptsas_firmware_event_work - work thread for processing fw events
 * @work: work queue payload containing info describing the event
 * Context: user
 *
 */
static void
mptsas_firmware_event_work(struct work_struct *work)
{
	struct fw_event_work *fw_event =
		container_of(work, struct fw_event_work, work.work);
	MPT_ADAPTER *ioc = fw_event->ioc;

	/* special rescan topology handling */
	if (fw_event->event == -1) {
		if (ioc->in_rescan) {
			devtprintk(ioc, printk(MYIOC_s_DEBUG_FMT
				"%s: rescan ignored as it is in progress\n",
				ioc->name, __func__));
			return;
		}
		devtprintk(ioc, printk(MYIOC_s_DEBUG_FMT "%s: rescan after "
		    "reset\n", ioc->name, __func__));
		ioc->in_rescan = 1;
		mptsas_not_responding_devices(ioc);
		mptsas_scan_sas_topology(ioc);
		ioc->in_rescan = 0;
		mptsas_free_fw_event(ioc, fw_event);
		mptsas_fw_event_on(ioc);
		return;
	}

	/* events handling turned off during host reset */
	if (ioc->fw_events_off) {
		mptsas_free_fw_event(ioc, fw_event);
		return;
	}

	devtprintk(ioc, printk(MYIOC_s_DEBUG_FMT "%s: fw_event=(0x%p), "
	    "event = (0x%02x)\n", ioc->name, __func__, fw_event,
	    (fw_event->event & 0xFF)));

	switch (fw_event->event) {
	case MPI_EVENT_SAS_DEVICE_STATUS_CHANGE:
		mptsas_send_sas_event(fw_event);
		break;
	case MPI_EVENT_INTEGRATED_RAID:
		mptsas_send_raid_event(fw_event);
		break;
	case MPI_EVENT_IR2:
		mptsas_send_ir2_event(fw_event);
		break;
	case MPI_EVENT_PERSISTENT_TABLE_FULL:
		mptbase_sas_persist_operation(ioc,
		    MPI_SAS_OP_CLEAR_NOT_PRESENT);
		mptsas_free_fw_event(ioc, fw_event);
		break;
	case MPI_EVENT_SAS_BROADCAST_PRIMITIVE:
		mptsas_broadcast_primative_work(fw_event);
		break;
	case MPI_EVENT_SAS_EXPANDER_STATUS_CHANGE:
		mptsas_send_expander_event(fw_event);
		break;
	case MPI_EVENT_SAS_PHY_LINK_STATUS:
		mptsas_send_link_status_event(fw_event);
		break;
	case MPI_EVENT_QUEUE_FULL:
		mptsas_handle_queue_full_event(fw_event);
		break;
	}
}



static int
mptsas_slave_configure(struct scsi_device *sdev)
{
	struct Scsi_Host	*host = sdev->host;
	MPT_SCSI_HOST	*hd = shost_priv(host);
	MPT_ADAPTER	*ioc = hd->ioc;
	VirtDevice	*vdevice = sdev->hostdata;

	if (vdevice->vtarget->deleted) {
		sdev_printk(KERN_INFO, sdev, "clearing deleted flag\n");
		vdevice->vtarget->deleted = 0;
	}

	/*
	 * RAID volumes placed beyond the last expected port.
	 * Ignore sending sas mode pages in that case..
	 */
	if (sdev->channel == MPTSAS_RAID_CHANNEL) {
		mptsas_add_device_component_starget_ir(ioc, scsi_target(sdev));
		goto out;
	}

	sas_read_port_mode_page(sdev);

	mptsas_add_device_component_starget(ioc, scsi_target(sdev));

 out:
	return mptscsih_slave_configure(sdev);
}

static int
mptsas_target_alloc(struct scsi_target *starget)
{
	struct Scsi_Host *host = dev_to_shost(&starget->dev);
	MPT_SCSI_HOST		*hd = shost_priv(host);
	VirtTarget		*vtarget;
	u8			id, channel;
	struct sas_rphy		*rphy;
	struct mptsas_portinfo	*p;
	int 			 i;
	MPT_ADAPTER		*ioc = hd->ioc;

	vtarget = kzalloc(sizeof(VirtTarget), GFP_KERNEL);
	if (!vtarget)
		return -ENOMEM;

	vtarget->starget = starget;
	vtarget->ioc_id = ioc->id;
	vtarget->tflags = MPT_TARGET_FLAGS_Q_YES;
	id = starget->id;
	channel = 0;

	/*
	 * RAID volumes placed beyond the last expected port.
	 */
	if (starget->channel == MPTSAS_RAID_CHANNEL) {
		if (!ioc->raid_data.pIocPg2) {
			kfree(vtarget);
			return -ENXIO;
		}
		for (i = 0; i < ioc->raid_data.pIocPg2->NumActiveVolumes; i++) {
			if (id == ioc->raid_data.pIocPg2->
					RaidVolume[i].VolumeID) {
				channel = ioc->raid_data.pIocPg2->
					RaidVolume[i].VolumeBus;
			}
		}
		vtarget->raidVolume = 1;
		goto out;
	}

	rphy = dev_to_rphy(starget->dev.parent);
	mutex_lock(&ioc->sas_topology_mutex);
	list_for_each_entry(p, &ioc->sas_topology, list) {
		for (i = 0; i < p->num_phys; i++) {
			if (p->phy_info[i].attached.sas_address !=
					rphy->identify.sas_address)
				continue;
			id = p->phy_info[i].attached.id;
			channel = p->phy_info[i].attached.channel;
			mptsas_set_starget(&p->phy_info[i], starget);

			/*
			 * Exposing hidden raid components
			 */
			if (mptscsih_is_phys_disk(ioc, channel, id)) {
				id = mptscsih_raid_id_to_num(ioc,
						channel, id);
				vtarget->tflags |=
				    MPT_TARGET_FLAGS_RAID_COMPONENT;
				p->phy_info[i].attached.phys_disk_num = id;
			}
			mutex_unlock(&ioc->sas_topology_mutex);
			goto out;
		}
	}
	mutex_unlock(&ioc->sas_topology_mutex);

	kfree(vtarget);
	return -ENXIO;

 out:
	vtarget->id = id;
	vtarget->channel = channel;
	starget->hostdata = vtarget;
	return 0;
}

static void
mptsas_target_destroy(struct scsi_target *starget)
{
	struct Scsi_Host *host = dev_to_shost(&starget->dev);
	MPT_SCSI_HOST		*hd = shost_priv(host);
	struct sas_rphy		*rphy;
	struct mptsas_portinfo	*p;
	int 			 i;
	MPT_ADAPTER	*ioc = hd->ioc;
	VirtTarget	*vtarget;

	if (!starget->hostdata)
		return;

	vtarget = starget->hostdata;

	mptsas_del_device_component_by_os(ioc, starget->channel,
	    starget->id);


	if (starget->channel == MPTSAS_RAID_CHANNEL)
		goto out;

	rphy = dev_to_rphy(starget->dev.parent);
	list_for_each_entry(p, &ioc->sas_topology, list) {
		for (i = 0; i < p->num_phys; i++) {
			if (p->phy_info[i].attached.sas_address !=
					rphy->identify.sas_address)
				continue;

			starget_printk(KERN_INFO, starget, MYIOC_s_FMT
			"delete device: fw_channel %d, fw_id %d, phy %d, "
			"sas_addr 0x%llx\n", ioc->name,
			p->phy_info[i].attached.channel,
			p->phy_info[i].attached.id,
			p->phy_info[i].attached.phy_id, (unsigned long long)
			p->phy_info[i].attached.sas_address);

			mptsas_set_starget(&p->phy_info[i], NULL);
		}
	}

 out:
	vtarget->starget = NULL;
	kfree(starget->hostdata);
	starget->hostdata = NULL;
}


static int
mptsas_slave_alloc(struct scsi_device *sdev)
{
	struct Scsi_Host	*host = sdev->host;
	MPT_SCSI_HOST		*hd = shost_priv(host);
	struct sas_rphy		*rphy;
	struct mptsas_portinfo	*p;
	VirtDevice		*vdevice;
	struct scsi_target 	*starget;
	int 			i;
	MPT_ADAPTER *ioc = hd->ioc;

	vdevice = kzalloc(sizeof(VirtDevice), GFP_KERNEL);
	if (!vdevice) {
		printk(MYIOC_s_ERR_FMT "slave_alloc kzalloc(%zd) FAILED!\n",
				ioc->name, sizeof(VirtDevice));
		return -ENOMEM;
	}
	starget = scsi_target(sdev);
	vdevice->vtarget = starget->hostdata;

	if (sdev->channel == MPTSAS_RAID_CHANNEL)
		goto out;

	rphy = dev_to_rphy(sdev->sdev_target->dev.parent);
	mutex_lock(&ioc->sas_topology_mutex);
	list_for_each_entry(p, &ioc->sas_topology, list) {
		for (i = 0; i < p->num_phys; i++) {
			if (p->phy_info[i].attached.sas_address !=
					rphy->identify.sas_address)
				continue;
			vdevice->lun = sdev->lun;
			/*
			 * Exposing hidden raid components
			 */
			if (mptscsih_is_phys_disk(ioc,
			    p->phy_info[i].attached.channel,
			    p->phy_info[i].attached.id))
				sdev->no_uld_attach = 1;
			mutex_unlock(&ioc->sas_topology_mutex);
			goto out;
		}
	}
	mutex_unlock(&ioc->sas_topology_mutex);

	kfree(vdevice);
	return -ENXIO;

 out:
	vdevice->vtarget->num_luns++;
	sdev->hostdata = vdevice;
	return 0;
}

static int
mptsas_qcmd(struct scsi_cmnd *SCpnt, void (*done)(struct scsi_cmnd *))
{
	MPT_SCSI_HOST	*hd;
	MPT_ADAPTER	*ioc;
	VirtDevice	*vdevice = SCpnt->device->hostdata;

	if (!vdevice || !vdevice->vtarget || vdevice->vtarget->deleted) {
		SCpnt->result = DID_NO_CONNECT << 16;
		done(SCpnt);
		return 0;
	}

	hd = shost_priv(SCpnt->device->host);
	ioc = hd->ioc;

	if (ioc->sas_discovery_quiesce_io)
		return SCSI_MLQUEUE_HOST_BUSY;

//	scsi_print_command(SCpnt);

	return mptscsih_qcmd(SCpnt,done);
}


static struct scsi_host_template mptsas_driver_template = {
	.module				= THIS_MODULE,
	.proc_name			= "mptsas",
	.proc_info			= mptscsih_proc_info,
	.name				= "MPT SPI Host",
	.info				= mptscsih_info,
	.queuecommand			= mptsas_qcmd,
	.target_alloc			= mptsas_target_alloc,
	.slave_alloc			= mptsas_slave_alloc,
	.slave_configure		= mptsas_slave_configure,
	.target_destroy			= mptsas_target_destroy,
	.slave_destroy			= mptscsih_slave_destroy,
	.change_queue_depth 		= mptscsih_change_queue_depth,
	.eh_abort_handler		= mptscsih_abort,
	.eh_device_reset_handler	= mptscsih_dev_reset,
	.eh_bus_reset_handler		= mptscsih_bus_reset,
	.eh_host_reset_handler		= mptscsih_host_reset,
	.bios_param			= mptscsih_bios_param,
	.can_queue			= MPT_SAS_CAN_QUEUE,
	.this_id			= -1,
	.sg_tablesize			= MPT_SCSI_SG_DEPTH,
	.max_sectors			= 8192,
	.cmd_per_lun			= 7,
	.use_clustering			= ENABLE_CLUSTERING,
	.shost_attrs			= mptscsih_host_attrs,
};

static int mptsas_get_linkerrors(struct sas_phy *phy)
{
	MPT_ADAPTER *ioc = phy_to_ioc(phy);
	ConfigExtendedPageHeader_t hdr;
	CONFIGPARMS cfg;
	SasPhyPage1_t *buffer;
	dma_addr_t dma_handle;
	int error;

	/* FIXME: only have link errors on local phys */
	if (!scsi_is_sas_phy_local(phy))
		return -EINVAL;

	hdr.PageVersion = MPI_SASPHY1_PAGEVERSION;
	hdr.ExtPageLength = 0;
	hdr.PageNumber = 1 /* page number 1*/;
	hdr.Reserved1 = 0;
	hdr.Reserved2 = 0;
	hdr.PageType = MPI_CONFIG_PAGETYPE_EXTENDED;
	hdr.ExtPageType = MPI_CONFIG_EXTPAGETYPE_SAS_PHY;

	cfg.cfghdr.ehdr = &hdr;
	cfg.physAddr = -1;
	cfg.pageAddr = phy->identify.phy_identifier;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;
	cfg.dir = 0;    /* read */
	cfg.timeout = SAS_CONFIG_PAGE_TIMEOUT;

	error = mpt_config(ioc, &cfg);
	if (error)
		return error;
	if (!hdr.ExtPageLength)
		return -ENXIO;

	buffer = pci_alloc_consistent(ioc->pcidev, hdr.ExtPageLength * 4,
				      &dma_handle);
	if (!buffer)
		return -ENOMEM;

	cfg.physAddr = dma_handle;
	cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;

	error = mpt_config(ioc, &cfg);
	if (error)
		goto out_free_consistent;

	mptsas_print_phy_pg1(ioc, buffer);

	phy->invalid_dword_count = le32_to_cpu(buffer->InvalidDwordCount);
	phy->running_disparity_error_count =
		le32_to_cpu(buffer->RunningDisparityErrorCount);
	phy->loss_of_dword_sync_count =
		le32_to_cpu(buffer->LossDwordSynchCount);
	phy->phy_reset_problem_count =
		le32_to_cpu(buffer->PhyResetProblemCount);

 out_free_consistent:
	pci_free_consistent(ioc->pcidev, hdr.ExtPageLength * 4,
			    buffer, dma_handle);
	return error;
}

static int mptsas_mgmt_done(MPT_ADAPTER *ioc, MPT_FRAME_HDR *req,
		MPT_FRAME_HDR *reply)
{
	ioc->sas_mgmt.status |= MPT_MGMT_STATUS_COMMAND_GOOD;
	if (reply != NULL) {
		ioc->sas_mgmt.status |= MPT_MGMT_STATUS_RF_VALID;
		memcpy(ioc->sas_mgmt.reply, reply,
		    min(ioc->reply_sz, 4 * reply->u.reply.MsgLength));
	}

	if (ioc->sas_mgmt.status & MPT_MGMT_STATUS_PENDING) {
		ioc->sas_mgmt.status &= ~MPT_MGMT_STATUS_PENDING;
		complete(&ioc->sas_mgmt.done);
		return 1;
	}
	return 0;
}

static int mptsas_phy_reset(struct sas_phy *phy, int hard_reset)
{
	MPT_ADAPTER *ioc = phy_to_ioc(phy);
	SasIoUnitControlRequest_t *req;
	SasIoUnitControlReply_t *reply;
	MPT_FRAME_HDR *mf;
	MPIHeader_t *hdr;
	unsigned long timeleft;
	int error = -ERESTARTSYS;

	/* FIXME: fusion doesn't allow non-local phy reset */
	if (!scsi_is_sas_phy_local(phy))
		return -EINVAL;

	/* not implemented for expanders */
	if (phy->identify.target_port_protocols & SAS_PROTOCOL_SMP)
		return -ENXIO;

	if (mutex_lock_interruptible(&ioc->sas_mgmt.mutex))
		goto out;

	mf = mpt_get_msg_frame(mptsasMgmtCtx, ioc);
	if (!mf) {
		error = -ENOMEM;
		goto out_unlock;
	}

	hdr = (MPIHeader_t *) mf;
	req = (SasIoUnitControlRequest_t *)mf;
	memset(req, 0, sizeof(SasIoUnitControlRequest_t));
	req->Function = MPI_FUNCTION_SAS_IO_UNIT_CONTROL;
	req->MsgContext = hdr->MsgContext;
	req->Operation = hard_reset ?
		MPI_SAS_OP_PHY_HARD_RESET : MPI_SAS_OP_PHY_LINK_RESET;
	req->PhyNum = phy->identify.phy_identifier;

	INITIALIZE_MGMT_STATUS(ioc->sas_mgmt.status)
	mpt_put_msg_frame(mptsasMgmtCtx, ioc, mf);

	timeleft = wait_for_completion_timeout(&ioc->sas_mgmt.done,
			10 * HZ);
	if (!timeleft) {
		/* On timeout reset the board */
		mpt_free_msg_frame(ioc, mf);
		mpt_HardResetHandler(ioc, CAN_SLEEP);
		error = -ETIMEDOUT;
		goto out_unlock;
	}

	/* a reply frame is expected */
	if ((ioc->sas_mgmt.status &
	    MPT_MGMT_STATUS_RF_VALID) == 0) {
		error = -ENXIO;
		goto out_unlock;
	}

	/* process the completed Reply Message Frame */
	reply = (SasIoUnitControlReply_t *)ioc->sas_mgmt.reply;
	if (reply->IOCStatus != MPI_IOCSTATUS_SUCCESS) {
		printk(MYIOC_s_INFO_FMT "%s: IOCStatus=0x%X IOCLogInfo=0x%X\n",
		    ioc->name, __func__, reply->IOCStatus, reply->IOCLogInfo);
		error = -ENXIO;
		goto out_unlock;
	}

	error = 0;

 out_unlock:
	CLEAR_MGMT_STATUS(ioc->sas_mgmt.status)
	mutex_unlock(&ioc->sas_mgmt.mutex);
 out:
	return error;
}

static int
mptsas_get_enclosure_identifier(struct sas_rphy *rphy, u64 *identifier)
{
	MPT_ADAPTER *ioc = rphy_to_ioc(rphy);
	int i, error;
	struct mptsas_portinfo *p;
	struct mptsas_enclosure enclosure_info;
	u64 enclosure_handle;

	mutex_lock(&ioc->sas_topology_mutex);
	list_for_each_entry(p, &ioc->sas_topology, list) {
		for (i = 0; i < p->num_phys; i++) {
			if (p->phy_info[i].attached.sas_address ==
			    rphy->identify.sas_address) {
				enclosure_handle = p->phy_info[i].
					attached.handle_enclosure;
				goto found_info;
			}
		}
	}
	mutex_unlock(&ioc->sas_topology_mutex);
	return -ENXIO;

 found_info:
	mutex_unlock(&ioc->sas_topology_mutex);
	memset(&enclosure_info, 0, sizeof(struct mptsas_enclosure));
	error = mptsas_sas_enclosure_pg0(ioc, &enclosure_info,
			(MPI_SAS_ENCLOS_PGAD_FORM_HANDLE <<
			 MPI_SAS_ENCLOS_PGAD_FORM_SHIFT), enclosure_handle);
	if (!error)
		*identifier = enclosure_info.enclosure_logical_id;
	return error;
}

static int
mptsas_get_bay_identifier(struct sas_rphy *rphy)
{
	MPT_ADAPTER *ioc = rphy_to_ioc(rphy);
	struct mptsas_portinfo *p;
	int i, rc;

	mutex_lock(&ioc->sas_topology_mutex);
	list_for_each_entry(p, &ioc->sas_topology, list) {
		for (i = 0; i < p->num_phys; i++) {
			if (p->phy_info[i].attached.sas_address ==
			    rphy->identify.sas_address) {
				rc = p->phy_info[i].attached.slot;
				goto out;
			}
		}
	}
	rc = -ENXIO;
 out:
	mutex_unlock(&ioc->sas_topology_mutex);
	return rc;
}

static int mptsas_smp_handler(struct Scsi_Host *shost, struct sas_rphy *rphy,
			      struct request *req)
{
	MPT_ADAPTER *ioc = ((MPT_SCSI_HOST *) shost->hostdata)->ioc;
	MPT_FRAME_HDR *mf;
	SmpPassthroughRequest_t *smpreq;
	struct request *rsp = req->next_rq;
	int ret;
	int flagsLength;
	unsigned long timeleft;
	char *psge;
	dma_addr_t dma_addr_in = 0;
	dma_addr_t dma_addr_out = 0;
	u64 sas_address = 0;

	if (!rsp) {
		printk(MYIOC_s_ERR_FMT "%s: the smp response space is missing\n",
		    ioc->name, __func__);
		return -EINVAL;
	}

	/* do we need to support multiple segments? */
	if (req->bio->bi_vcnt > 1 || rsp->bio->bi_vcnt > 1) {
		printk(MYIOC_s_ERR_FMT "%s: multiple segments req %u %u, rsp %u %u\n",
		    ioc->name, __func__, req->bio->bi_vcnt, blk_rq_bytes(req),
		    rsp->bio->bi_vcnt, blk_rq_bytes(rsp));
		return -EINVAL;
	}

	ret = mutex_lock_interruptible(&ioc->sas_mgmt.mutex);
	if (ret)
		goto out;

	mf = mpt_get_msg_frame(mptsasMgmtCtx, ioc);
	if (!mf) {
		ret = -ENOMEM;
		goto out_unlock;
	}

	smpreq = (SmpPassthroughRequest_t *)mf;
	memset(smpreq, 0, sizeof(*smpreq));

	smpreq->RequestDataLength = cpu_to_le16(blk_rq_bytes(req) - 4);
	smpreq->Function = MPI_FUNCTION_SMP_PASSTHROUGH;

	if (rphy)
		sas_address = rphy->identify.sas_address;
	else {
		struct mptsas_portinfo *port_info;

		mutex_lock(&ioc->sas_topology_mutex);
		port_info = ioc->hba_port_info;
		if (port_info && port_info->phy_info)
			sas_address =
				port_info->phy_info[0].phy->identify.sas_address;
		mutex_unlock(&ioc->sas_topology_mutex);
	}

	*((u64 *)&smpreq->SASAddress) = cpu_to_le64(sas_address);

	psge = (char *)
		(((int *) mf) + (offsetof(SmpPassthroughRequest_t, SGL) / 4));

	/* request */
	flagsLength = (MPI_SGE_FLAGS_SIMPLE_ELEMENT |
		       MPI_SGE_FLAGS_END_OF_BUFFER |
		       MPI_SGE_FLAGS_DIRECTION)
		       << MPI_SGE_FLAGS_SHIFT;
	flagsLength |= (blk_rq_bytes(req) - 4);

	dma_addr_out = pci_map_single(ioc->pcidev, bio_data(req->bio),
				      blk_rq_bytes(req), PCI_DMA_BIDIRECTIONAL);
	if (!dma_addr_out)
		goto put_mf;
	ioc->add_sge(psge, flagsLength, dma_addr_out);
	psge += ioc->SGE_size;

	/* response */
	flagsLength = MPI_SGE_FLAGS_SIMPLE_ELEMENT |
		MPI_SGE_FLAGS_SYSTEM_ADDRESS |
		MPI_SGE_FLAGS_IOC_TO_HOST |
		MPI_SGE_FLAGS_END_OF_BUFFER;

	flagsLength = flagsLength << MPI_SGE_FLAGS_SHIFT;
	flagsLength |= blk_rq_bytes(rsp) + 4;
	dma_addr_in =  pci_map_single(ioc->pcidev, bio_data(rsp->bio),
				      blk_rq_bytes(rsp), PCI_DMA_BIDIRECTIONAL);
	if (!dma_addr_in)
		goto unmap;
	ioc->add_sge(psge, flagsLength, dma_addr_in);

	INITIALIZE_MGMT_STATUS(ioc->sas_mgmt.status)
	mpt_put_msg_frame(mptsasMgmtCtx, ioc, mf);

	timeleft = wait_for_completion_timeout(&ioc->sas_mgmt.done, 10 * HZ);
	if (!timeleft) {
		printk(MYIOC_s_ERR_FMT "%s: smp timeout!\n", ioc->name, __func__);
		/* On timeout reset the board */
		mpt_HardResetHandler(ioc, CAN_SLEEP);
		ret = -ETIMEDOUT;
		goto unmap;
	}
	mf = NULL;

	if (ioc->sas_mgmt.status & MPT_MGMT_STATUS_RF_VALID) {
		SmpPassthroughReply_t *smprep;

		smprep = (SmpPassthroughReply_t *)ioc->sas_mgmt.reply;
		memcpy(req->sense, smprep, sizeof(*smprep));
		req->sense_len = sizeof(*smprep);
		req->resid_len = 0;
		rsp->resid_len -= smprep->ResponseDataLength;
	} else {
		printk(MYIOC_s_ERR_FMT
		    "%s: smp passthru reply failed to be returned\n",
		    ioc->name, __func__);
		ret = -ENXIO;
	}
unmap:
	if (dma_addr_out)
		pci_unmap_single(ioc->pcidev, dma_addr_out, blk_rq_bytes(req),
				 PCI_DMA_BIDIRECTIONAL);
	if (dma_addr_in)
		pci_unmap_single(ioc->pcidev, dma_addr_in, blk_rq_bytes(rsp),
				 PCI_DMA_BIDIRECTIONAL);
put_mf:
	if (mf)
		mpt_free_msg_frame(ioc, mf);
out_unlock:
	CLEAR_MGMT_STATUS(ioc->sas_mgmt.status)
	mutex_unlock(&ioc->sas_mgmt.mutex);
out:
	return ret;
}

static struct sas_function_template mptsas_transport_functions = {
	.get_linkerrors		= mptsas_get_linkerrors,
	.get_enclosure_identifier = mptsas_get_enclosure_identifier,
	.get_bay_identifier	= mptsas_get_bay_identifier,
	.phy_reset		= mptsas_phy_reset,
	.smp_handler		= mptsas_smp_handler,
};

static struct scsi_transport_template *mptsas_transport_template;

static int
mptsas_sas_io_unit_pg0(MPT_ADAPTER *ioc, struct mptsas_portinfo *port_info)
{
	ConfigExtendedPageHeader_t hdr;
	CONFIGPARMS cfg;
	SasIOUnitPage0_t *buffer;
	dma_addr_t dma_handle;
	int error, i;

	hdr.PageVersion = MPI_SASIOUNITPAGE0_PAGEVERSION;
	hdr.ExtPageLength = 0;
	hdr.PageNumber = 0;
	hdr.Reserved1 = 0;
	hdr.Reserved2 = 0;
	hdr.PageType = MPI_CONFIG_PAGETYPE_EXTENDED;
	hdr.ExtPageType = MPI_CONFIG_EXTPAGETYPE_SAS_IO_UNIT;

	cfg.cfghdr.ehdr = &hdr;
	cfg.physAddr = -1;
	cfg.pageAddr = 0;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;
	cfg.dir = 0;	/* read */
	cfg.timeout = SAS_CONFIG_PAGE_TIMEOUT;

	error = mpt_config(ioc, &cfg);
	if (error)
		goto out;
	if (!hdr.ExtPageLength) {
		error = -ENXIO;
		goto out;
	}

	buffer = pci_alloc_consistent(ioc->pcidev, hdr.ExtPageLength * 4,
					    &dma_handle);
	if (!buffer) {
		error = -ENOMEM;
		goto out;
	}

	cfg.physAddr = dma_handle;
	cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;

	error = mpt_config(ioc, &cfg);
	if (error)
		goto out_free_consistent;

	port_info->num_phys = buffer->NumPhys;
	port_info->phy_info = kcalloc(port_info->num_phys,
		sizeof(struct mptsas_phyinfo), GFP_KERNEL);
	if (!port_info->phy_info) {
		error = -ENOMEM;
		goto out_free_consistent;
	}

	ioc->nvdata_version_persistent =
	    le16_to_cpu(buffer->NvdataVersionPersistent);
	ioc->nvdata_version_default =
	    le16_to_cpu(buffer->NvdataVersionDefault);

	for (i = 0; i < port_info->num_phys; i++) {
		mptsas_print_phy_data(ioc, &buffer->PhyData[i]);
		port_info->phy_info[i].phy_id = i;
		port_info->phy_info[i].port_id =
		    buffer->PhyData[i].Port;
		port_info->phy_info[i].negotiated_link_rate =
		    buffer->PhyData[i].NegotiatedLinkRate;
		port_info->phy_info[i].portinfo = port_info;
		port_info->phy_info[i].handle =
		    le16_to_cpu(buffer->PhyData[i].ControllerDevHandle);
	}

 out_free_consistent:
	pci_free_consistent(ioc->pcidev, hdr.ExtPageLength * 4,
			    buffer, dma_handle);
 out:
	return error;
}

static int
mptsas_sas_io_unit_pg1(MPT_ADAPTER *ioc)
{
	ConfigExtendedPageHeader_t hdr;
	CONFIGPARMS cfg;
	SasIOUnitPage1_t *buffer;
	dma_addr_t dma_handle;
	int error;
	u16 device_missing_delay;

	memset(&hdr, 0, sizeof(ConfigExtendedPageHeader_t));
	memset(&cfg, 0, sizeof(CONFIGPARMS));

	cfg.cfghdr.ehdr = &hdr;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;
	cfg.timeout = SAS_CONFIG_PAGE_TIMEOUT;
	cfg.cfghdr.ehdr->PageType = MPI_CONFIG_PAGETYPE_EXTENDED;
	cfg.cfghdr.ehdr->ExtPageType = MPI_CONFIG_EXTPAGETYPE_SAS_IO_UNIT;
	cfg.cfghdr.ehdr->PageVersion = MPI_SASIOUNITPAGE1_PAGEVERSION;
	cfg.cfghdr.ehdr->PageNumber = 1;

	error = mpt_config(ioc, &cfg);
	if (error)
		goto out;
	if (!hdr.ExtPageLength) {
		error = -ENXIO;
		goto out;
	}

	buffer = pci_alloc_consistent(ioc->pcidev, hdr.ExtPageLength * 4,
					    &dma_handle);
	if (!buffer) {
		error = -ENOMEM;
		goto out;
	}

	cfg.physAddr = dma_handle;
	cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;

	error = mpt_config(ioc, &cfg);
	if (error)
		goto out_free_consistent;

	ioc->io_missing_delay  =
	    le16_to_cpu(buffer->IODeviceMissingDelay);
	device_missing_delay = le16_to_cpu(buffer->ReportDeviceMissingDelay);
	ioc->device_missing_delay = (device_missing_delay & MPI_SAS_IOUNIT1_REPORT_MISSING_UNIT_16) ?
	    (device_missing_delay & MPI_SAS_IOUNIT1_REPORT_MISSING_TIMEOUT_MASK) * 16 :
	    device_missing_delay & MPI_SAS_IOUNIT1_REPORT_MISSING_TIMEOUT_MASK;

 out_free_consistent:
	pci_free_consistent(ioc->pcidev, hdr.ExtPageLength * 4,
			    buffer, dma_handle);
 out:
	return error;
}

static int
mptsas_sas_phy_pg0(MPT_ADAPTER *ioc, struct mptsas_phyinfo *phy_info,
		u32 form, u32 form_specific)
{
	ConfigExtendedPageHeader_t hdr;
	CONFIGPARMS cfg;
	SasPhyPage0_t *buffer;
	dma_addr_t dma_handle;
	int error;

	hdr.PageVersion = MPI_SASPHY0_PAGEVERSION;
	hdr.ExtPageLength = 0;
	hdr.PageNumber = 0;
	hdr.Reserved1 = 0;
	hdr.Reserved2 = 0;
	hdr.PageType = MPI_CONFIG_PAGETYPE_EXTENDED;
	hdr.ExtPageType = MPI_CONFIG_EXTPAGETYPE_SAS_PHY;

	cfg.cfghdr.ehdr = &hdr;
	cfg.dir = 0;	/* read */
	cfg.timeout = SAS_CONFIG_PAGE_TIMEOUT;

	/* Get Phy Pg 0 for each Phy. */
	cfg.physAddr = -1;
	cfg.pageAddr = form + form_specific;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;

	error = mpt_config(ioc, &cfg);
	if (error)
		goto out;

	if (!hdr.ExtPageLength) {
		error = -ENXIO;
		goto out;
	}

	buffer = pci_alloc_consistent(ioc->pcidev, hdr.ExtPageLength * 4,
				      &dma_handle);
	if (!buffer) {
		error = -ENOMEM;
		goto out;
	}

	cfg.physAddr = dma_handle;
	cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;

	error = mpt_config(ioc, &cfg);
	if (error)
		goto out_free_consistent;

	mptsas_print_phy_pg0(ioc, buffer);

	phy_info->hw_link_rate = buffer->HwLinkRate;
	phy_info->programmed_link_rate = buffer->ProgrammedLinkRate;
	phy_info->identify.handle = le16_to_cpu(buffer->OwnerDevHandle);
	phy_info->attached.handle = le16_to_cpu(buffer->AttachedDevHandle);

 out_free_consistent:
	pci_free_consistent(ioc->pcidev, hdr.ExtPageLength * 4,
			    buffer, dma_handle);
 out:
	return error;
}

static int
mptsas_sas_device_pg0(MPT_ADAPTER *ioc, struct mptsas_devinfo *device_info,
		u32 form, u32 form_specific)
{
	ConfigExtendedPageHeader_t hdr;
	CONFIGPARMS cfg;
	SasDevicePage0_t *buffer;
	dma_addr_t dma_handle;
	__le64 sas_address;
	int error=0;

	hdr.PageVersion = MPI_SASDEVICE0_PAGEVERSION;
	hdr.ExtPageLength = 0;
	hdr.PageNumber = 0;
	hdr.Reserved1 = 0;
	hdr.Reserved2 = 0;
	hdr.PageType = MPI_CONFIG_PAGETYPE_EXTENDED;
	hdr.ExtPageType = MPI_CONFIG_EXTPAGETYPE_SAS_DEVICE;

	cfg.cfghdr.ehdr = &hdr;
	cfg.pageAddr = form + form_specific;
	cfg.physAddr = -1;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;
	cfg.dir = 0;	/* read */
	cfg.timeout = SAS_CONFIG_PAGE_TIMEOUT;

	memset(device_info, 0, sizeof(struct mptsas_devinfo));
	error = mpt_config(ioc, &cfg);
	if (error)
		goto out;
	if (!hdr.ExtPageLength) {
		error = -ENXIO;
		goto out;
	}

	buffer = pci_alloc_consistent(ioc->pcidev, hdr.ExtPageLength * 4,
				      &dma_handle);
	if (!buffer) {
		error = -ENOMEM;
		goto out;
	}

	cfg.physAddr = dma_handle;
	cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;

	error = mpt_config(ioc, &cfg);
	if (error)
		goto out_free_consistent;

	mptsas_print_device_pg0(ioc, buffer);

	memset(device_info, 0, sizeof(struct mptsas_devinfo));
	device_info->handle = le16_to_cpu(buffer->DevHandle);
	device_info->handle_parent = le16_to_cpu(buffer->ParentDevHandle);
	device_info->handle_enclosure =
	    le16_to_cpu(buffer->EnclosureHandle);
	device_info->slot = le16_to_cpu(buffer->Slot);
	device_info->phy_id = buffer->PhyNum;
	device_info->port_id = buffer->PhysicalPort;
	device_info->id = buffer->TargetID;
	device_info->phys_disk_num = ~0;
	device_info->channel = buffer->Bus;
	memcpy(&sas_address, &buffer->SASAddress, sizeof(__le64));
	device_info->sas_address = le64_to_cpu(sas_address);
	device_info->device_info =
	    le32_to_cpu(buffer->DeviceInfo);

 out_free_consistent:
	pci_free_consistent(ioc->pcidev, hdr.ExtPageLength * 4,
			    buffer, dma_handle);
 out:
	return error;
}

static int
mptsas_sas_expander_pg0(MPT_ADAPTER *ioc, struct mptsas_portinfo *port_info,
		u32 form, u32 form_specific)
{
	ConfigExtendedPageHeader_t hdr;
	CONFIGPARMS cfg;
	SasExpanderPage0_t *buffer;
	dma_addr_t dma_handle;
	int i, error;
	__le64 sas_address;

	memset(port_info, 0, sizeof(struct mptsas_portinfo));
	hdr.PageVersion = MPI_SASEXPANDER0_PAGEVERSION;
	hdr.ExtPageLength = 0;
	hdr.PageNumber = 0;
	hdr.Reserved1 = 0;
	hdr.Reserved2 = 0;
	hdr.PageType = MPI_CONFIG_PAGETYPE_EXTENDED;
	hdr.ExtPageType = MPI_CONFIG_EXTPAGETYPE_SAS_EXPANDER;

	cfg.cfghdr.ehdr = &hdr;
	cfg.physAddr = -1;
	cfg.pageAddr = form + form_specific;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;
	cfg.dir = 0;	/* read */
	cfg.timeout = SAS_CONFIG_PAGE_TIMEOUT;

	memset(port_info, 0, sizeof(struct mptsas_portinfo));
	error = mpt_config(ioc, &cfg);
	if (error)
		goto out;

	if (!hdr.ExtPageLength) {
		error = -ENXIO;
		goto out;
	}

	buffer = pci_alloc_consistent(ioc->pcidev, hdr.ExtPageLength * 4,
				      &dma_handle);
	if (!buffer) {
		error = -ENOMEM;
		goto out;
	}

	cfg.physAddr = dma_handle;
	cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;

	error = mpt_config(ioc, &cfg);
	if (error)
		goto out_free_consistent;

	if (!buffer->NumPhys) {
		error = -ENODEV;
		goto out_free_consistent;
	}

	/* save config data */
	port_info->num_phys = (buffer->NumPhys) ? buffer->NumPhys : 1;
	port_info->phy_info = kcalloc(port_info->num_phys,
		sizeof(struct mptsas_phyinfo), GFP_KERNEL);
	if (!port_info->phy_info) {
		error = -ENOMEM;
		goto out_free_consistent;
	}

	memcpy(&sas_address, &buffer->SASAddress, sizeof(__le64));
	for (i = 0; i < port_info->num_phys; i++) {
		port_info->phy_info[i].portinfo = port_info;
		port_info->phy_info[i].handle =
		    le16_to_cpu(buffer->DevHandle);
		port_info->phy_info[i].identify.sas_address =
		    le64_to_cpu(sas_address);
		port_info->phy_info[i].identify.handle_parent =
		    le16_to_cpu(buffer->ParentDevHandle);
	}

 out_free_consistent:
	pci_free_consistent(ioc->pcidev, hdr.ExtPageLength * 4,
			    buffer, dma_handle);
 out:
	return error;
}

static int
mptsas_sas_expander_pg1(MPT_ADAPTER *ioc, struct mptsas_phyinfo *phy_info,
		u32 form, u32 form_specific)
{
	ConfigExtendedPageHeader_t hdr;
	CONFIGPARMS cfg;
	SasExpanderPage1_t *buffer;
	dma_addr_t dma_handle;
	int error=0;

	hdr.PageVersion = MPI_SASEXPANDER1_PAGEVERSION;
	hdr.ExtPageLength = 0;
	hdr.PageNumber = 1;
	hdr.Reserved1 = 0;
	hdr.Reserved2 = 0;
	hdr.PageType = MPI_CONFIG_PAGETYPE_EXTENDED;
	hdr.ExtPageType = MPI_CONFIG_EXTPAGETYPE_SAS_EXPANDER;

	cfg.cfghdr.ehdr = &hdr;
	cfg.physAddr = -1;
	cfg.pageAddr = form + form_specific;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;
	cfg.dir = 0;	/* read */
	cfg.timeout = SAS_CONFIG_PAGE_TIMEOUT;

	error = mpt_config(ioc, &cfg);
	if (error)
		goto out;

	if (!hdr.ExtPageLength) {
		error = -ENXIO;
		goto out;
	}

	buffer = pci_alloc_consistent(ioc->pcidev, hdr.ExtPageLength * 4,
				      &dma_handle);
	if (!buffer) {
		error = -ENOMEM;
		goto out;
	}

	cfg.physAddr = dma_handle;
	cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;

	error = mpt_config(ioc, &cfg);

	if (error == MPI_IOCSTATUS_CONFIG_INVALID_PAGE) {
		error = -ENODEV;
		goto out;
	}

	if (error)
		goto out_free_consistent;


	mptsas_print_expander_pg1(ioc, buffer);

	/* save config data */
	phy_info->phy_id = buffer->PhyIdentifier;
	phy_info->port_id = buffer->PhysicalPort;
	phy_info->negotiated_link_rate = buffer->NegotiatedLinkRate;
	phy_info->programmed_link_rate = buffer->ProgrammedLinkRate;
	phy_info->hw_link_rate = buffer->HwLinkRate;
	phy_info->identify.handle = le16_to_cpu(buffer->OwnerDevHandle);
	phy_info->attached.handle = le16_to_cpu(buffer->AttachedDevHandle);

 out_free_consistent:
	pci_free_consistent(ioc->pcidev, hdr.ExtPageLength * 4,
			    buffer, dma_handle);
 out:
	return error;
}

static void
mptsas_parse_device_info(struct sas_identify *identify,
		struct mptsas_devinfo *device_info)
{
	u16 protocols;

	identify->sas_address = device_info->sas_address;
	identify->phy_identifier = device_info->phy_id;

	/*
	 * Fill in Phy Initiator Port Protocol.
	 * Bits 6:3, more than one bit can be set, fall through cases.
	 */
	protocols = device_info->device_info & 0x78;
	identify->initiator_port_protocols = 0;
	if (protocols & MPI_SAS_DEVICE_INFO_SSP_INITIATOR)
		identify->initiator_port_protocols |= SAS_PROTOCOL_SSP;
	if (protocols & MPI_SAS_DEVICE_INFO_STP_INITIATOR)
		identify->initiator_port_protocols |= SAS_PROTOCOL_STP;
	if (protocols & MPI_SAS_DEVICE_INFO_SMP_INITIATOR)
		identify->initiator_port_protocols |= SAS_PROTOCOL_SMP;
	if (protocols & MPI_SAS_DEVICE_INFO_SATA_HOST)
		identify->initiator_port_protocols |= SAS_PROTOCOL_SATA;

	/*
	 * Fill in Phy Target Port Protocol.
	 * Bits 10:7, more than one bit can be set, fall through cases.
	 */
	protocols = device_info->device_info & 0x780;
	identify->target_port_protocols = 0;
	if (protocols & MPI_SAS_DEVICE_INFO_SSP_TARGET)
		identify->target_port_protocols |= SAS_PROTOCOL_SSP;
	if (protocols & MPI_SAS_DEVICE_INFO_STP_TARGET)
		identify->target_port_protocols |= SAS_PROTOCOL_STP;
	if (protocols & MPI_SAS_DEVICE_INFO_SMP_TARGET)
		identify->target_port_protocols |= SAS_PROTOCOL_SMP;
	if (protocols & MPI_SAS_DEVICE_INFO_SATA_DEVICE)
		identify->target_port_protocols |= SAS_PROTOCOL_SATA;

	/*
	 * Fill in Attached device type.
	 */
	switch (device_info->device_info &
			MPI_SAS_DEVICE_INFO_MASK_DEVICE_TYPE) {
	case MPI_SAS_DEVICE_INFO_NO_DEVICE:
		identify->device_type = SAS_PHY_UNUSED;
		break;
	case MPI_SAS_DEVICE_INFO_END_DEVICE:
		identify->device_type = SAS_END_DEVICE;
		break;
	case MPI_SAS_DEVICE_INFO_EDGE_EXPANDER:
		identify->device_type = SAS_EDGE_EXPANDER_DEVICE;
		break;
	case MPI_SAS_DEVICE_INFO_FANOUT_EXPANDER:
		identify->device_type = SAS_FANOUT_EXPANDER_DEVICE;
		break;
	}
}

static int mptsas_probe_one_phy(struct device *dev,
		struct mptsas_phyinfo *phy_info, int index, int local)
{
	MPT_ADAPTER *ioc;
	struct sas_phy *phy;
	struct sas_port *port;
	int error = 0;

	if (!dev) {
		error = -ENODEV;
		goto out;
	}

	if (!phy_info->phy) {
		phy = sas_phy_alloc(dev, index);
		if (!phy) {
			error = -ENOMEM;
			goto out;
		}
	} else
		phy = phy_info->phy;

	mptsas_parse_device_info(&phy->identify, &phy_info->identify);

	/*
	 * Set Negotiated link rate.
	 */
	switch (phy_info->negotiated_link_rate) {
	case MPI_SAS_IOUNIT0_RATE_PHY_DISABLED:
		phy->negotiated_linkrate = SAS_PHY_DISABLED;
		break;
	case MPI_SAS_IOUNIT0_RATE_FAILED_SPEED_NEGOTIATION:
		phy->negotiated_linkrate = SAS_LINK_RATE_FAILED;
		break;
	case MPI_SAS_IOUNIT0_RATE_1_5:
		phy->negotiated_linkrate = SAS_LINK_RATE_1_5_GBPS;
		break;
	case MPI_SAS_IOUNIT0_RATE_3_0:
		phy->negotiated_linkrate = SAS_LINK_RATE_3_0_GBPS;
		break;
	case MPI_SAS_IOUNIT0_RATE_SATA_OOB_COMPLETE:
	case MPI_SAS_IOUNIT0_RATE_UNKNOWN:
	default:
		phy->negotiated_linkrate = SAS_LINK_RATE_UNKNOWN;
		break;
	}

	/*
	 * Set Max hardware link rate.
	 */
	switch (phy_info->hw_link_rate & MPI_SAS_PHY0_PRATE_MAX_RATE_MASK) {
	case MPI_SAS_PHY0_HWRATE_MAX_RATE_1_5:
		phy->maximum_linkrate_hw = SAS_LINK_RATE_1_5_GBPS;
		break;
	case MPI_SAS_PHY0_PRATE_MAX_RATE_3_0:
		phy->maximum_linkrate_hw = SAS_LINK_RATE_3_0_GBPS;
		break;
	default:
		break;
	}

	/*
	 * Set Max programmed link rate.
	 */
	switch (phy_info->programmed_link_rate &
			MPI_SAS_PHY0_PRATE_MAX_RATE_MASK) {
	case MPI_SAS_PHY0_PRATE_MAX_RATE_1_5:
		phy->maximum_linkrate = SAS_LINK_RATE_1_5_GBPS;
		break;
	case MPI_SAS_PHY0_PRATE_MAX_RATE_3_0:
		phy->maximum_linkrate = SAS_LINK_RATE_3_0_GBPS;
		break;
	default:
		break;
	}

	/*
	 * Set Min hardware link rate.
	 */
	switch (phy_info->hw_link_rate & MPI_SAS_PHY0_HWRATE_MIN_RATE_MASK) {
	case MPI_SAS_PHY0_HWRATE_MIN_RATE_1_5:
		phy->minimum_linkrate_hw = SAS_LINK_RATE_1_5_GBPS;
		break;
	case MPI_SAS_PHY0_PRATE_MIN_RATE_3_0:
		phy->minimum_linkrate_hw = SAS_LINK_RATE_3_0_GBPS;
		break;
	default:
		break;
	}

	/*
	 * Set Min programmed link rate.
	 */
	switch (phy_info->programmed_link_rate &
			MPI_SAS_PHY0_PRATE_MIN_RATE_MASK) {
	case MPI_SAS_PHY0_PRATE_MIN_RATE_1_5:
		phy->minimum_linkrate = SAS_LINK_RATE_1_5_GBPS;
		break;
	case MPI_SAS_PHY0_PRATE_MIN_RATE_3_0:
		phy->minimum_linkrate = SAS_LINK_RATE_3_0_GBPS;
		break;
	default:
		break;
	}

	if (!phy_info->phy) {

		error = sas_phy_add(phy);
		if (error) {
			sas_phy_free(phy);
			goto out;
		}
		phy_info->phy = phy;
	}

	if (!phy_info->attached.handle ||
			!phy_info->port_details)
		goto out;

	port = mptsas_get_port(phy_info);
	ioc = phy_to_ioc(phy_info->phy);

	if (phy_info->sas_port_add_phy) {

		if (!port) {
			port = sas_port_alloc_num(dev);
			if (!port) {
				error = -ENOMEM;
				goto out;
			}
			error = sas_port_add(port);
			if (error) {
				dfailprintk(ioc, printk(MYIOC_s_ERR_FMT
					"%s: exit at line=%d\n", ioc->name,
					__func__, __LINE__));
				goto out;
			}
			mptsas_set_port(ioc, phy_info, port);
			devtprintk(ioc, dev_printk(KERN_DEBUG, &port->dev,
			    MYIOC_s_FMT "add port %d, sas_addr (0x%llx)\n",
			    ioc->name, port->port_identifier,
			    (unsigned long long)phy_info->
			    attached.sas_address));
		}
		dsaswideprintk(ioc, printk(MYIOC_s_DEBUG_FMT
			"sas_port_add_phy: phy_id=%d\n",
			ioc->name, phy_info->phy_id));
		sas_port_add_phy(port, phy_info->phy);
		phy_info->sas_port_add_phy = 0;
		devtprintk(ioc, dev_printk(KERN_DEBUG, &phy_info->phy->dev,
		    MYIOC_s_FMT "add phy %d, phy-obj (0x%p)\n", ioc->name,
		     phy_info->phy_id, phy_info->phy));
	}
	if (!mptsas_get_rphy(phy_info) && port && !port->rphy) {

		struct sas_rphy *rphy;
		struct device *parent;
		struct sas_identify identify;

		parent = dev->parent->parent;
		/*
		 * Let the hotplug_work thread handle processing
		 * the adding/removing of devices that occur
		 * after start of day.
		 */
		if (mptsas_is_end_device(&phy_info->attached) &&
		    phy_info->attached.handle_parent) {
			goto out;
		}

		mptsas_parse_device_info(&identify, &phy_info->attached);
		if (scsi_is_host_device(parent)) {
			struct mptsas_portinfo *port_info;
			int i;

			port_info = ioc->hba_port_info;

			for (i = 0; i < port_info->num_phys; i++)
				if (port_info->phy_info[i].identify.sas_address ==
				    identify.sas_address) {
					sas_port_mark_backlink(port);
					goto out;
				}

		} else if (scsi_is_sas_rphy(parent)) {
			struct sas_rphy *parent_rphy = dev_to_rphy(parent);
			if (identify.sas_address ==
			    parent_rphy->identify.sas_address) {
				sas_port_mark_backlink(port);
				goto out;
			}
		}

		switch (identify.device_type) {
		case SAS_END_DEVICE:
			rphy = sas_end_device_alloc(port);
			break;
		case SAS_EDGE_EXPANDER_DEVICE:
		case SAS_FANOUT_EXPANDER_DEVICE:
			rphy = sas_expander_alloc(port, identify.device_type);
			break;
		default:
			rphy = NULL;
			break;
		}
		if (!rphy) {
			dfailprintk(ioc, printk(MYIOC_s_ERR_FMT
				"%s: exit at line=%d\n", ioc->name,
				__func__, __LINE__));
			goto out;
		}

		rphy->identify = identify;
		error = sas_rphy_add(rphy);
		if (error) {
			dfailprintk(ioc, printk(MYIOC_s_ERR_FMT
				"%s: exit at line=%d\n", ioc->name,
				__func__, __LINE__));
			sas_rphy_free(rphy);
			goto out;
		}
		mptsas_set_rphy(ioc, phy_info, rphy);
	}

 out:
	return error;
}

static int
mptsas_probe_hba_phys(MPT_ADAPTER *ioc)
{
	struct mptsas_portinfo *port_info, *hba;
	int error = -ENOMEM, i;

	hba = kzalloc(sizeof(struct mptsas_portinfo), GFP_KERNEL);
	if (! hba)
		goto out;

	error = mptsas_sas_io_unit_pg0(ioc, hba);
	if (error)
		goto out_free_port_info;

	mptsas_sas_io_unit_pg1(ioc);
	mutex_lock(&ioc->sas_topology_mutex);
	port_info = ioc->hba_port_info;
	if (!port_info) {
		ioc->hba_port_info = port_info = hba;
		ioc->hba_port_num_phy = port_info->num_phys;
		list_add_tail(&port_info->list, &ioc->sas_topology);
	} else {
		for (i = 0; i < hba->num_phys; i++) {
			port_info->phy_info[i].negotiated_link_rate =
				hba->phy_info[i].negotiated_link_rate;
			port_info->phy_info[i].handle =
				hba->phy_info[i].handle;
			port_info->phy_info[i].port_id =
				hba->phy_info[i].port_id;
		}
		kfree(hba->phy_info);
		kfree(hba);
		hba = NULL;
	}
	mutex_unlock(&ioc->sas_topology_mutex);
#if defined(CPQ_CIM)
	ioc->num_ports = port_info->num_phys;
#endif
	for (i = 0; i < port_info->num_phys; i++) {
		mptsas_sas_phy_pg0(ioc, &port_info->phy_info[i],
			(MPI_SAS_PHY_PGAD_FORM_PHY_NUMBER <<
			 MPI_SAS_PHY_PGAD_FORM_SHIFT), i);
		port_info->phy_info[i].identify.handle =
		    port_info->phy_info[i].handle;
		mptsas_sas_device_pg0(ioc, &port_info->phy_info[i].identify,
			(MPI_SAS_DEVICE_PGAD_FORM_HANDLE <<
			 MPI_SAS_DEVICE_PGAD_FORM_SHIFT),
			 port_info->phy_info[i].identify.handle);
		if (!ioc->hba_port_sas_addr)
			ioc->hba_port_sas_addr =
			    port_info->phy_info[i].identify.sas_address;
		port_info->phy_info[i].identify.phy_id =
		    port_info->phy_info[i].phy_id = i;
		if (port_info->phy_info[i].attached.handle)
			mptsas_sas_device_pg0(ioc,
				&port_info->phy_info[i].attached,
				(MPI_SAS_DEVICE_PGAD_FORM_HANDLE <<
				 MPI_SAS_DEVICE_PGAD_FORM_SHIFT),
				port_info->phy_info[i].attached.handle);
	}

	mptsas_setup_wide_ports(ioc, port_info);

	for (i = 0; i < port_info->num_phys; i++, ioc->sas_index++)
		mptsas_probe_one_phy(&ioc->sh->shost_gendev,
		    &port_info->phy_info[i], ioc->sas_index, 1);

	return 0;

 out_free_port_info:
	kfree(hba);
 out:
	return error;
}

static void
mptsas_expander_refresh(MPT_ADAPTER *ioc, struct mptsas_portinfo *port_info)
{
	struct mptsas_portinfo *parent;
	struct device *parent_dev;
	struct sas_rphy	*rphy;
	int		i;
	u64		sas_address; /* expander sas address */
	u32		handle;

	handle = port_info->phy_info[0].handle;
	sas_address = port_info->phy_info[0].identify.sas_address;
	for (i = 0; i < port_info->num_phys; i++) {
		mptsas_sas_expander_pg1(ioc, &port_info->phy_info[i],
		    (MPI_SAS_EXPAND_PGAD_FORM_HANDLE_PHY_NUM <<
		    MPI_SAS_EXPAND_PGAD_FORM_SHIFT), (i << 16) + handle);

		mptsas_sas_device_pg0(ioc,
		    &port_info->phy_info[i].identify,
		    (MPI_SAS_DEVICE_PGAD_FORM_HANDLE <<
		    MPI_SAS_DEVICE_PGAD_FORM_SHIFT),
		    port_info->phy_info[i].identify.handle);
		port_info->phy_info[i].identify.phy_id =
		    port_info->phy_info[i].phy_id;

		if (port_info->phy_info[i].attached.handle) {
			mptsas_sas_device_pg0(ioc,
			    &port_info->phy_info[i].attached,
			    (MPI_SAS_DEVICE_PGAD_FORM_HANDLE <<
			     MPI_SAS_DEVICE_PGAD_FORM_SHIFT),
			    port_info->phy_info[i].attached.handle);
			port_info->phy_info[i].attached.phy_id =
			    port_info->phy_info[i].phy_id;
		}
	}

	mutex_lock(&ioc->sas_topology_mutex);
	parent = mptsas_find_portinfo_by_handle(ioc,
	    port_info->phy_info[0].identify.handle_parent);
	if (!parent) {
		mutex_unlock(&ioc->sas_topology_mutex);
		return;
	}
	for (i = 0, parent_dev = NULL; i < parent->num_phys && !parent_dev;
	    i++) {
		if (parent->phy_info[i].attached.sas_address == sas_address) {
			rphy = mptsas_get_rphy(&parent->phy_info[i]);
			parent_dev = &rphy->dev;
		}
	}
	mutex_unlock(&ioc->sas_topology_mutex);

	mptsas_setup_wide_ports(ioc, port_info);
	for (i = 0; i < port_info->num_phys; i++, ioc->sas_index++)
		mptsas_probe_one_phy(parent_dev, &port_info->phy_info[i],
		    ioc->sas_index, 0);
}

static void
mptsas_expander_event_add(MPT_ADAPTER *ioc,
    MpiEventDataSasExpanderStatusChange_t *expander_data)
{
	struct mptsas_portinfo *port_info;
	int i;
	__le64 sas_address;

	port_info = kzalloc(sizeof(struct mptsas_portinfo), GFP_KERNEL);
	if (!port_info)
		BUG();
	port_info->num_phys = (expander_data->NumPhys) ?
	    expander_data->NumPhys : 1;
	port_info->phy_info = kcalloc(port_info->num_phys,
	    sizeof(struct mptsas_phyinfo), GFP_KERNEL);
	if (!port_info->phy_info)
		BUG();
	memcpy(&sas_address, &expander_data->SASAddress, sizeof(__le64));
	for (i = 0; i < port_info->num_phys; i++) {
		port_info->phy_info[i].portinfo = port_info;
		port_info->phy_info[i].handle =
		    le16_to_cpu(expander_data->DevHandle);
		port_info->phy_info[i].identify.sas_address =
		    le64_to_cpu(sas_address);
		port_info->phy_info[i].identify.handle_parent =
		    le16_to_cpu(expander_data->ParentDevHandle);
	}

	mutex_lock(&ioc->sas_topology_mutex);
	list_add_tail(&port_info->list, &ioc->sas_topology);
	mutex_unlock(&ioc->sas_topology_mutex);

	printk(MYIOC_s_INFO_FMT "add expander: num_phys %d, "
	    "sas_addr (0x%llx)\n", ioc->name, port_info->num_phys,
	    (unsigned long long)sas_address);

	mptsas_expander_refresh(ioc, port_info);
}

/**
 * mptsas_delete_expander_siblings - remove siblings attached to expander
 * @ioc: Pointer to MPT_ADAPTER structure
 * @parent: the parent port_info object
 * @expander: the expander port_info object
 **/
static void
mptsas_delete_expander_siblings(MPT_ADAPTER *ioc, struct mptsas_portinfo
    *parent, struct mptsas_portinfo *expander)
{
	struct mptsas_phyinfo *phy_info;
	struct mptsas_portinfo *port_info;
	struct sas_rphy *rphy;
	int i;

	phy_info = expander->phy_info;
	for (i = 0; i < expander->num_phys; i++, phy_info++) {
		rphy = mptsas_get_rphy(phy_info);
		if (!rphy)
			continue;
		if (rphy->identify.device_type == SAS_END_DEVICE)
			mptsas_del_end_device(ioc, phy_info);
	}

	phy_info = expander->phy_info;
	for (i = 0; i < expander->num_phys; i++, phy_info++) {
		rphy = mptsas_get_rphy(phy_info);
		if (!rphy)
			continue;
		if (rphy->identify.device_type ==
		    MPI_SAS_DEVICE_INFO_EDGE_EXPANDER ||
		    rphy->identify.device_type ==
		    MPI_SAS_DEVICE_INFO_FANOUT_EXPANDER) {
			port_info = mptsas_find_portinfo_by_sas_address(ioc,
			    rphy->identify.sas_address);
			if (!port_info)
				continue;
			if (port_info == parent) /* backlink rphy */
				continue;
			/*
			Delete this expander even if the expdevpage is exists
			because the parent expander is already deleted
			*/
			mptsas_expander_delete(ioc, port_info, 1);
		}
	}
}


/**
 *	mptsas_expander_delete - remove this expander
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@port_info: expander port_info struct
 *	@force: Flag to forcefully delete the expander
 *
 **/

static void mptsas_expander_delete(MPT_ADAPTER *ioc,
		struct mptsas_portinfo *port_info, u8 force)
{

	struct mptsas_portinfo *parent;
	int		i;
	u64		expander_sas_address;
	struct mptsas_phyinfo *phy_info;
	struct mptsas_portinfo buffer;
	struct mptsas_portinfo_details *port_details;
	struct sas_port *port;

	if (!port_info)
		return;

	/* see if expander is still there before deleting */
	mptsas_sas_expander_pg0(ioc, &buffer,
	    (MPI_SAS_EXPAND_PGAD_FORM_HANDLE <<
	    MPI_SAS_EXPAND_PGAD_FORM_SHIFT),
	    port_info->phy_info[0].identify.handle);

	if (buffer.num_phys) {
		kfree(buffer.phy_info);
		if (!force)
			return;
	}


	/*
	 * Obtain the port_info instance to the parent port
	 */
	port_details = NULL;
	expander_sas_address =
	    port_info->phy_info[0].identify.sas_address;
	parent = mptsas_find_portinfo_by_handle(ioc,
	    port_info->phy_info[0].identify.handle_parent);
	mptsas_delete_expander_siblings(ioc, parent, port_info);
	if (!parent)
		goto out;

	/*
	 * Delete rphys in the parent that point
	 * to this expander.
	 */
	phy_info = parent->phy_info;
	port = NULL;
	for (i = 0; i < parent->num_phys; i++, phy_info++) {
		if (!phy_info->phy)
			continue;
		if (phy_info->attached.sas_address !=
		    expander_sas_address)
			continue;
		if (!port) {
			port = mptsas_get_port(phy_info);
			port_details = phy_info->port_details;
		}
		dev_printk(KERN_DEBUG, &phy_info->phy->dev,
		    MYIOC_s_FMT "delete phy %d, phy-obj (0x%p)\n", ioc->name,
		    phy_info->phy_id, phy_info->phy);
		sas_port_delete_phy(port, phy_info->phy);
	}
	if (port) {
		dev_printk(KERN_DEBUG, &port->dev,
		    MYIOC_s_FMT "delete port %d, sas_addr (0x%llx)\n",
		    ioc->name, port->port_identifier,
		    (unsigned long long)expander_sas_address);
		sas_port_delete(port);
		mptsas_port_delete(ioc, port_details);
	}
 out:

	printk(MYIOC_s_INFO_FMT "delete expander: num_phys %d, "
	    "sas_addr (0x%llx)\n",  ioc->name, port_info->num_phys,
	    (unsigned long long)expander_sas_address);

	/*
	 * free link
	 */
	list_del(&port_info->list);
	kfree(port_info->phy_info);
	kfree(port_info);
}


/**
 * mptsas_send_expander_event - expanders events
 * @ioc: Pointer to MPT_ADAPTER structure
 * @expander_data: event data
 *
 *
 * This function handles adding, removing, and refreshing
 * device handles within the expander objects.
 */
static void
mptsas_send_expander_event(struct fw_event_work *fw_event)
{
	MPT_ADAPTER *ioc;
	MpiEventDataSasExpanderStatusChange_t *expander_data;
	struct mptsas_portinfo *port_info;
	__le64 sas_address;
	int i;

	ioc = fw_event->ioc;
	expander_data = (MpiEventDataSasExpanderStatusChange_t *)
	    fw_event->event_data;
	memcpy(&sas_address, &expander_data->SASAddress, sizeof(__le64));
	sas_address = le64_to_cpu(sas_address);
	port_info = mptsas_find_portinfo_by_sas_address(ioc, sas_address);

	if (expander_data->ReasonCode == MPI_EVENT_SAS_EXP_RC_ADDED) {
		if (port_info) {
			for (i = 0; i < port_info->num_phys; i++) {
				port_info->phy_info[i].portinfo = port_info;
				port_info->phy_info[i].handle =
				    le16_to_cpu(expander_data->DevHandle);
				port_info->phy_info[i].identify.sas_address =
				    le64_to_cpu(sas_address);
				port_info->phy_info[i].identify.handle_parent =
				    le16_to_cpu(expander_data->ParentDevHandle);
			}
			mptsas_expander_refresh(ioc, port_info);
		} else if (!port_info && expander_data->NumPhys)
			mptsas_expander_event_add(ioc, expander_data);
	} else if (expander_data->ReasonCode ==
	    MPI_EVENT_SAS_EXP_RC_NOT_RESPONDING)
		mptsas_expander_delete(ioc, port_info, 0);

	mptsas_free_fw_event(ioc, fw_event);
}


/**
 * mptsas_expander_add -
 * @ioc: Pointer to MPT_ADAPTER structure
 * @handle:
 *
 */
struct mptsas_portinfo *
mptsas_expander_add(MPT_ADAPTER *ioc, u16 handle)
{
	struct mptsas_portinfo buffer, *port_info;
	int i;

	if ((mptsas_sas_expander_pg0(ioc, &buffer,
	    (MPI_SAS_EXPAND_PGAD_FORM_HANDLE <<
	    MPI_SAS_EXPAND_PGAD_FORM_SHIFT), handle)))
		return NULL;

	port_info = kzalloc(sizeof(struct mptsas_portinfo), GFP_ATOMIC);
	if (!port_info) {
		dfailprintk(ioc, printk(MYIOC_s_ERR_FMT
		"%s: exit at line=%d\n", ioc->name,
		__func__, __LINE__));
		return NULL;
	}
	port_info->num_phys = buffer.num_phys;
	port_info->phy_info = buffer.phy_info;
	for (i = 0; i < port_info->num_phys; i++)
		port_info->phy_info[i].portinfo = port_info;
	mutex_lock(&ioc->sas_topology_mutex);
	list_add_tail(&port_info->list, &ioc->sas_topology);
	mutex_unlock(&ioc->sas_topology_mutex);
	printk(MYIOC_s_INFO_FMT "add expander: num_phys %d, "
	    "sas_addr (0x%llx)\n", ioc->name, port_info->num_phys,
	    (unsigned long long)buffer.phy_info[0].identify.sas_address);
	mptsas_expander_refresh(ioc, port_info);
	return port_info;
}

static void
mptsas_send_link_status_event(struct fw_event_work *fw_event)
{
	MPT_ADAPTER *ioc;
	MpiEventDataSasPhyLinkStatus_t *link_data;
	struct mptsas_portinfo *port_info;
	struct mptsas_phyinfo *phy_info = NULL;
	__le64 sas_address;
	u8 phy_num;
	u8 link_rate;

	ioc = fw_event->ioc;
	link_data = (MpiEventDataSasPhyLinkStatus_t *)fw_event->event_data;

	memcpy(&sas_address, &link_data->SASAddress, sizeof(__le64));
	sas_address = le64_to_cpu(sas_address);
	link_rate = link_data->LinkRates >> 4;
	phy_num = link_data->PhyNum;

	port_info = mptsas_find_portinfo_by_sas_address(ioc, sas_address);
	if (port_info) {
		phy_info = &port_info->phy_info[phy_num];
		if (phy_info)
			phy_info->negotiated_link_rate = link_rate;
	}

	if (link_rate == MPI_SAS_IOUNIT0_RATE_1_5 ||
	    link_rate == MPI_SAS_IOUNIT0_RATE_3_0) {

		if (!port_info) {
			if (ioc->old_sas_discovery_protocal) {
				port_info = mptsas_expander_add(ioc,
					le16_to_cpu(link_data->DevHandle));
				if (port_info)
					goto out;
			}
			goto out;
		}

		if (port_info == ioc->hba_port_info)
			mptsas_probe_hba_phys(ioc);
		else
			mptsas_expander_refresh(ioc, port_info);
	} else if (phy_info && phy_info->phy) {
		if (link_rate ==  MPI_SAS_IOUNIT0_RATE_PHY_DISABLED)
			phy_info->phy->negotiated_linkrate =
			    SAS_PHY_DISABLED;
		else if (link_rate ==
		    MPI_SAS_IOUNIT0_RATE_FAILED_SPEED_NEGOTIATION)
			phy_info->phy->negotiated_linkrate =
			    SAS_LINK_RATE_FAILED;
		else
			phy_info->phy->negotiated_linkrate =
			    SAS_LINK_RATE_UNKNOWN;
	}
 out:
	mptsas_free_fw_event(ioc, fw_event);
}

static void
mptsas_not_responding_devices(MPT_ADAPTER *ioc)
{
	struct mptsas_portinfo buffer, *port_info;
	struct mptsas_device_info	*sas_info;
	struct mptsas_devinfo sas_device;
	u32	handle;
	VirtTarget *vtarget = NULL;
	struct mptsas_phyinfo *phy_info;
	u8 found_expander;
	int retval, retry_count;
	unsigned long flags;

	mpt_findImVolumes(ioc);

	spin_lock_irqsave(&ioc->taskmgmt_lock, flags);
	if (ioc->ioc_reset_in_progress) {
		dfailprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		   "%s: exiting due to a parallel reset \n", ioc->name,
		    __func__));
		spin_unlock_irqrestore(&ioc->taskmgmt_lock, flags);
		return;
	}
	spin_unlock_irqrestore(&ioc->taskmgmt_lock, flags);

	/* devices, logical volumes */
	mutex_lock(&ioc->sas_device_info_mutex);
 redo_device_scan:
	list_for_each_entry(sas_info, &ioc->sas_device_info_list, list) {
		if (sas_info->is_cached)
			continue;
		if (!sas_info->is_logical_volume) {
			sas_device.handle = 0;
			retry_count = 0;
retry_page:
			retval = mptsas_sas_device_pg0(ioc, &sas_device,
				(MPI_SAS_DEVICE_PGAD_FORM_BUS_TARGET_ID
				<< MPI_SAS_DEVICE_PGAD_FORM_SHIFT),
				(sas_info->fw.channel << 8) +
				sas_info->fw.id);

			if (sas_device.handle)
				continue;
			if (retval == -EBUSY) {
				spin_lock_irqsave(&ioc->taskmgmt_lock, flags);
				if (ioc->ioc_reset_in_progress) {
					dfailprintk(ioc,
					printk(MYIOC_s_DEBUG_FMT
					"%s: exiting due to reset\n",
					ioc->name, __func__));
					spin_unlock_irqrestore
					(&ioc->taskmgmt_lock, flags);
					mutex_unlock(&ioc->
					sas_device_info_mutex);
					return;
				}
				spin_unlock_irqrestore(&ioc->taskmgmt_lock,
				flags);
			}

			if (retval && (retval != -ENODEV)) {
				if (retry_count < 10) {
					retry_count++;
					goto retry_page;
				} else {
					devtprintk(ioc, printk(MYIOC_s_DEBUG_FMT
					"%s: Config page retry exceeded retry "
					"count deleting device 0x%llx\n",
					ioc->name, __func__,
					sas_info->sas_address));
				}
			}

			/* delete device */
			vtarget = mptsas_find_vtarget(ioc,
				sas_info->fw.channel, sas_info->fw.id);

			if (vtarget)
				vtarget->deleted = 1;

			phy_info = mptsas_find_phyinfo_by_sas_address(ioc,
					sas_info->sas_address);

			if (phy_info) {
				mptsas_del_end_device(ioc, phy_info);
				goto redo_device_scan;
			}
		} else
			mptsas_volume_delete(ioc, sas_info->fw.id);
	}
	mutex_unlock(&ioc->sas_device_info_mutex);

	/* expanders */
	mutex_lock(&ioc->sas_topology_mutex);
 redo_expander_scan:
	list_for_each_entry(port_info, &ioc->sas_topology, list) {

		if (port_info->phy_info &&
		    (!(port_info->phy_info[0].identify.device_info &
		    MPI_SAS_DEVICE_INFO_SMP_TARGET)))
			continue;
		found_expander = 0;
		handle = 0xFFFF;
		while (!mptsas_sas_expander_pg0(ioc, &buffer,
		    (MPI_SAS_EXPAND_PGAD_FORM_GET_NEXT_HANDLE <<
		     MPI_SAS_EXPAND_PGAD_FORM_SHIFT), handle) &&
		    !found_expander) {

			handle = buffer.phy_info[0].handle;
			if (buffer.phy_info[0].identify.sas_address ==
			    port_info->phy_info[0].identify.sas_address) {
				found_expander = 1;
			}
			kfree(buffer.phy_info);
		}

		if (!found_expander) {
			mptsas_expander_delete(ioc, port_info, 0);
			goto redo_expander_scan;
		}
	}
	mutex_unlock(&ioc->sas_topology_mutex);
}

/**
 *	mptsas_probe_expanders - adding expanders
 *	@ioc: Pointer to MPT_ADAPTER structure
 *
 **/
static void
mptsas_probe_expanders(MPT_ADAPTER *ioc)
{
	struct mptsas_portinfo buffer, *port_info;
	u32 			handle;
	int i;

	handle = 0xFFFF;
	while (!mptsas_sas_expander_pg0(ioc, &buffer,
	    (MPI_SAS_EXPAND_PGAD_FORM_GET_NEXT_HANDLE <<
	     MPI_SAS_EXPAND_PGAD_FORM_SHIFT), handle)) {

		handle = buffer.phy_info[0].handle;
		port_info = mptsas_find_portinfo_by_sas_address(ioc,
		    buffer.phy_info[0].identify.sas_address);

		if (port_info) {
			/* refreshing handles */
			for (i = 0; i < buffer.num_phys; i++) {
				port_info->phy_info[i].handle = handle;
				port_info->phy_info[i].identify.handle_parent =
				    buffer.phy_info[0].identify.handle_parent;
			}
			mptsas_expander_refresh(ioc, port_info);
			kfree(buffer.phy_info);
			continue;
		}

		port_info = kzalloc(sizeof(struct mptsas_portinfo), GFP_KERNEL);
		if (!port_info) {
			dfailprintk(ioc, printk(MYIOC_s_ERR_FMT
			"%s: exit at line=%d\n", ioc->name,
			__func__, __LINE__));
			return;
		}
		port_info->num_phys = buffer.num_phys;
		port_info->phy_info = buffer.phy_info;
		for (i = 0; i < port_info->num_phys; i++)
			port_info->phy_info[i].portinfo = port_info;
		mutex_lock(&ioc->sas_topology_mutex);
		list_add_tail(&port_info->list, &ioc->sas_topology);
		mutex_unlock(&ioc->sas_topology_mutex);
		printk(MYIOC_s_INFO_FMT "add expander: num_phys %d, "
		    "sas_addr (0x%llx)\n", ioc->name, port_info->num_phys,
	    (unsigned long long)buffer.phy_info[0].identify.sas_address);
		mptsas_expander_refresh(ioc, port_info);
	}
}

static void
mptsas_probe_devices(MPT_ADAPTER *ioc)
{
	u16 handle;
	struct mptsas_devinfo sas_device;
	struct mptsas_phyinfo *phy_info;

	handle = 0xFFFF;
	while (!(mptsas_sas_device_pg0(ioc, &sas_device,
	    MPI_SAS_DEVICE_PGAD_FORM_GET_NEXT_HANDLE, handle))) {

		handle = sas_device.handle;

		if ((sas_device.device_info &
		     (MPI_SAS_DEVICE_INFO_SSP_TARGET |
		      MPI_SAS_DEVICE_INFO_STP_TARGET |
		      MPI_SAS_DEVICE_INFO_SATA_DEVICE)) == 0)
			continue;

		phy_info = mptsas_refreshing_device_handles(ioc, &sas_device);
		if (!phy_info)
			continue;

		if (mptsas_get_rphy(phy_info))
			continue;

		mptsas_add_end_device(ioc, phy_info);
	}
}

/**
 *	mptsas_scan_sas_topology -
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@sas_address:
 *
 **/
static void
mptsas_scan_sas_topology(MPT_ADAPTER *ioc)
{
	struct scsi_device *sdev;
	int i;

	mptsas_probe_hba_phys(ioc);
	mptsas_probe_expanders(ioc);
	mptsas_probe_devices(ioc);

	/*
	  Reporting RAID volumes.
	*/
	if (!ioc->ir_firmware || !ioc->raid_data.pIocPg2 ||
	    !ioc->raid_data.pIocPg2->NumActiveVolumes)
		return;
	for (i = 0; i < ioc->raid_data.pIocPg2->NumActiveVolumes; i++) {
		sdev = scsi_device_lookup(ioc->sh, MPTSAS_RAID_CHANNEL,
		    ioc->raid_data.pIocPg2->RaidVolume[i].VolumeID, 0);
		if (sdev) {
			scsi_device_put(sdev);
			continue;
		}
		printk(MYIOC_s_INFO_FMT "attaching raid volume, channel %d, "
		    "id %d\n", ioc->name, MPTSAS_RAID_CHANNEL,
		    ioc->raid_data.pIocPg2->RaidVolume[i].VolumeID);
		scsi_add_device(ioc->sh, MPTSAS_RAID_CHANNEL,
		    ioc->raid_data.pIocPg2->RaidVolume[i].VolumeID, 0);
	}
}


static void
mptsas_handle_queue_full_event(struct fw_event_work *fw_event)
{
	MPT_ADAPTER *ioc;
	EventDataQueueFull_t *qfull_data;
	struct mptsas_device_info *sas_info;
	struct scsi_device	*sdev;
	int depth;
	int id = -1;
	int channel = -1;
	int fw_id, fw_channel;
	u16 current_depth;


	ioc = fw_event->ioc;
	qfull_data = (EventDataQueueFull_t *)fw_event->event_data;
	fw_id = qfull_data->TargetID;/*
 *channellinux/drivers/mBus;
	current_depth = le16_to_cpu(ux/drivers/mC LSI PD chi);

	/* if hidden raid component, look for the volume id */
	mutex_lock(&ioc->sas_device_info_(c) 1);
	if (mptscsih_is_phys_disk(ioc, on/mptsas./
/*=id)) {
		list_for_each_entry( Corn
 *, 08 LSI Corporation
 * =-=-,
		    =-=-=-=-=-:DL-M=-=-=-=-->is_cached ||gram   Tfree softwarelogical_ *  Co)gram	continue;gram is free softwarege Pas_ing _echnology && redistr free softwfw.mptsas.c
=
/*=-=-=-=-ublished by    the Free   liFound-=-=-=-=-		  linfree softw *  Co_iderms 	mptsas.c
 MPTSAS_RAID_CHANNELpe thagoto outerms }
NTY; } else-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
    This program is free software; you can redistribute it and/l Public License as puan redistribute it and/or modify
    it under the terms of the GNU Genee Software Foundation; version 2 of the License.

    This rogram is distributed os.hope that it will NG, WITHOUT
 mptsas.WITHOUT ANY WARRANTY; wit
FRINY WA:t (c) 19un99-2008 LSI Corporation
 *  (mailtoo:DL-Mid != -1PLIED sho=-=-=-=-=-=-porati(sdev, 8 LSI hprogram is fdev->.

   id && g the Software Fomptsas.PLIED INCf (h LSI PCI chip>ness ofqueueCI chiPLIED IN	g th_printk(KERN_INFO,ness /*
 redistr"strange observation,.
 *
all r "ghts under I chipis (%d) meanwhile fwg but not limited to
     ris\n"e of rs all risks aights underh LSI PCI chilto: under the terms TY; w		I chip/ausio_track_all rix/dresponserrup or equipment, an - 1d unavag thm and as0it undd with its
    exercise of rights u"Qbut nI chipreduced toe to or lghts undet, and unavat eveNT NOR ANY<CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANTagged Command Y
   ing thebOWEVEot limi"disabled\n"EXEMPLARY, OR CONSEQUE==CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY
    DIRECTnot
    LOSTyetHEORY OF Y; witho

	PTFuas_free_fw_event)
 */
/*=THE
 );
}


static struct ANY WAYphyn
 * *
ANY WAY indTHE EXER_by_ WAYaddress(MPT_ADAPTER *
 */
u64Y WAR HEREUN)
{
	E PROGRAM OR THortEXERCIES

-=-=-;TY OF SUCH DAMAGEE EXERCIphy-=-=- = NUL WITint iLAR (c) 1999-2008 LSI Cortopology  (mailto:=-=-=-=-=-=-=-=-=-=-u should =-=-=-=-=-=- program,This prograare.(i = 0; i < u should ->numux@ls; i++program is !ANY WAYis_endolely reerrup&Boston, MA e GNU Ge[i].att you )it under the terms of t=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=. THE POSSIB redistr!NY WAR POSSIBIL under the terms e GNU Gener-=-=-=-=-=-=-=-=-=-=-=- unavbreak unaING I  MERCHANTABILITY OR FIT program; if not, return e GNU GeIBUTI/**
 * ANY WAY  RIGHTS GRANTEDx@lsi.com_num - <sc@ioc: PointerECT,ER, EVEN IF E PROGure
#incsi/scsi_cmnd.:
#incmptsas.includidincl
 **/ON OF THE PROGRAM OR THE EXERCISE OF ANY RIGHTS GRANTEDsi/scsi_cmnd.DER, EVEN IF ADVISED8elayscsi_cmnd.,
	u8
    dis "mptidILITY OF SUCH DAMAGEpy of the GNU General Publ OF SUCH DAMAGES

    You should havRaidPhysDiskPage1_tthe Gsi.comneral Public L 0211ath witD OF THE POSSIBuite blic License GNU General Publi*=-=8 LSIc Licvers.pIocPg3t.h>for mdeal Publ/* dual Bost sup_DESCght ed raid
  =RAM ic Licsas.h"


#getmnd.raid
 )
 */
sas.h"


#deflto:DL-M!_VERSION(t.h>T ANY WARRAed channel fkzalloc(offsetof(mptsas"

/*
 * Rese, Pa ass+
LIAB(AME);
MODU* sizeULE_l,
 PHYS_DISK1_PATH)), GFP_  exELc int mpted channe;
module_param(_LICENSE("GPL");
MOpg1my_VERSION);

statiERSION);

slto:ace, Suite 330, ed raid
 *07  USA
*/_PAG(ed channe->ar,
-=-=Flags & 1)ludeONTRIB/* =-=-= no longer valyright nder the termsnt ma.

   x_lun = MPTSAS_MAX_as"

/*
ID)ublishd by
Software Foefault=16895 ");

static u8Bus programmemcpy(& <linux/ini, &x_lun = MPTSAS_MAX_WWIDNTAL,  table:u64)d unave GNU GenerE OF ANY RIGHTS GRANTED
    HEREUND
 */TRIBUTO<linux/init unavT ANY WARRANING INNT,
   k OUTax_lun = Mlto:DL-Me GNU GeTHOR(MODULElay */

#iR);

	 * Extra codeECT,handle  ena0 case, where.
 *
ine MPTSAS_RisNEGLIupdatedTOCOLitx = eter is age_1rmwan hotswappt *worht (c) 1999-2008 LSI Cor program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1aten=0)" */

#07  USA
*/
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#include <linux/module.h>
#include <lisi/scsi_cmnd.h== ~ONTRIBUer the terms of t_devinfo *device_info);
static inline void mptsas redistrSION);

staticublished by
de <linux/module.h>
#include <liappropriruct mptsas_phyinfo	*mptsas_find_phyinfo_by_sSoftware F redistr
    distt.h>
#x/errno.h>
#include <linux/jiffies.h>
linux/workqueue.h>
#include <linux/delay.h>	/* for mdelay */

#inclN OF THvoidSE OF ANreprobe_lun(E PROGRoperaporati *sponsie_pg *versILITYic LrcAX_Pg the no_uld_-=-=-= = vers ? 1 :ID_CHrcof operaporatioAPTER *ng thRIBUTInclosure_pg0(MPT_ADAPTER *itssage
	struct mpts);
sta *s);
sta, ic Lific);
staILITY Ossage is
    solely resd_deviceific);
stat? (enclos)tsasal Pmands(MPT_ADAPTER *ioc,
	struct mptsas_phyinfo *adding_inactiveic License as pNDER, EVEN IF ADVISEDmy_NAME		"Fusion MPT CONFIGPARMSphy(fg;
	Config * RHeader_t		hdr;
	dmainux/atic mptstic voam(mmptsV*  Co * R0aticbufferneral Publmptsas"

/*
 * R0_t PT_ADsi.com_CHANNram ihave received a copy of 	he GNU GeT_LINUX_VERSION_Cporant(s*/
stporaticenseemset(&cfg, 0 ,y table:T_ADAPTER * MPT_s_not_rehdrnding_devices(as_address);
stat MPT_hdr. * RTypell beI_T_ADAP_PAGETYPEful,
 VOLUMEwithfg.d mpAddptsax = MPT_M<< 8) + hope ve_wcfgAPTEhruct tic vent);
ructon
static void mACTIONmptsa_HEADERLAR PURP_LICcas_ad)
 */
espole_param(mT ANY WARRIG_PAGEAPTER *iLengtholume_delete(MPTuct mptsapci_int, );
ssistE
    U->pciponsiTER *ioc, u8 i * 4, Clea &er_delete(ULAR PURP!uct mpolume_delete(MPTve_wohysstruct er_delete(MPTandle_queue_full_event(struct fw_eveREAD_CURRENTk *fw_event);
static void mptsas_volume_delete(MPT_ADAP(uct mp->TER *iStatus_LUN;
mo_PHY_Datic enaVOL0_STATUS_FLAGst_prim_INtrucVEpg0(Me_delete(MPT_ADAP    ioc-Numas"

/*
r;
module_paramdefine MPTSAS_MAX_BUG_FMT "Controller rk *fw_ev, " max global parmeter is m0 commandHY_D    ioc-as"

/*
;

static u8Nich L_DRIVERS;le_param(maer the terlerDevHand/
st Corporatiotk(ioc,  PT_MAporati/*
    T(aticusefDEVICE_PGAD_FORM_BUS_TARGET_ID <<printk(ta->Ax%X\n",
	    ioc->namSHIFT)mandsax_lun = Mstatic u8	mp_work *fT_MAX_PFMT "PHY Flags=IDpg0(MPata->Port));
X_PROTOCOL_DRIVERS; /* Used only for internal command 2 of thporatiinux/kernel.	mptsvent);
sta=-=-=-=-=-=-y_VERSIOasDevi;NGEMENT,
   _PAGsprintk(iorint OUT ata(MPT_ADAPTER *ioc,
					MPI_SAS_IO_UNIT0(MYIOC_/*
    TTA *phy_data)}
/port Workg but nthreaRECT,tic voiSASnd_splug THE
 s
strunclosure_pg0(MPT_ADasprint_workDER, EVEN IF ADVISEE PROGR OR DISTveryS * OR DIST,
 2 of PROGRAM OR T "DiscovTHE
  *hot_iscovsDevicPT SAS Host driver"
#define my_VERT_LINUX_VE mptsas_del_e phy_infevent_work *fw_event);
s=0x%X\n",
	;
	Virtessage *v sas_addric Licensswitch ( void mptsas_->me, letypetrolleas_f be usefADable=1 "
	:));
	dsaE_TIMEOUT		30
MODULE_A2t.h>
include Place, Suite 330, -----\n", ioc->name));T "CoAuct mTER *i1307  USA
*/
/*=-s_DEBUG_FMT
	    "AttachADAPTER *i-=-=TER *iID mptsas_sask(ioc, printk(MIMPLIED IN its
  MYIOC_s_WARN_FMT "firmware bug: unANY  AND
  nder to addage Passannel- struct id matchsme, (unsignein the hoor loPTER namata)IBUTANY WAY OUT OF THE
    USE OR DISTRIBIBUTfor mdISE) ARISINGr glo.h>
Im HandlenameULAR 
	    "---- SAS n",
	 : pg0_not_reDEBUG_FMT " 0g_devicesnt_work *fw_event);
s MPT_Msprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Port Flags=0x%X\n",
	    ioc->name, phy_data->PortFlags);
	dsasprintk(ioc, printk(MYIOC_s_Dt Flak(ioc, printk(Mw_event_work *foc->namntk(ioc, printk(MYt));
	dsa!0x%X\n",
	 phy_datX\n" ioc->nasprintk(ioc, printk(Mrefreshtic poratiotic vo(my_VER_DEBUG_FMT x%X\nLEAR=0)"asDeviceRasprintk(io	dsasprintkuct rphymptsasDeviY Info=0x%X\n\ny_data->NegotiatedLinkRate));
	dsasprinasprintk(i
	    "---- DELoc, printk(intk(ioc, printk(MYIOC_s_DEBUG_FMT "Negotiated Link Rate=k(ioc, printk(M   ioc->name, phy_data-deltic void mptsas_print_phy_pg1(MPT_ADAPTER *ioc, SasPhyPPHY PAGE 0 --0->AttachedPhyIdentifier))g1)
{
	dsasprintk(ioc, printk(MYIOC#include "mptsgram iOC_sk(ioc, printk(Msas.h"


#define			,  pg0->ProgrammedLinkrity Error Count=0x%xk(MYIn", ioc->name));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "InvSAS PHY PAGE_REPROBE 0 ------sprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Port Flags=0x%X\n",
	    ioc->name, phy_data->PortFlags));
	dsasprintk(ioc, printk(MYIOC_s_D>name,  pg0->ProgrammedLinkRate));
rintk(ioc, printk(MPLIED Idfail its
  y_VERSBUG_FMT "SAS AERRss=0ity "%s:  This=%d exit at line=%printk(MYIOC_s_R OF LI__func___DEBUG_FMT
	    "id, __LINE__ MPT_MAinclude <li*pg1)
{
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMTx%x\n\n",
	  x%X\n",
	    ioc->name, _s_DEBUG_FMT "PHYstatic void mptsas_print_device_pg0(MPT_ADAPTER **ioc, SasDevicePage0_t *pg0)
{
	__le64 sas_address;

	memcpy(&sas_address, &pg0->SASAddress, sizeof(__le64));phy_infOL_DRIVERSuct phy_infmptsasDeviC_s_DEBUGphy_inf
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Handle=0x%X\n",
	    ioc->name, le16_to_cpu(pg0->DevHandle)));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Parent Haizeof(_of ostruct->ientversC_s_DEBUGizeof(_
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Handle=0x%X\n",
	    ioc->name, le16_to_cpu(pg0->DevHandle)));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Parent Ha0x%x\n",
	    ioc->name,  p	struct  its
    exercise od_deviceT "SAS As=0x% ena Hitatic: AND
nder on/mptsas.=%d=-=-=-=k(iococ->dsk (ioc/
static 0x%llxIDENTAL"---- S sas_adDEBUG_FMT
	    "_NAME		"F&sas_address, &pg0  "---- SAS PHY PAGE 1rt_luns, which (unsigned 0);
 0);
unt=ate=0x%X\n",
	    ioc->name, tk(MYIOC_se apprDEBUG_FMT
	    "Running DispangDiIOC_s_DEBtfUN;
m|l be phy_datadle)eful,
  OMPONdsasp pg1->Inva->static inline void mptsaMT "Access Status=0x%X\n",
	    iyinfo *phy_info);
static_device RECIPYIOC_s_DEBUG_FMT "Invalid Dword C Dword Synch Count=0x%x\n", ioc->name,
	    pg1->LossDwordSynchCount));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT
	    "PHY Reset Problem Count=0x%x	", ioc->name,
	    pg1->PhyResetProblemCount));
}

staticprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Handle=nder oc, SasDevicePage0_t *pg0)
{
	___t *pg1)
c->name, pg

	memcpy_t *pg1)
&sas_address, &pg0->SASAddress, sizeof(__le64));

	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT
	 commands  DEVICE PAGE 0 ---------\", ioc->name));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Handleg1)
{
	dsasprintk(ioc, printk(MYIOC_sle64 sas_addresle)));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Parent Handle=0x%X\n",
	    ioc->name, le16_to_cpu(pg0->ParentDevHandle)));
	dsasprintk(ioc, printk(MYIOC_s_DEBUGr=0x%X\n",
	    ioc->name, pg1->PhyIdentifier));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Negotiated Link RatMYIOC_s_DEBUG_FMT "Slot=0x%X\n",
	    ioc->name, le16_to_cpu(pg0->Slot)));
	dsasprintk(ioc, pr=0x%X\n",
	    ioc->name, pg1->PhyIdentifier));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Negotiated Link Rat\n",
	oc->name, le16_t&cpu(pg0->AccessStatus)));
	dsasp

static void mptsas_print_device_pg0(MPT_ADAPTER *r=0x%X\n",
	    ioc->name, pg1->PhyIdentifier));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Negotiated Link Rat);
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Target ID=0x%X\n",
	    ioc->name,Expos->TagetID));

/*=-=-=-=-k(ioc, printk(MYIOC_s_DEBUG_FMT "Bus=0x%X\n",
	    ioc->name, pg0->Bus));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Parent Phy Num=0x%X\n",
	    ioc->name, pg0->PhyNum));
	dsasprintk(ioc, printk(MYIOC_s_DEBBUG_FMT
= ~pu(pg0->AccessStatus)));
	dsasprintIOC_s_DEBUG_FMT "Access Statuhope tk(ioc, printk(MYIOC_s_DEBUG_FMT "De~D_CHme, le32_to_cpu(pg0->DeviceInfo)0ingDisparity>NegporatioechnologyNTEDfwT
	    "---- SAS PHY PAGE 1printk(ioc, printk(MYIOC_sc, printk(MYIOC_s_DEBUG_FMT
	  enaount=0x%x\n",
	    ioc->name,_DEBUG_FMT "SAS Arcis ioc->-=-=-=WEVEing T *  Co,
    disDEBUGgetID));
id 
{
	__le64 sas_ad be useful,
    but _DEBUG_FMT "Parent Phy long deopera_add_fw_evname,
shRK(&fw_event->work, mptsas_firmware_event_worktic voidMPT_ADAPTER *ioc, SasPhyPgs;

	spin_lock_irqsave(&ioc->fw_event_lock, flags);
	list_aremovl(&fw_event->list, &ioc->fw_event_list);
	INIT_DELAYED_WORK(&fw_event->work, mptsas_firmware_event_work);
	devtpr    driskoc, p SAS PHY PAGE 1 oc,
	s	devtpr, printkutw_event(MPT_ADAPTER *ioc,intk(MYIOC_s_DEBUG_FMT
	 sprintk(st_prim
	spin_lock_irqsave(&ioc->fw_evvent);
static struct mptsas_portinfo	*mioc, struct fw_event_work *fw_event,
    unsigned long delay)
{
	udefaul
   izeof(__l IN ANY WAY OUT OF THE
    USE OR DISTRIBUTInclosure_pg0(MPT_ADs-=-= DEVTHE
     ioc->name, le32_to_cpu(phy_printER, EVEN IF ADVIT_LINUX_VERSION_Cs)));
}

stati void mptsas_;
	EVENT_DATAdsasprintk(iviceHan   bGE *_event_q,	30
M*/
#32 poration
 **/
#define MPTSAS_(MPT_oend_ OR DIST->	spin_uas firmware ect f);
}

/* free memory assoicated to a )_PHY_DDAPTER *iofirmware evenporation
 *p/ada32er(s)
 *uct fw_event_w->Deue_fI_to_cpprintk(nt_lock, flalishags=0x%X\n",
	  );
	lSSPphy_dat n re=0x%p)\n",
	    ioc->Tame, __func__, fw_event));
	list_d* frn",
	 ))HER IN-=-=-MT
	    "Attached PHY Identifier=0x%X\n ioc->nam64)) is freeoc, printk(MReasonCS;

mptsaatic);
}

x%X\n",assoi_RC_NO_PERSIS, EVDEDirqrestorbasew_eveper(MPT_oper, incintk(MYI;
	dsaspOP_CLEAR_NOT_PRES0x%Xqueue a sasoc->fw_event_lock, flags);
}

/* walk the firsasprinte event queue, and either irqre
	    "wait for
 * outstanding evT_RESPONDING:T_SCSI_HOST	*hd = shost_priv(ioc-omplentk(MYIOC_s_ void mptsas_"Attached Device Info=0x%Xs)));
}

stat MPT_M void mptsas_ange Cop/adapter(s)
 *tk(ioc, printk(MYIOHhy_data)eset_list, n,
		ATION, ANY WARoc, printk(Mse witset_list, n,
		is distrioc, printk(Message/fusiset_list, n,
		_FMT for id=%d\n",
			   PhyNc->nametx = MPT_MAX_PROTOCOLtk(ioc, printk(MSASstruOTOChyNum));rnalCtx = MPT_Met_list, n,
		ine MPTSAS_RAIle64et_reset_lisoc->name, phet_list, n,
		nt_lock, flags
static void
mrmware event queue, and either sasDoneCtarget_reset_list */
	if (!list_unt=0et_list, n,
		YIOC_s_DEBll be usefk(ioc, pri())
t evt, list) {
		if (cancel_delayed_work(&PhyPage1_tqueue a sas "DiscoverySt  USE OR DIST, target_reset_l  unsigned long delaywait for
 * outstanding events to complentk(Mtatic void
mptsas_cleanup_fw_event_qgs));
	dsaspPTER *ioc)
{
	struct fw_event_work *fw_event, *next;
	struct mpt
	struct Scsi_Host *shost = dev_to_shostSMAR

/* f:R);
MTODOstrucScsi_Host *shost = dev_to_shostINTERNAyPage1_t);

ETv.parent->paren\n", ioc->nER *rphy_to_ioc(struct sas_rphy *rphy)
{
	stru	}
	queue_delayed_work(ioc->fwc Licnt_q, &fw_event->work,
	    msecs_to_jiffies(delay));
	spin_);
}

/* frname,*tex alreadare evenunlock_irqrestore(&ioc->fw_event_lock, flagsic LN OFe witt_info, 0->S	struct mptsas_enclosureneral PublASAdYIOC_s *vas_enclsas_portinfo *port_info, u8 fce);
staticnt(MPT_ADAPTER *ioc, stre(MPT_ADAPTER *ork *fw_event)
_handl)ck_irqsave(&ioc->fw_evefo, *rags);
	devtprine(MPT_ADAPTER *->Settings le16_tk(i
	intct fnfo;
		>>rk *& 0xffptsas_not_rearget_reset_list)) {
		list_for_each_entry_safe(target_reg target reset for 	}
 out:
	return Handle))This function shomptsas.c
 e called with the sas_tse wit__func__,
			   tDEBUG_FMT "De	}
 out:
	return ort=0x%X\n"G_FMT "%	}
 out:
	return nd either sto_Host *shos enabRC));
	dsaDELETEDO WARgs)) sas_address)
{
	struct mptsas_portinfo *port_info, *rc CREALL;
	int i;

	if (sas_address >= ioc->hba_hy_data->Ainfo *port_info, *rc ssoicated to ete */
ry(port struct fw_efirmupprintk(MYIOC_s_DEBUG_FMT "%s: add (fw_event=0x%p)\.,
	    ioct_list);
		}
	}tex_locdev())
		retoc,
ame,pology, lig the Slot=0x%X\n64))devt mptsas_print_device_pg0(MPDEBUG ioc->Esi/sWEVEoc, "nt i;
"nd either =%02\n",
MT
	    "---- SAS EXPAN i;

	if (sas_address >= ioc->h MPTreset_evennlock(&ioc->sas_topology_mutMPT_SCSI_HOST	*hd =port_infOC_s_DEBU= NULL;c->ntsas_free_fw_event(ioc, fw_event);
	}
OC_s_DEBUG_FMT "should be care is a scsi end device
 */
stataddressne int
mptsas_is_end_device(struct mptT
	    "Loss Dword Sed)
{
	if ((attached->sas_address) &&
	    (->hba_port_infc->nset_event	intirqresScsi_HostPDDEVICE_ONASAdc->ned->device_info &
>sh));
	ATIBLMPI_Sr global parmeter is m(ioc, pritex_unlock(&ioc->sas_tort=0x%X\n",
	    ioc->n%X\n"g target reset for oc->name, phy_data->lse
		return 0;
}

mptsas.c
 oc->name, phy_dataremovinlist) {
		if (cancel_delayed_work(&fw_ePHY PAGEs, sizeof(__leed->device_info &
FAILSP_TARed->device_info &
MISSflush hed->device_info &
	FFASAddAT_HOSh);
QUES;
}
ort_info;
	struct mptsas_	port_info = port_details->port_info;
turn;

	FOR_ANOTHER>namSON  (atnt
mptsas_is_end_device(struct mptsas_devinfo s, sizeof(__le\n", ioc->n#include <lin
{
	if ((attached->sas_address) o, *rc = NULL;t_depg0->Pfo->num_include <pology,->IOC_s_DEBdeleteG_FM1; /* b99-2 I>parenG_FMT "%s: [%p]: num_phys=%02d "
	    " enaed)
{
	if ((attached->sas_address)    sas_addressned longlong)rogram struct fw_eventTER *ioc,__func__, portist) {
		if (cancel_delayed_work(&fw_ehy_info->port_details != port_details)
			contEVICE_INFO_SSP_TAR\n",
	nfo;
		
	  >AttachedDeviceHandle))ENABLED

staticlong long)
	     port_deetails->phy_bitmask));

	for (i = 0; i < port_inffo->num_phys; i++, phy_info++) {
		if(phy_info-__func__, portGET) |
	    (attached->devicttachedDeviceHanruct mptsas_phyinfo *phy_T_ADAPTER *ioc, struct	i;

	if (!truct mptsas_phyinfo *phy_info)
{
	if (phy_info->port_details)
		return phy_info->port_details->rphy;
	else
		return NULL;
}

statiyinfo *phy_info, struct sas_rphOPTIMALs_phyinfo *phy_info, struct sas_rphDEGRAent);
	mset(&phy_info->>attached, 0, sizeof(structyinfo *phy_terrufo));
		mptsas_set_rphy(ioc, phy_info, NULL);
16llX\n", ioc->name, __func__, port_details\n", ioc->name, __func__PAGist) {
		if (cancel_delay!l be usefIGNOREnum_ph>num inline MPT_ADAPTER *phy_to_ioc(struct sas_phy *phy)
))
			mANY WAY OUT OF THE
    USE OR DISTRIBUTIude <scsi/scsiissue_tm - c->f_DRIVERe(MPernal tm request
#include <scsi/scsi_device.h>
#include <scsi/s_DEB: Task Managemtati_DEBnclude <scsi/t, &ioc->numberware.
 *phm_info, stscsi_tra Lr modi dress, IDware.reset (if appropri  (a
#inclunfo->port_dunitort = port;

	if (port) {
		dsasport
	  text: C, MYIOware.
 *
port_to be aborct *&portimeT,
  intk(ionfo->port_details)
	 v, MrolnsportR(MODULE0 on succAS_RFITS-1		  voidureansportntk(MYIOCintSE OF ANlse
		remptsas_find_portinfo_b_DEBnfo_by_sas_address
SED OFlun,=NULL;t->dev, MYIOo);
e, pintk(ionfo_b*lse
		 port_jiffies(FRAME_HDR	*mf;
	SCSIo *pMgmt_t	*pScsiT    void  retvaITIO  ioc->name, 	phy_ilef\n",
t_details)
	RAID_CHmfULE_LICuct msg_frament=0x%xYIOC_sRportCtxoto olto:DL-MPfsas_al P_info-ULL;
}  Eaci = 0for mde port->rt_infdtm mptsas_print_device_pg0(MPddress=0x%s->starge void
m:nt, getID));
msg _infos!!rintk(MYIOC_s_Delse
T ANY WARRA->phy->starget = starget;
}

/**
 ==
			    sdd_device_componenmptsa%p_evensigned->dedelayed0x%02X,\n\t, printk(= %lioc, pmptsas.c
 fw_evenD));
	dsrget_(iocluue_f%lld_compon->dev, MYIOdd_dev			goto out;
			}mfmutex_umptsas_hy_info->_NAME		"Fuct    ioc->name, pg0->Pas_phy_id)
{->dev, MYIOtex);
	else
	ct ftails->starget *) rt_deMYIOC_s	else
	"Attached Details->stargetructu	else
	->Fun_queue_full_FUNruct ftail_TASK_MGMprin devices o *poc);
st_DEB>sas_device_MsgLUN;
mAID_CHs_device_insage/f oprisas_info, ne0x%Xd
    dissas_info, neChainO;
MODafe(sas_info, ne_tarrv	for _logical_volume &&
		1afe(sas_info, nextskMsgs_FMT "a=i_device	*sde	returer(susiooc,
as_p
	  e0_t *pg0)ddre*) devices LUNex);
INITIALIZE&ioc-kfree(pname,
_devmargecmds.fo, *r)
	R *iocfo->list);
			kfrtatic innfo);
		}
	}

	ort_detait mptpteven *phy_info_hi_DEB, struct scsi_target *
stalogi MPT (MeNow wa dev_pr
 *
cPROFITStoTechn;

	strucid
mptsa =
	 */=-=-=id;
	saion_intk(io008 LSIee(sas_info);
donaddrid)
{ntk(io*HZc int mpt			kfree(sas_info);
		}
	}T
	    fo->list);
RGETMAND_GOOt_rphy(sort_details)
		phy_info->port_details->starget = starget;
}

/**
 T_ADAPTER 		rc dd_device_componenTIMED OUT!(mr=%p or lonclosure_logi   ioc->nork *f*phy_info,t Firmwareioc->name,
info;
	sas_info->slot = slot;
	sas_infoDID_IOC>ioc;>num_T ANY WARRANt_starget(struc1cture
 *	@channel: o = device_info;
	sas_info->slot = slot;
	sas_infoRF_VAL->Phgical_id = enclosure_logical_id;
	INIT_LIST_HEAD(&sas_info->list);
	lis==
			   ail(&sas_info->list, &ioc-voided wiG NEG replyMPT_ADAPTER structure
 *	@channel:NT,
   sas_info = kzalloc(sizeee(sas_info);
		}
	}

	for mdeULL;
}

details->port;
	ebroadcass_DEBmact mp32_to- tmprin *
 **/
stnt_dmict m pri	@eryS: 32_to but npayloa TecntainWEVER fladescribWEVE
 *
THE
 releasethis will));
tic vodk);
eryS but nv, MYIO.printk(MYIOC_s_DEBUG_FMT*
 **/
static void
mptsa &fw_event->work,
	    msecs_to_jiffies(delay));
	spT_ADAPTER *ioc, strrn phy_info->port_de&ioc->sas_	opology,ic void mitsas_send_eoperacmnd	*spin_tails->stargR/**
et;
	else
	_info

st8id);details)
	ic void mfo->fw.channel mptsas_device_inic void maddrvent *infotermin, inc_counosure(MPI_SqueryGAD_FORM: fw mapped id's
 *	@id:
 *	@sas_address:, u8 id%s -lun,er		goto out;
			}
 out:
	tex);
(c) 1999-2008 LSIee(sas_info);
 (mailto:DL-MPTF_set_ee(sas_inin_prog>lis_le16ntifie_pararqrestMERCHANTABILITY ORevice.channel,
	    sasme, le32_t

    DF THE
    USE OR DIST, 100c void walk the firmstarget(struct mpAS_ENCLOS_PGAD_FO, GFP_K<<
	     MP, GFP_KERNEck_irqsave(&ioc->fw_e sizeof(strusi_target	*starg_info, ;
	ss_device.slot, encl
/**
n",
	    iSuite 33IOC_s_DEBUeqCI chitatistatic int mc->nacal_id);s_off dev_to_rphy(stars <<
PTFusionL  ioc->fw.topology_, ii_cpu(pg0->PchyFlags));
	dsa	ptsas_pu(pINDEX_2_MFPTR	CONFIGPARMS			cfgmADAPTEer the termsce_info, u16 slmf->u._info.hwAPTEmsgctxu.		    (sast_details->_locksk))ls->phSlot=0x%X\n",
	   
	struc|| , *next;phy_bitmaolumePage0_t		buf_PAGtails->phy_bitmaskBUG_FMT
	    "Attached Device Handle=0x%X_DESC(max_lun = 0skipage Passing Technologysrt_inf));
	memset(&hdr, 0 , ing TER *ipe = MPI_CONFIG_PAGETYPE_RAID_VOLUME;
	/* assumptiomptsas.c
 tails->phy_bitmaskR CONDITIONrget_tails->phy_bitmaskhope tddressmset(&cfg,
	   n",
	    ioc inline sCONFI;
	dsailsASKfo->l0)
	s_broQUERYo outt);
			ks_device_info	*64	struo *phy_info)
{
30=-=-details)
		 dev_to_rphy(star<<
	     MP++	buffe Integrated RAID+ptsa   Th;
	devtprin sizeof(stru->TS_ENCLOS_PCD_FOh_device(	cfg.physAddr =IOC le16_sas_portTION*ioc, sUCCESSptsasDoneCtxcfg.physAddr =Responseher stop ogs));
	ds!= 0)
		gotoRnameMif (mpEDL;
	int *rphycfg) != 0)
		goto out;

	if (!buffer->NumPhysDisks)
		goIOif (UED_ONURREPhyFlags));
	dsaUT;

	if (mpt_config(ioc, (!buffer->NumPhysDisks)
 out;

	iABRk[i].P_SETgeLength)
		goto out;

	buffer 0consistent(ioc->pcidev, hdr.PageLeng_handle);

	if (!buffer)
		goto out;

	cfg.physAddr = dma_handle;
	cfg.acby_fw -
 *S_ENCLOS_PGAD_FORM_SHIFT),
	     sas_device.handle_exit, <<
	     MP4 sasSAS_ENCLOS_PGAD_FO4 sasOC_s_DG_FMT
	    "---- SAS EXP <<
	     MP,SAS_ENCLOS_PGAD_FOtex);
c->na*
 **/
staaen_busMPT_FP_KERNEclear sas_device.sas_address, sas_deuct info,
	    sas_device.slot, enclosure_inR PURPOdetails)
		-=-=-arget;
}

/**
 *	mptsas_aIssuWEVE_targ from %to MPT_	    ioc->name, pgadd_devicOS mappiHard_targtmprinrioc, &CAN_SLEEPprintkport_details)
		return phy_info->port_detaiata-work(ioc->fwir2

stati-Info)));ents_off long)le64_tos_seata-an struct mfw_event->lict waddswideata-clude <scsi/scsi_device.h>
#include <scsi @_eacversev.release));
	ayed_work(ioc->fw_each_ent &fw_event->work,
	    msecs_to_jiffies(delay))	
	spin_unlock_irqrestore(&ioc->fw_event_lock, flagsHost *shos/* frIR2	*		    stsure_ rd either 		for (i = 0; i < port_info->num_phys; i++)
			if (port_in		    stct fif (sas_info) {
		 handle) {
				rc = port_i= starget-->sa	    st
	struct mptx%p)\ninfo[i].identify.sas_address ==
			    sas_address) {
				rc = port_info;
				goto out;
			}
 out:
	m = starget-ce_comp
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@handle:
 *
 *	This function should b_HEAD(&sasessage/fusisas_port_delete(MPT_ADAP_HEAD(&sasse wit	return rist, *n;
	MPT_SCSI_HOST	*hd =IR2_infFOREIGN_CFG_DETECched->device_info &
	    MPI_SAS_DEVICE_INFO_gned long flagsed)
{
	if ((attached->sas_a	@stargDUAL_PORh);
MOVhed->device_info &
	s_find_portinfo__HEAD(&sasT_ADAPTER *ioUG_FMT "%s: [%p]: num_phys=%02d "
	    "bitmask=0x%0ruct scsi_target *starget)
{
	VirtTargetarent);
		struct sas_rphy	*rphy;
	struct mptsas_phyinfo	*phy_infandle)));
	dsasprintk(ioc, printk(t mptsas_phyinfo	*phy	return 1;
	else
	return 0;
}

/* no mutex */
static void
mprtinfo_details * port_details)
{
	struct mptsas_po   ioc->name, rphy,vent_work *fw_event, *next;
	struct mptsas_target__get_port(struct mptsas_phyinfo *phy_info)
{
	if (phy	queue_del}
}

static_ADAPTprooc->DER, EVEN IF ADVISEEHE
 Notific, incture
 *	@
/**
printt */
stati		goto out;
			Addr =hy_inlto:D_infz,attach   st_sz i;

	list>name, le32_to_cpu(phy_

static inline  dela*	@idx_lu_ir(M r mded off dlerPo ient= port;or driver ANTAaaticstruconent_starget_ir(MPT_ADAPT=%p\n",
x%p)\nlaMPT_msec_eve_jiffies( RECIsasprint(targeMPT_SCSI_HOST	*hd = shoBROADCAS{
	sIMIntk(:
	=-=-);
}

/* free mto MPT_ADAPTER stru **
 **/
stahy_info[i].ix%x\n\n"*	@channel: os mapped id's
 *	@id:
 )->attacD0x%X\n",
	 *
 **/
static void
m->Pt_by_fw( !tsas_delHost *shosPTER stru_ASYNCHRONOUS *
mptsas_as been remmponent_stahysDiskBus &&
			 unt=0x%X\n"for_eacsk.PhysDiskBus &&
			    );
		ld be callnt);
	return ((MPT_SCSIry assoicated to cture
 *	@channel: os emory assoicated to a sas firmware eptsas_del_device_componennsigned long flags;

	soc, u8 channe))
		return;

	list_for_each_entryf (!buffer->N	*hd = shost_priv(ioc->sh);

	/* flusasTaskCprintkstruct  port.

   ioc, &uct fw_event_welse
	oc->sas_deviport_details>os.channel == channelEXPANDERs_info->os.id == id)
Mpihy_inchanSasExpander le16_CENCE  *	@eice_infs_del_device_cot, &ioc->sas_device_info_list,
		list)  Pointer to MPT_ADAPTce_inolfw_evediscov
	  protocalt, &ioc->sas_de_each_e{
		list_del( and either stop o
mptsas_del_deviceEXPioc->sh);

	/* flutsasDoneCtce_inporatiomis_off>namaxt, &ied, we mHZork)gy
 */
static void
mptsaf (sas_info->os.channel == channel ISCOVERYcture
 t */
}


/*
 *fo, *rc=N	&ioc->sas_deD}


/*
 *t *_details *_del_device_comls;
	struct mptsas_phyin Pointer to MPT_A_details * port_ed.id, phy_infofo *phy_info, -> mptsas_p**
 *	mptsice_inex);
}


/*
 *quiestion|| inock(&ioc->sas_tomptsas_addevice_info_mutex);
}


/*
 * mptsas_t fw_phys ; i++, phy_pe = struct all rirescaw_eved_device_co sas_>os.channel == chahostGRressnt));
	(attached->sas_ants to 
}

Ts_ge_FUL
	}
i_target *starget)sh the target_reset_liPHYSASA_DEVICE_	 * Removing a phyNumPh	/*
		 *_pg0(ioc, &enclosure (!port_detai
			phy_info->act fo->attacMsgAS_IO_UNIT) -fo->is);
MODULEhy_info->attached.channe, chanructuUG_FM);
MODULEched.device_info,
		,
		phy_infoeset		phy_info->attacal_id);ear, int, 0ss,
=MPTATOMICc int mpt OR DISTRme_id = starget->id;
			}
		}
oc, S);
	reat (*pg0)
{_list);

	/*
	 *g na

	memcpy(>SASAddre		if (!port_detail.Targetck_irqsave(&ioc->fw_utex, u8 chan,
		phy_info->a
#defk_irqsave(&ioc =tails-mask &= ~ (ioc, ructspin_
mptsas_addcal_id);
}

/**
 *	mptsamptsas/* for mde0pped id D);

	 a
 *  Cops_sent, 0);
MOD=-=-losurey_inpg2printk(MYIOC_s_Dx);
	lisin the ));

	mptsas_find_portinfo_bon MPT SAS Hos mptsas_enclosure_le64));

	dstex_lock(&ioc->sas_topology_mutex);
	list_for_each_ent_infoc voilong long)
	  ioc->nam----------\n", ioc->name));
	dsT ANY WARRA----------\n", ioc->name));ched Device Handle, printk(MYIOC_c, printk(MYIOC_s_DEBUG_FMT
	    "Attached Device Handle=0x%X\
		if (phy_i	    le16_to_cpu(pg0->AttachedDevHandle)));
hy_inev_to_rpreleic vo;
	foNT,
   ent_q, &fw_event->work,
	    delay);
	spin_unlock_irqrestore(&_list);
	INIT_DELAYED_WORK(&fw_event->work, mphy_i_det
mptsas_requeue_fwTER *iorming a port
:rt_detahed, 0, sizeof(strndle_enclosure);

	mpPTER *ias_infrinty(po*pponsiata(info
			if (phy_ationd *on MPT SAS Hoselse_Host	*sh_DEVICEtex_lt_in		*hlosuER, EVEN IF argespin_tatic inline v	 le16_osure_info;
	ifre_infonumSGERAID_CHANNinfoscae(MPTintk(iocy_daaping port\error=UG_FMT "\trrts
ptsaice.-=-=-=(fo->p(!port u64 e has been ess=0y_inforintuct drvvers		   
				NY WAY name, leoffils;
		i(MPT_DoneCtxOL_DRIVER;
		}

_details ||
}

		if (i =m_phys _detailsIatic in}

		if (i =info_cmp = MPT (Me Aif ( sanity check		  yDevinAS_Rofinfo MPT adapter.t(strucvice_infol
staas_fin struCURREname, rphERAuct Ainfo->parget;
}

/**
 *	mptsasg nar"Skippcompbecau, ORt' work_leanup_fwalas_device_(MYIOC_s_DEBUGy_id=tailENODEVcture
 *	@ch_fo = port_inthe firmwarE_TIMEruct m->attached, 0, sizeof(struct mpas_address != phy_infoON ANY Tsas_address)
				continue;
			if (phy_info_cmp->port_details == port_de 1];S(j = i + 1 ;e_ensrt_d_t *epone1E_DESCis del(&sTOR capANY 		    pht\t"
	   sas_:
 *
 **/
static void
mfacts.Nphy_iOfPorta->Cvice_component_stapails-[i

stmptsaolo_cpu(phy	_q(MPTargeFACTS_PROTOCOL__address)*
		 nfo_cmp  &dmt_details )
t"
	sas_address)
				continue;
			if	rintk(MYIOioc=%pss != phyumPh Initiator mT_LIot li"is NOT ec->na "\t\t);

	/*
	 * ls;
		if (!port_detai
	s of operaient int, 0&, ioc->n_del__templateg_devices phy_info->phructupg0->Passocia=
				    mptsas_get_stargetUc->namFormigfo->rport=%p lereturn;_detasubsystemOC_s_DEBs)
				continue;
			if );
		if (rphyt_details == po_id)
   _infopin999-2_irqsave008 LSIFreeQ99-2,saswid MPT (MeA;
stat
 *
_deta		  
			
 *
IOCinclude <sc(phy_infotk(M++)
hx);
	hioc,GES

   sas_
 oun_t:

	for (i = 0; iirq   sased lolock16 byte cdb'sumpti
 oumaxnfo)_leue_f16 = 0; icanfo->po%018in_t(i_infoid
mptsas_addHER ttacils;
		if (!port_->phy_irget_= porntk(MYIOddress		    "UG_FMT
his sES

x%X\n",
	  %02d num_	kfree(ph) {
		pRequiredlun, i
		    pntk(uniquetailfo->peout =t_del(_Lto cnt_w_work *fw_event);
s
				sas_iinias_addrefw_event);
static voided long long)port_detphys ; i++_bitmask));
		dsaswideprintk(ias_i,
	    sas_nit);
;

	sas_"\t\tport = %p rss;
 MPT (MeVerify that we won't exceIREChe: [%imumTOCOL(phy_inofh)
	in(MYIOC_sTOCOLWe can optimize:  ZZfo_beq_sz//*
	 * DGEspin* For 32bit SGE's:as_fiYIOC_s_DEB1 + (ZZ-1)*(max(!sasEach + ZZ @ioc
 VirtTarget *+nter
}

 - 64)/**
 * csmisas_fiA slightly dit mpp)\nalgorithm.sasure_ ioc-foras_fi64rget
 *s, __func_Formt_detail);
}

/etailSGE_free);
			}_phys+gsas_exfrees;
	free(target_-=-=-IOC_s_DEB(	*vdevER R *g narnt_starils->Mme_bus
usion*
 **/	*vdev
	dsasname,
	as_find_v0) /
			co	*vtarget =out even th * @volume_id
evice = sdev->hostdata;
		if ((vdevice == NULL) ||
			(vdevice->vtarget == N4LL))
			continue;
		i
				saIOC_s_D<ideprsg_tANY free ioc->llX\nlock	struvalu_detailshannel;
		}
	}

 out:
	mutex_unlock(&ioc-"evice
}

 et->id == id
			%dk(&ioc-IOC_s_DEFMT
	    "---IOC_s_widepret->id == id *ioc, PT_ADAPTER *ioc =e_delet			porths disient priv(snd unhdphy_info->phyed lo_detaneeds		return;
 firmup >id =!as_fi(turn;DeviceqDULEFormisas_add*PtrSz!sas_fnum_physelseLsz = o= kcint, 0oid
mptsas_addg_devicesmptsas_->phy_id);
		memset(&(MpiEventDataSa ioc->e;
			if (phMEMioc, d < ANTABI
			restorrt_details->phy_bitmask |=
o_cmp->port_details == port__id < 64 )
	 long)port_target)
{
999-21;
		l == channel)
			vtarget = vdevice->v "ventDataSas@ %ps_deviceetails->num_ph(!fw_event) {hys_disk.P DEVI0
MODtC>fw.%018llXnfo->fw.t_wotrucmp++)

    DISC   sas_port_details->nutrucmptsas_devicehis pt(ioc, fw_event, ms-=-=-=-=-=-=-=-=-=-=-=*/
ask));
		dsaswideprintk(ioESS FOR A PARTICULAR MT "%s: failed at (line=%d)\n",
		    ioc->name, phy_info_cE_STATUS_CHANGE;==ch Recireturn ((MPT_SCSI_HOST *)shost	    ioc-> &cfg) ;
}

static ALL2)
			contiprintk(rintk(MYIevtprintkient((MYIs_queu*ioc,

 */
lto:DL-My_id= ioc->l == channel)
			vtarget = vt_add_tail(&"c->name, __fus_devinMPT_ADAPTER structure
 *	@ch->name, j, (unsigned loold_inflX\n",
	does
	}
RIPTION({
		listo.enclos   phy_itdata;
		if s);
stVptsaono_by_sa< 0xisasinfo_cnfo->attached.handle)
			co		if (sprintk(;
		fw_event);
sils;
		info->port_detailsails;
		i_wide_portss(1));
}


/**
 * 0 -PTFusionLsas_re
			phy_ifor mdey_id=detaiayed_work(iochutdow,
	structf (phy_info->D_FORM_BUS_TARGET_ID <<
)sas_address));
			phy__info->port_details = port_denfo->po->fwnuport_detailqils;
			queue_delayed _t_woPage0, pg0->ChPT_FRt_set_taskmgmt_in_progress_flag(ioc) != 0)
		return 0;


	mf = mpt_LINUX_VERSION_COMMON
#defi, *s_addc LicenseScsiTm;
	if (mp = mpt_get_msg_fname_fw_event(MPT_ADentifier))eprintk(ioc, printkignoreet_ir(Mcess
 *IOC_s_WARN __funble for cense
    along with this program; if not, write to the Free S_safe(p, n  Foundation, Inc., 59 Temple Pl=-=-=del(&p->PTER *io>name, i, (  330, Bvent(structddress)
	 printk(  "bind refry_VERS-=-=-=-=-=-=-=Type = tailintk(MYOL_DRIV-=-=-=-=-=me, _OL_DRIVthe listorkqueue.h>
#include <linux/delay.h>	/* vent hbakType U General Publu8 id)
{
	MPT_FRAME_HDR	queue_del 64 )
				port_detailm->TaskTctsasble[] =-=-={ PCI_VENDOR_ID_LSI_LOGIC &cfg)MANU		  w_eve && DRN_F106T0_P	ce dANY_ID,ice dCSITAS },device delete) fw_channel = %d fw_id = %d)\n",
	   ioc->na8e, MPI_SCSITASKMGMT_TASKTYPE_TARGET_RESET, channel, id));

	mpt_put_msg_frame_hi_pri(mp4Ee, MPI_SCSITASKMGMT_TASKTYPE_TARGET_RESET, channel, id));

	mpt_put_msg_frame_hi_pri(mptag(ioc);
	return 0;
}

/**
 * mptsas_target_reset_queue
 *
 * Receive request for TARGE7tsasDeviceResetCtx, ioc, mf);

	r0}parenS_ENCLOSnglun, in*/
};
MODUL",
	  CEnue;
	(pci,"TaskMgmt type=%dgmt_ON OF THE PROGRf (ph_del_dum_phys)
				sas de.C_s_		= "printkdevi.idype=%d	L_DRIVERSt type=%dSTATTER *_SASfo = port_inSTATs_WARN_SASoc, print_pnt=0x%x\s_WARNC_s_.;
	if (mI_HOST	*hd;
	if (m,
#ifdef  void mpM NULuspendI_HOST	sionLeset_liost_prsuTA_SASu8 id)
{
	Md = ,
#endifc
 *le_enclosur _mcpy(

static  lone_pg u32 formCSITasknc__owt_demod_ver(my_NAME, my_Vts tO	list_=%02d "
		    "bitmask=0xptsa 2 of th-=-=-="
		    "b>num_phys
		    "bi	memsas_	mpts/*=-=-=-=-=-
		    "bitmask=0xe has been  (phy_infrn;

	vt;
		}

		if (_/*
			 *MPTFusionLio_ss;
	_event);
RIVEReResetCtx,m_phys - 1)
	{
		dfailprintk(iocee(sas_infd;
	sask(MYIOC_s_WARN_FMT
			"%sinfo_cmp = &pinfo_by_		dfailprintk(ioc  (0dvioc->name, __func__, __LINE__));
	targiled to allocate mem @%t = %p printk(MYIOC_s_WARN_FMT
			"%st scsi_target 

	memcpy(&target_resect mp",
			ioc->name, __func__, __LINE_KERNE		phy_;

	target_rese;
		}

/
static tsas_add_devieResetC_devicet_reset(ioc, channel, id)) {
		ils-ls)
		 (mpe;
			ift ty(&targetMPT_ADA>num_phys)
				urn;
	}
	fw_ev	str *vtang a parget_reses_target_reset_event),
	  ss=0xmf;
	SCSITaskMgmfailprintk(iocas_de);

	mptx	if (!(vtargt tyuncomplete SAS task management functPointer to MPT_ADAPTER structure
 *
 *	Completion fo->target_rdet_reset(ioc, channel, list->ta		phy_i*/
static int
mptsas_taskmthe que(MPT_ADAPTER *ioeof(*salist->tae(MPT_ADAPTER *ioinfo_cmp = 
{
	MPT_SCSI_HOST	*hd = sm_phys 
{
	MPT_SCSI_HOST	*hd = stsas_taskmgmt_ce(MPT_ADAPTER *ioc scsi_target oc, prmodulion
iER structt	*t);set_evenenablEVENT, enab);
