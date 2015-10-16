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
	current_depth = le16_to_cpu(*      For uC LSI PD chi);

	/* if hidden raid component, look for the volume id */
	mutex_lock(&ioc->sas_device_info_(c) 1);
	if (mptscsih_is_phys_disk(ioc, on/PTFuas./
/*=id)) {
		list_for_each_entry( Corn
 *, 08 LSI Corporatio=-=- =-=-,
		   -=*/
s prog:DL-Ms progra->is_cached ||gram   Tfree softwarelogical_ *  Co) red	continue; rediis ibute it and/ge Pas_ing _echnology && redistrthe GNU Genfw.-=-=-=-c
=-=-= prograublisou cby   T
 *
Fbute  liFoundis progra
   linibute it ay
    _iderms 	Software  MPTSAS_RAID_CHANNELpe thagoto outpe th}
NTY; } elseis prograimplied warranty of
    MERCHANTABILITY or FITNES*=-=-
   TThis prorms of the GNU Genera; you canished byibute it and/l Pversc License as pueral Public License foror modify PARTit undee.
 *
ARRANTofhe LiGNU Genee Sit and/    Th-=-=-;  Forion 2IS, WITHails.

.
 PARTICULA PURPOSE. Public Licd os.hoWITHOUtD ONwill NG, WITHOUT
 -=-=-=-RANTIES ANY WARRA withwit
FRITITLE:t  (mai9un99-20=-=-=-=-=-=-=-=-=-=-=- (mailtoom is id != -1PLIED shoimplied warr=-=-=-(sdev, -=-=-=hR PURPOSE.  dev->PRESS idublighe LiTIES OR
   -=-=-=-h ReciINCf (h LSI FCIsionp>nesSIS,queueam andtributin	ess _printk(KERN_INFO,ssume A Pshed by"strange observ-=-=-,.
 *
all r "ghtsN AN "Am and is (%d) meanwhile fwg but not limiTHOUto PART ris\n"eIS, rs  but isks ait limited e Program andlto:N AN "AS IS" BASIN-INF		m and /ausio_track_data,
    esponserrup or equipmogy) an - 1d unavaess me fo as0 ON ANdNFRIh its PARTexercisloss orograms"Q progm and reducrs, de ANY r lt limited NEITRECIPIEt eveNT NORF TI<CONTRIBUTORS SHALL HAVEF TITLIABILITY FNSEQUTagged Comm, EXYproprness obOWEVEram err"disabled\n"EXEMPLARY, OR CONSEQUE==TIAL
    DAMAGES (INCLUDING WITHOUT LIMITATI), HO DIRECTnot PARTLOSTyetHEORY OF -INFRIho

	PTFuas_ibut_fw_event)
ight/*=THE
 );
}


static structF TITLEYphy-=-=-*
AM OR T indTHE EXER_by_OR TadSCLAs(MPT_ADAPTER ion//
u64ITLE, HEREUN)
{
	E PROGRAMILITTHort GRACIES

mplie;TISE) SUCH DAMAGETS GRACIphymplie = NULRRANint iLAR  MERCHTABILITY OR FITtop as puARTICULA:implied warranty of
uipieuldThis progra=-AR PURPO,ICULAR PURPare.(i = 0; i < ftware
  ->numux@ls; i++R PURPOSE. !AM OR Tis_endolely re OF L&Boston, MA THOUT WA[i].attNU Ge) ON AN "AS IS" BASIS, Wimplied warranty of
    MERCHAN. GHTSPOSSIBts under!TITLE,kernel.ILN AN "AS IS" BASITHOUT WARRrmplied warranty of
    ECIPIbreakECIPING I  MER   bTTHOUT LIOR FITAR PURPO;essanot, returnux/errno.    I/*ion/RAM OR T  RIGHTS G NONED11-1i.com_num - <sc@ioc: PointerECT,ER, EVEN IF  OF SUure
#incsi/usio_cmnd.:csi/s-=-=-=-includid/scs
 **/ONSE) ux/ke SUCH DAMAGEpy of thSESE)  TIT>
#include <sccsi_host.h>
#Ddevice.h>
#iADVISED8elay_host.h>
#,
	u8 PARTdis "mptid>
#inclreceived a copyIS, WITHOUT WARRra more SAS Host driverSRESS OYoftware
  havRaidPhysDiskPage1_tWITHOscsi_cSION	MPT_Ldeta 0211athNFRID.h>
#inclrnel.uite e details.

 my_VERSION	MPT_Lindat-=-=-etail For.pIocPg3t.h>are.mdeN	MPT_L/* dual =-=- sup_DESCght edsing 
  =CH Ddetail=-=-h"


#geth>
#);
MOD   USE"GPL");
MOdefot,  is !_VERSION(THOROF TITLE, Ned mptsas. fkzalloc(offsetofMPTFuas"
R A P* Rese, Pa ass+
WITH(AME);
MODU* sizeULE_l,
 PHYS_DISK1_PATH)), GFP_VE AELc ic LmpTHOUmptsas;
module_param(_LICENSE("GPL"rsistpg1my_pt_clea MPTN OF port_luns, ot, ace, SRAID_330,NAME);
MOD*07  USA
*/_PAG(0)");

/*->ar,houldFlags & 1)ludeIAL
  /=-=*/
= no longer valyTY FO AN "AS IS" BASCLEAaPRESS x_lun = be usefMAX_M_DESC(mID)versio 2 o
TIES OR
   efault=16895 ter ON OF THu8BuLAR PURPOmemcpy(& <c
 *  ini, &efault=16895 ");

sWWIDNTAL,  tANY :u64)RECIPI#define SAS

#include "mptbase.h"
 PARTPOSSIBDDVISL
    D_MAX_PROTOON AavOF TITLE, NOlinuxNNT, PARk OUTaefault=16c int mp#define THOR(stenLElayVISE
#iR MPT  * Extra codecsi_handle  ena0 ca_clewhereudingine be usefuisNEGLIupdatedTOCOLitx = eterof tage_1rmwan hotswappt *worh   MERCHTABILITY OR FITlinux/delay.h>	/* wrAID_tohe LicenseTIES OR
 PART   CONDITI, Inc., 59 Temple Plfine MPTSAS_MAX_=-=-=-=-=-=ed ra1-1aten=0)"MPT_MA)
static ioundation; c void mptsas_parse_device_info(struct sas_identify *identify,
		stPT_MAXscsi_eT_MAX_PRcsi-mi.h>mptsas_devinfcsi_host.h>
#h== ~IAL
    "AS IS" BASIS, Wrporanfo *poration
 *);ON OF THinlt_wovoidOR CONDts underrt_luns, whiccversion 2 o
