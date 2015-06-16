/*
 *  linux/drivers/message/fusion/mptbase.c
 *      This is the Fusion MPT base driver which supports multiple
 *      (SCSI + LAN) specialized protocol drivers.
 *      For use with LSI PCI chip/adapter(s)
 *      running LSI Fusion MPT (Message Passing Technology) firmware.
 *
 *  Copyright (c) 1999-2008 LSI Corporation
 *  (mailto:DL-MPTFusionLinux@lsi.com)
 *
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kdev_t.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/interrupt.h>		/* needed for in_interrupt() proto */
#include <linux/dma-mapping.h>
#include <asm/io.h>
#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif

#include "mptbase.h"
#include "lsi/mpi_log_fc.h"

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
#define my_NAME		"Fusion MPT base driver"
#define my_VERSION	MPT_LINUX_VERSION_COMMON
#define MYNAM		"mptbase"

MODULE_AUTHOR(MODULEAUTHOR);
MODULE_DESCRIPTION(my_NAME);
MODULE_LICENSE("GPL");
MODULE_VERSION(my_VERSION);

/*
 *  cmd line parameters
 */

static int mpt_msi_enable_spi;
module_param(mpt_msi_enable_spi, int, 0);
MODULE_PARM_DESC(mpt_msi_enable_spi, " Enable MSI Support for SPI \
		controllers (default=0)");

static int mpt_msi_enable_fc;
module_param(mpt_msi_enable_fc, int, 0);
MODULE_PARM_DESC(mpt_msi_enable_fc, " Enable MSI Support for FC \
		controllers (default=0)");

static int mpt_msi_enable_sas;
module_param(mpt_msi_enable_sas, int, 0);
MODULE_PARM_DESC(mpt_msi_enable_sas, " Enable MSI Support for SAS \
		controllers (default=0)");


static int mpt_channel_mapping;
module_param(mpt_channel_mapping, int, 0);
MODULE_PARM_DESC(mpt_channel_mapping, " Mapping id's to channels (default=0)");

static int mpt_debug_level;
static int mpt_set_debug_level(const char *val, struct kernel_param *kp);
module_param_call(mpt_debug_level, mpt_set_debug_level, param_get_int,
		  &mpt_debug_level, 0600);
MODULE_PARM_DESC(mpt_debug_level, " debug level - refer to mptdebug.h \
	- (default=0)");

int mpt_fwfault_debug;
EXPORT_SYMBOL(mpt_fwfault_debug);
module_param_call(mpt_fwfault_debug, param_set_int, param_get_int,
	  &mpt_fwfault_debug, 0600);
MODULE_PARM_DESC(mpt_fwfault_debug, "Enable detection of Firmware fault"
	" and halt Firmware on fault - (default=0)");



#ifdef MFCNT
static int mfcounter = 0;
#define PRINT_MF_COUNT 20000
#endif

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *  Public data...
 */

static struct proc_dir_entry *mpt_proc_root_dir;

#define WHOINIT_UNKNOWN		0xAA

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *  Private data...
 */
					/* Adapter link list */
LIST_HEAD(ioc_list);
					/* Callback lookup table */
static MPT_CALLBACK		 MptCallbacks[MPT_MAX_PROTOCOL_DRIVERS];
					/* Protocol driver class lookup table */
static int			 MptDriverClass[MPT_MAX_PROTOCOL_DRIVERS];
					/* Event handler lookup table */
static MPT_EVHANDLER		 MptEvHandlers[MPT_MAX_PROTOCOL_DRIVERS];
					/* Reset handler lookup table */
static MPT_RESETHANDLER		 MptResetHandlers[MPT_MAX_PROTOCOL_DRIVERS];
static struct mpt_pci_driver 	*MptDeviceDriverHandlers[MPT_MAX_PROTOCOL_DRIVERS];


/*
 *  Driver Callback Index's
 */
static u8 mpt_base_index = MPT_MAX_PROTOCOL_DRIVERS;
static u8 last_drv_idx;

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *  Forward protos...
 */
static irqreturn_t mpt_interrupt(int irq, void *bus_id);
static int	mptbase_reply(MPT_ADAPTER *ioc, MPT_FRAME_HDR *req,
		MPT_FRAME_HDR *reply);
static int	mpt_handshake_req_reply_wait(MPT_ADAPTER *ioc, int reqBytes,
			u32 *req, int replyBytes, u16 *u16reply, int maxwait,
			int sleepFlag);
static int	mpt_do_ioc_recovery(MPT_ADAPTER *ioc, u32 reason, int sleepFlag);
static void	mpt_detect_bound_ports(MPT_ADAPTER *ioc, struct pci_dev *pdev);
static void	mpt_adapter_disable(MPT_ADAPTER *ioc);
static void	mpt_adapter_dispose(MPT_ADAPTER *ioc);

static void	MptDisplayIocCapabilities(MPT_ADAPTER *ioc);
static int	MakeIocReady(MPT_ADAPTER *ioc, int force, int sleepFlag);
static int	GetIocFacts(MPT_ADAPTER *ioc, int sleepFlag, int reason);
static int	GetPortFacts(MPT_ADAPTER *ioc, int portnum, int sleepFlag);
static int	SendIocInit(MPT_ADAPTER *ioc, int sleepFlag);
static int	SendPortEnable(MPT_ADAPTER *ioc, int portnum, int sleepFlag);
static int	mpt_do_upload(MPT_ADAPTER *ioc, int sleepFlag);
static int	mpt_downloadboot(MPT_ADAPTER *ioc, MpiFwHeader_t *pFwHeader, int sleepFlag);
static int	mpt_diag_reset(MPT_ADAPTER *ioc, int ignore, int sleepFlag);
static int	KickStart(MPT_ADAPTER *ioc, int ignore, int sleepFlag);
static int	SendIocReset(MPT_ADAPTER *ioc, u8 reset_type, int sleepFlag);
static int	PrimeIocFifos(MPT_ADAPTER *ioc);
static int	WaitForDoorbellAck(MPT_ADAPTER *ioc, int howlong, int sleepFlag);
static int	WaitForDoorbellInt(MPT_ADAPTER *ioc, int howlong, int sleepFlag);
static int	WaitForDoorbellReply(MPT_ADAPTER *ioc, int howlong, int sleepFlag);
static int	GetLanConfigPages(MPT_ADAPTER *ioc);
static int	GetIoUnitPage2(MPT_ADAPTER *ioc);
int		mptbase_sas_persist_operation(MPT_ADAPTER *ioc, u8 persist_opcode);
static int	mpt_GetScsiPortSettings(MPT_ADAPTER *ioc, int portnum);
static int	mpt_readScsiDevicePageHeaders(MPT_ADAPTER *ioc, int portnum);
static void 	mpt_read_ioc_pg_1(MPT_ADAPTER *ioc);
static void 	mpt_read_ioc_pg_4(MPT_ADAPTER *ioc);
static void	mpt_get_manufacturing_pg_0(MPT_ADAPTER *ioc);
static int	SendEventNotification(MPT_ADAPTER *ioc, u8 EvSwitch,
	int sleepFlag);
static int	SendEventAck(MPT_ADAPTER *ioc, EventNotificationReply_t *evnp);
static int	mpt_host_page_access_control(MPT_ADAPTER *ioc, u8 access_control_value, int sleepFlag);
static int	mpt_host_page_alloc(MPT_ADAPTER *ioc, pIOCInit_t ioc_init);

#ifdef CONFIG_PROC_FS
static int	procmpt_summary_read(char *buf, char **start, off_t offset,
				int request, int *eof, void *data);
static int	procmpt_version_read(char *buf, char **start, off_t offset,
				int request, int *eof, void *data);
static int	procmpt_iocinfo_read(char *buf, char **start, off_t offset,
				int request, int *eof, void *data);
#endif
static void	mpt_get_fw_exp_ver(char *buf, MPT_ADAPTER *ioc);

static int	ProcessEventNotification(MPT_ADAPTER *ioc,
		EventNotificationReply_t *evReply, int *evHandlers);
static void	mpt_iocstatus_info(MPT_ADAPTER *ioc, u32 ioc_status, MPT_FRAME_HDR *mf);
static void	mpt_fc_log_info(MPT_ADAPTER *ioc, u32 log_info);
static void	mpt_spi_log_info(MPT_ADAPTER *ioc, u32 log_info);
static void	mpt_sas_log_info(MPT_ADAPTER *ioc, u32 log_info);
static int	mpt_read_ioc_pg_3(MPT_ADAPTER *ioc);
static void	mpt_inactive_raid_list_free(MPT_ADAPTER *ioc);

/* module entry point */
static int  __init    fusion_init  (void);
static void __exit    fusion_exit  (void);

#define CHIPREG_READ32(addr) 		readl_relaxed(addr)
#define CHIPREG_READ32_dmasync(addr)	readl(addr)
#define CHIPREG_WRITE32(addr,val) 	writel(val, addr)
#define CHIPREG_PIO_WRITE32(addr,val)	outl(val, (unsigned long)addr)
#define CHIPREG_PIO_READ32(addr) 	inl((unsigned long)addr)

static void
pci_disable_io_access(struct pci_dev *pdev)
{
	u16 command_reg;

	pci_read_config_word(pdev, PCI_COMMAND, &command_reg);
	command_reg &= ~1;
	pci_write_config_word(pdev, PCI_COMMAND, command_reg);
}

static void
pci_enable_io_access(struct pci_dev *pdev)
{
	u16 command_reg;

	pci_read_config_word(pdev, PCI_COMMAND, &command_reg);
	command_reg |= 1;
	pci_write_config_word(pdev, PCI_COMMAND, command_reg);
}

static int mpt_set_debug_level(const char *val, struct kernel_param *kp)
{
	int ret = param_set_int(val, kp);
	MPT_ADAPTER *ioc;

	if (ret)
		return ret;

	list_for_each_entry(ioc, &ioc_list, list)
		ioc->debug_level = mpt_debug_level;
	return 0;
}

/**
 *	mpt_get_cb_idx - obtain cb_idx for registered driver
 *	@dclass: class driver enum
 *
 *	Returns cb_idx, or zero means it wasn't found
 **/
static u8
mpt_get_cb_idx(MPT_DRIVER_CLASS dclass)
{
	u8 cb_idx;

	for (cb_idx = MPT_MAX_PROTOCOL_DRIVERS-1; cb_idx; cb_idx--)
		if (MptDriverClass[cb_idx] == dclass)
			return cb_idx;
	return 0;
}

/**
 * mpt_is_discovery_complete - determine if discovery has completed
 * @ioc: per adatper instance
 *
 * Returns 1 when discovery completed, else zero.
 */
static int
mpt_is_discovery_complete(MPT_ADAPTER *ioc)
{
	ConfigExtendedPageHeader_t hdr;
	CONFIGPARMS cfg;
	SasIOUnitPage0_t *buffer;
	dma_addr_t dma_handle;
	int rc = 0;

	memset(&hdr, 0, sizeof(ConfigExtendedPageHeader_t));
	memset(&cfg, 0, sizeof(CONFIGPARMS));
	hdr.PageVersion = MPI_SASIOUNITPAGE0_PAGEVERSION;
	hdr.PageType = MPI_CONFIG_PAGETYPE_EXTENDED;
	hdr.ExtPageType = MPI_CONFIG_EXTPAGETYPE_SAS_IO_UNIT;
	cfg.cfghdr.ehdr = &hdr;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;

	if ((mpt_config(ioc, &cfg)))
		goto out;
	if (!hdr.ExtPageLength)
		goto out;

	buffer = pci_alloc_consistent(ioc->pcidev, hdr.ExtPageLength * 4,
	    &dma_handle);
	if (!buffer)
		goto out;

	cfg.physAddr = dma_handle;
	cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;

	if ((mpt_config(ioc, &cfg)))
		goto out_free_consistent;

	if (!(buffer->PhyData[0].PortFlags &
	    MPI_SAS_IOUNIT0_PORT_FLAGS_DISCOVERY_IN_PROGRESS))
		rc = 1;

 out_free_consistent:
	pci_free_consistent(ioc->pcidev, hdr.ExtPageLength * 4,
	    buffer, dma_handle);
 out:
	return rc;
}

/**
 *	mpt_fault_reset_work - work performed on workq after ioc fault
 *	@work: input argument, used to derive ioc
 *
**/
static void
mpt_fault_reset_work(struct work_struct *work)
{
	MPT_ADAPTER	*ioc =
	    container_of(work, MPT_ADAPTER, fault_reset_work.work);
	u32		 ioc_raw_state;
	int		 rc;
	unsigned long	 flags;

	if (ioc->ioc_reset_in_progress || !ioc->active)
		goto out;

	ioc_raw_state = mpt_GetIocState(ioc, 0);
	if ((ioc_raw_state & MPI_IOC_STATE_MASK) == MPI_IOC_STATE_FAULT) {
		printk(MYIOC_s_WARN_FMT "IOC is in FAULT state (%04xh)!!!\n",
		       ioc->name, ioc_raw_state & MPI_DOORBELL_DATA_MASK);
		printk(MYIOC_s_WARN_FMT "Issuing HardReset from %s!!\n",
		       ioc->name, __func__);
		rc = mpt_HardResetHandler(ioc, CAN_SLEEP);
		printk(MYIOC_s_WARN_FMT "%s: HardReset: %s\n", ioc->name,
		       __func__, (rc == 0) ? "success" : "failed");
		ioc_raw_state = mpt_GetIocState(ioc, 0);
		if ((ioc_raw_state & MPI_IOC_STATE_MASK) == MPI_IOC_STATE_FAULT)
			printk(MYIOC_s_WARN_FMT "IOC is in FAULT state after "
			    "reset (%04xh)\n", ioc->name, ioc_raw_state &
			    MPI_DOORBELL_DATA_MASK);
	} else if (ioc->bus_type == SAS && ioc->sas_discovery_quiesce_io) {
		if ((mpt_is_discovery_complete(ioc))) {
			devtprintk(ioc, printk(MYIOC_s_DEBUG_FMT "clearing "
			    "discovery_quiesce_io flag\n", ioc->name));
			ioc->sas_discovery_quiesce_io = 0;
		}
	}

 out:
	/*
	 * Take turns polling alternate controller
	 */
	if (ioc->alt_ioc)
		ioc = ioc->alt_ioc;

	/* rearm the timer */
	spin_lock_irqsave(&ioc->taskmgmt_lock, flags);
	if (ioc->reset_work_q)
		queue_delayed_work(ioc->reset_work_q, &ioc->fault_reset_work,
			msecs_to_jiffies(MPT_POLLING_INTERVAL));
	spin_unlock_irqrestore(&ioc->taskmgmt_lock, flags);
}


/*
 *  Process turbo (context) reply...
 */
static void
mpt_turbo_reply(MPT_ADAPTER *ioc, u32 pa)
{
	MPT_FRAME_HDR *mf = NULL;
	MPT_FRAME_HDR *mr = NULL;
	u16 req_idx = 0;
	u8 cb_idx;

	dmfprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Got TURBO reply req_idx=%08x\n",
				ioc->name, pa));

	switch (pa >> MPI_CONTEXT_REPLY_TYPE_SHIFT) {
	case MPI_CONTEXT_REPLY_TYPE_SCSI_INIT:
		req_idx = pa & 0x0000FFFF;
		cb_idx = (pa & 0x00FF0000) >> 16;
		mf = MPT_INDEX_2_MFPTR(ioc, req_idx);
		break;
	case MPI_CONTEXT_REPLY_TYPE_LAN:
		cb_idx = mpt_get_cb_idx(MPTLAN_DRIVER);
		/*
		 *  Blind set of mf to NULL here was fatal
		 *  after lan_reply says "freeme"
		 *  Fix sort of combined with an optimization here;
		 *  added explicit check for case where lan_reply
		 *  was just returning 1 and doing nothing else.
		 *  For this case skip the callback, but set up
		 *  proper mf value first here:-)
		 */
		if ((pa & 0x58000000) == 0x58000000) {
			req_idx = pa & 0x0000FFFF;
			mf = MPT_INDEX_2_MFPTR(ioc, req_idx);
			mpt_free_msg_frame(ioc, mf);
			mb();
			return;
			break;
		}
		mr = (MPT_FRAME_HDR *) CAST_U32_TO_PTR(pa);
		break;
	case MPI_CONTEXT_REPLY_TYPE_SCSI_TARGET:
		cb_idx = mpt_get_cb_idx(MPTSTM_DRIVER);
		mr = (MPT_FRAME_HDR *) CAST_U32_TO_PTR(pa);
		break;
	default:
		cb_idx = 0;
		BUG();
	}

	/*  Check for (valid) IO callback!  */
	if (!cb_idx || cb_idx >= MPT_MAX_PROTOCOL_DRIVERS ||
		MptCallbacks[cb_idx] == NULL) {
		printk(MYIOC_s_WARN_FMT "%s: Invalid cb_idx (%d)!\n",
				__func__, ioc->name, cb_idx);
		goto out;
	}

	if (MptCallbacks[cb_idx](ioc, mf, mr))
		mpt_free_msg_frame(ioc, mf);
 out:
	mb();
}

static void
mpt_reply(MPT_ADAPTER *ioc, u32 pa)
{
	MPT_FRAME_HDR	*mf;
	MPT_FRAME_HDR	*mr;
	u16		 req_idx;
	u8		 cb_idx;
	int		 freeme;

	u32 reply_dma_low;
	u16 ioc_stat;

	/* non-TURBO reply!  Hmmm, something may be up...
	 *  Newest turbo reply mechanism; get address
	 *  via left shift 1 (get rid of MPI_ADDRESS_REPLY_A_BIT)!
	 */

	/* Map DMA address of reply header to cpu address.
	 * pa is 32 bits - but the dma address may be 32 or 64 bits
	 * get offset based only only the low addresses
	 */

	reply_dma_low = (pa <<= 1);
	mr = (MPT_FRAME_HDR *)((u8 *)ioc->reply_frames +
			 (reply_dma_low - ioc->reply_frames_low_dma));

	req_idx = le16_to_cpu(mr->u.frame.hwhdr.msgctxu.fld.req_idx);
	cb_idx = mr->u.frame.hwhdr.msgctxu.fld.cb_idx;
	mf = MPT_INDEX_2_MFPTR(ioc, req_idx);

	dmfprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Got non-TURBO reply=%p req_idx=%x cb_idx=%x Function=%x\n",
			ioc->name, mr, req_idx, cb_idx, mr->u.hdr.Function));
	DBG_DUMP_REPLY_FRAME(ioc, (u32 *)mr);

	 /*  Check/log IOC log info
	 */
	ioc_stat = le16_to_cpu(mr->u.reply.IOCStatus);
	if (ioc_stat & MPI_IOCSTATUS_FLAG_LOG_INFO_AVAILABLE) {
		u32	 log_info = le32_to_cpu(mr->u.reply.IOCLogInfo);
		if (ioc->bus_type == FC)
			mpt_fc_log_info(ioc, log_info);
		else if (ioc->bus_type == SPI)
			mpt_spi_log_info(ioc, log_info);
		else if (ioc->bus_type == SAS)
			mpt_sas_log_info(ioc, log_info);
	}

	if (ioc_stat & MPI_IOCSTATUS_MASK)
		mpt_iocstatus_info(ioc, (u32)ioc_stat, mf);

	/*  Check for (valid) IO callback!  */
	if (!cb_idx || cb_idx >= MPT_MAX_PROTOCOL_DRIVERS ||
		MptCallbacks[cb_idx] == NULL) {
		printk(MYIOC_s_WARN_FMT "%s: Invalid cb_idx (%d)!\n",
				__func__, ioc->name, cb_idx);
		freeme = 0;
		goto out;
	}

	freeme = MptCallbacks[cb_idx](ioc, mf, mr);

 out:
	/*  Flush (non-TURBO) reply with a WRITE!  */
	CHIPREG_WRITE32(&ioc->chip->ReplyFifo, pa);

	if (freeme)
		mpt_free_msg_frame(ioc, mf);
	mb();
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_interrupt - MPT adapter (IOC) specific interrupt handler.
 *	@irq: irq number (not used)
 *	@bus_id: bus identifier cookie == pointer to MPT_ADAPTER structure
 *
 *	This routine is registered via the request_irq() kernel API call,
 *	and handles all interrupts generated from a specific MPT adapter
 *	(also referred to as a IO Controller or IOC).
 *	This routine must clear the interrupt from the adapter and does
 *	so by reading the reply FIFO.  Multiple replies may be processed
 *	per single call to this routine.
 *
 *	This routine handles register-level access of the adapter but
 *	dispatches (calls) a protocol-specific callback routine to handle
 *	the protocol-specific details of the MPT request completion.
 */
static irqreturn_t
mpt_interrupt(int irq, void *bus_id)
{
	MPT_ADAPTER *ioc = bus_id;
	u32 pa = CHIPREG_READ32_dmasync(&ioc->chip->ReplyFifo);

	if (pa == 0xFFFFFFFF)
		return IRQ_NONE;

	/*
	 *  Drain the reply FIFO!
	 */
	do {
		if (pa & MPI_ADDRESS_REPLY_A_BIT)
			mpt_reply(ioc, pa);
		else
			mpt_turbo_reply(ioc, pa);
		pa = CHIPREG_READ32_dmasync(&ioc->chip->ReplyFifo);
	} while (pa != 0xFFFFFFFF);

	return IRQ_HANDLED;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptbase_reply - MPT base driver's callback routine
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@req: Pointer to original MPT request frame
 *	@reply: Pointer to MPT reply frame (NULL if TurboReply)
 *
 *	MPT base driver's callback routine; all base driver
 *	"internal" request/reply processing is routed here.
 *	Currently used for EventNotification and EventAck handling.
 *
 *	Returns 1 indicating original alloc'd request frame ptr
 *	should be freed, or 0 if it shouldn't.
 */
static int
mptbase_reply(MPT_ADAPTER *ioc, MPT_FRAME_HDR *req, MPT_FRAME_HDR *reply)
{
	EventNotificationReply_t *pEventReply;
	u8 event;
	int evHandlers;
	int freereq = 1;

	switch (reply->u.hdr.Function) {
	case MPI_FUNCTION_EVENT_NOTIFICATION:
		pEventReply = (EventNotificationReply_t *)reply;
		evHandlers = 0;
		ProcessEventNotification(ioc, pEventReply, &evHandlers);
		event = le32_to_cpu(pEventReply->Event) & 0xFF;
		if (pEventReply->MsgFlags & MPI_MSGFLAGS_CONTINUATION_REPLY)
			freereq = 0;
		if (event != MPI_EVENT_EVENT_CHANGE)
			break;
	case MPI_FUNCTION_CONFIG:
	case MPI_FUNCTION_SAS_IO_UNIT_CONTROL:
		ioc->mptbase_cmds.status |= MPT_MGMT_STATUS_COMMAND_GOOD;
		if (reply) {
			ioc->mptbase_cmds.status |= MPT_MGMT_STATUS_RF_VALID;
			memcpy(ioc->mptbase_cmds.reply, reply,
			    min(MPT_DEFAULT_FRAME_SIZE,
				4 * reply->u.reply.MsgLength));
		}
		if (ioc->mptbase_cmds.status & MPT_MGMT_STATUS_PENDING) {
			ioc->mptbase_cmds.status &= ~MPT_MGMT_STATUS_PENDING;
			complete(&ioc->mptbase_cmds.done);
		} else
			freereq = 0;
		if (ioc->mptbase_cmds.status & MPT_MGMT_STATUS_FREE_MF)
			freereq = 1;
		break;
	case MPI_FUNCTION_EVENT_ACK:
		devtverboseprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "EventAck reply received\n", ioc->name));
		break;
	default:
		printk(MYIOC_s_ERR_FMT
		    "Unexpected msg function (=%02Xh) reply received!\n",
		    ioc->name, reply->u.hdr.Function);
		break;
	}

	/*
	 *	Conditionally tell caller to free the original
	 *	EventNotification/EventAck/unexpected request frame!
	 */
	return freereq;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_register - Register protocol-specific main callback handler.
 *	@cbfunc: callback function pointer
 *	@dclass: Protocol driver's class (%MPT_DRIVER_CLASS enum value)
 *
 *	This routine is called by a protocol-specific driver (SCSI host,
 *	LAN, SCSI target) to register its reply callback routine.  Each
 *	protocol-specific driver must do this before it will be able to
 *	use any IOC resources, such as obtaining request frames.
 *
 *	NOTES: The SCSI protocol driver currently calls this routine thrice
 *	in order to register separate callbacks; one for "normal" SCSI IO;
 *	one for MptScsiTaskMgmt requests; one for Scan/DV requests.
 *
 *	Returns u8 valued "handle" in the range (and S.O.D. order)
 *	{N,...,7,6,5,...,1} if successful.
 *	A return value of MPT_MAX_PROTOCOL_DRIVERS (including zero!) should be
 *	considered an error by the caller.
 */
u8
mpt_register(MPT_CALLBACK cbfunc, MPT_DRIVER_CLASS dclass)
{
	u8 cb_idx;
	last_drv_idx = MPT_MAX_PROTOCOL_DRIVERS;

	/*
	 *  Search for empty callback slot in this order: {N,...,7,6,5,...,1}
	 *  (slot/handle 0 is reserved!)
	 */
	for (cb_idx = MPT_MAX_PROTOCOL_DRIVERS-1; cb_idx; cb_idx--) {
		if (MptCallbacks[cb_idx] == NULL) {
			MptCallbacks[cb_idx] = cbfunc;
			MptDriverClass[cb_idx] = dclass;
			MptEvHandlers[cb_idx] = NULL;
			last_drv_idx = cb_idx;
			break;
		}
	}

	return last_drv_idx;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_deregister - Deregister a protocol drivers resources.
 *	@cb_idx: previously registered callback handle
 *
 *	Each protocol-specific driver should call this routine when its
 *	module is unloaded.
 */
void
mpt_deregister(u8 cb_idx)
{
	if (cb_idx && (cb_idx < MPT_MAX_PROTOCOL_DRIVERS)) {
		MptCallbacks[cb_idx] = NULL;
		MptDriverClass[cb_idx] = MPTUNKNOWN_DRIVER;
		MptEvHandlers[cb_idx] = NULL;

		last_drv_idx++;
	}
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_event_register - Register protocol-specific event callback handler.
 *	@cb_idx: previously registered (via mpt_register) callback handle
 *	@ev_cbfunc: callback function
 *
 *	This routine can be called by one or more protocol-specific drivers
 *	if/when they choose to be notified of MPT events.
 *
 *	Returns 0 for success.
 */
int
mpt_event_register(u8 cb_idx, MPT_EVHANDLER ev_cbfunc)
{
	if (!cb_idx || cb_idx >= MPT_MAX_PROTOCOL_DRIVERS)
		return -1;

	MptEvHandlers[cb_idx] = ev_cbfunc;
	return 0;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_event_deregister - Deregister protocol-specific event callback handler
 *	@cb_idx: previously registered callback handle
 *
 *	Each protocol-specific driver should call this routine
 *	when it does not (or can no longer) handle events,
 *	or when its module is unloaded.
 */
void
mpt_event_deregister(u8 cb_idx)
{
	if (!cb_idx || cb_idx >= MPT_MAX_PROTOCOL_DRIVERS)
		return;

	MptEvHandlers[cb_idx] = NULL;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_reset_register - Register protocol-specific IOC reset handler.
 *	@cb_idx: previously registered (via mpt_register) callback handle
 *	@reset_func: reset function
 *
 *	This routine can be called by one or more protocol-specific drivers
 *	if/when they choose to be notified of IOC resets.
 *
 *	Returns 0 for success.
 */
int
mpt_reset_register(u8 cb_idx, MPT_RESETHANDLER reset_func)
{
	if (!cb_idx || cb_idx >= MPT_MAX_PROTOCOL_DRIVERS)
		return -1;

	MptResetHandlers[cb_idx] = reset_func;
	return 0;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_reset_deregister - Deregister protocol-specific IOC reset handler.
 *	@cb_idx: previously registered callback handle
 *
 *	Each protocol-specific driver should call this routine
 *	when it does not (or can no longer) handle IOC reset handling,
 *	or when its module is unloaded.
 */
void
mpt_reset_deregister(u8 cb_idx)
{
	if (!cb_idx || cb_idx >= MPT_MAX_PROTOCOL_DRIVERS)
		return;

	MptResetHandlers[cb_idx] = NULL;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_device_driver_register - Register device driver hooks
 *	@dd_cbfunc: driver callbacks struct
 *	@cb_idx: MPT protocol driver index
 */
int
mpt_device_driver_register(struct mpt_pci_driver * dd_cbfunc, u8 cb_idx)
{
	MPT_ADAPTER	*ioc;
	const struct pci_device_id *id;

	if (!cb_idx || cb_idx >= MPT_MAX_PROTOCOL_DRIVERS)
		return -EINVAL;

	MptDeviceDriverHandlers[cb_idx] = dd_cbfunc;

	/* call per pci device probe entry point */
	list_for_each_entry(ioc, &ioc_list, list) {
		id = ioc->pcidev->driver ?
		    ioc->pcidev->driver->id_table : NULL;
		if (dd_cbfunc->probe)
			dd_cbfunc->probe(ioc->pcidev, id);
	 }

	return 0;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_device_driver_deregister - DeRegister device driver hooks
 *	@cb_idx: MPT protocol driver index
 */
void
mpt_device_driver_deregister(u8 cb_idx)
{
	struct mpt_pci_driver *dd_cbfunc;
	MPT_ADAPTER	*ioc;

	if (!cb_idx || cb_idx >= MPT_MAX_PROTOCOL_DRIVERS)
		return;

	dd_cbfunc = MptDeviceDriverHandlers[cb_idx];

	list_for_each_entry(ioc, &ioc_list, list) {
		if (dd_cbfunc->remove)
			dd_cbfunc->remove(ioc->pcidev);
	}

	MptDeviceDriverHandlers[cb_idx] = NULL;
}


/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_get_msg_frame - Obtain an MPT request frame from the pool
 *	@cb_idx: Handle of registered MPT protocol driver
 *	@ioc: Pointer to MPT adapter structure
 *
 *	Obtain an MPT request frame from the pool (of 1024) that are
 *	allocated per MPT adapter.
 *
 *	Returns pointer to a MPT request frame or %NULL if none are available
 *	or IOC is not active.
 */
MPT_FRAME_HDR*
mpt_get_msg_frame(u8 cb_idx, MPT_ADAPTER *ioc)
{
	MPT_FRAME_HDR *mf;
	unsigned long flags;
	u16	 req_idx;	/* Request index */

	/* validate handle and ioc identifier */

#ifdef MFCNT
	if (!ioc->active)
		printk(MYIOC_s_WARN_FMT "IOC Not Active! mpt_get_msg_frame "
		    "returning NULL!\n", ioc->name);
#endif

	/* If interrupts are not attached, do not return a request frame */
	if (!ioc->active)
		return NULL;

	spin_lock_irqsave(&ioc->FreeQlock, flags);
	if (!list_empty(&ioc->FreeQ)) {
		int req_offset;

		mf = list_entry(ioc->FreeQ.next, MPT_FRAME_HDR,
				u.frame.linkage.list);
		list_del(&mf->u.frame.linkage.list);
		mf->u.frame.linkage.arg1 = 0;
		mf->u.frame.hwhdr.msgctxu.fld.cb_idx = cb_idx;	/* byte */
		req_offset = (u8 *)mf - (u8 *)ioc->req_frames;
								/* u16! */
		req_idx = req_offset / ioc->req_sz;
		mf->u.frame.hwhdr.msgctxu.fld.req_idx = cpu_to_le16(req_idx);
		mf->u.frame.hwhdr.msgctxu.fld.rsvd = 0;
		/* Default, will be changed if necessary in SG generation */
		ioc->RequestNB[req_idx] = ioc->NB_for_64_byte_frame;
#ifdef MFCNT
		ioc->mfcnt++;
#endif
	}
	else
		mf = NULL;
	spin_unlock_irqrestore(&ioc->FreeQlock, flags);

#ifdef MFCNT
	if (mf == NULL)
		printk(MYIOC_s_WARN_FMT "IOC Active. No free Msg Frames! "
		    "Count 0x%x Max 0x%x\n", ioc->name, ioc->mfcnt,
		    ioc->req_depth);
	mfcounter++;
	if (mfcounter == PRINT_MF_COUNT)
		printk(MYIOC_s_INFO_FMT "MF Count 0x%x Max 0x%x \n", ioc->name,
		    ioc->mfcnt, ioc->req_depth);
#endif

	dmfprintk(ioc, printk(MYIOC_s_DEBUG_FMT "mpt_get_msg_frame(%d,%d), got mf=%p\n",
	    ioc->name, cb_idx, ioc->id, mf));
	return mf;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_put_msg_frame - Send a protocol-specific MPT request frame to an IOC
 *	@cb_idx: Handle of registered MPT protocol driver
 *	@ioc: Pointer to MPT adapter structure
 *	@mf: Pointer to MPT request frame
 *
 *	This routine posts an MPT request frame to the request post FIFO of a
 *	specific MPT adapter.
 */
void
mpt_put_msg_frame(u8 cb_idx, MPT_ADAPTER *ioc, MPT_FRAME_HDR *mf)
{
	u32 mf_dma_addr;
	int req_offset;
	u16	 req_idx;	/* Request index */

	/* ensure values are reset properly! */
	mf->u.frame.hwhdr.msgctxu.fld.cb_idx = cb_idx;		/* byte */
	req_offset = (u8 *)mf - (u8 *)ioc->req_frames;
								/* u16! */
	req_idx = req_offset / ioc->req_sz;
	mf->u.frame.hwhdr.msgctxu.fld.req_idx = cpu_to_le16(req_idx);
	mf->u.frame.hwhdr.msgctxu.fld.rsvd = 0;

	DBG_DUMP_PUT_MSG_FRAME(ioc, (u32 *)mf);

	mf_dma_addr = (ioc->req_frames_low_dma + req_offset) | ioc->RequestNB[req_idx];
	dsgprintk(ioc, printk(MYIOC_s_DEBUG_FMT "mf_dma_addr=%x req_idx=%d "
	    "RequestNB=%x\n", ioc->name, mf_dma_addr, req_idx,
	    ioc->RequestNB[req_idx]));
	CHIPREG_WRITE32(&ioc->chip->RequestFifo, mf_dma_addr);
}

/**
 *	mpt_put_msg_frame_hi_pri - Send a hi-pri protocol-specific MPT request frame
 *	@cb_idx: Handle of registered MPT protocol driver
 *	@ioc: Pointer to MPT adapter structure
 *	@mf: Pointer to MPT request frame
 *
 *	Send a protocol-specific MPT request frame to an IOC using
 *	hi-priority request queue.
 *
 *	This routine posts an MPT request frame to the request post FIFO of a
 *	specific MPT adapter.
 **/
void
mpt_put_msg_frame_hi_pri(u8 cb_idx, MPT_ADAPTER *ioc, MPT_FRAME_HDR *mf)
{
	u32 mf_dma_addr;
	int req_offset;
	u16	 req_idx;	/* Request index */

	/* ensure values are reset properly! */
	mf->u.frame.hwhdr.msgctxu.fld.cb_idx = cb_idx;
	req_offset = (u8 *)mf - (u8 *)ioc->req_frames;
	req_idx = req_offset / ioc->req_sz;
	mf->u.frame.hwhdr.msgctxu.fld.req_idx = cpu_to_le16(req_idx);
	mf->u.frame.hwhdr.msgctxu.fld.rsvd = 0;

	DBG_DUMP_PUT_MSG_FRAME(ioc, (u32 *)mf);

	mf_dma_addr = (ioc->req_frames_low_dma + req_offset);
	dsgprintk(ioc, printk(MYIOC_s_DEBUG_FMT "mf_dma_addr=%x req_idx=%d\n",
		ioc->name, mf_dma_addr, req_idx));
	CHIPREG_WRITE32(&ioc->chip->RequestHiPriFifo, mf_dma_addr);
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_free_msg_frame - Place MPT request frame back on FreeQ.
 *	@ioc: Pointer to MPT adapter structure
 *	@mf: Pointer to MPT request frame
 *
 *	This routine places a MPT request frame back on the MPT adapter's
 *	FreeQ.
 */
void
mpt_free_msg_frame(MPT_ADAPTER *ioc, MPT_FRAME_HDR *mf)
{
	unsigned long flags;

	/*  Put Request back on FreeQ!  */
	spin_lock_irqsave(&ioc->FreeQlock, flags);
	if (cpu_to_le32(mf->u.frame.linkage.arg1) == 0xdeadbeaf)
		goto out;
	/* signature to know if this mf is freed */
	mf->u.frame.linkage.arg1 = cpu_to_le32(0xdeadbeaf);
	list_add_tail(&mf->u.frame.linkage.list, &ioc->FreeQ);
#ifdef MFCNT
	ioc->mfcnt--;
#endif
 out:
	spin_unlock_irqrestore(&ioc->FreeQlock, flags);
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_add_sge - Place a simple 32 bit SGE at address pAddr.
 *	@pAddr: virtual address for SGE
 *	@flagslength: SGE flags and data transfer length
 *	@dma_addr: Physical address
 *
 *	This routine places a MPT request frame back on the MPT adapter's
 *	FreeQ.
 */
static void
mpt_add_sge(void *pAddr, u32 flagslength, dma_addr_t dma_addr)
{
	SGESimple32_t *pSge = (SGESimple32_t *) pAddr;
	pSge->FlagsLength = cpu_to_le32(flagslength);
	pSge->Address = cpu_to_le32(dma_addr);
}

/**
 *	mpt_add_sge_64bit - Place a simple 64 bit SGE at address pAddr.
 *	@pAddr: virtual address for SGE
 *	@flagslength: SGE flags and data transfer length
 *	@dma_addr: Physical address
 *
 *	This routine places a MPT request frame back on the MPT adapter's
 *	FreeQ.
 **/
static void
mpt_add_sge_64bit(void *pAddr, u32 flagslength, dma_addr_t dma_addr)
{
	SGESimple64_t *pSge = (SGESimple64_t *) pAddr;
	pSge->Address.Low = cpu_to_le32
			(lower_32_bits(dma_addr));
	pSge->Address.High = cpu_to_le32
			(upper_32_bits(dma_addr));
	pSge->FlagsLength = cpu_to_le32
			((flagslength | MPT_SGE_FLAGS_64_BIT_ADDRESSING));
}

/**
 *	mpt_add_sge_64bit_1078 - Place a simple 64 bit SGE at address pAddr (1078 workaround).
 *	@pAddr: virtual address for SGE
 *	@flagslength: SGE flags and data transfer length
 *	@dma_addr: Physical address
 *
 *	This routine places a MPT request frame back on the MPT adapter's
 *	FreeQ.
 **/
static void
mpt_add_sge_64bit_1078(void *pAddr, u32 flagslength, dma_addr_t dma_addr)
{
	SGESimple64_t *pSge = (SGESimple64_t *) pAddr;
	u32 tmp;

	pSge->Address.Low = cpu_to_le32
			(lower_32_bits(dma_addr));
	tmp = (u32)(upper_32_bits(dma_addr));

	/*
	 * 1078 errata workaround for the 36GB limitation
	 */
	if ((((u64)dma_addr + MPI_SGE_LENGTH(flagslength)) >> 32)  == 9) {
		flagslength |=
		    MPI_SGE_SET_FLAGS(MPI_SGE_FLAGS_LOCAL_ADDRESS);
		tmp |= (1<<31);
		if (mpt_debug_level & MPT_DEBUG_36GB_MEM)
			printk(KERN_DEBUG "1078 P0M2 addressing for "
			    "addr = 0x%llx len = %d\n",
			    (unsigned long long)dma_addr,
			    MPI_SGE_LENGTH(flagslength));
	}

	pSge->Address.High = cpu_to_le32(tmp);
	pSge->FlagsLength = cpu_to_le32(
		(flagslength | MPT_SGE_FLAGS_64_BIT_ADDRESSING));
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_add_chain - Place a 32 bit chain SGE at address pAddr.
 *	@pAddr: virtual address for SGE
 *	@next: nextChainOffset value (u32's)
 *	@length: length of next SGL segment
 *	@dma_addr: Physical address
 *
 */
static void
mpt_add_chain(void *pAddr, u8 next, u16 length, dma_addr_t dma_addr)
{
		SGEChain32_t *pChain = (SGEChain32_t *) pAddr;
		pChain->Length = cpu_to_le16(length);
		pChain->Flags = MPI_SGE_FLAGS_CHAIN_ELEMENT;
		pChain->NextChainOffset = next;
		pChain->Address = cpu_to_le32(dma_addr);
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_add_chain_64bit - Place a 64 bit chain SGE at address pAddr.
 *	@pAddr: virtual address for SGE
 *	@next: nextChainOffset value (u32's)
 *	@length: length of next SGL segment
 *	@dma_addr: Physical address
 *
 */
static void
mpt_add_chain_64bit(void *pAddr, u8 next, u16 length, dma_addr_t dma_addr)
{
		SGEChain64_t *pChain = (SGEChain64_t *) pAddr;
		u32 tmp = dma_addr & 0xFFFFFFFF;

		pChain->Length = cpu_to_le16(length);
		pChain->Flags = (MPI_SGE_FLAGS_CHAIN_ELEMENT |
				 MPI_SGE_FLAGS_64_BIT_ADDRESSING);

		pChain->NextChainOffset = next;

		pChain->Address.Low = cpu_to_le32(tmp);
		tmp = (u32)(upper_32_bits(dma_addr));
		pChain->Address.High = cpu_to_le32(tmp);
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_send_handshake_request - Send MPT request via doorbell handshake method.
 *	@cb_idx: Handle of registered MPT protocol driver
 *	@ioc: Pointer to MPT adapter structure
 *	@reqBytes: Size of the request in bytes
 *	@req: Pointer to MPT request frame
 *	@sleepFlag: Use schedule if CAN_SLEEP else use udelay.
 *
 *	This routine is used exclusively to send MptScsiTaskMgmt
 *	requests since they are required to be sent via doorbell handshake.
 *
 *	NOTE: It is the callers responsibility to byte-swap fields in the
 *	request which are greater than 1 byte in size.
 *
 *	Returns 0 for success, non-zero for failure.
 */
int
mpt_send_handshake_request(u8 cb_idx, MPT_ADAPTER *ioc, int reqBytes, u32 *req, int sleepFlag)
{
	int	r = 0;
	u8	*req_as_bytes;
	int	 ii;

	/* State is known to be good upon entering
	 * this function so issue the bus reset
	 * request.
	 */

	/*
	 * Emulate what mpt_put_msg_frame() does /wrt to sanity
	 * setting cb_idx/req_idx.  But ONLY if this request
	 * is in proper (pre-alloc'd) request buffer range...
	 */
	ii = MFPTR_2_MPT_INDEX(ioc,(MPT_FRAME_HDR*)req);
	if (reqBytes >= 12 && ii >= 0 && ii < ioc->req_depth) {
		MPT_FRAME_HDR *mf = (MPT_FRAME_HDR*)req;
		mf->u.frame.hwhdr.msgctxu.fld.req_idx = cpu_to_le16(ii);
		mf->u.frame.hwhdr.msgctxu.fld.cb_idx = cb_idx;
	}

	/* Make sure there are no doorbells */
	CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);

	CHIPREG_WRITE32(&ioc->chip->Doorbell,
			((MPI_FUNCTION_HANDSHAKE<<MPI_DOORBELL_FUNCTION_SHIFT) |
			 ((reqBytes/4)<<MPI_DOORBELL_ADD_DWORDS_SHIFT)));

	/* Wait for IOC doorbell int */
	if ((ii = WaitForDoorbellInt(ioc, 5, sleepFlag)) < 0) {
		return ii;
	}

	/* Read doorbell and check for active bit */
	if (!(CHIPREG_READ32(&ioc->chip->Doorbell) & MPI_DOORBELL_ACTIVE))
		return -5;

	dhsprintk(ioc, printk(MYIOC_s_DEBUG_FMT "mpt_send_handshake_request start, WaitCnt=%d\n",
		ioc->name, ii));

	CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);

	if ((r = WaitForDoorbellAck(ioc, 5, sleepFlag)) < 0) {
		return -2;
	}

	/* Send request via doorbell handshake */
	req_as_bytes = (u8 *) req;
	for (ii = 0; ii < reqBytes/4; ii++) {
		u32 word;

		word = ((req_as_bytes[(ii*4) + 0] <<  0) |
			(req_as_bytes[(ii*4) + 1] <<  8) |
			(req_as_bytes[(ii*4) + 2] << 16) |
			(req_as_bytes[(ii*4) + 3] << 24));
		CHIPREG_WRITE32(&ioc->chip->Doorbell, word);
		if ((r = WaitForDoorbellAck(ioc, 5, sleepFlag)) < 0) {
			r = -3;
			break;
		}
	}

	if (r >= 0 && WaitForDoorbellInt(ioc, 10, sleepFlag) >= 0)
		r = 0;
	else
		r = -4;

	/* Make sure there are no doorbells */
	CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);

	return r;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 * mpt_host_page_access_control - control the IOC's Host Page Buffer access
 * @ioc: Pointer to MPT adapter structure
 * @access_control_value: define bits below
 * @sleepFlag: Specifies whether the process can sleep
 *
 * Provides mechanism for the host driver to control the IOC's
 * Host Page Buffer access.
 *
 * Access Control Value - bits[15:12]
 * 0h Reserved
 * 1h Enable Access { MPI_DB_HPBAC_ENABLE_ACCESS }
 * 2h Disable Access { MPI_DB_HPBAC_DISABLE_ACCESS }
 * 3h Free Buffer { MPI_DB_HPBAC_FREE_BUFFER }
 *
 * Returns 0 for success, non-zero for failure.
 */

static int
mpt_host_page_access_control(MPT_ADAPTER *ioc, u8 access_control_value, int sleepFlag)
{
	int	 r = 0;

	/* return if in use */
	if (CHIPREG_READ32(&ioc->chip->Doorbell)
	    & MPI_DOORBELL_ACTIVE)
	    return -1;

	CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);

	CHIPREG_WRITE32(&ioc->chip->Doorbell,
		((MPI_FUNCTION_HOST_PAGEBUF_ACCESS_CONTROL
		 <<MPI_DOORBELL_FUNCTION_SHIFT) |
		 (access_control_value<<12)));

	/* Wait for IOC to clear Doorbell Status bit */
	if ((r = WaitForDoorbellAck(ioc, 5, sleepFlag)) < 0) {
		return -2;
	}else
		return 0;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_host_page_alloc - allocate system memory for the fw
 *	@ioc: Pointer to pointer to IOC adapter
 *	@ioc_init: Pointer to ioc init config page
 *
 *	If we already allocated memory in past, then resend the same pointer.
 *	Returns 0 for success, non-zero for failure.
 */
static int
mpt_host_page_alloc(MPT_ADAPTER *ioc, pIOCInit_t ioc_init)
{
	char	*psge;
	int	flags_length;
	u32	host_page_buffer_sz=0;

	if(!ioc->HostPageBuffer) {

		host_page_buffer_sz =
		    le32_to_cpu(ioc->facts.HostPageBufferSGE.FlagsLength) & 0xFFFFFF;

		if(!host_page_buffer_sz)
			return 0; /* fw doesn't need any host buffers */

		/* spin till we get enough memory */
		while(host_page_buffer_sz > 0) {

			if((ioc->HostPageBuffer = pci_alloc_consistent(
			    ioc->pcidev,
			    host_page_buffer_sz,
			    &ioc->HostPageBuffer_dma)) != NULL) {

				dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT
				    "host_page_buffer @ %p, dma @ %x, sz=%d bytes\n",
				    ioc->name, ioc->HostPageBuffer,
				    (u32)ioc->HostPageBuffer_dma,
				    host_page_buffer_sz));
				ioc->alloc_total += host_page_buffer_sz;
				ioc->HostPageBuffer_sz = host_page_buffer_sz;
				break;
			}

			host_page_buffer_sz -= (4*1024);
		}
	}

	if(!ioc->HostPageBuffer) {
		printk(MYIOC_s_ERR_FMT
		    "Failed to alloc memory for host_page_buffer!\n",
		    ioc->name);
		return -999;
	}

	psge = (char *)&ioc_init->HostPageBufferSGE;
	flags_length = MPI_SGE_FLAGS_SIMPLE_ELEMENT |
	    MPI_SGE_FLAGS_SYSTEM_ADDRESS |
	    MPI_SGE_FLAGS_HOST_TO_IOC |
	    MPI_SGE_FLAGS_END_OF_BUFFER;
	flags_length = flags_length << MPI_SGE_FLAGS_SHIFT;
	flags_length |= ioc->HostPageBuffer_sz;
	ioc->add_sge(psge, flags_length, ioc->HostPageBuffer_dma);
	ioc->facts.HostPageBufferSGE = ioc_init->HostPageBufferSGE;

return 0;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_verify_adapter - Given IOC identifier, set pointer to its adapter structure.
 *	@iocid: IOC unique identifier (integer)
 *	@iocpp: Pointer to pointer to IOC adapter
 *
 *	Given a unique IOC identifier, set pointer to the associated MPT
 *	adapter structure.
 *
 *	Returns iocid and sets iocpp if iocid is found.
 *	Returns -1 if iocid is not found.
 */
int
mpt_verify_adapter(int iocid, MPT_ADAPTER **iocpp)
{
	MPT_ADAPTER *ioc;

	list_for_each_entry(ioc,&ioc_list,list) {
		if (ioc->id == iocid) {
			*iocpp =ioc;
			return iocid;
		}
	}

	*iocpp = NULL;
	return -1;
}

/**
 *	mpt_get_product_name - returns product string
 *	@vendor: pci vendor id
 *	@device: pci device id
 *	@revision: pci revision id
 *	@prod_name: string returned
 *
 *	Returns product string displayed when driver loads,
 *	in /proc/mpt/summary and /sysfs/class/scsi_host/host<X>/version_product
 *
 **/
static void
mpt_get_product_name(u16 vendor, u16 device, u8 revision, char *prod_name)
{
	char *product_str = NULL;

	if (vendor == PCI_VENDOR_ID_BROCADE) {
		switch (device)
		{
		case MPI_MANUFACTPAGE_DEVICEID_FC949E:
			switch (revision)
			{
			case 0x00:
				product_str = "BRE040 A0";
				break;
			case 0x01:
				product_str = "BRE040 A1";
				break;
			default:
				product_str = "BRE040";
				break;
			}
			break;
		}
		goto out;
	}

	switch (device)
	{
	case MPI_MANUFACTPAGE_DEVICEID_FC909:
		product_str = "LSIFC909 B1";
		break;
	case MPI_MANUFACTPAGE_DEVICEID_FC919:
		product_str = "LSIFC919 B0";
		break;
	case MPI_MANUFACTPAGE_DEVICEID_FC929:
		product_str = "LSIFC929 B0";
		break;
	case MPI_MANUFACTPAGE_DEVICEID_FC919X:
		if (revision < 0x80)
			product_str = "LSIFC919X A0";
		else
			product_str = "LSIFC919XL A1";
		break;
	case MPI_MANUFACTPAGE_DEVICEID_FC929X:
		if (revision < 0x80)
			product_str = "LSIFC929X A0";
		else
			product_str = "LSIFC929XL A1";
		break;
	case MPI_MANUFACTPAGE_DEVICEID_FC939X:
		product_str = "LSIFC939X A1";
		break;
	case MPI_MANUFACTPAGE_DEVICEID_FC949X:
		product_str = "LSIFC949X A1";
		break;
	case MPI_MANUFACTPAGE_DEVICEID_FC949E:
		switch (revision)
		{
		case 0x00:
			product_str = "LSIFC949E A0";
			break;
		case 0x01:
			product_str = "LSIFC949E A1";
			break;
		default:
			product_str = "LSIFC949E";
			break;
		}
		break;
	case MPI_MANUFACTPAGE_DEVID_53C1030:
		switch (revision)
		{
		case 0x00:
			product_str = "LSI53C1030 A0";
			break;
		case 0x01:
			product_str = "LSI53C1030 B0";
			break;
		case 0x03:
			product_str = "LSI53C1030 B1";
			break;
		case 0x07:
			product_str = "LSI53C1030 B2";
			break;
		case 0x08:
			product_str = "LSI53C1030 C0";
			break;
		case 0x80:
			product_str = "LSI53C1030T A0";
			break;
		case 0x83:
			product_str = "LSI53C1030T A2";
			break;
		case 0x87:
			product_str = "LSI53C1030T A3";
			break;
		case 0xc1:
			product_str = "LSI53C1020A A1";
			break;
		default:
			product_str = "LSI53C1030";
			break;
		}
		break;
	case MPI_MANUFACTPAGE_DEVID_1030_53C1035:
		switch (revision)
		{
		case 0x03:
			product_str = "LSI53C1035 A2";
			break;
		case 0x04:
			product_str = "LSI53C1035 B0";
			break;
		default:
			product_str = "LSI53C1035";
			break;
		}
		break;
	case MPI_MANUFACTPAGE_DEVID_SAS1064:
		switch (revision)
		{
		case 0x00:
			product_str = "LSISAS1064 A1";
			break;
		case 0x01:
			product_str = "LSISAS1064 A2";
			break;
		case 0x02:
			product_str = "LSISAS1064 A3";
			break;
		case 0x03:
			product_str = "LSISAS1064 A4";
			break;
		default:
			product_str = "LSISAS1064";
			break;
		}
		break;
	case MPI_MANUFACTPAGE_DEVID_SAS1064E:
		switch (revision)
		{
		case 0x00:
			product_str = "LSISAS1064E A0";
			break;
		case 0x01:
			product_str = "LSISAS1064E B0";
			break;
		case 0x02:
			product_str = "LSISAS1064E B1";
			break;
		case 0x04:
			product_str = "LSISAS1064E B2";
			break;
		case 0x08:
			product_str = "LSISAS1064E B3";
			break;
		default:
			product_str = "LSISAS1064E";
			break;
		}
		break;
	case MPI_MANUFACTPAGE_DEVID_SAS1068:
		switch (revision)
		{
		case 0x00:
			product_str = "LSISAS1068 A0";
			break;
		case 0x01:
			product_str = "LSISAS1068 B0";
			break;
		case 0x02:
			product_str = "LSISAS1068 B1";
			break;
		default:
			product_str = "LSISAS1068";
			break;
		}
		break;
	case MPI_MANUFACTPAGE_DEVID_SAS1068E:
		switch (revision)
		{
		case 0x00:
			product_str = "LSISAS1068E A0";
			break;
		case 0x01:
			product_str = "LSISAS1068E B0";
			break;
		case 0x02:
			product_str = "LSISAS1068E B1";
			break;
		case 0x04:
			product_str = "LSISAS1068E B2";
			break;
		case 0x08:
			product_str = "LSISAS1068E B3";
			break;
		default:
			product_str = "LSISAS1068E";
			break;
		}
		break;
	case MPI_MANUFACTPAGE_DEVID_SAS1078:
		switch (revision)
		{
		case 0x00:
			product_str = "LSISAS1078 A0";
			break;
		case 0x01:
			product_str = "LSISAS1078 B0";
			break;
		case 0x02:
			product_str = "LSISAS1078 C0";
			break;
		case 0x03:
			product_str = "LSISAS1078 C1";
			break;
		case 0x04:
			product_str = "LSISAS1078 C2";
			break;
		default:
			product_str = "LSISAS1078";
			break;
		}
		break;
	}

 out:
	if (product_str)
		sprintf(prod_name, "%s", product_str);
}

/**
 *	mpt_mapresources - map in memory mapped io
 *	@ioc: Pointer to pointer to IOC adapter
 *
 **/
static int
mpt_mapresources(MPT_ADAPTER *ioc)
{
	u8		__iomem *mem;
	int		 ii;
	unsigned long	 mem_phys;
	unsigned long	 port;
	u32		 msize;
	u32		 psize;
	u8		 revision;
	int		 r = -ENODEV;
	struct pci_dev *pdev;

	pdev = ioc->pcidev;
	ioc->bars = pci_select_bars(pdev, IORESOURCE_MEM);
	if (pci_enable_device_mem(pdev)) {
		printk(MYIOC_s_ERR_FMT "pci_enable_device_mem() "
		    "failed\n", ioc->name);
		return r;
	}
	if (pci_request_selected_regions(pdev, ioc->bars, "mpt")) {
		printk(MYIOC_s_ERR_FMT "pci_request_selected_regions() with "
		    "MEM failed\n", ioc->name);
		return r;
	}

	pci_read_config_byte(pdev, PCI_CLASS_REVISION, &revision);

	if (sizeof(dma_addr_t) > 4) {
		const uint64_t required_mask = dma_get_required_mask
		    (&pdev->dev);
		if (required_mask > DMA_BIT_MASK(32)
			&& !pci_set_dma_mask(pdev, DMA_BIT_MASK(64))
			&& !pci_set_consistent_dma_mask(pdev,
						 DMA_BIT_MASK(64))) {
			ioc->dma_mask = DMA_BIT_MASK(64);
			dinitprintk(ioc, printk(MYIOC_s_INFO_FMT
				": 64 BIT PCI BUS DMA ADDRESSING SUPPORTED\n",
				ioc->name));
		} else if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(32))
			&& !pci_set_consistent_dma_mask(pdev,
						DMA_BIT_MASK(32))) {
			ioc->dma_mask = DMA_BIT_MASK(32);
			dinitprintk(ioc, printk(MYIOC_s_INFO_FMT
				": 32 BIT PCI BUS DMA ADDRESSING SUPPORTED\n",
				ioc->name));
		} else {
			printk(MYIOC_s_WARN_FMT "no suitable DMA mask for %s\n",
			    ioc->name, pci_name(pdev));
			return r;
		}
	} else {
		if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(32))
			&& !pci_set_consistent_dma_mask(pdev,
						DMA_BIT_MASK(32))) {
			ioc->dma_mask = DMA_BIT_MASK(32);
			dinitprintk(ioc, printk(MYIOC_s_INFO_FMT
				": 32 BIT PCI BUS DMA ADDRESSING SUPPORTED\n",
				ioc->name));
		} else {
			printk(MYIOC_s_WARN_FMT "no suitable DMA mask for %s\n",
			    ioc->name, pci_name(pdev));
			return r;
		}
	}

	mem_phys = msize = 0;
	port = psize = 0;
	for (ii = 0; ii < DEVICE_COUNT_RESOURCE; ii++) {
		if (pci_resource_flags(pdev, ii) & PCI_BASE_ADDRESS_SPACE_IO) {
			if (psize)
				continue;
			/* Get I/O space! */
			port = pci_resource_start(pdev, ii);
			psize = pci_resource_len(pdev, ii);
		} else {
			if (msize)
				continue;
			/* Get memmap */
			mem_phys = pci_resource_start(pdev, ii);
			msize = pci_resource_len(pdev, ii);
		}
	}
	ioc->mem_size = msize;

	mem = NULL;
	/* Get logical ptr for PciMem0 space */
	/*mem = ioremap(mem_phys, msize);*/
	mem = ioremap(mem_phys, msize);
	if (mem == NULL) {
		printk(MYIOC_s_ERR_FMT ": ERROR - Unable to map adapter"
			" memory!\n", ioc->name);
		return -EINVAL;
	}
	ioc->memmap = mem;
	dinitprintk(ioc, printk(MYIOC_s_INFO_FMT "mem = %p, mem_phys = %lx\n",
	    ioc->name, mem, mem_phys));

	ioc->mem_phys = mem_phys;
	ioc->chip = (SYSIF_REGS __iomem *)mem;

	/* Save Port IO values in case we need to do downloadboot */
	ioc->pio_mem_phys = port;
	ioc->pio_chip = (SYSIF_REGS __iomem *)port;

	return 0;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_attach - Install a PCI intelligent MPT adapter.
 *	@pdev: Pointer to pci_dev structure
 *	@id: PCI device ID information
 *
 *	This routine performs all the steps necessary to bring the IOC of
 *	a MPT adapter to a OPERATIONAL state.  This includes registering
 *	memory regions, registering the interrupt, and allocating request
 *	and reply memory pools.
 *
 *	This routine also pre-fetches the LAN MAC address of a Fibre Channel
 *	MPT adapter.
 *
 *	Returns 0 for success, non-zero for failure.
 *
 *	TODO: Add support for polled controllers
 */
int
mpt_attach(struct pci_dev *pdev, const struct pci_device_id *id)
{
	MPT_ADAPTER	*ioc;
	u8		 cb_idx;
	int		 r = -ENODEV;
	u8		 revision;
	u8		 pcixcmd;
	static int	 mpt_ids = 0;
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry *dent, *ent;
#endif

	ioc = kzalloc(sizeof(MPT_ADAPTER), GFP_ATOMIC);
	if (ioc == NULL) {
		printk(KERN_ERR MYNAM ": ERROR - Insufficient memory to add adapter!\n");
		return -ENOMEM;
	}

	ioc->id = mpt_ids++;
	sprintf(ioc->name, "ioc%d", ioc->id);
	dinitprintk(ioc, printk(KERN_WARNING MYNAM ": mpt_adapter_install\n"));

	/*
	 * set initial debug level
	 * (refer to mptdebug.h)
	 *
	 */
	ioc->debug_level = mpt_debug_level;
	if (mpt_debug_level)
		printk(KERN_INFO "mpt_debug_level=%xh\n", mpt_debug_level);

	dinitprintk(ioc, printk(MYIOC_s_INFO_FMT ": mpt_adapter_install\n", ioc->name));

	ioc->pcidev = pdev;
	if (mpt_mapresources(ioc)) {
		kfree(ioc);
		return r;
	}

	/*
	 * Setting up proper handlers for scatter gather handling
	 */
	if (ioc->dma_mask == DMA_BIT_MASK(64)) {
		if (pdev->device == MPI_MANUFACTPAGE_DEVID_SAS1078)
			ioc->add_sge = &mpt_add_sge_64bit_1078;
		else
			ioc->add_sge = &mpt_add_sge_64bit;
		ioc->add_chain = &mpt_add_chain_64bit;
		ioc->sg_addr_size = 8;
	} else {
		ioc->add_sge = &mpt_add_sge;
		ioc->add_chain = &mpt_add_chain;
		ioc->sg_addr_size = 4;
	}
	ioc->SGE_size = sizeof(u32) + ioc->sg_addr_size;

	ioc->alloc_total = sizeof(MPT_ADAPTER);
	ioc->req_sz = MPT_DEFAULT_FRAME_SIZE;		/* avoid div by zero! */
	ioc->reply_sz = MPT_REPLY_FRAME_SIZE;

	ioc->pcidev = pdev;

	spin_lock_init(&ioc->taskmgmt_lock);
	mutex_init(&ioc->internal_cmds.mutex);
	init_completion(&ioc->internal_cmds.done);
	mutex_init(&ioc->mptbase_cmds.mutex);
	init_completion(&ioc->mptbase_cmds.done);
	mutex_init(&ioc->taskmgmt_cmds.mutex);
	init_completion(&ioc->taskmgmt_cmds.done);

	/* Initialize the event logging.
	 */
	ioc->eventTypes = 0;	/* None */
	ioc->eventContext = 0;
	ioc->eventLogSize = 0;
	ioc->events = NULL;

#ifdef MFCNT
	ioc->mfcnt = 0;
#endif

	ioc->sh = NULL;
	ioc->cached_fw = NULL;

	/* Initilize SCSI Config Data structure
	 */
	memset(&ioc->spi_data, 0, sizeof(SpiCfgData));

	/* Initialize the fc rport list head.
	 */
	INIT_LIST_HEAD(&ioc->fc_rports);

	/* Find lookup slot. */
	INIT_LIST_HEAD(&ioc->list);


	/* Initialize workqueue */
	INIT_DELAYED_WORK(&ioc->fault_reset_work, mpt_fault_reset_work);

	snprintf(ioc->reset_work_q_name, MPT_KOBJ_NAME_LEN,
		 "mpt_poll_%d", ioc->id);
	ioc->reset_work_q =
		create_singlethread_workqueue(ioc->reset_work_q_name);
	if (!ioc->reset_work_q) {
		printk(MYIOC_s_ERR_FMT "Insufficient memory to add adapter!\n",
		    ioc->name);
		pci_release_selected_regions(pdev, ioc->bars);
		kfree(ioc);
		return -ENOMEM;
	}

	dinitprintk(ioc, printk(MYIOC_s_INFO_FMT "facts @ %p, pfacts[0] @ %p\n",
	    ioc->name, &ioc->facts, &ioc->pfacts[0]));

	pci_read_config_byte(pdev, PCI_CLASS_REVISION, &revision);
	mpt_get_product_name(pdev->vendor, pdev->device, revision, ioc->prod_name);

	switch (pdev->device)
	{
	case MPI_MANUFACTPAGE_DEVICEID_FC939X:
	case MPI_MANUFACTPAGE_DEVICEID_FC949X:
		ioc->errata_flag_1064 = 1;
	case MPI_MANUFACTPAGE_DEVICEID_FC909:
	case MPI_MANUFACTPAGE_DEVICEID_FC929:
	case MPI_MANUFACTPAGE_DEVICEID_FC919:
	case MPI_MANUFACTPAGE_DEVICEID_FC949E:
		ioc->bus_type = FC;
		break;

	case MPI_MANUFACTPAGE_DEVICEID_FC929X:
		if (revision < XL_929) {
			/* 929X Chip Fix. Set Split transactions level
		 	* for PCIX. Set MOST bits to zero.
		 	*/
			pci_read_config_byte(pdev, 0x6a, &pcixcmd);
			pcixcmd &= 0x8F;
			pci_write_config_byte(pdev, 0x6a, pcixcmd);
		} else {
			/* 929XL Chip Fix. Set MMRBC to 0x08.
		 	*/
			pci_read_config_byte(pdev, 0x6a, &pcixcmd);
			pcixcmd |= 0x08;
			pci_write_config_byte(pdev, 0x6a, pcixcmd);
		}
		ioc->bus_type = FC;
		break;

	case MPI_MANUFACTPAGE_DEVICEID_FC919X:
		/* 919X Chip Fix. Set Split transactions level
		 * for PCIX. Set MOST bits to zero.
		 */
		pci_read_config_byte(pdev, 0x6a, &pcixcmd);
		pcixcmd &= 0x8F;
		pci_write_config_byte(pdev, 0x6a, pcixcmd);
		ioc->bus_type = FC;
		break;

	case MPI_MANUFACTPAGE_DEVID_53C1030:
		/* 1030 Chip Fix. Disable Split transactions
		 * for PCIX. Set MOST bits to zero if Rev < C0( = 8).
		 */
		if (revision < C0_1030) {
			pci_read_config_byte(pdev, 0x6a, &pcixcmd);
			pcixcmd &= 0x8F;
			pci_write_config_byte(pdev, 0x6a, pcixcmd);
		}

	case MPI_MANUFACTPAGE_DEVID_1030_53C1035:
		ioc->bus_type = SPI;
		break;

	case MPI_MANUFACTPAGE_DEVID_SAS1064:
	case MPI_MANUFACTPAGE_DEVID_SAS1068:
		ioc->errata_flag_1064 = 1;
		ioc->bus_type = SAS;
		break;

	case MPI_MANUFACTPAGE_DEVID_SAS1064E:
	case MPI_MANUFACTPAGE_DEVID_SAS1068E:
	case MPI_MANUFACTPAGE_DEVID_SAS1078:
		ioc->bus_type = SAS;
		break;
	}


	switch (ioc->bus_type) {

	case SAS:
		ioc->msi_enable = mpt_msi_enable_sas;
		break;

	case SPI:
		ioc->msi_enable = mpt_msi_enable_spi;
		break;

	case FC:
		ioc->msi_enable = mpt_msi_enable_fc;
		break;

	default:
		ioc->msi_enable = 0;
		break;
	}
	if (ioc->errata_flag_1064)
		pci_disable_io_access(pdev);

	spin_lock_init(&ioc->FreeQlock);

	/* Disable all! */
	CHIPREG_WRITE32(&ioc->chip->IntMask, 0xFFFFFFFF);
	ioc->active = 0;
	CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);

	/* Set IOC ptr in the pcidev's driver data. */
	pci_set_drvdata(ioc->pcidev, ioc);

	/* Set lookup ptr. */
	list_add_tail(&ioc->list, &ioc_list);

	/* Check for "bound ports" (929, 929X, 1030, 1035) to reduce redundant resets.
	 */
	mpt_detect_bound_ports(ioc, pdev);

	INIT_LIST_HEAD(&ioc->fw_event_list);
	spin_lock_init(&ioc->fw_event_lock);
	snprintf(ioc->fw_event_q_name, MPT_KOBJ_NAME_LEN, "mpt/%d", ioc->id);
	ioc->fw_event_q = create_singlethread_workqueue(ioc->fw_event_q_name);

	if ((r = mpt_do_ioc_recovery(ioc, MPT_HOSTEVENT_IOC_BRINGUP,
	    CAN_SLEEP)) != 0){
		printk(MYIOC_s_ERR_FMT "didn't initialize properly! (%d)\n",
		    ioc->name, r);

		list_del(&ioc->list);
		if (ioc->alt_ioc)
			ioc->alt_ioc->alt_ioc = NULL;
		iounmap(ioc->memmap);
		if (r != -5)
			pci_release_selected_regions(pdev, ioc->bars);

		destroy_workqueue(ioc->reset_work_q);
		ioc->reset_work_q = NULL;

		kfree(ioc);
		pci_set_drvdata(pdev, NULL);
		return r;
	}

	/* call per device driver probe entry point */
	for(cb_idx = 0; cb_idx < MPT_MAX_PROTOCOL_DRIVERS; cb_idx++) {
		if(MptDeviceDriverHandlers[cb_idx] &&
		  MptDeviceDriverHandlers[cb_idx]->probe) {
			MptDeviceDriverHandlers[cb_idx]->probe(pdev,id);
		}
	}

#ifdef CONFIG_PROC_FS
	/*
	 *  Create "/proc/mpt/iocN" subdirectory entry for each MPT adapter.
	 */
	dent = proc_mkdir(ioc->name, mpt_proc_root_dir);
	if (dent) {
		ent = create_proc_entry("info", S_IFREG|S_IRUGO, dent);
		if (ent) {
			ent->read_proc = procmpt_iocinfo_read;
			ent->data = ioc;
		}
		ent = create_proc_entry("summary", S_IFREG|S_IRUGO, dent);
		if (ent) {
			ent->read_proc = procmpt_summary_read;
			ent->data = ioc;
		}
	}
#endif

	if (!ioc->alt_ioc)
		queue_delayed_work(ioc->reset_work_q, &ioc->fault_reset_work,
			msecs_to_jiffies(MPT_POLLING_INTERVAL));

	return 0;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_detach - Remove a PCI intelligent MPT adapter.
 *	@pdev: Pointer to pci_dev structure
 */

void
mpt_detach(struct pci_dev *pdev)
{
	MPT_ADAPTER 	*ioc = pci_get_drvdata(pdev);
	char pname[32];
	u8 cb_idx;
	unsigned long flags;
	struct workqueue_struct *wq;

	/*
	 * Stop polling ioc for fault condition
	 */
	spin_lock_irqsave(&ioc->taskmgmt_lock, flags);
	wq = ioc->reset_work_q;
	ioc->reset_work_q = NULL;
	spin_unlock_irqrestore(&ioc->taskmgmt_lock, flags);
	cancel_delayed_work(&ioc->fault_reset_work);
	destroy_workqueue(wq);

	spin_lock_irqsave(&ioc->fw_event_lock, flags);
	wq = ioc->fw_event_q;
	ioc->fw_event_q = NULL;
	spin_unlock_irqrestore(&ioc->fw_event_lock, flags);
	destroy_workqueue(wq);

	sprintf(pname, MPT_PROCFS_MPTBASEDIR "/%s/summary", ioc->name);
	remove_proc_entry(pname, NULL);
	sprintf(pname, MPT_PROCFS_MPTBASEDIR "/%s/info", ioc->name);
	remove_proc_entry(pname, NULL);
	sprintf(pname, MPT_PROCFS_MPTBASEDIR "/%s", ioc->name);
	remove_proc_entry(pname, NULL);

	/* call per device driver remove entry point */
	for(cb_idx = 0; cb_idx < MPT_MAX_PROTOCOL_DRIVERS; cb_idx++) {
		if(MptDeviceDriverHandlers[cb_idx] &&
		  MptDeviceDriverHandlers[cb_idx]->remove) {
			MptDeviceDriverHandlers[cb_idx]->remove(pdev);
		}
	}

	/* Disable interrupts! */
	CHIPREG_WRITE32(&ioc->chip->IntMask, 0xFFFFFFFF);

	ioc->active = 0;
	synchronize_irq(pdev->irq);

	/* Clear any lingering interrupt */
	CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);

	CHIPREG_READ32(&ioc->chip->IntStatus);

	mpt_adapter_dispose(ioc);

	pci_set_drvdata(pdev, NULL);
}

/**************************************************************************
 * Power Management
 */
#ifdef CONFIG_PM
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_suspend - Fusion MPT base driver suspend routine.
 *	@pdev: Pointer to pci_dev structure
 *	@state: new state to enter
 */
int
mpt_suspend(struct pci_dev *pdev, pm_message_t state)
{
	u32 device_state;
	MPT_ADAPTER *ioc = pci_get_drvdata(pdev);

	device_state = pci_choose_state(pdev, state);
	printk(MYIOC_s_INFO_FMT "pci-suspend: pdev=0x%p, slot=%s, Entering "
	    "operating state [D%d]\n", ioc->name, pdev, pci_name(pdev),
	    device_state);

	/* put ioc into READY_STATE */
	if(SendIocReset(ioc, MPI_FUNCTION_IOC_MESSAGE_UNIT_RESET, CAN_SLEEP)) {
		printk(MYIOC_s_ERR_FMT
		"pci-suspend:  IOC msg unit reset failed!\n", ioc->name);
	}

	/* disable interrupts */
	CHIPREG_WRITE32(&ioc->chip->IntMask, 0xFFFFFFFF);
	ioc->active = 0;

	/* Clear any lingering interrupt */
	CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);

	free_irq(ioc->pci_irq, ioc);
	if (ioc->msi_enable)
		pci_disable_msi(ioc->pcidev);
	ioc->pci_irq = -1;
	pci_save_state(pdev);
	pci_disable_device(pdev);
	pci_release_selected_regions(pdev, ioc->bars);
	pci_set_power_state(pdev, device_state);
	return 0;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_resume - Fusion MPT base driver resume routine.
 *	@pdev: Pointer to pci_dev structure
 */
int
mpt_resume(struct pci_dev *pdev)
{
	MPT_ADAPTER *ioc = pci_get_drvdata(pdev);
	u32 device_state = pdev->current_state;
	int recovery_state;
	int err;

	printk(MYIOC_s_INFO_FMT "pci-resume: pdev=0x%p, slot=%s, Previous "
	    "operating state [D%d]\n", ioc->name, pdev, pci_name(pdev),
	    device_state);

	pci_set_power_state(pdev, PCI_D0);
	pci_enable_wake(pdev, PCI_D0, 0);
	pci_restore_state(pdev);
	ioc->pcidev = pdev;
	err = mpt_mapresources(ioc);
	if (err)
		return err;

	if (ioc->dma_mask == DMA_BIT_MASK(64)) {
		if (pdev->device == MPI_MANUFACTPAGE_DEVID_SAS1078)
			ioc->add_sge = &mpt_add_sge_64bit_1078;
		else
			ioc->add_sge = &mpt_add_sge_64bit;
		ioc->add_chain = &mpt_add_chain_64bit;
		ioc->sg_addr_size = 8;
	} else {

		ioc->add_sge = &mpt_add_sge;
		ioc->add_chain = &mpt_add_chain;
		ioc->sg_addr_size = 4;
	}
	ioc->SGE_size = sizeof(u32) + ioc->sg_addr_size;

	printk(MYIOC_s_INFO_FMT "pci-resume: ioc-state=0x%x,doorbell=0x%x\n",
	    ioc->name, (mpt_GetIocState(ioc, 1) >> MPI_IOC_STATE_SHIFT),
	    CHIPREG_READ32(&ioc->chip->Doorbell));

	/*
	 * Errata workaround for SAS pci express:
	 * Upon returning to the D0 state, the contents of the doorbell will be
	 * stale data, and this will incorrectly signal to the host driver that
	 * the firmware is ready to process mpt commands.   The workaround is
	 * to issue a diagnostic reset.
	 */
	if (ioc->bus_type == SAS && (pdev->device ==
	    MPI_MANUFACTPAGE_DEVID_SAS1068E || pdev->device ==
	    MPI_MANUFACTPAGE_DEVID_SAS1064E)) {
		if (KickStart(ioc, 1, CAN_SLEEP) < 0) {
			printk(MYIOC_s_WARN_FMT "pci-resume: Cannot recover\n",
			    ioc->name);
			goto out;
		}
	}

	/* bring ioc to operational state */
	printk(MYIOC_s_INFO_FMT "Sending mpt_do_ioc_recovery\n", ioc->name);
	recovery_state = mpt_do_ioc_recovery(ioc, MPT_HOSTEVENT_IOC_BRINGUP,
						 CAN_SLEEP);
	if (recovery_state != 0)
		printk(MYIOC_s_WARN_FMT "pci-resume: Cannot recover, "
		    "error:[%x]\n", ioc->name, recovery_state);
	else
		printk(MYIOC_s_INFO_FMT
		    "pci-resume: success\n", ioc->name);
 out:
	return 0;

}
#endif

static int
mpt_signal_reset(u8 index, MPT_ADAPTER *ioc, int reset_phase)
{
	if ((MptDriverClass[index] == MPTSPI_DRIVER &&
	     ioc->bus_type != SPI) ||
	    (MptDriverClass[index] == MPTFC_DRIVER &&
	     ioc->bus_type != FC) ||
	    (MptDriverClass[index] == MPTSAS_DRIVER &&
	     ioc->bus_type != SAS))
		/* make sure we only call the relevant reset handler
		 * for the bus */
		return 0;
	return (MptResetHandlers[index])(ioc, reset_phase);
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_do_ioc_recovery - Initialize or recover MPT adapter.
 *	@ioc: Pointer to MPT adapter structure
 *	@reason: Event word / reason
 *	@sleepFlag: Use schedule if CAN_SLEEP else use udelay.
 *
 *	This routine performs all the steps necessary to bring the IOC
 *	to a OPERATIONAL state.
 *
 *	This routine also pre-fetches the LAN MAC address of a Fibre Channel
 *	MPT adapter.
 *
 *	Returns:
 *		 0 for success
 *		-1 if failed to get board READY
 *		-2 if READY but IOCFacts Failed
 *		-3 if READY but PrimeIOCFifos Failed
 *		-4 if READY but IOCInit Failed
 *		-5 if failed to enable_device and/or request_selected_regions
 *		-6 if failed to upload firmware
 */
static int
mpt_do_ioc_recovery(MPT_ADAPTER *ioc, u32 reason, int sleepFlag)
{
	int	 hard_reset_done = 0;
	int	 alt_ioc_ready = 0;
	int	 hard;
	int	 rc=0;
	int	 ii;
	int	 ret = 0;
	int	 reset_alt_ioc_active = 0;
	int	 irq_allocated = 0;
	u8	*a;

	printk(MYIOC_s_INFO_FMT "Initiating %s\n", ioc->name,
	    reason == MPT_HOSTEVENT_IOC_BRINGUP ? "bringup" : "recovery");

	/* Disable reply interrupts (also blocks FreeQ) */
	CHIPREG_WRITE32(&ioc->chip->IntMask, 0xFFFFFFFF);
	ioc->active = 0;

	if (ioc->alt_ioc) {
		if (ioc->alt_ioc->active ||
		    reason == MPT_HOSTEVENT_IOC_RECOVER) {
			reset_alt_ioc_active = 1;
			/* Disable alt-IOC's reply interrupts
			 *  (and FreeQ) for a bit
			 **/
			CHIPREG_WRITE32(&ioc->alt_ioc->chip->IntMask,
				0xFFFFFFFF);
			ioc->alt_ioc->active = 0;
		}
	}

	hard = 1;
	if (reason == MPT_HOSTEVENT_IOC_BRINGUP)
		hard = 0;

	if ((hard_reset_done = MakeIocReady(ioc, hard, sleepFlag)) < 0) {
		if (hard_reset_done == -4) {
			printk(MYIOC_s_WARN_FMT "Owned by PEER..skipping!\n",
			    ioc->name);

			if (reset_alt_ioc_active && ioc->alt_ioc) {
				/* (re)Enable alt-IOC! (reply interrupt, FreeQ) */
				dprintk(ioc, printk(MYIOC_s_INFO_FMT
				    "alt_ioc reply irq re-enabled\n", ioc->alt_ioc->name));
				CHIPREG_WRITE32(&ioc->alt_ioc->chip->IntMask, MPI_HIM_DIM);
				ioc->alt_ioc->active = 1;
			}

		} else {
			printk(MYIOC_s_WARN_FMT
			    "NOT READY WARNING!\n", ioc->name);
		}
		ret = -1;
		goto out;
	}

	/* hard_reset_done = 0 if a soft reset was performed
	 * and 1 if a hard reset was performed.
	 */
	if (hard_reset_done && reset_alt_ioc_active && ioc->alt_ioc) {
		if ((rc = MakeIocReady(ioc->alt_ioc, 0, sleepFlag)) == 0)
			alt_ioc_ready = 1;
		else
			printk(MYIOC_s_WARN_FMT
			    ": alt-ioc Not ready WARNING!\n",
			    ioc->alt_ioc->name);
	}

	for (ii=0; ii<5; ii++) {
		/* Get IOC facts! Allow 5 retries */
		if ((rc = GetIocFacts(ioc, sleepFlag, reason)) == 0)
			break;
	}


	if (ii == 5) {
		dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "Retry IocFacts failed rc=%x\n", ioc->name, rc));
		ret = -2;
	} else if (reason == MPT_HOSTEVENT_IOC_BRINGUP) {
		MptDisplayIocCapabilities(ioc);
	}

	if (alt_ioc_ready) {
		if ((rc = GetIocFacts(ioc->alt_ioc, sleepFlag, reason)) != 0) {
			dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT
			    "Initial Alt IocFacts failed rc=%x\n",
			    ioc->name, rc));
			/* Retry - alt IOC was initialized once
			 */
			rc = GetIocFacts(ioc->alt_ioc, sleepFlag, reason);
		}
		if (rc) {
			dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT
			    "Retry Alt IocFacts failed rc=%x\n", ioc->name, rc));
			alt_ioc_ready = 0;
			reset_alt_ioc_active = 0;
		} else if (reason == MPT_HOSTEVENT_IOC_BRINGUP) {
			MptDisplayIocCapabilities(ioc->alt_ioc);
		}
	}

	if ((ret == 0) && (reason == MPT_HOSTEVENT_IOC_BRINGUP) &&
	    (ioc->facts.Flags & MPI_IOCFACTS_FLAGS_FW_DOWNLOAD_BOOT)) {
		pci_release_selected_regions(ioc->pcidev, ioc->bars);
		ioc->bars = pci_select_bars(ioc->pcidev, IORESOURCE_MEM |
		    IORESOURCE_IO);
		if (pci_enable_device(ioc->pcidev))
			return -5;
		if (pci_request_selected_regions(ioc->pcidev, ioc->bars,
			"mpt"))
			return -5;
	}

	/*
	 * Device is reset now. It must have de-asserted the interrupt line
	 * (if it was asserted) and it should be safe to register for the
	 * interrupt now.
	 */
	if ((ret == 0) && (reason == MPT_HOSTEVENT_IOC_BRINGUP)) {
		ioc->pci_irq = -1;
		if (ioc->pcidev->irq) {
			if (ioc->msi_enable && !pci_enable_msi(ioc->pcidev))
				printk(MYIOC_s_INFO_FMT "PCI-MSI enabled\n",
				    ioc->name);
			else
				ioc->msi_enable = 0;
			rc = request_irq(ioc->pcidev->irq, mpt_interrupt,
			    IRQF_SHARED, ioc->name, ioc);
			if (rc < 0) {
				printk(MYIOC_s_ERR_FMT "Unable to allocate "
				    "interrupt %d!\n",
				    ioc->name, ioc->pcidev->irq);
				if (ioc->msi_enable)
					pci_disable_msi(ioc->pcidev);
				ret = -EBUSY;
				goto out;
			}
			irq_allocated = 1;
			ioc->pci_irq = ioc->pcidev->irq;
			pci_set_master(ioc->pcidev);		/* ?? */
			pci_set_drvdata(ioc->pcidev, ioc);
			dinitprintk(ioc, printk(MYIOC_s_INFO_FMT
			    "installed at interrupt %d\n", ioc->name,
			    ioc->pcidev->irq));
		}
	}

	/* Prime reply & request queues!
	 * (mucho alloc's) Must be done prior to
	 * init as upper addresses are needed for init.
	 * If fails, continue with alt-ioc processing
	 */
	dinitprintk(ioc, printk(MYIOC_s_INFO_FMT "PrimeIocFifos\n",
	    ioc->name));
	if ((ret == 0) && ((rc = PrimeIocFifos(ioc)) != 0))
		ret = -3;

	/* May need to check/upload firmware & data here!
	 * If fails, continue with alt-ioc processing
	 */
	dinitprintk(ioc, printk(MYIOC_s_INFO_FMT "SendIocInit\n",
	    ioc->name));
	if ((ret == 0) && ((rc = SendIocInit(ioc, sleepFlag)) != 0))
		ret = -4;
// NEW!
	if (alt_ioc_ready && ((rc = PrimeIocFifos(ioc->alt_ioc)) != 0)) {
		printk(MYIOC_s_WARN_FMT
		    ": alt-ioc (%d) FIFO mgmt alloc WARNING!\n",
		    ioc->alt_ioc->name, rc);
		alt_ioc_ready = 0;
		reset_alt_ioc_active = 0;
	}

	if (alt_ioc_ready) {
		if ((rc = SendIocInit(ioc->alt_ioc, sleepFlag)) != 0) {
			alt_ioc_ready = 0;
			reset_alt_ioc_active = 0;
			printk(MYIOC_s_WARN_FMT
				": alt-ioc: (%d) init failure WARNING!\n",
					ioc->alt_ioc->name, rc);
		}
	}

	if (reason == MPT_HOSTEVENT_IOC_BRINGUP){
		if (ioc->upload_fw) {
			ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT
			    "firmware upload required!\n", ioc->name));

			/* Controller is not operational, cannot do upload
			 */
			if (ret == 0) {
				rc = mpt_do_upload(ioc, sleepFlag);
				if (rc == 0) {
					if (ioc->alt_ioc && ioc->alt_ioc->cached_fw) {
						/*
						 * Maintain only one pointer to FW memory
						 * so there will not be two attempt to
						 * downloadboot onboard dual function
						 * chips (mpt_adapter_disable,
						 * mpt_diag_reset)
						 */
						ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT
						    "mpt_upload:  alt_%s has cached_fw=%p \n",
						    ioc->name, ioc->alt_ioc->name, ioc->alt_ioc->cached_fw));
						ioc->cached_fw = NULL;
					}
				} else {
					printk(MYIOC_s_WARN_FMT
					    "firmware upload failure!\n", ioc->name);
					ret = -6;
				}
			}
		}
	}

	/*  Enable MPT base driver management of EventNotification
	 *  and EventAck handling.
	 */
	if ((ret == 0) && (!ioc->facts.EventState)) {
		dinitprintk(ioc, printk(MYIOC_s_INFO_FMT
			"SendEventNotification\n",
		    ioc->name));
		ret = SendEventNotification(ioc, 1, sleepFlag);	/* 1=Enable */
	}

	if (ioc->alt_ioc && alt_ioc_ready && !ioc->alt_ioc->facts.EventState)
		rc = SendEventNotification(ioc->alt_ioc, 1, sleepFlag);

	if (ret == 0) {
		/* Enable! (reply interrupt) */
		CHIPREG_WRITE32(&ioc->chip->IntMask, MPI_HIM_DIM);
		ioc->active = 1;
	}
	if (rc == 0) {	/* alt ioc */
		if (reset_alt_ioc_active && ioc->alt_ioc) {
			/* (re)Enable alt-IOC! (reply interrupt) */
			dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "alt-ioc"
				"reply irq re-enabled\n",
				ioc->alt_ioc->name));
			CHIPREG_WRITE32(&ioc->alt_ioc->chip->IntMask,
				MPI_HIM_DIM);
			ioc->alt_ioc->active = 1;
		}
	}


	/*	Add additional "reason" check before call to GetLanConfigPages
	 *	(combined with GetIoUnitPage2 call).  This prevents a somewhat
	 *	recursive scenario; GetLanConfigPages times out, timer expired
	 *	routine calls HardResetHandler, which calls into here again,
	 *	and we try GetLanConfigPages again...
	 */
	if ((ret == 0) && (reason == MPT_HOSTEVENT_IOC_BRINGUP)) {

		/*
		 * Initalize link list for inactive raid volumes.
		 */
		mutex_init(&ioc->raid_data.inactive_list_mutex);
		INIT_LIST_HEAD(&ioc->raid_data.inactive_list);

		switch (ioc->bus_type) {

		case SAS:
			/* clear persistency table */
			if(ioc->facts.IOCExceptions &
			    MPI_IOCFACTS_EXCEPT_PERSISTENT_TABLE_FULL) {
				ret = mptbase_sas_persist_operation(ioc,
				    MPI_SAS_OP_CLEAR_NOT_PRESENT);
				if(ret != 0)
					goto out;
			}

			/* Find IM volumes
			 */
			mpt_findImVolumes(ioc);

			/* Check, and possibly reset, the coalescing value
			 */
			mpt_read_ioc_pg_1(ioc);

			break;

		case FC:
			if ((ioc->pfacts[0].ProtocolFlags &
				MPI_PORTFACTS_PROTOCOL_LAN) &&
			    (ioc->lan_cnfg_page0.Header.PageLength == 0)) {
				/*
				 *  Pre-fetch the ports LAN MAC address!
				 *  (LANPage1_t stuff)
				 */
				(void) GetLanConfigPages(ioc);
				a = (u8*)&ioc->lan_cnfg_page1.HardwareAddressLow;
				dprintk(ioc, printk(MYIOC_s_DEBUG_FMT
					"LanAddr = %02X:%02X:%02X"
					":%02X:%02X:%02X\n",
					ioc->name, a[5], a[4],
					a[3], a[2], a[1], a[0]));
			}
			break;

		case SPI:
			/* Get NVRAM and adapter maximums from SPP 0 and 2
			 */
			mpt_GetScsiPortSettings(ioc, 0);

			/* Get version and length of SDP 1
			 */
			mpt_readScsiDevicePageHeaders(ioc, 0);

			/* Find IM volumes
			 */
			if (ioc->facts.MsgVersion >= MPI_VERSION_01_02)
				mpt_findImVolumes(ioc);

			/* Check, and possibly reset, the coalescing value
			 */
			mpt_read_ioc_pg_1(ioc);

			mpt_read_ioc_pg_4(ioc);

			break;
		}

		GetIoUnitPage2(ioc);
		mpt_get_manufacturing_pg_0(ioc);
	}

 out:
	if ((ret != 0) && irq_allocated) {
		free_irq(ioc->pci_irq, ioc);
		if (ioc->msi_enable)
			pci_disable_msi(ioc->pcidev);
	}
	return ret;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_detect_bound_ports - Search for matching PCI bus/dev_function
 *	@ioc: Pointer to MPT adapter structure
 *	@pdev: Pointer to (struct pci_dev) structure
 *
 *	Search for PCI bus/dev_function which matches
 *	PCI bus/dev_function (+/-1) for newly discovered 929,
 *	929X, 1030 or 1035.
 *
 *	If match on PCI dev_function +/-1 is found, bind the two MPT adapters
 *	using alt_ioc pointer fields in their %MPT_ADAPTER structures.
 */
static void
mpt_detect_bound_ports(MPT_ADAPTER *ioc, struct pci_dev *pdev)
{
	struct pci_dev *peer=NULL;
	unsigned int slot = PCI_SLOT(pdev->devfn);
	unsigned int func = PCI_FUNC(pdev->devfn);
	MPT_ADAPTER *ioc_srch;

	dprintk(ioc, printk(MYIOC_s_DEBUG_FMT "PCI device %s devfn=%x/%x,"
	    " searching for devfn match on %x or %x\n",
	    ioc->name, pci_name(pdev), pdev->bus->number,
	    pdev->devfn, func-1, func+1));

	peer = pci_get_slot(pdev->bus, PCI_DEVFN(slot,func-1));
	if (!peer) {
		peer = pci_get_slot(pdev->bus, PCI_DEVFN(slot,func+1));
		if (!peer)
			return;
	}

	list_for_each_entry(ioc_srch, &ioc_list, list) {
		struct pci_dev *_pcidev = ioc_srch->pcidev;
		if (_pcidev == peer) {
			/* Paranoia checks */
			if (ioc->alt_ioc != NULL) {
				printk(MYIOC_s_WARN_FMT
				    "Oops, already bound (%s <==> %s)!\n",
				    ioc->name, ioc->name, ioc->alt_ioc->name);
				break;
			} else if (ioc_srch->alt_ioc != NULL) {
				printk(MYIOC_s_WARN_FMT
				    "Oops, already bound (%s <==> %s)!\n",
				    ioc_srch->name, ioc_srch->name,
				    ioc_srch->alt_ioc->name);
				break;
			}
			dprintk(ioc, printk(MYIOC_s_DEBUG_FMT
				"FOUND! binding %s <==> %s\n",
				ioc->name, ioc->name, ioc_srch->name));
			ioc_srch->alt_ioc = ioc;
			ioc->alt_ioc = ioc_srch;
		}
	}
	pci_dev_put(peer);
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_adapter_disable - Disable misbehaving MPT adapter.
 *	@ioc: Pointer to MPT adapter structure
 */
static void
mpt_adapter_disable(MPT_ADAPTER *ioc)
{
	int sz;
	int ret;

	if (ioc->cached_fw != NULL) {
		ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT
			"%s: Pushing FW onto adapter\n", __func__, ioc->name));
		if ((ret = mpt_downloadboot(ioc, (MpiFwHeader_t *)
		    ioc->cached_fw, CAN_SLEEP)) < 0) {
			printk(MYIOC_s_WARN_FMT
			    ": firmware downloadboot failure (%d)!\n",
			    ioc->name, ret);
		}
	}

	/*
	 * Put the controller into ready state (if its not already)
	 */
	if (mpt_GetIocState(ioc, 1) != MPI_IOC_STATE_READY) {
		if (!SendIocReset(ioc, MPI_FUNCTION_IOC_MESSAGE_UNIT_RESET,
		    CAN_SLEEP)) {
			if (mpt_GetIocState(ioc, 1) != MPI_IOC_STATE_READY)
				printk(MYIOC_s_ERR_FMT "%s:  IOC msg unit "
				    "reset failed to put ioc in ready state!\n",
				    ioc->name, __func__);
		} else
			printk(MYIOC_s_ERR_FMT "%s:  IOC msg unit reset "
			    "failed!\n", ioc->name, __func__);
	}


	/* Disable adapter interrupts! */
	synchronize_irq(ioc->pcidev->irq);
	CHIPREG_WRITE32(&ioc->chip->IntMask, 0xFFFFFFFF);
	ioc->active = 0;

	/* Clear any lingering interrupt */
	CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);
	CHIPREG_READ32(&ioc->chip->IntStatus);

	if (ioc->alloc != NULL) {
		sz = ioc->alloc_sz;
		dexitprintk(ioc, printk(MYIOC_s_INFO_FMT "free  @ %p, sz=%d bytes\n",
		    ioc->name, ioc->alloc, ioc->alloc_sz));
		pci_free_consistent(ioc->pcidev, sz,
				ioc->alloc, ioc->alloc_dma);
		ioc->reply_frames = NULL;
		ioc->req_frames = NULL;
		ioc->alloc = NULL;
		ioc->alloc_total -= sz;
	}

	if (ioc->sense_buf_pool != NULL) {
		sz = (ioc->req_depth * MPT_SENSE_BUFFER_ALLOC);
		pci_free_consistent(ioc->pcidev, sz,
				ioc->sense_buf_pool, ioc->sense_buf_pool_dma);
		ioc->sense_buf_pool = NULL;
		ioc->alloc_total -= sz;
	}

	if (ioc->events != NULL){
		sz = MPTCTL_EVENT_LOG_SIZE * sizeof(MPT_IOCTL_EVENTS);
		kfree(ioc->events);
		ioc->events = NULL;
		ioc->alloc_total -= sz;
	}

	mpt_free_fw_memory(ioc);

	kfree(ioc->spi_data.nvram);
	mpt_inactive_raid_list_free(ioc);
	kfree(ioc->raid_data.pIocPg2);
	kfree(ioc->raid_data.pIocPg3);
	ioc->spi_data.nvram = NULL;
	ioc->raid_data.pIocPg3 = NULL;

	if (ioc->spi_data.pIocPg4 != NULL) {
		sz = ioc->spi_data.IocPg4Sz;
		pci_free_consistent(ioc->pcidev, sz,
			ioc->spi_data.pIocPg4,
			ioc->spi_data.IocPg4_dma);
		ioc->spi_data.pIocPg4 = NULL;
		ioc->alloc_total -= sz;
	}

	if (ioc->ReqToChain != NULL) {
		kfree(ioc->ReqToChain);
		kfree(ioc->RequestNB);
		ioc->ReqToChain = NULL;
	}

	kfree(ioc->ChainToChain);
	ioc->ChainToChain = NULL;

	if (ioc->HostPageBuffer != NULL) {
		if((ret = mpt_host_page_access_control(ioc,
		    MPI_DB_HPBAC_FREE_BUFFER, NO_SLEEP)) != 0) {
			printk(MYIOC_s_ERR_FMT
			   ": %s: host page buffers free failed (%d)!\n",
			    ioc->name, __func__, ret);
		}
		dexitprintk(ioc, printk(MYIOC_s_DEBUG_FMT
			"HostPageBuffer free  @ %p, sz=%d bytes\n",
			ioc->name, ioc->HostPageBuffer,
			ioc->HostPageBuffer_sz));
		pci_free_consistent(ioc->pcidev, ioc->HostPageBuffer_sz,
		    ioc->HostPageBuffer, ioc->HostPageBuffer_dma);
		ioc->HostPageBuffer = NULL;
		ioc->HostPageBuffer_sz = 0;
		ioc->alloc_total -= ioc->HostPageBuffer_sz;
	}

	pci_set_drvdata(ioc->pcidev, NULL);
}
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_adapter_dispose - Free all resources associated with an MPT adapter
 *	@ioc: Pointer to MPT adapter structure
 *
 *	This routine unregisters h/w resources and frees all alloc'd memory
 *	associated with a MPT adapter structure.
 */
static void
mpt_adapter_dispose(MPT_ADAPTER *ioc)
{
	int sz_first, sz_last;

	if (ioc == NULL)
		return;

	sz_first = ioc->alloc_total;

	mpt_adapter_disable(ioc);

	if (ioc->pci_irq != -1) {
		free_irq(ioc->pci_irq, ioc);
		if (ioc->msi_enable)
			pci_disable_msi(ioc->pcidev);
		ioc->pci_irq = -1;
	}

	if (ioc->memmap != NULL) {
		iounmap(ioc->memmap);
		ioc->memmap = NULL;
	}

	pci_disable_device(ioc->pcidev);
	pci_release_selected_regions(ioc->pcidev, ioc->bars);

#if defined(CONFIG_MTRR) && 0
	if (ioc->mtrr_reg > 0) {
		mtrr_del(ioc->mtrr_reg, 0, 0);
		dprintk(ioc, printk(MYIOC_s_INFO_FMT "MTRR region de-registered\n", ioc->name));
	}
#endif

	/*  Zap the adapter lookup ptr!  */
	list_del(&ioc->list);

	sz_last = ioc->alloc_total;
	dprintk(ioc, printk(MYIOC_s_INFO_FMT "free'd %d of %d bytes\n",
	    ioc->name, sz_first-sz_last+(int)sizeof(*ioc), sz_first));

	if (ioc->alt_ioc)
		ioc->alt_ioc->alt_ioc = NULL;

	kfree(ioc);
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	MptDisplayIocCapabilities - Disply IOC's capabilities.
 *	@ioc: Pointer to MPT adapter structure
 */
static void
MptDisplayIocCapabilities(MPT_ADAPTER *ioc)
{
	int i = 0;

	printk(KERN_INFO "%s: ", ioc->name);
	if (ioc->prod_name)
		printk("%s: ", ioc->prod_name);
	printk("Capabilities={");

	if (ioc->pfacts[0].ProtocolFlags & MPI_PORTFACTS_PROTOCOL_INITIATOR) {
		printk("Initiator");
		i++;
	}

	if (ioc->pfacts[0].ProtocolFlags & MPI_PORTFACTS_PROTOCOL_TARGET) {
		printk("%sTarget", i ? "," : "");
		i++;
	}

	if (ioc->pfacts[0].ProtocolFlags & MPI_PORTFACTS_PROTOCOL_LAN) {
		printk("%sLAN", i ? "," : "");
		i++;
	}

#if 0
	/*
	 *  This would probably evoke more questions than it's worth
	 */
	if (ioc->pfacts[0].ProtocolFlags & MPI_PORTFACTS_PROTOCOL_TARGET) {
		printk("%sLogBusAddr", i ? "," : "");
		i++;
	}
#endif

	printk("}\n");
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	MakeIocReady - Get IOC to a READY state, using KickStart if needed.
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@force: Force hard KickStart of IOC
 *	@sleepFlag: Specifies whether the process can sleep
 *
 *	Returns:
 *		 1 - DIAG reset and READY
 *		 0 - READY initially OR soft reset and READY
 *		-1 - Any failure on KickStart
 *		-2 - Msg Unit Reset Failed
 *		-3 - IO Unit Reset Failed
 *		-4 - IOC owned by a PEER
 */
static int
MakeIocReady(MPT_ADAPTER *ioc, int force, int sleepFlag)
{
	u32	 ioc_state;
	int	 statefault = 0;
	int	 cntdn;
	int	 hard_reset_done = 0;
	int	 r;
	int	 ii;
	int	 whoinit;

	/* Get current [raw] IOC state  */
	ioc_state = mpt_GetIocState(ioc, 0);
	dhsprintk(ioc, printk(MYIOC_s_INFO_FMT "MakeIocReady [raw] state=%08x\n", ioc->name, ioc_state));

	/*
	 *	Check to see if IOC got left/stuck in doorbell handshake
	 *	grip of death.  If so, hard reset the IOC.
	 */
	if (ioc_state & MPI_DOORBELL_ACTIVE) {
		statefault = 1;
		printk(MYIOC_s_WARN_FMT "Unexpected doorbell active!\n",
				ioc->name);
	}

	/* Is it already READY? */
	if (!statefault &&
	    ((ioc_state & MPI_IOC_STATE_MASK) == MPI_IOC_STATE_READY)) {
		dinitprintk(ioc, printk(MYIOC_s_INFO_FMT
		    "IOC is in READY state\n", ioc->name));
		return 0;
	}

	/*
	 *	Check to see if IOC is in FAULT state.
	 */
	if ((ioc_state & MPI_IOC_STATE_MASK) == MPI_IOC_STATE_FAULT) {
		statefault = 2;
		printk(MYIOC_s_WARN_FMT "IOC is in FAULT state!!!\n",
		    ioc->name);
		printk(MYIOC_s_WARN_FMT "           FAULT code = %04xh\n",
		    ioc->name, ioc_state & MPI_DOORBELL_DATA_MASK);
	}

	/*
	 *	Hmmm...  Did it get left operational?
	 */
	if ((ioc_state & MPI_IOC_STATE_MASK) == MPI_IOC_STATE_OPERATIONAL) {
		dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "IOC operational unexpected\n",
				ioc->name));

		/* Check WhoInit.
		 * If PCI Peer, exit.
		 * Else, if no fault conditions are present, issue a MessageUnitReset
		 * Else, fall through to KickStart case
		 */
		whoinit = (ioc_state & MPI_DOORBELL_WHO_INIT_MASK) >> MPI_DOORBELL_WHO_INIT_SHIFT;
		dinitprintk(ioc, printk(MYIOC_s_INFO_FMT
			"whoinit 0x%x statefault %d force %d\n",
			ioc->name, whoinit, statefault, force));
		if (whoinit == MPI_WHOINIT_PCI_PEER)
			return -4;
		else {
			if ((statefault == 0 ) && (force == 0)) {
				if ((r = SendIocReset(ioc, MPI_FUNCTION_IOC_MESSAGE_UNIT_RESET, sleepFlag)) == 0)
					return 0;
			}
			statefault = 3;
		}
	}

	hard_reset_done = KickStart(ioc, statefault||force, sleepFlag);
	if (hard_reset_done < 0)
		return -1;

	/*
	 *  Loop here waiting for IOC to come READY.
	 */
	ii = 0;
	cntdn = ((sleepFlag == CAN_SLEEP) ? HZ : 1000) * 5;	/* 5 seconds */

	while ((ioc_state = mpt_GetIocState(ioc, 1)) != MPI_IOC_STATE_READY) {
		if (ioc_state == MPI_IOC_STATE_OPERATIONAL) {
			/*
			 *  BIOS or previous driver load left IOC in OP state.
			 *  Reset messaging FIFOs.
			 */
			if ((r = SendIocReset(ioc, MPI_FUNCTION_IOC_MESSAGE_UNIT_RESET, sleepFlag)) != 0) {
				printk(MYIOC_s_ERR_FMT "IOC msg unit reset failed!\n", ioc->name);
				return -2;
			}
		} else if (ioc_state == MPI_IOC_STATE_RESET) {
			/*
			 *  Something is wrong.  Try to get IOC back
			 *  to a known state.
			 */
			if ((r = SendIocReset(ioc, MPI_FUNCTION_IO_UNIT_RESET, sleepFlag)) != 0) {
				printk(MYIOC_s_ERR_FMT "IO unit reset failed!\n", ioc->name);
				return -3;
			}
		}

		ii++; cntdn--;
		if (!cntdn) {
			printk(MYIOC_s_ERR_FMT
				"Wait IOC_READY state (0x%x) timeout(%d)!\n",
				ioc->name, ioc_state, (int)((ii+5)/HZ));
			return -ETIME;
		}

		if (sleepFlag == CAN_SLEEP) {
			msleep(1);
		} else {
			mdelay (1);	/* 1 msec delay */
		}

	}

	if (statefault < 3) {
		printk(MYIOC_s_INFO_FMT "Recovered from %s\n", ioc->name,
			statefault == 1 ? "stuck handshake" : "IOC FAULT");
	}

	return hard_reset_done;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_GetIocState - Get the current state of a MPT adapter.
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@cooked: Request raw or cooked IOC state
 *
 *	Returns all IOC Doorbell register bits if cooked==0, else just the
 *	Doorbell bits in MPI_IOC_STATE_MASK.
 */
u32
mpt_GetIocState(MPT_ADAPTER *ioc, int cooked)
{
	u32 s, sc;

	/*  Get!  */
	s = CHIPREG_READ32(&ioc->chip->Doorbell);
	sc = s & MPI_IOC_STATE_MASK;

	/*  Save!  */
	ioc->last_state = sc;

	return cooked ? sc : s;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	GetIocFacts - Send IOCFacts request to MPT adapter.
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@sleepFlag: Specifies whether the process can sleep
 *	@reason: If recovery, only update facts.
 *
 *	Returns 0 for success, non-zero for failure.
 */
static int
GetIocFacts(MPT_ADAPTER *ioc, int sleepFlag, int reason)
{
	IOCFacts_t		 get_facts;
	IOCFactsReply_t		*facts;
	int			 r;
	int			 req_sz;
	int			 reply_sz;
	int			 sz;
	u32			 status, vv;
	u8			 shiftFactor=1;

	/* IOC *must* NOT be in RESET state! */
	if (ioc->last_state == MPI_IOC_STATE_RESET) {
		printk(KERN_ERR MYNAM
		    ": ERROR - Can't get IOCFacts, %s NOT READY! (%08x)\n",
		    ioc->name, ioc->last_state);
		return -44;
	}

	facts = &ioc->facts;

	/* Destination (reply area)... */
	reply_sz = sizeof(*facts);
	memset(facts, 0, reply_sz);

	/* Request area (get_facts on the stack right now!) */
	req_sz = sizeof(get_facts);
	memset(&get_facts, 0, req_sz);

	get_facts.Function = MPI_FUNCTION_IOC_FACTS;
	/* Assert: All other get_facts fields are zero! */

	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT
	    "Sending get IocFacts request req_sz=%d reply_sz=%d\n",
	    ioc->name, req_sz, reply_sz));

	/* No non-zero fields in the get_facts request are greater than
	 * 1 byte in size, so we can just fire it off as is.
	 */
	r = mpt_handshake_req_reply_wait(ioc, req_sz, (u32*)&get_facts,
			reply_sz, (u16*)facts, 5 /*seconds*/, sleepFlag);
	if (r != 0)
		return r;

	/*
	 * Now byte swap (GRRR) the necessary fields before any further
	 * inspection of reply contents.
	 *
	 * But need to do some sanity checks on MsgLength (byte) field
	 * to make sure we don't zero IOC's req_sz!
	 */
	/* Did we get a valid reply? */
	if (facts->MsgLength > offsetof(IOCFactsReply_t, RequestFrameSize)/sizeof(u32)) {
		if (reason == MPT_HOSTEVENT_IOC_BRINGUP) {
			/*
			 * If not been here, done that, save off first WhoInit value
			 */
			if (ioc->FirstWhoInit == WHOINIT_UNKNOWN)
				ioc->FirstWhoInit = facts->WhoInit;
		}

		facts->MsgVersion = le16_to_cpu(facts->MsgVersion);
		facts->MsgContext = le32_to_cpu(facts->MsgContext);
		facts->IOCExceptions = le16_to_cpu(facts->IOCExceptions);
		facts->IOCStatus = le16_to_cpu(facts->IOCStatus);
		facts->IOCLogInfo = le32_to_cpu(facts->IOCLogInfo);
		status = le16_to_cpu(facts->IOCStatus) & MPI_IOCSTATUS_MASK;
		/* CHECKME! IOCStatus, IOCLogInfo */

		facts->ReplyQueueDepth = le16_to_cpu(facts->ReplyQueueDepth);
		facts->RequestFrameSize = le16_to_cpu(facts->RequestFrameSize);

		/*
		 * FC f/w version changed between 1.1 and 1.2
		 *	Old: u16{Major(4),Minor(4),SubMinor(8)}
		 *	New: u32{Major(8),Minor(8),Unit(8),Dev(8)}
		 */
		if (facts->MsgVersion < MPI_VERSION_01_02) {
			/*
			 *	Handle old FC f/w style, convert to new...
			 */
			u16	 oldv = le16_to_cpu(facts->Reserved_0101_FWVersion);
			facts->FWVersion.Word =
					((oldv<<12) & 0xFF000000) |
					((oldv<<8)  & 0x000FFF00);
		} else
			facts->FWVersion.Word = le32_to_cpu(facts->FWVersion.Word);

		facts->ProductID = le16_to_cpu(facts->ProductID);

		if ((ioc->facts.ProductID & MPI_FW_HEADER_PID_PROD_MASK)
		    > MPI_FW_HEADER_PID_PROD_TARGET_SCSI)
			ioc->ir_firmware = 1;

		facts->CurrentHostMfaHighAddr =
				le32_to_cpu(facts->CurrentHostMfaHighAddr);
		facts->GlobalCredits = le16_to_cpu(facts->GlobalCredits);
		facts->CurrentSenseBufferHighAddr =
				le32_to_cpu(facts->CurrentSenseBufferHighAddr);
		facts->CurReplyFrameSize =
				le16_to_cpu(facts->CurReplyFrameSize);
		facts->IOCCapabilities = le32_to_cpu(facts->IOCCapabilities);

		/*
		 * Handle NEW (!) IOCFactsReply fields in MPI-1.01.xx
		 * Older MPI-1.00.xx struct had 13 dwords, and enlarged
		 * to 14 in MPI-1.01.0x.
		 */
		if (facts->MsgLength >= (offsetof(IOCFactsReply_t,FWImageSize) + 7)/4 &&
		    facts->MsgVersion > MPI_VERSION_01_00) {
			facts->FWImageSize = le32_to_cpu(facts->FWImageSize);
		}

		sz = facts->FWImageSize;
		if ( sz & 0x01 )
			sz += 1;
		if ( sz & 0x02 )
			sz += 2;
		facts->FWImageSize = sz;

		if (!facts->RequestFrameSize) {
			/*  Something is wrong!  */
			printk(MYIOC_s_ERR_FMT "IOC reported invalid 0 request size!\n",
					ioc->name);
			return -55;
		}

		r = sz = facts->BlockSize;
		vv = ((63 / (sz * 4)) + 1) & 0x03;
		ioc->NB_for_64_byte_frame = vv;
		while ( sz )
		{
			shiftFactor++;
			sz = sz >> 1;
		}
		ioc->NBShiftFactor  = shiftFactor;
		dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "NB_for_64_byte_frame=%x NBShiftFactor=%x BlockSize=%x\n",
		    ioc->name, vv, shiftFactor, r));

		if (reason == MPT_HOSTEVENT_IOC_BRINGUP) {
			/*
			 * Set values for this IOC's request & reply frame sizes,
			 * and request & reply queue depths...
			 */
			ioc->req_sz = min(MPT_DEFAULT_FRAME_SIZE, facts->RequestFrameSize * 4);
			ioc->req_depth = min_t(int, MPT_MAX_REQ_DEPTH, facts->GlobalCredits);
			ioc->reply_sz = MPT_REPLY_FRAME_SIZE;
			ioc->reply_depth = min_t(int, MPT_DEFAULT_REPLY_DEPTH, facts->ReplyQueueDepth);

			dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "reply_sz=%3d, reply_depth=%4d\n",
				ioc->name, ioc->reply_sz, ioc->reply_depth));
			dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "req_sz  =%3d, req_depth  =%4d\n",
				ioc->name, ioc->req_sz, ioc->req_depth));

			/* Get port facts! */
			if ( (r = GetPortFacts(ioc, 0, sleepFlag)) != 0 )
				return r;
		}
	} else {
		printk(MYIOC_s_ERR_FMT
		     "Invalid IOC facts reply, msgLength=%d offsetof=%zd!\n",
		     ioc->name, facts->MsgLength, (offsetof(IOCFactsReply_t,
		     RequestFrameSize)/sizeof(u32)));
		return -66;
	}

	return 0;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	GetPortFacts - Send PortFacts request to MPT adapter.
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@portnum: Port number
 *	@sleepFlag: Specifies whether the process can sleep
 *
 *	Returns 0 for success, non-zero for failure.
 */
static int
GetPortFacts(MPT_ADAPTER *ioc, int portnum, int sleepFlag)
{
	PortFacts_t		 get_pfacts;
	PortFactsReply_t	*pfacts;
	int			 ii;
	int			 req_sz;
	int			 reply_sz;
	int			 max_id;

	/* IOC *must* NOT be in RESET state! */
	if (ioc->last_state == MPI_IOC_STATE_RESET) {
		printk(MYIOC_s_ERR_FMT "Can't get PortFacts NOT READY! (%08x)\n",
		    ioc->name, ioc->last_state );
		return -4;
	}

	pfacts = &ioc->pfacts[portnum];

	/* Destination (reply area)...  */
	reply_sz = sizeof(*pfacts);
	memset(pfacts, 0, reply_sz);

	/* Request area (get_pfacts on the stack right now!) */
	req_sz = sizeof(get_pfacts);
	memset(&get_pfacts, 0, req_sz);

	get_pfacts.Function = MPI_FUNCTION_PORT_FACTS;
	get_pfacts.PortNumber = portnum;
	/* Assert: All other get_pfacts fields are zero! */

	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Sending get PortFacts(%d) request\n",
			ioc->name, portnum));

	/* No non-zero fields in the get_pfacts request are greater than
	 * 1 byte in size, so we can just fire it off as is.
	 */
	ii = mpt_handshake_req_reply_wait(ioc, req_sz, (u32*)&get_pfacts,
				reply_sz, (u16*)pfacts, 5 /*seconds*/, sleepFlag);
	if (ii != 0)
		return ii;

	/* Did we get a valid reply? */

	/* Now byte swap the necessary fields in the response. */
	pfacts->MsgContext = le32_to_cpu(pfacts->MsgContext);
	pfacts->IOCStatus = le16_to_cpu(pfacts->IOCStatus);
	pfacts->IOCLogInfo = le32_to_cpu(pfacts->IOCLogInfo);
	pfacts->MaxDevices = le16_to_cpu(pfacts->MaxDevices);
	pfacts->PortSCSIID = le16_to_cpu(pfacts->PortSCSIID);
	pfacts->ProtocolFlags = le16_to_cpu(pfacts->ProtocolFlags);
	pfacts->MaxPostedCmdBuffers = le16_to_cpu(pfacts->MaxPostedCmdBuffers);
	pfacts->MaxPersistentIDs = le16_to_cpu(pfacts->MaxPersistentIDs);
	pfacts->MaxLanBuckets = le16_to_cpu(pfacts->MaxLanBuckets);

	max_id = (ioc->bus_type == SAS) ? pfacts->PortSCSIID :
	    pfacts->MaxDevices;
	ioc->devices_per_bus = (max_id > 255) ? 256 : max_id;
	ioc->number_of_buses = (ioc->devices_per_bus < 256) ? 1 : max_id/256;

	/*
	 * Place all the devices on channels
	 *
	 * (for debuging)
	 */
	if (mpt_channel_mapping) {
		ioc->devices_per_bus = 1;
		ioc->number_of_buses = (max_id > 255) ? 255 : max_id;
	}

	return 0;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	SendIocInit - Send IOCInit request to MPT adapter.
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@sleepFlag: Specifies whether the process can sleep
 *
 *	Send IOCInit followed by PortEnable to bring IOC to OPERATIONAL state.
 *
 *	Returns 0 for success, non-zero for failure.
 */
static int
SendIocInit(MPT_ADAPTER *ioc, int sleepFlag)
{
	IOCInit_t		 ioc_init;
	MPIDefaultReply_t	 init_reply;
	u32			 state;
	int			 r;
	int			 count;
	int			 cntdn;

	memset(&ioc_init, 0, sizeof(ioc_init));
	memset(&init_reply, 0, sizeof(init_reply));

	ioc_init.WhoInit = MPI_WHOINIT_HOST_DRIVER;
	ioc_init.Function = MPI_FUNCTION_IOC_INIT;

	/* If we are in a recovery mode and we uploaded the FW image,
	 * then this pointer is not NULL. Skip the upload a second time.
	 * Set this flag if cached_fw set for either IOC.
	 */
	if (ioc->facts.Flags & MPI_IOCFACTS_FLAGS_FW_DOWNLOAD_BOOT)
		ioc->upload_fw = 1;
	else
		ioc->upload_fw = 0;
	ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "upload_fw %d facts.Flags=%x\n",
		   ioc->name, ioc->upload_fw, ioc->facts.Flags));

	ioc_init.MaxDevices = (U8)ioc->devices_per_bus;
	ioc_init.MaxBuses = (U8)ioc->number_of_buses;

	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "facts.MsgVersion=%x\n",
		   ioc->name, ioc->facts.MsgVersion));
	if (ioc->facts.MsgVersion >= MPI_VERSION_01_05) {
		// set MsgVersion and HeaderVersion host driver was built with
		ioc_init.MsgVersion = cpu_to_le16(MPI_VERSION);
	        ioc_init.HeaderVersion = cpu_to_le16(MPI_HEADER_VERSION);

		if (ioc->facts.Flags & MPI_IOCFACTS_FLAGS_HOST_PAGE_BUFFER_PERSISTENT) {
			ioc_init.HostPageBufferSGE = ioc->facts.HostPageBufferSGE;
		} else if(mpt_host_page_alloc(ioc, &ioc_init))
			return -99;
	}
	ioc_init.ReplyFrameSize = cpu_to_le16(ioc->reply_sz);	/* in BYTES */

	if (ioc->sg_addr_size == sizeof(u64)) {
		/* Save the upper 32-bits of the request
		 * (reply) and sense buffers.
		 */
		ioc_init.HostMfaHighAddr = cpu_to_le32((u32)((u64)ioc->alloc_dma >> 32));
		ioc_init.SenseBufferHighAddr = cpu_to_le32((u32)((u64)ioc->sense_buf_pool_dma >> 32));
	} else {
		/* Force 32-bit addressing */
		ioc_init.HostMfaHighAddr = cpu_to_le32(0);
		ioc_init.SenseBufferHighAddr = cpu_to_le32(0);
	}

	ioc->facts.CurrentHostMfaHighAddr = ioc_init.HostMfaHighAddr;
	ioc->facts.CurrentSenseBufferHighAddr = ioc_init.SenseBufferHighAddr;
	ioc->facts.MaxDevices = ioc_init.MaxDevices;
	ioc->facts.MaxBuses = ioc_init.MaxBuses;

	dhsprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Sending IOCInit (req @ %p)\n",
			ioc->name, &ioc_init));

	r = mpt_handshake_req_reply_wait(ioc, sizeof(IOCInit_t), (u32*)&ioc_init,
				sizeof(MPIDefaultReply_t), (u16*)&init_reply, 10 /*seconds*/, sleepFlag);
	if (r != 0) {
		printk(MYIOC_s_ERR_FMT "Sending IOCInit failed(%d)!\n",ioc->name, r);
		return r;
	}

	/* No need to byte swap the multibyte fields in the reply
	 * since we don't even look at its contents.
	 */

	dhsprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Sending PortEnable (req @ %p)\n",
			ioc->name, &ioc_init));

	if ((r = SendPortEnable(ioc, 0, sleepFlag)) != 0) {
		printk(MYIOC_s_ERR_FMT "Sending PortEnable failed(%d)!\n",ioc->name, r);
		return r;
	}

	/* YIKES!  SUPER IMPORTANT!!!
	 *  Poll IocState until _OPERATIONAL while IOC is doing
	 *  LoopInit and TargetDiscovery!
	 */
	count = 0;
	cntdn = ((sleepFlag == CAN_SLEEP) ? HZ : 1000) * 60;	/* 60 seconds */
	state = mpt_GetIocState(ioc, 1);
	while (state != MPI_IOC_STATE_OPERATIONAL && --cntdn) {
		if (sleepFlag == CAN_SLEEP) {
			msleep(1);
		} else {
			mdelay(1);
		}

		if (!cntdn) {
			printk(MYIOC_s_ERR_FMT "Wait IOC_OP state timeout(%d)!\n",
					ioc->name, (int)((count+5)/HZ));
			return -9;
		}

		state = mpt_GetIocState(ioc, 1);
		count++;
	}
	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Wait IOC_OPERATIONAL state (cnt=%d)\n",
			ioc->name, count));

	ioc->aen_event_read_flag=0;
	return r;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	SendPortEnable - Send PortEnable request to MPT adapter port.
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@portnum: Port number to enable
 *	@sleepFlag: Specifies whether the process can sleep
 *
 *	Send PortEnable to bring IOC to OPERATIONAL state.
 *
 *	Returns 0 for success, non-zero for failure.
 */
static int
SendPortEnable(MPT_ADAPTER *ioc, int portnum, int sleepFlag)
{
	PortEnable_t		 port_enable;
	MPIDefaultReply_t	 reply_buf;
	int	 rc;
	int	 req_sz;
	int	 reply_sz;

	/*  Destination...  */
	reply_sz = sizeof(MPIDefaultReply_t);
	memset(&reply_buf, 0, reply_sz);

	req_sz = sizeof(PortEnable_t);
	memset(&port_enable, 0, req_sz);

	port_enable.Function = MPI_FUNCTION_PORT_ENABLE;
	port_enable.PortNumber = portnum;
/*	port_enable.ChainOffset = 0;		*/
/*	port_enable.MsgFlags = 0;		*/
/*	port_enable.MsgContext = 0;		*/

	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Sending Port(%d)Enable (req @ %p)\n",
			ioc->name, portnum, &port_enable));

	/* RAID FW may take a long time to enable
	 */
	if (ioc->ir_firmware || ioc->bus_type == SAS) {
		rc = mpt_handshake_req_reply_wait(ioc, req_sz,
		(u32*)&port_enable, reply_sz, (u16*)&reply_buf,
		300 /*seconds*/, sleepFlag);
	} else {
		rc = mpt_handshake_req_reply_wait(ioc, req_sz,
		(u32*)&port_enable, reply_sz, (u16*)&reply_buf,
		30 /*seconds*/, sleepFlag);
	}
	return rc;
}

/**
 *	mpt_alloc_fw_memory - allocate firmware memory
 *	@ioc: Pointer to MPT_ADAPTER structure
 *      @size: total FW bytes
 *
 *	If memory has already been allocated, the same (cached) value
 *	is returned.
 *
 *	Return 0 if successfull, or non-zero for failure
 **/
int
mpt_alloc_fw_memory(MPT_ADAPTER *ioc, int size)
{
	int rc;

	if (ioc->cached_fw) {
		rc = 0;  /* use already allocated memory */
		goto out;
	}
	else if (ioc->alt_ioc && ioc->alt_ioc->cached_fw) {
		ioc->cached_fw = ioc->alt_ioc->cached_fw;  /* use alt_ioc's memory */
		ioc->cached_fw_dma = ioc->alt_ioc->cached_fw_dma;
		rc = 0;
		goto out;
	}
	ioc->cached_fw = pci_alloc_consistent(ioc->pcidev, size, &ioc->cached_fw_dma);
	if (!ioc->cached_fw) {
		printk(MYIOC_s_ERR_FMT "Unable to allocate memory for the cached firmware image!\n",
		    ioc->name);
		rc = -1;
	} else {
		dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "FW Image  @ %p[%p], sz=%d[%x] bytes\n",
		    ioc->name, ioc->cached_fw, (void *)(ulong)ioc->cached_fw_dma, size, size));
		ioc->alloc_total += size;
		rc = 0;
	}
 out:
	return rc;
}

/**
 *	mpt_free_fw_memory - free firmware memory
 *	@ioc: Pointer to MPT_ADAPTER structure
 *
 *	If alt_img is NULL, delete from ioc structure.
 *	Else, delete a secondary image in same format.
 **/
void
mpt_free_fw_memory(MPT_ADAPTER *ioc)
{
	int sz;

	if (!ioc->cached_fw)
		return;

	sz = ioc->facts.FWImageSize;
	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "free_fw_memory: FW Image  @ %p[%p], sz=%d[%x] bytes\n",
		 ioc->name, ioc->cached_fw, (void *)(ulong)ioc->cached_fw_dma, sz, sz));
	pci_free_consistent(ioc->pcidev, sz, ioc->cached_fw, ioc->cached_fw_dma);
	ioc->alloc_total -= sz;
	ioc->cached_fw = NULL;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_do_upload - Construct and Send FWUpload request to MPT adapter port.
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@sleepFlag: Specifies whether the process can sleep
 *
 *	Returns 0 for success, >0 for handshake failure
 *		<0 for fw upload failure.
 *
 *	Remark: If bound IOC and a successful FWUpload was performed
 *	on the bound IOC, the second image is discarded
 *	and memory is free'd. Both channels must upload to prevent
 *	IOC from running in degraded mode.
 */
static int
mpt_do_upload(MPT_ADAPTER *ioc, int sleepFlag)
{
	u8			 reply[sizeof(FWUploadReply_t)];
	FWUpload_t		*prequest;
	FWUploadReply_t		*preply;
	FWUploadTCSGE_t		*ptcsge;
	u32			 flagsLength;
	int			 ii, sz, reply_sz;
	int			 cmdStatus;
	int			request_size;
	/* If the image size is 0, we are done.
	 */
	if ((sz = ioc->facts.FWImageSize) == 0)
		return 0;

	if (mpt_alloc_fw_memory(ioc, ioc->facts.FWImageSize) != 0)
		return -ENOMEM;

	dinitprintk(ioc, printk(MYIOC_s_INFO_FMT ": FW Image  @ %p[%p], sz=%d[%x] bytes\n",
	    ioc->name, ioc->cached_fw, (void *)(ulong)ioc->cached_fw_dma, sz, sz));

	prequest = (sleepFlag == NO_SLEEP) ? kzalloc(ioc->req_sz, GFP_ATOMIC) :
	    kzalloc(ioc->req_sz, GFP_KERNEL);
	if (!prequest) {
		dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "fw upload failed "
		    "while allocating memory \n", ioc->name));
		mpt_free_fw_memory(ioc);
		return -ENOMEM;
	}

	preply = (FWUploadReply_t *)&reply;

	reply_sz = sizeof(reply);
	memset(preply, 0, reply_sz);

	prequest->ImageType = MPI_FW_UPLOAD_ITYPE_FW_IOC_MEM;
	prequest->Function = MPI_FUNCTION_FW_UPLOAD;

	ptcsge = (FWUploadTCSGE_t *) &prequest->SGL;
	ptcsge->DetailsLength = 12;
	ptcsge->Flags = MPI_SGE_FLAGS_TRANSACTION_ELEMENT;
	ptcsge->ImageSize = cpu_to_le32(sz);
	ptcsge++;

	flagsLength = MPT_SGE_FLAGS_SSIMPLE_READ | sz;
	ioc->add_sge((char *)ptcsge, flagsLength, ioc->cached_fw_dma);
	request_size = offsetof(FWUpload_t, SGL) + sizeof(FWUploadTCSGE_t) +
	    ioc->SGE_size;
	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Sending FW Upload "
	    " (req @ %p) fw_size=%d mf_request_size=%d\n", ioc->name, prequest,
	    ioc->facts.FWImageSize, request_size));
	DBG_DUMP_FW_REQUEST_FRAME(ioc, (u32 *)prequest);

	ii = mpt_handshake_req_reply_wait(ioc, request_size, (u32 *)prequest,
	    reply_sz, (u16 *)preply, 65 /*seconds*/, sleepFlag);

	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "FW Upload completed "
	    "rc=%x \n", ioc->name, ii));

	cmdStatus = -EFAULT;
	if (ii == 0) {
		/* Handshake transfer was complete and successful.
		 * Check the Reply Frame.
		 */
		int status;
		status = le16_to_cpu(preply->IOCStatus) &
				MPI_IOCSTATUS_MASK;
		if (status == MPI_IOCSTATUS_SUCCESS &&
		    ioc->facts.FWImageSize ==
		    le32_to_cpu(preply->ActualImageSize))
				cmdStatus = 0;
	}
	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT ": do_upload cmdStatus=%d \n",
			ioc->name, cmdStatus));


	if (cmdStatus) {
		ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "fw upload failed, "
		    "freeing image \n", ioc->name));
		mpt_free_fw_memory(ioc);
	}
	kfree(prequest);

	return cmdStatus;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_downloadboot - DownloadBoot code
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@pFwHeader: Pointer to firmware header info
 *	@sleepFlag: Specifies whether the process can sleep
 *
 *	FwDownloadBoot requires Programmed IO access.
 *
 *	Returns 0 for success
 *		-1 FW Image size is 0
 *		-2 No valid cached_fw Pointer
 *		<0 for fw upload failure.
 */
static int
mpt_downloadboot(MPT_ADAPTER *ioc, MpiFwHeader_t *pFwHeader, int sleepFlag)
{
	MpiExtImageH/*
 *_t	*p/fusion/;
	u32			 fwSizThis is thediag0val;
	int thecounthis is th*ptrFwn MPT base driRwDatahis is thene      This is theload_addrhis is  theioc_state=0;

	ddlprintk(ioc, s)
 *  MYIOC_s_DEBUG_FMT "down    boot: fw size 0x%x (%d), FW Ptr %p\n",
				ioc->name, pFwmptbas->sion/usio-2008 LSI Corporation
 *  (mailto))daptCHIPREG_WRITE32(& (c) chip->WriteSequence, 0xFF);)
 *
 */
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-MPI_WRSEQ_1ST_KEY_VALUE=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
    This progr2NDis free software; you can redistribute it and/or modify
    it unde3Rthe terms of the GNU General Public License as published by
    the 4THis free software; you can redistribute it and/or modify
    it unde5s distributed i)
 *
 */
/*=-=-=-=-=-=-=-=-=-=Diagnostic, (ThisDIAG_PREVENT_I FuBOOT | ThisLITY DISABLE_ARM.com)
/* wait 1 msec */
	if (x/drivers == CAN_SLEEP) {
		mx/dri(1=-=-} elsels.

 delay  WARRANaptedriver  =  *
 */
/READlied warranty of
    MERC=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-f
    MERCHADED ON ANPARTICULAR RESET_ADAPTERthoutfor (orts  = 0;ports  < 3RANTIES O++ils.

DED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR
    Cublic!(DED ON AN&NCLUDING, WITHOUT
    LIils.

ter(s)
 *      running LSI Fusion MPT (MesWITHOUT
    L cleared,ports =%dCopyright (c) 1999-2orts .com			breakd as}
	e
    GNU.1 neral Puublic License for more details.


    NO (100nd asNTY
    THEE PROGRAM ent, inc PROVIblicNTIES O== 30 F TITLEr(s)
 *      running LSI Fusion MPT (Message Passing failed! "
		"Unable to getARTICULAR PRWE modeOR IMPLIED=%xCopyrig (c) 1999-2DED ON Aand asreturn -3S PROVI *
 */
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without/* Set the f
  RwEn and Dis unavARM bitsal Pu even the implied warranty of
    MERCHANTABILITY RW_ENPOSE PARTICULAR PURPOSE.  See the Fusio = (008 LSI Corporation
 + 3)/4;
	*    he Gwith*)2008 LSI C THE EX-=-=-ISE OLoadStartAddressvailSE OF ANY  SoftwareRegister
	 * using Programmed IOle P Public (c) errata_flag_1064)
		pci_ed una_io_access2111-1pcidevthout even thPIO/*=-=-=-=-=-=-=pio_ EITHER EXRwSoftwar-2008 LSI Corthe Free Softwarnd aram errors, damage to or loss of data,
  the Free  or u writtengy) fins.

    DISCLAIME/kernel.h>
#include <linux/mthoutram errors, damage to or loss of data,
   writeFW sion/:gy) fibytes @*  Copyright (c) 1999-2 Fusio*4, *    and awhile ( Fusio--ils.

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#inclupeci,  *    ++IS PROVItocol dri =e <linux/typNocol drimptbasOffse mul		/* netocol driils.

 *      The Gage/fusion/mptbase.his ((char *)008 LSI C +otocol drithout	    For udef /fusion/>
#include <linux/-=-=-y of the GN-=-=-=-=-=Public License >> 2d asalong with this *      Thirogram errors, damage to or loss of data,
   writeExtblkdev.h>
#inrmwanclude <lin     For utions.

  lay.h>
#include <linux/in <linux/interru,     For uand as-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#include <linuxRSION);

/UX_VE		/* needed for in_innterrupt() proto */
#include <linux/dma-mapping.h>
#include < riskio.h>
#ifdef -=-=-=-=-=R
#include <asm/mtrr.h>
ROVIot, write to IopResetVectorRegSoftn, MARSION_COMMON
#define MYNAM		"mptbase"

MODULE_Anable_fc;
moduon, I=%x!ux/slI PC) 1999-2	008 LSI Corpable_fcle_paraand a-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#include <linux/kernel.h>
llers (default=0if not, write to nable_fc;
moduValuem(mpt_msi_enable_fc, int, 0);
MODULE_PARM_DESC(mpt_msi_enable_fc, "s, " ble MSI Support for Fsi_enable_sas, int,ble_sas, " 0)");

static int mpt_msi_enable_sas;
module_paping.hodule_param(mpt_channel_mapping THE EXCf us   Fointernal flash bad HER - autoincremente, Sr59 Temp,le Plso must do two#incles.ton, MA  02111-1bus_typecostSPIils.

/*
	e Pl1030GHTS 1035 H/W 307  U, workaroundvail=-=-=-ebug_lSE O

staFug_lBadSignatureBitebug_h it-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#include <linux0x3F0 (defnd asLAN) speciN "AS IS" B-=-=ASIS, WITHOUTt_channel_mapping, "ult=0)");

int m|= 0x4 (defa0
 *  cmd line parameters
 */

static int mpt_msi_enable\
	- (default=0t, 0);
MODULE_PARM_DESC(mpt_channel_mapping, " MLAN) specilt=0)NTY
   /* if(*kp);
module_param_AS) || *kp);
module_paramFC)) */ TITLE, NON-INFRINGEMENT,
    MERCHANTABILITY OR FITNESS FONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED Iebug  ARTICULAR CLEAR_FLASH_BAD_SIGule_pa
    GNU General Puts
    exercise of rights under this Agreemt, including but not limit to
    the risk111-1307  USA
*/
/*=-=-=-=-=-dRANTED=-=-=-=-=-=-=-=-=-=-=-=-=-=DED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR
    Cram errors, damage to or loss of data,
    programs orof operatio,ent, a   Ne, Soff or FITNESS FOR A, PURPOSE.  S, ES

    Yns.

    DISCLAIMER OF LIABILITARTICULAR = ~NTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  S INCLUDING, WS

    Y.
 */
					/* Adapter link list */
LIST_HEAD(ioc_list);
		now of operations.

    DISCLAIMER OF LIABILITONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIEDDULE_PARM_DESC=-=-vailre_fcdebugs-=-=-=-am(mpt *
 */
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=aram *kp);
module_param_e ons.

 PCI chip = mpt_GetIocSchip     rault=0risks(VERS];Fact-=-=-,ux/driversyrightMPT_HOST FITNESS FORINGUP)) !=  of prog*/
					/* Adapter link list */
LIST_HEAD(ck Index's
r equip: S];


/*y_NAME);
MODUpport for F PCI chipand ass
    NEIEFAULTto
    the TION, ANY =RANTIES <HZ*2RANTIES  OF TITLblicOWN	_PROTOCOL_DRIVERS];


/*
 *  Dri)R PURPOI FuSTATEbug;
YS;
static u8 last_drv_idx;

/*=-=-=-=-=-=-=-=-;
MOD   programs orsu-=-=-ful!irqreturR);
=-=-=-=-=-=-=-=-=-=-ing the Program =-=-=-=*/
/*
 *  F	*MptDeviceDriverHandlers[MPT_ Forward wfaul riskoid *bSendIocInit */
static u8 mL_DRIVE
			int *req,
		MPT_FRAME_HDR *reply);
static int	mppt_handshake_r: mpt_do_ioc_r equip-=-=-=-=-=-=-=-=-=-and assForward protos...
 static reason, int sleepFlag);
static void	mpt_detect_bound_ports(MPT_ADAPTER q_reply_wastruct pci_dev *pdev);
stasleepFlag);
risks
    exercise of rights under this Agreemet, including but not limite to
    thable(MPT_ADAPTER *ioc);
static void	mpt_ada   programs or equipme=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 * orward protos...}

/*=-leepFlag);
static int	SendIocInit(MPT_ADAPTER *ioc, int sleepFlag);
stati*/
/**
 *	Kickx/init- Perform hardR		 Mptof MPT adapter.ndPo@ioc: Pont mpvaile_inateness strucULE_eepFlforce: Fioc,R *ioc, inteepFlx/drivers: Specifies whethc inhe pro=-=- canux/dri
 endPoThis routine placesrtnum, int s in				/  MERCuptio viaAPTEndPo-=-=-=-=-=-=-bug_level(GHTS then pADAPTEs aR *ioc, int por, int sndPoMPT_ADAOF AN_diag_rug_levelleepndPoInputs:  ux/driA
*/ - more deta (non-nt mprupptReread)ndPo		or NO_ADAPTERc, u8 reset_type, use  PROGR, int   *ioc, - 1 if doorbell active, bo*iocfault  chip int s	llAck(operational, I FuRECOVERY oTER *			MAX_PROTOCOsleepFlare is an alt_iocleepFER *0TY
  ag);
stR    Ns:APTER 1 -R *ioc, int, T_FRAAPTER 0 - nER		 MptduavailHistory HERorbellReply(- intDAPTER *ioc, int howlong, i buMPT_trbellReply(MstatORR		 MptPT_A equipvailcomet sleepFlag)2
static intrograldADAPTetic iLITYuptiopFlag)3 -int	GetIoUnvel;FW HER
ESC( chiicint 
rtEnable((mpt_do_uploa**/
stlinu*ioc,  linux/drivers/messlinu *io_		 Mp_donOCOLwfauwith PCI chip/adastaticnt,cntdn-=-=-=nits)
 *      running LSI Fusion MPT (MesrtEnable(ing!SI Support forand aam *kp);
module_param_call(mpt_d Always issue a Msg UTER le_fc first. wHeadwill of us someebug_lSCSI bus hang condi, inerneDESC(mpmpt_do_le_fc     rThisFUNCTIONER *iMESSAGE_UNIT WITHOvery(MPT_ADAUX_VEs
    exercise of rights under this Agreemennt, including but not limitedpFlag);
sta
	c int	mpt_readScsi_DRIDED t	mpt_     rTER *io	SendEventAcMPT_Ac int	mpt_readSc< 0=-=-
    NEc int	mpt_readS int portnum);
static void 	mpt_read_ioc_pg_1(Mf
    MERCR		 Mptq_reply_wains.

    DISCLAIlinux/*ioc,he Gc License for more detail? HZ : page_ * 2;*ioc2ed wondEUNDERTION, turn_t nt<*ioc, pt_v irq, vois_id);
static int	mptbase_reply(ine WHid *bus_id);
sta=DAPTER *ioc, MPT_FRAME fault , int *eof, void *data);OPERA,
	iAL is
    sportnum);
static void 	mpt_read_ioc_pg_1(MPT_ADAPTEeq_reply_wait(M_ADAPTCopyrh LSes,
			u32 *rem and assge_alloc(MPT_ADAPTER *ioDAPTER *ioc);
static int	MakeIocReady(MPT_ADAPTER *ioc, int force, int sleepFlag);
stant portnum);
static void 	mpt_readERRT (MesFtPage2(MPT_ADAPTER  aftic 		 Mp);
static int	Get
    DISCLAIMEic int	mptbase_reply(MPT *ioc, int p1, int sleepFlag);
static int	SendIocInit(MPT_ADAPTER *ioc, int sleepFlag);
static int	SendPooc, u8 access_MPT_ADAPTER *ioc, int por int, int sleepFlag);
static int	mpt_do_upload(MPT_ADAPTER ignores(MPttic to honorGHTS DAPTERto );
staint h int		 Mpthhowlong, itatic int	mpt_doMPT_ADAPTEif calage2in a  *ioc, u8 reset_type,int hY
    MptRoepFlag);
stifos(MPT_A instype, in *pFwHeader, int sleepFl32 log_info int	mpt_diag_reset(MPT_ADAPTER *ioc, int ignore, intsleepFlag);
static int	KickStart(MPT_ADAPTER *ioc, int ignore, int sleepFl Adl_relashas_peb int typeyDAPTER *iouponocmpt_summaPT_Aple, inlag);
stpFlag);
  1 R *ioc, int q_reply_waAPTER *0 tatic int	;
statieaddrcaifosER *ioc);

/* modid _APTER -2  =-=-=e, S	mpt_diag_reset( equip *pdev)3 mand_reg;

			 Mptead_confi;
static int	oc, u8 access_ciPortSettings(MPT_ADAP);
sta  linux/drivers/messwith driver whid_regnt	WaitPT_ADAPc int	mpt_readScsiDeviADAPTANY WARRAand_reg);
1N AN "	u16Mpi08 LSI Cc.h"cached_fw voidstatic int	FWESC(mu8	 cb_idxt=0)");

statany exist_debc, u8 resEUNDER, EVEN IF ADVISED OF THE POSInt


/uwfauiver 	*MptDevi=-=-=-->devic *eof, voMANUFACTPlag)DEVID_SAS1078s[MPS FOR A );
sta=-=-ities(MPT_At=0)rss)
 *      running LSI FusiWARNT (Mes%s: Dnt	Wait=%p; m *kCOMMANDnt, 	"or uess=  Copy upport for F__func__yrigh KIND, EITHERnt	Wait, -=-=-=-=-=-=le_fc_m *kpF_COUNT 20000
#endif

/*=-=-=-=-= *	mpt_getfaul07		int req License for more detair this Agrfine WHg, inut not lifine mpt_debug_lCall each currentlynt sleepFedR *itocolleep_access(andlmplee Plwith pre-		 Mptindicg, incation NOTE: If we're doe, S_MAX_PROTOCO,nt(MPT_piFwbe nou8 cb_Mptle_fcHs)
{
	s[]et_cb_idx(MPyetcation(MPTTION, 	comm of, T_MAX_PROTOCOL_DRIVERS-1;;
	comma;
	commnable_spibliccb_idx] == dclass}

/**]	Retu	(*s completed
 * @ioc: per a)     ;
MODUL mptI FuPRMPT_SET MSI Suturn 0;
}ANY WARRANTIES OR 6ONDITIONS OF TITL	int	WaitF "AS IS" BASIS, WITHOUT WARRANnt	Waitnd assint	WaitF&of, void *data);MASKasn'tkp);
	MPT_ADAPTER *ioc;

	if (r
static int	mptlooke, STIONER *iooc, M:void
pci_=%xy(iocstati   " and
    distributing the Prooid
pci_rogram and atic intAPTER *iocof, void *data);
static		int sleepFl1apter_dbuff
    GNU G  Public ss: class driver enum
 *
 *	Retu
    NO Wage_acceor zero mmeans it age_access_T_ADAPTER *iatic intUseC_FS
static int	pr" method! (only the, Savail una!


#-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *  Private data...
 	mpt_read_idebug_leveAR PURTint rcrq, void *pportR *ioc,	Retumand_reg;

AS IS" BASIS, WITHOUTR *ioc,ANTABILITY OR FITNESS FdInit_t ioc_init);

#ifdef CONFIG_PROC_FbG1:eg);
}=%08x detec1N_PAGCopyrigh  DISCLAIMER OF LIAE_READ_LIABILITtic intD    For (cb_iRIVE aravaile2(MP);
staconsistent;c);

/*le Ploroid	mAPTER *ioc);

/* is 0ton, MA  021yData[||  PARTICULAR PURPOSE. Each ReHISTORY is
   		/* nePARTICULAR PURPOSE. Eterr)ASIO*ioc, u3ot, writemagicetHandler_exi *ioc, int ignore, intgeHea* Loop until int	mpt_diag_reset
/**
 TENDE *
 */
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- on workq after ioc fault
 *	@work: input argThis program is free softwased to derive ioc
 *
**/
static void
mpt_fault_reset_wor the terms of thsed to derive ioc
 *
**/
static void
mpt_fault_reset_woFree Software Foused to derive ioc
 *
**/
static void
mpt_fault_reset_wois distributed insed to derive ioc
 *
**/
static void
mpt_fault_reset_wo WARRANTY; withoutONFIG_PAGET00 *  Public MPT_ADAPTER *ioc, EventNotificatiohis Agreement, inncluding but  not limited to
 MPI_CONt(int i, u16 *u1c int
> 2*ioc, u32tatic void	mpt_iocstatusEd unavFS
static ieset(FAILED! (%02xhd	mpt_
MODULE_LICENSE("ble */
statstatic void	2ageVerPI_CON))
		goto out;
	if (!hdr.ExtPageLength)
		goto out;

hysAddr = dma_handle;
	cfg.action = MPI_CWro  buffer,f
  -=-=-En, dma_hand(%xom %s!!\n",  DISCLAIMER OF LIABILITYPI_CO	buffer = pci_alloc_consistent(ioc->pccidev, hdr.ExtPageLenggth * 4,
	    &dma_handle);
	if (!buffer)
		goto out;

	cfg.phyysAddr = dma_handle;
	cfg.action = MPI_CONF2G_ACTION_PAGE_READ_CURRENT;

	iff ((mpt_config(ioc, &cfg)))
		goto  risks u8 cb_GRANTED
IOUNerCl(Bug fix	Ret ->bus_C(mpt_debug_S OF ANY KIND, EITHER EXPRESS OR IMPLIED INCLUDING, PURPOSE.  See ifeans it wasn't found
 Now hiptResent	GetIib_id, int ignore, int sleepFu8 cb_(THE BIG HAMMER!) (

stas terrubit)cation(MPTONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED INCLUDING, WITHOUT
    LIMI	trol(MPT_ADAPTER *ipe = sAddr = dma_handle;
	cfg.action = MPI_COdev, PCI_COMMANDid
pci_diCopyright (c) 1999.PageVe found
 **/
static u8
mpt_get_cb_idx(MPT_DRIVER_CLASS dclass)
{
	u8 cb_idx;

	for (cb_idx = MPT_MAX_PROTOCOL_DRIVERS-1; cb_idx; cb_idx--)
		if (MptDriverClass[cb_idx] == dclass)
			return cb_idx;
	return 0;
}

/**
 * mpt_is_discovery_complete - determine if discovery has completed
 * @ioc: per ae (%04xhpt_s
MODlccess_c}

/**ailed");
	ult_ompleted, else zero. MPI_IOC_STATE_MASKN;
	hdr
	MPT_FRAME_HDR *mr = NULL;
	u16 r (!buffereq_idx = 0;
	u8 cb_idx;

	tatic = &etIocState(iocrd(pdev, 	Returd(pdev, nclude ad_config_wo)MPI_CONTEXT_REx, or ze;

	v, hdr.ExtPag &&upport!buffer)
	NTEXT_REPLY_TYPE_SHIFT) {
	case MPI_CONTEXT_REPFFF;
		cb_idx = (px, or zero mYPE_SHIFT) {NULLSS FOR A_idx = (pagth * 4,
IS_IOUNDage PasBs orwlong, inD, &cs)
		i
/**
 *CLASMPT_Abe left unuANTEDg_pg_0(ADAP fatalug_lo

/**
 *case.  mpt_sas_log_MPT_A
    NE<ISCOtion(MPTATION, ANY WARRANTIES OR CONDITIONS OF TITLTLE, NON-INFRINGEMENT,
    MERCHANTABILITY OR FITNESS F FOR A PARTICULAR PURPOSE. Each Recipient is
    ssumes all pt_HardR_ioc;

	/* rearm the timer */
	spin_lock_idx = (pG_ACTIOeratio and
    distributf ((mpt_config(ioc, &cgram and assNFIG_PAGETYPE_EXTENDEE_FAULT) {
		printk(MYIOC_s_WARN_FMonReply_t *evnp);
ULT state (%04xmpt_host_page_acce));

	switct requc int
mpoc, uage Passing     r_idx = (pvery(MPT_ADAPThostDOORBELL_DATA_MASK);
	et)
		ret_detecfirmw(!(b  programs or equuress"d)ioc);
static vo			req_i       ioccluding but ot,  GNUTIONFWDLER		    GHTS TIONllAck
/**
 *ailaoe
    FoER *io chipcatiass[caximum   GNUis 60*data);
or (valiIfD, &c,tati  aft(MPT_ADheck againr (valiidx;
  __e, Spite 33or (valmbined with an optimization hery_complete(MPT_ADDAPTER *ioc)
{
	ConfigExtendedPageHeader_t hdr;
	CONFFIGPARMS cfg;
	SasIOUnitPage0_t *bufffer;
	dma_addr_t dma_handle;
	int rc = 0;

	m_t));mset(&hdr, 0, sizeof(ConfigExtendedPageHe
 out: and
    distupport for FIGPARMS));
	hdr.PageVerrsion = MPI_SASIOUNITPAGE0_PAGEVERSION;
	hdrcase skip the callb & 0x0000FFFF;
			mf = MPT_INDEX_2_MFPTR(ioc, req_idx);
			mpt_free_msg_frame(ioc, mf);
			mb();
			return;
			breaeVersion = MPI_SA!of, void *data);
statiORBELL_DATA_MASK);
		printk(M_info(MPT_ADAPTER *io_ADAPTER *ic, u32 ioc_status, MPT_F
{
	MPT_FRAMEADAPTER *oid
pci_*evReply, int D ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR
    C	buffer = pci_alloc_consistent(ioc->pcidev, hdr.ExtPageLength * 4,
	    &dma_handle);
	if (!buffer)
		goto out;

	cfg.physAddr = dma_handle;
	cfg.action = MPI_CONF3G_ACTION_PAGE_READ_CURRENT;

	if ((mpt_config(ioc, &cfg)))
		goto out_fre

statonsistent:
	pesce!  PleepellAck(ery_qule Pl	mpt_diag_reset(to updOTOCIOUNay bnt sleepFla NULL)ay be 32 or 64 bits
	 * get offset based only only tpdev)
{
	u16_consistent(ioc->pcidev, hdr.ExtPageLength *4,
	    buffer, dma_handle);
 out:
	return rc;
}

/*
 *	mpt_fault_reset_work - work perquiesce_io) {
		if ((mpt_is_discovwork: input argument, usd to derive ioc
 *
**/
static void
mpt_fault_reset_work(struct work_strct *work)
{
	MPT_ADAPTER	*ioc =
	    container_of(work, MPT_ADAPTER, falt_reset_work.work);
	u32		 ioc_raw_state;
	int		 rc;
	unsigned long	 fags;

	if (ioc->ioc_reset_in_progress || !ioc->active)
		goto out;

	io_raw_state = mpt_GetIocState(ioc, 0);
	if ((ioc_raw_state & MPI_IOC_STAT_MASK) == MPI_IOC_STATs
    exercise of rights under this Agreement, including but not limited to
   
c->name, ioc_aw_state & MPI_DOORBEL_DATA_MASK);
		printk(MYIOC_s_WARN_FMT "Issuing HardReset from %s!!\n",		       ioc->name, __funumes all riskay be 32 or 64 bits
	 * get offset based only only ttatirotocol drivut_free_consistent:
	pL_DRIVERS];
					/* Reset handler lookup table */
statmay be 32 or 64 bits
	 * get offset based only only the lrc = 1;

 out_free_consistent:
	pc"mptba *ioc;

	if (ret)
		retuidx] =struct pci_itPage2(MPTf usry_read(gs);
	if (ioto out_freeRANTED
_irqsave(&iMrk pe[MPT_MAX_PROTOCOL_DRIVERS];
static struct mpt_pci_drmber (lt=0)");
OCOL_FWk;
	defa chiusebuggernel_parEG_WRITE32(&ioc->chip->ReplyFifo, pa);

	if (freeme)
		mpt_free_msgNTABILITY -=-=-=-=-=-=- INCLUDING, WITHOUT
    L PARTICULAR PURPOSE.  See
}

/*=-=-=-=-=-=-=-iocstatus_irqsave(&ioc->tag HardReset from %s!!\n"		       ioc->name, __fu
    NEITHER REC	buffer = pci_alloc_consistent(ioc->pcidev, hdr.ExtPageLength * 4,
	    &dma_handle);
	if (!buffer)
		goto out;

	cfg.physAddr = dma_handle;
	cfg.action = MPI_CONF4G_ACTION_PAGE_READ_CURRENT;

	if ((mpt_config(ioc, &cfg)))
		goto out_frle PlanufactResethat sstatwe've
	u16 ed even_ADAPif = MPT_ter (IOy reqfx's
.Endle


/*;

	pcar the inte.ExtPageLen	if (!buffer)
 details of the MPT requge_alloc(MPT_ADAPTER *ioint sleepFlag);
static int	SendIocInit(MPT_ADAPTER *ioc, int sleepFlag);
static int	SendPo_ADAPTER *io -(MPT_ BliFFFFFFr-=-=s__exitnum, int sleepFlag);
static int	mpt_do_upload(MPT_ADAPTER 	mpt_rle_p:R		 MptRype, expected v, " s (!(ndPo%EvSwitch,
	int sleepFlag);
static iPI_Smpt_turbo_reply();
static itatic int	mpt_downloadboot(MPT_ADAPTER *ioc, MpiFwHeader_t *pF		return IRQ_NONE;

	/*
	IOUNtnum, int sleepREG_PIO_REA 0:
		cq_replyx >=n-zero:
		cb_idx(Mleep;
static int	_ADAPTER *iociPortSettings(MPT_uach_entY_A_Bioc, int portnum);
statiuse with chipPT_ADAPTERdn			req_dapter;
	dma_addr_t dma_handle;
	int rc = 0 "mpt_e, SCLASS dcl(0xet fM_DRIV
	if ((mpt_conf
 *	@ioc: COL_DRIVERS];
					/* Reset handleGPARMS));
 *	@ioc: <<RTICUOORBELLwitch,
	inSHIF zeroid *b=-=-TR(pFor driver
Ac      r5(pa);
		break;
	ca routine m if not,FW ACK'oc, E;

	,   GNUr, 0, sizeAPTER er (IOpdev)
{
	u16 **start, off_t offset,
				int request, int *eof15 void15*data);
stat%x\n",
			char **start, off_t offset,
			_DRIVAPTER *ioc, MPT_FRAME_HDRinter-->altMPT_MAX_PROTOCO!interate & MPI_I License f!Type = MPI_CONFIGc int
*= 1val, kptCallbacks[cb_idx] == geHeader"TR(paeepFla original (NULx) timeoutTSTM_DRIVgeHeaderpport for F chip,stati)
		mr =+5)/HZ*
 *  Forward prTIMEpt_GetIocStat  exercise of rights under this Agry *mpt_proc_root_dir;

#define/
staGeneraPROGRAULL)    the /* TODO!le Ple.hwhnup */
stndle
stufficatitHeadIOC; re-ic voils ofN*	the protocol- NONE;

	/if neededrnel_param *kp);
 detaiFunc, in */
statirn_t
mpt_interrupt(int irq, voi0PT_ADAPTER *ioc = bus_id;
	u32 pa = CHIPREG_READ32_dmasync(&ioc->chip->ReplyFifo);

	if (pa portChainBuffers - AllocION_memlongTIONHTS portial Licc) {
 b		ioc-eepFlag);
static int	mpt_do_upload(MPT_ADAPTEndPotbase_cmsds.status |= MPT_MGMT_STATsUS_RF_VALID;
	
statS_RF_VALID;
Eventrol arrstatHTS spinlock	mptbase_reply - reply) {
			ioc-ver's callback roommand8		*memwhich susz, ii, num_S_RF_i_dev *		scaleUS_PENsg&ioc->SGE THE EXReqToy) {
hnologar *vequal_IOUNITq_depthle Plinde*
 *(iocidxcol-spec		req_id
		} else
	=PI_CONneratesz =ply = (ioc->mptnst izeofationntk(Mem = kmabase(T_STGFP_ATOMIC		int req:
		dreereq am_set_int(R *i*/
statiREE_MF)
			fstatihis &= ~MP, off_t offset,
				int request, int *eof, v
		} else
	verbo  <lin, sz=%dncludeCopyrigh DR *mf);
statem fun_cb_id:
		devtverboseprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "EventAck reply r;

	NB\n", ioc->name));
		break;
	default:
		printk(MYIOC_s_ERR_FMT
		ller toxpected msg function (=%02Xh) reply received!\n",
		    i}tic intiiWARRANii <eak;
	case MPI_=-=-(char *buf,ply received[ii]
 * mptdex _NO_CHAINr->u.frame.) {
} else
			freereq = 0;
		if to		 *numbmple PlofUS_RF_VALID;
	;

	beframe!atPI_EVEN_cmds.staS_RF_ & MPT_Mle PlCalculprintk(ic: calk function pointer!= MPI(plus 1)askm I/ston,pFlag)multipf ((m buf IO cavalue)
 *
siic daneous cmds%MPT_DRIVc->mptb =ts r ply val)	E;

	/framicenlastUS_RF_VALID;
const te(& callback ra prngth));
		}
	nt !oe it wielt_set alloc' must doak;
	casesz /such aSGE_CTIOy the low adsgFor ust frs_DECTION_Eu64)FMT
s reply car must +	req_ids obtai- 60)ining request framer zero  currently 1 + calls this routine thric4
 *	in order to regmes.
 *
 *	NOTES: The SCSI protocol driN;
	he_cmds\n", must - by *NT_CHANGE)
		MaxecifiD>mpt-1)acks; one ;

	is routine thrice
 *	in order to regiNTY
    THEts.
 *
 *back	Returns u8 valued "handle" in the range (an-=-=-=-s; one for "normal" SCSI IO;
 *	one for MptSptCallortnum);
static void 	mpt_read_ioc_pg_1(Mc->mptbctioe_cmds   distribpport for Fc->mptbase_cmdslinux/am *kp);
module_paramFCrq, void *ts.
 *
>q_idxatic_FC_SG_DEPTHFMT
	ts.
 *
 *allback slot in thisessful.
 *	A rh for empty callback s in this order: {N,...,7,6,5,..}
	 *  (slotsm/iPENDING;oc->alt#endif

s.
 *
-ts reply >	case MP; cb_idx-, ioc_s reply + *	Returns u8 */
u8 {
			MptCallu8
mpt_register(MPT_CALLBACK cbfunc, MPT_DRIVarinlass)
{
	drv_idx st_drv_iS_RF_{
	u8 cb_idx;
	last_drv_SGE_drv_idx = MPTx;
			X_PROTOCOL_DRIVERS;

	/*
	 _calLL) {
			Mptereqallback smoreQUEU0;
	ster separa-=-=-=-=-=-=-FC=-=-=-=-=-=-pecific; cb_idx--) _PENDING;
	

		bre-=-=-=-=-=-NCTION_EVENT_AC_STATUS_Fecific main cfreereq = 1;
:
		devtverboseprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "EventAck replck handle
 *
 ", ioc->name));
		break;
	default:
		printk(MYIOC_s_ERR_FMTecific main cxpecte */
	return freereq;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=TY
    THE 
		de(u8his unloaded.
 */
voi */
u8memme (\n",
=-=-,
		 *ioc, int _PENDING;
	int sleepFlag);
static int	SendIocInit(MPT_ADAPTER *ioc, int sleepFlag);
static int	SendPoPrimeIndeifo->mpioc_T_STATUCLASS E;

	/HTS reiverFIFOsleepFlag);
static int	mpt_do_upload(MPT_ADAPTE *pFwHeader, int class: PEFAULT_FRAME_=-=-=-=- eventific ee.  Each
 *ndPopools (nt !==-=-ary) sleeppvent) 		reCLASS vent cal		MptndPoore prch
 *lback-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptbase_reply - event_registe		ioc->mptbase_cmds.stgisteRAME_HDR *mfu16 n_FRAed lo&hdrier IPREmaTES: Ttter) c_dmializ8  &= ~MPT_M i *	"ply_T_STT_ST@cbfust frUS_PENDING;
		u64{
	ifmask[cb_ib_idx] MPT requ/*  eventmore protoc... ptbase_STATUS_FCOL_DRvers
 
 *	Each protoCallba; cb_idx--) TUS_PENDING) {
		_cmdion and E&hdr;
	cfg.act_debug_lev78ug_leve, param_get_le
 *	@e36GB limitatitoco
	 */
	t_set_debug_level(const char *val, struct kernel_param *k &&-=-=-=-fer = cbfunc;
> DMA_BIpt_iSK(35 is
    y_t *=-=-pt_reb_idx] et_debug_lev,routine
 *	when2river
 out&& (or can ncons Temn no longer) handle even module s,
 *	or when itslid cb_idcbfunc;
	routine
 *	when i		goto 36mem mf, mr))
		mpt_free_msg_frame(ioc, mf);
 out:sett_deb35iscov &ioc_l(&hdr, 0_ADAPTER *i reques/ROL_D/COL_DRInd(MPTse 			ioc-CopyrighttReply = (Eveand assT state (%04x/*le_fce, SDMA unc;
to 64esce
			mf or can no longer) handle even!!\n",outine
 *	whe dri__funcnloaded.
 */
void
mpt_event_deregister(u8 cbegister) callback hanORBELL_DATA_MASK);
		printkADAPTER *iitPage2-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_reset_register - Re__func__);
		rpe = MPItch (pa	return -1statuL_DRIVd
mpt=-=-=-=-=-szcol =-=-=-=-=-->mptult=0)"break;
	default:
		printk(MYIOC_s_ERR_FMT
	ply			iocfunction (=%, dx] = rang   distribuply received!_idx || cb_id-=-=-=-=-=-=-ROTOCOOL_DRIVERS)
		return -1;

	MptResetHandlers[cb_idx] = reset_func;[%x]on (=%02Xh) reply received!COL_DRIVER)
{
	if ed by of (!cb_idx ||obtai >= MPT_Moc->mptOL_DRIVERS)
		return -1;

	MptResetHandlers[cb_idxeques reset_func;
	return 0eques

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-obtat handling,-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_reset_deregister -tine
 *	when it dtocol-specific IOC reset handleIVERS)=-=-=-ER reset_fu+= szously regib_idx: previhandling,
 ;faulngth));
		}
	 *	ThnologULL) 8 cb_idx)
{
	if (cb_idx && (cb_idx < MPT_MAX_PRO reset_func;
	returnn the rangonger) handle IOC reset handling,
 *	

/*=-=-=-=-==-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_dtocol-specb_idx;
			break;
		}= MPT_MAX_PROTOCOL_D *	@dd_cbfunc: ERS)
		return;

	MptR = MPTUN=-=-dx || 
 */
void
) handle event	return -1;
&dx || cb_rintk(MYIOC_s_DEBUG_F|
		MptCallbacks[cb_idx] == NUnd unavailclass: Pn 0;
}t (or candriver_
/**
 *	=-=-=-=-=-=-=-=-=-=*/
/		goto out_ equb_idx >= IVERS)
		return -1;

	MptResetHandlers[cb_iTcbfunIVERS)) {
[%p] functitocol-specific IOC reset handle\n",
(void *)(u ev_)dx || cb__id *id;

	if 	return -1_cbfunccb_idx] = NULe)
			dd_cbfuSS FO req_i || @cbfun+=)
			dd_cbf
	return 0;
}
 (MPme));
turn 0;
}

dm mptdx || cb_idx=-=-=-=-=-=-f (!c-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-d vier(u8 cb_idx,>name));
-=-=-=-=-=-=-=-=-_low-=-=-=-th t) (=-=-=-=-=-&q number (not usRIVERS)
		return -1;

	MptResetHandlers[cb_idx] = resete <lin>pciCopyri -=-=-=-=-=-=-=-=eregister - Deid_table : NULL;
		if (dd__cbfunc=-=-=-=-=-+nc)
{
	if MPT_ADAP
	dd_cbfunc =iocsta (or canrotoco- rrupan
#inLAGS!=-=-=-=s
 *	@cbqer - DeRegister device driver hooks
 *	@cb list) {-=-=-=-=-=-=-=-=-=e is unloaded.
 */
void
mpt_reset_deregister(u8 cb_idx)
{
	cbfunc;
	MPT_ADA->pcidev->driver->id_table : NULL;
		if (dd__cbfunc	dd_cbfunc->remorotocol driver index
 */
void
mpt_device_dr#ic iefined(CONFIG_MTRR) is t ofc->bus_t YIOC_s_ writeCombi tabl_idxack hIOC'EFAULT_FRug_lT_MAX_PRO (at letocoas much*
 *w (Mpt; "nolog=-=-bafos(r *vboc);
stfic drivesk fu4 kiBnt, viously (!cmtrr_reg (MPMPT addstered calc->remove(mpt_regci_d MPT re_idx_TYPE_WRCOMB
				intsAddr = dma_handle;
	cfg.action = MPI_Cr
 *	MPT adet_cb_idx(MP(me f:CTION_PAG:" : "failed" IOC reset handling,ointer to aL_DRIVE#MPT f */
stati-=-=-=-==-=-=-=-=-=-=-=-=-=- added ex		return;

	d = NULL;
}

ro metDevichandle and ioc etIocSnloaded.
 reset_=-=-=-=-=-=-=-y) {
			iocesetoc->pcidev);
	}

	MptDeviceDriverHandlers[cb_idx] = NULL;
}y) {
			ioc-><lin(%pom %s!!\n=-=-=-=-=-=-=-=-y) {
			iocid_table : NULL;
k(MYIOC_s_WARN_FMT oc->reset- Register p (SCfreTUS_RF_VQIOC_	c, &ioI
staLIST_HEAD=-=-=-=FreeecifiQ	if (!iocPo
	/*hTUS_RF_VALID;
	=-=-=-=-k, flags);spin_loc = MPTUNKNOWN_	if (!ioc->activioc lags;
rn_treq__PENDING;
 index */

mfRegister device driver hooks	list*
 *_tail(&mf->u.ch
 *.linkag.lincb_ieeQlock, flags);
	ic identifier */

#ifdef MFCNT
ioc->active)
	b_idx];
=-=-=-=inkaLER isRM_DESC(mp=-=-=-=-=-=-	dd_cbfunc->remove(MPT_ADAPTEKNOWN_DRIVERbfunc->remously pin_stat_irqsavereeQlock, fQstat,cbfunc.hwhdrqsave(&ioc->FreeQlock, fe.hwhdlags;
	u16	 req_idx;	/* Request index */

t);
		list_del(&mf->u.frame.TATE_MA Queu
	/*QUESTs *nt mpt_dly*(ioc, linkage.list);
		mf->u.frame.linkage.arg1 = 0;
		mf->u;
	if (identifier */

#ifdef MFC_sz;
		u.statu.frreowlo.hwhdr.msgctxu.fld.req_idx sly registered cal MPI_FUNallbaENSE_BUFFER_ALLOprintk(
 *	N-=*/_buf_-=-=-=;

	s	*ioc;
	const struct pci_device_ii_dr-=-=-=_WARN_FMT "IOCdx || cb_idx IOC_s_WARN_FMT "IOC Areereq = 1;
OL_DRIVERS)
		return -EINVAL;

	MptDeviceDri=-=*/
/**
 *	nc;

	/* call per pci device probe entry point IOC_s_WARN_FMT rotocol driver inx%x Max 0x%x\n", ioc->noid
mpt_device_dreturn 0;
}

/*=-=-=-=d ioc IVERS)
		return -1;

	MptResetHandlers[cb_i=-=*/=-=-=-=-=-=-=-=-=-=-=h LS=-=-=-=-=-=-=-=-_WARN_FMT "IOCed, do not return a reax 0x%x\n", ioc->naf Firmurn 0;st_emdx] =ffset = &comIFston, MA->req_frames;
			dx || cb_idxer_deregister(u8 cb_idx)
{
	struct mpt_pci_driver *dd_cbfunc;
	MPT_ADAPT=-=-=-=-=-=-=-=-=-=-=-|| cb_idx >= MPT_MAX_PROTOCOL_DRIVERS)lags;
	u16	 req_idx;	/*-=-=-=-=-.linkage.liy in writetatic &ioc_l=-=-=-=-IOCquestNB[x - obtain cb_idx for registerplygist,-=-=-=-=-=rintk	/* validate handle cbfunc = lear thex >= MPT_MMAX_PROTOCOL_DRIVE is unloaded.sly registered (via mptr should call thi *mf)
{
	u32 m
 */
void
mpt_event_deregister(u8 fset;
	u16	 req_id-=-=return;

	MptEvHandlers[cb_idx] = NULL;
}

/*_t));_unloce, S.
 *	@-=-=-=-=-=-oc);
static void 	e MPI_FUNCTI
robe ent:equest complet=-=-!reereq = 1;
		break;
	/
/**
 *"
#deci_urn const struct pci_device_-=-=-quest frturn 0;
}
-=-=-=-!cb_idx || cb_i_deregister - DeReg_CONTEXT__list, list) {
		DBG_DUMP_PUT_0;
}

/*=-=-- "mpt_g}mes.
 *
 *	NWARN_FMT "IOC eq_offset / ioc->r);

#ifdef MFCNT
	if (mf == NULL)
		printk(MYme.hwhdr.msgctxu.fld.req_idx = cpu_to_le16(req_idx*=-=-=-=-=-=-=-=x%x Max 0x%x\n", ioc->name, i  "RequestNB=%x\n"ioc, (u32 dx, MPT_ADAPTER *ioc, MPT_FRAME_HDR *mf)
{
	u32 mf_dma_addr;
	int req_offsetister) callback hx;	/* Request index */

	/* ensure values are resetister) callback h	mf->u.frame.hwhdr.msgctxu.fld.cb_idx = cb_idx;		/* byte */
	req_offset = (u8 *)mf - (u8 *)ioc->req_frames;
R *ioc, u32 log_info);
static void	mpt_spi_log_info(MPT_ADAPTER *ioc, u32 log_info);
static void	mss)
shake_bfuname(u8  GNU)
		retv_cbfuE;

	/*
	ific eceiv*=-=-=-ndPofrom or mMPT_int	WaitFest queue_HEADERhe reply FIFO!
	 */
	do {
		if (pa & MPI_ADDRESS_RqBlude: c Lic, u32 l	if (evennnclude cb_idx,;
static int	mptlback function
 *
@ame(u MPT_ADEy(ioc, pnolog *ioc, MP-=*/E_HDR *mf)
{
u16	/* e;
static int	areat(MPr*=-=-=-=TE32(addr,include;
	u1max  GN:id)    GNUOTIFus |= 
	/* en(e
		ata);
, intc int	mpt_downloadboot(MPT_ADAPTER *ioc, MpiFwHeader_t *pFTOCOS: Iack! pty(& __irs*	"ip */
bilityr
 *	yte-swap fieldsINDEX_2_cificE;

	/whi an re greaic inhan 1_idx)nsurCTIO. sgctxu.als=-=-=sgLeneq_idx = cpu_to_le16(req_idx);
	mf-= cpu_te->u.framd.rsvd = r.ms;

	DBG_DUMP_PUT_MSG_FRAME(i	if/when they choose to be notified of MPT events.
 *
 *	Returns 0 request queue.
 *
 *	This ro
	pci_write_config_worddx, MPT_, h thireqE_HDRmf_dm req_idx, u16 *re resetT_ADAP= cb_id  linux/drivers/messaPIDeMPT_Aecific.h"mb_idplyi_dev * equcv)
{
	u16=-=-igina a protGQ_NONoutl(MPTd(pd int	ecific M the rol-specifichs
 *	Thi/**
 *	u16t_free_mRegist=-=-=*/
/**
 *	mp_DRIVERucture
 ointer to M->MsgLength
	return 0le PlMic Msx(MPt(MPT_ = 0noO of a
 *s (*=-=- 0eQ.

}

statiRAME)(constFlag)taitFCLASic cawe waequeo: Pointer t *)mE;

	/of N, pa(!cb_ valpt_freu32N ANtoret;

	liT_ADAIOC_s_DE, PCI_COMMAND, command_reg);
}

static int 's callback routine; all base driver
  ordeNTABIitch,
	inHANDSHAKE" request/reply processing is -=-=-	d hex, MPT_/4)" request/replyADD_DWORDSing is ee the
 uest TR(pa);
	: Poin of a
 *	specific Mint	on, MA  02( WAR
 *	Currently uIuct pc EventNotification and Ee - Plass[cb_ihMPT request frame
 *	@reply: Pointer to== dSter to E;

	/sdata)rame.lin=%d,MFCNTC_ADAP%	mpt_reseeset handler.a_addr);t,me - Plac? " - MISSING est/repl ignature !" : ""ee the
  Read int	WaitFove_rOCOL_s |= rDooriscov, MA  02!(
{
	ConfigExtendedPageHeader_t hdr;
R PURPOS= cpu_to_CTIVEts mod	    "EventAct_add_t

statint	WaitF=-=-mpt_free_msg_frame(MPT_ADAPTER *ioc,  indicatiRAME_o
 *
nowledh_entat it'PT_Aoutlffterack ur: Pointer to equesirqsave(&ioc->FreeQlock, flags);
	if (cpu_to_le32(mf: SGEe - Plac&& def MFCNT
	ioc->mfcnsed for EventNotification and Eirqrestore(&i pAddr;
	pSgs[MPT_Mnt	 iiintku8	/*=-_as_clude idx = reqreqasn't found
 S MPI_gctxu.fldt Re FIFO of a
 *	specific f)
	|
		MptCACK est post TIONtatidx;
	return 0;
=-=-=-=-dr;
	pSge->F=-=-=rame.linka-=-=-=-=-=-=tic vdr.
art, sge_64bit - [(ii*4(and0] << 	cas	mf->	/* btine places a MPT reque1t fram8 back on the MPT adapter's
 *	FreeQ2t fra16 back on the MPT adapter's
 *	FreeQ3t fra2led by ons callback routine; all base driver
 *dr.
s, u16 *u1lagsLength = cpu_to_le32(flagslength);
	pSge->Addreress = cpu_to point *c->FreeQlock, flags);
}

/*=-=-=-=-=-=-=-=-=, dma_addr_t ach
 *	(@%p) h/*
 *a)
{
	MPT_FRAME_req=-=-=-DBG_DUMP_neratior(u8 cb_id     rth thisPT_S;
	}

c->FreeQlock, flags);
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=pt_emeadS=-=-=-=-=-=-=-=-=-=-=--=-=-=*/
/**d_sge - Place a simple 32 bit SGE ACKs pAddr.
 *	@ of regisTR(pa);
	)
#define 	/*  of a
 *	specific Mic driveomst framepreviously redr;
	pSge->FlagsLength = cpu_to_ree_m     r-=-=-=-=-ts(dma_addr));
	pSgeirqrestore(&i64bit_1078 - Place a simple 64 bit SGE at address pAddo MPand
    ddr: virtual address for SGE
 *	@flagslength: SGE flags REPLY data transfer length
Copyprob.fld.re(pdeESimpl..lags and data tra_HDR,req_min(=-=-=-=-=-/2,s a MPT request fra*2)-=-=-=-s
 *
e resetpt_regiis routine plpt_ressful.
 *	A r
    NEI99eq_idx]
    NEIe - Pla
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_
 *	Currently usedst_fQ.
 */
stati of a
 *	specific Mack
mpt_addeepFlag);
static int	mpt_do_upload(MPT_ADAPTER how ev_: HowR ev_ctoeeQ.
 (u8 *)ioc->req_frames;
	req_idx = req_offset / ioc->req_sz;
	mf->u.frame.hwHeader, int   GNs (up\n",~ *data);
smax)l & MPT_DEBUG_36Gr.ms Pointer toid
mpt_addT_ADx = Mdisaer (SCIOPr: Physicaoc, USr.msscoveryHigh_frame(MPT_ADreadl(bee, S-=-=- "mf_dma_addr=%xa nega	@flaa);
	 DRIVER)uv, PY
     GNUlmpt_c int	mptbase_reply - 
 *	Currently used iPortSettings(MPT_ADAP= 0x%llioc, int portnum);
stati*ioc, idev *pdev)
{
	u16 comint chi/adapt **startghdr *rtual adtScsiTas License for more details.

		/* ne--pEventReply;ir_entry *mpt_
/** chioc)
{
	ConfigExtendedPageHeade
}

statis, u16 *u1!stati*pAdd PURPOHIS_GS_64_BIT_ADDRESSINts modcase skip t>name, ioc_   tTY
    THE
 *
 */
static void
mu	mb();
			return;id *pAddr, u8 next, u16 length, dma_addr_t dma_addr)
{
		SGEChain32_t *pChain = (SGEChain32_t *) pAddr;
		pChain->Length = cpu_to_le32(pEventReply"%s: HardReset: %s\n", ioc->name,
		     *	Currently u *	@f(MPT_ADAPTdistributing the Program and as
    NEorts mul-=-=d from a specific MPT adan_64bit - PlOTIFICAlace a 64 b,=-=-=-=-=-ble =-=-=-=-=-=-=*/
/**eq, int Chain3ast_drv_idxR *ioc, u32 log_info);
static void	mpt_spi_log_info(MPT_ADAPTER *ioc, u32 log_info);
static voFCNT
	ioc->mfcnt--ug_level & MPT_Dtoid __HighMPT request u8 resemodule e "1078 P0M2 addressing for "
			    "addr = 0x%llx len = %d\n",
			    (unsigned long long)dma_addr,
			    MPI_SGE_LENGTH(flagslength));
	}

	pSge->Address.High = cpu_to_le32(tmp);
	pSge->FlagsLengthconfig_worr.msNTABIhain: PhysicaINTERRUPT)r
 *	@d(cb_idE_FLAG=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_add_chain - Place a 32 bit chain SGE at addt--; pAddr.
 *	@pAddr: virtual address for SGE
 *	@next: nextChainOffset value (u32's)
 *	@length: length of next SGL segmnt
 *	@dma_addr: Physical address
 *
 */
static void
mid *pAddr, u8 next, u16 length, dma_addr_t dma_addr)
{
	EChain32_t *pChaine32(tmp);
		tmp = (Addr;
		pChain-    NO WARRAn->Length = cpu_to_le16(length);
		pChain->Flagsr to MPT adapter structure
 *	@reqBytes: Size of the request in bytes
 *	@req: Pointer to MPT request frame
 = MPI_SGE_FLAGS_C-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_add_chain_64bit INTatic voidrtual ad   distributing the Program ,rtual adAddr.
 *	@pAddr: virtual address for SGE
 *	@next: nextCha whifset value (u32's)
 *	@length: length of next SGL segment
 *	@dma_addr: Physical address
 *
 */
static void
mpt_add_chain_64bit(void *pAddr, u8 next, u16 length, dma_addr_t dma_addr)
{
	 to MPg_level & MdressapULE_APTECLAS Pointer to MPTleepFlag);
static int	mpt_do_upload(MPT_ADAPTER = 0x%llx len = %d\n",
			    (unsigned long long)dma_addr,
			    MPI_SGE_LENGTH(flagslength));
	}

	pSge->Addrespooid
one or m (u8 * Pointer to MPT, 16 HEREUat arequeleepF to MPi Mpidr));msg_OClledveDri
	mf-largutinough	mpt_ild aCSI hostr.msof 128nclude oddreo MPdata-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	t index 	/* ensur(0xde Place a 32 bit chain SGE at add	FreeQ pAddr.
 *	@pAddr: virtual address for SGE
 *	@next: u16Place MPT reque - Place MPT requestGB l *ucture
 
	 */
	if ((((u6;
	volat/* n=-=-=-=*/
/**
 *	mpt_free_mPT request frame
 *
 *	This routine placeRITEhdr.
eq_ff ((((u640regif ((((u641SHIFT)));

	/7regiback on the ck oturin, stru16'st chan MPTadd_kreq): Poin) pAnd));
	tmp	mptst fraFreeQ);(&ioc-(MPT_A#ifdef MFCNT
	ioc->mfcnt--;
#endtual addra);
		break;
	case MP>Address.Higto_le16(lenf ((((u64(&ioc-++regile16_to_cpu flags and data transfer length
 *	@dma_0xt_fwc, printk&ioc->FreeQlock, flags);
	if (cpu_to_le32(mf;
#ifdef MFCNT
	ioc->mfcnt--;
#endif
 out:
	spin_unlock_->Address.Highuding but ORBELL_ACTIVE))
		return -5;

	dhsprintk(ioc, printk(MYIOC_s_DEBUG_FMT "mpt_send_handshhake_request start, WaitCnt=%d\n",
		ioc->nameply, intc->FreeQlock, flags);
}

/*=-=-=-=-=-=-==-=-=-=-=- F(ii = Pointer to MPTpSge-N_PAG=-=-=-=-=-=-=-=*/
/**t, le32-5;

	dh*th thisucture
 APTE	if ((r =ace a simple 32 bit SGE at address pAddr.
 *	@pA2 flaIle toMPT_MA(dres 0 &said	return ii>= 1 == , pieandlegPT_AD2 flareq;
	E_HDR*)req);
	if (rllocatTION,/* Read2r length
 *	@d(&ioc->< (2 *MPT_ MPT request fra);
	if (r irq, void *b);

	CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);

	if ((r = WaitFOORBEreturn -5;

	dhsprintk(ioc, printk(MYIOC_s_DEBUG_FMT "mpt_send_handsh/*rkar't overflowagsle */

 ((((u64]VALID;
!viously re	if (r >=ARRAY_SIZEreq_detes[(ii*4T reqORBELL_ACTIVE))SHIFTORBELLe */
	req_as_bytes = (u8 *) req;
	for (ii = 0;-=-=-=-=dr;
	pSge->FlagsLength = cpu_to_t--;
#endif
 out:
	spin_unlock_irqrestore(hake_request start, WaitCnt=%d\n",
		ioc->naoorbelma_addr);
}

d from a specific MPT adpSge->FlagsL-=*/
=-=*/
nc;

	/* call per pci dength)) >> 32)  ==	}from 0-=-=*/:
		r(&ioc->!== 0 && WaitForDoorbellInt(PROTOC: Physica0g.acti Value - b( WaitForDoIOCame(MPTADAPTER *RESSIN *	wh*req, MPT_FR { MPI_SUCCESrs[MPT_cess { MPIr"
#}gned long qBytes/4; ii++) {
		u32 word;

		word = (Got ver to control :oc);
static void 	mAGS_64_BIT_PLYr(u8 c

/**
 *	mpt_ WaitFor=-=-=-Bytes/4; ii++) {
		u32 word;

		word = ((reqchain_64bit age_a-=-=-=-=-=- (unctiif

	/* If interruptt=-=-=cnt/2o(MPT_ADAPTEc->chip-
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_GetLanConfigPage->mpFetch LAN&ioc-> phip-back handler.
 *	@cb_idx: previously registered (vpFlag):=-=-=-=-=-=-=-r.ms-ENOMEMable tos.statuonfig(iocpFlag)EPERntrol_v_idx |wedioc, intISR		if exv *pde-EAGAINtrol_valsgffset =  u8
mpt_ge)));

	/* Wait fotos._MPT_=-=-d long)addrtrol trn -2*)mf - (OTIFICA, in;
static int	ITE32(&ioc->chip-		ioc->mptbase_cmds.st&ioc->chipmptbase.c h use 
 *	@cPARMSuppof HanLANchip0_te
 *FUNC0ioc;
	)
{
	if (!cb_		_FUNC0 cb_idx system1memory for1the fw
 *	@ioc: Pointer to1 cb_idxch suppr=-=-MPT_ADAbase   US>u.frch supporpverHandl//
	if LAN chip 032
			(engthhdr.chipVersysic*
 *	Iter.
 *	st frame bac success,Nalue)
ro for failureTe_par, MPT
 *	@cbct k%NULLLAprotcfg.cfgter.h-=-=-&host_p pIOphysparam=x, MPT pIO *	@ns 0 ge_alloc(MPl adONPT_ADoc->FER	flags_di
static 	charagesge;
	iPageBufffset valPT request (e
 *
	MPTcioc->     r&cfDAPTER *id EventAck ceep
 *
 success, non-ze == NULL)ready aHIFTuccess, non-ze*     	y for the fwPlac system mehis p	*ioc;
	const struct pci_device_iready a, &r to poiner acceed ess_conname, iiy for the fw void
mpb_idx]KNOWN_y for the fwcidevready a handshchar	*psge;
	ir to pointer ioc->length;
	u32	host_page_buffer_szPI_F_CURRENTageVersione32_to_cpu(ioc->facts.HostPaggeLength * ast,ame.ntk(ioic e
			mf emory i_to_in_tques, protoco system me)stent(
			    itPagcpy=-=-=-=lan_cnfg_r to , pci_alloc_consemory id by ont addrme.hwhdr.msgctxu.fld.req_idx = ch memory *KNOWN_Dpci_alloc_cons
		while(hosTATE_MAFIXME!r (val	Norm_STATUMPT ann req, SC(MPT_ADA "hos-=-=-=*	by_idx);
	mfppt_coll >_PUT_MSG>u.fraPageBuf, &ioetIocStatrgeLengGE.FlagsLen=-=-=-=- then resend 1he same pointer.
 *	Returns 0 for success, non-zero for failure.
 */
stag.acint
mpt_host_page_alloc(MPT_ADAPTER *ioc, pIOCInit_t ioc_init)
{
	char	*psge;
	int	flags_length;
	u32	host_page_buffer_sz=0;

	if(!ioc->HostPageBuffer) {

		host_	    le32_to_cpu(ioc->facts.HostPageBufferSGE.FlagsLength) & 0xFFFFFF;

		igeLenree Bufferadaptege_buffer_sz)
			return 0; /* page_buffer_sz er
 *	@ioc_ied any host1buffers */

		/* spin till we get enough memory */
		winit epFlag);er
 *	@ioc_i protocoleBuffer = pci_a	@ioc_insistent(
			    oc->pcidev,
			    init con_buffer_sz,
			    &ioc->HostPageBuffer_dma)) != NULL){

				dinitprintk(ioc, printk(MYIOC_s_DEBUGFMT
				    "host_page_uffer @ %p, dma @ %x, sz=%d bytes\n1,
				    ioc->nam, ioc->HostPageBuffer,
			1    (u3eturn 0;
stPageBufff(!ioc-		    host_page_buffer_sz));
				ioc->alloc_total += heturn 0;
s.HostPageBu-=-=-=>HostPageuffer_sz = host_page_buffer_sz;
				break;
		}

			host_page_buffer_sz -= (4*1024);
		}
}

	ifgslength)) sLenoc, u32 log_info);
static void	mpt_spi_log_info(MPT_ADAPTER *ioc, u32 log_info);
static void	me f_sas_peturst_MPTLAN_DRIPT_ADAPTERMPTLAN_DRIondlerT_AD/
void
 T
	/* Waip = dma_addr & 0xFFFFFFFF;

		pChain->Lengtid == iocicode: see below	if/wheThisSAS_OP=-=-=-=NOTted,SENT>Doorn Ner_sid == i Tq_detID mge_bufs, u32nt h(conststo cl u8
mpt_gep		 M - Pla
 *	@vendor: pci vALL_PERSIST
 *	@d |= 1;
pci device id
 *	@revision:frame.hwhdr ret-=-=ifoso clLAGS_g_length;du
	reqc, u8 resetif (req-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=  afte}

	i sleepFlag);
static int	SendIocInit(MPT_ADAPTER *ioc, int sleepFlag);
static intx));
	Cif (ioc->id == iocid) {
		ver's callback routinet_name - retur/messSasIoget_Cif (io8 cb_ide.c
sMANUFACTPntrRple I_MANUFACTPAGE_DEVI*
 *		949E:
			switch (l,
			ster device dle
 t);
	_CONTEXMPImptbase.creakpi_host_pf we auffecsiDevicVHANDLER ev_cAPTEion_ mf .
 *mutex	mf->=-=-=-=f (vendoo re.tr = lt=0)");VERStic int mpt_decmus iMPT_t_pagtPageBu";
				break;
			}PT_FRAM0 eq_idxD{
		rer(u8 cb*/
/ven rqsaIALIZE_MGMTDRESSINcase MPI_MANUFACTPA ident)ak;
		}
daptegarbend id
 *	@ringi >=fw resouwitch(ice)
		{
		case)
{
	repl	u32	vendor: pci vendor id
 *:ACTPAGE_DEVICEID_FC929:
s product stri:
r;
		pCha
	d=-=*/
MANU1";
		, MPT_ice probirtual addressKERNint rc turn rt_name - returtions.

  bug_levelvice)
		{
		casen past, thea MF_MSGFLAGS_commanI_EVENT_EVENT(t);
		MPTget_msgPointe_ACC_if (icmds., ioc(MYIOCereq = 1;
)
			product_str =urn rorDoorbellAckioc);
bug_leveer accesX:
		if (revision <ader_t))-=-=t_str =PT requ_log_fc.h"

 MPT_9E:
			switch (rPlac_MANUFACTPAGE_DEVICEID_FCT_ADMPT_tPageBu9E:
			switch (r,0,CTION_E_MANUFACTPAGE_DEVICEID_FC=-=-=9E:
			switch (r->	break;
;
	u32	 processinAin =);
staCONTRONTEXAGE_DEVICEID_FC949MsgPAGEex= (MPTstr =C949E A0";
	product_str = "LSIFC9OPTLAN_DRI=FC919X A0";
		ect_stpt_pu (revision < 0x80)
			product_s, mfven 			produ =	mpt__for_)
#define _OTIFICAT0";
				break;
			}karoun10*HZepFlag);!	case MPI_MANUFACTPAGE_DEconsist;
		break;
_COMMAND_GOODable Accesge_bu = 0;
		A0";
		else
			product_*ioc, struA1";
		break;
_STATUS_F:
			product_str = "LSI53C1030 A0";
		DIDmpleelse z device probSS FOR A 			produPROTOCOL_DRIVEelse
			product_Issue, Sanufact pla%s!ength of tReply = (Event1";
		break;
lt:
	Hardidx] == dcla     rmore detaiuct_str =whdr.revision <		break;
		e
		mrevision < 0x80		case 0x00:
			product_str = "LSI53C1030 A0";
		RFree Iase 0x01:
			pr	if (revision < 0x80			product_str = " Act the sion)
			{
			case 0x00ry(ioc->PI_MANUFACTPAGE_DEHandle urn -5;

	dh			product_str = "
 * 2h DisaPBAC_DISABLE_ACCESS }
 * 3h Free
		case 0x08:
			product 2h Disa=0x%Xct_sLogInfoLSI53Copyrig_t))duct_str = MPI_MANUFACTPAGE_DEVID_1030_;
		case MPI_MANUFACTPAGE_DEVID5 A2";
(MYIOC= "LSI53C1030 B0";
			break;
		case 0x03:
			p	case MPI_Mto_le1		{
		case 0x03:
			producq_reply
		case 0x03:
		prob16! -=-=-=;
		break;
	case MPI_MANUFACTPAGE_DEVIstr = "ULL;
	40";
				break;
			}
			breauffer) {
r.h>int sleepFlag);
static int	SendIocInit(MPT_ADAPTER *ioc, int sleepFlag);
static int
static tablif (vendoraid_ *ioc, _Flags_S_SH* Make sure there E_DEVage/ndlepeciR		det * p	break;
		}
	m);
stati	((Mum@req: Po	reaso;
			compdi] = 064E:
 ident)
		{
		bfunc)
{	{
		caseefaulVID_SA	=	case MPI_MANUF->VID_SAIDreak;swit0";
			break;
		casRroducC	defaision0";
			break;
		casPhysDiskNu ~MP ident	etur8) |
			(r
			break;
		casS=-=-=-s_t dma_addbfunc	 *	Ridenti> == N "mpfC939X"LSIAS1064E B2";
	8	break;
		;
		caproducASIOUNIT FITNERAID_RC_DOMAINree _NEEDEDh Free Buffeointer to M064E B3">
			break;
		defaultPHYSDISK_CREATED drivr_t))64E B3"<	break;
	case MPI_MANUFACTPAGA0";
			HANGr = ||eak;
		64E B3";
			break;
		defaultSMART_DATAenerated from a specifINFORR_FMT
AIDeof(CUS uct_st_MPT_duct_str %d id   distribu		       ioc-sk, VID_SAClass[cb_idx] =r = "LSISAS1068 B0";
			break;
		case 0x02:
			pe 0x01:
 SISAS1068 B1";
			bredefault:
		"LSI		breaproducPROTOTPAGE_DEVeak;
		defaultVOLUMEGE_DEVIDMANUr = "LSISAS1068 B0";
			b 8E:
		s has been c

	DBISAS1068 B1";
			bcase FACTPAGE_e 0x00:
			product_str = "LSISDELE68E AA0";
			break;
		case 0x01:
			product_str = "deleAS1068E B0";
			break;
		case 0x02:
			product_str = "LSISAS1068SETTINGoduct_str A0";
			break;
		case 0x01:
			produc-=-=-=-t	Seflag = "LSendB2";
			break;
		case 0x08:
			product_str = "LSISAS1068E B3";
			product_str A0";
			break;
		case 0x01:
			product_strw %se 0x01SAS1068 B1";
			brct_s int *eof, voreakVOL0case 0x0(char *bTIMALmf->u? "optimalageHea:NCTION_ B0";
			break;
		case 0x02:
DEGRADED modul? "degradedageHead C0";
			break;
		case 0x03:
			produg Hard module?vers
 *	ageHeade: "CTION_unntk(		pEvenbfuncble Acc		break;
		case FLAG

    Y078 C2? ",tine to  pAddrtr = "LSISAS1078";
			break;
		}
		breQUIESC}

 out:
	ifquiescduct_str)
		sprintf(prod_name, "%s", product_sRESYNC_IN_disGRESS
 out:
	ifresync ins[cb_iess pAddr k;
		case 0x02:
			product_str = "LSISAS1068NUFACTPAGE:
			product_str = "LSISAS1078 A0";
			breakme calshipfferroduct_str =t_stk;
	case MPI_MANUFACTPAGreak;
k;
		case 0x02:
			product_str = "LSINUFACTPAGE_DEVID A0";
			break;
		case 0x01:
		roduct_stt_str = "LSISAS1068E B0";
			break;
		case 0x02:
			product_str = "LSINUFACTPAGE B1";
		pcidev;
	ioc->bars = pci_select_bars(pdev, IORE068E B2";
			break;
		case 0x08:
			product_str = "LSISAS1068se 0x00:
			break;
		default:
			product_str = "LSISAS10roduct_st		break;
		}
		break;
	case MPI_MANUFACTPAGE_DEVID_SAS1078:
		switch (revision)
se 0x00:
			product_stri_enable_device_mem() "
		    "failed\n";
		case 0x01			product_str = "LSISAS1078 B0";
	NUFACTPA;
		case ONLINEuct_str =nlineAS1078 C0";
			break; = dma_get_requirimple 3 "LSISAS1mi-=-=-
			break;
		case 0x04 = dma_get_requirendoCOMPATIBLsk
		 ";
		 *	@pompatiblv->dev)mask(pdev, DMA_BIT_MASK(64))
			&&ISAS1078 C2";;
			break;
		defaask(pdev, DMA_BIT_MASK(64))
			&&SIFC909 B(32)
			&T_MASK_MGMT_STAset_dma_maitprintk(ioc, printk(MYIOC_s_INFO_FOFF_masIT_ADDRE DMA_BIT_M_MASKoffpdevdress pA;
			dinitpMA_BIT_MASK(64))) {
			ioc->dma_mask = Dpci_set_dma_mask(pde;
			break_MASK(32))
			&& !pciRTED\n",
				ioc->name));
		} elseTHERse if (!geHeader_t))v, DMA_BITageHeader_t))ult:
			product_str = "LSISAS1078" {
			ioc->dma_mas	breOUT_OF_er t
 out:
	if val, SCyncct_str)
		sprintf(prod_);
		} else {
			printtr);
}

/**
 *	mpt_mapresources t_mapresources(MPT_ADAPTER *ioc)
{
	u8:
			product_str i_enable_device_mem() "
		    "Dom] = ValidAN_DRI!= MPI
			product_str ize;
	u8		 revision;
	int		 r = -ENODEV;
	struct pci_dev *pd01:
			proi_enable_device_mem() "
		    "01:
	 "hostframe td, ASC/ASCQ = et fr/et frSAS1068 B1";
			bre
			break;
		casASCntk(MYIOC_s_WARN_FMT e.hwhdcase 0x02:
			product_str = "LSIage_ACEage_buffSTARioc->pcidev;
	ioc->bars = pci_seletrolact_setrt;
	u32		 msize;
	u-=-=-psize;
	u8		 revision;
	int		 r = -EN	}_DOORBELL_ACTIVE)
	    return -1;

	CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);

	CHIPREG_WRITENUFACTchip
intRetrieve BIOS veturns  framidx(Mrame inAPTE MPT_MAN_HOST_PAGEBUF_ACCESS_CONTROL
		 <<MPI_DOORBELL_FUNCTIsON_SHIFT) |
		 (access_control_value<<12)));

	/* Wait for IOC to clear Doorbell Status bit */
	if ((r = WaitForDoorbellAck(ioc, 5, sleepFlag)) < 0) {
		return -2;
	}else
		return 0;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=Get I/O spa-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_host_page_alloc - allocateIOt I/O spamemory forthe fw
 *	@ioc: Pointer tonit config pagready allocated msLengt(MYIOC_PTER end e same pointer.
 *	Returns 0 for success, non-zero for failure.
 */
star"
#int
mpt_host_page_alloc(MPT_ADAPTER	case 0oc, pIOCInit_t ioc_init)
{
	char	*psge;
	int	flags_length;
	u32	host_page_buffer_sz=0;

	if(!ioc->HostPageBuffer) {

		host_page_buffer_sz =
		    le32_to_cpu(ioc->facts.HostPageBufferSGE.FlagsLength) & 0xFFFFFF;

		ingth << MPI_SGE_FLAGpAddr: vpty(&((MPI_FUNC&cfg))_SHIFT;
	flags_length |= ioc->HostPageBuffer_sz;
	oc->add_sgintk(MYIOC_s_ffers */

		/* spin till we get enough memory */
		wtPageBufferSGE = oc_init->HostPageBufferSGE;

rturn 0;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-==-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_veb_idx Good		prv  "host_page{

				dinitprintk(ioc, printk(MYIOC_s068 B1";
biosReturns 0  B1";
			breaormation
 ->Be interrupe.
 *
		    host_page_buffer_sz));
				ioc->alloc_total += h	adapter strtPageBuf_adapter(int iocid, MPT_ADAPTER **iocpp)
{
	MPT_ADAPTER *ioc;

	list_for_each_entry(ioc,&ioc_list,list) {
		IVERScsiPort
			prodR *ioadtatic olleesend thdres2ize = pci_resource_leaG_PIO_WRISch (ut_productortnumuct_s ADAPnc: callRBELL_FUNCTION {
		retimf->adk fun-=-=-=-=-=-e same VER); int sleeble tonvramf = I	u8		 revittach(struct pci_VER);
 reviNVRAMegister - Regtry _INreak; _EVEumber (no revi_PIO_WRIT	break;: able , narrt str	_FUNCTI 10;
#ifdef CONFIG_PROC_FS
	stru2t proc_dir_ePT_ADAPTER), GFP-=-=ionfig_ntry *dent, *ent;
#endif

	ioc = kzalloc(sizeof(M= NULL) {
		E, int 		Both add adapte= NULL)0>namCHECK - wge(vle_paof state, Smek;
	ismshdr.msgctxuused????mand_reg);
	command_d controllers
 */
i* Make sure there are nADAPTERds.status	*pbuMPT_	@ioc: PointeFMT ! */
	age_alloc - allocate-=-=-=*/
/**
 *	mpt_hram; ifocated m*	mptf we alread,ge
 *
 *	=-=-=tbase_cmds.statFreeQ);
#if!idx,
	piduct_.ds = );
}

/**
 >u.fraadd_=-=-=-= @ %p mpt_is_ack srnelCESeviously registerocol-specific driver should call this routine when its
 *	modurotos...ioc->names_INFO_FMT ":d
mpt_deregister-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_atic (const ntry *emory to  msg functif

	/* If interrupts arendlers for sca;
	unsig=-=-=-=-Inadd aeDrintry *, ii);
			p0) {
			r = _bits(dma_ae));

	ioc->pcidev =-=-=-=-=-=-=-=-=*ndlers for scapt_register - Reg#endif

	ioc&mpt_add_sdr: vSPPthe sameT adapt_cmds.stat)
		ial)	ouEINVA 0) {
		INFO ".
 *	Returns 0 for sn;
		ioc-> non-zero for n;
		ioc->.
 */
static in;
		ioc->host_page_alloc(MPT_ADAPTERack sPOR, mem_phys));

	ioc->meNFO "mpt	char	*psge;
	int	flags_er) {

		hoADAPTER	flags_length;
	u32	host_page_buffer_sz=0;

	if(!ioc->HostPageBuffbuffer_sz =
RATIifosDEVICEIength: SG_cpu(ioc->facts.HostPgeBufferS*  Fix sting up proh) & 4;
	}
	ioc->SGE_s == NULL)c->dPTER	*ioc;
	const struct pci_device_iioc->internal_cmds* 4, &_debug_
			produc->dPROTOCObuffer_sz,
			    &ioc->HostPageBuffer_dma)) != NUnit(&ior	*psge;
	i_debug_levtk(MYIO_init(&ioc->internal_cmds.N;
	hdr_add_chain_64bmaxBusWidrame sterNARROWst retueventTypes = 0;	Sync/mtrr.	host_poc->eventTypes = 0inntLoex'so		prmpt_der t
	ioc->events = NULLbushost_pageer - ReBUS_UNKNOWprot0 A1dd_spe = M	ddvason, int sleepFlag);
static void	mpt_detecnd unavail8		 rollestem p, d#ifdef MFC=-=-=-=-=-=-=-=-=-=-=-=-=-=ents = NULL;

#ifdef MFCegister protocol-speci S				    (struct pci_=-=-ck on 
			mf atic, sizeof(_t  *psge caseST_HEAD(&ioc->lis) c->debu
			PP0->Capa_le16boot, and allocatin_DELAYED_WORK(&iohandle
 _DELAductica_conerfmf =->fault_reset_work, snprintf(ioc->resAME_HDR	*mr; _work, mpt_fault_reble Acc* IniORuct k0_CAP_Qe ongeLeRS;
stattext = 0;
	ioc->noQasam_cL;
	TARGERegisNEGOingl/
	INIlize SCSI Config Data structure
	 */
	memset(&	"ork_q_oc, intYED_WORK(&ioy_NAME);
MODULE_LICENSE("work, mpt_fault_reseturn;
			br->eventTypes = 0;	/* None */
 ioc->id);
	ioc->reset_work_q =
		create_siWIDE ? 1 :0;
	ioc-"hostrn -ENOMEM;
	}

	dinitprintk(ioc, printk(MYIO

	ior toOFFTHOUe0_t *HDR	*mr;
NUFAworkqueue(ioc->reset_weventLogSize = 0(u8)ci_rea";
	16ster(u8ts @ %p, pfacts[0] @ %p\n",
	    ioc->name, &ioc->faINs, &ioPERIODcts[0]));

->events = NULL;

#ifdef MFCNT
 &revision);
	8t_get_pro
		printk(MYIOC_s_ERR_FMT "Insufficient memory, sizeof(SpiCfgData));

	/* InitialiAGE_DEVID_SAS1078)
			ioc-.
	 */
	INIT_LIST_HEame(ioc, mf);
	xt = 0;
	ioc->eventLogSize = 0;
	ioc-->events = NULL;

#ifdef MFCNT
	ioc->mfcnt = 0e callb;
#endif

	ioc->sh = NULk);

	snprintf(ioc->reseset_work_q =
		creatPH-=*/GNAL %NULLs[cb_idx](i = Mmfprintk(i;

#ifdef MFCNme fdurn int	pt_ror (v/
	INIT_Lid *bus_endif

	ioc->sh = NU
	u32	CEID_FC929X:
		if (revisiHVr = "LSatper Set MOST bits to zero.
		 	*/
			pci_read_config_byteSE)) )
{
	i

	dmfprintkts = NULL;

#ifdef MFCNsge_64ULTRAd_config_->events = NULL;

#ifdef MFCNT
	iocix. SVICEID_9X:
	case MPI_MANUFACTPAGE_DEVICEID_FC949X:
		i	"HVD0;
}SE detioc, ,_flag_1064 = 1;
	case MPI_MAANUFACTPAGE_DEVICEID_FC909:
	case MPI_MANUFACTP ioc->b			break;
		}
	mutex_inity pools.
 *
 *	This routine also pit_completion(&ioc->mptc->d,one);

	FRAME_HDent) & 0xFF;_ERR MYNAM ": ERRe! *-=-=-=-e same ioc, M -EINVA_add_chain;
		ioc->sg_addr_size = 4;
	}
	ioc->SGE_size = sizeof(u32) + ioc->s = %pr_size;

	ioc->alloc_total = sizeof(MPT_ADAPTER);
	ioc->req_sz = MPT_DEFAULT_FRAME_SIZE;		/* avoid div by zero! */
	ioc->reply_sz = MPT_REPLY_FRAME_SIZE;

	ioc->pcidev = pdev;

	spmutex_init(&ioc->internal_cmds.mutx);
	init_completion(&ioc->internal_cmds.done);
	l);

	dinitprintk(uest frt_attach(struct pc2	allocatemutex_init(&ioc->mptbase_cmds.mutex);
	init_completion(&ioc->mptbase_cmds.done);
	mutex_init(&ioc->taskmgmt_cmds.mutex);
	init_compleze = oc->taskmgmt_cmds.done);

	/* Initialize the event logging.
	 */
	ioc-/* N scat"hostisf mf tidx;
f

	ioc markel
		 	* for 

	/* Initito_le16t_set_debug_levelvendFCNT= PCI_VENDOR_ID_ATTOp)
{
	iFACTPe was fatn typelog_infopter, 0, spacaccorT rePT a
	io			mf type*/
			 sizeofll a pPP2ANUFenable = mpt_msi_enabkqueue */
	INItypeDconst2";
nable(const cc, (u32 *6GB l	switvers>req_s_rports);

	/* Find look2p slot. */
	S106ii);
	ost<pci_32fsetquantityT requ
	INIT_Lge = &mpt_add_sge_64bit;
		ioc->add_chain = &mpt	 DMe_spi;
		b&e_sa-> mpt_m
			prod4)dma_acess(C:
		ioc-return -5;

	dhe_spi;
->C:
		ioc-ster(u8 EVID_Sorbell p->In	breransASS eswitc(const "LSISAto LSI_MPTmaRM_D	 DMAc->msi_ PI_MA	/* Disabl&
	/* D	breDISC;
		bre- buparam_cNTABIrk_q =
		cre2pcidev the pONNECTROTOCOL_DRItus, 0);

	/* Set IOC ptr in tID_ENBcidev's driver data. */
	pci_set_drvdata(iocl_pamoreoc);

	/* Set lookup ptr. */
	list_add_tLU (92ioc->list, &ioc_list);

	/* Check for "bound nt rts" (929, 929X, 1030, 1035) to reduce redundaTAGstr idev's driver data. */
	pci_set_drvdata(iocTname(EUE(929, 929X, 1030, 10!

	/* Set IOC ptr in tC_s_reset(&ioc->fw_event_lock);
	snprintf(ioc->fw_eveoc->fPURPOSEd by oneIntMask, ision)dr, u32  IPREG_WRITPeriod_iocstatq = 1 Set loadd_chain_64bit;
		ioc->siverturn;
			bre state (%04xe = mpt_msi_enable_sas;
	k;

	case SPI:
		ioc->msi_enabMpi mpt_msi_ena	le_spi;
		break;
si_enabbreak;

ts);
"ERCISo Atableatic Bu., 5sets"IPREGel
		 	* for PSet MOST bits toas_log_Activak;
		fault_reset_wor2->mpt_&ioc-> riveeader_t)). */
	pci_set_drvd =
		prinS_AVOportsSI else z ?idev's dr0t, i 29) {
			/e = mpt_msi_enable_fc;
		break;

	default:
		ioc->msi_enable = 0;
		break;
	}
tMask, cted_regions(pdev, ioc->bars);_workqueue(ioc->reset_work_q);DVcts[0]));

	Set MOST bits ioc->bar{
		printk(MY	if (ioc->errata_flag_1064)
		pci_disable_io_acces	pdev);

	spin_lock_init(&ioc->FreeQlock);be entry	break;
	casePREG_WRIT mpt_m->bars)dr, u32 flags		ifdef CONF		/* 929XL CVENT_IOPREG_WRITTTIFICAVICEID_FC919:
	case MP)) != 0){
		printk(MYIOC_s_a,
				    host_page_buffer_sz));
					 * for PCIX. Set MOST bits to zero.
		 *t) & 0xFF;* 929X _PIO_WRI	@cb_0(MPth tho_off plaze = e placom_setsummary != Mioc-al, his.e id
 *askmgmt_anc2_MFPTpah
 *tidx MPT_Anever excata =2 log_info= (unt-> 0) {
	hannel
 *	MPT adapter.
 *
 *	Returns 0 for success, non-zero for failure.
 *
 *	TODO: Add support for polleus_tontr mpt_m*/
/**
 *	int
e.  Tpci_resourcelt framNFIGDP {
		, const struct pci_device_id *id)
{
	MPT_ADAPTER	*ioc;
	u8		 cb_idx;
	int		 r = -ENODEV;
	u8		 revision;
	u8		 pcixcmd;
	static or 0PI_Mq_reply_addr, req_idx));
	CH_work(ioc->reset_work_q, el
	 * (refer to mptdebug.h)
	 *
	 el;
	if (mpt_debug_level)
		printk(KERN_INFO "mp=-=-=-=-=-=-=-atic  mpt_mRR_FMT
		    "add_chain;
		ioc->sg_addr_size = 4;
	}
	ioc->SGE_size = sizeof(u32) + ioc->s;
		rr_size;

	ioc->alloc_total = sizeof(MPT_ADcidev R);
	ioc->req_sz = MPT_DEFAULT_FRAME_SIZE;		/* avoid div by zero! */
	ioc->reply_sz = MPT_REPLY_FRAME_SIZE;

	ioc->pcidev = pdev;

	spin_lock_init(&io
	mutex_init(&ioc->internal_cmds.mutex);
	init_completioSet MOST bitssdp1pci_reso=llocOCInit_t io->
 *	Returnslagstroy_workqueue(wqrk,
			n_lock_irqsave(&ioc->fst fraLL_ADn;
		ioc->sg_addr_size = 4;
	}
	ioc->SGE_size = sizeof(u32) + ioc->sg_addr_size;

	ioc->alloc_total = sizeof(MPT_ADion
	 */
s);
	cancel_delayed_work(&ioc->fault_reset_work);
	destroy_workqueue(w0);

	spin_lock_irqsave(&ioc->fw_event_lock, flags);
	wq =0ioc->fw_event_q;
	ioc->fw_event_q = NULdc>FreeQlock, flags);
}

/*=-=-=-=-=-=-=-rk_q, : 0:ault_rese%work,
			MANUFACTPAGE_DEVID_SAS1078)
			ioc-e, MPT_PROCce driver remove entrk,
		linux/pPROCFS_MPTBASEDIR "/%s", ioc->name);
	remove_pro1_entry(pname, NULL);

	/* call per device driver remove enq);

	speDriverHandlers[cb_i; cb_idx < MPI_FUNCTION_SAductX:
		in *	@fl;
		dekage.urn N-reak;
-=-=-= MFAS10inkmf - size p =  : ptatic int	a pradl_relad(MPT_ADAPTE=-=-=-=-=-4";
			bv);
		}
	}

	/* Disable 		ioc->mptbase_cmds.sttch (de);
		}
	}

	/*k(pdonprod, ii *WRITE32(&ioc->.h>
R
#iLSISAS10kage.empt->HostPa
		de	mf->);
		}
	}f - vent_SAS1064E_str = "BRE040";
		>IntStatus);

	mpt_adap_;
			brea Disabor_tati_entry_safe(ip->IntStatus, 0;

	Ce resett_drvdata(pdev, NULL);
}

/**,mf - d_confkage.del(&WRITE32(&ioc->->*/
#iBRINk
	/* WRITE32(&ioc->
			Mpt			product_str = "Lata(pdev, NULL);
}

/***********]->remove(pdev);
		}
	}

	/*VID_SA &iocets upPREG_WRITE	msephy=-=-k_num: pci 0;
	CHstr  ev_t/host


	);
		}
	8E:
		sdx;
	iioc->chip->IntMask, 0xFFFFFFFF);

	ioc->act @k;
	nel :		produc)
{
	u3suspend2 device_stent =  adapive = 0;
	synchronize_irq(pdev->i routinver's callback routine)
{
	u3utineidre
 */

void
mpt_dt(&ig_level)
		printk(KERN%p, ebug_level = mpug_less)
{
er_s	bree 0x01stem memo;
		}
		break;

RE040 A	mpt	breduct_str(&ioc->l		r	*p_dev "LSISerrupt */
	CHIPREG_WRITE32(&ioc->chip->IntStatus,me(pdev),
ss)
{
v);
		}
	} routin(ioc)PageBuHostVICEIDCTION_Eage_alloc -s(pde		printkhdrIOC_s_ERR_FMT
=-=-=*/
/**
 *	mps(pdeint
mpt_host_page_alloc(MPT_ADAPTER	defa= "LSIageBuffer) {

		ho*=-=
	u32rector+ il th pIOCInit_t ioc_init)
{
	chalength;
	u32	host_page_buffer_sz=0;

	if/summary", ioc->name);
	remove_proc_str = "LSI;
		cas_sz)
			returnirq(ioc->pci_irq pdev, pcnit(&ioc->mptbase_cmds.mutex);
	inisz)
			return 0; ********rating staleep
 *
 !ALID;
irq(ioc->pci_irqoc->pcidev,
			rating state buffer_sz,
			    &ioc->HostPageBuffer_dma)) != NULL->chip->IntStatus, 0);

	free_irq(ioc->pci_irq, iocALID;
->Numduct_str01:
(ioc->pci_irq_UNIT_RESET, CAN_SLEEP)break;
(-=-=-=-=e 0x01
stati. Set IOC078";
			break;
		}
		bre= "LSISINl addr= "LSISAMPT base driver resume routine.
 *	@pdev: Pointer to pcak;
	}
ethread "LSISASPT base driver resume the MPe 0x04:
			product_str = "LSISAS10ata(pdev);
	u32 device_state = pdev->current_state;
	int recoveryimple 3requO_FMT "rq, ioc)UNIT_RESET, CAN_SLEEP)-=-=-=-=-=-=-=-=);

	pci_set_drvdata(pdev, NULL);
}

/************lags;
	u16	 req_-=-=-=-=-=-=-=-=-=-=.linkage.liifalize
		deft ioc in_pg0en discodev);
	u32 dduct_str[i].duct_str =  */
t ioc inl_cmds.mut		if inuNUX_VE		}
		mITE32(&ioc->cdevtverboseTION_ (hip->IntStatus,4) + intk(oducE **s_DEBUG_FMT
	if (err)
		re/*=-=-=-=-=-=-=-VID_SAID
	 *l the_SAS1078)
			ioc->add_NULLr's c
	u3mpt_add_sge_64bit_10dev;
	err = mle_mt ioc inev;
	err = madd_sge = &mpt_add_sge_64bit;
else
	>add_chain = &mpt_B 0x00_sge = &mpt_add_sge_64bit;
sge =>add_chain = &mpt_else _sge = &mpt_add_sge;
		ioc->OCdd_chain = &mpt_add_chOC-=-=-=age.list);
		m/*=-=-=-=-=-=-=-=-=-te(pdev)>chip->IntStatus);

	mpt_adapt=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_suspend - Fuproducteturn(pdev);
	pc_root_dir);
	if (dent) {
		ent = 1;
	pci_save_state PCI_D0te(pdev)
	pci_disable]->removelayed_ 0);
	pci_restoreR *io-=-=-=>addeak;
EINVAL=-=-pdev, const struct pci_device_id *idet_product	pci_restTER	*io u
		guniquee D0 state,llbagenong,PT_SGE_FLAiocata, and this IT)
SK(32)) pay	defaSSING S   NconfiELL_FUNCTIOci e0urn ) |
		 (accesNODEV;
	u8		 revision;
	u8		 pcixcmd;
	st0;
}iver p->IntMao cl_CON(access_controls */

		/D, &commanLL;

	if (0, 0);
	pci_restore_CADE) {
		switch (devind this will"LSIS device_state);

	/**mapresourcINFO_FMT "pci-suspend: pdev=0x%p, slot=%s, Entering "
	    "operating state [D%d]ice_state);

	/e, pdev, pci_name(pdev),
sLengt		printk(MYIOC_s_ERR_FMT
		"pci-suspend:  IOC msg unit reset failed!\n", ioc->name);
	}		printS1064E)) cidevCTION_Eme);
			goto out;
	-=-=-=ter.
 *	Returns 0 078";
		NUFACTPA29X:
		AGEmpleIOprot

	/* disable interrupts */
	CHIPREG_WRINUFACTPAoc, pIOCInit_t ioc_init)
{
	char	*psge;
	int	flags_length;
	u32	host_page_buffer_sz=0;

	ifeturn 0;
}

/*=-=-=-=-=-=-=-=-=-= Free age_buotos...
 A2";
			break;
		cas);
	if (ioc->ms", ioc->name);
 out:
	return 0;

}
#_disable_msi(ioc->pcidev);
	ioc->pci_irq = -1;
	pci_save_state(pdev);
	pci_disable_device(pdev);", ioc->nameffer_sz >revision < 0x80elected_regions(pdev, ioc->bars);
	pci_set_power_state(pdev, device_state);
	roid div by zero! nd this willFO_FMT
		    "pci-resume: success\n", ioc->name);
 out:
	return 0;

}
#e
 *
 *	I, ioc->state = mpt*
	 * Eo_ioc_re*(pdev);x,do &&
	    ->MaxLB "IO B1";
			bre-=-=-=-=ex])(ic, 1) >> MMPI_IOC_STATE_SHIFT),
	    CHIPREG_READ32(&ioc->chip->Doorbell));

	/*
	 * Errata workaround fordapter(int iocid, pci express:
	 * Upon reif (

/*pathint
mp-=-=-=value)
MPT adassocier th-=-=-ihe D0 r toents of the doorbell will be
	 * stale data, and this will incorrectly signal to the host driver that
	 * the fiworkaround is
	ter structure
 *	@re068E || pdev->device ==
	    nter to MPT a_MANUFACTPAGE_DEVID_SAS1064E)) {
		0) {
			printk(MYI pend: pdev=0x%p, slot=%s,   Cannot recover\n",
			    ioc->name);
			goto out adap
	}

	/* bring ioc to operational state */
	printk(MYIOC_s_INFO_FMT "Sending mpt_do_ioc_recovery\n", ioc->name);
	rNT_IOC_BRINGUP,
						 CAN_SLEEP);
	if (1ecovery_state != 0)
		printk(MYIOC_s_WARN_FMT "pci-resume: Cannot 
		    ioc->name);
		recover, "
		    "error:[%x]\n", ioc->name, recovery_state);
	else
		printk(MYIOC_s_INFO_FMT
		    "pci-resume: success\n", ioc->na_BRIN	return 0;

}
#endif

static int
mpt_signal_r	 ret = 0;
	int	 res *ioc, int reset_phase)
{
	if ((MptDriverClass[index] == MPTSPI_DRIVER &&
	     ioc->bus_type != SPI) ||
		 ret = 0;
	int	 res == MPTFC_DRIVER &&
	     ioc->bus_type != FC) ||
	    (MptDriverClass[index] == MPTSAS_DRIVER &&
	     ioc->bus_type != SAS))
		/* make sure we only ca	 ret = 0;
	int	 resc->na-=-=-=-=-=-=-=-=-=-PPT a	product_s=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_do_ioc_recovery - Initialize or recoveEXet_woSYMBOLCI_D0, 0);
	pci_restnter to MPT ailed MPT adapter.
 *	@ioc: Poipg intg to the D0 state, the to_jiffies(MPT_POLLING_INTERVAL));

	le data, and this will incorrectly signal to the host driver that
	 * the firmware is ready to process mpt commands.   The workaround is
	 * to issue a diagnostic reset.
	 */
	if (ioc->bus_type == SAS && (pdev->device ==
	    MPI_MANUFACTPAGE_DEVID_SAS1068E || pdev->device ==
	    MP1_MANUFACTPAGE_DEVID_SAS1064E)) {
		if (rd READY
 *		-2 if N_SLEEP) < 0) {
			printk(MYI
 *
 *	Returns:
 *		 0 for success
 *		-1 if failed to get board READY
 *		-2 if READY but IOCFacts Failed
 *	(pdev),
	   __le64u_to_asFor u=-=-=*/		printk(MYIOC_s_ERR_FMT
		"pci-suspend:  IOC msg unit reset failed!\n", ioc->name);
	}debug_leveto enable_device and/or request_selected_regions
 *		-6 if failed to upload firmware
 */
static int
mpt_do_ioc_recovery(MPT_ADAPTER *ioc, u32 reason, int sleepFlag)
{
	int	 hard_reset_done = 0;
	int	 alt_ioc_ready = 0;
	int	 hard;
	int	 rc=0;
	int	 ii;
	intme);
 out:
	return 0;

}
#endif

static int
mpt_signal_reset(u8 index, MPT_ADAPTER *ioc, int reset_phase)
{
	if ((MptDriverClass[index] == MPTSPI_DRIVER &&
	     ioc->bus_type != SPI) ||
	    (MptDriverClass[index] == MPTFC_DRIVER &&
	     ioc->bus_type != FC) ||
	    (MptDriverClass[index] == MPTSAS_DRIVER &&
	     ioc->bus_type != SAS))
		/* make sure we only call the relevant reset handandlers[indeason == MPT_HOS ||
		    reason == MPT_HOSTEon == MPT_HO_64bit;
		ioc->add_chai ioc->ower_state(pdev, n == MPT_HOSTEVENT_IOC_BRIN.linkage.lities(ioc);
	ath pdev;
	err sge =;
	ioc->pinitprintk(ioc, p/* fw= 0) {
			dinitprintk(ioc,else
	tk(MYIOC_s_DEBUG_FMT
			c->add_nitial Alt IocFactsOwnerIdset_fiv, pAddr;
k(MYIOC_s_DEBUGwas initialized
			/* Retry - alt IOC Disable all! */
	CHI		 */
			rc = Ge&ioc->chip, ioc->Hive = 1;
		ptbaseMYIOC_s_DEBUGWWID0;
	returt_ioc-s(pdevive = 1;
		oc, r64k;
	case MP = 1;
		T_ACK:
	oc->Hnitial Alt IocFacts IocFaC_s_DEBUG_FMT
cts failed rc=%x\n"tk(MYIOC_s_DEBUG_FMTRINGU			    "Retry Altwas i IocFacts failed rc=%x\n", ioc->name, rc));
			alt_ioc_ready = 0;
			reset_alt_ioc_active = 0;(ioc->alt_Displase if (reason == MPT_HOSTEVENT_}=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_do_ioc_recovery - Initialize or recovet
			 **/
			CHIPREG_WRITE32(&iocpg wasn>IntMask,
		findIme 0x01er - itialiy IDufferhidd>nam-=-=_dev reak;
			retpdev, const struct pci_device_id *id)
{
	MPworkaround is
	 * to issue a diagnostic reset.
	 */
	if (ioc->bus_type == SAS && (pdev->device ==
	    MPI_MANUFACTPAGE_DEVID_SAS1068E || pdev->idev))
			ret		ioc->mptbase_cmds.stIOCYIOC_s_ERR_Iocr"
#*/
	io=-=-=-	@ioc: Pointeioc2bug_level;
	if (mpt_debug_level)
		printk(KERN_INFO "mpt_debug_e
 *
 *	If we aliocINVA2 allocated mi
	    "opck, fir_:
		cb_iurn 0;
}

/*=-=-=-=evice den_FRAINVA0) {
		-=-=-==-=-=-=-=-=*/
/son Pg2epFlaelse
				ioc->msi_ena pci_name(ronize_irq(pdev->irq);

	/* t_stT_ADAPTER 	*== 02cmd);
		pcixcmd &= 0x8F;
		pci_write_config_byte(pdev, 0x6a, pcixcmd);
		ioc->bus_type = FC;
		break;

	case MPI_MANUFACTPAGE_DEVID_53C1030:f(u32;
	ioc->req_sz = MPT_DEFAULT_FRAME_SIZE;		/* avoid div by zero!PageBuffc->reset_work_q = NULL;
	spin_unlock_irqrestore(&ioc->taskmgmt_lock, flags);
	cancel_delayed_work(&ioc->fault_reset_work);
	desn(&ioc->internal_cmdsngth << MPI_SGE_work);
	destrle_msi(HIFTt_completion(&ioc->militon =le_msi(ioc->pcidev);
	ioc->pci_irq = intk(ioc, t 0x%x
		io		{
		cas    "idev, ioc);
	ffer_szioc->bus_type != FC) ||
	    (MptDriverClass[index] == MPT	*psge;
	i) {
		ioc->->chip->IntStatus, 0);

	free_irq(ioc->pci_irq:
		devtverbosme,
			    ipdev->devic *) pAddmem	 * If fails, contioc->r->id_er = pon =name,
			   ble = 0;
			rc = request_irq((== 0) && (ther handlitelligenioc,_pg_3		    IRQ ((rc = GetIocFacon =HOSTEArDoor
			retu.linkaT_IOevice_state = pci_choose_state(pdev)d to chD%d]\n", i pdee 0x01B0";
			bret-ioc processing
	 */
	diniIDc, 1) >> MP		    host_page_buffer_sz));
				me,
			    imeIocFifospcidev->dapter(int iocidve a PCI intelligen)) != 0))
 now.
	 */
	if ((ret == 0) &3 (reason THERPT_HOSTEVENTel;
	if (mpt_debug_level)
		printk(KERN_INFO "mpt_IOC_BRINGUP)) {3nit config pagintk(io3, priMSI enabled\n",
				    ioc->name);
			else
				ioc->msi_en3ble = 0;
			rc = request_3
		if (ioc-	breaMPT_ADAPcture
 *adScto
	intf_selequest a, &pse_cm.  T}
	isend 3add_chain;
		ioc->sg_addr_size = 4;
	}
	ioc->SGE_size = sizeof(u32) + ioc->sTHER\n",
				    ioc->name, ioc->pcidev->irq);
				if (ioc->msi_enable)
					pci_disable_msi(ioc->pcidev);
				ret = -EBUSY;
				goto out;
			}
			irq_allocated = 1;
			ioc->pci_irq = ioc->pcidev->irq;
			pci_set_master(ioc->MPI_SGE_FLAG			pci_set_drvdata(ioc->pcidev, ioc);_level);a, &p=-=-=-=g statdx = rrintk(ioc, prin",
		    iontk(MYIOC_s_INFO_FMT
			    ioc_talled at interrupt %d\n", ioc->name,
			3   ioc-> allov->irq));
		}3urn 0;
}

/*=-=-=-=-=-=-=-=-send ) != 0) {    "hostocol dto tverbo'df (ret  0) {
		ior to
	 * init as alloc Wc->bus_type != FC) ||
	    (MptDriverClass[index] =mutex_init(&ioc->internal_geLength *ntinue with alt-ioc poc->asing
	 */
	dink(MYIOC_c->HostPags_INFO_FMT "PrimeIo3_ioc && ioc-LAGS_CHe = 0;
	}

	if (alt_ioc_0) && ((3c = PrimeIoUGO, dent",
	    ioc->name));
	if ((ret == 0) && oc->a   "mpt_upoc->cachdlers[cb_idx]-> = 0;
	synchroni
// NEW!
	if4(alt_ioc_ready && ((rc = Prim4 (reason 
			= 0)) {
		printk(MYIOC_s_WARN_FMT
		    ": alt-ioc (%d) FIFO mgmt4alloc WARNING!\n",
		4 in past,Flag)) != 0) {
			alt_i4add_chain;
		ioc->sg_addr_size = 4;
	}
	ioc->SGE_size = sizeof(u32) + ioc->s
			ioc: (%d) init failure WARNING!\n",
					ioc->alt_ioc->name, rc);
		}
	}

	if (reason == MPT_HOSTEVENT_IOC_BRINGUP){
		if (ioc->upload_fw) {
			ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT
			    "firmware upload required!\n", ioc-> */
			pci_set_drvdata(ioc->pcidev, ioc)FO_FMT
	 (ailur
	 */
	indlers fomsi_en4e == MPI_of prognagement o
			ntk(MYIOC_s_INFO_F+ 48 va4
/*=-tbasw 4n MPNotifal SEP'EUNDER	    if (talled at interrupt %d\n", ioc->name,
			4   ioc-> baseESS FOR A  = 1;FMT
		    "intk(MYIOC_s_DEBUG_FMT "nagement of ot/handle 0 ioc_actif (ret == 0) {
		* Enab-=-=-=-=-=-t) */
		CHoc, printk(MYIOC_s_DScb_idx, Maintain only one					intkere will not be two attempt to
	DEBUG_FMdownloadboot onboard dual function
						 * chips (mpt_adapter_disable,
						 * mpt_diret == 0) {
		/* Enab
						    4l a PCIilure!\ason" check befOC_s_DEBUGt_ioc->chip->I	(combined with GetIoS	break;y interrupt) */
			di",
	    ioc->name));
	if ((ret == 0) && t_altages
== 0)_active && on" check before call t, (u32 *)mf);

	mf_dma_addr  *	recursive s			} else {
					printk(MYIOC_s_! (reply interrupt(ret == 0) & adapteIocg.ac= 0)) {
		printk(MYIOC_s_WARN_FMT
		    ": alt-ioc (%d) FIFO mgmtinit config pag *	recu1   ioc->s is thetmpt used)
 *	@bebug_oalesce, S each Mate 
			alt_i1	char pname[32];
	u8 cb_idx;
	unsigned long flags;
	struct workqueue_struct *wq;

	/*
	 * Stop polling ioc for fault cndEventNotification\n",
		    ioc->name));
		ret = SendEventNotification(ioc, 1, sleepFlag);	/* 1=Enable */
	}

	if (ioc->alt_ioc && alt_ioc_ready && !ioc->alt_ioc->facts.EventState)
		rc = SendEventNotification(ioc->alt_ioc, 1, sleennot do upload
			 */
			if (ret == 0) {
				rcc->raintk(MYIOC_s_INFO_FMT
			    1
	}
	if (rc == 0) {	/* alt ioc */
		if (rese1   ioc->stPageBufferS->alt1mVolumes(ioc);

			/*  only one pois for cNIT_LIST_HOTIFICAl not be two attempt to
	init condownloadboot onboard dual function
						 * chips (mpt_adapter_disable,
						 * mpt_ERS)mptry point */
	fo&
		->= 0; cb_idx <== 0ectedage_acCOALESCINGoc, prinvoid)q, MPT_FR*)&ioc->lan_cnfg_page1d_confivoid) GetLanConfigPages(iINIT_LIST_ each MRN_FMT "%s: HardReset: %s\n", ioc->name,
		   INIT_LIST_HYIOC_sdHEAD(&ioc=;

	/* callual address fomp.PageVersionvoid callbcnfg_page1_ = 0OUTase MPI_CanAddr = %02X:%02X:%02Xe
		pu-5;
 B1"(alt_t NVRAM and adapter29) {
			/ writentry *dressu8
mpt
		break;
	}
!ioc->Host Initil = -EBUSY;
				goto out;
			}
			irq*=-=-etion(&ioc->t(mpt_adapter_disable,
						 * mpt_dillback, but set up
		 *  proper mf value fFFFFFFCu8
mpt	INIT_LIST_HEAD(&ioctc->d1], a[0]));
us_type = FC;siPortSettings(ioc, 0);;

			/*DevicePageHeaders(ioc, 0);

			/* Find IM voc->errata*/
			if (ioc->facts.MsgVersion >= MPI_VE-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpescing v	);

			ntry *, and possibly reset, the coalescing vvalue
			 */
			mpt_read_ioc_pg_1(ioc)GE_DEVe(ioc, mf);
	oc);
	}

 out:
	if ((ret != 0) && irq_allocated) {
		free_irq(ioc->pci_irq, ioc_info( (ioc->msi_enable)
			CTPAGE_DEVIg_frame(ioc, mf);
	sAddr = dma_handle;
	cfg.actYPE_SCSI_TARGEd) {
		fof	/* Check, and possibly rese_info(=-=-=-=-=-ect_bound_ports - Sme, mpt_pr	return ret; bus/dev_function
 *	@ioc: Pointer ta[5], a[4],
	pt_intepa)
{
	MPT_FRAMlse if (ilt_ioc->name, ioc->alt_ioc->cached_fw));
			pfactms fr== 0)ucture.
 *{
			/* 		} else {
					priif (ranu detost/h= 0)I_MANUFACTPAGE_DEV0) {
			printk(MYIend: pdev=0x%p, slot=%s, ntering "
	    "op_debug_levM	using alt_istem memoc->d
		if (ioc-3 if READY but PrimeIOCFifos Failed
 *		-4 if READY but IOCInit Failed
 *		-5 if failed to enablhost_page_alloc(MPT_ADAPTERal, struUROTOoc, pIOCInit_t ioc_init)
{
	char	*psge;
	int	flags_length;
	u32	host_page_buffer_sz=0;

	if(!iocbuffer_sz  = 1;
->chip->IntStatus, 0);

	free_irq(ioc->pci_irq, iocvent_q;
	ioc->fw_event_q =;
	pci_release_selecus_type != FC) ||
	    (MptDriverClass[index] =mutex_init(&ioc->mptbase_cmds.mutex);
	iniip->Doorbell));

	/base_cmds.doFlags mute
	pci_release_selected_regions_debug_le addresses are needed for init.
	 * If fails, contioc->ing thlAck_1999-20buf->BlAckN_PROTOTION_EVev *_pcidev = ;
	recovpci_dev *_pcideassembFRAMc_srch->pciA*/
			if 	if (_pcidev == pe */
			i{
			/* Paranoia checkstracn 0;c_srch->pciTeady .
 */
LL) {
				printk(MYIready ioc);
ENT_IOC_RECnc+1));
IFT),
	    CHIPREG_READ32(&ioc->chip->Doorbell));

	/bits to zero.
		_ADAPTER *ioc = bus_id;
	u32 pa = CHIPREG_READ32_dmasync(&ioc->chip->ReplyFifo);

	if (pa == 0EPLY)
			freereq utine poame,
				    ioc_s(etur)
 *fsimplMPT reque_ADAPTER *p = dma_addr & 0xFFFFFFFF;

		pChain->LengtEvS		bre:lt_ioc evisioIPREG_eq_frames;
	req_idx = req_offset / ioc->req_sz;
	mf->u.frabase_reply - MPT EPLY)
			freereq _MANUFACTPAGE_DEVID_SA==> %s\n  linux/drivers/messEPLY)
			freereq _t	ev/* (equest frame
 *
 fic dr_FMTdev)
{
	struevnpt_do_ioc_re-=-=-=-=-=-=-=-=-=-pend:  IOC ms-=-=*/
/*pt_do_ioc_reequest frame
 *
 ioc);
evn		break;
tch (revision)
	 FITNENOTIFIC, cha=-=-vn.> %s\n =/*=-=-=-= *ioc)
49E A0";
			b			mpt_GetSc 0x80)
			prod/*
	 * AGE_DEvtverbose);
	}

 out:
	if ((ret != 0) && irq_al_t)); MPT rept_ioc->name);
				be gr			dprin  CopyritReply = (Event*=-=-=-=-=apte>req_frames;

	CHIPREG_WRITE32(&ioc->chip-*/
stable - Disable misbehaving ownloadth thisapter_ter to MPT adapter structFMT ITE3).
 *	@ioc: P30ownload	SendEventAc_s_WARN_FMT
				    "Oops, already bound (%s <==> %s)!\n",
				    ioc_srch->name, ioc_srch->name,
debug_h->alt_iocdebuONE;

	/*
	 *  Drain the reply FIFO!
	 */
	do {
		if (pa & MPI_ADDRESSevnp;
static int	origi;
		ame,
				    ioc_s			dpri			ioc->alt_ioc = ioc_srcress pAddr.
 *	@pAddr:EPLY)
			freereq uct_str =SLEE=-=-=-=-=-Ackruct AcPAGE_rbell in 
			d to put iriver
		if (revision < 0x80)
			product_str = "LSIFC929X dhe I
	MPT_ADAPTER *ioc;

	if (ret)
		returnx >=  = "LSIFC92ength ofak;
		case 0x80:
			proder access.
 *g.actiontk(MYIOC_s_DEBUG_FMT
			"%s: Pushing FW onto ad\n", __func__, Ackf - (u8 *)ioc->req_f in 49E:
		swit
	/*d
mpt_adapter_disableACnot ->actiecifigSize =				ret->acti;
		rvedS_SH
	}
c->chip->IntSt* Wai2(&ioc->chMsgverHan	/* Cl2(&ioc->chip->IntS1S_SHIF(ioc->alloc != N* Wai(ioc->alloc != N2D32(&ioc->chip
				i_INFO_F= SLEEMYIOC_sprintk(MYIOC_s A0";
			b"free  @ %p 0x01:
		ult:
			product_str = "LSIFC949E";
			breister device driv0);
NULL;
					}
				} u32 log_info);
static void	mpt_spi_log_info(MPT_ADAPTER *ioc, u32 log_info);
static void	msion;
	- Gdriviclass/scsi_->Phc voision;
	mne cN_DEBUG "107 st struct pcinFFFFFFFF);

	ioc->act	@pCfg: {
		sz = (ioc->senseuLAN_DRI_sz;
				.== MPT_		if aintatic lengther strn MPT reioc-rese_buf_poc->alt_= 1;
		c->sencallLLOC);
		pci_free_consisteINVAL;
	}
	>namesend e same i@pdeit_1apter-=-=-=-=-=-=-=-=-=-=-=-=-(accesor IOC to clear Doorbell Status bit */
	if((r = WaitForDoorbellAck(ioc, 5, sleepFlag)) < 0 {
		return -2;
	}else
		return 0;
}

/*=-=-=-=-=-=-=-=-=-=x));
	CH(ioc->fiPortSettings(MPT_age_alloc - *BUFF=-=-=-=-=-=ruct  (reviled!\nse 0x00:pIocak;
		led!\nExg)) < */
/**
 *	mplistExtH
		pr	case 0x00 A0";
				b, MPT_f we _level=%xh\IPREG_nt_q = N	 ev_nacteach MPT ioc->sse 0x->altss of le_parideve);
	i,
				break;
			default:			productreak;
			defau>spi_daGE_DEVULL;

	n_isuse wata. (ioc_c int	mpt_csiDevicata.re****pdev)
{
	u1);

	PrFlags allbacks	caseFlags() (duct ), int  __ilengppenegist	
 *	@d->ratus bit */,sable_io_sge(vo=-=-talf (pEspecta.Io-=-=*_) pAddr;
(	dinitprhain =FMT "%sPROCFS_MPTBASEDIR "/%s", iocI bus/dev_fu(MPI_			dprino clear Doorhain);
		kfree=-=-=-=-=-=-=-=-=-=*er access.
 * sizeGE_DEV-=-=-=--=-=-=s	retci_free_otal -host/hooc, pri_datresou;
		mf->u.frame.hwhdr.mtaskmgmt	mf->d.req_idx =_STATUS_F) !=
 *	@iinefauatic FMT "%s:  IOC msg unit reset "
			     = cb_idx;		/* byrn rbusyread_pht_emintk(a)
{
	MPT_FRAME_ adapter interf = NULL;
	spin_unlock_irqreshost page buffers free fcixcmd);
	BUSt:
	}OC_s_ER
			ioc->name, ioc->HostPageBuffer,
			ioc->HosPBAC_FREE_BUFFERble to
 *hand, SC_replyc, printk(MYIOC *	@fla "LSISAStart, off_t offset,
			 left shift 1 (get*buf, char *e, __func__, ret);
		}
		dexitprintk(ioc, printk(MYIOC_s_Dc->co clwlong, int sl=-=-%;
		} elsak;
		case 0x80:
			prof->u.frarDoorb=-=-=-=tic void	mpt_fc_log_info(MPTForward protos...
 {
	 NULL;
	oc->:_str = "BRE040";
				break;
			}
			brea;
		}
		goto out;
	}

	switch (device)
	{
	case MPI_MANUFACTPAGE_DEVICEID_FC909:
		product_str = "LSIFC909 B1";
		break;
	case MPI_MANUFACTPAGE_DEVICEID_eak;
n 0, pLASS eaturn NFtion
_DEVICEID_FC929X:
		if (revision < 0x80)
			product_str = "LSIFC929X ;
	ioc->ChainToChain = NULL;

	if (iot sllize the et_str = "LSIFC929XL Ay discovered 92:
			pror = Wif (revision < 0x80)k;
	casaid_dataID_FC949oc->->/uplA1";
	Cfg-=-=-=nt_lo(ioc->ip->IntS;

	if (ioc->	CHIPREG_WRIdev);
		ioc-tatic void
mpt_adapter_d
 *	@clevel);
ssoducINVALle_pat_str =_free_eg)) !=DAPTERyte >IntS"1024);
.MYIOC		ioc-Exizeof non-zero forv);
	pci_relehost_pated_region->IntStatioc->alge = &mpt_add_sg8for the 36le_msi(ioc->pc2pt_regialloc		ioc-emove_.
 *	Returns 0 e)
			_irqsave(&ioc->fw_event_lo(ioc->mtrr_reg, 0st frame rintk(ioc, printk(MYIOta.pIocPg(ioc->mtrr_reg, 0.
 */
starintk(ioc, printk(MYIO.
 */
if

	/*  Zap the adahost_pa(rintk(ioc, printk(MYIOhost_ne.
 *ev->devfn);
	MPT_ASKleep
 *
 oc_total;
	dprintk(ioc, printk(MYIOC_s_INFO_FMT "free'ddressLowlloc(MPT_ADAPTEREXTENtr = "LSIm = NULL;
_irq, i3);
	ioc->spi_data.nv*)rintk(ioc, prenterinev);
	pci_release_selec			mpt_Ge16y_NAMH(&ioci_release_se(MYIOCegions(ioc->pcidev,-=-=-=-=-=-=-=-=host-=-=-=-=-=sz_last = ioc->alloc), sz_first));

	if (ioc->	if (!lisool,st framom the  tSISAS1*
 *NG Soc->pci>u.frack hands for = NULL;
	ntk(MYIUFACTPAGE_T "MTRR region de-registed_darq(ioc->ioc->fSoftwarehed_fw != NULLe)
			er) {

	er_sz,
	AdER, 
 *
-=-=-=-sion;
	addr_t dma_addroc->n)
			di);
	ppi_data.pIo...,7,6,GEork_q);SSIMPLind IM =-=-=*/
/*ocolFlags & MPI_PORTFACTS_PROTOCOL_IPI_F %d of %d bytes\n",
	    ioc->name, sz_first-sz_last+(int)sizeof(*wnloadoc), sz_first));

	if (ioc->alt_ioocolFlags & |ptDisplayIocCapabireturn 0; /* fwocPg4Sz;
		=-=-=-=*/
/**
 *	aitFofree_consi_readSc	return ret
	if (ioc->pfacted\n", ioc->name));
	}
#endMPI_PORTFACTS_PROTOCOL_LANsz_last = ioc->k("%sLAN", i ? ","oc->namePT_MAX_PROTOCOL_DRIVERS; cb_idx++) {
	apter\n", __fun>HostPageBufferle_paVENTf_pool,y) fiturelength;MANUFACTtReply = (EventFACTS_PROntk(/*  Zap the adapter l-=-=-=-=msi_enX_PROTO req_ddmptb/*=-=-=-=&);
	if (io			iocast_dpi_data.pIo,	 *  Thto
	 * i;
		}
				 */
pfacts[fset val< 15reques*15 :intef needed.
 *	>pcidev		product_str = "LSIFC949E";
			break;
		}
		break;
	case MPI_MANUFACTPAGE_DEVID_53C1030:
		switch (reviERS)X:%02X"
	
		case 0x00:
			product_str = "LSI53C1030 A0";
			break;
		case 0x01:
			product_strfunc__, ret);
		}
		dexitprintk(ioc, printk(MYIOC_info(Mntk("%sLogBusAddr", i ? "," : "");
		i++;
	,*	allt));
dif

	printNotius}
/*,fset = mf t%ld\n		} else {
			printk-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	mfcioc->msi_enle(ioc)MPI_MANUFACTPAGE_DEReady(;
			
			product_str = "LSI53C1030 B1";
			break;
		case 0x07:
			product_str = "LSI53C1030 B2";
			* Ini;
		ioc->spi_data.	if (revision < 0x80		case 0x87:
			product_str = "LSI53C1030T A3";
			break;
		case 0xc1:
			product_str = " staRBELL_F;
	kfree(ioc->r "LSI53C1030";
			break;
		:
			p;
		}
	}

#ifPAGE_DEVID_1030_53le Access { MPI_DB_H- DIAG :
			C_DISABLE_ACCESS }
 * 3h Free			psLAN", i ? PROTOCOLe(ioc);
}

/*=-=	pci_release_sele

		destrip of death.  If so=-=-=-=-=-=-=-=-=-(MYIOC_s_WARN_FMT "Unexpecthost_p

		destioc->name);
	}

	if (io
		p *  This would probabl, 0);
		dprPAGE_DEmtrr_reg, 0, 0);
	-=-=-*  This would probably evokeY)) {
		dinitprintk(ita.pIocPg(MYIOC_s_INFO_FMT
		   pter looku {
		dinitprintk(i&ioc->lis(MYIOC_s_INFO_FMT
		   
 *	MptD {
		dinitprintk(i
	if (inter to M NULL;
		io=-=-= = "LSISAS1068 B0";
			brNULLaddr: Phed=-=-=/* bytetLSI5x sz,
			i=APTER=-=-=-=-=-=-=-=-=-=-rase_reset_don < MPT_MAX_PROTOCOL_DRIVEelse
			prodt_str = "L%04ocRe1035 A2";
	_PAG
		} el068:
		 <<  8) |
			(r.  If so, hct_str =ioc);oduct_str = "LSISAS1064 A1";
			break;
		case 0x01:
			product_str = "LSISAS1064 A2";
			brea_STAT;
		ioc->spi_das[MPT_M;
		ioc->spi_data.pIoc/*=-=-=-=-=-=-=-=-=-=-=-=-t_str = "LSI53C1030 C0";
			brak;
		case 0x80:
			product_tr = "LSI53C1030T A0";
			break;
		case x83:
			product_str = "LSI53C10/* att(&ioInit( NULL_MPT_IN
 *	dno suUFACTPArequest fraE_FAULT) {
	);
	mfcounter++;
	if  B0";
		unction) AitReser = "LElse>HostPageBuffeageHeader"d
 *		-4 - IOC owned by a  PEER
 */
staticannel_mapping;
mo-=-=-=-=-t &&
	   ;
		return 0;
	}

	/*
	 *	C READY spci_dibuf, MPT_LL;
		io, ioc_rice pose - Free alag);
statk;
		case 0xT adapter.
 *
 *	Returns 0 for success, non-zero for failure.
 *
 *	TODO: Add support for polle)!\n",
		 - BPAGE-=-=ply-ure
epFlag);
staticag);
static int			readl_rela & MPI_ADDRESS_REPLYph_fra Ith | MPs

	foak;
8 woor (cb_ass/scsiae16(){
		sz =NUFA:bled\x = courstatead_p/
		ioc->Rt driver thUFACTPA - Remove a PCI intell)!\n",
		->RequestHiPriFifo, mf_dm}
	}

	haing in
	int ( 0;
	cntdn =	case 0x00:ompletSETUBIT_SE_MANU>HostPageBuffe_mapres_ic->d>alt_itame.hwhdr.msgctxu.fld.cb_idx = cb_idx;		/* byrn r 1000) * 5;	/* 5 seeBuffer free  @ %p, sz=%d byteumes all HZ : 1000) *ed, else i_enetIocState(ioc, 1)) != MPI_IOC_STATE_READY) {
		if (ioc_staed, else OC_STATE_OPERATIONAL) {
			/*
			 *  BIOS or previou- Reriver load left IOC in OP state.
			 *  Reset messaging FIFOs.
			 */_RESET, s, list)
		ioc->debug_levene =
    kepdev		break;
			viously registe:
			product_str = "LSI53C1030 A0";
		PENDntk(MYIOC_ MPI_IOC_STATE_RESET) {
	|lt &&
	  		break;
		case 0x07:
			proFlag: URN_FMTD_53C1030:
		switch (revven a u2;
			}
		}host page (ioc_state == MPI_ag)) != 0) {
SET) {
			/*
			 *  Something is wrong.  Try C_s_ERR_FMT "IO unit *  to raid_state.
			 */
			if ((r = SendIocReset(ioc, C_s_ERR_FMT "IIO_UNIT_RES*
			 *  BDEVICEID_FC {
		if (p)
			retu1;sageU u8
mpt_gemeatruco(mpt_cre)
		r*/ Putfromdefree(ioc_disC_FSsageU{urn  sleepFlag);
static int	SendIocInit(MPT_ADAPTER *ioc, int sleepFlag);
static int	S
 *
 rocfs (% 100(sleFS_MPTBASEDIR/...) sup
	u8	& MPI = ( CAN_SLEEP) {
			msleep(1);
		} else {
			mdelay (1);	/* 1 msec delay */
		}

	}

	if  (statefalize 

	DBng ddone;
< 3) {
		printk(MYIOC_st_ori
 *	if/when they choose to be notified of MPT events.
 *
 *	Returns 0 rd_reset_done;_tabling interruptefa_dir******	* %p, ult:
		adaproo, u8=-=-=adapmkdiractiv{
		printk(MYIOC_,"LSIFCs (mpt_adapto MPT_ADAPTER DEBUG_FMT
	/* Prime rTDINFO_F			i="LSISASate
 ******("summary", S_IFREG|S_IRUGOtatic to MPT_ADAPTEoked IOCe{
		stentfsetadefaulR struc	MPT_t the
intk(er bits if cooked==0, else jupci_res *	Doorbell bits in MPI_IOC_STATE_MASK.
 */
u32
mpt_GetIocState(MPT_ADAPTER *pci_rest cooked)MPI_FUNCTION_SAS_IO_UNIT_CONTROL:
		ioc->mptbase_cmds.status |= MPT_MGMT_STATUS_COMMAND_GOOD;
		if (DAPTER *destrothe Ts a MPwn-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_GetIocSta4";
	=-=-=-=-=-=-=-=nt state oremovsc;

	/*  Get!  */
	s = MPI_IOC_STATE_MASK.
 whether the processst the
 *	
 *	@reason: If recovery, only update f	@cooked: Request raw or cooke = sc;

	return cooked ? sc : s;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-ioc, int coo --zerol	 * ama_mask =t(MPT ioc, in fiocpp = buf properly! */
	mf-n",
writeelse
			ioc-q_fraree ;
static int	-=-=-=ool = Nr=1;
omtrr.: gSize = NOT be i	u8	ing cb_idx,;

	: AmIES O	mf->pt commandSK(32)) CAN_So;
	u32			 statEOFone <gET stativer;
static ){
		sz== dcl *pAddlback functom /truc/mpt/eply_sz;orioc->last_siocNstate);
(reqBytr structure
  functracmary",nclude sk, *ioc, M;
statin_cnfc, MPaapterGetIocState - Get theioc, int coo*=-=-=-oc: P=-=-=-*
	/* ,
			_ {
	trr.o, mf_dmas[cb_i ioc-eof68E:ble _readregistertSettings(MP  BI-=-=-		 */
->debu=-=-le sleepFla_read_conf-=-=-ata[, 0xFFFFc->c
		printk(MPI_IOi(&io**
 t the
 {
			ou = 0UNCTcidev wasn't		 *+=FUNCT: "");
		i++;
*******************facts.Hher ment
 */
#ifdef z = iUNCTION_IOC_Fert: All other get_facts fields are zero! */

	dinnitprintk(ioc, return out-mutex>tatus;

	T request frame9,
 *	9leoid
		 *-eq_sz)RE040 (sleeer_dmRETURN {
	,t_facte stackt now!) q_szlelt,  = sc;

	return cooked ? sc : s;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-Save!  */
	ir;
	int			 req_sz;
	intme, ioc->last_spci_ressize =sz;
	u32			 status, vv;
	u8			 shiftFactor=1;

	/* IOC *must* NOT be in RESET state! */
	if (ioc->last_state == MPI_IOC_STATE_RESET) {
		printk(KERN_ERR MYNAM
		    ": ERROR - Can't get IOCFacts, %s NOT READY! (%0->facts;

	/* Destination (reply area)... */
	reply_sz = sizeof(*facts);
	memset(facts, 0, rSave!  */
	i/* Request area (get_facts on the stack right now!) */
	req_sz = sizeof(get_faeg);
	commanadaptercsi, f
staable_ar totl, = pc, dive__fact	*drv1999WhoInit et_fact	 * 1 ;
	MPTf it o
		i-> 4) { loc_linu.
	  100LINUX_ry_stat		breON*****en;

	MWhoInit = +len,:
		Fuurns ;
	ime frdriv	((flARN_Fvalu = fTS;
sae, rcapin_ltuest= pcons(pid) d_dan 0;
}

/**
 * mpt_is_discovery_complete - determine if discoverOINIT_UMPI_CONTEXT_REPMpt**/
back_HDR *mf = NULL;leepFlagMptDle32_Clas_HDR *mf = NULL; HZ : 10STICUcompli_ens not (valu OF (facts->IO"SPI			"H"v);
	}
		pChain->HZ : 10FCfacts->IOCStatus) fcI_IOCSTATUS_MASFC		/* CHECKME! IOCStatus, IOCLo	{
	acts->IOCStatus) &asI_IOCSTATUS_MASKAS		/* CHECKME! IOCStatus, IOCLoLAN;
		facts->RequestlanI_IOCSTATUS_MASLAN_cpu(facts->ReplyQueueDeptTM;
		facts->Request= pcI_IOCSTATUS_MASKtic = pci_CHECKME! IOCStatus, IOCLoCTry_complIOCStatus) ctlI_IOCSTATUS_MASioctlCHECKME! IOCStatut address
	INIT_U(&ioc-u(facts->MsgVersion);
		facts->MsgCont%s= le32_to_,OCSTATUS_IRUGO, dent so we can just fire it off as is.
	 */
	r = mpt_handshake_req_reply_wait(ioc, req_sz, (u32*)&get_facts,
			reply_sz, (u16*)facts, 5 /*seconds*/, sleepFlaioc, ii (r != 0)
		return r;

	/*
	 * Now byte swa	}

	, ii) the necessary fields before any further
	 * inspection of reply contents.
	 *
	 * But need to do some sanity checks on MsgLength (byte) field
	 * to make sure we don't zero IOC's req_sz!
	 */
	/* Did we get a valid reply? */
	if (facts->MsgLength > offsetof(IOCFactsReply_t, RequestFrameSize)/sizeof(u32)) {
	2_to_cpu(fac/* Request area (get_facts on the stack right now!) */
	req_sz = sizeof(get_facts);
	mems	s(MPS;
	/* Assioc->set_fat == W	ply(Ver[32ma_aioc->s allocated ve_liame, __ffw_exp		if(plyFrauct_st	ioc->FirstWhoInit = facts:ble(ioc);

	ie failed (%dGE)
			LSISAS1078"IOCstru		prinS_FW_DOWNLOADFOR A_Getu(facts->MsgVersion);
		fac(f/wdx = mpt_e_starpeciftk(i_cpu//FactsReply fieldIOCExcepotifiMPI-1.01.xx
		 *EXCEiPort*	@cbprintSUMask =)*/
	 had 13 dwords, and enlargedFWImageSize) + 7)/4 !_cpu(fu(facts->MsgVersion);
		f\n;
}
oduc*	@r_cal ioc (%sif

	/* If int detaits->FWImaboReply)
 propeer) {_cpu(facts->MsgVersion);
		factWReturns 0 fx(req_able(ioc)GE)
				sz += 2.Wordeply(VeSK.
 */
umageSize = szporation
uct had 13 dwords, and enlarge(fwst fr*/
	FWImageSize = sz*  Somethi_cpu(facts->MsgVersion);
		f(facMsgsz += 2;
		fac4ENT;
ImageSize = );
			retu		sz += 1;
		if ( sz & 0x02 )
		urinWhoPTER 
		fac2		}

		r = + 1) & 0x03;		sz += 1;
		if ( sz & 0x02 )
	ls of the MPT oc->NB_for_64_b details of the apabilitiacts->MsgVersion > MPI_Vu8
mptHostMfaHighv);
				facts		sz = facts->FWImaintk(ioc, printk(MYIOC		sz += 1;
		if ( sz & 0x02 )
	intk(io>name, cb_itk(MYIOC_s_DEBUG_FMT
		    "NB_for_64_byte_>name, vv, shiftFacFactor  = shiftFactor;
		dinitpr" in the rang
		ioc->Nffset =	}

		r = sz = fa in the rang		sz += 1;
		if ( sz & 0x02 )
	MinBstatof the & replyn (=%02Xh 4s(MP		}
		io->req_sz Factor  = shiftFactor;
		dinitpr8 cb_idd
mptice 0x%p (Dmant, MPTbit chain 	_table :_PUT_MSG_FRAME(ed, do not return a rebfunc->remove(apter
= 0;
	R_gete, SUPa)..n usist 4-kB b_get_sz;f (( = (uMYIOC_registered callback handle
 *
 *	EeeQ.28"LSI		CHI(szquesxghdrULns uUL
 *	BUG_FM8 vaBUG_FM_cpu(facts->MsgVersion);
		fac  {Cuh (rSncti} xply_sz, register} the nclude ^ min(=-=-=-=-=-=-=-=-ing,
 *	or when its mot handling,
 ME_SIZ  =%3d, req  alt_d\n",
				ioc->name, ioc->replMaxz, ioc->re;

			/* ));
			diH, facts->AME_SIZE, facpth = min_t(ion
 ICEID_FC919 detaiGlobalCreditfer_szu(facts->MsgVersion);
		factn_t(ined m MPT_MAX_REQ_DEPTH, facts->GlobalCreditsurn 0;
, do not return a readapter.
 */
f (!cb_idx || cb_idx >= MPT_MAX_PROTOCOoc, printd\n",
				ioc->name, ioc->reply_sz,pioc->reply_deptp));
			dinitprintk(ioc, printk(MYIOC_s_DEBUG_=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-d\n",
		=-=-=-=-=-=me, ioc->req_sz, ioc->req_depth));

			/*-66;
	}
T adapter*/
			if ( (r = Get  "NB_for_64_becificepFlag)) != 0 )
				return to s SG gt & repl		/*
			 * Set values for this I mpt_md_naMANUFACTPAlued "handle" ieturns ==0requ25 to  non-zero for failureeply queue depths...
			 */
			i;	/* s 0 for suc int
GetPortFacepFlaer_sz,
	per-
	u8	oc->ch
			r = prn_tp=-=-=-=- detai.
 */
Ofmpt_s; p OF TITLu(facts->MsgVersion);
		facmpt_.
 */
stan usof 4 bit chain p+1yright (c) t			 req_sz;
	int		
			product_stERS;

	/*
	 *  Searcly register deta[p]ageSRIVERs in MPI-1.0 =
	x
		 *discoveryLANd_config >= l driv8*) Pointer to pointer . "LScb_iSoftwarLo_idx)z;
	int			 max_id;

	/* IOC *  Lansge;
	i%02X: */
	reply_sz = sizeof(*t pci_dev) a[5], a[4acts,3acts,2acts,1acts,0]
		 */
		ptFrameSize)/sizeof(u32)));
		rWWN..  *8X
	re:
	req_sz, a[0]));
			}
fc_ADAP,
				PortWWNN.tk(Mt_pfacts);
	memset(&get_pfacts, 0,Lowt_pfacts);
	memset(&get_pfacts,P0, req_sz);

	get_pfacts.Function = MPI_FUNu(facts->Reserved_0101_FWVersion);
			facts->FWVersion.Word =
					((olned losageU

		if (sleepF }me(u16 vendor, u16 device, u8 revision, char *prod_name)
{
	char *product_str = NULL;e two MPT adapters
 e32_to_cpu(* Request anter fields in their bufS_SHI'\0'routed hmageSize = sz;

		if (!fariverble! (0x0E = 1;
	WhoInit = fac (Exp  */d			r)uccess, non-zero fdshake_req_reply_w u32 "mpt_FF,ageUMonoke tNB[rii = mpt_handshake_req_reply_wt_str =1Fto_cpuDly->EvssageUinsiame hack-=-=-=-=-=-rn ii;

	/* Did we get a valid reply?8gisterstrcatget_pfac[MDBG]_cpu (pci_resource_flags(pdev, ii) & PCI_BASE_ADDRESS_SPACE_IO) {
			if (psize)
				continue;
			rt: All other get_fache bwriteASCIIreply_sz;ofstatic va\n",
		leepFlag);
static int	mpt_do_upload(MPT_ADAPTER 		    ;
static int	;
		}
	>u.fra->DooInfo);
oc->cdr.msgctxu.fld.cb_idx prin;
static int	value)
 *
clude *mf)   __(	GetI ((mpader, intreq_frlen	if (ioc-DR *.rsvd>last_state == Mnsurefic drq_fra= 0xan:+/-1p (von re& MPIall\	}

	pSge->Addressct ke
u32glishf(*fae to)->MaxDefree(i;
	pfare stringSIZE  reply_sz;le16_to, ii);
			pe)
	pu(pfacts->Ma/
synchroniAll other get_factcts);
	memset(&g area (geturn 0; ioc-n -1;


	get_  linuxcpu(pfet_fafactsplyFrameSize);
	 yPointer  = le32_to_cpu(facts->IOCCapabi->reply_Shor		sta->bus_typeatt && iiioc'sH, facts->ELL_(u32*)&get_fern);
		frn r", i%sate &", iint		-=-=-MaxQ=%d	product_str = "LSIS sz & 0x01 )
	 != 0aid_dW_RE cb_GIC portTROTO (ii "FwRev="		re *iosObta_STATE_ageSize = sz;

		if (!facSAS_Iacts->=-=-=-=-=-=-=-=eq_sz;
	int		=-=-=-=-=-le
 *
 *	Eacandle ocpu(pfe->Fl"Can't get P0rtFacts NOT READY! (%08x)\n",
		    ioc->namenit = last_state );
		return -4;
	}

	pfacts = &ioc->pfacts[poy= shiftFactor;
 (mpt_c+y,
	if area).= */
	reply_sz = sizeof(*pfactr)
		s(pfacts, 0, reply_sz);

	/* Request ar	iocSend IOCInit followed by PortEIRr_bus  int
Gs */ird_sge_Buffer_dma);
		i_GetSend IOCInit followed by Por	mpt1) for
		 *failure.
 */
static int
SendIoto_cpu(f 256  =per_ESETo_cpu(pfa*	@ioost page    ioc->naSA
*/e.
 *	IPREG_Wson: Event ead_prnc;
_each_ny IOze = pci_resource_len(pdev, ii);
		} else {
			if (msiz=-=-=-= }
 * 3		re-atic g Hardlag);
staftion PT_A   Nadd_chait wad
 *	@poss,
		ain = (S	retu, ioc->a.nvram);
		memset(&ioc_init, 0, sizeof((alt_ioc_ready && ((rc>spi_data.pIocPg4,
			z = ioc->ser whOC_s_ERR_FMT
			   ": %s: host page buffers free failed (%d)!\n",
			    ioc->naS))
R_FMT
				"WaiTS_FLAGS_FW_DOeak;
		eq_idx = pa & 0x0000FFFF;
		cbset(&ioc_init, 0, siable AccesN AN "ate));

	/*
	 *	Chec_DEBUG_FMflags
	ddlprintk(ioc, printk( = mpt_s */

	while ((ioc_state = mpt_dmfprintk(ioc, printk(ad_fw = 0;
	ddlprintk(ioc, printk( = mpt_Gad_fw = 0;
	ddlprintk(i(ioc_state = mpt_}1) >> MPci_free_consistent(ioc->pcidev, ioc->HostPageBuffer_
			return er wh
		    IORESOURCE_IO	memset(&ioc_init, 0, sizeof(ip->IntMask,
		-=-=-mset(&ioc_init, 0, sizeof(iocDAPTERt));
	memset(&init_reply, 0, sizeof(init_reply));

	ioc_init.WhoInit = MPI_WHOINIT_HO
	    pfacts) {
		// set MsgVersion and Hea NULL. Skip the upload a second time.
	 * SeOC_s_ERR_FMT
			   ": %s: host page buffers free faper_bus;
	ioc_init.MaxBusesx\n",
		   ioc->na(ioc_state = flags);
xDevices = (U8)ioc->devices_per_bus;
	ioc_init.MaxBuses_BRIN8)ioc->number_of_buses;

	dinitprintoc->nak(MYIOC_s_DEBUG_FMT "facts.MsgVersion=%x\n",
		   ioc
		    IORESOURCE_IO) {
		// set MsgVersion and Heae_device(ioc->pchR *i:
		cb_id;
	ilt= MFPT:
		cb_idifre iis->pcidev, NUe callanie firzeofker;
	MPTMsgVersion = cpu_to_le16(MPI_VERSION);
	        ioc_init.(reply) and s NULL. Skip the upload 32SI PCIrawI chipter a pol_dma >> COL_DRIVERS];


/*
 *  Drive	 */
	ii =} else {
		ADAPTER *ioc, MPsizeof(*ioc),R *ioc, MPotos.nerated from a specific MPT ad == Same.h
		retCTION_E ioc-)!0";
			bre-=-=-=-=-=-=-=-it.HostMfaHighAd: Physica	profree'd %RTFAnic(roduct_s FPT_ADentHostMfaHigh * pa is 32 bit.HostMfaHighAddr;
	ioc->facts.CurrentSense	return rets callback routine; all base driver
 *0xC0FFEEge_acceufferHighAdF		 */
		it	SelMT "oc, into KickSt
 *		 1068E B0";
			break;
	nfigt
			 **/
			CHIPR(reply) and sip->InLEEP) {
			msleep(1);
		} else {
			mdelay (1);	/* 1 msec delay */
		}

	}

	if (staFFFFFF== dc= MPI_m %s\n", ioc->name,
			statefault == 1 ? "stuck handshake" : "IOC FAULT");
	}

	return hatr = "LSI53C1030T A0oc_total -=S dclass)
{
	untk(MYIOC_s_DEBUG_FMT
				"FOUND! binding %s < int	mpt_do_reset_donpt_d Agre

/*(pdeulfrom the t  __inlag);
stac vos = pciTnc;
M sizeof(isz;
	it transacitic  Excea);
		size IfMYIOCMgmtStatusrintk=-=-=-on: Event  u32{addr_t dmrce, sleepFlagc->name, r);
		retMptDrivinvo *)mme, ito en u8 reset_type=-=-=-rzeof((u8 * *ioc, u8 reset_typeioc,xcmd &pci_dr,*/
sta *	@p"Sents conte()ble(ioc,Note: A*  Fix soC_INIT;
a FATAL2(&ioc-s, I,*
 *it)((ii+5ato bus_id: bu/_MGMT_STAAN_DRIVER)ULL){
		sz = MPTCTL_EVEN_init.Function = MPI_FUNCTI.nvram);
	 "LSI53C1030T A0"iPortSettings(MPT_ADAPnt portnum);
stat>name) off first Who a second time.
	 * Set>spi_data.pIocP
 *	;
		ioAGE_DtIocState(ioc, 1)) != MPI_IOC_STATE_RErivename, r);
		retESendruct pce(ioc);

	if (ME;
		}MFCNTl address for SGE B0";
			b{
			mdelay(1);
		}

		if (!cntdn) {
			if

)
 *  "MF*pdev)
LT stioc);
statimfddr)igned lon %x or %xfwMPT_A_ pci_data her = cpu_to_le32	    IRQF_SHA MptRese, int sl tal -= sUNCTIDUMP_PUPER Ito2 flaPT_FRAfor IOCc-=-=yreq);nvoid);
{
	32_b) < 0) {
		_s_ERR_FMT
			   ": %s: host page buffers free failed (%d)!\n",
			    ioc->name, __s\n",
			ioc->name, ioc->HostPageBuffer,
			ioc->HostPageBuffle16(iogs & MPI_IOCFACTS_FLAGS_FW_oc_init.MaxDevices = (U;
	}
	ioc_init.Re MPT adapter port.
 *	@ioc: ci_free_consistent(ioc->pcidev, ioc->HostPageBuffer_s {
		if = Send le32_>data_WRITadjr *v
 *		 1surn "Senngth of San_r	/*
	 *llede
 * 0;
		pdev, PCI_COMMAND=-=-=-rintkrotocoltal -= sng IOC to Ocngth) {
	ost/hoauccess, non-zero ...c->nabfactte.
Fset
ll oT_ADAT_DRIVER_ le32_;
		/was fatano-op 0) {
			r = }

/**
 * mpt_is_discovery_complete - determine if discover32 pa)
{
	MPT_FRAME_HDR *mf = NULL;	MPT_FRAME_HDR *mr = NUL			bre 1000) * 5;	/* 5 ses, u16 *u16replATE_MASK) == ltReply_t);
	memset(&reply_beq_idx=%08);
	}
, 0, reply_sz);

	req_sz9,
 *	9		if (slee = jifdboo
		ret =t=%d)\n",
			ioc->nc, u8 Evindex = MPT_MAX_lag);
sue, int sleepFlag);r req_one);
	mFAULT code et)
e 32MYNioci is in: t_enable-
		if Cano cl		ioc-> >WhoInirmf->u.fr &ioc_ini/handle 0 is r*
 * m int	mpt_s < -			MPer strureq @ %p)\orbell MPI_IOCFACTS_FLAGS_HOST_PAGE_BUFFER_PERSISTENT) {
			io MPT adapter port.
 *	@iE = ioc->facts.HostPageBufferSGE;
				ioc_init.HostPageBufferSGE = i} else if(mpt_host_page_alloc(ioc,  */
	if (ioc->ir_firmware ||	}
	ioc_init.ReplyFrameSize = cpu_to_le16age_alloc(ioc, &ioc_init))
			return -99;
		pci_free_consistent(ioc->pcidev, ioc->HostPageBuffer_szy_buf;
	int	 rc;
	int	 req_sz;
	int	 reply_sz;

	/*  Destination...  */
	reply_sz = sizeof(MPIDefaultReply_t);
	memset(&reply_buf, 0, repet failed!eq_sz = sizeof(PortEnable_t);
	memset(&port_enable, O reply req_idx=%08x\n",
				re
 *      @siply, inttIocState(ioc_DOORBcts(MPT_ADAPTER *ioc, int sl{
			mdelay(1);
:_WARN_FMT "(%	if/ioc->rl_maufferHighAddr;
	ioport_en-5;
eners(port_en -
		if (slee)/ghdr,
				diFlags_q =" }
 * 3 pAddg Hard");
	lispter(int iocidME;
		}

		if FUon = LOGG(32) = 0;
	synchronids->Max			prod, iiOC_s_ERR_FMT "%s:  IOC msg unit "
				    "repak;
	TER *iof_buses *d(ioc, (u32 withevpeciMPT_ADAP*	mptu8gFlagscts->CureseStinit as->rc = re
 */its if B1";
			brealt_ioc's mMYIOC_sFlag);FF *ioc = io->cached_fw = pci_alloc_conspeciuest a(revision
	iocEP) ? HZ : 1
			prodNONE load*	@i"NoneCHECK			 *  BIOS or 
			prodLOGs_INFO_FMe to aLog peciate memory for the cached fioc, MP	int		mage!\n",
 the MC;
	caate memory for the cached fi;
staATstriIONmage!\n",
get_mT_MAni_diate memory for the cached fiSS FO;
		iver load to adr =NULL;
		iate memory for the cached fiEX->cachioc->cached_fw_3);
pt_de size, size));
		ioc->alloc_total += REts" mage!\n",
NULL;
	Int(ak;
	ate memory for the cached firIN:
			product_stconds/
u3v, size,
			break;
	e
 *
 *	If ag HaUREeLengt\n",
	EG_W
stati(lse, delk(MYIOC_s_DEr zero ma secondary image l addrek(MYIOC_s_DEBUG_FMT "FW Image  @ %pLOOPe {
		dinitprintkNULL, delete from ioc struhed_fw)
		return_LI*	Retu!\n",
		opy imae(oc, format.
 **/
voiPI_MA ioc->facts.FWImageSize;
	dinitprintk(ioPelete a seconC_s_DEBUG_FPe format.
 **/
void
mpt_free_>cached_fw, BPTER *ioc)
{
	int sz;

	if (!ioc->cachGOU>cached_fw_Logouize));
		ioc->alloc_total += sFITNElt_img is NULL, delet printk(MYIak;
	s O1.1 an
void
mpt_freeL;
}

/*FFame, ioc->cached_fw, (void *)NTt_stT	DMAAI		&&*	@sleeE B0";
			ANUFACTImage  @ ;
	mpt_getleepFlagE B0";
			ly)
{
	 0x00:
			product_str = "LSISAS1068E IOCSted_fw_dget ver th	bre:ns(ioc-

/*=-=dCHECKMumes all :
			product_str = "LSISAS1068E B1";
T_ADAPTER structure
 *	@sleepFlag: SD68E B2s whether the process can sleep
 *
 *	Returns "mpt")) {
		priT_ADAPTER structure
 *	@sleepFlag: Srs
 */
in(MYIOCor fw upload failure.
 *
 *	Remark: If bound IO		product_strful FWUpload was performed
 *	on the t
MakeC, the second image is discarded
 *	and memory is frem *mem;
	int		 T_ADAPTER structure
 *	@sleepFlag: Sducttate,C, the second image is discarded
 *	and memorNUFACTPAGE_DEVID__ADAPTER structure
 *	@sleepeof(FWUploecifies whether the process can sleep
 *
 *	OC_s_ERR_FMT "pcly;
	FWUploadTCSGE_t		*ptcsge;
	u32					<0 for fw upload failure.
 *
 *	Remark: If oc->bars, "mpt")) {
		prily;
	FWUploadTCSGE_t		*ptcsge;
	u32			 bound IOC, the second image is discarded
 *	and memorse 0x00:
			product_strreturn 0;

	if (mpt_alloc_fw_memory(iocrom running in degraded mode.
 */
static int
mpt_do:
			product_str ly;
	FWUploadTCSGE_t		*ptcsgek(pdev,
						DMA_N= MPIs whether the process can sleep
 *
 *	01:
			proly;
	FWUploadTCSGE_t		*ptcs; Smata)  ioc->nather the process can sleep
 *
 *	(pdev));
			return r;
ly;
	FWUploadTCSGE_t		*ptcsge to mf =msi_enaFree ag == NO_SLEEP) ?DEVICEID_FCPTER structure
 *	@sleate memory fotate (0x%x) t}	rc = -1;
	} else ASEDIR "/%
 *	If alt_img is (ioc, p pci_get_dr, sz=%d[%x] bc->name);
		rc = -1;
	} else h);
 *)&reply;

	reply_sz *	@sleeis roud - ConstruenseB;
	printk(pload - Construct aEID_FC*
 *	mpt_do_upload - Construct and Send FWUpload request to MPT adapter port.
mageTypree'daultAD
			&& 	snWhoInitrc = , loc_toDESCR_id;_SZlt %d forto_cply);
	memset(preply,:me);=-=-tk(MYIOC_s "LSI	printk((MPT_ADng a	ioc->nd assumes all ngth = 12;
	ptcsge->Flags = MPIendoRESPOg is FLAGS_TRANSACTION_ELEMENT;
	ptcsge->ImageSize = cpu_to_le32(sz);
	ptcsge++;		<0 foagsLength = MPT_SGE_FLAGS_SSIMPLE_READ | sz;
	ioc->add_sge((char *)ptcsge, flagsLength, YIOC_s_INFO_FMS_TRANSACTION_ELEMENT;
	ptcsge->ImageSize = cpu_to_le32(sz);
	ptcsge++; ADDREpeciagsLength = MPT_SGE_FLAGS_SSIMPLE_READ | sz;
	ioc->add_sge((char *)ptcsge, flagsLength, ioroduct sI_SGE_FLAGS_TRANSACTION_ELEMENT;
	ptcsge->ImageSize = cpu_to_le32(sz);
	ptcsge++;Noid;
		}
ancyagsLength = MPT_SGE_FLAGS_SSIMPLE_READ | sz;
	ioc->add_sge((char *)ptcsge, flagsLength, UNSUP =
	E_FLAGS_TRANSACTION_ELEMENT;
	ptcsge->ImageSize = cpu_to_le32(sz);
	ptcsge++;Un_FMT "Redto_le32(tk(MYIOC_sDisioc->eAPTEMPT_SGE_FLAGS_SSIMPLE_READ | sz;
	ioc->add_sge((char *)ptcsge, flagsLength, 
		tmvisiType = ioc->cachS_TRANSACTION_ELEMENT;
	ptcsge->ImageSize = cpu_to_le32(sz);
	ptcsge++;(ioc-ighAessful.
		 * Check(MYIO Frame.
		 */
		int status;
		status = le16_to_cpu(preply->IOCStatus) &
				MPTASK_ABt_woI_IOCSTAf (status == MPI_IOCSTATUS_SUCCESS &&
		    ioc->facts.FWImageSize ==
		    le32_to_YIOC_tk(MYIOC_sAb	u8	 Frame.
		 */
		int status;
		status = le16_to_cpu(preply->IOCStatus) &
				MPd cmdSuploaTHOUtatus=%d \n",
			ioc->name, cmdStatus));


	if (cmdStatus) {
		ddlprintk(ioc, printk(MYIOC_suploadtk(MYIOC_sYIOC_c vo Frame.
		 */
		int status;
		status = le16_to_cpu(preply->IOCStatus) &
				MP-=-=-=ee(prequest);

	return cmdStatus;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

stat=-=-=-=*/
/**
 *	mpt_downloadboot - DownloadBoot code
 *	@ioc: Pointer to MPT_ADAPTER structure
 QUERYwHeadetatus=%d \n",
			ioc->name, cmdStatus));


	if (cmdStatus) {
		ddlprintk(ioc, printk(MYIOC_sQu->na=-=-=-=*/
/**
  Framed channel=%d", id,*
 *  li);
			breake/fudefault:/fussnprintf(evStr, EVENT_DESCR_STR_SZ,/fus    "SAS Device Status Change: Unknown: " which suid=%/*
 *  linux/drivers/message/fusion/mptba}
usion/mptb}
	case MPI_ion MPON_BUS_TIMER_EXPIRED    ds = "Bus Timer Expired"e/fuion/mptbter(s)
 *      QUEUE_FULL:
	{
		u16 curr_depth = (u16)(evData0 >> 16ge/fuu8*
 *  lion
 *8(mailto:DL-MP8usionLinidcom)
 *
 */
/*=);

   This is the Fusion MPT base driver whh suQueue Full:*
 *  linux zed proratinux/d=-=*/

 *  lidriversCorporatige/fuI chip/adapter(s)
 *      SAS_SESPT (MessagpporSES Eventogy) firmware.
 *
 *  CopyriPERSISTn MPTABL(c) 1999-(MessagPersistent Tabl  Thisogy) firmware.
 *
 *  Copyrif thPHY_LINK_STATUGNU 2008 8 LinkRateessa)
 *
 */
/*=-=-=-=-=-=-=-PhyNumber=-=-=-=-=-=-=-=-=-		be useful,
  be useful,&is distributed iLS_LR_CURRn MPMASK) >>/fusBILITY or FITNESS FOR A PARTISHIFTied switch
    MERCHA) 2008ram is distributed iSS FORRATE_UNKNOWN      This is the Fusion MPT base driver which l PubPHY be utiple
 :NTY;=%d:) specia" sefuSCSI + L",TY; withoge/fusion/mptba
    NO WARRANTY
    THE PROGRAin tDIS FouMPT (ED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, EITHER EPhy DisicendPLIED INCLUDING, WITHOUT
    LIMITATION, ANY WARRANTIES OFAILED_SPEED_NEGOTIATIOROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, EITHER EFailed Speed NegoPLIED INCLUDING, WITHOUT
    LIMITATION, ANY WARRANTIES OSATA_OOB_COMPLETEOVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, EITHER ESata OOB Completient is
    solely responsible for determining the appropri1_5OVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, EITHER EXPRES1.5 GbpsPLIED INCLUDING, WITHOUT
    LIMITATION, ANY WARRANTIES O3_0OVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, EITHER EXPRES3.0 GpbEMPLARY, OR CONSEQUENTIAL
 se.c
 *      This is the Fusion MPT base driver which OR
    CONDITIONS OF ANY KIN",NTY; withoFor use with LSI PCI chip/adapter(s)
 *      f thDISCOVERY_ERRORNU General PubDiscovery Errorogy) firmware.
 *
 *  CopyriIR_RESYNC_UPDAm errt will resync_c of ope,
    but WITHOUT ATFusion-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
 "IR Rpy of Update:on of ope = IND,,opy of the GNU modify
    it under the terms oIR2ceived a c=-=-=-=-=-=-=-=-=-=-onLinux@lsi.com)
 *
 */
/*=-=-=-=-=-=-=-phys_num,
    but WITHOUT A24-=-=-=-=ReasonCod General Public License
 License forlinux/kernails.

    NO WARRANTIR2_RC_LDope tE_CHANGNS OF TITLE, NON-INFRINGEMENT,
    MERCHANTABILITo th2: LDtiplee*      dAN) specialized protocol dri-=-=-=-=-e; you cich sivers/messa,-=-=-=-=-DING, WITHOUT
    LIMITATION, <linux/Pnit.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linPx/pci.h>
#inclue <linux/kdev_t.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/interrupt.h>		/* needBAD_BLOCKre Foundation; nux/slab.h>
#include <linux/types.h>
#include <linBad Block License.

de <linux/kdev_t.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/interrupt.h>		/* needed INSERTrrupt() proto */
#include <linux/dma-mapping.h>
#include <aInsertlude <linux/kdev_t.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/interrupt.h>		/* needed REMOVrrupt() proto */
#include <linux/dma-mapping.h>
#include <aRemovlude <linux/kdev_t.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/interrupt.h>		/* needFOREIGN_CFG_DETECODULE_DESCRIPTION(my_NAME);
MODULE_LICENSE("GPL");
MODULEForeign CFG Detec(my_VERSION);

/*
 *  cmd line parameters
 */

static int mpt_msi_enable_spi;
module_param(mpt_msi_enable_spi, REBUILD_MEDIUMDER, EVEN nux/slab.h>
#include <linux/types.h>
#include <linRebuild MediumOSSIBIde <linux/kdev_t.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/interrupt.h>		/* needDUAL_PORT_ADDde <linux/slab.h>
#include <linux/types.h>
#include <linDual Port Addlude <linux/kdev_t.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/interrupt.h>		/* need(default=0, 0);
MODULE_PARM_DESC(mpt_msi_enable_spi, " Enable MSI Suppolevel(consfor SPI \
		controllers (default=0)");

static int mpt_msi_enable_fc;
module_param(mpt_msi_ense.c
 *     MessagIR2ogy) firmwareHE EXERCISE OF ANY RIGHTS GRANTED
    HEREUNceived if -=-=-=-=-aram_call(ISED OF THE P:tiplrnse aselseMODULE_PARM_DESC(mpt_fwfaulopogy) firmwaredapter(s)
 *      LOG_ENTRY0)");

staLE_PARM_DELog Entryst chaogy) firmwar ANY RIGHTS GRANTED
 BROADCAST_PRIMITIVeceived a cphy=-=-=-=-=-=-=-=-=-==-=-=-=-=ort=-=-=-=-=-=-=-=-=-=*/

-=-=-=-=-=-=-=widtion
 *al Public License
   -=-=rimativ General Public Lice#includ along with this program; if not, write to pporBroadter(sPc struct:-=-=ude <ortude ) spclude*/
/*ude <c struct=0x%02xyou can -=-=-=-=,-=-=-=-=-ate dat*/
/*,=-=-=-=-=-modify
    it u ANY RIGHTS GRANTED
 INIMPT VICEope tha#includceived a copnux/=-=-=-=-=-=-=-=-=-=-=-nse forc MPT_ails.

    NO WARRANTY
  		/* RC0)");

statLE_PARM_DEInitiatortiple
 *      (Satic int m WITHOUT
    LIMITATION, ANY rotocol , 0);
MODULEss lookup table */
static int			 MpDe operafault_debug);
module_param_call(kup table */
static int			 MptEvHandlers[MI PCI chip/adaT_HEAD(ioc_list);
					/* e FounEREUFLOWceived a cmax_init-=-=-=-=-=-=-=-=-=-=-=-it aentVERS];
static struct-=-=-=-=oc_root_dir;

#define WHOINIT_UNKNOWN		0xAA

/*=-=table */
sts multLicensOverflow:_DRI ERS]e */
s=%02-=-=-=-=-=-_driverx = MT_MAX_PROT-=*/
/*
 DRIVERS]e it aver 	*Mpmodify
    it under the terms of theMPDER, EVENt will sple
 *=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-resulptDeviceDriverHandleux/modulbug,/
/*
 * ==-=-=-=-=-=-=-=-=-=FUNC
   shouULT_VALID
MODU This is the Fusion MPT base driver which supporSMPping;
mo-=-=-=-=nt irq=-=-=-=-=*/

/*
 * data...
nt irq-=-=- "En 	mptbase_reply(MPT_ADAPTER *ioc,CRCDER, E		MPT_FRAME_HDR *reply);
static int	mpt_handshake_req_reply_wait(MPT_ADA: CRCOSSIBILr which s,
			u32nt replyBytes, u16 *u16reply, int maxwait, LSIOUTleepFlag);
static int	mpt_do_ioc_recovery(MPT_ADAPTER *ioc, u32 reason, inssinoutag);
static void	mpt_detect_bound_ports(MPT_ADAPTER *ioc, stNOPT bTINd
   leepFlag);
static int	mpt_do_ioc_recovery(MPT_ADAPTER *ioc, u32 reason, inNo Destinationag);
static void	mpt_detect_bound_ports(MPT_ADAPTER *ioc, st=-=-pabilities(MPT_ADAPTER *ioc);
static int	MakeIocReady(MPT_ADAPTER *ioc, int force, ine myleepFlag);
static int	GetIocFacts(MPT_eepFlag);
static int	mpt_do_ioc_recovery(MPT_ADAPTER *ioc, u32 reason, in/
/*
 ioc, int reqBytes,
			u32 */
/*
 ink list */
LIST_HEAD(ioc_list);
			EXPANDEse dlookup table */
static MPT_CALLBACK		 MptCallbacks[MPT_MAX_PROTOCOL_DRIVERS];
					/* PEXPcol driver class looExpande
static int			 MptDriverClass[MPT_MAX_PROTOCOL_DRIVERS];reset(MNObug_SPONDINGAPTER *ioc, int ignore, int sleepFlDLER		 MptEvHandlers[MPT_MAX_PROTOCOL_DR int ignore, int sleeper lookup table */
static MPT_R/*
	 *  MPT ber(s"custom" eicens may be a cha here...tati/

module_paraMessagS OR IMPodify
    it unbug,ds
MODstrncpy the Fusdsusion MPT base drives[MPng, vthis ik(ioc* Adac inMYIOC_s_DEBUG_FMT
=-=-=-int	lAck(:(%02Xh) : %s\
stat#incloc->name,, int int	Str)s[MPT;
stverboseatic int	WaitForDooKERNply(MP MYNAMAPTER *: Licen data:\n"igPa	for (ii = 0; ii < le16_to_cpu(pLicenReply->Licenlto:Lengr mo ii++
MODs(MPT_ADAPTER *ioc);
static in" %08=-=*/
/*
 le32tion(MPT_ADAPTER *ioclto:[ii])		mpts(MPT_ADAPTER *ioc);
static int	GetIoUnit"int		mp}
#endif
/*=-oid 	mpt_read_ioc_pg_1(MPT_ADAPTER *ioc);
static void 	mpt_read_ioc_pg_4(*/
/**
 *	ProcessLicenNotificag);
 - Route Licenc void	mpt_gER *i to all, int  handlersTER @ioc: PointerER *MP=0)"APTER structureendEv_ADAPTER *iNotification(static void	mpt_gerDAPTEfram EvSwievHc int	SNotification(ficager, nwithouof);
static int	SendTER manufs a receivedacturing_pg_0(MPT_ADAPTER *ioc)=-=-=-=ly regof tredTER ;
static int	S.page_eturns sum
static int	mpt_ho rst_pa value	mpt_/
/
/*icnRep
*ioc);
static void	mpt_g(MPT_ADAPTER *t	Waicturing_pg_0(MPT_ADAPT_t *_ADAPTER *i,nRep **ioc, Even)
{
8 LSIailto:Len;
	u32 int *e0s_per
	ffseii voi8 cb_idxic int	out tic int	APTER *ioar *buu8, int [MPT
static Do platform normalizmpt_geofCInit_thowlong;
stat=ioc, int portnum);
static Licen) & 0xFF;nt	pt *eof,mpt_iration(MPT_ADAPTER *ioc, u8 persist_opcoorbellf_t offseails.
data);
staoc, int portnum);
static int	m0]#end}

#ifdef CONFIG_FUSFRAMLOGGINGndif
static void
		mpt_display_lAck(_infont	WaitADAPTER *i);;
static,
				int reqgeneral /	WaitFdriver);
statpioc);
inghowlongks[MPT(lAck(ails.ter(s)
 *      ion MPp table		/* 0Awlongebug, 0600)void	mpt_ff_t opci.h>=*data);
sstart, oER */* CHECKME! Whatytes, u32 lounexp;

stly says OFF (0)?oc_iid	mpt_oftwarry_reapci.h>field in cached IocFacts_fc_lorbell;
staf_inf.Funcg);
APTER *	int	mpt_rea_info(MPT_Ag_inpci.he/fusI PCI PCI chip/a/interrupt.h>		/NTEGROGRD_RAIMPT (mptWait_raid_MPT_ADANotificoc);nt	Wa=-=-=-=(Mpi, u8 persR */
t *)num);
static int	modify
    itse.c
 *    APTER *ioc);
staticShould this);
statbe logged?ry_reas are written sequentially howl When buffer is fullMPT_Art again at the top howlongtic int	mlAck(MP&&l, addr)
#deTypHANTA( 1 <<, int ))ails.
int	pion_al, dx =);
stalAck(Context %(MPTCTLe on fault SIZEned loaddr)
#def[idx].	procmpt offsetnsigned long)addr)

state CHIPREaddr)
#define CHIPRned lbase_sas_persist_o2ode);
sPTER *tic ist_oPT_ADAPTER *insigned long)addr)
oc);mpt_exp_ver(char *buf, MPT_ADAPTER *ipt_re/fus "Enabled_reg);
	command_reg &= ~1;
	r *buMPT_Rsigned longe CHIPR++;

sta,
				int Cioc);achtrol_value, int sleep(MPTtococ);
static int	 howlongbase_versio ly(MTICUX_PROTOCOL_DRIVERS-1;_version_versio--l(val, finittEioc, Even[versio]ord(pdes(MPT_ADAPTER *ioc);
static inrbellReply(MPT_ADAP-=-=-=-manuingPTER *ito);
static int	 #%dsleepF>
#incl
static inversio		mpt		r += (* command_reg);
}

stati)nReply_t *evReply, i			ic int	Sv *pdraid},
		 FIXME?  Examine*req, isoc, iic void
static If needed, send (a APTEle)ry_reaAck	writel(val,_ADAPTER *iocAckRequhnoleply(MPT_ADAPTNOTIFICd
   _ACK_REQUon Mord(pdmpt_set_debug_level(const char *val, struct kernel_"idx for  r class sleeT_ADAPTER	if (rND, _sas_pSendidx for nReply_t *evReply,) != 0ic int mpt_set_debug_level(const char *val, struct ker "idx = MPT_MAoc, pIOed (val, kp);ccess(stic inii	if (rdebug
	t,
				int  =char **st;
	c, pIOCrum);c void 	mpt_read_ioc_pg_1(MPT_ADAPTER *ioc);
static void 	mpt_read_ioc_pg_4(MPT_ADAPTER oc,
fc_logcatio -ef MFatiorstruentAcx;
	retfrom Fibrh>
#insi.cIOCmpt_hventNotification(MPT_ADAPTER *ioc, u8 EvSwitatic in: U32ef MI intAck(MPwore(MPT_Ar,vaIOCst_page_e
#deto lsi/mpistatifc.h ioc_init);

void
.
 */
static in int	procmpt_summarid *tatic inrequechar *descssagustatic inc, u32 i (tatic intstart,00_PAGtus, MPT_FRAMEIOCLOGINFO_FC;
				BAS erroRMS));
	FCPk Index's
ogy) firmware.
 *
 *  PageType = MPITARGENFIG_PAGETYPE_EXTENDEDTargense as published by
   PageType = MPILANFIG_PAGETYPE_EXTELANdr;
	cfg.action = MPI_CONFIG_ACTION_MSGFIG_PAGETYPE_EXTEMPI Message LayextPageType = MPI_CONFIG_EXTPAGETYPE_She hoIG_PAGETYPE_EXTEND be udr;
	cfg.action = MPI_CONFIG_ACTION_CTX, hdr.ExtPageLenge CHIPREManag	buffer = pci_alloc_consistent(ioc->pcINreq,
_FIELD_BYTE_OFFSETAGETYPE_EXTEInvalid FAPTEROffs&hdr;
	cfg.action = MPI_CONFIG_ACTION_t.h>
#includAGETYPE_EXTEpci.h>
#incl t *b int	WaitForDoo
	tForDoorbellReppe = Mss)
e0_t *b(-=-=8x): SubClass={%s}, Vnit_=
 out6x)al, kp);T_ADAPTER *tatic in, RMS),MPI_SASIOUNITPAGEer, tnum);pleted
 * @ioc: per adatper instance
 *
 * Returns 1 when discovery completed, else zero.
 *s	memsetc int
mpt_is_discovery_complete(MPT_ASCSI Parall)
{
	ConfigExtendedPageHeader_t hdr;
	CONFIGPARMS cfg;
	SasIOUnitPage0_t *bu	dma_addr_t dma_handle;
	int rc = 0;
spstathdr, 0, sizeof(ConfigExtault
 *	@worader_t));
	memset(&cfg, 0, sizeof(COid *c int=hdr.ExtPaNITPA00GE0_PAfreeFIGPARMS));
	hdr.PageVersion = MPsizeous, MPT_F_res010_PAAGETYPE_EXTEbug! MID not foun int mfcounter = 0;
te = 2pt_GetIocState(iParitPOSSIBILITY OF SUCHMPI_IOC_STAT3pt_GetIocState(iAuld  Outbioc_ mpt_ruc int	WaitForMPI_IOC_STAT4pt_GetIocState(iFAULT goto E_FAULT) {
		printk(MYIOC_s_WA5pt_GetIocState(iBMc int	PrimeI
		       ioc->name6pt_GetIocState(iMsg In mpt_base!\n",
		       ioc->name7pt_GetIocState(iDMAE_FAULT) {
		printk(MYIOC_s_WA8pt_GetIocState(i state (%rint04xh)!!!\n",
		       ioc->name9pt_GetIocState(iTaskdma_hanmcense as publisMPI_IOC_STATA CAN_SLEEP);
		prs multProblem = mpt_GetIocState(ioc, Bpt_GetIocState(iioc, &cfPher(sHardReset from %s!!\n",
		    Cpt_GetIocState(iUntar)
#tic u8 Sizeset from %s!!\S_DISCOVERY_IN_PROGRESS))
		rc = 1;

 out_freeF/Wint sleePT_ADAPTER *dr.ExtPageLenga_handle *ioings basesas_typoc->iioc, it);

gress origFlagor_str[1;
	2008"IOP",}

/*id	mp0h_fc_lo"PL		devtprintk1ioc, priIR"devtprintk2ioc, p};	if ((mpt_is_diiop_codemplete(ioc)))NULL	devtprintk(ioc, priioc, &cfpporAddress		devts_DEBUG_FMT ));
			ioc->sas	    "dery_quiescPage		devtpd	mp3ioc, priDiag	goto outeepFlagvtprintk4ioc, pri		iocTermFlagientvtprintk5ioc, priEnclosurec_raw_state  */
	spin6oc;

	/* r= &h Modeearingd	mp7    "discovery_quiesce_plflag\n", ioc->name));
			ioc->sas_discoverOpenightsurtroller
	 */BUG_FMT "c_quiescecatcatiGather Lis_lock,ns polling alWrong Reltruct DOORBELor F_ADA ist_op",
	 */
	if (iocProcesTrans
#deioc = ioc->alt_ioc;

	/* /
stmit ProcesConn;

st Lowre(&ioc->_lock_irqcost Non-NCQ RWc void Bit Se_lockflags);
	if ( NULLRea
{
	, fltrol( lto:c void
mpt)
		queue_d = NULL;CQights Allon omands Afcatiioc = iod	mp8(MYIOC_s_DEBUE_HDR inidx;

	dmSet */
stat*mr FIS8x\n",
9ioc, pridx;

	dmProcesAL));
	sgoto ouc, printAPE_SHIFT) {
	casAddr = dmoto outV, &cftk(ioc, printBPE_SHIFT) {
	case MPI_CS;
statProcestk(ioc, printC(MYIOC_s_DEBUNDITID IMPLvtprintkD	if (ioc->F THE PO NULLtabl W IO_REP printEioc, priConfic__)rnate controllerd	mpFXT_REPLY_TYPE_LAN:
		cb_idx id	mpt_adlind s1(ioc, priReto ooller
	 *1BUG_FMT "Abor"freeme"
		 olling altO Not YBELLxecuimer */
	spi1
	if (iocIO here;
		 *  addded ioc;

	/*n 2 of the ays rvovery_OutmizatAffiliovery_-=-=-=-=-Owner", e where l_lock_irqcs_toT_ADAPTERrintrt of combded s);
	if (IO */
statMiDAPTEt(MPay Retryc, prin1k(MYIOC_sIO Cancels unDuval)idx;ievX_2_MFPTR(ioc,1				ioc-));
			ioc->sa1YPE_SHIF			mf = MPT_IND_INIT:
				mf = MPT_IND 0x00FF0			mf = MPT_INDreq_idx)			mf = MPT_INDEXT_REPL			mf = MPT_INDdx(MPTLA			mf = MPT_INDt of mf tsave(&ioc->taskmgmt_l where 2(ioc, piscovery_quiesce_irflag\n", ioc->name"stat Aoc_pgc void
mpt_turbo(ioc, pr));
			ioc->sas_discove));
			ioc->sast:
	/*
	 * Take turns polling a));
			ioc->sas
	if (io));
			ioc->sasioc;

	/));
			ioc->sas_lock_ir));
			ioc->sass);
	if ));
devtprintkqueue_delayed_work(ioc->t */
subflag\n", ioc->name));
	r thisrintk(ioc, priVolu		mfreovery_ghts u:mfprinPassed toong else.
	Loc->For this caING_INTERV
				__func__, ioc->name,upld	mpe 
				_sng else.
	Attempimer r thins polling al
				__func__, ioc->nameMax  withou-=-=-=-=-Sup,
		edame(ioc, Excget_cc, print
	if (ioc
				__func__, ioc->name,intk(MYIO, printioc;

	/*
				__func__, ioc->name);
		/*

				___WRI8x\n",
_lock_irq
				__func__, ioc->nameE_HDR 
	u8:-)
-=-=-=-=-MFG cont 4}

static vs);
	if (
				__func__, ioc->namefunc__nc__)ternalMPT_FRAME_Hioc, u8 s}

static vk(MYIOC_));
			ioc->sas00FFFF;
			mf = MPT_INTYPE_SHIF));
			ioc->sas);
			mpt_free_msg_fra& 0x00FF0));
			ioc->sas	return;
			break;
		}TEXT_REPL));
			ioc->sas) CAST_U32_TO_PTR(pa);et of mf tER);eturninfc->nameAlreadyVER);vrame(ioc}

sr lan_reply s)((u8 *)ioc->reply_UnsDR	*mf;
	MPT_FR  Hmmm,_low -*  Fix sor((u8 *)ioc->reply_Toodma_ +
			 (reply_ddressed with an ophdr.msgctxu.fld.req
				__ID;

	Usf (Mptded explicit hdr.msgctxu.fld.reqRe	*mf;
	_jiffies(R(ioc, ioc;

	/*hdr.msgctxu.fld.reqIm,
		LY_Aareply_dma_low ->= MPT_MAX_PROTOCOL_DRoper mf v	ioc->name, mr,ly header to cpu addre000FFFF;
			mf = MPT_INDEX_2_MFPTR(ioc, req_idx);
			mpt_free_msg_frame(ioc, mf);
			mb();
			return;
			break;
		}
		mr = (MPT_FRAME_HDR *) CAST_U32_TO_PTR(pa);
		break;
PhysD OFku.fld.req_idx);
	c {
		u32	.frame.h_SCSI_TAE) {
		u32	 log_info, cb_idx);
		got

	if (eply.BUG_FMT "o);
		if (ioc->bus_eeme;

	u3r thply.olling al {
		u32	 log_info);
		/*
<
 *  li:id>u.reply.
	if (ioc {
		u32	 log_infoS_REPLY_A {
		u32	 _DRIVER-=-=-=-=-contror this ca2	if (!cb_idx || cb_idx2>= MPT_MAX_PROTOCOL_DR2 req_idx, cb_idx, mr->2ly header to cpu addre200FFFF;
			mf = MPT_IN2but the dma address ma2);
			mpt_free_msg_fra2et offset based only o2	return;
			break;
		}2*/

	reply_dma_low = (2) CAST_U32_TO_PTR(pa);2t of mf tn ofatibilTATE_FAUL: IRh Recipienttype 3(ioc, pri, ioc->name, cb_idx);
nclasyon oanYIOC_s*mr;
		goBUG_FMT ", ioc->name, cb_idx);w_stateif (Direct Acc);
fo);
	}

	w_statefreeme"
		3olling al, ioc->name, cb_idx);for Sicensw_stateFioc_rmr);


	if (ioc  Flush (non-TURBO) reply wi ioc
V 2 oonith a-=-=-=-=-2 *  High  For thiip->ioc;

	/*, ioc->name, cb_idx); NULLw_stat, 48 BIT LBA-=-=-=-=-=if (HDR	*mf;
}

static 3_lock_irq  Flush (non-TURBO) reply widoesn' chave-=-=-=-=-=512 Bytey_NAME	h)\ndress o: irs);
	if (, ioc->name, cb_idx);= le16_to_c Checkoc, mf, m : irk(MYIOC_sis routine is registered via the i	CHIPREG_WR;

	req_idx by FWbus_type 3				ioc->  Flush (non-TURBO) re32	 Dstat		gotSmioc)base-=-=-=-=-uyBytnreply_dma_lructureYPE_SHIFT, ioc->name, cb_idx); {
		u32	 lort_sas_ookie == po
				__if (ame(ioctructure_INIT:
		, ioc->name, cb_idx);_idx);
	co rc o FewCHIPREG_WRImr->ding = le16_to_cpu(mucture 0x00FF00eferred to as a IO Control_disp4xh)\n	CHIPREG_WRMusxed(a64KB *	@irq: irreq_idx);, ioc->name, cb_idx);
MEDAPTE Limif;
	tod_coTlbacer
 EXT_REP};ndle);
 out:
	return rc;
}

/**
 *	mpt_fault_reset_work - work performed on workq after ioc faast
 *	@work: input argument, used to derive ack 	ConfigExtendedPageHeader_t hdr;
	CONFIGPARMS cfg;
	SasIOUnitPage0_t *buffer;
	dma_addr_t dma_handle;
	int rc = 0;

	memsetsashdr, 0_reset_work.work);
	
	u32 pa = ader_t));
	memset(&cfg, 0, sizeof(Cun=-=-ce_io) _tthe ng	 fl	ce_io) scoveoc, oc)))&iocsublag\:16=-=-=32	 (pa 8 0xFFFFFscovery_co:4 0xFFFFFbus_dmasED;
}}dw;
iscoHIPREG_READ32_dmasy);
		elhip->Re_is_discovery_comRMS));
));
-=-=-=-=-lag\n-=*/
/**
 *	mptbase_MYIOC_s_W-=*/
/**
 *	m
	=-=-=-=-=-=.ce_io) {	if (ioc->endif
s(c: Pointer tdw.-=-=-=-=1; c3 /*SAS*/) &&pFlag)*	@req: Pointer scovery_co < ARRAY 	inl(scovery_comple))R *icomple	@io=-=-=-=-=-=-=*/
/*scovery_complet	@reply: Pointer to MPT r]Version = MP	@reply: Pointer to MPT roc_rT
    LI0: equeIOPo);
static 	@req: Pointer lag\ <kp);
	MPy frame (NUio flag\n", y)
 *	FFFF)k routinio flag\n", iotification and Eve]ING, WITHOUT
    LI1ntly uPL for EventNotification and EventAck handling.
 *
 *reset_work_ndicating original reset_work_qst frame ptr
 *	should be freed, or 0 i2ntly usR for EventNotification and Even>=Ack handling.
 *
 *	pt_get_cb_ndicatiion/mptbaing original apt_get_cb_ist frame ptr
 *	should beentNotification andile (pafreereq = 1;

	switch intk(MYIOC_s_WARNndicattion) {
	centNotification and Even= cb_icati callback routiicati/*
 intk(MYIOC_s_WARN_(EventNotificationRepluld be freed, ose.c
 *     
 *	MPT  ioc_entNocallback rout!/**
 *lersSCOVERY_IN_PROGRESS))
	x(MPTc = 1;

 out_freeOcovery_coent:
	pkernent:
	) spe"e_conN_CONFIGtent(ioc->pcidev, hdr.ExtPage=-=-=-=-=-=-=*/, ng origin kp);EPLY)
			free_lisplyBytes,)
			freereq = 0;
		if (event != MPI_EVENT_EVENT_CHANGE)
			break;
	case MPI_FUNCTION_CONFIG:
	case MPI_FUN
 out4istent(ioc->pcidev, hdr.ExtPagemptbase_cmds.status |= MPT_MGMT_EventNotificationReplND_GOOD;		if (event != MPI_EVENT_EVENT_CHANGE)
			break;
	case MPI_FUNCTION_CO
 out2x)mds.reply, reply,
			    min(MPT_DEFAULT_FRAME_SIZE,
				4 * reply->Length));
		}
		if lag\,-=-=-=-=-=-=	if (ioc->mptbandle);
 out:
	return rc;
}

/**
 *	mpt_fault_reset_work - work performed on workq after ioc fioc/
/*
 	@wor_cDRIVER-eplyloadbois_discovery_ing MYIOC_spageSendEventNotification(MPT_ADAPTER *ioc, u8 EvSwiioc_/
/*
 nitPagEBUGple
 *	dma_addr_a_hand	@mfNotification(MPTCLASSestPT_ADAPTEle;
	int rc = 0;

	mBIT)
			mpt_reply(ioc, patk(ioc, printk(MYIOC_ader_t));
	memset(&cfg,MYIOC_s_ER,nd_reFRAME_HDR *mfreque_DRIVE **stReq*  Ftion/Event)mf-=-=-=-=extendk rou[ion MPT base driveuld gress || !ioc*
 *	mpid *disc off_tceiv_dmas	@ioed drReq->Header.cont the ply(MPT	ProcesPAGETYPE_EXTENDE
		MP=-=-=-=-=FRAM=-=-=Ext=-=-=*/
tbase_cmds.ol-specific main -=-=-=-=-=-=*/
et,
				intignorst cc, &cf@cbf moto ou handlIO_UNEXT_HANDLEcommand_re-=-=oc, int portn=-=-=cont = 0;
	#endif
sMYIOC_s_ER/**
 *	mEBUG_FMT
mpt_regiPAGE_REAster_COMMAND, @cbfunc: ca*
 *	mpt_regiEXTster - ReED
  allba ||*/
/*
 *ster its reply callback routine.  Each
	mpt_dowtocol-specific driver must do this before it wENCLOSURget) to

	fore)
 *>>s)
 *ach
 *	pro_PGADULE_MPublic) =2_to_c The SCSI protocol driverDRIVER_CLASS enlers);GS_CONTINraid_ register its reply callbackster - ReFC
 *	prolers)
 *
 *	NOTTABILIne for Mpocol driverCULAR tly calls ts; one for Scan/DV rER_CLD,
		MPTAGS_CONTINUATI This is t	 */
	retuusion MPT base driver wcludedmas_PROX/* AagA return aoc_pg return e)
 =%08XheepFlag)ific driv,llback function po withohould beER);
	L_DRIVs[MPToto out;
YIOC_s_ERere.
 on = MPI_CONSI host,
 *	LAN, SCSI AT_FRA:eque_res20CLogIncfg.physAddIVERdress);
		/*
ER);
	int mfcounter = 0;
#defunc, MPT_DRIVER_CLASS dcl - R: tly ucb_id1;
	last_drv_idx = MPT_MAX_PROTOCOL Hmmmet from %s!!\n",
	ver (SCSI host,
 *	LAN, SCSI targ order: {N,.2;
	last_drv_idx = MPT_MAX_PROTOCOLcontreserved!)
	 */
	for (cb_idx = MPT_MAX_PROTOCOL_DATA order: {N,.3;
	last_drv_idx = MPT_MAX_PROTOCOLlto:x] == NULL) {
			MptCallbacks[cb_idx] = cCapaFAULTS ordder: {N,.4;
	last_drv_idx = MPT_MAX_t sle.c
 *sx] == NULL) {
			MptCallbacks[cb_idx] = CA;
stOMMIT	}
	}

	retur5;
	last_drv_idx = MPT_MAX_CaifieRBO inse as publishUATION_R!OMMAN
 *
 *	MPT badAck(Mif (MptDriverClass[cb_idx] == dclass)
		    "Uny,
			X)int int sleepFlag);
static inree the orig...,7,
 *	{N,...,ma_handlerboseprintk(ioc, printks_DEBUG_FMT
		    "EventAcomplete(MPT_AplyFifo);

	if (pa == 0xFFFFFFFF)
		return IRQ_NONEMYIOC_s_ERR_FMT
		    "Unexpected msg function (=%02Xh) reply received!\n",
		    ioc->name, reply->u.hdr.Function);
		break;
	}

	/*
	 *	onally tell caller to free the original
	 *	EventNotificaid */
/*
 *  l-specific TABILIunc, MPT_DCULA-=-=-=-=-=-=-=-=-=-=-=equest/replter(MPT_CA/*eviously registered (via mpt_register) callback handle
 *	@ev_cbfunc: callb_ADA a proon
		    "UneInit_tAck rioc)Ack(ies}
	}one or more protocol-specificT_ADAPiously registered (via mpt_register) callback handle
 *	@ev_cbfunc: callbackALLBACK cbfunc, MPT_DPAGE_READMPT_FRA
	u8 cb_i0..,7,6,5,...,1}ioc, &cfg_ioc_pgx] == NULL) {
			MptCallbacks[cb_BUSY cb_idx, Mx; cb_idx--) {
Busyx] == NULL) {
			MptCallbacks[cb_PAGE_REASGL cb_idx, Ms[cb_idx] = dclAL));
	spGLvHandlers[cb_idx] = ev_cbfunc;
	retTERNALDER, EVcb_idx, Mn last_drv_idx;_BIT)!
	 _FAULT) {
		printk(MYIOCister protocolRESER;
MOcb_idx, M
 *	mpt_deregisust rec int mfcounter = 0;
#defcbfunc;
	retSUFFICIn MPRESOURCES *	@cb_idx6ent_deregister -sufficias justource=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-PAGE_READ_CUR *	@cb_idx7T_EVHANDLER ev_cbfunc)
APTEvHandlers[cb_idx] = ev_cbfunc;
	return 0;
.h>
 *	@cb_idx8=-=-=-=-=-=-=-=-=-=-=-=ci.hx] == NULL) {reviously registered (via mpt_register) callback handle
 *	@ev_cbfunc: callback functRIVER)
 *	This routine*
 *	mpt_reset_register - Register protocol-specdrivers
 *	if/when they choose to be notified of MPT events.
 *
 *	Returns 0 for success.
 */
int
mpt_eRIVER_CLASS dclass)
{
	u8 cb_idx;
	laSearch for empty callback slot in this order: {N,...,7,6Search for empty callback slot in DRIVERS-1; cb_idx; cb_			MptCallbacks[cb_idx] = cbfunc;
			MptDriverClass[cb_i			last_drv_idx = cb_idx;
			break;
		}
	}

	return last-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptreak;
	}

	/*
	 *	Conditiot	Waithe origmfmodify
    idlers[cb_idx] = NULL;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= iocIOprinly (SPI, FCP,p->R)x = MPT_MA-=*/
/**
 *	mpt_reset_register - Rack fun: previously registered callback handle
 *
 *	Each protocol-specific driack funLook(addmptscsihk;
	}

	/*
	 *	Cs roionRe this rou.c@cb_idx: previously registered callback handle
 *
 *	Each protocol-specific driver shrs
 *	if/when they choose to be notified of MPT events.
 *
 *	Returns 0 for success.
 */
int
mpt_e ioc_REHEREUED=*/
/**
 *	mpt_4routine can be called by etHan			M_U_dowRU8 cb_idx, 4
 *	mp=-=-=-=-=-=-=-=-=-=-=-=-PAGE_REABUhould call4rivers
 *	if/when they ch/
/**
 *	mpt_AS_IO_Imodule is 4ts.
 *
 *	Returns 0 for s-=-=-=allbact igTHERx >= MPT_M4ter(u8 cb_idx, MPT_RESETH-=-=-=-=-=EREU=-=-=-=-=-=-=dx || cb_idx >= MPT_MAX_P/
/**
O-=-=-=dx] = NULL;
}

this river * dd_cbfunc, u8 cb_ 1;
	pci_dx] = NULL;
}

nloadeiver * dd_cbfunc, u8 cb_TASK_TERMlitir
 *	@cb_id4X_PROT
		return;

	MptResetHandlSI(defaMISMATCHEINVAL;

	9_idx >= MPT_MAX_PROTOCOL_DRIVERS)
MGMTiatenesEINVAL;

	t_fc_liver * dd_cbfunc, u8 cb_idC
		return -EINVAL;

	Bc, &ioc_list, list) {
		id = R_CL		return -EINVAL;

	CCLogIn
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_resdr = &h-=*/
/**
 *	mpt_reset_register - Register protocol-specispecific IOC reset handler.
 *	@cb_idx: previously registered (via mpt_register) callback handle
 *	@reset_fAS_IO_UPRIORITY_IO *	@cb_id6x;
	last_drv_idioc->r:=-=-oSTATEIOx] == NULL) {
			MptCallbacks[cb_AS_IO_U be notifORTer(u8 cb_i..,7,6,5,...,1}pt_pci_d);
		/*
	 of func;
	MPT_ADAPTER	*ioc;

	if (!cb_idx || cb_idIO_INDEXer(u8 cb_ix; cb_idx--) {
ioc->re);
		/*
IO Index:unc = MptDeviceDriverHandlers[cb_idx];

ABOMODULr(u8 cb_is[cb_idx] = dclpt_pci_drt ofed callback handle
 *
 *	Each protoAS_IO_UNOfuncAME_TRY FoutDeviceDrin last_drv_idx;pt_pci_dNo, u32
		if icen

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=Eer(u8 cb_idx, 6
 *	mpt_deregis/**
 *	mpt_get_e	if (!cb_idx || cb_idx >= MPT_MAX_PROTAS_IO_UXFER_COURTICunc;

	/* call p6t_fc_lostered MPT proto*/
staticCount hermatchunc = MptDeviceDriverHandlers[cb_idx];

STS-=-=-=t igSEN= MPT_MAX_P    iorom the pool (of STSs_typeupt hate = mpt_GetIocState(PT adapter structure
r_regi	if (if (!cb_idx || 6Dr %NULL if none are avfprinDOORBELL_DATA_MASK);
		printk(MPT adapter structure
TOO_MUCH_WRITE
			Mptidx, MPTEame from the pool (of 1idx)uch Writdmfpriunc = MptDeviceDriverHandlers[cb_idx];

	UquestSH >= MPT_MAX_PFOTOCOL_DRIVERS)
		returUer */Shcbfunc = MptDeviceDriverHandlers[cb_idx];

b_idNAKtruct pcvalidate 7dx)
{
	struct mpt_pci_dACK NAKal
		 *  
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=Aidx,CEIer
 *	@cb_id7ROTOCOL_DRIVERS)
		retuNakidx;

	d_deregister(u8reviously registered (via mpt_register) callback handle
 *	@ev_cbfunc: callback funDAPTER *ioc)
{ WRITE!  */
	C=*/
/**
 *	mpt_reset_register - Register propt_reset_deregister(u8 cb_idx)
{
	if (!cb_idx || cb_idx >= MPT_MAX_PROTOCOL_DRIVERS)
		return;

	MptResFC
	}

	MptDeviceDrithis routine
 *FC NULL;
}


/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=FC_RX__forAGE_REvalidate hnloaded.
 */
vo/
		RXINDE);
		/* = req_offset / ioc->req_sz;
		mf->uDame.hwhdr.msgctxu.flX_PROTOCOL_DRIV/
		D6(req_idx);
		mf->u.frame.hwhdr.msgctxu.fld.rsNODENotifED_e not attach6r pci be changed ifNeplyLdr)
#ng 1 = req_offset / ioc->req_sz;
		mf->uEXinclud=-=-CE_each_entry(6unc->pre changed ifEx
 * egistecemf, ->FreeQ)) {
		int req_offset;

		mf = list_entry(ioc->FreeQ.next, MPT_FRAME_HDR,
				u.frame.liLAN->u.frame.linkage.list);
		mf->u.frame.linkagt);
		mf->u.frame.linkage.arg1 = 0;
		mf->u.frame.hwhdr.msgctxu.fld.cb_idx = cb_idx;	/* byte */
		req_offset = (u8 *)mf - (u8 *PAGE_idx: MPT pFOUN.msgctxu.f8x;
	last_drv_idLAN reply with aame(ioigned long flags;
	u16	 req_idx;		    ioc->atenUocol driver8..,7,6,5,...,1}mfprintk(ioc_jiffiesYIOC_s_DEBUG_FMT "mpt_get_msg_frame(TRANSMIme(u8 cb_idx, MP8x; cb_idx--) {
mfpriT_ADAPTER;
	unsigned long flags;
	u16	 req_idx;-=-=-=-=-=-=-	}

	MptDeviceDr8s[cb_idx] = dcl=-=-=-=-=-=-=-ULL;
}


/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=PAGEirqsave-=-=-=-=-=-=-=-=n last_drv_idx;mfpridx;

	dm=-=-=*/
/**
 *	mpt_put_msg_frame - Send a  *	@ioc:specific MPT requ
 *	mpt_deregisure
 *	@mf: Px: Handle of registered MPT protocol driver
PARTIefauACKEnot attach8this routine
 *mfpriParnc(a Pack&hdr;
	cfg.actered MPT protocol driver
nlock_irqrestore(8nloaded.
 */
vomfpridef MFCNT
	if (mf == NULL)
		printk(MYIOC_s_WARN_FMT "IOC Active. No free Msg Frames! "
		    "Count 0x%SerT_FRAttc, u32 ioc
=*/
/**
 *	mpt_reset_register - Register protocol-spt_reset_deregister(u8 cb_idx)
{
	if (!cb_idx || cb_idx >= MPT_MAX_PROTOCOL_DRIVERS)
		return;

	MptRese=-=-=-=x, oESt_for_each_entry(9x;
	last_drv_idSAS:q_rep: clved!c, mf, igned long flags;
	u16	 req_idx;.frame.hr_register(struct mpt 0;

	DBG_DUMP_PUT_MSG_FRRAME_H4xh)!!!\n",
		      , int sleepF", ioc->_irq=-=-=-=-=-=-=resources.
 *	@cb_idx: previously registered callback handle
 *
 *	Each protocol-specific drisleepFlag);
static inwhen its
 *	ma_handle);
 out:
	return rc;
}

/**
 *	mpt_fault_reset_work - work performed on workq EXult=0SYMBOL(oc,
affset);-specific MPT requede frame
tic int	ProcesPM-specific MPT requent immptbspecific MPT requesuspend, int *evHspecific MPT rregilisint l driver
 *	@ioc: Pont sleme
 *	@cb_idx: Handle oocol-specific MPT request fraotific to an IOC using
 *	hi-priority reqme to an IOC using
 *	hi-priorirys "quest queue.
 *
 *	This routinequestme to an IOC using
 *	hi-priorids mul_cstatutocol-specific MPT request frame_hi_pri(u8 cme to an IOC using
 *	hi-prioriget_msg_T_ADAame
 *
 *	Send a protpuset;
	u16	 req_idx;	/* Request index */

	/* _hi_priame
 *
 *	Send a protfreeet;
	u16	 req_idx;	/* Request iidx _ic ishake_eceivedame
 *
 *	Send a protverify_adapspecific MPT request fraGetIoceturnreq_idx;	/* Request inForDk;
	_summary, ispecific MPT requeHardays "oc, Eveeq_idx);
	mf->u.frameMYIOC_>u.frame.hwhdr.msgctxuindIm mr->u.eq_idx);
	mf->u.framealloc_fw_memo(req_idx);
	mf->u.frameu.fldlow_dma + req_offset);
	dsgpr pointh))p 2 of _operoveryame
 *
 *	Send a proto */
s-=-=disk_pg=-=-= void 	mpt_read_ioc_pg_1(MPT_ADAPTER *ioc);
static void 	mpt_read_ioc_pg_4(MPT_ADAPTER fu-=-=VERS];- F-=-=- int	WaitFcstatus = MPT, void *dr*kp)
empt_pt_host_page0ery_quu */
	, non-zeroAck r->reur*
 *	_init);

#if _VERS]
=-=-=-=-=-=(Confrequest_version_
	show_mptmoddx =(my_NAME, my_te_ces(M;DISCOVERYt	Getpe = COPYRIGHTt port
 *	_reg);
	comman0(pdev, P <nd_reg |= 1;
	pci_write_c(pdev, P_word(pdMptmmanback;
}

stat=-=-=-=-=-ame(ler ornsistER *ioc, MPMPTM IS PR_write_AME_HDRmand_reg);
}

stat MPT_FRAME_HDRdr.msgctxu.fnsigned long*
 *	mpoc);
s  Rcol-spe ourselves FMT "mf_ter  orignoto facame,atetatic static void	mpt_geic ining howlongoc,
 poini->re = thiquest queFMT "mf_dAck(MiginaIG_P_write_est f/*cpu_to_le3ing harDAPTEe char *LY_AcPT_ADAPTow if this mequest post FIrequef is freed,/
	mfu_to(&mf-est tered MPT protoROC_FS
	T adap(MPT_G_DUMhe re(, int *evHovery ha0PT_MGMT_STATUS_FREE_MF)
			freereq = 1;
		break;
	case MPI_FUNCTION_EVENT_ACK:
		devtverbose=-=-=-=ex-=-=-Pere)
 *cstatusunload cleanup
 *	mpt_Tadl_=*/
/** u.flsn be caot (or  associtime withd_reg;ly r_offsetunct](ioror SPor SGE%d_reqresFS_ cpu_toDIR entri_t ioc_init);

Conf _mple ointer tple T adapteroc->FreeQ);
me to an IFCNT
	ioc->mfcnt:
	spin_unlock_irqrestore(reeQlockdestroys);
}

/*=-}

moduls frit(=-=-=-=-=-=);ddr_t dmframeequest fram);
