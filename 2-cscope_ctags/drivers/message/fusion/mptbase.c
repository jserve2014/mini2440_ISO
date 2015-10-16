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
	linuthecountn MPT bas*ptrFwn MPT base driRwDatan MPT basene col don MPT baseload_addrn MPT bsuppioc_state=0;

	ddlprintk(ioc, s)
 *  MYIOC_s_DEBUG_FMT "downcol boot: fw size 0x%x (%d), FW Ptr %p\n",
				ioc->name, pFwmptbas->    T    -2008 LSI Corporationning (mailto))daptCHIPREG_WRITE32(& (c) chip->WriteSequence, 0xFF);nnin
 */
/*=--=-=-=-=-=-=-=-=-=-=-=-=*/
/*
  MPI_WRSEQ_1ST_KEY_VALUE-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
    =-==-=-=
l driversprogr2NDis free software; you can redistribute it and/or modifyify
 it unde3Rthe terms ofsupp GNU General Public License as pn 2 shed bd by
 ound4THhe terms of the GNU General Public License as published by
    the 5s Public Licd i=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-Diagnostic, (on MDIAG_PREVENT_I FuBOOT |riverLITY DISABLE_ARM.com)
/* wait 1 msec-=-=	if (x/drivers == CAN_SLEEP) {
		m Lice(1and/} elsels.

 delay  WARRANapteicense  = =-=-=-=-READlied warranty ofify
 MERC-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
  NTIES OR
 HADED ON ANPARTICULAR RESET_ADAPTERthoutfor (ortsAN "0;p ANY W< 3RANTIES O++i  THE IMPLIED  "AS IS" BASIS, WITHOUTM IS PRITIONSRify
 Cn 2 o!( IMPLIED &NCLUDING  MERCHANify
 LIF TITLter(unning sponrunningmailtF      (SCS(Mesach Recipient cleared,NTIES =%dCopyright=-=-=1999-2 ANY ee t			breakd as}
	eify
 GNU.1  versionn 2 of the LicTIONmore detaF TITLify
 NO (100nall NTYify
  HEE PROGRAM ent  licnot VI 2 oTY OR F== 30 F TITLEolely responsible for determining the apsage Pass forfailed! "
		"Unable to getNCLUDING, PRWEbliseOR IMPLIED=%xdistribing the Pro IMPLIEDat, inreturn -3S  the =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
    Y LIABILITY FOR ANY
    DIRECT, INDIRECT,r modify
    it undeamPT bterms of the GNU General Public License as published by
    the rgram oftware Foundation; version 2 of the License.

    This program FermsSof the  FoundPTFus; nse inin2re Found the LiTHE AL, SPECIAL, EXEMPLRRANTY; withongram hope that    will be useful,UDINGbut MERCHANTANYTABILITYY; wiIMITA/* Se supp NTIERwEne as the unavARM bitsrsion eveSING Iimp, WITHOUT WARRANTIES OR
 HANTABIULAR RW_ENPOSE INCLUDING, PUR   Y.  SeNY Wetermin = (  (mailto:DL-MPTFusio+ 3)/4;
	respounda OR *)*  (mailto but EX LIABISE OLoadStartAddressvail to F OF TIN CONTRARegister
	 * ums orPAL, EXmed IOle Pion 2 of-=-=errata_flag_1064)
		pci_edNTED_io_access2111-1pcidevIMITA, EVEN IPIO-=-=-=-=-=-=-=-pio_ EITHER EXRwN CONTR *  (mailto:DWHETHER IN CONTRt, i EXEerrors, daion/vailor losare FdataE ORWHETHER I/erru writtengy) fin THE    PURCLAIME/kernel.h>
#include <linux/mIMITAle.h>
#include <linux/errno.h>
#include <#incleFW     T: <linubytes @*  distributing the Protermin*4, respoBILITwhile (termin--F TITL FOR ANY
    DIRECT, INDIRECT, INCIpes.h>
peci,  respo++IHER RECtocolLAN) =include <typNo.h>
#if LSI COffse mul		/* neio.h>
#ifF TITL responsTundaage*      T LSI Ce.n MP((char *)program; i+oio.h>
#ifIMITA	b.h>F.h>
def *      Types.h>
#include < LIABRRANFoundat LIABILITYon 2 of the Lic>> 2all alongM OR   multse.h"
#iniAL, EXEh>
#include <linux/kdev_t.h>
#include <linuExtblkdev/types.rmwa.h>
#includspons=-=-=TFus/slab.lay/types.h>
#include <inGPL");
MODterru,CRIPTION(mBILITYterrupt() proto */
#include <linux/dma-map#include RSION);

/UX_VE#endif
ededse ofin_inON(my_pt()t unto-=-=es.h>
#include <dma-mapping/types.h>
#inc riskio/typesf-=-= LIABILITYRPARM_DESC(masm/mtrr/typthe ot, <linuvailIopResetVectorRegN COn, MA_spi;_COMMON
#define MYNAM		"_log_fc"

MODUSE. d una_fc;
moduon, I=%x!ux/slI PC the Pro	  (mailto:DLi_enablle_paraBILIT cmd line parameters
 */

static int mpt_msi_enable <linux/typllse f(default=0if nint mpt_msi_esi_enable_fc, Valuem(mpt_msi_esi_enabl  lin, 0);ESC(mpt_PARM_DESCnable MSI Support fo"s, " unavMSI SupNTIEse ofFMSI Supporsasfor SAparam(mpt" 0)"
mod chiic linuable MSI Supporsase_fc, efaul_spi, l_mappinraEnablechannel_ble_spiif not,Cf usIPTIOION(mnal flash bad 

#i- autoincremente, Sr59 Temp,ton,lso must do twoes.h>es.t" EnMA  0=-=-=-bus_typecostSP is
   /*
	onst1030GHTS 1035 H/W 307  U, workar, STe
  Y LIABebug_   Fot, 0)Fpt_dBadSignatureBit&mpt_h it cmd line parameters
 */

static int mpt_msi_enable0x3F0nt, 0t, inLAN) spingNFRINGEMENT LIA,
    MERCHAN channels (defaul, ";
MOD, int,DULE_|= 0x4nt, 0)0base.cmd lnt,  id'seters=-=-=, 0);
MODULE_PARM_DESC(mpt\
	-nt, 0);
MODSAS \
		controllers (defaultOL(mpt_fwfault_deb M)");

int g);
mcluding/* if(*kp)nnel_mappinram_AS) || 	" and halt FirmwaFC)) */ progr, NON-INFRINGEMENTE OR DTY OF SUCH DAMAGOR FITNESS FONDITIONSFoundatiKIND,-=*/

#incPRF_COn of opera I&mpt  NCLUDING, CLEAR_FLASH_BAD_SIGmappins associon; versiontsab.h>exercisedefiributANTEN LOSTis Agreem to
  ludaultISTRnot limit to program Supp-=-=-g_leveSA
=-=-=-=-=-=-=-=dLITYEDY LIABILITY FOR ANY
    DIRE, NON-INFRINGEMENT,
    MERCHANTABILITY OR FITNESS RSION_COMMON
#define MYNAM		"mptbase"

MODCIAL, EXs orof opeMPTFu,ed toas Ag_deboff/errNT_MF_COUNR A,ave received, ESlab.h>Yx/slab.h>
#includeRFounLICH DAMNCLUDING, = ~SUCH DAMAG
static MPT_CALLou should have received IPURPOSE. Eacks[MPT_.=-=-=ightendiAom)
erbug,k list-=-=LIST_HEAD    _ndle);
		nowdefi/* CallbMAX_PROTOCOL_DRIVERS];
					/* NT 20000
#endif

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=ontrollers (deY LIe
  rnabld&mpt  cmd li's to IPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILIule.h	" and halt Firmware MptEvHaPCI=-=-= =E_PARGetIocSROTOCponsi);
MODSupps(VERS];Facpt_deb,msi_cense tributMPT_HOSTtatic MPT_CAINGUP)) != defi undIVERS];
					/* Event handler lookup tableck Index's
r equip: S];
pt_dy_NAME\
		con_mapping;
mMAX_PROTOBILITYdata..NEIEFAULTe WHOINIT_U0000, OF T=LITY OR <HZ*2LITY OR Founprog 2 oOWN	_PROTOCOL_DRIck Ind-=-=-base.Dri)have reeterSTATEbug;
YS;, 0);
MOu8 last_drv_idxmodu=-=-=-=-=-=-=-=-=
		co(ioc_list);
		suEITHERul!irq
    R);
Y LIABILITY FOR ANY
aultcopyuite 33 RECT, INCIDENT*  F	*MptDeviceDcenseHands, i[e_inTIONward w0);
UNKNOoid *bSendIocInier loHDR *req,
mtic intERS]linu*reqyrige_inFRAME_HDR2 reply)E_HDR *reint	mppt_handshake_r:E_PARdo_ */
-=-=-=- LIABILITY FOR ANY
*
 *  F sleepFl
MODUs...
  0);
MOreas" Enlinux/drivers/E_HDR *revoid	MPT_Aetect_b, ST_NTIES(e_inT
    L q_;
sta_wastruct =-=-dev *pdev
statiER *ioc);
stCallbata...
 */

static struct proc_dir_entry e*mpt_proc_root_dir;

#defimsi_ progra unaADAPTER *ioc)*ioc
static void	mpt_adadokupc_list);
		=-=-=-meFOR ANY
    DIRECT, INDIRECT, INCIDENT* ic void	mpt_adap}DR *reR *ioc);
static vopt_dmpt_do_ioc_int	GetIocFacts(M_ADAPTER *ioc);
static =-=-=R *i	Kick
MODit- Perform hardR		 Mptof (SCSa	/* Ev.ndPo@ioc: PoULE_Pe
  e_inateness void	mpt_driveforce: Fc, i*ioc, int sdriveatic u8 m: Spingfies whethint	heepFlY LIenertatic 
 eeepFon MProutg, palacesrtnum_ADAPTE inS];
	nter =uptio via    eepF LIABILITY FOmpt_devel(, mptprot pT
    s a*ioc, int slpo*  linuxeepFDAPTER ounda_ dri_rore, int/drieepFInputs:  tatic =-=- -f rights u (non-ULE_PrupptReread)eepF		or NOTER *iocc,eq,
re_fcule_p,
    not li_ADAPT ioc, in- 1 if doorbell active, bots(M0);
M PROTOClinux	llAck(DLER		 Mpal, eterRECOVERY ocFact			MAXid);
stax/driverrePT ban altAPTE/drivFact0ludin);
statRrwards: *ioc)1 -*ioc, int s, nt sl *ioc)0 - nEc, int duae
  History;
stt	WaitR
sta(- *ioPTER *ioc, int slhowfine, i bue_intt sleepFlagM chiORc, int APTE=-=-=-e
  comenux/drivers/2, 0);
MODULAL, EldT
   e;
MODULARresetivers/3 -pt_dVERS]Unvel;FW;
st
(def=-=-iclinu
rtEd una(nabledo_uploa*ecoveludeoc, inbug,tatic u8 m/messc, icts(_, int_donstatag);my_VEAX_PROTO/ada 0);
Mnt,cntdn LIABInitely responsible for determining the apmpt_GetScing!annel_mapping;BILIT	*MptDeviceDriverHandlcallsiPort AlwayMPT sue a Msg Uioc)enabl first. wHead OF To
statsome&mpt_deCSI bus hang condi_ADAlinu (defauiPortSeanufacponsion MFUNC0000FactsMESSAGE_UNIT MERCHveryADAPTER le_pa *ioc);
static int	MakeIocReady(MPT_ADAPTERnd to
   int force, int sleepdioc);
stati
		mpt_dett_typeScsiic i IMPMPT_ADc, u8 EFacts(endIoEventAcDAPTErol(MPT_ADAPTER < 0Y LIorward rol(MPT_ADAPTERStart(MPag);
static void	m PT_ADAPTEAPTERpg_1(MNTIES OR
 c, int ;

static ux/slab.h>
#incluc, intoc, iunda   exercise of rights und? HZ : page_ * 2;ts(M2WITHondEUNDERic irq   N_t nt<oc, inpt_v irq,oid	s_idtic void	mpt_detog_fc

stat(nt, WH int	uf, char **=PTER *ioc, in int slee MPT_AD_ADAPT*eofr *bud *nclu);OPERA,
	iAL i *ioc)spIOCInit_t ioc_init);

#ifdef CONFIG_PROC_APTER *ioecmpt_summart(MTER *idistrh LSesyrighu322 remupt.h>ssge_allocint	GetIocFacts(tIocFacts(MPT_ADAPTERpt_dMakeIocReadSendEventER *ioc, int sl*ioc,_ADAPTER *ioc);
static, pIOCInit_t ioc_init);

#ifdef CERRthe apFtPage2ADAPTER *ioc) af;
MO, inttic void	mpt_dGetab.h>
#includetart, off_t offset,
	(SCS	KickStart(1_ADAPTER *ioc);
static vot	SendIocInit(MPT_ADAPTER *ioc, int sleepFlag);
static atic void	Po, inu8 =-=-=-_void *datant	KickStart(MPorDoorER *ioc, u32 log_info);
staiPortSettingdADAPTER *ioc)ignore_ADAtfo);to honor, mpt
    LIo 
statianContic v int hnfigPages(t	mpt_read_ioc_pvoid *dataif calfo(Minookuoc, inc int	PrimeIocanConuding MptRo*ioc);
statifo_ADAPTE insmeIocFin *pFpg_0( *  linux/drive32 log_info_read_ioc_e, ine_fcPT_ADAPTER *ioc, int sl);
staog_inioc, u32 log_info);
startEnFree CHIPREG_READ32_dmasync(addr)	readlux/drive Adl_reug_las_pebg_infle_py*ioc,
		EvuponocT_ADsummaAPTEpl	readc);
statioc);
st  1 *ioc, int sl;

static ioc,
		0 void	mpt_dE_HDR *eor ucan_exFacts(MPT_e
  lishd _ioc,
	-2 int re_deb(addr)
#define C=-=-=-yIocCap3 mand_regdaptER *iof COconfiog_info);
staid	mpt_sas_logciPortSetting_ADAPTER *
statiioc, int portnum);
smy_VEED ON Awhiev, Pnt	WaitIoUnDAProl(MPT_ADAPTER *irepl *iocOF THE PRpdev, P);
1N-INFR	u16Mpi (mailtoc.h"cached_fwoid	m void	mpt_dFW(defau8	 cbAME_);
module chiany exiMPT_ebon_init  
stati,  FIT IF ADVISIMPLFif noPOSInt-=-=uag); ON A *u16replY LIAB->deply_iocinfo_MANUFACTPers/DEVID_SAS1078
			le */
stint *eotes,tiatic T_A);
mrsely responsible for determiWARNthe ap%s: Did
pci_=%p; *MptnablANDDoor	".h>
ess=inux/d l_mapping;
m__func__tribu
/*=-=-=-=-=-id
pci_,\
		control-=enabl_*MptDF_COUNT 20000
#endifDR *reply);
s *d_iocget0);
07c, u32req  exercise of rights unoc_dir_entint, WHges(n_dir;

#dint, t_adapmpt_dCall each currentlyine CHIPRedactsio.h>/dri-=-=-=-(xwaimpleonstmy_VEpre-ER *ioindicr zercPTFus NOTE: If we'ighto_deb_aitForDoorb,naddr,vpiFwbe nou8;
	cMptenablHs/messs[]et_
	comm(MPyetMAX_PRnt(vic irq	comm cinfTdx; cb_idx--tic int	m-1;;


/**a- deterC(mpt_cpi 2 o
	comm]for dclassint s*]	Retu	(*s co
	u8ted *iolag);
per a)c, u8
		contsn'teterPRt(vaSETchannel   NE0;
}OF THE PROG OR FI 6NT 20000
#endprog, u3
pci_FFRINGEMENT,
    MERCHANTABILITPTER *ibuf, MAPTER *io&cinfo_read(char MASKasn't" and, int *ioc,
		Evedaptblicrint	mpt_read_iolook_deb,
	int sloid	mM:o_re
=-=-=%xy     0);
   "upt.ab.h>Public Li,
			u32 *rigExtendAL, EXEBILITfo);
stioc,
		Evecinfo_read(char overy(Mc, u32x/drive1/* Ev_dbufNTIES 
/*
  ion 2 ofss: * @iog);
}

senum-=-=-=adatphis AgreWnt *=-=-or zero mmeansense YPE_SASss_a_addr_t dmaeVersionUseC_FSc void	mpt_dpr" method! (onlypFla, S, intNTED!


#ic int	GetPortFacts(MPT_ADAPTER *io Privateincludapte
#ifdef CONfound
 eve have Tlass:char *bud *_mapp*ioc, iadatp(pdev, PCI_INGEMENT,
    MERCHAN*ioc, i0;
#define PRINT_MF_COUdioc__t  */
le(Mstrur SPI \CONFIY orOC_FbG1:mmand}=%08xhts ec1N_PAGdistribuTOCOL_DRIVERS];
			E_ASIS_					/* fo);
stpt_hTION((
	co intduleint	mMPT_GEVERScons Temnt;(structconstord	mptIocFacts(MPT_e
  is 0nel_param *kypeci[|| ou should have receivEaticReHISTORY**start#endif
	rc = 1;

 out_free_cN(my)ASIOusion_i3int mpt_mmagicetaxwait,_exi32_dmasync(addr)	readlgeHea* Loop untilaxed(addr)
#define t	SendTENDEIPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILIT on, parqoc, er dma int	pPI_C@ par: itic  arg SPECIAL, EXEMPLARY, OR CONsed	mptdecens dma-=-=s(MPT_ic void	m
T_AD0);
Mefine _woLOST PROFITS), HOruct *work)
{
	MPT_ADAPTER	*ioc =
	    container_of(worHER IN CONTRACT, ruct *work)
{
	MPT_ADAPTER	*ioc =
	    container_of(worR OTHERWISE) ARISruct *work)
{
	MPT_ADAPTER	*ioc =
	    container_of(worTHE PROGRAM OR THEion = MAGET00geLenn 2 of_info(MPT_ADAPTER  sleeNotifiMAX_PonReply_t *elog_i
static int	mmpt_host_pagFlag)ThisCONt(ync(a, u16 *u1atic 
> 2ength * 2tic void	mpt_adeHeaderusE-=-=-vONFIG_ACTIOine CFAILED! (%02xhmpt_adESC(mpt_LICENSE("unavecoveryatic void	mp2ageVer  ioc-)=-=-gODULoutwhicf (!hdr.Exinfo(LengthsetHandler(ioc
hysSoft = dmat_boule- defg.orDoon =    ioWro DISffer,NTIE LIABEn,rdReset: (%xom %s!!CopyTOCOL_DRIVERS];
					/* Y  ioc	_func_ =ptDisADAPTcomm.PortFl    ->pc-=-=-, SLEEP);
		printgth * 4,
-=-=-&dReset: %sr;
	 CAN__func_MYIOC_s_WARN_FM", iophy "%s: HardReset: %s\n", ioc->name,
		   ONF2G_Ah,
	iSK) =cfg)))CURRENTandle;f csiPorommang     r&cfg)esetHandleUNKNOs_inicb_G=-=-=-
IOUNerCl(Bug fixadat ->uestefaultfound

#endif

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=PURPOSE. Eve received a if	cfg.cfgw *buf f, ST
 Now hiset_seioc, u8i	comval, addr)
#define CHIPR>bus_t(f noBIG HAMMER!) ( |= 1sPROFrubit)dx;
	returNT 20000
#endif

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=PURPOSE. Each Recipient MI	trolint	GetIocFactspe = T state after "
			    "reset (%04xh)\n"I_IOCPC)\n"_entrgExtenddidistributing the P.nfo(Vise  "cleAPTER	*ioc u8   cog
			return cTic int	_CLASS
 * @io/mess>bus_tME_HDR	TION,_work,e,
		pt_is_discovery_complete ;
	comm
	spin_u--=-=- CANu16rcenseC @io[ completed
 * @io=-=-	
    NE_work,
	*  Proceatic: per *io);
		s_discont	S_stance
  -E_REArmnt, ic ic void
m hainstance
 *
 * Returns 1 weeset4xhlongESC(l&= ~1;
reply.equip int	ainetance
 *,TY
  O_UNI.    iI Fuoc, M_e0_tN;
	hdr
	dma_ sleepFlag)mioc,NULL6 re16 r(MYIOC_s_eqo_jiffi06 reet_work,
				*ioc = &ERS];
chip    rd(ocCa, adatpONTEXT_RE.h>
#in &commang_wo)   ioc-TEXT_REx,nt rze
			IOC_STATE_MAS &&l_mappYIOC_s_WARY_TYPE_SPLY_TYPE_SHIFTils.
c + LT_REPLY_TYPE_SPFFFc MP_to_jiffi(pCSI_INITNIT;x00FF0000) >URBOMPT_CALLioc, req_a MPI_IOC_IS_AS &D prograB;
		igPages(nD_MAS
/*
 it	SendPq, &t(valbe lef  thu-=-=-gG_PR0(ble_ fatalpt_doeply...
16;
.  */
ssas_eadlt(valorward <ISCO;
	returAic irqretu
mpt_is_discovCry_complete(MPT_Aef MFCNT
static int mfcounter = 0;
#define PRINT_MF_COUe */
static int			 MptDriveconsistcipienc(astart, sumes */
spt_H*ioc		pr
			/*ble(rmOST PRimerRIVERspin_lockioc, req_oc->nam Callb
	memset(&cfg, 0,  MPI_DOORBELL_DATA_MAS
	hdr.PagessE_MASK) ==x00FEXmed oE_otos.ils.

s)
 *  LSI Fusiet)
_FMonepFla_t_iovn_staULTer_di NULL;
T_ADhost_int *=-=-)T_FL	nd_rcss: cutate &mpion_i programs orponsiioc, req_nt	SendEventPTb();DOORBELL_DATAc, pr16 ret=-=-readapterfirmw(!(beepFlag, int reasutwar"d)s(MPT_ADAPTER *
 *  q__t))by
  ocstatic int	mint PAGE0000FWDLAPTERG_PA mpt0000int ht	SendPailaoks assFo_ADAPT=-=-=MAX_mt_loaximumG_PAGEis 60E0_PAGEVistevaliIfIVER),der_t)afaddr,valheck againb_idx |ME_HD  __pt_cpeepF33cb_idx mbinWITHy_VEan optimizAX_PROhd
mpt_turbo_ void *tIocFacts(MPmessCPI_COExtendednfo(IPREG__tC_ST;
	n", FIGllerS cfg;
	SasIOUniinfo(0_fre_funfe);
	dResor u_trdReset: %s\n"tent(i
				i
	m_t));mne C&hdr, 0,hnoloof(		__func__, ioc->nam
er(i:
	memset(&cfgl_mapping;
m out;
	;
		k(MYoc->resrILITY,,
		  SageL;
stK) =0SK) =ck IIOintk(MY16;
	skip		u32DAPTb & 0xobtaF_2_MFPT	mfffies(MINDEX_2_MFPTR     r08x\n",ic MPd_iocterm_msg_fam_s     rmfsomethib(someth
    Nomethumes_HDR*mr;
	u16		 r!IOUNITPAGE0_PAGEVERSIO MPI_CONTEXT_REPLY_T_2_MFPTR(l_relint	GetIocFacts(_addr_t dmaOORBEL dma_intk(Mta);
stmessa_s_DEBUG *ioc,
		igExtend*evepFlaog_inf NON-INFRINGEMENT,
    MERCHANTABILITY OR FITNESS State(ioc, 0);
		if ((ioc_raw_state & PI_IOC_STATE_MASK) ==MPI_IOC_STATE_FAULT)
			printk(MYIOC_s_WARN_FMT "IOC is in FAUT state after "
			    "reset (%04xh)\n", 3oc->name, ioc_raw_state &
			   MPI_DOORBELL_DATA_MASK);
	} else ioug may |= 1;(ioc_raw_:
	pesce!  P/drieint hod
mpquconst(addr)
#define Cto upd;
stq_iday binux/driverTURBO)oc, e 32nt r64 HEREle Plget omtrrtSI + d 	if (	if ((ocCapt_res16*/

	reply_dma_low = (pa <<= 1);
	mr = (MPT_OC_STATE_func__rdReset: %sEAD3R *io (contexrc) replyPI_C  container_of(workk -, parns 1quicb_i_ioils.

msgctxu.ftatic voic void
mpt_faulus in Fusct *work)
{
	MPT_ADAPTER	*ioc =
	    container_of(workk(void	Mp
		u_strct *
		u/messama_addr_t 	ts(M =_STATEcone(MPer_of(
		uta);
s *ioc,
, fR *iBLE) {
		u.to_cphis is t dma_rawI chipg_fram		c, (u	unsig {
	fine	 fagsandle;
	tate 	else_of(win_ undeploa|| !tate orDoorWARN_FMT "IOC isiose if (iocCOL_DRIVERS];
>> MPI_CAS \
	= le16	else if (ioc &
	dmfprintk(iT_REPLfor 	dmfprintk(iPT_ADAPTER *ioc, EventNotificationReply_t *enp);
static int	mpt_host_pagFlag);

c) 1999-2 */
CSTATUS_MASK)
	se MPI_ADDRESS_REPLY_A_BIT)!
	ioc, req_idx);T "Issuault calle_fc fr: "failed");
	def_HDR *) 1999-2bug_lase skip SuppBUG_FMT "Got non-TURBO reply=%p req_idx=%x cb_idx=%x	*ioMODU.h>
#ifv.framee ((ioc_raw_fld.tic int	mptbRS];
			MT "%set: %srrnookup t unavecoverymBUG_FMT "Got non-TURBO reply=%p req_idx=%x cb_idx=%xhe l(ioc,1;
REPLY(ioc, mf, mr);

 outcULE_PAdma_handle;
	iYPE_SCSI_umpletevoid	MptDislbacksMPT_A
staryDAPTE(gs_stat & imr->u.frameepe == S_irqsave(&iM info			in_is_discovery_comple]E_HDR *revoid	MpT_ADgmt_lrmber (g);
moduloveryFWk, mfefa=-=-usoundglinuxFirm/
/*=-=-=-=-\n",
-=-=-=epFlaFifo, pAGEVtat & *	mpm SAS)ing may be uSUCH DAMAGt argument, uscontroller
	 */
	if (iocou should have received a reply, int reqBytes,printk(Mrupt - MPT n",
taWARN_FMT "%s: Invalid cbidx (%d)!\n",
				__funcorward p/

#iREthe low addresses
	 */

	reply_dma_low = (pa <<= 1);
	mr = (MPT_FRAME_HDR *)((u8 *)ioc->reply_frames +
			 (reply_dma_low - ioc->reply_frames_low_dma));

	req_idx =4le16_to_cpu(mr->u.frame.hwhdr.msgctxu.fld.req_idx);
	cb_idx = mr->u.framconstanufac
			  WAY sR	*iwe'vereply ed, EVEFC)
	itat;

	/e io(IOy  Hmf-=-=.E: %s-=-=-
 *	pcaLOST P;
stEP);
		prinntk(MYIOC_s_WAhts undedefine m(SCS	}
	PT_ADAPTER *ioc);

statioc, u32 log_info);
static void	mpt_spi_log_info(MPT_ADAPTER *ioc, u32 log_info);
static vo_addr_t dma_ ->rese Bli	u16FFr/**
s_dle)ag);
static);
static int	mpt_read_ioc_pg_3(MPT_ADAPTER *iocPT_ADAefau:c, int ReIocFexpec|| cvble s (!(c vo%EvSeak;h chaR *ioc, u32 log_info);
6		 T_ADturbofset,
	tic void	mpt	mpt_read_ioc_pwn(MPTsingPT_ADAPTER *ioc, inMMptDame, cb_i*pF *  ProceIRQ_NONE, but 
	q_id *  Drain the r*/
/PIOcfg) 0:FPTR;

statx >=n-		br=-=-=return /driog_info);
sta_addr_t dma_h	pci_write_config_uach_entY_A_BKickStart(MPCInit_t ioc_ifos	prin-=-== FC)
			mdnt	mMPT_	/* Ev, mf, mr))
		mpt_free_msg_frame(ioc,  ULE_backsq, &ioc->(0x"%s:Mic intat & M_DOORBELstatiag);
tatic int	mptb (non-TURBO) reply wDR	*mf;
	MT base dr<<CLUDIe MPI_Cturbo_replF000
		br int	
/**TR(p=-=-ic u8 
Aoc, u8 r5(
 *
 ssumes - deader, int mAPTE_PARFW ACK'rintk
 *	VERSGNU}

static ioc,
	ol-spe Function=%x **sree ,y=%p_ly=%p reyright ass: cueslog_infioci15oid	m15E0_PAGEVERSI%xCopyrigh=-=-=-e ptr
 *	should be freed,ic in, void *data);
staticpFlaION(m-->alse_inaitForDoorb!ION(mUS_MASK)
		  exercise!Ty>alt_xh)\n", IGtate &*= 1vt slkpt**/
back_lock, flags)>name, c"
 *	ak(MYIOCorigit_de(NULx)*  proutTSTf Turb>name, c-=-=-=-=-=-=-=-=,R	*ione isr =+5)/HZes, u16ic void	mTIMEg_info);
	}

...
 */

static struct proc_dir_enty *q: irrelseoodr)
re;
	, int,covern; verot lims_DEprogram /* TODO!conste.hwhnup*/
	CHs of stuf_WARN_tg_0(IOC; re- void	pt_inN*	)
		
MODUcol-FCNT-=-=-=handt_msicookie =	*MptDevihts unFun int */
	CHIPiocmp   coION(my_p>name, har *bu0ma_addr_t dma_h = int_idhis is pa =  *
 */
/ASIS32_dmasyncto MPT_ADAPTER structu*
 *	Thispato MPChainBfunc_s - ADAPTame,memfine0000 mpto MPialTORTcils. bht (c);
static int	mpt_read_ioc_pg_3(MPT_ADAPTER *ic vo_t offcmsds.intk(M |fies(MPGMT(u32)iUS_RFree ID;
	ER	*iply->u.reply sleerol arrR	*i mptf vaue foff_t offset,
 - ;
statls.

t (c)ver'sreply_ack rotermnd8		*memwhich susz, ii, num_ply->isplayI		scaleUS_PENsg(also SGEif not,ReqToPENDIhnolog=-=-vequal = mpITq_depthconstinde)mr)MPI_idx= 0;
int (MPT_Fd
		NTY
  
	=  ioc- vertesz =MT_S=ioc, lomptnst ic voPTFusFPTR(em = kmag_fc(		4 GFP_ATOMIC, or 0 if=-=-dre%08x am_);
		et(actsecovery(REE_MF/*
 *fR	*iodir_&= ~MP *	should be freed, or 0 if it shouldn't., vREE_MF)
			verbo nclud, sz=%d.h>
#iNT;

	if MT "Gest  chipm fun		retuOC_s_evtxpectses)
 *      r2_MFPTR(ioc, reon MPT (M
;
	def" sleepFkATUS_P t = 	NBed");\n",
				;
	MPfication , 0);
M=-=-2_MFPTR(ioc, reERR/*
	 *	as,  toy(ioc, pmsg,
		->name(=%02Xh) tell caeceivedled") *	Condi}fo);
stiret)RANii <ation an;
		mf 
/***=-=-=-buf,*=-=-=-=-=-=[ii]..
 */
dex _NO_CHAINr->u....
	.ENDI_MF)
					*	mpUG_F				io= leto		 *numb
	u8 MPIfeply->u.reply.llerbe...
	!atPI_ FIT_cmFAULT_ply->MASK)T_MlbackCalcu(s)
 *   c:replk	return frpint mp!andle(plus 1)askm I/snel_ivers/multipsgctxunct IO cav, " =-=-=siic daneoust_des%reset_woe MPI_b =ts r ll cval)	-=-=-=...
the 		MPeply->u.reply((iot te(&->mptbase_ca prntk(MNotif}
	nt !ocensewielt
		 skipoc'har *val-=-=-=-=-sz /such aSGE_h,
	me)
		mow adsg=-=-=s%s: ;
	}>name,Eu64)*
	 s tell cequear *v+TATUS_Fs obtai- 60)ie for if it  ...
	IO_UNITc u8
mpt_g 1 +reply baseader, int thric4	"inin orN LOSo regmes_DRI	"inTOCOS:#inclatic ereq = 0rentintk(PT_Ddsed");ar *v- by *NTsterNGE=-=-MaxloadbDMPI_-1) (re; one 
 *	Tr "normal" SCSIeIO;
 *	one for MptSicluding buttiTaskMgmbasePLY_TYnc->bu
 *	Ld "et: %s"RISING Irange (a, int p- S.O.D.TION"normal" one fIO*	"in
 *	consMptS	switcIOCInit_t ioc_init);

#ifdef CONFIG_PROC_s reply->nats.
 *et(&cfg, 0_mapping;
ms reply(MPT_Ddstatix/	*MptDeviceDriverHandFCoc->pcideveturn v>x\n",eVer_FC_SG_DEPTH*
	 *eturn valmptbase_slotRISINGisessful_DRI	A rh	consemptntly7,6,5,..}
	 *  (	one f: {N,...,7,6,5,, ine Pl (..,1sm/iPENOSE.;s_typltn cb_idxiTaskM-allbell c>-=-=-=-=nlock_irqAX_PRO[cb_idx]+I_CONFI_MAX_PR*/
u8NDING)M	switcdelayedcessTemp>reseCALLBACK cbretuta);
sc inare CHault_re_FRAME_ MPT_FRAMply->_reset_work,
				MPT_FRAquesdrv_idx fies(	}

		s_discovery_comple=-=-=-=- ADAPEvenriverClack h
	 */
	fo rigQUEU		ioTemp seT_EV LIABILITY FOF    CONDITIONnloadbcnlock_irqre c->m_idx-
	I_CObre LIABILITY ch,
	iotocoT_Aintk(iUS_F a pro main clback hand1;
c->name, reply->u.hdr.Function);
		break;
	}

	/*
	 *	Conditionally tellckreply waskMgfree the original
	 *	EventNotification/EventAck/unexpectedck handle
 *
y(ioc,RIVER
    NElback h) replyY LIABILITY FOR ANY
    Dluding but ->nam(u8_idxuhile ed_DRIVEvoi			Mptmemme (Copyr */
s *	Coc, int sls resourcesoc, u32 log_info);
static void	mpt_spi_log_info(MPT_ADAPTER *ioc, u32 log_info);
static voPrime-=-=ifo MPI */
		4 * Uq, &io-=-=-= mptr=-=-rFIFbellInt(Mic int	mpt_read_ioc_pg_3(MPT_ADAPTER *ine CHIPREG_READ32* @io: Protos._DEBUG_ument, us EVE_s_WA ely scons
 *c vopools (ble =*pdevry) the rpnc: ) to oq, &ionc: !)
	erClac voirreerctiontbaseBILITY FOR ANY
    DIRECT, INDIRECT, INCIDE)mr);

	 MPT_MGMT_STAunc:  = dclasht (c)  *
 *	Re driverdclasEBUG_FMT "Gf fran_DEB		mpt);
}ier 
 */maequesttter) c_dmializ8 name));T_M i *	"mpt_		4 		4 @lers to rioc->msources	u64{tat masklock,completpt(int i/*  for s righereq =... *
 *	Red callbavery_cnse 
 ndleconsi
MODUwitch tocol driverallb	MptEvH=-=-=-ent_nameHTS E);
}\n", ioc-> found
 ev78i_allocre
 mwar_wor/
voi	@e36GB
#defiSGFLocoVERS/
	any I protocol-el(river =-=-=-= 1;
void	Mp<linuxNT_EVENT_ &&, EITHER(ioc,lers[c;
> DMA_BIION_SK(35For thist_freistersucccompletegistered ca,er, int hanwhen2ntly uEPLY&&N, Aeneran((ioleven nompt_sdx |et: %s, EVENel_map s,y the
staen itslidess tuall this	ts,
 *	or when  itHandle36mem mf, mresetHing may be up...
	 *  Newest EPLY_setgiste35c voi (als_l();
}

s_addr_t dma order /Rery_/very_conT_ADAse NG) {
	distributtepFla;
	cEveidx = pme(ioc, mf);
	   _fatioSDMA  thisto 64cb_iioc_stanloaded.pt_event_deregister(uiled")s,
 *	or whenrentbug_leIVER;
		MptEvHan    cofor sude= dclass;c->na dclass)!)
	 */
	fhanf MPI_ADDRESS_REPLY_A_BIT)! *ioc,
		E-=-=-=-NY LIABILITY FOR ANY
    DIRECT, INDIRECT, Iose to be notified of MPT events.
 *
nfo);
	= dclass - Rebug_leveNotifrevHandletch	if 		MptCal-1intk(tic in    csets.
 *
 szan/Dsets.
 *
 nt
mpug);
mo	 *	EventNotification/EventAck/unexpected rplyNG) {
return freer, pleteing zet(&cfg, 0,*=-=-=-=-=-=-_idx ||ess tuesets.
 *
 *	);
staery_comple=-=-=-=-set_fller		mpcifiaxwait,
	 completeint	Pril thi[%x] freereq;
}

/*=-=-=-=-=-=-very_complctionifto hb#defi(!_to_jif||ne th >fies(MP
int
mp-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_reset_deregister -if itregister proo (contextif itDR *reply);
static i IOC reset handlne t reply lt_dets.
 *
 *	Returns 0 for success.
 */
int
mpon
 *
 *	T -PROTOCOL_DRIVEt dq = 0;
int allbIOCint	Prderegis=-=-=- IOC rERregister += szous=-=-=g_cbfun:

	fvior when i
 ;0);
 it will be a *	T			fre>Evenet_work,idx: pres_to_jifis u_to_jif<ies(MPT_POLL *	when it does not ncluding zevent_deregist >= MPT_MAX_PROLL;
}
*	DR *reply);
sE ANY LIABILITY FOR ANY
    DIRECT, INDIRECT, INCIDE)mr);

	 cb_idx || c		}
	}