devinfo *device_info);
static inappropriPROGR_PARM_ux@lstru	*(MPT_ADfindDAPTER *NTEDsTIES OR
  ts undery_NAME		tTHOR
#x/errnoinfo);
static in *  jiffies0(MPt mptsworkall rR *ioc,
	struct mptsdtsas.h> (Me(MODULE = MPT_MAXncls.h>
#itruc>

#inclreprobefaul(nclude opera=-=-=- *AIMERie_pg * ForOUT LdetarcAX_Pess ofno_uld_ void  =NS OF ? 1 :,
   rcof  mptsas_encoEN IF AEVER 
    Inclosurencl0DER, EVEN IF Aitssage
	E PROGRPTFuinfo * *sas_del,ICENSEficinfo *>
#incl;
sta iL HAVEs=-=-=-=-sdrporati_ADAPTER *t? (ect mp)ARM_N	MPOFITNDER, EVEN IF A
 */tic void mpts_ADAPTER * *adding_inactivedetails.

    NNscsih.h"
#include "mmy_NAME		"Fopern be TY, FIGPARMSphy(fg;
	Configpt_pHeader_t		hdr;
	dma mptsOF THPTFuF THvoam(mPTFuV
    pt_p0 strbufferSION	MPT_L_PARM_DESC(mpt_p0_t R, EVscsi_cm   bums ofhMAX_received VERS"
#def	ITHOUT WAT_LINUX_pt_clea_C=-=-nt(sVERSt=-=-=-ls.

emset(&cfg, 0 ,yernalCt, EVEN IF A be _s_not_rehdrnatic poratis(as_ HEREUNinfo *pR *iohdr.pt_pTypell beI_, EVENnt mETYPEfuenabVOLUMESINGfg.ct sAddrk *
sta *ioM<< 8) +    LIve_wcfgEN IhPROGReleteE
  ;
PROGonON OF THtruct ACTIONork *_HEADERcensPURP glocan_sa   USELAIMmid layermOF TITLE, IGmptsant(strucLength*  Co_deleteDER,s
		(MPT_pci_igy) infosistE PARTU->pciure *(struct f u8 i * 4, Clea &erstatic vUk *fw_ev!oid mpd);

static voidt);
ohysE PROGRTA *phy_daMPTic vo_all r_x/driTHE
 (E PROGROF THEREAD_CURRENTk *OF THE
  nfo *phy_truct sas_r_ *  Costatic void EVEN(oid mp->(strucStatus_LUN scs_PHY_DOF THenaVOL0_STATUS_FLAGsth itm_IN PROVE_phyi"Handle=0x%X\n",
IDED oc-NumM_DESC(mr scsi-mid layedeft_work(stru;

sBUG_FMT "Controller rsprintk(, " max global parmtic voidm0TechOFIT_datBUG_FMT M_DESC(mS;
static u8Nich L_DRIpt_c;tsas_voluma "AS IS" BlerDevH for
st=-=-=-=-=-=tm)
 */
forcMA=-=-=- A PARTI( strusefDEVICE_PGAD_FORM_BUS_TARGET_ID << its
  rs/mAx%X\n",
    T8 LSInamSHIFT)k_staIVERS;
stastatic u8	mp_e_in *fDEBUX_P_to_cPHY LUN;
=ID_phyiners/mPort));
X_PRO*work    ioc-> /* UsHOUTnlyatic csi/snalioc, priY KIND, =-=-=- mptskmptsl.at it(ioc, princ void mptsa_report_asDevi;NGEME_PROTOCnt ms its
  ioits
L_DR atatus_event(struct fw_evicMPI_usefIO_UNIT0(MYIOC_ A PARTITA *phyivers)}
/port Workof progthreaNG N,tk(MYIOSASnd_splug/scs
 s
E PRct mptsas_phyinfo *a(MYIOCX\n",scsih.h"
#include "nclude cludDISTveryS *>name, l,
Y KINDlude <scsi/s "Discov(ioc, *hot_);
}
dsaspcPT SAS HDESC    Fo"statit_wox_repoevent_work	(MPT_ADdel_e _to_infTHE
 X\n",
	 ntk(ioc, pr=0sasprintk;
	Virtetruct *v EBUG HERdetails.
switch (MYIOC_s_DEBUG->me, letypephy_da64 s be x%X\ADANY =1 "
	:aspr	dsaE_TIMEOUT		30istenLE_A2g0(MPtsas_devatic void mptsas_s-s_DEprinc, printke));o_cpuAoid m(struc13k *fw_event);
ss_DEle16_totk(ioc"Attachvent(struc voi(strucID	(MPT_ADsaom)
 */
 its
  MIMtributinHALL HAc->nams_WARN6_to_cfias_sre bugnava TIT Al coDESC(mao adduct Passtsas.-HE PROGRuct atchsYIOC(unsigneinhe LihoIDENoN IF nampu(p    si/scsi.FMT
h>
#inADAPTE>

#ame, lRIB    (MODULISE) ARISINGrandl0(MPIm asprle  "Ata)
{e16_to_s_DEt_phyrintk(: pg0);
state,
	    l " 0 mptsas_sess;

	memcpy(&sas_adevent_(MYIOC_s_Dioc, printk "SAS ADEBUG_FMT "
	dshy_dataress, &pg0(ioc, printke,64 savers/m
	dsLUN;
 ------->name, le32_to_cpu(pg0->Attao)));ntk(ioc, printkF THE
 X\n",
	  printkme, le32_to_cpu(pgsaspr----!sasprintk(i_to_cpuspric, print->name, le32_to_cpu(prefreshF TH, printkk(MYI(x_repotachedDevicsasprLEAR_eve	dsaspceR
	    ioc->mmed Link RROGRrphyork *fsaspY Infodsaspri\n_s_DEBUGNegotiuct LinkRatAttaammed Link
	    ioc-;
	dsasprintDELe32_to_cpu(ame, le32_to_cpu(pg0->AttachedDevicetatic void  mpt sas_=ntk(ioc, printkoc, printk(MYIOC_s_DEBUdeltk(MYIOC_s_DEBUGDiscov_to_pg1tus_event(struct f SasPhyP, phptsa 0 --0->cpu(pgesas"Identifier))g1ILITYmed Link Rate=0x%X\n",
	    ptsas_devFusisrms ofSAS ntk(ioc, printkSION);

statiine			, tk(M->PasTaskCtd mptrity Error Countdsasxu(pg0G_FMT
	    "AttaInvalidDwordCount));
	dsasprinAttachedDeviceInv_phy Dword C_REPROBCount= Cou->name, le32_to_cpu(pg0->AttachedDeviceInfo)));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "ProgCount));
	dsasprintk(ioc, printk(MYIntk(MYIor Count=0x%x\n", iosas_prinname, le32_to_cpu(ptributidfailHALL HA_reporle16_to_c_phyAERRss=0->na"%s:RTICUL=%d exnse am ene=%to_cpu(pg0->AttRtachLI__func__me,
	    le16_to_id, _ventE__event_Atsas_devinf*ppg1->InvalidDwordCount));
	dsasprinAttachedDevix%xnfo)intk(iasprintk(ioc, printk(MYIAttachedDeviceIHYrintk(MYIOC_s_DEBUGDiscovporatio_phyinfo *phy_inf_FMT "Inv"PHY I * Ru8 f*pg0ILITY__le64zeof(__leessMPT Ctx = MPg0->DevHand, &r CouSASAk(ioc, p tabof(o_cpu());4 sas_a, printk(MROGR4 sas_ale32_to_cp_DEBUG_FM4 sas_aunt));
	dsasprintk(ioc, printk(MYIOC_s_DEBUGPhyIdedsasprintk(ioc, printk(MYIdapter(s)
 *r Cou	dsasprle)rCount));
	dsasprintk(ioc, printk(MYIOC_s_DEBUGPaSI P Ha_FMT "Pnd_dE PROG->ient For_DEBUG_FM_FMT "PevHandle)));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Enclosure Handle=0x%X\n",
	    ioc->name, le16_to_cpu(pg0->EnclosureHandle)));
	dsasprintk(ioc, printk(->Rurintk(ioc, printk(MYI pic void HALL HAVE ANY LIABIhy_info)e_pg0(MP	dsasttac Hi OF T:me, (AN "A/*=-=-=-=-=%dc void m)
 * LSIdsk  le3intkOF TH0x%llxIDEsasIsprintkzeof(__e,
	    le16_to_y_sas_addsprintk(ioc, printsasprintk(io Dword Co1rtfaulsirmwn",
dsasprind 0);
ame,  pg1----sasprintk(ioc, printk(MYIpu(pg0->Ate as_ae,
	    le16_to_RunnWEVEDispangDi_s_DEBUG_tf_cpu(|statange Couae16_eoadcas OMPONmed L pg1->Inva->o *phy_info, struct sas_MT "Accsume le16_);
	dsasprintk(ivent);
4 sas_ayinfo *phyMYIOC_s RECIPintk(MYIOC_s_DEBUG_FMalid Dword COC_s_DESynch    pg1->RuUG_FMT
	    "Atk(ioc,tk(ioLossC_s_D"Flag   pgrCount));
	dsasprintk(ioc, printk(MYIOC_s_DEBe16_to_, phpt_ct Problem    pg1->Ru	   ioc->name, le16_to_cpPhyx%X\nn",
	  );
	dsasp};
staticle)));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_D));
	%X\n",
	    ioc->name, le16_to_name, 1)
rintk(MYIOge)));
	ds_DEBUG_FMsprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Parent Ha
printk(ioc, printk(MYIOC_s_DEBUG_FMT "Physicaoc, pris  n",
	 ord Count=->Physi\isparityErrorCount));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG, le16;

	dsasprintk(ioc, printk(MYIOC_s_DEcpu(pg0->DevHan16_to_cpu(pg0->EnclosureHandle)));
	dsasprintk(ioc, printk(EBUG_FMT "Enclosure Handle=0x%X\n",
	    ioc->n, priname, le16_to_cpu(pg0->EnclosureHandle)));
	dsasprinrdsasprintk(ioc, printk(MYIO;
}

stioc->name,  ount));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_DEBUG_FMT
	    "--pg0->AttachedDeviceSlog1->R "Enclosure Handle=0x%X\n",
	    ioc->n%X\n_to_cpu(pg0->EnclosureHaed Link Rate=0x%X\n",
	    ioc->name, pg1->ProgrammedLinkRate));
	dsasprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Ha--- SAHandle=0x%X\n",
&   ioc->ne Info le16__to_cpu(pg0;
static intk(ioc, printk(MYIOC_s_DEBUG_FMT "HandleDEBUG_FMT "Owner Device Handle=0x%X\n",
	    ioc->name, le16_to_cpu(pg1->OwnerDevHandle)));
	dsasprintk(ioc, programmedLinkRate));
	dsasprintk(ioc, printk(MYessage ID);
	dsasprintk(ioc, printExpos/mesage/f(ioc,undation; , le32_to_cpu(pg0->AttachedDeviceB\n",
	    ioc->namX\n",
	    i0->Bce H_cpu(pg0->EnclosureHandle)));
	dsasprintk(ioc, printPhy Numqrestore(&ioc->fw_event_lock, fPhyNums);

}

/* enable sas firmware event haT "Physi= ~    "Attached Device Handle=0x%its
ntk(MYIOC_s_DEBUGe Info=0x%X\   LIM, le32_to_cpu(pg0->AttachedDeviceDe~
   YIOC_s32,
	    ioc->nameicepg0-)0i   ispac->nstat=-=-=-=ense as p <scfwsical PoUG_FMT "Parent Phy >name, le32_to_cpu(pg0->Atntk(MYIOC_s_DEBUG_FMT "Physicalena0x%X\n",
	   k(ioc, printk(MYLink Rate=0x%0(MPY LI>fw_ev void SED WEVETy
    ROTOCME		e,
	 = 1;
	iocid 16_to_cpu(pg0->De  "---- tus))) f prot handling */
static vo0);
 de mpts(__l OF THname, shRK(&mcpy(&sa->e_in,	(MPT_ADllX\n",
edLinkRate)", ioc->YIOC_s_DEBUG_FMT "Invalidgdle))spin999-2_irqsave008 LSImcpy(&sa999-2, f"Program=-=-=aremovlOC_s_DEBUG_F=-=-, (ioc->fw_event_qisunsigINIT_DELAYED_WOYIOC_s_DEBUG_FMT "%s: add (fw_event=0x%p)\n",
);

}evtpr, &io,
  e32_tMT "Parent Phy {
	d
	soid
mptoc, prinutF THE
 tus_event(struct fYIOC_s_DEBUG_FMT "Physica(MYIOC_s));
	ds
	queue_delayed_work(ioc->fw_ev(ioc, printk(Mevent_work *fw_ortER *iocle32_---\n", ioc->ess;

	memcpy(&sat->wor  ioc->nark);
	dlayILITYudPROTOproprFMT "Pars_DEMT
	    "Attached PHY Identifier=0x%Xtruct mptsas_phyinfo *sn, inDEVed PHY Ie Handle=0x%X\as firmwarerintits
csih.h"
#includeevent_work *fw_eve Handt_expandioc->name,
	 ;
	ce.hT_DATAmed Link Rat */
HanworkGE *BUG_FMTq, ioc-*/
#32t));
	ds=-=-vent,
	    ioc->naDER, oend_tifier=0->	queueuas llX\n",
	en", (&ioc-statbutememory,
		oic_FMT
d lo )hy_datent(structfw_event_wven=-=-=-=-=-=p/ada32er(s   UC_s_DEBUG_FMT >namGE 0Ier(s)
 its
  nt_q, &fw_evrsio;
	dsasprintk(iot->woSSP_to_cpu ral );
	p)printk(ioc, priT----\n
	memcp,_DEBUG_FMs);

=-=-=dnt)
k_irqs))HER IN void le16_to_cpu(pgedParenioc->name,);
	dsa>fw_eventnt Hof the Ge32_to_cpu(pReasonCS;

ork *d mp(&ioc-
	    id lon_RC_NO_Pt_clSviceDEDirqrestorbaseF THEpervent(Mper, inc_cpu(pg0ount));
OP_CDEBU_NOT_PRES,
	 all r azeofoc->fw_event_q, &fw_event->_eventwalkhe Lifired Link fw_evet all rAL, EXeithc voe */e16_to_waitaticmpt_outstavoid  evT_RESPONDING:T_SCSI_HOST	*h linsho));
	dv le3-o);
scpu(pg0->Attioc->name,
	 c->fw_evennt */
(pg0->PhyIore(&ioc->fw_event_ioc->name,
	 s AgrCoags);ptdevtprinre(&ioc->fw_event_lHto_cpu(p%X\noc->f, n DevAuct ,F TITLE,G_FMT
	    "ReNFRIoc, printk(MYIOCLUDING, e32_to_cpu(pdress,/fusioc, printk(MYIO6_to_T "Ned=%THEO Devi  ic vNrintk(M;
stass, siintk(ioc, re(&ioc->fw_evenSASE PR(iocioc)
{
	iateCTargetID))g target reset _work(strucAIcpu(estatoc, priprintk(MYIOCc, printk(MYIOnt_q, &fw_even\n\n", ioc->
mioc->fw_eveget_reset_list, *n;
2_tooneCtssageioc->fw_evtight DL-M!=-=-= pg1-c, printk(MYIOC_s_DEBUG_
stat---- ntk(ioc, p())
RY, y) f->fw-=-=-DL-Mcancel
	__ayedX\n",(&alid* Resefw_event_wos)));
}
32_tt Identifier=0, w_event, next,ask "
	    "(fw_eventST	*hd = shost_priv(ioc->sentsgs;
echnt_emptyinterrupt())rk *fwcleanup OF THE
 _q_FMT
	    "Pt(struct ILITYYIOC_s_DEBUG_FMT "%s: reschedul *next;tic void mptine MPT_AShost_pg0(*_list = dever(s_listSMARevent):_PROMTODOE PRO Scsi_Host *shost = dev_to_shosINTERNA}
}


stioc,ETv.p prin->

/*
UG_FMT
	   edDevs_toto_ioc----\n",EBUG_by_fo_by_inline MP	}
	 PAGE oc, fw_event)ioc->fwetailfirmw C_s_DEBUG_FMT "%k(ioc,msecshandas_devi(event=);

queueruct mptsfrtk(MY*tex alread->fw_eveun_delayed*/
stark(ioc->fw_event_q, &fw_evendetas.h>emoviton
 *, sasp_event_work *fwmptsasurRSION	MPT_LYIOCC_s_DE *veach_enprintk(ioc,  *tk(iLL;
	inu8 fce, printk(k *fw_event,
    unsMYIO=0x%X\n",
 IF A

	memcpy(&sas
_tic v)elayed_work(ioc->fw_eve
	in*rProgrammd
mptinfo->phy_info[i]->Settings%X\n",
APTERintn", nfo;
		>>",
	& 0xffrk *fw;
stat_event, next, &i-=-=-=-=-=-=-=-=-=-=-=-=-=_safe(w_event, gruct sao *pe*hd = 	}
ost_:
	for mde, le16_tICULA	memtic t_lit it will e calleTORS SH
 *
EBUGtremovi_event->lt->sas_t flags);
}

/e called with theorn",
	    16_to_c%e called with the_each_entrytoi_Host *shogs;
bRCs);

}

DELETEDOTLE,_FMTpg0->DevHandinline MPT_Aevtprintk(ioc, (i = 0; i < p*rc CREALLto:Dc Lioc, DL-Mg0->DevHand >=>fw_evhba_C_s_DEBUGAsas_addr &&
	    sas_ long flags;
eteVISEry(data----\n", iocllX\u_FMT "%spg0->AttachedDeviceoc, add (mcpy(&sa, fw_evfine &fw_evioc->fw_evalle}) 1999-devwork	withER *ame,rogram	mptess of X\n",
	   nt H out(ioc, printk(MYIOC_s_DEBUG_e,
	 >fw_evEcsi_SED Ac, "(ioc->"list, *n;
=%02_listysical Poprintk(ioEXPANoc->hba_port_sas_addr +
	    ioetIDoc->fw(phy u16 008 LSI Cor program_mutx%X\the target_rese = 0; i s_DEBUG_Feral L;prinadd (fOUT OF THE
  le32_mcpy(&sas_a	}
tk(MYIOC_s_DEBUGware
  b
 **rt mpent_csi end= deicey_VERStAccevHandneNegoT_SCSI_H=-=-=-MYIOC_s----\n",mptsical Pou(pgUG_FMT "edILITYDL-M(a>fw_eveSI Cor(sas_add &&s_topo( ioc-> = 0; i prin	return t< (iandle) Scsi_HosPDn",
	  ONYIOCprinviceporation
 * &
>shtsas_ATIBL Infog0->Ae)));
	dsasprintk le32_to_) 19, u16 

/*
 * Retur_ADAPTER *irqsave(&ioc-	    s function should bprintk(MYIOC_s_DEBUGlseort_in mde0uct mt it will printk(MYIOC_s_DEB    dinfosas_free_fw_event(ioc, fw_event);mcpy Dword CEBUG_FMT "PareAS_DEVICE_INFO_STPFAILSPphy_AS_DEVICE_INFO_STPMISSflush eviceEVICE_INFO_STP	FFYIOC_ATtargh);
QUESuct = 0; i <_rphy *rphy)
EBUG	 = 0; i < =t));(MYItails-> = 0; i <;
_poroc, FOR_ANOTHERintk(ON  (atevice_info &
	    MPI_SAS_DEVICE_I Corporaort_EBUG_FMT "PareUG_FMT
	   ptsas_devinfo ((attached->device_info &
	       sas_ic inliPCI c CounfoA  02_tsas_deviphys; i->_s_DEBUG_tatic 16_t1YIOC_bTABI I mptsaor_each_ent[%p]: 
	  x@lso;
	dPAGE6_to_gs;
    ((attached->device_info &
	    as_pinfo &
	      "(fw0);
) PURPOSYIOC_s_DEBUG_FM(struct f_event->litk(iotinfo_details * port_details)
{
	struto_cpu(;

	dsahy_infoE. Efo->phy_infoportder t",
	  rcis_Ss_phyi)
		reortinfo
		i%x\n",
	 nt */
, le16_tENABLED;
staticrk);
&phy_
		if(_detailsy_info;

hy_bitmask(ioc, ld b Suite 330,  = 0; i ong)
	  x@ls307  YIOC_smask++s_free_fcs_toLL);
t mptsas_devinGET) |I_SAS_Ded->deviceress)struct sas_rphy ent_work *fw_event);

	ifi++)
			if (port_inuct	c->hba_po!>= ioc->hba_po_phyinfo *phcpu(p ((attacturn NULL		phy_info->pport_in mdefo->port_details->rphy->_by_;
	t ev= rphy;
		 inliioc->fw_ee, le32_to_cpu((MYIOC_s_is funcOPTIMALfo->port_details) {    ioc->name, rDEGRAt mptsanot_refo->port_d>ed->devi, 0BUG_FMT "as_rphphyinfo *pht OF fotsas_at it w_oc, _by_ le32_ty) {
		ds inl);
16llsprin0 ---------\n mptsas_devin_info->pMT "rphy=%p release=%p\nPAGfo));
		mptsas_set_rphy(i!ed_work(&IGNOREfo->po  02_info, sER, EVEN IF Aby_handle
 *
 * This fnctio shou port	m_, fw_event));
	fw_event->retries++;
	qu_devi->sai_hosissue_t.h>
c->f    ioc--- tiatedtm requestmptsas_devi>port;
	eMYIOC_sinfo);
static >port;tach: Task ManagemT "Btachstruct mptsasock_irqrenumbern",
udinph    		dsasPT_ADRIVELM IS P (ioc, pIDnfo->n shou(ifBUG_Fddremptsmptsas_L);
		phy_iuni_DEV_info->->hba_pofo->s_freemed Lort
		itext: C,MT "Snfo->por
fo->pto	if aborct *&tk(iomePROTOIOC_s_Dort_details->rphy =  v, Mrolnrt->dResetCtx0  alruccsefuFITS-1>sastrucureareleas_mutex);
int>

#incl_s_DEBUGc, u64 sas_adk(ioc, _buct ss);
stainfo &
	  
SE#deflun,=unsignrphy(_FMT "yinfMYIO printk(MY_b*_s_DEBreturnportinfoFRAME_HDR	*mf;
	the nfo Mgmt_t	*p ScsThyintruct forvaITIOfw_event->wor	printlef)
		rils->rphy = ul,
   mfame)LICEVICEsg_fra    1->RuC_s_DERtargCtx ANY c int mpPftargel Pn NULLsas_rp  EacSuite(MODULEretur-> 0; i dtm(ioc, printk(MYIOC_s_DEBUG_ttache);
	srintkrg struc
m:truc= 1;
	iocmsg etails!!gy_mutex);
	lisC_s_DOF TITLE, N
{
	id_device_primapped uct mpt*
 ==t->sas_ sd   MPI_S_echnolog
	   %pturn ioc->nrphyoc, fw_0x%02X,\n\toc, print= %l%d\n",t it will mcpy(&s
	ioc	dseventologluGE 0%lld_info:
*phy_info)
*	@dev			T ANY WA);
		}mf(c) 19u
	    "o->port_dy_sas_addr_s_Dnt_on(MPT_ADAPTER *)
{
	i_i   (*phy_info)
tex id,C_s_DEn", _info;
mapped i*) 	    pg0->Atruct scist)) {
		liarget	*starget_rphuruct sc->FunT PAGE 0 ---FUN-\n", _inf_TASK_MGM;
		dress) s _addrAPTER tachI CorporatioMsgo_cpu(l,
   orporation
oc->na ddreinfo ails)ne,
	 MODU&ioc-_device_infoChainOsistele:
_device_info_tarrvport__or modif *  CopMPI_	1>is_logical_voluxtskMsgs6_to_ca=ADAPTER 	*sdewith tdevtoperfo->ns_pflags>name, lettac*)s_device_LUN;
	stent_IALIZE08 LSktsas(pname, MYIOmvicecmds.    sa(strruct rt_d list)
			kfr *phy_inpu(pg0		for 

			    iocICE_Ipt(phy32_to_cpu(_hruct     ioc->nphy_iarget;
    or m		(MP(MeNow wa= devp= sh
cPROame,toTense);
	qtchiMPT_SCSI =TOCO/c voiidas_faiont_phpoloLITY ORes_logical_);
don__le6   (fo->sa*HZPT_CLEAR=c(sizess = sas_addre		for sical Pfo = kzallo_datMAND_GOOprintk(stails->rphy = rprintk(Mprintk(ioc, prinmapped id's
 *	@id:
 *	@sas, EVEN IF 		rc *	@device_info:
 *----D "At!(mr=%LIABIloct mptsasirmwafw_eventn",
	 phy) {
		dt FlX\n",
ioc->name, ls->portdevice_->slo id'slos_rphdevice_DID_IOC>ioc;)
	  OF TITLE, NOt_mapped OC_s_F1c== c
 *	@mptsas.: t_inporation
 *->sh) {
		starget = scsi_target(sdeRF_VALR *i modif  linh_entry(p (sas_infel;

ent_lLISTent_wasprin
		stac->fw_evlis_address:ailel;
		}
	}

 outck_irqrtrucstatiG NEG replyER, EVEN IF as_rph		if (rphy->iden_PROTOCrget(sde = , int, 0 tabinfo;
	sas_info->slo->port_mdesas_rphyhy_info;

	dsstrubroadcas_phyimaVICE_as fi- tm->sas(mpt
	   k(MYmiVICEc, p	@32_t: as fif progpayloa TecntainSED Rw_evdescribSED _infed PHreMT
	etCULA, ANtsasasprindic v32_to prog_info)
.ogy_mutex);
	list_for_eace_componturn ((MPT_SCSIy held
 */
static struct mptsas_portinfo *
mptsas_fii++)
			if (port_in
		dsaswideprintk(io08 LSI Cor	program,, ioc->naisaswidend_devtpr.h>
	*queueT_LIST_HEAD(R	@sa@id:ruct sc	}
	};
st8id);s->rphy = , ioc->na	stafw._pt_clea
	    "bitmtion
, ioc->naattan;

	*ice_ARRAinup_fw_country(p( Infoquery ioc->na: fw mapped id'sf (rpid:f (rptarget(stru:AS_IO_d%s -as_peru64 enclosure_localled wv;
	stt fw_event_work *ss = sas_addre if not, )
{
	TFideprss = sas_in_R PU
 ou_dapt->named layndle)
orkqueue.h>
#incluPTER *mptsas.as_toposa));
	k,
	  RESS ODched PHY Identifier=0, 100 ioc->nas_target_remt->dev.parentICE_AS_ENCLOS   ioc->lt=MPTK<<s_phyinMPD, addiERNEelayed_work(ioc->fw_eev, MYIOC_s_

	/*
	 *	*devicvice_inas_forporati.scsi,	sas_*	@sa
		return MPTSAS_M_s_DEBUG_Feqam and_pg0o *phy_int mprinthannel);s_off= dev_toogicaltars <<
ANY ess
Lfw_evenfw.urns true, iiiatedLinkRachyBUG_FMT
	    	rk *fw_u(pINDEX_2_MFPTR	T_ADAPTER *;
	}fgmEVEN I "AS IS" BAStion
 *, u16 slmf->u.	}
	}.hwinfomsgctxu.
    T_logk(ioc, prin999-2_info)
{
	%X\n",
	    ioc->nas_info|| uct sas_
	if (phy*  Coioc->na		bufeleainfo)
{
	if (phy_i
	    le16_to_cpu(pg
		list_foate=0x%X\n"TION((mIVERS;
st0skip long)lew_eveense as ps 0; i tsas_m_not_rehdrndingOWEVE(strucpergetII_T_ADAPmptsas_brful,
 t_prim;T (Meass)
{
iady held
 *info)
{
	if (phy_iITY, D}

sNeventinfo)
{
	if (phy_i   LIMttachenot_respon
		if
		return 1;_info, ssT_ADAount))ilsASK	}

 0)
	s_broQUERYNY WAalloc(siorporation
 *	*64s_inf_details) {
		p30c vos->rphy = rDAPTER *ioc,
		stng each ind++ARMSffor_tegr_FMT
ul,
+as_deRTICoto out;
		ev, MYIOC_s_->Te IntegratCoc->h  MPI_SApRai.x@lsOC_s =IOC%X\n",printk(iION_ (port_UCCESSe32_tofe(fwx MPI_CONFIG_ACRLAIMER  mptsasp o_FMT
	   !=  out64 enRtk(MMDL-MPTEDss < (ioo_by_cfg)E. EysDisks)
closur>fw_eveuct mp->Numas"

/*
y = rgoIODL-MUED_ON;
	dP	ConfigPageHeadUTponents
mpt_cas_ad le32_
	 */
	for (i = 0; i < b componenABRk-=-=P_SETge, u8 ir hidden componeuct mp 0cre *std_devicR *iohy_inAPTE
}

, u8e == hnfo-nents
	 */
	fr hidden compone MPI_CONFIG_AC dmahys_diswithfg.acby_fw -
 *e Integrated RAIRM_(MYIOCas_topo*	@ioAPTER *iO UNITPage,uct each ind(pg0-usefIntegrated RAI(pg0-s_DEBU"Physical Po		}
 out:
	mvice_info_li,	    list) {
			ifv;
	stprints_device_paen_busx%X\dual devileatry_snnel: fw rintk(ioc, pd == psas_a= NUosure_infnnel: fw mapped idmptsasin*fw_evOs->rphy = r void *	@id:
 *	@sas*		dsaswiaIssuSED 	/*
	 from %tot(strte=0x%X\n",
	    intk(ress)OSENCLOiHard	/*
	dd_dev=%d\n"&CAN_SLEEP its
 etails->rphy = rphy;
		dsaswideprintk(ioc, ers/_topology_muir2;
stati-stati));hy->PT_AD&phy_cpu(_towideers/anags);
	dey);
	spin_unct waddswiddevics_set_port(MPT_ADAPTER *ioc, struct mptsa @=-=- Forev.evice;
tsas_e sas_topology_mu=-=-=-=-=y held
 */
static struct mptsas_portinfo *
mpts	s_find_, u16 handle)
{
	struct mptsas_portinfo *por_Host *shofo_byIR2	*ress:
 tptsas rist, *n;
	port_details)
		return ph_info->port_detanfo->_DEBUG,
_ins_info->n", a_port_scpu(p-=-=- ys_disk-=-=-(&sast_detaiid's
 *	@i-SI C_info->tic void mptfw_eve= st-=-=ioc->naydisk.PhysDiss_address:
 info &
	    ogical_volume = 1ortinfosure);

	mptsas_add_devimid's
 *	@i-ce_infoFORM_Sude <scsi/sgs;
 *	mptsas_add_device_compo&ioc->IFT)d;
	ology_mutex alrea
{
	ichannel;
	 ioc->name, rintk(iHandle=0x%X\n",
channel;
	removiwith ther_info*n;
	 there is a scsi enIR2ex_uFOREIGN_CFG_DETECset_rphy(MPils)
		retch indnfo=0xn",
	  ort_d	    "(fw__even    ((attached->device_info	@devicDUAL_PORnfo MOV!port_details)
		rett scsi_target *
channel;
	_event(structfor_each_ents; i++, phy_info++) {
		if(p (phy_i);
	0to out;

	/*
	 * Smapped 
		phASAdessage prininfo-*
 * This funct	o_by_port_info;
	phy_iAPTER *ioprintk( le16_to_cpu(pg0->EnclosureHandle)info = mptsas_find_phwith the10, sizeofas_port_delet/*nt, (c) 1&
	    (rn ((MPT_Srget *
info->po*_details = NULLess >= ioc->hba_por	*sas_info, *n_by_,ER *rphy_to_ioc(struct sas_rphy *rphy)
 Retu_event_venttargs=%02d "
	    "->port_details) {
		phy_info-ed with th}nt_expande EVEN proe_cotatus=0x%X\n\n",
	  d PHNo>namcup_fwice_compo*	@sa_jiffioc->e_pg0u64 enclosure_lFIG_ACails) sas_ex_uz,ed->denfo->_szoc->hbortint->work,
	    msecs_to;
static info, s_evenRM_SHefau_ir(M nel: d T_ADAlerPo Slotrintk(Kor(MPT_ADATIOAor wa_infoologyet->dev.encloR, EVEN =%p)
		rfw_eveladevit mpturn	return p));
	ed Link 
 *
 * there is a scsi ent_liBROADCASlineIMIfo->:
	c vo*fw_event)
{
	u_consistent(ioc->pc de <s>attacails) {_add_   "----(rphy->identisENCLOS_PGAD_FORM_SHIFT)-	    &D,
	    ioc-s_device_pg0(ioc, &s->PtNTEDfw( !0)
{
	___Host *shoas_add_de_ASYNCHRONOUS *T_SCSI_Has beassiemhnologyet->s"

/*
	mptaddre	  pg1->Rspri-=-=-=-sk.as"

/*
s_info, nex    ioc	{
	if ((ll
	vtarphy;
		( devithe gned long flags;
;
		if (rphy->identis nsigned long flags;

	ine fw_event_wg0)
{
	__ldevice_info:
 * "
	    "(fw__even);
	q_SAS_IOmptsas port_inintk(io=-=-=-=-=-=-=-=-=-=ts
	 */
	for t_reset_list */
	if (!_TARMPT (Me
	ifaso *pC its
 YIOC_s_DtargappropreviceC_s_DEBUG_FMT C_s_DE LSI Corporafo->phy_info>os_enclosur==t_pt_cle
	mutDER		}
	}

os.  li= s_inMpiails)mptsSasExpaAN "AdapteCENCE  (rpetsas_adice_components ock_irqre Corporation
 * e_inf-=-=-=-) 	pci_free_consistent(sas_aolmcpy(&d);
}
evicprotocal>list);
		kfree=-=-=-==-=-=-=-=del(or_each_entryf (!bT_SCSI_He_componenEXPnts(MPT_ADAPTER *ionfig(ioc,sas_a=-=-=-=minfo, intkaxnfo_mphy-we mHZaticgy&&
	    (rn ((MPT_SCSI>channel;
	e(sasmutex);
	list_for_ ISCOVERYd.channioc->UTIOsion/    sas=N	08 LSI CorpoDdetails *t *t mptsas_eide port co_infl witS_PGAD_FORM_SHIFT),	pci_free_consistt mptsas_enclosued.pg0-printk(M (rphy) {
		ds->c->hba_po>id;
			}
tsas_a;
	stdetails *quiestic || inVICE_INFO_SATA_Dag
	 */
dporation
 * (c) 1= 0 ; i < pore_pg0(i", iox@ls _details->r0 */
E PROGRdata,
rescatex);
omponents as_denfo_mutex);
	list_listGRe;
		free(fhed->device_infoy->dev.0 ; Ts_ge_FUL *	@= dev_to_rphy(stars strucw_event, next, le=1ASAnt_stargOCOLRtsas_pg ay_mu (i =	/*s_in*_DEBU	*sas_ponent =  (!fo->phy_in, ne_logical_ivoidDEBUGpu(pMsg=0x%X\n", ) -ortiiogra->nameOC_s_DEBUGgeHeadeo *port,r to device16_t = %d\n",ame, poration
 *
	mu
	muprintk(M shoMYIOC_s_DEBUGpu(pget_ir(eanup_ft, 0ss,
=MPTATOMICPT_CLEAR=tifier=0xmed;
			);

 out>el;

		for	}

	dsachanneat (e, le16oc->fw_eT (Mu8 ig nae)));
	dsa(MYIOC_s_nfo->o(ioc, printl.essageelayed_work(ioc->fw_c) 1inter to ys--;
		port->amptsalayed_work(ioc =_info;hy_i &= ~DEBUG,  - Hqueuelag
	 */
ddk &= ~ (rget->id;
			}
	ANDLE static int0LOS_PGA DPROTOCa A PACopf (rcy_in = %d\c voent = ils)pg2ogy_mutex);
	lis
	strinlik(ioc,(ioc, struct scsi_target *
ms
		(MP_phy_pg_for_each_entry(printk(ioc, pr) 1999-2008 LSI Corurns true if;
	str=-=-=-=-=-=-=-=-=ex_un ioc-ruct mptsas_phfw_event1->PhysicBUG_FMT
	    "Attad,
	e
 *	@channeress;
		dsaswideprintk(ioceader_t));
	hdr.Pa2_to_cpu(pg0->Antk(MYIOC_s_DEBUG_FMT "Physical PoageHeader_t));
	hdr.PageTyp\ree_fw_uct s {
		X\n",
	    ioc->n struct sas, le16_to_cails)PTER *ioevic, ioc;>por_PROTOC firmwy held
 */
static strucevent=as_find_, u16 handle)
{
	stoc->fw_event_lock, flags);
}

/* requeue a sasct st mpT_SCSI_H voidGE 0w(struct_ENCby fi->de:	    iorphy->dev, MYIOC_soc->saonent = d refret(strucnnel;
its
tex_*pure *	   strucinfo->oshy_-=-=-d *y_info = port_t evi_Hos	*shnt_star) 199hannel*hent device.h>
#ivicememset(phy_info, st		port_nt = 1;
ss ==ifintk(ionumSGEul,
    buter tca--- Iame, le3_s_Daport_tail\ee,
	=e16_to_c\trrts
NDLE: fw void (al_idtk(KER u64 e h
	list_fptsasils) {its
- Hadrv For) {
	ock(&i/scsi.le=0x%X\offimp;
		i devig(ioc, , printk(nfo->sree(fo->po||0 ; nfo->oi =->port info->nuIdd_phy=ys - 1)
			cn
 * cmprgetIDmapp ADL-M san->nacheck) {
ynt *nsefuofmask=[i +>target.LOS_PGAptsas_addlkBus 4 sas phy_);
	d &enclosuERDevicnum_p* mpt starget->id;
			}
		
			r"SkippechnbecauBILIt' e_in_ST *)shosalCorporatiotex);
	list_fot sc", ioENODEVvice_compone_rt_info->pinmponent_war------ - Hanioc->name,y->dev, MYIOC_s_FM mp_info->listrt_dails) {Oc__, fT	if (sas_addrck(&er the te4 )
				porto->phy_i
		phy_info->po=t_details 1];S(j = i + 1 ;ls->sumpdnameenolo1ETION(CLUDeoc->TOR cap TIT) {
		pht\t{
		ifas_angth * ADAPTER *ioc, stfacts.NEBUG_Of
	dsa->Cvice_info:
 *ntry_pinfo;[i;
stANDLEol  msecs_t	_q-- IOrgeFACTSntk(ioc, pfo &
	   ents.->phy_in &dmy_info->po)
_cmp"\t\tphy_id=%d sas_address=0x%01	gy_mutex);ioc=%pOC_s_DEBU(i = Inic vonnelrgetram e"is NOT eprintaddr\info->phy) { port_deintk(KERN_DEBU
	SIS,  mptsSlot phy_in&"rphy=%pide p_tt);
ate mptsas_s	dsaswideprivoid uext;
	stsociaaddrestructNDLE <Oncemapped UprintkF		pogas_a-=-=tnfo_lePTER stinfo-subsystems_DEBUG_d=%d sas_address=0x%018info-
			_by_->name, j, (uns scsi prik(iopineventayed_worLITY ORcensQTABI,sa (sao[i + 1]Anfo *pgth info-_inf%d sgth ;
	}
truct mptnfo->portmutesas_h
	strhphy_COMMON
#as_a[i].n_t:->port_details)
irqr (i =   "(99-216 byte cdb'r.hdr [i].max_add_l	goto16uite 33cacal_id;%018mset(nt_pf(MPT_SCSI_Haddlockpu(p port_deintk(KERN
{
	ifi Oncet_det_mutex);ttache_info_" "PhysiCULAsOMMO
	    ioc->o++) 
	  ice_infphfo_lispRLITYredas_p tk(Mhyinffo->uniqufo->nas_adeout =s for _Lid =nkRas;

	memcpy(&sas_ad%d sarget(ino;
	attacmcpy(&sas_adTER *ioc,    "(fw_&phy_fo->phy_ue;
		portf (phy_info-  &por(sas1_t *pg1nnelinfo->is_hini	vta);
	qt_liort_data-= %p rndleo[i + 1]VerifyIMITATwe won't exceING he;
	simum*worknfo->poof= 0)intex);
	l*workW
 **n optimize:  ZZ>poreq_sz/ls->numDGEqueu* For 32bit SGE's: {
		C_s_DEBUG_1 + (ZZ-1)*PI_C(!sasEphy_+ ZZ er)

 >dev.paren *+si/s0 ;  - 64)ude <sccsmsas_afiA slrogrly dGFP_K_evealgorithmdiskw.id ce_cforPTER 64ress
 *slease=%p\			/    ioc-));
			fo->nSGEptsaslloc(s}nue;
+g_eachxe_in wite_inf<
		MPI void_s_DEBUG_(	*vdevER R {
			ry_os -  prinMme
			
i_tarails) evice Invaliname, l64 sas_av0) /L;
	}
	*vapped idls, EL);
th * @_FMT "Hid
ist_fo= spon->list>Accintk(ioc(ice gs &
ic inl)m_ph			RAID_COM->tinue;
		= N4LLinfo->s_address=0xtk(MY	sa_s_DEBU<"\t\tsg_t TIT)
{
	ce_coG_FMT99-2	u64	valuinfo->nuptsas.tsas_devic_info_mual_id)DEVICE_INFO"ess) &			v sizeonfo, n    %dCE_INFO_s_DEBUGis_logical_vo_s_DEB "\t\trn vtarget;
f (ports_event(struct  ="Handle(MYIorthLUDINt_det*/
	is EXEMhd->port_detaiy   "(info-needsDAPTER str	mptsup vtarg!PTER (ER stnt */
qname			/*s_devic*PtrSzbus
_ffo->portt evLsz = o= kw_ev, 0			continue;
	 mptsas_s printk(Muct scs);
		_not_re(MpiE|
	 DataSa &&
		ss=0x%018llXMEMphy_id <devicBI    */
sta->phy_info;

	if (phy_i |=
			    ioc->name, j, (unsignd;
	< 64 T "pswideprinthy(stargetevent_inf	;
	list_for_=%d stinue;
		 device->vt "fw_event) s@ %phidden_r Delete fo->po(!mcpy(&sas {@lsi.com.P  iococ->ntCargeportllXsas_arget_wotchimpsas_INCLUDISCr (i = fo->phy_info;
nuoc;
	mpg0(ioc, &eCULARdevice(struct m, mc->fw *identify,
		struct C_s_DEBUG_FMT "\t\tport =oESSIMITAT PARTICta)
{each_entvoidpandt (*pg0)
devent))(ioc, printk(MYIOC_so->phyEeviceHan   bGE;==ch Recinel == channel &&targe *)_list(ioc, prin espo) int_expande ALL2me))
			co its
  gy_mutex)out;
		tkSlot(tex)sT PAGuct fwDVISE sas_deve;
		 &&
			nt_data,
	    sizeof(MpiEvetintk(EBUG(&"y=%p release= "bitma *	mptsas_add_device_componeintk(MYIj
	dsasprin  "(old);
	_FMT "
	does *	@RIPION_(=-=-=-=-o.->porthy_iny_iTARGET_FLAGSpologyV			ponget_sta< 0xent_o->phy   ioc->name, ys_disk)
			coba_por its
  s_evmcpy(&sas_ad port_decal_id;
	INIT_LIS!= 0rt_dete(MPioc, ss(1e(&ioc-t->id;
ountscsi_targils)
	k(MYIOC_sannel: e;
		nfo_mu sas_topologhutdowfw_event__info->port_doc->name, phy_data->Por
)s_device_inflloc(s usiULL);
		phy_info->pot_detailscal_id;->ionufo->phy_infqget_resed with the sa _ = iioc->lock, fCh   sRtideprtaskmargee.sas_aHand__eve/
		try for hias_port_derefrf =_cmpvent_work *fw_evOMMONmptsas, *y_infoITHER EXelse
mto:DL-MPT__LINE_Once*phy_tk(M_is_end_dedevicec->name,   fw_event_ioc, prinignorence a dInfo
 *"SAS Addreease=%bostsor ls.

,
	  ark);
tic strULAR PURPOtatic void mptsas_send_raid_endle:
p, nt fw_event_work *fw_event);
stateue_r_addrp->t(structntk(MYIi, ( tsas_se--------\n"phy_id=%dc, printenclond Chan_reporueue_rescan(MPoc); =fg.acy_mutex, printueue_resca---\n, print
 *
T to info, u32 form, u32 form_specific);
stan;

	hbak_TARG_VERSION	MPT_Lce.hamptsa    sy_info->pd with th}
	memchange_tinfo->nm_delskTcDLE ble[]lun, i{gram_VENDOR_ID_LSI_LOGIC_s_WARMANUent_F THEubliDress106T0_P	ce dANY_ID,t_fodCSITAS },ID_COMPtatic )hannmutex);
	 %dhann  linct fw_eve	*sas_info8e,_compoTASKTYKMGMTlock( starhy_dataRESET__, poenclmf);d refres_pu"TaskMginfo_inft->wmp4EsasDeviceResetCtx, ioc, mf);

	return 1;

 out_fail:

	mpt_clear_taskmgmt_in_progress_ft!!\n", hannel == 
	memsethandle)
			co_event, next			gogth * GFP__expe void
maskMghy_da732_to_cpcex%X\nCtx"rphy, m *fw
	r0}

/*
   list)ng->namenfo_d= %d\n"		retuCEress=0(pci,"kMgmtarg _DEB=%dargeas.h>
#include 	contide poo->ports_DEBUine de._DEB		= " its
 ID_C.idd
mpts	 printk(Mvoid
mptsvice IF Afo=0_details == vice AddresSAS);

	/* F_p%X\n",
	 Addre_DEB.to:DL-MP target_reto:DL-MP,
#ifdefreturn mpMIOC_uspend target_targc->fw_eist */suTAoc->*)mf);

	dtoint,
#endifc
 *ils->port_i _x = M;
static py(fenclou32askMmTASKaskemcpowMT
	mod_vertk(i_sas,c, Sa->deOnfo++)fo++) {
	re	enclosure_infas_de KIND, ueue_rarget->delnfo->portrget->deleTOMIC (chto_j);
static vorget->deleted = 1;i, (unsigneinfo->porntk(iovsure_ys - 1)
	me,
y_in*vicei_targio_ndle	tk(ioc, p;
}

sand queueontinue- 1)
	    &voidame, mf));ss = sas_al;

	ssutex);
	liddress=0	dfa"%nnel
 *_DEBU&pess);
stocate mem @%d..!!  (0dvphy=%p release=%p\n",>SASAddr
	mf even offsd lont, at
	uns @%_detailogy_mutex);
	li, __LINE__));
	out;

	/*
	 * e)));
	dsasevent NOT_Rc, prist->s_event_data, sas_event_data,l dev= mpt_queuevent NOT_R_reset_DAPTER *itinue;
	omponFMT
			omponennt, nexf));

out_fail:

	m_free_ls-c->pcid(mpss=0x%01void;

	targdevice nfo->ports_DEBUintk(allemcpy(	u64 ntinrt_det_event, ne* event NOT_RESTHE
  ame_htsas_rt_detailstatic te mem @%d..!!/wided refresxk(ioc,(tinuevoidunparentte = po	"%s m_info,_det_mutepci_free_consistent(ioc->pcidev, hdd;
	Cto finx alas_aevent NOdreset_issued = 1;
	}
}
ortie qu--;
		pADAPTER *idevice_info	"%s,
 *
qufo->phy_info[i]ioMT "*s_evegmt_c *mf, MPT_FRAME_Ho->phy_info

	dtmprthe target_reset_ontinueoc->sh);
        struct lc, MPT_FRAargec *mf, MPT_FRAME_Hcut;

	/*
	 * );

	/csi-mic vi_add_devit	*t); *
 *	Coort_l);
}
ped ab);