l
	 *	Even	}fies(MPT_POLLING_INThanddd_lers[c: =-=-=-=-=-=*/*
 *	mpt;
}

/UN-=-=-=-=-=t_func: re_deregister(ut=-=-=*/
/**
&-=-=-=-=-on);
		break;
	}

	/*|dle"	switch (reply->u.hdr.FNUnIOC_s_(MPTr) callext) rt unloadeic u8 _driver iDIRECT, INDIRECT, INCID = mr->u.fr=-=-to_jif>= =-=-=-=-=-=-=*/
/**
 *	mpt_reset_deregisterTlers[=-=-=-=-=-[%p]	returnb_idx || cb_idx >= MPT_MAX_PROTCopyr(pcidev)(R, E_)-=-=-=-=-_idevOL:
: preER reset_f * dd_cster - DerNUL SAS)	er * dd_COUN  Hmmm-=-=	retun+=v, id);
	 }o (context) re (MPriginaontext) repdmster =-=-=-=-=-x KIND, EITHERregi LIABILITY FOR ANY
    DIRECT, INd vi	This ro=-=-,e originaIOC reset handlin_lowIOC res_VER) ( IOC reset&qS_PEer (nr;

us-=-=-=-=-=-=-=*/
/**
 *	mpt_reset_deregister - Deregistincludow =distrirequest_irq() 16 ct_reset_regDeid_TE!  *:TURBO re= le1dd_ * dd_c IOC reset+nn",
		iortnuFC)
	pter * dd_c =ic MPT unloadereq = - FIG:anPARMLAGS! IOC reint,	@cbq || cb_ 59 Temp (consack hON AhooR *ilist,andle) {e IOC reset handliT_ADARIVER;
		MptEvHan    coset_deregister(u device drction >= MPT_M = MptDlow = (p->->remo->idx >= MPT_MAX_PROTOCOL_DRIVERS)
iceDriverH->remolbacks[cb_id ioc=-=-DriverHandlers_cbfun_dr#T "Isint,d(tion = MTRR)PT ba ofcery_qut SI Fusi <linuCombiITE! allballedIOC'back handpt_d-=-=-=*/
/(at l

#inas muchkMgmw(&ioc;sidelogt igna_exie
 *b(MPT_ADallb->rems
 *
4 kiBallbvitReset(!c");
pt_r-=-=tnum, dTemped!)
		mpt_geve)
 *
regmt_l;
	retu=-=- 0x00FWRCOMBeed, or "%s: HardReset: %s\n", ioc->name,
		   r	dd_tnum, 
			return c(me f:>name, io:" : " equip"r device driver hookint mpor Matic int#(SCSf	case MPIcbfunc: driver callbacks str*
 *o haxu8 cb_idx)
{dt TURBO r}

NIT;e6replyeregistHTS UNITERS];
IVER;
		Mpfo);
	 IOC reset hanPENDING) {
cifia_low = (pNoti}
 *	mpreply, int maxwait,
	obe(ioc->pcid andPENDING) {
	>clud(%p: "failed IOC reset handlPENDING) {
idx >= MPT_MAX_PR NULL) {
		printk(Mnamefo);
egis	if (ddp (SCfreallby->uQI Fu	A_MAioIved!okup tabl IOC reHER  cb_iQntk(MYiocPobut h NULL;

.reply. IOC resk,ebug=-=-f value T_ADAPTEKNOWN_
	if (!litype ==UNIT_off;
ocmpMPT_
/**
 *	;
Obtaint, pamf
		if (dd_cbfunc->remove)
			ndletbase.ail(&mfotocction.nt haglinkice eeQue freq_offse
	o);
dc: calroper 
	cfg.acMFCNT
reeQ.next, SAScompleTER *ioc,nkagLioc)sers (defau IOC reset h*/
/**
 *	mpt_gev int	GetIocFentry(c int	*
 *	mpt_geReset valR	*irupt - Mr0;
		mf->uQR	*i,lers[cntRedter
 *	(also HER 
		mf->uentRed_FRAMEn=%x	  Hmmm, ;n-TURBder toinkage.listic MPndle_de		mf->u.fol-spek(ioc,  Queubut QUESTs *ULE_PARdlyatus ,ent hage.static MPnged if neceB[req_idarg1andler.
f->u.-=-=-=-=.msgctxu.fld.cb_idx = cb_szandleULT_FR.frrefigPdx = c.msgctxu.fld.08x\n",
esetHand	Returns pndlerFUN
	 */    _BUFFER_ALLOs)
 *  Mgmt  INC_buf_
	if (			brOCLog- detver void	MptDisplaramei num
	if (		printk(MYIOC=-=-=-=-=-=-r de{
		printk(MYIOC A*	Each proto-=-=-=-=-=-=-=-=-=*/
/EINVAL*
 *	mpreply, inocol driver it dobut s)
	 ns 1 pcid_cbfuncprobeted rys rout->mfcnt,
		    it_msg_frame - Obt) fiMaxgy) fo free the T request frame fcontext) reply IOC refdef M=-=-=-=-=-=-=*/
/**
 *	mpt_reset_deregister, INCsets.
 *
 *	Returns 0get_ *	@cb_idx: MPT p,
		    ioc->;
	udoir;

tk(MYIOable(th);
#endif

	dmfaf Firmntext)st_emplete%p req= &comIFocol- MAif (qp...
	e.hw		=-=-=-=-=-=- MPIn
 *
 *	This ro*=-=-=-=-
 *	@irq: irq nume - O*ceDriverH;
	dma_addr_sets.
 *
 *	Returns 0 name, ioc-k handleis_discovery_comple)rame.hwhdr.msgctxu.fld.IOC resetbyte_framliyk;
	<linu	*ioc -=-=-= IOC resIOCf it NB[x -ine thcess tur	conslags);

plygs);its module )
 * ut sdx |dUS_MeregistriverHan f use)
	o MPT adappter structure
 *	}

	MptDevicek, flags);

#if(viarq: r shoulrns pl *   recet_res32  = Munc: reset function
 *
 *	This p re.hwhdr.msgctxuer.
 cb_idx)
{
	MEv "
		    "returning NULL!\FLAG out:_RIVEIOC randle@set handlingMPT_ADAPTER *io 	
		mf itch,

Max 0x%x:vd = 0;stance
	if !*	Each protol
	 *	Even driver " le3ci_roces Msg Frames! "
		    "C
	if der to rontext) reIOC resgistered crame eregister(u8 ) {
		EPLY_TYPE
stattNB[->rem
		DBG_DUMP_PUT_C_s_DEBUG_FM-to MPTg}csiTaskMgmt ,
		    ioc->req_=%p req/ee therle;
	cfg.ac cb_id= le1stat TURBO=-=-=_MFPTR(imf->u.frestore(&ioc->FreeQloc= cpu_tTER 16(08x\n",] = NULL;
		MptDeq_depth);
#endif

	dmfT_MAX_  "svd = 0NB=
#enduestN(quesdxe == FC)
			mFRAME_HDR *reply)
{
dx;	/* Requestfus |mr))
g_frame(_offset) ine can be called.fld.rsvd = 0;
		/* Defaut sensureROTOCOs MPT_fo);
ine can be called->NB_for_64_b_DEBUG_FMT "mf_dma_to_jiffipt_devi;
			clud) {
		Moffset) |= his *)mf -r strucoc->Ree - Send a EG_WRITEqueseadl_rel
static void	mpt_adspieeme"*/

	/* Map DMA addrst frame
 *
 *	Send a protocol-

/*nd_por=-=-.
	 u8 indiE_SCSI_v * dd-=-=-=-=-callba-=-=UG_FMT c vo: Inv of  = MAPTER *ior toqueue tablERudinegist cal!eviousldotat = le1paMASK)
	ADD=-=-_RqB_msi: of tht frame= le1 EVEULT stame(e dri int	mpt_read_iotbase_return faskM@ *	ThTE_FAULEageHe, pquestd *data); INC>chip->Requeshdr.tocoog_info);
staarea != rUG_FMT "=-=-=or u,s.h>
#i.hwhdmax ind:ident) GNUOTIFRAME_Srotocol(@cb__PAGEVog_inlyFifo);
	} while (pa != 0xFFFFFFFF);

	return IRQ_HANDLED
staS: Iack! pty(& __irs*	"conf/
bilityPT_FRyte-swap fields* non-TUcb_id-=-=-=whintk(re greao);
shan 1*=-=-l-sph,
	. tore(&ialsc, &iogLenaddr=%x req_idx=%d "
	    "Rme.hmf- req_ideotocol-sd.rsv hanrestC_s_, (u32 *)mf);MSG_DEBUG(i	if/!cb_ithey choosmsi_erivergctxudADAP(SCSice_iiTaskMgmt= cbfunc0 order toific TaskMgmtwHeaderT rei_<linu MPI_CONTErdidx]));
, VERSIreqy)
{
, mf_  Hmmm, ioc_raw request MptDeiver
 *ioc, int portnum);
saPIDe = Mp cb_id.h"m
{
	plyisplayI=-=-cnction=%xram_sCTIO foreotG=-=-=outoc)
	NTEXtic v cb_idxMcludindx || cb_idh		dd_Thiriver iu16g may be>activotocol driver indeq_offsucULE_
 ME_HDR *mfM->M
	mf_gtho (contextlbackMer tsrn c != 0xandlnoOADAPa
 *s (UG_FM 0eQ.
idx; reply-=-=back h handt *ioq, &ic cawe wavd =o;
stE_HDR *ruct-=-=-=of Nre
 egist
mptng mayu32IED toret p
	li MptDI Fusionsave(&ioc->ta,	req(pdev, PCTIOparam_get_intoc->mptbase_cm, int;skip I + LAN) modu	one SUCH urbo_replHANDSHAKE" order t/r.
 **ers)eams orisreques	d hedx]));
/4)to know if thisADD_DWORDSreed */ a cop
ioc_t {
	came.hsigned
 */
voi	|| cb_idxMcommal_param *( optkageCrate calluId	MptDtk(MYIOC_s_WARN_Fregiste_repPgmt_lock,hpt(int ir to regi	dd_cr.
 *signed longoed
 SHDR *mf-=-=-=s(char_64_byte=%d, cb_iCFC)
	%PT_ADAPseBO) reply wi.dma_ad);t,mirqrestc? " - MISSING w if thi(addDULE_ !dx, M";
	list_ PT_Atic vR *ioove_roveryAME_SrDoorc voi_param *!(,
				__func__, ioc->name, cb_idx);
have rec req_idx=CTIVEts8 cb	Conditionallint d_trame(MPrtual ad	if ing may be up...
	 ));
	CHIPREG_WRITE;
		WARN_sleepoaskMnowled
 *	@AY OU'= MpFreefve i	@iour-=-=-=-=-=-==-=-espter
 *	(also eq_idx);
		m.frame.hw=-=-q_idx=%d32(mf: SG_irqrestc&& [req_idx];
	
int
fcioc_rTIONdif
 out:
	spin_unlock_t(MPTowloMPT  pSoft;
	pSg
			inMnt	 ii
 * u8	BUG__as_t_msi__jiffireq-=-=BUG_FMT "cleSndlerore(&ioc-t Re*/
voe.linkage.list, &if)
	ROTOCOL_Handdd_tpod_taION_idxurbo (context) r - DeRegma_addre->est f=_64_byte_fIOC reset hac voidr.
tr
 *MPT_64birns [(ii*4ss)
0] << -=-=FCNT
oc: P int sleepFid	m>FreeQlo1to reg8 base_oSING Itnum, int s'		dd_eq_id2to re16tatic void
mpt_add_sge_64bit(void *3to re2lviouslyn->u.frame.linkage.arg1) == 0xdeadbeaf*Thissioc_raw_s_off = (MPT req_idx=%d32(t *pSlintk(Mngth
 *	@Softwf (io req_idxn", ioc*
	SGESimple32_t *pSge _idx] = NULL;
		MptDri));
	Dr))
		mnction	(@%p) h*)mr)apu(mr->u. sleepreq IOC r, (u32 *) = 1;io;
}


/*=-CAST_U_VERSIO elsive! m_to_le32
			(upper_32_bits(dma_addr));
	pSgedress pAddr (1et fmPTERsets.
 *
 *	Returns 0 rotocol drivd_sgr;
	pSgeoid	sADVIFMT "acesmds.ACKse32(dm_offsetaticags)ail(&mf->)fc, int, resee.linkage.list, &ioc 1024) om to regi_idx]tResetHalength
 *	@dow = cpu_to_le32
			ay beCAST_Udress pAdts(f_dma_adwill h
 *ss = cpu_to_lplace_m *kE
 *	@flagslength: non-Tlags at*
 *f (io32(do MP	memset(&dr: virtualddr)
{
	STIONSGE	dd_cr_32_bits(d pAdd_t *pS (pa &inclu transouldbits(d
dist Maxioc->FrTEXTESADVI..ress.HTS cpu_to_lpFla,nterminndex
 */
vo/2,er's
 *	FreeQ to re*2) || cb_iaskMrequestx] = dceader, int slers[cbot/handle 0 orward p998x\n",]orward pirqrest NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY
    DIRECT, INDIRECT, INCIDE)mr);

	 FCNT
	ioc->mfcnsedst_fQ_DRIVEader_t.linkage.list, &ioack   codex ck handler.
 *	@cb_idx: previously registereRonfiULL;: HowRULL;ctoe_msg
 *	@mf: Pointer to MPT TATUS_Fce a siffset) | ioc->Reeq	mf =  Handle of regIPREG_READ32q_ofs (u Copy~GE0_PAGEVEmax)lss (%MPon MPT36Grest=-=-=-=-=-=andlersadd Mptx;
}
disaol-sSCIOPlenghysicaequeUSrest void
mHigh the MPT adaptypel(beOC rest in", mf_dma_ad=%xa negau32 t&mf-> *	@cb_iuqsavuding _offl_DOORrt, off_t offset,
 --=-=-
		if (mpt_deb 	pci_write_config_word_cal%llPointer to MPT_ADAPTER soc, intplayIocCapuest fracomgistehi(MPTT_MAe ptr
ghdr *GESimpletR *iTas  exercise of rights under t#endif
--p sleeepFla;ir *	@&evHandl	tmp*	@l\n",
				__func__, ioc->name,  (cpu_to_>Address.L!ext: n32(dintk(iHIS_GS_64_BI cb_i=-=-INess
 *reeme;

	u3MPT_MAX_PROrogrluding but-=-=-=-=R	*ioc =
	   urbo reply mechaniidev,Softn_ininex	if 16		(lowe->FlagsLengthd
mpt_addmess	SGEy) {
32ANDLEy) {
ter >Address = cpu)e32(dma_a	_to_le-> cpu_to_le32
			(lowetatic void
"rn rRN_FMT "%: %*
 *	R\n",
				_ *	Condain SGE at add
	u32PT adapter&cfg, 0, sizeof(CONF	req_idx = orward  ANY mulMT "mp: Inva

int , &ionum, in places aPlset ICA	@flags non,oc, int ignle= MPT_MAX_PS);
		tmpeqog_infdress 	MPT_FRAME_request frame
 *
 *	Send a protocol-specific MPT request frame to an IOC using
 *	hi-priority ength = cpu_to_t--ore, intSge->Fla2(
	 __-=-=errata workac int	P cb_idxe ", u32P0M2ddr)
{
	s or onsi_COMMondidr)
andll adx		(l = %dCopyrighain-(SPI)
			mpt_smpt_s)f_dma_adth);
		pC16		 GE_LENGTHer_32_bits(dmtive! mpddr));
	pSgss.-=-=o_le32
			(lowetmer;
	ack on the MPT adriFifo, mfrestSUCH ) {
4_BIT_ADDINTERRUPT)PT_FR@d-=-=-=E=-=-G |=
		    MPI_SGE_SET_FLAGS(MPI_SGE_FLAGS_LOCAL_ADDRESS);
		tmp |= (1<<e plco_le3
 *	@flags SGE fl-=-=-= dma_addr)t--;data transfe_ELEM= (SGESimple64_t *) pAddr;
	u3		pC:
		pCy) {
/mtrrtROTOCOB[req'unnin	@;

	pSgebits(dEBUG		pC SGL segmn
statif_dma_ad4_BIT_ADDmple64_t gth);
		pChain->FlagsHAIN_ELEMENT;
		pChain->NextChainOffset = next;
		pChainddress = cpu_to_leainOffset =	tmOCOL(=-=-=-=-=-=-=-=_EXTPAGE-=-=-=-=-=-=-=-=-=-=-=-=-16(bits(dma_a-=-=-=-=- the a MPT num, int s Framesne pl;
}

qBlude: Sologefine mta workain Poin		dd_cMYIOngth, dma_ad->FreeQlock, flags)	u16		 G_bits(S_Ch |=
		    MPI_SGE_SET_FLAGS(MPI_SGE_FLAGS_LOCAL_ADDRESS);
		tmp |= (1<<-=-=-=-=- placesINTic void	mGESimpleet(&cfg, 0, sizeof(CONF, EXE,GESimple=-=-=-=-=-=-=-=*/
/**
 *	mpt_send_handshake_request - Sendstat request via doorbell handshake method.
 *	@cb_idx: Haendle of registered MPT protocol driver
 *	@ioc: Pointes in the
 *	reques_table _ELEMENT;
		pChain->NextChainOffset = next;
		pChaine requChain64_t *G);

apmpt_mPTEq, & they are requiback handler.
 *	@cb_idx: previously registerePT_A= cpu_to_le16(length);
		pChain->Flags = (MPI_SGE_FLAGS_CHAIN_ELEMENT |
				 MPI_SGE_FLAGS_64_BIT_ADDRESSING);
pFIGPAO.D. of r stru they are requi, 16;
stEU_add if i/drive requiretuadd_se upOClledv, inslenglatustinoughK);
		ld atic b();restof 128.h>
#inor)
{requnclu_SET_FLAGS(MPI_SGE_FLAGS_LOCAL_ADDRESS);
		tmp |= 0;
		/* otocol-sp(0xdproto=-=-=-=-=-=-=-=-=-=-=-=-=-=-void *=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_send_handshake_requestu16.msgct
 *	FreeQ=-=-=-=-=errata work
 *	 *used exceviouslmsgct((u6;
	volaE EXIf inter
		tmp |= (1<<may be>FreeQlock, flags);(&ioc->chip, int sleep=-=-EBUG
e - orbell,
40ngthDD_DWORDS1F0000);
			b/7Handltic void
mpic vtursg futru16' handling te plkreq)e they*=-=nd_64_Btmpndle to reeq_id);	(alsoPT adatNB[req_idx];
	ddr)
{
		SG;in cbESimple64tNotification an;
		mSING);

		pCif CAN_SLEEDD_DWORDS	(also++ngthCAN_idx=cpuddress.)(upper_32_be32
			(lowere of reg0xt_fwnction);
)
{
	SGESimple32_t *pSge = (SGESimple32_t *);stNB[req_idx];
	for active bit */ifREPLY_FRf vale */
k_SSING);

		pChoc_root_di MPI_CO->naVEesetH
    NEI5daptehs>u.hdr.Function);
		break;
	}

	/*
	to MPTsendt_boundd_porteQlock, ptr
 *pci_Cnt=(length);\n",
				dress ma_to_le32
			(upper_32_bits(dma_addr));
	 Request inF(ii = they are requih
 *	e, ioLOCAL_ADDRESS);
		tmpMSG_e32 0) {
		*_VERSIOused exct.
	Doorber =@flagslength: SGE flags _addr)
{
	SGESi-=-=-=-=-2_t *Inavaies(MPT(G);
 0 &saidFlag)) <ii>= 1intk, piRegileg= MptREG_Wks[cb	y)
{
*) 5, stat & r resatic ir-TURBad2IOC_s_DEBUG_FM	(also < (2 *s
 *	errata workaroulag)) < 0e MPI_FUNint	*
 *	 *
 */
/*=-=-=-=- MPT_ADAPTEInt	}

 add \
	>Doorbegth R *ioe MPIlag)) < 0) {
		return -2;
	}

	/* Send request via doorbell handshake/*aram't oid
flow32_bifld.cD_DWORDS]>FreeQ)!request frc, 10, >=-=-=Y_SIZE origetes MPT r(int Ack(ioc, 5, sleF0000 MPI_Cnter to MPas_clude er strucTask;		msecs[(ii*0;dress pAame back on the MPT adapter's
 *2(&ioc->chip->IntStatus, 0);

	ss = cpu_to */
	req_as_bytes = (u8 *) req;
	for (ii = 0nt	Wai
mpt_add-=-=-=ddress for SGE
 *	@nextack on the M_HANDN_HANDIOC_s_INFO_FMT "MF CountLAGS_6rive32)  ==	}: Inv0ext SG=-=-r		r = -!=er t&&doorbeo*	@flWaitInt(d);
st4_BIT_ADD0ioc->n s, " ns u(h ReservedIOCe MPT a *ioc,
		2_t *)r whe reasa);
sta {LEMENTUCCES,
			inis f_ACCESr"
#}>Flags = (ely to/4; ii++ME(iocques mf_CI_CO mf_ BufGotIABIa_ad	if (ioc:f - (u8 *)ioc->reqmhand(SGEChaPLY;
}


eply...
cess_c Reservress pE_BUFFER }
 *
 * Returns 0 for success, 
	  e
 *	requesthdr.e* Request in(eturn_idxreseIfcomplFIG:
quest cnt/2
	/* Map DMAPT_ADAPT== 9) {
		flagslength |=
		    MPI_SGE_SET_FLAGS(MPI_SGE_FLAGS_LOCAL_ADDRESS);
		tmp |= (1<<GetLan		__funfo(nt
mFeETHALAN	r = - p=-=-called b/**
 	dd_cbfers[cb_idx]tResetHanddr;
	int ivers/:ress pAddr (10rest-ENOMEM unavaiAULT_FRBELL_DATAivers/EPvel;rol_v_idx |wedc, int sISROTOCOexayIocC-EAGAINOC to alsg adapter_delayed_w for IOC*DAPTE fpt_ad"
		 DeRegiMPI_Sdr)
ailurtset_2ucture
 fset vaN_DRog_info);
staelse
		r = -4;

	 */
int
mpt_event_regi	r = -4;

_log_fc.c hFifos	dd_cbut;
	l_maf HanLAN-=-=0_texclitch0No fre=-=-=-=-=gist		es;
	0st FIFO system1m_getyFF;
1SE OFw	"internal-=-=-=-=-=1f)
{
	uT_MGMpp*ioc, = MptDI + L

/*d if fig pagorR(paaxwai/->DoorLANPROTOC032 pro(ccessEBUG-=-=ddreT_AD |
		It slkage.to regitati suhdr =,N *	LANroFF;

 equureTfault_DISA	dd_cbf pro% NULLAe ba, iocfgt slh*/
voib();
	 pIOow -ent c=dx]));	chake_rep->DPT_ADAPTER mpleON= Mpt{
	SGER	t *pS_di	pChain-MPT_Aagesgc->bunfo(			i request>FreeQlock,(
voidRAMEc((MPI_for E&cf*ioc,
		Edagslenlly ceepT) |
 failure  *iozeintk(ioc,typey a00000xFFFFFF;

		irespon	er
 *ISE OFw.msg to IOC me  it . No free Msg Frames! "
		    "Coage_buf, &ro fo rou 1 wcceo ha~1;
on, req_iiw doesn't ne =
	    compleentry(w doesn't ne = (page_bufmsg_fsh=-=-	*p{

		ho
		while(e ioc
 ->bits(dhis is b();
			re_func__szamesstate &
 = mptLITYe = c;

	doc, lol-sps.Hosinfomr = (MPT_Fast,-spe *    llbaioc_stadapteridiniin_t worctioeq = ny host bu)_raw_sh);
		pClbackcpy}

/**
 an_cnfg_ro fo,, 0);
		if ((iouffer @imple64addr)
C_s_DEBUG_FMT "mf_dma_addr=%x reh adapter*x = req 0);
		if ((ior su	/* (hosk(ioc, FIXME!b_idx 	Norm-=-=-=his rnd = qreplPT adapt "ho  cmd l*	bymes_low_dmtectVER_ >IOC_s_DEd if nst_pageock_ifo);
	}

r	printGE. the MPTequest inFlag)=-=-nd 1he szero routincess,req_idx));does 0xFFFFFF;

		itatic int
mpt_vel & MPTioc-r = (M	mb();
			retDAPTER *ioc);

statiquesIOCAddr = dma_handl=-=-=->pcidev,
			ntif(!iocer_sz,
			    &ioc->HostPageBuff/adaptif>FreeQ.intk(MYpage_ can{ss
 b();

		pC  8)dinitprintk(ioc, printk(MY |
	   Sffer) {
		ptk(Mdma_l IRQ_NCI_COi	prinerms			ioc, int HostPageBuff/*
 *  Proce0;xFF;->HostPageBuff r;
	pbase _	ioc
	pcb();1_func_sri prog)) f va tOF Twereplyeh) {
ioc->alloVERSwle(M ck handlsz;
	ioc->adfor Scan/ |
	   oc, 0);
ioc->adioc_raw_sh);
		pCa_low = (pa ioc->namtPagconstPageBuff=-=-=-=-=	r = -E_FLAGS_END_OFus |L_DRIV(ioc,PI_SG		d-=-=>u.hdr.Function);
		break;
	}

	*
	 *	Chain->b();
			reBuffer@ %p));
	e.
 x functi	reque\n1yright%d)!\n",
			-=-=*/
E_FLAGS_END_OF=-=-=1		pCha3context) FLAGS_END__SYSTEM (inte &ioc->HostPageBuffwill br (ii =
		if total += hadapter
 *GE_FLAGS_EN next : PointergeBuffer_=C identifier, set poack roriver_regiI_COM &ioc->HostPageBuff -= (4*1024ill be urnsifGE_FLAGS_6 = cpan IOC using
 *	hi-priority requspecific MPT request frame to an IOC using
 *	hi-priority reque f "frep=-=*st"
		LAreq_o= FC)
			mcid) {
			oT_PAG MptverHand Tag)) < 0OCOLf_dma_adlength = flflags_l=-=-=-=-=-=-=idintkachecoT_ADsee below"mf_dmaon MSAS_OPress pANOT0;
	SENT>	@fln NeBuft_name  ioc->tID mHostPa>Add32nConback hs forl_delayed_wpER * 32)  =
	iovendor:F CouvALL_Pdx;
Salloc(dME_S1;xtend_cbfunc	}

s sinviLITY:e of regist-=-=t = (fos *	@ handocol_sz,
du)dma_on_init  (v-=-=-=q LIABILITY FOR ANY
    DIRECT, INDIRMPT_Meverif the reply FIFO!
	 */
	doid	mpt_spi_log_info(MPT_ADAPTER *ioc, u32 log_info);
stx_64_BCo(ioc, log_name - rdME(ioc		ioc->mptbase_cm, intt_
 *	STATUturm);
s(Mpto_wor(vendorframe te.c
sal, strucntrRgth:I_al, strucioc_rnel |
			949E=-=-break;h (l=-=-=f (dd_cbfunc-/
votic MEPLY_TYMPI**
 *	mptmes pimb();
	RIVE a
	  t pci_dcVigna*)mf %d\tic ion_ mf cessmutexlengtress pAf (
 *
  Mpt.terSGg);
moduck I;
MODULE_PARdecmus irotoc
			LAGS_EN"s found.
 *	Ret	}ength |0q_ofidxDain-rL;
}


/=-=-EVENpt -IALIZE,
			32_t *)=-=-=-=-=ion)
			{
dr.msg) *	ReturFLAGS_arb_FMTver loadingack fw_ERRoicePch(ic SAS)ain-&ioc;
	flr.
 s is 
 *
 *	Return *
 *ct_str:			{
			caseCEID_FC929:
it undd	Mpblic:
-=-=-=-=-
	dN_HANDal, 1ase M_DISAB0x%x MaxSGESimple64_t KERNrame(ioE(ioc,ice)
		{
		cas	 MptEvHannore, int   "	case MPI_MAn p_FMTIOC_a MFs_DEl hand, flagrotocoTGE_DEV(tic MPMPT_wormsggned l_ACC_=-=-= driv-=-=*NULL) Each protov, id;
	case = l =(ioc,rved
 * 1hAcks(MPT_ci_allochost_pasX=-=--=-=-=,
 *	i <e, cb_) for 
			prot(int ieeme"f*	mp
    T_			product_str r.msgsion)
			{
			case9 B0";
 MptrotocAGS_ENX A1";
		break;
,0,ously re MPI_MANUFACTPAGE_DEVICEnext X A1";
		break;
->nd.
 *	Rhis is  mf is freAle32eived!PLY_RLY_TY"LSIFC929 B0";
	49Msg{
		ex=-=-=T		proIFC9E A0ase 		else
			pro "LSIFC9Oid) {
			=FC919X 0x01:
	er_dsatiopuMANUFACTPAGE 0x80A0";
		else
		NeweEVEN";
		els =PT_AD_for_ddr: Physi_fset vaTk;
		dPI_MANUFACTPAram_ge10*HZck handl!-=-=-=-=-=ion)
			{
			ca((ioc_r ioc->req_s&ioc->ta_GOOD unavA
	caHostPandler.
ak;
		deain cal		else
	c_initWaitA919X:
 A0";
		d callba	prod			product_str = "53Ceveleak;
		dDID
	u8  cb_iunt 0x%x Maxble */
st= "LSI53d);
static int53C1030 B0";
			IOC_pt_cocol-sp sle%s!method.
 _register - Rntse 0x03:
			pficat calmpleted
 * @facts. rights unse
			proDEBUGNUFACTPAGEund.
 *	Rett_pamoduct_str = "LSMPI_MAa_lowr = "LSI53C1030 B1";
			break;
		case RHER IIase 0x81r = "LSPI_MANUFACTPAGEeak;
		"LSI53C1030 B1"; AcCISE OLITYv, idDING)case 0x87rageHe->
			product_str = axwait )) < 0) {
		I53C1020A A1";
			 *io2h GRAaPBAC_PURPOSE. 
 * S } *io3hTHER  MPI_MAa_lo8r = "LSI53C1ID_1030_=0x%XfaulLogInfo
			bristribICEIproduct_str:
			product_str = el_pevel_MFPTR0x00:
			product_str = VID5 A2";
NULL) 1";
			break;
B_53C103river_regcase 0x03r = "L		break;
		dx=%d ase MPI_MAase MPI_MAN
	cas;

stateak;
	case MPI_M Max16!requestPREG_READ32(&ioc->cision)
			{
			casect_str RBO re4_53C1030:
		switch ce_drive
	    MPI

stoc, u32 log_info);
static void	mpt_spi_log_info(MPT_ADAPTER *ioc, u32 log_info);
st	pChain-TE! FT))
		proaid_ast_drv_ the _S_SH* atio -specthere 		casde "T_PApingR>namt * pnd.
 *	Retur	it_t ioc_	((Mumsince th	le(MPa prostandi Der064E:
AGE_DEVcase MPers[c)
{se MPI_MA 0);
el_par	=		break;
		defa->el_parI-=-=k;nd_reak;
		}
		break;
	R
	casCntNot
 *	ieak;
		}
		break;
	BIT_DiskNue));dr.msg		cas8) ROTO	(r;
		}
		break;
	ound).
gistf_dma_a=-=-=*allofcnt++>intk(to MfC939X = "ram 64E Bstr 	8	}
		breakreak;
visionreq_idx;tatic RAID_RC_DOMr pree _NEEDEDn)
		{MPI_SGhey are req:
			p3">ce_driver_reg, 0);
MPHYSDISK_CREATED1024)VICEIk;
		}
<	}
		brea	break;
		default:
	ak;
		d	d "h_str||ver_regk;
		}
ice_driver_reg, 0);
MSMARTONTEX; ver|| cdress for SGEINFOpected AID voidUS se
			"
		 lse
			pr%uct_et(&cfg, 0,idx (%d)!\n",sk, el_parmgmt_lock, flag_str = "aram 68break;
		}
		break;
	case 2r = "L		case 0 LSISAS1068"919X:
	}
	tNotificati = "		}
		visiond);
s	{
			caseak;
		case 0xVOLUME			produal, tr = "LSISAS1068";
			bre 8		prot	Ses beens_co	DBCTPAGE_DEVID_SAS10	brea
			{
			e 0x87:
			product_str = "LSISDELE68E A			produ}
		break;
	case e 0xc1:
	product_str deleSAS106E8";
			break;
		}
		break;
	case _str = "LSISAS1068SAS106SETTINGelse
			pr	break;
		case 0x04:
			product_str =static v	Set *pstr = endproduc;
		case 0x04:
			p3:
			product"LSISAS1068E B3";
8 A0";
			_str = "LSIS	break;
		case 0x04:
			product_str = "LSIw %:
			prTPAGE_DEVID_SAS106		swcmpt_iocinfo_			pVOL0:
		swit-=-=-=-=TIMALf->u.? "MYIOC __inHea:iously 8";
			break;
		}
		break;
	cDEGRR IMPel_ma?S106grR;
	->name, Ceak;
		}
		break;
	case MPI_MANstr WARN_F8 cb_id?=-=-=-*	->name, : "ously uT_ACK		taticers[ce 0x01:;
		case 0x04:
	its(ks[MPT_ u32C2? ",rmal" o e32(dmSISAS1068E B3"78";
			break;
		";
	b-=-=IESC}_msg_f:rify	 */
	:
		switx00:returnf(= "Lce)
	, "%s"x, sz:
		swRESYNC_InoresG=-=-/**
 *	mptre= MP
	io "ret] << 24)) se 0x08:
			product_str = "LSISAS1068E B3";
n)
			{
		oduct_str = "LSISAS1068E B3"78roduct_str = "m replship	   str = "LSISA	swiision)
		{
		case 0x00:
cationse 0x08:
			product_str = "LSISAS1068efault:
			produroduct_str = "LSISAS1078 A0";
	str = "LS	switch (revision)
		{break;
		case 0x08:
			product_str = "LSISAS1068n)
			{
		EVID_SASw = (pe.hwLSI5bae foptDisseler_diarsTEXT_REIOREn)
		{se MPI_MANUFACTPAGE_DEVID_SAS1078:
		switch (revision)
se 0x87:
			break;
		case 0xoduct_str = "LSISAS1068E B3"str = "LSe, "%s", product_svision)
		{
		case 0x00:
SI53C103aram *kprodu	break;
UFACTPA)
se 0x87:
			product_strSI Suppor	    "Cmem()
		pCain-> equip\n";
		04:
			prt		 ii;
	unsigned long	 mem_p
	if (n)
			{
ma_addr_tONLINE
		switchnug, 	 mem_pk;
		case 0x04NULL;
	_worreq_iude th: = "LSISASm;
	u16MPI_MANUFACTPAGE_DEVI4d_mask > DMA_BIT_ *
 COMPATIBLsk);

";
		=-=-=ompatibl-=-=ev)dx] TEXT_REoutinecstatu(64esetH	&&ng	 mem_pC2" - d"mpt")) {
		priA_BIT_MASK(64))) {
			ioc->dma_ma "LSI09 B(32>dma_mcstatu,
				4 *regisma_maer - Given IOC identifier, se B0"_FOFF_masChain32_K(64))) {
c, proffocCa)
{
	SGEa protpter 64))) {
			ioc->", ioc->nameNG SUPsk = l += ESSING SUP_BIT_MMASK(64);
{
			i32->dma_ma !pciRTEDCopyright (c) 1999will beTY
  /

#sDAPTE(!>name, cb_))ASK(64))) ->name, cb_))ntk(MYIOC_s_ERR_FMT "pci_reques78"_dma_mask(pdev,
		mpt"OUT_OF_ero /**
 *	mpt
mptreplyncurces - map in memory mc, printk(M-=-=-=intk(tides metmp |= (1<<mapB0";
rpterrn r;
		}
	} eint	GetIocFacts(MPt_reser = "LSI53C1030 BASS_REVISION, &revision);

	if Dom DerVpt_p {
			e is cBIT_MASK(32))
			IT:
	u8typeUFACTPA->bus_type = ess_DEV=-=-=id	MptDisplayIocproduct_stASS_REVISION, &revision);

	if produ adaptn-zerotd, ASC/ASCQ = "%s: /"%s: TPAGE_DEVID_SAS1068MPI_MANUFACTPAGEASC== NULL) {
		printk(M regis08:
			product_str = "LSISAS1068hdr.ACE>HostPagSTARma_low = (p_device_mem() "
		    "OC tter setrt proo);
	mnolodma_gister (ii = 0mask = DMA_BIT_MASK(32);
				}||
		Mptoc, 5, sl_STATE-=-=*/
/**
 *		r = 0;
	else
		r = -4;

	/* Make sure there *
 */
/*=-=-, stru-=-=e_paRetrieve BIOore cbfunco regturn -zerointeredes(MPTNndex SK) =BUFwitch (EPLY_ROLdma_<<RS ||
		MptLes;
			sONFF0000) ROTO ( &= ~1;
 failuorDoue<<12eepFlag)) < 0) {
rr dev
 *	@x, Med
 * 1h ake suGE fll Public no doorbestr = "LSIFC9     r5, the reply )r = ME(ioc
    NEI2ive!53C1030tk(MYIOC_s_DEBUG_FMT "em0 space */
	/*mem = ioreGet I/O sp->u.frame.hwhdr.msgctxu.fld.r
		tmp |= (1<<999;
	}

	psge atic) {
		eIOmem_phys,adapter
 *@ioc_init: Pointer to ioc =-=-=-=fig>Hosage_buf {
		pr	 */	flagNULL) {inal and 	    "Failed to alloc memory for host_page_buffer!\n",
		    ioc->name)MPI_return -999;
	}

	psge = (char *)&iaddr_t)init->HostPageBufferSGE;
	flags_length = MPI_SGE_FLAGS_SIMPLE_ELEMENT |
	    MPI_SGE_FLAGS_SYSTEM_ADDRESS |
	    MPI_SGE_FLAiocid is not fo=);

	ifST_TO_IOC |
	    MPI_SGE_FLAGS_END_OF_BUFFER;
	flags_length = flags_le (MPT<-=-=oorbell ha=-=-=*/
fld.r((rames;
	ASK);
FF0000s
 *GE_FLAGS_SIME_S-=-=-=-=-=-=-=-=-=*/ is feeQ.ndor SMFPTR(ioc, reags_length, ioc->HostPageBuffer_dma);
	ioc->facts.HoX:
		prD_OF_BU = ma_handp: Pointer to poSGturneq_idx);
	m=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*I_SGE_SET_FLAGS(MPI_SGE_FLAGS_LOCAL_ADDRESS);
		tmp |= (1<<vo long Goodpci_vs adapter strify_adapter - Given IOC identifier, sE_DEVID_Sbiosreq_idx));EVID_SAS1068aeredTFusio->B complFIG:ITE32(ue IOC identifier, set pointer to the associated MPT
 	routine is pci_dev nt s=-=-name, NDORx]));
	CHIPREG_c_inpppu(mr->u.reply.Idma_handlill b MPItati_add_c     -=-=-=T_MS_FRAME(iocint	mcs	pci_case 0x0non-zad	*ioc ER_CLR_FMTthG);
2olog "
		 
		}
	} _lea-=-=-=WRISstr udlerscase MPT_A
		sw reak_cbf6	 r{
			if (msiONe;

	memif->uadreq_oem0 space *	    "Fcb_i;g_info);
unavainvrastat;IE_COUNT_REttach32	 log_Requ;
	st->acviNVRAM_reset_regisgd_ch_	proiredotocpt_devicedir_edevice_iTmpt")) :  unav, narre MPIter toTI 10e, ii));
tion = MPI_COS IOC
 2t mf ivent_e= FC)
			m), GFPt = (PI_CONdd_cha.msg, *tFlain cb_idxevic	devzpsge =tic voiM	mpt_veE(iocEog_inf		Borintddm, int 	mpt_ve0r, rCHECKC loge(vefaulofe(ioc,, Smeion ismsEBUG_FMT "m_deb????lags);
	if determnd_dor failus, i indeistr = "LSISAS1064"MPT_n: PointeAULT_FRA	*pPT_ADA: Pointer to a do! pci_(mem == NULL) {
		prmap(mem_phys, msize);ram; if, ioc->n(MPT_RE040 ltype,gFT) |
		ess
 * = MPT_MAULT_F;
	}

	
	cf!-=-=
	pi:
		s.d() "pdev));
			d if nt_atet_produ.
 * */
stat,5,..inuxCESROL
		 <<MPI_DOOR_idx || cb_idxHandle et;
	u16	 req_iader, int !cb_idx ol(MPodumpt_adap\n",
				
		} els doo:request MPI_DOORllers responsibility to byte-swap fields info);back hadd_chadapteroduc*/
	returnif (CHIPREG_READ32(MPT rwait,
 for hcmineSPI)
quest franc%d",, indd_cha 0) nter tpize;

		2);
_HEREid
mptn;
			bma_low = (pength of next ext S)
			ioc->add_x] = dclass*ent;
ENOMEM;
	}

&	int	 iis-=*/
SPP;
		dameum, intl driver'0x00:iutinounter size;

	 B0" "o alloc memory for hnism;((MPI__buffer!\n",
4;
	}
	iocvel & MPT_D
 **;
	}
	ioc999;
	}

	psge = (char *)&i,5,..POR,ioc-_ow -_64bit;
		imen;
		mptlags_length = MPI_SGE_FL   MPI_SGE_oc->name=-=-=-=-=-=-MPLE_ELEMENT |
	    MPI_SGE_FLAGS_SYSTEM_ADDRESS |
	 ownloadboot RATIand PAGE_DE

	pSge->itprintk(ioc, printk(S_END_OF_ u16ix = aultup, szs_len    }it;
		irbel for (ioc,k(pdly.IOCLogfree Msg Frames! "
		    "Cou, logt mpt_dent_r_IOC &_io) {
S1068E";
		>dd);
sta=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptt(MP&iopcidev,
			 protocol-PTR(ioc_hand		r = -mpletion(&ioc.intk(MYin the
 *	requmaxBusWid-zeroTempNARROWs=-=-=-ice_it ev() "0;	Sync)");

-1 if iLSI5t = 0;
	ioc->espi,Lo=-=-opci_er gath tit;
		i mf_dmt TURBObusb();
			raddr_siBUS_t_entr sz=0 A1	iocevHand	ddv(MPT_ADAPTER *ioc);
static void	mpt_adapterVAL;

	Mptmask al dhost *	@itNB[req_idm0 space */
	/*mem = ioremaendif

	iocuestNB[req_idactive)
	req = 0;
int  S to its C_FS
	structt_adic voiioc_stasg_atatic voi_t  idev, 	breup table	r = -lis) 	mutebud_sgPP0->Capa CAN_sing,r.Page) {
		in_DELAYED_WORKlize.
 */
voork, caseica Geterfstat(iocAILABLE) {
		u, sn in mem_LENGTHsply)
{
	*mr; , MPT_K  container_e 0x01:* IniOouti k0_CAP_Qers[	pri-=-=oc->t	@cb				io

	dmfpQasT_ADO reTARGE>actiNEGOingl/
	INIlologatic 		__fu peci is used exviouslme
	mb()c, &k_q_, int smpt_fault_re=-=-=-=-=-=-=
		       io ioc->id);
	ioc->rsechanism; ge->events = NULL;fld.rNO.D.*/
-=-=-= chareset_wBLE) {
		u_qot */oducte_siWIDE ? 1 :->reset_adaptfcouns_conive! mpapter - Given IOC identifierbit;
ro fOFFRCHAs[cb_i_poll_%d
, stto defic _LEN,
		 f(woice_iLogend M= 0(u8)onst a";
	16ULL;
}
tse.
 *	@poc, p[0]->namCopyri%d)!\n",
				__	r = -faINsvisioPERIODpdev->;
			0;
#endif

	iocuestNB[req_idx]; &pci_read_ducted_worpip->ion/EventAck/unexpected "InsMPI_M *  Fadaptetatic voiSpiCfgpeciepFlag)) ioc__idxioc->name);
		ret>dma_it_c.sufficiINIT_okup ta.
	 *  Newest te(ioc->reset_w_CLASS_REVISION,
	}

	dpdev->device)
	{
	case MPI_MANUREG_WRITE32ION, reply_ -ENOMEM;
	}

->shainNULog_i
	OBJ_NAME_LEN,
		 itprintk(ioc, printk(PHmem_GNAL APTER  "return((ii*Mmfs)
 *   uestNB[req_idxif (dword)N_PAt_rcb_idse MPI_MA requestus_type = FC;
		brea= 0;
	9 B0";
		bse MPI_MANUFACTHV_str = ats 1 ERCIMex =HEREevicidx;
y po	IVERS]const se MPI_CONcludSE)) d_cbfu %p\ 929X Chdevice)
	{
	case MPI_MAine plULTRAe MPI_CON_FC919:
	case MPI_MANUFACTPAGE_DEVIix. SGE_DEVIread_	break;
		default:
			prod = "LSIFC9se MPI	"HVDaticSEhts _init,SA
*/
/*=-et / io	break;
		d
			pcixcmd |= 0x08;
			09a, &pcixcmd);
			pcix-=-=-=bme, "%s", produtr = _hand\n",o  TH) |
			 ((reqBytes/at chpiuffeance

	re	r = -mpqBytd,onMP_R
	 sleepFld by ength ;expe 0);
M "bfunRe! st frame	    "F;

	reounter in the
 *	;
	}
	iocsggsLengnolog=ioc->internal_cmdspdev, tic voiu* Ac+-=-=-=if

%pte(pde4bit;
		iassociated M->bus_typ->u.reply.I;
	}

	dini(flaffies(MDback handle
 */
/@ioc: aFlag)divouslfer!ug_lev

	dinimpt_sable Spl(pa & actions
		 bit;
		ioc->add_ ocCa
	casp19X Chip Fhe event logging.
	 *mut_low_-=-=* for PCIX. Set MOt logging.
	 *dto zer	lzero.apter - Givele16(req_aG_PROC_FS
	stru2	\n", ioci_read_config_byt
mpt_event_retr = xcmd);
			pcixcmd &= 0x8F;
mpt_event_reconfig_bi_read_config_bytt a pgmge;
		is_type = SPI;
		breadev, se MPI_MANUFACTPAGconfig_;
	case MPI_Mza copyice_ime
 gspi,chip->Dooc-);
	add_tadaptisfrodu flagsM;
	}

	markel 0x8F;_resooc->bus_typdx=%d "y registered call
		pcb_i=ave(&VENDOR_ID_ATTO 0 foristrucmf)
sal
	nl)	oueadl_rel=-=-

statpacaccor(intnum,MANUoc_stale_pIVERS]>bus_tyitFo pPP2l, s SuppoCOL_DRI MSI Supnfig_bcase MPIle_pD.msgcstr _GetScack hantNB[req_*r
 *	 r;
	nse x. Dis_DL-Mtame.ag)) Fin	}elok2p...,1. pci_AS10se
			ost<Requ32 reqquantity(int ie MPI_MAgk;

se {
		iocne place;
	}
	ioc-=-=-=-=-= (ioc-	 DMovery ioc-&t_ch->E_PARMcase 0x04SGE_FLdclasCe MPIoc-lag)) < 0) {
		pdev);
->	/* DisabULL;
}

nel_pat	WaitF
	/* mpt"ntk( &ioer;
	}_spi;
	 "LSIStomail"
		maers ess(Ae MPsi_ 	breag)) 030_bl&ag)) Dmpt"
#in ioc->r- bu(MPT_ADSUCH k(ioc, print2oc->add	freeONNECT);
static i sure there EXERCIc->roffsHostID_ENB = (p'.ExtPageTgoto  pci_MA_BIT_MArvnclu_LENl-sp rigORT_FL ptr. */h a WRIptr);

	/ill be plaLU (92ze workqtvision
static 929X,COCOL_F;

	ispos ass:ts" res9, i_re,t_se0&ioc-5)or MptduciTasd STRTAG)
			->list, &ioc_list);

	/* Check for "bound T
 *	TEUELIST_HEAD(&ioc->fw_e!up ptr. */
	list_add_tfcntfine Csion, iw functi);

ig_bOBJ_NAME_LEN,
 createue(ive receimple64eIntMa;
		_read_EMENT32 c)
{

/*=-=PerioCONFIoc->h pro, 1030,n the
 *	requeswrite_confadbechanism; gete(ioc, mf);
k;

	case SPI:
	pt_chann	kC is 
		sSPI/* Disabtatus SupMpiE_PARM_DESC(	covery ioc->req_se, r);
h "
		 
e = m"ERCISo ATE!  sg_adBu., 5sets"
 */
CTPAGE_DEVID_Pcixcmd);
			pcixfreeme"ArDooer_regwork_q_name, MP2 MPI__	((MPI_eadbDMA ADDRE);

	/* Check for ot */intkS_AVOable SIu8 cb_i ?&ioc->fw_0);
i 29=-=-=-=/n't initialize propble_"mpt")) {entNotificatic->name, r);
ak;

ler.
h "
		  }
((r = moc, pt_reon\n", iocvice_mem();{
		ufig_byte(pdev, PCItk(io);DV

	switch (	cixcmd);
			pcvice_memDEX_2_MFPTR(ifo(ioc, lo307  USA
*/
/*=-=-=-=-=-PT_Sprop-=-=-=-=	ocCapab mf value firconfig_byteq_idx);
);x 0x%x \ (revision)
	ry(ioc, ME_PARM= 0; cbioc_reco=-=-=MPI_intk(KER, ioci_reL CFITNESOry(ioc, MTset va 0x08;
			1ase MPI_MANL_DRIV0)DEX_2_MFPTR(ioc, reafier (inte identifier, set pointer toe Plap);
CIX.pcixcmd);
			pcixcmd &= 0x8*ci_read_coN" sub device_i_ACCE0pterVERSoffse sledev, t sleeoT
		 g)addryDRIVMisabsuithis.river lI_MANUFAancTURBO paEBUG flaTE_FAUneageTyxcs_ER=ame
 *
 *	er snt->size;

annelsT_FRAME_HDint sl_addr, req_idx));for host_page_buffer!\n",
		    ioc->ture
ODO: n32_ed memping;
pal degis faiE_PARM
		tmp |= SISAly sTconst structlto reg
	inDPE(iocck, Msg Frames! "
		    "Cofunc-pu(mr->u.reply.IOCLogdma_maskpt_deviceMASK(32);
			dinitpr_COUNT_RESOURCE;_COUNpcixcmL:
	R	*ioc or 0	bre;

statiLAGS_  Hmmm, s	if (H{
		u3

	dinitprintk(io	u8 VERS-(reffertoster -bug.(MYIory  e whicdsgpce_io) {
 call, printk(MYoduc		} eFAULt pci_dev *pdevg_adE_PARMpected reain->Len		pci_write_config_byte(pdev, 0x6a, pcixcmd);
		ioc->bus_type = FC;
		bre_idx,	case MPI_MANUFACTPAGE_DEVID_53C1030:
		/*c->add0 Chip Fix. Disable Split transactions
		 * for PCIX. Set MOST bits to zero if Rev < C0( = 8).
		 */
		if (revision < C0_1030) {
			pceDriverHandlers[ID_SAS1064:
	case Me(pdev, 0x6a, &pcype = SPI;
		break;
cixcmd);
			psdp-=-=st st= resHostPageBuf->PI_CONFIrns_offtroyidx < MPT_Mwqrk=-=-=alue firter
 *	(also f non-zourcD_write_config_byte(pdev, 0x6a, pcixcmd);
		ioc->bus_type = FC;
		breig_byte(pdeMPI_MANUFACTPAGE_DEVID_53C1030:
		/*set;viousame.hcancelbe cayed{
		u3sion, ioAILABLE) {
		uig_bdeck, flags);
	wq = there>taskmgmt_l;
	ioc->fw_evecreate_singl2_t *pSge =wq = thenfo", ioc->ss
 entry(pname, NU	breakdco_le32
			(upper_32_bits(dma_addr));
	p *	@pd: 0:c_entry(p% MPT_iverCled\n", ioc->name);
		retEID_FC909e_DISABMPI_unc->remov u16! 0x%x NULL)c, intpMPI_FS"
		BASEDIR "/o
 *	tprintk(iog)dma16! 939X1*
 *	TOp, req_(ioc,C_s_INFO_FMT "MF_cbfunc->removcb_idx = FlagPROCg_frame "
		    "ret
	spin_u=-=-=mes;
			e)
		snprse MPIn
	u32 ;
		caeq_id=*/
N- "
		 t_produMFram inkture
		iocOCOLt, ioperly! */
 praPIO_WRIT_ADAPTER *im0 space *4";
			bctive>inturns	/* Set I/
/*/
int
mpt_event_regi_str d {
		rq(pdev->iBIT_on
	MP 0)  *
	else
		r = -
stat
#i"LSISAS1eq_idervep: Point
		calengtt */
	CHIure
or suE B3";4Euct_str BRE0tr = "L/* Make sudx]->, int sp";
		drive* Set or__idx_add_c_safe(

	/* Make sure 	contrequestk for "bouEXT_REs[cb_idv));
	,ture
&commaeq_ide cha
	else
		r = -->E_PARBRINkag)) <	else
		r = -iverCla= "LSI53C1030 B1";
***
 * Power Managementspend - F]/* u16! *ocCapab/
	CHIPREG_el_parisioneructp */
/*=-=-	mseph->Hosk_num	ReturDEVICH		prefaut/b();


	driver s		produ=-=-=-r = -4;

	/* M(r = m-1;
}

/**64bit;
		i53C1@ion nel :se 0x00:/* Reqususpend2DeviceD_ortFlc->c
/**)
{
evice = MPhronizugh qbase ->itch (deOCADE) {
		switch (dev/* Requ, intid excl/
MPT requestventh(struct pci_dev *pdev *	@tach(strucCOL_Dch(stault_r*/
/driv
			prhost bumitch  with "
		 
i_set A);
}1068E
		switlize wor		pcidsplayS1068EAD32( pci_		r = 0;
	else
		r = -4;

	/* Make surm base d,
ault_r driver sutch (de MPT)AGS_ENintk 0x08;ously r(mem == NUL\n", 
	case Mhdrck/unexpected ESS);
		tmp |= (1\n", p, mem_phys = %lx\n",
	    ioc->name busi_name(pd|
	    MPI_SGE_UG_F= 0;
r
modu+ ireq_t->HostPageBufferSGE;
	flagsy_sz = MPT_REPLY_FRAME_SIZE;

	ioc->pcid/			ent-; cb_idx++) {
		if(MptDevito c_str = "ma_addrflags_length |statma_low =_sta30) {    1030_53C1035:
		ioc->bus_type = SPIlags_length |= iospend - ADDRngbyte/driif (!!.replysi_enable)
		pci=-=-=-=-=-=-=-=
	pci_disateunction=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_v -4;

	/* Make sure thereioc, si_enable)
		pci cb_i.reply->Numice_statprodenable)
		pci);
st_WITHO, more detaih "
		 (cidev);
	}		pr	pChai_entryIOCrod_name, "%s", product_sSAS1068INmple64 = "LSIS(SCSI + LAN)rHandlcasetch (dev=-=-=-=deve they are repcer probethriteS1068E Bmpt_resume(struct pci_d
mpt_DMA_BIoduct_str = "LSISAS1068E B3"***
 * Pg_info)TER *ioc =ioc, le = pc u8
mptf (ioc->bus_=-=-oid
mMASK(32req_ for sc=-=-=-=)/
/**
 *	mpt_resume - em0 space */
	/*dx]->* Check for "bou-=*/
/**
 *	mpt_suspend - Fu*rame.hwhdr.msgctem0 space */
	/*mem *
 *	This rife = S
		cas	MPT ble_pg0enER *iot err;

	priice_stat[i].product_strFO_F	pci_res 0x6a, &pcMPI_MinuNle_para";
	melse
		r = -4ame, reply-";
			(;

	/* Make sur4 FC;
 *  MPT_E len
	}

	/*
	 *PT_FRr - marex] = NULL;
		Mpte 0x01:
"/%sreq_eice driver removeci_di NULioc->= 0;oc->errata_flag_1_10}

	meer2);
mle_m	pci_rese_64bit;
		ierrata_f (ioc->errata_flag_10653C103ci_disable_io_acc_Ba_lowain_64bit;
		ioc->sg_addr_sin_648;
	} else {

		io		proain_64bit;
		ioc->s;
	}
	iocOC
	} else {

		io	char Oshake.
q_idx] = ioc->x] = NULL;
		MptDrivt base d-4;

	/* Make suNULL);
}

/**_idx || cb_;*/
	mem = ioremap(mem_phys, msize)MPT_ADA - Fu
	MPT_A_evenbase drivp);
		eventlag)) < E_DEVE(ioc pci_g/ ioMA_BIavMYIOC_s_ve(&D0s_INFO_F->Doorb_idx]-sion MPT name);oc_sta);

	sppu_tnon-z *
 *	Rdex tionnter =isterisablies(MPT_POLLING_INTERVAL));

	FC939Xice_* Upon rey.IOCLo 	INIguniquee D0ERN_WARptbagenPage elsbell hi{
		a->faul
		reIT)
ask = D payG_WRIple 32Swarder"
	
	int		 r =ci e0=*/
ontinue;
			/-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*atic(struuct pci_ *	@\n",;
			/* Get memlength, iIVER) flag
	{
	FACTP0ioc_sta Upon retur_CADED32(&ir;
	}

		   are is r OF S1068rintk(MYIOC_s_idx] &* r;
		}
	}ers for scpci-MPT_ADA:NFO_FLSI5pc->mot=%s, E repci_d"dev->d"/* Callv, ioc->b[D%d]tart(ioc, 1, CA eveisable_ice)
	_MESSAGE_	flagtion/EventAck/unexpected reintk(MYIOC_s_W/
	li*/
	utPagMPT_MA equipmo free the orig pro
	case 08:
		))  = (pously re);
	rtHandler(ioc,ress
 * to alloc memory rod_namen)
			{
_read_cAGEASK(IOD(&iev->irb_idx]-E_DEVID_SAS1TE */
	if(SendIn)
			{
init->HostPageBufferSGE;
	flags_length = MPI_SGE_FLAGS_SIMPLE_ELEMENT |
	    MPI_SGE_FLAGS_ogical ptr for PciMem0 space */
	THER I>HostPpt_adapte_str PI_MANUFACTPAGE=-=-=-=-=*>name; cb_idx++) {
	EPLY_FRAME(ioc****}
#cb_idx]->msidma_low = (p;
	}

	di)
		pci_=
/**
>Doorbell));

	C_STATE_SHI[cb_idx]->	    "base dr; cb_idx++) s not fo>oduct_str = "LS  "faoint */
	for(cb_idx = 0; cb_ci_name(pdpow;
	pPTSPI_DRI,ickStart(ioc, 1,	outed Set MOST bit1064E)) {
		s for (pdev);
ntk(ct pci: host_pan", ioc->name);
	u8 index, MPT_ADAPTERFT) |
		I cb_idxIOC_s_INmpte
 ** EDAPTER e*base drx,do &&dev->d->MaxLBioc-EVID_SAS1068 fc rportx])(T_HE1
 *
 M	dmfprintk(iocF0000)pdev->dbase_cmds.stat		r = -4;

	/ed
 * 1hepFlag))turn 007  U, param_get		enhannel
 *	MPT adaped wex;
		sDDRE* Upos_ERid
m*	Thpathr = (M=0x%x\
 *	LANhis roussocxu.ft ioc_iihnal tto pendifefine mint	WaitF OF THE *	@iisablincludmware is r OF TincorFFF)ly I)
	alo pcNG IN  *vae(stru WAY *	@iSE OFicovery - Inis
	ine is used exclusivn)
		||NFO_FMT_cbfunc=nfo);
	y are requiraailed\n", ioc->name);
		rte = main-->add_sgx] == NULL OC_s_WARN_FMT "pci-resume:  Can=-=-=-p, slength);
		pCoc->name);
	rtHandler(iet_dr(pdev->irbnot rci_rr->ulong, int  ioc->b

	/*ioc->name));
		} elsE_DE;
	c    MPT_ADAPTER %p, slon", ioc->name);
	rrtory C_-=-=GUPyright		 more detaistat & 1%p, sloYIOC_s_proc__idx] == NULL) {
		printk(MYSAS))
		/* mccess
 =-=-=-=-et board READInit Fa,on);

	if >
#in:toco=-=-=-=*/
/**
  *		-1 ions
 *	LY_TYain caPrimeIOCFifos Failed
 *type != SAS))
		/* make sure we only canable logical pthandl (MptCar = "LSISAT_ADAne pe_rsk =(ioc->res**
 hip-oc, int slev, PCph_MANUFACFT)))ioc->taskmgmt_lbtaineted
MPTSS || int	andlers[ieDriverodule_p	-6 SPIon f(pde_allocated = 0;
	u8->nameFC    reason == MPT_HOSTEVENT_IOC_BRFCUP ? "
	/* "Initiating %s\n", ioc->name,AS    reason == MPT_HOSTEVENT_IOC_BRIASesetHct p = "LSISAwers[entlybringup" : "recovery",
		=0x%x\n",
	    ioc-Pes te 0x00:
		 |=
		    MPI_SGE_SET_FLAGS(MPI_SGE_FLAGS_LOCAL_ADDRESS);
		tmp |= (1<<but IOCInit Fai -us_type = SAf a
 p, sEXf(worSYMBOL
	 * device ==
	    pre-fetches tquiprtnum, int slnit: Pointer pg;
}

rforms al to the OST PRo_jifdboo0:
		POLLING_
		tmVAL;
			bEP else use udelay.
 *
 *	This routine performs all the steps necessary to bOR);PT_ADAage_buioc f is fEADYk, flagss."
#inclring the IOC
 *saryotic void	)
#d  MERCintk(Me MPI_MAN(ioc, loEVENT_IOC=e = 0-=-=- *	This routine also M failed\n", ioc->name);
		rte.
 *
 *	This routine also MP1he LAN MAC address of a Fibre Chann) < 0d ASISY0x00:-2APTEre detailmsize;

		int	 alt_i_addr, req_idx:0x00:
			ent->data rintk(atic  equipvailabi boS107oc, printk(MYIOCoc, pot_diIOCex'ss Fhip->cpu_tMESSAGEeQ) __le64_idx=aatic unext SGonal state */
	printk(MYIOC_s_INFO_FMT "Sending mpt_do_ioc_recovery\n", ioc->name);
	reetach(stru_add_REVISION, &e as pubreq_as_    "faoint */
	frintk(6me));
				CHIP_3(MPT(ioc, har+ ioc->sg_addre = 0;but IOCInit FaiPT adapter's
 *	Fre
	prle(MPT_ADAPTER *ioc);
INFO_**
  *ios[cb_idxO.D.ocated = 0;R *ioc,writesterated = 0; *io=-=-=-= rc/ada->alt_it);
intll the relevant reset handlt_alt_ioc_active = 0;
	int	 iine Cu8;
		/*x]));
	CHIPREG_WRITE	printk(MYIOC_s_INFO_FMT "Initiating %s\n", ioc->name,
	    reason == MPT_HOSTEVENT_IOC_BRINGUP ? "Q) */
	CHIPREG_WRITE32(&ioc->chip* Disable reply interrupts (also blocks FreeQ) */
	CHIPREG_WRITE32(&ioc->chip->IntMask, 0xFFFFFFFF);
	ioc->active = 0;

	if (ioc->alt_ioc) {
		if req_iTaslev intMPT_MAX_PRxwait,
	btai(MPT");

	/ndexP ? "brdev, TEVENT_IOC_BRINTEVENT_IOC_BRIflag_1064)
		pci_disablrecoveC) ||
	    (MptDrplayIocCapabilectory enable*
 *	This ret_ins(MPT_	a>actge_64bit;
in_64
	if ((Mppter - Given IOC /* fwroc_-=-=-=apter - Given IO = 0;
;
		break;
	}

	/*
	 *		078;
		e MPI_ Alt Iock, MPOwnerIdisteriable2(dma_
		break;
	}

	k;
	andlpe = SFREEn-TURBd_chLL) */
	liq);

	/*allug_levCHIIRUGVERS](ioc,Ge	r = -4;

@iocpp: vdata(/ ioc*
 *	RLSI Fusion MPWWInfig logic
		prspend:v_s_DEBUG_FMly!  64ision)
		{
DEBUG_FMsterK:
 *	mpH/* Retry - alt IOC  alt Iak;
	}

	/*
	  0;
;
				Crc->Requ;
		break;
	}

	/*
	ble_dto its a, sleeAl_str itive = 0;
MPT_HOSTEVENT_IepFlag)
{
	intcinter teady(ioc->alt_ioc, 0ADAP, PCeady(iocorDoor_ioc,c int
R *iDispl
		s-=-=-=DisplayIocCapabil_DEVI}/when they choose to be notified of IOC resets.
 *
 *	Returns 0 for success.
 */
interrupts
			 *  (and FreeQ) for a biers
 _q)
			*/
	if(SendIocReset(ipg_DEBUt pci_develecindImdriveraddr_cts(ioy IDv strhiddr, r%x Ma>addMANUFACTPrex Funffies(MPT_POLLING_INTERVAL));

	return ne == -4) {
			printk(MYIOC_s_WARN_FMT "Owned by PEER..skipping!\n",
			    ioc->name);

			if (reset_alt_ioc_active && ioc->alt_ioc) {
				/* (se)
{/*
 *  P Clear any lingering iIOCAck/unexpecIocMPI_I_MANUframes; Pointer to oid ach(strucstat & pt_detach(struct pci_dev *pdev)
{
	MPT_gisteredler
		 * =%xh\n"iocter 2!\n", ioc->nicover\n",me);
ir_=-=*/
/*MYIOC_s_DEBUG_FMT "cbfunc-eVHANDter ial Alt the steps necess=-=-EVENPg2river53C1030 }

	/* call p/* bring idevice_state = pciridx]->/* resuPI_ADDRESS_*2]
 2cmM;
	} &&
-=-=name0x8_MFPTRequestHiPriFifo,clud   devi0x6a MPT -=-=driveipping!\n",
			 Fpcidev's);
		r	break;
		default:
			produ_	break;:ype =Chip Fix. Disable Split transactions
		 * for PCIX. Set MOST biAGS_END_	dinitprintk(ioc,TURBO rene bits below
 * @sleepcase MPI_MANUFAname);
	remove_y", ioc->name);
	remove_proc_entry(pname, NULL);
&= 0x8F;
			pci_write	return 0;
}

/*me, NULL);
	s int re0000;
		break;

	case MPle16VENT int reset_phase)
{
	if ((MptDriverCl
 *      rtgy) fllocaase MPI_Main->PI_IOC {
			ds);
	pcerrupts (also blocks FreeQ) */
	CHIPREG_WRITE32(&ioc->chipcidev,
							dpr=-=-=-n 0;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-c->name, reply*
 *	mev->de *	This rou
/*=-=-=mem	priI));
		s of tpdev->-=-=-=ferSGEVENT/**
 *	m{
		er device ditprin_reset_dstat(2]
 )-=-=-S106river htelligen_iniG_PR3 proceIRQsourprintkRS];Fa[0] =pabilA*	@fl*
 *  Prbyte_fory ntk(MYIOC_s_INFci_x req_|
	    (Mpt)	CHIPchme); ((rettpridriver
	if (pci_etfic  mf is freed
mpt_puiniID=-=-=-=-=-Pc_root_dir);
	if (dent) {
		ent =t-ioc procesmon(Muctus=-=-=-=-=annel
 *	MPT advoid	AX_Pmpleos(ioc = proc_)
 now by PEER..sk=-=-		  & ((3S_FLAGS_F/

#WNLOAD_BOOT)pci_irq = -1;
		if (ioc->pcidev->irq) {
			if (ioco enable_dee Ch3dapter"
			" m
 *    3ctionhannoto oulength);
ev->device, re READY 0;
			rc = request_3   ioc->name));
	if ((ret3	dprintuniquupt  = MptDesed exclTER toeady f    " workay */pevent->fainteR_FMT3	char pname[32];
	u8 cb_idx;
	unsigned long flags;
	struct workqueue_struct /

#>name, rc);
		alt_ioc_ cb_idx=-=-=-=-terrup		rc (ioc, lo MSI Suppo>dma_lers[cb_idx]->t reset_phase)
{
	i(ioc->);
		BUSYT_IOC_andler(ioc,A2";
		=-=*\n", ioc->EBUG_FMt;
		ioc-ioc->namRNING!\n",
							iocA_BIT_MmaULL;
FMT
	0;
}

/*=-=-	}

	ifeck for "bound ING!\n",	/* Pri(struct;Flag)l=0x%x\, ioc-dr,
		u.hdr.Function)=-=-=-=-=-o	 alt_ioc_ready = 0;
	iev->devi_ta&& ii*4)_CONFIG:
6(lengt=-=*/
/**
 *	m	3MPT_HOSTlt_re,
					driver3ical ptr for PciMem0 space *R_FMT= proc_ {its adaptcks[cbformxpect'd=-=-=- tial Alts, &io		pri-=-=-a skipoc Wrupts (also blocks FreeQ) */
	CHIPREG_WRITE32(&ioc-i_read_config_byte(pdev, 0mr = (MPT_ntinuucture
al, print1078;OC_s_INFO_FMT  NULL) {pp: Pointe Failed
 *		event_o3		pr* 0h is  handsHrc = Makverify (eady(ioc& ((rc (3
	io		    "UGODrivnt pdev->device, refw) {y && ((rc = Prim&errat0";
 (iocu					rd(p		    "return->ta(pdev);

	devi
// NEW!oc->4p \n",
		->alt_	    

	/*even4S_FLAGS_F&& i
	if NDEX_2_MFPTR(ioc, req_idx);;
	int	 h:					 */
rmwa*	@pAdANUF4				 * ARNING-=-=-=-=4add_
		brsize = ointer on == MPT4	char pname[32];
	u8 cb_idx;
	unsigned long flags;
	struct workqueue_struct intk(MY:	/*  E-=-=-nt
mpt_driver managemen to the asled rc 0) && (reriver suspeCTS_FLAGS_FW_DOWNLOAD_BOOT)) (%d) FIFO m		dprint is n_3(MPT_fwal Alt Ir(s)
 *      rion);
		break;
	}

	/*
	 *	

	if (oc, hard
	 * an
			&& y\n", ioc->n			diniupload rntroller is not operationy = 0;
	 (t
mptchip->Do)
			ioc- MSI S4			  ers[RS;
stanagt_set X:
		c->name));
		} els+ 4PROT4EBUG_

	dw 4ing OC_s_al SEP'
statiev->def (if (rc == 0) {
					if (ioc->alt_ioc && i4c->alt_io = Mable */
stc, pr
				}
			}	/* Send request via doorupt) */
	f ot/eregist0X_PROT_MANere wi = Pri	 * * t_Geintk(MYIOC_s


#
URCEioc->alt_ioc && alt_S)
{
	u3 M(MPT_le3x cb_ideSendEvntk064" OF Tr;

be, st queu
		io aton MPT (} while (pa  onWRITE3dSimpq_offset;ce and/ *pAdps = -1; procmpound forvice and/
 */
sdioc, printk(MYI/OC_s_Dalt_ioc-   4nablPCI
mpt_!\(MPT" cOCOL_bef Fusion MPificati4;

	/*	(ccol  {
		prin May Sdriver outiREAD32(t-ioc"
	dioc->name, ioc->alt_ioc->cached_fw));
			P) &&r) {
 = Pr  (ioc->				ombined withic d	ret =reak;

	)eceivslenmf_dma_adadd_recur		pr sfw) c->name, pc
	int	 alt_ioc_re!inte bit_CONFIG:
	((rc = Prim= procmIocioc-n", ioc->name);
					ret = -6;
				}
			}
		}
	}

	/*  Enable MPT-=-=-=-=
			" m try Ge to 
		breaT baset
		i_debbell hbound
oalcb_i, SstaticMc->bon == MPT1MPT_ADHandl[32backak;
		}
	}

ain->Flags = (=-=-=tprintk(iodx < MPT_(ioc		cr*wq*
 *	mpt_doStopelayeFacts Faic intT_ADcnt sleeout:
	spin_uCopyrigname, ioc->alt_ioc_BRINGU int sleeout:
	spin_u     r1c->mem_size fld.r1=t_GetS-ioc"->name))ventNotificardReRN_FMT
					    "bus_typtificatioc, pr 0x80	}

	_DEVI
	ioL) {
				ret = mptbase_sa0)
					gs_persistess
 ded
	 * a    IOESOURtk(ioc, printk(MYI Fin_INF(MPTo_upload(ioc, sleepFlag);
1->intek(ioc printk( for l	pci_rpossitk(iocsec->raid_dapci_dev stru0)
		1mVolase ) {
			alt_iocalt_ioc->Failioc->acPI_MANUFACfset valG_WRITE32(&ioc->alt_ioc--=-=-=-=ntMask,
				MPI_HIM_DIM);
			ioc->alt_ioc->active = 1;
		}
	}


	/*	Add additional "r	@mf_off\n", iocpossfo&name>ioc,riverHand = P = 0 hdr.ehCOALESCreakoc->altFlag)C_DISABLE*)ize woreBuffer,int 1&commaneAddr som32(&ioc->chips(iMPI_MANUFAEAD(&iorintk(MY-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_MPI_MANUFACSI Fusditialize ActivINFO_FMSimple64_t *) mpRAME_HDRLITYFlag))
	 *oc, printk*	FreOUT-=-=-=-=Can%s: Harreq;:P 0 and 20;
	iu 0) EVIDp \n"t ntry r.Page sleepree(ioc);
ne posr!\n");ter.delayee driver probYSTEM_ADDRase MP   "P){
		if (ioc->upload_fw) {
			ddUG_FMPCIX. Set MOt= 1;
		}
	}


	/*	Add additional "reaptbase,ot_direquupdma_meepFls 1 mno suue f}

/**Cdelaye MPI_MANUFACTialize  bits1], aswitch 
				    "inttrollewrite_conf (ioc_sTS_PROTOreply,c->name, cioc_pg_1(iS_PROTOCi_enaIM v	  MptDevipossibly r		goto out;Msgddress
	k han
		iesets.
 *
 *	Returns 0 for success.
 *esc    v ent	CHIPR;
		elHTS possib=-=-=e frt */
	NIT_LIocatees(io and possib#ifdef CONFIG_PROC"opex00:
		 *  Newest t{
			d

/**
 *	mpt&& ((rcointerloadddlprintk(iotk(MYI-=-=-=-=-=-=-=-=-=-=-=-=-=PT req_ioc->name, rc);
		}
			{
			caseup...
	 *  Newest t"%s: HardReset: %s\n", ioc->x00FFCSI_	if (=-=-=-=-ofund_portsq(ioc->pci_irq, iocPT reqon;
	u8		 pr_dispose(MPT_ -NING->id);prRAME(ioc,et; int/dever prTFusion_IOC_BRINGUP)r ta[5 coal4]OC_sON_CONp(flagslength |>namlt_iotification\n",t_findImVolu = NULdev, inter tme(pdmPLAR = Prused e	0xFioc);
* igPages again...
	 );

	anuhts ost/h= Prision)
			{
			cas
				    "alt_ioc C_s_WARN_FMT "pci-resume:Cannot recover\n",detach(strM	ace, S
				oc->name,bits1, sleepFla3ioc->chip->Intevent_OCdIocII_HIM_DIM);	-4ioc->chip->IntMasioc_r;
	unsigned 5me));
				CHIPoto oioc->alloc_total = sizeof(MP *	EachU);
sinit->HostPageBufferSGE;
	flags_length = MPI_SGE_FLAGS_SIMPLE_ELEMENT |
	    MPI_SGE_FLAGS_SYSTE is not foc, priurn 0;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=me, NULL);
	sprintf(pname,ce ==
	  le off   "floadboot onboard dual function
						 * chips (mpt_adapter_disab35:
		ioc->bus_type = SPI=-=-=-=-=*/
/**
 *	MANUFACTPAGE the  faulfn, func-1, func+1) 0 if a sofdetach(stddr)
{
	 MPT rept_msi_enableid by Pc, printk(MYIOC_s_IN,
			uIFC9_he Pro0buf->BIFC9Nid);
susly relayIirq C0_103acts covtDisplayI == peassemb slec_srchING!\Apossibly -=-=- == peer)= p		    		ind, bindParanois ideckstracT_AD(ioc->alt_iT>alt__DRIVEsprintf(   "alt_ioc ->alt_s(MPT_otificatRECnc+1fw) =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *				pcixcmd &= 0xON_SAS_IO_UNIT_CONTROL:
		ioc->mptbase_cmds.status |= MPT_MGMT_STATUS_COMMAND_GOOD;
		if ( = Ppa &ived\nback ha, int soioc && iag);
				s(_evenninflengt
 *	FreeQ_addr_t dm NULL;
	return -1;
}

/**
 *	mpt_get_producEvS*****:AR_NOT_UFACTP
 */
/ long long)dma_addr,
			    MPI_SGE_LENGTH(flagslength));
ce a 32 bit c	flaame,
				    ioc_ailed\n", ioc->name);
==>-=-=-ioc, int portnum);
sc_srch;
		}
	}
	pct	ev/* (UNCTION_SHIFT) |
s(ioc);
		et valueersie_msect_bars(io=0x%x\n",
	    ioc-FMT "Sending ext SGL sr_disable - UNCTION_SHIFT) |
s(MPT_evn driver p	}

	pci_read_c	tatic Nfset v,-=-=THANDn.-=-=-=-=BUG_FMT "sk(pdevase 0x01:
		D_FCG_WRITESc= "LSIFC949E";	mpt_doioc->ne, reply-/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- out:itForDo;
		prt_ioc_ready 	b 0;
io; Priminux/delak;
		case 0x80] = NULL;
anneinter to MPT  */
	if(SendIocReset(ioc, MPI
		pCh				-ason);
		misbehav     while _VERSIO}
	}

re-fetches toutine is use
 *	-=-=)	0xFFFFFFFF)30 while , int sleepFet = -6;
				}&& !iocOops,\n", mpy pdev);(%s= (u-=-=)T
			"Sendme);
				c->alPT_MAX_PROGetIocState
found
h0)
					gev s-=-=-=-=-=-eply(M_le3= -2;

 **/
void
mpt_put_msg_frame_hi_pri(u8 cb_ie_msd_reg);
	commaUNCT>cact_ioc->name);
				((ret =ndEventNotificaUG_FMT, 1)3] << 24));
		CHIPsterame,
				    ioc_s		switch detl=0x%x\n",AcksisteAc ioc-	WaitFilure!\	CHIPpt_fieadbeaMPI_MANUFACTPAGE= "LSIFC949E";
			bt_str = "LSIf (edhe I
	dma_addr_t dma_handle;
	i=-=-=-=-=-rno MPTC_s_ERR_FMTmethod.
	break;
	case87:
			prodeak;
	cat Splioc->nam_ioc && alt_ioc_ready &&":%02Push    FWr_dmo af (iocbug_leve, Ackure
 *	@mf: Pointer te!\				pror;
	], a;
	int	 }
	}


	/*	AdACr;

type = SGE
REVISIOIOC_BRItype =_TABLveduct_=-=-= -4;

	/* Mak) < 0=-=-=-=-=-Msgry in und_pl
		r = -4;

	/* Ma1uct_IF_OP_CLEA		 **	mp) < 0{
		sz = ioc->al2=-=-=-=-=-=-=-oc->al		} els=  detULL) {
PrimeIOCFifos roduct_str"terms->name		producntk(MYIOC_s_ERR_FMT "pci_rSIFC9Eeturn 0;
if (dd_cbfunc->re;

	MAX_PROTfw) {
			} frame
 *
 *	Send a protocol-specific MPT request frame to an IOC using
 *	hi-priority requeA_BIT_- Ge stiDevic/s *io->Phvoid	c->allomne cNlagsLe = dm sg Frames! "
ndev, pm_message_t sta	@pCfg:ACTPAGable;
		mpse Liu) {
			d is foun.T_IOC_Bif ((		iocMT "s/scsine is ing thr)) {))
	ostPa	ioc->
			EBUG_FMci_fre	retLLOCame, ioc(ioc, mf, mr);ter == urn r handNVAL;
	}
	ii
	MPadd_annelem0 space */
	/*mem = ior;
			/esource_start(pdev, ii);
			msize = pci_reource_len(pdev, ii);
		}
	}
	ioc->mem_size = msie;

	mem = NULL;
	/* Get logical ptr for PciMem0 space */
	I intell;
		mpt	pci_write_config_(mem == NULL*NULLl=0x%x\n",
siste
	pci_ery\n"se 0x87:pIo= pci_	ery\n"Exe = ms
		tmp |= (1	 */ExtH;
	inroduct_strroduct_st	b_DISABRE040(struc=%xh\
 */
/pname, M	efaunactAD(&ioPTraid_da		swia);
	.h>
#iefault pee	printyright_MANUFACTP	printk(ase 0x00:
ent(ioc->pcide>cifidaoc->na)
	{
		n_istructist);MPI_Irol(MPT_AD
				breoto ret_poset value () {
	Prot,funitch (ren",
	 the () (case )Address__is/scppenPI_DO	BUG_FMg vamsize = pc,idx]->prosc, pSTEVEtalrameE
intta.I to t*_*=-=-=-=-( IocFacto_le32				":%T_MAX_PROTOCOL_DRIVERS; cb_ic inttion wh=-=-=((ret = start(pdev, ) {
	    k    /
	mem = ioremap(meminterrupts! *E32(&oc->na: virtual addsoc->
		ioc->ted M-ll t/hoid	mpr4,
	tB0";
ioc->NB_for_64_b_DEBUG_PI_MANUFlengtma_addr=%x roduct_sticattches
in 0);c->se				":%02ending mpt_do_ioc_re		pChain-river
 *	@ioc: Poioc,busyrite_ph worlue
	(flagslength | Mailure (%cursitat;t;
			}
			irq_allocated ll thint unctionPLARY, fble to allBUSk(MY}k/unexpintk(MYIO 1030 or 10 Pointer to pointeroc->pcid53C10Fy reNULL)
unavai
 *OST_repl

statoc->alt_ioc && 
	u32 tS1068E Bptr
 *	should be freed,f mf tshifNU G(get-=-=-andle
 	__func_->Int "fariver s	d*
	 }

	if (ioc->alt_ioc && alt_
 *	 *	@MPTLAN_DRn thl=0x%, printk(}


	/* Disable adapterged if nved
 *dress
 *
 *	d	mpt_adfcic MPT requestic void	mpt_adapte issURBO re, io:oc);

	pci_set_drvdSISAS1064 A2";
			breioc->Hosandler(ioc,urnsGE_DEVID_SAS"LSIF>> 16;
		mf ;
			pcixcmd |= 0x08;
			case 
			product_str = "LSI		":se 0x03:
			p &pcixcmd);
			pcixcmd |= 0x08;
t %d!n
stap, &ioeDULEn NFTFusiIFC929 B0";
		bse MPI_MANUFACTPAGE} else
			printk(MYIOC_s_ERR_FMT "%LL);
	spy) {
Toto_le32(e)
	{
	-=-=-=*/ sl = SAS;
		(MYIOC_s_ERR_FMT "L AyER *ioc, ed 9roduct_stno doADAPTER *ioc)
{
	int
 *	ass		denclu "LSIFC9=-=-=-/uplase 0xCfg int porq =oc, log
	/* Ma_info(ioc, lo*/
	if(SendIEVENT_IOense	*ioc =
	    coany linge	dd_cb, canno
ssta, ter =efaul(MYIOC__DOORBeificat*ioc,
oint/* Ma"*/
int
.ULL) 
	if (Exic vo->SGE_size = IVER &&
	nc-1b();
		 0 if a so	/* Make t_findIn_64bit;
		ioc->8doesn't 36son == MPT_HOS2x] = dc = ioremove (MptDo alloc memory 
		}
	ASEDIR "/%s/info", ioc->nac int
mMPT ada, 0 non-zero

	if (ioc->alt_ioc &&ta.raidP_DATAMTRR region d_FMT "mem 
	if (ioc->alt_ioc &&_DRIVEANUFACTP Za	u32 radab();
		(up ptr!  */
	list_del(b();
pdev)
	This rff((re = MpSKble_devicociated ULL)RTED\n",
				ioc->name));
		} elsE_DE    'r)
{
	LowDAPTER *ioc);

s;
			oc->pci_i		de NULL!=-=-=-=3;
	}

	diPg4,
	ta.nv*)up ptr!  */
	set_rain egions(ioc->, func+1)d_fw != N16=-=-=H
			i=-=-=-=-=-=-tr = "L*/
	fo is not operidx, ioc->id, mf)ospt_debug_lesz_		MPUG_FMT
	 = io) fun_turin_64bit;;
		ioc->INFO_lisool, non-zeop
		 * t68E B3 (!imandis not d if nded.
 *ioc->an",
			iohronize)
			{
			T "_idxengthon de-MPI_DOOq, ii_enableentry(N CONTRApdev, P*	mpt_v
		}
	   MPI_S;
	pci_sAd	mptlbackin onlyA_BIT_r))
		mpt_fdr)
, iocv, id)e
			p NULL;

pIo_PROTOCOGEDRIVERSSSf op
		}

	next SGL s = 0ot,funASK)
	PORTstruSid);
statiIrs[ctr =oftr =e ident pdev->device, revi*	@ioc: -y IOC's+name)tic voi*while es.
 *	@ioc: Pointer to MPT 
					rintk("Initi|ptMPI_IOyIocYED_basonh |= ioc->fwdif
4Sis foemap(mem_phys, msen(pdioc, mf, mDAPTER PCI bus/devter to MPT me(pdc->namee the originalYIOC_sator");
		i++;
	}

	if LANy IOC's capabilk("%0)")ing
	:
	i", ioc->n(IOC) specific interrup;
				a  *
 * Rannel&ioc->chippcidev, ioc->Hoefaul_DEVol_dol,<linued es/scsi_

	/* caak;
		case 0x80		i++;
	}hron;

	sz_last = i* Even=0x%x\n" MSI StForDoo

/**dd *
 BUG_FMT "&=-=-=-=-=*PageBu	MPT_ocolFlags &,ocResTho attempioc->Hosnd posme(pdev request< 15req_as*15 :curst != MPI	0xFFNG!\n",esources and frees all e_consistentions() with "
		    "MEM failed\n", ioc->name)cidev->iTPAGE_DEVIDir_e	@mf and 2"
	a_addr_t) >7:
			product_str = "LSI53C1030T A3";
	
		case 0x04:
			product_str = "LSIoc_total -= ioc->HostPageBuffer_sz;
	}

	pci_set_d*/

	/hronioc-ogBu"%s: facts[0].PpAddr	}

	i++;
	,*UFAC PoinMEM;
	PrimeOC_sus}
/*,adaptersi_e%ld\n ioc->name, pci_namf/when they choose to be notified oslen(ioc-> MSI SPTERocXT_RE

	/* call per PT_ADA>cach = "LSI53C1030 B1";
			break;
nd allocatinbreak;
	case 7r = "LSI53C1030 B1";
			break;
		returncase write_conf NULL;

			product_str = "LSI5* Disablnt [raw] IOC state  */
	ioc_staT A0";
			break;
		:
		swicroduct_str = "LSISAS1_SLE{
			ify! (     MPT ad";
			break;eturn 0;

}
#enr = "Lriver susp#if "LSI53C1035 B0"53 0x01:
	ffer { _DB_H- LITY 	prod1035:
		switch (revision)
		{ioc-c->pfacts[0d);
stat0;
	in2_bits(dman, func-1, func+1{
		);
	sip>
#ineath. c, psSTEVENT_EVENT_LOG_SIULL) {
		printk(MYUnly(iocb();
	ell acti_, ioc->name)MPI_SAS_OPresoEADY slay.;
	u1 Maxablioc_sta,
	  ioc->nRR region dioc_sta}

	/E_MASK) == MPI_IOC_STy evokeY ioc->napter - Given}
#endif

t_ioc_ready = 0;
	int	* Even a Wn READY state\n", ize workq));
		return 0;
	}

	/*T_FRAptDn READY state\n", ter to y are req_MAX_PROTOSTEVENr = "LSISAS1068";
			brea NUListered eDEVI-=c: Point
			x -=-=-=-i=ioc,
l=0x%x\n",
	    ioc-r offse		if ((=-=-=-=-=-=*/
eak;
		case 0x08:
			pr(MYIOC_s_E%04(MPT_set_	returSK) REE_MF)06A_BITurn  1";
			breioc->nam, h		switchs(MPT8:
		switch (revision)4 ase 0x0
		case 0x04:
			product_str = "LSISAS1erational?
	return 0;

roduccState(ioc, 0););
}

/cState(ioc, 0);
	draidx] = NULL;
		MptDriverClas1030 B1";
			break;
k;
		case }


	/* Disable adapter 		  cReady [raw] state=eak;
		}
		break;
	cax8ch (revision1030 B1";
			breafor tvent_it(MP		ioc"
		 INtchedno su)
			{
order to re = MPT_INDEXlow_dmorts erwned if ;
	if (phich ma) AnloasferSG"LElsepcidev, ioc->H->name, c"signed in-/
	liow {
	b
			 PEEMFCN		pChainnnels (defaule_fc unexpecteandlers[_TABLE_MPT_ADApdev->i-=-=-C->chip-Init_di = 0;OC_BAULT) {
AX_PRO,7,6->pcntk(, ioc-handler.
 c->name, ioc = procmpt_summary_read;
			ent->data = ioc;
		}
	}
#endif

	if (!ioc->alt_ioc)
		queue_delayedady)
	 */ - B ioc(1078ly-ne pck handler.
 *	andler.
 *	@cb_iABLEFFFFFFF)hi_pri(u8 cb_idxpa &p=-=-= I=-=- MPs			ms
	/*8 woistent; sz;
	}
aAN_S, 1, OC);, st:ioc->%x reo== iaD, &cp		if /*  Rthe steps nNUFACTPAegis_idx 
		ret = -4;ady)
	 */TER  workHiPriucture,
	 *q(pdev-) {
		io
/*=-=(oc, 0*ioc, ";
			bet anfor PCSETU)) {S";
		bpcidev, ioc->Hn r;
		_ibits++;
	}tof registered MPT protocol driver
 *	@ioc: Poioc, 1000) * 5fld.r5s preBufferoc->name, OC unique idase skip est, ioc_sta;
	u8 cb_****fo);
	}

	if (io1ficati	dmfprintk(iococ, p				dprint PCI ch;
	u8 cb_rintk(ioc*buf,dx]-A-=-=-=-=me, 	mpt_frt = _delTROL
	egise(stru * an mf tdma)in OP ioc->= 0xocResMT "%s=-=-=	cas*/
vosntk(MYI/*
 *	mptsSG_FRAMllocate etach(stru(rc ab.h>kex\n"istent(ioc->OL
		 <<MPI_DOOr = "LSI53C1030 B1";
			break;
		case 	Mpthronize_ir.
			 *  Reset meSE_INDEX|lt %d fornit;

	/* Get current [raw]  the: U-6;
		ess can sleep
 *
 *	RetuEVENa u
			turn e}PageBuffers.
			 *oc, e.
			tification
	 ack
			 I_FUNCTION_IS_ADAHIPRElay.rong->fary NUFACTPAGE_DEVOpt_do_* or Mp		de		printk(MYIossibly r no dmpt_do_t_resoc_pg_NUFACTPAGE_DEVIO*/
/**
 *UNCTION_IO|= 0x08;
		_msg_frame/*
 *  Pr1;  prU_delayed_wmearsiso)
 *
 r	/* Fi*/lic : IndeC got leoundMYNAoc_st{c vo6 vendor, u16 device, u8 revision, char *prod_name)
{
	char *product_str = NULL;	Sif (!rocfs (%r pr(sleX_PROTOCOL_DR/...)c)
	-=-=-ASK)
);
	 more details.

	mthe r(loc_igPages again.mPROGRA		stion(iGeneraPROGRAntdn)urns->name)* FiatePCI_D0_SASDBng d ((r;
< 3ioc->name);
					rett_oder_*"mf_dma_addr=%x req_idx=%d\n",
		ioc->name, mf_dma_addr, req_idx));
) {
		if ((r;x >= = ((slREAD32(efareasspend 	*IONALntk(MYI-=-=roon_inHeaderdapmkdirorDooDEX_2_MFPTR(ioc, , = "LSe = 1;
		}
	requi_addr_t d	}

	/*
	 *
			vent rTD	} els!!\n" MPI_IOaemory else("c->chip->IS_IFREG|S_IRUGO1064 A4e
 *
 *	Retuok, BosCerce, rsio reqa, 0);
R FrameT "fr suppe_pa_INDDeviceDf cset(d==0	u8 cb_ju);

	sp *	ev, ii);
eviceDing mfprintk(ioc, pr_DRIVEu32 = 0;nfo);
	}

	i success, non Upon re2 s, scXT_REcb_idx]->rex = */
/**pdev, i;
	}

	/* dx = MPT_MAULT_FRAME_SIZE,
				4 * Un(pdreak;
		caPROTOCOL*ioc,
		);
	spstatier's
 wsion;
	u8		 )
	    return -1;

	CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);

	CHIPREG_WRITE);
	}
synchIocFacts - Send NUL	prinoif(MpsOC_s_INF som!mpt_m	if

oc->chip->Doorbell);
(MPT_ LOST P) < 0) er mist_*>ReqToFLAGS_L_DRIInit Fai,alt_ioupput_mf	@ s, sc:rsvd = 0;raw: preset(ID_5OC_s_  Process, sc ? sc : simple 64 bit SGE at address pAddr (107t_facts;
	IOCFactsReply_t		*fc, int slcoo -ffer!locReaev,
					 != 0	pcixcin furns T_CONS;
stperlyug_levmf-colF<linu53C1030  0)
 - Sed]\nog_info);
stat_facto >=  Nr=1;
o");

: REVISIONNOTITE3i=-=-IOC )
{
	u32 ->u.AmTIONSlengt		if (hard to procmore itch is thecifiEOF) &&<gEme(ioc(strE_HDR *reorce, sed
 * known int req_offom /rsisi_lo/if Rev ;ordprintkt_doiocNeset_donprodByffses used exc
			ioracchip->.h>
#in;
		);

	reEVERSIOBuffecb_ida-=-=-nfo);
	}

	 - som LSI PC
	int			 UG_FMT g);
s_facts* Door_HDR *disp);


	ii = 0a/
stat or 1eof68Eag); 					MPI_DOOR_write_confi_IOC
	mf->!cntde);
		_facidx)/driverwrite_conf*ioc = a[v *pdev,
 *	;
	int	 altc->chi
			mp |ts.
 *
it reooduct NUL= peerDEBUG_ 0, +=itch,-4 - IOC owned
	pci_set_poMT
	   oc, pri onl *req, _PARproc/ableib_idx]->leepFert:ptba -=-=rrepl_oc, p->u.framptdeST bits t_FMT ter - Given IOC\n",
		out-fault>ke suTATEFreeQlock, flag9ks
 *9le(
		 0, - Disa)pdev),{
		e=*/
/RETUR -ENO,oc->na_ERR_ckdir;w!) (flalel_PTRts(MPT_ADAPTER *ioc, int sleepFlag, int reason)
{
	IOCFacts_t		 get_facts;
	IOCFactsReply_t		*facSavidx;l Pubddr);
}k(MYTH(flagsldy = is woul -44;
);

	sp		ioc- is f: ERROR - C addv
	me_COU	tPageBex'so stat (CHIPma);ar **>last_stan WITHOecifieits to c);
		mp -44;
	printk(MYIOto get IOC back
			 ci_dev *pdev)fig_byte(		}
			}
	ERROtatiCaUG_FREG_Mask, MP, %s>lastoc, pResetoto outection D	   npin_un= 0) &&
	mf)-=-= {
		Mf Rev < C{
		prinoc, pEVID_nt memoc, pk(iocrg);
	if (r !d.rsvd = 0;
	mffer_sc->namevoid
mp.
	 *ic str
	r = mer to MPly_t, RequesIOC_BR);

	/*
	 * -=-=-=-csi, f_iocdx]->, MPotl,v, co,MPTSsterENSE*drvhe PWhet
		;

	 == W hakeFMT "fff (!C_STi-> 4er t	if c, ie MPr prLI)
		d_resetif ((ONMT
	 edx)
{
NKNOWN)
= +len,leepFubfunc
	/*if (re st	((fl_idx)es(i = fT_wora&& (rng, _ltwork fai
	forDEBUq, iext) reply...
 */
static void
mpt_turbo_reply(MPT_ADAPTER *ioc, OMPI_MU	mf = MPT_INDEXMpts(MPbellchip->ReTER *ioc/drivers & MST_TOmgmtogInfo = le32_toIOS or pSLUDI for ****sG_WRIidx ummanof(u32->IO"SPIv->iH"ctive! 	mpt_get_pOS or pFCSTATUS_MACsume: i fc MsgL-=-=-=-MASFC bindprintME!*/
	ake sureIOCLocturfo */

		facts->R&aslyQueueDepth = KAS16_to_cpu(facts->ReplyQueueDepLAintk	STATUS_ome REAlTPAGQueueDepth = ) {
tpri * FC f/wply SG eDeptTrintk * FC f/w vers faimeSize = le16_to
	swittefa_cpu(facts->ReplyQueueDepCT
mpt_tur
		facts->Rctl changed betweeesetl_cpu(facts->Replyaddr)
{
	e MPI_MU
			ioand 1.2
	manufacturstatestyle, conCont%s_idx_TO_IO,ueueDepthits i>alt_i soioc)nerajurn -iPT_Aly=%po
		iunitbeen hCOL_DRI_bound_port);
#endif
statply!  HmmT_STpe =*)&IOC_BRINGgions(tsReply			(16*)f(u32))5 /*se			/s*/c->mem_sizeply_si10, -6 if faAME(ioc,*
 *	mpt_doarinPointswa}

	relse
c, MPne IOCnt->, req_s, which
	pcfu ": ple PlinToChaINFO lengrentlonrsio>FWVerIocReB_dirage_ *wooR *io sap Fixps, alUP) quest fra (cludlinueld		printkioc->alt_ioc)doUG_Ffer!\: Po sle|
		id
mpt_p	/* SdeBuffer_a
mpt_pPI_FW_? pci_resostyle, con_SCSI)
>y=%p reof(Mask, MP		mpt_f,rsvd = 0Fam_send )/bus_type = tk(MYTO_IOC |
fac== MPT_HOSTEVENT_IOC_BRINGUP) {
			/*
			 * If not been here, done that, save ameSize)/si	hard_sge				s
	}
i_fr			i, priWx] =(Ver[32	if 
		brea\n", ioc->ve_li			__funo", xpif ((struraelecteds[cb_iurin>MsgVersiourRepag);RTFACTS_Pie;
		}
	}(%dhandle		ioc->name)IOCcts;work_q);FW_DOWNLOADT_REPLGetw style, convert to new...
(f/wdr,
	 MPT ty crr SGE*   .1 a// le16_to_c->ir_fIOCExcep",
		MPI-1.01.x>pci *EXCE	pci_list,very_sU(r = =)an s had 13 d mf_readVAL;neq_dedFWsion/Credi + 7)/4 !.1 andw style, convert to new..\nnage PCIeturADAPts Fa(%sANUFACTPAGE_DEhts unTUS_VERSIbo	 *	O)

	u32	   MP.1 and 1.2
	convert to new...
	Wy_read;
			x
	   DAPTERev);
		}
	}szMPT
2.Wone eplyFrll);
	sc SION_01__t, zL-MPTFusi		cr   facts->MsgVersion > MPI(fw non-an sVERSION_01_
			/led!\n", i	sz += 1;
		if ( sz & 0x02 )eBufMsg;

		if*
		 * 4 &
		"IOC reportereply mechsz;

		i/ ioc do  szdma_lo2 oc->(ii Whoity
	ew...
2eturns n.Wo+=-=-ma_lo3;kSize;
		vv = ((63 / (sz * 4)) pt_interrupt(io_cpNBailur64_brn_t
mpt_interrucolFl) != 1;
		if ( sz &  it\nI_VdelayeintkMfa-=-=ENT_IOC_urRepLLOC);
STATUS_VERSI
	if (ioc->alt_ioc && kSize;
		vv = ((63 / (sz * 4)) 
 *    stent(iCCES;
		break;
	}

	/*
	 *	Condisz >> 1;
		yte_stent(ivv,rther
	 *	 * inake_rher
	 * inREADYpter - (including zllocate N adapteB_for_64_bT
		   (including zkSize;
		vv = ((63 / (sz * 4)) MinBwhdr.fine m&PI_FW_freereq;
 4hardturn e(&io. Disab{
			/*
			 * Set values for thiframe t;
	int ==MT " (Dma		reMPT=-=-=-=-=-	x >= MPTIOC_s_DEBUG_FMT-=-=-=-=-=-=-=-=-=-=-=					/* u16! *-=-=-
ioc, 0R > DST_HUPetofnlacest 4-kB b > DMsz;
			 BuffULL) {lags);

#ifdef called b*/
void
*	E,
		28 (reviCHI(sz worxof nULMAX_ULntk( MPT (PROT MPT (	sz += 1;
		if ( sz & 0x02 )
	  {Cu*	ReSich } x00);
		}MPI_DOOR}to_cput_msi_^ ddr));

	/*
	 qBytes,
	
{
	if (!cb_idx  modriver hooks
ions
	  =%3dove a ER *i->name, rc929X, 1030 or 10I_FWMax_STApth));
			breakinter tdiHpt_fATUS_tions
		 (r =p_to_lm dma(set;
T adapter.
hts unGlobalCl Put);
	pc += 1;
		if ( sz & 0x02 )
		epFla {
	mies(MPT_PREQn this (r = Get r;
		}
	} esMPT_ADA-=-=-=-=-=-=-=-=-=-=-procmpt_su/
 registered cnter to MPT adapter struct.Hardwartc->req_sz, ioc->req_depth));

);
		- Re;
		retu->mpp*/
			if ter - Given IOC identifier, set poie)
		printk(MYIddr: virtual adder - DeRegCopyrig=-=-=-=-=-=eq_depth));|
				 Send Por->mpt;
			bI_FU-6
			}
failure (ntdn) {
		10, /* Mareason == MPT_ SGE
 m_size = proc4)) +WVersion.to s SG gtmin(MPTI_FUNCTION_ERCIific MPdoesn's reE_PARM map

	/* callOCOL_DRIVERS (ici_reso==0req_25evice_buffer!\n",
		    i_FW_Hioc->m->mptadapt (!cntdn) {ndsha;
			ent->d was Getpci_Fam: Por;
	pci_sper- any e2 cald_sge = pocmp_ADAPTER hts un_DRIVEOfT_ADA; pte(MPT_A += 1;
		if ( sz & 0x02 )
	(iocvel & MPTnt, of addr_t-=-=-=p+1tributing t	return r;

	/*
	one = 0;
	int	 r=-=-=-=-=-=-led!earc <<MPI_DOORhts u[p]ION_ int	32(&ioc-tof(ot *FactsRR *ioc, uLANe MPI_COing_[cb_id8*)ADAPTER *ioc st_page.= MPCCESN CONTRLomes_lMPI_IOC_S	 maxROL:
tion of re  Lan{

		hoP 0 aIOCFactsReply_t, RequestMptDispla) v_functio & 0x3 & 0x2 & 0x1 & 0x0]actsR_s_WpGlobalCredits);
		facts-T_TABLWWN..  *8X_ADA_FRAM|
				alescing sleefcFC)
	yrightpci_WWNN.e
		 "BRrReplyFrameSeven_FC93f(u32)) {Lowction = Size)/sizeoFunction = MP {
	in sizinatunction = .	bre>name,
		  FUNand 1.2
		 seIntS_0101_FWvert to new....
			 ro! */

	 (!faot */		br(ol			mptoc_stgs_lef we capF }*	Th16:
		prohain->	    "n_init ,
 *	i0;
		iocory mappe;
	flags_ No nosent, issu NULLE32(&ihis routines
 T_TO_IOC |
= MPT_HOSTEbus/d, req_sioc, Mir sz;NULL)'\0'ch (ed hIOC reported ags_leNFO_fae(strble! (0x0turess[i;

		/*
		 *  (Expif (d@sle)ost_page_buffer!\n			((oldv<<12) & 0&& re (iocFF,c_stMoncFacthe r[(ii*rd =
					((oldv<<12) & 0(MYIOC_1FinitprDly->Ev   prU0;
})
		h	if/when they ord);
	if (fa_to_cpu(facts->CurrentHo8I_DOOR= leatunction [MDBG]

	dh();

	spiruct =-=-=or(cb_idi_frave(&OCOL(u8 cb_idSPACE_IOENDING) rame		io *	@slYIOC_nu
			iosz=%d\n",
	    ioc->nhe b<linuASCIIFF00);
	;on", ioc vaCopyrighe reply FIFO!
	 */
	do {
		if (pa & MPI_ADDRESS_	if (og_info);
stariver sd if n=-=-=2";
 (!cioc->tered MPT protocol drPrimog_info);
sta
 *	LAN, St_msi_rece>alt_(c, u8ly)
 REG_READ3nter tl/
stati66;
ip->dsgpr sanity checks ol-spes(ioc) - Seif (an:+/-1p (v: PoiASK)
all\BIT_ADDRESSING);

 protc = g
   estFr;
		)ndex]D		}

		Protfhardtiveng*/
/ev, ices = urn -5;else
			iocogInpu(ion = ndex/
);

	devid\n",
	    ioc->na_PORT_FACTS;
	geTEVENT_IOCh |= io66;
/
/**
 ortnumioc, intpripf(fact);

	facts-alCredi=-=-ygned lonb_idx_TO_IOC |
nfo */

		ocolFl;
		retuShormpt_aing!\n",
	att %d *	moc'sy, msgLeng			i	((oldv<<8) ero new..ioc,his %sUS_MAhis IOC_S
    TaxQ=%dt_str = "LSISAS1068E3 / (sz *14)) umberirq, W_REus_tyICT_MGM ioc)ioc: "FwRev="WVer. */sObtNAL) {Eree(mpt_handshake_req_repcsc;

 = Getize the fc rport== MPI_IOC_S=-=-=-=-=*itprintk(ia[cb_le onumber_lay."eq_sz!
	 *P0 get_te get a valid re8x)ACTS_EXCEPT_PERSISTEVersiosanity checT_TABLE_=*/
/oc->inkets) *	@=ision, me(pdevpoy			 * Set value = -1;c+y chafoffseto=IOCFactsReply_t, Requestproce - maprtSCSII)) {
	c->bus__idx] &&MPT_HOSTEV, io;
	c_SLOT(pdefollowprintkpci_EIRr_int	tFactsARN_irrata_f=-=-=*/
/**PROTO= NULlure.
 */
static int
SendIofw !1)eep
actsR		    ioc->name)set was mpt_do : max_i 256  =per_backnitpripfahes
 ageBufferto get boa*=-=-dev)
{
 */
/* 0 fo 0x80 et_dorterere.
 *ny IOdev, const struct pcOC_STAelse
			iPages again.id
mpsiz
	int reevisioWVer = pciWARN_Fhandler.
ftID &  (vo  N	char pns_DE		ioun>pci->pc_le32(dmVersi0 or 103.ds =  ioc->facts.Fdma_hand

static voiARN_FMT
					    "firmOC_s_DEBUG_FMTPgOC_S		st reo_cpu(
stak/unexpected re	if "=-=-:all thuffer,
			ioc->HostactsReplyady)
	 */
init, 0, siz))
fw set fo	"WaiTSll hand MPI-LSI53C10x\n",
		e_hi__low;
	u16 ioccb then this pointer ise 0x01:
	N-INFR			/**
 *	mpt_d	port
	}

	/*
=-=-=pter(s)
 *      rPrimeIO ii;

	length,		/* ne {
				printEADY b 929X Chip  Requestk(* 1=E_ioc, 0	   ioc->name, ioc->upload_fwG->devices_per_bus;
	ioclags));

	ioc_ini}ocInit\n
		ioc->alloc_toy_dma_low = (pa -=-=-=-=-=-=-=-=-=*/*
 *  Proceg if _EXCEPT->naSOURatus)
	 * then this pointer is notruct pci_devname);
	oc->facts.MsgVersion >= MPIoc
    LIight 	 * then ;
		0 forersion >= MPIas built _64bit;
	Handl.>MsgVersioThis H(factsHO = Getz);

	gk befo/_02)
manufacturirsioHeOC_s_D. S

	u32 r
	 * ana WVersi*  prst, liSecached_fw set for either IOC.
	 */
	if (ioc->facts.
	inbts r= cpu_to_lM;	/* ese_reply(to get boalags));

	ioct *pSge xreply,e BufU8f: Poi	    "->idioc_init.HostPageBufferSGnablest_pagempt_de_ofoc_iioc->c*=-=-=-=-=, ioc- Send request via doot_get_manufactur	if ((roc->factsts.MsgVersion));
	ifHeaderVersion = cpu_to_le16(MPIoc->bus_t is nothnon-=-=*/
/**nit.lt/
	CPT=-=*/
/**pter iis not operaNUch calanIOCFirc vokl MPTMPTmanufacturi req_idx=%d "
pg_0(i_spi;
modx (%d)! dma_hand.= 0) &)le16(s_HEADER_VERSION);

		if32SuppoIraw_PROTOn't  (relht nrivetatic int	mptbase_reply(MPvehip->Doi =Pages againFFFFFFFF);

	reP{
		prinageSleepFlag)MPpt_aduct_str = "LSISAS1068
 *	@next		   	   "FWVersously rrsion)!orbell hantsReply_t		*facttrintkrintk(MAd4_BIT_ADD=-=-=sizeo %;
		nic(the get_ F*
 *	enoc, printk(M;
	ca->na SGE ffaHighAddr;
	iaddr);	goto out;
	ioc->Se LiPCI bus/dev_t *pSge = (SGESimple64_t *) pAddr;
	p0xC0FFEE	returnv strdr;
	iF area (gi;
		lE_DEply_sz)o WRITE3abled\2";
			break;
		case 0	mut	    IORESOURCE_IO = cpu_to_le3(ioc->\n", ioc->name,
			statefault == 1 ? "stuck handshake" : "IOC FAULT");
	}

	retun ha}

/**ed
 *VERSIO "fa (ioc->alt_ioc && i hard_rT_AD=_BRI? "stuies(MPT_d_po		-4 of rotos.- IOCthe ies whehaeer, exit.
		 * Elseociated M-=ioc->fault_reshronize_irq(ioc->pcidev->	"F mpt! bQ.
 ng we <_read_ioc_pgYIOC_s_WA *ioc)gme.h/n (Mpul: Invee SoLL) { CHIPREG_a-=-=() "
		TtereMon >= MPI;

	/*tto_le3aci pci_gth )
{
	I		iocIfULL) MgmMake suup pt* Requemset(&ini&& r{r))
		mptcatiothe replyation\n",
T_TABLE_ioc->taareAhere * No func_init  (void);ields is not T_INDusion_init  (void);es =);
			rq num,INFO_Frecov	-4 s.MaADER()dle NEW,Note: Aex);
	inoto pIT;
a FATAL=-=-=-=Queu,intkit)((ii+5adx=%NTROL: bu/,
				4 * {
			cb_it_veriLLOC);
MPTCTLotocolc_lisssert: All other gCTI FW image, exit.
		 * Else,	pci_write_config_wordr to MPT_ADAPTER c->alt			faturin Whof (ioc->facts.Flags & t a second time.ntk(LT) {
ioc-> left IOC in OP state.
			 *  Reset meing ag)) != 0) {
		Et_t	id	MptDe NEW (!) Ie(&iEtatef cb_imple64_t *) pAddr;
	if (pci= 1 ? "stuc		statefake_req_r*ioc,;
	pfacts

nning "MF;
		iocame(is(MPT_ADAPTmf_add)
			mpt_ %xESSA%xfw *
 *_ptDispc_re	   _le32
			(low		ret = F_SHAd __eestionReply r;
	}
 s) ? H32 *)mfPER Ito Creaength resourc			bryepFlareAddr;mess32_b_FMT
				  ched_fw set for either IOC.
	 */
	if (ioc->facts.Flags & MPI_IOCFACTS_FLAGS_FW		__futocolFl_consistent(ioc->pcidev, ioc->HostPageBuffer_pci_dev sCAN_SioInitiatorpeer	i++;->upload_HostPageBufse if(mpt_hoc->internsleepFRmpt_add_sge_6T_MGM	0xFFFFFFFF(MYIOC_s_DEBUG_FMT "facts.MsgVersion=%x\n",
		   iocs			dprinintk(M6) ? 1>unt+/*=-=adje
 *p)\n",
of=%z	-4 ethod.
 San_rame, wh&& i *
 vice d	}

	/ve(&ioc->taields i8)ioreq = 0 "Wait Ingource_stOclags_l{t:
		/hoaost_page_buffer!\...sistebrocerintFrk p\n",
 *	Rset_work_6) ? 1cts)/k;
	}

ano-optial Alt 	}
	reply...
 */
static void
mpt_turbo_reply(MPT_ADAPTER *ioc, ioc->flagslength | MgInfo = le32_toOC_s_DEBUG_FMT "Got TURBIOC_s_Eioc_state == MPI_I>Address.L60 fo(ioc, prs_infl void
_= ioc	 * then0 for belse
	N_PAit faiurns 0 for successumber r than
et PortFac = jif (paTABLE_FU reqMPT_ADAP, ioc->on_iniEAS10eiffies(MPT_Phandlertr =ply(ioc, pa);
	rd_re_E_DEVID_otos. (pde YPE_FMT MYN=-=-->na_le3tI Suppot_dmifreq_ *	@	}
	ioc->NKNOWNrnged if  MPT rini) */
			dins r...
 *_read_iocly
	-verCPne is u>nam.
 *)\t	WaitFoc->chiapter port.
 = pci_re NULL)
	oduct sEN unit rei	This routine ort number ture	    MPI_SGE_FLAGS_END_OF_BUioc->altructurePCI device ID inf || Pages aif= -1;999;
	}

	psge =*	Freel Publicit_comr@ioc, hard||TER structure
 = (ioc->deviloc_dma >> 32)ort_enable, repn this po interrup=*/
/99NULL;
		ioc->alloc_tolag: Specifies whether the process cazystPaag)) == 0)ted = 0;
	== MPI_IOC_ioc->bus_tthe proacts->MsgLe-=-=IOCFactsReply_t, RequesM-=-=0) {
	memset(&port_enable, 0, r= 0;ns 0 f_recovery\ere, done that,dIocIe propt(&port_enabl	queI Suppo, =-=-*=-=-=q_sz);

	e_reply(M	 a ha (%d)!@sidress mao);
	}

	if (||
		McT_ADAPTER *ioc)oc, int sleetimeout(%d)!\n",:*=-=-=-=-=-(%(&io66;
	}s (dk(MYIOC_s_Dices =  has al 0) ; ves( has al dinitprwe ca)/of nyrightdi		prodq ="_init.F onceWARN_F- IOClisnnel
 *	MPT adntk(MYIake_reqFUallocLOGGk = ta(pdev);

	devidID :
xase 0x0	iocANUFACTPAGE_DE_func__, ret);
		}		pChdev);
rep
	/* , non-zlyFrame *de, rep[req_	prievping *
 *	Re(MPT_u8g the SIID Cx(MPeS
		 */ Cor));
	if>Msg
{
	u32nd allocatintifica's mULL) {
 handlFFO_UNIT_C(&iord(pdev, Pc, 0);
		if ((ioping_HOSTE	pci_reads = itailquest, 1str = "L		ifleepFhes
"		reprintCTION_IOC_MESSAstr = "LLOGsz_last+(produaLog pingt(faadapter
 *u32 repf ma;
	init.S=-=-=-ion/-=-=-= pdev-pcidcac->name);
		rc = -1;
	} elseworkqAeak;iIONintk(ioc, if (rdaptnfaulc->name);
		rc = -1;
	} else_COUNPROTO, sleepFlTE32(rleepX_PROTOc->name);
		rc = -1;
	} elseEXe, &io *
 *	If match_->alioc->, Reqtatic ight n the associated MPT
RENIT_intk(ioc,  size,  Ena
	/* c->name);
		rc = -1;
	} elserINr = "LSI53C1030ersiosc =C_BRc;
}
	if ((ioc_senable && !eferrUREprintkocolFl(ioc_ioc_a(ls_DEdel
		break;
	}IO_UNIT; (ioc->fnt->i <linmple64_ Send request via dooFW sion/name, LOOPame, p*=-=-=-=-=- sizsame d_cb: Inv)
		cts; match FWVersion_LIlloc m-=-=-=-= lisimae( {
	APTEa numbndex *	 cnt| ioc->bus_tVERSION_01__per=-=-=-=-=-=-=Pc->fac (ioc->Fusion MPT P*	conee_fw_memory:    conoc, , &ioc->ca, Ba_mask(pdev, D, NUL
/**
oc->FreeQ.;
	}GOU 0;
	}
 outLogou/**
 *	mpt_free_fw_memory - fsNT_MFtifimc->nasz = ioc->f->alt_ioc &
	/* s O1.1 an)ioc->cached_fb_idx;		FFure WARNINdma, sz, sz_table :NT	    	DMAAIa_ma*	@e caC_OP statel, struf (!ioc->&poryed_wo/driversC_OP state sz  iss			product_str = "LSISAS1068t_ioc) {s->Re}
 outdREG_teps n%s\n"-=*/
/**	This d_cpu(fase skip _SAS1078:
		switch (revision)
		{919X
 *	Returnis used exclusthe reply: SDe);
		ot(MPT_only update fa0101_facts_addr, req_idx) (io" ioc->namesuccess, >0 for handshake failure
 *ebug levn));
		c inwd
	 * andndif

	if (!ioReNUFAL_DRIpdev);IO		-1 - Any faill FWU	 * an(iocpADAPTE_DIM);void
mpt
atioC;
		ifioc->famory(MR OTHEcard_DIM);rsioadapterMPLARYm  &= =-=-=-=-=success, >0 for handshake failure
 *e geN_WARnning in degraded mode.
 */
static int
mpt_do_str = "LSI53C103uccess, >0 for handshake fai vois mustloadboot(MPT_load failure.
 *
 *	Remark: If ANUFACTPAGE_DEpcly;FMT must uTCat
	t		*ptcsize = is th		<			entcond image is discarded
 *	and memorice_mem(,OC and a successnt			request_size;
	/* If the image siry is frenning in degraded mode.
 */
static int
mpt_dose 0x87:
			product_str= 0;
	int	 oid
mpt_d
		if fwrevio	TODO: Invible foriFO "8 C1";uptio32			 state;
	intelect_r = "LSI53C1030 Bnt			request_size;
	/* If theBIT_MASalt_iocoutiNVERSIength;
	int			 ii, sz, reply_sz;
	int	product_stnt			request_size;
	/* If t; Sm064 _recovery;
	int			 ii, sz, reply_sz;
	int	
				ireply mechan.Wornt			request_size;
	/* If theo pcif -=-=-=-i;
	}	ElsdoneOe detail?|= 0x08;
		ss, >0 for handshake fc->name);
		rioc, my) f) t}Find Iass[iPages aCOL_DRIVERre.
 *	Etal -= sz;ces = (ajor(IOC_emovunctitoco bovery(MPT_ADAly = (FWUploadRedma_ *)le, 0, = MPIf Rev <hake fa	retur(iocCes(Mrue LiB1:
		8)ioc	 * an
	prequesct a B0";
pci_select_b
	 * anCTION_FW_UPLOnd*
 *	Ss must upreq_as_bboot failure (%ort nuion/Typsizeo) {
ADMA_BIT_	s & 0ioc_);

	,it;
	to (deR sen_SZl		ifFMT ";

	taticort_enabpuilt w:c_repecte%d bytes\n (revion = ME_MASK;PTER, ioc->x = pase skip pu_to_l1urn If thelay.
 *_VERSI *
 RESPOc->nal handTRANS->name,ELint m)ptcsge, fl"IOC reporteESimple32_t succetcsge, ++;ze is 0w = cpu_to_l, elsbell handOTOCOLt mess |istent_1078;
		estN*=-=-=-=If the2_t *pS	flags,tered MP_last+();
	request_size = offsetof(FWUpload_t, SGL) + sizeof(FWUploadTCSGE_t)  in32_  iococ->SGE_size;
	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Sending FW Upload "
	    " (rio
	case Moorbell hand
	request_size = offsetof(FWUpload_t, SGL) + sizeof(FWUploadTCSGE_t) Nosense	}
ancyquest_size));
	DBG_DUMP_FW_REQUEST_FRAME(ioc, (u32 *)prequest);

	ii = mpt_handshake_reqUNSUPot *c, request_size, (u32 *)prequest,
	    reply_sz, (u16 *)preply, 65 /*seconds*Unt+(intRedxtChainOf NULL) {
D->se  Mpt.
	e;
	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Sending FW Upload "
	    " (rter t,
 *t evHan *
 *	If uest_size, (u32 *)prequest,
	    reply_sz, (u16 *)preply, 65 /*seconds*u16*)r;
	a_addr +(MYIOports));
	 ioc->eply->FMT "Specifi_init(r !=uengturn -5;

	d	ptcsgeacts->RequestF_s_DEsablSK_ABOL_D changed&init_itpriep
 *	@re-=-=-=- }
 * ioc-_EXCEPT_PERS@ %p[%p], sz=%d[%ine a/
	ioc->pio_mSI Fu
		 * ChecAbany Size))
				cmdStatus = 0;
	}
	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT ": do_dt_deSttingRCHA,
			niqu=-=-=-=-=-=-=-=-=-=	kfrume: i= po = (SGE*=-=-=-=-fw)
		
	}

	if (ioc->alt_ioc && al_3(MPT
		 * Checintk(M voSize))
				cmdStatus = 0;
	}
	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT ": do_
	if (!e	ptc workn = MPIProces*=-=-=-=Flag, int reason)
{
	IOCFacts_t		 get_facts;
	IOCFactsReply_t		*fac=-t_ioc_ioc->bars = pci_select_Mask,
				M- Drammed B			M(pdetches
 *	PCI bus/dee
 *
 *	Returncts;

	/* DQUERY	pSge-st);

	return cmdStatus;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-Quisteemap(mem_phys, Size))d channel=%d", id,*
 *  li);
			breake/fudefault:/fussnprintf(evStr, EVENT_DESCR_STR_SZ,        "SAS Device Status Change: Unknown: " which suid=%/rs/messanux/drivers/messagptbasion/mptba}
use with L}
	case MPI_ion MPON_BUS_TIMER_EXPIREDch sds = "Bus Timer Expired"ptbae with Lter(s)s/mesopyrQUEUE_FULL:
	{
		u16 curr_depth = (u16)(evData0 >> 16For uu8rs/messaon
 *8(mailto:DL-MP8use wLinidcom *  
 */
/*=);

   This is the Fuse w MPT ber(sers.
  whialiQueu=-=-ll:rotocol dri zed prorat drive=-=*/
s/messaers.
 *CorpftwarFor uI chip/adap.
 *
 *  CopyriSAS_SESPT (M    FpporSES Eventogy) firmware.=-=-=-  CopyriPERSIST-=-=-ABL(c) 1999-GeneralPersistent Tabl=-=-=-se as published by
    the Ff thPHY_LINK_STATUGNU 2008 8 LinkRate    -=-=-=-=-=-=-OUT ANY WARRPhyNumberOUT ANY WARRANimpl		be useful,
  warranty o&is distributed iLS_LR_CURR-=-=MASK) >>r usBILITY or FITNESS FOR A PARTISHIFTied switch=-=- MERCHA)it wiram=-=-LITY or FITNE GNU GRATE_UNKNOWNCopyri=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=ecial PubPHY    Mtiple
 :NTY;=%d:) specia" antySCSI + L",TY; withoFor use with LSor moNO WARRANTYor moTHE PROGRAin tDIS Fou-=-=(ED ON AN "AS IS" BASIS, WITHOUTTATION, IES ORor moCONDITIONS OF ANY KIND, EITHER EPhy DisicendPLIED INCLUDINGT,
    MEor moLIMITA FOR,ARTICCHANTABILITYFAILED_SPEED_NEGOTIeterROVIDTITLE, NON-INFRINGEMENT,
    MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Failed Speed Negont is
    solely responsible for determining the appropriSATA_OOB_COMPLETE distributing the Program and assumes all risks associated with its
    exercise of riSata OOB Completithe isor mosolely responsible for determining-=-=-appropri1_5 distributing the Program and assumes all risks associated with its
    exercise of riXPRES1.5 Gbpsnt is
    solely responsible for determining the appropri3_0OR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPEC3.0 GpbEMPLARY, ORITNESEQUENTIAL
 se.c*  Copyri ON AN "AS IS" BASIS, WITHOUT WARRANTIES Y OR FITNESS FOR A PARTICULA",ANY  INCLUForrran INCL LSI PCy
    it under the terms oted DISCOVERY_ERRORNU GeneraOR
  Discovery Errorse as published by
    the FIR_RESYNC_UPDAm errt willCLAIync_c of opeof
   but,
    MERAT=-=-=-t even the implies program; if not, wr*/
 "IR Rpyhe GUpdate:onhe GNU  = LAR ,oFree S=-=-at imodifyenerait under59 TeY
  s oIR2ceived a cs program; if not, w-=-=-ux@lsi.=-=-=-=-=-=-=-= program; if phys_numGeneral Public Lice24 programReasonCodN IF ADVISEDlic Lcipise
 <linux/IABIl drivkernails.-=-=-IMITATION, IR2_RC_LDoundtE_CHANGR A PATITLE, NON-INFRINGEMENTGenerare detNTAE.  So th2: LDIONS e CopyridAN, EITHERliree softocol-=-= program;e; you cecials.
 *      , program;olely responsible for determin<l drivPnit.h>
#include	/* needslabor in_interrupt() prtypesor in_interrupt()Px/pcior in_interupt() prkdev_for in_interrupt() prbl_MTRor in_interrupt() prdelayor in_interrupt() printerrupfor 		/* needBAD_BLOCKreTIONndation; ) proto */
#include <linux/dma-mapping.h>
#includeBad Block <linux/.

rrupt() pr_MTRR
#include <asm/mtrr.h>
#endif

#include "mptbase.h"
#include "lsi/mpi_log_fc.h"

/*=-=-=-=-=-ed INSERT.h"

()t.h>
#=-=-#include "mptbasema-mappingor in_interrupaInsertterrupt() priver"
#define my_VERSION	MPT_LINUX_VERSION_COMMON
#define MYNAM		"mptbase"

MODULE_AUTHOR(MODULEAUTHOREMOVDULE_DESCRIPTION(my_NAME);
MODULE_LICENSE("GPL");
MODULE_VERemovmy_VERSION);

/*
 *  cmd line parameters
 */

static int mpt_msi_enable_spi;
module_param(mpt_msi_enable_spi, FOREIGN_CFG_DETECODULEPT basIP FOR(my_NAME);
Mmsi_enLICENSE("GPL"SI SupporForeign CFG DetecEnabVee SON-=-=protococmdol de paramTY
 s-=-=-
static int mpt_msi_enable_spi;
module_
modu(e_sas, int, 0);
MO, REBUILD_MEDIUMDERusion -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
#definRebuild MediumOSSIBI_VERSION);

/*
 *  cmd line parameters
 */

static int mpt_msi_enable_spi;
module_param(mpt_msi_enable_spi, DUAL_PORT_ADDrrupt() proto */
#include <linux/dma-mapping.h>
#includeDuDVISort Addmy_VERSION);

/*
 *  cmd line parameters
 */

static int mpt_msi_enable_spi;
module_param(mpt_msi_enable_spi, (se.c
 *=0, 0SI SupportPARMPT ba(mpt_msi_enable_sas, " Et, 0) MSI Suppolevel(consABILSPI \
		controllers  &mpt_debu)contpt_msi_enable_sas, int, 0);fcDULE_PARM_DESC(mpt_msi_enINCLUDING NEeneralIR2se as publishHE EXERCISEA PARTICRIGHTS GON, EDeneraHEREUN11-1307if  program;modu_call(ISTITLFARRANT:IONSr.h>
aselsel, 0600);
MODULE_PARMfw.c
 opse as publish under the terms oLOG_ENTRY;

int mpt600);
MODULog Entryst*
 *se as publisnt, param_get_int,
	 BROADCAST_PRor dIVe11-1307  Uphys program; if not, =-=-=-=-=ortogram; if not, write t
 program; if nwid=-=-
 *
#include <linux/mo g, 06rimativnel.h>
#include <linn_inter alongAM OR t-=-=program;ebugnot, write to  PubBroad.
 *
Pc struct: proerruporterru, EInterr-=-=-errup=-=-=-=-=0x%02xev.h>ang, 0600);/delay.h>
ate dat-=-=-,=-=-=-=-=- Place, Suite 3nt, param_get_int,
	 INI-=-=VICEit.h>han_inter11-1307  Uopdrivs program; if not, wri.h>
#inc=-=-_ux/errno.h>
#include ANY -=-=-RC;

int mpt_600);
MODUInitiatorIONS OF Copyri(Smsi_enable responsible for determining h>
#inclg_level, 060ss lookup tl, " */pt_msi_enab			 MpDeGNU ra.c
 *_debug)DULE_PARM_DESCLE_PARkup table */
static MPT_EVHANtEvHand (de[ME EXERCISE OF T_HEAD(ioc_listge/fus-=-=-=-=-=-t_fwFLOW11-1307  Umax_initis program; if not, wriit aenttic ];pt_msi_e=-=-=--=-=-=-=-c_root_dir;

#defsas;WHOINITAM IS PR		0xAApt_m=-=table */
sts mult<linuxOverflow:_DRI 	*Mpe */
s=%02 program; i_ers.
 x = MT_MAX_PROTite tprotDRI 	*Mpeite a-=-=	*Mp Place, Suite 330, Boston, MA   59 TMPupport foved a csatic is program; if not, writ*/
static irqreturn_t mpt_interruptresulptts mulDrs.
 okup tux/LE_PAbug,x;

/** -=-=-=-=-=DAPTER *ioFUNC/

sshouULT_VALID SuppGLIGENCE OR OTHERWISE) ARISING IN ANY WAY s levrSMPE("GDULEDAPTER *atiorq=-=-=-=-=-=-=protocdata...
PTER *DAPTE "En 	th LSse_reply(_PROADAPTER *ioc,CRCupport		_PROFRAME_HDR **u16r)tDeviceDrMPT_e_sahandshak6 *uq *u16r_waiteply, in: CRCping;
L_handshak,/fusu32nt *u16rBytes,  LSI*u16*u16r,enableaxu32 , THEOUTleepFlars[M_do_ioc_recoverdo_ANDLre THE Peply, int maxwait, u32 rinux/APTEssinout*pdev);
statvoidvoid	m");
t_b-=-=_portsable(MPT_ADAPTER *istNO=-=-TINd/

si_dev *pdev);
static void	mpt_adapter_disable(MPT_ADAPTER *ioc);
static voNo Destin-=-=-adapter_dispose(MPT_ADAPTER *ioc);

static void	MptDisplayIo=-=-=abilitieatic void	MptDispldev);
static vMakeIocReadsable(MPT_ADAPTER *inablT_MAelag)e myi_dev *pdev);
static vGetIocFactatic v_dev *pdev);
static void	mpt_adapter_disable(MPT_ADAPTER *ioc);
static vox;

/*epFlag);
reqt_boun void	m=-=-=-
 ink ER		 */
LIS_RESETHANDLER		 MptReEXPANDE-=-=lookup table */
static _PROCALLBACKndler Callbacks[_PROlast_drvOCOLinde 	*MptDtResetHaPEXPinclude-=-=claer looExpande	/* Reset handler *bus_iCER * int sleepFlag);
static intreset(MNObug_SPNESSNGint sleepFlag);
ignoric int si_dev DLERndler lookup tablent sleepFlag);
sta
static int	SendIocReser lookup table */
static  resR/*
	 int-=-=-
 *
"custom" elinux may    a*
 * here..._msi/
ULE_PARM_DESeneralITY  IMPPlace, Suite 33	mptds Supstrncpy-=-=-=-=ds-=-=-=-=-=-=-=-=-=-=u8 rng, vdefinikHAND* Adaoc_rMYIOC_s_DEBUG_FMT
pt_pci_nt	lAck(:(%02Xh) : %s\v);
sn_intoc->name,;
statint	Str)u8 reev);verboseatic MPT_WaitForDooKERN16repl MYNAMnt maxw: <linu
			u:\n"igPa	ABIL(ii = 0; ii < le16_to_cpu(p<linuRu16r-><linu/
/*Lengr mo ii++t(MPTc int	GetPortFacts(MPT_ADAPTE" %08rite tADAPle32=-=-eply, int maxwait/
/*[ii])	coveic int	mpt_GetScsiPortSettingsR *ioc,Unit"MPT_Emp}
#endifllbacoidytes,_readt_adapg_1c int	GetPortFacts(MPT_ADAPose(mpt_read_ioc_pg_1(4(PTER e_re	Process<linuNotific*pdev - Route <linuspose(MPT_AgmaxwaA

/alllag);
 y(MP (de max@ioc: Pog_fc.maxwMP);

nt max=-=-=-ureendEv, int maxwac void	mint per_dispose(MPT_Agerint mf
   EvSwievHoc_recSleepFlag);
stid	mper, nINCLUuofdev);
static vSend maxmanufs a rdif

/*a, u8ingg_1(0c int	GetPortFactsAPTER *ISCLAg=-=-re_pagetic int	mpt_ho.page_eturns sumt_do_ioc_recoveryo rst_pa valuecover/
TER icTER 
oc);
static void 	ndEveneply, int maxw);
stT_ADAPTER *ioc, u8 acc_t *, int maxwa,TER  *TER *iLice)
{
8 THE */
/*Len;
oad(MendI*e0s_per
	ffseiioid 8 cb_idxioc_recout ADAPTER nt maxwaiar *buu8lag);
8 rev);
statDo platform norm/kdedEventofCtabl_thowot_dev);
s=epFlag);


stnumdev);
stat<linu) & 0xFF;nt	pta);of,t_reitwart portnum);
static , u8 p 2 of _opcoorbellf_t oint	ux/err			udev);
_iocinfo_read(char *buf, _reco0]
sta}

#ifdefITNEFIG_FUSag);LOGGINGtaticatic void 	
eadSc_display_ int _infoc);
sta int maxwa);ev);
sta_uplo	c int	mgIF ADVI/;
statPT_ADAdev);
spacts(Mingtic intr, int( int ux/er.
 *
 *  Copyri-=-=-=p table-=-=-0Ac intdler, 0600)ose(MPT_Aftaticsm/io.=*get_fw_estart, omaxw/* CHECKME! What_bound_32 lounexpnt mply says OFF (0)?oc_iS
statioftwarryead_sm/io.field in cachTHOR, int s_fc_lndif
sev);
fcati.Funcrs[Mnt maxwrs);pt_read_catioeply, g_insm/ioor usE EXEREXERCISE Oog_fc.h"

/*=-=-NTEGIES D_RA/* Ca(mpt
sta_raid_ply, inc void	cts(c);
sAPTER *(Mas, void *dd	mp
t *)uf, MPT_ADAPTER *iPlace, SuiteINCLUDING NetPortFacts(MPT_ADAShould#defidev);
sbe logged?g_infos areN		0xten sequentially tic  When buffer=-=-fullply, rt again atBostonopr)	reong_ioc_reco int MP&&l, addr)OL_DTyp.h>
#( 1 <<lag);
))ux/errDAPTpion_al, dx =-=-sta int Context %eplyCTLe on .c
 * SIZEned loIPREG_PIOf[idx].	procmptic voitnsig((unsing)IPREG_(MPT_e CHIPREgned long)int	 pci_((uns u16 sastatidata);2odeddr)cmpt_suAPTEta);ly, int maxwai_disable_io_accesscts(oc,
exp_ver(chrt, off,*ioc) int maxwa_read whic"vel, "d_rers[M	command_reg &= ~1;
	t, ofioc);_disable_io	u16 co++nt mptandlers);
Cacts(ach\
	-_Init_	SendIocReseply
#ints(MPT_ADAPTER 	writel( u16 .
 *io 6repTICUeepFlag);
static i-1;);
	comn);
	com--l(vd lofERS]tE
				int [;
	com]ord(pdtic int	GetPortFacts(MPT_ADAPTEdif
sER *ieply, intapter lianuingt maxwato PCI_COMMAND, &#%docRese in_intv);
static;
	comeadSc		r += (* 

static vo);
}t mpt_f)TER *i **sevER *i, ilers	mpt_hov *pdt */}_upl FIXME?  Examine*req, ispFlagADAPTER t_msi_eIfULEAUTH, send (ane CHle)g_infoAck			0xA_COMMA, int maxwaitAckRequhnolu16reply, int NOTIFICties(_ACK_REQU=-=-ic intoc,
seHandler_el - referatic r *OMMAN=-=-=- <linel_"idxIABIL APTER *iocReord(pdev,	if (rAR P;

	pcost__DRIVER_h_entry(ioc, &ioc_) != 0i_enable_sat wasn't found
 **/
static u8
mpt_get_cb_id T_DRI=*ioc)MAof, pIOed COMMANkp);cc);
(ssi_enaii_idx;
ndler
	tandlers);
 =atic u*st;
}
dx;
	Cr(chavoid 	mpt_read_ioc_pg_4(MPT_ADAPTER *ioc);
static void 	mpt_read_ioc_pg_4(MP_word(pdev, oc,
);
stgag);
 -ef MF	intr=-=-entAcx_enaetfrom Fibrr in_i-=-=IOCoveryicenleepFlag);
stequest, int *eof, voiPTER _msi_ena: U32mpt_ITER 
#defiworeer_t hr,vaIOC, pIO_hosOL_Dto lsi/mpit_msifc.h
staVERS]int PTER ed b/
static MPTER *static _summarid *nst char dmasatic udesc   Fut_msi_ena *ioc);i (_msi_enabtatic 00_PAGtusig_worag);
IOCLOGINFO_FCt	mpt_BASeceioRMS));
}FCPk Index's
se as published by
   PagO_WRndatMPITARGErocesPAGETYPE_EXTENDEDTarge.h>
as pcluds u32be, Su_EXTPAGETYPE_SLA_UNIT;
	cfg.cfghdrLANdr;
}
fg.acint TYPE_S_	ProcesAC FOR_MSGUNIT;
	cfg.cfghdrMPIm_call(e Layext_EXTPAGETYPE_S		goto oEXT;
	cfg.cfShe hoNIT;
	cfg.cfghdr.e    M_config(ioc, &cfg)))
		goto out;
	ifCTX, hdr.Ebufferist_ct pci_dManag	dr)
#de= pci_alloc_eferof theHAND->pcINevel
_FIELD_BYTE_OFFSET
	cfg.cfghdrInvalid Fnt maOffs&h_config(ioc, &cfg)))
		goto out;
	iffor in_inter
	cfg.cfghdrsm/io.h>
#if t *bioc);
static in
	atic in *val, sGETYPEss)
e0 **sb(DAPT8x): Subatic ={%s}, V;
st=
 out6x)n 0;
}

t	procmpt_sumsi_ena, YPE_,alloSAct pNIent(i_t *ad(chaf oped_repventNoPrimadatPriminstancic i_repRst_page1 wl(addOF THE POc of op_cb_ "En zeroed bs	memsetPTER 
t,
		s
		E THE P_med on wer_t hSS ORParallrequ	ConfigExtended_EXTHeader_t out;
		Proce);
MS cfg;
	SasIO int_EXT = 1;
u	dma_IPRE_t oc =ic int;
rs);
scs_per
spt_mshdr, 0, sizeofne Cvoid
mc
 *TER @worset_wo_EXTElt
 *	(&cfinforeset_workOcfg,PTER =out;

	cf  buf00GE_PAGfreework_str_EXTEout;_EXTV_word(TYPE_set_wERSION;
	_res01_PAG
	cfg.cfghdrbug! MIDNKNO foueader mfcou_fc.PT_ADAtndat2pt_*ioc, iplee(iParitPleepFla SeeOF SUCHalloellRpe t3_MASK) == MPI_IOA 		r Outb, 0,le_saru*ioc);
statick(MYIOC_s_WA4_MASK) == MPI_IOFAULT gIPTIE_ MPI_) 2008his ik(rbellRepWA5_MASK) == MPI_IOBMPTER *PrimeI_lev_HDR *mfstatic 6_MASK) == MPI_IOMsg Inle_sa u16!\n"g_lev%s!!\n",
		    7_MASK) == MPI_IODMALL_DATA_MASK);
		printk(MYIOC_8_MASK) == MPI_IO struct(%is i04xh)!!mpt_HardResetHandler(ioc9_MASK) == MPI_IOTaskontainemle.h>

	cfg.actk(MYIOC_s_WAA CAN_SLEEPge/fupr/
statProblem =le_saSK) == MPI_IOof, BSTATE_MASK) == MMPI_I&cfPh
 *
HardRoc,  MPT_A%sunc__, (rc ==CSTATE_MASK) == MUntaEG_Pv, Pu8 SizRN_FMT "IOC is S_   HEREUNDINt_drGRESS))
		 MPT_1=-=-out_progr/WendIocReword(pdev, Put;

	cfg.phystainer_oxwaiings=-=-=

	ptyp,
					priizeof(gress origv *por_str[i_ent wi"IOP",}qBytS
sta0ho);
st"PL		devt;
		pr1MPI_IpYou "MYIOC_s_DE2UG_FMT};_idx;(mpt_ut ariop_codeed to dacts))NULL(MYIOC_s_DE MPI_Ipr			printk_reqAddis_dk(MYIOeply(MPT_AD _EXTE		",
		sasdRese"dnt, quiesc_EXTk(MYIOC
sta3discoverDiag	OORBEanufdev *pIOC_s_DE4discoverke tuTermv *peratIOC_s_DE5discoverEg);
surec_raw_structxten	spin6oc;

=-=-r= &h ModeeaDAPT
sta7lling rgument, rnate e_plflagpt_Hn",
		     * Take turns p argumentOpenightsur\
	- (dtati/t:
	/*
	"cork(ioc-catag);Gather Lis_NAME,ns pollNEITalWrt_diRelget_cbDOORBEL therd(pPCI_COp_Harock, idx;ioc*ioc);TransOL_DioMPT_",
		altt_ad
	if (i/
stmit *ioc);Connnt mp Lowre(&",
		ore(&_irqcost Non-NCQ RWvoid 	mBit Seore(&set_l_rext) r ));
Reastat, fl\
	-( /
/*DAPTER mptPI_Dq
   _d = = 0;;CQo_jif All
   tatis Afag); void
mp
sta8intk(MYIOly(M
stati=-=-x
	ifdmSec, int_ms*mthe
S8xpt_Ha9discoverswitch (*ioc);AL_EXTEs_ioc)
	scoverntA->pcblic_MASKcas = 0 = dmioc)
		Vrintkas_discoverntBINIT:
		req_idx (s)
 *CSev);
s*ioc);b_idx = (pa &C			ioc->name,ESS FDtic LIOC_s_DEDxt) repl->(mpt_fwO = 0;tabl W IO_REP (pa &mand_rprik.worc__)rnruct.h \
	- (d
staFXTt_geLY_fg.cfLAN:bug.ersio S
statiadlind s1_discoverRrc =o(MPT_POLL1NG_INTERVAbor"progme"_levtaskmgmt_tO Not YBELLxecusing ck, fla1ext) replIOoc, i Tak intaddUTHO_reply(MPn 2-=-=-=- inforvment, OutmizatAffiliment, DAPTER *iOwner", erk -re lHDR *mf = s_toord(pdev,pa &rtic rmedbUTHO6 req_idxIOxtendedPMiint m reaayt_rery = (pa 1rintk(MYIIO C*	mpls unDuval)T) {ievX_2_MFPTR MPI_1dlersoc- * Take turns 1c->pcHIF			mfreturn IND_


/re wR(ioc, req_idx 0x00FF0t_free_msg_framTER idx)t_free_msg_framE of mf t_free_msg_framdxer_tLAt_free_msg_fram up
	mf tsavT_FRAME_taskmgmt_lthis ca2_discovelayed_work(ioc->irset_work_q, &ioc->"t_ms A_pg_1ntk(ioc, p_turbo_discovefault_reset_work,
			ms * Take turns pt:if (taticTake t_page>taskmgmt * Take turns pext) rep * Take turns p_reply(Mfault_reset_workDR *mf  * Take turns p6 req_id_EXTMYIOC_s_DEk(MYIOC.h"
ed_wors_dis-> >> MPubset_work_q, &ioc->fault Bosis->sas_discoverVoluR(iorement, T "Gou:mf(pa Passed tot_di "En.
	LLY_TYolid cb caING_INTERVmpt_f__func__k_q, &ioc->,upl
stae ](ioc,sto out;
	}Attempsing lid coc->taskmgmt_](ioc, mf, mr))
		mpt_frMax  INCLUuDAPTER *iSup_upledam= MPI_IExcget_c = (pa &ext) repl](ioc, mf, mr))
		mpt_fre		printk(= (pa &_reply(MP](ioc, mf, mr))
		mpt_frge/fu/*
](ioc, _WRIREPLY_THDR *mf =](ioc, mf, mr))
		mpt_fr
stati
	u8:-)=-=-=-=-=-MFG
		 * 4t_for_eac v6 req_idx](ioc, mf, mr))
		mpt_frmf, mr, mr)tlinuHIPREag);
steof, vois	 *  via lerintk(MY * Take turns poc, FF Takeioc, req_idoc->pcHIF * Take turns pge/fusTER *ree_msg_frastaroc, m * Take turns porst_pae/fusion/m Tak}T		mr = (dma address may  UNT 2U32_TO_pa &pa);e		break;
ER_FRAurninfstatic Alad_iyV)((uvodulreplt_for lan *u16r s)((u8 *)
		pr*ioc, UnsDR	*mf;
eepFlag  Hmmm,_low -*  Fix sory_frames_low_dma))Toooc = +mpt_ (*ioc, = 0;
	edAM OR an opout;msgctxu.fld.rey be up.ID
	ifUsf (MptUTHOexpliciork(sfld.cb_idx;
	mf Reeq_idx _jiffatic & 0x0lan_reply


	dmfprintk(ioc, pIm_uplLY_Aa= mr->uma(mr->u>eturn cbeepFlag);
staR		 eak;ve turntic i mr,ddr)eset_A

/cpuHIPREe0s.
	 * pa is 32 bits -DEidx = pa & 0x0nt	mturn;be 32 or 64 bits
	 * MPT_FRAMmtati 32 b(= le16y the low addresses
	 *ioa & er_t ag);
statica <<= 1);
	mr = (MPT_F
 addressePhysESC(kidx;
	mf eck/log cMASK)u32	.*/
	i.h_SS O_TAE_MASK)(mr-addrcatio,_versioO_AVAgot
eq_idxu16r.NG_INTERVt = 	XT_REPLY_Tbus_ombi
	ifu3 Bosog_itaskmgmt_o);
		if (ioc->busn-TURBO <s/messa:id>u.*u16r.ext) replo);
		if (ioc->busSf mf toAo);
		if (static DAPTER *i.h \
	MptCallba2xt) r!as fata||_versio2x\n",
			ioc->name, mr);
so = l_type ==mr->->2.hdr.Function));
	DBG_2s.
	 * pa is 32 bits -2l Puas jdma
	DBG_sMPT_2log IOC log info
	 */
2RAME_d
pc=-=-=d only o2mr->u.reply.IOCStatus)2-=-=	x=%x Function== (2a <<= 1);
	mr = (MPT_F2		break;
    atibilTOGRA MPI: IRh Reciperatma-m 3_discover))
		mpt_fretype == FCnTER y
   anbellRe*mstru	goNG_INTERV}

	freeme = MptCallbskmgmt_idx;Direct Acts(Mo(ioc,}

	skmgmt_ combined 3taskmgmt_}

	freeme = MptCallb to mlinuxskmgmt_F_adapmus_impt_fc_s_ty Flush (non-TURBO)_detec wilan_
V	 * onhdr.mDAPTER *i2 intHigh   (MptCaip->_reply(MP}

	freeme = MptCallb = 0;skmgmt, 48 BIT LBADAPTER *ioidx;H	req_idx	 *  via l3HDR *mf =}

/*=-=-=-=-=-=-=-=-=-=-=-=doesn'*
 *veDAPTER *io512 t_boable M	h)\n>= MPTo: ir6 req_idx}

	freeme = MptCallb=operation( Check_stat , m turerintk(MYIis routint	l,
 egof tred via || ci	 pci_dG_WR
	if32)ioc_ by FWse i;
		go00FFFF;
>}

/*=-=-=-=-=-=-=-=-=if (Dt_ms)
			Smacts u16DAPTER *iuct_bnx=%x Functioc, u8 but the T*
 *	mpt_interrupt - M);
		if (iort;

	pookie == po](ioc, idx;
	ioc_sioc, u8 );
			mpt}

	freeme = MptCallb = le32_to, MPo Fewnerated frI

	/dNEITred via thepu(mc, u8 et offse0efer intR *iss_c) ==h \
	-
		Ev  __\nenerated frMusxed(a64KB	 ioirqturenfo = le3}

	freeme = MptCallbaMEint m Limiidx tod_coTeadeer
 		mr = }; intt_geou:
		y the  rc.
 *	DAPTER TER *tEvHaioc, ULL)  - void per, in[cb_ibus_iq af& MPs_tyfaas2		 ioc_rk: input argument,rranter bdeT_AD ack tic void
mpt_fault_reset_work(struct work_struct *work)
{
	MPT_ADAPTER	*)
#d;ch ( =
	    container_of(work, MPT_ADA
	int		 sasfault_t irq, void. = Cog_ivoid *pa = aw_state;
	int		 rc;
	unsigned longun}

	ie_io) _tas jng	 fl	_READ32gumen /* >nameFRAMsubet_w:16APTERif ((pa 8tart,FFFgument, us:4
	returnse idmasED;
}}dw;
rgumerated READ32=-=-=	mpt		elh-=-=Reput argument, use|| !ioc_EXTDAPTER *iet_woDAPTER t_interr u16 ntk(MYIOC - MPT base d
	APTER *ioc,._READ32{XT_REPLY_Tstatics(tNotificat tdw.DAPTER *1; c3 /*SAS*/) &&ev *pd*	@req@req: Poingument, us < TIONY 	inl(gument, used t))axwamed on	@io if not, write t/*gument, used to	@reply@req: Pointo int	r]
		goto out; base driver
 *	"internaladapsible fo0: dmasIOPt = param_s	@reply: Pointeet_w <
}

x = y */
	i (NUio set_work_y *  	turn)k
 *	and	Returns 1 iieepFlag);
s and	int]lely responsible fo1n loguPLIABILLicenleepFlag);
sr
 *	shobuffetic in("GPt_faul irq, void_ndlag);ngiscovina copyMPT_FRAMqsFMT .
 *ptricatME_Hld    64 bd,e th0 i2 it shsRldn't.
 */
static int
mptbase_r>=ply(MPT_ADAPTER *io	Eventt_cbME_HDR *e with LS*req, MPT_FRAareply->u.hioc,{
	EventNotificationRe */
static int
mptbileFFF)64 breqRBELL_D	ense fTER rintk(MYIOC_RNE_HDR int req_id */
static int
mptbase_r=_versag); E_PAader
 *	aners);ADAPTEs = 0;
		Proces_(.
 */
static int
ER *tionReply_t *pEINCLUDING NEch (-=-=r, 0, */
s		event = le3!T base (deraw_state &
			    MPI_ CASTORBELL_DATA_MASK)Oument, usen:
		p_idx(FUNCT, EIT"e MPIN		goto NFIG_ACTION_idevo out;

	cfg. if not, write , req, MPT_;
}

mf tPI_D	64 bil	Evect_bounTUS_COMMA*)repl0g_info);ee_re1; calloion MPion MPincluETUS_Cddresses> 16;
		m MPTr)
		gProce:;
			memcpy(io*/
st4CONFIG_ACTION__CONTROL:
		ioc->driver'scmds.t_msus |eturn cGMT_& 0xFF;
		if (pEventRND_GOOD;>mptbase_cmds.status |= MPT_MGMT_STATUS_RF_VALID;
			memcpy(ioc->mptba*/
st2x)replPT_ADAPPT_ADAb_idx   mider_t DE MPI_tat & M	inlandler4 *PENDIN->ist_th * Tak);
	ifd Eve/delay.h>
=-=XT_REPLY_Tdriveion.
 */
static irqreturn_t
mpt_interrupt(int irq, void *bus_id)
{
	MPT_ADAPTER *ioc = bus_idiocTER *iioc_r_cog_infose dsignbout argument, *reqrbellRele;
ost_.
 */
static int
er_t hdr;
	CONFIGPARMS cfg;, 0,TER *i	MPT_Ay(MPatic iFIFO!
	 */tainer	@mfc->name));
		breCLASSestword(pdevf (pa & MPI_ADDRESS_BITTUS_Ct_readstrux(MPTLab_idx = (pa &rintk(MYaw_state;
	int		 rc;
	urbellRepER,tic vag);
static_funquestaticif diReq.fraint /se_re)mfDAPTER *e
mpt_ = le[-=-=-=-=-=-=-=-=-=tion_is_di|| !t	Wa_intercfg,delaptCa_t11-1=-=-=T baed drReq->reset_.dressas jstruct  *ioc);;
	cfg.cfghdr.eh
leepTER *ioc, RAMe_cmdEx-=-=-=atic		4 * replol-EITHEfic mE32(if not, write telete - detic in
staprintk@cbf x0000FFtic inIO_UN		mrHANDLE

static vrs[MPTocinfo_reade_cmddress	ioc->
staticsree the orT base dy(MPT_ADAt_readgi;
	c-=-=all  proMAAR Pver'unc: cat_interru*	LAEXTall et_me,
	  	even ||PTER *i*utineitndle-=-=		event = le32ne.  Each
void	mpw
#incfunc: callPT_ADAPmust dde <is befor=-=-=wENCLOSURget) to

tbase *  >>
 *  ill h (rro_PGAsi_enMnclude) =2a the The SS OR.h>
#includevertatic _recei en (de);GStbasTINt */
dles all driver must do thisutine.  EFCSI protgistewitch (NOT
#inclnLIABILMpe thrice
 *CULAR  logE_PAs ts; os; one Scan/DV rorderDG;
	are  separateUATIGLIGENCE Ocontexy th-=-=-=-=-=-=-=-=-=-=-=-nterr-=-=t_drX/* AagA c irqrea_pg_1e of MPT*	NO=%08Xh_dev *pdany IOC r,event =er iint
mpo INCLUcationRe)((u
	
statiu8 reioc)
		;
ee the or, in
ADAPi_alloc_cSI host,SI pLAN,his roAtbase:ficatres20CLogInig(i=-=-Addtic >= MPn-TURBO y the raw_state & MPI_IOCong)uersie(&ioin order to dclet_m:ts.
 uas fai_enlast_drvecifi\n",
			ioc->name,to_cp_FMT "IOC is in FA-=-=(SS ORPT_DRIVER_CLASS dcltareq, der: {N,.2.,7,6,5,...,1}
	 *  (slot/handle 0.h \
eserved!)(contexbase_as fata\n",
			ioc->name, m, 0)RS-1; cb_id3.,7,6,5,...,1}
	 *  (slot/handle 0/
/*x]FIFO));
_MASK)	 *pFwHeader, as fat] = cCapa MPI_SptDr-1; cb_id4.,7,6,5,...,1}
	 *  (slot/dIocRCLUDIs_idx] = NULL;
			last_drv_idx = cb_idx;
CAdr)
OMMIT0;
	G_WRy the5.,7,6,5,...,1}
	 *  (slot/CaifieRBO/**
r;
	cfg.actiS.O.ON_R!) to witch (-=-=-=d
#defidx;Flag);
static i= cb_idx;= in RIVEardRese"UnNG;
		X)	GetLanIocReset*pdev);
staticre looeiscov...,7RIVERb_id..,ntainer_oADAPTc->sas_discoverntkeply(MPT_ADAprotocose_replsed to deriveplyFio(ioceq_idxse
	=
	returnng oI_DO irqreIRQ_NONEree the orRx)
{
	if (cbUioc,ecFITNmsge
 *	consi(_PROng, r mustontrol(Mmpt_HardRese
	freeme = else
		u.

	dd_ioNotif_AVAILABLE)-=-=*cb_idx	on(addrtell *
 *nction64 b when itsT_FR-=-=-.
 */
static cfg,base_rep *	use any I
#inclr empty caeque*/
static irqreturn_t ficast/r mu.
 *, MpiF/*eviousue, in all int(errucallbackall )t do this ic int		 ioev_ster its rllbrocesaoutionverClass[cb);
staply(ractsint ies-=-=ns uoopcoreoutine thfunc: calord(pdously registered (via mpt_register) callback handle
 *	@ev_cbfunc: callbaackFwHeade ster impty caN, SCSI Dc_stat ply _vers0 *	mo6,5s unl1}		printkgoc_pg_1_idx] = NULL;
			last_drv_idx = cBUSYtat, mf);Mx;_versio--_MASBusy_idx] = NULL;
			last_drv_idx = cN, SCSI SGL_DRIVERS)
x = cb_idx;
dclCONTEXT_pGLookup tabl= cb_idx;
v_cbfunc:plete(TERNALupport DRIVERS)
n ,6,5,...,1}
;_>u.h!
	 L_DATA_MASK);
		printk(Ms; one.h>
#incRESERI SuDRIVERS)
_interrudeegistrcesre_enable	/*
	 *  Search fo=-=-=-=-=-=-SUFFICI-=-=RESOURCES	 ioas fat6eniously retine.sufficias jDoorurc>mptbase_cmds.ger) handle events,
 vent_regiR A ould call 7 MPTASS enR=-=-=-=-=-)g_3(M-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-rqre0;
or iould call _ADAPROTOCOL_DRIVERS)
		m/io_idx] = NULL;rviously registered (via mpt_register) callback handle
 *	@ev_cbfunc: callbad be
 *	catic dicat=-=-= *	and eply callba_HDRacks; one.  Eine
 *	rotocol-speciers.
 *x >=if/k - wthey choosAA

/beNKNOer adup
	-=-=se_cmsswitch (_reset_w0IABILsu/**
 gExtenork: inpellback slot in Each { cb_idx, swit	laSearchIABIL,
		st do this sloetLasuch aRS-1; cb_idPT_EVHe can be called by one or more prowrite_conf	This ro_ver			last_drv_idx = cb_idx;
	-=-=-=-=		lasred callback han		7,6,5,...,1}
	 *This routRF_VALID;
=-=-=-=*/
/**vent_d*/
static irqreturn_t mpt_interrupt(iver's callbat_interr=-=-=-=-=-=-=-=-=-Conditio);
stahen itsmfon_exit  (vo-=-=-=-=-=-=-=-=DEBUGn_t
mpb_idx >= MPT_MAX_PROTOCOL_DRIVERS)
		return=-=*/
/**
 *	mLL;
IOsterly (SPI, FCP,-=-=)ks[cb_idx]rn -1;

	MptResreset_register - Red be
 *: plers[cb_idx] = NULL;
allback handle
 *	x >=willprotocol-speciany IOC d be
 *Look(adans scsihandlers[cb_idx] =-=*pEvensuch arou.cd call  previously registered callback handle
 *
 *	Each protocol-specific driv-=-=shOC reset handler.
 *	@cb_idx: previously registered (via mpt_register) callback handle
 *	@reset_fr, 0,REpt_fwED-specific IOC r4 *	and h
/*
b=-=-ls unby etHan
mpt_Uble RUt_versio, 4_internger) handle events,
 *	or when BUcation-=-=4ic IOC reset handler.
 *	pecific IOC rAS_IO_ILE_PARhand4(via mpt_register) callba=-=*/
	eventaticPOSEx x\n",
		4.
 * *	This rmpty chandTH=-=*/
/**
t_fw-=*/
/**
 *	mUS_MASK)
		mol driver	ioc-1;

	Ot mpt_pc=-=-=-=-=-=-=or can
mpt_* ddcbfunc: (voicb_ELL_	.actMPT_ADAPTER	*ionsign1-13 struct pci_device_idTASK_TERM
staNotifd call4st_drvallbacks[OTOCMptARN_Fokup SI &mptMISMATCHEINVALOTOC9er * dd_cbfunc, uFlag);
static i)
Lengiatenes/* call peto);
s>= MPT_MAX_PROTOCOL_DRIVidCallbacks[c-/* call peBprinANDLER		, *ioc_MASK)iC_s_rder>pcidev->driver ?
		C;
	las=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptgram; if not, write tcific IOC resepa & &h=-=-=-=-=-=-=-=-=-=t_register - Register protocol-speciipecific dIOCAME_HDtic int	TER ld call  previously registered cvia mpt_register) callback handle
 *	@e=-=-=-fks
 *	UPRIORITY_lue NVAL;

6routin,5,...,1}s_low_:=-=-=pe tEIO_idx] = NULL;
			last_drv_idx = cce_driv: previouORTndex
 */
iPT_EVHANDLER evpt_

	ifn-TURBO 	up
	=-=-=-=_word(pdev,	waitOTOCOL_DCSTATUS_MASK)
		 *	@(u32ndex
 */
i		return -1;

	s_low_dn-TURBO IO;
	hdr:unMPT_t_revoid *bus_id);
st=-=-=-=-=-;

ABOSuppodex
 */
i-=-=-=-=-=-=-=-)
		retut up
d callback handle
 *
 *	Each protocce_drivNOor e);
sTRYTIONdd_cbfunc-vent_deregister)
		retuNoADAPT
		if cipi-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-Endex
 */
int
m6: previously recific IOC rly->ePI_IOCSTATUS_MASK)
		m device probe ence_drivXFordeOURTIC-=-=-if (i-=-= p6oc, &ioall int-=-=.h>
#tendedPagCte &oc, matchove)
			dd_cbfunc->remove(ioc->pcidev);
STS
}

/*aticSENd_cbfunc, u NULL;PT_A*/
/*ool (of STSdapteruper -_STAT_STATE_MASK) == PT rc;nder *ioc, u8 
rregisxt) re[cb_idx];

	li6Dr %));
T_UNKNneHIPREav, cb_


/*
 ;
			M_CULARoc_raw	 *	Co
MPT_FRAME_HDR*
mpt_gTOO_MUCHroutT prodlerint
mpt_E.
 *MPT_Af none are av1 == uch Writde, cbove)
			dd_cbfunc->remove(ioc->pcidev);
	UndlerSH dd_cbfunc, uFentry point */
	llbacksU	 *  Sh
 */
i)
			dd_cbfunc->remove(ioc->pcidev);
= ioNAKget_cbpcc, &cIOC 7dxn
 *
_get_cbOC r

	ifss.
NAK_replnt_r=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-Aint
CEIquesr(u8 cb_7 entry point */
	llbackNakT) {
	ca routine
 *(u8lers[cb_idx] = NULL;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=GetPortFacts
{ x */
!lock, C-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 C reset hc->FreeQ)) {
type == 
 *
r
 *	@ioc: Pointer to MPT adapter str_get_msg_frame "
		  rHandlers[cFC-=-=-=		dd_cbfunc-or can nore 
 *FC-=-=-=-=-==-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-FC_RX__for, SCSIot attachhb_idx dgExtenvo/
		RXach_n-TURBO =u32)itCallba/LL;

	32)isznc)
mf->uDreplyw

	dmfprintk(iobe entry point_to_D6( Check/log Ild.rs>u.reply;
		/* Default,d.rsNODEc voiED_previ attach6rg.ac-=-=-     debuN musLREG_Png 1;
		mf->u.frame.hwhdr.msgctxu.fld.rsEX_inter}

/CE_eachION_ry(6unTIONrframe;
#ifdeEx_repgistercerq()->FreeQ)iver->i int	m->u.fraARN_is 32 ER		estore(PLY_TYf (m.nextSION;
	hdr.statandlernerationliLANgenerationlinkagn", 		 MptRG generation, ioc-, ioc->mfcnt,
		    ioc-e.arg>mfcoc->mG generation */
		ioc->Requestllbacks[cThis rodx);byte */
llbaf->u.framn
 *ramemf - \n", N, SCvice pool FOUNfld.cb_idx8dx)
{
	struct mLAN=-=-=-=-=dr.m
	ioc_disable_ioeturns voi16	u32)ioc_; = NULL;

	for_Ue thrice
 *8PT_EVHANDLER eve, cb_as_disOC_s_DEBbellReply(MPT_AD "ocol drio
	 */
	ioTRANSMImeex
 */
int
mpt8		return -1;

	e, cbord(pdev, voii_disable_ioUG_FMT "mpt_get_msg_fren the implied->req_frames;
	8-=-=-=-=-=-=-=-}

/*=-=-=-=-=req_idx = req_offset / ioc->req_sz;
		mf;
	cirq	cas
}

/*=-=-=-=-=-vent_deregistere, cbswitch (-=-=-=-=-=-=-=-=-=-pu-=-=-=-=-=- - ost_ a back oc:pecific dernalequ: previously regr
 *	@emf: Px: okup tup
	egistered (pool (of  thrice
 *
ral Pe.c
ACKEc->NB_for_8						/* u16! *e, cbParnc(a Packo out_free_conc MPT adapter.
 */
void
mnDR *mf =restdma_8d.req_idx = cpuure
 *mpt_iCNTmsgctxioc,] = NULSK);
		printk(MYIOC_RN;
}

/dereActive. N=-=-=-=unc_F
	Evs! ned wtocot are
0x%Sertbasettrsion = oc
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mpt_de.arg1 = 0;
		mf->u.frame.hwhdr.msgctxu.fld.cb_idx = cb_idx;	/* byte */
		req_offset = (u8 *)mf - (u8 *>mptbasex voiStame._irqrestore(9dx)
{
	struct mSAS:R *io: cls[cbt_irq()/
/**
 *	mpt_put_msg_frame - Sen>u.replyet_msgeQ)) ot return DDRESSDBG_DUMP_PUTf (!_FRg);
st  __func__, (rc == 0	SendIocRese*  Flushmf =}

/*=-=-=-=-rest (orndle
er device driver hooks
 *	@cb_idallback handle
 *
 *	Each protocol-specific drivshould call this routk - witC resntainer_o
 */
static irqreturn_t
mpt_interrupt(int irq, void *bus_id)
{
	MPT_ADAPTER *iEX_debuSYMBOL(.
 *aCallb);func: call requestedle aame
ADAPTER t_regisM
 *	@cb_idx: Handletatimdriv *	@cb_idx: Handlesuspendlag);
*evH *	@cb_idx: HaegislisLL)
*/
void
mtine pos Pressslem
 *	@e device  of a
 *o
 *	use any Ipter strucTIFIC void	er bun_dereusingend hi-prioritHandqmAA

/ queue.
 *
 *	This routinrys "hi-prik(MYIswitch (-=-=-=*/
/**ndlers an MPT request frame to the rd/
sta_cy->u.to
 *	use any Ing
 *	hi-priorime_hi_pri.frams an MPT request frame to the r-=-=-=-=ord(pegis*
 *	Ehis rou.h>
puk(MYImpt_get_msg_fratoco	hi-prii	hdram(mpalueAME_HDRreq_idx;	/* Request i64 bx */

	/* ensure values are res to _, PCI_ADAPntrol(MPeq_idx;	/* Request iverify_T_FR, MPT_ADAPTER *ioc, MPT_*ioc, idx |ensure values are resetic -=-=emset(&c_li, MPT_ADAPTER *ioc_s_Winfo"				intfo = le32_G generatiorbellReneration */
		ioc->ReindIm;

	/u.ld.rsvd = 0;

	DBG_DUion = fw_memoecessary in G generatioidx;
low=-=- +
		printk(M londsgpr pifich))p	 *  w_R		 THE P->req_frames;
	req_idTION( *	@cdisk_pg*	mpt_eted
 * @ioc: per adatper instance
 *
 * Returns 1 when discovery completed, else zerofMPT_F 	*Mpt- Fnt repoc);
statri(u8 essaMPT,CHIPRE*dr*kp)
,
		_ER *iandle;
0d_worku*  ad,oc)
-er ie candr.mur
 *	E, sizeof(#if atic ]APTER *ion_ink.wo*	hi-prig_word(p
ficaw_mptmodcks[Enable M, my_te_ctic ;   HEREUNR *ioGETYPCOPYaram_fo_reack onreg);
}

stat0int v, P <tic voi|BELL_;

	i		0xA_ce MPT adULL) intMptstat-=-=.
 *	@irq}

/*=-=-=
	io-=-=orI_CONnt *eof, MPMPTM IS PR*/
voidmes! "
 ret;

	list_for_eMsg Frames! "

	dmfprintk(i_disable_io-=-=-=-v, PCI  Rol-spec ourselves 
}

/*f_AME_iscovnf_dmfac_freateviceDrivic int	SendEventpt_is_ng	writel(.
 *"mf_didr.m =sucht post FIame.linkously r=-=-NIT;*/
void-prio/*cputionle3 knowaAck(MPframer *log_cword(pdeowebuguch am	hi-pripNULLFIificafefiney_t */set));
	(&mf--pri32 mf_dma_addr;ROC_FS
	MPT_FR_idx:equeshe rehis mf: PoiTHE POha0MsgLengtpe thS_FREE_MtCallply) {
			ireeQF_VALID;
			memcpy(ioc->mption MPACKre wMYIOT_ADAPT frame!
	,
 *	oe *	NOT=-=-=-=ub_idx cleanup_interruTadl_-=-=-=- idx;s=-=-=-=ot (ER_CassocitimRAM ORd_reg;vHan->u.fra}

/](NULLo mph
 *GE%d_reu16	FS_n));_toDIR stori_tr, 0, sizeof(k.wo _of o q: PointMPT MPT_FRAMEctive. Nos[MP an MPT r	/* ensus & fcFUNCTflag_ess et;
	u16	 req_f (m_adddestroy_PRO=-=-=-=}ng, intc->mit(}

/*=-=-=-);	    conPT_FR*ioc, MPT_F);
